// FB Alpha Hole Land driver module
// Based on MAME driver by Mathis Rosenhauer

#include "tiles_generic.h"
#include "z80_intf.h"
#include "ay8910.h"
#include "sp0256.h"
#include "watchdog.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvSndROM;
static UINT8 *DrvNVRAM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvVidRAM;
static UINT8 *DrvColRAM;
static UINT8 *DrvSprRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 flipscreen[2];
static UINT8 palette_offset;
static UINT8 scrollx;

static INT32 game_select;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static struct BurnInputInfo HolelandInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 start"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Holeland)

static struct BurnInputInfo CrzrallyInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 start"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Crzrally)

static struct BurnDIPInfo HolelandDIPList[]=
{
	{0x0d, 0xff, 0xff, 0x9c, NULL				},
	{0x0e, 0xff, 0xff, 0xc4, NULL				},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x0d, 0x01, 0x01, 0x00, "Off"				},
	{0x0d, 0x01, 0x01, 0x01, "On"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x0d, 0x01, 0x02, 0x02, "Off"				},
	{0x0d, 0x01, 0x02, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Language"			},
	{0x0d, 0x01, 0x04, 0x00, "English"			},
	{0x0d, 0x01, 0x04, 0x04, "Nihongo"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x0d, 0x01, 0x08, 0x08, "Upright"			},
	{0x0d, 0x01, 0x08, 0x00, "Cocktail"			},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x0d, 0x01, 0x10, 0x10, "Off"				},
	{0x0d, 0x01, 0x10, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x0d, 0x01, 0x20, 0x00, "60000"			},
	{0x0d, 0x01, 0x20, 0x20, "90000"			},

	{0   , 0xfe, 0   ,    2, "Fase 3 Difficulty"},
	{0x0d, 0x01, 0x40, 0x00, "Easy"				},
	{0x0d, 0x01, 0x40, 0x40, "Hard"				},

	{0   , 0xfe, 0   ,    2, "Coin Mode"		},
	{0x0d, 0x01, 0x80, 0x80, "A"				},
	{0x0d, 0x01, 0x80, 0x00, "B"				},

	{0   , 0xfe, 0   ,    4, "Coin Case"		},
	{0x0e, 0x01, 0x03, 0x00, "1"				},
	{0x0e, 0x01, 0x03, 0x01, "2"				},
	{0x0e, 0x01, 0x03, 0x02, "3"				},
	{0x0e, 0x01, 0x03, 0x03, "4"				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x0e, 0x01, 0x0c, 0x00, "2"				},
	{0x0e, 0x01, 0x0c, 0x04, "3"				},
	{0x0e, 0x01, 0x0c, 0x08, "4"				},
	{0x0e, 0x01, 0x0c, 0x0c, "5"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x0e, 0x01, 0x30, 0x00, "Very Easy"		},
	{0x0e, 0x01, 0x30, 0x10, "Easy"				},
	{0x0e, 0x01, 0x30, 0x20, "Hard"				},
	{0x0e, 0x01, 0x30, 0x30, "Very Hard"		},

	{0   , 0xfe, 0   ,    2, "Monsters"			},
	{0x0e, 0x01, 0x40, 0x40, "Min"				},
	{0x0e, 0x01, 0x40, 0x00, "Max"				},

	{0   , 0xfe, 0   ,    2, "Mode"				},
	{0x0e, 0x01, 0x80, 0x00, "Stop"				},
	{0x0e, 0x01, 0x80, 0x80, "Play"				},
};

STDDIPINFO(Holeland)

