// FB Alpha Deco Cassette driver module
// Based on MAME driver by Juergen Buchmueller, David Haywood, Ajrhacker
// Uses tape loader & protection code by Juergen Buchmueller, David Haywood
// Some graphics routines by Juergen Buchmueller, David Haywood, Ajrhacker

/*
note -- 
cflyball, cpsoccer, coozumou, & zeroize overload bios (glitches normal)
*/

#include "tiles_generic.h"
#include "m6502_intf.h"
#include "bitswap.h"
#include "flt_rc.h"
#include "i8x41.h"
#include "ay8910.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvMainBIOS;
static UINT8 *DrvSoundBIOS;
static UINT8 *DrvCassette;
static UINT32 DrvCassetteLen; // here for now
static UINT8 *DrvDongle;
static UINT8 *DrvGfxData;
static UINT8 *DrvMCUROM;
static UINT8 *DrvCharExp;
static UINT8 *DrvTileExp;
static UINT8 *DrvObjExp;
static UINT8 *DrvMainRAM;
static UINT8 *DrvCharRAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvColRAM;
static UINT8 *DrvTileRAM;
static UINT8 *DrvObjRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvSoundRAM;
static UINT8 *DrvPalLut;
static UINT32 *DrvPaletteTable;
static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT16 *pTempDraw[2];

static UINT8 decocass_reset;

static UINT8 watchdog_count;
static UINT8 watchdog_flip;
static INT32 watchdog;

static UINT8 soundlatch;
static UINT8 soundlatch2;
static UINT8 sound_ack;
static UINT8 audio_nmi_enabled;
static UINT8 audio_nmi_state;

static INT32 burgertime_mode = 0; // for sound-fix
static INT32 skater_mode = 0;

static INT32 e900_enable = 0;
static INT32 e900_gfxbank = 0;

static UINT8 mux_data;

static UINT8 color_missiles;
static UINT8 mode_set;
static UINT8 color_center_bot;
static UINT8 back_h_shift;
static UINT8 back_vl_shift;
static UINT8 back_vr_shift;
static UINT8 part_h_shift;
static UINT8 part_v_shift;
static UINT8 center_h_shift_space;
static UINT8 center_v_shift;

static INT32 vblank;
static INT32 flipscreen = 0;

static UINT8 i8041_p1;
static UINT8 i8041_p2;

static void (*prot_write)(UINT16, UINT8) = NULL;
static UINT8 (*prot_read)(UINT16) = NULL;

#define E5XX_MASK   0x02

#define MAKE_MAP(m0,m1,m2,m3,m4,m5,m6,m7)	\
	((UINT32)(m0)) | \
	((UINT32)(m1) << 3) | \
	((UINT32)(m2) << 6) | \
	((UINT32)(m3) << 9) | \
	((UINT32)(m4) << 12) | \
	((UINT32)(m5) << 15) | \
	((UINT32)(m6) << 18) | \
	((UINT32)(m7) << 21)

#define T1MAP(x, m) (((m)>>(x*3))&7)

static UINT32 type1_inmap = 0;
static UINT32 type1_outmap = 0;
static UINT8  type1_latch1 = 0;
static UINT8 *type1_map = NULL;

#define T1PROM 1
#define T1DIRECT 2
#define T1LATCH 4
#define T1LATCHINV 8

static UINT8 type2_xx_latch = 0;
static UINT8 type2_d2_latch = 0;
static UINT8 type2_promaddr = 0;

enum {
	TYPE3_SWAP_01,
	TYPE3_SWAP_12,
	TYPE3_SWAP_13,
	TYPE3_SWAP_24,
	TYPE3_SWAP_25,
	TYPE3_SWAP_34_0,
	TYPE3_SWAP_34_7,
	TYPE3_SWAP_45,
	TYPE3_SWAP_23_56,
	TYPE3_SWAP_56,
	TYPE3_SWAP_67
};

static UINT8 type3_pal_19 = 0;
static UINT16 type3_ctrs = 0;
static UINT8 type3_swap = 0;
static UINT8 type3_d0_latch = 0;

static UINT16 type4_ctrs = 0;
static UINT8 type4_latch = 0;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvJoy5[8];
static UINT8 DrvJoy6[8];
static UINT8 DrvJoy7[8];
static UINT8 DrvJoy8[8];
static UINT8 DrvJoy9[8];
static UINT8 DrvJoy10[8];
static UINT8 DrvJoy11[8];
static UINT8 DrvInputs[11];
static UINT8 DrvDips[3]; // #2 is bios dip
static UINT8 DrvReset;

static INT32 fourway_mode = 0;

// analog inputs - not emulated - unused by games in this system.

static struct BurnInputInfo DecocassInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Bios setting",	BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Decocass)

static struct BurnDIPInfo DecocassDIPList[]=
{
	{0x11, 0xff, 0xff, 0xff, NULL			},
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x11, 0x01, 0x03, 0x00, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0x03, 0x01, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x11, 0x01, 0x0c, 0x00, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0x0c, 0x04, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Type of Tape"		},
	{0x11, 0x01, 0x30, 0x00, "MT (Big)"		},
	{0x11, 0x01, 0x30, 0x10, "invalid?"		},
	{0x11, 0x01, 0x30, 0x20, "invalid?"		},
	{0x11, 0x01, 0x30, 0x30, "MD (Small)"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x11, 0x01, 0x40, 0x00, "Upright"		},
	{0x11, 0x01, 0x40, 0x40, "Cocktail"		},

	{0   , 0xfe, 0   ,    6, "Country Code"		},
	{0x12, 0x01, 0xe0, 0xe0, "A"			},
	{0x12, 0x01, 0xe0, 0xc0, "B"			},
	{0x12, 0x01, 0xe0, 0xa0, "C"			},
	{0x12, 0x01, 0xe0, 0x80, "D"			},
	{0x12, 0x01, 0xe0, 0x60, "E"			},
	{0x12, 0x01, 0xe0, 0x40, "F"			},

	{0   , 0xfe, 0   ,    4, "Bios Version"		},
	{0x13, 0x01, 0x03, 0x00, "Japan A, Newer"	},
	{0x13, 0x01, 0x03, 0x01, "Japan A, Older"	},
	{0x13, 0x01, 0x03, 0x02, "USA B, Newer"		},
	{0x13, 0x01, 0x03, 0x03, "USA B, Older"		},
};

STDDIPINFO(Decocass)

static struct BurnDIPInfo CtsttapeDIPList[]=
{
	{0x13, 0xff, 0xff, 0x02, NULL			},
};

STDDIPINFOEXT(Ctsttape,	Decocass,	Ctsttape)

static struct BurnDIPInfo DecomultDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL			},
};

STDDIPINFOEXT(Decomult,	Decocass,	Decomult)

static struct BurnDIPInfo ChwyDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x02, NULL			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x12, 0x01, 0x01, 0x01, "3"			},
	{0x12, 0x01, 0x01, 0x00, "5"			},
};

STDDIPINFOEXT(Chwy,	Decocass,	Chwy)

static struct BurnDIPInfo CptennisDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x02, NULL			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x12, 0x01, 0x01, 0x01, "2"			},
	{0x12, 0x01, 0x01, 0x00, "3"			},
};

STDDIPINFOEXT(Cptennis,	Decocass,	Cptennis)

static struct BurnDIPInfo CptennisjDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x12, 0x01, 0x01, 0x01, "2"			},
	{0x12, 0x01, 0x01, 0x00, "3"			},
};

STDDIPINFOEXT(Cptennisj,	Decocass,	Cptennisj)

static struct BurnDIPInfo CmanhatDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x12, 0x01, 0x01, 0x01, "4"			},
	{0x12, 0x01, 0x01, 0x00, "6"			},
};

STDDIPINFOEXT(Cmanhat,	Decocass,	Cmanhat)

static struct BurnDIPInfo CterraniDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x02, NULL			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x12, 0x01, 0x01, 0x01, "3"			},
	{0x12, 0x01, 0x01, 0x00, "5"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x12, 0x01, 0x06, 0x00, "None"			},
	{0x12, 0x01, 0x06, 0x06, "3000"			},
	{0x12, 0x01, 0x06, 0x04, "5000"			},
	{0x12, 0x01, 0x06, 0x02, "7000"			},

	{0   , 0xfe, 0   ,    2, "Rocket Movement"	},
	{0x12, 0x01, 0x08, 0x08, "Easy"			},
	{0x12, 0x01, 0x08, 0x00, "Difficult"		},

	{0   , 0xfe, 0   ,    2, "Alien Craft Movement"	},
	{0x12, 0x01, 0x10, 0x10, "Easy"			},
	{0x12, 0x01, 0x10, 0x00, "Difficult"		},
};

STDDIPINFOEXT(Cterrani,	Decocass,	Cterrani)

static struct BurnDIPInfo CastfantDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x02, NULL			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x12, 0x01, 0x01, 0x01, "3"			},
	{0x12, 0x01, 0x01, 0x00, "5"			},
};

STDDIPINFOEXT(Castfant,	Decocass,	Castfant)

static struct BurnDIPInfo CsuperasDIPList[]=
{
	{0x12, 0xff, 0xff, 0xef, NULL			},
	{0x13, 0xff, 0xff, 0x02, NULL			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x12, 0x01, 0x01, 0x01, "3"			},
	{0x12, 0x01, 0x01, 0x00, "5"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x12, 0x01, 0x06, 0x00, "None"			},
	{0x12, 0x01, 0x06, 0x06, "20000"		},
	{0x12, 0x01, 0x06, 0x04, "30000"		},
	{0x12, 0x01, 0x06, 0x02, "40000"		},

	{0   , 0xfe, 0   ,    2, "Alien Craft Movement"	},
	{0x12, 0x01, 0x08, 0x08, "Easy"			},
	{0x12, 0x01, 0x08, 0x00, "Difficult"		},
};

STDDIPINFOEXT(Csuperas, Decocass, Csuperas)

static struct BurnDIPInfo CtislandDIPList[]=
{
	{0x12, 0xff, 0xff, 0xef, NULL			},
	{0x13, 0xff, 0xff, 0x02, NULL			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x12, 0x01, 0x01, 0x01, "3"			},
	{0x12, 0x01, 0x01, 0x00, "5"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x12, 0x01, 0x06, 0x06, "15000"		},
	{0x12, 0x01, 0x06, 0x04, "20000"		},
	{0x12, 0x01, 0x06, 0x02, "25000"		},
	{0x12, 0x01, 0x06, 0x00, "30000"		},
};

STDDIPINFOEXT(Ctisland, Decocass, Ctisland)

static struct BurnDIPInfo Cocean1aDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    8, "1 Coin Credit"	},
	{0x11, 0x01, 0x07, 0x07, "1"			},
	{0x11, 0x01, 0x07, 0x06, "2"			},
	{0x11, 0x01, 0x07, 0x05, "5"			},
	{0x11, 0x01, 0x07, 0x04, "10"			},
	{0x11, 0x01, 0x07, 0x03, "20"			},
	{0x11, 0x01, 0x07, 0x02, "30"			},
	{0x11, 0x01, 0x07, 0x01, "40"			},
	{0x11, 0x01, 0x07, 0x00, "50"			},

	{0   , 0xfe, 0   ,    2, "None"			},
	{0x11, 0x01, 0x08, 0x08, "Off"			},
	{0x11, 0x01, 0x08, 0x00, "None"			},

	{0   , 0xfe, 0   ,    4, "Key Switch Credit"	},
	{0x12, 0x01, 0x03, 0x03, "1 Coin 10 Credits"	},
	{0x12, 0x01, 0x03, 0x02, "1 Coin 20 Credits"	},
	{0x12, 0x01, 0x03, 0x01, "1 Coin 50 Credits"	},
	{0x12, 0x01, 0x03, 0x00, "1 Coin 100 Credits"	},

	{0   , 0xfe, 0   ,    2, "Game Select"		},
	{0x12, 0x01, 0x04, 0x04, "1 to 8 Lines"		},
	{0x12, 0x01, 0x04, 0x00, "Center Line"		},

	{0   , 0xfe, 0   ,    2, "Background Music"	},
	{0x12, 0x01, 0x08, 0x08, "Off"			},
	{0x12, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Pay Out %"		},
	{0x12, 0x01, 0x10, 0x10, "Payout 75%"		},
	{0x12, 0x01, 0x10, 0x00, "Payout 85%"		},
};

STDDIPINFOEXT(Cocean1a,	Decocass,	Cocean1a)

static struct BurnDIPInfo Cocean6bDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x02, NULL			},

	{0   , 0xfe, 0   ,    8, "1 Coin Credit"	},
	{0x11, 0x01, 0x07, 0x07, "1"			},
	{0x11, 0x01, 0x07, 0x06, "2"			},
	{0x11, 0x01, 0x07, 0x05, "5"			},
	{0x11, 0x01, 0x07, 0x04, "10"			},
	{0x11, 0x01, 0x07, 0x03, "20"			},
	{0x11, 0x01, 0x07, 0x02, "30"			},
	{0x11, 0x01, 0x07, 0x01, "40"			},
	{0x11, 0x01, 0x07, 0x00, "50"			},

	{0   , 0xfe, 0   ,    2, "None"			},
	{0x11, 0x01, 0x08, 0x08, "Off"			},
	{0x11, 0x01, 0x08, 0x00, "None"			},

	{0   , 0xfe, 0   ,    4, "Key Switch Credit"	},
	{0x12, 0x01, 0x03, 0x03, "1 Coin 10 Credits"	},
	{0x12, 0x01, 0x03, 0x02, "1 Coin 20 Credits"	},
	{0x12, 0x01, 0x03, 0x01, "1 Coin 50 Credits"	},
	{0x12, 0x01, 0x03, 0x00, "1 Coin 100 Credits"	},

	{0   , 0xfe, 0   ,    2, "Game Select"		},
	{0x12, 0x01, 0x04, 0x04, "1 to 8 Lines"		},
	{0x12, 0x01, 0x04, 0x00, "Center Line"		},

	{0   , 0xfe, 0   ,    2, "Background Music"	},
	{0x12, 0x01, 0x08, 0x08, "Off"			},
	{0x12, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Pay Out %"		},
	{0x12, 0x01, 0x10, 0x10, "Payout 75%"		},
	{0x12, 0x01, 0x10, 0x00, "Payout 85%"		},
};

STDDIPINFOEXT(Cocean6b,	Decocass,	Cocean6b)

static struct BurnDIPInfo ClocknchDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x02, NULL			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x12, 0x01, 0x01, 0x01, "3"			},
	{0x12, 0x01, 0x01, 0x00, "5"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x12, 0x01, 0x06, 0x00, "None"			},
	{0x12, 0x01, 0x06, 0x06, "15000"		},
	{0x12, 0x01, 0x06, 0x04, "20000"		},
	{0x12, 0x01, 0x06, 0x02, "30000"		},

	{0   , 0xfe, 0   ,    2, "Game Speed"		},
	{0x12, 0x01, 0x08, 0x08, "Slow"			},
	{0x12, 0x01, 0x08, 0x00, "Hard"			},
};

STDDIPINFOEXT(Clocknch,	Decocass,	Clocknch)

static struct BurnDIPInfo ClocknchjDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x12, 0x01, 0x01, 0x01, "3"			},
	{0x12, 0x01, 0x01, 0x00, "5"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x12, 0x01, 0x06, 0x00, "None"			},
	{0x12, 0x01, 0x06, 0x06, "15000"		},
	{0x12, 0x01, 0x06, 0x04, "20000"		},
	{0x12, 0x01, 0x06, 0x02, "30000"		},
};

STDDIPINFOEXT(Clocknchj,	Decocass,	Clocknchj)

static struct BurnDIPInfo CprogolfDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x02, NULL			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x12, 0x01, 0x01, 0x01, "2"			},
	{0x12, 0x01, 0x01, 0x00, "3"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x12, 0x01, 0x06, 0x00, "None"			},
	{0x12, 0x01, 0x06, 0x02, "6 Under"		},
	{0x12, 0x01, 0x06, 0x04, "3 Under"		},
	{0x12, 0x01, 0x06, 0x06, "1 Under"		},

	{0   , 0xfe, 0   ,    2, "Number of Strokes"	},
	{0x12, 0x01, 0x08, 0x00, "Par +2"		},
	{0x12, 0x01, 0x08, 0x08, "Par +3"		},

	{0   , 0xfe, 0   ,    2, "Stroke Power/Ball Direction"		},
	{0x12, 0x01, 0x10, 0x10, "Off"			},
	{0x12, 0x01, 0x10, 0x00, "On"			},
};

STDDIPINFOEXT(Cprogolf,	Decocass,	Cprogolf)

static struct BurnDIPInfo CprogolfjDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x12, 0x01, 0x01, 0x01, "2"			},
	{0x12, 0x01, 0x01, 0x00, "3"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x12, 0x01, 0x06, 0x00, "None"			},
	{0x12, 0x01, 0x06, 0x02, "6 Under"		},
	{0x12, 0x01, 0x06, 0x04, "3 Under"		},
	{0x12, 0x01, 0x06, 0x06, "1 Under"		},

	{0   , 0xfe, 0   ,    2, "Number of Strokes"	},
	{0x12, 0x01, 0x08, 0x00, "Par +2"		},
	{0x12, 0x01, 0x08, 0x08, "Par +3"		},

	{0   , 0xfe, 0   ,    2, "Stroke Power/Ball Direction"		},
	{0x12, 0x01, 0x10, 0x10, "Off"			},
	{0x12, 0x01, 0x10, 0x00, "On"			},
};

STDDIPINFOEXT(Cprogolfj,	Decocass,	Cprogolfj)

static struct BurnDIPInfo CexploreDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x02, NULL			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x12, 0x01, 0x01, 0x01, "3"			},
	{0x12, 0x01, 0x01, 0x00, "5"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x12, 0x01, 0x06, 0x00, "None"			},
	{0x12, 0x01, 0x06, 0x06, "10000"		},
	{0x12, 0x01, 0x06, 0x04, "1500000"		},
	{0x12, 0x01, 0x06, 0x02, "30000"		},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x12, 0x01, 0x08, 0x08, "Easy"			},
	{0x12, 0x01, 0x08, 0x00, "Difficult"		},

	{0   , 0xfe, 0   ,    2, "Number of UFOs"	},
	{0x12, 0x01, 0x10, 0x10, "Few"			},
	{0x12, 0x01, 0x10, 0x00, "Many"			},
};

STDDIPINFOEXT(Cexplore,	Decocass,	Cexplore)

static struct BurnDIPInfo CluckypoDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x02, NULL			},

	{0   , 0xfe, 0   ,    2, "Show Dealer Hand"	},
	{0x12, 0x01, 0x01, 0x00, "Yes"			},
	{0x12, 0x01, 0x01, 0x01, "No"			},
};

STDDIPINFOEXT(Cluckypo,	Decocass,	Cluckypo)

static struct BurnDIPInfo Cdiscon1DIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x02, NULL			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x12, 0x01, 0x01, 0x01, "3"			},
	{0x12, 0x01, 0x01, 0x00, "5"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x12, 0x01, 0x06, 0x00, "None"			},
	{0x12, 0x01, 0x06, 0x06, "10000"		},
	{0x12, 0x01, 0x06, 0x04, "20000"		},
	{0x12, 0x01, 0x06, 0x02, "30000"		},

	{0   , 0xfe, 0   ,    2, "Music Weapons"	},
	{0x12, 0x01, 0x08, 0x00, "2"			},
	{0x12, 0x01, 0x08, 0x08, "1"			},
};

STDDIPINFOEXT(Cdiscon1,	Decocass,	Cdiscon1)

static struct BurnDIPInfo CsweethtDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x02, NULL			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x12, 0x01, 0x01, 0x01, "3"			},
	{0x12, 0x01, 0x01, 0x00, "5"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x12, 0x01, 0x06, 0x00, "None"			},
	{0x12, 0x01, 0x06, 0x06, "10000"		},
	{0x12, 0x01, 0x06, 0x04, "20000"		},
	{0x12, 0x01, 0x06, 0x02, "30000"		},

	{0   , 0xfe, 0   ,    2, "Music Weapons"	},
	{0x12, 0x01, 0x08, 0x00, "8"			},
	{0x12, 0x01, 0x08, 0x08, "5"			},
};

STDDIPINFOEXT(Csweetht,	Decocass,	Csweetht)

static struct BurnDIPInfo CtornadoDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x02, NULL			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x12, 0x01, 0x01, 0x01, "3"			},
	{0x12, 0x01, 0x01, 0x00, "5"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x12, 0x01, 0x06, 0x00, "None"			},
	{0x12, 0x01, 0x06, 0x06, "10000"		},
	{0x12, 0x01, 0x06, 0x04, "20000"		},
	{0x12, 0x01, 0x06, 0x02, "30000"		},

	{0   , 0xfe, 0   ,    2, "Crash Bombs"		},
	{0x12, 0x01, 0x08, 0x08, "3"			},
	{0x12, 0x01, 0x08, 0x00, "5"			},

	{0   , 0xfe, 0   ,    2, "Alens' Speed"		},
	{0x12, 0x01, 0x10, 0x10, "Slow"			},
	{0x12, 0x01, 0x10, 0x00, "Fast"			},
};

STDDIPINFOEXT(Ctornado,	Decocass,	Ctornado)

