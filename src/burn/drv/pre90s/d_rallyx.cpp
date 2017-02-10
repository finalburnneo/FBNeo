// FinalBurn Alpha driver module for Rally-X, based on the MAME driver by Nicola Salmoria.
// Emulates Rally-X variants, Jungler, Tactician, Loco-Motion & Commando (Sega)
// Oddities: Jungler has flipped-mode on by default, but it only affects sprites
//           and bullets.

#include "tiles_generic.h"
#include "z80_intf.h"
#include "namco_snd.h"
#include "timeplt_snd.h"
#include "samples.h"

static UINT8 DrvInputPort0[8]     = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvInputPort1[8]     = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvInputPort2[8]     = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvDip[2]            = {0, 0};
static UINT8 DrvInput[3]          = {0, 0, 0};
static UINT8 DrvReset             = 0;

static UINT8 *Mem                 = NULL;
static UINT8 *MemEnd              = NULL;
static UINT8 *RamStart            = NULL;
static UINT8 *RamEnd              = NULL;
static UINT8 *DrvZ80Rom1          = NULL;
static UINT8 *DrvZ80Rom2          = NULL;
static UINT8 *DrvZ80Ram1          = NULL;
static UINT8 *DrvZ80Ram1_weird    = NULL;
static UINT8 *DrvZ80Ram2          = NULL;
static UINT8 *DrvVideoRam         = NULL;
static UINT8 *DrvRadarAttrRam     = NULL;
static UINT8 *DrvPromPalette      = NULL;
static UINT8 *DrvPromLookup       = NULL;
static UINT8 *DrvPromVidLayout    = NULL;
static UINT8 *DrvPromVidTiming    = NULL;
static UINT8 *DrvChars            = NULL;
static UINT8 *DrvSprites          = NULL;
static UINT8 *DrvDots             = NULL;
static UINT8 *DrvTempRom          = NULL;
static UINT32 *DrvPalette         = NULL;

static INT16 *pAY8910Buffer[6];

static UINT8 DrvCPUFireIRQ;
static UINT8 last_sound_irq;
static UINT8 DrvCPUIRQVector;
static UINT8 xScroll;
static UINT8 yScroll;
static UINT8 DrvLastBang;
static INT32 rallyx = 0;
static INT32 junglermode = 0; // jungler, locomtn, tactician, commsega use this
static INT32 junglerinputs = 0; // jungler has different dips than the others.
static INT32 junglerflip = 0;
static INT32 locomotnmode = 0; // locomotn, tactician, etc use this.
static INT32 commsegamode = 0;

struct jungler_star {
	int x, y, color;
};

static INT32 stars_enable;
static INT32 total_stars;
static struct jungler_star j_stars[1000];

static struct BurnInputInfo DrvInputList[] =
{
	{ "Coin 1"            , BIT_DIGITAL  , DrvInputPort0 + 7, "p1 coin"   },
	{ "Start 1"           , BIT_DIGITAL  , DrvInputPort0 + 6, "p1 start"  },
	{ "Coin 2"            , BIT_DIGITAL  , DrvInputPort1 + 7, "p2 coin"   },
	{ "Start 2"           , BIT_DIGITAL  , DrvInputPort1 + 6, "p2 start"  },

	{ "Up"                , BIT_DIGITAL  , DrvInputPort0 + 5, "p1 up"     },
	{ "Down"              , BIT_DIGITAL  , DrvInputPort0 + 4, "p1 down"   },
	{ "Left"              , BIT_DIGITAL  , DrvInputPort0 + 2, "p1 left"   },
	{ "Right"             , BIT_DIGITAL  , DrvInputPort0 + 3, "p1 right"  },
	{ "Fire 1"            , BIT_DIGITAL  , DrvInputPort0 + 1, "p1 fire 1" },
	
	{ "Up (Cocktail)"     , BIT_DIGITAL  , DrvInputPort1 + 5, "p2 up"     },
	{ "Down (Cocktail)"   , BIT_DIGITAL  , DrvInputPort1 + 4, "p2 down"   },
	{ "Left (Cocktail)"   , BIT_DIGITAL  , DrvInputPort1 + 2, "p2 left"   },
	{ "Right (Cocktail)"  , BIT_DIGITAL  , DrvInputPort1 + 3, "p2 right"  },
	{ "Fire 1 (Cocktail)" , BIT_DIGITAL  , DrvInputPort1 + 1, "p2 fire 1" },

	{ "Reset"             , BIT_DIGITAL  , &DrvReset        , "reset"     },
	{ "Service"           , BIT_DIGITAL  , DrvInputPort0 + 0, "service"   },
	{ "Dip 1"             , BIT_DIPSWITCH, DrvDip + 0       , "dip"       },
	{ "Dip 2"             , BIT_DIPSWITCH, DrvDip + 1       , "dip"       },
};

STDINPUTINFO(Drv)

static struct BurnInputInfo JunglerInputList[] =
{
	{ "Coin 1"            , BIT_DIGITAL  , DrvInputPort0 + 7, "p1 coin"   },
	{ "Start 1"           , BIT_DIGITAL  , DrvInputPort1 + 7, "p1 start"  },
	{ "Coin 2"            , BIT_DIGITAL  , DrvInputPort0 + 6, "p2 coin"   },
	{ "Start 2"           , BIT_DIGITAL  , DrvInputPort1 + 6, "p2 start"  },

	{ "Up"                , BIT_DIGITAL  , DrvInputPort1 + 0, "p1 up"     },
	{ "Down"              , BIT_DIGITAL  , DrvInputPort2 + 7, "p1 down"   },
	{ "Left"              , BIT_DIGITAL  , DrvInputPort0 + 4, "p1 left"   },
	{ "Right"             , BIT_DIGITAL  , DrvInputPort0 + 5, "p1 right"  },
	{ "Fire 1"            , BIT_DIGITAL  , DrvInputPort0 + 3, "p1 fire 1" },
	
	{ "Up (Cocktail)"     , BIT_DIGITAL  , DrvInputPort0 + 0, "p2 up"     },
	{ "Down (Cocktail)"   , BIT_DIGITAL  , DrvInputPort1 + 1, "p2 down"   },
	{ "Left (Cocktail)"   , BIT_DIGITAL  , DrvInputPort1 + 5, "p2 left"   },
	{ "Right (Cocktail)"  , BIT_DIGITAL  , DrvInputPort1 + 4, "p2 right"  },
	{ "Fire 1 (Cocktail)" , BIT_DIGITAL  , DrvInputPort1 + 3, "p2 fire 1" },

	{ "Reset"             , BIT_DIGITAL  , &DrvReset        , "reset"     },
	{ "Service"           , BIT_DIGITAL  , DrvInputPort0 + 2, "service"   },
	{ "Dip 1"             , BIT_DIPSWITCH, DrvDip + 0       , "dip"       },
};

STDINPUTINFO(Jungler)

static struct BurnInputInfo LocomotnInputList[] = {
	{"P1 Coin",		    BIT_DIGITAL,	DrvInputPort0 + 7,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvInputPort1 + 7,	"p1 start"},
	{"P1 Up",		    BIT_DIGITAL,	DrvInputPort1 + 0,	"p1 up"},
	{"P1 Down",		    BIT_DIGITAL,	DrvInputPort2 + 7,	"p1 down"},
	{"P1 Left",		    BIT_DIGITAL,	DrvInputPort0 + 4,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvInputPort0 + 5,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvInputPort0 + 3,	"p1 fire 1"},

	{"P2 Coin",		    BIT_DIGITAL,	DrvInputPort0 + 6,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvInputPort1 + 6,	"p2 start"},
	{"P2 Up",		    BIT_DIGITAL,	DrvInputPort0 + 0,	"p2 up"},
	{"P2 Down",		    BIT_DIGITAL,	DrvInputPort1 + 1,	"p2 down"},
	{"P2 Left",		    BIT_DIGITAL,	DrvInputPort1 + 5,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvInputPort1 + 4,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvInputPort1 + 3,	"p2 fire 1"},

	{"Reset",		    BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		    BIT_DIGITAL,	DrvInputPort0 + 2,	"service"},
	{"Dip A",		    BIT_DIPSWITCH,	DrvDip + 0,	"dip"},
	{"Dip B",		    BIT_DIPSWITCH,	DrvDip + 1,	"dip"},
};

STDINPUTINFO(Locomotn)


static struct BurnDIPInfo LocomotnDIPList[]=
{
	{0x10, 0xff, 0xff, 0x36, NULL		},
	{0x11, 0xff, 0xff, 0xff, NULL		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x10, 0x01, 0x30, 0x30, "3"		},
	{0x10, 0x01, 0x30, 0x20, "4"		},
	{0x10, 0x01, 0x30, 0x10, "5"		},
	{0x10, 0x01, 0x30, 0x00, "255"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x10, 0x01, 0x08, 0x00, "Upright"		},
	{0x10, 0x01, 0x08, 0x08, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x10, 0x01, 0x04, 0x04, "Off"		},
	{0x10, 0x01, 0x04, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Intermissions"		},
	{0x10, 0x01, 0x02, 0x00, "Off"		},
	{0x10, 0x01, 0x02, 0x02, "On"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x10, 0x01, 0x01, 0x01, "Off"		},
	{0x10, 0x01, 0x01, 0x00, "On"		},

	{0   , 0xfe, 0   ,    16, "Coin A"		},
	{0x11, 0x01, 0x0f, 0x04, "4 Coins 1 Credits"		},
	{0x11, 0x01, 0x0f, 0x0a, "3 Coins 1 Credits"		},
	{0x11, 0x01, 0x0f, 0x01, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x0f, 0x02, "3 Coins 2 Credits"		},
	{0x11, 0x01, 0x0f, 0x08, "4 Coins 3 Credits"		},
	{0x11, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x0f, 0x0c, "3 Coins 4 Credits"		},
	{0x11, 0x01, 0x0f, 0x0e, "2 Coins 3 Credits"		},
	{0x11, 0x01, 0x0f, 0x07, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"		},
	{0x11, 0x01, 0x0f, 0x0b, "1 Coin  3 Credits"		},
	{0x11, 0x01, 0x0f, 0x03, "1 Coin  4 Credits"		},
	{0x11, 0x01, 0x0f, 0x0d, "1 Coin  5 Credits"		},
	{0x11, 0x01, 0x0f, 0x05, "1 Coin  6 Credits"		},
	{0x11, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"		},
	{0x11, 0x01, 0x0f, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    16, "Coin B"		},
	{0x11, 0x01, 0xf0, 0x40, "4 Coins 1 Credits"		},
	{0x11, 0x01, 0xf0, 0xa0, "3 Coins 1 Credits"		},
	{0x11, 0x01, 0xf0, 0x10, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0xf0, 0x20, "3 Coins 2 Credits"		},
	{0x11, 0x01, 0xf0, 0x80, "4 Coins 3 Credits"		},
	{0x11, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0xf0, 0xc0, "3 Coins 4 Credits"		},
	{0x11, 0x01, 0xf0, 0xe0, "2 Coins 3 Credits"		},
	{0x11, 0x01, 0xf0, 0x70, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"		},
	{0x11, 0x01, 0xf0, 0xb0, "1 Coin  3 Credits"		},
	{0x11, 0x01, 0xf0, 0x30, "1 Coin  4 Credits"		},
	{0x11, 0x01, 0xf0, 0xd0, "1 Coin  5 Credits"		},
	{0x11, 0x01, 0xf0, 0x50, "1 Coin  6 Credits"		},
	{0x11, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"		},
	{0x11, 0x01, 0xf0, 0x00, "No Coin B"		},
};

STDDIPINFO(Locomotn)

static struct BurnInputInfo TactcianInputList[] = {
	{"P1 Coin",		    BIT_DIGITAL,	DrvInputPort0 + 7,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvInputPort1 + 7,	"p1 start"},
	{"P1 Up",		    BIT_DIGITAL,	DrvInputPort1 + 0,	"p1 up"},
	{"P1 Down",		    BIT_DIGITAL,	DrvInputPort2 + 7,	"p1 down"},
	{"P1 Left",		    BIT_DIGITAL,	DrvInputPort0 + 4,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvInputPort0 + 5,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvInputPort0 + 3,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvInputPort0 + 1,	"p1 fire 2"},

	{"P2 Coin",		    BIT_DIGITAL,	DrvInputPort0 + 6,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvInputPort1 + 6,	"p2 start"},
	{"P2 Up",		    BIT_DIGITAL,	DrvInputPort0 + 0,	"p2 up"},
	{"P2 Down",		    BIT_DIGITAL,	DrvInputPort1 + 1,	"p2 down"},
	{"P2 Left",		    BIT_DIGITAL,	DrvInputPort1 + 5,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvInputPort1 + 4,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvInputPort1 + 3,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvInputPort1 + 2,	"p2 fire 2"},

	{"Reset",		    BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		    BIT_DIGITAL,	DrvInputPort0 + 2,	"service"},
	{"Dip A",		    BIT_DIPSWITCH,	DrvDip + 0,	"dip"},
	{"Dip B",		    BIT_DIPSWITCH,	DrvDip + 1,	"dip"},
};

STDINPUTINFO(Tactcian)


static struct BurnDIPInfo TactcianDIPList[]=
{
	{0x12, 0xff, 0xff, 0x00, NULL		},
	{0x13, 0xff, 0xff, 0x00, NULL		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x12, 0x01, 0x30, 0x00, "3"		},
	{0x12, 0x01, 0x30, 0x10, "4"		},
	{0x12, 0x01, 0x30, 0x20, "5"		},
	{0x12, 0x01, 0x30, 0x30, "255"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x12, 0x01, 0x08, 0x00, "Upright"		},
	{0x12, 0x01, 0x08, 0x08, "Cocktail"		},

	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{0x12, 0x01, 0x06, 0x06, "4 Coins 1 Credits"		},
	{0x12, 0x01, 0x06, 0x02, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0x06, 0x00, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0x06, 0x04, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{0x12, 0x01, 0x06, 0x02, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0x06, 0x04, "A 2C/1C  B 1C/3C"		},
	{0x12, 0x01, 0x06, 0x00, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0x06, 0x06, "A 1C/1C  B 1C/6C"		},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x12, 0x01, 0x01, 0x00, "10k, 80k then every 100k"		},
	{0x12, 0x01, 0x01, 0x01, "20k, 80k then every 100k"		},

	{0   , 0xfe, 0   ,    2, "Coin Mode"		},
	{0x13, 0x01, 0x01, 0x00, "Mode 1"		},
	{0x13, 0x01, 0x01, 0x01, "Mode 2"		},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x13, 0x01, 0x02, 0x00, "Off"		},
	{0x13, 0x01, 0x02, 0x02, "On"		},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x13, 0x01, 0x04, 0x00, "Off"		},
	{0x13, 0x01, 0x04, 0x04, "On"		},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x13, 0x01, 0x08, 0x00, "Off"		},
	{0x13, 0x01, 0x08, 0x08, "On"		},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x13, 0x01, 0x10, 0x00, "Off"		},
	{0x13, 0x01, 0x10, 0x10, "On"		},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x13, 0x01, 0x20, 0x00, "Off"		},
	{0x13, 0x01, 0x20, 0x20, "On"		},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x13, 0x01, 0x40, 0x00, "Off"		},
	{0x13, 0x01, 0x40, 0x40, "On"		},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x13, 0x01, 0x80, 0x00, "Off"		},
	{0x13, 0x01, 0x80, 0x80, "On"		},
};

