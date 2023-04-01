// FinalBurn Neo Stunt Air driver module
// Based on MAME driver by David Haywood

#include "tiles_generic.h"
#include "z80_intf.h"
#include "ay8910.h"
#include "watchdog.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvZ80ROM[2];
static UINT8 *DrvGfxROM[3];
static UINT8 *DrvColPROM;
static UINT8 *DrvNVRAM;
static UINT8 *DrvBgARAM;
static UINT8 *DrvBgVRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvZ80RAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 soundlatch;
static INT32 bg_scrollx;
static INT32 spritebank;
static INT32 nmi_enable;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo StuntairInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Stuntair)

static struct BurnDIPInfo StuntairDIPList[]=
{
	{0x0b, 0xf0, 0xff, 0xff, "dip offset"                           },

	{0x00, 0xff, 0xff, 0x08, NULL									},
	{0x01, 0xff, 0xff, 0xc0, NULL									},
	{0x02, 0xff, 0xff, 0x04, NULL									},

	{0   , 0xfe, 0   ,    2, "Infinite Lives (Cheat)"				},
	{0x00, 0x01, 0x04, 0x00, "Off"									},
	{0x00, 0x01, 0x04, 0x04, "On"									},

	{0   , 0xfe, 0   ,    4, "Difficulty"							},
	{0x00, 0x01, 0x28, 0x00, "Easy"									},
	{0x00, 0x01, 0x28, 0x08, "Normal"								},
	{0x00, 0x01, 0x28, 0x20, "Hard"									},
	{0x00, 0x01, 0x28, 0x28, "Hardest"								},

	{0   , 0xfe, 0   ,    4, "Coin A"								},
	{0x01, 0x01, 0x18, 0x08, "2 Coins 1 Credits"					},
	{0x01, 0x01, 0x18, 0x00, "1 Coin  1 Credits"					},
	{0x01, 0x01, 0x18, 0x18, "1 Coin/1 Credit - 2 Coins/3 Credits"	},
	{0x01, 0x01, 0x18, 0x10, "1 Coin  2 Credits"					},

	{0   , 0xfe, 0   ,    4, "Coin B"								},
	{0x01, 0x01, 0x24, 0x04, "2 Coins 1 Credits"					},
	{0x01, 0x01, 0x24, 0x00, "1 Coin  1 Credits"					},
	{0x01, 0x01, 0x24, 0x24, "1 Coin/1 Credit - 2 Coins/3 Credits"	},
	{0x01, 0x01, 0x24, 0x20, "1 Coin  2 Credits"					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"							},
	{0x01, 0x01, 0x42, 0x00, "10000 20000"							},
	{0x01, 0x01, 0x42, 0x40, "20000 30000"							},
	{0x01, 0x01, 0x42, 0x02, "30000 50000"							},
	{0x01, 0x01, 0x42, 0x42, "50000 100000"							},

	{0   , 0xfe, 0   ,    4, "Lives"								},
	{0x01, 0x01, 0x81, 0x00, "2"									},
	{0x01, 0x01, 0x81, 0x80, "3"									},
	{0x01, 0x01, 0x81, 0x01, "4"									},
	{0x01, 0x01, 0x81, 0x81, "5"									},

	{0   , 0xfe, 0   ,    2, "Clear Credits on Reset"				},
	{0x02, 0x01, 0x04, 0x04, "Off"									},
	{0x02, 0x01, 0x04, 0x00, "On"									},
};

STDDIPINFO(Stuntair)

static void __fastcall stuntair_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xe000:
			// coin counter = data & 3
		return;

		case 0xe800:
			bg_scrollx = data;
		return;

		case 0xf000: return; // nop
		case 0xf001: { nmi_enable = data & 1; if (!nmi_enable) ZetSetIRQLine(0x20, CPU_IRQSTATUS_NONE); }return;
		case 0xf002: return;
		case 0xf003: spritebank = (spritebank & 2) | (data & 1); return;
		case 0xf004: return;
		case 0xf005: spritebank = (spritebank & 1) | ((data & 1) << 1); return;
		case 0xf006: return;
		case 0xf007: return;

		case 0xfc03:
			soundlatch = data;
			ZetSetIRQLine(1, 0x20, (data & 0x80) ? CPU_IRQSTATUS_NONE : CPU_IRQSTATUS_ACK);
		return;
	}
}

static UINT8 __fastcall stuntair_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xe000:
			return DrvDips[1];

		case 0xe800:
			return DrvDips[0];

		case 0xf000:
			return DrvInputs[0];

		case 0xf002:
			return (DrvInputs[1] & 0xfb) | (DrvDips[2] & 4);

		case 0xf003:
			BurnWatchdogRead();
			return 0;
	}

	return 0;
}

