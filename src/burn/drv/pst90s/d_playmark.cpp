// FinalBurn Neo Playmark hardware driver module
// Based on MAME driver by Nicola Salmoria, Pierpaolo Prazzoli, Quench

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "pic16c5x_intf.h"
#include "msm6295.h"
#include "eeprom.h"
#include "burn_pal.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvPicROM;
static UINT8 *DrvMSM6295ROM;
static UINT8 *DrvSpriteRAM;
static UINT8 *DrvTxVideoRAM;
static UINT8 *DrvFgVideoRAM;
static UINT8 *DrvBgVideoRAM;
static UINT8 *DrvNVRAM;
static UINT8 *DrvSprites;
static UINT8 *DrvTiles;
static UINT8 *DrvChars;

static UINT16 DrvFgScrollX = 0;
static UINT16 DrvFgScrollY = 0;
static UINT16 DrvCharScrollX = 0;
static UINT16 DrvCharScrollY = 0;
static UINT16 DrvBgScrollX = 0;
static UINT16 DrvBgScrollY = 0;
static UINT16 DrvBgEnable = 0;
static UINT16 DrvBgFullSize = 0;
static UINT16 DrvScreenEnable = 0;

static UINT8 DrvSoundCommand = 0;
static UINT8 DrvSoundFlag = 0;
static UINT8 DrvOkiControl = 0;
static UINT8 DrvOkiCommand = 0;
static UINT8 DrvOkiBank = 0;

static INT32 Drv68KROMSize = 0;
static INT32 DrvMSM6295RomSize = 0;
static INT32 DrvTileSize = 0;
static INT32 DrvCharSize = 0;
static INT32 DrvSpriteSize = 0;
static INT32 DrvEEPROMInUse = 0;
static INT32 DrvPrioMasks[3] = { 0, 0, 0 };
static INT32 nIRQLine = 2;

static INT32 is_hardtimes = 0; // hardtimes hardware

static UINT8 DrvInputPort0[8];
static UINT8 DrvInputPort1[8];
static UINT8 DrvInputPort2[8];
static UINT8 DrvInputPort3[8];
static UINT8 DrvInputPort4[8];
static UINT8 DrvDip[2];
static UINT16 DrvInput[5];
static UINT8 DrvReset;

static struct BurnInputInfo BigtwinInputList[] = {
	{"P1 Coin"           , BIT_DIGITAL  , DrvInputPort0 + 4, "p1 coin"   },
	{"P1 Start"          , BIT_DIGITAL  , DrvInputPort1 + 7, "p1 start"  },
	{"P1 Up"             , BIT_DIGITAL  , DrvInputPort1 + 0, "p1 up"     },
	{"P1 Down"           , BIT_DIGITAL  , DrvInputPort1 + 1, "p1 down"   },
	{"P1 Left"           , BIT_DIGITAL  , DrvInputPort1 + 2, "p1 left"   },
	{"P1 Right"          , BIT_DIGITAL  , DrvInputPort1 + 3, "p1 right"  },
	{"P1 Fire 1"         , BIT_DIGITAL  , DrvInputPort1 + 4, "p1 fire 1" },

	{"P2 Coin"           , BIT_DIGITAL  , DrvInputPort0 + 5, "p2 coin"   },
	{"P2 Start"          , BIT_DIGITAL  , DrvInputPort2 + 7, "p2 start"  },
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

static struct BurnInputInfo BigtwinbInputList[] = {
	{"P1 Coin"           , BIT_DIGITAL  , DrvInputPort0 + 4, "p1 coin"   },
	{"P1 Start"          , BIT_DIGITAL  , DrvInputPort1 + 7, "p1 start"  },
	{"P1 Up"             , BIT_DIGITAL  , DrvInputPort1 + 3, "p1 up"     },
	{"P1 Down"           , BIT_DIGITAL  , DrvInputPort1 + 2, "p1 down"   },
	{"P1 Left"           , BIT_DIGITAL  , DrvInputPort1 + 1, "p1 left"   },
	{"P1 Right"          , BIT_DIGITAL  , DrvInputPort1 + 0, "p1 right"  },
	{"P1 Fire 1"         , BIT_DIGITAL  , DrvInputPort1 + 4, "p1 fire 1" },

	{"P2 Coin"           , BIT_DIGITAL  , DrvInputPort0 + 5, "p2 coin"   },
	{"P2 Start"          , BIT_DIGITAL  , DrvInputPort2 + 7, "p2 start"  },
	{"P2 Up"             , BIT_DIGITAL  , DrvInputPort2 + 3, "p2 up"     },
	{"P2 Down"           , BIT_DIGITAL  , DrvInputPort2 + 2, "p2 down"   },
	{"P2 Left"           , BIT_DIGITAL  , DrvInputPort2 + 1, "p2 left"   },
	{"P2 Right"          , BIT_DIGITAL  , DrvInputPort2 + 0, "p2 right"  },
	{"P2 Fire 1"         , BIT_DIGITAL  , DrvInputPort2 + 4, "p2 fire 1" },

	{"Reset"             , BIT_DIGITAL  , &DrvReset        , "reset"     },
	{"Dip 1"             , BIT_DIPSWITCH, DrvDip + 0       , "dip"       },
	{"Dip 2"             , BIT_DIPSWITCH, DrvDip + 1       , "dip"       },
};

STDINPUTINFO(Bigtwinb)

static struct BurnInputInfo HrdtimesInputList[] = {
	{"P1 Coin",			   BIT_DIGITAL,	  DrvInputPort0 + 4, "p1 coin"	 },
	{"P1 Start",		   BIT_DIGITAL,	  DrvInputPort1 + 7, "p1 start"	 },
	{"P1 Up",			   BIT_DIGITAL,   DrvInputPort1 + 0, "p1 up"	 },
	{"P1 Down",			   BIT_DIGITAL,   DrvInputPort1 + 1, "p1 down"	 },
	{"P1 Left",			   BIT_DIGITAL,	  DrvInputPort1 + 2, "p1 left"	 },
	{"P1 Right",		   BIT_DIGITAL,	  DrvInputPort1 + 3, "p1 right"	 },
	{"P1 Button 1",		   BIT_DIGITAL,	  DrvInputPort1 + 4, "p1 fire 1" },
	{"P1 Button 2",		   BIT_DIGITAL,	  DrvInputPort1 + 5, "p1 fire 2" },
	{"P1 Button 3",		   BIT_DIGITAL,	  DrvInputPort1 + 6, "p1 fire 3" },

	{"P2 Coin",			   BIT_DIGITAL,	  DrvInputPort0 + 5, "p2 coin"	 },
	{"P2 Start",		   BIT_DIGITAL,	  DrvInputPort2 + 7, "p2 start"	 },
	{"P2 Up",			   BIT_DIGITAL,	  DrvInputPort2 + 0, "p2 up"	 },
	{"P2 Down",			   BIT_DIGITAL,	  DrvInputPort2 + 1, "p2 down"	 },
	{"P2 Left",			   BIT_DIGITAL,	  DrvInputPort2 + 2, "p2 left"	 },
	{"P2 Right",		   BIT_DIGITAL,	  DrvInputPort2 + 3, "p2 right"	 },
	{"P2 Button 1",		   BIT_DIGITAL,	  DrvInputPort2 + 4, "p2 fire 1" },
	{"P2 Button 2",		   BIT_DIGITAL,	  DrvInputPort2 + 5, "p2 fire 2" },
	{"P2 Button 3",		   BIT_DIGITAL,	  DrvInputPort2 + 6, "p2 fire 3" },

	{"Reset",			   BIT_DIGITAL,	 &DrvReset,	         "reset"	 },
	{"Dip A",			   BIT_DIPSWITCH, DrvDip + 0,	     "dip"		 },
	{"Dip B",			   BIT_DIPSWITCH, DrvDip + 1,	     "dip"		 },
};

STDINPUTINFO(Hrdtimes)

static struct BurnInputInfo ExcelsrInputList[] = {
	{"P1 Coin"           , BIT_DIGITAL  , DrvInputPort0 + 4, "p1 coin"   },
	{"P1 Start"          , BIT_DIGITAL  , DrvInputPort1 + 7, "p1 start"  },
	{"P1 Up"             , BIT_DIGITAL  , DrvInputPort1 + 0, "p1 up"     },
	{"P1 Down"           , BIT_DIGITAL  , DrvInputPort1 + 1, "p1 down"   },
	{"P1 Left"           , BIT_DIGITAL  , DrvInputPort1 + 2, "p1 left"   },
	{"P1 Right"          , BIT_DIGITAL  , DrvInputPort1 + 3, "p1 right"  },
	{"P1 Fire 1"         , BIT_DIGITAL  , DrvInputPort1 + 4, "p1 fire 1" },
	{"P1 Fire 2"         , BIT_DIGITAL  , DrvInputPort1 + 5, "p1 fire 2" },
	{"P1 Fire 3"         , BIT_DIGITAL  , DrvInputPort1 + 6, "p1 fire 3" },

	{"P2 Coin"           , BIT_DIGITAL  , DrvInputPort0 + 5, "p2 coin"   },
	{"P2 Start"          , BIT_DIGITAL  , DrvInputPort2 + 7, "p2 start"  },
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
	{"P1 Coin"           , BIT_DIGITAL  , DrvInputPort0 + 4, "p1 coin"   },
	{"P1 Start"          , BIT_DIGITAL  , DrvInputPort1 + 7, "p1 start"  },
	{"P1 Up"             , BIT_DIGITAL  , DrvInputPort1 + 0, "p1 up"     },
	{"P1 Down"           , BIT_DIGITAL  , DrvInputPort1 + 1, "p1 down"   },
	{"P1 Left"           , BIT_DIGITAL  , DrvInputPort1 + 2, "p1 left"   },
	{"P1 Right"          , BIT_DIGITAL  , DrvInputPort1 + 3, "p1 right"  },
	{"P1 Fire 1"         , BIT_DIGITAL  , DrvInputPort1 + 4, "p1 fire 1" },
	{"P1 Fire 2"         , BIT_DIGITAL  , DrvInputPort1 + 5, "p1 fire 2" },

	{"P2 Coin"           , BIT_DIGITAL  , DrvInputPort0 + 5, "p2 coin"   },
	{"P2 Start"          , BIT_DIGITAL  , DrvInputPort2 + 7, "p2 start"  },

	{"Reset"             , BIT_DIGITAL  , &DrvReset        , "reset"     },
	{"Dip 1"             , BIT_DIPSWITCH, DrvDip + 0       , "dip"       },
	{"Dip 2"             , BIT_DIPSWITCH, DrvDip + 1       , "dip"       },
};

STDINPUTINFO(Hotmind)

static struct BurnInputInfo WbeachvlInputList[] = {
	{"P1 Coin",			   BIT_DIGITAL,	  DrvInputPort0 + 0, "p1 coin"	 },
	{"P1 Start",		   BIT_DIGITAL,	  DrvInputPort1 + 7, "p1 start"	 },
	{"P1 Up",			   BIT_DIGITAL,	  DrvInputPort1 + 0, "p1 up"	 },
	{"P1 Down",			   BIT_DIGITAL,	  DrvInputPort1 + 1, "p1 down"	 },
	{"P1 Left",			   BIT_DIGITAL,	  DrvInputPort1 + 2, "p1 left"	 },
	{"P1 Right",		   BIT_DIGITAL,	  DrvInputPort1 + 3, "p1 right"	 },
	{"P1 Button 1",		   BIT_DIGITAL,	  DrvInputPort1 + 4, "p1 fire 1" },
	{"P1 Button 2",		   BIT_DIGITAL,	  DrvInputPort1 + 5, "p1 fire 2" },
	{"P1 Button 3",		   BIT_DIGITAL,	  DrvInputPort1 + 6, "p1 fire 3" },

	{"P2 Coin",			   BIT_DIGITAL,	  DrvInputPort0 + 1, "p2 coin"	 },
	{"P2 Start",		   BIT_DIGITAL,	  DrvInputPort2 + 7, "p2 start"	 },
	{"P2 Up",			   BIT_DIGITAL,	  DrvInputPort2 + 0, "p2 up"	 },
	{"P2 Down",			   BIT_DIGITAL,	  DrvInputPort2 + 1, "p2 down" 	 },
	{"P2 Left",			   BIT_DIGITAL,	  DrvInputPort2 + 2, "p2 left"	 },
	{"P2 Right",		   BIT_DIGITAL,	  DrvInputPort2 + 3, "p2 right"	 },
	{"P2 Button 1",		   BIT_DIGITAL,	  DrvInputPort2 + 4, "p2 fire 1" },
	{"P2 Button 2",		   BIT_DIGITAL,	  DrvInputPort2 + 5, "p2 fire 2" },
	{"P2 Button 3",		   BIT_DIGITAL,	  DrvInputPort2 + 6, "p2 fire 3" },

