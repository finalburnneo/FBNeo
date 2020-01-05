#include "tiles_generic.h"
#include "bitswap.h"
#include "z80_intf.h"
#include "ay8910.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM[3];
static UINT8 *DrvGfxROM[5];
static UINT8 *DrvZ80RAM[2];
static UINT8 *DrvPalRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 *bg_base;
static UINT8 soundlatch;
static UINT8 flipscreen;
static UINT8 calorie_bg;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvReset;
static UINT8 DrvInputs[3];

static struct BurnInputInfo CalorieInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Calorie)

static struct BurnDIPInfo CalorieDIPList[]=
{
	{0x11, 0xff, 0xff, 0x30, NULL							},
	{0x12, 0xff, 0xff, 0x40, NULL							},

	{0   , 0xfe, 0   ,    4, "Coin A"						},
	{0x11, 0x01, 0x03, 0x00, "1 Coin 1 Credits"				},
	{0x11, 0x01, 0x03, 0x01, "1 Coin 2 Credits"				},
	{0x11, 0x01, 0x03, 0x02, "1 Coin 3 Credits"				},
	{0x11, 0x01, 0x03, 0x03, "1 Coin 6 Credits"				},

	{0   , 0xfe, 0   ,    4, "Coin B"						},
	{0x11, 0x01, 0x0c, 0x0c, "2 Coins 1 Credits"			},
	{0x11, 0x01, 0x0c, 0x00, "1 Coin 1 Credits"				},
	{0x11, 0x01, 0x0c, 0x04, "1 Coin 2 Credits"				},
	{0x11, 0x01, 0x0c, 0x08, "1 Coin 3 Credits"				},

	{0   , 0xfe, 0   ,    2, "Cabinet"						},
	{0x11, 0x01, 0x10, 0x10, "Upright"						},
	{0x11, 0x01, 0x10, 0x00, "Cocktail"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x11, 0x01, 0x20, 0x00, "Off"							},
	{0x11, 0x01, 0x20, 0x20, "On"							},

	{0   , 0xfe, 0   ,    4, "Lives"						},
	{0x11, 0x01, 0xc0, 0xc0, "2"							},
	{0x11, 0x01, 0xc0, 0x00, "3"							},
	{0x11, 0x01, 0xc0, 0x40, "4"							},
	{0x11, 0x01, 0xc0, 0x80, "5"							},

	{0   , 0xfe, 0   ,    3, "Bonus Life"					},
	{0x12, 0x01, 0x03, 0x00, "None"							},
	{0x12, 0x01, 0x03, 0x01, "20,000 Only"					},
	{0x12, 0x01, 0x03, 0x03, "20,000 and 60,000"			},

	{0   , 0xfe, 0   ,    2, "Number of Bombs"				},
	{0x12, 0x01, 0x04, 0x00, "3"							},
	{0x12, 0x01, 0x04, 0x04, "5"							},

	{0   , 0xfe, 0   ,    2, "Difficulty - Mogura Nian"		},
	{0x12, 0x01, 0x08, 0x00, "Normal"						},
	{0x12, 0x01, 0x08, 0x08, "Hard"							},

	{0   , 0xfe, 0   ,    4, "Difficulty - Select of Mogura"},
	{0x12, 0x01, 0x30, 0x00, "Easy"							},
	{0x12, 0x01, 0x30, 0x20, "Normal"						},
	{0x12, 0x01, 0x30, 0x10, "Hard"							},
	{0x12, 0x01, 0x30, 0x30, "Hardest"						},

	{0   , 0xfe, 0   ,    0, "Infinite Lives"				},
	{0x12, 0x01, 0x80, 0x00, "Off"							},
	{0x12, 0x01, 0x80, 0x80, "On"							},
};

STDDIPINFO(Calorie)

static void palette_write(INT32 offset)
{
	offset &= 0xfe;

	INT32 r = DrvPalRAM[offset + 0] & 0xf;
	INT32 g = DrvPalRAM[offset + 0] >> 4;
	INT32 b = DrvPalRAM[offset + 1] & 0xf;

	DrvPalette[offset / 2] = BurnHighCol(r + (r * 16), g + (g * 16), b + (b * 16), 0);
}

