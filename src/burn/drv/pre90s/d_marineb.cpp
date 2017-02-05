// Based on original MAME driver writen by Zsolt Vasvari
// updated with all the romsets by dink, Oct. 2015

#include "tiles_generic.h"
#include "z80_intf.h"

#include "driver.h"
extern "C" {
 #include "ay8910.h"
}

enum { SPRINGER = 0, MARINEB, HOPPROBO, CHANGES, HOCCER, WANTED, BCRUZM12 };

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *RamStart;
static UINT8 *RamEnd;
static UINT32 *DrvPalette;
static UINT8 DrvRecalcPalette;

static UINT8 DrvInputPort0[8];
static UINT8 DrvInputPort1[8];
static UINT8 DrvInputPort2[8];
static UINT8 DrvDip;
static UINT8 DrvInput[3];
static UINT8 DrvReset;

static UINT8 *DrvZ80ROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvColPROM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvColRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;

static UINT8 DrvPaletteBank;
static UINT8 DrvColumnScroll;
static UINT8 DrvFlipScreenY;
static UINT8 DrvFlipScreenX;
static INT32 DrvInterruptEnable;
static UINT8 ActiveLowFlipscreen;
static INT32 hardware;

static INT16 *pAY8910Buffer[6];


static struct BurnInputInfo MarinebInputList[] =
{
	{"P1 Coin"  	   , BIT_DIGITAL  , DrvInputPort2 + 0, "p1 coin"   },
	{"P1 Start"        , BIT_DIGITAL  , DrvInputPort2 + 2, "p1 start"  },
	{"P1 Up"           , BIT_DIGITAL  , DrvInputPort0 + 0, "p1 up"     },
	{"P1 Down"         , BIT_DIGITAL  , DrvInputPort0 + 1, "p1 down"   },
	{"P1 Left"         , BIT_DIGITAL  , DrvInputPort0 + 6, "p1 left"   },
	{"P1 Right"        , BIT_DIGITAL  , DrvInputPort0 + 7, "p1 right"  },
	{"P1 Fire"         , BIT_DIGITAL  , DrvInputPort2 + 4, "p1 fire 1" },

	{"P2 Start"        , BIT_DIGITAL  , DrvInputPort2 + 3, "p2 start"  },
	{"P2 Up"           , BIT_DIGITAL  , DrvInputPort1 + 0, "p2 up"     },
	{"P2 Down"         , BIT_DIGITAL  , DrvInputPort1 + 1, "p2 down"   },
	{"P2 Left"         , BIT_DIGITAL  , DrvInputPort1 + 6, "p2 left"   },
	{"P2 Right"        , BIT_DIGITAL  , DrvInputPort1 + 7, "p2 right"  },
	{"P2 Fire"         , BIT_DIGITAL  , DrvInputPort2 + 5, "p2 fire 1" },

	{"Reset"           , BIT_DIGITAL  , &DrvReset        , "reset"     },
	{"Dip"             , BIT_DIPSWITCH, &DrvDip          , "dip"       },

};

STDINPUTINFO(Marineb)

static struct BurnDIPInfo MarinebDIPList[]=
{
	{0x0E, 0xFF, 0xFF, 0x40, NULL			},

	{0   , 0xFE, 0   ,    4, "Lives"		},
	{0x0E, 0x01, 0x03, 0x00, "3"			},
	{0x0E, 0x01, 0x03, 0x01, "4"			},
	{0x0E, 0x01, 0x03, 0x02, "5"			},
	{0x0E, 0x01, 0x03, 0x03, "6"			},

	{0   , 0xFE, 0   ,    2, "Coinage"		},
	{0x0E, 0x01, 0x1C, 0x00, "1 Coin 1 Credit" },
	{0x0E, 0x01, 0x1C, 0x1C, "Free Play"	},

	{0   , 0xFE, 0   ,    2, "Bonus Life"	},
	{0x0E, 0x01, 0x20, 0x00, "20000 50000"	},
	{0x0E, 0x01, 0x20, 0x20, "40000 70000"	},

	{0   , 0xFE, 0   ,    2, "Cabinet"		},
	{0x0E, 0x01, 0x40, 0x40, "Upright"		},
	{0x0E, 0x01, 0x40, 0x00, "Cocktail"		},

};

STDDIPINFO(Marineb)

static struct BurnInputInfo ChangesInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvInputPort2 + 0, "p1 coin"    },
	{"P1 Start",	BIT_DIGITAL,	DrvInputPort2 + 2, "p1 start"   },
	{"P1 Up",       BIT_DIGITAL,    DrvInputPort0 + 0, "p1 up"      },
	{"P1 Down",     BIT_DIGITAL,    DrvInputPort0 + 1, "p1 down"    },
	{"P1 Left",     BIT_DIGITAL,    DrvInputPort0 + 6, "p1 left"    },
	{"P1 Right",    BIT_DIGITAL,    DrvInputPort0 + 7, "p1 right"   },

	{"P2 Start",	BIT_DIGITAL,	DrvInputPort2 + 3, "p2 start"   },
	{"P2 Up",       BIT_DIGITAL,    DrvInputPort1 + 0, "p2 up"      },
	{"P2 Down",     BIT_DIGITAL,    DrvInputPort1 + 1, "p2 down"    },
	{"P2 Left",     BIT_DIGITAL,    DrvInputPort1 + 6, "p2 left"    },
	{"P2 Right",    BIT_DIGITAL,    DrvInputPort1 + 7, "p2 right"   },

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Dip A",		BIT_DIPSWITCH,	&DrvDip,	"dip"},
};

STDINPUTINFO(Changes)


static struct BurnDIPInfo ChangesDIPList[]=
{
	{0x0c, 0xff, 0xff, 0x40, NULL		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0c, 0x01, 0x03, 0x00, "3"		},
	{0x0c, 0x01, 0x03, 0x01, "4"		},
	{0x0c, 0x01, 0x03, 0x02, "5"		},
	{0x0c, 0x01, 0x03, 0x03, "6"		},

	{0   , 0xfe, 0   ,    2, "Coinage"		},
	{0x0c, 0x01, 0x0c, 0x00, "1 Coin  1 Credits" },
	{0x0c, 0x01, 0x0c, 0x0c, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "1st Bonus Life"		},
	{0x0c, 0x01, 0x10, 0x00, "20000"		},
	{0x0c, 0x01, 0x10, 0x10, "40000"		},

	{0   , 0xfe, 0   ,    2, "2nd Bonus Life"		},
	{0x0c, 0x01, 0x20, 0x00, "50000"		},
	{0x0c, 0x01, 0x20, 0x20, "100000"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0c, 0x01, 0x40, 0x40, "Upright"		},
	{0x0c, 0x01, 0x40, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x0c, 0x01, 0x80, 0x00, "Off"		},
	{0x0c, 0x01, 0x80, 0x80, "On"		},
};

static struct BurnInputInfo HoccerInputList[] = {
	{"P1 Coin"  	   , BIT_DIGITAL  , DrvInputPort2 + 0, "p1 coin"   },
	{"P1 Start"        , BIT_DIGITAL  , DrvInputPort2 + 2, "p1 start"  },
	{"P1 Up"           , BIT_DIGITAL  , DrvInputPort0 + 0, "p1 up"     },
	{"P1 Down"         , BIT_DIGITAL  , DrvInputPort0 + 1, "p1 down"   },
	{"P1 Left"         , BIT_DIGITAL  , DrvInputPort0 + 6, "p1 left"   },
	{"P1 Right"        , BIT_DIGITAL  , DrvInputPort0 + 7, "p1 right"  },
	{"P1 Fire"         , BIT_DIGITAL  , DrvInputPort2 + 4, "p1 fire 1" },

	{"P2 Coin"  	   , BIT_DIGITAL  , DrvInputPort2 + 1, "p2 coin"   },
	{"P2 Start"        , BIT_DIGITAL  , DrvInputPort2 + 3, "p2 start"  },
	{"P2 Up"           , BIT_DIGITAL  , DrvInputPort1 + 0, "p2 up"     },
	{"P2 Down"         , BIT_DIGITAL  , DrvInputPort1 + 1, "p2 down"   },
	{"P2 Left"         , BIT_DIGITAL  , DrvInputPort1 + 6, "p2 left"   },
	{"P2 Right"        , BIT_DIGITAL  , DrvInputPort1 + 7, "p2 right"  },
	{"P2 Fire"         , BIT_DIGITAL  , DrvInputPort2 + 5, "p2 fire 1" },

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Dip A",		BIT_DIPSWITCH,	&DrvDip,	"dip"},
};

STDINPUTINFO(Hoccer)


static struct BurnDIPInfo HoccerDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x00, NULL		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0f, 0x01, 0x01, 0x00, "Upright"		},
	{0x0f, 0x01, 0x01, 0x01, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x0f, 0x01, 0x02, 0x00, "Off"		},
	{0x0f, 0x01, 0x02, 0x02, "On"		},

	{0   , 0xfe, 0   ,    4, "Unknown"		},
	{0x0f, 0x01, 0x0c, 0x00, "0"		},
	{0x0f, 0x01, 0x0c, 0x04, "1"		},
	{0x0f, 0x01, 0x0c, 0x08, "2"		},
	{0x0f, 0x01, 0x0c, 0x0c, "3"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0f, 0x01, 0x30, 0x00, "3"		},
	{0x0f, 0x01, 0x30, 0x10, "4"		},
	{0x0f, 0x01, 0x30, 0x20, "5"		},
	{0x0f, 0x01, 0x30, 0x30, "6"		},

	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{0x0f, 0x01, 0xc0, 0xc0, "2 Coins 1 Credits"		},
	{0x0f, 0x01, 0xc0, 0x00, "1 Coin  1 Credits"		},
	{0x0f, 0x01, 0xc0, 0x40, "1 Coin  2 Credits"		},
	{0x0f, 0x01, 0xc0, 0x80, "1 Coin  3 Credits"		},
};