	{"P3 Coin",			   BIT_DIGITAL,	  DrvInputPort0 + 2, "p3 coin"	 },
	{"P3 Start",		   BIT_DIGITAL,	  DrvInputPort3 + 7, "p3 start"	 },
	{"P3 Up",			   BIT_DIGITAL,	  DrvInputPort3 + 0, "p3 up"	 },
	{"P3 Down",			   BIT_DIGITAL,	  DrvInputPort3 + 1, "p3 down"	 },
	{"P3 Left",			   BIT_DIGITAL,	  DrvInputPort3 + 2, "p3 left"	 },
	{"P3 Right",		   BIT_DIGITAL,	  DrvInputPort3 + 3, "p3 right"	 },
	{"P3 Button 1",		   BIT_DIGITAL,	  DrvInputPort3 + 4, "p3 fire 1" },
	{"P3 Button 2",		   BIT_DIGITAL,	  DrvInputPort3 + 5, "p3 fire 2" },
	{"P3 Button 3",		   BIT_DIGITAL,	  DrvInputPort3 + 6, "p3 fire 3" },

	{"P4 Coin",			   BIT_DIGITAL,	  DrvInputPort0 + 3, "p4 coin"	 },
	{"P4 Start",		   BIT_DIGITAL,	  DrvInputPort4 + 7, "p4 start"  },
	{"P4 Up",			   BIT_DIGITAL,	  DrvInputPort4 + 0, "p4 up"	 },
	{"P4 Down",			   BIT_DIGITAL,	  DrvInputPort4 + 1, "p4 down"	 },
	{"P4 Left",			   BIT_DIGITAL,	  DrvInputPort4 + 2, "p4 left"	 },
	{"P4 Right",		   BIT_DIGITAL,	  DrvInputPort4 + 3, "p4 right"	 },
	{"P4 Button 1",		   BIT_DIGITAL,	  DrvInputPort4 + 4, "p4 fire 1" },
	{"P4 Button 2",		   BIT_DIGITAL,	  DrvInputPort4 + 5, "p4 fire 2" },
	{"P4 Button 3",		   BIT_DIGITAL,	  DrvInputPort4 + 6, "p4 fire 3" },

	{"Reset",		       BIT_DIGITAL,	 &DrvReset,			 "reset"	 },
	{"Service",		       BIT_DIGITAL,	  DrvInputPort0 + 4, "service"	 },
	{"Dip A",		       BIT_DIPSWITCH, DrvDip + 0,		 "dip"		 },
};

STDINPUTINFO(Wbeachvl)

static struct BurnInputInfo LuckboomhInputList[] = {
	{"P1 Coin",			  BIT_DIGITAL,	 DrvInputPort0 + 4,	 "p1 coin"	 },
	{"P1 Start",		  BIT_DIGITAL,	 DrvInputPort1 + 7,	 "p1 start"	 },
	{"P1 Up",			  BIT_DIGITAL,	 DrvInputPort1 + 0,  "p1 up"	 },
	{"P1 Down",			  BIT_DIGITAL,	 DrvInputPort1 + 1,	 "p1 down"	 },
	{"P1 Left",			  BIT_DIGITAL,	 DrvInputPort1 + 2,	 "p1 left"	 },
	{"P1 Right",		  BIT_DIGITAL,	 DrvInputPort1 + 3,	 "p1 right"	 },
	{"P1 Button 1",		  BIT_DIGITAL,	 DrvInputPort1 + 4,	 "p1 fire 1" },
	{"P1 Button 2",		  BIT_DIGITAL,	 DrvInputPort1 + 5,	 "p1 fire 2" },

	{"Reset",			  BIT_DIGITAL,	&DrvReset,			 "reset"	 },
	{"Dip A",			  BIT_DIPSWITCH, DrvDip + 1,		 "dip"		 },
};

STDINPUTINFO(Luckboomh)

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

static struct BurnDIPInfo BigtwinbDIPList[] =
{
	// Default Values
	{0x0f, 0xff, 0xff, 0x4a, NULL                     },
	{0x10, 0xff, 0xff, 0xff, NULL                     },

	// Dip 1
	{0   , 0xfe, 0   , 2   , "Language"               },
	{0x0f, 0x01, 0x01, 0x00, "English"                },
	{0x0f, 0x01, 0x01, 0x01, "Italian"                },

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

STDDIPINFO(Bigtwinb)

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

static struct BurnDIPInfo HrdtimesDIPList[]=
{
	{0x13, 0xff, 0xff, 0x7f, NULL					  },
	{0x14, 0xff, 0xff, 0xff, NULL					  },

	{0   , 0xfe, 0   ,    4, "Lives"			  	  },
	{0x13, 0x01, 0x03, 0x00, "1"					  },
	{0x13, 0x01, 0x03, 0x02, "2"					  },
	{0x13, 0x01, 0x03, 0x03, "3"					  },
	{0x13, 0x01, 0x03, 0x01, "5"					  },

	{0   , 0xfe, 0   ,    4, "Bonus Life"			  },
	{0x13, 0x01, 0x0c, 0x0c, "Every 300k - 500k"	  },
	{0x13, 0x01, 0x0c, 0x08, "Every 500k - 500k"	  },
	{0x13, 0x01, 0x0c, 0x04, "Only 500k"			  },
	{0x13, 0x01, 0x0c, 0x00, "No"					  },

	{0   , 0xfe, 0   ,    4, "Difficulty"			  },
	{0x13, 0x01, 0x30, 0x20, "Easy"					  },
	{0x13, 0x01, 0x30, 0x30, "Normal"		  		  },
	{0x13, 0x01, 0x30, 0x10, "Hard"					  },
	{0x13, 0x01, 0x30, 0x00, "Very Hard"			  },

	{0   , 0xfe, 0   ,    2, "Allow Continue"		  },
	{0x13, 0x01, 0x40, 0x00, "No"					  },
	{0x13, 0x01, 0x40, 0x40, "Yes"					  },

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			  },
	{0x13, 0x01, 0x80, 0x80, "Off"					  },
	{0x13, 0x01, 0x80, 0x00, "On"					  },

	{0   , 0xfe, 0   ,    2, "Coin Slots"			  },
	{0x14, 0x01, 0x01, 0x00, "Separate"				  },
	{0x14, 0x01, 0x01, 0x01, "Common"				  },

	{0   , 0xfe, 0   ,    16, "Coinage"				  },
	{0x14, 0x01, 0x1e, 0x14, "6 Coins 1 Credits"	  },
	{0x14, 0x01, 0x1e, 0x16, "5 Coins 1 Credits"	  },
	{0x14, 0x01, 0x1e, 0x18, "4 Coins 1 Credits"	  },
	{0x14, 0x01, 0x1e, 0x1a, "3 Coins 1 Credits"	  },
	{0x14, 0x01, 0x1e, 0x02, "8 Coins 3 Credits"	  },
	{0x14, 0x01, 0x1e, 0x1c, "2 Coins 1 Credits"	  },
	{0x14, 0x01, 0x1e, 0x04, "5 Coins 3 Credits"	  },
	{0x14, 0x01, 0x1e, 0x06, "3 Coins 2 Credits"	  },
	{0x14, 0x01, 0x1e, 0x1e, "1 Coin  1 Credits"	  },
	{0x14, 0x01, 0x1e, 0x08, "2 Coins 3 Credits"	  },
	{0x14, 0x01, 0x1e, 0x12, "1 Coin  2 Credits"	  },
	{0x14, 0x01, 0x1e, 0x10, "1 Coin  3 Credits"	  },
	{0x14, 0x01, 0x1e, 0x0e, "1 Coin  4 Credits"	  },
	{0x14, 0x01, 0x1e, 0x0c, "1 Coin  5 Credits"	  },
	{0x14, 0x01, 0x1e, 0x0a, "1 Coin  6 Credits"	  },
	{0x14, 0x01, 0x1e, 0x00, "Free Play"			  },

	{0   , 0xfe, 0   ,    4, "Coin A"				  },
	{0x14, 0x01, 0x06, 0x00, "5 Coins 1 Credits"	  },
	{0x14, 0x01, 0x06, 0x02, "3 Coins 1 Credits"	  },
	{0x14, 0x01, 0x06, 0x04, "2 Coins 1 Credits"	  },
	{0x14, 0x01, 0x06, 0x06, "1 Coin  1 Credits"	  },

	{0   , 0xfe, 0   ,    4, "Coin B"				  },
	{0x14, 0x01, 0x18, 0x18, "1 Coin  2 Credits"	  },
	{0x14, 0x01, 0x18, 0x10, "1 Coin  3 Credits"	  },
	{0x14, 0x01, 0x18, 0x08, "1 Coin  5 Credits"	  },
	{0x14, 0x01, 0x18, 0x00, "1 Coin  6 Credits"	  },

	{0   , 0xfe, 0   ,    2, "Credits to Start"		  },
	{0x14, 0x01, 0x20, 0x20, "1"					  },
	{0x14, 0x01, 0x20, 0x00, "2"					  },

	{0   , 0xfe, 0   ,    2, "1 Life If Continue"	  },
	{0x14, 0x01, 0x40, 0x40, "No"					  },
	{0x14, 0x01, 0x40, 0x00, "Yes"					  },

	{0   , 0xfe, 0   ,    2, "Service Mode"			  },
	{0x14, 0x01, 0x80, 0x80, "Off"					  },
	{0x14, 0x01, 0x80, 0x00, "On"					  },
};

STDDIPINFO(Hrdtimes)

static struct BurnDIPInfo WbeachvlDIPList[]=
{
	{0x26, 0xff, 0xff, 0xff, NULL					  },

	{0   , 0xfe, 0   ,    2, "Service Mode"			  },
	{0x26, 0x01, 0x20, 0x20, "Off"					  },
	{0x26, 0x01, 0x20, 0x00, "On"					  },
};

STDDIPINFO(Wbeachvl)

static struct BurnDIPInfo LuckboomhDIPList[]=
{
	{0x09, 0xff, 0xff, 0xff, NULL					 },

