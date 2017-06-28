// FB Alpha Playmark hardware driver module
// Based on MAME driver by Nicola Salmoria, Pierpaolo Prazzoli, Quench

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "pic16c5x_intf.h"
#include "msm6295.h"
#include "eeprom.h"

static UINT8 DrvInputPort0[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvInputPort1[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvInputPort2[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvDip[2]        = {0, 0};
static UINT8 DrvInput[3]      = {0x00, 0x00, 0x00};
static UINT8 DrvReset         = 0;

static UINT8 *Mem             = NULL;
static UINT8 *MemEnd          = NULL;
static UINT8 *RamStart        = NULL;
static UINT8 *RamEnd          = NULL;
static UINT8 *Drv68kRom       = NULL;
static UINT8 *Drv68kRam       = NULL;
static UINT8 *DrvPicRom       = NULL;
static UINT8 *DrvMSM6295Src   = NULL;
static UINT8 *DrvSpriteRam    = NULL;
static UINT8 *DrvVideo1Ram    = NULL;
static UINT8 *DrvVideo2Ram    = NULL;
static UINT8 *DrvBgVideoRam   = NULL;
static UINT8 *DrvPaletteRam   = NULL;
static UINT32 *DrvPalette     = NULL;
static UINT8 *DrvSprites      = NULL;
static UINT8 *DrvTiles        = NULL;
static UINT8 *DrvChars        = NULL;
static UINT8 *DrvTempGfx      = NULL;

static UINT16 DrvFgScrollX = 0;
static UINT16 DrvFgScrollY = 0;
static UINT16 DrvCharScrollX = 0;
static UINT16 DrvCharScrollY = 0;
static UINT16 DrvBgEnable = 0;
static UINT16 DrvBgFullSize = 0;
static UINT16 DrvBgScrollX = 0;
static UINT16 DrvBgScrollY = 0;
static UINT16 DrvScreenEnable = 0;
static UINT8 DrvVBlank = 0;

static UINT8 DrvSoundCommand = 0;
static UINT8 DrvSoundFlag = 0;
static UINT8 DrvOkiControl = 0;
static UINT8 DrvOkiCommand = 0;
static UINT8 DrvOldOkiBank = 0;
static UINT8 DrvOkiBank = 0;

static INT32 Drv68kRomSize = 0;
static INT32 DrvMSM6295RomSize = 0;
static INT32 DrvNumTiles = 0;
static INT32 DrvTileSize = 0;
static INT32 DrvNumChars = 0;
static INT32 DrvCharSize = 0;
static INT32 DrvNumSprites = 0;
static INT32 DrvSpriteSize = 0;
static INT32 DrvEEPROMInUse = 0;

static INT32 nCyclesDone[2], nCyclesTotal[2];
static INT32 nCyclesSegment;
static INT32 nIRQLine = 2;

typedef void (*Render)();
static Render DrawFunction;
static void DrvRender();
static void ExcelsrRender();
static void HotmindRender();

static struct BurnInputInfo BigtwinInputList[] = {
	{"Coin 1"            , BIT_DIGITAL  , DrvInputPort0 + 4, "p1 coin"   },
	{"Start 1"           , BIT_DIGITAL  , DrvInputPort1 + 7, "p1 start"  },
	{"Coin 2"            , BIT_DIGITAL  , DrvInputPort0 + 5, "p2 coin"   },
	{"Start 2"           , BIT_DIGITAL  , DrvInputPort2 + 7, "p2 start"  },

	{"P1 Up"             , BIT_DIGITAL  , DrvInputPort1 + 0, "p1 up"     },
	{"P1 Down"           , BIT_DIGITAL  , DrvInputPort1 + 1, "p1 down"   },
	{"P1 Left"           , BIT_DIGITAL  , DrvInputPort1 + 2, "p1 left"   },
	{"P1 Right"          , BIT_DIGITAL  , DrvInputPort1 + 3, "p1 right"  },
	{"P1 Fire 1"         , BIT_DIGITAL  , DrvInputPort1 + 4, "p1 fire 1" },

	{"P2 Up"             , BIT_DIGITAL  , DrvInputPort2 + 0, "p2 up"     },
	{"P2 Down"           , BIT_DIGITAL  , DrvInputPort2 + 1, "p2 down"   },
	{"P2 Left"           , BIT_DIGITAL  , DrvInputPort2 + 2, "p2 left"   },
	{"P2 Right"          , BIT_DIGITAL  , DrvInputPort2 + 3, "p2 right"  },
	{"P2 Fire 1"         , BIT_DIGITAL  , DrvInputPort2 + 4, "p2 fire 1" },

	{"Reset"             , BIT_DIGITAL  , &DrvReset        , "reset"     },
	{"Dip 1"             , BIT_DIPSWITCH, DrvDip + 0       , "dip"       },
	{"Dip 2"             , BIT_DIPSWITCH, DrvDip + 1       , "dip"       },
};

STDINPUTINFO(Bigtwin)

static struct BurnInputInfo ExcelsrInputList[] = {
	{"Coin 1"            , BIT_DIGITAL  , DrvInputPort0 + 4, "p1 coin"   },
	{"Start 1"           , BIT_DIGITAL  , DrvInputPort1 + 7, "p1 start"  },
	{"Coin 2"            , BIT_DIGITAL  , DrvInputPort0 + 5, "p2 coin"   },
	{"Start 2"           , BIT_DIGITAL  , DrvInputPort2 + 7, "p2 start"  },

	{"P1 Up"             , BIT_DIGITAL  , DrvInputPort1 + 0, "p1 up"     },
	{"P1 Down"           , BIT_DIGITAL  , DrvInputPort1 + 1, "p1 down"   },
	{"P1 Left"           , BIT_DIGITAL  , DrvInputPort1 + 2, "p1 left"   },
	{"P1 Right"          , BIT_DIGITAL  , DrvInputPort1 + 3, "p1 right"  },
	{"P1 Fire 1"         , BIT_DIGITAL  , DrvInputPort1 + 4, "p1 fire 1" },
	{"P1 Fire 2"         , BIT_DIGITAL  , DrvInputPort1 + 5, "p1 fire 2" },
	{"P1 Fire 3"         , BIT_DIGITAL  , DrvInputPort1 + 6, "p1 fire 3" },
	
	{"P2 Up"             , BIT_DIGITAL  , DrvInputPort2 + 0, "p2 up"     },
	{"P2 Down"           , BIT_DIGITAL  , DrvInputPort2 + 1, "p2 down"   },
	{"P2 Left"           , BIT_DIGITAL  , DrvInputPort2 + 2, "p2 left"   },
	{"P2 Right"          , BIT_DIGITAL  , DrvInputPort2 + 3, "p2 right"  },
	{"P2 Fire 1"         , BIT_DIGITAL  , DrvInputPort2 + 4, "p2 fire 1" },
	{"P2 Fire 2"         , BIT_DIGITAL  , DrvInputPort2 + 5, "p2 fire 2" },
	{"P2 Fire 3"         , BIT_DIGITAL  , DrvInputPort2 + 6, "p2 fire 3" },

	{"Reset"             , BIT_DIGITAL  , &DrvReset        , "reset"     },
	{"Dip 1"             , BIT_DIPSWITCH, DrvDip + 0       , "dip"       },
	{"Dip 2"             , BIT_DIPSWITCH, DrvDip + 1       , "dip"       },
};

STDINPUTINFO(Excelsr)

static struct BurnInputInfo HotmindInputList[] = {
	{"Coin 1"            , BIT_DIGITAL  , DrvInputPort0 + 4, "p1 coin"   },
	{"Start 1"           , BIT_DIGITAL  , DrvInputPort1 + 7, "p1 start"  },
	{"Coin 2"            , BIT_DIGITAL  , DrvInputPort0 + 5, "p2 coin"   },
	{"Start 2"           , BIT_DIGITAL  , DrvInputPort2 + 7, "p2 start"  },

	{"P1 Up"             , BIT_DIGITAL  , DrvInputPort1 + 0, "p1 up"     },
	{"P1 Down"           , BIT_DIGITAL  , DrvInputPort1 + 1, "p1 down"   },
	{"P1 Left"           , BIT_DIGITAL  , DrvInputPort1 + 2, "p1 left"   },
	{"P1 Right"          , BIT_DIGITAL  , DrvInputPort1 + 3, "p1 right"  },
	{"P1 Fire 1"         , BIT_DIGITAL  , DrvInputPort1 + 4, "p1 fire 1" },
	{"P1 Fire 2"         , BIT_DIGITAL  , DrvInputPort1 + 5, "p1 fire 2" },

	{"Reset"             , BIT_DIGITAL  , &DrvReset        , "reset"     },
	{"Dip 1"             , BIT_DIPSWITCH, DrvDip + 0       , "dip"       },
	{"Dip 2"             , BIT_DIPSWITCH, DrvDip + 1       , "dip"       },
};

STDINPUTINFO(Hotmind)

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

	DrvClearOpposites(&DrvInput[1]);
	DrvClearOpposites(&DrvInput[2]);
}

static struct BurnDIPInfo BigtwinDIPList[] =
{
	// Default Values
	{0x0f, 0xff, 0xff, 0x4a, NULL                     },
	{0x10, 0xff, 0xff, 0xff, NULL                     },
	
	// Dip 1
	{0   , 0xfe, 0   , 2   , "Language"               },
	{0x0f, 0x01, 0x01, 0x00, "English"                },
	{0x0f, 0x01, 0x01, 0x01, "Italian"                },
	
	{0   , 0xfe, 0   , 2   , "Censor Pictures"        },
	{0x0f, 0x01, 0x04, 0x00, "No"                     },
	{0x0f, 0x01, 0x04, 0x04, "Yes"                    },
	
	{0   , 0xfe, 0   , 3   , "Difficulty"             },
	{0x0f, 0x01, 0x30, 0x30, "Normal"                 },
	{0x0f, 0x01, 0x30, 0x10, "Hard"                   },
	{0x0f, 0x01, 0x30, 0x00, "Very Hard"              },
	
	{0   , 0xfe, 0   , 2   , "Allow Continue"         },
	{0x0f, 0x01, 0x40, 0x00, "No"                     },
	{0x0f, 0x01, 0x40, 0x40, "Yes"                    },
	
	{0   , 0xfe, 0   , 2   , "Demo Sound"             },
	{0x0f, 0x01, 0x80, 0x80, "Off"                    },
	{0x0f, 0x01, 0x80, 0x00, "On"                     },

	// Dip 2
	{0   , 0xfe, 0   , 2   , "Coin Mode"              },
	{0x10, 0x01, 0x01, 0x01, "Mode 1"                 },
	{0x10, 0x01, 0x01, 0x00, "Mode 2"                 },
	
	{0   , 0xfe, 0   , 16  , "Coinage Mode 1"         },
	{0x10, 0x01, 0x1e, 0x14, "6 Coins 1 Credit"       },
	{0x10, 0x01, 0x1e, 0x16, "5 Coins 1 Credit"       },
	{0x10, 0x01, 0x1e, 0x14, "4 Coins 1 Credit"       },
	{0x10, 0x01, 0x1e, 0x18, "3 Coins 1 Credit"       },
	{0x10, 0x01, 0x1e, 0x1a, "8 Coins 3 Credits"      },
	{0x10, 0x01, 0x1e, 0x02, "2 Coins 1 Credit"       },
	{0x10, 0x01, 0x1e, 0x04, "5 Coins 1 Credits"      },
	{0x10, 0x01, 0x1e, 0x06, "3 Coins 2 Credits"      },
	{0x10, 0x01, 0x1e, 0x1e, "1 Coin  1 Credit"       },
	{0x10, 0x01, 0x1e, 0x08, "2 Coins 3 Credits"      },
	{0x10, 0x01, 0x1e, 0x12, "1 Coin  2 Credits"      },
	{0x10, 0x01, 0x1e, 0x10, "1 Coin  3 Credits"      },
	{0x10, 0x01, 0x1e, 0x0e, "1 Coin  4 Credits"      },
	{0x10, 0x01, 0x1e, 0x0c, "1 Coin  5 Credits"      },
	{0x10, 0x01, 0x1e, 0x0a, "1 Coin  6 Credits"      },
	{0x10, 0x01, 0x1e, 0x00, "Free Play"              },
	
	{0   , 0xfe, 0   , 2   , "Minimum Credits to Start" },
	{0x10, 0x01, 0x20, 0x20, "1"                      },
	{0x10, 0x01, 0x20, 0x00, "2"                      },
};

STDDIPINFO(Bigtwin)

static struct BurnDIPInfo ExcelsrDIPList[] =
{
	// Default Values
	{0x13, 0xff, 0xff, 0x63, NULL                     },
	{0x14, 0xff, 0xff, 0xff, NULL                     },
	
	// Dip 1
	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x13, 0x01, 0x03, 0x00, "1"                      },
	{0x13, 0x01, 0x03, 0x02, "2"                      },
	{0x13, 0x01, 0x03, 0x03, "3"                      },
	{0x13, 0x01, 0x03, 0x01, "4"                      },
	
	{0   , 0xfe, 0   , 3   , "Censor Pictures"        },
	{0x13, 0x01, 0x0c, 0x00, "No"                     },
	{0x13, 0x01, 0x0c, 0x08, "50%"                    },
	{0x13, 0x01, 0x0c, 0x0c, "100%"                   },
	
	{0   , 0xfe, 0   , 4   , "Difficulty"             },
	{0x13, 0x01, 0x30, 0x30, "Easy"                   },
	{0x13, 0x01, 0x30, 0x20, "Normal"                 },
	{0x13, 0x01, 0x30, 0x10, "Medium"                 },
	{0x13, 0x01, 0x30, 0x00, "Hard"                   },
	
	{0   , 0xfe, 0   , 2   , "Allow Continue"         },
	{0x13, 0x01, 0x40, 0x00, "No"                     },
	{0x13, 0x01, 0x40, 0x40, "Yes"                    },
	
	{0   , 0xfe, 0   , 2   , "Demo Sound"             },
	{0x13, 0x01, 0x80, 0x80, "Off"                    },
	{0x13, 0x01, 0x80, 0x00, "On"                     },

	// Dip 2
	{0   , 0xfe, 0   , 2   , "Coin Mode"              },
	{0x14, 0x01, 0x01, 0x01, "Mode 1"                 },
	{0x14, 0x01, 0x01, 0x00, "Mode 2"                 },
	
	{0   , 0xfe, 0   , 16  , "Coinage Mode 1"         },
	{0x14, 0x01, 0x1e, 0x14, "6 Coins 1 Credit"       },
	{0x14, 0x01, 0x1e, 0x16, "5 Coins 1 Credit"       },
	{0x14, 0x01, 0x1e, 0x14, "4 Coins 1 Credit"       },
	{0x14, 0x01, 0x1e, 0x18, "3 Coins 1 Credit"       },
	{0x14, 0x01, 0x1e, 0x1a, "8 Coins 3 Credits"      },
	{0x14, 0x01, 0x1e, 0x02, "2 Coins 1 Credit"       },
	{0x14, 0x01, 0x1e, 0x04, "5 Coins 1 Credits"      },
	{0x14, 0x01, 0x1e, 0x06, "3 Coins 2 Credits"      },
	{0x14, 0x01, 0x1e, 0x1e, "1 Coin  1 Credit"       },
	{0x14, 0x01, 0x1e, 0x08, "2 Coins 3 Credits"      },
	{0x14, 0x01, 0x1e, 0x12, "1 Coin  2 Credits"      },
	{0x14, 0x01, 0x1e, 0x10, "1 Coin  3 Credits"      },
	{0x14, 0x01, 0x1e, 0x0e, "1 Coin  4 Credits"      },
	{0x14, 0x01, 0x1e, 0x0c, "1 Coin  5 Credits"      },
	{0x14, 0x01, 0x1e, 0x0a, "1 Coin  6 Credits"      },
	{0x14, 0x01, 0x1e, 0x00, "Free Play"              },
	
	{0   , 0xfe, 0   , 2   , "Minimum Credits to Start" },
	{0x14, 0x01, 0x20, 0x20, "1"                      },
	{0x14, 0x01, 0x20, 0x00, "2"                      },
	
	{0   , 0xfe, 0   , 2   , "Percentage to Reveal"   },
	{0x14, 0x01, 0x40, 0x40, "80%"                    },
	{0x14, 0x01, 0x40, 0x00, "90%"                    },
	
	{0   , 0xfe, 0   , 2   , "Service Mode"           },
	{0x14, 0x01, 0x80, 0x80, "Off"                    },
	{0x14, 0x01, 0x80, 0x00, "On"                     },
};

STDDIPINFO(Excelsr)

static struct BurnDIPInfo HotmindDIPList[] =
{
	// Default Values
	{0x0b, 0xff, 0xff, 0xef, NULL                     },
	{0x0c, 0xff, 0xff, 0xff, NULL                     },
	
	// Dip 1
	{0   , 0xfe, 0   , 8   , "Difficulty"             },
	{0x0b, 0x01, 0x07, 0x07, "Easy"                   },
	{0x0b, 0x01, 0x07, 0x06, "Normal"                 },
	{0x0b, 0x01, 0x07, 0x05, "Hard"                   },
	{0x0b, 0x01, 0x07, 0x04, "Very Hard 1"            },
	{0x0b, 0x01, 0x07, 0x03, "Very Hard 2"            },
	{0x0b, 0x01, 0x07, 0x02, "Very Hard 3"            },
	{0x0b, 0x01, 0x07, 0x01, "Very Hard 4"            },
	{0x0b, 0x01, 0x07, 0x00, "Very Hard 5"            },
	
	{0   , 0xfe, 0   , 2   , "Service Mode"           },
	{0x0b, 0x01, 0x08, 0x08, "Off"                    },
	{0x0b, 0x01, 0x08, 0x00, "On"                     },
	
	{0   , 0xfe, 0   , 2   , "Demo Sound"             },
	{0x0b, 0x01, 0x10, 0x10, "Off"                    },
	{0x0b, 0x01, 0x10, 0x00, "On"                     },
	
	{0   , 0xfe, 0   , 2   , "Erogatore Gettoni"      },
	{0x0b, 0x01, 0x20, 0x20, "Off"                    },
	{0x0b, 0x01, 0x20, 0x00, "On"                     },
	
	{0   , 0xfe, 0   , 2   , "Erogatore Ticket"       },
	{0x0b, 0x01, 0x40, 0x40, "Off"                    },
	{0x0b, 0x01, 0x40, 0x00, "On"                     },
	
	{0   , 0xfe, 0   , 2   , "Clear Memory"           },
	{0x0b, 0x01, 0x80, 0x80, "Off"                    },
	{0x0b, 0x01, 0x80, 0x00, "On"                     },
	
	// Dip 2
	{0   , 0xfe, 0   , 2   , "Coin Mode"              },
	{0x0c, 0x01, 0x01, 0x01, "Mode 1"                 },
	{0x0c, 0x01, 0x01, 0x00, "Mode 2"                 },
	
	{0   , 0xfe, 0   , 16  , "Coinage Mode 1"         },
	{0x0c, 0x01, 0x1e, 0x14, "6 Coins 1 Credit"       },
	{0x0c, 0x01, 0x1e, 0x16, "5 Coins 1 Credit"       },
	{0x0c, 0x01, 0x1e, 0x14, "4 Coins 1 Credit"       },
	{0x0c, 0x01, 0x1e, 0x18, "3 Coins 1 Credit"       },
	{0x0c, 0x01, 0x1e, 0x1a, "8 Coins 3 Credits"      },
	{0x0c, 0x01, 0x1e, 0x02, "2 Coins 1 Credit"       },
	{0x0c, 0x01, 0x1e, 0x04, "5 Coins 1 Credits"      },
	{0x0c, 0x01, 0x1e, 0x06, "3 Coins 2 Credits"      },
	{0x0c, 0x01, 0x1e, 0x1e, "1 Coin  1 Credit"       },
	{0x0c, 0x01, 0x1e, 0x08, "2 Coins 3 Credits"      },
	{0x0c, 0x01, 0x1e, 0x12, "1 Coin  2 Credits"      },
	{0x0c, 0x01, 0x1e, 0x10, "1 Coin  3 Credits"      },
	{0x0c, 0x01, 0x1e, 0x0e, "1 Coin  4 Credits"      },
	{0x0c, 0x01, 0x1e, 0x0c, "1 Coin  5 Credits"      },
	{0x0c, 0x01, 0x1e, 0x0a, "1 Coin  6 Credits"      },
	{0x0c, 0x01, 0x1e, 0x00, "Free Play"              },
};

STDDIPINFO(Hotmind)

static struct BurnRomInfo BigtwinRomDesc[] = {
	{ "2.302",            0x80000, 0xe6767f60, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "3.301",            0x80000, 0x5aba6990, BRF_ESS | BRF_PRG }, //  1	68000 Program Code
	
	{ "pic16c57-hs_bigtwin_015.hex", 0x02d4c, 0xc07e9375, BRF_ESS | BRF_PRG }, //  2	PIC16C57 HEX

	{ "4.311",            0x40000, 0x6f628fbc, BRF_GRA },			//  3	Tiles
	{ "5.312",            0x40000, 0x6a9b1752, BRF_GRA },			//  4	Tiles
	{ "6.313",            0x40000, 0x411cf852, BRF_GRA },			//  5	Tiles
	{ "7.314",            0x40000, 0x635c81fd, BRF_GRA },			//  6	Tiles
	
	{ "8.321",            0x20000, 0x2749644d, BRF_GRA },			//  7	Sprites
	{ "9.322",            0x20000, 0x1d1897af, BRF_GRA },			//  8	Sprites
	{ "10.323",           0x20000, 0x2a03432e, BRF_GRA },			//  9	Sprites
	{ "11.324",           0x20000, 0x2c980c4c, BRF_GRA },			//  10	Sprites
	
	{ "1.013",            0x40000, 0xff6671dc, BRF_SND },			//  11	Samples
};

STD_ROM_PICK(Bigtwin)
STD_ROM_FN(Bigtwin)

static struct BurnRomInfo ExcelsrRomDesc[] = {
	{ "22.u301",          0x80000, 0xf0aa1c1b, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "19.u302",          0x80000, 0x9a8acddc, BRF_ESS | BRF_PRG }, //  1	68000 Program Code
	{ "21.u303",          0x80000, 0xfdf9bd64, BRF_ESS | BRF_PRG }, //  2	68000 Program Code
	{ "18.u304",          0x80000, 0xfe517e0e, BRF_ESS | BRF_PRG }, //  3	68000 Program Code
	{ "20.u305",          0x80000, 0x8692afe9, BRF_ESS | BRF_PRG }, //  4	68000 Program Code
	{ "17.u306",          0x80000, 0x978f9a6b, BRF_ESS | BRF_PRG }, //  5	68000 Program Code
	
	{ "pic16c57-hs_excelsior_i015.hex", 0x02d4c, 0x022c6941, BRF_ESS | BRF_PRG }, //  6	PIC16C57 HEX

	{ "26.u311",          0x80000, 0xc171c059, BRF_GRA },			//  7	Tiles
	{ "30.u312",          0x80000, 0xb4a4c510, BRF_GRA },			//  8	Tiles
	{ "25.u313",          0x80000, 0x667eec1b, BRF_GRA },			//  9	Tiles
	{ "29.u314",          0x80000, 0x4acb0745, BRF_GRA },			//  10	Tiles
	
	{ "24.u321",          0x80000, 0x17f46825, BRF_GRA },			//  11	Sprites
	{ "28.u322",          0x80000, 0xa823f2bd, BRF_GRA },			//  12	Sprites
	{ "23.u323",          0x80000, 0xd8e1453b, BRF_GRA },			//  13	Sprites
	{ "27.u324",          0x80000, 0xeca2c079, BRF_GRA },			//  14	Sprites
	
	{ "16.i013",          0x80000, 0x7ed9da5d, BRF_SND },			//  15	Samples
};

STD_ROM_PICK(Excelsr)
STD_ROM_FN(Excelsr)

static struct BurnRomInfo ExcelsraRomDesc[] = {
	{ "22.u301",          0x80000, 0x55dca2da, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "19.u302",          0x80000, 0xd13990a8, BRF_ESS | BRF_PRG }, //  1	68000 Program Code
	{ "21.u303",          0x80000, 0xfdf9bd64, BRF_ESS | BRF_PRG }, //  2	68000 Program Code
	{ "18.u304",          0x80000, 0xfe517e0e, BRF_ESS | BRF_PRG }, //  3	68000 Program Code
	{ "20.u305",          0x80000, 0x8692afe9, BRF_ESS | BRF_PRG }, //  4	68000 Program Code
	{ "17.u306",          0x80000, 0x978f9a6b, BRF_ESS | BRF_PRG }, //  5	68000 Program Code
	
	{ "pic16c57-hs_excelsior_i015.hex", 0x02d4c, 0x022c6941, BRF_ESS | BRF_PRG }, //  6	PIC16C57 HEX

	{ "26.u311",          0x80000, 0xc171c059, BRF_GRA },			//  7	Tiles
	{ "30.u312",          0x80000, 0xb4a4c510, BRF_GRA },			//  8	Tiles
	{ "25.u313",          0x80000, 0x667eec1b, BRF_GRA },			//  9	Tiles
	{ "29.u314",          0x80000, 0x4acb0745, BRF_GRA },			//  10	Tiles
	
	{ "24.u321",          0x80000, 0x17f46825, BRF_GRA },			//  11	Sprites
	{ "28.u322",          0x80000, 0xa823f2bd, BRF_GRA },			//  12	Sprites
	{ "23.u323",          0x80000, 0xd8e1453b, BRF_GRA },			//  13	Sprites
	{ "27.u324",          0x80000, 0xeca2c079, BRF_GRA },			//  14	Sprites
	
	{ "16.i013",          0x80000, 0x7ed9da5d, BRF_SND },			//  15	Samples
};

STD_ROM_PICK(Excelsra)
STD_ROM_FN(Excelsra)

static struct BurnRomInfo HotmindRomDesc[] = {
	{ "21.u67",           0x20000, 0xe9000f7f, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "22.u66",           0x20000, 0x2c518ec5, BRF_ESS | BRF_PRG }, //  1	68000 Program Code
	
	{ "hotmind_pic16c57.hex", 0x02d4c, 0x11957803, BRF_ESS | BRF_PRG }, //  2	PIC16C57 HEX

	{ "23.u36",           0x20000, 0xddcf60b9, BRF_GRA },			//  3	Tiles
	{ "27.u42",           0x20000, 0x413bbcf4, BRF_GRA },			//  4	Tiles
	{ "24.u39",           0x20000, 0x4baa5b4c, BRF_GRA },			//  5	Tiles
	{ "28.u45",           0x20000, 0x8df34d6a, BRF_GRA },			//  6	Tiles
	
	{ "26.u86",           0x20000, 0xff8d3b75, BRF_GRA },			//  7	Sprites
	{ "30.u85",           0x20000, 0x87a640c7, BRF_GRA },			//  8	Sprites
	{ "25.u84",           0x20000, 0xc4fd4445, BRF_GRA },			//  9	Sprites
	{ "29.u83",           0x20000, 0x0bebfb53, BRF_GRA },			//  10	Sprites
	
	{ "20.io13",          0x40000, 0x0bf3a3e5, BRF_SND },			//  11	Samples
	
	{ "hotmind_pic16c57-hs_io15.hex", 0x02d4c, 0xf3300d13, BRF_OPT },
	{ "palce16v8h-25-pc4_u58.jed",    0x00b89, 0xba88c1da, BRF_OPT },
	{ "palce16v8h-25-pc4_u182.jed",   0x00b89, 0xba88c1da, BRF_OPT },
	{ "palce16v8h-25-pc4_jamma.jed",  0x00b89, 0xba88c1da, BRF_OPT },
	{ "tibpal22v10acnt_u113.jed",     0x01e84, 0x94106c63, BRF_OPT },
	{ "tibpal22v10acnt_u183.jed",     0x01e84, 0x95a446b6, BRF_OPT },
	{ "tibpal22v10acnt_u211.jed",     0x01e84, 0x94106c63, BRF_OPT },
};

STD_ROM_PICK(Hotmind)
STD_ROM_FN(Hotmind)

static INT32 DrvDoReset()
{
	SekOpen(0);
	SekReset();
	SekClose();
	
	pic16c5xReset();
	
	MSM6295Reset(0);
	
	if (DrvEEPROMInUse) EEPROMReset();
	
	DrvFgScrollX = 0;
	DrvFgScrollY = 0;
	DrvCharScrollX = 0;
	DrvCharScrollY = 0;
	DrvBgEnable = 0;
	DrvBgFullSize = 0;
	DrvBgScrollX = 0;
	DrvBgScrollY = 0;
	
	DrvSoundCommand = 0;
	DrvSoundFlag = 0;
	DrvOkiControl = 0;
	DrvOkiCommand = 0;
	DrvOldOkiBank = 0;
	DrvOkiBank = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = Mem;

	Drv68kRom            = Next; Next += Drv68kRomSize;
	MSM6295ROM           = Next; Next += 0x040000;
	DrvMSM6295Src        = Next; Next += DrvMSM6295RomSize;
	DrvPicRom            = Next; Next += 0x001000;

	RamStart = Next;

	Drv68kRam            = Next; Next += 0x010000;
	DrvSpriteRam         = Next; Next += 0x001000;
	DrvVideo1Ram         = Next; Next += 0x008000;
	DrvVideo2Ram         = Next; Next += 0x004000;
	DrvBgVideoRam        = Next; Next += 0x080000;
	DrvPaletteRam        = Next; Next += 0x000800;
	
	RamEnd = Next;

	DrvSprites           = Next; Next += (DrvNumSprites * DrvSpriteSize);
	DrvTiles             = Next; Next += (DrvNumTiles * DrvTileSize);
	DrvChars             = Next; Next += (DrvNumChars * DrvCharSize);
	DrvPalette           = (UINT32*)Next; Next += 0x00400 * sizeof(UINT32);

	MemEnd = Next;

	return 0;
}

static inline UINT8 pal5bit(UINT8 bits)
{
	bits &= 0x1f;
	return (bits << 3) | (bits >> 2);
}

static inline void CalcCol(UINT16 Offset, UINT16 nColour)
{
	INT32 r, g, b;
	
	r = (nColour >> 11) & 0x1e;
	g = (nColour >>  7) & 0x1e;
	b = (nColour >>  3) & 0x1e;

	r |= ((nColour & 0x08) >> 3);
	g |= ((nColour & 0x04) >> 2);
	b |= ((nColour & 0x02) >> 1);

	DrvPalette[Offset] = BurnHighCol(pal5bit(r), pal5bit(g), pal5bit(b), 0);
}

UINT8 __fastcall DrvReadByte(UINT32 a)
{
	switch (a) {
		case 0x700013: {
			return 0xff - DrvInput[1];
		}
		
		case 0x700015: {
			return 0xff - DrvInput[2];
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("Read byte -> %06X\n"), a);
		}
	}
	
	return 0;
}

