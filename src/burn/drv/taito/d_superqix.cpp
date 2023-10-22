// FB Neo Super Qix driver module
// Based on MAME driver by Mirko Buffoni, Nicola Salmoria, Tomasz Slanina

/*
	to do:
		Add sqixb1 (not planning on adding it)
*/

#include "tiles_generic.h"
#include "z80_intf.h"
#include "ay8910.h"
#include "taito_m68705.h"
#include "mcs51.h"
#include "bitswap.h"
#include "burn_gun.h" // for dial (pbillian, hotsmash)

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvMCUROM;
static UINT8 *DrvSndROM;
static UINT8 *DrvGfxROM[2];
static UINT8 *DrvZ80RAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvBitmapRAM[2];

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 gfxbank;
static INT32 bitmap_select;
static INT32 bankdata;
static INT32 nmi_mask;
static INT32 flipscreen;
static INT32 mcu_port2;

static INT32 vblank;
static INT32 mcu_type = 0; // none

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvInputs[5];
static UINT8 DrvDips[2];
static UINT8 DrvReset;

static INT16 Analog[4];

static INT32 is_sqix = 0;
static INT32 is_perestroika = 0;
static INT32 is_hotsmash = 0;
static INT32 has_dial = 0;

static INT32 scanline;

static INT32 sample_offset;

static INT32 nCyclesExtra[3];

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}

static struct BurnInputInfo PbillianInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy4 + 5,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy4 + 3,	"p1 start"	},
	A("P1 Dial", 		BIT_ANALOG_REL, &Analog[0],		"p1 x-axis"),
	A("P1 Plunger",		BIT_ANALOG_REL, &Analog[1],		"p1 z-axis"),
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy4 + 4,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy4 + 2,	"p2 start"	},
	A("P2 Dial", 		BIT_ANALOG_REL, &Analog[2],		"p2 x-axis"),
	A("P2 Plunger",		BIT_ANALOG_REL, &Analog[3],		"p2 z-axis"),
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy4 + 1,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Pbillian)

static struct BurnInputInfo PbillianbInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy4 + 5,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy4 + 3,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy4 + 4,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy4 + 2,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy4 + 1,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Pbillianb)

static struct BurnInputInfo HotsmashInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy4 + 5,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy4 + 3,	"p1 start"	},
	A("P1 Dial", 		BIT_ANALOG_REL, &Analog[0],		"p1 x-axis"),

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy4 + 4,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy4 + 2,	"p2 start"	},
	A("P2 Dial", 		BIT_ANALOG_REL, &Analog[2],		"p2 x-axis"),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy4 + 1,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Hotsmash)

static struct BurnInputInfo SuperqixInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy4 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy4 + 3,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy4 + 4,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy4 + 6,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy1 + 7,	"diag"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy4 + 5,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Superqix)

static struct BurnDIPInfo PbillianOffsetDIPList[]=
{
	DIP_OFFSET(0x0c)
};

static struct BurnDIPInfo PbillianbOffsetDIPList[]=
{
	DIP_OFFSET(0x12)
};

static struct BurnDIPInfo PbillianCommonDIPList[]=
{
	{0x00, 0xff, 0xff, 0xbf, NULL						},
	{0x01, 0xff, 0xff, 0x9f, NULL						},

	{0   , 0xfe, 0   ,    8, "Coin A"					},
	{0x00, 0x01, 0x07, 0x03, "5 Coins 1 Credits"		},
	{0x00, 0x01, 0x07, 0x04, "4 Coins 1 Credits"		},
	{0x00, 0x01, 0x07, 0x05, "3 Coins 1 Credits"		},
	{0x00, 0x01, 0x07, 0x06, "2 Coins 1 Credits"		},
	{0x00, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x00, 0x01, 0x07, 0x00, "2 Coins 3 Credits"		},
	{0x00, 0x01, 0x07, 0x02, "1 Coin  2 Credits"		},
	{0x00, 0x01, 0x07, 0x01, "1 Coin  3 Credits"		},