STDDIPINFO(Hoccer)

static struct BurnInputInfo WantedInputList[] = {
	{"P1 Coin"  	   , BIT_DIGITAL  , DrvInputPort0 + 7, "p1 coin"   },
	{"P1 Start"        , BIT_DIGITAL  , DrvInputPort1 + 7, "p1 start"  },
	{"P1 Up"           , BIT_DIGITAL  , DrvInputPort0 + 3, "p1 up"     },
	{"P1 Down"         , BIT_DIGITAL  , DrvInputPort0 + 2, "p1 down"   },
	{"P1 Left"         , BIT_DIGITAL  , DrvInputPort0 + 0, "p1 left"   },
	{"P1 Right"        , BIT_DIGITAL  , DrvInputPort0 + 1, "p1 right"  },
	{"P1 Fire"         , BIT_DIGITAL  , DrvInputPort0 + 5, "p1 fire 1" },

	{"P2 Coin"  	   , BIT_DIGITAL  , DrvInputPort0 + 6, "p2 coin"   },
	{"P2 Start"        , BIT_DIGITAL  , DrvInputPort1 + 6, "p2 start"  },
	{"P2 Up"           , BIT_DIGITAL  , DrvInputPort1 + 3, "p2 up"     },
	{"P2 Down"         , BIT_DIGITAL  , DrvInputPort1 + 2, "p2 down"   },
	{"P2 Left"         , BIT_DIGITAL  , DrvInputPort1 + 0, "p2 left"   },
	{"P2 Right"        , BIT_DIGITAL  , DrvInputPort1 + 1, "p2 right"  },
	{"P2 Fire"         , BIT_DIGITAL  , DrvInputPort1 + 5, "p2 fire 1" },

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Dip A",		BIT_DIPSWITCH,	&DrvDip,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvInput + 2,	"dip"},
};

STDINPUTINFO(Wanted)


static struct BurnDIPInfo WantedDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x30, NULL		},
	{0x10, 0xff, 0xff, 0x10, NULL		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0f, 0x01, 0x03, 0x00, "3"		},
	{0x0f, 0x01, 0x03, 0x01, "4"		},
	{0x0f, 0x01, 0x03, 0x02, "5"		},
	{0x0f, 0x01, 0x03, 0x03, "6"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x0f, 0x01, 0x04, 0x00, "Off"		},
	{0x0f, 0x01, 0x04, 0x04, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x0f, 0x01, 0x08, 0x00, "Off"		},
	{0x0f, 0x01, 0x08, 0x08, "On"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x0f, 0x01, 0x10, 0x10, "Off"		},
	{0x0f, 0x01, 0x10, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0f, 0x01, 0x20, 0x20, "Upright"		},
	{0x0f, 0x01, 0x20, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x0f, 0x01, 0x40, 0x00, "Off"		},
	{0x0f, 0x01, 0x40, 0x40, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x0f, 0x01, 0x80, 0x00, "Off"		},
	{0x0f, 0x01, 0x80, 0x80, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x10, 0x01, 0x01, 0x00, "Off"		},
	{0x10, 0x01, 0x01, 0x01, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x10, 0x01, 0x02, 0x00, "Off"		},
	{0x10, 0x01, 0x02, 0x02, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x10, 0x01, 0x04, 0x00, "Off"		},
	{0x10, 0x01, 0x04, 0x04, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x10, 0x01, 0x08, 0x00, "Off"		},
	{0x10, 0x01, 0x08, 0x08, "On"		},

	{0   , 0xfe, 0   ,    16, "Coinage"		},
	{0x10, 0x01, 0xf0, 0x40, "A 3C/1C  B 3C/1C"		},
	{0x10, 0x01, 0xf0, 0xe0, "A 3C/1C  B 1C/2C"		},
	{0x10, 0x01, 0xf0, 0xf0, "A 3C/1C  B 1C/4C"		},
	{0x10, 0x01, 0xf0, 0x20, "A 2C/1C  B 2C/1C"		},
	{0x10, 0x01, 0xf0, 0xd0, "A 2C/1C  B 1C/1C"		},
	{0x10, 0x01, 0xf0, 0x70, "A 2C/1C  B 1C/3C"		},
	{0x10, 0x01, 0xf0, 0xb0, "A 2C/1C  B 1C/5C"		},
	{0x10, 0x01, 0xf0, 0xc0, "A 2C/1C  B 1C/6C"		},
	{0x10, 0x01, 0xf0, 0x60, "A 1C/1C  B 4C/1C"		},
	{0x10, 0x01, 0xf0, 0x50, "A 1C/1C  B 2C/1C"		},
	{0x10, 0x01, 0xf0, 0x10, "A 1C/1C  B 1C/1C"		},
	{0x10, 0x01, 0xf0, 0x30, "A 1C/2C  B 1C/2C"		},
	{0x10, 0x01, 0xf0, 0xa0, "A 1C/1C  B 1C/3C"		},
	{0x10, 0x01, 0xf0, 0x80, "A 1C/1C  B 1C/5C"		},
	{0x10, 0x01, 0xf0, 0x90, "A 1C/1C  B 1C/6C"		},
	{0x10, 0x01, 0xf0, 0x00, "Free Play"		},
};

STDDIPINFO(Wanted)

static struct BurnInputInfo Bcruzm12InputList[] = {
	{"P1 Coin"  	   , BIT_DIGITAL  , DrvInputPort0 + 7, "p1 coin"   },
	{"P1 Start"        , BIT_DIGITAL  , DrvInputPort1 + 7, "p1 start"  },
	{"P1 Up"           , BIT_DIGITAL  , DrvInputPort0 + 3, "p1 up"     },
	{"P1 Down"         , BIT_DIGITAL  , DrvInputPort0 + 2, "p1 down"   },
	{"P1 Left"         , BIT_DIGITAL  , DrvInputPort0 + 0, "p1 left"   },
	{"P1 Right"        , BIT_DIGITAL  , DrvInputPort0 + 1, "p1 right"  },
	{"P1 Fire"         , BIT_DIGITAL  , DrvInputPort0 + 5, "p1 fire 1" },

	{"P2 Coin"  	   , BIT_DIGITAL  , DrvInputPort0 + 6, "p2 coin"   },
	{"P2 Start"        , BIT_DIGITAL  , DrvInputPort1 + 6, "p2 start"  },
	{"P2 Up"           , BIT_DIGITAL  , DrvInputPort1 + 3, "p2 up"     },
	{"P2 Down"         , BIT_DIGITAL  , DrvInputPort1 + 2, "p2 down"   },
	{"P2 Left"         , BIT_DIGITAL  , DrvInputPort1 + 0, "p2 left"   },
	{"P2 Right"        , BIT_DIGITAL  , DrvInputPort1 + 1, "p2 right"  },
	{"P2 Fire"         , BIT_DIGITAL  , DrvInputPort1 + 5, "p2 fire 1" },

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Dip A",		BIT_DIPSWITCH,	&DrvDip,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvInput + 2,	"dip"},
};

STDINPUTINFO(Bcruzm12)


static struct BurnDIPInfo Bcruzm12DIPList[]=
{
	{0x0f, 0xff, 0xff, 0xfc, NULL		},
	{0x10, 0xff, 0xff, 0x15, NULL		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0f, 0x01, 0x03, 0x00, "3"		},
	{0x0f, 0x01, 0x03, 0x01, "4"		},
	{0x0f, 0x01, 0x03, 0x02, "5"		},
	{0x0f, 0x01, 0x03, 0x03, "6"		},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x0f, 0x01, 0x0c, 0x0c, "Off"		},
	{0x0f, 0x01, 0x0c, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x0f, 0x01, 0x10, 0x10, "Off"		},
	{0x0f, 0x01, 0x10, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0f, 0x01, 0x20, 0x20, "Upright"		},
	{0x0f, 0x01, 0x20, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x0f, 0x01, 0x40, 0x00, "Off"		},
	{0x0f, 0x01, 0x40, 0x40, "On"		},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x0f, 0x01, 0x80, 0x80, "Off"		},
	{0x0f, 0x01, 0x80, 0x00, "On"		},

	{0   , 0xfe, 0   ,    4, "2nd Bonus Life"		},
	{0x10, 0x01, 0x03, 0x00, "None"		},
	{0x10, 0x01, 0x03, 0x01, "60000"		},
	{0x10, 0x01, 0x03, 0x02, "80000"		},
	{0x10, 0x01, 0x03, 0x03, "100000"		},

	{0   , 0xfe, 0   ,    4, "1st Bonus Life"		},
	{0x10, 0x01, 0x0c, 0x00, "None"		},
	{0x10, 0x01, 0x0c, 0x04, "30000"		},
	{0x10, 0x01, 0x0c, 0x08, "40000"		},
	{0x10, 0x01, 0x0c, 0x0c, "50000"		},

	{0   , 0xfe, 0   ,    16, "Coinage"		},
	{0x10, 0x01, 0xf0, 0x40, "A 3C/1C  B 3C/1C"		},
	{0x10, 0x01, 0xf0, 0xe0, "A 3C/1C  B 1C/2C"		},
	{0x10, 0x01, 0xf0, 0xf0, "A 3C/1C  B 1C/4C"		},
	{0x10, 0x01, 0xf0, 0x20, "A 2C/1C  B 2C/1C"		},
	{0x10, 0x01, 0xf0, 0xd0, "A 2C/1C  B 1C/1C"		},
	{0x10, 0x01, 0xf0, 0x70, "A 2C/1C  B 1C/3C"		},
	{0x10, 0x01, 0xf0, 0xb0, "A 2C/1C  B 1C/5C"		},
	{0x10, 0x01, 0xf0, 0xc0, "A 2C/1C  B 1C/6C"		},
	{0x10, 0x01, 0xf0, 0x60, "A 1C/1C  B 4C/5C"		},
	{0x10, 0x01, 0xf0, 0x50, "A 1C/1C  B 2C/3C"		},
	{0x10, 0x01, 0xf0, 0x10, "A 1C/1C  B 1C/1C"		},
	{0x10, 0x01, 0xf0, 0x30, "A 1C/2C  B 1C/2C"		},
	{0x10, 0x01, 0xf0, 0xa0, "A 1C/1C  B 1C/3C"		},
	{0x10, 0x01, 0xf0, 0x80, "A 1C/1C  B 1C/5C"		},
	{0x10, 0x01, 0xf0, 0x90, "A 1C/1C  B 1C/6C"		},
	{0x10, 0x01, 0xf0, 0x00, "Free Play"		},
};