void __fastcall DrvWriteByte(UINT32 a, UINT8 d)
{
	switch (a) {
		case 0x70001f: {
			DrvSoundCommand = d;
			DrvSoundFlag = 1;
			return;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("Write byte -> %06X, %02X\n"), a, d);
		}
	}
}

UINT16 __fastcall DrvReadWord(UINT32 a)
{
	switch (a) {
		case 0x700010: {
			return 0xffff - DrvInput[0];
		}
		
		case 0x70001a: {
			return 0xff00 | DrvDip[0];
		}
		
		case 0x70001c: {
			return 0xff00 | DrvDip[1];
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("Read Word -> %06X\n"), a);
		}
	}
	
	return 0;
}

void __fastcall DrvWriteWord(UINT32 a, UINT16 d)
{
	if (a >= 0x501000 && a <= 0x501fff) {
		// unused ram???
		return;
	}
	
	if (a >= 0x504000 && a <= 0x50ffff) {
		// unused ram???
		return;
	}
	
	if (a >= 0x780000 && a <= 0x7807ff) {
		UINT16 *PalRam = (UINT16*)DrvPaletteRam;
		INT32 Offset = (a & 0x7ff) >> 1;
		PalRam[Offset] = d;
		CalcCol(Offset, d);
		return;
	}
	
	switch (a) {
		case 0x304000: {
			// irq ack???
			return;
		}
		
		case 0x510000: {
			DrvCharScrollX = (d + 2) & 0x1ff;
			return;
		}
		
		case 0x510002: {
			DrvCharScrollY = d & 0xff;
			return;
		}
		
		case 0x510004: {
			DrvBgScrollX = -(d + 4);
			return;
		}
		
		case 0x510006: {
			DrvBgScrollY = -d & 0x1ff;
			DrvBgEnable = d & 0x200;
			DrvBgFullSize = d & 0x400;	
			return;
		}
		
		case 0x510008: {
			DrvFgScrollX = (d + 6) & 0x1ff;
			return;
		}
		
		case 0x51000a: {
			DrvFgScrollY = d & 0x1ff;
			return;
		}
		
		case 0x51000c: {
			// nop???
			return;
		}
		
		case 0xe00000: {
			// ???
			return;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("Write word -> %06X, %04X\n"), a, d);
		}
	}
}