STDDIPINFO(Tactcian)

static struct BurnInputInfo CommsegaInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvInputPort0 + 7,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvInputPort1 + 7,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvInputPort1 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvInputPort2 + 7,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvInputPort0 + 4,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvInputPort0 + 5,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvInputPort2 + 6,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvInputPort0 + 3,	"p1 fire 2"},

	{"P2 Coin",		BIT_DIGITAL,	DrvInputPort0 + 6,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvInputPort1 + 6,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvInputPort0 + 0,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvInputPort1 + 1,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvInputPort1 + 5,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvInputPort1 + 4,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvInputPort1 + 2,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvInputPort1 + 3,	"p2 fire 2"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDip + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDip + 1,	"dip"},
};

STDINPUTINFO(Commsega)


static struct BurnDIPInfo CommsegaDIPList[]=
{
	{0x11, 0xff, 0xff, 0x3f, NULL		},
	{0x12, 0xff, 0xff, 0x7f, NULL		},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x11, 0x01, 0x20, 0x20, "Off"		},
	{0x11, 0x01, 0x20, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x11, 0x01, 0x10, 0x10, "Off"		},
	{0x11, 0x01, 0x10, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x11, 0x01, 0x08, 0x08, "Off"		},
	{0x11, 0x01, 0x08, 0x00, "On"		},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x11, 0x01, 0x07, 0x04, "4 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x05, "3 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x06, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x07, 0x01, "2 Coins 3 Credits"		},
	{0x11, 0x01, 0x07, 0x03, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0x07, 0x02, "1 Coin  3 Credits"		},
	{0x11, 0x01, 0x07, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x12, 0x01, 0x03, 0x03, "3"		},
	{0x12, 0x01, 0x03, 0x02, "4"		},
	{0x12, 0x01, 0x03, 0x01, "5"		},
	{0x12, 0x01, 0x03, 0x00, "6"		},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x12, 0x01, 0x1c, 0x10, "4 Coins 1 Credits"		},
	{0x12, 0x01, 0x1c, 0x14, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0x1c, 0x18, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0x1c, 0x1c, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0x1c, 0x04, "2 Coins 3 Credits"		},
	{0x12, 0x01, 0x1c, 0x0c, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0x1c, 0x08, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0x1c, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x12, 0x01, 0x20, 0x20, "Off"		},
	{0x12, 0x01, 0x20, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x12, 0x01, 0x40, 0x40, "Easy"		},
	{0x12, 0x01, 0x40, 0x00, "Hard"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x12, 0x01, 0x80, 0x00, "Upright"		},
	{0x12, 0x01, 0x80, 0x80, "Cocktail"		},
};

STDDIPINFO(Commsega)

static inline void DrvMakeInputs()
{
	// Reset Inputs
	DrvInput[0] = 0xff;
	DrvInput[1] = 0xfe;

	// Compile Digital Inputs
	for (INT32 i = 0; i < 8; i++) {
		DrvInput[0] ^= (DrvInputPort0[i] & 1) << i;
		DrvInput[1] ^= (DrvInputPort1[i] & 1) << i;
	}
}

static inline void JunglerMakeInputs()
{
	// Reset Inputs
	DrvInput[0] = 0xff;
	DrvInput[1] = 0xff;
	DrvInput[2] = 0xff;

	// Compile Digital Inputs
	for (INT32 i = 0; i < 8; i++) {
		DrvInput[0] ^= (DrvInputPort0[i] & 1) << i;
		DrvInput[1] ^= (DrvInputPort1[i] & 1) << i;
		DrvInput[2] ^= (DrvInputPort2[i] & 1) << i;
	}
}

static struct BurnDIPInfo DrvDIPList[]=
{
	// Default Values
	{0x10, 0xff, 0xff, 0x01, NULL                     },
	{0x11, 0xff, 0xff, 0xcb, NULL                     },
	
	// Dip 1
	{0   , 0xfe, 0   , 2   , "Cabinet"                },
	{0x10, 0x01, 0x01, 0x00, "Cocktail"               },
	{0x10, 0x01, 0x01, 0x01, "Upright"                },
	
	// Dip 2
	{0   , 0xfe, 0   , 2   , "Service Mode"           },
	{0x11, 0x01, 0x01, 0x01, "Off"                    },
	{0x11, 0x01, 0x01, 0x00, "On"                     },
	
	{0   , 0xfe, 0   , 4   , "Bonus Life"             }, // This is conditional on difficulty
	{0x11, 0x01, 0x06, 0x02, "15000"                  },
	{0x11, 0x01, 0x06, 0x04, "30000"                  },
	{0x11, 0x01, 0x06, 0x06, "40000"                  },
	{0x11, 0x01, 0x06, 0x00, "None"                   },
	
	{0   , 0xfe, 0   , 8   , "Difficulty"             },
	{0x11, 0x01, 0x38, 0x10, "1 Car,  Medium"         },
	{0x11, 0x01, 0x38, 0x28, "1 Car,  Hard"           },
	{0x11, 0x01, 0x38, 0x00, "2 Cars, Easy"           },
	{0x11, 0x01, 0x38, 0x18, "2 Cars, Medium"         },
	{0x11, 0x01, 0x38, 0x30, "2 Cars, Hard"           },
	{0x11, 0x01, 0x38, 0x08, "3 Cars, Easy"           },
	{0x11, 0x01, 0x38, 0x20, "3 Cars, Medium"         },
	{0x11, 0x01, 0x38, 0x38, "3 Cars, Hard"           },
	
	{0   , 0xfe, 0   , 4   , "Coinage"                },
	{0x11, 0x01, 0xc0, 0x40, "2 Coins 1 Play"         },
	{0x11, 0x01, 0xc0, 0xc0, "1 Coin  1 Play"         },
	{0x11, 0x01, 0xc0, 0x80, "1 Coin  2 Plays"        },
	{0x11, 0x01, 0xc0, 0x00, "Freeplay"               },
};

STDDIPINFO(Drv)

static struct BurnDIPInfo JunglerDIPList[]=
{
	// Default Values
	{0x10, 0xff, 0xff, 0xbf, NULL                     },
	
	// Dip 1
	{0   , 0xfe, 0   , 8   , "Coin A"                 },
	{0x10, 0x01, 0x07, 0x01, "4 Coins 1 Play"         },
	{0x10, 0x01, 0x07, 0x02, "3 Coins 1 Play"         },
	{0x10, 0x01, 0x07, 0x03, "2 Coins 1 Play"         },
	{0x10, 0x01, 0x07, 0x00, "4 Coins 3 Plays"        },
	{0x10, 0x01, 0x07, 0x07, "1 Coin  1 Play"         },
	{0x10, 0x01, 0x07, 0x06, "1 Coin  2 Plays"        },
	{0x10, 0x01, 0x07, 0x05, "1 Coin  3 Plays"        },
	{0x10, 0x01, 0x07, 0x04, "1 Coin  4 Plays"        },
	
	{0   , 0xfe, 0   , 8   , "Coin B"                 },
	{0x10, 0x01, 0x38, 0x08, "4 Coins 1 Play"         },
	{0x10, 0x01, 0x38, 0x10, "3 Coins 1 Play"         },
	{0x10, 0x01, 0x38, 0x18, "2 Coins 1 Play"         },
	{0x10, 0x01, 0x38, 0x00, "4 Coins 3 Plays"        },
	{0x10, 0x01, 0x38, 0x38, "1 Coin  1 Play"         },
	{0x10, 0x01, 0x38, 0x30, "1 Coin  2 Plays"        },
	{0x10, 0x01, 0x38, 0x28, "1 Coin  3 Plays"        },
	{0x10, 0x01, 0x38, 0x20, "1 Coin  4 Plays"        },
		
	{0   , 0xfe, 0   , 2   , "Cabinet"                },
	{0x10, 0x01, 0x40, 0x40, "Cocktail"               },
	{0x10, 0x01, 0x40, 0x00, "Upright"                },
	
	{0   , 0xfe, 0   , 2   , "Test Mode (255 lives)"  },
	{0x10, 0x01, 0x80, 0x80, "Off"                    },
	{0x10, 0x01, 0x80, 0x00, "On"                     },	
};

STDDIPINFO(Jungler)

static struct BurnRomInfo RallyxRomDesc[] = {
	{ "1b",            0x01000, 0x5882700d, BRF_ESS | BRF_PRG }, //  0	Z80 Program Code
	{ "rallyxn.1e",    0x01000, 0xed1eba2b, BRF_ESS | BRF_PRG }, //	 1
	{ "rallyxn.1h",    0x01000, 0x4f98dd1c, BRF_ESS | BRF_PRG }, //	 2
	{ "rallyxn.1k",    0x01000, 0x9aacccf0, BRF_ESS | BRF_PRG }, //	 3
	
	{ "8e",            0x01000, 0x277c1de5, BRF_GRA },	     //  4	Characters & Sprites
	
	{ "rx1-6.8m",      0x00100, 0x3c16f62c, BRF_GRA },	     //  5	Dots
	
	{ "rx1-1.11n",     0x00020, 0xc7865434, BRF_GRA },	     //  6	Palette PROM
	{ "rx1-7.8p",      0x00100, 0x834d4fda, BRF_GRA },	     //  7	Lookup PROM
	{ "rx1-2.4n",      0x00020, 0x8f574815, BRF_GRA },	     //  8	Video Layout PROM
	{ "rx1-3.7k",      0x00020, 0xb8861096, BRF_GRA },	     //  9	Video Timing PROM
	
	{ "rx1-5.3p",      0x00100, 0x4bad7017, BRF_SND },	     //  10	Sound PROMs
	{ "rx1-4.2m",      0x00100, 0x77245b66, BRF_SND },	     //  11
};

STD_ROM_PICK(Rallyx)
STD_ROM_FN(Rallyx)

