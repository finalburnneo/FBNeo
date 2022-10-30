// FB Neo Penguin-Kun Wars Driver Module
// Based on MAME Driver by David Haywood and Phil Stroffolino

#include "tiles_generic.h"
#include "z80_intf.h"
#include "ay8910.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvMainROM;
static UINT8 *DrvSubROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvColPROM;
static UINT8 *DrvBgRAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvMainRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvSubRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvReset;
static UINT8 DrvInputs[8];

static HoldCoin<2> hold_coin;

static INT32 flipscreen;
static INT32 xscroll;
static INT32 yscroll;
static INT32 vblank;
static INT32 watchdog;
static UINT8 ninjakun_ioctrl;

static struct BurnInputInfo PkunwarInputList[] = {
	{"P1 Coin", 		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 coin"	},
	{"P1 Start",    	BIT_DIGITAL,	DrvJoy1 + 5,	"p1 start"	},
	{"P1 Left", 		BIT_DIGITAL,	DrvJoy1 + 0, 	"p1 left" 	},
	{"P1 Right", 		BIT_DIGITAL,	DrvJoy1 + 1, 	"p1 right"	},
	{"P1 Button 1", 	BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 1"	},

	{"P2 Start",    	BIT_DIGITAL,	DrvJoy2 + 5,	"p2 start"	},
	{"P2 Left", 		BIT_DIGITAL,	DrvJoy2 + 0, 	"p2 left" 	},
	{"P2 Right", 		BIT_DIGITAL,	DrvJoy2 + 1, 	"p2 right"	},
	{"P2 Button 1", 	BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 1"	},

	{"Reset",	  		BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip 1",	  		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip 2",	  		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Pkunwar)

static struct BurnInputInfo Nova2001InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 2"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Nova2001)

static struct BurnInputInfo NinjakunInputList[] = {
	{"P1 coin",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 start"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 start"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Ninjakun)

static struct BurnInputInfo Raiders5InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Raiders5)

static struct BurnDIPInfo PkunwarDIPList[]=
{
	{0x0a, 0xff, 0xff, 0xfb, NULL 			},
	{0x0b, 0xff, 0xff, 0x40, NULL 			},

	{0   , 0xfe, 0   , 4   , "Coinage"   	},
	{0x0a, 0x01, 0x03, 0x00, "3 Coins 1 Credit"		},
	{0x0a, 0x01, 0x03, 0x02, "2 Coins 1 Credit"		},
	{0x0a, 0x01, 0x03, 0x03, "1 Coin 1 Credit"		},
	{0x0a, 0x01, 0x03, 0x01, "1 Coin 2 Credits"		},

	{0   , 0xfe, 0   , 2   , "Cabinet"		},
	{0x0a, 0x01, 0x04, 0x00, "Upright"		},
	{0x0a, 0x01, 0x04, 0x04, "Cocktail"		},

	{0   , 0xfe, 0   , 2   , "Demo Sounds"		},
	{0x0a, 0x01, 0x08, 0x00, "Off"			},
	{0x0a, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   , 4   , "Difficulty"		},
	{0x0a, 0x01, 0x30, 0x10, "Easy"			},
	{0x0a, 0x01, 0x30, 0x30, "Medium"		},
	{0x0a, 0x01, 0x30, 0x20, "Hard"			},
	{0x0a, 0x01, 0x30, 0x00, "Hardest"		},

	{0   , 0xfe, 0   , 2   , "Flip screen"		},
	{0x0a, 0x01, 0x40, 0x40, "Off"			},
	{0x0a, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   , 2   , "Free Play"		},
	{0x0a, 0x01, 0x80, 0x80, "Off"			},
	{0x0a, 0x01, 0x80, 0x00, "On"			},

    {0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x0b, 0x01, 0x40, 0x40, "Off"			},
	{0x0b, 0x01, 0x40, 0x00, "On"			},
};

STDDIPINFO(Pkunwar)

static struct BurnDIPInfo Nova2001DIPList[]=
{
	{0x10, 0xff, 0xff, 0xfe, NULL			},
	{0x11, 0xff, 0xff, 0xf8, NULL			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x10, 0x01, 0x01, 0x00, "Upright"		},
	{0x10, 0x01, 0x01, 0x01, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x10, 0x01, 0x02, 0x02, "3"			},
	{0x10, 0x01, 0x02, 0x00, "4"			},

	{0   , 0xfe, 0   ,    2, "1st Bonus Life"	},
	{0x10, 0x01, 0x04, 0x04, "20K"			},
	{0x10, 0x01, 0x04, 0x00, "30K"			},

	{0   , 0xfe, 0   ,    4, "Extra Bonus Life"	},
	{0x10, 0x01, 0x18, 0x18, "60K"			},
	{0x10, 0x01, 0x18, 0x10, "70K"			},
	{0x10, 0x01, 0x18, 0x08, "90K"			},
	{0x10, 0x01, 0x18, 0x00, "None"			},

	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{0x10, 0x01, 0x60, 0x40, "2 Coins 1 Credits"	},
	{0x10, 0x01, 0x60, 0x00, "2 Coins 2 Credits"	},
	{0x10, 0x01, 0x60, 0x60, "1 Coin  1 Credits"	},
	{0x10, 0x01, 0x60, 0x20, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x10, 0x01, 0x80, 0x80, "Off"			},
	{0x10, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x11, 0x01, 0x03, 0x00, "Easy"			},
	{0x11, 0x01, 0x03, 0x03, "Medium"		},
	{0x11, 0x01, 0x03, 0x02, "Hard"			},
	{0x11, 0x01, 0x03, 0x01, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x11, 0x01, 0x04, 0x04, "Off"			},
	{0x11, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "High Score Names"	},
	{0x11, 0x01, 0x08, 0x00, "3 Letters"		},
	{0x11, 0x01, 0x08, 0x08, "8 Letters"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x11, 0x01, 0x80, 0x80, "Off"			},
	{0x11, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Nova2001)

static struct BurnDIPInfo NinjakunDIPList[]=
{
	{0x0c, 0xff, 0xff, 0xac, NULL			},
	{0x0d, 0xff, 0xff, 0xcf, NULL			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0c, 0x01, 0x01, 0x00, "Upright"		},
	{0x0c, 0x01, 0x01, 0x01, "Cocktail"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0c, 0x01, 0x06, 0x02, "2"			},
	{0x0c, 0x01, 0x06, 0x04, "3"			},
	{0x0c, 0x01, 0x06, 0x06, "4"			},
	{0x0c, 0x01, 0x06, 0x00, "5"			},

	{0   , 0xfe, 0   ,    2, "First Bonus"		},
	{0x0c, 0x01, 0x08, 0x08, "30000"		},
	{0x0c, 0x01, 0x08, 0x00, "40000"		},

	{0   , 0xfe, 0   ,    4, "Second Bonus"		},
	{0x0c, 0x01, 0x30, 0x00, "No Bonus"		},
	{0x0c, 0x01, 0x30, 0x10, "Every 30000"		},
	{0x0c, 0x01, 0x30, 0x30, "Every 50000"		},
	{0x0c, 0x01, 0x30, 0x20, "Every 70000"		},

	{0   , 0xfe, 0   ,    2, "Demo_Sounds"		},
	{0x0c, 0x01, 0x40, 0x40, "Off"			},
	{0x0c, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x0c, 0x01, 0x80, 0x80, "Normal"		},
	{0x0c, 0x01, 0x80, 0x00, "Hard"			},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x0d, 0x01, 0x07, 0x04, "4C_1C"		},
	{0x0d, 0x01, 0x07, 0x05, "3C_1C"		},
	{0x0d, 0x01, 0x07, 0x00, "4C_2C"		},
	{0x0d, 0x01, 0x07, 0x06, "2C_1C"		},
	{0x0d, 0x01, 0x07, 0x01, "3C_2C"		},
	{0x0d, 0x01, 0x07, 0x02, "2C_2C"		},
	{0x0d, 0x01, 0x07, 0x07, "1C_1C"		},
	{0x0d, 0x01, 0x07, 0x03, "1C_2C"		},

	{0   , 0xfe, 0   ,    2, "High Score Names"	},
	{0x0d, 0x01, 0x08, 0x00, "3 Letters"		},
	{0x0d, 0x01, 0x08, 0x08, "8 Letters"		},

	{0   , 0xfe, 0   ,    2, "Allow_Continue"	},
	{0x0d, 0x01, 0x10, 0x10, "No"			},
	{0x0d, 0x01, 0x10, 0x00, "Yes"			},

	{0   , 0xfe, 0   ,    2, "Free_Play"		},
	{0x0d, 0x01, 0x40, 0x40, "Off"			},
	{0x0d, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Infinite Lives"	},
	{0x0d, 0x01, 0x80, 0x80, "Off"			},
	{0x0d, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Ninjakun)

static struct BurnDIPInfo Raiders5DIPList[]=
{
	{0x0e, 0xff, 0xff, 0xfe, NULL                             },
	{0x0f, 0xff, 0xff, 0xff, NULL                             },

	{0   , 0xfe, 0   ,    2, "Cabinet"                        },
	{0x0e, 0x01, 0x01, 0x00, "Upright"                        },
	{0x0e, 0x01, 0x01, 0x01, "Cocktail"                       },

	{0   , 0xfe, 0   ,    4, "Lives"                          },
	{0x0e, 0x01, 0x06, 0x00, "2"                              },
	{0x0e, 0x01, 0x06, 0x06, "3"                              },
	{0x0e, 0x01, 0x06, 0x04, "4"                              },
	{0x0e, 0x01, 0x06, 0x02, "5"                              },

	{0   , 0xfe, 0   ,    2, "1st Bonus"                      },
	{0x0e, 0x01, 0x08, 0x08, "30000"                          },
	{0x0e, 0x01, 0x08, 0x00, "40000"                          },

	{0   , 0xfe, 0   ,    4, "2nd Bonus"                      },
	{0x0e, 0x01, 0x30, 0x30, "Every 50000"                    },
	{0x0e, 0x01, 0x30, 0x20, "Every 70000"                    },
	{0x0e, 0x01, 0x30, 0x10, "Every 90000"                    },
	{0x0e, 0x01, 0x30, 0x00, "None"                           },

	{0   , 0xfe, 0   ,    2, "Exercise"                       },
	{0x0e, 0x01, 0x40, 0x00, "Off"                            },
	{0x0e, 0x01, 0x40, 0x40, "On"                             },

	{0   , 0xfe, 0   ,    2, "Difficulty"                     },
	{0x0e, 0x01, 0x80, 0x80, "Normal"                         },
	{0x0e, 0x01, 0x80, 0x00, "Hard"                           },

	{0   , 0xfe, 0   ,    8, "Coinage"                        },
	{0x0f, 0x01, 0x07, 0x04, "4 Coins 1 Credits"              },
	{0x0f, 0x01, 0x07, 0x05, "3 Coins 1 Credits"              },
	{0x0f, 0x01, 0x07, 0x00, "4 Coins 2 Credits"              },
	{0x0f, 0x01, 0x07, 0x06, "2 Coins 1 Credits"              },
	{0x0f, 0x01, 0x07, 0x01, "3 Coins 2 Credits"              },
	{0x0f, 0x01, 0x07, 0x02, "2 Coins 2 Credits"              },
	{0x0f, 0x01, 0x07, 0x07, "1 Coin  1 Credits"              },
	{0x0f, 0x01, 0x07, 0x03, "1 Coin  2 Credits"              },

	{0   , 0xfe, 0   ,    2, "High Score Names"               },
	{0x0f, 0x01, 0x08, 0x00, "3 Letters"                      },
	{0x0f, 0x01, 0x08, 0x08, "8 Letters"                      },

	{0   , 0xfe, 0   ,    2, "Allow Continue"                 },
	{0x0f, 0x01, 0x10, 0x00, "No"                             },
	{0x0f, 0x01, 0x10, 0x10, "Yes"                            },

	{0   , 0xfe, 0   ,    0, "Free Play"                      },
	{0x0f, 0x01, 0x40, 0x40, "Off"                            },
	{0x0f, 0x01, 0x40, 0x00, "On"                             },

	{0   , 0xfe, 0   ,    2, "Unlimited Lives (If Free Play)" },
	{0x0f, 0x01, 0x80, 0x80, "Off"                            },
	{0x0f, 0x01, 0x80, 0x00, "On"                             },
};

STDDIPINFO(Raiders5)

static struct BurnDIPInfo Raiders5taDIPList[] =
{
	{0x0e, 0xff, 0xff, 0xfe, NULL                             },
	{0x0f, 0xff, 0xff, 0xff, NULL                             },

	{0   , 0xfe, 0   ,    2, "Cabinet"                        },
	{0x0e, 0x01, 0x01, 0x00, "Upright"                        },
	{0x0e, 0x01, 0x01, 0x01, "Cocktail"                       },

	{0   , 0xfe, 0   ,    4, "Lives"                          },
	{0x0e, 0x01, 0x06, 0x00, "2"                              },
	{0x0e, 0x01, 0x06, 0x06, "3"                              },
	{0x0e, 0x01, 0x06, 0x04, "4"                              },
	{0x0e, 0x01, 0x06, 0x02, "5"                              },

	{0   , 0xfe, 0   ,    2, "1st Bonus"                      },
	{0x0e, 0x01, 0x08, 0x08, "30000"                          },
	{0x0e, 0x01, 0x08, 0x00, "40000"                          },

	{0   , 0xfe, 0   ,    4, "2nd Bonus"                      },
	{0x0e, 0x01, 0x30, 0x30, "Every 50000"                    },
	{0x0e, 0x01, 0x30, 0x20, "Every 70000"                    },
	{0x0e, 0x01, 0x30, 0x10, "Every 90000"                    },
	{0x0e, 0x01, 0x30, 0x00, "None"                           },

	{0   , 0xfe, 0   ,    2, "Swap Controls + Flip Screen"    },
	{0x0e, 0x01, 0x40, 0x00, "On"                             },
	{0x0e, 0x01, 0x40, 0x40, "off"                            },

	{0   , 0xfe, 0   ,    2, "Difficulty"                     },
	{0x0e, 0x01, 0x80, 0x80, "Normal"                         },
	{0x0e, 0x01, 0x80, 0x00, "Hard"                           },

	{0   , 0xfe, 0   ,    8, "Coinage"                        },
	{0x0f, 0x01, 0x07, 0x04, "4 Coins 1 Credits"              },
	{0x0f, 0x01, 0x07, 0x05, "3 Coins 1 Credits"              },
	{0x0f, 0x01, 0x07, 0x00, "4 Coins 2 Credits"              },
	{0x0f, 0x01, 0x07, 0x06, "2 Coins 1 Credits"              },
	{0x0f, 0x01, 0x07, 0x01, "3 Coins 2 Credits"              },
	{0x0f, 0x01, 0x07, 0x02, "2 Coins 2 Credits"              },
	{0x0f, 0x01, 0x07, 0x07, "1 Coin  1 Credits"              },
	{0x0f, 0x01, 0x07, 0x03, "1 Coin  2 Credits"              },

	{0   , 0xfe, 0   ,    2, "High Score Names"               },
	{0x0f, 0x01, 0x08, 0x00, "3 Letters"                      },
	{0x0f, 0x01, 0x08, 0x08, "8 Letters"                      },

	{0   , 0xfe, 0   ,    2, "Allow Continue"                 },
	{0x0f, 0x01, 0x10, 0x00, "No"                             },
	{0x0f, 0x01, 0x10, 0x10, "Yes"                            },

	{0   , 0xfe, 0   ,    0, "Free Play"                      },
	{0x0f, 0x01, 0x40, 0x40, "Off"                            },
	{0x0f, 0x01, 0x40, 0x00, "On"                             },

	{0   , 0xfe, 0   ,    2, "Unlimited Lives (If Free Play)" },
	{0x0f, 0x01, 0x80, 0x80, "Off"                            },
	{0x0f, 0x01, 0x80, 0x00, "On"                             },
};

STDDIPINFO(Raiders5ta)

static UINT8 __fastcall raiders5_main_read(UINT16 address)
{
	if(address >= 0x9000 && address <= 0x97ff)
	{
		return DrvBgRAM[(((address & 0x3ff) + (xscroll >> 3) + ((yscroll >> 3) << 5)) & 0x3ff) + (address & 0x400)];
	}

	if(address >= 0xd000 && address <= 0xd1ff)
	{
		return DrvPalRAM[address - 0xd000];
	}

	switch (address)
	{
		case 0xc001:
			return AY8910Read(0);

		case 0xc003:
			return AY8910Read(1);
	}

	return 0;
}

static void __fastcall raiders5_main_write(UINT16 address, UINT8 data)
{
	if(address >= 0x9000 && address <= 0x97ff)
	{
		DrvBgRAM[(((address & 0x3ff) + (xscroll >> 3) + ((yscroll >> 3) << 5)) & 0x3ff) + (address & 0x400)] = data;
		return;
	}

	if(address >= 0xd000 && address <= 0xd1ff)
	{
		INT32 offset = address - 0xd000;

		DrvPalRAM[offset] = data;
		if (offset < 16) {
			DrvPalRAM[0x200 + offset * 16 + 1] = data;

			if (offset != 1) {
				for (INT32 i = 0; i < 16; i++) {
					DrvPalRAM[0x200 + offset + i * 16] = data;
				}
			}
		}
		return;
	}


	switch (address)
	{
		case 0xa000:
			xscroll = data;
			break;

		case 0xa001:
			yscroll = data;
			break;

		case 0xc000:
			AY8910Write(0, 0, data);
			break;

		case 0xc001:
			AY8910Write(0, 1, data);
			break;

		case 0xc002:
			AY8910Write(1, 0, data);
			break;

		case 0xc003:
			AY8910Write(1, 1, data);
			break;

		case 0xa002:
			flipscreen = ~data & 0x01;
			break;
	}
}

static UINT8 __fastcall raiders5_sub_read(UINT16 address)
{
	switch (address)
	{
		case 0x8001:
			return AY8910Read(0);

		case 0x8003:
			return AY8910Read(1);

		case 0x9000:
		case 0xc000:
		case 0xc800:
		case 0xd000:
			return 0; // NOP
	}

	return 0;
}

static UINT8 __fastcall raiders5_in(UINT16 /*address*/)
{
	return 0; // NOP
}

static void __fastcall raiders5_sub_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x8000:
			AY8910Write(0, 0, data);
			break;

		case 0x8001:
			AY8910Write(0, 1, data);
			break;

		case 0x8002:
			AY8910Write(1, 0, data);
			break;

		case 0x8003:
			AY8910Write(1, 1, data);
			break;

		case 0xe000:
			xscroll = data;
			break;

		case 0xe001:
			yscroll = data;
			break;

		case 0xe002:
			flipscreen = ~data & 0x01;
			break;
	}
}

static UINT8 __fastcall pkunwar_read(UINT16 address)
{
	switch (address)
	{
		case 0xa001:
			return AY8910Read(0);
		break;

		case 0xa003:
			return AY8910Read(1);
		break;
	}

	return 0;
}

static void __fastcall pkunwar_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xa000:
			AY8910Write(0, 0, data);
		break;

		case 0xa001:
			AY8910Write(0, 1, data);
		break;

		case 0xa002:
			AY8910Write(1, 0, data);
		break;

		case 0xa003:
			AY8910Write(1, 1, data);
		break;
	}
}

static void __fastcall pkunwar_out(UINT16 address, UINT8 data)
{
	address &= 0xff;

	if (address == 0) flipscreen = data & 1;
}

static UINT8 __fastcall nova2001_read(UINT16 address)
{
	switch (address)
	{
		case 0xc000:
			return AY8910Read(0);

		case 0xc001:
			return AY8910Read(1);

		case 0xc004:
			watchdog = 0;
			return 0;

		case 0xc006:
			return DrvInputs[0];

		case 0xc007:
			return DrvInputs[1];

		case 0xc00e:
			return (DrvInputs[2] & 0x7f) | vblank;
	}

	return 0;
}

static void __fastcall nova2001_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xbfff:
			flipscreen = ~data & 0x01;
			break;

