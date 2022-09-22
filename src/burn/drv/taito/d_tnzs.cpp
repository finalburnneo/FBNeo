// FB Alpha The NewZealand Story driver module
// Based on MAME driver by Chris Moore, Brad Oliver, Nicola Salmoria, and many others

#include "tiles_generic.h"
#include "z80_intf.h"
#include "burn_gun.h"
#include "burn_ym2203.h"
#include "burn_ym2151.h"
#include "tnzs_prot.h"
#include "dac.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvZ80ROM2;
static UINT8 *DrvGfxROM;
static UINT8 *DrvColPROM;
static UINT8 *DrvSndROM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvShareRAM;
static UINT8 *DrvScrollRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvZ80RAM2;
static UINT8 *DrvObjCtrl;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 *coin_lockout;
static UINT8 *soundlatch;
static UINT8 *tnzs_bg_flag;

static INT32  kageki_csport_sel;
static INT32  kageki_sample_pos;
static INT32  kageki_sample_select;
static INT16 *kageki_sample_data[0x30];
static INT32  kageki_sample_size[0x30];
static double kageki_sample_gain;
static INT32 kageki_sample_output_dir;
static INT32 game_kabukiz = 0;

static INT32 cpu1_reset;
static INT32 tnzs_banks[3];

static INT32 system_type;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static INT16 DrvAxis[2];

static INT32 nExtraCycles[3];

#define A(a, b, c, d) { a, b, (UINT8*)(c), d }

static struct BurnInputInfo CommonInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 4,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 5,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 0,	"service"	},
	{"Tilt",			BIT_DIGITAL,	DrvJoy3 + 1,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Common)

static struct BurnInputInfo InsectxInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 3,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 0,	"service"	},
	{"Tilt",			BIT_DIGITAL,	DrvJoy3 + 1,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Insectx)

static struct BurnInputInfo Arknoid2InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	A("P1 Right / left",	BIT_ANALOG_REL, DrvAxis + 0,	"p1 x-axis"	),

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 start"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p2 fire 1"	},

	A("P2 Right / left",	BIT_ANALOG_REL, DrvAxis + 1,	"p2 x-axis"	),

	{"Reset",			BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 5,	"service"	},
	{"Tilt",			BIT_DIGITAL,	DrvJoy2 + 7,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Arknoid2)

static struct BurnInputInfo PlumppopInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 4,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	A("P1 Right / left",	BIT_ANALOG_REL, DrvAxis + 0,	"p1 x-axis"	),

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 5,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	A("P2 Right / left",	BIT_ANALOG_REL, DrvAxis + 1,	"p2 x-axis"	),

	{"Reset",			BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 0,	"service"	},
	{"Tilt",			BIT_DIGITAL,	DrvJoy3 + 1,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Plumppop)

static struct BurnInputInfo JpopnicsInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	A("P1 Right / left",	BIT_ANALOG_REL, DrvAxis + 0,	"p1 x-axis"	),

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 6,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	A("P2 Right / left",	BIT_ANALOG_REL, DrvAxis + 1,	"p2 x-axis"	),

	{"Reset",			BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Jpopnics)

static struct BurnDIPInfo ExtrmatnDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},

	TNZS_CABINET_FLIP_SERVICE_DIPSETTING(0x13)

	TNZS_COINAGE_JAPAN_OLD(0x13)

	TNZS_DIFFICULTY_DIPSETTING(0x14)

	{0   , 0xfe, 0   ,    4, "Damage Multiplier"	},
	{0x14, 0x01, 0xc0, 0xc0, "*1"			},
	{0x14, 0x01, 0xc0, 0x80, "*1.5"			},
	{0x14, 0x01, 0xc0, 0x40, "*2"			},
	{0x14, 0x01, 0xc0, 0x00, "*3"			},
};

STDDIPINFO(Extrmatn)

static struct BurnDIPInfo DrtoppelDIPList[]=
{
	{0x13, 0xff, 0xff, 0xfe, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},

	TNZS_CABINET_FLIP_SERVICE_DIPSETTING(0x13)

	TNZS_DEMOSOUNDS_DIPSETTING(0x13)

	TNZS_COINAGE_WORLD(0x13)

	TNZS_DIFFICULTY_DIPSETTING(0x14)

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x14, 0x01, 0x0c, 0x0c, "30k 100k 200k 100k+"	},
	{0x14, 0x01, 0x0c, 0x00, "50k 100k 200k 200k+"	},
	{0x14, 0x01, 0x0c, 0x04, "30k 100k"		},
	{0x14, 0x01, 0x0c, 0x08, "30k only"		},

	TNZS_LIVES_DIPSETTING(0x14)
};

STDDIPINFO(Drtoppel)

static struct BurnDIPInfo DrtoppluDIPList[]=
{
	{0x13, 0xff, 0xff, 0xfe, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},

	TNZS_CABINET_FLIP_SERVICE_DIPSETTING(0x13)

	TNZS_DEMOSOUNDS_DIPSETTING(0x13)

	TNZS_COINAGE_JAPAN_OLD(0x13)

	TNZS_DIFFICULTY_DIPSETTING(0x14)

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x14, 0x01, 0x0c, 0x0c, "30k 100k 200k 100k+"	},
	{0x14, 0x01, 0x0c, 0x00, "50k 100k 200k 200k+"	},
	{0x14, 0x01, 0x0c, 0x04, "30k 100k"		},
	{0x14, 0x01, 0x0c, 0x08, "30k only"		},

	TNZS_LIVES_DIPSETTING(0x14)
};

STDDIPINFO(Drtopplu)

static struct BurnDIPInfo ChukataiDIPList[]=
{
	{0x13, 0xff, 0xff, 0xfe, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},

	TNZS_CABINET_FLIP_SERVICE_DIPSETTING(0x13)

	TNZS_DEMOSOUNDS_DIPSETTING(0x13)

	TNZS_COINAGE_WORLD(0x13)

	TNZS_DIFFICULTY_DIPSETTING(0x14)

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x14, 0x01, 0x0c, 0x08, "100k 300k 440k"	},
	{0x14, 0x01, 0x0c, 0x00, "100k 300k 500k"	},
	{0x14, 0x01, 0x0c, 0x0c, "100k 400k"		},
	{0x14, 0x01, 0x0c, 0x04, "100k 500k"		},

	TNZS_LIVES_DIPSETTING(0x14)
};

STDDIPINFO(Chukatai)

static struct BurnDIPInfo ChukatauDIPList[]=
{
	{0x13, 0xff, 0xff, 0xfe, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},

	TNZS_CABINET_FLIP_SERVICE_DIPSETTING(0x13)

	TNZS_DEMOSOUNDS_DIPSETTING(0x13)

	TNZS_COINAGE_JAPAN_OLD(0x13)

	TNZS_DIFFICULTY_DIPSETTING(0x14)

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x14, 0x01, 0x0c, 0x08, "100k 300k 440k"	},
	{0x14, 0x01, 0x0c, 0x00, "100k 300k 500k"	},
	{0x14, 0x01, 0x0c, 0x0c, "100k 400k"		},
	{0x14, 0x01, 0x0c, 0x04, "100k 500k"		},

	TNZS_LIVES_DIPSETTING(0x14)
};

STDDIPINFO(Chukatau)

static struct BurnDIPInfo TnzsDIPList[]=
{
	{0x13, 0xff, 0xff, 0xfe, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},

	TNZS_CABINET_FLIP_SERVICE_DIPSETTING(0x13)

	TNZS_COINAGE_WORLD(0x13)

	TNZS_DIFFICULTY_DIPSETTING(0x14)

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x14, 0x01, 0x0c, 0x00, "50k 150k 150k+"	},
	{0x14, 0x01, 0x0c, 0x0c, "70k 200k 200k+"	},
	{0x14, 0x01, 0x0c, 0x04, "100k 250k 250k+"	},
	{0x14, 0x01, 0x0c, 0x08, "200k 300k 300k+"	},

	TNZS_LIVES_DIPSETTING(0x14)

	TNZS_ALLOWCONTINUE_DIPSETTING(0x14, 0x40)
};

STDDIPINFO(Tnzs)

static struct BurnDIPInfo TnzsjDIPList[]=
{
	{0x13, 0xff, 0xff, 0xfe, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},

	TNZS_CABINET_FLIP_SERVICE_DIPSETTING(0x13)

	TNZS_COINAGE_JAPAN_OLD(0x13)

	TNZS_DIFFICULTY_DIPSETTING(0x14)

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x14, 0x01, 0x0c, 0x00, "50k 150k 150k+"	},
	{0x14, 0x01, 0x0c, 0x0c, "70k 200k 200k+"	},
	{0x14, 0x01, 0x0c, 0x04, "100k 250k 250k+"	},
	{0x14, 0x01, 0x0c, 0x08, "200k 300k 300k+"	},

	TNZS_LIVES_DIPSETTING(0x14)

	TNZS_ALLOWCONTINUE_DIPSETTING(0x14, 0x40)
};

STDDIPINFO(Tnzsj)

static struct BurnDIPInfo TnzsjoDIPList[]=
{
	{0x13, 0xff, 0xff, 0xfe, NULL				},
	{0x14, 0xff, 0xff, 0xff, NULL				},

	TNZS_CABINET_FLIP_SERVICE_DIPSETTING(0x13)

	{0   , 0xfe, 0   ,    2, "Invulnerability (Debug)"	},
	{0x14, 0x01, 0x08, 0x08, "Off"				},
	{0x14, 0x01, 0x08, 0x00, "On"				},

	TNZS_COINAGE_JAPAN_OLD(0x13)

	TNZS_DIFFICULTY_DIPSETTING(0x14)

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x14, 0x01, 0x0c, 0x00, "50k 150k 150k+"		},
	{0x14, 0x01, 0x0c, 0x0c, "70k 200k 200k+"		},
	{0x14, 0x01, 0x0c, 0x04, "100k 250k 250k+"		},
	{0x14, 0x01, 0x0c, 0x08, "200k 300k 300k+"		},

	TNZS_LIVES_DIPSETTING(0x14)

	TNZS_ALLOWCONTINUE_DIPSETTING(0x14, 0x40)
};

STDDIPINFO(Tnzsjo)

static struct BurnDIPInfo TnzsopDIPList[]=
{
	{0x13, 0xff, 0xff, 0xfe, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},

	TNZS_CABINET_FLIP_SERVICE_DIPSETTING(0x13)

	TNZS_COINAGE_WORLD(0x13)

	TNZS_DIFFICULTY_DIPSETTING(0x14)

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x14, 0x01, 0x0c, 0x00, "10k 100k 100k+"	},
	{0x14, 0x01, 0x0c, 0x0c, "10k 150k 150k+"	},
	{0x14, 0x01, 0x0c, 0x04, "10k 200k 200k+"	},
	{0x14, 0x01, 0x0c, 0x08, "10k 300k 300k+"	},

	TNZS_LIVES_DIPSETTING(0x14)

	TNZS_ALLOWCONTINUE_DIPSETTING(0x14, 0x40)
};

STDDIPINFO(Tnzsop)

static struct BurnDIPInfo InsectxDIPList[]=
{
	{0x13, 0xff, 0xff, 0xfe, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},

	TNZS_CABINET_FLIP_SERVICE_DIPSETTING(0x13)

	TNZS_DEMOSOUNDS_DIPSETTING(0x13)

	TNZS_COINAGE_WORLD(0x13)

	TNZS_DIFFICULTY_DIPSETTING(0x14)

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x14, 0x01, 0x0c, 0x08, "100k 200k 300k 440k"	},
	{0x14, 0x01, 0x0c, 0x0c, "100k 400k"		},
	{0x14, 0x01, 0x0c, 0x04, "100k 500k"		},
	{0x14, 0x01, 0x0c, 0x00, "150000 Only"		},

	TNZS_LIVES_DIPSETTING(0x14)
};

STDDIPINFO(Insectx)

static struct BurnDIPInfo InsectxjDIPList[]=
{
	{0x13, 0xff, 0xff, 0xfe, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},

	TNZS_CABINET_FLIP_SERVICE_DIPSETTING(0x13)

	TNZS_DEMOSOUNDS_DIPSETTING(0x13)

	TNZS_COINAGE_JAPAN_OLD(0x13)

	TNZS_DIFFICULTY_DIPSETTING(0x14)

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x14, 0x01, 0x0c, 0x08, "100k 200k 300k 440k"	},
	{0x14, 0x01, 0x0c, 0x0c, "100k 400k"		},
	{0x14, 0x01, 0x0c, 0x04, "100k 500k"		},
	{0x14, 0x01, 0x0c, 0x00, "150000 Only"		},

	TNZS_LIVES_DIPSETTING(0x14)
};

STDDIPINFO(Insectxj)

static struct BurnDIPInfo KagekiDIPList[]=
{
	{0x13, 0xff, 0xff, 0xfe, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},

	// Cabinet type in Japan set only
	TNZS_CABINET_FLIP_SERVICE_DIPSETTING(0x13)

	TNZS_DEMOSOUNDS_DIPSETTING(0x13)

	TNZS_COINAGE_JAPAN_OLD(0x13)

	TNZS_DIFFICULTY_DIPSETTING(0x14)

	TNZS_ALLOWCONTINUE_DIPSETTING(0x14, 0x80)
};

STDDIPINFO(Kageki)

static struct BurnDIPInfo Arknoid2DIPList[]=
{
	{0x0b, 0xff, 0xff, 0xfe, NULL			},
	{0x0c, 0xff, 0xff, 0x7f, NULL			},

	TNZS_CABINET_FLIP_SERVICE_DIPSETTING(0x0b)

	TNZS_DEMOSOUNDS_DIPSETTING(0x0b)

	TNZS_COINAGE_WORLD(0x0b)

	TNZS_DIFFICULTY_DIPSETTING(0x0c)

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x0c, 0x01, 0x0c, 0x00, "50k 150k"		},
	{0x0c, 0x01, 0x0c, 0x0c, "100k 200k"		},
	{0x0c, 0x01, 0x0c, 0x04, "50k Only"		},
	{0x0c, 0x01, 0x0c, 0x08, "100k Only"		},

	TNZS_LIVES_DIPSETTING(0x0c)

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x0c, 0x01, 0x80, 0x80, "No"			},
	{0x0c, 0x01, 0x80, 0x00, "Yes"			},
};

STDDIPINFO(Arknoid2)

static struct BurnDIPInfo Arknid2uDIPList[]=
{
	{0x0b, 0xff, 0xff, 0xfe, NULL			},
	{0x0c, 0xff, 0xff, 0x7f, NULL			},

	TNZS_CABINET_FLIP_SERVICE_DIPSETTING(0x0b)

	TNZS_DEMOSOUNDS_DIPSETTING(0x0b)

	TNZS_COINAGE_JAPAN_OLD(0x0b)

	TNZS_DIFFICULTY_DIPSETTING(0x0c)

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x0c, 0x01, 0x0c, 0x00, "50k 150k"		},
	{0x0c, 0x01, 0x0c, 0x0c, "100k 200k"		},
	{0x0c, 0x01, 0x0c, 0x04, "50k Only"		},
	{0x0c, 0x01, 0x0c, 0x08, "100k Only"		},

	TNZS_LIVES_DIPSETTING(0x0c)

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x0c, 0x01, 0x80, 0x80, "No"			},
	{0x0c, 0x01, 0x80, 0x00, "Yes"			},
};

STDDIPINFO(Arknid2u)

static struct BurnDIPInfo PlumppopDIPList[]=
{
	{0x0d, 0xff, 0xff, 0xff, NULL			},
	{0x0e, 0xff, 0xff, 0xff, NULL			},

	TNZS_CABINET_FLIP_SERVICE_DIPSETTING(0x0d)

	TNZS_DEMOSOUNDS_DIPSETTING(0x0d)

	TNZS_COINAGE_JAPAN_OLD(0x0d)

	TNZS_DIFFICULTY_DIPSETTING(0x0e)

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x0e, 0x01, 0x0c, 0x08, "50k 200k 150k+"	},
	{0x0e, 0x01, 0x0c, 0x0c, "50k 250k 200k+"	},
	{0x0e, 0x01, 0x0c, 0x04, "100k 300k 200k+"	},
	{0x0e, 0x01, 0x0c, 0x00, "100k 400k 300k+"	},

	TNZS_LIVES_DIPSETTING(0x0e)

	TNZS_ALLOWCONTINUE_DIPSETTING(0x0e, 0x80)
};

STDDIPINFO(Plumppop)

static struct BurnDIPInfo JpopnicsDIPList[]=
{
	{0x0b, 0xff, 0xff, 0xff, NULL			},
	{0x0c, 0xff, 0xff, 0xff, NULL			},