static struct BurnRomInfo RallyxaRomDesc[] = {
	{ "rx1_prg_1.1b",  0x00800, 0xef9238db, BRF_ESS | BRF_PRG }, //  0	Z80 Program Code
	{ "rx1_prg_2.1c",  0x00800, 0x7cbeb656, BRF_ESS | BRF_PRG }, //	 1
	{ "rx1_prg_3.1d",  0x00800, 0x334b1042, BRF_ESS | BRF_PRG }, //	 2
	{ "rx1_prg_4.1e",  0x00800, 0xd6618add, BRF_ESS | BRF_PRG }, //	 3
	{ "rx1_prg_5.bin", 0x00800, 0x3d69f24e, BRF_ESS | BRF_PRG }, //	 4
	{ "rx1_prg_6.bin", 0x00800, 0xe9740f16, BRF_ESS | BRF_PRG }, //	 5
	{ "rx1_prg_7.1k",  0x00800, 0x843109f2, BRF_ESS | BRF_PRG }, //	 6
	{ "rx1_prg_8.1l",  0x00800, 0x9b846ec9, BRF_ESS | BRF_PRG }, //	 7
	
	{ "rx1_chg_1.8e",  0x00800, 0x1fff38a4, BRF_GRA },	     //  8	Characters & Sprites
	{ "rx1_chg_2.8d",  0x00800, 0x68dff552, BRF_GRA },	     //  9
	
	{ "rx1-6.8m",      0x00100, 0x3c16f62c, BRF_GRA },	     //  10	Dots
	
	{ "rx1-1.11n",     0x00020, 0xc7865434, BRF_GRA },	     //  11	Palette PROM
	{ "rx1-7.8p",      0x00100, 0x834d4fda, BRF_GRA },	     //  12	Lookup PROM
	{ "rx1-2.4n",      0x00020, 0x8f574815, BRF_GRA },	     //  13	Video Layout PROM
	{ "rx1-3.7k",      0x00020, 0xb8861096, BRF_GRA },	     //  14	Video Timing PROM
	
	{ "rx1-5.3p",      0x00100, 0x4bad7017, BRF_SND },	     //  15	Sound PROMs
	{ "rx1-4.2m",      0x00100, 0x77245b66, BRF_SND },	     //  16
};

STD_ROM_PICK(Rallyxa)
STD_ROM_FN(Rallyxa)

static struct BurnRomInfo RallyxmRomDesc[] = {
	{ "1b",            0x01000, 0x5882700d, BRF_ESS | BRF_PRG }, //  0	Z80 Program Code
	{ "1e",            0x01000, 0x786585ec, BRF_ESS | BRF_PRG }, //	 1
	{ "1h",            0x01000, 0x110d7dcd, BRF_ESS | BRF_PRG }, //	 2
	{ "1k",            0x01000, 0x473ab447, BRF_ESS | BRF_PRG }, //	 3
	
	{ "8e",            0x01000, 0x277c1de5, BRF_GRA },	     //  4	Characters & Sprites
	
	{ "rx1-6.8m",      0x00100, 0x3c16f62c, BRF_GRA },	     //  5	Dots
	
	{ "rx1-1.11n",     0x00020, 0xc7865434, BRF_GRA },	     //  6	Palette PROM
	{ "rx1-7.8p",      0x00100, 0x834d4fda, BRF_GRA },	     //  7	Lookup PROM
	{ "rx1-2.4n",      0x00020, 0x8f574815, BRF_GRA },	     //  8	Video Layout PROM
	{ "rx1-3.7k",      0x00020, 0xb8861096, BRF_GRA },	     //  9	Video Timing PROM
	
	{ "rx1-5.3p",      0x00100, 0x4bad7017, BRF_SND },	     //  10	Sound PROMs
	{ "rx1-4.2m",      0x00100, 0x77245b66, BRF_SND },	     //  11
};

STD_ROM_PICK(Rallyxm)
STD_ROM_FN(Rallyxm)

static struct BurnRomInfo RallyxmrRomDesc[] = {
	{ "166.bin",  	   0x00800, 0xef9238db, BRF_ESS | BRF_PRG }, //  0	Z80 Program Code
	{ "167.bin",  	   0x00800, 0x7cbeb656, BRF_ESS | BRF_PRG }, //	 1
	{ "168.bin",  	   0x00800, 0x334b1042, BRF_ESS | BRF_PRG }, //	 2
	{ "169.bin",  	   0x00800, 0xb4852b52, BRF_ESS | BRF_PRG }, //	 3
	{ "170.bin", 	   0x00800, 0x3d69f24e, BRF_ESS | BRF_PRG }, //	 4
	{ "171.bin", 	   0x00800, 0xe9740f16, BRF_ESS | BRF_PRG }, //	 5
	{ "172.bin",  	   0x00800, 0x843109f2, BRF_ESS | BRF_PRG }, //	 6
	{ "173.bin",  	   0x00800, 0x3b5b1a81, BRF_ESS | BRF_PRG }, //	 7
	
	{ "175.bin",  	   0x00800, 0x50a224e2, BRF_GRA },	     //  8	Characters & Sprites
	{ "174.bin",  	   0x00800, 0x68dff552, BRF_GRA },	     //  9
	
	{ "rx1-6.8m",      0x00100, 0x3c16f62c, BRF_GRA },	     //  10	Dots
	
	{ "rx1-1.11n",     0x00020, 0xc7865434, BRF_GRA },	     //  11	Palette PROM
	{ "rx1-7.8p",      0x00100, 0x834d4fda, BRF_GRA },	     //  12	Lookup PROM
	{ "rx1-2.4n",      0x00020, 0x8f574815, BRF_GRA },	     //  13	Video Layout PROM
	{ "rx1-3.7k",      0x00020, 0xb8861096, BRF_GRA },	     //  14	Video Timing PROM
	
	{ "rx1-5.3p",      0x00100, 0x4bad7017, BRF_SND },	     //  15	Sound PROMs
	{ "rx1-4.2m",      0x00100, 0x77245b66, BRF_SND },	     //  16
};

STD_ROM_PICK(Rallyxmr)
STD_ROM_FN(Rallyxmr)

static struct BurnRomInfo NrallyxRomDesc[] = {
	{ "nrx_prg1.1d",   0x01000, 0xba7de9fc, BRF_ESS | BRF_PRG }, //  0	Z80 Program Code
	{ "nrx_prg2.1e",   0x01000, 0xeedfccae, BRF_ESS | BRF_PRG }, //	 1
	{ "nrx_prg3.1k",   0x01000, 0xb4d5d34a, BRF_ESS | BRF_PRG }, //	 2
	{ "nrx_prg4.1l",   0x01000, 0x7da5496d, BRF_ESS | BRF_PRG }, //	 3
	
	{ "nrx_chg1.8e",   0x00800, 0x1fff38a4, BRF_GRA },	     //  4	Characters & Sprites
	{ "nrx_chg2.8d",   0x00800, 0x85d9fffd, BRF_GRA },	     //  5
	
	{ "rx1-6.8m",      0x00100, 0x3c16f62c, BRF_GRA },	     //  6	Dots
	
	{ "nrx1-1.11n",    0x00020, 0xa0a49017, BRF_GRA },	     //  7	Palette PROM
	{ "nrx1-7.8p",     0x00100, 0x4e46f485, BRF_GRA },	     //  8	Lookup PROM
	{ "rx1-2.4n",      0x00020, 0x8f574815, BRF_GRA },	     //  9	Video Layout PROM
	{ "rx1-3.7k",      0x00020, 0xb8861096, BRF_GRA },	     //  10	Video Timing PROM
	
	{ "rx1-5.3p",      0x00100, 0x4bad7017, BRF_SND },	     //  11	Sound PROMs
	{ "rx1-4.2m",      0x00100, 0x77245b66, BRF_SND },	     //  12
};

STD_ROM_PICK(Nrallyx)
STD_ROM_FN(Nrallyx)

static struct BurnRomInfo JunglerRomDesc[] = {
	{ "jungr1",        0x01000, 0x5bd6ad15, BRF_ESS | BRF_PRG }, //  0	Z80 Program Code #1
	{ "jungr2",        0x01000, 0xdc99f1e3, BRF_ESS | BRF_PRG }, //  1
	{ "jungr3",        0x01000, 0x3dcc03da, BRF_ESS | BRF_PRG }, //  2
	{ "jungr4",        0x01000, 0xf92e9940, BRF_ESS | BRF_PRG }, //  3
	
	{ "1b",            0x01000, 0xf86999c3, BRF_ESS | BRF_PRG }, //  4	Z80 Program Code #2
	
	{ "5k",            0x00800, 0x924262bf, BRF_GRA },	     //  5	Characters & Sprites
	{ "5m",            0x00800, 0x131a08ac, BRF_GRA },	     //  6
	
	{ "82s129.10g",    0x00100, 0xc59c51b7, BRF_GRA },	     //  7	Dots
	
	{ "18s030.8b",     0x00020, 0x55a7e6d1, BRF_GRA },	     //  8	Palette PROM
	{ "tbp24s10.9d",   0x00100, 0xd223f7b8, BRF_GRA },	     //  9	Lookup PROM
	{ "18s030.7a",     0x00020, 0x8f574815, BRF_GRA },	     //  10	Video Layout PROM
	{ "6331-1.10a",    0x00020, 0xb8861096, BRF_GRA },	     //  11	Video Timing PROM
};

STD_ROM_PICK(Jungler)
STD_ROM_FN(Jungler)

static struct BurnSampleInfo RallyxSampleDesc[] = {
	{ "bang", SAMPLE_NOLOOP },
   { "", 0 }
};

STD_SAMPLE_PICK(Rallyx)
STD_SAMPLE_FN(Rallyx)

static INT32 MemIndex()
{
	UINT8 *Next; Next = Mem;

	DrvZ80Rom1             = Next; Next += 0x04000;	
	DrvPromPalette         = Next; Next += 0x00020;
	DrvPromLookup          = Next; Next += 0x00100;
	DrvPromVidLayout       = Next; Next += 0x00020;
	DrvPromVidTiming       = Next; Next += 0x00020;
	NamcoSoundProm         = Next; Next += 0x00100;
	
	RamStart               = Next;

	DrvZ80Ram1             = Next; Next += 0x00800;	
	DrvVideoRam            = Next; Next += 0x01000;
	DrvRadarAttrRam        = Next; Next += 0x00010;
	
	RamEnd                 = Next;

	DrvChars               = Next; Next += 0x100 * 8 * 8;
	DrvSprites             = Next; Next += 0x040 * 16 * 16;
	DrvDots                = Next; Next += 0x008 * 4 * 4;
	DrvPalette             = (UINT32*)Next; Next += 260 * sizeof(UINT32);

	MemEnd                 = Next;

	return 0;
}

static INT32 JunglerMemIndex()
{
	UINT8 *Next; Next = Mem;

	DrvZ80Rom1             = Next; Next += 0x08000;
	DrvZ80Rom2             = Next; Next += 0x02000;
	DrvPromPalette         = Next; Next += 0x00020;
	DrvPromLookup          = Next; Next += 0x00100;
	DrvPromVidLayout       = Next; Next += 0x00020;
	DrvPromVidTiming       = Next; Next += 0x00020;
	
	RamStart               = Next;

	DrvZ80Ram1             = Next; Next += 0x00800;	
	DrvZ80Ram1_weird       = Next; Next += 0x00800;
	DrvZ80Ram2             = Next; Next += 0x00400;	
	DrvVideoRam            = Next; Next += 0x01000;
	DrvRadarAttrRam        = Next; Next += 0x00010;
	
	RamEnd                 = Next;

	DrvChars               = Next; Next += 0x400 * 8 * 8;
	DrvSprites             = Next; Next += 0x180 * 16 * 16;
	DrvDots                = Next; Next += 0x018 * 4 * 4;
	DrvPalette             = (UINT32*)Next; Next += 324 * sizeof(UINT32);

	pAY8910Buffer[0]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[1]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[2]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[3]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[4]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[5]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);

	MemEnd                 = Next;

	return 0;
}

static INT32 DrvDoReset()
{
	memset (RamStart, 0, RamEnd - RamStart);
	ZetOpen(0);
	ZetReset();
	ZetClose();
	
	BurnSampleReset();

	NamcoSoundReset();

	DrvCPUFireIRQ = 0;
	DrvCPUIRQVector = 0;
	xScroll = 0;
	yScroll = 0;
	DrvLastBang = 0;

	HiscoreReset();

	return 0;
}

void calculate_star_field(); // forward..