		case 0xc000:
			AY8910Write(0, 1, data);
			break;

		case 0xc002:
			AY8910Write(0, 0, data);
			break;

		case 0xc001:
			AY8910Write(1, 1, data);
			break;

		case 0xc003:
			AY8910Write(1, 0, data);
			break;
	}
}


static UINT8 __fastcall ninjakun_main_read(UINT16 address)
{
	if ((address & 0xf800) == 0xc800) {
		return DrvBgRAM[(((address & 0x3ff) + (xscroll >> 3) + ((yscroll >> 3) << 5)) & 0x3ff) + (address & 0x400)];
	}

	switch (address)
	{
		case 0x8001:
			return AY8910Read(0);

		case 0x8003:
			return AY8910Read(1);

		case 0xa000:
			return DrvInputs[0] ^ 0xc0;

		case 0xa001:
			return DrvInputs[1] ^ 0xc0;

		case 0xa002:
			return ((vblank ? 0 : 2) | (ninjakun_ioctrl << 2));
	}

	return 0;
}

static void __fastcall ninjakun_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf800) == 0xc800) {
		DrvBgRAM[(((address & 0x3ff) + (xscroll >> 3) + ((yscroll >> 3) << 5)) & 0x3ff) + (address & 0x400)] = data;
		return;
	}

	switch (address)
	{
		case 0x8000:
			AY8910Write(0, 0, data);
			break;

		case 0x8001:
			AY8910Write(0, 1, data);
			break;

		case 0x8002:
			AY8910Write(1, 0, data);
			break;

		case 0x8003:
			AY8910Write(1, 1, data);
			break;

		case 0xa002:
			if (data == 0x80) ninjakun_ioctrl |=  0x01;
			if (data == 0x40) ninjakun_ioctrl &= ~0x02;
			break;

		case 0xa003:
			flipscreen = ~data & 0x01;
			break;
	}
}

