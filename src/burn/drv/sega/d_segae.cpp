// based on MESS/MAME driver by David Haywood

#include "tiles_generic.h"
#include "z80_intf.h"
#include "sn76496.h"
#include "bitswap.h"
#include "mc8123.h"
#include "burn_gun.h" // dial for megrescu / ridleofp

static UINT8 DrvJoy0[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvJoy1[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvJoy2[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvJoy3[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvJoy4[8] = {0, 0, 0, 0, 0, 0, 0, 0};

static UINT8 DrvInput[5];
static UINT8 DrvDip[2];
static UINT8 DrvReset;
static UINT8 DrvRecalc;

static INT16 DrvWheel = 0;
static INT16 DrvAccel = 0;
static INT16 Analog[2];

static INT32 nCyclesExtra;

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvRAM;
static UINT8 *DrvMainROM;
static UINT8 *DrvMainROMFetch;

static UINT8 *mc8123key;

static UINT32 *DrvPalette;
static UINT32 *Palette;
static UINT8 *cache_bitmap;

static UINT8 segae_8000bank;
static UINT8 port_fa_last;
static UINT8 rombank;

static UINT8 mc8123 = 0; // enabled?
static UINT8 mc8123_banked = 0; // enabled?

static UINT8 hintcount;			/* line interrupt counter, decreased each scanline */
static UINT8 vintpending;
static UINT8 hintpending;

//static UINT8 m_port_select;
static UINT8 currentLine = 0;

static UINT8 leftcolumnblank = 0; // most games need this, except tetris
static UINT8 leftcolumnblank_special = 0; // for fantzn2, move over the screen 8px
static UINT8 ridleofp = 0;
static UINT8 megrescu = 0;

#define CHIPS 2							/* There are 2 VDP Chips */

static UINT8  segae_vdp_cmdpart[CHIPS];		/* VDP Command Part Counter */
static UINT16 segae_vdp_command[CHIPS];		/* VDP Command Word */

static UINT8  segae_vdp_accessmode[CHIPS];		/* VDP Access Mode (VRAM, CRAM) */
static UINT16 segae_vdp_accessaddr[CHIPS];		/* VDP Access Address */
static UINT8  segae_vdp_readbuffer[CHIPS];		/* VDP Read Buffer */

static UINT8 *segae_vdp_vram[CHIPS];			/* Pointer to VRAM */
static UINT8 *segae_vdp_cram[CHIPS];			/* Pointer to the VDP's CRAM */
static UINT8 *segae_vdp_regs[CHIPS];			/* Pointer to the VDP's Registers */

static UINT8 segae_vdp_vrambank[CHIPS];		/* Current VRAM Bank number (from writes to Port 0xf7) */

static struct BurnInputInfo TransfrmInputList[] = {

	{"P1 Coin",		BIT_DIGITAL,	DrvJoy0 + 0,	"p1 coin"},
	{"P1 Start",	BIT_DIGITAL,	DrvJoy0 + 6,	"p1 start"},
	{"P2 Coin",		BIT_DIGITAL,	DrvJoy0 + 1,	"p2 coin"},
	{"P2 Start",	BIT_DIGITAL,	DrvJoy0 + 7,	"p2 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy0 + 3,	"service"},
	{"Service Mode",		BIT_DIGITAL,	DrvJoy0 + 2,	"diag"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDip + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDip + 1,	"dip"},
};
STDINPUTINFO(Transfrm)

static struct BurnInputInfo Segae2pInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy0 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy0 + 6,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy0 + 1,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy0 + 7,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy0 + 3,	"service"},
	{"Service Mode",		BIT_DIGITAL,	DrvJoy0 + 2,	"diag"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDip + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDip + 1,	"dip"},
};

STDINPUTINFO(Segae2p)

static struct BurnDIPInfo TransfrmDIPList[]=
{
	{0x0d, 0xff, 0xff, 0xff, NULL		},
	{0x0e, 0xff, 0xff, 0xfc, NULL		},

	{0   , 0xfe, 0   ,    2, "1 Player Only"		},
	{0x0e, 0x01, 0x01, 0x00, "Off"		},
	{0x0e, 0x01, 0x01, 0x01, "On"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x0e, 0x01, 0x02, 0x02, "Off"		},
	{0x0e, 0x01, 0x02, 0x00, "On"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0e, 0x01, 0x0c, 0x0c, "3"		},
	{0x0e, 0x01, 0x0c, 0x08, "4"		},
	{0x0e, 0x01, 0x0c, 0x04, "5"		},
	{0x0e, 0x01, 0x0c, 0x00, "Infinite (Cheat)"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x0e, 0x01, 0x30, 0x20, "10k, 30k, 50k and 70k"		},
	{0x0e, 0x01, 0x30, 0x30, "20k, 60k, 100k and 140k"		},
	{0x0e, 0x01, 0x30, 0x10, "30k, 80k, 130k and 180k"		},
	{0x0e, 0x01, 0x30, 0x00, "50k, 150k and 250k"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x0e, 0x01, 0xc0, 0x40, "Easy"		},
	{0x0e, 0x01, 0xc0, 0xc0, "Medium"		},
	{0x0e, 0x01, 0xc0, 0x80, "Hard"		},
	{0x0e, 0x01, 0xc0, 0x00, "Hardest"		},
};

STDDIPINFO(Transfrm)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo HangonjrInputList[] = {

    {"P1 Coin",     BIT_DIGITAL,    DrvJoy0 + 0,    "p1 coin"},
    {"P1 Start",    BIT_DIGITAL,    DrvJoy0 + 4,    "p1 start"},
	A("P1 Steering", BIT_ANALOG_REL, &DrvWheel,     "p1 x-axis"),
	A("P1 Accelerate", BIT_ANALOG_REL, &DrvAccel,   "p1 z-axis"),

	{"Reset",		BIT_DIGITAL,	&DrvReset,	    "reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy0 + 3,	"service"},
	{"Service Mode",		BIT_DIGITAL,	DrvJoy0 + 2,	"diag"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDip + 0,	    "dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDip + 1,	    "dip"},
};

STDINPUTINFO(Hangonjr)
#undef A


static struct BurnDIPInfo HangonjrDIPList[]=
{
	{0x07, 0xff, 0xff, 0xff, NULL		},
	{0x08, 0xff, 0xff, 0x14, NULL		},

	{0   , 0xfe, 0   ,    4, "Enemies"		},
	{0x08, 0x01, 0x06, 0x06, "Easy"		},
	{0x08, 0x01, 0x06, 0x04, "Medium"		},
	{0x08, 0x01, 0x06, 0x02, "Hard"		},
	{0x08, 0x01, 0x06, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Time Adj."		},
	{0x08, 0x01, 0x18, 0x18, "Easy"		},
	{0x08, 0x01, 0x18, 0x10, "Medium"		},
	{0x08, 0x01, 0x18, 0x08, "Hard"		},
	{0x08, 0x01, 0x18, 0x00, "Hardest"		},
};

STDDIPINFO(Hangonjr)

static struct BurnInputInfo TetrisseInputList[] = {

	{"P1 Coin",		BIT_DIGITAL,	DrvJoy0 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy0 + 6,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy0 + 3,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDip + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDip + 1,	"dip"},
};

STDINPUTINFO(Tetrisse)


static struct BurnDIPInfo TetrisseDIPList[]=
{
	{0x0a, 0xff, 0xff, 0xff, NULL		},
	{0x0b, 0xff, 0xff, 0x30, NULL		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x0b, 0x01, 0x02, 0x02, "Off"		},
	{0x0b, 0x01, 0x02, 0x00, "On"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x0b, 0x01, 0x30, 0x20, "Easy"		},
	{0x0b, 0x01, 0x30, 0x30, "Normal"		},
	{0x0b, 0x01, 0x30, 0x10, "Hard"		},
	{0x0b, 0x01, 0x30, 0x00, "Hardest"		},
};
STDDIPINFO(Tetrisse)

static struct BurnDIPInfo Fantzn2DIPList[]=
{
	{0x0d, 0xff, 0xff, 0xff, NULL		}, // coinage (missing for now)
	{0x0e, 0xff, 0xff, 0xfc, NULL		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x0e, 0x01, 0x02, 0x02, "Off"		},
	{0x0e, 0x01, 0x02, 0x00, "On"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0e, 0x01, 0x0c, 0x00, "2"		},
	{0x0e, 0x01, 0x0c, 0x0c, "3"		},
	{0x0e, 0x01, 0x0c, 0x08, "4"		},
	{0x0e, 0x01, 0x0c, 0x04, "5"		},

	{0   , 0xfe, 0   ,    4, "Timer"		},
	{0x0e, 0x01, 0x30, 0x20, "90"		},
	{0x0e, 0x01, 0x30, 0x30, "80"		},
	{0x0e, 0x01, 0x30, 0x10, "70"		},
	{0x0e, 0x01, 0x30, 0x00, "60"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x0e, 0x01, 0xc0, 0x80, "Easy"		},
	{0x0e, 0x01, 0xc0, 0xc0, "Normal"		},
	{0x0e, 0x01, 0xc0, 0x40, "Hard"		},
	{0x0e, 0x01, 0xc0, 0x00, "Hardest"		},
};

STDDIPINFO(Fantzn2)

static struct BurnDIPInfo OpaopaDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL		}, // coinage defs.
	{0x14, 0xff, 0xff, 0xff, NULL		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x14, 0x01, 0x02, 0x02, "Off"		},
	{0x14, 0x01, 0x02, 0x00, "On"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0x0c, 0x00, "2"		},
	{0x14, 0x01, 0x0c, 0x0c, "3"		},
	{0x14, 0x01, 0x0c, 0x08, "4"		},
	{0x14, 0x01, 0x0c, 0x04, "5"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x14, 0x01, 0x30, 0x20, "25k, 45k and 70k"		},
	{0x14, 0x01, 0x30, 0x30, "40k, 60k and 90k"		},
	{0x14, 0x01, 0x30, 0x10, "50k and 90k"		},
	{0x14, 0x01, 0x30, 0x00, "None"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0xc0, 0x80, "Easy"		},
	{0x14, 0x01, 0xc0, 0xc0, "Normal"		},
	{0x14, 0x01, 0xc0, 0x40, "Hard"		},
	{0x14, 0x01, 0xc0, 0x00, "Hardest"		},
};

STDDIPINFO(Opaopa)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo RidleofpInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy0 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy0 + 6,	"p1 start"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy4 + 0,	"p1 left"   },
	{"P1 Right",		BIT_DIGITAL,	DrvJoy4 + 1,	"p1 right"  },
	A("P1 Spinner",		BIT_ANALOG_REL, &Analog[0],		"p1 x-axis"),
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 fire 1"},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy0 + 3,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDip + 0,		"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDip + 1,		"dip"		},
};

STDINPUTINFO(Ridleofp)

static struct BurnDIPInfo RidleOfpDIPList[]=
{
	DIP_OFFSET(0x08)
	{0x00, 0xff, 0xff, 0xff, NULL				}, // coinage defs.
	{0x01, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x01, 0x01, 0x03, 0x03, "3"				},
	{0x01, 0x01, 0x03, 0x02, "4"				},
	{0x01, 0x01, 0x03, 0x01, "5"				},
	{0x01, 0x01, 0x03, 0x00, "100 (Cheat)"		},

	{0   , 0xfe, 0   ,    2, "Ball Speed"		},
	{0x01, 0x01, 0x08, 0x08, "Easy"				},
	{0x01, 0x01, 0x08, 0x00, "Difficult"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x01, 0x01, 0x60, 0x60, "50K 100K 200K 1M 2M 10M 20M 50M"	},
	{0x01, 0x01, 0x60, 0x40, "100K 200K 1M 2M 10M 20M 50M"		},
	{0x01, 0x01, 0x60, 0x20, "200K 1M 2M 10M 20M 50M"			},
	{0x01, 0x01, 0x60, 0x00, "None"				},
};

STDDIPINFO(RidleOfp)

static struct BurnInputInfo MegrescuInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy0 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy0 + 6,	"p1 start"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy4 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy4 + 1,	"p1 right"	},
	A("P1 Spinner",		BIT_ANALOG_REL, &Analog[0],		"p1 x-axis"),
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy0 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy0 + 7,	"p2 start"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy4 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy4 + 3,	"p2 right"	},
	A("P2 Spinner",		BIT_ANALOG_REL, &Analog[1],		"p2 x-axis"),
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy0 + 3,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDip + 0,		"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDip + 1,		"dip"		},
};

