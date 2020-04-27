// FinalBurn Neo Eolith 16-bit Hyperstone driver module
// Based on MAME driver by Pierpaolo Prazzoli

#include "tiles_generic.h"
#include "e132xs_intf.h"
#include "eeprom.h"
#include "msm6295.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvMainROM;
static UINT8 *DrvBootROM;
static UINT8 *DrvSndROM;
static UINT8 *DrvMainRAM;
static UINT8 *DrvVidRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 vidrambank;

static INT32 vblank;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT16 DrvInputs[2];
static UINT8 DrvReset;

static struct BurnInputInfo Eolith16InputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",	BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy2 + 5,	"service"	},
	{"Service Mode",BIT_DIGITAL,	DrvJoy2 + 6,	"diag"		},
};

STDINPUTINFO(Eolith16)

static void set_vidrambank(INT32 data)
{
	vidrambank = data & 0x80;

	INT32 bank = ((vidrambank ^ 0x80) >> 7) * 0x10000;

	E132XSMapMemory(DrvVidRAM + bank,			0x50000000, 0x5000ffff, MAP_RAM);
}

static void eolith16_write_word(UINT32 address, UINT16 data)
{
	switch (address & ~3)
	{
		case 0xffe40000:
			MSM6295Write(0, data);
		return;

		case 0xffe80000:
			set_vidrambank(data);
			EEPROMWriteBit(data & 0x40);
			EEPROMSetCSLine((data & 0x10) ? EEPROM_CLEAR_LINE : EEPROM_ASSERT_LINE);
			EEPROMSetClockLine((data & 0x20) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
		return;
	}
}

static UINT32 eolith16_read_long(UINT32 address)
{
	switch (address)
	{
		case 0xffe40000:
			return MSM6295Read(0);

		case 0xffea0000:
		{
			UINT32 ret = (DrvInputs[1] << 16) | 0xff6f;
			if (EEPROMRead()) ret |= 0x10;
			ret ^= (vblank ? 0 : 0x80);
			if (vblank == 0 && E132XSGetPC(0) == 0x1a046) E132XSBurnCycles(100);
			return (ret >> 16) | (ret << 16);
		}

		case 0xffec0000:
			return DrvInputs[0] | (DrvInputs[0] << 16);
	}

	return 0;
}

static UINT16 eolith16_read_word(UINT32 address)
{
	switch (address)
	{
		case 0xffe40000:
			return MSM6295Read(0);

		case 0xffea0000:
		{
			UINT32 ret = (DrvInputs[1] << 16) | (0xff6f);
			if (EEPROMRead()) ret |= 0x10;
			ret ^= (vblank ? 0 : 0x80);
			if (vblank == 0 && E132XSGetPC(0) == 0x1a046) E132XSBurnCycles(100);
			return ret;
		}

		case 0xffea0002:
			return DrvInputs[1];

		case 0xffec0000:
			return 0; // nop

		case 0xffec0002:
			return DrvInputs[0];
	}

	return 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	E132XSOpen(0);
	set_vidrambank(0);
	E132XSReset();
	E132XSClose();

	MSM6295Reset();

	EEPROMReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvMainROM	= Next; Next += 0x200000;
	DrvBootROM	= Next; Next += 0x080000;

	MSM6295ROM	= Next;
	DrvSndROM	= Next; Next += 0x080000;

	DrvPalette	= (UINT32*)Next; Next += 0x0100 * sizeof(UINT32);

	AllRam		= Next;

	DrvMainRAM	= Next; Next += 0x200000;
	DrvVidRAM	= Next; Next += 0x020000;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static const eeprom_interface eeprom_interface_93C66 =
{
	9,				// address bits 9
	8,				// data bits    8
	"*110",			// read         110 aaaaaaaaa
	"*101",			// write        101 aaaaaaaaa dddddddd
	"*111",			// erase        111 aaaaaaaaa
	"*10000xxxxxx",	// lock         100 00xxxxxxx
	"*10011xxxxxx",	// unlock       100 11xxxxxxx
	0,0
};

static INT32 Eolith16Init()
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRomExt(DrvBootROM + 0x0000000, k++, 1, LD_BYTESWAP)) return 1;

		if (BurnLoadRom(DrvMainROM + 0x0000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x0000000, k++, 1)) return 1;
	}

	E132XSInit(0, TYPE_E116T, 60000000);
	E132XSOpen(0);
	E132XSMapMemory(DrvMainRAM,			0x00000000, 0x001fffff, MAP_RAM);
	E132XSMapMemory(DrvVidRAM,			0x50000000, 0x5000ffff, MAP_RAM);
	E132XSMapMemory(DrvMainROM,			0xff000000, 0xff1fffff, MAP_ROM);
	E132XSMapMemory(DrvBootROM,			0xfff80000, 0xffffffff, MAP_ROM);
	E132XSSetWriteWordHandler(eolith16_write_word);
	E132XSSetReadLongHandler(eolith16_read_long);
	E132XSSetReadWordHandler(eolith16_read_word);
	E132XSClose();

	EEPROMInit(&eeprom_interface_93C66);

	MSM6295Init(0, 1000000 / MSM6295_PIN7_HIGH, 0);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	E132XSExit();
	EEPROMExit();
	MSM6295Exit();

	BurnFreeMemIndex();

	MSM6295ROM = NULL;

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 c = 0; c < 256; c++)
	{
		INT32 bit0 = (c & 0x01) ? 0x21 : 0;
		INT32 bit1 = (c & 0x02) ? 0x47 : 0;
		INT32 bit2 = (c & 0x04) ? 0x97 : 0;

		INT32 bit3 = (c & 0x08) ? 0x21 : 0;
		INT32 bit4 = (c & 0x10) ? 0x47 : 0;
		INT32 bit5 = (c & 0x20) ? 0x97 : 0;

		INT32 bit6 = (c & 0x40) ? 0x55 : 0;
		INT32 bit7 = (c & 0x80) ? 0xaa : 0;

		INT32 r = bit0 + bit1 + bit2;
		INT32 g = bit3 + bit4 + bit5;
		INT32 b = bit6 + bit7;

		DrvPalette[c] = BurnHighCol(r,g,b,0);
	}
}

