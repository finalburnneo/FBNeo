// FB Alpha Super Basketball driver module
// Based on MAME driver by Zsolt Vasvari

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "z80_intf.h"
#include "sn76496.h"
#include "dac.h"
#include "vlm5030.h"
#include "watchdog.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6809ROM;
static UINT8 *DrvM6809Dec;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvSndROM;
static UINT8 *DrvM6809RAM;
static UINT8 *DrvColRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvZ80RAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 palette_bank;
static UINT8 sprite_bank;
static UINT8 flipscreen;
static UINT8 soundlatch;
static UINT8 scroll;
static UINT8 sn76496_latch;
static UINT8 irq_mask;
static UINT16 previous_sound_address;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvInputs[3];
static UINT8 DrvDips[2];
static UINT8 DrvReset;

static struct BurnInputInfo SbasketbInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Sbasketb)

static struct BurnDIPInfo SbasketbDIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL					},
	{0x15, 0xff, 0xff, 0x68, NULL					},

	{0   , 0xfe, 0   ,    16, "Coin A"				},
	{0x14, 0x01, 0x0f, 0x02, "4 Coins 1 Credits"	},
	{0x14, 0x01, 0x0f, 0x05, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x14, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x14, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x14, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x14, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x14, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x14, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x14, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x14, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x14, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x14, 0x01, 0x0f, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    16, "Coin B"				},
	{0x14, 0x01, 0xf0, 0x20, "4 Coins 1 Credits"	},
	{0x14, 0x01, 0xf0, 0x50, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"	},
	{0x14, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"	},
	{0x14, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"	},
	{0x14, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"	},
	{0x14, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x14, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x14, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x14, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x14, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x14, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x14, 0x01, 0xf0, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    4, "Game Time"			},
	{0x15, 0x01, 0x03, 0x03, "30"					},
	{0x15, 0x01, 0x03, 0x01, "40"					},
	{0x15, 0x01, 0x03, 0x02, "50"					},
	{0x15, 0x01, 0x03, 0x00, "60"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x15, 0x01, 0x04, 0x00, "Upright"				},
	{0x15, 0x01, 0x04, 0x04, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Starting Score"		},
	{0x15, 0x01, 0x08, 0x08, "70-78"				},
	{0x15, 0x01, 0x08, 0x00, "100-115"				},

	{0   , 0xfe, 0   ,    2, "Ranking"				},
	{0x15, 0x01, 0x10, 0x00, "Data Remaining"		},
	{0x15, 0x01, 0x10, 0x10, "Data Initialized"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x15, 0x01, 0x60, 0x60, "Easy"					},
	{0x15, 0x01, 0x60, 0x40, "Medium"				},
	{0x15, 0x01, 0x60, 0x20, "Hard"					},
	{0x15, 0x01, 0x60, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x15, 0x01, 0x80, 0x80, "Off"					},
	{0x15, 0x01, 0x80, 0x00, "On"					},
};

STDDIPINFO(Sbasketb)

static void sbasketb_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x3c00:
			BurnWatchdogWrite();
		return;

		case 0x3c20:
			palette_bank = data & 0xf;
		return;

		case 0x3c80:
			flipscreen = data & 1;
		return;

		case 0x3c81:
			irq_mask = data & 1;
			if (irq_mask == 0)
				M6809SetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;

		case 0x3c82:
		return;		// nop?

		case 0x3c83:
		case 0x3c84:
			// coin counters
		return;

		case 0x3c85:
			sprite_bank = data & 1;
		return;

		case 0x3c86:
		case 0x3c87:
		return;		// nop

		case 0x3d00:
			soundlatch = data;
		return;

		case 0x3d80:
			ZetSetVector(0xff);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		return;

		case 0x3f80:
			scroll = data;
		return;
	}
}

static UINT8 sbasketb_main_read(UINT16 address)
{
	switch (address)
	{
		case 0x3e00:
			return DrvInputs[0];

		case 0x3e01:
			return DrvInputs[1];

		case 0x3e02:
			return DrvInputs[2];

		case 0x3e03:
			return 0; // nop

		case 0x3e80:
			return DrvDips[1];

		case 0x3f00:
			return DrvDips[0];
	}

	return 0;
}

static void __fastcall sbasketb_sound_write(UINT16 address, UINT8 data)
{
	if ((address & 0xe000) == 0xc000) {
		UINT16 addr = address ^ previous_sound_address;

		if (addr & 0x010) vlm5030_st(0, (address & 0x010)>>4);
		if (addr & 0x020) vlm5030_rst(0, (address & 0x020)>>5);

		previous_sound_address = address;
		return;
	}

	switch (address)
	{
		case 0xa000:
			vlm5030_data_write(0, data);
		return;

		case 0xe000:
			DACWrite(0, data);
		return;

		case 0xe001:
			sn76496_latch = data;
		return;

		case 0xe002:
			SN76496Write(0, sn76496_latch);
		return;
	}
}

static UINT8 __fastcall sbasketb_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x6000:
			return soundlatch;

		case 0x8000:
			return ((ZetTotalCycles() / 1024) & 3) | (vlm5030_bsy(0) ? 4 : 0);
	}

	return 0;
}

