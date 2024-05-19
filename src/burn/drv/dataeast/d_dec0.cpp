// Based on MAME driver by Bryan McPhail

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "m6502_intf.h"
#include "msm6295.h"
#include "burn_ym2203.h"
#include "burn_ym3812.h"
#include "h6280_intf.h"
#include "mcs51.h"

static UINT8 DrvInputPort0[8]       = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvInputPort1[8]       = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvInputPort2[8]       = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvDip[3]              = {0, 0, 0};
static UINT8 DrvInput[3]            = {0x00, 0x00, 0x00};
static UINT8 DrvReset               = 0;

static UINT8 *Mem                   = NULL;
static UINT8 *MemEnd                = NULL;
static UINT8 *RamStart              = NULL;
static UINT8 *RamEnd                = NULL;
static UINT8 *Drv68KRom             = NULL;
static UINT8 *Drv68KRam             = NULL;
static UINT8 *DrvMCURom             = NULL;
static UINT8 *DrvM6502Rom           = NULL;
static UINT8 *DrvM6502Ram           = NULL;
static UINT8 *DrvH6280Rom           = NULL;
static UINT8 *DrvH6280Ram           = NULL;
static UINT8 *DrvCharRam            = NULL;
static UINT8 *DrvCharCtrl0Ram       = NULL;
static UINT8 *DrvCharCtrl1Ram       = NULL;
static UINT8 *DrvCharColScrollRam   = NULL;
static UINT8 *DrvCharRowScrollRam   = NULL;
static UINT8 *DrvVideo1Ram          = NULL;
static UINT8 *DrvVideo1Ctrl0Ram     = NULL;
static UINT8 *DrvVideo1Ctrl1Ram     = NULL;
static UINT8 *DrvVideo1ColScrollRam = NULL;
static UINT8 *DrvVideo1RowScrollRam = NULL;
static UINT8 *DrvVideo2Ram          = NULL;
static UINT8 *DrvVideo2Ctrl0Ram     = NULL;
static UINT8 *DrvVideo2Ctrl1Ram     = NULL;
static UINT8 *DrvVideo2ColScrollRam = NULL;
static UINT8 *DrvVideo2RowScrollRam = NULL;
static UINT8 *DrvPaletteRam         = NULL;
static UINT8 *DrvPalette2Ram        = NULL;
static UINT8 *DrvSpriteRam          = NULL;
static UINT8 *DrvSpriteDMABufferRam = NULL;
static UINT8 *DrvSharedRam          = NULL;
static UINT8 *DrvChars              = NULL;
static UINT8 *DrvTiles1             = NULL;
static UINT8 *DrvTiles2             = NULL;
static UINT8 *DrvSprites            = NULL;
static UINT8 *DrvTempRom            = NULL;
static UINT32 *DrvPalette           = NULL;
static UINT16 *pCharLayerDraw       = NULL;
static UINT16 *pTile1LayerDraw      = NULL;
static UINT16 *pTile2LayerDraw      = NULL;

// i8751 MCU emulation (hbarrel, birdtry) & MCU Simulation
static INT32 realMCU = 0;
static UINT16 i8751RetVal;
static UINT16 i8751Command;
static UINT8 i8751PortData[4] = { 0, 0, 0, 0 };

static INT32 bTimerNullCPU = 0;

static UINT8 DrvVBlank;
static UINT8 DrvSoundLatch;
static UINT8 DrvFlipScreen;
static INT32 DrvPriority;
static INT32 DrvCharTilemapWidth;
static INT32 DrvCharTilemapHeight;
static INT32 DrvTile1TilemapWidth;
static INT32 DrvTile1TilemapHeight;
static INT32 DrvTile2TilemapWidth;
static INT32 DrvTile2TilemapHeight;
static UINT8 DrvTileRamBank[3];
static UINT8 DrvSlyspyProtValue;
static UINT8 DrvSlyspySoundProt;

typedef INT32 (*Dec0LoadRoms)();
static Dec0LoadRoms LoadRomsFunction;

static void SlyspySetProtectionMap(UINT8 Type);

static INT32 DrvCharPalOffset = 0;
static INT32 DrvSpritePalOffset = 256;

// Rotation stuff! -dink
static UINT8  DrvFakeInput[14]       = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; // 0-5 legacy; 6-9 P1, 10-13 P2
static UINT8  nRotateHoldInput[2]   = {0, 0};
static INT32  nRotate[2]            = {0, 0};
static INT32  nRotateTarget[2]      = {0, 0};
static INT32  nRotateTry[2]         = {0, 0};
static UINT32 nRotateTime[2]        = {0, 0};
static UINT8  game_rotates = 0;
static UINT8  nAutoFireCounter[2] 	= {0, 0};

static INT32 nCyclesDone[3], nCyclesTotal[3], nExtraCycles[3];
static INT32 nPrevBurnCPUSpeedAdjust;

static INT32 slyspy_mode = 0;

static INT32 Dec0Game = 0;

#define DEC0_GAME_BADDUDES	1
#define DEC1_GAME_MIDRES    4

static struct BurnInputInfo Dec0InputList[] =
{
	{"P1 Coin"              , BIT_DIGITAL  , DrvInputPort2 + 4, "p1 coin"   },
	{"P1 Start"             , BIT_DIGITAL  , DrvInputPort2 + 2, "p1 start"  },
	{"P1 Up"                , BIT_DIGITAL  , DrvInputPort0 + 0, "p1 up"     },
	{"P1 Down"              , BIT_DIGITAL  , DrvInputPort0 + 1, "p1 down"   },
	{"P1 Left"              , BIT_DIGITAL  , DrvInputPort0 + 2, "p1 left"   },
	{"P1 Right"             , BIT_DIGITAL  , DrvInputPort0 + 3, "p1 right"  },
	{"P1 Fire 1"            , BIT_DIGITAL  , DrvInputPort0 + 4, "p1 fire 1" },
	{"P1 Fire 2"            , BIT_DIGITAL  , DrvInputPort0 + 5, "p1 fire 2" },
	{"P1 Fire 3"            , BIT_DIGITAL  , DrvInputPort0 + 6, "p1 fire 3" },
	{"P1 Fire 4"            , BIT_DIGITAL  , DrvInputPort0 + 7, "p1 fire 4" },
	{"P1 Fire 5"            , BIT_DIGITAL  , DrvInputPort2 + 0, "p1 fire 5" },
	
	{"P2 Coin"              , BIT_DIGITAL  , DrvInputPort2 + 5, "p2 coin"   },
	{"P2 Start"             , BIT_DIGITAL  , DrvInputPort2 + 3, "p2 start"  },
	{"P2 Up (Cocktail)"     , BIT_DIGITAL  , DrvInputPort1 + 0, "p2 up"     },
	{"P2 Down (Cocktail)"   , BIT_DIGITAL  , DrvInputPort1 + 1, "p2 down"   },
	{"P2 Left (Cocktail)"   , BIT_DIGITAL  , DrvInputPort1 + 2, "p2 left"   },
	{"P2 Right (Cocktail)"  , BIT_DIGITAL  , DrvInputPort1 + 3, "p2 right"  },
	{"P2 Fire 1 (Cocktail)" , BIT_DIGITAL  , DrvInputPort1 + 4, "p2 fire 1" },
	{"P2 Fire 2 (Cocktail)" , BIT_DIGITAL  , DrvInputPort1 + 5, "p2 fire 2" },
	{"P2 Fire 3 (Cocktail)" , BIT_DIGITAL  , DrvInputPort1 + 6, "p2 fire 3" },
	{"P2 Fire 4 (Cocktail)" , BIT_DIGITAL  , DrvInputPort1 + 7, "p2 fire 4" },
	{"P2 Fire 5 (Cocktail)" , BIT_DIGITAL  , DrvInputPort2 + 1, "p2 fire 5" },

	{"Reset"             , BIT_DIGITAL  , &DrvReset        , "reset"     },
	{"Service"           , BIT_DIGITAL  , DrvInputPort2 + 6, "service"   },
	{"Dip 1"             , BIT_DIPSWITCH, DrvDip + 0       , "dip"       },
	{"Dip 2"             , BIT_DIPSWITCH, DrvDip + 1       , "dip"       },
};

STDINPUTINFO(Dec0)

static struct BurnInputInfo Dec1InputList[] =
{
	{"P1 Coin"              , BIT_DIGITAL  , DrvInputPort2 + 0, "p1 coin"   },
	{"P1 Start"             , BIT_DIGITAL  , DrvInputPort0 + 7, "p1 start"  },
	{"P1 Up"                , BIT_DIGITAL  , DrvInputPort0 + 0, "p1 up"     },
	{"P1 Down"              , BIT_DIGITAL  , DrvInputPort0 + 1, "p1 down"   },
	{"P1 Left"              , BIT_DIGITAL  , DrvInputPort0 + 2, "p1 left"   },
	{"P1 Right"             , BIT_DIGITAL  , DrvInputPort0 + 3, "p1 right"  },
	{"P1 Fire 1"            , BIT_DIGITAL  , DrvInputPort0 + 4, "p1 fire 1" },
	{"P1 Fire 2"            , BIT_DIGITAL  , DrvInputPort0 + 5, "p1 fire 2" },
	{"P1 Fire 3"            , BIT_DIGITAL  , DrvInputPort0 + 6, "p1 fire 3" },
	
	{"P2 Coin"              , BIT_DIGITAL  , DrvInputPort2 + 1, "p2 coin"   },
	{"P2 Start"             , BIT_DIGITAL  , DrvInputPort1 + 7, "p2 start"  },
	{"P2 Up (Cocktail)"     , BIT_DIGITAL  , DrvInputPort1 + 0, "p2 up"     },
	{"P2 Down (Cocktail)"   , BIT_DIGITAL  , DrvInputPort1 + 1, "p2 down"   },
	{"P2 Left (Cocktail)"   , BIT_DIGITAL  , DrvInputPort1 + 2, "p2 left"   },
	{"P2 Right (Cocktail)"  , BIT_DIGITAL  , DrvInputPort1 + 3, "p2 right"  },
	{"P2 Fire 1 (Cocktail)" , BIT_DIGITAL  , DrvInputPort1 + 4, "p2 fire 1" },
	{"P2 Fire 2 (Cocktail)" , BIT_DIGITAL  , DrvInputPort1 + 5, "p2 fire 2" },
	{"P2 Fire 3 (Cocktail)" , BIT_DIGITAL  , DrvInputPort1 + 6, "p2 fire 3" },

	{"Reset"             , BIT_DIGITAL  , &DrvReset        , "reset"     },
	{"Service"           , BIT_DIGITAL  , DrvInputPort2 + 2, "service"   },
	{"Dip 1"             , BIT_DIPSWITCH, DrvDip + 0       , "dip"       },
	{"Dip 2"             , BIT_DIPSWITCH, DrvDip + 1       , "dip"       },
};

STDINPUTINFO(Dec1)

static struct BurnInputInfo HbarrelInputList[] =
{
	{"P1 Coin"              , BIT_DIGITAL  , DrvInputPort2 + 4, "p1 coin"   },
	{"P1 Start"             , BIT_DIGITAL  , DrvInputPort2 + 2, "p1 start"  },
	{"P1 Up"                , BIT_DIGITAL  , DrvInputPort0 + 0, "p1 up"     },
	{"P1 Down"              , BIT_DIGITAL  , DrvInputPort0 + 1, "p1 down"   },
	{"P1 Left"              , BIT_DIGITAL  , DrvInputPort0 + 2, "p1 left"   },
	{"P1 Right"             , BIT_DIGITAL  , DrvInputPort0 + 3, "p1 right"  },
	{"P1 Fire 1"            , BIT_DIGITAL  , DrvInputPort0 + 4, "p1 fire 1" },
	{"P1 Fire 2"            , BIT_DIGITAL  , DrvInputPort0 + 5, "p1 fire 2" },
	{"P1 Fire 3 (Rotate)"   , BIT_DIGITAL  , DrvFakeInput + 4,  "p1 fire 3" },
	{"P1 Rotate Left"       , BIT_DIGITAL  , DrvFakeInput + 0,  "p1 rotate left" },
	{"P1 Rotate Right"      , BIT_DIGITAL  , DrvFakeInput + 1,  "p1 rotate right" },
	{"P1 Shoot Up"       	, BIT_DIGITAL  , DrvFakeInput + 6,  "p1 up 2" }, // 6
	{"P1 Shoot Down"      	, BIT_DIGITAL  , DrvFakeInput + 7,  "p1 down 2" }, // 7
	{"P1 Shoot Left"       	, BIT_DIGITAL  , DrvFakeInput + 8,  "p1 left 2" }, // 8
	{"P1 Shoot Right"      	, BIT_DIGITAL  , DrvFakeInput + 9,  "p1 right 2" }, // 9
	
	{"P2 Coin"              , BIT_DIGITAL  , DrvInputPort2 + 5, "p2 coin"   },
	{"P2 Start"             , BIT_DIGITAL  , DrvInputPort2 + 3, "p2 start"  },
	{"P2 Up (Cocktail)"     , BIT_DIGITAL  , DrvInputPort1 + 0, "p2 up"     },
	{"P2 Down (Cocktail)"   , BIT_DIGITAL  , DrvInputPort1 + 1, "p2 down"   },
	{"P2 Left (Cocktail)"   , BIT_DIGITAL  , DrvInputPort1 + 2, "p2 left"   },
	{"P2 Right (Cocktail)"  , BIT_DIGITAL  , DrvInputPort1 + 3, "p2 right"  },
	{"P2 Fire 1 (Cocktail)" , BIT_DIGITAL  , DrvInputPort1 + 4, "p2 fire 1" },
	{"P2 Fire 2 (Cocktail)" , BIT_DIGITAL  , DrvInputPort1 + 5, "p2 fire 2" },
	{"P2 Fire 3 (Rotate)"   , BIT_DIGITAL  , DrvFakeInput + 5,  "p2 fire 3" },
	{"P2 Rotate Left"       , BIT_DIGITAL  , DrvFakeInput + 2,  "p2 rotate left" },
	{"P2 Rotate Right"      , BIT_DIGITAL  , DrvFakeInput + 3,  "p2 rotate right" },
	{"P2 Shoot Up"       	, BIT_DIGITAL  , DrvFakeInput + 10, "p2 up 2" },
	{"P2 Shoot Down"      	, BIT_DIGITAL  , DrvFakeInput + 11, "p2 down 2" },
	{"P2 Shoot Left"       	, BIT_DIGITAL  , DrvFakeInput + 12, "p2 left 2" },
	{"P2 Shoot Right"      	, BIT_DIGITAL  , DrvFakeInput + 13, "p2 right 2" },

	{"Reset"             , BIT_DIGITAL  , &DrvReset        , "reset"     },
	{"Service"           , BIT_DIGITAL  , DrvInputPort2 + 6, "service"   },
	{"Dip 1"             , BIT_DIPSWITCH, DrvDip + 0       , "dip"       },
	{"Dip 2"             , BIT_DIPSWITCH, DrvDip + 1       , "dip"       },
	{"Dip 3"             , BIT_DIPSWITCH, DrvDip + 2       , "dip"       },
};

STDINPUTINFO(Hbarrel)

static struct BurnInputInfo BirdtryInputList[] =
{
	{"P1 Coin"              , BIT_DIGITAL  , DrvInputPort2 + 4, "p1 coin"   },
	{"P1 Start"             , BIT_DIGITAL  , DrvInputPort2 + 2, "p1 start"  },
	{"P1 Up"                , BIT_DIGITAL  , DrvInputPort0 + 0, "p1 up"     },
	{"P1 Down"              , BIT_DIGITAL  , DrvInputPort0 + 1, "p1 down"   },
	{"P1 Left"              , BIT_DIGITAL  , DrvInputPort0 + 2, "p1 left"   },
	{"P1 Right"             , BIT_DIGITAL  , DrvInputPort0 + 3, "p1 right"  },
	{"P1 Fire 1 (Hit)"      , BIT_DIGITAL  , DrvInputPort0 + 4, "p1 fire 1" },
	{"P1 Fire 2 (Select)"   , BIT_DIGITAL  , DrvInputPort0 + 5, "p1 fire 2" },
	
	{"P2 Coin"              , BIT_DIGITAL  , DrvInputPort2 + 5, "p2 coin"   },
	{"P2 Start"             , BIT_DIGITAL  , DrvInputPort2 + 3, "p2 start"  },
	{"P2 Up (Cocktail)"     , BIT_DIGITAL  , DrvInputPort1 + 0, "p2 up"     },
	{"P2 Down (Cocktail)"   , BIT_DIGITAL  , DrvInputPort1 + 1, "p2 down"   },
	{"P2 Left (Cocktail)"   , BIT_DIGITAL  , DrvInputPort1 + 2, "p2 left"   },
	{"P2 Right (Cocktail)"  , BIT_DIGITAL  , DrvInputPort1 + 3, "p2 right"  },
	{"P2 Fire 1 (Cocktail)" , BIT_DIGITAL  , DrvInputPort1 + 4, "p2 fire 1" },
	{"P2 Fire 2 (Cocktail)" , BIT_DIGITAL  , DrvInputPort1 + 5, "p2 fire 2" },

	{"Reset"             , BIT_DIGITAL  , &DrvReset        , "reset"     },
	{"Service"           , BIT_DIGITAL  , DrvInputPort2 + 6, "service"   },
	{"Dip 1"             , BIT_DIPSWITCH, DrvDip + 0       , "dip"       },
	{"Dip 2"             , BIT_DIPSWITCH, DrvDip + 1       , "dip"       },
};

STDINPUTINFO(Birdtry)

static struct BurnInputInfo MidresInputList[] =
{
	{"P1 Coin"              , BIT_DIGITAL  , DrvInputPort2 + 0, "p1 coin"   },
	{"P1 Start"             , BIT_DIGITAL  , DrvInputPort0 + 7, "p1 start"  },
	{"P1 Up"                , BIT_DIGITAL  , DrvInputPort0 + 0, "p1 up"     },
	{"P1 Down"              , BIT_DIGITAL  , DrvInputPort0 + 1, "p1 down"   },
	{"P1 Left"              , BIT_DIGITAL  , DrvInputPort0 + 2, "p1 left"   },
	{"P1 Right"             , BIT_DIGITAL  , DrvInputPort0 + 3, "p1 right"  },
	{"P1 Fire 1"            , BIT_DIGITAL  , DrvInputPort0 + 4, "p1 fire 1" },
	{"P1 Fire 2"            , BIT_DIGITAL  , DrvInputPort0 + 5, "p1 fire 2" },
	{"P1 Fire 3"            , BIT_DIGITAL  , DrvInputPort0 + 6, "p1 fire 3" },
	{"P1 Fire 4 (Rotate)"   , BIT_DIGITAL  , DrvFakeInput + 4,  "p1 fire 4" },
	{"P1 Rotate Left"       , BIT_DIGITAL  , DrvFakeInput + 0,  "p1 rotate left" },
	{"P1 Rotate Right"      , BIT_DIGITAL  , DrvFakeInput + 1,  "p1 rotate right" },
	{"P1 Shoot Up"       	, BIT_DIGITAL  , DrvFakeInput + 6,  "p1 up 2" }, // 6
	{"P1 Shoot Down"      	, BIT_DIGITAL  , DrvFakeInput + 7,  "p1 down 2" }, // 7
	{"P1 Shoot Left"       	, BIT_DIGITAL  , DrvFakeInput + 8,  "p1 left 2" }, // 8
	{"P1 Shoot Right"      	, BIT_DIGITAL  , DrvFakeInput + 9,  "p1 right 2" }, // 9
	
	{"P2 Coin"              , BIT_DIGITAL  , DrvInputPort2 + 1, "p2 coin"   },
	{"P2 Start"             , BIT_DIGITAL  , DrvInputPort1 + 7, "p2 start"  },
	{"P2 Up (Cocktail)"     , BIT_DIGITAL  , DrvInputPort1 + 0, "p2 up"     },
	{"P2 Down (Cocktail)"   , BIT_DIGITAL  , DrvInputPort1 + 1, "p2 down"   },
	{"P2 Left (Cocktail)"   , BIT_DIGITAL  , DrvInputPort1 + 2, "p2 left"   },
	{"P2 Right (Cocktail)"  , BIT_DIGITAL  , DrvInputPort1 + 3, "p2 right"  },
	{"P2 Fire 1 (Cocktail)" , BIT_DIGITAL  , DrvInputPort1 + 4, "p2 fire 1" },
	{"P2 Fire 2 (Cocktail)" , BIT_DIGITAL  , DrvInputPort1 + 5, "p2 fire 2" },
	{"P2 Fire 3 (Cocktail)" , BIT_DIGITAL  , DrvInputPort1 + 6, "p2 fire 3" },
	{"P2 Fire 4 (Rotate)"   , BIT_DIGITAL  , DrvFakeInput + 5,  "p2 fire 4" },
	{"P2 Rotate Left"       , BIT_DIGITAL  , DrvFakeInput + 2,  "p2 rotate left" },
	{"P2 Rotate Right"      , BIT_DIGITAL  , DrvFakeInput + 3,  "p2 rotate right" },
	{"P2 Shoot Up"       	, BIT_DIGITAL  , DrvFakeInput + 10, "p2 up 2" },
	{"P2 Shoot Down"      	, BIT_DIGITAL  , DrvFakeInput + 11, "p2 down 2" },
	{"P2 Shoot Left"       	, BIT_DIGITAL  , DrvFakeInput + 12, "p2 left 2" },
	{"P2 Shoot Right"      	, BIT_DIGITAL  , DrvFakeInput + 13, "p2 right 2" },

	{"Reset"             , BIT_DIGITAL  , &DrvReset        , "reset"     },
	{"Service"           , BIT_DIGITAL  , DrvInputPort2 + 2, "service"   },
	{"Dip 1"             , BIT_DIPSWITCH, DrvDip + 0       , "dip"       },
	{"Dip 2"             , BIT_DIPSWITCH, DrvDip + 1       , "dip"       },
	{"Dip 3"             , BIT_DIPSWITCH, DrvDip + 2       , "dip"       },
};

STDINPUTINFO(Midres)

static inline void DrvClearOpposites(UINT8* nJoystickInputs)
{
	if ((*nJoystickInputs & 0x03) == 0x03) {
		*nJoystickInputs &= ~0x03;
	}
	if ((*nJoystickInputs & 0x0c) == 0x0c) {
		*nJoystickInputs &= ~0x0c;
	}
}

static inline void DrvMakeInputs()
{
	DrvInput[0] = DrvInput[1] = DrvInput[2] = 0x00;

	for (INT32 i = 0; i < 8; i++) {
		DrvInput[0] |= (DrvInputPort0[i] & 1) << i;
		DrvInput[1] |= (DrvInputPort1[i] & 1) << i;
		DrvInput[2] |= (DrvInputPort2[i] & 1) << i;
	}

	DrvClearOpposites(&DrvInput[0]);
	DrvClearOpposites(&DrvInput[1]);
}

static struct BurnDIPInfo BaddudesDIPList[]=
{
	DIP_OFFSET(0x18)

	// Default Values
	{0x00, 0xff, 0xff, 0xff, NULL                     },
	{0x01, 0xff, 0xff, 0xff, NULL                     },

	// Dip 1
	{0   , 0xfe, 0   , 4   , "Coin A"                 },
	{0x00, 0x01, 0x03, 0x00, "3 Coins 1 Play"         },
	{0x00, 0x01, 0x03, 0x01, "2 Coins 1 Play"         },
	{0x00, 0x01, 0x03, 0x03, "1 Coin  1 Play"         },
	{0x00, 0x01, 0x03, 0x02, "1 Coin  2 Plays"        },
	
	{0   , 0xfe, 0   , 4   , "Coin B"                 },
	{0x00, 0x01, 0x0c, 0x00, "3 Coins 1 Play"         },
	{0x00, 0x01, 0x0c, 0x04, "2 Coins 1 Play"         },
	{0x00, 0x01, 0x0c, 0x0c, "1 Coin  1 Play"         },
	{0x00, 0x01, 0x0c, 0x08, "1 Coin  2 Plays"        },
	
	{0   , 0xfe, 0   , 2   , "Service Mode"           },
	{0x00, 0x01, 0x10, 0x10, "Off"                    },
	{0x00, 0x01, 0x10, 0x00, "On"                     },
	
	{0   , 0xfe, 0   , 2   , "Demo Sounds"            },
	{0x00, 0x01, 0x20, 0x00, "Off"                    },
	{0x00, 0x01, 0x20, 0x20, "On"                     },
	
	{0   , 0xfe, 0   , 2   , "Flip Screen"            },
	{0x00, 0x01, 0x40, 0x40, "Off"                    },
	{0x00, 0x01, 0x40, 0x00, "On"                     },
	
	// Dip 2
	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x01, 0x01, 0x03, 0x01, "1"                      },
	{0x01, 0x01, 0x03, 0x03, "3"                      },
	{0x01, 0x01, 0x03, 0x02, "5"                      },
	{0x01, 0x01, 0x03, 0x00, "Infinite"               },
	
	{0   , 0xfe, 0   , 4   , "Difficulty"             },
	{0x01, 0x01, 0x0c, 0x08, "Easy"                   },
	{0x01, 0x01, 0x0c, 0x0c, "Normal"                 },
	{0x01, 0x01, 0x0c, 0x04, "Hard"                   },
	{0x01, 0x01, 0x0c, 0x00, "Hardest"                },
	
	{0   , 0xfe, 0   , 2   , "Allow continue"         },
	{0x01, 0x01, 0x10, 0x10, "Yes"                    },
	{0x01, 0x01, 0x10, 0x00, "No"                     },
};

STDDIPINFO(Baddudes)

static struct BurnDIPInfo BirdtryDIPList[]=
{
	DIP_OFFSET(0x12)

	{0x00, 0xff, 0xff, 0xff, NULL		},
	{0x01, 0xff, 0xff, 0x3f, NULL		},

	// Dip 1
	{0   , 0xfe, 0   , 4   , "Coin A"                 },
	{0x00, 0x01, 0x03, 0x00, "2 Coins 1 Play"         },
	{0x00, 0x01, 0x03, 0x03, "1 Coin  1 Play"         },
	{0x00, 0x01, 0x03, 0x02, "1 Coin  2 Plays"        },
	{0x00, 0x01, 0x03, 0x01, "1 Coin  3 Plays"        },
	
	{0   , 0xfe, 0   , 4   , "Coin B"                 },
	{0x00, 0x01, 0x0c, 0x00, "2 Coins 1 Play"         },
	{0x00, 0x01, 0x0c, 0x0c, "1 Coin  1 Play"         },
	{0x00, 0x01, 0x0c, 0x08, "1 Coin  2 Plays"        },
	{0x00, 0x01, 0x0c, 0x04, "1 Coin  3 Plays"        },

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x00, 0x01, 0x10, 0x10, "Off"		},
	{0x00, 0x01, 0x10, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x00, 0x01, 0x20, 0x00, "Off"		},
	{0x00, 0x01, 0x20, 0x20, "On"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x00, 0x01, 0x40, 0x40, "Off"		},
	{0x00, 0x01, 0x40, 0x00, "On"		},

	{0   , 0xfe, 0   ,    4, "Difficulty (Extend)"		},
	{0x01, 0x01, 0x03, 0x02, "Easy"		},
	{0x01, 0x01, 0x03, 0x03, "Normal"		},
	{0x01, 0x01, 0x03, 0x01, "Hard"		},
	{0x01, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Difficulty (Course)"		},
	{0x01, 0x01, 0x0c, 0x08, "Easy"		},
	{0x01, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x01, 0x01, 0x0c, 0x04, "Hard"		},
	{0x01, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x01, 0x01, 0x10, 0x00, "Off"		},
	{0x01, 0x01, 0x10, 0x10, "On"		},

	{0   , 0xfe, 0   ,    2, "Timer"		},
	{0x01, 0x01, 0x20, 0x20, "Normal"		},
	{0x01, 0x01, 0x20, 0x00, "Fast"		},

	{0   , 0xfe, 0   ,    4, "Control Panel Type"		},
	{0x01, 0x01, 0xc0, 0xc0, "Type A - Cocktail"		},
	{0x01, 0x01, 0xc0, 0x80, "Type B - Cocktail 2"		},
	{0x01, 0x01, 0xc0, 0x40, "Unused"		},
	{0x01, 0x01, 0xc0, 0x00, "Type C - Upright"		},
};

STDDIPINFO(Birdtry)

static struct BurnDIPInfo BouldashDIPList[]=
{
	DIP_OFFSET(0x14)

	// Default Values
	{0x00, 0xff, 0xff, 0x7f, NULL                     },
	{0x01, 0xff, 0xff, 0x7f, NULL                     },

	// Dip 1
	{0   , 0xfe, 0   , 8   , "Coin A"                 },
	{0x00, 0x01, 0x07, 0x00, "3 Coins 1 Play"         },
	{0x00, 0x01, 0x07, 0x01, "2 Coins 1 Play"         },
	{0x00, 0x01, 0x07, 0x07, "1 Coin  1 Play"         },
	{0x00, 0x01, 0x07, 0x06, "1 Coin  2 Plays"        },
	{0x00, 0x01, 0x07, 0x05, "1 Coin  3 Plays"        },
	{0x00, 0x01, 0x07, 0x04, "1 Coin  4 Plays"        },
	{0x00, 0x01, 0x07, 0x03, "1 Coin  5 Plays"        },
	{0x00, 0x01, 0x07, 0x02, "1 Coin  6 Plays"        },
	
	{0   , 0xfe, 0   , 8   , "Coin B"                 },
	{0x00, 0x01, 0x38, 0x00, "3 Coins 1 Play"         },
	{0x00, 0x01, 0x38, 0x08, "2 Coins 1 Play"         },
	{0x00, 0x01, 0x38, 0x38, "1 Coin  1 Play"         },
	{0x00, 0x01, 0x38, 0x30, "1 Coin  2 Plays"        },
	{0x00, 0x01, 0x38, 0x28, "1 Coin  3 Plays"        },
	{0x00, 0x01, 0x38, 0x20, "1 Coin  4 Plays"        },
	{0x00, 0x01, 0x38, 0x18, "1 Coin  5 Plays"        },
	{0x00, 0x01, 0x38, 0x10, "1 Coin  6 Plays"        },
	
	{0   , 0xfe, 0   , 2   , "Flip Screen"            },
	{0x00, 0x01, 0x40, 0x40, "Off"                    },
	{0x00, 0x01, 0x40, 0x00, "On"                     },
	
	{0   , 0xfe, 0   , 2   , "Cabinet"                },
	{0x00, 0x01, 0x80, 0x00, "Upright"                },
	{0x00, 0x01, 0x80, 0x80, "Cocktail"               },
	
	// Dip 2
	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x01, 0x01, 0x03, 0x00, "2"                      },
	{0x01, 0x01, 0x03, 0x03, "3"                      },
	{0x01, 0x01, 0x03, 0x02, "4"                      },
	{0x01, 0x01, 0x03, 0x01, "5"                      },
	
	{0   , 0xfe, 0   , 4   , "Difficulty"             },
	{0x01, 0x01, 0x0c, 0x08, "Easy"                   },
	{0x01, 0x01, 0x0c, 0x0c, "Normal"                 },
	{0x01, 0x01, 0x0c, 0x04, "Hard"                   },
	{0x01, 0x01, 0x0c, 0x00, "Hardest"                },
	
	{0   , 0xfe, 0   , 2   , "Game Change Mode"       },
	{0x01, 0x01, 0x20, 0x20, "Part 1"                 },
	{0x01, 0x01, 0x20, 0x00, "Part 2"                 },
	
	{0   , 0xfe, 0   , 2   , "Allow continue"         },
	{0x01, 0x01, 0x40, 0x00, "No"                     },
	{0x01, 0x01, 0x40, 0x40, "Yes"                    },
	
	{0   , 0xfe, 0   , 2   , "Demo Sounds"            },
	{0x01, 0x01, 0x00, 0x80, "Off"                    },
	{0x01, 0x01, 0x00, 0x00, "On"                     },
};

STDDIPINFO(Bouldash)

static struct BurnDIPInfo HbarrelDIPList[]=
{
	DIP_OFFSET(0x20)

	// Default Values
	{0x00, 0xff, 0xff, 0xff, NULL                     },
	{0x01, 0xff, 0xff, 0xbf, NULL                     },
	{0x02, 0xff, 0xff, 0x00, NULL                     },

	// Dip 1
	{0   , 0xfe, 0   , 4   , "Coin A"                 },
	{0x00, 0x01, 0x03, 0x00, "2 Coins 1 Play"         },
	{0x00, 0x01, 0x03, 0x03, "1 Coin  1 Play"         },
	{0x00, 0x01, 0x03, 0x02, "1 Coin  2 Plays"        },
	{0x00, 0x01, 0x03, 0x01, "1 Coin  3 Plays"        },
	
	{0   , 0xfe, 0   , 4   , "Coin B"                 },
	{0x00, 0x01, 0x0c, 0x00, "2 Coins 1 Play"         },
	{0x00, 0x01, 0x0c, 0x0c, "1 Coin  1 Play"         },
	{0x00, 0x01, 0x0c, 0x08, "1 Coin  2 Plays"        },
	{0x00, 0x01, 0x0c, 0x04, "1 Coin  3 Plays"        },
	
	{0   , 0xfe, 0   , 2   , "Service Mode"           },
	{0x00, 0x01, 0x10, 0x10, "Off"                    },
	{0x00, 0x01, 0x10, 0x00, "On"                     },
	
	{0   , 0xfe, 0   , 2   , "Demo Sounds"            },
	{0x00, 0x01, 0x20, 0x00, "Off"                    },
	{0x00, 0x01, 0x20, 0x20, "On"                     },
	
	{0   , 0xfe, 0   , 2   , "Flip Screen"            },
	{0x00, 0x01, 0x40, 0x40, "Off"                    },
	{0x00, 0x01, 0x40, 0x00, "On"                     },
	
	// Dip 2
	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x01, 0x01, 0x03, 0x01, "1"                      },
	{0x01, 0x01, 0x03, 0x03, "3"                      },
	{0x01, 0x01, 0x03, 0x02, "5"                      },
	{0x01, 0x01, 0x03, 0x00, "Infinite"               },
	
	{0   , 0xfe, 0   , 4   , "Difficulty"             },
	{0x01, 0x01, 0x0c, 0x08, "Easy"                   },
	{0x01, 0x01, 0x0c, 0x0c, "Normal"                 },
	{0x01, 0x01, 0x0c, 0x04, "Hard"                   },
	{0x01, 0x01, 0x0c, 0x00, "Hardest"                },
	
	{0   , 0xfe, 0   , 4   , "Bonus Life"             },
	{0x01, 0x01, 0x30, 0x30, "30k   80k 160k"         },
	{0x01, 0x01, 0x30, 0x20, "50k  120k 190k"         },
	{0x01, 0x01, 0x30, 0x10, "100k 200k 300k"         },
	{0x01, 0x01, 0x30, 0x00, "150k 300k 450k"         },
	
	{0   , 0xfe, 0   , 2   , "Allow continue"         },
	{0x01, 0x01, 0x40, 0x40, "No"                     },
	{0x01, 0x01, 0x40, 0x00, "Yes"                    },

	// Dip 3
	{0   , 0xfe, 0   , 2   , "Second Stick"           },
	{0x02, 0x01, 0x01, 0x00, "Moves & Shoots"         },
	{0x02, 0x01, 0x01, 0x01, "Moves"                  },
};

STDDIPINFO(Hbarrel)

static struct BurnDIPInfo HippodrmDIPList[]=
{
	DIP_OFFSET(0x18)

	// Default Values
	{0x00, 0xff, 0xff, 0xff, NULL                     },
	{0x01, 0xff, 0xff, 0xff, NULL                     },
	
	// Dip 1
	{0   , 0xfe, 0   , 4   , "Coin A"                 },
	{0x00, 0x01, 0x03, 0x00, "3 Coins 1 Play"         },
	{0x00, 0x01, 0x03, 0x01, "2 Coins 1 Play"         },
	{0x00, 0x01, 0x03, 0x03, "1 Coin  1 Play"         },
	{0x00, 0x01, 0x03, 0x02, "1 Coin  2 Plays"        },
	
	{0   , 0xfe, 0   , 4   , "Coin B"                 },
	{0x00, 0x01, 0x0c, 0x00, "3 Coins 1 Play"         },
	{0x00, 0x01, 0x0c, 0x04, "2 Coins 1 Play"         },
	{0x00, 0x01, 0x0c, 0x0c, "1 Coin  1 Play"         },
	{0x00, 0x01, 0x0c, 0x08, "1 Coin  2 Plays"        },
	
	{0   , 0xfe, 0   , 2   , "Demo Sounds"            },
	{0x00, 0x01, 0x20, 0x00, "Off"                    },
	{0x00, 0x01, 0x20, 0x20, "On"                     },
	
	{0   , 0xfe, 0   , 2   , "Flip Screen"            },
	{0x00, 0x01, 0x40, 0x40, "Off"                    },
	{0x00, 0x01, 0x40, 0x00, "On"                     },
	