static INT32 JunglerDoReset()
{
	memset (RamStart, 0, RamEnd - RamStart);
	ZetOpen(0);
	ZetReset();
	ZetClose();
	
	DrvCPUFireIRQ = 0;
	last_sound_irq = 0;
	DrvCPUIRQVector = 0;
	xScroll = 0;
	yScroll = 0;
	junglerflip = 0;
	stars_enable = 0;

	calculate_star_field();

	HiscoreReset();
	TimepltSndReset();

	return 0;
}

UINT8 __fastcall RallyxZ80ProgRead(UINT16 a)
{
	switch (a) {
		case 0xa000: {
			return DrvInput[0];
		}
		
		case 0xa080: {
			return DrvInput[1] | DrvDip[0];
		}
		
		case 0xa100: {
			return DrvDip[1];
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 Read %04x\n"), a);
		}
	}
	
	return 0;
}

void __fastcall RallyxZ80ProgWrite(UINT16 a, UINT8 d)
{
	if (a >= 0xa100 && a <= 0xa11f) {
		NamcoSoundWrite(a - 0xa100, d);
		return;
	}

	switch (a) {
		case 0xa000:
		case 0xa001:
		case 0xa002:
		case 0xa003:
		case 0xa004:
		case 0xa005:
		case 0xa006:
		case 0xa007:
		case 0xa008:
		case 0xa009:
		case 0xa00a:
		case 0xa00b:
		case 0xa00c:
		case 0xa00d:
		case 0xa00e:
		case 0xa00f: {
			DrvRadarAttrRam[a & 0xf] = d;
			return;
		}

		case 0xa080: {
			// watchdog write
			return;
		}
		
		case 0xa130: {
			xScroll = d;
			return;
		}
		
		case 0xa140: {
			yScroll = d;
			return;
		}
		
		case 0xa170: {
			// NOP
			return;
		}
		
		case 0xa180: {
			UINT8 Bit = d & 0x01;
			if (Bit == 0 && DrvLastBang != 0) {
				BurnSamplePlay(0);
			}
			DrvLastBang = Bit;
			return;
		}
		
		case 0xa181: {
			DrvCPUFireIRQ = d & 0x01;
			if (!DrvCPUFireIRQ) ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			return;
		}
		
		case 0xa182: {
			// sound on
			return;
		}
		
		case 0xa183: {
			//bprintf(0, _T("Flipmode %X\n"), d);
			return;
		}
		
		case 0xa184: {
			// lamp
			return;
		}
		
		case 0xa185: {
			// lamp
			return;
		}
		
		case 0xa186: {
			// coin lockout
			return;
		}
		
		case 0xa187: {
			// coin counter
			return;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 Write %04x, %02x\n"), a, d);
		}
	}
}

UINT8 __fastcall RallyxZ80PortRead(UINT16 a)
{
	a &= 0xff;
	
	switch (a) {
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 Port Read => %02X\n"), a);
		}
	}

	return 0;
}

void __fastcall RallyxZ80PortWrite(UINT16 a, UINT8 d)
{
	a &= 0xff;
	
	switch (a) {
		case 0x00: {
			DrvCPUIRQVector = d;
			ZetSetVector(d);
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			return;
		}
				
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 Port Write => %02X, %02X\n"), a, d);
		}
	}
}

UINT8 __fastcall JunglerZ80ProgRead1(UINT16 a)
{
	switch (a) {
		case 0xa000: {
			return DrvInput[0];
		}
		
		case 0xa080: {
			return DrvInput[1];
		}
		
		case 0xa100: {
			if (junglerinputs)
				return DrvInput[2];

			return (DrvInput[2] & 0xc0) | DrvDip[0];
		}
		
		case 0xa180: {
			if (junglerinputs)
				return DrvDip[0];

			return DrvDip[1];
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 #1 Read %04x\n"), a);
		}
	}
	
	return 0;
}

void __fastcall JunglerZ80ProgWrite1(UINT16 a, UINT8 d)
{
	switch (a) {
		case 0xa100: {
			TimepltSndSoundlatch(d);
			return;
		}
		case 0xa030:
		case 0xa031:
		case 0xa032:
		case 0xa033:
		case 0xa034:
		case 0xa035:
		case 0xa036:
		case 0xa037:
		case 0xa038:
		case 0xa039:
		case 0xa03a:
		case 0xa03b:
		case 0xa03c:
		case 0xa03d:
		case 0xa03e:
		case 0xa03f:
		case 0xa000:
		case 0xa001:
		case 0xa002:
		case 0xa003:
		case 0xa004:
		case 0xa005:
		case 0xa006:
		case 0xa007:
		case 0xa008:
		case 0xa009:
		case 0xa00a:
		case 0xa00b:
		case 0xa00c:
		case 0xa00d:
		case 0xa00e:
		case 0xa00f: {
			DrvRadarAttrRam[a & 0xf] = d;
			return;
		}

		case 0xa080: {
			// watchdog write
			return;
		}
		
		case 0xa130: {
			xScroll = d;
			return;
		}
		
		case 0xa140: {
			yScroll = d;
			return;
		}
		
		case 0xa180: {
			if (last_sound_irq == 0 && d) {
				ZetClose();
				ZetOpen(1);
				ZetSetVector(0xff);
				ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
				ZetClose();
				ZetOpen(0);
			}
			last_sound_irq = d;
			return;
		}

		case 0xa181: {
			DrvCPUFireIRQ = d & 0x01;
			return;
		}

		case 0xa183: {
			//bprintf(0, _T("Flipmode %X\n"), d);
			junglerflip = d;
			return;
		}

		case 0xa182:
		case 0xa184:
		case 0xa186: { // nop for now
			return;
		}

		case 0xa187: {
			stars_enable = d & 1;
			return;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 #1 Write %04x, %02x\n"), a, d);
		}
	}
}

UINT8 __fastcall JunglerZ80PortRead1(UINT16 a)
{
	a &= 0xff;
	
	switch (a) {
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 #1 Port Read => %02X\n"), a);
		}
	}

	return 0;
}

void __fastcall JunglerZ80PortWrite1(UINT16 a, UINT8 d)
{
	a &= 0xff;
	
	switch (a) {
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 #1 Port Write => %02X, %02X\n"), a, d);
		}
	}
}

UINT8 __fastcall JunglerZ80ProgRead2(UINT16 a)
{
	switch (a) {
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 #2 Read %04x\n"), a);
		}
	}
	
	return 0;
}

void __fastcall JunglerZ80ProgWrite2(UINT16 a, UINT8 d)
{
	switch (a) {
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 #2 Write %04x, %02x\n"), a, d);
		}
	}
}

UINT8 __fastcall JunglerZ80PortRead2(UINT16 a)
{
	a &= 0xff;
	
	switch (a) {
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 #2 Port Read => %02X\n"), a);
		}
	}

	return 0;
}

void __fastcall JunglerZ80PortWrite2(UINT16 a, UINT8 d)
{
	a &= 0xff;
	
	switch (a) {
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 #2 Port Write => %02X, %02X\n"), a, d);
		}
	}
}

static INT32 CharPlaneOffsets[2]          = { 0, 4 };
static INT32 CharXOffsets[8]              = { 64, 65, 66, 67, 0, 1, 2, 3 };
static INT32 CharYOffsets[8]              = { 0, 8, 16, 24, 32, 40, 48, 56 };
static INT32 JunglerCharPlaneOffsets[2]   = { 4, 0 };
static INT32 SpritePlaneOffsets[2]        = { 0, 4 };
static INT32 SpriteXOffsets[16]           = { 64, 65, 66, 67, 128, 129, 130, 131, 192, 193, 194, 195, 0, 1, 2, 3 };
static INT32 SpriteYOffsets[16]           = { 0, 8, 16, 24, 32, 40, 48, 56, 256, 264, 272, 280, 288, 296, 304, 312 };
static INT32 JunglerSpritePlaneOffsets[2] = { 4, 0 };
static INT32 JunglerSpriteXOffsets[16]    = { 64, 65, 66, 67, 0, 1, 2, 3, 192, 193, 194, 195, 128, 129, 130, 131 };
static INT32 DotPlaneOffsets[2]           = { 6, 7 };
static INT32 DotXOffsets[4]               = { 0, 8, 16, 24 };
static INT32 DotYOffsets[4]               = { 0, 32, 64, 96 };

static void MachineInit()
{
	ZetInit(0);
	ZetOpen(0);
	ZetSetReadHandler(RallyxZ80ProgRead);
	ZetSetWriteHandler(RallyxZ80ProgWrite);
	ZetSetInHandler(RallyxZ80PortRead);
	ZetSetOutHandler(RallyxZ80PortWrite);
	ZetMapMemory(DrvZ80Rom1      , 0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvVideoRam     , 0x8000, 0x8fff, MAP_RAM);
	ZetMapMemory(DrvZ80Ram1      , 0x9800, 0x9fff, MAP_RAM);
	ZetClose();
	
	NamcoSoundInit(18432000 / 6 / 32, 3, 0);
	NacmoSoundSetAllRoutes(1.00, BURN_SND_ROUTE_BOTH);
	BurnSampleInit(1);
	BurnSampleSetAllRoutesAllSamples(0.80, BURN_SND_ROUTE_BOTH);
	
	GenericTilesInit();

	DrvDoReset();
}

static void JunglerMachineInit()
{
	ZetInit(0);
	ZetOpen(0);
	ZetSetReadHandler(JunglerZ80ProgRead1);
	ZetSetWriteHandler(JunglerZ80ProgWrite1);
	ZetSetInHandler(JunglerZ80PortRead1);
	ZetSetOutHandler(JunglerZ80PortWrite1);
	ZetMapMemory(DrvZ80Rom1      , 0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvVideoRam     , 0x8000, 0x8fff, MAP_RAM);
	ZetMapMemory(DrvZ80Ram1_weird, 0x9000, 0x93ff, MAP_RAM); // checked at boot
	ZetMapMemory(DrvZ80Ram1      , 0x9800, 0x9fff, MAP_RAM);
	ZetClose();

	LocomotnSndInit(DrvZ80Rom2, DrvZ80Ram2, 1);
	TimepltSndVol(0.55, 0.55);

	GenericTilesInit();

	JunglerDoReset();
}