static void __fastcall calorie_write(UINT16 address, UINT8 data)
{
	if ((address & 0xff00) == 0xdc00) {
		DrvPalRAM[address & 0x0ff] = data;
		palette_write(address);
		return;
	}

	switch (address)
	{
		case 0xde00:
			calorie_bg = data;
		return;

		case 0xf004:
			flipscreen = data & 1;
		return;

		case 0xf800:
			soundlatch = data;
		return;
	}
}

static UINT8 __fastcall calorie_read(UINT16 address)
{
	switch (address)
	{
		case 0xf000:
			return DrvInputs[0];

		case 0xf001:
			return DrvInputs[1];

		case 0xf002:
			return DrvInputs[2];

		case 0xf004:
			return DrvDips[0];

		case 0xf005:
			return DrvDips[1];
	}

	return 0;
}

static UINT8 __fastcall calorie_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xc000:
			UINT8 latch = soundlatch;
			soundlatch = 0;
			return latch;
	}

	return 0;
}

static UINT8 __fastcall calorie_sound_in(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x01:
		case 0x11:
			return AY8910Read((port >> 4) & 1);
	}

	return 0;
}

static void __fastcall calorie_sound_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
		case 0x10:
		case 0x11:
			AY8910Write((port >> 4) & 1, port & 1, data);
		return;
	}
}

static tilemap_callback( bg )
{
	INT32 attr = bg_base[offs + 0x100];
	INT32 code = bg_base[offs] | ((attr << 4) & 0x100);

	TILE_SET_INFO(3, code, attr, (attr & 0x40) ? TILE_FLIPX : 0);
}

static tilemap_callback( fg )
{
	INT32 attr = DrvVidRAM[offs + 0x400];
	INT32 code = DrvVidRAM[offs] | ((attr << 4) & 0x300);

	TILE_SET_INFO(2, code, attr, TILE_FLIPYX(attr >> 6));
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	ZetClose();

	AY8910Reset(0);
	AY8910Reset(1);

	soundlatch = 0;
	flipscreen = 0;
	calorie_bg = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM[0]	= Next; Next += 0x010000;
	DrvZ80ROM[2]	= Next; Next += 0x010000;
	DrvZ80ROM[1]	= Next; Next += 0x010000;

	DrvGfxROM[4]	= Next; Next += 0x002000;
	DrvGfxROM[0]	= Next; Next += 0x020000;
	DrvGfxROM[1]	= Next; Next += 0x020000;
	DrvGfxROM[2]	= Next; Next += 0x020000;
	DrvGfxROM[3]	= Next; Next += 0x020000;

	DrvPalette		= (UINT32*)Next; Next += 0x0080 * sizeof(INT32);

	AllRam			= Next;

	DrvSprRAM		= Next; Next += 0x000400;
	DrvPalRAM		= Next; Next += 0x000100;
	DrvVidRAM		= Next; Next += 0x000800;

	DrvZ80RAM[0]	= Next; Next += 0x001000;
	DrvZ80RAM[1]	= Next; Next += 0x000800;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[3] = { RGN_FRAC(0xc000, 0, 3), RGN_FRAC(0xc000, 1, 3), RGN_FRAC(0xc000, 2, 3) };
	INT32 Plane1[3] = { RGN_FRAC(0x6000, 0, 3), RGN_FRAC(0x6000, 1, 3), RGN_FRAC(0x6000, 2, 3) };

	INT32 XOffs[32] = { STEP8(0,1), STEP8(64,1), STEP8(256,1), STEP8(256+64,1) };
	INT32 YOffs[32] = { STEP8(0,8), STEP8(128,8), STEP8(512,8), STEP8(640,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0xc000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM[0], 0xc000);

	GfxDecode(0x0200, 3, 16, 16, Plane0, XOffs, YOffs, 0x100, tmp, DrvGfxROM[0]);
	GfxDecode(0x0080, 3, 32, 32, Plane0, XOffs, YOffs, 0x400, tmp, DrvGfxROM[1]);

	memcpy (tmp, DrvGfxROM[2], 0xc000);

	GfxDecode(0x0400, 3,  8,  8, Plane1, XOffs, YOffs, 0x040, tmp, DrvGfxROM[2]);

	memcpy (tmp, DrvGfxROM[3], 0xc000);

	GfxDecode(0x0200, 3, 16, 16, Plane0, XOffs, YOffs, 0x100, tmp, DrvGfxROM[3]);

	BurnFree (tmp);

	return 0;
}

static INT32 DrvInit(void (*pInitCallback)())
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM[0] + 0x0000,	0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x4000,	1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x8000,	2, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM[1] + 0x0000,	3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[4] + 0x0000,	4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[0] + 0x0000,	5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x4000,	6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x8000,	7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[2] + 0x0000,	8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x2000,	9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x4000, 10, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[3] + 0x0000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[3] + 0x4000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[3] + 0x8000, 13, 1)) return 1;

		DrvGfxDecode();
	}

	if (pInitCallback) {
		pInitCallback();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM[0],		0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80ROM[2],		0x0000, 0x7fff, MAP_FETCHOP);
	ZetMapMemory(DrvZ80RAM[0],		0xc000, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,			0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0xd800, 0xdbff, MAP_RAM);
	ZetSetWriteHandler(calorie_write);
	ZetSetReadHandler(calorie_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM[1],		0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM[1],		0x8000, 0x87ff, MAP_RAM);
	ZetSetReadHandler(calorie_sound_read);
	ZetSetOutHandler(calorie_sound_out);
	ZetSetInHandler(calorie_sound_in);
	ZetClose();

	AY8910Init(0, 1500000, 0);
	AY8910Init(1, 1500000, 1);
	AY8910SetAllRoutes(0, 0.20, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.20, BURN_SND_ROUTE_BOTH);
	AY8910SetBuffered(ZetTotalCycles, 3000000);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 16, 16, 16, 16);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, fg_map_callback,  8,  8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 3, 16, 16, 0x20000, 0, 0xf);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 3, 32, 32, 0x20000, 0, 0xf);
	GenericTilemapSetGfx(2, DrvGfxROM[2], 3,  8,  8, 0x20000, 0, 0xf);
	GenericTilemapSetGfx(3, DrvGfxROM[3], 3, 16, 16, 0x20000, 0, 0xf);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	AY8910Exit(0);
	AY8910Exit(1);

	BurnFree (AllMem);

	return 0;
}

