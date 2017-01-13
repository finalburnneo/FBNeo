// FB Alpha Galivan driver module
// Based on MAME driver by Luca Elia and Olivier Galibert

#include "tiles_generic.h"
#include "z80_intf.h"
#include "dac.h"
#include "burn_ym3526.h"
#include "flt_rc.h"
#include "nb1414m4_8bit.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvMapROM;
static UINT8 *DrvColPROM;
static UINT8 *DrvSprPal;
static UINT8 *DrvColTable;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprBuf;

static UINT32 *DrvPalette;
static UINT8  DrvRecalc;

static INT16 *hpfiltbuffer;

static UINT8 sprite_priority;
static UINT16 scrollx;
static UINT16 scrolly;
static UINT8 flipscreen;
static UINT8 soundlatch;
static UINT8 bankdata;
static UINT8 display_disable;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDip[2]; // for ninjemak service mode
static UINT8 DrvInputs[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
static UINT8 DrvReset;

static INT32 game_mode;

static struct BurnInputInfo DrvInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 4,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvInputs + 3,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvInputs + 4,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvJoy3 + 5,	"dip"		},
};

STDINPUTINFO(Drv)

static struct BurnInputInfo ninjemakInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 4,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvInputs + 3,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvInputs + 4,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDip + 0,	"dip"		},
};

STDINPUTINFO(ninjemak)

static struct BurnDIPInfo GalivanDIPList[]=
{
	{0x14, 0xff, 0xff, 0xdf, NULL			},
	{0x15, 0xff, 0xff, 0xff, NULL			},
	{0x16, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0x03, 0x03, "3"			},
	{0x14, 0x01, 0x03, 0x02, "4"			},
	{0x14, 0x01, 0x03, 0x01, "5"			},
	{0x14, 0x01, 0x03, 0x00, "6"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x14, 0x01, 0x0c, 0x0c, "20k and every 60k"	},
	{0x14, 0x01, 0x0c, 0x08, "50k and every 60k"	},
	{0x14, 0x01, 0x0c, 0x04, "20k and every 90k"	},
	{0x14, 0x01, 0x0c, 0x00, "50k and every 90k"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x14, 0x01, 0x10, 0x00, "Off"			},
	{0x14, 0x01, 0x10, 0x10, "On"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x14, 0x01, 0x20, 0x00, "Upright"		},
	{0x14, 0x01, 0x20, 0x20, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Power Invulnerability"},
	{0x14, 0x01, 0x40, 0x40, "Off"			},
	{0x14, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Life Invulnerability"	},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x15, 0x01, 0x03, 0x01, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x15, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},
	{0x15, 0x01, 0x03, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x15, 0x01, 0x0c, 0x00, "3 Coins 1 Credits"	},
	{0x15, 0x01, 0x0c, 0x04, "2 Coins 3 Credits"	},
	{0x15, 0x01, 0x0c, 0x0c, "1 Coin  3 Credits"	},
	{0x15, 0x01, 0x0c, 0x08, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x15, 0x01, 0x10, 0x10, "Easy"			},
	{0x15, 0x01, 0x10, 0x00, "Hard"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x15, 0x01, 0x20, 0x20, "Off"			},
	{0x15, 0x01, 0x20, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"	},
	{0x16, 0x01, 0x01, 0x00, "Off"			},
	{0x16, 0x01, 0x01, 0x01, "On (reset after turning off)"			},
};

STDDIPINFO(Galivan)

static struct BurnDIPInfo DangarDIPList[]=
{
	{0x14, 0xff, 0xff, 0x9f, NULL			},
	{0x15, 0xff, 0xff, 0x7f, NULL			},
	{0x16, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0x03, 0x03, "3"			},
	{0x14, 0x01, 0x03, 0x02, "4"			},
	{0x14, 0x01, 0x03, 0x01, "5"			},
	{0x14, 0x01, 0x03, 0x00, "6"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x14, 0x01, 0x0c, 0x0c, "20k and every 60k"	},
	{0x14, 0x01, 0x0c, 0x08, "50k and every 60k"	},
	{0x14, 0x01, 0x0c, 0x04, "20k and every 90k"	},
	{0x14, 0x01, 0x0c, 0x00, "50k and every 90k"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x14, 0x01, 0x10, 0x00, "Off"			},
	{0x14, 0x01, 0x10, 0x10, "On"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x14, 0x01, 0x20, 0x00, "Upright"		},
	{0x14, 0x01, 0x20, 0x20, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Alternate Enemies"	},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x15, 0x01, 0x03, 0x01, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x15, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},
	{0x15, 0x01, 0x03, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x15, 0x01, 0x0c, 0x04, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"	},
	{0x15, 0x01, 0x0c, 0x00, "2 Coins 3 Credits"	},
	{0x15, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x15, 0x01, 0x10, 0x10, "Easy"			},
	{0x15, 0x01, 0x10, 0x00, "Hard"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x15, 0x01, 0x20, 0x20, "Off"			},
	{0x15, 0x01, 0x20, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Allow Continue"	},
	{0x15, 0x01, 0xc0, 0xc0, "No"			},
	{0x15, 0x01, 0xc0, 0x80, "3 Times"		},
	{0x15, 0x01, 0xc0, 0x40, "5 Times"		},
	{0x15, 0x01, 0xc0, 0x00, "99 Times"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"	},
	{0x16, 0x01, 0x01, 0x00, "Off"			},
	{0x16, 0x01, 0x01, 0x01, "On"			},
};

STDDIPINFO(Dangar)

static struct BurnDIPInfo NinjemakDIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL			},
	{0x15, 0xff, 0xff, 0x7c, NULL			},
	{0x16, 0xff, 0xff, 0x02, NULL			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0x03, 0x03, "3"			},
	{0x14, 0x01, 0x03, 0x02, "4"			},
	{0x14, 0x01, 0x03, 0x01, "5"			},
	{0x14, 0x01, 0x03, 0x00, "6"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x14, 0x01, 0x0c, 0x0c, "20k and every 60k"	},
	{0x14, 0x01, 0x0c, 0x08, "50k and every 60k"	},
	{0x14, 0x01, 0x0c, 0x04, "20k and every 90k"	},
	{0x14, 0x01, 0x0c, 0x00, "50k and every 90k"	},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x14, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0x30, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x14, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"	},
	{0x14, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x15, 0x01, 0x01, 0x01, "Off"			},
	{0x15, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x15, 0x01, 0x02, 0x00, "Upright"		},
	{0x15, 0x01, 0x02, 0x02, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x15, 0x01, 0x04, 0x04, "Easy"			},
	{0x15, 0x01, 0x04, 0x00, "Hard"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x15, 0x01, 0x20, 0x20, "Off"			},
	{0x15, 0x01, 0x20, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Allow Continue"	},
	{0x15, 0x01, 0xc0, 0xc0, "No"			},
	{0x15, 0x01, 0xc0, 0x80, "3 Times"		},
	{0x15, 0x01, 0xc0, 0x40, "5 Times"		},
	{0x15, 0x01, 0xc0, 0x00, "99 Times"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"	},
	{0x16, 0x01, 0x02, 0x02, "Off"			},
	{0x16, 0x01, 0x02, 0x00, "On"			},
};

STDDIPINFO(Ninjemak)

static void bankswitch(INT32 data)
{
	bankdata = data;

	ZetMapMemory(DrvZ80ROM0 + 0x10000 + data * 0x2000, 0xc000, 0xdfff, MAP_ROM);
}

static void __fastcall galivan_main_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x40:
		//	coin counter data & 0x03
			flipscreen = data & 0x04;
			bankswitch((data & 0x80) >> 7);
		return;

		case 0x41:
			scrollx = (scrollx & 0xff00) | data;
		return;

		case 0x42:
		{
			if ((data & 0x80) == 0 && scrollx & 0x8000)
			{
				scrollx &= 0x7fff;
				sprite_priority = data & 0x20;
				display_disable = data & 0x40;
			}

			scrollx = (scrollx & 0x00ff) | (data << 8);
		}
		return;

		case 0x43:
			scrolly = (scrolly & 0xff00) | data;
		return;

		case 0x44:
			scrolly = (scrolly & 0x00ff) | (data << 8);
		return;

		case 0x45:
		case 0x85:
			soundlatch = (data << 1) | 1;
		return;

		case 0x80:
			// coin counter = data & 0x03
			flipscreen = data & 0x04;
			display_disable = data & 0x10;
			bankswitch((data & 0xc0) >> 6);
		return;

		case 0x86: {
			nb_1414m4_exec8b((DrvVidRAM[0] << 8) | (DrvVidRAM[1] & 0xff), DrvVidRAM,&scrollx,&scrolly,game_mode);
		}
		return;

		case 0x46:
		case 0x47:
		case 0x87:
		return; // nop
	}
}

static UINT8 __fastcall galivan_main_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
		case 0x04:
		case 0x05:
			return DrvInputs[port & 7];

		case 0xc0: // dangar
			return 0x58;

		// ninja emaki (ninjemak)
		case 0x80:
		case 0x81:
		case 0x82:
			return DrvInputs[port & 7];

		case 0x84:
		case 0x85:
			return DrvInputs[(port & 7)-1];

		case 0x83:
			return (DrvDip[0] & 0x02);

	}

	return 0;
}

static void __fastcall galivan_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
			BurnYM3526Write(port & 1, data);
		return;

		case 0x02:
		case 0x03:
			DACWrite(port & 1, data);
		return;
	}
}

static UINT8 __fastcall galivan_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x04:
			soundlatch = 0;
			return 0;