static struct BurnDIPInfo CmissnxDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x02, NULL			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x12, 0x01, 0x01, 0x01, "3"			},
	{0x12, 0x01, 0x01, 0x00, "5"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x12, 0x01, 0x06, 0x00, "None"			},
	{0x12, 0x01, 0x06, 0x06, "5000"			},
	{0x12, 0x01, 0x06, 0x04, "10000"		},
	{0x12, 0x01, 0x06, 0x02, "15000"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x12, 0x01, 0x18, 0x18, "Easy"			},
	{0x12, 0x01, 0x18, 0x10, "Normal"		},
	{0x12, 0x01, 0x18, 0x08, "Hard"			},
	{0x12, 0x01, 0x18, 0x00, "Hardest"		},
};

STDDIPINFOEXT(Cmissnx,	Decocass,	Cmissnx)

static struct BurnDIPInfo CbtimeDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x02, NULL			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x12, 0x01, 0x01, 0x01, "3"			},
	{0x12, 0x01, 0x01, 0x00, "5"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x12, 0x01, 0x06, 0x06, "20000"		},
	{0x12, 0x01, 0x06, 0x04, "30000"		},
	{0x12, 0x01, 0x06, 0x02, "40000"		},
	{0x12, 0x01, 0x06, 0x00, "50000"		},

	{0   , 0xfe, 0   ,    2, "Enemies"		},
	{0x12, 0x01, 0x08, 0x08, "4"			},
	{0x12, 0x01, 0x08, 0x00, "6"			},

	{0   , 0xfe, 0   ,    2, "End of Level Pepper"	},
	{0x12, 0x01, 0x10, 0x10, "No"			},
	{0x12, 0x01, 0x10, 0x00, "Yes"			},
};

STDDIPINFOEXT(Cbtime,	Decocass,	Cbtime)

static struct BurnDIPInfo ChamburgerDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x12, 0x01, 0x01, 0x01, "3"			},
	{0x12, 0x01, 0x01, 0x00, "5"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x12, 0x01, 0x06, 0x06, "20000"		},
	{0x12, 0x01, 0x06, 0x04, "30000"		},
	{0x12, 0x01, 0x06, 0x02, "40000"		},
	{0x12, 0x01, 0x06, 0x00, "50000"		},

	{0   , 0xfe, 0   ,    2, "Enemies"		},
	{0x12, 0x01, 0x08, 0x08, "4"			},
	{0x12, 0x01, 0x08, 0x00, "6"			},

	{0   , 0xfe, 0   ,    2, "End of Level Pepper"	},
	{0x12, 0x01, 0x10, 0x10, "No"			},
	{0x12, 0x01, 0x10, 0x00, "Yes"			},
};

STDDIPINFOEXT(Chamburger,	Decocass,	Chamburger)

static struct BurnDIPInfo CburnrubDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x02, NULL			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x12, 0x01, 0x01, 0x01, "3"			},
	{0x12, 0x01, 0x01, 0x00, "5"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x12, 0x01, 0x06, 0x02, "20000"		},
	{0x12, 0x01, 0x06, 0x00, "30000"		},
	{0x12, 0x01, 0x06, 0x06, "Every 30000"		},
	{0x12, 0x01, 0x06, 0x04, "Every 70000"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x12, 0x01, 0x08, 0x08, "Off"			},
	{0x12, 0x01, 0x08, 0x00, "On"			},
};

STDDIPINFOEXT(Cburnrub,	Decocass,	Cburnrub)

static struct BurnDIPInfo CgraplopDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x02, NULL			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x12, 0x01, 0x01, 0x01, "3"			},
	{0x12, 0x01, 0x01, 0x00, "5"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x12, 0x01, 0x06, 0x00, "None"			},
	{0x12, 0x01, 0x06, 0x06, "20000"		},
	{0x12, 0x01, 0x06, 0x04, "50000"		},
	{0x12, 0x01, 0x06, 0x02, "70000"		},

	{0   , 0xfe, 0   ,    2, "Number of Up Sign"	},
	{0x12, 0x01, 0x08, 0x08, "Few"			},
	{0x12, 0x01, 0x08, 0x00, "Many"			},

	{0   , 0xfe, 0   ,    2, "Falling Speed"	},
	{0x12, 0x01, 0x10, 0x10, "Easy"			},
	{0x12, 0x01, 0x10, 0x00, "Difficult"		},
};

STDDIPINFOEXT(Cgraplop,	Decocass,	Cgraplop)

static struct BurnDIPInfo ClapapaDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x02, NULL			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x12, 0x01, 0x01, 0x01, "3"			},
	{0x12, 0x01, 0x01, 0x00, "5"			},
};

STDDIPINFOEXT(Clapapa,	Decocass,	Clapapa)

static struct BurnDIPInfo CnightstDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x02, NULL			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x12, 0x01, 0x01, 0x01, "3"			},
	{0x12, 0x01, 0x01, 0x00, "5"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x12, 0x01, 0x06, 0x06, "When Night Star Completed (First 2 Times)"		},
	{0x12, 0x01, 0x06, 0x04, "When Night Star Completed (First Time Only)"		},
	{0x12, 0x01, 0x06, 0x02, "Every 70000"		},
	{0x12, 0x01, 0x06, 0x00, "30000 Only"		},

	{0   , 0xfe, 0   ,    2, "Number of Missles"	},
	{0x12, 0x01, 0x08, 0x08, "Few"			},
	{0x12, 0x01, 0x08, 0x00, "Many"			},

	{0   , 0xfe, 0   ,    2, "Enemy's Speed"	},
	{0x12, 0x01, 0x10, 0x10, "Slow"			},
	{0x12, 0x01, 0x10, 0x00, "Fast"			},
};

STDDIPINFOEXT(Cnightst,	Decocass,	Cnightst)

static struct BurnDIPInfo CprobowlDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x02, NULL			},

	{0   , 0xfe, 0   ,    2, "Show Bonus Instructions"	},
	{0x12, 0x01, 0x01, 0x00, "Off"			},
	{0x12, 0x01, 0x01, 0x01, "On"			},
};

STDDIPINFOEXT(Cprobowl,	Decocass,	Cprobowl)

static struct BurnDIPInfo CskaterDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x12, 0x01, 0x01, 0x01, "3"			},
	{0x12, 0x01, 0x01, 0x00, "5"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x12, 0x01, 0x06, 0x00, "60000"		},
	{0x12, 0x01, 0x06, 0x06, "20000"		},
	{0x12, 0x01, 0x06, 0x04, "30000"		},
	{0x12, 0x01, 0x06, 0x02, "40000"		},

	{0   , 0xfe, 0   ,    2, "Enemy's Speed"	},
	{0x12, 0x01, 0x08, 0x08, "Easy"			},
	{0x12, 0x01, 0x08, 0x00, "Difficult"		},

	{0   , 0xfe, 0   ,    2, "Number of Skates"	},
	{0x12, 0x01, 0x10, 0x10, "Small"		},
	{0x12, 0x01, 0x10, 0x00, "Large"		},
};

STDDIPINFOEXT(Cskater,	Decocass,	Cskater)

static struct BurnDIPInfo CpsoccerDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x02, NULL			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x12, 0x01, 0x01, 0x01, "3"			},
	{0x12, 0x01, 0x01, 0x00, "5"			},

	{0   , 0xfe, 0   ,    4, "Number of Nice Goal"	},
	{0x12, 0x01, 0x06, 0x00, "None"			},
	{0x12, 0x01, 0x06, 0x06, "5"			},
	{0x12, 0x01, 0x06, 0x04, "10"			},
	{0x12, 0x01, 0x06, 0x02, "20"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x08, 0x00, "Off"			},
	{0x12, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x12, 0x01, 0x10, 0x10, "Easy"			},
	{0x12, 0x01, 0x10, 0x00, "Difficult"		},
};

STDDIPINFOEXT(Cpsoccer,	Decocass,	Cpsoccer)

static struct BurnDIPInfo CpsoccerjDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x12, 0x01, 0x01, 0x01, "3"			},
	{0x12, 0x01, 0x01, 0x00, "5"			},

	{0   , 0xfe, 0   ,    4, "Number of Nice Goal"	},
	{0x12, 0x01, 0x06, 0x00, "None"			},
	{0x12, 0x01, 0x06, 0x06, "5"			},
	{0x12, 0x01, 0x06, 0x04, "10"			},
	{0x12, 0x01, 0x06, 0x02, "20"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x08, 0x00, "Off"			},
	{0x12, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x12, 0x01, 0x10, 0x10, "Easy"			},
	{0x12, 0x01, 0x10, 0x00, "Difficult"		},
};

STDDIPINFOEXT(Cpsoccerj,	Decocass,	Cpsoccerj)

static struct BurnDIPInfo CsdtenisDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x12, 0x01, 0x01, 0x01, "2"			},
	{0x12, 0x01, 0x01, 0x00, "1"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x12, 0x01, 0x06, 0x00, "None"			},
	{0x12, 0x01, 0x06, 0x06, "Every 1set"		},
	{0x12, 0x01, 0x06, 0x04, "Every 2set"		},
	{0x12, 0x01, 0x06, 0x02, "Every 3set"		},

	{0   , 0xfe, 0   ,    2, "Speed Level"		},
	{0x12, 0x01, 0x08, 0x08, "Low Speed"		},
	{0x12, 0x01, 0x08, 0x00, "High Speed"		},

	{0   , 0xfe, 0   ,    2, "Attack Level"		},
	{0x12, 0x01, 0x10, 0x10, "Easy"			},
	{0x12, 0x01, 0x10, 0x00, "Difficult"		},
};

STDDIPINFOEXT(Csdtenis,	Decocass, Csdtenis)

static struct BurnDIPInfo CzeroizeDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x02, NULL			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x12, 0x01, 0x01, 0x01, "3"			},
	{0x12, 0x01, 0x01, 0x00, "5"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x12, 0x01, 0x06, 0x06, "None"			},
	{0x12, 0x01, 0x06, 0x04, "50000"		},
	{0x12, 0x01, 0x06, 0x02, "70000"		},
	{0x12, 0x01, 0x06, 0x00, "90000"		},
};

STDDIPINFOEXT(Czeroize,	Decocass,	Czeroize)

static struct BurnDIPInfo CppicfDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x02, NULL			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x12, 0x01, 0x01, 0x01, "3"			},
	{0x12, 0x01, 0x01, 0x00, "5"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x12, 0x01, 0x06, 0x00, "None"			},
	{0x12, 0x01, 0x06, 0x06, "30000"		},
	{0x12, 0x01, 0x06, 0x04, "50000"		},
	{0x12, 0x01, 0x06, 0x02, "70000"		},

	{0   , 0xfe, 0   ,    2, "Infinite Lives"	},
	{0x12, 0x01, 0x10, 0x10, "Off"			},
	{0x12, 0x01, 0x10, 0x00, "On"			},
};

STDDIPINFOEXT(Cppicf,	Decocass,	Cppicf)

static struct BurnDIPInfo CscrtryDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x02, NULL			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x12, 0x01, 0x01, 0x01, "2"			},
	{0x12, 0x01, 0x01, 0x00, "4"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x12, 0x01, 0x06, 0x00, "None"			},
	{0x12, 0x01, 0x06, 0x06, "30000"		},
	{0x12, 0x01, 0x06, 0x04, "50000"		},
	{0x12, 0x01, 0x06, 0x02, "70000"		},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x12, 0x01, 0x08, 0x08, "Easy"			},
	{0x12, 0x01, 0x08, 0x00, "Difficult"		},

	{0   , 0xfe, 0   ,    2, "Timer(Don't Change)"	},
	{0x12, 0x01, 0x10, 0x10, "Timer decrease"	},
	{0x12, 0x01, 0x10, 0x00, "Timer infinity"	},
};

STDDIPINFOEXT(Cscrtry,	Decocass,	Cscrtry)

static struct BurnDIPInfo CoozumouDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x12, 0x01, 0x01, 0x01, "2"			},
	{0x12, 0x01, 0x01, 0x00, "4"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x12, 0x01, 0x06, 0x00, "None"			},
	{0x12, 0x01, 0x06, 0x06, "30000"		},
	{0x12, 0x01, 0x06, 0x04, "50000"		},
	{0x12, 0x01, 0x06, 0x02, "70000"		},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x12, 0x01, 0x08, 0x08, "Easy"			},
	{0x12, 0x01, 0x08, 0x00, "Difficult"		},

	{0   , 0xfe, 0   ,    2, "Timer(Don't Change)"	},
	{0x12, 0x01, 0x10, 0x10, "Timer decrease"	},
	{0x12, 0x01, 0x10, 0x00, "Timer infinity"	},
};

STDDIPINFOEXT(Coozumou,	Decocass,	Coozumou)

static struct BurnDIPInfo CfghticeDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x03, NULL			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x12, 0x01, 0x01, 0x01, "3"			},
	{0x12, 0x01, 0x01, 0x00, "5"			},

	{0   , 0xfe, 0   ,    4, "Very Difficult"	},
	{0x12, 0x01, 0x06, 0x04, "Very Easy"		},
	{0x12, 0x01, 0x06, 0x06, "Easy"			},
	{0x12, 0x01, 0x06, 0x00, "Difficult"		},
	{0x12, 0x01, 0x06, 0x02, "Very Difficult"	},

	{0   , 0xfe, 0   ,    2, "Enemy's Speed"	},
	{0x12, 0x01, 0x08, 0x08, "Normal"		},
	{0x12, 0x01, 0x08, 0x00, "Fast"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x12, 0x01, 0x10, 0x10, "Off"			},
	{0x12, 0x01, 0x10, 0x00, "On"			},
};

STDDIPINFOEXT(Cfghtice,	Decocass,	Cfghtice)

static struct BurnDIPInfo CbdashDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x03, NULL			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x12, 0x01, 0x01, 0x01, "3"			},
	{0x12, 0x01, 0x01, 0x00, "5"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x12, 0x01, 0x06, 0x00, "None"			},
	{0x12, 0x01, 0x06, 0x06, "20000"		},
	{0x12, 0x01, 0x06, 0x04, "30000"		},
	{0x12, 0x01, 0x06, 0x02, "40000"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x12, 0x01, 0x18, 0x18, "Normal"		},
	{0x12, 0x01, 0x18, 0x10, "Hard"			},
	{0x12, 0x01, 0x18, 0x08, "Harder"		},
	{0x12, 0x01, 0x18, 0x00, "Hardest"		},
};

STDDIPINFOEXT(Cbdash,	Decocass,	Cbdash)

static struct BurnDIPInfo CfishingDIPList[]=
{
	{0x12, 0xff, 0xff, 0xef, NULL			},
	{0x13, 0xff, 0xff, 0x03, NULL			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x12, 0x01, 0x01, 0x01, "3"			},
	{0x12, 0x01, 0x01, 0x00, "5"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x12, 0x01, 0x06, 0x00, "None"			},
	{0x12, 0x01, 0x06, 0x06, "10000"		},
	{0x12, 0x01, 0x06, 0x04, "20000"		},
	{0x12, 0x01, 0x06, 0x02, "30000"		},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x12, 0x01, 0x08, 0x08, "Easy"			},
	{0x12, 0x01, 0x08, 0x00, "Difficult"		},
};

STDDIPINFOEXT(Cfishing,	Decocass,	Cfishing)

static struct BurnDIPInfo CfishingjDIPList[]=
{
	{0x12, 0xff, 0xff, 0xef, NULL			},
	{0x13, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x12, 0x01, 0x01, 0x01, "3"			},
	{0x12, 0x01, 0x01, 0x00, "5"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x12, 0x01, 0x06, 0x00, "None"			},
	{0x12, 0x01, 0x06, 0x06, "10000"		},
	{0x12, 0x01, 0x06, 0x04, "20000"		},
	{0x12, 0x01, 0x06, 0x02, "30000"		},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x12, 0x01, 0x08, 0x08, "Easy"			},
	{0x12, 0x01, 0x08, 0x00, "Difficult"		},
};

STDDIPINFOEXT(Cfishingj,	Decocass,	Cfishingj)

static struct BurnDIPInfo Cfboy0a1DIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    2, "Number of The Deco Kids"	},
	{0x12, 0x01, 0x01, 0x01, "3"			},
	{0x12, 0x01, 0x01, 0x00, "5"			},

	{0   , 0xfe, 0   ,    4, "Bonus Points"		},
	{0x12, 0x01, 0x06, 0x06, "5000"			},
	{0x12, 0x01, 0x06, 0x04, "10000"		},
	{0x12, 0x01, 0x06, 0x02, "15000"		},
	{0x12, 0x01, 0x06, 0x00, "None"			},

	{0   , 0xfe, 0   ,    2, "Number of Alien Missiles"	},
	{0x12, 0x01, 0x08, 0x08, "Easy"			},
	{0x12, 0x01, 0x08, 0x00, "Difficult"		},

	{0   , 0xfe, 0   ,    2, "Alien Craft Movement"	},
	{0x12, 0x01, 0x10, 0x10, "Easy"			},
	{0x12, 0x01, 0x10, 0x00, "Difficult"		},
};

STDDIPINFOEXT(Cfboy0a1,	Decocass,	Cfboy0a1)

static struct BurnDIPInfo CflyballDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x02, NULL			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x12, 0x01, 0x01, 0x01, "3"			},
	{0x12, 0x01, 0x01, 0x00, "5"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x12, 0x01, 0x06, 0x00, "None"			},
	{0x12, 0x01, 0x06, 0x06, "20000"			},
	{0x12, 0x01, 0x06, 0x04, "50000"		},
	{0x12, 0x01, 0x06, 0x02, "70000"		},

	{0   , 0xfe, 0   ,    2, "Push Backs"		},
	{0x12, 0x01, 0x08, 0x08, "2"			},
	{0x12, 0x01, 0x08, 0x00, "3"			},
};

STDDIPINFOEXT(Cflyball,	Decocass,	Cflyball)

static struct BurnInputInfo CdsteljnInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"A",			BIT_DIGITAL,	DrvJoy5 + 0,	"mah a"		},
	{"B",			BIT_DIGITAL,	DrvJoy5 + 1,	"mah b"		},
	{"C",			BIT_DIGITAL,	DrvJoy5 + 2,	"mah c"		},
	{"D",			BIT_DIGITAL,	DrvJoy5 + 3,	"mah d"		},
	{"E",			BIT_DIGITAL,	DrvJoy5 + 4,	"mah e"		},
	{"F",			BIT_DIGITAL,	DrvJoy5 + 5,	"mah f"		},
	{"G",			BIT_DIGITAL,	DrvJoy5 + 6,	"mah g"		},
	{"H",			BIT_DIGITAL,	DrvJoy6 + 0,	"mah h"		},
	{"I",			BIT_DIGITAL,	DrvJoy6 + 1,	"mah i"		},
	{"J",			BIT_DIGITAL,	DrvJoy6 + 2,	"mah j"		},
	{"K",			BIT_DIGITAL,	DrvJoy6 + 3,	"mah k"		},
	{"L",			BIT_DIGITAL,	DrvJoy6 + 4,	"mah l"		},
	{"M",			BIT_DIGITAL,	DrvJoy6 + 5,	"mah m"		},
	{"N",			BIT_DIGITAL,	DrvJoy6 + 6,	"mah n"		},
	{"Pon",			BIT_DIGITAL,	DrvJoy7 + 1,	"mah pon"	},
	{"Chi",			BIT_DIGITAL,	DrvJoy7 + 0,	"mah chi"	},
	{"Kan",			BIT_DIGITAL,	DrvJoy7 + 2,	"mah kan"	},
	{"Ron",			BIT_DIGITAL,	DrvJoy7 + 4,	"mah ron"	},
	{"Reach",		BIT_DIGITAL,	DrvJoy7 + 3,	"mah reach"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"A",			BIT_DIGITAL,	DrvJoy9 + 0,	"mah a"		},
	{"B",			BIT_DIGITAL,	DrvJoy9 + 1,	"mah b"		},
	{"C",			BIT_DIGITAL,	DrvJoy9 + 2,	"mah c"		},
	{"D",			BIT_DIGITAL,	DrvJoy9 + 3,	"mah d"		},
	{"E",			BIT_DIGITAL,	DrvJoy9 + 4,	"mah e"		},
	{"F",			BIT_DIGITAL,	DrvJoy9 + 5,	"mah f"		},
	{"G",			BIT_DIGITAL,	DrvJoy9 + 6,	"mah g"		},
	{"H",			BIT_DIGITAL,	DrvJoy10 + 0,	"mah h"		},
	{"I",			BIT_DIGITAL,	DrvJoy10 + 1,	"mah i"		},
	{"J",			BIT_DIGITAL,	DrvJoy10 + 2,	"mah j"		},
	{"K",			BIT_DIGITAL,	DrvJoy10 + 3,	"mah k"		},
	{"L",			BIT_DIGITAL,	DrvJoy10 + 4,	"mah l"		},
	{"M",			BIT_DIGITAL,	DrvJoy10 + 5,	"mah m"		},
	{"N",			BIT_DIGITAL,	DrvJoy10 + 6,	"mah n"		},
	{"Pon",			BIT_DIGITAL,	DrvJoy11 + 1,	"mah pon"	},
	{"Chi",			BIT_DIGITAL,	DrvJoy11 + 0,	"mah chi"	},
	{"Kan",			BIT_DIGITAL,	DrvJoy11 + 2,	"mah kan"	},
	{"Ron",			BIT_DIGITAL,	DrvJoy11 + 4,	"mah ron"	},
	{"Reach",		BIT_DIGITAL,	DrvJoy11 + 3,	"mah reach"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Cdsteljn)

