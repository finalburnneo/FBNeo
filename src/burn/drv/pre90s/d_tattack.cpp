// FB Neo Shonan's Time Attacker driver module
// Based on MAME driver by Tomasz Slanina, Angelo Salese

#include "tiles_generic.h"
#include "z80_intf.h"
#include "samples.h"
#include "burn_pal.h"
#include "burn_gun.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM[1];
static UINT8 *DrvZ80RAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvColRAM;

static UINT8 bricks_color_bank;
static UINT8 flipscreen;
static UINT8 paddle_reg;
static UINT8 paddle_ysize;
static UINT8 bottom_edge_enable;
static UINT8 ball_regs[2];

static UINT8 DrvJoy1[8];
static UINT8 DrvDips[3];
static INT16 Analog[1];
static UINT8 DrvInputs[1];
static UINT8 DrvReset;

#define A(a, b, c, d) { a, b, (UINT8*)(c), d }

static struct BurnInputInfo TattackInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"		},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"		},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"		},
	A("P1 Paddle",		BIT_ANALOG_REL,	&Analog[0],		"p1 x-axis"		),

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 start"		},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"			},
	{"Dips A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"			},
	{"Dips B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"			},
	{"Dips C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"			},
};

STDINPUTINFO(Tattack)

#undef A

static struct BurnDIPInfo TattackDIPList[]=
{
	DIP_OFFSET(0x06)
	{0x00, 0xff, 0xff, 0x0e, NULL									},
	{0x01, 0xff, 0xff, 0xdf, NULL									},
	{0x02, 0xff, 0xff, 0xf5, NULL									},

	{0   , 0xfe, 0   ,    2, "1-01"									},
	{0x00, 0x01, 0x01, 0x01, "Off"									},
	{0x00, 0x01, 0x01, 0x00, "On"									},

	{0   , 0xfe, 0   ,    2, "1-02"									},
	{0x00, 0x01, 0x02, 0x02, "Off"									},
	{0x00, 0x01, 0x02, 0x00, "On"									},

	{0   , 0xfe, 0   ,    2, "1-03"									},
	{0x00, 0x01, 0x04, 0x04, "Off"									},
	{0x00, 0x01, 0x04, 0x00, "On"									},

	{0   , 0xfe, 0   ,    2, "1-04"									},
	{0x00, 0x01, 0x08, 0x08, "Off"									},
	{0x00, 0x01, 0x08, 0x00, "On"									},

	{0   , 0xfe, 0   ,   16, "Game Time"							},
	{0x01, 0x01, 0x0f, 0x00, "2:00"									},
	{0x01, 0x01, 0x0f, 0x01, "2:05"									},
	{0x01, 0x01, 0x0f, 0x02, "2:10"									},
	{0x01, 0x01, 0x0f, 0x03, "2:15"									},
	{0x01, 0x01, 0x0f, 0x04, "2:20"									},
	{0x01, 0x01, 0x0f, 0x05, "2:25"									},
	{0x01, 0x01, 0x0f, 0x06, "2:30"									},
	{0x01, 0x01, 0x0f, 0x07, "2:35"									},
	{0x01, 0x01, 0x0f, 0x08, "2:40"									},
	{0x01, 0x01, 0x0f, 0x09, "2:45"									},
	{0x01, 0x01, 0x0f, 0x0a, "2:50"									},
	{0x01, 0x01, 0x0f, 0x0b, "2:55"									},
	{0x01, 0x01, 0x0f, 0x0c, "3:00"									},
	{0x01, 0x01, 0x0f, 0x0d, "3:05"									},
	{0x01, 0x01, 0x0f, 0x0e, "3:10"									},
	{0x01, 0x01, 0x0f, 0x0f, "3:15"									},

	{0   , 0xfe, 0   ,    2, "Blinking Brick Awards 30 Seconds"		},
	{0x01, 0x01, 0x10, 0x00, "Once Only"							},
	{0x01, 0x01, 0x10, 0x10, "No Limit"								},

	{0   , 0xfe, 0   ,    2, "Cabinet"								},
	{0x01, 0x01, 0x20, 0x00, "Upright"								},
	{0x01, 0x01, 0x20, 0x20, "Cocktail"								},

	{0   , 0xfe, 0   ,    4, "Lives"								},
	{0x01, 0x01, 0xc0, 0x00, "3"									},
	{0x01, 0x01, 0xc0, 0x40, "5"									},
	{0x01, 0x01, 0xc0, 0x80, "7"									},
	{0x01, 0x01, 0xc0, 0xc0, "Infinite"								},

	{0   , 0xfe, 0   ,    2, "Oil Zones"							},
	{0x02, 0x01, 0x01, 0x00, "No"									},
	{0x02, 0x01, 0x01, 0x01, "Yes"									},

	{0   , 0xfe, 0   ,    2, "Coin A"								},
	{0x02, 0x01, 0x02, 0x00, "1 Coin/1 Credit"						},
	{0x02, 0x01, 0x02, 0x02, "1 Coin/2 Credits"						},