static struct BurnDIPInfo Holeland2DIPList[]=
{
	{0x0d, 0xff, 0xff, 0x9c, NULL				},
	{0x0e, 0xff, 0xff, 0xc4, NULL				},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x0d, 0x01, 0x01, 0x00, "Off"				},
	{0x0d, 0x01, 0x01, 0x01, "On"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x0d, 0x01, 0x02, 0x02, "Off"				},
	{0x0d, 0x01, 0x02, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Language"			},
	{0x0d, 0x01, 0x04, 0x00, "English"			},
	{0x0d, 0x01, 0x04, 0x04, "Spanish"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x0d, 0x01, 0x08, 0x08, "Upright"			},
	{0x0d, 0x01, 0x08, 0x00, "Cocktail"			},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x0d, 0x01, 0x10, 0x10, "Off"				},
	{0x0d, 0x01, 0x10, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x0d, 0x01, 0x20, 0x00, "60000"			},
	{0x0d, 0x01, 0x20, 0x20, "90000"			},

	{0   , 0xfe, 0   ,    2, "Fase 3 Difficulty"},
	{0x0d, 0x01, 0x40, 0x00, "Easy"				},
	{0x0d, 0x01, 0x40, 0x40, "Hard"				},

	{0   , 0xfe, 0   ,    2, "Coin Mode"		},
	{0x0d, 0x01, 0x80, 0x80, "A"				},
	{0x0d, 0x01, 0x80, 0x00, "B"				},

	{0   , 0xfe, 0   ,    4, "Coin Case"		},
	{0x0e, 0x01, 0x03, 0x00, "1"				},
	{0x0e, 0x01, 0x03, 0x01, "2"				},
	{0x0e, 0x01, 0x03, 0x02, "3"				},
	{0x0e, 0x01, 0x03, 0x03, "4"				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x0e, 0x01, 0x0c, 0x00, "2"				},
	{0x0e, 0x01, 0x0c, 0x04, "3"				},
	{0x0e, 0x01, 0x0c, 0x08, "4"				},
	{0x0e, 0x01, 0x0c, 0x0c, "5"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x0e, 0x01, 0x30, 0x00, "Very Easy"		},
	{0x0e, 0x01, 0x30, 0x10, "Easy"				},
	{0x0e, 0x01, 0x30, 0x20, "Hard"				},
	{0x0e, 0x01, 0x30, 0x30, "Very Hard"		},

	{0   , 0xfe, 0   ,    2, "Monsters"			},
	{0x0e, 0x01, 0x40, 0x40, "Min"				},
	{0x0e, 0x01, 0x40, 0x00, "Max"				},

	{0   , 0xfe, 0   ,    2, "Mode"				},
	{0x0e, 0x01, 0x80, 0x00, "Stop"				},
	{0x0e, 0x01, 0x80, 0x80, "Play"				},
};

STDDIPINFO(Holeland2)

static struct BurnDIPInfo CrzrallyDIPList[]=
{
	{0x0d, 0xff, 0xff, 0x71, NULL				},
	{0x0e, 0xff, 0xff, 0xc0, NULL				},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x0d, 0x01, 0x01, 0x01, "Off"				},
	{0x0d, 0x01, 0x01, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x0d, 0x01, 0x02, 0x02, "Off"				},
	{0x0d, 0x01, 0x02, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Drive"			},
	{0x0d, 0x01, 0x0c, 0x00, "A"				},
	{0x0d, 0x01, 0x0c, 0x04, "B"				},
	{0x0d, 0x01, 0x0c, 0x08, "C"				},
	{0x0d, 0x01, 0x0c, 0x0c, "D"				},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x0d, 0x01, 0x10, 0x10, "Off"				},
	{0x0d, 0x01, 0x10, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Extra Time"		},
	{0x0d, 0x01, 0x60, 0x00, "None"				},
	{0x0d, 0x01, 0x60, 0x20, "5 Sec"			},
	{0x0d, 0x01, 0x60, 0x40, "10 Sec"			},
	{0x0d, 0x01, 0x60, 0x60, "15 Sec"			},

	{0   , 0xfe, 0   ,    2, "Coin Mode"		},
	{0x0d, 0x01, 0x80, 0x00, "Upright"			},
	{0x0d, 0x01, 0x80, 0x80, "Cocktail"			},

	{0   , 0xfe, 0   ,    4, "Coin A"			},
	{0x0e, 0x01, 0x03, 0x03, "4 Coins 1 Credits"},
	{0x0e, 0x01, 0x03, 0x02, "3 Coins 1 Credits"},
	{0x0e, 0x01, 0x03, 0x01, "2 Coins 1 Credits"},
	{0x0e, 0x01, 0x03, 0x00, "1 Coin  1 Credits"},

	{0   , 0xfe, 0   ,    4, "Coin B"			},
	{0x0e, 0x01, 0x0c, 0x00, "1 Coin  2 Credits"},
	{0x0e, 0x01, 0x0c, 0x04, "1 Coin  3 Credits"},
	{0x0e, 0x01, 0x0c, 0x08, "1 Coin  4 Credits"},
	{0x0e, 0x01, 0x0c, 0x0c, "1 Coin  6 Credits"},

	{0   , 0xfe, 0   ,    2, "Unknown"			},
	{0x0e, 0x01, 0x10, 0x00, "Off"				},
	{0x0e, 0x01, 0x10, 0x10, "On"				},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x0e, 0x01, 0x20, 0x00, "Easy"				},
	{0x0e, 0x01, 0x20, 0x20, "Hard"				},

	{0   , 0xfe, 0   ,    2, "Controller"		},
	{0x0e, 0x01, 0x40, 0x40, "Joystick"			},
	{0x0e, 0x01, 0x40, 0x00, "Wheel"			},

	{0   , 0xfe, 0   ,    2, "Mode"				},
	{0x0e, 0x01, 0x80, 0x00, "Stop"				},
	{0x0e, 0x01, 0x80, 0x80, "Play"				},
};