UINT8 __fastcall ExcelsrReadByte(UINT32 a)
{
	switch (a) {
		default: {
			bprintf(PRINT_NORMAL, _T("Read byte -> %06X\n"), a);
		}
	}
	
	return 0;
}

void __fastcall ExcelsrWriteByte(UINT32 a, UINT8 d)
{
	switch (a) {
		case 0x70001f: {
			DrvSoundCommand = d;
			DrvSoundFlag = 1;
			return;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("Write byte -> %06X, %02X\n"), a, d);
		}
	}
}

UINT16 __fastcall ExcelsrReadWord(UINT32 a)
{
	switch (a) {
		case 0x700010: {
			return 0xffff - DrvInput[0];
		}
		
		case 0x700012: {
			return 0xffff - DrvInput[1];
		}
		
		case 0x700014: {
			return 0xffff - DrvInput[2];
		}
		
		case 0x70001a: {
			return 0xff00 | DrvDip[0];
		}
		
		case 0x70001c: {
			return 0xff00 | DrvDip[1];
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("Read Word -> %06X\n"), a);
		}
	}
	
	return 0;
}

void __fastcall ExcelsrWriteWord(UINT32 a, UINT16 d)
{
	if (a >= 0x780000 && a <= 0x7807ff) {
		UINT16 *PalRam = (UINT16*)DrvPaletteRam;
		INT32 Offset = (a & 0x7ff) >> 1;
		PalRam[Offset] = d;
		CalcCol(Offset, d);
		return;
	}
	
	switch (a) {
		case 0x304000: {
			// nop
			return;
		}
		
		case 0x510000: {
			DrvCharScrollX = (d + 2) & 0x1ff;
			return;
		}
		
		case 0x510002: {
			DrvCharScrollY = d & 0xff;
			return;
		}
		
		case 0x510004: {
			DrvBgScrollX = -d;
			return;
		}
		
		case 0x510006: {
			DrvBgScrollY = (-d + 2) & 0x1ff;
			DrvBgEnable = d & 0x200;
			DrvBgFullSize = d & 0x400;	
			return;
		}
		
		case 0x510008: {
			DrvFgScrollX = (d + 6) & 0x1ff;
			return;
		}
		
		case 0x51000a: {
			DrvFgScrollY = d & 0x1ff;
			return;
		}
		
		case 0x51000c: {
			// nop???
			return;
		}
		
		case 0xe00000: {
			// ???
			return;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("Write word -> %06X, %04X\n"), a, d);
		}
	}
}