		case 0x06:
			return soundlatch;

	}

	return 0;
}

static INT32 DrvSyncDAC()
{
	return (INT32)(float)(nBurnSoundLen * (ZetTotalCycles() / (4000000.000 / (nBurnFPS / 100.000))));
}

inline static INT32 DrvYM3526SynchroniseStream(INT32 nSoundRate)
{
	return (INT64)ZetTotalCycles() * nSoundRate / 4000000;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	bankswitch(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	ZetClose();

	BurnYM3526Reset();

	sprite_priority = 0;
	scrollx = 0;
	scrolly = 0;
	flipscreen = 0;
	soundlatch = 0;
	display_disable = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x018000;
	DrvZ80ROM1		= Next; Next += 0x00c000;

	DrvGfxROM0		= Next; Next += 0x010000;
	DrvGfxROM1		= Next; Next += 0x040000;
	DrvGfxROM2		= Next; Next += 0x040000;

	DrvMapROM		= Next; Next += 0x008000;

	DrvColPROM		= Next; Next += 0x000400;
	DrvSprPal		= Next; Next += 0x000100;

	nb1414_blit_data8b	= Next; Next += 0x004000;

	DrvColTable		= Next; Next += 0x001180;

	DrvPalette		= (UINT32*)Next; Next += 0x1180 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM1		= Next; Next += 0x000800;
	DrvVidRAM		= Next; Next += 0x000800;
	DrvSprRAM		= Next; Next += 0x002000;
	DrvSprBuf		= Next; Next += 0x000200;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void DrvPaletteTable()
{
	for (INT32 i = 0; i < 0x80; i++) {
		DrvColTable[i] = i;
	}

	for (INT32 i = 0; i < 0x100; i++) {
		DrvColTable[i+0x80] = 0xc0 | ((i >> ((i&8)>>2)) & 0x30) | (i & 0x0f);
	}

	for (INT32 i = 0; i < 0x1000; i++) {

		UINT8 ctabentry;
		INT32 i_swapped = ((i & 0x0f) << 8) | ((i & 0xff0) >> 4);

		if (i & 0x80)
			ctabentry = 0x80 | (DrvColPROM[0x300 + (i >> 4)] & 0x0f) | ((i & 0x0c) << 2);
		else
			ctabentry = 0x80 | (DrvColPROM[0x300 + (i >> 4)] & 0x0f) | ((i & 0x03) << 4);

		DrvColTable[0x180 + i_swapped] = ctabentry;
	}
}

static void DrvNibbleExpand(UINT8 *rom, INT32 len)
{
	for (INT32 i = (len - 1) * 2; i >= 0; i -= 2) {
		rom[i+1] = rom[i/2] >> 4;
		rom[i+0] = rom[i/2] & 0xf;
	}
}

static INT32 DrvInit(INT32 game)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	game_mode = game;

	if (game == 0)
	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x08000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x10000,  2, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  3, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM1 + 0x04000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  5, 1)) return 1;
		memcpy (DrvGfxROM0 + 0x4000, DrvGfxROM0, 0x4000);

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x08000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x10000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x18000,  9, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000, 10, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x00001, 11, 2)) return 1;
		memcpy (DrvGfxROM2 + 0x10000, DrvGfxROM2, 0x10000);

		if (BurnLoadRom(DrvMapROM  + 0x00000, 12, 1)) return 1;
		if (BurnLoadRom(DrvMapROM  + 0x04000, 13, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000, 14, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00100, 15, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00200, 16, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00300, 17, 1)) return 1;

		if (BurnLoadRom(DrvSprPal  + 0x00000, 18, 1)) return 1;
	}

	if (game == 1)
	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x08000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x10000,  2, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  3, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM1 + 0x04000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x08000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x10000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x18000,  9, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000, 10, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x10000, 11, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x00001, 12, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x10001, 13, 2)) return 1;

		if (BurnLoadRom(DrvMapROM  + 0x00000, 14, 1)) return 1;
		if (BurnLoadRom(DrvMapROM  + 0x04000, 15, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000, 16, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00100, 17, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00200, 18, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00300, 19, 1)) return 1;

		if (BurnLoadRom(DrvSprPal  + 0x00000, 20, 1)) return 1;

		if (BurnLoadRom(nb1414_blit_data8b,   21, 1)) return 1;
	}

	DrvNibbleExpand(DrvGfxROM0, 0x08000);
	DrvNibbleExpand(DrvGfxROM1, 0x20000);
	DrvNibbleExpand(DrvGfxROM2, 0x20000);
	DrvPaletteTable();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvVidRAM,		0xd800, 0xdfff, MAP_WRITE);
	ZetMapMemory(DrvSprRAM,		0xe000, 0xffff, MAP_RAM);
	ZetSetOutHandler(galivan_main_write_port);
	ZetSetInHandler(galivan_main_read_port);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,	0xc000, 0xc7ff, MAP_RAM);
	ZetSetOutHandler(galivan_sound_write_port);
	ZetSetInHandler(galivan_sound_read_port);
	ZetClose();

	// dac0 -> dac1 -> dc-offset removal (hp filter) -> ym3526 -> OUT

	BurnYM3526Init(4000000, NULL, &DrvYM3526SynchroniseStream, 1);
	BurnTimerAttachZetYM3526(4000000);
	BurnYM3526SetRoute(BURN_SND_YM3526_ROUTE, 0.85, BURN_SND_ROUTE_BOTH);

	DACInit(0, 0, 0, DrvSyncDAC);
	DACInit(1, 0, 0, DrvSyncDAC);
	DACSetRoute(0, 0.50, BURN_SND_ROUTE_BOTH);
	DACSetRoute(1, 0.50, BURN_SND_ROUTE_BOTH);

	// #0 takes dac #0,1 and highpasses it a little to get rid of the dc offset.
	filter_rc_init(0, FLT_RC_HIGHPASS, 3846, 0, 0, CAP_N(0x310), 0);
	filter_rc_set_src_stereo(0);
	hpfiltbuffer = (INT16*)BurnMalloc(nBurnSoundLen*8); // for #0

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	BurnYM3526Exit();
	DACExit();
	filter_rc_exit();

	BurnFree(AllMem);

	BurnFree(hpfiltbuffer);
	hpfiltbuffer = NULL;

	nb1414_blit_data8b = NULL;

	return 0;
}

