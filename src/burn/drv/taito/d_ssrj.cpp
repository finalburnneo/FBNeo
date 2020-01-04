// FB Alpha Super Speed Race Junior driver module
// Based on MAME driver by Tomasz Slanina

#include "tiles_generic.h"
#include "z80_intf.h"
#include "ay8910.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvVidRAM0;
static UINT8 *DrvVidRAM1;
static UINT8 *DrvVidRAM2;
static UINT8 *DrvVidRAM3;
static UINT8 *DrvSprBuf;
static UINT8 *DrvScrRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvInputs[3];
static UINT8 DrvDips[2];
static UINT8 DrvReset;

static struct BurnInputInfo SsrjInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 2"	},

	{"P1 Left",		    BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 3,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Ssrj)

static struct BurnDIPInfo SsrjDIPList[]=
{
	{0x07, 0xff, 0xff, 0xc0, NULL					},
	{0x08, 0xff, 0xff, 0x48, NULL					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x07, 0x01, 0x30, 0x10, "Easy"					},
	{0x07, 0x01, 0x30, 0x00, "Normal"				},
	{0x07, 0x01, 0x30, 0x20, "Difficult"			},
	{0x07, 0x01, 0x30, 0x30, "Very Difficult"		},

	{0   , 0xfe, 0   ,    2, "Free Play"			},
	{0x07, 0x01, 0x40, 0x40, "Off"					},
	{0x07, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "No Hit"				},
	{0x07, 0x01, 0x80, 0x80, "Off"					},
	{0x07, 0x01, 0x80, 0x00, "On"					},

	{0   , 0xfe, 0   ,    7, "Coinage"				},
	{0x08, 0x01, 0x07, 0x07, "4 Coins 1 Credits"	},
	{0x08, 0x01, 0x07, 0x06, "3 Coins 1 Credits"	},
	{0x08, 0x01, 0x07, 0x05, "2 Coins 1 Credits"	},
	{0x08, 0x01, 0x07, 0x00, "1 Coin  1 Credits"	},
	{0x08, 0x01, 0x07, 0x01, "1 Coin  2 Credits"	},
	{0x08, 0x01, 0x07, 0x02, "1 Coin  3 Credits"	},
	{0x08, 0x01, 0x07, 0x03, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    2, "Freeze"				},
	{0x08, 0x01, 0x08, 0x08, "Off"					},
	{0x08, 0x01, 0x08, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Controls"				},
	{0x08, 0x01, 0x30, 0x00, "Type 1"				},
	{0x08, 0x01, 0x30, 0x10, "Type 2"				},
	{0x08, 0x01, 0x30, 0x20, "Type 3"				},
	{0x08, 0x01, 0x30, 0x30, "Type 4"				},
};

STDDIPINFO(Ssrj)

static void __fastcall ssrj_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xf400:
		case 0xf401:
			AY8910Write(0, address & 1, data);
		return;

		case 0xf003:
		case 0xf800:
		case 0xfc00:
		return;		// nop
	}
}

static UINT8 __fastcall ssrj_read(UINT16 address)
{
	switch (address)
	{
		case 0xf000:
			return (DrvInputs[0] & 0x1f) | (DrvJoy2[1] ? 0xe0 : 0x00);

		case 0xf001:
			return (DrvJoy2[2] ? 0x01 : 0x00) | (DrvJoy2[3] ? 0xff : 0x00);

		case 0xf002:
			return (DrvInputs[1] & 0x0f) | (DrvDips[0] & 0xf0);

		case 0xf401:
			return AY8910Read(0);
	}

	return 0;
}

static tilemap_callback( layer0 )
{
	INT32 attr = DrvVidRAM0[offs * 2] + (DrvVidRAM0[offs * 2 + 1] * 256);

	TILE_SET_INFO(0, attr & 0x3ff, attr >> 12, TILE_FLIPYX(attr >> 14));
}

static tilemap_callback( layer1 )
{
	INT32 attr = DrvVidRAM1[offs * 2] + (DrvVidRAM1[offs * 2 + 1] * 256);

	TILE_SET_INFO(1, attr & 0x3ff, attr >> 12, TILE_FLIPYX(attr >> 14));
}

static tilemap_callback( layer2 )
{
	INT32 attr = DrvVidRAM3[offs * 2] + (DrvVidRAM3[offs * 2 + 1] * 256);

	TILE_SET_INFO(2, attr & 0x3ff, attr >> 12, TILE_FLIPYX(attr >> 14));
}