STDDIPINFO(Crzrally)

static void __fastcall holeland_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xa000:
			sp0256_ald_write(data);
		return;

		case 0xc000:
		case 0xf800: // crzrally
			palette_offset = (palette_offset & 2) | (data & 1);
		return;

		case 0xc001:
		case 0xf801:
			palette_offset = (palette_offset & 1) | ((data & 1) << 1);
		return;

		case 0xc005:
		case 0xf005: // crzrally
			// coin counter
		return;

		case 0xc006:
			flipscreen[0] = data & 1; // x
		return;

		case 0xc007:
			flipscreen[1] = data & 1; // y
		return;

		case 0xf000: // crzrally
			scrollx = data;
		return;
	}
}

static void __fastcall holeland_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x04:
		case 0x05:
		case 0x06:
		case 0x07:
			AY8910Write((port / 2) & 1, port & 1, data);
		return;
	}
}

static UINT8 __fastcall holeland_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x01:
			return BurnWatchdogRead();

		case 0x04:
		case 0x06:
			return AY8910Read((port / 2) & 1);
	}

	return 0;
}

static UINT8 ay8910_0_read_A(UINT32)
{
	return DrvInputs[0];
}

static UINT8 ay8910_0_read_B(UINT32)
{
	return DrvInputs[1];
}

static UINT8 ay8910_1_read_A(UINT32)
{
	return DrvDips[0];
}

static UINT8 ay8910_1_read_B(UINT32)
{
	return DrvDips[1];
}