static void DrvPaletteInit()
{
	UINT32 tab[0x100];

	for (INT32 i = 0; i < 0x100; i++)
	{
		INT32 r = DrvColPROM[i+0x000] & 0xf;
		INT32 g = DrvColPROM[i+0x100] & 0xf;
		INT32 b = DrvColPROM[i+0x200] & 0xf;

		tab[i] = BurnHighCol(r+r*16, g+g*16, b+b*16, 0);
	}

	for (INT32 i = 0; i < 0x1180; i++) {
		DrvPalette[i] = tab[DrvColTable[i]];
	}
}

static void draw_fg_layer(INT32 mode)
{
	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sy = (offs & 0x1f) * 8;
		INT32 sx = (offs / 0x20) * 8;

		INT32 attr  = DrvVidRAM[offs + 0x400];
		INT32 code  = DrvVidRAM[offs] | ((attr & 0x03) << 8);
		INT32 color = (attr >> ((mode) ? 2 : 5)) & 7;

		if(offs < 0x12 && mode)
			code = attr = 0x01;

	//	necessary??
	//	INT32 category = -1;
	//	if (mode == 0) category = (attr & 8) ? 0 : 1;

		Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy - 16, color, 4, 0xf, 0, DrvGfxROM0);
	}
}

static void draw_bg_layer(INT32 mode)
{
	INT32 width = (mode) ? 512 : 128;
	INT32 height = (128 * 128) / width;

	INT32 xscroll = scrollx % (width * 16);
	INT32 yscroll = (scrolly + 16) % (height * 16);

	for (INT32 offs = 0; offs < width * height; offs++)
	{
		INT32 sx,sy;
		if (mode) {
			sx = ((offs / height) * 16) - xscroll;
			sy = ((offs % height) * 16) - yscroll;
		} else {
			sx = ((offs % width) * 16) - xscroll;
			sy = ((offs / width) * 16) - yscroll;
		}
		if (sx < -15) sx += width*16;
		if (sy < -15) sy += height*16;

		if (sx >= nScreenWidth || sy >= nScreenHeight) continue;

		INT32 attr = DrvMapROM[offs + 0x4000];
		INT32 code = DrvMapROM[offs] + ((attr & 0x03) << 8);
		INT32 color = ((attr & 0x60) >> 3) | ((mode) ? ((attr & 0x0c) >> 2) : ((attr & 0x18) >> 3));

		Render16x16Tile_Clip(pTransDraw, code, sx, sy, ((0x80/0x10)+color), 4, 0, DrvGfxROM1);
	}
}

static void draw_sprites(INT32 mode)
{
	for (INT32 offs = 0; offs < (mode ? 0x200 : 0x100); offs += 4)
	{
		INT32 attr = DrvSprBuf[offs + 2];
		INT32 code = DrvSprBuf[offs + 1] + ((attr & 0x06) << 7);
		INT32 color = (attr & 0x3c) >> 2;
		color += 16 * (DrvSprPal[code >> 2] & 0x0f);
		INT32 flipx = attr & 0x40;
		INT32 flipy = attr & 0x80;

		INT32 sx = (DrvSprBuf[offs + 3] - 0x80) + 256 * (attr & 0x01);
		INT32 sy = 240 - DrvSprBuf[offs + 0];

		if (flipscreen)
		{
			sx = 240 - sx;
			sy = 240 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		if (flipy) {
			if (flipx) {
				Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy - 16, color+(0x180/0x10), 4, 0xf, 0, DrvGfxROM2);
			} else {
				Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy - 16, color+(0x180/0x10), 4, 0xf, 0, DrvGfxROM2);
			}
		} else {
			if (flipx) {
				Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy - 16, color+(0x180/0x10), 4, 0xf, 0, DrvGfxROM2);
			} else {
				Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy - 16, color+(0x180/0x10), 4, 0xf, 0, DrvGfxROM2);
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

	if (display_disable || (nBurnLayer & 1) == 0) {
		BurnTransferClear(); // 0 should fill black
	} else {
		draw_bg_layer(game_mode);
	}

	if ((nBurnLayer & 2) && sprite_priority == 0) draw_sprites(game_mode);
	if (nBurnLayer & 4) draw_fg_layer(game_mode);
	if ((nBurnLayer & 8) && sprite_priority != 0) draw_sprites(game_mode);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();
	nb1414_frame8b++;

	{
		DrvInputs[0] = 0xff;
		DrvInputs[1] = 0xff;
		DrvInputs[2] = 0xff;
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 130; // sound irqs
	INT32 nCyclesTotal[2] = { 6000000 / 60, 4000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		INT32 nSegment = nCyclesTotal[0] / nInterleave;
		nCyclesDone[0] += ZetRun(nSegment);
		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(1);
		nSegment = nCyclesTotal[1] / nInterleave;
		BurnTimerUpdateYM3526((i + 1) * (nCyclesTotal[1] / nInterleave));

		ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();
	}

	ZetOpen(1);
	BurnTimerEndFrameYM3526(nCyclesTotal[1]);
	ZetClose();

	if (pBurnSoundOut) {
		ZetOpen(1);

		DACUpdate(pBurnSoundOut, nBurnSoundLen);
		filter_rc_update(0, pBurnSoundOut, hpfiltbuffer, nBurnSoundLen);
		memmove(pBurnSoundOut, hpfiltbuffer, nBurnSoundLen*4);

		BurnYM3526Update(pBurnSoundOut, nBurnSoundLen);

		ZetClose();
	}

	if (pBurnDraw) {
		DrvDraw();
	}

	memcpy (DrvSprBuf, DrvSprRAM, 0x200);

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

		BurnYM3526Scan(nAction, pnMin);

		SCAN_VAR(sprite_priority);
		SCAN_VAR(scrollx);
		SCAN_VAR(scrolly);
		SCAN_VAR(flipscreen);
		SCAN_VAR(soundlatch);
		SCAN_VAR(bankdata);
		SCAN_VAR(display_disable);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		bankswitch(bankdata);
		ZetClose();
	}

	return 0;
}