	TNZS_CABINET_FLIP_SERVICE_DIPSETTING(0x0b)

	TNZS_DEMOSOUNDS_DIPSETTING(0x0b)

	TNZS_DIFFICULTY_DIPSETTING(0x0c)

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x0c, 0x01, 0x0c, 0x08, "50k 200k 150k+"	},
	{0x0c, 0x01, 0x0c, 0x0c, "50k 250k 200k+"	},
	{0x0c, 0x01, 0x0c, 0x04, "100k 300k 200k+"	},
	{0x0c, 0x01, 0x0c, 0x00, "100k 400k 300k+"	},

	TNZS_LIVES_DIPSETTING(0x0c)

	TNZS_ALLOWCONTINUE_DIPSETTING(0x0c, 0x80)
};

STDDIPINFO(Jpopnics)

static struct BurnDIPInfo KabukizDIPList[]=
{
	{0x13, 0xff, 0xff, 0xfe, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},

	TNZS_CABINET_FLIP_SERVICE_DIPSETTING(0x13)

	TNZS_DEMOSOUNDS_DIPSETTING(0x13)

	TNZS_ALLOWCONTINUE_DIPSETTING(0x13, 0x80)

	TNZS_DIFFICULTY_DIPSETTING(0x14)

	TNZS_COINAGE_WORLD(0x14)
};

STDDIPINFO(Kabukiz)

static struct BurnDIPInfo KabukizjDIPList[]=
{
	{0x13, 0xff, 0xff, 0xfe, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},

	TNZS_CABINET_FLIP_SERVICE_DIPSETTING(0x13)

	TNZS_DEMOSOUNDS_DIPSETTING(0x13)

	TNZS_ALLOWCONTINUE_DIPSETTING(0x13, 0x80)

	TNZS_DIFFICULTY_DIPSETTING(0x14)

	TNZS_COINAGE_JAPAN_OLD(0x14)
};

STDDIPINFO(Kabukizj)

static void __fastcall bankswitch0(UINT8 data)
{
	// CPU #0 expects CPU #1 to be stopped while reset line is triggered.
	if ((~data & 0x10) != cpu1_reset) {
		INT32 cycles = ZetTotalCycles();
		ZetCPUPush(1);
		cycles -= ZetTotalCycles();
		if (cycles > 0) ZetIdle(cycles);
		if (~data & 0x10) ZetReset();
		ZetCPUPop();
	}

	cpu1_reset = ~data & 0x10;

	tnzs_banks[0] = data;

	INT32 bank = (data & 7) * 0x4000;

	// Low banks are RAM regions high ones are ROM.
	if ((data & 6) == 0) {
		ZetMapMemory(DrvZ80RAM0 + bank, 0x8000, 0xbfff, MAP_RAM);
	} else {
		ZetUnmapMemory(0x8000, 0xbfff, MAP_RAM); // ? just incase? -dink
		ZetMapMemory(DrvZ80ROM0 + 0x10000 + bank, 0x8000, 0xbfff, MAP_ROM);
	}
}

static void __fastcall bankswitch1(UINT8 data)
{
	tnzs_banks[1] = data & ~0x04;

	if (data & 0x04) {
		tnzs_mcu_reset();
	}

	*coin_lockout = ~data & 0x30;

	ZetMapMemory(DrvZ80ROM1 + 0x08000 + 0x2000 * (data & 3), 0x8000, 0x9fff, MAP_ROM);
}

static void __fastcall tnzs_cpu0_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xf400:
			*tnzs_bg_flag = data;
		break;

		case 0xf600:
			bankswitch0(data);
		break;
	}

	if ((address & 0xff00) == 0xf300) {
		DrvObjCtrl[address & 3] = data;
		return;
	}
}

static UINT8 __fastcall tnzs_cpu0_read(UINT16 address)
{
	// This is a hack to keep tnzs & clones from freezing.  The sub cpu
	// writes back 0xff to shared ram as an acknowledge and the main
	// cpu doesn't expect it so soon. This can be fixed by an extremely 
	// high sync, but this hack is much less costly.
	if ((address & 0xf000) == 0xe000) {
		if (address == 0xef10 && DrvShareRAM[0x0f10] == 0xff) {
			return 0;
		}
		return DrvShareRAM[address & 0xfff];
	}

	return 0;
}

static void __fastcall tnzsb_cpu1_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xa000:
			bankswitch1(data);
		break;

		case 0xb004:
			*soundlatch = data;
			//ZetSetVector(2, 0xff);
			ZetSetIRQLine(2, 0, CPU_IRQSTATUS_HOLD);
		break;
	}
}

static UINT8 __fastcall tnzsb_cpu1_read(UINT16 address)
{
	switch (address)
	{
		case 0xb002:
			return DrvDips[0];

		case 0xb003:
			return DrvDips[1];

		case 0xc000:
		case 0xc001:
			return DrvInputs[address & 1];

		case 0xc002:
			return DrvInputs[2] | *coin_lockout;
	}

	if (address >= 0xf000 && address <= 0xf003) {
		return DrvPalRAM[address & 0x0003];
	}

	return 0;
}

static void __fastcall tnzs_cpu1_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xa000:
			bankswitch1(data);
		break;

		case 0xb000:
			if (tnzs_mcu_type() == MCU_NONE_JPOPNICS) {
				BurnYM2151SelectRegister(data);
			} else {
				BurnYM2203Write(0, 0, data);
			}
		break;

		case 0xb001:
			if (tnzs_mcu_type() == MCU_NONE_JPOPNICS) {
				BurnYM2151WriteRegister(data);
			} else {
				BurnYM2203Write(0, 1, data);
			}
		break;

		case 0xc000:
		case 0xc001:
			tnzs_mcu_write(address, data);
		break;
	}
}

static UINT8 __fastcall tnzs_cpu1_read(UINT16 address)
{
	switch (address)
	{
		case 0xb000:
			if (tnzs_mcu_type() == MCU_NONE_JPOPNICS)
				return 0;
			return BurnYM2203Read(0, 0);

		case 0xb001:
			if (tnzs_mcu_type() == MCU_NONE_JPOPNICS)
				return BurnYM2151Read();
			return BurnYM2203Read(0, 1);

		case 0xc000:
		case 0xc001:
			return tnzs_mcu_read(address);

		case 0xc002:
			return DrvInputs[2];

		case 0xc600:
			return DrvDips[0];

		case 0xc601:
			return DrvDips[1];

		case 0xf000:
		case 0xf001:
		case 0xf002:
		case 0xf003: {
			UINT32 t = BurnTrackballReadWord(0, (address & 3) >> 1) & 0xfff;
			if (address & 1)
				return (t >> 8) & 0xff;
			else
				return t & 0xff;
		}
	}

	return 0;
}

static void __fastcall tnzs_cpu2_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			BurnYM2203Write(0, 0, data);
		break;

		case 0x01:
			BurnYM2203Write(0, 1, data);
		break;
	}
}

static UINT8 __fastcall tnzs_cpu2_in(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
			return BurnYM2203Read(0, 0);

		case 0x02:
			return *soundlatch;
	}

	return 0;
}

static void kabukiz_sound_bankswitch(UINT32, UINT32 data)
{
	if (game_kabukiz && data != 0xff) {
		tnzs_banks[2] = data;

		if (ZetGetActive() == -1) return;

		ZetMapMemory(DrvZ80ROM2 + 0x4000 * (data & 0x07), 0x8000, 0xbfff, MAP_ROM);
	}
}

static void kabukiz_dac_write(UINT32, UINT32 data)
{
	if (game_kabukiz && data != 0xff) {
		if (ZetGetActive() == -1) return;

		DACSignedWrite(0, data);
	}
}

static UINT8 tnzs_ym2203_portA(UINT32 )
{
	return DrvDips[0];
}

static UINT8 tnzs_ym2203_portB(UINT32 )
{
	return DrvDips[1];
}

static UINT8 kageki_ym2203_portA(UINT32)
{
	INT32 dsw1 = DrvDips[0];
	INT32 dsw2 = DrvDips[1];
	UINT8 dsw = 0x0f;

	switch (kageki_csport_sel & 3)
	{
		case 0x00:
			dsw = (((dsw2 & 0x10) >> 1) | ((dsw2 & 0x01) << 2) | ((dsw1 & 0x10) >> 3) | ((dsw1 & 0x01) >> 0));
			break;

		case 0x01:
			dsw = (((dsw2 & 0x40) >> 3) | ((dsw2 & 0x04) >> 0) | ((dsw1 & 0x40) >> 5) | ((dsw1 & 0x04) >> 2));
			break;

		case 0x02:
			dsw = (((dsw2 & 0x20) >> 2) | ((dsw2 & 0x02) << 1) | ((dsw1 & 0x20) >> 4) | ((dsw1 & 0x02) >> 1));
			break;

		case 0x03:
			dsw = (((dsw2 & 0x80) >> 4) | ((dsw2 & 0x08) >> 1) | ((dsw1 & 0x80) >> 6) | ((dsw1 & 0x08) >> 3));
			break;
	}

	return dsw;
}

static void kageki_ym2203_write_portB(UINT32, UINT32 data)
{
	if (data > 0x3f) {
		kageki_csport_sel = data;
	} else {
		if (data > 0x2f) {
			kageki_sample_select = -1;
		} else {
			kageki_sample_select = data;
			kageki_sample_pos = 0;
		}
	}
}

