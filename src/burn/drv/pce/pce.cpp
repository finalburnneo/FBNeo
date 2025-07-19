// FB Neo PC-Engine / TurboGrafx 16 / SuperGrafx driver module
// Based on MESS driver by Charles MacDonald

#include "tiles_generic.h"
#include "pce.h"
#include "h6280_intf.h"
#include "vdc.h"
#include "c6280.h"
#include "bitswap.h"

/*
Notes:

	There is no CD emulation at all.
*/

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *PCECartROM;
static UINT8 *PCECartRAM;
static UINT8 *PCEUserRAM;
static UINT8 *PCECDBRAM;

static UINT32 *DrvPalette;
UINT8 PCEPaletteRecalc;

static UINT8 joystick_port_select;
static UINT8 joystick_data_select;
static UINT8 joystick_6b_select[5];

static void (*interrupt)();
static void (*hblank)();

static UINT16 PCEInputs[5];
static ClearOpposite<5, UINT16> clear_opposite;
UINT8 PCEReset;
UINT8 PCEJoy1[12];
UINT8 PCEJoy2[12];
UINT8 PCEJoy3[12];
UINT8 PCEJoy4[12];
UINT8 PCEJoy5[12];
UINT8 PCEDips[3];

static UINT8 last_dip;

static INT32 nExtraCycles;

static UINT8 system_identify;
static INT32 pce_sf2 = 0;
static INT32 pce_sf2_bank;
static UINT8 bram_locked = 1;
static INT32 wondermomohack = 0;

INT32 PceGetZipName(char** pszName, UINT32 i)
{
	static char szFilename[MAX_PATH];
	char* pszGameName = NULL;

	if (pszName == NULL) {
		return 1;
	}

	if (i == 0) {
		pszGameName = BurnDrvGetTextA(DRV_NAME);
	} else {
		pszGameName = BurnDrvGetTextA(DRV_PARENT);
	}

	if (pszGameName == NULL || i > 1) {
		*pszName = NULL;
		return 1;
	}

	// remove the "pce_"
	memset(szFilename, 0, MAX_PATH);
	for (UINT32 j = 0; j < (strlen(pszGameName) - 4); j++) {
		szFilename[j] = pszGameName[j + 4];
	}

	*pszName = szFilename;

	return 0;
}

INT32 TgGetZipName(char** pszName, UINT32 i)
{
	static char szFilename[MAX_PATH];
	char* pszGameName = NULL;

	if (pszName == NULL) {
		return 1;
	}

	if (i == 0) {
		pszGameName = BurnDrvGetTextA(DRV_NAME);
	} else {
		pszGameName = BurnDrvGetTextA(DRV_PARENT);
	}

	if (pszGameName == NULL || i > 1) {
		*pszName = NULL;
		return 1;
	}

	// remove the "tg_"
	memset(szFilename, 0, MAX_PATH);
	for (UINT32 j = 0; j < (strlen(pszGameName) - 3); j++) {
		szFilename[j] = pszGameName[j + 3];
	}

	*pszName = szFilename;

	return 0;
}

INT32 SgxGetZipName(char** pszName, UINT32 i)
{
	static char szFilename[MAX_PATH];
	char* pszGameName = NULL;

	if (pszName == NULL) {
		return 1;
	}

	if (i == 0) {
		pszGameName = BurnDrvGetTextA(DRV_NAME);
	} else {
		pszGameName = BurnDrvGetTextA(DRV_PARENT);
	}

	if (pszGameName == NULL || i > 1) {
		*pszName = NULL;
		return 1;
	}

	// remove the "sgx_"
	memset(szFilename, 0, MAX_PATH);
	for (UINT32 j = 0; j < (strlen(pszGameName) - 4); j++) {
		szFilename[j] = pszGameName[j + 4];
	}

	*pszName = szFilename;

	return 0;
}

static void sf2_bankswitch(UINT8 offset)
{
	pce_sf2_bank = offset;

	h6280MapMemory(PCECartROM + (offset * 0x80000) + 0x080000, 0x080000, 0x0fffff, MAP_ROM);
}