// Cosmo Police Galivan (12/26/1985)

static struct BurnRomInfo galivanRomDesc[] = {
	{ "1.1b",			0x8000, 0x1e66b3f8, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "2.3b",			0x4000, 0xa45964f1, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gv3.4b",			0x4000, 0x82f0c5e6, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "gv11.14b",		0x4000, 0x05f1a0e3, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code
	{ "gv12.15b",		0x8000, 0x5b7a0d6d, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "gv4.13d",		0x4000, 0x162490b4, 3 | BRF_GRA },           //  5 Characters

	{ "gv7.14f",		0x8000, 0xeaa1a0db, 4 | BRF_GRA },           //  6 Background Tiles
	{ "gv8.15f",		0x8000, 0xf174a41e, 4 | BRF_GRA },           //  7
	{ "gv9.17f",		0x8000, 0xedc60f5d, 4 | BRF_GRA },           //  8
	{ "gv10.19f",		0x8000, 0x41f27fca, 4 | BRF_GRA },           //  9

	{ "gv14.4f",		0x8000, 0x03e2229f, 5 | BRF_GRA },           // 10 Sprites
	{ "gv13.1f",		0x8000, 0xbca9e66b, 5 | BRF_GRA },           // 11

	{ "gv6.19d",		0x4000, 0xda38168b, 6 | BRF_GRA },           // 12 Background Tilemaps
	{ "gv5.17d",		0x4000, 0x22492d2a, 6 | BRF_GRA },           // 13

	{ "mb7114e.9f",		0x0100, 0xde782b3e, 7 | BRF_GRA },           // 14 Color data
	{ "mb7114e.10f",	0x0100, 0x0ae2a857, 7 | BRF_GRA },           // 15
	{ "mb7114e.11f",	0x0100, 0x7ba8b9d1, 7 | BRF_GRA },           // 16
	{ "mb7114e.2d",		0x0100, 0x75466109, 7 | BRF_GRA },           // 17

	{ "mb7114e.7f",		0x0100, 0x06538736, 8 | BRF_GRA },           // 18 Sprite LUT
};

STD_ROM_PICK(galivan)
STD_ROM_FN(galivan)

static INT32 galivanInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvGalivan = {
	"galivan", NULL, NULL, NULL, "1985",
	"Cosmo Police Galivan (12/26/1985)\0", NULL, "Nichibutsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, galivanRomInfo, galivanRomName, NULL, NULL, DrvInputInfo, GalivanDIPInfo,
	galivanInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1180,
	224, 256, 3, 4
};


// Cosmo Police Galivan (12/16/1985)

static struct BurnRomInfo galivan2RomDesc[] = {
	{ "gv1.1b",			0x8000, 0x5e480bfc, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "gv2.3b",			0x4000, 0x0d1b3538, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gv3.4b",			0x4000, 0x82f0c5e6, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "gv11.14b",		0x4000, 0x05f1a0e3, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code
	{ "gv12.15b",		0x8000, 0x5b7a0d6d, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "gv4.13d",		0x4000, 0x162490b4, 3 | BRF_GRA },           //  5 Characters

	{ "gv7.14f",		0x8000, 0xeaa1a0db, 4 | BRF_GRA },           //  6 Background Tiles
	{ "gv8.15f",		0x8000, 0xf174a41e, 4 | BRF_GRA },           //  7
	{ "gv9.17f",		0x8000, 0xedc60f5d, 4 | BRF_GRA },           //  8
	{ "gv10.19f",		0x8000, 0x41f27fca, 4 | BRF_GRA },           //  9

	{ "gv14.4f",		0x8000, 0x03e2229f, 5 | BRF_GRA },           // 10 Sprites
	{ "gv13.1f",		0x8000, 0xbca9e66b, 5 | BRF_GRA },           // 11

	{ "gv6.19d",		0x4000, 0xda38168b, 6 | BRF_GRA },           // 12 Background Tilemaps
	{ "gv5.17d",		0x4000, 0x22492d2a, 6 | BRF_GRA },           // 13

	{ "mb7114e.9f",		0x0100, 0xde782b3e, 7 | BRF_GRA },           // 14 Color data
	{ "mb7114e.10f",	0x0100, 0x0ae2a857, 7 | BRF_GRA },           // 15
	{ "mb7114e.11f",	0x0100, 0x7ba8b9d1, 7 | BRF_GRA },           // 16
	{ "mb7114e.2d",		0x0100, 0x75466109, 7 | BRF_GRA },           // 17

	{ "mb7114e.7f",		0x0100, 0x06538736, 8 | BRF_GRA },           // 18 Sprite LUT
};

STD_ROM_PICK(galivan2)
STD_ROM_FN(galivan2)

struct BurnDriver BurnDrvGalivan2 = {
	"galivan2", "galivan", NULL, NULL, "1985",
	"Cosmo Police Galivan (12/16/1985)\0", NULL, "Nichibutsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, galivan2RomInfo, galivan2RomName, NULL, NULL, DrvInputInfo, GalivanDIPInfo,
	galivanInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1180,
	224, 256, 3, 4
};


// Cosmo Police Galivan (12/11/1985)

static struct BurnRomInfo galivan3RomDesc[] = {
	{ "e-1.1b",			0x8000, 0xd8cc72b8, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "e-2.3b",			0x4000, 0x9e5b3157, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gv3.4b",			0x4000, 0x82f0c5e6, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "gv11.14b",		0x4000, 0x05f1a0e3, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code
	{ "gv12.15b",		0x8000, 0x5b7a0d6d, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "gv4.13d",		0x4000, 0x162490b4, 3 | BRF_GRA },           //  5 Characters

	{ "gv7.14f",		0x8000, 0xeaa1a0db, 4 | BRF_GRA },           //  6 Background Tiles
	{ "gv8.15f",		0x8000, 0xf174a41e, 4 | BRF_GRA },           //  7
	{ "gv9.17f",		0x8000, 0xedc60f5d, 4 | BRF_GRA },           //  8
	{ "gv10.19f",		0x8000, 0x41f27fca, 4 | BRF_GRA },           //  9