static tilemap_callback( bg )
{
	UINT8 attr = DrvColRAM[offs];

	TILE_SET_INFO(0, DrvVidRAM[offs] + ((attr & 0x20) << 3), attr, TILE_FLIPYX(attr >> 6));
}

static UINT32 vlm_sync(INT32 samples_rate)
{
	return (samples_rate * ZetTotalCycles()) / 59659;
}

static INT32 DrvSyncDAC()
{
	return (INT32)(float)(nBurnSoundLen * (ZetTotalCycles() / (3579545.0000 / (nBurnFPS / 100.0000))));
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);

		palette_bank = 0;
		sprite_bank = 0;
		flipscreen = 0;
		soundlatch = 0;
		scroll = 0;
		sn76496_latch = 0;
		irq_mask = 0;
		previous_sound_address = 0;
	}

	M6809Open(0);
	M6809Reset();
	M6809Close();

	ZetOpen(0);
	ZetReset();
	DACReset();
	vlm5030Reset(0);
	ZetClose();

	BurnWatchdogReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6809ROM		= Next; Next += 0x010000;
	DrvM6809Dec		= Next; Next += 0x010000;

	DrvZ80ROM		= Next; Next += 0x002000;

	DrvGfxROM0		= Next; Next += 0x008000;
	DrvGfxROM1		= Next; Next += 0x020000;

	DrvColPROM		= Next; Next += 0x000500;

	DrvSndROM		= Next; Next += 0x002000;

	DrvPalette		= (UINT32*)Next; Next += 0x1100 * sizeof(UINT32);

	AllRam			= Next;

	DrvM6809RAM		= Next; Next += 0x003000;
	DrvColRAM		= Next; Next += 0x000400;
	DrvVidRAM		= Next; Next += 0x000400;
	DrvSprRAM		= Next; Next += 0x000400;
	DrvZ80RAM		= Next; Next += 0x000400;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void Konami1Decode()
{
	for (INT32 i = 0; i < 0x10000; i++) {
		DrvM6809Dec[i] ^= (((i & 2) ? 0x80 : 0x20) | ((i & 8) ? 0x08 : 0x02));
	}
}

static void DrvGfxExpand(UINT8 *src, INT32 len)
{
	for (INT32 i = len-1; i >= 0; i--) {
		src[i*2+0] = src[i] >> 4;
		src[i*2+1] = src[i] & 0xf;
	}
}