static UINT8 __fastcall ninjakun_sub_read(UINT16 address)
{
	if ((address & 0xf800) == 0xc800) {
		return DrvBgRAM[(((address & 0x3ff) + (xscroll >> 3) + ((yscroll >> 3) << 5)) & 0x3ff) + (address & 0x400)];
	}

	switch (address)
	{
		case 0x8001:
			return AY8910Read(0);

		case 0x8003:
			return AY8910Read(1);

		case 0xa000:
			return DrvInputs[0] ^ 0xc0;

		case 0xa001:
			return DrvInputs[1] ^ 0xc0;

		case 0xa002:
			return ((vblank ? 0 : 2) | (ninjakun_ioctrl << 2));
	}

	return 0;
}

static void __fastcall ninjakun_sub_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf800) == 0xc800) {
		DrvBgRAM[(((address & 0x3ff) + (xscroll >> 3) + ((yscroll >> 3) << 5)) & 0x3ff) + (address & 0x400)] = data;
		return;
	}

	switch (address)
	{
		case 0x8000:
			AY8910Write(0, 0, data);
			break;

		case 0x8001:
			AY8910Write(0, 1, data);
			break;

		case 0x8002:
			AY8910Write(1, 0, data);
			break;

		case 0x8003:
			AY8910Write(1, 1, data);
			break;

		case 0xa002:
			if (data == 0x40) ninjakun_ioctrl |=  0x02;
			if (data == 0x80) ninjakun_ioctrl &= ~0x01;
			break;

		case 0xa003:
		//	flipscreen = ~data & 0x01;
			break;
	}
}

static UINT8 pkunwar_port_0(UINT32)
{
	return (DrvInputs[0] & 0x7f) | (vblank ? 0 : 0x80);
}