static struct BurnDIPInfo CdsteljnDIPList[]=
{
	{0x37, 0xff, 0xff, 0xff, NULL			},
	{0x38, 0xff, 0xff, 0xff, NULL			},
	{0x39, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x37, 0x01, 0x03, 0x00, "2 Coins 1 Credits"	},
	{0x37, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x37, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},
	{0x37, 0x01, 0x03, 0x01, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x37, 0x01, 0x0c, 0x00, "2 Coins 1 Credits"	},
	{0x37, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"	},
	{0x37, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"	},
	{0x37, 0x01, 0x0c, 0x04, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Type of Tape"		},
	{0x37, 0x01, 0x30, 0x00, "MT (Big)"		},
	{0x37, 0x01, 0x30, 0x10, "invalid?"		},
	{0x37, 0x01, 0x30, 0x20, "invalid?"		},
	{0x37, 0x01, 0x30, 0x30, "MD (Small)"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x37, 0x01, 0x40, 0x00, "Upright"		},
	{0x37, 0x01, 0x40, 0x40, "Cocktail"		},

	{0   , 0xfe, 0   ,    6, "Country Code"		},
	{0x38, 0x01, 0xe0, 0xe0, "A"			},
	{0x38, 0x01, 0xe0, 0xc0, "B"			},
	{0x38, 0x01, 0xe0, 0xa0, "C"			},
	{0x38, 0x01, 0xe0, 0x80, "D"			},
	{0x38, 0x01, 0xe0, 0x60, "E"			},
	{0x38, 0x01, 0xe0, 0x40, "F"			},

	{0   , 0xfe, 0   ,    4, "Bios Version"		},
	{0x39, 0x01, 0x03, 0x00, "Japan A, Newer"	},
	{0x39, 0x01, 0x03, 0x01, "Japan A, Older"	},
	{0x39, 0x01, 0x03, 0x02, "USA B, Newer"		},
	{0x39, 0x01, 0x03, 0x03, "USA B, Older"		},
};

STDDIPINFO(Cdsteljn)

#define TAPE_CLOCKRATE	4800
#define TAPE_LEADER 	TAPE_CLOCKRATE
#define TAPE_GAP		TAPE_CLOCKRATE*3/2
#define TAPE_HOLE		TAPE_CLOCKRATE/400
#define TAPE_PRE_GAP	34
#define TAPE_LEADIN 	(TAPE_PRE_GAP + 1)
#define TAPE_HEADER 	(TAPE_LEADIN + 1)
#define TAPE_BLOCK		(TAPE_HEADER + 256)
#define TAPE_CRC16_MSB	(TAPE_BLOCK + 1)
#define TAPE_CRC16_LSB	(TAPE_CRC16_MSB + 1)
#define TAPE_TRAILER	(TAPE_CRC16_LSB + 1)
#define TAPE_LEADOUT	(TAPE_TRAILER + 1)
#define TAPE_LONGCLOCK	(TAPE_LEADOUT + 1)
#define TAPE_POST_GAP	(TAPE_LONGCLOCK + 34)
#define TAPE_CHUNK		TAPE_POST_GAP

static double tape_time0 = 0;
static INT32 tape_speed = 0;
static INT32 tape_timer = 0;
static INT32 tape_dir = 0;

static INT64 tape_freerun = 0; // continuously counts up from mcu clock

static double tapetimer()
{
	return tape_freerun * (1.0 / 500000);
}

static void tapetimer_reset()
{
	tape_freerun = 0;
}

static INT32 firsttime = 1;
static INT32 tape_blocks;
static INT32 tape_length;
static INT32 tape_bot_eot;
static UINT8 crc16_lsb;
static UINT8 crc16_msb;
static UINT8 tape_crc16_lsb[256];
static UINT8 tape_crc16_msb[256];

static void tape_crc16(UINT8 data)
{
	UINT8 c0, c1;
	UINT8 old_lsb = crc16_lsb;
	UINT8 old_msb = crc16_msb;
	UINT8 feedback;

	feedback = ((data >> 7) ^ crc16_msb) & 1;

	c0 = crc16_lsb & 1;
	c1 = crc16_msb & 1;
	crc16_msb = (crc16_msb >> 1) | (c0 << 7);
	crc16_lsb = (crc16_lsb >> 1) | (c1 << 7);

	if (feedback)
		crc16_lsb |= 0x80;
	else
		crc16_lsb &= ~0x80;

	if (((old_lsb >> 6) ^ feedback) & 1)
		crc16_lsb |= 0x20;
	else
		crc16_lsb &= ~0x20;

	if (((old_msb >> 1) ^ feedback) & 1)
		crc16_msb |= 0x01;
	else
		crc16_msb &= ~0x01;
}

static void tape_update(void)
{
	INT32 offset, rclk, rdata, tape_bit, tape_byte, tape_block;
	double tape_time = tape_time0;

	if (tape_timer) tape_time += tape_dir * tapetimer();

	if (tape_time < 0.0) tape_time = 0.0;
	else if (tape_time > 999.9) tape_time = 999.9;

	offset = (int)(tape_time * TAPE_CLOCKRATE + 0.499995);

	rclk = 0;
	rdata = 0;

	if (offset < TAPE_LEADER)	// LEADER area
	{
		if (offset < 0) offset = 0;

		if (tape_bot_eot == 0)
		{
			tape_bot_eot = 1;
		}
	}
	else
	if (offset < TAPE_LEADER + TAPE_GAP)	// GAP between LEADER and BOT hole 
	{
		if (tape_bot_eot == 1)
		{
			tape_bot_eot = 0;
		}
	}
	else
	if (offset < TAPE_LEADER + TAPE_GAP + TAPE_HOLE)	// during BOT hole
	{
		if (tape_bot_eot == 0)
		{
			tape_bot_eot = 1;
		}
	}
	else
	if (offset < tape_length - TAPE_LEADER - TAPE_GAP - TAPE_HOLE)
	{
		offset -= TAPE_LEADER + TAPE_GAP + TAPE_HOLE;

		if (tape_bot_eot == 1)	// data area
		{
			tape_bot_eot = 0;
		}
		rclk = (offset ^ 1) & 1;
		tape_bit = (offset / 2) % 8;
		tape_byte = (offset / 16) % TAPE_CHUNK;
		tape_block = offset / 16 / TAPE_CHUNK;

		if (tape_byte < TAPE_PRE_GAP)
		{
			rclk = 0;
			rdata = 0;
		}
		else if (tape_byte < TAPE_LEADIN)
		{
			rdata = (0x00 >> tape_bit) & 1;
		}
		else if (tape_byte < TAPE_HEADER)
		{
			rdata = (0xaa >> tape_bit) & 1;
		}
		else if (tape_byte < TAPE_BLOCK)
		{
			UINT8 *ptr = DrvCassette + tape_block * 256 + tape_byte - TAPE_HEADER;
			rdata = (*ptr >> tape_bit) & 1;
		}
		else if (tape_byte < TAPE_CRC16_MSB)
		{
			rdata = (tape_crc16_msb[tape_block] >> tape_bit) & 1;
		}
		else if (tape_byte < TAPE_CRC16_LSB)
		{
			rdata = (tape_crc16_lsb[tape_block] >> tape_bit) & 1;
		}
		else if (tape_byte < TAPE_TRAILER)
		{
			rdata = (0xaa >> tape_bit) & 1;
		}
		else if (tape_byte < TAPE_LEADOUT)
		{
			rdata = (0x00 >> tape_bit) & 1;
		}
		else if (tape_byte < TAPE_LONGCLOCK)
		{
			rclk = 1;
			rdata = 0;
		}
	}
	else if (offset < tape_length - TAPE_LEADER - TAPE_GAP)// during EOT hole
	{
		if (tape_bot_eot == 0)
		{
			tape_bot_eot = 1;
		}
	}
	else if (offset < tape_length - TAPE_LEADER)	// GAP between EOT and trailer
	{
		if (tape_bot_eot == 1)
		{
			tape_bot_eot = 0;
		}
	}
	else	// TRAILER area
	{
		if (tape_bot_eot == 0)
		{
			tape_bot_eot = 1;

		}
		offset = tape_length - 1;
	}

	i8041_p2 = (i8041_p2 & ~0xe0) | (tape_bot_eot << 5) | (rclk << 6) | (rdata << 7);
}

static void tape_stop(void)
{
	if (tape_timer) {
		tape_time0 += tape_dir * tapetimer();
		tape_timer = 0; // timer off.
	}
}

static void decocass_init_common()
{
	UINT8 *image = DrvCassette;
	INT32 i, offs;

	tape_dir = 0;
	tape_speed = 0;
	tape_timer = 0;

	firsttime = 1;
	tape_blocks = 0;

	for (i = DrvCassetteLen / 256 - 1; !tape_blocks && i > 0; i--)
		for (offs = 256 * i; !tape_blocks && offs < 256 * i + 256; offs++)
			if (image[offs])
				tape_blocks = i+1;
	for (i = 0; i < tape_blocks; i++)
	{
		crc16_lsb = 0;
		crc16_msb = 0;
		for (offs = 256 * i; offs < 256 * i + 256; offs++)
		{
			tape_crc16(image[offs] << 7);
			tape_crc16(image[offs] << 6);
			tape_crc16(image[offs] << 5);
			tape_crc16(image[offs] << 4);
			tape_crc16(image[offs] << 3);
			tape_crc16(image[offs] << 2);
			tape_crc16(image[offs] << 1);
			tape_crc16(image[offs] << 0);
		}
		tape_crc16_lsb[i] = crc16_lsb;
		tape_crc16_msb[i] = crc16_msb;
	}

	tape_length = tape_blocks * TAPE_CHUNK * 8 * 2 + 2 * (TAPE_LEADER + TAPE_GAP + TAPE_HOLE);

	tapetimer_reset();

	tape_time0 = (double)(TAPE_LEADER + TAPE_GAP - TAPE_HOLE) / TAPE_CLOCKRATE;

	tape_bot_eot = 0;

	decocass_reset = 0;
	i8041_p1 = 0xff;
	i8041_p2 = 0xff;

	if (!type1_inmap) type1_inmap = MAKE_MAP(0,1,2,3,4,5,6,7);
	if (!type1_outmap) type1_outmap = MAKE_MAP(0,1,2,3,4,5,6,7);

	type2_d2_latch = 0;
	type2_xx_latch = 0;
	type2_promaddr = 0;

	type3_pal_19 = 0;
	type3_ctrs = 0;
	type3_d0_latch = 0;

	type4_ctrs = 0;
	type4_latch = 0;
}

static UINT8 decocass_type1_read(UINT16 offset)
{
	if (!type1_map)
		return 0x00;

	UINT8 data;

	if (1 == (offset & 1))
	{
		if (0 == (offset & E5XX_MASK))
			data = i8x41_get_register(I8X41_STAT);
		else
			data = 0xff;

		data = (BIT(data, 0) << 0) | (BIT(data, 1) << 1) | 0x7c;
	}
	else
	{
		INT32 promaddr;
		UINT8 save;
		UINT8 *prom = DrvDongle;

		if (firsttime)
		{
			firsttime = 0;
			type1_latch1 = 0;    /* reset latch (??) */
		}

		if (0 == (offset & E5XX_MASK))
			data = i8x41_get_register(I8X41_DATA);
		else
			data = 0xff;

		save = data;    /* save the unmodifed data for the latch */

		promaddr = 0;
		INT32 promshift = 0;

		for (INT32 i=0;i<8;i++)
		{
			if (type1_map[i] == T1PROM) { promaddr |= (((data >> T1MAP(i,type1_inmap)) & 1) << promshift); promshift++; }
		}

		data = 0;
		promshift = 0;

		for (INT32 i=0;i<8;i++)
		{
			if (type1_map[i] == T1PROM)     { data |= (((prom[promaddr] >> promshift) & 1)               << T1MAP(i,type1_outmap)); promshift++; }
			if (type1_map[i] == T1LATCHINV) { data |= ((1 - ((type1_latch1 >> T1MAP(i,type1_inmap)) & 1)) << T1MAP(i,type1_outmap)); }
			if (type1_map[i] == T1LATCH)    { data |= (((type1_latch1 >> T1MAP(i,type1_inmap)) & 1)    << T1MAP(i,type1_outmap)); }
			if (type1_map[i] == T1DIRECT)   { data |= (((save >> T1MAP(i,type1_inmap)) & 1)        << T1MAP(i,type1_outmap)); }
		}

		type1_latch1 = save;        /* latch the data for the next A0 == 0 read */
	}
	return data;
}

static UINT8 decocass_nodongle_read(UINT16 offset)
{
	if ((offset & 0x02) == 0)
	{
		return i8x41_get_register((offset & 1) ? I8X41_STAT : I8X41_DATA);
	}

	return 0xff;
}

static UINT8 decocass_type2_read(UINT16 offset)
{
	if (type2_xx_latch == 1)
	{
		if (offset & 1)
			return DrvDongle[256 * type2_d2_latch + type2_promaddr];

		return 0xff;
	}
	else
	{
		if ((offset & 0x02) == 0)
			return i8x41_get_register((offset & 1) ? I8X41_STAT : I8X41_DATA);

		return offset;
	}

	return 0;
}

static void decocass_type2_write(UINT16 offset, UINT8 data)
{
	if (type2_xx_latch == 1)
	{
		if ((offset & 1) == 0)
		{
			type2_promaddr = data;
			return;
		}
	}

	if (offset & 1)
	{
		if ((data & 0xf0) == 0xc0)
		{
			type2_xx_latch = 1;
			type2_d2_latch = (data & 0x04) ? 1 : 0;
		}
	}

	i8x41_set_register((offset & 1) ? I8X41_CMND : I8X41_DATA, data);
}

static UINT8 decocass_type3_read(UINT16 offset)
{
	UINT8 data, save;

	if (1 == (offset & 1))
	{
		if (1 == type3_pal_19)
		{
			UINT8 *prom = DrvDongle;
			data = prom[type3_ctrs];
			if (++type3_ctrs == 4096)
				type3_ctrs = 0;
		}
		else
		{
			if (0 == (offset & E5XX_MASK))
			{
				data = i8x41_get_register(1 ? I8X41_STAT : I8X41_DATA);
			}
			else
			{
				data = 0xff;    /* open data bus? */
			}
		}
	}
	else
	{
		if (1 == type3_pal_19)
		{
			save = data = 0xff;    /* open data bus? */
		}
		else
		{
			if (0 == (offset & E5XX_MASK))
			{
				save = i8x41_get_register(0 ? I8X41_STAT : I8X41_DATA);

				switch (type3_swap)
				{
				case TYPE3_SWAP_01:
					data =
						(BIT(save, 1) << 0) |
						(type3_d0_latch << 1) |
						(BIT(save, 2) << 2) |
						(BIT(save, 3) << 3) |
						(BIT(save, 4) << 4) |
						(BIT(save, 5) << 5) |
						(BIT(save, 6) << 6) |
						(BIT(save, 7) << 7);
					break;
				case TYPE3_SWAP_12:
					data =
						(type3_d0_latch << 0) |
						(BIT(save, 2) << 1) |
						(BIT(save, 1) << 2) |
						(BIT(save, 3) << 3) |
						(BIT(save, 4) << 4) |
						(BIT(save, 5) << 5) |
						(BIT(save, 6) << 6) |
						(BIT(save, 7) << 7);
					break;
				case TYPE3_SWAP_13:
					data =
						(type3_d0_latch << 0) |
						(BIT(save, 3) << 1) |
						(BIT(save, 2) << 2) |
						(BIT(save, 1) << 3) |
						(BIT(save, 4) << 4) |
						(BIT(save, 5) << 5) |
						(BIT(save, 6) << 6) |
						(BIT(save, 7) << 7);
					break;
				case TYPE3_SWAP_24:
					data =
						(type3_d0_latch << 0) |
						(BIT(save, 1) << 1) |
						(BIT(save, 4) << 2) |
						(BIT(save, 3) << 3) |
						(BIT(save, 2) << 4) |
						(BIT(save, 5) << 5) |
						(BIT(save, 6) << 6) |
						(BIT(save, 7) << 7);
					break;
				case TYPE3_SWAP_25:
					data =
						(type3_d0_latch << 0) |
						(BIT(save, 1) << 1) |
						(BIT(save, 5) << 2) |
						(BIT(save, 3) << 3) |
						(BIT(save, 4) << 4) |
						(BIT(save, 2) << 5) |
						(BIT(save, 6) << 6) |
						(BIT(save, 7) << 7);
					break;
				case TYPE3_SWAP_34_0:
					data =
						(type3_d0_latch << 0) |
						(BIT(save, 1) << 1) |
						(BIT(save, 2) << 2) |
						(BIT(save, 3) << 4) |
						(BIT(save, 4) << 3) |
						(BIT(save, 5) << 5) |
						(BIT(save, 6) << 6) |
						(BIT(save, 7) << 7);
					break;
				case TYPE3_SWAP_34_7:
					data =
						(BIT(save, 7) << 0) |
						(BIT(save, 1) << 1) |
						(BIT(save, 2) << 2) |
						(BIT(save, 4) << 3) |
						(BIT(save, 3) << 4) |
						(BIT(save, 5) << 5) |
						(BIT(save, 6) << 6) |
						(type3_d0_latch << 7);
					break;
				case TYPE3_SWAP_45:
					data =
						type3_d0_latch |
						(BIT(save, 1) << 1) |
						(BIT(save, 2) << 2) |
						(BIT(save, 3) << 3) |
						(BIT(save, 5) << 4) |
						(BIT(save, 4) << 5) |
						(BIT(save, 6) << 6) |
						(BIT(save, 7) << 7);
					break;
				case TYPE3_SWAP_23_56:
					data =
						(type3_d0_latch << 0) |
						(BIT(save, 1) << 1) |
						(BIT(save, 3) << 2) |
						(BIT(save, 2) << 3) |
						(BIT(save, 4) << 4) |
						(BIT(save, 6) << 5) |
						(BIT(save, 5) << 6) |
						(BIT(save, 7) << 7);
					break;
				case TYPE3_SWAP_56:
					data =
						type3_d0_latch |
						(BIT(save, 1) << 1) |
						(BIT(save, 2) << 2) |
						(BIT(save, 3) << 3) |
						(BIT(save, 4) << 4) |
						(BIT(save, 6) << 5) |
						(BIT(save, 5) << 6) |
						(BIT(save, 7) << 7);
					break;
				case TYPE3_SWAP_67:
					data =
						type3_d0_latch |
						(BIT(save, 1) << 1) |
						(BIT(save, 2) << 2) |
						(BIT(save, 3) << 3) |
						(BIT(save, 4) << 4) |
						(BIT(save, 5) << 5) |
						(BIT(save, 7) << 6) |
						(BIT(save, 6) << 7);
					break;
				default:
					data =
						type3_d0_latch |
						(BIT(save, 1) << 1) |
						(BIT(save, 2) << 2) |
						(BIT(save, 3) << 3) |
						(BIT(save, 4) << 4) |
						(BIT(save, 5) << 5) |
						(BIT(save, 6) << 6) |
						(BIT(save, 7) << 7);
				}
				type3_d0_latch = save & 1;
			}
			else
			{
				save = 0xff;    /* open data bus? */
				data =
					type3_d0_latch |
					(BIT(save, 1) << 1) |
					(BIT(save, 2) << 2) |
					(BIT(save, 3) << 3) |
					(BIT(save, 4) << 4) |
					(BIT(save, 5) << 5) |
					(BIT(save, 6) << 7) |
					(BIT(save, 7) << 6);
				type3_d0_latch = save & 1;
			}
		}
	}

	return data;
}

static void decocass_type3_write(UINT16 offset,UINT8 data)
{
	if (1 == (offset & 1))
	{
		if (1 == type3_pal_19)
		{
			type3_ctrs = data << 4;
			return;
		}
		else if (0xc0 == (data & 0xf0))
			type3_pal_19 = 1;
	}

	i8x41_set_register((offset & 1) ? I8X41_CMND : I8X41_DATA, data);
}

static UINT8 decocass_type4_read(UINT16 offset)
{
	if (offset & 1)
	{
		if ((offset & E5XX_MASK) == 0)
		{
			return i8x41_get_register(I8X41_STAT);
		}
	}
	else
	{
		if (type4_latch)
		{
			UINT8 data = DrvDongle[type4_ctrs];
			type4_ctrs = (type4_ctrs + 1) & 0x7fff;
			return data;
		}
		else
		{
			if ((offset & E5XX_MASK) == 0)
			{
				return i8x41_get_register(I8X41_DATA);
			}
		}
	}

	return 0xff;
}

static void decocass_type4_write(UINT16 offset, UINT8 data)
{
	if (offset & 1)
	{
		if (type4_latch)
		{
			type4_ctrs = (type4_ctrs & 0x00ff) | ((data & 0x7f) << 8);
			return;
		}
		else if ((data & 0xf0) == 0xc0)
		{
			type4_latch = 1;
		}
	}
	else
	{
		if (type4_latch)
		{
			type4_ctrs = (type4_ctrs & 0xff00) | data;
			return;
		}
	}

	i8x41_set_register((offset & 1) ? I8X41_CMND : I8X41_DATA, data);
}

static UINT8 decocass_e5xx_read(UINT8 offset)
{
	if (2 == (offset & E5XX_MASK))
	{
		return  (BIT(i8041_p1, 7)   << 0) |   /* D0 = P17 - REQ/ */
			(BIT(i8041_p2, 0)   << 1) |   /* D1 = P20 - FNO/ */
			(BIT(i8041_p2, 1)   << 2) |   /* D2 = P21 - EOT/ */
			(BIT(i8041_p2, 2)   << 3) |   /* D3 = P22 - ERR/ */
			((tape_bot_eot)     << 4) |   /* D4 = BOT/EOT (direct from drive) */
			(1                  << 5) |   /* D5 floating input */
			(1                  << 6) |   /* D6 floating input */
			((0) << 7);                   /* D7 = cassette present (active low) */
	}
	else
	{
		if (prot_read) {
			return prot_read(offset);
		} else {
			return 0xff;
		}
	}

	return 0;
}

static void decocass_e5xx_write(UINT8 offset, UINT8 data)
{
	if (prot_write)
	{
		prot_write(offset, data);
		return;
	}

	if (0 == (offset & E5XX_MASK))
	{
		i8x41_set_register((offset & 1) ? I8X41_CMND : I8X41_DATA, data);
	}
}

static inline void decode_chars_one(INT32 offset)
{
	offset &= 0x1fff;

	UINT8 a = DrvCharRAM[0x0000 + offset];
	UINT8 b = DrvCharRAM[0x2000 + offset];
	UINT8 c = DrvCharRAM[0x4000 + offset];

	UINT8 *d = DrvCharExp + (offset * 8);

	for (INT32 bit = 0; bit < 8; bit++)
	{
		UINT8 p;
		p  = ((a >> bit) & 1) << 0;
		p |= ((b >> bit) & 1) << 1;
		p |= ((c >> bit) & 1) << 2;

		d[bit ^ 7] = p;
	}
}

static inline void decode_tiles_one(INT32 offset)
{
	offset &= 0x3ff;

	INT32 i = offset * 8;

	UINT8 a = DrvTileRAM[0x0000 + offset];
	UINT8 b = DrvTileRAM[0x0400 + offset] >> 4;
	UINT8 c = DrvTileRAM[0x0400 + offset];

	for (INT32 bit = 0; bit < 4; bit++)
	{
		INT32 z = 0xf ^ bit ^ ((i & 0x1e00) >> 1) ^ ((i & 0x78) << 1) ^ ((i & 0x180) >> 5);

		UINT8 p;
		p  = ((a >> bit) & 1) << 0;
		p |= ((b >> bit) & 1) << 1;
		p |= ((c >> bit) & 1) << 2;

		DrvTileExp[z] = p;
	}
}

static inline void decode_obj_one(INT32 offset)
{
	offset &= 0x3ff;

	for (INT32 bit = 0; bit < 8; bit++)
	{
		INT32 z = (offset * 8) + bit;

		z = (~z & 0x3f) | ((~z & 0x1f80) >> 1) | ((z & 0x40) << 6);

		DrvObjExp[z] = (DrvObjRAM[offset] >> bit) & 1;
	}
}

static void decode_ram_tiles()
{
	for (INT32 i = 0; i < 0x2000; i++)
		decode_chars_one(i);

	for (INT32 i = 0; i < 0x400; i++)
		decode_tiles_one(i);

	for (INT32 i = 0; i < 0x400; i++)
		decode_obj_one(i);

}

static void set_gfx_bank(INT32 bank)
{
	e900_gfxbank = bank;

	if (bank == 3 || !e900_enable) return;

	UINT8 *ptr = DrvCharRAM; // bank == 0

	if (bank == 1) ptr = DrvGfxData;
	if (bank == 2) ptr = DrvGfxData + 0x5000;

	M6502MapMemory(ptr, 0x6000, 0xafff, MAP_ROM);
}

static void decocass_main_write(UINT16 address, UINT8 data)
{
	//bprintf (0, _T("MW %4.4x, %2.2x\n"), address, data);

	if (address >= 0x6000 && address <= 0xbfff) {
		DrvCharRAM[address - 0x6000] = data;
		decode_chars_one(address - 0x6000);
		return;
	}

	if ((address & 0xf800) == 0xd000) {
		DrvTileRAM[address & 0x7ff] = data;
		decode_tiles_one(address);
		return;
	}

	if ((address & 0xfc00) == 0xd800) {
		DrvObjRAM[address & 0x3ff] = data;
		decode_obj_one(address);
		return;
	}

	if ((address & 0xfc00) == 0xc800) {
		address = ((address >> 5) & 0x1f) | ((address & 0x1f) << 5);
		DrvFgRAM[address] = data;
		return;
	}

	if ((address & 0xfc00) == 0xcc00) {
		address = ((address >> 5) & 0x1f) | ((address & 0x1f) << 5);
		DrvColRAM[address] = data;
		return;
	}

	if ((address & 0xff00) == 0xe000) {
		DrvPalRAM[address & 0xff] = data;

		INT32 offset = (address & 31) ^ 16;
		data ^= 0xff;

		UINT8 r = (data >>  0) & 7;
		UINT8 g = (data >>  3) & 7;
		UINT8 b = (data >>  6) & 3;

		r = (r << 5) | (r << 2) | (r >> 1);
		g = (g << 5) | (g << 2) | (g >> 1);
		b = (b << 6) | (b << 4) | (b << 2) | (b << 0);

		DrvPaletteTable[offset] = (r << 16) + (g << 8) + b;

		DrvRecalc = 1; // needs to traverse the entire table for duplicates etc.

		return;
	}

	if ((address & 0xff00) == 0xe500) {
		decocass_e5xx_write(address & 0xff, data);
		return;
	}

	switch (address)
	{
		case 0xe300:
			watchdog_count = data & 0x0f;
		return;

		case 0xe301:
			watchdog_flip = data;
		return;

		case 0xe302:
			color_missiles = data & 0x77;
		return;

		case 0xe400:
		{
			decocass_reset = data;

			if (data & 1)
			{
				M6502Close();
				M6502Open(1);
				M6502Reset();
				M6502Close();
				M6502Open(0);

				audio_nmi_enabled = 0;

				M6502SetIRQLine(0x20, (audio_nmi_enabled && audio_nmi_state) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
			}

			if ((data & 8) ^ 8)
			{
				i8x41_reset();
			}
		}
		return;

		case 0xe402:
			mode_set = data;
		return;

		case 0xe403:
			back_h_shift = data;
		return;

		case 0xe404:
			back_vl_shift = data;
		return;

		case 0xe405:
			back_vr_shift = data;
		return;

		case 0xe406:
			part_h_shift = data;
		return;

		case 0xe407:
			part_v_shift = data;
		return;

		case 0xe410:
			color_center_bot = data;
		return;

		case 0xe411:
			center_h_shift_space = data;
		return;

		case 0xe412:
			center_v_shift = data;
		return;

		case 0xe413:
			// coin counter (not emulated)
			mux_data = (data & 0xc) >> 2; // cdsteljn
		return;

		case 0xe414:
		{
			soundlatch = data;
			sound_ack |= 0x80;
			sound_ack &= ~0x40;
			M6502Close();
			M6502Open(1);
			M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
			M6502Close();
			M6502Open(0);
		}
		return;

		case 0xe415:
		case 0xe416:
			// Unused analog stuff.
		return;

		case 0xe417:
			M6502SetIRQLine(0x20, CPU_IRQSTATUS_NONE);
		return;

		case 0xe420:
		case 0xe421:
		case 0xe422:
		case 0xe423:
		case 0xe424:
		case 0xe425:
		case 0xe426:
		case 0xe427:
		case 0xe428:
		case 0xe429:
		case 0xe42a:
		case 0xe42b:
		case 0xe42c:
		case 0xe42d:
		case 0xe42e:
		case 0xe42f:
			// adc_w (UNEMULATED)
		return;

		case 0xe900:
			set_gfx_bank(data & 3);
		return;
	}

	bprintf (0, _T("MW %4.4x, %2.2x\n"), address, data);
}

static UINT8 input_read(INT32 offset)
{
	offset &= 7;

	switch (offset & 7)
	{
		case 0:
		case 1:
		case 2:
			return DrvInputs[offset];

		case 3:
		case 4:
		case 5:
		case 6:
			return 0; // Unused analog stuff.
	}

	return 0xff;
}

static UINT8 decocass_main_read(UINT16 address)
{
	if ((address & 0xfc00) == 0xc800) {
		address = ((address >> 5) & 0x1f) | ((address & 0x1f) << 5);
		return DrvFgRAM[address];
	}

	if ((address & 0xfc00) == 0xcc00) {
		address = ((address >> 5) & 0x1f) | ((address & 0x1f) << 5);
		return DrvColRAM[address];
	}

	if ((address & 0xff00) == 0xe500) {
		return decocass_e5xx_read(address&0xff);
	}

	if ((address & 0xff00) == 0xe600) {
		if ((BurnDrvGetGenreFlags() & GBF_MAHJONG) && (address & 6) == 0)
			return DrvInputs[3 + mux_data + ((address & 1) * 4)];
		return input_read(address);
	}

	switch (address)
	{
		case 0xe300:
			return (DrvDips[0] & 0x7f) | (vblank ? 0x80 : 0);

		case 0xe301:
			return DrvDips[1];

		case 0xe414:
			return 0xc0; // sound command (0xc0 ok)

		case 0xe700:
			return soundlatch2;

		case 0xe701:
			return sound_ack;
	}

	bprintf (0, _T("MR %4.4x\n"), address);

	return 0;
}

// BurgerTime Anti-migraine 8910fixer Hack.  See d_btime.cpp for notes.
static UINT8 last01[3] = {0xff,0xff};
static UINT8 last02[3] = {0xff,0xff};
static UINT8 ignext = 0;

static void checkhiss_add02(UINT8 data)
{
	last02[1] = last02[0];
	last02[0] = data;
}

static void checkhiss_and_add01(UINT8 data)
{
	last01[1] = last01[0];
	last01[0] = data;

	if (last01[0] == 0 && last02[0] == 1 && last01[1] == 0 && last02[1] == 0)
	{
		ignext = 1; // next command will be VolA
	}
	if (last01[0] == 0 && last02[0] == 3 && last01[1] == 0 && last02[1] == 2)
	{
		ignext = 1; // next command will be VolB
	}
	if (last01[0] == 0 && last02[0] == 5 && last01[1] == 0 && last02[1] == 4)
	{
		ignext = 1; // next command will be VolC
	}
}

static void decocass_sound_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf800) == 0x1000) {
		audio_nmi_enabled = 1;
		M6502SetIRQLine(0x20, (audio_nmi_enabled && audio_nmi_state) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
		return;
	}

	if ((address & 0xf800) == 0x1800) {
		sound_ack &= ~0x40;
		return;
	}

	switch (address & 0xf000)
	{
		case 0x2000:
			if (burgertime_mode && ignext) {
				data = 0; // set volume to 0
				ignext = 0;
			}
			AY8910Write(0, 1, data);
			if (burgertime_mode) checkhiss_and_add01(data);
		return;

		case 0x4000:
			AY8910Write(0, 0, data);
			if (burgertime_mode) checkhiss_add02(data);
		return;

		case 0x6000:
			AY8910Write(1, 1, data);
		return;

		case 0x8000:
			AY8910Write(1, 0, data);
		return;

		case 0xc000:
			soundlatch2 = data;
			sound_ack |= 0x40;
		return;
	}
}

static UINT8 decocass_sound_read(UINT16 address)
{
	if ((address & 0xf800) == 0x1000) {
		audio_nmi_enabled = 1;
		M6502SetIRQLine(0x20, (audio_nmi_enabled && audio_nmi_state) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
		return 0xff;
	}

	if ((address & 0xf800) == 0x1800) {
		sound_ack &= ~0x40;
		return 0xFF;
	}

	switch (address & 0xf000)
	{
		case 0xa000:
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
			sound_ack &= ~0x80;
			return soundlatch;
	}


	return 0;
}


// i8041 tape intf

static void i8041_p1_write(UINT8 data)
{
	if ((data ^ i8041_p1) & 0x10)
	{
		tape_stop(); // iq_132

		if (0 == (data & 0x10))
		{
			tape_dir = -1;
			tape_timer = 1;
			tapetimer_reset();
		}
		else
		{
			tape_dir = 0;
			tape_speed = 0;
		}
	}

	if ((data ^ i8041_p1) & 0x20)
	{
		tape_stop();

		if (0 == (data & 0x20))
		{
			tape_dir = +1;
			tape_timer = 1;
			tapetimer_reset();
		}
		else
		{
			tape_dir = 0;
			tape_speed = 0;
		}
	}


	if (tape_timer && (data ^ i8041_p1) & 0x04)
	{
		tape_stop();
		tape_speed = (0 == (data & 0x04)) ? 1 : 0;

		if (tape_dir < 0)
		{
			tape_dir = (tape_speed) ? -7 : -1;
			tape_timer = 1;
			tapetimer_reset();
		}
		else
		if (tape_dir > 0)
		{
			tape_dir = (tape_speed) ? +7 : +1;
			tape_timer = 1;
			tapetimer_reset();
		}
	}

	i8041_p1 = data;
}

static UINT8 i8041_p1_read()
{
	return i8041_p1;
}

static void i8041_p2_write(UINT8 data)
{
	i8041_p2 = data;
}

static UINT8 i8041_p2_read()
{
	tape_update();

	return i8041_p2;
}

static UINT8 i8x41_read_ports(UINT16 port)
{
	switch (port)
	{
		case 0x01:
			return i8041_p1_read();

		case 0x02:
			return i8041_p2_read();
	}

	return 0;
}

static void i8x41_write_ports(UINT16 port, UINT8 data)
{
	switch (port)
	{
		case 0x01:
			i8041_p1_write(data);
		return;

		case 0x02:
			i8041_p2_write(data);
		return;
	}
}

static tilemap_scan( fg )
{
	return (31 - col) * 32 + row;
}

static tilemap_callback( fg )
{
	UINT16 code = DrvFgRAM[offs] + ((DrvColRAM[offs] & 3) * 256);

	TILE_SET_INFO(1, code, color_center_bot & 1, 0);
}

static INT32 BurnGetRomLen(INT32 nIndex)
{
	struct BurnRomInfo ri;
	BurnDrvGetRomInfo(&ri, nIndex);
	return ri.nLen;
}

static INT32 DrvDoReset()
{
	// load bios roms
	// call before cpu resets to catch vectors!
	if (DrvDips[2] != 0xff) // widel multi
	{
		INT32 bios_sets = 4; // total bios sets supported
		INT32 bios_select = ((DrvDips[2] % bios_sets) * 8) + 0x80;

		if (BurnLoadRom(DrvMainBIOS, bios_select + 0, 1)) return 1; // main m6502 bios

		if (BurnGetRomLen(bios_select + 1)) { // older bios' use split main rom
			if (BurnLoadRom(DrvMainBIOS + 0x800, bios_select + 1, 1)) return 1;
		}

		// split type uses 1k rather than 2k rom
		if (BurnLoadRom(DrvSoundBIOS + ((DrvDips[2] & 1) ? 0x400 : 0), bios_select + 2, 1)) return 1;

		// load the mcu bios
		if (BurnLoadRom(DrvMCUROM, 0x80 + (bios_sets * 8), 1)) return 1;
	}

	memset (AllRam, 0, RamEnd - AllRam);

	M6502Open(0);
	set_gfx_bank(0);
	M6502Reset();
	M6502Close();

	M6502Open(1);
	M6502Reset();
	M6502Close();

	i8x41_reset();

	AY8910Reset(0);
	AY8910Reset(1);

	decocass_reset = 0;

	watchdog_count = 0;
	watchdog_flip = 0;
	watchdog = 0;

	soundlatch = 0;
	soundlatch2 = 0;
	sound_ack = 0;
	audio_nmi_enabled = 0;
	audio_nmi_state = 0;

	color_missiles = 0;
	mode_set = 0;
	color_center_bot = 0;
	back_h_shift = 0;
	back_vl_shift = 0;
	back_vr_shift = 0;
	part_h_shift = 0;
	part_v_shift = 0;
	center_h_shift_space = 0;
	center_v_shift = 0;

	mux_data = 0;

	decocass_init_common();

	DrvInputs[2] = 0xc0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvMainBIOS		= Next; Next += 0x001000;
	DrvSoundBIOS		= Next; Next += 0x001000;
	DrvCassette		= Next; Next += 0x0020000;
	DrvGfxData		= Next; Next += 0x00a0000;
	DrvDongle		= Next; Next += 0x0100000; // 1mb (needed for widel multi)
	I8x41Mem		= Next;
	DrvMCUROM		= Next; Next += 0x0009000; // 0x400 + 0x500 for ram, for now

	DrvCharExp		= Next; Next += 0x0100000;
	DrvTileExp		= Next; Next += 0x0011000;
	DrvObjExp		= Next; Next += 0x0008000;

	DrvPalette		= (UINT32*)Next; Next += 0x00400 * sizeof(UINT32);

	DrvPalLut		= Next; Next += 0x0000400;
	DrvPaletteTable		= (UINT32*)Next; Next += 0x00200 * sizeof(UINT32);

	pTempDraw[0]		= (UINT16*)Next; Next += 512 * 512 * sizeof(UINT16);
	pTempDraw[1]		= (UINT16*)Next; Next += 512 * 512 * sizeof(UINT16);

	AllRam			= Next;

	DrvMainRAM		= Next; Next += 0x006000;
	DrvCharRAM		= Next; Next += 0x006000;
	DrvFgRAM		= Next; Next += 0x000400;
	DrvColRAM		= Next; Next += 0x000400;
	DrvTileRAM		= Next; Next += 0x000800;
	DrvObjRAM		= Next; Next += 0x000400;

	DrvPalRAM		= Next; Next += 0x000100;
	DrvPaletteTable = (UINT32*)Next; Next += 0x00200 * sizeof(UINT32);

	DrvSoundRAM		= Next; Next += 0x001000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void BurnPaletteLut()
{
	for (INT32 i = 0; i < 32; i++) { // char/sprite layer
		DrvPalLut[i] = i;
		DrvPalLut[i+32] = BITSWAP08(i, 7,6,5,4,3,1,2,0);
	}
}

static INT32 DecocassGetRoms()
{
	char* pRomName;
	struct BurnRomInfo ri;

	UINT8 *gfx = DrvGfxData;
	UINT8 *dnl = DrvDongle;
	UINT8 *mbi = DrvMainBIOS;
	UINT8 *sbi = DrvSoundBIOS;

	memset (gfx, 0xff, 0xa000);

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++)
	{
		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & BRF_PRG) && (ri.nType & 0x0f) == 1) {
			if (BurnLoadRom(dnl, i, 1)) return 1;
			dnl += ri.nLen;
			continue;
		}

		if ((ri.nType & BRF_PRG) && (ri.nType & 0x0f) == 2) {
			if (BurnLoadRom(DrvCassette, i, 1)) return 1;
			DrvCassetteLen = BurnGetRomLen(i);
			continue;
		}

		if ((ri.nType & BRF_GRA) && (ri.nType & 0x0f) == 3) {
			if (BurnLoadRom(gfx, i, 1)) return 1;
			gfx += ri.nLen;
			continue;
		}

	// widel multi
		if ((ri.nType & BRF_BIOS) && (ri.nType & 0x0f) == 8) {
			if (BurnLoadRom(mbi, i, 1)) return 1;
			if (ri.nLen != 0x1000 && mbi == DrvMainBIOS) memcpy (DrvMainBIOS + 0x800, DrvMainBIOS, 0x800);
			mbi += ri.nLen;
			continue;
		}

		if ((ri.nType & BRF_BIOS) && (ri.nType & 0x0f) == 9) {
			if (BurnLoadRom(sbi, i, 1)) return 1;
			if (ri.nLen != 0x800 && sbi == DrvSoundBIOS) memcpy (DrvSoundBIOS + 0x400, DrvSoundBIOS, 0x400);
			sbi += ri.nLen;
			continue;
		}

		if ((ri.nType & BRF_BIOS) && (ri.nType & 0x0f) == 10) {
			if (BurnLoadRom(DrvMCUROM, i, 1)) return 1;
			continue;
		}
	}

	return 0;
}

static INT32 DecocassInit(UINT8 (*read)(UINT16),void (*write)(UINT16,UINT8))
{
	BurnSetRefreshRate(57.44);

	prot_write = write;
	prot_read = read;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (DecocassGetRoms()) return 1;

		BurnPaletteLut();
		decocass_init_common();
	}

