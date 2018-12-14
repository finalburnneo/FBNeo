// FB Alpha Ping Pong Master '93 driver module
// Based on MAME driver by David Haywood

#include "tiles_generic.h"
#include "z80_intf.h"
#include "burn_ym2413.h"
#include "dac.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvBgRAM;
static UINT8 *DrvFgRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 bankdata;
static UINT8 soundlatch;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo Ppmast93InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Ppmast93)

static struct BurnDIPInfo Ppmast93DIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL					},
	{0x14, 0xff, 0xff, 0xfe, NULL					},

	{0   , 0xfe, 0   ,    16, "Coin A"				},
	{0x13, 0x01, 0x0f, 0x01, "5 Coins 1 Credits"	},
	{0x13, 0x01, 0x0f, 0x04, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x0f, 0x07, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x0f, 0x00, "5 Coins/2 Credits"	},
	{0x13, 0x01, 0x0f, 0x0a, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x0f, 0x06, "3 Coins 2 Credits"	},
	{0x13, 0x01, 0x0f, 0x03, "4 Coins 3 Credits"	},
	{0x13, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x0f, 0x02, "4 Coins 5 Credits"	},
	{0x13, 0x01, 0x0f, 0x09, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0x0f, 0x05, "3 Coins/5 Credits"	},
	{0x13, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x0f, 0x08, "2 Coins 5 Credits"	},
	{0x13, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},

	{0   , 0xfe, 0   ,    16, "Coin B"				},
	{0x13, 0x01, 0xf0, 0x10, "5 Coins 1 Credits"	},
	{0x13, 0x01, 0xf0, 0x40, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0xf0, 0x70, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0xf0, 0x00, "5 Coins/2 Credits"	},
	{0x13, 0x01, 0xf0, 0xa0, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0xf0, 0x60, "3 Coins 2 Credits"	},
	{0x13, 0x01, 0xf0, 0x30, "4 Coins 3 Credits"	},
	{0x13, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xf0, 0x20, "4 Coins 5 Credits"	},
	{0x13, 0x01, 0xf0, 0x90, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0xf0, 0x50, "3 Coins/5 Credits"	},
	{0x13, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xf0, 0x80, "2 Coins 5 Credits"	},
	{0x13, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x14, 0x01, 0x03, 0x00, "Very Hard"			},
	{0x14, 0x01, 0x03, 0x01, "Hard"					},
	{0x14, 0x01, 0x03, 0x02, "Normal"				},
	{0x14, 0x01, 0x03, 0x03, "Easy"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x14, 0x01, 0x04, 0x00, "Off"					},
	{0x14, 0x01, 0x04, 0x04, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x14, 0x01, 0x08, 0x08, "Off"					},
	{0x14, 0x01, 0x08, 0x00, "On"					},
};

STDDIPINFO(Ppmast93)

static void bankswitch(INT32 data)
{
	bankdata = data & 7;

	ZetMapMemory(DrvZ80ROM0 + (bankdata * 0x4000), 0x8000, 0xbfff, MAP_ROM);
}

static void __fastcall ppmast93_main_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			soundlatch = data;
		return;

		case 0x04:
			bankswitch(data);
			// coin latch - coin counter = data & 0x18
		return;
	}
}

static UINT8 __fastcall ppmast93_main_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
			return DrvInputs[0];

		case 0x02:
			return DrvInputs[1];

		case 0x04:
			return DrvInputs[2] & ~0x40;

		case 0x06:
			return DrvDips[0];

		case 0x08:
			return DrvDips[1];
	}

	return 0;
}

static UINT8 __fastcall ppmast93_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xfc00:
			return soundlatch;
	}

	return 0;
}

static UINT8 __fastcall ppmast93_sound_read_port(UINT16 port)
{
	return DrvZ80ROM1[0x10000 + port];
}

static void __fastcall ppmast93_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0x00ff)
	{
		case 0x0000:
		case 0x0001:
			BurnYM2413Write(port & 1, data);
		return;

		case 0x0002:
			DACSignedWrite(0, data);
		return;
	}
}

static tilemap_callback( bg )
{
	UINT16 code = (DrvBgRAM[offs * 2 + 1] << 8) | DrvBgRAM[offs * 2];

	TILE_SET_INFO(0, code, code >> 12, 0);
}

