// FB Alpha PC-Engine / TurboGrafx 16 / SuperGrafx driver module
// Based on MESS driver by Charles MacDonald

#include "tiles_generic.h"
#include "h6280_intf.h"
#include "vdc.h"
#include "c6280.h"
#include "bitswap.h"

/*
Notes:

	There is no CD emulation at all.
	As this driver is based on MESS emulation, compatibility *should* be the same.

	Known emulation issues - also present in MESS unless noted.
	SOUND PROBLEMS
		Champions Forever Boxing
		Bouken Danshaku Don - The Lost Sunheart

	GRAPHICS PROBLEMS
		Cadash - graphics shaking

	OTHER PROBLEMS
		Niko Niko Pun - hangs in-game
		Benkei Gaiden - hangs after sunsoft logo
		Power Tennis - frozen
		Tennokoe Bank - ??
		Air Zonk / PC Denjin - Punkic Cyborgs - hangs in-game
		Hisou Kihei - Xserd: black screen
*/

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *PCECartROM;
static UINT8 *PCECartRAM;
static UINT8 *PCEUserRAM;

static UINT32 *DrvPalette;
static UINT8 PCEPaletteRecalc;

static UINT8 joystick_port_select;
static UINT8 joystick_data_select;
static UINT8 joystick_6b_select[5];

static void (*interrupt)();

static UINT16 PCEInputs[5];
static UINT8 PCEReset;
static UINT8 PCEJoy1[12];
static UINT8 PCEJoy2[12];
static UINT8 PCEJoy3[12];
static UINT8 PCEJoy4[12];
static UINT8 PCEJoy5[12];
static UINT8 PCEDips[3];

static UINT8 system_identify;
static INT32 pce_sf2 = 0;
static INT32 pce_sf2_bank;

static struct BurnInputInfo pceInputList[] = {
	{"P1 Start",		BIT_DIGITAL,	PCEJoy1 + 3,	"p1 start"	}, // 0
	{"P1 Select",		BIT_DIGITAL,	PCEJoy1 + 2,	"p1 select"	},
	{"P1 Up",		BIT_DIGITAL,	PCEJoy1 + 4,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	PCEJoy1 + 6,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	PCEJoy1 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	PCEJoy1 + 5,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	PCEJoy1 + 1,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	PCEJoy1 + 0,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	PCEJoy1 + 8,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	PCEJoy1 + 9,	"p1 fire 4"	},
	{"P1 Button 5",		BIT_DIGITAL,	PCEJoy1 + 10,	"p1 fire 5"	},
	{"P1 Button 6",		BIT_DIGITAL,	PCEJoy1 + 11,	"p1 fire 6"	},

	{"P2 Start",		BIT_DIGITAL,	PCEJoy2 + 3,	"p2 start"	}, // 12
	{"P2 Select",		BIT_DIGITAL,	PCEJoy2 + 2,	"p2 select"	},
	{"P2 Up",		BIT_DIGITAL,	PCEJoy2 + 4,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	PCEJoy2 + 6,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	PCEJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	PCEJoy2 + 5,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	PCEJoy2 + 1,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	PCEJoy2 + 0,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	PCEJoy2 + 8,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	PCEJoy2 + 9,	"p2 fire 4"	},
	{"P2 Button 5",		BIT_DIGITAL,	PCEJoy2 + 10,	"p2 fire 5"	},
	{"P2 Button 6",		BIT_DIGITAL,	PCEJoy2 + 11,	"p2 fire 6"	},

	{"P3 Start",		BIT_DIGITAL,	PCEJoy3 + 3,	"p3 start"	}, // 24
	{"P3 Select",		BIT_DIGITAL,	PCEJoy3 + 2,	"p3 select"	},
	{"P3 Up",		BIT_DIGITAL,	PCEJoy3 + 4,	"p3 up"		},
	{"P3 Down",		BIT_DIGITAL,	PCEJoy3 + 6,	"p3 down"	},
	{"P3 Left",		BIT_DIGITAL,	PCEJoy3 + 7,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	PCEJoy3 + 5,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	PCEJoy3 + 1,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	PCEJoy3 + 0,	"p3 fire 2"	},
	{"P3 Button 3",		BIT_DIGITAL,	PCEJoy3 + 8,	"p3 fire 3"	},
	{"P3 Button 4",		BIT_DIGITAL,	PCEJoy3 + 9,	"p3 fire 4"	},
	{"P3 Button 5",		BIT_DIGITAL,	PCEJoy3 + 10,	"p3 fire 5"	},
	{"P3 Button 6",		BIT_DIGITAL,	PCEJoy3 + 11,	"p3 fire 6"	},

	{"P4 Start",		BIT_DIGITAL,	PCEJoy4 + 3,	"p4 start"	}, // 36
	{"P4 Select",		BIT_DIGITAL,	PCEJoy4 + 2,	"p4 select"	},
	{"P4 Up",		BIT_DIGITAL,	PCEJoy4 + 4,	"p4 up"		},
	{"P4 Down",		BIT_DIGITAL,	PCEJoy4 + 6,	"p4 down"	},
	{"P4 Left",		BIT_DIGITAL,	PCEJoy4 + 7,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	PCEJoy4 + 5,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	PCEJoy4 + 1,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	PCEJoy4 + 0,	"p4 fire 2"	},
	{"P4 Button 3",		BIT_DIGITAL,	PCEJoy4 + 8,	"p4 fire 3"	},
	{"P4 Button 4",		BIT_DIGITAL,	PCEJoy4 + 9,	"p4 fire 4"	},
	{"P4 Button 5",		BIT_DIGITAL,	PCEJoy4 + 10,	"p4 fire 5"	},
	{"P4 Button 6",		BIT_DIGITAL,	PCEJoy4 + 11,	"p4 fire 6"	},

	{"P5 Start",		BIT_DIGITAL,	PCEJoy5 + 3,	"p5 start"	}, // 48
	{"P5 Select",		BIT_DIGITAL,	PCEJoy5 + 2,	"p5 select"	},
	{"P5 Up",		BIT_DIGITAL,	PCEJoy5 + 4,	"p5 up"		},
	{"P5 Down",		BIT_DIGITAL,	PCEJoy5 + 6,	"p5 down"	},
	{"P5 Left",		BIT_DIGITAL,	PCEJoy5 + 7,	"p5 left"	},
	{"P5 Right",		BIT_DIGITAL,	PCEJoy5 + 5,	"p5 right"	},
	{"P5 Button 1",		BIT_DIGITAL,	PCEJoy5 + 1,	"p5 fire 1"	},
	{"P5 Button 2",		BIT_DIGITAL,	PCEJoy5 + 0,	"p5 fire 2"	},
	{"P5 Button 3",		BIT_DIGITAL,	PCEJoy5 + 8,	"p5 fire 3"	},
	{"P5 Button 4",		BIT_DIGITAL,	PCEJoy5 + 9,	"p5 fire 4"	},
	{"P5 Button 5",		BIT_DIGITAL,	PCEJoy5 + 10,	"p5 fire 5"	},
	{"P5 Button 6",		BIT_DIGITAL,	PCEJoy5 + 11,	"p5 fire 6"	},

	{"Reset",		BIT_DIGITAL,	&PCEReset,	"reset"		}, // 60
	{"Dip A",		BIT_DIPSWITCH,	PCEDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	PCEDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	PCEDips + 2,	"dip"		},
};

STDINPUTINFO(pce)

static struct BurnDIPInfo pceDIPList[]=
{
	{0x3d, 0xff, 0xff, 0x00, NULL				},
	{0x3e, 0xff, 0xff, 0x00, NULL				},
	{0x3f, 0xff, 0xff, 0x01, NULL				},

	{0   , 0xfe, 0   ,    2, "Joystick Type Player 1"	},
	{0x3d, 0x01, 0x03, 0x00, "2-buttons"			},
	{0x3d, 0x01, 0x03, 0x02, "6-buttons"			},

	{0   , 0xfe, 0   ,    2, "Joystick Type Player 2"	},
	{0x3d, 0x01, 0x0c, 0x00, "2-buttons"			},
	{0x3d, 0x01, 0x0c, 0x08, "6-buttons"			},

	{0   , 0xfe, 0   ,    2, "Joystick Type Player 3"	},
	{0xdd, 0x01, 0x30, 0x00, "2-buttons"			},
	{0x3d, 0x01, 0x30, 0x20, "6-buttons"			},

	{0   , 0xfe, 0   ,    2, "Joystick Type Player 4"	},
	{0x3d, 0x01, 0xc0, 0x00, "2-buttons"			},
	{0x3d, 0x01, 0xc0, 0x80, "6-buttons"			},

	{0   , 0xfe, 0   ,    2, "Joystick Type Player 5"	},
	{0x3e, 0x01, 0x03, 0x00, "2-buttons"			},
	{0x3e, 0x01, 0x03, 0x02, "6-buttons"			},

	{0   , 0xfe, 0   ,    2, "Arcade Card"			},
	{0x3f, 0x01, 0x01, 0x00, "Off"				},
	{0x3f, 0x01, 0x01, 0x01, "On"				},
};

STDDIPINFO(pce)

static INT32 PceGetZipName(char** pszName, UINT32 i)
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

	if (pszGameName == NULL) {
		*pszName = NULL;
		return 1;
	}

	// remove the "pce_"
	for (UINT32 j = 0; j < strlen(pszGameName); j++) {
		szFilename[j] = pszGameName[j + 4];
	}
	strcat(szFilename, ".zip");

	*pszName = szFilename;

	return 0;
}

static INT32 TgGetZipName(char** pszName, UINT32 i)
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

	if (pszGameName == NULL) {
		*pszName = NULL;
		return 1;
	}

	// remove the "tg_"
	for (UINT32 j = 0; j < strlen(pszGameName); j++) {
		szFilename[j] = pszGameName[j + 3];
	}
	strcat(szFilename, ".zip");

	*pszName = szFilename;

	return 0;
}

static INT32 SgxGetZipName(char** pszName, UINT32 i)
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

	if (pszGameName == NULL) {
		*pszName = NULL;
		return 1;
	}

	// remove the "sgx_"
	for (UINT32 j = 0; j < strlen(pszGameName); j++) {
		szFilename[j] = pszGameName[j + 4];
	}
	strcat(szFilename, ".zip");

	*pszName = szFilename;

	return 0;
}

static void sf2_bankswitch(UINT8 offset)
{
	pce_sf2_bank = offset;

	h6280MapMemory(PCECartROM + (offset * 0x80000) + 0x080000, 0x080000, 0x0fffff, H6280_ROM);
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
			c6280_write(address, data);
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

		case 0x1ff800:
			// cd system
		return;
	}
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
			return 0; // cd system
	}

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

static INT32 pce_cpu_sound_sync()
{
	return (INT32)((INT64)7159090 * nBurnCPUSpeedAdjust / (0x0100 * 60));
}

static INT32 MemIndex(UINT32 cart_size, INT32 type)
{
	UINT8 *Next; Next = AllMem;

	PCECartROM	= Next; Next += (cart_size <= 0x100000) ? 0x100000 : cart_size;

	DrvPalette	= (UINT32*)Next; Next += 0x0401 * sizeof(UINT32);

	AllRam		= Next;

	PCEUserRAM	= Next; Next += (type == 2) ? 0x008000 : 0x002000; // pce/tg16 0x2000, sgx 0x8000

	PCECartRAM	= Next; Next += 0x008000; // populous

	vce_data	= (UINT16*)Next; Next += 0x200 * sizeof(UINT16);

	vdc_vidram[0]	= Next; Next += 0x010000;
	vdc_vidram[1]	= Next; Next += 0x010000; // sgx

	RamEnd		= Next;

	vdc_tmp_draw	= (UINT16*)Next; Next += 684 * 262 * sizeof(UINT16);

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
		h6280Init(1);
		h6280Open(0);
		h6280MapMemory(PCECartROM + 0x000000, 0x000000, 0x0fffff, H6280_ROM);
		h6280MapMemory(PCEUserRAM + 0x000000, 0x1f0000, 0x1f1fff, H6280_RAM); // mirrored
		h6280MapMemory(PCEUserRAM + 0x000000, 0x1f2000, 0x1f3fff, H6280_RAM);
		h6280MapMemory(PCEUserRAM + 0x000000, 0x1f4000, 0x1f5fff, H6280_RAM);
		h6280MapMemory(PCEUserRAM + 0x000000, 0x1f6000, 0x1f7fff, H6280_RAM);
		h6280SetWritePortHandler(pce_write_port);
		h6280SetWriteHandler(pce_write);
		h6280SetReadHandler(pce_read);
		h6280Close();

		interrupt = pce_interrupt;

		if (type == 0) {		// pce
			system_identify = 0x40;
		} else {			// tg16
			system_identify = 0x00;
		}
	}
	else if (type == 2) // sgx
	{
		h6280Init(1);
		h6280Open(0);
		h6280MapMemory(PCECartROM, 0x000000, 0x0fffff, H6280_ROM);
		h6280MapMemory(PCEUserRAM, 0x1f0000, 0x1f7fff, H6280_RAM);
		h6280SetWritePortHandler(sgx_write_port);
		h6280SetWriteHandler(sgx_write);
		h6280SetReadHandler(sgx_read);
		h6280Close();

		interrupt = sgx_interrupt;
		system_identify = 0x40;
	}

	vce_palette_init(DrvPalette);

	c6280_init(3579545, 0);

	GenericTilesInit();

	PCEDoReset();

	return 0;
}

static INT32 PCEInit()
{
//	bprintf (0, _T("booting PCE!\n"));
	return CommonInit(0);
}

static INT32 TG16Init()
{
//	bprintf (0, _T("booting TG16!\n"));
	return CommonInit(1);
}

static INT32 SGXInit()
{
//	bprintf (0, _T("booting SGX!\n"));
	return CommonInit(2);
}

static INT32 PCEExit()
{
	GenericTilesExit();

	c6280_exit();
	// video exit

	h6280Exit();

	BurnFree (AllMem);

	pce_sf2 = 0;

	return 0;
}

static INT32 PCEDraw()
{
	if (PCEPaletteRecalc) {
		vce_palette_init(DrvPalette);
		PCEPaletteRecalc = 0;
	}

	{
		UINT16 *src = vdc_tmp_draw + (14 * 684) + 86;
		UINT16 *dst = pTransDraw;
	
		for (INT32 y = 0; y < nScreenHeight; y++) {
			for (INT32 x = 0; x < nScreenWidth; x++) {
				dst[x] = src[x];
			}
			dst += nScreenWidth;
			src += 684;
		}
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void PCECompileInputs()
{
	memset (PCEInputs, 0xff, 5 * sizeof(UINT16));

	for (INT32 i = 0; i < 12; i++) {
		PCEInputs[0] ^= (PCEJoy1[i] & 1) << i;
		PCEInputs[1] ^= (PCEJoy2[i] & 1) << i;
		PCEInputs[2] ^= (PCEJoy3[i] & 1) << i;
		PCEInputs[3] ^= (PCEJoy4[i] & 1) << i;
		PCEInputs[4] ^= (PCEJoy5[i] & 1) << i;
	}
}

static INT32 PCEFrame()
{
	if (PCEReset) {
		PCEDoReset();
	}

	h6280NewFrame();

	PCECompileInputs();

	INT32 nCyclesTotal = (INT32)((INT64)7159090 * nBurnCPUSpeedAdjust / (0x0100 * 60));
	
	h6280Open(0);
	
	for (INT32 i = 0; i < 262; i++)
	{
		h6280Run(nCyclesTotal / 262);
		interrupt();
	}
	
	if (pBurnSoundOut) {
		c6280_update(pBurnSoundOut, nBurnSoundLen);
	}

	h6280Close();

	if (pBurnDraw) {
		PCEDraw();
	}

	return 0;
}

static INT32 PCEScan(INT32 nAction, INT32 *pnMin)
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
		h6280CpuScan(nAction);

		vdc_scan(nAction, pnMin);
		c6280_scan(nAction, pnMin);

		SCAN_VAR(joystick_port_select);
		SCAN_VAR(joystick_data_select);
		SCAN_VAR(joystick_6b_select[0]);
		SCAN_VAR(joystick_6b_select[1]);
		SCAN_VAR(joystick_6b_select[2]);
		SCAN_VAR(joystick_6b_select[3]);
		SCAN_VAR(joystick_6b_select[4]);

		if (pce_sf2) {
			SCAN_VAR(pce_sf2_bank);
			sf2_bankswitch(pce_sf2_bank);
		}
	}

	return 0;
}

// 1943 Kai