	{0   , 0xfe, 0   ,    1, "Service Mode"			 },
	{0x09, 0x01, 0x01, 0x01, "Off"					 },
	{0x09, 0x01, 0x01, 0x00, "On"					 },

};

STDDIPINFO(Luckboomh)

static void msm6295_set_bank(INT32 bank)
{
	INT32 bankmask = (DrvMSM6295RomSize / 0x20000) - 1;
	DrvOkiBank = (bank & 0x07) & bankmask;

	MSM6295SetBank(0, DrvMSM6295ROM + (0x20000 * DrvOkiBank), 0x20000, 0x3ffff);
}

static inline void CalcCol(UINT16 Offset, UINT16 nColour)
{
	INT32 r = (nColour >> 11) & 0x1f;
	INT32 g = (nColour >>  6) & 0x1f;
	INT32 b = (nColour >>  1) & 0x1f;

	BurnPalette[Offset] = BurnHighCol(pal5bit(r), pal5bit(g), pal5bit(b), 0);
}

static UINT8 __fastcall DrvReadByte(UINT32 a)
{
	switch (a)
	{
		case 0x700013:
			return DrvInput[1];

		case 0x700015:
			return DrvInput[2];

		default:
			bprintf(PRINT_NORMAL, _T("Read byte -> %06X\n"), a);
	}

	return 0;
}

static void __fastcall DrvWriteByte(UINT32 a, UINT8 d)
{
	switch (a)
	{
		case 0x700016:
		return;	// coin control

		case 0x70001f: {
			DrvSoundCommand = d;
			DrvSoundFlag = 1;
			return;
		}

		default:
			bprintf(PRINT_NORMAL, _T("Write byte -> %06X, %02X\n"), a, d);
	}
}

static UINT16 __fastcall DrvReadWord(UINT32 a)
{
	switch (a)
	{
		case 0x700010:
			return DrvInput[0];

		case 0x700012:
			return DrvInput[1];

		case 0x700014:
			return DrvInput[2];

		case 0x70001a:
			return 0xff00 | DrvDip[0];

		case 0x70001c:
			return 0xff00 | DrvDip[1];

		default:
			bprintf(PRINT_NORMAL, _T("Read Word -> %06X\n"), a);
	}

	return 0;
}

static void __fastcall DrvWriteWord(UINT32 a, UINT16 d)
{
	if (a >= 0x501000 && a <= 0x50ffff) {
		// catch any unused ram
		return;
	}

	if (a >= 0x680000 && a <= 0x680fff) {
		// unused ram???
		return;
	}

	if ((a & 0xfff800) == 0x780000) {
		UINT16 *PalRam = (UINT16*)BurnPalRAM;
		PalRam[(a & 0x7ff) >> 1] = d;
		BurnPaletteWrite_RRRRGGGGBBBBRGBx(a & 0x7fe);
		return;
	}

	switch (a) {
		case 0x304000: // irq ack???
		return;

		case 0x510000:
			DrvCharScrollX = (d + 2) & 0x1ff;
		return;

		case 0x510002:
			DrvCharScrollY = d & 0xff;
		return;

		case 0x510004:
			DrvBgScrollX = -(d + 4);
		return;

		case 0x510006: {
			DrvBgScrollY = -d & 0x1ff;
			DrvBgEnable = d & 0x200;
			DrvBgFullSize = d & 0x400;
			return;
		}

		case 0x510008:
			DrvFgScrollX = (d + 6) & 0x1ff;
			return;

		case 0x51000a:
			DrvFgScrollY = d & 0x1ff;
			return;

		case 0x51000c: // nop???
			return;

		case 0xe00000: // ???
			return;

		default:
			bprintf(PRINT_NORMAL, _T("Write word -> %06X, %04X\n"), a, d);
	}
}

static void __fastcall ExcelsrWriteWord(UINT32 a, UINT16 d)
{
	switch (a)
	{
		case 0x510004:
			DrvBgScrollX = -d;
		return;

		case 0x510006: {
			DrvBgScrollY = (-d + 2) & 0x1ff;
			DrvBgEnable = d & 0x200;
			DrvBgFullSize = d & 0x400;
			return;
		}
	}

	DrvWriteWord(a, d);
}

static UINT8 __fastcall HotmindReadByte(UINT32 a)
{
	switch (a)
	{
		case 0x300011:
			return DrvInput[0];

		case 0x300013:
			return DrvInput[1];

		case 0x300015:
			return (DrvInput[2] ^ 0x40) ^ (DrvEEPROMInUse ? (EEPROMRead() ? 0x40 : 0x00) : 0x40);

		case 0x30001b:
			return DrvDip[0];

		case 0x30001d:
			return DrvDip[1];

		default:
			bprintf(PRINT_NORMAL, _T("Read byte -> %06X\n"), a);
	}

	return 0;
}

static void __fastcall HotmindWriteByte(UINT32 a, UINT8 d)
{
	switch (a)
	{
		case 0x300015:
			if (DrvEEPROMInUse) {
				EEPROMSetCSLine((d & 0x01) ? EEPROM_CLEAR_LINE : EEPROM_ASSERT_LINE);
				EEPROMWriteBit(d & 0x04);
				EEPROMSetClockLine((d & 0x02) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
			}
		return;

		case 0x300017:
		return; // coin handling?

		case 0x30001f: {
			DrvSoundCommand = d;
			DrvSoundFlag = 1;
			return;
		}

		default:
			bprintf(PRINT_NORMAL, _T("Write byte -> %06X, %02X\n"), a, d);
	}
}

static UINT16 __fastcall HotmindReadWord(UINT32 a)
{
	switch (a)
	{
		case 0x300010:
			return DrvInput[0];

		default:
			bprintf(PRINT_NORMAL, _T("Read Word -> %06X\n"), a);
	}

	return 0;
}

static void __fastcall HotmindWriteWord(UINT32 a, UINT16 d)
{
	if ((a & 0xfff800) == 0x280000) {
		UINT16 *PalRam = (UINT16*)BurnPalRAM;
		PalRam[(a & 0x7ff) >> 1] = d;
		BurnPaletteWrite_RRRRGGGGBBBBRGBx(a & 0x7fe);
		return;
	}

	switch (a)
	{
		case 0x110000:
			DrvCharScrollX = d;
		return;

		case 0x110002:
			DrvCharScrollY = d;
		return;

		case 0x110004:
			DrvFgScrollX = d;
		return;

		case 0x110006:
			DrvFgScrollY = d;
		return;

		case 0x110008:
			DrvBgScrollX = d;
		return;

		case 0x11000a:
			DrvBgScrollY = d;
		return;

		case 0x11000c:
			DrvScreenEnable = d & 0x01;
		return;

		case 0x304000: // nop
		return;

		default:
			bprintf(PRINT_NORMAL, _T("Write word -> %06X, %04X\n"), a, d);
	}
}

static UINT8 __fastcall WbeachvlReadByte(UINT32 a)
{
	switch (a)
	{
		case 0x710011:
		{
			INT32 ret = DrvInput[0] ^ 0xffdf;	// service dip
			ret |= (EEPROMRead() ? 0x80 : 0x00);
			ret |= (DrvDip[0] & 0x20);
			return ret;
		}

		case 0x710013:
			return DrvInput[1];

		case 0x710015:
			return DrvInput[2];

		case 0x710019:
			return DrvInput[3];

		case 0x71001b:
			return DrvInput[4];

		default:
			bprintf(PRINT_NORMAL, _T("Read byte -> %06X\n"), a);
	}

	return 0;
}

static UINT16 __fastcall WbeachvlReadWord(UINT32 a)
{
	switch (a)
	{
		default:
			bprintf(PRINT_NORMAL, _T("Read Word -> %06X\n"), a);
	}

	return 0;
}

static void __fastcall WbeachvlWriteByte(UINT32 a, UINT8 d)
{
	switch (a)
	{
		case 0x710017:
		{
			EEPROMSetCSLine((d & 0x20) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
			EEPROMWriteBit((d & 0x80)>>7);
			EEPROMSetClockLine((d & 0x40) ? EEPROM_CLEAR_LINE : EEPROM_ASSERT_LINE);
			return;
		}

		case 0x71001f: {
			DrvSoundCommand = d;
			DrvSoundFlag = 1;
			return;
		}

		default:
			bprintf(PRINT_NORMAL, _T("Write byte -> %06X, %02X\n"), a, d);
	}
}

static void __fastcall WbeachvlWriteWord(UINT32 a, UINT16 d)
{
	if ((a & 0xfff000) == 0x780000) {
		UINT16 *PalRam = (UINT16*)BurnPalRAM;
		INT32 Offset = (a & 0xfff) >> 1;
		PalRam[Offset] = d;
		CalcCol(Offset, d);
		return;
	}

	switch (a) {
		case 0x510000:
			DrvCharScrollX = d + 2;
		return;

		case 0x510002:
			DrvCharScrollY = d;
		return;

		case 0x510004:
			DrvFgScrollX = d + 4;
		return;

		case 0x510006: {
			DrvFgScrollY = d & 0x3ff;
			DrvBgEnable = d & 0x800; // rowscroll enable
			return;
		}

		case 0x510008:
			DrvBgScrollX = d + 6;
		return;

		case 0x51000a:
			DrvBgScrollY = d;
		return;

		case 0x51000c: // nop
		return;

		default:
			bprintf(PRINT_NORMAL, _T("Write word -> %06X, %04X\n"), a, d);
	}
}

static UINT8 PlaymarkSoundReadPort(UINT16 Port)
{
	switch (Port)
	{
		case 0x0:
		case 0x10:
			return 0;

		case 0x01: {
			UINT8 Data = 0;

			if ((DrvOkiControl & 0x38) == 0x30) {
				Data = DrvSoundCommand;
			} else {
				if ((DrvOkiControl & 0x38) == 0x28) {
					Data = MSM6295Read(0) & 0x0f;
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

		default: {
			bprintf(PRINT_NORMAL, _T("Sound Read Port %x\n"), Port);
		}
	}

	return 0;
}

static void PlaymarkSoundWritePort(UINT16 Port, UINT8 Data)
{
	switch (Port & 0xff)
	{
		case 0x00:
			if (is_hardtimes == 0) {
				msm6295_set_bank(Data);
			}
		return;

		case 0x01:
			DrvOkiCommand = Data;
		return;

		case 0x02: {
			DrvOkiControl = Data;

			if (is_hardtimes) {
				msm6295_set_bank(Data & 0x03);
			} else if (DrvMSM6295RomSize) {
				msm6295_set_bank(Data & 0x07); // wbeachvl
			}

			if ((Data & 0x38) == 0x18) {
				MSM6295Write(0, DrvOkiCommand);
			}
			return;
		}

		default:
			bprintf(PRINT_NORMAL, _T("Sound Write Port %x, %x\n"), Port, Data);
	}
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	pic16c5xReset();

	MSM6295SetBank(0, DrvMSM6295ROM, 0x00000, 0x1ffff); // set static bank
	msm6295_set_bank(1);                                // default dynamic bank

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
	DrvScreenEnable = 0;

	DrvSoundCommand = 0;
	DrvSoundFlag = 0;
	DrvOkiControl = 0;
	DrvOkiCommand = 0;
	DrvOkiBank = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM            = Next; Next += Drv68KROMSize;
	DrvPicROM            = Next; Next += 0x003000;

	DrvSprites           = Next; Next += DrvSpriteSize * 2;
	DrvTiles             = Next; Next += DrvTileSize * 2;
	DrvChars             = Next; Next += DrvCharSize * 2;

	MSM6295ROM 			 = Next;
	DrvMSM6295ROM        = Next; Next += 0x100000;

	BurnPalette          = (UINT32*)Next; Next += 0x00800 * sizeof(UINT32);

	DrvNVRAM			 = Next; Next += 0x000400;

	AllRam 				 = Next;

	Drv68KRAM            = Next; Next += 0x040000;
	DrvSpriteRAM         = Next; Next += 0x001000;
	DrvTxVideoRAM        = Next; Next += 0x008000;
	DrvFgVideoRAM        = Next; Next += 0x004000;
	DrvBgVideoRAM        = Next; Next += 0x080000;
	BurnPalRAM        	 = Next; Next += 0x001000;

	RamEnd = Next;

	MemEnd = Next;

	return 0;
}

static INT32 DrvLoadRoms(bool bLoad)
{
	char* pRomName;
	struct BurnRomInfo ri;
	UINT8 *pLoad[6] = { 0, Drv68KROM, DrvPicROM, DrvTiles, DrvSprites, DrvMSM6295ROM };

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {

		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & 0xf) == 1) { // 68k code
			if (bLoad) if (BurnLoadRom(pLoad[(ri.nType & 0xf)] + 1, i+0, 2)) return 1;
			if (bLoad) if (BurnLoadRom(pLoad[(ri.nType & 0xf)] + 0, i+1, 2)) return 1;
			pLoad[(ri.nType & 0xf)] += ri.nLen * 2;
			i++;
			continue;
		}

		if ((ri.nType & 0xf) == 2) { // pic - hex & non hex format
			if (!bLoad) continue;
			bprintf (0, _T("Loading - %x\n"), ri.nLen);
			if (ri.nLen == 0x2d4c) {
				if (BurnLoadPicROM(pLoad[(ri.nType & 0xf)], i, ri.nLen)) return 1;
			} else {
				if (BurnLoadRom(pLoad[(ri.nType & 0xf)], i, 1)) return 1;
			}
			continue;
		}

		if ((ri.nType & 0xf) >= 3 && (ri.nType & 0xf) <= 5) {	// tile/char & sprite (pLoad 3 & 4) - normal load, samples
			if (bLoad) if (BurnLoadRom(pLoad[(ri.nType & 0xf)], i, 1)) return 1;
			pLoad[(ri.nType & 0xf)] += ri.nLen;
			continue;
		}

		if ((ri.nType & 0xf) >= 6 && (ri.nType & 0xf) <= 7) {	// tile/char & sprite (pLoad 3 & 4) - interleaved
			if (bLoad) if (BurnLoadRom(pLoad[(ri.nType & 0xf) - 3] + 0, i+0, 2)) return 1;
			if (bLoad) if (BurnLoadRom(pLoad[(ri.nType & 0xf) - 3] + 1, i+1, 2)) return 1;
			pLoad[(ri.nType & 0xf) - 3] += ri.nLen * 2;
			i++;
			continue;
		}
	}

	Drv68KROMSize = pLoad[1] - Drv68KROM;
	DrvMSM6295RomSize = pLoad[5] - DrvMSM6295ROM;
	bprintf(0, _T("msm6295 rom size:  %x\n"), DrvMSM6295RomSize);
	DrvTileSize = DrvCharSize = pLoad[3] - DrvTiles;
	DrvSpriteSize = pLoad[4] - DrvSprites;

	return 0;
}

static void DrvGfxDecode(UINT8 *src, UINT8 *dst, INT32 len, INT32 type, INT32 num_override = 0)
{
	INT32 DrvType1PlaneOffsets[4]     = { (len * 8 * 3) / 4, (len * 8 * 2) / 4, (len * 8 * 1) / 4, (len * 8 * 0) / 4  };
	INT32 DrvType1XOffsets[32]        = { STEP8(0,1), STEP8(128,1), STEP8(256,1), STEP8(384,1) }; // same for 8x8, 16x16, 32x32
	INT32 DrvType1YOffsets[32]        = { STEP16(0,8), STEP16(512,8) };

	INT32 DrvType2PlaneOffsets[4]     = { (len * 8) / 2 + 8, (len * 8) / 2, 8, 0 };
	INT32 DrvType2XOffsets[16]        = { STEP8(0,1), STEP8(256,1) }; // same for 8x8, 16x16, 32x32
	INT32 DrvType2YOffsets[16]        = { STEP16(0,16) };

	INT32 DrvType36PlaneOffsets[6]    = { (len * 8 * 5) / 6, (len * 8 * 4) / 6, (len * 8 * 3) / 6, (len * 8 * 2) / 6, (len * 8 * 1) / 6, (len * 8 * 0) / 6 };
	INT32 DrvType35PlaneOffsets[5]    = { (len * 8 * 4) / 6, (len * 8 * 3) / 6, (len * 8 * 2) / 6, (len * 8 * 1) / 6, (len * 8 * 0) / 6 };
	INT32 DrvType3XOffsets[16]        = { STEP8(0,1), STEP8(128,1) }; // same for 8x8, 16x16, 32x32
	INT32 DrvType3YOffsets[16]        = { STEP16(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(len * 2); // double or GfxDecode(((len * 8) / 4) / ( 8 *  8), 4,  8,  8, DrvType1PlaneOffsets, DrvType1XOffsets, DrvType1YOffsets, 0x100 <-- causes a crash

	memcpy(tmp, src, len);

	switch (type)
	{
		// bigtwin, excelsr
		case 0: GfxDecode(num_override ? num_override : (((len * 8) / 4) / ( 8 *  8)), 4,  8,  8, DrvType1PlaneOffsets, DrvType1XOffsets, DrvType1YOffsets, 0x100, tmp, dst); break;
		case 1: GfxDecode(num_override ? num_override : (((len * 8) / 4) / (16 * 16)), 4, 16, 16, DrvType1PlaneOffsets, DrvType1XOffsets, DrvType1YOffsets, 0x100, tmp, dst); break;
		case 2: GfxDecode(num_override ? num_override : (((len * 8) / 4) / (32 * 32)), 4, 32, 32, DrvType1PlaneOffsets, DrvType1XOffsets, DrvType1YOffsets, 0x400, tmp, dst); break;

		// hrdtimes, hotmind, luckboom, bigtwinb...
		case 4: GfxDecode(num_override ? num_override : (((len * 8) / 4) / ( 8 *  8)), 4,  8,  8, DrvType2PlaneOffsets, DrvType2XOffsets, DrvType2YOffsets, 0x080, tmp, dst); break;
		case 5: GfxDecode(num_override ? num_override : (((len * 8) / 4) / (16 * 16)), 4, 16, 16, DrvType2PlaneOffsets, DrvType2XOffsets, DrvType2YOffsets, 0x200, tmp, dst); break;
//		case 6: GfxDecode(num_override ? num_override : (((len * 8) / 4) / (16 * 16)), 4, 16, 16, DrvType2PlaneOffsets, DrvType2XOffsets, DrvType2YOffsets, 0x200, tmp, dst); break;

		// wbeachvl
		case 8: GfxDecode(num_override ? num_override : (((len * 8) / 6) / ( 8 *  8)), 6,  8,  8, DrvType36PlaneOffsets, DrvType3XOffsets, DrvType3YOffsets, 0x040, tmp, dst); break;
		case 9: GfxDecode(num_override ? num_override : (((len * 8) / 6) / (16 * 16)), 6, 16, 16, DrvType36PlaneOffsets, DrvType3XOffsets, DrvType3YOffsets, 0x100, tmp, dst); break;
		case 10:GfxDecode(num_override ? num_override : (((len * 8) / 5) / (16 * 16)), 5, 16, 16, DrvType35PlaneOffsets, DrvType3XOffsets, DrvType3YOffsets, 0x100, tmp, dst); break;

	}

	BurnFree(tmp);
}

static tilemap_callback( bg )
{
	UINT16 *VideoRam = (UINT16*)DrvFgVideoRAM;
	INT32 code = BURN_ENDIAN_SWAP_INT16(VideoRam[(offs * 2) + 0]);
	INT32 color = BURN_ENDIAN_SWAP_INT16(VideoRam[(offs * 2) + 1]);

	TILE_SET_INFO(2, code, color, 0);
}

static tilemap_callback( tx )
{
	UINT16 *VideoRam = (UINT16*)DrvTxVideoRAM;
	INT32 code = BURN_ENDIAN_SWAP_INT16(VideoRam[(offs * 2) + 0]);
	INT32 color = BURN_ENDIAN_SWAP_INT16(VideoRam[(offs * 2) + 1]);

	TILE_SET_INFO(1, code, color, 0);
}

static tilemap_callback( hm_bg )
{
	UINT16 *VideoRam = (UINT16*)DrvBgVideoRAM;
	INT32 attr = VideoRam[offs];

	TILE_SET_INFO(2, attr, attr >> 13, 0);
}

static tilemap_scan( hardtimes )
{
	return (col / 32) * 1024 + (col & 0x1f) + (row / 32) * 1024 + (row & 0x1f) * 32;
}

static tilemap_callback( hm_fg )
{
	UINT16 *VideoRam = (UINT16*)DrvFgVideoRAM;
	INT32 attr = VideoRam[offs];

	TILE_SET_INFO(3, attr, attr >> 13, 0);
}

static tilemap_callback( hm_tx )
{
	UINT16 *VideoRam = (UINT16*)DrvTxVideoRAM;
	INT32 attr = BURN_ENDIAN_SWAP_INT16(VideoRam[offs]);

	TILE_SET_INFO(1, attr & 0xfff, attr >> 13, 0);
}

static tilemap_callback( btb_tx )
{
	UINT16 *VideoRam = (UINT16*)DrvTxVideoRAM;
	INT32 attr = BURN_ENDIAN_SWAP_INT16(VideoRam[offs]);

	TILE_SET_INFO(1, attr & 0xfff, attr >> 12, 0);
}

static tilemap_callback( wbv_tx )
{
	UINT16 *ram = (UINT16*)DrvTxVideoRAM;
	INT32 code = ram[2 * offs];
	INT32 color = ram[2 * offs + 1];

	TILE_SET_INFO(1, code, (color >> 2), 0);
}

static tilemap_callback( wbv_fg )
{
	UINT16 *ram = (UINT16*)DrvFgVideoRAM;
	INT32 code = ram[2 * offs];
	INT32 color = ram[2 * offs + 1];

	TILE_SET_INFO(3, code & 0x7fff, color >> 2, (code & 0x8000) ? TILE_FLIPX : 0);
}

static tilemap_callback( wbv_bg )
{
	UINT16 *ram = (UINT16*)DrvBgVideoRAM;
	INT32 code = ram[2 * offs];
	INT32 color = ram[2 * offs + 1];

	TILE_SET_INFO(2, code & 0x7fff, color >> 2, (code & 0x8000) ? TILE_FLIPX : 0);
}

static INT32 BigtwinInit()
{
	BurnSetRefreshRate(58.0);

	DrvLoadRoms(false);

	BurnAllocMemIndex();

	if (DrvLoadRoms(true)) return 1;

	DrvGfxDecode(DrvTiles, DrvChars, DrvTileSize, 0);
	DrvGfxDecode(DrvTiles, DrvTiles, DrvTileSize, 1);
	DrvGfxDecode(DrvSprites, DrvSprites, DrvSpriteSize, 2);

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM       , 0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(DrvSpriteRAM    , 0x440000, 0x4403ff, MAP_RAM);
	SekMapMemory(DrvFgVideoRAM   , 0x500000, 0x500fff, MAP_RAM);
	SekMapMemory(DrvTxVideoRAM   , 0x502000, 0x503fff, MAP_RAM);
	SekMapMemory(DrvBgVideoRAM   , 0x600000, 0x67ffff, MAP_RAM);
	SekMapMemory(BurnPalRAM      , 0x780000, 0x7807ff, MAP_READ);
	SekMapMemory(Drv68KRAM       , 0xff0000, 0xffffff, MAP_RAM);
	SekSetReadByteHandler(0, DrvReadByte);
	SekSetReadWordHandler(0, DrvReadWord);
	SekSetWriteByteHandler(0, DrvWriteByte);
	SekSetWriteWordHandler(0, DrvWriteWord);
	SekClose();

	pic16c5xInit(0, 0x16C57, DrvPicROM);
	pic16c5xSetReadPortHandler(PlaymarkSoundReadPort);
	pic16c5xSetWritePortHandler(PlaymarkSoundWritePort);

	MSM6295Init(0, 1000000 / 132, 0);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);


	GenericTilesInit();
	GenericTilemapSetGfx(0, DrvSprites, 4, 32, 32, DrvSpriteSize * 2, 0x200, 0x0f);
	GenericTilemapSetGfx(1, DrvChars,   4,  8,  8, DrvCharSize * 2,   0x080, 0x07);
	GenericTilemapSetGfx(2, DrvTiles,   4, 16, 16, DrvTileSize * 2,   0x000, 0x07);
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, tx_map_callback,  8,  8, 64, 32);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);
	GenericTilemapSetTransparent(1, 0);

	nIRQLine = 2;
	DrvPrioMasks[0] = DrvPrioMasks[1] = DrvPrioMasks[2] = 0;

	DrvDoReset();

	return 0;
}

static INT32 ExcelsrInit()
{
	BurnSetRefreshRate(58.0);

	DrvLoadRoms(false);

	BurnAllocMemIndex();

	if (DrvLoadRoms(true)) return 1;

	DrvGfxDecode(DrvTiles, DrvChars, DrvTileSize, 1);
	DrvGfxDecode(DrvTiles, DrvTiles, DrvTileSize, 1);
	DrvGfxDecode(DrvSprites, DrvSprites, DrvSpriteSize, 1);

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM       , 0x000000, 0x2fffff, MAP_ROM);
	SekMapMemory(DrvSpriteRAM    , 0x440000, 0x440fff, MAP_RAM);
	SekMapMemory(DrvFgVideoRAM   , 0x500000, 0x500fff, MAP_RAM);
	SekMapMemory(DrvTxVideoRAM   , 0x501000, 0x501fff, MAP_RAM);
	SekMapMemory(DrvBgVideoRAM   , 0x600000, 0x67ffff, MAP_RAM);
	SekMapMemory(BurnPalRAM      , 0x780000, 0x7807ff, MAP_READ);
	SekMapMemory(Drv68KRAM       , 0xff0000, 0xffffff, MAP_RAM);
	SekSetReadByteHandler(0, DrvReadByte);
	SekSetReadWordHandler(0, DrvReadWord);
	SekSetWriteByteHandler(0, DrvWriteByte);
	SekSetWriteWordHandler(0, ExcelsrWriteWord);
	SekClose();

	pic16c5xInit(0, 0x16C57, DrvPicROM);
	pic16c5xSetReadPortHandler(PlaymarkSoundReadPort);
	pic16c5xSetWritePortHandler(PlaymarkSoundWritePort);

	MSM6295Init(0, 1000000 / 132, 0);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapSetGfx(0, DrvSprites, 4, 16, 16, DrvSpriteSize * 2, 0x200, 0x0f);
	GenericTilemapSetGfx(1, DrvChars,   4, 16, 16, DrvCharSize * 2,   0x080, 0x07);
	GenericTilemapSetGfx(2, DrvTiles,   4, 16, 16, DrvTileSize * 2,   0x000, 0x07);
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, tx_map_callback, 16, 16, 32, 32);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);
	GenericTilemapSetTransparent(1, 0);