static void draw_sprites()
{
	for (INT32 x = 0x400; x >= 0; x -= 4)
	{
		INT32 code  = DrvSprRAM[x+0];
		INT32 color = DrvSprRAM[x+1] & 0x0f;
		INT32 flipx = DrvSprRAM[x+1] & 0x40;
		INT32 size  =(DrvSprRAM[x+1] & 0x10) >> 4;
		INT32 flipy = 0;
		INT32 sy = 0xff - DrvSprRAM[x+2];
		INT32 sx = DrvSprRAM[x+3];

		if (flipscreen)
		{
			if (DrvSprRAM[x+1] & 0x10 )
				sy = 0xff - sy + 32;
			else
				sy = 0xff - sy + 16;

			sx = 0xff - sx - 16;
			flipx = !flipx;
			flipy = !flipy;
		}

		GenericTilesGfx *gfx = &GenericGfxData[size];

		if (size) {
			code |= 0x40;
			sy -= 16;
		}

		DrawCustomMaskTile(pTransDraw, gfx->width, gfx->height, code % gfx->code_mask, sx, sy - 31, flipx, flipy, color & gfx->color_mask, gfx->depth, 0, gfx->color_offset, gfx->gfxbase);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x100; i+=2) {
			palette_write(i);
		}
		DrvRecalc = 0;
	}

	bg_base = DrvGfxROM[4] + (calorie_bg & 0x0f) * 0x200;

	GenericTilemapSetFlip(0, flipscreen);

	if (calorie_bg & 0x10)
	{
		GenericTilemapDraw(0, pTransDraw, 0);
	}
	else
	{
		BurnTransferClear();
	}

	GenericTilemapDraw(1, pTransDraw, 0);

	draw_sprites();

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
		memset (DrvInputs, 0, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 4000000 / 60, 3000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		CPU_RUN(0, Zet);
		if (i == nInterleave - 1) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(1);
		CPU_RUN(1, Zet);
		if (i == nInterleave - 1) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();
	}

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
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
		*pnMin = 0x029735;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);
		AY8910Scan(nAction, pnMin);

		SCAN_VAR(soundlatch);
		SCAN_VAR(flipscreen);
		SCAN_VAR(calorie_bg);
	}

	return 0;
}


// Calorie Kun vs Moguranian