static struct BurnRomInfo p1943kaiRomDesc[] = {
	{ "1943 kai (japan).pce",	0x80000, 0xfde08d6d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(p1943kai)
STD_ROM_FN(p1943kai)

struct BurnDriver BurnDrvPCE_p1943kai = {
	"pce_1943kai", NULL, NULL, NULL, "1991",
	"1943 Kai\0", NULL, "Naxat", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, p1943kaiRomInfo, p1943kaiRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// 21 Emon - Mezase Hotel ou!!

static struct BurnRomInfo p21emonRomDesc[] = {
	{ "21 emon - mezase hotel ou!! (japan).pce",	0x80000, 0x73614660, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(p21emon)
STD_ROM_FN(p21emon)

struct BurnDriver BurnDrvPCE_p21emon = {
	"pce_21emon", NULL, NULL, NULL, "1994",
	"21 Emon - Mezase Hotel ou!!\0", NULL, "NEC Home Electronics", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, p21emonRomInfo, p21emonRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Adventure Island

static struct BurnRomInfo padvislndRomDesc[] = {
	{ "adventure island (japan).pce",	0x40000, 0x8e71d4f3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(padvislnd)
STD_ROM_FN(padvislnd)

struct BurnDriver BurnDrvPCE_padvislnd = {
	"pce_advislnd", NULL, NULL, NULL, "1991",
	"Adventure Island\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, padvislndRomInfo, padvislndRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Aero Blasters

static struct BurnRomInfo paeroblstRomDesc[] = {
	{ "aero blasters (japan).pce",	0x80000, 0x25be2b81, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(paeroblst)
STD_ROM_FN(paeroblst)

struct BurnDriver BurnDrvPCE_paeroblst = {
	"pce_aeroblst", NULL, NULL, NULL, "1990",
	"Aero Blasters\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, paeroblstRomInfo, paeroblstRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// After Burner II

static struct BurnRomInfo paburner2RomDesc[] = {
	{ "after burner ii (japan).pce",	0x80000, 0xca72a828, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(paburner2)
STD_ROM_FN(paburner2)

struct BurnDriver BurnDrvPCE_paburner2 = {
	"pce_aburner2", NULL, NULL, NULL, "1990",
	"After Burner II\0", NULL, "NEC Avenue", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, paburner2RomInfo, paburner2RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Alien Crush

static struct BurnRomInfo pacrushRomDesc[] = {
	{ "alien crush (japan).pce",	0x40000, 0x60edf4e1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pacrush)
STD_ROM_FN(pacrush)

struct BurnDriver BurnDrvPCE_pacrush = {
	"pce_acrush", NULL, NULL, NULL, "1988",
	"Alien Crush\0", NULL, "Naxat", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pacrushRomInfo, pacrushRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Ankoku Densetsu

static struct BurnRomInfo pankokuRomDesc[] = {
	{ "ankoku densetsu (japan).pce",	0x40000, 0xcacc06fb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pankoku)
STD_ROM_FN(pankoku)

struct BurnDriver BurnDrvPCE_pankoku = {
	"pce_ankoku", NULL, NULL, NULL, "1990",
	"Ankoku Densetsu\0", NULL, "Victor Interactive Software", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pankokuRomInfo, pankokuRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Aoi Blink

static struct BurnRomInfo paoiblinkRomDesc[] = {
	{ "aoi blink (japan).pce",	0x60000, 0x08a09b9a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(paoiblink)
STD_ROM_FN(paoiblink)

struct BurnDriver BurnDrvPCE_paoiblink = {
	"pce_aoiblink", NULL, NULL, NULL, "1990",
	"Aoi Blink\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, paoiblinkRomInfo, paoiblinkRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Appare Gateball

static struct BurnRomInfo pappgatebRomDesc[] = {
	{ "appare gateball (japan).pce",	0x40000, 0x2b54cba2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pappgateb)
STD_ROM_FN(pappgateb)

struct BurnDriver BurnDrvPCE_pappgateb = {
	"pce_appgateb", NULL, NULL, NULL, "1988",
	"Appare Gateball\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pappgatebRomInfo, pappgatebRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Armed Formation F

static struct BurnRomInfo parmedfRomDesc[] = {
	{ "armed formation f (japan).pce",	0x40000, 0x20ef87fd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(parmedf)
STD_ROM_FN(parmedf)

struct BurnDriver BurnDrvPCE_parmedf = {
	"pce_armedf", NULL, NULL, NULL, "1990",
	"Armed Formation F\0", NULL, "Pack-In-Video", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, parmedfRomInfo, parmedfRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Artist Tool

static struct BurnRomInfo parttoolRomDesc[] = {
	{ "artist tool (japan).pce",	0x40000, 0x5e4fa713, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(parttool)
STD_ROM_FN(parttool)

struct BurnDriver BurnDrvPCE_parttool = {
	"pce_arttool", NULL, NULL, NULL, "1990",
	"Artist Tool\0", NULL, "NEC Home Electronics", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, parttoolRomInfo, parttoolRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Atomic Robo-kid Special

static struct BurnRomInfo probokidsRomDesc[] = {
	{ "atomic robo-kid special (japan).pce",	0x80000, 0xdd175efd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(probokids)
STD_ROM_FN(probokids)

struct BurnDriver BurnDrvPCE_probokids = {
	"pce_robokids", NULL, NULL, NULL, "1990",
	"Atomic Robo-kid Special\0", NULL, "UPL", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, probokidsRomInfo, probokidsRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// AV Poker

static struct BurnRomInfo pavpokerRomDesc[] = {
	{ "av poker (japan).pce",	0x80000, 0xb866d282, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pavpoker)
STD_ROM_FN(pavpoker)

struct BurnDriver BurnDrvPCE_pavpoker = {
	"pce_avpoker", NULL, NULL, NULL, "19??",
	"AV Poker\0", NULL, "Unknown", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pavpokerRomInfo, pavpokerRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Ballistix

static struct BurnRomInfo pballistxRomDesc[] = {
	{ "ballistix (japan).pce",	0x40000, 0x8acfc8aa, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pballistx)
STD_ROM_FN(pballistx)

struct BurnDriver BurnDrvPCE_pballistx = {
	"pce_ballistx", NULL, NULL, NULL, "1991",
	"Ballistix\0", NULL, "Coconuts Japan", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pballistxRomInfo, pballistxRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Bari Bari Densetsu

static struct BurnRomInfo pbaribariRomDesc[] = {
	{ "bari bari densetsu (japan).pce",	0x60000, 0xc267e25d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pbaribari)
STD_ROM_FN(pbaribari)

struct BurnDriver BurnDrvPCE_pbaribari = {
	"pce_baribari", NULL, NULL, NULL, "1989",
	"Bari Bari Densetsu\0", NULL, "Taito", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pbaribariRomInfo, pbaribariRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Barunba

static struct BurnRomInfo pbarunbaRomDesc[] = {
	{ "barunba (japan).pce",	0x80000, 0x4a3df3ca, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pbarunba)
STD_ROM_FN(pbarunba)

struct BurnDriver BurnDrvPCE_pbarunba = {
	"pce_barunba", NULL, NULL, NULL, "1990",
	"Barunba\0", NULL, "Namco", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pbarunbaRomInfo, pbarunbaRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Batman

static struct BurnRomInfo pbatmanRomDesc[] = {
	{ "batman (japan).pce",	0x60000, 0x106bb7b2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pbatman)
STD_ROM_FN(pbatman)

struct BurnDriver BurnDrvPCE_pbatman = {
	"pce_batman", NULL, NULL, NULL, "1990",
	"Batman\0", NULL, "Sunsoft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pbatmanRomInfo, pbatmanRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Battle Lode Runner

static struct BurnRomInfo pbatloderRomDesc[] = {
	{ "battle lode runner (japan).pce",	0x40000, 0x59e44f45, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pbatloder)
STD_ROM_FN(pbatloder)

struct BurnDriver BurnDrvPCE_pbatloder = {
	"pce_batloder", NULL, NULL, NULL, "1993",
	"Battle Lode Runner\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pbatloderRomInfo, pbatloderRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Be Ball

static struct BurnRomInfo pbeballRomDesc[] = {
	{ "be ball (japan).pce",	0x40000, 0xe439f299, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pbeball)
STD_ROM_FN(pbeball)

struct BurnDriver BurnDrvPCE_pbeball = {
	"pce_beball", NULL, NULL, NULL, "1990",
	"Be Ball\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pbeballRomInfo, pbeballRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Benkei Gaiden

static struct BurnRomInfo pbenkeiRomDesc[] = {
	{ "benkei gaiden (japan).pce",	0x60000, 0xe1a73797, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pbenkei)
STD_ROM_FN(pbenkei)

struct BurnDriverD BurnDrvPCE_pbenkei = {
	"pce_benkei", NULL, NULL, NULL, "1989",
	"Benkei Gaiden\0", NULL, "Sunsoft", "PC Engine",
	NULL, NULL, NULL, NULL,
	0, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pbenkeiRomInfo, pbenkeiRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Benkei Gaiden (Alt)

static struct BurnRomInfo pbenkei1RomDesc[] = {
	{ "benkei gaiden (japan) [a].pce",	0x60000, 0xc9626a43, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pbenkei1)
STD_ROM_FN(pbenkei1)

struct BurnDriverD BurnDrvPCE_pbenkei1 = {
	"pce_benkei1", "pce_benkei", NULL, NULL, "1989",
	"Benkei Gaiden (Alt)\0", NULL, "Sunsoft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_CLONE, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pbenkei1RomInfo, pbenkei1RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Bikkuriman World

static struct BurnRomInfo pbikkuriRomDesc[] = {
	{ "bikkuriman world (japan).pce",	0x40000, 0x2841fd1e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pbikkuri)
STD_ROM_FN(pbikkuri)

struct BurnDriver BurnDrvPCE_pbikkuri = {
	"pce_bikkuri", NULL, NULL, NULL, "1987",
	"Bikkuriman World\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pbikkuriRomInfo, pbikkuriRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Bikkuriman World (Alt)

static struct BurnRomInfo pbikkuri1RomDesc[] = {
	{ "bikkuriman world (japan) [a].pce",	0x40000, 0x34893891, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pbikkuri1)
STD_ROM_FN(pbikkuri1)

struct BurnDriver BurnDrvPCE_pbikkuri1 = {
	"pce_bikkuri1", "pce_bikkuri", NULL, NULL, "1987",
	"Bikkuriman World (Alt)\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pbikkuri1RomInfo, pbikkuri1RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Blodia

static struct BurnRomInfo pblodiaRomDesc[] = {
	{ "blodia (japan).pce",	0x20000, 0x958bcd09, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pblodia)
STD_ROM_FN(pblodia)

struct BurnDriver BurnDrvPCE_pblodia = {
	"pce_blodia", NULL, NULL, NULL, "1990",
	"Blodia\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pblodiaRomInfo, pblodiaRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Body Conquest II

static struct BurnRomInfo pbodycon2RomDesc[] = {
	{ "body conquest ii (japan).pce",	0x80000, 0xffd92458, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pbodycon2)
STD_ROM_FN(pbodycon2)

struct BurnDriver BurnDrvPCE_pbodycon2 = {
	"pce_bodycon2", NULL, NULL, NULL, "19??",
	"Body Conquest II\0", NULL, "Unknown", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pbodycon2RomInfo, pbodycon2RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Bomberman '93

static struct BurnRomInfo pbombmn93RomDesc[] = {
	{ "bomberman '93 (japan).pce",	0x80000, 0xb300c5d0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pbombmn93)
STD_ROM_FN(pbombmn93)

struct BurnDriver BurnDrvPCE_pbombmn93 = {
	"pce_bombmn93", NULL, NULL, NULL, "1992",
	"Bomberman '93\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pbombmn93RomInfo, pbombmn93RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Bomberman '93 (Special Version)

static struct BurnRomInfo pbombmn93sRomDesc[] = {
	{ "bomberman '93 (special version) (japan).pce",	0x80000, 0x02309aa0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pbombmn93s)
STD_ROM_FN(pbombmn93s)

struct BurnDriver BurnDrvPCE_pbombmn93s = {
	"pce_bombmn93s", "pce_bombmn93", NULL, NULL, "1992",
	"Bomberman '93 (Special Version)\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pbombmn93sRomInfo, pbombmn93sRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Bomberman '94

static struct BurnRomInfo pbombmn94RomDesc[] = {
	{ "bomberman '94 (japan).pce",	0x100000, 0x05362516, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pbombmn94)
STD_ROM_FN(pbombmn94)

struct BurnDriver BurnDrvPCE_pbombmn94 = {
	"pce_bombmn94", NULL, NULL, NULL, "1993",
	"Bomberman '94\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pbombmn94RomInfo, pbombmn94RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Bomberman

static struct BurnRomInfo pbombmanRomDesc[] = {
	{ "bomberman (japan).pce",	0x40000, 0x9abb4d1f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pbombman)
STD_ROM_FN(pbombman)

struct BurnDriver BurnDrvPCE_pbombman = {
	"pce_bombman", NULL, NULL, NULL, "1990",
	"Bomberman\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pbombmanRomInfo, pbombmanRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Bomberman - Users Battle

static struct BurnRomInfo pbombmnubRomDesc[] = {
	{ "bomberman - users battle (japan).pce",	0x40000, 0x1489fa51, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pbombmnub)
STD_ROM_FN(pbombmnub)

struct BurnDriver BurnDrvPCE_pbombmnub = {
	"pce_bombmnub", NULL, NULL, NULL, "19??",
	"Bomberman - Users Battle\0", NULL, "Unknown", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pbombmnubRomInfo, pbombmnubRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Bouken Danshaku Don - The Lost Sunheart

static struct BurnRomInfo plostsunhRomDesc[] = {
	{ "bouken danshaku don - the lost sunheart (japan).pce",	0x80000, 0x8f4d9f94, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(plostsunh)
STD_ROM_FN(plostsunh)

struct BurnDriver BurnDrvPCE_plostsunh = {
	"pce_lostsunh", NULL, NULL, NULL, "1992",
	"Bouken Danshaku Don - The Lost Sunheart\0", "Bad sound", "IMax", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, plostsunhRomInfo, plostsunhRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Break In

static struct BurnRomInfo pbreakinRomDesc[] = {
	{ "break in (japan).pce",	0x40000, 0xc9d7426a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pbreakin)
STD_ROM_FN(pbreakin)

struct BurnDriver BurnDrvPCE_pbreakin = {
	"pce_breakin", NULL, NULL, NULL, "1989",
	"Break In\0", NULL, "Naxat", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pbreakinRomInfo, pbreakinRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Bubblegum Crash! - Knight Sabers 2034

static struct BurnRomInfo pbubblegmRomDesc[] = {
	{ "bubblegum crash! - knight sabers 2034 (japan).pce",	0x80000, 0x0d766139, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pbubblegm)
STD_ROM_FN(pbubblegm)

struct BurnDriver BurnDrvPCE_pbubblegm = {
	"pce_bubblegm", NULL, NULL, NULL, "1991",
	"Bubblegum Crash! - Knight Sabers 2034\0", NULL, "Naxat", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pbubblegmRomInfo, pbubblegmRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Bull Fight - Ring no Haja

static struct BurnRomInfo pbullfghtRomDesc[] = {
	{ "bull fight - ring no haja (japan).pce",	0x60000, 0x5c4d1991, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pbullfght)
STD_ROM_FN(pbullfght)

struct BurnDriver BurnDrvPCE_pbullfght = {
	"pce_bullfght", NULL, NULL, NULL, "1989",
	"Bull Fight - Ring no Haja\0", NULL, "Cream", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pbullfghtRomInfo, pbullfghtRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Burning Angels

static struct BurnRomInfo pburnanglRomDesc[] = {
	{ "burning angels (japan).pce",	0x60000, 0xd233c05a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pburnangl)
STD_ROM_FN(pburnangl)

struct BurnDriver BurnDrvPCE_pburnangl = {
	"pce_burnangl", NULL, NULL, NULL, "1990",
	"Burning Angels\0", NULL, "Naxat", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pburnanglRomInfo, pburnanglRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Busou Keiji - Cyber Cross

static struct BurnRomInfo pcyberxRomDesc[] = {
	{ "busou keiji - cyber cross (japan).pce",	0x60000, 0xd0c250ca, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pcyberx)
STD_ROM_FN(pcyberx)

struct BurnDriver BurnDrvPCE_pcyberx = {
	"pce_cyberx", NULL, NULL, NULL, "1989",
	"Busou Keiji - Cyber Cross\0", NULL, "Face", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pcyberxRomInfo, pcyberxRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Cadash

static struct BurnRomInfo pcadashRomDesc[] = {
	{ "cadash (japan).pce",	0x60000, 0x8dc0d85f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pcadash)
STD_ROM_FN(pcadash)

struct BurnDriver BurnDrvPCE_pcadash = {
	"pce_cadash", NULL, NULL, NULL, "1991",
	"Cadash\0", "Bad graphics", "Taito", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pcadashRomInfo, pcadashRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Champion Wrestler

static struct BurnRomInfo pchampwrsRomDesc[] = {
	{ "champion wrestler (japan).pce",	0x60000, 0x9edc0aea, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pchampwrs)
STD_ROM_FN(pchampwrs)

struct BurnDriver BurnDrvPCE_pchampwrs = {
	"pce_champwrs", NULL, NULL, NULL, "1990",
	"Champion Wrestler\0", NULL, "Taito", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pchampwrsRomInfo, pchampwrsRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Chibi Maruko Chan - Quiz de Piihyara

static struct BurnRomInfo pchibiRomDesc[] = {
	{ "chibi maruko chan - quiz de piihyara (japan).pce",	0x80000, 0x951ed380, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pchibi)
STD_ROM_FN(pchibi)

struct BurnDriver BurnDrvPCE_pchibi = {
	"pce_chibi", NULL, NULL, NULL, "1992",
	"Chibi Maruko Chan - Quiz de Piihyara\0", NULL, "Namco", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pchibiRomInfo, pchibiRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Chikudenya Toubei - Kubikiri Yakata Yori

static struct BurnRomInfo pchikudenRomDesc[] = {
	{ "chikudenya toubei - kubikiri yakata yori (japan).pce",	0x80000, 0xcab21b2e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pchikuden)
STD_ROM_FN(pchikuden)

struct BurnDriver BurnDrvPCE_pchikuden = {
	"pce_chikuden", NULL, NULL, NULL, "1990",
	"Chikudenya Toubei - Kubikiri Yakata Yori\0", NULL, "Naxat", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pchikudenRomInfo, pchikudenRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Chikudenya Toubei - Kubikiri Yakata Yori (Alt)

static struct BurnRomInfo pchikuden1RomDesc[] = {
	{ "chikudenya toubei - kubikiri yakata yori (japan) [a].pce",	0x80000, 0x84098884, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pchikuden1)
STD_ROM_FN(pchikuden1)

struct BurnDriver BurnDrvPCE_pchikuden1 = {
	"pce_chikuden1", "pce_chikuden", NULL, NULL, "1990",
	"Chikudenya Toubei - Kubikiri Yakata Yori (Alt)\0", NULL, "Naxat", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pchikuden1RomInfo, pchikuden1RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Chouzetsu Rinjin - Bravoman

static struct BurnRomInfo pbravomanRomDesc[] = {
	{ "chouzetsu rinjin - bravoman (japan).pce",	0x80000, 0x0df57c90, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pbravoman)
STD_ROM_FN(pbravoman)

struct BurnDriver BurnDrvPCE_pbravoman = {
	"pce_bravoman", NULL, NULL, NULL, "1990",
	"Chouzetsu Rinjin - Bravoman\0", NULL, "Namco", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pbravomanRomInfo, pbravomanRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Circus Lido

static struct BurnRomInfo pcircusldRomDesc[] = {
	{ "circus lido (japan).pce",	0x40000, 0xc3212c24, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pcircusld)
STD_ROM_FN(pcircusld)

struct BurnDriver BurnDrvPCE_pcircusld = {
	"pce_circusld", NULL, NULL, NULL, "1991",
	"Circus Lido\0", NULL, "Yuni Post", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pcircusldRomInfo, pcircusldRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// City Hunter

static struct BurnRomInfo pcityhuntRomDesc[] = {
	{ "city hunter (japan).pce",	0x60000, 0xf91b055f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pcityhunt)
STD_ROM_FN(pcityhunt)

struct BurnDriver BurnDrvPCE_pcityhunt = {
	"pce_cityhunt", NULL, NULL, NULL, "1990",
	"City Hunter\0", NULL, "Sunsoft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pcityhuntRomInfo, pcityhuntRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Columns

static struct BurnRomInfo pcolumnsRomDesc[] = {
	{ "columns (japan).pce",	0x20000, 0x99f7a572, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pcolumns)
STD_ROM_FN(pcolumns)

struct BurnDriver BurnDrvPCE_pcolumns = {
	"pce_columns", NULL, NULL, NULL, "1991",
	"Columns\0", NULL, "Nippon Telenet", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pcolumnsRomInfo, pcolumnsRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Coryoon - Child of Dragon

static struct BurnRomInfo pcoryoonRomDesc[] = {
	{ "coryoon - child of dragon (japan).pce",	0x80000, 0xb4d29e3b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pcoryoon)
STD_ROM_FN(pcoryoon)

struct BurnDriver BurnDrvPCE_pcoryoon = {
	"pce_coryoon", NULL, NULL, NULL, "1991",
	"Coryoon - Child of Dragon\0", NULL, "Naxat", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pcoryoonRomInfo, pcoryoonRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Coryoon - Child of Dragon (Alt)

static struct BurnRomInfo pcoryoon1RomDesc[] = {
	{ "coryoon - child of dragon (japan) [a].pce",	0x80000, 0xd5389889, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pcoryoon1)
STD_ROM_FN(pcoryoon1)

struct BurnDriver BurnDrvPCE_pcoryoon1 = {
	"pce_coryoon1", "pce_coryoon", NULL, NULL, "1991",
	"Coryoon - Child of Dragon (Alt)\0", NULL, "Naxat", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pcoryoon1RomInfo, pcoryoon1RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Cross Wiber - Cyber Combat Police

static struct BurnRomInfo pxwiberRomDesc[] = {
	{ "cross wiber - cyber combat police (japan).pce",	0x80000, 0x2df97bd0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pxwiber)
STD_ROM_FN(pxwiber)

struct BurnDriver BurnDrvPCE_pxwiber = {
	"pce_xwiber", NULL, NULL, NULL, "1990",
	"Cross Wiber - Cyber Combat Police\0", NULL, "Face", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pxwiberRomInfo, pxwiberRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Cyber Core

static struct BurnRomInfo pcybrcoreRomDesc[] = {
	{ "cyber core (japan).pce",	0x60000, 0xa98d276a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pcybrcore)
STD_ROM_FN(pcybrcore)

struct BurnDriver BurnDrvPCE_pcybrcore = {
	"pce_cybrcore", NULL, NULL, NULL, "1990",
	"Cyber Core\0", NULL, "IGS", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pcybrcoreRomInfo, pcybrcoreRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Cyber Dodge

static struct BurnRomInfo pcyberdodRomDesc[] = {
	{ "cyber dodge (japan).pce",	0x80000, 0xb5326b16, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pcyberdod)
STD_ROM_FN(pcyberdod)

struct BurnDriver BurnDrvPCE_pcyberdod = {
	"pce_cyberdod", NULL, NULL, NULL, "1992",
	"Cyber Dodge\0", NULL, "Tonkin House", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pcyberdodRomInfo, pcyberdodRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Cyber Knight

static struct BurnRomInfo pcybrkngtRomDesc[] = {
	{ "cyber knight (japan).pce",	0x80000, 0xa594fac0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pcybrkngt)
STD_ROM_FN(pcybrkngt)

struct BurnDriver BurnDrvPCE_pcybrkngt = {
	"pce_cybrkngt", NULL, NULL, NULL, "1990",
	"Cyber Knight\0", NULL, "Tonkin House", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pcybrkngtRomInfo, pcybrkngtRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Dai Senpu

static struct BurnRomInfo pdaisenpuRomDesc[] = {
	{ "dai senpu (japan).pce",	0x80000, 0x9107bcc8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pdaisenpu)
STD_ROM_FN(pdaisenpu)

struct BurnDriver BurnDrvPCE_pdaisenpu = {
	"pce_daisenpu", NULL, NULL, NULL, "1990",
	"Dai Senpu\0", NULL, "NEC Avenue", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pdaisenpuRomInfo, pdaisenpuRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Daichi Kun Crisis - Do Natural

static struct BurnRomInfo pdonaturlRomDesc[] = {
	{ "daichi kun crisis - do natural (japan).pce",	0x60000, 0x61a2935f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pdonaturl)
STD_ROM_FN(pdonaturl)

struct BurnDriver BurnDrvPCE_pdonaturl = {
	"pce_donaturl", NULL, NULL, NULL, "1989",
	"Daichi Kun Crisis - Do Natural\0", NULL, "Salio", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pdonaturlRomInfo, pdonaturlRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Darius Alpha

static struct BurnRomInfo pdariusaRomDesc[] = {
	{ "darius alpha (japan).pce",	0x60000, 0xb0ba689f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pdariusa)
STD_ROM_FN(pdariusa)

struct BurnDriver BurnDrvPCE_pdariusa = {
	"pce_dariusa", NULL, NULL, NULL, "1990",
	"Darius Alpha\0", NULL, "NEC Avenue", "SuperGrafx Enhanced",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pdariusaRomInfo, pdariusaRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	SGXInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Darius Plus

static struct BurnRomInfo pdariuspRomDesc[] = {
	{ "darius plus (japan).pce",	0xc0000, 0xbebfe042, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pdariusp)
STD_ROM_FN(pdariusp)

struct BurnDriver BurnDrvPCE_pdariusp = {
	"pce_dariusp", NULL, NULL, NULL, "1990",
	"Darius Plus\0", NULL, "NEC Avenue", "SuperGrafx Enhanced",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pdariuspRomInfo, pdariuspRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	SGXInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Dead Moon - Tsuki Sekai no Akumu

static struct BurnRomInfo pdeadmoonRomDesc[] = {
	{ "dead moon (japan).pce",	0x80000, 0x56739bc7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pdeadmoon)
STD_ROM_FN(pdeadmoon)

struct BurnDriver BurnDrvPCE_pdeadmoon = {
	"pce_deadmoon", NULL, NULL, NULL, "1990",
	"Dead Moon - Tsuki Sekai no Akumu\0", NULL, "B.S.S.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pdeadmoonRomInfo, pdeadmoonRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Deep Blue - Kaitei Shinwa

static struct BurnRomInfo pdeepblueRomDesc[] = {
	{ "deep blue - kaitei shinwa (japan).pce",	0x40000, 0x053a0f83, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pdeepblue)
STD_ROM_FN(pdeepblue)

struct BurnDriver BurnDrvPCE_pdeepblue = {
	"pce_deepblue", NULL, NULL, NULL, "1989",
	"Deep Blue - Kaitei Shinwa\0", NULL, "Pack-In-Video", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pdeepblueRomInfo, pdeepblueRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Detana!! Twinbee

static struct BurnRomInfo ptwinbeeRomDesc[] = {
	{ "detana!! twinbee (japan).pce",	0x80000, 0x5cf59d80, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ptwinbee)
STD_ROM_FN(ptwinbee)

struct BurnDriver BurnDrvPCE_ptwinbee = {
	"pce_twinbee", NULL, NULL, NULL, "1992",
	"Detana!! Twinbee\0", NULL, "Konami", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ptwinbeeRomInfo, ptwinbeeRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Devil Crash - Naxat Pinball

static struct BurnRomInfo pdevlcrshRomDesc[] = {
	{ "devil crash - naxat pinball (japan).pce",	0x60000, 0x4ec81a80, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pdevlcrsh)
STD_ROM_FN(pdevlcrsh)

struct BurnDriver BurnDrvPCE_pdevlcrsh = {
	"pce_devlcrsh", NULL, NULL, NULL, "1990",
	"Devil Crash - Naxat Pinball\0", NULL, "Naxat", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pdevlcrshRomInfo, pdevlcrshRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Die Hard

static struct BurnRomInfo pdiehardRomDesc[] = {
	{ "die hard (japan).pce",	0x80000, 0x1b5b1cb1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pdiehard)
STD_ROM_FN(pdiehard)

struct BurnDriver BurnDrvPCE_pdiehard = {
	"pce_diehard", NULL, NULL, NULL, "1990",
	"Die Hard\0", NULL, "Pack-In-Video", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pdiehardRomInfo, pdiehardRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Digital Champ

static struct BurnRomInfo pdigichmpRomDesc[] = {
	{ "digital champ (japan).pce",	0x40000, 0x17ba3032, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pdigichmp)
STD_ROM_FN(pdigichmp)

struct BurnDriver BurnDrvPCE_pdigichmp = {
	"pce_digichmp", NULL, NULL, NULL, "1989",
	"Digital Champ\0", NULL, "Naxat", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pdigichmpRomInfo, pdigichmpRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Don Doko Don!

static struct BurnRomInfo pdondokoRomDesc[] = {
	{ "don doko don! (japan).pce",	0x60000, 0xf42aa73e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pdondoko)
STD_ROM_FN(pdondoko)

struct BurnDriver BurnDrvPCE_pdondoko = {
	"pce_dondoko", NULL, NULL, NULL, "1990",
	"Don Doko Don!\0", NULL, "Taito", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pdondokoRomInfo, pdondokoRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Doraemon - Meikyuu Dai Sakusen

static struct BurnRomInfo pdoramsRomDesc[] = {
	{ "doraemon - meikyuu dai sakusen (japan).pce",	0x40000, 0xdc760a07, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pdorams)
STD_ROM_FN(pdorams)

struct BurnDriver BurnDrvPCE_pdorams = {
	"pce_dorams", NULL, NULL, NULL, "1989",
	"Doraemon - Meikyuu Dai Sakusen\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pdoramsRomInfo, pdoramsRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Doraemon - Nobita no Dorabian Night

static struct BurnRomInfo pdorandnRomDesc[] = {
	{ "doraemon - nobita no dorabian night (japan).pce",	0x80000, 0x013a747f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pdorandn)
STD_ROM_FN(pdorandn)

struct BurnDriver BurnDrvPCE_pdorandn = {
	"pce_dorandn", NULL, NULL, NULL, "1991",
	"Doraemon - Nobita no Dorabian Night\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pdorandnRomInfo, pdorandnRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Double Dungeons - W

static struct BurnRomInfo pddungwRomDesc[] = {
	{ "double dungeons - w (japan).pce",	0x40000, 0x86087b39, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pddungw)
STD_ROM_FN(pddungw)

struct BurnDriver BurnDrvPCE_pddungw = {
	"pce_ddungw", NULL, NULL, NULL, "1989",
	"Double Dungeons - W\0", NULL, "NCS", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pddungwRomInfo, pddungwRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Download

static struct BurnRomInfo pdownloadRomDesc[] = {
	{ "download (japan).pce",	0x80000, 0x85101c20, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pdownload)
STD_ROM_FN(pdownload)

struct BurnDriver BurnDrvPCE_pdownload = {
	"pce_download", NULL, NULL, NULL, "1990",
	"Download\0", NULL, "NEC Avenue", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pdownloadRomInfo, pdownloadRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Download (Alt)

static struct BurnRomInfo pdownload1RomDesc[] = {
	{ "download (japan) [a].pce",	0x80000, 0x4e0de488, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pdownload1)
STD_ROM_FN(pdownload1)

struct BurnDriver BurnDrvPCE_pdownload1 = {
	"pce_download1", "pce_download", NULL, NULL, "1990",
	"Download (Alt)\0", NULL, "NEC Avenue", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pdownload1RomInfo, pdownload1RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Dragon Egg!

static struct BurnRomInfo pdragneggRomDesc[] = {
	{ "dragon egg! (japan).pce",	0x80000, 0x442405d5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pdragnegg)
STD_ROM_FN(pdragnegg)

struct BurnDriver BurnDrvPCE_pdragnegg = {
	"pce_dragnegg", NULL, NULL, NULL, "1991",
	"Dragon Egg!\0", NULL, "Masiya", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pdragneggRomInfo, pdragneggRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Dragon Saber - After Story of Dragon Spirit

static struct BurnRomInfo pdsaberRomDesc[] = {
	{ "dragon saber - after story of dragon spirit (japan).pce",	0x80000, 0x3219849c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pdsaber)
STD_ROM_FN(pdsaber)

struct BurnDriver BurnDrvPCE_pdsaber = {
	"pce_dsaber", NULL, NULL, NULL, "1991",
	"Dragon Saber - After Story of Dragon Spirit\0", NULL, "Namco", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pdsaberRomInfo, pdsaberRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Dragon Saber - After Story of Dragon Spirit (Alt)

static struct BurnRomInfo pdsaber1RomDesc[] = {
	{ "dragon saber - after story of dragon spirit (japan) [a].pce",	0x80000, 0xc89ce75a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pdsaber1)
STD_ROM_FN(pdsaber1)

struct BurnDriver BurnDrvPCE_pdsaber1 = {
	"pce_dsaber1", "pce_dsaber", NULL, NULL, "1991",
	"Dragon Saber - After Story of Dragon Spirit (Alt)\0", NULL, "Namco", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pdsaber1RomInfo, pdsaber1RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Dragon Spirit

static struct BurnRomInfo pdspiritRomDesc[] = {
	{ "dragon spirit (japan).pce",	0x40000, 0x01a76935, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pdspirit)
STD_ROM_FN(pdspirit)

struct BurnDriver BurnDrvPCE_pdspirit = {
	"pce_dspirit", NULL, NULL, NULL, "1988",
	"Dragon Spirit\0", NULL, "Namco", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pdspiritRomInfo, pdspiritRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Drop Rock Hora Hora

static struct BurnRomInfo pdroprockRomDesc[] = {
	{ "drop rock hora hora (japan).pce",	0x40000, 0x67ec5ec4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pdroprock)
STD_ROM_FN(pdroprock)

struct BurnDriver BurnDrvPCE_pdroprock = {
	"pce_droprock", NULL, NULL, NULL, "1990",
	"Drop Rock Hora Hora\0", NULL, "Data East", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pdroprockRomInfo, pdroprockRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Drop Rock Hora Hora (Alt)

static struct BurnRomInfo pdroprock1RomDesc[] = {
	{ "drop rock hora hora (japan) [a].pce",	0x40000, 0x8e81fcac, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pdroprock1)
STD_ROM_FN(pdroprock1)

struct BurnDriver BurnDrvPCE_pdroprock1 = {
	"pce_droprock1", "pce_droprock", NULL, NULL, "1990",
	"Drop Rock Hora Hora (Alt)\0", NULL, "Data East", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pdroprock1RomInfo, pdroprock1RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Dungeon Explorer

static struct BurnRomInfo pdungexplRomDesc[] = {
	{ "dungeon explorer (japan).pce",	0x60000, 0x1b1a80a2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pdungexpl)
STD_ROM_FN(pdungexpl)

struct BurnDriver BurnDrvPCE_pdungexpl = {
	"pce_dungexpl", NULL, NULL, NULL, "1989",
	"Dungeon Explorer\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pdungexplRomInfo, pdungexplRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Energy

static struct BurnRomInfo penergyRomDesc[] = {
	{ "energy (japan).pce",	0x40000, 0xca68ff21, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(penergy)
STD_ROM_FN(penergy)

struct BurnDriver BurnDrvPCE_penergy = {
	"pce_energy", NULL, NULL, NULL, "1989",
	"Energy\0", NULL, "NCS", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, penergyRomInfo, penergyRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// F-1 Dream

static struct BurnRomInfo pf1dreamRomDesc[] = {
	{ "f-1 dream (japan).pce",	0x40000, 0xd50ff730, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pf1dream)
STD_ROM_FN(pf1dream)

struct BurnDriver BurnDrvPCE_pf1dream = {
	"pce_f1dream", NULL, NULL, NULL, "1989",
	"F-1 Dream\0", NULL, "NEC", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pf1dreamRomInfo, pf1dreamRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// F-1 Pilot - You're King of Kings

static struct BurnRomInfo pf1pilotRomDesc[] = {
	{ "f-1 pilot - you're king of kings (japan).pce",	0x60000, 0x09048174, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pf1pilot)
STD_ROM_FN(pf1pilot)

struct BurnDriver BurnDrvPCE_pf1pilot = {
	"pce_f1pilot", NULL, NULL, NULL, "1989",
	"F-1 Pilot - You're King of Kings\0", NULL, "Pack-In-Video", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pf1pilotRomInfo, pf1pilotRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// F1 Circus '91 - World Championship

static struct BurnRomInfo pf1circ91RomDesc[] = {
	{ "f1 circus '91 - world championship (japan).pce",	0x80000, 0xd7cfd70f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pf1circ91)
STD_ROM_FN(pf1circ91)

struct BurnDriver BurnDrvPCE_pf1circ91 = {
	"pce_f1circ91", NULL, NULL, NULL, "1991",
	"F1 Circus '91 - World Championship\0", NULL, "Nihon Bussan", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pf1circ91RomInfo, pf1circ91RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// F1 Circus '92 - The Speed of Sound

static struct BurnRomInfo pf1circ92RomDesc[] = {
	{ "f1 circus '92 - the speed of sound (japan).pce",	0xc0000, 0xb268f2a2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pf1circ92)
STD_ROM_FN(pf1circ92)

struct BurnDriver BurnDrvPCE_pf1circ92 = {
	"pce_f1circ92", NULL, NULL, NULL, "1992",
	"F1 Circus '92 - The Speed of Sound\0", NULL, "Nihon Bussan", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pf1circ92RomInfo, pf1circ92RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// F1 Circus

static struct BurnRomInfo pf1circusRomDesc[] = {
	{ "f1 circus (japan).pce",	0x80000, 0xe14dee08, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pf1circus)
STD_ROM_FN(pf1circus)

struct BurnDriver BurnDrvPCE_pf1circus = {
	"pce_f1circus", NULL, NULL, NULL, "1990",
	"F1 Circus\0", NULL, "Nihon Bussan", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pf1circusRomInfo, pf1circusRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// F1 Circus (Alt)

static struct BurnRomInfo pf1circus1RomDesc[] = {
	{ "f1 circus (japan) [a].pce",	0x80000, 0x79705779, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pf1circus1)
STD_ROM_FN(pf1circus1)

struct BurnDriver BurnDrvPCE_pf1circus1 = {
	"pce_f1circus1", "pce_f1circus", NULL, NULL, "1990",
	"F1 Circus (Alt)\0", NULL, "Nihon Bussan", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pf1circus1RomInfo, pf1circus1RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// F1 Triple Battle

static struct BurnRomInfo pf1tbRomDesc[] = {
	{ "f1 triple battle (japan).pce",	0x60000, 0x13bf0409, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pf1tb)
STD_ROM_FN(pf1tb)

struct BurnDriver BurnDrvPCE_pf1tb = {
	"pce_f1tb", NULL, NULL, NULL, "1989",
	"F1 Triple Battle\0", NULL, "Human", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pf1tbRomInfo, pf1tbRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Fantasy Zone

static struct BurnRomInfo pfantzoneRomDesc[] = {
	{ "fantasy zone (japan).pce",	0x40000, 0x72cb0f9d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pfantzone)
STD_ROM_FN(pfantzone)

struct BurnDriver BurnDrvPCE_pfantzone = {
	"pce_fantzone", NULL, NULL, NULL, "1988",
	"Fantasy Zone\0", NULL, "NEC", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pfantzoneRomInfo, pfantzoneRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Fighting Run

static struct BurnRomInfo pfightrunRomDesc[] = {
	{ "fighting run (japan).pce",	0x80000, 0x1828d2e5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pfightrun)
STD_ROM_FN(pfightrun)

struct BurnDriver BurnDrvPCE_pfightrun = {
	"pce_fightrun", NULL, NULL, NULL, "1991",
	"Fighting Run\0", NULL, "Nihon Bussan", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pfightrunRomInfo, pfightrunRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Final Blaster

static struct BurnRomInfo pfinlblstRomDesc[] = {
	{ "final blaster (japan).pce",	0x60000, 0xc90971ba, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pfinlblst)
STD_ROM_FN(pfinlblst)

struct BurnDriver BurnDrvPCE_pfinlblst = {
	"pce_finlblst", NULL, NULL, NULL, "1990",
	"Final Blaster\0", NULL, "Namco", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pfinlblstRomInfo, pfinlblstRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Final Lap Twin

static struct BurnRomInfo pfinallapRomDesc[] = {
	{ "final lap twin (japan).pce",	0x60000, 0xc8c084e3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pfinallap)
STD_ROM_FN(pfinallap)

struct BurnDriver BurnDrvPCE_pfinallap = {
	"pce_finallap", NULL, NULL, NULL, "1989",
	"Final Lap Twin\0", NULL, "Namco", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pfinallapRomInfo, pfinallapRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Final Match Tennis

static struct BurnRomInfo pfinalmtRomDesc[] = {
	{ "final match tennis (japan).pce",	0x40000, 0x560d2305, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pfinalmt)
STD_ROM_FN(pfinalmt)

struct BurnDriver BurnDrvPCE_pfinalmt = {
	"pce_finalmt", NULL, NULL, NULL, "1991",
	"Final Match Tennis\0", NULL, "Human", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pfinalmtRomInfo, pfinalmtRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Final Soldier

static struct BurnRomInfo pfinalsolRomDesc[] = {
	{ "final soldier (japan).pce",	0x80000, 0xaf2dd2af, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pfinalsol)
STD_ROM_FN(pfinalsol)

struct BurnDriver BurnDrvPCE_pfinalsol = {
	"pce_finalsol", NULL, NULL, NULL, "1991",
	"Final Soldier\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pfinalsolRomInfo, pfinalsolRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Final Soldier (Special Version)

static struct BurnRomInfo pfinalsolsRomDesc[] = {
	{ "final soldier (special version) (japan).pce",	0x80000, 0x02a578c5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pfinalsols)
STD_ROM_FN(pfinalsols)

struct BurnDriver BurnDrvPCE_pfinalsols = {
	"pce_finalsols", "pce_finalsol", NULL, NULL, "19??",
	"Final Soldier (Special Version)\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pfinalsolsRomInfo, pfinalsolsRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Fire Pro Wrestling - Combination Tag

static struct BurnRomInfo pfpwrestRomDesc[] = {
	{ "fire pro wrestling - combination tag (japan).pce",	0x60000, 0x90ed6575, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pfpwrest)
STD_ROM_FN(pfpwrest)

struct BurnDriver BurnDrvPCE_pfpwrest = {
	"pce_fpwrest", NULL, NULL, NULL, "1989",
	"Fire Pro Wrestling - Combination Tag\0", NULL, "Human", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pfpwrestRomInfo, pfpwrestRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Fire Pro Wrestling 2 - 2nd Bout

static struct BurnRomInfo pfpwrest2RomDesc[] = {
	{ "fire pro wrestling 2 - 2nd bout (japan).pce",	0x80000, 0xe88987bb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pfpwrest2)
STD_ROM_FN(pfpwrest2)

struct BurnDriver BurnDrvPCE_pfpwrest2 = {
	"pce_fpwrest2", NULL, NULL, NULL, "1991",
	"Fire Pro Wrestling 2 - 2nd Bout\0", NULL, "Human", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pfpwrest2RomInfo, pfpwrest2RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Fire Pro Wrestling 3 - Legend Bout

static struct BurnRomInfo pfpwrest3RomDesc[] = {
	{ "fire pro wrestling 3 - legend bout (japan).pce",	0x100000, 0x534e8808, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pfpwrest3)
STD_ROM_FN(pfpwrest3)

struct BurnDriver BurnDrvPCE_pfpwrest3 = {
	"pce_fpwrest3", NULL, NULL, NULL, "1992",
	"Fire Pro Wrestling 3 - Legend Bout\0", NULL, "Human", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pfpwrest3RomInfo, pfpwrest3RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Formation Soccer - Human Cup '90

static struct BurnRomInfo pfsoccr90RomDesc[] = {
	{ "formation soccer - human cup '90 (japan).pce",	0x40000, 0x85a1e7b6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pfsoccr90)
STD_ROM_FN(pfsoccr90)

struct BurnDriver BurnDrvPCE_pfsoccr90 = {
	"pce_fsoccr90", NULL, NULL, NULL, "1990",
	"Formation Soccer - Human Cup '90\0", NULL, "Human", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pfsoccr90RomInfo, pfsoccr90RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Formation Soccer - On J. League

static struct BurnRomInfo pfsoccerRomDesc[] = {
	{ "formation soccer - on j. league (japan).pce",	0x80000, 0x7146027c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pfsoccer)
STD_ROM_FN(pfsoccer)

struct BurnDriver BurnDrvPCE_pfsoccer = {
	"pce_fsoccer", NULL, NULL, NULL, "1994",
	"Formation Soccer - On J. League\0", NULL, "Human", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pfsoccerRomInfo, pfsoccerRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Fushigi no Yume no Alice

static struct BurnRomInfo paliceRomDesc[] = {
	{ "fushigi no yume no alice (japan).pce",	0x60000, 0x12c4e6fd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(palice)
STD_ROM_FN(palice)

struct BurnDriver BurnDrvPCE_palice = {
	"pce_alice", NULL, NULL, NULL, "1990",
	"Fushigi no Yume no Alice\0", NULL, "Face", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, paliceRomInfo, paliceRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Gai Flame

static struct BurnRomInfo pgaiflameRomDesc[] = {
	{ "gai flame (japan).pce",	0x60000, 0x95f90dec, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pgaiflame)
STD_ROM_FN(pgaiflame)

struct BurnDriver BurnDrvPCE_pgaiflame = {
	"pce_gaiflame", NULL, NULL, NULL, "1990",
	"Gai Flame\0", NULL, "NCS", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pgaiflameRomInfo, pgaiflameRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Gaia no Monshou

static struct BurnRomInfo pgaiamonRomDesc[] = {
	{ "gaia no monshou (japan).pce",	0x40000, 0x6fd6827c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pgaiamon)
STD_ROM_FN(pgaiamon)

struct BurnDriver BurnDrvPCE_pgaiamon = {
	"pce_gaiamon", NULL, NULL, NULL, "1988",
	"Gaia no Monshou\0", NULL, "NCS", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pgaiamonRomInfo, pgaiamonRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Galaga '88

static struct BurnRomInfo pgalaga88RomDesc[] = {
	{ "galaga '88 (japan).pce",	0x40000, 0x1a8393c6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pgalaga88)
STD_ROM_FN(pgalaga88)

struct BurnDriver BurnDrvPCE_pgalaga88 = {
	"pce_galaga88", NULL, NULL, NULL, "1988",
	"Galaga '88\0", NULL, "Namco", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pgalaga88RomInfo, pgalaga88RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Ganbare! Golf Boys

static struct BurnRomInfo pganbgolfRomDesc[] = {
	{ "ganbare! golf boys (japan).pce",	0x40000, 0x27a4d11a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pganbgolf)
STD_ROM_FN(pganbgolf)

struct BurnDriver BurnDrvPCE_pganbgolf = {
	"pce_ganbgolf", NULL, NULL, NULL, "1989",
	"Ganbare! Golf Boys\0", NULL, "NCS", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pganbgolfRomInfo, pganbgolfRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Gekisha Boy

static struct BurnRomInfo pgekisboyRomDesc[] = {
	{ "gekisha boy (japan).pce",	0x80000, 0xe8702d51, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pgekisboy)
STD_ROM_FN(pgekisboy)

struct BurnDriver BurnDrvPCE_pgekisboy = {
	"pce_gekisboy", NULL, NULL, NULL, "1992",
	"Gekisha Boy\0", NULL, "Irem", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pgekisboyRomInfo, pgekisboyRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Genji Tsuushin Agedama

static struct BurnRomInfo pgenjitsuRomDesc[] = {
	{ "genji tsuushin agedama (japan).pce",	0x80000, 0xad450dfc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pgenjitsu)
STD_ROM_FN(pgenjitsu)

struct BurnDriver BurnDrvPCE_pgenjitsu = {
	"pce_genjitsu", NULL, NULL, NULL, "1991",
	"Genji Tsuushin Agedama\0", NULL, "NEC Home Electronics", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pgenjitsuRomInfo, pgenjitsuRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Genpei Toumaden

static struct BurnRomInfo pgenpeitoRomDesc[] = {
	{ "genpei toumaden (japan).pce",	0x80000, 0xb926c682, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pgenpeito)
STD_ROM_FN(pgenpeito)

struct BurnDriver BurnDrvPCE_pgenpeito = {
	"pce_genpeito", NULL, NULL, NULL, "1990",
	"Genpei Toumaden\0", NULL, "Namco", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pgenpeitoRomInfo, pgenpeitoRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Genpei Toumaden ni no Maki

static struct BurnRomInfo pgentomakRomDesc[] = {
	{ "genpei toumaden ni no maki (japan).pce",	0x80000, 0x8793758c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pgentomak)
STD_ROM_FN(pgentomak)

struct BurnDriver BurnDrvPCE_pgentomak = {
	"pce_gentomak", NULL, NULL, NULL, "1992",
	"Genpei Toumaden ni no Maki\0", NULL, "Namco", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pgentomakRomInfo, pgentomakRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Gokuraku! Chuuka Taisen

static struct BurnRomInfo pchukataiRomDesc[] = {
	{ "gokuraku! chuuka taisen (japan).pce",	0x60000, 0xe749a22c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pchukatai)
STD_ROM_FN(pchukatai)

struct BurnDriver BurnDrvPCE_pchukatai = {
	"pce_chukatai", NULL, NULL, NULL, "1992",
	"Gokuraku! Chuuka Taisen\0", NULL, "Taito", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pchukataiRomInfo, pchukataiRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Gomola Speed

static struct BurnRomInfo pgomolaRomDesc[] = {
	{ "gomola speed (japan).pce",	0x60000, 0x9a353afd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pgomola)
STD_ROM_FN(pgomola)

struct BurnDriver BurnDrvPCE_pgomola = {
	"pce_gomola", NULL, NULL, NULL, "1990",
	"Gomola Speed\0", NULL, "UPL", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pgomolaRomInfo, pgomolaRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Gradius

static struct BurnRomInfo pgradiusRomDesc[] = {
	{ "gradius (japan).pce",	0x40000, 0x0517da65, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pgradius)
STD_ROM_FN(pgradius)

struct BurnDriver BurnDrvPCE_pgradius = {
	"pce_gradius", NULL, NULL, NULL, "1991",
	"Gradius\0", NULL, "Konami", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pgradiusRomInfo, pgradiusRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Gunhed

static struct BurnRomInfo pgunhedRomDesc[] = {
	{ "gunhed (japan).pce",	0x60000, 0xa17d4d7e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pgunhed)
STD_ROM_FN(pgunhed)

struct BurnDriver BurnDrvPCE_pgunhed = {
	"pce_gunhed", NULL, NULL, NULL, "1989",
	"Gunhed\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pgunhedRomInfo, pgunhedRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Gunhed - Hudson Gunhed Taikai

static struct BurnRomInfo pgunhedhtRomDesc[] = {
	{ "gunhed - hudson gunhed taikai (japan).pce",	0x40000, 0x57f183ae, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pgunhedht)
STD_ROM_FN(pgunhedht)

struct BurnDriver BurnDrvPCE_pgunhedht = {
	"pce_gunhedht", NULL, NULL, NULL, "1989",
	"Gunhed - Hudson Gunhed Taikai\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pgunhedhtRomInfo, pgunhedhtRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Hana Taaka Daka!

static struct BurnRomInfo phanatakaRomDesc[] = {
	{ "hana taaka daka! (japan).pce",	0x80000, 0xba4d0dd4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(phanataka)
STD_ROM_FN(phanataka)

struct BurnDriver BurnDrvPCE_phanataka = {
	"pce_hanataka", NULL, NULL, NULL, "1991",
	"Hana Taaka Daka!\0", NULL, "Taito", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, phanatakaRomInfo, phanatakaRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Hanii in the Sky

static struct BurnRomInfo phaniiskyRomDesc[] = {
	{ "hanii in the sky (japan).pce",	0x40000, 0xbf3e2cc7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(phaniisky)
STD_ROM_FN(phaniisky)

struct BurnDriver BurnDrvPCE_phaniisky = {
	"pce_haniisky", NULL, NULL, NULL, "1989",
	"Hanii in the Sky\0", NULL, "Face", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, phaniiskyRomInfo, phaniiskyRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Hanii on the Road

static struct BurnRomInfo phaniirodRomDesc[] = {
	{ "hanii on the road (japan).pce",	0x60000, 0x9897fa86, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(phaniirod)
STD_ROM_FN(phaniirod)

struct BurnDriver BurnDrvPCE_phaniirod = {
	"pce_haniirod", NULL, NULL, NULL, "1990",
	"Hanii on the Road\0", NULL, "Face", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, phaniirodRomInfo, phaniirodRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Hatris

static struct BurnRomInfo phatrisRomDesc[] = {
	{ "hatris (japan).pce",	0x20000, 0x44e7df53, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(phatris)
STD_ROM_FN(phatris)

struct BurnDriver BurnDrvPCE_phatris = {
	"pce_hatris", NULL, NULL, NULL, "1991",
	"Hatris\0", NULL, "Tengen", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, phatrisRomInfo, phatrisRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Heavy Unit

static struct BurnRomInfo phvyunitRomDesc[] = {
	{ "heavy unit (japan).pce",	0x60000, 0xeb923de5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(phvyunit)
STD_ROM_FN(phvyunit)

struct BurnDriver BurnDrvPCE_phvyunit = {
	"pce_hvyunit", NULL, NULL, NULL, "1989",
	"Heavy Unit\0", NULL, "Taito", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, phvyunitRomInfo, phvyunitRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Hisou Kihei - Xserd

static struct BurnRomInfo pxserdRomDesc[] = {
	{ "hisou kihei - xserd (japan).pce",	0x60000, 0x1cab1ee6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pxserd)
STD_ROM_FN(pxserd)

struct BurnDriverD BurnDrvPCE_pxserd = {
	"pce_xserd", NULL, NULL, NULL, "1990",
	"Hisou Kihei - Xserd\0", NULL, "Masiya", "PC Engine",
	NULL, NULL, NULL, NULL,
	0, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pxserdRomInfo, pxserdRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Hit the Ice - VHL - The Official Video Hockey League

static struct BurnRomInfo phiticeRomDesc[] = {
	{ "hit the ice - vhl the official video hockey league (japan).pce",	0x60000, 0x7acb60c8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(phitice)
STD_ROM_FN(phitice)

struct BurnDriver BurnDrvPCE_phitice = {
	"pce_hitice", NULL, NULL, NULL, "1991",
	"Hit the Ice - VHL - The Official Video Hockey League\0", NULL, "Taito", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, phiticeRomInfo, phiticeRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Honoo no Toukyuuji Dodge Danpei

static struct BurnRomInfo pdodgedanRomDesc[] = {
	{ "honoo no toukyuuji dodge danpei (japan).pce",	0x80000, 0xb01ee703, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pdodgedan)
STD_ROM_FN(pdodgedan)

struct BurnDriver BurnDrvPCE_pdodgedan = {
	"pce_dodgedan", NULL, NULL, NULL, "1992",
	"Honoo no Toukyuuji Dodge Danpei\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pdodgedanRomInfo, pdodgedanRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Idol Hanafuda Fan Club

static struct BurnRomInfo pidolhanaRomDesc[] = {
	{ "idol hanafuda fan club (japan).pce",	0x80000, 0x9ec6fc6c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pidolhana)
STD_ROM_FN(pidolhana)

struct BurnDriver BurnDrvPCE_pidolhana = {
	"pce_idolhana", NULL, NULL, NULL, "19??",
	"Idol Hanafuda Fan Club\0", NULL, "Unknown", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pidolhanaRomInfo, pidolhanaRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Image Fight

static struct BurnRomInfo pimagefgtRomDesc[] = {
	{ "image fight (japan).pce",	0x80000, 0xa80c565f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pimagefgt)
STD_ROM_FN(pimagefgt)

struct BurnDriver BurnDrvPCE_pimagefgt = {
	"pce_imagefgt", NULL, NULL, NULL, "1990",
	"Image Fight\0", NULL, "Irem", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pimagefgtRomInfo, pimagefgtRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// J. League Greatest Eleven

static struct BurnRomInfo pjleag11RomDesc[] = {
	{ "j. league greatest eleven (japan).pce",	0x80000, 0x0ad97b04, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pjleag11)
STD_ROM_FN(pjleag11)

struct BurnDriver BurnDrvPCE_pjleag11 = {
	"pce_jleag11", NULL, NULL, NULL, "1993",
	"J. League Greatest Eleven\0", NULL, "Nihon Bussan", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pjleag11RomInfo, pjleag11RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Jack Nicklaus' Greatest 18 Holes of Major Championship Golf

static struct BurnRomInfo pjn18holeRomDesc[] = {
	{ "jack nicklaus' greatest 18 holes of major championship golf (japan).pce",	0x40000, 0xea751e82, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pjn18hole)
STD_ROM_FN(pjn18hole)

struct BurnDriver BurnDrvPCE_pjn18hole = {
	"pce_jn18hole", NULL, NULL, NULL, "1989",
	"Jack Nicklaus' Greatest 18 Holes of Major Championship Golf\0", NULL, "Victor Interactive Software", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pjn18holeRomInfo, pjn18holeRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Jackie Chan

static struct BurnRomInfo pjchanRomDesc[] = {
	{ "jackie chan (japan).pce",	0x80000, 0xc6fa6373, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pjchan)
STD_ROM_FN(pjchan)

struct BurnDriver BurnDrvPCE_pjchan = {
	"pce_jchan", NULL, NULL, NULL, "1991",
	"Jackie Chan\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pjchanRomInfo, pjchanRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Jigoku Meguri

static struct BurnRomInfo pjigomeguRomDesc[] = {
	{ "jigoku meguri (japan).pce",	0x60000, 0xcc7d3eeb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pjigomegu)
STD_ROM_FN(pjigomegu)

struct BurnDriver BurnDrvPCE_pjigomegu = {
	"pce_jigomegu", NULL, NULL, NULL, "1990",
	"Jigoku Meguri\0", NULL, "Taito", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pjigomeguRomInfo, pjigomeguRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Jinmu Denshou

static struct BurnRomInfo pjinmuRomDesc[] = {
	{ "jinmu denshou (japan).pce",	0x80000, 0xc150637a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pjinmu)
STD_ROM_FN(pjinmu)

struct BurnDriver BurnDrvPCE_pjinmu = {
	"pce_jinmu", NULL, NULL, NULL, "1989",
	"Jinmu Denshou\0", NULL, "Big Club", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pjinmuRomInfo, pjinmuRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Jinmu Denshou (Alt)

static struct BurnRomInfo pjinmu1RomDesc[] = {
	{ "jinmu denshou (japan) [a].pce",	0x80000, 0x84240ef9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pjinmu1)
STD_ROM_FN(pjinmu1)

struct BurnDriver BurnDrvPCE_pjinmu1 = {
	"pce_jinmu1", "pce_jinmu", NULL, NULL, "1989",
	"Jinmu Denshou (Alt)\0", NULL, "Big Club", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pjinmu1RomInfo, pjinmu1RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Juuouki

static struct BurnRomInfo pjuoukiRomDesc[] = {
	{ "juuouki (japan).pce",	0x80000, 0xc8c7d63e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pjuouki)
STD_ROM_FN(pjuouki)

struct BurnDriver BurnDrvPCE_pjuouki = {
	"pce_juouki", NULL, NULL, NULL, "1989",
	"Juuouki\0", NULL, "NEC", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pjuoukiRomInfo, pjuoukiRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Juuouki (Alt)

static struct BurnRomInfo pjuouki1RomDesc[] = {
	{ "juuouki (japan) [a].pce",	0x80000, 0x6a628982, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pjuouki1)
STD_ROM_FN(pjuouki1)

struct BurnDriver BurnDrvPCE_pjuouki1 = {
	"pce_juouki1", "pce_juouki", NULL, NULL, "1989",
	"Juuouki (Alt)\0", NULL, "NEC", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pjuouki1RomInfo, pjuouki1RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Kaizou Choujin Shubibinman

static struct BurnRomInfo pshubibiRomDesc[] = {
	{ "kaizou choujin shubibinman (japan).pce",	0x40000, 0xa9084d6e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pshubibi)
STD_ROM_FN(pshubibi)

struct BurnDriver BurnDrvPCE_pshubibi = {
	"pce_shubibi", NULL, NULL, NULL, "1989",
	"Kaizou Choujin Shubibinman\0", NULL, "NCS", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pshubibiRomInfo, pshubibiRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Kaizou Choujin Shubibinman 2 - Aratanaru Teki

static struct BurnRomInfo pshubibi2RomDesc[] = {
	{ "kaizou choujin shubibinman 2 - aratanaru teki (japan).pce",	0x80000, 0x109ba474, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pshubibi2)
STD_ROM_FN(pshubibi2)

struct BurnDriver BurnDrvPCE_pshubibi2 = {
	"pce_shubibi2", NULL, NULL, NULL, "1991",
	"Kaizou Choujin Shubibinman 2 - Aratanaru Teki\0", NULL, "Masiya", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pshubibi2RomInfo, pshubibi2RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Kato Chan & Ken Chan

static struct BurnRomInfo pkatochanRomDesc[] = {
	{ "kato chan & ken chan (japan).pce",	0x40000, 0x6069c5e7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pkatochan)
STD_ROM_FN(pkatochan)

struct BurnDriver BurnDrvPCE_pkatochan = {
	"pce_katochan", NULL, NULL, NULL, "1987",
	"Kato Chan & Ken Chan\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pkatochanRomInfo, pkatochanRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Kattobi! Takuhai Kun

static struct BurnRomInfo pkattobiRomDesc[] = {
	{ "kattobi! takuhai kun (japan).pce",	0x60000, 0x4f2844b0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pkattobi)
STD_ROM_FN(pkattobi)

struct BurnDriver BurnDrvPCE_pkattobi = {
	"pce_kattobi", NULL, NULL, NULL, "1990",
	"Kattobi! Takuhai Kun\0", NULL, "Tonkin House", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pkattobiRomInfo, pkattobiRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Kickball

static struct BurnRomInfo pkickballRomDesc[] = {
	{ "kickball (japan).pce",	0x60000, 0x7e3c367b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pkickball)
STD_ROM_FN(pkickball)

struct BurnDriver BurnDrvPCE_pkickball = {
	"pce_kickball", NULL, NULL, NULL, "1990",
	"Kickball\0", NULL, "Masiya", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pkickballRomInfo, pkickballRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Kiki KaiKai

static struct BurnRomInfo pkikikaiRomDesc[] = {
	{ "kiki kaikai (japan).pce",	0x60000, 0xc0cb5add, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pkikikai)
STD_ROM_FN(pkikikai)

struct BurnDriver BurnDrvPCE_pkikikai = {
	"pce_kikikai", NULL, NULL, NULL, "1990",
	"Kiki KaiKai\0", NULL, "Taito", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pkikikaiRomInfo, pkikikaiRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// King of Casino

static struct BurnRomInfo pkingcasnRomDesc[] = {
	{ "king of casino (japan).pce",	0x40000, 0xbf52788e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pkingcasn)
STD_ROM_FN(pkingcasn)

struct BurnDriver BurnDrvPCE_pkingcasn = {
	"pce_kingcasn", NULL, NULL, NULL, "1990",
	"King of Casino\0", NULL, "Victor Interactive Software", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pkingcasnRomInfo, pkingcasnRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Klax

static struct BurnRomInfo pklaxRomDesc[] = {
	{ "klax (japan).pce",	0x40000, 0xc74ffbc9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pklax)
STD_ROM_FN(pklax)

struct BurnDriver BurnDrvPCE_pklax = {
	"pce_klax", NULL, NULL, NULL, "1990",
	"Klax\0", NULL, "Tengen", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pklaxRomInfo, pklaxRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Knight Rider Special

static struct BurnRomInfo pknightrsRomDesc[] = {
	{ "knight rider special (japan).pce",	0x40000, 0xc614116c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pknightrs)
STD_ROM_FN(pknightrs)

struct BurnDriver BurnDrvPCE_pknightrs = {
	"pce_knightrs", NULL, NULL, NULL, "1989",
	"Knight Rider Special\0", NULL, "Pack-In-Video", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pknightrsRomInfo, pknightrsRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Kore Ga Pro Yakyuu '89

static struct BurnRomInfo pproyak89RomDesc[] = {
	{ "kore ga pro yakyuu '89 (japan).pce",	0x40000, 0x44f60137, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pproyak89)
STD_ROM_FN(pproyak89)

struct BurnDriver BurnDrvPCE_pproyak89 = {
	"pce_proyak89", NULL, NULL, NULL, "1989",
	"Kore Ga Pro Yakyuu '89\0", NULL, "Intec", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pproyak89RomInfo, pproyak89RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Kore Ga Pro Yakyuu '90

static struct BurnRomInfo pproyak90RomDesc[] = {
	{ "kore ga pro yakyuu '90 (japan).pce",	0x40000, 0x1772b229, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pproyak90)
STD_ROM_FN(pproyak90)

struct BurnDriver BurnDrvPCE_pproyak90 = {
	"pce_proyak90", NULL, NULL, NULL, "1990",
	"Kore Ga Pro Yakyuu '90\0", NULL, "Intec", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pproyak90RomInfo, pproyak90RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// The Kung Fu

static struct BurnRomInfo pkungfuRomDesc[] = {
	{ "kung fu, the (japan).pce",	0x40000, 0xb552c906, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pkungfu)
STD_ROM_FN(pkungfu)

struct BurnDriver BurnDrvPCE_pkungfu = {
	"pce_kungfu", NULL, NULL, NULL, "1987",
	"The Kung Fu\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pkungfuRomInfo, pkungfuRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Kyuukyoku Mahjong - Idol Graphics

static struct BurnRomInfo pkyukyomjRomDesc[] = {
	{ "kyuukyoku mahjong - idol graphics (japan).pce",	0x80000, 0x02dde03e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pkyukyomj)
STD_ROM_FN(pkyukyomj)

struct BurnDriver BurnDrvPCE_pkyukyomj = {
	"pce_kyukyomj", NULL, NULL, NULL, "19??",
	"Kyuukyoku Mahjong - Idol Graphics\0", NULL, "Unknown", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pkyukyomjRomInfo, pkyukyomjRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Kyuukyoku Mahjong II

static struct BurnRomInfo pkyukyom2RomDesc[] = {
	{ "kyuukyoku mahjong ii (japan).pce",	0x80000, 0xe5b6b3e5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pkyukyom2)
STD_ROM_FN(pkyukyom2)

struct BurnDriver BurnDrvPCE_pkyukyom2 = {
	"pce_kyukyom2", NULL, NULL, NULL, "19??",
	"Kyuukyoku Mahjong II\0", NULL, "Unknown", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pkyukyom2RomInfo, pkyukyom2RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Kyuukyoku Tiger

static struct BurnRomInfo pkyutigerRomDesc[] = {
	{ "kyuukyoku tiger (japan).pce",	0x40000, 0x09509315, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pkyutiger)
STD_ROM_FN(pkyutiger)

struct BurnDriver BurnDrvPCE_pkyutiger = {
	"pce_kyutiger", NULL, NULL, NULL, "1989",
	"Kyuukyoku Tiger\0", NULL, "Taito", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pkyutigerRomInfo, pkyutigerRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Lady Sword

static struct BurnRomInfo pladyswrdRomDesc[] = {
	{ "lady sword (japan).pce",	0x100000, 0xc6f764ec, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pladyswrd)
STD_ROM_FN(pladyswrd)

struct BurnDriver BurnDrvPCE_pladyswrd = {
	"pce_ladyswrd", NULL, NULL, NULL, "19??",
	"Lady Sword\0", NULL, "Unknown", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pladyswrdRomInfo, pladyswrdRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Lady Sword (Alt)

static struct BurnRomInfo pladyswrd1RomDesc[] = {
	{ "lady sword (japan) [a].pce",	0x100000, 0xeb833d15, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pladyswrd1)
STD_ROM_FN(pladyswrd1)

struct BurnDriver BurnDrvPCE_pladyswrd1 = {
	"pce_ladyswrd1", "pce_ladyswrd", NULL, NULL, "19??",
	"Lady Sword (Alt)\0", NULL, "Unknown", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pladyswrd1RomInfo, pladyswrd1RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Legend of Hero Tonma

static struct BurnRomInfo plohtRomDesc[] = {
	{ "legend of hero tonma (japan).pce",	0x80000, 0xc28b0d8a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ploht)
STD_ROM_FN(ploht)

struct BurnDriver BurnDrvPCE_ploht = {
	"pce_loht", NULL, NULL, NULL, "1991",
	"Legend of Hero Tonma\0", NULL, "Irem", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, plohtRomInfo, plohtRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Lode Runner - Lost Labyrinth

static struct BurnRomInfo ploderunRomDesc[] = {
	{ "lode runner - lost labyrinth (japan).pce",	0x40000, 0xe6ee1468, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ploderun)
STD_ROM_FN(ploderun)

struct BurnDriver BurnDrvPCE_ploderun = {
	"pce_loderun", NULL, NULL, NULL, "1990",
	"Lode Runner - Lost Labyrinth\0", NULL, "Pack-In-Video", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ploderunRomInfo, ploderunRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Märchen Maze

static struct BurnRomInfo pmarchenRomDesc[] = {
	{ "maerchen maze (japan).pce",	0x40000, 0xa15a1f37, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pmarchen)
STD_ROM_FN(pmarchen)

struct BurnDriver BurnDrvPCE_pmarchen = {
	"pce_marchen", NULL, NULL, NULL, "1990",
	"Märchen Maze\0", NULL, "Namco", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pmarchenRomInfo, pmarchenRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Magical Chase

static struct BurnRomInfo pmagchaseRomDesc[] = {
	{ "magical chase (japan).pce",	0x80000, 0xdd0ebf8c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pmagchase)
STD_ROM_FN(pmagchase)

struct BurnDriver BurnDrvPCE_pmagchase = {
	"pce_magchase", NULL, NULL, NULL, "1991",
	"Magical Chase\0", NULL, "Palsoft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pmagchaseRomInfo, pmagchaseRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Mahjong Gakuen - Touma Soushirou Toujou

static struct BurnRomInfo pmjgakuenRomDesc[] = {
	{ "mahjong gakuen - touma soushirou toujou (japan).pce",	0x80000, 0xf5b90d55, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pmjgakuen)
STD_ROM_FN(pmjgakuen)

struct BurnDriver BurnDrvPCE_pmjgakuen = {
	"pce_mjgakuen", NULL, NULL, NULL, "1990",
	"Mahjong Gakuen - Touma Soushirou Toujou\0", NULL, "Face", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pmjgakuenRomInfo, pmjgakuenRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Mahjong Gakuen Mild - Touma Soushirou Toujou

static struct BurnRomInfo pmjgakmldRomDesc[] = {
	{ "mahjong gakuen mild - touma soushirou toujou (japan).pce",	0x80000, 0xf4148600, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pmjgakmld)
STD_ROM_FN(pmjgakmld)

struct BurnDriver BurnDrvPCE_pmjgakmld = {
	"pce_mjgakmld", NULL, NULL, NULL, "1990",
	"Mahjong Gakuen Mild - Touma Soushirou Toujou\0", NULL, "Face", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pmjgakmldRomInfo, pmjgakmldRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Mahjong Gakuen Mild - Touma Soushirou Toujou (Alt)

static struct BurnRomInfo pmjgakmld1RomDesc[] = {
	{ "mahjong gakuen mild - touma soushirou toujou (japan) [a].pce",	0x80000, 0x3e4d432a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pmjgakmld1)
STD_ROM_FN(pmjgakmld1)

struct BurnDriver BurnDrvPCE_pmjgakmld1 = {
	"pce_mjgakmld1", "pce_mjgakmld", NULL, NULL, "1990",
	"Mahjong Gakuen Mild - Touma Soushirou Toujou (Alt)\0", NULL, "Face", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pmjgakmld1RomInfo, pmjgakmld1RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Mahjong Gokuu Special

static struct BurnRomInfo pmjgokuspRomDesc[] = {
	{ "mahjong gokuu special (japan).pce",	0x60000, 0xf8861456, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pmjgokusp)
STD_ROM_FN(pmjgokusp)

struct BurnDriver BurnDrvPCE_pmjgokusp = {
	"pce_mjgokusp", NULL, NULL, NULL, "1990",
	"Mahjong Gokuu Special\0", NULL, "Sunsoft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pmjgokuspRomInfo, pmjgokuspRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Mahjong Haou Den - Kaiser's Quest

static struct BurnRomInfo pmjkaiserRomDesc[] = {
	{ "mahjong haou den - kaiser's quest (japan).pce",	0x80000, 0xdf10c895, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pmjkaiser)
STD_ROM_FN(pmjkaiser)

struct BurnDriver BurnDrvPCE_pmjkaiser = {
	"pce_mjkaiser", NULL, NULL, NULL, "1992",
	"Mahjong Haou Den - Kaiser's Quest\0", NULL, "UPL", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pmjkaiserRomInfo, pmjkaiserRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Mahjong Shikyaku Retsuden - Mahjong Wars

static struct BurnRomInfo pmjwarsRomDesc[] = {
	{ "mahjong shikyaku retsuden - mahjong wars (japan).pce",	0x40000, 0x6c34aaea, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pmjwars)
STD_ROM_FN(pmjwars)

struct BurnDriver BurnDrvPCE_pmjwars = {
	"pce_mjwars", NULL, NULL, NULL, "1990",
	"Mahjong Shikyaku Retsuden - Mahjong Wars\0", NULL, "Nihon Bussan", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pmjwarsRomInfo, pmjwarsRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Maison Ikkoku

static struct BurnRomInfo pmikkokuRomDesc[] = {
	{ "maison ikkoku (japan).pce",	0x40000, 0x5c78fee1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pmikkoku)
STD_ROM_FN(pmikkoku)

struct BurnDriver BurnDrvPCE_pmikkoku = {
	"pce_mikkoku", NULL, NULL, NULL, "1989",
	"Maison Ikkoku\0", NULL, "Micro Cabin", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pmikkokuRomInfo, pmikkokuRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Majin Eiyuu Den Wataru

static struct BurnRomInfo pmajinewRomDesc[] = {
	{ "majin eiyuu den wataru (japan).pce",	0x40000, 0x2f8935aa, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pmajinew)
STD_ROM_FN(pmajinew)

struct BurnDriver BurnDrvPCE_pmajinew = {
	"pce_majinew", NULL, NULL, NULL, "1988",
	"Majin Eiyuu Den Wataru\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pmajinewRomInfo, pmajinewRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Makai Hakken Den Shada

static struct BurnRomInfo pmakaihakRomDesc[] = {
	{ "makai hakken den shada (japan).pce",	0x40000, 0xbe62eef5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pmakaihak)
STD_ROM_FN(pmakaihak)

struct BurnDriver BurnDrvPCE_pmakaihak = {
	"pce_makaihak", NULL, NULL, NULL, "1989",
	"Makai Hakken Den Shada\0", NULL, "Data East", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pmakaihakRomInfo, pmakaihakRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Makai Prince Dorabocchan

static struct BurnRomInfo pmakaipriRomDesc[] = {
	{ "makai prince dorabocchan (japan).pce",	0x60000, 0xb101b333, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pmakaipri)
STD_ROM_FN(pmakaipri)

struct BurnDriver BurnDrvPCE_pmakaipri = {
	"pce_makaipri", NULL, NULL, NULL, "1990",
	"Makai Prince Dorabocchan\0", NULL, "Naxat", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pmakaipriRomInfo, pmakaipriRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Makyou Densetsu

static struct BurnRomInfo pmakyodenRomDesc[] = {
	{ "makyou densetsu (japan).pce",	0x40000, 0xd4c5af46, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pmakyoden)
STD_ROM_FN(pmakyoden)

struct BurnDriver BurnDrvPCE_pmakyoden = {
	"pce_makyoden", NULL, NULL, NULL, "1988",
	"Makyou Densetsu\0", NULL, "Victor Interactive Software", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pmakyodenRomInfo, pmakyodenRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Maniac Puroresu - Asu Heno Tatakai

static struct BurnRomInfo pmaniacRomDesc[] = {
	{ "maniac puroresu - asu heno tatakai (japan).pce",	0x80000, 0x99f2865c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pmaniac)
STD_ROM_FN(pmaniac)

struct BurnDriver BurnDrvPCE_pmaniac = {
	"pce_maniac", NULL, NULL, NULL, "1990",
	"Maniac Puroresu - Asu Heno Tatakai\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pmaniacRomInfo, pmaniacRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Mesopotamia

static struct BurnRomInfo pmesopotRomDesc[] = {
	{ "mesopotamia (japan).pce",	0x80000, 0xe87190f1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pmesopot)
STD_ROM_FN(pmesopot)

struct BurnDriver BurnDrvPCE_pmesopot = {
	"pce_mesopot", NULL, NULL, NULL, "1991",
	"Mesopotamia\0", NULL, "Atlus", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pmesopotRomInfo, pmesopotRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Metal Stoker

static struct BurnRomInfo pmetlstokRomDesc[] = {
	{ "metal stoker (japan).pce",	0x80000, 0x25a02bee, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pmetlstok)
STD_ROM_FN(pmetlstok)

struct BurnDriver BurnDrvPCE_pmetlstok = {
	"pce_metlstok", NULL, NULL, NULL, "1991",
	"Metal Stoker\0", NULL, "Face", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pmetlstokRomInfo, pmetlstokRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Mizubaku Dai Bouken

static struct BurnRomInfo pmizubakuRomDesc[] = {
	{ "mizubaku dai bouken (japan).pce",	0x80000, 0xb2ef558d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pmizubaku)
STD_ROM_FN(pmizubaku)

struct BurnDriver BurnDrvPCE_pmizubaku = {
	"pce_mizubaku", NULL, NULL, NULL, "1992",
	"Mizubaku Dai Bouken\0", NULL, "Namco", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pmizubakuRomInfo, pmizubakuRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Momotarou Densetsu Gaiden Dai 1 Shuu

static struct BurnRomInfo pmomogdnRomDesc[] = {
	{ "momotarou densetsu gaiden dai 1 shuu (japan).pce",	0x80000, 0xf860455c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pmomogdn)
STD_ROM_FN(pmomogdn)

struct BurnDriver BurnDrvPCE_pmomogdn = {
	"pce_momogdn", NULL, NULL, NULL, "1992",
	"Momotarou Densetsu Gaiden Dai 1 Shuu\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pmomogdnRomInfo, pmomogdnRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Momotarou Densetsu II

static struct BurnRomInfo pmomo2RomDesc[] = {
	{ "momotarou densetsu ii (japan).pce",	0xc0000, 0xd9e1549a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pmomo2)
STD_ROM_FN(pmomo2)

struct BurnDriver BurnDrvPCE_pmomo2 = {
	"pce_momo2", NULL, NULL, NULL, "1990",
	"Momotarou Densetsu II\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pmomo2RomInfo, pmomo2RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Momotarou Densetsu Turbo

static struct BurnRomInfo pmomotrboRomDesc[] = {
	{ "momotarou densetsu turbo (japan).pce",	0x60000, 0x625221a6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pmomotrbo)
STD_ROM_FN(pmomotrbo)

struct BurnDriver BurnDrvPCE_pmomotrbo = {
	"pce_momotrbo", NULL, NULL, NULL, "1990",
	"Momotarou Densetsu Turbo\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pmomotrboRomInfo, pmomotrboRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Momotarou Katsugeki

static struct BurnRomInfo pmomoktsgRomDesc[] = {
	{ "momotarou katsugeki (japan).pce",	0x80000, 0x345f43e9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pmomoktsg)
STD_ROM_FN(pmomoktsg)

struct BurnDriver BurnDrvPCE_pmomoktsg = {
	"pce_momoktsg", NULL, NULL, NULL, "1990",
	"Momotarou Katsugeki\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pmomoktsgRomInfo, pmomoktsgRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Monster Puroresu

static struct BurnRomInfo pmonstpurRomDesc[] = {
	{ "monster puroresu (japan).pce",	0x80000, 0xf2e46d25, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pmonstpur)
STD_ROM_FN(pmonstpur)

struct BurnDriver BurnDrvPCE_pmonstpur = {
	"pce_monstpur", NULL, NULL, NULL, "1991",
	"Monster Puroresu\0", NULL, "ASK", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pmonstpurRomInfo, pmonstpurRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Morita Shougi PC

static struct BurnRomInfo pmorishogRomDesc[] = {
	{ "morita shougi pc (japan).pce",	0x80000, 0x2546efe0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pmorishog)
STD_ROM_FN(pmorishog)

struct BurnDriver BurnDrvPCE_pmorishog = {
	"pce_morishog", NULL, NULL, NULL, "1991",
	"Morita Shougi PC\0", NULL, "NEC Avenue", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pmorishogRomInfo, pmorishogRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Moto Roader

static struct BurnRomInfo pmotoroadRomDesc[] = {
	{ "moto roader (japan).pce",	0x40000, 0x428f36cd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pmotoroad)
STD_ROM_FN(pmotoroad)

struct BurnDriver BurnDrvPCE_pmotoroad = {
	"pce_motoroad", NULL, NULL, NULL, "1989",
	"Moto Roader\0", NULL, "NCS", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pmotoroadRomInfo, pmotoroadRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Moto Roader II

static struct BurnRomInfo pmotorod2RomDesc[] = {
	{ "moto roader ii (japan).pce",	0x60000, 0x0b7f6e5f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pmotorod2)
STD_ROM_FN(pmotorod2)

struct BurnDriver BurnDrvPCE_pmotorod2 = {
	"pce_motorod2", NULL, NULL, NULL, "1991",
	"Moto Roader II\0", NULL, "Masiya", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pmotorod2RomInfo, pmotorod2RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Moto Roader II (Alt)

static struct BurnRomInfo pmotorod2aRomDesc[] = {
	{ "moto roader ii (japan) [a].pce",	0x60000, 0x4ba525ba, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pmotorod2a)
STD_ROM_FN(pmotorod2a)

struct BurnDriver BurnDrvPCE_pmotorod2a = {
	"pce_motorod2a", "pce_motorod2", NULL, NULL, "1991",
	"Moto Roader II (Alt)\0", NULL, "Masiya", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pmotorod2aRomInfo, pmotorod2aRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Mr. Heli no Daibouken

static struct BurnRomInfo pmrheliRomDesc[] = {
	{ "mr. heli no daibouken (japan).pce",	0x80000, 0x2cb92290, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pmrheli)
STD_ROM_FN(pmrheli)

struct BurnDriver BurnDrvPCE_pmrheli = {
	"pce_mrheli", NULL, NULL, NULL, "1989",
	"Mr. Heli no Daibouken\0", NULL, "Irem", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pmrheliRomInfo, pmrheliRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Mr. Heli no Daibouken (Alt)

static struct BurnRomInfo pmrheli1RomDesc[] = {
	{ "mr. heli no daibouken (japan) [a].pce",	0x80000, 0xac0cd796, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pmrheli1)
STD_ROM_FN(pmrheli1)

struct BurnDriver BurnDrvPCE_pmrheli1 = {
	"pce_mrheli1", "pce_mrheli", NULL, NULL, "1989",
	"Mr. Heli no Daibouken (Alt)\0", NULL, "Irem", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pmrheli1RomInfo, pmrheli1RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Narazumono Sentai Butai - Bloody Wolf

static struct BurnRomInfo pblodwolfRomDesc[] = {
	{ "narazumono sentai butai - bloody wolf (japan).pce",	0x80000, 0xb01f70c2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pblodwolf)
STD_ROM_FN(pblodwolf)

struct BurnDriver BurnDrvPCE_pblodwolf = {
	"pce_blodwolf", NULL, NULL, NULL, "1989",
	"Narazumono Sentai Butai - Bloody Wolf\0", NULL, "Data East", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pblodwolfRomInfo, pblodwolfRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Naxat Open

static struct BurnRomInfo pnaxopenRomDesc[] = {
	{ "naxat open (japan).pce",	0x60000, 0x60ecae22, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pnaxopen)
STD_ROM_FN(pnaxopen)

struct BurnDriver BurnDrvPCE_pnaxopen = {
	"pce_naxopen", NULL, NULL, NULL, "1989",
	"Naxat Open\0", NULL, "Naxat", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pnaxopenRomInfo, pnaxopenRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Naxat Stadium

static struct BurnRomInfo pnaxstadRomDesc[] = {
	{ "naxat stadium (japan).pce",	0x80000, 0x20a7d128, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pnaxstad)
STD_ROM_FN(pnaxstad)

struct BurnDriver BurnDrvPCE_pnaxstad = {
	"pce_naxstad", NULL, NULL, NULL, "1990",
	"Naxat Stadium\0", NULL, "Naxat", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pnaxstadRomInfo, pnaxstadRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Nazo no Mascarade

static struct BurnRomInfo pnazomascRomDesc[] = {
	{ "nazo no mascarade (japan).pce",	0x60000, 0x0441d85a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pnazomasc)
STD_ROM_FN(pnazomasc)

struct BurnDriver BurnDrvPCE_pnazomasc = {
	"pce_nazomasc", NULL, NULL, NULL, "1990",
	"Nazo no Mascarade\0", NULL, "Masiya", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pnazomascRomInfo, pnazomascRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Necromancer

static struct BurnRomInfo pnecromcrRomDesc[] = {
	{ "necromancer (japan).pce",	0x40000, 0x53109ae6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pnecromcr)
STD_ROM_FN(pnecromcr)

struct BurnDriver BurnDrvPCE_pnecromcr = {
	"pce_necromcr", NULL, NULL, NULL, "1988",
	"Necromancer\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pnecromcrRomInfo, pnecromcrRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Necros no Yousai

static struct BurnRomInfo pnecrosRomDesc[] = {
	{ "necros no yousai (japan).pce",	0x80000, 0xfb0fdcfe, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pnecros)
STD_ROM_FN(pnecros)

struct BurnDriver BurnDrvPCE_pnecros = {
	"pce_necros", NULL, NULL, NULL, "1990",
	"Necros no Yousai\0", NULL, "ASK", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pnecrosRomInfo, pnecrosRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Nectaris

static struct BurnRomInfo pnectarisRomDesc[] = {
	{ "nectaris (japan).pce",	0x60000, 0x0243453b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pnectaris)
STD_ROM_FN(pnectaris)

struct BurnDriver BurnDrvPCE_pnectaris = {
	"pce_nectaris", NULL, NULL, NULL, "1990",
	"Nectaris\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pnectarisRomInfo, pnectarisRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Nekketsu Koukou Dodgeball Bu - PC Bangai Hen

static struct BurnRomInfo pdodgebanRomDesc[] = {
	{ "nekketsu koukou dodgeball bu - pc bangai hen (japan).pce",	0x40000, 0x65fdb863, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pdodgeban)
STD_ROM_FN(pdodgeban)

struct BurnDriver BurnDrvPCE_pdodgeban = {
	"pce_dodgeban", NULL, NULL, NULL, "1990",
	"Nekketsu Koukou Dodgeball Bu - PC Bangai Hen\0", NULL, "Naxat", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pdodgebanRomInfo, pdodgebanRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Nekketsu Koukou Dodgeball Bu - Soccer PC Hen

static struct BurnRomInfo pdodgesocRomDesc[] = {
	{ "nekketsu koukou dodgeball bu - soccer pc hen (japan).pce",	0x80000, 0xf2285c6d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pdodgesoc)
STD_ROM_FN(pdodgesoc)

struct BurnDriver BurnDrvPCE_pdodgesoc = {
	"pce_dodgesoc", NULL, NULL, NULL, "1992",
	"Nekketsu Koukou Dodgeball Bu - Soccer PC Hen\0", NULL, "Naxat", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pdodgesocRomInfo, pdodgesocRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Neutopia

static struct BurnRomInfo pneutopiaRomDesc[] = {
	{ "neutopia (japan).pce",	0x60000, 0x9c49ef11, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pneutopia)
STD_ROM_FN(pneutopia)

struct BurnDriver BurnDrvPCE_pneutopia = {
	"pce_neutopia", NULL, NULL, NULL, "1989",
	"Neutopia\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pneutopiaRomInfo, pneutopiaRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Neutopia II

static struct BurnRomInfo pneutopi2RomDesc[] = {
	{ "neutopia ii (japan).pce",	0xc0000, 0x2b94aedc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pneutopi2)
STD_ROM_FN(pneutopi2)

struct BurnDriver BurnDrvPCE_pneutopi2 = {
	"pce_neutopi2", NULL, NULL, NULL, "1991",
	"Neutopia II\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pneutopi2RomInfo, pneutopi2RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// The New Zealand Story

static struct BurnRomInfo ptnzsRomDesc[] = {
	{ "new zealand story, the (japan).pce",	0x60000, 0x8e4d75a8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ptnzs)
STD_ROM_FN(ptnzs)

struct BurnDriver BurnDrvPCE_ptnzs = {
	"pce_tnzs", NULL, NULL, NULL, "1990",
	"The New Zealand Story\0", NULL, "Taito", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ptnzsRomInfo, ptnzsRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// NHK Taiga Drama - Taiheiki

static struct BurnRomInfo pnhktaidrRomDesc[] = {
	{ "nhk taiga drama - taiheiki (japan).pce",	0x80000, 0xa32430d5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pnhktaidr)
STD_ROM_FN(pnhktaidr)

struct BurnDriver BurnDrvPCE_pnhktaidr = {
	"pce_nhktaidr", NULL, NULL, NULL, "1992",
	"NHK Taiga Drama - Taiheiki\0", NULL, "NHK Enterprise", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pnhktaidrRomInfo, pnhktaidrRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Niko Niko Pun

static struct BurnRomInfo pnikopunRomDesc[] = {
	{ "niko niko pun (japan).pce",	0x80000, 0x82def9ee, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pnikopun)
STD_ROM_FN(pnikopun)

struct BurnDriverD BurnDrvPCE_pnikopun = {
	"pce_nikopun", NULL, NULL, NULL, "1991",
	"Niko Niko Pun\0", "Locks up in-game?", "NHK Enterprise", "PC Engine",
	NULL, NULL, NULL, NULL,
	0, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pnikopunRomInfo, pnikopunRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Ninja Ryukenden

static struct BurnRomInfo pninjaryuRomDesc[] = {
	{ "ninja ryuuken den (japan).pce",	0x80000, 0x67573bac, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pninjaryu)
STD_ROM_FN(pninjaryu)

struct BurnDriver BurnDrvPCE_pninjaryu = {
	"pce_ninjaryu", NULL, NULL, NULL, "1992",
	"Ninja Ryukenden\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pninjaryuRomInfo, pninjaryuRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// The Ninja Warriors

static struct BurnRomInfo pninjawarRomDesc[] = {
	{ "ninja warriors, the (japan).pce",	0x60000, 0x96e0cd9d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pninjawar)
STD_ROM_FN(pninjawar)

struct BurnDriver BurnDrvPCE_pninjawar = {
	"pce_ninjawar", NULL, NULL, NULL, "1989",
	"The Ninja Warriors\0", NULL, "Taito", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pninjawarRomInfo, pninjawarRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Obocchama Kun

static struct BurnRomInfo pobocchaRomDesc[] = {
	{ "obocchama kun (japan).pce",	0x80000, 0x4d3b0bc9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(poboccha)
STD_ROM_FN(poboccha)

struct BurnDriver BurnDrvPCE_poboccha = {
	"pce_oboccha", NULL, NULL, NULL, "1991",
	"Obocchama Kun\0", NULL, "Namco", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pobocchaRomInfo, pobocchaRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Operation Wolf

static struct BurnRomInfo popwolfRomDesc[] = {
	{ "operation wolf (japan).pce",	0x80000, 0xff898f87, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(popwolf)
STD_ROM_FN(popwolf)

struct BurnDriver BurnDrvPCE_popwolf = {
	"pce_opwolf", NULL, NULL, NULL, "1990",
	"Operation Wolf\0", NULL, "NEC Avenue", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, popwolfRomInfo, popwolfRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Ordyne

static struct BurnRomInfo pordyneRomDesc[] = {
	{ "ordyne (japan).pce",	0x80000, 0x8c565cb6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pordyne)
STD_ROM_FN(pordyne)

struct BurnDriver BurnDrvPCE_pordyne = {
	"pce_ordyne", NULL, NULL, NULL, "1989",
	"Ordyne\0", NULL, "Namco", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pordyneRomInfo, pordyneRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Out Live

static struct BurnRomInfo poutliveRomDesc[] = {
	{ "out live (japan).pce",	0x40000, 0x5cdb3f5b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(poutlive)
STD_ROM_FN(poutlive)

struct BurnDriver BurnDrvPCE_poutlive = {
	"pce_outlive", NULL, NULL, NULL, "1989",
	"Out Live\0", NULL, "Sunsoft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, poutliveRomInfo, poutliveRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Out Run

static struct BurnRomInfo poutrunRomDesc[] = {
	{ "out run (japan).pce",	0x80000, 0xe203f223, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(poutrun)
STD_ROM_FN(poutrun)

struct BurnDriver BurnDrvPCE_poutrun = {
	"pce_outrun", NULL, NULL, NULL, "1990",
	"Out Run\0", NULL, "NEC Avenue", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, poutrunRomInfo, poutrunRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Override

static struct BurnRomInfo poverrideRomDesc[] = {
	{ "override (japan).pce",	0x40000, 0xb74ec562, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(poverride)
STD_ROM_FN(poverride)

struct BurnDriver BurnDrvPCE_poverride = {
	"pce_override", NULL, NULL, NULL, "1991",
	"Override\0", NULL, "Data East", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, poverrideRomInfo, poverrideRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// P-47 - The Freedom Fighter

static struct BurnRomInfo pp47RomDesc[] = {
	{ "p-47 - the freedom fighter (japan).pce",	0x40000, 0x7632db90, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pp47)
STD_ROM_FN(pp47)

struct BurnDriver BurnDrvPCE_pp47 = {
	"pce_p47", NULL, NULL, NULL, "1989",
	"P-47 - The Freedom Fighter\0", NULL, "Aicom", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pp47RomInfo, pp47RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Pac-land

static struct BurnRomInfo ppaclandRomDesc[] = {
	{ "pac-land (japan).pce",	0x40000, 0x14fad3ba, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ppacland)
STD_ROM_FN(ppacland)

struct BurnDriver BurnDrvPCE_ppacland = {
	"pce_pacland", NULL, NULL, NULL, "1989",
	"Pac-land\0", NULL, "Namco", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ppaclandRomInfo, ppaclandRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Pachio Kun - Juuban Shoubu

static struct BurnRomInfo ppachikunRomDesc[] = {
	{ "pachio kun - juuban shoubu (japan).pce",	0x80000, 0x4148fd7c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ppachikun)
STD_ROM_FN(ppachikun)

struct BurnDriver BurnDrvPCE_ppachikun = {
	"pce_pachikun", NULL, NULL, NULL, "1992",
	"Pachio Kun - Juuban Shoubu\0", NULL, "Coconuts Japan", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ppachikunRomInfo, ppachikunRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Paranoia

static struct BurnRomInfo pparanoiaRomDesc[] = {
	{ "paranoia (japan).pce",	0x40000, 0x9893e0e6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pparanoia)
STD_ROM_FN(pparanoia)

struct BurnDriver BurnDrvPCE_pparanoia = {
	"pce_paranoia", NULL, NULL, NULL, "1990",
	"Paranoia\0", NULL, "Naxat", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pparanoiaRomInfo, pparanoiaRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Parasol Stars - The Story of Bubble Bobble III

static struct BurnRomInfo pparasolRomDesc[] = {
	{ "parasol stars - the story of bubble bobble iii (japan).pce",	0x60000, 0x51e86451, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pparasol)
STD_ROM_FN(pparasol)

struct BurnDriver BurnDrvPCE_pparasol = {
	"pce_parasol", NULL, NULL, NULL, "1991",
	"Parasol Stars - The Story of Bubble Bobble III\0", NULL, "Taito", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pparasolRomInfo, pparasolRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Parodius da! - Shinwa Kara Owarai He

static struct BurnRomInfo pparodiusRomDesc[] = {
	{ "parodius da! - shinwa kara owarai he (japan).pce",	0x100000, 0x647718f9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pparodius)
STD_ROM_FN(pparodius)

struct BurnDriver BurnDrvPCE_pparodius = {
	"pce_parodius", NULL, NULL, NULL, "1992",
	"Parodius da! - Shinwa Kara Owarai He\0", NULL, "Konami", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pparodiusRomInfo, pparodiusRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// PC Denjin - Punkic Cyborgs

static struct BurnRomInfo ppcdenjRomDesc[] = {
	{ "pc denjin - punkic cyborgs (japan).pce",	0x80000, 0x740491c2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ppcdenj)
STD_ROM_FN(ppcdenj)

struct BurnDriverD BurnDrvPCE_ppcdenj = {
	"pce_pcdenj", NULL, NULL, NULL, "1992",
	"PC Denjin - Punkic Cyborgs\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	0, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ppcdenjRomInfo, ppcdenjRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// PC Denjin - Punkic Cyborgs (Alt)

static struct BurnRomInfo ppcdenjaRomDesc[] = {
	{ "pc denjin - punkic cyborgs (japan) [a].pce",	0x80000, 0x8fb4f228, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ppcdenja)
STD_ROM_FN(ppcdenja)

struct BurnDriverD BurnDrvPCE_ppcdenja = {
	"pce_pcdenja", "pce_pcdenj", NULL, NULL, "1992",
	"PC Denjin - Punkic Cyborgs (Alt)\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_CLONE, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ppcdenjaRomInfo, ppcdenjaRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// PC Genjin - Pithecanthropus Computerurus

static struct BurnRomInfo ppcgenjRomDesc[] = {
	{ "pc genjin - pithecanthropus computerurus (japan).pce",	0x60000, 0x2cb5cd55, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ppcgenj)
STD_ROM_FN(ppcgenj)

struct BurnDriver BurnDrvPCE_ppcgenj = {
	"pce_pcgenj", NULL, NULL, NULL, "1989",
	"PC Genjin - Pithecanthropus Computerurus\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ppcgenjRomInfo, ppcgenjRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// PC Genjin - Pithecanthropus Computerurus (Alt)

static struct BurnRomInfo ppcgenjaRomDesc[] = {
	{ "pc genjin - pithecanthropus computerurus (japan) [a].pce",	0x60000, 0x67b35e6e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ppcgenja)
STD_ROM_FN(ppcgenja)

struct BurnDriver BurnDrvPCE_ppcgenja = {
	"pce_pcgenja", "pce_pcgenj", NULL, NULL, "1989",
	"PC Genjin - Pithecanthropus Computerurus (Alt)\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ppcgenjaRomInfo, ppcgenjaRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// PC Genjin 2 - Pithecanthropus Computerurus

static struct BurnRomInfo ppcgenj2RomDesc[] = {
	{ "pc genjin 2 - pithecanthropus computerurus (japan).pce",	0x80000, 0x3028f7ca, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ppcgenj2)
STD_ROM_FN(ppcgenj2)

struct BurnDriver BurnDrvPCE_ppcgenj2 = {
	"pce_pcgenj2", NULL, NULL, NULL, "1991",
	"PC Genjin 2 - Pithecanthropus Computerurus\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ppcgenj2RomInfo, ppcgenj2RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// PC Genjin 3 - Pithecanthropus Computerurus

static struct BurnRomInfo ppcgenj3RomDesc[] = {
	{ "pc genjin 3 - pithecanthropus computerurus (japan).pce",	0x100000, 0xa170b60e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ppcgenj3)
STD_ROM_FN(ppcgenj3)

struct BurnDriver BurnDrvPCE_ppcgenj3 = {
	"pce_pcgenj3", NULL, NULL, NULL, "1993",
	"PC Genjin 3 - Pithecanthropus Computerurus\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ppcgenj3RomInfo, ppcgenj3RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// PC Genjin 3 - Pithecanthropus Computerurus (Taikenban)

static struct BurnRomInfo ppcgenj3tRomDesc[] = {
	{ "pc genjin 3 - pithecanthropus computerurus (taikenban) (japan).pce",	0xc0000, 0x6f6ed301, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ppcgenj3t)
STD_ROM_FN(ppcgenj3t)

struct BurnDriver BurnDrvPCE_ppcgenj3t = {
	"pce_pcgenj3t", NULL, NULL, NULL, "1993",
	"PC Genjin 3 - Pithecanthropus Computerurus (Taikenban)\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ppcgenj3tRomInfo, ppcgenj3tRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// PC Pachi-slot

static struct BurnRomInfo ppcpachiRomDesc[] = {
	{ "pc pachi-slot (japan).pce",	0x80000, 0x0aa88f33, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ppcpachi)
STD_ROM_FN(ppcpachi)

struct BurnDriver BurnDrvPCE_ppcpachi = {
	"pce_pcpachi", NULL, NULL, NULL, "1992",
	"PC Pachi-slot\0", NULL, "Game Express", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ppcpachiRomInfo, ppcpachiRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Populous

static struct BurnRomInfo ppopulousRomDesc[] = {
	{ "populous (japan).pce",	0x80000, 0x083c956a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ppopulous)
STD_ROM_FN(ppopulous)

static INT32 populousInit()
{
	int nRet = PCEInit();

	if (nRet == 0) {
		h6280Open(0);
		h6280MapMemory(PCECartRAM, 0x080000, 0x087fff, H6280_RAM);
		h6280Close();
	}

	return nRet;
}

struct BurnDriver BurnDrvPCE_ppopulous = {
	"pce_populous", NULL, NULL, NULL, "1991",
	"Populous\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ppopulousRomInfo, ppopulousRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	populousInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Populous (Alt)

static struct BurnRomInfo ppopulous1RomDesc[] = {
	{ "populous (japan) [a].pce",	0x80000, 0x0a9ade99, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ppopulous1)
STD_ROM_FN(ppopulous1)

struct BurnDriver BurnDrvPCE_ppopulous1 = {
	"pce_populous1", "pce_populous", NULL, NULL, "1991",
	"Populous (Alt)\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ppopulous1RomInfo, ppopulous1RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	populousInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Power Drift

static struct BurnRomInfo ppdriftRomDesc[] = {
	{ "power drift (japan).pce",	0x80000, 0x25e0f6e9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ppdrift)
STD_ROM_FN(ppdrift)

struct BurnDriver BurnDrvPCE_ppdrift = {
	"pce_pdrift", NULL, NULL, NULL, "1990",
	"Power Drift\0", NULL, "Asmik", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ppdriftRomInfo, ppdriftRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Power Drift (Alt)

static struct BurnRomInfo ppdrift1RomDesc[] = {
	{ "power drift (japan) [a].pce",	0x80000, 0x99e6d988, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ppdrift1)
STD_ROM_FN(ppdrift1)

struct BurnDriver BurnDrvPCE_ppdrift1 = {
	"pce_pdrift1", "pce_pdrift", NULL, NULL, "1990",
	"Power Drift (Alt)\0", NULL, "Asmik", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ppdrift1RomInfo, ppdrift1RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Power Eleven

static struct BurnRomInfo ppower11RomDesc[] = {
	{ "power eleven (japan).pce",	0x60000, 0x3e647d8b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ppower11)
STD_ROM_FN(ppower11)

struct BurnDriver BurnDrvPCE_ppower11 = {
	"pce_power11", NULL, NULL, NULL, "1991",
	"Power Eleven\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ppower11RomInfo, ppower11RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Power Gate

static struct BurnRomInfo ppowergatRomDesc[] = {
	{ "power gate (japan).pce",	0x40000, 0xbe8b6e3b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ppowergat)
STD_ROM_FN(ppowergat)

struct BurnDriver BurnDrvPCE_ppowergat = {
	"pce_powergat", NULL, NULL, NULL, "1991",
	"Power Gate\0", NULL, "Pack-In-Video", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ppowergatRomInfo, ppowergatRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Power Golf

static struct BurnRomInfo ppowerglfRomDesc[] = {
	{ "power golf (japan).pce",	0x60000, 0xea324f07, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ppowerglf)
STD_ROM_FN(ppowerglf)

struct BurnDriver BurnDrvPCE_ppowerglf = {
	"pce_powerglf", NULL, NULL, NULL, "1989",
	"Power Golf\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ppowerglfRomInfo, ppowerglfRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Power League '93

static struct BurnRomInfo ppleag93RomDesc[] = {
	{ "power league '93 (japan).pce",	0xc0000, 0x7d3e6f33, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ppleag93)
STD_ROM_FN(ppleag93)

struct BurnDriver BurnDrvPCE_ppleag93 = {
	"pce_pleag93", NULL, NULL, NULL, "1993",
	"Power League '93\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ppleag93RomInfo, ppleag93RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Power League (All Star Version)

static struct BurnRomInfo ppleagasRomDesc[] = {
	{ "power league (all star version) (japan).pce",	0x40000, 0x04a85769, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ppleagas)
STD_ROM_FN(ppleagas)

struct BurnDriver BurnDrvPCE_ppleagas = {
	"pce_pleagas", NULL, NULL, NULL, "19??",
	"Power League (All Star Version)\0", NULL, "Unknown", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ppleagasRomInfo, ppleagasRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Power League

static struct BurnRomInfo ppleagueRomDesc[] = {
	{ "power league (japan).pce",	0x40000, 0x69180984, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ppleague)
STD_ROM_FN(ppleague)

struct BurnDriver BurnDrvPCE_ppleague = {
	"pce_pleague", NULL, NULL, NULL, "1988",
	"Power League\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ppleagueRomInfo, ppleagueRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Power League II

static struct BurnRomInfo ppleag2RomDesc[] = {
	{ "power league ii (japan).pce",	0x60000, 0xc5fdfa89, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ppleag2)
STD_ROM_FN(ppleag2)

struct BurnDriver BurnDrvPCE_ppleag2 = {
	"pce_pleag2", NULL, NULL, NULL, "1989",
	"Power League II\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ppleag2RomInfo, ppleag2RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Power League III

static struct BurnRomInfo ppleag3RomDesc[] = {
	{ "power league iii (japan).pce",	0x60000, 0x8aa4b220, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ppleag3)
STD_ROM_FN(ppleag3)

struct BurnDriver BurnDrvPCE_ppleag3 = {
	"pce_pleag3", NULL, NULL, NULL, "1990",
	"Power League III\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ppleag3RomInfo, ppleag3RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Power League IV

static struct BurnRomInfo ppleag4RomDesc[] = {
	{ "power league iv (japan).pce",	0x80000, 0x30cc3563, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ppleag4)
STD_ROM_FN(ppleag4)

struct BurnDriver BurnDrvPCE_ppleag4 = {
	"pce_pleag4", NULL, NULL, NULL, "1991",
	"Power League IV\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ppleag4RomInfo, ppleag4RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Power League V

static struct BurnRomInfo ppleag5RomDesc[] = {
	{ "power league v (japan).pce",	0xc0000, 0x8b61e029, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ppleag5)
STD_ROM_FN(ppleag5)

struct BurnDriver BurnDrvPCE_ppleag5 = {
	"pce_pleag5", NULL, NULL, NULL, "1992",
	"Power League V\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ppleag5RomInfo, ppleag5RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Power Sports

static struct BurnRomInfo ppsportsRomDesc[] = {
	{ "power sports (japan).pce",	0x80000, 0x29eec024, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ppsports)
STD_ROM_FN(ppsports)

struct BurnDriver BurnDrvPCE_ppsports = {
	"pce_psports", NULL, NULL, NULL, "1992",
	"Power Sports\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ppsportsRomInfo, ppsportsRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Power Tennis

static struct BurnRomInfo pptennisRomDesc[] = {
	{ "power tennis (japan).pce",	0x80000, 0x8def5aa1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pptennis)
STD_ROM_FN(pptennis)

struct BurnDriverD BurnDrvPCE_pptennis = {
	"pce_ptennis", NULL, NULL, NULL, "1993",
	"Power Tennis\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	0, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pptennisRomInfo, pptennisRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Pro Tennis World Court

static struct BurnRomInfo pptennwcRomDesc[] = {
	{ "pro tennis world court (japan).pce",	0x40000, 0x11a36745, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pptennwc)
STD_ROM_FN(pptennwc)

struct BurnDriver BurnDrvPCE_pptennwc = {
	"pce_ptennwc", NULL, NULL, NULL, "1988",
	"Pro Tennis World Court\0", NULL, "Namco", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pptennwcRomInfo, pptennwcRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Pro Yakyuu World Stadium '91

static struct BurnRomInfo pproyak91RomDesc[] = {
	{ "pro yakyuu world stadium '91 (japan).pce",	0x40000, 0x66b167a9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pproyak91)
STD_ROM_FN(pproyak91)

struct BurnDriver BurnDrvPCE_pproyak91 = {
	"pce_proyak91", NULL, NULL, NULL, "1991",
	"Pro Yakyuu World Stadium '91\0", NULL, "Namco", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pproyak91RomInfo, pproyak91RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Pro Yakyuu World Stadium

static struct BurnRomInfo pproyakRomDesc[] = {
	{ "pro yakyuu world stadium (japan).pce",	0x40000, 0x34e089a9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pproyak)
STD_ROM_FN(pproyak)

struct BurnDriver BurnDrvPCE_pproyak = {
	"pce_proyak", NULL, NULL, NULL, "1988",
	"Pro Yakyuu World Stadium\0", NULL, "Namco", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pproyakRomInfo, pproyakRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Psycho Chaser

static struct BurnRomInfo ppsychasRomDesc[] = {
	{ "psycho chaser (japan).pce",	0x40000, 0x03883ee8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ppsychas)
STD_ROM_FN(ppsychas)

struct BurnDriver BurnDrvPCE_ppsychas = {
	"pce_psychas", NULL, NULL, NULL, "1990",
	"Psycho Chaser\0", NULL, "Naxat", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ppsychasRomInfo, ppsychasRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Puzzle Boy

static struct BurnRomInfo ppuzzlebRomDesc[] = {
	{ "puzzle boy (japan).pce",	0x40000, 0xfaa6e187, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ppuzzleb)
STD_ROM_FN(ppuzzleb)

struct BurnDriver BurnDrvPCE_ppuzzleb = {
	"pce_puzzleb", NULL, NULL, NULL, "1991",
	"Puzzle Boy\0", NULL, "Nippon Telenet", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ppuzzlebRomInfo, ppuzzlebRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Puzznic

static struct BurnRomInfo ppuzznicRomDesc[] = {
	{ "puzznic (japan).pce",	0x40000, 0x965c95b3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ppuzznic)
STD_ROM_FN(ppuzznic)

struct BurnDriver BurnDrvPCE_ppuzznic = {
	"pce_puzznic", NULL, NULL, NULL, "1990",
	"Puzznic\0", NULL, "Taito", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ppuzznicRomInfo, ppuzznicRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Quiz Toukou Shashin

static struct BurnRomInfo pquiztsRomDesc[] = {
	{ "quiz toukou shashin (japan).pce",	0x100000, 0xf2e6856d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pquizts)
STD_ROM_FN(pquizts)

struct BurnDriver BurnDrvPCE_pquizts = {
	"pce_quizts", NULL, NULL, NULL, "19??",
	"Quiz Toukou Shashin\0", NULL, "Unknown", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pquiztsRomInfo, pquiztsRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// R-Type Part-1

static struct BurnRomInfo prtypep1RomDesc[] = {
	{ "r-type part-1 (japan).pce",	0x40000, 0xcec3d28a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(prtypep1)
STD_ROM_FN(prtypep1)

struct BurnDriver BurnDrvPCE_prtypep1 = {
	"pce_rtypep1", NULL, NULL, NULL, "1988",
	"R-Type Part-1\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, prtypep1RomInfo, prtypep1RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// R-Type Part-2

static struct BurnRomInfo prtypep2RomDesc[] = {
	{ "r-type part-2 (japan).pce",	0x40000, 0xf207ecae, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(prtypep2)
STD_ROM_FN(prtypep2)

struct BurnDriver BurnDrvPCE_prtypep2 = {
	"pce_rtypep2", NULL, NULL, NULL, "1988",
	"R-Type Part-2\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, prtypep2RomInfo, prtypep2RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Rabio Lepus Special

static struct BurnRomInfo prabiolepRomDesc[] = {
	{ "rabio lepus special (japan).pce",	0x60000, 0xd8373de6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(prabiolep)
STD_ROM_FN(prabiolep)

struct BurnDriver BurnDrvPCE_prabiolep = {
	"pce_rabiolep", NULL, NULL, NULL, "1990",
	"Rabio Lepus Special\0", NULL, "Video System", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, prabiolepRomInfo, prabiolepRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Racing Damashii

static struct BurnRomInfo pracingdRomDesc[] = {
	{ "racing damashii (japan).pce",	0x80000, 0x3e79734c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pracingd)
STD_ROM_FN(pracingd)

struct BurnDriver BurnDrvPCE_pracingd = {
	"pce_racingd", NULL, NULL, NULL, "1991",
	"Racing Damashii\0", NULL, "Irem", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pracingdRomInfo, pracingdRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Raiden

static struct BurnRomInfo praidenRomDesc[] = {
	{ "raiden (japan).pce",	0xc0000, 0x850829f2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(praiden)
STD_ROM_FN(praiden)

struct BurnDriver BurnDrvPCE_praiden = {
	"pce_raiden", NULL, NULL, NULL, "1991",
	"Raiden\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, praidenRomInfo, praidenRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Rastan Saga II

static struct BurnRomInfo prastan2RomDesc[] = {
	{ "rastan saga ii (japan).pce",	0x60000, 0x00c38e69, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(prastan2)
STD_ROM_FN(prastan2)

struct BurnDriver BurnDrvPCE_prastan2 = {
	"pce_rastan2", NULL, NULL, NULL, "1990",
	"Rastan Saga II\0", NULL, "Taito", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, prastan2RomInfo, prastan2RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Rock-on

static struct BurnRomInfo prockonRomDesc[] = {
	{ "rock-on (japan).pce",	0x60000, 0x2fd65312, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(prockon)
STD_ROM_FN(prockon)

struct BurnDriver BurnDrvPCE_prockon = {
	"pce_rockon", NULL, NULL, NULL, "1989",
	"Rock-on\0", NULL, "Big Club", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, prockonRomInfo, prockonRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Ryukyu

static struct BurnRomInfo pryukyuRomDesc[] = {
	{ "ryukyu (japan).pce",	0x40000, 0x91e6896f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pryukyu)
STD_ROM_FN(pryukyu)

struct BurnDriver BurnDrvPCE_pryukyu = {
	"pce_ryukyu", NULL, NULL, NULL, "1990",
	"Ryukyu\0", NULL, "Face", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pryukyuRomInfo, pryukyuRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Sadakichi 7 Series - Hideyoshi no Ougon

static struct BurnRomInfo psadaki7RomDesc[] = {
	{ "sadakichi 7 series - hideyoshi no ougon (japan).pce",	0x40000, 0xf999356f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(psadaki7)
STD_ROM_FN(psadaki7)

struct BurnDriver BurnDrvPCE_psadaki7 = {
	"pce_sadaki7", NULL, NULL, NULL, "1988",
	"Sadakichi 7 Series - Hideyoshi no Ougon\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, psadaki7RomInfo, psadaki7RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Saigo no Nindou - Ninja Spirit

static struct BurnRomInfo pnspiritRomDesc[] = {
	{ "saigo no nindou - ninja spirit (japan).pce",	0x80000, 0x0590a156, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pnspirit)
STD_ROM_FN(pnspirit)

struct BurnDriver BurnDrvPCE_pnspirit = {
	"pce_nspirit", NULL, NULL, NULL, "1990",
	"Saigo no Nindou - Ninja Spirit\0", NULL, "Irem", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pnspiritRomInfo, pnspiritRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Salamander

static struct BurnRomInfo psalamandRomDesc[] = {
	{ "salamander (japan).pce",	0x40000, 0xfaecce20, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(psalamand)
STD_ROM_FN(psalamand)

struct BurnDriver BurnDrvPCE_psalamand = {
	"pce_salamand", NULL, NULL, NULL, "1991",
	"Salamander\0", NULL, "Konami", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, psalamandRomInfo, psalamandRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Sekigahara

static struct BurnRomInfo psekigahaRomDesc[] = {
	{ "sekigahara (japan).pce",	0x80000, 0x2e955051, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(psekigaha)
STD_ROM_FN(psekigaha)

struct BurnDriver BurnDrvPCE_psekigaha = {
	"pce_sekigaha", NULL, NULL, NULL, "1990",
	"Sekigahara\0", NULL, "Tonkin House", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, psekigahaRomInfo, psekigahaRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Sengoku Mahjong

static struct BurnRomInfo psengokmjRomDesc[] = {
	{ "sengoku mahjong (japan).pce",	0x40000, 0x90e6bf49, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(psengokmj)
STD_ROM_FN(psengokmj)

struct BurnDriver BurnDrvPCE_psengokmj = {
	"pce_sengokmj", NULL, NULL, NULL, "1988",
	"Sengoku Mahjong\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, psengokmjRomInfo, psengokmjRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Shanghai

static struct BurnRomInfo pshanghaiRomDesc[] = {
	{ "shanghai (japan).pce",	0x20000, 0x6923d736, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pshanghai)
STD_ROM_FN(pshanghai)

struct BurnDriver BurnDrvPCE_pshanghai = {
	"pce_shanghai", NULL, NULL, NULL, "1987",
	"Shanghai\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pshanghaiRomInfo, pshanghaiRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Shinobi

static struct BurnRomInfo pshinobiRomDesc[] = {
	{ "shinobi (japan).pce",	0x60000, 0xbc655cf3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pshinobi)
STD_ROM_FN(pshinobi)

struct BurnDriver BurnDrvPCE_pshinobi = {
	"pce_shinobi", NULL, NULL, NULL, "1989",
	"Shinobi\0", NULL, "Asmik", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pshinobiRomInfo, pshinobiRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Shiryou Sensen

static struct BurnRomInfo pshiryoRomDesc[] = {
	{ "shiryou sensen (japan).pce",	0x40000, 0x469a0fdf, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pshiryo)
STD_ROM_FN(pshiryo)

struct BurnDriver BurnDrvPCE_pshiryo = {
	"pce_shiryo", NULL, NULL, NULL, "1989",
	"Shiryou Sensen\0", NULL, "Victor Interactive Software", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pshiryoRomInfo, pshiryoRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Shougi Shodan Icchokusen

static struct BurnRomInfo pshogisiRomDesc[] = {
	{ "shougi shodan icchokusen (japan).pce",	0x40000, 0x23ec8970, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pshogisi)
STD_ROM_FN(pshogisi)

struct BurnDriver BurnDrvPCE_pshogisi = {
	"pce_shogisi", NULL, NULL, NULL, "1990",
	"Shougi Shodan Icchokusen\0", NULL, "Home Data", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pshogisiRomInfo, pshogisiRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Shougi Shoshinsha Muyou

static struct BurnRomInfo pshogismRomDesc[] = {
	{ "shougi shoshinsha muyou (japan).pce",	0x40000, 0x457f2bc4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pshogism)
STD_ROM_FN(pshogism)

struct BurnDriver BurnDrvPCE_pshogism = {
	"pce_shogism", NULL, NULL, NULL, "1991",
	"Shougi Shoshinsha Muyou\0", NULL, "Home Data", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pshogismRomInfo, pshogismRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Sidearms - Hyper Dyne

static struct BurnRomInfo psidearmsRomDesc[] = {
	{ "sidearms - hyper dyne (japan).pce",	0x40000, 0xe5e7b8b7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(psidearms)
STD_ROM_FN(psidearms)

struct BurnDriver BurnDrvPCE_psidearms = {
	"pce_sidearms", NULL, NULL, NULL, "1989",
	"Sidearms - Hyper Dyne\0", NULL, "NEC", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, psidearmsRomInfo, psidearmsRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Silent Debuggers

static struct BurnRomInfo psilentdRomDesc[] = {
	{ "silent debuggers (japan).pce",	0x80000, 0x616ea179, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(psilentd)
STD_ROM_FN(psilentd)

struct BurnDriver BurnDrvPCE_psilentd = {
	"pce_silentd", NULL, NULL, NULL, "1991",
	"Silent Debuggers\0", NULL, "Data East", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, psilentdRomInfo, psilentdRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Sindibad Chitei no Dai Makyuu

static struct BurnRomInfo psindibadRomDesc[] = {
	{ "sindibad chitei no dai makyuu (japan).pce",	0x60000, 0xb5c4eebd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(psindibad)
STD_ROM_FN(psindibad)

struct BurnDriver BurnDrvPCE_psindibad = {
	"pce_sindibad", NULL, NULL, NULL, "1990",
	"Sindibad Chitei no Dai Makyuu\0", NULL, "IGS", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, psindibadRomInfo, psindibadRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Skweek

static struct BurnRomInfo pskweekRomDesc[] = {
	{ "skweek (japan).pce",	0x40000, 0x4d539c9f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pskweek)
STD_ROM_FN(pskweek)

struct BurnDriver BurnDrvPCE_pskweek = {
	"pce_skweek", NULL, NULL, NULL, "1991",
	"Skweek\0", NULL, "Victor Interactive Software", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pskweekRomInfo, pskweekRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Soldier Blade

static struct BurnRomInfo psoldbladRomDesc[] = {
	{ "soldier blade (japan).pce",	0x80000, 0x8420b12b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(psoldblad)
STD_ROM_FN(psoldblad)

struct BurnDriver BurnDrvPCE_psoldblad = {
	"pce_soldblad", NULL, NULL, NULL, "1992",
	"Soldier Blade\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, psoldbladRomInfo, psoldbladRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Soldier Blade Special - Caravan Stage

static struct BurnRomInfo psoldblasRomDesc[] = {
	{ "soldier blade special - caravan stage (japan).pce",	0x80000, 0xf39f38ed, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(psoldblas)
STD_ROM_FN(psoldblas)

struct BurnDriver BurnDrvPCE_psoldblas = {
	"pce_soldblas", NULL, NULL, NULL, "19??",
	"Soldier Blade Special - Caravan Stage\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, psoldblasRomInfo, psoldblasRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Son Son II

static struct BurnRomInfo psonson2RomDesc[] = {
	{ "son son ii (japan).pce",	0x40000, 0xd7921df2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(psonson2)
STD_ROM_FN(psonson2)

struct BurnDriver BurnDrvPCE_psonson2 = {
	"pce_sonson2", NULL, NULL, NULL, "1989",
	"Son Son II\0", NULL, "NEC", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, psonson2RomInfo, psonson2RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Sokoban World

static struct BurnRomInfo psokobanRomDesc[] = {
	{ "soukoban world (japan).pce",	0x20000, 0xfb37ddc4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(psokoban)
STD_ROM_FN(psokoban)

struct BurnDriver BurnDrvPCE_psokoban = {
	"pce_sokoban", NULL, NULL, NULL, "1990",
	"Sokoban World\0", NULL, "Media Rings", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, psokobanRomInfo, psokobanRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Space Harrier

static struct BurnRomInfo psharrierRomDesc[] = {
	{ "space harrier (japan).pce",	0x80000, 0x64580427, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(psharrier)
STD_ROM_FN(psharrier)

struct BurnDriver BurnDrvPCE_psharrier = {
	"pce_sharrier", NULL, NULL, NULL, "1988",
	"Space Harrier\0", NULL, "NEC", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, psharrierRomInfo, psharrierRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Space Invaders - Fukkatsu no Hi

static struct BurnRomInfo psinvRomDesc[] = {
	{ "space invaders - fukkatsu no hi (japan).pce",	0x40000, 0x99496db3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(psinv)
STD_ROM_FN(psinv)

struct BurnDriver BurnDrvPCE_psinv = {
	"pce_sinv", NULL, NULL, NULL, "1990",
	"Space Invaders - Fukkatsu no Hi\0", NULL, "Taito", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, psinvRomInfo, psinvRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Special Criminal Investigation

static struct BurnRomInfo psciRomDesc[] = {
	{ "special criminal investigation (japan).pce",	0x80000, 0x09a0bfcc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(psci)
STD_ROM_FN(psci)

struct BurnDriver BurnDrvPCE_psci = {
	"pce_sci", NULL, NULL, NULL, "1991",
	"Special Criminal Investigation\0", NULL, "Taito", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, psciRomInfo, psciRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Spin Pair

static struct BurnRomInfo pspinpairRomDesc[] = {
	{ "spin pair (japan).pce",	0x40000, 0x1c6ff459, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pspinpair)
STD_ROM_FN(pspinpair)

struct BurnDriver BurnDrvPCE_pspinpair = {
	"pce_spinpair", NULL, NULL, NULL, "1990",
	"Spin Pair\0", NULL, "Media Rings", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pspinpairRomInfo, pspinpairRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Spiral Wave

static struct BurnRomInfo pspirwaveRomDesc[] = {
	{ "spiral wave (japan).pce",	0x80000, 0xa5290dd0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pspirwave)
STD_ROM_FN(pspirwave)

struct BurnDriver BurnDrvPCE_pspirwave = {
	"pce_spirwave", NULL, NULL, NULL, "1991",
	"Spiral Wave\0", NULL, "Media Rings", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pspirwaveRomInfo, pspirwaveRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Splatterhouse

static struct BurnRomInfo psplatthRomDesc[] = {
	{ "splatterhouse (japan).pce",	0x80000, 0x6b319457, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(psplatth)
STD_ROM_FN(psplatth)

struct BurnDriver BurnDrvPCE_psplatth = {
	"pce_splatth", NULL, NULL, NULL, "1990",
	"Splatterhouse\0", NULL, "Namco", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, psplatthRomInfo, psplatthRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Stratego

static struct BurnRomInfo pstrategoRomDesc[] = {
	{ "stratego (japan).pce",	0x40000, 0x727f4656, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pstratego)
STD_ROM_FN(pstratego)

struct BurnDriver BurnDrvPCE_pstratego = {
	"pce_stratego", NULL, NULL, NULL, "1992",
	"Stratego\0", NULL, "Victor Interactive Software", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pstrategoRomInfo, pstrategoRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Street Fighter II' - Champion Edition

static struct BurnRomInfo psf2ceRomDesc[] = {
	{ "street fighter ii' - champion edition (japan).pce",	0x280000, 0xd15cb6bb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(psf2ce)
STD_ROM_FN(psf2ce)

struct BurnDriver BurnDrvPCE_psf2ce = {
	"pce_sf2ce", NULL, NULL, NULL, "1993",
	"Street Fighter II' - Champion Edition\0", NULL, "NEC Home Electronics", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, psf2ceRomInfo, psf2ceRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Strip Fighter II

static struct BurnRomInfo pstripf2RomDesc[] = {
	{ "strip fighter ii (japan).pce",	0x100000, 0xd6fc51ce, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pstripf2)
STD_ROM_FN(pstripf2)

struct BurnDriver BurnDrvPCE_pstripf2 = {
	"pce_stripf2", NULL, NULL, NULL, "19??",
	"Strip Fighter II\0", NULL, "Unknown", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pstripf2RomInfo, pstripf2RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Super Metal Crusher

static struct BurnRomInfo psmcrushRomDesc[] = {
	{ "super metal crusher (japan).pce",	0x40000, 0x56488b36, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(psmcrush)
STD_ROM_FN(psmcrush)

struct BurnDriver BurnDrvPCE_psmcrush = {
	"pce_smcrush", NULL, NULL, NULL, "1991",
	"Super Metal Crusher\0", NULL, "Pack-In-Video", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, psmcrushRomInfo, psmcrushRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Super Momotaro Dentetsu

static struct BurnRomInfo psmomoRomDesc[] = {
	{ "super momotarou dentetsu (japan).pce",	0x60000, 0x3eb5304a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(psmomo)
STD_ROM_FN(psmomo)

struct BurnDriver BurnDrvPCE_psmomo = {
	"pce_smomo", NULL, NULL, NULL, "1989",
	"Super Momotaro Dentetsu\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, psmomoRomInfo, psmomoRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Super Momotaro Dentetsu II

static struct BurnRomInfo psmomo2RomDesc[] = {
	{ "super momotarou dentetsu ii (japan).pce",	0xc0000, 0x2bc023fc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(psmomo2)
STD_ROM_FN(psmomo2)

struct BurnDriver BurnDrvPCE_psmomo2 = {
	"pce_smomo2", NULL, NULL, NULL, "1991",
	"Super Momotaro Dentetsu II\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, psmomo2RomInfo, psmomo2RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Super Star Soldier

static struct BurnRomInfo psssoldrRomDesc[] = {
	{ "super star soldier (japan).pce",	0x80000, 0x5d0e3105, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(psssoldr)
STD_ROM_FN(psssoldr)

struct BurnDriver BurnDrvPCE_psssoldr = {
	"pce_sssoldr", NULL, NULL, NULL, "1990",
	"Super Star Soldier\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, psssoldrRomInfo, psssoldrRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Super Volleyball

static struct BurnRomInfo psvolleyRomDesc[] = {
	{ "super volleyball (japan).pce",	0x40000, 0xce2e4f9f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(psvolley)
STD_ROM_FN(psvolley)

struct BurnDriver BurnDrvPCE_psvolley = {
	"pce_svolley", NULL, NULL, NULL, "1990",
	"Super Volleyball\0", NULL, "Video System", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, psvolleyRomInfo, psvolleyRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Susanoo Densetsu

static struct BurnRomInfo psusanoRomDesc[] = {
	{ "susanoo densetsu (japan).pce",	0x80000, 0xcf73d8fc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(psusano)
STD_ROM_FN(psusano)

struct BurnDriver BurnDrvPCE_psusano = {
	"pce_susano", NULL, NULL, NULL, "1989",
	"Susanoo Densetsu\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, psusanoRomInfo, psusanoRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Taito Chase H.Q.

static struct BurnRomInfo pchasehqRomDesc[] = {
	{ "taito chase h.q. (japan).pce",	0x60000, 0x6f4fd790, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pchasehq)
STD_ROM_FN(pchasehq)

struct BurnDriver BurnDrvPCE_pchasehq = {
	"pce_chasehq", NULL, NULL, NULL, "1990",
	"Taito Chase H.Q.\0", NULL, "Taito", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pchasehqRomInfo, pchasehqRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Takahashi Meijin no Shin Boukenjima

static struct BurnRomInfo ptakameibRomDesc[] = {
	{ "takahashi meijin no shin boukenjima (japan).pce",	0x80000, 0xe415ea19, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ptakameib)
STD_ROM_FN(ptakameib)

struct BurnDriver BurnDrvPCE_ptakameib = {
	"pce_takameib", NULL, NULL, NULL, "1992",
	"Takahashi Meijin no Shin Boukenjima\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ptakameibRomInfo, ptakameibRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Takeda Shingen

static struct BurnRomInfo ptakedaRomDesc[] = {
	{ "takeda shingen (japan).pce",	0x40000, 0xf022be13, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ptakeda)
STD_ROM_FN(ptakeda)

struct BurnDriver BurnDrvPCE_ptakeda = {
	"pce_takeda", NULL, NULL, NULL, "1989",
	"Takeda Shingen\0", NULL, "Aicom", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ptakedaRomInfo, ptakedaRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Takeda Shingen (Alt)

static struct BurnRomInfo ptakeda1RomDesc[] = {
	{ "takeda shingen (japan) [a].pce",	0x40000, 0xdf7af71c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ptakeda1)
STD_ROM_FN(ptakeda1)

struct BurnDriver BurnDrvPCE_ptakeda1 = {
	"pce_takeda1", "pce_takeda", NULL, NULL, "1989",
	"Takeda Shingen (Alt)\0", NULL, "Aicom", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ptakeda1RomInfo, ptakeda1RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Tatsujin

static struct BurnRomInfo ptatsujinRomDesc[] = {
	{ "tatsujin (japan).pce",	0x80000, 0xa6088275, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ptatsujin)
STD_ROM_FN(ptatsujin)

struct BurnDriver BurnDrvPCE_ptatsujin = {
	"pce_tatsujin", NULL, NULL, NULL, "1992",
	"Tatsujin\0", NULL, "Taito", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ptatsujinRomInfo, ptatsujinRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Tatsujin (Prototype)

static struct BurnRomInfo ptatsujinpRomDesc[] = {
	{ "tatsujin (japan) (proto).pce",	0x80000, 0xc1b26659, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ptatsujinp)
STD_ROM_FN(ptatsujinp)

struct BurnDriver BurnDrvPCE_ptatsujinp = {
	"pce_tatsujinp", "pce_tatsujin", NULL, NULL, "1992",
	"Tatsujin (Prototype)\0", NULL, "Taito", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ptatsujinpRomInfo, ptatsujinpRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Tatsunoko Fighter

static struct BurnRomInfo ptatsunokRomDesc[] = {
	{ "tatsunoko fighter (japan).pce",	0x40000, 0xeeb6dd43, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ptatsunok)
STD_ROM_FN(ptatsunok)

struct BurnDriver BurnDrvPCE_ptatsunok = {
	"pce_tatsunok", NULL, NULL, NULL, "1989",
	"Tatsunoko Fighter\0", NULL, "Tonkin House", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ptatsunokRomInfo, ptatsunokRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Tennokoe Bank

static struct BurnRomInfo ptennokoeRomDesc[] = {
	{ "tennokoe bank (japan).pce",	0x20000, 0x3b3808bd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ptennokoe)
STD_ROM_FN(ptennokoe)

struct BurnDriverD BurnDrvPCE_ptennokoe = {
	"pce_tennokoe", NULL, NULL, NULL, "1991",
	"Tennokoe Bank\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	0, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ptennokoeRomInfo, ptennokoeRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Tenseiryuu - Saint Dragon

static struct BurnRomInfo psdragonRomDesc[] = {
	{ "tenseiryuu - saint dragon (japan).pce",	0x60000, 0x2e278ccb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(psdragon)
STD_ROM_FN(psdragon)

struct BurnDriver BurnDrvPCE_psdragon = {
	"pce_sdragon", NULL, NULL, NULL, "1990",
	"Tenseiryuu - Saint Dragon\0", NULL, "Irem", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, psdragonRomInfo, psdragonRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Terra Cresta II - Mandoraa no Gyakushuu

static struct BurnRomInfo ptcresta2RomDesc[] = {
	{ "terra cresta ii - mandoraa no gyakushuu (japan).pce",	0x80000, 0x1b2d0077, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ptcresta2)
STD_ROM_FN(ptcresta2)

struct BurnDriver BurnDrvPCE_ptcresta2 = {
	"pce_tcresta2", NULL, NULL, NULL, "1992",
	"Terra Cresta II - Mandoraa no Gyakushuu\0", NULL, "Nihon Bussan", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ptcresta2RomInfo, ptcresta2RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Thunder Blade

static struct BurnRomInfo ptbladeRomDesc[] = {
	{ "thunder blade (japan).pce",	0x80000, 0xddc3e809, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ptblade)
STD_ROM_FN(ptblade)

struct BurnDriver BurnDrvPCE_ptblade = {
	"pce_tblade", NULL, NULL, NULL, "1990",
	"Thunder Blade\0", NULL, "NEC Avenue", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ptbladeRomInfo, ptbladeRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Time Cruise II

static struct BurnRomInfo ptimcrus2RomDesc[] = {
	{ "time cruise ii (japan).pce",	0x80000, 0xcfec1d6a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ptimcrus2)
STD_ROM_FN(ptimcrus2)

struct BurnDriver BurnDrvPCE_ptimcrus2 = {
	"pce_timcrus2", NULL, NULL, NULL, "1991",
	"Time Cruise II\0", NULL, "Face", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ptimcrus2RomInfo, ptimcrus2RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Titan

static struct BurnRomInfo ptitanRomDesc[] = {
	{ "titan (japan).pce",	0x40000, 0xd20f382f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ptitan)
STD_ROM_FN(ptitan)

struct BurnDriver BurnDrvPCE_ptitan = {
	"pce_titan", NULL, NULL, NULL, "1991",
	"Titan\0", NULL, "Naxat", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ptitanRomInfo, ptitanRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Toilet Kids

static struct BurnRomInfo ptoiletkRomDesc[] = {
	{ "toilet kids (japan).pce",	0x80000, 0x53b7784b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ptoiletk)
STD_ROM_FN(ptoiletk)

struct BurnDriver BurnDrvPCE_ptoiletk = {
	"pce_toiletk", NULL, NULL, NULL, "1992",
	"Toilet Kids\0", NULL, "Media Rings", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ptoiletkRomInfo, ptoiletkRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Tora Heno Michi

static struct BurnRomInfo ptorahenoRomDesc[] = {
	{ "tora heno michi (japan).pce",	0x60000, 0x82ae3b16, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ptoraheno)
STD_ROM_FN(ptoraheno)

struct BurnDriver BurnDrvPCE_ptoraheno = {
	"pce_toraheno", NULL, NULL, NULL, "1990",
	"Tora Heno Michi\0", NULL, "Victor Entertainment", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ptorahenoRomInfo, ptorahenoRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Toshi Tensou Keikaku - Eternal City

static struct BurnRomInfo petercityRomDesc[] = {
	{ "toshi tensou keikaku - eternal city (japan).pce",	0x60000, 0xb18d102d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(petercity)
STD_ROM_FN(petercity)

struct BurnDriver BurnDrvPCE_petercity = {
	"pce_etercity", NULL, NULL, NULL, "1991",
	"Toshi Tensou Keikaku - Eternal City\0", NULL, "Naxat", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, petercityRomInfo, petercityRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// The Tower of Druaga

static struct BurnRomInfo pdruagaRomDesc[] = {
	{ "tower of druaga, the (japan).pce",	0x80000, 0x72e00bc4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pdruaga)
STD_ROM_FN(pdruaga)

struct BurnDriver BurnDrvPCE_pdruaga = {
	"pce_druaga", NULL, NULL, NULL, "19??",
	"The Tower of Druaga\0", NULL, "Unknown", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pdruagaRomInfo, pdruagaRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Toy Shop Boys

static struct BurnRomInfo ptoyshopbRomDesc[] = {
	{ "toy shop boys (japan).pce",	0x40000, 0x97c5ee9a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ptoyshopb)
STD_ROM_FN(ptoyshopb)

struct BurnDriver BurnDrvPCE_ptoyshopb = {
	"pce_toyshopb", NULL, NULL, NULL, "1990",
	"Toy Shop Boys\0", NULL, "Victor", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ptoyshopbRomInfo, ptoyshopbRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Tricky

static struct BurnRomInfo ptrickyRomDesc[] = {
	{ "tricky (japan).pce",	0x40000, 0x3aea2f8f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ptricky)
STD_ROM_FN(ptricky)

struct BurnDriver BurnDrvPCE_ptricky = {
	"pce_tricky", NULL, NULL, NULL, "1991",
	"Tricky\0", NULL, "IGS", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ptrickyRomInfo, ptrickyRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Tsuppari Oozumou - Heisei Ban

static struct BurnRomInfo ptsupozumRomDesc[] = {
	{ "tsuppari oozumou - heisei ban (japan).pce",	0x40000, 0x61a6e210, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ptsupozum)
STD_ROM_FN(ptsupozum)

struct BurnDriver BurnDrvPCE_ptsupozum = {
	"pce_tsupozum", NULL, NULL, NULL, "19??",
	"Tsuppari Oozumou - Heisei Ban\0", NULL, "Naxat", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ptsupozumRomInfo, ptsupozumRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Tsuru Teruhito no Jissen Kabushiki Bai Bai Game

static struct BurnRomInfo pbaibaiRomDesc[] = {
	{ "tsuru teruhito no jissen kabushiki bai bai game (japan).pce",	0x40000, 0xf70112e5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pbaibai)
STD_ROM_FN(pbaibai)

struct BurnDriver BurnDrvPCE_pbaibai = {
	"pce_baibai", NULL, NULL, NULL, "1989",
	"Tsuru Teruhito no Jissen Kabushiki Bai Bai Game\0", NULL, "Intec", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pbaibaiRomInfo, pbaibaiRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// TV Sports Basketball

static struct BurnRomInfo ptvbasketRomDesc[] = {
	{ "tv sports basketball (japan).pce",	0x80000, 0x10b60601, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ptvbasket)
STD_ROM_FN(ptvbasket)

struct BurnDriver BurnDrvPCE_ptvbasket = {
	"pce_tvbasket", NULL, NULL, NULL, "1993",
	"TV Sports Basketball\0", NULL, "Victor Interactive Software", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ptvbasketRomInfo, ptvbasketRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// TV Sports Football

static struct BurnRomInfo ptvfootblRomDesc[] = {
	{ "tv sports football (japan).pce",	0x60000, 0x968d908a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ptvfootbl)
STD_ROM_FN(ptvfootbl)

struct BurnDriver BurnDrvPCE_ptvfootbl = {
	"pce_tvfootbl", NULL, NULL, NULL, "1991",
	"TV Sports Football\0", NULL, "Victor Interactive Software", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ptvfootblRomInfo, ptvfootblRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// TV Sports Hockey

static struct BurnRomInfo ptvhockeyRomDesc[] = {
	{ "tv sports hockey (japan).pce",	0x60000, 0xe7529890, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ptvhockey)
STD_ROM_FN(ptvhockey)

struct BurnDriver BurnDrvPCE_ptvhockey = {
	"pce_tvhockey", NULL, NULL, NULL, "1993",
	"TV Sports Hockey\0", NULL, "Victor Interactive Software", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, ptvhockeyRomInfo, ptvhockeyRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// USA Pro Basketball

static struct BurnRomInfo pusaprobsRomDesc[] = {
	{ "usa pro basketball (japan).pce",	0x40000, 0x1cad4b7f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pusaprobs)
STD_ROM_FN(pusaprobs)

struct BurnDriver BurnDrvPCE_pusaprobs = {
	"pce_usaprobs", NULL, NULL, NULL, "1989",
	"USA Pro Basketball\0", NULL, "Aicom", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pusaprobsRomInfo, pusaprobsRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Veigues - Tactical Gladiator

static struct BurnRomInfo pveiguesRomDesc[] = {
	{ "veigues - tactical gladiator (japan).pce",	0x60000, 0x04188c5c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pveigues)
STD_ROM_FN(pveigues)

struct BurnDriver BurnDrvPCE_pveigues = {
	"pce_veigues", NULL, NULL, NULL, "1990",
	"Veigues - Tactical Gladiator\0", NULL, "Victor Entertainment", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pveiguesRomInfo, pveiguesRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Victory Run

static struct BurnRomInfo pvictoryrRomDesc[] = {
	{ "victory run (japan).pce",	0x40000, 0x03e28cff, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pvictoryr)
STD_ROM_FN(pvictoryr)

struct BurnDriver BurnDrvPCE_pvictoryr = {
	"pce_victoryr", NULL, NULL, NULL, "1987",
	"Victory Run\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pvictoryrRomInfo, pvictoryrRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Vigilante

static struct BurnRomInfo pvigilantRomDesc[] = {
	{ "vigilante (japan).pce",	0x60000, 0xe4124fe0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pvigilant)
STD_ROM_FN(pvigilant)

struct BurnDriver BurnDrvPCE_pvigilant = {
	"pce_vigilant", NULL, NULL, NULL, "1989",
	"Vigilante\0", NULL, "Irem", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pvigilantRomInfo, pvigilantRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Violent Soldier

static struct BurnRomInfo pviolentsRomDesc[] = {
	{ "violent soldier (japan).pce",	0x60000, 0x1bc36b36, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pviolents)
STD_ROM_FN(pviolents)

struct BurnDriver BurnDrvPCE_pviolents = {
	"pce_violents", NULL, NULL, NULL, "1990",
	"Violent Soldier\0", NULL, "IGS", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pviolentsRomInfo, pviolentsRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Volfied

static struct BurnRomInfo pvolfiedRomDesc[] = {
	{ "volfied (japan).pce",	0x60000, 0xad226f30, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pvolfied)
STD_ROM_FN(pvolfied)

struct BurnDriver BurnDrvPCE_pvolfied = {
	"pce_volfied", NULL, NULL, NULL, "1989",
	"Volfied\0", NULL, "Taito", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pvolfiedRomInfo, pvolfiedRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// W-ring - The Double Rings

static struct BurnRomInfo pwringRomDesc[] = {
	{ "w-ring - the double rings (japan).pce",	0x60000, 0xbe990010, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pwring)
STD_ROM_FN(pwring)

struct BurnDriver BurnDrvPCE_pwring = {
	"pce_wring", NULL, NULL, NULL, "1990",
	"W-ring - The Double Rings\0", NULL, "Naxat", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pwringRomInfo, pwringRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Wai Wai Mahjong - Yukaina Janyuu Tachi

static struct BurnRomInfo pwaiwaimjRomDesc[] = {
	{ "wai wai mahjong - yukaina janyuu tachi (japan).pce",	0x40000, 0xa2a0776e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pwaiwaimj)
STD_ROM_FN(pwaiwaimj)

struct BurnDriver BurnDrvPCE_pwaiwaimj = {
	"pce_waiwaimj", NULL, NULL, NULL, "1989",
	"Wai Wai Mahjong - Yukaina Janyuu Tachi\0", NULL, "Video System", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pwaiwaimjRomInfo, pwaiwaimjRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Walkuere no Densetsu

static struct BurnRomInfo pvalkyrieRomDesc[] = {
	{ "walkuere no densetsu (japan).pce",	0x80000, 0xa3303978, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pvalkyrie)
STD_ROM_FN(pvalkyrie)

struct BurnDriver BurnDrvPCE_pvalkyrie = {
	"pce_valkyrie", NULL, NULL, NULL, "1990",
	"Walkuere no Densetsu\0", NULL, "Namco", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pvalkyrieRomInfo, pvalkyrieRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Wallaby!! - Usagi no Kuni no Kangaroo Race

static struct BurnRomInfo pwallabyRomDesc[] = {
	{ "wallaby!! - usagi no kuni no kangaroo race (japan).pce",	0x60000, 0x0112d0c7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pwallaby)
STD_ROM_FN(pwallaby)

struct BurnDriver BurnDrvPCE_pwallaby = {
	"pce_wallaby", NULL, NULL, NULL, "1990",
	"Wallaby!! - Usagi no Kuni no Kangaroo Race\0", NULL, "Masiya", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pwallabyRomInfo, pwallabyRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Winning Shot

static struct BurnRomInfo pwinshotRomDesc[] = {
	{ "winning shot (japan).pce",	0x40000, 0x9b5ebc58, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pwinshot)
STD_ROM_FN(pwinshot)

struct BurnDriver BurnDrvPCE_pwinshot = {
	"pce_winshot", NULL, NULL, NULL, "1989",
	"Winning Shot\0", NULL, "Data East", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pwinshotRomInfo, pwinshotRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Wonder Momo

static struct BurnRomInfo pwondermRomDesc[] = {
	{ "wonder momo (japan).pce",	0x40000, 0x59d07314, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pwonderm)
STD_ROM_FN(pwonderm)

struct BurnDriver BurnDrvPCE_pwonderm = {
	"pce_wonderm", NULL, NULL, NULL, "1989",
	"Wonder Momo\0", NULL, "Namco", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pwondermRomInfo, pwondermRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// World Beach Volley

static struct BurnRomInfo pwbeachRomDesc[] = {
	{ "world beach volley (japan).pce",	0x40000, 0xbe850530, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pwbeach)
STD_ROM_FN(pwbeach)

struct BurnDriver BurnDrvPCE_pwbeach = {
	"pce_wbeach", NULL, NULL, NULL, "1990",
	"World Beach Volley\0", NULL, "IGS", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pwbeachRomInfo, pwbeachRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// World Circuit

static struct BurnRomInfo pwcircuitRomDesc[] = {
	{ "world circuit (japan).pce",	0x40000, 0xb3eeea2e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pwcircuit)
STD_ROM_FN(pwcircuit)

struct BurnDriver BurnDrvPCE_pwcircuit = {
	"pce_wcircuit", NULL, NULL, NULL, "1991",
	"World Circuit\0", NULL, "Namco", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pwcircuitRomInfo, pwcircuitRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// World Jockey

static struct BurnRomInfo pwjockeyRomDesc[] = {
	{ "world jockey (japan).pce",	0x40000, 0xa9ab2954, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pwjockey)
STD_ROM_FN(pwjockey)

struct BurnDriver BurnDrvPCE_pwjockey = {
	"pce_wjockey", NULL, NULL, NULL, "1991",
	"World Jockey\0", NULL, "Namco", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pwjockeyRomInfo, pwjockeyRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Xevious - Fardraut Densetsu

static struct BurnRomInfo pxeviousRomDesc[] = {
	{ "xevious - fardraut densetsu (japan).pce",	0x40000, 0xf8f85eec, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pxevious)
STD_ROM_FN(pxevious)

struct BurnDriver BurnDrvPCE_pxevious = {
	"pce_xevious", NULL, NULL, NULL, "1990",
	"Xevious - Fardraut Densetsu\0", NULL, "Namco", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pxeviousRomInfo, pxeviousRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Youkai Douchuuki

static struct BurnRomInfo pyukaidoRomDesc[] = {
	{ "youkai douchuuki (japan).pce",	0x40000, 0xf131b706, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pyukaido)
STD_ROM_FN(pyukaido)

struct BurnDriver BurnDrvPCE_pyukaido = {
	"pce_yukaido", NULL, NULL, NULL, "1988",
	"Youkai Douchuuki\0", NULL, "Namco", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pyukaidoRomInfo, pyukaidoRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Yuu Yuu Jinsei - Victory Life

static struct BurnRomInfo pyuyuRomDesc[] = {
	{ "yuu yuu jinsei - victory life (japan).pce",	0x40000, 0xc0905ca9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pyuyu)
STD_ROM_FN(pyuyu)

struct BurnDriver BurnDrvPCE_pyuyu = {
	"pce_yuyu", NULL, NULL, NULL, "1988",
	"Yuu Yuu Jinsei - Victory Life\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pyuyuRomInfo, pyuyuRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Zero 4 Champ

static struct BurnRomInfo pzero4caRomDesc[] = {
	{ "zero 4 champ (japan).pce",	0x80000, 0xee156721, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pzero4ca)
STD_ROM_FN(pzero4ca)

struct BurnDriver BurnDrvPCE_pzero4ca = {
	"pce_zero4ca", "pce_zero4c", NULL, NULL, "1991",
	"Zero 4 Champ\0", NULL, "Media Rings", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pzero4caRomInfo, pzero4caRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Zero 4 Champ (v1.5)

static struct BurnRomInfo pzero4cRomDesc[] = {
	{ "zero 4 champ (japan) (v1.5).pce",	0x80000, 0xb77f2e2f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pzero4c)
STD_ROM_FN(pzero4c)

struct BurnDriver BurnDrvPCE_pzero4c = {
	"pce_zero4c", NULL, NULL, NULL, "1991",
	"Zero 4 Champ (v1.5)\0", NULL, "Media Rings", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pzero4cRomInfo, pzero4cRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Zipang

static struct BurnRomInfo pzipangRomDesc[] = {
	{ "zipang (japan).pce",	0x40000, 0x67aab7a1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pzipang)
STD_ROM_FN(pzipang)

struct BurnDriver BurnDrvPCE_pzipang = {
	"pce_zipang", NULL, NULL, NULL, "1990",
	"Zipang\0", NULL, "Pack-In-Video", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pzipangRomInfo, pzipangRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// CD-Rom System Card (v1.0)

static struct BurnRomInfo pcdsysbRomDesc[] = {
	{ "[cd] cd-rom system (japan) (v1.0).pce",	0x40000, 0x3f9f95a4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pcdsysb)
STD_ROM_FN(pcdsysb)

struct BurnDriverD BurnDrvPCE_pcdsysb = {
	"pce_cdsysb", "pce_cdsys", NULL, NULL, "19??",
	"CD-Rom System Card (v1.0)\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pcdsysbRomInfo, pcdsysbRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// CD-Rom System Card (v2.0)

static struct BurnRomInfo pcdsysaRomDesc[] = {
	{ "[cd] cd-rom system (japan) (v2.0).pce",	0x40000, 0x52520bc6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pcdsysa)
STD_ROM_FN(pcdsysa)

struct BurnDriverD BurnDrvPCE_pcdsysa = {
	"pce_cdsysa", "pce_cdsys", NULL, NULL, "19??",
	"CD-Rom System Card (v2.0)\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pcdsysaRomInfo, pcdsysaRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// CD-Rom System Card (v2.1)

static struct BurnRomInfo pcdsysRomDesc[] = {
	{ "[cd] cd-rom system (japan) (v2.1).pce",	0x40000, 0x283b74e0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pcdsys)
STD_ROM_FN(pcdsys)

struct BurnDriverD BurnDrvPCE_pcdsys = {
	"pce_cdsys", NULL, NULL, NULL, "19??",
	"CD-Rom System Card (v2.1)\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pcdsysRomInfo, pcdsysRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Games Express CD Card

static struct BurnRomInfo pgecdRomDesc[] = {
	{ "[cd] games express cd card (japan).pce",	0x08000, 0x51a12d90, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pgecd)
STD_ROM_FN(pgecd)

struct BurnDriverD BurnDrvPCE_pgecd = {
	"pce_gecd", NULL, NULL, NULL, "19??",
	"Games Express CD Card\0", NULL, "Games Express", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pgecdRomInfo, pgecdRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Super CD-Rom System Card (v3.0)

static struct BurnRomInfo pscdsysRomDesc[] = {
	{ "[cd] super cd-rom system (japan) (v3.0).pce",	0x40000, 0x6d9a73ef, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pscdsys)
STD_ROM_FN(pscdsys)

struct BurnDriverD BurnDrvPCE_pscdsys = {
	"pce_scdsys", NULL, NULL, NULL, "19??",
	"Super CD-Rom System Card (v3.0)\0", NULL, "Hudson", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pscdsysRomInfo, pscdsysRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Aero Blasters

static struct BurnRomInfo taeroblstRomDesc[] = {
	{ "aero blasters (usa).pce",	0x80000, 0xb03e0b32, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(taeroblst)
STD_ROM_FN(taeroblst)

struct BurnDriver BurnDrvTG_taeroblst = {
	"tg_aeroblst", NULL, NULL, NULL, "1990",
	"Aero Blasters\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, taeroblstRomInfo, taeroblstRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Air Zonk

static struct BurnRomInfo tairzonkRomDesc[] = {
	{ "air zonk (usa).pce",	0x80000, 0x933d5bcc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tairzonk)
STD_ROM_FN(tairzonk)

struct BurnDriverD BurnDrvTG_tairzonk = {
	"tg_airzonk", NULL, NULL, NULL, "1992",
	"Air Zonk\0", NULL, "Hudson Soft", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	0, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tairzonkRomInfo, tairzonkRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Alien Crush

static struct BurnRomInfo tacrushRomDesc[] = {
	{ "alien crush (usa).pce",	0x40000, 0xea488494, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tacrush)
STD_ROM_FN(tacrush)

struct BurnDriver BurnDrvTG_tacrush = {
	"tg_acrush", NULL, NULL, NULL, "1989",
	"Alien Crush\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tacrushRomInfo, tacrushRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Ballistix

static struct BurnRomInfo tballistxRomDesc[] = {
	{ "ballistix (usa).pce",	0x40000, 0x420fa189, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tballistx)
STD_ROM_FN(tballistx)

struct BurnDriver BurnDrvTG_tballistx = {
	"tg_ballistx", NULL, NULL, NULL, "1992",
	"Ballistix\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tballistxRomInfo, tballistxRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Battle Royale

static struct BurnRomInfo tbatlroylRomDesc[] = {
	{ "battle royale (usa).pce",	0x80000, 0xe70b01af, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tbatlroyl)
STD_ROM_FN(tbatlroyl)

struct BurnDriver BurnDrvTG_tbatlroyl = {
	"tg_batlroyl", NULL, NULL, NULL, "1990",
	"Battle Royale\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tbatlroylRomInfo, tbatlroylRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Blazing Lazers

static struct BurnRomInfo tblazlazrRomDesc[] = {
	{ "blazing lazers (usa).pce",	0x60000, 0xb4a1b0f6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tblazlazr)
STD_ROM_FN(tblazlazr)

struct BurnDriver BurnDrvTG_tblazlazr = {
	"tg_blazlazr", NULL, NULL, NULL, "1989",
	"Blazing Lazers\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tblazlazrRomInfo, tblazlazrRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Bloody Wolf

static struct BurnRomInfo tblodwolfRomDesc[] = {
	{ "bloody wolf (usa).pce",	0x80000, 0x37baf6bc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tblodwolf)
STD_ROM_FN(tblodwolf)

struct BurnDriver BurnDrvTG_tblodwolf = {
	"tg_blodwolf", NULL, NULL, NULL, "1990",
	"Bloody Wolf\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tblodwolfRomInfo, tblodwolfRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Bomberman '93

static struct BurnRomInfo tbombmn93RomDesc[] = {
	{ "bomberman '93 (usa).pce",	0x80000, 0x56171c1c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tbombmn93)
STD_ROM_FN(tbombmn93)

struct BurnDriver BurnDrvTG_tbombmn93 = {
	"tg_bombmn93", NULL, NULL, NULL, "1990",
	"Bomberman '93\0", NULL, "Hudson Soft", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tbombmn93RomInfo, tbombmn93RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Bomberman

static struct BurnRomInfo tbombmanRomDesc[] = {
	{ "bomberman (usa).pce",	0x40000, 0x5f6f3c2a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tbombman)
STD_ROM_FN(tbombman)

struct BurnDriver BurnDrvTG_tbombman = {
	"tg_bombman", NULL, NULL, NULL, "1991",
	"Bomberman\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tbombmanRomInfo, tbombmanRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Bonk III - Bonk's Big Adventure

static struct BurnRomInfo tbonk3RomDesc[] = {
	{ "bonk iii - bonk's big adventure (usa).pce",	0x100000, 0x5a3f76d8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tbonk3)
STD_ROM_FN(tbonk3)

struct BurnDriver BurnDrvTG_tbonk3 = {
	"tg_bonk3", NULL, NULL, NULL, "1993",
	"Bonk III - Bonk's Big Adventure\0", NULL, "TTI", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tbonk3RomInfo, tbonk3RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Bonk's Adventure

static struct BurnRomInfo tbonkRomDesc[] = {
	{ "bonk's adventure (usa).pce",	0x60000, 0x599ead9b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tbonk)
STD_ROM_FN(tbonk)

struct BurnDriver BurnDrvTG_tbonk = {
	"tg_bonk", NULL, NULL, NULL, "1990",
	"Bonk's Adventure\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tbonkRomInfo, tbonkRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Bonk's Revenge

static struct BurnRomInfo tbonk2RomDesc[] = {
	{ "bonk's revenge (usa).pce",	0x80000, 0x14250f9a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tbonk2)
STD_ROM_FN(tbonk2)

struct BurnDriver BurnDrvTG_tbonk2 = {
	"tg_bonk2", NULL, NULL, NULL, "1991",
	"Bonk's Revenge\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tbonk2RomInfo, tbonk2RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Boxyboy

static struct BurnRomInfo tboxyboyRomDesc[] = {
	{ "boxyboy (usa).pce",	0x20000, 0x605be213, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tboxyboy)
STD_ROM_FN(tboxyboy)

struct BurnDriver BurnDrvTG_tboxyboy = {
	"tg_boxyboy", NULL, NULL, NULL, "1990",
	"Boxyboy\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tboxyboyRomInfo, tboxyboyRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Bravoman

static struct BurnRomInfo tbravomanRomDesc[] = {
	{ "bravoman (usa).pce",	0x80000, 0xcca08b02, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tbravoman)
STD_ROM_FN(tbravoman)

struct BurnDriver BurnDrvTG_tbravoman = {
	"tg_bravoman", NULL, NULL, NULL, "1990",
	"Bravoman\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tbravomanRomInfo, tbravomanRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Cadash

static struct BurnRomInfo tcadashRomDesc[] = {
	{ "cadash (usa).pce",	0x80000, 0xbb0b3aef, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tcadash)
STD_ROM_FN(tcadash)

struct BurnDriver BurnDrvTG_tcadash = {
	"tg_cadash", NULL, NULL, NULL, "1991",
	"Cadash\0", "Bad graphics", "Working Designs", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tcadashRomInfo, tcadashRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Champions Forever Boxing

static struct BurnRomInfo tforevboxRomDesc[] = {
	{ "champions forever boxing (usa).pce",	0x80000, 0x15ee889a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tforevbox)
STD_ROM_FN(tforevbox)

struct BurnDriver BurnDrvTG_tforevbox = {
	"tg_forevbox", NULL, NULL, NULL, "1991",
	"Champions Forever Boxing\0", "Bad sound", "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tforevboxRomInfo, tforevboxRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Chew Man Fu

static struct BurnRomInfo tchewmanRomDesc[] = {
	{ "chew man fu (usa).pce",	0x40000, 0x8cd13e9a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tchewman)
STD_ROM_FN(tchewman)

struct BurnDriver BurnDrvTG_tchewman = {
	"tg_chewman", NULL, NULL, NULL, "1990",
	"Chew Man Fu\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tchewmanRomInfo, tchewmanRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// China Warrior

static struct BurnRomInfo tchinawarRomDesc[] = {
	{ "china warrior (usa).pce",	0x40000, 0xa2ee361d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tchinawar)
STD_ROM_FN(tchinawar)

struct BurnDriver BurnDrvTG_tchinawar = {
	"tg_chinawar", NULL, NULL, NULL, "1989",
	"China Warrior\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tchinawarRomInfo, tchinawarRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Cratermaze

static struct BurnRomInfo tcratermzRomDesc[] = {
	{ "cratermaze (usa).pce",	0x40000, 0x9033e83a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tcratermz)
STD_ROM_FN(tcratermz)

struct BurnDriver BurnDrvTG_tcratermz = {
	"tg_cratermz", NULL, NULL, NULL, "1990",
	"Cratermaze\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tcratermzRomInfo, tcratermzRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Cyber Core

static struct BurnRomInfo tcybrcoreRomDesc[] = {
	{ "cyber core (usa).pce",	0x60000, 0x4cfb6e3e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tcybrcore)
STD_ROM_FN(tcybrcore)

struct BurnDriver BurnDrvTG_tcybrcore = {
	"tg_cybrcore", NULL, NULL, NULL, "1990",
	"Cyber Core\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tcybrcoreRomInfo, tcybrcoreRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Darkwing Duck

static struct BurnRomInfo tdarkwingRomDesc[] = {
	{ "darkwing duck (usa).pce",	0x80000, 0x4ac97606, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tdarkwing)
STD_ROM_FN(tdarkwing)

struct BurnDriver BurnDrvTG_tdarkwing = {
	"tg_darkwing", NULL, NULL, NULL, "1992",
	"Darkwing Duck\0", NULL, "TTI", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tdarkwingRomInfo, tdarkwingRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Davis Cup Tennis

static struct BurnRomInfo tdaviscupRomDesc[] = {
	{ "davis cup tennis (usa).pce",	0x80000, 0x9edab596, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tdaviscup)
STD_ROM_FN(tdaviscup)

struct BurnDriver BurnDrvTG_tdaviscup = {
	"tg_daviscup", NULL, NULL, NULL, "1991",
	"Davis Cup Tennis\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tdaviscupRomInfo, tdaviscupRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Dead Moon

static struct BurnRomInfo tdeadmoonRomDesc[] = {
	{ "dead moon (usa).pce",	0x80000, 0xf5d98b0b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tdeadmoon)
STD_ROM_FN(tdeadmoon)

struct BurnDriver BurnDrvTG_tdeadmoon = {
	"tg_deadmoon", NULL, NULL, NULL, "1992",
	"Dead Moon\0", NULL, "TTI", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tdeadmoonRomInfo, tdeadmoonRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Deep Blue

static struct BurnRomInfo tdeepblueRomDesc[] = {
	{ "deep blue (usa).pce",	0x40000, 0x16b40b44, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tdeepblue)
STD_ROM_FN(tdeepblue)

struct BurnDriver BurnDrvTG_tdeepblue = {
	"tg_deepblue", NULL, NULL, NULL, "1989",
	"Deep Blue\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tdeepblueRomInfo, tdeepblueRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Devil's Crush - Naxat Pinball

static struct BurnRomInfo tdevlcrshRomDesc[] = {
	{ "devil's crush - naxat pinball (usa).pce",	0x60000, 0x157b4492, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tdevlcrsh)
STD_ROM_FN(tdevlcrsh)

struct BurnDriver BurnDrvTG_tdevlcrsh = {
	"tg_devlcrsh", NULL, NULL, NULL, "1990",
	"Devil's Crush - Naxat Pinball\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tdevlcrshRomInfo, tdevlcrshRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Double Dungeons - W

static struct BurnRomInfo tddungwRomDesc[] = {
	{ "double dungeons - w (usa).pce",	0x40000, 0x4a1a8c60, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tddungw)
STD_ROM_FN(tddungw)

struct BurnDriver BurnDrvTG_tddungw = {
	"tg_ddungw", NULL, NULL, NULL, "1990",
	"Double Dungeons - W\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tddungwRomInfo, tddungwRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Dragon Spirit

static struct BurnRomInfo tdspiritRomDesc[] = {
	{ "dragon spirit (usa).pce",	0x40000, 0x086f148c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tdspirit)
STD_ROM_FN(tdspirit)

struct BurnDriver BurnDrvTG_tdspirit = {
	"tg_dspirit", NULL, NULL, NULL, "1989",
	"Dragon Spirit\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tdspiritRomInfo, tdspiritRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Dragon's Curse

static struct BurnRomInfo tdragcrseRomDesc[] = {
	{ "dragon's curse (usa).pce",	0x40000, 0x7d2c4b09, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tdragcrse)
STD_ROM_FN(tdragcrse)

struct BurnDriver BurnDrvTG_tdragcrse = {
	"tg_dragcrse", NULL, NULL, NULL, "1990",
	"Dragon's Curse\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tdragcrseRomInfo, tdragcrseRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Drop.Off

static struct BurnRomInfo tdropoffRomDesc[] = {
	{ "drop.off (usa).pce",	0x40000, 0xfea27b32, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tdropoff)
STD_ROM_FN(tdropoff)

struct BurnDriver BurnDrvTG_tdropoff = {
	"tg_dropoff", NULL, NULL, NULL, "1990",
	"Drop.Off\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tdropoffRomInfo, tdropoffRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Dungeon Explorer

static struct BurnRomInfo tdungexplRomDesc[] = {
	{ "dungeon explorer (usa).pce",	0x60000, 0x4ff01515, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tdungexpl)
STD_ROM_FN(tdungexpl)

struct BurnDriver BurnDrvTG_tdungexpl = {
	"tg_dungexpl", NULL, NULL, NULL, "1989",
	"Dungeon Explorer\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tdungexplRomInfo, tdungexplRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Falcon

static struct BurnRomInfo tfalconRomDesc[] = {
	{ "falcon (usa).pce",	0x80000, 0x0bc0a12b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tfalcon)
STD_ROM_FN(tfalcon)

struct BurnDriver BurnDrvTG_tfalcon = {
	"tg_falcon", NULL, NULL, NULL, "1992",
	"Falcon\0", NULL, "TTI", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tfalconRomInfo, tfalconRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Fantasy Zone

static struct BurnRomInfo tfantzoneRomDesc[] = {
	{ "fantasy zone (usa).pce",	0x40000, 0xe8c3573d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tfantzone)
STD_ROM_FN(tfantzone)

struct BurnDriver BurnDrvTG_tfantzone = {
	"tg_fantzone", NULL, NULL, NULL, "1989",
	"Fantasy Zone\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tfantzoneRomInfo, tfantzoneRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Final Lap Twin

static struct BurnRomInfo tfinallapRomDesc[] = {
	{ "final lap twin (usa).pce",	0x60000, 0x26408ea3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tfinallap)
STD_ROM_FN(tfinallap)

struct BurnDriver BurnDrvTG_tfinallap = {
	"tg_finallap", NULL, NULL, NULL, "1990",
	"Final Lap Twin\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tfinallapRomInfo, tfinallapRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Galaga '90

static struct BurnRomInfo tgalaga90RomDesc[] = {
	{ "galaga '90 (usa).pce",	0x40000, 0x2909dec6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tgalaga90)
STD_ROM_FN(tgalaga90)

struct BurnDriver BurnDrvTG_tgalaga90 = {
	"tg_galaga90", NULL, NULL, NULL, "1989",
	"Galaga '90\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tgalaga90RomInfo, tgalaga90RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Ghost Manor

static struct BurnRomInfo tghostmanRomDesc[] = {
	{ "ghost manor (usa).pce",	0x80000, 0x2db4c1fd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tghostman)
STD_ROM_FN(tghostman)

struct BurnDriver BurnDrvTG_tghostman = {
	"tg_ghostman", NULL, NULL, NULL, "1992",
	"Ghost Manor\0", NULL, "TTI", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tghostmanRomInfo, tghostmanRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Gunboat

static struct BurnRomInfo tgunboatRomDesc[] = {
	{ "gunboat (usa).pce",	0x80000, 0xf370b58e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tgunboat)
STD_ROM_FN(tgunboat)

struct BurnDriver BurnDrvTG_tgunboat = {
	"tg_gunboat", NULL, NULL, NULL, "1992",
	"Gunboat\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tgunboatRomInfo, tgunboatRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Hit the Ice - VHL - The Official Video Hockey League

static struct BurnRomInfo thiticeRomDesc[] = {
	{ "hit the ice - vhl the official video hockey league (usa).pce",	0x60000, 0x8b29c3aa, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(thitice)
STD_ROM_FN(thitice)

struct BurnDriver BurnDrvTG_thitice = {
	"tg_hitice", NULL, NULL, NULL, "1992",
	"Hit the Ice - VHL - The Official Video Hockey League\0", NULL, "TTI", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, thiticeRomInfo, thiticeRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Impossamole

static struct BurnRomInfo timpossamRomDesc[] = {
	{ "impossamole (usa).pce",	0x80000, 0xe2470f5f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(timpossam)
STD_ROM_FN(timpossam)

struct BurnDriver BurnDrvTG_timpossam = {
	"tg_impossam", NULL, NULL, NULL, "1991",
	"Impossamole\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, timpossamRomInfo, timpossamRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// J.J. & Jeff

static struct BurnRomInfo tjjnjeffRomDesc[] = {
	{ "j.j. & jeff (usa).pce",	0x40000, 0xe01c5127, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tjjnjeff)
STD_ROM_FN(tjjnjeff)

struct BurnDriver BurnDrvTG_tjjnjeff = {
	"tg_jjnjeff", NULL, NULL, NULL, "1990",
	"J.J. & Jeff\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tjjnjeffRomInfo, tjjnjeffRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Jack Nicklaus' Turbo Golf

static struct BurnRomInfo tturboglfRomDesc[] = {
	{ "jack nicklaus' turbo golf (usa).pce",	0x40000, 0x83384572, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tturboglf)
STD_ROM_FN(tturboglf)

struct BurnDriver BurnDrvTG_tturboglf = {
	"tg_turboglf", NULL, NULL, NULL, "1990",
	"Jack Nicklaus' Turbo Golf\0", NULL, "Accolade", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tturboglfRomInfo, tturboglfRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Jackie Chan's Action Kung Fu

static struct BurnRomInfo tjchanRomDesc[] = {
	{ "jackie chan's action kung fu (usa).pce",	0x80000, 0x9d2f6193, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tjchan)
STD_ROM_FN(tjchan)

struct BurnDriver BurnDrvTG_tjchan = {
	"tg_jchan", NULL, NULL, NULL, "1992",
	"Jackie Chan's Action Kung Fu\0", NULL, "Hudson Soft", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tjchanRomInfo, tjchanRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Keith Courage in Alpha Zones

static struct BurnRomInfo tkeithcorRomDesc[] = {
	{ "keith courage in alpha zones (usa).pce",	0x40000, 0x474d7a72, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tkeithcor)
STD_ROM_FN(tkeithcor)

struct BurnDriver BurnDrvTG_tkeithcor = {
	"tg_keithcor", NULL, NULL, NULL, "1989",
	"Keith Courage in Alpha Zones\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tkeithcorRomInfo, tkeithcorRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// King of Casino

static struct BurnRomInfo tkingcasnRomDesc[] = {
	{ "king of casino (usa).pce",	0x40000, 0x2f2e2240, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tkingcasn)
STD_ROM_FN(tkingcasn)

struct BurnDriver BurnDrvTG_tkingcasn = {
	"tg_kingcasn", NULL, NULL, NULL, "1990",
	"King of Casino\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tkingcasnRomInfo, tkingcasnRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Klax

static struct BurnRomInfo tklaxRomDesc[] = {
	{ "klax (usa).pce",	0x40000, 0x0f1b59b4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tklax)
STD_ROM_FN(tklax)

struct BurnDriver BurnDrvTG_tklax = {
	"tg_klax", NULL, NULL, NULL, "1990",
	"Klax\0", NULL, "Tengen", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tklaxRomInfo, tklaxRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Legend of Hero Tonma

static struct BurnRomInfo tlohtRomDesc[] = {
	{ "legend of hero tonma (usa).pce",	0x80000, 0x3c131486, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tloht)
STD_ROM_FN(tloht)

struct BurnDriver BurnDrvTG_tloht = {
	"tg_loht", NULL, NULL, NULL, "1991",
	"Legend of Hero Tonma\0", NULL, "TTI", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tlohtRomInfo, tlohtRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// The Legendary Axe II

static struct BurnRomInfo tlegaxe2RomDesc[] = {
	{ "legendary axe ii, the (usa).pce",	0x40000, 0x220ebf91, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tlegaxe2)
STD_ROM_FN(tlegaxe2)

struct BurnDriver BurnDrvTG_tlegaxe2 = {
	"tg_legaxe2", NULL, NULL, NULL, "1990",
	"The Legendary Axe II\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tlegaxe2RomInfo, tlegaxe2RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// The Legendary Axe

static struct BurnRomInfo tlegaxeRomDesc[] = {
	{ "legendary axe, the (usa).pce",	0x40000, 0x2d211007, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tlegaxe)
STD_ROM_FN(tlegaxe)

struct BurnDriver BurnDrvTG_tlegaxe = {
	"tg_legaxe", NULL, NULL, NULL, "1989",
	"The Legendary Axe\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tlegaxeRomInfo, tlegaxeRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Magical Chase

static struct BurnRomInfo tmagchaseRomDesc[] = {
	{ "magical chase (usa).pce",	0x80000, 0x95cd2979, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tmagchase)
STD_ROM_FN(tmagchase)

struct BurnDriver BurnDrvTG_tmagchase = {
	"tg_magchase", NULL, NULL, NULL, "1991",
	"Magical Chase\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tmagchaseRomInfo, tmagchaseRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Military Madness

static struct BurnRomInfo tmiltrymdRomDesc[] = {
	{ "military madness (usa).pce",	0x60000, 0x93f316f7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tmiltrymd)
STD_ROM_FN(tmiltrymd)

struct BurnDriver BurnDrvTG_tmiltrymd = {
	"tg_miltrymd", NULL, NULL, NULL, "1989",
	"Military Madness\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tmiltrymdRomInfo, tmiltrymdRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Moto Roader

static struct BurnRomInfo tmotoroadRomDesc[] = {
	{ "moto roader (usa).pce",	0x40000, 0xe2b0d544, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tmotoroad)
STD_ROM_FN(tmotoroad)

struct BurnDriver BurnDrvTG_tmotoroad = {
	"tg_motoroad", NULL, NULL, NULL, "1989",
	"Moto Roader\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tmotoroadRomInfo, tmotoroadRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Neutopia

static struct BurnRomInfo tneutopiaRomDesc[] = {
	{ "neutopia (usa).pce",	0x60000, 0xa9a94e1b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tneutopia)
STD_ROM_FN(tneutopia)

struct BurnDriver BurnDrvTG_tneutopia = {
	"tg_neutopia", NULL, NULL, NULL, "1990",
	"Neutopia\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tneutopiaRomInfo, tneutopiaRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Neutopia II

static struct BurnRomInfo tneutopi2RomDesc[] = {
	{ "neutopia ii (usa).pce",	0xc0000, 0xc4ed4307, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tneutopi2)
STD_ROM_FN(tneutopi2)

struct BurnDriver BurnDrvTG_tneutopi2 = {
	"tg_neutopi2", NULL, NULL, NULL, "1992",
	"Neutopia II\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tneutopi2RomInfo, tneutopi2RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// New Adventure Island

static struct BurnRomInfo tadvislndRomDesc[] = {
	{ "new adventure island (usa).pce",	0x80000, 0x756a1802, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tadvislnd)
STD_ROM_FN(tadvislnd)

struct BurnDriver BurnDrvTG_tadvislnd = {
	"tg_advislnd", NULL, NULL, NULL, "1992",
	"New Adventure Island\0", NULL, "TTI", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tadvislndRomInfo, tadvislndRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Night Creatures

static struct BurnRomInfo tnightcrRomDesc[] = {
	{ "night creatures (usa).pce",	0x80000, 0xc159761b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tnightcr)
STD_ROM_FN(tnightcr)

struct BurnDriver BurnDrvTG_tnightcr = {
	"tg_nightcr", NULL, NULL, NULL, "1992",
	"Night Creatures\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tnightcrRomInfo, tnightcrRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Ninja Spirit

static struct BurnRomInfo tnspiritRomDesc[] = {
	{ "ninja spirit (usa).pce",	0x80000, 0xde8af1c1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tnspirit)
STD_ROM_FN(tnspirit)

struct BurnDriver BurnDrvTG_tnspirit = {
	"tg_nspirit", NULL, NULL, NULL, "1990",
	"Ninja Spirit\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tnspiritRomInfo, tnspiritRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Order of the Griffon

static struct BurnRomInfo tgriffonRomDesc[] = {
	{ "order of the griffon (usa).pce",	0x80000, 0xfae0fc60, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tgriffon)
STD_ROM_FN(tgriffon)

struct BurnDriver BurnDrvTG_tgriffon = {
	"tg_griffon", NULL, NULL, NULL, "1992",
	"Order of the Griffon\0", NULL, "TTI", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tgriffonRomInfo, tgriffonRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Ordyne

static struct BurnRomInfo tordyneRomDesc[] = {
	{ "ordyne (usa).pce",	0x80000, 0xe7bf2a74, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tordyne)
STD_ROM_FN(tordyne)

struct BurnDriver BurnDrvTG_tordyne = {
	"tg_ordyne", NULL, NULL, NULL, "1989",
	"Ordyne\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tordyneRomInfo, tordyneRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Pac-land

static struct BurnRomInfo tpaclandRomDesc[] = {
	{ "pac-land (usa).pce",	0x40000, 0xd6e30ccd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tpacland)
STD_ROM_FN(tpacland)

struct BurnDriver BurnDrvTG_tpacland = {
	"tg_pacland", NULL, NULL, NULL, "1990",
	"Pac-land\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tpaclandRomInfo, tpaclandRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Panza Kick Boxing

static struct BurnRomInfo tpanzakbRomDesc[] = {
	{ "panza kick boxing (usa).pce",	0x80000, 0xa980e0e9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tpanzakb)
STD_ROM_FN(tpanzakb)

struct BurnDriver BurnDrvTG_tpanzakb = {
	"tg_panzakb", NULL, NULL, NULL, "1990",
	"Panza Kick Boxing\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tpanzakbRomInfo, tpanzakbRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Parasol Stars - The Story of Bubble Bobble III

static struct BurnRomInfo tparasolRomDesc[] = {
	{ "parasol stars - the story of bubble bobble iii (usa).pce",	0x60000, 0xe6458212, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tparasol)
STD_ROM_FN(tparasol)

struct BurnDriver BurnDrvTG_tparasol = {
	"tg_parasol", NULL, NULL, NULL, "1991",
	"Parasol Stars - The Story of Bubble Bobble III\0", NULL, "Working Designs", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tparasolRomInfo, tparasolRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Power Golf

static struct BurnRomInfo tpgolfRomDesc[] = {
	{ "power golf (usa).pce",	0x60000, 0xed1d3843, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tpgolf)
STD_ROM_FN(tpgolf)

struct BurnDriver BurnDrvTG_tpgolf = {
	"tg_pgolf", NULL, NULL, NULL, "1989",
	"Power Golf\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tpgolfRomInfo, tpgolfRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Psychosis

static struct BurnRomInfo tpsychosRomDesc[] = {
	{ "psychosis (usa).pce",	0x40000, 0x6cc10824, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tpsychos)
STD_ROM_FN(tpsychos)

struct BurnDriver BurnDrvTG_tpsychos = {
	"tg_psychos", NULL, NULL, NULL, "1990",
	"Psychosis\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tpsychosRomInfo, tpsychosRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// R-Type

static struct BurnRomInfo trtypeRomDesc[] = {
	{ "r-type (usa).pce",	0x80000, 0x91ce5156, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(trtype)
STD_ROM_FN(trtype)

struct BurnDriver BurnDrvTG_trtype = {
	"tg_rtype", NULL, NULL, NULL, "1989",
	"R-Type\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, trtypeRomInfo, trtypeRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Raiden

static struct BurnRomInfo traidenRomDesc[] = {
	{ "raiden (usa).pce",	0xc0000, 0xbc59c31e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(traiden)
STD_ROM_FN(traiden)

struct BurnDriver BurnDrvTG_traiden = {
	"tg_raiden", NULL, NULL, NULL, "1990",
	"Raiden\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, traidenRomInfo, traidenRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Samurai-Ghost

static struct BurnRomInfo tsamuraigRomDesc[] = {
	{ "samurai-ghost (usa).pce",	0x80000, 0x77a924b7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tsamuraig)
STD_ROM_FN(tsamuraig)

struct BurnDriver BurnDrvTG_tsamuraig = {
	"tg_samuraig", NULL, NULL, NULL, "1992",
	"Samurai-Ghost\0", NULL, "TTI", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tsamuraigRomInfo, tsamuraigRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Shockman

static struct BurnRomInfo tshockmanRomDesc[] = {
	{ "shockman (usa).pce",	0x80000, 0x2774462c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tshockman)
STD_ROM_FN(tshockman)

struct BurnDriver BurnDrvTG_tshockman = {
	"tg_shockman", NULL, NULL, NULL, "1992",
	"Shockman\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tshockmanRomInfo, tshockmanRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Sidearms - Hyper Dyne

static struct BurnRomInfo tsidearmsRomDesc[] = {
	{ "sidearms - hyper dyne (usa).pce",	0x40000, 0xd1993c9f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tsidearms)
STD_ROM_FN(tsidearms)

struct BurnDriver BurnDrvTG_tsidearms = {
	"tg_sidearms", NULL, NULL, NULL, "1989",
	"Sidearms - Hyper Dyne\0", NULL, "Radiance Software", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tsidearmsRomInfo, tsidearmsRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Silent Debuggers

static struct BurnRomInfo tsilentdRomDesc[] = {
	{ "silent debuggers (usa).pce",	0x80000, 0xfa7e5d66, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tsilentd)
STD_ROM_FN(tsilentd)

struct BurnDriver BurnDrvTG_tsilentd = {
	"tg_silentd", NULL, NULL, NULL, "1991",
	"Silent Debuggers\0", NULL, "Hudson Soft", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tsilentdRomInfo, tsilentdRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Sinistron

static struct BurnRomInfo tsinistrnRomDesc[] = {
	{ "sinistron (usa).pce",	0x60000, 0x4f6e2dbd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tsinistrn)
STD_ROM_FN(tsinistrn)

struct BurnDriver BurnDrvTG_tsinistrn = {
	"tg_sinistrn", NULL, NULL, NULL, "1991",
	"Sinistron\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tsinistrnRomInfo, tsinistrnRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Soldier Blade

static struct BurnRomInfo tsoldbladRomDesc[] = {
	{ "soldier blade (usa).pce",	0x80000, 0x4bb68b13, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tsoldblad)
STD_ROM_FN(tsoldblad)

struct BurnDriver BurnDrvTG_tsoldblad = {
	"tg_soldblad", NULL, NULL, NULL, "1992",
	"Soldier Blade\0", NULL, "TTI", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tsoldbladRomInfo, tsoldbladRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Somer Assault

static struct BurnRomInfo tsomerassRomDesc[] = {
	{ "somer assault (usa).pce",	0x80000, 0x8fcaf2e9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tsomerass)
STD_ROM_FN(tsomerass)

struct BurnDriver BurnDrvTG_tsomerass = {
	"tg_somerass", NULL, NULL, NULL, "1992",
	"Somer Assault\0", NULL, "Atlus", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tsomerassRomInfo, tsomerassRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Sonic Spike - World Championship Beach Volleyball

static struct BurnRomInfo twbeachRomDesc[] = {
	{ "sonic spike - world championship beach volleyball (usa).pce",	0x40000, 0xf74e5eb3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(twbeach)
STD_ROM_FN(twbeach)

struct BurnDriver BurnDrvTG_twbeach = {
	"tg_wbeach", NULL, NULL, NULL, "1990",
	"Sonic Spike - World Championship Beach Volleyball\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, twbeachRomInfo, twbeachRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Space Harrier

static struct BurnRomInfo tsharrierRomDesc[] = {
	{ "space harrier (usa).pce",	0x80000, 0x43b05eb8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tsharrier)
STD_ROM_FN(tsharrier)

struct BurnDriver BurnDrvTG_tsharrier = {
	"tg_sharrier", NULL, NULL, NULL, "1989",
	"Space Harrier\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tsharrierRomInfo, tsharrierRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Splatterhouse

static struct BurnRomInfo tsplatthRomDesc[] = {
	{ "splatterhouse (usa).pce",	0x80000, 0xd00ca74f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tsplatth)
STD_ROM_FN(tsplatth)

struct BurnDriver BurnDrvTG_tsplatth = {
	"tg_splatth", NULL, NULL, NULL, "1990",
	"Splatterhouse\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tsplatthRomInfo, tsplatthRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Super Star Soldier

static struct BurnRomInfo tsssoldrRomDesc[] = {
	{ "super star soldier (usa).pce",	0x80000, 0xdb29486f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tsssoldr)
STD_ROM_FN(tsssoldr)

struct BurnDriver BurnDrvTG_tsssoldr = {
	"tg_sssoldr", NULL, NULL, NULL, "1990",
	"Super Star Soldier\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tsssoldrRomInfo, tsssoldrRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Super Volleyball

static struct BurnRomInfo tsvolleyRomDesc[] = {
	{ "super volleyball (usa).pce",	0x40000, 0x245040b3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tsvolley)
STD_ROM_FN(tsvolley)

struct BurnDriver BurnDrvTG_tsvolley = {
	"tg_svolley", NULL, NULL, NULL, "1990",
	"Super Volleyball\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tsvolleyRomInfo, tsvolleyRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Taito Chase H.Q.

static struct BurnRomInfo tchasehqRomDesc[] = {
	{ "taito chase h.q. (usa).pce",	0x60000, 0x9298254c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tchasehq)
STD_ROM_FN(tchasehq)

struct BurnDriver BurnDrvTG_tchasehq = {
	"tg_chasehq", NULL, NULL, NULL, "1992",
	"Taito Chase H.Q.\0", NULL, "TTI", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tchasehqRomInfo, tchasehqRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Takin' It to the Hoop

static struct BurnRomInfo ttaknhoopRomDesc[] = {
	{ "takin' it to the hoop (usa).pce",	0x40000, 0xe9d51797, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ttaknhoop)
STD_ROM_FN(ttaknhoop)

struct BurnDriver BurnDrvTG_ttaknhoop = {
	"tg_taknhoop", NULL, NULL, NULL, "1989",
	"Takin' It to the Hoop\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, ttaknhoopRomInfo, ttaknhoopRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// TaleSpin

static struct BurnRomInfo ttalespinRomDesc[] = {
	{ "talespin (usa).pce",	0x80000, 0xbae9cecc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ttalespin)
STD_ROM_FN(ttalespin)

struct BurnDriver BurnDrvTG_ttalespin = {
	"tg_talespin", NULL, NULL, NULL, "1991",
	"TaleSpin\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, ttalespinRomInfo, ttalespinRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Tiger Road

static struct BurnRomInfo ttigerrodRomDesc[] = {
	{ "tiger road (usa).pce",	0x60000, 0x985d492d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ttigerrod)
STD_ROM_FN(ttigerrod)

struct BurnDriver BurnDrvTG_ttigerrod = {
	"tg_tigerrod", NULL, NULL, NULL, "1990",
	"Tiger Road\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, ttigerrodRomInfo, ttigerrodRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Time Cruise

static struct BurnRomInfo ttimcrusRomDesc[] = {
	{ "time cruise (usa).pce",	0x80000, 0x02c39660, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ttimcrus)
STD_ROM_FN(ttimcrus)

struct BurnDriver BurnDrvTG_ttimcrus = {
	"tg_timcrus", NULL, NULL, NULL, "1992",
	"Time Cruise\0", NULL, "TTI", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, ttimcrusRomInfo, ttimcrusRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Timeball

static struct BurnRomInfo ttimeballRomDesc[] = {
	{ "timeball (usa).pce",	0x20000, 0x5d395019, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ttimeball)
STD_ROM_FN(ttimeball)

struct BurnDriver BurnDrvTG_ttimeball = {
	"tg_timeball", NULL, NULL, NULL, "1990",
	"Timeball\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, ttimeballRomInfo, ttimeballRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Tricky Kick

static struct BurnRomInfo ttrickyRomDesc[] = {
	{ "tricky kick (usa).pce",	0x40000, 0x48e6fd34, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ttricky)
STD_ROM_FN(ttricky)

struct BurnDriver BurnDrvTG_ttricky = {
	"tg_tricky", NULL, NULL, NULL, "1990",
	"Tricky Kick\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, ttrickyRomInfo, ttrickyRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Turrican

static struct BurnRomInfo tturricanRomDesc[] = {
	{ "turrican (usa).pce",	0x40000, 0xeb045edf, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tturrican)
STD_ROM_FN(tturrican)

struct BurnDriver BurnDrvTG_tturrican = {
	"tg_turrican", NULL, NULL, NULL, "1991",
	"Turrican\0", NULL, "Ballistic", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tturricanRomInfo, tturricanRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// TV Sports Basketball

static struct BurnRomInfo ttvbasketRomDesc[] = {
	{ "tv sports basketball (usa).pce",	0x80000, 0xea54d653, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ttvbasket)
STD_ROM_FN(ttvbasket)

struct BurnDriver BurnDrvTG_ttvbasket = {
	"tg_tvbasket", NULL, NULL, NULL, "1991",
	"TV Sports Basketball\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, ttvbasketRomInfo, ttvbasketRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// TV Sports Football

static struct BurnRomInfo ttvfootblRomDesc[] = {
	{ "tv sports football (usa).pce",	0x60000, 0x5e25b557, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ttvfootbl)
STD_ROM_FN(ttvfootbl)

struct BurnDriver BurnDrvTG_ttvfootbl = {
	"tg_tvfootbl", NULL, NULL, NULL, "1990",
	"TV Sports Football\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, ttvfootblRomInfo, ttvfootblRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// TV Sports Hockey

static struct BurnRomInfo ttvhockeyRomDesc[] = {
	{ "tv sports hockey (usa).pce",	0x60000, 0x97fe5bcf, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(ttvhockey)
STD_ROM_FN(ttvhockey)

struct BurnDriver BurnDrvTG_ttvhockey = {
	"tg_tvhockey", NULL, NULL, NULL, "1991",
	"TV Sports Hockey\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, ttvhockeyRomInfo, ttvhockeyRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Veigues - Tactical Gladiator

static struct BurnRomInfo tveiguesRomDesc[] = {
	{ "veigues - tactical gladiator (usa).pce",	0x60000, 0x99d14fb7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tveigues)
STD_ROM_FN(tveigues)

struct BurnDriver BurnDrvTG_tveigues = {
	"tg_veigues", NULL, NULL, NULL, "1990",
	"Veigues - Tactical Gladiator\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tveiguesRomInfo, tveiguesRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Victory Run

static struct BurnRomInfo tvictoryrRomDesc[] = {
	{ "victory run (usa).pce",	0x40000, 0x85cbd045, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tvictoryr)
STD_ROM_FN(tvictoryr)

struct BurnDriver BurnDrvTG_tvictoryr = {
	"tg_victoryr", NULL, NULL, NULL, "1989",
	"Victory Run\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tvictoryrRomInfo, tvictoryrRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Vigilante

static struct BurnRomInfo tvigilantRomDesc[] = {
	{ "vigilante (usa).pce",	0x60000, 0x79d49a0d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tvigilant)
STD_ROM_FN(tvigilant)

struct BurnDriver BurnDrvTG_tvigilant = {
	"tg_vigilant", NULL, NULL, NULL, "1989",
	"Vigilante\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tvigilantRomInfo, tvigilantRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// World Class Baseball

static struct BurnRomInfo twcbaseblRomDesc[] = {
	{ "world class baseball (usa).pce",	0x40000, 0x4186d0c0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(twcbasebl)
STD_ROM_FN(twcbasebl)

struct BurnDriver BurnDrvTG_twcbasebl = {
	"tg_wcbasebl", NULL, NULL, NULL, "1989",
	"World Class Baseball\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, twcbaseblRomInfo, twcbaseblRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// World Court Tennis

static struct BurnRomInfo twctennisRomDesc[] = {
	{ "world court tennis (usa).pce",	0x40000, 0xa4457df0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(twctennis)
STD_ROM_FN(twctennis)

struct BurnDriver BurnDrvTG_twctennis = {
	"tg_wctennis", NULL, NULL, NULL, "1989",
	"World Court Tennis\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, twctennisRomInfo, twctennisRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// World Sports Competition

static struct BurnRomInfo twscompRomDesc[] = {
	{ "world sports competition (usa).pce",	0x80000, 0x4b93f0ac, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(twscomp)
STD_ROM_FN(twscomp)

struct BurnDriver BurnDrvTG_twscomp = {
	"tg_wscomp", NULL, NULL, NULL, "1992",
	"World Sports Competition\0", NULL, "TTI", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, twscompRomInfo, twscompRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Yo, Bro

static struct BurnRomInfo tyobroRomDesc[] = {
	{ "yo, bro (usa).pce",	0x80000, 0x3ca7db48, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tyobro)
STD_ROM_FN(tyobro)

struct BurnDriver BurnDrvTG_tyobro = {
	"tg_yobro", NULL, NULL, NULL, "1991",
	"Yo, Bro\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tyobroRomInfo, tyobroRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Turbografx CD Super System Card (v3.0)

static struct BurnRomInfo tscdsysRomDesc[] = {
	{ "[cd] turbografx cd super system card (usa) (v3.0).pce",	0x40000, 0x2b5b75fe, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tscdsys)
STD_ROM_FN(tscdsys)

struct BurnDriverD BurnDrvTG_tscdsys = {
	"tg_scdsys", NULL, NULL, NULL, "19??",
	"Turbografx CD Super System Card (v3.0)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tscdsysRomInfo, tscdsysRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Turbografx CD System Card (v2.0)

static struct BurnRomInfo tcdsysRomDesc[] = {
	{ "[cd] turbografx cd system card (usa) (v2.0).pce",	0x40000, 0xff2a5ec3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tcdsys)
STD_ROM_FN(tcdsys)

struct BurnDriverD BurnDrvTG_tcdsys = {
	"tg_cdsys", NULL, NULL, NULL, "19??",
	"Turbografx CD System Card (v2.0)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_MISC, 0,
	TgGetZipName, tcdsysRomInfo, tcdsysRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// 1941 - Counter Attack

static struct BurnRomInfo s1941RomDesc[] = {
	{ "1941 - counter attack (japan).pce",	0x100000, 0x8c4588e2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(s1941)
STD_ROM_FN(s1941)

struct BurnDriver BurnDrvSGX_s1941 = {
	"sgx_1941", NULL, NULL, NULL, "1991",
	"1941 - Counter Attack\0", NULL, "Hudson", "SuperGrafx",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_SGX, GBF_MISC, 0,
	SgxGetZipName, s1941RomInfo, s1941RomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	SGXInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Aldynes

static struct BurnRomInfo saldynesRomDesc[] = {
	{ "aldynes (japan).pce",	0x100000, 0x4c2126b0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(saldynes)
STD_ROM_FN(saldynes)

struct BurnDriver BurnDrvSGX_saldynes = {
	"sgx_aldynes", NULL, NULL, NULL, "1991",
	"Aldynes\0", NULL, "Hudson", "SuperGrafx",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_SGX, GBF_MISC, 0,
	SgxGetZipName, saldynesRomInfo, saldynesRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	SGXInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Battle Ace

static struct BurnRomInfo sbattlaceRomDesc[] = {
	{ "battle ace (japan).pce",	0x80000, 0x3b13af61, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sbattlace)
STD_ROM_FN(sbattlace)

struct BurnDriver BurnDrvSGX_sbattlace = {
	"sgx_battlace", NULL, NULL, NULL, "1989",
	"Battle Ace\0", NULL, "Hudson", "SuperGrafx",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_SGX, GBF_MISC, 0,
	SgxGetZipName, sbattlaceRomInfo, sbattlaceRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	SGXInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Daimakai Mura

static struct BurnRomInfo sdaimakaiRomDesc[] = {
	{ "daimakai mura (japan).pce",	0x100000, 0xb486a8ed, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sdaimakai)
STD_ROM_FN(sdaimakai)

struct BurnDriver BurnDrvSGX_sdaimakai = {
	"sgx_daimakai", NULL, NULL, NULL, "1990",
	"Daimakai Mura\0", NULL, "NEC Avenue", "SuperGrafx",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_SGX, GBF_MISC, 0,
	SgxGetZipName, sdaimakaiRomInfo, sdaimakaiRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	SGXInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};


// Madou Ou Granzort

static struct BurnRomInfo sgranzortRomDesc[] = {
	{ "madou ou granzort (japan).pce",	0x80000, 0x1f041166, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sgranzort)
STD_ROM_FN(sgranzort)

struct BurnDriver BurnDrvSGX_sgranzort = {
	"sgx_granzort", NULL, NULL, NULL, "1990",
	"Madou Ou Granzort\0", NULL, "Hudson", "SuperGrafx",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_SGX, GBF_MISC, 0,
	SgxGetZipName, sgranzortRomInfo, sgranzortRomName, NULL, NULL, pceInputInfo, pceDIPInfo,
	SGXInit, PCEExit, PCEFrame, PCEDraw, PCEScan, &PCEPaletteRecalc, 0x400,
	512, 240, 4, 3
};