static void sp0256_drq_callback(UINT8 data)
{
	ZetSetIRQLine(0x20, (data) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static tilemap_callback( holeland )
{
	INT32 attr = DrvColRAM[offs];
	INT32 code = DrvVidRAM[offs] | ((attr & 0x03) << 8);

	TILE_SET_INFO(0, code, (palette_offset << 4) | (attr >> 4), TILE_FLIPYX(attr >> 2));

	*category = attr >> 7; // 0x20 in crzrally, but not used
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
	AY8910Reset(1);
	sp0256_reset();

	BurnWatchdogReset();

	scrollx = 0;
	flipscreen[0] = 0;
	flipscreen[1] = 0;
	palette_offset = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += 0x040000;
	DrvGfxROM1		= Next; Next += 0x020000;

	DrvColPROM		= Next; Next += 0x000300;

	DrvSndROM		= Next; Next += 0x010000;

	DrvPalette		= (UINT32*)Next; Next += 0x100 * sizeof(UINT32);

	DrvNVRAM		= Next; Next += 0x000800;

	AllRam			= Next;

	DrvZ80RAM0		= Next; Next += 0x000800;
	DrvVidRAM		= Next; Next += 0x000400;
	DrvColRAM		= Next; Next += 0x000400;
	DrvSprRAM		= Next; Next += 0x000400;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[2] = { 4, 0 };
	INT32 XOffs0[16] =  { 0,0, 1,1, 2,2, 3,3, 8+0,8+0, 8+1,8+1, 8+2,8+2, 8+3,8+3 };
	INT32 YOffs0[16] = { 0*16,0*16, 1*16,1*16, 2*16,2*16, 3*16,3*16, 4*16,4*16, 5*16,5*16, 6*16,6*16, 7*16,7*16 };

	INT32 Plane1[2] = { 4, 0 };
	INT32 XOffs1[8] = { STEP4(0,1), STEP4(8,1) };
	INT32 YOffs1[8] = { STEP8(0,16) };

	INT32 Plane2[2] = { 4, 0 };
	INT32 XOffs2[32] = { 0, 2, 1, 3, 1*8+0, 1*8+2, 1*8+1, 1*8+3, 2*8+0, 2*8+2, 2*8+1, 2*8+3, 3*8+0, 3*8+2, 3*8+1, 3*8+3,
			4*8+0, 4*8+2, 4*8+1, 4*8+3, 5*8+0, 5*8+2, 5*8+1, 5*8+3, 6*8+0, 6*8+2, 6*8+1, 6*8+3, 7*8+0, 7*8+2, 7*8+1, 7*8+3 };
	INT32 YOffs2[32] = { 0, 4*64, (0x2000*8), (0x2000*8)+4*64, (0x4000*8), (0x4000*8)+4*64, (0x6000*8), (0x6000*8)+4*64,
		1*64, 5*64, (0x2000*8)+1*64, (0x2000*8)+5*64, (0x4000*8)+1*64, (0x4000*8)+5*64, (0x6000*8)+1*64, (0x6000*8)+5*64,
		2*64, 6*64, (0x2000*8)+2*64, (0x2000*8)+6*64, (0x4000*8)+2*64, (0x4000*8)+6*64, (0x6000*8)+2*64, (0x6000*8)+6*64,
		3*64, 7*64, (0x2000*8)+3*64, (0x2000*8)+7*64, (0x4000*8)+3*64, (0x4000*8)+7*64, (0x6000*8)+3*64, (0x6000*8)+7*64 };

	INT32 Plane3[2] = { 0, 1 };
	INT32 XOffs3[16] = { 3*2, 2*2, 1*2, 0*2, 7*2, 6*2, 5*2, 4*2,
			16+3*2, 16+2*2, 16+1*2, 16+0*2, 16+7*2, 16+6*2, 16+5*2, 16+4*2 };
	INT32 YOffs3[16] = { (0x6000*8)+0*16, (0x4000*8)+0*16, (0x2000*8)+0*16, 0+0*16,
			(0x6000*8)+2*16, (0x4000*8)+2*16, (0x2000*8)+2*16, 0+2*16,
			(0x6000*8)+4*16, (0x4000*8)+4*16, (0x2000*8)+4*16, 0+4*16,
			(0x6000*8)+6*16, (0x4000*8)+6*16, (0x2000*8)+6*16, 0+6*16 };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x8000);
	if (tmp == NULL) {
		return 1;
	}

	for (INT32 i = 0; i < 0x4000; i++) tmp[i] = DrvGfxROM0[i] ^ 0xff; // invert

	if (game_select == 0)
	{
		GfxDecode(0x0400, 2, 16, 16, Plane0, XOffs0, YOffs0, 0x080, tmp, DrvGfxROM0);

		memcpy (tmp, DrvGfxROM1, 0x8000);

		GfxDecode(0x0080, 2, 32, 32, Plane2, XOffs2, YOffs2, 0x200, tmp, DrvGfxROM1);
	}
	else
	{

		GfxDecode(0x0400, 2, 8, 8, Plane1, XOffs1, YOffs1, 0x080, tmp, DrvGfxROM0);

		memcpy (tmp, DrvGfxROM1, 0x8000);

		GfxDecode(0x0200, 2, 16, 16, Plane3, XOffs3, YOffs3, 0x080, tmp, DrvGfxROM1);
	}

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit(INT32 game)
{
	game_select = game;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	memset (DrvNVRAM, 0xff, 0x800);

	if (game == 0)
	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x02000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x04000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x06000,  3, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x0a000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x02000,  6, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x02000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x04000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x06000, 10, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000, 11, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00100, 12, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00200, 13, 1)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x01000, 14, 1)) return 1;
	}
	else
	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x04000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x08000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  3, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x02000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x02000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x04000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x06000,  8, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000,  9, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00100, 10, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00200, 11, 1)) return 1;
	}

	DrvGfxDecode();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,			0x0000, 0xbfff, MAP_ROM);
	if (game == 0)
		ZetMapMemory(DrvZ80RAM0,		0x8000, 0x87ff, MAP_RAM);
	if (game == 1)
		ZetMapMemory(DrvNVRAM,			0xc000, 0xc7ff, MAP_RAM); // crzrally
	ZetMapMemory(DrvColRAM,				0xe000, 0xe3ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,				0xe400, 0xe7ff, MAP_RAM);
	if (game == 0)
		ZetMapMemory(DrvSprRAM,			0xf000,	0xf3ff, MAP_RAM);
	else
		ZetMapMemory(DrvSprRAM,			0xe800, 0xebff, MAP_RAM); // crzrally
	ZetSetWriteHandler(holeland_write);
	ZetSetOutHandler(holeland_write_port);
	ZetSetInHandler(holeland_read_port);
	ZetClose();

	AY8910Init(0, game ? 1250000 : 625000, 0);
	AY8910Init(1, 1250000, 1);
	AY8910SetPorts(0, &ay8910_0_read_A, &ay8910_0_read_B, NULL, NULL);
	AY8910SetPorts(1, &ay8910_1_read_A, &ay8910_1_read_B, NULL, NULL);
	AY8910SetAllRoutes(0, 0.25, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.25, BURN_SND_ROUTE_BOTH);

	sp0256_init(DrvSndROM, 3355700);
	sp0256_set_drq_cb(sp0256_drq_callback);

	BurnWatchdogInit(DrvDoReset, 180);

	GenericTilesInit();

	if (game == 0)
	{
		GenericTilemapInit(0, TILEMAP_SCAN_ROWS, holeland_map_callback, 16, 16, 32, 32);
		GenericTilemapSetGfx(0, DrvGfxROM0, 2, 16, 16, 0x40000, 0, 0x3f);
		GenericTilemapSetOffsets(0, 0, -32);
		GenericTilemapCategoryConfig(0, 4);
		GenericTilemapSetTransMask(0, 0, 0xff); // fg TMAP_DRAWLAYER0 category 0
		GenericTilemapSetTransMask(0, 1, 0x01); // "" category 1
		GenericTilemapSetTransMask(0, 2, 0x00); // bg TMAP_DRAWLAYER1 category 0
		GenericTilemapSetTransMask(0, 3, 0xfe); // "" category 1
	}
	else
	{
		GenericTilemapInit(0, TILEMAP_SCAN_COLS, holeland_map_callback, 8, 8, 32, 32);
		GenericTilemapSetGfx(0, DrvGfxROM0, 2, 8, 8, 0x10000, 0, 0x3f);
		GenericTilemapSetOffsets(0, 0, -16);
	}
	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	ZetExit();
	AY8910Exit(0);
	AY8910Exit(1);
	sp0256_exit();

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x100; i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 0x01;
		INT32 bit3 = (DrvColPROM[i] >> 3) & 0x01;
		INT32 r = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[i + 0x100] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + 0x100] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + 0x100] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + 0x100] >> 3) & 0x01;
		INT32 g = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[i + 0x200] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + 0x200] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + 0x200] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + 0x200] >> 3) & 0x01;
		INT32 b = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void holeland_draw_sprites()
{
	for (INT32 offs = 3; offs < 0x400 - 1; offs += 4)
	{
		INT32 sy = 236 - DrvSprRAM[offs];
		INT32 sx = DrvSprRAM[offs + 2];
		INT32 code = DrvSprRAM[offs + 1] & 0x7f;
		INT32 color = (palette_offset << 4) | (DrvSprRAM[offs + 3] >> 4);
		INT32 flipx = DrvSprRAM[offs + 3] & 0x04;
		INT32 flipy = DrvSprRAM[offs + 3] & 0x08;

		if (flipscreen[0])
		{
			flipx = !flipx;
			sx = 240 - sx;
		}

		if (flipscreen[1])
		{
			flipy = !flipy;
			sy = 240 - sy;
		}

		Draw32x32MaskTile(pTransDraw, code, sx*2, (sy*2)-32, flipx, flipy, color, 2, 0, 0, DrvGfxROM1);
	}
}