static UINT8 AY8910_read_B(UINT32)
{
	return DrvDips[1];
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	AY8910Reset(0);

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x00c000;

	DrvGfxROM		= Next; Next += 0x010000;

	DrvPalette		= (UINT32*)Next; Next += 0x0080 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x000800;
	DrvVidRAM0		= Next; Next += 0x000800;
	DrvVidRAM1		= Next; Next += 0x000800;
	DrvVidRAM2		= Next; Next += 0x000800;
	DrvVidRAM3		= Next; Next += 0x000800;
	DrvSprBuf		= Next; Next += 0x000080;
	DrvScrRAM		= Next; Next += 0x000800;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode() // col & 0xf
{
	INT32 Plane[3] = { 0x2000 * 8 * 0, 0x2000 * 8 * 1, 0x2000 * 8 * 2 };
	INT32 XOffs[8] = { STEP8(0,1) };
	INT32 YOffs[8] = { STEP8(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x6000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM, 0x6000);

	GfxDecode(0x0400, 3, 8, 8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM);

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
		if (BurnLoadRom(DrvZ80ROM + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x4000,  1, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM + 0x0000,  2, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x2000,  3, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x4000,  4, 1)) return 1;

		// color prom not dumped :(

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvVidRAM0,	0xc000, 0xc7ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM1,	0xc800, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvVidRAM2,	0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM3,	0xd800, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM,		0xe000, 0xe7ff, MAP_RAM);
	ZetMapMemory(DrvScrRAM,		0xe800, 0xefff, MAP_RAM);
	ZetSetWriteHandler(ssrj_write);
	ZetSetReadHandler(ssrj_read);
	ZetClose();

	AY8910Init(0, 1600000, 0);
	AY8910SetPorts(0, NULL, &AY8910_read_B, NULL, NULL);
	AY8910SetAllRoutes(0, 0.20, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	GenericTilemapInit(0, TILEMAP_SCAN_COLS, layer0_map_callback, 8, 8, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_COLS, layer1_map_callback, 8, 8, 32, 32);
	GenericTilemapInit(2, TILEMAP_SCAN_COLS, layer2_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM, 3, 8, 8, 0x10000, 0x00, 3);
	GenericTilemapSetGfx(1, DrvGfxROM, 3, 8, 8, 0x10000, 0x20, 3);
	GenericTilemapSetGfx(2, DrvGfxROM, 3, 8, 8, 0x10000, 0x60, 3);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetTransparent(2, 0);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	ZetExit();
	AY8910Exit(0);

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	const UINT32 tab[128] = { // MAME has 512 entries, but only uses 128?
		0x000000, 0x2a578c, 0x000000, 0x214ba0, 0xffffff, 0x253851, 0x1f1f2f, 0x377bbe,
		0x000000, 0x006329, 0x0000ff, 0x00ff00, 0xffffff, 0xff0000, 0x002d69, 0xffff00,
		0x000000, 0x002000, 0x004000, 0x006000, 0x008000, 0x00a000, 0x00c000, 0x00f000,
		0x000000, 0x200020, 0x400040, 0x600060, 0x800080, 0xa000a0, 0xc000c0, 0xf000f0,
		0x000000, 0xff0000, 0x7f0000, 0x000000, 0x000000, 0xaf0000, 0xffffff, 0xff7f7f,
		0x000000, 0x202020, 0x404040, 0x606060, 0x808080, 0xa0a0a0, 0xc0c0c0, 0xf0f0f0,
		0x000000, 0x202020, 0x404040, 0x606060, 0x808080, 0xa0a0a0, 0xc0c0c0, 0xf0f0f0,
		0x000000, 0xff0000, 0x00009f, 0x606060, 0x000000, 0xffff00, 0x00ff00, 0xffffff,
		0x000000, 0x0000ff, 0x00007f, 0x000000, 0x000000, 0x0000af, 0xffffff, 0x7f7fff,
		0x000000, 0xffff00, 0x7f7f00, 0x000000, 0x000000, 0xafaf00, 0xffffff, 0xffff7f,
		0x000000, 0x00ff00, 0x007f00, 0x000000, 0x000000, 0x00af00, 0xffffff, 0x7fff7f,
		0x000000, 0x202020, 0x404040, 0x606060, 0x808080, 0xa0a0a0, 0xc0c0c0, 0xf0f0f0,
		0x000000, 0x202020, 0x404040, 0x606060, 0x808080, 0xa0a0a0, 0xc0c0c0, 0xf0f0f0,
		0x000000, 0x202020, 0x404040, 0x606060, 0x808080, 0xa0a0a0, 0xc0c0c0, 0xf0f0f0,
		0x000000, 0x202020, 0x404040, 0x606060, 0x808080, 0xa0a0a0, 0xc0c0c0, 0xf0f0f0,
		0x000000, 0xffafaf, 0x0000ff, 0xffffff, 0x000000, 0xff5050, 0xffff00, 0x00ff00,
	};

	for (INT32 i = 0; i < 128; i++) {
		DrvPalette[i] = BurnHighCol(tab[i]/0x10000, (tab[i]/0x100)&0xff, tab[i]&0xff, 0);
	}
}

static void draw_sprites()
{
	for (INT32 i = 0; i < 6; i++)
	{
		INT32 sy = DrvSprBuf[20*i];
		INT32 sx = DrvSprBuf[20*i+2];
		if (!DrvSprBuf[20*i+3])
		{
			for (INT32 k = 0; k < 5;k++, sx+=8)
			{
				for (INT32 j = 0; j < 0x20; j++)
				{
					INT32 offs = (i * 5 + k) * 64 + (31 - j) * 2;
					INT32 code = DrvVidRAM2[offs] + (256 * DrvVidRAM2[offs + 1]);

					INT32 yy = (247-(sy+(j<<3)))&0xff;
					INT32 flipx = code & 0x4000;
					INT32 flipy = code & 0x8000;
					INT32 color = (code & 0x3000) >> 12;

					if (flipy) {
						if (flipx) {
							Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, code & 0x3ff, sx, yy, color, 3, 0, 0x40, DrvGfxROM);
						} else {
							Render8x8Tile_Mask_FlipY_Clip(pTransDraw, code & 0x3ff, sx, yy, color, 3, 0, 0x40, DrvGfxROM);
						}
					} else {
						if (flipx) {
							Render8x8Tile_Mask_FlipX_Clip(pTransDraw, code & 0x3ff, sx, yy, color, 3, 0, 0x40, DrvGfxROM);
						} else {
							Render8x8Tile_Mask_Clip(pTransDraw, code & 0x3ff, sx, yy, color, 3, 0, 0x40, DrvGfxROM);
						}
					}
				}
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

	GenericTilemapSetScrollX(0, DrvScrRAM[2] ^ 0xff);
	GenericTilemapSetScrollY(0, DrvScrRAM[0]);

	GenericTilemapDraw(0, pTransDraw, 0);
	GenericTilemapDraw(1, pTransDraw, 0);

	draw_sprites();

	if (DrvScrRAM[0x101] == 0x0b) // hack
		GenericTilemapDraw(2, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		DrvInputs[0] = 0x08;
		DrvInputs[1] = 0x01;
		for (INT32 i = 0; i < 4; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
		}

		DrvInputs[1] |= (DrvJoy2[0] ? 0xf : 0);
	}

	ZetOpen(0);
	ZetRun(4000000 / 60);
	ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);

	ZetClose();

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		DrvDraw();
	}

	memcpy (DrvSprBuf, DrvScrRAM + 0x80, 0x80);

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
	}

	return 0;
}


// Super Speed Race Junior (Japan)

static struct BurnRomInfo ssrjRomDesc[] = {
	{ "a40-01.bin",		0x4000, 0x1ff7dbff, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "a40-02.bin",		0x4000, 0xbbb36f9f, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "a40-03.bin",		0x2000, 0x3753182a, 2 | BRF_GRA },           //  2 Graphics
	{ "a40-04.bin",		0x2000, 0x96471816, 2 | BRF_GRA },           //  3
	{ "a40-05.bin",		0x2000, 0xdce9169e, 2 | BRF_GRA },           //  4

	{ "proms",			0x0100, 0x00000000, 3 | BRF_NODUMP | BRF_GRA },           //  5 Color data (undumped)
};

STD_ROM_PICK(ssrj)
STD_ROM_FN(ssrj)

struct BurnDriver BurnDrvSsrj = {
	"ssrj", NULL, NULL, NULL, "1985",
	"Super Speed Race Junior (Japan)\0", "Weird colors", "Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_PREFIX_TAITO, GBF_RACING, 0,
	NULL, ssrjRomInfo, ssrjRomName, NULL, NULL, NULL, NULL, SsrjInputInfo, SsrjDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	240, 270, 3, 4
};