STDINPUTINFO(Megrescu)

static struct BurnDIPInfo MegrescuDIPList[]=
{
	DIP_OFFSET(0x0e)
	{0x00, 0xff, 0xff, 0xff, NULL				}, // coinage defs.
	{0x01, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x01, 0x01, 0x0c, 0x0c, "2"				},
	{0x01, 0x01, 0x0c, 0x08, "3"				},
	{0x01, 0x01, 0x0c, 0x04, "4"				},
	{0x01, 0x01, 0x0c, 0x00, "100 (Cheat)"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x01, 0x01, 0x10, 0x00, "Upright"			},
	{0x01, 0x01, 0x10, 0x10, "Coctail"			},
};

STDDIPINFO(Megrescu)


static UINT8 __fastcall systeme_main_read(UINT16 address)
{
	//bprintf(0, _T("systeme_main_read adr %X.\n"), address);

	return 0;
}

static void __fastcall systeme_main_write(UINT16 address, UINT8 data)
{
	if(address >= 0x8000 && address <= 0xbfff)
	{
		segae_vdp_vram [1-segae_8000bank][(address - 0x8000) + (0x4000-(segae_vdp_vrambank[1-segae_8000bank] * 0x4000))] = data;

		return;
	}
}

static UINT8 scale_accel(UINT32 PaddlePortnum) {
	UINT8 Temp = PaddlePortnum >> 4;
	if (Temp < 0x08) Temp = 0x00; // sometimes default for digital button -> analog is "1"
	if (Temp > 0x30) Temp = 0xff;

	return Temp;
}

static UINT8 __fastcall hangonjr_port_f8_read(UINT8 port)
{
	UINT8 temp = 0;

	if (port_fa_last == 0x08)
		temp = ProcessAnalog(DrvWheel, 0, 0, 0x20, 0xe0);

	if (port_fa_last == 0x09)
		temp = scale_accel(DrvAccel);

	return temp;
}

static void __fastcall hangonjr_port_fa_write(UINT8 data)
{
	/* Seems to write the same pattern again and again bits ---- xx-x used */
	port_fa_last = data;
}

// Paddle stuff for ridleofp
static UINT16 paddle_diff1;
static UINT16 paddle_diff2;
static UINT16 paddle_last1;
static UINT16 paddle_last2;

static UINT8 __fastcall ridleofp_port_f8_read(UINT8 port)
{
	switch (port_fa_last)
	{
		default:
		case 0: return paddle_diff1 & 0xff;
		case 1: return paddle_diff1 >> 8;
		case 2: return paddle_diff2 & 0xff;
		case 3: return paddle_diff2 >> 8;
	}
}

static void __fastcall ridleofp_port_fa_write(UINT8 data)
{
	/* 0x10 is written before reading the dial (hold counters?) */
	/* 0x03 is written after reading the dial (reset counters?) */
	//bprintf(0, _T("FA write: %x\n"), data);
	port_fa_last = (data & 0x0c) >> 2;

	if (data & 1)
	{
		INT32 curr = (BurnTrackballReadWord(0, 0) & 0xfff) | ((DrvInput[3]&3) ? 0xf000 : 0);
		paddle_diff1 = ((curr - paddle_last1) & 0x0fff) | (curr & 0xf000);
		paddle_last1 = curr;
	}
	if (data & 2)
	{
		INT32 curr = (BurnTrackballReadWord(0, 1) & 0xfff);
		paddle_diff2 = ((curr - paddle_last2) & 0x0fff) | (curr & 0xf000);
		paddle_last2 = curr;
	}
}

static void segae_bankswitch (void)
{
	UINT32 bankloc = 0x10000 + rombank * 0x4000;

	ZetMapArea(0x8000, 0xbfff, 0, DrvMainROM + bankloc); // read
	ZetMapArea(0x8000, 0xbfff, 2, DrvMainROM + bankloc); // read
	if (mc8123_banked) {
		ZetMapArea(0x8000, 0xbfff, 2, DrvMainROMFetch + bankloc, DrvMainROM + bankloc); // fetch ops(encrypted), opargs(unencrypted)
	}
}

static void __fastcall bank_write(UINT8 data)
{
	//bprintf(0, _T("bank write: %x\n"), data);
	segae_vdp_vrambank[0]	= (data & 0x80) >> 7; /* Back  Layer VDP (0) VRAM Bank */
	segae_vdp_vrambank[1]	= (data & 0x40) >> 6; /* Front Layer VDP (1) VRAM Bank */
	segae_8000bank			= (data & 0x20) >> 5; /* 0x8000 Write Select */
	rombank					=  data & 0x0f;		  /* Rom Banking */
	segae_bankswitch();
}

static void segae_vdp_setregister ( UINT8 chip, UINT16 cmd )
{
	UINT8 regnumber;
	UINT8 regdata;

	regnumber = (cmd & 0x0f00) >> 8;
	regdata   = (cmd & 0x00ff);
	if (regnumber < 11) {
		segae_vdp_regs[chip][regnumber] = regdata;
		if (regnumber == 1 && chip == 1) {
			if ((segae_vdp_regs[chip][0x1]&0x20) && vintpending) {
				ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
			} else {
				ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			}
		}
		if (regnumber == 0 && chip == 1) {
			if ((segae_vdp_regs[chip][0x0]&0x10) && hintpending) { // dink
				ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
			} else {
				ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			}
		}
	} else {
		/* Illegal, there aren't this many registers! */
	}
}

static void segae_vdp_processcmd ( UINT8 chip, UINT16 cmd )
{
	if ( (cmd & 0xf000) == 0x8000 ) { /*  1 0 0 0 - - - - - - - - - - - -  VDP Register Set */
		segae_vdp_setregister (chip, cmd);
	} else { /* Anything Else */
		segae_vdp_accessmode[chip] = (cmd & 0xc000) >> 14;
		segae_vdp_accessaddr[chip] = (cmd & 0x3fff);

		if ((segae_vdp_accessmode[chip]==0x03) && (segae_vdp_accessaddr[chip] > 0x1f) ) { /* Check Address is valid for CRAM */
			/* Illegal, CRAM isn't this large! */
			segae_vdp_accessaddr[chip] &= 0x1f;
		}

		if (segae_vdp_accessmode[chip] == 0x00) { /*  0 0 - - - - - - - - - - - - - -  VRAM Acess Mode (Special Read) */
			segae_vdp_readbuffer[chip] = segae_vdp_vram[chip][ segae_vdp_vrambank[chip]*0x4000 + segae_vdp_accessaddr[chip] ];
			segae_vdp_accessaddr[chip] += 1;
			segae_vdp_accessaddr[chip] &= 0x3fff;
		}
	}
}

