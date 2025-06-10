// FB Alpha Fast Lane driver module
// Based on MAME driver by Manuel Abadia

#include "tiles_generic.h"
#include "hd6309_intf.h"
#include "konamiic.h" // K051733
#include "k007232.h"
#include "k007121.h"
#include "watchdog.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvHD6309ROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvColPROM;
static UINT8 *DrvSndROM0;
static UINT8 *DrvSndROM1;
static UINT8 *DrvK007121RAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvVidRAM0;
static UINT8 *DrvVidRAM1;
static UINT8 *DrvSprRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 *color_table;

static UINT8 main_bank;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo FastlaneInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Fastlane)

static struct BurnDIPInfo FastlaneDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL						},
	{0x13, 0xff, 0xff, 0x5a, NULL						},
	{0x14, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    16, "Coin A"					},
	{0x12, 0x01, 0x0f, 0x02, "4 Coins 1 Credits"		},
	{0x12, 0x01, 0x0f, 0x05, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"		},
	{0x12, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"		},
	{0x12, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"		},
	{0x12, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"		},
	{0x12, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"		},
	{0x12, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"		},
	{0x12, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"		},
	{0x12, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"		},
	{0x12, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"		},
	{0x12, 0x01, 0x0f, 0x00, "Free Play"				},

	{0   , 0xfe, 0   ,    16, "Coin B"					},
	{0x12, 0x01, 0xf0, 0x20, "4 Coins 1 Credits"		},
	{0x12, 0x01, 0xf0, 0x50, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"		},
	{0x12, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"		},
	{0x12, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"		},
	{0x12, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"		},
	{0x12, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"		},
	{0x12, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"		},
	{0x12, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"		},
	{0x12, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"		},
	{0x12, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"		},
	{0x12, 0x01, 0xf0, 0x00, "No Coin B"				},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x13, 0x01, 0x03, 0x03, "2"						},
	{0x13, 0x01, 0x03, 0x02, "3"						},
	{0x13, 0x01, 0x03, 0x01, "4"						},
	{0x13, 0x01, 0x03, 0x00, "7"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x13, 0x01, 0x04, 0x00, "Upright"					},
	{0x13, 0x01, 0x04, 0x04, "Cocktail"					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"				},
	{0x13, 0x01, 0x18, 0x18, "20k 100k 200k 400k 800k"	},
	{0x13, 0x01, 0x18, 0x10, "30k 150k 300k 600k"		},
	{0x13, 0x01, 0x18, 0x08, "20k only"					},
	{0x13, 0x01, 0x18, 0x00, "30k only"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x13, 0x01, 0x60, 0x60, "Easy"						},
	{0x13, 0x01, 0x60, 0x40, "Medium"					},
	{0x13, 0x01, 0x60, 0x20, "Hard"						},
	{0x13, 0x01, 0x60, 0x00, "Hardest"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x13, 0x01, 0x80, 0x80, "Off"						},
	{0x13, 0x01, 0x80, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Flip Screen"				},
	{0x14, 0x01, 0x01, 0x01, "Off"						},
	{0x14, 0x01, 0x01, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Upright Controls"			},
	{0x14, 0x01, 0x02, 0x02, "Single"					},
	{0x14, 0x01, 0x02, 0x00, "Dual"						},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x14, 0x01, 0x04, 0x04, "Off"						},
	{0x14, 0x01, 0x04, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Allow Continue"			},
	{0x14, 0x01, 0x08, 0x08, "3 Times"					},
	{0x14, 0x01, 0x08, 0x00, "Infinite"					},
};

STDDIPINFO(Fastlane)

static void bankswitch(INT32 data)
{
	// coin counters = data & 3

	main_bank = data;

	HD6309MapMemory(DrvHD6309ROM + 0x10000 + (((data >> 2) & 3) * 0x4000),	0x4000, 0x7fff, MAP_ROM);

	k007232_set_bank(1, (data >> 4) & 1, 2 + ((data >> 4) & 1));
}

static void fastlane_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfff8) == 0x0000) {
		k007121_ctrl_write(0, address & 7, data);
	}

	if (address < 0x60) {
		DrvK007121RAM[address & 0x7f] = data;
		return;
	}

	if ((address & 0xfff0) == 0x0d00) {
		K007232WriteReg(0, (address & 0xf) ^ 1, data);
		return;
	}

	if ((address & 0xfff0) == 0x0e00) {
		K007232WriteReg(1, (address & 0xf) ^ 1, data);
		return;
	}

	if ((address & 0xffe0) == 0x0f00) {
		K051733Write(address, data);
		return;
	}

	switch (address)
	{
		case 0x0b00:
			BurnWatchdogWrite();
		return;

		case 0x0c00:
			bankswitch(data);
		return;
	}
}

static UINT8 fastlane_read(UINT16 address)
{
	if ((address & 0xfff0) == 0x0d00) {
		return K007232ReadReg(0, (address & 0xf) ^ 1);
	}

	if ((address & 0xfff0) == 0x0e00) {
		return K007232ReadReg(1, (address & 0xf) ^ 1);
	}

	if ((address & 0xffe0) == 0x0f00) {
		return K051733Read(address);
	}

	switch (address)
	{
		case 0x0800:
			return DrvDips[2];

		case 0x0801:
			return DrvInputs[2];

		case 0x0802:
			return DrvInputs[1];

		case 0x0803:
			return DrvInputs[0];

		case 0x0900:
			return DrvDips[0];

		case 0x0901:
			return DrvDips[1];
	}

	return 0;
}

static tilemap_callback( bg )
{
	offs &= 0x7ff;
	INT32 attr = DrvVidRAM0[offs];
	UINT8 ctrl_3 = k007121_ctrl_read(0, 3);
	UINT8 ctrl_4 = k007121_ctrl_read(0, 4);
	UINT8 ctrl_5 = k007121_ctrl_read(0, 5);
	INT32 bit0 = (ctrl_5 >> 0) & 0x03;
	INT32 bit1 = (ctrl_5 >> 2) & 0x03;
	INT32 bit2 = (ctrl_5 >> 4) & 0x03;
	INT32 bit3 = (ctrl_5 >> 6) & 0x03;
	INT32 bank = ((attr & 0x80) >> 7) |
			((attr >> (bit0+2)) & 0x02) |
			((attr >> (bit1+1)) & 0x04) |
			((attr >> (bit2  )) & 0x08) |
			((attr >> (bit3-1)) & 0x10) |
			((ctrl_3 & 0x01) << 5);

	INT32 mask = (ctrl_4 & 0xf0) >> 4;

	bank = (bank & ~(mask << 1)) | ((ctrl_4 & mask) << 1);

	TILE_SET_INFO(0, DrvVidRAM0[offs + 0x400] + (bank * 256), 0x40 * (attr & 0xf) + 1, 0);
}

static tilemap_callback( fg )
{
	offs &= 0x7ff;
	INT32 attr = DrvVidRAM1[offs];
	UINT8 ctrl_3 = k007121_ctrl_read(0, 3);
	UINT8 ctrl_4 = k007121_ctrl_read(0, 4);
	UINT8 ctrl_5 = k007121_ctrl_read(0, 5);
	INT32 bit0 = (ctrl_5 >> 0) & 0x03;
	INT32 bit1 = (ctrl_5 >> 2) & 0x03;
	INT32 bit2 = (ctrl_5 >> 4) & 0x03;
	INT32 bit3 = (ctrl_5 >> 6) & 0x03;
	INT32 bank = ((attr & 0x80) >> 7) |
			((attr >> (bit0+2)) & 0x02) |
			((attr >> (bit1+1)) & 0x04) |
			((attr >> (bit2  )) & 0x08) |
			((attr >> (bit3-1)) & 0x10) |
			((ctrl_3 & 0x01) << 5);
	INT32 mask = (ctrl_4 & 0xf0) >> 4;

	bank = (bank & ~(mask << 1)) | ((ctrl_4 & mask) << 1);

	TILE_SET_INFO(0, DrvVidRAM1[offs + 0x400] + (bank * 256), 0x40 * (attr & 0xf) + 0, 0);
}

static void DrvK007232VolCallback0(INT32 v)
{
	K007232SetVolume(0, 0, (v >> 0x4) * 0x11, 0);
	K007232SetVolume(0, 1, 0, (v & 0x0f) * 0x11);
}

static void DrvK007232VolCallback1(INT32 v)
{
	K007232SetVolume(1, 0, (v >> 0x4) * 0x11, 0);
	K007232SetVolume(1, 1, 0, (v & 0x0f) * 0x11);
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	HD6309Open(0);
	bankswitch(0);
	HD6309Reset();
	HD6309Close();

	K007232Reset(0);
	K007232Reset(1);

	KonamiICReset();

	BurnWatchdogReset();

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvHD6309ROM	= Next; Next += 0x020000;

	DrvGfxROM		= Next; Next += 0x100000;

	DrvColPROM		= Next; Next += 0x000100;

	DrvSndROM0		= Next; Next += 0x020000;
	DrvSndROM1		= Next; Next += 0x080000;

	color_table		= Next; Next += 0x004000;

	DrvPalette		= (UINT32*)Next; Next += 0x4000 * sizeof(UINT32);

	AllRam			= Next;

	DrvK007121RAM	= Next; Next += 0x000100;
	DrvPalRAM		= Next; Next += 0x001000;
	DrvVidRAM0		= Next; Next += 0x000800;
	DrvVidRAM1		= Next; Next += 0x000800;
	DrvSprRAM		= Next; Next += 0x001000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void DrvColorTableInit()
{
	for (INT32 i = 0; i < 0x4000; i++)
	{
		color_table[i] = (i & 0x3f0) | DrvColPROM[((i >> 10) << 4) | (i & 0x0f)];
	}
}

static void DrvGfxExpand(UINT8 *src, INT32 len)
{
	for (INT32 i = (len - 1) * 2; i >= 0; i-=2) {
		src[i+0] = src[(i/2)^1] >> 4;
		src[i+1] = src[(i/2)^1] & 0xf;
	}
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvHD6309ROM + 0x08000,  0, 1)) return 1;
		if (BurnLoadRom(DrvHD6309ROM + 0x10000,  1, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM    + 0x00000,  2, 1)) return 1;

		if (BurnLoadRom(DrvColPROM   + 0x00000,  3, 1)) return 1;

		if (BurnLoadRom(DrvSndROM0   + 0x00000,  4, 1)) return 1;

		if (BurnLoadRom(DrvSndROM1   + 0x00000,  5, 1)) return 1;

		DrvGfxExpand(DrvGfxROM, 0x80000);
		DrvColorTableInit();
	}

	HD6309Init(0);
	HD6309Open(0);
	HD6309MapMemory(DrvK007121RAM,			0x0000, 0x00ff, MAP_ROM);
	HD6309MapMemory(DrvPalRAM,				0x1000, 0x1fff, MAP_RAM);
	HD6309MapMemory(DrvVidRAM0,				0x2000, 0x27ff, MAP_RAM);
	HD6309MapMemory(DrvVidRAM1,				0x2800, 0x2fff, MAP_RAM);
	HD6309MapMemory(DrvSprRAM,				0x3000, 0x3fff, MAP_RAM);
	HD6309MapMemory(DrvHD6309ROM + 0x8000,	0x8000, 0xffff, MAP_ROM);
	HD6309SetWriteHandler(fastlane_write);
	HD6309SetReadHandler(fastlane_read);
	HD6309Close();

	BurnWatchdogInit(DrvDoReset, 180);

	k007121_init(0, (0x100000 / (8 * 8)) - 1, DrvSprRAM);

	K007232Init(0, 3579545, DrvSndROM0, 0x20000);
	K007232SetPortWriteHandler(0, DrvK007232VolCallback0);
	K007232SetRoute(0, BURN_SND_K007232_ROUTE_1, 0.50, BURN_SND_ROUTE_BOTH);
	K007232SetRoute(0, BURN_SND_K007232_ROUTE_2, 0.50, BURN_SND_ROUTE_BOTH);

	K007232Init(1, 3579545, DrvSndROM1, 0x80000);
	K007232SetPortWriteHandler(1, DrvK007232VolCallback1);
	K007232SetRoute(1, BURN_SND_K007232_ROUTE_1, 0.50, BURN_SND_ROUTE_BOTH);
	K007232SetRoute(1, BURN_SND_K007232_ROUTE_2, 0.50, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, fg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM, 4, 8, 8, 0x100000, 0, 0x3ff);
	GenericTilemapSetOffsets(0, 40, -16);
	GenericTilemapSetOffsets(1, 0, -16);
	GenericTilemapSetScrollRows(0, 32);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	HD6309Exit();

	KonamiICExit();

	K007232Exit();

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteUpdate()
{
	UINT32 tmp[0x400];

	for (INT32 i = 0; i < 0x800; i+=2) {
		UINT16 d = DrvPalRAM[i + 1] | (DrvPalRAM[i] << 8);
		UINT8 r = (d >>  0) & 0x1f;
		UINT8 g = (d >>  5) & 0x1f;
		UINT8 b = (d >> 10) & 0x1f;

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);

		tmp[i/2] = BurnHighCol(r, g, b, 0);
	}

	for (INT32 i = 0; i < 0x4000; i++) {
		DrvPalette[i] = tmp[color_table[i]];
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	}

	if ((nBurnLayer & 1) == 0) BurnTransferClear();

	INT32 xoffs = k007121_ctrl_read(0, 0);

	for (INT32 i = 0; i < 32; i++) {
		GenericTilemapSetScrollRow(0, i, DrvK007121RAM[0x20 + i] + xoffs);
	}

	GenericTilemapSetScrollY(0, k007121_ctrl_read(0, 2));

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);

	if (nSpriteEnable & 1) k007121_draw(0, pTransDraw, DrvGfxROM, color_table, 0, 40, 16, 0, -1, 0x0000);

	GenericTilesSetClip(-1, 40, -1, -1);
	if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 0);
	GenericTilesClearClip();

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
		memset (DrvInputs, 0xff, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	HD6309NewFrame();

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[1] = { 12000000 / 60 };
	INT32 nCyclesDone[1] = { 0 };

	HD6309Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, HD6309);

		if (i == (nInterleave -1)) {
			if (k007121_ctrl_read(0, 7) & 0x02) HD6309SetIRQLine(0, CPU_IRQSTATUS_HOLD);
			k007121_buffer(0);
		}

		if ((i & 0x1f) == 0 && (k007121_ctrl_read(0, 7) & 0x01))
			HD6309SetIRQLine(0x20, CPU_IRQSTATUS_AUTO);
	}

	HD6309Close();

	if (pBurnSoundOut) {
		BurnSoundClear(); // k007232 by default "adds to stream"
		K007232Update(0, pBurnSoundOut, nBurnSoundLen);
		K007232Update(1, pBurnSoundOut, nBurnSoundLen);
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

		HD6309Scan(nAction);

		BurnWatchdogScan(nAction);
		k007121_scan(nAction);
		KonamiICScan(nAction);

		K007232Scan(nAction, pnMin);

		SCAN_VAR(main_bank);
	}

	if (nAction & ACB_WRITE) {
		HD6309Open(0);
		bankswitch(main_bank);
		HD6309Close();
	}

	return 0;
}


// Fast Lane

static struct BurnRomInfo fastlaneRomDesc[] = {
	{ "752_m02.9h",		0x08000, 0xe1004489, 1 | BRF_PRG | BRF_ESS }, //  0 HD6309 Code
	{ "752_e01.10h",	0x10000, 0xff4d6029, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "752e04.2i",		0x80000, 0xa126e82d, 2 | BRF_GRA },           //  2 Graphics

	{ "752e03.6h",		0x00100, 0x44300aeb, 3 | BRF_GRA },           //  3 Color LUT

	{ "752e06.4c",		0x20000, 0x85d691ed, 4 | BRF_GRA },           //  4 K007232 #0 Samples

	{ "752e05.12b",		0x80000, 0x119e9cbf, 5 | BRF_GRA },           //  5 K007232 #1 Samples (banked)
};

STD_ROM_PICK(fastlane)
STD_ROM_FN(fastlane)

struct BurnDriver BurnDrvFastlane = {
	"fastlane", NULL, NULL, NULL, "1987",
	"Fast Lane\0", NULL, "Konami", "GX752",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_RACING, 0,
	NULL, fastlaneRomInfo, fastlaneRomName, NULL, NULL, NULL, NULL, FastlaneInputInfo, FastlaneDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	224, 280, 3, 4
};
