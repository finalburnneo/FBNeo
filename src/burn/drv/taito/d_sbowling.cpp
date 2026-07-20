// FinalBurn Neo Strike Bowling driver module
// Based on MAME driver by Jarek Burczynski, Tomasz Slanina

// dink-notes: game writes past vram in the ball-test :)

#include "tiles_generic.h"
#include "z80_intf.h"
#include "watchdog.h"
#include "burn_gun.h"
#include "resnet.h"
#include "ay8910.h"
#include "samples.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvI8080ROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvMapROM;
static UINT8 *DrvColPROM;
static UINT8 *DrvI8080RAM;
static UINT8 *DrvVidRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 shift_value;
static UINT16 shift_data;
static INT32 flipscreen;
static INT32 port_select;
static UINT16 scrolly;
static UINT8 color_prom_address;

static UINT8 an_state;

static INT32 nCyclesExtra;

static INT16 Analog[4] = { 0, 0, 0, 0 };
static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

#define A(a, b, c, d) { a, b, (UINT8*)(c), d }
static struct BurnInputInfo SbowlingInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"		},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 start"		},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"		},
	A("P1 Trackball X",	BIT_ANALOG_REL,	&Analog[1],		"p1 x-axis"		),
	A("P1 Trackball Y",	BIT_ANALOG_REL,	&Analog[0],		"p1 y-axis"		),

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"		},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"		},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 1"		},
	A("P2 Trackball X",	BIT_ANALOG_REL,	&Analog[2],		"p2 x-axis"		),
	A("P2 Trackball Y",	BIT_ANALOG_REL,	&Analog[3],		"p2 y-axis"		),

	{"Service",			BIT_DIGITAL,	DrvJoy1 + 4,	"service"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 5,	"tilt"			},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"			},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"			},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"			},
};

STDINPUTINFO(Sbowling)
#undef A

static struct BurnDIPInfo SbowlingDIPList[]=
{
	DIP_OFFSET(0x0d)
	{0x00, 0xff, 0xff, 0x00, NULL					},
	{0x01, 0xff, 0xff, 0x03, NULL					},

	{0   , 0xfe, 0   ,   16, "Coin A"				},
	{0x00, 0x01, 0x0f, 0x0f, "9 Coins/1 Credit"		},
	{0x00, 0x01, 0x0f, 0x0e, "8 Coins/1 Credit"		},
	{0x00, 0x01, 0x0f, 0x0d, "7 Coins/1 Credit"		},
	{0x00, 0x01, 0x0f, 0x0c, "6 Coins/1 Credit"		},
	{0x00, 0x01, 0x0f, 0x0b, "5 Coins/1 Credit"		},
	{0x00, 0x01, 0x0f, 0x0a, "4 Coins/1 Credit"		},
	{0x00, 0x01, 0x0f, 0x09, "3 Coins/1 Credit"		},
	{0x00, 0x01, 0x0f, 0x08, "2 Coins/1 Credit"		},
	{0x00, 0x01, 0x0f, 0x00, "1 Coin/1 Credit"		},
	{0x00, 0x01, 0x0f, 0x01, "1 Coin/2 Credits"		},
	{0x00, 0x01, 0x0f, 0x02, "1 Coin/3 Credits"		},
	{0x00, 0x01, 0x0f, 0x03, "1 Coin/4 Credits"		},
	{0x00, 0x01, 0x0f, 0x04, "1 Coin/5 Credits"		},
	{0x00, 0x01, 0x0f, 0x05, "1 Coin/6 Credits"		},
	{0x00, 0x01, 0x0f, 0x06, "1 Coin/7 Credits"		},
	{0x00, 0x01, 0x0f, 0x07, "1 Coin/8 Credits"		},