STDDIPINFO(Bcruzm12)


STDDIPINFO(Changes)

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM			= Next; Next += 0x10000;
	DrvColPROM    		= Next; Next += 0x200;
	DrvGfxROM0    		= Next; Next += 1024 * 8 * 8;
	DrvGfxROM1   		= Next; Next += 64*2 * 16 * 16;
	DrvGfxROM2    		= Next; Next += 64*2 * 32 * 32;

	DrvPalette  = (UINT32*)Next; Next += 0x100 * sizeof(UINT32);

	pAY8910Buffer[0]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[1]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[2]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[3]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[4]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[5]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);

	RamStart	= Next;

	DrvZ80RAM	= Next;  Next += 0x800;
	DrvVidRAM	= Next;  Next += 0x400;
	DrvSprRAM   = Next;  Next += 0x100;
	DrvColRAM   = Next;  Next += 0x400;

	RamEnd		= Next;
	MemEnd		= Next;

	return 0;
}

static void CleanAndInitStuff()
{
	DrvPaletteBank = 0;
	DrvColumnScroll = 0;
	DrvFlipScreenY = 0;
	DrvFlipScreenX = 0;
	DrvInterruptEnable = 0;
	hardware = 0;
	ActiveLowFlipscreen = 0;

	memset(DrvInputPort0, 0, 8);
	memset(DrvInputPort1, 0, 8);
	memset(DrvInputPort2, 0, 8);
	memset(DrvInput, 0, 3);

    DrvDip = 0;
    DrvReset = 0;
}


UINT8 __fastcall marineb_read(UINT16 address)
{
	switch (address) {
		case 0xa800:
			return DrvInput[0];

		case 0xa000:
			return DrvInput[1];

		case 0xb000:
			return DrvDip;

		case 0xb800:
			return DrvInput[2];
	}

	return 0;
}


void __fastcall marineb_write(UINT16 address, UINT8 data)
{
	switch (address) {

		case 0x9800:
			DrvColumnScroll = data;
			return;

		case 0x9a00:
			DrvPaletteBank = (DrvPaletteBank & 0x02) | (data & 0x01);
			return;

		case 0x9c00:
			DrvPaletteBank = (DrvPaletteBank & 0x01) | ((data & 0x01) << 1);
			return;


		case 0xa000:
			DrvInterruptEnable = data & 1;
			return;

		case 0xa001:
			DrvFlipScreenY = data ^ ActiveLowFlipscreen;
			return;

		case 0xa002:
			DrvFlipScreenX = data ^ ActiveLowFlipscreen;
			return;
	}
}

void __fastcall marineb_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xFF) {
		case 0x00:
		case 0x01:
		case 0x08:
		case 0x09:
			AY8910Write(0, port & 1, data);
			break;
		case 0x02:
		case 0x03:
			AY8910Write(1, port & 1, data);
			break;
	}
}

static INT32 DrvDoReset()
{
	memset (RamStart, 0, RamEnd - RamStart);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	AY8910Reset(0);

	DrvPaletteBank = 0;
	DrvColumnScroll = 0;
	DrvFlipScreenY = 0;
	DrvFlipScreenX = 0;
	DrvInterruptEnable = 0;

	return 0;
}

static void DrvCreatePalette()
{
	for (INT32 i = 0; i < 256; i++)
	{
		INT32 bit0, bit1, bit2, r, g, b;

		// Red
		bit0 = (DrvColPROM[i] >> 0) & 0x01;
		bit1 = (DrvColPROM[i] >> 1) & 0x01;
		bit2 = (DrvColPROM[i] >> 2) & 0x01;
		r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		// Green
		bit0 = (DrvColPROM[i] >> 3) & 0x01;
		bit1 = (DrvColPROM[i + 256] >> 0) & 0x01;
		bit2 = (DrvColPROM[i + 256] >> 1) & 0x01;
		g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		// Blue
		bit0 = 0;
		bit1 = (DrvColPROM[i + 256] >> 2) & 0x01;
		bit2 = (DrvColPROM[i + 256] >> 3) & 0x01;
		b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}
}


static INT32 MarinebCharPlane[2] = { 0, 4 };
static INT32 MarinebCharXOffs[8] = { 0, 1, 2, 3, 64, 65, 66, 67 };
static INT32 MarinebCharYOffs[8] = { 0, 8, 16, 24, 32, 40, 48, 56 };

static INT32 WantedCharPlane[2] = { 4, 0 };
static INT32 WantedCharXOffs[8] = { 0, 1, 2, 3, 64, 65, 66, 67 };
static INT32 WantedCharYOffs[8] = { 0, 8, 16, 24, 32, 40, 48, 56 };

static INT32 MarinebSmallSpriteCharPlane[2] = { 0, 65536 };
static INT32 MarinebSmallSpriteXOffs[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 64, 65, 66, 67, 68, 69, 70, 71 };
static INT32 MarinebSmallSpriteYOffs[16] = { 0, 8, 16, 24, 32, 40, 48, 56, 128, 136, 144, 152, 160, 168, 176, 184 };

static INT32 MarinebBigSpriteCharPlane[2] = { 0, 65536 };
static INT32 MarinebBigSpriteXOffs[32] = { 0, 1, 2, 3, 4, 5, 6, 7, 64, 65, 66, 67, 68, 69, 70, 71, 256, 257, 258, 259, 260, 261, 262, 263, 320, 321, 322, 323, 324, 325, 326, 327 };
static INT32 MarinebBigSpriteYOffs[32] = { 0, 8, 16, 24, 32, 40, 48, 56, 128, 136, 144, 152, 160, 168, 176, 184, 512, 520, 528, 536, 544, 552, 560, 568, 640, 648, 656, 664, 672, 680, 688, 696 };

static INT32 ChangesSmallSpriteCharPlane[2] = { 0, 4 };
static INT32 ChangesSmallSpriteXOffs[16] = { 0, 1, 2, 3, 8*8+0, 8*8+1, 8*8+2, 8*8+3, 16*8+0, 16*8+1, 16*8+2, 16*8+3, 24*8+0, 24*8+1, 24*8+2, 24*8+3 };
static INT32 ChangesSmallSpriteYOffs[16] = { 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8, 32*8, 33*8, 34*8, 35*8, 36*8, 37*8, 38*8, 39*8 };

static INT32 ChangesBigSpriteCharPlane[2] = { 0, 4 };
static INT32 ChangesBigSpriteXOffs[32] = { 0, 1, 2, 3, 8*8+0, 8*8+1, 8*8+2, 8*8+3, 16*8+0, 16*8+1, 16*8+2, 16*8+3, 24*8+0, 24*8+1, 24*8+2, 24*8+3, 64*8+0, 64*8+1, 64*8+2, 64*8+3, 72*8+0, 72*8+1, 72*8+2, 72*8+3, 80*8+0, 80*8+1, 80*8+2, 80*8+3, 88*8+0, 88*8+1, 88*8+2, 88*8+3 };
static INT32 ChangesBigSpriteYOffs[32] = { 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8, 32*8, 33*8, 34*8, 35*8, 36*8, 37*8, 38*8, 39*8, 128*8, 129*8, 130*8, 131*8, 132*8, 133*8, 134*8, 135*8, 160*8, 161*8, 162*8, 163*8, 164*8, 165*8, 166*8, 167*8 };