	nIRQLine = 2;
	DrvPrioMasks[0] = 0;
	DrvPrioMasks[1] = 0xfffc;
	DrvPrioMasks[2] = 0xfff0;

	DrvDoReset();

	return 0;
}

static const eeprom_interface hotmind_eeprom_intf =
{
	6,              // address bits
	16,             // data bits
	"*110",         //  read command
	"*101",         // write command
	0,              // erase command
	"*10000xxxx",   // lock command
	"*10011xxxx",   // unlock command
	0,              // enable_multi_read
	5               // reset_delay (otherwise wbeachvl will hang when saving settings)
};

static INT32 HotmindInit()
{
	BurnSetRefreshRate(58.0);

	DrvLoadRoms(false);

	BurnAllocMemIndex();

	if (DrvLoadRoms(true)) return 1;

	DrvGfxDecode(DrvTiles + 0x30000, DrvChars, DrvTileSize, 4, 0x1000);
	DrvGfxDecode(DrvTiles, DrvTiles, DrvTileSize, 5);
	DrvGfxDecode(DrvSprites, DrvSprites, DrvSpriteSize, 5);

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM       , 0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(DrvBgVideoRAM   , 0x100000, 0x103fff, MAP_RAM);
	SekMapMemory(DrvFgVideoRAM   , 0x104000, 0x107fff, MAP_RAM);
	SekMapMemory(DrvTxVideoRAM   , 0x108000, 0x10ffff, MAP_RAM);
	SekMapMemory(DrvSpriteRAM    , 0x200000, 0x200fff, MAP_RAM);
	SekMapMemory(BurnPalRAM      , 0x280000, 0x2807ff, MAP_READ);
	SekMapMemory(Drv68KRAM       , 0xff0000, 0xffffff, MAP_RAM);
	SekSetReadByteHandler(0, HotmindReadByte);
	SekSetReadWordHandler(0, HotmindReadWord);
	SekSetWriteByteHandler(0, HotmindWriteByte);
	SekSetWriteWordHandler(0, HotmindWriteWord);
	SekClose();

	pic16c5xInit(0, 0x16C57, DrvPicROM);
	pic16c5xSetReadPortHandler(PlaymarkSoundReadPort);
	pic16c5xSetWritePortHandler(PlaymarkSoundWritePort);

	MSM6295Init(0, 1000000 / 132, 0);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	EEPROMInit(&hotmind_eeprom_intf);

	GenericTilesInit();
	GenericTilemapSetGfx(0, DrvSprites, 			4, 16, 16, DrvSpriteSize * 2, 0x200, 0x1f);
	GenericTilemapSetGfx(1, DrvChars,   			4,  8,  8, 0x040000,          0x100, 0x07);
	GenericTilemapSetGfx(2, DrvTiles,   			4, 16, 16, 0x080000,          0x000, 0x07);
	GenericTilemapSetGfx(3, DrvTiles + 0x080000,	4, 16, 16, 0x080000,          0x080, 0x07);
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, hm_bg_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, hm_fg_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, hm_tx_map_callback,  8,  8, 64, 64);
//	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);
	GenericTilemapSetOffsets(0, -14, -16);
	GenericTilemapSetOffsets(1, -14, -16);
	GenericTilemapSetOffsets(2, -14, -16);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetTransparent(2, 0);

	nIRQLine = 6;
	DrvEEPROMInUse = 1;
	DrvPrioMasks[0] = 0xfff0;
	DrvPrioMasks[1] = 0xfffc;
	DrvPrioMasks[2] = 0;
	is_hardtimes = 1;

	DrvDoReset();

	return 0;
}