	{0   , 0xfe, 0   ,   16, "Coin B"				},
	{0x00, 0x01, 0xf0, 0xf0, "9 Coins/1 Credit"		},
	{0x00, 0x01, 0xf0, 0xe0, "8 Coins/1 Credit"		},
	{0x00, 0x01, 0xf0, 0xd0, "7 Coins/1 Credit"		},
	{0x00, 0x01, 0xf0, 0xc0, "6 Coins/1 Credit"		},
	{0x00, 0x01, 0xf0, 0xb0, "5 Coins/1 Credit"		},
	{0x00, 0x01, 0xf0, 0xa0, "4 Coins/1 Credit"		},
	{0x00, 0x01, 0xf0, 0x90, "3 Coins/1 Credit"		},
	{0x00, 0x01, 0xf0, 0x80, "2 Coins/1 Credit"		},
	{0x00, 0x01, 0xf0, 0x00, "1 Coin/1 Credit"		},
	{0x00, 0x01, 0xf0, 0x10, "1 Coin/2 Credits"		},
	{0x00, 0x01, 0xf0, 0x20, "1 Coin/3 Credits"		},
	{0x00, 0x01, 0xf0, 0x30, "1 Coin/4 Credits"		},
	{0x00, 0x01, 0xf0, 0x40, "1 Coin/5 Credits"		},
	{0x00, 0x01, 0xf0, 0x50, "1 Coin/6 Credits"		},
	{0x00, 0x01, 0xf0, 0x60, "1 Coin/7 Credits"		},
	{0x00, 0x01, 0xf0, 0x70, "1 Coin/8 Credits"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x01, 0x01, 0x01, 0x01, "Upright"				},
	{0x01, 0x01, 0x01, 0x00, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Coin Slots"			},
	{0x01, 0x01, 0x02, 0x00, "1"					},
	{0x01, 0x01, 0x02, 0x02, "2"					},

	{0   , 0xfe, 0   ,    2, "Year Display"			},
	{0x01, 0x01, 0x04, 0x04, "Off"					},
	{0x01, 0x01, 0x04, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Ball Control Check"	},
	{0x01, 0x01, 0x40, 0x00, "Off"					},
	{0x01, 0x01, 0x40, 0x40, "On"					},

	{0   , 0xfe, 0   ,    2, "Video Test"			},
	{0x01, 0x01, 0x80, 0x00, "Off"					},
	{0x01, 0x01, 0x80, 0x80, "On"					},
};

STDDIPINFO(Sbowling)

static void anasound(UINT8 data)
{
	// sbowling dink-notes:
	// 0x01 ball (?)
	// 0x02 ball
	// 0x04 gutter
	// 0x08 return
	// 0x10 pin
	// 0x20 pin (?)
	// 0x80 !mute
	UINT8 high = (an_state ^ data) & data;
	UINT8 low = (an_state ^ data) & ~data;
	an_state = data;

//	if (low || high) bprintf(0, _T("ansound, low  %02x   high %02x      fr %d\n"), low, high, nCurrentFrame);

	if (high & 0x02) { splay(0, 0.25); }
	if (high & 0x04) { splay(1, 0.45); sstop(0); }
	if (high & 0x08) { splay(2, 0.25); sstop(0); }
	if ( low & 0x08) { sstop(2); }
	if (high & 0x30) { splay(3, 0.25); }
}

static void __fastcall sbowling_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xf800:
		case 0xf801:
			AY8910Write(0, address & 1, data);
		return;
	}
	bprintf(0, _T("w  %x  %x\n"), address, data);
}

static UINT8 __fastcall sbowling_read(UINT16 address)
{
	switch (address)
	{
		case 0xf801:
			return AY8910Read(0);
	}

	bprintf(0, _T("r  %x\n"), address);

	return 0;
}

static void __fastcall sbowling_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			BurnWatchdogWrite();
		return;

		case 0x01:
			shift_data = (shift_data >> 8) | (data << 8);
		return;

		case 0x02:
			shift_value = data & 7;
		return;

		case 0x03:
			anasound(data);
		return;

		case 0x04:
			port_select = (data & 0x02) >> 1;
			flipscreen = (data & 0x08) >> 3;
		return;

		case 0x05:
			color_prom_address = ((data & 0x07) << 7) | ((data & 0xc0) >> 3);
			scrolly = (~data & 0x30) << 4;
		return;
	}
}

static UINT8 __fastcall sbowling_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
			return DrvInputs[0];

		case 0x01:
			return BurnTrackballRead(flipscreen, port_select ? 1 : 0);

		case 0x02:
			return (shift_data << shift_value) >> 8;

		case 0x03:
			return DrvInputs[1];

		case 0x04:
			return DrvDips[0];

		case 0x05:
			return DrvDips[1];
	}

	bprintf(0, _T("rp  %x\n"), port);

	return 0;
}