UINT8 __fastcall HotmindReadByte(UINT32 a)
{
	switch (a) {
		case 0x300011: {
			return 0xff - DrvInput[0];
		}
		
		case 0x300013: {
			return 0xff - DrvInput[1];
		}
		
		case 0x300015: {
			return 0x3f - DrvInput[2] + (DrvVBlank ? 0x00 : 0x40) + (EEPROMRead() ? 0x80 : 0x00);
		}
		
		case 0x30001b: {
			return DrvDip[0];
		}
		
		case 0x30001d: {
			return DrvDip[1];
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("Read byte -> %06X\n"), a);
		}
	}
	
	return 0;
}

void __fastcall HotmindWriteByte(UINT32 a, UINT8 d)
{
	switch (a) {
		case 0x300015: {
			EEPROMSetCSLine((d & 0x01) ? EEPROM_CLEAR_LINE : EEPROM_ASSERT_LINE );
			EEPROMWriteBit(d & 0x04);
			EEPROMSetClockLine((d & 0x02) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE );
			return;
		}
		
		case 0x30001f: {
			DrvSoundCommand = d;
			DrvSoundFlag = 1;
			return;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("Write byte -> %06X, %02X\n"), a, d);
		}
	}
}

UINT16 __fastcall HotmindReadWord(UINT32 a)
{
	switch (a) {
		default: {
			bprintf(PRINT_NORMAL, _T("Read Word -> %06X\n"), a);
		}
	}
	
	return 0;
}