static INT32 HolelandDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	GenericTilemapSetFlip(0, (flipscreen[0] ? TMAP_FLIPX : 0) | (flipscreen[1] ? TMAP_FLIPY : 0));

	BurnTransferClear();

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, TMAP_DRAWLAYER1); // background layer

	if (nSpriteEnable & 1) holeland_draw_sprites();

	if (nBurnLayer & 2) GenericTilemapDraw(0, pTransDraw, TMAP_DRAWLAYER0); // foreground layer

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void crzrally_draw_sprites()
{
	for (INT32 offs = 3; offs < 0x400 - 1; offs += 4)
	{
		INT32 sy = 236 - DrvSprRAM[offs];
		INT32 sx = DrvSprRAM[offs + 2];
		INT32 code = DrvSprRAM[offs + 1] + ((DrvSprRAM[offs + 3] & 0x01) << 8);
		INT32 color = (DrvSprRAM[offs + 3] >> 4) + ((DrvSprRAM[offs + 3] & 0x01) << 4);
		INT32 flipx = DrvSprRAM[offs + 3] & 0x04;
		INT32 flipy = DrvSprRAM[offs + 3] & 0x08;

		if (flipscreen[0])
		{
			flipx = !flipx;
			sx = 240 - sx;
		}

		if (flipscreen[1])
		{
			flipy = !flipy;
			sy = 240 - sy;
		}

		Draw16x16MaskTile(pTransDraw, code, sx, sy-16, flipx, flipy, color, 2, 0, 0, DrvGfxROM1);
	}
}