static tilemap_callback( bg )
{
	TILE_SET_INFO(0, DrvMapROM[offs], 0, 0);
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	ZetOpen(0);
	ZetReset();
	ZetClose();

	AY8910Reset(0);
	BurnSampleReset();

	BurnWatchdogReset();

	shift_value = 0;
	shift_data = 0;
	flipscreen = 0;
	port_select = 0;
	scrolly = 0;
	color_prom_address = 0;

	an_state = 0;

	nCyclesExtra = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; 	Next = AllMem;

	DrvI8080ROM		= Next; Next += 0x003000;

	DrvGfxROM		= Next; Next += 0x004000;
	DrvMapROM		= Next; Next += 0x001000;

	DrvColPROM		= Next; Next += 0x000800;

	DrvPalette		= (UINT32*)Next; Next += 0x0400 * sizeof(UINT32);

	AllRam			= Next;

	DrvI8080RAM		= Next; Next += 0x000400;
	DrvVidRAM		= Next; Next += 0x004000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[3] = { 0x800*8*0, 0x800*8*1, 0x800*8*2 };
	INT32 XOffs[8] = { STEP8(7,-1) };
	INT32 YOffs[8] = { STEP8(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x1800);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM, 0x1800);

	GfxDecode(0x0100, 3, 8, 8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvI8080ROM + 0x0000, 0, 1)) return 1;
		if (BurnLoadRom(DrvI8080ROM + 0x1000, 1, 1)) return 1;
		if (BurnLoadRom(DrvI8080ROM + 0x2000, 2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM   + 0x0000, 3, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM   + 0x0800, 4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM   + 0x1000, 5, 1)) return 1;

		if (BurnLoadRom(DrvMapROM   + 0x0000, 6, 1)) return 1;

		if (BurnLoadRom(DrvColPROM  + 0x0000, 7, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x0400, 8, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvI8080ROM,		0x0000, 0x2fff, MAP_ROM);
	ZetMapMemory(DrvVidRAM,			0x8000, 0xbfff, MAP_RAM);
	ZetMapMemory(DrvI8080RAM,		0xfc00, 0xffff, MAP_RAM);
	ZetSetWriteHandler(sbowling_write);
	ZetSetReadHandler(sbowling_read);
	ZetSetOutHandler(sbowling_write_port);
	ZetSetInHandler(sbowling_read_port);
	ZetClose();

	BurnWatchdogInit(DrvDoReset, 180);

	AY8910Init(0, 1248000, 0);
	AY8910SetAllRoutes(0, 0.16, BURN_SND_ROUTE_BOTH);
	AY8910SetBuffered(ZetTotalCycles, 1996800);

	BurnSampleInit(1);
	BurnSampleSetBuffered(ZetTotalCycles, 1996800);
	BurnSampleSetAllRoutesAllSamples(1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 32, 256);
	GenericTilemapSetGfx(0, DrvGfxROM, 3, 8, 8, 0x4000, 0x18, 0);
	GenericTilemapSetOffsets(0, -8, -32);

	BurnTrackballInit(2);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	ZetExit();
	AY8910Exit(0);
	BurnSampleExit();
	BurnTrackballExit();

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteInit()
{
	const INT32 resistances_rg[3] = { 470, 270, 100 };
	const INT32 resistances_b[2]  = { 270, 100 };
	double rweights[3], gweights[3], bweights[2];

	compute_resistor_weights(0, 255, -1.0,
			3, &resistances_rg[0], rweights, 470, 0,
			3, &resistances_rg[0], gweights, 470, 0,
			2, &resistances_b[0],  bweights, 470, 0);

	for (INT32 i = 0; i < BurnDrvGetPaletteEntries(); i++)
	{
		INT32 bit0 = (DrvColPROM[i + 0x400] >>  1) & 1;
		INT32 bit1 = (DrvColPROM[i + 0x400] >>  2) & 1;
		INT32 bit2 = (DrvColPROM[i + 0x400] >>  3) & 1;
		INT32 r = combine_3_weights(rweights, bit0, bit1, bit2);

		bit0 = (DrvColPROM[i] >>  2) & 1;
		bit1 = (DrvColPROM[i] >>  3) & 1;
		bit2 = (DrvColPROM[i + 0x400] >>  0) & 1;
		INT32 g = combine_3_weights(rweights, bit0, bit1, bit2);

		bit0 = (DrvColPROM[i] >>  0) & 1;
		bit1 = (DrvColPROM[i] >>  1) & 1;
		INT32 b = combine_2_weights(bweights, bit0, bit1);

		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}
}

static void draw_bitmap()
{
	INT32 flipy = flipscreen ? 0xff : 0;
	INT32 flipx = flipscreen ? 0xf8 : 0;
	INT32 flipx2 = flipscreen ? 0x07 : 0;

	for (INT32 i = 0; i < 0x2000; i++)
	{
		INT32 sy = ((i >> 5) & 0xff) ^ flipy;
		INT32 sx = ((i & 0x1f) << 3) ^ flipx;

		sy -= (flipscreen) ? 0 : 32;
		sx -= 8;

		if (sy < 0 || sy >= nScreenHeight || sx < 0 || sx >= nScreenWidth) continue;

		UINT8 pdat0 = DrvVidRAM[i + 0x0000];
		UINT8 pdat1 = DrvVidRAM[i + 0x2000];

		for (INT32 x = 0; x < 8; x++, pdat0 >>= 1, pdat1 >>= 1)
		{
			INT32 pxl = ((pdat0 & 1) << 5) | ((pdat1 & 1) << 6);

			if (pxl)
			{
				pTransDraw[sy * nScreenWidth + sx + (x ^ flipx2)] = color_prom_address | pxl;
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

	if (~nBurnLayer & 1) BurnTransferClear(0x18);

	GenericTilemapSetFlip(0, flipscreen ? TMAP_FLIPXY : 0);
	GenericTilemapSetScrollY(0, scrolly);

	if ( nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);

	if ( nBurnLayer & 2) draw_bitmap();

	BurnTransferFlip(flipscreen, flipscreen);
	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	ZetNewFrame();

	{
		memset (DrvInputs, 0, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}

		BurnTrackballConfig(0, AXIS_REVERSED, AXIS_NORMAL);
		BurnTrackballFrame(0, Analog[1], Analog[0], 1, 0x3f);
		BurnTrackballUpdate(0);
		BurnTrackballConfig(1, AXIS_REVERSED, AXIS_NORMAL);
		BurnTrackballFrame(1, Analog[2], Analog[3], 1, 0x3f);
		BurnTrackballUpdate(1);
	}

	INT32 nInterleave = 262;
	INT32 nCyclesTotal[1] = { 1996800 / 60 };
	INT32 nCyclesDone[1] = { nCyclesExtra };

	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Zet);
		if (i == 128+6) {
			ZetSetVector(0xd7);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
		if (i == nInterleave-1) {
			ZetSetVector(0xcf);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
	}

	ZetClose();

	nCyclesExtra = nCyclesDone[0] - nCyclesTotal[0];

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
		BurnSampleRender(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	if (pnMin) *pnMin = 0x029521;

	if (nAction & ACB_VOLATILE) {
		ScanVar(AllRam, RamEnd - AllRam, "All Ram");

		ZetScan(nAction);
		BurnTrackballScan();
		BurnWatchdogScan(nAction);
		AY8910Scan(nAction, pnMin);
		BurnSampleScan(nAction, pnMin);

		SCAN_VAR(shift_value);
		SCAN_VAR(shift_data);
		SCAN_VAR(flipscreen);
		SCAN_VAR(port_select);
		SCAN_VAR(scrolly);
		SCAN_VAR(color_prom_address);
		SCAN_VAR(an_state);

		SCAN_VAR(nCyclesExtra);

	}

	return 0;
}


// Strike Bowling

static struct BurnSampleInfo sbowlingSampleDesc[] = {
	{ "ball", 				SAMPLE_NOLOOP },
	{ "gutter", 			SAMPLE_NOLOOP },
	{ "return", 			SAMPLE_NOLOOP },
	{ "pin", 				SAMPLE_NOLOOP },
	{ "", 0 }
};

STD_SAMPLE_PICK(sbowling)
STD_SAMPLE_FN(sbowling)

static struct BurnRomInfo sbowlingRomDesc[] = {
	{ "kb01.6h",		0x001000, 0xdd5d411a,  1 | BRF_PRG | BRF_ESS },	//  0 i8080 Code
	{ "kb02.5h",		0x001000, 0x75d3c45f,  1 | BRF_PRG | BRF_ESS },	//  1 
	{ "kb03.3h",		0x001000, 0x955fbfb8,  1 | BRF_PRG | BRF_ESS },	//  2 

	{ "kb05.9k",		0x000800, 0x4b4d9569,  2 | BRF_GRA },			//  3 Background Tiles
	{ "kb06.7k",		0x000800, 0xd89ba78b,  2 | BRF_GRA },			//  4 
	{ "kb07.6k",		0x000800, 0x9fb5db1a,  2 | BRF_GRA },			//  5 

	{ "kb04.10k",		0x001000, 0x1c27adc1,  3 | BRF_GRA },			//  6 Tilemap

	{ "kb08.7m",		0x000400, 0xe949e441,  4 | BRF_GRA },			//  7 Color PROM
	{ "kb09.6m",		0x000400, 0xe29191a6,  4 | BRF_GRA },			//  8 
};

STD_ROM_PICK(sbowling)
STD_ROM_FN(sbowling)

struct BurnDriver BurnDrvSbowling = {
	"sbowling", NULL, NULL, "sbowling", "1982",
	"Strike Bowling\0", NULL, "Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_SPORTSMISC, 0,
	NULL, sbowlingRomInfo, sbowlingRomName, NULL, NULL, sbowlingSampleInfo, sbowlingSampleName, SbowlingInputInfo, SbowlingDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 240, 3, 4
};