static void __fastcall stuntair_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x03:
			AY8910Write(1, 0, data);
		return;

		case 0x07:
			AY8910Write(1, 1, data);
		return;

		case 0x0c:
		case 0x0d:
			AY8910Write(0, port & 1, data);
		return;
	}
}

static UINT8 __fastcall stuntair_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x0c:
		case 0x0d:
			return AY8910Read(0);
	}

	return 0;
}

static tilemap_callback( bg )
{
	INT32 attr = DrvBgARAM[offs];
	INT32 code = DrvBgVRAM[offs] + ((attr & 0x08) << 5);

	TILE_SET_INFO(1, code, attr, 0);
}

static tilemap_callback( fg0 )
{
	INT32 attr = DrvFgRAM[offs];

	TILE_SET_INFO(0, attr & 0x7f, 0, TILE_GROUP((~attr & 0x80) >> 7));
}

static UINT8 AY8910_0_portA(UINT32)
{
	return soundlatch;
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	ZetOpen(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	ZetClose();

	AY8910Reset(0);
	AY8910Reset(1);

	bg_scrollx = 0;
	soundlatch = 0;
	spritebank = 0;
	nmi_enable = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM[0]	= Next; Next += 0x00a000;
	DrvZ80ROM[1]	= Next; Next += 0x00a000;

	DrvGfxROM[0]	= Next; Next += 0x010000;
	DrvGfxROM[1]	= Next; Next += 0x010000;
	DrvGfxROM[2]	= Next; Next += 0x010000;

	DrvColPROM		= Next; Next += 0x000200;

	DrvPalette		= (UINT32*)Next; Next += 0x0030 * sizeof(UINT32);

	DrvNVRAM		= Next; Next += 0x000800;

	AllRam			= Next;

	DrvBgARAM		= Next; Next += 0x000400;
	DrvBgVRAM		= Next; Next += 0x000400;
	DrvSprRAM		= Next; Next += 0x000800;
	DrvFgRAM		= Next; Next += 0x000400;
	DrvZ80RAM		= Next; Next += 0x000400;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[2] = { 0, 0x2000*8 };
	INT32 XOffs[16] = { STEP8(0,1), STEP8(64,1) };
	INT32 YOffs[16] = { STEP8(0,8), STEP8(128,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x4000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM[0], 0x2000);

	GfxDecode(0x0400, 1,  8,  8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM[0]);

	memcpy (tmp, DrvGfxROM[1], 0x4000);

	GfxDecode(0x0400, 2,  8,  8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM[1]);

	memcpy (tmp, DrvGfxROM[2], 0x4000);

	GfxDecode(0x0100, 2, 16, 16, Plane, XOffs, YOffs, 0x100, tmp, DrvGfxROM[2]);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x2000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x4000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x6000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x8000, k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM[1] + 0x0000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[0] + 0x0000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[1] + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x2000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[2] + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x2000, k++, 1)) return 1;

		if (BurnLoadRom(DrvColPROM   + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x0100, k++, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM[0],		0x0000, 0x9fff, MAP_ROM);
	ZetMapMemory(DrvNVRAM,			0xc000, 0xc7ff, MAP_RAM);
	ZetMapMemory(DrvBgARAM,			0xc800, 0xcbff, MAP_RAM);
	ZetMapMemory(DrvBgVRAM,			0xd000, 0xd3ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0xd800, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvFgRAM,			0xf800, 0xfbff, MAP_RAM);
	ZetSetWriteHandler(stuntair_main_write);
	ZetSetReadHandler(stuntair_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM[1],		0x0000, 0x9fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,			0x4000, 0x43ff, MAP_RAM);
	ZetSetOutHandler(stuntair_sound_write_port);
	ZetSetInHandler(stuntair_sound_read_port);
	ZetClose();

	BurnWatchdogInit(DrvDoReset, -1/*180*/);

	AY8910Init(0, 1536000, 0);
	AY8910Init(1, 1536000, 0);
	AY8910SetPorts(0, AY8910_0_portA, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.20, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.20, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, fg0_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 1, 8, 8, 0x10000, 0x20, 1);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 2, 8, 8, 0x10000, 0x00, 7);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);
	GenericTilemapSetTransparent(1, 0);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	ZetExit();
	AY8910Exit(0);
	AY8910Exit(1);
	BurnWatchdogExit();
	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0xe0; i < 0x100; i++) {
		INT32 data = (DrvColPROM[i] & 0xf) | (DrvColPROM[i + 0x100] << 4);

		INT32 r = (data >> 0) & 7;
		INT32 g = (data >> 3) & 7;
		INT32 b = (data >> 6) & 3;

		r = (r << 5) | (r << 2) | (r >> 1);
		g = (g << 5) | (g << 2) | (g >> 1);
		b = (b << 6) | (b << 4) | (b << 2) | b;

		DrvPalette[i - 0xe0] = BurnHighCol(r, g, b, 0);
	}

	DrvPalette[0x20] = 0;
	DrvPalette[0x21] = BurnHighCol(0xff, 0xff, 0xff, 0);
}

