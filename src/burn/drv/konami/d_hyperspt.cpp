// FB Alpha Hyper Sports driver module
// Based on MAME driver by Chris Hardy

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "z80_intf.h"
#include "vlm5030.h"
#include "sn76496.h"
#include "dac.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6809ROM;
static UINT8 *DrvM6809DecROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvSndROM;
static UINT8 *DrvColPROM;
static UINT8 *DrvNVRAM;
static UINT8 *DrvM6809RAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvColRAM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvTransTable;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 previous_sound_irq;
static UINT8 flipscreen;
static UINT8 irq_enable;
static UINT8 soundlatch;
static UINT16 last_sound_addr;
static UINT8 sn76496_latch;
static INT32 watchdog;
static INT32 game_select = 0;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[9];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo HypersptInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 3"	},

	{"P3 Coin",		BIT_DIGITAL,	DrvJoy1 + 2,	"p3 coin"	},
	{"P3 Start",		BIT_DIGITAL,	DrvJoy2 + 3,	"p3 start"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy3 + 2,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy3 + 1,	"p3 fire 2"	},
	{"P3 Button 3",		BIT_DIGITAL,	DrvJoy3 + 0,	"p3 fire 3"	},

	{"P4 Coin",		BIT_DIGITAL,	DrvJoy1 + 5,	"p4 coin"	},
	{"P4 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p4 start"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy3 + 6,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p4 fire 2"	},
	{"P4 Button 3",		BIT_DIGITAL,	DrvJoy3 + 4,	"p4 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Hyperspt)

static struct BurnInputInfo RoadfInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Roadf)

static struct BurnDIPInfo HypersptDIPList[]=
{
	{0x15, 0xff, 0xff, 0xff, NULL			},
	{0x16, 0xff, 0xff, 0x49, NULL			},

	{0   , 0xfe, 0   ,   16, "Coin A"		},
	{0x15, 0x01, 0x0f, 0x02, "4 Coins 1 Credits"	},
	{0x15, 0x01, 0x0f, 0x05, "3 Coins 1 Credits"	},
	{0x15, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x15, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x15, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x15, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x15, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x15, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x15, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x15, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x15, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x15, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x15, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x15, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x15, 0x01, 0x0f, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,   16, "Coin B"		},
	{0x15, 0x01, 0xf0, 0x20, "4 Coins 1 Credits"	},
	{0x15, 0x01, 0xf0, 0x50, "3 Coins 1 Credits"	},
	{0x15, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"	},
	{0x15, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"	},
	{0x15, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x15, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"	},
	{0x15, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"	},
	{0x15, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x15, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x15, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x15, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x15, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x15, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x15, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x15, 0x01, 0xf0, 0x00, "No Coin B"		},

	{0   , 0xfe, 0   ,    2, "After Last Event"	},
	{0x16, 0x01, 0x01, 0x01, "Game Over"		},
	{0x16, 0x01, 0x01, 0x00, "Game Continues"	},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x16, 0x01, 0x02, 0x00, "Upright"		},
	{0x16, 0x01, 0x02, 0x02, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x16, 0x01, 0x04, 0x04, "Off"			},
	{0x16, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "World Records"	},
	{0x16, 0x01, 0x08, 0x08, "Don't Erase"		},
	{0x16, 0x01, 0x08, 0x00, "Erase on Reset"	},

	{0   , 0xfe, 0   ,   16, "Difficulty"		},
	{0x16, 0x01, 0xf0, 0xf0, "Easy 1"		},
	{0x16, 0x01, 0xf0, 0xe0, "Easy 2"		},
	{0x16, 0x01, 0xf0, 0xd0, "Easy 3"		},
	{0x16, 0x01, 0xf0, 0xc0, "Easy 4"		},
	{0x16, 0x01, 0xf0, 0xb0, "Normal 1"		},
	{0x16, 0x01, 0xf0, 0xa0, "Normal 2"		},
	{0x16, 0x01, 0xf0, 0x90, "Normal 3"		},
	{0x16, 0x01, 0xf0, 0x80, "Normal 4"		},
	{0x16, 0x01, 0xf0, 0x70, "Normal 5"		},
	{0x16, 0x01, 0xf0, 0x60, "Normal 6"		},
	{0x16, 0x01, 0xf0, 0x50, "Normal 7"		},
	{0x16, 0x01, 0xf0, 0x40, "Normal 8"		},
	{0x16, 0x01, 0xf0, 0x30, "Difficult 1"		},
	{0x16, 0x01, 0xf0, 0x20, "Difficult 2"		},
	{0x16, 0x01, 0xf0, 0x10, "Difficult 3"		},
	{0x16, 0x01, 0xf0, 0x00, "Difficult 4"		},
};

STDDIPINFO(Hyperspt)

static struct BurnDIPInfo RoadfDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x2d, NULL			},

	{0   , 0xfe, 0   ,   16, "Coin A"		},
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
	{0x12, 0x01, 0x0f, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,   16, "Coin B"		},
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
	{0x12, 0x01, 0xf0, 0x00, "No Coin B"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x13, 0x01, 0x01, 0x01, "No"			},
	{0x13, 0x01, 0x01, 0x00, "Yes"			},

	{0   , 0xfe, 0   ,    4, "Number of Opponents"	},
	{0x13, 0x01, 0x06, 0x06, "Few"			},
	{0x13, 0x01, 0x06, 0x04, "Normal"		},
	{0x13, 0x01, 0x06, 0x02, "Many"			},
	{0x13, 0x01, 0x06, 0x00, "Great Many"		},

	{0   , 0xfe, 0   ,    2, "Speed of Opponents"	},
	{0x13, 0x01, 0x08, 0x08, "Fast"			},
	{0x13, 0x01, 0x08, 0x00, "Slow"			},

	{0   , 0xfe, 0   ,    4, "Fuel Consumption"	},
	{0x13, 0x01, 0x30, 0x30, "Slow"			},
	{0x13, 0x01, 0x30, 0x20, "Normal"		},
	{0x13, 0x01, 0x30, 0x10, "Fast"			},
	{0x13, 0x01, 0x30, 0x00, "Very Fast"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x13, 0x01, 0x40, 0x00, "Upright"		},
	{0x13, 0x01, 0x40, 0x40, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x80, 0x80, "Off"			},
	{0x13, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Roadf)

static void hyperspt_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x1400:
			watchdog = 0;
		return;

		case 0x1480:
			flipscreen = data & 1;
		return;

		case 0x1481:
			if (previous_sound_irq == 0 && data != 0) {
				ZetSetVector(0xff);
				ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
			}
			previous_sound_irq = data;
		return;

		case 0x1483:
			// coin counter
		return;

		case 0x1487:
			irq_enable = data & 1;
		return;

		case 0x1500:
			soundlatch = data;
		return;
	}
}

static UINT8 hyperspt_main_read(UINT16 address)
{
	switch (address)
	{
		case 0x1600:
			return DrvDips[1];

		case 0x1680:
			return DrvInputs[0];

		case 0x1681:
			return DrvInputs[1];

		case 0x1682:
			return DrvInputs[2] ^ ((game_select > 1) ? 0x40 : 0); // roadf needs this

		case 0x1683:
			return DrvDips[0];
	}

	return 0;
}

static void __fastcall hyperspt_sound_write(UINT16 address, UINT8 data)
{
	if ((address & 0xe000) == 0xc000) {
		INT32 offset = address & 0x1fff;
		INT32 changes = offset ^ last_sound_addr;
		if (changes & 0x10) vlm5030_st(0, offset & 0x10);
		if (changes & 0x20) vlm5030_rst(0, offset & 0x20);
		last_sound_addr = offset;
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

static UINT8 __fastcall hyperspt_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x6000:
			return soundlatch;

		case 0x8000:
			return ((ZetTotalCycles() / 1024) & 0x3) | (vlm5030_bsy(0) ? 4 : 0);
	}

	return 0;
}

static tilemap_callback( hyperspt )
{
	UINT8 attr = DrvColRAM[offs];
	INT32 code = DrvVidRAM[offs] + (((attr & 0x80) << 1) | ((attr & 0x40) << 3));

	TILE_SET_INFO(0, code, attr, TILE_FLIPXY(attr >> 4));
}

static tilemap_callback( roadf )
{
	UINT8 attr = DrvColRAM[offs];
	INT32 code = DrvVidRAM[offs] + (((attr & 0x80) << 1) | ((attr & 0x60) << 4));

	TILE_SET_INFO(0, code, attr, (attr & 0x10) ? TILE_FLIPX : 0);
}

static INT32 DrvDACSync()
{
	return (INT32)(float)(nBurnSoundLen * (ZetTotalCycles() / (3579545.000 / (nBurnFPS / 100.000))));
}

static UINT32 DrvVLMSync(INT32 samples_rate)
{
	return (samples_rate * ZetTotalCycles()) / 59659;
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam,   0, RamEnd - AllRam);

		previous_sound_irq = 0;
		flipscreen = 0;
		irq_enable = 0;
		soundlatch = 0;
		last_sound_addr = 0;
		sn76496_latch = 0;
	}

	M6809Open(0);
	M6809Reset();
	M6809Close();

	ZetOpen(0);
	vlm5030Reset(0);
	DACReset();
	ZetReset();
	ZetClose();
	watchdog = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6809ROM		= Next; Next += 0x010000;
	DrvM6809DecROM		= Next; Next += 0x010000;
	DrvZ80ROM		= Next; Next += 0x004000;

	DrvGfxROM0		= Next; Next += 0x020000;
	DrvGfxROM1		= Next; Next += 0x020000;

	DrvSndROM		= Next; Next += 0x010000;

	DrvColPROM		= Next; Next += 0x000220;

	DrvNVRAM		= Next; Next += 0x000800;

	DrvTransTable		= Next; Next += 0x000200;

	DrvPalette		= (UINT32*)Next; Next += 0x200 * sizeof(UINT32);

	AllRam			= Next;

	DrvM6809RAM		= Next; Next += 0x000800;
	DrvSprRAM		= Next; Next += 0x000100;

	DrvVidRAM		= Next; Next += 0x000800;
	DrvColRAM		= Next; Next += 0x000800;

	DrvZ80RAM		= Next; Next += 0x001000;

	RamEnd			= Next;
	MemEnd			= Next;

	return 0;
}