static INT32 MarinebLoadRoms()
{
	// Load roms

	// Z80
	for (INT32 i = 0; i < 5; i++) {
		if (BurnLoadRom(DrvZ80ROM + (i * 0x1000), i, 1)) return 1;
	}

	UINT8 *tmp = (UINT8*)BurnMalloc(0x4000);
	if (tmp == NULL) return 1;

	// Chars
	memset(tmp, 0, 0x2000);
	if (BurnLoadRom(tmp, 5, 1)) return 1;
	GfxDecode(0x200, 2, 8, 8, MarinebCharPlane, MarinebCharXOffs, MarinebCharYOffs, 0x80, tmp, DrvGfxROM0);

	// Sprites
	memset(tmp, 0, 0x4000);
	if (BurnLoadRom(tmp, 6, 1))     return 1;
	if (BurnLoadRom(tmp + 0x2000, 7, 1)) 	 return 1;

	// Small Sprites
	GfxDecode(0x40, 2, 16, 16, MarinebSmallSpriteCharPlane, MarinebSmallSpriteXOffs, MarinebSmallSpriteYOffs, 0x100, tmp, DrvGfxROM1);

	// Big Sprites
	GfxDecode(0x40, 2, 32, 32, MarinebBigSpriteCharPlane, MarinebBigSpriteXOffs, MarinebBigSpriteYOffs, 0x400, tmp, DrvGfxROM2);

	BurnFree(tmp);

	// ColorRoms
	if (BurnLoadRom(DrvColPROM, 8, 1)) return 1;
	if (BurnLoadRom(DrvColPROM + 0x100, 9, 1)) return 1;

	return 0;
}

static INT32 ChangesLoadRoms()
{
	// Load roms

	// Z80
	for (INT32 i = 0; i < 5; i++) {
		if (BurnLoadRom(DrvZ80ROM + (i * 0x1000), i, 1)) return 1;
	}

	UINT8 *tmp = (UINT8*)BurnMalloc(0x8000);
	if (tmp == NULL) return 1;

	// Chars
	memset(tmp, 0, 0x2000);
	if (BurnLoadRom(tmp, 5, 1)) return 1;
	GfxDecode(0x200, 2, 8, 8, MarinebCharPlane, MarinebCharXOffs, MarinebCharYOffs, 0x80, tmp, DrvGfxROM0);

	// Sprites
	memset(tmp, 0, 0x4000);
	if (BurnLoadRom(tmp, 6, 1))     return 1;

	// Small Sprites
	GfxDecode(0x40, 2, 16, 16, ChangesSmallSpriteCharPlane, ChangesSmallSpriteXOffs, ChangesSmallSpriteYOffs, 0x200, tmp, DrvGfxROM1);

	// Big Sprites
	GfxDecode(0xf, 2, 32, 32, ChangesBigSpriteCharPlane, ChangesBigSpriteXOffs, ChangesBigSpriteYOffs, 0x800, tmp + 0x1000, DrvGfxROM2);

	BurnFree(tmp);

	// ColorRoms
	if (BurnLoadRom(DrvColPROM, 7, 1)) return 1;
	if (BurnLoadRom(DrvColPROM + 0x100, 8, 1)) return 1;

	return 0;
}

static INT32 HoccerLoadRoms()
{
	// Load roms

	// Z80
	for (INT32 i = 0; i < 4; i++) {
		if (BurnLoadRom(DrvZ80ROM + (i * 0x2000), i, 1)) return 1;
	}

	UINT8 *tmp = (UINT8*)BurnMalloc(0x8000);
	if (tmp == NULL) return 1;

	// Chars
	memset(tmp, 0, 0x2000);
	if (BurnLoadRom(tmp, 4, 1)) return 1;
	GfxDecode(0x200, 2, 8, 8, MarinebCharPlane, MarinebCharXOffs, MarinebCharYOffs, 0x80, tmp, DrvGfxROM0);

	// Sprites
	memset(tmp, 0, 0x4000);
	if (BurnLoadRom(tmp, 5, 1))     return 1;

	// Small Sprites
	GfxDecode(0x40, 2, 16, 16, ChangesSmallSpriteCharPlane, ChangesSmallSpriteXOffs, ChangesSmallSpriteYOffs, 0x200, tmp, DrvGfxROM1);

	BurnFree(tmp);

	// ColorRoms
	if (BurnLoadRom(DrvColPROM, 6, 1)) return 1;
	if (BurnLoadRom(DrvColPROM + 0x100, 7, 1)) return 1;

	return 0;
}

static INT32 WantedLoadRoms()
{
	// Load roms

	// Z80
	for (INT32 i = 0; i < 3; i++) {
		if (BurnLoadRom(DrvZ80ROM + (i * 0x2000), i, 1)) return 1;
	}

	UINT8 *tmp = (UINT8*)BurnMalloc(0x8000);
	if (tmp == NULL) return 1;

	// Chars
	memset(tmp, 0, 0x4000);
	if (BurnLoadRom(tmp, 3, 1)) return 1;
	if (BurnLoadRom(tmp + 0x2000, 4, 1)) return 1;
	GfxDecode(0x400, 2, 8, 8, WantedCharPlane, WantedCharXOffs, WantedCharYOffs, 0x80, tmp, DrvGfxROM0);

	// Sprites
	memset(tmp, 0, 0x4000);
	if (BurnLoadRom(tmp, 5, 1))     return 1;
	if (BurnLoadRom(tmp + 0x2000, 6, 1))     return 1;

	// Small Sprites
	GfxDecode(0x40, 2, 16, 16, MarinebSmallSpriteCharPlane, MarinebSmallSpriteXOffs, MarinebSmallSpriteYOffs, 0x100, tmp, DrvGfxROM1);

	// Big Sprites
	GfxDecode(0x40, 2, 32, 32, MarinebBigSpriteCharPlane, MarinebBigSpriteXOffs, MarinebBigSpriteYOffs, 0x400, tmp, DrvGfxROM2);

	BurnFree(tmp);

	// ColorRoms
	if (BurnLoadRom(DrvColPROM, 7, 1)) return 1;
	if (BurnLoadRom(DrvColPROM + 0x100, 8, 1)) return 1;

	return 0;
}

static INT32 SpringerLoadRoms()
{
	// Load roms

	// Z80
	for (INT32 i = 0; i < 5; i++) {
		if (BurnLoadRom(DrvZ80ROM + (i * 0x1000), i, 1)) return 1;
	}

	UINT8 *tmp = (UINT8*)BurnMalloc(0x4000);
	if (tmp == NULL) return 1;

	// Chars
	memset(tmp, 0, 0x4000);
	if (BurnLoadRom(tmp, 5, 1)) return 1;
	if (BurnLoadRom(tmp + 0x1000, 6, 1)) return 1;

	GfxDecode(0x200, 2, 8, 8, MarinebCharPlane, MarinebCharXOffs, MarinebCharYOffs, 0x80, tmp, DrvGfxROM0);

	memset(tmp, 0, 0x4000);
	if (BurnLoadRom(tmp, 7, 1))     return 1;
	if (BurnLoadRom(tmp + 0x2000, 8, 1)) 	 return 1;

	// Small Sprites
	GfxDecode(0x40, 2, 16, 16, MarinebSmallSpriteCharPlane, MarinebSmallSpriteXOffs, MarinebSmallSpriteYOffs, 0x100, tmp, DrvGfxROM1);
	// Big Sprites
	GfxDecode(0x40, 2, 32, 32, MarinebBigSpriteCharPlane, MarinebBigSpriteXOffs, MarinebBigSpriteYOffs, 0x400, tmp, DrvGfxROM2);

	BurnFree(tmp);

	// ColorRoms
	if (BurnLoadRom(DrvColPROM, 9, 1)) return 1;
	if (BurnLoadRom(DrvColPROM + 0x100, 10, 1)) return 1;

	return 0;
}

static INT32 HopproboLoadRoms()
{
	// Load roms

	// Z80
	for (INT32 i = 0; i < 5; i++) {
		if (BurnLoadRom(DrvZ80ROM + (i * 0x1000), i, 1)) return 1;
	}

	UINT8 *tmp = (UINT8*)BurnMalloc(0x4000);
	if (tmp == NULL) return 1;

	// Chars
	memset(tmp, 0, 0x4000);
	if (BurnLoadRom(tmp, 5, 1)) return 1;
	if (BurnLoadRom(tmp + 0x2000, 6, 1)) return 1;
	if (BurnLoadRom(tmp + 0x3000, 6, 1)) return 1;

	GfxDecode(0x400, 2, 8, 8, MarinebCharPlane, MarinebCharXOffs, MarinebCharYOffs, 0x80, tmp, DrvGfxROM0);

	memset(tmp, 0, 0x4000);
	if (BurnLoadRom(tmp, 7, 1))     return 1;
	if (BurnLoadRom(tmp + 0x2000, 8, 1)) 	 return 1;

	// Small Sprites
	GfxDecode(0x40, 2, 16, 16, MarinebSmallSpriteCharPlane, MarinebSmallSpriteXOffs, MarinebSmallSpriteYOffs, 0x100, tmp, DrvGfxROM1);
	// Big Sprites
	GfxDecode(0x40, 2, 32, 32, MarinebBigSpriteCharPlane, MarinebBigSpriteXOffs, MarinebBigSpriteYOffs, 0x400, tmp, DrvGfxROM2);

	BurnFree(tmp);

	// ColorRoms
	if (BurnLoadRom(DrvColPROM, 9, 1)) return 1;
	if (BurnLoadRom(DrvColPROM + 0x100, 10, 1)) return 1;

	return 0;
}