static INT32 DrvInit()
{
	INT32 nRet = 0, nLen;

	// Allocate and Blank all required memory
	Mem = NULL;
	MemIndex();
	nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(Mem, 0, nLen);
	MemIndex();

	DrvTempRom = (UINT8 *)BurnMalloc(0x01000);

	// Load Z80 Program Roms
	nRet = BurnLoadRom(DrvZ80Rom1 + 0x00000,  0, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvZ80Rom1 + 0x01000,  1, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvZ80Rom1 + 0x02000,  2, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvZ80Rom1 + 0x03000,  3, 1); if (nRet != 0) return 1;
	
	// Load and decode the chars and sprites
	nRet = BurnLoadRom(DrvTempRom,            4, 1); if (nRet != 0) return 1;
	GfxDecode(0x100, 2, 8, 8, CharPlaneOffsets, CharXOffsets, CharYOffsets, 0x80, DrvTempRom, DrvChars);
	GfxDecode(0x40, 2, 16, 16, SpritePlaneOffsets, SpriteXOffsets, SpriteYOffsets, 0x200, DrvTempRom, DrvSprites);
	
	// Load and decode the dots
	memset(DrvTempRom, 0, 0x1000);
	nRet = BurnLoadRom(DrvTempRom,            5, 1); if (nRet != 0) return 1;
	GfxDecode(0x08, 2, 4, 4, DotPlaneOffsets, DotXOffsets, DotYOffsets, 0x80, DrvTempRom, DrvDots);

	// Load the PROMs
	nRet = BurnLoadRom(DrvPromPalette,        6, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvPromLookup,         7, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvPromVidLayout,      8, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvPromVidTiming,      9, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(NamcoSoundProm,       10, 1); if (nRet != 0) return 1;
	
	BurnFree(DrvTempRom);

	rallyx = 1;

	MachineInit();

	return 0;
}

static INT32 DrvaInit()
{
	INT32 nRet = 0, nLen;

	// Allocate and Blank all required memory
	Mem = NULL;
	MemIndex();
	nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(Mem, 0, nLen);
	MemIndex();

	DrvTempRom = (UINT8 *)BurnMalloc(0x01000);

	// Load Z80 Program Roms
	nRet = BurnLoadRom(DrvZ80Rom1 + 0x00000,  0, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvZ80Rom1 + 0x00800,  1, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvZ80Rom1 + 0x01000,  2, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvZ80Rom1 + 0x01800,  3, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvZ80Rom1 + 0x02000,  4, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvZ80Rom1 + 0x02800,  5, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvZ80Rom1 + 0x03000,  6, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvZ80Rom1 + 0x03800,  7, 1); if (nRet != 0) return 1;
	
	// Load and decode the chars and sprites
	nRet = BurnLoadRom(DrvTempRom + 0x0000,   8, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x0800,   9, 1); if (nRet != 0) return 1;
	GfxDecode(0x100, 2, 8, 8, CharPlaneOffsets, CharXOffsets, CharYOffsets, 0x80, DrvTempRom, DrvChars);
	GfxDecode(0x40, 2, 16, 16, SpritePlaneOffsets, SpriteXOffsets, SpriteYOffsets, 0x200, DrvTempRom, DrvSprites);
	
	// Load and decode the dots
	memset(DrvTempRom, 0, 0x1000);
	nRet = BurnLoadRom(DrvTempRom,           10, 1); if (nRet != 0) return 1;
	GfxDecode(0x08, 2, 4, 4, DotPlaneOffsets, DotXOffsets, DotYOffsets, 0x80, DrvTempRom, DrvDots);

	// Load the PROMs
	nRet = BurnLoadRom(DrvPromPalette,       11, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvPromLookup,        12, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvPromVidLayout,     13, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvPromVidTiming,     14, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(NamcoSoundProm,       15, 1); if (nRet != 0) return 1;
	
	BurnFree(DrvTempRom);
	
	rallyx = 1;

	MachineInit();

	return 0;
}

static INT32 NrallyxInit()
{
	INT32 nRet = 0, nLen;

	// Allocate and Blank all required memory
	Mem = NULL;
	MemIndex();
	nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(Mem, 0, nLen);
	MemIndex();

	DrvTempRom = (UINT8 *)BurnMalloc(0x01000);

	// Load Z80 Program Roms
	nRet = BurnLoadRom(DrvTempRom, 0, 1); if (nRet != 0) return 1;
	memcpy(DrvZ80Rom1 + 0x0000, DrvTempRom + 0x0000, 0x800);
	memcpy(DrvZ80Rom1 + 0x1000, DrvTempRom + 0x0800, 0x800);
	nRet = BurnLoadRom(DrvTempRom, 1, 1); if (nRet != 0) return 1;
	memcpy(DrvZ80Rom1 + 0x0800, DrvTempRom + 0x0000, 0x800);
	memcpy(DrvZ80Rom1 + 0x1800, DrvTempRom + 0x0800, 0x800);
	nRet = BurnLoadRom(DrvTempRom, 2, 1); if (nRet != 0) return 1;
	memcpy(DrvZ80Rom1 + 0x2000, DrvTempRom + 0x0000, 0x800);
	memcpy(DrvZ80Rom1 + 0x3000, DrvTempRom + 0x0800, 0x800);
	nRet = BurnLoadRom(DrvTempRom, 3, 1); if (nRet != 0) return 1;
	memcpy(DrvZ80Rom1 + 0x2800, DrvTempRom + 0x0000, 0x800);
	memcpy(DrvZ80Rom1 + 0x3800, DrvTempRom + 0x0800, 0x800);
	
	// Load and decode the chars and sprites
	memset(DrvTempRom, 0, 0x1000);
	nRet = BurnLoadRom(DrvTempRom + 0x0000,  4, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x0800,  5, 1); if (nRet != 0) return 1;
	GfxDecode(0x100, 2, 8, 8, CharPlaneOffsets, CharXOffsets, CharYOffsets, 0x80, DrvTempRom, DrvChars);
	GfxDecode(0x40, 2, 16, 16, SpritePlaneOffsets, SpriteXOffsets, SpriteYOffsets, 0x200, DrvTempRom, DrvSprites);
	
	// Load and decode the dots
	memset(DrvTempRom, 0, 0x1000);
	nRet = BurnLoadRom(DrvTempRom,           6, 1); if (nRet != 0) return 1;
	GfxDecode(0x08, 2, 4, 4, DotPlaneOffsets, DotXOffsets, DotYOffsets, 0x80, DrvTempRom, DrvDots);

	// Load the PROMs
	nRet = BurnLoadRom(DrvPromPalette,       7, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvPromLookup,        8, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvPromVidLayout,     9, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvPromVidTiming,    10, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(NamcoSoundProm,      11, 1); if (nRet != 0) return 1;
	
	BurnFree(DrvTempRom);
	
	rallyx = 1;

	MachineInit();

	return 0;
}

static INT32 JunglerInit()
{
	INT32 nRet = 0, nLen;

	// Allocate and Blank all required memory
	Mem = NULL;
	JunglerMemIndex();
	nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(Mem, 0, nLen);
	JunglerMemIndex();

	DrvTempRom = (UINT8 *)BurnMalloc(0x01000);

	// Load Z80 Program Roms
	nRet = BurnLoadRom(DrvZ80Rom1 + 0x00000,  0, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvZ80Rom1 + 0x01000,  1, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvZ80Rom1 + 0x02000,  2, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvZ80Rom1 + 0x03000,  3, 1); if (nRet != 0) return 1;
	
	// Load Z80 Sound Program Roms
	nRet = BurnLoadRom(DrvZ80Rom2 + 0x00000,  4, 1); if (nRet != 0) return 1;
	
	// Load and decode the chars and sprites
	nRet = BurnLoadRom(DrvTempRom + 0x00000,  5, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x00800,  6, 1); if (nRet != 0) return 1;
	GfxDecode(0x100, 2, 8, 8, JunglerCharPlaneOffsets, CharXOffsets, CharYOffsets, 0x80, DrvTempRom, DrvChars);
	GfxDecode(0x40, 2, 16, 16, JunglerSpritePlaneOffsets, JunglerSpriteXOffsets, SpriteYOffsets, 0x200, DrvTempRom, DrvSprites);
	
	// Load and decode the dots
	memset(DrvTempRom, 0, 0x1000);
	nRet = BurnLoadRom(DrvTempRom,            7, 1); if (nRet != 0) return 1;
	GfxDecode(0x08, 2, 4, 4, DotPlaneOffsets, DotXOffsets, DotYOffsets, 0x80, DrvTempRom, DrvDots);

	// Load the PROMs
	nRet = BurnLoadRom(DrvPromPalette,        8, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvPromLookup,         9, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvPromVidLayout,     10, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvPromVidTiming,     11, 1); if (nRet != 0) return 1;
	
	BurnFree(DrvTempRom);

	junglermode = 1; // for locomotn(timeplt) sound driver
	junglerinputs = 1;

	JunglerMachineInit();

	return 0;
}

static INT32 LococommonDrvInit(INT32 prgroms, INT32 soundroms)
{
	INT32 nRet = 0, nLen;

	// Allocate and Blank all required memory
	Mem = NULL;
	JunglerMemIndex();
	nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(Mem, 0, nLen);
	JunglerMemIndex();

	DrvTempRom = (UINT8 *)BurnMalloc(0x04000);

	// Load Z80 Program Roms
	for (INT32 i = 0; i < prgroms; i++) {
		nRet = BurnLoadRom(DrvZ80Rom1 + (i * 0x01000),  i, 1); if (nRet != 0) return 1;
	}
	
	// Load Z80 Sound Program Roms
	for (INT32 i = 0; i < soundroms; i++) {
		nRet = BurnLoadRom(DrvZ80Rom2 + (i * 0x01000),  i + prgroms, 1); if (nRet != 0) return 1;
	}

	INT32 rompos = (prgroms) + (soundroms);

	// Load and decode the chars and sprites
	nRet = BurnLoadRom(DrvTempRom + 0x00000,  0 + rompos, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x01000,  1 + rompos, 1); if (nRet != 0) return 1;
	GfxDecode(0x200, 2, 8, 8, JunglerCharPlaneOffsets, CharXOffsets, CharYOffsets, 0x80, DrvTempRom, DrvChars);
	GfxDecode(0x80, 2, 16, 16, JunglerSpritePlaneOffsets, JunglerSpriteXOffsets, SpriteYOffsets, 0x200, DrvTempRom, DrvSprites);
	
	// Load and decode the dots
	memset(DrvTempRom, 0, 0x1000);
	nRet = BurnLoadRom(DrvTempRom,           2 + rompos, 1); if (nRet != 0) return 1;
	GfxDecode(0x08, 2, 4, 4, DotPlaneOffsets, DotXOffsets, DotYOffsets, 0x80, DrvTempRom, DrvDots);

	// Load the PROMs
	nRet = BurnLoadRom(DrvPromPalette,       3 + rompos, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvPromLookup,        4 + rompos, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvPromVidLayout,     5 + rompos, 1); if (nRet != 0) return 1;

	BurnFree(DrvTempRom);

	junglermode = 1; // for locomotn(timeplt) sound driver
	locomotnmode = 1;

	JunglerMachineInit();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	if (junglermode) {
		TimepltSndExit(); // konami
	} else {
		NamcoSoundExit(); // rallyx
		BurnSampleExit();
	}

	ZetExit();
	
	BurnFree(Mem);
	
	DrvCPUFireIRQ = 0;
	DrvCPUIRQVector = 0;
	xScroll = 0;
	yScroll = 0;
	DrvLastBang = 0;

	stars_enable = 0;
	total_stars = 0;

	junglermode = 0;
	junglerflip = 0;
	junglerinputs = 0;

	locomotnmode = 0;
	rallyx = 0;
	commsegamode = 0;

	return 0;
}

#define MAX_NETS			3
#define MAX_RES_PER_NET			18
#define Combine2Weights(tab,w0,w1)	((int)(((tab)[0]*(w0) + (tab)[1]*(w1)) + 0.5))
#define Combine3Weights(tab,w0,w1,w2)	((int)(((tab)[0]*(w0) + (tab)[1]*(w1) + (tab)[2]*(w2)) + 0.5))

static double ComputeResistorWeights(INT32 MinVal, INT32 MaxVal, double Scaler, INT32 Count1, const INT32 *Resistances1, double *Weights1, INT32 PullDown1, INT32 PullUp1,	INT32 Count2, const INT32 *Resistances2, double *Weights2, INT32 PullDown2, INT32 PullUp2, INT32 Count3, const INT32 *Resistances3, double *Weights3, INT32 PullDown3, INT32 PullUp3)
{
	INT32 NetworksNum;

	INT32 ResCount[MAX_NETS];
	double r[MAX_NETS][MAX_RES_PER_NET];
	double w[MAX_NETS][MAX_RES_PER_NET];
	double ws[MAX_NETS][MAX_RES_PER_NET];
	INT32 r_pd[MAX_NETS];
	INT32 r_pu[MAX_NETS];

	double MaxOut[MAX_NETS];
	double *Out[MAX_NETS];

	INT32 i, j, n;
	double Scale;
	double Max;

	NetworksNum = 0;
	for (n = 0; n < MAX_NETS; n++) {
		INT32 Count, pd, pu;
		const INT32 *Resistances;
		double *Weights;

		switch (n) {
			case 0: {
				Count = Count1;
				Resistances = Resistances1;
				Weights = Weights1;
				pd = PullDown1;
				pu = PullUp1;
				break;
			}
			
			case 1: {
				Count = Count2;
				Resistances = Resistances2;
				Weights = Weights2;
				pd = PullDown2;
				pu = PullUp2;
				break;
			}
		
			case 2:
			default: {
				Count = Count3;
				Resistances = Resistances3;
				Weights = Weights3;
				pd = PullDown3;
				pu = PullUp3;
				break;
			}
		}

		if (Count > 0) {
			ResCount[NetworksNum] = Count;
			for (i = 0; i < Count; i++) {
				r[NetworksNum][i] = 1.0 * Resistances[i];
			}
			Out[NetworksNum] = Weights;
			r_pd[NetworksNum] = pd;
			r_pu[NetworksNum] = pu;
			NetworksNum++;
		}
	}

	for (i = 0; i < NetworksNum; i++) {
		double R0, R1, Vout, Dst;

		for (n = 0; n < ResCount[i]; n++) {
			R0 = (r_pd[i] == 0) ? 1.0 / 1e12 : 1.0 / r_pd[i];
			R1 = (r_pu[i] == 0) ? 1.0 / 1e12 : 1.0 / r_pu[i];

			for (j = 0; j < ResCount[i]; j++) {
				if (j == n) {
					if (r[i][j] != 0.0) R1 += 1.0 / r[i][j];
				} else {
					if (r[i][j] != 0.0) R0 += 1.0 / r[i][j];
				}
			}

			R0 = 1.0/R0;
			R1 = 1.0/R1;
			Vout = (MaxVal - MinVal) * R0 / (R1 + R0) + MinVal;

			Dst = (Vout < MinVal) ? MinVal : (Vout > MaxVal) ? MaxVal : Vout;

			w[i][n] = Dst;
		}
	}

	j = 0;
	Max = 0.0;
	for (i = 0; i < NetworksNum; i++) {
		double Sum = 0.0;

		for (n = 0; n < ResCount[i]; n++) Sum += w[i][n];

		MaxOut[i] = Sum;
		if (Max < Sum) {
			Max = Sum;
			j = i;
		}
	}

	if (Scaler < 0.0) {
		Scale = ((double)MaxVal) / MaxOut[j];
	} else {
		Scale = Scaler;
	}

	for (i = 0; i < NetworksNum; i++) {
		for (n = 0; n < ResCount[i]; n++) {
			ws[i][n] = w[i][n] * Scale;
			(Out[i])[n] = ws[i][n];
		}
	}

	return Scale;

}

static void DrvCalcPalette()
{
	static const INT32 ResistancesRG[3] = { 1000, 470, 220 };
	static const INT32 ResistancesB[2] = { 470, 220 };
	double rWeights[3], gWeights[3], bWeights[2];
	UINT32 Palette[32];
	UINT32 i;
	
	ComputeResistorWeights(0, 255, -1.0, 3, &ResistancesRG[0], rWeights, 0, 0, 3, &ResistancesRG[0], gWeights, 0, 0, 2, &ResistancesB[0], bWeights, 1000, 0);
	
	for (i = 0; i < 32; i++) {
		INT32 Bit0, Bit1, Bit2;
		INT32 r, g, b;

		Bit0 = (DrvPromPalette[i] >> 0) & 0x01;
		Bit1 = (DrvPromPalette[i] >> 1) & 0x01;
		Bit2 = (DrvPromPalette[i] >> 2) & 0x01;
		r = Combine3Weights(rWeights, Bit0, Bit1, Bit2);

		/* green component */
		Bit0 = (DrvPromPalette[i] >> 3) & 0x01;
		Bit1 = (DrvPromPalette[i] >> 4) & 0x01;
		Bit2 = (DrvPromPalette[i] >> 5) & 0x01;
		g = Combine3Weights(gWeights, Bit0, Bit1, Bit2);

		/* blue component */
		Bit0 = (DrvPromPalette[i] >> 6) & 0x01;
		Bit1 = (DrvPromPalette[i] >> 7) & 0x01;
		b = Combine2Weights(bWeights, Bit0, Bit1);

		Palette[i] = BurnHighCol(r, g, b, 0);
	}
	
	for (i = 0; i < 256; i++) {
		UINT8 PaletteEntry = DrvPromLookup[i] & 0x0f;
		DrvPalette[i] = Palette[PaletteEntry];
	}
	
	for (i = 256; i < 260; i++) {
		DrvPalette[i] = Palette[(i - 0x100) | 0x10];
	}
}

static void DrvCalcPaletteJungler()
{
	static const INT32 ResistancesRG[3] =   { 1000, 470, 220 };
	static const INT32 ResistancesB[2] =    { 470, 220 };
	static const INT32 ResistancesSTAR[2] = { 150, 100 };
	double rWeights[3], gWeights[3], bWeights[2];
	double rWeightsSTAR[3], gWeightsSTAR[3], bWeightsSTAR[2];
	UINT32 Palette[0x60];
	UINT32 i;
	
	double scale =
		ComputeResistorWeights(0, 255, -1.0,
							   2, &ResistancesSTAR[0], rWeightsSTAR, 0, 0,
							   2, &ResistancesSTAR[0], gWeightsSTAR, 0, 0,
							   2, &ResistancesSTAR[0], bWeightsSTAR, 0, 0);

	ComputeResistorWeights(0, 255, scale,
						   3, &ResistancesRG[0], rWeights, 1000, 0,
						   3, &ResistancesRG[0], gWeights, 1000, 0,
						   2, &ResistancesB[0],  bWeights, 1000, 0);
	
	for (i = 0; i < 0x20; i++) { // create palette
		INT32 Bit0, Bit1, Bit2;
		INT32 r, g, b;

		Bit0 = (DrvPromPalette[i] >> 0) & 0x01;
		Bit1 = (DrvPromPalette[i] >> 1) & 0x01;
		Bit2 = (DrvPromPalette[i] >> 2) & 0x01;
		r = Combine3Weights(rWeights, Bit0, Bit1, Bit2);

		/* green component */
		Bit0 = (DrvPromPalette[i] >> 3) & 0x01;
		Bit1 = (DrvPromPalette[i] >> 4) & 0x01;
		Bit2 = (DrvPromPalette[i] >> 5) & 0x01;
		g = Combine3Weights(gWeights, Bit0, Bit1, Bit2);

		/* blue component */
		Bit0 = (DrvPromPalette[i] >> 6) & 0x01;
		Bit1 = (DrvPromPalette[i] >> 7) & 0x01;
		b = Combine2Weights(bWeights, Bit0, Bit1);

		Palette[i] = BurnHighCol(r, g, b, 0);
	}

	for (i = 0x20; i < 0x60; i++) { // stars
		INT32 bit0, bit1;
		INT32 r, g, b;

		/* red component */
		bit0 = ((i - 0x20) >> 0) & 0x01;
		bit1 = ((i - 0x20) >> 1) & 0x01;
		r = Combine2Weights(rWeightsSTAR, bit0, bit1);

		/* green component */
		bit0 = ((i - 0x20) >> 2) & 0x01;
		bit1 = ((i - 0x20) >> 3) & 0x01;
		g = Combine2Weights(gWeightsSTAR, bit0, bit1);

		/* blue component */
		bit0 = ((i - 0x20) >> 4) & 0x01;
		bit1 = ((i - 0x20) >> 5) & 0x01;
		b = Combine2Weights(bWeightsSTAR, bit0, bit1);

		Palette[i] = BurnHighCol(r, g, b, 0);
	}

	for (i = 0x000; i < 0x100; i++)	{ // char&sprites
		UINT8 PaletteEntry = DrvPromLookup[i] & 0x0f;
		DrvPalette[i] = Palette[PaletteEntry];
	}

	for (i = 0x100; i < 0x104; i++) { // bullets
		DrvPalette[i] = Palette[(i - 0x100) | 0x10];
	}

	for (i = 0x104; i < 0x144; i++) { // stars
		DrvPalette[i] = Palette[(i - 0x104) + 0x20];
	}
}

#undef MAX_NETS
#undef MAX_RES_PER_NET
#undef Combine2Weights
#undef Combine3Weights

static void DrvRenderBgLayer(INT32 priority)
{
	INT32 sx, sy, Code, Colour, x, y, xFlip, yFlip;

	INT32 Displacement = (rallyx) ? 1 : 0;
	INT32 scrollx = -(xScroll - 3 * Displacement);
	INT32 scrolly = -(yScroll + 16);

	for (INT32 offs = 0x3ff; offs >= 0; offs--) {
		Code   = DrvVideoRam[0x400 + offs];
		Colour = DrvVideoRam[0xc00 + offs];

		if (locomotnmode) {
			Code = (Code & 0x7f) + 2 * (Colour & 0x40) + 2 * (Code & 0x80);
		}

		if (((Colour & 0x20) >> 5) != priority) continue;

		sx = offs % 32;
		sy = offs / 32;
		xFlip = ~Colour & 0x40;
		yFlip = Colour & 0x80;

		if (locomotnmode) {
			xFlip = yFlip;
		}

		Colour &= 0x3f;

		x = 8 * sx;
		y = 8 * sy;

		x += scrollx;
		y += scrolly;

		if (x < -7) x += 256;
		if (y < -7) y += 256;

		if (x >= nScreenWidth || y >= nScreenHeight) continue;

		if (xFlip) {
			if (yFlip) {
				Render8x8Tile_FlipXY_Clip(pTransDraw, Code, x, y, Colour, 2, 0, DrvChars);
				Render8x8Tile_FlipXY_Clip(pTransDraw, Code, x-256, y, Colour, 2, 0, DrvChars);
			} else {
				Render8x8Tile_FlipX_Clip(pTransDraw, Code, x, y, Colour, 2, 0, DrvChars);
				Render8x8Tile_FlipX_Clip(pTransDraw, Code, x-256, y, Colour, 2, 0, DrvChars);
			}
		} else {
			if (yFlip) {
				Render8x8Tile_FlipY_Clip(pTransDraw, Code, x, y, Colour, 2, 0, DrvChars);
				Render8x8Tile_FlipY_Clip(pTransDraw, Code, x-256, y, Colour, 2, 0, DrvChars);
			} else {
				Render8x8Tile_Clip(pTransDraw, Code, x, y, Colour, 2, 0, DrvChars);
				Render8x8Tile_Clip(pTransDraw, Code, x-256, y, Colour, 2, 0, DrvChars);
			}
		}
	}
}

static void DrvRender8x32Layer()
{
	INT32 mx, my, Code, Colour, x, y, TileIndex, Flip, xFlip, yFlip;

	for (my = 0; my < 32; my++) {
		for (mx = 0; mx < 8; mx++) {
			TileIndex = mx + (my << 5);
			Code   = DrvVideoRam[TileIndex + 0x000 + 0x000];
			Colour = DrvVideoRam[TileIndex + 0x000 + 0x800];

			Flip = ((Colour >> 6) & 0x03) ^ 1;
			xFlip = Flip & 0x01;
			yFlip = Flip & 0x02;

			if (locomotnmode) {
				Code = (Code & 0x7f) + 2 * (Colour & 0x40) + 2 * (Code & 0x80);
				xFlip = yFlip = Colour & 0x80;
			}

			Colour &= 0x3f;

			x = 8 * mx;
			y = 8 * my;
			
			y -= 16;
			
			x -= 32;
			if (x < 0) x += 64;
			
			x += 224;

			if (x >= nScreenWidth || y >= nScreenHeight) continue;

			if (x > 8 && x < 280 && y > 8 && y < 216) {
				if (xFlip) {
					if (yFlip) {
						Render8x8Tile_FlipXY(pTransDraw, Code, x, y, Colour, 2, 0, DrvChars);
					} else {
						Render8x8Tile_FlipX(pTransDraw, Code, x, y, Colour, 2, 0, DrvChars);
					}
				} else {
					if (yFlip) {
						Render8x8Tile_FlipY(pTransDraw, Code, x, y, Colour, 2, 0, DrvChars);
					} else {
						Render8x8Tile(pTransDraw, Code, x, y, Colour, 2, 0, DrvChars);
					}
				}
			} else {
				if (xFlip) {
					if (yFlip) {
						Render8x8Tile_FlipXY_Clip(pTransDraw, Code, x, y, Colour, 2, 0, DrvChars);
					} else {
						Render8x8Tile_FlipX_Clip(pTransDraw, Code, x, y, Colour, 2, 0, DrvChars);
					}
				} else {
					if (yFlip) {
						Render8x8Tile_FlipY_Clip(pTransDraw, Code, x, y, Colour, 2, 0, DrvChars);
					} else {
						Render8x8Tile_Clip(pTransDraw, Code, x, y, Colour, 2, 0, DrvChars);
					}
				}
			}
		}
	}
}

static void DrvRenderSprites()
{
	INT32 SpriteRamBase = (commsegamode) ? 0x00 : 0x14;
	UINT8 *SpriteRam = DrvVideoRam;
	UINT8 *SpriteRam2 = DrvVideoRam + 0x800;
	INT32 Displacement = (rallyx || junglermode) ? 1 : 0;

	for (INT32 Offs = 0x20 - 2; Offs >= SpriteRamBase; Offs -= 2) {
		INT32 sx = SpriteRam[Offs + 1] + ((SpriteRam2[Offs + 1] & 0x80) << 1) - Displacement;
		INT32 sy = 241 - SpriteRam2[Offs] - Displacement;
		INT32 Colour = SpriteRam2[Offs + 1] & 0x3f;

		INT32 xFlip = SpriteRam[Offs] & 1;
		INT32 yFlip = SpriteRam[Offs] & 2;
		INT32 Code = (SpriteRam[Offs] & 0xfc) >> 2;

		if (locomotnmode) {
			xFlip = yFlip;
			Code = ((SpriteRam[Offs] & 0x7c) >> 2) + 0x20*(SpriteRam[Offs] & 0x01) + ((SpriteRam[Offs] & 0x80) >> 1);
		}

		if (junglerflip) {
			sx = (nScreenWidth-16-1) - sx;
			sy = SpriteRam2[Offs] - Displacement;
			xFlip = !xFlip;
			yFlip = !yFlip;
		}


		sy -= 16;

		if (sx >= nScreenWidth || sy >= nScreenHeight) continue;
		
		if (sx > 16 && sx < 272 && sy > 16 && sy < 208) {
			if (xFlip) {
				if (yFlip) {
					Render16x16Tile_Mask_FlipXY(pTransDraw, Code, sx, sy, Colour, 2, 0, 0, DrvSprites);
				} else {
					Render16x16Tile_Mask_FlipX(pTransDraw, Code, sx, sy, Colour, 2, 0, 0, DrvSprites);
				}
			} else {
				if (yFlip) {
					Render16x16Tile_Mask_FlipY(pTransDraw, Code, sx, sy, Colour, 2, 0, 0, DrvSprites);
				} else {
					Render16x16Tile_Mask(pTransDraw, Code, sx, sy, Colour, 2, 0, 0, DrvSprites);
				}
			}
		} else {
			if (xFlip) {
				if (yFlip) {
					Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, Code, sx, sy, Colour, 2, 0, 0, DrvSprites);
				} else {
					Render16x16Tile_Mask_FlipX_Clip(pTransDraw, Code, sx, sy, Colour, 2, 0, 0, DrvSprites);
				}
			} else {
				if (yFlip) {
					Render16x16Tile_Mask_FlipY_Clip(pTransDraw, Code, sx, sy, Colour, 2, 0, 0, DrvSprites);
				} else {
					Render16x16Tile_Mask_Clip(pTransDraw, Code, sx, sy, Colour, 2, 0, 0, DrvSprites);
				}
			}
		}
	}
}

static void DrvRenderBullets()
{
	INT32 SpriteRamBase = (commsegamode) ? 0x00 : 0x14;
	UINT8 *RadarX = DrvVideoRam + 0x020;
	UINT8 *RadarY = DrvVideoRam + 0x820;
	
	for (INT32 Offs = SpriteRamBase; Offs < 0x20; Offs++) {
		INT32 x, y, Code, Flip;

		Code = ((DrvRadarAttrRam[Offs & 0x0f] & 0x0e) >> 1) ^ 0x07;
		x = RadarX[Offs] + ((~DrvRadarAttrRam[Offs & 0x0f] & 0x01) << 8);
		y = ((locomotnmode) ? 252 : 253) - RadarY[Offs];
		Flip = 0;

		if (junglermode) {
			Code = (DrvRadarAttrRam[Offs & 0x0f] & 0x07) ^ 0x07;
			x = RadarX[Offs] + ((~DrvRadarAttrRam[Offs & 0x0f] & 0x08) << 5);

			if (junglerflip) {
				x = (nScreenWidth-4) - x;
				y = RadarY[Offs] - 1;
				Flip = 1;
			}
		}

		y -= 16;

		if (x >= nScreenWidth || y >= nScreenHeight) continue;
		
//		if (flip_screen_get(machine))
//			x -= 3;

//		if (transpen)
//			drawgfx_transpen(bitmap,cliprect,machine->gfx[2],
//					((state->radarattr[offs & 0x0f] & 0x0e) >> 1) ^ 0x07,
//					0,
//					0,0,
//					x,y,
//					3);
//		else
//			drawgfx_transtable(bitmap,cliprect,machine->gfx[2],
//					((state->radarattr[offs & 0x0f] & 0x0e) >> 1) ^ 0x07,
//					0,
//					0,0,
//					x,y,
///					state->drawmode_table,machine->shadow_table);
		if (Flip) {
			RenderCustomTile_Mask_FlipXY_Clip(pTransDraw, 4, 4, Code, x, y, 0, 2, 3, 0x100, DrvDots);
		} else {
			RenderCustomTile_Mask_Clip(pTransDraw, 4, 4, Code, x, y, 0, 2, 3, 0x100, DrvDots);
		}
	}
}

void calculate_star_field()
{
	INT32 generator;

	total_stars = 0;
	generator = 0;
	memset(&j_stars, 0, sizeof(j_stars));

	for (INT32 y = 0; y < 256; y++) {
		for (INT32 x = 0; x < 288; x++) {
			INT32 bit1, bit2;

			generator <<= 1;
			bit1 = (~generator >> 17) & 1;
			bit2 = (generator >> 5) & 1;

			if (bit1 ^ bit2)
				generator |= 1;

			if (((~generator >> 16) & 1) && (generator & 0xfe) == 0xfe) {
				INT32 color = (~(generator >> 8)) & 0x3f;

				if (color && total_stars < 1000)
				{
					j_stars[total_stars].x = x;
					j_stars[total_stars].y = y;
					j_stars[total_stars].color = color;

					total_stars++;
				}
			}
		}
	}
}

void plot_star(INT32 x, INT32 y, INT32 color)
{
	if (junglerflip) {
		x = 255 - x;
		y = 255 - y;
	}

	if ((x >= 0 && x < nScreenWidth) && (y >= 0 && y < nScreenHeight)) {
		UINT16 *pxl = &pTransDraw[(y * nScreenWidth) + x];
		if (*pxl == 0x1c || *pxl == 0x6c || *pxl == 0x00)
			*pxl = 0x104 + color;
	}
}

void draw_stars()
{
	for (INT32 offs = 0; offs < total_stars; offs++) {
		INT32 x = j_stars[offs].x;
		INT32 y = j_stars[offs].y;

		if ((y & 0x01) ^ ((x >> 3) & 0x01))
			plot_star(x, y, j_stars[offs].color);
	}
}

static void DrvDraw()
{
	BurnTransferClear();
	DrvCalcPalette();
	if (nBurnLayer & 1) DrvRenderBgLayer(0);
	if (nBurnLayer & 2) DrvRenderSprites();
	if (nBurnLayer & 4) DrvRenderBgLayer(1);
	if (nBurnLayer & 8) DrvRender8x32Layer();
	if (nBurnLayer & 8) DrvRenderBullets();
	BurnTransferCopy(DrvPalette);
}

extern int counter;   int funk = 0;

static void DrvDrawJungler()
{
	BurnTransferClear();
	DrvCalcPaletteJungler();
	if (nBurnLayer & 1) DrvRenderBgLayer(0);
	if (nBurnLayer & 4) DrvRenderBgLayer(1);
	if (nBurnLayer & 8) DrvRender8x32Layer();
	if (nSpriteEnable & 1) DrvRenderSprites();
	if (nBurnLayer & 2) DrvRenderBullets();

#if 0
	// for debugging tactician stars. grrr!
	FILE * f = fopen("c:\\transdraw.bin", "wb+");
	fwrite(pTransDraw, 1, nScreenWidth * nScreenHeight * 2, f);
	fclose(f);
#endif

	if (stars_enable)
		draw_stars();

	BurnTransferCopy(DrvPalette);
}

static INT32 DrvFrame()
{
	INT32 nInterleave = nBurnSoundLen;
	
	if (DrvReset) DrvDoReset();

	DrvMakeInputs();
	
	INT32 nSoundBufferPos = 0;
	INT32 nCyclesTotal = { (18432000 / 6) / 60 };
	INT32 nCyclesDone = 0;
	INT32 nCyclesSegment;
	
	ZetNewFrame();
	
	for (INT32 i = 0; i < nInterleave; i++) {
		INT32 nNext;
		
		// Run Z80 #1
		ZetOpen(0);
		nNext = (i + 1) * nCyclesTotal / nInterleave;
		nCyclesSegment = nNext - nCyclesDone;
		nCyclesSegment = ZetRun(nCyclesSegment);
		nCyclesDone += nCyclesSegment;
		if (i == (nInterleave - 1) && DrvCPUFireIRQ) {
			ZetSetVector(DrvCPUIRQVector);
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		}
		ZetClose();
		
		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			
			if (nSegmentLength) {
				NamcoSoundUpdate(pSoundBuf, nSegmentLength);
				BurnSampleRender(pSoundBuf, nSegmentLength);
			}
			nSoundBufferPos += nSegmentLength;
		}
	}
	
	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);

		if (nSegmentLength) {
			NamcoSoundUpdate(pSoundBuf, nSegmentLength);
			BurnSampleRender(pSoundBuf, nSegmentLength);
		}
	}

	if (pBurnDraw) DrvDraw();

	return 0;
}

