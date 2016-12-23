// FB Alpha Submarine (Sigma) driver module
// Based on MAME driver by Angelo Salese and David Haywood

#include "tiles_generic.h"
#include "z80_intf.h"
#include "driver.h"
extern "C" {
#include "ay8910.h"
}

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvLutPROM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvVidRAM;
static UINT8 *DrvAttrRAM;
static UINT8 *DrvSprRAM0;
static UINT8 *DrvScrollY;
static UINT8 *DrvSprRAM1;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT16 *pAY8910Buffer[6];

static UINT8 *soundlatch;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static UINT8 sound_nmi_mask;

static struct BurnInputInfo SubInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Sub)

static struct BurnDIPInfo SubDIPList[]=
{
	{0x10, 0xff, 0xff, 0x1f, NULL			},
	{0x11, 0xff, 0xff, 0xbc, NULL			},

	{0   , 0xfe, 0   ,    11, "Coinage"		},
	{0x10, 0x01, 0xf0, 0x40, "3 Coins 1 Credits"	},
	{0x10, 0x01, 0xf0, 0x20, "2 Coins 1 Credits"	},
	{0x10, 0x01, 0xf0, 0x10, "1 Coin  1 Credits"	},
	{0x10, 0x01, 0xf0, 0x60, "4 Coins 5 Credits"	},
	{0x10, 0x01, 0xf0, 0x50, "2 Coins 3 Credits"	},
	{0x10, 0x01, 0xf0, 0x30, "1 Coin  2 Credits"	},
	{0x10, 0x01, 0xf0, 0x70, "1 Coin  3 Credits"	},
	{0x10, 0x01, 0xf0, 0xf0, "1 Coin  4 Credits"	},
	{0x10, 0x01, 0xf0, 0x80, "1 Coin  5 Credits"	},
	{0x10, 0x01, 0xf0, 0x90, "1 Coin  6 Credits"	},
	{0x10, 0x01, 0xf0, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x11, 0x01, 0x03, 0x00, "3"			},
	{0x11, 0x01, 0x03, 0x01, "4"			},
	{0x11, 0x01, 0x03, 0x02, "5"			},
	{0x11, 0x01, 0x03, 0x03, "6"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x11, 0x01, 0x10, 0x10, "Off"			},
	{0x11, 0x01, 0x10, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x11, 0x01, 0x20, 0x20, "Upright"		},
	{0x11, 0x01, 0x20, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x11, 0x01, 0x40, 0x00, "Off"			},
	{0x11, 0x01, 0x40, 0x40, "On"			},
	
};

STDDIPINFO(Sub)

static UINT8 __fastcall sub_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xf000:
			return DrvDips[0];

		case 0xf020:
			return DrvDips[1];

		case 0xf040:
			return DrvInputs[0] ^ 0xc0;

		case 0xf060:
			return DrvInputs[1];
	}

	return 0;
}

static void __fastcall sub_main_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			soundlatch[0] = data;
			ZetClose();
			ZetOpen(1);
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
			ZetClose();
			ZetOpen(0);
		return;
	}
}

static UINT8 __fastcall sub_main_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
			return soundlatch[1];
	}

	return 0;
}

static void __fastcall sub_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x6000:
			sound_nmi_mask = data & 1;
		return;
	}
}

static void __fastcall sub_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			soundlatch[1] = data;
		return;

		case 0x40:
		case 0x41:
			AY8910Write(0, port & 1, data);
		return;

		case 0x80:
		case 0x81:
			AY8910Write(1, port & 1, data);
		return;
	}
}

static UINT8 __fastcall sub_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			return soundlatch[0];

		case 0x40:
		case 0x41:
			return AY8910Read(0);

		case 0x80:
		case 0x81:
			return AY8910Read(1);
	}

	return 0;
}

static tilemap_callback( background )
{
	INT32 attr = DrvAttrRAM[offs];

	TILE_SET_INFO(0, DrvVidRAM[offs] + ((attr & 0xe0) << 3), attr & 0x1f, 0);
}