static INT32 CrzrallyDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	GenericTilemapSetFlip(0, (flipscreen[0] ? TMAP_FLIPX : 0) | (flipscreen[1] ? TMAP_FLIPY : 0));

	GenericTilemapSetScrollX(0, scrollx);

	BurnTransferClear();

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);

	if (nSpriteEnable & 1) crzrally_draw_sprites();

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
		memset (DrvInputs, 0xff, 2);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nSoundBufferPos = 0;
	INT32 nInterleave = 256;
	INT32 nCyclesTotal = (game_select ? 5000000 : 3355700) / 60;
	INT32 nCyclesDone = 0;

	ZetOpen(0);
	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone += ZetRun((nCyclesTotal * (i + 1) / nInterleave) - nCyclesDone);

		if (i == 240) {
			if (pBurnDraw) {
				BurnDrvRedraw();
			}

			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}

		if (pBurnSoundOut && (i%16) == 15) {
			INT32 nSegmentLength = nBurnSoundLen / (nInterleave / 16);
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			AY8910Render(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}
	ZetClose();

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		if (nSegmentLength) {
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			AY8910Render(pSoundBuf, nSegmentLength);
		}
		ZetOpen(0);
		if (game_select == 0) sp0256_update(pBurnSoundOut, nBurnSoundLen);
		ZetClose();
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
		sp0256_scan(nAction, pnMin);
		BurnWatchdogScan(nAction);

		SCAN_VAR(flipscreen);
		SCAN_VAR(palette_offset);
		SCAN_VAR(scrollx);
	}

	if (nAction & ACB_NVRAM && game_select == 1) {
		ba.Data		= DrvNVRAM;
		ba.nLen		= 0x01000;
		ba.nAddress	= 0;
		ba.szName	= "NV RAM";
		BurnAcb(&ba);
	}

	return 0;
}


// Hole Land (Japan)

static struct BurnRomInfo holelandRomDesc[] = {
	{ "0.2a",			0x2000, 0xb640e12b, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "1.2b",			0x2000, 0x2f180851, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "2.1d",			0x2000, 0x35cfde75, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "3.2d",			0x2000, 0x5537c22e, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "4.1e",			0x2000, 0xc95c355d, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "5.4d",			0x2000, 0x7f19e1f9, 2 | BRF_GRA },           //  5 Background Tiles
	{ "6.4e",			0x2000, 0x844400e3, 2 | BRF_GRA },           //  6

	{ "7.4m",			0x2000, 0xd7feb25b, 3 | BRF_GRA },           //  7 Sprites
	{ "8.4n",			0x2000, 0x4b6eec16, 3 | BRF_GRA },           //  8
	{ "9.4p",			0x2000, 0x6fe7fcc0, 3 | BRF_GRA },           //  9
	{ "10.4r",			0x2000, 0xe1e11e8f, 3 | BRF_GRA },           // 10

	{ "82s129.3m",		0x0100, 0x9d6fef5a, 4 | BRF_GRA },           // 11 Color Data
	{ "82s129.3l",		0x0100, 0xf6682705, 4 | BRF_GRA },           // 12
	{ "82s129.3n",		0x0100, 0x3d7b3af6, 4 | BRF_GRA },           // 13

	{ "sp0256a-al2.1b",	0x0800, 0xb504ac15, 5 | BRF_GRA },           // 14 Speech Data
};

STD_ROM_PICK(holeland)
STD_ROM_FN(holeland)

static INT32 HolelandInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvHoleland = {
	"holeland", NULL, NULL, NULL, "1984",
	"Hole Land (Japan)\0", NULL, "Tecfri", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_ACTION, 0,
	NULL, holelandRomInfo, holelandRomName, NULL, NULL, NULL, NULL, HolelandInputInfo, HolelandDIPInfo,
	HolelandInit, DrvExit, DrvFrame, HolelandDraw, DrvScan, &DrvRecalc, 0x100,
	512, 448, 4, 3
};


// Hole Land (Spain)

static struct BurnRomInfo holeland2RomDesc[] = {
	{ "2.2a",			0x2000, 0xb26212a9, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "3.2b",			0x2000, 0x623bca75, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "0.1d",			0x2000, 0xa3bafdae, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.2d",			0x2000, 0x88a8ba11, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "1.1e",			0x2000, 0xec338f4b, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "5.4d",			0x2000, 0x7f19e1f9, 2 | BRF_GRA },           //  5 Background Tiles
	{ "6.4e",			0x2000, 0x844400e3, 2 | BRF_GRA },           //  6

