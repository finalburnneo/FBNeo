// FB Neo Sega System 1/2 driver module
// Based on MAME driver by Jarek Parchanski, Nicola Salmoria, Mirko Buffoni

#include "tiles_generic.h"
#include "z80_intf.h"
#include "sn76496.h"
#include "bitswap.h"
#include "mc8123.h"
#include "8255ppi.h"
#include "mcs51.h"
#include "dtimer.h"
#include "burn_gun.h"

static UINT8 System1InputPort0[8]    = { 0, 0, 0, 0, 0, 0, 0, 0 };
static UINT8 System1InputPort1[8]    = { 0, 0, 0, 0, 0, 0, 0, 0 };
static UINT8 System1InputPort2[8]    = { 0, 0, 0, 0, 0, 0, 0, 0 };
static UINT8 System1InputPort3[8]    = { 0, 0, 0, 0, 0, 0, 0, 0 };
static UINT8 System1Dip[3]           = { 0, 0, 0 };
static UINT8 System1Input[4]         = { 0x00, 0x00, 0x00, 0x00 };
static UINT8 System1Reset            = 0;

static INT16 Analog[2];
static INT32 sht_trigger             = 0;
static INT32 is_shtngmst			 = 0;
static INT32 is_nob                  = 0;
static UINT8 nob_cpu_latch;
static UINT8 nob_mcu_latch;
static UINT8 nob_mcu_status;

static UINT8 *Mem                    = NULL;
static UINT8 *MemEnd                 = NULL;
static UINT8 *RamStart               = NULL;
static UINT8 *RamEnd                 = NULL;

static INT32 has_dial = 0;

static UINT8 *DrvMCURom;
static INT32 has_mcu = 0;
static UINT8 i8751Command;
// i8751 MCU (choplift)
static void DrvMCUReset(); // forward
static void DrvMCUSync(); // ""

static UINT8 *System1Rom1            = NULL;
static UINT8 *System1Rom2            = NULL;
static UINT8 *System1PromRed         = NULL;
static UINT8 *System1PromGreen       = NULL;
static UINT8 *System1PromBlue        = NULL;
static UINT8 *System1Ram1            = NULL;
static UINT8 *System1Ram2            = NULL;
static UINT8 *System1SpriteRam       = NULL;
static UINT8 *System1PaletteRam      = NULL;

static UINT8 *System1BgRam           = NULL;
static UINT8 *System1FgRam           = NULL;
static UINT8 *System1VideoRam        = NULL;

static UINT8 *System1ScrollXRam      = NULL;
static UINT8 *System1BgCollisionRam  = NULL;
static UINT8 *System1SprCollisionRam = NULL;
static UINT8 *System1f4Ram           = NULL;
static UINT8 *System1fcRam           = NULL;
static UINT32 *System1Palette        = NULL;
static UINT8 *System1Tiles           = NULL;
static UINT8 *System1Sprites         = NULL;
static UINT8 *System1TempRom         = NULL;
static UINT8 *SpriteOnScreenMap      = NULL;
static UINT8 *System1Fetch1          = NULL;
static UINT8 *System1MC8123Key       = NULL;
static UINT32 *System1TilesPenUsage  = NULL;
// for vbt's choplifter driver
static UINT8 System1RowScroll = 0;
static INT32 System1BankSwitch;
static UINT8 System1BgBankLatch;
static UINT8 System1BgBank = 0;
// end choplifter
static UINT8 System1ScrollX[2];
static UINT8 System1ScrollY;
static INT32 System1BgScrollX;
static INT32 System1BgScrollY;
static INT32 System1VideoMode;
static INT32 System1FlipScreen;
static INT32 System1SoundLatch;
static INT32 System1RomBank;
static INT32 NoboranbInp16Step;
static INT32 NoboranbInp17Step;
static INT32 NoboranbInp23Step;

static INT32 System1SpriteRomSize;
static INT32 System1NumTiles;
static INT32 System1SpriteXOffset;
static INT32 System1ColourProms = 0;
static INT32 System1BankedRom = 0;

static INT32 IsSystem2 = 0;
static INT32 Sys1UsePPI = 0; // For regulus/regulusu
static INT32 EnforceBars = 0; // see notes in enforce_bars()

// Set to 1 before init for doubled X (Sys1 only!), allows for smoother h-scrolling
// used in Wonder Boy (WboyInit(), etc)
static INT32 wide_mode = 0;

typedef void (*Decode)();
static Decode DecodeFunction;
static Decode TileDecodeFunction;

static INT32 System1Render();
static INT32 System2Render();

typedef void (*MakeInputs)();
static MakeInputs MakeInputsFunction;

static INT32 nCyclesExtra[3];

/*==============================================================================================
Input Definitions
===============================================================================================*/


#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo BlockgalInputList[] = {
	{"P1 Coin"           , BIT_DIGITAL  , System1InputPort2 + 0, "p1 coin"   },
	{"P1 Start"          , BIT_DIGITAL  , System1InputPort2 + 4, "p1 start"  },
	A("P1 Paddle"		 , BIT_ANALOG_REL, &Analog[0]          , "p1 x-axis"),
	{"P1 Fire 1"         , BIT_DIGITAL  , System1InputPort2 + 6, "p1 fire 1" },

	{"P2 Coin"           , BIT_DIGITAL  , System1InputPort2 + 1, "p2 coin"   },
	{"P2 Start"          , BIT_DIGITAL  , System1InputPort2 + 5, "p2 start"  },
	A("P2 Paddle"		 , BIT_ANALOG_REL, &Analog[1]          , "p2 x-axis"),
	{"P2 Fire 1"         , BIT_DIGITAL  , System1InputPort2 + 7, "p2 fire 1" },

	{"Reset"             , BIT_DIGITAL  , &System1Reset        , "reset"     },
	{"Service"           , BIT_DIGITAL  , System1InputPort2 + 3, "service"   },
	{"Dip 1"             , BIT_DIPSWITCH, System1Dip + 0       , "dip"       },
	{"Dip 2"             , BIT_DIPSWITCH, System1Dip + 1       , "dip"       },
};
#undef A
STDINPUTINFO(Blockgal)

static struct BurnInputInfo FlickyInputList[] = {
	{"P1 Coin"           , BIT_DIGITAL  , System1InputPort2 + 0, "p1 coin"   },
	{"P1 Start"          , BIT_DIGITAL  , System1InputPort2 + 4, "p1 start"  },
	{"P1 Left"           , BIT_DIGITAL  , System1InputPort0 + 7, "p1 left"   },
	{"P1 Right"          , BIT_DIGITAL  , System1InputPort0 + 6, "p1 right"  },
	{"P1 Fire 1"         , BIT_DIGITAL  , System1InputPort0 + 2, "p1 fire 1" },

	{"P2 Coin"           , BIT_DIGITAL  , System1InputPort2 + 1, "p2 coin"   },
	{"P2 Start"          , BIT_DIGITAL  , System1InputPort2 + 5, "p2 start"  },
	{"P2 Left"           , BIT_DIGITAL  , System1InputPort1 + 7, "p2 left"   },
	{"P2 Right"          , BIT_DIGITAL  , System1InputPort1 + 6, "p2 right"  },
	{"P2 Fire 1"         , BIT_DIGITAL  , System1InputPort1 + 2, "p2 fire 1" },

	{"Reset"             , BIT_DIGITAL  , &System1Reset        , "reset"     },
	{"Service"           , BIT_DIGITAL  , System1InputPort2 + 3, "service"   },
	{"Test"              , BIT_DIGITAL  , System1InputPort2 + 2, "diag"      },
	{"Dip 1"             , BIT_DIPSWITCH, System1Dip + 0       , "dip"       },
	{"Dip 2"             , BIT_DIPSWITCH, System1Dip + 1       , "dip"       },
};

STDINPUTINFO(Flicky)

static struct BurnInputInfo MyheroInputList[] = {
	{"P1 Coin"           , BIT_DIGITAL  , System1InputPort2 + 0, "p1 coin"   },
	{"P1 Start"          , BIT_DIGITAL  , System1InputPort2 + 4, "p1 start"  },
	{"P1 Up"             , BIT_DIGITAL  , System1InputPort0 + 5, "p1 up"     },
	{"P1 Down"           , BIT_DIGITAL  , System1InputPort0 + 4, "p1 down"   },
	{"P1 Left"           , BIT_DIGITAL  , System1InputPort0 + 7, "p1 left"   },
	{"P1 Right"          , BIT_DIGITAL  , System1InputPort0 + 6, "p1 right"  },
	{"P1 Fire 1"         , BIT_DIGITAL  , System1InputPort0 + 1, "p1 fire 1" },
	{"P1 Fire 2"         , BIT_DIGITAL  , System1InputPort0 + 2, "p1 fire 2" },

	{"P2 Coin"           , BIT_DIGITAL  , System1InputPort2 + 1, "p2 coin"   },
	{"P2 Start"          , BIT_DIGITAL  , System1InputPort2 + 5, "p2 start"  },
	{"P2 Up"             , BIT_DIGITAL  , System1InputPort1 + 5, "p2 up"     },
	{"P2 Down"           , BIT_DIGITAL  , System1InputPort1 + 4, "p2 down"   },
	{"P2 Left"           , BIT_DIGITAL  , System1InputPort1 + 7, "p2 left"   },
	{"P2 Right"          , BIT_DIGITAL  , System1InputPort1 + 6, "p2 right"  },
	{"P2 Fire 1"         , BIT_DIGITAL  , System1InputPort1 + 1, "p2 fire 1" },
	{"P2 Fire 2"         , BIT_DIGITAL  , System1InputPort1 + 2, "p2 fire 2" },

	{"Reset"             , BIT_DIGITAL  , &System1Reset        , "reset"     },
	{"Service"           , BIT_DIGITAL  , System1InputPort2 + 3, "service"   },
	{"Test"              , BIT_DIGITAL  , System1InputPort2 + 2, "diag"      },
	{"Dip 1"             , BIT_DIPSWITCH, System1Dip + 0       , "dip"       },
	{"Dip 2"             , BIT_DIPSWITCH, System1Dip + 1       , "dip"       },
};

STDINPUTINFO(Myhero)

static struct BurnInputInfo RegulusInputList[] = {
	{"P1 Coin"           , BIT_DIGITAL  , System1InputPort2 + 0, "p1 coin"   },
	{"P1 Start"          , BIT_DIGITAL  , System1InputPort2 + 4, "p1 start"  },
	{"P1 Up"             , BIT_DIGITAL  , System1InputPort0 + 5, "p1 up"     },
	{"P1 Down"           , BIT_DIGITAL  , System1InputPort0 + 4, "p1 down"   },
	{"P1 Left"           , BIT_DIGITAL  , System1InputPort0 + 7, "p1 left"   },
	{"P1 Right"          , BIT_DIGITAL  , System1InputPort0 + 6, "p1 right"  },
	{"P1 Fire 1"         , BIT_DIGITAL  , System1InputPort0 + 2, "p1 fire 1" },
	{"P1 Fire 2"         , BIT_DIGITAL  , System1InputPort0 + 1, "p1 fire 2" },

	{"P2 Coin"           , BIT_DIGITAL  , System1InputPort2 + 1, "p2 coin"   },
	{"P2 Start"          , BIT_DIGITAL  , System1InputPort2 + 5, "p2 start"  },
	{"P2 Up"             , BIT_DIGITAL  , System1InputPort1 + 5, "p2 up"     },
	{"P2 Down"           , BIT_DIGITAL  , System1InputPort1 + 4, "p2 down"   },
	{"P2 Left"           , BIT_DIGITAL  , System1InputPort1 + 7, "p2 left"   },
	{"P2 Right"          , BIT_DIGITAL  , System1InputPort1 + 6, "p2 right"  },
	{"P2 Fire 1"         , BIT_DIGITAL  , System1InputPort1 + 2, "p2 fire 1" },
	{"P2 Fire 2"         , BIT_DIGITAL  , System1InputPort1 + 1, "p2 fire 2" },

	{"Reset"             , BIT_DIGITAL  , &System1Reset        , "reset"     },
	{"Service"           , BIT_DIGITAL  , System1InputPort2 + 3, "service"   },
	{"Test"              , BIT_DIGITAL  , System1InputPort2 + 2, "diag"      },
	{"Dip 1"             , BIT_DIPSWITCH, System1Dip + 0       , "dip"       },
	{"Dip 2"             , BIT_DIPSWITCH, System1Dip + 1       , "dip"       },
};

STDINPUTINFO(Regulus)

static struct BurnInputInfo SeganinjInputList[] = {
	{"P1 Coin"           , BIT_DIGITAL  , System1InputPort2 + 0, "p1 coin"   },
	{"P1 Start"          , BIT_DIGITAL  , System1InputPort2 + 4, "p1 start"  },
	{"P1 Up"             , BIT_DIGITAL  , System1InputPort0 + 5, "p1 up"     },
	{"P1 Down"           , BIT_DIGITAL  , System1InputPort0 + 4, "p1 down"   },
	{"P1 Left"           , BIT_DIGITAL  , System1InputPort0 + 7, "p1 left"   },
	{"P1 Right"          , BIT_DIGITAL  , System1InputPort0 + 6, "p1 right"  },
	{"P1 Fire 1"         , BIT_DIGITAL  , System1InputPort0 + 0, "p1 fire 1" },
	{"P1 Fire 2"         , BIT_DIGITAL  , System1InputPort0 + 1, "p1 fire 2" },
	{"P1 Fire 3"         , BIT_DIGITAL  , System1InputPort0 + 2, "p1 fire 3" },

	{"P2 Coin"           , BIT_DIGITAL  , System1InputPort2 + 1, "p2 coin"   },
	{"P2 Start"          , BIT_DIGITAL  , System1InputPort2 + 5, "p2 start"  },
	{"P2 Up"             , BIT_DIGITAL  , System1InputPort1 + 5, "p2 up"     },
	{"P2 Down"           , BIT_DIGITAL  , System1InputPort1 + 4, "p2 down"   },
	{"P2 Left"           , BIT_DIGITAL  , System1InputPort1 + 7, "p2 left"   },
	{"P2 Right"          , BIT_DIGITAL  , System1InputPort1 + 6, "p2 right"  },
	{"P2 Fire 1"         , BIT_DIGITAL  , System1InputPort1 + 0, "p2 fire 1" },
	{"P2 Fire 2"         , BIT_DIGITAL  , System1InputPort1 + 1, "p2 fire 2" },
	{"P2 Fire 3"         , BIT_DIGITAL  , System1InputPort1 + 2, "p2 fire 3" },

	{"Reset"             , BIT_DIGITAL  , &System1Reset        , "reset"     },
	{"Service"           , BIT_DIGITAL  , System1InputPort2 + 3, "service"   },
	{"Test"              , BIT_DIGITAL  , System1InputPort2 + 2, "diag"      },
	{"Dip 1"             , BIT_DIPSWITCH, System1Dip + 0       , "dip"       },
	{"Dip 2"             , BIT_DIPSWITCH, System1Dip + 1       , "dip"       },
};

STDINPUTINFO(Seganinj)

static struct BurnInputInfo UpndownInputList[] = {
	{"P1 Coin"           , BIT_DIGITAL  , System1InputPort2 + 0, "p1 coin"   },
	{"P1 Start"          , BIT_DIGITAL  , System1InputPort2 + 4, "p1 start"  },
	{"P1 Up"             , BIT_DIGITAL  , System1InputPort0 + 5, "p1 up"     },
	{"P1 Down"           , BIT_DIGITAL  , System1InputPort0 + 4, "p1 down"   },
	{"P1 Left"           , BIT_DIGITAL  , System1InputPort0 + 7, "p1 left"   },
	{"P1 Right"          , BIT_DIGITAL  , System1InputPort0 + 6, "p1 right"  },
	{"P1 Fire 1"         , BIT_DIGITAL  , System1InputPort0 + 2, "p1 fire 1" },

	{"P2 Coin"           , BIT_DIGITAL  , System1InputPort2 + 1, "p2 coin"   },
	{"P2 Start"          , BIT_DIGITAL  , System1InputPort2 + 5, "p2 start"  },
	{"P2 Up"             , BIT_DIGITAL  , System1InputPort1 + 5, "p2 up"     },
	{"P2 Down"           , BIT_DIGITAL  , System1InputPort1 + 4, "p2 down"   },
	{"P2 Left"           , BIT_DIGITAL  , System1InputPort1 + 7, "p2 left"   },
	{"P2 Right"          , BIT_DIGITAL  , System1InputPort1 + 6, "p2 right"  },
	{"P2 Fire 1"         , BIT_DIGITAL  , System1InputPort1 + 2, "p2 fire 1" },

	{"Reset"             , BIT_DIGITAL  , &System1Reset        , "reset"     },
	{"Service"           , BIT_DIGITAL  , System1InputPort2 + 3, "service"   },
	{"Test"              , BIT_DIGITAL  , System1InputPort2 + 2, "diag"      },
	{"Dip 1"             , BIT_DIPSWITCH, System1Dip + 0       , "dip"       },
	{"Dip 2"             , BIT_DIPSWITCH, System1Dip + 1       , "dip"       },
};

STDINPUTINFO(Upndown)

static struct BurnInputInfo WboyInputList[] = {
	{"P1 Coin"           , BIT_DIGITAL  , System1InputPort2 + 0, "p1 coin"   },
	{"P1 Start"          , BIT_DIGITAL  , System1InputPort2 + 4, "p1 start"  },
	{"P1 Left"           , BIT_DIGITAL  , System1InputPort0 + 7, "p1 left"   },
	{"P1 Right"          , BIT_DIGITAL  , System1InputPort0 + 6, "p1 right"  },
	{"P1 Fire 1"         , BIT_DIGITAL  , System1InputPort0 + 1, "p1 fire 1" },
	{"P1 Fire 2"         , BIT_DIGITAL  , System1InputPort0 + 2, "p1 fire 2" },

	{"P2 Coin"           , BIT_DIGITAL  , System1InputPort2 + 1, "p2 coin"   },
	{"P2 Start"          , BIT_DIGITAL  , System1InputPort2 + 5, "p2 start"  },
	{"P2 Left"           , BIT_DIGITAL  , System1InputPort1 + 7, "p2 left"   },
	{"P2 Right"          , BIT_DIGITAL  , System1InputPort1 + 6, "p2 right"  },
	{"P2 Fire 1"         , BIT_DIGITAL  , System1InputPort1 + 1, "p2 fire 1" },
	{"P2 Fire 2"         , BIT_DIGITAL  , System1InputPort1 + 2, "p2 fire 2" },

	{"Reset"             , BIT_DIGITAL  , &System1Reset        , "reset"     },
	{"Service"           , BIT_DIGITAL  , System1InputPort2 + 3, "service"   },
	{"Test"              , BIT_DIGITAL  , System1InputPort2 + 2, "diag"      },
	{"Dip 1"             , BIT_DIPSWITCH, System1Dip + 0       , "dip"       },
	{"Dip 2"             , BIT_DIPSWITCH, System1Dip + 1       , "dip"       },
};

STDINPUTINFO(Wboy)

static struct BurnInputInfo WmatchInputList[] = {
	{"P1 Coin"           , BIT_DIGITAL  , System1InputPort2 + 0, "p1 coin"   },
	{"P1 Start"          , BIT_DIGITAL  , System1InputPort2 + 4, "p1 start"  },
	{"P1 Left Up"        , BIT_DIGITAL  , System1InputPort0 + 5, "p1 up"     },
	{"P1 Left Down"      , BIT_DIGITAL  , System1InputPort0 + 4, "p1 down"   },
	{"P1 Left Left"      , BIT_DIGITAL  , System1InputPort0 + 7, "p1 left"   },
	{"P1 Left Right"     , BIT_DIGITAL  , System1InputPort0 + 6, "p1 right"  },
	{"P1 Right Up"       , BIT_DIGITAL  , System1InputPort0 + 1, "p1 fire 1" },
	{"P1 Right Down"     , BIT_DIGITAL  , System1InputPort0 + 0, "p1 fire 2" },
	{"P1 Right Left"     , BIT_DIGITAL  , System1InputPort0 + 3, "p1 fire 3" },
	{"P1 Right Right"    , BIT_DIGITAL  , System1InputPort0 + 2, "p1 fire 4" },
	{"P1 Fire 1"         , BIT_DIGITAL  , System1InputPort2 + 6, "p1 fire 5" },

	{"P2 Coin"           , BIT_DIGITAL  , System1InputPort2 + 1, "p2 coin"   },
	{"P2 Start"          , BIT_DIGITAL  , System1InputPort2 + 5, "p2 start"  },
	{"P2 Left Up"        , BIT_DIGITAL  , System1InputPort1 + 5, "p2 up"     },
	{"P2 Left Down"      , BIT_DIGITAL  , System1InputPort1 + 4, "p2 down"   },
	{"P2 Left Left"      , BIT_DIGITAL  , System1InputPort1 + 7, "p2 left"   },
	{"P2 Left Right"     , BIT_DIGITAL  , System1InputPort1 + 6, "p2 right"  },
	{"P2 Right Up"       , BIT_DIGITAL  , System1InputPort1 + 1, "p2 fire 1" },
	{"P2 Right Down"     , BIT_DIGITAL  , System1InputPort1 + 0, "p2 fire 2" },
	{"P2 Right Left"     , BIT_DIGITAL  , System1InputPort1 + 3, "p2 fire 3" },
	{"P2 Right Right"    , BIT_DIGITAL  , System1InputPort1 + 2, "p2 fire 4" },
	{"P2 Fire 1"         , BIT_DIGITAL  , System1InputPort2 + 7, "p2 fire 5" },

	{"Reset"             , BIT_DIGITAL  , &System1Reset        , "reset"     },
	{"Service"           , BIT_DIGITAL  , System1InputPort2 + 3, "service"   },
	{"Test"              , BIT_DIGITAL  , System1InputPort2 + 2, "diag"      },
	{"Dip 1"             , BIT_DIPSWITCH, System1Dip + 0       , "dip"       },
	{"Dip 2"             , BIT_DIPSWITCH, System1Dip + 1       , "dip"       },
};

STDINPUTINFO(Wmatch)

static struct BurnInputInfo ChopliftInputList[] = {
	{"P1 Coin"           , BIT_DIGITAL  , System1InputPort2 + 0, "p1 coin"   },
	{"P1 Start"          , BIT_DIGITAL  , System1InputPort2 + 4, "p1 start"  },
	{"P1 Up"             , BIT_DIGITAL  , System1InputPort0 + 5, "p1 up"     },
	{"P1 Down"           , BIT_DIGITAL  , System1InputPort0 + 4, "p1 down"   },
	{"P1 Left"           , BIT_DIGITAL  , System1InputPort0 + 7, "p1 left"   },
	{"P1 Right"          , BIT_DIGITAL  , System1InputPort0 + 6, "p1 right"  },
	{"P1 Fire 1"         , BIT_DIGITAL  , System1InputPort0 + 1, "p1 fire 1" },
	{"P1 Fire 2"         , BIT_DIGITAL  , System1InputPort0 + 2, "p1 fire 2" },

	{"P2 Coin"           , BIT_DIGITAL  , System1InputPort2 + 1, "p2 coin"   },
	{"P2 Start"          , BIT_DIGITAL  , System1InputPort2 + 5, "p2 start"  },
	{"P2 Up"             , BIT_DIGITAL  , System1InputPort1 + 5, "p2 up"     },
	{"P2 Down"           , BIT_DIGITAL  , System1InputPort1 + 4, "p2 down"   },
	{"P2 Left"           , BIT_DIGITAL  , System1InputPort1 + 7, "p2 left"   },
	{"P2 Right"          , BIT_DIGITAL  , System1InputPort1 + 6, "p2 right"  },
	{"P2 Fire 1"         , BIT_DIGITAL  , System1InputPort1 + 1, "p2 fire 1" },
	{"P2 Fire 2"         , BIT_DIGITAL  , System1InputPort1 + 2, "p2 fire 2" },

	{"Reset"             , BIT_DIGITAL  , &System1Reset        , "reset"     },
	{"Service"           , BIT_DIGITAL  , System1InputPort2 + 3, "service"   },
	{"Test"              , BIT_DIGITAL  , System1InputPort2 + 2, "diag"      },
	{"Dip 1"             , BIT_DIPSWITCH, System1Dip + 0       , "dip"       },
	{"Dip 2"             , BIT_DIPSWITCH, System1Dip + 1       , "dip"       },
};

STDINPUTINFO(Choplift)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo ShtngmstInputList[] = {
	{"P1 Coin"           , BIT_DIGITAL  , System1InputPort2 + 0, "p1 coin"   },
	{"P1 Start"          , BIT_DIGITAL  , System1InputPort2 + 4, "p1 start"  },

	{"P1 Fire 1"         , BIT_DIGITAL  , System1InputPort3 + 6, "p1 fire 1" },
	A("P1 Gun X"		 , BIT_ANALOG_REL, &Analog[0],    "mouse x-axis"	),
	A("P1 Gun Y"		 , BIT_ANALOG_REL, &Analog[1],    "mouse y-axis"	),

	{"P2 Coin"           , BIT_DIGITAL  , System1InputPort2 + 1, "p2 coin"   },
	{"P2 Start"          , BIT_DIGITAL  , System1InputPort2 + 5, "p2 start"  },

	{"Reset"             , BIT_DIGITAL  , &System1Reset        , "reset"     },
	{"Service"           , BIT_DIGITAL  , System1InputPort2 + 3, "service"   },
	{"Test"              , BIT_DIGITAL  , System1InputPort2 + 2, "diag"      },
	{"Dip 1"             , BIT_DIPSWITCH, System1Dip + 0       , "dip"       },
	{"Dip 2"             , BIT_DIPSWITCH, System1Dip + 1       , "dip"       },
	{"Dip 3"             , BIT_DIPSWITCH, System1Dip + 2       , "dip"       },
};
#undef A
STDINPUTINFO(Shtngmst)

static struct BurnInputInfo UfosensiInputList[] = {
	{"P1 Coin"           , BIT_DIGITAL  , System1InputPort2 + 0, "p1 coin"   },
	{"P1 Start"          , BIT_DIGITAL  , System1InputPort2 + 4, "p1 start"  },
	{"P1 Up"             , BIT_DIGITAL  , System1InputPort0 + 5, "p1 up"     },
	{"P1 Down"           , BIT_DIGITAL  , System1InputPort0 + 4, "p1 down"   },
	{"P1 Left"           , BIT_DIGITAL  , System1InputPort0 + 7, "p1 left"   },
	{"P1 Right"          , BIT_DIGITAL  , System1InputPort0 + 6, "p1 right"  },
	{"P1 Fire 1"         , BIT_DIGITAL  , System1InputPort0 + 2, "p1 fire 1" },
	{"P1 Fire 2"         , BIT_DIGITAL  , System1InputPort0 + 1, "p1 fire 2" },
	{"P1 Fire 3"         , BIT_DIGITAL  , System1InputPort0 + 0, "p1 fire 3" },

	{"P2 Coin"           , BIT_DIGITAL  , System1InputPort2 + 1, "p2 coin"   },
	{"P2 Start"          , BIT_DIGITAL  , System1InputPort2 + 5, "p2 start"  },
	{"P2 Up"             , BIT_DIGITAL  , System1InputPort1 + 5, "p2 up"     },
	{"P2 Down"           , BIT_DIGITAL  , System1InputPort1 + 4, "p2 down"   },
	{"P2 Left"           , BIT_DIGITAL  , System1InputPort1 + 7, "p2 left"   },
	{"P2 Right"          , BIT_DIGITAL  , System1InputPort1 + 6, "p2 right"  },
	{"P2 Fire 1"         , BIT_DIGITAL  , System1InputPort1 + 2, "p2 fire 1" },
	{"P2 Fire 2"         , BIT_DIGITAL  , System1InputPort1 + 1, "p2 fire 2" },
	{"P2 Fire 3"         , BIT_DIGITAL  , System1InputPort1 + 0, "p2 fire 3" },

	{"Reset"             , BIT_DIGITAL  , &System1Reset        , "reset"     },
	{"Service"           , BIT_DIGITAL  , System1InputPort2 + 3, "service"   },
	{"Test"              , BIT_DIGITAL  , System1InputPort2 + 2, "diag"      },
	{"Dip 1"             , BIT_DIPSWITCH, System1Dip + 0       , "dip"       },
	{"Dip 2"             , BIT_DIPSWITCH, System1Dip + 1       , "dip"       },

};

STDINPUTINFO(Ufosensi)

static inline void System1ClearOpposites(UINT8* nJoystickInputs)
{
	if ((*nJoystickInputs & 0x30) == 0x00) {
		*nJoystickInputs |= 0x30;
	}
	if ((*nJoystickInputs & 0xc0) == 0x00) {
		*nJoystickInputs |= 0xc0;
	}
}

static inline void System1MakeInputs()
{
	// Reset Inputs
	System1Input[0] = System1Input[1] = System1Input[2] = 0xff;

	// Compile Digital Inputs
	for (INT32 i = 0; i < 8; i++) {
		System1Input[0] ^= (System1InputPort0[i] & 1) << i;
		System1Input[1] ^= (System1InputPort1[i] & 1) << i;
		System1Input[2] ^= (System1InputPort2[i] & 1) << i;
	}

	// Clear Opposites
	System1ClearOpposites(&System1Input[0]);
	System1ClearOpposites(&System1Input[1]);
}

static inline void ShtngmstMakeInputs()
{
	// Reset Inputs
	System1Input[0] = System1Input[1] = System1Input[2] = System1Input[3] = 0xff;

	// Compile Digital Inputs
	for (INT32 i = 0; i < 8; i++) {
		System1Input[0] ^= (System1InputPort0[i] & 1) << i;
		System1Input[1] ^= (System1InputPort1[i] & 1) << i;
		System1Input[2] ^= (System1InputPort2[i] & 1) << i;
		System1Input[3] ^= (System1InputPort3[i] & 1) << i;
	}

	{ // gun trigger pulser: trigger must pulse 1 frame for HS entry to work
		if (sht_trigger && ~System1Input[3] & 0x40) {
			System1Input[3] |= 0x40;
		}
		sht_trigger = System1InputPort3[6];
	}

	BurnGunMakeInputs(0, Analog[0], Analog[1]);
}

static inline void BlockgalMakeInputs()
{
	System1Input[2] = 0xff;

	for (INT32 i = 0; i < 8; i++) {
		System1Input[2] ^= (System1InputPort2[i] & 1) << i;
	}

	// blockgal: the game uses an asynchronous dial.
	// player could die @ far-right side of screen with value 0xff
	// but the game re-starts with the paddle centered.
	// the game doesn't always get it right - though - moving very fast
	// (f.ex, with mouse) and changing direction at a corner can cause the
	// player to get "stuck" for a few frames

	BurnTrackballConfig(0, AXIS_REVERSED, AXIS_REVERSED);
	BurnTrackballFrame(0, Analog[0], Analog[1], 0x01, 0x1f);
	BurnTrackballUpdate(0);

	System1Input[0] = BurnTrackballRead(0);
	System1Input[1] = BurnTrackballRead(1);
}

#define SYSTEM1_COINAGE(dipval)								\
	{0   , 0xfe, 0   , 15   , "Coin A"                },				\
	{dipval, 0x01, 0x0f, 0x07, "4 Coins 1 Credit"       },				\
	{dipval, 0x01, 0x0f, 0x08, "3 Coins 1 Credit"       },				\
	{dipval, 0x01, 0x0f, 0x09, "2 Coins 1 Credit"       },				\
	{dipval, 0x01, 0x0f, 0x05, "2 Coins 1 Credit 4/2 5/3 6/4"},			\
	{dipval, 0x01, 0x0f, 0x04, "2 Coins 1 Credit 4/3"   },				\
	{dipval, 0x01, 0x0f, 0x0f, "1 Coin  1 Credit"       },				\
	{dipval, 0x01, 0x0f, 0x03, "1 Coin  1 Credit 5/6"   },				\
	{dipval, 0x01, 0x0f, 0x02, "1 Coin  1 Credit 4/5"   },				\
	{dipval, 0x01, 0x0f, 0x01, "1 Coin  1 Credit 2/3"   },				\
	{dipval, 0x01, 0x0f, 0x06, "2 Coins 3 Credits"      },				\
	{dipval, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"      },				\
	{dipval, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"      },				\
	{dipval, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"      },				\
	{dipval, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"      },				\
	{dipval, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"      },				\
											\
	{0   , 0xfe, 0   , 15   , "Coin B"                },				\
	{dipval, 0x01, 0xf0, 0x70, "4 Coins 1 Credit"       },				\
	{dipval, 0x01, 0xf0, 0x80, "3 Coins 1 Credit"       },				\
	{dipval, 0x01, 0xf0, 0x90, "2 Coins 1 Credit"       },				\
	{dipval, 0x01, 0xf0, 0x50, "2 Coins 1 Credit 4/2 5/3 6/4"},			\
	{dipval, 0x01, 0xf0, 0x40, "2 Coins 1 Credit 4/3"   },				\
	{dipval, 0x01, 0xf0, 0xf0, "1 Coin  1 Credit"       },				\
	{dipval, 0x01, 0xf0, 0x30, "1 Coin  1 Credit 5/6"   },				\
	{dipval, 0x01, 0xf0, 0x20, "1 Coin  1 Credit 4/5"   },				\
	{dipval, 0x01, 0xf0, 0x10, "1 Coin  1 Credit 2/3"   },				\
	{dipval, 0x01, 0xf0, 0x60, "2 Coins 3 Credits"      },				\
	{dipval, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"      },				\
	{dipval, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"      },				\
	{dipval, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"      },				\
	{dipval, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"      },				\
	{dipval, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"      },

static struct BurnDIPInfo FourdwarrioDIPList[]=
{
	// Default Values
	{0x13, 0xff, 0xff, 0xff, NULL                     },
	{0x14, 0xff, 0xff, 0xfe, NULL                     },

	// Dip 1
	SYSTEM1_COINAGE(0x13)

	// Dip 2
	{0   , 0xfe, 0   , 2   , "Cabinet"                },
	{0x14, 0x01, 0x01, 0x00, "Upright"                },
	{0x14, 0x01, 0x01, 0x01, "Cocktail"               },

	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x14, 0x01, 0x06, 0x06, "3"                      },
	{0x14, 0x01, 0x06, 0x04, "4"                      },
	{0x14, 0x01, 0x06, 0x02, "5"                      },
	{0x14, 0x01, 0x06, 0x00, "Infinite"               },

	{0   , 0xfe, 0   , 8   , "Bonus Life"             },
	{0x14, 0x01, 0x38, 0x38, "30k"                    },
	{0x14, 0x01, 0x38, 0x30, "40k"                    },
	{0x14, 0x01, 0x38, 0x28, "50k"                    },
	{0x14, 0x01, 0x38, 0x20, "60k"                    },
	{0x14, 0x01, 0x38, 0x18, "70k"                    },
	{0x14, 0x01, 0x38, 0x10, "80k"                    },
	{0x14, 0x01, 0x38, 0x08, "90k"                    },
	{0x14, 0x01, 0x38, 0x00, "None"                   },

	{0   , 0xfe, 0   , 2   , "Difficulty"             },
	{0x14, 0x01, 0x40, 0x40, "Easy"                   },
	{0x14, 0x01, 0x40, 0x00, "Hard"                   },
};

STDDIPINFO(Fourdwarrio)

static struct BurnDIPInfo BlockgalDIPList[]=
{
	DIP_OFFSET(0x0a)
	// Default Values
	{0x00, 0xff, 0xff, 0xd7, NULL                     },
	{0x01, 0xff, 0xff, 0xff, NULL                     },

	// Dip 1
	{0   , 0xfe, 0   , 2   , "Cabinet"                },
	{0x00, 0x01, 0x01, 0x00, "Upright"                },
	{0x00, 0x01, 0x01, 0x01, "Cocktail"               },

	{0   , 0xfe, 0   , 2   , "Demo Sounds"            },
	{0x00, 0x01, 0x02, 0x00, "Off"                    },
	{0x00, 0x01, 0x02, 0x02, "On"                     },

	{0   , 0xfe, 0   , 2   , "Lives"                  },
	{0x00, 0x01, 0x08, 0x08, "2"                      },
	{0x00, 0x01, 0x08, 0x00, "3"                      },

	{0   , 0xfe, 0   , 2   , "Bonus Life"             },
	{0x00, 0x01, 0x10, 0x10, "10k 30k 60k 100k 150k"  },
	{0x00, 0x01, 0x10, 0x00, "30k 50k 100k 200k 300k" },

	{0   , 0xfe, 0   , 2   , "Allow Continue"         },
	{0x00, 0x01, 0x20, 0x20, "Off"                    },
	{0x00, 0x01, 0x20, 0x00, "On"                     },

	{0   , 0xfe, 0   , 2   , "Service Mode"           },
	{0x00, 0x01, 0x80, 0x80, "Off"                    },
	{0x00, 0x01, 0x80, 0x00, "On"                     },

	// Dip 2
	SYSTEM1_COINAGE(0x01)
};

STDDIPINFO(Blockgal)

static struct BurnDIPInfo BrainDIPList[]=
{
	// Default Values
	{0x13, 0xff, 0xff, 0xff, NULL                     },
	{0x14, 0xff, 0xff, 0xfc, NULL                     },

	// Dip 1
	SYSTEM1_COINAGE(0x13)

	// Dip 2
	{0   , 0xfe, 0   , 2   , "Cabinet"                },
	{0x14, 0x01, 0x01, 0x00, "Upright"                },
	{0x14, 0x01, 0x01, 0x01, "Cocktail"               },

	{0   , 0xfe, 0   , 2   , "Demo Sounds"            },
	{0x14, 0x01, 0x02, 0x02, "Off"                    },
	{0x14, 0x01, 0x02, 0x00, "On"                     },

	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x14, 0x01, 0x0c, 0x0c, "3"                      },
	{0x14, 0x01, 0x0c, 0x08, "4"                      },
	{0x14, 0x01, 0x0c, 0x04, "5"                      },
	{0x14, 0x01, 0x0c, 0x00, "Infinite"               },
};

STDDIPINFO(Brain)

static struct BurnDIPInfo BullfgtDIPList[]=
{
	// Default Values
	{0x13, 0xff, 0xff, 0xff, NULL                     },
	{0x14, 0xff, 0xff, 0xfc, NULL                     },

	// Dip 1
	SYSTEM1_COINAGE(0x13)

	// Dip 2
	{0   , 0xfe, 0   , 2   , "Cabinet"                },
	{0x14, 0x01, 0x01, 0x00, "Upright"                },
	{0x14, 0x01, 0x01, 0x01, "Cocktail"               },

	{0   , 0xfe, 0   , 2   , "Demo Sounds"            },
	{0x14, 0x01, 0x02, 0x02, "Off"                    },
	{0x14, 0x01, 0x02, 0x00, "On"                     },

	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x14, 0x01, 0x0c, 0x0c, "3"                      },
	{0x14, 0x01, 0x0c, 0x08, "4"                      },
	{0x14, 0x01, 0x0c, 0x04, "5"                      },
	{0x14, 0x01, 0x0c, 0x00, "Infinite"               },

	{0   , 0xfe, 0   , 4   , "Bonus Life"             },
	{0x14, 0x01, 0x30, 0x30, "30k"                    },
	{0x14, 0x01, 0x30, 0x20, "50k"                    },
	{0x14, 0x01, 0x30, 0x10, "70k"                    },
	{0x14, 0x01, 0x30, 0x00, "None"                   },
};

STDDIPINFO(Bullfgt)

static struct BurnDIPInfo FlickyDIPList[]=
{
	// Default Values
	{0x0d, 0xff, 0xff, 0xff, NULL                     },
	{0x0e, 0xff, 0xff, 0xfe, NULL                     },

	// Dip 1
	SYSTEM1_COINAGE(0x0d)

	// Dip 2
	{0   , 0xfe, 0   , 2   , "Cabinet"                },
	{0x0e, 0x01, 0x01, 0x00, "Upright"                },
	{0x0e, 0x01, 0x01, 0x01, "Cocktail"               },

	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x0e, 0x01, 0x0c, 0x0c, "3"                      },
	{0x0e, 0x01, 0x0c, 0x08, "4"                      },
	{0x0e, 0x01, 0x0c, 0x04, "5"                      },
	{0x0e, 0x01, 0x0c, 0x00, "Infinite"               },

	{0   , 0xfe, 0   , 4   , "Bonus Life"             },
	{0x0e, 0x01, 0x30, 0x30, "30k  80k 160k"          },
	{0x0e, 0x01, 0x30, 0x20, "30k 100k 200k"          },
	{0x0e, 0x01, 0x30, 0x10, "40k 120k 240k"          },
	{0x0e, 0x01, 0x30, 0x00, "40k 140k 280k"          },

	{0   , 0xfe, 0   , 2   , "Difficulty"             },
	{0x0e, 0x01, 0x40, 0x40, "Easy"                   },
	{0x0e, 0x01, 0x40, 0x00, "Hard"                   },
};

STDDIPINFO(Flicky)

static struct BurnDIPInfo FlickybDIPList[]=
{
	// Default Values
	{0x0d, 0xff, 0xff, 0xff, NULL                     },
	{0x0e, 0xff, 0xff, 0xfe, NULL                     },

	// Dip 1
	SYSTEM1_COINAGE(0x0d)

	// Dip 2
	{0   , 0xfe, 0   , 2   , "Cabinet"                },
	{0x0e, 0x01, 0x01, 0x00, "Upright"                },
	{0x0e, 0x01, 0x01, 0x01, "Cocktail"               },

	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x0e, 0x01, 0x0c, 0x0c, "1"                      },
	{0x0e, 0x01, 0x0c, 0x08, "2"                      },
	{0x0e, 0x01, 0x0c, 0x04, "3"                      },
	{0x0e, 0x01, 0x0c, 0x00, "Infinite"               },

	{0   , 0xfe, 0   , 4   , "Bonus Life"             },
	{0x0e, 0x01, 0x30, 0x30, "30k  80k 160k"          },
	{0x0e, 0x01, 0x30, 0x20, "30k 100k 200k"          },
	{0x0e, 0x01, 0x30, 0x10, "40k 120k 240k"          },
	{0x0e, 0x01, 0x30, 0x00, "40k 140k 280k"          },

	{0   , 0xfe, 0   , 2   , "Difficulty"             },
	{0x0e, 0x01, 0x40, 0x40, "Easy"                   },
	{0x0e, 0x01, 0x40, 0x00, "Hard"                   },
};

STDDIPINFO(Flickyb)

static struct BurnDIPInfo Flickys1DIPList[]=
{
	// Default Values
	{0x0d, 0xff, 0xff, 0xff, NULL                     },
	{0x0e, 0xff, 0xff, 0x78, NULL                     },

	// Dip 1
	SYSTEM1_COINAGE(0x0d)

	// Dip 2
	{0   , 0xfe, 0   , 2   , "Cabinet"                },
	{0x0e, 0x01, 0x01, 0x00, "Upright"                },
	{0x0e, 0x01, 0x01, 0x01, "Cocktail"               },

	{0   , 0xfe, 0   , 2   , "Demo Sounds"            },
	{0x0e, 0x01, 0x02, 0x02, "Off"                    },
	{0x0e, 0x01, 0x02, 0x00, "On"                     },

	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x0e, 0x01, 0x0c, 0x0c, "2"                      },
	{0x0e, 0x01, 0x0c, 0x08, "3"                      },
	{0x0e, 0x01, 0x0c, 0x04, "4"                      },
	{0x0e, 0x01, 0x0c, 0x00, "5 (Infinite)"           },

	{0   , 0xfe, 0   , 4   , "Bonus Life"             },
	{0x0e, 0x01, 0x30, 0x30, "80000"                  },
	{0x0e, 0x01, 0x30, 0x20, "160000"                 },
	{0x0e, 0x01, 0x30, 0x10, "240000"                 },
	{0x0e, 0x01, 0x30, 0x00, "320000"                 },

	{0   , 0xfe, 0   , 2   , "Difficulty"             },
	{0x0e, 0x01, 0x40, 0x40, "Easy"                   },
	{0x0e, 0x01, 0x40, 0x00, "Hard"                   },
};

STDDIPINFO(Flickys1)

static struct BurnDIPInfo GardiaDIPList[]=
{
	// Default Values
	{0x13, 0xff, 0xff, 0xff, NULL                     },
	{0x14, 0xff, 0xff, 0x7c, NULL                     },

	// Dip 1
	SYSTEM1_COINAGE(0x13)

	// Dip 2
	{0   , 0xfe, 0   , 2   , "Cabinet"                },
	{0x14, 0x01, 0x01, 0x00, "Upright"                },
	{0x14, 0x01, 0x01, 0x01, "Cocktail"               },

	{0   , 0xfe, 0   , 2   , "Demo Sounds"            },
	{0x14, 0x01, 0x02, 0x02, "Off"                    },
	{0x14, 0x01, 0x02, 0x00, "On"                     },

	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x14, 0x01, 0x0c, 0x0c, "3"                      },
	{0x14, 0x01, 0x0c, 0x08, "4"                      },
	{0x14, 0x01, 0x0c, 0x04, "5"                      },
	{0x14, 0x01, 0x0c, 0x00, "Infinite"               },

	{0   , 0xfe, 0   , 4   , "Bonus Life"             },
	{0x14, 0x01, 0x30, 0x30, " 5k 20k 30k"            },
	{0x14, 0x01, 0x30, 0x20, "10k 25k 50k"            },
	{0x14, 0x01, 0x30, 0x10, "15k 30k 60k"            },
	{0x14, 0x01, 0x30, 0x00, "None"                   },

	{0   , 0xfe, 0   , 2   , "Difficulty"             },
	{0x14, 0x01, 0x40, 0x40, "Easy"                   },
	{0x14, 0x01, 0x40, 0x00, "Hard"                   },
};

STDDIPINFO(Gardia)

static struct BurnDIPInfo HvymetalDIPList[]=
{
	// Default Values
	{0x13, 0xff, 0xff, 0xff, NULL                     },
	{0x14, 0xff, 0xff, 0xfd, NULL                     },

	// Dip 1
	SYSTEM1_COINAGE(0x13)

	// Dip 2
	{0   , 0xfe, 0   , 2   , "Demo Sounds"            },
	{0x14, 0x01, 0x02, 0x02, "Off"                    },
	{0x14, 0x01, 0x02, 0x00, "On"                     },

	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x14, 0x01, 0x0c, 0x0c, "3"                      },
	{0x14, 0x01, 0x0c, 0x08, "4"                      },
	{0x14, 0x01, 0x0c, 0x04, "5"                      },
	{0x14, 0x01, 0x0c, 0x00, "Infinite"               },

	{0   , 0xfe, 0   , 4   , "Bonus Life"             },
	{0x14, 0x01, 0x30, 0x30, "50k, 100k"              },
	{0x14, 0x01, 0x30, 0x20, "60k, 120k"              },
	{0x14, 0x01, 0x30, 0x10, "70k, 150k"              },
	{0x14, 0x01, 0x30, 0x00, "100k"                   },

	{0   , 0xfe, 0   , 2   , "Difficulty"             },
	{0x14, 0x01, 0x40, 0x40, "Easy"                   },
	{0x14, 0x01, 0x40, 0x00, "Hard"                   },

	{0   , 0xfe, 0   , 2   , "Allow Continue"         },
	{0x14, 0x01, 0x80, 0x80, "Off"                    },
	{0x14, 0x01, 0x80, 0x00, "On"                     },
};

STDDIPINFO(Hvymetal)

static struct BurnDIPInfo ImsorryDIPList[]=
{
	// Default Values
	{0x13, 0xff, 0xff, 0xff, NULL                     },
	{0x14, 0xff, 0xff, 0xfc, NULL                     },

	// Dip 1
	SYSTEM1_COINAGE(0x13)

	// Dip 2
	{0   , 0xfe, 0   , 2   , "Cabinet"                },
	{0x14, 0x01, 0x01, 0x00, "Upright"                },
	{0x14, 0x01, 0x01, 0x01, "Cocktail"               },

	{0   , 0xfe, 0   , 2   , "Demo Sounds"            },
	{0x14, 0x01, 0x02, 0x02, "Off"                    },
	{0x14, 0x01, 0x02, 0x00, "On"                     },

	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x14, 0x01, 0x0c, 0x0c, "3"                      },
	{0x14, 0x01, 0x0c, 0x08, "4"                      },
	{0x14, 0x01, 0x0c, 0x04, "5"                      },
	{0x14, 0x01, 0x0c, 0x00, "Infinite"               },

	{0   , 0xfe, 0   , 4   , "Bonus Life"             },
	{0x14, 0x01, 0x30, 0x30, "30k"                    },
	{0x14, 0x01, 0x30, 0x20, "40k"                    },
	{0x14, 0x01, 0x30, 0x10, "50k"                    },
	{0x14, 0x01, 0x30, 0x00, "None"                   },
};

STDDIPINFO(Imsorry)

static struct BurnDIPInfo MrvikingDIPList[]=
{
	// Default Values
	{0x13, 0xff, 0xff, 0xff, NULL                     },
	{0x14, 0xff, 0xff, 0x7c, NULL                     },

	// Dip 1
	SYSTEM1_COINAGE(0x13)

	// Dip 2
	{0   , 0xfe, 0   , 2   , "Cabinet"                },
	{0x14, 0x01, 0x01, 0x00, "Upright"                },
	{0x14, 0x01, 0x01, 0x01, "Cocktail"               },

	{0   , 0xfe, 0   , 2   , "Maximum Credit"         },
	{0x14, 0x01, 0x02, 0x02, "9"                      },
	{0x14, 0x01, 0x02, 0x00, "99"                     },

	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x14, 0x01, 0x0c, 0x0c, "3"                      },
	{0x14, 0x01, 0x0c, 0x08, "4"                      },
	{0x14, 0x01, 0x0c, 0x04, "5"                      },
	{0x14, 0x01, 0x0c, 0x00, "Infinite"               },

	{0   , 0xfe, 0   , 4   , "Bonus Life"             },
	{0x14, 0x01, 0x30, 0x30, "10k, 30k then every 30k"},
	{0x14, 0x01, 0x30, 0x20, "20k, 40k then every 30k"},
	{0x14, 0x01, 0x30, 0x10, "30k, then every 30k"    },
	{0x14, 0x01, 0x30, 0x00, "40k, then every 30k"    },

	{0   , 0xfe, 0   , 2   , "Difficulty"             },
	{0x14, 0x01, 0x40, 0x40, "Easy"                   },
	{0x14, 0x01, 0x40, 0x00, "Hard"                   },
};

STDDIPINFO(Mrviking)

static struct BurnDIPInfo MrvikngjDIPList[]=
{
	// Default Values
	{0x13, 0xff, 0xff, 0xff, NULL                     },
	{0x14, 0xff, 0xff, 0x7c, NULL                     },

	// Dip 1
	SYSTEM1_COINAGE(0x13)

	// Dip 2
	{0   , 0xfe, 0   , 2   , "Cabinet"                },
	{0x14, 0x01, 0x01, 0x00, "Upright"                },
	{0x14, 0x01, 0x01, 0x01, "Cocktail"               },

	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x14, 0x01, 0x0c, 0x0c, "3"                      },
	{0x14, 0x01, 0x0c, 0x08, "4"                      },
	{0x14, 0x01, 0x0c, 0x04, "5"                      },
	{0x14, 0x01, 0x0c, 0x00, "Infinite"               },

	{0   , 0xfe, 0   , 4   , "Bonus Life"             },
	{0x14, 0x01, 0x30, 0x30, "10k, 30k then every 30k"},
	{0x14, 0x01, 0x30, 0x20, "20k, 40k then every 30k"},
	{0x14, 0x01, 0x30, 0x10, "30k, then every 30k"    },
	{0x14, 0x01, 0x30, 0x00, "40k, then every 30k"    },

	{0   , 0xfe, 0   , 2   , "Difficulty"             },
	{0x14, 0x01, 0x40, 0x40, "Easy"                   },
	{0x14, 0x01, 0x40, 0x00, "Hard"                   },
};

STDDIPINFO(Mrvikngj)

static struct BurnDIPInfo MyheroDIPList[]=
{
	// Default Values
	{0x13, 0xff, 0xff, 0xff, NULL                     },
	{0x14, 0xff, 0xff, 0xfc, NULL                     },

	// Dip 1
	SYSTEM1_COINAGE(0x13)

	// Dip 2
	{0   , 0xfe, 0   , 2   , "Cabinet"                },
	{0x14, 0x01, 0x01, 0x00, "Upright"                },
	{0x14, 0x01, 0x01, 0x01, "Cocktail"               },

	{0   , 0xfe, 0   , 2   , "Demo Sounds"            },
	{0x14, 0x01, 0x02, 0x02, "Off"                    },
	{0x14, 0x01, 0x02, 0x00, "On"                     },

	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x14, 0x01, 0x0c, 0x0c, "3"                      },
	{0x14, 0x01, 0x0c, 0x08, "4"                      },
	{0x14, 0x01, 0x0c, 0x04, "5"                      },
	{0x14, 0x01, 0x0c, 0x00, "Infinite"               },

	{0   , 0xfe, 0   , 4   , "Bonus Life"             },
	{0x14, 0x01, 0x30, 0x30, "30k"                    },
	{0x14, 0x01, 0x30, 0x20, "50k"                    },
	{0x14, 0x01, 0x30, 0x10, "70k"                    },
	{0x14, 0x01, 0x30, 0x00, "90k"                    },

	{0   , 0xfe, 0   , 2   , "Difficulty"             },
	{0x14, 0x01, 0x40, 0x40, "Easy"                   },
	{0x14, 0x01, 0x40, 0x00, "Hard"                   },
};

STDDIPINFO(Myhero)

static struct BurnDIPInfo NobbDIPList[]=
{
	// Default Values
	{0x13, 0xff, 0xff, 0x2f, NULL                     },
	{0x14, 0xff, 0xff, 0xff, NULL                     },

	// Dip 1
	{0   , 0xfe, 0   , 4   , "Coin A"                 },
	{0x13, 0x01, 0x03, 0x00, "2 Coins 1 Credit"       },
	{0x13, 0x01, 0x03, 0x03, "1 Coin  1 Credit"       },
	{0x13, 0x01, 0x03, 0x02, "1 Coin  2 Credits"      },
	{0x13, 0x01, 0x03, 0x01, "1 Coin  3 Credits"      },

	{0   , 0xfe, 0   , 4   , "Coin B"                 },
	{0x13, 0x01, 0x0c, 0x00, "2 Coins 1 Credit"       },
	{0x13, 0x01, 0x0c, 0x0c, "1 Coin  1 Credit"       },
	{0x13, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"      },
	{0x13, 0x01, 0x0c, 0x04, "1 Coin  3 Credits"      },

	{0   , 0xfe, 0   , 4   , "Difficulty"             },
	{0x13, 0x01, 0x30, 0x20, "Easy"                   },
	{0x13, 0x01, 0x30, 0x30, "Medium"                 },
	{0x13, 0x01, 0x30, 0x10, "Hard"                   },
	{0x13, 0x01, 0x30, 0x00, "Hardest"                },

	{0   , 0xfe, 0   , 2   , "Cabinet"                },
	{0x13, 0x01, 0x40, 0x00, "Upright"                },
	{0x13, 0x01, 0x40, 0x40, "Cocktail"               },

	{0   , 0xfe, 0   , 2   , "Flip Screen"            },
	{0x13, 0x01, 0x80, 0x00, "Off"                    },
	{0x13, 0x01, 0x80, 0x80, "On"                     },

	// Dip 2
	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x14, 0x01, 0x03, 0x02, "2"                      },
	{0x14, 0x01, 0x03, 0x03, "3"                      },
	{0x14, 0x01, 0x03, 0x01, "5"                      },
	{0x14, 0x01, 0x03, 0x00, "99"                     },

	{0   , 0xfe, 0   , 4   , "Bonus Life"             },
	{0x14, 0x01, 0x0c, 0x08, "40k,  80k, 120k, 160k"  },
	{0x14, 0x01, 0x0c, 0x0c, "50k, 100k, 150k"        },
	{0x14, 0x01, 0x0c, 0x04, "60k, 120k, 180k"        },
	{0x14, 0x01, 0x0c, 0x00, "None"                   },

	{0   , 0xfe, 0   , 2   , "Allow Continue"         },
	{0x14, 0x01, 0x10, 0x00, "Off"                    },
	{0x14, 0x01, 0x10, 0x10, "On"                     },

	{0   , 0xfe, 0   , 2   , "Free Play"              },
	{0x14, 0x01, 0x20, 0x20, "Off"                    },
	{0x14, 0x01, 0x20, 0x00, "On"                     },
};

STDDIPINFO(Nobb)

static struct BurnDIPInfo Pitfall2DIPList[]=
{
	// Default Values
	{0x13, 0xff, 0xff, 0xff, NULL                     },
	{0x14, 0xff, 0xff, 0xdc, NULL                     },

	// Dip 1
	SYSTEM1_COINAGE(0x13)

	// Dip 2
	{0   , 0xfe, 0   , 2   , "Cabinet"                },
	{0x14, 0x01, 0x01, 0x00, "Upright"                },
	{0x14, 0x01, 0x01, 0x01, "Cocktail"               },

	{0   , 0xfe, 0   , 2   , "Demo Sounds"            },
	{0x14, 0x01, 0x02, 0x02, "Off"                    },
	{0x14, 0x01, 0x02, 0x00, "On"                     },

	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x14, 0x01, 0x0c, 0x0c, "3"                      },
	{0x14, 0x01, 0x0c, 0x08, "4"                      },
	{0x14, 0x01, 0x0c, 0x04, "5"                      },
	{0x14, 0x01, 0x0c, 0x00, "Infinite"               },

	{0   , 0xfe, 0   , 2   , "Bonus Life"             },
	{0x14, 0x01, 0x10, 0x10, "20k 50k"                },
	{0x14, 0x01, 0x10, 0x00, "30k 70k"                },

	{0   , 0xfe, 0   , 2   , "Allow Continue"         },
	{0x14, 0x01, 0x20, 0x20, "Off"                    },
	{0x14, 0x01, 0x20, 0x00, "On"                     },

	{0   , 0xfe, 0   , 2   , "Time"                   },
	{0x14, 0x01, 0x40, 0x00, "2 minutes"              },
	{0x14, 0x01, 0x40, 0x40, "3 minutes"              },
};

STDDIPINFO(Pitfall2)

static struct BurnDIPInfo PitfalluDIPList[]=
{
	// Default Values
	{0x13, 0xff, 0xff, 0xff, NULL                     },
	{0x14, 0xff, 0xff, 0xde, NULL                     },

	// Dip 1
	SYSTEM1_COINAGE(0x13)

	// Dip 2
	{0   , 0xfe, 0   , 2   , "Cabinet"                },
	{0x14, 0x01, 0x01, 0x00, "Upright"                },
	{0x14, 0x01, 0x01, 0x01, "Cocktail"               },

	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x14, 0x01, 0x06, 0x06, "3"                      },
	{0x14, 0x01, 0x06, 0x04, "4"                      },
	{0x14, 0x01, 0x06, 0x02, "5"                      },
	{0x14, 0x01, 0x06, 0x00, "Infinite"               },

	{0   , 0xfe, 0   , 4   , "Starting Stage"         },
	{0x14, 0x01, 0x18, 0x18, "1"                      },
	{0x14, 0x01, 0x18, 0x10, "2"                      },
	{0x14, 0x01, 0x18, 0x08, "3"                      },
	{0x14, 0x01, 0x18, 0x00, "4"                      },

	{0   , 0xfe, 0   , 2   , "Allow Continue"         },
	{0x14, 0x01, 0x20, 0x20, "Off"                    },
	{0x14, 0x01, 0x20, 0x00, "On"                     },

	{0   , 0xfe, 0   , 2   , "Time"                   },
	{0x14, 0x01, 0x40, 0x00, "2 minutes"              },
	{0x14, 0x01, 0x40, 0x40, "3 minutes"              },
};

STDDIPINFO(Pitfallu)

static struct BurnDIPInfo RaflesiaDIPList[]=
{
	// Default Values
	{0x13, 0xff, 0xff, 0xff, NULL                     },
	{0x14, 0xff, 0xff, 0xfc, NULL                     },

	// Dip 1
	SYSTEM1_COINAGE(0x13)

	// Dip 2
	{0   , 0xfe, 0   , 2   , "Cabinet"                },
	{0x14, 0x01, 0x01, 0x00, "Upright"                },
	{0x14, 0x01, 0x01, 0x01, "Cocktail"               },

	{0   , 0xfe, 0   , 2   , "Demo Sounds"            },
	{0x14, 0x01, 0x02, 0x02, "Off"                    },
	{0x14, 0x01, 0x02, 0x00, "On"                     },

	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x14, 0x01, 0x0c, 0x0c, "3"                      },
	{0x14, 0x01, 0x0c, 0x08, "4"                      },
	{0x14, 0x01, 0x0c, 0x04, "5"                      },
	{0x14, 0x01, 0x0c, 0x00, "Infinite"               },

	{0   , 0xfe, 0   , 4   , "Bonus Life"             },
	{0x14, 0x01, 0x30, 0x30, "20k,  70k and 120k"     },
	{0x14, 0x01, 0x30, 0x20, "30k,  80k and 150k"     },
	{0x14, 0x01, 0x30, 0x10, "50k, 100k and 200k"     },
	{0x14, 0x01, 0x30, 0x00, "None"                   },
};

STDDIPINFO(Raflesia)

static struct BurnDIPInfo RegulusDIPList[]=
{
	// Default Values
	{0x13, 0xff, 0xff, 0xff, NULL                     },
	{0x14, 0xff, 0xff, 0x7e, NULL                     },

	// Dip 1
	SYSTEM1_COINAGE(0x13)

	// Dip 2
	{0   , 0xfe, 0   , 2   , "Cabinet"                },
	{0x14, 0x01, 0x01, 0x00, "Upright"                },
	{0x14, 0x01, 0x01, 0x01, "Cocktail"               },

	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x14, 0x01, 0x0c, 0x0c, "3"                      },
	{0x14, 0x01, 0x0c, 0x08, "4"                      },
	{0x14, 0x01, 0x0c, 0x04, "5"                      },
	{0x14, 0x01, 0x0c, 0x00, "Infinite"               },

	{0   , 0xfe, 0   , 2   , "Difficulty"             },
	{0x14, 0x01, 0x40, 0x40, "Easy"                   },
	{0x14, 0x01, 0x40, 0x00, "Hard"                   },

	{0   , 0xfe, 0   , 2   , "Allow Continue"         },
	{0x14, 0x01, 0x80, 0x80, "Off"                    },
	{0x14, 0x01, 0x80, 0x00, "On"                     },
};

STDDIPINFO(Regulus)

static struct BurnDIPInfo RegulusoDIPList[]=
{
	// Default Values
	{0x13, 0xff, 0xff, 0xff, NULL                     },
	{0x14, 0xff, 0xff, 0xfe, NULL                     },

	// Dip 1
	SYSTEM1_COINAGE(0x13)

	// Dip 2
	{0   , 0xfe, 0   , 2   , "Cabinet"                },
	{0x14, 0x01, 0x01, 0x00, "Upright"                },
	{0x14, 0x01, 0x01, 0x01, "Cocktail"               },

	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x14, 0x01, 0x0c, 0x0c, "3"                      },
	{0x14, 0x01, 0x0c, 0x08, "4"                      },
	{0x14, 0x01, 0x0c, 0x04, "5"                      },
	{0x14, 0x01, 0x0c, 0x00, "Infinite"               },

	{0   , 0xfe, 0   , 2   , "Difficulty"             },
	{0x14, 0x01, 0x40, 0x40, "Easy"                   },
	{0x14, 0x01, 0x40, 0x00, "Hard"                   },
};

STDDIPINFO(Reguluso)

static struct BurnDIPInfo SeganinjDIPList[]=
{
	// Default Values
	{0x15, 0xff, 0xff, 0xff, NULL                     },
	{0x16, 0xff, 0xff, 0xdc, NULL                     },

	// Dip 1
	SYSTEM1_COINAGE(0x15)

	// Dip 2
	{0   , 0xfe, 0   , 2   , "Cabinet"                },
	{0x16, 0x01, 0x01, 0x00, "Upright"                },
	{0x16, 0x01, 0x01, 0x01, "Cocktail"               },

	{0   , 0xfe, 0   , 2   , "Demo Sounds"            },
	{0x16, 0x01, 0x02, 0x02, "Off"                    },
	{0x16, 0x01, 0x02, 0x00, "On"                     },

	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x16, 0x01, 0x0c, 0x08, "2"                      },
	{0x16, 0x01, 0x0c, 0x0c, "3"                      },
	{0x16, 0x01, 0x0c, 0x04, "4"                      },
	{0x16, 0x01, 0x0c, 0x00, "240"                    },

	{0   , 0xfe, 0   , 2   , "Bonus Life"             },
	{0x16, 0x01, 0x10, 0x10, "20k  70k 120k 170k"     },
	{0x16, 0x01, 0x10, 0x00, "50k 100k 150k 200k"     },

	{0   , 0xfe, 0   , 2   , "Allow Continue"         },
	{0x16, 0x01, 0x20, 0x20, "Off"                    },
	{0x16, 0x01, 0x20, 0x00, "On"                     },

	{0   , 0xfe, 0   , 2   , "Difficulty"             },
	{0x16, 0x01, 0x40, 0x40, "Easy"                   },
	{0x16, 0x01, 0x40, 0x00, "Hard"                   },
};

STDDIPINFO(Seganinj)

static struct BurnDIPInfo SpatterDIPList[]=
{
	// Default Values
	{0x13, 0xff, 0xff, 0xff, NULL                     },
	{0x14, 0xff, 0xff, 0xfc, NULL                     },

	// Dip 1
	SYSTEM1_COINAGE(0x13)

	// Dip 2
	{0   , 0xfe, 0   , 2   , "Cabinet"                },
	{0x14, 0x01, 0x01, 0x00, "Upright"                },
	{0x14, 0x01, 0x01, 0x01, "Cocktail"               },

	{0   , 0xfe, 0   , 2   , "Demo Sounds"            },
	{0x14, 0x01, 0x02, 0x02, "Off"                    },
	{0x14, 0x01, 0x02, 0x00, "On"                     },

	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x14, 0x01, 0x0c, 0x08, "2"                      },
	{0x14, 0x01, 0x0c, 0x0c, "3"                      },
	{0x14, 0x01, 0x0c, 0x04, "4"                      },
	{0x14, 0x01, 0x0c, 0x00, "Infinite"               },

	{0   , 0xfe, 0   , 4   , "Bonus Life"             },
	{0x14, 0x01, 0x30, 0x30, "40k, 120k and 480k"     },
	{0x14, 0x01, 0x30, 0x20, "50k and 200k"           },
	{0x14, 0x01, 0x30, 0x10, "100k only"              },
	{0x14, 0x01, 0x30, 0x00, "None"                   },

	{0   , 0xfe, 0   , 2   , "Reset Timer/Objects on Life Loss" },
	{0x14, 0x01, 0x40, 0x40, "No"                     },
	{0x14, 0x01, 0x40, 0x00, "Yes"                    },
};

STDDIPINFO(Spatter)

static struct BurnDIPInfo StarjackDIPList[]=
{
	// Default Values
	{0x13, 0xff, 0xff, 0xff, NULL                     },
	{0x14, 0xff, 0xff, 0xf6, NULL                     },

	// Dip 1
	SYSTEM1_COINAGE(0x13)

	// Dip 2
	{0   , 0xfe, 0   , 2   , "Cabinet"                },
	{0x14, 0x01, 0x01, 0x00, "Upright"                },
	{0x14, 0x01, 0x01, 0x01, "Cocktail"               },

	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x14, 0x01, 0x06, 0x06, "3"                      },
	{0x14, 0x01, 0x06, 0x04, "4"                      },
	{0x14, 0x01, 0x06, 0x02, "5"                      },
	{0x14, 0x01, 0x06, 0x00, "Infinite"               },

	{0   , 0xfe, 0   , 8   , "Bonus Life"             },
	{0x14, 0x01, 0x38, 0x38, "Every 20k"              },
	{0x14, 0x01, 0x38, 0x28, "Every 30k"              },
	{0x14, 0x01, 0x38, 0x18, "Every 40k"              },
	{0x14, 0x01, 0x38, 0x08, "Every 50k"              },
	{0x14, 0x01, 0x38, 0x30, "20k, then every 30k"    },
	{0x14, 0x01, 0x38, 0x20, "30k, then every 40k"    },
	{0x14, 0x01, 0x38, 0x10, "40k, then every 50k"    },
	{0x14, 0x01, 0x38, 0x00, "50k, then every 60k"    },

	{0   , 0xfe, 0   , 4   , "Difficulty"             },
	{0x14, 0x01, 0xc0, 0xc0, "Easy"                   },
	{0x14, 0x01, 0xc0, 0x80, "Medium"                 },
	{0x14, 0x01, 0xc0, 0x40, "Hard"                   },
	{0x14, 0x01, 0xc0, 0x00, "Hardest"                },
};

STDDIPINFO(Starjack)

static struct BurnDIPInfo StarjacsDIPList[]=
{
	// Default Values
	{0x13, 0xff, 0xff, 0xff, NULL                     },
	{0x14, 0xff, 0xff, 0xfe, NULL                     },

	// Dip 1
	SYSTEM1_COINAGE(0x13)

	// Dip 2
	{0   , 0xfe, 0   , 2   , "Cabinet"                },
	{0x14, 0x01, 0x01, 0x00, "Upright"                },
	{0x14, 0x01, 0x01, 0x01, "Cocktail"               },

	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x14, 0x01, 0x06, 0x06, "3"                      },
	{0x14, 0x01, 0x06, 0x04, "4"                      },
	{0x14, 0x01, 0x06, 0x02, "5"                      },
	{0x14, 0x01, 0x06, 0x00, "Infinite"               },

	{0   , 0xfe, 0   , 2   , "Ship"                   },
	{0x14, 0x01, 0x08, 0x08, "Single"                 },
	{0x14, 0x01, 0x08, 0x00, "Multi"                  },

	{0   , 0xfe, 0   , 4   , "Bonus Life"             },
	{0x14, 0x01, 0x30, 0x30, "30k, then every 40k"    },
	{0x14, 0x01, 0x30, 0x20, "40k, then every 50k"    },
	{0x14, 0x01, 0x30, 0x10, "50k, then every 60k"    },
	{0x14, 0x01, 0x30, 0x00, "60k, then every 70k"    },

	{0   , 0xfe, 0   , 4   , "Difficulty"             },
	{0x14, 0x01, 0xc0, 0xc0, "Easy"                   },
	{0x14, 0x01, 0xc0, 0x80, "Medium"                 },
	{0x14, 0x01, 0xc0, 0x40, "Hard"                   },
	{0x14, 0x01, 0xc0, 0x00, "Hardest"                },
};

STDDIPINFO(Starjacs)

static struct BurnDIPInfo SwatDIPList[]=
{
	// Default Values
	{0x13, 0xff, 0xff, 0xff, NULL                     },
	{0x14, 0xff, 0xff, 0xfe, NULL                     },

	// Dip 1
	SYSTEM1_COINAGE(0x13)

	// Dip 2
	{0   , 0xfe, 0   , 2   , "Cabinet"                },
	{0x14, 0x01, 0x01, 0x00, "Upright"                },
	{0x14, 0x01, 0x01, 0x01, "Cocktail"               },

	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x14, 0x01, 0x06, 0x06, "3"                      },
	{0x14, 0x01, 0x06, 0x04, "4"                      },
	{0x14, 0x01, 0x06, 0x02, "5"                      },
	{0x14, 0x01, 0x06, 0x00, "Infinite"               },

	{0   , 0xfe, 0   , 8   , "Bonus Life"             },
	{0x14, 0x01, 0x38, 0x38, "30k"                    },
	{0x14, 0x01, 0x38, 0x30, "40k"                    },
	{0x14, 0x01, 0x38, 0x28, "50k"                    },
	{0x14, 0x01, 0x38, 0x20, "60k"                    },
	{0x14, 0x01, 0x38, 0x18, "70k"                    },
	{0x14, 0x01, 0x38, 0x10, "80k"                    },
	{0x14, 0x01, 0x38, 0x08, "90k"                    },
	{0x14, 0x01, 0x38, 0x00, "None"                   },
};

STDDIPINFO(Swat)

static struct BurnDIPInfo TeddybbDIPList[]=
{
	// Default Values
	{0x13, 0xff, 0xff, 0xff, NULL                     },
	{0x14, 0xff, 0xff, 0xfe, NULL                     },

	// Dip 1
	SYSTEM1_COINAGE(0x13)

	// Dip 2
	{0   , 0xfe, 0   , 2   , "Cabinet"                },
	{0x14, 0x01, 0x01, 0x00, "Upright"                },
	{0x14, 0x01, 0x01, 0x01, "Cocktail"               },

	{0   , 0xfe, 0   , 2   , "Demo Sounds"            },
	{0x14, 0x01, 0x02, 0x00, "Off"                    },
	{0x14, 0x01, 0x02, 0x02, "On"                     },

	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x14, 0x01, 0x0c, 0x08, "2"                      },
	{0x14, 0x01, 0x0c, 0x0c, "3"                      },
	{0x14, 0x01, 0x0c, 0x04, "4"                      },
	{0x14, 0x01, 0x0c, 0x00, "252"                    },

	{0   , 0xfe, 0   , 4   , "Bonus Life"             },
	{0x14, 0x01, 0x30, 0x30, "100k 400k"              },
	{0x14, 0x01, 0x30, 0x20, "200k 600k"              },
	{0x14, 0x01, 0x30, 0x10, "400k 800k"              },
	{0x14, 0x01, 0x30, 0x00, "600k"                   },

	{0   , 0xfe, 0   , 2   , "Difficulty"             },
	{0x14, 0x01, 0x40, 0x40, "Easy"                   },
	{0x14, 0x01, 0x40, 0x00, "Hard"                   },
};

STDDIPINFO(Teddybb)

static struct BurnDIPInfo TokisensDIPList[]=
{
	// Default Values
	{0x13, 0xff, 0xff, 0xfc, NULL                     },
	{0x14, 0xff, 0xff, 0xff, NULL                     },

	// Dip 1
	{0   , 0xfe, 0   ,    2, "Demo Sounds"          },
	{0x13, 0x01, 0x02, 0x02, "Off"          },
	{0x13, 0x01, 0x02, 0x00, "On"           },

	{0   , 0xfe, 0   ,    4, "Lives"                },
	{0x13, 0x01, 0x0c, 0x00, "1"            },
	{0x13, 0x01, 0x0c, 0x08, "2"            },
	{0x13, 0x01, 0x0c, 0x0c, "3"            },
	{0x13, 0x01, 0x0c, 0x04, "4"            },

	{0   , 0xfe, 0   ,    2, "Unknown"              },
	{0x13, 0x01, 0x10, 0x10, "Off"          },
	{0x13, 0x01, 0x10, 0x00, "On"           },

	{0   , 0xfe, 0   ,    2, "Unknown"              },
	{0x13, 0x01, 0x20, 0x20, "Off"          },
	{0x13, 0x01, 0x20, 0x00, "On"           },

	{0   , 0xfe, 0   ,    2, "Unknown"              },
	{0x13, 0x01, 0x40, 0x40, "Off"          },
	{0x13, 0x01, 0x40, 0x00, "On"           },

	{0   , 0xfe, 0   ,    2, "Unknown"              },
	{0x13, 0x01, 0x80, 0x80, "Off"          },
	{0x13, 0x01, 0x80, 0x00, "On"           },

	// Dip 2
	SYSTEM1_COINAGE(0x14)

};

STDDIPINFO(Tokisens)

static struct BurnDIPInfo TokisensaDIPList[]=
{
	// Default Values
	{0x13, 0xff, 0xff, 0xfe, NULL                     },
	{0x14, 0xff, 0xff, 0xff, NULL                     },

	// Dip 1
	{0   , 0xfe, 0   ,    2, "Demo Sounds"          },
	{0x13, 0x01, 0x02, 0x00, "Off"          },
	{0x13, 0x01, 0x02, 0x02, "On"           },

	{0   , 0xfe, 0   ,    4, "Lives"                },
	{0x13, 0x01, 0x0c, 0x00, "1"            },
	{0x13, 0x01, 0x0c, 0x08, "2"            },
	{0x13, 0x01, 0x0c, 0x0c, "3"            },
	{0x13, 0x01, 0x0c, 0x04, "4"            },

	{0   , 0xfe, 0   ,    2, "Unknown"              },
	{0x13, 0x01, 0x10, 0x10, "Off"          },
	{0x13, 0x01, 0x10, 0x00, "On"           },

	{0   , 0xfe, 0   ,    2, "Unknown"              },
	{0x13, 0x01, 0x20, 0x20, "Off"          },
	{0x13, 0x01, 0x20, 0x00, "On"           },

	{0   , 0xfe, 0   ,    2, "Unknown"              },
	{0x13, 0x01, 0x40, 0x40, "Off"          },
	{0x13, 0x01, 0x40, 0x00, "On"           },

	{0   , 0xfe, 0   ,    2, "Unknown"              },
	{0x13, 0x01, 0x80, 0x80, "Off"          },
	{0x13, 0x01, 0x80, 0x00, "On"           },

	// Dip 2
	SYSTEM1_COINAGE(0x14)
};

STDDIPINFO(Tokisensa)

static struct BurnDIPInfo UpndownDIPList[]=
{
	// Default Values
	{0x11, 0xff, 0xff, 0xff, NULL                     },
	{0x12, 0xff, 0xff, 0xfe, NULL                     },

	// Dip 1
	SYSTEM1_COINAGE(0x11)

	// Dip 2
	{0   , 0xfe, 0   , 2   , "Cabinet"                },
	{0x12, 0x01, 0x01, 0x00, "Upright"                },
	{0x12, 0x01, 0x01, 0x01, "Cocktail"               },

	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x12, 0x01, 0x06, 0x06, "3"                      },
	{0x12, 0x01, 0x06, 0x04, "4"                      },
	{0x12, 0x01, 0x06, 0x02, "5"                      },
	{0x12, 0x01, 0x06, 0x00, "Infinite"               },

	{0   , 0xfe, 0   , 8   , "Bonus Life"             },
	{0x12, 0x01, 0x38, 0x38, "10k"                    },
	{0x12, 0x01, 0x38, 0x30, "20k"                    },
	{0x12, 0x01, 0x38, 0x28, "30k"                    },
	{0x12, 0x01, 0x38, 0x20, "40k"                    },
	{0x12, 0x01, 0x38, 0x18, "50k"                    },
	{0x12, 0x01, 0x38, 0x10, "60k"                    },
	{0x12, 0x01, 0x38, 0x08, "70k"                    },
	{0x12, 0x01, 0x38, 0x00, "None"                   },

	{0   , 0xfe, 0   , 4   , "Difficulty"             },
	{0x12, 0x01, 0xc0, 0xc0, "Easy"                   },
	{0x12, 0x01, 0xc0, 0x80, "Medium"                 },
	{0x12, 0x01, 0xc0, 0x40, "Hard"                   },
	{0x12, 0x01, 0xc0, 0x00, "Hardest"                },
};

STDDIPINFO(Upndown)

static struct BurnDIPInfo WboyDIPList[]=
{
	// Default Values
	{0x0f, 0xff, 0xff, 0xff, NULL                     },
	{0x10, 0xff, 0xff, 0xec, NULL                     },

	// Dip 1
	SYSTEM1_COINAGE(0x0f)

	// Dip 2
	{0   , 0xfe, 0   , 2   , "Cabinet"                },
	{0x10, 0x01, 0x01, 0x00, "Upright"                },
	{0x10, 0x01, 0x01, 0x01, "Cocktail"               },

	{0   , 0xfe, 0   , 2   , "Demo Sounds"            },
	{0x10, 0x01, 0x02, 0x02, "Off"                    },
	{0x10, 0x01, 0x02, 0x00, "On"                     },

	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x10, 0x01, 0x0c, 0x0c, "3"                      },
	{0x10, 0x01, 0x0c, 0x08, "4"                      },
	{0x10, 0x01, 0x0c, 0x04, "5"                      },
	{0x10, 0x01, 0x0c, 0x00, "Freeplay"               },

	{0   , 0xfe, 0   , 2   , "Bonus Life"             },
	{0x10, 0x01, 0x10, 0x10, "30k 100k 170k 240k"     },
	{0x10, 0x01, 0x10, 0x00, "30k 120k 210k 300k"     },

	{0   , 0xfe, 0   , 2   , "Allow Continue"         },
	{0x10, 0x01, 0x20, 0x00, "Off"                    },
	{0x10, 0x01, 0x20, 0x20, "On"                     },

	{0   , 0xfe, 0   , 2   , "Difficulty"             },
	{0x10, 0x01, 0x40, 0x40, "Easy"                   },
	{0x10, 0x01, 0x40, 0x00, "Hard"                   },
};

STDDIPINFO(Wboy)

static struct BurnDIPInfo Wboy3DIPList[]=
{
	// Default Values
	{0x0f, 0xff, 0xff, 0xff, NULL                     },
	{0x10, 0xff, 0xff, 0xec, NULL                     },

	// Dip 1
	SYSTEM1_COINAGE(0x0f)

	// Dip 2
	{0   , 0xfe, 0   , 2   , "Cabinet"                },
	{0x10, 0x01, 0x01, 0x00, "Upright"                },
	{0x10, 0x01, 0x01, 0x01, "Cocktail"               },

	{0   , 0xfe, 0   , 2   , "Demo Sounds"            },
	{0x10, 0x01, 0x02, 0x02, "Off"                    },
	{0x10, 0x01, 0x02, 0x00, "On"                     },

	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x10, 0x01, 0x0c, 0x0c, "1"                      },
	{0x10, 0x01, 0x0c, 0x08, "2"                      },
	{0x10, 0x01, 0x0c, 0x04, "3"                      },
	{0x10, 0x01, 0x0c, 0x00, "Freeplay"               },

	{0   , 0xfe, 0   , 2   , "Bonus Life"             },
	{0x10, 0x01, 0x10, 0x10, "30k 100k 170k 240k"     },
	{0x10, 0x01, 0x10, 0x00, "30k 120k 210k 300k"     },

	{0   , 0xfe, 0   , 2   , "Allow Continue"         },
	{0x10, 0x01, 0x20, 0x00, "Off"                    },
	{0x10, 0x01, 0x20, 0x20, "On"                     },

	{0   , 0xfe, 0   , 2   , "Difficulty"             },
	{0x10, 0x01, 0x40, 0x40, "Easy"                   },
	{0x10, 0x01, 0x40, 0x00, "Hard"                   },
};

STDDIPINFO(Wboy3)

static struct BurnDIPInfo WboyuDIPList[]=
{
	// Default Values
	{0x0f, 0xff, 0xff, 0xbe, NULL                     },
	{0x10, 0xff, 0xff, 0xff, NULL                     },

	// Dip 1
	{0   , 0xfe, 0   , 2   , "Cabinet"                },
	{0x0f, 0x01, 0x01, 0x00, "Upright"                },
	{0x0f, 0x01, 0x01, 0x01, "Cocktail"               },

	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x0f, 0x01, 0x06, 0x00, "2"                      },
	{0x0f, 0x01, 0x06, 0x06, "3"                      },
	{0x0f, 0x01, 0x06, 0x04, "4"                      },
	{0x0f, 0x01, 0x06, 0x02, "5"                      },

	{0   , 0xfe, 0   , 2   , "Demo Sounds"            },
	{0x0f, 0x01, 0x40, 0x40, "Off"                    },
	{0x0f, 0x01, 0x40, 0x00, "On"                     },

	// Dip 2
	{0   , 0xfe, 0   , 8   , "Coinage"                },
	{0x10, 0x01, 0x07, 0x04, "4 Coins 1 Credit"       },
	{0x10, 0x01, 0x07, 0x05, "3 Coins 1 Credit"       },
	{0x10, 0x01, 0x07, 0x00, "4 Coins 2 Credits"      },
	{0x10, 0x01, 0x07, 0x06, "2 Coins 1 Credit"       },
	{0x10, 0x01, 0x07, 0x01, "3 Coins 2 Credits"      },
	{0x10, 0x01, 0x07, 0x02, "2 Coins 1 Credits"      },
	{0x10, 0x01, 0x07, 0x07, "1 Coin  1 Credit"       },
	{0x10, 0x01, 0x07, 0x03, "1 Coin  2 Credits"      },

	{0   , 0xfe, 0   , 2   , "Allow Continue"         },
	{0x10, 0x01, 0x10, 0x00, "Off"                    },
	{0x10, 0x01, 0x10, 0x10, "On"                     },

	{0   , 0xfe, 0   , 4   , "Mode"                   },
	{0x10, 0x01, 0xc0, 0xc0, "Normal Game"            },
	{0x10, 0x01, 0xc0, 0x80, "Free Play"              },
	{0x10, 0x01, 0xc0, 0x40, "Test Mode"              },
	{0x10, 0x01, 0xc0, 0x00, "Endless Game"           },
};

STDDIPINFO(Wboyu)

static struct BurnDIPInfo WbdeluxeDIPList[]=
{
	// Default Values
	{0x0f, 0xff, 0xff, 0xff, NULL                     },
	{0x10, 0xff, 0xff, 0x7c, NULL                     },

	// Dip 1
	SYSTEM1_COINAGE(0x0f)

	// Dip 2
	{0   , 0xfe, 0   , 2   , "Cabinet"                },
	{0x10, 0x01, 0x01, 0x00, "Upright"                },
	{0x10, 0x01, 0x01, 0x01, "Cocktail"               },

	{0   , 0xfe, 0   , 2   , "Demo Sounds"            },
	{0x10, 0x01, 0x02, 0x02, "Off"                    },
	{0x10, 0x01, 0x02, 0x00, "On"                     },

	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x10, 0x01, 0x0c, 0x0c, "3"                      },
	{0x10, 0x01, 0x0c, 0x08, "4"                      },
	{0x10, 0x01, 0x0c, 0x04, "5"                      },
	{0x10, 0x01, 0x0c, 0x00, "Freeplay"               },

	{0   , 0xfe, 0   , 2   , "Bonus Life"             },
	{0x10, 0x01, 0x10, 0x10, "30k 100k 170k 240k"     },
	{0x10, 0x01, 0x10, 0x00, "30k 120k 210k 300k"     },

	{0   , 0xfe, 0   , 2   , "Allow Continue"         },
	{0x10, 0x01, 0x20, 0x00, "Off"                    },
	{0x10, 0x01, 0x20, 0x20, "On"                     },

	{0   , 0xfe, 0   , 2   , "Difficulty"             },
	{0x10, 0x01, 0x40, 0x40, "Easy"                   },
	{0x10, 0x01, 0x40, 0x00, "Hard"                   },

	{0   , 0xfe, 0   , 2   , "Energy Consumption"     },
	{0x10, 0x01, 0x80, 0x00, "Slow"                   },
	{0x10, 0x01, 0x80, 0x80, "Fast"                   },
};

STDDIPINFO(Wbdeluxe)

static struct BurnDIPInfo WmatchDIPList[]=
{
	// Default Values
	{0x19, 0xff, 0xff, 0xff, NULL                     },
	{0x1a, 0xff, 0xff, 0xfc, NULL                     },

	// Dip 1
	SYSTEM1_COINAGE(0x19)

	// Dip 2
	{0   , 0xfe, 0   , 2   , "Cabinet"                },
	{0x1a, 0x01, 0x01, 0x00, "Upright"                },
	{0x1a, 0x01, 0x01, 0x01, "Cocktail"               },

	{0   , 0xfe, 0   , 2   , "Demo Sounds"            },
	{0x1a, 0x01, 0x02, 0x02, "Off"                    },
	{0x1a, 0x01, 0x02, 0x00, "On"                     },

	{0   , 0xfe, 0   , 4   , "Time"                   },
	{0x1a, 0x01, 0x0c, 0x0c, "Normal"                 },
	{0x1a, 0x01, 0x0c, 0x08, "Fast"                   },
	{0x1a, 0x01, 0x0c, 0x04, "Faster"                 },
	{0x1a, 0x01, 0x0c, 0x00, "Fastest"                },

	{0   , 0xfe, 0   , 2   , "Bonus Life"             },
	{0x1a, 0x01, 0x10, 0x10, "20k 50k"                },
	{0x1a, 0x01, 0x10, 0x00, "30k 70k"                },

	{0   , 0xfe, 0   , 2   , "Difficulty"             },
	{0x1a, 0x01, 0x40, 0x40, "Easy"                   },
	{0x1a, 0x01, 0x40, 0x00, "Hard"                   },
};

STDDIPINFO(Wmatch)

static struct BurnDIPInfo ShtngmstDIPList[]=
{
	DIP_OFFSET(0x0a)
	{0x00, 0xff, 0xff, 0xfd, NULL					},
	{0x01, 0xff, 0xff, 0xff, NULL					},
	{0x02, 0xff, 0xff, 0xff, NULL					}, // "port 18"

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x00, 0x01, 0x02, 0x02, "Off"					},
	{0x00, 0x01, 0x02, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x00, 0x01, 0x0c, 0x0c, "3"					},
	{0x00, 0x01, 0x0c, 0x08, "4"					},
	{0x00, 0x01, 0x0c, 0x04, "5"					},
	{0x00, 0x01, 0x0c, 0x00, "Infinite"				},

	{0   , 0xfe, 0   ,    4, "Extra Player"			},
	{0x00, 0x01, 0x30, 0x30, "100000 - 500000"		},
	{0x00, 0x01, 0x30, 0x20, "150000 - 600000"		},
	{0x00, 0x01, 0x30, 0x10, "200000 - 700000"		},
	{0x00, 0x01, 0x30, 0x00, "300000 - 800000"		},

	{0   , 0xfe, 0   ,    2, "Shots per second"		},
	{0x00, 0x01, 0x01, 0x01, "5"					},
	{0x00, 0x01, 0x01, 0x00, "3"					},

	SYSTEM1_COINAGE(0x01)
};

STDDIPINFO(Shtngmst)

static struct BurnDIPInfo ChopliftDIPList[]=
{
	DIP_OFFSET(0x13)

	{0x00, 0xff, 0xff, 0xdc, NULL						},
	{0x01, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x00, 0x01, 0x01, 0x00, "Upright"					},
	{0x00, 0x01, 0x01, 0x01, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x00, 0x01, 0x02, 0x02, "Off"						},
	{0x00, 0x01, 0x02, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x00, 0x01, 0x0c, 0x08, "2"						},
	{0x00, 0x01, 0x0c, 0x0c, "3"						},
	{0x00, 0x01, 0x0c, 0x04, "4"						},
	{0x00, 0x01, 0x0c, 0x00, "Free Play"				},

	{0   , 0xfe, 0   ,    2, "Bonus Life"				},
	{0x00, 0x01, 0x10, 0x10, "20k 70k 120k 170k"		},
	{0x00, 0x01, 0x10, 0x00, "50k 100k 150k 200k"		},

	{0   , 0xfe, 0   ,    2, "Difficulty"				},
	{0x00, 0x01, 0x20, 0x00, "Hard"						},
	{0x00, 0x01, 0x20, 0x20, "Easy"						},

	SYSTEM1_COINAGE(0x01)
};

STDDIPINFO(Choplift)

static struct BurnDIPInfo UfosensiDIPList[]=
{
   // Default Values
   {0x15, 0xff, 0xff, 0x8d, NULL                     },
   {0x16, 0xff, 0xff, 0xff, NULL                     },

   // Dip 1
   {0   , 0xfe, 0   ,    4, "Difficulty"		},
   {0x15, 0x01, 0x03, 0x00, "Easy"		},
   {0x15, 0x01, 0x03, 0x01, "Normal"		},
   {0x15, 0x01, 0x03, 0x02, "Hard"		},
   {0x15, 0x01, 0x03, 0x03, "Hardest"		},

   {0   , 0xfe, 0   ,    3, "Lives"		},
   {0x15, 0x01, 0x0c, 0x0c, "3"		},
   {0x15, 0x01, 0x0c, 0x04, "4"		},
   {0x15, 0x01, 0x0c, 0x00, "5"		},

   {0   , 0xfe, 0   ,    2, "Cabinet"		},
   {0x15, 0x01, 0x10, 0x00, "Upright"		},
   {0x15, 0x01, 0x10, 0x10, "Cocktail"		},

   {0   , 0xfe, 0   ,    2, "Allow Continue"		},
   {0x15, 0x01, 0x20, 0x20, "No"		},
   {0x15, 0x01, 0x20, 0x00, "Yes"		},

   {0   , 0xfe, 0   ,    2, "Unknown"		},
   {0x15, 0x01, 0x40, 0x40, "Off"		},
   {0x15, 0x01, 0x40, 0x00, "On"		},

   {0   , 0xfe, 0   ,    2, "Invulnerability"		},
   {0x15, 0x01, 0x80, 0x80, "Off"		},
   {0x15, 0x01, 0x80, 0x00, "On"		},

   // Dip 2
   {0   , 0xfe, 0   , 8   , "Coinage"                },
   {0x16, 0x01, 0x07, 0x04, "4 Coins 1 Credit"       },
   {0x16, 0x01, 0x07, 0x05, "3 Coins 1 Credit"       },
   {0x16, 0x01, 0x07, 0x00, "4 Coins 2 Credits"      },
   {0x16, 0x01, 0x07, 0x06, "2 Coins 1 Credit"       },
   {0x16, 0x01, 0x07, 0x01, "3 Coins 2 Credits"      },
   {0x16, 0x01, 0x07, 0x02, "2 Coins 1 Credits"      },
   {0x16, 0x01, 0x07, 0x07, "1 Coin  1 Credit"       },
   {0x16, 0x01, 0x07, 0x03, "1 Coin  2 Credits"      },

   {0   , 0xfe, 0   , 2   , "Allow Continue"         },
   {0x16, 0x01, 0x10, 0x00, "Off"                    },
   {0x16, 0x01, 0x10, 0x10, "On"                     },

   {0   , 0xfe, 0   , 4   , "Mode"                   },
   {0x16, 0x01, 0xc0, 0xc0, "Normal Game"            },
   {0x16, 0x01, 0xc0, 0x80, "Free Play"              },
   {0x16, 0x01, 0xc0, 0x40, "Test Mode"              },
   {0x16, 0x01, 0xc0, 0x00, "Endless Game"           },
};

STDDIPINFO(Ufosensi)

static struct BurnDIPInfo WbmlDIPList[]=
{
	// Default Values
	{0x13, 0xff, 0xff, 0xfe, NULL                     },
	{0x14, 0xff, 0xff, 0xff, NULL                     },

	// Dip 1
	{0   , 0xfe, 0   ,    2, "Cabinet"                },
	{0x13, 0x01, 0x01, 0x00, "Upright"                },
	//{0x13, 0x01, 0x01, 0x01, "Cocktail"               }, no screen flipping here :)

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x02, 0x00, "Off"		},
	{0x13, 0x01, 0x02, 0x02, "On"		},

	{0   , 0xfe, 0   ,    3, "Lives"		},
	{0x13, 0x01, 0x0c, 0x04, "3"		},
	{0x13, 0x01, 0x0c, 0x0c, "4"		},
	{0x13, 0x01, 0x0c, 0x08, "5"		},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x13, 0x01, 0x10, 0x10, "30000 100000 200000"		},
	{0x13, 0x01, 0x10, 0x00, "50000 150000 250000"		},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x13, 0x01, 0x20, 0x20, "Easy"		},
	{0x13, 0x01, 0x20, 0x00, "Hard"		},

	{0   , 0xfe, 0   ,    2, "Test Mode"		},
	{0x13, 0x01, 0x40, 0x40, "Off"		},
	{0x13, 0x01, 0x40, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x13, 0x01, 0x80, 0x80, "Off"		},
	{0x13, 0x01, 0x80, 0x00, "On"		},

	// Dip 2
	SYSTEM1_COINAGE(0x14)
};

STDDIPINFO(Wbml)

#undef SYSTEM1_COINAGE

/*==============================================================================================
ROM Descriptions
===============================================================================================*/

static struct BurnRomInfo FourdwarrioRomDesc[] = {
	{ "4d.116",            0x004000, 0x546d1bc7, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "4d.109",            0x004000, 0xf1074ec3, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "4d.96",             0x004000, 0x387c1e8f, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code

	{ "4d.120",            0x002000, 0x5241c009, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program Code

	{ "4d.62",             0x002000, 0xf31b2e09, BRF_GRA },		  	  //  4 Tiles
	{ "4d.61",             0x002000, 0x5430e925, BRF_GRA },		  	  //  5 Tiles
	{ "4d.64",             0x002000, 0x9f442351, BRF_GRA },		  	  //  6 Tiles
	{ "4d.63",             0x002000, 0x633232bd, BRF_GRA },		  	  //  7 Tiles
	{ "4d.66",             0x002000, 0x52bfa2ed, BRF_GRA },		  	  //  8 Tiles
	{ "4d.65",             0x002000, 0xe9ba4658, BRF_GRA },		  	  //  9 Tiles

	{ "4d.117",            0x004000, 0x436e4141, BRF_GRA },		  	  // 10 Sprites
	{ "4d.04",             0x004000, 0x8b7cecef, BRF_GRA },		  	  // 11 Sprites
	{ "4d.110",            0x004000, 0x6ec5990a, BRF_GRA },		  	  // 12 Sprites
	{ "4d.05",             0x004000, 0xf31a1e6a, BRF_GRA },		  	  // 13 Sprites

	{ "pr5317.76",         0x000100, 0x648350b8, BRF_OPT },		  	  // 14 Timing PROM
};

STD_ROM_PICK(Fourdwarrio)
STD_ROM_FN(Fourdwarrio)

static struct BurnRomInfo BlockgalRomDesc[] = {
	{ "bg.116",            0x004000, 0xa99b231a, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "bg.109",            0x004000, 0xa6b573d5, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code

	{ "bg.120",            0x002000, 0xd848faff, BRF_ESS | BRF_PRG }, //  2	Z80 #2 Program Code

	{ "bg.62",             0x002000, 0x7e3ea4eb, BRF_GRA },		  	  //  3 Tiles
	{ "bg.61",             0x002000, 0x4dd3d39d, BRF_GRA },		  	  //  4 Tiles
	{ "bg.64",             0x002000, 0x17368663, BRF_GRA },		  	  //  5 Tiles
	{ "bg.63",             0x002000, 0x0c8bc404, BRF_GRA },		  	  //  6 Tiles
	{ "bg.66",             0x002000, 0x2b7dc4fa, BRF_GRA },		  	  //  7 Tiles
	{ "bg.65",             0x002000, 0xed121306, BRF_GRA },		  	  //  8 Tiles

	{ "bg.117",            0x004000, 0xe99cc920, BRF_GRA },		  	  //  9 Sprites
	{ "bg.04",             0x004000, 0x213057f8, BRF_GRA },		  	  // 10 Sprites
	{ "bg.110",            0x004000, 0x064c812c, BRF_GRA },		  	  // 11 Sprites
	{ "bg.05",             0x004000, 0x02e0b040, BRF_GRA },		  	  // 12 Sprites

	{ "pr5317.76",         0x000100, 0x648350b8, BRF_OPT },		  	  // 13 Timing PROM

	{ "317-0029.key",      0x002000, 0x350d7f93, BRF_ESS | BRF_PRG }, // 14 MC8123 Key
};

STD_ROM_PICK(Blockgal)
STD_ROM_FN(Blockgal)

static struct BurnRomInfo BrainRomDesc[] = {
	{ "brain.1",           0x008000, 0x2d2aec31, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "brain.2",           0x008000, 0x810a8ab5, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "brain.3",           0x008000, 0x9a225634, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code

	{ "brain.120",         0x008000, 0xc7e50278, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program Code

	{ "brain.62",          0x004000, 0x7dce2302, BRF_GRA },		  	  //  4 Tiles
	{ "brain.64",          0x004000, 0x7ce03fd3, BRF_GRA },		  	  //  5 Tiles
	{ "brain.66",          0x004000, 0xea54323f, BRF_GRA },		  	  //  6 Tiles

	{ "brain.117",         0x008000, 0x92ff71a4, BRF_GRA },		  	  //  7 Sprites
	{ "brain.110",         0x008000, 0xa1b847ec, BRF_GRA },		  	  //  8 Sprites
	{ "brain.4",           0x008000, 0xfd2ea53b, BRF_GRA },		  	  //  9 Sprites

	{ "bprom.3",           0x000100, 0x8eee0f72, BRF_GRA },		  	  // 10 Red PROM
	{ "bprom.2",           0x000100, 0x3e7babd7, BRF_GRA },		  	  // 11 Green PROM
	{ "bprom.1",           0x000100, 0x371c44a6, BRF_GRA },		  	  // 12 Blue PROM
	{ "pr5317.76",         0x000100, 0x648350b8, BRF_OPT },		  	  // 13 Timing PROM
};

STD_ROM_PICK(Brain)
STD_ROM_FN(Brain)

static struct BurnRomInfo BullfgtRomDesc[] = {
	{ "epr-.129",          0x002000, 0x29f19156, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "epr-.130",          0x002000, 0xe37d2b95, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "epr-.131",          0x002000, 0xeaf5773d, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code
	{ "epr-.132",          0x002000, 0x72c3c712, BRF_ESS | BRF_PRG }, //  3	Z80 #1 Program Code
	{ "epr-.133",          0x002000, 0x7d9fa4cd, BRF_ESS | BRF_PRG }, //  4	Z80 #1 Program Code
	{ "epr-.134",          0x002000, 0x061f2797, BRF_ESS | BRF_PRG }, //  5	Z80 #1 Program Code

	{ "epr-6077.120",      0x002000, 0x02a37602, BRF_ESS | BRF_PRG }, //  6	Z80 #2 Program Code

	{ "epr-.82",           0x002000, 0xb71c349f, BRF_GRA },		  	  //  7 Tiles
	{ "epr-.65",           0x002000, 0x86deafa8, BRF_GRA },		  	  //  8 Tiles
	{ "epr-6087.81",       0x002000, 0x2677742c, BRF_GRA },		  	  //  9 Tiles
	{ "epr-.64",           0x002000, 0x6f0a62be, BRF_GRA },		  	  // 10 Tiles
	{ "epr-6085.80",       0x002000, 0x9c3ddc62, BRF_GRA },		  	  // 11 Tiles
	{ "epr-.63",           0x002000, 0xc0fce57c, BRF_GRA },		  	  // 12 Tiles

	{ "epr-6069.86",       0x004000, 0xfe691e41, BRF_GRA },		  	  // 13 Sprites
	{ "epr-6070.93",       0x004000, 0x34f080df, BRF_GRA },		  	  // 14 Sprites

	{ "pr-5317.106",       0x000100, 0x648350b8, BRF_OPT },		  	  // 15 Timing PROM
};

STD_ROM_PICK(Bullfgt)
STD_ROM_FN(Bullfgt)

static struct BurnRomInfo ThetogyuRomDesc[] = {
	{ "epr-6071.116",      0x004000, 0x96b57df9, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "epr-6072.109",      0x004000, 0xf7baadd0, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "epr-6073.96",       0x004000, 0x721af166, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code

	{ "epr-6077.120",      0x002000, 0x02a37602, BRF_ESS | BRF_PRG }, //  6	Z80 #2 Program Code

	{ "epr-6089.62",       0x002000, 0xa183e5ff, BRF_GRA },		  	  //  7 Tiles
	{ "epr-6088.61",       0x002000, 0xb919b4a6, BRF_GRA },		  	  //  8 Tiles
	{ "epr-6087.64",       0x002000, 0x2677742c, BRF_GRA },		  	  //  9 Tiles
	{ "epr-6086.63",       0x002000, 0x76b5a084, BRF_GRA },		  	  // 10 Tiles
	{ "epr-6085.66",       0x002000, 0x9c3ddc62, BRF_GRA },		  	  // 11 Tiles
	{ "epr-6084.65",       0x002000, 0x90e1fa5f, BRF_GRA },		  	  // 12 Tiles

	{ "epr-6069.117",      0x004000, 0xfe691e41, BRF_GRA },		  	  // 13 Sprites
	{ "epr-6070.110",      0x004000, 0x34f080df, BRF_GRA },		  	  // 14 Sprites

	{ "pr-5317.76",        0x000100, 0x648350b8, BRF_OPT },		  	  // 15 Timing PROM
};

STD_ROM_PICK(Thetogyu)
STD_ROM_FN(Thetogyu)

static struct BurnRomInfo FlickyRomDesc[] = {
	{ "epr-5978a.116",     0x004000, 0x296f1492, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "epr-5979a.109",     0x004000, 0x64b03ef9, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code

	{ "epr-5869.120",      0x002000, 0x6d220d4e, BRF_ESS | BRF_PRG }, //  2	Z80 #2 Program Code

	{ "epr-5868.62",       0x002000, 0x7402256b, BRF_GRA },		  	  //  3 Tiles
	{ "epr-5867.61",       0x002000, 0x2f5ce930, BRF_GRA },		  	  //  4 Tiles
	{ "epr-5866.64",       0x002000, 0x967f1d9a, BRF_GRA },		  	  //  5 Tiles
	{ "epr-5865.63",       0x002000, 0x03d9a34c, BRF_GRA },		  	  //  6 Tiles
	{ "epr-5864.66",       0x002000, 0xe659f358, BRF_GRA },		  	  //  7 Tiles
	{ "epr-5863.65",       0x002000, 0xa496ca15, BRF_GRA },		  	  //  8 Tiles

	{ "epr-5855.117",      0x004000, 0xb5f894a1, BRF_GRA },		  	  //  9 Sprites
	{ "epr-5856.110",      0x004000, 0x266af78f, BRF_GRA },		  	  // 10 Sprites

	{ "pr-5317.76",        0x000100, 0x648350b8, BRF_OPT },		  	  // 11 Timing PROM
};

STD_ROM_PICK(Flicky)
STD_ROM_FN(Flicky)

static struct BurnRomInfo FlickyaRomDesc[] = {
	{ "epr-5978a.116",     0x004000, 0x296f1492, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "epr-5979a.109",     0x004000, 0x64b03ef9, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code

	{ "epr-5869.120",      0x002000, 0x6d220d4e, BRF_ESS | BRF_PRG }, //  2	Z80 #2 Program Code

	{ "epr-6001.62",       0x004000, 0xf1a75200, BRF_GRA },		  	  //  3 Tiles
	{ "epr-6000.64",       0x004000, 0x299aefb7, BRF_GRA },		  	  //  4 Tiles
	{ "epr-5999.66",       0x004000, 0x1ca53157, BRF_GRA },		  	  //  5 Tiles

	{ "epr-5855.117",      0x004000, 0xb5f894a1, BRF_GRA },		  	  //  6 Sprites
	{ "epr-5856.110",      0x004000, 0x266af78f, BRF_GRA },		  	  //  7 Sprites

	{ "pr-5317.76",        0x000100, 0x648350b8, BRF_OPT },		  	  //  8 Timing PROM
};

STD_ROM_PICK(Flickya)
STD_ROM_FN(Flickya)

static struct BurnRomInfo FlickybRomDesc[] = {
	{ "e-2_0.116",         0x004000, 0xec94fdbb, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "109",               0x004000, 0xaa11b394, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code

	{ "epr-5869.120",      0x002000, 0x6d220d4e, BRF_ESS | BRF_PRG }, //  2	Z80 #2 Program Code

	{ "epr-6001.62",       0x004000, 0xf1a75200, BRF_GRA },		  	  //  3 Tiles
	{ "epr-6000.64",       0x004000, 0x299aefb7, BRF_GRA },		  	  //  4 Tiles
	{ "epr-5999.66",       0x004000, 0x1ca53157, BRF_GRA },		  	  //  5 Tiles

	{ "epr-5855.117",      0x004000, 0xb5f894a1, BRF_GRA },		  	  //  6 Sprites
	{ "epr-5856.110",      0x004000, 0x266af78f, BRF_GRA },		  	  //  7 Sprites

	{ "pr-5317.76",        0x000100, 0x648350b8, BRF_OPT },		  	  //  8 Timing PROM
};

STD_ROM_PICK(Flickyb)
STD_ROM_FN(Flickyb)

static struct BurnRomInfo FlickygRomDesc[] = {
	{ "epr5978a.116",      0x004000, 0x296f1492, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "epr5979a.109",      0x004000, 0x64b03ef9, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code

	{ "epr-5869.120",      0x002000, 0x6d220d4e, BRF_ESS | BRF_PRG }, //  2	Z80 #2 Program Code

	{ "66xx.ic62",         0x004000, 0xf1a75200, BRF_GRA },		  	  //  3 Tiles
	{ "66x0.ic64",         0x004000, 0x299aefb7, BRF_GRA },		  	  //  4 Tiles
	{ "5999.ic66",         0x004000, 0x1ca53157, BRF_GRA },		  	  //  5 Tiles

	{ "epr-5855.117",      0x004000, 0xb5f894a1, BRF_GRA },		  	  //  6 Sprites
	{ "epr-5856.110",      0x004000, 0x266af78f, BRF_GRA },		  	  //  7 Sprites

	{ "pr-5317.76",        0x000100, 0x648350b8, BRF_OPT },		  	  //  8 Timing PROM
};

STD_ROM_PICK(Flickyg)
STD_ROM_FN(Flickyg)

static struct BurnRomInfo Flickys1RomDesc[] = {
	{ "ic129",             0x002000, 0x7011275c, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "ic130",             0x002000, 0xe7ed012d, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "ic131",             0x002000, 0xc5e98cd1, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code
	{ "ic132",             0x002000, 0x0e5122c2, BRF_ESS | BRF_PRG }, //  3	Z80 #1 Program Code

	{ "epr-5869.120",      0x002000, 0x6d220d4e, BRF_ESS | BRF_PRG }, //  4	Z80 #2 Program Code

	{ "epr-5868.62",       0x002000, 0x7402256b, BRF_GRA },		  	  //  5 Tiles
	{ "epr-5867.61",       0x002000, 0x2f5ce930, BRF_GRA },		  	  //  6 Tiles
	{ "epr-5866.64",       0x002000, 0x967f1d9a, BRF_GRA },		  	  //  7 Tiles
	{ "epr-5865.63",       0x002000, 0x03d9a34c, BRF_GRA },		  	  //  8 Tiles
	{ "epr-5864.66",       0x002000, 0xe659f358, BRF_GRA },		  	  //  9 Tiles
	{ "epr-5863.65",       0x002000, 0xa496ca15, BRF_GRA },		  	  // 10 Tiles

	{ "epr-5855.117",      0x004000, 0xb5f894a1, BRF_GRA },		  	  // 11 Sprites
	{ "epr-5856.110",      0x004000, 0x266af78f, BRF_GRA },		  	  // 12 Sprites

	{ "pr-5317.76",        0x000100, 0x648350b8, BRF_OPT },		  	  // 13 Timing PROM
};

STD_ROM_PICK(Flickys1)
STD_ROM_FN(Flickys1)

static struct BurnRomInfo Flickys2RomDesc[] = {
	{ "epr-6621.bin",      0x004000, 0xb21ff546, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "epr-6622.bin",      0x004000, 0x133a8bf1, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code

	{ "epr-5869.120",      0x002000, 0x6d220d4e, BRF_ESS | BRF_PRG }, //  2	Z80 #2 Program Code

	{ "epr-5868.62",       0x002000, 0x7402256b, BRF_GRA },		  	  //  3 Tiles
	{ "epr-5867.61",       0x002000, 0x2f5ce930, BRF_GRA },		  	  //  4 Tiles
	{ "epr-5866.64",       0x002000, 0x967f1d9a, BRF_GRA },		  	  //  5 Tiles
	{ "epr-5865.63",       0x002000, 0x03d9a34c, BRF_GRA },		  	  //  6 Tiles
	{ "epr-5864.66",       0x002000, 0xe659f358, BRF_GRA },		  	  //  7 Tiles
	{ "epr-5863.65",       0x002000, 0xa496ca15, BRF_GRA },		  	  //  8 Tiles

	{ "epr-5855.117",      0x004000, 0xb5f894a1, BRF_GRA },		  	  //  9 Sprites
	{ "epr-5856.110",      0x004000, 0x266af78f, BRF_GRA },		  	  // 10 Sprites

	{ "pr-5317.76",        0x000100, 0x648350b8, BRF_OPT },		  	  // 11 Timing PROM
};

STD_ROM_PICK(Flickys2)
STD_ROM_FN(Flickys2)

static struct BurnRomInfo Flickys2gRomDesc[] = {
	{ "epr-6621.bin",      0x004000, 0xb21ff546, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "epr-6622.bin",      0x004000, 0x133a8bf1, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code

	{ "epr-5869.120",      0x002000, 0x6d220d4e, BRF_ESS | BRF_PRG }, //  2	Z80 #2 Program Code

	{ "66xx.ic62",         0x004000, 0xf1a75200, BRF_GRA },		  	  //  3 Tiles
	{ "66x0.ic64",         0x004000, 0x299aefb7, BRF_GRA },		  	  //  4 Tiles
	{ "5999.ic66",         0x004000, 0x1ca53157, BRF_GRA },		  	  //  5 Tiles

	{ "epr-5855.117",      0x004000, 0xb5f894a1, BRF_GRA },		  	  //  6 Sprites
	{ "epr-5856.110",      0x004000, 0x266af78f, BRF_GRA },		  	  //  7 Sprites

	{ "pr-5317.76",        0x000100, 0x648350b8, BRF_OPT },		  	  //  8 Timing PROM
};

STD_ROM_PICK(Flickys2g)
STD_ROM_FN(Flickys2g)

static struct BurnRomInfo FlickyoRomDesc[] = {
	{ "epr-5857.ic129",    0x002000, 0xa65ac88e, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "epr-5858a.ic30",    0x002000, 0x18b412f4, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "epr-5859.ic31",     0x002000, 0xa5558d7e, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code
	{ "epr-5860.ic32",     0x002000, 0x1b35fef1, BRF_ESS | BRF_PRG }, //  3	Z80 #1 Program Code

	{ "epr-5869.120",      0x002000, 0x6d220d4e, BRF_ESS | BRF_PRG }, //  4	Z80 #2 Program Code

	{ "epr-5868.62",       0x002000, 0x7402256b, BRF_GRA },		  	  //  5 Tiles
	{ "epr-5867.61",       0x002000, 0x2f5ce930, BRF_GRA },		  	  //  6 Tiles
	{ "epr-5866.64",       0x002000, 0x967f1d9a, BRF_GRA },		  	  //  7 Tiles
	{ "epr-5865.63",       0x002000, 0x03d9a34c, BRF_GRA },		  	  //  8 Tiles
	{ "epr-5864.66",       0x002000, 0xe659f358, BRF_GRA },		  	  //  9 Tiles
	{ "epr-5863.65",       0x002000, 0xa496ca15, BRF_GRA },		  	  // 10 Tiles

	{ "epr-5855.117",      0x004000, 0xb5f894a1, BRF_GRA },		  	  // 11 Sprites
	{ "epr-5856.110",      0x004000, 0x266af78f, BRF_GRA },		  	  // 12 Sprites

	{ "pr-5317.76",        0x000100, 0x648350b8, BRF_OPT },		  	  // 13 Timing PROM
};

STD_ROM_PICK(Flickyo)
STD_ROM_FN(Flickyo)

static struct BurnRomInfo FlickyupRomDesc[] = {
	// Coverted from and running on a Up n' Down board.
	{ "2764-ic29",         0x002000, 0x59ba3107, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "2764-ic30",         0x002000, 0x5c84216f, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "2764-ic31",         0x002000, 0x106132fa, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code
	{ "2764-ic32",         0x002000, 0xc5ea7f58, BRF_ESS | BRF_PRG }, //  3	Z80 #1 Program Code

	{ "epr-5869.120",      0x002000, 0x6d220d4e, BRF_ESS | BRF_PRG }, //  4	Z80 #2 Program Code

	{ "epr-5868.62",       0x002000, 0x7402256b, BRF_GRA },		  	  //  5 Tiles
	{ "epr-5867.61",       0x002000, 0x2f5ce930, BRF_GRA },		  	  //  6 Tiles
	{ "epr-5866.64",       0x002000, 0x967f1d9a, BRF_GRA },		  	  //  7 Tiles
	{ "epr-5865.63",       0x002000, 0x03d9a34c, BRF_GRA },		  	  //  8 Tiles
	{ "epr-5864.66",       0x002000, 0xe659f358, BRF_GRA },		  	  //  9 Tiles
	{ "epr-5863.65",       0x002000, 0xa496ca15, BRF_GRA },		  	  // 10 Tiles

	{ "epr-5855.117",      0x004000, 0xb5f894a1, BRF_GRA },		  	  // 11 Sprites
	{ "epr-5856.110",      0x004000, 0x266af78f, BRF_GRA },		  	  // 12 Sprites

	{ "pr-5317.76",        0x000100, 0x648350b8, BRF_OPT },		  	  // 13 Timing PROM
};

STD_ROM_PICK(Flickyup)
STD_ROM_FN(Flickyup)

static struct BurnRomInfo FlickyupaRomDesc[] = {
	{ "1",				0x002000, 0x45391848, BRF_ESS | BRF_PRG },              //  0 Z80 #1 Program Code
	{ "2",				0x002000, 0xbf15cb82, BRF_ESS | BRF_PRG },              //  1 Z80 #1 Program Code
	{ "3",				0x002000, 0x6ee40df4, BRF_ESS | BRF_PRG },              //  2 Z80 #1 Program Code
	{ "4",				0x002000, 0xfdecc2b5, BRF_ESS | BRF_PRG },              //  3 Z80 #1 Program Code

	{ "13.120",			0x002000, 0x6d220d4e, BRF_ESS | BRF_PRG },              //  4 Z80 #2 Program Code

	{ "7.62",			0x002000, 0x7402256b, BRF_GRA },                        //  5 Tiles
	{ "10.61",			0x002000, 0x2f5ce930, BRF_GRA },                        //  6 Tiles
	{ "6.64",			0x002000, 0x967f1d9a, BRF_GRA },                        //  7 Tiles
	{ "9.63",			0x002000, 0x03d9a34c, BRF_GRA },                        //  8 Tiles
	{ "5.66",			0x002000, 0xe659f358, BRF_GRA },                        //  9 Tiles
	{ "8.65",			0x002000, 0xa496ca15, BRF_GRA },                        // 10 Tiles

	{ "12.117",			0x004000, 0xb5f894a1, BRF_GRA },                        // 11 Sprites
	{ "11.110",			0x004000, 0x266af78f, BRF_GRA },                        // 12 Sprites

	{ "pr-5317.76",		0x000100, 0x648350b8, BRF_OPT },                        // 13 Timing PROM

	{ "n82s129n",		0x000200, 0x00000000, BRF_ESS | BRF_PRG | BRF_NODUMP }, // 14 Decryption PROM

	{ "pal20l10cns",	0x0000cc, 0x00000000, BRF_OPT | BRF_NODUMP },           // 15 PLDs
};

STD_ROM_PICK(Flickyupa)
STD_ROM_FN(Flickyupa)

static struct BurnRomInfo GardiaRomDesc[] = {
	{ "epr-10255.1",       0x008000, 0x89282a6b, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "epr-10254.2",       0x008000, 0x2826b6d8, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "epr-10253.3",       0x008000, 0x7911260f, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code

	{ "epr-10243.120",     0x004000, 0x87220660, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program Code

	{ "epr-10249.61",      0x004000, 0x4e0ad0f2, BRF_GRA },		  	  //  4 Tiles
	{ "epr-10248.64",      0x004000, 0x3515d124, BRF_GRA },		  	  //  5 Tiles
	{ "epr-10247.66",      0x004000, 0x541e1555, BRF_GRA },		  	  //  6 Tiles

	{ "epr-10234.117",     0x008000, 0x8a6aed33, BRF_GRA },		  	  //  7 Sprites
	{ "epr-10233.110",     0x008000, 0xc52784d3, BRF_GRA },		  	  //  8 Sprites
	{ "epr-10236.04",      0x008000, 0xb35ab227, BRF_GRA },		  	  //  9 Sprites
	{ "epr-10235.5",       0x008000, 0x006a3151, BRF_GRA },		  	  // 10 Sprites

	{ "pr-7345.3",         0x000100, 0x8eee0f72, BRF_GRA },		  	  // 11 Red PROM
	{ "pr-7344.2",         0x000100, 0x3e7babd7, BRF_GRA },		  	  // 12 Green PROM
	{ "pr-7343.1",         0x000100, 0x371c44a6, BRF_GRA },		  	  // 13 Blue PROM
	{ "pr5317.4",          0x000100, 0x648350b8, BRF_OPT },		  	  // 14 Timing PROM
};

STD_ROM_PICK(Gardia)
STD_ROM_FN(Gardia)

static struct BurnRomInfo GardiabRomDesc[] = {
	{ "gardiabl.5",        0x008000, 0x207f9cbb, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "gardiabl.6",        0x008000, 0xb2ed05dc, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "gardiabl.7",        0x008000, 0x0a490588, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code

	{ "epr-10243.120",     0x004000, 0x87220660, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program Code

	{ "gardiabl.8",        0x004000, 0x367c9a17, BRF_GRA },		  	  //  4 Tiles
	{ "gardiabl.9",        0x004000, 0x1540fd30, BRF_GRA },		  	  //  5 Tiles
	{ "gardiabl.10",       0x004000, 0xe5c9af10, BRF_GRA },		  	  //  6 Tiles

	{ "epr-10234.117",     0x008000, 0x8a6aed33, BRF_GRA },		  	  //  7 Sprites
	{ "epr-10233.110",     0x008000, 0xc52784d3, BRF_GRA },		  	  //  8 Sprites
	{ "epr-10236.04",      0x008000, 0xb35ab227, BRF_GRA },		  	  //  9 Sprites
	{ "epr-10235.5",       0x008000, 0x006a3151, BRF_GRA },		  	  // 10 Sprites

	{ "pr-7345.3",         0x000100, 0x8eee0f72, BRF_GRA },		  	  // 11 Red PROM
	{ "pr-7344.2",         0x000100, 0x3e7babd7, BRF_GRA },		  	  // 12 Green PROM
	{ "pr-7343.1",         0x000100, 0x371c44a6, BRF_GRA },		  	  // 13 Blue PROM
	{ "pr5317.4",          0x000100, 0x648350b8, BRF_OPT },		  	  // 14 Timing PROM
};

STD_ROM_PICK(Gardiab)
STD_ROM_FN(Gardiab)

static struct BurnRomInfo GardiajRomDesc[] = {
	{ "epr-10250.ic90",     0x8000, 0xc97943a7, BRF_ESS | BRF_PRG }, //  0 Z80 #1 Program Code
	{ "epr-10251.ic91",     0x8000, 0xb2ed05dc, BRF_ESS | BRF_PRG }, //  1
	{ "epr-10252.ic92",     0x8000, 0x0a490588, BRF_ESS | BRF_PRG }, //  2

	{ "epr-10243.ic126",    0x4000, 0x87220660, BRF_ESS | BRF_PRG }, //  3 Z80 #2 Program Code

	{ "epr-10240.ic4",      0x8000, 0x998ce090, BRF_GRA }, 			//  4 Tiles
	{ "epr-10241.ic5",      0x8000, 0x81ab0b07, BRF_GRA }, 			//  5
	{ "epr-10242.ic6",      0x8000, 0x2dc4c4c7, BRF_GRA }, 			//  6

	{ "epr-10234.ic87",     0x8000, 0x8a6aed33, BRF_GRA }, 			//  7 Sprites
	{ "epr-10233.ic86",     0x8000, 0xc52784d3, BRF_GRA }, 			//  8
	{ "epr-10236.ic89",     0x8000, 0xb35ab227, BRF_GRA }, 			//  9
	{ "epr-10235.ic88",     0x8000, 0x006a3151, BRF_GRA }, 			// 10

	{ "pr-7345.ic20",       0x0100, 0x8eee0f72, BRF_GRA }, 			// 11 Palette
	{ "pr-7344.ic14",       0x0100, 0x3e7babd7, BRF_GRA }, 			// 12
	{ "pr-7343.ic8",        0x0100, 0x371c44a6, BRF_GRA }, 			// 13
	{ "pr5317.ic28",        0x0100, 0x648350b8, BRF_OPT }, 			// 14 Timing proms
	
	{ "315-5137.bin",       0x0104, 0x6ffd9e6f, BRF_OPT }, 			// 15 Plds
	{ "315-5138.bin",       0x0104, 0xdd223015, BRF_OPT }, 			// 16
};

STD_ROM_PICK(Gardiaj)
STD_ROM_FN(Gardiaj)

static struct BurnRomInfo HvymetalRomDesc[] = {
	{ "epr-6790a.1",       0x008000, 0x59195bb9, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "epr-6789a.2",       0x008000, 0x83e1d18a, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "epr-6788a.3",       0x008000, 0x6ecefd57, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code

	{ "epr-6787.120",      0x008000, 0xb64ac7f0, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program Code

	{ "epr-6795.62",       0x004000, 0x58a3d038, BRF_GRA },		  	  //  4 Tiles
	{ "epr-6796.61",       0x004000, 0xd8b08a55, BRF_GRA },		  	  //  5 Tiles
	{ "epr-6793.64",       0x004000, 0x487407c2, BRF_GRA },		  	  //  6 Tiles
	{ "epr-6794.63",       0x004000, 0x89eb3793, BRF_GRA },		  	  //  7 Tiles
	{ "epr-6791.66",       0x004000, 0xa7dcd042, BRF_GRA },		  	  //  8 Tiles
	{ "epr-6792.65",       0x004000, 0xd0be5e33, BRF_GRA },		  	  //  9 Tiles

	{ "epr-6778.117",      0x008000, 0x0af61aee, BRF_GRA },		  	  // 10 Sprites
	{ "epr-6777.110",      0x008000, 0x91d7a197, BRF_GRA },		  	  // 11 Sprites
	{ "epr-6780.4",        0x008000, 0x55b31df5, BRF_GRA },		  	  // 12 Sprites
	{ "epr-6779.5",        0x008000, 0xe03a2b28, BRF_GRA },		  	  // 13 Sprites

	{ "pr7036.3",          0x000100, 0x146f16fb, BRF_GRA },		  	  // 14 Red PROM
	{ "pr7035.2",          0x000100, 0x50b201ed, BRF_GRA },		  	  // 15 Green PROM
	{ "pr7034.1",          0x000100, 0xdfb5f139, BRF_GRA },		  	  // 16 Blue PROM

	{ "pr5317p.4",         0x000100, 0x648350b8, BRF_OPT },		  	  // 17 Timing PROM
};

STD_ROM_PICK(Hvymetal)
STD_ROM_FN(Hvymetal)

static struct BurnRomInfo ImsorryRomDesc[] = {
	{ "epr-6676.116",      0x004000, 0xeb087d7f, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "epr-6677.109",      0x004000, 0xbd244bee, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "epr-6678.96",       0x004000, 0x2e16b9fd, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code

	{ "epr-6656.120",      0x002000, 0x25e3d685, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program Code

	{ "epr-6684.62",       0x002000, 0x2c8df377, BRF_GRA },		  	  //  4 Tiles
	{ "epr-6683.61",       0x002000, 0x89431c48, BRF_GRA },		  	  //  5 Tiles
	{ "epr-6682.64",       0x002000, 0x256a9246, BRF_GRA },		  	  //  6 Tiles
	{ "epr-6681.63",       0x002000, 0x6974d189, BRF_GRA },		  	  //  7 Tiles
	{ "epr-6680.66",       0x002000, 0x10a629d6, BRF_GRA },		  	  //  8 Tiles
	{ "epr-6674.65",       0x002000, 0x143d883c, BRF_GRA },		  	  //  9 Tiles

	{ "epr-6645.117",      0x004000, 0x1ba167ee, BRF_GRA },		  	  // 10 Sprites
	{ "epr-6646.04",       0x004000, 0xedda7ad6, BRF_GRA },		  	  // 11 Sprites

	{ "pr-5317.76",        0x000100, 0x648350b8, BRF_OPT },		  	  // 12 Timing PROM
};

STD_ROM_PICK(Imsorry)
STD_ROM_FN(Imsorry)

static struct BurnRomInfo ImsorryjRomDesc[] = {
	{ "epr-6647.116",      0x004000, 0xcc5d915d, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "epr-6648.109",      0x004000, 0x37574d60, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "epr-6649.96",       0x004000, 0x5f59bdee, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code

	{ "epr-6656.120",      0x002000, 0x25e3d685, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program Code

	{ "epr-6655.62",       0x002000, 0xbe1f762f, BRF_GRA },		  	  //  4 Tiles
	{ "epr-6654.61",       0x002000, 0xed5f7fc8, BRF_GRA },		  	  //  5 Tiles
	{ "epr-6653.64",       0x002000, 0x8b4845a7, BRF_GRA },		  	  //  6 Tiles
	{ "epr-6652.63",       0x002000, 0x001d68cb, BRF_GRA },		  	  //  7 Tiles
	{ "epr-6651.66",       0x002000, 0x4ee9b5e6, BRF_GRA },		  	  //  8 Tiles
	{ "epr-6650.65",       0x002000, 0x3fca4414, BRF_GRA },		  	  //  9 Tiles

	{ "epr-6645.117",      0x004000, 0x1ba167ee, BRF_GRA },		  	  // 10 Sprites
	{ "epr-6646.04",       0x004000, 0xedda7ad6, BRF_GRA },		  	  // 11 Sprites

	{ "pr-5317.76",        0x000100, 0x648350b8, BRF_OPT },		  	  // 12 Timing PROM
};

STD_ROM_PICK(Imsorryj)
STD_ROM_FN(Imsorryj)

static struct BurnRomInfo MrvikingRomDesc[] = {
	{ "epr-5873.129",      0x002000, 0x14d21624, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "epr-5874.130",      0x002000, 0x6df7de87, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "epr-5875.131",      0x002000, 0xac226100, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code
	{ "epr-5876.132",      0x002000, 0xe77db1dc, BRF_ESS | BRF_PRG }, //  3	Z80 #1 Program Code
	{ "epr-5755.133",      0x002000, 0xedd62ae1, BRF_ESS | BRF_PRG }, //  4	Z80 #1 Program Code
	{ "epr-5756.134",      0x002000, 0x11974040, BRF_ESS | BRF_PRG }, //  5	Z80 #1 Program Code

	{ "epr-5763.3",        0x002000, 0xd712280d, BRF_ESS | BRF_PRG }, //  6	Z80 #2 Program Code

	{ "epr-5762.82",       0x002000, 0x4a91d08a, BRF_GRA },		  	  //  7 Tiles
	{ "epr-5761.65",       0x002000, 0xf7d61b65, BRF_GRA },		  	  //  8 Tiles
	{ "epr-5760.81",       0x002000, 0x95045820, BRF_GRA },		  	  //  9 Tiles
	{ "epr-5759.64",       0x002000, 0x5f9bae4e, BRF_GRA },		  	  // 10 Tiles
	{ "epr-5758.80",       0x002000, 0x808ee706, BRF_GRA },		  	  // 11 Tiles
	{ "epr-5757.63",       0x002000, 0x480f7074, BRF_GRA },		  	  // 12 Tiles

	{ "epr-5749.86",       0x004000, 0xe24682cd, BRF_GRA },		  	  // 13 Sprites
	{ "epr-5750.93",       0x004000, 0x6564d1ad, BRF_GRA },		  	  // 14 Sprites

	{ "pr-5317.106",       0x000100, 0x648350b8, BRF_OPT },		  	  // 15 Timing PROM
};

STD_ROM_PICK(Mrviking)
STD_ROM_FN(Mrviking)

static struct BurnRomInfo MrvikingjRomDesc[] = {
	{ "epr-5751.129",      0x002000, 0xae97a4c5, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "epr-5752.130",      0x002000, 0xd48e6726, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "epr-5753.131",      0x002000, 0x28c60887, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code
	{ "epr-5754.132",      0x002000, 0x1f47ed02, BRF_ESS | BRF_PRG }, //  3	Z80 #1 Program Code
	{ "epr-5755.133",      0x002000, 0xedd62ae1, BRF_ESS | BRF_PRG }, //  4	Z80 #1 Program Code
	{ "epr-5756.134",      0x002000, 0x11974040, BRF_ESS | BRF_PRG }, //  5	Z80 #1 Program Code

	{ "epr-5763.3",        0x002000, 0xd712280d, BRF_ESS | BRF_PRG }, //  6	Z80 #2 Program Code

	{ "epr-5762.82",       0x002000, 0x4a91d08a, BRF_GRA },		  	  //  7 Tiles
	{ "epr-5761.65",       0x002000, 0xf7d61b65, BRF_GRA },		  	  //  8 Tiles
	{ "epr-5760.81",       0x002000, 0x95045820, BRF_GRA },		  	  //  9 Tiles
	{ "epr-5759.64",       0x002000, 0x5f9bae4e, BRF_GRA },		  	  // 10 Tiles
	{ "epr-5758.80",       0x002000, 0x808ee706, BRF_GRA },		  	  // 11 Tiles
	{ "epr-5757.63",       0x002000, 0x480f7074, BRF_GRA },		  	  // 12 Tiles

	{ "epr-5749.86",       0x004000, 0xe24682cd, BRF_GRA },		  	  // 13 Sprites
	{ "epr-5750.93",       0x004000, 0x6564d1ad, BRF_GRA },		  	  // 14 Sprites

	{ "pr-5317.106",       0x000100, 0x648350b8, BRF_OPT },		  	  // 15 Timing PROM
};

STD_ROM_PICK(Mrvikingj)
STD_ROM_FN(Mrvikingj)

static struct BurnRomInfo MyheroRomDesc[] = {
	{ "epr-6963b.116",     0x004000, 0x4daf89d4, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "epr-6964a.109",     0x004000, 0xc26188e5, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "epr-6927.96",       0x004000, 0x3cbbaf64, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code

	{ "epr-69xx.120",      0x002000, 0x0039e1e9, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program Code

	{ "epr-6966.62",       0x002000, 0x157f0401, BRF_GRA },		  	  //  4 Tiles
	{ "epr-6961.61",       0x002000, 0xbe53ce47, BRF_GRA },		  	  //  5 Tiles
	{ "epr-6960.64",       0x002000, 0xbd381baa, BRF_GRA },		  	  //  6 Tiles
	{ "epr-6959.63",       0x002000, 0xbc04e79a, BRF_GRA },		  	  //  7 Tiles
	{ "epr-6958.66",       0x002000, 0x714f2c26, BRF_GRA },		  	  //  8 Tiles
	{ "epr-6957.65",       0x002000, 0x80920112, BRF_GRA },		  	  //  9 Tiles

	{ "epr-6921.117",      0x004000, 0xf19e05a1, BRF_GRA },		  	  // 10 Sprites
	{ "epr-6923.04",       0x004000, 0x7988adc3, BRF_GRA },		  	  // 11 Sprites
	{ "epr-6922.110",      0x004000, 0x37f77a78, BRF_GRA },		  	  // 12 Sprites
	{ "epr-6924.05",       0x004000, 0x42bdc8f6, BRF_GRA },		  	  // 13 Sprites

	{ "pr-5317.76",        0x000100, 0x648350b8, BRF_OPT },		  	  // 14 Timing PROM
};

STD_ROM_PICK(Myhero)
STD_ROM_FN(Myhero)

static struct BurnRomInfo SscandalRomDesc[] = {
	{ "epr-6925b.116",     0x004000, 0xff54dcec, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "epr-6926a.109",     0x004000, 0x5c41eea8, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "epr-6927.96",       0x004000, 0x3cbbaf64, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code

	{ "epr-6934.120",      0x002000, 0xaf467223, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program Code

	{ "epr-6933.62",       0x002000, 0xe7304036, BRF_GRA },		  	  //  4 Tiles
	{ "epr-6932.61",       0x002000, 0xf5cfbfda, BRF_GRA },		  	  //  5 Tiles
	{ "epr-6931.64",       0x002000, 0x599d7f87, BRF_GRA },		  	  //  6 Tiles
	{ "epr-6930.63",       0x002000, 0xcb6616c2, BRF_GRA },		  	  //  7 Tiles
	{ "epr-6929.66",       0x002000, 0x27a16856, BRF_GRA },		  	  //  8 Tiles
	{ "epr-6928.65",       0x002000, 0xc0c9cfa4, BRF_GRA },		  	  //  9 Tiles

	{ "epr-6921.117",      0x004000, 0xf19e05a1, BRF_GRA },		  	  // 10 Sprites
	{ "epr-6923.04",       0x004000, 0x7988adc3, BRF_GRA },		  	  // 11 Sprites
	{ "epr-6922.110",      0x004000, 0x37f77a78, BRF_GRA },		  	  // 12 Sprites
	{ "epr-6924.05",       0x004000, 0x42bdc8f6, BRF_GRA },		  	  // 13 Sprites

	{ "pr-5317.76",        0x000100, 0x648350b8, BRF_OPT },		  	  // 14 Timing PROM
};

STD_ROM_PICK(Sscandal)
STD_ROM_FN(Sscandal)

static struct BurnRomInfo MyherokRomDesc[] = {
	{ "ry-11.rom",         0x004000, 0x6f4c8ee5, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "ry-09.rom",         0x004000, 0x369302a1, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "ry-07.rom",         0x004000, 0xb8e9922e, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code

	{ "epr-6934.120",      0x002000, 0xaf467223, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program Code

	{ "ry-04.rom",         0x004000, 0xdfb75143, BRF_GRA },		  	  //  4 Tiles
	{ "ry-03.rom",         0x004000, 0xcf68b4a2, BRF_GRA },		  	  //  5 Tiles
	{ "ry-02.rom",         0x004000, 0xd100eaef, BRF_GRA },		  	  //  6 Tiles

	{ "epr-6921.117",      0x004000, 0xf19e05a1, BRF_GRA },		  	  //  7 Sprites
	{ "epr-6923.04",       0x004000, 0x7988adc3, BRF_GRA },		  	  //  8 Sprites
	{ "epr-6922.110",      0x004000, 0x37f77a78, BRF_GRA },		  	  //  9 Sprites
	{ "epr-6924.05",       0x004000, 0x42bdc8f6, BRF_GRA },		  	  // 10 Sprites

	{ "pr-5317.76",        0x000100, 0x648350b8, BRF_OPT },		  	  // 11 Timing PROM
};

STD_ROM_PICK(Myherok)
STD_ROM_FN(Myherok)

static struct BurnRomInfo MyheroblRomDesc[] = {
	{ "1.f2",         	   0x004000, 0xc1d354dc, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "2.g2",         	   0x004000, 0x688c9ede, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "3.h2",              0x004000, 0x3cbbaf64, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code

	{ "6.e10",      	   0x002000, 0xaf467223, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program Code

	{ "b13.r10",           0x004000, 0x9a4861b1, BRF_GRA },		  	  //  4 Tiles
	{ "b11.r7",            0x004000, 0x0d6f248a, BRF_GRA },		  	  //  5 Tiles
	{ "x.r8",              0x004000, 0x24537709, BRF_GRA },		  	  //  6 Tiles

	{ "4.f4",      		   0x004000, 0xf19e05a1, BRF_GRA },		  	  //  7 Sprites
	{ "x.h4",              0x004000, 0x7988adc3, BRF_GRA },		  	  //  8 Sprites
	{ "x.g4",      		   0x004000, 0x37f77a78, BRF_GRA },		  	  //  9 Sprites
	{ "b7.k4",       	   0x004000, 0x42bdc8f6, BRF_GRA },		  	  // 10 Sprites

	{ "pr-5317.76",        0x000100, 0x648350b8, BRF_OPT },		  	  // 11 Timing PROM

	{ "prom.a2",           0x000200, 0x4fcaf000, BRF_OPT },       	  // 12 Timing PROMBL
};

STD_ROM_PICK(Myherobl)
STD_ROM_FN(Myherobl)

static struct BurnRomInfo NobRomDesc[] = {
	{ "dm08.1f",           0x008000, 0x98d602d6, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "dm10.1k",           0x008000, 0xe7c06663, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "dm09.1h",           0x008000, 0xdc4c872f, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code

	{ "dm03.9h",           0x004000, 0x415adf76, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program Code

	{ "dm.bin",            0x001000, 0x6fde9dcb, BRF_ESS | BRF_PRG }, //  4 MCU

	{ "dm01.12b",          0x008000, 0x446fbcdd, BRF_GRA },		  	  //  5 Tiles
	{ "dm02.13b",          0x008000, 0xf12df039, BRF_GRA },		  	  //  6 Tiles
	{ "dm00.10b",          0x008000, 0x35f396df, BRF_GRA },		  	  //  7 Tiles

	{ "dm04.5f",           0x008000, 0x2442b86d, BRF_GRA },		  	  //  8 Sprites
	{ "dm06.5k",           0x008000, 0xe33743a6, BRF_GRA },		  	  //  9 Sprites
	{ "dm05.5h",           0x008000, 0x7fbba01d, BRF_GRA },		  	  // 10 Sprites
	{ "dm07.5l",           0x008000, 0x85e7a29f, BRF_GRA },		  	  // 11 Sprites

	{ "nobo_pr.16d",       0x000100, 0x95010ac2, BRF_GRA },		  	  // 12 Red PROM
	{ "nobo_pr.15d",       0x000100, 0xc55aac0c, BRF_GRA },		  	  // 13 Green PROM
	{ "dm-12.ic3",         0x000100, 0xde394cee, BRF_GRA },		  	  // 14 Blue PROM
	{ "dc-11.6a",          0x000100, 0x648350b8, BRF_OPT },		  	  // 15 Timing PROM
};

STD_ROM_PICK(Nob)
STD_ROM_FN(Nob)

static struct BurnRomInfo NobbRomDesc[] = {
	{ "nobo-t.bin",        0x008000, 0x176fd168, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "nobo-r.bin",        0x008000, 0xd61cf3c9, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "nobo-s.bin",        0x008000, 0xb0e7697f, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code

	{ "nobo-m.bin",        0x004000, 0x415adf76, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program Code

	{ "nobo-k.bin",        0x008000, 0x446fbcdd, BRF_GRA },		  	  //  4 Tiles
	{ "nobo-j.bin",        0x008000, 0xf12df039, BRF_GRA },		  	  //  5 Tiles
	{ "nobo-l.bin",        0x008000, 0x35f396df, BRF_GRA },		  	  //  6 Tiles

	{ "nobo-q.bin",        0x008000, 0x2442b86d, BRF_GRA },		  	  //  7 Sprites
	{ "nobo-o.bin",        0x008000, 0xe33743a6, BRF_GRA },		  	  //  8 Sprites
	{ "nobo-p.bin",        0x008000, 0x7fbba01d, BRF_GRA },		  	  //  9 Sprites
	{ "nobo-n.bin",        0x008000, 0x85e7a29f, BRF_GRA },		  	  // 10 Sprites

	{ "nobo_pr.16d",       0x000100, 0x95010ac2, BRF_GRA },		  	  // 11 Red PROM
	{ "nobo_pr.15d",       0x000100, 0xc55aac0c, BRF_GRA },		  	  // 12 Green PROM
	{ "nobo_pr.14d",       0x000100, 0xde394cee, BRF_GRA },		  	  // 13 Blue PROM
	{ "nobo_pr.13a",       0x000100, 0x648350b8, BRF_OPT },		  	  // 14 Timing PROM
};

STD_ROM_PICK(Nobb)
STD_ROM_FN(Nobb)

static struct BurnRomInfo Pitfall2RomDesc[] = {
	{ "epr-6456a.116",     0x004000, 0xbcc8406b, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "epr-6457a.109",     0x004000, 0xa016fd2a, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "epr-6458a.96",      0x004000, 0x5c30b3e8, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code

	{ "epr-6462.120",      0x002000, 0x86bb9185, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program Code

	{ "epr-6474a.62",      0x002000, 0x9f1711b9, BRF_GRA },		  	  //  4 Tiles
	{ "epr-6473a.61",      0x002000, 0x8e53b8dd, BRF_GRA },		  	  //  5 Tiles
	{ "epr-6472a.64",      0x002000, 0xe0f34a11, BRF_GRA },		  	  //  6 Tiles
	{ "epr-6471a.63",      0x002000, 0xd5bc805c, BRF_GRA },		  	  //  7 Tiles
	{ "epr-6470a.66",      0x002000, 0x1439729f, BRF_GRA },		  	  //  8 Tiles
	{ "epr-6469a.65",      0x002000, 0xe4ac6921, BRF_GRA },		  	  //  9 Tiles

	{ "epr-6454a.117",     0x004000, 0xa5d96780, BRF_GRA },		  	  // 10 Sprites
	{ "epr-6455.05",       0x004000, 0x32ee64a1, BRF_GRA },		  	  // 11 Sprites

	{ "pr-5317.76",        0x000100, 0x648350b8, BRF_OPT },		  	  // 12 Timing PROM
};

STD_ROM_PICK(Pitfall2)
STD_ROM_FN(Pitfall2)

static struct BurnRomInfo Pitfall2aRomDesc[] = {
	{ "epr-6505.116",      0x004000, 0xb6769739, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "epr-6506.109",      0x004000, 0x1ce6aec4, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "epr-6458a.96",      0x004000, 0x5c30b3e8, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code

	{ "epr-6462.120",      0x002000, 0x86bb9185, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program Code

	{ "epr-6474a.62",      0x002000, 0x9f1711b9, BRF_GRA },		  	  //  4 Tiles
	{ "epr-6473a.61",      0x002000, 0x8e53b8dd, BRF_GRA },		  	  //  5 Tiles
	{ "epr-6472a.64",      0x002000, 0xe0f34a11, BRF_GRA },		  	  //  6 Tiles
	{ "epr-6471a.63",      0x002000, 0xd5bc805c, BRF_GRA },		  	  //  7 Tiles
	{ "epr-6470a.66",      0x002000, 0x1439729f, BRF_GRA },		  	  //  8 Tiles
	{ "epr-6469a.65",      0x002000, 0xe4ac6921, BRF_GRA },		  	  //  9 Tiles

	{ "epr-6454a.117",     0x004000, 0xa5d96780, BRF_GRA },		  	  // 10 Sprites
	{ "epr-6455.05",       0x004000, 0x32ee64a1, BRF_GRA },		  	  // 11 Sprites

	{ "pr-5317.76",        0x000100, 0x648350b8, BRF_OPT },		  	  // 12 Timing PROM
};

STD_ROM_PICK(Pitfall2a)
STD_ROM_FN(Pitfall2a)

static struct BurnRomInfo Pitfall2uRomDesc[] = {
	{ "epr-6623.116",      0x004000, 0xbcb47ed6, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "epr-6624a.109",     0x004000, 0x6e8b09c1, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "epr-6625.96",       0x004000, 0xdc5484ba, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code

	{ "epr-6462.120",      0x002000, 0x86bb9185, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program Code

	{ "epr-6474a.62",      0x002000, 0x9f1711b9, BRF_GRA },		  	  //  4 Tiles
	{ "epr-6473a.61",      0x002000, 0x8e53b8dd, BRF_GRA },		  	  //  5 Tiles
	{ "epr-6472a.64",      0x002000, 0xe0f34a11, BRF_GRA },		  	  //  6 Tiles
	{ "epr-6471a.63",      0x002000, 0xd5bc805c, BRF_GRA },		  	  //  7 Tiles
	{ "epr-6470a.66",      0x002000, 0x1439729f, BRF_GRA },		  	  //  8 Tiles
	{ "epr-6469a.65",      0x002000, 0xe4ac6921, BRF_GRA },		  	  //  9 Tiles

	{ "epr-6454a.117",     0x004000, 0xa5d96780, BRF_GRA },		  	  // 10 Sprites
	{ "epr-6455.05",       0x004000, 0x32ee64a1, BRF_GRA },		  	  // 11 Sprites

	{ "pr-5317.76",        0x000100, 0x648350b8, BRF_OPT },		  	  // 12 Timing PROM
};

STD_ROM_PICK(Pitfall2u)
STD_ROM_FN(Pitfall2u)

static struct BurnRomInfo RaflesiaRomDesc[] = {
	{ "epr-7411.116",      0x004000, 0x88a0c6c6, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "epr-7412.109",      0x004000, 0xd3b8cddf, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "epr-7413.96",       0x004000, 0xb7e688b3, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code

	{ "epr-7420.120",      0x002000, 0x14387666, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program Code

	{ "epr-7419.62",       0x002000, 0xbfd5f34c, BRF_GRA },		  	  //  4 Tiles
	{ "epr-7418.61",       0x002000, 0xf8cbc9b6, BRF_GRA },		  	  //  5 Tiles
	{ "epr-7417.64",       0x002000, 0xe63501bc, BRF_GRA },		  	  //  6 Tiles
	{ "epr-7416.63",       0x002000, 0x093e5693, BRF_GRA },		  	  //  7 Tiles
	{ "epr-7415.66",       0x002000, 0x1a8d6bd6, BRF_GRA },		  	  //  8 Tiles
	{ "epr-7414.65",       0x002000, 0x5d20f218, BRF_GRA },		  	  //  9 Tiles

	{ "epr-7407.117",      0x004000, 0xf09fc057, BRF_GRA },		  	  // 10 Sprites
	{ "epr-7409.04",       0x004000, 0x819fedb8, BRF_GRA },		  	  // 11 Sprites
	{ "epr-7408.110",      0x004000, 0x3189f33c, BRF_GRA },		  	  // 12 Sprites
	{ "epr-7410.05",       0x004000, 0xced74789, BRF_GRA },		  	  // 13 Sprites

	{ "pr-5317.76",        0x000100, 0x648350b8, BRF_OPT },		  	  // 14 Timing PROM
};

STD_ROM_PICK(Raflesia)
STD_ROM_FN(Raflesia)

static struct BurnRomInfo RaflesiauRomDesc[] = {
	{ "epr-7433.129",      0x002000, 0x6f4931b0, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "epr-7434.130",      0x002000, 0xec46e21b, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "epr-7435.131",      0x002000, 0xe035ff6b, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code
	{ "epr-7436.132",      0x002000, 0x6527aae7, BRF_ESS | BRF_PRG }, //  3	Z80 #1 Program Code
	{ "epr-7437.133",      0x002000, 0xe13dd5e4, BRF_ESS | BRF_PRG }, //  4	Z80 #1 Program Code
	{ "epr-7438.134",      0x002000, 0xa0aa4729, BRF_ESS | BRF_PRG }, //  5	Z80 #1 Program Code

	{ "epr-7420.120",      0x002000, 0x14387666, BRF_ESS | BRF_PRG }, //  6	Z80 #2 Program Code

	{ "epr-7419.82",       0x002000, 0xbfd5f34c, BRF_GRA },		  	  //  4 Tiles
	{ "epr-7418.65",       0x002000, 0xf8cbc9b6, BRF_GRA },		  	  //  5 Tiles
	{ "epr-7417.81",       0x002000, 0xe63501bc, BRF_GRA },		  	  //  6 Tiles
	{ "epr-7416.64",       0x002000, 0x093e5693, BRF_GRA },		  	  //  7 Tiles
	{ "epr-7415.80",       0x002000, 0x1a8d6bd6, BRF_GRA },		  	  //  8 Tiles
	{ "epr-7414.63",       0x002000, 0x5d20f218, BRF_GRA },		  	  //  9 Tiles

	{ "epr-7407.3",        0x004000, 0xf09fc057, BRF_GRA },		  	  // 10 Sprites
	{ "epr-7409.1",        0x004000, 0x819fedb8, BRF_GRA },		  	  // 11 Sprites
	{ "epr-7408.4",        0x004000, 0x3189f33c, BRF_GRA },		  	  // 12 Sprites
	{ "epr-7410.2",        0x004000, 0xced74789, BRF_GRA },		  	  // 13 Sprites

	{ "pr-5317.106",       0x000100, 0x648350b8, BRF_OPT },		  	  // 14 Timing PROM
};

STD_ROM_PICK(Raflesiau)
STD_ROM_FN(Raflesiau)

static struct BurnRomInfo RegulusRomDesc[] = {
	{ "epr-5640a.129",     0x002000, 0xdafb1528, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "epr-5641a.130",     0x002000, 0x0fcc850e, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "epr-5642a.131",     0x002000, 0x4feffa17, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code
	{ "epr-5643a.132",     0x002000, 0xb8ac7eb4, BRF_ESS | BRF_PRG }, //  3	Z80 #1 Program Code
	{ "epr-5644.133",      0x002000, 0xffd05b7d, BRF_ESS | BRF_PRG }, //  4	Z80 #1 Program Code
	{ "epr-5645a.134",     0x002000, 0x6b4bf77c, BRF_ESS | BRF_PRG }, //  5	Z80 #1 Program Code

	{ "epr-5652.3",        0x002000, 0x74edcb98, BRF_ESS | BRF_PRG }, //  6	Z80 #2 Program Code

	{ "epr-5651.82",       0x002000, 0xf07f3e82, BRF_GRA },		  //  7 Tiles
	{ "epr-5650.65",       0x002000, 0x84c1baa2, BRF_GRA },		  //  8 Tiles
	{ "epr-5649.81",       0x002000, 0x6774c895, BRF_GRA },		  //  9 Tiles
	{ "epr-5648.64",       0x002000, 0x0c69e92a, BRF_GRA },		  //  10 Tiles
	{ "epr-5647.80",       0x002000, 0x9330f7b5, BRF_GRA },		  //  11 Tiles
	{ "epr-5646.63",       0x002000, 0x4dfacbbc, BRF_GRA },		  //  12 Tiles

	{ "epr-5638.86",       0x004000, 0x617363dd, BRF_GRA },		  //  13 Sprites
	{ "epr-5639.93",       0x004000, 0xa4ec5131, BRF_GRA },		  //  14 Sprites

	{ "pr-5317.106",       0x000100, 0x648350b8, BRF_OPT },		  //  15 Timing PROM
};

STD_ROM_PICK(Regulus)
STD_ROM_FN(Regulus)

static struct BurnRomInfo RegulusoRomDesc[] = {
	{ "epr-5640.129",      0x002000, 0x8324d0d4, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "epr-5641.130",      0x002000, 0x0a09f5c7, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "epr-5642.131",      0x002000, 0xff27b2f6, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code
	{ "epr-5643.132",      0x002000, 0x0d867df0, BRF_ESS | BRF_PRG }, //  3	Z80 #1 Program Code
	{ "epr-5644.133",      0x002000, 0xffd05b7d, BRF_ESS | BRF_PRG }, //  4	Z80 #1 Program Code
	{ "epr-5645.134",      0x002000, 0x57a2b4b4, BRF_ESS | BRF_PRG }, //  5	Z80 #1 Program Code

	{ "epr-5652.3",        0x002000, 0x74edcb98, BRF_ESS | BRF_PRG }, //  6	Z80 #2 Program Code

	{ "epr-5651.82",       0x002000, 0xf07f3e82, BRF_GRA },		  //  7 Tiles
	{ "epr-5650.65",       0x002000, 0x84c1baa2, BRF_GRA },		  //  8 Tiles
	{ "epr-5649.81",       0x002000, 0x6774c895, BRF_GRA },		  //  9 Tiles
	{ "epr-5648.64",       0x002000, 0x0c69e92a, BRF_GRA },		  //  10 Tiles
	{ "epr-5647.80",       0x002000, 0x9330f7b5, BRF_GRA },		  //  11 Tiles
	{ "epr-5646.63",       0x002000, 0x4dfacbbc, BRF_GRA },		  //  12 Tiles

	{ "epr-5638.86",       0x004000, 0x617363dd, BRF_GRA },		  //  13 Sprites
	{ "epr-5639.93",       0x004000, 0xa4ec5131, BRF_GRA },		  //  14 Sprites

	{ "pr-5317.106",       0x000100, 0x648350b8, BRF_OPT },		  //  15 Timing PROM
};

STD_ROM_PICK(Reguluso)
STD_ROM_FN(Reguluso)

static struct BurnRomInfo RegulusuRomDesc[] = {
	{ "epr-5950.129",      0x002000, 0x3b047b67, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "epr-5951.130",      0x002000, 0xd66453ab, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "epr-5952.131",      0x002000, 0xf3d0158a, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code
	{ "epr-5953.132",      0x002000, 0xa9ad4f44, BRF_ESS | BRF_PRG }, //  3	Z80 #1 Program Code
	{ "epr-5644.133",      0x002000, 0xffd05b7d, BRF_ESS | BRF_PRG }, //  4	Z80 #1 Program Code
	{ "epr-5955.134",      0x002000, 0x65ddb2a3, BRF_ESS | BRF_PRG }, //  5	Z80 #1 Program Code

	{ "epr-5652.3",        0x002000, 0x74edcb98, BRF_ESS | BRF_PRG }, //  6	Z80 #2 Program Code

	{ "epr-5651.82",       0x002000, 0xf07f3e82, BRF_GRA },		  //  7 Tiles
	{ "epr-5650.65",       0x002000, 0x84c1baa2, BRF_GRA },		  //  8 Tiles
	{ "epr-5649.81",       0x002000, 0x6774c895, BRF_GRA },		  //  9 Tiles
	{ "epr-5648.64",       0x002000, 0x0c69e92a, BRF_GRA },		  //  10 Tiles
	{ "epr-5647.80",       0x002000, 0x9330f7b5, BRF_GRA },		  //  11 Tiles
	{ "epr-5646.63",       0x002000, 0x4dfacbbc, BRF_GRA },		  //  12 Tiles

	{ "epr-5638.86",       0x004000, 0x617363dd, BRF_GRA },		  //  13 Sprites
	{ "epr-5639.93",       0x004000, 0xa4ec5131, BRF_GRA },		  //  14 Sprites

	{ "pr-5317.106",       0x000100, 0x648350b8, BRF_OPT },		  //  15 Timing PROM
};

STD_ROM_PICK(Regulusu)
STD_ROM_FN(Regulusu)

static struct BurnRomInfo SeganinjRomDesc[] = {
	{ "epr-6594a.116",     0x004000, 0xa5d0c9d0, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "epr-6595a.109",     0x004000, 0xb9e6775c, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "epr-6596a.96",      0x004000, 0xf2eeb0d8, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code

	{ "epr-6559.120",      0x002000, 0x5a1570ee, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program Code

	{ "epr-6558.62",       0x002000, 0x2af9eaeb, BRF_GRA },		  //  4 Tiles
	{ "epr-6592.61",       0x002000, 0x7804db86, BRF_GRA },		  //  5 Tiles
	{ "epr-6556.64",       0x002000, 0x79fd26f7, BRF_GRA },		  //  6 Tiles
	{ "epr-6590.63",       0x002000, 0xbf858cad, BRF_GRA },		  //  7 Tiles
	{ "epr-6554.66",       0x002000, 0x5ac9d205, BRF_GRA },		  //  8 Tiles
	{ "epr-6588.65",       0x002000, 0xdc931dbb, BRF_GRA },		  //  9 Tiles

	{ "epr-6546.117",      0x004000, 0xa4785692, BRF_GRA },		  //  10 Sprites
	{ "epr-6548.04",       0x004000, 0xbdf278c1, BRF_GRA },		  //  11 Sprites
	{ "epr-6547.110",      0x004000, 0x34451b08, BRF_GRA },		  //  12 Sprites
	{ "epr-6549a.05",      0x004000, 0x7c51488c, BRF_GRA },		  //  13 Sprites

	{ "pr-5317.76",        0x000100, 0x648350b8, BRF_OPT },		  //  14 Timing PROM
};

STD_ROM_PICK(Seganinj)
STD_ROM_FN(Seganinj)

static struct BurnRomInfo SeganinjuRomDesc[] = {
	{ "epr-7149.116",      0x004000, 0xcd9fade7, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "epr-7150.109",      0x004000, 0xc36351e2, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "epr-7151.96",       0x004000, 0xf2eeb0d8, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code

	{ "epr-6559.120",      0x002000, 0x5a1570ee, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program Code

	{ "epr-6558.62",       0x002000, 0x2af9eaeb, BRF_GRA },		  //  4 Tiles
	{ "epr-6592.61",       0x002000, 0x7804db86, BRF_GRA },		  //  5 Tiles
	{ "epr-6556.64",       0x002000, 0x79fd26f7, BRF_GRA },		  //  6 Tiles
	{ "epr-6590.63",       0x002000, 0xbf858cad, BRF_GRA },		  //  7 Tiles
	{ "epr-6554.66",       0x002000, 0x5ac9d205, BRF_GRA },		  //  8 Tiles
	{ "epr-6588.65",       0x002000, 0xdc931dbb, BRF_GRA },		  //  9 Tiles

	{ "epr-6546.117",      0x004000, 0xa4785692, BRF_GRA },		  //  10 Sprites
	{ "epr-6548.04",       0x004000, 0xbdf278c1, BRF_GRA },		  //  11 Sprites
	{ "epr-6547.110",      0x004000, 0x34451b08, BRF_GRA },		  //  12 Sprites
	{ "epr-6549.05",       0x004000, 0xd2057668, BRF_GRA },		  //  13 Sprites

	{ "pr-5317.76",        0x000100, 0x648350b8, BRF_OPT },		  //  14 Timing PROM
};

STD_ROM_PICK(Seganinju)
STD_ROM_FN(Seganinju)

static struct BurnRomInfo SeganinjaRomDesc[] = {
	{ "epr-6879.116",      0x004000, 0xcae7e51f, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "epr-6880.109",      0x004000, 0x7af85e01, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "epr-6881.96",       0x004000, 0xf2eeb0d8, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code

	{ "epr-6559.120",      0x002000, 0x5a1570ee, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program Code

	{ "epr-6558.62",       0x002000, 0x2af9eaeb, BRF_GRA },		  //  4 Tiles
	{ "epr-6592.61",       0x002000, 0x7804db86, BRF_GRA },		  //  5 Tiles
	{ "epr-6556.64",       0x002000, 0x79fd26f7, BRF_GRA },		  //  6 Tiles
	{ "epr-6590.63",       0x002000, 0xbf858cad, BRF_GRA },		  //  7 Tiles
	{ "epr-6554.66",       0x002000, 0x5ac9d205, BRF_GRA },		  //  8 Tiles
	{ "epr-6588.65",       0x002000, 0xdc931dbb, BRF_GRA },		  //  9 Tiles

	{ "epr-6546.117",      0x004000, 0xa4785692, BRF_GRA },		  //  10 Sprites
	{ "epr-6548.04",       0x004000, 0xbdf278c1, BRF_GRA },		  //  11 Sprites
	{ "epr-6547.110",      0x004000, 0x34451b08, BRF_GRA },		  //  12 Sprites
	{ "epr-6549.05",       0x004000, 0xd2057668, BRF_GRA },		  //  13 Sprites

	{ "pr-5317.76",        0x000100, 0x648350b8, BRF_OPT },		  //  14 Timing PROM
};

STD_ROM_PICK(Seganinja)
STD_ROM_FN(Seganinja)

static struct BurnRomInfo NinjaRomDesc[] = {
	{ "epr-6594.116",      0x004000, 0x3ef0e5fc, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "epr-6595.109",      0x004000, 0xb16f13cd, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "epr-6552.96",       0x004000, 0xf2eeb0d8, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code

	{ "epr-6559.120",      0x002000, 0x5a1570ee, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program Code

	{ "epr-6558.62",       0x002000, 0x2af9eaeb, BRF_GRA },		  //  4 Tiles
	{ "epr-6592.61",       0x002000, 0x88d0c7a1, BRF_GRA },		  //  5 Tiles
	{ "epr-6556.64",       0x002000, 0x79fd26f7, BRF_GRA },		  //  6 Tiles
	{ "epr-6590.63",       0x002000, 0x956e3b61, BRF_GRA },		  //  7 Tiles
	{ "epr-6554.66",       0x002000, 0x5ac9d205, BRF_GRA },		  //  8 Tiles
	{ "epr-6588.65",       0x002000, 0x023a14a3, BRF_GRA },		  //  9 Tiles

	{ "epr-6546.117",      0x004000, 0xa4785692, BRF_GRA },		  //  10 Sprites
	{ "epr-6548.04",       0x004000, 0xbdf278c1, BRF_GRA },		  //  11 Sprites
	{ "epr-6547.110",      0x004000, 0x34451b08, BRF_GRA },		  //  12 Sprites
	{ "epr-6549.05",       0x004000, 0xd2057668, BRF_GRA },		  //  13 Sprites

	{ "pr-5317.76",        0x000100, 0x648350b8, BRF_OPT },		  //  14 Timing PROM
};

STD_ROM_PICK(Ninja)
STD_ROM_FN(Ninja)

static struct BurnRomInfo NprincesRomDesc[] = {
	{ "epr-6612.129",      0x002000, 0x1b30976f, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "epr-6613.130",      0x002000, 0x18281f27, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "epr-6614.131",      0x002000, 0x69fc3d73, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code
	{ "epr-6615.132",      0x002000, 0x1d0374c8, BRF_ESS | BRF_PRG }, //  3	Z80 #1 Program Code
	{ "epr-6616.133",      0x002000, 0x73616e03, BRF_ESS | BRF_PRG }, //  4	Z80 #1 Program Code
	{ "epr-6617.134",      0x002000, 0x20b6f895, BRF_ESS | BRF_PRG }, //  5	Z80 #1 Program Code

	{ "epr-6559.120",      0x002000, 0x5a1570ee, BRF_ESS | BRF_PRG }, //  6	Z80 #2 Program Code

	{ "epr-6558.62",       0x002000, 0x2af9eaeb, BRF_GRA },		  //  7 Tiles
	{ "epr-6557.61",       0x002000, 0x6eb131d0, BRF_GRA },		  //  8 Tiles
	{ "epr-6556.64",       0x002000, 0x79fd26f7, BRF_GRA },		  //  9 Tiles
	{ "epr-6555.63",       0x002000, 0x7f669aac, BRF_GRA },		  //  10 Tiles
	{ "epr-6554.66",       0x002000, 0x5ac9d205, BRF_GRA },		  //  11 Tiles
	{ "epr-6553.65",       0x002000, 0xeb82a8fe, BRF_GRA },		  //  12 Tiles

	{ "epr-6546.117",      0x004000, 0xa4785692, BRF_GRA },		  //  13 Sprites
	{ "epr-6548.04",       0x004000, 0xbdf278c1, BRF_GRA },		  //  14 Sprites
	{ "epr-6547.110",      0x004000, 0x34451b08, BRF_GRA },		  //  15 Sprites
	{ "epr-6549.05",       0x004000, 0xd2057668, BRF_GRA },		  //  16 Sprites

	{ "pr-5317.76",        0x000100, 0x648350b8, BRF_OPT },		  //  17 Timing PROM
};

STD_ROM_PICK(Nprinces)
STD_ROM_FN(Nprinces)

static struct BurnRomInfo NprincesoRomDesc[] = {
	{ "epr-6550.116",      0x004000, 0x5f6d59f1, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "epr-6551.109",      0x004000, 0x1af133b2, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "epr-6552.96",       0x004000, 0xf2eeb0d8, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code

	{ "epr-6559.120",      0x002000, 0x5a1570ee, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program Code

	{ "epr-6558.62",       0x002000, 0x2af9eaeb, BRF_GRA },		  //  4 Tiles
	{ "epr-6557.61",       0x002000, 0x6eb131d0, BRF_GRA },		  //  5 Tiles
	{ "epr-6556.64",       0x002000, 0x79fd26f7, BRF_GRA },		  //  6 Tiles
	{ "epr-6555.63",       0x002000, 0x7f669aac, BRF_GRA },		  //  7 Tiles
	{ "epr-6554.66",       0x002000, 0x5ac9d205, BRF_GRA },		  //  8 Tiles
	{ "epr-6553.65",       0x002000, 0xeb82a8fe, BRF_GRA },		  //  9 Tiles

	{ "epr-6546.117",      0x004000, 0xa4785692, BRF_GRA },		  //  10 Sprites
	{ "epr-6548.04",       0x004000, 0xbdf278c1, BRF_GRA },		  //  11 Sprites
	{ "epr-6547.110",      0x004000, 0x34451b08, BRF_GRA },		  //  12 Sprites
	{ "epr-6549.05",       0x004000, 0xd2057668, BRF_GRA },		  //  13 Sprites

	{ "pr-5317.76",        0x000100, 0x648350b8, BRF_OPT },		  //  14 Timing PROM
};

STD_ROM_PICK(Nprinceso)
STD_ROM_FN(Nprinceso)

static struct BurnRomInfo NprincesuRomDesc[] = {
	{ "epr-6573.129",      0x002000, 0xd2919c7d, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "epr-6574.130",      0x002000, 0x5a132833, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "epr-6575.131",      0x002000, 0xa94b0bd4, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code
	{ "epr-6576.132",      0x002000, 0x27d3bbdb, BRF_ESS | BRF_PRG }, //  3	Z80 #1 Program Code
	{ "epr-6577.133",      0x002000, 0x73616e03, BRF_ESS | BRF_PRG }, //  4	Z80 #1 Program Code
	{ "epr-6578.134",      0x002000, 0xab68499f, BRF_ESS | BRF_PRG }, //  5	Z80 #1 Program Code

	{ "epr-6559.120",      0x002000, 0x5a1570ee, BRF_ESS | BRF_PRG }, //  6	Z80 #2 Program Code

	{ "epr-6558.62",       0x002000, 0x2af9eaeb, BRF_GRA },		  //  7 Tiles
	{ "epr-6557.61",       0x002000, 0x6eb131d0, BRF_GRA },		  //  8 Tiles
	{ "epr-6556.64",       0x002000, 0x79fd26f7, BRF_GRA },		  //  9 Tiles
	{ "epr-6555.63",       0x002000, 0x7f669aac, BRF_GRA },		  //  10 Tiles
	{ "epr-6554.66",       0x002000, 0x5ac9d205, BRF_GRA },		  //  11 Tiles
	{ "epr-6553.65",       0x002000, 0xeb82a8fe, BRF_GRA },		  //  12 Tiles

	{ "epr-6546.117",      0x004000, 0xa4785692, BRF_GRA },		  //  13 Sprites
	{ "epr-6548.04",       0x004000, 0xbdf278c1, BRF_GRA },		  //  14 Sprites
	{ "epr-6547.110",      0x004000, 0x34451b08, BRF_GRA },		  //  15 Sprites
	{ "epr-6549.05",       0x004000, 0xd2057668, BRF_GRA },		  //  16 Sprites

	{ "pr-5317.76",        0x000100, 0x648350b8, BRF_OPT },		  //  17 Timing PROM
};

STD_ROM_PICK(Nprincesu)
STD_ROM_FN(Nprincesu)

static struct BurnRomInfo NprincesbRomDesc[] = {
	{ "nprinces.001",      0x004000, 0xe0de073c, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "nprinces.002",      0x004000, 0x27219c7f, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "epr-6552.96",       0x004000, 0xf2eeb0d8, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code

	{ "epr-6559.120",      0x002000, 0x5a1570ee, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program Code

	{ "epr-6558.62",       0x002000, 0x2af9eaeb, BRF_GRA },		  //  4 Tiles
	{ "epr-6557.61",       0x002000, 0x6eb131d0, BRF_GRA },		  //  5 Tiles
	{ "epr-6556.64",       0x002000, 0x79fd26f7, BRF_GRA },		  //  6 Tiles
	{ "epr-6555.63",       0x002000, 0x7f669aac, BRF_GRA },		  //  7 Tiles
	{ "epr-6554.66",       0x002000, 0x5ac9d205, BRF_GRA },		  //  8 Tiles
	{ "epr-6553.65",       0x002000, 0xeb82a8fe, BRF_GRA },		  //  9 Tiles

	{ "epr-6546.117",      0x004000, 0xa4785692, BRF_GRA },		  //  10 Sprites
	{ "epr-6548.04",       0x004000, 0xbdf278c1, BRF_GRA },		  //  11 Sprites
	{ "epr-6547.110",      0x004000, 0x34451b08, BRF_GRA },		  //  12 Sprites
	{ "epr-6549.05",       0x004000, 0xd2057668, BRF_GRA },		  //  13 Sprites

	{ "pr-5317.76",        0x000100, 0x648350b8, BRF_OPT },		  //  14 Timing PROM
	{ "nprinces.129",      0x000100, 0xae765f62, BRF_PRG },		  //  15 Decryption Table
	{ "nprinces.123",      0x000020, 0xed5146e9, BRF_PRG },		  //  15 Decryption Table
};

STD_ROM_PICK(Nprincesb)
STD_ROM_FN(Nprincesb)

static struct BurnRomInfo SpatterRomDesc[] = {
	{ "epr-6392.116",      0x004000, 0x329b4506, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "epr-6393.109",      0x004000, 0x3b56e25f, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "epr-6394.96",       0x004000, 0x647c1301, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code

	{ "epr-6316.120",      0x002000, 0x1df95511, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program Code

	{ "epr-6328.62",       0x002000, 0xa2bf2832, BRF_GRA },		  //  4 Tiles
	{ "epr-6397.61",       0x002000, 0xc60d4471, BRF_GRA },		  //  5 Tiles
	{ "epr-6326.64",       0x002000, 0x269fbb4c, BRF_GRA },		  //  6 Tiles
	{ "epr-6396.63",       0x002000, 0xc15ccf3b, BRF_GRA },		  //  7 Tiles
	{ "epr-6324.66",       0x002000, 0x8ab3b563, BRF_GRA },		  //  8 Tiles
	{ "epr-6395.65",       0x002000, 0x3f083065, BRF_GRA },		  //  9 Tiles

	{ "epr-6306.04",       0x004000, 0xe871e132, BRF_GRA },		  //  10 Sprites
	{ "epr-6308.117",      0x004000, 0x99c2d90e, BRF_GRA },		  //  11 Sprites
	{ "epr-6307.05",       0x004000, 0x0a5ad543, BRF_GRA },		  //  12 Sprites
	{ "epr-6309.110",      0x004000, 0x7423ad98, BRF_GRA },		  //  13 Sprites

	{ "pr-5317.106",       0x000100, 0x648350b8, BRF_OPT },		  //  14 Timing PROM
};

STD_ROM_PICK(Spatter)
STD_ROM_FN(Spatter)

static struct BurnRomInfo SsanchanRomDesc[] = {
	{ "epr-6310.116",      0x004000, 0x26b43701, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "epr-6311.109",      0x004000, 0xcb2bc620, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "epr-6312.96",       0x004000, 0x71b15b47, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code

	{ "epr-6316.120",      0x002000, 0x1df95511, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program Code

	{ "epr-6328.62",       0x002000, 0xa2bf2832, BRF_GRA },		  //  4 Tiles
	{ "epr-6327.61",       0x002000, 0x53298109, BRF_GRA },		  //  5 Tiles
	{ "epr-6326.64",       0x002000, 0x269fbb4c, BRF_GRA },		  //  6 Tiles
	{ "epr-6325.63",       0x002000, 0xbf038745, BRF_GRA },		  //  7 Tiles
	{ "epr-6324.66",       0x002000, 0x8ab3b563, BRF_GRA },		  //  8 Tiles
	{ "epr-6323.65",       0x002000, 0x0394673c, BRF_GRA },		  //  9 Tiles

	{ "epr-6306.04",       0x004000, 0xe871e132, BRF_GRA },		  //  10 Sprites
	{ "epr-6308.117",      0x004000, 0x99c2d90e, BRF_GRA },		  //  11 Sprites
	{ "epr-6307.05",       0x004000, 0x0a5ad543, BRF_GRA },		  //  12 Sprites
	{ "epr-6309.110",      0x004000, 0x7423ad98, BRF_GRA },		  //  13 Sprites

	{ "pr-5317.106",       0x000100, 0x648350b8, BRF_OPT },		  //  14 Timing PROM
};

STD_ROM_PICK(Ssanchan)
STD_ROM_FN(Ssanchan)

static struct BurnRomInfo StarjackRomDesc[] = {
	{ "epr-5320b.129",     0x002000, 0x7ab72ecd, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "epr-5321a.130",     0x002000, 0x38b99050, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "epr-5322a.131",     0x002000, 0x103a595b, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code
	{ "epr-5323.132",      0x002000, 0x46af0d58, BRF_ESS | BRF_PRG }, //  3	Z80 #1 Program Code
	{ "epr-5324.133",      0x002000, 0x1e89efe2, BRF_ESS | BRF_PRG }, //  4	Z80 #1 Program Code
	{ "epr-5325.134",      0x002000, 0xd6e379a1, BRF_ESS | BRF_PRG }, //  5	Z80 #1 Program Code

	{ "epr-5332.3",        0x002000, 0x7a72ab3d, BRF_ESS | BRF_PRG }, //  6	Z80 #2 Program Code

	{ "epr-5331.82",       0x002000, 0x251d898f, BRF_GRA },		  //  7 Tiles
	{ "epr-5330.65",       0x002000, 0xeb048745, BRF_GRA },		  //  8 Tiles
	{ "epr-5329.81",       0x002000, 0x3e8bcaed, BRF_GRA },		  //  9 Tiles
	{ "epr-5328.64",       0x002000, 0x9ed7849f, BRF_GRA },		  //  10 Tiles
	{ "epr-5327.80",       0x002000, 0x79e92cb1, BRF_GRA },		  //  11 Tiles
	{ "epr-5326.63",       0x002000, 0xba7e2b47, BRF_GRA },		  //  12 Tiles

	{ "epr-5318.86",       0x004000, 0x6f2e1fd3, BRF_GRA },		  //  13 Sprites
	{ "epr-5319.93",       0x004000, 0xebee4999, BRF_GRA },		  //  14 Sprites

	{ "pr-5317.106",       0x000100, 0x648350b8, BRF_OPT },		  //  15 Timing PROM
};

STD_ROM_PICK(Starjack)
STD_ROM_FN(Starjack)

static struct BurnRomInfo StarjacksRomDesc[] = {
	{ "star_jacker_a1_ic29.129",       	0x002000, 0x59a22a1f, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "star_jacker_a1_ic30.130",       	0x002000, 0x7f4597dc, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "star_jacker_a1_ic31.131",       	0x002000, 0x6074c046, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code
	{ "star_jacker_a1_ic32.132",       	0x002000, 0x1c48a3fa, BRF_ESS | BRF_PRG }, //  3	Z80 #1 Program Code
	{ "star_jacker_a1_ic33.133",       	0x002000, 0x7598bd51, BRF_ESS | BRF_PRG }, //  4	Z80 #1 Program Code
	{ "star_jacker_a1_ic34.134",       	0x002000, 0xf66fa604, BRF_ESS | BRF_PRG }, //  5	Z80 #1 Program Code

	{ "star_jacker_a1_ic3.3",          	0x002000, 0x7a72ab3d, BRF_ESS | BRF_PRG }, //  6	Z80 #2 Program Code

	{ "star_jacker_a1_ic82.82",       	0x002000, 0x251d898f, BRF_GRA },		  //  7 Tiles
	{ "star_jacker_a1_ic65.65",         0x002000, 0x0ab1893c, BRF_GRA },		  //  8 Tiles
	{ "epr-5456.81",       				0x002000, 0x3e8bcaed, BRF_GRA },		  //  9 Tiles
	{ "epr-5455.64",        			0x002000, 0x7f628ae6, BRF_GRA },		  //  10 Tiles
	{ "epr-5454.80",       				0x002000, 0x79e92cb1, BRF_GRA },		  //  11 Tiles
	{ "epr-5453.63",        			0x002000, 0x5bcb253e, BRF_GRA },		  //  12 Tiles

	{ "star_jacker_a1_ic86.86",        	0x004000, 0x6f2e1fd3, BRF_GRA },		  //  13 Sprites
	{ "epr-5446.93",        			0x004000, 0x07987244, BRF_GRA },		  //  14 Sprites

	{ "pr-5317.106",       				0x000100, 0x648350b8, BRF_OPT },		  //  15 Timing PROM
};

STD_ROM_PICK(Starjacks)
STD_ROM_FN(Starjacks)

static struct BurnRomInfo SwatRomDesc[] = {
	{ "epr5807b.129",      0x002000, 0x93db9c9f, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "epr-5808.130",      0x002000, 0x67116665, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "epr-5809.131",      0x002000, 0xfd792fc9, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code
	{ "epr-5810.132",      0x002000, 0xdc2b279d, BRF_ESS | BRF_PRG }, //  3	Z80 #1 Program Code
	{ "epr-5811.133",      0x002000, 0x093e3ab1, BRF_ESS | BRF_PRG }, //  4	Z80 #1 Program Code
	{ "epr-5812.134",      0x002000, 0x5bfd692f, BRF_ESS | BRF_PRG }, //  5	Z80 #1 Program Code

	{ "epr-5819.3",        0x002000, 0xf6afd0fd, BRF_ESS | BRF_PRG }, //  6	Z80 #2 Program Code

	{ "epr-5818.82",       0x002000, 0xb22033d9, BRF_GRA },		  //  7 Tiles
	{ "epr-5817.65",       0x002000, 0xfd942797, BRF_GRA },		  //  8 Tiles
	{ "epr-5816.81",       0x002000, 0x4384376d, BRF_GRA },		  //  9 Tiles
	{ "epr-5815.64",       0x002000, 0x16ad046c, BRF_GRA },		  //  10 Tiles
	{ "epr-5814.80",       0x002000, 0xbe721c99, BRF_GRA },		  //  11 Tiles
	{ "epr-5813.63",       0x002000, 0x0d42c27e, BRF_GRA },		  //  12 Tiles

	{ "epr-5805.86",       0x004000, 0x5a732865, BRF_GRA },		  //  13 Sprites
	{ "epr-5806.93",       0x004000, 0x26ac258c, BRF_GRA },		  //  14 Sprites

	{ "pr-5317.106",       0x000100, 0x648350b8, BRF_OPT },		  //  15 Timing PROM
};

STD_ROM_PICK(Swat)
STD_ROM_FN(Swat)

static struct BurnRomInfo TeddybbRomDesc[] = {
	{ "epr-6768.116",      0x004000, 0x5939817e, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "epr-6769.109",      0x004000, 0x14a98ddd, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "epr-6770.96",       0x004000, 0x67b0c7c2, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code

	{ "epr6748x.120",      0x002000, 0xc2a1b89d, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program Code

	{ "epr-6747.62",       0x002000, 0xa0e5aca7, BRF_GRA },		  //  4 Tiles
	{ "epr-6746.61",       0x002000, 0xcdb77e51, BRF_GRA },		  //  5 Tiles
	{ "epr-6745.64",       0x002000, 0x0cab75c3, BRF_GRA },		  //  6 Tiles
	{ "epr-6744.63",       0x002000, 0x0ef8d2cd, BRF_GRA },		  //  7 Tiles
	{ "epr-6743.66",       0x002000, 0xc33062b5, BRF_GRA },		  //  8 Tiles
	{ "epr-6742.65",       0x002000, 0xc457e8c5, BRF_GRA },		  //  9 Tiles

	{ "epr-6735.117",      0x004000, 0x1be35a97, BRF_GRA },		  //  10 Sprites
	{ "epr-6737.04",       0x004000, 0x6b53aa7a, BRF_GRA },		  //  11 Sprites
	{ "epr-6736.110",      0x004000, 0x565c25d0, BRF_GRA },		  //  12 Sprites
	{ "epr-6738.05",       0x004000, 0xe116285f, BRF_GRA },		  //  13 Sprites

	{ "pr-5317.76",        0x000100, 0x648350b8, BRF_OPT },		  //  14 Timing PROM
};

STD_ROM_PICK(Teddybb)
STD_ROM_FN(Teddybb)

static struct BurnRomInfo TeddybboRomDesc[] = {
	{ "epr-6739.116",      0x004000, 0x81a37e69, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "epr-6740.109",      0x004000, 0x715388a9, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "epr-6741.96",       0x004000, 0xe5a74f5f, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code

	{ "epr-6748.120",      0x002000, 0x9325a1cf, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program Code

	{ "epr-6747.62",       0x002000, 0xa0e5aca7, BRF_GRA },		  //  4 Tiles
	{ "epr-6746.61",       0x002000, 0xcdb77e51, BRF_GRA },		  //  5 Tiles
	{ "epr-6745.64",       0x002000, 0x0cab75c3, BRF_GRA },		  //  6 Tiles
	{ "epr-6744.63",       0x002000, 0x0ef8d2cd, BRF_GRA },		  //  7 Tiles
	{ "epr-6743.66",       0x002000, 0xc33062b5, BRF_GRA },		  //  8 Tiles
	{ "epr-6742.65",       0x002000, 0xc457e8c5, BRF_GRA },		  //  9 Tiles

	{ "epr-6735.117",      0x004000, 0x1be35a97, BRF_GRA },		  //  10 Sprites
	{ "epr-6737.04",       0x004000, 0x6b53aa7a, BRF_GRA },		  //  11 Sprites
	{ "epr-6736.110",      0x004000, 0x565c25d0, BRF_GRA },		  //  12 Sprites
	{ "epr-6738.05",       0x004000, 0xe116285f, BRF_GRA },		  //  13 Sprites

	{ "pr-5317.76",        0x000100, 0x648350b8, BRF_OPT },		  //  14 Timing PROM
};

STD_ROM_PICK(Teddybbo)
STD_ROM_FN(Teddybbo)

static struct BurnRomInfo TeddybboblRomDesc[] = {
	{ "1.f2",        	   0x004000, 0x81a37e69, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "2.j2",	           0x004000, 0x715388a9, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "3.k2",        	   0x004000, 0xe5a74f5f, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code

//	No sound rom present on the PCB - Instead use epr-6748.120 from Teddybbo set //
	{ "6.e10",             0x002000, 0x9325a1cf, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program Code

	{ "11.r7",       	   0x004000, 0x55d7aaf7, BRF_GRA },		  //  4 Tiles
	{ "10.r8",       	   0x004000, 0x52a5083d, BRF_GRA },		  //  5 Tiles
	{ "9.r10",       	   0x004000, 0x8076d3a3, BRF_GRA },		  //  6 Tiles

	{ "4.f3",	           0x004000, 0x1be35a97, BRF_GRA },		  //  7 Sprites
	{ "6.k3",        	   0x004000, 0x6b53aa7a, BRF_GRA },		  //  8 Sprites
	{ "5.h3",	           0x004000, 0x565c25d0, BRF_GRA },		  //  9 Sprites
	{ "7.m3",        	   0x004000, 0xe116285f, BRF_GRA },		  // 10 Sprites

	{ "pr-5317.76",        0x000100, 0x648350b8, BRF_OPT },		  //  14 Timing PROM

	{ "74s287.bin",        0x000100, 0xde9af32c, BRF_OPT },		  //  15
};

STD_ROM_PICK(Teddybbobl)
STD_ROM_FN(Teddybbobl)

static struct BurnRomInfo tokisensRomDesc[] = {
	{ "epr-10961.ic90",      0x8000, 0x5c71c203, BRF_ESS | BRF_PRG }, //  0 Z80 #1 Program Code
	{ "epr-10962.ic91",      0x8000, 0xdb9080e3, BRF_ESS | BRF_PRG }, //  1
	{ "epr-10963.ic92",      0x8000, 0xd17ad93f, BRF_ESS | BRF_PRG }, //  2

	{ "epr-10967.ic126",     0x8000, 0x97966bf2, BRF_ESS | BRF_PRG }, //  3 Z80 #2 Program Code

	{ "epr-10964.ic4",       0x8000, 0x25af5c93, BRF_GRA }, //  4 Tiles
	{ "epr-10965.ic5",       0x8000, 0xcc8eb99a, BRF_GRA }, //  5
	{ "epr-10966.ic6",       0x8000, 0x7ecf2459, BRF_GRA }, //  6

	{ "epr-10958.ic87",      0x8000, 0xbb62dbc8, BRF_GRA }, //  7 Sprites
	{ "epr-10957.ic86",      0x8000, 0x4ec56860, BRF_GRA }, //  8
	{ "epr-10960.ic89",      0x8000, 0x880e0d44, BRF_GRA }, //  9
	{ "epr-10959.ic88",      0x8000, 0x4deda48f, BRF_GRA }, // 10

	{ "pr10956.ic20",        0x0100, 0xfd1bba8a, BRF_GRA }, // 11 Palette
	{ "pr10955.ic14",        0x0100, 0x72b35df7, BRF_GRA }, // 12
	{ "pr10954.ic8",         0x0100, 0xb7984867, BRF_GRA }, // 13

	{ "pr-5317.ic28",        0x0100, 0x648350b8, BRF_GRA }, // 14 Timing PROM

	{ "317-0040.key",        0x2000, 0xe2b67fd6, BRF_ESS | BRF_PRG }, // 15 MC8123 Key
};

STD_ROM_PICK(tokisens)
STD_ROM_FN(tokisens)

static struct BurnRomInfo tokisensaRomDesc[] = {
	{ "ic90",                0x8000, 0x1466b61d, BRF_ESS | BRF_PRG }, //  0 Z80 #1 Program Code
	{ "ic91",                0x8000, 0xa8479f91, BRF_ESS | BRF_PRG }, //  1
	{ "ic92",                0x8000, 0xb7193b39, BRF_ESS | BRF_PRG }, //  2

	{ "epr-10967.ic126",     0x8000, 0x97966bf2, BRF_ESS | BRF_PRG }, //  3 Z80 #2 Program Code

	{ "ic4",                 0x8000, 0x9013b85c, BRF_GRA }, //  4 Tiles
	{ "ic5",                 0x8000, 0xe4755cc6, BRF_GRA }, //  5
	{ "ic6",                 0x8000, 0x5bbfbdcc, BRF_GRA }, //  6

	{ "ic87",                0x8000, 0xfc2bcbd7, BRF_GRA }, //  7 Sprites
	{ "epr-10957.ic86",      0x8000, 0x4ec56860, BRF_GRA }, //  8
	{ "epr-10960.ic89",      0x8000, 0x880e0d44, BRF_GRA }, //  9
	{ "epr-10959.ic88",      0x8000, 0x4deda48f, BRF_GRA }, // 10

	{ "ic20",                0x0100, 0x8eee0f72, BRF_GRA }, // 11 Palette
	{ "ic14",                0x0100, 0x3e7babd7, BRF_GRA }, // 12
	{ "ic8",                 0x0100, 0x371c44a6, BRF_GRA }, // 13

	{ "pr-5317.ic28",        0x0100, 0x648350b8, BRF_GRA }, // 14 Timing PROM
};

STD_ROM_PICK(tokisensa)
STD_ROM_FN(tokisensa)

static struct BurnRomInfo ufosensiRomDesc[] = {
	{ "epr-11661.90",	0x8000, 0xf3e394e2, BRF_ESS | BRF_PRG }, //  0 Z80 #1 Program Code
	{ "epr-11662.91",	0x8000, 0x0c2e4120, BRF_ESS | BRF_PRG }, //  1
	{ "epr-11663.92",	0x8000, 0x4515ebae, BRF_ESS | BRF_PRG }, //  2

	{ "epr-11667.126",	0x8000, 0x110baba9, BRF_ESS | BRF_PRG }, //  3 Z80 #2 Program Code

	{ "epr-11664.4",	0x8000, 0x1b1bc3d5, BRF_GRA }, //  4 Tiles
	{ "epr-11665.5",	0x8000, 0x3659174a, BRF_GRA }, //  5
	{ "epr-11666.6",	0x8000, 0x99dcc793, BRF_GRA }, //  6

	{ "epr-11658.87",	0x8000, 0x3b5a20f7, BRF_GRA }, //  7 Sprites
	{ "epr-11657.86",	0x8000, 0x010f81a9, BRF_GRA }, //  8
	{ "epr-11660.89",	0x8000, 0xe1e2e7c5, BRF_GRA }, //  9
	{ "epr-11659.88",	0x8000, 0x286c7286, BRF_GRA }, // 10

	{ "pr11656.20",		0x0100, 0x640740eb, BRF_GRA }, // 11 Palette
	{ "pr11655.14",		0x0100, 0xa0c3fa77, BRF_GRA }, // 12
	{ "pr11654.8",		0x0100, 0xba624305, BRF_GRA }, // 13

	{ "pr5317.28",		0x0100, 0x648350b8, BRF_GRA }, // 14 Timing PROM

	{ "317-0064.key",	0x2000, 0xda326f36, BRF_ESS }, // 15 Encryption Key
};

STD_ROM_PICK(ufosensi)
STD_ROM_FN(ufosensi)

static struct BurnRomInfo UpndownRomDesc[] = {
	{ "epr5516a.129",      0x002000, 0x038c82da, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "epr5517a.130",      0x002000, 0x6930e1de, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "epr-5518.131",      0x002000, 0x2a370c99, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code
	{ "epr-5519.132",      0x002000, 0x9d664a58, BRF_ESS | BRF_PRG }, //  3	Z80 #1 Program Code
	{ "epr-5520.133",      0x002000, 0x208dfbdf, BRF_ESS | BRF_PRG }, //  4	Z80 #1 Program Code
	{ "epr-5521.134",      0x002000, 0xe7b8d87a, BRF_ESS | BRF_PRG }, //  5	Z80 #1 Program Code

	{ "epr-5535.3",        0x002000, 0xcf4e4c45, BRF_ESS | BRF_PRG }, //  6	Z80 #2 Program Code

	{ "epr-5527.82",       0x002000, 0xb2d616f1, BRF_GRA },		  //  7 Tiles
	{ "epr-5526.65",       0x002000, 0x8a8b33c2, BRF_GRA },		  //  8 Tiles
	{ "epr-5525.81",       0x002000, 0xe749c5ef, BRF_GRA },		  //  9 Tiles
	{ "epr-5524.64",       0x002000, 0x8b886952, BRF_GRA },		  //  10 Tiles
	{ "epr-5523.80",       0x002000, 0xdede35d9, BRF_GRA },		  //  11 Tiles
	{ "epr-5522.63",       0x002000, 0x5e6d9dff, BRF_GRA },		  //  12 Tiles

	{ "epr-5514.86",       0x004000, 0xfcc0a88b, BRF_GRA },		  //  13 Sprites
	{ "epr-5515.93",       0x004000, 0x60908838, BRF_GRA },		  //  14 Sprites

	{ "pr-5317.106",       0x000100, 0x648350b8, BRF_OPT },		  //  15 Timing PROM
};

STD_ROM_PICK(Upndown)
STD_ROM_FN(Upndown)

static struct BurnRomInfo UpndownuRomDesc[] = {
	{ "epr-5679.129",      0x002000, 0xc4f2f9c2, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "epr-5680.130",      0x002000, 0x837f021c, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "epr-5681.131",      0x002000, 0xe1c7ff7e, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code
	{ "epr-5682.132",      0x002000, 0x4a5edc1e, BRF_ESS | BRF_PRG }, //  3	Z80 #1 Program Code
	{ "epr-5520.133",      0x002000, 0x208dfbdf, BRF_ESS | BRF_PRG }, //  4	Z80 #1 Program Code
	{ "epr-5684.133",      0x002000, 0x32fa95da, BRF_ESS | BRF_PRG }, //  5	Z80 #1 Program Code

	{ "epr-5528.3",        0x002000, 0x00cd44ab, BRF_ESS | BRF_PRG }, //  6	Z80 #2 Program Code

	{ "epr-5527.82",       0x002000, 0xb2d616f1, BRF_GRA },		  //  7 Tiles
	{ "epr-5526.65",       0x002000, 0x8a8b33c2, BRF_GRA },		  //  8 Tiles
	{ "epr-5525.81",       0x002000, 0xe749c5ef, BRF_GRA },		  //  9 Tiles
	{ "epr-5524.64",       0x002000, 0x8b886952, BRF_GRA },		  //  10 Tiles
	{ "epr-5523.80",       0x002000, 0xdede35d9, BRF_GRA },		  //  11 Tiles
	{ "epr-5522.63",       0x002000, 0x5e6d9dff, BRF_GRA },		  //  12 Tiles

	{ "epr-5514.86",       0x004000, 0xfcc0a88b, BRF_GRA },		  //  13 Sprites
	{ "epr-5515.93",       0x004000, 0x60908838, BRF_GRA },		  //  14 Sprites

	{ "pr-5317.106",       0x000100, 0x648350b8, BRF_OPT },		  //  15 Timing PROM
};

STD_ROM_PICK(Upndownu)
STD_ROM_FN(Upndownu)

static struct BurnRomInfo WboyRomDesc[] = {
	{ "epr-7489.116",      0x004000, 0x130f4b70, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "epr-7490.109",      0x004000, 0x9e656733, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "epr-7491.96",       0x004000, 0x1f7d0efe, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code

	{ "epr-7498.120",      0x002000, 0x78ae1e7b, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program Code

	{ "epr-7497.62",       0x002000, 0x08d609ca, BRF_GRA },		  //  4 Tiles
	{ "epr-7496.61",       0x002000, 0x6f61fdf1, BRF_GRA },		  //  5 Tiles
	{ "epr-7495.64",       0x002000, 0x6a0d2c2d, BRF_GRA },		  //  6 Tiles
	{ "epr-7494.63",       0x002000, 0xa8e281c7, BRF_GRA },		  //  7 Tiles
	{ "epr-7493.66",       0x002000, 0x89305df4, BRF_GRA },		  //  8 Tiles
	{ "epr-7492.65",       0x002000, 0x60f806b1, BRF_GRA },		  //  9 Tiles

	{ "epr-7485.117",      0x004000, 0xc2891722, BRF_GRA },		  //  10 Sprites
	{ "epr-7487.04",       0x004000, 0x2d3a421b, BRF_GRA },		  //  11 Sprites
	{ "epr-7486.110",      0x004000, 0x8d622c50, BRF_GRA },		  //  12 Sprites
	{ "epr-7488.05",       0x004000, 0x007c2f1b, BRF_GRA },		  //  13 Sprites

	{ "pr-5317.76",        0x000100, 0x648350b8, BRF_OPT },		  //  14 Timing PROM
};

STD_ROM_PICK(Wboy)
STD_ROM_FN(Wboy)

static struct BurnRomInfo WboyoRomDesc[] = {
	{ "epr-7532.116",      0x004000, 0x51d27534, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "epr-7533.109",      0x004000, 0xe29d1cd1, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "epr-7534.96",       0x004000, 0x1f7d0efe, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code

	{ "epr-7498.120",      0x002000, 0x78ae1e7b, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program Code

	{ "epr-7497.62",       0x002000, 0x08d609ca, BRF_GRA },		  //  4 Tiles
	{ "epr-7496.61",       0x002000, 0x6f61fdf1, BRF_GRA },		  //  5 Tiles
	{ "epr-7495.64",       0x002000, 0x6a0d2c2d, BRF_GRA },		  //  6 Tiles
	{ "epr-7494.63",       0x002000, 0xa8e281c7, BRF_GRA },		  //  7 Tiles
	{ "epr-7493.66",       0x002000, 0x89305df4, BRF_GRA },		  //  8 Tiles
	{ "epr-7492.65",       0x002000, 0x60f806b1, BRF_GRA },		  //  9 Tiles

	{ "epr-7485.117",      0x004000, 0xc2891722, BRF_GRA },		  //  10 Sprites
	{ "epr-7487.04",       0x004000, 0x2d3a421b, BRF_GRA },		  //  11 Sprites
	{ "epr-7486.110",      0x004000, 0x8d622c50, BRF_GRA },		  //  12 Sprites
	{ "epr-7488.05",       0x004000, 0x007c2f1b, BRF_GRA },		  //  13 Sprites

	{ "pr-5317.76",        0x000100, 0x648350b8, BRF_OPT },		  //  14 Timing PROM
};

STD_ROM_PICK(Wboyo)
STD_ROM_FN(Wboyo)

static struct BurnRomInfo Wboy2RomDesc[] = {
	{ "epr-7587.129",      0x002000, 0x1bbb7354, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "epr-7588.130",      0x002000, 0x21007413, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "epr-7589.131",      0x002000, 0x44b30433, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code
	{ "epr-7590.132",      0x002000, 0xbb525a0b, BRF_ESS | BRF_PRG }, //  3	Z80 #1 Program Code
	{ "epr-7591.133",      0x002000, 0x8379aa23, BRF_ESS | BRF_PRG }, //  4	Z80 #1 Program Code
	{ "epr-7592.134",      0x002000, 0xc767a5d7, BRF_ESS | BRF_PRG }, //  5	Z80 #1 Program Code

	{ "epr-7498.120",      0x002000, 0x78ae1e7b, BRF_ESS | BRF_PRG }, //  6	Z80 #2 Program Code

	{ "epr-7497.62",       0x002000, 0x08d609ca, BRF_GRA },		  //  7 Tiles
	{ "epr-7496.61",       0x002000, 0x6f61fdf1, BRF_GRA },		  //  8 Tiles
	{ "epr-7495.64",       0x002000, 0x6a0d2c2d, BRF_GRA },		  //  9 Tiles
	{ "epr-7494.63",       0x002000, 0xa8e281c7, BRF_GRA },		  //  10 Tiles
	{ "epr-7493.66",       0x002000, 0x89305df4, BRF_GRA },		  //  11 Tiles
	{ "epr-7492.65",       0x002000, 0x60f806b1, BRF_GRA },		  //  12 Tiles

	{ "epr-7485.117",      0x004000, 0xc2891722, BRF_GRA },		  //  13 Sprites
	{ "epr-7487.04",       0x004000, 0x2d3a421b, BRF_GRA },		  //  14 Sprites
	{ "epr-7486.110",      0x004000, 0x8d622c50, BRF_GRA },		  //  15 Sprites
	{ "epr-7488.05",       0x004000, 0x007c2f1b, BRF_GRA },		  //  16 Sprites

	{ "pr-5317.76",        0x000100, 0x648350b8, BRF_OPT },		  //  17 Timing PROM
};

STD_ROM_PICK(Wboy2)
STD_ROM_FN(Wboy2)

static struct BurnRomInfo Wboy2uRomDesc[] = {
	{ "ic129_02.bin",      0x002000, 0x32c4b709, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "ic130_03.bin",      0x002000, 0x56463ede, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "ic131_04.bin",      0x002000, 0x775ed392, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code
	{ "ic132_05.bin",      0x002000, 0x7b922708, BRF_ESS | BRF_PRG }, //  3	Z80 #1 Program Code
	{ "epr-7591.133",      0x002000, 0x8379aa23, BRF_ESS | BRF_PRG }, //  4	Z80 #1 Program Code
	{ "epr-7592.134",      0x002000, 0xc767a5d7, BRF_ESS | BRF_PRG }, //  5	Z80 #1 Program Code

	{ "epr7498a.3",        0x002000, 0xc198205c, BRF_ESS | BRF_PRG }, //  6	Z80 #2 Program Code

	{ "epr-7497.62",       0x002000, 0x08d609ca, BRF_GRA },		  //  7 Tiles
	{ "epr-7496.61",       0x002000, 0x6f61fdf1, BRF_GRA },		  //  8 Tiles
	{ "epr-7495.64",       0x002000, 0x6a0d2c2d, BRF_GRA },		  //  9 Tiles
	{ "epr-7494.63",       0x002000, 0xa8e281c7, BRF_GRA },		  //  10 Tiles
	{ "epr-7493.66",       0x002000, 0x89305df4, BRF_GRA },		  //  11 Tiles
	{ "epr-7492.65",       0x002000, 0x60f806b1, BRF_GRA },		  //  12 Tiles

	{ "epr-7485.117",      0x004000, 0xc2891722, BRF_GRA },		  //  13 Sprites
	{ "epr-7487.04",       0x004000, 0x2d3a421b, BRF_GRA },		  //  14 Sprites
	{ "epr-7486.110",      0x004000, 0x8d622c50, BRF_GRA },		  //  15 Sprites
	{ "epr-7488.05",       0x004000, 0x007c2f1b, BRF_GRA },		  //  16 Sprites

	{ "pr-5317.76",        0x000100, 0x648350b8, BRF_OPT },		  //  17 Timing PROM
};

STD_ROM_PICK(Wboy2u)
STD_ROM_FN(Wboy2u)

static struct BurnRomInfo Wboy3RomDesc[] = {
	{ "wb_1",              0x004000, 0xbd6fef49, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "wb_2",              0x004000, 0x4081b624, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "wb_3",              0x004000, 0xc48a0e36, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code

	{ "epr-7498.120",      0x002000, 0x78ae1e7b, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program Code

	{ "epr-7497.62",       0x002000, 0x08d609ca, BRF_GRA },		  //  4 Tiles
	{ "epr-7496.61",       0x002000, 0x6f61fdf1, BRF_GRA },		  //  5 Tiles
	{ "epr-7495.64",       0x002000, 0x6a0d2c2d, BRF_GRA },		  //  6 Tiles
	{ "epr-7494.63",       0x002000, 0xa8e281c7, BRF_GRA },		  //  7 Tiles
	{ "epr-7493.66",       0x002000, 0x89305df4, BRF_GRA },		  //  8 Tiles
	{ "epr-7492.65",       0x002000, 0x60f806b1, BRF_GRA },		  //  9 Tiles

	{ "epr-7485.117",      0x004000, 0xc2891722, BRF_GRA },		  //  10 Sprites
	{ "epr-7487.04",       0x004000, 0x2d3a421b, BRF_GRA },		  //  11 Sprites
	{ "epr-7486.110",      0x004000, 0x8d622c50, BRF_GRA },		  //  12 Sprites
	{ "epr-7488.05",       0x004000, 0x007c2f1b, BRF_GRA },		  //  13 Sprites

	{ "pr-5317.76",        0x000100, 0x648350b8, BRF_OPT },		  //  14 Timing PROM
};

STD_ROM_PICK(Wboy3)
STD_ROM_FN(Wboy3)

static struct BurnRomInfo Wboy4RomDesc[] = {
	{ "epr-7622.ic1",      0x008000, 0x48b2c006, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "epr-7621.ic2",      0x008000, 0x466cae31, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code

	{ "epr-7583.126",      0x008000, 0x99334b3c, BRF_ESS | BRF_PRG }, //  2	Z80 #2 Program Code

	{ "epr-7610.ic62",     0x004000, 0x1685d26a, BRF_GRA },		  //  3 Tiles
	{ "epr-7609.ic64",     0x004000, 0x87ecba53, BRF_GRA },		  //  4 Tiles
	{ "epr-7608.ic66",     0x004000, 0xe812b3ec, BRF_GRA },		  //  5 Tiles

	{ "epr-7578.87",       0x008000, 0x6ff1637f, BRF_GRA },		  //  6 Sprites
	{ "epr-7577.86",       0x008000, 0x58b3705e, BRF_GRA },		  //  7 Sprites

	{ "pr-5317.76",        0x000100, 0x648350b8, BRF_OPT },		  //  8 Timing PROM
};

STD_ROM_PICK(Wboy4)
STD_ROM_FN(Wboy4)

static struct BurnRomInfo Wboy5RomDesc[] = {
	{ "wb1.ic116",         0x004000, 0x6c67407c, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "wb_2",              0x004000, 0x4081b624, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "wb_3",              0x004000, 0xc48a0e36, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code

	{ "epr-7498.120",      0x002000, 0x78ae1e7b, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program Code

	{ "epr-7497.62",       0x002000, 0x08d609ca, BRF_GRA },		  //  4 Tiles
	{ "epr-7496.61",       0x002000, 0x6f61fdf1, BRF_GRA },		  //  5 Tiles
	{ "epr-7495.64",       0x002000, 0x6a0d2c2d, BRF_GRA },		  //  6 Tiles
	{ "epr-7494.63",       0x002000, 0xa8e281c7, BRF_GRA },		  //  7 Tiles
	{ "epr-7493.66",       0x002000, 0x89305df4, BRF_GRA },		  //  8 Tiles
	{ "epr-7492.65",       0x002000, 0x60f806b1, BRF_GRA },		  //  9 Tiles

	{ "epr-7485.117",      0x004000, 0xc2891722, BRF_GRA },		  //  10 Sprites
	{ "epr-7487.04",       0x004000, 0x2d3a421b, BRF_GRA },		  //  11 Sprites
	{ "epr-7486.110",      0x004000, 0x8d622c50, BRF_GRA },		  //  12 Sprites
	{ "epr-7488.05",       0x004000, 0x007c2f1b, BRF_GRA },		  //  13 Sprites

	{ "pr-5317.76",        0x000100, 0x648350b8, BRF_OPT },		  //  14 Timing PROM
};

STD_ROM_PICK(Wboy5)
STD_ROM_FN(Wboy5)

static struct BurnRomInfo WboyuRomDesc[] = {
	{ "ic116_89.bin",      0x004000, 0x73d8cef0, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "ic109_90.bin",      0x004000, 0x29546828, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "ic096_91.bin",      0x004000, 0xc7145c2a, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code

	{ "epr-7498.120",      0x002000, 0x78ae1e7b, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program Code

	{ "epr-7497.62",       0x002000, 0x08d609ca, BRF_GRA },		  //  4 Tiles
	{ "epr-7496.61",       0x002000, 0x6f61fdf1, BRF_GRA },		  //  5 Tiles
	{ "epr-7495.64",       0x002000, 0x6a0d2c2d, BRF_GRA },		  //  6 Tiles
	{ "epr-7494.63",       0x002000, 0xa8e281c7, BRF_GRA },		  //  7 Tiles
	{ "epr-7493.66",       0x002000, 0x89305df4, BRF_GRA },		  //  8 Tiles
	{ "epr-7492.65",       0x002000, 0x60f806b1, BRF_GRA },		  //  9 Tiles

	{ "ic117_85.bin",      0x004000, 0x1ee96ae8, BRF_GRA },		  //  10 Sprites
	{ "ic004_87.bin",      0x004000, 0x119735bb, BRF_GRA },		  //  11 Sprites
	{ "ic110_86.bin",      0x004000, 0x26d0fac4, BRF_GRA },		  //  12 Sprites
	{ "ic005_88.bin",      0x004000, 0x2602e519, BRF_GRA },		  //  13 Sprites

	{ "pr-5317.76",        0x000100, 0x648350b8, BRF_OPT },		  //  14 Timing PROM
};

STD_ROM_PICK(Wboyu)
STD_ROM_FN(Wboyu)

static struct BurnRomInfo WboybltRomDesc[] = {
	{ "1.bin",         	   0x004000, 0x67d5af52, BRF_ESS | BRF_PRG }, 	 //  0	Z80 #1 Program Code
	{ "2.bin",             0x004000, 0xe0d36862, BRF_ESS | BRF_PRG }, 	 //  1	Z80 #1 Program Code
	{ "3.bin",             0x004000, 0x8dba0aad, BRF_ESS | BRF_PRG }, 	 //  2	Z80 #1 Program Code

	{ "14.bin",      	   0x002000, 0x78ae1e7b, BRF_ESS | BRF_PRG }, 	 //  3	Z80 #2 Program Code

	{ "9.bin",       	   0x002000, 0x08d609ca, BRF_GRA },		  		 //  4 Tiles
	{ "8.bin",       	   0x002000, 0x6f61fdf1, BRF_GRA },		  		 //  5 Tiles
	{ "11.bin",       	   0x002000, 0x6a0d2c2d, BRF_GRA },		  		 //  6 Tiles
	{ "10.bin",       	   0x002000, 0xa8e281c7, BRF_GRA },		  		 //  7 Tiles
	{ "13.bin",       	   0x002000, 0x89305df4, BRF_GRA },		  		 //  8 Tiles
	{ "12.bin",       	   0x002000, 0x60f806b1, BRF_GRA },		  		 //  9 Tiles

	{ "4.bin",      	   0x004000, 0xc2891722, BRF_GRA },		  		 //  10 Sprites
	{ "6.bin",       	   0x004000, 0x2d3a421b, BRF_GRA },		  		 //  11 Sprites
	{ "5.bin",      	   0x004000, 0x8d622c50, BRF_GRA },		  		 //  12 Sprites
	{ "7.bin",       	   0x004000, 0x007c2f1b, BRF_GRA },		  		 //  13 Sprites

	{ "pr-5317.76",        0x000100, 0x648350b8, BRF_OPT },		  		 //  14 Timing PROM
	
	{ "tbp24s10n",         0x000100, 0x00000000, BRF_OPT | BRF_NODUMP }, //  15 decryption_prom
	
	{ "pal20l10cns",       0x000100, 0x00000000, BRF_OPT | BRF_NODUMP }, //  16 decryption_pal
};

STD_ROM_PICK(Wboyblt)
STD_ROM_FN(Wboyblt)

static struct BurnRomInfo WbdeluxeRomDesc[] = {
	{ "wbd1.bin",          0x002000, 0xa1bedbd7, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "ic130_03.bin",      0x002000, 0x56463ede, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "wbd3.bin",          0x002000, 0x6fcdbd4c, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code
	{ "ic132_05.bin",      0x002000, 0x7b922708, BRF_ESS | BRF_PRG }, //  3	Z80 #1 Program Code
	{ "wbd5.bin",          0x002000, 0xf6b02902, BRF_ESS | BRF_PRG }, //  4	Z80 #1 Program Code
	{ "wbd6.bin",          0x002000, 0x43df21fe, BRF_ESS | BRF_PRG }, //  5	Z80 #1 Program Code

	{ "epr7498a.3",        0x002000, 0xc198205c, BRF_ESS | BRF_PRG }, //  6	Z80 #2 Program Code

	{ "epr-7497.62",       0x002000, 0x08d609ca, BRF_GRA },		  //  7 Tiles
	{ "epr-7496.61",       0x002000, 0x6f61fdf1, BRF_GRA },		  //  8 Tiles
	{ "epr-7495.64",       0x002000, 0x6a0d2c2d, BRF_GRA },		  //  9 Tiles
	{ "epr-7494.63",       0x002000, 0xa8e281c7, BRF_GRA },		  //  10 Tiles
	{ "epr-7493.66",       0x002000, 0x89305df4, BRF_GRA },		  //  11 Tiles
	{ "epr-7492.65",       0x002000, 0x60f806b1, BRF_GRA },		  //  12 Tiles

	{ "epr-7485.117",      0x004000, 0xc2891722, BRF_GRA },		  //  13 Sprites
	{ "epr-7487.04",       0x004000, 0x2d3a421b, BRF_GRA },		  //  14 Sprites
	{ "epr-7486.110",      0x004000, 0x8d622c50, BRF_GRA },		  //  15 Sprites
	{ "epr-7488.05",       0x004000, 0x007c2f1b, BRF_GRA },		  //  16 Sprites

	{ "pr-5317.106",       0x000100, 0x648350b8, BRF_OPT },		  //  17 Timing PROM
};

STD_ROM_PICK(Wbdeluxe)
STD_ROM_FN(Wbdeluxe)

static struct BurnRomInfo WmatchRomDesc[] = {
	{ "wm.129",            0x002000, 0xb6db4442, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "wm.130",            0x002000, 0x59a0a7a0, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "wm.131",            0x002000, 0x4cb3856a, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code
	{ "wm.132",            0x002000, 0xe2e44b29, BRF_ESS | BRF_PRG }, //  3	Z80 #1 Program Code
	{ "wm.133",            0x002000, 0x43a36445, BRF_ESS | BRF_PRG }, //  4	Z80 #1 Program Code
	{ "wm.134",            0x002000, 0x5624794c, BRF_ESS | BRF_PRG }, //  5	Z80 #1 Program Code

	{ "wm.3",              0x002000, 0x50d2afb7, BRF_ESS | BRF_PRG }, //  6	Z80 #2 Program Code

	{ "wm.82",             0x002000, 0x540f0bf3, BRF_GRA },		  //  7 Tiles
	{ "wm.65",             0x002000, 0x92c1e39e, BRF_GRA },		  //  8 Tiles
	{ "wm.81",             0x002000, 0x6a01ff2a, BRF_GRA },		  //  9 Tiles
	{ "wm.64",             0x002000, 0xaae6449b, BRF_GRA },		  //  10 Tiles
	{ "wm.80",             0x002000, 0xfc3f0bd4, BRF_GRA },		  //  11 Tiles
	{ "wm.63",             0x002000, 0xc2ce9b93, BRF_GRA },		  //  12 Tiles

	{ "wm.86",             0x004000, 0x238ae0e5, BRF_GRA },		  //  13 Sprites
	{ "wm.93",             0x004000, 0xa2f19170, BRF_GRA },		  //  14 Sprites

	{ "pr-5317.106",       0x000100, 0x648350b8, BRF_OPT },		  //  15 Timing PROM
};

STD_ROM_PICK(Wmatch)
STD_ROM_FN(Wmatch)

// Shooting Master (8751 315-5159a)

static struct BurnRomInfo shtngmstRomDesc[] = {
	{ "epr-7100.ic18",	0x8000, 0x268ecb1d, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "epr-7101.ic91",	0x8000, 0xebf5ff72, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-7102.ic92",	0x8000, 0xc890a4ad, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "epr-7043.ic126",	0x8000, 0x99a368ab, 2 | BRF_PRG | BRF_ESS }, //  3 soundcpu

	{ "315-5159a.ic74",	0x1000, 0x1f774912, 3 | BRF_PRG | BRF_ESS }, //  4 mcu

	{ "epr-7040.ic4",	0x8000, 0xf30769fa, 4 | BRF_GRA },           //  5 tiles
	{ "epr-7041.ic5",	0x8000, 0xf3e273f9, 4 | BRF_GRA },           //  6
	{ "epr-7042.ic6",	0x8000, 0x6841c917, 4 | BRF_GRA },           //  7

	{ "epr-7110.ic26",	0x8000, 0x5d1a5048, 5 | BRF_GRA },           //  8 sprites
	{ "epr-7106.ic22",	0x8000, 0xae7ab7a2, 5 | BRF_GRA },           //  9
	{ "epr-7108.ic24",	0x8000, 0x816180ac, 5 | BRF_GRA },           // 10
	{ "epr-7104.ic20",	0x8000, 0x84a679c5, 5 | BRF_GRA },           // 11
	{ "epr-7109.ic25",	0x8000, 0x097f7481, 5 | BRF_GRA },           // 12
	{ "epr-7105.ic21",	0x8000, 0x13111729, 5 | BRF_GRA },           // 13
	{ "epr-7107.ic23",	0x8000, 0x8f50ea24, 5 | BRF_GRA },           // 14

	{ "epr-7113.ic20",	0x0100, 0x5c0e1360, 6 | BRF_GRA },           // 15 color_proms
	{ "epr-7112.ic14",	0x0100, 0x46fbd351, 6 | BRF_GRA },           // 16
	{ "epr-7111.ic8",	0x0100, 0x8123b6b9, 6 | BRF_GRA },           // 17

	{ "pr5317.ic37",	0x0100, 0x648350b8, 7 | BRF_GRA },           // 18 lookup_proms

	{ "315-5137.ic10",	0x0104, 0x6ffd9e6f, 8 | BRF_OPT },           // 19 plds
	{ "315-5138.ic11",	0x0104, 0xdd223015, 8 | BRF_OPT },           // 20
	{ "315-5139.ic50",	0x00e7, 0x943d91b0, 8 | BRF_OPT },           // 21
};

STD_ROM_PICK(shtngmst)
STD_ROM_FN(shtngmst)

static struct BurnRomInfo ChopliftRomDesc[] = {
	{ "epr-7124.ic90",       0x008000, 0x678d5c41, BRF_ESS | BRF_PRG }, 	//  0	Z80 #1 Program Code
	{ "epr-7125.ic91",       0x008000, 0xf5283498, BRF_ESS | BRF_PRG }, 	//  1	Z80 #1 Program Code
	{ "epr-7126.ic92",       0x008000, 0xdbd192ab, BRF_ESS | BRF_PRG }, 	//  2	Z80 #1 Program Code

	{ "epr-7130.ic126",      0x008000, 0x346af118, BRF_ESS | BRF_PRG }, 	//  3	Z80 #2 Program Code

	{ "315-5151.ic74",       0x001000, 0x1377a6ef, BRF_ESS | BRF_PRG },		//  4 mcu

	{ "epr-7127.ic4",        0x008000, 0x1e708f6d, BRF_GRA },		  		//  5 Tiles
	{ "epr-7128.ic5",        0x008000, 0xb922e787, BRF_GRA },		  		//  6 Tiles
	{ "epr-7129.ic6",        0x008000, 0xbd3b6e6e, BRF_GRA },		  		//  7 Tiles

	{ "epr-7121.ic87",       0x008000, 0xf2b88f73, BRF_GRA },		 	 	//  8 Sprites
	{ "epr-7120.ic86",       0x008000, 0x517d7fd3, BRF_GRA },		  		//  9 Sprites
	{ "epr-7123.ic89",       0x008000, 0x8f16a303, BRF_GRA },		  		// 10 Sprites
	{ "epr-7122.ic88",       0x008000, 0x7c93f160, BRF_GRA },		  		// 11 Sprites

	{ "pr7119.ic20",         0x000100, 0xb2a8260f, BRF_GRA },		  		// 12 Red PROM
	{ "pr7118.ic14",         0x000100, 0x693e20c7, BRF_GRA },		  		// 13 Green PROM
	{ "pr7117.ic8",          0x000100, 0x4124307e, BRF_GRA },		  		// 14 Blue PROM

	{ "pr5317.ic28",         0x000100, 0x648350b8, BRF_OPT },				// 15 lookup_proms
	
	{ "315-5152.ic10",       0x000104, 0x2c9229b4, BRF_OPT },				// 16 plds
	{ "315-5138.ic11",       0x000104, 0xdd223015, BRF_OPT },				// 17
	{ "315-5139.ic50",       0x0000e7, 0x943d91b0, BRF_OPT },				// 18
	{ "315-5025.ic7",        0x000104, 0x00000000, BRF_OPT | BRF_NODUMP },	// 19
	{ "315-5025.ic13",       0x000104, 0x00000000, BRF_OPT | BRF_NODUMP },	// 20
	{ "315-5025.ic19",       0x000104, 0x00000000, BRF_OPT | BRF_NODUMP },	// 21
};

STD_ROM_PICK(Choplift)
STD_ROM_FN(Choplift)

static struct BurnRomInfo ChopliftuRomDesc[] = {
	{ "epr-7152.ic90",       0x008000, 0xfe49d83e, BRF_ESS | BRF_PRG }, 	//  0	Z80 #1 Program Code
	{ "epr-7153.ic91",       0x008000, 0x48697666, BRF_ESS | BRF_PRG }, 	//  1	Z80 #1 Program Code
	{ "epr-7154.ic92",       0x008000, 0x56d6222a, BRF_ESS | BRF_PRG }, 	//  2	Z80 #1 Program Code

    { "epr-7130.ic126",      0x008000, 0x346af118, BRF_ESS | BRF_PRG }, 	//  3	Z80 #2 Program Code

	{ "epr-7127.ic4",        0x008000, 0x1e708f6d, BRF_GRA },		  		//  4 Tiles
	{ "epr-7128.ic5",        0x008000, 0xb922e787, BRF_GRA },		  		//  5 Tiles
	{ "epr-7129.ic6",        0x008000, 0xbd3b6e6e, BRF_GRA },		  		//  6 Tiles

	{ "epr-7121.ic87",       0x008000, 0xf2b88f73, BRF_GRA },		  		//  4 Sprites
	{ "epr-7120.ic86",       0x008000, 0x517d7fd3, BRF_GRA },		  		//  5 Sprites
	{ "epr-7123.ic89",       0x008000, 0x8f16a303, BRF_GRA },		  		//  6 Sprites
	{ "epr-7122.ic88",       0x008000, 0x7c93f160, BRF_GRA },		  		//  7 Sprites

	{ "pr7119.ic20",         0x000100, 0xb2a8260f, BRF_OPT },		  		//  8 Red PROM
	{ "pr7118.ic14",         0x000100, 0x693e20c7, BRF_OPT },		  		//  9 Green PROM
	{ "pr7117.ic8",          0x000100, 0x4124307e, BRF_OPT },		  		//  10 Blue PROM

	{ "pr5317.ic28",         0x000100, 0x648350b8, BRF_OPT },				// 15 lookup_proms
	
	{ "315-5152.ic10",       0x000104, 0x2c9229b4, BRF_OPT },				// 16 plds
	{ "315-5138.ic11",       0x000104, 0xdd223015, BRF_OPT },				// 17
	{ "315-5139.ic50",       0x0000e7, 0x943d91b0, BRF_OPT },				// 18
	{ "315-5025.ic7",        0x000104, 0x00000000, BRF_OPT | BRF_NODUMP },	// 19
	{ "315-5025.ic13",       0x000104, 0x00000000, BRF_OPT | BRF_NODUMP },	// 20
	{ "315-5025.ic19",       0x000104, 0x00000000, BRF_OPT | BRF_NODUMP },	// 21
};

STD_ROM_PICK(Chopliftu)
STD_ROM_FN(Chopliftu)

static struct BurnRomInfo ChopliftblRomDesc[] = {
	{ "ep7124bl.90",         0x008000, 0x71a37932, BRF_ESS | BRF_PRG }, 	//  0	Z80 #1 Program Code
	{ "epr-7125.91",         0x008000, 0xf5283498, BRF_ESS | BRF_PRG }, 	//  1	Z80 #1 Program Code
	{ "epr-7126.92",         0x008000, 0xdbd192ab, BRF_ESS | BRF_PRG }, 	//  2	Z80 #1 Program Code

	{ "epr-7130.126",        0x008000, 0x346af118, BRF_ESS | BRF_PRG }, 	//  3	Z80 #2 Program Code

	{ "epr-7127.4",          0x008000, 0x1e708f6d, BRF_GRA },		  		//  4 Tiles
	{ "epr-7128.5",          0x008000, 0xb922e787, BRF_GRA },		  		//  5 Tiles
	{ "epr-7129.6",          0x008000, 0xbd3b6e6e, BRF_GRA },		  		//  6 Tiles

	{ "epr-7121.87",         0x008000, 0xf2b88f73, BRF_GRA },		  		//  4 Sprites
	{ "epr-7120.86",         0x008000, 0x517d7fd3, BRF_GRA },		  		//  5 Sprites
	{ "epr-7123.89",         0x008000, 0x8f16a303, BRF_GRA },		  		//  6 Sprites
	{ "epr-7122.88",         0x008000, 0x7c93f160, BRF_GRA },		  		//  7 Sprites

	{ "pr7119.20",           0x000100, 0xb2a8260f, BRF_OPT },		  		//  8 Red PROM
	{ "pr7118.14",           0x000100, 0x693e20c7, BRF_OPT },		  		//  9 Green PROM
	{ "pr7117.8",            0x000100, 0x4124307e, BRF_OPT },		  		//  10 Blue PROM

	{ "pr5317.28",           0x000100, 0x648350b8, BRF_OPT },				// 11 lookup_proms
	
	{ "pal16r4.bin",         0x000001, 0x00000000, BRF_OPT | BRF_NODUMP }, 	// 12 plds_main
	{ "pal16r4.bin",         0x000001, 0x00000000, BRF_OPT | BRF_NODUMP },	// 13
	{ "pal16l8.bin",         0x000001, 0x00000000, BRF_OPT | BRF_NODUMP },	// 14
	
	{ "pal16r4.bin",         0x000001, 0x00000000, BRF_OPT | BRF_NODUMP },	// 15 plds_600a
	{ "pal16l8.bin",         0x000001, 0x00000000, BRF_OPT | BRF_NODUMP },	// 16
	
	{ "pal20r4.bin",         0x000001, 0x00000000, BRF_OPT | BRF_NODUMP },	// 17 plds_600b
	{ "pal16l8.bin",         0x000001, 0x00000000, BRF_OPT | BRF_NODUMP },	// 18
	{ "pal16l8.bin",         0x000001, 0x00000000, BRF_OPT | BRF_NODUMP },	// 19
	{ "pal16l8.bin",         0x000001, 0x00000000, BRF_OPT | BRF_NODUMP },	// 20
	{ "pal16l8.bin",         0x000001, 0x00000000, BRF_OPT | BRF_NODUMP },	// 21
	
	{ "pal16r4a.ic9",        0x000104, 0xdd223015, BRF_OPT },				// 22 plds_unk
	{ "pal16r4a.ic10",       0x000104, 0x2c9229b4, BRF_OPT },				// 23
	{ "pal16r4a-chopbl1.bin",0x000104, 0xe1628a8e, BRF_OPT },				// 24
	{ "pal16l8a-chopbl2.bin",0x000104, 0xafa7425d, BRF_OPT },				// 25
};

STD_ROM_PICK(Chopliftbl)
STD_ROM_FN(Chopliftbl)

// "Wonder Boy - Monster Land (Japan New Ver., MC-8123, 317-0043)

static struct BurnRomInfo wbmlRomDesc[] = {
	{ "epr-11031a.90",	0x8000, 0xbd3349e5, BRF_ESS | BRF_PRG }, //  0 Z80 #1 Program Code
	{ "epr-11032.91",	0x8000, 0x9d03bdb2, BRF_ESS | BRF_PRG }, //  1
	{ "epr-11033.92",	0x8000, 0x7076905c, BRF_ESS | BRF_PRG }, //  2

	{ "epr-11037.126",	0x8000, 0x7a4ee585, BRF_ESS | BRF_PRG }, //  3 Z80 #2 Program Code

	{ "epr-11034.4",	0x8000, 0x37a2077d, BRF_GRA },           //  4 Tiles
	{ "epr-11035.5",	0x8000, 0xcdf2a21b, BRF_GRA },           //  5
	{ "epr-11036.6",	0x8000, 0x644687fa, BRF_GRA },           //  6

	{ "epr-11028.87",	0x8000, 0xaf0b3972, BRF_GRA },           //  7 Sprites
	{ "epr-11027.86",	0x8000, 0x277d8f1d, BRF_GRA },           //  8
	{ "epr-11030.89",	0x8000, 0xf05ffc76, BRF_GRA },           //  9
	{ "epr-11029.88",	0x8000, 0xcedc9c61, BRF_GRA },           // 10

	{ "pr11026.20",		0x0100, 0x27057298, BRF_GRA },           // 11 Red PROM
	{ "pr11025.14",		0x0100, 0x41e4d86b, BRF_GRA },           // 12 Blue
	{ "pr11024.8",		0x0100, 0x08d71954, BRF_GRA },           // 13 Green
	{ "pr5317.37",		0x0100, 0x648350b8, BRF_GRA },           // 14 Timing PROM

	{ "317-0043.key",	0x2000, 0xe354abfc, BRF_ESS },           // 15 Encryption Key
};

STD_ROM_PICK(wbml)
STD_ROM_FN(wbml)

// Wonder Boy - Monster Land (Japan bootleg)

static struct BurnRomInfo wbmljbRomDesc[] = {
	{ "wbml.01",		0x10000, 0x66482638, BRF_ESS | BRF_PRG }, //  0 Z80 #1 Program Code
	{ "m-6.bin",		0x10000, 0x8c08cd11, BRF_ESS | BRF_PRG }, //  1
	{ "m-7.bin",		0x10000, 0x11881703, BRF_ESS | BRF_PRG }, //  2

	{ "epr-11037.126",	0x08000, 0x7a4ee585, BRF_ESS | BRF_PRG }, //  3 Z80 #2 Program Code

	{ "epr-11034.4",	0x08000, 0x37a2077d, BRF_GRA },           //  4 Tiles
	{ "epr-11035.5",	0x08000, 0xcdf2a21b, BRF_GRA },           //  5
	{ "epr-11036.6",	0x08000, 0x644687fa, BRF_GRA },           //  6

	{ "epr-11028.87",	0x08000, 0xaf0b3972, BRF_GRA },           //  7 Sprites
	{ "epr-11027.86",	0x08000, 0x277d8f1d, BRF_GRA },           //  8
	{ "epr-11030.89",	0x08000, 0xf05ffc76, BRF_GRA },           //  9
	{ "epr-11029.88",	0x08000, 0xcedc9c61, BRF_GRA },           // 10

	{ "pr11026.20",		0x00100, 0x27057298, BRF_GRA },           // 11 Red PROM
	{ "pr11025.14",		0x00100, 0x41e4d86b, BRF_GRA },           // 12 Blue
	{ "pr11024.8",		0x00100, 0x08d71954, BRF_GRA },           // 13 Green
	{ "pr5317.37",		0x00100, 0x648350b8, BRF_GRA },           // 14 Timing PROM
};

STD_ROM_PICK(wbmljb)
STD_ROM_FN(wbmljb)

// Wonder Boy - Monster Land (Japan Old Ver., MC-8123, 317-0043)

static struct BurnRomInfo wbmljoRomDesc[] = {
	{ "epr-11031.90",	0x8000, 0x497ebfb4, BRF_ESS | BRF_PRG }, //  0 Z80 #1 Program Code
	{ "epr-11032.91",	0x8000, 0x9d03bdb2, BRF_ESS | BRF_PRG }, //  1
	{ "epr-11033.92",	0x8000, 0x7076905c, BRF_ESS | BRF_PRG }, //  2

	{ "epr-11037.126",	0x8000, 0x7a4ee585, BRF_ESS | BRF_PRG }, //  3 Z80 #2 Program Code

	{ "epr-11034.4",	0x8000, 0x37a2077d, BRF_GRA },           //  4 Tiles
	{ "epr-11035.5",	0x8000, 0xcdf2a21b, BRF_GRA },           //  5
	{ "epr-11036.6",	0x8000, 0x644687fa, BRF_GRA },           //  6

	{ "epr-11028.87",	0x8000, 0xaf0b3972, BRF_GRA },           //  7 Sprites
	{ "epr-11027.86",	0x8000, 0x277d8f1d, BRF_GRA },           //  8
	{ "epr-11030.89",	0x8000, 0xf05ffc76, BRF_GRA },           //  9
	{ "epr-11029.88",	0x8000, 0xcedc9c61, BRF_GRA },           // 10

	{ "pr11026.20",		0x0100, 0x27057298, BRF_GRA },           // 11 Red PROM
	{ "pr11025.14",		0x0100, 0x41e4d86b, BRF_GRA },           // 12 Blue
	{ "pr11024.8",		0x0100, 0x08d71954, BRF_GRA },           // 13 Green
	{ "pr5317.37",		0x0100, 0x648350b8, BRF_GRA },           // 14 Timing PROM

	{ "317-0043.key",	0x2000, 0xe354abfc, BRF_ESS },           // 15 Encryption Key
};

STD_ROM_PICK(wbmljo)
STD_ROM_FN(wbmljo)

// Wonder Boy - Monster Land (English bootleg set 1)

static struct BurnRomInfo wbmlbRomDesc[] = {
	{ "wbml.01",		0x10000, 0x66482638, BRF_ESS | BRF_PRG }, //  0 Z80 #1 Program Code
	{ "wbml.02",		0x10000, 0x48746bb6, BRF_ESS | BRF_PRG }, //  1
	{ "wbml.03",		0x10000, 0xd57ba8aa, BRF_ESS | BRF_PRG }, //  2

	{ "epr-11037.126",	0x08000, 0x7a4ee585, BRF_ESS | BRF_PRG }, //  3 Z80 #2 Program Code

	{ "wbml.08",		0x08000, 0xbbea6afe, BRF_GRA },           //  4 Tiles
	{ "wbml.09",		0x08000, 0x77567d41, BRF_GRA },           //  5
	{ "wbml.10",		0x08000, 0xa52ffbdd, BRF_GRA },           //  6

	{ "epr-11028.87",	0x08000, 0xaf0b3972, BRF_GRA },           //  7 Sprites
	{ "epr-11027.86",	0x08000, 0x277d8f1d, BRF_GRA },           //  8
	{ "epr-11030.89",	0x08000, 0xf05ffc76, BRF_GRA },           //  9
	{ "epr-11029.88",	0x08000, 0xcedc9c61, BRF_GRA },           // 10

	{ "pr11026.20",		0x00100, 0x27057298, BRF_GRA },           // 11 Red PROM
	{ "pr11025.14",		0x00100, 0x41e4d86b, BRF_GRA },           // 12 Blue
	{ "pr11024.8",		0x00100, 0x08d71954, BRF_GRA },           // 13 Green
	{ "pr5317.37",		0x00100, 0x648350b8, BRF_GRA },           // 14 Timing PROM
};

STD_ROM_PICK(wbmlb)
STD_ROM_FN(wbmlb)

// Wonder Boy - Monster Land (English bootleg set 2)

static struct BurnRomInfo wbmlbgRomDesc[] = {
	{ "galaxy.ic90",	0x10000, 0x66482638, BRF_ESS | BRF_PRG }, //  0 Z80 #1 Program Code
	{ "galaxy.ic91",	0x10000, 0x89a8ab93, BRF_ESS | BRF_PRG }, //  1
	{ "galaxy.ic92",	0x08000, 0x39e07286, BRF_ESS | BRF_PRG }, //  2

	{ "epr-11037.126",	0x08000, 0x7a4ee585, BRF_ESS | BRF_PRG }, //  3 Z80 #2 Program Code

	{ "galaxy.ic4",		0x08000, 0xab75d056, BRF_GRA },           //  4 Tiles
	{ "galaxy.ic6",		0x08000, 0x6bb5e601, BRF_GRA },           //  5
	{ "galaxy.ic5",		0x08000, 0x3c11d151, BRF_GRA },           //  6

	{ "epr-11028.87",	0x08000, 0xaf0b3972, BRF_GRA },           //  7 Sprites
	{ "epr-11027.86",	0x08000, 0x277d8f1d, BRF_GRA },           //  8
	{ "epr-11030.89",	0x08000, 0xf05ffc76, BRF_GRA },           //  9
	{ "epr-11029.88",	0x08000, 0xcedc9c61, BRF_GRA },           // 10

	{ "pr11026.20",		0x00100, 0x27057298, BRF_GRA },           // 11 Red PROM
	{ "pr11025.14",		0x00100, 0x41e4d86b, BRF_GRA },           // 12 Blue
	{ "pr11024.8",		0x00100, 0x08d71954, BRF_GRA },           // 13 Green
	{ "pr5317.37",		0x00100, 0x648350b8, BRF_GRA },           // 14 Timing PROM
};

STD_ROM_PICK(wbmlbg)
STD_ROM_FN(wbmlbg)

// Wonder Boy - Monster Land (English bootleg set 3)

static struct BurnRomInfo wbmlbgeRomDesc[] = {
	{ "3.k3",		0x10000, 0xb4f90adc, BRF_ESS | BRF_PRG }, //  0 Z80 #1 Program Code
	{ "2.k4",		0x10000, 0x1896c19b, BRF_ESS | BRF_PRG }, //  1
	{ "1.k4",		0x10000, 0x0e827f13, BRF_ESS | BRF_PRG }, //  2

	{ "11.d9",		0x08000, 0x7a4ee585, BRF_ESS | BRF_PRG }, //  3 Z80 #2 Program Code

	{ "8.y6",		0x08000, 0xab75d056, BRF_GRA },           //  4 Tiles
	{ "9.y5",		0x08000, 0x6bb5e601, BRF_GRA },           //  5
	{ "10.y5",		0x08000, 0x3c11d151, BRF_GRA },           //  6

	{ "5.k2",		0x08000, 0xaf0b3972, BRF_GRA },           //  7 Sprites
	{ "4.k2",		0x08000, 0x277d8f1d, BRF_GRA },           //  8
	{ "7.k1",		0x08000, 0xf05ffc76, BRF_GRA },           //  9
	{ "6.k1",		0x08000, 0xcedc9c61, BRF_GRA },           // 10

	{ "3.z8",		0x00100, 0x27057298, BRF_GRA },           // 11 Red PROM
	{ "2.y8",		0x00100, 0x41e4d86b, BRF_GRA },           // 12 Blue
	{ "1.x8",		0x00100, 0x08d71954, BRF_GRA },           // 13 Green
	{ "pr5317.37",	0x00100, 0x648350b8, BRF_GRA },           // 14 Timing PROM
};

STD_ROM_PICK(wbmlbge)
STD_ROM_FN(wbmlbge)

// Wonder Boy - Monster Land (English, Virtual Console)

static struct BurnRomInfo wbmlvcRomDesc[] = {
	{ "vc.ic90",		0x10000, 0x093c4852, BRF_ESS | BRF_PRG }, //  0 Z80 #1 Program Code
	{ "vc.ic91",		0x10000, 0x7e973ece, BRF_ESS | BRF_PRG }, //  1
	{ "vc.ic92",		0x08000, 0x32661e7e, BRF_ESS | BRF_PRG }, //  2

	{ "epr-11037.126",	0x08000, 0x7a4ee585, BRF_ESS | BRF_PRG }, //  3 Z80 #2 Program Code

	{ "vc.ic4",		    0x08000, 0x820bee59, BRF_GRA },           //  4 Tiles
	{ "vc.ic5",		    0x08000, 0xa9a1447e, BRF_GRA },           //  5
	{ "vc.ic6",		    0x08000, 0x359026a0, BRF_GRA },           //  6

	{ "epr-11028.87",	0x08000, 0xaf0b3972, BRF_GRA },           //  7 Sprites
	{ "epr-11027.86",	0x08000, 0x277d8f1d, BRF_GRA },           //  8
	{ "epr-11030.89",	0x08000, 0xf05ffc76, BRF_GRA },           //  9
	{ "epr-11029.88",	0x08000, 0xcedc9c61, BRF_GRA },           // 10

	{ "pr11026.20",		0x00100, 0x27057298, BRF_GRA },           // 11 Red PROM
	{ "pr11025.14",		0x00100, 0x41e4d86b, BRF_GRA },           // 12 Blue
	{ "pr11024.8",		0x00100, 0x08d71954, BRF_GRA },           // 13 Green
	{ "pr5317.37",		0x00100, 0x648350b8, BRF_GRA },           // 14 Timing PROM
};

STD_ROM_PICK(wbmlvc)
STD_ROM_FN(wbmlvc)

// Wonder Boy - Monster Land (decrypted bootleg of English, Virtual Console release)
// Fully decrypted version

static struct BurnRomInfo wbmlvcdRomDesc[] = {
	{ "wbmlvcd.ic90",	0x08000, 0xf9c04c07, BRF_ESS | BRF_PRG }, //  0 Z80 #1 Program Code
	{ "wbmlvcd.ic91",	0x08000, 0x87167a57, BRF_ESS | BRF_PRG }, //  1
	{ "wbmlvcd.ic92",	0x08000, 0xffb69e82, BRF_ESS | BRF_PRG }, //  2

	{ "epr-11037.126",	0x08000, 0x7a4ee585, BRF_ESS | BRF_PRG }, //  3 Z80 #2 Program Code

	{ "vc.ic4",			0x08000, 0x820bee59, BRF_GRA },           //  4 Tiles
	{ "vc.ic5",			0x08000, 0xa9a1447e, BRF_GRA },           //  5
	{ "vc.ic6",			0x08000, 0x359026a0, BRF_GRA },           //  6

	{ "epr-11028.87",	0x08000, 0xaf0b3972, BRF_GRA },           //  7 Sprites
	{ "epr-11027.86",	0x08000, 0x277d8f1d, BRF_GRA },           //  8
	{ "epr-11030.89",	0x08000, 0xf05ffc76, BRF_GRA },           //  9
	{ "epr-11029.88",	0x08000, 0xcedc9c61, BRF_GRA },           // 10

	{ "pr11026.20",		0x00100, 0x27057298, BRF_GRA },           // 11 Red PROM
	{ "pr11025.14",		0x00100, 0x41e4d86b, BRF_GRA },           // 12 Blue
	{ "pr11024.8",		0x00100, 0x08d71954, BRF_GRA },           // 13 Green
	{ "pr5317.37",		0x00100, 0x648350b8, BRF_GRA },           // 14 Timing PROM
};

STD_ROM_PICK(wbmlvcd)
STD_ROM_FN(wbmlvcd)

// Wonder Boy - Monster Land (decrypted bootleg of Japan New Ver., MC-8123, 317-0043)
// Fully decrypted version

static struct BurnRomInfo wbmldRomDesc[] = {
	{ "decrypted_epr-11031a.90",	0x08000, 0xaba42eb7, BRF_ESS | BRF_PRG }, //  0 Z80 #1 Program Code
	{ "decrypted_epr-11032.91",		0x08000, 0x1b158845, BRF_ESS | BRF_PRG }, //  1
	{ "decrypted_epr-11033.92",		0x08000, 0x39e07286, BRF_ESS | BRF_PRG }, //  2

	{ "epr-11037.126",				0x08000, 0x7a4ee585, BRF_ESS | BRF_PRG }, //  3 Z80 #2 Program Code

	{ "epr-11034.4",				0x08000, 0x37a2077d, BRF_GRA },           //  4 Tiles
	{ "epr-11035.5",				0x08000, 0xcdf2a21b, BRF_GRA },           //  5
	{ "epr-11036.6",				0x08000, 0x644687fa, BRF_GRA },           //  6

	{ "epr-11028.87",				0x08000, 0xaf0b3972, BRF_GRA },           //  7 Sprites
	{ "epr-11027.86",				0x08000, 0x277d8f1d, BRF_GRA },           //  8
	{ "epr-11030.89",				0x08000, 0xf05ffc76, BRF_GRA },           //  9
	{ "epr-11029.88",				0x08000, 0xcedc9c61, BRF_GRA },           // 10

	{ "pr11026.20",					0x00100, 0x27057298, BRF_GRA },           // 11 Red PROM
	{ "pr11025.14",					0x00100, 0x41e4d86b, BRF_GRA },           // 12 Blue
	{ "pr11024.8",					0x00100, 0x08d71954, BRF_GRA },           // 13 Green
	{ "pr5317.37",					0x00100, 0x648350b8, BRF_GRA },           // 14 Timing PROM
};

STD_ROM_PICK(wbmld)
STD_ROM_FN(wbmld)

// Wonder Boy - Monster Land (decrypted bootleg of Japan Old Ver., MC-8123, 317-0043)
// Fully decrypted version

static struct BurnRomInfo wbmljodRomDesc[] = {
	{ "decrypted_epr-11031.90",	0x08000, 0x940b35bf, BRF_ESS | BRF_PRG }, //  0 Z80 #1 Program Code
	{ "decrypted_epr-11032.91",	0x08000, 0x1b158845, BRF_ESS | BRF_PRG }, //  1
	{ "decrypted_epr-11033.92",	0x08000, 0x39e07286, BRF_ESS | BRF_PRG }, //  2

	{ "epr-11037.126",			0x08000, 0x7a4ee585, BRF_ESS | BRF_PRG }, //  3 Z80 #2 Program Code

	{ "epr-11034.4",			0x08000, 0x37a2077d, BRF_GRA },           //  4 Tiles
	{ "epr-11035.5",			0x08000, 0xcdf2a21b, BRF_GRA },           //  5
	{ "epr-11036.6",			0x08000, 0x644687fa, BRF_GRA },           //  6

	{ "epr-11028.87",			0x08000, 0xaf0b3972, BRF_GRA },           //  7 Sprites
	{ "epr-11027.86",			0x08000, 0x277d8f1d, BRF_GRA },           //  8
	{ "epr-11030.89",			0x08000, 0xf05ffc76, BRF_GRA },           //  9
	{ "epr-11029.88",			0x08000, 0xcedc9c61, BRF_GRA },           // 10

	{ "pr11026.20",				0x00100, 0x27057298, BRF_GRA },           // 11 Red PROM
	{ "pr11025.14",				0x00100, 0x41e4d86b, BRF_GRA },           // 12 Blue
	{ "pr11024.8",				0x00100, 0x08d71954, BRF_GRA },           // 13 Green
	{ "pr5317.37",				0x00100, 0x648350b8, BRF_GRA },           // 14 Timing PROM
};

STD_ROM_PICK(wbmljod)
STD_ROM_FN(wbmljod)

// Wonder Boy - Monster Land (English, difficulty hack)

static struct BurnRomInfo wbmlhRomDesc[] = {
	{ "6",			0x10000, 0x1ace78a0, BRF_ESS | BRF_PRG }, //  0 Z80 #1 Program Code
	{ "5",			0x10000, 0x5aa6a908, BRF_ESS | BRF_PRG }, //  1
	{ "4",			0x10000, 0xcb3ea856, BRF_ESS | BRF_PRG }, //  2

	{ "11",			0x08000, 0x7a4ee585, BRF_ESS | BRF_PRG }, //  3 Z80 #2 Program Code

	{ "3",			0x08000, 0xab75d056, BRF_GRA },           //  4 Tiles
	{ "2",			0x08000, 0x6bb5e601, BRF_GRA },           //  5
	{ "1",			0x08000, 0x3c11d151, BRF_GRA },           //  6

	{ "9",			0x08000, 0xaf0b3972, BRF_GRA },           //  7 Sprites
	{ "10",			0x08000, 0x277d8f1d, BRF_GRA },           //  8
	{ "7",			0x08000, 0xf05ffc76, BRF_GRA },           //  9
	{ "8",			0x08000, 0xcedc9c61, BRF_GRA },           // 10

	{ "pr11026.20",	0x00100, 0x27057298, BRF_GRA },           // 11 Red PROM
	{ "pr11025.14",	0x00100, 0x41e4d86b, BRF_GRA },           // 12 Blue
	{ "pr11024.8",	0x00100, 0x08d71954, BRF_GRA },           // 13 Green
	{ "pr5317.37",	0x00100, 0x648350b8, BRF_GRA },           // 14 Timing PROM
};

STD_ROM_PICK(wbmlh)
STD_ROM_FN(wbmlh)

/*==============================================================================================
Decode Functions
===============================================================================================*/

static void sega_decode(const UINT8 convtable[32][4])
{
	INT32 A;

	INT32 length = 0x10000;
	INT32 cryptlen = 0x8000;
	UINT8 *rom = System1Rom1;
	UINT8 *decrypted = System1Fetch1;

	for (A = 0x0000;A < cryptlen;A++)
	{
		INT32 xorval = 0;

		UINT8 src = rom[A];

		/* pick the translation table from bits 0, 4, 8 and 12 of the address */
		INT32 row = (A & 1) + (((A >> 4) & 1) << 1) + (((A >> 8) & 1) << 2) + (((A >> 12) & 1) << 3);

		/* pick the offset in the table from bits 3 and 5 of the source data */
		INT32 col = ((src >> 3) & 1) + (((src >> 5) & 1) << 1);
		/* the bottom half of the translation table is the mirror image of the top */
		if (src & 0x80)
		{
			col = 3 - col;
			xorval = 0xa8;
		}

		/* decode the opcodes */
		decrypted[A] = (src & ~0xa8) | (convtable[2*row][col] ^ xorval);

		/* decode the data */
		rom[A] = (src & ~0xa8) | (convtable[2*row+1][col] ^ xorval);

		if (convtable[2*row][col] == 0xff)	/* table incomplete! (for development) */
			decrypted[A] = 0xee;
		if (convtable[2*row+1][col] == 0xff)	/* table incomplete! (for development) */
			rom[A] = 0xee;
	}

	/* this is a kludge to catch anyone who has code that crosses the encrypted/ */
	/* decrypted boundary. ssanchan does it */
	if (length > 0x8000)
	{
		INT32 bytes = 0x4000;
		memcpy(&decrypted[0x8000], &rom[0x8000], bytes);
	}
}

static void bullfgtj_decode(void)
{
	static const UINT8 convtable[32][4] =
	{
		/*       opcode                   data                     address      */
		/*  A    B    C    D         A    B    C    D                           */
		{ 0xa0,0xa8,0x20,0x28 }, { 0x80,0xa0,0x00,0x20 },	/* ...0...0...0...0 */
		{ 0x20,0x28,0x00,0x08 }, { 0x20,0x28,0x00,0x08 },	/* ...0...0...0...1 */
		{ 0xa0,0xa8,0x20,0x28 }, { 0x08,0x28,0x00,0x20 },	/* ...0...0...1...0 */
		{ 0x88,0x08,0xa8,0x28 }, { 0x88,0x08,0xa8,0x28 },	/* ...0...0...1...1 */
		{ 0xa0,0xa8,0x20,0x28 }, { 0x20,0x28,0x00,0x08 },	/* ...0...1...0...0 */
		{ 0x28,0xa8,0x20,0xa0 }, { 0x20,0x28,0x00,0x08 },	/* ...0...1...0...1 */
		{ 0xa0,0xa8,0x20,0x28 }, { 0x08,0x28,0x00,0x20 },	/* ...0...1...1...0 */
		{ 0x88,0x08,0xa8,0x28 }, { 0x88,0x08,0xa8,0x28 },	/* ...0...1...1...1 */
		{ 0x28,0xa8,0x20,0xa0 }, { 0xa0,0xa8,0x20,0x28 },	/* ...1...0...0...0 */
		{ 0x88,0x08,0xa8,0x28 }, { 0x80,0xa0,0x00,0x20 },	/* ...1...0...0...1 */
		{ 0x28,0xa8,0x20,0xa0 }, { 0x08,0x28,0x00,0x20 },	/* ...1...0...1...0 */
		{ 0x28,0xa8,0x20,0xa0 }, { 0x80,0xa0,0x00,0x20 },	/* ...1...0...1...1 */
		{ 0x20,0x28,0x00,0x08 }, { 0x20,0x28,0x00,0x08 },	/* ...1...1...0...0 */
		{ 0x88,0x08,0xa8,0x28 }, { 0x20,0x28,0x00,0x08 },	/* ...1...1...0...1 */
		{ 0x08,0x28,0x00,0x20 }, { 0x80,0xa0,0x00,0x20 },	/* ...1...1...1...0 */
		{ 0x08,0x28,0x00,0x20 }, { 0x88,0x08,0xa8,0x28 }	/* ...1...1...1...1 */
	};

	sega_decode(convtable);
}

static void flicky_decode(void)
{
	static const UINT8 convtable[32][4] =
	{
		/*       opcode                   data                     address      */
		/*  A    B    C    D         A    B    C    D                           */
		{ 0x08,0x88,0x00,0x80 }, { 0xa0,0x80,0xa8,0x88 },	/* ...0...0...0...0 */
		{ 0x80,0x00,0xa0,0x20 }, { 0x88,0x80,0x08,0x00 },	/* ...0...0...0...1 */
		{ 0xa0,0x80,0xa8,0x88 }, { 0x28,0x08,0x20,0x00 },	/* ...0...0...1...0 */
		{ 0x28,0x08,0x20,0x00 }, { 0xa0,0x80,0xa8,0x88 },	/* ...0...0...1...1 */
		{ 0x08,0x88,0x00,0x80 }, { 0x80,0x00,0xa0,0x20 },	/* ...0...1...0...0 */
		{ 0x80,0x00,0xa0,0x20 }, { 0x88,0x80,0x08,0x00 },	/* ...0...1...0...1 */
		{ 0x28,0x08,0x20,0x00 }, { 0x28,0x08,0x20,0x00 },	/* ...0...1...1...0 */
		{ 0x28,0x08,0x20,0x00 }, { 0x88,0x80,0x08,0x00 },	/* ...0...1...1...1 */
		{ 0x08,0x88,0x00,0x80 }, { 0xa8,0x88,0x28,0x08 },	/* ...1...0...0...0 */
		{ 0xa8,0x88,0x28,0x08 }, { 0x80,0x00,0xa0,0x20 },	/* ...1...0...0...1 */
		{ 0x28,0x08,0x20,0x00 }, { 0x88,0x80,0x08,0x00 },	/* ...1...0...1...0 */
		{ 0xa8,0x88,0x28,0x08 }, { 0x88,0x80,0x08,0x00 },	/* ...1...0...1...1 */
		{ 0x08,0x88,0x00,0x80 }, { 0x80,0x00,0xa0,0x20 },	/* ...1...1...0...0 */
		{ 0xa8,0x88,0x28,0x08 }, { 0x80,0x00,0xa0,0x20 },	/* ...1...1...0...1 */
		{ 0x28,0x08,0x20,0x00 }, { 0x28,0x08,0x20,0x00 },	/* ...1...1...1...0 */
		{ 0x08,0x88,0x00,0x80 }, { 0x88,0x80,0x08,0x00 }	/* ...1...1...1...1 */
	};

	sega_decode(convtable);
}

static void hvymetal_decode(void)
{
	static const UINT8 convtable[32][4] =
	{
		/*       opcode                   data                     address      */
		/*  A    B    C    D         A    B    C    D                           */
		{ 0x88,0xa8,0x80,0xa0 }, { 0xa0,0x80,0xa8,0x88 },	/* ...0...0...0...0 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0x88,0x80,0x08,0x00 },	/* ...0...0...0...1 */
		{ 0xa0,0x80,0xa8,0x88 }, { 0x88,0xa8,0x80,0xa0 },	/* ...0...0...1...0 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0x88,0x80,0x08,0x00 },	/* ...0...0...1...1 */
		{ 0xa0,0x80,0xa8,0x88 }, { 0x88,0x80,0x08,0x00 },	/* ...0...1...0...0 */
		{ 0x88,0x80,0x08,0x00 }, { 0x88,0x80,0x08,0x00 },	/* ...0...1...0...1 */
		{ 0xa0,0x80,0xa8,0x88 }, { 0x88,0x80,0x08,0x00 },	/* ...0...1...1...0 */
		{ 0x88,0x80,0x08,0x00 }, { 0x28,0x08,0xa8,0x88 },	/* ...0...1...1...1 */
		{ 0xa0,0x20,0xa8,0x28 }, { 0x88,0xa8,0x80,0xa0 },	/* ...1...0...0...0 */
		{ 0xa0,0x20,0xa8,0x28 }, { 0x88,0xa8,0x80,0xa0 },	/* ...1...0...0...1 */
		{ 0xa0,0x20,0xa8,0x28 }, { 0x88,0xa8,0x80,0xa0 },	/* ...1...0...1...0 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0x28,0x08,0xa8,0x88 },	/* ...1...0...1...1 */
		{ 0x28,0xa8,0x08,0x88 }, { 0xa0,0x20,0xa8,0x28 },	/* ...1...1...0...0 */
		{ 0xa0,0x20,0xa8,0x28 }, { 0x28,0xa8,0x08,0x88 },	/* ...1...1...0...1 */
		{ 0x28,0xa8,0x08,0x88 }, { 0xa0,0x20,0xa8,0x28 },	/* ...1...1...1...0 */
		{ 0x28,0x08,0xa8,0x88 }, { 0x28,0xa8,0x08,0x88 }	/* ...1...1...1...1 */
	};

	sega_decode(convtable);
}

static void imsorry_decode(void)
{
	static const UINT8 convtable[32][4] =
	{
		/*       opcode                   data                     address      */
		/*  A    B    C    D         A    B    C    D                           */
		{ 0x88,0x08,0x80,0x00 }, { 0x00,0x20,0x80,0xa0 },	/* ...0...0...0...0 */
		{ 0x00,0x20,0x80,0xa0 }, { 0x88,0x08,0x80,0x00 },	/* ...0...0...0...1 */
		{ 0x88,0x08,0xa8,0x28 }, { 0x00,0x20,0x80,0xa0 },	/* ...0...0...1...0 */
		{ 0x00,0x20,0x80,0xa0 }, { 0x88,0x08,0xa8,0x28 },	/* ...0...0...1...1 */
		{ 0x00,0x20,0x80,0xa0 }, { 0x08,0x00,0x88,0x80 },	/* ...0...1...0...0 */
		{ 0x00,0x20,0x80,0xa0 }, { 0x20,0x28,0xa0,0xa8 },	/* ...0...1...0...1 */
		{ 0x20,0x28,0xa0,0xa8 }, { 0x00,0x20,0x80,0xa0 },	/* ...0...1...1...0 */
		{ 0x20,0x28,0xa0,0xa8 }, { 0x88,0x08,0xa8,0x28 },	/* ...0...1...1...1 */
		{ 0x88,0x08,0x80,0x00 }, { 0x08,0x00,0x88,0x80 },	/* ...1...0...0...0 */
		{ 0x08,0x00,0x88,0x80 }, { 0x88,0x08,0x80,0x00 },	/* ...1...0...0...1 */
		{ 0x08,0x28,0x00,0x20 }, { 0x08,0x28,0x00,0x20 },	/* ...1...0...1...0 */
		{ 0x88,0x08,0x80,0x00 }, { 0x08,0x28,0x00,0x20 },	/* ...1...0...1...1 */
		{ 0x08,0x28,0x00,0x20 }, { 0x08,0x00,0x88,0x80 },	/* ...1...1...0...0 */
		{ 0x08,0x28,0x00,0x20 }, { 0x20,0x28,0xa0,0xa8 },	/* ...1...1...0...1 */
		{ 0x20,0x28,0xa0,0xa8 }, { 0x08,0x28,0x00,0x20 },	/* ...1...1...1...0 */
		{ 0x20,0x28,0xa0,0xa8 }, { 0x08,0x28,0x00,0x20 }	/* ...1...1...1...1 */
	};

	sega_decode(convtable);
}

static void mrviking_decode(void)
{
	static const UINT8 convtable[32][4] =
	{
		/*       opcode                   data                     address      */
		/*  A    B    C    D         A    B    C    D                           */
		{ 0x28,0xa8,0x08,0x88 }, { 0x88,0x80,0x08,0x00 },	/* ...0...0...0...0 */
		{ 0x88,0x08,0x80,0x00 }, { 0x88,0x80,0x08,0x00 },	/* ...0...0...0...1 */
		{ 0x28,0x08,0xa8,0x88 }, { 0x28,0xa8,0x08,0x88 },	/* ...0...0...1...0 */
		{ 0x88,0x08,0x80,0x00 }, { 0x88,0x08,0x80,0x00 },	/* ...0...0...1...1 */
		{ 0x28,0x08,0xa8,0x88 }, { 0x88,0x80,0x08,0x00 },	/* ...0...1...0...0 */
		{ 0x88,0x80,0x08,0x00 }, { 0x28,0xa8,0x08,0x88 },	/* ...0...1...0...1 */
		{ 0xa0,0x80,0xa8,0x88 }, { 0x28,0x08,0xa8,0x88 },	/* ...0...1...1...0 */
		{ 0xa0,0x80,0xa8,0x88 }, { 0xa0,0x80,0xa8,0x88 },	/* ...0...1...1...1 */
		{ 0x88,0x80,0x08,0x00 }, { 0x88,0x80,0x08,0x00 },	/* ...1...0...0...0 */
		{ 0x88,0x08,0x80,0x00 }, { 0x88,0x80,0x08,0x00 },	/* ...1...0...0...1 */
		{ 0xa0,0x80,0x20,0x00 }, { 0x28,0x08,0xa8,0x88 },	/* ...1...0...1...0 */
		{ 0xa0,0x80,0x20,0x00 }, { 0x88,0x08,0x80,0x00 },	/* ...1...0...1...1 */
		{ 0x28,0x08,0xa8,0x88 }, { 0xa0,0x80,0x20,0x00 },	/* ...1...1...0...0 */
		{ 0xa0,0x80,0x20,0x00 }, { 0xa0,0x80,0x20,0x00 },	/* ...1...1...0...1 */
		{ 0xa0,0x80,0xa8,0x88 }, { 0x28,0x08,0xa8,0x88 },	/* ...1...1...1...0 */
		{ 0xa0,0x80,0x20,0x00 }, { 0xa0,0x80,0xa8,0x88 }	/* ...1...1...1...1 */
	};

	sega_decode(convtable);
}

static void myheroj_decode(void)
{
	static const UINT8 convtable[32][4] =
	{
		/*       opcode                   data                     address      */
		/*  A    B    C    D         A    B    C    D                           */
		{ 0x20,0x00,0xa0,0x80 }, { 0x80,0xa0,0x88,0xa8 },	/* ...0...0...0...0 */
		{ 0x20,0x00,0xa0,0x80 }, { 0x80,0xa0,0x88,0xa8 },	/* ...0...0...0...1 */
		{ 0xa8,0xa0,0x88,0x80 }, { 0xa8,0xa0,0x88,0x80 },	/* ...0...0...1...0 */
		{ 0x08,0x88,0x00,0x80 }, { 0x80,0xa0,0x88,0xa8 },	/* ...0...0...1...1 */
		{ 0x20,0x00,0xa0,0x80 }, { 0x28,0xa8,0x08,0x88 },	/* ...0...1...0...0 */
		{ 0x20,0x00,0xa0,0x80 }, { 0x08,0x88,0x00,0x80 },	/* ...0...1...0...1 */
		{ 0x28,0xa8,0x08,0x88 }, { 0xa8,0xa0,0x88,0x80 },	/* ...0...1...1...0 */
		{ 0x08,0x88,0x00,0x80 }, { 0xa8,0xa0,0x88,0x80 },	/* ...0...1...1...1 */
		{ 0x28,0xa8,0x08,0x88 }, { 0x20,0x00,0xa0,0x80 },	/* ...1...0...0...0 */
		{ 0x80,0xa0,0x88,0xa8 }, { 0x20,0x00,0xa0,0x80 },	/* ...1...0...0...1 */
		{ 0x80,0xa0,0x88,0xa8 }, { 0x80,0xa0,0x88,0xa8 },	/* ...1...0...1...0 */
		{ 0xa8,0xa0,0x88,0x80 }, { 0x80,0xa0,0x88,0xa8 },	/* ...1...0...1...1 */
		{ 0x88,0x80,0x08,0x00 }, { 0x88,0x80,0x08,0x00 },	/* ...1...1...0...0 */
		{ 0x88,0x80,0x08,0x00 }, { 0x08,0x88,0x00,0x80 },	/* ...1...1...0...1 */
		{ 0x88,0x80,0x08,0x00 }, { 0xa8,0xa0,0x88,0x80 },	/* ...1...1...1...0 */
		{ 0x88,0x80,0x08,0x00 }, { 0xa8,0xa0,0x88,0x80 }	/* ...1...1...1...1 */
	};

	sega_decode(convtable);
}

static void myherok_decode(void)
{
	UINT8 *rom = System1Rom1;

	for (INT32 A = 0; A < 0xc000; A++)
		rom[A] = (rom[A] & 0xfc) | ((rom[A] & 1) << 1) | ((rom[A] & 2) >> 1);

	static const UINT8 convtable[32][4] =
	{
		/*       opcode                   data                     address      */
		/*  A    B    C    D         A    B    C    D                           */
		{ 0x20,0x00,0xa0,0x80 }, { 0x80,0xa0,0x88,0xa8 },	/* ...0...0...0...0 */
		{ 0x20,0x00,0xa0,0x80 }, { 0x80,0xa0,0x88,0xa8 },	/* ...0...0...0...1 */
		{ 0xa8,0xa0,0x88,0x80 }, { 0xa8,0xa0,0x88,0x80 },	/* ...0...0...1...0 */
		{ 0x08,0x88,0x00,0x80 }, { 0x80,0xa0,0x88,0xa8 },	/* ...0...0...1...1 */
		{ 0x20,0x00,0xa0,0x80 }, { 0x28,0xa8,0x08,0x88 },	/* ...0...1...0...0 */
		{ 0x20,0x00,0xa0,0x80 }, { 0x08,0x88,0x00,0x80 },	/* ...0...1...0...1 */
		{ 0x28,0xa8,0x08,0x88 }, { 0xa8,0xa0,0x88,0x80 },	/* ...0...1...1...0 */
		{ 0x08,0x88,0x00,0x80 }, { 0xa8,0xa0,0x88,0x80 },	/* ...0...1...1...1 */
		{ 0x28,0xa8,0x08,0x88 }, { 0x20,0x00,0xa0,0x80 },	/* ...1...0...0...0 */
		{ 0x80,0xa0,0x88,0xa8 }, { 0x20,0x00,0xa0,0x80 },	/* ...1...0...0...1 */
		{ 0x80,0xa0,0x88,0xa8 }, { 0x80,0xa0,0x88,0xa8 },	/* ...1...0...1...0 */
		{ 0xa8,0xa0,0x88,0x80 }, { 0x80,0xa0,0x88,0xa8 },	/* ...1...0...1...1 */
		{ 0x88,0x80,0x08,0x00 }, { 0x88,0x80,0x08,0x00 },	/* ...1...1...0...0 */
		{ 0x88,0x80,0x08,0x00 }, { 0x08,0x88,0x00,0x80 },	/* ...1...1...0...1 */
		{ 0x88,0x80,0x08,0x00 }, { 0xa8,0xa0,0x88,0x80 },	/* ...1...1...1...0 */
		{ 0x88,0x80,0x08,0x00 }, { 0xa8,0xa0,0x88,0x80 }	/* ...1...1...1...1 */
	};

	sega_decode(convtable);
}

static void nprinces_decode(void)
{
	static const UINT8 convtable[32][4] =
	{
		/*       opcode                   data                     address      */
		/*  A    B    C    D         A    B    C    D                           */
		{ 0x08,0x88,0x00,0x80 }, { 0xa0,0x20,0x80,0x00 },	/* ...0...0...0...0 */
		{ 0xa8,0xa0,0x28,0x20 }, { 0x88,0xa8,0x80,0xa0 },	/* ...0...0...0...1 */
		{ 0x88,0x80,0x08,0x00 }, { 0x28,0x08,0xa8,0x88 },	/* ...0...0...1...0 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0x28,0x08,0xa8,0x88 },	/* ...0...0...1...1 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0xa0,0x20,0x80,0x00 },	/* ...0...1...0...0 */
		{ 0xa8,0xa0,0x28,0x20 }, { 0xa8,0xa0,0x28,0x20 },	/* ...0...1...0...1 */
		{ 0x88,0x80,0x08,0x00 }, { 0x88,0xa8,0x80,0xa0 },	/* ...0...1...1...0 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0x88,0xa8,0x80,0xa0 },	/* ...0...1...1...1 */
		{ 0xa0,0x20,0x80,0x00 }, { 0xa0,0x20,0x80,0x00 },	/* ...1...0...0...0 */
		{ 0x08,0x88,0x00,0x80 }, { 0x28,0x08,0xa8,0x88 },	/* ...1...0...0...1 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0x88,0x80,0x08,0x00 },	/* ...1...0...1...0 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0x28,0x08,0xa8,0x88 },	/* ...1...0...1...1 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0x88,0xa8,0x80,0xa0 },	/* ...1...1...0...0 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0x88,0xa8,0x80,0xa0 },	/* ...1...1...0...1 */
		{ 0x88,0x80,0x08,0x00 }, { 0x88,0x80,0x08,0x00 },	/* ...1...1...1...0 */
		{ 0x08,0x88,0x00,0x80 }, { 0x28,0x08,0xa8,0x88 }	/* ...1...1...1...1 */
	};

	sega_decode(convtable);
}

static void pitfall2_decode(void)
{
	static const UINT8 convtable[32][4] =
	{
		/*       opcode                   data                     address      */
		/*  A    B    C    D         A    B    C    D                           */
		{ 0xa0,0x80,0xa8,0x88 }, { 0xa0,0x80,0xa8,0x88 },	/* ...0...0...0...0 */
		{ 0x08,0x88,0x28,0xa8 }, { 0x28,0xa8,0x20,0xa0 },	/* ...0...0...0...1 */
		{ 0xa0,0x80,0xa8,0x88 }, { 0xa0,0x80,0xa8,0x88 },	/* ...0...0...1...0 */
		{ 0xa0,0xa8,0x20,0x28 }, { 0xa0,0xa8,0x20,0x28 },	/* ...0...0...1...1 */
		{ 0xa0,0x80,0xa8,0x88 }, { 0x20,0x00,0xa0,0x80 },	/* ...0...1...0...0 */
		{ 0x28,0xa8,0x20,0xa0 }, { 0x20,0x00,0xa0,0x80 },	/* ...0...1...0...1 */
		{ 0xa0,0xa8,0x20,0x28 }, { 0xa0,0xa8,0x20,0x28 },	/* ...0...1...1...0 */
		{ 0x28,0xa8,0x20,0xa0 }, { 0xa0,0xa8,0x20,0x28 },	/* ...0...1...1...1 */
		{ 0x20,0x00,0xa0,0x80 }, { 0x80,0x88,0xa0,0xa8 },	/* ...1...0...0...0 */
		{ 0x80,0x88,0xa0,0xa8 }, { 0x80,0x88,0xa0,0xa8 },	/* ...1...0...0...1 */
		{ 0xa0,0xa8,0x20,0x28 }, { 0xa0,0x80,0xa8,0x88 },	/* ...1...0...1...0 */
		{ 0x80,0x88,0xa0,0xa8 }, { 0x28,0xa8,0x20,0xa0 },	/* ...1...0...1...1 */
		{ 0x20,0x00,0xa0,0x80 }, { 0x80,0x88,0xa0,0xa8 },	/* ...1...1...0...0 */
		{ 0x80,0x88,0xa0,0xa8 }, { 0x20,0x00,0xa0,0x80 },	/* ...1...1...0...1 */
		{ 0xa0,0xa8,0x20,0x28 }, { 0xa0,0x80,0xa8,0x88 },	/* ...1...1...1...0 */
		{ 0x80,0x88,0xa0,0xa8 }, { 0x28,0xa8,0x20,0xa0 }	/* ...1...1...1...1 */
	};

	sega_decode(convtable);
}

static void regulus_decode(void)
{
	static const UINT8 convtable[32][4] =
	{
		/*       opcode                   data                     address      */
		/*  A    B    C    D         A    B    C    D                           */
		{ 0x28,0x08,0xa8,0x88 }, { 0x88,0x80,0x08,0x00 },	/* ...0...0...0...0 */
		{ 0x28,0x08,0xa8,0x88 }, { 0x28,0xa8,0x08,0x88 },	/* ...0...0...0...1 */
		{ 0x88,0x80,0x08,0x00 }, { 0x88,0x08,0x80,0x00 },	/* ...0...0...1...0 */
		{ 0x88,0x08,0x80,0x00 }, { 0x28,0xa8,0x08,0x88 },	/* ...0...0...1...1 */
		{ 0x28,0x08,0xa8,0x88 }, { 0x88,0x80,0x08,0x00 },	/* ...0...1...0...0 */
		{ 0x88,0x80,0x08,0x00 }, { 0x88,0x80,0x08,0x00 },	/* ...0...1...0...1 */
		{ 0x88,0x08,0x80,0x00 }, { 0x88,0x08,0x80,0x00 },	/* ...0...1...1...0 */
		{ 0xa0,0x80,0xa8,0x88 }, { 0xa0,0x80,0xa8,0x88 },	/* ...0...1...1...1 */
		{ 0x80,0xa0,0x00,0x20 }, { 0x28,0x08,0xa8,0x88 },	/* ...1...0...0...0 */
		{ 0x28,0xa8,0x08,0x88 }, { 0x28,0x08,0xa8,0x88 },	/* ...1...0...0...1 */
		{ 0x80,0xa0,0x00,0x20 }, { 0x80,0xa0,0x00,0x20 },	/* ...1...0...1...0 */
		{ 0x28,0xa8,0x08,0x88 }, { 0x80,0xa0,0x00,0x20 },	/* ...1...0...1...1 */
		{ 0xa0,0x80,0xa8,0x88 }, { 0x28,0x08,0xa8,0x88 },	/* ...1...1...0...0 */
		{ 0x80,0xa0,0x00,0x20 }, { 0xa0,0x80,0xa8,0x88 },	/* ...1...1...0...1 */
		{ 0xa0,0x80,0xa8,0x88 }, { 0x80,0xa0,0x00,0x20 },	/* ...1...1...1...0 */
		{ 0xa0,0x80,0xa8,0x88 }, { 0xa0,0x80,0xa8,0x88 }	/* ...1...1...1...1 */
	};

	sega_decode(convtable);
}

static void seganinj_decode(void)
{
	static const UINT8 convtable[32][4] =
	{
		/*       opcode                   data                     address      */
		/*  A    B    C    D         A    B    C    D                           */
		{ 0x88,0xa8,0x80,0xa0 }, { 0x88,0x08,0x80,0x00 },	/* ...0...0...0...0 */
		{ 0x28,0xa8,0x08,0x88 }, { 0xa0,0xa8,0x80,0x88 },	/* ...0...0...0...1 */
		{ 0xa8,0xa0,0x28,0x20 }, { 0xa8,0xa0,0x28,0x20 },	/* ...0...0...1...0 */
		{ 0x28,0xa8,0x08,0x88 }, { 0xa0,0xa8,0x80,0x88 },	/* ...0...0...1...1 */
		{ 0x28,0x08,0xa8,0x88 }, { 0x28,0x08,0xa8,0x88 },	/* ...0...1...0...0 */
		{ 0x28,0xa8,0x08,0x88 }, { 0x88,0x08,0x80,0x00 },	/* ...0...1...0...1 */
		{ 0x28,0x08,0xa8,0x88 }, { 0x28,0x08,0xa8,0x88 },	/* ...0...1...1...0 */
		{ 0x28,0xa8,0x08,0x88 }, { 0xa8,0xa0,0x28,0x20 },	/* ...0...1...1...1 */
		{ 0x88,0x08,0x80,0x00 }, { 0x88,0xa8,0x80,0xa0 },	/* ...1...0...0...0 */
		{ 0xa0,0xa8,0x80,0x88 }, { 0x28,0xa8,0x08,0x88 },	/* ...1...0...0...1 */
		{ 0xa8,0xa0,0x28,0x20 }, { 0x88,0xa8,0x80,0xa0 },	/* ...1...0...1...0 */
		{ 0xa8,0xa0,0x28,0x20 }, { 0x28,0xa8,0x08,0x88 },	/* ...1...0...1...1 */
		{ 0x28,0x08,0xa8,0x88 }, { 0x88,0xa8,0x80,0xa0 },	/* ...1...1...0...0 */
		{ 0x28,0x08,0xa8,0x88 }, { 0x28,0x08,0xa8,0x88 },	/* ...1...1...0...1 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0x88,0xa8,0x80,0xa0 },	/* ...1...1...1...0 */
		{ 0xa8,0xa0,0x28,0x20 }, { 0x28,0x08,0xa8,0x88 }	/* ...1...1...1...1 */
	};

	sega_decode(convtable);
}

static void spatter_decode(void)
{
	static const UINT8 convtable[32][4] =
	{
		/*       opcode                   data                     address      */
		/*  A    B    C    D         A    B    C    D                           */
		{ 0x88,0x08,0x80,0x00 }, { 0x00,0x08,0x20,0x28 },	/* ...0...0...0...0 */
		{ 0x28,0xa8,0x08,0x88 }, { 0x28,0xa8,0x08,0x88 },	/* ...0...0...0...1 */
		{ 0x28,0x20,0xa8,0xa0 }, { 0x28,0x20,0xa8,0xa0 },	/* ...0...0...1...0 */
		{ 0x88,0x08,0x80,0x00 }, { 0x88,0x08,0x80,0x00 },	/* ...0...0...1...1 */
		{ 0x00,0x08,0x20,0x28 }, { 0x88,0x08,0x80,0x00 },	/* ...0...1...0...0 */
		{ 0xa0,0x80,0x20,0x00 }, { 0x80,0x88,0x00,0x08 },	/* ...0...1...0...1 */
		{ 0x88,0x08,0x80,0x00 }, { 0xa0,0x80,0x20,0x00 },	/* ...0...1...1...0 */
		{ 0x88,0x08,0x80,0x00 }, { 0x28,0x20,0xa8,0xa0 },	/* ...0...1...1...1 */
		{ 0x28,0xa8,0x08,0x88 }, { 0x80,0x88,0x00,0x08 },	/* ...1...0...0...0 */
		{ 0x80,0x88,0x00,0x08 }, { 0x00,0x08,0x20,0x28 },	/* ...1...0...0...1 */
		{ 0x28,0x20,0xa8,0xa0 }, { 0x28,0xa8,0x08,0x88 },	/* ...1...0...1...0 */
		{ 0x00,0x08,0x20,0x28 }, { 0x80,0xa0,0x88,0xa8 },	/* ...1...0...1...1 */
		{ 0x80,0x88,0x00,0x08 }, { 0xa0,0x80,0x20,0x00 },	/* ...1...1...0...0 */
		{ 0x80,0xa0,0x88,0xa8 }, { 0xa0,0x80,0x20,0x00 },	/* ...1...1...0...1 */
		{ 0xa0,0x80,0x20,0x00 }, { 0x80,0xa0,0x88,0xa8 },	/* ...1...1...1...0 */
		{ 0x28,0x20,0xa8,0xa0 }, { 0x00,0x08,0x20,0x28 }	/* ...1...1...1...1 */
	};

	sega_decode(convtable);
}

static void swat_decode(void)
{
	static const UINT8 convtable[32][4] =
	{
		/*       opcode                   data                     address      */
		/*  A    B    C    D         A    B    C    D                           */
		{ 0x88,0x08,0x80,0x00 }, { 0xa0,0xa8,0x80,0x88 },	/* ...0...0...0...0 */
		{ 0x88,0x08,0x80,0x00 }, { 0x88,0xa8,0x80,0xa0 },	/* ...0...0...0...1 */
		{ 0xa0,0x80,0x20,0x00 }, { 0x88,0x08,0x80,0x00 },	/* ...0...0...1...0 */
		{ 0xa0,0xa8,0x80,0x88 }, { 0x88,0x08,0x80,0x00 },	/* ...0...0...1...1 */
		{ 0x28,0x20,0xa8,0xa0 }, { 0xa0,0xa8,0x80,0x88 },	/* ...0...1...0...0 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0x28,0x20,0xa8,0xa0 },	/* ...0...1...0...1 */
		{ 0xa0,0x80,0x20,0x00 }, { 0xa0,0xa8,0x80,0x88 },	/* ...0...1...1...0 */
		{ 0x28,0x20,0xa8,0xa0 }, { 0xa0,0xa8,0x80,0x88 },	/* ...0...1...1...1 */
		{ 0xa0,0x80,0x20,0x00 }, { 0xa0,0x80,0x20,0x00 },	/* ...1...0...0...0 */
		{ 0xa0,0x20,0x80,0x00 }, { 0x88,0xa8,0x80,0xa0 },	/* ...1...0...0...1 */
		{ 0xa0,0x20,0x80,0x00 }, { 0xa0,0x20,0x80,0x00 },	/* ...1...0...1...0 */
		{ 0xa0,0x20,0x80,0x00 }, { 0xa0,0x20,0x80,0x00 },	/* ...1...0...1...1 */
		{ 0xa0,0x80,0x20,0x00 }, { 0xa0,0x80,0x20,0x00 },	/* ...1...1...0...0 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0x28,0x20,0xa8,0xa0 },	/* ...1...1...0...1 */
		{ 0xa0,0xa8,0x80,0x88 }, { 0xa0,0x80,0x20,0x00 },	/* ...1...1...1...0 */
		{ 0x28,0x20,0xa8,0xa0 }, { 0xa0,0xa8,0x80,0x88 }	/* ...1...1...1...1 */
	};

	sega_decode(convtable);
}

static void teddybb_decode(void)
{
	static const UINT8 convtable[32][4] =
	{
		/*       opcode                   data                     address      */
		/*  A    B    C    D         A    B    C    D                           */
		{ 0x20,0x28,0x00,0x08 }, { 0x80,0x00,0xa0,0x20 },	/* ...0...0...0...0 */
		{ 0x20,0x28,0x00,0x08 }, { 0xa0,0xa8,0x20,0x28 },	/* ...0...0...0...1 */
		{ 0x28,0x08,0xa8,0x88 }, { 0xa0,0x80,0xa8,0x88 },	/* ...0...0...1...0 */
		{ 0xa0,0xa8,0x20,0x28 }, { 0xa0,0x80,0xa8,0x88 },	/* ...0...0...1...1 */
		{ 0x20,0x28,0x00,0x08 }, { 0x28,0x08,0xa8,0x88 },	/* ...0...1...0...0 */
		{ 0xa0,0xa8,0x20,0x28 }, { 0xa0,0xa8,0x20,0x28 },	/* ...0...1...0...1 */
		{ 0xa0,0x80,0xa8,0x88 }, { 0x28,0x08,0xa8,0x88 },	/* ...0...1...1...0 */
		{ 0xa0,0xa8,0x20,0x28 }, { 0x28,0x08,0xa8,0x88 },	/* ...0...1...1...1 */
		{ 0x80,0x00,0xa0,0x20 }, { 0x80,0x00,0xa0,0x20 },	/* ...1...0...0...0 */
		{ 0xa0,0x20,0xa8,0x28 }, { 0xa0,0xa8,0x20,0x28 },	/* ...1...0...0...1 */
		{ 0xa0,0x20,0xa8,0x28 }, { 0xa0,0x80,0xa8,0x88 },	/* ...1...0...1...0 */
		{ 0xa0,0x80,0xa8,0x88 }, { 0xa0,0x80,0xa8,0x88 },	/* ...1...0...1...1 */
		{ 0x80,0x00,0xa0,0x20 }, { 0x20,0x28,0x00,0x08 },	/* ...1...1...0...0 */
		{ 0xa0,0xa8,0x20,0x28 }, { 0xa0,0x20,0xa8,0x28 },	/* ...1...1...0...1 */
		{ 0x80,0x00,0xa0,0x20 }, { 0xa0,0x80,0xa8,0x88 },	/* ...1...1...1...0 */
		{ 0xa0,0xa8,0x20,0x28 }, { 0xa0,0x20,0xa8,0x28 }	/* ...1...1...1...1 */
	};

	sega_decode(convtable);
}

static void wmatch_decode(void)
{
	static const UINT8 convtable[32][4] =
	{
		/*       opcode                   data                     address      */
		/*  A    B    C    D         A    B    C    D                           */
		{ 0x88,0xa8,0x80,0xa0 }, { 0xa0,0x80,0x20,0x00 },	/* ...0...0...0...0 */
		{ 0x08,0x88,0x00,0x80 }, { 0x88,0xa8,0x80,0xa0 },	/* ...0...0...0...1 */
		{ 0x20,0x00,0xa0,0x80 }, { 0x20,0x28,0xa0,0xa8 },	/* ...0...0...1...0 */
		{ 0x20,0x28,0xa0,0xa8 }, { 0xa0,0x80,0x20,0x00 },	/* ...0...0...1...1 */
		{ 0xa8,0x28,0x88,0x08 }, { 0xa8,0x28,0x88,0x08 },	/* ...0...1...0...0 */
		{ 0x08,0x88,0x00,0x80 }, { 0xa8,0x28,0x88,0x08 },	/* ...0...1...0...1 */
		{ 0xa8,0x28,0x88,0x08 }, { 0x20,0x28,0xa0,0xa8 },	/* ...0...1...1...0 */
		{ 0xa8,0x28,0x88,0x08 }, { 0xa8,0x28,0x88,0x08 },	/* ...0...1...1...1 */
		{ 0x20,0x28,0xa0,0xa8 }, { 0x88,0xa8,0x80,0xa0 },	/* ...1...0...0...0 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0x20,0x28,0xa0,0xa8 },	/* ...1...0...0...1 */
		{ 0x20,0x28,0xa0,0xa8 }, { 0xa0,0x80,0x20,0x00 },	/* ...1...0...1...0 */
		{ 0x20,0x28,0xa0,0xa8 }, { 0x20,0x28,0xa0,0xa8 },	/* ...1...0...1...1 */
		{ 0x20,0x00,0xa0,0x80 }, { 0x20,0x28,0xa0,0xa8 },	/* ...1...1...0...0 */
		{ 0xa8,0x28,0x88,0x08 }, { 0xa0,0x80,0x20,0x00 },	/* ...1...1...0...1 */
		{ 0x20,0x28,0xa0,0xa8 }, { 0x20,0x28,0xa0,0xa8 },	/* ...1...1...1...0 */
		{ 0xa8,0x28,0x88,0x08 }, { 0xa8,0x28,0x88,0x08 }	/* ...1...1...1...1 */
	};

	sega_decode(convtable);
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

	sega_decode_2(System1Rom1, System1Fetch1, opcode_xor,opcode_swap_select,data_xor,data_swap_select);
}

static void fdwarrio_decode(void)
{
	static const UINT8 opcode_xor[64] =
	{
		0x40,0x50,0x44,0x54,0x41,0x51,0x45,0x55,
		0x40,0x50,0x44,0x54,0x41,0x51,0x45,0x55,
		0x40,0x50,0x44,0x54,0x41,0x51,0x45,0x55,
		0x40,0x50,0x44,0x54,0x41,0x51,0x45,0x55,
		0x40,0x50,0x44,0x54,0x41,0x51,0x45,0x55,
		0x40,0x50,0x44,0x54,0x41,0x51,0x45,0x55,
		0x40,0x50,0x44,0x54,0x41,0x51,0x45,0x55,
		0x40,0x50,0x44,0x54,0x41,0x51,0x45,0x55,
	};

	static const UINT8 data_xor[64] =
	{
		0x10,0x04,0x14,0x01,0x11,0x05,0x15,0x00,
		0x10,0x04,0x14,0x01,0x11,0x05,0x15,0x00,
		0x10,0x04,0x14,0x01,0x11,0x05,0x15,0x00,
		0x10,0x04,0x14,0x01,0x11,0x05,0x15,0x00,
		0x10,0x04,0x14,0x01,0x11,0x05,0x15,0x00,
		0x10,0x04,0x14,0x01,0x11,0x05,0x15,0x00,
		0x10,0x04,0x14,0x01,0x11,0x05,0x15,0x00,
		0x10,0x04,0x14,0x01,0x11,0x05,0x15,0x00,
	};

	static const INT32 opcode_swap_select[64] =
	{
		4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,
		6,6,6,6,6,6,6,6,7,7,7,7,7,7,7,7,
		8,8,8,8,8,8,8,8,9,9,9,9,9,9,9,9,
		10,10,10,10,10,10,10,10,11,11,11,11,11,11,11,11,
	};

	static const INT32 data_swap_select[64] =
	{
		  4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,
		6,6,6,6,6,6,6,6,7,7,7,7,7,7,7,7,
		8,8,8,8,8,8,8,8,9,9,9,9,9,9,9,9,
		10,10,10,10,10,10,10,10,11,11,11,11,11,11,11,11,
		12,
	};

	sega_decode_2(System1Rom1, System1Fetch1, opcode_xor,opcode_swap_select,data_xor,data_swap_select);
}

static void wboy2_decode(void)
{
	static const UINT8 opcode_xor[64] =
	{
		0x00,0x45,0x11,0x01,0x44,0x10,0x55,0x05,0x41,0x14,0x04,0x40,0x15,0x51,
		0x01,0x44,0x10,0x00,0x45,0x11,0x54,0x04,0x40,0x15,0x05,0x41,0x14,0x50,
		0x00,0x45,0x11,0x01,
		0x00,0x45,0x11,0x01,0x44,0x10,0x55,0x05,0x41,0x14,0x04,0x40,0x15,0x51,
		0x01,0x44,0x10,0x00,0x45,0x11,0x54,0x04,0x40,0x15,0x05,0x41,0x14,0x50,
		0x00,0x45,0x11,0x01,
	};

	static const UINT8 data_xor[64] =
	{
		0x55,0x05,0x41,0x14,0x50,0x00,0x15,0x51,0x01,0x44,0x10,0x55,0x05,0x11,
		0x54,0x04,0x40,0x15,0x51,0x01,0x14,0x50,0x00,0x45,0x11,0x54,0x04,0x10,
		0x55,0x05,0x41,0x14,
		0x55,0x05,0x41,0x14,0x50,0x00,0x15,0x51,0x01,0x44,0x10,0x55,0x05,0x11,
		0x54,0x04,0x40,0x15,0x51,0x01,0x14,0x50,0x00,0x45,0x11,0x54,0x04,0x10,
		0x55,0x05,0x41,0x14,
	};

	static const INT32 opcode_swap_select[64] =
	{
		2,
		5,1,5,1,5,
		0,4,0,4,0,4,
		7,3,7,3,7,3,
		6,2,6,2,6,
		1,5,1,5,1,5,
		0,4,0,

		10,
		13,9,13,9,13,
		8,12,8,12,8,12,
		15,11,15,11,15,11,
		14,10,14,10,14,
		9,13,9,13,9,13,
		8,12,8,
	};

	static const INT32 data_swap_select[64] =
	{
		3,7,3,7,3,7,
		2,6,2,6,2,
		5,1,5,1,5,1,
		4,0,4,0,4,
		8,
		3,7,3,7,3,
		6,2,6,2,

		11,15,11,15,11,15,
		10,14,10,14,10,
		13,9,13,9,13,9,
		12,8,12,8,12,
		16,
		11,15,11,15,11,
		14,10,14,10,
	};

	sega_decode_2(System1Rom1, System1Fetch1, opcode_xor,opcode_swap_select,data_xor,data_swap_select);
}

void sega_decode_317(UINT8 *pDest, UINT8 *pDestDec, INT32 order, INT32 opcode_shift, INT32 data_shift)
{
	static const UINT8 xor1_317[1+64] =
	{
		0x54,
		0x14,0x15,0x41,0x14,0x50,0x55,0x05,0x41,0x01,0x10,0x51,0x05,0x11,0x05,0x14,0x55,
		0x41,0x05,0x04,0x41,0x14,0x10,0x45,0x50,0x00,0x45,0x00,0x00,0x00,0x45,0x00,0x00,
		0x54,0x04,0x15,0x10,0x04,0x05,0x11,0x44,0x04,0x01,0x05,0x00,0x44,0x15,0x40,0x45,
		0x10,0x15,0x51,0x50,0x00,0x15,0x51,0x44,0x15,0x04,0x44,0x44,0x50,0x10,0x04,0x04,
	};

	static const UINT8 xor2_317[2+64] =
	{
		0x04,
		0x44,
		0x15,0x51,0x41,0x10,0x15,0x54,0x04,0x51,0x05,0x55,0x05,0x54,0x45,0x04,0x10,0x01,
		0x51,0x55,0x45,0x55,0x45,0x04,0x55,0x40,0x11,0x15,0x01,0x40,0x01,0x11,0x45,0x44,
		0x40,0x05,0x15,0x15,0x01,0x50,0x00,0x44,0x04,0x50,0x51,0x45,0x50,0x54,0x41,0x40,
		0x14,0x40,0x50,0x45,0x10,0x05,0x50,0x01,0x40,0x01,0x50,0x50,0x50,0x44,0x40,0x10,
	};

	static const INT32 swap1_317[1+64] =
	{
		 7,
		 1,11,23,17,23, 0,15,19,
		20,12,10, 0,18,18, 5,20,
		13, 0,18,14, 5, 6,10,21,
		 1,11, 9, 3,21, 4, 1,17,
		 5, 7,16,13,19,23,20, 2,
		10,23,23,15,10,12, 0,22,
		14, 6,15,11,17,15,21, 0,
		 6, 1, 1,18, 5,15,15,20,
	};

	static const INT32 swap2_317[2+64] =
	{
		 7,
		12,
		18, 8,21, 0,22,21,13,21,
		20,13,20,14, 6, 3, 5,20,
		 8,20, 4, 8,17,22, 0, 0,
		 6,17,17, 9, 0,16,13,21,
		 3, 2,18, 6,11, 3, 3,18,
		18,19, 3, 0, 5, 0,11, 8,
		 8, 1, 7, 2,10, 8,10, 2,
		 1, 3,12,16, 0,17,10, 1,
	};

	if (order)
		sega_decode_2( pDest, pDestDec, xor2_317+opcode_shift, swap2_317+opcode_shift, xor1_317+data_shift, swap1_317+data_shift );
	else
		sega_decode_2( pDest, pDestDec, xor1_317+opcode_shift, swap1_317+opcode_shift, xor2_317+data_shift, swap2_317+data_shift );
}

static void gardia_decode()
{
	sega_decode_317( System1Rom1, System1Fetch1, 1, 1, 1 );
}

static void gardiab_decode()
{
	sega_decode_317( System1Rom1, System1Fetch1, 0, 1, 2 );
}

static void blockgal_decode()
{
	mc8123_decrypt_rom(0, 0, System1Rom1, System1Fetch1, System1MC8123Key);
}

static void tokisens_decode()
{
	mc8123_decrypt_rom(1, 4, System1Rom1, System1Rom1 + 0x20000, System1MC8123Key);
}

static void wbml_decode() // & Ufosensi
{
	mc8123_decrypt_rom(1, 4, System1Rom1, System1Rom1 + 0x20000, System1MC8123Key);
}

static void wbmljb_decode()
{
	return; // fake decode function
}

static void myherok_tile_decode()
{
	INT32 A;
	UINT8 *rom = System1TempRom;

	/* the first ROM has data lines D0 and D6 swapped. */
	for (A = 0x0000;A < 0x4000;A++)
		rom[A] = (rom[A] & 0xbe) | ((rom[A] & 0x01) << 6) | ((rom[A] & 0x40) >> 6);

	/* the second ROM has data lines D1 and D5 swapped. */
	for (A = 0x4000;A < 0x8000;A++)
		rom[A] = (rom[A] & 0xdd) | ((rom[A] & 0x02) << 4) | ((rom[A] & 0x20) >> 4);

	/* the third ROM has data lines D0 and D6 swapped. */
	for (A = 0x8000;A < 0xc000;A++)
		rom[A] = (rom[A] & 0xbe) | ((rom[A] & 0x01) << 6) | ((rom[A] & 0x40) >> 6);

	/* also, all three ROMs have address lines A4 and A5 swapped. */
	for (A = 0;A < 0xc000;A++)
	{
		INT32 A1;
		UINT8 temp;

		A1 = (A & 0xffcf) | ((A & 0x0010) << 1) | ((A & 0x0020) >> 1);
		if (A < A1)
		{
			temp = rom[A];
			rom[A] = rom[A1];
			rom[A1] = temp;
		}
	}
}

/*==============================================================================================
Allocate Memory
===============================================================================================*/

static INT32 MemIndex()
{
	UINT8 *Next; Next = Mem;

	System1Rom1            = Next; Next += 0x040000;
	System1Fetch1          = Next; Next += 0x040000;
	System1Rom2            = Next; Next += 0x010000;
	DrvMCURom              = Next; Next += 0x010000;
	System1PromRed         = Next; Next += 0x000100;
	System1PromGreen       = Next; Next += 0x000100;
	System1PromBlue        = Next; Next += 0x000100;

	RamStart = Next;

	System1Ram1            = Next; Next += 0x004100;
	System1Ram2            = Next; Next += 0x000800;
	System1SpriteRam       = Next; Next += 0x000800;
	System1PaletteRam      = Next; Next += 0x000800;
	System1VideoRam        = Next; Next += 0x004000;

	System1BgRam           = System1VideoRam + 0x0000;
	System1FgRam           = System1VideoRam + 0x0800;

	System1ScrollXRam      = System1VideoRam + 0x7C0;
	System1BgCollisionRam  = Next; Next += 0x000400;
	System1SprCollisionRam = Next; Next += 0x000400;
	System1f4Ram           = Next; Next += 0x000400;
	System1fcRam           = Next; Next += 0x000400;
	SpriteOnScreenMap      = Next; Next += (((wide_mode) ? 512 : 256) * 256);

	RamEnd = Next;

	System1Sprites         = Next; Next += System1SpriteRomSize;
	System1Tiles           = Next; Next += (System1NumTiles * ((wide_mode) ? 16 : 8) * 8);
	System1TilesPenUsage   = (UINT32*)Next; Next += System1NumTiles * sizeof(UINT32);
	System1Palette         = (UINT32*)Next; Next += 0x000800 * sizeof(UINT32);
	MemEnd = Next;

	return 0;
}

/*==============================================================================================
Reset Functions
===============================================================================================*/

static INT32 System1DoReset()
{
	if (IsSystem2 || Sys1UsePPI) {
		ppi8255_reset();
	}

	memset (RamStart, 0, RamEnd - RamStart);

    ZetReset(0);
	ZetReset(1);
	if (has_mcu) {
		DrvMCUReset();
	}

	SN76496Reset();

	System1ScrollX[0] = System1ScrollX[1] = System1ScrollY = 0;
	System1BgScrollX = 0;
	System1BgScrollY = 0;
	System1VideoMode = 0;
	System1FlipScreen = 0;
	System1SoundLatch = 0;
	System1RomBank = 0;
	System1BankSwitch = 0;
	System1BgBankLatch = 0;
	System1BgBank = 0;
	NoboranbInp16Step = 0;
	NoboranbInp17Step = 0;
	NoboranbInp23Step = 0;

	nCyclesExtra[0] = nCyclesExtra[1] = nCyclesExtra[2] = 0;

	HiscoreReset();

	return 0;
}

/*==============================================================================================
Memory Handlers
===============================================================================================*/

static void System1BankRom()
{
	INT32 BankAddress = (System1RomBank * 0x4000) + 0x10000;

	ZetMapArea(0x8000, 0xbfff, 0, System1Rom1 + BankAddress);

	if (DecodeFunction && IsSystem2)
	{
		ZetMapArea(0x8000, 0xbfff, 2, System1Rom1 + BankAddress + 0x20000, System1Rom1 + BankAddress);
	}
	else
	{
		ZetMapArea(0x8000, 0xbfff, 2, System1Rom1 + BankAddress);
	}
}

static inline void System2_bankswitch_w(UINT8 d)
{
	System1RomBank = (d & 0x0c) >> 2;
	System1BankRom();
	System1BankSwitch = d;
}

static inline void System2_videoram_bank_latch_w(UINT8 d)
{
	System1BgBankLatch = d;
	System1BgBank = (d >> 1) & 0x03;

	// iq_132
	ZetMapMemory(System1VideoRam + System1BgBank * 0x1000, 0xe000, 0xefff, MAP_RAM);
}

static inline void __fastcall System1SoundLatchWrite(UINT8 d)
{
	INT32 cyc = ZetTotalCycles(0) - ZetTotalCycles(1);
	if (cyc > 0) {
		ZetRun(1, cyc);
	}
	System1SoundLatch = d;
	ZetNmi(1);
}

static UINT8 __fastcall System1Z801PortRead(UINT16 a)
{
	a &= 0xff;

	switch (a) {
		case 0x00: {
			return System1Input[0];
		}

		case 0x04: {
			return System1Input[1];
		}

		case 0x08: {
			return System1Input[2];
		}

		case 0x0c: {
			return System1Dip[0];
		}

		case 0x0d: {
			return System1Dip[1];
		}

		case 0x10: {
			return System1Dip[1];
		}

		case 0x11: {
			return System1Dip[0];
		}

		case 0x15: {
			return System1VideoMode;
		}

		case 0x19: {
			return System1VideoMode;
		}
	}

	//bprintf(PRINT_NORMAL, _T("IO Read %x\n"), a);
	return 0;
}

static UINT8 __fastcall NoboranbZ801PortRead(UINT16 a)
{
	a &= 0xff;

	switch (a) {
		case 0x00: {
			return System1Input[0];
		}

		case 0x04: {
			return System1Input[1];
		}

		case 0x08: {
			return System1Input[2];
		}

		case 0x0c: {
			return System1Dip[0];
		}

		case 0x0d: {
			return System1Dip[1];
		}

		case 0x18: {
			if (has_mcu) {
				DrvMCUSync();
			}
			return nob_cpu_latch;
		}

		case 0x1c: {
			if (has_mcu) {
				DrvMCUSync();
				return nob_mcu_status;
			}
			return 0x80;
		}

		case 0x15: {
			return System1VideoMode;
		}

		case 0x16: {
			return NoboranbInp16Step;
		}

		case 0x22: {
			return NoboranbInp17Step;
		}

		case 0x23: {
			return NoboranbInp23Step;
		}
	}

	//bprintf(PRINT_NORMAL, _T("IO Read %x\n"), a);
	return 0;
}

static void __fastcall System1Z801PortWrite(UINT16 a, UINT8 d)
{
	if (Sys1UsePPI) {
		a &= 0x1f;
		switch (a)
		{
			case 0x14:
			case 0x15:
			case 0x16:
			case 0x17:
				ppi8255_w(0, a & 3, d);
				return;
		}
		return;
	}

	a &= 0xff;

	switch (a) {
		case 0x14:
		case 0x18:
			System1SoundLatchWrite(d);
			return;

		case 0x15:
		case 0x19: {
			System1VideoMode = d;
			System1FlipScreen = d & 0x80;
			return;
		}
		case 0x1c: return; // NOP
	}

	//bprintf(PRINT_NORMAL, _T("IO Write %x, %x\n"), a, d);
}

// mcu stuff (choplift, shtngmst)

static INT32 from_mcu = 0; // avoid recursion when mcu r/w ppi through z80's IO map

static UINT8 mcu_read_port(INT32 port)
{
	if (port >= 0x0000 && port <= 0xffff) {
		switch (i8751Command & 0x18) {
			case 0x00: return ZetReadByte(port);
			case 0x08: return System1Rom1[0x10000 + port];
			case 0x10: {
				from_mcu = 1;
				UINT8 f = ZetReadIO(port);
				from_mcu = 0;
				return f;
			}
		}
	}

	return 0xff;
}


static void mcu_write_port(INT32 port, UINT8 data)
{
	if (port >= 0x0000 && port <= 0xffff) {
		switch (i8751Command & 0x18) {
			case 0x00: ZetWriteByte(port, data); return;
			case 0x10: from_mcu = 1; ZetWriteIO(port, data); from_mcu = 0; return;
		}
	}

	if (port == MCS51_PORT_P1) {
		i8751Command = data;

		ZetSetHALT(0, (data & 0x40) >> 6);
		if (~data & 0x01) ZetSetIRQLine(0, 0, CPU_IRQSTATUS_HOLD);
		return;
	}
}

// Noboranka MCU (Dataeast Style)
static UINT8 nob_mcu_read_port(INT32 port)
{
	if (port == MCS51_PORT_P0) {
		return nob_mcu_latch;
	}

	return 0xff;
}

static void nob_mcu_write_port(INT32 port, UINT8 data)
{
	switch (port) {
		case MCS51_PORT_P0:
			nob_mcu_latch = data;
			break;
		case MCS51_PORT_P1:
			nob_mcu_status = data;
			break;
		case MCS51_PORT_P2:
			if ((i8751Command ^ data) & 1 && (~data & 1)) {
				nob_mcu_latch = nob_cpu_latch;
			}
			if ((i8751Command ^ data) & 2 && (~data & 2)) {
				nob_cpu_latch = nob_mcu_latch;
			}
			if ((i8751Command ^ data) & 4 && (~data & 4)) {
				mcs51_set_irq_line(MCS51_INT0_LINE, CPU_IRQSTATUS_NONE);
			}

			i8751Command = data;
			break;
	}
}

static dtimer mcu_t0;

static void t0_cb(INT32 cb)
{
	mcs51_set_irq_line(MCS51_T0_LINE, CPU_IRQSTATUS_ACK);
	mcs51_set_irq_line(MCS51_T0_LINE, CPU_IRQSTATUS_NONE);
}

static void DrvMCUInit()
{
	mcs51_init();
	mcs51_set_program_data(DrvMCURom);
	mcs51_set_write_handler((is_nob) ? nob_mcu_write_port : mcu_write_port);
	mcs51_set_read_handler((is_nob) ? nob_mcu_read_port : mcu_read_port);

	timerInit();
	if (is_nob == 0) {
		timerAdd(mcu_t0, 0, t0_cb);
		mcu_t0.start(usec_to_cycles(8000000/12, 2500), 0, 1, 1);
	}

	DrvMCUReset();
}

static void DrvMCUExit() {
	mcs51_exit();
}

static void DrvMCUNewFrame()
{
	timerNewFrame();
	mcs51NewFrame();
}

static INT32 DrvMCURun(INT32 cycles)
{
	INT32 cyc = mcs51Run(cycles);
	timerRun(cyc);
	return cyc;
}

static INT32 DrvMCUIdle(INT32 cycles)
{
	INT32 cyc = mcs51Idle(cycles);
	timerIdle(cyc);
	return cyc;
}

static INT32 DrvMCUTotalCycles()
{
	return mcs51TotalCycles();
}

static INT32 DrvMCUScan(INT32 nAction)
{
	mcs51_scan(nAction);
	timerScan();

	SCAN_VAR(i8751Command);
	if (is_nob) {
		SCAN_VAR(nob_cpu_latch);
		SCAN_VAR(nob_mcu_latch);
		SCAN_VAR(nob_mcu_status);
	}
	return 0;
}

static void DrvMCUVblank()
{
	if (is_nob == 0) {
		mcs51_set_irq_line(MCS51_INT0_LINE, CPU_IRQSTATUS_ACK);
		mcs51_set_irq_line(MCS51_INT0_LINE, CPU_IRQSTATUS_NONE);
	}
}

static void DrvMCUSync()
{
	INT32 todo = ((double)ZetTotalCycles(0) * (8000000/12) / 4000000) - mcs51TotalCycles();

	if (todo > 0) {
		DrvMCURun(todo);
	}
}

static void DrvMCUReset()
{
	i8751Command = 0;
	nob_cpu_latch = 0;
	nob_mcu_latch = 0;
	nob_mcu_status = 0;
	mcs51Open(0);
	mcs51_reset();
	mcs51Close();

	timerReset();
}

static void __fastcall BrainZ801PortWrite(UINT16 a, UINT8 d)
{
	a &= 0xff;

	switch (a) {
		case 0x14:
		case 0x18:
			System1SoundLatchWrite(d);
			return;

		case 0x15:
		case 0x19: {
			System1VideoMode = d;
			System1FlipScreen = d & 0x80;

			System1RomBank = ((d & 0x04) >> 2) + ((d & 0x40) >> 5);
			System1BankRom();
			return;
		}
	}

	//bprintf(PRINT_NORMAL, _T("IO Write %x, %x\n"), a, d);
}

static void __fastcall NoboranbZ801PortWrite(UINT16 a, UINT8 d)
{
	a &= 0xff;

	switch (a) {
		case 0x14:
			System1SoundLatchWrite(d);
			return;

		case 0x15: {
			System1VideoMode = d;
			System1FlipScreen = d & 0x80;

			System1RomBank = ((d & 0x04) >> 2) + ((d & 0x40) >> 5);
			System1BankRom();
			return;
		}

		case 0x16: {
			NoboranbInp16Step = d;
			return;
		}

		case 0x17: {
			NoboranbInp17Step = d;
			return;
		}

		case 0x18: {
			if (has_mcu) {
				DrvMCUSync();
				nob_cpu_latch = d;
				mcs51_set_irq_line(MCS51_INT0_LINE, CPU_IRQSTATUS_ACK);
			}
			return;
		}

		case 0x24: {
			NoboranbInp23Step = d;
			return;
		}
	}

	//bprintf(PRINT_NORMAL, _T("IO Write %x, %x\n"), a, d);
}

static void __fastcall System1Z801ProgWrite(UINT16 a, UINT8 d)
{
	if (a >= 0xf000 && a <= 0xf3ff) { System1BgCollisionRam[a & 0x3ff] = 0x7e; return; }
	if (a >= 0xf800 && a <= 0xfbff) { System1SprCollisionRam[a & 0x3ff] = 0x7e; return; }

//	//bprintf(PRINT_NORMAL, _T("Prog Write %x, %x\n"), a, d);
}

static void __fastcall NoboranbZ801ProgWrite(UINT16 a, UINT8 d)
{
	if (a >= 0xc000 && a <= 0xc3ff) { System1BgCollisionRam[a & 0x3ff] = 0x7e; return; }
	if (a >= 0xc800 && a <= 0xcbff) { System1SprCollisionRam[a & 0x3ff] = 0x7e; return; }

	bprintf(PRINT_NORMAL, _T("Prog Write %x, %x\n"), a, d);
}


static UINT8 __fastcall System1Z802ProgRead(UINT16 a)
{
	switch (a & ~0x1fff) {
		case 0xe000: {
			return System1SoundLatch;
		}
	}

	bprintf(PRINT_NORMAL, _T("Z80 2 Prog Read %x\tPC:  %x\n"), a, ZetGetPrevPC(-1));
	return 0;
}

static void __fastcall System1Z802ProgWrite(UINT16 a, UINT8 d)
{
	switch (a & ~0x1fff) {
		case 0xa000: {
			SN76496Write(0, d);
			return;
		}

		case 0xc000: {
			SN76496Write(1, d);
			return;
		}
	}

	bprintf(PRINT_NORMAL, _T("Z80 2 Prog Write %x, %x\tPC:  %x\n"), a, d, ZetGetPrevPC(-1));
}

static void System2PPI0WriteA(UINT8 data)
{
	System1SoundLatchWrite(data);
}

static void System2PPI0WriteB(UINT8 data)
{
	System2_bankswitch_w(data);
	System1VideoMode = data;

	if (has_mcu) {
		if (from_mcu == 0) DrvMCUSync(); // don't let mcu derp itself
		mcs51_set_irq_line(MCS51_INT1_LINE, (~data & 0x40) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
	}
}

static void System2PPI0WriteC(UINT8 data)
{
	ZetSetIRQLine(1, 0x20, (data & 0x80) ? CPU_IRQSTATUS_NONE : CPU_IRQSTATUS_ACK);

	if (!Sys1UsePPI)
		System2_videoram_bank_latch_w(data);
}

static UINT8 __fastcall System2Z801PortRead(UINT16 a)
{
	a &= 0x1f;
	switch (a & ~3) {
		case 0x00: return System1Input[0];
		case 0x04: return System1Input[1];
		case 0x08: return System1Input[2];
		case 0x10: return System1Dip[1];
	}
	switch (a & ~2) {
		case 0x0c: return System1Dip[0];
		case 0x0d: return System1Dip[1];
	}
	switch (a) {
		case 0x14:
		case 0x15:
		case 0x16:
		case 0x17: return ppi8255_r(0, a & 3);
	}

	return 0;
}

static UINT8 __fastcall ShtngmstZ801PortRead(UINT16 a)
{
	a &= 0x1f;

	switch (a) {
		case 0x12:
			return System1Input[3];
		case 0x18:
		case 0x19:
		case 0x1a:
		case 0x1b:
			return System1Dip[2]; // mysterious port 18
		case 0x1c:
		case 0x1e: {
			return BurnGunReturnX(0) - 16;
		}
		case 0x1d:
		case 0x1f: {
			UINT8 y = 0xff - BurnGunReturnY(0);
			y = scalerange(y, 0, 0xff, 0x20, 0xff);
			return y;
		}
	}

	return System2Z801PortRead(a);
}


static void __fastcall System2Z801PortWrite(UINT16 a, UINT8 d)
{
	a &= 0x1f;
	switch (a)
	{
		case 0x14:
		case 0x15:
		case 0x16:
		case 0x17:
			ppi8255_w(0, a & 3, d);
		return;
	}
}

/*==============================================================================================
Driver Inits
===============================================================================================*/

static void CalcPenUsage()
{
	INT32 i, x, y;
	UINT32 Usage;
	UINT8 *dp = NULL;

	for (i = 0; i < System1NumTiles; i++) {
		dp = System1Tiles + (i * ((wide_mode) ? 128 : 64)); // 16x8, 8x8
		Usage = 0;
		for (y = 0; y < 8; y++) {
			for (x = 0; x < ((wide_mode) ? 16 : 8); x++) {
				Usage |= 1 << dp[x];
			}

			dp += ((wide_mode) ? 16 : 8);
		}

		System1TilesPenUsage[i] = Usage;
	}
}

static INT32 TileXOffsets[8]      = { 0, 1, 2, 3, 4, 5, 6, 7 };
static INT32 TileXOffsetsWide[16] = { 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7 };
static INT32 TileYOffsets[8]      = { 0, 8, 16, 24, 32, 40, 48, 56 };
static const UINT8 cc_op[0x100] = {
	4*5+1*2,10*5+3*2, 7*5+1*2, 6*5+1*2, 4*5+1*2, 4*5+1*2, 7*5+2*2, 4*5+1*2, 4*5+1*2,11*5+1*2, 7*5+1*2, 6*5+1*2, 4*5+1*2, 4*5+1*2, 7*5+2*2, 4*5+1*2,
	8*5+2*2,10*5+3*2, 7*5+1*2, 6*5+1*2, 4*5+1*2, 4*5+1*2, 7*5+2*2, 4*5+1*2,12*5+2*2,11*5+1*2, 7*5+1*2, 6*5+1*2, 4*5+1*2, 4*5+1*2, 7*5+2*2, 4*5+1*2,
	7*5+2*2,10*5+3*2,16*5+3*2, 6*5+1*2, 4*5+1*2, 4*5+1*2, 7*5+2*2, 4*5+1*2, 7*5+2*2,11*5+1*2,16*5+3*2, 6*5+1*2, 4*5+1*2, 4*5+1*2, 7*5+2*2, 4*5+1*2,
	7*5+2*2,10*5+3*2,13*5+3*2, 6*5+1*2,11*5+1*2,11*5+1*2,10*5+2*2, 4*5+1*2, 7*5+2*2,11*5+1*2,13*5+3*2, 6*5+1*2, 4*5+1*2, 4*5+1*2, 7*5+2*2, 4*5+1*2,
	4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 7*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 7*5+1*2, 4*5+1*2,
	4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 7*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 7*5+1*2, 4*5+1*2,
	4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 7*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 7*5+1*2, 4*5+1*2,
	7*5+1*2, 7*5+1*2, 7*5+1*2, 7*5+1*2, 7*5+1*2, 7*5+1*2, 4*5+1*2, 7*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 7*5+1*2, 4*5+1*2,
	4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 7*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 7*5+1*2, 4*5+1*2,
	4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 7*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 7*5+1*2, 4*5+1*2,
	4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 7*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 7*5+1*2, 4*5+1*2,
	4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 7*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 4*5+1*2, 7*5+1*2, 4*5+1*2,
	5*5+1*2,10*5+1*2,10*5+3*2,10*5+3*2,10*5+3*2,11*5+1*2, 7*5+2*2,11*5+1*2, 5*5+1*2,10*5+1*2,10*5+3*2, 4*5+1*2,10*5+3*2,17*5+3*2, 7*5+2*2,11*5+1*2,
	5*5+1*2,10*5+1*2,10*5+3*2,11*5+2*2,10*5+3*2,11*5+1*2, 7*5+2*2,11*5+1*2, 5*5+1*2, 4*5+1*2,10*5+3*2,11*5+2*2,10*5+3*2, 4*5+1*2, 7*5+2*2,11*5+1*2,
	5*5+1*2,10*5+1*2,10*5+3*2,19*5+1*2,10*5+3*2,11*5+1*2, 7*5+2*2,11*5+1*2, 5*5+1*2, 4*5+1*2,10*5+3*2, 4*5+1*2,10*5+3*2, 4*5+1*2, 7*5+2*2,11*5+1*2,
	5*5+1*2,10*5+1*2,10*5+3*2, 4*5+1*2,10*5+3*2,11*5+1*2, 7*5+2*2,11*5+1*2, 5*5+1*2, 6*5+1*2,10*5+3*2, 4*5+1*2,10*5+3*2, 4*5+1*2, 7*5+2*2,11*5+1*2
};

static const UINT8 cc_cb[0x100] = {
	8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2,15*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2,15*5+2*2, 8*5+2*2,
	8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2,15*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2,15*5+2*2, 8*5+2*2,
	8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2,15*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2,15*5+2*2, 8*5+2*2,
	8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2,15*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2,15*5+2*2, 8*5+2*2,
	8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2,12*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2,12*5+2*2, 8*5+2*2,
	8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2,12*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2,12*5+2*2, 8*5+2*2,
	8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2,12*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2,12*5+2*2, 8*5+2*2,
	8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2,12*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2,12*5+2*2, 8*5+2*2,
	8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2,15*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2,15*5+2*2, 8*5+2*2,
	8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2,15*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2,15*5+2*2, 8*5+2*2,
	8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2,15*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2,15*5+2*2, 8*5+2*2,
	8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2,15*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2,15*5+2*2, 8*5+2*2,
	8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2,15*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2,15*5+2*2, 8*5+2*2,
	8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2,15*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2,15*5+2*2, 8*5+2*2,
	8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2,15*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2,15*5+2*2, 8*5+2*2,
	8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2,15*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2,15*5+2*2, 8*5+2*2
};

static const UINT8 cc_ed[0x100] = {
	8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2,
	8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2,
	8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2,
	8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2,
12*5+2*2,12*5+2*2,15*5+2*2,20*5+4*2, 8*5+2*2,14*5+2*2, 8*5+2*2, 9*5+2*2,12*5+2*2,12*5+2*2,15*5+2*2,20*5+4*2, 8*5+2*2,14*5+2*2, 8*5+2*2, 9*5+2*2,
12*5+2*2,12*5+2*2,15*5+2*2,20*5+4*2, 8*5+2*2,14*5+2*2, 8*5+2*2, 9*5+2*2,12*5+2*2,12*5+2*2,15*5+2*2,20*5+4*2, 8*5+2*2,14*5+2*2, 8*5+2*2, 9*5+2*2,
12*5+2*2,12*5+2*2,15*5+2*2,20*5+4*2, 8*5+2*2,14*5+2*2, 8*5+2*2,18*5+2*2,12*5+2*2,12*5+2*2,15*5+2*2,20*5+4*2, 8*5+2*2,14*5+2*2, 8*5+2*2,18*5+2*2,
12*5+2*2,12*5+2*2,15*5+2*2,20*5+4*2, 8*5+2*2,14*5+2*2, 8*5+2*2, 8*5+2*2,12*5+2*2,12*5+2*2,15*5+2*2,20*5+4*2, 8*5+2*2,14*5+2*2, 8*5+2*2, 8*5+2*2,
	8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2,
	8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2,
16*5+2*2,16*5+2*2,16*5+2*2,16*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2,16*5+2*2,16*5+2*2,16*5+2*2,16*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2,
16*5+2*2,16*5+2*2,16*5+2*2,16*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2,16*5+2*2,16*5+2*2,16*5+2*2,16*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2,
	8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2,
	8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2,
	8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2,
	8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2, 8*5+2*2
};

static const UINT8 cc_xy[0x100] = {
( 4+4)*5+2*2,(10+4)*5+4*2,( 7+4)*5+2*2,( 6+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 7+4)*5+3*2,( 4+4)*5+2*2,( 4+4)*5+2*2,(11+4)*5+2*2,( 7+4)*5+2*2,( 6+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 7+4)*5+3*2,( 4+4)*5+2*2,
( 8+4)*5+3*2,(10+4)*5+4*2,( 7+4)*5+2*2,( 6+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 7+4)*5+3*2,( 4+4)*5+2*2,(12+4)*5+3*2,(11+4)*5+2*2,( 7+4)*5+2*2,( 6+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 7+4)*5+3*2,( 4+4)*5+2*2,
( 7+4)*5+3*2,(10+4)*5+4*2,(16+4)*5+4*2,( 6+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 7+4)*5+3*2,( 4+4)*5+2*2,( 7+4)*5+3*2,(11+4)*5+2*2,(16+4)*5+4*2,( 6+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 7+4)*5+3*2,( 4+4)*5+2*2,
( 7+4)*5+3*2,(10+4)*5+4*2,(13+4)*5+4*2,( 6+4)*5+2*2,(23  )*5+3*2,(23  )*5+3*2,(19  )*5+4*2,( 4+4)*5+2*2,( 7+4)*5+3*2,(11+4)*5+2*2,(13+4)*5+4*2,( 6+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 7+4)*5+3*2,( 4+4)*5+2*2,
( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,(19  )*5+3*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,(19  )*5+3*2,( 4+4)*5+2*2,
( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,(19  )*5+3*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,(19  )*5+3*2,( 4+4)*5+2*2,
( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,(19  )*5+3*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,(19  )*5+3*2,( 4+4)*5+2*2,
(19  )*5+3*2,(19  )*5+3*2,(19  )*5+3*2,(19  )*5+3*2,(19  )*5+3*2,(19  )*5+3*2,( 4+4)*5+2*2,(19  )*5+3*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,(19  )*5+3*2,( 4+4)*5+2*2,
( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,(19  )*5+3*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,(19  )*5+3*2,( 4+4)*5+2*2,
( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,(19  )*5+3*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,(19  )*5+3*2,( 4+4)*5+2*2,
( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,(19  )*5+3*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,(19  )*5+3*2,( 4+4)*5+2*2,
( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,(19  )*5+3*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,( 4+4)*5+2*2,(19  )*5+3*2,( 4+4)*5+2*2,
( 5+4)*5+2*2,(10+4)*5+2*2,(10+4)*5+4*2,(10+4)*5+4*2,(10+4)*5+4*2,(11+4)*5+2*2,( 7+4)*5+3*2,(11+4)*5+2*2,( 5+4)*5+2*2,(10+4)*5+2*2,(10+4)*5+4*2,( 0  )*5    ,(10+4)*5+4*2,(17+4)*5+4*2,( 7+4)*5+3*2,(11+4)*5+2*2,
( 5+4)*5+2*2,(10+4)*5+2*2,(10+4)*5+4*2,(11+4)*5+3*2,(10+4)*5+4*2,(11+4)*5+2*2,( 7+4)*5+3*2,(11+4)*5+2*2,( 5+4)*5+2*2,( 4+4)*5+2*2,(10+4)*5+4*2,(11+4)*5+3*2,(10+4)*5+4*2,( 4  )*5+2*2,( 7+4)*5+3*2,(11+4)*5+2*2,
( 5+4)*5+2*2,(10+4)*5+2*2,(10+4)*5+4*2,(19+4)*5+2*2,(10+4)*5+4*2,(11+4)*5+2*2,( 7+4)*5+3*2,(11+4)*5+2*2,( 5+4)*5+2*2,( 4+4)*5+2*2,(10+4)*5+4*2,( 4+4)*5+2*2,(10+4)*5+4*2,( 4  )*5+2*2,( 7+4)*5+3*2,(11+4)*5+2*2,
( 5+4)*5+2*2,(10+4)*5+2*2,(10+4)*5+4*2,( 4+4)*5+2*2,(10+4)*5+4*2,(11+4)*5+2*2,( 7+4)*5+3*2,(11+4)*5+2*2,( 5+4)*5+2*2,( 6+4)*5+2*2,(10+4)*5+4*2,( 4+4)*5+2*2,(10+4)*5+4*2,( 4  )*5+2*2,( 7+4)*5+3*2,(11+4)*5+2*2
};

static const UINT8 cc_xycb[0x100] = {
23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,
23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,
23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,
23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,
20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,
20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,
20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,
20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,20*5+4*2,
23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,
23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,
23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,
23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,
23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,
23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,
23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,
23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2,23*5+4*2
};

/* extra cycles if jr/jp/call taken and 'interrupt latency' on rst 0-7 */
static const UINT8 cc_ex[0x100] = {
	0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5,
	5*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, /* DJNZ */
	5*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 5*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, /* JR NZ/JR Z */
	5*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 5*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, /* JR NC/JR C */
	0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5,
	0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5,
	0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5,
	0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5,
	0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5,
	0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5,
	0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5, 0*5,
	5*5, 5*5, 5*5, 5*5, 0*5, 0*5, 0*5, 0*5, 5*5, 5*5, 5*5, 5*5, 0*5, 0*5, 0*5, 0*5, /* LDIR/CPIR/INIR/OTIR LDDR/CPDR/INDR/OTDR */
	6*5, 0*5, 0*5, 0*5, 7*5, 0*5, 0*5, 2*5, 6*5, 0*5, 0*5, 0*5, 7*5, 0*5, 0*5, 2*5,
	6*5, 0*5, 0*5, 0*5, 7*5, 0*5, 0*5, 2*5, 6*5, 0*5, 0*5, 0*5, 7*5, 0*5, 0*5, 2*5,
	6*5, 0*5, 0*5, 0*5, 7*5, 0*5, 0*5, 2*5, 6*5, 0*5, 0*5, 0*5, 7*5, 0*5, 0*5, 2*5,
	6*5, 0*5, 0*5, 0*5, 7*5, 0*5, 0*5, 2*5, 6*5, 0*5, 0*5, 0*5, 7*5, 0*5, 0*5, 2*5
};

static INT32 System1Init(INT32 nZ80Rom1Num, INT32 nZ80Rom1Size, INT32 nZ80Rom2Num, INT32 nZ80Rom2Size, INT32 nTileRomNum, INT32 nTileRomSize, INT32 nSpriteRomNum, INT32 nSpriteRomSize, bool bReset)
{
	INT32 TilePlaneOffsets[3]  = { RGN_FRAC((nTileRomSize * nTileRomNum), 0, 3), RGN_FRAC((nTileRomSize * nTileRomNum), 1, 3), RGN_FRAC((nTileRomSize * nTileRomNum), 2, 3) };
	INT32 nRet = 0, nLen, i, RomOffset;
	struct BurnRomInfo ri;

	System1NumTiles = (((nTileRomNum * nTileRomSize) / 3) * 8) / (8 * 8);
	System1SpriteRomSize = (nSpriteRomNum + is_shtngmst) * nSpriteRomSize;

	// Allocate and Blank all required memory
	Mem = NULL;
	MemIndex();
	nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(Mem, 0, nLen);
	MemIndex();

	System1TempRom = (UINT8*)BurnMalloc(0x40000);

	// Load Z80 #1 Program roms
	RomOffset = 0;
	for (i = 0; i < nZ80Rom1Num; i++) {
		nRet = BurnLoadRom(System1Rom1 + (i * nZ80Rom1Size), i + RomOffset, 1); if (nRet != 0) return 1;
		BurnDrvGetRomInfo(&ri, i);
	}
	RomOffset += nZ80Rom1Num;

	if (System1BankedRom) {
		memcpy(System1TempRom, System1Rom1, 0x40000);
		memset(System1Rom1, 0, 0x40000);

		if (System1BankedRom == 1) {
			// Encrypted, banked
			memcpy(System1Rom1 + 0x00000, System1TempRom + 0x00000, 0x8000);
			memcpy(System1Rom1 + 0x10000, System1TempRom + 0x08000, 0x8000);
			memcpy(System1Rom1 + 0x18000, System1TempRom + 0x10000, 0x8000);
		}

		if (System1BankedRom == 2) {
			// last rom half the size, reload it into the last slot
			UINT32 nOffset = ((UINT32)nZ80Rom1Size == (ri.nLen * 2)) ? 0x20000 : 0x28000;

			// Unencrypted, banked
			memcpy(System1Rom1 + 0x20000, System1TempRom + 0x00000, 0x8000);
			memcpy(System1Rom1 + 0x00000, System1TempRom + 0x08000, 0x8000);
			memcpy(System1Rom1 + 0x30000, System1TempRom + 0x10000, 0x8000);//fetch
			memcpy(System1Rom1 + 0x10000, System1TempRom + 0x18000, 0x8000);
			memcpy(System1Rom1 + 0x38000, System1TempRom + 0x20000, 0x8000);//fetch
			memcpy(System1Rom1 + 0x18000, System1TempRom + nOffset, 0x8000);
		}
	}

	memset(System1Rom2, 0, 0x10000);

	if (DecodeFunction) DecodeFunction();

	// Load Z80 #2 Program roms
	for (i = 0; i < nZ80Rom2Num; i++) {
		nRet = BurnLoadRom(System1Rom2 + (i * nZ80Rom2Size), i + RomOffset, 1); if (nRet != 0) return 1;
	}
	RomOffset += nZ80Rom2Num;

	if (has_mcu) {
		bprintf(0, _T("Loading MCU @ %d\n"), RomOffset);
		nRet = BurnLoadRom(DrvMCURom, RomOffset, 1);
		RomOffset++; // account for mcu
	}

	// Load and decode tiles
	memset(System1TempRom, 0, 0x20000);
	for (i = 0; i < nTileRomNum; i++) {
		nRet = BurnLoadRom(System1TempRom + (i * nTileRomSize), i + RomOffset, 1);
	}
	RomOffset += nTileRomNum;

	if (TileDecodeFunction) TileDecodeFunction();

	if (wide_mode) {
		GfxDecode(System1NumTiles, 3, 16, 8, TilePlaneOffsets, TileXOffsetsWide, TileYOffsets, 0x40, System1TempRom, System1Tiles);
	} else {
		GfxDecode(System1NumTiles, 3, 8, 8, TilePlaneOffsets, TileXOffsets, TileYOffsets, 0x40, System1TempRom, System1Tiles);
	}

	CalcPenUsage();
	BurnFree(System1TempRom);

	// Load Sprite roms
	if (is_shtngmst) memset(System1Sprites, 0xff, System1SpriteRomSize);
	for (i = 0; i < nSpriteRomNum; i++) {
		nRet = BurnLoadRom(System1Sprites + (i * nSpriteRomSize), i + RomOffset, 1);
	}
	RomOffset += nSpriteRomNum;

	// Load Colour proms
	if (System1ColourProms) {
		nRet = BurnLoadRom(System1PromRed, 0 + RomOffset, 1);
		nRet = BurnLoadRom(System1PromGreen, 1 + RomOffset, 1);
		nRet = BurnLoadRom(System1PromBlue, 2 + RomOffset, 1);
		RomOffset += 3; // add color-prom count
	}

	// Setup the Z80 emulation
	ZetInit(0);
	ZetOpen(0);
	z80_set_cycle_tables(&cc_op[0], &cc_cb[0], &cc_ed[0], &cc_xy[0], &cc_xycb[0], &cc_ex[0]);
	if (IsSystem2) {
		ZetSetWriteHandler(System1Z801ProgWrite);
		ZetSetInHandler(System2Z801PortRead);
		ZetSetOutHandler(System2Z801PortWrite);

		ZetMapMemory(System1Rom1,			0x0000, 0x7fff, MAP_ROM);
		ZetMapMemory(System1Rom1 + 0x8000,	0x8000, 0xbfff, MAP_ROM);
		if (DecodeFunction) {
			ZetMapArea(0x0000, 0x7fff, 2, System1Rom1 + 0x20000, System1Rom1);
			ZetMapArea(0x8000, 0xbfff, 2, System1Rom1 + 0x30000, System1Rom1 + 0x10000);  // 30 fetch, 10 for code(?)
		}
	} else {
		ZetSetWriteHandler(System1Z801ProgWrite);
		ZetSetInHandler(System1Z801PortRead);
		ZetSetOutHandler(System1Z801PortWrite);
		ZetMapMemory(System1Rom1,			0x0000, 0x7fff, MAP_ROM);
		ZetMapMemory(System1Rom1 + 0x8000,	0x8000, 0xbfff, MAP_ROM);
		if (DecodeFunction) {
			ZetMapArea(0x0000, 0x7fff, 2, System1Fetch1, System1Rom1);
			ZetMapArea(0x8000, 0xbfff, 2, System1Fetch1 + 0x8000, System1Rom1 + 0x8000);
		}
	}

	ZetMapMemory(System1Ram1,		0xc000, 0xcfff, MAP_RAM);
	ZetMapMemory(System1SpriteRam,	0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(System1PaletteRam,	0xd800, 0xdfff, MAP_RAM);

	System2_videoram_bank_latch_w(0);

	if (!IsSystem2) {
		// System1 has a static fg/bg page
		System1BgRam = System1VideoRam + 0x0000;
		System1FgRam = System1VideoRam + 0x0800;
	}

	ZetMapMemory(System1BgCollisionRam, 0xf000, 0xf3ff, MAP_ROM); // R/O
	ZetMapMemory(System1f4Ram,			0xf400, 0xf7ff, MAP_RAM);
	ZetMapMemory(System1SprCollisionRam,0xf800, 0xfbff, MAP_ROM); // R/O
	ZetMapMemory(System1fcRam,			0xfc00, 0xffff, MAP_RAM);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetSetReadHandler(System1Z802ProgRead);
	ZetSetWriteHandler(System1Z802ProgWrite);
	ZetMapMemory(System1Rom2, 0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(System1Ram2, 0x8000, 0x87ff, MAP_RAM);
	ZetMapMemory(System1Ram2, 0x8800, 0x8fff, MAP_RAM); // mirror
	ZetMapMemory(System1Ram2, 0x9000, 0x97ff, MAP_RAM); // mirror
	ZetMapMemory(System1Ram2, 0x9800, 0x9fff, MAP_RAM); // mirror
	ZetClose();

	if (has_mcu) {
		DrvMCUInit();
	}

	if (Sys1UsePPI) { // Regulus uses the PPI for sound
		ppi8255_init(1);
		ppi8255_set_write_ports(0, System2PPI0WriteA, NULL, System2PPI0WriteC);
	}
	if (IsSystem2) {
		ppi8255_init(1);
		ppi8255_set_write_ports(0, System2PPI0WriteA, System2PPI0WriteB, System2PPI0WriteC);

		System1SpriteXOffset = 15;
	} else {
		System1SpriteXOffset = 1;
	}

	memset(SpriteOnScreenMap, 255, ((wide_mode) ? 512 : 256) * 256);

	SN76489AInit(0, 2000000, 0);
	SN76489AInit(1, 4000000, 1);
	SN76496SetRoute(0, 0.35, BURN_SND_ROUTE_BOTH);
	SN76496SetRoute(1, 0.35, BURN_SND_ROUTE_BOTH);
    SN76496SetBuffered(ZetTotalCycles, 4000000);

	GenericTilesInit();

	MakeInputsFunction = System1MakeInputs;

	if (has_dial) {
		BurnTrackballInit(1);
	}

	// Reset the driver
	if (bReset) System1DoReset();

	return 0;
}

static INT32 System2Init(INT32 nZ80Rom1Num, INT32 nZ80Rom1Size, INT32 nZ80Rom2Num, INT32 nZ80Rom2Size, INT32 nTileRomNum, INT32 nTileRomSize, INT32 nSpriteRomNum, INT32 nSpriteRomSize, bool bReset)
{
	IsSystem2 = 1;

	return System1Init(nZ80Rom1Num, nZ80Rom1Size, nZ80Rom2Num, nZ80Rom2Size, nTileRomNum, nTileRomSize, nSpriteRomNum, nSpriteRomSize, bReset);
}

static INT32 FourdwarrioInit()
{
	DecodeFunction = fdwarrio_decode;

	return System1Init(3, 0x4000, 1, 0x2000, 6, 0x2000, 4, 0x4000, 1);
}

static INT32 BlockgalInit()
{
	INT32 nRet;

	System1MC8123Key = (UINT8*)BurnMalloc(0x2000);
	BurnLoadRom(System1MC8123Key, 14, 1);

	DecodeFunction = blockgal_decode;

	has_dial = 1;

	nRet = System1Init(2, 0x4000, 1, 0x2000, 6, 0x2000, 4, 0x4000, 1);
	BurnFree(System1MC8123Key);

	if (nRet == 0) {
		MakeInputsFunction = BlockgalMakeInputs;
	}

	return nRet;
}

static INT32 BrainInit()
{
	INT32 nRet;

	System1ColourProms = 1;
	System1BankedRom = 1;

	nRet = System1Init(3, 0x8000, 1, 0x8000, 3, 0x4000, 3, 0x8000, 0);

	if (nRet == 0) {
		ZetOpen(0);
		ZetSetOutHandler(BrainZ801PortWrite);
		ZetClose();

		System1DoReset();
	}

	return nRet;
}

static INT32 BullfgtInit()
{
	DecodeFunction = bullfgtj_decode;

	return System1Init(6, 0x2000, 1, 0x2000, 6, 0x2000, 2, 0x4000, 1);
}

static INT32 ThetogyuInit()
{
	DecodeFunction = bullfgtj_decode;

	return System1Init(3, 0x4000, 1, 0x2000, 6, 0x2000, 2, 0x4000, 1);
}

static INT32 FlickyInit()
{
	DecodeFunction = flicky_decode;

	return System1Init(2, 0x4000, 1, 0x2000, 6, 0x2000, 2, 0x4000, 1);
}

static INT32 FlickygInit()
{
	DecodeFunction = flicky_decode;

	return System1Init(2, 0x4000, 1, 0x2000, 3, 0x4000, 2, 0x4000, 1);
}

static INT32 Flicks1Init()
{
	DecodeFunction = flicky_decode;

	return System1Init(4, 0x2000, 1, 0x2000, 6, 0x2000, 2, 0x4000, 1);
}

static INT32 Flicks2Init()
{
	return System1Init(2, 0x4000, 1, 0x2000, 6, 0x2000, 2, 0x4000, 1);
}

static INT32 Flicks2gInit()
{
	return System1Init(2, 0x4000, 1, 0x2000, 3, 0x4000, 2, 0x4000, 1);
}

static INT32 GardiaInit()
{
	INT32 nRet;

	System1ColourProms = 1;
	System1BankedRom = 1;

	DecodeFunction = gardia_decode;

	nRet = System1Init(3, 0x8000, 1, 0x4000, 3, 0x4000, 4, 0x8000, 0);

	if (nRet == 0) {
		ZetOpen(0);
		ZetSetOutHandler(BrainZ801PortWrite);
		ZetClose();

		System1DoReset();
	}

	return nRet;
}

static INT32 GardiabInit()
{
	INT32 nRet;

	System1ColourProms = 1;
	System1BankedRom = 1;

	DecodeFunction = gardiab_decode;

	nRet = System1Init(3, 0x8000, 1, 0x4000, 3, 0x4000, 4, 0x8000, 0);

	if (nRet == 0) {
		ZetOpen(0);
		ZetSetOutHandler(BrainZ801PortWrite);
		ZetClose();

		System1DoReset();
	}

	return nRet;
}

static INT32 GardiajInit()
{
	INT32 nRet;

	System1ColourProms = 1;
	System1BankedRom = 1;

	DecodeFunction = gardia_decode;

	nRet = System1Init(3, 0x8000, 1, 0x4000, 3, 0x8000, 4, 0x8000, 0);

	if (nRet == 0) {
		ZetOpen(0);
		ZetSetOutHandler(BrainZ801PortWrite);
		ZetClose();

		System1DoReset();
	}

	return nRet;
}

static INT32 HvymetalInit()
{
	INT32 nRet;

	System1ColourProms = 1;
	System1BankedRom = 1;

	DecodeFunction = hvymetal_decode;

	nRet = System1Init(3, 0x8000, 1, 0x8000, 6, 0x4000, 4, 0x8000, 0);

	if (nRet == 0) {
		ZetOpen(0);
		ZetSetOutHandler(BrainZ801PortWrite);
		ZetClose();

		System1DoReset();
	}

	return nRet;
}

static INT32 ImsorryInit()
{
	DecodeFunction = imsorry_decode;

	return System1Init(3, 0x4000, 1, 0x2000, 6, 0x2000, 2, 0x4000, 1);
}

static INT32 MrvikingInit()
{
	DecodeFunction = mrviking_decode;

	return System1Init(6, 0x2000, 1, 0x2000, 6, 0x2000, 2, 0x4000, 1);
}

static INT32 MyheroInit()
{
	return System1Init(3, 0x4000, 1, 0x2000, 6, 0x2000, 4, 0x4000, 1);
}

static INT32 SscandalInit()
{
	DecodeFunction = myheroj_decode;

	return System1Init(3, 0x4000, 1, 0x2000, 6, 0x2000, 4, 0x4000, 1);
}

static INT32 MyheroblInit()
{
	DecodeFunction = myheroj_decode;

	return System1Init(3, 0x4000, 1, 0x2000, 3, 0x4000, 4, 0x4000, 1);
}

static INT32 MyherokInit()
{
	DecodeFunction = myherok_decode;
	TileDecodeFunction = myherok_tile_decode;

	return System1Init(3, 0x4000, 1, 0x2000, 3, 0x4000, 4, 0x4000, 1);
}

static INT32 NobbInit()
{
	INT32 nRet;

	System1ColourProms = 1;
	System1BankedRom = 1;

	is_nob = 1;

	nRet = System1Init(3, 0x8000, 1, 0x4000, 3, 0x8000, 4, 0x8000, 0);

	if (nRet == 0) {
		ZetOpen(1);
		ZetMapMemory(System1Rom2, 0x4000, 0x7fff, MAP_ROM); // sound rom mirror (fixes: end of stg. 1 music loss)
		ZetClose();

		ZetOpen(0);
		ZetSetWriteHandler(NoboranbZ801ProgWrite);
		ZetSetInHandler(NoboranbZ801PortRead);
		ZetSetOutHandler(NoboranbZ801PortWrite);
		ZetUnmapMemory(0xc000, 0xcfff, MAP_RAM);
		ZetMapMemory(System1BgCollisionRam,	0xc000, 0xc3ff, MAP_ROM); // .w in handler
		ZetMapMemory(System1f4Ram,			0xc400, 0xc7ff, MAP_RAM);
		ZetMapMemory(System1SprCollisionRam,0xc800, 0xcbff, MAP_ROM); // .w in handler
		ZetMapMemory(System1fcRam,			0xcc00, 0xcfff, MAP_RAM);
		ZetMapMemory(System1SpriteRam,		0xd000, 0xd7ff, MAP_RAM);
		ZetMapMemory(System1PaletteRam,		0xd800, 0xdfff, MAP_RAM);
		ZetMapMemory(System1Ram1,			0xf000, 0xffff, MAP_RAM);
		ZetClose();

		System1DoReset();
	}

	return nRet;
}

static INT32 NobInit()
{
	has_mcu = 1;
	is_nob = 1;

	INT32 rc = NobbInit();

	if (!rc) {
		bprintf(0, _T("nob: patching startup opcode\n"));
		ZetOpen(0);
		memcpy(System1Fetch1, System1Rom1, 0x100);
		System1Fetch1[1] = 0x80;
		ZetMapMemory(System1Fetch1, 0x0000, 0x00ff, MAP_FETCH);
		ZetClose();
	}

	return rc;
}

static INT32 Pitfall2Init()
{
	INT32 nRet;

	DecodeFunction = pitfall2_decode;

	nRet = System1Init(3, 0x4000, 1, 0x2000, 6, 0x2000, 2, 0x4000, 1);

	return nRet;
}

static INT32 PitfalluInit()
{
	INT32 nRet;

	nRet = System1Init(3, 0x4000, 1, 0x2000, 6, 0x2000, 2, 0x4000, 1);

	return nRet;
}

static INT32 RaflesiaInit()
{
	DecodeFunction = fdwarrio_decode;

	return System1Init(3, 0x4000, 1, 0x2000, 6, 0x2000, 4, 0x4000, 1);
}

static INT32 RaflesiauInit()
{
	return System1Init(6, 0x2000, 1, 0x2000, 6, 0x2000, 4, 0x4000, 1);
}

static INT32 RegulusInit()
{
	DecodeFunction = regulus_decode;
	Sys1UsePPI = 1;

	return System1Init(6, 0x2000, 1, 0x2000, 6, 0x2000, 2, 0x4000, 1);
}

static INT32 RegulusuInit()
{
	Sys1UsePPI = 1;

	return System1Init(6, 0x2000, 1, 0x2000, 6, 0x2000, 2, 0x4000, 1);
}

static INT32 SeganinjInit()
{
	DecodeFunction = seganinj_decode;

	return System1Init(3, 0x4000, 1, 0x2000, 6, 0x2000, 4, 0x4000, 1);
}

static INT32 SeganinuInit()
{
	return System1Init(3, 0x4000, 1, 0x2000, 6, 0x2000, 4, 0x4000, 1);
}

static INT32 NprincesInit()
{
	DecodeFunction = flicky_decode;

	return System1Init(6, 0x2000, 1, 0x2000, 6, 0x2000, 4, 0x4000, 1);
}

static INT32 NprincsoInit()
{
	DecodeFunction = nprinces_decode;

	return System1Init(3, 0x4000, 1, 0x2000, 6, 0x2000, 4, 0x4000, 1);
}

static INT32 NprincsuInit()
{
	return System1Init(6, 0x2000, 1, 0x2000, 6, 0x2000, 4, 0x4000, 1);
}

static INT32 NprincsbInit()
{
	DecodeFunction = flicky_decode;

	return System1Init(3, 0x4000, 1, 0x2000, 6, 0x2000, 4, 0x4000, 1);
}

static INT32 SpatterInit()
{
	DecodeFunction = spatter_decode;

	return System1Init(3, 0x4000, 1, 0x2000, 6, 0x2000, 4, 0x4000, 1);
}

static INT32 StarjackInit()
{
	return System1Init(6, 0x2000, 1, 0x2000, 6, 0x2000, 2, 0x4000, 1);
}

static INT32 SwatInit()
{
	DecodeFunction = swat_decode;

	return System1Init(6, 0x2000, 1, 0x2000, 6, 0x2000, 2, 0x4000, 1);
}

static INT32 TeddybbInit()
{
	DecodeFunction = teddybb_decode;

	return System1Init(3, 0x4000, 1, 0x2000, 6, 0x2000, 4, 0x4000, 1);
}

static INT32 TeddybboblInit()
{
	DecodeFunction = teddybb_decode;

	return System1Init(3, 0x4000, 1, 0x2000, 3, 0x4000, 4, 0x4000, 1);
}

static INT32 UpndownInit()
{
	DecodeFunction = nprinces_decode;

	return System1Init(6, 0x2000, 1, 0x2000, 6, 0x2000, 2, 0x4000, 1);
}

static INT32 UpndownuInit()
{
	return System1Init(6, 0x2000, 1, 0x2000, 6, 0x2000, 2, 0x4000, 1);
}

static INT32 WboyInit()
{
	wide_mode = 1;
	DecodeFunction = astrofl_decode;

	return System1Init(3, 0x4000, 1, 0x2000, 6, 0x2000, 4, 0x4000, 1);
}

static INT32 WboyoInit()
{
	wide_mode = 1;
	DecodeFunction = hvymetal_decode;

	return System1Init(3, 0x4000, 1, 0x2000, 6, 0x2000, 4, 0x4000, 1);
}

static INT32 Wboy2Init()
{
	wide_mode = 1;
	DecodeFunction = wboy2_decode;

	return System1Init(6, 0x2000, 1, 0x2000, 6, 0x2000, 4, 0x4000, 1);
}

static INT32 Wboy2uInit()
{
	wide_mode = 1;

	return System1Init(6, 0x2000, 1, 0x2000, 6, 0x2000, 4, 0x4000, 1);
}

static INT32 Wboy4Init()
{
	wide_mode = 1;
	DecodeFunction = fdwarrio_decode;

	return System1Init(2, 0x8000, 1, 0x8000, 3, 0x4000, 2, 0x8000, 1);
}

static INT32 WboyuInit()
{
	wide_mode = 1;

	return System1Init(3, 0x4000, 1, 0x2000, 6, 0x2000, 4, 0x4000, 1);
}

static INT32 WmatchInit()
{
	DecodeFunction = wmatch_decode;

	return System1Init(6, 0x2000, 1, 0x2000, 6, 0x2000, 2, 0x4000, 1);
}

static INT32 ChplftbInit()
{
	System1ColourProms = 1;
	System1BankedRom = 1;

	INT32 nRet = System2Init(3, 0x8000, 1, 0x8000, 3, 0x8000, (is_shtngmst) ? 7 : 4, 0x8000, 1);

	if (nRet == 0) {
		System1RowScroll = (is_shtngmst) ? 0 : 1;
		System1FgRam = System1VideoRam + 0x0000;
		System1BgRam = System1VideoRam + 0x0800;
	}

	return nRet;
}

static INT32 ChopliftInit()
{
	has_mcu = 1;

	INT32 nRet = ChplftbInit();
	if (!nRet) {
	}

	return nRet;
}

static INT32 ShtngmstInit()
{
	EnforceBars = 1;
	has_mcu = 1;
	is_shtngmst = 1;

	INT32 nRet = ChplftbInit();
	if (!nRet) {

		MakeInputsFunction = ShtngmstMakeInputs;

		ZetOpen(0);
		ZetSetInHandler(ShtngmstZ801PortRead);
		ZetClose();

		BurnGunInit(1, true);
	}

	return nRet;
}

static INT32 WbmlInit()
{
	INT32 nRet;
	System1ColourProms = 1;
	System1BankedRom = 1;

	DecodeFunction = wbml_decode;

	System1MC8123Key = (UINT8*)BurnMalloc(0x2000);
	BurnLoadRom(System1MC8123Key, 15, 1);

	nRet = System2Init(3, 0x8000, 1, 0x8000, 3, 0x8000, 4, 0x8000, 1);
	BurnFree(System1MC8123Key);
	System1MC8123Key = NULL;

	return nRet;
}

static INT32 WbmljbInit()
{
	INT32 nRet;

	System1ColourProms = 1;
	System1BankedRom = 2;

	DecodeFunction = wbmljb_decode;

	nRet = System2Init(3, 0x10000, 1, 0x8000, 3, 0x8000, 4, 0x8000, 1);

	System1ScrollXRam = NULL;

	return nRet;
}

static INT32 TokisensInit()
{
	INT32 nRet;

	System1ColourProms = 1;
	System1BankedRom = 1;

	System1MC8123Key = (UINT8*)BurnMalloc(0x2000);
	BurnLoadRom(System1MC8123Key, 15, 1);

	DecodeFunction = tokisens_decode;

	nRet = System2Init(3, 0x8000, 1, 0x8000, 3, 0x8000, 4, 0x8000, 1);
	BurnFree(System1MC8123Key);
	System1MC8123Key = NULL;

	System1ScrollXRam = NULL;

	return nRet;
}

static INT32 TokisensaInit()
{
	INT32 nRet;

	System1ColourProms = 1;
	System1BankedRom = 1;

	DecodeFunction = NULL;

	nRet = System2Init(3, 0x8000, 1, 0x8000, 3, 0x8000, 4, 0x8000, 1);

	System1ScrollXRam = NULL;

	return nRet;
}

static INT32 UfosensiInit()
{
	EnforceBars = 1;
	System1RowScroll = 1;

	return WbmlInit();
}

static INT32 System1Exit()
{
	ZetExit();

	if (has_mcu) {
		DrvMCUExit();
		has_mcu = 0;
	}

	if (has_dial) {
		BurnTrackballExit();
		has_dial = 0;
	}

	SN76496Exit();

	if (IsSystem2 || Sys1UsePPI)
		ppi8255_exit();

	GenericTilesExit();

	BurnFree(Mem);

	if (is_shtngmst) {
		BurnGunExit();
		is_shtngmst = 0;
	}
	is_nob = 0;

	System1SoundLatch = 0;
	System1ScrollX[0] = System1ScrollX[1] = System1ScrollY = 0;
	System1BgScrollX = 0;
	System1BgScrollY = 0;
	System1VideoMode = 0;
	System1FlipScreen = 0;
	System1RomBank = 0;
	NoboranbInp16Step = 0;
	NoboranbInp17Step = 0;
	NoboranbInp23Step = 0;

	System1SpriteRomSize = 0;
	System1NumTiles = 0;
	System1SpriteXOffset = 0;
	System1ColourProms = 0;
	System1BankedRom = 0;
	System1BankSwitch = 0;
	System1BgBankLatch = 0;
	System1BgBank = 0;

	DecodeFunction = NULL;
	TileDecodeFunction = NULL;
	MakeInputsFunction = NULL;

	System1RowScroll = 0;
	IsSystem2 = 0;
	Sys1UsePPI = 0;
	EnforceBars = 0;
	wide_mode = 0;

	return 0;
}

/*==============================================================================================
Graphics Rendering
===============================================================================================*/

static void DrawPixel(INT32 x, INT32 y, INT32 SpriteNum, INT32 Colour)
{
	INT32 xr, yr, SpriteOnScreen, dx, dy;
	INT32 width = (wide_mode) ? 512 : 256;

	dx = x;
	dy = y;
	if (nScreenWidth == 240) dx -= 8;

	if (x < 0 || x > (width-1)  || y < 0 || y > 255) return;

	if (SpriteOnScreenMap[(y * width) + x] != 255) {
		SpriteOnScreen = SpriteOnScreenMap[(y * width) + x];
		System1SprCollisionRam[SpriteOnScreen + (32 * SpriteNum)] = 0xff;
	}

	SpriteOnScreenMap[(y * width) + x] = SpriteNum;

	if (pBurnDraw && dx >= 0 && dx < nScreenWidth && dy >= 0 && dy < nScreenHeight) {
		UINT16 *pPixel = pTransDraw + (dy * nScreenWidth);
		pPixel[dx] = Colour;
	}

	xr = ((x - System1BgScrollX) & 0xff) / 8;
	yr = ((y - System1BgScrollY) & 0xff) / 8;

	if (IsSystem2 == 0) {
		if (System1BgRam[2 * (32 * yr + xr) + 1] & 0x10)
		{
			System1BgCollisionRam[0x20 + SpriteNum] = 0xff;
		}
	}
}

static void DrawSprite(INT32 Num)
{
	INT32 Src, Bank, Height, sy, Row;
	INT16 Skip;
	UINT8 *SpriteBase;
	UINT32 *SpritePalette;

	SpriteBase = System1SpriteRam + (0x10 * Num);
	Src = (SpriteBase[7] << 8) | SpriteBase[6];
	Bank = 0x8000 * (((SpriteBase[3] & 0x80) >> 7) + ((SpriteBase[3] & 0x40) >> 5) + ((SpriteBase[3] & 0x20) >> 3));
	Bank &= (System1SpriteRomSize - 1);
	Skip = (SpriteBase[5] << 8) | SpriteBase[4];

	Height = SpriteBase[1] - SpriteBase[0];
	SpritePalette = System1Palette + (0x10 * Num);

	sy = SpriteBase[0] + 1;

	for (Row = 0; Row < Height; Row++) {
		INT32 x, y, Src2;

		Src = Src2 = Src + Skip;
		x = ((SpriteBase[3] & 0x01) << 8) + SpriteBase[2] + System1SpriteXOffset;
		y = sy + Row;

		if (System1FlipScreen) {
			// flipped
			y = Height + (sy - Row);
			y -= 3; // flipped offsets
			x += 7; // ""
		}

		if (!wide_mode) x /= 2;

		while(1) {
			INT32 Colour1, Colour2, Data;

			Data = System1Sprites[Bank + (Src2 & 0x7fff)];

			if (Src & 0x8000) {
				Src2--;
				Colour1 = Data & 0x0f;
				Colour2 = Data >> 4;
			} else {
				Src2++;
				Colour1 = Data >> 4;
				Colour2 = Data & 0x0f;
			}

			if (Colour1 == 0x0f) break;
			if (Colour1) DrawPixel(x, y, Num, Colour1 + (0x10 * Num));
			x++;

			if (wide_mode) {
				if (Colour1 == 0x0f) break;
				if (Colour1) DrawPixel(x, y, Num, Colour1 + (0x10 * Num));
				x++;
			}

			if (Colour2 == 0x0f) break;
			if (Colour2) DrawPixel(x, y, Num, Colour2 + (0x10 * Num));
			x++;

			if (wide_mode) {
				if (Colour2 == 0x0f) break;
				if (Colour2) DrawPixel(x, y, Num, Colour2 + (0x10 * Num));
				x++;
			}
		}
	}
}

static void System1DrawSprites()
{
	INT32 i, SpriteBottomY, SpriteTopY;
	UINT8 *SpriteBase;

	if (System1SpriteRam[0] == 0xff) return; // 0xff in first byte of spriteram is all-sprite-disable mode

	memset(SpriteOnScreenMap, 255, ((wide_mode) ? 512 : 256) * 256);

	for (i = 0; i < 32; i++) {
		SpriteBase = System1SpriteRam + (0x10 * i);
		SpriteTopY = SpriteBase[0];
		SpriteBottomY = SpriteBase[1];
		if (SpriteBottomY && (SpriteBottomY - SpriteTopY > 0)) {
			DrawSprite(i);
		}
	}
}

static void System1DrawBgLayer(INT32 PriorityDraw)
{
	INT32 Offs, sx, sy;

	if (wide_mode) {
		System1BgScrollX = ((System1ScrollX[0] | (System1ScrollX[1] << 8)) & 0x1ff) + 28;
	} else {
		System1BgScrollX = ((System1ScrollX[0] >> 1) + ((System1ScrollX[1] & 1) << 7) + 14) & 0xff;
		if (System1FlipScreen) {
			System1BgScrollX -= 19; // flipped offset
		}
	}
	System1BgScrollY = (-System1ScrollY & 0xff);

	if (PriorityDraw == -1) {
		for (Offs = 0; Offs < 0x800; Offs += 2) {
			INT32 Code, Colour;

			Code = (System1BgRam[Offs + 1] << 8) | System1BgRam[Offs + 0];
			Code = ((Code >> 4) & 0x800) | (Code & 0x7ff);
			Colour = ((Code >> 5) & 0x3f);

			sx = (Offs >> 1) % 32;
			sy = (Offs >> 1) / 32;

			if (System1RowScroll)
				System1BgScrollX = (System1ScrollXRam[(Offs/32) & ~1] >> 1) + ((System1ScrollXRam[(Offs/32) | 1] & 1) << 7) ;
			sx = ((wide_mode) ? 16 : 8) * sx + System1BgScrollX;
			sy = 8 * sy + System1BgScrollY;

			if (nScreenWidth == 240) sx -= 8;

			Code &= (System1NumTiles - 1);

			if (wide_mode) {
				RenderCustomTile_Clip(pTransDraw, 16, 8, Code, sx      , sy      , Colour, 3, 512 * 2, System1Tiles);
				RenderCustomTile_Clip(pTransDraw, 16, 8, Code, sx - 512, sy      , Colour, 3, 512 * 2, System1Tiles);
				RenderCustomTile_Clip(pTransDraw, 16, 8, Code, sx      , sy - 256, Colour, 3, 512 * 2, System1Tiles);
				RenderCustomTile_Clip(pTransDraw, 16, 8, Code, sx - 512, sy - 256, Colour, 3, 512 * 2, System1Tiles);
			} else {
				Render8x8Tile_Clip(pTransDraw, Code, sx      , sy      , Colour, 3, 512 * 2, System1Tiles);
				Render8x8Tile_Clip(pTransDraw, Code, sx - 256, sy      , Colour, 3, 512 * 2, System1Tiles);
				Render8x8Tile_Clip(pTransDraw, Code, sx      , sy - 256, Colour, 3, 512 * 2, System1Tiles);
				Render8x8Tile_Clip(pTransDraw, Code, sx - 256, sy - 256, Colour, 3, 512 * 2, System1Tiles);
			}
		}
	} else {
		PriorityDraw <<= 3;

		for (Offs = 0; Offs < 0x800; Offs += 2) {
			if ((System1BgRam[Offs + 1] & 0x08) == PriorityDraw) {
				INT32 Code, Colour;

				Code = (System1BgRam[Offs + 1] << 8) | System1BgRam[Offs + 0];
				Code = ((Code >> 4) & 0x800) | (Code & 0x7ff);
				Colour = ((Code >> 5) & 0x3f);

				INT32 ColourOffs = 0x40;
				if (Colour >= 0x10 && Colour <= 0x1f) ColourOffs += 0x10;
				if (Colour >= 0x20 && Colour <= 0x2f) ColourOffs += 0x20;
				if (Colour >= 0x30 && Colour <= 0x3f) ColourOffs += 0x30;

				sx = (Offs >> 1) % 32;
				sy = (Offs >> 1) / 32;

				if(System1RowScroll)
					System1BgScrollX = (System1ScrollXRam[(Offs/32) & ~1] >> 1) + ((System1ScrollXRam[(Offs/32) | 1] & 1) << 7) ;

				sx = ((wide_mode) ? 16 : 8) * sx + System1BgScrollX;
				sy = 8 * sy + System1BgScrollY;

				if (nScreenWidth == 240) sx -= 8;

				Code &= (System1NumTiles - 1);

				if (wide_mode) {
					RenderCustomTile_Mask_Clip(pTransDraw, 16, 8, Code, sx      , sy      , Colour, 3, 0, 512 * 2, System1Tiles);
					RenderCustomTile_Mask_Clip(pTransDraw, 16, 8, Code, sx - 512, sy      , Colour, 3, 0, 512 * 2, System1Tiles);
					RenderCustomTile_Mask_Clip(pTransDraw, 16, 8, Code, sx      , sy - 256, Colour, 3, 0, 512 * 2, System1Tiles);
					RenderCustomTile_Mask_Clip(pTransDraw, 16, 8, Code, sx - 512, sy - 256, Colour, 3, 0, 512 * 2, System1Tiles);
				} else {
					Render8x8Tile_Mask_Clip(pTransDraw, Code, sx      , sy      , Colour, 3, 0, 512 * 2, System1Tiles);
					Render8x8Tile_Mask_Clip(pTransDraw, Code, sx - 256, sy      , Colour, 3, 0, 512 * 2, System1Tiles);
					Render8x8Tile_Mask_Clip(pTransDraw, Code, sx      , sy - 256, Colour, 3, 0, 512 * 2, System1Tiles);
					Render8x8Tile_Mask_Clip(pTransDraw, Code, sx - 256, sy - 256, Colour, 3, 0, 512 * 2, System1Tiles);
				}
			}
		}
	}
}

static void System1DrawFgLayer(INT32 PriorityDraw)
{
	INT32 Offs, sx, sy;

	PriorityDraw <<= 3;

	for (Offs = 0; Offs < 0x700; Offs += 2) {
		INT32 Code, Colour;

		if ((System1FgRam[Offs + 1] & 0x08) == PriorityDraw) {
			Code = (System1FgRam[Offs + 1] << 8) | System1FgRam[Offs + 0];
			Code = ((Code >> 4) & 0x800) | (Code & 0x7ff);
			Colour = (Code >> 5) & 0x3f;

			sx = (Offs >> 1) % 32;
			sy = (Offs >> 1) / 32;

			sx *= ((wide_mode) ? 16 : 8);
			sy *= 8;

			if (nScreenWidth == 240) sx -= 8;

			Code %= System1NumTiles;
			Code &= (System1NumTiles - 1);

			if (System1TilesPenUsage[Code] & ~1) {
				RenderCustomTile_Mask_Clip(pTransDraw, (wide_mode) ? 16 : 8, 8, Code, sx, sy, Colour, 3, 0, 512, System1Tiles);
			}
		}
	}
}

static inline UINT8 pal2bit(UINT8 bits)
{
	bits &= 3;
	return (bits << 6) | (bits << 4) | (bits << 2) | bits;
}

static inline UINT8 pal3bit(UINT8 bits)
{
	bits &= 7;
	return (bits << 5) | (bits << 2) | (bits >> 1);
}

inline static UINT32 CalcCol(UINT16 nColour)
{
	INT32 r, g, b;

	r = pal3bit(nColour >> 0);
	g = pal3bit(nColour >> 3);
	b = pal2bit(nColour >> 6);

	return BurnHighCol(r, g, b, 0);
}

static INT32 System1CalcPalette()
{
	if (System1ColourProms) {

		INT32 i;
		for (i = 0; i < 0x800; i++) {
			INT32 bit0, bit1, bit2, bit3, r, g, b, val;

			val = System1PromRed[System1PaletteRam[i]];
			bit0 = (val >> 0) & 0x01;
			bit1 = (val >> 1) & 0x01;
			bit2 = (val >> 2) & 0x01;
			bit3 = (val >> 3) & 0x01;
			r = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

			val = System1PromGreen[System1PaletteRam[i]];
			bit0 = (val >> 0) & 0x01;
			bit1 = (val >> 1) & 0x01;
			bit2 = (val >> 2) & 0x01;
			bit3 = (val >> 3) & 0x01;
			g = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

			val = System1PromBlue[System1PaletteRam[i]];
			bit0 = (val >> 0) & 0x01;
			bit1 = (val >> 1) & 0x01;
			bit2 = (val >> 2) & 0x01;
			bit3 = (val >> 3) & 0x01;
			b = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

			System1Palette[i] = BurnHighCol(r, g, b, 0);
		}
	} else {

		for (INT32 i = 0; i < 0x800; i++) {
			System1Palette[i] = CalcCol(System1PaletteRam[i]);
		}
	}
	return 0;
}

static INT32 System1Render()
{
	BurnTransferClear();
	System1CalcPalette();

	System1ScrollY = System1VideoRam[0x800+0x7bd];
	System1ScrollX[0] = System1VideoRam[0x800+0x7fc];
	System1ScrollX[1] = System1VideoRam[0x800+0x7fd];

	if (nBurnLayer & 1) System1DrawBgLayer(-1);
	if (nBurnLayer & 2) System1DrawFgLayer(0);
	if (nBurnLayer & 4) System1DrawBgLayer(0);
	if (nSpriteEnable & 1) System1DrawSprites();
	if (nBurnLayer & 8) System1DrawBgLayer(1);
	if (nSpriteEnable & 2) System1DrawFgLayer(1);
	if (System1VideoMode & 0x10) BurnTransferClear();
	BurnTransferCopy(System1Palette);

	return 0;
}

static void System2DrawFgLayer()
{
	for (INT32 offs = 0; offs < 0x700; offs += 2)
	{
		INT32 sx, sy, code;

		sx = (offs / 2) % 32;
		sy = (offs / 2) / 32;
		code = System1VideoRam[offs] | (System1VideoRam[offs + 1] << 8);
		code = ((code >> 4) & 0x800) | (code & 0x7ff);
		sx *= 8;
		sy *= 8;
		Render8x8Tile_Mask_Clip(pTransDraw, code, sx      , sy      , ((code >> 5) & 0x3f), 3, 0, 512, System1Tiles);
		Render8x8Tile_Mask_Clip(pTransDraw, code, sx - 256, sy      , ((code >> 5) & 0x3f), 3, 0, 512, System1Tiles);
		Render8x8Tile_Mask_Clip(pTransDraw, code, sx      , sy - 256, ((code >> 5) & 0x3f), 3, 0, 512, System1Tiles);
		Render8x8Tile_Mask_Clip(pTransDraw, code, sx - 256, sy - 256, ((code >> 5) & 0x3f), 3, 0, 512, System1Tiles);
	}
}

static void System2DrawBgLayer(INT32 trasp)
{
	INT32 scrollx = (System1VideoRam[0x7c0] >> 1) + ((System1VideoRam[0x7c1] & 1) << 7) - 256 + 5;
	INT32 scrolly = -System1VideoRam[0x7ba];
	if (System1RowScroll) scrollx = 0;

	for (INT32 page = 0; page < 4; page++)
	{
		if ((nSpriteEnable & (1 << page)) == 0) continue;

		UINT8 *source = System1VideoRam + (System1VideoRam[0x0740 + page * 2] & 0x07) * 0x800;

		INT32 startx = (page & 1) * 256 + scrollx;
		INT32 starty = (page >> 1) * 256 + scrolly;

		INT32 offs = 0;
		for (INT32 row = 0; row < 32 * 8; row += 8)
		{
			for (INT32 col = 0; col < 32 * 8; col += 8)
			{
				if (System1RowScroll) {
					System1BgScrollX = (((System1ScrollXRam[(row/4)] + (System1ScrollXRam[(row/4) + 1] << 8)) & 0x1ff) >> 1) - 256 + 5;
				}
				INT32 x = (startx + System1BgScrollX + col) & 0x1ff;
				INT32 y = (starty + row) & 0x1ff;

				if (x > 256) x -= 512;
				if (y > 224) y -= 512;

				INT32 code = source[offs * 2 + 0] + (source[offs * 2 + 1] << 8);
				INT32 color = (code >> 5) & 0x3f;
				INT32 priority = code & 0x800;
				code = ((code >> 4) & 0x800) | (code & 0x7ff);

				if (!trasp)
				{
					Render8x8Tile_Clip(pTransDraw, code, x      , y      , color , 3, 512 * 2, System1Tiles);
					Render8x8Tile_Clip(pTransDraw, code, x - 256, y      , color , 3, 512 * 2, System1Tiles);
					Render8x8Tile_Clip(pTransDraw, code, x      , y -256 , color , 3, 512 * 2, System1Tiles);
					Render8x8Tile_Clip(pTransDraw, code, x - 256, y -256 , color , 3, 512 * 2, System1Tiles);
				}
				else  if (priority)
				{
					Render8x8Tile_Mask_Clip(pTransDraw, code, x      , y      , color, 3, 0, 512 * 2, System1Tiles);
					Render8x8Tile_Mask_Clip(pTransDraw, code, x - 256, y      , color, 3, 0, 512 * 2, System1Tiles);
					Render8x8Tile_Mask_Clip(pTransDraw, code, x      , y - 256, color, 3, 0, 512 * 2, System1Tiles);
					Render8x8Tile_Mask_Clip(pTransDraw, code, x - 256, y - 256, color, 3, 0, 512 * 2, System1Tiles);
				}

				offs++;
			}
		}
	}
}

static void enforce_bars()
{   // shtngmst, ufosensi:
	// game uses black bars (columns) to hide tilemap-wrapping glitches
	// but sometimes they go missing, and it looks aweful.
	for (INT32 y = 0; y < nScreenHeight; y++) {
		UINT16 *p = &pTransDraw[y * nScreenWidth];
		memset(&p[  0], 0, 8*2);
		memset(&p[248], 0, 8*2);
	}
}

static INT32 System2Render()
{
	BurnTransferClear();
	System1CalcPalette();
	if (nBurnLayer & 1) System2DrawBgLayer(0);
	if (nBurnLayer & 2) System1DrawSprites();
	if (nBurnLayer & 4) System2DrawBgLayer(1);
	if (nBurnLayer & 8) System2DrawFgLayer();
	if (System1VideoMode & 0x10) BurnTransferClear();
	if (EnforceBars) enforce_bars();
	BurnTransferCopy(System1Palette);

	return 0;
}

/*==============================================================================================
Frame functions
===============================================================================================*/

INT32 System1Frame()
{
	if (System1Reset)
	{
		System1DoReset();
	}

	MakeInputsFunction();

	ZetNewFrame();

	if (has_mcu) {
		DrvMCUNewFrame();

		mcs51Open(0);
		DrvMCUIdle(nCyclesExtra[2]);
		mcs51Close();
	}
	ZetIdle(1, nCyclesExtra[1]);

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[3] = { 4000000 / 60, 4000000 / 60, 8000000 / 12 / 60 };
	INT32 nCyclesDone[3] = { nCyclesExtra[0], nCyclesExtra[1], nCyclesExtra[2] };

	for (INT32 i = 0; i < nInterleave; i++) {
		ZetOpen(0);
		if (has_mcu) {
			mcs51Open(0);
		}

        CPU_RUN(0, Zet);
        if (i == nInterleave-1 && (has_mcu == 0 || is_nob)) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		if (has_mcu) {
			CPU_RUN_SYNCINT(2, DrvMCU);
			if (i == nInterleave-1) DrvMCUVblank();
		}

		if (has_mcu) {
			mcs51Close();
		}
		ZetClose();

		ZetOpen(1);
        CPU_RUN_SYNCINT(1, Zet);
		if ((i & 0x3f) == 0x3f) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();
	}

	nCyclesExtra[0] = nCyclesDone[0] - nCyclesTotal[0];
	nCyclesExtra[1] = ZetTotalCycles(1) - nCyclesTotal[1];
	if (has_mcu) {
		mcs51Open(0);
		nCyclesExtra[2] = mcs51TotalCycles() - nCyclesTotal[2];
		mcs51Close();
	}

//	bprintf(0, _T("cyc debug:  %d\t%d\t%d\n"), nCyclesExtra[0], nCyclesExtra[1], nCyclesExtra[2]);

	if (pBurnSoundOut) {
        SN76496Update(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		BurnDrvRedraw();

		if (is_shtngmst) {
			BurnGunDrawTargets();
		}
	} else {
		// if video output disabled, we still have to draw sprites for HW
		// collisions to work
		System1DrawSprites();
	}

	return 0;
}

/*==============================================================================================
Scan Driver
===============================================================================================*/

static INT32 System1Scan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin != NULL) {
		*pnMin = 0x029736;
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

		if (has_mcu) {
			DrvMCUScan(nAction);
		}

		SN76496Scan(nAction, pnMin);

		if (IsSystem2 || Sys1UsePPI) {
			ppi8255_scan();
		}

		if (is_shtngmst) {
			BurnGunScan();

			SCAN_VAR(sht_trigger);
		}

		if (has_dial) {
			BurnTrackballScan();
		}

		SCAN_VAR(System1ScrollX);
		SCAN_VAR(System1ScrollY);
		SCAN_VAR(System1BgScrollX);
		SCAN_VAR(System1BgScrollY);
		SCAN_VAR(System1VideoMode);
		SCAN_VAR(System1FlipScreen);
		SCAN_VAR(System1SoundLatch);
		SCAN_VAR(System1RomBank);
		SCAN_VAR(NoboranbInp16Step);
		SCAN_VAR(NoboranbInp17Step);
		SCAN_VAR(NoboranbInp23Step);

		SCAN_VAR(System1BankSwitch);
		SCAN_VAR(System1BgBankLatch);
		SCAN_VAR(System1BgBank);

		SCAN_VAR(nCyclesExtra);

		if (nAction & ACB_WRITE) {
			if (System1BankedRom) {
				ZetOpen(0);
				System1BankRom();
				ZetClose();
			}
		}
	}

	return 0;
}

/*==============================================================================================
Driver defs
===============================================================================================*/

struct BurnDriver BurnDrvFourdwarrio = {
	"4dwarrio", NULL, NULL, NULL, "1985",
	"4-D Warriors (315-5162)\0", NULL, "Coreland / Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_HORSHOOT, 0,
	NULL, FourdwarrioRomInfo, FourdwarrioRomName, NULL, NULL, NULL, NULL, MyheroInputInfo, FourdwarrioDIPInfo,
	FourdwarrioInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvBlockgal = {
	"blockgal", NULL, NULL, NULL, "1987",
	"Block Gal (MC-8123B, 317-0029)\0", NULL, "Sega / Vic Tokai", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_BREAKOUT, 0,
	NULL, BlockgalRomInfo, BlockgalRomName, NULL, NULL, NULL, NULL, BlockgalInputInfo, BlockgalDIPInfo,
	BlockgalInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 224, 256, 3, 4
};

struct BurnDriver BurnDrvBrain = {
	"brain", NULL, NULL, NULL, "1986",
	"Brain\0", NULL, "Coreland / Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_HORSHOOT, 0,
	NULL, BrainRomInfo, BrainRomName, NULL, NULL, NULL, NULL, MyheroInputInfo, BrainDIPInfo,
	BrainInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvBullfgt = {
	"bullfgt", NULL, NULL, NULL, "1984",
	"Bullfight (315-5065)\0", NULL, "Coreland / Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_SPORTSMISC, 0,
	NULL, BullfgtRomInfo, BullfgtRomName, NULL, NULL, NULL, NULL, MyheroInputInfo, BullfgtDIPInfo,
	BullfgtInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvThetogyu = {
	"thetogyu", "bullfgt", NULL, NULL, "1984",
	"The Togyu (315-5065, Japan)\0", NULL, "Coreland / Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_SPORTSMISC, 0,
	NULL, ThetogyuRomInfo, ThetogyuRomName, NULL, NULL, NULL, NULL, MyheroInputInfo, BullfgtDIPInfo,
	ThetogyuInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvFlicky = {
	"flicky", NULL, NULL, NULL, "1984",
	"Flicky (128k Version, 315-5051)\0", NULL, "Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_PLATFORM, 0,
	NULL, FlickyRomInfo, FlickyRomName, NULL, NULL, NULL, NULL, FlickyInputInfo, FlickyDIPInfo,
	FlickyInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvFlickya = {
	"flickya", "flicky", NULL, NULL, "1984",
	"Flicky (128k Version, 315-5051, larger ROMs)\0", NULL, "Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_PLATFORM, 0,
	NULL, FlickyaRomInfo, FlickyaRomName, NULL, NULL, NULL, NULL, FlickyInputInfo, FlickyDIPInfo,
	FlickygInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvFlickyb = {
	"flickyb", "flicky", NULL, NULL, "1984",
	"Flicky (128k Version, 315-5051, larger ROMs, newer)\0", NULL, "Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_PLATFORM, 0,
	NULL, FlickybRomInfo, FlickybRomName, NULL, NULL, NULL, NULL, FlickyInputInfo, FlickybDIPInfo,
	FlickygInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvFlickyg = {
	"flickyg", "flicky", NULL, NULL, "1984",
	"Flicky (128k Version, System 2, 315-5051, alt graphics)\0", NULL, "Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_PLATFORM, 0,
	NULL, FlickygRomInfo, FlickygRomName, NULL, NULL, NULL, NULL, FlickyInputInfo, FlickyDIPInfo,
	FlickygInit, System1Exit, System1Frame, System2Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvFlickys1 = {
	"flickys1", "flicky", NULL, NULL, "1984",
	"Flicky (64k Version, 315-5051, set 2)\0", NULL, "Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_PLATFORM, 0,
	NULL, Flickys1RomInfo, Flickys1RomName, NULL, NULL, NULL, NULL, FlickyInputInfo, Flickys1DIPInfo,
	Flicks1Init, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvFlickys2 = {
	"flickys2", "flicky", NULL, NULL, "1984",
	"Flicky (128k Version, not encrypted)\0", NULL, "Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_PLATFORM, 0,
	NULL, Flickys2RomInfo, Flickys2RomName, NULL, NULL, NULL, NULL, FlickyInputInfo, FlickyDIPInfo,
	Flicks2Init, System1Exit, System1Frame, System2Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvFlickys2g = {
	"flickys2g", "flicky", NULL, NULL, "1984",
	"Flicky (128k Version, System 2, not encrypted, alt graphics)\0", NULL, "Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_PLATFORM, 0,
	NULL, Flickys2gRomInfo, Flickys2gRomName, NULL, NULL, NULL, NULL, FlickyInputInfo, FlickyDIPInfo,
	Flicks2gInit, System1Exit, System1Frame, System2Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvFlickyo = {
	"flickyo", "flicky", NULL, NULL, "1984",
	"Flicky (64k Version, 315-5051, set 1)\0", NULL, "Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_PLATFORM, 0,
	NULL, FlickyoRomInfo, FlickyoRomName, NULL, NULL, NULL, NULL, FlickyInputInfo, FlickyDIPInfo,
	Flicks1Init, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvFlickyup = {
	"flickyup", "flicky", NULL, NULL, "1984",
	"Flicky (64k Version, on Up'n Down boardset, set 1)\0", NULL, "Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_PLATFORM, 0,
	NULL, FlickyupRomInfo, FlickyupRomName, NULL, NULL, NULL, NULL, FlickyInputInfo, FlickyDIPInfo,
	Flicks1Init, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvFlickyupa = {
	"flickyupa", "flicky", NULL, NULL, "1984",
	"Flicky (64k Version, on Up'n Down boardset, set 2)\0", NULL, "bootleg", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_PLATFORM, 0,
	NULL, FlickyupaRomInfo, FlickyupaRomName, NULL, NULL, NULL, NULL, FlickyInputInfo, FlickyDIPInfo,
	Flicks1Init, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvGardia = {
	"gardia", NULL, NULL, NULL, "1986",
	"Gardia (317-0006)\0", NULL, "Coreland / Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_VERSHOOT, 0,
	NULL, GardiaRomInfo, GardiaRomName, NULL, NULL, NULL, NULL, MyheroInputInfo, GardiaDIPInfo,
	GardiaInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 224, 256, 3, 4
};

struct BurnDriver BurnDrvGardiab = {
	"gardiab", "gardia", NULL, NULL, "1986",
	"Gardia (317-0007?, bootleg)\0", NULL, "bootleg", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_VERSHOOT, 0,
	NULL, GardiabRomInfo, GardiabRomName, NULL, NULL, NULL, NULL, MyheroInputInfo, GardiaDIPInfo,
	GardiabInit, System1Exit, System1Frame, System2Render, System1Scan,
	NULL, 0x800, 224, 256, 3, 4
};

struct BurnDriver BurnDrvGardiaj = {
	"gardiaj", "gardia", NULL, NULL, "1986",
	"Gardia (Japan, 317-0006)\0", NULL, "Coreland / Sega", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_VERSHOOT, 0,
	NULL, GardiajRomInfo, GardiajRomName, NULL, NULL, NULL, NULL, MyheroInputInfo, GardiaDIPInfo,
	GardiajInit, System1Exit, System1Frame, System2Render, System1Scan,
	NULL, 0x800, 224, 256, 3, 4
};

struct BurnDriver BurnDrvHvymetal = {
	"hvymetal", NULL, NULL, NULL, "1985",
	"Heavy Metal (315-5135)\0", NULL, "Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_VERSHOOT, 0,
	NULL, HvymetalRomInfo, HvymetalRomName, NULL, NULL, NULL, NULL, MyheroInputInfo, HvymetalDIPInfo,
	HvymetalInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvImsorry = {
	"imsorry", NULL, NULL, NULL, "1985",
	"I'm Sorry (315-5110, US)\0", NULL, "Coreland / Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_MAZE, 0,
	NULL, ImsorryRomInfo, ImsorryRomName, NULL, NULL, NULL, NULL, MyheroInputInfo, ImsorryDIPInfo,
	ImsorryInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvImsorryj = {
	"imsorryj", "imsorry", NULL, NULL, "1985",
	"Gonbee no I'm Sorry (315-5110, Japan)\0", NULL, "Coreland / Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_MAZE, 0,
	NULL, ImsorryjRomInfo, ImsorryjRomName, NULL, NULL, NULL, NULL, MyheroInputInfo, ImsorryDIPInfo,
	ImsorryInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvMrviking = {
	"mrviking", NULL, NULL, NULL, "1984",
	"Mister Viking (315-5041)\0", NULL, "Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_VERSHOOT, 0,
	NULL, MrvikingRomInfo, MrvikingRomName, NULL, NULL, NULL, NULL, MyheroInputInfo, MrvikingDIPInfo,
	MrvikingInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 224, 240, 3, 4
};

struct BurnDriver BurnDrvMrvikingj = {
	"mrvikingj", "mrviking", NULL, NULL, "1984",
	"Mister Viking (315-5041, Japan)\0", NULL, "Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_VERSHOOT, 0,
	NULL, MrvikingjRomInfo, MrvikingjRomName, NULL, NULL, NULL, NULL, MyheroInputInfo, MrvikngjDIPInfo,
	MrvikingInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 224, 240, 3, 4
};

struct BurnDriver BurnDrvMyhero = {
	"myhero", NULL, NULL, NULL, "1985",
	"My Hero (US, not encrypted)\0", NULL, "Coreland / Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_PLATFORM, 0,
	NULL, MyheroRomInfo, MyheroRomName, NULL, NULL, NULL, NULL, MyheroInputInfo, MyheroDIPInfo,
	MyheroInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvSscandal = {
	"sscandal", "myhero", NULL, NULL, "1985",
	"Seishun Scandal (315-5132, Japan)\0", NULL, "Coreland / Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_PLATFORM, 0,
	NULL, SscandalRomInfo, SscandalRomName, NULL, NULL, NULL, NULL, MyheroInputInfo, MyheroDIPInfo,
	SscandalInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvMyherobl = {
	"myherobl", "myhero", NULL, NULL, "1985",
	"My Hero (bootleg, 315-5132 encryption)\0", NULL, "bootleg", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_PLATFORM, 0,
	NULL, MyheroblRomInfo, MyheroblRomName, NULL, NULL, NULL, NULL, MyheroInputInfo, MyheroDIPInfo,
	MyheroblInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvMyherok = {
	"myherok", "myhero", NULL, NULL, "1985",
	"Cheongchun Ilbeonji (Korea)\0", NULL, "Coreland / Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_PLATFORM, 0,
	NULL, MyherokRomInfo, MyherokRomName, NULL, NULL, NULL, NULL, MyheroInputInfo, MyheroDIPInfo,
	MyherokInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvNob = {
	"nob", NULL, NULL, NULL, "1986",
	"Noboranka (Japan)\0", NULL, "Coreland / Data East Corporation", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_VERSHOOT, 0,
	NULL, NobRomInfo, NobRomName, NULL, NULL, NULL, NULL, MyheroInputInfo, NobbDIPInfo,
	NobInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 224, 240, 3, 4
};

struct BurnDriver BurnDrvNobb = {
	"nobb", "nob", NULL, NULL, "1986",
	"Noboranka (Japan, bootleg)\0", NULL, "bootleg (Game Electronics)", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_VERSHOOT, 0,
	NULL, NobbRomInfo, NobbRomName, NULL, NULL, NULL, NULL, MyheroInputInfo, NobbDIPInfo,
	NobbInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 224, 240, 3, 4
};

struct BurnDriver BurnDrvPitfall2 = {
	"pitfall2", NULL, NULL, NULL, "1985",
	"Pitfall II (315-5093)\0", NULL, "Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_PLATFORM, 0,
	NULL, Pitfall2RomInfo, Pitfall2RomName, NULL, NULL, NULL, NULL, MyheroInputInfo, Pitfall2DIPInfo,
	Pitfall2Init, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvPitfall2a = {
	"pitfall2a", "pitfall2", NULL, NULL, "1985",
	"Pitfall II (315-5093, Flicky Conversion)\0", NULL, "Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_PLATFORM, 0,
	NULL, Pitfall2aRomInfo, Pitfall2aRomName, NULL, NULL, NULL, NULL, MyheroInputInfo, Pitfall2DIPInfo,
	Pitfall2Init, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvPitfall2u = {
	"pitfall2u", "pitfall2", NULL, NULL, "1985",
	"Pitfall II (not encrypted)\0", NULL, "Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_PLATFORM, 0,
	NULL, Pitfall2uRomInfo, Pitfall2uRomName, NULL, NULL, NULL, NULL, MyheroInputInfo, PitfalluDIPInfo,
	PitfalluInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvRaflesia = {
	"raflesia", NULL, NULL, NULL, "1986",
	"Rafflesia (315-5162)\0", NULL, "Coreland / Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_VERSHOOT, 0,
	NULL, RaflesiaRomInfo, RaflesiaRomName, NULL, NULL, NULL, NULL, MyheroInputInfo, RaflesiaDIPInfo,
	RaflesiaInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 224, 256, 3, 4
};

struct BurnDriver BurnDrvRaflesiau = {
	"raflesiau", "raflesia", NULL, NULL, "1986",
	"Rafflesia (not encrypted)\0", NULL, "Coreland / Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_VERSHOOT, 0,
	NULL, RaflesiauRomInfo, RaflesiauRomName, NULL, NULL, NULL, NULL, MyheroInputInfo, RaflesiaDIPInfo,
	RaflesiauInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 224, 256, 3, 4
};

struct BurnDriver BurnDrvRegulus = {
	"regulus", NULL, NULL, NULL, "1983",
	"Regulus (315-5033, Rev A.)\0", NULL, "Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_VERSHOOT, 0,
	NULL, RegulusRomInfo, RegulusRomName, NULL, NULL, NULL, NULL, RegulusInputInfo, RegulusDIPInfo,
	RegulusInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 224, 240, 3, 4
};

struct BurnDriver BurnDrvReguluso = {
	"reguluso", "regulus", NULL, NULL, "1983",
	"Regulus (315-5033)\0", NULL, "Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_VERSHOOT, 0,
	NULL, RegulusoRomInfo, RegulusoRomName, NULL, NULL, NULL, NULL, RegulusInputInfo, RegulusoDIPInfo,
	RegulusInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 224, 240, 3, 4
};

struct BurnDriver BurnDrvRegulusu = {
	"regulusu", "regulus", NULL, NULL, "1983",
	"Regulus (not encrypted)\0", NULL, "Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_VERSHOOT, 0,
	NULL, RegulusuRomInfo, RegulusuRomName, NULL, NULL, NULL, NULL, RegulusInputInfo, RegulusDIPInfo,
	RegulusuInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 224, 240, 3, 4
};

struct BurnDriver BurnDrvSeganinj = {
	"seganinj", NULL, NULL, NULL, "1985",
	"Sega Ninja (315-5102)\0", NULL, "Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_RUNGUN, 0,
	NULL, SeganinjRomInfo, SeganinjRomName, NULL, NULL, NULL, NULL, SeganinjInputInfo, SeganinjDIPInfo,
	SeganinjInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvSeganinju = {
	"seganinju", "seganinj", NULL, NULL, "1985",
	"Sega Ninja (not encrypted)\0", NULL, "Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_RUNGUN, 0,
	NULL, SeganinjuRomInfo, SeganinjuRomName, NULL, NULL, NULL, NULL, SeganinjInputInfo, SeganinjDIPInfo,
	SeganinuInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvSeganinja = {
	"seganinja", "seganinj", NULL, NULL, "1985",
	"Sega Ninja (315-5113)\0", "needs decrypting", "Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_RUNGUN, 0,
	NULL, SeganinjaRomInfo, SeganinjaRomName, NULL, NULL, NULL, NULL, SeganinjInputInfo, SeganinjDIPInfo,
	SeganinjInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvNinja = {
	"ninja", "seganinj", NULL, NULL, "1985",
	"Ninja (315-5102)\0", NULL, "Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_RUNGUN, 0,
	NULL, NinjaRomInfo, NinjaRomName, NULL, NULL, NULL, NULL, SeganinjInputInfo, SeganinjDIPInfo,
	SeganinjInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvNprinces = {
	"nprinces", "seganinj", NULL, NULL, "1985",
	"Ninja Princess (315-5051, 64k Ver. bootleg?)\0", NULL, "bootleg?", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_RUNGUN, 0,
	NULL, NprincesRomInfo, NprincesRomName, NULL, NULL, NULL, NULL, SeganinjInputInfo, SeganinjDIPInfo,
	NprincesInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvNprinceso = {
	"nprinceso", "seganinj", NULL, NULL, "1985",
	"Ninja Princess (315-5098, 128k Ver.)\0", NULL, "Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_RUNGUN, 0,
	NULL, NprincesoRomInfo, NprincesoRomName, NULL, NULL, NULL, NULL, SeganinjInputInfo, SeganinjDIPInfo,
	NprincsoInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvNprincesu = {
	"nprincesu", "seganinj", NULL, NULL, "1985",
	"Ninja Princess (64k Ver. not encrypted)\0", NULL, "Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_RUNGUN, 0,
	NULL, NprincesuRomInfo, NprincesuRomName, NULL, NULL, NULL, NULL, SeganinjInputInfo, SeganinjDIPInfo,
	NprincsuInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvNprincesb = {
	"nprincesb", "seganinj", NULL, NULL, "1985",
	"Ninja Princess (315-5051?, 128k Ver. bootleg?)\0", NULL, "bootleg?", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_RUNGUN, 0,
	NULL, NprincesbRomInfo, NprincesbRomName, NULL, NULL, NULL, NULL, SeganinjInputInfo, SeganinjDIPInfo,
	NprincsbInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvSpatter = {
	"spatter", NULL, NULL, NULL, "1984",
	"Spatter (315-5096)\0", NULL, "Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_MAZE, 0,
	NULL, SpatterRomInfo, SpatterRomName, NULL, NULL, NULL, NULL, MyheroInputInfo, SpatterDIPInfo,
	SpatterInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 240, 224, 4, 3
};

struct BurnDriver BurnDrvSsanchan = {
	"ssanchan", "spatter", NULL, NULL, "1984",
	"Sanrin San Chan (Japan, 315-5096)\0", NULL, "Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_MAZE, 0,
	NULL, SsanchanRomInfo, SsanchanRomName, NULL, NULL, NULL, NULL, MyheroInputInfo, SpatterDIPInfo,
	SpatterInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 240, 224, 4, 3
};

struct BurnDriver BurnDrvStarjack = {
	"starjack", NULL, NULL, NULL, "1983",
	"Star Jacker (Sega)\0", NULL, "Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_VERSHOOT, 0,
	NULL, StarjackRomInfo, StarjackRomName, NULL, NULL, NULL, NULL, MyheroInputInfo, StarjackDIPInfo,
	StarjackInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 224, 240, 3, 4
};

struct BurnDriver BurnDrvStarjacks = {
	"starjacks", "starjack", NULL, NULL, "1983",
	"Star Jacker (Stern Electronics)\0", NULL, "Sega (Stern Electronics license)", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_VERSHOOT, 0,
	NULL, StarjacksRomInfo, StarjacksRomName, NULL, NULL, NULL, NULL, MyheroInputInfo, StarjacsDIPInfo,
	StarjackInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 224, 240, 3, 4
};

struct BurnDriver BurnDrvSwat = {
	"swat", NULL, NULL, NULL, "1984",
	"SWAT (315-5048)\0", NULL, "Coreland / Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_SHOOT, 0,
	NULL, SwatRomInfo, SwatRomName, NULL, NULL, NULL, NULL, MyheroInputInfo, SwatDIPInfo,
	SwatInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 224, 256, 3, 4
};

struct BurnDriver BurnDrvTeddybb = {
	"teddybb", NULL, NULL, NULL, "1985",
	"TeddyBoy Blues (315-5115, New Ver.)\0", NULL, "Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_PLATFORM, 0,
	NULL, TeddybbRomInfo, TeddybbRomName, NULL, NULL, NULL, NULL, MyheroInputInfo, TeddybbDIPInfo,
	TeddybbInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvTeddybbo = {
	"teddybbo", "teddybb", NULL, NULL, "1985",
	"TeddyBoy Blues (315-5115, Old Ver.)\0", NULL, "Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_PLATFORM, 0,
	NULL, TeddybboRomInfo, TeddybboRomName, NULL, NULL, NULL, NULL, MyheroInputInfo, TeddybbDIPInfo,
	TeddybbInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvTeddybbobl = {
	"teddybbobl", "teddybb", NULL, NULL, "1985",
	"TeddyBoy Blues (Old Ver. bootleg)\0", NULL, "bootleg", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_PLATFORM, 0,
	NULL, TeddybboblRomInfo, TeddybboblRomName, NULL, NULL, NULL, NULL, MyheroInputInfo, TeddybbDIPInfo,
	TeddybboblInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvTokisens = {
	"tokisens", NULL, NULL, NULL, "1987",
	"Toki no Senshi - Chrono Soldier (MC-8123, 317-0040)\0", NULL, "Sega", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_PLATFORM, 0,
	NULL, tokisensRomInfo, tokisensRomName, NULL, NULL, NULL, NULL, MyheroInputInfo, TokisensDIPInfo,
	TokisensInit, System1Exit, System1Frame, System2Render, System1Scan,
	NULL, 0x800, 224, 256, 3, 4
};

struct BurnDriver BurnDrvTokisensa = {
	"tokisensa", "tokisens", NULL, NULL, "1987",
	"Toki no Senshi - Chrono Soldier (prototype?)\0", NULL, "Sega", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_PROTOTYPE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_PLATFORM, 0,
	NULL, tokisensaRomInfo, tokisensaRomName, NULL, NULL, NULL, NULL, MyheroInputInfo, TokisensaDIPInfo,
	TokisensaInit, System1Exit, System1Frame, System2Render, System1Scan,
	NULL, 0x800, 224, 256, 3, 4
};

struct BurnDriver BurnDrvUpndown = {
	"upndown", NULL, NULL, NULL, "1983",
	"Up'n Down (315-5030)\0", NULL, "Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_RACING, 0,
	NULL, UpndownRomInfo, UpndownRomName, NULL, NULL, NULL, NULL, UpndownInputInfo, UpndownDIPInfo,
	UpndownInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 224, 256, 3, 4
};

struct BurnDriver BurnDrvUpndownu = {
	"upndownu", "upndown", NULL, NULL, "1983",
	"Up'n Down (not encrypted)\0", NULL, "Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_RACING, 0,
	NULL, UpndownuRomInfo, UpndownuRomName, NULL, NULL, NULL, NULL, UpndownInputInfo, UpndownDIPInfo,
	UpndownuInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 224, 256, 3, 4
};

struct BurnDriver BurnDrvWboy = {
	"wboy", NULL, NULL, NULL, "1986",
	"Wonder Boy (set 1, 315-5177)\0", NULL, "Escape (Sega license)", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_PLATFORM, 0,
	NULL, WboyRomInfo, WboyRomName, NULL, NULL, NULL, NULL, WboyInputInfo, WboyDIPInfo,
	WboyInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 512, 224, 4, 3
};

struct BurnDriver BurnDrvWboyo = {
	"wboyo", "wboy", NULL, NULL, "1986",
	"Wonder Boy (set 2, 315-5135)\0", NULL, "Escape (Sega license)", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_PLATFORM, 0,
	NULL, WboyoRomInfo, WboyoRomName, NULL, NULL, NULL, NULL, WboyInputInfo, WboyDIPInfo,
	WboyoInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 512, 224, 4, 3
};

struct BurnDriver BurnDrvWboy2 = {
	"wboy2", "wboy", NULL, NULL, "1986",
	"Wonder Boy (set 2, 315-5178)\0", NULL, "Escape (Sega license)", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_PLATFORM, 0,
	NULL, Wboy2RomInfo, Wboy2RomName, NULL, NULL, NULL, NULL, WboyInputInfo, WboyDIPInfo,
	Wboy2Init, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 512, 224, 4, 3
};

struct BurnDriver BurnDrvWboy2u = {
	"wboy2u", "wboy", NULL, NULL, "1986",
	"Wonder Boy (set 2, not encrypted)\0", NULL, "Escape (Sega license)", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_PLATFORM, 0,
	NULL, Wboy2uRomInfo, Wboy2uRomName, NULL, NULL, NULL, NULL, WboyInputInfo, WboyDIPInfo,
	Wboy2uInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 512, 224, 4, 3
};

struct BurnDriver BurnDrvWboy3 = {
	"wboy3", "wboy", NULL, NULL, "1986",
	"Wonder Boy (set 3, 315-5135)\0", NULL, "Escape (Sega license)", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_PLATFORM, 0,
	NULL, Wboy3RomInfo, Wboy3RomName, NULL, NULL, NULL, NULL, WboyInputInfo, Wboy3DIPInfo,
	WboyoInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 512, 224, 4, 3
};

struct BurnDriver BurnDrvWboy4 = {
	"wboy4", "wboy", NULL, NULL, "1986",
	"Wonder Boy (315-5162, 4-D Warriors Conversion)\0", NULL, "Escape (Sega license)", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_PLATFORM, 0,
	NULL, Wboy4RomInfo, Wboy4RomName, NULL, NULL, NULL, NULL, WboyInputInfo, WboyDIPInfo,
	Wboy4Init, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 512, 224, 4, 3
};

struct BurnDriver BurnDrvWboy5 = {
	"wboy5", "wboy", NULL, NULL, "1986",
	"Wonder Boy (set 5, bootleg)\0", NULL, "bootleg", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_PLATFORM, 0,
	NULL, Wboy5RomInfo, Wboy5RomName, NULL, NULL, NULL, NULL, WboyInputInfo, Wboy3DIPInfo,
	WboyoInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 512, 224, 4, 3
};

struct BurnDriver BurnDrvWboyblt = {
	"wboyblt", "wboy", NULL, NULL, "1986",
	"Wonder Boy (Tecfri bootleg)\0", NULL, "bootleg (Tecfri)", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_PLATFORM, 0,
	NULL, WboybltRomInfo, WboybltRomName, NULL, NULL, NULL, NULL, WboyInputInfo, Wboy3DIPInfo,
	WboyoInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 512, 224, 4, 3
};

struct BurnDriver BurnDrvWboyu = {
	"wboyu", "wboy", NULL, NULL, "1986",
	"Wonder Boy (prototype?)\0", NULL, "Escape (Sega license)", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_PLATFORM, 0,
	NULL, WboyuRomInfo, WboyuRomName, NULL, NULL, NULL, NULL, WboyInputInfo, WboyuDIPInfo,
	WboyuInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 512, 224, 4, 3
};

struct BurnDriver BurnDrvWbdeluxe = {
	"wbdeluxe", "wboy", NULL, NULL, "1986",
	"Wonder Boy Deluxe\0", NULL, "hack (Vision Electronics)", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_PLATFORM, 0,
	NULL, WbdeluxeRomInfo, WbdeluxeRomName, NULL, NULL, NULL, NULL, WboyInputInfo, WbdeluxeDIPInfo,
	Wboy2uInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 512, 224, 4, 3
};

struct BurnDriver BurnDrvWmatch = {
	"wmatch", NULL, NULL, NULL, "1984",
	"Water Match (315-5064)\0", NULL, "Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_SPORTSMISC, 0,
	NULL, WmatchRomInfo, WmatchRomName, NULL, NULL, NULL, NULL, WmatchInputInfo, WmatchDIPInfo,
	WmatchInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 224, 240, 3, 4
};

struct BurnDriver BurnDrvShtngmst = {
	"shtngmst", NULL, NULL, NULL, "1985",
	"Shooting Master (8751 315-5159a)\0", NULL, "Sega", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_SHOOT, 0,
	NULL, shtngmstRomInfo, shtngmstRomName, NULL, NULL, NULL, NULL, ShtngmstInputInfo, ShtngmstDIPInfo,
	ShtngmstInit, System1Exit, System1Frame, System2Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvChoplift = {
	"choplift", NULL, NULL,  NULL, "1985",
	"Choplifter (8751 315-5151)\0", NULL, "Sega (licensed from Dan Gorlin)", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_ACTION | GBF_SHOOT, 0,
	NULL, ChopliftRomInfo, ChopliftRomName, NULL, NULL, NULL, NULL, ChopliftInputInfo, ChopliftDIPInfo,
	ChopliftInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvChopliftu = {
	"chopliftu", "choplift", NULL,  NULL, "1985",
	"Choplifter (unprotected)\0", NULL, "Sega (licensed from Dan Gorlin)", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_ACTION | GBF_SHOOT, 0,
	NULL, ChopliftuRomInfo, ChopliftuRomName, NULL, NULL, NULL, NULL, ChopliftInputInfo, ChopliftDIPInfo,
	ChplftbInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvChopliftbl = {
	"chopliftbl", "choplift", NULL,  NULL, "1985",
	"Choplifter (bootleg)\0", NULL, "bootleg", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_ACTION | GBF_SHOOT, 0,
	NULL, ChopliftblRomInfo, ChopliftblRomName, NULL, NULL, NULL, NULL, ChopliftInputInfo, ChopliftDIPInfo,
	ChplftbInit, System1Exit, System1Frame, System1Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvUfosensi = {
	"ufosensi", NULL, NULL, NULL, "1988",
	"Ufo Senshi Yohko Chan (MC-8123, 317-0064)\0", NULL, "Sega", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_PLATFORM, 0,
	NULL, ufosensiRomInfo, ufosensiRomName, NULL, NULL, NULL, NULL, UfosensiInputInfo, UfosensiDIPInfo,
	UfosensiInit, System1Exit, System1Frame, System2Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvWbml = {
	"wbml", NULL, NULL, NULL, "1987",
	"Wonder Boy - Monster Land (Japan New Ver., MC-8123, 317-0043)\0", NULL, "Sega / Westone", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_PLATFORM, 0,
	NULL, wbmlRomInfo, wbmlRomName, NULL, NULL, NULL, NULL, MyheroInputInfo, WbmlDIPInfo,
	WbmlInit, System1Exit, System1Frame, System2Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvWbmljb = {
	"wbmljb", "wbml", NULL, NULL, "1987",
	"Wonder Boy - Monster Land (Japan bootleg)\0", NULL, "bootleg", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_PLATFORM, 0,
	NULL, wbmljbRomInfo, wbmljbRomName, NULL, NULL, NULL, NULL, MyheroInputInfo, WbmlDIPInfo,
	WbmljbInit, System1Exit, System1Frame, System2Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvWbmljo = {
	"wbmljo", "wbml", NULL, NULL, "1987",
	"Wonder Boy - Monster Land (Japan Old Ver., MC-8123, 317-0043)\0", NULL, "Sega / Westone", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_PLATFORM, 0,
	NULL, wbmljoRomInfo, wbmljoRomName, NULL, NULL, NULL, NULL, MyheroInputInfo, WbmlDIPInfo,
	WbmlInit, System1Exit, System1Frame, System2Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvWbmlb = {
	"wbmlb", "wbml", NULL, NULL, "1987",
	"Wonder Boy - Monster Land (English bootleg set 1)\0", NULL, "bootleg", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_PLATFORM, 0,
	NULL, wbmlbRomInfo, wbmlbRomName, NULL, NULL, NULL, NULL, MyheroInputInfo, WbmlDIPInfo,
	WbmljbInit, System1Exit, System1Frame, System2Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvWbmlbg = {
	"wbmlbg", "wbml", NULL, NULL, "1987",
	"Wonder Boy - Monster Land (English bootleg set 2)\0", NULL, "bootleg (Galaxy Electronics)", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_PLATFORM, 0,
	NULL, wbmlbgRomInfo, wbmlbgRomName, NULL, NULL, NULL, NULL, MyheroInputInfo, WbmlDIPInfo,
	WbmljbInit, System1Exit, System1Frame, System2Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvWbmlbge = {
	"wbmlbge", "wbml", NULL, NULL, "1987",
	"Wonder Boy - Monster Land (English bootleg set 3)\0", NULL, "bootleg (Gecas)", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_PLATFORM, 0,
	NULL, wbmlbgeRomInfo, wbmlbgeRomName, NULL, NULL, NULL, NULL, MyheroInputInfo, WbmlDIPInfo,
	WbmljbInit, System1Exit, System1Frame, System2Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvWbmlvc = {
	"wbmlvc", "wbml", NULL, NULL, "2009",
	"Wonder Boy - Monster Land (English, Virtual Console)\0", NULL, "Sega", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_PLATFORM, 0,
	NULL, wbmlvcRomInfo, wbmlvcRomName, NULL, NULL, NULL, NULL, MyheroInputInfo, WbmlDIPInfo,
	WbmljbInit, System1Exit, System1Frame, System2Render, System1Scan,
	NULL, 0x600, 256, 224, 4, 3
};

struct BurnDriver BurnDrvWbmlvcd = {
	"wbmlvcd", "wbml", NULL, NULL, "2009",
	"Wonder Boy - Monster Land (decrypted bootleg of English, Virtual Console release)\0", NULL, "bootleg (mpatou)", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG , 2, HARDWARE_SEGA_SYSTEM1, GBF_PLATFORM, 0,
	NULL, wbmlvcdRomInfo, wbmlvcdRomName, NULL, NULL, NULL, NULL, MyheroInputInfo, WbmlDIPInfo,
	TokisensaInit, System1Exit, System1Frame, System2Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvWbmld = {
	"wbmld", "wbml", NULL, NULL, "1987",
	"Wonder Boy - Monster Land (decrypted bootleg of Japan New Ver., MC-8123, 317-0043)\0", NULL, "bootleg (mpatou)", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_SEGA_SYSTEM1, GBF_PLATFORM, 0,
	NULL, wbmldRomInfo, wbmldRomName, NULL, NULL, NULL, NULL, MyheroInputInfo, WbmlDIPInfo,
	TokisensaInit, System1Exit, System1Frame, System2Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvWbmljod = {
	"wbmljod", "wbml", NULL, NULL, "1987",
	"Wonder Boy - Monster Land (decrypted bootleg of Japan Old Ver., MC-8123, 317-0043)\0", NULL, "bootleg (mpatou)", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_SEGA_SYSTEM1, GBF_PLATFORM, 0,
	NULL, wbmljodRomInfo, wbmljodRomName, NULL, NULL, NULL, NULL, MyheroInputInfo, WbmlDIPInfo,
	TokisensaInit, System1Exit, System1Frame, System2Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

struct BurnDriver BurnDrvWbmlh = {
	"wbmlh", "wbml", NULL, NULL, "1987",
	"Wonder Boy - Monster Land (English, difficulty hack)\0", NULL, "bootleg", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM1, GBF_PLATFORM, 0,
	NULL, wbmlhRomInfo, wbmlhRomName, NULL, NULL, NULL, NULL, MyheroInputInfo, WbmlDIPInfo,
	WbmljbInit, System1Exit, System1Frame, System2Render, System1Scan,
	NULL, 0x800, 256, 224, 4, 3
};

// wboysys2
// blckgalb
// dakkochn