static tilemap_callback( fg )
{
	UINT16 code = (DrvFgRAM[offs * 2 + 1] << 8) | DrvFgRAM[offs * 2];

	TILE_SET_INFO(1, code, code >> 12, 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	bankswitch(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	BurnYM2413Reset();
	DACReset();
	ZetClose();

	soundlatch = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x020000;
	DrvZ80ROM1		= Next; Next += 0x020000;

	DrvGfxROM		= Next; Next += 0x080000;

	DrvColPROM		= Next; Next += 0x000300;

	DrvPalette		= (UINT32*)Next; Next += 0x0100 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM1		= Next; Next += 0x000300;
	DrvBgRAM		= Next; Next += 0x000800;
	DrvFgRAM		= Next; Next += 0x001000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[4] = { STEP4(0,2) };
	INT32 XOffs[8] = { 1, 0, 9, 8, 17, 16, 25, 24 };
	INT32 YOffs[8] = { STEP8(0,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x40000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM, 0x40000);

	GfxDecode(0x2000, 4, 8, 8, Plane, XOffs, YOffs, 0x100, tmp, DrvGfxROM);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit()
{
	BurnSetRefreshRate(55.00);

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  1, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM  + 0x00000,  2, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM  + 0x20000,  3, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000,  4, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00100,  5, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00200,  6, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvBgRAM,			0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvFgRAM,			0xf000, 0xffff, MAP_RAM);
	ZetSetOutHandler(ppmast93_main_write_port);
	ZetSetInHandler(ppmast93_main_read_port);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,		0x0000, 0xfbff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,		0xfd00, 0xffff, MAP_RAM);
	ZetSetReadHandler(ppmast93_sound_read);
	ZetSetOutHandler(ppmast93_sound_write_port);
	ZetSetInHandler(ppmast93_sound_read_port);
	ZetClose();

	BurnYM2413Init(2500000);
	BurnYM2413SetAllRoutes(1.40, BURN_SND_ROUTE_BOTH);

	DACInit(0, 0, 1, ZetTotalCycles, 5000000);
	DACSetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, fg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM + 0x00000, 4, 8, 8, 0x40000, 0, 0xf);
	GenericTilemapSetGfx(1, DrvGfxROM + 0x40000, 4, 8, 8, 0x40000, 0, 0xf);
	GenericTilemapSetTransparent(1,0);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	BurnYM2413Exit();
	DACExit();

	BurnFree(AllMem);

	return 0;
}

static inline void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x100; i++)
	{
		INT32 bit0 = (DrvColPROM[0x000 + i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[0x000 + i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[0x000 + i] >> 2) & 0x01;
		INT32 bit3 = (DrvColPROM[0x000 + i] >> 3) & 0x01;
		INT32 r = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[0x100 + i] >> 0) & 0x01;
		bit1 = (DrvColPROM[0x100 + i] >> 1) & 0x01;
		bit2 = (DrvColPROM[0x100 + i] >> 2) & 0x01;
		bit3 = (DrvColPROM[0x100 + i] >> 3) & 0x01;
		INT32 g = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[0x200 + i] >> 0) & 0x01;
		bit1 = (DrvColPROM[0x200 + i] >> 1) & 0x01;
		bit2 = (DrvColPROM[0x200 + i] >> 2) & 0x01;
		bit3 = (DrvColPROM[0x200 + i] >> 3) & 0x01;
		INT32 b = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	GenericTilemapDraw(0, pTransDraw, 0);

	GenericTilemapDraw(1, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 266;
	INT32 nCyclesTotal[2] = { 5000000 / 55, 5000000 / 55 };
	INT32 nCyclesDone[2] = { 0, 0 };
	INT32 nSoundBufferPos = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);
		if (i == (nInterleave-1)) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(1);
		nCyclesDone[1] += ZetRun(nCyclesTotal[1] / nInterleave);
		if (i & 1) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD); // 8000/60 x frame
		ZetClose();

		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			BurnYM2413Render(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		BurnYM2413Render(pSoundBuf, nSegmentLength);
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029702;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);

		BurnYM2413Scan(nAction, pnMin);
		DACScan(nAction,pnMin);

		SCAN_VAR(soundlatch);
		SCAN_VAR(bankdata);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		bankswitch(bankdata);
		ZetClose();
	}

	return 0;
}


// Ping Pong Masters '93

static struct BurnRomInfo ppmast93RomDesc[] = {
	{ "2.up7",		0x20000, 0x8854d8db, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	{ "1.ue7",		0x20000, 0x8e26939e, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code

	{ "3.ug16",		0x20000, 0x8ab24641, 3 | BRF_GRA },           //  2 Graphics
	{ "4.ug15",		0x20000, 0xb16e9fb6, 3 | BRF_GRA },           //  3

	{ "prom3.ug24",	0x00100, 0xb1a4415a, 4 | BRF_GRA },           //  4 Color data
	{ "prom2.ug25",	0x00100, 0x4b5055ba, 4 | BRF_GRA },           //  5
	{ "prom1.ug26",	0x00100, 0xd979c64e, 4 | BRF_GRA },           //  6
};

STD_ROM_PICK(ppmast93)
STD_ROM_FN(ppmast93)

struct BurnDriver BurnDrvPpmast93 = {
	"ppmast93", NULL, NULL, NULL, "1993",
	"Ping Pong Masters '93\0", NULL, "Electronic Devices S.R.L.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_BALLPADDLE, 0,
	NULL, ppmast93RomInfo, ppmast93RomName, NULL, NULL, NULL, NULL, Ppmast93InputInfo, Ppmast93DIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 256, 4, 3
};
