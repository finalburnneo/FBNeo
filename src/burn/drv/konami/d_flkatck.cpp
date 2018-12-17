// FB Alpha Flack Attack driver module
// Based on MAME driver by Manuel Abadia

#include "tiles_generic.h"
#include "hd6309_intf.h"
#include "z80_intf.h"
#include "k007121.h"
#include "burn_ym2151.h"
#include "k007232.h"
#include "watchdog.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvHD6309ROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvSndROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvHD6309RAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvVidRAM0;
static UINT8 *DrvVidRAM1;
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprBUF;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 multiply_register[2];
static UINT8 main_bank;
static UINT8 soundlatch;
static UINT8 flipscreen;

static INT32 nExtraCycles;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo FlkatckInputList[] = {
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

STDINPUTINFO(Flkatck)

static struct BurnDIPInfo FlkatckDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL					},
	{0x13, 0xff, 0xff, 0x51, NULL					},
	{0x14, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,   16, "Coin A"				},
	{0x12, 0x01, 0x0f, 0x02, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0x0f, 0x05, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x12, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x12, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x12, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x12, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x12, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x12, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x12, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x12, 0x01, 0x0f, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,   16, "Coin B"				},
	{0x12, 0x01, 0xf0, 0x20, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0xf0, 0x50, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"	},
	{0x12, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"	},
	{0x12, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"	},
	{0x12, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x12, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x12, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x12, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x12, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x12, 0x01, 0xf0, 0x00, "Invalid"				},
	
	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x13, 0x01, 0x03, 0x03, "1"					},
	{0x13, 0x01, 0x03, 0x02, "2"					},
	{0x13, 0x01, 0x03, 0x01, "3"					},
	{0x13, 0x01, 0x03, 0x00, "5"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x13, 0x01, 0x04, 0x00, "Upright"				},
	{0x13, 0x01, 0x04, 0x04, "Cocktail"				},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x13, 0x01, 0x18, 0x18, "30K, Every 70K"		},
	{0x13, 0x01, 0x18, 0x10, "40K, Every 80K"		},
	{0x13, 0x01, 0x18, 0x08, "30K Only"				},
	{0x13, 0x01, 0x18, 0x00, "40K Only"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x13, 0x01, 0x60, 0x60, "Easy"					},
	{0x13, 0x01, 0x60, 0x40, "Normal"				},
	{0x13, 0x01, 0x60, 0x20, "Difficult"			},
	{0x13, 0x01, 0x60, 0x00, "Very Difficult"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x13, 0x01, 0x80, 0x80, "Off"					},
	{0x13, 0x01, 0x80, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x14, 0x01, 0x01, 0x01, "Off"					},
	{0x14, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Upright Controls"		},
	{0x14, 0x01, 0x02, 0x02, "Single"				},
	{0x14, 0x01, 0x02, 0x00, "Dual"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x14, 0x01, 0x04, 0x04, "Off"					},
	{0x14, 0x01, 0x04, 0x00, "On"					},
};

STDDIPINFO(Flkatck)



static void bankswitch(INT32 data)
{
	if ((data & 0x03) < 3) {
		main_bank = data & 3;

		HD6309MapMemory(DrvHD6309ROM + (main_bank * 0x2000), 0x4000, 0x5fff, MAP_ROM);
	}
}

static UINT8 flkatck_main_read(UINT16 address)
{
	switch (address)
	{
		case 0x0400:
			return DrvInputs[1];

		case 0x0401:
			return DrvInputs[2];

		case 0x0402:
			return DrvDips[2]; // dsw3

		case 0x0403:
			return DrvInputs[0];

		case 0x0406:
			return DrvDips[1];

		case 0x0407:
			return DrvDips[0];
	}

	return 0;
}

static void flkatck_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfff8) == 0x0000) {
		k007121_ctrl_write(0, address & 7, data);
	}

	if (address < 0x100) {
		DrvHD6309RAM[address] = data;
		return;
	}

	switch (address & ~3)
	{
		case 0x0410:
			bankswitch(data); // and coin counters - data & 0x18
		return;

		case 0x0414:
			soundlatch = data;
		return;

		case 0x0418:
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		return;

		case 0x041c:
			BurnWatchdogWrite();
		return;
	}
}

