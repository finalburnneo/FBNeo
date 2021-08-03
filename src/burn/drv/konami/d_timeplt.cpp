// FB Alpha Time Pilot driver module
// Based on MAME driver by Nicola Salmoria

/*
	To do:
	    tc8830f sound core (only for chkun - *very* low priority)
*/


#include "tiles_generic.h"
#include "z80_intf.h"
#include "timeplt_snd.h"
//#include "tc8830f.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvSndROM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvVidRAM;
static UINT8 *DrvColRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprTmp;

static UINT32 *DrvPalette;
static UINT8  DrvRecalc;

static UINT8 nmi_enable;
static UINT8 last_sound_irq;

static UINT8  DrvJoy1[8];
static UINT8  DrvJoy2[8];
static UINT8  DrvJoy3[8];
static UINT8  DrvDips[2];
static UINT8  DrvInputs[3];
static UINT8  DrvReset;

static INT32 game_select;
static INT32 watchdog;

static struct BurnInputInfo TimepltInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",	BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",	BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"	},
	{"P2 Right",	BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},
	{"P2 Button 1",	BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Timeplt)

static struct BurnInputInfo ChkunInputList[] = {
	{"Coin 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"Coin 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"Button 1",	BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"Bet 1B",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 fire 2"	},
	{"Bet 2B",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 fire 3"	},
	{"Bet 3B",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 4"	},
	{"Bet HR",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 5"	},
	{"Keyout",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 6"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Chkun)

static struct BurnInputInfo BikkuricInputList[] = {
	{"Coin 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"Coin 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 coin"	},
	{"Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"Right",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 right"	},
	{"Button 1",	BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"Keyout",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Bikkuric)

static struct BurnDIPInfo TimepltDIPList[]=
{
	{0x10, 0xff, 0xff, 0xff, NULL				},
	{0x11, 0xff, 0xff, 0x4b, NULL				},

	{0   , 0xfe, 0   ,    16, "Coin A"			},
	{0x10, 0x01, 0x0f, 0x02, "4 Coins 1 Credits"		},
	{0x10, 0x01, 0x0f, 0x05, "3 Coins 1 Credits"		},
	{0x10, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"		},
	{0x10, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"		},
	{0x10, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"		},
	{0x10, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"		},
	{0x10, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"		},
	{0x10, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"		},
	{0x10, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"		},
	{0x10, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"		},
	{0x10, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"		},
	{0x10, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"		},
	{0x10, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"		},
	{0x10, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"		},
	{0x10, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"		},
	{0x10, 0x01, 0x0f, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    16, "Coin B"			},
	{0x10, 0x01, 0xf0, 0x20, "4 Coins 1 Credits"		},
	{0x10, 0x01, 0xf0, 0x50, "3 Coins 1 Credits"		},
	{0x10, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"		},
	{0x10, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"		},
	{0x10, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"		},
	{0x10, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x10, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"		},
	{0x10, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"		},
	{0x10, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"		},
	{0x10, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"		},
	{0x10, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"		},
	{0x10, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"		},
	{0x10, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"		},
	{0x10, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"		},
	{0x10, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"		},
	{0x10, 0x01, 0xf0, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x11, 0x01, 0x03, 0x03, "3"				},
	{0x11, 0x01, 0x03, 0x02, "4"				},
	{0x11, 0x01, 0x03, 0x01, "5"				},
	{0x11, 0x01, 0x03, 0x00, "255 (Cheat)"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x11, 0x01, 0x04, 0x00, "Upright"			},
	{0x11, 0x01, 0x04, 0x04, "Cocktail"			},

	{0   , 0xfe, 0   ,    2, "Bonus Life"			},
	{0x11, 0x01, 0x08, 0x08, "10000 50000"			},
	{0x11, 0x01, 0x08, 0x00, "20000 60000"			},

	{0   , 0xfe, 0   ,    8, "Difficulty"			},
	{0x11, 0x01, 0x70, 0x70, "1 (Easiest)"			},
	{0x11, 0x01, 0x70, 0x60, "2"				},
	{0x11, 0x01, 0x70, 0x50, "3"				},
	{0x11, 0x01, 0x70, 0x40, "4"				},
	{0x11, 0x01, 0x70, 0x30, "5"				},
	{0x11, 0x01, 0x70, 0x20, "6"				},
	{0x11, 0x01, 0x70, 0x10, "7"				},
	{0x11, 0x01, 0x70, 0x00, "8 (Difficult)"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x11, 0x01, 0x80, 0x80, "Off"				},
	{0x11, 0x01, 0x80, 0x00, "On"				},
};

STDDIPINFO(Timeplt)

static struct BurnDIPInfo PsurgeDIPList[]=
{
	{0x10, 0xff, 0xff, 0xff, NULL				},
	{0x11, 0xff, 0xff, 0x7f, NULL				},

	{0   , 0xfe, 0   ,    2, "Initial Energy"		},
	{0x10, 0x01, 0x08, 0x00, "4"				},
	{0x10, 0x01, 0x08, 0x08, "6"				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x10, 0x01, 0x30, 0x30, "3"				},
	{0x10, 0x01, 0x30, 0x20, "4"				},
	{0x10, 0x01, 0x30, 0x10, "5"				},
	{0x10, 0x01, 0x30, 0x00, "6"				},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x10, 0x01, 0x80, 0x80, "Upright"			},
	{0x10, 0x01, 0x80, 0x00, "Cocktail"			},

	{0   , 0xfe, 0   ,    4, "Coin A"			},
	{0x11, 0x01, 0x03, 0x02, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x03, 0x03, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x03, 0x01, "1 Coin  3 Credits"		},
	{0x11, 0x01, 0x03, 0x00, "1 Coin  5 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"			},
	{0x11, 0x01, 0x0c, 0x08, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x0c, 0x04, "1 Coin  3 Credits"		},
	{0x11, 0x01, 0x0c, 0x00, "1 Coin  5 Credits"		},

	{0   , 0xfe, 0   ,    2, "Infinite Shots (Cheat)"	},
	{0x11, 0x01, 0x10, 0x10, "Off"				},
	{0x11, 0x01, 0x10, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Stop at Junctions"		},
	{0x11, 0x01, 0x80, 0x80, "No"				},
	{0x11, 0x01, 0x80, 0x00, "Yes"				},
};

STDDIPINFO(Psurge)

static struct BurnDIPInfo ChkunDIPList[]=
{
	{0x0a, 0xff, 0xff, 0x7f, NULL				},
	{0x0b, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x0a, 0x01, 0x40, 0x40, "Off"				},
	{0x0a, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Freeze"			},
	{0x0a, 0x01, 0x80, 0x00, "Off"				},
	{0x0a, 0x01, 0x80, 0x80, "On"				},

	{0   , 0xfe, 0   ,    2, "Coin A"			},
	{0x0b, 0x01, 0x01, 0x01, "2 Coins 1 Credits"		},
	{0x0b, 0x01, 0x01, 0x00, "1 Coin  1 Credits"		},
};

STDDIPINFO(Chkun)

static struct BurnDIPInfo BikkuricDIPList[]=
{
	{0x09, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    2, "Coin A"			},
	{0x09, 0x01, 0x01, 0x01, "2 Coins 1 Credits"		},
	{0x09, 0x01, 0x01, 0x00, "1 Coin  1 Credits"		},
};

STDDIPINFO(Bikkuric)

static void __fastcall timeplt_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xc000:
			TimepltSndSoundlatch(data);
		return;

		case 0xc200:
			watchdog = 0;
		return;

		case 0xc300:
			if (game_select != 2) {		// psurge doesn't use this
				nmi_enable = data & 1;
				if (!nmi_enable) ZetSetIRQLine(0x20, CPU_IRQSTATUS_NONE);
			}
		return;

		case 0xc302:
	//		flipscreen = ~data & 1;
		return;

		case 0xc304:
			if (last_sound_irq == 0 && data) {
				ZetSetVector(1, 0xff);
				ZetSetIRQLine(1, 0, CPU_IRQSTATUS_ACK);
			}
			last_sound_irq = data;
		return;

		case 0xc30a:
			// coin counter
		return;
	}
}

static UINT8 __fastcall timeplt_main_read(UINT16 address)
{
	switch (address)
	{
		case 0x6004:
			return 0x80; // psurge protection

		case 0xc000:
			return ZetTotalCycles() / 200; // vpos

		case 0xc200:
			return DrvDips[1];

		case 0xc300:
			return DrvInputs[0];

		case 0xc320:
			if (game_select > 2) return (DrvInputs[1] & ~0x02) | (ZetTotalCycles() & 0x02);
			return DrvInputs[1];

		case 0xc340:
			return DrvInputs[2];

		case 0xc360:
			return DrvDips[0];
	}

	return 0;
}

#if 0
static void AY8910_1_portA_w(UINT32, UINT32 /*data*/)
{
//	if (~data & 0x40) tc8830fWrite(data & 0xf);
//	if (~data & 0x10) tc8830fReset();
}
#endif

static INT32 DrvDoReset(INT32 clear_ram)
{
	if (clear_ram) {
		memset(AllRam, 0, RamEnd - AllRam);
	}

	ZetReset(0);

	TimepltSndReset();

//	tc8830fReset();

	last_sound_irq = 0;
	nmi_enable = 0;

	watchdog = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x006000;
	DrvZ80ROM1		= Next; Next += 0x002000;

	DrvGfxROM0		= Next; Next += 0x020000;
	DrvGfxROM1		= Next; Next += 0x010000;

	DrvColPROM		= Next; Next += 0x000240;

	DrvSndROM		= Next; Next += 0x020000;

	DrvPalette		= (UINT32*)Next; Next += 0x0180 * sizeof(UINT32);

	DrvSprTmp		= Next; Next += 0x6000;

	AllRam			= Next;

	DrvZ80RAM0		= Next; Next += 0x001000;
	DrvZ80RAM1		= Next; Next += 0x000400;
	DrvVidRAM		= Next; Next += 0x000400;
	DrvColRAM		= Next; Next += 0x000400;
	DrvSprRAM		= Next; Next += 0x000200;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode(INT32 mode)
{
	INT32 Plane[3]  = { 4, 0, 4 };
	INT32 XOffs[16] = { STEP4(0,1), STEP4(8*8,1), STEP4(16*8,1), STEP4(24*8,1) };
	INT32 YOffs[16] = { STEP8(0,8), STEP8(32*8,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x8000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x8000);

	GfxDecode(0x0800, 2,  8,  8, Plane, XOffs, YOffs, 0x080, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x4000);

	GfxDecode(0x0100, 2, 16, 16, Plane + mode, XOffs, YOffs, 0x200, tmp, DrvGfxROM1);

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

	if (game_select == 1) // timeplt
	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x02000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x04000,  2, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x02000,  6, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000,  7, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00020,  8, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00040,  9, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00140, 10, 1)) return 1;

		DrvGfxDecode(0);
	}

	if (game_select == 2) // psurge
	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x02000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x04000,  2, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  3, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM1 + 0x01000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x02000,  7, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000,  8, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00020,  9, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00040, 10, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00140, 11, 1)) return 1;

		DrvGfxDecode(0);
	}

	if (game_select == 3) // chkun
	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x04000,  1, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  4, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000,  5, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00020,  6, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00040,  7, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00140,  8, 1)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x00000,  9, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x10000, 10, 1)) return 1;

		DrvGfxDecode(1);
	}

	if (game_select == 4) // bikkuric
	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x04000,  1, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  4, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000,  5, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00020,  6, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00040,  7, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00140,  8, 1)) return 1;

		DrvGfxDecode(1);
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x5fff, MAP_ROM);
	if (game_select >= 3) {
		ZetMapMemory(DrvZ80RAM0 + 0x0800,0x6000, 0x67ff, MAP_RAM);
	}
	ZetMapMemory(DrvColRAM,		0xa000, 0xa3ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,		0xa400, 0xa7ff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM0,	0xa800, 0xafff, MAP_RAM);
	ZetMapMemory(DrvSprRAM + 0x0000,0xb000, 0xb0ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM + 0x0000,0xb100, 0xb1ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM + 0x0000,0xb200, 0xb2ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM + 0x0000,0xb300, 0xb3ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM + 0x0100,0xb400, 0xb4ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM + 0x0100,0xb500, 0xb5ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM + 0x0100,0xb600, 0xb6ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM + 0x0100,0xb700, 0xb7ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM + 0x0000,0xb800, 0xb8ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM + 0x0000,0xb900, 0xb9ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM + 0x0000,0xba00, 0xbaff, MAP_RAM);
	ZetMapMemory(DrvSprRAM + 0x0000,0xbb00, 0xbbff, MAP_RAM);
	ZetMapMemory(DrvSprRAM + 0x0100,0xbc00, 0xbcff, MAP_RAM);
	ZetMapMemory(DrvSprRAM + 0x0100,0xbd00, 0xbdff, MAP_RAM);
	ZetMapMemory(DrvSprRAM + 0x0100,0xbe00, 0xbeff, MAP_RAM);
	ZetMapMemory(DrvSprRAM + 0x0100,0xbf00, 0xbfff, MAP_RAM);
	ZetSetWriteHandler(timeplt_main_write);
	ZetSetReadHandler(timeplt_main_read);
	ZetClose();

	TimepltSndInit(DrvZ80ROM1, DrvZ80RAM1, 1);
	TimepltSndSrcGain(0.55); // quench distortion when ship blows up

//	tc8830fInit(512000, DrvSndROM, 0x20000, 1);
//	tc8830fSetAllRoutes(0.60, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	TimepltSndExit();

//	tc8830fInit();

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	UINT32 pens[32];

	for (INT32 i = 0; i < 32; i++)
	{
		INT32 bit0 = (DrvColPROM[i + 1 * 32] >> 1) & 0x01;
		INT32 bit1 = (DrvColPROM[i + 1 * 32] >> 2) & 0x01;
		INT32 bit2 = (DrvColPROM[i + 1 * 32] >> 3) & 0x01;
		INT32 bit3 = (DrvColPROM[i + 1 * 32] >> 4) & 0x01;
		INT32 bit4 = (DrvColPROM[i + 1 * 32] >> 5) & 0x01;
		INT32 r = 0x19 * bit0 + 0x24 * bit1 + 0x35 * bit2 + 0x40 * bit3 + 0x4d * bit4;
		bit0 = (DrvColPROM[i + 1 * 32] >> 6) & 0x01;
		bit1 = (DrvColPROM[i + 1 * 32] >> 7) & 0x01;
		bit2 = (DrvColPROM[i + 0 * 32] >> 0) & 0x01;
		bit3 = (DrvColPROM[i + 0 * 32] >> 1) & 0x01;
		bit4 = (DrvColPROM[i + 0 * 32] >> 2) & 0x01;
		INT32 g = 0x19 * bit0 + 0x24 * bit1 + 0x35 * bit2 + 0x40 * bit3 + 0x4d * bit4;
		bit0 = (DrvColPROM[i + 0 * 32] >> 3) & 0x01;
		bit1 = (DrvColPROM[i + 0 * 32] >> 4) & 0x01;
		bit2 = (DrvColPROM[i + 0 * 32] >> 5) & 0x01;
		bit3 = (DrvColPROM[i + 0 * 32] >> 6) & 0x01;
		bit4 = (DrvColPROM[i + 0 * 32] >> 7) & 0x01;
		INT32 b = 0x19 * bit0 + 0x24 * bit1 + 0x35 * bit2 + 0x40 * bit3 + 0x4d * bit4;

		pens[i] = BurnHighCol(r,g,b,0);
	}

	for (INT32 i = 0; i < 256; i++) {
		DrvPalette[i + 0x80] = pens[DrvColPROM[i+0x040] & 0x0f];
	}

	for (INT32 i = 0; i < 128; i++) {
		DrvPalette[i + 0x00] = pens[(DrvColPROM[i+0x140] & 0x0f) + 0x10];
	}
}

static void draw_layer(INT32 priority)
{
	INT32 global_flip = (game_select == 2) ? ((32*32)-1) : 0;

	for (INT32 offs = (32 * 2); offs < (32 * 32) - (32 * 2); offs++)
	{
		INT32 sx = (offs & 0x1f) * 8;
		INT32 sy = (offs / 0x20) * 8;

		INT32 attr  = DrvColRAM[offs^global_flip];
		INT32 code  =(DrvVidRAM[offs^global_flip] + ((attr & 0x60) << 3));
		INT32 color = attr & 0x1f;
		INT32 category, flipx, flipy;

		if (game_select > 2) {
			flipx = flipy = 0;
			category = attr >> 7;
		} else {
			flipx = attr & 0x40;
			flipy = attr & 0x80;
			category = (attr >> 4) & 1;
			code &= 0x1ff;
		}

		if (game_select == 2) {
			flipx = !flipx;
			flipy = !flipy;
		}

		if (priority != category) continue;

		if (flipy) {
			if (flipx) {
				Render8x8Tile_FlipXY(pTransDraw, code, sx, sy - 16, color, 2, 0, DrvGfxROM0);
			} else {
				Render8x8Tile_FlipY(pTransDraw, code, sx, sy - 16, color, 2, 0, DrvGfxROM0);
			}
		} else {
			if (flipx) {
				Render8x8Tile_FlipX(pTransDraw, code, sx, sy - 16, color, 2, 0, DrvGfxROM0);
			} else {
				Render8x8Tile(pTransDraw, code, sx, sy - 16, color, 2, 0, DrvGfxROM0);
			}
		}
	}
}

static void draw_sprites()
{
	for (INT32 line = 16; line < 240; line++)
	{
		UINT8 *ram = DrvSprTmp + (line * 0x60);
		UINT16 *dst = pTransDraw + ((line - 16) * nScreenWidth);

		for (INT32 offs = 0x2e; offs >= 0; offs -= 2)
		{
			INT32 sy    = 241 - ram[offs + 0x31];
			if (sy < (line-15) || sy > line) continue;

			INT32 sx    = ram[offs + 0x00];
			INT32 code  = ram[offs + 0x01];
			INT32 color = ram[offs + 0x30] & 0x3f;
			INT32 flipx =~ram[offs + 0x30] & 0x40;
			INT32 flipy = ram[offs + 0x30] & 0x80;

			{
				UINT8 *gfx = DrvGfxROM1 + (code * 0x100);
				color = (color << 2) + 0x80;
				flipx = (flipx ? 0x0f : 0);
				flipy = (flipy ? 0xf0 : 0);
				gfx += (((line - sy) * 0x10) ^ flipy);

				for (INT32 x = 0; x < 16; x++, sx++)
				{
					if (sx < 0 || sx >= nScreenWidth) continue;

					INT32 pxl = gfx[x^flipx];

					if (pxl) {
						dst[sx] = pxl + color;
					}
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

	if (nBurnLayer & 1) draw_layer(0);
	if (nSpriteEnable & 1) draw_sprites();
	if (nBurnLayer & 2) draw_layer(1);

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
	INT32 nCyclesTotal[2] = { 3072000 / 60, 1789772 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };
	INT32 scanline = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		CPU_RUN(0, Zet);
		if (i == (nInterleave - 1) && nmi_enable) ZetSetIRQLine(0x20, CPU_IRQSTATUS_ACK);
		if (i == (nInterleave - 1) && game_select == 2) ZetNmi();
		ZetClose();

		scanline = i + 5;
		if (scanline >= 16 && scanline < 240) {
			memcpy (DrvSprTmp + scanline * 0x60 + 0x00, DrvSprRAM + 0x010, 0x30);
			memcpy (DrvSprTmp + scanline * 0x60 + 0x30, DrvSprRAM + 0x110, 0x30);
		}

		ZetOpen(1);
		CPU_RUN(1, Zet);
		ZetClose();
	}

	if (pBurnSoundOut) {
		TimepltSndUpdate(pBurnSoundOut, nBurnSoundLen);
		//tc8830fUpdate(pBurnSoundOut, nBurnSoundLen);
		BurnSoundDCFilter();
	}

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction,INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029521;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);
		TimepltSndScan(nAction, pnMin);

		SCAN_VAR(nmi_enable);
		SCAN_VAR(last_sound_irq);
		SCAN_VAR(watchdog);
	}

	return 0;
}


// Time Pilot

static struct BurnRomInfo timepltRomDesc[] = {
	{ "tm1",			0x2000, 0x1551f1b9, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "tm2",			0x2000, 0x58636cb5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tm3",			0x2000, 0xff4e0d83, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "tm7",			0x1000, 0xd66da813, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "tm6",			0x2000, 0xc2507f40, 3 | BRF_GRA },           //  4 Characters

	{ "tm4",			0x2000, 0x7e437c3e, 4 | BRF_GRA },           //  5 Sprites
	{ "tm5",			0x2000, 0xe8ca87b9, 4 | BRF_GRA },           //  6

	{ "timeplt.b4",		0x0020, 0x34c91839, 5 | BRF_GRA },           //  7 Color PROMs
	{ "timeplt.b5",		0x0020, 0x463b2b07, 5 | BRF_GRA },           //  8
	{ "timeplt.e9",		0x0100, 0x4bbb2150, 5 | BRF_GRA },           //  9
	{ "timeplt.e12",	0x0100, 0xf7b7663e, 5 | BRF_GRA },           // 10
};

STD_ROM_PICK(timeplt)
STD_ROM_FN(timeplt)

static INT32 timepltInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvTimeplt = {
	"timeplt", NULL, NULL, NULL, "1982",
	"Time Pilot\0", NULL, "Konami", "GX393",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_VERSHOOT, 0,
	NULL, timepltRomInfo, timepltRomName, NULL, NULL, NULL, NULL, TimepltInputInfo, TimepltDIPInfo,
	timepltInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x180,
	224, 256, 3, 4
};


// Time Pilot (Atari)

static struct BurnRomInfo timepltaRomDesc[] = {
	{ "cd_e1.bin",		0x2000, 0xa4513b35, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "cd_e2.bin",		0x2000, 0x38b0c72a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "cd_e3.bin",		0x2000, 0x83846870, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "tm7",			0x1000, 0xd66da813, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "tm6",			0x2000, 0xc2507f40, 3 | BRF_GRA },           //  4 Characters

	{ "tm4",			0x2000, 0x7e437c3e, 4 | BRF_GRA },           //  5 Sprites
	{ "tm5",			0x2000, 0xe8ca87b9, 4 | BRF_GRA },           //  6

	{ "timeplt.b4",		0x0020, 0x34c91839, 5 | BRF_GRA },           //  7 Color PROMs
	{ "timeplt.b5",		0x0020, 0x463b2b07, 5 | BRF_GRA },           //  8
	{ "timeplt.e9",		0x0100, 0x4bbb2150, 5 | BRF_GRA },           //  9
	{ "timeplt.e12",	0x0100, 0xf7b7663e, 5 | BRF_GRA },           // 10
};

STD_ROM_PICK(timeplta)
STD_ROM_FN(timeplta)

struct BurnDriver BurnDrvTimeplta = {
	"timeplta", "timeplt", NULL, NULL, "1982",
	"Time Pilot (Atari)\0", NULL, "Konami (Atari license)", "GX393",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_VERSHOOT, 0,
	NULL, timepltaRomInfo, timepltaRomName, NULL, NULL, NULL, NULL, TimepltInputInfo, TimepltDIPInfo,
	timepltInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x180,
	224, 256, 3, 4
};


// Time Pilot (Centuri)

static struct BurnRomInfo timepltcRomDesc[] = {
	{ "cd1y",			0x2000, 0x83ec72c2, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "cd2y",			0x2000, 0x0dcf5287, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "cd3y",			0x2000, 0xc789b912, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "tm7",			0x1000, 0xd66da813, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "tm6",			0x2000, 0xc2507f40, 3 | BRF_GRA },           //  4 Characters

	{ "tm4",			0x2000, 0x7e437c3e, 4 | BRF_GRA },           //  5 Sprites
	{ "tm5",			0x2000, 0xe8ca87b9, 4 | BRF_GRA },           //  6

	{ "timeplt.b4",		0x0020, 0x34c91839, 5 | BRF_GRA },           //  7 Color PROMs
	{ "timeplt.b5",		0x0020, 0x463b2b07, 5 | BRF_GRA },           //  8
	{ "timeplt.e9",		0x0100, 0x4bbb2150, 5 | BRF_GRA },           //  9
	{ "timeplt.e12",	0x0100, 0xf7b7663e, 5 | BRF_GRA },           // 10
};

STD_ROM_PICK(timepltc)
STD_ROM_FN(timepltc)

struct BurnDriver BurnDrvTimepltc = {
	"timepltc", "timeplt", NULL, NULL, "1982",
	"Time Pilot (Centuri)\0", NULL, "Konami (Centuri license)", "GX393",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_VERSHOOT, 0,
	NULL, timepltcRomInfo, timepltcRomName, NULL, NULL, NULL, NULL, TimepltInputInfo, TimepltDIPInfo,
	timepltInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x180,
	224, 256, 3, 4
};


// Space Pilot (set 1)

static struct BurnRomInfo spacepltRomDesc[] = {
	{ "sp1",			0x2000, 0xac8ca3ae, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "sp2",			0x2000, 0x1f0308ef, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sp3",			0x2000, 0x90aeca50, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "tm7",			0x1000, 0xd66da813, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "sp6",			0x2000, 0x76caa8af, 3 | BRF_GRA },           //  4 Characters

	{ "sp4",			0x2000, 0x3781ce7a, 4 | BRF_GRA },           //  5 Sprites
	{ "tm5",			0x2000, 0xe8ca87b9, 4 | BRF_GRA },           //  6

	{ "timeplt.b4",		0x0020, 0x34c91839, 5 | BRF_GRA },           //  7 Color PROMs
	{ "timeplt.b5",		0x0020, 0x463b2b07, 5 | BRF_GRA },           //  8
	{ "timeplt.e9",		0x0100, 0x4bbb2150, 5 | BRF_GRA },           //  9
	{ "timeplt.e12",	0x0100, 0xf7b7663e, 5 | BRF_GRA },           // 10
};

STD_ROM_PICK(spaceplt)
STD_ROM_FN(spaceplt)

struct BurnDriver BurnDrvSpaceplt = {
	"spaceplt", "timeplt", NULL, NULL, "1982",
	"Space Pilot (set 1)\0", NULL, "bootleg", "GX393",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_VERSHOOT, 0,
	NULL, spacepltRomInfo, spacepltRomName, NULL, NULL, NULL, NULL, TimepltInputInfo, TimepltDIPInfo,
	timepltInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x180,
	224, 256, 3, 4
};


// Space Pilot (set 2)

static struct BurnRomInfo spacepltaRomDesc[] = {
	{ "1",				0x2000, 0xc6672087, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "2",				0x2000, 0x5b246c47, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3",				0x2000, 0xcc9e745e, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "4",				0x1000, 0xd66da813, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "5",				0x2000, 0x76caa8af, 3 | BRF_GRA },           //  4 Characters

	{ "6",				0x2000, 0x86ab1ae7, 4 | BRF_GRA },           //  5 Sprites
	{ "7",				0x2000, 0xe8ca87b9, 4 | BRF_GRA },           //  6

	{ "timeplt.b4",		0x0020, 0x34c91839, 5 | BRF_GRA },           //  7 Color PROMs
	{ "timeplt.b5",		0x0020, 0x463b2b07, 5 | BRF_GRA },           //  8
	{ "timeplt.e9",		0x0100, 0x4bbb2150, 5 | BRF_GRA },           //  9
	{ "timeplt.e12",	0x0100, 0xf7b7663e, 5 | BRF_GRA },           // 10
};

STD_ROM_PICK(spaceplta)
STD_ROM_FN(spaceplta)

struct BurnDriver BurnDrvSpaceplta = {
	"spaceplta", "timeplt", NULL, NULL, "1982",
	"Space Pilot (set 2)\0", NULL, "bootleg", "GX393",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_VERSHOOT, 0,
	NULL, spacepltaRomInfo, spacepltaRomName, NULL, NULL, NULL, NULL, TimepltInputInfo, TimepltDIPInfo,
	timepltInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x180,
	224, 256, 3, 4
};


// Power Surge

static struct BurnRomInfo psurgeRomDesc[] = {
	{ "p1",			0x2000, 0x05f9ba12, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "p2",			0x2000, 0x3ff41576, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "p3",			0x2000, 0xe8fe120a, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "p6",			0x1000, 0xb52d01fa, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code
	{ "p7",			0x1000, 0x9db5c0ce, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "p4",			0x2000, 0x26fd7f81, 3 | BRF_GRA },           //  5 Characters

	{ "p5",			0x2000, 0x6066ec8e, 4 | BRF_GRA },           //  6 Sprites
	{ "tm5",		0x2000, 0xe8ca87b9, 4 | BRF_GRA },           //  7

	{ "timeplt.b4",		0x0020, 0x34c91839, 5 | BRF_GRA },           //  8 Color PROMs
	{ "timeplt.b5",		0x0020, 0x463b2b07, 5 | BRF_GRA },           //  9
	{ "timeplt.e9",		0x0100, 0x4bbb2150, 5 | BRF_GRA },           // 10
	{ "timeplt.e12",	0x0100, 0xf7b7663e, 5 | BRF_GRA },           // 11
};

STD_ROM_PICK(psurge)
STD_ROM_FN(psurge)

static INT32 psurgeInit()
{
	return DrvInit(2);
}

struct BurnDriver BurnDrvPsurge = {
	"psurge", NULL, NULL, NULL, "1988",
	"Power Surge\0", NULL, "Vision Electronics", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_MAZE, 0,
	NULL, psurgeRomInfo, psurgeRomName, NULL, NULL, NULL, NULL, TimepltInputInfo, PsurgeDIPInfo,
	psurgeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x180,
	224, 256, 3, 4
};


// Chance Kun (Japan)

static struct BurnRomInfo chkunRomDesc[] = {
	{ "n1.16a",		0x04000, 0xc5879f9b, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "12.14a",		0x02000, 0x80cc55da, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "15.3l",		0x02000, 0x1f1463ca, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "13.4d",		0x04000, 0x776427c0, 3 | BRF_GRA },           //  3 Characters

	{ "14.8h",		0x04000, 0x0cb76a48, 4 | BRF_GRA },           //  4 Sprites

	{ "3.2j",		0x00020, 0x34c91839, 5 | BRF_GRA },           //  5 Color PROMs
	{ "2.1h",		0x00020, 0x463b2b07, 5 | BRF_GRA },           //  6
	{ "4.10h",		0x00100, 0x4bbb2150, 5 | BRF_GRA },           //  7
	{ "mb7114e.2b",		0x00100, 0xadfa399a, 5 | BRF_GRA },           //  8

	{ "v1.8k",		0x10000, 0xd5ca802d, 6 | BRF_GRA },           //  9 tc8830f Samples
	{ "v2.9k",		0x10000, 0x70e902eb, 6 | BRF_GRA },           // 10

	{ "a.16c",		0x000eb, 0xe0d54999, 7 | BRF_GRA },           // 11 PLDs
	{ "b.9f",		0x000eb, 0xe3857f83, 7 | BRF_GRA },           // 12
};

STD_ROM_PICK(chkun)
STD_ROM_FN(chkun)

static INT32 chkunInit()
{
	return DrvInit(3);
}

struct BurnDriver BurnDrvChkun = {
	"chkun", NULL, NULL, NULL, "1988",
	"Chance Kun (Japan)\0", NULL, "Peni Soft", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_CASINO, 0,
	NULL, chkunRomInfo, chkunRomName, NULL, NULL, NULL, NULL, ChkunInputInfo, ChkunDIPInfo,
	chkunInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x180,
	224, 256, 3, 4
};


// Bikkuri Card (Japan)

static struct BurnRomInfo bikkuricRomDesc[] = {
	{ "1.a16",		0x4000, 0xe8d595ab, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "2.a14",		0x2000, 0x63fd7d53, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "5.l3",		0x2000, 0xbc438531, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "3.d4",		0x8000, 0x74e8a64b, 3 | BRF_GRA },           //  3 Characters

	{ "4.h8",		0x2000, 0xd303942d, 4 | BRF_GRA },           //  4 Sprites

	{ "3.2j",		0x0020, 0x34c91839, 5 | BRF_GRA },           //  5 Color PROMs
	{ "2.1h",		0x0020, 0x463b2b07, 5 | BRF_GRA },           //  6
	{ "4.10h",		0x0100, 0x4bbb2150, 5 | BRF_GRA },           //  7
	{ "1.2b",		0x0100, 0xf7b7663e, 5 | BRF_GRA },           //  8

	{ "a.16c",		0x00eb, 0x00000000, 6 | BRF_NODUMP },        //  9 PLDs
	{ "b.9f",		0x00eb, 0x00000000, 6 | BRF_NODUMP },        // 10
};

STD_ROM_PICK(bikkuric)
STD_ROM_FN(bikkuric)

static INT32 bikkuricInit()
{
	return DrvInit(4);
}

struct BurnDriver BurnDrvBikkuric = {
	"bikkuric", NULL, NULL, NULL, "1987",
	"Bikkuri Card (Japan)\0", NULL, "Peni Soft", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_CASINO, 0,
	NULL, bikkuricRomInfo, bikkuricRomName, NULL, NULL, NULL, NULL, BikkuricInputInfo, BikkuricDIPInfo,
	bikkuricInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x180,
	224, 256, 3, 4
};