void __fastcall HotmindWriteWord(UINT32 a, UINT16 d)
{
	if (a >= 0x280000 && a <= 0x2807ff) {
		UINT16 *PalRam = (UINT16*)DrvPaletteRam;
		INT32 Offset = (a & 0x7ff) >> 1;
		PalRam[Offset] = d;
		CalcCol(Offset, d);
		return;
	}
	
	switch (a) {
		case 0x110000: {
			DrvCharScrollX = (d + 14) & 0x1ff;
			return;
		}
		
		case 0x110002: {
			DrvCharScrollY = d & 0x1ff;
			return;
		}
		
		case 0x110004: {
			DrvFgScrollX = (d + 14) & 0x1ff;
			return;
		}
		
		case 0x110006: {
			DrvFgScrollY = d & 0x1ff;
			return;
		}
		
		case 0x110008: {
			DrvBgScrollX = (d + 14) & 0x1ff;
			return;
		}
		
		case 0x11000a: {
			DrvBgScrollY = d & 0x1ff;
			return;
		}
		
		case 0x11000c: {
			DrvScreenEnable = d & 0x01;
			return;
		}
		
		case 0x304000: {
			// nop
			return;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("Write word -> %06X, %04X\n"), a, d);
		}
	}
}

UINT8 PlaymarkSoundReadPort(UINT16 Port)
{
	switch (Port) {
		case 0x01: {
			UINT8 Data = 0;

			if ((DrvOkiControl & 0x38) == 0x30) {
				Data = DrvSoundCommand;
			} else {
				if ((DrvOkiControl & 0x38) == 0x28) {
					Data = MSM6295ReadStatus(0) & 0x0f;
				}
			}

			return Data;
		}
		
		case 0x02: {
			if (DrvSoundFlag) {
				DrvSoundFlag = 0;
				return 0x00;
			}
			return 0x40;
		}
		
		case 0x10: {
			return 0;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("Sound Read Port %x\n"), Port);
		}
	}

	return 0;
}

void PlaymarkSoundWritePort(UINT16 Port, UINT8 Data)
{
	switch (Port & 0xff) {
		case 0x00: {
			if (DrvMSM6295RomSize > 0x40000) {
				if (DrvOldOkiBank != (Data & 0x07)) {
					DrvOldOkiBank = Data & 0x07;

					if (((DrvOldOkiBank - 1) * 0x40000) < DrvMSM6295RomSize) {
						memcpy(MSM6295ROM + 0x00000, DrvMSM6295Src + (0x40000 * (DrvOldOkiBank - 1)), 0x40000);
					}
				}
			}
			return;
		}
		
		case 0x01: {
			DrvOkiCommand = Data;
			return;
		}
		
		case 0x02: {
			DrvOkiControl = Data;

			if ((Data & 0x38) == 0x18) {
				MSM6295Command(0, DrvOkiCommand);
			}
			return;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("Sound Write Port %x, %x\n"), Port, Data);
		}
	}
}

static INT32 DrvCharPlaneOffsets[4]       = { 0x600000, 0x400000, 0x200000, 0 };
static INT32 DrvCharXOffsets[8]           = { 0, 1, 2, 3, 4, 5, 6, 7 };
static INT32 DrvCharYOffsets[8]           = { 0, 8, 16, 24, 32, 40, 48, 56 };
static INT32 DrvTilePlaneOffsets[4]       = { 0x600000, 0x400000, 0x200000, 0 };
static INT32 DrvTileXOffsets[16]          = { 0, 1, 2, 3, 4, 5, 6, 7, 128, 129, 130, 131, 132, 133, 134, 135 };
static INT32 DrvTileYOffsets[16]          = { 0, 8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 88, 96, 104, 112, 120 };
static INT32 DrvSpritePlaneOffsets[4]     = { 0x300000, 0x200000, 0x100000, 0 };
static INT32 DrvSpriteXOffsets[32]        = { 0, 1, 2, 3, 4, 5, 6, 7, 128, 129, 130, 131, 132, 133, 134, 135, 256, 257, 258, 259, 260, 261, 262, 263, 384, 385, 386, 387, 388, 389, 390, 391 };
static INT32 DrvSpriteYOffsets[32]        = { 0, 8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 88, 96, 104, 112, 120, 512, 520, 528, 536, 544, 552, 560, 568, 576, 584, 592, 600, 608, 616, 624, 632 };

static INT32 DrvInit()
{
	INT32 nRet = 0, nLen;
	
	Drv68kRomSize = 0x100000;
	DrvMSM6295RomSize = 0;
	DrvNumTiles = 0x2000;
	DrvTileSize = 16 * 16;
	DrvNumChars = 0x2000;
	DrvCharSize = 8 * 8;
	DrvNumSprites = 0x400;
	DrvSpriteSize = 32 * 32;
	
	Mem = NULL;
	MemIndex();
	nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(Mem, 0, nLen);
	MemIndex();

	DrvTempGfx = (UINT8*)BurnMalloc(0x100000);
	
	nRet = BurnLoadRom(Drv68kRom  + 0x00001, 0, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68kRom  + 0x00000, 1, 2); if (nRet != 0) return 1;
	
	if (BurnLoadPicROM(DrvPicRom, 2, 0x2d4c)) return 1;
	
	nRet = BurnLoadRom(DrvTempGfx + 0x00000, 3, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempGfx + 0x40000, 4, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempGfx + 0x80000, 5, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempGfx + 0xc0000, 6, 1); if (nRet != 0) return 1;
	GfxDecode(DrvNumTiles, 4, 16, 16, DrvTilePlaneOffsets, DrvTileXOffsets, DrvTileYOffsets, 0x100, DrvTempGfx, DrvTiles);
	GfxDecode(DrvNumChars, 4, 8, 8, DrvCharPlaneOffsets, DrvCharXOffsets, DrvCharYOffsets, 0x100, DrvTempGfx, DrvChars);

	memset(DrvTempGfx, 0, 0x100000);
	nRet = BurnLoadRom(DrvTempGfx + 0x00000, 7, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempGfx + 0x20000, 8, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempGfx + 0x40000, 9, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempGfx + 0x60000, 10, 1); if (nRet != 0) return 1;
	GfxDecode(DrvNumSprites, 4, 32, 32, DrvSpritePlaneOffsets, DrvSpriteXOffsets, DrvSpriteYOffsets, 0x400, DrvTempGfx, DrvSprites);
	BurnFree(DrvTempGfx);

	nRet = BurnLoadRom(MSM6295ROM, 11, 1); if (nRet != 0) return 1;
	
	BurnSetRefreshRate(58.0);
	
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68kRom       , 0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(DrvSpriteRam    , 0x440000, 0x4403ff, MAP_RAM);
	SekMapMemory(DrvVideo2Ram    , 0x500000, 0x500fff, MAP_RAM);
	SekMapMemory(DrvVideo1Ram    , 0x502000, 0x503fff, MAP_RAM);
	SekMapMemory(DrvBgVideoRam   , 0x600000, 0x67ffff, MAP_RAM);
	SekMapMemory(DrvPaletteRam   , 0x780000, 0x7807ff, MAP_READ);
	SekMapMemory(Drv68kRam       , 0xff0000, 0xffffff, MAP_RAM);
	SekSetReadByteHandler(0, DrvReadByte);
	SekSetReadWordHandler(0, DrvReadWord);
	SekSetWriteByteHandler(0, DrvWriteByte);
	SekSetWriteWordHandler(0, DrvWriteWord);
	SekClose();
	
	pic16c5xInit(0, 0x16C57, DrvPicRom);
	pic16c5xSetReadPortHandler(PlaymarkSoundReadPort);
	pic16c5xSetWritePortHandler(PlaymarkSoundWritePort);
	
	MSM6295Init(0, 1000000 / 132, 0);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);
	
	DrawFunction = DrvRender;
	nIRQLine = 2;
	
	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 ExcelsrTilePlaneOffsets[4]   = { 0xc00000, 0x800000, 0x400000, 0 };