	M6502Init(0, TYPE_DECO222);
	M6502Open(0);
	M6502MapMemory(DrvMainRAM,	0x0000, 0x5fff, MAP_RAM);
	M6502MapMemory(DrvCharRAM,	0x6000, 0xbfff, MAP_ROM);
	M6502MapMemory(DrvFgRAM,	0xc000, 0xc3ff, MAP_RAM);
	M6502MapMemory(DrvColRAM,	0xc400, 0xc7ff, MAP_RAM);
	M6502MapMemory(DrvTileRAM,	0xd000, 0xd7ff, MAP_ROM);
	M6502MapMemory(DrvObjRAM,	0xd800, 0xdbff, MAP_ROM);
	M6502MapMemory(DrvPalRAM,	0xe000, 0xe0ff, MAP_ROM);
	M6502MapMemory(DrvMainBIOS,	0xf000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(decocass_main_write);
	M6502SetReadHandler(decocass_main_read);
	M6502Close();

	M6502Init(1, TYPE_M6502);
	M6502Open(1);
	M6502MapMemory(DrvSoundRAM,	0x0000, 0x0fff, MAP_RAM);
	M6502MapMemory(DrvSoundBIOS,	0xf800, 0xffff, MAP_ROM);
	M6502SetWriteHandler(decocass_sound_write);
	M6502SetReadHandler(decocass_sound_read);
	M6502Close();

	i8x41_init(NULL);
	i8x41_set_read_port(i8x41_read_ports);
	i8x41_set_write_port(i8x41_write_ports);

	AY8910Init(0, 1500000, 0);
	AY8910Init(1, 1500000, 1);
	AY8910SetAllRoutes(0, 0.20, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.20, BURN_SND_ROUTE_BOTH);
    AY8910SetBuffered(M6502TotalCycles, 500000);

	GenericTilesInit();
	GenericTilemapInit(2, fg_map_scan, fg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(1, DrvCharExp, 3, 8, 8, 0x10000, 0, 0x03);
	GenericTilemapSetOffsets(2, 0, -8);
	GenericTilemapSetTransparent(2,0);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6502Exit();
	i8x41_exit();

	AY8910Exit(0);
	AY8910Exit(1);

	BurnFree(AllMem);

	prot_write = NULL;
	prot_read = NULL;
	type1_inmap = 0;
	type1_outmap = 0;

	burgertime_mode = 0;
	skater_mode = 0;

	fourway_mode = 0;

	e900_enable = 0;

	return 0;
}

static void DrvPaletteUpdate()
{
	for (INT32 i = 0; i < 64; i++) {
		UINT32 p = DrvPaletteTable[DrvPalLut[i]];

		DrvPalette[i] = BurnHighCol(p >> 16, (p >> 8) & 0xff, p & 0xff, 0);
	}
}

static void draw_object()
{
	INT32 crossing = mode_set & 3;

	if ((crossing == 0 || BIT(mode_set, 6)) && !BIT(mode_set, 5)) // in daylight or during explosion
		return;

	INT32 color = (BITSWAP08(color_center_bot, 0, 1, 7, 2, 3, 4, 5, 6) & 0x27) | 0x08;

	INT32 sy = 64 - part_v_shift + ((skater_mode) ? 1 : 0);
	if (sy < 0)
		sy += 256;
	INT32 sx = part_h_shift - 128 + 1;

	const UINT8 *objdata0 = DrvObjExp + 0 * (64 * 64);
	const UINT8 *objdata1 = DrvObjExp + 1 * (64 * 64);

	for (INT32 y = 0; y < nScreenHeight; y++)
	{
		const INT32 dy = y - sy + 8;
		for (INT32 x = 0; x < nScreenWidth; x++)
		{
			const INT32 dx = x - sx;

			bool pri2 = false;
			INT32 scroll = back_h_shift;

			switch (crossing)
			{
				case 0: pri2 = true; break; // outside tunnel (for reference; not usually handled in this loop)
				case 1: pri2 = (x >= scroll); break; // exiting tunnel
				case 2: break; // inside tunnel
				case 3: pri2 = (x < scroll); break; // entering tunnel
			}

			if (BIT(mode_set, 7))
			{
				// check coordinates against object data
				if ((dy >= -64 && dy < 0 && dx >= 64 && dx < 128 && objdata0[((-1 - dy) * 64 + dx - 64) & 0xfff] != 0) ||
					(dy >= 0 && dy < 64 && dx >= 64 && dx < 128 && objdata0[(dy * 64 + dx - 64) & 0xfff] != 0) ||
					(dy >= -64 && dy < 0 && dx >= 0 && dx < 64 && objdata1[((-1 - dy) * 64 + dx) & 0xfff] != 0) ||
					(dy >= 0 && dy < 64 && dx >= 0 && dx < 64 && objdata1[(dy * 64 + dx) & 0xfff] != 0))
				{
					pri2 = true;
					if (BIT(mode_set, 5) && pPrioDraw[(y * nScreenWidth) + x] == 0) //priority.pix8(y, x) == 0) // least priority?
						pTransDraw[(y * nScreenWidth) + x] = color;
				}
			}

			if (!pri2)
				pTransDraw[(y * nScreenWidth) + x] |= 0x10;
		}
	}
}

static void draw_missiles(INT32 missile_y_adjust, INT32 missile_y_adjust_flip_screen, UINT8 *missile_ram, INT32 interleave)
{
	INT32 x;

	INT32 min_y = 0;
	INT32 max_y = nScreenHeight - 1;
	INT32 min_x = 0;
	INT32 max_x = nScreenWidth - 1;

	for (INT32 i = 0, offs = 0; i < 8; i++, offs += 4 * interleave)
	{
		INT32 sx, sy;

		sy = 255 - missile_ram[offs + 0 * interleave];
		sx = 255 - missile_ram[offs + 2 * interleave];
		if (flipscreen)
		{
			sx = 240 - sx;
			sy = 240 - sy + missile_y_adjust_flip_screen;
		}
		sy -= missile_y_adjust;
		if (sy >= min_y && sy <= max_y)
			for (x = 0; x < 4; x++)
			{
				if (sx >= min_x && sx <= max_x) {
					pTransDraw[(sy * nScreenWidth) + sx] = (color_missiles & 7) | 8;
					pPrioDraw[(sy * nScreenWidth) + sx] |= 1 << 2;
				}
				sx++;
			}

		sy = 255 - missile_ram[offs + 1 * interleave];
		sx = 255 - missile_ram[offs + 3 * interleave];
		if (flipscreen)
		{
			sx = 240 - sx;
			sy = 240 - sy + missile_y_adjust_flip_screen;
		}
		sy -= missile_y_adjust;
		if (sy >= min_y && sy <= max_y)
			for (x = 0; x < 4; x++)
			{
				if (sx >= min_x && sx <= max_x) {
					pTransDraw[(sy * nScreenWidth) + sx] = ((color_missiles >> 4) & 7) | 8;
					pPrioDraw[(sy * nScreenWidth) + sx] |= 1 << 3;
				}
				sx++;
			}
	}
}

static void draw_sprites(INT32 color, INT32 sprite_y_adjust, INT32 sprite_y_adjust_flip_screen, UINT8 *sprite_ram, INT32 interleave)
{
	for (INT32 i = 0, offs = 0; i < 8; i++, offs += 4 * interleave)
	{
		INT32 sx, sy, flipx, flipy;

		if (!(sprite_ram[offs + 0] & 0x01))
			continue;

		sx = 240 - sprite_ram[offs + 3 * interleave];
		sy = 240 - sprite_ram[offs + 2 * interleave];

		flipx = sprite_ram[offs + 0] & 0x04;
		flipy = sprite_ram[offs + 0] & 0x02;

		if (flipscreen)
		{
			sx = 240 - sx;
			sy = 240 - sy + sprite_y_adjust_flip_screen;

			flipx = !flipx;
			flipy = !flipy;
		}

		sy -= sprite_y_adjust;

		INT32 code = sprite_ram[offs + interleave];

		code *= 4;

		if (flipy) {
			if (flipx) {
				Render8x8Tile_Prio_Mask_FlipXY_Clip(pTransDraw, code + 1, sx + 0, sy + 0, color, 3, 0, 0, 1 << 1, DrvCharExp);
				Render8x8Tile_Prio_Mask_FlipXY_Clip(pTransDraw, code + 3, sx + 8, sy + 0, color, 3, 0, 0, 1 << 1, DrvCharExp);
				Render8x8Tile_Prio_Mask_FlipXY_Clip(pTransDraw, code + 0, sx + 0, sy + 8, color, 3, 0, 0, 1 << 1, DrvCharExp);
				Render8x8Tile_Prio_Mask_FlipXY_Clip(pTransDraw, code + 2, sx + 8, sy + 8, color, 3, 0, 0, 1 << 1, DrvCharExp);
			} else {
				Render8x8Tile_Prio_Mask_FlipY_Clip(pTransDraw, code + 3, sx + 0, sy + 0, color, 3, 0, 0, 1 << 1, DrvCharExp);
				Render8x8Tile_Prio_Mask_FlipY_Clip(pTransDraw, code + 1, sx + 8, sy + 0, color, 3, 0, 0, 1 << 1, DrvCharExp);
				Render8x8Tile_Prio_Mask_FlipY_Clip(pTransDraw, code + 2, sx + 0, sy + 8, color, 3, 0, 0, 1 << 1, DrvCharExp);
				Render8x8Tile_Prio_Mask_FlipY_Clip(pTransDraw, code + 0, sx + 8, sy + 8, color, 3, 0, 0, 1 << 1, DrvCharExp);
			}
		} else {
			if (flipx) {
				Render8x8Tile_Prio_Mask_FlipX_Clip(pTransDraw, code + 0, sx + 0, sy + 0, color, 3, 0, 0, 1 << 1, DrvCharExp);
				Render8x8Tile_Prio_Mask_FlipX_Clip(pTransDraw, code + 2, sx + 8, sy + 0, color, 3, 0, 0, 1 << 1, DrvCharExp);
				Render8x8Tile_Prio_Mask_FlipX_Clip(pTransDraw, code + 1, sx + 0, sy + 8, color, 3, 0, 0, 1 << 1, DrvCharExp);
				Render8x8Tile_Prio_Mask_FlipX_Clip(pTransDraw, code + 3, sx + 8, sy + 8, color, 3, 0, 0, 1 << 1, DrvCharExp);
			} else {
				Render8x8Tile_Prio_Mask_Clip(pTransDraw, code + 2, sx + 0, sy + 0, color, 3, 0, 0, 1 << 1, DrvCharExp);
				Render8x8Tile_Prio_Mask_Clip(pTransDraw, code + 0, sx + 8, sy + 0, color, 3, 0, 0, 1 << 1, DrvCharExp);
				Render8x8Tile_Prio_Mask_Clip(pTransDraw, code + 3, sx + 0, sy + 8, color, 3, 0, 0, 1 << 1, DrvCharExp);
				Render8x8Tile_Prio_Mask_Clip(pTransDraw, code + 1, sx + 8, sy + 8, color, 3, 0, 0, 1 << 1, DrvCharExp);
			}
		}

		sy += (flipscreen ? -256 : 256);

		// Wrap around
		if (flipy) {
			if (flipx) {
				Render8x8Tile_Prio_Mask_FlipXY_Clip(pTransDraw, code + 1, sx + 0, sy + 0, color, 3, 0, 0, 1 << 1, DrvCharExp);
				Render8x8Tile_Prio_Mask_FlipXY_Clip(pTransDraw, code + 3, sx + 8, sy + 0, color, 3, 0, 0, 1 << 1, DrvCharExp);
				Render8x8Tile_Prio_Mask_FlipXY_Clip(pTransDraw, code + 0, sx + 0, sy + 8, color, 3, 0, 0, 1 << 1, DrvCharExp);
				Render8x8Tile_Prio_Mask_FlipXY_Clip(pTransDraw, code + 2, sx + 8, sy + 8, color, 3, 0, 0, 1 << 1, DrvCharExp);
			} else {
				Render8x8Tile_Prio_Mask_FlipY_Clip(pTransDraw, code + 3, sx + 0, sy + 0, color, 3, 0, 0, 1 << 1, DrvCharExp);
				Render8x8Tile_Prio_Mask_FlipY_Clip(pTransDraw, code + 1, sx + 8, sy + 0, color, 3, 0, 0, 1 << 1, DrvCharExp);
				Render8x8Tile_Prio_Mask_FlipY_Clip(pTransDraw, code + 2, sx + 0, sy + 8, color, 3, 0, 0, 1 << 1, DrvCharExp);
				Render8x8Tile_Prio_Mask_FlipY_Clip(pTransDraw, code + 0, sx + 8, sy + 8, color, 3, 0, 0, 1 << 1, DrvCharExp);
			}
		} else {
			if (flipx) {
				Render8x8Tile_Prio_Mask_FlipX_Clip(pTransDraw, code + 0, sx + 0, sy + 0, color, 3, 0, 0, 1 << 1, DrvCharExp);
				Render8x8Tile_Prio_Mask_FlipX_Clip(pTransDraw, code + 2, sx + 8, sy + 0, color, 3, 0, 0, 1 << 1, DrvCharExp);
				Render8x8Tile_Prio_Mask_FlipX_Clip(pTransDraw, code + 1, sx + 0, sy + 8, color, 3, 0, 0, 1 << 1, DrvCharExp);
				Render8x8Tile_Prio_Mask_FlipX_Clip(pTransDraw, code + 3, sx + 8, sy + 8, color, 3, 0, 0, 1 << 1, DrvCharExp);
			} else {
				Render8x8Tile_Prio_Mask_Clip(pTransDraw, code + 2, sx + 0, sy + 0, color, 3, 0, 0, 1 << 1, DrvCharExp);
				Render8x8Tile_Prio_Mask_Clip(pTransDraw, code + 0, sx + 8, sy + 0, color, 3, 0, 0, 1 << 1, DrvCharExp);
				Render8x8Tile_Prio_Mask_Clip(pTransDraw, code + 3, sx + 0, sy + 8, color, 3, 0, 0, 1 << 1, DrvCharExp);
				Render8x8Tile_Prio_Mask_Clip(pTransDraw, code + 1, sx + 8, sy + 8, color, 3, 0, 0, 1 << 1, DrvCharExp);
			}
		}
	}
}

static void draw_center()
{
	INT32 min_y = 0;
	INT32 max_y = nScreenHeight - 1;

	INT32 sx, sy, x, y, color;

	color = 0;
	if (color_center_bot & 0x10)
		color |= 4;
	if (color_center_bot & 0x20)
		color |= 2;
	if (color_center_bot & 0x40)
		color |= 1;
	if (color_center_bot & 0x80)
		color = (color & 4) + ((color << 1) & 2) + ((color >> 1) & 1);

	sy = center_v_shift - 8;
	sx = (center_h_shift_space >> 2) & 0x3c;

	for (y = 0; y < 4; y++)
		if ((sy + y) >= min_y && (sy + y) <= max_y)
		{
			if (((sy + y) & color_center_bot & 3) == (sy & color_center_bot & 3))
				for (x = 0; x < 256; x++)
					if (0 != (x & 16) || 0 != (center_h_shift_space & 1))
						pTransDraw[(sy + y) * nScreenWidth + ((sx + x) & 255)] = color;
		}
}

static void draw_edge(INT32 which, bool opaque)
{
	INT32 scrolly_l = back_vl_shift + 8;
	INT32 scrolly_r = 256 - back_vr_shift + 8;

	// bit 0x04 of the mode select effectively selects between two banks of data
	if (0 == (mode_set & 0x04))
		scrolly_r += 256;
	else
		scrolly_l += 256;

	INT32 scrollx = 256 - back_h_shift;
	INT32 scrolly = (which) ? scrolly_r : scrolly_l;

	INT32 miny = (which) ? (nScreenHeight/2) : 0;
	INT32 maxy = (which) ? nScreenHeight : (nScreenHeight / 2);

	for (INT32 y=miny; y<maxy;y++)
	{
		INT32 srcline = (y + scrolly) & 0x1ff;
		UINT16* src = pTempDraw[which] + srcline * 512;
		UINT16* dst = pTransDraw + y * nScreenWidth;

		for (INT32 x=0; x<nScreenWidth;x++)
		{
			INT32 srccol = 0;

			// 2 bits control the x scroll mode, allowing it to wrap either half of the tilemap, or transition one way or the other between the two halves

			switch (mode_set & 3)
			{
				case 0x00: srccol = ((x + scrollx) & 0xff); break; // hwy normal case
				case 0x01: srccol = (x + scrollx + 0x100) & 0x1ff; break; // manhattan building top
				case 0x02: srccol = ((x + scrollx) & 0xff) + 0x100; break; // manhattan normal case
				case 0x03: srccol = (x + scrollx) & 0x1ff; break; // hwy, burnrub etc.
			}

			UINT16 pix = src[srccol];

			if ((pix & 0x3) || opaque)
			{
				if (pix!=0) dst[x] = pix; // if (pix!=0) because it needs to inherit the background fill from underneath it.
			}
		}
	}
}

static void predraw_bg()
{
	INT32 color = (color_center_bot >> 7) & 1;

	color = (color * 4) + 1;

	color = (color << 3);

	if (~nSpriteEnable & 2) memset(pTempDraw[0], 0, 512*512*sizeof(UINT16));
	if (~nSpriteEnable & 4) memset(pTempDraw[1], 0, 512*512*sizeof(UINT16));

	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sy = (offs & 0x1f) * 16;
		INT32 sx = (offs / 0x20) * 16;

		INT32 ofst = (offs & 7) ^ ((offs & 0x1e0) >> 2) ^ ((offs & 0x10) << 5) ^ ((offs & 0x200) >> 1) ^ 0x078;
		if (offs & 0x008) ofst ^= 0x087;

		INT32 code = DrvTileRAM[ofst] >> 4;

		{
			UINT8 *gfx0 = DrvTileExp + (code * 0x100);
			UINT8 *gfx1 = DrvTileExp + (code * 0x100) + (15 * 16);

			UINT16 *ds0 = pTempDraw[0] + (sy * 512) + sx;
			UINT16 *ds1 = pTempDraw[1] + (sy * 512) + sx;

			for (INT32 y = 0; y < 16; y++)
			{
				for (INT32 x = 0; x < 16; x++)
				{
					if (!(ofst & 0x80)) if (nSpriteEnable & 2) ds0[x] = gfx0[x] + color;
					if ( (ofst & 0x80)) if (nSpriteEnable & 4) ds1[x] = gfx1[x] + color;
				}
				ds0 += 512;
				ds1 += 512;
				gfx0 += 16;
				gfx1 -= 16;
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 0;
	}

	BurnTransferClear(8);

	predraw_bg();

	if (mode_set & 0x08)
	{
		draw_edge(0, true);
		draw_edge(1, true);
	}

	if (mode_set & 0x20)
	{
		if (nBurnLayer & 2) draw_center();
	}
	else
	{
		if (nBurnLayer & 2) draw_center();
		if (mode_set & 0x08)
		{
			draw_edge(0, false);
			draw_edge(1, false);
		}
	}

	if (nBurnLayer & 4) GenericTilemapDraw(2, pTransDraw, (skater_mode) ? 1 : 0);

	if (nSpriteEnable & 1) draw_sprites((color_center_bot >> 1) & 1, 8, 0, DrvFgRAM, 0x20);

	if (nBurnLayer & 8) draw_missiles(1+8, 0, DrvColRAM, 0x20);

	if (nBurnLayer & 1) draw_object();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if ((watchdog_flip & 0x04) == 0)
		watchdog = 0;
	else if (watchdog_count-- > 0)
		watchdog = 0;

	watchdog++;
	if (watchdog > 180) {
		M6502Open(0);
		M6502Reset();
		M6502Close();

		M6502Open(1);
		M6502Reset();
		M6502Close();

		watchdog = 0;
	}

	if (DrvReset) {
		DrvDoReset();
	}

	M6502NewFrame();

	{
		INT32 prev = DrvInputs[2] & 0xc0;

		memset (DrvInputs, 0, 11);

		DrvInputs[2] = 0xc0;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		if (fourway_mode) {
			ProcessJoystick(&DrvInputs[0], 0, 2,3,1,0, INPUT_4WAY | INPUT_CLEAROPPOSITES );
			ProcessJoystick(&DrvInputs[1], 1, 2,3,1,0, INPUT_4WAY | INPUT_CLEAROPPOSITES );
		}

		if (BurnDrvGetGenreFlags() & GBF_MAHJONG) {
			for (INT32 i = 0; i < 8; i++) {
				DrvInputs[ 3] ^= (DrvJoy4[i] & 1) << i;
				DrvInputs[ 4] ^= (DrvJoy5[i] & 1) << i;
				DrvInputs[ 5] ^= (DrvJoy6[i] & 1) << i;
				DrvInputs[ 6] ^= (DrvJoy7[i] & 1) << i;
				DrvInputs[ 7] ^= (DrvJoy8[i] & 1) << i;
				DrvInputs[ 8] ^= (DrvJoy9[i] & 1) << i;
				DrvInputs[ 9] ^= (DrvJoy10[i] & 1) << i;
				DrvInputs[10] ^= (DrvJoy11[i] & 1) << i;
			}
		}

		if (prev != (DrvInputs[2] & 0xc0) && (DrvInputs[2] & 0xc0) == 0xc0) {
			M6502Open(0);
			M6502SetIRQLine(0x20, CPU_IRQSTATUS_ACK);
			M6502Close();
		}
	}

	INT32 nInterleave = 272;
	INT32 nCyclesTotal[3] = { (INT32)((double)750000 / 57.444853), (INT32)((double)500000 / 57.444853), (INT32)((double)500000 / 57.444853) }; //1.5mhz -> .75mhz?
	INT32 nCyclesDone[3]  = { 0, 0, 0 };

	vblank = 1;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		M6502Open(0);
		INT32 nSegment = (i + 1) * nCyclesTotal[0] / nInterleave;
		nCyclesDone[0] += M6502Run(nSegment - nCyclesDone[0]);
		M6502Close();

		if (i == 248) vblank = 1;
		if (i == 8) {
			vblank = 0;
		}
		if (i == 248) {
			if (pBurnDraw) {
				BurnDrvRedraw();
			}
		}

		M6502Open(1);
		if (decocass_reset & 1)
		{
			nSegment = (i + 1) * nCyclesTotal[1] / nInterleave;
			nCyclesDone[1] += nSegment - nCyclesDone[1];
		}
		else
		{	
			nSegment = (i + 1) * nCyclesTotal[1] / nInterleave;
			nCyclesDone[1] += M6502Run(nSegment - nCyclesDone[1]);

			if ((i+1)%8 == 7)
			{
				audio_nmi_state = (i+1) & 8;
				M6502SetIRQLine(0x20, (audio_nmi_enabled && audio_nmi_state) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
			}
		}
		M6502Close();

		if (decocass_reset & 0x08)
		{
			nSegment = (i + 1) * nCyclesTotal[2] / nInterleave;
			INT32 derp = nSegment - nCyclesDone[2];
			nCyclesDone[2] += derp;
			tape_freerun += derp;
		}
		else
		{
			nSegment = (i + 1) * nCyclesTotal[2] / nInterleave;
			INT32 derp = i8x41_run(nSegment - nCyclesDone[2]);
			nCyclesDone[2] += derp;
			tape_freerun += derp;

		}
	}

	if (pBurnSoundOut) {
        AY8910Render(pBurnSoundOut, nBurnSoundLen);
	}

	return 0;
}


static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029719;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		memset(&ba, 0, sizeof(ba));
		ba.Data	  = I8x41Mem;
		ba.nLen	  = 0x900;
		ba.szName = "MCU Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		M6502Scan(nAction);
		i8x41_scan(nAction);

		AY8910Scan(nAction, pnMin);

		SCAN_VAR(watchdog_count);
		SCAN_VAR(watchdog_flip);
		SCAN_VAR(watchdog);

		SCAN_VAR(color_missiles);
		SCAN_VAR(mode_set);
		SCAN_VAR(color_center_bot);

		SCAN_VAR(back_h_shift);
		SCAN_VAR(back_vl_shift);
		SCAN_VAR(back_vr_shift);
		SCAN_VAR(part_h_shift);
		SCAN_VAR(part_v_shift);
		SCAN_VAR(center_h_shift_space);
		SCAN_VAR(center_v_shift);

		SCAN_VAR(flipscreen);

		SCAN_VAR(soundlatch);
		SCAN_VAR(sound_ack);
		SCAN_VAR(soundlatch2);
		SCAN_VAR(mux_data);

		SCAN_VAR(decocass_reset);
		SCAN_VAR(audio_nmi_enabled);
		SCAN_VAR(audio_nmi_state);

		SCAN_VAR(i8041_p1);
		SCAN_VAR(i8041_p2);

		SCAN_VAR(type1_latch1);

		SCAN_VAR(type2_d2_latch);
		SCAN_VAR(type2_xx_latch);
		SCAN_VAR(type2_promaddr);

		SCAN_VAR(type3_pal_19);
		SCAN_VAR(type3_ctrs);
		SCAN_VAR(type3_d0_latch);

		SCAN_VAR(type4_ctrs);
		SCAN_VAR(type4_latch);

		SCAN_VAR(tape_time0);
		SCAN_VAR(tape_speed);
		SCAN_VAR(tape_timer);
		SCAN_VAR(tape_dir);

		SCAN_VAR(tape_freerun);

		SCAN_VAR(firsttime);
		SCAN_VAR(tape_bot_eot);
	}

	if (nAction & ACB_WRITE) {
		decode_ram_tiles();
		M6502Open(0);
		set_gfx_bank(e900_gfxbank);
		M6502Close();
	}

	return 0;
}

// necessary for setting up bios relationship
static struct BurnRomInfo emptyRomDesc[] = {
	{ "", 0, 0, 0 },
};

// DECO Cassette System

static struct BurnRomInfo decocassRomDesc[] = {
	{ "v0a-.7e",   		0x1000, 0x3d33ac34, BRF_BIOS | BRF_PRG }, // 0x80 - BIOS "A" Japan (Main M6502)
	{ "",             	0x0000, 0x00000000, 0                  }, // 0x81