	{ "gv14.4f",		0x8000, 0x03e2229f, 5 | BRF_GRA },           // 10 Sprites
	{ "gv13.1f",		0x8000, 0xbca9e66b, 5 | BRF_GRA },           // 11

	{ "gv6.19d",		0x4000, 0xda38168b, 6 | BRF_GRA },           // 12 Background Tilemaps
	{ "gv5.17d",		0x4000, 0x22492d2a, 6 | BRF_GRA },           // 13

	{ "mb7114e.9f",		0x0100, 0xde782b3e, 7 | BRF_GRA },           // 14 Color data
	{ "mb7114e.10f",	0x0100, 0x0ae2a857, 7 | BRF_GRA },           // 15
	{ "mb7114e.11f",	0x0100, 0x7ba8b9d1, 7 | BRF_GRA },           // 16
	{ "mb7114e.2d",		0x0100, 0x75466109, 7 | BRF_GRA },           // 17

	{ "mb7114e.7f",		0x0100, 0x06538736, 8 | BRF_GRA },           // 18 Sprite LUT
};

STD_ROM_PICK(galivan3)
STD_ROM_FN(galivan3)

struct BurnDriver BurnDrvGalivan3 = {
	"galivan3", "galivan", NULL, NULL, "1985",
	"Cosmo Police Galivan (12/11/1985)\0", NULL, "Nichibutsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, galivan3RomInfo, galivan3RomName, NULL, NULL, DrvInputInfo, GalivanDIPInfo,
	galivanInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1180,
	224, 256, 3, 4
};


// Ufo Robo Dangar (4/07/1987)

static struct BurnRomInfo dangarRomDesc[] = {
	{ "8.1b",			0x8000, 0xfe4a3fd6, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "9.3b",			0x4000, 0x809d280f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "10.4b",			0x4000, 0x99a3591b, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "13.b14",			0x4000, 0x3e041873, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code
	{ "14.b15",			0x8000, 0x488e3463, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "5.13d",			0x4000, 0x40cb378a, 3 | BRF_GRA },           //  5 Characters

	{ "1.14f",			0x8000, 0xd59ed1f1, 4 | BRF_GRA },           //  6 Background Tiles
	{ "2.15f",			0x8000, 0xdfdb931c, 4 | BRF_GRA },           //  7
	{ "3.17f",			0x8000, 0x6954e8c3, 4 | BRF_GRA },           //  8
	{ "4.19f",			0x8000, 0x4af6a8bf, 4 | BRF_GRA },           //  9

	{ "12.f4",			0x8000, 0x55711884, 5 | BRF_GRA },           // 10 Sprites
	{ "11.f1",			0x8000, 0x8cf11419, 5 | BRF_GRA },           // 11

	{ "7.19d",			0x4000, 0x6dba32cf, 6 | BRF_GRA },           // 12 Background Tilemaps
	{ "6.17d",			0x4000, 0x6c899071, 6 | BRF_GRA },           // 13

	{ "82s129.9f",		0x0100, 0xb29f6a07, 7 | BRF_GRA },           // 14 Color data
	{ "82s129.10f",		0x0100, 0xc6de5ecb, 7 | BRF_GRA },           // 15
	{ "82s129.11f",		0x0100, 0xa5bbd6dc, 7 | BRF_GRA },           // 16
	{ "82s129.2d",		0x0100, 0xa4ac95a5, 7 | BRF_GRA },           // 17

	{ "82s129.7f",		0x0100, 0x29bc6216, 8 | BRF_GRA },           // 18 Sprite LUT
};

STD_ROM_PICK(dangar)
STD_ROM_FN(dangar)

struct BurnDriver BurnDrvDangar = {
	"dangar", NULL, NULL, NULL, "1986",
	"Ufo Robo Dangar (4/07/1987)\0", NULL, "Nichibutsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, dangarRomInfo, dangarRomName, NULL, NULL, DrvInputInfo, DangarDIPInfo,
	galivanInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1180,
	224, 256, 3, 4
};


// Ufo Robo Dangar (12/1/1986)

static struct BurnRomInfo dangaraRomDesc[] = {
	{ "8.1b",			0x8000, 0xe52638f2, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "9.3b",			0x4000, 0x809d280f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "10.4b",			0x4000, 0x99a3591b, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "13.14b",			0x4000, 0x3e041873, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code
	{ "14.15b",			0x8000, 0x488e3463, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "5.13d",			0x4000, 0x40cb378a, 3 | BRF_GRA },           //  5 Characters

	{ "1.14f",			0x8000, 0xd59ed1f1, 4 | BRF_GRA },           //  6 Background Tiles
	{ "2.15f",			0x8000, 0xdfdb931c, 4 | BRF_GRA },           //  7
	{ "3.17f",			0x8000, 0x6954e8c3, 4 | BRF_GRA },           //  8
	{ "4.19f",			0x8000, 0x4af6a8bf, 4 | BRF_GRA },           //  9

	{ "12.4f",			0x8000, 0x55711884, 5 | BRF_GRA },           // 10 Sprites
	{ "11.1f",			0x8000, 0x8cf11419, 5 | BRF_GRA },           // 11

	{ "7.19d",			0x4000, 0x6dba32cf, 6 | BRF_GRA },           // 12 Background Tilemaps
	{ "6.17d",			0x4000, 0x6c899071, 6 | BRF_GRA },           // 13
	
	{ "82s129.9f",		0x0100, 0xb29f6a07, 7 | BRF_GRA },           // 14 Color data
	{ "82s129.10f",		0x0100, 0xc6de5ecb, 7 | BRF_GRA },           // 15
	{ "82s129.11f",		0x0100, 0xa5bbd6dc, 7 | BRF_GRA },           // 16
	{ "82s129.2d",		0x0100, 0xa4ac95a5, 7 | BRF_GRA },           // 17

	{ "82s129.7f",		0x0100, 0x29bc6216, 8 | BRF_GRA },           // 18 Sprite LUT
};

STD_ROM_PICK(dangara)
STD_ROM_FN(dangara)

struct BurnDriver BurnDrvDangara = {
	"dangara", "dangar", NULL, NULL, "1986",
	"Ufo Robo Dangar (12/1/1986)\0", NULL, "Nichibutsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, dangaraRomInfo, dangaraRomName, NULL, NULL, DrvInputInfo, DangarDIPInfo, // dangar2
	galivanInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1180,
	224, 256, 3, 4
};


// Ufo Robo Dangar (9/26/1986)

static struct BurnRomInfo dangarbRomDesc[] = {
	{ "16.1b",			0x8000, 0x743fa2d4, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "17.3b",			0x4000, 0x1cdc60a5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "18.4b",			0x4000, 0xdb7f6613, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "13.14b",			0x4000, 0x3e041873, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code
	{ "14.15b",			0x8000, 0x488e3463, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "11.13d",			0x4000, 0xe804ffe1, 3 | BRF_GRA },           //  5 Characters