static INT32 HrdtimesInit()
{
	BurnSetRefreshRate(58.0);

	DrvLoadRoms(false);

	BurnAllocMemIndex();

	if (DrvLoadRoms(true)) return 1;

	DrvGfxDecode(DrvTiles + 0xfc000, DrvChars, DrvTileSize, 4, 0x400);
	DrvGfxDecode(DrvTiles, DrvTiles, DrvTileSize, 5);
	DrvGfxDecode(DrvSprites, DrvSprites, DrvSpriteSize, 5);

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM       , 0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(Drv68KRAM       , 0x080000, 0x0bffff, MAP_RAM);
	SekMapMemory(DrvBgVideoRAM   , 0x100000, 0x103fff, MAP_RAM);
	SekMapMemory(DrvFgVideoRAM   , 0x104000, 0x107fff, MAP_RAM);
	SekMapMemory(DrvTxVideoRAM   , 0x108000, 0x10ffff, MAP_RAM);
	SekMapMemory(DrvSpriteRAM    , 0x200000, 0x200fff, MAP_RAM);
	SekMapMemory(BurnPalRAM      , 0x280000, 0x2807ff, MAP_READ);
	SekSetReadByteHandler(0, HotmindReadByte);
	SekSetReadWordHandler(0, HotmindReadWord);
	SekSetWriteByteHandler(0, HotmindWriteByte);
	SekSetWriteWordHandler(0, HotmindWriteWord);
	SekClose();

	pic16c5xInit(0, 0x16C57, DrvPicROM);
	pic16c5xSetReadPortHandler(PlaymarkSoundReadPort);
	pic16c5xSetWritePortHandler(PlaymarkSoundWritePort);

	MSM6295Init(0, 1000000 / 132, 0);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	EEPROMInit(&hotmind_eeprom_intf);

	GenericTilesInit();
	GenericTilemapSetGfx(0, DrvSprites, 		   4, 16, 16, DrvSpriteSize * 2, 0x200, 0x1f);
	GenericTilemapSetGfx(1, DrvChars,   		   4,  8,  8, 0x010000, 0x100, 0x07);
	GenericTilemapSetGfx(2, DrvTiles,   		   4, 16, 16, 0x200000, 0x000, 0x07);
	GenericTilemapSetGfx(3, DrvTiles + 0x200000,   4, 16, 16, 0x200000, 0x080, 0x07);
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS,  hm_bg_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(1, hardtimes_map_scan, hm_fg_map_callback, 16, 16, 128, 32);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS,  hm_tx_map_callback,  8,  8, 64, 64);
//	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);
	GenericTilemapSetOffsets(0, -12, -16);
	GenericTilemapSetOffsets(1, -10, -16);
	GenericTilemapSetOffsets(2, -14, -16);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetTransparent(2, 0);

	nIRQLine = 6;
	DrvEEPROMInUse = 1;
	DrvPrioMasks[0] = 0xfff0;
	DrvPrioMasks[1] = 0xfffc;
	DrvPrioMasks[2] = 0;
	is_hardtimes = 1;

	DrvDoReset();

	return 0;
}

static INT32 BigtwinbInit()
{
	BurnSetRefreshRate(58.0);

	DrvLoadRoms(false);

	BurnAllocMemIndex();

	if (DrvLoadRoms(true)) return 1;

	DrvGfxDecode(DrvTiles + 0x40000, DrvChars, DrvTileSize, 4, 0x1000);
	DrvGfxDecode(DrvTiles, DrvTiles, DrvTileSize, 5);
	DrvGfxDecode(DrvSprites, DrvSprites, DrvSpriteSize, 2);

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM       , 0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(DrvBgVideoRAM   , 0x100000, 0x103fff, MAP_RAM);
	SekMapMemory(DrvFgVideoRAM   , 0x104000, 0x107fff, MAP_RAM);
	SekMapMemory(DrvTxVideoRAM   , 0x108000, 0x10ffff, MAP_RAM);
	SekMapMemory(DrvSpriteRAM    , 0x201000, 0x2013ff, MAP_RAM);
	SekMapMemory(BurnPalRAM      , 0x280000, 0x2807ff, MAP_READ);
	SekMapMemory(Drv68KRAM       , 0xff0000, 0xffffff, MAP_RAM);
	SekSetReadByteHandler(0, HotmindReadByte);
	SekSetReadWordHandler(0, HotmindReadWord);
	SekSetWriteByteHandler(0, HotmindWriteByte);
	SekSetWriteWordHandler(0, HotmindWriteWord);
	SekClose();

	pic16c5xInit(0, 0x16C57, DrvPicROM);
	pic16c5xSetReadPortHandler(PlaymarkSoundReadPort);
	pic16c5xSetWritePortHandler(PlaymarkSoundWritePort);

	MSM6295Init(0, 1000000 / 132, 0);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapSetGfx(0, DrvSprites, 			4, 32, 32, DrvSpriteSize * 2, 0x300, 0x0f);
	GenericTilemapSetGfx(1, DrvChars,   			4,  8,  8, 0x040000,          0x200, 0x0f);
	GenericTilemapSetGfx(2, DrvTiles,   			4, 16, 16, 0x100000,          0x000, 0x07);
	GenericTilemapSetGfx(3, DrvTiles + 0x100000,	4, 16, 16, 0x100000,          0x080, 0x07);
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, hm_bg_map_callback,  16, 16, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, hm_fg_map_callback,  16, 16, 32, 32);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, btb_tx_map_callback,  8,  8, 64, 64);
//	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);
	GenericTilemapSetOffsets(0, -4, -16);
	GenericTilemapSetOffsets(1, -0, -16);
	GenericTilemapSetOffsets(2, -0, -16);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetTransparent(2, 0);

	nIRQLine = 2;
	DrvPrioMasks[0] = DrvPrioMasks[1] = DrvPrioMasks[2] = 0;

	DrvDoReset();

	return 0;
}

static INT32 LuckboomhInit()
{
	BurnSetRefreshRate(58.0);

	DrvLoadRoms(false);

	BurnAllocMemIndex();

	if (DrvLoadRoms(true)) return 1;

	DrvGfxDecode(DrvTiles + 0x30000, DrvChars, DrvTileSize, 4, 0x1000);
	DrvGfxDecode(DrvTiles, DrvTiles, DrvTileSize, 5);
	DrvGfxDecode(DrvSprites, DrvSprites, DrvSpriteSize, 5);

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM       , 0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(DrvBgVideoRAM   , 0x100000, 0x103fff, MAP_RAM);
	SekMapMemory(DrvFgVideoRAM   , 0x104000, 0x107fff, MAP_RAM);
	SekMapMemory(DrvTxVideoRAM   , 0x108000, 0x10ffff, MAP_RAM);
	SekMapMemory(DrvSpriteRAM    , 0x200000, 0x200fff, MAP_RAM);
	SekMapMemory(BurnPalRAM      , 0x280000, 0x2807ff, MAP_READ);
	SekMapMemory(DrvNVRAM		 , 0xff0000, 0xff03ff, MAP_RAM);
	SekMapMemory(Drv68KRAM       , 0xff8000, 0xffffff, MAP_RAM);
	SekSetReadByteHandler(0, HotmindReadByte);
	SekSetReadWordHandler(0, HotmindReadWord);
	SekSetWriteByteHandler(0, HotmindWriteByte);
	SekSetWriteWordHandler(0, HotmindWriteWord);
	SekClose();

	pic16c5xInit(0, 0x16C57, DrvPicROM);
	pic16c5xSetReadPortHandler(PlaymarkSoundReadPort);
	pic16c5xSetWritePortHandler(PlaymarkSoundWritePort);

	MSM6295Init(0, 1000000 / 132, 0);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapSetGfx(0, DrvSprites, 4, 16, 16, DrvSpriteSize * 2, 0x200, 0x1f);
	GenericTilemapSetGfx(1, DrvChars,   4,  8,  8, 0x040000,          0x100, 0x07);
	GenericTilemapSetGfx(2, DrvTiles,   4, 16, 16, DrvTileSize * 2,   0x000, 0x07);
	GenericTilemapSetGfx(3, DrvTiles,	4, 16, 16, DrvTileSize * 2,   0x080, 0x07);
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, hm_bg_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, hm_fg_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, hm_tx_map_callback,  8,  8, 64, 64);
//	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);
	GenericTilemapSetOffsets(0, -13, -16);
	GenericTilemapSetOffsets(1, -11, -16);
	GenericTilemapSetOffsets(2,  -9, -16);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetTransparent(2, 0);

	nIRQLine = 6;
	DrvPrioMasks[0] = 0xfff0;
	DrvPrioMasks[1] = 0xfffc;
	DrvPrioMasks[2] = 0;
	is_hardtimes = 1;

	DrvDoReset();

	return 0;
}

static INT32 WbeachvlInit()
{
	BurnSetRefreshRate(58.0);

	DrvLoadRoms(false);
	DrvTileSize = DrvSpriteSize = DrvCharSize = 0x600000;

	BurnAllocMemIndex();

	if (DrvLoadRoms(true)) return 1;

	DrvTileSize = DrvSpriteSize = DrvCharSize = 0x600000;

	DrvGfxDecode(DrvTiles, DrvChars,   DrvTileSize, 8);
	DrvGfxDecode(DrvTiles, DrvSprites, DrvSpriteSize, 10);
	DrvGfxDecode(DrvTiles, DrvTiles,   DrvTileSize, 9);

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM       , 0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(DrvSpriteRAM    , 0x440000, 0x440fff, MAP_RAM);
	SekMapMemory(DrvBgVideoRAM   , 0x500000, 0x501fff, MAP_RAM);
	SekMapMemory(DrvFgVideoRAM   , 0x504000, 0x505fff, MAP_RAM);
	SekMapMemory(DrvTxVideoRAM   , 0x508000, 0x509fff, MAP_RAM);
	SekMapMemory(DrvTxVideoRAM + 0x2000,	0x50f000, 0x50ffff, MAP_RAM); // scroll ram
	SekMapMemory(BurnPalRAM      , 0x780000, 0x780fff, MAP_READ);
	SekMapMemory(Drv68KRAM       , 0xff0000, 0xffffff, MAP_RAM);
	SekSetReadByteHandler(0, WbeachvlReadByte);
	SekSetReadWordHandler(0, WbeachvlReadWord);
	SekSetWriteByteHandler(0, WbeachvlWriteByte);
	SekSetWriteWordHandler(0, WbeachvlWriteWord);
	SekClose();

	pic16c5xInit(0, 0x16C57, DrvPicROM);
	pic16c5xSetReadPortHandler(PlaymarkSoundReadPort);
	pic16c5xSetWritePortHandler(PlaymarkSoundWritePort);

	EEPROMInit(&hotmind_eeprom_intf);

	MSM6295Init(0, 1000000 / 132, 0);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapSetGfx(0, DrvSprites, 5, 16, 16, DrvSpriteSize * 2, 0x600, 0x0f);
	GenericTilemapSetGfx(1, DrvChars,   6,  8,  8, DrvTileSize * 2,   0x400, 0x07);
	GenericTilemapSetGfx(2, DrvTiles,   6, 16, 16, DrvTileSize * 2,   0x000, 0x07);
	GenericTilemapSetGfx(3, DrvTiles,   6, 16, 16, DrvTileSize * 2,   0x200, 0x07);
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, wbv_bg_map_callback, 16, 16, 64, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, wbv_fg_map_callback, 16, 16, 64, 32);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, wbv_tx_map_callback,  8,  8, 64, 64);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetTransparent(2, 0);

	nIRQLine = 2;
	DrvEEPROMInUse = 1;
	DrvPrioMasks[0] = 0xfff0;
	DrvPrioMasks[1] = 0xfffc;
	DrvPrioMasks[2] = 0;

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	SekExit();
	pic16c5xExit();
	MSM6295Exit(0);
	MSM6295ROM = NULL;
	if (DrvEEPROMInUse) EEPROMExit();
	GenericTilesExit();

	BurnFreeMemIndex();

	DrvEEPROMInUse = 0;
	Drv68KROMSize = 0;
	DrvMSM6295RomSize = 0;
	DrvTileSize = 0;
	DrvCharSize = 0;
	DrvSpriteSize = 0;

	nIRQLine = 2;

	is_hardtimes = 0;

	return 0;
}