static struct BurnRomInfo calorieRomDesc[] = {
	{ "epr10072.1j",	0x4000, 0xade792c1, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code (Encrypted)
	{ "epr10073.1k",	0x4000, 0xb53e109f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr10074.1m",	0x4000, 0xa08da685, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "epr10075.4d",	0x4000, 0xca547036, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "epr10079.8d",	0x2000, 0x3c61a42c, 3 | BRF_GRA },           //  4 Background Tilemap

	{ "epr10071.7m",	0x4000, 0x5f55527a, 4 | BRF_GRA },           //  5 Sprites (16x16 & 32x32)
	{ "epr10070.7k",	0x4000, 0x97f35a23, 4 | BRF_GRA },           //  6
	{ "epr10069.7j",	0x4000, 0xc0c3deaf, 4 | BRF_GRA },           //  7

	{ "epr10082.5r",	0x2000, 0x5984ea44, 5 | BRF_GRA },           //  8 Foreground Tiles
	{ "epr10081.4r",	0x2000, 0xe2d45dd8, 5 | BRF_GRA },           //  9
	{ "epr10080.3r",	0x2000, 0x42edfcfe, 5 | BRF_GRA },           // 10

	{ "epr10078.7d",	0x4000, 0x5b8eecce, 6 | BRF_GRA },           // 11 Background Tiles
	{ "epr10077.6d",	0x4000, 0x01bcb609, 6 | BRF_GRA },           // 12
	{ "epr10076.5d",	0x4000, 0xb1529782, 6 | BRF_GRA },           // 13
};

STD_ROM_PICK(calorie)
STD_ROM_FN(calorie)

static void calorie_decode()
{
	static const UINT8 swaptable[24][4] =
	{
		{ 6,4,2,0 }, { 4,6,2,0 }, { 2,4,6,0 }, { 0,4,2,6 },
		{ 6,2,4,0 }, { 6,0,2,4 }, { 6,4,0,2 }, { 2,6,4,0 },
		{ 4,2,6,0 }, { 4,6,0,2 }, { 6,0,4,2 }, { 0,6,4,2 },
		{ 4,0,6,2 }, { 0,4,6,2 }, { 6,2,0,4 }, { 2,6,0,4 },
		{ 0,6,2,4 }, { 2,0,6,4 }, { 0,2,6,4 }, { 4,2,0,6 },
		{ 2,4,0,6 }, { 4,0,2,6 }, { 2,0,4,6 }, { 0,2,4,6 },
	};

	static const UINT8 data_xor[1+64] =
	{
		0x54,
		0x14,0x15,0x41,0x14,0x50,0x55,0x05,0x41,0x01,0x10,0x51,0x05,0x11,0x05,0x14,0x55,
		0x41,0x05,0x04,0x41,0x14,0x10,0x45,0x50,0x00,0x45,0x00,0x00,0x00,0x45,0x00,0x00,
		0x54,0x04,0x15,0x10,0x04,0x05,0x11,0x44,0x04,0x01,0x05,0x00,0x44,0x15,0x40,0x45,
		0x10,0x15,0x51,0x50,0x00,0x15,0x51,0x44,0x15,0x04,0x44,0x44,0x50,0x10,0x04,0x04,
	};

	static const UINT8 opcode_xor[2+64] =
	{
		0x04,
		0x44,
		0x15,0x51,0x41,0x10,0x15,0x54,0x04,0x51,0x05,0x55,0x05,0x54,0x45,0x04,0x10,0x01,
		0x51,0x55,0x45,0x55,0x45,0x04,0x55,0x40,0x11,0x15,0x01,0x40,0x01,0x11,0x45,0x44,
		0x40,0x05,0x15,0x15,0x01,0x50,0x00,0x44,0x04,0x50,0x51,0x45,0x50,0x54,0x41,0x40,
		0x14,0x40,0x50,0x45,0x10,0x05,0x50,0x01,0x40,0x01,0x50,0x50,0x50,0x44,0x40,0x10,
	};

	static const INT32 data_swap_select[1+64] =
	{
		 7, 1,11,23,17,23, 0,15,19,20,12,10, 0,18,18, 5,20, 13, 0,18,14, 5, 6,10,21, 1,11, 9, 3,21, 4, 1,17,
		 5, 7,16,13,19,23,20, 2, 10,23,23,15,10,12, 0,22, 14, 6,15,11,17,15,21, 0, 6, 1, 1,18, 5,15,15,20,
	};

	static const INT32 opcode_swap_select[2+64] =
	{
		 7, 12, 18, 8,21, 0,22,21,13,21, 20,13,20,14, 6, 3, 5,20, 8,20, 4, 8,17,22, 0, 0,  6,17,17, 9, 0,16,13,21,
		 3, 2,18, 6,11, 3, 3,18, 18,19, 3, 0, 5, 0,11, 8, 8, 1, 7, 2,10, 8,10, 2, 1, 3,12,16, 0,17,10, 1,
	};

	for (INT32 A = 0x0000; A < 0x8000; A++)
	{
		INT32 row = BITSWAP16(A, 15, 13, 11, 10, 8, 7, 5, 4, 2, 1,  14, 12, 9, 6, 3, 0) & 0x3f;

		const UINT8 *tbl = swaptable[opcode_swap_select[row]];
		DrvZ80ROM[2][A] = BITSWAP08(DrvZ80ROM[0][A],7,tbl[0],5,tbl[1],3,tbl[2],1,tbl[3]) ^ opcode_xor[row];

		tbl = swaptable[data_swap_select[row]];
		DrvZ80ROM[0][A] = BITSWAP08(DrvZ80ROM[0][A],7,tbl[0],5,tbl[1],3,tbl[2],1,tbl[3]) ^ data_xor[row];
	}
}

static INT32 CalorieInit()
{
	return DrvInit(calorie_decode);
}

struct BurnDriver BurnDrvCalorie = {
	"calorie", NULL, NULL, NULL, "1986",
	"Calorie Kun vs Moguranian\0", NULL, "Sega", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, calorieRomInfo, calorieRomName, NULL, NULL, NULL, NULL, CalorieInputInfo, CalorieDIPInfo,
	CalorieInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	256, 224, 4, 3
};


// Calorie Kun vs Moguranian (bootleg)

static struct BurnRomInfo caloriebRomDesc[] = {
	{ "12.bin",			0x8000, 0xcf5fa69e, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "13.bin",			0x8000, 0x52e7263f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr10074.1m",	0x4000, 0xa08da685, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "epr10075.4d",	0x4000, 0xca547036, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "epr10079.8d",	0x2000, 0x3c61a42c, 3 | BRF_GRA },           //  4 Background Tilemap