static INT32 JunglerFrame()
{
	INT32 nInterleave = 256;
	
	if (DrvReset) JunglerDoReset();

	JunglerMakeInputs();
	
	INT32 nSoundBufferPos = 0;

	INT32 nCyclesTotal[2] = { (18432000 / 6) / 60, (14318180 / 8) / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };
	INT32 nCyclesSegment;
	
	ZetNewFrame();
	
	for (INT32 i = 0; i < nInterleave; i++) {
		INT32 nCurrentCPU, nNext;
		
		// Run Z80 #1
		nCurrentCPU = 0;
		ZetOpen(nCurrentCPU);
		nNext = (i + 1) * nCyclesTotal[nCurrentCPU] / nInterleave;
		nCyclesSegment = nNext - nCyclesDone[nCurrentCPU];
		nCyclesSegment = ZetRun(nCyclesSegment);
		nCyclesDone[nCurrentCPU] += nCyclesSegment;
		if (i == (nInterleave - 1) && DrvCPUFireIRQ) {
			ZetNmi();
		}
		ZetClose();

		ZetOpen(1);
		nNext = (nCyclesTotal[1] * i) / nInterleave;
		nCyclesDone[1] += ZetRun(nNext - nCyclesDone[1]);
		ZetClose();

		// Render Sound Segment
		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			TimepltSndUpdate(pAY8910Buffer, pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}
	
	// Make sure the buffer is entirely filled.
	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		TimepltSndUpdate(pAY8910Buffer, pSoundBuf, nSegmentLength);
	}

	if (pBurnDraw) DrvDrawJungler();

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {			// Return minimum compatible version
		*pnMin = 0x029708;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = RamStart;
		ba.nLen	  = RamEnd-RamStart;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}
	
	if (nAction & ACB_DRIVER_DATA) {
		ZetScan(nAction);

		if (junglermode) {
			TimepltSndScan(nAction, pnMin);
			SCAN_VAR(last_sound_irq);
		}

		if (rallyx) {
			NamcoSoundScan(nAction, pnMin);
		}
	}
	
	return 0;
}