static void pce_write(UINT32 address, UINT8 data)
{
	address &= 0x1fffff;

	if ((address & 0x1ffff0) == 0x001ff0) {
		if (pce_sf2) sf2_bankswitch(address & 3);
		return;
	}
	
	switch (address & ~0x3ff)
	{
		case 0x1fe000:
			vdc_write(0, address, data);
		return;

		case 0x1fe400:
			vce_write(address, data);
		return;

		case 0x1fe800:
			c6280_write(address & 0xf, data);
		return;

		case 0x1fec00:
			h6280_timer_w(address & 0x3ff, data);
		return;

		case 0x1ff000:
		{
			h6280io_set_buffer(data);

			INT32 type = (PCEDips[1] << 8) | (PCEDips[0] << 0);

			type = type;

			if (joystick_data_select == 0 && (data & 0x01)) {
				joystick_port_select = (joystick_port_select + 1) & 0x07;
			}

			joystick_data_select = data & 0x01;

			if (data & 0x02) {
				joystick_port_select = 0;

				for (int i = 0; i < 5; i++) {
					if (((type >> (i*2)) & 3) == 2) {
						joystick_6b_select[i] ^= 1;
					}
				}
			}
		}
		return;

		case 0x1ff400:
			h6280_irq_status_w(address & 0x3ff, data);
		return;

		case 0x1ff800:	// cd system
		{
			switch( address & 0xf )
			{
				case 0x07:	/* BRAM unlock / CD status */
				{
					if (data & 0x80)
					{
						bram_locked = 0;
					}
				}
				break;
			}

			bprintf(0,_T("CD write %x:%x\n"), address, data );
		}
		return;
	}
	
	if ((address >= 0x1ee000) && (address <= 0x1ee7ff)) {
//		bprintf(0,_T("bram write %x:%x\n"), address & 0x7ff, data );
		if (!bram_locked)
		{
			PCECDBRAM[address & 0x7FF] = data;
		}
		return;
	}	
	
	
	bprintf(0,_T("unknown write %x:%x\n"), address, data );
}

static UINT8 pce_read(UINT32 address)
{
	address &= 0x1fffff;
	
	switch (address & ~0x3ff)
	{
		case 0x1fe000:
			return vdc_read(0, address);

		case 0x1fe400:
			return vce_read(address);

		case 0x1fe800:
			return c6280_read();

		case 0x1fec00:
			return h6280_timer_r(address & 0x3ff);

		case 0x1ff000:
		{
			INT32 type = (PCEDips[1] << 8) | (PCEDips[0] << 0);
			UINT16 ret = 0;

			type = (type >> (joystick_port_select << 1)) & 0x03;

			if (joystick_port_select <= 4) {
				if (type == 0) {
					ret = PCEInputs[joystick_port_select] & 0x0ff;
				} else {
					ret = PCEInputs[joystick_port_select] & 0xfff;
					ret >>= joystick_6b_select[joystick_port_select] * 8;
				}

				if (joystick_data_select) ret >>= 4;
			} else {
				ret = 0xff;
			}

			ret &= 0x0f;
			ret |= 0x30; // ?
			ret |= 0x80; // no cd!
			ret |= system_identify; // 0x40 pce, sgx, 0x00 tg16

			return ret;
		}

		case 0x1ff400:
			return h6280_irq_status_r(address & 0x3ff);

		case 0x1ff800:
			switch( address & 0xf )
			{
				case 0x03:	/* BRAM lock / CD status */
					bram_locked = 1;
					break;
			}
			bprintf(0,_T("CD read %x\n"), address );
			return 0; // cd system
	}
	
	if ((address >= 0x1ee000) && (address <= 0x1ee7ff)) {
	//	bprintf(0,_T("bram read %x:%x\n"), address,address & 0x7ff );
		return PCECDBRAM[address & 0x7ff];
	}	
		
	bprintf(0,_T("Unknown read %x\n"), address );

	return 0;
}