	{ "epr10071.7m",	0x4000, 0x5f55527a, 4 | BRF_GRA },           //  5 Sprites (16x16 & 32x32)
	{ "epr10070.7k",	0x4000, 0x97f35a23, 4 | BRF_GRA },           //  6
	{ "epr10069.7j",	0x4000, 0xc0c3deaf, 4 | BRF_GRA },           //  7

	{ "epr10082.5r",	0x2000, 0x5984ea44, 5 | BRF_GRA },           //  8 Foreground Tiles
	{ "epr10081.4r",	0x2000, 0xe2d45dd8, 5 | BRF_GRA },           //  9
	{ "epr10080.3r",	0x2000, 0x42edfcfe, 5 | BRF_GRA },           // 10

	{ "epr10078.7d",	0x4000, 0x5b8eecce, 6 | BRF_GRA },           // 11 Background Tiles
	{ "epr10077.6d",	0x4000, 0x01bcb609, 6 | BRF_GRA },           // 12
	{ "epr10076.5d",	0x4000, 0xb1529782, 6 | BRF_GRA },           // 13
};

STD_ROM_PICK(calorieb)
STD_ROM_FN(calorieb)

static void calorieb_decode()
{
	memset (DrvZ80ROM[0], 0x00, 0x10000);

	BurnLoadRom(DrvZ80ROM[2] + 0x0000, 0, 1);
	memcpy (DrvZ80ROM[0] + 0x0000, DrvZ80ROM[2] + 0x4000, 0x4000);

	BurnLoadRom(DrvZ80ROM[2] + 0x4000, 1, 1);
	memcpy (DrvZ80ROM[0] + 0x4000, DrvZ80ROM[2] + 0x8000, 0x4000);

	BurnLoadRom(DrvZ80ROM[0] + 0x8000, 2, 1);

	memset (DrvZ80ROM[2] + 0x8000, 0x00, 0x4000);
}

static INT32 CaloriebInit()
{
	return DrvInit(calorieb_decode);
}

struct BurnDriver BurnDrvCalorieb = {
	"calorieb", "calorie", NULL, NULL, "1986",
	"Calorie Kun vs Moguranian (bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, caloriebRomInfo, caloriebRomName, NULL, NULL, NULL, NULL, CalorieInputInfo, CalorieDIPInfo,
	CaloriebInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	256, 224, 4, 3
};