static INT32 DrvInit(INT32 rom_load, INT32 encrypted)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (rom_load == 1)
		{
			if (BurnLoadRom(DrvM6809ROM + 0x6000,  0, 1)) return 1;
			if (BurnLoadRom(DrvM6809ROM + 0x8000,  1, 1)) return 1;
			if (BurnLoadRom(DrvM6809ROM + 0xa000,  2, 1)) return 1;
			if (BurnLoadRom(DrvM6809ROM + 0xc000,  3, 1)) return 1;
			if (BurnLoadRom(DrvM6809ROM + 0xe000,  4, 1)) return 1;

			if (BurnLoadRom(DrvZ80ROM   + 0x0000,  5, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM0  + 0x0000,  6, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM1  + 0x0000,  7, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1  + 0x2000,  8, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1  + 0x4000,  9, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1  + 0x6000, 10, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1  + 0x8000, 11, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1  + 0xa000, 12, 1)) return 1;

			if (BurnLoadRom(DrvColPROM  + 0x0000, 13, 1)) return 1;
			if (BurnLoadRom(DrvColPROM  + 0x0100, 14, 1)) return 1;
			if (BurnLoadRom(DrvColPROM  + 0x0200, 15, 1)) return 1;
			if (BurnLoadRom(DrvColPROM  + 0x0300, 16, 1)) return 1;
			if (BurnLoadRom(DrvColPROM  + 0x0400, 17, 1)) return 1;

			if (BurnLoadRom(DrvSndROM   + 0x0000, 18, 1)) return 1;
		} else {
			if (BurnLoadRom(DrvM6809ROM + 0x6000,  0, 1)) return 1;
			if (BurnLoadRom(DrvM6809ROM + 0x8000,  1, 1)) return 1;
			if (BurnLoadRom(DrvM6809ROM + 0xc000,  2, 1)) return 1;

			if (BurnLoadRom(DrvZ80ROM   + 0x0000,  3, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM0  + 0x0000,  4, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM1  + 0x0000,  5, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1  + 0x4000,  6, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1  + 0x8000,  7, 1)) return 1;

			if (BurnLoadRom(DrvColPROM  + 0x0000,  8, 1)) return 1;
			if (BurnLoadRom(DrvColPROM  + 0x0100,  9, 1)) return 1;
			if (BurnLoadRom(DrvColPROM  + 0x0200, 10, 1)) return 1;
			if (BurnLoadRom(DrvColPROM  + 0x0300, 11, 1)) return 1;
			if (BurnLoadRom(DrvColPROM  + 0x0400, 12, 1)) return 1;

			if (BurnLoadRom(DrvSndROM   + 0x0000, 13, 1)) return 1;
		}

		memcpy (DrvM6809Dec, DrvM6809ROM, 0x10000);
		if (encrypted) Konami1Decode();
		DrvGfxExpand(DrvGfxROM0, 0x4000);
		DrvGfxExpand(DrvGfxROM1, 0xc000);
	}

	M6809Init(1);
	M6809Open(0);
	M6809MapMemory(DrvM6809RAM,				0x0000, 0x2fff, MAP_RAM);
	M6809MapMemory(DrvColRAM,				0x3000, 0x33ff, MAP_RAM);
	M6809MapMemory(DrvVidRAM,				0x3400, 0x37ff, MAP_RAM);
	M6809MapMemory(DrvSprRAM,				0x3800, 0x3bff, MAP_RAM); // 800-9ff
	M6809MapMemory(DrvM6809ROM + 0x6000,	0x6000, 0xffff, MAP_ROM);
	M6809MapMemory(DrvM6809Dec + 0x6000,	0x6000, 0xffff, MAP_FETCH);
	M6809SetWriteHandler(sbasketb_main_write);
	M6809SetReadHandler(sbasketb_main_read);
	M6809Close();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,					0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,					0x4000, 0x43ff, MAP_RAM);
	ZetSetWriteHandler(sbasketb_sound_write);
	ZetSetReadHandler(sbasketb_sound_read);
	ZetClose();

	BurnWatchdogInit(DrvDoReset, 180);

	SN76489AInit(0, 1789772, 0);
	SN76496SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	DACInit(0, 0, 1, DrvSyncDAC);
	DACSetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	vlm5030Init(0, 3579545, vlm_sync, DrvSndROM, 0x2000, 1);
	vlm5030SetAllRoutes(0, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 4, 8, 8, 0x08000, 0, 0xf);
	GenericTilemapSetScrollCols(0, 32);
	GenericTilemapSetOffsets(0, 0, -16);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6809Exit();
	ZetExit();

	SN76496Exit();
	DACExit();
	vlm5030Exit();

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	UINT32 pal[0x100];

	for (INT32 i = 0; i < 0x100; i++)
	{
		INT32 bit0, bit1, bit2, bit3;
		INT32 r, g, b;

		bit0 = (DrvColPROM[i + 0x000] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + 0x000] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + 0x000] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + 0x000] >> 3) & 0x01;
		r = ((bit3 * 13821) + (bit2 * 6911) + (bit1 * 3248) + (bit0 * 1520)) / 100;

		bit0 = (DrvColPROM[i + 0x100] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + 0x100] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + 0x100] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + 0x100] >> 3) & 0x01;
		g = ((bit3 * 13821) + (bit2 * 6911) + (bit1 * 3248) + (bit0 * 1520)) / 100;

		bit0 = (DrvColPROM[i + 0x200] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + 0x200] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + 0x200] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + 0x200] >> 3) & 0x01;
		b = ((bit3 * 13821) + (bit2 * 6911) + (bit1 * 3248) + (bit0 * 1520)) / 100;

		pal[i] = BurnHighCol(r,g,b,0);
	}

	for (INT32 i = 0; i < 0x100; i++)
	{
		DrvPalette[i] = pal[(DrvColPROM[0x300 + i] & 0xf) + 0xf0];

		for (INT32 j = 0; j < 0x10; j++)
		{
			DrvPalette[0x100 + (j * 256) + i] = pal[(DrvColPROM[0x400 + i] & 0xf) | (j * 16)];
		}
	}
}