	{ "7.4m",			0x2000, 0xd7feb25b, 3 | BRF_GRA },           //  7 Sprites
	{ "8.4n",			0x2000, 0x4b6eec16, 3 | BRF_GRA },           //  8
	{ "9.4p",			0x2000, 0x6fe7fcc0, 3 | BRF_GRA },           //  9
	{ "10.4r",			0x2000, 0xe1e11e8f, 3 | BRF_GRA },           // 10

	{ "82s129.3m",		0x0100, 0x9d6fef5a, 4 | BRF_GRA },           // 11 Color Data
	{ "82s129.3l",		0x0100, 0xf6682705, 4 | BRF_GRA },           // 12
	{ "82s129.3n",		0x0100, 0x3d7b3af6, 4 | BRF_GRA },           // 13

	{ "sp0256a_al2.1b",	0x0800, 0xb504ac15, 5 | BRF_GRA },           // 14 Speech Data
};

STD_ROM_PICK(holeland2)
STD_ROM_FN(holeland2)

struct BurnDriver BurnDrvHoleland2 = {
	"holeland2", "holeland", NULL, NULL, "1984",
	"Hole Land (Spain)\0", NULL, "Tecfri", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_ACTION, 0,
	NULL, holeland2RomInfo, holeland2RomName, NULL, NULL, NULL, NULL, HolelandInputInfo, Holeland2DIPInfo,
	HolelandInit, DrvExit, DrvFrame, HolelandDraw, DrvScan, &DrvRecalc, 0x100,
	512, 448, 4, 3
};


// Crazy Rally (set 1)

static struct BurnRomInfo crzrallyRomDesc[] = {
	{ "1.7g",			0x4000, 0x8fe01f86, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "2.7f",			0x4000, 0x67110f1d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.7d",			0x4000, 0x25c861c3, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "4.5h",			0x2000, 0x29dece8b, 2 | BRF_GRA },           //  3 Background Tiles
	{ "5.5g",			0x2000, 0xb34aa904, 2 | BRF_GRA },           //  4

	{ "6.1f",			0x2000, 0xa909ff0f, 3 | BRF_GRA },           //  5 Sprites
	{ "7.1l",			0x2000, 0x38fb0a16, 3 | BRF_GRA },           //  6
	{ "8.1k",			0x2000, 0x660aa0f0, 3 | BRF_GRA },           //  7
	{ "9.1i",			0x2000, 0x37d0790e, 3 | BRF_GRA },           //  8

	{ "82s129.9n",		0x0100, 0x98ff725a, 4 | BRF_GRA },           //  9 Color Data
	{ "82s129.9m",		0x0100, 0xd41f5800, 4 | BRF_GRA },           // 10
	{ "82s129.9l",		0x0100, 0x9ed49cb4, 4 | BRF_GRA },           // 11

	{ "82s147.1f",		0x0200, 0x5261bc11, 5 | BRF_OPT },           // 12 Unknown