static UINT8 segae_vdp_counter_r (UINT8 chip, UINT8 offset)
{
	UINT8 temp = 0;
	UINT16 sline;

	switch (offset)
	{
		case 0: /* port 0x7e, Vert Position (in scanlines) */
			sline = currentLine;
//			if (sline > 0xDA) sline -= 6;
//			temp = sline-1 ;
			if (sline > 0xDA) sline -= 5;
			temp = sline ;
			break;
		case 1: /* port 0x7f, Horz Position (in pixel clock cycles)  */
			/* unhandled for now */
			break;
	}
	return temp;
}

static UINT8 segae_vdp_data_r(UINT8 chip)
{
	UINT8 temp;

	segae_vdp_cmdpart[chip] = 0;

	temp = segae_vdp_readbuffer[chip];

	if (segae_vdp_accessmode[chip]==0x03) { /* CRAM Access */
		/* error CRAM can't be read!! */
	} else { /* VRAM */
		segae_vdp_readbuffer[chip] = segae_vdp_vram[chip][ segae_vdp_vrambank[chip]*0x4000 + segae_vdp_accessaddr[chip] ];
		segae_vdp_accessaddr[chip] += 1;
		segae_vdp_accessaddr[chip] &= 0x3fff;
	}
	return temp;
}

static UINT8 segae_vdp_reg_r ( UINT8 chip )
{
	UINT8 temp;

	if (chip == 0) return 0; // slave vdp (chip==0) doesn't get to clear hint/vint status.

	temp = 0;

	temp |= (vintpending << 7);
	temp |= (hintpending << 6);

	ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);

	hintpending = vintpending = 0;

	return temp;
}

static inline UINT8 pal2bit(UINT8 bits)
{
	bits &= 3;
	return (bits << 6) | (bits << 4) | (bits << 2) | bits;
}

static void segae_vdp_data_w ( UINT8 chip, UINT8 data )
{
	segae_vdp_cmdpart[chip] = 0;

	if (segae_vdp_accessmode[chip]==0x03) { /* CRAM Access */
		UINT8 r,g,b, temp;

		temp = segae_vdp_cram[chip][segae_vdp_accessaddr[chip]];

		segae_vdp_cram[chip][segae_vdp_accessaddr[chip]] = data;

		if (temp != data) 
		{

			r = (segae_vdp_cram[chip][segae_vdp_accessaddr[chip]] & 0x03) >> 0;
			g = (segae_vdp_cram[chip][segae_vdp_accessaddr[chip]] & 0x0c) >> 2;
			b = (segae_vdp_cram[chip][segae_vdp_accessaddr[chip]] & 0x30) >> 4;

			Palette[segae_vdp_accessaddr[chip] + 32*chip] = pal2bit(r) << 16 | pal2bit(g) << 8 | pal2bit(b);  //BurnHighCol(pal2bit(r), pal2bit(g), pal2bit(b), 0);
			DrvPalette[segae_vdp_accessaddr[chip] + 32*chip] = BurnHighCol(pal2bit(r), pal2bit(g), pal2bit(b), 0);
		}

		segae_vdp_accessaddr[chip] += 1;
		segae_vdp_accessaddr[chip] &= 0x1f;
	} else if (segae_vdp_accessmode[chip]==0x01) { /* VRAM Accesses */
		segae_vdp_vram[chip][ segae_vdp_vrambank[chip]*0x4000 + segae_vdp_accessaddr[chip] ] = data;
		segae_vdp_accessaddr[chip] += 1;
		segae_vdp_accessaddr[chip] &= 0x3fff;
	}
}

static void segae_vdp_reg_w ( UINT8 chip, UINT8 data )
{
	if (!segae_vdp_cmdpart[chip]) {
		segae_vdp_cmdpart[chip] = 1;
		segae_vdp_command[chip] = data;
	} else {
		segae_vdp_cmdpart[chip] = 0;
		segae_vdp_command[chip] |= (data << 8);
		segae_vdp_processcmd (chip, segae_vdp_command[chip]);
	}
}

static UINT8 __fastcall systeme_main_in(UINT16 port)
{
	port &= 0xff;

	switch (port) {
		case 0x7e: return segae_vdp_counter_r(0, 0);
		case 0x7f: return segae_vdp_counter_r(0, 1);
		case 0xba: return segae_vdp_data_r(0);
		case 0xbb: return segae_vdp_reg_r(0);

		case 0xbe: return segae_vdp_data_r(1);
		case 0xbf: return segae_vdp_reg_r(1);

		case 0xe0: return DrvInput[0];
		case 0xe1: return DrvInput[1];
		case 0xe2: return DrvInput[2];
		case 0xf2: return DrvDip[0];
		case 0xf3: return DrvDip[1];
		case 0xf8: return (ridleofp) ? ridleofp_port_f8_read(port) : hangonjr_port_f8_read(port);
	}	
	//bprintf(PRINT_NORMAL, _T("IO Read %x\n"), port);
	return 0;
}

static void __fastcall systeme_main_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x7b: SN76496Write(0, data);
		return;

		case 0x7f: SN76496Write(1, data);
		return;

		case 0xba: segae_vdp_data_w(0, data);
		return;

		case 0xbb: segae_vdp_reg_w(0, data);
		return;

		case 0xbe: segae_vdp_data_w(1, data);
		return;

		case 0xbf:	segae_vdp_reg_w(1, data);
		return;

		case 0xf7:	bank_write(data);
		return;

		case 0xfa:	if (ridleofp) { ridleofp_port_fa_write(data); } else { hangonjr_port_fa_write(data); }
		return;
	}
	//bprintf(PRINT_NORMAL, _T("IO Write %x %X\n"), port, data);
}

static void sega_decode_2(UINT8 *pDest, UINT8 *pDestDec, const UINT8 opcode_xor[64],const INT32 opcode_swap_select[64],
		const UINT8 data_xor[64],const INT32 data_swap_select[64])
{
	INT32 A;
	static const UINT8 swaptable[24][4] =
	{
		{ 6,4,2,0 }, { 4,6,2,0 }, { 2,4,6,0 }, { 0,4,2,6 },
		{ 6,2,4,0 }, { 6,0,2,4 }, { 6,4,0,2 }, { 2,6,4,0 },
		{ 4,2,6,0 }, { 4,6,0,2 }, { 6,0,4,2 }, { 0,6,4,2 },
		{ 4,0,6,2 }, { 0,4,6,2 }, { 6,2,0,4 }, { 2,6,0,4 },
		{ 0,6,2,4 }, { 2,0,6,4 }, { 0,2,6,4 }, { 4,2,0,6 },
		{ 2,4,0,6 }, { 4,0,2,6 }, { 2,0,4,6 }, { 0,2,4,6 },
	};


	UINT8 *rom = pDest;
	UINT8 *decrypted = pDestDec;

	for (A = 0x0000;A < 0x8000;A++)
	{
		INT32 row;
		UINT8 src;
		const UINT8 *tbl;


		src = rom[A];

		/* pick the translation table from bits 0, 3, 6, 9, 12 and 14 of the address */
		row = (A & 1) + (((A >> 3) & 1) << 1) + (((A >> 6) & 1) << 2)
				+ (((A >> 9) & 1) << 3) + (((A >> 12) & 1) << 4) + (((A >> 14) & 1) << 5);

		/* decode the opcodes */
		tbl = swaptable[opcode_swap_select[row]];
		decrypted[A] = BITSWAP08(src,7,tbl[0],5,tbl[1],3,tbl[2],1,tbl[3]) ^ opcode_xor[row];

		/* decode the data */
		tbl = swaptable[data_swap_select[row]];
		rom[A] = BITSWAP08(src,7,tbl[0],5,tbl[1],3,tbl[2],1,tbl[3]) ^ data_xor[row];
	}
	
	memcpy(pDestDec + 0x8000, pDest + 0x8000, 0x4000);
}