	{0   , 0xfe, 0   ,    2, "Game Mode"							},
	{0x02, 0x01, 0x04, 0x04, "Normal"								},
	{0x02, 0x01, 0x04, 0x00, "Hit 5 Bricks Then Game Over"			},

	{0   , 0xfe, 0   ,    4, "Enemies"								},
	{0x02, 0x01, 0x30, 0x00, "No"									},
	{0x02, 0x01, 0x30, 0x10, "Show When 40 Bricks Remaining"		},
	{0x02, 0x01, 0x30, 0x20, "Show When 20 Bricks Remaining"		},
	{0x02, 0x01, 0x30, 0x30, "Yes"									},

	{0   , 0xfe, 0   ,    2, "Enemy Delay"							},
	{0x02, 0x01, 0x40, 0x40, "Appear In Last 30 Seconds"			},
	{0x02, 0x01, 0x40, 0x00, "Disable"								},

	{0   , 0xfe, 0   ,    2, "Oil Zone Delay"						},
	{0x02, 0x01, 0x80, 0x80, "Appear In Last 30 Seconds"			},
	{0x02, 0x01, 0x80, 0x00, "Disable"								},
};

STDDIPINFO(Tattack)

static UINT8 __fastcall tattack_read(UINT16 address)
{
	switch (address)
	{
		case 0x4000:
			return BurnTrackballRead(0);

		case 0x6000:
			return DrvDips[1];

		case 0xa000:
			return DrvDips[2];

		case 0xc000:
			return DrvInputs[0];
	}

	return 0;
}

static void __fastcall tattack_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xc000:
			for (INT32 i = 0; i < 4; i++) {
				if ((data & (1 << i)) && BurnSampleGetStatus(i) == 0)
					BurnSampleChannelPlay(i, i);
			}
		return;

		case 0xc001:
			bricks_color_bank = (data & 0x40) >> 6;
			flipscreen = ~data & 0x20;
			paddle_ysize = (data & 0x10) ? 8 : 16;
			bottom_edge_enable = (~data & 0x08) >> 3;
		return;

		case 0xc002:
		return; // ?

		case 0xc005:
			paddle_reg = data;
		return;

		case 0xc006:
		case 0xc007:
			ball_regs[address & 1] = data;
		return;
	}
}