	{ "pal16r6a.5k",	0x0104, 0x3d12afba, 6 | BRF_OPT },           // 13 plds
	{ "pal16r4a.5l",	0x0104, 0x00000000, 6 | BRF_NODUMP | BRF_OPT },           // 14
	{ "pal16r4a.5m",	0x0104, 0x00000000, 6 | BRF_NODUMP | BRF_OPT },           // 15
	{ "pal16r8a.1d",	0x0104, 0x00000000, 6 | BRF_NODUMP | BRF_OPT },           // 16
};

STD_ROM_PICK(crzrally)
STD_ROM_FN(crzrally)

static INT32 CrzrallyInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvCrzrally = {
	"crzrally", NULL, NULL, NULL, "1985",
	"Crazy Rally (set 1)\0", "Graphics issues on some levels", "Tecfri", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, crzrallyRomInfo, crzrallyRomName, NULL, NULL, NULL, NULL, CrzrallyInputInfo, CrzrallyDIPInfo,
	CrzrallyInit, DrvExit, DrvFrame, CrzrallyDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Crazy Rally (set 2)

static struct BurnRomInfo crzrallyaRomDesc[] = {
	{ "crzralla_1.7g",	0x4000, 0x8c6a70aa, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "crzralla_2.7f",	0x4000, 0x7fdd4a45, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "crzralla_3.7d",	0x4000, 0xa25edd17, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "4.5h",			0x2000, 0x29dece8b, 2 | BRF_GRA },           //  3 Background Tiles
	{ "crzralla_5.5g",	0x2000, 0x81e9b043, 2 | BRF_GRA },           //  4

	{ "6.1f",			0x2000, 0xa909ff0f, 3 | BRF_GRA },           //  5 Sprites
	{ "7.1l",			0x2000, 0x38fb0a16, 3 | BRF_GRA },           //  6
	{ "8.1k",			0x2000, 0x660aa0f0, 3 | BRF_GRA },           //  7
	{ "9.1i",			0x2000, 0x37d0790e, 3 | BRF_GRA },           //  8

	{ "82s129.9n",		0x0100, 0x98ff725a, 4 | BRF_GRA },           //  9 Color Data
	{ "82s129.9m",		0x0100, 0xd41f5800, 4 | BRF_GRA },           // 10
	{ "82s129.9l",		0x0100, 0x9ed49cb4, 4 | BRF_GRA },           // 11

	{ "82s147.1f",		0x0200, 0x5261bc11, 5 | BRF_OPT },           // 12 Unknown

	{ "pal16r6a.5k",	0x0104, 0x3d12afba, 6 | BRF_OPT },           // 13 plds
	{ "pal16r4a.5l",	0x0104, 0x00000000, 6 | BRF_NODUMP | BRF_OPT },           // 14
	{ "pal16r4a.5m",	0x0104, 0x00000000, 6 | BRF_NODUMP | BRF_OPT },           // 15
	{ "pal16r8a.1d",	0x0104, 0x00000000, 6 | BRF_NODUMP | BRF_OPT },           // 16
};

STD_ROM_PICK(crzrallya)
STD_ROM_FN(crzrallya)

struct BurnDriver BurnDrvCrzrallya = {
	"crzrallya", "crzrally", NULL, NULL, "1985",
	"Crazy Rally (set 2)\0", "Graphics issues on some levels", "Tecfri", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, crzrallyaRomInfo, crzrallyaRomName, NULL, NULL, NULL, NULL, CrzrallyInputInfo, CrzrallyDIPInfo,
	CrzrallyInit, DrvExit, DrvFrame, CrzrallyDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Crazy Rally (Gecas license)

static struct BurnRomInfo crzrallygRomDesc[] = {
	{ "12.7g",			0x4000, 0x0cab3ef9, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "13.7f",			0x4000, 0xe19a8e13, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "14.7d",			0x4000, 0x4c0351ba, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "4.5h",			0x2000, 0x29dece8b, 2 | BRF_GRA },           //  3 Background Tiles
	{ "16.5g",			0x2000, 0x94289f9e, 2 | BRF_GRA },           //  4

	{ "17.1n",			0x2000, 0x985ed5c8, 3 | BRF_GRA },           //  5 Sprites
	{ "18.1l",			0x2000, 0xc02ddda2, 3 | BRF_GRA },           //  6
	{ "19.1k",			0x2000, 0x2a0d5bca, 3 | BRF_GRA },           //  7
	{ "20.1i",			0x2000, 0x49c0c2b8, 3 | BRF_GRA },           //  8

	{ "82s129.9n",		0x0100, 0x98ff725a, 4 | BRF_GRA },           //  9 Color Data
	{ "82s129.9m",		0x0100, 0xd41f5800, 4 | BRF_GRA },           // 10
	{ "82s129.9l",		0x0100, 0x9ed49cb4, 4 | BRF_GRA },           // 11

	{ "82s147.1f",		0x0200, 0x5261bc11, 5 | BRF_OPT },           // 12 Unknown

	{ "pal16r6a.5k",	0x0104, 0x3d12afba, 6 | BRF_OPT },           // 13 plds
	{ "pal16r4a.5l",	0x0104, 0x00000000, 6 | BRF_NODUMP | BRF_OPT },           // 14
	{ "pal16r4a.5m",	0x0104, 0x00000000, 6 | BRF_NODUMP | BRF_OPT },           // 15
	{ "pal16r8a.1d",	0x0104, 0x00000000, 6 | BRF_NODUMP | BRF_OPT },           // 16
};

STD_ROM_PICK(crzrallyg)
STD_ROM_FN(crzrallyg)

struct BurnDriver BurnDrvCrzrallyg = {
	"crzrallyg", "crzrally", NULL, NULL, "1985",
	"Crazy Rally (Gecas license)\0", "Graphics issues on some levels", "Tecfri (Gecas license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, crzrallygRomInfo, crzrallygRomName, NULL, NULL, NULL, NULL, CrzrallyInputInfo, CrzrallyDIPInfo,
	CrzrallyInit, DrvExit, DrvFrame, CrzrallyDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};