	{ "v1-.5a",     	0x0800, 0xb66b2c2a, BRF_BIOS | BRF_PRG }, // 0x82 - BIOS "A" Japan (Sound M6502)

	{ "v2.3m",		0x0020, 0x238fdb40, BRF_BIOS | BRF_OPT }, // 0x83 - BIOS "A" Japan (prom)
	{ "v4.10d",		0x0020, 0x3b5836b4, BRF_BIOS | BRF_OPT }, // 0x84 - BIOS "A" Japan (prom)
	{ "v3.3j",		0x0020, 0x51eef657, BRF_BIOS | BRF_OPT }, // 0x85 - BIOS "A" Japan (prom)

	{ "",             	0x0000, 0x00000000, 0                  }, // 0x86
	{ "",             	0x0000, 0x00000000, 0                  }, // 0x87

	{ "dsp-3_p0-a.m9",      0x0800, 0x2541e34b, BRF_BIOS | BRF_PRG }, // 0x88 - BIOS "A" Japan, Older (Main M6502)
	{ "dsp-3_p1-.l9",       0x0800, 0x3bfff5f3, BRF_BIOS | BRF_PRG }, // 0x89 - BIOS "A" Japan, Older (Main M6502)

	{ "rms-3_p2-.c9",       0x0400, 0x6c4a891f, BRF_BIOS | BRF_PRG }, // 0x8a - BIOS "A" Japan, Older (Sound M6502)