static void astrofl_decode(void)
{
	static const UINT8 opcode_xor[64] =
	{
		0x04,0x51,0x40,0x01,0x55,0x44,0x05,0x50,0x41,0x00,0x54,0x45,
		0x04,0x51,0x40,0x01,0x55,0x44,0x05,0x50,0x41,0x00,0x54,0x45,
		0x04,0x51,0x40,0x01,0x55,0x44,0x05,0x50,
		0x04,0x51,0x40,0x01,0x55,0x44,0x05,0x50,0x41,0x00,0x54,0x45,
		0x04,0x51,0x40,0x01,0x55,0x44,0x05,0x50,0x41,0x00,0x54,0x45,
		0x04,0x51,0x40,0x01,0x55,0x44,0x05,0x50,
	};

	static const UINT8 data_xor[64] =
	{
		0x54,0x15,0x44,0x51,0x10,0x41,0x55,0x14,0x45,0x50,0x11,0x40,
		0x54,0x15,0x44,0x51,0x10,0x41,0x55,0x14,0x45,0x50,0x11,0x40,
		0x54,0x15,0x44,0x51,0x10,0x41,0x55,0x14,
		0x54,0x15,0x44,0x51,0x10,0x41,0x55,0x14,0x45,0x50,0x11,0x40,
		0x54,0x15,0x44,0x51,0x10,0x41,0x55,0x14,0x45,0x50,0x11,0x40,
		0x54,0x15,0x44,0x51,0x10,0x41,0x55,0x14,
	};

	static const INT32 opcode_swap_select[64] =
	{
		0,0,1,1,1,2,2,3,3,4,4,4,5,5,6,6,
		6,7,7,8,8,9,9,9,10,10,11,11,11,12,12,13,

		8,8,9,9,9,10,10,11,11,12,12,12,13,13,14,14,
		14,15,15,16,16,17,17,17,18,18,19,19,19,20,20,21,
	};

	static const INT32 data_swap_select[64] =
	{
		0,0,1,1,2,2,2,3,3,4,4,5,5,5,6,6,
		7,7,7,8,8,9,9,10,10,10,11,11,12,12,12,13,

		8,8,9,9,10,10,10,11,11,12,12,13,13,13,14,14,
		15,15,15,16,16,17,17,18,18,18,19,19,20,20,20,21,
	};
	sega_decode_2(DrvMainROM, DrvMainROMFetch, opcode_xor,opcode_swap_select,data_xor,data_swap_select);
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvMainROM	 	    = Next; Next += 0x80000;
	DrvMainROMFetch     = Next; Next += 0x80000;

	mc8123key           = Next; Next += 0x02000;

	AllRam				= Next;
	DrvRAM			    = Next; Next += 0x10000;

	segae_vdp_vram[0]	= Next; Next += 0x8000; /* 32kb (2 banks) */
	segae_vdp_vram[1]	= Next; Next += 0x8000; /* 32kb (2 banks) */

	segae_vdp_cram[0]	= Next; Next += 0x20;
	segae_vdp_regs[0]	= Next; Next += 0x20;

	segae_vdp_cram[1]	= Next; Next += 0x20;
	segae_vdp_regs[1]	= Next; Next += 0x20;

	cache_bitmap		= Next; Next += ( (16+256+16) * 192+17 ) + 0x0f; /* 16 pixels either side to simplify drawing + 0xf for alignment */

	DrvPalette			= (UINT32*)Next; Next += 0x040 * sizeof(UINT32);
	Palette			    = (UINT32*)Next; Next += 0x040 * sizeof(UINT32);

	RamEnd			= Next;
	MemEnd			= Next;

	return 0;
}

static INT32 DrvExit()
{
	ZetExit();
	GenericTilesExit();
	SN76496Exit();
	BurnFreeMemIndex();

	if (ridleofp) { // and megrescu
		BurnTrackballExit();
	}

	leftcolumnblank = 0;
	leftcolumnblank_special = 0;
	mc8123 = 0;
	mc8123_banked = 0;

	ridleofp = 0;
	megrescu = 0;

	return 0;
}

static INT32 DrvDoReset()
{
	memset (DrvRAM, 0, RamEnd - DrvRAM);

	rombank = 0;
	hintcount = 0;
	vintpending = 0;
	hintpending = 0;

	SN76496Reset();
	ZetOpen(0);
	segae_bankswitch();
	ZetReset();
	ZetClose();

	nCyclesExtra = 0;

	HiscoreReset();

	return 0;
}

static inline void DrvClearOpposites(UINT8* nJoystickInputs)
{
	if ((*nJoystickInputs & 0x03) == 0x00) {
		*nJoystickInputs |= 0x03;
	}
	if ((*nJoystickInputs & 0x0c) == 0x0c) {
		*nJoystickInputs |= 0x0c;
	}
}

static inline void DrvMakeInputs()
{
	// Reset Inputs
	DrvInput[0] = DrvInput[1] = DrvInput[2] = 0xff; // active low
	DrvInput[3] = DrvInput[4] = 0x00;

	// Compile Digital Inputs
	for (INT32 i = 0; i < 8; i++) {
		DrvInput[0] ^= (DrvJoy0[i] & 1) << i;
		DrvInput[1] ^= (DrvJoy1[i] & 1) << i;
		DrvInput[2] ^= (DrvJoy2[i] & 1) << i;
		DrvInput[3] ^= (DrvJoy3[i] & 1) << i;
		DrvInput[4] ^= (DrvJoy4[i] & 1) << i;
	}

	if (ridleofp) { // paddle
		BurnTrackballConfig(0, AXIS_NORMAL, AXIS_NORMAL);
		BurnTrackballFrame(0, Analog[0]*2, Analog[1]*2, 0x00, 0x3f);
		BurnTrackballUDLR(0, DrvJoy4[2], DrvJoy4[3], DrvJoy4[0], DrvJoy4[1]);
		BurnTrackballUpdate(0);
	}

	// Clear Opposites
	DrvClearOpposites(&DrvInput[0]);
	DrvClearOpposites(&DrvInput[1]);
}

static void segae_draw8pix_solid16(UINT8 *dest, UINT8 chip, UINT16 tile, UINT8 line, UINT8 flipx, UINT8 col)
{

	UINT32 pix8 = BURN_ENDIAN_SWAP_INT32(*(UINT32 *)&segae_vdp_vram[chip][(32)*tile + (4)*line + (0x4000) * segae_vdp_vrambank[chip]]);
	UINT8  pix, coladd;

	if (!pix8 && !col) return; /*note only the colour 0 of each vdp is transparent NOT colour 16???, fixes sky in HangonJr */

	coladd = 16*col;

	if (flipx)	{
		pix = ((pix8 >> 0) & 0x01) | ((pix8 >> 7) & 0x02) | ((pix8 >> 14) & 0x04) | ((pix8 >> 21) & 0x08) ; pix+= coladd ; if (pix) dest[0] = pix+ 32*chip;
		pix = ((pix8 >> 1) & 0x01) | ((pix8 >> 8) & 0x02) | ((pix8 >> 15) & 0x04) | ((pix8 >> 22) & 0x08) ; pix+= coladd ; if (pix) dest[1] = pix+ 32*chip;
		pix = ((pix8 >> 2) & 0x01) | ((pix8 >> 9) & 0x02) | ((pix8 >> 16) & 0x04) | ((pix8 >> 23) & 0x08) ; pix+= coladd ; if (pix) dest[2] = pix+ 32*chip;
		pix = ((pix8 >> 3) & 0x01) | ((pix8 >>10) & 0x02) | ((pix8 >> 17) & 0x04) | ((pix8 >> 24) & 0x08) ; pix+= coladd ; if (pix) dest[3] = pix+ 32*chip;
		pix = ((pix8 >> 4) & 0x01) | ((pix8 >>11) & 0x02) | ((pix8 >> 18) & 0x04) | ((pix8 >> 25) & 0x08) ; pix+= coladd ; if (pix) dest[4] = pix+ 32*chip;
		pix = ((pix8 >> 5) & 0x01) | ((pix8 >>12) & 0x02) | ((pix8 >> 19) & 0x04) | ((pix8 >> 26) & 0x08) ; pix+= coladd ; if (pix) dest[5] = pix+ 32*chip;
		pix = ((pix8 >> 6) & 0x01) | ((pix8 >>13) & 0x02) | ((pix8 >> 20) & 0x04) | ((pix8 >> 27) & 0x08) ; pix+= coladd ; if (pix) dest[6] = pix+ 32*chip;
		pix = ((pix8 >> 7) & 0x01) | ((pix8 >>14) & 0x02) | ((pix8 >> 21) & 0x04) | ((pix8 >> 28) & 0x08) ; pix+= coladd ; if (pix) dest[7] = pix+ 32*chip;
	} else {
		pix = ((pix8 >> 7) & 0x01) | ((pix8 >>14) & 0x02) | ((pix8 >> 21) & 0x04) | ((pix8 >> 28) & 0x08) ; pix+= coladd ; if (pix) dest[0] = pix+ 32*chip;
		pix = ((pix8 >> 6) & 0x01) | ((pix8 >>13) & 0x02) | ((pix8 >> 20) & 0x04) | ((pix8 >> 27) & 0x08) ; pix+= coladd ; if (pix) dest[1] = pix+ 32*chip;
		pix = ((pix8 >> 5) & 0x01) | ((pix8 >>12) & 0x02) | ((pix8 >> 19) & 0x04) | ((pix8 >> 26) & 0x08) ; pix+= coladd ; if (pix) dest[2] = pix+ 32*chip;
		pix = ((pix8 >> 4) & 0x01) | ((pix8 >>11) & 0x02) | ((pix8 >> 18) & 0x04) | ((pix8 >> 25) & 0x08) ; pix+= coladd ; if (pix) dest[3] = pix+ 32*chip;
		pix = ((pix8 >> 3) & 0x01) | ((pix8 >>10) & 0x02) | ((pix8 >> 17) & 0x04) | ((pix8 >> 24) & 0x08) ; pix+= coladd ; if (pix) dest[4] = pix+ 32*chip;
		pix = ((pix8 >> 2) & 0x01) | ((pix8 >> 9) & 0x02) | ((pix8 >> 16) & 0x04) | ((pix8 >> 23) & 0x08) ; pix+= coladd ; if (pix) dest[5] = pix+ 32*chip;
		pix = ((pix8 >> 1) & 0x01) | ((pix8 >> 8) & 0x02) | ((pix8 >> 15) & 0x04) | ((pix8 >> 22) & 0x08) ; pix+= coladd ; if (pix) dest[6] = pix+ 32*chip;
		pix = ((pix8 >> 0) & 0x01) | ((pix8 >> 7) & 0x02) | ((pix8 >> 14) & 0x04) | ((pix8 >> 21) & 0x08) ; pix+= coladd ; if (pix) dest[7] = pix+ 32*chip;
	}
}