	{0   , 0xfe, 0   ,    8, "Coin B"					},
	{0x00, 0x01, 0x38, 0x18, "5 Coins 1 Credits"		},
	{0x00, 0x01, 0x38, 0x20, "4 Coins 1 Credits"		},
	{0x00, 0x01, 0x38, 0x28, "3 Coins 1 Credits"		},
	{0x00, 0x01, 0x38, 0x30, "2 Coins 1 Credits"		},
	{0x00, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x00, 0x01, 0x38, 0x00, "2 Coins 3 Credits"		},
	{0x00, 0x01, 0x38, 0x10, "1 Coin  2 Credits"		},
	{0x00, 0x01, 0x38, 0x08, "1 Coin  3 Credits"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"			},
	{0x00, 0x01, 0x40, 0x40, "No"						},
	{0x00, 0x01, 0x40, 0x00, "Yes"						},

	{0   , 0xfe, 0   ,    2, "Freeze"					},
	{0x00, 0x01, 0x80, 0x80, "No"						},
	{0x00, 0x01, 0x80, 0x00, "Yes"						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x01, 0x01, 0x03, 0x03, "2"						},
	{0x01, 0x01, 0x03, 0x02, "3"						},
	{0x01, 0x01, 0x03, 0x01, "4"						},
	{0x01, 0x01, 0x03, 0x00, "5"						},

	{0   , 0xfe, 0   ,    4, "Bonus Life"				},
	{0x01, 0x01, 0x0c, 0x0c, "10/20/300K Points"		},
	{0x01, 0x01, 0x0c, 0x00, "10/30/500K Points"		},
	{0x01, 0x01, 0x0c, 0x08, "20/30/400K Points"		},
	{0x01, 0x01, 0x0c, 0x04, "30/40/500K Points"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x01, 0x01, 0x30, 0x00, "Easy"						},
	{0x01, 0x01, 0x30, 0x10, "Normal"					},
	{0x01, 0x01, 0x30, 0x20, "Hard"						},
	{0x01, 0x01, 0x30, 0x30, "Very Hard"				},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x01, 0x01, 0x40, 0x00, "Upright"					},
	{0x01, 0x01, 0x40, 0x40, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"				},
	{0x01, 0x01, 0x80, 0x80, "Off"						},
	{0x01, 0x01, 0x80, 0x00, "On"						},
};

STDDIPINFOEXT(Pbillian, PbillianCommon, PbillianOffset)
STDDIPINFOEXT(Pbillianb, PbillianCommon, PbillianbOffset)

static struct BurnDIPInfo SuperqixDIPList[]=
{
	DIP_OFFSET(0x15)
	{0x00, 0xff, 0xff, 0xf6, NULL						},
	{0x01, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x00, 0x01, 0x01, 0x00, "Upright"					},
	{0x00, 0x01, 0x01, 0x01, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"				},
	{0x00, 0x01, 0x02, 0x02, "Off"						},
	{0x00, 0x01, 0x02, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Freeze"					},
	{0x00, 0x01, 0x04, 0x04, "Off"						},
	{0x00, 0x01, 0x04, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Allow Continue"			},
	{0x00, 0x01, 0x08, 0x08, "No"						},
	{0x00, 0x01, 0x08, 0x00, "Yes"						},

	{0   , 0xfe, 0   ,    4, "Coin A"					},
	{0x00, 0x01, 0x30, 0x10, "2 Coins 1 Credits"		},
	{0x00, 0x01, 0x30, 0x30, "1 Coin  1 Credits"		},
	{0x00, 0x01, 0x30, 0x00, "2 Coins 3 Credits"		},
	{0x00, 0x01, 0x30, 0x20, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"					},
	{0x00, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"		},
	{0x00, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"		},
	{0x00, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"		},
	{0x00, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x01, 0x01, 0x03, 0x02, "Easy"						},
	{0x01, 0x01, 0x03, 0x03, "Normal"					},
	{0x01, 0x01, 0x03, 0x01, "Hard"						},
	{0x01, 0x01, 0x03, 0x00, "Hardest"					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"				},
	{0x01, 0x01, 0x0c, 0x08, "20000 50000"				},
	{0x01, 0x01, 0x0c, 0x0c, "30000 100000"				},
	{0x01, 0x01, 0x0c, 0x04, "50000 100000"				},
	{0x01, 0x01, 0x0c, 0x00, "None"						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x01, 0x01, 0x30, 0x20, "2"						},
	{0x01, 0x01, 0x30, 0x30, "3"						},
	{0x01, 0x01, 0x30, 0x10, "4"						},
	{0x01, 0x01, 0x30, 0x00, "5"						},

	{0   , 0xfe, 0   ,    4, "Fill Area"				},
	{0x01, 0x01, 0xc0, 0x80, "70%"						},
	{0x01, 0x01, 0xc0, 0xc0, "75%"						},
	{0x01, 0x01, 0xc0, 0x40, "80%"						},
	{0x01, 0x01, 0xc0, 0x00, "85%"						},
};

STDDIPINFO(Superqix)

static struct BurnDIPInfo HotsmashDIPList[]=
{
	DIP_OFFSET(0x08)
	{0x00, 0xff, 0xff, 0xff, NULL						},
	{0x01, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    2, "Flip Screen"				},
	{0x00, 0x01, 0x02, 0x02, "Off"						},
	{0x00, 0x01, 0x02, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x00, 0x01, 0x08, 0x00, "Off"						},
	{0x00, 0x01, 0x08, 0x08, "On"						},
														
	{0   , 0xfe, 0   ,    4, "Coin A"					},
	{0x00, 0x01, 0x30, 0x10, "2 Coins 1 Credits"		},
	{0x00, 0x01, 0x30, 0x30, "1 Coin  1 Credits"		},
	{0x00, 0x01, 0x30, 0x00, "2 Coins 3 Credits"		},
	{0x00, 0x01, 0x30, 0x20, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"					},
	{0x00, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"		},
	{0x00, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"		},
	{0x00, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"		},
	{0x00, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    4, "Difficulty vs. CPU"		},
	{0x01, 0x01, 0x03, 0x02, "Easy"						},
	{0x01, 0x01, 0x03, 0x03, "Normal"					},
	{0x01, 0x01, 0x03, 0x01, "Hard"						},
	{0x01, 0x01, 0x03, 0x00, "Hardest"					},

	{0   , 0xfe, 0   ,    4, "Difficulty vs. 2P"		},
	{0x01, 0x01, 0x0c, 0x08, "Easy"						},
	{0x01, 0x01, 0x0c, 0x0c, "Normal"					},
	{0x01, 0x01, 0x0c, 0x04, "Hard"						},
	{0x01, 0x01, 0x0c, 0x00, "Hardest"					},

	{0   , 0xfe, 0   ,    2, "Points per game"			},
	{0x01, 0x01, 0x10, 0x00, "3"						},
	{0x01, 0x01, 0x10, 0x10, "4"						},
};

STDDIPINFO(Hotsmash)

static void play_sample(INT32 sample)
{
//	bprintf (0, _T("Play sample: %d\n"), sample);
	sample_offset = (sample * 0x80) << 16;
}

static void sample_render(INT16 *buffer, INT32 nLen)
{
	if (sample_offset == -1 || (sample_offset >> 16) >= 0x8000 ) {
		sample_offset = -1; // stopped
		return;
	}

	INT32 step = (3906 << 16) / nBurnSoundRate;
	INT32 pos = 0;
	UINT8 *rom = DrvSndROM;

	while (pos < nLen)
	{
		INT16 sample = (INT8)(rom[(sample_offset >> 16)] ^ 0x80) * 256;

		buffer[0] = BURN_SND_CLIP((INT32)(buffer[0] + sample * 0.50));
		buffer[1] = BURN_SND_CLIP((INT32)(buffer[1] + sample * 0.50));

		sample_offset += step;

		buffer += 2;
		pos++;

		if (rom[(sample_offset >> 16)] == 0xff || (sample_offset >> 16) >= 0x8000) {
			sample_offset = -1; // stop
			break;
		}
	}
}

static void bankswitch(INT32 data)
{
	bankdata = data;
	ZetMapMemory(DrvZ80ROM + 0x08000 + (bankdata * 0x4000), 0x8000, 0xbfff, MAP_ROM);
}

static void __fastcall sqix_out_port(UINT16 port, UINT8 data)
{
	if ((port & 0xff00) == 0x0000) {
		DrvPalRAM[port] = data; // palette
		return;
	}

	if (port >= 0x0800 && port <= 0xf7ff) {
		INT32 offset = port - 0x0800;
		DrvBitmapRAM[offset / 0x8000][offset & 0x7fff] = data;
		return;
	}

	switch (port)
	{
		case 0x0402:
		case 0x0403:
		case 0x0406:
		case 0x0407:
			AY8910Write((port / 4) & 1, ~port & 1, data);
		return;

		case 0x0410:
			gfxbank = data & 0x03;
			bitmap_select = (data & 0x04) >> 2;
			nmi_mask = data & 0x08;
			bankswitch((data & 0x30) >> 4);
		return;
	}
}

static UINT8 __fastcall sqix_in_port(UINT16 port)
{
	if ((port & 0xff00) == 0x0000) {
		return DrvPalRAM[port];
	}

	if (port >= 0x0800 && port <= 0xf7ff) {
		INT32 offset = port - 0x0800;
		return DrvBitmapRAM[offset / 0x8000][offset & 0x7fff];
	}

	switch (port)
	{
		case 0x0401:
		case 0x0405:
			return AY8910Read((port/4) & 1);

		case 0x0408:
			if (mcu_type == 2) main_sent = 1;
			return 0;

		case 0x0418: {
			INT32 ret = (mcu_type == 0) ? (DrvInputs[3] & 0x7f) : 0xff;
			ZetSetIRQLine(0x20, CPU_IRQSTATUS_NONE);
			return ret;
		}
	}

	return 0;
}

static void __fastcall pbillian_out_port(UINT16 port, UINT8 data)
{
	if ((port & 0xfe00) == 0x0000) {
		DrvPalRAM[port] = data; // palette
		return;
	}

	if ((port & 0xfe00) == 0x0200) {
		return; // junk
	}

	switch (port)
	{
		case 0x0402:
		case 0x0403:
			AY8910Write(0, ~port & 1, data);
		return;

		case 0x0408:
			if (mcu_type == 1) {
				standard_taito_mcu_write(data);
			}
		return;

		case 0x0410:
			// coin counters = data & 6
			nmi_mask = data & 0x10;
			if (nmi_mask == 0) ZetSetIRQLine(0x20, CPU_IRQSTATUS_NONE);
			bankswitch((data & 0x08) >> 3);
			flipscreen = data & 0x20;
		return;

		case 0x0419:
		return; // nop

		case 0x041a:
			play_sample(data);
		return;
	}
}

static UINT8 __fastcall pbillian_in_port(UINT16 port)
{
	if ((port & 0xfe00) == 0x0000) {
		return DrvPalRAM[port];
	}

	if ((port & 0xfe00) == 0x0200) {
		return 0; // junk
	}

	switch (port)
	{
		case 0x0401:
			return AY8910Read(0);

		case 0x0408:
			return (mcu_type == 1) ? standard_taito_mcu_read() : 0;

		case 0x0418: {
			INT32 ret = 0xff;
			if (mcu_type == 0) ret = (DrvInputs[3] & 0x7f) | (!vblank ? 0x80 : 0);
			ZetSetIRQLine(0x20, CPU_IRQSTATUS_NONE);
			return ret;
		}

		case 0x041b:
			return 0; // ??

	// bootleg features
		case 0x0c00:
			return DrvDips[0];

		case 0x0c01:
			return DrvDips[1];

		case 0x0c06:
			return DrvInputs[0]; // controls

		case 0x0c07:
			return DrvInputs[1]; // controls2
	}

	return 0;
}

struct plungee {
	INT32 accu;
	INT32 speed;
	INT32 quad;
	INT32 launch;

	INT32 latch;
	INT32 hi;
};

static plungee pl[2];

static void plunger_frametimer() // once per frame!
{
	// theory:
	// when trigger is squeezed, it usually looks something like this:
	// frame#	1, plunger: 0x00
	//          2, plunger: 0x1b
	//          3, plunger: 0x3a
	//          4, plunger: 0x4d  <-- we want to latch this!
	//          5, plunger: 0x2c
	//          6, plunger: 0x1b
	//          7, plunger: 0x00
	// rules:
	// 1: we want to latch the highest value before it falls
	// 2: after we latch it, must wait until 0x00 to re-latch

	for (int player = 0; player < 2; player++) {
		plungee *p = &pl[player];

		int plunger = ProcessAnalog(Analog[1 + player], 0, INPUT_LINEAR | INPUT_MIGHTBEDIGITAL, 0x00, 0xff);

		if (p->hi == -1) {
			// execute: rule #2
			if (plunger == 0) {
				// OK to process input on next frame.
				p->hi = 0;
			}
		} else {
			if (plunger > p->hi) {
				p->hi = plunger;
			}

			if (plunger < p->hi && p->latch == 0 && p->quad == 0) {
				p->latch = p->hi;
				p->hi = -1; // next frame, do rule #2
			}
		}

		if (p->launch > 0) p->launch--; // launch downcounter (see below func.)
	}
}

static UINT8 read_plunger(INT32 player)
{
	// this func. called 355-357x per frame, per player
	const UINT8 QUADRADIC[4] = { 3, 2, 0, 1 }; // reversed, because: downcounter
	const INT32 DIVISOR = 32;
	const INT32 player_xor = is_hotsmash && player == 1;
	plungee *p = &pl[player];

	if (p->latch && p->quad == 0) {
		p->quad = p->latch;
		p->latch = 0; // clear latch!
		p->speed = (0x100 / DIVISOR) - (p->quad / DIVISOR);
		p->launch = 0;
		if (player == 0) bprintf(0, _T("plunger start, quad %x  speed %x\n"), p->quad, p->speed);
	}

	p->accu--;
	if (p->accu < 0) {
		p->accu = p->speed;
		if (p->quad > 0) {
			p->quad--;
			if (p->quad == 0) {
				// at the end of quad accu, arm the launch frame-counter
				// when it gets to 3 it'll hold the /launch signal for 3 frames
				p->launch = 16;
			}
		}
	}

	// prebillian, vs. hotsmash
	// input read via mcu portC (&7 .. [6,7]&1)
	// ...l dsqq
	//    q.uadradic plunger
	//    s.pinner
	//    d.irection spinner
	//   /l.aunch

	return (!(p->launch > 0 && p->launch < 4)) << 4 |
		((BurnTrackballGetDirection(player) < 0) << (3 ^ player_xor)) |
		((BurnTrackballReadInterpolated(player, scanline) & 1) << (2 ^ player_xor)) |
		QUADRADIC[p->quad & 3];
}

static void hotsmash_68705_portC_out(UINT8 *data)
{
	UINT8 changed_portC_out = portC_out ^ *data;
	portC_out = *data;

	if ((changed_portC_out & 8) && !(portC_out & 8))
	{
		switch (portC_out & 0x07)
		{
			case 0x00:   // dsw A
			case 0x01:   // dsw B
				portA_in = DrvDips[portC_out & 0x01];
			break;

			case 0x03:
				m68705SetIrqLine(0, 0 /*CLEAR_LINE*/);
				portA_in = from_main;
				main_sent = 0;
			break;

			case 0x05:
				from_mcu = portB_out;
				mcu_sent = 1;
				portA_in = 0xff;
			break;

			case 0x06:
			case 0x07:
				portA_in = read_plunger(portC_out & 1);
			break;
			default:
				portA_in = 0xff;
			break;
		}
	}
}

static UINT8 mcu_read_port(INT32 port)
{
	switch (port)
	{
		case MCS51_PORT_P0:
			return (DrvInputs[3] & 0x7f) | (main_sent ? 0x80 : 0);

		case MCS51_PORT_P1:
			return DrvDips[0];

		case MCS51_PORT_P2:
			return 0;

		case MCS51_PORT_P3:
			return from_main;
	}

	return 0;
}

static void mcu_write_port(INT32 port, UINT8 data)
{
	switch (port)
	{
		case MCS51_PORT_P0:
		case MCS51_PORT_P1:
		break;

		case MCS51_PORT_P2:
		{
			INT32 changed = mcu_port2 ^ data;
			mcu_port2 = data;

			if ((changed & 1) && (~data & 1)) {
				flipscreen = data & 0x10;
				ZetSetRESETLine(0, (data & 0x20) ? CPU_IRQSTATUS_NONE : CPU_IRQSTATUS_ACK);
				mcu_sent = data & 0x40;
			}
			if ((changed & 0x80) && (~data & 0x80)) {
				main_sent = 0;
			}
		}
		break;

		case MCS51_PORT_P3:
			from_mcu = data;
		break;
	}
}

static UINT8 sq_ay1_read_A(UINT32)
{
	return (DrvInputs[0] & 0xbf) | (vblank ? 0x40 : 0); // iq_132 - wrong!
}

static UINT8 sq_ay1_read_B(UINT32)
{
	UINT8 ret = DrvInputs[1] & 0x3f; // iq_132 - wrong
	if (mcu_type == 2)
	{
		if (mcu_sent) ret |= 0x40;
		if (main_sent) ret |= 0x80;
	}
	return ret;
}

static UINT8 sq_ay2_read_A(UINT32)
{
	return DrvDips[0];
}

static UINT8 sq_ay2_read_B(UINT32)
{
	if (mcu_type == 0)
		return BITSWAP08(DrvDips[0], 0,1,2,3,4,5,6,7);

	return from_mcu; // i8751
}

static void sq_ay2_write_B(UINT32, UINT32 data)
{
	from_main = data;
}

static UINT8 ay_read_A(UINT32)
{
	standard_m68705_portC_in();

	return (DrvInputs[2] & 0x7f) ^ ((mcu_type == 0) ? 0 : (portC_in << 6));
}

static UINT8 ay_read_B(UINT32)
{
	return (DrvInputs[3] & 0x7f) | (vblank ? 0 : 0x80); // system (pbillian)
}

static tilemap_callback( sqbg )
{
	INT32 attr = DrvVidRAM[offs + 0x400];
	INT32 bank = ~attr & 0x04;
	INT32 code = DrvVidRAM[offs] + ((attr & 0x03) << 8);

	if (bank) {
		code += (gfxbank * 0x400); // apply gfxbank
		code += (is_perestroika) ? 0x800 : 0x400; // get to next gfx rom
	}

	TILE_SET_INFO(0, code, attr >> 4, 0);
	sTile->category = (attr & 0x08) >> 3;
}

static tilemap_callback( pbbg )
{
	INT32 code = DrvVidRAM[offs] + (DrvVidRAM[offs + 0x400] << 8);

	TILE_SET_INFO(0, code, code >> 12, 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	switch (mcu_type)
	{
		case 1: m67805_taito_reset(); break; // pbillian / hotsmash
		case 2: {
			mcs51_reset();
			mcu_port2 = 1;
			mcu_write_port(MCS51_PORT_P2, 0);
		}
		break; // sqix
	}

	AY8910Reset(0);

	bankdata = 0;
	nmi_mask = 0;
	gfxbank = 0;
	bitmap_select = 0;

	sample_offset = -1;

	nCyclesExtra[0] = nCyclesExtra[1] = nCyclesExtra[2] = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x020000;

	DrvMCUROM		= Next; Next += 0x001000;

	DrvSndROM		= Next; Next += 0x008000;

	DrvGfxROM[0]	= Next; Next += 0x080000;
	DrvGfxROM[1]	= Next; Next += 0x020000;

	DrvPalette		= (UINT32*)Next; Next += 0x0200 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x002000;
	DrvVidRAM		= Next; Next += 0x000800;
	DrvSprRAM		= Next; Next += 0x000100;
	DrvPalRAM		= Next; Next += 0x000200;

	DrvBitmapRAM[0]	= Next; Next += 0x008000;
	DrvBitmapRAM[1]	= Next; Next += 0x008000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 SqixCommonInit(INT32 (*pLoadRoms)())
{
	BurnAllocMemIndex();

	{
		if (pLoadRoms) {
			if (pLoadRoms()) return 1;
		}

		BurnNibbleExpand(DrvGfxROM[0], NULL, 0x30000, 0, 0);
		BurnNibbleExpand(DrvGfxROM[1], NULL, 0x10000, 0, 0);
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,			0xe000, 0xffff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0xe000, 0xe0ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,			0xe800, 0xefff, MAP_RAM);
	ZetSetOutHandler(sqix_out_port);
	ZetSetInHandler(sqix_in_port);
	ZetClose();

	mcs51_init();
	mcs51_set_program_data(DrvMCUROM);
	mcs51_set_write_handler(mcu_write_port);
	mcs51_set_read_handler(mcu_read_port);

	AY8910Init(0, 1500000, 0);
	AY8910Init(1, 1500000, 0);
	AY8910SetAllRoutes(0, 0.30, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.30, BURN_SND_ROUTE_BOTH);
	AY8910SetPorts(0, &sq_ay1_read_A, &sq_ay1_read_B, NULL, NULL);
	AY8910SetPorts(1, &sq_ay2_read_A, &sq_ay2_read_B, NULL, &sq_ay2_write_B);
	AY8910SetBuffered(ZetTotalCycles, 6000000);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, sqbg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 4, 8, 8, 0x60000, 0x000, 0xf);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 4, 8, 8, 0x20000, 0x000, 0xf);
	GenericTilemapSetOffsets(0, 0, -16);
	GenericTilemapSetTransSplit(0, 0, 0xffff, 0);
	GenericTilemapSetTransSplit(0, 1, 1, 0xfffe);

	DrvDoReset();

	return 0;
}

static m68705_interface pbillian_mcu_interface = {
	NULL /* portA */, NULL /* portB */, hotsmash_68705_portC_out /* portC */,
	NULL /* ddrA  */, NULL /* ddrB  */, NULL /* ddrC  */,
	NULL /* portA */, NULL /* portB */, standard_m68705_portC_in  /* portC */
};

static INT32 PbillianCommonInit(INT32 (*pLoadRoms)())
{
	BurnAllocMemIndex();

	{
		if (pLoadRoms) {
			if (pLoadRoms()) return 1;
		}

		BurnNibbleExpand(DrvGfxROM[0], NULL, 0x18000, 0, 0);
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,			0xe000, 0xffff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0xe000, 0xe0ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,			0xe800, 0xefff, MAP_RAM);
	ZetSetOutHandler(pbillian_out_port);
	ZetSetInHandler(pbillian_in_port);
	ZetClose();

	if (mcu_type == 1) {
		m67805_taito_init(DrvMCUROM, DrvZ80RAM /*not used*/, &pbillian_mcu_interface);
	}

	AY8910Init(0, 1500000, 0);
	AY8910SetAllRoutes(0, 0.30, BURN_SND_ROUTE_BOTH);
	AY8910SetPorts(0, &ay_read_A, &ay_read_B, NULL, NULL);
	AY8910SetBuffered(ZetTotalCycles, 6000000);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, pbbg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 4, 8, 8, 0x20000, 0x100, 0xf);
	GenericTilemapSetGfx(1, DrvGfxROM[0], 4, 8, 8, 0x40000, 0x000, 0xf);
	GenericTilemapSetOffsets(0, 0, -16);

	BurnTrackballInit(1); // for dial
	has_dial = 1;

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	ZetExit();
	AY8910Exit(0);

	if (has_dial) {
		BurnTrackballExit();
		has_dial = 0;
	}

	switch (mcu_type)
	{
		case 1:	m67805_taito_exit(); break; // pbillian / hotsmash
		case 2: mcs51_exit(); break; // sqix
	}

	BurnFreeMemIndex();

	mcu_type = 0;
	is_sqix = 0;
	is_perestroika = 0;
	is_hotsmash = 0;

	return 0;
}

static void DrvPaletteUpdate()
{
	for (INT32 i = 0; i < BurnDrvGetPaletteEntries(); i++) {
		INT32 value = DrvPalRAM[i];

		INT32 r = (((value >> 0) & 0x0c) | (value & 0x03)) * 0x11;
		INT32 g = (((value >> 2) & 0x0c) | (value & 0x03)) * 0x11;
		INT32 b = (((value >> 4) & 0x0c) | (value & 0x03)) * 0x11;

		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}
}

static void copy_bitmap_layer()
{
	UINT8 *bmp = DrvBitmapRAM[bitmap_select] + ((flipscreen) ? (128 * 223) : 0);
	UINT16 *dst = pTransDraw;

	INT32 flipx = (flipscreen) ? 0xfe : 0;

	for (INT32 y = 0; y < 224; y++)
	{
		for (INT32 x = 0; x < 128; x++)
		{
			if (bmp[x] & 0xf0) dst[(x * 2 + 0) ^ flipx] = bmp[x] >> 4;
			if (bmp[x] & 0x0f) dst[(x * 2 + 1) ^ flipx] = bmp[x] & 0x0f;
		}

		bmp += (flipscreen) ? -128 : 128;
		dst += nScreenWidth;
	}
}

static void pbillian_draw_sprites()
{
	for (INT32 offs = 0; offs < 0x100; offs += 4)
	{
		INT32 attr = DrvSprRAM[offs + 3];
		INT32 code = (DrvSprRAM[offs] & 0xfc) + ((attr & 0x0f) << 8);
		INT32 sx = DrvSprRAM[offs + 1] + ((DrvSprRAM[offs] & 0x01) << 8);
		INT32 sy = DrvSprRAM[offs + 2];
		INT32 color = (attr & 0xf0) >> 4;

		if (flipscreen)
		{
			sx = 240 - sx;
			sy = 240 - sy;
		}

		DrawGfxMaskTile(0, 1, code | 0, sx + 0, sy + 0 - 16, 0, 0, color, 0);
		DrawGfxMaskTile(0, 1, code | 1, sx + 8, sy + 0 - 16, 0, 0, color, 0);
		DrawGfxMaskTile(0, 1, code | 2, sx + 0, sy + 8 - 16, 0, 0, color, 0);
		DrawGfxMaskTile(0, 1, code | 3, sx + 8, sy + 8 - 16, 0, 0, color, 0);
	}
}

static void superqix_draw_sprites()
{
	for (INT32 offs = 0; offs < 0x100; offs += 4)
	{
		INT32 attr = DrvSprRAM[offs + 3];
		INT32 code = (DrvSprRAM[offs] + ((attr & 0x01) << 8)) << 2;
		INT32 color = (attr & 0xf0) >> 4;
		INT32 flipx = attr & 0x04;
		INT32 flipy = attr & 0x08;
		INT32 sx = DrvSprRAM[offs + 1];
		INT32 sy = DrvSprRAM[offs + 2];

		if (flipscreen)
		{
			sx = 240 - sx;
			sy = 240 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		DrawGfxMaskTile(0, 1, code | 0, sx + 0, sy + 0 - 16, 0, 0, color, 0);
		DrawGfxMaskTile(0, 1, code | 1, sx + 8, sy + 0 - 16, 0, 0, color, 0);
		DrawGfxMaskTile(0, 1, code | 2, sx + 0, sy + 8 - 16, 0, 0, color, 0);
		DrawGfxMaskTile(0, 1, code | 3, sx + 8, sy + 8 - 16, 0, 0, color, 0);
	}
}

static INT32 SqixDraw()
{
//	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
//	}

	if (~nBurnLayer & 1) BurnTransferClear();

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, TMAP_DRAWLAYER1); //TMAP_FORCEOPAQUE);

	if (nBurnLayer & 2) copy_bitmap_layer();

	if (nSpriteEnable & 1) superqix_draw_sprites();

	if (nBurnLayer & 4) GenericTilemapDraw(0, pTransDraw, TMAP_DRAWLAYER0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 PbillianDraw()
{
//	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
//	}

	if (~nBurnLayer & 1) BurnTransferClear();

	if ( nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);

	if (nSpriteEnable & 1) pbillian_draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}

		if (has_dial) {
			BurnTrackballConfig(0, AXIS_NORMAL, AXIS_NORMAL);
			BurnTrackballFrame(0, Analog[0]/2, Analog[2]/2, 1, 0x0f, 256);
			BurnTrackballUpdate(0);

			plunger_frametimer();
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[3] = { 6000000 / 60, 6000000 / 60, 6000000 / 12 / 60  };
	INT32 nCyclesDone[3] = { nCyclesExtra[0], nCyclesExtra[1], nCyclesExtra[2] };

	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		scanline = i;
		vblank = (i >= 240) ? 0 : 1;

		if (nmi_mask) {
			// 4x sqix(&clones), 1x everything else
			if ( (is_sqix == 0 && i == 239) || (is_sqix == 1 && (i == 48 || i == 112 || i == 176 || i == 240)) ) {
				ZetSetIRQLine(0x20, CPU_IRQSTATUS_ACK);
			}
		}

		CPU_RUN(0, Zet);

		switch (mcu_type)
		{
			case 1:
				m6805Open(0);
				CPU_RUN(1, m6805);
				m6805Close();
			break;

			case 2:
				CPU_RUN(2, mcs51);
			break;
		}

		if (i == 239) {
			if (pBurnDraw) {
				BurnDrvRedraw();
			}
		}

	}

	ZetClose();

	nCyclesExtra[0] = nCyclesDone[0] - nCyclesTotal[0];
	nCyclesExtra[1] = nCyclesDone[1] - nCyclesTotal[1];
	nCyclesExtra[2] = nCyclesDone[2] - nCyclesTotal[2];

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
		sample_render(pBurnSoundOut, nBurnSoundLen);
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

	if (nAction & ACB_DRIVER_DATA)
	{
		ZetScan(nAction);
		switch (mcu_type)
		{
			case 1: m68705_taito_scan(nAction); break;
			case 2: {
				mcs51_scan(nAction);
				SCAN_VAR(mcu_sent);
				SCAN_VAR(main_sent);
				SCAN_VAR(from_mcu);
				SCAN_VAR(from_main);
				SCAN_VAR(mcu_port2);
			}
			break;
		}

		AY8910Scan(nAction, pnMin);

		SCAN_VAR(gfxbank);
		SCAN_VAR(bitmap_select);
		SCAN_VAR(bankdata);
		SCAN_VAR(nmi_mask);
		SCAN_VAR(flipscreen);
		SCAN_VAR(pl);
		SCAN_VAR(sample_offset);

		SCAN_VAR(nCyclesExtra);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		bankswitch(bankdata);
		ZetClose();
	}

	return 0;
}


// Prebillian

static struct BurnRomInfo pbillianRomDesc[] = {
	{ "mitsubishi__electric__1.m5l27256k.6bc",			0x8000, 0xd379fe23, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "mitsubishi__electric__2.m5l27128k.6d",			0x4000, 0x1af522bc, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "mitsubishi__electric__3.m5l27256k.7h",			0x8000, 0x3f9bc7f1, 2 | BRF_SND },           //  2 Samples

	{ "mitsubishi__electric__4.m5l27256k.1n",			0x8000, 0x9c08a072, 3 | BRF_GRA },           //  3 Tiles
	{ "mitsubishi__electric__5.m5l27256k.1r",			0x8000, 0x2dd5b83f, 3 | BRF_GRA },           //  4
	{ "mitsubishi__electric__6.m5l27256k.1t",			0x8000, 0x33b855b0, 3 | BRF_GRA },           //  5

	{ "mitsubishi__electric__7.mc68705p5s.7k",			0x0800, 0x03de0c74, 9 | BRF_PRG | BRF_ESS }, //  6 M68705 MCU
};

STD_ROM_PICK(pbillian)
STD_ROM_FN(pbillian)

static INT32 PbillianLoadRoms()
{
	if (BurnLoadRom(DrvZ80ROM    + 0x00000, 0, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM    + 0x08000, 1, 1)) return 1;
	memcpy (DrvZ80ROM + 0xc000, DrvZ80ROM + 0x8000, 0x4000);

	if (BurnLoadRom(DrvSndROM    + 0x00000, 2, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM[0] + 0x00000, 3, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM[0] + 0x08000, 4, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM[0] + 0x10000, 5, 1)) return 1;
	memcpy (DrvGfxROM[0] + 0x18000, DrvGfxROM[0] + 0x10000, 0x08000);

	if (BurnLoadRom(DrvMCUROM + 0x00000, 6, 1));

	return 0;
}

static INT32 PbillianInit()
{
	mcu_type = 1;

	return PbillianCommonInit(PbillianLoadRoms);
}

struct BurnDriver BurnDrvPbillian = {
	"pbillian", NULL, NULL, NULL, "1986",
	"Prebillian\0", NULL, "Kaneko / Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_ACTION, 0,
	NULL, pbillianRomInfo, pbillianRomName, NULL, NULL, NULL, NULL, PbillianInputInfo, PbillianDIPInfo,
	PbillianInit, DrvExit, DrvFrame, PbillianDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// Prebillian (bootleg)

static struct BurnRomInfo pbillianbRomDesc[] = {
	{ "1",												0x8000, 0xcd8e34f0, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "2",												0x8000, 0xe60a2cb0, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "3",												0x8000, 0x3f9bc7f1, 2 | BRF_SND },           //  2 Samples

	{ "4",												0x8000, 0x9c08a072, 3 | BRF_GRA },           //  3 Tiles
	{ "5",												0x8000, 0x63f3437b, 3 | BRF_GRA },           //  4
	{ "6",												0x8000, 0x33b855b0, 3 | BRF_GRA },           //  5
};

STD_ROM_PICK(pbillianb)
STD_ROM_FN(pbillianb)

static INT32 PbillianbLoadRoms()
{
	if (BurnLoadRom(DrvZ80ROM    + 0x00000, 0, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM    + 0x08000, 1, 1)) return 1;

	if (BurnLoadRom(DrvSndROM    + 0x00000, 2, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM[0] + 0x00000, 3, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM[0] + 0x08000, 4, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM[0] + 0x10000, 5, 1)) return 1;
	memcpy (DrvGfxROM[0] + 0x18000, DrvGfxROM[0] + 0x10000, 0x08000);

	return 0;
}

static INT32 PbillianbInit()
{
	mcu_type = 0;

	return PbillianCommonInit(PbillianbLoadRoms);
}

struct BurnDriver BurnDrvPbillianb = {
	"pbillianb", "pbillian", NULL, NULL, "1987",
	"Prebillian (bootleg)\0", NULL, "bootleg (Game Corp.)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_ACTION, 0,
	NULL, pbillianbRomInfo, pbillianbRomName, NULL, NULL, NULL, NULL, PbillianbInputInfo, PbillianbDIPInfo,
	PbillianbInit, DrvExit, DrvFrame, PbillianDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// Vs. Hot Smash

static struct BurnRomInfo hotsmashRomDesc[] = {
	{ "b18-04",											0x8000, 0x981bde2c, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code

	{ "b18-05",											0x8000, 0xdab5e718, 2 | BRF_SND },           //  1 Samples

	{ "b18-01",											0x8000, 0x870a4c04, 3 | BRF_GRA },           //  2 Tiles
	{ "b18-02",											0x8000, 0x4e625cac, 3 | BRF_GRA },           //  3
	{ "b18-03",											0x4000, 0x1c82717d, 3 | BRF_GRA },           //  4

	{ "b18-06.mcu",										0x0800, 0x67c0920a, 9 | BRF_PRG | BRF_ESS }, //  5 M68705 MCU
};

STD_ROM_PICK(hotsmash)
STD_ROM_FN(hotsmash)

static INT32 HotsmashLoadRoms()
{
	if (BurnLoadRom(DrvZ80ROM    + 0x00000, 0, 1)) return 1;

	if (BurnLoadRom(DrvSndROM    + 0x00000, 1, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM[0] + 0x00000, 2, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM[0] + 0x08000, 3, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM[0] + 0x10000, 4, 1)) return 1;
	// mirroring fixes sprites
	memcpy (DrvGfxROM[0] + 0x14000, DrvGfxROM[0] + 0x10000, 0x4000);
	memcpy (DrvGfxROM[0] + 0x18000, DrvGfxROM[0] + 0x10000, 0x08000);

	if (BurnLoadRom(DrvMCUROM + 0x00000, 5, 1));

	return 0;
}

static INT32 HotsmashInit()
{
	is_hotsmash = 1;
	mcu_type = 1;

	return PbillianCommonInit(HotsmashLoadRoms);
}

struct BurnDriver BurnDrvHotsmash = {
	"hotsmash", NULL, NULL, NULL, "1987",
	"Vs. Hot Smash\0", NULL, "Kaneko / Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_BALLPADDLE, 0,
	NULL, hotsmashRomInfo, hotsmashRomName, NULL, NULL, NULL, NULL, HotsmashInputInfo, HotsmashDIPInfo,
	HotsmashInit, DrvExit, DrvFrame, PbillianDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Super Qix (World/Japan, V1.2)

static struct BurnRomInfo sqixRomDesc[] = {
	{ "b03__01-2.ef3",									0x08000, 0x5ded636b, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "b03__02.h3",										0x10000, 0x9c23cb64, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "b03__04.s8",										0x08000, 0xf815ef45, 3 | BRF_GRA },           //  2 Tiles
	{ "taito_sq-iu3__lh231041__sharp_japan__8709_d.p8",	0x20000, 0xb8d0c493, 3 | BRF_GRA },   		  //  3

	{ "b03__05.t8",										0x10000, 0xdf326540, 4 | BRF_GRA },           //  4 Sprites

	{ "b03__03.l2",										0x01000, 0xf0c3af2b, 9 | BRF_PRG | BRF_ESS }, //  5 i8751 MCU
};

STD_ROM_PICK(sqix)
STD_ROM_FN(sqix)

static INT32 SqixRomLoad()
{
	if (BurnLoadRom(DrvZ80ROM    + 0x00000, 0, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM    + 0x08000, 1, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM[0] + 0x00000, 2, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM[0] + 0x08000, 3, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM[1] + 0x00000, 4, 1)) return 1;

	if (BurnLoadRom(DrvMCUROM    + 0x00000, 5, 1)) return 1;

	return 0;
}

static INT32 SqixInit()
{
	is_sqix = 1;
	mcu_type = 2;

	return SqixCommonInit(SqixRomLoad);
}

struct BurnDriver BurnDrvSqix = {
	"sqix", NULL, NULL, NULL, "1987",
	"Super Qix (World/Japan, V1.2)\0", NULL, "Kaneko / Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_ACTION | GBF_PUZZLE, 0,
	NULL, sqixRomInfo, sqixRomName, NULL, NULL, NULL, NULL, SuperqixInputInfo, SuperqixDIPInfo,
	SqixInit, DrvExit, DrvFrame, SqixDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Super Qix (World/Japan, V1.1)

static struct BurnRomInfo sqixr1RomDesc[] = {
	{ "b03__01-1.ef3",									0x08000, 0xad614117, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "b03__02.h3",										0x10000, 0x9c23cb64, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "b03__04.s8",										0x08000, 0xf815ef45, 3 | BRF_GRA },           //  2 Tiles
	{ "taito_sq-iu3__lh231041__sharp_japan__8709_d.p8",	0x20000, 0xb8d0c493, 3 | BRF_GRA },           //  3

	{ "b03__05.t8",										0x10000, 0xdf326540, 4 | BRF_GRA },           //  4 Sprites

	{ "b03__03.l2",										0x01000, 0xf0c3af2b, 9 | BRF_PRG | BRF_ESS }, //  5 i8751 MCU
};

STD_ROM_PICK(sqixr1)
STD_ROM_FN(sqixr1)

struct BurnDriver BurnDrvSqixr1 = {
	"sqixr1", "sqix", NULL, NULL, "1987",
	"Super Qix (World/Japan, V1.1)\0", NULL, "Kaneko / Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_ACTION | GBF_PUZZLE, 0,
	NULL, sqixr1RomInfo, sqixr1RomName, NULL, NULL, NULL, NULL, SuperqixInputInfo, SuperqixDIPInfo,
	SqixInit, DrvExit, DrvFrame, SqixDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Super Qix (World/Japan, V1.0)

static struct BurnRomInfo sqixr0RomDesc[] = {
	{ "b03__01.ef3",									0x08000, 0x0888b7de, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "b03__02.h3",										0x10000, 0x9c23cb64, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "b03__04.s8",										0x08000, 0xf815ef45, 3 | BRF_GRA },           //  2 Tiles
	{ "taito_sq-iu3__lh231041__sharp_japan__8709_d.p8",	0x20000, 0xb8d0c493, 3 | BRF_GRA },           //  3

	{ "b03__05.t8",										0x10000, 0xdf326540, 4 | BRF_GRA },           //  4 Sprites

	{ "b03__03.l2",										0x01000, 0xf0c3af2b, 9 | BRF_PRG | BRF_ESS }, //  5 i8751 MCU
};

STD_ROM_PICK(sqixr0)
STD_ROM_FN(sqixr0)

struct BurnDriver BurnDrvSqixr0 = {
	"sqixr0", "sqix", NULL, NULL, "1987",
	"Super Qix (World/Japan, V1.0)\0", NULL, "Kaneko / Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_ACTION | GBF_PUZZLE, 0,
	NULL, sqixr0RomInfo, sqixr0RomName, NULL, NULL, NULL, NULL, SuperqixInputInfo, SuperqixDIPInfo,
	SqixInit, DrvExit, DrvFrame, SqixDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Super Qix (US)

static struct BurnRomInfo sqixuRomDesc[] = {
	{ "b03__06.ef3",									0x08000, 0x4f59f7af, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "b03__07.h3",										0x10000, 0x4c417d4a, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "b03__04.s8",										0x08000, 0xf815ef45, 3 | BRF_GRA },           //  2 Tiles
	{ "taito_sq-iu3__lh231041__sharp_japan__8709_d.p8",	0x20000, 0xb8d0c493, 3 | BRF_GRA },           //  3

	{ "b03__09.t8",										0x10000, 0x69d2a84a, 4 | BRF_GRA },           //  4 Sprites

	{ "b03__08.l2",										0x01000, 0x7c338c0f, 9 | BRF_PRG | BRF_ESS }, //  5 i8751 MCU
};

STD_ROM_PICK(sqixu)
STD_ROM_FN(sqixu)

struct BurnDriver BurnDrvSqixu = {
	"sqixu", "sqix", NULL, NULL, "1987",
	"Super Qix (US)\0", NULL, "Kaneko / Taito (Romstar License)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_ACTION | GBF_PUZZLE, 0,
	NULL, sqixuRomInfo, sqixuRomName, NULL, NULL, NULL, NULL, SuperqixInputInfo, SuperqixDIPInfo,
	SqixInit, DrvExit, DrvFrame, SqixDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Super Qix (bootleg of V1.0, 8031 MCU)

static struct BurnRomInfo sqixb1RomDesc[] = {
	{ "sq01.97",										0x08000, 0x0888b7de, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "b03__02.h3",										0x10000, 0x9c23cb64, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "b03__04.s8",										0x08000, 0xf815ef45, 3 | BRF_GRA },           //  2 Tiles
	{ "b03-03",											0x10000, 0x6e8b6a67, 3 | BRF_GRA },           //  3
	{ "b03-06",											0x10000, 0x38154517, 3 | BRF_GRA },           //  4

	{ "b03__05.t8",										0x10000, 0xdf326540, 4 | BRF_GRA },           //  5 Sprites

	{ "sq07.ic108",										0x01000, 0xd11411fb, 9 | BRF_PRG | BRF_ESS }, //  6 i8031 MCU
};

STD_ROM_PICK(sqixb1)
STD_ROM_FN(sqixb1)

static INT32 Sqixb1RomLoad()
{
	if (BurnLoadRom(DrvZ80ROM + 0x00000, 0, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM + 0x08000, 1, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM[0] + 0x00000, 2, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM[0] + 0x08000, 3, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM[0] + 0x18000, 4, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM[1] + 0x00000, 5, 1)) return 1;

	BurnLoadRom(DrvMCUROM    + 0x00000, 6, 1);

	return 0;
}

static INT32 Sqixb1Init()
{
	is_sqix = 1;
	mcu_type = 3;
	return SqixCommonInit(Sqixb1RomLoad);
}

struct BurnDriverD BurnDrvSqixb1 = {
	"sqixb1", "sqix", NULL, NULL, "1987",
	"Super Qix (bootleg of V1.0, 8031 MCU)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_ACTION | GBF_PUZZLE, 0,
	NULL, sqixb1RomInfo, sqixb1RomName, NULL, NULL, NULL, NULL, SuperqixInputInfo, SuperqixDIPInfo,
	Sqixb1Init, DrvExit, DrvFrame, SqixDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Super Qix (bootleg, No MCU)

static struct BurnRomInfo sqixb2RomDesc[] = {
	{ "cpu.2",											0x08000, 0x682e28e3, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "b03__02.h3",										0x10000, 0x9c23cb64, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "b03__04.s8",										0x08000, 0xf815ef45, 3 | BRF_GRA },           //  2 Tiles
	{ "b03-03",											0x10000, 0x6e8b6a67, 3 | BRF_GRA },           //  3
	{ "b03-06",											0x10000, 0x38154517, 3 | BRF_GRA },           //  4

	{ "b03__05.t8",										0x10000, 0xdf326540, 4 | BRF_GRA },           //  5 Sprites
};

STD_ROM_PICK(sqixb2)
STD_ROM_FN(sqixb2)

static INT32 Sqixb2RomLoad()
{
	if (BurnLoadRom(DrvZ80ROM + 0x00000, 0, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM + 0x08000, 1, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM[0] + 0x00000, 2, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM[0] + 0x08000, 3, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM[0] + 0x18000, 4, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM[1] + 0x00000, 5, 1)) return 1;

	BurnLoadRom(DrvMCUROM    + 0x00000, 6, 1);

	return 0;
}

static INT32 Sqixb2Init()
{
	is_sqix = 1;
	mcu_type = 0;
	return SqixCommonInit(Sqixb2RomLoad);
}

struct BurnDriver BurnDrvSqixb2 = {
	"sqixb2", "sqix", NULL, NULL, "1987",
	"Super Qix (bootleg, No MCU)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_ACTION | GBF_PUZZLE, 0,
	NULL, sqixb2RomInfo, sqixb2RomName, NULL, NULL, NULL, NULL, SuperqixInputInfo, SuperqixDIPInfo,
	Sqixb2Init, DrvExit, DrvFrame, SqixDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Perestroika Girls

static struct BurnRomInfo perestroRomDesc[] = {
	{ "rom1.bin",										0x20000, 0x0cbf96c1, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code

	{ "rom4.bin",										0x10000, 0xc56122a8, 3 | BRF_GRA },           //  1 Tiles
	{ "rom2.bin",										0x20000, 0x36f93701, 3 | BRF_GRA },           //  2

	{ "rom3a.bin",										0x10000, 0x7a2a563f, 4 | BRF_GRA },           //  3 Sprites
};

STD_ROM_PICK(perestro)
STD_ROM_FN(perestro)

static void decode_program(UINT8 *src, INT32 len)
{
	static const int convtable[16] = {
		0xc, 0x9, 0xb, 0xa,	0x8, 0xd, 0xf, 0xe,	0x4, 0x1, 0x3, 0x2,	0x0, 0x5, 0x7, 0x6
	};

	UINT8 tmp[16];

	for (INT32 i = 0; i < len; i+=16)
	{
		memcpy (tmp, src + i, 16);

		for (INT32 j = 0; j < 16; j++)
		{
			src[i+j] = tmp[convtable[j]];
		}
	}
}

static void decode_single(UINT8 *src, INT32 len, const int *bs)
{
	UINT8 tmp[16];

	for (INT32 i = 0; i < len; i+=16)
	{
		memcpy (tmp, src + i, 16);

		for (INT32 j = 0; j < 16; j++)
		{
			src[i+j] = tmp[BITSWAP08(j,7,6,5,4,bs[0],bs[1],bs[2],bs[3])];
		}
	}
}

static INT32 PerestroLoadRoms()
{
	const int bitswaps[3][4] = { { 3,2,0,1}, { 0,1,2,3 }, { 1,0,3,2 } };

	if (BurnLoadRom(DrvZ80ROM + 0x00000, 0, 1)) return 1;
	memmove (DrvZ80ROM + 0x08000, DrvZ80ROM + 0x10000, 0x10000);

	decode_program(DrvZ80ROM, 0x20000);

	if (BurnLoadRom(DrvGfxROM[0] + 0x00000, 1, 1)) return 1;

	decode_single(DrvGfxROM[0], 0x10000, bitswaps[0]);

	if (BurnLoadRom(DrvGfxROM[0] + 0x10000, 2, 1)) return 1;

	decode_single(DrvGfxROM[0] + 0x10000, 0x20000, bitswaps[1]);

	if (BurnLoadRom(DrvGfxROM[1] + 0x00000, 3, 1)) return 1;

	decode_single(DrvGfxROM[1], 0x10000, bitswaps[2]);

	return 0;
}

static INT32 PerestroInit()
{
	is_sqix = 1;        // both!
	is_perestroika = 1; // both!
	mcu_type = 0;
	return SqixCommonInit(PerestroLoadRoms);
}

struct BurnDriver BurnDrvPerestro = {
	"perestro", NULL, NULL, NULL, "1994",
	"Perestroika Girls\0", NULL, "Promat", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_ACTION | GBF_PUZZLE, 0,
	NULL, perestroRomInfo, perestroRomName, NULL, NULL, NULL, NULL, SuperqixInputInfo, SuperqixDIPInfo,
	PerestroInit, DrvExit, DrvFrame, SqixDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Perestroika Girls (Fuuki license)

static struct BurnRomInfo perestrofRomDesc[] = {
	{ "rom1.bin",										0x20000, 0x0cbf96c1, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code

	{ "rom4.bin",										0x10000, 0xc56122a8, 3 | BRF_GRA },           //  1 Tiles
	{ "rom2.bin",										0x20000, 0x36f93701, 3 | BRF_GRA },           //  2

	{ "rom3.bin",										0x10000, 0x00c91d5a, 4 | BRF_GRA },           //  3 Sprites
};

STD_ROM_PICK(perestrof)
STD_ROM_FN(perestrof)

struct BurnDriver BurnDrvPerestrof = {
	"perestrof", "perestro", NULL, NULL, "1993",
	"Perestroika Girls (Fuuki license)\0", NULL, "Promat (Fuuki license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_ACTION | GBF_PUZZLE, 0,
	NULL, perestrofRomInfo, perestrofRomName, NULL, NULL, NULL, NULL, SuperqixInputInfo, SuperqixDIPInfo,
	PerestroInit, DrvExit, DrvFrame, SqixDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};