static void bigtwinb_draw_sprites(INT32 xAdjust, INT32 yAdjust)
{
	UINT16 *SpriteRam = (UINT16*)DrvSpriteRAM;
	int start_offset = 0x400 / 2 - 4;

	for (INT32 offs = 4; offs < 0x400 / 2; offs += 4)
	{
		if (SpriteRam[offs + 3 - 4] == 0x2000) // end of list marker
		{
			start_offset = offs - 4;
			break;
		}
	}

	for (INT32 offs = start_offset; offs >= 4; offs -= 4)
	{
		int sy = SpriteRam[offs + 3 - 4];   // -4? what the... ???

		int flipx = sy & 0x4000;
		int sx = (SpriteRam[offs + 1] & 0x01ff) - 16 - 7;
		sy = (256 - 8 - 16 - sy) & 0xff;
		int code = SpriteRam[offs + 2] >> 4;
		int color = ((SpriteRam[offs + 1] & 0xf000) >> 12);

		DrawGfxMaskTile(0, 0, code, sx + xAdjust, sy + yAdjust, flipx, 0, color, 0);
	}
}

static void draw_sprites(INT32 codeshift, INT32 ram_size, INT32 xAdjust, INT32 yAdjust)
{
	INT32 start_offset = ram_size / 2 - 4;
	GenericTilesGfx *pGfx = &GenericGfxData[0];
	UINT16 *SpriteRam = (UINT16*)DrvSpriteRAM;
	INT32 color_divider = (1 << pGfx->depth) / 16;

	for (INT32 offs = 4; offs < ram_size / 2; offs += 4)
	{
		if (SpriteRam[offs + 3 - 4] == 0x2000) // end of list marker
		{
			start_offset = offs - 4;
			break;
		}
	}

	for (INT32 offs = start_offset; offs >= 4; offs -= 4)
	{
		INT32 sy = SpriteRam[offs + 3 - 4];

		INT32 flipx = sy & 0x4000;
		INT32 sx = (SpriteRam[offs + 1] & 0x01ff) - 16 - 7;
		sy = (256 - 8 - pGfx->height - sy) & 0xff;
		INT32 code = SpriteRam[offs + 2] >> codeshift;
		INT32 color = ((SpriteRam[offs + 1] & 0x3e00) >> 9) / color_divider;
		INT32 pri = (SpriteRam[offs + 1] & 0x8000) >> 15;

		if(!pri && (color & 0x0c) == 0x0c)
			pri = 2;

		RenderPrioSprite(pTransDraw, pGfx->gfxbase, code % pGfx->code_mask, ((color & pGfx->color_mask) << pGfx->depth) + pGfx->color_offset, 0, sx + xAdjust, sy + yAdjust, flipx, 0, pGfx->width, pGfx->height, DrvPrioMasks[pri]);
	}
}

static void DrvRenderBitmap()
{
	UINT16 *VideoRam = (UINT16*)DrvBgVideoRAM;

	for (INT32 y = 0, Count = 0; y < 512; y++) {
		for (INT32 x = 0; x < 512; x++) {
			INT32 Colour = VideoRam[Count] & 0xff;

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

static INT32 BigtwinRender()
{
	if (BurnRecalc) {
		BurnPaletteUpdate_RRRRGGGGBBBBRGBx();
		BurnRecalc = 0;
	}

	GenericTilemapSetScrollX(0, DrvFgScrollX);
	GenericTilemapSetScrollY(0, DrvFgScrollY);
	GenericTilemapSetScrollX(1, DrvCharScrollX);
	GenericTilemapSetScrollY(1, DrvCharScrollY);

	BurnTransferClear();

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0, 0);

	if (nBurnLayer & 2 && DrvBgEnable) DrvRenderBitmap();

	if (nSpriteEnable & 1) draw_sprites(4, 0x400, 0, -16);

	if (nBurnLayer & 4) GenericTilemapDraw(1, 0, 0);

	BurnTransferCopy(BurnPalette);

	return 0;
}

static INT32 ExcelsrRender()
{
	if (BurnRecalc) {
		BurnPaletteUpdate_RRRRGGGGBBBBRGBx();
		BurnRecalc = 0;
	}

	GenericTilemapSetScrollX(0, DrvFgScrollX);
	GenericTilemapSetScrollY(0, DrvFgScrollY);
	GenericTilemapSetScrollX(1, DrvCharScrollX);
	GenericTilemapSetScrollY(1, DrvCharScrollY);

	BurnTransferClear();

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 1, 0xff);

	if (nBurnLayer & 2 && DrvBgEnable) DrvRenderBitmap();

	if (nBurnLayer & 4) GenericTilemapDraw(1, pTransDraw, 4, 0xff);

	if (nSpriteEnable & 1) draw_sprites(2, 0xd00, 0, -16);

	BurnTransferCopy(BurnPalette);

	return 0;
}

static INT32 HotmindRender()
{
	if (BurnRecalc) {
		BurnPaletteUpdate_RRRRGGGGBBBBRGBx();
		BurnRecalc = 0;
	}

	GenericTilemapSetScrollX(0, DrvBgScrollX);
	GenericTilemapSetScrollY(0, DrvBgScrollY);
	GenericTilemapSetScrollX(1, DrvFgScrollX);
	GenericTilemapSetScrollY(1, DrvFgScrollY);
	GenericTilemapSetScrollX(2, DrvCharScrollX);
	GenericTilemapSetScrollY(2, DrvCharScrollY);

	BurnTransferClear();

	if (DrvScreenEnable)
	{
		if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 1, 0xff);

		if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 2, 0xff);

		if (nSpriteEnable & 1) draw_sprites(2, 0x1000, -9, -(8+16));

		if (nBurnLayer & 4) GenericTilemapDraw(2, pTransDraw, 0, 0);
	}

	BurnTransferCopy(BurnPalette);

	return 0;
}

static INT32 BigtwinbRender()
{
	if (BurnRecalc) {
		BurnPaletteUpdate_RRRRGGGGBBBBRGBx();
		BurnRecalc = 0;
	}

	GenericTilemapSetScrollX(0, DrvBgScrollX);
	GenericTilemapSetScrollY(0, DrvBgScrollY);
	GenericTilemapSetScrollX(1, DrvFgScrollX);
	GenericTilemapSetScrollY(1, DrvFgScrollY);
	GenericTilemapSetScrollX(2, DrvCharScrollX);
	GenericTilemapSetScrollY(2, DrvCharScrollY);

	BurnTransferClear();

	if (DrvScreenEnable)
	{
		if (nBurnLayer & 1) GenericTilemapDraw(0, 0, 0);

		if (nSpriteEnable & 1) bigtwinb_draw_sprites(0,-16);

		if (nBurnLayer & 2) GenericTilemapDraw(1, 0, 0);

		if (nBurnLayer & 4) GenericTilemapDraw(2, 0, 0);
	}

	BurnTransferCopy(BurnPalette);

	return 0;
}

static INT32 WbeachvlRender()
{
	GenericTilemapSetScrollX(0, DrvBgScrollX);
	GenericTilemapSetScrollY(0, DrvBgScrollY);
//	GenericTilemapSetScrollX(1, DrvFgScrollX);
	GenericTilemapSetScrollY(1, DrvFgScrollY);
	GenericTilemapSetScrollX(2, DrvCharScrollX);
	GenericTilemapSetScrollY(2, DrvCharScrollY);

	BurnTransferClear();

	if (DrvBgEnable) // rowscroll enable
	{
		GenericTilemapSetScrollRows(1, 512);
		UINT16 *rs = (UINT16*)(DrvTxVideoRAM + 0x2000);

		GenericTilemapSetScrollRow(1, 0, 0);

		for (INT32 i = 0; i < 256; i++)
		{
			GenericTilemapSetScrollRow(1, i+1, rs[i*8]);
		}
	}
	else
	{
		GenericTilemapSetScrollRows(1, 1);
		GenericTilemapSetScrollX(1, DrvFgScrollX);
	}

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 1, 0xff);

	if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 2, 0xff);

	if (nSpriteEnable & 1) draw_sprites(0, 0x1000, 0, -16);

	if (nBurnLayer & 4) GenericTilemapDraw(2, pTransDraw, 0, 0);

	BurnTransferCopy(BurnPalette);

	return 0;
}

static inline void DrvClearOpposites(UINT16* nJoystickInputs)
{
	if ((*nJoystickInputs & 0x03) == 0x00) {
		*nJoystickInputs |= 0x03;
	}
	if ((*nJoystickInputs & 0x0c) == 0x00) {
		*nJoystickInputs |= 0x0c;
	}
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	SekNewFrame();

	{
		memset (DrvInput, 0xff, sizeof(DrvInput));

		for (INT32 i = 0; i < 8; i++) {
			DrvInput[0] ^= (DrvInputPort0[i] & 1) << i;
			DrvInput[1] ^= (DrvInputPort1[i] & 1) << i;
			DrvInput[2] ^= (DrvInputPort2[i] & 1) << i;
			DrvInput[3] ^= (DrvInputPort3[i] & 1) << i;
			DrvInput[4] ^= (DrvInputPort4[i] & 1) << i;
		}

		DrvClearOpposites(&DrvInput[1]);
		DrvClearOpposites(&DrvInput[2]);
		DrvClearOpposites(&DrvInput[3]);
		DrvClearOpposites(&DrvInput[4]);
	}

	INT32 nInterleave = 100;
	INT32 nCyclesTotal[2] = { (INT32)((double)12000000 / 58.0), (INT32)((double)3000000 / 58.0) };
	INT32 nCyclesDone[2] = { 0, 0 };
	INT32 nSoundBufferPos = 0;

	SekOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);

		if (i == 90) {
			if (pBurnDraw) BurnDrvRedraw();

			SekSetIRQLine(nIRQLine, CPU_IRQSTATUS_AUTO);
		}

		CPU_RUN(1, pic16c5x);

		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			MSM6295Render(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}
	SekClose();

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);

		if (nSegmentLength) {
			MSM6295Render(pSoundBuf, nSegmentLength);
		}
	}

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
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);
		pic16c5xScan(nAction);

		MSM6295Scan(nAction, pnMin);

		if (DrvEEPROMInUse) EEPROMScan(nAction, pnMin);

		SCAN_VAR(DrvFgScrollX);
		SCAN_VAR(DrvFgScrollY);
		SCAN_VAR(DrvCharScrollX);
		SCAN_VAR(DrvCharScrollY);
		SCAN_VAR(DrvBgEnable);
		SCAN_VAR(DrvBgFullSize);
		SCAN_VAR(DrvBgScrollX);
		SCAN_VAR(DrvBgScrollY);
		SCAN_VAR(DrvScreenEnable);
		SCAN_VAR(DrvSoundCommand);
		SCAN_VAR(DrvSoundFlag);
		SCAN_VAR(DrvOkiControl);
		SCAN_VAR(DrvOkiCommand);
		SCAN_VAR(DrvOkiBank);
	}

	if (nAction & ACB_WRITE) {
		msm6295_set_bank(DrvOkiBank);
	}

	return 0;
}


// Big Twin

static struct BurnRomInfo BigtwinRomDesc[] = {
	{ "2.302",            				0x80000, 0xe6767f60, 1 | BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "3.301",            				0x80000, 0x5aba6990, 1 | BRF_ESS | BRF_PRG }, //  1

	{ "pic16c57-hs_bigtwin_015.hex", 	0x02d4c, 0xc07e9375, 2 | BRF_ESS | BRF_PRG }, //  2	PIC16C57 HEX

	{ "4.311",             				0x40000, 0x6f628fbc, 3 | BRF_GRA },			  //  3 Tiles
	{ "5.312",             				0x40000, 0x6a9b1752, 3 | BRF_GRA },			  //  4
	{ "6.313",             				0x40000, 0x411cf852, 3 | BRF_GRA },			  //  5
	{ "7.314",             				0x40000, 0x635c81fd, 3 | BRF_GRA },			  //  6

	{ "8.321",             				0x20000, 0x2749644d, 4 | BRF_GRA },			  //  7	Sprites
	{ "9.322",             				0x20000, 0x1d1897af, 4 | BRF_GRA },			  //  8
	{ "10.323",            				0x20000, 0x2a03432e, 4 | BRF_GRA },			  //  9
	{ "11.324",            				0x20000, 0x2c980c4c, 4 | BRF_GRA },			  // 10

	{ "1.013",             				0x40000, 0xff6671dc, 5 | BRF_SND },			  // 11	Samples
};

STD_ROM_PICK(Bigtwin)
STD_ROM_FN(Bigtwin)

struct BurnDriver BurnDrvBigtwin = {
	"bigtwin", NULL, NULL, NULL, "1995",
	"Big Twin\0", NULL, "Playmark", "Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, BigtwinRomInfo, BigtwinRomName, NULL, NULL, NULL, NULL, BigtwinInputInfo, BigtwinDIPInfo,
	BigtwinInit, DrvExit, DrvFrame, BigtwinRender, DrvScan, &BurnRecalc, 0x400,
	320, 240, 4, 3
};


// Big Twin (No Girls Conversion)

static struct BurnRomInfo bigtwinbRomDesc[] = {
	{ "2.u67",							0x20000, 0xf5cdf1a9, 1 | BRF_PRG | BRF_ESS }, //  0 68000 Program Code
	{ "3.u66",							0x20000, 0x084e990f, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "pic16c57-hs_bigtwin_015.hex",	0x02d4c, 0xc07e9375, 2 | BRF_PRG | BRF_ESS }, //  2 PIC16C57 HEX