static void segae_draw8pix(UINT8 *dest, UINT8 chip, UINT16 tile, UINT8 line, UINT8 flipx, UINT8 col)
{

	UINT32 pix8 = BURN_ENDIAN_SWAP_INT32(*(UINT32 *)&segae_vdp_vram[chip][(32)*tile + (4)*line + (0x4000) * segae_vdp_vrambank[chip]]);
	UINT8  pix, coladd;

	if (!pix8) return;

	coladd = 16*col+32*chip;

	if (flipx)	{
		pix = ((pix8 >> 0) & 0x01) | ((pix8 >> 7) & 0x02) | ((pix8 >> 14) & 0x04) | ((pix8 >> 21) & 0x08) ; if (pix) dest[0] = pix+ coladd;
		pix = ((pix8 >> 1) & 0x01) | ((pix8 >> 8) & 0x02) | ((pix8 >> 15) & 0x04) | ((pix8 >> 22) & 0x08) ; if (pix) dest[1] = pix+ coladd;
		pix = ((pix8 >> 2) & 0x01) | ((pix8 >> 9) & 0x02) | ((pix8 >> 16) & 0x04) | ((pix8 >> 23) & 0x08) ; if (pix) dest[2] = pix+ coladd;
		pix = ((pix8 >> 3) & 0x01) | ((pix8 >>10) & 0x02) | ((pix8 >> 17) & 0x04) | ((pix8 >> 24) & 0x08) ; if (pix) dest[3] = pix+ coladd;
		pix = ((pix8 >> 4) & 0x01) | ((pix8 >>11) & 0x02) | ((pix8 >> 18) & 0x04) | ((pix8 >> 25) & 0x08) ; if (pix) dest[4] = pix+ coladd;
		pix = ((pix8 >> 5) & 0x01) | ((pix8 >>12) & 0x02) | ((pix8 >> 19) & 0x04) | ((pix8 >> 26) & 0x08) ; if (pix) dest[5] = pix+ coladd;
		pix = ((pix8 >> 6) & 0x01) | ((pix8 >>13) & 0x02) | ((pix8 >> 20) & 0x04) | ((pix8 >> 27) & 0x08) ; if (pix) dest[6] = pix+ coladd;
		pix = ((pix8 >> 7) & 0x01) | ((pix8 >>14) & 0x02) | ((pix8 >> 21) & 0x04) | ((pix8 >> 28) & 0x08) ; if (pix) dest[7] = pix+ coladd;
	} else {
		pix = ((pix8 >> 7) & 0x01) | ((pix8 >>14) & 0x02) | ((pix8 >> 21) & 0x04) | ((pix8 >> 28) & 0x08) ; if (pix) dest[0] = pix+ coladd;
		pix = ((pix8 >> 6) & 0x01) | ((pix8 >>13) & 0x02) | ((pix8 >> 20) & 0x04) | ((pix8 >> 27) & 0x08) ; if (pix) dest[1] = pix+ coladd;
		pix = ((pix8 >> 5) & 0x01) | ((pix8 >>12) & 0x02) | ((pix8 >> 19) & 0x04) | ((pix8 >> 26) & 0x08) ; if (pix) dest[2] = pix+ coladd;
		pix = ((pix8 >> 4) & 0x01) | ((pix8 >>11) & 0x02) | ((pix8 >> 18) & 0x04) | ((pix8 >> 25) & 0x08) ; if (pix) dest[3] = pix+ coladd;
		pix = ((pix8 >> 3) & 0x01) | ((pix8 >>10) & 0x02) | ((pix8 >> 17) & 0x04) | ((pix8 >> 24) & 0x08) ; if (pix) dest[4] = pix+ coladd;
		pix = ((pix8 >> 2) & 0x01) | ((pix8 >> 9) & 0x02) | ((pix8 >> 16) & 0x04) | ((pix8 >> 23) & 0x08) ; if (pix) dest[5] = pix+ coladd;
		pix = ((pix8 >> 1) & 0x01) | ((pix8 >> 8) & 0x02) | ((pix8 >> 15) & 0x04) | ((pix8 >> 22) & 0x08) ; if (pix) dest[6] = pix+ coladd;
		pix = ((pix8 >> 0) & 0x01) | ((pix8 >> 7) & 0x02) | ((pix8 >> 14) & 0x04) | ((pix8 >> 21) & 0x08) ; if (pix) dest[7] = pix+ coladd;
	}
}

static void segae_draw8pixsprite(UINT8 *dest, UINT8 chip, UINT16 tile, UINT8 line)
{
	UINT32 pix8 = BURN_ENDIAN_SWAP_INT32(*(UINT32 *)&segae_vdp_vram[chip][(((32)*tile + (4)*line)&0x3fff) + (0x4000) * segae_vdp_vrambank[chip]]);
	UINT8  pix;

	if (!pix8) return; /*note only the colour 0 of each vdp is transparent NOT colour 16, fixes sky in HangonJr */

	pix = ((pix8 >> 7) & 0x01) | ((pix8 >>14) & 0x02) | ((pix8 >> 21) & 0x04) | ((pix8 >> 28) & 0x08) ; if (pix) dest[0] = pix+16+32*chip;
	pix = ((pix8 >> 6) & 0x01) | ((pix8 >>13) & 0x02) | ((pix8 >> 20) & 0x04) | ((pix8 >> 27) & 0x08) ; if (pix) dest[1] = pix+16+32*chip;
	pix = ((pix8 >> 5) & 0x01) | ((pix8 >>12) & 0x02) | ((pix8 >> 19) & 0x04) | ((pix8 >> 26) & 0x08) ; if (pix) dest[2] = pix+16+32*chip;
	pix = ((pix8 >> 4) & 0x01) | ((pix8 >>11) & 0x02) | ((pix8 >> 18) & 0x04) | ((pix8 >> 25) & 0x08) ; if (pix) dest[3] = pix+16+32*chip;
	pix = ((pix8 >> 3) & 0x01) | ((pix8 >>10) & 0x02) | ((pix8 >> 17) & 0x04) | ((pix8 >> 24) & 0x08) ; if (pix) dest[4] = pix+16+32*chip;
	pix = ((pix8 >> 2) & 0x01) | ((pix8 >> 9) & 0x02) | ((pix8 >> 16) & 0x04) | ((pix8 >> 23) & 0x08) ; if (pix) dest[5] = pix+16+32*chip;
	pix = ((pix8 >> 1) & 0x01) | ((pix8 >> 8) & 0x02) | ((pix8 >> 15) & 0x04) | ((pix8 >> 22) & 0x08) ; if (pix) dest[6] = pix+16+32*chip;
	pix = ((pix8 >> 0) & 0x01) | ((pix8 >> 7) & 0x02) | ((pix8 >> 14) & 0x04) | ((pix8 >> 21) & 0x08) ; if (pix) dest[7] = pix+16+32*chip;

}

static void segae_drawspriteline(UINT8 *dest, UINT8 chip, UINT8 line)
{
	/* todo: figure out what riddle of pythagoras hates about this */

	int nosprites;
	int loopcount;

	UINT16 spritebase;

	nosprites = 63;
	if (segae_vdp_regs[chip][1] & 0x1) {
		bprintf(0, _T("double-size spr. not supported. "));
		return;
	}

	spritebase =  (segae_vdp_regs[chip][5] & 0x7e) << 7;
	spritebase += (segae_vdp_vrambank[chip] * 0x4000);

	/*- find out how many sprites there are -*/

	for (loopcount=0;loopcount<64;loopcount++) {
		UINT8 ypos;

		ypos = segae_vdp_vram[chip][spritebase+loopcount];

		if (ypos==208) {
			nosprites=loopcount;
			break;
		}
	}

	/*- draw sprites IN REVERSE ORDER -*/

	for (loopcount = nosprites; loopcount >= 0;loopcount--) {
		int ypos;
		UINT8 sheight;

		ypos = segae_vdp_vram[chip][spritebase+loopcount] +1;

		if (segae_vdp_regs[chip][1] & 0x02) sheight=16; else sheight=8;

		if ( (line >= ypos) && (line < ypos+sheight) ) {
			int xpos;
			UINT16 sprnum;
			UINT8 spline;

			spline = line - ypos;

			xpos   = segae_vdp_vram[chip][spritebase+0x80+ (2*loopcount)];
			sprnum = segae_vdp_vram[chip][spritebase+0x81+ (2*loopcount)];

			if (segae_vdp_regs[chip][6] & 0x04)
				sprnum |= 0x100;
			if (segae_vdp_regs[chip][1] & 0x02)
				sprnum &= 0x01FE;

			segae_draw8pixsprite(dest+xpos, chip, sprnum, spline);

		}
	}
}