	{ "dsp-3_p3-.e5",       0x0020, 0x539a5a64, BRF_BIOS | BRF_OPT }, // 0x8b - BIOS "A" Japan, Older (prom)
	{ "rms-3_p4-.f6",       0x0020, 0x9014c0fd, BRF_BIOS | BRF_OPT }, // 0x8c - BIOS "A" Japan, Older (prom)
	{ "dsp-3_p5-.m4",       0x0020, 0xe52089a0, BRF_BIOS | BRF_OPT }, // 0x8d - BIOS "A" Japan, Older (prom)

	{ "",             	0x0000, 0x00000000, 0                  }, // 0x8e
	{ "",             	0x0000, 0x00000000, 0                  }, // 0x8f

	{ "v0b-.7e",  		0x1000, 0x23d929b7, BRF_BIOS | BRF_PRG }, // 0x90 - BIOS "B" USA (Main M6502)
	{ "",             	0x0000, 0x00000000, 0                  }, // 0x91

	{ "v1-.5a",     	0x0800, 0xb66b2c2a, BRF_BIOS | BRF_PRG }, // 0x92 - BIOS "B" USA (Sound M6502)

	{ "v2.3m",		0x0020, 0x238fdb40, BRF_BIOS | BRF_OPT }, // 0x93 - BIOS "B" USA (prom)
	{ "v4.10d",		0x0020, 0x3b5836b4, BRF_BIOS | BRF_OPT }, // 0x94 - BIOS "B" USA (prom)
	{ "v3.3j",		0x0020, 0x51eef657, BRF_BIOS | BRF_OPT }, // 0x95 - BIOS "B" USA (prom)

	{ "",             	0x0000, 0x00000000, 0                  }, // 0x96
	{ "",             	0x0000, 0x00000000, 0                  }, // 0x97

	{ "dsp-3_p0-b.m9",      0x0800, 0xb67a91d9, BRF_BIOS | BRF_PRG }, // 0x98 - BIOS "B" USA, Older (Main M6502)
	{ "dsp-3_p1-.l9",       0x0800, 0x3bfff5f3, BRF_BIOS | BRF_PRG }, // 0x99 - BIOS "B" USA, Older (Main M6502)

	{ "rms-3_p2-.c9",       0x0400, 0x6c4a891f, BRF_BIOS | BRF_PRG }, // 0x9a - BIOS "B" USA, older (Sound M6502)

	{ "dsp-3_p3-.e5",       0x0020, 0x539a5a64, BRF_BIOS | BRF_OPT }, // 0x9b - BIOS "B" USA, older (prom)
	{ "rms-3_p4-.f6",       0x0020, 0x9014c0fd, BRF_BIOS | BRF_OPT }, // 0x9c - BIOS "B" USA, older (prom)
	{ "dsp-3_p5-.m4",       0x0020, 0xe52089a0, BRF_BIOS | BRF_OPT }, // 0x9d - BIOS "B" USA, older (prom)

	{ "",             	0x0000, 0x00000000, 0                  }, // 0x9e
	{ "",             	0x0000, 0x00000000, 0                  }, // 0x9f

	{ "cassmcu.1c", 	0x0400, 0xa6df18fd, BRF_BIOS | BRF_PRG }, // 0xa0 - MCU BIOS (Shared)

#ifdef ROM_VERIFY
	{ "v0d-.7e",		0x1000, 0x1e0c22b1, BRF_BIOS | BRF_PRG }, // 0xa1 - handcrafted (single byte changed) because ctisland3 requires region D
#endif
};

STD_ROM_PICK(decocass)
STD_ROM_FN(decocass)

static INT32 BiosInit()
{
	return 1;
}

struct BurnDriver BurnDrvDecocass = {
	"decocass", NULL, NULL, NULL, "1981",
	"DECO Cassette System Bios\0", "BIOS only", "Data East", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_BOARDROM | BDF_ORIENTATION_VERTICAL, 0, HARDWARE_PREFIX_DATAEAST, GBF_BIOS, 0,
	NULL, decocassRomInfo, decocassRomName, NULL, NULL, NULL, NULL, DecocassInputInfo, DecocassDIPInfo,
	BiosInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Test Tape (DECO Cassette) (US)

static struct BurnRomInfo ctsttapeRomDesc[] = {
	{ "de-0061.pro",	0x0020, 0xe09ae5de, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "testtape.cas",	0x2000, 0x4f9d8efb, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(ctsttape, ctsttape, decocass)
STD_ROM_FN(ctsttape)

static UINT8 type1_pass_136_table[8] ={ T1PROM,T1DIRECT,T1PROM,T1DIRECT,T1PROM,T1PROM,T1DIRECT,T1PROM };

static INT32 CtsttapeInit()
{
	type1_map = type1_pass_136_table;

	return DecocassInit(decocass_type1_read,NULL);
}

struct BurnDriver BurnDrvCtsttape = {
	"ctsttape", NULL, "decocass", NULL, "1981",
	"Test Tape (DECO Cassette) (US)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_PREFIX_DATAEAST, GBF_MISC, 0,
	NULL, ctsttapeRomInfo, ctsttapeRomName, NULL, NULL, NULL, NULL, DecocassInputInfo, CtsttapeDIPInfo,
	CtsttapeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Highway Chase (DECO Cassette) (US)

static struct BurnRomInfo chwyRomDesc[] = {
	{ "chwy.pro",		0x0020, 0x2fae678e, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "chwy.cas",		0x8000, 0x68a48064, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(chwy, chwy, decocass)
STD_ROM_FN(chwy)

static UINT8 type1_latch_27_pass_3_inv_2_table[8] = { T1PROM,T1PROM,T1LATCHINV,T1DIRECT,T1PROM,T1PROM,T1PROM,T1LATCH };

static INT32 ChwyInit()
{
	type1_map = type1_latch_27_pass_3_inv_2_table;

	return DecocassInit(decocass_type1_read,NULL);
}

struct BurnDriver BurnDrvChwy = {
	"chwy", NULL, "decocass", NULL, "1980",
	"Highway Chase (DECO Cassette) (US)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_PREFIX_DATAEAST, GBF_VERSHOOT, 0,
	NULL, chwyRomInfo, chwyRomName, NULL, NULL, NULL, NULL, DecocassInputInfo, ChwyDIPInfo,
	ChwyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Manhattan (DECO Cassette) (Japan)

static struct BurnRomInfo cmanhatRomDesc[] = {
	{ "manhattan.pro",	0x0020, 0x1bc9fccb, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "manhattan.cas",	0x6000, 0x92dae2b1, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(cmanhat, cmanhat, decocass)
STD_ROM_FN(cmanhat)

static UINT8 type1_latch_xab_pass_x54_table[8] = { T1PROM,T1PROM,T1DIRECT,T1PROM,T1DIRECT,T1PROM,T1DIRECT,T1PROM };

static INT32 CmanhatInit()
{
	type1_map = type1_latch_xab_pass_x54_table;

	return DecocassInit(decocass_type1_read,NULL);
}

struct BurnDriver BurnDrvCmanhat = {
	"cmanhat", NULL, "decocass", NULL, "1981",
	"Manhattan (DECO Cassette) (Japan)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_PREFIX_DATAEAST, GBF_ACTION, 0,
	NULL, cmanhatRomInfo, cmanhatRomName, NULL, NULL, NULL, NULL, DecocassInputInfo, CmanhatDIPInfo,
	CmanhatInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Terranean (DECO Cassette) (US)

static struct BurnRomInfo cterraniRomDesc[] = {
	{ "dp-1040.dgl",	0x0020, 0xe09ae5de, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "dt-1040.cas",	0x8000, 0xeb71adbc, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(cterrani, cterrani, decocass)
STD_ROM_FN(cterrani)

static UINT8 type1_latch_26_pass_3_inv_2_table[8] = { T1PROM,T1PROM,T1LATCHINV,T1DIRECT,T1PROM, T1PROM,T1LATCH,T1PROM };

static INT32 CterraniInit()
{
	type1_map = type1_latch_26_pass_3_inv_2_table;
	type1_inmap = MAKE_MAP(0,1,2,3,4,5,6,7);
	type1_outmap = MAKE_MAP(0,1,2,3,4,5,6,7);

	return DecocassInit(decocass_type1_read,NULL);
}

struct BurnDriver BurnDrvCterrani = {
	"cterrani", NULL, "decocass", NULL, "1981",
	"Terranean (DECO Cassette) (US)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_PREFIX_DATAEAST, GBF_VERSHOOT, 0,
	NULL, cterraniRomInfo, cterraniRomName, NULL, NULL, NULL, NULL, DecocassInputInfo, CterraniDIPInfo,
	CterraniInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Astro Fantasia (DECO Cassette) (US)

static struct BurnRomInfo castfantRomDesc[] = {
	{ "de-0061.pro",	0x0020, 0xe09ae5de, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "castfant.cas",	0x8000, 0x6d77d1b5, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(castfant, castfant, decocass)
STD_ROM_FN(castfant)

static UINT8 type1_latch_16_pass_3_inv_1_table[8] = { T1PROM,T1LATCHINV,T1PROM,T1DIRECT,T1PROM,T1PROM,T1LATCH,T1PROM };

static INT32 CastfantInit()
{
	type1_map = type1_latch_16_pass_3_inv_1_table;

	return DecocassInit(decocass_type1_read,NULL);
}

struct BurnDriver BurnDrvCastfant = {
	"castfant", NULL, "decocass", NULL, "1981",
	"Astro Fantasia (DECO Cassette) (US)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_PREFIX_DATAEAST, GBF_VERSHOOT, 0,
	NULL, castfantRomInfo, castfantRomName, NULL, NULL, NULL, NULL, DecocassInputInfo, CastfantDIPInfo,
	CastfantInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Super Astro Fighter (DECO Cassette) (US)

static struct BurnRomInfo csuperasRomDesc[] = {
	{ "de-0061.pro",	0x0020, 0xe09ae5de, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "csuperas.cas",	0x8000, 0xfabcd07f, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(csuperas, csuperas, decocass)
STD_ROM_FN(csuperas)

static INT32 CsuperasInit()
{
	type1_map = type1_latch_26_pass_3_inv_2_table;
	type1_inmap = MAKE_MAP(0,1,2,3,5,4,6,7);
	type1_outmap = MAKE_MAP(0,1,2,3,5,4,6,7);

	return DecocassInit(decocass_type1_read,NULL);
}

struct BurnDriver BurnDrvCsuperas = {
	"csuperas", NULL, "decocass", NULL, "1981",
	"Super Astro Fighter (DECO Cassette) (US)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_PREFIX_DATAEAST, GBF_VERSHOOT, 0,
	NULL, csuperasRomInfo, csuperasRomName, NULL, NULL, NULL, NULL, DecocassInputInfo, CsuperasDIPInfo,
	CsuperasInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Ocean to Ocean (Medal) (DECO Cassette MD) (No.10/Ver.1,Japan)

static struct BurnRomInfo cocean1aRomDesc[] = {
	{ "dp-1100-a.rom",	0x0020, 0x1bc9fccb, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data'