	// Dip 2
	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x01, 0x01, 0x03, 0x01, "1"                      },
	{0x01, 0x01, 0x03, 0x03, "2"                      },
	{0x01, 0x01, 0x03, 0x02, "3"                      },
	{0x01, 0x01, 0x03, 0x00, "5"                      },
	
	{0   , 0xfe, 0   , 4   , "Difficulty"             },
	{0x01, 0x01, 0x0c, 0x08, "Easy"                   },
	{0x01, 0x01, 0x0c, 0x0c, "Normal"                 },
	{0x01, 0x01, 0x0c, 0x04, "Hard"                   },
	{0x01, 0x01, 0x0c, 0x00, "Hardest"                },
	
	{0   , 0xfe, 0   , 4   , "Player & Enemy Energy"  },
	{0x01, 0x01, 0x30, 0x10, "Very Low"               },
	{0x01, 0x01, 0x30, 0x20, "Low"                    },
	{0x01, 0x01, 0x30, 0x30, "Medium"                 },
	{0x01, 0x01, 0x30, 0x00, "High"                   },
	
	{0   , 0xfe, 0   , 2   , "Energy Power Decrease on Continue"},
	{0x01, 0x01, 0x40, 0x40, "2 Dots"                 },
	{0x01, 0x01, 0x40, 0x00, "3 Dots"                 },
};

STDDIPINFO(Hippodrm)

static struct BurnDIPInfo FfantasyDIPList[]=
{
	DIP_OFFSET(0x18)

	// Default Values
	{0x00, 0xff, 0xff, 0xff, NULL                     },
	{0x01, 0xff, 0xff, 0xff, NULL                     },
	
	// Dip 1
	{0   , 0xfe, 0   , 4   , "Coin A"                 },
	{0x00, 0x01, 0x03, 0x00, "3 Coins 1 Play"         },
	{0x00, 0x01, 0x03, 0x01, "2 Coins 1 Play"         },
	{0x00, 0x01, 0x03, 0x03, "1 Coin  1 Play"         },
	{0x00, 0x01, 0x03, 0x02, "1 Coin  2 Plays"        },
	
	{0   , 0xfe, 0   , 4   , "Coin B"                 },
	{0x00, 0x01, 0x0c, 0x00, "3 Coins 1 Play"         },
	{0x00, 0x01, 0x0c, 0x04, "2 Coins 1 Play"         },
	{0x00, 0x01, 0x0c, 0x0c, "1 Coin  1 Play"         },
	{0x00, 0x01, 0x0c, 0x08, "1 Coin  2 Plays"        },
	
	{0   , 0xfe, 0   , 2   , "Demo Sounds"            },
	{0x00, 0x01, 0x20, 0x00, "Off"                    },
	{0x00, 0x01, 0x20, 0x20, "On"                     },
	
	{0   , 0xfe, 0   , 2   , "Flip Screen"            },
	{0x00, 0x01, 0x40, 0x40, "Off"                    },
	{0x00, 0x01, 0x40, 0x00, "On"                     },
	
	// Dip 2
	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x01, 0x01, 0x03, 0x01, "1"                      },
	{0x01, 0x01, 0x03, 0x03, "2"                      },
	{0x01, 0x01, 0x03, 0x02, "3"                      },
	{0x01, 0x01, 0x03, 0x00, "5"                      },
	
	{0   , 0xfe, 0   , 4   , "Difficulty"             },
	{0x01, 0x01, 0x0c, 0x08, "Easy"                   },
	{0x01, 0x01, 0x0c, 0x0c, "Normal"                 },
	{0x01, 0x01, 0x0c, 0x04, "Hard"                   },
	{0x01, 0x01, 0x0c, 0x00, "Hardest"                },
	
	{0   , 0xfe, 0   , 4   , "Player & Enemy Energy"  },
	{0x01, 0x01, 0x30, 0x10, "Very Low"               },
	{0x01, 0x01, 0x30, 0x20, "Low"                    },
	{0x01, 0x01, 0x30, 0x30, "Medium"                 },
	{0x01, 0x01, 0x30, 0x00, "High"                   },
	
	{0   , 0xfe, 0   , 2   , "Energy Power Decrease on Continue"},
	{0x01, 0x01, 0x40, 0x40, "2 Dots"                 },
	{0x01, 0x01, 0x40, 0x00, "None"                   },
};

STDDIPINFO(Ffantasy)

static struct BurnDIPInfo MidresDIPList[]=
{
	DIP_OFFSET(0x22)

	// Default Values
	{0x00, 0xff, 0xff, 0xff, NULL                     },
	{0x01, 0xff, 0xff, 0xbf, NULL                     },
	{0x02, 0xff, 0xff, 0x00, NULL                     },

	// Dip 1
	{0   , 0xfe, 0   , 4   , "Coin A"                 },
	{0x00, 0x01, 0x03, 0x00, "3 Coins 1 Play"         },
	{0x00, 0x01, 0x03, 0x01, "2 Coins 1 Play"         },
	{0x00, 0x01, 0x03, 0x03, "1 Coin  1 Play"         },
	{0x00, 0x01, 0x03, 0x02, "1 Coin  2 Plays"        },
	
	{0   , 0xfe, 0   , 4   , "Coin B"                 },
	{0x00, 0x01, 0x0c, 0x00, "3 Coins 1 Play"         },
	{0x00, 0x01, 0x0c, 0x04, "2 Coins 1 Play"         },
	{0x00, 0x01, 0x0c, 0x0c, "1 Coin  1 Play"         },
	{0x00, 0x01, 0x0c, 0x08, "1 Coin  2 Plays"        },
	
	{0   , 0xfe, 0   , 2   , "Service Mode"           },
	{0x00, 0x01, 0x10, 0x10, "Off"                    },
	{0x00, 0x01, 0x10, 0x00, "On"                     },
	
	{0   , 0xfe, 0   , 2   , "Demo Sounds"            },
	{0x00, 0x01, 0x20, 0x00, "Off"                    },
	{0x00, 0x01, 0x20, 0x20, "On"                     },
	
	{0   , 0xfe, 0   , 2   , "Flip Screen"            },
	{0x00, 0x01, 0x40, 0x40, "Off"                    },
	{0x00, 0x01, 0x40, 0x00, "On"                     },
	
	// Dip 2
	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x01, 0x01, 0x03, 0x01, "1"                      },
	{0x01, 0x01, 0x03, 0x03, "3"                      },
	{0x01, 0x01, 0x03, 0x02, "5"                      },
	{0x01, 0x01, 0x03, 0x00, "Infinite"               },
	
	{0   , 0xfe, 0   , 4   , "Difficulty"             },
	{0x01, 0x01, 0x0c, 0x08, "Easy"                   },
	{0x01, 0x01, 0x0c, 0x0c, "Normal"                 },
	{0x01, 0x01, 0x0c, 0x04, "Hard"                   },
	{0x01, 0x01, 0x0c, 0x00, "Hardest"                },
	
	{0   , 0xfe, 0   , 2   , "Allow continue"         },
	{0x01, 0x01, 0x40, 0x40, "No"                     },
	{0x01, 0x01, 0x40, 0x00, "Yes"                    },

	// Dip 3
	{0   , 0xfe, 0   , 2   , "Second Stick"           },
	{0x02, 0x01, 0x01, 0x00, "Moves & Shoots"         },
	{0x02, 0x01, 0x01, 0x01, "Moves"                  },
};

STDDIPINFO(Midres)

static struct BurnDIPInfo RobocopDIPList[]=
{
	DIP_OFFSET(0x18)

	// Default Values
	{0x00, 0xff, 0xff, 0x7f, NULL                     },
	{0x01, 0xff, 0xff, 0xff, NULL                     },
	
	// Dip 1
	{0   , 0xfe, 0   , 4   , "Coin A"                 },
	{0x00, 0x01, 0x03, 0x00, "3 Coins 1 Play"         },
	{0x00, 0x01, 0x03, 0x01, "2 Coins 1 Play"         },
	{0x00, 0x01, 0x03, 0x03, "1 Coin  1 Play"         },
	{0x00, 0x01, 0x03, 0x02, "1 Coin  2 Plays"        },
	
	{0   , 0xfe, 0   , 4   , "Coin B"                 },
	{0x00, 0x01, 0x0c, 0x00, "3 Coins 1 Play"         },
	{0x00, 0x01, 0x0c, 0x04, "2 Coins 1 Play"         },
	{0x00, 0x01, 0x0c, 0x0c, "1 Coin  1 Play"         },
	{0x00, 0x01, 0x0c, 0x08, "1 Coin  2 Plays"        },
	
	{0   , 0xfe, 0   , 2   , "Demo Sounds"            },
	{0x00, 0x01, 0x20, 0x00, "Off"                    },
	{0x00, 0x01, 0x20, 0x20, "On"                     },
	
	{0   , 0xfe, 0   , 2   , "Flip Screen"            },
	{0x00, 0x01, 0x40, 0x40, "Off"                    },
	{0x00, 0x01, 0x40, 0x00, "On"                     },
	
	{0   , 0xfe, 0   , 2   , "Cabinet"                },
	{0x00, 0x01, 0x80, 0x80, "Upright"                },
	{0x00, 0x01, 0x80, 0x00, "Cocktail"               },
	
	// Dip 2
	{0   , 0xfe, 0   , 4   , "Player Energy"          },
	{0x01, 0x01, 0x03, 0x01, "Low"                    },
	{0x01, 0x01, 0x03, 0x03, "Medium"                 },
	{0x01, 0x01, 0x03, 0x02, "High"                   },
	{0x01, 0x01, 0x03, 0x00, "Very High"              },
	
	{0   , 0xfe, 0   , 4   , "Difficulty"             },
	{0x01, 0x01, 0x0c, 0x08, "Easy"                   },
	{0x01, 0x01, 0x0c, 0x0c, "Normal"                 },
	{0x01, 0x01, 0x0c, 0x04, "Hard"                   },
	{0x01, 0x01, 0x0c, 0x00, "Hardest"                },
	
	{0   , 0xfe, 0   , 2   , "Allow continue"         },
	{0x01, 0x01, 0x10, 0x10, "Yes"                    },
	{0x01, 0x01, 0x10, 0x00, "No"                     },
	
	{0   , 0xfe, 0   , 2   , "Bonus Stage Energy"     },
	{0x01, 0x01, 0x20, 0x00, "Low"                    },
	{0x01, 0x01, 0x20, 0x20, "High"                   },
	
	{0   , 0xfe, 0   , 2   , "Brink Time"             },
	{0x01, 0x01, 0x40, 0x40, "Normal"                 },
	{0x01, 0x01, 0x40, 0x00, "Less"                   },
};

STDDIPINFO(Robocop)

static struct BurnDIPInfo SlyspyDIPList[]=
{
	DIP_OFFSET(0x14)

	// Default Values
	{0x00, 0xff, 0xff, 0x7f, NULL                     },
	{0x01, 0xff, 0xff, 0xff, NULL                     },

	// Dip 1
	{0   , 0xfe, 0   , 4   , "Coin A"                 },
	{0x00, 0x01, 0x03, 0x00, "3 Coins 1 Play"         },
	{0x00, 0x01, 0x03, 0x01, "2 Coins 1 Play"         },
	{0x00, 0x01, 0x03, 0x03, "1 Coin  1 Play"         },
	{0x00, 0x01, 0x03, 0x02, "1 Coin  2 Plays"        },
	
	{0   , 0xfe, 0   , 4   , "Coin B"                 },
	{0x00, 0x01, 0x0c, 0x00, "3 Coins 1 Play"         },
	{0x00, 0x01, 0x0c, 0x04, "2 Coins 1 Play"         },
	{0x00, 0x01, 0x0c, 0x0c, "1 Coin  1 Play"         },
	{0x00, 0x01, 0x0c, 0x08, "1 Coin  2 Plays"        },
	
	{0   , 0xfe, 0   , 2   , "Service Mode"           },
	{0x00, 0x01, 0x10, 0x10, "Off"                    },
	{0x00, 0x01, 0x10, 0x00, "On"                     },
	
	{0   , 0xfe, 0   , 2   , "Demo Sounds"            },
	{0x00, 0x01, 0x20, 0x00, "Off"                    },
	{0x00, 0x01, 0x20, 0x20, "On"                     },
	
	{0   , 0xfe, 0   , 2   , "Flip Screen"            },
	{0x00, 0x01, 0x40, 0x40, "Off"                    },
	{0x00, 0x01, 0x40, 0x00, "On"                     },
	
	{0   , 0xfe, 0   , 2   , "Cabinet"                },
	{0x00, 0x01, 0x80, 0x00, "Upright"                },
	{0x00, 0x01, 0x80, 0x80, "Cocktail"               },
	
	// Dip 2
	{0   , 0xfe, 0   , 4   , "Energy"                 },
	{0x01, 0x01, 0x03, 0x02, "8 bars"                 },
	{0x01, 0x01, 0x03, 0x03, "10 bars"                },
	{0x01, 0x01, 0x03, 0x01, "12 bars"                },
	{0x01, 0x01, 0x03, 0x00, "14 bars"                },
	
	{0   , 0xfe, 0   , 4   , "Difficulty"             },
	{0x01, 0x01, 0x0c, 0x08, "Easy"                   },
	{0x01, 0x01, 0x0c, 0x0c, "Normal"                 },
	{0x01, 0x01, 0x0c, 0x04, "Hard"                   },
	{0x01, 0x01, 0x0c, 0x00, "Hardest"                },
	
	{0   , 0xfe, 0   , 2   , "Allow continue"         },
	{0x01, 0x01, 0x10, 0x00, "No"                     },
	{0x01, 0x01, 0x10, 0x10, "Yes"                    },
};

STDDIPINFO(Slyspy)

// Bad Dudes vs. Dragonninja (US revision 1)
// DE-0297-1 main board, DE-0299-1 sub/rom board 

static struct BurnRomInfo BaddudesRomDesc[] = {
	{ "ei04-1.3c",  	0x10000, 0x4bf158a7, BRF_ESS | BRF_PRG },	//  0	68000 Program Code
	{ "ei01-1.3a",      0x10000, 0x74f5110c, BRF_ESS | BRF_PRG },	//  1
	{ "ei06.6c",        0x10000, 0x3ff8da57, BRF_ESS | BRF_PRG },	//  2
	{ "ei03.6a",        0x10000, 0xf8f2bd94, BRF_ESS | BRF_PRG },	//  3
	
	{ "ei07.8a",        0x08000, 0x9fb1ef4b, BRF_ESS | BRF_PRG },	//  4	6502 Program 
	
	{ "ei25.15h",       0x08000, 0xbcf59a69, BRF_GRA },				//  5	Characters
	{ "ei26.16h",       0x08000, 0x9aff67b8, BRF_GRA },				//  6
	
	{ "ei18.14d",       0x10000, 0x05cfc3e5, BRF_GRA },				//  7	Tiles 1
	{ "ei20.17d",       0x10000, 0xe11e988f, BRF_GRA },				//  8
	{ "ei22.14f",       0x10000, 0xb893d880, BRF_GRA },				//  9
	{ "ei24.17f",       0x10000, 0x6f226dda, BRF_GRA },				// 10
	
	{ "ei30.9h",        0x10000, 0x982da0d1, BRF_GRA },				// 11	Tiles 2
	{ "ei28.9f",        0x10000, 0xf01ebb3b, BRF_GRA },				// 12
	
	{ "ei15.16c",       0x10000, 0xa38a7d30, BRF_GRA },				// 13	Sprites
	{ "ei16.17c",       0x08000, 0x17e42633, BRF_GRA },				// 14
	{ "ei11.16a",       0x10000, 0x3a77326c, BRF_GRA },				// 15
	{ "ei12.17a",       0x08000, 0xfea2a134, BRF_GRA },				// 16
	{ "ei13.13c",       0x10000, 0xe5ae2751, BRF_GRA },				// 17
	{ "ei14.14c",       0x08000, 0xe83c760a, BRF_GRA },				// 18
	{ "ei09.13a",       0x10000, 0x6901e628, BRF_GRA },				// 19
	{ "ei10.14a",       0x08000, 0xeeee8a1a, BRF_GRA },				// 20
	
	{ "ei08.2c",        0x10000, 0x3c87463e, BRF_SND },				// 21	Samples
	
	{ "ei31.9a",        0x01000, 0x2a8745d2, BRF_OPT },				// 22	I8751
	
	{ "mb7116e.12c",	0x00200, 0x86e775f8, BRF_OPT }, 			// 23 PROMs
	{ "mb7122e.17e",	0x00400, 0xa5cda23e, BRF_OPT }, 			// 24
};

STD_ROM_PICK(Baddudes)
STD_ROM_FN(Baddudes)


// Birdie Try (Japan revision 2, revision 1 MCU)
// DE-0311-0 main board, DE-0299-2 sub/rom board 

static struct BurnRomInfo birdtryRomDesc[] = {
	{ "ek-04-2.3c",		0x10000, 0x5f0f4686, 1 }, //  0 maincpu
	{ "ek-01-2.3a",		0x10000, 0x47f470db, 1 }, //  1
	{ "ek-05-1.4c",		0x10000, 0xb508cffd, 1 }, //  2
	{ "ek-02-1.4a",		0x10000, 0x0195d989, 1 }, //  3
	{ "ek-06-1.6c",		0x10000, 0x301d57d8, 1 }, //  4
	{ "ek-03-1.6a",		0x10000, 0x73b0acc5, 1 }, //  5

	{ "ek-07.8a",		0x08000, 0x236549bc, 2 }, //  6 audiocpu

	{ "ek-31-1.9a",		0x01000, 0x3bf41abb, 3 }, //  7 mcu

	{ "ek-25.15h",		0x08000, 0x4df134ad, 4 }, //  8 gfx1
	{ "ek-26.16h",		0x08000, 0xa00d3e8e, 4 }, //  9

	{ "ek-18.14d",		0x10000, 0x9886fb70, 5 }, // 10 gfx2
	{ "ek-17.12d",		0x10000, 0xbed91bf7, 5 }, // 11
	{ "ek-20.17d",		0x10000, 0x45d53965, 5 }, // 12
	{ "ek-19.15d",		0x10000, 0xc2949dd2, 5 }, // 13
	{ "ek-22.14f",		0x10000, 0x7f2cc80a, 5 }, // 14
	{ "ek-21.12f",		0x10000, 0x281bc793, 5 }, // 15
	{ "ek-24.17f",		0x10000, 0x2244cc75, 5 }, // 16
	{ "ek-23.15f",		0x10000, 0xd0ed0116, 5 }, // 17

	{ "ek-15.16c",		0x10000, 0xa6a041a3, 6 }, // 18 gfx4
	{ "ek-16.17c",		0x08000, 0x784f62b0, 6 }, // 19
	{ "ek-11.16a",		0x10000, 0x9224a6b9, 6 }, // 20
	{ "ek-12.17a",		0x08000, 0x12deecfa, 6 }, // 21
	{ "ek-13.13c",		0x10000, 0x1f023459, 6 }, // 22
	{ "ek-14.14c",		0x08000, 0x57d54943, 6 }, // 23
	{ "ek-09.13a",		0x10000, 0x6d2d488a, 6 }, // 24
	{ "ek-10.14a",		0x08000, 0x580ba206, 6 }, // 25

	{ "ek-08.2c",		0x10000, 0xbe3db6cb, 7 }, // 26 oki
	
	{ "mb7116e.12c",	0x00200, 0x86e775f8, 0 }, // 27 PROMs
	{ "mb7122e.17e",	0x00400, 0xa5cda23e, 0 }, // 28
};

STD_ROM_PICK(birdtry)
STD_ROM_FN(birdtry)


// Birdie Try (Japan revision 2)
// DE-0311-0 main board, DE-0299-2 sub/rom board 

static struct BurnRomInfo birdtryaRomDesc[] = {
	{ "ek-04-2.3c",		0x10000, 0x5f0f4686, 1 }, //  0 maincpu
	{ "ek-01-2.3a",		0x10000, 0x47f470db, 1 }, //  1
	{ "ek-05-1.4c",		0x10000, 0xb508cffd, 1 }, //  2
	{ "ek-02-1.4a",		0x10000, 0x0195d989, 1 }, //  3
	{ "ek-06-1.6c",		0x10000, 0x301d57d8, 1 }, //  4
	{ "ek-03-1.6a",		0x10000, 0x73b0acc5, 1 }, //  5

	{ "ek-07.8a",		0x08000, 0x236549bc, 2 }, //  6 audiocpu

	{ "ek-31.9a",		0x01000, 0x68831ae9, 3 }, //  7 mcu

	{ "ek-25.15h",		0x08000, 0x4df134ad, 4 }, //  8 gfx1
	{ "ek-26.16h",		0x08000, 0xa00d3e8e, 4 }, //  9

	{ "ek-18.14d",		0x10000, 0x9886fb70, 5 }, // 10 gfx2
	{ "ek-17.12d",		0x10000, 0xbed91bf7, 5 }, // 11
	{ "ek-20.17d",		0x10000, 0x45d53965, 5 }, // 12
	{ "ek-19.15d",		0x10000, 0xc2949dd2, 5 }, // 13
	{ "ek-22.14f",		0x10000, 0x7f2cc80a, 5 }, // 14
	{ "ek-21.12f",		0x10000, 0x281bc793, 5 }, // 15
	{ "ek-24.17f",		0x10000, 0x2244cc75, 5 }, // 16
	{ "ek-23.15f",		0x10000, 0xd0ed0116, 5 }, // 17

	{ "ek-15.16c",		0x10000, 0xa6a041a3, 6 }, // 18 gfx4
	{ "ek-16.17c",		0x08000, 0x784f62b0, 6 }, // 19
	{ "ek-11.16a",		0x10000, 0x9224a6b9, 6 }, // 20
	{ "ek-12.17a",		0x08000, 0x12deecfa, 6 }, // 21
	{ "ek-13.13c",		0x10000, 0x1f023459, 6 }, // 22
	{ "ek-14.14c",		0x08000, 0x57d54943, 6 }, // 23
	{ "ek-09.13a",		0x10000, 0x6d2d488a, 6 }, // 24
	{ "ek-10.14a",		0x08000, 0x580ba206, 6 }, // 25

	{ "ek-08.2c",		0x10000, 0xbe3db6cb, 7 }, // 26 oki
	
	{ "mb7116e.12c",	0x00200, 0x86e775f8, 0 }, // 27 PROMs
	{ "mb7122e.17e",	0x00400, 0xa5cda23e, 0 }, // 28
};

STD_ROM_PICK(birdtrya)
STD_ROM_FN(birdtrya)


// Birdie Try (Japan revision S)
// DE-0311-0 main board, DE-0299-2 sub/rom board - All ROMs and the MCU clearly stamped with "S"

static struct BurnRomInfo birdtrybRomDesc[] = {
	{ "ek-04-s.3c",		0x10000, 0xbdc7c617, 1 }, //  0 maincpu
	{ "ek-01-s.3a",		0x10000, 0x2d36f998, 1 }, //  1
	{ "ek-05-s.4c",		0x10000, 0x2e0226df, 1 }, //  2
	{ "ek-02-s.4a",		0x10000, 0x7a3133cc, 1 }, //  3
	{ "ek-06-s.6c",		0x10000, 0x301d57d8, 1 }, //  4
	{ "ek-03-s.6a",		0x10000, 0xd36189f6, 1 }, //  5

	{ "ek-07-s.8a",		0x08000, 0x236549bc, 2 }, //  6 audiocpu

	{ "ek-31-s.9a",		0x01000, 0x68831ae9, 3 }, //  7 mcu

	{ "ek-25-s.15h",	0x08000, 0x4df134ad, 4 }, //  8 gfx1
	{ "ek-26-s.16h",	0x08000, 0xa00d3e8e, 4 }, //  9

	{ "ek-18-s.14d",	0x10000, 0x9886fb70, 5 }, // 10 gfx2
	{ "ek-17-s.12d",	0x10000, 0xbed91bf7, 5 }, // 11
	{ "ek-20-s.17d",	0x10000, 0x45d53965, 5 }, // 12
	{ "ek-19-s.15d",	0x10000, 0xc2949dd2, 5 }, // 13
	{ "ek-22-s.14f",	0x10000, 0x7f2cc80a, 5 }, // 14
	{ "ek-21-s.12f",	0x10000, 0x281bc793, 5 }, // 15
	{ "ek-24-s.17f",	0x10000, 0x2244cc75, 5 }, // 16
	{ "ek-23-s.15f",	0x10000, 0xd0ed0116, 5 }, // 17

	{ "ek-15-s.16c",	0x10000, 0xa6a041a3, 6 }, // 18 gfx4
	{ "ek-16-s.17c",	0x08000, 0x784f62b0, 6 }, // 19
	{ "ek-11-s.16a",	0x10000, 0x9224a6b9, 6 }, // 20
	{ "ek-12-s.17a",	0x08000, 0x12deecfa, 6 }, // 21
	{ "ek-13-s.13c",	0x10000, 0x1f023459, 6 }, // 22
	{ "ek-14-s.14c",	0x08000, 0x57d54943, 6 }, // 23
	{ "ek-09-s.13a",	0x10000, 0x6d2d488a, 6 }, // 24
	{ "ek-10-s.14a",	0x08000, 0x580ba206, 6 }, // 25

	{ "ek-08-s.2c",		0x10000, 0xbe3db6cb, 7 }, // 26 oki
	
	{ "mb7116e.12c",	0x00200, 0x86e775f8, 0 }, // 27 PROMs
	{ "mb7122e.17e",	0x00400, 0xa5cda23e, 0 }, // 28
};

STD_ROM_PICK(birdtryb)
STD_ROM_FN(birdtryb)


// Boulder Dash / Boulder Dash Part 2 (World)

static struct BurnRomInfo BouldashRomDesc[] = {
	{ "fw-15-2.17l",        0x10000, 0xca19a967, BRF_ESS | BRF_PRG },	//  0	68000 Program Code
	{ "fw-12-2.9l",         0x10000, 0x242bdc2a, BRF_ESS | BRF_PRG },	//  1
	{ "fw-16-2.19l",        0x10000, 0xb7217265, BRF_ESS | BRF_PRG },	//  2
	{ "fw-13-2.11l",        0x10000, 0x19209ef4, BRF_ESS | BRF_PRG },	//  3
	{ "fw-17-2.20l",        0x10000, 0x78a632a1, BRF_ESS | BRF_PRG },	//  4
	{ "fw-14-2.13l",        0x10000, 0x69b6112d, BRF_ESS | BRF_PRG },	//  5
	
	{ "fn-10",              0x10000, 0xc74106e7, BRF_ESS | BRF_PRG },	//  6	HuC6280 Program
	
	{ "fn-04",              0x10000, 0x40f5a760, BRF_GRA },				//  7	Characters
	{ "fn-05",              0x10000, 0x824f2168, BRF_GRA },				//  8

	{ "fn-07",              0x10000, 0xeac6a3b3, BRF_GRA },				//  9	Tiles 1
	{ "fn-06",              0x10000, 0x3feee292, BRF_GRA },				// 10
	
	{ "fn-09",              0x20000, 0xc2b27bd2, BRF_GRA },				// 11	Tiles 2
	{ "fn-08",              0x20000, 0x5ac97178, BRF_GRA },				// 12
	
	{ "fn-01",              0x10000, 0x9333121b, BRF_GRA },				// 13	Sprites
	{ "fn-03",              0x10000, 0x254ba60f, BRF_GRA },				// 14
	{ "fn-00",              0x10000, 0xec18d098, BRF_GRA },				// 15
	{ "fn-02",              0x10000, 0x4f060cba, BRF_GRA },				// 16
	
	{ "fn-11",              0x10000, 0x990fd8d9, BRF_SND },				// 17	Samples
	
	{ "ta-16.21k",          0x00100, 0xad26e8d4, BRF_OPT},				// 18	PROMs
};

STD_ROM_PICK(Bouldash)
STD_ROM_FN(Bouldash)


// Boulder Dash / Boulder Dash Part 2 (Japan)

static struct BurnRomInfo BouldashjRomDesc[] = {
	{ "fn-15-1.17l",        0x10000, 0xd3ef20f8, BRF_ESS | BRF_PRG },	//  0	68000 Program Code
	{ "fn-12-1.9l",         0x10000, 0xf4a10b45, BRF_ESS | BRF_PRG },	//  1
	{ "fn-16-.19l",         0x10000, 0xfd1806a5, BRF_ESS | BRF_PRG },	//  2
	{ "fn-13-.11l",         0x10000, 0xd24d3681, BRF_ESS | BRF_PRG },	//  3
	{ "fn-17-.20l",         0x10000, 0x28d48a37, BRF_ESS | BRF_PRG },	//  4
	{ "fn-14-.13l",         0x10000, 0x8c61c682, BRF_ESS | BRF_PRG },	//  5
	
	{ "fn-10",              0x10000, 0xc74106e7, BRF_ESS | BRF_PRG },	//  6	HuC6280 Program
	
	{ "fn-04",              0x10000, 0x40f5a760, BRF_GRA },				//  7	Characters
	{ "fn-05",              0x10000, 0x824f2168, BRF_GRA },				//  8

	{ "fn-07",              0x10000, 0xeac6a3b3, BRF_GRA },				//  9	Tiles 1
	{ "fn-06",              0x10000, 0x3feee292, BRF_GRA },				// 10
	
	{ "fn-09",              0x20000, 0xc2b27bd2, BRF_GRA },				// 11	Tiles 2
	{ "fn-08",              0x20000, 0x5ac97178, BRF_GRA },				// 12
	
	{ "fn-01",              0x10000, 0x9333121b, BRF_GRA },				// 13	Sprites
	{ "fn-03",              0x10000, 0x254ba60f, BRF_GRA },				// 14
	{ "fn-00",              0x10000, 0xec18d098, BRF_GRA },				// 15
	{ "fn-02",              0x10000, 0x4f060cba, BRF_GRA },				// 16
	
	{ "fn-11",              0x10000, 0x990fd8d9, BRF_SND },				// 17	Samples
	
	{ "ta-16.21k",          0x00100, 0xad26e8d4, BRF_OPT},				// 18	PROMs
};

STD_ROM_PICK(Bouldashj)
STD_ROM_FN(Bouldashj)


// Dragonninja (Japan revision 1)
// DE-0297-0 main board, DE-0299-0 sub/rom board 

static struct BurnRomInfo DrgninjaRomDesc[] = {
	{ "eg04-1.3c",          0x10000, 0x41b8b3f8, BRF_ESS | BRF_PRG },	//  0	68000 Program Code
	{ "eg01-1.3a",          0x10000, 0xe08e6885, BRF_ESS | BRF_PRG },	//  1
	{ "eg06.6c",            0x10000, 0x2b81faf7, BRF_ESS | BRF_PRG },	//  2
	{ "eg03.6a",            0x10000, 0xc52c2e9d, BRF_ESS | BRF_PRG },	//  3
	
	{ "eg07.8a",            0x08000, 0x001d2f51, BRF_ESS | BRF_PRG },	//  4	6502 Program 
	
	{ "eg25.15h",           0x08000, 0x6791bc20, BRF_GRA },				//  5	Characters
	{ "eg26.16h",           0x08000, 0x5d75fc8f, BRF_GRA },				//  6
	
	{ "eg18.14d",           0x10000, 0x05cfc3e5, BRF_GRA },				//  7	Tiles 1
	{ "eg20.17d",           0x10000, 0xe11e988f, BRF_GRA },				//  8
	{ "eg22.14f",           0x10000, 0xb893d880, BRF_GRA },				//  9
	{ "eg24.17f",           0x10000, 0x6f226dda, BRF_GRA },				// 10
	
	{ "eg30.9h",            0x10000, 0x2438e67e, BRF_GRA },				// 11	Tiles 2
	{ "eg28.9f",            0x10000, 0x5c692ab3, BRF_GRA },				// 12
	
	{ "eg15.16c",           0x10000, 0x5617d67f, BRF_GRA },				// 13	Sprites
	{ "eg16.17c",           0x08000, 0x17e42633, BRF_GRA },				// 14
	{ "eg11.16a",           0x10000, 0xba83e8d8, BRF_GRA },				// 15
	{ "eg12.17a",           0x08000, 0xfea2a134, BRF_GRA },				// 16
	{ "eg13.13c",           0x10000, 0xfd91e08e, BRF_GRA },				// 17
	{ "eg14.14c",           0x08000, 0xe83c760a, BRF_GRA },				// 18
	{ "eg09.13a",           0x10000, 0x601b7b23, BRF_GRA },				// 19
	{ "eg10.14a",           0x08000, 0xeeee8a1a, BRF_GRA },				// 20
	
	{ "eg08.2c",            0x10000, 0x92f2c916, BRF_SND },				// 21	Samples
	
	{ "eg31.9a",            0x01000, 0x657aab2d, BRF_OPT },				// 22	I8751
	
	{ "mb7116e.12c",		0x00200, 0x86e775f8, BRF_OPT }, 			// 23 PROMs
	{ "mb7122e.17e",		0x00400, 0xa5cda23e, BRF_OPT }, 			// 24
};

STD_ROM_PICK(Drgninja)
STD_ROM_FN(Drgninja)


// Dragonninja (bootleg)

static struct BurnRomInfo DrgninjabRomDesc[] = {
	{ "n-12.d2",            0x10000, 0x5a70eb52, BRF_ESS | BRF_PRG },	//  0	68000 Program Code
	{ "n-11.a2",            0x10000, 0x3887eb92, BRF_ESS | BRF_PRG },	//  1
	{ "eg06.6c",            0x10000, 0x2b81faf7, BRF_ESS | BRF_PRG },	//  2
	{ "eg03.6a",            0x10000, 0xc52c2e9d, BRF_ESS | BRF_PRG },	//  3
	
	{ "eg07.8a",            0x08000, 0x001d2f51, BRF_ESS | BRF_PRG },	//  4	6502 Program 
	
	{ "eg25.bin",           0x08000, 0xdd557b19, BRF_GRA },				//  5	Characters
	{ "eg26.16h",           0x08000, 0x5d75fc8f, BRF_GRA },				//  6
	
	{ "eg18.14d",           0x10000, 0x05cfc3e5, BRF_GRA },				//  7	Tiles 1
	{ "eg20.17d",           0x10000, 0xe11e988f, BRF_GRA },				//  8
	{ "eg22.14f",           0x10000, 0xb893d880, BRF_GRA },				//  9
	{ "eg24.17f",           0x10000, 0x6f226dda, BRF_GRA },				// 10
	
	{ "eg30.9h",            0x10000, 0x2438e67e, BRF_GRA },				// 11	Tiles 2
	{ "eg28.9f",            0x10000, 0x5c692ab3, BRF_GRA },				// 12
	
	{ "eg15.16c",           0x10000, 0x5617d67f, BRF_GRA },				// 13	Sprites
	{ "eg16.17c",           0x08000, 0x17e42633, BRF_GRA },				// 14
	{ "eg11.16a",           0x10000, 0xba83e8d8, BRF_GRA },				// 15
	{ "eg12.17a",           0x08000, 0xfea2a134, BRF_GRA },				// 16
	{ "eg13.13c",           0x10000, 0xfd91e08e, BRF_GRA },				// 17
	{ "eg14.14c",           0x08000, 0xe83c760a, BRF_GRA },				// 18
	{ "eg09.13a",           0x10000, 0x601b7b23, BRF_GRA },				// 19
	{ "eg10.14a",           0x08000, 0xeeee8a1a, BRF_GRA },				// 20
	
	{ "eg08.2c",            0x10000, 0x92f2c916, BRF_SND },				// 21	Samples
	
	{ "i8751",              0x01000, 0xc3f6bc70, BRF_OPT },				// 22	I8751
};

STD_ROM_PICK(Drgninjab)
STD_ROM_FN(Drgninjab)


// Dragonninja (bootleg with 68705)
/* f205v id 932
f205v's page shows that this uses an MSM5205 instead of the MSM6295, the MSM5205 data ROM isn't dumped yet, but the MSM6295 ROM is still
present on the board? For now I'm using the original M6502 program and MSM6295, pending getting a dump of the MSM5205 data
this set should also use a M68705 apparently, no dump available though */

static struct BurnRomInfo Drgninjab2RomDesc[] = {
	{ "a14.3e",             0x10000, 0xc4b9f4e7, BRF_ESS | BRF_PRG },	//  0	68000 Program Code
	{ "a11.3b",             0x10000, 0xe4cc7c60, BRF_ESS | BRF_PRG },	//  1
	{ "a12.2e",             0x10000, 0x2b81faf7, BRF_ESS | BRF_PRG },	//  2
	{ "a9.2b",              0x10000, 0xc52c2e9d, BRF_ESS | BRF_PRG },	//  3
	
	{ "a15.7b",             0x08000, 0x82007af2, BRF_ESS | BRF_PRG },	//  4	6502 Program (ADPCM sounds are wrong with this, see above, 1st half identical to orginal, 2nd half 99.225%)
	
	{ "a22.9m",             0x08000, 0x6791bc20, BRF_GRA },				//  5	Characters
	{ "a23.9n",             0x08000, 0x5d75fc8f, BRF_GRA },				//  6
	
	{ "a25.10f",            0x10000, 0x05cfc3e5, BRF_GRA },				//  7	Tiles 1
	{ "a27.10h",            0x10000, 0xe11e988f, BRF_GRA },				//  8
	{ "a24.10e",            0x10000, 0xb893d880, BRF_GRA },				//  9
	{ "a26.10g",            0x10000, 0x6f226dda, BRF_GRA },				// 10
	