static void DrvGfxDecode()
{
	INT32 Plane[4]  = { 0x8000*8+4, 0x8000*8+0, 4, 0 };
	INT32 XOffs[16] = { STEP4(0,1), STEP4(64,1), STEP4(128,1), STEP4(192,1) };
	INT32 YOffs[16] = { STEP8(0,8), STEP8(256,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x10000);

	memcpy (tmp, DrvGfxROM1, 0x10000);

	GfxDecode(0x800, 4,  8,  8, Plane, XOffs, YOffs, 0x080, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM0, 0x10000);

	GfxDecode(0x200, 4, 16, 16, Plane, XOffs, YOffs, 0x200, tmp, DrvGfxROM0);

	BurnFree(tmp);
}

static void Konami1Decode()
{
	for (INT32 i = 0; i < 0x10000; i++)
	{
		UINT8 xor1 = 0;

		if ( i & 0x02) xor1 |= 0x80;
		else           xor1 |= 0x20;
		if ( i & 0x08) xor1 |= 0x08;
		else           xor1 |= 0x02;

		DrvM6809DecROM[i] = DrvM6809ROM[i] ^ xor1;
	}
}

static INT32 DrvInit(INT32 select)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	game_select = select;

	if (select == 0) // hyperspt, hpolym84, 
	{
		if (BurnLoadRom(DrvM6809ROM + 0x04000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x06000,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x08000,  2, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x0a000,  3, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x0c000,  4, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x0e000,  5, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM   + 0x00000,  6, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM   + 0x02000,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0  + 0x00000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x02000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x04000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x06000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x08000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x0a000, 13, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x0c000, 14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x0e000, 15, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1  + 0x00000, 16, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x02000, 17, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x08000, 18, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x0a000, 19, 1)) return 1;

		if (BurnLoadRom(DrvColPROM  + 0x00000, 20, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00020, 21, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00120, 22, 1)) return 1;

		if (BurnLoadRom(DrvSndROM   + 0x00000, 23, 1)) return 1;
	}
	else if (select == 1) // hypersptb
	{
		if (BurnLoadRom(DrvM6809ROM + 0x06000,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x08000,  2, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x0a000,  3, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x0c000,  4, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x0e000,  5, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM   + 0x00000,  6, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM   + 0x02000,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0  + 0x0c000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x00000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x02000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x04000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x06000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x08000, 13, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x0a000, 14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x0c000, 15, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1  + 0x00000, 16, 1)) return 1;
		memcpy (DrvGfxROM1, DrvGfxROM1 + 0x4000, 0x4000);
		memset (DrvGfxROM1 + 0x4000, 0, 0x4000);
		if (BurnLoadRom(DrvGfxROM1  + 0x02000, 17, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x08000, 18, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x0a000, 19, 1)) return 1;

		if (BurnLoadRom(DrvColPROM  + 0x00000, 20, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00020, 21, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00120, 22, 1)) return 1;

		if (BurnLoadRom(DrvSndROM   + 0x08000, 23, 1)) return 1;
		if (BurnLoadRom(DrvSndROM   + 0x0a000, 24, 1)) return 1;
		if (BurnLoadRom(DrvSndROM   + 0x0c000, 25, 1)) return 1;
		if (BurnLoadRom(DrvSndROM   + 0x0e000, 26, 1)) return 1;
	}
	else if (select == 2) // roadf, roadf2
	{
		if (BurnLoadRom(DrvM6809ROM + 0x04000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x06000,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x08000,  2, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x0a000,  3, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x0c000,  4, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x0e000,  5, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM   + 0x00000,  6, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0  + 0x00000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x08000,  8, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1  + 0x00000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x04000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x08000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x0c000, 12, 1)) return 1;

		if (BurnLoadRom(DrvColPROM  + 0x00000, 13, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00020, 14, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00120, 15, 1)) return 1;
	}
	else if (select == 3) // roadf3
	{
		if (BurnLoadRom(DrvM6809ROM + 0x04000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x08000,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x0c000,  2, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM   + 0x00000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0  + 0x00000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x02000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x08000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x0A000,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1  + 0x00000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x02000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x04000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x08000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x0a000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x0c000, 13, 1)) return 1;

		if (BurnLoadRom(DrvColPROM  + 0x00000, 14, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00020, 15, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00120, 16, 1)) return 1;
	}

	Konami1Decode();
	DrvGfxDecode();

	M6809Init(1);
	M6809Open(0);
	M6809MapMemory(DrvSprRAM,		0x1000, 0x10ff, MAP_RAM);
	M6809MapMemory(DrvVidRAM,		0x2000, 0x27ff, MAP_RAM);
	M6809MapMemory(DrvColRAM,		0x2800, 0x2fff, MAP_RAM);
	M6809MapMemory(DrvM6809RAM,		0x3000, 0x37ff, MAP_RAM);
	M6809MapMemory(DrvNVRAM,		0x3800, 0x3fff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM + 0x4000,	0x4000, 0xffff, MAP_READ);
	M6809MapMemory(DrvM6809DecROM + 0x4000,	0x4000, 0xffff, MAP_FETCH);
	M6809SetWriteHandler(hyperspt_main_write);
	M6809SetReadHandler(hyperspt_main_read);
	M6809Close();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,			0x4000, 0x4fff, MAP_RAM);
	ZetSetWriteHandler(hyperspt_sound_write);
	ZetSetReadHandler(hyperspt_sound_read);
	ZetClose();

	SN76489AInit(0, 1789772, 0);
	SN76496SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	vlm5030Init(0, 3579545, DrvVLMSync, DrvSndROM, 0x2000, 1);
	vlm5030SetAllRoutes(0, (select != 0) ? 0 : 1.00, BURN_SND_ROUTE_BOTH);

	DACInit(0, 0, 1, DrvDACSync);
	DACSetRoute(0, 0.40, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, (select > 1) ? roadf_map_callback : hyperspt_map_callback, 8, 8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM1, 4, 8, 8, 0x20000, 0x100, 0xf);
	GenericTilemapSetScrollRows(0, 32);
	GenericTilemapSetOffsets(0, 0, -16);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6809Exit();
	ZetExit();
	vlm5030Exit();
	SN76496Exit();
	DACExit();

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	UINT32 pal[0x20];

	for (INT32 i = 0; i < 0x20; i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 0x01;
		INT32 r = bit0 * 33 + bit1 * 71 + bit2 * 151;

		bit0 = (DrvColPROM[i] >> 3) & 0x01;
		bit1 = (DrvColPROM[i] >> 4) & 0x01;
		bit2 = (DrvColPROM[i] >> 5) & 0x01;
		INT32 g = bit0 * 33 + bit1 * 71 + bit2 * 151;

		bit0 = (DrvColPROM[i] >> 6) & 0x01;
		bit1 = (DrvColPROM[i] >> 7) & 0x01;
		INT32 b = bit0 * 81 + bit1 * 174;

		pal[i] = BurnHighCol(r,g,b,0);
	}

	for (INT32 i = 0; i < 0x200; i++) {
		DrvPalette[i] = pal[(DrvColPROM[0x020 + i] & 0xf) + ((i&0x100)>>4)];

		DrvTransTable[i] = (DrvColPROM[0x20 + i] == 0) ? 0 : 1;
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0xc0 - 4;offs >= 0;offs -= 4)
	{
		INT32 sx = DrvSprRAM[offs + 3];
		INT32 sy = 240 - DrvSprRAM[offs + 1];
		INT32 code = DrvSprRAM[offs + 2] + 8 * (DrvSprRAM[offs] & 0x20);
		INT32 color = DrvSprRAM[offs] & 0x0f;
		INT32 flipx = ~DrvSprRAM[offs] & 0x40;
		INT32 flipy = DrvSprRAM[offs] & 0x80;

		if (flipscreen)
		{
			sy = 240 - sy;
			flipy = !flipy;
		}

		sy += 1 - 16;

		RenderTileTranstab(pTransDraw, DrvGfxROM0, code, color*16, 0, sx, sy, flipx, flipy, 16, 16, DrvTransTable);
		RenderTileTranstab(pTransDraw, DrvGfxROM0, code, color*16, 0, sx - 256, sy, flipx, flipy, 16, 16, DrvTransTable);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	UINT8 *scroll = DrvSprRAM + 0xc0;

	GenericTilemapSetFlip(0, (flipscreen) ? TMAP_FLIPXY : 0);

	for (INT32 row = 0; row < 32; row++)
	{
		INT32 scrollx = scroll[row * 2] + (scroll[(row * 2) + 1] & 0x01) * 256;
		if (flipscreen) scrollx = -scrollx;
		GenericTilemapSetScrollRow(0, row, scrollx);
	}

	if (~nBurnLayer & 1) BurnTransferClear();
	if ( nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);
	if ( nBurnLayer & 2) draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	watchdog++;
	if (watchdog >= 180) {
		DrvDoReset(0);
	}

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

	ZetNewFrame();

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 1536000 / 60, 3579545 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };
	INT32 nSoundBufferPos = 0, nSegment = 0;

	ZetOpen(0);
	M6809Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone[0] += M6809Run(nCyclesTotal[0] / nInterleave);

		if (i == 255 && irq_enable) M6809SetIRQLine(0, CPU_IRQSTATUS_AUTO);

		nSegment = (i + 1) * nCyclesTotal[1] / nInterleave;
		nCyclesDone[1] += ZetRun(nSegment - nCyclesDone[1]);

		// Render Sound Segment
		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			SN76496Update(0, pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	// Make sure the buffer is entirely filled.
	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			SN76496Update(0, pSoundBuf, nSegmentLength);
		}
		// vlm5030 won't interlace, so just run it at the end of the frame..
		if (game_select == 0) vlm5030Update(0, pBurnSoundOut, nBurnSoundLen);
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
	}

	M6809Close();
	ZetClose();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029698;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		M6809Scan(nAction);
		ZetScan(nAction);

		vlm5030Scan(nAction);
		SN76496Scan(nAction, pnMin);
		DACScan(nAction, pnMin);

		SCAN_VAR(sn76496_latch);
		SCAN_VAR(previous_sound_irq);
		SCAN_VAR(flipscreen);
		SCAN_VAR(irq_enable);
		SCAN_VAR(soundlatch);
		SCAN_VAR(last_sound_addr);
	}

	return 0;
}