static UINT8 pkunwar_port_1(UINT32)
{
	return (DrvInputs[1] & ~0x40) | (DrvDips[1] & 0x40);
}

static UINT8 pkunwar_port_2(UINT32)
{
	return 0xff;
}

static UINT8 pkunwar_port_3(UINT32)
{
	return DrvDips[0];
}

void nova2001_scroll_x_w(UINT32 /*offset*/,UINT32 data)
{
	xscroll = data;
}

void nova2001_scroll_y_w(UINT32 /*offset*/,UINT32 data)
{
	yscroll = data;
}

static UINT8 nova2001_port_3(UINT32)
{
	return DrvDips[0];
}

static UINT8 nova2001_port_4(UINT32)
{
	return DrvDips[1];
}

static UINT8 raiders5_port_0(UINT32)
{
	return (DrvInputs[0] & 0x7f) | (vblank ? 0 : 0x80);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	AY8910Reset(0);
	AY8910Reset(1);

	hold_coin.reset();

	HiscoreReset();

	flipscreen = 0;
	watchdog = 0;

	xscroll = 0;
	yscroll = 0;
	ninjakun_ioctrl = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvMainROM		= Next; Next += 0x010000;
	DrvSubROM		= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += 0x020000;
	DrvGfxROM1		= Next; Next += 0x020000;
	DrvGfxROM2		= Next; Next += 0x020000;

	DrvPalette		= (UINT32*)Next; Next += 0x0300 * sizeof(UINT32);

	DrvColPROM		= Next; Next += 0x000020;

	AllRam			= Next;

	DrvBgRAM		= Next; Next += 0x000800;

	DrvFgRAM		= Next; Next += 0x000800;

	DrvSprRAM		= Next; Next += 0x000800;

	DrvMainRAM		= Next; Next += 0x000800;

	DrvSubRAM		= Next; Next += 0x000800;

	DrvPalRAM		= Next; Next += 0x000300;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void pkunwar_palette_init()
{
	for (INT32 i = 0; i < 0x200; i++)
	{
		INT32 shift = ((i & 0x0f) == 1) ? 4 : 0;
		INT32 entry = ((i >> shift) & 0xf) | ((i & 0x100) >> 4);

		INT32 intensity = DrvColPROM[entry] & 0x03;

		INT32 r = (((DrvColPROM[entry] >> 0) & 0x0c) | intensity) * 0x11;
		INT32 g = (((DrvColPROM[entry] >> 2) & 0x0c) | intensity) * 0x11;
		INT32 b = (((DrvColPROM[entry] >> 4) & 0x0c) | intensity) * 0x11;

		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}
}

static void DrvGfxDecode(UINT8 *src, UINT8 *dst, INT32 type)
{
	INT32 Planes[4]    = { STEP4(0,1) };
	INT32 XOffsets[16] = { STEP8(0,4), STEP8(256,4) };
	INT32 YOffsets[16] = { STEP8(0,32), STEP8(512,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x10000);

	memcpy (tmp, src, 0x10000);

	if (type == 0) {
		GfxDecode(0x800, 4,  8,  8, Planes, XOffsets, YOffsets, 0x100, tmp, dst);
	} else {
		GfxDecode(0x200, 4, 16, 16, Planes, XOffsets, YOffsets, 0x400, tmp, dst);
	}

	BurnFree(tmp);
}

static void DrvGfxDescramble(UINT8 *gfx)
{
	UINT8 *tmp = (UINT8*)BurnMalloc(0x10000);
	if (tmp == NULL) {
		return;
	}

	memcpy (tmp, gfx, 0x10000);

	for (INT32 i = 0; i < 0x10000; i++)
	{
		gfx[(i & ~0x3fff) | ((i << 1) & 0x3fff) | ((i >> 13) & 1)] = tmp[i];
	}

	BurnFree(tmp);
}

static INT32 PkunwarInit()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvMainROM + 0x0000, 0, 1)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x4000, 1, 1)) return 1;
		if (BurnLoadRom(DrvMainROM + 0xe000, 2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x0000, 3, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x4000, 4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x8000, 5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0xc000, 6, 1)) return 1;

		if (BurnLoadRom(DrvColPROM, 7, 1)) return 1;

		DrvGfxDescramble(DrvGfxROM0);

		DrvGfxDecode(DrvGfxROM0, DrvGfxROM1, 1);
		DrvGfxDecode(DrvGfxROM0, DrvGfxROM0, 0);

		pkunwar_palette_init();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetSetOutHandler(pkunwar_out);
	ZetSetReadHandler(pkunwar_read);
	ZetSetWriteHandler(pkunwar_write);
	ZetMapMemory(DrvMainROM + 0x0000,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvSprRAM,			0x8000, 0x87ff, MAP_RAM);
	ZetMapMemory(DrvBgRAM,			0x8800, 0x8fff, MAP_RAM);
	ZetMapMemory(DrvMainRAM,		0xc000, 0xc7ff, MAP_RAM);
	ZetMapMemory(DrvMainROM + 0xe000,	0xe000, 0xffff, MAP_ROM);
	ZetClose();

	AY8910Init(0, 1500000, 0);
	AY8910Init(1, 1500000, 1);
	AY8910SetPorts(0, &pkunwar_port_0, &pkunwar_port_1, NULL, NULL);
	AY8910SetPorts(1, &pkunwar_port_2, &pkunwar_port_3, NULL, NULL);
	AY8910SetAllRoutes(0, 0.25, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.25, BURN_SND_ROUTE_BOTH);
	AY8910SetBuffered(ZetTotalCycles, 3000000);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 NovaInit()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvMainROM + 0x0000, 0, 1)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x2000, 1, 1)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x4000, 2, 1)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x6000, 3, 1)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x7000, 3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x0000, 4, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0001, 5, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x4000, 6, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x4001, 7, 2)) return 1;

		if (BurnLoadRom(DrvColPROM, 8, 1)) return 1;

		DrvGfxDecode(DrvGfxROM0, DrvGfxROM1, 1);
		DrvGfxDecode(DrvGfxROM0, DrvGfxROM0, 0);

		pkunwar_palette_init();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetSetReadHandler(nova2001_read);
	ZetSetWriteHandler(nova2001_write);

	ZetMapMemory(DrvMainROM,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvFgRAM,			0xa000, 0xa7ff, MAP_RAM);
	ZetMapMemory(DrvBgRAM,			0xa800, 0xafff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0xb000, 0xb7ff, MAP_RAM);
	ZetMapMemory(DrvMainRAM,		0xe000, 0xe7ff, MAP_RAM);
	ZetClose();

	AY8910Init(0, 2000000, 0);
	AY8910Init(1, 2000000, 1);
	AY8910SetPorts(0, NULL, NULL, &nova2001_scroll_x_w, &nova2001_scroll_y_w);
	AY8910SetPorts(1, &nova2001_port_3, &nova2001_port_4, NULL, NULL);
	AY8910SetAllRoutes(0, 0.20, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.20, BURN_SND_ROUTE_BOTH);
	AY8910SetBuffered(ZetTotalCycles, 3000000);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 NinjakunDoReset()
{
	DrvDoReset();

	ZetOpen(1);
	ZetReset();
	ZetClose();

	return 0;
}

static INT32 NinjakunInit()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvMainROM + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x2000,  1, 1)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x4000,  2, 1)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x6000,  3, 1)) return 1;

		if (BurnLoadRom(DrvSubROM  + 0x0000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x0000,  5, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0001,  6, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x4000,  7, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x4001,  8, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x0000,  9, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x0001, 10, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x4000, 11, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x4001, 12, 2)) return 1;

		DrvGfxDecode(DrvGfxROM0, DrvGfxROM1, 1);
		DrvGfxDecode(DrvGfxROM0, DrvGfxROM0, 0);

		DrvGfxDecode(DrvGfxROM2, DrvGfxROM2, 0);
	}

	ZetInit(0);
	ZetOpen(0);
	ZetSetReadHandler(ninjakun_main_read);
	ZetSetWriteHandler(ninjakun_main_write);
	ZetMapMemory(DrvMainROM,		0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvMainROM + 0x2000,	0x2000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvFgRAM,			0xc000, 0xc7ff, MAP_RAM);