	{ "a21.9k",             0x08000, 0xb2e989fc, BRF_GRA },				// 11	Tiles 2
	{ "a29.10k",            0x08000, 0x4bf80966, BRF_GRA },				// 12
	{ "a20.9j",             0x08000, 0xe71c0793, BRF_GRA },				// 13
	{ "a28.10j",            0x08000, 0x2d38032d, BRF_GRA },				// 14
	
	{ "a6.4g",              0x10000, 0x5617d67f, BRF_GRA },				// 15	Sprites
	{ "a2.4c",              0x08000, 0x17e42633, BRF_GRA },				// 16
	{ "a8.5g",              0x10000, 0xba83e8d8, BRF_GRA },				// 17
	{ "a4.5c",              0x08000, 0xfea2a134, BRF_GRA },				// 18
	{ "a5.3g",              0x10000, 0xfd91e08e, BRF_GRA },				// 19
	{ "a1.3c",              0x08000, 0xe83c760a, BRF_GRA },				// 20
	{ "a7.4-5g",            0x10000, 0x601b7b23, BRF_GRA },				// 21
	{ "a3.4-5c",            0x08000, 0xeeee8a1a, BRF_GRA },				// 22
	
	{ "a30.10b",            0x10000, 0xf6806826, BRF_SND },				// 23	Samples
	
	{ "68705r3.0m",         0x01000, 0x34bc5e7f, BRF_PRG },				// 24	I8751
	
	{ "n82s129an.12c",		0x00100, 0x78994fdb, BRF_OPT },				// 25   proms
	{ "n82s131n.5q",		0x00200, 0x86e775f8, BRF_OPT },
	{ "n82s129an.3p",		0x00100, 0x9f6aa3e5, BRF_OPT },
	{ "n82s137n.8u",		0x00400, 0xa5cda23e, BRF_OPT },
	{ "n82s129an.2q",		0x00100, 0xaf46d1ee, BRF_OPT },
};

STD_ROM_PICK(Drgninjab2)
STD_ROM_FN(Drgninjab2)


// Heavy Barrel (World)
// DE-0289-2 main board, DE-0293-1 sub/rom board 

static struct BurnRomInfo HbarrelRomDesc[] = {
	{ "ec04-e.3c",        	0x10000, 0xd01bc3db, BRF_ESS | BRF_PRG },	//  0	68000 Program Code
	{ "ec01-e.3a",        	0x10000, 0x6756f8ae, BRF_ESS | BRF_PRG },	//  1
	{ "ec05.4c",           	0x10000, 0x2087d570, BRF_ESS | BRF_PRG },	//  2
	{ "ec02.4a",           	0x10000, 0x815536ae, BRF_ESS | BRF_PRG },	//  3
	{ "ec06.6c",        	0x10000, 0x61ec20d8, BRF_ESS | BRF_PRG },	//  4
	{ "ec03.6a",        	0x10000, 0x720c6b13, BRF_ESS | BRF_PRG },	//  5
	
	{ "ec07.8a",        	0x08000, 0x16a5a1aa, BRF_ESS | BRF_PRG },	//  6	6502 Program 

	{ "ec25.15h",        	0x10000, 0x2e5732a2, BRF_GRA },				//  7	Characters
	{ "ec26.16h",        	0x10000, 0x161a2c4d, BRF_GRA },				//  8
	
	{ "ec18.14d",           0x10000, 0xef664373, BRF_GRA },				//  9	Tiles 1
	{ "ec17.12d",           0x10000, 0xa4f186ac, BRF_GRA },				// 10
	{ "ec20.17d",           0x10000, 0x2fc13be0, BRF_GRA },				// 11
	{ "ec19.15d",           0x10000, 0xd6b47869, BRF_GRA },				// 12
	{ "ec22.14f",           0x10000, 0x50d6a1ad, BRF_GRA },				// 13
	{ "ec21.12f",           0x10000, 0xf01d75c5, BRF_GRA },				// 14
	{ "ec24.17f",           0x10000, 0xae377361, BRF_GRA },				// 15
	{ "ec23.15f",           0x10000, 0xbbdaf771, BRF_GRA },				// 16
	
	{ "ec29.8h",           	0x10000, 0x5514b296, BRF_GRA },				// 17	Tiles 2
	{ "ec30.9h",            0x10000, 0x5855e8ef, BRF_GRA },				// 18
	{ "ec27.8f",           	0x10000, 0x99db7b9c, BRF_GRA },				// 19
	{ "ec28.9f",            0x10000, 0x33ce2b1a, BRF_GRA },				// 20
	
	{ "ec15.16c",           0x10000, 0x21816707, BRF_GRA },				// 21	Sprites
	{ "ec16.17c",           0x10000, 0xa5684574, BRF_GRA },				// 22
	{ "ec11.16a",           0x10000, 0x5c768315, BRF_GRA },				// 23
	{ "ec12.17a",           0x10000, 0x8b64d7a4, BRF_GRA },				// 24
	{ "ec13.13c",           0x10000, 0x56e3ed65, BRF_GRA },				// 25
	{ "ec14.14c",           0x10000, 0xbedfe7f3, BRF_GRA },				// 26
	{ "ec09.13a",           0x10000, 0x26240ea0, BRF_GRA },				// 27
	{ "ec10.14a",           0x10000, 0x47d95447, BRF_GRA },				// 28
	
	{ "ec08.2c",        	0x10000, 0x2159a609, BRF_SND },				// 29	Samples
	
	{ "ec31-e.9a",          0x01000, 0xbf93a2ec, BRF_ESS | BRF_PRG },	// 30	I8751
	
	{ "mb7116e.12c",		0x00200, 0x86e775f8, BRF_OPT }, 			// 31 PROMs
	{ "mb7122e.17e",		0x00400, 0xa5cda23e, BRF_OPT }, 			// 32
};

STD_ROM_PICK(Hbarrel)
STD_ROM_FN(Hbarrel)


// Heavy Barrel (US)
// DE-0297-1 main board, DE-0299-0 sub/rom board 

static struct BurnRomInfo HbarreluRomDesc[] = {
	{ "heavy_barrel_04.3c", 	0x10000, 0x4877b09e, BRF_ESS | BRF_PRG },	//  0	68000 Program Code
	{ "heavy_barrel_01.3a", 	0x10000, 0x8b41c219, BRF_ESS | BRF_PRG },	//  1
	{ "heavy_barrel_05.4c", 	0x10000, 0x2087d570, BRF_ESS | BRF_PRG },	//  2
	{ "heavy_barrel_02.4a", 	0x10000, 0x815536ae, BRF_ESS | BRF_PRG },	//  3
	{ "heavy_barrel_06.6c", 	0x10000, 0xda4e3fbc, BRF_ESS | BRF_PRG },	//  4
	{ "heavy_barrel_03.6a", 	0x10000, 0x7fed7c46, BRF_ESS | BRF_PRG },	//  5
	
	{ "heavy_barrel_07.8a", 	0x08000, 0xa127f0f7, BRF_ESS | BRF_PRG },	//  6	6502 Program 	
	
	{ "heavy_barrel_25.15h",	0x10000, 0x8649762c, BRF_GRA },				//  7	Characters
	{ "heavy_barrel_26.16h",    0x10000, 0xf8189bbd, BRF_GRA },				//  8
	
	{ "heavy_barrel_18.14d",    0x10000, 0xef664373, BRF_GRA },				//  9	Tiles 1
	{ "heavy_barrel_17.12d",    0x10000, 0xa4f186ac, BRF_GRA },				// 10
	{ "heavy_barrel_20.17d",    0x10000, 0x2fc13be0, BRF_GRA },				// 11
	{ "heavy_barrel_19.15d",    0x10000, 0xd6b47869, BRF_GRA },				// 12
	{ "heavy_barrel_22.14f",    0x10000, 0x50d6a1ad, BRF_GRA },				// 13
	{ "heavy_barrel_21.12f",    0x10000, 0xf01d75c5, BRF_GRA },				// 14
	{ "heavy_barrel_24.17f",    0x10000, 0xae377361, BRF_GRA },				// 15
	{ "heavy_barrel_23.15f",    0x10000, 0xbbdaf771, BRF_GRA },				// 16
	
	{ "heavy_barrel_29.8h",     0x10000, 0x5514b296, BRF_GRA },				// 17	Tiles 2
	{ "heavy_barrel_30.9h",     0x10000, 0x5855e8ef, BRF_GRA },				// 18
	{ "heavy_barrel_27.8f",     0x10000, 0x99db7b9c, BRF_GRA },				// 19
	{ "heavy_barrel_28.9f",     0x10000, 0x33ce2b1a, BRF_GRA },				// 20
	
	{ "heavy_barrel_15.16c",    0x10000, 0x21816707, BRF_GRA },				// 21	Sprites
	{ "heavy_barrel_16.17c",    0x10000, 0xa5684574, BRF_GRA },				// 22
	{ "heavy_barrel_11.16a",    0x10000, 0x5c768315, BRF_GRA },				// 23
	{ "heavy_barrel_12.17a",    0x10000, 0x8b64d7a4, BRF_GRA },				// 24
	{ "heavy_barrel_13.13c",    0x10000, 0x56e3ed65, BRF_GRA },				// 25
	{ "heavy_barrel_14.14c",    0x10000, 0xbedfe7f3, BRF_GRA },				// 26
	{ "heavy_barrel_09.13a",    0x10000, 0x26240ea0, BRF_GRA },				// 27
	{ "heavy_barrel_10.14a",    0x10000, 0x47d95447, BRF_GRA },				// 28
	
	{ "heavy_barrel_08.2c",     0x10000, 0x645c5b68, BRF_SND },				// 29	Samples
	
	{ "heavy_barrel_31.9a", 	0x01000, 0x239d726f, BRF_ESS | BRF_PRG },	// 30	I8751
	
	{ "mb7116e.12c",			0x00200, 0x86e775f8, BRF_OPT }, 			// 31 PROMs
	{ "mb7122e.17e",			0x00400, 0xa5cda23e, BRF_OPT }, 			// 32
};

STD_ROM_PICK(Hbarrelu)
STD_ROM_FN(Hbarrelu)


// Hippodrome (US)

static struct BurnRomInfo HippodrmRomDesc[] = {
	{ "ew02",               0x10000, 0xdf0d7dc6, BRF_ESS | BRF_PRG },	//  0	68000 Program Code
	{ "ew01",               0x10000, 0xd5670aa7, BRF_ESS | BRF_PRG },	//  1
	{ "ew05",               0x10000, 0xc76d65ec, BRF_ESS | BRF_PRG },	//  2
	{ "ew00",               0x10000, 0xe9b427a6, BRF_ESS | BRF_PRG },	//  3
	
	{ "ew04",               0x08000, 0x9871b98d, BRF_ESS | BRF_PRG },	//  4	6502 Program 
	
	{ "ew08",               0x10000, 0x53010534, BRF_ESS | BRF_PRG },	//  5	HuC6280 Program
	
	{ "ew14",               0x10000, 0x71ca593d, BRF_GRA },				//  6	Characters
	{ "ew13",               0x10000, 0x86be5fa7, BRF_GRA },				//  7

	{ "ew19",               0x08000, 0x6b80d7a3, BRF_GRA },				//  8	Tiles 1
	{ "ew18",               0x08000, 0x78d3d764, BRF_GRA },				//  9
	{ "ew20",               0x08000, 0xce9f5de3, BRF_GRA },				// 10
	{ "ew21",               0x08000, 0x487a7ba2, BRF_GRA },				// 11
	
	{ "ew24",               0x08000, 0x4e1bc2a4, BRF_GRA },				// 12	Tiles 2
	{ "ew25",               0x08000, 0x9eb47dfb, BRF_GRA },				// 13
	{ "ew23",               0x08000, 0x9ecf479e, BRF_GRA },				// 14
	{ "ew22",               0x08000, 0xe55669aa, BRF_GRA },				// 15
	
	{ "ew15",               0x10000, 0x95423914, BRF_GRA },				// 16	Sprites
	{ "ew16",               0x10000, 0x96233177, BRF_GRA },				// 17
	{ "ew10",               0x10000, 0x4c25dfe8, BRF_GRA },				// 18
	{ "ew11",               0x10000, 0xf2e007fc, BRF_GRA },				// 19
	{ "ew06",               0x10000, 0xe4bb8199, BRF_GRA },				// 20
	{ "ew07",               0x10000, 0x470b6989, BRF_GRA },				// 21
	{ "ew17",               0x10000, 0x8c97c757, BRF_GRA },				// 22
	{ "ew12",               0x10000, 0xa2d244bc, BRF_GRA },				// 23
	
	{ "ew03",               0x10000, 0xb606924d, BRF_SND },				// 24	Samples
	
	{ "mb7116e_a-1.12c",	0x00200, 0x86e775f8, BRF_OPT }, 			// 25 PROMs
	{ "mb7122e_a-2.17e",	0x00400, 0xa5cda23e, BRF_OPT }, 			// 26
};

STD_ROM_PICK(Hippodrm)
STD_ROM_FN(Hippodrm)


// Fighting Fantasy (Japan revision 3)

static struct BurnRomInfo FfantasyRomDesc[] = {
	{ "ex02-3.4b",          0x10000, 0xdf0d7dc6, BRF_ESS | BRF_PRG },	//  0	68000 Program Code
	{ "ex01-3.3b",          0x10000, 0xc0fb4fe5, BRF_ESS | BRF_PRG },	//  1
	{ "ex05-.4c",           0x10000, 0xc76d65ec, BRF_ESS | BRF_PRG },	//  2
	{ "ex00-.1b",           0x10000, 0xe9b427a6, BRF_ESS | BRF_PRG },	//  3
	
	{ "ex04-.1c",           0x08000, 0x9871b98d, BRF_ESS | BRF_PRG },	//  4	6502 Program 
	
	{ "ew08",               0x10000, 0x53010534, BRF_ESS | BRF_PRG },	//  5	HuC6280 Program
	
	{ "ev14",               0x10000, 0x686f72c1, BRF_GRA },				//  6	Characters
	{ "ev13",               0x10000, 0xb787dcc9, BRF_GRA },				//  7

	{ "ew19",               0x08000, 0x6b80d7a3, BRF_GRA },				//  8	Tiles 1
	{ "ew18",               0x08000, 0x78d3d764, BRF_GRA },				//  9
	{ "ew20",               0x08000, 0xce9f5de3, BRF_GRA },				// 10
	{ "ew21",               0x08000, 0x487a7ba2, BRF_GRA },				// 11
	
	{ "ew24",               0x08000, 0x4e1bc2a4, BRF_GRA },				// 12	Tiles 2
	{ "ew25",               0x08000, 0x9eb47dfb, BRF_GRA },				// 13
	{ "ew23",               0x08000, 0x9ecf479e, BRF_GRA },				// 14
	{ "ew22",               0x08000, 0xe55669aa, BRF_GRA },				// 15
	
	{ "ev15",               0x10000, 0x1d80f797, BRF_GRA },				// 16	Sprites
	{ "ew16",               0x10000, 0x96233177, BRF_GRA },				// 17
	{ "ev10",               0x10000, 0xc4e7116b, BRF_GRA },				// 18
	{ "ew11",               0x10000, 0xf2e007fc, BRF_GRA },				// 19
	{ "ev06",               0x10000, 0x6c794f1a, BRF_GRA },				// 20
	{ "ew07",               0x10000, 0x470b6989, BRF_GRA },				// 21
	{ "ev17",               0x10000, 0x045509d4, BRF_GRA },				// 22
	{ "ew12",               0x10000, 0xa2d244bc, BRF_GRA },				// 23
	
	{ "ew03",               0x10000, 0xb606924d, BRF_SND },				// 24	Samples
	
	{ "mb7116e_a-1.12c",	0x00200, 0x86e775f8, BRF_OPT }, 			// 25 PROMs
	{ "mb7122e_a-2.17e",	0x00400, 0xa5cda23e, BRF_OPT }, 			// 26
};

STD_ROM_PICK(Ffantasy)
STD_ROM_FN(Ffantasy)


// Fighting Fantasy (Japan revision 2)

static struct BurnRomInfo FfantasyjRomDesc[] = {
	{ "ff-02-2.bin",        0x10000, 0x29fc22a7, BRF_ESS | BRF_PRG },	//  0	68000 Program Code
	{ "ff-01-2.bin",        0x10000, 0x9f617cb4, BRF_ESS | BRF_PRG },	//  1
	{ "ew05",               0x10000, 0xc76d65ec, BRF_ESS | BRF_PRG },	//  2
	{ "ew00",               0x10000, 0xe9b427a6, BRF_ESS | BRF_PRG },	//  3
	
	{ "ew04",               0x08000, 0x9871b98d, BRF_ESS | BRF_PRG },	//  4	6502 Program 
	
	{ "ew08",               0x10000, 0x53010534, BRF_ESS | BRF_PRG },	//  5	HuC6280 Program
	
	{ "ev14",               0x10000, 0x686f72c1, BRF_GRA },				//  6	Characters
	{ "ev13",               0x10000, 0xb787dcc9, BRF_GRA },				//  7

	{ "ew19",               0x08000, 0x6b80d7a3, BRF_GRA },				//  8	Tiles 1
	{ "ew18",               0x08000, 0x78d3d764, BRF_GRA },				//  9
	{ "ew20",               0x08000, 0xce9f5de3, BRF_GRA },				// 10
	{ "ew21",               0x08000, 0x487a7ba2, BRF_GRA },				// 11
	
	{ "ew24",               0x08000, 0x4e1bc2a4, BRF_GRA },				// 12	Tiles 2
	{ "ew25",               0x08000, 0x9eb47dfb, BRF_GRA },				// 13
	{ "ew23",               0x08000, 0x9ecf479e, BRF_GRA },				// 14
	{ "ew22",               0x08000, 0xe55669aa, BRF_GRA },				// 15
	
	{ "ev15",               0x10000, 0x1d80f797, BRF_GRA },				// 16	Sprites
	{ "ew16",               0x10000, 0x96233177, BRF_GRA },				// 17
	{ "ev10",               0x10000, 0xc4e7116b, BRF_GRA },				// 18
	{ "ew11",               0x10000, 0xf2e007fc, BRF_GRA },				// 19
	{ "ev06",               0x10000, 0x6c794f1a, BRF_GRA },				// 20
	{ "ew07",               0x10000, 0x470b6989, BRF_GRA },				// 21
	{ "ev17",               0x10000, 0x045509d4, BRF_GRA },				// 22
	{ "ew12",               0x10000, 0xa2d244bc, BRF_GRA },				// 23
	
	{ "ew03",               0x10000, 0xb606924d, BRF_SND },				// 24	Samples
	
	{ "mb7116e_a-1.12c",	0x00200, 0x86e775f8, BRF_OPT }, 			// 25 PROMs
	{ "mb7122e_a-2.17e",	0x00400, 0xa5cda23e, BRF_OPT }, 			// 26
};

STD_ROM_PICK(Ffantasyj)
STD_ROM_FN(Ffantasyj)


// Fighting Fantasy (Japan)

static struct BurnRomInfo FfantasyaRomDesc[] = {
	{ "ev02",               0x10000, 0x797a7860, BRF_ESS | BRF_PRG },	//  0	68000 Program Code
	{ "ev01",               0x10000, 0x0f17184d, BRF_ESS | BRF_PRG },	//  1
	{ "ew05",               0x10000, 0xc76d65ec, BRF_ESS | BRF_PRG },	//  2
	{ "ew00",               0x10000, 0xe9b427a6, BRF_ESS | BRF_PRG },	//  3
	
	{ "ew04",               0x08000, 0x9871b98d, BRF_ESS | BRF_PRG },	//  4	6502 Program 
	
	{ "ew08",               0x10000, 0x53010534, BRF_ESS | BRF_PRG },	//  5	HuC6280 Program
	
	{ "ev14",               0x10000, 0x686f72c1, BRF_GRA },				//  6	Characters
	{ "ev13",               0x10000, 0xb787dcc9, BRF_GRA },				//  7

	{ "ew19",               0x08000, 0x6b80d7a3, BRF_GRA },				//  8	Tiles 1
	{ "ew18",               0x08000, 0x78d3d764, BRF_GRA },				//  9
	{ "ew20",               0x08000, 0xce9f5de3, BRF_GRA },				// 10
	{ "ew21",               0x08000, 0x487a7ba2, BRF_GRA },				// 11
	
	{ "ew24",               0x08000, 0x4e1bc2a4, BRF_GRA },				// 12	Tiles 2
	{ "ew25",               0x08000, 0x9eb47dfb, BRF_GRA },				// 13
	{ "ew23",               0x08000, 0x9ecf479e, BRF_GRA },				// 14
	{ "ew22",               0x08000, 0xe55669aa, BRF_GRA },				// 15
	
	{ "ev15",               0x10000, 0x1d80f797, BRF_GRA },				// 16	Sprites
	{ "ew16",               0x10000, 0x96233177, BRF_GRA },				// 17
	{ "ev10",               0x10000, 0xc4e7116b, BRF_GRA },				// 18
	{ "ew11",               0x10000, 0xf2e007fc, BRF_GRA },				// 19
	{ "ev06",               0x10000, 0x6c794f1a, BRF_GRA },				// 20
	{ "ew07",               0x10000, 0x470b6989, BRF_GRA },				// 21
	{ "ev17",               0x10000, 0x045509d4, BRF_GRA },				// 22
	{ "ew12",               0x10000, 0xa2d244bc, BRF_GRA },				// 23
	
	{ "ew03",               0x10000, 0xb606924d, BRF_SND },				// 24	Samples
	
	{ "mb7116e_a-1.12c",	0x00200, 0x86e775f8, BRF_OPT }, 			// 25 PROMs
	{ "mb7122e_a-2.17e",	0x00400, 0xa5cda23e, BRF_OPT }, 			// 26
};

STD_ROM_PICK(Ffantasya)
STD_ROM_FN(Ffantasya)


// Fighting Fantasy (Japan revision ?)
// DE-0297-3 PCB. All EX labels.

static struct BurnRomInfo FfantasybRomDesc[] = {
	{ "ex02-2",             0x10000, 0x4c26cda6, BRF_ESS | BRF_PRG },	//  0	68000 Program Code
	{ "ex01",               0x10000, 0xd2c4ab91, BRF_ESS | BRF_PRG },	//  1
	{ "ex05",               0x10000, 0xc76d65ec, BRF_ESS | BRF_PRG },	//  2
	{ "ex00",               0x10000, 0xe9b427a6, BRF_ESS | BRF_PRG },	//  3
	
	{ "ex04",               0x08000, 0x9871b98d, BRF_ESS | BRF_PRG },	//  4	6502 Program 
	
	{ "ex08",               0x10000, 0x53010534, BRF_ESS | BRF_PRG },	//  5	HuC6280 Program
	
	{ "ex14",               0x10000, 0x686f72c1, BRF_GRA },				//  6	Characters
	{ "ex13",               0x10000, 0xb787dcc9, BRF_GRA },				//  7

	{ "ex19",               0x08000, 0x6b80d7a3, BRF_GRA },				//  8	Tiles 1
	{ "ex18",               0x08000, 0x78d3d764, BRF_GRA },				//  9
	{ "ex20",               0x08000, 0xce9f5de3, BRF_GRA },				// 10
	{ "ex21",               0x08000, 0x487a7ba2, BRF_GRA },				// 11
	
	{ "ex24",               0x08000, 0x4e1bc2a4, BRF_GRA },				// 12	Tiles 2
	{ "ex25",               0x08000, 0x9eb47dfb, BRF_GRA },				// 13
	{ "ex23",               0x08000, 0x9ecf479e, BRF_GRA },				// 14
	{ "ex22",               0x08000, 0xe55669aa, BRF_GRA },				// 15
	
	{ "ex15",               0x10000, 0x95423914, BRF_GRA },				// 16	Sprites
	{ "ex16",               0x10000, 0x96233177, BRF_GRA },				// 17
	{ "ex10",               0x10000, 0x4c25dfe8, BRF_GRA },				// 18
	{ "ex11",               0x10000, 0xf2e007fc, BRF_GRA },				// 19
	{ "ex06",               0x10000, 0xe4bb8199, BRF_GRA },				// 20
	{ "ex07",               0x10000, 0x470b6989, BRF_GRA },				// 21
	{ "ex17",               0x10000, 0x8c97c757, BRF_GRA },				// 22
	{ "ex12",               0x10000, 0xa2d244bc, BRF_GRA },				// 23
	
	{ "ex03",               0x10000, 0xb606924d, BRF_SND },				// 24	Samples
	
	{ "mb7116e_a-1.12c",	0x00200, 0x86e775f8, BRF_OPT }, 			// 25 PROMs
	{ "mb7122e_a-2.17e",	0x00400, 0xa5cda23e, BRF_OPT }, 			// 26
};

STD_ROM_PICK(Ffantasyb)
STD_ROM_FN(Ffantasyb)


// Midnight Resistance (World, set 1)

static struct BurnRomInfo MidresRomDesc[] = {
	{ "fk14.17k",           0x20000, 0xde7522df, BRF_ESS | BRF_PRG },	//  0	68000 Program Code
	{ "fk12.13k",           0x20000, 0x3494b8c9, BRF_ESS | BRF_PRG },	//  1
	{ "fl15.18k",           0x20000, 0x1328354e, BRF_ESS | BRF_PRG },	//  2
	{ "fl13.15k",           0x20000, 0xe3b3955e, BRF_ESS | BRF_PRG },	//  3
	
	{ "fl16.5f",            0x10000, 0x66360bdf, BRF_ESS | BRF_PRG },	//  4	HuC6280 Program
	
	{ "fk05.11a",           0x10000, 0x3cdb7453, BRF_GRA },				//  5	Characters
	{ "fk04.10a",           0x10000, 0x325ba20c, BRF_GRA },				//  6

	{ "fl-09.18a",          0x20000, 0x907d5910, BRF_GRA },				//  7	Tiles 1
	{ "fl-08.16a",          0x20000, 0xa936c03c, BRF_GRA },				//  8
	{ "fl-07.15a",          0x20000, 0x2068c45c, BRF_GRA },				//  9
	{ "fl-06.13a",          0x20000, 0xb7241ab9, BRF_GRA },				// 10
	
	{ "fl-11.21a",          0x20000, 0xb86b73b4, BRF_GRA },				// 11	Tiles 2
	{ "fl-10.20a",          0x20000, 0x92245b29, BRF_GRA },				// 12
	
	{ "fl-01.4a",           0x20000, 0x2c8b35a7, BRF_GRA },				// 13	Sprites
	{ "fl-03.8a",           0x20000, 0x1eefed3c, BRF_GRA },				// 14
	{ "fl-00.2a",           0x20000, 0x756fb801, BRF_GRA },				// 15
	{ "fl-02.6a",           0x20000, 0x54d2c120, BRF_GRA },				// 16
	
	{ "fl17.1h",            0x20000, 0x9029965d, BRF_SND },				// 17	Samples
	
	{ "tb-7.22f",           0x00100, 0xeb539ffb, BRF_OPT },				// 18	PROMs
	
	{ "tb-1-1.19h",         0x00104, 0xd28fb8e0, BRF_OPT },				// 19	PLDs
	{ "tb-2.10h",           0x00104, 0xbcb591e3, BRF_OPT },				// 20
	{ "tb-3.9h",            0x00104, 0xe12972ac, BRF_OPT },				// 21
	{ "tb-4.8h",            0x00104, 0xc6437e49, BRF_OPT },				// 22
	{ "tb-5.14h",           0x00104, 0xe9ee3a67, BRF_OPT },				// 23
	{ "tb-6.20h",           0x00104, 0x23b17abe, BRF_OPT },				// 24
};

STD_ROM_PICK(Midres)
STD_ROM_FN(Midres)


// Midnight Resistance (World, set 2)
// DE-0323-4 PCB, only the first 2 main CPU ROMs differ, ROM labels weren't original so unfortunately not possible to determine version

static struct BurnRomInfo Midres2RomDesc[] = {
	{ "mr14.17k",           0x20000, 0xad4617a9, BRF_ESS | BRF_PRG },	//  0	68000 Program Code
	{ "mr12.13k",           0x20000, 0xc9ed677b, BRF_ESS | BRF_PRG },	//  1
	{ "fl15.18k",           0x20000, 0x1328354e, BRF_ESS | BRF_PRG },	//  2
	{ "fl13.15k",           0x20000, 0xe3b3955e, BRF_ESS | BRF_PRG },	//  3
	
	{ "fl16.5f",            0x10000, 0x66360bdf, BRF_ESS | BRF_PRG },	//  4	HuC6280 Program
	
	{ "fl05.11a",           0x10000, 0xd75aba06, BRF_GRA },				//  5	Characters
	{ "fl04.10a",           0x10000, 0x8f5bbb79, BRF_GRA },				//  6

	{ "fl-09.18a",          0x20000, 0x907d5910, BRF_GRA },				//  7	Tiles 1
	{ "fl-08.16a",          0x20000, 0xa936c03c, BRF_GRA },				//  8
	{ "fl-07.15a",          0x20000, 0x2068c45c, BRF_GRA },				//  9
	{ "fl-06.13a",          0x20000, 0xb7241ab9, BRF_GRA },				// 10
	
	{ "fl-11.21a",          0x20000, 0xb86b73b4, BRF_GRA },				// 11	Tiles 2
	{ "fl-10.20a",          0x20000, 0x92245b29, BRF_GRA },				// 12
	
	{ "fl-01.4a",           0x20000, 0x2c8b35a7, BRF_GRA },				// 13	Sprites
	{ "fl-03.8a",           0x20000, 0x1eefed3c, BRF_GRA },				// 14
	{ "fl-00.2a",           0x20000, 0x756fb801, BRF_GRA },				// 15
	{ "fl-02.6a",           0x20000, 0x54d2c120, BRF_GRA },				// 16
	
	{ "fl17.1h",            0x20000, 0x9029965d, BRF_SND },				// 17	Samples
	
	{ "tb-7.22f",           0x00100, 0xeb539ffb, BRF_OPT },				// 18	PROMs
	
	{ "tb-1-1.19h",         0x00104, 0xd28fb8e0, BRF_OPT },				// 19	PLDs
	{ "tb-2.10h",           0x00104, 0xbcb591e3, BRF_OPT },				// 20
	{ "tb-3.9h",            0x00104, 0xe12972ac, BRF_OPT },				// 21
	{ "tb-4.8h",            0x00104, 0xc6437e49, BRF_OPT },				// 22
	{ "tb-5.14h",           0x00104, 0xe9ee3a67, BRF_OPT },				// 23
	{ "tb-6.20h",           0x00104, 0x23b17abe, BRF_OPT },				// 24
};

STD_ROM_PICK(Midres2)
STD_ROM_FN(Midres2)


// Midnight Resistance (US)

static struct BurnRomInfo MidresuRomDesc[] = {
	{ "fl14.17k",           0x20000, 0x2f9507a2, BRF_ESS | BRF_PRG },	//  0	68000 Program Code
	{ "fl12.13k",           0x20000, 0x3815ad9f, BRF_ESS | BRF_PRG },	//  1
	{ "fl15.18k",           0x20000, 0x1328354e, BRF_ESS | BRF_PRG },	//  2
	{ "fl13.15k",           0x20000, 0xe3b3955e, BRF_ESS | BRF_PRG },	//  3
	
	{ "fl16.5f",            0x10000, 0x66360bdf, BRF_ESS | BRF_PRG },	//  4	HuC6280 Program
	
	{ "fl05.11a",           0x10000, 0xd75aba06, BRF_GRA },				//  5	Characters
	{ "fl04.10a",           0x10000, 0x8f5bbb79, BRF_GRA },				//  6

	{ "fl-09.18a",          0x20000, 0x907d5910, BRF_GRA },				//  7	Tiles 1
	{ "fl-08.16a",          0x20000, 0xa936c03c, BRF_GRA },				//  8
	{ "fl-07.15a",          0x20000, 0x2068c45c, BRF_GRA },				//  9
	{ "fl-06.13a",          0x20000, 0xb7241ab9, BRF_GRA },				// 10
	
	{ "fl-11.21a",          0x20000, 0xb86b73b4, BRF_GRA },				// 11	Tiles 2
	{ "fl-10.20a",          0x20000, 0x92245b29, BRF_GRA },				// 12
	
	{ "fl-01.4a",           0x20000, 0x2c8b35a7, BRF_GRA },				// 13	Sprites
	{ "fl-03.8a",           0x20000, 0x1eefed3c, BRF_GRA },				// 14
	{ "fl-00.2a",           0x20000, 0x756fb801, BRF_GRA },				// 15
	{ "fl-02.6a",           0x20000, 0x54d2c120, BRF_GRA },				// 16
	
	{ "fl17.1h",            0x20000, 0x9029965d, BRF_SND },				// 17	Samples
	
	{ "tb-7.22f",           0x00100, 0xeb539ffb, BRF_OPT },				// 18	PROMs
	
	{ "tb-1-1.19h",         0x00104, 0xd28fb8e0, BRF_OPT },				// 19	PLDs
	{ "tb-2.10h",           0x00104, 0xbcb591e3, BRF_OPT },				// 20
	{ "tb-3.9h",            0x00104, 0xe12972ac, BRF_OPT },				// 21
	{ "tb-4.8h",            0x00104, 0xc6437e49, BRF_OPT },				// 22
	{ "tb-5.14h",           0x00104, 0xe9ee3a67, BRF_OPT },				// 23
	{ "tb-6.20h",           0x00104, 0x23b17abe, BRF_OPT },				// 24
};

STD_ROM_PICK(Midresu)
STD_ROM_FN(Midresu)


// Midnight Resistance (Japan)

static struct BurnRomInfo MidresjRomDesc[] = {
	{ "fh14.17k",           0x20000, 0x6d632a51, BRF_ESS | BRF_PRG },	//  0	68000 Program Code
	{ "fh12.13k",           0x20000, 0x45143384, BRF_ESS | BRF_PRG },	//  1
	{ "fl15.18k",           0x20000, 0x1328354e, BRF_ESS | BRF_PRG },	//  2
	{ "fl13.15k",           0x20000, 0xe3b3955e, BRF_ESS | BRF_PRG },	//  3
	
	{ "fh16.5f",            0x10000, 0x00736f32, BRF_ESS | BRF_PRG },	//  4	HuC6280 Program
	
	{ "fl05.11a",           0x10000, 0xd75aba06, BRF_GRA },				//  5	Characters
	{ "fl04.10a",           0x10000, 0x8f5bbb79, BRF_GRA },				//  6

	{ "fl-09.18a",          0x20000, 0x907d5910, BRF_GRA },				//  7	Tiles 1
	{ "fl-08.16a",          0x20000, 0xa936c03c, BRF_GRA },				//  8
	{ "fl-07.15a",          0x20000, 0x2068c45c, BRF_GRA },				//  9
	{ "fl-06.13a",          0x20000, 0xb7241ab9, BRF_GRA },				// 10
	
	{ "fl-11.21a",          0x20000, 0xb86b73b4, BRF_GRA },				// 11	Tiles 2
	{ "fl-10.20a",          0x20000, 0x92245b29, BRF_GRA },				// 12
	
	{ "fl-01.4a",           0x20000, 0x2c8b35a7, BRF_GRA },				// 13	Sprites
	{ "fl-03.8a",           0x20000, 0x1eefed3c, BRF_GRA },				// 14
	{ "fl-00.2a",           0x20000, 0x756fb801, BRF_GRA },				// 15
	{ "fl-02.6a",           0x20000, 0x54d2c120, BRF_GRA },				// 16
	
	{ "fh17.1h",            0x20000, 0xc7b0a24e, BRF_SND },				// 17	Samples
	
	{ "tb-7.22f",           0x00100, 0xeb539ffb, BRF_OPT },				// 18	PROMs
	
	{ "tb-1-1.19h",         0x00104, 0xd28fb8e0, BRF_OPT },				// 19	PLDs
	{ "tb-2.10h",           0x00104, 0xbcb591e3, BRF_OPT },				// 20
	{ "tb-3.9h",            0x00104, 0xe12972ac, BRF_OPT },				// 21
	{ "tb-4.8h",            0x00104, 0xc6437e49, BRF_OPT },				// 22
	{ "tb-5.14h",           0x00104, 0xe9ee3a67, BRF_OPT },				// 23
	{ "tb-6.20h",           0x00104, 0x23b17abe, BRF_OPT },				// 24
};

STD_ROM_PICK(Midresj)
STD_ROM_FN(Midresj)


// Robocop (World revision 4)
// DE-0297-3 main board, DE-0316-3 sub/rom board 

static struct BurnRomInfo RobocopRomDesc[] = {
	{ "ep05-4.11c",         0x10000, 0x29c35379, BRF_ESS | BRF_PRG },	//  0	68000 Program Code
	{ "ep01-4.11b",         0x10000, 0x77507c69, BRF_ESS | BRF_PRG },	//  1
	{ "ep04-3",             0x10000, 0x39181778, BRF_ESS | BRF_PRG },	//  2
	{ "ep00-3",             0x10000, 0xe128541f, BRF_ESS | BRF_PRG },	//  3
	
	{ "ep03-3",             0x08000, 0x5b164b24, BRF_ESS | BRF_PRG },	//  4	6502 Program 
	