static void draw_sprites()
{
	UINT8 *ram = DrvSprRAM + (sprite_bank ? 0x100 : 0);

	for (INT32 i = 0, offs = 0; i < 64; i++, offs += 4)
	{
		INT32 sx = ram[offs + 2];
		INT32 sy = ram[offs + 3] - 16;

		if (sx || sy)
		{
			INT32 attr   = ram[offs + 1];
			INT32 code  =  ram[offs + 0] | ((attr & 0x20) << 3);
			INT32 color = (attr & 0x0f) + (16 * palette_bank);
			INT32 flipx =  attr & 0x40;
			INT32 flipy =  attr & 0x80;

			if (flipscreen)
			{
				sx = 240 - sx;
				sy = 240 - sy;
				flipx = !flipx;
				flipy = !flipy;
			}

			if (flipy) {
				if (flipx) {
					Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0x100, DrvGfxROM1);
				} else {
					Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0x100, DrvGfxROM1);
				}
			} else {
				if (flipx) {
					Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0x100, DrvGfxROM1);
				} else {
					Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0x100, DrvGfxROM1);
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

	BurnTransferClear();

	for (INT32 i = 6; i < 32; i++) {
		GenericTilemapSetScrollCol(0, i, scroll);
	}

	GenericTilemapDraw(0, pTransDraw, 0);

	draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	M6809NewFrame();
	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
 			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nSoundBufferPos = 0;
	INT32 nCyclesTotal[2] = { 1400000 / 60, 3579545 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	M6809Open(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone[0] += M6809Run(nCyclesTotal[0] / nInterleave);

		if (i == 240 && irq_mask) M6809SetIRQLine(0, CPU_IRQSTATUS_HOLD);

		nCyclesDone[1] += ZetRun(nCyclesTotal[1] / nInterleave);

		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			SN76496Update(0, pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		SN76496Update(0, pSoundBuf, nSegmentLength);

		DACUpdate(pBurnSoundOut, nBurnSoundLen);
		// vlm5030 won't interlace, so just run it at the end of the frame..
		vlm5030Update(0, pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	M6809Close();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029704;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		M6809Scan(nAction);
		ZetScan(nAction);

		DACScan(nAction,pnMin);
		vlm5030Scan(nAction, pnMin);
		SN76496Scan(nAction,pnMin);

		BurnWatchdogScan(nAction);

		SCAN_VAR(palette_bank);
		SCAN_VAR(sprite_bank);
		SCAN_VAR(flipscreen);
		SCAN_VAR(soundlatch);
		SCAN_VAR(scroll);
		SCAN_VAR(sn76496_latch);
		SCAN_VAR(irq_mask);
		SCAN_VAR(previous_sound_address);
	}

	return 0;
}


// Super Basketball (version I, encrypted)

static struct BurnRomInfo sbasketbRomDesc[] = {
	{ "405g05.14j",	0x2000, 0x336dc0ab, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "405i03.11j",	0x4000, 0xd33b82dd, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "405i01.9j",	0x4000, 0x1c09cc3f, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "405e13.7a",	0x2000, 0x1ec7458b, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 Code

	{ "405e12.22f",	0x4000, 0xe02c54da, 3 | BRF_GRA },           //  4 Background Tiles

	{ "405h06.14g",	0x4000, 0xcfbbff07, 4 | BRF_GRA },           //  5 Sprites
	{ "405h08.17g",	0x4000, 0xc75901b6, 4 | BRF_GRA },           //  6
	{ "405h10.20g",	0x4000, 0x95bc5942, 4 | BRF_GRA },           //  7

	{ "405e17.5a",	0x0100, 0xb4c36d57, 5 | BRF_GRA },           //  8 Color data
	{ "405e16.4a",	0x0100, 0x0b7b03b8, 5 | BRF_GRA },           //  9
	{ "405e18.6a",	0x0100, 0x9e533bad, 5 | BRF_GRA },           // 10
	{ "405e20.19d",	0x0100, 0x8ca6de2f, 5 | BRF_GRA },           // 11
	{ "405e19.16d",	0x0100, 0xe0bc782f, 5 | BRF_GRA },           // 12

	{ "405e15.11f",	0x2000, 0x01bb5ce9, 6 | BRF_SND },           // 13 VLM data
};

STD_ROM_PICK(sbasketb)
STD_ROM_FN(sbasketb)

static INT32 SbasketbInit()
{
	return DrvInit(0, 1);
}

struct BurnDriver BurnDrvSbasketb = {
	"sbasketb", NULL, NULL, NULL, "1984",
	"Super Basketball (version I, encrypted)\0", NULL, "Konami", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_PREFIX_KONAMI, GBF_SPORTSMISC, 0,
	NULL, sbasketbRomInfo, sbasketbRomName, NULL, NULL, NULL, NULL, SbasketbInputInfo, SbasketbDIPInfo,
	SbasketbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1100,
	224, 256, 3, 4
};


// Super Basketball (version H, unprotected)

static struct BurnRomInfo sbaskethRomDesc[] = {
	{ "405h05.14j",	0x2000, 0x263ec36b, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "405h03.11j",	0x4000, 0x0a4d7a82, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "405h01.9j",	0x4000, 0x4f9dd9a0, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "405e13.7a",	0x2000, 0x1ec7458b, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 Code

	{ "405e12.22f",	0x4000, 0xe02c54da, 3 | BRF_GRA },           //  4 Background Tiles

	{ "405h06.14g",	0x4000, 0xcfbbff07, 4 | BRF_GRA },           //  5 Sprites
	{ "405h08.17g",	0x4000, 0xc75901b6, 4 | BRF_GRA },           //  6
	{ "405h10.20g",	0x4000, 0x95bc5942, 4 | BRF_GRA },           //  7

	{ "405e17.5a",	0x0100, 0xb4c36d57, 5 | BRF_GRA },           //  8 Color data
	{ "405e16.4a",	0x0100, 0x0b7b03b8, 5 | BRF_GRA },           //  9
	{ "405e18.6a",	0x0100, 0x9e533bad, 5 | BRF_GRA },           // 10
	{ "405e20.19d",	0x0100, 0x8ca6de2f, 5 | BRF_GRA },           // 11
	{ "405e19.16d",	0x0100, 0xe0bc782f, 5 | BRF_GRA },           // 12

	{ "405e15.11f",	0x2000, 0x01bb5ce9, 6 | BRF_SND },           // 13 VLM data
};

STD_ROM_PICK(sbasketh)
STD_ROM_FN(sbasketh)

static INT32 SbaskethInit()
{
	return DrvInit(0, 0);
}

struct BurnDriver BurnDrvSbasketh = {
	"sbasketh", "sbasketb", NULL, NULL, "1984",
	"Super Basketball (version H, unprotected)\0", NULL, "Konami", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_PREFIX_KONAMI, GBF_SPORTSMISC, 0,
	NULL, sbaskethRomInfo, sbaskethRomName, NULL, NULL, NULL, NULL, SbasketbInputInfo, SbasketbDIPInfo,
	SbaskethInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1100,
	224, 256, 3, 4
};


// Super Basketball (version G, encrypted)

static struct BurnRomInfo sbasketgRomDesc[] = {
	{ "405g05.14j",	0x2000, 0x336dc0ab, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "405g04.13j",	0x2000, 0xf064a9bc, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "405g03.11j",	0x2000, 0xb9de7d53, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "405g02.10j",	0x2000, 0xe98470a0, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "405g01.9j",	0x2000, 0x1bd0cd2e, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "405e13.7a",	0x2000, 0x1ec7458b, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 Code

	{ "405e12.22f",	0x4000, 0xe02c54da, 3 | BRF_GRA },           //  6 Background Tiles

	{ "405e06.14g",	0x2000, 0x7e2f5bb2, 4 | BRF_GRA },           //  7 Sprites
	{ "405e07.16g",	0x2000, 0x963a44f9, 4 | BRF_GRA },           //  8
	{ "405e08.17g",	0x2000, 0x63901deb, 4 | BRF_GRA },           //  9
	{ "405e09.19g",	0x2000, 0xe1873677, 4 | BRF_GRA },           // 10
	{ "405e10.20g",	0x2000, 0x824815e8, 4 | BRF_GRA },           // 11
	{ "405e11.22g",	0x2000, 0xdca9b447, 4 | BRF_GRA },           // 12

	{ "405e17.5a",	0x0100, 0xb4c36d57, 5 | BRF_GRA },           // 13 Color data
	{ "405e16.4a",	0x0100, 0x0b7b03b8, 5 | BRF_GRA },           // 14
	{ "405e18.6a",	0x0100, 0x9e533bad, 5 | BRF_GRA },           // 15
	{ "405e20.19d",	0x0100, 0x8ca6de2f, 5 | BRF_GRA },           // 16
	{ "405e19.16d",	0x0100, 0xe0bc782f, 5 | BRF_GRA },           // 17

	{ "405e15.11f",	0x2000, 0x01bb5ce9, 6 | BRF_SND },           // 18 VLM data
};

STD_ROM_PICK(sbasketg)
STD_ROM_FN(sbasketg)

static INT32 SbasketgInit()
{
	return DrvInit(1, 1);
}

struct BurnDriver BurnDrvSbasketg = {
	"sbasketg", "sbasketb", NULL, NULL, "1984",
	"Super Basketball (version G, encrypted)\0", NULL, "Konami", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_PREFIX_KONAMI, GBF_SPORTSMISC, 0,
	NULL, sbasketgRomInfo, sbasketgRomName, NULL, NULL, NULL, NULL, SbasketbInputInfo, SbasketbDIPInfo,
	SbasketgInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1100,
	224, 256, 3, 4
};


// Super Basketball (version E, encrypted)

static struct BurnRomInfo sbasketeRomDesc[] = {
	{ "405e05.14j",	0x2000, 0x32ea5b71, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "405e04.13j",	0x2000, 0x7abf3087, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "405e03.11j",	0x2000, 0x9c6fcdcd, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "405e02.10j",	0x2000, 0x0f145648, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "405e01.9j",	0x2000, 0x6a27f1b1, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "405e13.7a",	0x2000, 0x1ec7458b, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 Code

	{ "405e12.22f",	0x4000, 0xe02c54da, 3 | BRF_GRA },           //  6 Background Tiles

	{ "405e06.14g",	0x2000, 0x7e2f5bb2, 4 | BRF_GRA },           //  7 Sprites
	{ "405e07.16g",	0x2000, 0x963a44f9, 4 | BRF_GRA },           //  8
	{ "405e08.17g",	0x2000, 0x63901deb, 4 | BRF_GRA },           //  9
	{ "405e09.19g",	0x2000, 0xe1873677, 4 | BRF_GRA },           // 10
	{ "405e10.20g",	0x2000, 0x824815e8, 4 | BRF_GRA },           // 11
	{ "405e11.22g",	0x2000, 0xdca9b447, 4 | BRF_GRA },           // 12

	{ "405e17.5a",	0x0100, 0xb4c36d57, 5 | BRF_GRA },           // 13 Color data
	{ "405e16.4a",	0x0100, 0x0b7b03b8, 5 | BRF_GRA },           // 14
	{ "405e18.6a",	0x0100, 0x9e533bad, 5 | BRF_GRA },           // 15
	{ "405e20.19d",	0x0100, 0x8ca6de2f, 5 | BRF_GRA },           // 16
	{ "405e19.16d",	0x0100, 0xe0bc782f, 5 | BRF_GRA },           // 17

	{ "405e15.11f",	0x2000, 0x01bb5ce9, 6 | BRF_SND },           // 18 VLM data
};

STD_ROM_PICK(sbaskete)
STD_ROM_FN(sbaskete)

struct BurnDriver BurnDrvSbaskete = {
	"sbaskete", "sbasketb", NULL, NULL, "1984",
	"Super Basketball (version E, encrypted)\0", NULL, "Konami", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_PREFIX_KONAMI, GBF_SPORTSMISC, 0,
	NULL, sbasketeRomInfo, sbasketeRomName, NULL, NULL, NULL, NULL, SbasketbInputInfo, SbasketbDIPInfo,
	SbasketgInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1100,
	224, 256, 3, 4
};