	{ "1.14f",			0x8000, 0xd59ed1f1, 4 | BRF_GRA },           //  6 Background Tiles
	{ "2.15f",			0x8000, 0xdfdb931c, 4 | BRF_GRA },           //  7
	{ "3.17f",			0x8000, 0x6954e8c3, 4 | BRF_GRA },           //  8
	{ "4.19f",			0x8000, 0x4af6a8bf, 4 | BRF_GRA },           //  9

	{ "12.4f",			0x8000, 0x55711884, 5 | BRF_GRA },           // 10 Sprites
	{ "11.1f",			0x8000, 0x8cf11419, 5 | BRF_GRA },           // 11

	{ "7.19d",			0x4000, 0x6dba32cf, 6 | BRF_GRA },           // 12 Background Tilemaps
	{ "6.17d",			0x4000, 0x6c899071, 6 | BRF_GRA },           // 13

	{ "82s129.9f",		0x0100, 0xb29f6a07, 7 | BRF_GRA },           // 14 Color data
	{ "82s129.10f",		0x0100, 0xc6de5ecb, 7 | BRF_GRA },           // 15
	{ "82s129.11f",		0x0100, 0xa5bbd6dc, 7 | BRF_GRA },           // 16
	{ "82s129.2d",		0x0100, 0xa4ac95a5, 7 | BRF_GRA },           // 17

	{ "82s129.7f",		0x0100, 0x29bc6216, 8 | BRF_GRA },           // 18 Sprite LUT
};

STD_ROM_PICK(dangarb)
STD_ROM_FN(dangarb)

struct BurnDriver BurnDrvDangarb = {
	"dangarb", "dangar", NULL, NULL, "1986",
	"Ufo Robo Dangar (9/26/1986)\0", NULL, "Nichibutsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, dangarbRomInfo, dangarbRomName, NULL, NULL, DrvInputInfo, DangarDIPInfo, // Dangar2
	galivanInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1180,
	224, 256, 3, 4
};


// Ufo Robo Dangar (9/26/1986, Japan)

static struct BurnRomInfo dangarjRomDesc[] = {
	{ "16.1b",			0x8000, 0x1e14b0b4, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "17.3b",			0x4000, 0x9ba92111, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "18.4b",			0x4000, 0xdb7f6613, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "21.14b",			0x4000, 0x3e041873, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code
	{ "22.15b",			0x4000, 0x1d484f68, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "11.13d",			0x4000, 0xe804ffe1, 3 | BRF_GRA },           //  5 Characters

	{ "7.14f",			0x8000, 0xd59ed1f1, 4 | BRF_GRA },           //  6 Background Tiles
	{ "8.15f",			0x8000, 0xdfdb931c, 4 | BRF_GRA },           //  7
	{ "9.17f",			0x8000, 0x6954e8c3, 4 | BRF_GRA },           //  8
	{ "10.19f",			0x8000, 0x4af6a8bf, 4 | BRF_GRA },           //  9

	{ "20.4f",			0x8000, 0x55711884, 5 | BRF_GRA },           // 10 Sprites
	{ "19.1f",			0x8000, 0x8cf11419, 5 | BRF_GRA },           // 11

	{ "15.19d",			0x4000, 0x6dba32cf, 6 | BRF_GRA },           // 12 Background Tilemaps
	{ "12.17d",			0x4000, 0x6c899071, 6 | BRF_GRA },           // 13

	{ "82s129.9f",		0x0100, 0xb29f6a07, 7 | BRF_GRA },           // 14 Color data
	{ "82s129.10f",		0x0100, 0xc6de5ecb, 7 | BRF_GRA },           // 15
	{ "82s129.11f",		0x0100, 0xa5bbd6dc, 7 | BRF_GRA },           // 16
	{ "82s129.2d",		0x0100, 0xa4ac95a5, 7 | BRF_GRA },           // 17

	{ "82s129.7f",		0x0100, 0x29bc6216, 8 | BRF_GRA },           // 18 Sprite LUT

	{ "dg-3.ic7.2764",	0x2000, 0x84a56d26, 9 | BRF_GRA },           // 19 user2
};

STD_ROM_PICK(dangarj)
STD_ROM_FN(dangarj)

struct BurnDriver BurnDrvDangarj = {
	"dangarj", "dangar", NULL, NULL, "1986",
	"Ufo Robo Dangar (9/26/1986, Japan)\0", NULL, "Nichibutsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, dangarjRomInfo, dangarjRomName, NULL, NULL, DrvInputInfo, DangarDIPInfo, // Dangar2
	galivanInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1180,
	224, 256, 3, 4
};


// Ufo Robo Dangar (bootleg)

static struct BurnRomInfo dangarbtRomDesc[] = {
	{ "8",				0x8000, 0x8136fd10, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "9",				0x4000, 0x3ce5ec11, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "dangar2.018",	0x4000, 0xdb7f6613, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "dangar13.b14",	0x4000, 0x3e041873, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code
	{ "dangar14.b15",	0x8000, 0x488e3463, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "dangar2.011",	0x4000, 0xe804ffe1, 3 | BRF_GRA },           //  5 Characters

	{ "dangar01.14f",	0x8000, 0xd59ed1f1, 4 | BRF_GRA },           //  6 Background Tiles
	{ "dangar02.15f",	0x8000, 0xdfdb931c, 4 | BRF_GRA },           //  7
	{ "dangar03.17f",	0x8000, 0x6954e8c3, 4 | BRF_GRA },           //  8
	{ "dangar04.19f",	0x8000, 0x4af6a8bf, 4 | BRF_GRA },           //  9

	{ "dangarxx.f4",	0x8000, 0x55711884, 5 | BRF_GRA },           // 10 Sprites
	{ "dangarxx.f1",	0x8000, 0x8cf11419, 5 | BRF_GRA },           // 11

	{ "dangar07.19d",	0x4000, 0x6dba32cf, 6 | BRF_GRA },           // 12 Background Tilemaps
	{ "dangar06.17d",	0x4000, 0x6c899071, 6 | BRF_GRA },           // 13

	{ "82s129.9f",		0x0100, 0xb29f6a07, 7 | BRF_GRA },           // 14 Color data
	{ "82s129.10f",		0x0100, 0xc6de5ecb, 7 | BRF_GRA },           // 15
	{ "82s129.11f",		0x0100, 0xa5bbd6dc, 7 | BRF_GRA },           // 16
	{ "82s129.2d",		0x0100, 0xa4ac95a5, 7 | BRF_GRA },           // 17

	{ "82s129.7f",		0x0100, 0x29bc6216, 8 | BRF_GRA },           // 18 Sprite LUT
};

STD_ROM_PICK(dangarbt)
STD_ROM_FN(dangarbt)

struct BurnDriver BurnDrvDangarbt = {
	"dangarbt", "dangar", NULL, NULL, "1986",
	"Ufo Robo Dangar (bootleg)\0", NULL, "Nichibutsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, dangarbtRomInfo, dangarbtRomName, NULL, NULL, DrvInputInfo, DangarDIPInfo, // Dangar2
	galivanInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1180,
	224, 256, 3, 4
};


// Ninja Emaki (US)

static struct BurnRomInfo ninjemakRomDesc[] = {
	{ "ninjemak.1",		0x8000, 0x12b0a619, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "ninjemak.2",		0x4000, 0xd5b505d1, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ninjemak.3",		0x8000, 0x68c92bf6, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "ninjemak.12",	0x4000, 0x3d1cd329, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code
	{ "ninjemak.13",	0x8000, 0xac3a0b81, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "ninjemak.4",		0x8000, 0x83702c37, 3 | BRF_GRA },           //  5 Characters