static void segae_drawtilesline(UINT8 *dest, int line, UINT8 chip, UINT8 pri)
{
	UINT8 hscroll;
	UINT8 vscroll, vscroll_line;
	UINT16 tmbase;
	UINT8 tilesline, tilesline2;
	UINT8 coloffset, coloffset2;
	UINT8 loopcount;

	hscroll = (256-segae_vdp_regs[chip][8]);
	vscroll = segae_vdp_regs[chip][9];
	vscroll_line = (line + vscroll) % 224;

	tmbase =  (segae_vdp_regs[chip][2] & 0x0e) << 10;
	//tmbase =  ((segae_vdp_regs[chip][2]) << 10) & 0x3800; // if other vdp problems, try this instead
	tmbase += (segae_vdp_vrambank[chip] * 0x4000);

	tilesline = vscroll_line >> 3;
	tilesline2= vscroll_line % 8;


	coloffset = (hscroll >> 3);
	coloffset2= (hscroll % 8);

	dest -= coloffset2;

	for (loopcount=0;loopcount<33;loopcount++) {

		UINT16 vram_offset, vram_word;
		UINT16 tile_no;
		UINT8  palette, priority, flipx, flipy;

		vram_offset = tmbase
					+ (2 * (32*tilesline + ((coloffset+loopcount)&0x1f) ) );
		vram_word = segae_vdp_vram[chip][vram_offset] | (segae_vdp_vram[chip][vram_offset+1] << 8);

		tile_no = (vram_word & 0x01ff);
		flipx =   (vram_word & 0x0200) >> 9;
		flipy =   (vram_word & 0x0400) >> 10;
		palette = (vram_word & 0x0800) >> 11;
		priority= (vram_word & 0x1000) >> 12;

		tilesline2= vscroll_line % 8;
		if (flipy) tilesline2 = 7-tilesline2;

		if (priority == pri) {
			if (chip == 0) segae_draw8pix_solid16(dest, chip, tile_no,tilesline2,flipx,palette);
			else segae_draw8pix(dest, chip, tile_no,tilesline2,flipx,palette);
		}
		dest+=8;
	}
}

static void segae_drawscanline(int line)
{
	UINT8 *dest = cache_bitmap + (16+256+16) * line;
	UINT32 offset = (leftcolumnblank_special) ? 8 : 16;

	/* This should be cleared to bg colour, but which vdp determines that !, neither seems to be right, maybe its always the same col? */
	memset(dest, 0, 16+256+16);

	if (segae_vdp_regs[0][1] & 0x40 && nBurnLayer & 1)
	{
		segae_drawtilesline (dest+offset, line, 0,0);
		segae_drawspriteline(dest+offset, 0, line);
		segae_drawtilesline (dest+offset, line, 0,1);
	}

	{
		if (segae_vdp_regs[1][1] & 0x40 && nBurnLayer & 2)
		{
			segae_drawtilesline (dest+offset, line, 1,0);
			segae_drawspriteline(dest+offset, 1, line);
			segae_drawtilesline (dest+offset, line, 1,1);
		}
	}

	if (leftcolumnblank && !leftcolumnblank_special) memset(dest+16, 32+16, 8); /* Clear Leftmost column, there should be a register for this like on the SMS i imagine    */
							   			  /* on the SMS this is bit 5 of register 0 (according to CMD's SMS docs) for system E this  */
							   			  /* appears to be incorrect, most games need it blanked 99% of the time so we blank it      */

}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x40; i++) {
			DrvPalette[i] = BurnHighCol((Palette[i] >> 16) & 0xff, (Palette[i] >> 8) & 0xff, Palette[i] & 0xff, 0);
		}
		DrvRecalc = 0;
	}

	UINT16 *pDst = pTransDraw;
	UINT8 *pSrc = &cache_bitmap[16];

	for (INT32 y = 0; y < nScreenHeight; y++)
	{
		for (INT32 x = 0; x < nScreenWidth; x++)
		{
			pDst[x] = pSrc[x];
		}
		pDst += nScreenWidth;
		pSrc += 288;
	}

	if ( megrescu && (DrvDip[1] & 0x10) && (DrvRAM[0x18/*c018*/] == 0xff) ) {
		BurnTransferFlip(1, 1); //- megrescu - unflip 2p coctail
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void segae_interrupt ()
{
	if (currentLine == 0) {
		hintcount = segae_vdp_regs[1][10];
	}

	if (currentLine <= 192) {

		if (currentLine != 192) segae_drawscanline(currentLine);

		if (currentLine == 192)
			vintpending = 1;

		if (hintcount == 0) {
			hintcount = segae_vdp_regs[1][10];
			hintpending = 1;

			if  ((segae_vdp_regs[1][0] & 0x10)) {
				ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
				return;
			} else {
				ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			}

		} else {
			hintcount--;
		}
	}

	if (currentLine > 192) {
		hintcount = segae_vdp_regs[1][10];

		if ( (currentLine<0xe0) && (vintpending) ) {
			if (segae_vdp_regs[1][0x1]&0x20) {
				ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
			} else {
				ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			}
		}
	}
}


static INT32 DrvFrame()
{
	if (DrvReset) DrvDoReset();

	DrvMakeInputs();

	INT32 nInterleave = 262;
	INT32 nCyclesTotal[1] = { 10738635 / 2 / 60 };
	INT32 nCyclesDone[1] = { nCyclesExtra };

	currentLine = 0;

	ZetNewFrame();

	ZetOpen(0);
	for (INT32 i = 0; i < nInterleave; i++) {
		CPU_RUN(0, Zet);

		currentLine = (i - 4) & 0xff;

		segae_interrupt();
	}
	ZetClose();

	nCyclesExtra = nCyclesDone[0] - nCyclesTotal[0];

	if (pBurnSoundOut)
	{
		SN76496Update(0, pBurnSoundOut, nBurnSoundLen);
		SN76496Update(1, pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) DrvDraw();

	return 0;
}

static INT32 DrvInit(UINT8 game)
{
	BurnAllocMemIndex();

	switch (game) {
		case 0:
			if (BurnLoadRom(DrvMainROM + 0x00000,  0, 1)) return 1;	// ( "rom5.ic7",   0x00000, 0x08000, CRC(d63925a7) SHA1(699f222d9712fa42651c753fe75d7b60e016d3ad) ) /* Fixed Code */
			if (BurnLoadRom(DrvMainROM + 0x10000,  1, 1)) return 1;	// ( "rom4.ic5",   0x10000, 0x08000, CRC(ee3caab3) SHA1(f583cf92c579d1ca235e8b300e256ba58a04dc90) )
			if (BurnLoadRom(DrvMainROM + 0x18000,  2, 1)) return 1;	// ( "rom3.ic4",   0x18000, 0x08000, CRC(d2ba9bc9) SHA1(85cf2a801883bf69f78134fc4d5075134f47dc03) )
			break;
		case 1:
		case 2:
			if (BurnLoadRom(DrvMainROM + 0x00000,  0, 1)) return 1;	// ( "rom5.ic7",   0x00000, 0x08000, CRC(d63925a7) SHA1(699f222d9712fa42651c753fe75d7b60e016d3ad) ) /* Fixed Code */
			if (BurnLoadRom(DrvMainROM + 0x10000,  1, 1)) return 1;	// ( "rom4.ic5",   0x10000, 0x08000, CRC(ee3caab3) SHA1(f583cf92c579d1ca235e8b300e256ba58a04dc90) )
			if (BurnLoadRom(DrvMainROM + 0x18000,  2, 1)) return 1;	// ( "rom3.ic4",   0x18000, 0x08000, CRC(d2ba9bc9) SHA1(85cf2a801883bf69f78134fc4d5075134f47dc03) )
			if (BurnLoadRom(DrvMainROM + 0x20000,  3, 1)) return 1;	// ( "rom2.ic3",   0x20000, 0x08000, CRC(e14da070) SHA1(f8781f65be5246a23c1f492905409775bbf82ea8) )
			if (BurnLoadRom(DrvMainROM + 0x28000,  4, 1)) return 1; // ( "rom1.ic2",   0x28000, 0x08000, CRC(3810cbf5) SHA1(c8d5032522c0c903ab3d138f62406a66e14a5c69) )
			break;
		case 3: // fantzn2
			if (BurnLoadRom(DrvMainROM + 0x00000,  0, 1)) return 1;
			if (BurnLoadRom(DrvMainROM + 0x10000,  1, 1)) return 1;
			if (BurnLoadRom(DrvMainROM + 0x20000,  2, 1)) return 1;
			if (BurnLoadRom(DrvMainROM + 0x30000,  3, 1)) return 1;
			if (BurnLoadRom(DrvMainROM + 0x40000,  4, 1)) return 1;
			if (BurnLoadRom(mc8123key  + 0x00000,  5, 1)) return 1;
			mc8123_decrypt_rom(0, 0, DrvMainROM, DrvMainROMFetch, mc8123key);
			mc8123 = 1;

			break;
		case 4: // opaopa
			if (BurnLoadRom(DrvMainROM + 0x00000,  0, 1)) return 1;
			if (BurnLoadRom(DrvMainROM + 0x10000,  1, 1)) return 1;
			if (BurnLoadRom(DrvMainROM + 0x18000,  2, 1)) return 1;
			if (BurnLoadRom(DrvMainROM + 0x20000,  3, 1)) return 1;
			if (BurnLoadRom(DrvMainROM + 0x28000,  4, 1)) return 1;
			if (BurnLoadRom(mc8123key  + 0x00000,  5, 1)) return 1;
			mc8123_decrypt_rom(1, 16, DrvMainROM, DrvMainROMFetch, mc8123key);
			mc8123 = 1;
			mc8123_banked = 1;
			break;
		case 5: // astrofl
			if (BurnLoadRom(DrvMainROM + 0x00000,  0, 1)) return 1;	// ( "rom5.ic7",   0x00000, 0x08000, CRC(d63925a7) SHA1(699f222d9712fa42651c753fe75d7b60e016d3ad) ) /* Fixed Code */
			if (BurnLoadRom(DrvMainROM + 0x10000,  1, 1)) return 1;	// ( "rom4.ic5",   0x10000, 0x08000, CRC(ee3caab3) SHA1(f583cf92c579d1ca235e8b300e256ba58a04dc90) )
			if (BurnLoadRom(DrvMainROM + 0x18000,  2, 1)) return 1;	// ( "rom3.ic4",   0x18000, 0x08000, CRC(d2ba9bc9) SHA1(85cf2a801883bf69f78134fc4d5075134f47dc03) )
			if (BurnLoadRom(DrvMainROM + 0x20000,  3, 1)) return 1;	// ( "rom2.ic3",   0x20000, 0x08000, CRC(e14da070) SHA1(f8781f65be5246a23c1f492905409775bbf82ea8) )
			if (BurnLoadRom(DrvMainROM + 0x28000,  4, 1)) return 1; // ( "rom1.ic2",   0x28000, 0x08000, CRC(3810cbf5) SHA1(c8d5032522c0c903ab3d138f62406a66e14a5c69) )
			mc8123 = 1;
			astrofl_decode();
			break;
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvMainROM, 0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvRAM, 0xc000, 0xffff, MAP_RAM);
	if (mc8123) {
		ZetMapArea(0x0000, 0x7fff, 2, DrvMainROMFetch, DrvMainROM);
	}

	ZetSetWriteHandler(systeme_main_write);
	ZetSetReadHandler(systeme_main_read);
	ZetSetInHandler(systeme_main_in);
	ZetSetOutHandler(systeme_main_out);

	ZetClose();

	SN76489Init(0, 10738635 / 3, 0);
	SN76489Init(1, 10738635 / 3, 1);

	SN76496SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);
	SN76496SetRoute(1, 1.00, BURN_SND_ROUTE_BOTH);

	if (ridleofp) { // and megrescu
		BurnTrackballInit(1);
	}

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029702;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		memset(&ba, 0, sizeof(ba));
		ba.Data	  = segae_vdp_vram[0];
		ba.nLen	  = 0x8000;
		ba.szName = "vram0";
		BurnAcb(&ba);

		memset(&ba, 0, sizeof(ba));
		ba.Data	  = segae_vdp_vram[1];
		ba.nLen	  = 0x8000;
		ba.szName = "vram1";
		BurnAcb(&ba);

		memset(&ba, 0, sizeof(ba));
		ba.Data	  = segae_vdp_cram[0];
		ba.nLen	  = 0x20;
		ba.szName = "cram0";
		BurnAcb(&ba);

		memset(&ba, 0, sizeof(ba));
		ba.Data	  = segae_vdp_cram[1];
		ba.nLen	  = 0x20;
		ba.szName = "cram1";
		BurnAcb(&ba);

		memset(&ba, 0, sizeof(ba));
		ba.Data	  = segae_vdp_regs[0];
		ba.nLen	  = 0x20;
		ba.szName = "regs0";
		BurnAcb(&ba);

		memset(&ba, 0, sizeof(ba));
		ba.Data	  = segae_vdp_regs[1];
		ba.nLen	  = 0x20;
		ba.szName = "regs1";
		BurnAcb(&ba);

	}

	if (nAction & ACB_DRIVER_DATA) {

		ZetScan(nAction);
		SN76496Scan(nAction, pnMin);

		SCAN_VAR(segae_8000bank);
		SCAN_VAR(port_fa_last);
		SCAN_VAR(rombank);

		SCAN_VAR(hintcount);
		SCAN_VAR(vintpending);
		SCAN_VAR(hintpending);
		SCAN_VAR(segae_vdp_cmdpart);
		SCAN_VAR(segae_vdp_command);

		SCAN_VAR(segae_vdp_accessmode);
		SCAN_VAR(segae_vdp_accessaddr);
		SCAN_VAR(segae_vdp_readbuffer);

		SCAN_VAR(segae_vdp_vrambank);

		if (ridleofp) {
			BurnTrackballScan();

			SCAN_VAR(paddle_diff1);
			SCAN_VAR(paddle_diff2);
			SCAN_VAR(paddle_last1);
			SCAN_VAR(paddle_last2);
		}

		SCAN_VAR(nCyclesExtra);

		if (nAction & ACB_WRITE) {
			ZetOpen(0);
			segae_bankswitch();
			ZetClose();
		}
	}

	return 0;
}