static void __fastcall flkatck_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x9000:
		case 0x9001:
			multiply_register[address & 1] = data;
		return;

		case 0x9006:
		return;

		case 0xb000:
		case 0xb001:
		case 0xb002:
		case 0xb003:
		case 0xb004:
		case 0xb005:
		case 0xb006:
		case 0xb007:
		case 0xb008:
		case 0xb009:
		case 0xb00a:
		case 0xb00b:
		case 0xb00c:
		case 0xb00d:
			K007232WriteReg(0, address & 0x0f, data);
		return;

		case 0xc000:
		case 0xc001:
			BurnYM2151Write(address & 1, data);
		return;
	}
}

static UINT8 __fastcall flkatck_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x9000:
			return (multiply_register[0] * multiply_register[1]) & 0xff;

		case 0x9001:
		case 0x9004:
			return 0;

		case 0xa000:
			return soundlatch;

		case 0xb000:
		case 0xb001:
		case 0xb002:
		case 0xb003:
		case 0xb004:
		case 0xb005:
		case 0xb006:
		case 0xb007:
		case 0xb008:
		case 0xb009:
		case 0xb00a:
		case 0xb00b:
		case 0xb00c:
		case 0xb00d:
			return K007232ReadReg(0, address & 0x0f);

		case 0xc000:
		case 0xc001:
			return BurnYM2151Read();
	}
	
	return 0;
}

static tilemap_callback( bg )
{
	UINT8 ctrl_0 = k007121_ctrl_read(0,0);
	UINT8 ctrl_2 = k007121_ctrl_read(0,2);
	UINT8 ctrl_3 = k007121_ctrl_read(0,3);
	UINT8 ctrl_4 = k007121_ctrl_read(0,4);
	UINT8 ctrl_5 = k007121_ctrl_read(0,5);
	UINT8 attr = DrvVidRAM0[offs];
	INT32 bit0 = (ctrl_5 >> 0) & 0x03;
	INT32 bit1 = (ctrl_5 >> 2) & 0x03;
	INT32 bit2 = (ctrl_5 >> 4) & 0x03;
	INT32 bit3 = (ctrl_5 >> 6) & 0x03;
	INT32 bank = ((attr & 0x80) >> 7) |
			((attr >> (bit0 + 2)) & 0x02) |
			((attr >> (bit1 + 1)) & 0x04) |
			((attr >> (bit2  )) & 0x08) |
			((attr >> (bit3 - 1)) & 0x10) |
			((ctrl_3 & 0x01) << 5);
	INT32 mask = (ctrl_4 & 0xf0) >> 4;

	bank = (bank & ~(mask << 1)) | ((ctrl_4 & mask) << 1);

	if ((attr == 0x0d) && (!ctrl_0) && (!ctrl_2))
		bank = 0;

	TILE_SET_INFO(0, DrvVidRAM0[offs + 0x400] + (bank * 256), (attr & 0x0f) + 16, (attr & 0x20) ? TILE_FLIPY : 0);
}

static tilemap_callback( fg )
{
	TILE_SET_INFO(0, DrvVidRAM1[offs + 0x400], DrvVidRAM1[offs], 0);
}