	{ "en_24_mb7124e.a2",   0x00200, 0xb8e2ca98, BRF_ESS | BRF_PRG },	//  5	HuC6280 Program
	
	{ "ep23",               0x10000, 0xa77e4ab1, BRF_GRA },				//  6	Characters
	{ "ep22",               0x10000, 0x9fbd6903, BRF_GRA },				//  7

	{ "ep20",               0x10000, 0x1d8d38b8, BRF_GRA },				//  8	Tiles 1
	{ "ep21",               0x10000, 0x187929b2, BRF_GRA },				//  9
	{ "ep18",               0x10000, 0xb6580b5e, BRF_GRA },				// 10
	{ "ep19",               0x10000, 0x9bad01c7, BRF_GRA },				// 11
	
	{ "ep14",               0x08000, 0xca56ceda, BRF_GRA },				// 12	Tiles 2
	{ "ep15",               0x08000, 0xa945269c, BRF_GRA },				// 13
	{ "ep16",               0x08000, 0xe7fa4d58, BRF_GRA },				// 14
	{ "ep17",               0x08000, 0x84aae89d, BRF_GRA },				// 15
	
	{ "ep07",               0x10000, 0x495d75cf, BRF_GRA },				// 16	Sprites
	{ "ep06",               0x08000, 0xa2ae32e2, BRF_GRA },				// 17
	{ "ep11",               0x10000, 0x62fa425a, BRF_GRA },				// 18
	{ "ep10",               0x08000, 0xcce3bd95, BRF_GRA },				// 19
	{ "ep09",               0x10000, 0x11bed656, BRF_GRA },				// 20
	{ "ep08",               0x08000, 0xc45c7b4c, BRF_GRA },				// 21
	{ "ep13",               0x10000, 0x8fca9f28, BRF_GRA },				// 22
	{ "ep12",               0x08000, 0x3cd1d0c3, BRF_GRA },				// 23
	
	{ "ep02",               0x10000, 0x711ce46f, BRF_SND },				// 24	Samples
	
	{ "mb7116e_a-1.12c",	0x00200, 0x86e775f8, BRF_OPT }, 			// 25 PROMs
	{ "mb7122e_a-2.17e",	0x00400, 0xa5cda23e, BRF_OPT }, 			// 26
};

STD_ROM_PICK(Robocop)
STD_ROM_FN(Robocop)


// Robocop (World revision 3)
// DE-0297-3 main board, DE-0316-3 sub/rom board 

static struct BurnRomInfo RobocopwRomDesc[] = {
	{ "ep05-3",             0x10000, 0xba69bf84, BRF_ESS | BRF_PRG },	//  0	68000 Program Code
	{ "ep01-3",             0x10000, 0x2a9f6e2c, BRF_ESS | BRF_PRG },	//  1
	{ "ep04-3",             0x10000, 0x39181778, BRF_ESS | BRF_PRG },	//  2
	{ "ep00-3",             0x10000, 0xe128541f, BRF_ESS | BRF_PRG },	//  3
	
	{ "ep03-3",             0x08000, 0x5b164b24, BRF_ESS | BRF_PRG },	//  4	6502 Program 
	
	{ "en_24.a2",           0x00200, 0xb8e2ca98, BRF_ESS | BRF_PRG },	//  5	HuC6280 Program
	
	{ "ep23",               0x10000, 0xa77e4ab1, BRF_GRA },				//  6	Characters
	{ "ep22",               0x10000, 0x9fbd6903, BRF_GRA },				//  7

	{ "ep20",               0x10000, 0x1d8d38b8, BRF_GRA },				//  8	Tiles 1
	{ "ep21",               0x10000, 0x187929b2, BRF_GRA },				//  9
	{ "ep18",               0x10000, 0xb6580b5e, BRF_GRA },				// 10
	{ "ep19",               0x10000, 0x9bad01c7, BRF_GRA },				// 11
	
	{ "ep14",               0x08000, 0xca56ceda, BRF_GRA },				// 12	Tiles 2
	{ "ep15",               0x08000, 0xa945269c, BRF_GRA },				// 13
	{ "ep16",               0x08000, 0xe7fa4d58, BRF_GRA },				// 14
	{ "ep17",               0x08000, 0x84aae89d, BRF_GRA },				// 15
	
	{ "ep07",               0x10000, 0x495d75cf, BRF_GRA },				// 16	Sprites
	{ "ep06",               0x08000, 0xa2ae32e2, BRF_GRA },				// 17
	{ "ep11",               0x10000, 0x62fa425a, BRF_GRA },				// 18
	{ "ep10",               0x08000, 0xcce3bd95, BRF_GRA },				// 19
	{ "ep09",               0x10000, 0x11bed656, BRF_GRA },				// 20
	{ "ep08",               0x08000, 0xc45c7b4c, BRF_GRA },				// 21
	{ "ep13",               0x10000, 0x8fca9f28, BRF_GRA },				// 22
	{ "ep12",               0x08000, 0x3cd1d0c3, BRF_GRA },				// 23
	
	{ "ep02",               0x10000, 0x711ce46f, BRF_SND },				// 24	Samples
	
	{ "mb7116e_a-1.12c",	0x00200, 0x86e775f8, BRF_OPT }, 			// 25 PROMs
	{ "mb7122e_a-2.17e",	0x00400, 0xa5cda23e, BRF_OPT }, 			// 26
};

STD_ROM_PICK(Robocopw)
STD_ROM_FN(Robocopw)


// Robocop (Japan)
// DE-0297-3 main board, DE-0316-3 sub/rom board

static struct BurnRomInfo RobocopjRomDesc[] = {
	{ "em05-1.c11",         0x10000, 0x954ea8f4, BRF_ESS | BRF_PRG },	//  0	68000 Program Code
	{ "em01-1.b12",         0x10000, 0x1b87b622, BRF_ESS | BRF_PRG },	//  1
	{ "ep04-3",             0x10000, 0x39181778, BRF_ESS | BRF_PRG },	//  2
	{ "ep00-3",             0x10000, 0xe128541f, BRF_ESS | BRF_PRG },	//  3
	
	{ "ep03-3",             0x08000, 0x5b164b24, BRF_ESS | BRF_PRG },	//  4	6502 Program 
	
	{ "en_24.a2",           0x00200, 0xb8e2ca98, BRF_ESS | BRF_PRG },	//  5	HuC6280 Program
	
	{ "ep23",               0x10000, 0xa77e4ab1, BRF_GRA },				//  6	Characters
	{ "ep22",               0x10000, 0x9fbd6903, BRF_GRA },				//  7

	{ "ep20",               0x10000, 0x1d8d38b8, BRF_GRA },				//  8	Tiles 1
	{ "ep21",               0x10000, 0x187929b2, BRF_GRA },				//  9
	{ "ep18",               0x10000, 0xb6580b5e, BRF_GRA },				// 10
	{ "ep19",               0x10000, 0x9bad01c7, BRF_GRA },				// 11
	
	{ "ep14",               0x08000, 0xca56ceda, BRF_GRA },				// 12	Tiles 2
	{ "ep15",               0x08000, 0xa945269c, BRF_GRA },				// 13
	{ "ep16",               0x08000, 0xe7fa4d58, BRF_GRA },				// 14
	{ "ep17",               0x08000, 0x84aae89d, BRF_GRA },				// 15
	
	{ "ep07",               0x10000, 0x495d75cf, BRF_GRA },				// 16	Sprites
	{ "ep06",               0x08000, 0xa2ae32e2, BRF_GRA },				// 17
	{ "ep11",               0x10000, 0x62fa425a, BRF_GRA },				// 18
	{ "ep10",               0x08000, 0xcce3bd95, BRF_GRA },				// 19
	{ "ep09",               0x10000, 0x11bed656, BRF_GRA },				// 20
	{ "ep08",               0x08000, 0xc45c7b4c, BRF_GRA },				// 21
	{ "ep13",               0x10000, 0x8fca9f28, BRF_GRA },				// 22
	{ "ep12",               0x08000, 0x3cd1d0c3, BRF_GRA },				// 23
	
	{ "ep02",               0x10000, 0x711ce46f, BRF_SND },				// 24	Samples
	
	{ "mb7116e_a-1.12c",	0x00200, 0x86e775f8, BRF_OPT }, 			// 25 PROMs
	{ "mb7122e_a-2.17e",	0x00400, 0xa5cda23e, BRF_OPT }, 			// 26
};

STD_ROM_PICK(Robocopj)
STD_ROM_FN(Robocopj)


// Robocop (US revision 1)
// DE-0297-3 main board, DE-0316-3 sub/rom board 

static struct BurnRomInfo RobocopuRomDesc[] = {
	{ "ep05-1",             0x10000, 0x8de5cb3d, BRF_ESS | BRF_PRG },	//  0	68000 Program Code
	{ "ep01-1",             0x10000, 0xb3c6bc02, BRF_ESS | BRF_PRG },	//  1
	{ "ep04",               0x10000, 0xc38b9d18, BRF_ESS | BRF_PRG },	//  2
	{ "ep00",               0x10000, 0x374c91aa, BRF_ESS | BRF_PRG },	//  3
	
	{ "ep03",               0x08000, 0x1089eab8, BRF_ESS | BRF_PRG },	//  4	6502 Program 
	
	{ "en_24.a2",           0x00200, 0xb8e2ca98, BRF_ESS | BRF_PRG },	//  5	HuC6280 Program
	
	{ "ep23",               0x10000, 0xa77e4ab1, BRF_GRA },				//  6	Characters
	{ "ep22",               0x10000, 0x9fbd6903, BRF_GRA },				//  7

	{ "ep20",               0x10000, 0x1d8d38b8, BRF_GRA },				//  8	Tiles 1
	{ "ep21",               0x10000, 0x187929b2, BRF_GRA },				//  9
	{ "ep18",               0x10000, 0xb6580b5e, BRF_GRA },				// 10
	{ "ep19",               0x10000, 0x9bad01c7, BRF_GRA },				// 11
	
	{ "ep14",               0x08000, 0xca56ceda, BRF_GRA },				// 12	Tiles 2
	{ "ep15",               0x08000, 0xa945269c, BRF_GRA },				// 13
	{ "ep16",               0x08000, 0xe7fa4d58, BRF_GRA },				// 14
	{ "ep17",               0x08000, 0x84aae89d, BRF_GRA },				// 15
	
	{ "ep07",               0x10000, 0x495d75cf, BRF_GRA },				// 16	Sprites
	{ "ep06",               0x08000, 0xa2ae32e2, BRF_GRA },				// 17
	{ "ep11",               0x10000, 0x62fa425a, BRF_GRA },				// 18
	{ "ep10",               0x08000, 0xcce3bd95, BRF_GRA },				// 19
	{ "ep09",               0x10000, 0x11bed656, BRF_GRA },				// 20
	{ "ep08",               0x08000, 0xc45c7b4c, BRF_GRA },				// 21
	{ "ep13",               0x10000, 0x8fca9f28, BRF_GRA },				// 22
	{ "ep12",               0x08000, 0x3cd1d0c3, BRF_GRA },				// 23
	
	{ "ep02",               0x10000, 0x711ce46f, BRF_SND },				// 24	Samples
	
	{ "mb7116e_a-1.12c",	0x00200, 0x86e775f8, BRF_OPT }, 			// 25 PROMs
	{ "mb7122e_a-2.17e",	0x00400, 0xa5cda23e, BRF_OPT }, 			// 26
};

STD_ROM_PICK(Robocopu)
STD_ROM_FN(Robocopu)


// Robocop (US revision 0)
// DE-0297-3 main board, DE-0316-3 sub/rom board 

static struct BurnRomInfo Robocopu0RomDesc[] = {
	{ "ep05",               0x10000, 0xc465bdd8, BRF_ESS | BRF_PRG },	//  0	68000 Program Code
	{ "ep01",               0x10000, 0x1352d36e, BRF_ESS | BRF_PRG },	//  1
	{ "ep04",               0x10000, 0xc38b9d18, BRF_ESS | BRF_PRG },	//  2
	{ "ep00",               0x10000, 0x374c91aa, BRF_ESS | BRF_PRG },	//  3
	
	{ "ep03",               0x08000, 0x1089eab8, BRF_ESS | BRF_PRG },	//  4	6502 Program 
	
	{ "en_24.a2",           0x00200, 0xb8e2ca98, BRF_ESS | BRF_PRG },	//  5	HuC6280 Program
	
	{ "ep23",               0x10000, 0xa77e4ab1, BRF_GRA },				//  6	Characters
	{ "ep22",               0x10000, 0x9fbd6903, BRF_GRA },				//  7

	{ "ep20",               0x10000, 0x1d8d38b8, BRF_GRA },				//  8	Tiles 1
	{ "ep21",               0x10000, 0x187929b2, BRF_GRA },				//  9
	{ "ep18",               0x10000, 0xb6580b5e, BRF_GRA },				// 10
	{ "ep19",               0x10000, 0x9bad01c7, BRF_GRA },				// 11
	
	{ "ep14",               0x08000, 0xca56ceda, BRF_GRA },				// 12	Tiles 2
	{ "ep15",               0x08000, 0xa945269c, BRF_GRA },				// 13
	{ "ep16",               0x08000, 0xe7fa4d58, BRF_GRA },				// 14
	{ "ep17",               0x08000, 0x84aae89d, BRF_GRA },				// 15
	
	{ "ep07",               0x10000, 0x495d75cf, BRF_GRA },				// 16	Sprites
	{ "ep06",               0x08000, 0xa2ae32e2, BRF_GRA },				// 17
	{ "ep11",               0x10000, 0x62fa425a, BRF_GRA },				// 18
	{ "ep10",               0x08000, 0xcce3bd95, BRF_GRA },				// 19
	{ "ep09",               0x10000, 0x11bed656, BRF_GRA },				// 20
	{ "ep08",               0x08000, 0xc45c7b4c, BRF_GRA },				// 21
	{ "ep13",               0x10000, 0x8fca9f28, BRF_GRA },				// 22
	{ "ep12",               0x08000, 0x3cd1d0c3, BRF_GRA },				// 23
	
	{ "ep02",               0x10000, 0x711ce46f, BRF_SND },				// 24	Samples
	
	{ "mb7116e_a-1.12c",	0x00200, 0x86e775f8, BRF_OPT }, 			// 25 PROMs
	{ "mb7122e_a-2.17e",	0x00400, 0xa5cda23e, BRF_OPT }, 			// 26
};

STD_ROM_PICK(Robocopu0)
STD_ROM_FN(Robocopu0)


// Robocop (World bootleg)

static struct BurnRomInfo RobocopbRomDesc[] = {
	{ "robop_05.rom",       0x10000, 0xbcef3e9b, BRF_ESS | BRF_PRG },	//  0	68000 Program Code
	{ "robop_01.rom",       0x10000, 0xc9803685, BRF_ESS | BRF_PRG },	//  1
	{ "robop_04.rom",       0x10000, 0x9d7b79e0, BRF_ESS | BRF_PRG },	//  2
	{ "robop_00.rom",       0x10000, 0x80ba64ab, BRF_ESS | BRF_PRG },	//  3
	
	{ "ep03-3",             0x08000, 0x5b164b24, BRF_ESS | BRF_PRG },	//  4	6502 Program 
		
	{ "ep23",               0x10000, 0xa77e4ab1, BRF_GRA },				//  5	Characters
	{ "ep22",               0x10000, 0x9fbd6903, BRF_GRA },				//  6

	{ "ep20",               0x10000, 0x1d8d38b8, BRF_GRA },				//  7	Tiles 1
	{ "ep21",               0x10000, 0x187929b2, BRF_GRA },				//  8
	{ "ep18",               0x10000, 0xb6580b5e, BRF_GRA },				//  9
	{ "ep19",               0x10000, 0x9bad01c7, BRF_GRA },				// 10
	
	{ "ep14",               0x08000, 0xca56ceda, BRF_GRA },				// 11	Tiles 2
	{ "ep15",               0x08000, 0xa945269c, BRF_GRA },				// 12
	{ "ep16",               0x08000, 0xe7fa4d58, BRF_GRA },				// 13
	{ "ep17",               0x08000, 0x84aae89d, BRF_GRA },				// 14
	
	{ "ep07",               0x10000, 0x495d75cf, BRF_GRA },				// 15	Sprites
	{ "ep06",               0x08000, 0xa2ae32e2, BRF_GRA },				// 16
	{ "ep11",               0x10000, 0x62fa425a, BRF_GRA },				// 17
	{ "ep10",               0x08000, 0xcce3bd95, BRF_GRA },				// 18
	{ "ep09",               0x10000, 0x11bed656, BRF_GRA },				// 19
	{ "ep08",               0x08000, 0xc45c7b4c, BRF_GRA },				// 20
	{ "ep13",               0x10000, 0x8fca9f28, BRF_GRA },				// 21
	{ "ep12",               0x08000, 0x3cd1d0c3, BRF_GRA },				// 22
	
	{ "ep02",               0x10000, 0x711ce46f, BRF_SND },				// 23	Samples
};

STD_ROM_PICK(Robocopb)
STD_ROM_FN(Robocopb)


// Robocop (Red Corporation World bootleg)

static struct BurnRomInfo Robocopb2RomDesc[] = {
	{ "s-9.e3",       		0x10000, 0xbcef3e9b, BRF_ESS | BRF_PRG },	//  0	68000 Program Code
	{ "s-11.c3",       		0x10000, 0xc9803685, BRF_ESS | BRF_PRG },	//  1
	{ "s-10.e2",       		0x10000, 0x9d7b79e0, BRF_ESS | BRF_PRG },	//  2
	{ "s-12.c2",       		0x10000, 0x631301c1, BRF_ESS | BRF_PRG },	//  3
	
	{ "ep03-3",             0x08000, 0x5b164b24, BRF_ESS | BRF_PRG },	//  4	6502 Program 
		
	{ "ep23",               0x10000, 0xa77e4ab1, BRF_GRA },				//  5	Characters
	{ "ep22",               0x10000, 0x9fbd6903, BRF_GRA },				//  6

	{ "ep20",               0x10000, 0x1d8d38b8, BRF_GRA },				//  7	Tiles 1
	{ "ep21",               0x10000, 0x187929b2, BRF_GRA },				//  8
	{ "ep18",               0x10000, 0xb6580b5e, BRF_GRA },				//  9
	{ "ep19",               0x10000, 0x9bad01c7, BRF_GRA },				// 10
	
	{ "ep14",               0x08000, 0xca56ceda, BRF_GRA },				// 11	Tiles 2
	{ "ep15",               0x08000, 0xa945269c, BRF_GRA },				// 12
	{ "ep16",               0x08000, 0xe7fa4d58, BRF_GRA },				// 13
	{ "ep17",               0x08000, 0x84aae89d, BRF_GRA },				// 14
	
	{ "ep07",               0x10000, 0x495d75cf, BRF_GRA },				// 15	Sprites
	{ "ep06",               0x08000, 0xa2ae32e2, BRF_GRA },				// 16
	{ "ep11",               0x10000, 0x62fa425a, BRF_GRA },				// 17
	{ "ep10",               0x08000, 0xcce3bd95, BRF_GRA },				// 18
	{ "ep09",               0x10000, 0x11bed656, BRF_GRA },				// 19
	{ "ep08",               0x08000, 0xc45c7b4c, BRF_GRA },				// 20
	{ "ep13",               0x10000, 0x8fca9f28, BRF_GRA },				// 21
	{ "ep12",               0x08000, 0x3cd1d0c3, BRF_GRA },				// 22
	
	{ "ep02",               0x10000, 0x711ce46f, BRF_SND },				// 23	Samples
};

STD_ROM_PICK(Robocopb2)
STD_ROM_FN(Robocopb2)


// Secret Agent (World revision 3)
// DE-0322-2 PCB 

static struct BurnRomInfo SecretagRomDesc[] = {
	{ "fb14-3.17l",         0x10000, 0x9be6ac90, BRF_ESS | BRF_PRG },	//  0	68000 Program Code
	{ "fb12-3.9l",          0x10000, 0x28904b6b, BRF_ESS | BRF_PRG },	//  1
	{ "fb15.19l",           0x10000, 0x106bb26c, BRF_ESS | BRF_PRG },	//  2
	{ "fb13.11l",           0x10000, 0x90523413, BRF_ESS | BRF_PRG },	//  3
	
	{ "fb10.5h",            0x10000, 0xdfd2ff25, BRF_ESS | BRF_PRG },	//  4	HuC6280 Program
	
	{ "fb05.11a",           0x08000, 0x09802924, BRF_GRA },				//  5	Characters
	{ "fb04.9a",            0x08000, 0xec25b895, BRF_GRA },				//  6

	{ "fb07.17a",           0x10000, 0xe932268b, BRF_GRA },				//  7	Tiles 1
	{ "fb06.15a",           0x10000, 0xc4dd38c0, BRF_GRA },				//  8
	
	{ "fb09.22a",           0x20000, 0x1395e9be, BRF_GRA },				//  9	Tiles 2
	{ "fb08.21a",           0x20000, 0x4d7464db, BRF_GRA },				// 10

	{ "fb01.4a",            0x20000, 0x99b0cd92, BRF_GRA },				// 11	Sprites
	{ "fb03.7a",            0x20000, 0x0e7ea74d, BRF_GRA },				// 12
	{ "fb00.2a",            0x20000, 0xf7df3fd7, BRF_GRA },				// 13
	{ "fb02.5a",            0x20000, 0x84e8da9d, BRF_GRA },				// 14
	
	{ "fa11.11k",           0x20000, 0x4e547bad, BRF_SND },				// 15	Samples
	
	{ "mb7114h.21k",        0x00100, 0xad26e8d4, BRF_OPT},				// 16	PROMs
	
	{ "pal16l8b-ta-1.15k",  0x00104, 0x79a87527, BRF_OPT},				// 17	PLDs
	{ "pal16r4a-ta-2.16k",  0x00104, 0xeca31311, BRF_OPT},				// 18
	{ "pal16l8a-ta-3.17k",  0x00104, 0x6c324919, BRF_OPT},				// 19
	{ "pal16l8a-ta-4.11m",  0x00104, 0x116177fa, BRF_OPT},				// 20
};

STD_ROM_PICK(Secretag)
STD_ROM_FN(Secretag)


// Secret Agent (Japan revision 2)
// DE-0322-2 PCB 

static struct BurnRomInfo SecretagjRomDesc[] = {
	{ "fc14-2.17l",         0x10000, 0xe4cc767d, BRF_ESS | BRF_PRG },	//  0	68000 Program Code
	{ "fc12-2.9l",          0x10000, 0x8a589c90, BRF_ESS | BRF_PRG },	//  1
	{ "fc15.19l",           0x10000, 0x106bb26c, BRF_ESS | BRF_PRG },	//  2
	{ "fc13.11l",           0x10000, 0x90523413, BRF_ESS | BRF_PRG },	//  3
	
	{ "fc10.5h",            0x10000, 0xdfd2ff25, BRF_ESS | BRF_PRG },	//  4	HuC6280 Program
	
	{ "fc05.11a",           0x08000, 0x09802924, BRF_GRA },				//  5	Characters
	{ "fc04.9a",            0x08000, 0xec25b895, BRF_GRA },				//  6

	{ "fc07.17a",           0x10000, 0xe932268b, BRF_GRA },				//  7	Tiles 1
	{ "fc06.15a",           0x10000, 0xc4dd38c0, BRF_GRA },				//  8
	
	{ "fc09.22a",           0x20000, 0x1395e9be, BRF_GRA },				//  9	Tiles 2
	{ "fc08.21a",           0x20000, 0x4d7464db, BRF_GRA },				// 10

	{ "fc01.4a",            0x20000, 0x99b0cd92, BRF_GRA },				// 11	Sprites
	{ "fc03.7a",            0x20000, 0x0e7ea74d, BRF_GRA },				// 12
	{ "fc00.2a",            0x20000, 0xf7df3fd7, BRF_GRA },				// 13
	{ "fc02.5a",            0x20000, 0x84e8da9d, BRF_GRA },				// 14
	
	{ "fc11.11k",           0x20000, 0x4e547bad, BRF_SND },				// 15	Samples
	
	{ "mb7114h.21k",        0x00100, 0xad26e8d4, BRF_OPT},				// 16	PROMs
	
	{ "pal16l8b-ta-1.15k",  0x00104, 0x79a87527, BRF_OPT},				// 17	PLDs
	{ "pal16r4a-ta-2.16k",  0x00104, 0xeca31311, BRF_OPT},				// 18
	{ "pal16l8a-ta-3.17k",  0x00104, 0x6c324919, BRF_OPT},				// 19
	{ "pal16l8a-ta-4.11m",  0x00104, 0x116177fa, BRF_OPT},				// 20
};

STD_ROM_PICK(Secretagj)
STD_ROM_FN(Secretagj)


// Sly Spy (US revision 4)
// DE-0322-3 PCB 

static struct BurnRomInfo SlyspyRomDesc[] = {
	{ "fa14-4.17l",         0x10000, 0x60f16e31, BRF_ESS | BRF_PRG },	//  0	68000 Program Code
	{ "fa12-4.9l",          0x10000, 0xb9b9fdcf, BRF_ESS | BRF_PRG },	//  1
	{ "fa15.19l",           0x10000, 0x04a79266, BRF_ESS | BRF_PRG },	//  2
	{ "fa13.11l",           0x10000, 0x641cc4b3, BRF_ESS | BRF_PRG },	//  3
	
	{ "fa10.5h",            0x10000, 0xdfd2ff25, BRF_ESS | BRF_PRG },	//  4	HuC6280 Program
	
	{ "fa05.11a",           0x08000, 0x09802924, BRF_GRA },				//  5	Characters
	{ "fa04.9a",            0x08000, 0xec25b895, BRF_GRA },				//  6

	{ "fa07.17a",           0x10000, 0xe932268b, BRF_GRA },				//  7	Tiles 1
	{ "fa06.15a",           0x10000, 0xc4dd38c0, BRF_GRA },				//  8
	
	{ "fa09.22a",           0x20000, 0x1395e9be, BRF_GRA },				//  9	Tiles 2
	{ "fa08.21a",           0x20000, 0x4d7464db, BRF_GRA },				// 10
	
	{ "fa01.4a",            0x20000, 0x99b0cd92, BRF_GRA },				// 11	Sprites
	{ "fa03.7a",            0x20000, 0x0e7ea74d, BRF_GRA },				// 12
	{ "fa00.2a",            0x20000, 0xf7df3fd7, BRF_GRA },				// 13
	{ "fa02.5a",            0x20000, 0x84e8da9d, BRF_GRA },				// 14
	
	{ "fa11.11k",           0x20000, 0x4e547bad, BRF_SND },				// 15	Samples
	
	{ "mb7114h.21k",        0x00100, 0xad26e8d4, BRF_OPT},				// 16	PROMs
	
	{ "pal16l8b-ta-1.15k",  0x00104, 0x79a87527, BRF_OPT},				// 17	PLDs
	{ "pal16r4a-ta-2.16k",  0x00104, 0xeca31311, BRF_OPT},				// 18
	{ "pal16l8a-ta-3.17k",  0x00104, 0x6c324919, BRF_OPT},				// 19
	{ "pal16l8a-ta-4.11m",  0x00104, 0x116177fa, BRF_OPT},				// 20
};

STD_ROM_PICK(Slyspy)
STD_ROM_FN(Slyspy)


// Sly Spy (US revision 2)
// DE-0322-3 PCB 

static struct BurnRomInfo Slyspy2RomDesc[] = {
	{ "fa14-2.17l",         0x10000, 0x0e431e39, BRF_ESS | BRF_PRG },	//  0	68000 Program Code
	{ "fa12-2.9l",          0x10000, 0x1b534294, BRF_ESS | BRF_PRG },	//  1
	{ "fa15.19l",           0x10000, 0x04a79266, BRF_ESS | BRF_PRG },	//  2
	{ "fa13.11l",           0x10000, 0x641cc4b3, BRF_ESS | BRF_PRG },	//  3
	
	{ "fa10.5h",            0x10000, 0xdfd2ff25, BRF_ESS | BRF_PRG },	//  4	HuC6280 Program
	
	{ "fa05.11a",           0x08000, 0x09802924, BRF_GRA },				//  5	Characters
	{ "fa04.9a",            0x08000, 0xec25b895, BRF_GRA },				//  6

	{ "fa07.17a",           0x10000, 0xe932268b, BRF_GRA },				//  7	Tiles 1
	{ "fa06.15a",           0x10000, 0xc4dd38c0, BRF_GRA },				//  8
	
	{ "fa09.22a",           0x20000, 0x1395e9be, BRF_GRA },				//  9	Tiles 2
	{ "fa08.21a",           0x20000, 0x4d7464db, BRF_GRA },				// 10

	{ "fa01.4a",            0x20000, 0x99b0cd92, BRF_GRA },				// 11	Sprites
	{ "fa03.7a",            0x20000, 0x0e7ea74d, BRF_GRA },				// 12
	{ "fa00.2a",            0x20000, 0xf7df3fd7, BRF_GRA },				// 13
	{ "fa02.5a",            0x20000, 0x84e8da9d, BRF_GRA },				// 14
	
	{ "fa11.11k",           0x20000, 0x4e547bad, BRF_SND },				// 15	Samples
	
	{ "mb7114h.21k",        0x00100, 0xad26e8d4, BRF_OPT},				// 16	PROMs
	
	{ "pal16l8b-ta-1.15k",  0x00104, 0x79a87527, BRF_OPT},				// 17	PLDs
	{ "pal16r4a-ta-2.16k",  0x00104, 0xeca31311, BRF_OPT},				// 18
	{ "pal16l8a-ta-3.17k",  0x00104, 0x6c324919, BRF_OPT},				// 19
	{ "pal16l8a-ta-4.11m",  0x00104, 0x116177fa, BRF_OPT},				// 20
};

STD_ROM_PICK(Slyspy2)
STD_ROM_FN(Slyspy2)


// Sly Spy (US revision 3)
// DE-0322-3 PCB 
 
static struct BurnRomInfo Slyspy3RomDesc[] = {
	{ "fa14-3.17l",         0x10000, 0x54353a84, BRF_ESS | BRF_PRG },	//  0	68000 Program Code
	{ "fa12-2.9l",          0x10000, 0x1b534294, BRF_ESS | BRF_PRG },	//  1
	{ "fa15.19l",           0x10000, 0x04a79266, BRF_ESS | BRF_PRG },	//  2
	{ "fa13.11l",           0x10000, 0x641cc4b3, BRF_ESS | BRF_PRG },	//  3
	
	{ "fa10.5h",            0x10000, 0xdfd2ff25, BRF_ESS | BRF_PRG },	//  4	HuC6280 Program
	
	{ "fa05.11a",           0x08000, 0x09802924, BRF_GRA },				//  5	Characters
	{ "fa04.9a",            0x08000, 0xec25b895, BRF_GRA },				//  6

	{ "fa07.17a",           0x10000, 0xe932268b, BRF_GRA },				//  7	Tiles 1
	{ "fa06.15a",           0x10000, 0xc4dd38c0, BRF_GRA },				//  8
	
	{ "fa09.22a",           0x20000, 0x1395e9be, BRF_GRA },				//  9	Tiles 2
	{ "fa08.21a",           0x20000, 0x4d7464db, BRF_GRA },				// 10
	
	{ "fa01.4a",            0x20000, 0x99b0cd92, BRF_GRA },				// 11	Sprites
	{ "fa03.7a",            0x20000, 0x0e7ea74d, BRF_GRA },				// 12
	{ "fa00.2a",            0x20000, 0xf7df3fd7, BRF_GRA },				// 13
	{ "fa02.5a",            0x20000, 0x84e8da9d, BRF_GRA },				// 14
	
	{ "fa11.11k",           0x20000, 0x4e547bad, BRF_SND },				// 15	Samples
	
	{ "mb7114h.21k",        0x00100, 0xad26e8d4, BRF_OPT},				// 16	PROMs
	
	{ "pal16l8b-ta-1.15k",  0x00104, 0x79a87527, BRF_OPT},				// 17	PLDs
	{ "pal16r4a-ta-2.16k",  0x00104, 0xeca31311, BRF_OPT},				// 18
	{ "pal16l8a-ta-3.17k",  0x00104, 0x6c324919, BRF_OPT},				// 19
	{ "pal16l8a-ta-4.11m",  0x00104, 0x116177fa, BRF_OPT},				// 20
};

STD_ROM_PICK(Slyspy3)
STD_ROM_FN(Slyspy3)

static INT32 MemIndex()
{
	UINT8 *Next; Next = Mem;

	Drv68KRom              = Next; Next += 0x80000;
	DrvM6502Rom            = Next; Next += 0x08000;
	DrvH6280Rom            = Next; Next += 0x10000;
	DrvMCURom              = Next; Next += 0x01000;
	MSM6295ROM             = Next; Next += 0x40000;

	RamStart               = Next;

	Drv68KRam              = Next; Next += 0x05800;
	DrvM6502Ram            = Next; Next += 0x00600;
	DrvH6280Ram            = Next; Next += 0x02000;
	DrvCharRam             = Next; Next += 0x04000;
	DrvCharCtrl0Ram        = Next; Next += 0x00008;
	DrvCharCtrl1Ram        = Next; Next += 0x00008;
	DrvCharColScrollRam    = Next; Next += 0x00100;
	DrvCharRowScrollRam    = Next; Next += 0x00400;
	DrvVideo1Ram           = Next; Next += 0x04000;
	DrvVideo1Ctrl0Ram      = Next; Next += 0x00008;
	DrvVideo1Ctrl1Ram      = Next; Next += 0x00008;
	DrvVideo1ColScrollRam  = Next; Next += 0x00100;
	DrvVideo1RowScrollRam  = Next; Next += 0x00400;
	DrvVideo2Ram           = Next; Next += 0x04000;
	DrvVideo2Ctrl0Ram      = Next; Next += 0x00008;
	DrvVideo2Ctrl1Ram      = Next; Next += 0x00008;
	DrvVideo2ColScrollRam  = Next; Next += 0x00100;
	DrvVideo2RowScrollRam  = Next; Next += 0x00400;
	DrvPaletteRam          = Next; Next += 0x00800;
	DrvPalette2Ram         = Next; Next += 0x00800;
	DrvSpriteRam           = Next; Next += 0x00800;
	DrvSpriteDMABufferRam  = Next; Next += 0x00800;
	DrvSharedRam           = Next; Next += 0x02000;

	RamEnd                 = Next;

	DrvChars               = Next; Next += 0x1000 * 8 * 8;
	DrvTiles1              = Next; Next += 0x1000 * 16 * 16;
	DrvTiles2              = Next; Next += 0x0800 * 16 * 16;
	DrvSprites             = Next; Next += 0x1000 * 16 * 16;
	DrvPalette             = (UINT32*)Next; Next += 0x00400 * sizeof(UINT32);

	pCharLayerDraw         = (UINT16*)Next; Next += (1024 * 256 * sizeof(UINT16));
	pTile1LayerDraw        = (UINT16*)Next; Next += (1024 * 256 * sizeof(UINT16));
	pTile2LayerDraw        = (UINT16*)Next; Next += (1024 * 256 * sizeof(UINT16));

	MemEnd                 = Next;

	return 0;
}

static void RotateReset(); // forward -dink
static void DrvMCUReset(); // forward
static void DrvMCUCPUReset(); // forward
static void DrvMCUSync(); // ""

static INT32 DrvDoReset()
{
	SekOpen(0);
	SekReset();
	SekClose();
	
	BurnYM3812Reset();
	BurnYM2203Reset();
	MSM6295Reset(0);

	i8751RetVal = 0;
	DrvVBlank = 0;
	DrvSoundLatch = 0;
	DrvFlipScreen = 0;
	DrvPriority = 0;
	memset(DrvTileRamBank, 0, 3);

	nExtraCycles[0] = nExtraCycles[1] = nExtraCycles[2] = 0;

	RotateReset();

	HiscoreReset();

	nPrevBurnCPUSpeedAdjust = -1;

	return 0;
}

static INT32 BaddudesDoReset()
{
	M6502Open(0); // prevent crash while resetting the sound cores in DrvDoReset(); w/Bird Try
	INT32 nRet = DrvDoReset();
	M6502Reset();
	M6502Close();

	if (realMCU) {
		DrvMCUCPUReset();
	}

	return nRet;
}

static INT32 RobocopDoReset()
{
	INT32 nRet = BaddudesDoReset();
	
	h6280Open(0);
	h6280Reset();
	h6280Close();
	
	return nRet;
}

static INT32 SlyspyDoReset()
{
	INT32 nRet = DrvDoReset();
	
	h6280Open(0);
	h6280Reset();
	h6280Close();

	DrvSlyspySoundProt = 0;

	return nRet;
}

// I8751 Simulations

static void BaddudesI8751Write(UINT16 Data)
{
	i8751RetVal = 0;

	switch (Data & 0xffff) {
		case 0x714: i8751RetVal = 0x700; break;
		case 0x73b: i8751RetVal = 0x701; break;
		case 0x72c: i8751RetVal = 0x702; break;
		case 0x73f: i8751RetVal = 0x703; break;
		case 0x755: i8751RetVal = 0x704; break;
		case 0x722: i8751RetVal = 0x705; break;
		case 0x72b: i8751RetVal = 0x706; break;
		case 0x724: i8751RetVal = 0x707; break;
		case 0x728: i8751RetVal = 0x708; break;
		case 0x735: i8751RetVal = 0x709; break;
		case 0x71d: i8751RetVal = 0x70a; break;
		case 0x721: i8751RetVal = 0x70b; break;
		case 0x73e: i8751RetVal = 0x70c; break;
		case 0x761: i8751RetVal = 0x70d; break;
		case 0x753: i8751RetVal = 0x70e; break;
		case 0x75b: i8751RetVal = 0x70f; break;
	}
}