static INT32 ExcelsrInit()
{
	INT32 nRet = 0, nLen;
	
	Drv68kRomSize = 0x300000;
	DrvMSM6295RomSize = 0x0c0000;
	DrvNumTiles = 0x4000;
	DrvTileSize = 16 * 16;
	DrvNumChars = 0x4000;
	DrvCharSize = 16 * 16;
	DrvNumSprites = 0x4000;
	DrvSpriteSize = 16 * 16;
	
	Mem = NULL;
	MemIndex();
	nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(Mem, 0, nLen);
	MemIndex();

	DrvTempGfx = (UINT8*)BurnMalloc(0x200000);
	
	nRet = BurnLoadRom(Drv68kRom  + 0x000000, 0, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68kRom  + 0x000001, 1, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68kRom  + 0x100000, 2, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68kRom  + 0x100001, 3, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68kRom  + 0x200000, 4, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68kRom  + 0x200001, 5, 2); if (nRet != 0) return 1;
	
	if (BurnLoadPicROM(DrvPicRom, 6, 0x2d4c)) return 1;
	
	nRet = BurnLoadRom(DrvTempGfx + 0x000000, 7, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempGfx + 0x080000, 8, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempGfx + 0x100000, 9, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempGfx + 0x180000, 10, 1); if (nRet != 0) return 1;
	GfxDecode(DrvNumTiles, 4, 16, 16, ExcelsrTilePlaneOffsets, DrvTileXOffsets, DrvTileYOffsets, 0x100, DrvTempGfx, DrvTiles);
	GfxDecode(DrvNumChars, 4, 16, 16, ExcelsrTilePlaneOffsets, DrvTileXOffsets, DrvTileYOffsets, 0x100, DrvTempGfx, DrvChars);

	memset(DrvTempGfx, 0, 0x200000);
	nRet = BurnLoadRom(DrvTempGfx + 0x000000, 11, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempGfx + 0x080000, 12, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempGfx + 0x100000, 13, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempGfx + 0x180000, 14, 1); if (nRet != 0) return 1;
	GfxDecode(DrvNumSprites, 4, 16, 16, ExcelsrTilePlaneOffsets, DrvTileXOffsets, DrvTileYOffsets, 0x100, DrvTempGfx, DrvSprites);
	
	nRet = BurnLoadRom(DrvTempGfx, 15, 1); if (nRet != 0) return 1;
	memcpy(DrvMSM6295Src + 0x000000, DrvTempGfx + 0x000000, 0x20000);
	memcpy(DrvMSM6295Src + 0x020000, DrvTempGfx + 0x020000, 0x20000);
	memcpy(DrvMSM6295Src + 0x040000, DrvTempGfx + 0x000000, 0x20000);
	memcpy(DrvMSM6295Src + 0x060000, DrvTempGfx + 0x040000, 0x20000);
	memcpy(DrvMSM6295Src + 0x080000, DrvTempGfx + 0x000000, 0x20000);
	memcpy(DrvMSM6295Src + 0x0a0000, DrvTempGfx + 0x060000, 0x20000);
	BurnFree(DrvTempGfx);
	memcpy(MSM6295ROM, DrvMSM6295Src, 0x40000);
	
	BurnSetRefreshRate(58.0);
	
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68kRom       , 0x000000, 0x2fffff, MAP_ROM);
	SekMapMemory(DrvSpriteRam    , 0x440000, 0x440fff, MAP_RAM);
	SekMapMemory(DrvVideo2Ram    , 0x500000, 0x500fff, MAP_RAM);
	SekMapMemory(DrvVideo1Ram    , 0x501000, 0x501fff, MAP_RAM);
	SekMapMemory(DrvBgVideoRam   , 0x600000, 0x67ffff, MAP_RAM);
	SekMapMemory(DrvPaletteRam   , 0x780000, 0x7807ff, MAP_READ);
	SekMapMemory(Drv68kRam       , 0xff0000, 0xffffff, MAP_RAM);
	SekSetReadByteHandler(0, ExcelsrReadByte);
	SekSetReadWordHandler(0, ExcelsrReadWord);
	SekSetWriteByteHandler(0, ExcelsrWriteByte);
	SekSetWriteWordHandler(0, ExcelsrWriteWord);
	SekClose();
	
	pic16c5xInit(0, 0x16C57, DrvPicRom);
	pic16c5xSetReadPortHandler(PlaymarkSoundReadPort);
	pic16c5xSetWritePortHandler(PlaymarkSoundWritePort);
	
	MSM6295Init(0, 1000000 / 132, 0);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);
	
	DrawFunction = ExcelsrRender;
	nIRQLine = 2;
	
	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 HotmindTilePlaneOffsets[4]   = { 0x800008, 0x800000, 8, 0 };
static INT32 HotmindTileXOffsets[16]      = { 0, 1, 2, 3, 4, 5, 6, 7, 256, 257, 258, 259, 260, 261, 262, 263 };
static INT32 HotmindTileYOffsets[16]      = { 0, 16, 32, 48, 64, 80, 96, 112, 128, 144, 160, 176, 192, 208, 224, 240 };
static INT32 HotmindCharXOffsets[8]       = { 0, 1, 2, 3, 4, 5, 6, 7 };
static INT32 HotmindCharYOffsets[8]       = { 0, 16, 32, 48, 64, 80, 96, 112 };
static INT32 HotmindSpritePlaneOffsets[4] = { 0x200008, 0x200000, 8, 0 };

static const eeprom_interface hotmind_eeprom_intf =
{
	6,              /* address bits */
	16,             /* data bits */
	"*110",         /*  read command */
	"*101",         /* write command */
	0,              /* erase command */
	"*10000xxxx",   /* lock command */
	"*10011xxxx",   /* unlock command */
	0,              /* enable_multi_read */
	5               /* reset_delay (otherwise wbeachvl will hang when saving settings) */
};