// Hyper Sports

static struct BurnRomInfo hypersptRomDesc[] = {
	{ "c01",		0x2000, 0x0c720eeb, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "c02",		0x2000, 0x560258e0, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "c03",		0x2000, 0x9b01c7e6, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "c04",		0x2000, 0x10d7e9a2, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "c05",		0x2000, 0xb105a8cd, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "c06",		0x2000, 0x1a34a849, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "c10",		0x2000, 0x3dc1a6ff, 2 | BRF_PRG | BRF_ESS }, //  6 Z80 Code
	{ "c09",		0x2000, 0x9b525c3e, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "c14",		0x2000, 0xc72d63be, 3 | BRF_GRA },           //  8 Background Tiles
	{ "c13",		0x2000, 0x76565608, 3 | BRF_GRA },           //  9
	{ "c12",		0x2000, 0x74d2cc69, 3 | BRF_GRA },           // 10
	{ "c11",		0x2000, 0x66cbcb4d, 3 | BRF_GRA },           // 11
	{ "c18",		0x2000, 0xed25e669, 3 | BRF_GRA },           // 12
	{ "c17",		0x2000, 0xb145b39f, 3 | BRF_GRA },           // 13
	{ "c16",		0x2000, 0xd7ff9f2b, 3 | BRF_GRA },           // 14
	{ "c15",		0x2000, 0xf3d454e6, 3 | BRF_GRA },           // 15