static INT32 DrvInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	switch(hardware) {
		case MARINEB:
			MarinebLoadRoms();
			break;
		case SPRINGER:
			SpringerLoadRoms();
			break;
		case HOPPROBO:
			HopproboLoadRoms();
			break;
		case CHANGES:
			ChangesLoadRoms();
			break;
		case HOCCER:
			HoccerLoadRoms();
			break;
		case WANTED:
			WantedLoadRoms();
			break;
		case BCRUZM12:
			WantedLoadRoms();
			break;
	}

	DrvCreatePalette();

	ZetInit(0);
	ZetOpen(0);

	ZetMapArea (0x0000, 0x7fff, 0, DrvZ80ROM);
	ZetMapArea (0x0000, 0x7fff, 2, DrvZ80ROM);

	ZetMapArea (0x8000, 0x87ff, 0, DrvZ80RAM);
	ZetMapArea (0x8000, 0x87ff, 1, DrvZ80RAM);
	ZetMapArea (0x8000, 0x87ff, 2, DrvZ80RAM);

	ZetMapArea (0x8800, 0x8bff, 0, DrvVidRAM);
	ZetMapArea (0x8800, 0x8bff, 1, DrvVidRAM);
	ZetMapArea (0x8800, 0x8bff, 2, DrvVidRAM);

	ZetMapArea (0x8c00, 0x8c3f, 0, DrvSprRAM);
	ZetMapArea (0x8c00, 0x8c3f, 1, DrvSprRAM);
	ZetMapArea (0x8c00, 0x8c3f, 2, DrvSprRAM);

	ZetMapArea (0x9000, 0x93ff, 0, DrvColRAM);
	ZetMapArea (0x9000, 0x93ff, 1, DrvColRAM);
	ZetMapArea (0x9000, 0x93ff, 2, DrvColRAM);

	ZetSetReadHandler(marineb_read);
	ZetSetWriteHandler(marineb_write);
	ZetSetOutHandler(marineb_write_port);

	ZetClose();

	AY8910Init(0, 1500000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.15, BURN_SND_ROUTE_BOTH);
	AY8910Init(1, 1500000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(1, 0.15, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	DrvDoReset();

	return 0;

}

static INT32 DrvExit()
{
	GenericTilesExit();
	ZetExit();
	AY8910Exit(0);

	BurnFree (AllMem);

	CleanAndInitStuff();

	return 0;
}

static void RenderMarinebBg()
{
	INT32 TileIndex = 0;

	for (INT32 my = 0; my < 32; my++) {
		for (INT32 mx = 0; mx < 32; mx++) {

			TileIndex = (my * 32) + mx;

			INT32 code = DrvVidRAM[TileIndex];
		    INT32 color = DrvColRAM[TileIndex];

			INT32 flipx = (color >> 4) & 0x02;
			INT32 flipy = (color >> 4) & 0x01;

			/*if (hardware == MARINEB) {
				flipx = 0;
				flipy = 0;
			}*/

			code = (code | ((color & 0xc0) << 2)) & 0x1ff;
			color = ((color & 0x0f) | (DrvPaletteBank << 4)) & 0xff;


			INT32 x = mx << 3;
			INT32 y = my << 3;

			// stuff from 192 to 256 does not scroll
			if (((hardware == MARINEB) && (x >> 3) < 24) || ((hardware == CHANGES) && (x >> 3) < 26)) {
				y -= DrvColumnScroll;
				if (y < -7) y += 256;
			}

			y -= 16;

			if (flipy) {
				if (flipx) {
					Render8x8Tile_FlipXY_Clip(pTransDraw, code, x, y, color, 2, 0, DrvGfxROM0);
				} else {
					Render8x8Tile_FlipY_Clip(pTransDraw, code, x, y, color, 2, 0, DrvGfxROM0);
				}
			} else {
				if (flipx) {
					Render8x8Tile_FlipX_Clip(pTransDraw, code, x, y, color, 2, 0, DrvGfxROM0);
				} else {
					Render8x8Tile_Clip(pTransDraw, code, x, y, color, 2, 0, DrvGfxROM0);
				}
			}
		}
	}
}

static void RenderSpringerBg()
{
	INT32 TileIndex = 0;

	for (INT32 my = 0; my < 32; my++) {
		for (INT32 mx = 0; mx < 32; mx++) {

			TileIndex = (my * 32) + mx;

			INT32 code = DrvVidRAM[TileIndex];
		    INT32 color = DrvColRAM[TileIndex];

			INT32 flipx = (color >> 4) & 0x02;
			INT32 flipy = (color >> 4) & 0x01;

			code |= ((color & 0xc0) << 2);

			color &= 0x0f;
			color |= DrvPaletteBank << 4;

			INT32 x = mx << 3;
			INT32 y = my << 3;

			y -= 16; // remove garbage on left side

			if (flipy) {
				if (flipx) {
					Render8x8Tile_FlipXY_Clip(pTransDraw, code, x, y, color, 2, 0, DrvGfxROM0);
				} else {
					Render8x8Tile_FlipY_Clip(pTransDraw, code, x, y, color, 2, 0, DrvGfxROM0);
				}
			} else {
				if (flipx) {
					Render8x8Tile_FlipX_Clip(pTransDraw, code, x, y, color, 2, 0, DrvGfxROM0);
				} else {
					Render8x8Tile_Clip(pTransDraw, code, x, y, color, 2, 0, DrvGfxROM0);
				}
			}
		}
	}
}

static void RenderBcruz12mBg()
{
	INT32 TileIndex = 0;

	for (INT32 my = 0; my < 32; my++) {
		for (INT32 mx = 0; mx < 32; mx++) {

			TileIndex = (my * 32) + mx;

			INT32 code = DrvVidRAM[TileIndex];
		    INT32 color = DrvColRAM[TileIndex];

			INT32 flipx = (color >> 4) & 0x02;
			INT32 flipy = (color >> 4) & 0x01;

			code |= ((color & 0xc0) << 2);

			color &= 0x0f;
			color |= DrvPaletteBank << 4;

			INT32 x = mx << 3;
			INT32 y = my << 3;

			y -= 16; // remove garbage on left side

			if (flipy) {
				if (flipx) {
					Render8x8Tile_FlipXY_Clip(pTransDraw, code, x, y, color, 2, 0, DrvGfxROM0);
				} else {
					Render8x8Tile_FlipY_Clip(pTransDraw, code, x, y, color, 2, 0, DrvGfxROM0);
				}
			} else {
				if (flipx) {
					Render8x8Tile_FlipX_Clip(pTransDraw, code, x, y, color, 2, 0, DrvGfxROM0);
				} else {
					Render8x8Tile_Clip(pTransDraw, code, x, y, color, 2, 0, DrvGfxROM0);
				}
			}
		}
	}
}

static void RenderWantedBg()
{
	for (INT32 offs = 0x3ff; offs >= 0; offs--) {
		INT32 sx,sy,flipx,flipy,x,y;
		sx = offs % 32;
		sy = offs / 32;

		flipx = DrvColRAM[offs] & 0x20;
		flipy = DrvColRAM[offs] & 0x10;
		INT32 code = DrvVidRAM[offs] | ((DrvColRAM[offs] & 0xc0) << 2);
		INT32 color = (DrvColRAM[offs] & 0x0f) + 16 * DrvPaletteBank;

		if (DrvFlipScreenY)
		{
			sy = 31 - sy;
			flipy = !flipy;
		}

		if (DrvFlipScreenX)
		{
			sx = 31 - sx;
			flipx = !flipx;
		}
		x = 8*sx;
		y = 8*sy;
		y -=16;

		if (flipy) {
			if (flipx) {
				Render8x8Tile_FlipXY_Clip(pTransDraw, code, x, y, color, 2, 0, DrvGfxROM0);
			} else {
				Render8x8Tile_FlipY_Clip(pTransDraw, code, x, y, color, 2, 0, DrvGfxROM0);
			}
		} else {
			if (flipx) {
				Render8x8Tile_FlipX_Clip(pTransDraw, code, x, y, color, 2, 0, DrvGfxROM0);
			} else {
				Render8x8Tile_Clip(pTransDraw, code, x, y, color, 2, 0, DrvGfxROM0);
			}
		}
	}
}

static void MarinebDrawSprites()
{
	// Render Tiles
	RenderMarinebBg();

	for (INT32 offs = 0x0f; offs >= 0; offs--) {

		INT32 gfx, sx, sy, code, color, flipx, flipy, offs2;

		if ((offs == 0) || (offs == 2))
			continue;

		if (offs < 8) {
			offs2 = 0x0018 + offs;
		} else {
			offs2 = 0x03d8 - 8 + offs;
		}

		code = DrvVidRAM[offs2];
		sx = DrvVidRAM[offs2 + 0x20];
		sy = DrvColRAM[offs2];
		color = (DrvColRAM[offs2 + 0x20] & 0x0f) + 16 * DrvPaletteBank;
		flipx = code & 0x02;
		flipy = !(code & 0x01);

		if (offs < 4) {
			gfx = 2;
			code = (code >> 4) | ((code & 0x0c) << 2);
		} else {
			gfx = 1;
			code >>= 2;
		}

		if (!DrvFlipScreenY) {

			if (gfx == 1) {
				sy = 256 - 16 - sy;
			} else {
				sy = 256 - 32 - sy;
			}
			flipy = !flipy;
		}

		if (DrvFlipScreenX) {
			sx++;
		}

		sy -= 16; // proper alignement

		// Small Sprites
		if (gfx == 1) {
			if (flipy) {
				if (flipx) {
					Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM1);
				} else {
					Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM1);
				}
			} else {
				if (flipx) {
					Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM1);
				} else {
					Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM1);
				}
			}
		// Big sprites
		} else {
			if (flipy) {
				if (flipx) {
					Render32x32Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM2);
				} else {
					Render32x32Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM2);
				}
			} else {
				if (flipx) {
					Render32x32Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM2);
				} else {
					Render32x32Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM2);
				}
			}
		}
	}
}