struct BurnDriver BurnDrvRallyx = {
	"rallyx", NULL, NULL, "rallyx", "1980",
	"Rally X (32k Ver.?)\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, RallyxRomInfo, RallyxRomName, RallyxSampleInfo, RallyxSampleName, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, NULL, DrvScan,
	NULL, 260, 288, 224, 4, 3
};

struct BurnDriver BurnDrvRallyxa = {
	"rallyxa", "rallyx", NULL, "rallyx", "1980",
	"Rally X\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, RallyxaRomInfo, RallyxaRomName, RallyxSampleInfo, RallyxSampleName, DrvInputInfo, DrvDIPInfo,
	DrvaInit, DrvExit, DrvFrame, NULL, DrvScan,
	NULL, 260, 288, 224, 4, 3
};

struct BurnDriver BurnDrvRallyxm = {
	"rallyxm", "rallyx", NULL, "rallyx", "1980",
	"Rally X (Midway)\0", NULL, "Namco (Midway License)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, RallyxmRomInfo, RallyxmRomName, RallyxSampleInfo, RallyxSampleName, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, NULL, DrvScan,
	NULL, 260, 288, 224, 4, 3
};

struct BurnDriver BurnDrvRallyxmr = {
	"rallyxmr", "rallyx", NULL, "rallyx", "1980",
	"Rally X (Model Racing)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, RallyxmrRomInfo, RallyxmrRomName, RallyxSampleInfo, RallyxSampleName, DrvInputInfo, DrvDIPInfo,
	DrvaInit, DrvExit, DrvFrame, NULL, DrvScan,
	NULL, 260, 288, 224, 4, 3
};

struct BurnDriver BurnDrvNrallyx = {
	"nrallyx", NULL, NULL, "rallyx", "1981",
	"New Rally X\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, NrallyxRomInfo, NrallyxRomName, RallyxSampleInfo, RallyxSampleName, DrvInputInfo, DrvDIPInfo,
	NrallyxInit, DrvExit, DrvFrame, NULL, DrvScan,
	NULL, 260, 288, 224, 4, 3
};

struct BurnDriver BurnDrvJungler = {
	"jungler", NULL, NULL, NULL, "1981",
	"Jungler\0", NULL, "Konami", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, JunglerRomInfo, JunglerRomName, NULL, NULL, JunglerInputInfo, JunglerDIPInfo,
	JunglerInit, DrvExit, JunglerFrame, NULL, DrvScan,
	NULL, 324, 224, 288, 3, 4
};

INT32 TactcianDrvInit()
{
	return LococommonDrvInit(6, 2);
}

// Tactician (set 1)

static struct BurnRomInfo tactcianRomDesc[] = {
	{ "tacticia.001",	0x1000, 0x99163e39, 1 }, //  0 maincpu
	{ "tacticia.002",	0x1000, 0x6d3e8a69, 1 }, //  1
	{ "tacticia.003",	0x1000, 0x0f71d0fa, 1 }, //  2
	{ "tacticia.004",	0x1000, 0x5e15f3b3, 1 }, //  3
	{ "tacticia.005",	0x1000, 0x76456106, 1 }, //  4
	{ "tacticia.006",	0x1000, 0xb33ca9ea, 1 }, //  5