	{ "ninjemak.8",		0x8000, 0x655f0a58, 4 | BRF_GRA },           //  6 Background Tiles
	{ "ninjemak.9",		0x8000, 0x934e1703, 4 | BRF_GRA },           //  7
	{ "ninjemak.10",	0x8000, 0x955b5c45, 4 | BRF_GRA },           //  8
	{ "ninjemak.11",	0x8000, 0xbbd2e51c, 4 | BRF_GRA },           //  9

	{ "ninjemak.16",	0x8000, 0x8df93fed, 5 | BRF_GRA },           // 10 Sprites
	{ "ninjemak.17",	0x8000, 0xa3efd0fc, 5 | BRF_GRA },           // 11
	{ "ninjemak.14",	0x8000, 0xbff332d3, 5 | BRF_GRA },           // 12
	{ "ninjemak.15",	0x8000, 0x56430ed4, 5 | BRF_GRA },           // 13

	{ "ninjemak.7",		0x4000, 0x80c20d36, 6 | BRF_GRA },           // 14 Background Tilemaps
	{ "ninjemak.6",		0x4000, 0x1da7a651, 6 | BRF_GRA },           // 15

	{ "ninjemak.pr1",	0x0100, 0x8a62d4e4, 7 | BRF_GRA },           // 16 Color data
	{ "ninjemak.pr2",	0x0100, 0x2ccf976f, 7 | BRF_GRA },           // 17
	{ "ninjemak.pr3",	0x0100, 0x16b2a7a4, 7 | BRF_GRA },           // 18
	{ "yncp-2d.bin",	0x0100, 0x23bade78, 7 | BRF_GRA },           // 19

	{ "yncp-7f.bin",	0x0100, 0x262d0809, 8 | BRF_GRA },           // 20 Sprite LUT

	{ "ninjemak.5",		0x4000, 0x5f91dd30, 9 | BRF_GRA },           // 21 nb1414m4 data
};

STD_ROM_PICK(ninjemak)
STD_ROM_FN(ninjemak)

static INT32 ninjemakInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvNinjemak = {
	"ninjemak", NULL, NULL, NULL, "1986",
	"Ninja Emaki (US)\0", NULL, "Nichibutsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, ninjemakRomInfo, ninjemakRomName, NULL, NULL, ninjemakInputInfo, NinjemakDIPInfo,
	ninjemakInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1180,
	224, 256, 3, 4
};


// Youma Ninpou Chou (Japan)

static struct BurnRomInfo youmaRomDesc[] = {
	{ "ync-1.bin",		0x8000, 0x0552adab, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "ync-2.bin",		0x4000, 0xf961e5e6, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ync-3.bin",		0x8000, 0x9ad50a5e, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "ninjemak.12",	0x4000, 0x3d1cd329, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code
	{ "ninjemak.13",	0x8000, 0xac3a0b81, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "ync-4.bin",		0x8000, 0xa1954f44, 3 | BRF_GRA },           //  5 Characters

	{ "ninjemak.8",		0x8000, 0x655f0a58, 4 | BRF_GRA },           //  6 Background Tiles
	{ "ninjemak.9",		0x8000, 0x934e1703, 4 | BRF_GRA },           //  7
	{ "ninjemak.10",	0x8000, 0x955b5c45, 4 | BRF_GRA },           //  8
	{ "ninjemak.11",	0x8000, 0xbbd2e51c, 4 | BRF_GRA },           //  9

	{ "ninjemak.16",	0x8000, 0x8df93fed, 5 | BRF_GRA },           // 10 Sprites
	{ "ninjemak.17",	0x8000, 0xa3efd0fc, 5 | BRF_GRA },           // 11
	{ "ninjemak.14",	0x8000, 0xbff332d3, 5 | BRF_GRA },           // 12
	{ "ninjemak.15",	0x8000, 0x56430ed4, 5 | BRF_GRA },           // 13

	{ "ninjemak.7",		0x4000, 0x80c20d36, 6 | BRF_GRA },           // 14 Background Tilemaps
	{ "ninjemak.6",		0x4000, 0x1da7a651, 6 | BRF_GRA },           // 15

	{ "yncp-6e.bin",	0x0100, 0xea47b91a, 8 | BRF_GRA },           // 16 Color data
	{ "yncp-7e.bin",	0x0100, 0xe94c0fed, 8 | BRF_GRA },           // 17
	{ "yncp-8e.bin",	0x0100, 0xffb4b287, 8 | BRF_GRA },           // 18
	{ "yncp-2d.bin",	0x0100, 0x23bade78, 8 | BRF_GRA },           // 19

	{ "yncp-7f.bin",	0x0100, 0x262d0809, 9 | BRF_GRA },           // 20 Sprite LUT

	{ "ync-5.bin",		0x4000, 0x993e4ab2, 7 | BRF_GRA },           // 21 nb1414m4 data
};

STD_ROM_PICK(youma)
STD_ROM_FN(youma)

struct BurnDriver BurnDrvYouma = {
	"youma", "ninjemak", NULL, NULL, "1986",
	"Youma Ninpou Chou (Japan)\0", NULL, "Nichibutsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, youmaRomInfo, youmaRomName, NULL, NULL, ninjemakInputInfo, NinjemakDIPInfo,
	ninjemakInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1180,
	224, 256, 3, 4
};


// Youma Ninpou Chou (Japan, alt)

static struct BurnRomInfo youma2RomDesc[] = {
	{ "1.1d",			0x8000, 0x171dbe99, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "2.3d",			0x4000, 0xe502d62a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.4d",			0x8000, 0xcb84745c, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "12.14b",			0x4000, 0x3d1cd329, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code
	{ "13.15b",			0x8000, 0xac3a0b81, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "4.7d",			0x8000, 0x40aeffd8, 3 | BRF_GRA },           //  5 Characters

	{ "ninjemak.8",		0x8000, 0x655f0a58, 4 | BRF_GRA },           //  6 Background Tiles
	{ "ninjemak.9",		0x8000, 0x934e1703, 4 | BRF_GRA },           //  7
	{ "ninjemak.10",	0x8000, 0x955b5c45, 4 | BRF_GRA },           //  8
	{ "ninjemak.11",	0x8000, 0xbbd2e51c, 4 | BRF_GRA },           //  9

	{ "ninjemak.16",	0x8000, 0x8df93fed, 5 | BRF_GRA },           // 10 Sprites
	{ "ninjemak.17",	0x8000, 0xa3efd0fc, 5 | BRF_GRA },           // 11
	{ "ninjemak.14",	0x8000, 0xbff332d3, 5 | BRF_GRA },           // 12
	{ "ninjemak.15",	0x8000, 0x56430ed4, 5 | BRF_GRA },           // 13

