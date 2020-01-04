// FB Alpha Flipper Jack driver module
// Based on MAME driver by Algelo Salese and Hap

#include "tiles_generic.h"
#include "z80_intf.h"
#include "8255ppi.h"
#include "ay8910.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM;
static UINT8 *DrvBlitROM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvColRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvFbRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 bankdata;
static UINT8 layer_reg;
static UINT8 previous_coin;
static UINT8 soundlatch;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[1];
static UINT8 DrvDips[1];
static UINT8 DrvInputs[4];
static UINT8 DrvReset;

static struct BurnInputInfo FlipjackInputList[] = {
	{"P1 Coin",				BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	{"P1 Left Flipper",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 1"	},
	{"P1 Right Flipper",	BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 2"	},
	{"P1 Shoot",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 3"	},
	{"P1 Tilt",				BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 4"	},

	{"P2 Start",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 start"	},
	{"P2 Left Flipper",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 1"	},
	{"P2 Right Flipper",	BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 2"	},
	{"P2 Shoot",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 3"	},
	{"P2 Tilt",				BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 4"	},

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",				BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Flipjack)

static struct BurnDIPInfo FlipjackDIPList[]=
{
	{0x0c, 0xff, 0xff, 0xf6, NULL					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x0c, 0x01, 0x01, 0x01, "Off"					},
	{0x0c, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Coinage"				},
	{0x0c, 0x01, 0x02, 0x02, "1 Coin  1 Credits"	},
	{0x0c, 0x01, 0x02, 0x00, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Drop Target"			},
	{0x0c, 0x01, 0x04, 0x00, "Off"					},
	{0x0c, 0x01, 0x04, 0x04, "On"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x0c, 0x01, 0x08, 0x00, "Upright"				},
	{0x0c, 0x01, 0x08, 0x08, "Cocktail"				},

	{0   , 0xfe, 0   ,    5, "Bonus Life"			},
	{0x0c, 0x01, 0x70, 0x70, "150K & Every 70K"		},
	{0x0c, 0x01, 0x70, 0x60, "150K & Every 100K"	},
	{0x0c, 0x01, 0x70, 0x50, "200K & Every 70K"		},
	{0x0c, 0x01, 0x70, 0x40, "200K & Every 100K"	},
	{0x0c, 0x01, 0x70, 0x00, "None"					},

	{0   , 0xfe, 0   ,    2, "Lives"				},
	{0x0c, 0x01, 0x80, 0x80, "3"					},
	{0x0c, 0x01, 0x80, 0x00, "5"					},
};

STDDIPINFO(Flipjack)

static void bankswitch(INT32 data)
{
	bankdata = data;

	ZetMapMemory(DrvZ80ROM0 + ((data & 4) ? 0x6000 : 0x4000), 0x2000, 0x3fff, MAP_ROM);
}

static void __fastcall flipjack_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x6800:
		case 0x6801:
		case 0x6802:
		case 0x6803:
			ppi8255_w(0, address & 3, data);
		return;

		case 0x7000:
			soundlatch = data;
			ZetSetIRQLine(1, 0, CPU_IRQSTATUS_ACK);
		return;

		case 0x7010:// hd6845 address
		case 0x7011:// hd6845 register
		return;

		case 0x7800:
			layer_reg = data;
		return;
	}
}

static UINT8 __fastcall flipjack_main_read(UINT16 address)
{
	switch (address)
	{
		case 0x6800:
		case 0x6801:
		case 0x6802:
		case 0x6803:
		    return ppi8255_r(0, address & 3);

		case 0x7020:
			return DrvDips[0];
	}

	return 0;
}

static void __fastcall flipjack_main_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0xff:
			bankswitch(data);
		return;
	}
}

static void __fastcall flipjack_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x4000:
			AY8910Write(1, 1, data);
		return;

		case 0x6000:
			AY8910Write(1, 0, data);
		return;

		case 0x8000:
			AY8910Write(0, 1, data);
		return;

		case 0xa000:
			AY8910Write(0, 0, data);
		return;
	}
}

static UINT8 __fastcall flipjack_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x4000:
			return AY8910Read(1);

		case 0x8000:
			return AY8910Read(0);
	}

	return 0;
}

static void __fastcall flipjack_sound_write_port(UINT16 port, UINT8 /*data*/)
{
	switch (port & 0xff)
	{
		case 0x00:
			ZetSetIRQLine(0x20, CPU_IRQSTATUS_NONE);
		return;
	}
}

static tilemap_callback( bg )
{
	TILE_SET_INFO(0, DrvVidRAM[offs] + (bankdata * 256), DrvColRAM[offs], 0);
}

static UINT8 ppiportAread()
{
	return DrvInputs[0];
}

static UINT8 ppiportBread()
{
	return DrvInputs[1];
}

static UINT8 ppiportCread()
{
	return DrvInputs[2];
}

static UINT8 ay8910_0_read_A(UINT32)
{
	ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
	return soundlatch;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	bankswitch(0);
	ZetReset();
	ZetClose();

	ZetReset(1);

	AY8910Reset(0);

	previous_coin = 0;
	soundlatch = 0;
	layer_reg = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x008000;
	DrvZ80ROM1		= Next; Next += 0x002000;

	DrvGfxROM		= Next; Next += 0x010000;
	DrvBlitROM		= Next; Next += 0x006000;

	DrvPalette		= (UINT32*)Next; Next += BurnDrvGetPaletteEntries() * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM0		= Next; Next += 0x002800;
	DrvZ80RAM1		= Next; Next += 0x000800;

	DrvColRAM		= Next; Next += 0x002000;
	DrvVidRAM		= Next; Next += 0x002000;
	DrvFbRAM		= Next; Next += 0x002000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[1] = { 0 };
	INT32 XOffs[8] = { STEP8(0,1) };
	INT32 YOffs[8] = { STEP8(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x2000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM, 0x2000);

	GfxDecode(0x0400, 1, 8, 8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM);

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
		if (BurnLoadRom(DrvZ80ROM0 + 0x2000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x4000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x6000,  3, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x0000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM  + 0x0000,  5, 1)) return 1;

		if (BurnLoadRom(DrvBlitROM + 0x0000,  6, 1)) return 1;
		if (BurnLoadRom(DrvBlitROM + 0x2000,  7, 1)) return 1;
		if (BurnLoadRom(DrvBlitROM + 0x4000,  8, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,			0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,			0x4000, 0x67ff, MAP_RAM);
	ZetMapMemory(DrvZ80ROM0 + 0x2000,	0x8000, 0x9fff, MAP_RAM);
	ZetMapMemory(DrvColRAM,				0xa000, 0xbfff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,				0xc000, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvFbRAM,				0xe000, 0xffff, MAP_RAM);
	ZetSetWriteHandler(flipjack_main_write);
	ZetSetReadHandler(flipjack_main_read);
	ZetSetOutHandler(flipjack_main_write_port);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,			0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,			0x2000, 0x27ff, MAP_RAM);
	ZetSetWriteHandler(flipjack_sound_write);
	ZetSetReadHandler(flipjack_sound_read);
	ZetSetOutHandler(flipjack_sound_write_port);
	ZetClose();

	ppi8255_init(1);
	ppi8255_set_read_ports(0, ppiportAread, ppiportBread, ppiportCread);

	AY8910Init(0, 2000000, 0);
	AY8910Init(1, 2000000, 1);
	AY8910SetPorts(0, &ay8910_0_read_A, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.14, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.14, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 256, 32);
	GenericTilemapSetGfx(0, DrvGfxROM, 1, 8, 8, 0x10000, 0, 7);
	GenericTilemapSetTransparent(0, 0);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	ppi8255_exit();

	AY8910Exit(0);
	AY8910Exit(1);

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 16; i++)
	{
		UINT8 r = (i & 4) ? 0xff : 0;
		UINT8 g = (i & 8) ? 0xff : 0;
		UINT8 b = (i & 2) ? 0xff : 0;

		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}
}

static void draw_playfield()
{
	for (INT32 offs = 0; offs < 0x1800; offs++)
	{
		INT32 sx = (offs & 0x1f) * 8;
		INT32 sy = (offs / 0x20);

		UINT8 r = DrvBlitROM[offs];
		UINT8 g = DrvBlitROM[offs + 0x2000];
		UINT8 b = DrvBlitROM[offs + 0x4000];

		UINT16 *dest = pTransDraw + (sy * nScreenWidth) + sx;

		for (INT32 x = 0; x < 8; x++)
		{
			dest[7 - x] = (((r >> x) & 1) << 1) | (((g >> x) & 1) << 2) | (((b >> x) & 1) << 3);
		}
	}
}

static void draw_framebuffer()
{
	for (INT32 offs = 0; offs < 0x1800; offs++)
	{
		INT32 sx = (offs & 0x1f) * 8;
		INT32 sy = (offs / 0x20);

		UINT8 data = DrvFbRAM[offs];

		UINT16 *dest = pTransDraw + (sy * nScreenWidth) + sx;

		for (INT32 x = 0; x < 8; x++)
		{
			if (data & (0x80 >> x))
			{
				dest[x] = 0x0e;
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
	
	if ((layer_reg & 0x02) && (nBurnLayer & 1))
	{
		draw_playfield();
	}
	else
	{
		BurnTransferClear();
	}

	if (nBurnLayer & 2) GenericTilemapDraw(0, pTransDraw, 0);

	if ((layer_reg & 0x04) && (nBurnLayer & 4))
	{
		draw_framebuffer();
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		if (previous_coin && (DrvJoy4[0] & 1) == 0) {
			ZetOpen(0);
			ZetNmi();
			ZetClose();
		}
		previous_coin = DrvJoy4[0] & 1;
	}

	INT32 nInterleave = 10;
	INT32 nCyclesTotal[2] = { 4000000 / 60, 4000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);
		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(1);
		nCyclesDone[1] += ZetRun(nCyclesTotal[1] / nInterleave);
		if (i == (nInterleave - 1)) ZetSetIRQLine(0x20, CPU_IRQSTATUS_ACK);
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
		*pnMin = 0x029708;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);
		AY8910Scan(nAction, pnMin);

		SCAN_VAR(bankdata);
		SCAN_VAR(layer_reg);
		SCAN_VAR(previous_coin);
		SCAN_VAR(soundlatch);
	}

	if (nAction & ACB_WRITE)
	{
		ZetOpen(0);
		bankswitch(bankdata);
		ZetClose();
	}

	return 0;
}


// Flipper Jack

static struct BurnRomInfo flipjackRomDesc[] = {
	{ "3.d5",			0x2000, 0x123bd992, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "4.f5",			0x2000, 0xd27e0184, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "1.l5",			0x2000, 0x4632263b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "2.m5",			0x2000, 0xe2bdce13, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "s.s5",			0x2000, 0x34515a7b, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code

	{ "cg.l6",			0x2000, 0x8d87f6b9, 3 | BRF_GRA },           //  5 Tiles

	{ "b.h6",			0x2000, 0xbbc8fdcc, 4 | BRF_GRA },           //  6 Blit data
	{ "r.f6",			0x2000, 0x8c02fe71, 4 | BRF_GRA },           //  7
	{ "g.d6",			0x2000, 0x8624d07f, 4 | BRF_GRA },           //  8

	{ "m3-7611-5.f8",	0x0100, 0xf0248102, 5 | BRF_GRA },           //  9 Color data
};

STD_ROM_PICK(flipjack)
STD_ROM_FN(flipjack)

struct BurnDriver BurnDrvFlipjack = {
	"flipjack", NULL, NULL, NULL, "1983",
	"Flipper Jack\0", NULL, "Jackson Co., Ltd.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PINBALL, 0,
	NULL, flipjackRomInfo, flipjackRomName, NULL, NULL, NULL, NULL, FlipjackInputInfo, FlipjackDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 16,
	192, 256, 3, 4
};