static void DrvK007232VolCallback(INT32 v)
{
	K007232SetVolume(0, 0, (v >> 4) * 0x11, 0);
	K007232SetVolume(0, 1, 0, (v & 0x0f) * 0x11);
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

	ZetOpen(0);
	ZetReset();
	ZetClose();

	BurnYM2151Reset();
	K007232Reset(0);
	k007232_set_bank(0, 0, 1);

	k007121_reset();

	BurnWatchdogReset();

	multiply_register[0] = 0;
	multiply_register[1] = 0;
	flipscreen = 0;
	soundlatch = 0;

	nExtraCycles = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvHD6309ROM	= Next; Next += 0x010000;
	DrvZ80ROM		= Next; Next += 0x008000;

	DrvGfxROM		= Next; Next += 0x100000;

	DrvSndROM		= Next; Next += 0x040000;

	DrvPalette		= (UINT32*)Next; Next += 0x200 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x000800;

	DrvHD6309RAM	= Next; Next += 0x004000;
	DrvPalRAM		= Next; Next += 0x000400;
	DrvVidRAM0		= Next; Next += 0x000800;
	DrvVidRAM1		= Next; Next += 0x000800;
	DrvSprRAM		= Next; Next += 0x000800;
	DrvSprBUF		= Next; Next += 0x000800;

	RamEnd			= Next;
	MemEnd			= Next;

	return 0;
}

static void graphics_expand()
{
	for (INT32 i = 0x80000-1; i >= 0; i--)
	{
		DrvGfxROM[i*2+1] = DrvGfxROM[i^1] & 0xf;
		DrvGfxROM[i*2+0] = DrvGfxROM[i^1] >> 4;
	}
}