static INT32 DrvFantzn2Init()
{
	leftcolumnblank = 1;
	leftcolumnblank_special = 1;

	return DrvInit(3);
}

static INT32 DrvOpaopapInit()
{
	leftcolumnblank = 1;
	leftcolumnblank_special = 1;

	return DrvInit(4);
}

static INT32 DrvOpaopaInit()
{
	leftcolumnblank = 1;
	leftcolumnblank_special = 1;

	return DrvInit(2);
}

static INT32 DrvTransfrmInit()
{
	leftcolumnblank = 1;

	return DrvInit(2);
}

static INT32 DrvSlapshtrInit()
{
	leftcolumnblank = 1;

	return DrvInit(2);
}

static INT32 DrvAstroflInit()
{
	leftcolumnblank = 1;

	return DrvInit(5);
}

static INT32 DrvHangonJrInit()
{
	leftcolumnblank = 1;

	return DrvInit(1);
}

static INT32 DrvTetrisInit()
{
	return DrvInit(0);
}

static INT32 DrvRidleOfpInit()
{
	leftcolumnblank = 1;
	leftcolumnblank_special = 1;
	ridleofp = 1;

	return DrvInit(2);
}
//-----------------------

// Hang-On Jr. Rev.B

static struct BurnRomInfo hangonjrRomDesc[] = {
	{ "epr-7257b.ic7",	0x8000, 0xd63925a7, 1 }, //  0 maincpu
	{ "epr-7258.ic5",	0x8000, 0xee3caab3, 1 }, //  1
	{ "epr-7259.ic4",	0x8000, 0xd2ba9bc9, 1 }, //  2
	{ "epr-7260.ic3",	0x8000, 0xe14da070, 1 }, //  3
	{ "epr-7261.ic2",	0x8000, 0x3810cbf5, 1 }, //  4
};

STD_ROM_PICK(hangonjr)
STD_ROM_FN(hangonjr)

//  Tetris (Japan, System E)

static struct BurnRomInfo TetrisseRomDesc[] = {

	{ "epr-12213.7", 0x8000, 0xef3c7a38, BRF_ESS | BRF_PRG }, // 0 maincpu
	{ "epr-12212.5", 0x8000, 0x28b550bf, BRF_ESS | BRF_PRG }, // 1
	{ "epr-12211.4", 0x8000, 0x5aa114e9, BRF_ESS | BRF_PRG }, // 2
};

STD_ROM_PICK(Tetrisse)
STD_ROM_FN(Tetrisse)

//  Transformer

static struct BurnRomInfo TransfrmRomDesc[] = {

	{ "epr-7605.ic7", 0x8000, 0xccf1d123, BRF_ESS | BRF_PRG }, // 0 maincpu
	{ "epr-7347.ic5", 0x8000, 0xdf0f639f, BRF_ESS | BRF_PRG }, // 1
	{ "epr-7348.ic4", 0x8000, 0x0f38ea96, BRF_ESS | BRF_PRG }, // 2
	{ "epr-7606.ic3", 0x8000, 0x9d485df6, BRF_ESS | BRF_PRG }, // 3
	{ "epr-7350.ic2", 0x8000, 0x0052165d, BRF_ESS | BRF_PRG }, // 4
};

STD_ROM_PICK(Transfrm)
STD_ROM_FN(Transfrm)

struct BurnDriver BurnDrvHangonjr = {
	"hangonjr", NULL, NULL, NULL, "1985",
	"Hang-On Jr. (Rev. B)\0", NULL, "Sega", "System E",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_RACING, 0,
	NULL, hangonjrRomInfo, hangonjrRomName, NULL, NULL, NULL, NULL, HangonjrInputInfo, HangonjrDIPInfo,
	DrvHangonJrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 64,
	256, 192, 4, 3
};

struct BurnDriver BurnDrvTetrisse = {
	"tetrisse", NULL, NULL, NULL, "1988",
	"Tetris (Japan, System E)\0", NULL, "Sega", "System E",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_PUZZLE, 0,
	NULL, TetrisseRomInfo, TetrisseRomName, NULL, NULL, NULL, NULL, TetrisseInputInfo, TetrisseDIPInfo,
	DrvTetrisInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 64,
	256, 192, 4, 3
};

struct BurnDriver BurnDrvTransfrm = {
	"transfrm", NULL, NULL, NULL, "1986",
	"Transformer\0", NULL, "Sega", "System E",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_HORSHOOT, 0,
	NULL, TransfrmRomInfo, TransfrmRomName, NULL, NULL, NULL, NULL, TransfrmInputInfo, TransfrmDIPInfo,
	DrvTransfrmInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 64,
	256, 192, 4, 3
};