	{ "c26",		0x2000, 0xa6897eac, 4 | BRF_GRA },           // 16 Sprites
	{ "c24",		0x2000, 0x5fb230c0, 4 | BRF_GRA },           // 17
	{ "c22",		0x2000, 0xed9271a0, 4 | BRF_GRA },           // 18
	{ "c20",		0x2000, 0x183f4324, 4 | BRF_GRA },           // 19

	{ "c03_c27.bin",	0x0020, 0xbc8a5956, 5 | BRF_GRA },           // 20 Color Proms
	{ "j12_c28.bin",	0x0100, 0x2c891d59, 5 | BRF_GRA },           // 21
	{ "a09_c29.bin",	0x0100, 0x811a3f3f, 5 | BRF_GRA },           // 22

	{ "c08",		0x2000, 0xe8f8ea78, 6 | BRF_GRA },           // 23 VLM Samples
};

STD_ROM_PICK(hyperspt)
STD_ROM_FN(hyperspt)

static INT32 HypersptInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvHyperspt = {
	"hyperspt", NULL, NULL, NULL, "1984",
	"Hyper Sports\0", NULL, "Konami (Centuri license)", "GX330",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PREFIX_KONAMI, GBF_SPORTSMISC, 0,
	NULL, hypersptRomInfo, hypersptRomName, NULL, NULL, HypersptInputInfo, HypersptDIPInfo,
	HypersptInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// Hyper Sports (bootleg)

static struct BurnRomInfo hypersptbRomDesc[] = {
	{ "1.5g",		0x2000, 0x0cfc68a7, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "2.7g",		0x2000, 0x560258e0, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.8g",		0x2000, 0x9b01c7e6, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.11g",		0x2000, 0x4ed32240, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "5.13g",		0x2000, 0xb105a8cd, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "6.15g",		0x2000, 0x1a34a849, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "12.17a",		0x2000, 0x3dc1a6ff, 2 | BRF_PRG | BRF_ESS }, //  6 Z80 Code
	{ "11.15a",		0x2000, 0x9b525c3e, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "20.15g",		0x8000, 0x551d222f, 3 | BRF_GRA },           //  8 Background Tiles
	{ "16.19j",		0x2000, 0xc72d63be, 3 | BRF_GRA },           //  9
	{ "15.18j",		0x2000, 0x76565608, 3 | BRF_GRA },           // 10
	{ "14.17j",		0x2000, 0x74d2cc69, 3 | BRF_GRA },           // 11
	{ "13.15j",		0x2000, 0x66cbcb4d, 3 | BRF_GRA },           // 12
	{ "17.19g",		0x2000, 0xed25e669, 3 | BRF_GRA },           // 13
	{ "18.18g",		0x2000, 0xb145b39f, 3 | BRF_GRA },           // 14
	{ "19.17g",		0x2000, 0xd7ff9f2b, 3 | BRF_GRA },           // 15

	{ "b.14a",		0x8000, 0x8fd90bd2, 4 | BRF_GRA },           // 16 Sprites
	{ "a.12a",		0x2000, 0xa3d422c6, 4 | BRF_GRA },           // 17
	{ "d.14c",		0x2000, 0xed9271a0, 4 | BRF_GRA },           // 18
	{ "c.12c",		0x2000, 0x0c8ed053, 4 | BRF_GRA },           // 19

	{ "mmi6331-1.3c",	0x0020, 0xbc8a5956, 5 | BRF_GRA },           // 20 Color Proms
	{ "j12_c28.bin",	0x0100, 0x2c891d59, 5 | BRF_GRA },           // 21
	{ "a09_c29.bin",	0x0100, 0x811a3f3f, 5 | BRF_GRA },           // 22

	{ "10.20c",		0x2000, 0xa4cddeb8, 6 | BRF_SND },           // 23 M6502 Code / Adpcm
	{ "9.20cd",		0x2000, 0xe9919365, 6 | BRF_SND },           // 24
	{ "8.20d",		0x2000, 0x49a06454, 6 | BRF_SND },           // 25
	{ "7.20b",		0x2000, 0x607a36df, 6 | BRF_SND },           // 26
};

STD_ROM_PICK(hypersptb)
STD_ROM_FN(hypersptb)

static INT32 HypersptbInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvHypersptb = {
	"hypersptb", "hyperspt", NULL, NULL, "1984",
	"Hyper Sports (bootleg)\0", "imcomplete sound", "bootleg", "GX330",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_PREFIX_KONAMI, GBF_SPORTSMISC, 0,
	NULL, hypersptbRomInfo, hypersptbRomName, NULL, NULL, HypersptInputInfo, HypersptDIPInfo,
	HypersptbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// Hyper Olympic '84

static struct BurnRomInfo hpolym84RomDesc[] = {
	{ "c01",		0x2000, 0x0c720eeb, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "c02",		0x2000, 0x560258e0, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "c03",		0x2000, 0x9b01c7e6, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "330e04.bin",		0x2000, 0x9c5e2934, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "c05",		0x2000, 0xb105a8cd, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "c06",		0x2000, 0x1a34a849, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "c10",		0x2000, 0x3dc1a6ff, 2 | BRF_PRG | BRF_ESS }, //  6 Z80 Code
	{ "c09",		0x2000, 0x9b525c3e, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "c14",		0x2000, 0xc72d63be, 3 | BRF_GRA },           //  8 Background Tiles
	{ "c13",		0x2000, 0x76565608, 3 | BRF_GRA },           //  9
	{ "c12",		0x2000, 0x74d2cc69, 3 | BRF_GRA },           // 10
	{ "c11",		0x2000, 0x66cbcb4d, 3 | BRF_GRA },           // 11
	{ "c18",		0x2000, 0xed25e669, 3 | BRF_GRA },           // 12
	{ "c17",		0x2000, 0xb145b39f, 3 | BRF_GRA },           // 13
	{ "c16",		0x2000, 0xd7ff9f2b, 3 | BRF_GRA },           // 14
	{ "c15",		0x2000, 0xf3d454e6, 3 | BRF_GRA },           // 15

	{ "c26",		0x2000, 0xa6897eac, 4 | BRF_GRA },           // 16 Sprites
	{ "330e24.bin",		0x2000, 0xf9bbfe1d, 4 | BRF_GRA },           // 17
	{ "c22",		0x2000, 0xed9271a0, 4 | BRF_GRA },           // 18
	{ "330e20.bin",		0x2000, 0x29969b92, 4 | BRF_GRA },           // 19

	{ "c03_c27.bin",	0x0020, 0xbc8a5956, 5 | BRF_GRA },           // 20 Color Proms
	{ "j12_c28.bin",	0x0100, 0x2c891d59, 5 | BRF_GRA },           // 21
	{ "a09_c29.bin",	0x0100, 0x811a3f3f, 5 | BRF_GRA },           // 22

	{ "c08",		0x2000, 0xe8f8ea78, 6 | BRF_GRA },           // 23 VLM Samples
};

STD_ROM_PICK(hpolym84)
STD_ROM_FN(hpolym84)

struct BurnDriver BurnDrvHpolym84 = {
	"hpolym84", "hyperspt", NULL, NULL, "1984",
	"Hyper Olympic '84\0", NULL, "Konami", "GX330",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_SPORTSMISC, 0,
	NULL, hpolym84RomInfo, hpolym84RomName, NULL, NULL, HypersptInputInfo, HypersptDIPInfo,
	HypersptInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// Road Fighter (set 1)

static struct BurnRomInfo roadfRomDesc[] = {
	{ "g05_g01.bin",	0x2000, 0xe2492a06, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "g07_f02.bin",	0x2000, 0x0bf75165, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "g09_g03.bin",	0x2000, 0xdde401f8, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "g11_f04.bin",	0x2000, 0xb1283c77, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "g13_f05.bin",	0x2000, 0x0ad4d796, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "g15_f06.bin",	0x2000, 0xfa42e0ed, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "a17_d10.bin",	0x2000, 0xc33c927e, 2 | BRF_PRG | BRF_ESS }, //  6 Z80 Code

	{ "j19_e14.bin",	0x4000, 0x16d2bcff, 3 | BRF_GRA },           //  7 Background Tiles
	{ "g19_e18.bin",	0x4000, 0x490685ff, 3 | BRF_GRA },           //  8

	{ "a14_e26.bin",	0x4000, 0xf5c738e2, 4 | BRF_GRA },           //  9 Sprites
	{ "a12_d24.bin",	0x2000, 0x2d82c930, 4 | BRF_GRA },           // 10
	{ "c14_e22.bin",	0x4000, 0xfbcfbeb9, 4 | BRF_GRA },           // 11
	{ "c12_d20.bin",	0x2000, 0x5e0cf994, 4 | BRF_GRA },           // 12

	{ "c03_c27.bin",	0x0020, 0x45d5e352, 5 | BRF_GRA },           // 13 Color Proms
	{ "j12_c28.bin",	0x0100, 0x2955e01f, 5 | BRF_GRA },           // 14
	{ "a09_c29.bin",	0x0100, 0x5b3b5f2a, 5 | BRF_GRA },           // 15
};

STD_ROM_PICK(roadf)
STD_ROM_FN(roadf)

static INT32 RoadfInit()
{
	return DrvInit(2);
}

struct BurnDriver BurnDrvRoadf = {
	"roadf", NULL, NULL, NULL, "1984",
	"Road Fighter (set 1)\0", NULL, "Konami", "GX461",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_PREFIX_KONAMI, GBF_RACING, 0,
	NULL, roadfRomInfo, roadfRomName, NULL, NULL, RoadfInputInfo, RoadfDIPInfo,
	RoadfInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Road Fighter (set 2)

static struct BurnRomInfo roadf2RomDesc[] = {
	{ "5g",			0x2000, 0xd8070d30, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "6g",			0x2000, 0x8b661672, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "8g",			0x2000, 0x714929e8, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "11g",		0x2000, 0x0f2c6b94, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "g13_f05.bin",	0x2000, 0x0ad4d796, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "g15_f06.bin",	0x2000, 0xfa42e0ed, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "a17_d10.bin",	0x2000, 0xc33c927e, 2 | BRF_PRG | BRF_ESS }, //  6 Z80 Code

	{ "j19_e14.bin",	0x4000, 0x16d2bcff, 3 | BRF_GRA },           //  7 Background Tiles
	{ "g19_e18.bin",	0x4000, 0x490685ff, 3 | BRF_GRA },           //  8

	{ "a14_e26.bin",	0x4000, 0xf5c738e2, 4 | BRF_GRA },           //  9 Sprites
	{ "a12_d24.bin",	0x2000, 0x2d82c930, 4 | BRF_GRA },           // 10
	{ "c14_e22.bin",	0x4000, 0xfbcfbeb9, 4 | BRF_GRA },           // 11
	{ "c12_d20.bin",	0x2000, 0x5e0cf994, 4 | BRF_GRA },           // 12

	{ "c03_c27.bin",	0x0020, 0x45d5e352, 5 | BRF_GRA },           // 13 Color Proms
	{ "j12_c28.bin",	0x0100, 0x2955e01f, 5 | BRF_GRA },           // 14
	{ "a09_c29.bin",	0x0100, 0x5b3b5f2a, 5 | BRF_GRA },           // 15
};

STD_ROM_PICK(roadf2)
STD_ROM_FN(roadf2)

struct BurnDriver BurnDrvRoadf2 = {
	"roadf2", "roadf", NULL, NULL, "1984",
	"Road Fighter (set 2)\0", NULL, "Konami", "GX461",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_PREFIX_KONAMI, GBF_RACING, 0,
	NULL, roadf2RomInfo, roadf2RomName, NULL, NULL, RoadfInputInfo, RoadfDIPInfo,
	RoadfInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Road Fighter (set 3, conversion hack on Hyper Sports PCB)

static struct BurnRomInfo roadf3RomDesc[] = {
	{ "1-2.g7",		0x4000, 0x93b168f2, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "3-4.g11",		0x4000, 0xb9ba77f0, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "5-6.g15",		0x4000, 0x91c1788b, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "sound7.a9",		0x2000, 0xc33c927e, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 Code

	{ "j19.bin",		0x2000, 0x5eeb0283, 3 | BRF_GRA },           //  4 Background Tiles
	{ "j18.bin",		0x2000, 0x43c1590d, 3 | BRF_GRA },           //  5
	{ "g19.bin",		0x2000, 0xf2819ef3, 3 | BRF_GRA },           //  6
	{ "g18.bin",		0x2000, 0xdbd1d844, 3 | BRF_GRA },           //  7

	{ "a14.bin",		0x2000, 0x0b595c1e, 4 | BRF_GRA },           //  8 Sprites
	{ "a13.bin",		0x2000, 0x4f0acc76, 4 | BRF_GRA },           //  9
	{ "a12.bin",		0x2000, 0x2d82c930, 4 | BRF_GRA },           // 10
	{ "c14.bin",		0x2000, 0x412a9dda, 4 | BRF_GRA },           // 11
	{ "c13.bin",		0x2000, 0x0c2d50ae, 4 | BRF_GRA },           // 12
	{ "c12.bin",		0x2000, 0x5e0cf994, 4 | BRF_GRA },           // 13

	{ "6331.c3",		0x0020, 0x45d5e352, 5 | BRF_GRA },           // 14 Color Proms
	{ "82s129.j12",		0x0100, 0x2955e01f, 5 | BRF_GRA },           // 15
	{ "82s129.a9",		0x0100, 0x5b3b5f2a, 5 | BRF_GRA },           // 16
};

STD_ROM_PICK(roadf3)
STD_ROM_FN(roadf3)

static INT32 Roadf3Init()
{
	return DrvInit(3);
}

struct BurnDriver BurnDrvRoadf3 = {
	"roadf3", "roadf", NULL, NULL, "1984",
	"Road Fighter (set 3, conversion hack on Hyper Sports PCB)\0", NULL, "hack", "GX330",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_PREFIX_KONAMI, GBF_RACING, 0,
	NULL, roadf3RomInfo, roadf3RomName, NULL, NULL, RoadfInputInfo, RoadfDIPInfo,
	Roadf3Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};