static INT32 DrvDoReset()
{
	ZetOpen(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	ZetClose();

	AY8910Reset(0);
	AY8910Reset(1);

	sound_nmi_mask = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x00b000;
	DrvZ80ROM1		= Next; Next += 0x002000;

	DrvGfxROM0		= Next; Next += 0x020000;
	DrvGfxROM1		= Next; Next += 0x020000;

	DrvColPROM		= Next; Next += 0x000300;
	DrvLutPROM		= Next; Next += 0x000800;

	DrvPalette		= (UINT32*)Next; Next += 0x0400 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM0		= Next; Next += 0x001000;
	DrvZ80RAM1		= Next; Next += 0x000800;
	DrvVidRAM		= Next; Next += 0x000400;
	DrvAttrRAM		= Next; Next += 0x000400;
	DrvSprRAM0		= Next; Next += 0x000100;
	DrvScrollY		= Next + 0x40;
	DrvSprRAM1		= Next; Next += 0x000100;

	soundlatch		= Next; Next += 0x000002;

	RamEnd			= Next;

	pAY8910Buffer[0]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[1]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[2]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[3]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[4]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[5]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[3]  = { RGN_FRAC(0xc000, 2,3), RGN_FRAC(0xc000, 1,3), RGN_FRAC(0xc000, 0,3) };
	INT32 XOffs[16] = { STEP8(64,1), STEP8(0,1) };
	INT32 YOffs[32] = { STEP8(55*8,-8), STEP8(39*8,-8), STEP8(23*8,-8), STEP8(7*8,-8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0xc000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0xc000);

	GfxDecode(0x0800, 3,  8,  8, Plane, XOffs + 8, YOffs + 24, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0xc000);

	GfxDecode(0x0100, 3, 16, 32, Plane, XOffs + 0, YOffs +  0, 0x200, tmp, DrvGfxROM1);

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
		if (BurnLoadRom(DrvZ80ROM0 + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x4000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x8000,  2, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x0000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x0000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x4000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x8000,  6, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x0000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x4000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x8000,  9, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x0200, 10, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0100, 11, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0000, 12, 1)) return 1;

		if (BurnLoadRom(DrvLutPROM + 0x0000, 13, 1)) return 1;
		if (BurnLoadRom(DrvLutPROM + 0x0200, 14, 1)) return 1;
		if (BurnLoadRom(DrvLutPROM + 0x0400, 15, 1)) return 1;
		if (BurnLoadRom(DrvLutPROM + 0x0600, 16, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0xafff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,	0xb000, 0xbfff, MAP_RAM);
	ZetMapMemory(DrvAttrRAM,	0xc000, 0xc3ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,		0xc400, 0xc7ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM0,	0xd000, 0xd0ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM1,	0xd800, 0xd8ff, MAP_RAM);
	ZetSetReadHandler(sub_main_read);
	ZetSetInHandler(sub_main_read_port);
	ZetSetOutHandler(sub_main_write_port);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvZ80ROM1,	0x2000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,	0x4000, 0x47ff, MAP_RAM);
	ZetSetWriteHandler(sub_sound_write);
	ZetSetInHandler(sub_sound_read_port);
	ZetSetOutHandler(sub_sound_write_port);
	ZetClose();

	AY8910Init(0, 1536000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910Init(1, 1536000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.23, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.23, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, background_map_callback, 8, 8, 32, 32);
	GenericTilemapSetOffsets(0, 0, -16);
	GenericTilemapSetScrollCols(0, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 3, 8, 8, 0x20000, 0x200, 0x1f);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	AY8910Exit(0);
	AY8910Exit(1);

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	UINT32 tmp[0x100];

	for (INT32 i = 0; i < 0x100; i++)
	{
		INT32 r = DrvColPROM[i + 0x000] & 0xf;
		INT32 g = DrvColPROM[i + 0x100] & 0xf;
		INT32 b = DrvColPROM[i + 0x200] & 0xf;

		tmp[i] = BurnHighCol(r+r*16,g+g*16,b+b*16,0);
	}

	for (INT32 i = 0; i < 0x400; i++)
	{
		DrvPalette[i] = tmp[DrvLutPROM[i + 0x400] + (DrvLutPROM[i] << 4)];
	}
}

static void draw_sprites()
{
	for (INT32 i = 0;i < 0x40; i+=2)
	{
		INT32 code  = DrvSprRAM0[i+1];
		INT32 sx    = DrvSprRAM0[i+0];
		INT32 sy    = (0xe0 - DrvSprRAM1[i+1]) - 16;
		INT32 color = DrvSprRAM1[i+0] & 0x3f;
		INT32 flipx =~DrvSprRAM1[i+0] & 0x80;
		INT32 flipy =~DrvSprRAM1[i+0] & 0x40;

		if (flipx) {
			sx = 0xe0 - sx;
		}

		if (flipy) {
			if (flipx) {
				RenderCustomTile_Mask_FlipXY_Clip(pTransDraw, 16, 32, code, sx, sy, color, 3, 0, 0, DrvGfxROM1);
			} else {
				RenderCustomTile_Mask_FlipY_Clip(pTransDraw, 16, 32, code, sx, sy, color, 3, 0, 0, DrvGfxROM1);
			}
		} else {
			if (flipx) {
				RenderCustomTile_Mask_FlipX_Clip(pTransDraw, 16, 32, code, sx, sy, color, 3, 0, 0, DrvGfxROM1);
			} else {
				RenderCustomTile_Mask_Clip(pTransDraw, 16, 32, code, sx, sy, color, 3, 0, 0, DrvGfxROM1);
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	BurnTransferClear();

	for (INT32 i = 0; i < 32; i++) {
		GenericTilemapSetScrollCol(0, i, DrvScrollY[i]);
	}

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);

	if (nBurnLayer & 2) draw_sprites();

	GenericTilesSetClip(28*8, -1, -1, -1);
	if (nBurnLayer & 4) GenericTilemapDraw(0, pTransDraw, 0);
	GenericTilesClearClip();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0, 2);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 16;
	INT32 nCyclesTotal[2] = { 3072000 / 60, 3072000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);
		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(1);
		nCyclesDone[1] += ZetRun(nCyclesTotal[1] / nInterleave);

		if ((i % (nInterleave / 2)) == ((nInterleave / 2) - 1)) {
			if (sound_nmi_mask) {
				ZetNmi();
			}
		}

		ZetClose();
	}

	if (pBurnSoundOut) {
		AY8910Render(&pAY8910Buffer[0], pBurnSoundOut, nBurnSoundLen, 0);
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

		SCAN_VAR(sound_nmi_mask);
	}

	return 0;
}



// Submarine (Sigma)

static struct BurnRomInfo subRomDesc[] = {
	{ "temp 1 pos b6 27128.bin",		0x4000, 0x6875b31d, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "temp 2 pos c6 27128.bin",		0x4000, 0xbc7f8f43, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "temp 3 pos d6 2764.bin",		0x2000, 0x3546c226, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "m sound pos f14 2764.bin",		0x2000, 0x61536a97, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #0 Code

	{ "vram 1 pos f12 27128  version3.bin",	0x4000, 0x8d176ba0, 3 | BRF_GRA },           //  4 Background Tiles
	{ "vram 2 pos f14 27128  version3.bin",	0x4000, 0x0677cf3a, 3 | BRF_GRA },           //  5
	{ "vram 3 pos f15 27128  version3.bin",	0x4000, 0x9a4cd1a0, 3 | BRF_GRA },           //  6

	{ "obj 1 pos h1 27128  version3.bin",	0x4000, 0x63173e65, 4 | BRF_GRA },           //  7 Sprites
	{ "obj 2 pos h3 27128  version3.bin",	0x4000, 0x3898d1a8, 4 | BRF_GRA },           //  8
	{ "obj 3 pos h4 27128  version3.bin",	0x4000, 0x304e2145, 4 | BRF_GRA },           //  9

	{ "prom pos a9 n82s129",		0x0100, 0x8df9cefe, 5 | BRF_GRA },           // 10 Color data
	{ "prom pos a10 n82s129",		0x0100, 0x3c834094, 5 | BRF_GRA },           // 11
	{ "prom pos a11 n82s129",		0x0100, 0x339afa95, 5 | BRF_GRA },           // 12

	{ "prom pos e5 n82s131",		0x0200, 0x0024b5dd, 6 | BRF_GRA },           // 13 Color lookup data
	{ "prom pos c7 n82s129",		0x0100, 0x9072d259, 6 | BRF_GRA },           // 14
	{ "prom pos e4 n82s131",		0x0200, 0x307aa2cf, 6 | BRF_GRA },           // 15
	{ "prom pos c8 n82s129",		0x0100, 0x351e1ef8, 6 | BRF_GRA },           // 16
};

STD_ROM_PICK(sub)
STD_ROM_FN(sub)

struct BurnDriver BurnDrvSub = {
	"sub", NULL, NULL, NULL, "1985",
	"Submarine (Sigma)\0", NULL, "Sigma Enterprises Inc.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, subRomInfo, subRomName, NULL, NULL, SubInputInfo, SubDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	224, 256, 3, 4
};