	{ "4.u36",							0x40000, 0x99aaeacc, 6 | BRF_GRA },           //  3 Tiles
	{ "5.u42",							0x40000, 0x5c1dfd72, 6 | BRF_GRA },           //  4
	{ "6.u39",							0x40000, 0x788f2df6, 6 | BRF_GRA },           //  5
	{ "7.u45",							0x40000, 0xaedb2e6d, 6 | BRF_GRA },           //  6

	{ "11.u86",							0x20000, 0x2749644d, 4 | BRF_GRA },           //  7 Sprites
	{ "10.u85",							0x20000, 0x1d1897af, 4 | BRF_GRA },           //  8
	{ "9.u84",							0x20000, 0x2a03432e, 4 | BRF_GRA },           //  9
	{ "8.u83",							0x20000, 0x2c980c4c, 4 | BRF_GRA },           // 10

	{ "io13.bin",						0x40000, 0xff6671dc, 5 | BRF_SND },           // 11 Samples
};

STD_ROM_PICK(bigtwinb)
STD_ROM_FN(bigtwinb)

struct BurnDriver BurnDrvBigtwinb = {
	"bigtwinb", "bigtwin", NULL, NULL, "1995",
	"Big Twin (No Girls Conversion)\0", NULL, "Playmark", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, bigtwinbRomInfo, bigtwinbRomName, NULL, NULL, NULL, NULL, BigtwinbInputInfo, BigtwinbDIPInfo,
	BigtwinbInit, DrvExit, DrvFrame, BigtwinbRender, DrvScan, &BurnRecalc, 0x400,
	320, 240, 4, 3
};


// Excelsior (set 1)

static struct BurnRomInfo ExcelsrRomDesc[] = {
	{ "19.u302",           				0x80000, 0x9a8acddc, 1 | BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "22.u301",           				0x80000, 0xf0aa1c1b, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "18.u304",           				0x80000, 0xfe517e0e, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "21.u303",           				0x80000, 0xfdf9bd64, 1 | BRF_ESS | BRF_PRG }, //  3
	{ "17.u306",           				0x80000, 0x978f9a6b, 1 | BRF_ESS | BRF_PRG }, //  4
	{ "20.u305",           				0x80000, 0x8692afe9, 1 | BRF_ESS | BRF_PRG }, //  5

	{ "pic16c57-hs_excelsior_i015.hex",	0x02d4c, 0x022c6941, 2 | BRF_ESS | BRF_PRG }, //  6	PIC16C57 HEX

	{ "26.u311",           				0x80000, 0xc171c059, 3 | BRF_GRA },			  //  7	Tiles
	{ "30.u312",           				0x80000, 0xb4a4c510, 3 | BRF_GRA },			  //  8
	{ "25.u313",           				0x80000, 0x667eec1b, 3 | BRF_GRA },			  //  9
	{ "29.u314",           				0x80000, 0x4acb0745, 3 | BRF_GRA },			  // 10

	{ "24.u321",           				0x80000, 0x17f46825, 4 | BRF_GRA },			  // 11	Sprites
	{ "28.u322",           				0x80000, 0xa823f2bd, 4 | BRF_GRA },			  // 12
	{ "23.u323",           				0x80000, 0xd8e1453b, 4 | BRF_GRA },			  // 13
	{ "27.u324",           				0x80000, 0xeca2c079, 4 | BRF_GRA },			  // 14

	{ "16.i013",           				0x80000, 0x7ed9da5d, 5 | BRF_SND },			  // 15
};

STD_ROM_PICK(Excelsr)
STD_ROM_FN(Excelsr)

struct BurnDriver BurnDrvExcelsr = {
	"excelsr", NULL, NULL, NULL, "1996",
	"Excelsior (set 1)\0", NULL, "Playmark", "Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, ExcelsrRomInfo, ExcelsrRomName, NULL, NULL, NULL, NULL, ExcelsrInputInfo, ExcelsrDIPInfo,
	ExcelsrInit, DrvExit, DrvFrame, ExcelsrRender, DrvScan, &BurnRecalc, 0x400,
	320, 240, 4, 3
};


// Excelsior (set 2)

static struct BurnRomInfo ExcelsraRomDesc[] = {
	{ "19.u302",           				0x80000, 0xd13990a8, 1 | BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "22.u301",           				0x80000, 0x55dca2da, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "18.u304",           				0x80000, 0xfe517e0e, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "21.u303",           				0x80000, 0xfdf9bd64, 1 | BRF_ESS | BRF_PRG }, //  3
	{ "17.u306",           				0x80000, 0x978f9a6b, 1 | BRF_ESS | BRF_PRG }, //  4
	{ "20.u305",           				0x80000, 0x8692afe9, 1 | BRF_ESS | BRF_PRG }, //  5

	{ "pic16c57-hs_excelsior_i015.hex",	0x02d4c, 0x022c6941, 2 | BRF_ESS | BRF_PRG }, //  6	PIC16C57 HEX

	{ "26.u311",           				0x80000, 0xc171c059, 3 | BRF_GRA },			  //  7	Tiles
	{ "30.u312",           				0x80000, 0xb4a4c510, 3 | BRF_GRA },			  //  8
	{ "25.u313",           				0x80000, 0x667eec1b, 3 | BRF_GRA },			  //  9
	{ "29.u314",           				0x80000, 0x4acb0745, 3 | BRF_GRA },			  // 10

	{ "24.u321",           				0x80000, 0x17f46825, 4 | BRF_GRA },			  // 11	Sprites
	{ "28.u322",           				0x80000, 0xa823f2bd, 4 | BRF_GRA },			  // 12
	{ "23.u323",           				0x80000, 0xd8e1453b, 4 | BRF_GRA },			  // 13
	{ "27.u324",          				0x80000, 0xeca2c079, 4 | BRF_GRA },			  // 14

	{ "16.i013",           				0x80000, 0x7ed9da5d, 5 | BRF_SND },			  // 15	Samples
};

STD_ROM_PICK(Excelsra)
STD_ROM_FN(Excelsra)

struct BurnDriver BurnDrvExcelsra = {
	"excelsra", "excelsr", NULL, NULL, "1996",
	"Excelsior (set 2)\0", NULL, "Playmark", "Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, ExcelsraRomInfo, ExcelsraRomName, NULL, NULL, NULL, NULL, ExcelsrInputInfo, ExcelsrDIPInfo,
	ExcelsrInit, DrvExit, DrvFrame, ExcelsrRender, DrvScan, &BurnRecalc, 0x400,
	320, 240, 4, 3
};


// Hot Mind (Hard Times hardware)

static struct BurnRomInfo HotmindRomDesc[] = {
	{ "21.u67",            				0x20000, 0xe9000f7f, 1 | BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "22.u66",            				0x20000, 0x2c518ec5, 1 | BRF_ESS | BRF_PRG }, //  1

	{ "hotmind_pic16c57.hex", 			0x02d4c, 0x11957803, 2 | BRF_ESS | BRF_PRG }, //  2	PIC16C57 HEX

	{ "23.u36",            				0x20000, 0xddcf60b9, 6 | BRF_GRA },			  //  3	Tiles
	{ "27.u42",            				0x20000, 0x413bbcf4, 6 | BRF_GRA },			  //  4
	{ "24.u39",            				0x20000, 0x4baa5b4c, 6 | BRF_GRA },			  //  5
	{ "28.u45",            				0x20000, 0x8df34d6a, 6 | BRF_GRA },			  //  6

	{ "26.u86",            				0x20000, 0xff8d3b75, 7 | BRF_GRA },			  //  7	Sprites
	{ "30.u85",            				0x20000, 0x87a640c7, 7 | BRF_GRA },			  //  8
	{ "25.u84",            				0x20000, 0xc4fd4445, 7 | BRF_GRA },			  //  9
	{ "29.u83",           				0x20000, 0x0bebfb53, 7 | BRF_GRA },			  // 10

	{ "20.io13",           				0x40000, 0x0bf3a3e5, 5 | BRF_SND },			  // 11	Samples

	{ "hotmind_pic16c57-hs_io15.hex", 	0x02d4c, 0xf3300d13, 0 | BRF_OPT },			  // 12 PALs
	{ "palce16v8h-25-pc4_u58.jed",    	0x00b89, 0xba88c1da, 0 | BRF_OPT },			  // 13
	{ "palce16v8h-25-pc4_u182.jed",   	0x00b89, 0xba88c1da, 0 | BRF_OPT },			  // 14
	{ "palce16v8h-25-pc4_jamma.jed",  	0x00b89, 0xba88c1da, 0 | BRF_OPT },			  // 15
	{ "tibpal22v10acnt_u113.jed",     	0x01e84, 0x94106c63, 0 | BRF_OPT },			  // 16
	{ "tibpal22v10acnt_u183.jed",     	0x01e84, 0x95a446b6, 0 | BRF_OPT },			  // 17
	{ "tibpal22v10acnt_u211.jed",     	0x01e84, 0x94106c63, 0 | BRF_OPT },			  // 18
};

STD_ROM_PICK(Hotmind)
STD_ROM_FN(Hotmind)

struct BurnDriver BurnDrvHotmind = {
	"hotmind", NULL, NULL, NULL, "1995",
	"Hot Mind (Hard Times hardware)\0", NULL, "Playmark", "Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, HotmindRomInfo, HotmindRomName, NULL, NULL, NULL, NULL, HotmindInputInfo, HotmindDIPInfo,
	HotmindInit, DrvExit, DrvFrame, HotmindRender, DrvScan, &BurnRecalc, 0x400,
	320, 224, 4, 3
};


// Hard Times (set 1)

static struct BurnRomInfo hrdtimesRomDesc[] = {
	{ "31.u67",							0x80000, 0x53eb041b, 1 | BRF_PRG | BRF_ESS }, //  0 68000 Program Code
	{ "32.u66",							0x80000, 0xf2c6b382, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "pic16c57.bin",					0x01000, 0xdb307198, 2 | BRF_PRG | BRF_ESS }, //  2 PIC16C57 Binary

	{ "33.u36",							0x80000, 0xd1239ce5, 6 | BRF_GRA },           //  3 Tiles
	{ "37.u42",							0x80000, 0xaa692005, 6 | BRF_GRA },           //  4
	{ "34.u39",							0x80000, 0xe4108c59, 6 | BRF_GRA },           //  5
	{ "38.u45",							0x80000, 0xff7cacf3, 6 | BRF_GRA },           //  6

	{ "36.u86",							0x80000, 0xf2fc1ca3, 7 | BRF_GRA },           //  7 Sprites
	{ "40.u85",							0x80000, 0x368c15f4, 7 | BRF_GRA },           //  8
	{ "35.u84",							0x80000, 0x7bde46ec, 7 | BRF_GRA },           //  9
	{ "39.u83",							0x80000, 0xa0bae586, 7 | BRF_GRA },           // 10

	{ "30.io13",						0x80000, 0xfa5e50ae, 5 | BRF_SND },           // 11 Samples
};

STD_ROM_PICK(hrdtimes)
STD_ROM_FN(hrdtimes)

struct BurnDriver BurnDrvHrdtimes = {
	"hrdtimes", NULL, NULL, NULL, "1994",
	"Hard Times (set 1)\0", NULL, "Playmark", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, hrdtimesRomInfo, hrdtimesRomName, NULL, NULL, NULL, NULL, HrdtimesInputInfo, HrdtimesDIPInfo,
	HrdtimesInit, DrvExit, DrvFrame, HotmindRender, DrvScan, &BurnRecalc, 0x400,
	320, 224, 4, 3
};


// Hard Times (set 2)

static struct BurnRomInfo hrdtimesaRomDesc[] = {
	{ "u67.bin",						0x080000, 0x3e1334cb, 1 | BRF_PRG | BRF_ESS }, //  0 68000 Program Code
	{ "u66.bin",						0x080000, 0x041ec30a, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "pic16c57.bin",					0x001000, 0xdb307198, 2 | BRF_PRG | BRF_ESS }, //  2 PIC16C57 Binary

	{ "fh1_playmark_ht",				0x100000, 0x3cca02b0, 3 | BRF_GRA },           //  3 Tiles
	{ "fh2_playmark_ht",				0x100000, 0xed699acd, 3 | BRF_GRA },           //  4

	{ "mh1_playmark_ht",				0x100000, 0x927e5989, 4 | BRF_GRA },           //  5 Sprites
	{ "mh2_playmark_ht",				0x100000, 0xe76f001b, 4 | BRF_GRA },           //  6

	{ "io13.bin",						0x080000, 0xfa5e50ae, 5 | BRF_SND },           //  7 Samples
};

STD_ROM_PICK(hrdtimesa)
STD_ROM_FN(hrdtimesa)