// Fantasy Zone II - The Tears of Opa-Opa (MC-8123, 317-0057)

static struct BurnRomInfo fantzn2RomDesc[] = {
	{ "epr-11416.ic7",	0x08000, 0x76db7b7b, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu (encr)
	{ "epr-11415.ic5",	0x10000, 0x57b45681, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-11413.ic3",	0x10000, 0xa231dc85, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-11414.ic4",	0x10000, 0x6f7a9f5f, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "epr-11412.ic2",	0x10000, 0xb14db5af, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "317-0057.key",	0x02000, 0xee43d0f0, 2 | BRF_GRA },           //  5 key
};

STD_ROM_PICK(fantzn2)
STD_ROM_FN(fantzn2)

struct BurnDriver BurnDrvFantzn2 = {
	"fantzn2", NULL, NULL, NULL, "1988",
	"Fantasy Zone II - The Tears of Opa-Opa (MC-8123, 317-0057)\0", NULL, "Sega", "System E",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_HORSHOOT, 0,
	NULL, fantzn2RomInfo, fantzn2RomName, NULL, NULL, NULL, NULL, TransfrmInputInfo, Fantzn2DIPInfo,
	DrvFantzn2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 64,
	248, 192, 4, 3
};


// Opa Opa (MC-8123, 317-0042)

static struct BurnRomInfo opaopaRomDesc[] = {
	{ "epr-11054.ic7",	0x8000, 0x024b1244, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu  (encr)
	{ "epr-11053.ic5",	0x8000, 0x6bc41d6e, 1 | BRF_PRG | BRF_ESS }, //  1          ""
	{ "epr-11052.ic4",	0x8000, 0x395c1d0a, 1 | BRF_PRG | BRF_ESS }, //  2          ""
	{ "epr-11051.ic3",	0x8000, 0x4ca132a2, 1 | BRF_PRG | BRF_ESS }, //  3          ""
	{ "epr-11050.ic2",	0x8000, 0xa165e2ef, 1 | BRF_PRG | BRF_ESS }, //  4          ""

	{ "317-0042.key",	0x2000, 0xd6312538, 2 | BRF_GRA },           //  5 key
};

STD_ROM_PICK(opaopa)
STD_ROM_FN(opaopa)

struct BurnDriver BurnDrvOpaopa = {
	"opaopa", NULL, NULL, NULL, "1987",
	"Opa Opa (MC-8123, 317-0042)\0", NULL, "Sega", "System E",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_MAZE, 0,
	NULL, opaopaRomInfo, opaopaRomName, NULL, NULL, NULL, NULL, Segae2pInputInfo, OpaopaDIPInfo,
	DrvOpaopapInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 64,
	248, 192, 4, 3
};


// Opa Opa (Rev A, unprotected)

static struct BurnRomInfo opaopanRomDesc[] = {
	{ "epr-11023a.ic7",	0x8000, 0x101c5c6a, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "epr-11022.ic5",	0x8000, 0x15203a42, 1 | BRF_PRG | BRF_ESS }, //  1          
	{ "epr-11021.ic4",	0x8000, 0xb4e83340, 1 | BRF_PRG | BRF_ESS }, //  2          
	{ "epr-11020.ic3",	0x8000, 0xc51aad27, 1 | BRF_PRG | BRF_ESS }, //  3          
	{ "epr-11019.ic2",	0x8000, 0xbd0a6248, 1 | BRF_PRG | BRF_ESS }, //  4          
};

STD_ROM_PICK(opaopan)
STD_ROM_FN(opaopan)

struct BurnDriver BurnDrvOpaopan = {
	"opaopan", "opaopa", NULL, NULL, "1987",
	"Opa Opa (Rev A, unprotected)\0", NULL, "Sega", "System E",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_MAZE, 0,
	NULL, opaopanRomInfo, opaopanRomName, NULL, NULL, NULL, NULL, Segae2pInputInfo, OpaopaDIPInfo,
	DrvOpaopaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 64,
	248, 192, 4, 3
};


//  Slap Shooter

static struct BurnRomInfo slapshtrRomDesc[] = {

	{ "epr-7351.ic7", 0x8000, 0x894adb04, BRF_ESS | BRF_PRG }, // 0 maincpu
	{ "epr-7352.ic5", 0x8000, 0x61c938b6, BRF_ESS | BRF_PRG }, // 1
	{ "epr-7353.ic4", 0x8000, 0x8ee2951a, BRF_ESS | BRF_PRG }, // 2
	{ "epr-7354.ic3", 0x8000, 0x41482aa0, BRF_ESS | BRF_PRG }, // 3
	{ "epr-7355.ic2", 0x8000, 0xc67e1aef, BRF_ESS | BRF_PRG }, // 4
};
STD_ROM_PICK(slapshtr)
STD_ROM_FN(slapshtr)

struct BurnDriver BurnDrvSlapshtr = {
	"slapshtr", NULL, NULL, NULL, "1986",
	"Slap Shooter\0", NULL, "Sega", "System E",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_SPORTSMISC, 0,
	NULL, slapshtrRomInfo, slapshtrRomName, NULL, NULL, NULL, NULL, TransfrmInputInfo, TransfrmDIPInfo,
	DrvSlapshtrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 64,
	256, 192, 4, 3
};

// Astro Flash (Japan)

static struct BurnRomInfo AstroflRomDesc[] = {

	{ "epr-7723.ic7", 0x8000, 0x66061137, BRF_ESS | BRF_PRG }, // 0 maincpu
	{ "epr-7347.ic5", 0x8000, 0xdf0f639f, BRF_ESS | BRF_PRG }, // 1
	{ "epr-7348.ic4", 0x8000, 0x0f38ea96, BRF_ESS | BRF_PRG }, // 2
	{ "epr-7349.ic3", 0x8000, 0xf8c352d5, BRF_ESS | BRF_PRG }, // 3
	{ "epr-7350.ic2", 0x8000, 0x0052165d, BRF_ESS | BRF_PRG }, // 4
};

STD_ROM_PICK(Astrofl)
STD_ROM_FN(Astrofl)

struct BurnDriver BurnDrvAstrofl = {
	"astrofl", "transfrm", NULL, NULL, "1986",
	"Astro Flash (Japan)\0", NULL, "Sega", "System E",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_HORSHOOT, 0,
	NULL, AstroflRomInfo, AstroflRomName, NULL, NULL, NULL, NULL, TransfrmInputInfo, TransfrmDIPInfo,
	DrvAstroflInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 64,
	256, 192, 4, 3
};

// Riddle of Pythagoras (Japan)

static struct BurnRomInfo RidleOfpRomDesc[] = {

	{ "epr-10426.bin", 0x8000, 0x4404c7e7, BRF_ESS | BRF_PRG }, // 0 maincpu
	{ "epr-10425.bin", 0x8000, 0x35964109, BRF_ESS | BRF_PRG }, // 1
	{ "epr-10424.bin", 0x8000, 0xfcda1dfa, BRF_ESS | BRF_PRG }, // 2
	{ "epr-10423.bin", 0x8000, 0x0b87244f, BRF_ESS | BRF_PRG }, // 3
	{ "epr-10422.bin", 0x8000, 0x14781e56, BRF_ESS | BRF_PRG }, // 4
};

STD_ROM_PICK(RidleOfp)
STD_ROM_FN(RidleOfp)

struct BurnDriver BurnDrvRidleOfp = {
	"ridleofp", NULL, NULL, NULL, "1986",
	"Riddle of Pythagoras (Japan)\0", NULL, "Sega / Nasco", "System E",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_BREAKOUT, 0,
	NULL, RidleOfpRomInfo, RidleOfpRomName, NULL, NULL, NULL, NULL, RidleofpInputInfo, RidleOfpDIPInfo,
	DrvRidleOfpInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 64,
	192, 240, 3, 4
};

// Megumi Rescue

static struct BurnRomInfo megrescuRomDesc[] = {
	{ "megumi_rescue_version_10.30_final_version_ic-7.ic7",	0x8000, 0x490d0059, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "megumi_rescue_version_10.30_final_version_ic-5.ic5",	0x8000, 0x278caba8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "megumi_rescue_version_10.30_final_version_ic-4.ic4",	0x8000, 0xbda242d1, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "megumi_rescue_version_10.30_final_version_ic-3.ic3",	0x8000, 0x56e36f85, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "megumi_rescue_version_10.30_final_version_ic-2.ic2",	0x8000, 0x5b74c767, 1 | BRF_PRG | BRF_ESS }, //  4
};

STD_ROM_PICK(megrescu)
STD_ROM_FN(megrescu)

static INT32 MegrescuInit()
{
	megrescu = 1;

	return (DrvRidleOfpInit());
}

struct BurnDriver BurnDrvMegrescu = {
	"megrescu", NULL, NULL, NULL, "1987",
	"Megumi Rescue\0", NULL, "Sega / Exa", "System E",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_BREAKOUT, 0,
	NULL, megrescuRomInfo, megrescuRomName, NULL, NULL, NULL, NULL, MegrescuInputInfo, MegrescuDIPInfo,
	MegrescuInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 64,
	192, 248, 3, 4
};