// Video write functions

static void deco_bac06_pf_control_0_w(INT32 Layer, UINT16 *Control0, INT32 Offset, UINT16 Data, UINT16 Mask)
{
	Offset &= 0x03;
	
	Control0[Offset] &= Mask;
	Control0[Offset] += Data;

	if (Offset == 2) {
		DrvTileRamBank[Layer] = Control0[Offset] & 0x01;
	}
}

static void deco_bac06_pf_control_1_w(UINT16 *Control1, INT32 Offset, UINT16 Data, UINT16 Mask)
{
	Offset &= 0x07;
	
	Control1[Offset] &= Mask;
	Control1[Offset] += Data;
}

static UINT16 deco_bac06_pf_data_r(INT32 Layer, UINT16 *RAM, INT32 Offset, UINT16 Mask)
{
	if (DrvTileRamBank[Layer] & 0x01) Offset += 0x1000;

	return RAM[Offset] & Mask;
}

static void deco_bac06_pf_data_w(INT32 Layer, UINT16 *RAM, INT32 Offset, UINT16 Data, UINT16 Mask)
{
	if (DrvTileRamBank[Layer] & 0x01) Offset += 0x1000;
	
	RAM[Offset] &= Mask;
	RAM[Offset] += Data;
}

// Rotation-handler code

static void RotateReset() {
	for (INT32 playernum = 0; playernum < 2; playernum++) {
		nRotate[playernum] = 0; // start out pointing straight up (0=up)
		if (strstr(BurnDrvGetTextA(DRV_NAME), "midres"))
			nRotate[0] = nRotate[1] = 2; // start out pointing straight in Midnight Resistance (2=right)
		nRotateTarget[playernum] = -1;
		nRotateTime[playernum] = 0;
		nRotateHoldInput[0] = nRotateHoldInput[1] = 0;
	}
}

static UINT32 RotationTimer(void) {
    return nCurrentFrame;
}

static void RotateRight(INT32 *v) {
    (*v)--;
    if (*v < 0) *v = 11;
}

static void RotateLeft(INT32 *v) {
    (*v)++;
    if (*v > 11) *v = 0;
}

static UINT8 Joy2Rotate(UINT8 *joy) { // ugly code, but the effect is awesome. -dink
	if (joy[0] && joy[2]) return 7;    // up left
	if (joy[0] && joy[3]) return 1;    // up right

	if (joy[1] && joy[2]) return 5;    // down left
	if (joy[1] && joy[3]) return 3;    // down right

	if (joy[0]) return 0;    // up
	if (joy[1]) return 4;    // down
	if (joy[2]) return 6;    // left
	if (joy[3]) return 2;    // right

	return 0xff;
}

static int dialRotation(INT32 playernum) {
    // p1 = 0, p2 = 1
	UINT8 player[2] = { 0, 0 };
	static UINT8 lastplayer[2][2] = { { 0, 0 }, { 0, 0 } };

    if ((playernum != 0) && (playernum != 1)) {
		//bprintf(PRINT_NORMAL, _T("Strange Rotation address => %06X\n"), playernum);
        return 0;
    }
    if (playernum == 0) {
        player[0] = DrvFakeInput[0]; player[1] = DrvFakeInput[1];
    }
    if (playernum == 1) {
        player[0] = DrvFakeInput[2]; player[1] = DrvFakeInput[3];
    }

    if (player[0] && (player[0] != lastplayer[playernum][0] || (RotationTimer() > nRotateTime[playernum]+0xf))) {
		RotateLeft(&nRotate[playernum]);
        //bprintf(PRINT_NORMAL, _T("Player %d Rotate Left => %06X\n"), playernum+1, nRotate[playernum]);
		nRotateTime[playernum] = RotationTimer();
		nRotateTarget[playernum] = -1;
    }

	if (player[1] && (player[1] != lastplayer[playernum][1] || (RotationTimer() > nRotateTime[playernum]+0xf))) {
        RotateRight(&nRotate[playernum]);
        //bprintf(PRINT_NORMAL, _T("Player %d Rotate Right => %06X\n"), playernum+1, nRotate[playernum]);
        nRotateTime[playernum] = RotationTimer();
		nRotateTarget[playernum] = -1;
	}

	lastplayer[playernum][0] = player[0];
	lastplayer[playernum][1] = player[1];

	return ~(1 << nRotate[playernum]);
}

static UINT8 *rotate_gunpos[2] = {NULL, NULL};
static UINT8 rotate_gunpos_multiplier = 1;

// Gun-rotation memory locations - do not remove this tag. - dink :)
// game     p1           p2           clockwise value in memory  multiplier
// hbarrell 0xff8066     0xff80aa     00 04 08 0c 10 14 18 1c    4
// midres   0x1021bc     0x102238     SAME

static void RotateSetGunPosRAM(UINT8 *p1, UINT8 *p2, UINT8 multiplier) {
	rotate_gunpos[0] = p1;
	rotate_gunpos[1] = p2;
	rotate_gunpos_multiplier = multiplier;
}

static INT32 get_distance(INT32 from, INT32 to) {
// this function finds the easiest way to get from "from" to "to", wrapping at 0 and 7
	INT32 countA = 0;
	INT32 countB = 0;
	INT32 fromtmp = from / rotate_gunpos_multiplier;
	INT32 totmp = to / rotate_gunpos_multiplier;

	while (1) {
		fromtmp++;
		countA++;
		if(fromtmp>7) fromtmp = 0;
		if(fromtmp == totmp || countA > 32) break;
	}

	fromtmp = from / rotate_gunpos_multiplier;
	totmp = to / rotate_gunpos_multiplier;

	while (1) {
		fromtmp--;
		countB++;
		if(fromtmp<0) fromtmp = 7;
		if(fromtmp == totmp || countB > 32) break;
	}

	if (countA > countB) {
		return 1; // go negative
	} else {
		return 0; // go positive
	}
}

static void RotateDoTick() {
	// since the game only allows for 1 rotation every other frame, we have to
	// do this.
	if (nCurrentFrame&1) return;

	for (INT32 i = 0; i < 2; i++) {
		if (rotate_gunpos[i] && (nRotateTarget[i] != -1) && (nRotateTarget[i] != (*rotate_gunpos[i] & 0xff))) {
			if (get_distance(nRotateTarget[i], *rotate_gunpos[i] & 0xff)) {
				RotateRight(&nRotate[i]); // --
			} else {
				RotateLeft(&nRotate[i]);  // ++
			}
			bprintf(0, _T("p%X target %X mempos %X nRotate %X.\n"), i, nRotateTarget[0], *rotate_gunpos[0] & 0xff, nRotate[0]);
			nRotateTry[i]++;
			if (nRotateTry[i] > 10) nRotateTarget[i] = -1; // don't get stuck in a loop if something goes horribly wrong here.
		} else {
			nRotateTarget[i] = -1;
		}
	}
}

static void SuperJoy2Rotate() {
	/// let's try not to over-optimize
	UINT8 FakeDrvInputPort0[4] = {0, 0, 0, 0};
	UINT8 FakeDrvInputPort1[4] = {0, 0, 0, 0};
	UINT8 NeedsSecondStick[2] = {0, 0};

	// prepare for right-stick rotation
	// this is not especially readable though
	for (INT32 i = 0; i < 2; i++) {
		for (INT32 n = 0; n < 4; n++) {
			UINT8* RotationInput = (!i) ? &FakeDrvInputPort0[0] : &FakeDrvInputPort1[0];
			RotationInput[n] = DrvFakeInput[6 + i*4 + n];
			NeedsSecondStick[i] |= RotationInput[n];
		}
	}

	for (INT32 i = 0; i < 2; i++) { // p1 = 0, p2 = 1
		if (!NeedsSecondStick[i])
			nAutoFireCounter[i] = 0;
		if (NeedsSecondStick[i]) { // or using Second Stick
			UINT8 rot = Joy2Rotate(((!i) ? &FakeDrvInputPort0[0] : &FakeDrvInputPort1[0]));
			if (rot != 0xff) {
				nRotateTarget[i] = rot * rotate_gunpos_multiplier;
			}
			nRotateTry[i] = 0;

			if (~DrvDip[2] & 1) {
				// fake auto-fire - there's probably a more elegant solution for this
				if (nAutoFireCounter[i]++ & 0x4)
				{
					DrvInput[i] &= 0xef; // remove the fire bit &= ~0x10; //
				}
				else
				{
					DrvInput[i] |= 0x10; // turn on the fire bit
				}
			}
		}
		else if (DrvFakeInput[4 + i]) { //  rotate-button had been pressed
			UINT8 rot = Joy2Rotate(((!i) ? &DrvInputPort0[0] : &DrvInputPort1[0]));
			if (rot != 0xff) {
				nRotateTarget[i] = rot * rotate_gunpos_multiplier;
			}
			//DrvInput[i] &= ~0xf; // cancel out directionals since they are used to rotate here.
			DrvInput[i] = (DrvInput[i] & ~0xf) | (nRotateHoldInput[i] & 0xf); // for midnight resistance! be able to duck + change direction of gun.
			nRotateTry[i] = 0;
		} 
		else { // cache joystick UDLR if the rotate button isn't pressed.
			// This feature is for Midnight Resistance, if you are crawling on the
			// ground and need to rotate your gun WITHOUT getting up.
			nRotateHoldInput[i] = DrvInput[i];
		}
	}

	RotateDoTick();
}

// end Rotation-handler

// i8751 MCU, currently only for hbarrel & birdtry.

static UINT8 mcu_read_port(INT32 port)
{
	if (!(port >= MCS51_PORT_P0 && port <= MCS51_PORT_P3))
		return 0xff;

	if ((port & 0x3) == 0)
	{
		if (~i8751PortData[2] & 0x10)
			return i8751Command >> 8;
		if (~i8751PortData[2] & 0x20)
			return i8751Command & 0xff;
	}

	return 0xff;
}

static void mcu_write_port(INT32 port, UINT8 data)
{
	if (!(port >= MCS51_PORT_P0 && port <= MCS51_PORT_P3))
		return;

	port &= 0x3;

	if (port == 2)
	{
		if (~data & 0x4 && i8751PortData[2] & 0x4) {
			SekSetIRQLine(5, CPU_IRQSTATUS_AUTO);
		}
		if (~data & 0x8) {
			mcs51_set_irq_line(MCS51_INT1_LINE, CPU_IRQSTATUS_NONE);
		}
		if (data & 0x40 && ~i8751PortData[2] & 0x40) {
			i8751RetVal = (i8751RetVal & 0xff00) | (i8751PortData[0]);
		}
		if (data & 0x80 && ~i8751PortData[2] & 0x80) {
			i8751RetVal = (i8751RetVal & 0xff) | (i8751PortData[0] << 8);
		}
	}

	i8751PortData[port] = data;
}

static void DrvMCUInit()
{
	mcs51_init();
	mcs51_set_program_data(DrvMCURom);
	mcs51_set_write_handler(mcu_write_port);
	mcs51_set_read_handler(mcu_read_port);

	DrvMCUReset();
}

static void DrvMCUExit() {
	mcs51_exit();
}

INT32 DrvMCURun(INT32 cycles)
{
	return mcs51Run(cycles);
}

static INT32 DrvMCUScan(INT32 nAction)
{
	mcs51_scan(nAction);

	//SCAN_VAR(i8751RetVal); // in DrvScan (also used by MCU Simulation)
	SCAN_VAR(i8751Command);
	SCAN_VAR(i8751PortData);

	return 0;
}

static void DrvMCUWrite(UINT16 data)
{
	DrvMCUSync();

	i8751Command = data;

	if (i8751PortData[2] & 8) {
		mcs51_set_irq_line(MCS51_INT1_LINE, CPU_IRQSTATUS_ACK);
	}
}

static void DrvMCUSync()
{
	INT32 todo = ((double)SekTotalCycles() * (8000000/12) / 10000000) - nCyclesDone[2];

	nCyclesDone[2] += DrvMCURun((todo > 0) ? todo : 0);
}

static void DrvMCUReset()
{ // game calls this!
	i8751RetVal = i8751Command = 0;
}

static void DrvMCUCPUReset()
{ // boot & f3 resets this!
	DrvMCUReset();
	memset(&i8751PortData, 0, sizeof(i8751PortData));
	mcs51_reset();
}

// Normal hardware cpu memory handlers

static UINT8 __fastcall Dec068KReadByte(UINT32 a)
{
	if (a >= 0x244000 && a <= 0x245fff) {
		INT32 Offset = a - 0x244000;
		if (DrvTileRamBank[0] & 0x01) Offset += 0x2000;
		return DrvCharRam[Offset ^ 1];
	}
	
	if (a >= 0x24a000 && a <= 0x24a7ff) {
		INT32 Offset = a - 0x24a000;
		if (DrvTileRamBank[1] & 0x01) Offset += 0x2000;
		return DrvVideo1Ram[Offset];
	}	
	
	if (a >= 0x24d000 && a <= 0x24d7ff) {
		INT32 Offset = a - 0x24d000;
		if (DrvTileRamBank[2] & 0x01) Offset += 0x2000;
		return DrvVideo2Ram[Offset];
	}
	
	if (a >= 0x300000 && a <= 0x30001f) {
		return dialRotation((a - 0x300000) / 8);
	}

	if ((a&0xfffff0) == 0x804030) return 0; // nop?

	switch (a) {
		case 0x30c000: {
			return 0xff - DrvInput[1];
		}
		
		case 0x30c001: {
			return 0xff - DrvInput[0];
		}
		
		case 0x30c003: {
			return (0x7f - DrvInput[2]) | ((DrvVBlank) ? 0x80 : 0x00);
		}
		
		case 0x30c004: {
			return DrvDip[1];
		}
		
		case 0x30c005: {
			return DrvDip[0];
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("68K Read byte => %06X\n"), a);
		}
	}
	
	return 0;
}

static void __fastcall Dec068KWriteByte(UINT32 a, UINT8 d)
{
	if (a >= 0x244000 && a <= 0x245fff) {
		INT32 Offset = a - 0x244000;
		if (DrvTileRamBank[0] & 0x01) Offset += 0x2000;
		DrvCharRam[Offset ^ 1] = d;
		return;
	}
	
	if (a >= 0x24a000 && a <= 0x24a7ff) {
		INT32 Offset = a - 0x24a000;
		if (DrvTileRamBank[1] & 0x01) Offset += 0x2000;
		DrvVideo1Ram[Offset] = d;
		return;
	}	
	
	if (a >= 0x24d000 && a <= 0x24d7ff) {
		INT32 Offset = a - 0x24d000;
		if (DrvTileRamBank[2] & 0x01) Offset += 0x2000;
		DrvVideo2Ram[Offset] = d;
		return;
	}
	
	switch (a) {
		case 0x30c011: {
			DrvPriority = d;
			return;
		}
		
		case 0x30c015: {
			DrvSoundLatch = d;
			M6502SetIRQLine(M6502_INPUT_LINE_NMI, CPU_IRQSTATUS_AUTO);
			return;
		}
		
		case 0x30c01f: {
			if (realMCU) {
				DrvMCUSync();
				DrvMCUReset();
			}
			i8751RetVal = 0;
			return;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("68K Write byte => %06X, %02X\n"), a, d);
		}
	}
}

static UINT16 __fastcall Dec068KReadWord(UINT32 a)
{
	if (a >= 0x244000 && a <= 0x245fff) {
		UINT16 *RAM = (UINT16*)DrvCharRam;
		INT32 Offset = (a - 0x244000) >> 1;
		if (DrvTileRamBank[0] & 0x01) Offset += 0x1000;
		return BURN_ENDIAN_SWAP_INT16(RAM[Offset]);
	}
	
	if (a >= 0x24a000 && a <= 0x24a7ff) {
		UINT16 *RAM = (UINT16*)DrvVideo1Ram;
		INT32 Offset = (a - 0x24a000) >> 1;
		if (DrvTileRamBank[1] & 0x01) Offset += 0x1000;
		return BURN_ENDIAN_SWAP_INT16(RAM[Offset]);
	}	
	
	if (a >= 0x24d000 && a <= 0x24d7ff) {
		UINT16 *RAM = (UINT16*)DrvVideo2Ram;
		INT32 Offset = (a - 0x24d000) >> 1;
		if (DrvTileRamBank[2] & 0x01) Offset += 0x1000;
		return BURN_ENDIAN_SWAP_INT16(RAM[Offset]);
	}
	
	if (a >= 0x300000 && a <= 0x30001f) {
		return dialRotation((a - 0x300000) / 8);
	}
	
	switch (a) {
		case 0x30c000: {
			return ((0xff - DrvInput[1]) << 8) | (0xff - DrvInput[0]);
		}
		
		case 0x30c002: {
			return (0xff7f - DrvInput[2]) | ((DrvVBlank) ? 0x80 : 0x00);
		}
		
		case 0x30c004: {
			return (DrvDip[1] << 8) | DrvDip[0];
		}
		
		case 0x30c008: {
			if (realMCU) DrvMCUSync();

			return i8751RetVal;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("68K Read word => %06X\n"), a);
		}
	}
	
	return 0;
}

static void __fastcall Dec068KWriteWord(UINT32 a, UINT16 d)
{
	if (a >= 0x244000 && a <= 0x245fff) {
		UINT16 *RAM = (UINT16*)DrvCharRam;
		INT32 Offset = (a - 0x244000) >> 1;
		if (DrvTileRamBank[0] & 0x01) Offset += 0x1000;
		RAM[Offset] = BURN_ENDIAN_SWAP_INT16(d);
		return;
	}
	
	if (a >= 0x24a000 && a <= 0x24a7ff) {
		UINT16 *RAM = (UINT16*)DrvVideo1Ram;
		INT32 Offset = (a - 0x24a000) >> 1;
		if (DrvTileRamBank[1] & 0x01) Offset += 0x1000;
		RAM[Offset] = BURN_ENDIAN_SWAP_INT16(d);
		return;
	}	
	
	if (a >= 0x24d000 && a <= 0x24d7ff) {
		UINT16 *RAM = (UINT16*)DrvVideo2Ram;
		INT32 Offset = (a - 0x24d000) >> 1;
		if (DrvTileRamBank[2] & 0x01) Offset += 0x1000;
		RAM[Offset] = BURN_ENDIAN_SWAP_INT16(d);
		return;
	}
	
	if (a >= 0x31c000 && a <= 0x31c7ff) {
		// ???
		return;
	}

	if (a >= 0x249800 && a <= 0x249fff) {
		// birdtry unmapped writes
		return;
	}
	
	if (a >= 0xffc800 && a <= 0xffc8ff) {
		// ???
		return;
	}
	
	switch (a) {
		case 0x240000:
		case 0x240002:
		case 0x240004:
		case 0x240006: {		
			UINT16 *Control0 = (UINT16*)DrvCharCtrl0Ram;
			Control0[(a - 0x240000) >> 1] = BURN_ENDIAN_SWAP_INT16(d);
			if (a == 0x240004) {
				DrvTileRamBank[0] = d & 0x01;
				if (DrvTileRamBank[0]) bprintf(PRINT_IMPORTANT, _T("68K Set Tile RAM Bank 0\n"));
			}
			return;
		}
		
		case 0x240010:
		case 0x240012:
		case 0x240014:
		case 0x240016: {		
			UINT16 *Control1 = (UINT16*)DrvCharCtrl1Ram;
			Control1[(a - 0x240010) >> 1] = BURN_ENDIAN_SWAP_INT16(d);
			return;
		}
		
		case 0x246000:
		case 0x246002:
		case 0x246004:
		case 0x246006: {		
			UINT16 *Control0 = (UINT16*)DrvVideo1Ctrl0Ram;
			Control0[(a - 0x246000) >> 1] = BURN_ENDIAN_SWAP_INT16(d);
			if (a == 0x246004) {
				DrvTileRamBank[1] = d & 0x01;
				if (DrvTileRamBank[1]) bprintf(PRINT_IMPORTANT, _T("68K Set Tile RAM Bank 1\n"));
			}
			return;
		}
		
		case 0x246010:
		case 0x246012:
		case 0x246014:
		case 0x246016: {		
			UINT16 *Control1 = (UINT16*)DrvVideo1Ctrl1Ram;
			Control1[(a - 0x246010) >> 1] = BURN_ENDIAN_SWAP_INT16(d);
			return;
		}
		
		case 0x24c000:
		case 0x24c002:
		case 0x24c004:
		case 0x24c006: {		
			UINT16 *Control0 = (UINT16*)DrvVideo2Ctrl0Ram;
			Control0[(a - 0x24c000) >> 1] = BURN_ENDIAN_SWAP_INT16(d);
			if (a == 0x24c004) {
				DrvTileRamBank[2] = d & 0x01;
				if (DrvTileRamBank[2]) bprintf(PRINT_IMPORTANT, _T("68K Set Tile RAM Bank 2\n"));
			}
			return;
		}
		
		case 0x24c010:
		case 0x24c012:
		case 0x24c014:
		case 0x24c016: {		
			UINT16 *Control1 = (UINT16*)DrvVideo2Ctrl1Ram;
			Control1[(a - 0x24c010) >> 1] = BURN_ENDIAN_SWAP_INT16(d);
			return;
		}
		
		case 0x30c010: {
			DrvPriority = d;
			return;
		}
		
		case 0x30c012: {
			memcpy(DrvSpriteDMABufferRam, DrvSpriteRam, 0x800);
			return;
		}
		
		case 0x30c014: {
			DrvSoundLatch = d & 0xff;
			M6502SetIRQLine(M6502_INPUT_LINE_NMI, CPU_IRQSTATUS_AUTO);
			return;
		}
		
		case 0x30c016: {
			if (Dec0Game == DEC0_GAME_BADDUDES) BaddudesI8751Write(d);
			if (realMCU) {
				DrvMCUWrite(d);
			}

			if (!realMCU) SekSetIRQLine(5, CPU_IRQSTATUS_AUTO);

			return;
		}
		
		case 0x30c018: {
			SekSetIRQLine(6, CPU_IRQSTATUS_NONE);
			return;
		}

		case 0x30c01a: {
			// NOP
			return;
		}
		
		case 0x30c01e: {
			if (realMCU) {
				DrvMCUSync();
				DrvMCUReset();
			}

			i8751RetVal = 0;
			return;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("68K Write word => %06X, %04X\n"), a, d);
		}
	}
}

static UINT8 Dec0SoundReadByte(UINT16 a)
{
	switch (a) {
		case 0x3000: {
			return DrvSoundLatch;
		}
		
		case 0x3800: {
			return MSM6295Read(0);
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("M6502 Read Byte %04X\n"), a);
		}
	}

	return 0;
}

static void Dec0SoundWriteByte(UINT16 a, UINT8 d)
{
	switch (a) {
		case 0x0800: {
			BurnYM2203Write(0, 0, d);
			return;
		}
		
		case 0x0801: {
			BurnYM2203Write(0, 1, d);
			return;
		}
		
		case 0x1000: {
			BurnYM3812Write(0, 0, d);
			return;
		}
		
		case 0x1001: {
			BurnYM3812Write(0, 1, d);
			return;
		}
		
		case 0x3800: {
			MSM6295Write(0, d);
			return;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("M6502 Write Byte %04X, %02X\n"), a, d);
		}
	}
}

// Hippodrome hardware cpu memory handlers

static UINT8 __fastcall HippodrmShared68KReadByte(UINT32 a)
{
	INT32 Offset = (a - 0x180000) >> 1;
	return DrvSharedRam[Offset];
}

static void __fastcall HippodrmShared68KWriteByte(UINT32 a, UINT8 d)
{
	INT32 Offset = (a - 0x180000) >> 1;
	DrvSharedRam[Offset] = d;
}

static UINT16 __fastcall HippodrmShared68KReadWord(UINT32 a)
{
	INT32 Offset = (a - 0x180000) >> 1;
	return DrvSharedRam[Offset];
}

static void __fastcall HippodrmShared68KWriteWord(UINT32 a, UINT16 d)
{
	INT32 Offset = (a - 0x180000) >> 1;
	DrvSharedRam[Offset] = d & 0xff;
}

static UINT8 HippodrmH6280ReadProg(UINT32 Address)
{
	if (Address >= 0x1a1000 && Address <= 0x1a17ff) {
		INT32 Offset = (Address - 0x1a1000) ^ 1;
		if (Offset & 0x01) {
			return deco_bac06_pf_data_r(2, (UINT16*)DrvVideo2Ram, Offset >> 1, 0x00ff);
		} else {
			return deco_bac06_pf_data_r(2, (UINT16*)DrvVideo2Ram, Offset >> 1, 0xff00) >> 8;
		}
	}
	
	switch (Address) {
		case 0x1ff403: {
			return DrvVBlank;
		}
	}
	
	bprintf(PRINT_NORMAL, _T("H6280 Read Prog %x\n"), Address);
	
	return 0;
}

static void HippodrmH6280WriteProg(UINT32 Address, UINT8 Data)
{
	if (Address >= 0x1a0000 && Address <= 0x1a0007) {
		INT32 Offset = Address - 0x1a0000;
		if (Offset & 0x01) {
			deco_bac06_pf_control_0_w(2, (UINT16*)DrvVideo2Ctrl0Ram, Offset >> 1, Data << 8, 0x00ff);
		} else {
			deco_bac06_pf_control_0_w(2, (UINT16*)DrvVideo2Ctrl0Ram, Offset >> 1, Data, 0xff00);
		}
		return;
	}
	
	if (Address >= 0x1a0010 && Address <= 0x1a001f) {
		INT32 Offset = (Address - 0x1a0010) ^ 1;
		if (Offset < 0x04) {
			if (Offset & 0x01) {
				deco_bac06_pf_control_1_w((UINT16*)DrvVideo2Ctrl1Ram, Offset >> 1, Data, 0xff00);
			} else {
				deco_bac06_pf_control_1_w((UINT16*)DrvVideo2Ctrl1Ram, Offset >> 1, Data << 8, 0x00ff);
			}
		} else {
			if (Offset & 0x01) {
				deco_bac06_pf_control_1_w((UINT16*)DrvVideo2Ctrl1Ram, Offset >> 1, Data, 0xff00);
			} else {
				deco_bac06_pf_control_1_w((UINT16*)DrvVideo2Ctrl1Ram, Offset >> 1, Data, 0xff00);
			}
		}
		return;
	}
	
	if (Address >= 0x1a1000 && Address <= 0x1a17ff) {
		INT32 Offset = (Address - 0x1a1000) ^ 1;
		if (Offset & 0x01) {
			deco_bac06_pf_data_w(2, (UINT16*)DrvVideo2Ram, Offset >> 1, Data, 0xff00);
		} else {
			deco_bac06_pf_data_w(2, (UINT16*)DrvVideo2Ram, Offset >> 1, Data << 8, 0x00ff);
		}
		return;
		
	}
	
	if (Address >= 0x1ff400 && Address <= 0x1ff403) {
		h6280_irq_status_w(Address - 0x1ff400, Data);
		return;
	}
	
	bprintf(PRINT_NORMAL, _T("H6280 Write Prog %x, %x\n"), Address, Data);
}

// Robocop hardware cpu memory handlers

static UINT8 __fastcall RobocopShared68KReadByte(UINT32 a)
{
	INT32 Offset = (a - 0x180000) >> 1;
	return DrvSharedRam[Offset];
}

static void __fastcall RobocopShared68KWriteByte(UINT32 a, UINT8 d)
{
	INT32 Offset = (a - 0x180000) >> 1;
	DrvSharedRam[Offset] = d;
	if (Offset == 0x7ff) {
		h6280SetIRQLine(0, CPU_IRQSTATUS_AUTO);
	}
}

static UINT16 __fastcall RobocopShared68KReadWord(UINT32 a)
{
	INT32 Offset = (a - 0x180000) >> 1;
	return DrvSharedRam[Offset];
}

static void __fastcall RobocopShared68KWriteWord(UINT32 a, UINT16 d)
{
	INT32 Offset = (a - 0x180000) >> 1;
	DrvSharedRam[Offset] = d & 0xff;
	if (Offset == 0x7ff) {
		h6280SetIRQLine(0, CPU_IRQSTATUS_AUTO);
	}
}

static UINT8 RobocopH6280ReadProg(UINT32 Address)
{
	bprintf(PRINT_NORMAL, _T("H6280 Read Prog %x\n"), Address);
	
	return 0;
}

static void RobocopH6280WriteProg(UINT32 Address, UINT8 Data)
{
	if (Address >= 0x1ff400 && Address <= 0x1ff403) {
		h6280_irq_status_w(Address - 0x1ff400, Data);
		return;
	}
	
	bprintf(PRINT_NORMAL, _T("H6280 Write Prog %x, %x\n"), Address, Data);
}

// Sly Spy hardware cpu memory handlers

static UINT8 __fastcall Slyspy68KReadByte(UINT32 a)
{
	if (a >= 0x31c000 && a <= 0x31c00f) {
		INT32 Offset = (a - 0x31c000) >> 1;
		
		switch (Offset << 1) {
			case 0x00: return 0x00;
			case 0x02: return 0x13;
			case 0x04: return 0x00;
			case 0x06: return 0x02;
			case 0x0c: {
				UINT16 *mem = (UINT16*)Drv68KRam;
				return mem[0x2028/2] >> 8;
			}
		}
		
		return 0;
	}
	
	switch (a) {
		case 0x314008: {
			return DrvDip[1];
		}
		
		case 0x314009: {
			return DrvDip[0];
		}
		
		case 0x31400a: {
			return 0xff - DrvInput[1];
		}
		
		case 0x31400b: {
			return 0xff - DrvInput[0];
		}
		
		case 0x31400d: {
			return (0xf7 - DrvInput[2]) | ((DrvVBlank) ? 0x08 : 0x00);
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("68K Read byte => %06X\n"), a);
		}
	}
	
	return 0;
}

static void __fastcall Slyspy68KWriteByte(UINT32 a, UINT8 d)
{
	switch (a) {
		case 0x300000:
		case 0x300001:
		case 0x300002:
		case 0x300003:
		case 0x300004:
		case 0x300005:
		case 0x300006:
		case 0x300007: {
			DrvVideo2Ctrl0Ram[(a - 0x300000) ^ 1] = d;
			if (a == 0x300005) {
				DrvTileRamBank[2] = d & 0x01;
				if (DrvTileRamBank[2]) bprintf(PRINT_IMPORTANT, _T("68K Set Tile RAM Bank 2\n"));
			}
			return;
		}
		
		case 0x300010:
		case 0x300011:
		case 0x300012:
		case 0x300013:
		case 0x300014:
		case 0x300015:
		case 0x300016:
		case 0x300017: {
			DrvVideo2Ctrl1Ram[(a - 0x300010) ^ 1] = d;
			return;
		}
		
		case 0x314001: {
			DrvSoundLatch = d;
			h6280SetIRQLine(H6280_INPUT_LINE_NMI, CPU_IRQSTATUS_AUTO);
			return;
		}
		
		case 0x314003: {
			DrvPriority = d;
			return;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("68K Write byte => %06X, %02X\n"), a, d);
		}
	}
}

static UINT16 __fastcall Slyspy68KReadWord(UINT32 a)
{
	if (a >= 0x31c000 && a <= 0x31c00f) {
		INT32 Offset = (a - 0x31c000) >> 1;
		
		switch (Offset << 1) {
			case 0x00: return 0x00;
			case 0x02: return 0x13;
			case 0x04: return 0x00;
			case 0x06: return 0x02;
			case 0x0c: {
				UINT16 *mem = (UINT16*)Drv68KRam;
				return mem[0x2028/2] >> 8;
			}
		}
		
		return 0;
	}
	
	switch (a) {
		case 0x244000: {
			DrvSlyspyProtValue++;
			DrvSlyspyProtValue = DrvSlyspyProtValue % 4;
			SlyspySetProtectionMap(DrvSlyspyProtValue);
			return 0;
		}
		
		case 0x314008: {
			return (DrvDip[1] << 8) | DrvDip[0];
		}
		
		case 0x31400a: {
			return ((0xff - DrvInput[1]) << 8) | (0xff - DrvInput[0]);
		}
		
		case 0x31400c: {
			return 0xff00 | ((0xf7 - DrvInput[2]) | ((DrvVBlank) ? 0x08 : 0x00));
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("68K Read word => %06X\n"), a);
		}
	}
	
	return 0;
}

static void __fastcall Slyspy68KWriteWord(UINT32 a, UINT16 d)
{
	if (a >= 0x31c000 && a <= 0x31c00f) {
		// nop
		return;
	}
	
	switch (a) {
		case 0x24a000: {
			DrvSlyspyProtValue = 0;
			SlyspySetProtectionMap(DrvSlyspyProtValue);
			return;
		}
		
		case 0x300000:
		case 0x300002:
		case 0x300004:
		case 0x300006: {		
			UINT16 *Control0 = (UINT16*)DrvVideo2Ctrl0Ram;
			Control0[(a - 0x300000) >> 1] = d;
			if (a == 0x300004) {
				DrvTileRamBank[2] = d & 0x01;
				if (DrvTileRamBank[2]) bprintf(PRINT_IMPORTANT, _T("68K Set Tile RAM Bank 2\n"));
			}
			return;
		}
		
		case 0x300010:
		case 0x300012:
		case 0x300014:
		case 0x300016: {		
			UINT16 *Control1 = (UINT16*)DrvVideo2Ctrl1Ram;
			Control1[(a - 0x300010) >> 1] = d;
			return;
		}
		
		case 0x314000: {
			DrvSoundLatch = d & 0xff;
			h6280SetIRQLine(H6280_INPUT_LINE_NMI, CPU_IRQSTATUS_AUTO);
			return;
		}
		
		case 0x314002: {
			DrvPriority = d;
			return;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("68K Write word => %06X, %04X\n"), a, d);
		}
	}
}

static void __fastcall SlyspyProt68KWriteByte(UINT32 a, UINT8 d)
{
	switch (a) {
		case 0x240000:
		case 0x240001:
		case 0x240002:
		case 0x240003:
		case 0x240004:
		case 0x240005:
		case 0x240006:
		case 0x240007: {
			DrvVideo1Ctrl0Ram[(a - 0x240000) ^ 1] = d;
			if (a == 0x240005) {
				DrvTileRamBank[1] = d & 0x01;
				if (DrvTileRamBank[1]) bprintf(PRINT_IMPORTANT, _T("68K Set Tile RAM Bank 1\n"));
			}
			return;
		}
		
		case 0x240010:
		case 0x240011:
		case 0x240012:
		case 0x240013:
		case 0x240014:
		case 0x240015:
		case 0x240016:
		case 0x240017: {
			DrvVideo1Ctrl1Ram[(a - 0x240010) ^ 1] = d;
			return;
		}
		
		case 0x248000:
		case 0x248001:
		case 0x248002:
		case 0x248003:
		case 0x248004:
		case 0x248005:
		case 0x248006:
		case 0x248007: {		
			DrvCharCtrl0Ram[(a - 0x248000) ^ 1] = d;
			if (a == 0x248005) {
				DrvTileRamBank[0] = d & 0x01;
				if (DrvTileRamBank[0]) bprintf(PRINT_IMPORTANT, _T("68K Set Tile RAM Bank 0\n"));
			}
			return;
		}
		
		case 0x248010:
		case 0x248011:
		case 0x248012:
		case 0x248013:
		case 0x248014:
		case 0x248015:
		case 0x248016:
		case 0x248017: {		
			DrvCharCtrl1Ram[(a - 0x248010) ^ 1] = d;
			return;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("68K Write byte => %06X, %02X\n"), a, d);
		}
	}
}

static void __fastcall SlyspyProt68KWriteWord(UINT32 a, UINT16 d)
{
	switch (a) {
		case 0x240000:
		case 0x240002:
		case 0x240004:
		case 0x240006: {		
			UINT16 *Control0 = (UINT16*)DrvVideo1Ctrl0Ram;
			Control0[(a - 0x240000) >> 1] = d;
			if (a == 0x240004) {
				DrvTileRamBank[1] = d & 0x01;
				if (DrvTileRamBank[1]) bprintf(PRINT_IMPORTANT, _T("68K Set Tile RAM Bank 1\n"));
			}
			return;
		}
		
		case 0x240010:
		case 0x240012:
		case 0x240014:
		case 0x240016: {		
			UINT16 *Control1 = (UINT16*)DrvVideo1Ctrl1Ram;
			Control1[(a - 0x240010) >> 1] = d;
			return;
		}
		
		case 0x244000: {
			// ???
			return;
		}
		
		case 0x248000:
		case 0x248002:
		case 0x248004:
		case 0x248006: {		
			UINT16 *Control0 = (UINT16*)DrvCharCtrl0Ram;
			Control0[(a - 0x248000) >> 1] = d;
			if (a == 0x248004) {
				DrvTileRamBank[0] = d & 0x01;
				if (DrvTileRamBank[0]) bprintf(PRINT_IMPORTANT, _T("68K Set Tile RAM Bank 0\n"));
			}
			return;
		}
		
		case 0x248010:
		case 0x248012:
		case 0x248014:
		case 0x248016: {
			UINT16 *Control1 = (UINT16*)DrvCharCtrl1Ram;
			Control1[(a - 0x248010) >> 1] = d;
			return;
		}
		
		case 0x248800: {
			// ???
			return;
		}
		
		case 0x24a000: {
			DrvSlyspyProtValue = 0;
			SlyspySetProtectionMap(DrvSlyspyProtValue);
			return;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("68K Write word => %06X, %04X\n"), a, d);
		}
	}
}