static tilemap_callback( bg )
{
	TILE_SET_INFO(0, DrvVidRAM[offs], DrvColRAM[offs] >> 1, 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	BurnSampleReset();

	bricks_color_bank = 0;
	flipscreen = 0;
	paddle_ysize = 0;
	paddle_reg = 0;
	bottom_edge_enable = 0;
	ball_regs[0] = ball_regs[1] = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x001000;

	DrvGfxROM[0]	= Next; Next += 0x008000;

	BurnPalette		= (UINT32*)Next; Next += 0x0010 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x000400;
	DrvVidRAM		= Next; Next += 0x000400;
	DrvColRAM		= Next; Next += 0x000400;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[1] = { 0 };
	INT32 XOffs[8] = { STEP8(0,1) };
	INT32 YOffs[8] = { STEP8(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x1000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM[0], 0x1000);

	GfxDecode(0x0200, 1, 8, 8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM[0]);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM, 0, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[0], 2, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x0fff, MAP_ROM);
	ZetMapMemory(DrvVidRAM,			0x5000, 0x53ff, MAP_RAM);
	ZetMapMemory(DrvColRAM,			0x7000, 0x73ff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM,			0xe000, 0xe3ff, MAP_RAM);
	ZetSetWriteHandler(tattack_write);
	ZetSetReadHandler(tattack_read);
	ZetClose();

	BurnSampleInit(0);
	BurnSampleSetAllRoutesAllSamples(0.80, BURN_SND_ROUTE_BOTH);

	BurnTrackballInit(1);
	BurnTrackballConfigStartStopPoints(0, 0x00, 0x0f, 0x00, 0x0f);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 1, 8, 8, 0x8000, 0, 7);
	GenericTilemapSetOffsets(0, -24, -13);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	BurnSampleExit();
	BurnTrackballExit();
	ZetExit();

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteInit()
{
	BurnPalette[0] = BurnHighCol(0, 0, 0xff, 0);
	BurnPalette[1] = BurnHighCol(0x80, 0x80, 0x80, 0);

	for (INT32 i = 1; i < 8; i++)
	{
		BurnPalette[i * 2 + 0] = BurnHighCol(0, 0, 0xff, 0);
		BurnPalette[i * 2 + 1] = BurnHighCol((i & 1) ? 0xff : 0, (i & 2) ? 0xff : 0, (i & 4) ? 0xff : 0, 0);
	}
}

static void draw_bricks()
{
	INT32 ram_base = 0x40 + (DrvZ80RAM[0x33] & 0x10);
	INT32 blink_row = DrvZ80RAM[0x2b];
	INT32 blink_col = DrvZ80RAM[0x2c];
	INT32 blink_enable = ((DrvZ80RAM[0x33] & 3) == 3) ? 1 : 0;

	for (INT32 ram_offs = ram_base; ram_offs < ram_base + 0xe; ram_offs++)
	{
		UINT8 cur_column = DrvZ80RAM[ram_offs];

		for (INT32 bit = 7; bit > -1; bit--)
		{
			INT32 draw_block = ((cur_column >> bit) & 1);

			if (blink_enable)
			{
				if (bit == blink_col && (ram_offs & 0xf) == blink_row)
					draw_block = 0;
			}

			if (draw_block)
			{
				for (INT32 sx = 0; sx < 3; sx++)
				{
					for (INT32 sy = 0; sy < 15; sy++)
					{
						INT32 x = bit * 4 + sx + 160 - 8;
						INT32 y = (ram_offs & 0xf) * 16 + sy + 16;

						draw_plot_pixel(0, x - 24, y - 13, bricks_color_bank ? 0x3 : (bit & 4) ? 0x7 : 0x5);
					}
				}
			}
		}
	}
}

static void draw_paddle()
{
	if (bottom_edge_enable == 0) return;

	for (INT32 sx = 0; sx < 4; sx++)
	{
		for (INT32 sy = 0; sy < paddle_ysize; sy++)
		{
			draw_plot_pixel(0, (38 + sx) - 24, (paddle_reg + sy) - 13, 0xf);
		}
	}
}

static void draw_ball()
{
	for (INT32 sx = 0; sx < 3; sx++)
	{
		for (INT32 sy = 0; sy < 3; sy++)
		{
			draw_plot_pixel(0, (ball_regs[0] + sx - 2 + -8) - 24, (ball_regs[1] + sy) - 13, 0xf);
		}
	}
}

static void draw_edges()
{
	draw_plot_box(0,   0 - 24,  16 - 13, 216,   4, 0xf);
	draw_plot_box(0, 216 - 24,  16 - 13,   6, 226, 0xf);
	draw_plot_box(0,   0 - 24, 238 - 13, 216,   4, 0xf);

	if (!bottom_edge_enable)
		draw_plot_box(0, 38 - 24, 16 - 13, 4, 226, 0xf);
}

static INT32 DrvDraw()
{
	if (BurnRecalc) {
		DrvPaletteInit();
		BurnRecalc = 0;
	}

	GenericTilemapDraw(0, pTransDraw, 0);

	draw_bricks();
	draw_ball();
	draw_edges();
	draw_paddle();

	BurnTransferCopy(BurnPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		DrvInputs[0] = DrvDips[0];

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
		}

		BurnTrackballConfig(0, AXIS_NORMAL, AXIS_NORMAL);
		BurnTrackballFrame(0, Analog[0] / 8, 0, 0x01, 0x1f);
		BurnTrackballUpdate(0);
	}

	INT32 nInterleave = 1;
	INT32 nCyclesTotal[1] = { 4000000 / 60 };
	INT32 nCyclesDone[1] = { 0 };

	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Zet);

		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
	}

	ZetClose();

	if (pBurnSoundOut) {
		BurnSampleRender(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	if (pnMin) {
		*pnMin = 0x029702;
	}

	if (nAction & ACB_VOLATILE) {
		ScanVar(AllRam, RamEnd - AllRam, "All Ram");

		ZetScan(nAction);
		BurnSampleScan(nAction, pnMin);
		BurnTrackballScan();

		SCAN_VAR(bricks_color_bank);
		SCAN_VAR(flipscreen);
		SCAN_VAR(paddle_ysize);
		SCAN_VAR(paddle_reg);
		SCAN_VAR(bottom_edge_enable);
		SCAN_VAR(ball_regs);
	}

	return 0;
}


// Time Attacker

static struct BurnRomInfo tattackRomDesc[] = {
	{ "rom.9a",		0x001000, 0x47120994,  1 | BRF_PRG | BRF_ESS },		//  0 Z80 Code

	{ "7910cq",		0x000800, 0x00000000,  2 | BRF_SND | BRF_NODUMP },	//  1 Melody data

	{ "rom.6c",		0x001000, 0x88ce45cf,  3 | BRF_GRA },				//  2 Layer Graphics
};

STD_ROM_PICK(tattack)
STD_ROM_FN(tattack)

static struct BurnSampleInfo TattackSampleDesc[] = {
	{ "paddle_hit", 	SAMPLE_NOLOOP },
	{ "wall_hit", 		SAMPLE_NOLOOP },
	{ "brick_destroy",	SAMPLE_NOLOOP },
	{ "win_bgm",		SAMPLE_NOLOOP },
	{ "", 0 }
};

STD_SAMPLE_PICK(Tattack)
STD_SAMPLE_FN(Tattack)

struct BurnDriver BurnDrvTattack = {
	"tattack", NULL, NULL, "tattack", "1983?",
	"Time Attacker\0", NULL, "Shonan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_BREAKOUT, 0,
	NULL, tattackRomInfo, tattackRomName, NULL, NULL, TattackSampleInfo, TattackSampleName, TattackInputInfo, TattackDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x10,
	232, 200, 3, 4
};