static UINT8 sgx_read(UINT32 address)
{
	address &= 0x1fffff;

	switch (address & ~0x3e7)
	{
		case 0x1fe000:
			return vdc_read(0, address & 0x07);

		case 0x1fe008:
			return vpc_read(address & 0x07);

		case 0x1fe010:
			return vdc_read(1, address & 0x07);
	}

	return pce_read(address);
}

static void sgx_write(UINT32 address, UINT8 data)
{
	address &= 0x1fffff;

	switch (address & ~0x3e7)
	{
		case 0x1fe000:
			vdc_write(0, address & 0x07, data);
		return;

		case 0x1fe008:
			vpc_write(address & 0x07, data);
		return;

		case 0x1fe010:
			vdc_write(1, address & 0x07, data);
		return;
	}

	pce_write(address, data);
}

static void pce_write_port(UINT8 port, UINT8 data)
{
	if (port < 4) {
		vdc_write(0, port, data);
	}
}

static void sgx_write_port(UINT8 port, UINT8 data)
{
	if (port < 4) {
		sgx_vdc_write(port, data);
	}
}

static INT32 MemIndex(UINT32 cart_size, INT32 type)
{
	UINT8 *Next; Next = AllMem;

	PCECartROM	= Next; Next += (cart_size <= 0x100000) ? 0x100000 : cart_size;

	DrvPalette	= (UINT32*)Next; Next += 0x0401 * sizeof(UINT32);

	AllRam		= Next;

	PCEUserRAM	= Next; Next += (type == 2) ? 0x008000 : 0x002000; // pce/tg16 0x2000, sgx 0x8000

	PCECartRAM	= Next; Next += 0x008000; // populous
	PCECDBRAM	= Next; Next += 0x00800; // Bram thingy
	vce_data	= (UINT16*)Next; Next += 0x200 * sizeof(UINT16);

	vdc_vidram[0]	= Next; Next += 0x010000;
	vdc_vidram[1]	= Next; Next += 0x010000; // sgx

	RamEnd		= Next;

	vdc_tmp_draw	= (UINT16*)Next; Next += PCE_LINE * 263 * sizeof(UINT16);

	MemEnd		= Next;

	return 0;
}

static INT32 PCEDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	h6280Open(0);
	h6280Reset();
	h6280Close();

	vdc_reset();
	vce_reset();
	vpc_reset();

	c6280_reset();

	memset (joystick_6b_select, 0, 5);
	joystick_port_select = 0;
	joystick_data_select = 0;

	pce_sf2_bank = 0;

	last_dip = PCEDips[2];

	nExtraCycles = 0;

	clear_opposite.reset();

	return 0;
}