struct BurnDriver BurnDrvHrdtimesa = {
	"hrdtimesa", "hrdtimes", NULL, NULL, "1994",
	"Hard Times (set 2)\0", NULL, "Playmark", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, hrdtimesaRomInfo, hrdtimesaRomName, NULL, NULL, NULL, NULL, HrdtimesInputInfo, HrdtimesDIPInfo,
	HrdtimesInit, DrvExit, DrvFrame, HotmindRender, DrvScan, &BurnRecalc, 0x400,
	320, 224, 4, 3
};


// Lucky Boom (Hard Times hardware)

static struct BurnRomInfo luckboomhRomDesc[] = {
	{ "21.u67",							0x20000, 0x5578dd75, 1 | BRF_PRG | BRF_ESS }, //  0 68000 Program Code
	{ "22.u66",							0x20000, 0x1eb72a39, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "luckyboom_pic16c57-hs_io15.bin",	0x02000, 0xc4b9c78e, 0 | BRF_PRG | BRF_OPT }, //  2 PIC16C57 Binary
	{ "luckyboom_pic16c57.hex",			0x02d4c, 0x5c4b5c39, 2 | BRF_PRG | BRF_ESS }, //  3 PIC16C57 HEX

	{ "23.u36",							0x20000, 0x71840dd9, 6 | BRF_GRA },           //  4 Tiles
	{ "27.u42",							0x20000, 0x2f86b37f, 6 | BRF_GRA },           //  5
	{ "24.u39",							0x20000, 0xc6725797, 6 | BRF_GRA },           //  6
	{ "28.u40",							0x20000, 0x40e65ed1, 6 | BRF_GRA },           //  7

	{ "26.u86",							0x20000, 0xd3ee7d82, 7 | BRF_GRA },           //  8 Sprites
	{ "30.u85",							0x20000, 0x4b8a9558, 7 | BRF_GRA },           //  9
	{ "25.u84",							0x20000, 0xe1ab5cf5, 7 | BRF_GRA },           // 10
	{ "29.u83",							0x20000, 0x9572d2d4, 7 | BRF_GRA },           // 11

	{ "20.io13",						0x40000, 0x0d42c0a3, 5 | BRF_SND },           // 12 Samples
};

STD_ROM_PICK(luckboomh)
STD_ROM_FN(luckboomh)

struct BurnDriver BurnDrvLuckboomh = {
	"luckboomh", "luckboom", NULL, NULL, "1996",
	"Lucky Boom (Hard Times hardware)\0", NULL, "Playmark", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_ACTION, 0,
	NULL, luckboomhRomInfo, luckboomhRomName, NULL, NULL, NULL, NULL, LuckboomhInputInfo, LuckboomhDIPInfo,
	LuckboomhInit, DrvExit, DrvFrame, HotmindRender, DrvScan, &BurnRecalc, 0x400,
	320, 224, 4, 3
};


// World Beach Volley (set 1, PIC16C57 audio CPU)

static struct BurnRomInfo wbeachvlRomDesc[] = {
	{ "wbv_02.bin",						0x040000, 0xc7cca29e, 1 | BRF_PRG | BRF_ESS }, //  0 68000 Program Code
	{ "wbv_03.bin",						0x040000, 0xdb4e69d5, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "pic16c57",						0x001009, 0x35439064, 2 | BRF_PRG | BRF_ESS }, //  2 PIC16C57 Binary

	{ "wbv_10.bin",						0x080000, 0x50680f0b, 3 | BRF_GRA },           //  3 Tiles & Sprites
	{ "wbv_04.bin",						0x080000, 0xdf9cbff1, 3 | BRF_GRA },           //  4
	{ "wbv_11.bin",						0x080000, 0xe59ad0d1, 3 | BRF_GRA },           //  5
	{ "wbv_05.bin",						0x080000, 0x51245c3c, 3 | BRF_GRA },           //  6
	{ "wbv_12.bin",						0x080000, 0x36b87d0b, 3 | BRF_GRA },           //  7
	{ "wbv_06.bin",						0x080000, 0x9eb808ef, 3 | BRF_GRA },           //  8
	{ "wbv_13.bin",						0x080000, 0x7021107b, 3 | BRF_GRA },           //  9
	{ "wbv_07.bin",						0x080000, 0x4fff9fe8, 3 | BRF_GRA },           // 10
	{ "wbv_14.bin",						0x080000, 0x0595e675, 3 | BRF_GRA },           // 11
	{ "wbv_08.bin",						0x080000, 0x07e4b416, 3 | BRF_GRA },           // 12
	{ "wbv_15.bin",						0x080000, 0x4e1a82d2, 3 | BRF_GRA },           // 13
	{ "wbv_09.bin",						0x020000, 0x894ce354, 3 | BRF_GRA },           // 14

	{ "wbv_01.bin",						0x100000, 0xac33f25f, 5 | BRF_SND },           // 15 Samples
};

STD_ROM_PICK(wbeachvl)
STD_ROM_FN(wbeachvl)

struct BurnDriver BurnDrvWbeachvl = {
	"wbeachvl", NULL, NULL, NULL, "1995",
	"World Beach Volley (set 1, PIC16C57 audio CPU)\0", NULL, "Playmark", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 4, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, wbeachvlRomInfo, wbeachvlRomName, NULL, NULL, NULL, NULL, WbeachvlInputInfo, WbeachvlDIPInfo,
	WbeachvlInit, DrvExit, DrvFrame, WbeachvlRender, DrvScan, &BurnRecalc, 0x800,
	320, 240, 4, 3
};


// World Beach Volley (set 1, S87C751 audio CPU)

static struct BurnRomInfo wbeachvlaRomDesc[] = {
	{ "wbv_02.bin",						0x040000, 0xc7cca29e, 1 | BRF_PRG | BRF_ESS }, //  0 68000 Program Code
	{ "wbv_03.bin",						0x040000, 0xdb4e69d5, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "s87c751.u36",					0x000800, 0xbc4daa35, 2 | BRF_PRG | BRF_ESS }, //  2 audiomcs

	{ "s87c751.u5",						0x000800, 0x54a6004f, 0 | BRF_PRG | BRF_ESS }, //  3 extracpu

	{ "wbv_10.bin",						0x080000, 0x50680f0b, 3 | BRF_GRA },           //  3 Tiles & Sprites
	{ "wbv_04.bin",						0x080000, 0xdf9cbff1, 3 | BRF_GRA },           //  4
	{ "wbv_11.bin",						0x080000, 0xe59ad0d1, 3 | BRF_GRA },           //  5
	{ "wbv_05.bin",						0x080000, 0x51245c3c, 3 | BRF_GRA },           //  6
	{ "wbv_12.bin",						0x080000, 0x36b87d0b, 3 | BRF_GRA },           //  7
	{ "wbv_06.bin",						0x080000, 0x9eb808ef, 3 | BRF_GRA },           //  8
	{ "wbv_13.bin",						0x080000, 0x7021107b, 3 | BRF_GRA },           //  9
	{ "wbv_07.bin",						0x080000, 0x4fff9fe8, 3 | BRF_GRA },           // 10
	{ "wbv_14.bin",						0x080000, 0x0595e675, 3 | BRF_GRA },           // 11
	{ "wbv_08.bin",						0x080000, 0x07e4b416, 3 | BRF_GRA },           // 12
	{ "wbv_15.bin",						0x080000, 0x4e1a82d2, 3 | BRF_GRA },           // 13
	{ "wbv_09.bin",						0x020000, 0x894ce354, 3 | BRF_GRA },           // 14

	{ "wbv_01.bin",						0x100000, 0xac33f25f, 5 | BRF_SND },           // 15 Samples
};

STD_ROM_PICK(wbeachvla)
STD_ROM_FN(wbeachvla)

struct BurnDriverX BurnDrvWbeachvla = {
	"wbeachvla", "wbeachvl", NULL, NULL, "1995",
	"World Beach Volley (set 1, S87C751 audio CPU)\0", "No sound, use parent!", "Playmark", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE, 4, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, wbeachvlaRomInfo, wbeachvlaRomName, NULL, NULL, NULL, NULL, WbeachvlInputInfo, WbeachvlDIPInfo,
	WbeachvlInit, DrvExit, DrvFrame, WbeachvlRender, DrvScan, &BurnRecalc, 0x800,
	320, 240, 4, 3
};


// World Beach Volley (set 2)

static struct BurnRomInfo wbeachvl2RomDesc[] = {
	{ "2.bin",							0x040000, 0x8993487e, 1 | BRF_PRG | BRF_ESS }, //  0 68000 Program Code
	{ "3.bin",							0x040000, 0x15904789, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "pic16c57",						0x001009, 0x35439064, 2 | BRF_PRG | BRF_ESS }, //  2 PIC16C57 Binary

	{ "wbv_10.bin",						0x080000, 0x50680f0b, 3 | BRF_GRA },           //  3 Tiles & Sprites
	{ "wbv_04.bin",						0x080000, 0xdf9cbff1, 3 | BRF_GRA },           //  4
	{ "wbv_11.bin",						0x080000, 0xe59ad0d1, 3 | BRF_GRA },           //  5
	{ "wbv_05.bin",						0x080000, 0x51245c3c, 3 | BRF_GRA },           //  6
	{ "wbv_12.bin",						0x080000, 0x36b87d0b, 3 | BRF_GRA },           //  7
	{ "wbv_06.bin",						0x080000, 0x9eb808ef, 3 | BRF_GRA },           //  8
	{ "wbv_13.bin",						0x080000, 0x7021107b, 3 | BRF_GRA },           //  9
	{ "wbv_07.bin",						0x080000, 0x4fff9fe8, 3 | BRF_GRA },           // 10
	{ "wbv_14.bin",						0x080000, 0x0595e675, 3 | BRF_GRA },           // 11
	{ "wbv_08.bin",						0x080000, 0x07e4b416, 3 | BRF_GRA },           // 12
	{ "wbv_15.bin",						0x080000, 0x4e1a82d2, 3 | BRF_GRA },           // 13
	{ "wbv_09.bin",						0x020000, 0x894ce354, 3 | BRF_GRA },           // 14

	{ "wbv_01.bin",						0x100000, 0xac33f25f, 5 | BRF_SND },           // 15 Samples
};

STD_ROM_PICK(wbeachvl2)
STD_ROM_FN(wbeachvl2)

struct BurnDriver BurnDrvWbeachvl2 = {
	"wbeachvl2", "wbeachvl", NULL, NULL, "1995",
	"World Beach Volley (set 2)\0", NULL, "Playmark", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, wbeachvl2RomInfo, wbeachvl2RomName, NULL, NULL, NULL, NULL, WbeachvlInputInfo, WbeachvlDIPInfo,
	WbeachvlInit, DrvExit, DrvFrame, WbeachvlRender, DrvScan, &BurnRecalc, 0x800,
	320, 240, 4, 3
};


// World Beach Volley (set 3)

static struct BurnRomInfo wbeachvl3RomDesc[] = {
	{ "2.u16",							0x040000, 0xf0f4c282, 1 | BRF_PRG | BRF_ESS }, //  0 68000 Program Code
	{ "3.u15",							0x040000, 0x99775c21, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "pic16c57",						0x001009, 0x35439064, 2 | BRF_PRG | BRF_ESS }, //  2 PIC16C57 Binary

	{ "wbv_10.bin",						0x080000, 0x50680f0b, 3 | BRF_GRA },           //  3 Tiles & Sprites
	{ "wbv_04.bin",						0x080000, 0xdf9cbff1, 3 | BRF_GRA },           //  4
	{ "wbv_11.bin",						0x080000, 0xe59ad0d1, 3 | BRF_GRA },           //  5
	{ "wbv_05.bin",						0x080000, 0x51245c3c, 3 | BRF_GRA },           //  6
	{ "wbv_12.bin",						0x080000, 0x36b87d0b, 3 | BRF_GRA },           //  7
	{ "wbv_06.bin",						0x080000, 0x9eb808ef, 3 | BRF_GRA },           //  8
	{ "wbv_13.bin",						0x080000, 0x7021107b, 3 | BRF_GRA },           //  9
	{ "wbv_07.bin",						0x080000, 0x4fff9fe8, 3 | BRF_GRA },           // 10
	{ "wbv_14.bin",						0x080000, 0x0595e675, 3 | BRF_GRA },           // 11
	{ "wbv_08.bin",						0x080000, 0x07e4b416, 3 | BRF_GRA },           // 12
	{ "wbv_15.bin",						0x080000, 0x4e1a82d2, 3 | BRF_GRA },           // 13
	{ "wbv_09.bin",						0x020000, 0x894ce354, 3 | BRF_GRA },           // 14

	{ "wbv_01.bin",						0x100000, 0xac33f25f, 5 | BRF_SND },           // 15 Samples
};

STD_ROM_PICK(wbeachvl3)
STD_ROM_FN(wbeachvl3)

struct BurnDriver BurnDrvWbeachvl3 = {
	"wbeachvl3", "wbeachvl", NULL, NULL, "1995",
	"World Beach Volley (set 3)\0", NULL, "Playmark", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, wbeachvl3RomInfo, wbeachvl3RomName, NULL, NULL, NULL, NULL, WbeachvlInputInfo, WbeachvlDIPInfo,
	WbeachvlInit, DrvExit, DrvFrame, WbeachvlRender, DrvScan, &BurnRecalc, 0x800,
	320, 240, 4, 3
};