static void SlyspySetProtectionMap(UINT8 Type)
{
	// I should really set this up to go through handlers, in case a layer's alt RAM bank gets activated,
	// so far I've not seen evidence that the game activates the alt RAM banks and this implementation is much
	// cleaner and quicker
	
	SekMapHandler(8, 0x240000, 0x24ffff, MAP_WRITE);
	SekSetWriteByteHandler(8, SlyspyProt68KWriteByte);
	SekSetWriteWordHandler(8, SlyspyProt68KWriteWord);
	
	switch (Type) {
		case 0: {
			SekMapMemory(DrvVideo1ColScrollRam   , 0x242000, 0x24207f, MAP_WRITE);
			SekMapMemory(DrvVideo1RowScrollRam   , 0x242400, 0x2427ff, MAP_WRITE);
			SekMapMemory(DrvVideo1Ram            , 0x246000, 0x247fff, MAP_WRITE);
			SekMapMemory(DrvCharColScrollRam     , 0x24c000, 0x24c07f, MAP_WRITE);
			SekMapMemory(DrvCharRowScrollRam     , 0x24c400, 0x24c7ff, MAP_WRITE);
			SekMapMemory(DrvCharRam              , 0x24e000, 0x24ffff, MAP_WRITE);
			break;
		}

		case 1: {
			SekMapMemory(DrvCharRam              , 0x248000, 0x249fff, MAP_WRITE);
			SekMapMemory(DrvVideo1Ram            , 0x24c000, 0x24dfff, MAP_WRITE);
			break;
		}

		case 2: {
			SekMapMemory(DrvVideo1Ram            , 0x240000, 0x241fff, MAP_WRITE);
			SekMapMemory(DrvCharRam              , 0x242000, 0x243fff, MAP_WRITE);
			SekMapMemory(DrvCharRam              , 0x24e000, 0x24ffff, MAP_WRITE);
			break;
		}

		case 3: {
			SekMapMemory(DrvCharRam              , 0x240000, 0x241fff, MAP_WRITE);
			SekMapMemory(DrvVideo1Ram            , 0x248000, 0x249fff, MAP_WRITE);
			break;
		}
	}

}

static UINT8 SlyspyH6280ReadProg(UINT32 Address)
{
	if (!(Address >= 0x80000 && Address <= 0xfffff)) return 0;

	Address = (Address & 0x7ffff) | (DrvSlyspySoundProt * 0x80000);

	if ((Address&~0x180001) == 0x20000) {
		DrvSlyspySoundProt++;
		DrvSlyspySoundProt &= 3;

		return 0xff;
	}

	if ((Address&~0x180001) == 0x50000) {
		DrvSlyspySoundProt = 0;

		return 0xff;
	}

	if (DrvSlyspySoundProt == 0) {
		switch (Address) {
			case 0x060000: return MSM6295Read(0);
			case 0x070000: return DrvSoundLatch;
		}
	}
	if (DrvSlyspySoundProt == 1) {
		switch (Address) {
			case 0x090000: return MSM6295Read(0);
			case 0x0c0000: return DrvSoundLatch;
		}
	}
	if (DrvSlyspySoundProt == 2) {
		switch (Address) {
			case 0x110000: return DrvSoundLatch;
			case 0x130000: return MSM6295Read(0);
		}
	}
	if (DrvSlyspySoundProt == 3) {
		switch (Address) {
			case 0x1e0000: return DrvSoundLatch;
			case 0x1f0000: return MSM6295Read(0);
		}
	}

	bprintf(PRINT_NORMAL, _T("H6280 Read Prog %x\n"), Address);
	
	return 0;
}

static void SlyspyH6280WriteProg(UINT32 Address, UINT8 Data)
{
	if (Address >= 0x1ff400 && Address <= 0x1ff403) {
		h6280_irq_status_w(Address - 0x1ff400, Data);
		return;
	}

	if (!(Address >= 0x80000 && Address <= 0xfffff)) return;

	Address = (Address & 0x7ffff) | (DrvSlyspySoundProt * 0x80000);

	if (DrvSlyspySoundProt == 0) {
		switch (Address) {
			case 0x010000:
			case 0x010001:
				BurnYM3812Write(0, Address&1, Data);
				return;

			case 0x030000:
			case 0x030001:
				BurnYM2203Write(0, Address&1, Data);
				return;

			case 0x060000:
				MSM6295Write(0, Data);
				return;
		}
	}

	if (DrvSlyspySoundProt == 1) {
		switch (Address) {
			case 0x0f0000:
			case 0x0f0001:
				BurnYM3812Write(0, Address&1, Data);
				return;

			case 0x0e0000:
			case 0x0e0001:
				BurnYM2203Write(0, Address&1, Data);
				return;

			case 0x090000:
				MSM6295Write(0, Data);
				return;
		}
	}

	if (DrvSlyspySoundProt == 2) {
		switch (Address) {
			case 0x170000:
			case 0x170001:
				BurnYM3812Write(0, Address&1, Data);
				return;

			case 0x140000:
			case 0x140001:
				BurnYM2203Write(0, Address&1, Data);
				return;

			case 0x130000:
				MSM6295Write(0, Data);
				return;
		}
	}

	if (DrvSlyspySoundProt == 3) {
		switch (Address) {
			case 0x190000:
			case 0x190001:
				BurnYM3812Write(0, Address&1, Data);
				return;

			case 0x1c0000:
			case 0x1c0001:
				BurnYM2203Write(0, Address&1, Data);
				return;

			case 0x1f0000:
				MSM6295Write(0, Data);
				return;
		}
	}

	bprintf(PRINT_NORMAL, _T("H6280 Write Prog %x, %x\n"), Address, Data);
}

// Midnight Resistance hardware cpu memory handlers

static UINT8 __fastcall Midres68KReadByte(UINT32 a)
{
#if 0
	if (a >= 0x220000 && a <= 0x2207ff) {
		INT32 Offset = a - 0x220000;
		if (DrvTileRamBank[1] & 0x01) Offset += 0x2000;
		return DrvVideo1Ram[Offset ^ 1];
	}
	
	if (a >= 0x220800 && a <= 0x220fff) {
		// mirror
		INT32 Offset = a - 0x220800;
		if (DrvTileRamBank[1] & 0x01) Offset += 0x2000;
		return DrvVideo1Ram[Offset ^ 1];
	}
	
	if (a >= 0x2a0000 && a <= 0x2a07ff) {
		INT32 Offset = a - 0x2a0000;
		if (DrvTileRamBank[2] & 0x01) Offset += 0x2000;
		return DrvVideo2Ram[Offset ^ 1];
	}
	
	if (a >= 0x320000 && a <= 0x321fff) {
		INT32 Offset = a - 0x320000;
		if (DrvTileRamBank[0] & 0x01) Offset += 0x2000;
		return DrvCharRam[Offset ^ 1];
	}
#endif
	switch (a) {
		case 0x180009: {
			return (0xf7 - DrvInput[2]) | ((DrvVBlank) ? 0x08 : 0x00);
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("68K Read byte => %06X PC: %X\n"), a, SekGetPC(-1));
		}
	}
	
	return 0;
}

static void __fastcall Midres68KWriteByte(UINT32 a, UINT8 d)
{
#if 0
	if (a >= 0x220000 && a <= 0x2207ff) {
		INT32 Offset = a - 0x220000;
		if (DrvTileRamBank[1] & 0x01) Offset += 0x2000;
		DrvVideo1Ram[Offset ^ 1] = d;
		return;
	}
	
	if (a >= 0x220800 && a <= 0x220fff) {
		// mirror
		INT32 Offset = a - 0x220800;
		if (DrvTileRamBank[1] & 0x01) Offset += 0x2000;
		DrvVideo1Ram[Offset ^ 1] = d;
		return;
	}
	
	if (a >= 0x2a0000 && a <= 0x2a07ff) {
		INT32 Offset = a - 0x2a0000;
		if (DrvTileRamBank[2] & 0x01) Offset += 0x2000;
		DrvVideo2Ram[Offset ^ 1] = d;
		return;
	}
	
	if (a >= 0x320000 && a <= 0x321fff) {
		INT32 Offset = a - 0x320000;
		if (DrvTileRamBank[0] & 0x01) Offset += 0x2000;
		DrvCharRam[Offset ^ 1] = d;
		return;
	}
#endif
	switch (a) {
		case 0x1a0001: {
			DrvSoundLatch = d;
			h6280SetIRQLine(H6280_INPUT_LINE_NMI, CPU_IRQSTATUS_AUTO);
			return;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("68K Write byte => %06X, %02X PC: %X\n"), a, d, SekGetPC(-1));
		}
	}
}

static UINT16 __fastcall Midres68KReadWord(UINT32 a)
{
#if 0
	if (a >= 0x220000 && a <= 0x2207ff) {
		UINT16 *RAM = (UINT16*)DrvVideo1Ram;
		INT32 Offset = (a - 0x220000) >> 1;
		if (DrvTileRamBank[1] & 0x01) Offset += 0x1000;
		return BURN_ENDIAN_SWAP_INT16(RAM[Offset]);
	}
	
	if (a >= 0x220800 && a <= 0x220fff) {
		// mirror
		UINT16 *RAM = (UINT16*)DrvVideo1Ram;
		INT32 Offset = (a - 0x220800) >> 1;
		if (DrvTileRamBank[1] & 0x01) Offset += 0x1000;
		return BURN_ENDIAN_SWAP_INT16(RAM[Offset]);
	}
	
	if (a >= 0x2a0000 && a <= 0x2a07ff) {
		UINT16 *RAM = (UINT16*)DrvVideo2Ram;
		INT32 Offset = (a - 0x2a0000) >> 1;
		if (DrvTileRamBank[2] & 0x01) Offset += 0x1000;
		return BURN_ENDIAN_SWAP_INT16(RAM[Offset]);
	}
	
	if (a >= 0x320000 && a <= 0x321fff) {
		UINT16 *RAM = (UINT16*)DrvCharRam;
		INT32 Offset = (a - 0x320000) >> 1;
		if (DrvTileRamBank[0] & 0x01) Offset += 0x1000;
		return BURN_ENDIAN_SWAP_INT16(RAM[Offset]);
	}
#endif
	switch (a) {
		case 0x180000: {
			return ((0xff - DrvInput[1]) << 8) | (0xff - DrvInput[0]);
		}
		
		case 0x180002: {
			return (DrvDip[1] << 8) | DrvDip[0];
		}
		
		case 0x180004: {
			return dialRotation(0);
		}
		
		case 0x180006: {
			return dialRotation(1);
		}
		
		case 0x180008: {
			return 0xff00 | ((0xf7 - DrvInput[2]) | ((DrvVBlank) ? 0x08 : 0x00));
		}
		
		case 0x18000c: {
			// watchdog?
			return 0;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("68K Read word => %06X PC: %X\n"), a, SekGetPC(-1));
		}
	}
	
	return 0;
}

static void __fastcall Midres68KWriteWord(UINT32 a, UINT16 d)
{
#if 0
	if (a >= 0x220000 && a <= 0x2207ff) {
		UINT16 *RAM = (UINT16*)DrvVideo1Ram;
		INT32 Offset = (a - 0x220000) >> 1;
		if (DrvTileRamBank[1] & 0x01) Offset += 0x1000;
		RAM[Offset] = BURN_ENDIAN_SWAP_INT16(d);
		return;
	}
	
	if (a >= 0x220800 && a <= 0x220fff) {
		// mirror
		UINT16 *RAM = (UINT16*)DrvVideo1Ram;
		INT32 Offset = (a - 0x220800) >> 1;
		if (DrvTileRamBank[1] & 0x01) Offset += 0x1000;
		RAM[Offset] = BURN_ENDIAN_SWAP_INT16(d);
		return;
	}
	
	if (a >= 0x2a0000 && a <= 0x2a07ff) {
		UINT16 *RAM = (UINT16*)DrvVideo2Ram;
		INT32 Offset = (a - 0x2a0000) >> 1;
		if (DrvTileRamBank[2] & 0x01) Offset += 0x1000;
		RAM[Offset] = BURN_ENDIAN_SWAP_INT16(d);
		return;
	}
	
	if (a >= 0x320000 && a <= 0x321fff) {
		UINT16 *RAM = (UINT16*)DrvCharRam;
		INT32 Offset = (a - 0x320000) >> 1;
		if (DrvTileRamBank[0] & 0x01) Offset += 0x1000;
		RAM[Offset] = BURN_ENDIAN_SWAP_INT16(d);
		return;
	}
#endif
	switch (a) {
		case 0x160000: {
			DrvPriority = d;
			return;
		}
		
		case 0x18000a:
		case 0x18000c: {
			// nop?
			return;
		}

		case 0x1a0000: { // midres ending sequence writes here after the credits, it brings the music volume down quite a bit making the drums quite prominent during hiscore name entry.
			DrvSoundLatch = d & 0xff;
			h6280SetIRQLine(H6280_INPUT_LINE_NMI, CPU_IRQSTATUS_AUTO);
			return;
		}
		
		case 0x200000:
		case 0x200002:
		case 0x200004:
		case 0x200006: {		
			UINT16 *Control0 = (UINT16*)DrvVideo1Ctrl0Ram;
			Control0[(a - 0x200000) >> 1] = BURN_ENDIAN_SWAP_INT16(d);
			if (a == 0x200004) {
				DrvTileRamBank[1] = d & 0x01;
				if (DrvTileRamBank[1]) bprintf(PRINT_IMPORTANT, _T("68K Set Tile RAM Bank 1\n"));
			}
			return;
		}
		
		case 0x200010:
		case 0x200012:
		case 0x200014:
		case 0x200016: {		
			UINT16 *Control1 = (UINT16*)DrvVideo1Ctrl1Ram;
			Control1[(a - 0x200010) >> 1] = BURN_ENDIAN_SWAP_INT16(d);
			return;
		}
		
		case 0x280000:
		case 0x280002:
		case 0x280004:
		case 0x280006: {		
			UINT16 *Control0 = (UINT16*)DrvVideo2Ctrl0Ram;
			Control0[(a - 0x280000) >> 1] = BURN_ENDIAN_SWAP_INT16(d);
			if (a == 0x280004) {
				DrvTileRamBank[2] = d & 0x01;
				if (DrvTileRamBank[2]) bprintf(PRINT_IMPORTANT, _T("68K Set Tile RAM Bank 2\n"));
			}
			return;
		}
		
		case 0x280010:
		case 0x280012:
		case 0x280014:
		case 0x280016: {		
			UINT16 *Control1 = (UINT16*)DrvVideo2Ctrl1Ram;
			Control1[(a - 0x280010) >> 1] = BURN_ENDIAN_SWAP_INT16(d);
			return;
		}
		
		case 0x300000:
		case 0x300002:
		case 0x300004:
		case 0x300006: {		
			UINT16 *Control0 = (UINT16*)DrvCharCtrl0Ram;
			Control0[(a - 0x300000) >> 1] = BURN_ENDIAN_SWAP_INT16(d);
			if (a == 0x300004) {
				DrvTileRamBank[0] = d & 0x01;
				if (DrvTileRamBank[0]) bprintf(PRINT_IMPORTANT, _T("68K Set Tile RAM Bank 0\n"));
			}
			return;
		}
		
		case 0x300010:
		case 0x300012:
		case 0x300014:
		case 0x300016: {		
			UINT16 *Control1 = (UINT16*)DrvCharCtrl1Ram;
			Control1[(a - 0x300010) >> 1] = BURN_ENDIAN_SWAP_INT16(d);
			return;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("68K Write word => %06X, %04X PC: %X\n"), a, d, SekGetPC(-1));
		}
	}
}

static UINT8 MidresH6280ReadProg(UINT32 Address)
{
	switch (Address) {
		case 0x130000: {
			return MSM6295Read(0);
		}
		
		case 0x138000: {
			return DrvSoundLatch;
		}
	}
	
	bprintf(PRINT_NORMAL, _T("H6280 Read Prog %x\n"), Address);
	
	return 0;
}

static void MidresH6280WriteProg(UINT32 Address, UINT8 Data)
{
	switch (Address) {
		case 0x0108000: {
			BurnYM3812Write(0, 0, Data);
			return;
		}
		
		case 0x108001: {
			BurnYM3812Write(0, 1, Data);
			return;
		}
		
		case 0x118000: {
			BurnYM2203Write(0, 0, Data);
			return;
		}
		
		case 0x118001: {
			BurnYM2203Write(0, 1, Data);
			return;
		}
		
		case 0x130000: {
			MSM6295Write(0, Data);
			return;
		}
	}
	
	if (Address >= 0x1ff400 && Address <= 0x1ff403) {
		h6280_irq_status_w(Address - 0x1ff400, Data);
		return;
	}
	
	bprintf(PRINT_NORMAL, _T("H6280 Write Prog %x, %x\n"), Address, Data);
}

static INT32 CharPlaneOffsets[4]         = { 0x000000, 0x040000, 0x020000, 0x060000 };
static INT32 RobocopCharPlaneOffsets[4]  = { 0x000000, 0x080000, 0x040000, 0x0c0000 };
static INT32 CharXOffsets[8]             = { 0, 1, 2, 3, 4, 5, 6, 7 };
static INT32 CharYOffsets[8]             = { 0, 8, 16, 24, 32, 40, 48, 56 };
static INT32 Tile1PlaneOffsets[4]        = { 0x080000, 0x180000, 0x000000, 0x100000 };
static INT32 Tile2PlaneOffsets[4]        = { 0x040000, 0x0c0000, 0x000000, 0x080000 };
static INT32 SpritePlaneOffsets[4]       = { 0x100000, 0x300000, 0x000000, 0x200000 };
static INT32 TileXOffsets[16]            = { 128, 129, 130, 131, 132, 133, 134, 135, 0, 1, 2, 3, 4, 5, 6, 7 };
static INT32 TileYOffsets[16]            = { 0, 8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 88, 96, 104, 112, 120 };

static void Dec0YM3812IRQHandler(INT32, INT32 nStatus)
{
	if (nStatus) {
		M6502SetIRQLine(M6502_IRQ_LINE, CPU_IRQSTATUS_ACK);
	} else {
		M6502SetIRQLine(M6502_IRQ_LINE, CPU_IRQSTATUS_NONE);
	}
}

static void Dec1YM3812IRQHandler(INT32, INT32 nStatus)
{
	if (nStatus) {
		h6280SetIRQLine(1, CPU_IRQSTATUS_ACK);
	} else {
		h6280SetIRQLine(1, CPU_IRQSTATUS_NONE);
	}
}

static INT32 Dec0MachineInit()
{
	INT32 nLen;
	
	BurnSetRefreshRate(57.41);
	
	Mem = NULL;
	MemIndex();
	nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(Mem, 0, nLen);
	MemIndex();

	DrvTempRom = (UINT8 *)BurnMalloc(0x80000);
	
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KRom               , 0x000000, 0x05ffff, MAP_ROM);
	SekMapMemory(DrvCharColScrollRam     , 0x242000, 0x24207f, MAP_RAM);
	SekMapMemory(DrvCharRowScrollRam     , 0x242400, 0x2427ff, MAP_RAM);
	SekMapMemory(Drv68KRam + 0x4000      , 0x242800, 0x243fff, MAP_RAM);
	SekMapMemory(DrvVideo1ColScrollRam   , 0x248000, 0x24807f, MAP_RAM);
	SekMapMemory(DrvVideo1RowScrollRam   , 0x248400, 0x2487ff, MAP_RAM);
	SekMapMemory(DrvVideo2ColScrollRam   , 0x24c800, 0x24c87f, MAP_RAM);
	SekMapMemory(DrvVideo2RowScrollRam   , 0x24cc00, 0x24cfff, MAP_RAM);
	SekMapMemory(DrvPaletteRam           , 0x310000, 0x3107ff, MAP_RAM);
	SekMapMemory(DrvPalette2Ram          , 0x314000, 0x3147ff, MAP_RAM);
	SekMapMemory(Drv68KRam               , 0xff8000, 0xffbfff, MAP_RAM);
	SekMapMemory(DrvSpriteRam            , 0xffc000, 0xffc7ff, MAP_RAM);
	SekSetReadByteHandler(0, Dec068KReadByte);
	SekSetWriteByteHandler(0, Dec068KWriteByte);
	SekSetReadWordHandler(0, Dec068KReadWord);
	SekSetWriteWordHandler(0, Dec068KWriteWord);
	SekClose();
	
	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvM6502Ram            , 0x0000, 0x05ff, MAP_RAM);
	M6502MapMemory(DrvM6502Rom            , 0x8000, 0xffff, MAP_ROM);
	M6502SetReadHandler(Dec0SoundReadByte);
	M6502SetWriteHandler(Dec0SoundWriteByte);
	M6502Close();
	
	GenericTilesInit();
	
	BurnYM3812Init(1, 3000000, &Dec0YM3812IRQHandler, 1);
	BurnTimerAttach(&M6502Config, 1500000);
	BurnYM3812SetRoute(0, BURN_SND_YM3812_ROUTE, 0.80, BURN_SND_ROUTE_BOTH);
	
	BurnYM2203Init(1, 1500000, NULL, 0);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE, 0.35, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.50, BURN_SND_ROUTE_BOTH);
	
	MSM6295Init(0, 1023924 / 132, 1);
	MSM6295SetRoute(0, 0.55, BURN_SND_ROUTE_BOTH);
	
	return 0;
}

static INT32 BaddudesInit()
{
	INT32 nRet = 0;

	Dec0MachineInit();

	nRet = BurnLoadRom(Drv68KRom + 0x00001, 0, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x00000, 1, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x40001, 2, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x40000, 3, 2); if (nRet != 0) return 1;
	
	nRet = BurnLoadRom(DrvM6502Rom, 4, 1); if (nRet != 0) return 1;
	
	nRet = BurnLoadRom(DrvTempRom + 0x00000,  5, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x08000,  6, 1); if (nRet != 0) return 1;
	GfxDecode(0x800, 4, 8, 8, CharPlaneOffsets, CharXOffsets, CharYOffsets, 0x40, DrvTempRom, DrvChars);
	
	memset(DrvTempRom, 0, 0x80000);
	nRet = BurnLoadRom(DrvTempRom + 0x00000,  7, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x10000,  8, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x20000,  9, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x30000, 10, 1); if (nRet != 0) return 1;
	GfxDecode(0x800, 4, 16, 16, Tile1PlaneOffsets, TileXOffsets, TileYOffsets, 0x100, DrvTempRom, DrvTiles1);
	
	memset(DrvTempRom, 0, 0x80000);
	nRet = BurnLoadRom(DrvTempRom + 0x20000, 11, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x30000, 12, 1); if (nRet != 0) return 1;
	memcpy(DrvTempRom + 0x08000, DrvTempRom + 0x20000, 0x8000);
	memcpy(DrvTempRom + 0x00000, DrvTempRom + 0x28000, 0x8000);
	memcpy(DrvTempRom + 0x18000, DrvTempRom + 0x30000, 0x8000);
	memcpy(DrvTempRom + 0x10000, DrvTempRom + 0x38000, 0x8000);	
	GfxDecode(0x400, 4, 16, 16, Tile2PlaneOffsets, TileXOffsets, TileYOffsets, 0x100, DrvTempRom, DrvTiles2);
	
	memset(DrvTempRom, 0, 0x80000);
	nRet = BurnLoadRom(DrvTempRom + 0x00000, 13, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x10000, 14, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x20000, 15, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x30000, 16, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x40000, 17, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x50000, 18, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x60000, 19, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x70000, 20, 1); if (nRet != 0) return 1;
	GfxDecode(0x1000, 4, 16, 16, SpritePlaneOffsets, TileXOffsets, TileYOffsets, 0x100, DrvTempRom, DrvSprites);
	
	nRet = BurnLoadRom(MSM6295ROM + 0x00000, 21, 1); if (nRet != 0) return 1;
	
	BurnFree(DrvTempRom);
	
	Dec0Game = DEC0_GAME_BADDUDES;

	BaddudesDoReset();

	return 0;
}

static INT32 BirdtryInit()
{
	INT32 nRet = 0;

	Dec0MachineInit();

	nRet = BurnLoadRom(Drv68KRom + 0x00001, 0, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x00000, 1, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x20001, 2, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x20000, 3, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x40001, 4, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x40000, 5, 2); if (nRet != 0) return 1;
	
	nRet = BurnLoadRom(DrvM6502Rom, 6, 1); if (nRet != 0) return 1;
	
	nRet = BurnLoadRom(DrvTempRom + 0x00000,  8, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x08000,  9, 1); if (nRet != 0) return 1;
	GfxDecode(0x800, 4, 8, 8, CharPlaneOffsets, CharXOffsets, CharYOffsets, 0x40, DrvTempRom, DrvChars);
	
	memset(DrvTempRom, 0, 0x80000);
	nRet = BurnLoadRom(DrvTempRom + 0x00000, 10, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x10000, 11, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x20000, 12, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x30000, 13, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x40000, 14, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x50000, 15, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x60000, 16, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x70000, 17, 1); if (nRet != 0) return 1;
	GfxDecode(0x1000, 4, 16, 16, SpritePlaneOffsets, TileXOffsets, TileYOffsets, 0x100, DrvTempRom, DrvTiles1);
	
	memset(DrvTempRom, 0, 0x80000);
	nRet = BurnLoadRom(DrvTempRom + 0x00000, 18, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x10000, 19, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x20000, 20, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x30000, 21, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x40000, 22, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x50000, 23, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x60000, 24, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x70000, 25, 1); if (nRet != 0) return 1;
	GfxDecode(0x1000, 4, 16, 16, SpritePlaneOffsets, TileXOffsets, TileYOffsets, 0x100, DrvTempRom, DrvSprites);
	
	nRet = BurnLoadRom(MSM6295ROM + 0x00000, 26, 1); if (nRet != 0) return 1;
	
	BurnFree(DrvTempRom);
	
	realMCU = 1;
	nRet = BurnLoadRom(DrvMCURom + 0x00000, 7, 1); if (nRet != 0) return 1;
	DrvMCUInit();

	BaddudesDoReset();

	return 0;

}

static INT32 Drgninjab2Init()
{
	INT32 nRet = 0;

	Dec0MachineInit();

	nRet = BurnLoadRom(Drv68KRom + 0x00001, 0, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x00000, 1, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x40001, 2, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x40000, 3, 2); if (nRet != 0) return 1;
	
	nRet = BurnLoadRom(DrvM6502Rom, 4, 1); if (nRet != 0) return 1;
	
	nRet = BurnLoadRom(DrvTempRom + 0x00000,  5, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x08000,  6, 1); if (nRet != 0) return 1;
	GfxDecode(0x800, 4, 8, 8, CharPlaneOffsets, CharXOffsets, CharYOffsets, 0x40, DrvTempRom, DrvChars);
	
	memset(DrvTempRom, 0, 0x80000);
	nRet = BurnLoadRom(DrvTempRom + 0x00000,  7, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x10000,  8, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x20000,  9, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x30000, 10, 1); if (nRet != 0) return 1;
	GfxDecode(0x800, 4, 16, 16, Tile1PlaneOffsets, TileXOffsets, TileYOffsets, 0x100, DrvTempRom, DrvTiles1);
	
	memset(DrvTempRom, 0, 0x80000);
	nRet = BurnLoadRom(DrvTempRom + 0x08000, 11, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x00000, 12, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x18000, 13, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x10000, 14, 1); if (nRet != 0) return 1;
	GfxDecode(0x400, 4, 16, 16, Tile2PlaneOffsets, TileXOffsets, TileYOffsets, 0x100, DrvTempRom, DrvTiles2);
	
	memset(DrvTempRom, 0, 0x80000);
	nRet = BurnLoadRom(DrvTempRom + 0x00000, 15, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x10000, 16, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x20000, 17, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x30000, 18, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x40000, 19, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x50000, 20, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x60000, 21, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x70000, 22, 1); if (nRet != 0) return 1;
	GfxDecode(0x1000, 4, 16, 16, SpritePlaneOffsets, TileXOffsets, TileYOffsets, 0x100, DrvTempRom, DrvSprites);
	
	nRet = BurnLoadRom(MSM6295ROM + 0x00000, 23, 1); if (nRet != 0) return 1;
	
	BurnFree(DrvTempRom);
	
	Dec0Game = DEC0_GAME_BADDUDES;

	BaddudesDoReset();

	return 0;
}

static INT32 HbarrelInit()
{
	INT32 nRet = 0;

	Dec0MachineInit();

	nRet = BurnLoadRom(Drv68KRom + 0x00001, 0, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x00000, 1, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x20001, 2, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x20000, 3, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x40001, 4, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x40000, 5, 2); if (nRet != 0) return 1;
	
	nRet = BurnLoadRom(DrvM6502Rom, 6, 1); if (nRet != 0) return 1;
	
	nRet = BurnLoadRom(DrvTempRom + 0x00000,  7, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x10000,  8, 1); if (nRet != 0) return 1;
	GfxDecode(0x1000, 4, 8, 8, RobocopCharPlaneOffsets, CharXOffsets, CharYOffsets, 0x40, DrvTempRom, DrvChars);
	
	memset(DrvTempRom, 0, 0x80000);
	nRet = BurnLoadRom(DrvTempRom + 0x00000,  9, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x10000, 10, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x20000, 11, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x30000, 12, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x40000, 13, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x50000, 14, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x60000, 15, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x70000, 16, 1); if (nRet != 0) return 1;
	GfxDecode(0x1000, 4, 16, 16, SpritePlaneOffsets, TileXOffsets, TileYOffsets, 0x100, DrvTempRom, DrvTiles1);
	
	memset(DrvTempRom, 0, 0x80000);
	nRet = BurnLoadRom(DrvTempRom + 0x00000, 17, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x10000, 18, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x20000, 19, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x30000, 20, 1); if (nRet != 0) return 1;
	GfxDecode(0x800, 4, 16, 16, Tile1PlaneOffsets, TileXOffsets, TileYOffsets, 0x100, DrvTempRom, DrvTiles2);
	
	memset(DrvTempRom, 0, 0x80000);
	nRet = BurnLoadRom(DrvTempRom + 0x00000, 21, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x10000, 22, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x20000, 23, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x30000, 24, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x40000, 25, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x50000, 26, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x60000, 27, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x70000, 28, 1); if (nRet != 0) return 1;
	GfxDecode(0x1000, 4, 16, 16, SpritePlaneOffsets, TileXOffsets, TileYOffsets, 0x100, DrvTempRom, DrvSprites);
	
	nRet = BurnLoadRom(MSM6295ROM + 0x00000, 29, 1); if (nRet != 0) return 1;

	realMCU = 1;
	nRet = BurnLoadRom(DrvMCURom + 0x00000, 30, 1); if (nRet != 0) return 1;
	DrvMCUInit();

	BurnFree(DrvTempRom);

	RotateSetGunPosRAM(Drv68KRam + (0x66+1), Drv68KRam + (0xaa+1), 4);
	game_rotates = 1;

	BaddudesDoReset();

	return 0;
}

static INT32 HippodrmInit()
{
	INT32 nRet = 0;

	Dec0MachineInit();
	
	nRet = BurnLoadRom(Drv68KRom + 0x00001, 0, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x00000, 1, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x20001, 2, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x20000, 3, 2); if (nRet != 0) return 1;
	
	nRet = BurnLoadRom(DrvM6502Rom, 4, 1); if (nRet != 0) return 1;
	
	nRet = BurnLoadRom(DrvH6280Rom, 5, 1); if (nRet != 0) return 1;
	
	nRet = BurnLoadRom(DrvTempRom + 0x00000,  6, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x10000,  7, 1); if (nRet != 0) return 1;
	GfxDecode(0x1000, 4, 8, 8, RobocopCharPlaneOffsets, CharXOffsets, CharYOffsets, 0x40, DrvTempRom, DrvChars);
	
	memset(DrvTempRom, 0, 0x80000);
	nRet = BurnLoadRom(DrvTempRom + 0x00000,  8, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x08000,  9, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x10000, 10, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x18000, 11, 1); if (nRet != 0) return 1;
	GfxDecode(0x400, 4, 16, 16, Tile2PlaneOffsets, TileXOffsets, TileYOffsets, 0x100, DrvTempRom, DrvTiles1);
	
	memset(DrvTempRom, 0, 0x80000);
	nRet = BurnLoadRom(DrvTempRom + 0x00000, 12, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x08000, 13, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x10000, 14, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x18000, 15, 1); if (nRet != 0) return 1;
	GfxDecode(0x400, 4, 16, 16, Tile2PlaneOffsets, TileXOffsets, TileYOffsets, 0x100, DrvTempRom, DrvTiles2);
	
	memset(DrvTempRom, 0, 0x80000);
	nRet = BurnLoadRom(DrvTempRom + 0x00000, 16, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x10000, 17, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x20000, 18, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x30000, 19, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x40000, 20, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x50000, 21, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x60000, 22, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x70000, 23, 1); if (nRet != 0) return 1;
	GfxDecode(0x1000, 4, 16, 16, SpritePlaneOffsets, TileXOffsets, TileYOffsets, 0x100, DrvTempRom, DrvSprites);
	
	nRet = BurnLoadRom(MSM6295ROM + 0x00000, 24, 1); if (nRet != 0) return 1;

	BurnFree(DrvTempRom);
	
	for (INT32 i = 0x00000; i < 0x10000; i++) {
		DrvH6280Rom[i] = (DrvH6280Rom[i] & 0x7e) | ((DrvH6280Rom[i] & 0x1) << 7) | ((DrvH6280Rom[i] & 0x80) >> 7);
	}
	DrvH6280Rom[0x189] = 0x60;
	DrvH6280Rom[0x1af] = 0x60;
	DrvH6280Rom[0x1db] = 0x60;
	DrvH6280Rom[0x21a] = 0x60;
	
	SekOpen(0);
	SekMapHandler(1, 0x180000, 0x180fff, MAP_RAM);
	SekSetReadByteHandler(1, HippodrmShared68KReadByte);
	SekSetWriteByteHandler(1, HippodrmShared68KWriteByte);
	SekSetReadWordHandler(1, HippodrmShared68KReadWord);
	SekSetWriteWordHandler(1, HippodrmShared68KWriteWord);	
	SekClose();
	
	h6280Init(0);
	h6280Open(0);
	h6280MapMemory(DrvH6280Rom , 0x000000, 0x00ffff, MAP_ROM);
	h6280MapMemory(DrvSharedRam, 0x180000, 0x1800ff, MAP_RAM);
	h6280MapMemory(DrvH6280Ram , 0x1f0000, 0x1f1fff, MAP_RAM);
	h6280SetReadHandler(HippodrmH6280ReadProg);
	h6280SetWriteHandler(HippodrmH6280WriteProg);
	h6280Close();

	RobocopDoReset();

	return 0;
}

static INT32 RobocopInit()
{
	INT32 nRet = 0;

	Dec0MachineInit();
	
	nRet = BurnLoadRom(Drv68KRom + 0x00001, 0, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x00000, 1, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x20001, 2, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x20000, 3, 2); if (nRet != 0) return 1;
	
	nRet = BurnLoadRom(DrvM6502Rom, 4, 1); if (nRet != 0) return 1;
	
	nRet = BurnLoadRom(DrvH6280Rom + 0x01e00, 5, 1); if (nRet != 0) return 1;
	
	nRet = BurnLoadRom(DrvTempRom + 0x00000,  6, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x10000,  7, 1); if (nRet != 0) return 1;
	GfxDecode(0x1000, 4, 8, 8, RobocopCharPlaneOffsets, CharXOffsets, CharYOffsets, 0x40, DrvTempRom, DrvChars);
	
	memset(DrvTempRom, 0, 0x80000);
	nRet = BurnLoadRom(DrvTempRom + 0x00000,  8, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x10000,  9, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x20000, 10, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x30000, 11, 1); if (nRet != 0) return 1;
	GfxDecode(0x800, 4, 16, 16, Tile1PlaneOffsets, TileXOffsets, TileYOffsets, 0x100, DrvTempRom, DrvTiles1);
	
	memset(DrvTempRom, 0, 0x80000);
	nRet = BurnLoadRom(DrvTempRom + 0x00000, 12, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x08000, 13, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x10000, 14, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x18000, 15, 1); if (nRet != 0) return 1;
	GfxDecode(0x400, 4, 16, 16, Tile2PlaneOffsets, TileXOffsets, TileYOffsets, 0x100, DrvTempRom, DrvTiles2);
	
	memset(DrvTempRom, 0, 0x80000);
	nRet = BurnLoadRom(DrvTempRom + 0x00000, 16, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x10000, 17, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x20000, 18, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x30000, 19, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x40000, 20, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x50000, 21, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x60000, 22, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x70000, 23, 1); if (nRet != 0) return 1;
	GfxDecode(0x1000, 4, 16, 16, SpritePlaneOffsets, TileXOffsets, TileYOffsets, 0x100, DrvTempRom, DrvSprites);
	
	nRet = BurnLoadRom(MSM6295ROM + 0x00000, 24, 1); if (nRet != 0) return 1;

	BurnFree(DrvTempRom);
	
	SekOpen(0);
	SekMapHandler(1, 0x180000, 0x180fff, MAP_RAM);
	SekSetReadByteHandler(1, RobocopShared68KReadByte);
	SekSetWriteByteHandler(1, RobocopShared68KWriteByte);
	SekSetReadWordHandler(1, RobocopShared68KReadWord);
	SekSetWriteWordHandler(1, RobocopShared68KWriteWord);	
	SekClose();
	
	h6280Init(0);
	h6280Open(0);
	h6280MapMemory(DrvH6280Rom , 0x000000, 0x00ffff, MAP_ROM);
	h6280MapMemory(DrvH6280Ram , 0x1f0000, 0x1f1fff, MAP_RAM);
	h6280MapMemory(DrvSharedRam, 0x1f2000, 0x1f3fff, MAP_RAM);
	h6280SetReadHandler(RobocopH6280ReadProg);
	h6280SetWriteHandler(RobocopH6280WriteProg);
	h6280Close();

	RobocopDoReset();

	return 0;
}