//	ZetMapMemory(DrvBgRAM,			0xc800, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvPalRAM,			0xd800, 0xd9ff, MAP_RAM);
	ZetMapMemory(DrvMainRAM + 0x0000,	0xe000, 0xe3ff, MAP_RAM);
	ZetMapMemory(DrvMainRAM + 0x0400,	0xe400, 0xe7ff, MAP_RAM);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetSetReadHandler(ninjakun_sub_read);
	ZetSetWriteHandler(ninjakun_sub_write);
	ZetMapMemory(DrvSubROM,			0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvMainROM + 0x2000,	0x2000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvFgRAM,			0xc000, 0xc7ff, MAP_RAM);
//	ZetMapMemory(DrvBgRAM,			0xc800, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvPalRAM,			0xd800, 0xd9ff, MAP_RAM);
	ZetMapMemory(DrvMainRAM + 0x0400,	0xe000, 0xe3ff, MAP_RAM);
	ZetMapMemory(DrvMainRAM + 0x0000,	0xe400, 0xe7ff, MAP_RAM);
	ZetClose();

	AY8910Init(0, 3000000, 0);
	AY8910Init(1, 3000000, 1);
	AY8910SetPorts(1, NULL, NULL, &nova2001_scroll_x_w, &nova2001_scroll_y_w);
	AY8910SetPorts(0, &nova2001_port_3, &nova2001_port_4, NULL, NULL);
	AY8910SetAllRoutes(0, 0.20, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.20, BURN_SND_ROUTE_BOTH);
	AY8910SetBuffered(ZetTotalCycles, 3000000);

	GenericTilesInit();

	NinjakunDoReset();

	return 0;
}

static INT32 Raiders5Init()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvMainROM + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x4000,  1, 1)) return 1;

		if (BurnLoadRom(DrvSubROM  + 0x0000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x0000,  3, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x4000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x0000,  5, 1)) return 1;

		DrvGfxDescramble(DrvGfxROM0);
		DrvGfxDescramble(DrvGfxROM2);
		DrvGfxDecode(DrvGfxROM0, DrvGfxROM1, 1);
		DrvGfxDecode(DrvGfxROM0, DrvGfxROM0, 0);

		DrvGfxDecode(DrvGfxROM2, DrvGfxROM2, 0);
	}

	ZetInit(0);
	ZetOpen(0);
	ZetSetInHandler(raiders5_in);
	ZetSetReadHandler(raiders5_main_read);
	ZetSetWriteHandler(raiders5_main_write);

	ZetMapMemory(DrvMainROM,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvSprRAM,			0x8000, 0x87ff, MAP_RAM);
	ZetMapMemory(DrvFgRAM,			0x8800, 0x8fff, MAP_RAM);
	ZetMapMemory(DrvMainRAM + 0x0000,	0xe000, 0xe7ff, MAP_RAM);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetSetInHandler(raiders5_in); // a verifier
	ZetSetReadHandler(raiders5_sub_read);
	ZetSetWriteHandler(raiders5_sub_write);
	ZetMapMemory(DrvSubROM,			0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvMainRAM + 0x0000,	0xa000, 0xa7ff, MAP_RAM);
	ZetClose();

	AY8910Init(0, 1500000, 0);
	AY8910Init(1, 1500000, 1);
	AY8910SetPorts(0, &raiders5_port_0, &pkunwar_port_1, NULL, NULL);
	AY8910SetPorts(1, &nova2001_port_3, &nova2001_port_4, NULL, NULL);
	AY8910SetAllRoutes(0, 0.25, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.25, BURN_SND_ROUTE_BOTH);
	AY8910SetBuffered(ZetTotalCycles, 3000000);

	GenericTilesInit();

	NinjakunDoReset();

	return 0;
}

static INT32 DrvExit()
{
	ZetExit();
	AY8910Exit(0);
	AY8910Exit(1);

	GenericTilesExit();

	BurnFreeMemIndex();

	return 0;
}

static void draw_layer(UINT8 *ram_base, UINT8 *gfx_base, INT32 config, INT32 color_base, INT32 priority)
{
	INT32 color_shift = 0;
	INT32 code_extend = -1;
	INT32 code_extend_shift = 0;
	INT32 group_select_bit = -1;
	INT32 transparent = 0xff; // opaque
	INT32 enable_scroll = 0;
	INT32 color_mask = 0x0f;
	INT32 xskew = 0;

	switch (config)
	{
		case 0: // nova2001 background
			enable_scroll = 1;
		break;

		case 1: // nova2001 foreground
			group_select_bit = 4;
			transparent = 0;
		break;

		case 2: // ninjakun background
			code_extend = 3;
			code_extend_shift = 6;
			enable_scroll = 1;
		break;

		case 3: // ninjakun foreground
			code_extend = 1;
			code_extend_shift = 5;
			transparent = 0;
		break;

		case 4: // pkunwar background
			code_extend = 7;
			code_extend_shift = 0;
			color_shift = 4;
			color_mask = 0xf0;
		break;

		case 5: // pkunwar foreground (background + transparency + group)
			code_extend = 7;
			code_extend_shift = 0;
			color_shift = 4;
			group_select_bit = 3;
			transparent = 0;
			color_mask = 0xf0;
		break;

		case 6: // raiders5 background   <--- something is wrong here(?) -dink
			code_extend = 1;
			code_extend_shift = 0;
			color_shift = 4;
			enable_scroll = 1;
			color_mask = 0x0f;
			xskew = 7;
		break;

		case 7: // raiders5 foreground
			color_shift = 4;
			transparent = 0;
			color_mask = 0xf0;
		break;
	}

	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sx = (offs & 0x1f) * 8;
		INT32 sy = (offs / 0x20) * 8;

		sy -= 32; // all games y offset by 32 pixels

		if (enable_scroll) {
			sx -= xscroll;
			sy -= yscroll;
		}

		if (sy < -7) sy += 256;
		if (sx < -7) sx += 256;
		if (sx >= nScreenWidth) continue;
		if (sy >= nScreenHeight) continue;
		sx += xskew;

		INT32 code = ram_base[offs + 0x000];
		INT32 attr = ram_base[offs + 0x400];

		INT32 color = (attr & color_mask) >> color_shift;

		INT32 group = 0;

		if (group_select_bit != -1) {
			group = (attr >> group_select_bit) & 1;

			if (group != priority) continue;
		}

		if (code_extend != -1) code |= ((attr >> code_extend_shift) & code_extend) << 8;

		if (config == 6) {//hack. -dink
			code = ram_base[offs + 0x000] + ((attr & 0x01) << 8);
			color = (attr >> 4) & 0x0f;
		}

		if (flipscreen) {
			Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, code, 248 - sx, 184 - sy, color, 4, transparent, color_base, gfx_base);
		} else {
			Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 4, transparent, color_base, gfx_base);
		}
	}
}