inline static void DrvYM2203IRQHandler(INT32, INT32 nStatus)
{
	ZetSetIRQLine(CPU_IRQLINE_NMI, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);
	memcpy (DrvPalRAM, DrvColPROM, 0x400);

	cpu1_reset = 0;

	for (INT32 i = 0; i < 3; i++) {
		ZetOpen(i);
		ZetReset();
		if (i == 0) bankswitch0(0x12);
		if (i == 1) bankswitch1(0);
		if (i == 2) kabukiz_sound_bankswitch(0, 0);
		ZetClose();
	}

	tnzs_mcu_reset();

	ZetOpen(1);
	if (tnzs_mcu_type() == MCU_NONE_JPOPNICS) {
		BurnYM2151Reset();
	} else {
		BurnYM2203Reset();
	}
	ZetClose();

	DACReset();

	kageki_sample_pos = 0;
	kageki_sample_select = -1;
	kageki_csport_sel = 0;

	nExtraCycles[0] = nExtraCycles[1] = nExtraCycles[2] = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x040000;
	DrvZ80ROM1		= Next; Next += 0x020000;
	DrvZ80ROM2		= Next; Next += 0x020000;

	DrvGfxROM		= Next; Next += 0x400000;

	DrvColPROM		= Next; Next += 0x000400;

	DrvSndROM		= Next; Next += 0x010000;

	DrvPalette		= (UINT32*)Next; Next += 0x0200 * sizeof(UINT32);

	AllRam			= Next;

	DrvObjCtrl		= Next; Next += 0x000004;

	DrvPalRAM		= Next; Next += 0x000400;
	DrvSprRAM		= Next; Next += 0x002000;
	DrvShareRAM		= Next; Next += 0x001000;
	DrvScrollRAM	= Next; Next += 0x000100;
	DrvVidRAM		= Next; Next += 0x000200;

	DrvZ80RAM0		= Next; Next += 0x008000;
	DrvZ80RAM1		= Next; Next += 0x001000;
	DrvZ80RAM2		= Next; Next += 0x002000;

	coin_lockout	= Next; Next += 0x000001;
	soundlatch		= Next; Next += 0x000001;
	tnzs_bg_flag	= Next; Next += 0x000001;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void kageki_sample_init()
{
	UINT8 *src = DrvSndROM + 0x0090;

	for (INT32 i = 0; i < 0x2f; i++)
	{
		INT32 start = (src[(i * 2) + 1] << 8) + src[(i * 2)];
		UINT8 *scan = &src[start];
		INT32 size = 0;

		while (1) {
			if (*scan++ == 0x00) {
				break;
			} else {
				size++;
			}
		}

		kageki_sample_data[i] = (INT16*)BurnMalloc(size * sizeof(INT16));
		kageki_sample_size[i] = size;

		if (start < 0x100) start = size = 0;

		INT16 *dest = kageki_sample_data[i];
		scan = &src[start];
		for (INT32 n = 0; n < size; n++)
		{
			*dest++ = ((*scan++) ^ 0x80) << 8;
		}
	}

	kageki_sample_gain = 0.45;
	kageki_sample_output_dir = BURN_SND_ROUTE_BOTH;
}

static void kageki_sample_exit()
{
	for (INT32 i = 0; i < 0x2f; i++) {
		BurnFree (kageki_sample_data[i]);
	}
	kageki_sample_select = -1;
}

static INT32 tnzs_gfx_decode()
{
	UINT8 *tmp = (UINT8*)BurnMalloc(0x200000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM, 0x200000);

	static INT32 Plane[4]  = { 0xc00000, 0x800000, 0x400000, 0x000000 };
	static INT32 XOffs[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 64, 65, 66, 67, 68, 69, 70, 71 };
	static INT32 YOffs[16] = { 0, 8, 16, 24, 32, 40, 48, 56, 128, 136, 144, 152, 160, 168, 176, 184 };

	GfxDecode(0x4000, 4, 16, 16, Plane, XOffs, YOffs, 0x100, tmp, DrvGfxROM);

	// the drawing routines allow for 0x4000 tiles, but some (most) games
	// only have 0x2000. Mirroring the second half is cheaper and easier than
	// masking the tile code.
	if (memcmp (DrvGfxROM + 0x200000, DrvGfxROM + 0x300000, 0x100000) == 0) {
		memcpy (DrvGfxROM + 0x200000, DrvGfxROM + 0x000000, 0x200000);
	}

	BurnFree (tmp);

	return 0;
}

static INT32 insectx_gfx_decode()
{
	UINT8 *tmp = (UINT8*)BurnMalloc(0x100000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM, 0x100000);

	static INT32 Plane[4]  = { 0x000008, 0x000000, 0x400008, 0x400000 };
	static INT32 XOffs[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 128, 129, 130, 131, 132, 133, 134, 135 };
	static INT32 YOffs[16] = { 0, 16, 32, 48, 64, 80, 96, 112, 256, 272, 288, 304, 320, 336, 352, 368 };

	GfxDecode(0x2000, 4, 16, 16, Plane, XOffs, YOffs, 0x200, tmp, DrvGfxROM);

	memcpy (DrvGfxROM + 0x200000, DrvGfxROM + 0x000000, 0x200000);

	BurnFree (tmp);

	return 0;
}

static INT32 Type1Init(INT32 mcutype)
{
	BurnAllocMemIndex();

	system_type = 1;

	switch (mcutype)
	{
		case MCU_CHUKATAI:
		{
			if (BurnLoadRom(DrvZ80ROM0 + 0x10000,  0, 1)) return 1;
			memcpy (DrvZ80ROM0 + 0x00000, DrvZ80ROM0 + 0x10000, 0x08000);
			if (BurnLoadRom(DrvZ80ROM0 + 0x20000,  1, 1)) return 1;

			if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  2, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM + 0x000000,  4, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x020000,  5, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x080000,  6, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x0a0000,  7, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x100000,  8, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x120000,  9, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x180000, 10, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x1a0000, 11, 1)) return 1;

			if (tnzs_gfx_decode()) return 1;
		}
		break;

		case MCU_TNZS:
		{
			if (BurnLoadRom(DrvZ80ROM0 + 0x10000, 0, 1)) return 1;
			memcpy (DrvZ80ROM0 + 0x00000, DrvZ80ROM0 + 0x10000, 0x08000);
			if (BurnLoadRom(DrvZ80ROM1 + 0x00000, 1, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM + 0x000000,  3, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x020000,  4, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x080000,  5, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x0a0000,  6, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x100000,  7, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x120000,  8, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x180000,  9, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x1a0000, 10, 1)) return 1;

			if (tnzs_gfx_decode()) return 1;
		}
		break;

		case MCU_DRTOPPEL:
		{
			if (BurnLoadRom(DrvZ80ROM0 + 0x10000, 0, 1)) return 1;
			memcpy (DrvZ80ROM0 + 0x00000, DrvZ80ROM0 + 0x10000, 0x08000);
			if (BurnLoadRom(DrvZ80ROM0 + 0x20000, 1, 1)) return 1;

			if (BurnLoadRom(DrvZ80ROM1 + 0x00000, 2, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM + 0x000000,  4, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x020000,  5, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x080000,  6, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x0a0000,  7, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x100000,  8, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x120000,  9, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x180000, 10, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x1a0000, 11, 1)) return 1;

			if (BurnLoadRom(DrvColPROM + 0x00001, 12, 2)) return 1;
			if (BurnLoadRom(DrvColPROM + 0x00000, 13, 2)) return 1;

			if (tnzs_gfx_decode()) return 1;
		}
		break;

		case MCU_EXTRMATN:
		{
			if (BurnLoadRom(DrvZ80ROM0 + 0x10000,  0, 1)) return 1;
			memcpy (DrvZ80ROM0 + 0x00000, DrvZ80ROM0 + 0x10000, 0x08000);
			if (BurnLoadRom(DrvZ80ROM0 + 0x20000,  1, 1)) return 1;

			if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  2, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM + 0x000000,  4, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x080000,  5, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x100000,  6, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x180000,  7, 1)) return 1;

			if (BurnLoadRom(DrvColPROM + 0x00001,  8, 2)) return 1;
			if (BurnLoadRom(DrvColPROM + 0x00000,  9, 2)) return 1;

			if (tnzs_gfx_decode()) return 1;
		}
		break;

		case MCU_NONE_INSECTX:
		{
			if (BurnLoadRom(DrvZ80ROM0 + 0x10000,  0, 1)) return 1;
			memcpy (DrvZ80ROM0 + 0x00000, DrvZ80ROM0 + 0x10000, 0x08000);

			if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  1, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM + 0x000000,  2, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x080000,  3, 1)) return 1;

			insectx_gfx_decode();
		}
		break;

		case MCU_ARKANOID:
		{
			if (BurnLoadRom(DrvZ80ROM0 + 0x10000,  0, 1)) return 1;
			memcpy (DrvZ80ROM0 + 0x00000, DrvZ80ROM0 + 0x10000, 0x08000);

			if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  1, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM + 0x000000,  3, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x080000,  4, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x100000,  5, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x180000,  6, 1)) return 1;

			if (BurnLoadRom(DrvColPROM + 0x00001,  7, 2)) return 1;
			if (BurnLoadRom(DrvColPROM + 0x00000,  8, 2)) return 1;

			if (tnzs_gfx_decode()) return 1;
		}
		break;

		case MCU_PLUMPOP:
		{
			if (BurnLoadRom(DrvZ80ROM0 + 0x10000,  0, 1)) return 1;
			memcpy (DrvZ80ROM0 + 0x00000, DrvZ80ROM0 + 0x10000, 0x08000);
			if (BurnLoadRom(DrvZ80ROM0 + 0x20000,  1, 1)) return 1;

			if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  2, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM + 0x000000,  4, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x010000,  4, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x020000,  5, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x030000,  5, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x080000,  6, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x090000,  6, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x0a0000,  7, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x0b0000,  7, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x100000,  8, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x110000,  8, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x120000,  9, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x130000,  9, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x180000, 10, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x190000, 10, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x1a0000, 11, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x1b0000, 11, 1)) return 1;

			if (BurnLoadRom(DrvColPROM + 0x00001, 12, 2)) return 1;
			if (BurnLoadRom(DrvColPROM + 0x00000, 13, 2)) return 1;

			if (tnzs_gfx_decode()) return 1;
		}
		break;

		case MCU_NONE_KAGEKI:
		{
			if (BurnLoadRom(DrvZ80ROM0 + 0x10000,  0, 1)) return 1;
			memcpy (DrvZ80ROM0 + 0x00000, DrvZ80ROM0 + 0x10000, 0x08000);
			if (BurnLoadRom(DrvZ80ROM0 + 0x20000,  1, 1)) return 1;

			if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  2, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM + 0x000000,  3, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x020000,  4, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x080000,  5, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x0a0000,  6, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x100000,  7, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x120000,  8, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x180000,  9, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x1a0000, 10, 1)) return 1;

			if (BurnLoadRom(DrvSndROM + 0x000000, 11, 1)) return 1;

			if (tnzs_gfx_decode()) return 1;

			kageki_sample_init();
		}
		break;

		case MCU_NONE_JPOPNICS:
		{
			if (BurnLoadRom(DrvZ80ROM0 + 0x10000,  0, 1)) return 1;
			memcpy (DrvZ80ROM0 + 0x00000, DrvZ80ROM0 + 0x10000, 0x08000);

			if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  1, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM + 0x000000,  2, 1)) return 1;
			memcpy (DrvGfxROM + 0x020000, DrvGfxROM + 0x010000, 0x010000);
			if (BurnLoadRom(DrvGfxROM + 0x080000,  3, 1)) return 1;
			memcpy (DrvGfxROM + 0x0a0000, DrvGfxROM + 0x090000, 0x010000);
			if (BurnLoadRom(DrvGfxROM + 0x100000,  4, 1)) return 1;
			memcpy (DrvGfxROM + 0x120000, DrvGfxROM + 0x110000, 0x010000);
			if (BurnLoadRom(DrvGfxROM + 0x180000,  5, 1)) return 1;
			memcpy (DrvGfxROM + 0x1a0000, DrvGfxROM + 0x190000, 0x010000);

			if (tnzs_gfx_decode()) return 1;
		}
		break;
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapArea(0x0000, 0x7fff, 0, DrvZ80ROM0);
	ZetMapArea(0x0000, 0x7fff, 2, DrvZ80ROM0);
	ZetMapArea(0x8000, 0xbfff, 0, DrvZ80ROM0 + 0x18000);
	ZetMapArea(0x8000, 0xbfff, 2, DrvZ80ROM0 + 0x18000);
	ZetMapArea(0xc000, 0xdfff, 0, DrvSprRAM);
	ZetMapArea(0xc000, 0xdfff, 1, DrvSprRAM);
	ZetMapArea(0xe000, 0xeeff, 0, DrvShareRAM);
	if (mcutype != MCU_TNZS) {
		ZetMapArea(0xef00, 0xefff, 0, DrvShareRAM + 0x0f00);
	}
	ZetMapArea(0xe000, 0xefff, 1, DrvShareRAM);
	ZetMapArea(0xe000, 0xefff, 2, DrvShareRAM);
	ZetMapArea(0xf000, 0xf1ff, 0, DrvVidRAM);
	ZetMapArea(0xf000, 0xf1ff, 1, DrvVidRAM);
	ZetMapArea(0xf200, 0xf2ff, 1, DrvScrollRAM);
	if (mcutype != MCU_DRTOPPEL) {
		ZetMapArea(0xf800, 0xfbff, 0, DrvPalRAM);
		ZetMapArea(0xf800, 0xfbff, 1, DrvPalRAM);
	}
	ZetSetWriteHandler(tnzs_cpu0_write);
	ZetSetReadHandler(tnzs_cpu0_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapArea(0x0000, 0x9fff, 0, DrvZ80ROM1);
	ZetMapArea(0x0000, 0x9fff, 2, DrvZ80ROM1);
	ZetMapArea(0xd000, 0xdfff, 0, DrvZ80RAM1);
	ZetMapArea(0xd000, 0xdfff, 1, DrvZ80RAM1);
	ZetMapArea(0xd000, 0xdfff, 2, DrvZ80RAM1);
	ZetMapArea(0xe000, 0xefff, 0, DrvShareRAM);
	ZetMapArea(0xe000, 0xefff, 1, DrvShareRAM);
	ZetMapArea(0xe000, 0xefff, 2, DrvShareRAM);
	ZetSetWriteHandler(tnzs_cpu1_write);
	ZetSetReadHandler(tnzs_cpu1_read);
	ZetClose();

	ZetInit(2); // For common reset routine

	tnzs_mcu_init(mcutype);

	if (mcutype == MCU_NONE_JPOPNICS) {
		BurnYM2151InitBuffered(3000000, 1, NULL, 0); // jpopnics
		BurnYM2151SetAllRoutes(0.30, BURN_SND_ROUTE_BOTH);
		BurnTimerAttachZet(6000000);
	} else {
		BurnYM2203Init(1, 3000000, NULL, 0);
		BurnYM2203SetAllRoutes(0, 0.30, BURN_SND_ROUTE_BOTH);
		BurnTimerAttachZet(6000000);

		if (mcutype == MCU_EXTRMATN || mcutype == MCU_DRTOPPEL) {
			BurnYM2203SetPSGVolume(0, 0.10);
		}

		if (mcutype == MCU_NONE_KAGEKI) {
			BurnYM2203SetPorts(0, &kageki_ym2203_portA, NULL, NULL, &kageki_ym2203_write_portB);
			BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE, 0.35, BURN_SND_ROUTE_BOTH);
			BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.15, BURN_SND_ROUTE_BOTH);
			BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.15, BURN_SND_ROUTE_BOTH);
			BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.15, BURN_SND_ROUTE_BOTH);
		} else {
			BurnYM2203SetPorts(0, &tnzs_ym2203_portA, &tnzs_ym2203_portB, NULL, NULL);
		}
	}

	DACInit(0, 0, 1, ZetTotalCycles, 6000000); // kabukiz
	DACSetRoute(0, 0.50, BURN_SND_ROUTE_BOTH);

	BurnTrackballInit(2);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 Type2Init()
{
	BurnAllocMemIndex();

	system_type = 2;

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x10000, 0, 1)) return 1;
		memcpy (DrvZ80ROM0, DrvZ80ROM0 + 0x10000, 0x08000);

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000, 1, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM2 + 0x00000, 2, 1)) return 1;

		if (game_kabukiz) {
			if (BurnLoadRom(DrvGfxROM + 0x000000,  3, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x080000,  4, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x100000,  5, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x180000,  6, 1)) return 1;
		} else {
			if (BurnLoadRom(DrvGfxROM + 0x000000,  3, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x020000,  4, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x080000,  5, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x0a0000,  6, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x100000,  7, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x120000,  8, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x180000,  9, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x1a0000, 10, 1)) return 1;
		}

		if (tnzs_gfx_decode()) return 1;
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapArea(0x0000, 0x7fff, 0, DrvZ80ROM0);
	ZetMapArea(0x0000, 0x7fff, 2, DrvZ80ROM0);
	ZetMapArea(0x8000, 0xbfff, 0, DrvZ80ROM0 + 0x18000);
	ZetMapArea(0x8000, 0xbfff, 1, DrvZ80ROM0 + 0x18000);
	ZetMapArea(0x8000, 0xbfff, 2, DrvZ80ROM0 + 0x18000);
	ZetMapArea(0xc000, 0xdfff, 0, DrvSprRAM);
	ZetMapArea(0xc000, 0xdfff, 1, DrvSprRAM);
	ZetMapArea(0xc000, 0xdfff, 2, DrvSprRAM);
	ZetMapArea(0xe000, 0xeeff, 0, DrvShareRAM);
	if (game_kabukiz) {
		ZetMapArea(0xef00, 0xefff, 0, DrvShareRAM + 0x0f00);
	}
	ZetMapArea(0xe000, 0xefff, 1, DrvShareRAM);
	ZetMapArea(0xe000, 0xefff, 2, DrvShareRAM);
	ZetMapArea(0xf000, 0xf1ff, 0, DrvVidRAM);
	ZetMapArea(0xf000, 0xf1ff, 1, DrvVidRAM);
	ZetMapArea(0xf000, 0xf1ff, 2, DrvVidRAM);
	ZetMapArea(0xf200, 0xf2ff, 0, DrvScrollRAM);
	ZetMapArea(0xf200, 0xf2ff, 1, DrvScrollRAM);
	ZetSetWriteHandler(tnzs_cpu0_write);
	ZetSetReadHandler(tnzs_cpu0_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapArea(0x0000, 0x9fff, 0, DrvZ80ROM1);
	ZetMapArea(0x0000, 0x9fff, 2, DrvZ80ROM1);
	ZetMapArea(0xd000, 0xdfff, 0, DrvZ80RAM1);
	ZetMapArea(0xd000, 0xdfff, 1, DrvZ80RAM1);
	ZetMapArea(0xd000, 0xdfff, 2, DrvZ80RAM1);
	ZetMapArea(0xe000, 0xefff, 0, DrvShareRAM);
	ZetMapArea(0xe000, 0xefff, 1, DrvShareRAM);
	ZetMapArea(0xe000, 0xefff, 2, DrvShareRAM);
	ZetMapArea(0xf000, 0xf3ff, 1, DrvPalRAM); // tnzsb
	ZetMapArea(0xf800, 0xfbff, 1, DrvPalRAM); // kabukiz
	ZetSetWriteHandler(tnzsb_cpu1_write);
	ZetSetReadHandler(tnzsb_cpu1_read);
	ZetClose();

	ZetInit(2);
	ZetOpen(2);
	ZetMapArea(0x0000, 0x7fff, 0, DrvZ80ROM2 + 0x0000);
	ZetMapArea(0x0000, 0x7fff, 2, DrvZ80ROM2 + 0x0000);
	ZetMapArea(0xc000, 0xdfff, 0, DrvZ80RAM2); // tnzsb
	ZetMapArea(0xc000, 0xdfff, 1, DrvZ80RAM2);
	ZetMapArea(0xc000, 0xdfff, 2, DrvZ80RAM2);
	ZetMapArea(0xe000, 0xffff, 0, DrvZ80RAM2); // kabukiz
	ZetMapArea(0xe000, 0xffff, 1, DrvZ80RAM2);
	ZetMapArea(0xe000, 0xffff, 2, DrvZ80RAM2);
	ZetSetOutHandler(tnzs_cpu2_out);
	ZetSetInHandler(tnzs_cpu2_in);
	ZetClose();

	BurnYM2203Init(1, 3000000, &DrvYM2203IRQHandler, 0);
	BurnYM2203SetPorts(0, NULL, NULL, &kabukiz_sound_bankswitch, &kabukiz_dac_write);
	BurnTimerAttachZet(6000000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE, 2.00, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 1.00, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 1.00, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 1.00, BURN_SND_ROUTE_BOTH);

	DACInit(0, 0, 1, ZetTotalCycles, 6000000); // kabukiz
	DACSetRoute(0, 0.50, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	BurnTrackballInit(2); // not on this hw

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	if (tnzs_mcu_type() != MCU_NONE_JPOPNICS) BurnYM2203Exit();
	if (tnzs_mcu_type() == MCU_NONE_JPOPNICS) BurnYM2151Exit();
	DACExit();

	BurnFreeMemIndex();

	BurnTrackballExit();

	if (tnzs_mcu_type() == MCU_NONE_KAGEKI) {
		kageki_sample_exit();
	}

	tnzs_mcu_init(0);

	game_kabukiz = 0;

	return 0;
}

static void kageki_sample_render(INT16 *pSoundBuf, INT32 nLength)
{
	if (kageki_sample_select == -1) return;

	INT32 offset = kageki_sample_pos;
	INT32 step = (7000 << 16) / nBurnSoundRate;

	INT32 size = kageki_sample_size[kageki_sample_select];
	INT16 *ptr = kageki_sample_data[kageki_sample_select];

	for (INT32 i = 0; i < nLength; i++) {
		if ((offset >> 16) >= size) {
			kageki_sample_select = -1;
			break;
		}
		INT16 Sample = ptr[(offset >> 16)];

		offset += step;

		INT16 nLeftSample = 0, nRightSample = 0;

		if ((kageki_sample_output_dir & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) {
			nLeftSample += (INT32)(Sample * kageki_sample_gain);
		}
		if ((kageki_sample_output_dir & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) {
			nRightSample += (INT32)(Sample * kageki_sample_gain);
		}

		pSoundBuf[i * 2 + 0] += nLeftSample;
		pSoundBuf[i * 2 + 1] += nRightSample;
	}

	kageki_sample_pos = offset;
	if ((offset >> 16) >= size) kageki_sample_select = -1;
}

static void draw_16x16(INT32 sx, INT32 sy, INT32 code, INT32 color, INT32 flipx, INT32 flipy, INT32 transp)
{
	if (transp) {
		Draw16x16MaskTile(pTransDraw, code, sx, sy, flipx, flipy, color, 4, 0, 0, DrvGfxROM);
	} else {
		Draw16x16Tile(pTransDraw, code, sx, sy, flipx, flipy, color, 4, 0, DrvGfxROM);
	}
}

static void draw_background(INT32 ctrl, INT32 flipscreen)
{
	INT32 scrollx, scrolly;
	UINT8 *m = DrvSprRAM + 0x400;

	if ((ctrl ^ (ctrl << 1)) & 0x40)
	{
		m += 0x800;
	}

	INT32 transpen = (*tnzs_bg_flag & 0x80) ^ 0x80;

	INT32 tot = DrvObjCtrl[1] & 0x1f;
	if (tot == 1) tot = 16;

	UINT32 upperbits = DrvObjCtrl[2] | (DrvObjCtrl[3] << 8);

	for (INT32 column = 0; column < tot; column++)
	{
		scrollx = DrvScrollRAM[(column << 4) | 4] - ((upperbits & 0x01) << 8);
		if (flipscreen)
			scrolly = DrvScrollRAM[column << 4] + 1 - 256;
		else
			scrolly = -DrvScrollRAM[column << 4] + 1;

		for (INT32 y = 0; y < 16; y++)
		{
			for (INT32 x = 0; x < 2; x++)
			{
				INT32 i = ((column ^ 8) << 5) | (y << 1) | x;

				INT32 code  = m[i + 0x0000] | ((m[i + 0x1000] & 0x3f) << 8);
				INT32 color = m[i + 0x1200] >> 3;
				INT32 flipx = m[i + 0x1000] & 0x80;
				INT32 flipy = m[i + 0x1000] & 0x40;
				INT32 sx = x << 4;
				INT32 sy = y << 4;
				if (flipscreen)
				{
					sy = 240 - sy;
					flipx = !flipx;
					flipy = !flipy;
				}

				sy = (sy + scrolly) & 0xff;
				sx += scrollx;

				if (sx >= nScreenWidth || sx < -15 || sy >= nScreenHeight + 16 || sy < 1) continue;

				draw_16x16(sx, sy - 16, code, color, flipx, flipy, transpen);
			}
		}

		upperbits >>= 1;
	}
}

static void draw_foreground(INT32 ctrl, INT32 flipscreen)
{
	UINT8 *char_pointer  = DrvSprRAM + 0x0000;
	UINT8 *x_pointer     = DrvSprRAM + 0x0200;
	UINT8 *y_pointer     = DrvVidRAM + 0x0000;
	UINT8 *ctrl_pointer  = DrvSprRAM + 0x1000;
	UINT8 *color_pointer = DrvSprRAM + 0x1200;

	if ((ctrl ^ (ctrl << 1)) & 0x40)
	{
		char_pointer  += 0x800;
		x_pointer     += 0x800;
		ctrl_pointer  += 0x800;
		color_pointer += 0x800;
	}

	for (INT32 i = 0x1ff; i >= 0; i--)
	{
		INT32 code  = char_pointer[i] + ((ctrl_pointer[i] & 0x3f) << 8);
		INT32 color = (color_pointer[i] & 0xf8) >> 3;
		INT32 sx    = x_pointer[i] - ((color_pointer[i] & 1) << 8);
		INT32 sy    = 240 - y_pointer[i];
		INT32 flipx = ctrl_pointer[i] & 0x80;
		INT32 flipy = ctrl_pointer[i] & 0x40;
		if (flipscreen)
		{
			sy = 240 - sy;
			flipx = !flipx;
			flipy = !flipy;
			if ((sy == 0) && (code == 0)) sy += 240;
		}

		if (sx >= nScreenWidth || sx < -15) continue;

		draw_16x16(sx, sy - 14, code, color, flipx, flipy, 1);
	}
}

static inline void DrvRecalcPalette()
{
	UINT8 r,g,b;
	if (tnzs_mcu_type() == MCU_NONE_JPOPNICS) {
		for (INT32 i = 0; i < 0x400; i+=2) {
			UINT16 pal = (DrvPalRAM[i] << 8) | DrvPalRAM[i | 1];

			r = (pal >>  4) & 0x0f;
			g = (pal >> 12) & 0x0f;
			b = (pal >>  8) & 0x0f;

			r |= r << 4;
			g |= g << 4;
			b |= b << 4;

			DrvPalette[i / 2] = BurnHighCol(r, g, b, 0);
		}
	} else {
		for (INT32 i = 0; i < 0x400; i+=2) {
			UINT16 pal = (DrvPalRAM[i | 1] << 8) | DrvPalRAM[i];

			r = (pal >> 10) & 0x1f;
			g = (pal >>  5) & 0x1f;
			b = (pal >>  0) & 0x1f;

			r = (r << 3) | (r >> 2);
			g = (g << 3) | (g >> 2);
			b = (b << 3) | (b >> 2);

			DrvPalette[i / 2] = BurnHighCol(r, g, b, 0);
		}
	}
}

static void sprite_buffer(INT32 ctrl)
{
	if (~ctrl & 0x20)
	{
		if (ctrl & 0x40) {
			memcpy (DrvSprRAM + 0x0000, DrvSprRAM + 0x0800, 0x0400);
			memcpy (DrvSprRAM + 0x1000, DrvSprRAM + 0x1800, 0x0400);
		} else {
			memcpy (DrvSprRAM + 0x0800, DrvSprRAM + 0x0000, 0x0400);
			memcpy (DrvSprRAM + 0x1800, DrvSprRAM + 0x1000, 0x0400);
		}

		memcpy (DrvSprRAM + 0x0400, DrvSprRAM + 0x0c00, 0x0400);
		memcpy (DrvSprRAM + 0x1400, DrvSprRAM + 0x1c00, 0x0400);
	}
}

static INT32 DrvDraw()
{
	DrvRecalcPalette();

	INT32 flip =  DrvObjCtrl[0] & 0x40;
	INT32 ctrl = (DrvObjCtrl[1] & 0x60) ^ 0x20;

	BurnTransferClear(0x1f0);

	draw_background(ctrl, flip);
	draw_foreground(ctrl, flip);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void assemble_inputs()
{
	tnzs_mcu_inputs = DrvInputs;
	memset (DrvInputs, 0xff, 3);
	for (INT32 i = 0; i < 8; i++) {
		DrvInputs[0] ^= DrvJoy1[i] << i;
		DrvInputs[1] ^= DrvJoy2[i] << i;
		DrvInputs[2] ^= DrvJoy3[i] << i;
	}

	BurnTrackballConfig(0, AXIS_NORMAL, AXIS_NORMAL);
	BurnTrackballFrame(0, DrvAxis[0], DrvAxis[1], 0x6, 0x3f);
	BurnTrackballUpdate(0);
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();

	assemble_inputs();

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[3] = { 6000000 / 60, 6000000 / 60, 6000000 / 60 };
	INT32 nCyclesDone[3] = { nExtraCycles[0], nExtraCycles[1], 0 };

	for (INT32 i = 0; i < nInterleave; i++) {
		// Run Z80 #0
		ZetOpen(0);
		CPU_RUN(0, Zet);
		if (i == 240) {
			tnzs_mcu_interrupt();
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
		ZetClose();

		// Run Z80 #1
		ZetOpen(1);
		if (!cpu1_reset) {
			if (system_type == 1) {
				CPU_RUN_TIMER(1);
			} else {
				CPU_RUN(1, Zet);
			}
		}
		if (i == 240)
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		// Run Z80 #2
		if (tnzs_mcu_type() == MCU_NONE)
		{
			ZetOpen(2);
			CPU_RUN_TIMER(2);
			ZetClose();
		}

		if (i == 240) {
			if (pBurnDraw) {
				DrvDraw();
			}

			sprite_buffer(DrvObjCtrl[1]);
		}
	}

	ZetOpen(2);
	if (pBurnSoundOut) {
		if (tnzs_mcu_type() == MCU_NONE_JPOPNICS) {
			BurnYM2151Render(pBurnSoundOut, nBurnSoundLen);
		}

		if (tnzs_mcu_type() != MCU_NONE_JPOPNICS) {
			BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
			DACUpdate(pBurnSoundOut, nBurnSoundLen);
			if (tnzs_mcu_type() == MCU_NONE_KAGEKI) {
				kageki_sample_render(pBurnSoundOut, nBurnSoundLen);
			}
		}
	}

	ZetClose();

	nExtraCycles[0] = nCyclesDone[0] - nCyclesTotal[0];
	nExtraCycles[1] = nCyclesDone[1] - nCyclesTotal[1];

	return 0;
}

static INT32 DrvScan(INT32 nAction,INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029730;
	}

	if (nAction & ACB_VOLATILE) {
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);

		if (tnzs_mcu_type() == MCU_NONE_JPOPNICS) {
			BurnYM2151Scan(nAction, pnMin);
		} else {
			BurnYM2203Scan(nAction, pnMin);
		}

		DACScan(nAction, pnMin);

		BurnTrackballScan();

		tnzs_mcu_scan();

		SCAN_VAR(tnzs_banks);
		SCAN_VAR(cpu1_reset);

		SCAN_VAR(kageki_csport_sel);
		SCAN_VAR(kageki_sample_pos);
		SCAN_VAR(kageki_sample_select);

		SCAN_VAR(nExtraCycles);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		bankswitch0(tnzs_banks[0]);
		ZetClose();

		ZetOpen(1);
		bankswitch1(tnzs_banks[1]);
		ZetClose();

		if (game_kabukiz) {
			ZetOpen(2);
			kabukiz_sound_bankswitch(0, tnzs_banks[2]);
			ZetClose();
		}
	}

	return 0;
}


// Plump Pop (Japan)

static struct BurnRomInfo plumppopRomDesc[] = {
	{ "a98__09.11c",			0x10000, 0x107f9e06, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "a98__10.9c",				0x10000, 0xdf6e6af2, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "a98__11.4e",				0x10000, 0xbc56775c, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "b06__14.1g",				0x00800, 0x28907072, 3 },	      			  //  3 I8742 MCU

	{ "a98__01.mbm27c512.13a",	0x10000, 0xf3033dca, 4 | BRF_GRA },	      	  //  4 Graphics
	{ "a98__02.mbm27c512.12a",	0x10000, 0xf2d17b0c, 4 | BRF_GRA },	      	  //  5
	{ "a98__03.mbm27c512.10a",	0x10000, 0x1a519b0a, 4 | BRF_GRA },	      	  //  6
	{ "a98__04.mbm27c512.8a",	0x10000, 0xb64501a1, 4 | BRF_GRA },	      	  //  7
	{ "a98__05.mbm27c512.7a",	0x10000, 0x45c36963, 4 | BRF_GRA },	      	  //  8
	{ "a98__06.mbm27c512.5a",	0x10000, 0xe075341b, 4 | BRF_GRA },	      	  //  9
	{ "a98__07.mbm27c512.4a",	0x10000, 0x8e16cd81, 4 | BRF_GRA },	      	  // 10
	{ "a98__08.mbm27c512.2a",	0x10000, 0xbfa7609a, 4 | BRF_GRA },	      	  // 11

	{ "a98-13.15f",				0x00200, 0x7cde2da5, 5 | BRF_GRA },	      	  // 12 Color PROMs
	{ "a98-12.17f",				0x00200, 0x90dc9da7, 5 | BRF_GRA },	      	  // 13

	/* pals on plumppop are the same set as arkanoid2/extrmatn/drtoppel/chukataio/etc with the exception of d9? */
	{ "b06-10-1.pal16l8a.d9.jed", 	0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 14 Pal
	{ "b06-11.pal16l8a.d6.jed", 	0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 15
	{ "b06-12.pal16l8a.c3.jed", 	0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 16
	{ "b06-13.pal16l8a.c2.jed", 	0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 17
};

STD_ROM_PICK(plumppop)
STD_ROM_FN(plumppop)

static INT32 PlumppopInit()
{
	return Type1Init(MCU_PLUMPOP);
}

struct BurnDriver BurnDrvPlumppop = {
	"plumppop", NULL, NULL, NULL, "1987",
	"Plump Pop (Japan)\0", NULL, "Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_BREAKOUT, 0,
	NULL, plumppopRomInfo, plumppopRomName, NULL, NULL, NULL, NULL, PlumppopInputInfo, PlumppopDIPInfo,
	PlumppopInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// Extermination (World)

static struct BurnRomInfo extrmatnRomDesc[] = {
	{ "b06-05.11c",		0x10000, 0x918e1fe3, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "b06-06.9c",		0x10000, 0x8842e105, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "b06-19.4e",		0x10000, 0x8de43ed9, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "b06__14.1g",		0x00800, 0x28907072, 3 },        //  3 I8742 MCU

	{ "b06-01.13a",		0x20000, 0xd2afbf7e, 4 | BRF_GRA },	      	  //  4 Graphics
	{ "b06-02.10a",		0x20000, 0xe0c2757a, 4 | BRF_GRA },	      	  //  5
	{ "b06-03.7a",		0x20000, 0xee80ab9d, 4 | BRF_GRA },	      	  //  6
	{ "b06-04.4a",		0x20000, 0x3697ace4, 4 | BRF_GRA },	      	  //  7

	{ "b06-09.15f",		0x00200, 0xf388b361, 5 | BRF_GRA },	      	  //  8 Color PROMs
	{ "b06-08.17f",		0x00200, 0x10c9aac3, 5 | BRF_GRA },	      	  //  9

	/* these are shared with several other games on this hardware */
	{ "b06-10.pal16l8a.d9.jed", 0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 10 Pal
	{ "b06-11.pal16l8a.d6.jed", 0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 11
	{ "b06-12.pal16l8a.c3.jed", 0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 12
	{ "b06-13.pal16l8a.c2.jed", 0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 13
};

STD_ROM_PICK(extrmatn)
STD_ROM_FN(extrmatn)

static INT32 ExtrmatnInit()
{
	return Type1Init(MCU_EXTRMATN);
}

struct BurnDriver BurnDrvExtrmatn = {
	"extrmatn", NULL, NULL, NULL, "1987",
	"Extermination (World)\0", NULL, "Taito Corporation Japan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_RUNGUN, 0,
	NULL, extrmatnRomInfo, extrmatnRomName, NULL, NULL, NULL, NULL, CommonInputInfo, ExtrmatnDIPInfo,
	ExtrmatnInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Extermination (US, set 1)

static struct BurnRomInfo extrmatuRomDesc[] = {
	{ "b06-20.11c",		0x10000, 0x04e3fc1f, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "b06-21.9c",		0x10000, 0x1614d6a2, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "b06-22.4e",		0x10000, 0x744f2c84, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "b06__14.1g",		0x00800, 0x28907072, 3 },        //  3 I8742 MCU

	{ "b06-01.13a",		0x20000, 0xd2afbf7e, 4 | BRF_GRA },	      	  //  4 Graphics
	{ "b06-02.10a",		0x20000, 0xe0c2757a, 4 | BRF_GRA },	      	  //  5
	{ "b06-03.7a",		0x20000, 0xee80ab9d, 4 | BRF_GRA },	      	  //  6
	{ "b06-04.4a",		0x20000, 0x3697ace4, 4 | BRF_GRA },	      	  //  7

	{ "b06-09.15f",		0x00200, 0xf388b361, 5 | BRF_GRA },	      	  //  8 Color PROMs
	{ "b06-08.17f",		0x00200, 0x10c9aac3, 5 | BRF_GRA },	      	  //  9

	/* these are shared with several other games on this hardware */
	{ "b06-10.pal16l8a.d9.jed", 0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 10 Pal
	{ "b06-11.pal16l8a.d6.jed", 0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 11
	{ "b06-12.pal16l8a.c3.jed", 0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 12
	{ "b06-13.pal16l8a.c2.jed", 0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 13
};

STD_ROM_PICK(extrmatu)
STD_ROM_FN(extrmatu)

struct BurnDriver BurnDrvExtrmatu = {
	"extrmatnu", "extrmatn", NULL, NULL, "1987",
	"Extermination (US, set 1)\0", NULL, "Taito (World Games license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_RUNGUN, 0,
	NULL, extrmatuRomInfo, extrmatuRomName, NULL, NULL, NULL, NULL, CommonInputInfo, ExtrmatnDIPInfo,
	ExtrmatnInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Extermination (US, Romstar)

static struct BurnRomInfo extrmaturRomDesc[] = {
	{ "b06-15.11c",		0x10000, 0x4b3ee597, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "b06-16.9c",		0x10000, 0x86175ea4, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "b06-17.4e",		0x10000, 0x744f2c84, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "b06__14.1g",		0x00800, 0x28907072, 3 },        //  3 I8742 MCU

	{ "b06-01.13a",		0x20000, 0xd2afbf7e, 4 | BRF_GRA },	      	  //  4 Graphics
	{ "b06-02.10a",		0x20000, 0xe0c2757a, 4 | BRF_GRA },	      	  //  5
	{ "b06-03.7a",		0x20000, 0xee80ab9d, 4 | BRF_GRA },	      	  //  6
	{ "b06-04.4a",		0x20000, 0x3697ace4, 4 | BRF_GRA },	      	  //  7

	{ "b06-09.15f",		0x00200, 0xf388b361, 5 | BRF_GRA },	      	  //  8 Color PROMs
	{ "b06-08.17f",		0x00200, 0x10c9aac3, 5 | BRF_GRA },	      	  //  9

	/* these are shared with several other games on this hardware */
	{ "b06-10.pal16l8a.d9.jed", 0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 10 Pal
	{ "b06-11.pal16l8a.d6.jed", 0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 11
	{ "b06-12.pal16l8a.c3.jed", 0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 12
	{ "b06-13.pal16l8a.c2.jed", 0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 13
};

STD_ROM_PICK(extrmatur)
STD_ROM_FN(extrmatur)

struct BurnDriver BurnDrvExtrmatur = {
	"extrmatnur", "extrmatn", NULL, NULL, "1987",
	"Extermination (US, Romstar)\0", NULL, "Taito America Corporation (Romstar license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_RUNGUN, 0,
	NULL, extrmaturRomInfo, extrmaturRomName, NULL, NULL, NULL, NULL, CommonInputInfo, ExtrmatnDIPInfo,
	ExtrmatnInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Extermination (Japan)

static struct BurnRomInfo extrmatjRomDesc[] = {
	{ "b06-05.11c",		0x10000, 0x918e1fe3, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "b06-06.9c",		0x10000, 0x8842e105, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "b06-07.4e",		0x10000, 0xb37fb8b3, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "b06__14.1g",		0x00800, 0x28907072, 3 },        //  3 I8742 MCU

	{ "b06-01.13a",		0x20000, 0xd2afbf7e, 4 | BRF_GRA },	      	  //  4 Graphics
	{ "b06-02.10a",		0x20000, 0xe0c2757a, 4 | BRF_GRA },	      	  //  5
	{ "b06-03.7a",		0x20000, 0xee80ab9d, 4 | BRF_GRA },	      	  //  6
	{ "b06-04.4a",		0x20000, 0x3697ace4, 4 | BRF_GRA },	      	  //  7

	{ "b06-09.15f",		0x00200, 0xf388b361, 5 | BRF_GRA },	      	  //  8 Color PROMs
	{ "b06-08.17f",		0x00200, 0x10c9aac3, 5 | BRF_GRA },	      	  //  9

	/* these are shared with several other games on this hardware */
	{ "b06-10.pal16l8a.d9.jed", 0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 10 Pal
	{ "b06-11.pal16l8a.d6.jed", 0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 11
	{ "b06-12.pal16l8a.c3.jed", 0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 12
	{ "b06-13.pal16l8a.c2.jed", 0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 13
};

STD_ROM_PICK(extrmatj)
STD_ROM_FN(extrmatj)

struct BurnDriver BurnDrvExtrmatj = {
	"extrmatnj", "extrmatn", NULL, NULL, "1987",
	"Extermination (Japan)\0", NULL, "Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_RUNGUN, 0,
	NULL, extrmatjRomInfo, extrmatjRomName, NULL, NULL, NULL, NULL, CommonInputInfo, ExtrmatnDIPInfo,
	ExtrmatnInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Arkanoid - Revenge of DOH (World)

static struct BurnRomInfo arknoid2RomDesc[] = {
	{ "b08__05.11c",	0x10000, 0x136edf9d, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	{ "b08__13.3e",		0x10000, 0xe8035ef1, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code

	{ "b08__09.3g",		0x00800, 0x00000000, 3 | BRF_NODUMP },        //  2 I8742 MCU

	{ "b08-01.13a",		0x20000, 0x2ccc86b4, 4 | BRF_GRA },	      	  //  3 Graphics
	{ "b08-02.10a",		0x20000, 0x056a985f, 4 | BRF_GRA },	      	  //  4
	{ "b08-03.7a",		0x20000, 0x274a795f, 4 | BRF_GRA },	      	  //  5
	{ "b08-04.4a",		0x20000, 0x9754f703, 4 | BRF_GRA },	      	  //  6

	{ "b08-08.15f",		0x00200, 0xa4f7ebd9, 5 | BRF_GRA },	      	  //  7 Color PROMs
	{ "b08-07.16f",		0x00200, 0xea34d9f7, 5 | BRF_GRA },	      	  //  8

	/* these are shared with extermination */
	{ "b06-10.pal16l8a.d9.jed", 0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, //  9 Pal
	{ "b06-11.pal16l8a.d6.jed", 0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 10
	{ "b06-12.pal16l8a.c3.jed", 0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 11
	{ "b06-13.pal16l8a.c2.jed", 0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 12
};

STD_ROM_PICK(arknoid2)
STD_ROM_FN(arknoid2)

static INT32 Arknoid2Init()
{
	return Type1Init(MCU_ARKANOID);
}

struct BurnDriver BurnDrvArknoid2 = {
	"arknoid2", NULL, NULL, NULL, "1987",
	"Arkanoid - Revenge of DOH (World)\0", NULL, "Taito Corporation Japan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_BREAKOUT, 0,
	NULL, arknoid2RomInfo, arknoid2RomName, NULL, NULL, NULL, NULL, Arknoid2InputInfo, Arknoid2DIPInfo,
	Arknoid2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Arkanoid - Revenge of DOH (US)

static struct BurnRomInfo arknid2uRomDesc[] = {
	{ "b08__11.11c",	0x10000, 0x99555231, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	{ "b08__12.3e",		0x10000, 0xdc84e27d, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code

	{ "b08__09.3g",		0x00800, 0x00000000, 3 | BRF_NODUMP },	      //  2 I8742 MCU

	{ "b08-01.13a",		0x20000, 0x2ccc86b4, 4 | BRF_GRA },	      	  //  3 Graphics
	{ "b08-02.10a",		0x20000, 0x056a985f, 4 | BRF_GRA },	      	  //  4
	{ "b08-03.7a",		0x20000, 0x274a795f, 4 | BRF_GRA },	      	  //  5
	{ "b08-04.4a",		0x20000, 0x9754f703, 4 | BRF_GRA },	      	  //  6

	{ "b08-08.15f",		0x00200, 0xa4f7ebd9, 5 | BRF_GRA },	      	  //  7 Color PROMs
	{ "b08-07.16f",		0x00200, 0xea34d9f7, 5 | BRF_GRA },	      	  //  8

	/* these are shared with extermination */
	{ "b06-10.pal16l8a.d9.jed", 0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, //  9 Pal
	{ "b06-11.pal16l8a.d6.jed", 0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 10
	{ "b06-12.pal16l8a.c3.jed", 0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 11
	{ "b06-13.pal16l8a.c2.jed", 0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 12
};

STD_ROM_PICK(arknid2u)
STD_ROM_FN(arknid2u)

struct BurnDriver BurnDrvArknid2u = {
	"arknoid2u", "arknoid2", NULL, NULL, "1987",
	"Arkanoid - Revenge of DOH (US)\0", NULL, "Taito America Corporation (Romstar license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_BREAKOUT, 0,
	NULL, arknid2uRomInfo, arknid2uRomName, NULL, NULL, NULL, NULL, Arknoid2InputInfo, Arknid2uDIPInfo,
	Arknoid2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Arkanoid - Revenge of DOH (Japan)

static struct BurnRomInfo arknid2jRomDesc[] = {
	{ "b08_05.11c",		0x10000, 0x136edf9d, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	{ "b08_06.3e",		0x10000, 0xadfcd40c, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code

	{ "b08__09.3g",		0x00800, 0x00000000, 3 | BRF_NODUMP }, 	      //  2 I8742 MCU

	{ "b08-01.13a",		0x20000, 0x2ccc86b4, 4 | BRF_GRA },	      	  //  3 Graphics
	{ "b08-02.10a",		0x20000, 0x056a985f, 4 | BRF_GRA },	      	  //  4
	{ "b08-03.7a",		0x20000, 0x274a795f, 4 | BRF_GRA },	      	  //  5
	{ "b08-04.4a",		0x20000, 0x9754f703, 4 | BRF_GRA },	      	  //  6

	{ "b08-08.15f",		0x00200, 0xa4f7ebd9, 5 | BRF_GRA },	      	  //  7 Color PROMs
	{ "b08-07.16f",		0x00200, 0xea34d9f7, 5 | BRF_GRA },	      	  //  8

	/* these are shared with extermination */
	{ "b06-10.pal16l8a.d9.jed", 0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, //  9 Pal
	{ "b06-11.pal16l8a.d6.jed", 0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 10
	{ "b06-12.pal16l8a.c3.jed", 0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 11
	{ "b06-13.pal16l8a.c2.jed", 0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 12
};

STD_ROM_PICK(arknid2j)
STD_ROM_FN(arknid2j)

struct BurnDriver BurnDrvArknid2j = {
	"arknoid2j", "arknoid2", NULL, NULL, "1987",
	"Arkanoid - Revenge of DOH (Japan)\0", NULL, "Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_BREAKOUT, 0,
	NULL, arknid2jRomInfo, arknid2jRomName, NULL, NULL, NULL, NULL, Arknoid2InputInfo, Arknid2uDIPInfo,
	Arknoid2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Arkanoid - Revenge of DOH (Japan bootleg)

static struct BurnRomInfo arknid2bRomDesc[] = {
	{ "boot.11c",		0x10000, 0x3847dfb0, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	{ "b08_13.3e",		0x10000, 0xe8035ef1, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code

	/* Labeled B08 // 09 and under printed label "?Taito M-009?", is a mask 8042... does the bootleg set even HAVE the mcu? */
	{ "ark28742.3g",	0x00800, 0x00000000, 3 | BRF_NODUMP },	      //  2 I8742 MCU

	{ "b08-01.13a",		0x20000, 0x2ccc86b4, 4 | BRF_GRA },	      	  //  3 Graphics
	{ "b08-02.10a",		0x20000, 0x056a985f, 4 | BRF_GRA },	      	  //  4
	{ "b08-03.7a",		0x20000, 0x274a795f, 4 | BRF_GRA },	      	  //  5
	{ "b08-04.4a",		0x20000, 0x9754f703, 4 | BRF_GRA },	      	  //  6

	{ "b08-08.15f",		0x00200, 0xa4f7ebd9, 5 | BRF_GRA },	      	  //  7 Color PROMs
	{ "b08-07.16f",		0x00200, 0xea34d9f7, 5 | BRF_GRA },	      	  //  8

	/* these are shared with extermination */
	{ "b06-10.pal16l8a.d9.jed", 0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, //  9 Pal
	{ "b06-11.pal16l8a.d6.jed", 0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 10
	{ "b06-12.pal16l8a.c3.jed", 0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 11
	{ "b06-13.pal16l8a.c2.jed", 0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 12
};

STD_ROM_PICK(arknid2b)
STD_ROM_FN(arknid2b)

struct BurnDriver BurnDrvArknid2b = {
	"arknoid2b", "arknoid2", NULL, NULL, "1987",
	"Arkanoid - Revenge of DOH (Japan bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_BREAKOUT, 0,
	NULL, arknid2bRomInfo, arknid2bRomName, NULL, NULL, NULL, NULL, Arknoid2InputInfo, Arknid2uDIPInfo,
	Arknoid2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Dr. Toppel's Adventure (World)

static struct BurnRomInfo drtoppelRomDesc[] = {
	{ "b19__09.11c",			0x10000, 0x3e654f82, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "b19__10.9c",				0x10000, 0x7e72fd25, 1 | BRF_PRG | BRF_ESS }, //  1

	/* Region-Hacked??, need correct Taito rom number */
	{ "b19__15.3e",				0x10000, 0x37a0d3fb, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "b06__14.1g",				0x00800, 0x28907072, 3 | BRF_PRG | BRF_OPT }, //  3 I8742 MCU

	{ "b19-01.23c1000.13a",		0x20000, 0xa7e8a0c1, 4 | BRF_GRA },	      	  //  4 Graphics
	{ "b19-02.23c1000.12a",		0x20000, 0x790ae654, 4 | BRF_GRA },	      	  //  5
	{ "b19-03.23c1000.10a",		0x20000, 0x495c4c5a, 4 | BRF_GRA },	      	  //  6
	{ "b19-04.23c1000.8a",		0x20000, 0x647007a0, 4 | BRF_GRA },	      	  //  7
	{ "b19-05.23c1000.7a",		0x20000, 0x49f2b1a5, 4 | BRF_GRA },	      	  //  8
	{ "b19-06.23c1000.5a",		0x20000, 0x2d39f1d0, 4 | BRF_GRA },	      	  //  9
	{ "b19-07.23c1000.4a",		0x20000, 0x8bb06f41, 4 | BRF_GRA },	      	  // 10
	{ "b19-08.23c1000.2a",		0x20000, 0x3584b491, 4 | BRF_GRA },	      	  // 11

	{ "b19-13.am27s29.15f",		0x00200, 0x6a547980, 5 | BRF_GRA },	      	  // 12 Color PROMs
	{ "b19-12.am27s29.16f",		0x00200, 0x5754e9d8, 5 | BRF_GRA },	      	  // 13

	/* these are shared with extermination */
	{ "b06-10.pal16l8a.d9.jed", 0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 14 Pal
	{ "b06-11.pal16l8a.d6.jed", 0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 15
	{ "b06-12.pal16l8a.c3.jed", 0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 16
	{ "b06-13.pal16l8a.c2.jed", 0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 17
};

STD_ROM_PICK(drtoppel)
STD_ROM_FN(drtoppel)

static INT32 DrtoppelInit()
{
	return Type1Init(MCU_DRTOPPEL);
}

struct BurnDriver BurnDrvDrtoppel = {
	"drtoppel", NULL, NULL, NULL, "1987",
	"Dr. Toppel's Adventure (World)\0", NULL, "Taito Corporation Japan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, drtoppelRomInfo, drtoppelRomName, NULL, NULL, NULL, NULL, CommonInputInfo, DrtoppelDIPInfo,
	DrtoppelInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Dr. Toppel's Adventure (World, alt?)
// one byte different (db8f : 39 instead of 37) - possible of bad and/or hacked rom

static struct BurnRomInfo drtoppelaRomDesc[] = {
	{ "b19_09.bin",		0x10000, 0x6364a970, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "b19-10.9c",		0x10000, 0x7e72fd25, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "b19-15.3e",		0x10000, 0x37a0d3fb, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "drt8742.3g",		0x00800, 0x00000000, 3 | BRF_NODUMP },        //  3 I8742 MCU

	{ "b19-01.23c1000.13a",		0x20000, 0xa7e8a0c1, 4 | BRF_GRA },	      	  //  4 Graphics
	{ "b19-02.23c1000.12a",		0x20000, 0x790ae654, 4 | BRF_GRA },	      	  //  5
	{ "b19-03.23c1000.10a",		0x20000, 0x495c4c5a, 4 | BRF_GRA },	      	  //  6
	{ "b19-04.23c1000.8a",		0x20000, 0x647007a0, 4 | BRF_GRA },	      	  //  7
	{ "b19-05.23c1000.7a",		0x20000, 0x49f2b1a5, 4 | BRF_GRA },	      	  //  8
	{ "b19-06.23c1000.5a",		0x20000, 0x2d39f1d0, 4 | BRF_GRA },	      	  //  9
	{ "b19-07.23c1000.4a",		0x20000, 0x8bb06f41, 4 | BRF_GRA },	      	  // 10
	{ "b19-08.23c1000.2a",		0x20000, 0x3584b491, 4 | BRF_GRA },	      	  // 11

	{ "b19-13.15f",		0x00200, 0x6a547980, 5 | BRF_GRA },	      	  // 12 Color PROMs
	{ "b19-12.16f",		0x00200, 0x5754e9d8, 5 | BRF_GRA },	      	  // 13
};

STD_ROM_PICK(drtoppela)
STD_ROM_FN(drtoppela)

struct BurnDriver BurnDrvDrtoppela = {
	"drtoppela", "drtoppel", NULL, NULL, "1987",
	"Dr. Toppel's Adventure (World, alt?)\0", NULL, "Taito Corporation Japan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, drtoppelaRomInfo, drtoppelaRomName, NULL, NULL, NULL, NULL, CommonInputInfo, DrtoppelDIPInfo,
	DrtoppelInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Dr. Toppel's Adventure (US)

static struct BurnRomInfo drtoppluRomDesc[] = {
	{ "b19__09.11c",			0x10000, 0x3e654f82, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "b19__10.9c",				0x10000, 0x7e72fd25, 1 | BRF_PRG | BRF_ESS }, //  1

	/* Region-Hacked??, need correct Taito rom number */
	{ "b19__14.3e",				0x10000, 0x05565b22, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "b06__14.1g",				0x00800, 0x28907072, 3 | BRF_PRG | BRF_OPT }, //  3 I8742 MCU

	{ "b19-01.23c1000.13a",		0x20000, 0xa7e8a0c1, 4 | BRF_GRA },	      	  //  4 Graphics
	{ "b19-02.23c1000.12a",		0x20000, 0x790ae654, 4 | BRF_GRA },	      	  //  5
	{ "b19-03.23c1000.10a",		0x20000, 0x495c4c5a, 4 | BRF_GRA },	      	  //  6
	{ "b19-04.23c1000.8a",		0x20000, 0x647007a0, 4 | BRF_GRA },	      	  //  7
	{ "b19-05.23c1000.7a",		0x20000, 0x49f2b1a5, 4 | BRF_GRA },	      	  //  8
	{ "b19-06.23c1000.5a",		0x20000, 0x2d39f1d0, 4 | BRF_GRA },	      	  //  9
	{ "b19-07.23c1000.4a",		0x20000, 0x8bb06f41, 4 | BRF_GRA },	      	  // 10
	{ "b19-08.23c1000.2a",		0x20000, 0x3584b491, 4 | BRF_GRA },	      	  // 11

	{ "b19-13.am27s29.15f",		0x00200, 0x6a547980, 5 | BRF_GRA },	      	  // 12 Color PROMs
	{ "b19-12.am27s29.16f",		0x00200, 0x5754e9d8, 5 | BRF_GRA },	      	  // 13

	/* these are shared with extermination */
	{ "b06-10.pal16l8a.d9.jed", 0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 14 Pal
	{ "b06-11.pal16l8a.d6.jed", 0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 15
	{ "b06-12.pal16l8a.c3.jed", 0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 16
	{ "b06-13.pal16l8a.c2.jed", 0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 17
};

STD_ROM_PICK(drtopplu)
STD_ROM_FN(drtopplu)

struct BurnDriver BurnDrvDrtopplu = {
	"drtoppelu", "drtoppel", NULL, NULL, "1987",
	"Dr. Toppel's Adventure (US)\0", NULL, "Taito America Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, drtoppluRomInfo, drtoppluRomName, NULL, NULL, NULL, NULL, CommonInputInfo, DrtoppluDIPInfo,
	DrtoppelInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Dr. Toppel's Tankentai (Japan)

static struct BurnRomInfo drtoppljRomDesc[] = {
	{ "b19__09.11c",			0x10000, 0x3e654f82, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "b19__10.9c",				0x10000, 0x7e72fd25, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "b19__11.3e",				0x10000, 0x524dc249, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "b06__14.1g",				0x00800, 0x28907072, 3 | BRF_PRG | BRF_OPT }, //  3 I8742 MCU

	{ "b19-01.23c1000.13a",		0x20000, 0xa7e8a0c1, 4 | BRF_GRA },	      	  //  4 Graphics
	{ "b19-02.23c1000.12a",		0x20000, 0x790ae654, 4 | BRF_GRA },	      	  //  5
	{ "b19-03.23c1000.10a",		0x20000, 0x495c4c5a, 4 | BRF_GRA },	      	  //  6
	{ "b19-04.23c1000.8a",		0x20000, 0x647007a0, 4 | BRF_GRA },	      	  //  7
	{ "b19-05.23c1000.7a",		0x20000, 0x49f2b1a5, 4 | BRF_GRA },	      	  //  8
	{ "b19-06.23c1000.5a",		0x20000, 0x2d39f1d0, 4 | BRF_GRA },	      	  //  9
	{ "b19-07.23c1000.4a",		0x20000, 0x8bb06f41, 4 | BRF_GRA },	      	  // 10
	{ "b19-08.23c1000.2a",		0x20000, 0x3584b491, 4 | BRF_GRA },	      	  // 11

	{ "b19-13.am27s29.15f",		0x00200, 0x6a547980, 5 | BRF_GRA },	      	  // 12 Color PROMs
	{ "b19-12.am27s29.16f",		0x00200, 0x5754e9d8, 5 | BRF_GRA },	      	  // 13

	/* these are shared with extermination */
	{ "b06-10.pal16l8a.d9.jed", 0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 14 Pal
	{ "b06-11.pal16l8a.d6.jed", 0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 15
	{ "b06-12.pal16l8a.c3.jed", 0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 16
	{ "b06-13.pal16l8a.c2.jed", 0x01000, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 17
};

STD_ROM_PICK(drtopplj)
STD_ROM_FN(drtopplj)

struct BurnDriver BurnDrvDrtopplj = {
	"drtoppelj", "drtoppel", NULL, NULL, "1987",
	"Dr. Toppel's Tankentai (Japan)\0", NULL, "Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, drtoppljRomInfo, drtoppljRomName, NULL, NULL, NULL, NULL, CommonInputInfo, DrtoppluDIPInfo,
	DrtoppelInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Kageki (World)

static struct BurnRomInfo kagekiRomDesc[] = {
	{ "b35-13.bin",		0x10000, 0xdc4b025f, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "b35-10.9c",		0x10000, 0xb150457d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "b35-14.bin",		0x10000, 0x8adef2d0, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "b35__01.13a",	0x20000, 0x01d83a69, 4 | BRF_GRA },	      	  //  3 Graphics
	{ "b35__02.12a",	0x20000, 0xd8af47ac, 4 | BRF_GRA },	      	  //  4
	{ "b35__03.10a",	0x20000, 0x3cb68797, 4 | BRF_GRA },	      	  //  5
	{ "b35__04.8a",		0x20000, 0x71c03f91, 4 | BRF_GRA },	      	  //  6
	{ "b35__05.7a",		0x20000, 0xa4e20c08, 4 | BRF_GRA },	      	  //  7
	{ "b35__06.5a",		0x20000, 0x3f8ab658, 4 | BRF_GRA },	      	  //  8
	{ "b35__07.4a",		0x20000, 0x1b4af049, 4 | BRF_GRA },	      	  //  9
	{ "b35__08.2a",		0x20000, 0xdeb2268c, 4 | BRF_GRA },	      	  // 10

	{ "b35-15.98g",		0x10000, 0xe6212a0f, 6 | BRF_SND },	      	  // 11 Samples

	/* these are shared with extermination except d9 */
	{ "b06-101.pal16l8a.d9.jed", 0x01000, 0x00000000, 7 | BRF_OPT | BRF_NODUMP }, // 12 Pal
	{ "b06-11.pal16l8a.d6.jed",  0x01000, 0x00000000, 7 | BRF_OPT | BRF_NODUMP }, // 13
	{ "b06-12.pal16l8a.c3.jed",  0x01000, 0x00000000, 7 | BRF_OPT | BRF_NODUMP }, // 14
	{ "b06-13.pal16l8a.c2.jed",  0x01000, 0x00000000, 7 | BRF_OPT | BRF_NODUMP }, // 15
};

STD_ROM_PICK(kageki)
STD_ROM_FN(kageki)

static INT32 KagekiInit()
{
	return Type1Init(MCU_NONE_KAGEKI);
}

struct BurnDriver BurnDrvKageki = {
	"kageki", NULL, NULL, NULL, "1988",
	"Kageki (World)\0", NULL, "Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_VSFIGHT, 0,
	NULL, kagekiRomInfo, kagekiRomName, NULL, NULL, NULL, NULL, CommonInputInfo, KagekiDIPInfo,
	KagekiInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Kageki (US)

static struct BurnRomInfo kagekiuRomDesc[] = {
	{ "b35-16.11c",		0x10000, 0xa4e6fd58, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "b35-10.9c",		0x10000, 0xb150457d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "b35-17.43e",		0x10000, 0xfdd9c246, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "b35__01.13a",	0x20000, 0x01d83a69, 4 | BRF_GRA },	      	  //  3 Graphics
	{ "b35__02.12a",	0x20000, 0xd8af47ac, 4 | BRF_GRA },	      	  //  4
	{ "b35__03.10a",	0x20000, 0x3cb68797, 4 | BRF_GRA },	      	  //  5
	{ "b35__04.8a",		0x20000, 0x71c03f91, 4 | BRF_GRA },	      	  //  6
	{ "b35__05.7a",		0x20000, 0xa4e20c08, 4 | BRF_GRA },	      	  //  7
	{ "b35__06.5a",		0x20000, 0x3f8ab658, 4 | BRF_GRA },	      	  //  8
	{ "b35__07.4a",		0x20000, 0x1b4af049, 4 | BRF_GRA },	      	  //  9
	{ "b35__08.2a",		0x20000, 0xdeb2268c, 4 | BRF_GRA },	      	  // 10

	{ "b35-15.98g",		0x10000, 0xe6212a0f, 6 | BRF_SND },	      	  // 11 Samples

	/* these are shared with extermination except d9 */
	{ "b06-101.pal16l8a.d9.jed", 0x01000, 0x00000000, 7 | BRF_OPT | BRF_NODUMP }, // 12 Pal
	{ "b06-11.pal16l8a.d6.jed",  0x01000, 0x00000000, 7 | BRF_OPT | BRF_NODUMP }, // 13
	{ "b06-12.pal16l8a.c3.jed",  0x01000, 0x00000000, 7 | BRF_OPT | BRF_NODUMP }, // 14
	{ "b06-13.pal16l8a.c2.jed",  0x01000, 0x00000000, 7 | BRF_OPT | BRF_NODUMP }, // 15
};

STD_ROM_PICK(kagekiu)
STD_ROM_FN(kagekiu)

struct BurnDriver BurnDrvKagekiu = {
	"kagekiu", "kageki", NULL, NULL, "1988",
	"Kageki (US)\0", NULL, "Taito America Corporation (Romstar license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_VSFIGHT, 0,
	NULL, kagekiuRomInfo, kagekiuRomName, NULL, NULL, NULL, NULL, CommonInputInfo, KagekiDIPInfo,
	KagekiInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Kageki (Japan)

static struct BurnRomInfo kagekijRomDesc[] = {
	{ "b35-09.11c",		0x10000, 0x829637d5, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "b35-10.9c",		0x10000, 0xb150457d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "b35-11.43e",		0x10000, 0x64d093fc, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "b35-01.13a",		0x20000, 0x01d83a69, 4 | BRF_GRA },	      	  //  3 Graphics
	{ "b35-02.12a",		0x20000, 0xd8af47ac, 4 | BRF_GRA },	      	  //  4
	{ "b35-03.10a",		0x20000, 0x3cb68797, 4 | BRF_GRA },	      	  //  5
	{ "b35-04.8a",		0x20000, 0x71c03f91, 4 | BRF_GRA },	      	  //  6
	{ "b35-05.7a",		0x20000, 0xa4e20c08, 4 | BRF_GRA },	      	  //  7
	{ "b35-06.5a",		0x20000, 0x3f8ab658, 4 | BRF_GRA },	      	  //  8
	{ "b35-07.4a",		0x20000, 0x1b4af049, 4 | BRF_GRA },	      	  //  9
	{ "b35-08.2a",		0x20000, 0xdeb2268c, 4 | BRF_GRA },	      	  // 10

	{ "b35-12.98g",		0x10000, 0x184409f1, 6 | BRF_SND },	      	  // 11 Samples

	/* these are shared with extermination except d9 */
	{ "b06-101.pal16l8a.d9.jed", 0x01000, 0x00000000, 7 | BRF_OPT | BRF_NODUMP }, // 12 Pal
	{ "b06-11.pal16l8a.d6.jed",  0x01000, 0x00000000, 7 | BRF_OPT | BRF_NODUMP }, // 13
	{ "b06-12.pal16l8a.c3.jed",  0x01000, 0x00000000, 7 | BRF_OPT | BRF_NODUMP }, // 14
	{ "b06-13.pal16l8a.c2.jed",  0x01000, 0x00000000, 7 | BRF_OPT | BRF_NODUMP }, // 15
};

STD_ROM_PICK(kagekij)
STD_ROM_FN(kagekij)

struct BurnDriver BurnDrvKagekij = {
	"kagekij", "kageki", NULL, NULL, "1988",
	"Kageki (Japan)\0", NULL, "Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_VSFIGHT, 0,
	NULL, kagekijRomInfo, kagekijRomName, NULL, NULL, NULL, NULL, CommonInputInfo, KagekiDIPInfo,
	KagekiInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Kageki (hack)

static struct BurnRomInfo kagekihRomDesc[] = {
	{ "b35_16.11c",		0x10000, 0x1cf67603, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "b35-10.9c",		0x10000, 0xb150457d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "b35-11.43e",		0x10000, 0x64d093fc, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "b35-01.13a",		0x20000, 0x01d83a69, 4 | BRF_GRA },	      	  //  3 Graphics
	{ "b35-02.12a",		0x20000, 0xd8af47ac, 4 | BRF_GRA },	      	  //  4
	{ "b35-03.10a",		0x20000, 0x3cb68797, 4 | BRF_GRA },	      	  //  5
	{ "b35-04.8a",		0x20000, 0x71c03f91, 4 | BRF_GRA },	      	  //  6
	{ "b35-05.7a",		0x20000, 0xa4e20c08, 4 | BRF_GRA },	      	  //  7
	{ "b35-06.5a",		0x20000, 0x3f8ab658, 4 | BRF_GRA },	      	  //  8
	{ "b35-07.4a",		0x20000, 0x1b4af049, 4 | BRF_GRA },	      	  //  9
	{ "b35-08.2a",		0x20000, 0xdeb2268c, 4 | BRF_GRA },	      	  // 10

	{ "b35-12.98g",		0x10000, 0x184409f1, 6 | BRF_SND },	      	  // 11 Samples

	/* these are shared with extermination except d9 */
	{ "b06-101.pal16l8a.d9.jed", 0x01000, 0x00000000, 7 | BRF_OPT | BRF_NODUMP }, // 12 Pal
	{ "b06-11.pal16l8a.d6.jed",  0x01000, 0x00000000, 7 | BRF_OPT | BRF_NODUMP }, // 13
	{ "b06-12.pal16l8a.c3.jed",  0x01000, 0x00000000, 7 | BRF_OPT | BRF_NODUMP }, // 14
	{ "b06-13.pal16l8a.c2.jed",  0x01000, 0x00000000, 7 | BRF_OPT | BRF_NODUMP }, // 15
};

STD_ROM_PICK(kagekih)
STD_ROM_FN(kagekih)

struct BurnDriver BurnDrvKagekih = {
	"kagekih", "kageki", NULL, NULL, "1992",
	"Kageki (hack)\0", NULL, "Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_VSFIGHT, 0,
	NULL, kagekihRomInfo, kagekihRomName, NULL, NULL, NULL, NULL, CommonInputInfo, KagekiDIPInfo,
	KagekiInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Chuka Taisen (World) (P0-028-A PCB)

static struct BurnRomInfo chukataiRomDesc[] = {
	{ "b44-10",			0x10000, 0x8c69e008, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "b44-11",			0x10000, 0x32484094, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "b44-12w",		0x10000, 0xe80ecdca, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	/* B44 // 09 is the label? what is the mask number under the label? maybe Taito M-011? last digit is definitely 1 */
	{ "b44-8742.mcu",	0x00800, 0x7dff3f9f, 3 | BRF_PRG | BRF_OPT }, //  3 I8742 MCU

	{ "b44-01.a13",		0x20000, 0xaae7b3d5, 4 | BRF_GRA },	      	  //  4 Graphics
	{ "b44-02.a12",		0x20000, 0x7f0b9568, 4 | BRF_GRA },	      	  //  5
	{ "b44-03.a10",		0x20000, 0x5a54a3b9, 4 | BRF_GRA },	      	  //  6
	{ "b44-04.a08",		0x20000, 0x3c5f544b, 4 | BRF_GRA },	      	  //  7
	{ "b44-05.a07",		0x20000, 0xd1b7e314, 4 | BRF_GRA },	      	  //  8
	{ "b44-06.a05",		0x20000, 0x269978a8, 4 | BRF_GRA },	      	  //  9
	{ "b44-07.a04",		0x20000, 0x3e0e737e, 4 | BRF_GRA },	      	  // 10
	{ "b44-08.a02",		0x20000, 0x6cb1e8fc, 4 | BRF_GRA },	      	  // 11

	/* these are shared with extermination except d9 */
	{ "b06-101.pal16l8a.d9.jed", 0x01000, 0x00000000, 5 | BRF_OPT | BRF_NODUMP }, // 12 Pal
	{ "b06-11.pal16l8a.d6.jed",  0x01000, 0x00000000, 5 | BRF_OPT | BRF_NODUMP }, // 13
	{ "b06-12.pal16l8a.c3.jed",  0x01000, 0x00000000, 5 | BRF_OPT | BRF_NODUMP }, // 14
	{ "b06-13.pal16l8a.c2.jed",  0x01000, 0x00000000, 5 | BRF_OPT | BRF_NODUMP }, // 15
};

STD_ROM_PICK(chukatai)
STD_ROM_FN(chukatai)

static INT32 ChukataiInit()
{
	return Type1Init(MCU_CHUKATAI);
}

struct BurnDriver BurnDrvChukatai = {
	"chukatai", NULL, NULL, NULL, "1988",
	"Chuka Taisen (World) (P0-028-A PCB)\0", NULL, "Taito Corporation Japan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_HORSHOOT, 0,
	NULL, chukataiRomInfo, chukataiRomName, NULL, NULL, NULL, NULL, CommonInputInfo, ChukataiDIPInfo,
	ChukataiInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// Chuka Taisen (US) (P0-028-A PCB)

static struct BurnRomInfo chukatauRomDesc[] = {
	{ "b44-10",			0x10000, 0x8c69e008, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "b44-11",			0x10000, 0x32484094, 1 | BRF_PRG | BRF_ESS }, //  1

	/* Hacked??, need correct Taito rom number */
	{ "b44-12u",		0x10000, 0x9f09fd5c, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	/* B44 // 09 is the label? what is the mask number under the label? maybe Taito M-011? last digit is definitely 1 */
	{ "b44-8742.mcu",	0x00800, 0x7dff3f9f, 3 | BRF_PRG | BRF_OPT }, //  3 I8742 MCU

	{ "b44-01.a13",		0x20000, 0xaae7b3d5, 4 | BRF_GRA },	      	  //  4 Graphics
	{ "b44-02.a12",		0x20000, 0x7f0b9568, 4 | BRF_GRA },	      	  //  5
	{ "b44-03.a10",		0x20000, 0x5a54a3b9, 4 | BRF_GRA },	      	  //  6
	{ "b44-04.a08",		0x20000, 0x3c5f544b, 4 | BRF_GRA },	      	  //  7
	{ "b44-05.a07",		0x20000, 0xd1b7e314, 4 | BRF_GRA },	      	  //  8
	{ "b44-06.a05",		0x20000, 0x269978a8, 4 | BRF_GRA },	      	  //  9
	{ "b44-07.a04",		0x20000, 0x3e0e737e, 4 | BRF_GRA },	      	  // 10
	{ "b44-08.a02",		0x20000, 0x6cb1e8fc, 4 | BRF_GRA },	      	  // 11

	/* these are shared with extermination except d9 */
	{ "b06-101.pal16l8a.d9.jed", 0x01000, 0x00000000, 5 | BRF_OPT | BRF_NODUMP }, // 12 Pal
	{ "b06-11.pal16l8a.d6.jed",  0x01000, 0x00000000, 5 | BRF_OPT | BRF_NODUMP }, // 13
	{ "b06-12.pal16l8a.c3.jed",  0x01000, 0x00000000, 5 | BRF_OPT | BRF_NODUMP }, // 14
	{ "b06-13.pal16l8a.c2.jed",  0x01000, 0x00000000, 5 | BRF_OPT | BRF_NODUMP }, // 15
};

STD_ROM_PICK(chukatau)
STD_ROM_FN(chukatau)

struct BurnDriver BurnDrvChukatau = {
	"chukataiu", "chukatai", NULL, NULL, "1988",
	"Chuka Taisen (US) (P0-028-A PCB)\0", NULL, "Taito America Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_HORSHOOT, 0,
	NULL, chukatauRomInfo, chukatauRomName, NULL, NULL, NULL, NULL, CommonInputInfo, ChukatauDIPInfo,
	ChukataiInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// Chuka Taisen (Japan) (P0-028-A PCB)

static struct BurnRomInfo chukatajRomDesc[] = {
	{ "b44-10",			0x10000, 0x8c69e008, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "b44-11",			0x10000, 0x32484094, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "b44-12",			0x10000, 0x0600ace6, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	/* B44 // 09 is the label? what is the mask number under the label? maybe Taito M-011? last digit is definitely 1 */
	{ "b44-8742.mcu",	0x00800, 0x7dff3f9f, 3 | BRF_PRG | BRF_OPT }, //  3 I8742 MCU

	{ "b44-01.a13",		0x20000, 0xaae7b3d5, 4 | BRF_GRA },	      	  //  4 Graphics
	{ "b44-02.a12",		0x20000, 0x7f0b9568, 4 | BRF_GRA },	      	  //  5
	{ "b44-03.a10",		0x20000, 0x5a54a3b9, 4 | BRF_GRA },	      	  //  6
	{ "b44-04.a08",		0x20000, 0x3c5f544b, 4 | BRF_GRA },	      	  //  7
	{ "b44-05.a07",		0x20000, 0xd1b7e314, 4 | BRF_GRA },	      	  //  8
	{ "b44-06.a05",		0x20000, 0x269978a8, 4 | BRF_GRA },	      	  //  9
	{ "b44-07.a04",		0x20000, 0x3e0e737e, 4 | BRF_GRA },	      	  // 10
	{ "b44-08.a02",		0x20000, 0x6cb1e8fc, 4 | BRF_GRA },	      	  // 11

	/* these are shared with extermination except d9 */
	{ "b06-101.pal16l8a.d9.jed", 0x01000, 0x00000000, 5 | BRF_OPT | BRF_NODUMP }, // 12 Pal
	{ "b06-11.pal16l8a.d6.jed",  0x01000, 0x00000000, 5 | BRF_OPT | BRF_NODUMP }, // 13
	{ "b06-12.pal16l8a.c3.jed",  0x01000, 0x00000000, 5 | BRF_OPT | BRF_NODUMP }, // 14
	{ "b06-13.pal16l8a.c2.jed",  0x01000, 0x00000000, 5 | BRF_OPT | BRF_NODUMP }, // 15
};

STD_ROM_PICK(chukataj)
STD_ROM_FN(chukataj)

struct BurnDriver BurnDrvChukataj = {
	"chukataij", "chukatai", NULL, NULL, "1988",
	"Chuka Taisen (Japan) (P0-028-A PCB)\0", NULL, "Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_HORSHOOT, 0,
	NULL, chukatajRomInfo, chukatajRomName, NULL, NULL, NULL, NULL, CommonInputInfo, ChukatauDIPInfo,
	ChukataiInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// Chuka Taisen (Japan) (P0-025-A PCB)

static struct BurnRomInfo chukatajaRomDesc[] = {
	{ "b44-31.11c",		0x10000, 0x134d3c9e, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "b44-11.9c",		0x10000, 0x32484094, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "b44-32.4e",		0x10000, 0xf52d2f90, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	/* Labeled B06-14 and under printed label "Taito M-001, 128P, 720100", is a mask 8042 */
	{ "b06__14.1g",		0x00800, 0x28907072, 3 | BRF_PRG | BRF_OPT }, //  3 I8742 MCU

	{ "b44-21.rom4l",	0x20000, 0xaae7b3d5, 4 | BRF_GRA },	      	  //  4 Graphics
	{ "b44-22.rom4h",	0x20000, 0x7f0b9568, 4 | BRF_GRA },	      	  //  5
	{ "b44-23.rom3l",	0x20000, 0x5a54a3b9, 4 | BRF_GRA },	      	  //  6
	{ "b44-24.rom3h",	0x20000, 0x3c5f544b, 4 | BRF_GRA },	      	  //  7
	{ "b44-25.rom2l",	0x20000, 0xd1b7e314, 4 | BRF_GRA },	      	  //  8
	{ "b44-26.rom2h",	0x20000, 0x269978a8, 4 | BRF_GRA },	      	  //  9
	{ "b44-27.rom1l",	0x20000, 0x3e0e737e, 4 | BRF_GRA },	      	  // 10
	{ "b44-28.rom1h",	0x20000, 0x6cb1e8fc, 4 | BRF_GRA },	      	  // 11

	{ "b44-30.15f",		0x00200, 0xb3de8312, 5 | BRF_GRA },	      	  // 12 Color PROMs
	{ "b44-29.17f",		0x00200, 0xae44b8fb, 5 | BRF_GRA },	      	  // 13

	/* these are shared with extermination */
	{ "b06-10.pal16l8a.d9.jed",  0x01000, 0x00000000, 5 | BRF_OPT | BRF_NODUMP }, // 14 Pal
	{ "b06-11.pal16l8a.d6.jed",  0x01000, 0x00000000, 5 | BRF_OPT | BRF_NODUMP }, // 15
	{ "b06-12.pal16l8a.c3.jed",  0x01000, 0x00000000, 5 | BRF_OPT | BRF_NODUMP }, // 16
	{ "b06-13.pal16l8a.c2.jed",  0x01000, 0x00000000, 5 | BRF_OPT | BRF_NODUMP }, // 17
};

STD_ROM_PICK(chukataja)
STD_ROM_FN(chukataja)

static INT32 ChukataijaInit()
{
	return Type1Init(MCU_DRTOPPEL);
}

struct BurnDriver BurnDrvChukataja = {
	"chukataija", "chukatai", NULL, NULL, "1988",
	"Chuka Taisen (Japan) (P0-025-A PCB)\0", NULL, "Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_HORSHOOT, 0,
	NULL, chukatajaRomInfo, chukatajaRomName, NULL, NULL, NULL, NULL, CommonInputInfo, ChukatauDIPInfo,
	ChukataijaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// The NewZealand Story (World, newer)

static struct BurnRomInfo tnzsRomDesc[] = {
	{ "b53-24.u1",   	0x20000, 0xd66824c6, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	{ "b53-25.u3",   	0x10000, 0xd6ac4e71, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code

	{ "b53-26.u34", 	0x10000, 0xcfd5649c, 3 | BRF_PRG | BRF_ESS }, //  2 Z80 #2 Code

	{ "b53-16.ic7",		0x20000, 0xc3519c2a, 4 | BRF_GRA },	      	  //  3 Graphics
	{ "b53-17.ic8",		0x20000, 0x2bf199e8, 4 | BRF_GRA },	      	  //  4
	{ "b53-18.ic9",		0x20000, 0x92f35ed9, 4 | BRF_GRA },	      	  //  5
	{ "b53-19.ic10",	0x20000, 0xedbb9581, 4 | BRF_GRA },	      	  //  6
	{ "b53-22.ic11",	0x20000, 0x59d2aef6, 4 | BRF_GRA },	      	  //  7
	{ "b53-23.ic13",	0x20000, 0x74acfb9b, 4 | BRF_GRA },	      	  //  8
	{ "b53-20.ic12",	0x20000, 0x095d0dc0, 4 | BRF_GRA },	      	  //  9
	{ "b53-21.ic14",	0x20000, 0x9800c54d, 4 | BRF_GRA },	      	  // 10

	/* these are shared with extermination except for the subpcb pal */
	{ "b06-13.pal16l8a.f2.jed", 		0x01000, 0x00000000, 5 | BRF_OPT | BRF_NODUMP }, // 11 Pal
	{ "b06-101.pal16l8a.i2.jed", 		0x01000, 0x00000000, 5 | BRF_OPT | BRF_NODUMP }, // 12
	{ "b53-15.pal16l8a.subpcb.ic6.jed", 0x01000, 0x00000000, 5 | BRF_OPT | BRF_NODUMP }, // 13 on sub pcb
};

STD_ROM_PICK(tnzs)
STD_ROM_FN(tnzs)

struct BurnDriver BurnDrvtnzs = {
	"tnzs", NULL, NULL, NULL, "1988",
	"The NewZealand Story (World, newer)\0", NULL, "Taito Corporation Japan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, tnzsRomInfo, tnzsRomName, NULL, NULL, NULL, NULL, CommonInputInfo, TnzsDIPInfo,
	Type2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// The NewZealand Story (Japan, newer)

static struct BurnRomInfo tnzsjRomDesc[] = {
	{ "b53-24.u1",  	0x20000, 0xd66824c6, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	{ "b53-27.u3",  	0x10000, 0xb3415fc3, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code

	{ "b53-26.u34", 	0x10000, 0xcfd5649c, 3 | BRF_PRG | BRF_ESS }, //  2 Z80 #2 Code

	{ "b53-16.ic7",		0x20000, 0xc3519c2a, 4 | BRF_GRA },	      	  //  3 Graphics
	{ "b53-17.ic8",		0x20000, 0x2bf199e8, 4 | BRF_GRA },	      	  //  4
	{ "b53-18.ic9",		0x20000, 0x92f35ed9, 4 | BRF_GRA },	      	  //  5
	{ "b53-19.ic10",	0x20000, 0xedbb9581, 4 | BRF_GRA },	      	  //  6
	{ "b53-22.ic11",	0x20000, 0x59d2aef6, 4 | BRF_GRA },	      	  //  7
	{ "b53-23.ic13",	0x20000, 0x74acfb9b, 4 | BRF_GRA },	      	  //  8
	{ "b53-20.ic12",	0x20000, 0x095d0dc0, 4 | BRF_GRA },	      	  //  9
	{ "b53-21.ic14",	0x20000, 0x9800c54d, 4 | BRF_GRA },	      	  // 10

	/* these are shared with extermination except for the subpcb pal */
	{ "b06-13.pal16l8a.f2.jed", 		0x01000, 0x00000000, 5 | BRF_OPT | BRF_NODUMP }, // 11 Pal
	{ "b06-101.pal16l8a.i2.jed", 		0x01000, 0x00000000, 5 | BRF_OPT | BRF_NODUMP }, // 12
	{ "b53-15.pal16l8a.subpcb.ic6.jed", 0x01000, 0x00000000, 5 | BRF_OPT | BRF_NODUMP }, // 13 on sub pcb
};

STD_ROM_PICK(tnzsj)
STD_ROM_FN(tnzsj)

struct BurnDriver BurnDrvtnzsj = {
	"tnzsj", "tnzs", NULL, NULL, "1988",
	"The NewZealand Story (Japan, new version, newer PCB)\0", NULL, "Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, tnzsjRomInfo, tnzsjRomName, NULL, NULL, NULL, NULL, CommonInputInfo, TnzsjDIPInfo,
	Type2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// The NewZealand Story (Japan, old version) (older PCB)

static struct BurnRomInfo tnzsjoRomDesc[] = {
	{ "b53-10.27c1001d.u32",		0x20000, 0xa73745c6, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	{ "b53-11.27c512.u38",			0x10000, 0x9784d443, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code

	{ "b53-09.u46",					0x00800, 0xa4bfce19, 3 | BRF_PRG | BRF_OPT }, //  2 I8742 MCU

	{ "b53-08.u8",					0x20000, 0xc3519c2a, 4 | BRF_GRA },	      	  //  3 Graphics
	{ "b53-07.u7",					0x20000, 0x2bf199e8, 4 | BRF_GRA },	      	  //  4
	{ "b53-06.u6",					0x20000, 0x92f35ed9, 4 | BRF_GRA },	      	  //  5
	{ "b53-05.u5",					0x20000, 0xedbb9581, 4 | BRF_GRA },	      	  //  6
	{ "b53-04.u4",					0x20000, 0x59d2aef6, 4 | BRF_GRA },	      	  //  7
	{ "b53-03.u3",					0x20000, 0x74acfb9b, 4 | BRF_GRA },	      	  //  8
	{ "b53-02.u2",					0x20000, 0x095d0dc0, 4 | BRF_GRA },	      	  //  9
	{ "b53-01.u1",					0x20000, 0x9800c54d, 4 | BRF_GRA },	      	  // 10

	/* these are probably shared with extermination except for u35 */
	{ "b06-12.pal16l8a.u26.jed",  	0x01000, 0x00000000, 5 | BRF_OPT | BRF_NODUMP }, // 11 Pal
	{ "b06-13.pal16l8a.u25.jed",  	0x01000, 0x00000000, 5 | BRF_OPT | BRF_NODUMP }, // 12
	{ "b53-12.pal16l8a.u35.jed",  	0x01000, 0x00000000, 5 | BRF_OPT | BRF_NODUMP }, // 13
	{ "b06-101.pal16l8a.u36.jed", 	0x01000, 0x00000000, 5 | BRF_OPT | BRF_NODUMP }, // 14
};

STD_ROM_PICK(tnzsjo)
STD_ROM_FN(tnzsjo)

static INT32 TnzsoInit()
{
	return Type1Init(MCU_TNZS);
}

struct BurnDriver BurnDrvTnzsjo = {
	"tnzsjo", "tnzs", NULL, NULL, "1988",
	"The NewZealand Story (Japan, old version) (older PCB)\0", NULL, "Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, tnzsjoRomInfo, tnzsjoRomName, NULL, NULL, NULL, NULL, CommonInputInfo, TnzsjoDIPInfo,
	TnzsoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// The NewZealand Story (US, old version) (older PCB)

static struct BurnRomInfo tnzsuoRomDesc[] = {
	{ "b53-10.27c1001d.u32",		0x20000, 0xa73745c6, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	{ "b53-13.27c512.u38",			0x10000, 0xc09f4d28, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code

	{ "b53-09.u46",					0x00800, 0xa4bfce19, 3 | BRF_PRG | BRF_OPT }, //  2 I8742 MCU

	{ "b53-08.u8",					0x20000, 0xc3519c2a, 4 | BRF_GRA },	      	  //  3 Graphics
	{ "b53-07.u7",					0x20000, 0x2bf199e8, 4 | BRF_GRA },	      	  //  4
	{ "b53-06.u6",					0x20000, 0x92f35ed9, 4 | BRF_GRA },	      	  //  5
	{ "b53-05.u5",					0x20000, 0xedbb9581, 4 | BRF_GRA },	      	  //  6
	{ "b53-04.u4",					0x20000, 0x59d2aef6, 4 | BRF_GRA },	      	  //  7
	{ "b53-03.u3",					0x20000, 0x74acfb9b, 4 | BRF_GRA },	      	  //  8
	{ "b53-02.u2",					0x20000, 0x095d0dc0, 4 | BRF_GRA },	      	  //  9
	{ "b53-01.u1",					0x20000, 0x9800c54d, 4 | BRF_GRA },	      	  // 10

	/* these are probably shared with extermination except for u35 */
	{ "b06-12.pal16l8a.u26.jed",  	0x01000, 0x00000000, 5 | BRF_OPT | BRF_NODUMP }, // 11 Pal
	{ "b06-13.pal16l8a.u25.jed",  	0x01000, 0x00000000, 5 | BRF_OPT | BRF_NODUMP }, // 12
	{ "b53-12.pal16l8a.u35.jed",  	0x01000, 0x00000000, 5 | BRF_OPT | BRF_NODUMP }, // 13
	{ "b06-101.pal16l8a.u36.jed", 	0x01000, 0x00000000, 5 | BRF_OPT | BRF_NODUMP }, // 14
};

STD_ROM_PICK(tnzsuo)
STD_ROM_FN(tnzsuo)

struct BurnDriver BurnDrvTnzsuo = {
	"tnzsuo", "tnzs", NULL, NULL, "1988",
	"The NewZealand Story (US, old version) (older PCB)\0", NULL, "Taito America Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, tnzsuoRomInfo, tnzsuoRomName, NULL, NULL, NULL, NULL, CommonInputInfo, TnzsjoDIPInfo,
	TnzsoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// The NewZealand Story (World, old version) (older PCB)

static struct BurnRomInfo tnzsoRomDesc[] = {
	{ "b53-10.27c1001d.u32",		0x20000, 0xa73745c6, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	{ "b53-14.u38",					0x10000, 0xf269c5f1, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code

	{ "b53-09.u46",					0x00800, 0xa4bfce19, 3 | BRF_PRG | BRF_OPT }, //  2 I8742 MCU

	{ "b53-08.u8",					0x20000, 0xc3519c2a, 4 | BRF_GRA },	      	  //  3 Graphics
	{ "b53-07.u7",					0x20000, 0x2bf199e8, 4 | BRF_GRA },	      	  //  4
	{ "b53-06.u6",					0x20000, 0x92f35ed9, 4 | BRF_GRA },	      	  //  5
	{ "b53-05.u5",					0x20000, 0xedbb9581, 4 | BRF_GRA },	      	  //  6
	{ "b53-04.u4",					0x20000, 0x59d2aef6, 4 | BRF_GRA },	      	  //  7
	{ "b53-03.u3",					0x20000, 0x74acfb9b, 4 | BRF_GRA },	      	  //  8
	{ "b53-02.u2",					0x20000, 0x095d0dc0, 4 | BRF_GRA },	      	  //  9
	{ "b53-01.u1",					0x20000, 0x9800c54d, 4 | BRF_GRA },	      	  // 10

	/* these are probably shared with extermination except for u35 */
	{ "b06-12.pal16l8a.u26.jed",  	0x01000, 0x00000000, 5 | BRF_OPT | BRF_NODUMP }, // 11 Pal
	{ "b06-13.pal16l8a.u25.jed",  	0x01000, 0x00000000, 5 | BRF_OPT | BRF_NODUMP }, // 12
	{ "b53-12.pal16l8a.u35.jed",  	0x01000, 0x00000000, 5 | BRF_OPT | BRF_NODUMP }, // 13
	{ "b06-101.pal16l8a.u36.jed", 	0x01000, 0x00000000, 5 | BRF_OPT | BRF_NODUMP }, // 14
};

STD_ROM_PICK(tnzso)
STD_ROM_FN(tnzso)

struct BurnDriver BurnDrvTnzso = {
	"tnzso", "tnzs", NULL, NULL, "1988",
	"The NewZealand Story (World, old version) (older PCB)\0", NULL, "Taito Corporation Japan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, tnzsoRomInfo, tnzsoRomName, NULL, NULL, NULL, NULL, CommonInputInfo, TnzsopDIPInfo,
	TnzsoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};

// The NewZealand Story (World, unknown version) (older PCB)
// is this a legit set, or a hack, or a near-final (later than tnzsop below) prototype?

static struct BurnRomInfo tnzsoaRomDesc[] = {
	{ "b53-unknown.27c1001d.u32",	0x20000, 0xedf3b39e, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	{ "b53-unknown.27c512.u38",		0x10000, 0x60340d63, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code

	{ "b53-09.u46",					0x00800, 0xa4bfce19, 3 | BRF_PRG | BRF_OPT }, //  2 I8742 MCU

	{ "b53-08.u8",					0x20000, 0xc3519c2a, 4 | BRF_GRA },	      	  //  3 Graphics
	{ "b53-07.u7",					0x20000, 0x2bf199e8, 4 | BRF_GRA },	      	  //  4
	{ "b53-06.u6",					0x20000, 0x92f35ed9, 4 | BRF_GRA },	      	  //  5
	{ "b53-05.u5",					0x20000, 0xedbb9581, 4 | BRF_GRA },	      	  //  6
	{ "b53-04.u4",					0x20000, 0x59d2aef6, 4 | BRF_GRA },	      	  //  7
	{ "b53-03.u3",					0x20000, 0x74acfb9b, 4 | BRF_GRA },	      	  //  8
	{ "b53-02.u2",					0x20000, 0x095d0dc0, 4 | BRF_GRA },	      	  //  9
	{ "b53-01.u1",					0x20000, 0x9800c54d, 4 | BRF_GRA },	      	  // 10

	/* PALS not directly observed on this board but assumed to exist */
	/* these are probably shared with extermination except for u35 */
	{ "b06-12.pal16l8a.u26.jed",  	0x01000, 0x00000000, 5 | BRF_OPT | BRF_NODUMP }, // 11 Pal
	{ "b06-13.pal16l8a.u25.jed",  	0x01000, 0x00000000, 5 | BRF_OPT | BRF_NODUMP }, // 12
	{ "b53-12.pal16l8a.u35.jed",  	0x01000, 0x00000000, 5 | BRF_OPT | BRF_NODUMP }, // 13
	{ "b06-101.pal16l8a.u36.jed", 	0x01000, 0x00000000, 5 | BRF_OPT | BRF_NODUMP }, // 14
};

STD_ROM_PICK(tnzsoa)
STD_ROM_FN(tnzsoa)

struct BurnDriver BurnDrvTnzsoa = {
	"tnzsoa", "tnzs", NULL, NULL, "1988",
	"The NewZealand Story (World, unknown version) (older PCB)\0", NULL, "Taito Corporation Japan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, tnzsoaRomInfo, tnzsoaRomName, NULL, NULL, NULL, NULL, CommonInputInfo, TnzsopDIPInfo,
	TnzsoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};

/* This is a prototype CA403001A PCB (Seta: P0-041-1), and is ALMOST but not exactly the same as the K1100356A/J1100156A (Seta: P0-041A) 'tnzsuo/tnzsjo/arkanoid2/etc' pcb above:
This pcb uses 32-pin 27c1000d eproms for the 8 gfx roms, and the final K1100356A/J1100156A pcb uses 28 pin 23c1000 mask roms instead. Some capacitors near the jamma connector were moved as well.
No other obviously evident routing/wiring changes are present.
This type of pcb might have been used for in-house testing of all the games on this hardware.
*/

// The NewZealand Story (World, prototype) (older PCB)
// prototype (location test?) version; has different rom labels, and the Seta X1-001 chip has prototype markings revealing it was fabbed by Yamaha, as 'YM3906'

static struct BurnRomInfo tnzsopRomDesc[] = {
	{ "c-11__6-24__1959h.d27c1000d-15.u32",	0x20000, 0x3c1dae7b, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	{ "e-3__6-24__c4ach.tmm27512d-20.u38",	0x10000, 0xc7662e96, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code

	{ "b8042h___88-6-22__0fcc.d8742.u46",	0x00800, 0xa4bfce19, 3 | BRF_PRG | BRF_OPT }, //  2 I8742 MCU

	{ "a13__03e8.d27c1000d-15.a13",			0x20000, 0x7e0bd5bb, 4 | BRF_GRA },	      	  //  3 Graphics
	{ "a12__f4ec.d27c1000d-15.a12",			0x20000, 0x95880726, 4 | BRF_GRA },	      	  //  4
	{ "a10__f2b5.d27c1000d-15.a10",			0x20000, 0x2bc4c053, 4 | BRF_GRA },	      	  //  5
	{ "a08__bd49.d27c1000d-15.a8",			0x20000, 0x8ff8d88c, 4 | BRF_GRA },	      	  //  6
	{ "a07__d5f3.d27c1000d-15.a7",			0x20000, 0x291bcaca, 4 | BRF_GRA },	      	  //  7
	{ "a05__662a.d27c1000d-15.a5",			0x20000, 0x6e762e20, 4 | BRF_GRA },	      	  //  8
	{ "a04__0c21.d27c1000d-15.a4",			0x20000, 0xe1fd1b9d, 4 | BRF_GRA },	      	  //  9
	{ "a02__904f.d27c1000d-15.a2",			0x20000, 0x2ab06bda, 4 | BRF_GRA },	      	  // 10

	/* these are probably shared with extermination except for u35 */
	{ "b06-12.pal16l8a.u26.jed",  	0x01000, 0x00000000, 5 | BRF_OPT | BRF_NODUMP }, // 11 Pal
	{ "b06-13.pal16l8a.u25.jed",  	0x01000, 0x00000000, 5 | BRF_OPT | BRF_NODUMP }, // 12
	{ "st-6.pal16l8a.u35.jed",    	0x01000, 0x00000000, 5 | BRF_OPT | BRF_NODUMP }, // 13 // likely has a different name on the proto pcb...
	{ "b06-101.pal16l8a.u36.jed", 	0x01000, 0x00000000, 5 | BRF_OPT | BRF_NODUMP }, // 14
};

STD_ROM_PICK(tnzsop)
STD_ROM_FN(tnzsop)

struct BurnDriver BurnDrvTnzsop = {
	"tnzsop", "tnzs", NULL, NULL, "1988",
	"The NewZealand Story (World, prototype) (older PCB)\0", NULL, "Taito Corporation Japan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, tnzsopRomInfo, tnzsopRomName, NULL, NULL, NULL, NULL, CommonInputInfo, TnzsopDIPInfo,
	TnzsoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


static INT32 kabukizInit()
{
	game_kabukiz = 1;

	return Type2Init();
}

// Kabuki-Z (World)

static struct BurnRomInfo kabukizRomDesc[] = {
	{ "b50-05.u1",		0x20000, 0x9cccb129, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	{ "b50-08.1e",		0x10000, 0xcb92d34c, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code

	{ "b50-07.u34",		0x20000, 0xbf7fc2ed, 3 | BRF_PRG | BRF_ESS }, //  2 Z80 #2 Code

	{ "b50-04.u35",		0x80000, 0x04829aa9, 4 | BRF_GRA },	      	  //  3 Graphics
	{ "b50-03.u39",		0x80000, 0x31489a4c, 4 | BRF_GRA },	      	  //  4
	{ "b50-02.u43",		0x80000, 0x90b8a8e7, 4 | BRF_GRA },	      	  //  5
	{ "b50-01.u46",		0x80000, 0xf4277751, 4 | BRF_GRA },	      	  //  6
};

STD_ROM_PICK(kabukiz)
STD_ROM_FN(kabukiz)

struct BurnDriver BurnDrvKabukiz = {
	"kabukiz", NULL, NULL, NULL, "1988",
	"Kabuki-Z (World)\0", NULL, "Taito Corporation Japan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_SCRFIGHT, 0,
	NULL, kabukizRomInfo, kabukizRomName, NULL, NULL, NULL, NULL, CommonInputInfo, KabukizDIPInfo,
	kabukizInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// Kabuki-Z (Japan)

static struct BurnRomInfo kabukizjRomDesc[] = {
	{ "b50-05.u1",		0x20000, 0x9cccb129, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	{ "b50-06.u3",		0x10000, 0x45650aab, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code

	{ "b50-07.u34",		0x20000, 0xbf7fc2ed, 3 | BRF_PRG | BRF_ESS }, //  2 Z80 #2 Code

	{ "b50-04.u35",		0x80000, 0x04829aa9, 4 | BRF_GRA },	      //  3 Graphics
	{ "b50-03.u39",		0x80000, 0x31489a4c, 4 | BRF_GRA },	      //  4
	{ "b50-02.u43",		0x80000, 0x90b8a8e7, 4 | BRF_GRA },	      //  5
	{ "b50-01.u46",		0x80000, 0xf4277751, 4 | BRF_GRA },	      //  6
};

STD_ROM_PICK(kabukizj)
STD_ROM_FN(kabukizj)

struct BurnDriver BurnDrvKabukizj = {
	"kabukizj", "kabukiz", NULL, NULL, "1988",
	"Kabuki-Z (Japan)\0", NULL, "Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_SCRFIGHT, 0,
	NULL, kabukizjRomInfo, kabukizjRomName, NULL, NULL, NULL, NULL, CommonInputInfo, KabukizjDIPInfo,
	kabukizInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// Insector X (World)

static struct BurnRomInfo insectxRomDesc[] = {
	{ "b97__03.u32",	0x20000, 0x18eef387, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	/* Label is B97 07* with an asterisk */
	{ "b97__07.u38",	0x10000, 0x324b28c9, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code

	{ "b97__01.u1",		0x80000, 0xd00294b1, 4 | BRF_GRA },	      	  //  2 Graphics
	{ "b97__02.u2",		0x80000, 0xdb5a7434, 4 | BRF_GRA },	      	  //  3
};

STD_ROM_PICK(insectx)
STD_ROM_FN(insectx)

static INT32 InsectxInit()
{
	return Type1Init(MCU_NONE_INSECTX);
}

struct BurnDriver BurnDrvInsectx = {
	"insectx", NULL, NULL, NULL, "1989",
	"Insector X (World)\0", NULL, "Taito Corporation Japan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_HORSHOOT, 0,
	NULL, insectxRomInfo, insectxRomName, NULL, NULL, NULL, NULL, InsectxInputInfo, InsectxDIPInfo,
	InsectxInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// Insector X (Japan)

static struct BurnRomInfo insectxjRomDesc[] = {
	{ "b97__03.u32",	0x20000, 0x18eef387, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	/* Label is B97 04* with an asterisk */
	{ "b97__04.u38",	0x10000, 0xdc4549e5, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code

	{ "b97__01.u1",		0x80000, 0xd00294b1, 4 | BRF_GRA },	      	  //  2 Graphics
	{ "b97__02.u2",		0x80000, 0xdb5a7434, 4 | BRF_GRA },	      	  //  3
};

STD_ROM_PICK(insectxj)
STD_ROM_FN(insectxj)

struct BurnDriver BurnDrvInsectxj = {
	"insectxj", "insectx", NULL, NULL, "1989",
	"Insector X (Japan)\0", NULL, "Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_HORSHOOT, 0,
	NULL, insectxjRomInfo, insectxjRomName, NULL, NULL, NULL, NULL, InsectxInputInfo, InsectxjDIPInfo,
	InsectxInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// Jumping Pop (Nics, Korean bootleg of Plump Pop)

static struct BurnRomInfo jpopnicsRomDesc[] = {
	{ "u96cpu2",		0x20000, 0x649e951c, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	{ "u124cpu1",		0x10000, 0x8453e8e4, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code

	{ "u94gfx",			0x20000, 0xe49f2fdd, 4 | BRF_GRA },	      	  //  2 Graphics
	{ "u93gfx",			0x20000, 0xa7791b5b, 4 | BRF_GRA },	      	  //  3
	{ "u92gfx",			0x20000, 0xb30caac7, 4 | BRF_GRA },	      	  //  4
	{ "u91gfx",			0x20000, 0x18ada5f2, 4 | BRF_GRA },	      	  //  5
};

STD_ROM_PICK(jpopnics)
STD_ROM_FN(jpopnics)

static INT32 JpopnicsInit()
{
	return Type1Init(MCU_NONE_JPOPNICS);
}

struct BurnDriver BurnDrvJpopnics = {
	"jpopnics", NULL, NULL, NULL, "1992",
	"Jumping Pop (Nics, Korean bootleg of Plump Pop)\0", NULL, "Nics", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_BREAKOUT, 0,
	NULL, jpopnicsRomInfo, jpopnicsRomName, NULL, NULL, NULL, NULL, JpopnicsInputInfo, JpopnicsDIPInfo,
	JpopnicsInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};