static INT32 DrvInit(INT32 rom_layout)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvHD6309ROM + 0x00000,  0, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM    + 0x00000,  1, 1)) return 1;

		if (BurnLoadRom(DrvSndROM    + 0x00000,  2, 1)) return 1;

		if (rom_layout == 0)
		{
			if (BurnLoadRom(DrvGfxROM   + 0x00000,  3, 1)) return 1;
		}
		else
		{
			if (BurnLoadRom(DrvGfxROM   + 0x00001,  3, 2)) return 1;
			if (BurnLoadRom(DrvGfxROM   + 0x00000,  4, 2)) return 1;
			if (BurnLoadRom(DrvGfxROM   + 0x20001,  5, 2)) return 1;
			if (BurnLoadRom(DrvGfxROM   + 0x20000,  6, 2)) return 1;
			if (BurnLoadRom(DrvGfxROM   + 0x40001,  7, 2)) return 1;
			if (BurnLoadRom(DrvGfxROM   + 0x40000,  8, 2)) return 1;
			if (BurnLoadRom(DrvGfxROM   + 0x60001,  9, 2)) return 1;
			if (BurnLoadRom(DrvGfxROM   + 0x60000, 10, 2)) return 1;
		}

		graphics_expand();
	}

	HD6309Init(1);
	HD6309Open(0);
	HD6309MapMemory(DrvHD6309RAM,			0x0000, 0x00ff, MAP_ROM); // write through handler
	HD6309MapMemory(DrvHD6309RAM + 0x0100,	0x0100, 0x03ff, MAP_RAM);
	HD6309MapMemory(DrvPalRAM,				0x0800, 0x0bff, MAP_RAM);
	HD6309MapMemory(DrvHD6309RAM + 0x1000,	0x1000, 0x17ff, MAP_RAM);
	HD6309MapMemory(DrvSprBUF,				0x1800, 0x1fff, MAP_RAM);
	HD6309MapMemory(DrvVidRAM0,				0x2000, 0x27ff, MAP_RAM);
	HD6309MapMemory(DrvVidRAM1,				0x2800, 0x2fff, MAP_RAM);
	HD6309MapMemory(DrvHD6309RAM + 0x3000,	0x3000, 0x3fff, MAP_RAM);
	HD6309MapMemory(DrvHD6309ROM + 0x6000,	0x6000, 0xffff, MAP_ROM);
	HD6309SetWriteHandler(flkatck_main_write);
	HD6309SetReadHandler(flkatck_main_read);
	HD6309Close();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,					0x0000, 0x7fff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM,					0x8000, 0x87ff, MAP_RAM);
	ZetSetWriteHandler(flkatck_sound_write);
	ZetSetReadHandler(flkatck_sound_read);
	ZetClose();

	BurnWatchdogInit(DrvDoReset, 180);

	BurnYM2151Init(3579545);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);

	K007232Init(0, 3579545, DrvSndROM, 0x40000);
	K007232SetPortWriteHandler(0, DrvK007232VolCallback);
	K007232PCMSetAllRoutes(0, 0.35, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, fg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM, 4, 8, 8, 0x100000, 0x100, 0xf);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);

	k007121_init(0, (0x100000 / (8 * 8)) - 1);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	HD6309Exit();
	ZetExit();

	K007232Exit();
	BurnYM2151Exit();

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteUpdate()
{
	for (INT32 i = 0; i < 0x400; i+=2)
	{
		UINT16 p = (DrvPalRAM[i+1] * 256) + DrvPalRAM[i];

		UINT8 r = (p & 0x1f);
		UINT8 g = (p >> 5) & 0x1f;
		UINT8 b = (p >> 10) & 0x1f;

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);

		DrvPalette[i/2] = BurnHighCol(r,g,b,0);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	}

	GenericTilemapSetScrollX(0, k007121_ctrl_read(0, 0) - 40);
	GenericTilemapSetScrollY(0, k007121_ctrl_read(0, 2));

	BurnTransferClear();

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);

	if (nSpriteEnable & 1) k007121_draw(0, pTransDraw, DrvGfxROM, NULL, DrvSprRAM, 0, 40, 16, 0, -1, 0x0000);

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
		memset (DrvInputs, 0xff, 3 * sizeof(UINT8));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nSoundBufferPos = 0;
	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 3000000 / 60, 3579545 / 60 };
	INT32 nCyclesDone[2] = { nExtraCycles, 0 };

	HD6309Open(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone[0] += HD6309Run((nCyclesTotal[0] * (i + 1) / nInterleave) - nCyclesDone[0]);
		if (i == 240) {
			if (k007121_ctrl_read(0, 7) & 0x02)
				HD6309SetIRQLine(0, CPU_IRQSTATUS_HOLD);

			if (pBurnDraw) { // missing text in service mode if drawn after vbl
				DrvDraw();
			}

			memcpy(DrvSprRAM, DrvSprBUF, 0x800);
		}

		nCyclesDone[1] += ZetRun(nCyclesTotal[1] / nInterleave);

		if (pBurnSoundOut && i&1) {
			INT32 nSegmentLength = nBurnSoundLen / (nInterleave / 2);
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		if (nSegmentLength) {
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			BurnYM2151Render(pSoundBuf, nSegmentLength);
		}
		K007232Update(0, pBurnSoundOut, nBurnSoundLen); // only update K007232 once per frame
	}

	ZetClose();
	HD6309Close();

	nExtraCycles = nCyclesDone[0] - nCyclesTotal[0];

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
		ZetScan(nAction);
		BurnWatchdogScan(nAction);

		k007121_scan(nAction);

		BurnYM2151Scan(nAction, pnMin);
		K007232Scan(nAction, pnMin);

		SCAN_VAR(soundlatch);
		SCAN_VAR(flipscreen);
		SCAN_VAR(multiply_register);
		SCAN_VAR(main_bank);
		SCAN_VAR(nExtraCycles);
	}

	if (nAction & ACB_WRITE)
	{
		HD6309Open(0);
		bankswitch(main_bank);
		HD6309Close();
	}

	return 0;
}


// MX5000

static struct BurnRomInfo mx5000RomDesc[] = {
	{ "669_r01.16c",	0x10000, 0x79b226fc, 1 | BRF_PRG | BRF_ESS }, //  0 HD6309 Code