static INT32 HotmindInit()
{
	INT32 nRet = 0, nLen;
	
	Drv68kRomSize = 0x100000;
	DrvMSM6295RomSize = 0;
	DrvNumTiles = 0x4000;
	DrvTileSize = 16 * 16;
	DrvNumChars = 0x10000;
	DrvCharSize = 8 * 8;
	DrvNumSprites = 0x1000;
	DrvSpriteSize = 16 * 16;
	
	Mem = NULL;
	MemIndex();
	nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(Mem, 0, nLen);
	MemIndex();

	DrvTempGfx = (UINT8*)BurnMalloc(0x200000);
	
	nRet = BurnLoadRom(Drv68kRom  + 0x00001, 0, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68kRom  + 0x00000, 1, 2); if (nRet != 0) return 1;
	
	if (BurnLoadPicROM(DrvPicRom, 2, 0x2d4c)) return 1;
	
	nRet = BurnLoadRom(DrvTempGfx + 0x000000, 3, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempGfx + 0x000001, 4, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempGfx + 0x100000, 5, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempGfx + 0x100001, 6, 2); if (nRet != 0) return 1;
	memcpy(DrvTempGfx + 0x080000, DrvTempGfx + 0x020000, 0x20000);
	memset(DrvTempGfx + 0x020000, 0x00, 0x20000);
	memcpy(DrvTempGfx + 0x180000, DrvTempGfx + 0x120000, 0x20000);
	memset(DrvTempGfx + 0x120000, 0x00, 0x20000);
	GfxDecode(DrvNumTiles, 4, 16, 16, HotmindTilePlaneOffsets, HotmindTileXOffsets, HotmindTileYOffsets, 0x200, DrvTempGfx, DrvTiles);
	GfxDecode(DrvNumChars, 4, 8, 8, HotmindTilePlaneOffsets, HotmindCharXOffsets, HotmindCharYOffsets, 0x80, DrvTempGfx, DrvChars);

	memset(DrvTempGfx, 0, 0x200000);
	nRet = BurnLoadRom(DrvTempGfx + 0x00000, 7, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempGfx + 0x00001, 8, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempGfx + 0x40000, 9, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempGfx + 0x40001, 10, 2); if (nRet != 0) return 1;
	GfxDecode(DrvNumSprites, 4, 16, 16, HotmindSpritePlaneOffsets, HotmindTileXOffsets, HotmindTileYOffsets, 0x200, DrvTempGfx, DrvSprites);
	BurnFree(DrvTempGfx);

	nRet = BurnLoadRom(MSM6295ROM, 11, 1); if (nRet != 0) return 1;
	
	BurnSetRefreshRate(58.0);
	
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68kRom       , 0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(DrvBgVideoRam   , 0x100000, 0x103fff, MAP_RAM);
	SekMapMemory(DrvVideo2Ram    , 0x104000, 0x107fff, MAP_RAM);
	SekMapMemory(DrvVideo1Ram    , 0x108000, 0x10ffff, MAP_RAM);
	SekMapMemory(DrvSpriteRam    , 0x200000, 0x200fff, MAP_RAM);
	SekMapMemory(DrvPaletteRam   , 0x280000, 0x2807ff, MAP_READ);
	SekMapMemory(Drv68kRam       , 0xff0000, 0xffffff, MAP_RAM);
	SekSetReadByteHandler(0, HotmindReadByte);
	SekSetReadWordHandler(0, HotmindReadWord);
	SekSetWriteByteHandler(0, HotmindWriteByte);
	SekSetWriteWordHandler(0, HotmindWriteWord);
	SekClose();
	
	pic16c5xInit(0, 0x16C57, DrvPicRom);
	pic16c5xSetReadPortHandler(PlaymarkSoundReadPort);
	pic16c5xSetWritePortHandler(PlaymarkSoundWritePort);
	
	MSM6295Init(0, 1000000 / 132, 0);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);
	
	EEPROMInit(&hotmind_eeprom_intf);
	
	DrawFunction = HotmindRender;
	nIRQLine = 6;
	DrvEEPROMInUse = 1;
	
	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	SekExit();
	pic16c5xExit();
	MSM6295Exit(0);
	
	GenericTilesExit();
	
	BurnFree(Mem);
	
	DrvFgScrollX = 0;
	DrvFgScrollY = 0;
	DrvCharScrollX = 0;
	DrvCharScrollY = 0;
	DrvBgEnable = 0;
	DrvBgFullSize = 0;
	DrvBgScrollX = 0;
	DrvBgScrollY = 0;
	DrvScreenEnable = 0;
	DrvVBlank = 0;
	DrvEEPROMInUse = 0;
	
	DrvSoundCommand = 0;
	DrvSoundFlag = 0;
	DrvOkiControl = 0;
	DrvOkiCommand = 0;
	DrvOldOkiBank = 0;
	DrvOkiBank = 0;
	
	Drv68kRomSize = 0;
	DrvMSM6295RomSize = 0;
	DrvNumTiles = 0;
	DrvTileSize = 0;
	DrvNumChars = 0;
	DrvCharSize = 0;
	DrvNumSprites = 0;
	DrvSpriteSize = 0;
	
	nIRQLine = 2;
	
	DrawFunction = NULL;
	
	return 0;
}

static void DrvRenderFgLayer()
{
	INT32 mx, my, Code, Colour, x, y, TileIndex = 0;
	INT32 ClipHeight = nScreenHeight - 16;
	
	UINT16 *VideoRam = (UINT16*)DrvVideo2Ram;
	
	for (my = 0; my < 32; my++) {
		for (mx = 0; mx < 32; mx++) {
			Code = BURN_ENDIAN_SWAP_INT16(VideoRam[(TileIndex * 2) + 0]);
			Colour = BURN_ENDIAN_SWAP_INT16(VideoRam[(TileIndex * 2) + 1]);
			
			x = 16 * mx;
			y = 16 * my;
			
			x -= DrvFgScrollX;
			y -= DrvFgScrollY;
			if (x < -16) x += 512;
			if (y < -16) y += 512;
			
			y -= 16;
			
			if (x > 16 && x < 304 && y > 16 && y < ClipHeight) {
				Render16x16Tile(pTransDraw, Code, x, y, Colour, 4, 0, DrvTiles);
			} else {
				Render16x16Tile_Clip(pTransDraw, Code, x, y, Colour, 4, 0, DrvTiles);
			}
			
			TileIndex++;
		}
	}
}

static void HotmindRenderTileLayer(INT32 Layer)
{
	INT32 mx, my, Code, Colour, x, y, TileIndex = 0;
	INT32 ClipHeight = nScreenHeight - 16;
	
	UINT16 *VideoRam = (UINT16*)DrvBgVideoRam;
	if (Layer == 1) VideoRam = (UINT16*)DrvVideo2Ram;
	
	for (my = 0; my < 32; my++) {
		for (mx = 0; mx < 32; mx++) {
			Code = BURN_ENDIAN_SWAP_INT16(VideoRam[TileIndex]);
			Colour = Code & 0xe000;
			Code &= 0x1fff;
			Colour >>= 13;
			
			if (Layer == 1) {
				Code += 0x2000;
				Colour += 8;
			}
			
			x = 16 * mx;
			y = 16 * my;
			
			x -= DrvFgScrollX;
			y -= DrvFgScrollY;
			if (x < -16) x += 512;
			if (y < -16) y += 512;
			y -= 16;
			
			if (Layer == 0) {
				if (x > 16 && x < 304 && y > 16 && y < ClipHeight) {
					Render16x16Tile(pTransDraw, Code, x, y, Colour, 4, 0, DrvTiles);
				} else {
					Render16x16Tile_Clip(pTransDraw, Code, x, y, Colour, 4, 0, DrvTiles);
				}
			}
			
			if (Layer == 1) {
				if (x > 16 && x < 304 && y > 16 && y < ClipHeight) {
					Render16x16Tile_Mask(pTransDraw, Code, x, y, Colour, 4, 0, 0, DrvTiles);
				} else {
					Render16x16Tile_Mask_Clip(pTransDraw, Code, x, y, Colour, 4, 0, 0, DrvTiles);
				}
			}
			
			TileIndex++;
		}
	}
}

static void DrvRenderCharLayer(INT32 Columns, INT32 CharSize)
{
	INT32 mx, my, Code, Colour, x, y, TileIndex = 0;
	INT32 ClipHeight = nScreenHeight - CharSize;
	
	UINT16 *VideoRam = (UINT16*)DrvVideo1Ram;
	
	for (my = 0; my < 32; my++) {
		for (mx = 0; mx < Columns; mx++) {
			Code = BURN_ENDIAN_SWAP_INT16(VideoRam[(TileIndex * 2) + 0]);
			Colour = BURN_ENDIAN_SWAP_INT16(VideoRam[(TileIndex * 2) + 1]);
			
			x = CharSize * mx;
			y = CharSize * my;
			
			x -= DrvCharScrollX;
			y -= DrvCharScrollY;
			if (x < -8) x += (CharSize * Columns);
			if (y < -8) y += 256;
			
			y -= 16;

			if (CharSize == 8) {
				if (x > 8 && x < 312 && y > 8 && y < ClipHeight) {
					Render8x8Tile_Mask(pTransDraw, Code, x, y, Colour, 4, 0, 0x80, DrvChars);
				} else {
					Render8x8Tile_Mask_Clip(pTransDraw, Code, x, y, Colour, 4, 0, 0x80, DrvChars);
				}
			}
			
			if (CharSize == 16) {
				if (x > 16 && x < 304 && y > 16 && y < ClipHeight) {
					Render16x16Tile_Mask(pTransDraw, Code, x, y, Colour, 4, 0, 0x80, DrvChars);
				} else {
					Render16x16Tile_Mask_Clip(pTransDraw, Code, x, y, Colour, 4, 0, 0x80, DrvChars);
				}
			}
			
			TileIndex++;
		}
	}
}

static void HotmindRenderCharLayer()
{
	INT32 mx, my, Code, Colour, x, y, TileIndex = 0;
	INT32 ClipHeight = nScreenHeight - 8;
	
	UINT16 *VideoRam = (UINT16*)DrvVideo1Ram;
	
	for (my = 0; my < 64; my++) {
		for (mx = 0; mx < 64; mx++) {
			Code = BURN_ENDIAN_SWAP_INT16(VideoRam[TileIndex]);
			Colour = Code & 0xe000;
			Code &= 0x3fff;
			Colour >>= 13;
			Code += 0x9800;
			
			x = mx * 64;
			y = my * 64;
			
			x -= DrvCharScrollX;
			y -= DrvCharScrollY;
			if (x < -8) x += 512;
			if (y < -8) y += 512;
			
			y -= 16;

			if (x > 8 && x < 312 && y > 8 && y < ClipHeight) {
				Render8x8Tile_Mask(pTransDraw, Code, x, y, Colour, 4, 0, 0x100, DrvChars);
			} else {
				Render8x8Tile_Mask_Clip(pTransDraw, Code, x, y, Colour, 4, 0, 0x100, DrvChars);
			}
			
			TileIndex++;
		}
	}
}