static void pkunwar_draw_sprites(INT32 color_base)
{
	for (INT32 offs = 0; offs < 0x800; offs += 32)
	{
		INT32 attr = DrvSprRAM[offs+3];
		INT32 flipx = DrvSprRAM[offs+0] & 0x01;
		INT32 flipy = DrvSprRAM[offs+0] & 0x02;
		INT32 sx = DrvSprRAM[offs+1];
		INT32 sy = DrvSprRAM[offs+2];
		INT32 code = ((DrvSprRAM[offs+0] & 0xfc) >> 2) + ((attr & 0x07) << 6);
		INT32 color = (attr & 0xf0) >> 4;

		if (attr & 0x08) continue;

		if (flipscreen)
		{
			sx = 240 - sx;
			sy = 240 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		sy -= 32; // all games y offset by 32 pixels

		Draw16x16MaskTile(pTransDraw, code, sx, sy, flipx, flipy, color, 4, 0, color_base, DrvGfxROM1);
		// wrap around
		Draw16x16MaskTile(pTransDraw, code, sx - 256, sy, flipx, flipy, color, 4, 0, color_base, DrvGfxROM1);
	}
}

static void nova_draw_sprites(INT32 color_base)
{
	for (INT32 offs = 0; offs < 0x800; offs += 32)
	{
		INT32 attr = DrvSprRAM[offs+3];
		INT32 flipx = attr & 0x10;
		INT32 flipy = attr & 0x20;
		INT32 sx = DrvSprRAM[offs+1] - ((attr & 0x40) << 2);
		INT32 sy = DrvSprRAM[offs+2];
		INT32 code = DrvSprRAM[offs+0];
		INT32 color = attr & 0x0f;

		if (attr & 0x80) continue;

		if (flipscreen)
		{
			sx = 240 - sx;
			sy = 240 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		sy -= 32; // all games y offset by 32 pixels

		Draw16x16MaskTile(pTransDraw, code, sx, sy, flipx, flipy, color, 4, 0, color_base, DrvGfxROM1);
	}
}

static INT32 NovaDraw()
{
	if (DrvRecalc) {
		pkunwar_palette_init();
		DrvRecalc = 0;
	}

//	BurnTransferClear();

	draw_layer(DrvBgRAM, DrvGfxROM0 + 0x8000, 0, 0x100, 0);

	nova_draw_sprites(0x000);

	draw_layer(DrvFgRAM, DrvGfxROM0 + 0x0000, 1, 0x000, 0);
	draw_layer(DrvFgRAM, DrvGfxROM0 + 0x0000, 1, 0x000, 1);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 PkunwarDraw()
{
	if (DrvRecalc) {
		pkunwar_palette_init();
		DrvRecalc = 0;
	}

//	BurnTransferClear();

	draw_layer(DrvBgRAM, DrvGfxROM0 + 0x0000, 4, 0x100, 0);

	pkunwar_draw_sprites(0);

	draw_layer(DrvBgRAM, DrvGfxROM0 + 0x0000, 5, 0x100, 1);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void DrvPalRAMUpdate(INT32 ninjakun)
{
	if (ninjakun) {
		for (INT32 i = 0; i < 16; i++) {
			if (i != 1) { // ??
				for (INT32 j = 0; j < 16; j++) {
					DrvPalRAM[0x200 + i + j * 16 + 0] = DrvPalRAM[i];
				}
			}
			DrvPalRAM[0x200 + i * 16 + 1] = DrvPalRAM[i];
		}
	}

	for (INT32 i = 0; i < 0x300; i++) {
		INT32 intensity = DrvPalRAM[i] & 0x03;

		INT32 r = (((DrvPalRAM[i] >> 0) & 0x0c) | intensity) * 0x11;
		INT32 g = (((DrvPalRAM[i] >> 2) & 0x0c) | intensity) * 0x11;
		INT32 b = (((DrvPalRAM[i] >> 4) & 0x0c) | intensity) * 0x11;

		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}
}

static INT32 Raiders5Draw()
{
	DrvPalRAMUpdate(0);

	BurnTransferClear();

	draw_layer(DrvBgRAM, DrvGfxROM2 + 0x0000, 6, 0x100, 0);

	pkunwar_draw_sprites(0x200);

	draw_layer(DrvFgRAM, DrvGfxROM0 + 0x0000, 7, 0x000, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 NinjakunDraw()
{
	DrvPalRAMUpdate(1);

	BurnTransferClear();

	if (nBurnLayer & 1) draw_layer(DrvBgRAM, DrvGfxROM2 + 0x0000, 2, 0x100, 0);

	if (nBurnLayer & 2) draw_layer(DrvFgRAM, DrvGfxROM0 + 0x0000, 3, 0x000, 1);

	if (nBurnLayer & 4) nova_draw_sprites(0x200);

	if (nBurnLayer & 8) draw_layer(DrvFgRAM, DrvGfxROM0 + 0x0000, 3, 0x000, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 NovaFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}
	watchdog++;

	{
		memset (DrvInputs, 0xff, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		hold_coin.checklow(0, DrvInputs[2], 1 << 0, 4);
	}

	vblank = 0;
	ZetNewFrame();

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[1] = { 3000000 / 60 };
	INT32 nCyclesDone[1]  = { 0 };

	ZetOpen(0);
	for (INT32 i = 0; i < nInterleave; i++) {
		CPU_RUN(0, Zet);

		if (i == 240) {
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
			vblank = 0x80;
		}
	}
	ZetClose();

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		NovaDraw();
	}

	return 0;
}

static INT32 PkunwarFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	memset (DrvInputs, 0xff, 2);

	for (INT32 i = 0; i < 8; i++) {
		DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
		DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
	}

	vblank = 0;
	ZetNewFrame();

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[1] = { 3000000 / 60 };
	INT32 nCyclesDone[1]  = { 0 };

	ZetOpen(0);
	for (INT32 i = 0; i < nInterleave; i++) {
		CPU_RUN(0, Zet);

		if (i == 240) {
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
			vblank = 1;
		}
	}
	ZetClose();

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		PkunwarDraw();
	}

	return 0;
}

static INT32 Raiders5Frame()
{
	if (DrvReset) {
		NinjakunDoReset();
	}
	watchdog++;

	memset (DrvInputs, 0xff, 2);

	for (INT32 i = 0; i < 8; i++) {
		DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
		DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
	}

	vblank = 0;
	ZetNewFrame();

	INT32 nInterleave = 2000; // needs high interleave
	INT32 nCyclesTotal[2] = { 3000000 / 60, 3000000 / 60 };
	INT32 nCyclesDone[2]  = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++) {
		ZetOpen(0);
		CPU_RUN(0, Zet);

		if (i == 1880) {
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
			vblank = 1;
		}
		ZetClose();

		ZetOpen(1);
		CPU_RUN(1, Zet);

		if (i%(nInterleave/4) == (nInterleave/4)-10) {
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
		ZetClose();
	}

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		Raiders5Draw();
	}

	return 0;
}

static INT32 NinjakunFrame()
{
	if (DrvReset) {
		NinjakunDoReset();
	}
	watchdog++;

	memset (DrvInputs, 0xff, 3);

	for (INT32 i = 0; i < 8; i++) {
		DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
		DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
	}

	vblank = 0;
	ZetNewFrame();

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 3000000 / 60, 3000000 / 60 };
	INT32 nCyclesDone[2]  = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++) {
		ZetOpen(0);
		CPU_RUN(0, Zet);

		if (i == 224) {
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
			vblank = 1;
		}
		ZetClose();

		ZetOpen(1);
		CPU_RUN(1, Zet);

		if (i == 32 || i == 96 || i == 160 || i == 224) {
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
		ZetClose();

		if (i == 224 && pBurnDraw) {
			NinjakunDraw();
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

	if (pnMin) {
		*pnMin = 0x029521;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);

		AY8910Scan(nAction, pnMin);

		hold_coin.scan();

		SCAN_VAR(flipscreen);
		SCAN_VAR(yscroll);
		SCAN_VAR(xscroll);
		SCAN_VAR(watchdog);
		SCAN_VAR(ninjakun_ioctrl);
	}

	return 0;
}


// Penguin-Kun Wars (US)

static struct BurnRomInfo pkunwarRomDesc[] = {
	{ "pkwar.01r",		0x4000, 0xce2d2c7b, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "pkwar.02r",		0x4000, 0xabc1f661, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "pkwar.03r",		0x2000, 0x56faebea, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "pkwar.01y",		0x4000, 0x428d3b92, 2 | BRF_GRA },	     //  3 Graphics
	{ "pkwar.02y",		0x4000, 0xce1da7bc, 2 | BRF_GRA },	     //  4
	{ "pkwar.03y",		0x4000, 0x63204400, 2 | BRF_GRA },	     //  5
	{ "pkwar.04y",		0x4000, 0x061dfca8, 2 | BRF_GRA },	     //  6

	{ "pkwar.col",		0x0020, 0xaf0fc5e2, 3 | BRF_GRA },	     //  7 Color Prom
};

STD_ROM_PICK(pkunwar)
STD_ROM_FN(pkunwar)

struct BurnDriver BurnDrvpkunwar = {
	"pkunwar", NULL, NULL, NULL, "1985",
	"Penguin-Kun Wars (US)\0", NULL, "UPL", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, pkunwarRomInfo, pkunwarRomName, NULL, NULL, NULL, NULL, PkunwarInputInfo, PkunwarDIPInfo,
	PkunwarInit, DrvExit, PkunwarFrame, PkunwarDraw, DrvScan, &DrvRecalc, 0x200,
	256, 192, 4, 3
};


// Penguin-Kun Wars (Japan, set 1)

static struct BurnRomInfo pkunwarjRomDesc[] = {
	{ "pgunwar.6",		0x4000, 0x357f3ef3, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "pgunwar.5",		0x4000, 0x0092e49e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "pkwar.03r",		0x2000, 0x56faebea, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "pkwar.01y",		0x4000, 0x428d3b92, 2 | BRF_GRA },	     //  3 Graphics
	{ "pkwar.02y",		0x4000, 0xce1da7bc, 2 | BRF_GRA },	     //  4
	{ "pgunwar.2",		0x4000, 0xa2a43443, 2 | BRF_GRA },	     //  5
	{ "pkwar.04y",		0x4000, 0x061dfca8, 2 | BRF_GRA },	     //  6

	{ "pkwar.col",		0x0020, 0xaf0fc5e2, 3 | BRF_GRA },	     //  7 Color Prom
};

STD_ROM_PICK(pkunwarj)
STD_ROM_FN(pkunwarj)

struct BurnDriver BurnDrvpkunwarj = {
	"pkunwarj", "pkunwar", NULL, NULL, "1985",
	"Penguin-Kun Wars (Japan, set 1)\0", NULL, "UPL", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, pkunwarjRomInfo, pkunwarjRomName, NULL, NULL, NULL, NULL, PkunwarInputInfo, PkunwarDIPInfo,
	PkunwarInit, DrvExit, PkunwarFrame, PkunwarDraw, DrvScan, &DrvRecalc, 0x200,
	256, 192, 4, 3
};


// Penguin-Kun Wars (Japan, set 2)

static struct BurnRomInfo pkunwarjaRomDesc[] = {
	{ "peng_wars_1_red.7a",	0x4000, 0x9dfdf1b2, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "peng_wars_2_red.7b",	0x4000, 0xbc286b8c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3_red.5b",			0x2000, 0x56faebea, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "1_yellow.7h",		0x4000, 0x428d3b92, 2 | BRF_GRA },           //  3 Graphics
	{ "2_yellow.7k",		0x4000, 0xce1da7bc, 2 | BRF_GRA },           //  4
	{ "3_yellow.7l",		0x4000, 0xa2a43443, 2 | BRF_GRA },           //  5
	{ "4_yellow.7m",		0x4000, 0x061dfca8, 2 | BRF_GRA },           //  6

	{ "tbp18s030n.1f",		0x0020, 0xaf0fc5e2, 3 | BRF_GRA },           //  7 Color Prom
};

STD_ROM_PICK(pkunwarja)
STD_ROM_FN(pkunwarja)

struct BurnDriver BurnDrvpkunwarja = {
	"pkunwarja", "pkunwar", NULL, NULL, "1985",
	"Penguin-Kun Wars (Japan, set 2)\0", NULL, "UPL", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, pkunwarjaRomInfo, pkunwarjaRomName, NULL, NULL, NULL, NULL, PkunwarInputInfo, PkunwarDIPInfo,
	PkunwarInit, DrvExit, PkunwarFrame, PkunwarDraw, DrvScan, &DrvRecalc, 0x200,
	256, 192, 4, 3
};


// Nova 2001 (Japan)

static struct BurnRomInfo nova2001RomDesc[] = {
	{ "1.6c",			0x2000, 0x368cffc0, 1 | BRF_PRG | BRF_ESS}, //  0 Z80 Code
	{ "2.6d",	        0x2000, 0xbc4e442b, 1 | BRF_PRG | BRF_ESS}, //  1
	{ "3.6f",			0x2000, 0xb2849038, 1 | BRF_PRG | BRF_ESS}, //  2
	{ "4.6g",	        0x1000, 0x6b5bb12d, 1 | BRF_PRG | BRF_ESS}, //  3

	{ "5.12s",			0x2000, 0x54198941, 2 | BRF_GRA },          //  4 Graphics
	{ "6.12p",			0x2000, 0xcbd90dca, 2 | BRF_GRA },          //  5
	{ "7.12n",			0x2000, 0x9ebd8806, 2 | BRF_GRA },          //  6
	{ "8.12l",			0x2000, 0xd1b18389, 2 | BRF_GRA },          //  7

	{ "nova2001.clr",	0x0020, 0xa2fac5cd, 3 | BRF_GRA },          //  8 Color Prom
};

STD_ROM_PICK(nova2001)
STD_ROM_FN(nova2001)

struct BurnDriver BurnDrvNova2001 = {
	"nova2001", NULL, NULL, NULL, "1983",
	"Nova 2001 (Japan)\0", NULL, "UPL", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, nova2001RomInfo, nova2001RomName, NULL, NULL, NULL, NULL, Nova2001InputInfo, Nova2001DIPInfo,
	NovaInit, DrvExit, NovaFrame, NovaDraw, DrvScan, &DrvRecalc, 0x200,
	256, 192, 4, 3
};


// Nova 2001 (Japan, hack?)

static struct BurnRomInfo nova2001hRomDesc[] = {
	// roms 1 and 2 had green stickers, but looks like an unofficial mod, bytes have been added in empty space to fix game checksum after mods were made to code.
	// one of the mods fixes the game resetting if the coin input is held down for too short / long of a period, the purpose of the other is unknown.
	{ "1,green.6c",		0x2000, 0x1a8731b3, 1 | BRF_PRG | BRF_ESS}, //  0 Z80 Code
	{ "2,green.6d",		0x2000, 0xbc4e442b, 1 | BRF_PRG | BRF_ESS}, //  1
	{ "3.6f",			0x2000, 0xb2849038, 1 | BRF_PRG | BRF_ESS}, //  2
	{ "4.6g",	        0x1000, 0x6b5bb12d, 1 | BRF_PRG | BRF_ESS}, //  3

	{ "5.12s",			0x2000, 0x54198941, 2 | BRF_GRA },          //  4 Graphics
	{ "6.12p",			0x2000, 0xcbd90dca, 2 | BRF_GRA },          //  5
	{ "7.12n",			0x2000, 0x9ebd8806, 2 | BRF_GRA },          //  6
	{ "8.12l",			0x2000, 0xd1b18389, 2 | BRF_GRA },          //  7

	{ "nova2001.clr",	0x0020, 0xa2fac5cd, 3 | BRF_GRA },          //  8 Color Prom
};

STD_ROM_PICK(nova2001h)
STD_ROM_FN(nova2001h)

struct BurnDriver BurnDrvNova2001h = {
	"nova2001h", "nova2001", NULL, NULL, "1983",
	"Nova 2001 (Japan, hack?)\0", NULL, "UPL", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, nova2001hRomInfo, nova2001hRomName, NULL, NULL, NULL, NULL, Nova2001InputInfo, Nova2001DIPInfo,
	NovaInit, DrvExit, NovaFrame, NovaDraw, DrvScan, &DrvRecalc, 0x200,
	256, 192, 4, 3
};


// Nova 2001 (US)

static struct BurnRomInfo nova2001uRomDesc[] = {
	{ "nova2001.1",		0x2000, 0xb79461bd, 1 | BRF_PRG | BRF_ESS}, //  0 Z80 Code
	{ "nova2001.2",	    0x2000, 0xfab87144, 1 | BRF_PRG | BRF_ESS}, //  1
	{ "3.6f",			0x2000, 0xb2849038, 1 | BRF_PRG | BRF_ESS}, //  2
	{ "4.6g",	        0x1000, 0x6b5bb12d, 1 | BRF_PRG | BRF_ESS}, //  3

	{ "nova2001.5",		0x2000, 0x8ea576e8, 2 | BRF_GRA },          //  4 Graphics
	{ "nova2001.6",		0x2000, 0x0c61656c, 2 | BRF_GRA },          //  5
	{ "7.12n",			0x2000, 0x9ebd8806, 2 | BRF_GRA },          //  6
	{ "8.12l",			0x2000, 0xd1b18389, 2 | BRF_GRA },          //  7

	{ "nova2001.clr",	0x0020, 0xa2fac5cd, 3 | BRF_GRA },          //  8 Color Prom
};

STD_ROM_PICK(nova2001u)
STD_ROM_FN(nova2001u)

struct BurnDriver BurnDrvNova2001u = {
	"nova2001u", "nova2001", NULL, NULL, "1983",
	"Nova 2001 (US)\0", NULL, "UPL (Universal license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, nova2001uRomInfo, nova2001uRomName, NULL, NULL, NULL, NULL, Nova2001InputInfo, Nova2001DIPInfo,
	NovaInit, DrvExit, NovaFrame, NovaDraw, DrvScan, &DrvRecalc, 0x200,
	256, 192, 4, 3
};


// Ninjakun Majou no Bouken

static struct BurnRomInfo ninjakunRomDesc[] = {
	{ "ninja-1.7a",		0x2000, 0x1c1dc141, 1 | BRF_PRG | BRF_ESS}, //  0 Z80 #0 Code
	{ "ninja-2.7b",		0x2000, 0x39cc7d37, 1 | BRF_PRG | BRF_ESS}, //  1
	{ "ninja-3.7d",		0x2000, 0xd542bfe3, 1 | BRF_PRG | BRF_ESS}, //  2
	{ "ninja-4.7e",		0x2000, 0xa57385c6, 1 | BRF_PRG | BRF_ESS}, //  3

	{ "ninja-5.7h",		0x2000, 0x164a42c4, 2 | BRF_PRG | BRF_ESS}, //  4 Z80 #1 Code

	{ "ninja-6.7n",		0x2000, 0xa74c4297, 3 | BRF_GRA },          //  5 Foreground & Sprites
	{ "ninja-7.7p",		0x2000, 0x53a72039, 3 | BRF_GRA },          //  6
	{ "ninja-8.7s",		0x2000, 0x4a99d857, 3 | BRF_GRA },          //  7
	{ "ninja-9.7t",		0x2000, 0xdede49e4, 3 | BRF_GRA },          //  8

	{ "ninja-10.2c",	0x2000, 0x0d55664a, 4 | BRF_GRA },          //  9 Backgrounds
	{ "ninja-11.2d",	0x2000, 0x12ff9597, 4 | BRF_GRA },          // 10
	{ "ninja-12.4c",	0x2000, 0xe9b75807, 4 | BRF_GRA },          // 11
	{ "ninja-13.4d",	0x2000, 0x1760ed2c, 4 | BRF_GRA },          // 12
};

STD_ROM_PICK(ninjakun)
STD_ROM_FN(ninjakun)

struct BurnDriver BurnDrvNinjakun = {
	"ninjakun", NULL, NULL, NULL, "1984",
	"Ninjakun Majou no Bouken\0", NULL, "UPL (Taito license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, ninjakunRomInfo, ninjakunRomName, NULL, NULL, NULL, NULL, NinjakunInputInfo, NinjakunDIPInfo,
	NinjakunInit, DrvExit, NinjakunFrame, NinjakunDraw, DrvScan, &DrvRecalc, 0x300,
	256, 192, 4, 3
};


// Raiders5

static struct BurnRomInfo raiders5RomDesc[] = {
	{ "raiders5.1",		0x4000, 0x47cea11f, 1 | BRF_PRG | BRF_ESS}, //  0 Z80 #0 Code
	{ "raiders5.2",		0x4000, 0xeb2ff410, 1 | BRF_PRG | BRF_ESS}, //  1

	{ "raiders5.2",		0x4000, 0xeb2ff410, 2 | BRF_PRG | BRF_ESS}, //  2 Z80 #1 Code

	{ "raiders3.11f",	0x4000, 0x30041d58, 3 | BRF_GRA },          //  3 Foreground & Sprites
	{ "raiders4.11g",	0x4000, 0xe441931c, 3 | BRF_GRA },          //  4

	{ "raiders5.11n",	0x4000, 0xc0895090, 4 | BRF_GRA },          //  5 Backgrounds
};

STD_ROM_PICK(raiders5)
STD_ROM_FN(raiders5)

struct BurnDriver BurnDrvRaiders5 = {
	"raiders5", NULL, NULL, NULL, "1985",
	"Raiders5\0", NULL, "UPL", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, raiders5RomInfo, raiders5RomName, NULL, NULL, NULL, NULL, Raiders5InputInfo, Raiders5DIPInfo,
	Raiders5Init, DrvExit, Raiders5Frame, Raiders5Draw, NULL, &DrvRecalc, 0x300,
	256, 192, 4, 3
};


// Raiders5 (Japan, set 1)

static struct BurnRomInfo raiders5tRomDesc[] = {
	{ "raiders1.4c",	0x4000, 0x4e2d5679, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "raiders2.4d",	0x4000, 0xc8604be1, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "raiders2.4d",	0x4000, 0xc8604be1, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "raiders3.11f",	0x4000, 0x30041d58, 3 | BRF_GRA },           //  3 Foreground & Sprites
	{ "raiders4.11g",	0x4000, 0xe441931c, 3 | BRF_GRA },           //  4

	{ "raiders5.11n",	0x4000, 0xc0895090, 4 | BRF_GRA },           //  5 Backgrounds
};

STD_ROM_PICK(raiders5t)
STD_ROM_FN(raiders5t)

struct BurnDriver BurnDrvRaidrs5t = {
	"raiders5t", "raiders5", NULL, NULL, "1985",
	"Raiders5 (Japan, set 1)\0", NULL, "UPL (Taito license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_FLIPPED | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, raiders5tRomInfo, raiders5tRomName, NULL, NULL, NULL, NULL, Raiders5InputInfo, Raiders5DIPInfo,
	Raiders5Init, DrvExit, Raiders5Frame, Raiders5Draw, NULL, &DrvRecalc, 0x300,
	256, 192, 4, 3
};


// Raiders5 (Japan, set 2, bootleg?)

static struct BurnRomInfo raiders5taRomDesc[] = {
	{ "1.4c",	0x4000, 0xe6264952, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "2.4d",	0x4000, 0x06f7c5b0, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "2.4d",	0x4000, 0x06f7c5b0, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "3.11f",	0x4000, 0x30041d58, 3 | BRF_GRA },           //  3 Foreground & Sprites
	{ "4.11g",	0x4000, 0xe441931c, 3 | BRF_GRA },           //  4

	// single byte different in unused area at 2fff ee -> 2e, possibly bitrot although more than a single bit changed
	{ "5.11n",	0x4000, 0xfb532e4d, 4 | BRF_GRA },           //  5 Backgrounds
};

STD_ROM_PICK(raiders5ta)
STD_ROM_FN(raiders5ta)

struct BurnDriver BurnDrvRaidrs5ta = {
	"raiders5ta", "raiders5", NULL, NULL, "1985",
	"Raiders5 (Japan, set 2, bootleg?)\0", NULL, "UPL (Taito license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_FLIPPED | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, raiders5taRomInfo, raiders5taRomName, NULL, NULL, NULL, NULL, Raiders5InputInfo, Raiders5taDIPInfo,
	Raiders5Init, DrvExit, Raiders5Frame, Raiders5Draw, NULL, &DrvRecalc, 0x300,
	256, 192, 4, 3
};