static void ChangesDrawSprites()
{
	INT32 sx, sy, code, color, flipx, flipy, offs2;
	// Render Tiles
	RenderMarinebBg();

	for (INT32 offs = 0x05; offs >= 0; offs--) {
		offs2 = 0x001a + offs;

		code = DrvVidRAM[offs2];
		sx = DrvVidRAM[offs2 + 0x20];
		sy = DrvColRAM[offs2];
		color = ((DrvColRAM[offs2 + 0x20] & 0x0f) + 16 * DrvPaletteBank) & 0xff;
		flipx = (code & 0x02);
		flipy = !(code & 0x01);

		if (!DrvFlipScreenY) {
			sy = 256 - 16 - sy;
			flipy = !flipy;
		}

		if (DrvFlipScreenX) {
			sx++;
		}

		sy -= 16; // proper alignement
		code >>= 2 & 0x3f;

		// Small Sprites
		if (flipy) {
			if (flipx) {
				Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM1);
			} else {
				Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM1);
			}
		} else {
			if (flipx) {
				Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM1);
			} else {
				Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM1);
			}
		}
	}

	code = DrvVidRAM[0x3df];
	sx = DrvVidRAM[0x3ff];
	sy = DrvColRAM[0x3df];
	color = DrvColRAM[0x3ff] & 0xff;
	flipx = (code & 0x02);
	flipy = !(code & 0x01);

	if (!DrvFlipScreenY) {
		sy = 256 - 32 - sy;
		flipy = !flipy;
	}

	if (DrvFlipScreenX) {
		sx++;
	}

	code >>= 4 & 0xf;

	sy -= 16; // proper alignement

	// THE Big sprite
	if (flipy) {
		if (flipx) {
			Render32x32Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM2);
			Render32x32Tile_Mask_FlipXY_Clip(pTransDraw, code, sx-256, sy, color, 2, 0, 0, DrvGfxROM2);
		} else {
			Render32x32Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM2);
			Render32x32Tile_Mask_FlipY_Clip(pTransDraw, code, sx-256, sy, color, 2, 0, 0, DrvGfxROM2);
		}
	} else {
		if (flipx) {
			Render32x32Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM2);
			Render32x32Tile_Mask_FlipX_Clip(pTransDraw, code, sx-256, sy, color, 2, 0, 0, DrvGfxROM2);
		} else {
			Render32x32Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM2);
			Render32x32Tile_Mask_Clip(pTransDraw, code, sx-256, sy, color, 2, 0, 0, DrvGfxROM2);
		}
	}
}

static void HoccerDrawSprites()
{
	INT32 sx, sy, code, color, flipx, flipy, offs2;
	// Render Tiles
	RenderMarinebBg();

	for (INT32 offs = 0x07; offs >= 0; offs--) {
		offs2 = 0x0018 + offs;

		code = DrvSprRAM[offs2];
		sx = DrvSprRAM[offs2 + 0x20];
		sy = DrvColRAM[offs2];
		color = (DrvColRAM[offs2 + 0x20] & 0xff);
		flipx = (code & 0x02);
		flipy = !(code & 0x01);

		if (!DrvFlipScreenY) {
			sy = 256 - 16 - sy;
			flipy = !flipy;
		}

		if (DrvFlipScreenX) {
			sx = 256 - 16 - sx;
			flipx = !flipx;
		}

		sy -= 16; // proper alignement
		code >>= 2 & 0x3f;

		// Small Sprites
		if (flipy) {
			if (flipx) {
				Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM1);
			} else {
				Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM1);
			}
		} else {
			if (flipx) {
				Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM1);
			} else {
				Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM1);
			}
		}
	}
}

static void SpringerDrawSprites()
{
	// Render Tiles
	RenderSpringerBg();

	for (INT32 offs = 0x0f; offs >= 0; offs--)
	{
		INT32 gfx, sx, sy, code, color, flipx, flipy, offs2;

		if ((offs == 0) || (offs == 2))
			continue;

		offs2 = 0x0010 + offs;

		code = DrvVidRAM[offs2];
		sx = 240 - DrvVidRAM[offs2 + 0x20];
		sy = DrvColRAM[offs2];
		color = (DrvColRAM[offs2 + 0x20] & 0x0f) + 16 * DrvPaletteBank;
		flipx = !(code & 0x02);
		flipy = !(code & 0x01);

		if (offs < 4)
		{
			sx -= 0x10;
			gfx = 2;
			code = (code >> 4) | ((code & 0x0c) << 2);
		}
		else
		{
			gfx = 1;
			code >>= 2;
		}

		if (((hardware == SPRINGER) && !DrvFlipScreenY) || (((hardware == HOPPROBO) || (hardware == BCRUZM12)) && DrvFlipScreenY)) {

			if (gfx == 1) {
				sy = 256 - 16 - sy;
			} else {
				sy = 256 - 32 - sy;
			}
			flipy = !flipy;
		}

		if (!DrvFlipScreenX)
		{
			sx--;
		}

		sy -= 16; // proper alignement

		// Small Sprites
		if (gfx == 1) {
			if (flipy) {
				if (flipx) {
					Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM1);
				} else {
					Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM1);
				}
			} else {
				if (flipx) {
					Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM1);
				} else {
					Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM1);
				}
			}
		// Big Sprites
		} else {
			if (flipy) {
				if (flipx) {
					Render32x32Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM2);
				} else {
					Render32x32Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM2);
				}
			} else {
				if (flipx) {
					Render32x32Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM2);
				} else {
					Render32x32Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM2);
				}
			}
		}
	}
}

static void Bcruz12mDrawSprites()
{
	// Render Tiles
	RenderBcruz12mBg();

	for (INT32 offs = 0x0f; offs >= 0; offs--)
	{
		INT32 gfx, sx, sy, code, color, flipx, flipy, offs2;

		if ((offs == 0) || (offs == 2))
			continue;

		offs2 = 0x0010 + offs;

		code = DrvVidRAM[offs2];
		sx = /*240 - */DrvVidRAM[offs2 + 0x20];
		sy = DrvColRAM[offs2];
		color = (DrvColRAM[offs2 + 0x20] & 0x0f) + 16 * DrvPaletteBank;
		flipx = (code & 0x02);
		flipy = !(code & 0x01);

		if (offs < 4)
		{
			//sx -= 0x10;
			gfx = 2;
			code = (code >> 4) | ((code & 0x0c) << 2);
		}
		else
		{
			gfx = 1;
			code >>= 2;
		}

		if (!DrvFlipScreenY) {

			if (gfx == 1) {
				sy = 256 - 16 - sy;
			} else {
				sy = 256 - 32 - sy;
			}
			flipy = !flipy;
		}

		if (!DrvFlipScreenX)
		{
			sx--;
		}

		sy -= 16; // proper alignement

		// Small Sprites
		if (gfx == 1) {
			if (flipy) {
				if (flipx) {
					Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM1);
				} else {
					Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM1);
				}
			} else {
				if (flipx) {
					Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM1);
				} else {
					Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM1);
				}
			}
		// Big Sprites
		} else {
			if (flipy) {
				if (flipx) {
					Render32x32Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM2);
				} else {
					Render32x32Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM2);
				}
			} else {
				if (flipx) {
					Render32x32Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM2);
				} else {
					Render32x32Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM2);
				}
			}
		}
	}
}