static INT32 RobocopbInit()
{
	INT32 nRet = 0;

	Dec0MachineInit();

	nRet = BurnLoadRom(Drv68KRom + 0x00001, 0, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x00000, 1, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x20001, 2, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x20000, 3, 2); if (nRet != 0) return 1;
	
	nRet = BurnLoadRom(DrvM6502Rom, 4, 1); if (nRet != 0) return 1;
	
	nRet = BurnLoadRom(DrvTempRom + 0x00000,  5, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x10000,  6, 1); if (nRet != 0) return 1;
	GfxDecode(0x1000, 4, 8, 8, RobocopCharPlaneOffsets, CharXOffsets, CharYOffsets, 0x40, DrvTempRom, DrvChars);
	
	memset(DrvTempRom, 0, 0x80000);
	nRet = BurnLoadRom(DrvTempRom + 0x00000,  7, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x10000,  8, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x20000,  9, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x30000, 10, 1); if (nRet != 0) return 1;
	GfxDecode(0x800, 4, 16, 16, Tile1PlaneOffsets, TileXOffsets, TileYOffsets, 0x100, DrvTempRom, DrvTiles1);
	
	memset(DrvTempRom, 0, 0x80000);
	nRet = BurnLoadRom(DrvTempRom + 0x00000, 11, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x08000, 12, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x10000, 13, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x18000, 14, 1); if (nRet != 0) return 1;
	GfxDecode(0x400, 4, 16, 16, Tile2PlaneOffsets, TileXOffsets, TileYOffsets, 0x100, DrvTempRom, DrvTiles2);
	
	memset(DrvTempRom, 0, 0x80000);
	nRet = BurnLoadRom(DrvTempRom + 0x00000, 15, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x10000, 16, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x20000, 17, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x30000, 18, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x40000, 19, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x50000, 20, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x60000, 21, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x70000, 22, 1); if (nRet != 0) return 1;
	GfxDecode(0x1000, 4, 16, 16, SpritePlaneOffsets, TileXOffsets, TileYOffsets, 0x100, DrvTempRom, DrvSprites);
	
	nRet = BurnLoadRom(MSM6295ROM + 0x00000, 23, 1); if (nRet != 0) return 1;
	
	BurnFree(DrvTempRom);
	
	BaddudesDoReset();

	return 0;
}

static INT32 SlyspyDrvInit()
{
	INT32 nLen;
	
	BurnSetRefreshRate(57.41);

	Mem = NULL;
	MemIndex();
	nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(Mem, 0, nLen);
	MemIndex();

	if (LoadRomsFunction()) return 1;
	
	for (INT32 i = 0x00000; i < 0x10000; i++) {
		DrvH6280Rom[i] = (DrvH6280Rom[i] & 0x7e) | ((DrvH6280Rom[i] & 0x1) << 7) | ((DrvH6280Rom[i] & 0x80) >> 7);
	}
	
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KRom               , 0x000000, 0x05ffff, MAP_ROM);
	SekMapMemory(DrvVideo2ColScrollRam   , 0x300800, 0x30087f, MAP_RAM);
	SekMapMemory(DrvVideo2RowScrollRam   , 0x300c00, 0x300fff, MAP_RAM);
	SekMapMemory(DrvVideo2Ram            , 0x301000, 0x3017ff, MAP_RAM);
	SekMapMemory(Drv68KRam               , 0x304000, 0x307fff, MAP_RAM);
	SekMapMemory(DrvSpriteRam            , 0x308000, 0x3087ff, MAP_RAM);
	SekMapMemory(DrvPaletteRam           , 0x310000, 0x3107ff, MAP_RAM);
	SekSetReadByteHandler(0, Slyspy68KReadByte);
	SekSetWriteByteHandler(0, Slyspy68KWriteByte);
	SekSetReadWordHandler(0, Slyspy68KReadWord);
	SekSetWriteWordHandler(0, Slyspy68KWriteWord);	
	SekClose();
	
	h6280Init(0);
	h6280Open(0);
	h6280MapMemory(DrvH6280Rom , 0x000000, 0x00ffff, MAP_ROM);
	h6280MapMemory(DrvH6280Ram , 0x1f0000, 0x1f1fff, MAP_RAM);
	h6280SetReadHandler(SlyspyH6280ReadProg);
	h6280SetWriteHandler(SlyspyH6280WriteProg);
	h6280Close();
	
	GenericTilesInit();
	
	BurnYM3812Init(1, 3000000, &Dec1YM3812IRQHandler, 1);
	BurnTimerAttach(&H6280Config, 3000000);
	BurnYM3812SetRoute(0, BURN_SND_YM3812_ROUTE, 0.80, BURN_SND_ROUTE_BOTH);

	BurnYM2203Init(1, 1500000, NULL, 0);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE, 0.35, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.90, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.90, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.90, BURN_SND_ROUTE_BOTH);
	
	MSM6295Init(0, 1000000 / 132, 1);
	MSM6295SetRoute(0, 0.80, BURN_SND_ROUTE_BOTH);
	
	DrvSpriteDMABufferRam = DrvSpriteRam;

	slyspy_mode = 1;

	SlyspyDoReset();

	return 0;
}

static INT32 SlyspyLoadRoms()
{
	INT32 nRet;
	
	DrvTempRom = (UINT8 *)BurnMalloc(0x80000);
	
	nRet = BurnLoadRom(Drv68KRom + 0x00001, 0, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x00000, 1, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x20001, 2, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x20000, 3, 2); if (nRet != 0) return 1;
	
	nRet = BurnLoadRom(DrvH6280Rom, 4, 1); if (nRet != 0) return 1;
	
	nRet = BurnLoadRom(DrvTempRom + 0x10000,  5, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x18000,  6, 1); if (nRet != 0) return 1;
	memcpy(DrvTempRom + 0x4000, DrvTempRom + 0x10000, 0x4000);
	memcpy(DrvTempRom + 0x0000, DrvTempRom + 0x14000, 0x4000);
	memcpy(DrvTempRom + 0xc000, DrvTempRom + 0x18000, 0x4000);
	memcpy(DrvTempRom + 0x8000, DrvTempRom + 0x1c000, 0x4000);
	GfxDecode(0x800, 4, 8, 8, CharPlaneOffsets, CharXOffsets, CharYOffsets, 0x40, DrvTempRom, DrvChars);
	
	memset(DrvTempRom, 0, 0x80000);
	nRet = BurnLoadRom(DrvTempRom + 0x00000,  7, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x10000,  8, 1); if (nRet != 0) return 1;
	GfxDecode(0x400, 4, 16, 16, Tile2PlaneOffsets, TileXOffsets, TileYOffsets, 0x100, DrvTempRom, DrvTiles1);
	
	memset(DrvTempRom, 0, 0x80000);
	nRet = BurnLoadRom(DrvTempRom + 0x00000,  9, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x20000, 10, 1); if (nRet != 0) return 1;
	GfxDecode(0x800, 4, 16, 16, Tile1PlaneOffsets, TileXOffsets, TileYOffsets, 0x100, DrvTempRom, DrvTiles2);
	
	memset(DrvTempRom, 0, 0x80000);
	nRet = BurnLoadRom(DrvTempRom + 0x00000, 11, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x20000, 12, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x40000, 13, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x60000, 14, 1); if (nRet != 0) return 1;
	GfxDecode(0x1000, 4, 16, 16, SpritePlaneOffsets, TileXOffsets, TileYOffsets, 0x100, DrvTempRom, DrvSprites);
	
	nRet = BurnLoadRom(MSM6295ROM + 0x00000, 15, 1); if (nRet != 0) return 1;
	
	BurnFree(DrvTempRom);
	
	return 0;
}

static INT32 SlyspyInit()
{
	LoadRomsFunction = SlyspyLoadRoms;
	
	return SlyspyDrvInit();
}

static INT32 BouldashLoadRoms()
{
	INT32 nRet;
	
	DrvTempRom = (UINT8 *)BurnMalloc(0x40000);
	
	nRet = BurnLoadRom(Drv68KRom + 0x00001, 0, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x00000, 1, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x20001, 2, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x20000, 3, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x40001, 4, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x40000, 5, 2); if (nRet != 0) return 1;
	
	nRet = BurnLoadRom(DrvH6280Rom, 6, 1); if (nRet != 0) return 1;
	
	nRet = BurnLoadRom(DrvTempRom + 0x20000,  7, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x30000,  8, 1); if (nRet != 0) return 1;
	memcpy(DrvTempRom + 0x08000, DrvTempRom + 0x20000, 0x8000);
	memcpy(DrvTempRom + 0x00000, DrvTempRom + 0x28000, 0x8000);
	memcpy(DrvTempRom + 0x18000, DrvTempRom + 0x30000, 0x8000);
	memcpy(DrvTempRom + 0x10000, DrvTempRom + 0x38000, 0x8000);
	GfxDecode(0x1000, 4, 8, 8, RobocopCharPlaneOffsets, CharXOffsets, CharYOffsets, 0x40, DrvTempRom, DrvChars);
	
	memset(DrvTempRom, 0, 0x40000);
	nRet = BurnLoadRom(DrvTempRom + 0x00000,  9, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x10000, 10, 1); if (nRet != 0) return 1;
	GfxDecode(0x400, 4, 16, 16, Tile2PlaneOffsets, TileXOffsets, TileYOffsets, 0x100, DrvTempRom, DrvTiles1);
	
	memset(DrvTempRom, 0, 0x40000);
	nRet = BurnLoadRom(DrvTempRom + 0x00000, 11, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x20000, 12, 1); if (nRet != 0) return 1;
	GfxDecode(0x800, 4, 16, 16, Tile1PlaneOffsets, TileXOffsets, TileYOffsets, 0x100, DrvTempRom, DrvTiles2);
	
	memset(DrvTempRom, 0, 0x40000);
	nRet = BurnLoadRom(DrvTempRom + 0x00000, 13, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x10000, 14, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x20000, 15, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x30000, 16, 1); if (nRet != 0) return 1;
	GfxDecode(0x0800, 4, 16, 16, Tile1PlaneOffsets, TileXOffsets, TileYOffsets, 0x100, DrvTempRom, DrvSprites);
	
	nRet = BurnLoadRom(MSM6295ROM + 0x00000, 17, 1); if (nRet != 0) return 1;
	
	BurnFree(DrvTempRom);
	
	return 0;
}

static INT32 BouldashInit()
{
	LoadRomsFunction = BouldashLoadRoms;
	
	return SlyspyDrvInit();
}

static INT32 MidresInit()
{
	INT32 nRet = 0, nLen;
	
	BurnSetRefreshRate(57.44);

	Mem = NULL;
	MemIndex();
	nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(Mem, 0, nLen);
	MemIndex();

	DrvTempRom = (UINT8 *)BurnMalloc(0x80000);
	
	nRet = BurnLoadRom(Drv68KRom + 0x00001, 0, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x00000, 1, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x40001, 2, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x40000, 3, 2); if (nRet != 0) return 1;
	
	nRet = BurnLoadRom(DrvH6280Rom, 4, 1); if (nRet != 0) return 1;
	
	nRet = BurnLoadRom(DrvTempRom + 0x20000,  5, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x30000,  6, 1); if (nRet != 0) return 1;
	memcpy(DrvTempRom + 0x08000, DrvTempRom + 0x20000, 0x8000);
	memcpy(DrvTempRom + 0x00000, DrvTempRom + 0x28000, 0x8000);
	memcpy(DrvTempRom + 0x18000, DrvTempRom + 0x30000, 0x8000);
	memcpy(DrvTempRom + 0x10000, DrvTempRom + 0x38000, 0x8000);
	GfxDecode(0x1000, 4, 8, 8, RobocopCharPlaneOffsets, CharXOffsets, CharYOffsets, 0x40, DrvTempRom, DrvChars);
	
	memset(DrvTempRom, 0, 0x80000);
	nRet = BurnLoadRom(DrvTempRom + 0x00000,  7, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x20000,  8, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x40000,  9, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x60000, 10, 1); if (nRet != 0) return 1;
	GfxDecode(0x1000, 4, 16, 16, SpritePlaneOffsets, TileXOffsets, TileYOffsets, 0x100, DrvTempRom, DrvTiles1);
	
	memset(DrvTempRom, 0, 0x80000);
	nRet = BurnLoadRom(DrvTempRom + 0x00000, 11, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x20000, 12, 1); if (nRet != 0) return 1;
	GfxDecode(0x800, 4, 16, 16, Tile1PlaneOffsets, TileXOffsets, TileYOffsets, 0x100, DrvTempRom, DrvTiles2);
	
	memset(DrvTempRom, 0, 0x80000);
	nRet = BurnLoadRom(DrvTempRom + 0x00000, 13, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x20000, 14, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x40000, 15, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x60000, 16, 1); if (nRet != 0) return 1;
	GfxDecode(0x1000, 4, 16, 16, SpritePlaneOffsets, TileXOffsets, TileYOffsets, 0x100, DrvTempRom, DrvSprites);
	
	nRet = BurnLoadRom(MSM6295ROM + 0x00000, 17, 1); if (nRet != 0) return 1;
	
	BurnFree(DrvTempRom);
	
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KRom               , 0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(Drv68KRam               , 0x100000, 0x103fff, MAP_RAM);
	SekMapMemory(DrvSpriteRam            , 0x120000, 0x1207ff, MAP_RAM);
	SekMapMemory(DrvPaletteRam           , 0x140000, 0x1407ff, MAP_RAM);
	SekMapMemory(DrvVideo1ColScrollRam   , 0x240000, 0x2400ff, MAP_RAM);
	SekMapMemory(DrvVideo1RowScrollRam   , 0x240400, 0x2407ff, MAP_RAM);
	SekMapMemory(DrvVideo2ColScrollRam   , 0x2c0000, 0x2c00ff, MAP_RAM);
	SekMapMemory(DrvVideo2RowScrollRam   , 0x2c0400, 0x2c07ff, MAP_RAM);
	SekMapMemory(DrvCharColScrollRam     , 0x340000, 0x3400ff, MAP_RAM);
	SekMapMemory(DrvCharRowScrollRam     , 0x340400, 0x3407ff, MAP_RAM);
	//  moved from Midres68KRead/WriteByte/Word
	SekMapMemory(DrvVideo1Ram            , 0x220000, 0x2207ff, MAP_RAM);
	SekMapMemory(DrvVideo1Ram            , 0x220800, 0x220fff, MAP_RAM); // mirror
	SekMapMemory(DrvVideo2Ram            , 0x2a0000, 0x2a07ff, MAP_RAM);
	SekMapMemory(DrvCharRam              , 0x320000, 0x321fff, MAP_RAM);
	//
	SekSetReadByteHandler(0, Midres68KReadByte);
	SekSetWriteByteHandler(0, Midres68KWriteByte);
	SekSetReadWordHandler(0, Midres68KReadWord);
	SekSetWriteWordHandler(0, Midres68KWriteWord);
	SekClose();

	h6280Init(0);
	h6280Open(0);
	h6280MapMemory(DrvH6280Rom , 0x000000, 0x00ffff, MAP_ROM);
	h6280MapMemory(DrvH6280Ram , 0x1f0000, 0x1f1fff, MAP_RAM);
	h6280SetReadHandler(MidresH6280ReadProg);
	h6280SetWriteHandler(MidresH6280WriteProg);
	h6280Close();
	
	GenericTilesInit();
	
	BurnYM3812Init(1, 3000000, &Dec1YM3812IRQHandler, 1);
	BurnTimerAttach(&H6280Config, 2000000);
	BurnYM3812SetRoute(0, BURN_SND_YM3812_ROUTE, 0.70, BURN_SND_ROUTE_BOTH);
	
	BurnYM2203Init(1, 1500000, NULL, 0);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE, 0.75, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 1.75, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 1.75, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 1.75, BURN_SND_ROUTE_BOTH);

	MSM6295Init(0, 1000000 / 132, 1);
	MSM6295SetRoute(0, 1.80, BURN_SND_ROUTE_BOTH);
	
	DrvSpriteDMABufferRam = DrvSpriteRam;
	
	DrvCharPalOffset = 256;
	DrvSpritePalOffset = 0;

	RotateSetGunPosRAM(Drv68KRam + (0x21bc+1), Drv68KRam + (0x2238+1), 4);
	game_rotates = 1;

	Dec0Game = DEC1_GAME_MIDRES;

	SlyspyDoReset();

	return 0;
}

static INT32 DrvExit()
{
	SekExit();
	
	BurnYM2203Exit();
	BurnYM3812Exit();
	MSM6295Exit(0);

	if (realMCU)
		DrvMCUExit();

	GenericTilesExit();
	
	i8751RetVal = 0;
	DrvVBlank = 0;
	DrvSoundLatch = 0;
	DrvFlipScreen = 0;
	DrvPriority = 0;
	
	DrvCharTilemapWidth = 0;
	DrvCharTilemapHeight = 0;
	DrvTile1TilemapWidth = 0;
	DrvTile1TilemapHeight = 0;
	DrvTile2TilemapWidth = 0;
	DrvTile2TilemapHeight = 0;
	memset(DrvTileRamBank, 0, 3);
	DrvSlyspyProtValue = 0;
	
	LoadRomsFunction = NULL;

	slyspy_mode = 0;

	Dec0Game = 0;
	
	DrvCharPalOffset = 0;
	DrvSpritePalOffset = 256;
	
	game_rotates = 0;
	realMCU = 0;

	bTimerNullCPU = 0;

	BurnFree(Mem);

	return 0;
}

static INT32 BaddudesExit()
{
	M6502Exit();
	return DrvExit();
}

static INT32 RobocopExit()
{
	h6280Exit();
	return BaddudesExit();
}

static INT32 SlyspyExit()
{
	h6280Exit();
	return DrvExit();
}

static inline UINT8 pal4bit(UINT8 bits)
{
	bits &= 0x0f;
	return (bits << 4) | bits;
}

static void DrvCalcPalette()
{
	UINT16 *PaletteRam = (UINT16*)DrvPaletteRam;
	UINT16 *Palette2Ram = (UINT16*)DrvPalette2Ram;
	
	INT32 r, g, b;
	
	for (INT32 i = 0; i < 0x400; i++) {
		r = (BURN_ENDIAN_SWAP_INT16(PaletteRam[i]) >> 0) & 0xff;
		g = (BURN_ENDIAN_SWAP_INT16(PaletteRam[i]) >> 8) & 0xff;
		b = (BURN_ENDIAN_SWAP_INT16(Palette2Ram[i]) >> 0) & 0xff;
		
		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}
}

static void Dec1CalcPalette()
{
	UINT16 *PaletteRam = (UINT16*)DrvPaletteRam;
	
	INT32 r, g, b;
	
	for (INT32 i = 0; i < 0x400; i++) {
		r = pal4bit(BURN_ENDIAN_SWAP_INT16(PaletteRam[i]) >> 0);
		g = pal4bit(BURN_ENDIAN_SWAP_INT16(PaletteRam[i]) >> 4);
		b = pal4bit(BURN_ENDIAN_SWAP_INT16(PaletteRam[i]) >> 8);
		
		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}
}

#define PLOTPIXEL(x, po) pPixel[x] = nPalette | pTileData[x] | po;
#define PLOTPIXEL_FLIPX(x, a, po) pPixel[x] = nPalette | pTileData[a] | po;
#define PLOTPIXEL_MASK(x, mc, po) if (pTileData[x] != mc) {pPixel[x] = nPalette | pTileData[x] | po;}
#define PLOTPIXEL_MASK_FLIPX(x, a, mc, po) if (pTileData[a] != mc) {pPixel[x] = nPalette | pTileData[a] | po;}
#define CLIPPIXEL(x, sx, mx, a) if ((sx + x) >= 0 && (sx + x) < mx) { a; };