	{ "dt-1101-a-0.cas",	0x3e00, 0xdb8ab848, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(cocean1a, cocean1a, decocass)
STD_ROM_FN(cocean1a)

static UINT8 type1_map1100[8] = { T1PROM,T1PROM,T1LATCHINV,T1PROM,T1DIRECT,T1PROM,T1LATCH,T1PROM };

static INT32 Cocean1aInit()
{
	type1_map = type1_map1100;
	type1_inmap = MAKE_MAP(0,1,2,3,4,5,6,7);
	type1_outmap = MAKE_MAP(0,1,2,3,4,5,6,7);

	return DecocassInit(decocass_type1_read,NULL);
}

struct BurnDriver BurnDrvCocean1a = {
	"cocean1a", NULL, "decocass", NULL, "1981",
	"Ocean to Ocean (Medal) (DECO Cassette MD) (No.10/Ver.1,Japan)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_PREFIX_DATAEAST, GBF_CASINO, 0,
	NULL, cocean1aRomInfo, cocean1aRomName, NULL, NULL, NULL, NULL, DecocassInputInfo, Cocean1aDIPInfo,
	Cocean1aInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Ocean to Ocean (Medal) (DECO Cassette MD) (No.10/Ver.6,US)

static struct BurnRomInfo cocean6bRomDesc[] = {
	{ "dp-1100-b.rom",	0x0020, 0xe09ae5de, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "dt-1106-b-0.cas",	0x4500, 0xfa6ffc95, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(cocean6b, cocean6b, decocass)
STD_ROM_FN(cocean6b)

struct BurnDriver BurnDrvCocean6b = {
	"cocean6b", "cocean1a", "decocass", NULL, "1981",
	"Ocean to Ocean (Medal) (DECO Cassette MD) (No.10/Ver.6,US)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE, 2, HARDWARE_PREFIX_DATAEAST, GBF_CASINO, 0,
	NULL, cocean6bRomInfo, cocean6bRomName, NULL, NULL, NULL, NULL, DecocassInputInfo, Cocean6bDIPInfo,
	Cocean1aInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Lock'n'Chase (DECO Cassette) (US)

static struct BurnRomInfo clocknchRomDesc[] = {
	{ "dp-1110_b.dgl",	0x0020, 0xe09ae5de, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "clocknch.cas",	0x8000, 0xc9d163a4, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(clocknch, clocknch, decocass)
STD_ROM_FN(clocknch)

static INT32 ClocknchInit()
{
	type1_map = type1_latch_26_pass_3_inv_2_table;
	type1_inmap = MAKE_MAP(0,1,3,2,4,5,6,7);
	type1_outmap = MAKE_MAP(0,1,3,2,4,5,6,7);

	return DecocassInit(decocass_type1_read,NULL);
}

struct BurnDriver BurnDrvClocknch = {
	"clocknch", NULL, "decocass", NULL, "1981",
	"Lock'n'Chase (DECO Cassette) (US)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_PREFIX_DATAEAST, GBF_MAZE, 0,
	NULL, clocknchRomInfo, clocknchRomName, NULL, NULL, NULL, NULL, DecocassInputInfo, ClocknchDIPInfo,
	ClocknchInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Lock'n'Chase (DECO Cassette) (Japan)

static struct BurnRomInfo clocknchjRomDesc[] = {
	{ "a-0061.dgl",		0x0020, 0x1bc9fccb, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "dt-1111-a-0.bin",	0x6300, 0x9753e815, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(clocknchj, clocknchj, decocass)
STD_ROM_FN(clocknchj)

static UINT8 type1_map_clocknchj[8] = { T1PROM,T1PROM,T1DIRECT,T1LATCHINV,T1PROM,T1PROM,T1LATCH,T1PROM };

static INT32 ClocknchjInit()
{
	type1_map = type1_map_clocknchj;
	type1_inmap = MAKE_MAP(0,1,2,3,4,5,6,7);
	type1_outmap = MAKE_MAP(0,1,2,3,4,5,6,7);

	return DecocassInit(decocass_type1_read,NULL);
}

struct BurnDriver BurnDrvClocknchj = {
	"clocknchj", "clocknch", "decocass", NULL, "1981",
	"Lock'n'Chase (DECO Cassette) (Japan)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE, 2, HARDWARE_PREFIX_DATAEAST, GBF_MAZE, 0,
	NULL, clocknchjRomInfo, clocknchjRomName, NULL, NULL, NULL, NULL, DecocassInputInfo, ClocknchjDIPInfo,
	ClocknchjInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Flash Boy (vertical) [DECO Cassette MD] (No.12/Ver.0/Set.1,Japan)

static struct BurnRomInfo cfboy0a1RomDesc[] = {
	{ "dp-1120-a.rom",	0x0020, 0x1bc9fccb, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "dt-1120-a-1.cas",	0x6a00, 0xc6746dc0, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(cfboy0a1, cfboy0a1, decocass)
STD_ROM_FN(cfboy0a1)

static UINT8 type1_map1120[8] = { T1PROM,T1PROM,T1LATCHINV,T1DIRECT,T1PROM,T1LATCH,T1PROM,T1PROM };

static INT32 Cfboy0a1Init()
{
	type1_map = type1_map1120;
	type1_inmap = MAKE_MAP(0,1,2,3,4,5,6,7);
	type1_outmap = MAKE_MAP(0,1,2,3,4,5,6,7);

	return DecocassInit(decocass_type1_read,NULL);
}

struct BurnDriver BurnDrvCfboy0a1 = {
	"cfboy0a1", NULL, "decocass", NULL, "1981",
	"Flash Boy (vertical) [DECO Cassette MD] (No.12/Ver.0/Set.1,Japan)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_PREFIX_DATAEAST, GBF_VERSHOOT, 0,
	NULL, cfboy0a1RomInfo, cfboy0a1RomName, NULL, NULL, NULL, NULL, DecocassInputInfo, Cfboy0a1DIPInfo,
	Cfboy0a1Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Tournament Pro Golf (DECO Cassette) (US)

static struct BurnRomInfo cprogolfRomDesc[] = {
	{ "dp-1130_b.dgl",	0x0020, 0xe09ae5de, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "dt-1130_9b.cas",	0x8000, 0x02123cd1, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(cprogolf, cprogolf, decocass)
STD_ROM_FN(cprogolf)

static INT32 CprogolfInit()
{
	type1_map = type1_latch_26_pass_3_inv_2_table;
	type1_inmap = MAKE_MAP(1,0,2,3,4,5,6,7);
	type1_outmap = MAKE_MAP(1,0,2,3,4,5,6,7);

	return DecocassInit(decocass_type1_read,NULL);
}

struct BurnDriver BurnDrvCprogolf = {
	"cprogolf", NULL, "decocass", NULL, "1981",
	"Tournament Pro Golf (DECO Cassette) (US)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, cprogolfRomInfo, cprogolfRomName, NULL, NULL, NULL, NULL, DecocassInputInfo, CprogolfDIPInfo,
	CprogolfInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Tournament Pro Golf (DECO Cassette) (Japan)

static struct BurnRomInfo cprogolfjRomDesc[] = {
	{ "a-0061.dgl",		0x0020, 0x1bc9fccb, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "dt-113_a.cas",	0x8000, 0x8408248f, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(cprogolfj, cprogolfj, decocass)
STD_ROM_FN(cprogolfj)

static INT32 CprogolfjInit()
{
	type1_map = type1_latch_26_pass_3_inv_2_table;
	type1_inmap = MAKE_MAP(1,0,2,3,4,5,6,7);
	type1_outmap = MAKE_MAP(1,0,2,3,4,5,6,7);

	return DecocassInit(decocass_type1_read,NULL);
}

struct BurnDriver BurnDrvCprogolfj = {
	"cprogolfj", "cprogolf", "decocass", NULL, "1981",
	"Tournament Pro Golf (DECO Cassette) (Japan)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE, 2, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, cprogolfjRomInfo, cprogolfjRomName, NULL, NULL, NULL, NULL, DecocassInputInfo, CprogolfjDIPInfo,
	CprogolfjInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// 18 Challenge Pro Golf (DECO Cassette) (Japan)

static struct BurnRomInfo cprogolf18RomDesc[] = {
	{ "de-0061-a-0.rom",	0x0020, 0x1bc9fccb, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "progolf18.cas",	0x6700, 0x3024396c, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(cprogolf18, cprogolf18, decocass)
STD_ROM_FN(cprogolf18)

struct BurnDriver BurnDrvCprogolf18 = {
	"cprogolf18", "cprogolf", "decocass", NULL, "1982",
	"18 Challenge Pro Golf (DECO Cassette) (Japan)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE, 2, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, cprogolf18RomInfo, cprogolf18RomName, NULL, NULL, NULL, NULL, DecocassInputInfo, CprogolfjDIPInfo,
	CprogolfjInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// DS Telejan (DECO Cassette) (Japan)

static struct BurnRomInfo cdsteljnRomDesc[] = {
	{ "a-0061.dgl",		0x0020, 0x1bc9fccb, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "dt-1144-a3.cas",	0x7300, 0x1336a912, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(cdsteljn, cdsteljn, decocass)
STD_ROM_FN(cdsteljn)

static INT32 CdsteljnInit()
{
	type1_map = type1_latch_27_pass_3_inv_2_table;

	return DecocassInit(decocass_type1_read,NULL);
}

struct BurnDriver BurnDrvCdsteljn = {
	"cdsteljn", NULL, "decocass", NULL, "1981",
	"DS Telejan (DECO Cassette) (Japan)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_PREFIX_DATAEAST, GBF_MAHJONG, 0,
	NULL, cdsteljnRomInfo, cdsteljnRomName, NULL, NULL, NULL, NULL, CdsteljnInputInfo, CdsteljnDIPInfo,
	CdsteljnInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Lucky Poker (DECO Cassette) (US)

static struct BurnRomInfo cluckypoRomDesc[] = {
	{ "dp-1150_b.dgl",	0x0020, 0xe09ae5de, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "cluckypo.cas",	0x8000, 0x2070c243, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(cluckypo, cluckypo, decocass)
STD_ROM_FN(cluckypo)

static INT32 CluckypoInit()
{
	type1_map = type1_latch_26_pass_3_inv_2_table;
	type1_inmap = MAKE_MAP(0,3,2,1,4,5,6,7);
	type1_outmap = MAKE_MAP(0,3,2,1,4,5,6,7);

	return DecocassInit(decocass_type1_read,NULL);
}

struct BurnDriver BurnDrvCluckypo = {
	"cluckypo", NULL, "decocass", NULL, "1981",
	"Lucky Poker (DECO Cassette) (US)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_PREFIX_DATAEAST, GBF_CASINO, 0,
	NULL, cluckypoRomInfo, cluckypoRomName, NULL, NULL, NULL, NULL, DecocassInputInfo, CluckypoDIPInfo,
	CluckypoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Treasure Island (DECO Cassette) (US) (set 1)

static struct BurnRomInfo ctislandRomDesc[] = {
	{ "de-0061.pro",	0x0020, 0xe09ae5de, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "ctisland.cas",	0x8000, 0x3f63b8f8, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data

	{ "deco-ti.x1",		0x1000, 0xa7f8aeba, 3 | BRF_GRA },           //  2 Overlay data
	{ "deco-ti.x2",		0x1000, 0x2a0d3c91, 3 | BRF_GRA },           //  3
	{ "deco-ti.x3",		0x1000, 0x3a26b97c, 3 | BRF_GRA },           //  4
	{ "deco-ti.x4",		0x1000, 0x1cbe43de, 3 | BRF_GRA },           //  5
};

STDROMPICKEXT(ctisland, ctisland, decocass)
STD_ROM_FN(ctisland)

static INT32 CtislandInit()
{
	type1_map = type1_latch_26_pass_3_inv_2_table;
	type1_inmap = MAKE_MAP(2,1,0,3,4,5,6,7);
	type1_outmap = MAKE_MAP(2,1,0,3,4,5,6,7);

	e900_enable = 1;

	fourway_mode = 1;

	return DecocassInit(decocass_type1_read,NULL);
}

struct BurnDriver BurnDrvCtisland = {
	"ctisland", NULL, "decocass", NULL, "1981",
	"Treasure Island (DECO Cassette) (US) (set 1)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_PREFIX_DATAEAST, GBF_PLATFORM, 0,
	NULL, ctislandRomInfo, ctislandRomName, NULL, NULL, NULL, NULL, DecocassInputInfo, CtislandDIPInfo,
	CtislandInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Treasure Island (DECO Cassette) (US) (set 2)

static struct BurnRomInfo ctisland2RomDesc[] = {
	{ "de-0061.pro",	0x0020, 0xe09ae5de, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "ctislnd2.cas",	0x8000, 0x2854b4c0, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data

	{ "deco-ti.x1",		0x1000, 0xa7f8aeba, 3 | BRF_GRA },           //  2 Overlay data
	{ "deco-ti.x2",		0x1000, 0x2a0d3c91, 3 | BRF_GRA },           //  3
	{ "deco-ti.x3",		0x1000, 0x3a26b97c, 3 | BRF_GRA },           //  4
	{ "deco-ti.x4",		0x1000, 0x1cbe43de, 3 | BRF_GRA },           //  5
};

STDROMPICKEXT(ctisland2, ctisland2, decocass)
STD_ROM_FN(ctisland2)

struct BurnDriver BurnDrvCtisland2 = {
	"ctisland2", "ctisland", "decocass", NULL, "1981",
	"Treasure Island (DECO Cassette) (US) (set 2)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE, 2, HARDWARE_PREFIX_DATAEAST, GBF_PLATFORM, 0,
	NULL, ctisland2RomInfo, ctisland2RomName, NULL, NULL, NULL, NULL, DecocassInputInfo, CtislandDIPInfo,
	CtislandInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Treasure Island (DECO Cassette) (unk)

static struct BurnRomInfo ctisland3RomDesc[] = {
#ifdef ROM_VERIFY
	{ "ctisland3.pro",	0x0020, 0xb87b56a7, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data
#else
	{ "de-0061.pro",	0x0020, 0xe09ae5de, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data
#endif

	{ "ctislnd3.cas",	0x8000, 0x45464e1e, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data

	{ "deco-ti.x1",		0x1000, 0xa7f8aeba, 3 | BRF_GRA },           //  2 Overlay data
	{ "deco-ti.x2",		0x1000, 0x2a0d3c91, 3 | BRF_GRA },           //  3
	{ "deco-ti.x3",		0x1000, 0x3a26b97c, 3 | BRF_GRA },           //  4
	{ "deco-ti.x4",		0x1000, 0x1cbe43de, 3 | BRF_GRA },           //  5
};

STDROMPICKEXT(ctisland3, ctisland3, decocass)
STD_ROM_FN(ctisland3)

struct BurnDriver BurnDrvCtisland3 = {
	"ctisland3", "ctisland", "decocass", NULL, "1981",
	"Treasure Island (DECO Cassette) (unk)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_ORIENTATION_VERTICAL | BDF_CLONE, 2, HARDWARE_PREFIX_DATAEAST, GBF_PLATFORM, 0,
	NULL, ctisland3RomInfo, ctisland3RomName, NULL, NULL, NULL, NULL, DecocassInputInfo, CtislandDIPInfo,
	CtislandInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Explorer (DECO Cassette) (US)

static struct BurnRomInfo cexploreRomDesc[] = {
	{ "dp-1180_b.dgl",	      0x0020, 0xc7a9ac8f, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "cexplore.cas",	      0x8000, 0xfae49c66, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data

	{ "x1_made_in_japan_18.x1",   0x1000, 0xf2ca58f0, 3 | BRF_GRA },           //  2 Overlay data
	{ "x2_made_in_japan_18.x2",   0x1000, 0x75d999bf, 3 | BRF_GRA },           //  3
	{ "x3_made_in_japan_18.x3",   0x1000, 0x941539c6, 3 | BRF_GRA },           //  4
	{ "x4_made_in_japan_18.x4",   0x1000, 0x73388544, 3 | BRF_GRA },           //  5
	{ "x5_made_in_japan_18.x5",   0x1000, 0xb40699c5, 3 | BRF_GRA },           //  6
	{ "y1_made_in_japan_18.y1",   0x1000, 0xd887dc50, 3 | BRF_GRA },           //  7
	{ "y2_made_in_japan_18.y2",   0x1000, 0xfe325d0d, 3 | BRF_GRA },           //  8
	{ "y3_made_in_japan_18.y3",   0x1000, 0x7a787ecf, 3 | BRF_GRA },           //  9
	{ "y4_made_in_japan_18.y4",   0x1000, 0xac30e8c7, 3 | BRF_GRA },           // 10
	{ "y5_made_in_japan_18.y5",   0x1000, 0x0a6b8f03, 3 | BRF_GRA },           // 11
};

STDROMPICKEXT(cexplore, cexplore, decocass)
STD_ROM_FN(cexplore)

static UINT8 type1_latch_26_pass_5_inv_2_table[8] = { T1PROM,T1PROM,T1LATCHINV,T1PROM,T1PROM,T1DIRECT,T1LATCH,T1PROM };

static INT32 CexploreInit()
{
	type1_map = type1_latch_26_pass_5_inv_2_table;

	e900_enable = 1;

	return DecocassInit(decocass_type1_read,NULL);
}

struct BurnDriver BurnDrvCexplore = {
	"cexplore", NULL, "decocass", NULL, "1982",
	"Explorer (DECO Cassette) (US)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_PREFIX_DATAEAST, GBF_SHOOT, 0,
	NULL, cexploreRomInfo, cexploreRomName, NULL, NULL, NULL, NULL, DecocassInputInfo, CexploreDIPInfo,
	CexploreInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Disco No.1 (DECO Cassette) (US)

static struct BurnRomInfo cdiscon1RomDesc[] = {
	{ "dp-1190_b.dgl",	0x0800, 0x0f793fab, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "cdiscon1.cas",	0x8000, 0x1429a397, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(cdiscon1, cdiscon1, decocass)
STD_ROM_FN(cdiscon1)

static INT32 Cdiscon1Init()
{
	fourway_mode = 1;

	return DecocassInit(decocass_type2_read, decocass_type2_write);
}

struct BurnDriver BurnDrvCdiscon1 = {
	"cdiscon1", NULL, "decocass", NULL, "1982",
	"Disco No.1 (DECO Cassette) (US)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_PREFIX_DATAEAST, GBF_ACTION, 0,
	NULL, cdiscon1RomInfo, cdiscon1RomName, NULL, NULL, NULL, NULL, DecocassInputInfo, Cdiscon1DIPInfo,
	Cdiscon1Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Sweet Heart (DECO Cassette) (US)

static struct BurnRomInfo csweethtRomDesc[] = {
	{ "cdiscon1.pro",	0x0800, 0x0f793fab, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "csweetht.cas",	0x8000, 0x175ef706, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(csweetht, csweetht, decocass)
STD_ROM_FN(csweetht)

struct BurnDriver BurnDrvCsweetht = {
	"csweetht", "cdiscon1", "decocass", NULL, "1982",
	"Sweet Heart (DECO Cassette) (US)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE, 2, HARDWARE_PREFIX_DATAEAST, GBF_ACTION, 0,
	NULL, csweethtRomInfo, csweethtRomName, NULL, NULL, NULL, NULL, DecocassInputInfo, CsweethtDIPInfo,
	Cdiscon1Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Tornado (DECO Cassette) (US)

static struct BurnRomInfo ctornadoRomDesc[] = {
	{ "ctornado.pro",	0x0800, 0xc9a91697, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "ctornado.cas",	0x8000, 0xe4e36ce0, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(ctornado, ctornado, decocass)
STD_ROM_FN(ctornado)

static INT32 CtornadoInit()
{
	return DecocassInit(decocass_type2_read, decocass_type2_write);
}

struct BurnDriver BurnDrvCtornado = {
	"ctornado", NULL, "decocass", NULL, "1982",
	"Tornado (DECO Cassette) (US)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_PREFIX_DATAEAST, GBF_SHOOT, 0,
	NULL, ctornadoRomInfo, ctornadoRomName, NULL, NULL, NULL, NULL, DecocassInputInfo, CtornadoDIPInfo,
	CtornadoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Mission-X (DECO Cassette) (US)

static struct BurnRomInfo cmissnxRomDesc[] = {
	{ "dp-121_b.dgl",	0x0800, 0x8a41c071, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "cmissnx.cas",	0x8000, 0x3a094e11, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(cmissnx, cmissnx, decocass)
STD_ROM_FN(cmissnx)

static INT32 CmissnxInit()
{
	return DecocassInit(decocass_type2_read, decocass_type2_write);
}

struct BurnDriver BurnDrvCmissnx = {
	"cmissnx", NULL, "decocass", NULL, "1982",
	"Mission-X (DECO Cassette) (US)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_PREFIX_DATAEAST, GBF_VERSHOOT, 0,
	NULL, cmissnxRomInfo, cmissnxRomName, NULL, NULL, NULL, NULL, DecocassInputInfo, CmissnxDIPInfo,
	CmissnxInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Pro Tennis (DECO Cassette) (US)

static struct BurnRomInfo cptennisRomDesc[] = {
	{ "cptennis.pro",	0x0800, 0x59b8cede, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "cptennis.cas",	0x8000, 0x6bb257fe, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(cptennis, cptennis, decocass)
STD_ROM_FN(cptennis)

static INT32 CptennisInit()
{
	return DecocassInit(decocass_type2_read, decocass_type2_write);
}

struct BurnDriver BurnDrvCptennis = {
	"cptennis", NULL, "decocass", NULL, "1982",
	"Pro Tennis (DECO Cassette) (US)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, cptennisRomInfo, cptennisRomName, NULL, NULL, NULL, NULL, DecocassInputInfo, CptennisDIPInfo,
	CptennisInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Pro Tennis (DECO Cassette) (Japan)

static struct BurnRomInfo cptennisjRomDesc[] = {
	{ "dp-1220-a-0.rom",	0x0800, 0x1c603239, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "dt-1222-a-0.bin",	0x6a00, 0xee29eba7, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(cptennisj, cptennisj, decocass)
STD_ROM_FN(cptennisj)

struct BurnDriver BurnDrvCptennisj = {
	"cptennisj", "cptennis", "decocass", NULL, "1982",
	"Pro Tennis (DECO Cassette) (Japan)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE, 2, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, cptennisjRomInfo, cptennisjRomName, NULL, NULL, NULL, NULL, DecocassInputInfo, CptennisjDIPInfo,
	CptennisInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Angler Dangler (DECO Cassette) (US)

static struct BurnRomInfo cadanglrRomDesc[] = {
	{ "dp-1250-a-0.dgl",	0x1000, 0x92a3b387, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "dt-1255-b-0.cas",	0x7400, 0xeb985257, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(cadanglr, cadanglr, decocass)
STD_ROM_FN(cadanglr)

static INT32 CadanglrInit()
{
	type3_swap = TYPE3_SWAP_01;

	return DecocassInit(decocass_type3_read,decocass_type3_write);
}

struct BurnDriver BurnDrvCadanglr = {
	"cadanglr", NULL, "decocass", NULL, "1982",
	"Angler Dangler (DECO Cassette) (US)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, cadanglrRomInfo, cadanglrRomName, NULL, NULL, NULL, NULL, DecocassInputInfo, CfishingDIPInfo,
	CadanglrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Fishing (DECO Cassette) (Japan)

static struct BurnRomInfo cfishingRomDesc[] = {
	{ "dp-1250-a-0.dgl",	0x1000, 0x92a3b387, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "dt-1250-a-0.cas",	0x7500, 0xd4a16425, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(cfishing, cfishing, decocass)
STD_ROM_FN(cfishing)

struct BurnDriver BurnDrvCfishing = {
	"cfishing", "cadanglr", "decocass", NULL, "1982",
	"Fishing (DECO Cassette) (Japan)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE, 2, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, cfishingRomInfo, cfishingRomName, NULL, NULL, NULL, NULL, DecocassInputInfo, CfishingjDIPInfo,
	CadanglrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Burger Time (DECO Cassette) (US)

static struct BurnRomInfo cbtimeRomDesc[] = {
	{ "dp-126_b.dgl",	0x1000, 0x25bec0f0, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "dt-126_7b.cas",	0x8000, 0x56d7dc58, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(cbtime, cbtime, decocass)
STD_ROM_FN(cbtime)

static INT32 CbtimeInit()
{
	type3_swap = TYPE3_SWAP_12;

	burgertime_mode = 1;
	fourway_mode = 1;

	return DecocassInit(decocass_type3_read,decocass_type3_write);
}

struct BurnDriver BurnDrvCbtime = {
	"cbtime", NULL, "decocass", NULL, "1983",
	"Burger Time (DECO Cassette) (US)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_PREFIX_DATAEAST, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, cbtimeRomInfo, cbtimeRomName, NULL, NULL, NULL, NULL, DecocassInputInfo, CbtimeDIPInfo,
	CbtimeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Hamburger (DECO Cassette) (Japan)

static struct BurnRomInfo chamburgerRomDesc[] = {
	{ "dp-126_a.dgl",	0x1000, 0x25bec0f0, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "dt-126_a.cas",	0x8000, 0x334fb987, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(chamburger, chamburger, decocass)
STD_ROM_FN(chamburger)

struct BurnDriver BurnDrvChamburger = {
	"chamburger", "cbtime", "decocass", NULL, "1982",
	"Hamburger (DECO Cassette) (Japan)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE, 2, HARDWARE_PREFIX_DATAEAST, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, chamburgerRomInfo, chamburgerRomName, NULL, NULL, NULL, NULL, DecocassInputInfo, ChamburgerDIPInfo,
	CbtimeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Burnin' Rubber (DECO Cassette) (US) (set 1)

static struct BurnRomInfo cburnrubRomDesc[] = {
	{ "dp-127_b.pro",	0x1000, 0x9f396832, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "cburnrub.cas",	0x8000, 0x4528ac22, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(cburnrub, cburnrub, decocass)
STD_ROM_FN(cburnrub)

static INT32 CburnrubInit()
{
	type3_swap = TYPE3_SWAP_67;

	return DecocassInit(decocass_type3_read,decocass_type3_write);
}

struct BurnDriver BurnDrvCburnrub = {
	"cburnrub", NULL, "decocass", NULL, "1982",
	"Burnin' Rubber (DECO Cassette) (US) (set 1)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_PREFIX_DATAEAST, GBF_RACING | GBF_ACTION, 0,
	NULL, cburnrubRomInfo, cburnrubRomName, NULL, NULL, NULL, NULL, DecocassInputInfo, CburnrubDIPInfo,
	CburnrubInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Burnin' Rubber (DECO Cassette) (US) (set 2)

static struct BurnRomInfo cburnrub2RomDesc[] = {
	{ "dp-127_b.pro",	0x1000, 0x9f396832, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "cburnrb2.cas",	0x8000, 0x84a9ed66, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(cburnrub2, cburnrub2, decocass)
STD_ROM_FN(cburnrub2)

struct BurnDriver BurnDrvCburnrub2 = {
	"cburnrub2", "cburnrub", "decocass", NULL, "1982",
	"Burnin' Rubber (DECO Cassette) (US) (set 2)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE, 2, HARDWARE_PREFIX_DATAEAST, GBF_RACING | GBF_ACTION, 0,
	NULL, cburnrub2RomInfo, cburnrub2RomName, NULL, NULL, NULL, NULL, DecocassInputInfo, CburnrubDIPInfo,
	CburnrubInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Bump 'n' Jump (DECO Cassette) (US)

static struct BurnRomInfo cbnjRomDesc[] = {
	{ "dp-127_b.pro",	0x1000, 0x9f396832, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "cbnj.cas",		0x8000, 0xeed41560, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(cbnj, cbnj, decocass)
STD_ROM_FN(cbnj)

struct BurnDriver BurnDrvCbnj = {
	"cbnj", "cburnrub", "decocass", NULL, "1982",
	"Bump 'n' Jump (DECO Cassette) (US)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE, 2, HARDWARE_PREFIX_DATAEAST, GBF_RACING | GBF_ACTION, 0,
	NULL, cbnjRomInfo, cbnjRomName, NULL, NULL, NULL, NULL, DecocassInputInfo, CburnrubDIPInfo,
	CburnrubInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Cluster Buster (DECO Cassette) (US)

static struct BurnRomInfo cgraplopRomDesc[] = {
	{ "cgraplop.pro",	0x1000, 0xee93787d, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "cgraplop.cas",	0x8000, 0xd2c1c1bb, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(cgraplop, cgraplop, decocass)
STD_ROM_FN(cgraplop)

static INT32 CgraplopInit()
{
	type3_swap = TYPE3_SWAP_56;

	return DecocassInit(decocass_type3_read,decocass_type3_write);
}

struct BurnDriver BurnDrvCgraplop = {
	"cgraplop", NULL, "decocass", NULL, "1983",
	"Cluster Buster (DECO Cassette) (US)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_PREFIX_DATAEAST, GBF_BREAKOUT, 0,
	NULL, cgraplopRomInfo, cgraplopRomName, NULL, NULL, NULL, NULL, DecocassInputInfo, CgraplopDIPInfo,
	CgraplopInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Graplop (no title screen) (DECO Cassette) (US)

static struct BurnRomInfo cgraplop2RomDesc[] = {
	{ "cgraplop.pro",	0x1000, 0xee93787d, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "cgraplop2.cas",	0x8000, 0x2e728981, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(cgraplop2, cgraplop2, decocass)
STD_ROM_FN(cgraplop2)

static INT32 Cgraplop2Init()
{
	type3_swap = TYPE3_SWAP_67;

	return DecocassInit(decocass_type3_read,decocass_type3_write);
}

struct BurnDriver BurnDrvCgraplop2 = {
	"cgraplop2", "cgraplop", "decocass", NULL, "1983",
	"Graplop (no title screen) (DECO Cassette) (US)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE, 2, HARDWARE_PREFIX_DATAEAST, GBF_BREAKOUT, 0,
	NULL, cgraplop2RomInfo, cgraplop2RomName, NULL, NULL, NULL, NULL, DecocassInputInfo, CgraplopDIPInfo,
	Cgraplop2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Rootin' Tootin' / La-Pa-Pa (DECO Cassette) (US)

static struct BurnRomInfo clapapaRomDesc[] = {
	{ "clapapa.pro",	0x1000, 0xe172819a, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "clapapa.cas",	0x8000, 0x4ffbac24, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(clapapa, clapapa, decocass)
STD_ROM_FN(clapapa)

static INT32 ClapapaInit()
{
	type3_swap = TYPE3_SWAP_34_7;
	burgertime_mode = 1;

	return DecocassInit(decocass_type3_read,decocass_type3_write);
}

struct BurnDriver BurnDrvClapapa = {
	"clapapa", NULL, "decocass", NULL, "1983",
	"Rootin' Tootin' / La-Pa-Pa (DECO Cassette) (US)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_PREFIX_DATAEAST, GBF_MAZE, 0,
	NULL, clapapaRomInfo, clapapaRomName, NULL, NULL, NULL, NULL, DecocassInputInfo, ClapapaDIPInfo,
	ClapapaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Rootin' Tootin' (DECO Cassette) (US)

static struct BurnRomInfo clapapa2RomDesc[] = {
	{ "clapapa.pro",	0x1000, 0xe172819a, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "clapapa2.cas",	0x8000, 0x069dd3c4, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(clapapa2, clapapa2, decocass)
STD_ROM_FN(clapapa2)

struct BurnDriver BurnDrvClapapa2 = {
	"clapapa2", "clapapa", "decocass", NULL, "1983",
	"Rootin' Tootin' (DECO Cassette) (US)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE, 2, HARDWARE_PREFIX_DATAEAST, GBF_MAZE, 0,
	NULL, clapapa2RomInfo, clapapa2RomName, NULL, NULL, NULL, NULL, DecocassInputInfo, ClapapaDIPInfo,
	ClapapaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Skater (DECO Cassette) (Japan)

static struct BurnRomInfo cskaterRomDesc[] = {
	{ "dp-130_a.dgl",	0x1000, 0x469e80a8, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "dt-130_a.cas",	0x8000, 0x1722e5e1, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(cskater, cskater, decocass)
STD_ROM_FN(cskater)

static INT32 CskaterInit()
{
	type3_swap = TYPE3_SWAP_45;
	skater_mode = 1; // object "under tilemap" + 1px correction

	return DecocassInit(decocass_type3_read,decocass_type3_write);
}

struct BurnDriver BurnDrvCskater = {
	"cskater", NULL, "decocass", NULL, "1983",
	"Skater (DECO Cassette) (Japan)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_PREFIX_DATAEAST, GBF_ACTION, 0,
	NULL, cskaterRomInfo, cskaterRomName, NULL, NULL, NULL, NULL, DecocassInputInfo, CskaterDIPInfo,
	CskaterInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Pro Bowling (DECO Cassette) (US)

static struct BurnRomInfo cprobowlRomDesc[] = {
	{ "cprobowl.pro",	0x1000, 0xe3a88e60, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "cprobowl.cas",	0x8000, 0xcb86c5e1, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(cprobowl, cprobowl, decocass)
STD_ROM_FN(cprobowl)

static INT32 CprobowlInit()
{
	type3_swap = TYPE3_SWAP_34_0;

	return DecocassInit(decocass_type3_read,decocass_type3_write);
}

struct BurnDriver BurnDrvCprobowl = {
	"cprobowl", NULL, "decocass", NULL, "1983",
	"Pro Bowling (DECO Cassette) (US)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, cprobowlRomInfo, cprobowlRomName, NULL, NULL, NULL, NULL, DecocassInputInfo, CprobowlDIPInfo,
	CprobowlInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Night Star (DECO Cassette) (US) (set 1)

static struct BurnRomInfo cnightstRomDesc[] = {
	{ "cnightst.pro",	0x1000, 0x553b0fbc, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "cnightst.cas",	0x8000, 0xc6f844cb, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(cnightst, cnightst, decocass)
STD_ROM_FN(cnightst)

static INT32 CnightstInit()
{
	type3_swap = TYPE3_SWAP_13;

	return DecocassInit(decocass_type3_read,decocass_type3_write);
}

struct BurnDriver BurnDrvCnightst = {
	"cnightst", NULL, "decocass", NULL, "1983",
	"Night Star (DECO Cassette) (US) (set 1)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_PREFIX_DATAEAST, GBF_VERSHOOT, 0,
	NULL, cnightstRomInfo, cnightstRomName, NULL, NULL, NULL, NULL, DecocassInputInfo, CnightstDIPInfo,
	CnightstInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Night Star (DECO Cassette) (US) (set 2)

static struct BurnRomInfo cnightst2RomDesc[] = {
	{ "cnightst.pro",	0x1000, 0x553b0fbc, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "cnights2.cas",	0x8000, 0x1a28128c, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(cnightst2, cnightst2, decocass)
STD_ROM_FN(cnightst2)

struct BurnDriver BurnDrvCnightst2 = {
	"cnightst2", "cnightst", "decocass", NULL, "1983",
	"Night Star (DECO Cassette) (US) (set 2)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE, 2, HARDWARE_PREFIX_DATAEAST, GBF_VERSHOOT, 0,
	NULL, cnightst2RomInfo, cnightst2RomName, NULL, NULL, NULL, NULL, DecocassInputInfo, CnightstDIPInfo,
	CnightstInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Pro Soccer (DECO Cassette) (US)

static struct BurnRomInfo cpsoccerRomDesc[] = {
	{ "cprosocc.pro",	0x01000, 0x919fabb2, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "cprosocc.cas",	0x10000, 0x76b1ad2c, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(cpsoccer, cpsoccer, decocass)
STD_ROM_FN(cpsoccer)

static INT32 CpsoccerInit()
{
	type3_swap = TYPE3_SWAP_24;

	return DecocassInit(decocass_type3_read,decocass_type3_write);
}

struct BurnDriver BurnDrvCpsoccer = {
	"cpsoccer", NULL, "decocass", NULL, "1983",
	"Pro Soccer (DECO Cassette) (US)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSFOOTBALL, 0,
	NULL, cpsoccerRomInfo, cpsoccerRomName, NULL, NULL, NULL, NULL, DecocassInputInfo, CpsoccerDIPInfo,
	CpsoccerInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Pro Soccer (DECO Cassette) (Japan)

static struct BurnRomInfo cpsoccerjRomDesc[] = {
	{ "dp-133_a.dgl",	0x01000, 0x919fabb2, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "dt-133_a.cas",	0x10000, 0xde682a29, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(cpsoccerj, cpsoccerj, decocass)
STD_ROM_FN(cpsoccerj)

struct BurnDriver BurnDrvCpsoccerj = {
	"cpsoccerj", "cpsoccer", "decocass", NULL, "1983",
	"Pro Soccer (DECO Cassette) (Japan)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE, 2, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSFOOTBALL, 0,
	NULL, cpsoccerjRomInfo, cpsoccerjRomName, NULL, NULL, NULL, NULL, DecocassInputInfo, CpsoccerjDIPInfo,
	CpsoccerInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Super Doubles Tennis (DECO Cassette) (Japan)

static struct BurnRomInfo csdtenisRomDesc[] = {
	{ "dp-134_a.dgl",	0x01000, 0xe484d2f5, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "dt-134_a.cas",	0x10000, 0x9a69d961, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(csdtenis, csdtenis, decocass)
STD_ROM_FN(csdtenis)

static INT32 CsdtenisInit()
{
	type3_swap = TYPE3_SWAP_23_56;

	return DecocassInit(decocass_type3_read,decocass_type3_write);
}

struct BurnDriver BurnDrvCsdtenis = {
	"csdtenis", NULL, "decocass", NULL, "1983",
	"Super Doubles Tennis (DECO Cassette) (Japan)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, csdtenisRomInfo, csdtenisRomName, NULL, NULL, NULL, NULL, DecocassInputInfo, CsdtenisDIPInfo,
	CsdtenisInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Zeroize (DECO Cassette) (US)

static struct BurnRomInfo czeroizeRomDesc[] = {
	{ "czeroize.pro",	0x01000, 0x00000000, 0 | BRF_NODUMP | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "czeroize.cas",	0x10000, 0x3ef0a406, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(czeroize, czeroize, decocass)
STD_ROM_FN(czeroize)

static INT32 CzeroizeInit()
{
	type3_swap = TYPE3_SWAP_23_56;

	INT32 nRet = DecocassInit(decocass_type3_read,decocass_type3_write);

	if (nRet == 0)
	{
		// hack around missing dongle
		DrvDongle[0x8a0] = 0x18;
		DrvDongle[0x8a1] = 0xf7;

	}

	return nRet;
}

struct BurnDriver BurnDrvCzeroize = {
	"czeroize", NULL, "decocass", NULL, "1983",
	"Zeroize (DECO Cassette) (US)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_PREFIX_DATAEAST, GBF_ACTION, 0,
	NULL, czeroizeRomInfo, czeroizeRomName, NULL, NULL, NULL, NULL, DecocassInputInfo, CzeroizeDIPInfo,
	CzeroizeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Peter Pepper's Ice Cream Factory (DECO Cassette) (US) (set 1)

static struct BurnRomInfo cppicfRomDesc[] = {
	{ "cppicf.pro",		0x1000, 0x0b1a1ecb, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "cppicf.cas",		0x8000, 0x8c02f160, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(cppicf, cppicf, decocass)
STD_ROM_FN(cppicf)

static INT32 CppicfInit()
{
	type3_swap = TYPE3_SWAP_01;

	return DecocassInit(decocass_type3_read,decocass_type3_write);
}

struct BurnDriver BurnDrvCppicf = {
	"cppicf", NULL, "decocass", NULL, "1984",
	"Peter Pepper's Ice Cream Factory (DECO Cassette) (US) (set 1)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_PREFIX_DATAEAST, GBF_PLATFORM, 0,
	NULL, cppicfRomInfo, cppicfRomName, NULL, NULL, NULL, NULL, DecocassInputInfo, CppicfDIPInfo,
	CppicfInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Peter Pepper's Ice Cream Factory (DECO Cassette) (US) (set 2)

static struct BurnRomInfo cppicf2RomDesc[] = {
	{ "cppicf.pro",		0x1000, 0x0b1a1ecb, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "cppicf2.cas",	0x8000, 0x78ffa1bc, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(cppicf2, cppicf2, decocass)
STD_ROM_FN(cppicf2)

struct BurnDriver BurnDrvCppicf2 = {
	"cppicf2", "cppicf", "decocass", NULL, "1984",
	"Peter Pepper's Ice Cream Factory (DECO Cassette) (US) (set 2)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE, 2, HARDWARE_PREFIX_DATAEAST, GBF_PLATFORM, 0,
	NULL, cppicf2RomInfo, cppicf2RomName, NULL, NULL, NULL, NULL, DecocassInputInfo, CppicfDIPInfo,
	CppicfInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Fighting Ice Hockey (DECO Cassette) (US)

static struct BurnRomInfo cfghticeRomDesc[] = {
	{ "cfghtice.pro",	0x01000, 0x5abd27b5, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "cfghtice.cas",	0x10000, 0x906dd7fb, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(cfghtice, cfghtice, decocass)
STD_ROM_FN(cfghtice)

static INT32 CfghticeInit()
{
	type3_swap = TYPE3_SWAP_25;

	return DecocassInit(decocass_type3_read,decocass_type3_write);
}

struct BurnDriver BurnDrvCfghtice = {
	"cfghtice", NULL, "decocass", NULL, "1984",
	"Fighting Ice Hockey (DECO Cassette) (US)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, cfghticeRomInfo, cfghticeRomName, NULL, NULL, NULL, NULL, DecocassInputInfo, CfghticeDIPInfo,
	CfghticeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Scrum Try (DECO Cassette) (US) (set 1)

static struct BurnRomInfo cscrtryRomDesc[] = {
	{ "cscrtry.pro",	0x8000, 0x7bc3460b, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "cscrtry.cas",	0x8000, 0x5625f0ca, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(cscrtry, cscrtry, decocass)
STD_ROM_FN(cscrtry)

static INT32 CscrtryInit()
{
	return DecocassInit(decocass_type4_read,decocass_type4_write);
}

struct BurnDriver BurnDrvCscrtry = {
	"cscrtry", NULL, "decocass", NULL, "1984",
	"Scrum Try (DECO Cassette) (US) (set 1)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, cscrtryRomInfo, cscrtryRomName, NULL, NULL, NULL, NULL, DecocassInputInfo, CscrtryDIPInfo,
	CscrtryInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Scrum Try (DECO Cassette) (US) (set 2)

static struct BurnRomInfo cscrtry2RomDesc[] = {
	{ "cscrtry.pro",	0x8000, 0x7bc3460b, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "cscrtry2.cas",	0x8000, 0x04597842, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(cscrtry2, cscrtry2, decocass)
STD_ROM_FN(cscrtry2)

struct BurnDriver BurnDrvCscrtry2 = {
	"cscrtry2", "cscrtry", "decocass", NULL, "1984",
	"Scrum Try (DECO Cassette) (US) (set 2)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE, 2, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, cscrtry2RomInfo, cscrtry2RomName, NULL, NULL, NULL, NULL, DecocassInputInfo, CscrtryDIPInfo,
	CscrtryInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Oozumou - The Grand Sumo (DECO Cassette) (Japan)

static struct BurnRomInfo coozumouRomDesc[] = {
	{ "dp-141_a.dgl",	0x08000, 0xbc379d2c, 1 | BRF_PRG | BRF_ESS }, //  0 Dongle data

	{ "dt-141_1a.cas",	0x10000, 0x20c2e86a, 2 | BRF_PRG | BRF_ESS }, //  1 Cassette data
};

STDROMPICKEXT(coozumou, coozumou, decocass)
STD_ROM_FN(coozumou)

struct BurnDriver BurnDrvCoozumou = {
	"coozumou", NULL, "decocass", NULL, "1984",
	"Oozumou - The Grand Sumo (DECO Cassette) (Japan)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_PREFIX_DATAEAST, GBF_VSFIGHT, 0,
	NULL, coozumouRomInfo, coozumouRomName, NULL, NULL, NULL, NULL, DecocassInputInfo, CoozumouDIPInfo,
	CscrtryInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Boulder Dash (DECO Cassette) (US)

static struct BurnRomInfo cbdashRomDesc[] = {
	{ "cbdash.cas",		0x8000, 0xcba4c1af, 2 | BRF_PRG | BRF_ESS }, //  0 Cassette data
};

STDROMPICKEXT(cbdash, cbdash, decocass)
STD_ROM_FN(cbdash)

static INT32 CbdashInit()
{
	INT32 nRet = DecocassInit(decocass_type4_read,decocass_type4_write);

	if (nRet == 0)
	{
		memset (DrvDongle, 0x55, 0x8000);
	}

	return nRet;
}

struct BurnDriver BurnDrvCbdash = {
	"cbdash", NULL, "decocass", NULL, "1985",
	"Boulder Dash (DECO Cassette) (US)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_PREFIX_DATAEAST, GBF_MAZE, 0,
	NULL, cbdashRomInfo, cbdashRomName, NULL, NULL, NULL, NULL, DecocassInputInfo, CbdashDIPInfo,
	CbdashInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Flying Ball (DECO Cassette) (US)

static struct BurnRomInfo cflyballRomDesc[] = {
	{ "cflyball.cas",	0x10000, 0xcb40d043, 2 | BRF_PRG | BRF_ESS }, //  0 Cassette data
};

STDROMPICKEXT(cflyball, cflyball, decocass)
STD_ROM_FN(cflyball)

static INT32 CflyballInit()
{
	return DecocassInit(decocass_nodongle_read,NULL);
}

struct BurnDriver BurnDrvCflyball = {
	"cflyball", NULL, "decocass", NULL, "1985",
	"Flying Ball (DECO Cassette) (US)\0", NULL, "Data East Corporation", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_PREFIX_DATAEAST, GBF_BREAKOUT, 0,
	NULL, cflyballRomInfo, cflyballRomName, NULL, NULL, NULL, NULL, DecocassInputInfo, CflyballDIPInfo,
	CflyballInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};





static UINT32 widel_ctrs = 0;
static UINT8 widel_latch = 0;

static UINT8 decocass_widel_read(UINT16 offset)
{
	if (offset & 1)
	{
		if ((offset & E5XX_MASK) == 0)
		{
			if (widel_latch) widel_ctrs = (widel_ctrs + 0x100) & 0xfffff;

			return i8x41_get_register(I8X41_STAT);
		}
	}
	else
	{
		if (widel_latch)
		{
			UINT8 data = DrvDongle[widel_ctrs];
			widel_ctrs = (widel_ctrs + 1) & 0xfffff;
			return data;
		}
		else if ((offset & E5XX_MASK) == 0)
		{
			return i8x41_get_register(I8X41_DATA);
		}
	}

	return 0xff;
}

static void decocass_widel_write(UINT16 offset, UINT8 data)
{
	if (offset & 1)
	{
		if (widel_latch)
		{
			widel_ctrs = 0;
			return;
		}
		else if (0xc0 == (data & 0xf0))
		{
			widel_latch = 1;
		}
	}
	else
	{
		if (widel_latch)
		{
			widel_ctrs = (widel_ctrs & 0xfff00) | data;
			return;
		}
	}

	i8x41_set_register((offset & 1) ? I8X41_CMND : I8X41_DATA, data);
}


// Deco Cassette System Multigame (ROM based)

static struct BurnRomInfo decomultRomDesc[] = {
	{ "widldeco.low",	0x80000, 0xfd4dc36c,  1 | BRF_PRG | BRF_ESS },  //  0 Dongle data
	{ "widldeco.hgh",	0x80000, 0xa8a30112,  1 | BRF_PRG | BRF_ESS },  //  1

	{ "widlbios.v0b",	0x00800, 0x9ad7c451,  8 | BRF_BIOS | BRF_ESS }, //  2 Main M6502 BIOS

	{ "v1-.5a",		0x00800, 0xb66b2c2a,  9 | BRF_BIOS | BRF_ESS }, //  3 Sound M6502 BIOS

	{ "cassmcu.1c", 	0x00400, 0xa6df18fd, 10 | BRF_BIOS | BRF_ESS }, //  4 MCU BIOS

	{ "v2.3m",		0x00020, 0x238fdb40,  0 | BRF_OPT },            //  5 Unknown PROMs
	{ "v4.10d",		0x00020, 0x3b5836b4,  0 | BRF_OPT },            //  6
	{ "v3.3j", 		0x00020, 0x51eef657,  0 | BRF_OPT },            //  7
};

STD_ROM_PICK(decomult)
STD_ROM_FN(decomult)

static INT32 DecomultInit()
{
	return DecocassInit(decocass_widel_read,decocass_widel_write);
}

struct BurnDriver BurnDrvDecomult = {
	"decomult", NULL, "decocass", NULL, "2008",
	"Deco Cassette System Multigame (ROM based)\0", NULL, "bootleg (David Widel)", "Cassette System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_BOOTLEG, 2, HARDWARE_PREFIX_DATAEAST, GBF_MISC, 0,
	NULL, decomultRomInfo, decomultRomName, NULL, NULL, NULL, NULL, DecocassInputInfo, DecomultDIPInfo,
	DecomultInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};