static void draw_sprites()
{
	for (INT32 i = 0; i < 0x400; i+=0x10)
	{
		INT32 sy     = DrvSprRAM[i + 0];
		INT32 attr   = DrvSprRAM[i + 1];
		INT32 color  = DrvSprRAM[i + 4];
		INT32 sx     = DrvSprRAM[i + 5];
		INT32 code   =(attr & 0x3f) | (spritebank << 6);
		INT32 flipy  = attr & 0x80;

		Draw16x16MaskTile(pTransDraw, code, sx, (240 - sy) - 16, 0, flipy, color & 7, 2, 0, 0, DrvGfxROM[2]);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	GenericTilemapSetScrollX(0, bg_scrollx);

	if (~nBurnLayer & 1) BurnTransferClear();

	if ( nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);
	if ( nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, TMAP_SET_GROUP(1));

	if ( nSpriteEnable & 1) draw_sprites();

	if ( nBurnLayer & 4) GenericTilemapDraw(1, pTransDraw, TMAP_SET_GROUP(0) | TMAP_DRAWOPAQUE);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 70;
	INT32 nCyclesTotal[2] = { 3072000 / 60, 3072000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		CPU_RUN(0, Zet);
		if (i == (nInterleave - 1)) ZetNmi();
		ZetClose();

		ZetOpen(1);
		CPU_RUN(1, Zet);
		if ((i % 10) == 9) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD); // 7x / frame
		ZetClose();
	}

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
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
		AY8910Scan(nAction, pnMin);
		BurnWatchdogScan(nAction);

		SCAN_VAR(soundlatch);
		SCAN_VAR(nmi_enable);
		SCAN_VAR(spritebank);
		SCAN_VAR(bg_scrollx);
	}

	if (nAction & ACB_NVRAM) {
		ba.Data		= DrvNVRAM;
		ba.nLen		= 0x00800;
		ba.nAddress	= 0;
		ba.szName	= "NV RAM";
		BurnAcb(&ba);
	}

	return 0;
}


// Stunt Air

static struct BurnRomInfo stuntairRomDesc[] = {
	{ "stuntair.a0",	0x2000, 0xf61c4a1d, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "stuntair.a1",	0x2000, 0x1546f041, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "stuntair.a3",	0x2000, 0x63d00b97, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "stuntair.a4",	0x2000, 0x01fe2697, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "stuntair.a6",	0x2000, 0x6704d05c, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "stuntair.e14",	0x2000, 0x641fc9db, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 #1 Code

	{ "stuntair.a9",	0x2000, 0xbfd861f5, 3 | BRF_GRA },           //  6 Foreground Tiles

	{ "stuntair.a11",	0x2000, 0x421fef4c, 4 | BRF_GRA },           //  7 Background Tiles
	{ "stuntair.a12",	0x2000, 0xe6ee7489, 4 | BRF_GRA },           //  8

	{ "stuntair.a13",	0x2000, 0xbfdc0d38, 5 | BRF_GRA },           //  9 Sprites
	{ "stuntair.a15",	0x2000, 0x4531cab5, 5 | BRF_GRA },           // 10

	{ "dm74s287n.11m",	0x0100, 0xd330ff90, 6 | BRF_GRA },           // 11 Color Data
	{ "dm74s287n.11l",	0x0100, 0x6c98f964, 6 | BRF_GRA },           // 12

	{ "dm74s288n.7a",	0x0020, 0x5779e751, 0 | BRF_OPT },           // 13 Unknown
};

STD_ROM_PICK(stuntair)
STD_ROM_FN(stuntair)

struct BurnDriver BurnDrvStuntair = {
	"stuntair", NULL, NULL, NULL, "1983",
	"Stunt Air\0", NULL, "Nuova Videotron", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, stuntairRomInfo, stuntairRomName, NULL, NULL, NULL, NULL, StuntairInputInfo, StuntairDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x22,
	224, 256, 3, 4
};