static INT32 CommonInit(int type)
{
	struct BurnRomInfo ri;
	BurnDrvGetRomInfo(&ri, 0);
	UINT32 length = ri.nLen;

	AllMem = NULL;
	MemIndex(length, type);
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex(length, type);

	{
		memset (PCECartROM, 0xff, length);

		if (BurnLoadRom(PCECartROM, 0, 1)) return 1;

		if (ri.nLen & 0x200) { // remove header
			memcpy (PCECartROM, PCECartROM + 0x200, ri.nLen - 0x200);
			length -= 0x200;
		}

		if (PCECartROM[0x1fff] < 0xe0) { // decrypt japanese card
			for (UINT32 i = 0; i < length; i++) {
				PCECartROM[i] = BITSWAP08(PCECartROM[i], 0,1,2,3,4,5,6,7);
			}
		}

		if (length == 0x280000) pce_sf2 = 1;

		if (length == 0x60000)
		{
			memcpy (PCECartROM + 0x60000, PCECartROM + 0x40000, 0x20000);
			memcpy (PCECartROM + 0x80000, PCECartROM + 0x40000, 0x40000);
			memcpy (PCECartROM + 0xc0000, PCECartROM + 0x40000, 0x40000);
			memcpy (PCECartROM + 0x40000, PCECartROM + 0x00000, 0x40000);
		}
		else
		{
			if (length <= 0x40000)
			{
				memcpy (PCECartROM + 0x40000, PCECartROM + 0x00000, 0x40000);
			}
	
			if (length <= 0x80000)
			{
				memcpy (PCECartROM + 0x80000, PCECartROM + 0x00000, 0x80000);
			}
		}
	}

	if (type == 0 || type == 1) // pce / tg-16
	{
		h6280Init(0);
		h6280Open(0);
		h6280MapMemory(PCECartROM + 0x000000, 0x000000, 0x0fffff, MAP_ROM);
		h6280MapMemory(PCEUserRAM + 0x000000, 0x1f0000, 0x1f1fff, MAP_RAM); // mirrored
		h6280MapMemory(PCEUserRAM + 0x000000, 0x1f2000, 0x1f3fff, MAP_RAM);
		h6280MapMemory(PCEUserRAM + 0x000000, 0x1f4000, 0x1f5fff, MAP_RAM);
		h6280MapMemory(PCEUserRAM + 0x000000, 0x1f6000, 0x1f7fff, MAP_RAM);
		h6280SetWritePortHandler(pce_write_port);
		h6280SetWriteHandler(pce_write);
		h6280SetReadHandler(pce_read);

		if (strcmp(BurnDrvGetTextA(DRV_NAME), "pce_deepblue") == 0 || strcmp(BurnDrvGetTextA(DRV_NAME), "pce_f1pilot") == 0 ||
		    strcmp(BurnDrvGetTextA(DRV_NAME), "pce_wonderm") == 0) {
			bprintf(0, _T("**  (PCE) Using timing hack for F-1 Pilot / Wonder Momo / Deep Blue\n"));
			h6280SetVDCPenalty(0);
		}

		h6280Close();

		interrupt = pce_interrupt;
		hblank = pce_hblank;

		if (type == 0) {		// pce
			system_identify = 0x40;
		} else {			// tg16
			system_identify = 0x00;
		}
	}
	else if (type == 2) // sgx
	{
		h6280Init(0);
		h6280Open(0);
		h6280MapMemory(PCECartROM, 0x000000, 0x0fffff, MAP_ROM);
		h6280MapMemory(PCEUserRAM, 0x1f0000, 0x1f7fff, MAP_RAM);
		h6280SetWritePortHandler(sgx_write_port);
		h6280SetWriteHandler(sgx_write);
		h6280SetReadHandler(sgx_read);
		h6280Close();

		interrupt = sgx_interrupt;
		hblank = sgx_hblank;

		system_identify = 0x40;
	}
	
	bram_locked = 1;
	
	vdc_init();
	vce_palette_init(DrvPalette, NULL);

	c6280_init(3579545, 0, (strcmp(BurnDrvGetTextA(DRV_NAME), "pce_lostsunh") == 0) ? 1 : 0);
	c6280_set_renderer(PCEDips[2] & 0x80);
	c6280_set_route(BURN_SND_C6280_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	c6280_set_route(BURN_SND_C6280_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);

	GenericTilesInit();

	PCEDoReset();

	return 0;
}

INT32 PCEInit()
{
	return CommonInit(0);
}

INT32 TG16Init()
{
	return CommonInit(1);
}

INT32 SGXInit()
{
	return CommonInit(2);
}

INT32 populousInit()
{
	int nRet = PCEInit();

	if (nRet == 0) {
		h6280Open(0);
		h6280MapMemory(PCECartRAM, 0x080000, 0x087fff, MAP_RAM);
		h6280Close();
	}

	return nRet;
}

INT32 wondermomoInit()
{
	wondermomohack = 1;
	return PCEInit();
}

INT32 PCEExit()
{
	GenericTilesExit();

	c6280_exit();
	vdc_exit();

	h6280Exit();

	BurnFree (AllMem);

	pce_sf2 = 0;
	wondermomohack = 0;

	return 0;
}

static UINT32 alt_palette[0x200] = {
	0x00020101, 0x0002011a, 0x0003033e, 0x0002015b, 0x00030580, 0x0002019c, 0x000004c5, 0x000203e0, 0x001e0105, 0x00220526, 0x001f0042, 0x00230769, 0x001f0384, 0x002006ab, 0x001c05c9, 0x002208ee,
	0x00420711, 0x003e022a, 0x00450951, 0x0041046d, 0x00420b90, 0x003e07af, 0x00440ad5, 0x004009f2, 0x005e0415, 0x005c032e, 0x00610655, 0x005d0571, 0x00600894, 0x005c04b4, 0x00620bd8, 0x005e06f6,
	0x007c0518, 0x0081083a, 0x007f0759, 0x00830a7c, 0x007e0998, 0x00820cc1, 0x007f08dd, 0x00850fff, 0x00a00a24, 0x009f093e, 0x00a30c65, 0x00a10b80, 0x00a30ea9, 0x00a00dc5, 0x00a610ea, 0x00a30cff,
	0x00bc0b28, 0x00bb0645, 0x00c10d69, 0x00bd0884, 0x00c00fad, 0x00be0bc9, 0x00c40eee, 0x00c10dff, 0x00da082c, 0x00e10f53, 0x00dd0a6d, 0x00e01191, 0x00de0cb1, 0x00e413d5, 0x00e10ff2, 0x00e812ff,
	0x000b2403, 0x0007231c, 0x000b2643, 0x0004255d, 0x00082a82, 0x000426a1, 0x00092dc7, 0x000528e3, 0x00272507, 0x00232020, 0x00272747, 0x00202263, 0x00242b86, 0x002027a5, 0x00272acb, 0x002329e7,
	0x0043220b, 0x0047292c, 0x0045244b, 0x00462b6e, 0x0042268a, 0x00472fb2, 0x00452bcf, 0x00492ef4, 0x00692b16, 0x00652630, 0x00662d57, 0x00642872, 0x00683196, 0x00642cb7, 0x006b33da, 0x00672ff8,
	0x0085281a, 0x00812734, 0x00842a5b, 0x00802976, 0x00862e9d, 0x00822dbb, 0x008930df, 0x00852cfc, 0x00a1291e, 0x00a72c43, 0x00a02b5f, 0x00a62e82, 0x00a32fa1, 0x00a932c7, 0x00a72ee4, 0x00ac35ff,
	0x00c52e2a, 0x00c32d47, 0x00c6306a, 0x00c43186, 0x00c834af, 0x00c733cb, 0x00cd36f0, 0x00ca32ff, 0x00e32f2e, 0x00e12a4d, 0x00e2316e, 0x00e02c8a, 0x00e635b3, 0x00e530ce, 0x00eb37f4, 0x00e833ff,
	0x000b4301, 0x000c461e, 0x0008473d, 0x000c4a60, 0x0008497b, 0x000d4ea3, 0x00094ac0, 0x001251e5, 0x002f4808, 0x00284722, 0x002c4c49, 0x00284b64, 0x002f4e88, 0x002b4fa7, 0x00274bc5, 0x00304ee9,
	0x004b490c, 0x00444426, 0x004a4d4d, 0x00464868, 0x004b4f8c, 0x00494cab, 0x004d53d0, 0x004e4fee, 0x00694610, 0x006a4f35, 0x00664a51, 0x006c5174, 0x00694c90, 0x006f55b9, 0x006b50d4, 0x007659fd,
	0x008a4f1c, 0x00864c39, 0x008c535d, 0x00884e78, 0x008f579f, 0x008b52bd, 0x00894ed8, 0x009456ff, 0x00a64c20, 0x00a44d3d, 0x00a85060, 0x00a64f7c, 0x00ad54a4, 0x00a953c1, 0x00b456e6, 0x00b252ff,
	0x00c24d24, 0x00c8524b, 0x00c65164, 0x00cd5488, 0x00c955a9, 0x00cf58cc, 0x00d257ea, 0x00d85cff, 0x00e85533, 0x00e6534f, 0x00ec5670, 0x00e9558c, 0x00e750ad, 0x00ed59d0, 0x00f054ee, 0x00f65dff,
	0x00106801, 0x000c6718, 0x00106a3f, 0x000d6b59, 0x00116e7e, 0x000d6f9d, 0x001672c1, 0x00126ede, 0x002c6902, 0x00306c24, 0x002c6b43, 0x00337066, 0x002f6f82, 0x003874a9, 0x003473c6, 0x003978ec,
	0x00486606, 0x004e6d2b, 0x004a6847, 0x004f716a, 0x004b6c86, 0x005675ad, 0x005270ca, 0x005879f3, 0x006e6f12, 0x006a6a2f, 0x006f7153, 0x006d6e6e, 0x00717595, 0x007472b2, 0x007879d6, 0x007677f7,
	0x008a6c16, 0x008e733b, 0x008d6e57, 0x0093777a, 0x008f7299, 0x009a7bbe, 0x009676da, 0x009c7fff, 0x00a66d1a, 0x00ac703f, 0x00a96f5a, 0x00af747e, 0x00ad739d, 0x00b878c2, 0x00b477df, 0x00ba7cff,
	0x00cc7529, 0x00c87143, 0x00cf7666, 0x00cd7582, 0x00d878ab, 0x00d579c6, 0x00da7cef, 0x00d87dff, 0x00e8732d, 0x00ef7651, 0x00ed776a, 0x00f27a93, 0x00f579af, 0x00fb7ed2, 0x00f87ef3, 0x00ff82ff,
	0x00108701, 0x00158a1a, 0x00118b37, 0x00158e5a, 0x00168f76, 0x001a949f, 0x001693ba, 0x001b98e0, 0x00358c04, 0x00318b1e, 0x00359045, 0x0033905e, 0x003c9487, 0x003895a3, 0x003d98c8, 0x003b96e5,
	0x00518d08, 0x0057902d, 0x00539149, 0x005c966c, 0x005a958b, 0x005e9aaf, 0x005b99cc, 0x00619ef5, 0x006f8a0c, 0x00739131, 0x006f8e4d, 0x007a9770, 0x0076928f, 0x007c9bb3, 0x007996d0, 0x007f9ff9,
	0x00939218, 0x008f8e35, 0x00959758, 0x00989474, 0x009c9b9b, 0x009a98b8, 0x00a0a0dd, 0x009d9cfd, 0x00af901c, 0x00b59941, 0x00b3945c, 0x00bc9d83, 0x00ba989f, 0x00c1a1c4, 0x00be9ce1, 0x00c3a5ff,
	0x00cd9123, 0x00d19645, 0x00cf9560, 0x00da9a87, 0x00d899a4, 0x00df9ec8, 0x00db9ee9, 0x00e1a2ff, 0x00f1992f, 0x00ef974b, 0x00fa9a6c, 0x00f89b8b, 0x00fe9eb0, 0x00fc9fcc, 0x00ffa4f5, 0x00ffa3ff,
	0x0019aa01, 0x001db31f, 0x001eaf3b, 0x0022b75c, 0x001eb47c, 0x0025bda1, 0x0021b8bd, 0x001bb6da, 0x0035ab01, 0x0039b023, 0x003cb13f, 0x0040b462, 0x003cb581, 0x0041baa5, 0x003db9c1, 0x0043beeb,
	0x0059b30a, 0x0057b127, 0x0060b64a, 0x005cb566, 0x0063ba8d, 0x005fbba9, 0x0065bfce, 0x0061bfef, 0x0077b00e, 0x0080b933, 0x007eb74e, 0x0082ba72, 0x0081bb91, 0x007db8ac, 0x0083c0d2, 0x007fbcf3,
	0x0093ae12, 0x009eb737, 0x009ab452, 0x00a0bb79, 0x009db895, 0x00a3c1ba, 0x00a0bdd6, 0x00a2c5ff, 0x00b7b621, 0x00bab43b, 0x00c0bd5e, 0x00bcb87d, 0x00c3c1a1, 0x00c1bebe, 0x00c7c7e7, 0x00c0c2ff,
	0x00d5b725, 0x00e0bc46, 0x00dcba62, 0x00e3c389, 0x00e1bea7, 0x00dfbfc2, 0x00e5c4eb, 0x00dec3ff, 0x00f1b429, 0x00fcbd4c, 0x00fabb66, 0x00ffbf8d, 0x00ffbfaa, 0x00ffc4ce, 0x00ffc5ef, 0x00ffc9ff,
	0x0026d201, 0x0022ce19, 0x0026d73d, 0x0023d456, 0x0027dd7f, 0x0025d99b, 0x0026e2bf, 0x0022dedf, 0x0042d300, 0x003ecf1d, 0x0044d440, 0x0041d55a, 0x0045da83, 0x0041db9f, 0x0044dfc4, 0x0040dfe4,
	0x005ed004, 0x0064d729, 0x0060d544, 0x0067da68, 0x0063db87, 0x0067e0aa, 0x0062e0c8, 0x0066e5f1, 0x0084d913, 0x0080d52d, 0x0087da50, 0x0083db6c, 0x0089e093, 0x0085e1ae, 0x0088e6d7, 0x0084e6f5,
	0x00a0d617, 0x009ed231, 0x00a3db54, 0x00a1d873, 0x00a7e197, 0x00a3deb3, 0x00a6e7db, 0x00a2e3f9, 0x00bcd71b, 0x00c2da3c, 0x00c1d858, 0x00c7e17f, 0x00c3de9b, 0x00c6e7c0, 0x00c4e4e0, 0x00c9edff,
	0x00e2dc27, 0x00e0db40, 0x00e5e064, 0x00e3de83, 0x00e9e7a8, 0x00e4e4c4, 0x00eaeded, 0x00e7e9ff, 0x00fedd2b, 0x00fcd844, 0x00ffe168, 0x00ffdf87, 0x00ffe4ac, 0x00ffe5c8, 0x00ffeaf1, 0x00ffebff,
	0x0026f101, 0x002bf61b, 0x0027f435, 0x002bfd58, 0x0027f977, 0x002aff9c, 0x0026ffb8, 0x002bffe1, 0x004af602, 0x0047f71f, 0x004dfc42, 0x0049fa5c, 0x004aff85, 0x0047ffa0, 0x004cffc6, 0x0049ffe6,
	0x0068f709, 0x0065f523, 0x0069fa46, 0x0067fb62, 0x0068ff89, 0x0064ffa4, 0x006affcd, 0x0067ffeb, 0x0084f40d, 0x0089fd2f, 0x0087fb4a, 0x008bff71, 0x0086ff8d, 0x008affb0, 0x0088ffd1, 0x008efff7,
	0x00a9fd19, 0x00a7fa33, 0x00abff56, 0x00a9ff75, 0x00acff98, 0x00a8ffb5, 0x00afffde, 0x00acfffa, 0x00c5fa1d, 0x00c3fb36, 0x00c9ff5a, 0x00c7fe79, 0x00c8ff9c, 0x00c6ffba, 0x00cdffe2, 0x00caffff,
	0x00e3fb21, 0x00e9ff42, 0x00e5ff5e, 0x00e8ff85, 0x00e6ffa0, 0x00ecffc9, 0x00ebffe7, 0x00f1ffff, 0x00ffff2d, 0x00ffff46, 0x00ffff6d, 0x00ffff89, 0x00ffffae, 0x00ffffcd, 0x00fffff3, 0x00ffffff
};

INT32 PCEDraw()
{
	if (PCEPaletteRecalc) {
		vce_palette_init(DrvPalette, (PCEDips[2] & 0x20) ? &alt_palette[0] : NULL);
		PCEPaletteRecalc = 0;
	}

	{
		UINT16 *dst = pTransDraw;
		for (INT32 y = 0; y < nScreenHeight; y++) {

			memcpy(dst, vdc_get_line(y), nScreenWidth * sizeof(UINT16));

			dst += nScreenWidth;
		}
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void PCECompileInputs()
{
	memset(PCEInputs, 0xff, 5 * sizeof(UINT16));

	for (INT32 i = 0; i < 12; i++) {
		PCEInputs[0] ^= (PCEJoy1[i] & 1) << i;
		PCEInputs[1] ^= (PCEJoy2[i] & 1) << i;
		PCEInputs[2] ^= (PCEJoy3[i] & 1) << i;
		PCEInputs[3] ^= (PCEJoy4[i] & 1) << i;
		PCEInputs[4] ^= (PCEJoy5[i] & 1) << i;
	}
	for (INT32 i = 0; i < 5; i++) {
		if ((0 == nSocd[i]) || (nSocd[i] > 6)) continue;

		PCEInputs[i] = ~PCEInputs[i];
		clear_opposite.check(i, PCEInputs[i], 0x10, 0x40, 0x80, 0x20, nSocd[i]);
		PCEInputs[i] = ~PCEInputs[i];
	}

	if ((last_dip ^ PCEDips[2]) & 0x80) {
		bprintf(0, _T("Sound core switched to: %s\n"), (PCEDips[2] & 0x80) ? _T("HQ") : _T("LQ"));
		c6280_set_renderer(PCEDips[2] & 0x80);
	}
	if ((last_dip ^ PCEDips[2]) & 0x20) {
		bprintf(0, _T("Palette switched to: %s\n"), (PCEDips[2] & 0x20) ? _T("Alternate") : _T("Normal"));
		PCEPaletteRecalc = 1;
	}
	last_dip = PCEDips[2];
}

INT32 PCEFrame()
{
	if (PCEReset) {
		PCEDoReset();
	}

	h6280NewFrame(); // needed for c6280

	PCECompileInputs();

	INT32 nInterleave = vce_linecount();
	INT32 nCyclesTotal[1] = { (INT32)((INT64)7159090 * nBurnCPUSpeedAdjust / (0x0100 * 60)) };
	INT32 nCyclesDone[1] = { nExtraCycles };

	h6280Open(0);
	h6280Idle(nExtraCycles);
	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone[0] += h6280Run(1128/3); // 1128 m-cycles brings us to hblank. "/ 3" m-cycles -> cpu cycles
		hblank();
		// if thinking of changing this, make sure to check for these side effects:
		// blodia, game select bounce
		// dragon egg, hang booting
		// air zonk, crash ingame when moving up
		// side arms, playfield bounce ingame (note: bottom selection cut-off is normal - if you're seeing the whole thing, you've most certainly broken other games.)
		// huzero, corruption above the "Sc 000000"
		// cadash, bouncing playfield, corrupted HUD *not entirely fixed - game intro*
		// ghost manor, crash while going down the slide / moving around a bit
		CPU_RUN_SYNCINT(0, h6280); // finish line cycles
		interrupt();       // advance line in vdc & vbl @ last line
	}

	if (pBurnSoundOut) {
		c6280_update(pBurnSoundOut, nBurnSoundLen);
	}

	nExtraCycles = h6280TotalCycles() - nCyclesTotal[0];

	h6280Close();

	if (pBurnDraw) {
		PCEDraw();
	}

	return 0;
}

INT32 PCEScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029698;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		h6280Scan(nAction);

		vdc_scan(nAction, pnMin);
		c6280_scan(nAction, pnMin);

		SCAN_VAR(joystick_port_select);
		SCAN_VAR(joystick_data_select);
		SCAN_VAR(joystick_6b_select);
		SCAN_VAR(bram_locked);

		SCAN_VAR(nExtraCycles);

		clear_opposite.scan();

		if (pce_sf2) {
			SCAN_VAR(pce_sf2_bank);
			sf2_bankswitch(pce_sf2_bank);
		}
	}

	return 0;
}