static void Dec0Render8x8Tile_Mask(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile, INT32 TilemapWidth)
{
	UINT32 nPalette = nTilePalette << nColourDepth;
	pTileData = pTile + (nTileNumber << 6);
	
	UINT16* pPixel = pDestDraw + (StartY * TilemapWidth) + StartX;

	for (INT32 y = 0; y < 8; y++, pPixel += TilemapWidth, pTileData += 8) {
		PLOTPIXEL_MASK( 0, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK( 1, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK( 2, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK( 3, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK( 4, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK( 5, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK( 6, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK( 7, nMaskColour, nPaletteOffset);
	}
}

static void Dec0Render8x8Tile_Mask_FlipXY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile, INT32 TilemapWidth)
{
	UINT32 nPalette = nTilePalette << nColourDepth;
	pTileData = pTile + (nTileNumber << 6);
	
	UINT16* pPixel = pDestDraw + ((StartY + 7) * TilemapWidth) + StartX;

	for (INT32 y = 7; y >= 0; y--, pPixel -= TilemapWidth, pTileData += 8) {
		PLOTPIXEL_MASK_FLIPX(7, 0, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK_FLIPX(6, 1, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK_FLIPX(5, 2, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK_FLIPX(4, 3, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK_FLIPX(3, 4, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK_FLIPX(2, 5, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK_FLIPX(1, 6, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK_FLIPX(0, 7, nMaskColour, nPaletteOffset);
	}
}

static void Dec0Render16x16Tile(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile, INT32 TilemapWidth)
{
	UINT32 nPalette = nTilePalette << nColourDepth;
	pTileData = pTile + (nTileNumber << 8);

	UINT16* pPixel = pDestDraw + (StartY * TilemapWidth) + StartX;

	for (INT32 y = 0; y < 16; y++, pPixel += TilemapWidth, pTileData += 16) {
		PLOTPIXEL( 0, nPaletteOffset);
		PLOTPIXEL( 1, nPaletteOffset);
		PLOTPIXEL( 2, nPaletteOffset);
		PLOTPIXEL( 3, nPaletteOffset);
		PLOTPIXEL( 4, nPaletteOffset);
		PLOTPIXEL( 5, nPaletteOffset);
		PLOTPIXEL( 6, nPaletteOffset);
		PLOTPIXEL( 7, nPaletteOffset);
		PLOTPIXEL( 8, nPaletteOffset);
		PLOTPIXEL( 9, nPaletteOffset);
		PLOTPIXEL(10, nPaletteOffset);
		PLOTPIXEL(11, nPaletteOffset);
		PLOTPIXEL(12, nPaletteOffset);
		PLOTPIXEL(13, nPaletteOffset);
		PLOTPIXEL(14, nPaletteOffset);
		PLOTPIXEL(15, nPaletteOffset);
	}
}

static void Dec0Render16x16Tile_FlipXY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile, INT32 TilemapWidth)
{
	UINT32 nPalette = nTilePalette << nColourDepth;
	pTileData = pTile + (nTileNumber << 8);

	UINT16* pPixel = pDestDraw + ((StartY + 15) * TilemapWidth) + StartX;

	for (INT32 y = 15; y >= 0; y--, pPixel -= TilemapWidth, pTileData += 16) {
		PLOTPIXEL_FLIPX(15,  0, nPaletteOffset);
		PLOTPIXEL_FLIPX(14,  1, nPaletteOffset);
		PLOTPIXEL_FLIPX(13,  2, nPaletteOffset);
		PLOTPIXEL_FLIPX(12,  3, nPaletteOffset);
		PLOTPIXEL_FLIPX(11,  4, nPaletteOffset);
		PLOTPIXEL_FLIPX(10,  5, nPaletteOffset);
		PLOTPIXEL_FLIPX( 9,  6, nPaletteOffset);
		PLOTPIXEL_FLIPX( 8,  7, nPaletteOffset);
		PLOTPIXEL_FLIPX( 7,  8, nPaletteOffset);
		PLOTPIXEL_FLIPX( 6,  9, nPaletteOffset);
		PLOTPIXEL_FLIPX( 5, 10, nPaletteOffset);
		PLOTPIXEL_FLIPX( 4, 11, nPaletteOffset);
		PLOTPIXEL_FLIPX( 3, 12, nPaletteOffset);
		PLOTPIXEL_FLIPX( 2, 13, nPaletteOffset);
		PLOTPIXEL_FLIPX( 1, 14, nPaletteOffset);
		PLOTPIXEL_FLIPX( 0, 15, nPaletteOffset);
	}
}

static void Dec0Render16x16Tile_Mask(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile, INT32 TilemapWidth)
{
	UINT32 nPalette = nTilePalette << nColourDepth;
	pTileData = pTile + (nTileNumber << 8);
	
	UINT16* pPixel = pDestDraw + (StartY * TilemapWidth) + StartX;

	for (INT32 y = 0; y < 16; y++, pPixel += TilemapWidth, pTileData += 16) {
		PLOTPIXEL_MASK( 0, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK( 1, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK( 2, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK( 3, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK( 4, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK( 5, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK( 6, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK( 7, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK( 8, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK( 9, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK(10, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK(11, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK(12, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK(13, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK(14, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK(15, nMaskColour, nPaletteOffset);
	}
}

static void Dec0Render16x16Tile_Mask_FlipXY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile, INT32 TilemapWidth)
{
	UINT32 nPalette = nTilePalette << nColourDepth;
	pTileData = pTile + (nTileNumber << 8);
	
	UINT16* pPixel = pDestDraw + ((StartY + 15) * TilemapWidth) + StartX;

	for (INT32 y = 15; y >= 0; y--, pPixel -= TilemapWidth, pTileData += 16) {
		PLOTPIXEL_MASK_FLIPX(15,  0, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK_FLIPX(14,  1, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK_FLIPX(13,  2, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK_FLIPX(12,  3, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK_FLIPX(11,  4, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK_FLIPX(10,  5, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK_FLIPX( 9,  6, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK_FLIPX( 8,  7, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK_FLIPX( 7,  8, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK_FLIPX( 6,  9, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK_FLIPX( 5, 10, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK_FLIPX( 4, 11, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK_FLIPX( 3, 12, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK_FLIPX( 2, 13, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK_FLIPX( 1, 14, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK_FLIPX( 0, 15, nMaskColour, nPaletteOffset);
	}
}

#undef PLOTPIXEL
#undef PLOTPIXEL_FLIPX
#undef PLOTPIXEL_MASK
#undef CLIPPIXEL

#define TILEMAP_BOTH_LAYERS	2
#define TILEMAP_LAYER0		1
#define TILEMAP_LAYER1		0

static void DrvRenderCustomTilemap(UINT16 *pSrc, UINT16 *pControl0, UINT16 *pControl1, UINT16 *RowScrollRam, UINT16 *ColScrollRam, INT32 TilemapWidth, INT32 TilemapHeight, INT32 Opaque, INT32 DrawLayer)
{
	INT32 x, y, xSrc, ySrc, ColOffset = 0, pPixel;
	UINT32 xScroll = BURN_ENDIAN_SWAP_INT16(pControl1[0]);
	UINT32 yScroll = BURN_ENDIAN_SWAP_INT16(pControl1[1]);
	INT32 WidthMask = TilemapWidth - 1;
	INT32 HeightMask = TilemapHeight - 1;
	INT32 RowScrollEnabled = BURN_ENDIAN_SWAP_INT16(pControl0[0]) & 0x04;
	INT32 ColScrollEnabled = BURN_ENDIAN_SWAP_INT16(pControl0[0]) & 0x08;
	
	ySrc = yScroll;
	
	ySrc += 8;
	
	for (y = 0; y < nScreenHeight; y++) {
		if (RowScrollEnabled) {
			xSrc = xScroll + BURN_ENDIAN_SWAP_INT16(RowScrollRam[(ySrc >> (BURN_ENDIAN_SWAP_INT16(pControl1[3]) & 0x0f)) & (0x1ff >> (BURN_ENDIAN_SWAP_INT16(pControl1[3]) & 0x0f))]);
		} else {
			xSrc = xScroll;
		}
		
		xSrc &= WidthMask;
		
		if (DrvFlipScreen) xSrc = -xSrc;
		
		for (x = 0; x < nScreenWidth; x++) {
			if (ColScrollEnabled) ColOffset = BURN_ENDIAN_SWAP_INT16(ColScrollRam[((xSrc >> 3) >> (BURN_ENDIAN_SWAP_INT16(pControl1[2]) & 0x0f)) & (0x3f >> (BURN_ENDIAN_SWAP_INT16(pControl1[2]) & 0x0f))]);
			
			pPixel = pSrc[(((ySrc + ColOffset) & HeightMask) * TilemapWidth) + (xSrc & WidthMask)];
			
			xSrc++;
			
			if (Opaque || (pPixel & 0x0f)) {
				if (DrawLayer == TILEMAP_LAYER0) {
					if ((pPixel & 0x88) == 0x88) pTransDraw[(y * nScreenWidth) + x] = pPixel;
				} else {
					pTransDraw[(y * nScreenWidth) + x] = pPixel;
				}
			}
		}
		
		ySrc++;
	}
}

static void DrvRenderTile1Layer(INT32 Opaque, INT32 DrawLayer)
{
	INT32 mx, my, Code, Attr, Colour, x, y, TileIndex, Layer;
	UINT16 *Control0 = (UINT16*)DrvVideo1Ctrl0Ram;
	UINT16 *VideoRam = (UINT16*)DrvVideo1Ram;
	
	INT32 RenderType = BURN_ENDIAN_SWAP_INT16(Control0[3]) & 0x03;
	
	switch (RenderType) {
		case 0x00: {
			DrvTile1TilemapWidth = 1024;
			DrvTile1TilemapHeight = 256;
			break;
		}
		
		case 0x01: {
			DrvTile1TilemapWidth = 512;
			DrvTile1TilemapHeight = 512;
			break;
		}
		
		case 0x02: {
			DrvTile1TilemapWidth = 256;
			DrvTile1TilemapHeight = 1024;
			break;
		}
	}
	
	memset(pTile1LayerDraw, 0, DrvTile1TilemapWidth * DrvTile1TilemapHeight * sizeof(UINT16));
	
	for (my = 0; my < (DrvTile1TilemapHeight / 16); my++) {
		for (mx = 0; mx < (DrvTile1TilemapWidth / 16); mx++) {
			TileIndex = (mx & 0x0f) + ((my & 0x0f) << 4) + ((mx & 0x30) << 4);
			if (RenderType == 1) TileIndex = (mx & 0x0f) + ((my & 0x0f) << 4) + ((my & 0x10) << 4) + ((mx & 0x10) << 5);
			if (RenderType == 2) TileIndex = (mx & 0x0f) + ((my & 0x3f) << 4);
			if (DrvTileRamBank[1] & 0x01) TileIndex += 0x1000;			
			
			Attr = BURN_ENDIAN_SWAP_INT16(VideoRam[TileIndex]);
			Code = Attr & 0xfff;
			Colour = Attr >> 12;
			Layer = (Attr >> 12) > 7;
			
			if (DrawLayer == TILEMAP_BOTH_LAYERS || DrawLayer == Layer) {
				x = 16 * mx;
				y = 16 * my;
			
				if (Opaque) {
					if (DrvFlipScreen) {
						x = 240 - x;
						y = 240 - y;
						x &= (DrvTile1TilemapWidth - 1);
						y &= (DrvTile1TilemapHeight - 1);
						Dec0Render16x16Tile_FlipXY(pTile1LayerDraw, Code, x, y, Colour, 4, 512, DrvTiles1, DrvTile1TilemapWidth);
					} else {
						Dec0Render16x16Tile(pTile1LayerDraw, Code, x, y, Colour, 4, 512, DrvTiles1, DrvTile1TilemapWidth);
					}
				} else {
					if (DrvFlipScreen) {
						x = 240 - x;
						y = 240 - y;
						x &= (DrvTile1TilemapWidth - 1);
						y &= (DrvTile1TilemapHeight - 1);
						Dec0Render16x16Tile_Mask_FlipXY(pTile1LayerDraw, Code, x, y, Colour, 4, 0, 512, DrvTiles1, DrvTile1TilemapWidth);
					} else {
						Dec0Render16x16Tile_Mask(pTile1LayerDraw, Code, x, y, Colour, 4, 0, 512, DrvTiles1, DrvTile1TilemapWidth);
					}
				}
			}
		}
	}
	
	DrvRenderCustomTilemap(pTile1LayerDraw, (UINT16*)DrvVideo1Ctrl0Ram, (UINT16*)DrvVideo1Ctrl1Ram, (UINT16*)DrvVideo1RowScrollRam, (UINT16*)DrvVideo1ColScrollRam, DrvTile1TilemapWidth, DrvTile1TilemapHeight, Opaque, DrawLayer);
}

static void DrvRenderTile2Layer(INT32 Opaque, INT32 DrawLayer)
{
	INT32 mx, my, Code, Attr, Colour, x, y, TileIndex, Layer;
	UINT16 *Control0 = (UINT16*)DrvVideo2Ctrl0Ram;
	UINT16 *VideoRam = (UINT16*)DrvVideo2Ram;
	
	INT32 RenderType = BURN_ENDIAN_SWAP_INT16(Control0[3]) & 0x03;
	
	switch (RenderType) {
		case 0x00: {
			DrvTile2TilemapWidth = 1024;
			DrvTile2TilemapHeight = 256;
			break;
		}
		
		case 0x01: {
			DrvTile2TilemapWidth = 512;
			DrvTile2TilemapHeight = 512;
			break;
		}
		
		case 0x02: {
			DrvTile2TilemapWidth = 256;
			DrvTile2TilemapHeight = 1024;
			break;
		}
	}
	
	memset(pTile2LayerDraw, 0, DrvTile2TilemapWidth * DrvTile2TilemapHeight * sizeof(UINT16));
	
	for (my = 0; my < (DrvTile2TilemapHeight / 16); my++) {
		for (mx = 0; mx < (DrvTile2TilemapWidth / 16); mx++) {
			TileIndex = (mx & 0x0f) + ((my & 0x0f) << 4) + ((mx & 0x30) << 4);
			if (RenderType == 1) TileIndex = (mx & 0x0f) + ((my & 0x0f) << 4) + ((my & 0x10) << 4) + ((mx & 0x10) << 5);
			if (RenderType == 2) TileIndex = (mx & 0x0f) + ((my & 0x3f) << 4);
			if (DrvTileRamBank[2] & 0x01) TileIndex += 0x1000;
			
			Attr = BURN_ENDIAN_SWAP_INT16(VideoRam[TileIndex]);
			Code = Attr & 0xfff;
			Colour = Attr >> 12;
			Layer = (Attr >> 12) > 7;
			
			if (DrawLayer == TILEMAP_BOTH_LAYERS || DrawLayer == Layer) {
				x = 16 * mx;
				y = 16 * my;
			
				if (Opaque) {
					if (DrvFlipScreen) {
						x = 240 - x;
						y = 240 - y;
						x &= (DrvTile2TilemapWidth - 1);
						y &= (DrvTile2TilemapHeight - 1);
						Dec0Render16x16Tile_FlipXY(pTile2LayerDraw, Code, x, y, Colour, 4, 768, DrvTiles2, DrvTile2TilemapWidth);
					} else {
						Dec0Render16x16Tile(pTile2LayerDraw, Code, x, y, Colour, 4, 768, DrvTiles2, DrvTile2TilemapWidth);
					}
				} else {
					if (DrvFlipScreen) {
						x = 240 - x;
						y = 240 - y;
						x &= (DrvTile2TilemapWidth - 1);
						y &= (DrvTile2TilemapHeight - 1);
						Dec0Render16x16Tile_Mask_FlipXY(pTile2LayerDraw, Code, x, y, Colour, 4, 0, 768, DrvTiles2, DrvTile2TilemapWidth);
					} else {
						Dec0Render16x16Tile_Mask(pTile2LayerDraw, Code, x, y, Colour, 4, 0, 768, DrvTiles2, DrvTile2TilemapWidth);
					}
				}
			}
		}
	}
	
	DrvRenderCustomTilemap(pTile2LayerDraw, (UINT16*)DrvVideo2Ctrl0Ram, (UINT16*)DrvVideo2Ctrl1Ram, (UINT16*)DrvVideo2RowScrollRam, (UINT16*)DrvVideo2ColScrollRam, DrvTile2TilemapWidth, DrvTile2TilemapHeight, Opaque, DrawLayer);
}

static void DrvRenderCharLayer()
{
	INT32 mx, my, Code, Attr, Colour, x, y, TileIndex;
	UINT16 *Control0 = (UINT16*)DrvCharCtrl0Ram;
	UINT16 *CharRam = (UINT16*)DrvCharRam;
	
	INT32 RenderType = BURN_ENDIAN_SWAP_INT16(Control0[3]) & 0x03;
	
	switch (RenderType) {
		case 0x00: {
			DrvCharTilemapWidth = 1024;
			DrvCharTilemapHeight = 256;
			break;
		}
		
		case 0x01: {
			DrvCharTilemapWidth = 512;
			DrvCharTilemapHeight = 512;
			break;
		}
		
		case 0x02: {
			DrvCharTilemapWidth = 256;
			DrvCharTilemapHeight = 1024;
			break;
		}
	}
	
	memset(pCharLayerDraw, 0, DrvCharTilemapWidth * DrvCharTilemapHeight * sizeof(UINT16));
	
	for (my = 0; my < (DrvCharTilemapHeight / 8); my++) {
		for (mx = 0; mx < (DrvCharTilemapWidth / 8); mx++) {
			TileIndex = (mx & 0x1f) + ((my & 0x1f) << 5) + ((mx & 0x60) << 5);
			if (RenderType == 1) TileIndex = (mx & 0x1f) + ((my & 0x1f) << 5) + ((my & 0x20) << 5) + ((mx & 0x20) << 6);
			if (RenderType == 2) TileIndex = (mx & 0x1f) + ((my & 0x7f) << 5);
			if (DrvTileRamBank[0] & 0x01) TileIndex += 0x1000;
			
			Attr = BURN_ENDIAN_SWAP_INT16(CharRam[TileIndex]);
			Code = Attr & 0xfff;
			Colour = Attr >> 12;
			
			x = 8 * mx;
			y = 8 * my;
			
			if (DrvFlipScreen) {
				x = 248 - x;
				y = 248 - y;
				x &= (DrvCharTilemapWidth - 1);
				y &= (DrvCharTilemapHeight - 1);
				Dec0Render8x8Tile_Mask_FlipXY(pCharLayerDraw, Code, x, y, Colour, 4, 0, DrvCharPalOffset, DrvChars, DrvCharTilemapWidth);
			} else {
				Dec0Render8x8Tile_Mask(pCharLayerDraw, Code, x, y, Colour, 4, 0, DrvCharPalOffset, DrvChars, DrvCharTilemapWidth);
			}
		}
	}
	
	DrvRenderCustomTilemap(pCharLayerDraw, (UINT16*)DrvCharCtrl0Ram, (UINT16*)DrvCharCtrl1Ram, (UINT16*)DrvCharRowScrollRam, (UINT16*)DrvCharColScrollRam, DrvCharTilemapWidth, DrvCharTilemapHeight, 0, TILEMAP_BOTH_LAYERS);
}

static void DrvRenderSprites(INT32 PriorityMask, INT32 PriorityVal)
{
	UINT16 *spriteram = (UINT16*)DrvSpriteDMABufferRam;
	const INT32 size = 0x800/2;
	for (INT32 offs = 0; offs < size;)
	{
		INT32 incy;
		INT32 sy = BURN_ENDIAN_SWAP_INT16(spriteram[offs]);
		INT32 sx = BURN_ENDIAN_SWAP_INT16(spriteram[offs + 2]);
		INT32 color = sx >> 12;
		INT32 flash = sx & 0x800;
		INT32 flipx = sy & 0x2000;
		INT32 flipy = sy & 0x4000;
		INT32 h = (1 << ((sy & 0x1800) >> 11));
		INT32 w = (1 << ((sy & 0x0600) >>  9));

		sx = sx & 0x01ff;
		sy = sy & 0x01ff;
		if (sx >= 256) sx -= 512;
		if (sy >= 256) sy -= 512;
		sx = 240 - sx;
		sy = 240 - sy;

		INT32 mult = -16;

		if (DrvFlipScreen)
		{
			sy = 240 - sy;
			sx = 240 - sx;
			if (flipx) flipx = 0; else flipx = 1;
			if (flipy) flipy = 0; else flipy = 1;
			mult = 16;
		}
		else
			mult = -16;

		if ((BURN_ENDIAN_SWAP_INT16(spriteram[offs]) & 0x8000) == 0) {
			offs+=4;
			continue;
		}

		for (INT32 x = 0; x < w; x++)
		{
			if (offs < size)
			{
				INT32 code = (BURN_ENDIAN_SWAP_INT16(spriteram[offs + 1]) & 0x1fff) & ~(h - 1);

				if (BURN_ENDIAN_SWAP_INT16(spriteram[offs]) & 0x4000) {
					incy = -1;
				} else {
					code += h - 1;
					incy = 1;
				}

				for (INT32 y = 0; y < h; y++)
				{
					INT32 draw = 0;
					if (!flash || (GetCurrentFrame() & 1))
					{
						if ((color & PriorityMask) == PriorityVal)
						{
							draw = 1;
						}
					}

					if (draw)
					{
						Draw16x16MaskTile(pTransDraw, (code - y * incy) & 0xfff, sx + (mult * x), (sy + (mult * y)) - 8, flipx, flipy, color & 0xf, 4, 0, DrvSpritePalOffset, DrvSprites);
					}
				}
			}
			offs += 4;
		}
	}
}

static INT32 BaddudesDraw()
{
	UINT16 *Control0 = (UINT16*)DrvCharCtrl0Ram;
	DrvFlipScreen = BURN_ENDIAN_SWAP_INT16(Control0[0]) & 0x80;
	
	BurnTransferClear();
	DrvCalcPalette();
	
	if ((DrvPriority & 0x01) == 0x00) {
		if (nBurnLayer & 1) DrvRenderTile1Layer(1, TILEMAP_BOTH_LAYERS);
		if (nBurnLayer & 2) DrvRenderTile2Layer(0, TILEMAP_BOTH_LAYERS);
		if (nBurnLayer & 4) if (DrvPriority & 0x02) DrvRenderTile1Layer(0, TILEMAP_LAYER0);
		if (nSpriteEnable & 1) DrvRenderSprites(0, 0);
		if (nBurnLayer & 8) if (DrvPriority & 0x04) DrvRenderTile2Layer(0, TILEMAP_LAYER0);
	} else {
		if (nBurnLayer & 1) DrvRenderTile2Layer(1, TILEMAP_BOTH_LAYERS);
		if (nBurnLayer & 2) DrvRenderTile1Layer(0, TILEMAP_BOTH_LAYERS);
		if (nBurnLayer & 4) if (DrvPriority & 0x02) DrvRenderTile2Layer(0, TILEMAP_LAYER0);
		if (nSpriteEnable & 1) DrvRenderSprites(0, 0);
		if (nBurnLayer & 8) if (DrvPriority & 0x04) DrvRenderTile1Layer(0, TILEMAP_LAYER0);
	}
	
	if (nSpriteEnable & 2) DrvRenderCharLayer(); // not sprite layer, but ran out of layer bits
	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 BirdtryDraw()
{
	UINT16 *Control0 = (UINT16*)DrvCharCtrl0Ram;
	DrvFlipScreen = BURN_ENDIAN_SWAP_INT16(Control0[0]) & 0x80;
	
	BurnTransferClear();
	DrvCalcPalette();

	if (nBurnLayer & 1) DrvRenderTile2Layer(1, TILEMAP_BOTH_LAYERS);
	if (nSpriteEnable & 1) DrvRenderSprites(0x00, 0x00);
	if (nBurnLayer & 2) DrvRenderTile1Layer(0, TILEMAP_BOTH_LAYERS);
	if (nSpriteEnable & 2) DrvRenderSprites(0x00, 0x00);
		
	if (nBurnLayer & 4) DrvRenderCharLayer();
	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 HbarrelDraw()
{
	UINT16 *Control0 = (UINT16*)DrvCharCtrl0Ram;
	DrvFlipScreen = BURN_ENDIAN_SWAP_INT16(Control0[0]) & 0x80;
	
	BurnTransferClear();
	DrvCalcPalette();
	
	if (nBurnLayer & 1) DrvRenderTile2Layer(1, TILEMAP_BOTH_LAYERS);
	if (nSpriteEnable & 1) DrvRenderSprites(0x08, 0x08);
	if (nBurnLayer & 2) DrvRenderTile1Layer(0, TILEMAP_BOTH_LAYERS);
	if (nSpriteEnable & 2) DrvRenderSprites(0x08, 0x00);
		
	if (nBurnLayer & 4) DrvRenderCharLayer();
	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 HippodrmDraw()
{
	UINT16 *Control0 = (UINT16*)DrvCharCtrl0Ram;
	DrvFlipScreen = BURN_ENDIAN_SWAP_INT16(Control0[0]) & 0x80;
	
	BurnTransferClear();
	DrvCalcPalette();
	
	if (DrvPriority & 0x01) {
		if (nBurnLayer & 1) DrvRenderTile1Layer(1, TILEMAP_BOTH_LAYERS);
		if (nBurnLayer & 2) DrvRenderTile2Layer(0, TILEMAP_BOTH_LAYERS);
	} else {
		if (nBurnLayer & 1) DrvRenderTile2Layer(1, TILEMAP_BOTH_LAYERS);
		if (nBurnLayer & 2) DrvRenderTile1Layer(0, TILEMAP_BOTH_LAYERS);
	}
	
	if (nSpriteEnable & 1) DrvRenderSprites(0x00, 0x00);
	if (nBurnLayer & 4) DrvRenderCharLayer();
	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 MidresDraw()
{
	INT32 Trans;
	UINT16 *Control0 = (UINT16*)DrvCharCtrl0Ram;
	DrvFlipScreen = BURN_ENDIAN_SWAP_INT16(Control0[0]) & 0x80;
	
	if (DrvPriority & 0x04) {
		Trans = 0x00;
	} else {
		Trans = 0x08;
	}
	
	BurnTransferClear();
	Dec1CalcPalette();
	
	if (DrvPriority & 0x01) {
		if (nBurnLayer & 1) DrvRenderTile1Layer(1, TILEMAP_BOTH_LAYERS);
		if (nSpriteEnable & 1) if (DrvPriority & 0x02) DrvRenderSprites(0x08, Trans);
		if (nBurnLayer & 2) DrvRenderTile2Layer(0, TILEMAP_BOTH_LAYERS);
	} else {
		if (nBurnLayer & 1) DrvRenderTile2Layer(1, TILEMAP_BOTH_LAYERS);
		if (nSpriteEnable & 1) if (DrvPriority & 0x02) DrvRenderSprites(0x08, Trans);
		if (nBurnLayer & 2) DrvRenderTile1Layer(0, TILEMAP_BOTH_LAYERS);
	}
	
	if (DrvPriority & 0x02) {
		if (nSpriteEnable & 2) DrvRenderSprites(0x08, Trans ^ 0x08);
	} else {
		if (nSpriteEnable & 2) DrvRenderSprites(0x00, 0x00);
	}
	
	if (nBurnLayer & 4) DrvRenderCharLayer();
	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 RobocopDraw()
{
	INT32 Trans;
	UINT16 *Control0 = (UINT16*)DrvCharCtrl0Ram;
	DrvFlipScreen = BURN_ENDIAN_SWAP_INT16(Control0[0]) & 0x80;
	
	if (DrvPriority & 0x04) {
		Trans = 0x08;
	} else {
		Trans = 0x00;
	}
	
	BurnTransferClear();
	DrvCalcPalette();
	
	if (DrvPriority & 0x01) {
		if (nBurnLayer & 1) DrvRenderTile1Layer(1, TILEMAP_LAYER1);
		if (nSpriteEnable & 1) if (DrvPriority & 0x02) DrvRenderSprites(0x08, Trans);
		if (nBurnLayer & 2) DrvRenderTile2Layer(0, TILEMAP_BOTH_LAYERS);
	} else {
		if (nBurnLayer & 1) DrvRenderTile2Layer(1, TILEMAP_BOTH_LAYERS);
		if (nSpriteEnable & 1) if (DrvPriority & 0x02) DrvRenderSprites(0x08, Trans);
		if (nBurnLayer & 2) DrvRenderTile1Layer(0, TILEMAP_BOTH_LAYERS);
	}
	
	if (DrvPriority & 0x02) {
		if (nSpriteEnable & 2) DrvRenderSprites(0x08, Trans ^ 0x08);
	} else {
		if (nSpriteEnable & 2) DrvRenderSprites(0x00, 0x00);
	}
	
	if (nBurnLayer & 4) DrvRenderCharLayer();
	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 SlyspyDraw()
{
	UINT16 *Control0 = (UINT16*)DrvCharCtrl0Ram;
	DrvFlipScreen = BURN_ENDIAN_SWAP_INT16(Control0[0]) & 0x80;
	
	BurnTransferClear();
	Dec1CalcPalette();
	
	if (nBurnLayer & 1) DrvRenderTile2Layer(1, TILEMAP_BOTH_LAYERS);
	if (nBurnLayer & 2) DrvRenderTile1Layer(0, TILEMAP_BOTH_LAYERS);
	if (nSpriteEnable & 2) DrvRenderSprites(0x00, 0x00);
	if (nBurnLayer & 4) if (DrvPriority & 0x80) DrvRenderTile1Layer(0, TILEMAP_LAYER0);
	if (nBurnLayer & 8) DrvRenderCharLayer();
	BurnTransferCopy(DrvPalette);

	return 0;
}

#undef TILEMAP_BOTH_LAYERS
#undef TILEMAP_LAYER0
#undef TILEMAP_LAYER1

static INT32 DrvFrame()
{
	INT32 nInterleave = 272*4;

	if (DrvReset) BaddudesDoReset();

	DrvMakeInputs();

	if (game_rotates) {
		SuperJoy2Rotate();
	}

	nCyclesTotal[0] = (INT32)((double)10000000 / 57.41);
	nCyclesTotal[1] = (INT32)((double)1500000 / 57.41);
	nCyclesTotal[2] = (INT32)((double)8000000 / 57.41 / 12);
	nCyclesDone[0] = nExtraCycles[0]; nCyclesDone[1] = nExtraCycles[1]; nCyclesDone[2] = nExtraCycles[2];

	SekNewFrame();
	M6502NewFrame();
	mcs51NewFrame();
	NullNewFrame();

	SekOpen(0);
	M6502Open(0);

	for (INT32 i = 0; i < nInterleave; i++) {
		if (i == 8*4) DrvVBlank = 0;
		if (i == 248*4) {
			DrvVBlank = 1;
			SekSetIRQLine(6, CPU_IRQSTATUS_ACK);
		}

		CPU_RUN(0, Sek);

		CPU_RUN_TIMER(1);

		if (realMCU) {
			CPU_RUN(2, DrvMCU);
		}
	}

	SekClose();
	M6502Close();

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
		BurnYM3812Update(pBurnSoundOut, nBurnSoundLen);
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
	}

	nExtraCycles[0] = nCyclesDone[0] - nCyclesTotal[0];
	nExtraCycles[1] = 0; // sound cpu, not needed
	nExtraCycles[2] = nCyclesDone[2] - nCyclesTotal[2];

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 RobocopFrame()
{
	INT32 nInterleave = 264;

	if (DrvReset) RobocopDoReset();

	DrvMakeInputs();

	nCyclesTotal[0] = (INT32)((double)10000000 / 57.41);
	nCyclesTotal[1] = (INT32)((double)1500000 / 57.41);
	nCyclesTotal[2] = (INT32)((double)1342329 / 57.41);
	nCyclesDone[0] = nCyclesDone[1] = nCyclesDone[2] = 0;
	
	SekNewFrame();
	M6502NewFrame();
	
	SekOpen(0);
	M6502Open(0);
	h6280Open(0);

	for (INT32 i = 0; i < nInterleave; i++) {
		CPU_RUN(0, Sek);
		if (i == 8) DrvVBlank = 0;
		if (i == 248) {
			DrvVBlank = 1;
			SekSetIRQLine(6, CPU_IRQSTATUS_ACK);
		}

		CPU_RUN_TIMER(1);

		CPU_RUN(2, h6280);
	}

	SekClose();
	M6502Close();
	h6280Close();

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
		BurnYM3812Update(pBurnSoundOut, nBurnSoundLen);
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 Dec1Frame()
{
	INT32 nInterleave = 272;

	if (DrvReset) SlyspyDoReset();

	DrvMakeInputs();

	if (game_rotates) {
		SuperJoy2Rotate();
	}

	if (nPrevBurnCPUSpeedAdjust != nBurnCPUSpeedAdjust) {
		// 68K CPU clock is 10MHz, modified by nBurnCPUSpeedAdjust
		nCyclesTotal[0] = (INT32)((INT64)10000000.0 * 100 * nBurnCPUSpeedAdjust / (0x100 * nBurnFPS));
		INT32 adj_mhz = (INT32)(10000000.0 * nBurnCPUSpeedAdjust / 0x100);
		bprintf(0, _T("adjusted mhz / cycles per frame:  %d  /  %d\n"), adj_mhz, nCyclesTotal[0]);
		nPrevBurnCPUSpeedAdjust = nBurnCPUSpeedAdjust;
	}

	nCyclesTotal[1] = (INT32)((double)2000000 / 57.44);
	if (slyspy_mode)
		nCyclesTotal[1] = (INT32)((double)3000000 / 57.44);
	nCyclesDone[0] = nCyclesDone[1] = 0;
	
	SekNewFrame();
	h6280NewFrame();
	
	SekOpen(0);
	h6280Open(0);

	for (INT32 i = 0; i < nInterleave; i++) {
		CPU_RUN(0, Sek);

		if (i == 8) DrvVBlank = 0;
		if (i == 248) {
			DrvVBlank = 1;
			SekSetIRQLine(6, CPU_IRQSTATUS_AUTO);
		}

		CPU_RUN_TIMER(1);
	}

	SekClose();
	h6280Close();

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
		BurnYM3812Update(pBurnSoundOut, nBurnSoundLen);
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029722;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = RamStart;
		ba.nLen	  = RamEnd-RamStart;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}
	
	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);
		BurnYM2203Scan(nAction, pnMin);
		BurnYM3812Scan(nAction, pnMin);
		MSM6295Scan(nAction, pnMin);
		if (realMCU) {
			DrvMCUScan(nAction);
		}

		SCAN_VAR(i8751RetVal);
		SCAN_VAR(DrvVBlank);
		SCAN_VAR(DrvSoundLatch);
		SCAN_VAR(DrvFlipScreen);
		SCAN_VAR(DrvPriority);
		SCAN_VAR(DrvTileRamBank);
		SCAN_VAR(DrvSlyspyProtValue);
		SCAN_VAR(DrvSlyspySoundProt);

		SCAN_VAR(nRotate);
		SCAN_VAR(nRotateTarget);
		SCAN_VAR(nRotateTry);
		SCAN_VAR(nRotateHoldInput);
		SCAN_VAR(nAutoFireCounter);
		SCAN_VAR(nRotateTime);

		SCAN_VAR(nExtraCycles);
	}

	return 0;
}

static INT32 BaddudesScan(INT32 nAction, INT32 *pnMin)
{
	if (nAction & ACB_DRIVER_DATA) {
		M6502Scan(nAction);
	}
	
	return DrvScan(nAction, pnMin);
}

static INT32 RobocopScan(INT32 nAction, INT32 *pnMin)
{
	if (nAction & ACB_DRIVER_DATA) {
		h6280Scan(nAction);
	}

	return BaddudesScan(nAction, pnMin);
}

static INT32 SlyspyScan(INT32 nAction, INT32 *pnMin)
{
	if (nAction & ACB_DRIVER_DATA) {
		h6280Scan(nAction);
	}

	return DrvScan(nAction, pnMin);
}

struct BurnDriver BurnDrvBaddudes = {
	"baddudes", NULL, NULL, NULL, "1988",
	"Bad Dudes vs. Dragonninja (US, revision 1)\0", NULL, "Data East USA", "DEC0",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_SCRFIGHT, 0,
	NULL, BaddudesRomInfo, BaddudesRomName, NULL, NULL, NULL, NULL, Dec0InputInfo, BaddudesDIPInfo,
	BaddudesInit, BaddudesExit, DrvFrame, BaddudesDraw, BaddudesScan,
	NULL, 0x400, 256, 240, 4, 3
};

struct BurnDriver BurnDrvBirdtry = {
	"birdtry", NULL, NULL, NULL, "1988",
	"Birdie Try (Japan, revision 2, revision 1 MCU)\0", NULL, "Data East Corporation", "DEC0",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, birdtryRomInfo, birdtryRomName, NULL, NULL, NULL, NULL, BirdtryInputInfo, BirdtryDIPInfo,
	BirdtryInit, BaddudesExit, DrvFrame, BirdtryDraw, BaddudesScan,
	NULL, 0x400, 240, 256, 3, 4
};

struct BurnDriver BurnDrvBirdtrya = {
	"birdtrya", "birdtry", NULL, NULL, "1988",
	"Birdie Try (Japan, revision 2)\0", NULL, "Data East Corporation", "DEC0",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, birdtryaRomInfo, birdtryaRomName, NULL, NULL, NULL, NULL, BirdtryInputInfo, BirdtryDIPInfo,
	BirdtryInit, BaddudesExit, DrvFrame, BirdtryDraw, BaddudesScan,
	NULL, 0x400, 240, 256, 3, 4
};

struct BurnDriver BurnDrvBirdtryb = {
	"birdtryb", "birdtry", NULL, NULL, "1988",
	"Birdie Try (Japan, sample version)\0", NULL, "Data East Corporation", "DEC0",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, birdtrybRomInfo, birdtrybRomName, NULL, NULL, NULL, NULL, BirdtryInputInfo, BirdtryDIPInfo,
	BirdtryInit, BaddudesExit, DrvFrame, BirdtryDraw, BaddudesScan,
	NULL, 0x400, 240, 256, 3, 4
};

struct BurnDriver BurnDrvDrgninja = {
	"drgninja", "baddudes", NULL, NULL, "1988",
	"Dragonninja (Japan, revision 1)\0", NULL, "Data East Corporation", "DEC0",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_SCRFIGHT, 0,
	NULL, DrgninjaRomInfo, DrgninjaRomName, NULL, NULL, NULL, NULL, Dec0InputInfo, BaddudesDIPInfo,
	BaddudesInit, BaddudesExit, DrvFrame, BaddudesDraw, BaddudesScan,
	NULL, 0x400, 256, 240, 4, 3
};

struct BurnDriver BurnDrvDrgninjab = {
	"drgninjab", "baddudes", NULL, NULL, "1988",
	"Dragonninja (bootleg)\0", NULL, "bootleg", "DEC0",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_SCRFIGHT, 0,
	NULL, DrgninjabRomInfo, DrgninjabRomName, NULL, NULL, NULL, NULL, Dec0InputInfo, BaddudesDIPInfo,
	BaddudesInit, BaddudesExit, DrvFrame, BaddudesDraw, BaddudesScan,
	NULL, 0x400, 256, 240, 4, 3
};

struct BurnDriver BurnDrvDrgninjab2 = {
	"drgninjab2", "baddudes", NULL, NULL, "1988",
	"Dragonninja (bootleg with 68705)\0", NULL, "bootleg", "DEC0",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_SCRFIGHT, 0,
	NULL, Drgninjab2RomInfo, Drgninjab2RomName, NULL, NULL, NULL, NULL, Dec0InputInfo, BaddudesDIPInfo,
	Drgninjab2Init, BaddudesExit, DrvFrame, BaddudesDraw, BaddudesScan,
	NULL, 0x400, 256, 240, 4, 3
};

struct BurnDriver BurnDrvBouldash = {
	"bouldash", NULL, NULL, NULL, "1990",
	"Boulder Dash / Boulder Dash Part 2 (World)\0", NULL, "Data East Corporation (licensed from First Star)", "DEC0",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_MAZE, 0,
	NULL, BouldashRomInfo, BouldashRomName, NULL, NULL, NULL, NULL, Dec1InputInfo, BouldashDIPInfo,
	BouldashInit, SlyspyExit, Dec1Frame, SlyspyDraw, SlyspyScan,
	NULL, 0x400, 256, 240, 4, 3
};

struct BurnDriver BurnDrvBouldashj = {
	"bouldashj", "bouldash", NULL, NULL, "1990",
	"Boulder Dash / Boulder Dash Part 2 (Japan)\0", NULL, "Data East Corporation (licensed from First Star)", "DEC0",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_MAZE, 0,
	NULL, BouldashjRomInfo, BouldashjRomName, NULL, NULL, NULL, NULL, Dec1InputInfo, BouldashDIPInfo,
	BouldashInit, SlyspyExit, Dec1Frame, SlyspyDraw, SlyspyScan,
	NULL, 0x400, 256, 240, 4, 3
};

struct BurnDriver BurnDrvHbarrel = {
	"hbarrel", NULL, NULL, NULL, "1987",
	"Heavy Barrel (World)\0", NULL, "Data East Corporation", "DEC0",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_RUNGUN, 0,
	NULL, HbarrelRomInfo, HbarrelRomName, NULL, NULL, NULL, NULL, HbarrelInputInfo, HbarrelDIPInfo,
	HbarrelInit, BaddudesExit, DrvFrame, HbarrelDraw, BaddudesScan,
	NULL, 0x400, 240, 256, 3, 4
};

struct BurnDriver BurnDrvHbarrelu = {
	"hbarrelu", "hbarrel", NULL, NULL, "1987",
	"Heavy Barrel (US)\0", NULL, "Data East USA", "DEC0",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_RUNGUN, 0,
	NULL, HbarreluRomInfo, HbarreluRomName, NULL, NULL, NULL, NULL, HbarrelInputInfo, HbarrelDIPInfo,
	HbarrelInit, BaddudesExit, DrvFrame, HbarrelDraw, BaddudesScan,
	NULL, 0x400, 240, 256, 3, 4
};

struct BurnDriver BurnDrvHippodrm = {
	"hippodrm", NULL, NULL, NULL, "1989",
	"Hippodrome (US)\0", NULL, "Data East USA", "DEC0",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_VSFIGHT, 0,
	NULL, HippodrmRomInfo, HippodrmRomName, NULL, NULL, NULL, NULL, Dec0InputInfo, HippodrmDIPInfo,
	HippodrmInit, RobocopExit, RobocopFrame, HippodrmDraw, RobocopScan,
	NULL, 0x400, 256, 240, 4, 3
};

struct BurnDriver BurnDrvFfantasy = {
	"ffantasy", "hippodrm", NULL, NULL, "1989",
	"Fighting Fantasy (Japan, revision 3)\0", NULL, "Data East Corporation", "DEC0",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_VSFIGHT, 0,
	NULL, FfantasyRomInfo, FfantasyRomName, NULL, NULL, NULL, NULL, Dec0InputInfo, FfantasyDIPInfo,
	HippodrmInit, RobocopExit, RobocopFrame, HippodrmDraw, RobocopScan,
	NULL, 0x400, 256, 240, 4, 3
};

struct BurnDriver BurnDrvFfantasyj = {
	"ffantasyj", "hippodrm", NULL, NULL, "1989",
	"Fighting Fantasy (Japan, revision 2)\0", NULL, "Data East Corporation", "DEC0",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_VSFIGHT, 0,
	NULL, FfantasyjRomInfo, FfantasyjRomName, NULL, NULL, NULL, NULL, Dec0InputInfo, FfantasyDIPInfo,
	HippodrmInit, RobocopExit, RobocopFrame, HippodrmDraw, RobocopScan,
	NULL, 0x400, 256, 240, 4, 3
};

struct BurnDriver BurnDrvFfantasya = {
	"ffantasya", "hippodrm", NULL, NULL, "1989",
	"Fighting Fantasy (Japan)\0", NULL, "Data East Corporation", "DEC0",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_VSFIGHT, 0,
	NULL, FfantasyaRomInfo, FfantasyaRomName, NULL, NULL, NULL, NULL, Dec0InputInfo, FfantasyDIPInfo,
	HippodrmInit, RobocopExit, RobocopFrame, HippodrmDraw, RobocopScan,
	NULL, 0x400, 256, 240, 4, 3
};

struct BurnDriver BurnDrvFfantasyb = {
	"ffantasyb", "hippodrm", NULL, NULL, "1989",
	"Fighting Fantasy (Japan revision ?)\0", NULL, "Data East Corporation", "DEC0",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_VSFIGHT, 0,
	NULL, FfantasybRomInfo, FfantasybRomName, NULL, NULL, NULL, NULL, Dec0InputInfo, FfantasyDIPInfo,
	HippodrmInit, RobocopExit, RobocopFrame, HippodrmDraw, RobocopScan,
	NULL, 0x400, 256, 240, 4, 3
};

struct BurnDriver BurnDrvMidres = {
	"midres", NULL, NULL, NULL, "1989",
	"Midnight Resistance (World, set 1)\0", NULL, "Data East Corporation", "DEC0",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_RUNGUN, 0,
	NULL, MidresRomInfo, MidresRomName, NULL, NULL, NULL, NULL, MidresInputInfo, MidresDIPInfo,
	MidresInit, SlyspyExit, Dec1Frame, MidresDraw, SlyspyScan,
	NULL, 0x400, 256, 240, 4, 3
};

struct BurnDriver BurnDrvMidres2 = {
	"midres2", "midres", NULL, NULL, "1989",
	"Midnight Resistance (World, set 2)\0", NULL, "Data East Corporation", "DEC0",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_RUNGUN, 0,
	NULL, Midres2RomInfo, Midres2RomName, NULL, NULL, NULL, NULL, MidresInputInfo, MidresDIPInfo,
	MidresInit, SlyspyExit, Dec1Frame, MidresDraw, SlyspyScan,
	NULL, 0x400, 256, 240, 4, 3
};

struct BurnDriver BurnDrvMidresu = {
	"midresu", "midres", NULL, NULL, "1989",
	"Midnight Resistance (US)\0", NULL, "Data East USA", "DEC0",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_RUNGUN, 0,
	NULL, MidresuRomInfo, MidresuRomName, NULL, NULL, NULL, NULL, MidresInputInfo, MidresDIPInfo,
	MidresInit, SlyspyExit, Dec1Frame, MidresDraw, SlyspyScan,
	NULL, 0x400, 256, 240, 4, 3
};

struct BurnDriver BurnDrvMidresj = {
	"midresj", "midres", NULL, NULL, "1989",
	"Midnight Resistance (Japan)\0", NULL, "Data East Corporation", "DEC0",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_RUNGUN, 0,
	NULL, MidresjRomInfo, MidresjRomName, NULL, NULL, NULL, NULL, MidresInputInfo, MidresDIPInfo,
	MidresInit, SlyspyExit, Dec1Frame, MidresDraw, SlyspyScan,
	NULL, 0x400, 256, 240, 4, 3
};

struct BurnDriver BurnDrvRobocop = {
	"robocop", NULL, NULL, NULL, "1988",
	"Robocop (World, revision 4)\0", NULL, "Data East Corporation", "DEC0",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_RUNGUN, 0,
	NULL, RobocopRomInfo, RobocopRomName, NULL, NULL, NULL, NULL, Dec0InputInfo, RobocopDIPInfo,
	RobocopInit, RobocopExit, RobocopFrame, RobocopDraw, RobocopScan,
	NULL, 0x400, 256, 240, 4, 3
};

struct BurnDriver BurnDrvRobocopw = {
	"robocopw", "robocop", NULL, NULL, "1988",
	"Robocop (World, revision 3)\0", NULL, "Data East Corporation", "DEC0",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_RUNGUN, 0,
	NULL, RobocopwRomInfo, RobocopwRomName, NULL, NULL, NULL, NULL, Dec0InputInfo, RobocopDIPInfo,
	RobocopInit, RobocopExit, RobocopFrame, RobocopDraw, RobocopScan,
	NULL, 0x400, 256, 240, 4, 3
};

struct BurnDriver BurnDrvRobocopj = {
	"robocopj", "robocop", NULL, NULL, "1988",
	"Robocop (Japan)\0", NULL, "Data East Corporation", "DEC0",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_RUNGUN, 0,
	NULL, RobocopjRomInfo, RobocopjRomName, NULL, NULL, NULL, NULL, Dec0InputInfo, RobocopDIPInfo,
	RobocopInit, RobocopExit, RobocopFrame, RobocopDraw, RobocopScan,
	NULL, 0x400, 256, 240, 4, 3
};

struct BurnDriver BurnDrvRobocopu = {
	"robocopu", "robocop", NULL, NULL, "1988",
	"Robocop (US, revision 1)\0", NULL, "Data East USA", "DEC0",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_RUNGUN, 0,
	NULL, RobocopuRomInfo, RobocopuRomName, NULL, NULL, NULL, NULL, Dec0InputInfo, RobocopDIPInfo,
	RobocopInit, RobocopExit, RobocopFrame, RobocopDraw, RobocopScan,
	NULL, 0x400, 256, 240, 4, 3
};

struct BurnDriver BurnDrvRobocopu0 = {
	"robocopu0", "robocop", NULL, NULL, "1988",
	"Robocop (US, revision 0)\0", NULL, "Data East USA", "DEC0",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_RUNGUN, 0,
	NULL, Robocopu0RomInfo, Robocopu0RomName, NULL, NULL, NULL, NULL, Dec0InputInfo, RobocopDIPInfo,
	RobocopInit, RobocopExit, RobocopFrame, RobocopDraw, RobocopScan,
	NULL, 0x400, 256, 240, 4, 3
};

struct BurnDriver BurnDrvRobocopb = {
	"robocopb", "robocop", NULL, NULL, "1988",
	"Robocop (World bootleg)\0", NULL, "bootleg", "DEC0",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_RUNGUN, 0,
	NULL, RobocopbRomInfo, RobocopbRomName, NULL, NULL, NULL, NULL, Dec0InputInfo, RobocopDIPInfo,
	RobocopbInit, BaddudesExit, DrvFrame, RobocopDraw, BaddudesScan,
	NULL, 0x400, 256, 240, 4, 3
};

struct BurnDriver BurnDrvRobocopb2 = {
	"robocopb2", "robocop", NULL, NULL, "1989",
	"Robocop (Red Corporation World bootleg)\0", NULL, "bootleg", "DEC0",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_RUNGUN, 0,
	NULL, Robocopb2RomInfo, Robocopb2RomName, NULL, NULL, NULL, NULL, Dec0InputInfo, RobocopDIPInfo,
	RobocopbInit, BaddudesExit, DrvFrame, RobocopDraw, BaddudesScan,
	NULL, 0x400, 256, 240, 4, 3
};

struct BurnDriver BurnDrvSecretag = {
	"secretag", NULL, NULL, NULL, "1989",
	"Secret Agent (World, revision 3)\0", NULL, "Data East Corporation", "DEC0",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_RUNGUN, 0,
	NULL, SecretagRomInfo, SecretagRomName, NULL, NULL, NULL, NULL, Dec1InputInfo, SlyspyDIPInfo,
	SlyspyInit, SlyspyExit, Dec1Frame, SlyspyDraw, SlyspyScan,
	NULL, 0x400, 256, 240, 4, 3
};

struct BurnDriver BurnDrvSecretagj = {
	"secretagj", "secretag", NULL, NULL, "1989",
	"Secret Agent (Japan, revision 2)\0", NULL, "Data East Corporation", "DEC0",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_RUNGUN, 0,
	NULL, SecretagjRomInfo, SecretagjRomName, NULL, NULL, NULL, NULL, Dec1InputInfo, SlyspyDIPInfo,
	SlyspyInit, SlyspyExit, Dec1Frame, SlyspyDraw, SlyspyScan,
	NULL, 0x400, 256, 240, 4, 3
};

struct BurnDriver BurnDrvSlyspy = {
	"slyspy", "secretag", NULL, NULL, "1989",
	"Sly Spy (US, revision 4)\0", NULL, "Data East USA", "DEC0",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_RUNGUN, 0,
	NULL, SlyspyRomInfo, SlyspyRomName, NULL, NULL, NULL, NULL, Dec1InputInfo, SlyspyDIPInfo,
	SlyspyInit, SlyspyExit, Dec1Frame, SlyspyDraw, SlyspyScan,
	NULL, 0x400, 256, 240, 4, 3
};

struct BurnDriver BurnDrvSlyspy2 = {
	"slyspy2", "secretag", NULL, NULL, "1989",
	"Sly Spy (US, revision 2)\0", NULL, "Data East USA", "DEC0",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_RUNGUN, 0,
	NULL, Slyspy2RomInfo, Slyspy2RomName, NULL, NULL, NULL, NULL, Dec1InputInfo, SlyspyDIPInfo,
	SlyspyInit, SlyspyExit, Dec1Frame, SlyspyDraw, SlyspyScan,
	NULL, 0x400, 256, 240, 4, 3
};

struct BurnDriver BurnDrvSlyspy3 = {
	"slyspy3", "secretag", NULL, NULL, "1989",
	"Sly Spy (US, revision 3)\0", NULL, "Data East USA", "DEC0",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_RUNGUN, 0,
	NULL, Slyspy3RomInfo, Slyspy3RomName, NULL, NULL, NULL, NULL, Dec1InputInfo, SlyspyDIPInfo,
	SlyspyInit, SlyspyExit, Dec1Frame, SlyspyDraw, SlyspyScan,
	NULL, 0x400, 256, 240, 4, 3
};