static void WantedDrawSprites()
{
	// Render Tiles
	RenderWantedBg();

	for (INT32 offs = 0x0f; offs >= 0; offs--)
	{
		INT32 gfx, sx, sy, code, color, flipx, flipy, offs2;

		if ((offs == 0) || (offs == 2))
			continue;

		offs2 = 0x0010 + offs;

		code = DrvVidRAM[offs2];
		sx = DrvVidRAM[offs2 + 0x20];
		sy = DrvColRAM[offs2];
		color = (DrvColRAM[offs2 + 0x20] & 0x0f) + 16 * DrvPaletteBank;
		flipx = (code & 0x02);
		flipy = !(code & 0x01);

		if (offs < 4)
		{
			gfx = 2;
			code = (code >> 4) | ((code & 0x0c) << 2);
		}
		else
		{
			gfx = 1;
			code >>= 2;
		}

		if (!DrvFlipScreenY) {

			if (gfx == 1) {
				sy = 256 - 16 - sy;
			} else {
				sy = 256 - 32 - sy;
			}
			flipy = !flipy;
		}

		if (DrvFlipScreenX)
		{
			if (gfx == 1) {
				sx = 256 - 16 - sx;
			} else {
				sx = 256 - 32 - sx;
			}
			flipx = !flipx;
			sx--;
		} else {
			if (sx >= 240) continue; // don't draw sprites in the statusbar
		}



		sy -= 16; // proper alignement

		// Small Sprites
		if (gfx == 1) {
			if (flipy) {
				if (flipx) {
					Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM1);
				} else {
					Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM1);
				}
			} else {
				if (flipx) {
					Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM1);
				} else {
					Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM1);
				}
			}
		// Big Sprites
		} else {
			if (flipy) {
				if (flipx) {
					Render32x32Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM2);
				} else {
					Render32x32Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM2);
				}
			} else {
				if (flipx) {
					Render32x32Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM2);
				} else {
					Render32x32Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM2);
				}
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalcPalette) {
		DrvCreatePalette();
		DrvRecalcPalette = 0;
	}
	BurnTransferClear();
	switch(hardware) {
		case CHANGES:
			ChangesDrawSprites();
			break;

		case HOCCER:
			HoccerDrawSprites();
			break;

		case MARINEB:
			MarinebDrawSprites();
			break;

		case BCRUZM12:
			Bcruz12mDrawSprites();
			break;

		case WANTED:
			WantedDrawSprites();
			break;

		case SPRINGER:
		case HOPPROBO:
			SpringerDrawSprites();
			break;
	}

	BurnTransferCopy(DrvPalette);
	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	DrvInput[0] = DrvInput[1] = 0;
	if (hardware != WANTED && hardware != BCRUZM12)
		 DrvInput[2] = 0;

	for (INT32 i = 0; i < 8; i++) {
		DrvInput[0] |= (DrvInputPort0[i] & 1) << i;
		DrvInput[1] |= (DrvInputPort1[i] & 1) << i;
		if (hardware != WANTED && hardware != BCRUZM12)
			DrvInput[2] |= (DrvInputPort2[i] & 1) << i;
	}

	INT32 nInterleave = 256;

	ZetOpen(0);
	for (INT32 i = 0; i < nInterleave; i++) {
		ZetRun(3072000 / 60 / nInterleave);

		if (i == nInterleave - 1) {
			if (DrvInterruptEnable) {
				switch (hardware) {
					case WANTED:
					case BCRUZM12:
						ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
						break;
					default:
						ZetNmi();
						break;
				}
			}
		}
	}
	ZetClose();

	if (pBurnSoundOut) {
		AY8910Render(&pAY8910Buffer[0], pBurnSoundOut, nBurnSoundLen, 0);
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
		*pnMin = 0x029736;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = RamStart;
		ba.nLen	  = RamEnd - RamStart;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);
		AY8910Scan(nAction, pnMin);

		SCAN_VAR(DrvPaletteBank);
		SCAN_VAR(DrvColumnScroll);
		SCAN_VAR(DrvFlipScreenY);
		SCAN_VAR(DrvFlipScreenX);
		SCAN_VAR(DrvInterruptEnable);
	}

	return 0;
}

static INT32 MarinebInit()
{
	CleanAndInitStuff();
	hardware = MARINEB;

	return DrvInit();
}

static INT32 SpringerInit()
{
	CleanAndInitStuff();

	ActiveLowFlipscreen = 1;
	hardware = SPRINGER;

	return DrvInit();
}

static INT32 hopproboInit()
{
	CleanAndInitStuff();

	hardware = HOPPROBO;

	return DrvInit();
}

static INT32 changesInit()
{
	CleanAndInitStuff();

	hardware = CHANGES;

	return DrvInit();
}

static INT32 hoccerInit()
{
	CleanAndInitStuff();

	hardware = HOCCER;

	return DrvInit();
}

static INT32 wantedInit()
{
	CleanAndInitStuff();

	ActiveLowFlipscreen = 1;
	hardware = WANTED;

	return DrvInit();
}

static INT32 bcruzm12Init()
{
	CleanAndInitStuff();

	hardware = BCRUZM12;

	return DrvInit();
}

//  Marine Boy

static struct BurnRomInfo MarinebRomDesc[] = {

	{ "marineb.1", 0x1000, 0x661d6540, BRF_ESS | BRF_PRG }, // 0 maincpu
	{ "marineb.2", 0x1000, 0x922da17f, BRF_ESS | BRF_PRG }, // 1
	{ "marineb.3", 0x1000, 0x820a235b, BRF_ESS | BRF_PRG }, // 2
	{ "marineb.4", 0x1000, 0xa157a283, BRF_ESS | BRF_PRG }, // 3
	{ "marineb.5", 0x1000, 0x9ffff9c0, BRF_ESS | BRF_PRG }, // 4

	{ "marineb.6", 0x2000, 0xee53ec2e, BRF_GRA }, // 5 gfx1

	{ "marineb.8", 0x2000, 0xdc8bc46c, BRF_GRA }, // 6 gfx2
	{ "marineb.7", 0x2000, 0x9d2e19ab, BRF_GRA }, // 7

	{ "marineb.1b", 0x100, 0xf32d9472, BRF_GRA }, // 8 proms
	{ "marineb.1c", 0x100, 0x93c69d3e, BRF_GRA }, // 9
};

STD_ROM_PICK(Marineb)
STD_ROM_FN(Marineb)

struct BurnDriver BurnDrvMarineb = {
	"marineb", NULL, NULL, NULL, "1982",
	"Marine Boy\0", NULL, "Orca", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, MarinebRomInfo, MarinebRomName, NULL, NULL, MarinebInputInfo, MarinebDIPInfo,
	MarinebInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalcPalette, 0x100, 256, 224, 4, 3
};

//  Springer

static struct BurnRomInfo SpringerRomDesc[] = {

	{ "springer.1", 0x1000, 0x0794103a, BRF_ESS | BRF_PRG }, // 0 maincpu
	{ "springer.2", 0x1000, 0xf4aecd9a, BRF_ESS | BRF_PRG }, // 1
	{ "springer.3", 0x1000, 0x2f452371, BRF_ESS | BRF_PRG }, // 2
	{ "springer.4", 0x1000, 0x859d1bf5, BRF_ESS | BRF_PRG }, // 3
	{ "springer.5", 0x1000, 0x72adbbe3, BRF_ESS | BRF_PRG }, // 4

	{ "springer.6", 0x1000, 0x6a961833, BRF_GRA }, // 5 gfx1
	{ "springer.7", 0x1000, 0x95ab8fc0, BRF_GRA }, // 6

	{ "springer.8", 0x1000, 0xa54bafdc, BRF_GRA }, // 7 gfx2
	{ "springer.9", 0x1000, 0xfa302775, BRF_GRA }, // 8

	{ "1b.vid", 0x100, 0xa2f935aa, BRF_GRA }, // 9 proms
	{ "1c.vid", 0x100, 0xb95421f4, BRF_GRA }, // 10
};

STD_ROM_PICK(Springer)
STD_ROM_FN(Springer)

struct BurnDriver BurnDrvSpringer = {
	"springer", NULL, NULL, NULL, "1982",
	"Springer\0", NULL, "Orca", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, SpringerRomInfo, SpringerRomName, NULL, NULL, MarinebInputInfo, MarinebDIPInfo,
	SpringerInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalcPalette, 0x100, 224, 256, 3, 4
};

//  Hopper Robo

static struct BurnRomInfo hopproboRomDesc[] = {
	{ "hopper01.3k",	0x1000, 0xfd7935c0, 1 | BRF_ESS | BRF_PRG }, //  0 maincpu
	{ "hopper02.3l",	0x1000, 0xdf1a479a, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "hopper03.3n",	0x1000, 0x097ac2a7, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "hopper04.3p",	0x1000, 0x0f4f3ca8, 1 | BRF_ESS | BRF_PRG }, //  3
	{ "hopper05.3r",	0x1000, 0x9d77a37b, 1 | BRF_ESS | BRF_PRG }, //  4

	{ "hopper06.5c",	0x2000, 0x68f79bc8, 2 | BRF_GRA }, //  5 gfx1
	{ "hopper07.5d",	0x1000, 0x33d82411, 2 | BRF_GRA }, //  6

	{ "hopper09.6k",	0x2000, 0x047921c7, 3 | BRF_GRA }, //  7 gfx2
	{ "hopper08.6f",	0x2000, 0x06d37e64, 3 | BRF_GRA }, //  8

	{ "7052hop.1b",		0x0100, 0x94450775, 4 | BRF_GRA }, //  9 proms
	{ "7052hop.1c",		0x0100, 0xa76bbd51, 4 | BRF_GRA }, // 10
};

STD_ROM_PICK(hopprobo)
STD_ROM_FN(hopprobo)

struct BurnDriver BurnDrvhopprobo = {
	"hopprobo", NULL, NULL, NULL, "1983",
	"Hopper Robo\0", NULL, "Sega", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, hopproboRomInfo, hopproboRomName, NULL, NULL, MarinebInputInfo, MarinebDIPInfo,
	hopproboInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalcPalette, 0x100, 224, 256, 3, 4
};

// Changes

static struct BurnRomInfo changesRomDesc[] = {
	{ "changes.1",	0x1000, 0x56f83813, 1 | BRF_ESS | BRF_PRG }, //  0 maincpu
	{ "changes.2",	0x1000, 0x0e627f0b, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "changes.3",	0x1000, 0xff8291e9, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "changes.4",	0x1000, 0xa8e9aa22, 1 | BRF_ESS | BRF_PRG }, //  3
	{ "changes.5",	0x1000, 0xf4198e9e, 1 | BRF_ESS | BRF_PRG }, //  4

	{ "changes.7",	0x2000, 0x2204194e, 2 | BRF_GRA }, //  5 gfx1

	{ "changes.6",	0x2000, 0x985c9db4, 3 | BRF_GRA }, //  6 gfx2

	{ "changes.1b",	0x0100, 0xf693c153, 4 | BRF_GRA }, //  7 proms
	{ "changes.1c",	0x0100, 0xf8331705, 4 | BRF_GRA }, //  8
};