	{ "tacticia.s2",	0x1000, 0x97d145a7, 2 }, //  6 tpsound
	{ "tacticia.s1",	0x1000, 0x067f781b, 2 }, //  7

	{ "tacticia.c1",	0x1000, 0x5d3ee965, 3 }, //  8 gfx1
	{ "tacticia.c2",	0x1000, 0xe8c59c4f, 3 }, //  9

	{ "tact6301.004",	0x0100, 0x88b0b511, 4 }, // 10 gfx2

	{ "tact6331.002",	0x0020, 0xb7ef83b7, 5 }, // 11 proms
	{ "tact6301.003",	0x0100, 0xa92796f2, 5 }, // 12
	{ "tact6331.001",	0x0020, 0x8f574815, 5 }, // 13
};

STD_ROM_PICK(tactcian)
STD_ROM_FN(tactcian)

struct BurnDriver BurnDrvTactcian = {
	"tactcian", NULL, NULL, NULL, "1982",
	"Tactician (set 1)\0", NULL, "Konami (Sega license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, tactcianRomInfo, tactcianRomName, NULL, NULL, TactcianInputInfo, TactcianDIPInfo,
	TactcianDrvInit, DrvExit, JunglerFrame, NULL, DrvScan,
	NULL, 324, 224, 288, 3, 4
};


// Tactician (set 2)

static struct BurnRomInfo tactcian2RomDesc[] = {
	{ "tan1",		0x1000, 0xddf38b75, 1 }, //  0 maincpu
	{ "tan2",		0x1000, 0xf065ee2e, 1 }, //  1
	{ "tan3",		0x1000, 0x2dba64fe, 1 }, //  2
	{ "tan4",		0x1000, 0x2ba07847, 1 }, //  3
	{ "tan5",		0x1000, 0x1dae4c61, 1 }, //  4
	{ "tan6",		0x1000, 0x2b36a18d, 1 }, //  5

	{ "tacticia.s2",	0x1000, 0x97d145a7, 2 }, //  6 tpsound
	{ "tacticia.s1",	0x1000, 0x067f781b, 2 }, //  7

	{ "c1",			0x1000, 0x5399471f, 3 }, //  8 gfx1
	{ "c2",			0x1000, 0x8e8861e8, 3 }, //  9

	{ "tact6301.004",	0x0100, 0x88b0b511, 4 }, // 10 gfx2

	{ "tact6331.002",	0x0020, 0xb7ef83b7, 5 }, // 11 proms
	{ "tact6301.003",	0x0100, 0xa92796f2, 5 }, // 12
	{ "tact6331.001",	0x0020, 0x8f574815, 5 }, // 13
};

STD_ROM_PICK(tactcian2)
STD_ROM_FN(tactcian2)

struct BurnDriver BurnDrvTactcian2 = {
	"tactcian2", "tactcian", NULL, NULL, "1981",
	"Tactician (set 2)\0", NULL, "Konami (Sega license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, tactcian2RomInfo, tactcian2RomName, NULL, NULL, TactcianInputInfo, TactcianDIPInfo,
	TactcianDrvInit, DrvExit, JunglerFrame, NULL, DrvScan,
	NULL, 324, 224, 288, 3, 4
};

INT32 LocomotnDrvInit()
{
	return LococommonDrvInit(5, 1);
}

// Loco-Motion

static struct BurnRomInfo locomotnRomDesc[] = {
	{ "1a.cpu",	0x1000, 0xb43e689a, 1 }, //  0 maincpu
	{ "2a.cpu",	0x1000, 0x529c823d, 1 }, //  1
	{ "3.cpu",	0x1000, 0xc9dbfbd1, 1 }, //  2
	{ "4.cpu",	0x1000, 0xcaf6431c, 1 }, //  3
	{ "5.cpu",	0x1000, 0x64cf8dd6, 1 }, //  4

	{ "1b_s1.bin",	0x1000, 0xa1105714, 2 }, //  5 tpsound

	{ "5l_c1.bin",	0x1000, 0x5732eda9, 3 }, //  6 gfx1
	{ "c2.cpu",	0x1000, 0xc3035300, 3 }, //  7

	{ "10g.bpr",	0x0100, 0x2ef89356, 4 }, //  8 gfx2

	{ "8b.bpr",	0x0020, 0x75b05da0, 5 }, //  9 proms
	{ "9d.bpr",	0x0100, 0xaa6cf063, 5 }, // 10
	{ "7a.bpr",	0x0020, 0x48c8f094, 5 }, // 11
	{ "10a.bpr",	0x0020, 0xb8861096, 5 }, // 12
};

STD_ROM_PICK(locomotn)
STD_ROM_FN(locomotn)

struct BurnDriver BurnDrvLocomotn = {
	"locomotn", NULL, NULL, NULL, "1982",
	"Loco-Motion\0", NULL, "Konami (Centuri license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, locomotnRomInfo, locomotnRomName, NULL, NULL, LocomotnInputInfo, LocomotnDIPInfo,
	LocomotnDrvInit, DrvExit, JunglerFrame, NULL, DrvScan,
	NULL, 324, 224, 256, 3, 4
};

INT32 GutangtnDrvInit()
{
	return LococommonDrvInit(5, 1);
}

// Guttang Gottong

static struct BurnRomInfo gutangtnRomDesc[] = {
	{ "3d_1.bin",	0x1000, 0xe9757395, 1 }, //  0 maincpu
	{ "3e_2.bin",	0x1000, 0x11d21d2e, 1 }, //  1
	{ "3f_3.bin",	0x1000, 0x4d80f895, 1 }, //  2
	{ "3h_4.bin",	0x1000, 0xaa258ddf, 1 }, //  3
	{ "3j_5.bin",	0x1000, 0x52aec87e, 1 }, //  4

	{ "1b_s1.bin",	0x1000, 0xa1105714, 2 }, //  5 tpsound

	{ "5l_c1.bin",	0x1000, 0x5732eda9, 3 }, //  6 gfx1
	{ "5m_c2.bin",	0x1000, 0x51c542fd, 3 }, //  7

	{ "10g.bpr",	0x0100, 0x2ef89356, 4 }, //  8 gfx2

	{ "8b.bpr",	0x0020, 0x75b05da0, 5 }, //  9 proms
	{ "9d.bpr",	0x0100, 0xaa6cf063, 5 }, // 10
	{ "7a.bpr",	0x0020, 0x48c8f094, 5 }, // 11
	{ "10a.bpr",	0x0020, 0xb8861096, 5 }, // 12
};

STD_ROM_PICK(gutangtn)
STD_ROM_FN(gutangtn)

struct BurnDriver BurnDrvGutangtn = {
	"gutangtn", "locomotn", NULL, NULL, "1982",
	"Guttang Gottong\0", NULL, "Konami (Sega license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, gutangtnRomInfo, gutangtnRomName, NULL, NULL, LocomotnInputInfo, LocomotnDIPInfo,
	GutangtnDrvInit, DrvExit, JunglerFrame, NULL, DrvScan,
	NULL, 324, 224, 256, 3, 4
};

INT32 CottongDrvInit()
{
	return LococommonDrvInit(4, 2);
}


// Cotocoto Cottong

static struct BurnRomInfo cottongRomDesc[] = {
	{ "c1",		0x1000, 0x2c256fe6, 1 }, //  0 maincpu
	{ "c2",		0x1000, 0x1de5e6a0, 1 }, //  1
	{ "c3",		0x1000, 0x01f909fe, 1 }, //  2
	{ "c4",		0x1000, 0xa89eb3e3, 1 }, //  3

	{ "c7",		0x1000, 0x3d83f6d3, 2 }, //  4 tpsound
	{ "c8",		0x1000, 0x323e1937, 2 }, //  5

	{ "c5",		0x1000, 0x992d079c, 3 }, //  6 gfx1
	{ "c6",		0x1000, 0x0149ef46, 3 }, //  7

	{ "5.bpr",	0x0100, 0x21fb583f, 4 }, //  8 gfx2

	{ "2.bpr",	0x0020, 0x26f42e6f, 5 }, //  9 proms
	{ "3.bpr",	0x0100, 0x4aecc0c8, 5 }, // 10
	{ "7a.bpr",	0x0020, 0x48c8f094, 5 }, // 11
	{ "10a.bpr",	0x0020, 0xb8861096, 5 }, // 12
};

STD_ROM_PICK(cottong)
STD_ROM_FN(cottong)

struct BurnDriver BurnDrvCottong = {
	"cottong", "locomotn", NULL, NULL, "1982",
	"Cotocoto Cottong\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, cottongRomInfo, cottongRomName, NULL, NULL, LocomotnInputInfo, LocomotnDIPInfo,
	CottongDrvInit, DrvExit, JunglerFrame, NULL, DrvScan,
	NULL, 324, 224, 256, 3, 4
};

INT32 LocobootDrvInit()
{
	return LococommonDrvInit(4, 2);
}

// Loco-Motion (bootleg)

static struct BurnRomInfo locobootRomDesc[] = {
	{ "g.116",	0x1000, 0x1248799c, 1 }, //  0 maincpu
	{ "g.117",	0x1000, 0x5b5b5753, 1 }, //  1
	{ "g.118",	0x1000, 0x6bc269e1, 1 }, //  2
	{ "g.119",	0x1000, 0x3feb762e, 1 }, //  3

	{ "c7",		0x1000, 0x3d83f6d3, 2 }, //  4 tpsound
	{ "c8",		0x1000, 0x323e1937, 2 }, //  5

	{ "c5",		0x1000, 0x992d079c, 3 }, //  6 gfx1
	{ "c6",		0x1000, 0x0149ef46, 3 }, //  7

	{ "5.bpr",	0x0100, 0x21fb583f, 4 }, //  8 gfx2

	{ "2.bpr",	0x0020, 0x26f42e6f, 5 }, //  9 proms
	{ "3.bpr",	0x0100, 0x4aecc0c8, 5 }, // 10
	{ "7a.bpr",	0x0020, 0x48c8f094, 5 }, // 11
	{ "10a.bpr",	0x0020, 0xb8861096, 5 }, // 12
};

STD_ROM_PICK(locoboot)
STD_ROM_FN(locoboot)

struct BurnDriver BurnDrvLocoboot = {
	"locoboot", "locomotn", NULL, NULL, "1982",
	"Loco-Motion (bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, locobootRomInfo, locobootRomName, NULL, NULL, LocomotnInputInfo, LocomotnDIPInfo,
	LocobootDrvInit, DrvExit, JunglerFrame, NULL, DrvScan,
	NULL, 324, 224, 256, 3, 4
};


INT32 CommsegaDrvInit()
{
	commsegamode = 1;
	return LococommonDrvInit(5, 1);
}

// Commando (Sega)

static struct BurnRomInfo commsegaRomDesc[] = {
	{ "csega1",	0x1000, 0x92de3405, 1 }, //  0 maincpu
	{ "csega2",	0x1000, 0xf14e2f9a, 1 }, //  1
	{ "csega3",	0x1000, 0x941dbf48, 1 }, //  2
	{ "csega4",	0x1000, 0xe0ac69b4, 1 }, //  3
	{ "csega5",	0x1000, 0xbc56ebd0, 1 }, //  4

	{ "csega8",	0x1000, 0x588b4210, 2 }, //  5 tpsound

	{ "csega7",	0x1000, 0xe8e374f9, 3 }, //  6 gfx1
	{ "csega6",	0x1000, 0xcf07fd5e, 3 }, //  7

	{ "gg3.bpr",	0x0100, 0xae7fd962, 4 }, //  8 gfx2

	{ "gg1.bpr",	0x0020, 0xf69e585a, 5 }, //  9 proms
	{ "gg2.bpr",	0x0100, 0x0b756e30, 5 }, // 10
	{ "gg0.bpr",	0x0020, 0x48c8f094, 5 }, // 11
	{ "tt3.bpr",	0x0020, 0xb8861096, 5 }, // 12
};

STD_ROM_PICK(commsega)
STD_ROM_FN(commsega)

struct BurnDriver BurnDrvCommsega = {
	"commsega", NULL, NULL, NULL, "1983",
	"Commando (Sega)\0", NULL, "Sega", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, commsegaRomInfo, commsegaRomName, NULL, NULL, CommsegaInputInfo, CommsegaDIPInfo,
	CommsegaDrvInit, DrvExit, JunglerFrame, NULL, DrvScan,
	NULL, 324, 224, 256, 3, 4
};