static void DrvRenderSprites(INT32 CodeShift, INT32 SpriteRamSize, INT32 SpriteSize, INT32 PriDraw, INT32 xOffset, INT32 yOffset)
{
	INT32 Offs;
	INT32 ColourDiv = 0x10 / 16;
	UINT16 *SpriteRam = (UINT16*)DrvSpriteRam;
	INT32 ClipHeight = nScreenHeight - SpriteSize;

	for (Offs = 0; Offs < SpriteRamSize; Offs += 4) {
		INT32 sx, sy, Code, Colour, xFlip, Pri;
		
		sy = SpriteRam[Offs + 3 - 4];
		if (sy == 0x2000) break;
		
		Pri = (SpriteRam[Offs + 1] & 0x8000) >> 15;
		Colour = ((SpriteRam[Offs + 1] & 0x3e00) >> 9) / ColourDiv;
		if(!Pri && (Colour & 0x0c) == 0x0c)	Pri = 2;
		
		if (Pri == PriDraw || PriDraw == -1) {
			xFlip = sy & 0x4000;
			sx = (SpriteRam[Offs + 1] & 0x01ff) - 16 - 7;
			sy = (256 - 8 - SpriteSize - sy) & 0xff;
			Code = SpriteRam[Offs + 2] >> CodeShift;
			Code &= (DrvNumSprites - 1);
			
			sy -= 16;
			sx += xOffset;
			sy += yOffset;
			
			if (SpriteSize == 16) {
				if (sx > 16 && sx < 304 && sy > 16 && sy < ClipHeight) {
					if (xFlip) {
						Render16x16Tile_Mask_FlipX(pTransDraw, Code, sx, sy, Colour, 4, 0, 0x200, DrvSprites);
					} else {
						Render16x16Tile_Mask(pTransDraw, Code, sx, sy, Colour, 4, 0, 0x200, DrvSprites);
					}
				} else {
					if (xFlip) {
						Render16x16Tile_Mask_FlipX_Clip(pTransDraw, Code, sx, sy, Colour, 4, 0, 0x200, DrvSprites);
					} else {
						Render16x16Tile_Mask_Clip(pTransDraw, Code, sx, sy, Colour, 4, 0, 0x200, DrvSprites);
					}
				}
			}
			
			if (SpriteSize == 32) {
				if (sx > 32 && sx < 288 && sy > 32 && sy < ClipHeight) {
					if (xFlip) {
						Render32x32Tile_Mask_FlipX(pTransDraw, Code, sx, sy, Colour, 4, 0, 0x200, DrvSprites);
					} else {
						Render32x32Tile_Mask(pTransDraw, Code, sx, sy, Colour, 4, 0, 0x200, DrvSprites);
					}
				} else {
					if (xFlip) {
						Render32x32Tile_Mask_FlipX_Clip(pTransDraw, Code, sx, sy, Colour, 4, 0, 0x200, DrvSprites);
					} else {
						Render32x32Tile_Mask_Clip(pTransDraw, Code, sx, sy, Colour, 4, 0, 0x200, DrvSprites);
					}
				}
			}
		}
	}
}

static void DrvRenderBitmap()
{
	INT32 Colour;
	UINT16 *VideoRam = (UINT16*)DrvBgVideoRam;
	INT32 Count = 0;
	
	for (INT32 y = 0; y < 512; y++) {
		for (INT32 x = 0; x < 512; x++) {
			Colour = VideoRam[Count] & 0xff;

			if (Colour) {
				if (DrvBgFullSize) {
					INT32 yPlot = (y - 16 + DrvBgScrollY) & 0x1ff;
					INT32 xPlot = (x + DrvBgScrollX) & 0x1ff;
					
					if (xPlot >= 0 && xPlot < 320 && yPlot >= 0 && yPlot < nScreenHeight) {
						pTransDraw[(yPlot * nScreenWidth) + xPlot] = 0x100 + Colour;
					}
				} else {
					// 50% size
					if(!(x % 2) && !(y % 2)) {
						// untested for now
						INT32 yPlot = ((y / 2) - 16 + DrvBgScrollY) & 0x1ff;
						INT32 xPlot = ((x / 2) + DrvBgScrollX) & 0x1ff;
					
						if (xPlot >= 0 && xPlot < 320 && yPlot >= 0 && yPlot < nScreenHeight) {
							pTransDraw[(yPlot * nScreenWidth) + xPlot] = 0x100 + Colour;
						}
					}
				}
			}

			Count++;
		}
	}
}

static void DrvRender()
{
	BurnTransferClear();
	DrvRenderFgLayer();
	if (DrvBgEnable) DrvRenderBitmap();
	DrvRenderSprites(4, 0x400, 32, -1, 0, 0);
	DrvRenderCharLayer(64, 8);
	BurnTransferCopy(DrvPalette);
}

static void ExcelsrRender()
{
	BurnTransferClear();
	DrvRenderFgLayer();
	DrvRenderSprites(2, 0xd00, 16, 2, 0, 0);
	if (DrvBgEnable) DrvRenderBitmap();
	DrvRenderSprites(2, 0xd00, 16, 1, 0, 0);
	DrvRenderCharLayer(32, 16);
	DrvRenderSprites(2, 0xd00, 16, 0, 0, 0);
	BurnTransferCopy(DrvPalette);
}

static void HotmindRender()
{
	BurnTransferClear();
	if (DrvScreenEnable) {
		HotmindRenderTileLayer(0);
		HotmindRenderTileLayer(1);
		DrvRenderSprites(2, 0x1000, 16, 2, -9, -8);
		DrvRenderSprites(2, 0x1000, 16, 1, -9, -8);
		DrvRenderSprites(2, 0x1000, 16, 0, -9, -8);
		HotmindRenderCharLayer();
	}
	BurnTransferCopy(DrvPalette);
}

static INT32 DrvFrame()
{
	INT32 nInterleave = 100;
	INT32 nSoundBufferPos = 0;
	DrvVBlank = 0;

	if (DrvReset) DrvDoReset();

	DrvMakeInputs();

	nCyclesTotal[0] = (INT32)((double)12000000 / 58.0);
	nCyclesTotal[1] = (INT32)((double)3000000 / 58.0);
	nCyclesDone[0] = nCyclesDone[1] = 0;

	SekNewFrame();

	SekOpen(0);
	for (INT32 i = 0; i < nInterleave; i++) {
		INT32 nCurrentCPU, nNext;

		nCurrentCPU = 0;
		nNext = (i + 1) * nCyclesTotal[nCurrentCPU] / nInterleave;
		nCyclesSegment = nNext - nCyclesDone[nCurrentCPU];
		nCyclesDone[nCurrentCPU] += SekRun(nCyclesSegment);
		if (i == 90) {
			DrvVBlank = 1;
			SekSetIRQLine(nIRQLine, CPU_IRQSTATUS_AUTO);
		}
		
		nCurrentCPU = 1;
		nNext = (i + 1) * nCyclesTotal[nCurrentCPU] / nInterleave;
		nCyclesSegment = nNext - nCyclesDone[nCurrentCPU];
		nCyclesDone[nCurrentCPU] += pic16c5xRun(nCyclesSegment);
		
		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			MSM6295Render(0, pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}
	SekClose();
	
	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);

		if (nSegmentLength) {
			MSM6295Render(0, pSoundBuf, nSegmentLength);
		}
	}

	if (pBurnDraw) DrawFunction();

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin != NULL) {
		*pnMin = 0x02729;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = RamStart;
		ba.nLen	  = RamEnd - RamStart;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}
	
	return 0;
}

struct BurnDriver BurnDrvBigtwin = {
	"bigtwin", NULL, NULL, NULL, "1995",
	"Big Twin\0", NULL, "Playmark", "Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, BigtwinRomInfo, BigtwinRomName, NULL, NULL, BigtwinInputInfo, BigtwinDIPInfo,
	DrvInit, DrvExit, DrvFrame, NULL, DrvScan,
	NULL, 0x400, 320, 240, 4, 3
};

struct BurnDriver BurnDrvExcelsr = {
	"excelsr", NULL, NULL, NULL, "1996",
	"Excelsior (set 1)\0", NULL, "Playmark", "Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, ExcelsrRomInfo, ExcelsrRomName, NULL, NULL, ExcelsrInputInfo, ExcelsrDIPInfo,
	ExcelsrInit, DrvExit, DrvFrame, NULL, DrvScan,
	NULL, 0x400, 320, 240, 4, 3
};

struct BurnDriver BurnDrvExcelsra = {
	"excelsra", "excelsr", NULL, NULL, "1996",
	"Excelsior (set 2)\0", NULL, "Playmark", "Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, ExcelsraRomInfo, ExcelsraRomName, NULL, NULL, ExcelsrInputInfo, ExcelsrDIPInfo,
	ExcelsrInit, DrvExit, DrvFrame, NULL, DrvScan,
	NULL, 0x400, 320, 240, 4, 3
};

struct BurnDriver BurnDrvHotmind = {
	"hotmind", NULL, NULL, NULL, "1996",
	"Hot Mind (Hard Times hardware)\0", NULL, "Playmark", "Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, HotmindRomInfo, HotmindRomName, NULL, NULL, HotmindInputInfo, HotmindDIPInfo,
	HotmindInit, DrvExit, DrvFrame, NULL, DrvScan,
	NULL, 0x400, 320, 224, 4, 3
};