STD_ROM_PICK(changes)
STD_ROM_FN(changes)

struct BurnDriver BurnDrvChanges = {
	"changes", NULL, NULL, NULL, "1982",
	"Changes\0", NULL, "Orca", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, changesRomInfo, changesRomName, NULL, NULL, ChangesInputInfo, ChangesDIPInfo,
	changesInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalcPalette, 0x100, 256, 224, 4, 3
};


// Changes (EME license)

static struct BurnRomInfo changesaRomDesc[] = {
	{ "changes3.1",	0x1000, 0xff80cad7, 1 | BRF_ESS | BRF_PRG }, //  0 maincpu
	{ "changes.2",	0x1000, 0x0e627f0b, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "changes3.3",	0x1000, 0x359bf7e1, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "changes.4",	0x1000, 0xa8e9aa22, 1 | BRF_ESS | BRF_PRG }, //  3
	{ "changes3.5",	0x1000, 0xc197e64a, 1 | BRF_ESS | BRF_PRG }, //  4

	{ "changes.7",	0x2000, 0x2204194e, 2 | BRF_GRA }, //  5 gfx1

	{ "changes.6",	0x2000, 0x985c9db4, 3 | BRF_GRA }, //  6 gfx2

	{ "changes.1b",	0x0100, 0xf693c153, 4 | BRF_GRA }, //  7 proms
	{ "changes.1c",	0x0100, 0xf8331705, 4 | BRF_GRA }, //  8
};

STD_ROM_PICK(changesa)
STD_ROM_FN(changesa)

struct BurnDriver BurnDrvChangesa = {
	"changesa", "changes", NULL, NULL, "1982",
	"Changes (EME license)\0", NULL, "Orca (Eastern Micro Electronics, Inc. license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, changesaRomInfo, changesaRomName, NULL, NULL, ChangesInputInfo, ChangesDIPInfo,
	changesInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalcPalette, 0x100, 256, 224, 4, 3
};

// Looper

static struct BurnRomInfo looperRomDesc[] = {
	{ "changes.1",		0x1000, 0x56f83813, 1 | BRF_ESS | BRF_PRG }, //  0 maincpu
	{ "changes.2",		0x1000, 0x0e627f0b, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "changes.3",		0x1000, 0xff8291e9, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "changes.4",		0x1000, 0xa8e9aa22, 1 | BRF_ESS | BRF_PRG }, //  3
	{ "changes.5",		0x1000, 0xf4198e9e, 1 | BRF_ESS | BRF_PRG }, //  4

	{ "looper_7.bin",	0x2000, 0x71a89975, 2 | BRF_GRA }, //  5 gfx1

	{ "looper_6.bin",	0x2000, 0x1f3f70c2, 3 | BRF_GRA }, //  6 gfx2

	{ "changes.1b",		0x0100, 0xf693c153, 4 | BRF_GRA }, //  7 proms
	{ "changes.1c",		0x0100, 0xf8331705, 4 | BRF_GRA }, //  8
};

STD_ROM_PICK(looper)
STD_ROM_FN(looper)

struct BurnDriver BurnDrvLooper = {
	"looper", "changes", NULL, NULL, "1982",
	"Looper\0", NULL, "Orca", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, looperRomInfo, looperRomName, NULL, NULL, ChangesInputInfo, ChangesDIPInfo,
	changesInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalcPalette, 0x100, 256, 224, 4, 3
};

// Hoccer (set 1)

static struct BurnRomInfo hoccerRomDesc[] = {
	{ "hr1.cpu",	0x2000, 0x12e96635, 1 | BRF_ESS | BRF_PRG }, //  0 maincpu
	{ "hr2.cpu",	0x2000, 0xcf1fc328, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "hr3.cpu",	0x2000, 0x048a0659, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "hr4.cpu",	0x2000, 0x9a788a2c, 1 | BRF_ESS | BRF_PRG }, //  3

	{ "hr.d",	0x2000, 0xd33aa980, 2 | BRF_GRA }, //  4 gfx1

	{ "hr.c",	0x2000, 0x02808294, 3 | BRF_GRA }, //  5 gfx2

	{ "hr.1b",	0x0100, 0x896521d7, 4 | BRF_GRA }, //  6 proms
	{ "hr.1c",	0x0100, 0x2efdd70b, 4 | BRF_GRA }, //  7
};

STD_ROM_PICK(hoccer)
STD_ROM_FN(hoccer)

struct BurnDriver BurnDrvHoccer = {
	"hoccer", NULL, NULL, NULL, "1983",
	"Hoccer (set 1)\0", NULL, "Eastern Micro Electronics, Inc.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, hoccerRomInfo, hoccerRomName, NULL, NULL, HoccerInputInfo, HoccerDIPInfo,
	hoccerInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalcPalette, 0x100, 224, 256, 3, 4
};


// Hoccer (set 2)

static struct BurnRomInfo hoccer2RomDesc[] = {
	{ "hr.1",	0x2000, 0x122d159f, 1 | BRF_ESS | BRF_PRG }, //  0 maincpu
	{ "hr.2",	0x2000, 0x48e1efc0, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "hr.3",	0x2000, 0x4e67b0be, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "hr.4",	0x2000, 0xd2b44f58, 1 | BRF_ESS | BRF_PRG }, //  3

	{ "hr.d",	0x2000, 0xd33aa980, 2 | BRF_GRA }, //  4 gfx1

	{ "hr.c",	0x2000, 0x02808294, 3 | BRF_GRA }, //  5 gfx2

	{ "hr.1b",	0x0100, 0x896521d7, 4 | BRF_GRA }, //  6 proms
	{ "hr.1c",	0x0100, 0x2efdd70b, 4 | BRF_GRA }, //  7
};

STD_ROM_PICK(hoccer2)
STD_ROM_FN(hoccer2)

struct BurnDriver BurnDrvHoccer2 = {
	"hoccer2", "hoccer", NULL, NULL, "1983",
	"Hoccer (set 2)\0", NULL, "Eastern Micro Electronics, Inc.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, hoccer2RomInfo, hoccer2RomName, NULL, NULL, HoccerInputInfo, HoccerDIPInfo,
	hoccerInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalcPalette, 0x100, 224, 256, 3, 4
};


// Wanted

static struct BurnRomInfo wantedRomDesc[] = {
	{ "prg-1",	0x2000, 0x2dd90aed, 1 | BRF_ESS | BRF_PRG }, //  0 maincpu
	{ "prg-2",	0x2000, 0x67ac0210, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "prg-3",	0x2000, 0x373c7d82, 1 | BRF_ESS | BRF_PRG }, //  2

	{ "vram-1",	0x2000, 0xc4226e54, 2 | BRF_GRA }, //  3 gfx1
	{ "vram-2",	0x2000, 0x2a9b1e36, 2 | BRF_GRA }, //  4

	{ "obj-a",	0x2000, 0x90b60771, 3 | BRF_GRA }, //  5 gfx2
	{ "obj-b",	0x2000, 0xe14ee689, 3 | BRF_GRA }, //  6

	{ "wanted.k7",	0x0100, 0x2ba90a00, 4 | BRF_GRA }, //  7 proms
	{ "wanted.k6",	0x0100, 0xa93d87cc, 4 | BRF_GRA }, //  8
};

STD_ROM_PICK(wanted)
STD_ROM_FN(wanted)

struct BurnDriver BurnDrvWanted = {
	"wanted", NULL, NULL, NULL, "1984",
	"Wanted\0", NULL, "Sigma Enterprises Inc.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, wantedRomInfo, wantedRomName, NULL, NULL, WantedInputInfo, WantedDIPInfo,
	wantedInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalcPalette, 0x100, 224, 256, 3, 4
};


// Battle Cruiser M-12

static struct BurnRomInfo bcruzm12RomDesc[] = {
	{ "d-84_3.12c",		0x2000, 0x132baa3d, 1 | BRF_ESS | BRF_PRG }, //  0 maincpu
	{ "d-84_2.12b",		0x2000, 0x1a788d1f, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "d-84_1.12ab",	0x2000, 0x9d5b3017, 1 | BRF_ESS | BRF_PRG }, //  2

	{ "d-84_5.17f",		0x2000, 0x2e963f6a, 2 | BRF_GRA }, //  3 gfx1
	{ "d-84_4.17ef",	0x2000, 0xfe186459, 2 | BRF_GRA }, //  4

	{ "d-84_6.17fh",	0x2000, 0x1337dc01, 3 | BRF_GRA }, //  5 gfx2
	{ "d-84_7.17h",		0x2000, 0xa5be90ef, 3 | BRF_GRA }, //  6
	
	{ "bcm12col.7k",	0x0100, 0xbf4f2671, 4 | BRF_GRA }, //  7 proms
	{ "bcm12col.6k",	0x0100, 0x59f955f6, 4 | BRF_GRA }, //  8
};

STD_ROM_PICK(bcruzm12)
STD_ROM_FN(bcruzm12)

struct BurnDriver BurnDrvBcruzm12 = {
	"bcruzm12", NULL, NULL, NULL, "1983",
	"Battle Cruiser M-12\0", NULL, "Sigma Enterprises Inc.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, bcruzm12RomInfo, bcruzm12RomName, NULL, NULL, Bcruzm12InputInfo, Bcruzm12DIPInfo,
	bcruzm12Init, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalcPalette, 0x100, 224, 256, 3, 4
};