	{ "669_m02.16b",	0x08000, 0x7e11e6b9, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 Code

	{ "mask2m.11a",		0x40000, 0x6d1ea61c, 3 | BRF_SND },           //  2 k007232 Samples

	{ "mask4m.5e",		0x80000, 0xff1d718b, 4 | BRF_GRA },           //  3 Graphics
};

STD_ROM_PICK(mx5000)
STD_ROM_FN(mx5000)

static INT32 Mx5000Init()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvMx5000 = {
	"mx5000", NULL, NULL, NULL, "1987",
	"MX5000\0", NULL, "Konami", "GX669",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_PREFIX_KONAMI, GBF_VERSHOOT, 0,
	NULL, mx5000RomInfo, mx5000RomName, NULL, NULL, NULL, NULL, FlkatckInputInfo, FlkatckDIPInfo,
	Mx5000Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 280, 3, 4
};


// Flak Attack (Japan)

static struct BurnRomInfo flkatckRomDesc[] = {
	{ "669_p01.16c",	0x10000, 0xc5cd2807, 1 | BRF_PRG | BRF_ESS }, //  0 HD6309 Code

	{ "669_m02.16b",	0x08000, 0x7e11e6b9, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 Code

	{ "mask2m.11a",		0x40000, 0x6d1ea61c, 3 | BRF_SND },           //  2 k007232 Samples

	{ "mask4m.5e",		0x80000, 0xff1d718b, 4 | BRF_GRA },           //  3 Graphics
};

STD_ROM_PICK(flkatck)
STD_ROM_FN(flkatck)

struct BurnDriver BurnDrvFlkatck = {
	"flkatck", "mx5000", NULL, NULL, "1987",
	"Flak Attack (Japan)\0", NULL, "Konami", "GX669",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_PREFIX_KONAMI, GBF_VERSHOOT, 0,
	NULL, flkatckRomInfo, flkatckRomName, NULL, NULL, NULL, NULL, FlkatckInputInfo, FlkatckDIPInfo,
	Mx5000Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 280, 3, 4
};


// Flak Attack (Japan, PWB 450593 sub-board)

static struct BurnRomInfo flkatckaRomDesc[] = {
	{ "669_p01.16c",	0x10000, 0xc5cd2807, 1 | BRF_PRG | BRF_ESS }, //  0 HD6309 Code

	{ "669_m02.16b",	0x08000, 0x7e11e6b9, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 Code

	{ "mask2m.11a",		0x40000, 0x6d1ea61c, 3 | BRF_GRA },           //  2 k007232 Samples

	{ "669_f03a.4b",	0x10000, 0xf0ed4c1e, 4 | BRF_GRA },           //  3 Graphics
	{ "669_f03e.4d",	0x10000, 0x95a57a26, 4 | BRF_GRA },           //  4
	{ "669_f03b.5b",	0x10000, 0xe2593f3c, 4 | BRF_GRA },           //  5
	{ "669_f03f.5d",	0x10000, 0xc6c9903e, 4 | BRF_GRA },           //  6
	{ "669_f03c.6b",	0x10000, 0x47be92dd, 4 | BRF_GRA },           //  7
	{ "669_f03g.6d",	0x10000, 0x70d35fbd, 4 | BRF_GRA },           //  8
	{ "669_f03d.7b",	0x10000, 0x18d48f9e, 4 | BRF_GRA },           //  9
	{ "669_f03h.7d",	0x10000, 0xabfe76e7, 4 | BRF_GRA },           // 10
};

STD_ROM_PICK(flkatcka)
STD_ROM_FN(flkatcka)

static INT32 FlkatckaInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvFlkatcka = {
	"flkatcka", "mx5000", NULL, NULL, "1987",
	"Flak Attack (Japan, PWB 450593 sub-board)\0", NULL, "Konami", "GX669",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_PREFIX_KONAMI, GBF_VERSHOOT, 0,
	NULL, flkatckaRomInfo, flkatckaRomName, NULL, NULL, NULL, NULL, FlkatckInputInfo, FlkatckDIPInfo,
	FlkatckaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 280, 3, 4
};