static void draw_bitmap()
{
	for (INT32 y = 0; y < nScreenHeight; y++)
	{
		UINT8 *src = DrvVidRAM + y * 320;
		UINT16 *dst = pTransDraw + y * nScreenWidth;

		for (INT32 x = 0; x < nScreenWidth; x++)
		{
			dst[x] = src[x];
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	draw_bitmap();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 262;
	INT32 nCyclesTotal[1] = { 60000000 / 60 };
	INT32 nCyclesDone[1] = { 0 };

	E132XSOpen(0);

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, E132XS);

		if (i >= 200) vblank = 1;
	}

	E132XSClose();

	if (pBurnSoundOut) {
		MSM6295Render(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
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
		E132XSScan(nAction);

		MSM6295Scan(nAction, pnMin);

		SCAN_VAR(vidrambank);
	}

	if (nAction & ACB_NVRAM) {
		EEPROMScan(nAction, pnMin);
	}

	if (nAction & ACB_WRITE) {
		E132XSOpen(0);
		set_vidrambank(vidrambank);
		E132XSClose();
	}

	return 0;
}


// KlonDike+

static struct BurnRomInfo klondkpRomDesc[] = {
	{ "kd.u5",	0x080000, 0x591f0c73, 1 | BRF_PRG | BRF_ESS }, //  0 E116T Boot ROM

	{ "kd.u31",	0x200000, 0xe5dd12b5, 2 | BRF_PRG | BRF_ESS }, //  1 E116T Main ROM

	{ "kd.u28",	0x080000, 0xc12112a1, 3 | BRF_SND },           //  2 Samples
};

STD_ROM_PICK(klondkp)
STD_ROM_FN(klondkp)

struct BurnDriver BurnDrvKlondkp = {
	"klondkp", NULL, NULL, NULL, "1999",
	"KlonDike+\0", NULL, "Eolith", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_CASINO, 0,
	NULL, klondkpRomInfo, klondkpRomName, NULL, NULL, NULL, NULL, Eolith16InputInfo, NULL,
	Eolith16Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 200, 4, 3
};