	{ "ninjemak.7",		0x4000, 0x80c20d36, 6 | BRF_GRA },           // 14 Background Tilemaps
	{ "ninjemak.6",		0x4000, 0x1da7a651, 6 | BRF_GRA },           // 15

	{ "bpr.6e",			0x0100, 0x8a62d4e4, 8 | BRF_GRA },           // 16 Color data
	{ "bpr.7e",			0x0100, 0x2ccf976f, 8 | BRF_GRA },           // 17
	{ "bpr.8e",			0x0100, 0x16b2a7a4, 8 | BRF_GRA },           // 18
	{ "bpr.2d",			0x0100, 0x23bade78, 8 | BRF_GRA },           // 19

	{ "bpr.7f",			0x0100, 0x262d0809, 9 | BRF_GRA },           // 20 Sprite LUT

	{ "5.15d",			0x4000, 0x1b4f64aa, 7 | BRF_GRA },           // 21 nb1414m4 data
};

STD_ROM_PICK(youma2)
STD_ROM_FN(youma2)

struct BurnDriver BurnDrvYouma2 = {
	"youma2", "ninjemak", NULL, NULL, "1986",
	"Youma Ninpou Chou (Japan, alt)\0", NULL, "Nichibutsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, youma2RomInfo, youma2RomName, NULL, NULL, ninjemakInputInfo, NinjemakDIPInfo,
	ninjemakInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1180,
	224, 256, 3, 4
};


// Youma Ninpou Chou (Game Electronics bootleg, set 1)

static struct BurnRomInfo youmabRomDesc[] = {
	{ "electric1.3u",	0x8000, 0xcc4fdb92, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "electric3.3r",	0x8000, 0xc1bc7387, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "electric2.3t",	0x8000, 0x99aee3bc, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "electric12.5e",	0x4000, 0x3d1cd329, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code
	{ "electric13.5d",	0x8000, 0xac3a0b81, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "electric4.3m",	0x8000, 0xa1954f44, 3 | BRF_GRA },           //  5 Characters

	{ "electric8.1f",	0x8000, 0x655f0a58, 4 | BRF_GRA },           //  6 Background Tiles
	{ "electric9.1d",	0x8000, 0x77a964c1, 4 | BRF_GRA },           //  7
	{ "electric10.1b",	0x8000, 0x955b5c45, 4 | BRF_GRA },           //  8
	{ "electric11.1a",	0x8000, 0xbbd2e51c, 4 | BRF_GRA },           //  9

	{ "electric16.1p",	0x8000, 0x8df93fed, 5 | BRF_GRA },           // 10 Sprites
	{ "electric17.1m",	0x8000, 0xa3efd0fc, 5 | BRF_GRA },           // 11
	{ "electric14.1t",	0x8000, 0xbff332d3, 5 | BRF_GRA },           // 12
	{ "electric15.1r",	0x8000, 0x56430ed4, 5 | BRF_GRA },           // 13

	{ "electric7.3a",	0x4000, 0x80c20d36, 6 | BRF_GRA },           // 14 Background Tilemaps
	{ "electric6.3b",	0x4000, 0x1da7a651, 6 | BRF_GRA },           // 15

	{ "prom82s129.2n",	0x0100, 0xea47b91a, 7 | BRF_GRA },           // 16 Color data
	{ "prom82s129.2m",	0x0100, 0xe94c0fed, 7 | BRF_GRA },           // 17
	{ "prom82s129.2l",	0x0100, 0xffb4b287, 7 | BRF_GRA },           // 18
	{ "prom82s129.3s",	0x0100, 0x23bade78, 7 | BRF_GRA },           // 19

	{ "prom82s129.1l",	0x0100, 0x262d0809, 8 | BRF_GRA },           // 20 Sprite LUT
};

STD_ROM_PICK(youmab)
STD_ROM_FN(youmab)

struct BurnDriverD BurnDrvYoumab = {
	"youmab", "ninjemak", NULL, NULL, "1986",
	"Youma Ninpou Chou (Game Electronics bootleg, set 1)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	0 | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, youmabRomInfo, youmabRomName, NULL, NULL, ninjemakInputInfo, NinjemakDIPInfo,
	ninjemakInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1180,
	224, 256, 3, 4
};


// Youma Ninpou Chou (Game Electronics bootleg, set 2)

static struct BurnRomInfo youmab2RomDesc[] = {
	{ "1.1d",		0x8000, 0x692ae497, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "3.4d",		0x8000, 0xebf61afc, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "2.2d",		0x8000, 0x99aee3bc, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "11.13b",		0x4000, 0x3d1cd329, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code
	{ "12.15b",		0x8000, 0xac3a0b81, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "4.7d",		0x8000, 0xa1954f44, 3 | BRF_GRA },           //  5 Characters

	{ "7.13f",		0x8000, 0x655f0a58, 4 | BRF_GRA },           //  6 Background Tiles
	{ "8.15f",		0x8000, 0x934e1703, 4 | BRF_GRA },           //  7
	{ "9.16f",		0x8000, 0x955b5c45, 4 | BRF_GRA },           //  8
	{ "10.18f",		0x8000, 0xbbd2e51c, 4 | BRF_GRA },           //  9

	{ "15.4h",		0x8000, 0x8df93fed, 5 | BRF_GRA },           // 10 Sprites
	{ "16.6h",		0x8000, 0xa3efd0fc, 5 | BRF_GRA },           // 11
	{ "13.1h",		0x8000, 0xbff332d3, 5 | BRF_GRA },           // 12
	{ "14.3h",		0x8000, 0x56430ed4, 5 | BRF_GRA },           // 13

	{ "6.18d",		0x4000, 0x80c20d36, 6 | BRF_GRA },           // 14 Background Tilemaps
	{ "5.17d",		0x4000, 0x1da7a651, 6 | BRF_GRA },           // 15

	{ "pr.6e",		0x0100, 0xea47b91a, 7 | BRF_GRA },           // 16 Color data
	{ "pr.7e",		0x0100, 0x6d66da81, 7 | BRF_GRA },           // 17
	{ "pr.8e",		0x0100, 0xffb4b287, 7 | BRF_GRA },           // 18
	{ "pr.2e",		0x0100, 0x23bade78, 7 | BRF_GRA },           // 19

	{ "pr.7h",		0x0100, 0x262d0809, 8 | BRF_GRA },           // 20 Sprite LUT
};

STD_ROM_PICK(youmab2)
STD_ROM_FN(youmab2)

struct BurnDriverD BurnDrvYoumab2 = {
	"youmab2", "ninjemak", NULL, NULL, "1986",
	"Youma Ninpou Chou (Game Electronics bootleg, set 2)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	0 | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, youmab2RomInfo, youmab2RomName, NULL, NULL, ninjemakInputInfo, NinjemakDIPInfo,
	ninjemakInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1180,
	224, 256, 3, 4
};
