// Based on MAME driver by Chris Moore, Nicola Salmoria

// .. FBH!

#include "tiles_generic.h"
#include "z80_intf.h"
#include "m6800_intf.h"
#include "taito_m68705.h"
#include "burn_ym2203.h"
#include "burn_ym3526.h"
#include "bitswap.h"

static UINT8 DrvInputPort0[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvInputPort1[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvInputPort2[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvDip[2]        = {0, 0};
static UINT8 DrvInput[3]      = {0x00, 0x00, 0x00};
static UINT8 DrvReset         = 0;

static UINT8 *AllMem              = NULL;
static UINT8 *MemEnd              = NULL;
static UINT8 *RamStart            = NULL;
static UINT8 *RamEnd              = NULL;
static UINT8 *DrvZ80Rom1          = NULL;
static UINT8 *DrvZ80Rom2          = NULL;
static UINT8 *DrvZ80Rom3          = NULL;
static UINT8 *DrvMcuRom           = NULL;
static UINT8 *DrvProm             = NULL;
static UINT8 *DrvZ80Ram1          = NULL;
static UINT8 *DrvZ80Ram3          = NULL;
static UINT8 *DrvSharedRam        = NULL;
static UINT8 *DrvMcuRam           = NULL;
static UINT8 *DrvVideoRam         = NULL;
static UINT8 *DrvSpriteRam        = NULL;
static UINT8 *DrvPaletteRam       = NULL;
static UINT8 *DrvTiles            = NULL;
static UINT8 *DrvTempRom          = NULL;
static UINT32 *DrvPalette         = NULL;

static UINT8 DrvRomBank;
static UINT8 DrvMCUActive;
static UINT8 DrvVideoEnable;
static UINT8 DrvFlipScreen;
static INT32 IC43A;
static INT32 IC43B;
static INT32 DrvSoundNmiEnable;
static INT32 DrvSoundNmiPending;

static UINT8 DrvSoundStatus;
static UINT8 DrvSoundStatusPending;
static UINT8 DrvSoundLatch;
static UINT8 DrvSoundLatchPending;

typedef INT32 (*BublboblCallbackFunc)();
static BublboblCallbackFunc BublboblCallbackFunction;

static UINT8 DrvMCUInUse;

static INT32 bublbobl2 = 0;
static INT32 tokiob = 0;
static INT32 tokiosnd = 0;

static INT32 mcu_address, mcu_latch;
static UINT8 ddr1, ddr2, ddr3, ddr4;
static UINT8 port1_in, port2_in, port3_in, port4_in;
static UINT8 port1_out, port2_out, port3_out, port4_out;

static struct BurnInputInfo BublboblInputList[] =
{
	{"P1 Coin"           , BIT_DIGITAL  , DrvInputPort0 + 2, "p1 coin"   },
	{"P1 Start"          , BIT_DIGITAL  , DrvInputPort1 + 6, "p1 start"  },
	{"P1 Left"           , BIT_DIGITAL  , DrvInputPort1 + 0, "p1 left"   },
	{"P1 Right"          , BIT_DIGITAL  , DrvInputPort1 + 1, "p1 right"  },
	{"P1 Fire 1"         , BIT_DIGITAL  , DrvInputPort1 + 5, "p1 fire 1" },
	{"P1 Fire 2"         , BIT_DIGITAL  , DrvInputPort1 + 4, "p1 fire 2" },

	{"P2 Coin"           , BIT_DIGITAL  , DrvInputPort0 + 3, "p2 coin"   },
	{"P2 Start"          , BIT_DIGITAL  , DrvInputPort2 + 6, "p2 start"  },
	{"P2 Left"           , BIT_DIGITAL  , DrvInputPort2 + 0, "p2 left"   },
	{"P2 Right"          , BIT_DIGITAL  , DrvInputPort2 + 1, "p2 right"  },
	{"P2 Fire 1"         , BIT_DIGITAL  , DrvInputPort2 + 5, "p2 fire 1" },
	{"P2 Fire 2"         , BIT_DIGITAL  , DrvInputPort2 + 4, "p2 fire 2" },

	{"Reset"             , BIT_DIGITAL  , &DrvReset        , "reset"     },
	{"Service"           , BIT_DIGITAL  , DrvInputPort0 + 1, "service"   },
	{"Tilt"              , BIT_DIGITAL  , DrvInputPort0 + 0, "tilt"      },
	{"Dip 1"             , BIT_DIPSWITCH, DrvDip + 0       , "dip"       },
	{"Dip 2"             , BIT_DIPSWITCH, DrvDip + 1       , "dip"       },
};

STDINPUTINFO(Bublbobl)

static struct BurnInputInfo BoblboblInputList[] =
{
	{"P1 Coin"           , BIT_DIGITAL  , DrvInputPort0 + 3, "p1 coin"   },
	{"P1 Start"          , BIT_DIGITAL  , DrvInputPort0 + 6, "p1 start"  },
	{"P1 Left"           , BIT_DIGITAL  , DrvInputPort0 + 0, "p1 left"   },
	{"P1 Right"          , BIT_DIGITAL  , DrvInputPort0 + 1, "p1 right"  },
	{"P1 Fire 1"         , BIT_DIGITAL  , DrvInputPort0 + 5, "p1 fire 1" },
	{"P1 Fire 2"         , BIT_DIGITAL  , DrvInputPort0 + 4, "p1 fire 2" },

	{"P2 Coin"           , BIT_DIGITAL  , DrvInputPort0 + 2, "p2 coin"   },
	{"P2 Start"          , BIT_DIGITAL  , DrvInputPort1 + 6, "p2 start"  },
	{"P2 Left"           , BIT_DIGITAL  , DrvInputPort1 + 0, "p2 left"   },
	{"P2 Right"          , BIT_DIGITAL  , DrvInputPort1 + 1, "p2 right"  },
	{"P2 Fire 1"         , BIT_DIGITAL  , DrvInputPort1 + 5, "p2 fire 1" },
	{"P2 Fire 2"         , BIT_DIGITAL  , DrvInputPort1 + 4, "p2 fire 2" },

	{"Reset"             , BIT_DIGITAL  , &DrvReset        , "reset"     },
	{"Service"           , BIT_DIGITAL  , DrvInputPort1 + 3, "service"   },
	{"Tilt"              , BIT_DIGITAL  , DrvInputPort1 + 2, "tilt"      },
	{"Dip 1"             , BIT_DIPSWITCH, DrvDip + 0       , "dip"       },
	{"Dip 2"             , BIT_DIPSWITCH, DrvDip + 1       , "dip"       },
};

STDINPUTINFO(Boblbobl)

static inline void BublboblMakeInputs()
{
	DrvInput[0] = 0xf3;
	DrvInput[1] = 0xff;
	DrvInput[2] = 0xff;
	for (INT32 i = 0; i < 8; i++) {
		DrvInput[0] ^= (DrvInputPort0[i] & 1) << i;
		DrvInput[1] ^= (DrvInputPort1[i] & 1) << i;
		DrvInput[2] ^= (DrvInputPort2[i] & 1) << i;
	}

	if (bublbobl2) {
		DrvInput[0] ^= 0x8c;
		// Swap coins
		DrvInput[0] = (DrvInput[0] & 0xf3) | ((DrvInput[0] & 0x04) << 1) | ((DrvInput[0] & 0x08) >> 1);
	}
}

static struct BurnDIPInfo BublboblDIPList[]=
{
	// Default Values
	{0x0f, 0xff, 0xff, 0xfe, NULL                     },
	{0x10, 0xff, 0xff, 0xff, NULL                     },
	
	// Dip 1
	{0   , 0xfe, 0   , 4   , "Mode"                   },
	{0x0f, 0x01, 0x05, 0x04, "Game, English"          },
	{0x0f, 0x01, 0x05, 0x05, "Game, Japanese"         },
	{0x0f, 0x01, 0x05, 0x01, "Test (Grid and Inputs)" },
	{0x0f, 0x01, 0x05, 0x00, "Test (RAM and Sound)/Pause"},
	
	{0   , 0xfe, 0   , 2   , "Flip Screen"            },
	{0x0f, 0x01, 0x02, 0x02, "Off"                    },
	{0x0f, 0x01, 0x02, 0x00, "On"                     },
	
	{0   , 0xfe, 0   , 2   , "Demo Sounds"            },
	{0x0f, 0x01, 0x08, 0x00, "Off"                    },
	{0x0f, 0x01, 0x08, 0x08, "On"                     },
	
	{0   , 0xfe, 0   , 4   , "Coin A"                 },
	{0x0f, 0x01, 0x30, 0x10, "2 Coins 1 Play"         },
	{0x0f, 0x01, 0x30, 0x30, "1 Coin  1 Play"         },
	{0x0f, 0x01, 0x30, 0x00, "2 Coins 3 Plays"        },
	{0x0f, 0x01, 0x30, 0x20, "1 Coin  2 Plays"        },
	
	{0   , 0xfe, 0   , 4   , "Coin B"                 },
	{0x0f, 0x01, 0xc0, 0x40, "2 Coins 1 Play"         },
	{0x0f, 0x01, 0xc0, 0xc0, "1 Coin  1 Play"         },
	{0x0f, 0x01, 0xc0, 0x00, "2 Coins 3 Plays"        },
	{0x0f, 0x01, 0xc0, 0x80, "1 Coin  2 Plays"        },
	
	// Dip 2
	{0   , 0xfe, 0   , 4   , "Difficulty"             },
	{0x10, 0x01, 0x03, 0x02, "Easy"                   },
	{0x10, 0x01, 0x03, 0x03, "Normal"                 },
	{0x10, 0x01, 0x03, 0x01, "Hard"                   },
	{0x10, 0x01, 0x03, 0x00, "Very Hard"              },
	
	{0   , 0xfe, 0   , 4   , "Bonus Life"             },
	{0x10, 0x01, 0x0c, 0x08, "20k  80k 300k"          },
	{0x10, 0x01, 0x0c, 0x0c, "30k 100k 400k"          },
	{0x10, 0x01, 0x0c, 0x04, "40k 200k 500k"          },
	{0x10, 0x01, 0x0c, 0x00, "50k 250k 500k"          },
	
	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x10, 0x01, 0x30, 0x10, "1"                      },
	{0x10, 0x01, 0x30, 0x00, "2"                      },
	{0x10, 0x01, 0x30, 0x30, "3"                      },
	{0x10, 0x01, 0x30, 0x20, "5"                      },
	
	{0   , 0xfe, 0   , 2   , "ROM Type"               },
	{0x10, 0x01, 0x80, 0x80, "IC52=512kb, IC53=none"  },
	{0x10, 0x01, 0x80, 0x00, "IC52=256kb, IC53=256kb" },
};

STDDIPINFO(Bublbobl)

static struct BurnDIPInfo BoblboblDIPList[]=
{
	// Default Values
	{0x0f, 0xff, 0xff, 0xfe, NULL                     },
	{0x10, 0xff, 0xff, 0x3f, NULL                     },
	
	// Dip 1
	{0   , 0xfe, 0   , 4   , "Mode"                   },
	{0x0f, 0x01, 0x05, 0x04, "Game, English"          },
	{0x0f, 0x01, 0x05, 0x05, "Game, Japanese"         },
	{0x0f, 0x01, 0x05, 0x01, "Test (Grid and Inputs)" },
	{0x0f, 0x01, 0x05, 0x00, "Test (RAM and Sound)"   },
	
	{0   , 0xfe, 0   , 2   , "Flip Screen"            },
	{0x0f, 0x01, 0x02, 0x02, "Off"                    },
	{0x0f, 0x01, 0x02, 0x00, "On"                     },
	
	{0   , 0xfe, 0   , 2   , "Demo Sounds"            },
	{0x0f, 0x01, 0x08, 0x00, "Off"                    },
	{0x0f, 0x01, 0x08, 0x08, "On"                     },
	
	{0   , 0xfe, 0   , 4   , "Coin A"                 },
	{0x0f, 0x01, 0x30, 0x10, "2 Coins 1 Play"         },
	{0x0f, 0x01, 0x30, 0x30, "1 Coin  1 Play"         },
	{0x0f, 0x01, 0x30, 0x00, "2 Coins 3 Plays"        },
	{0x0f, 0x01, 0x30, 0x20, "1 Coin  2 Plays"        },
	
	{0   , 0xfe, 0   , 4   , "Coin B"                 },
	{0x0f, 0x01, 0xc0, 0x40, "2 Coins 1 Play"         },
	{0x0f, 0x01, 0xc0, 0xc0, "1 Coin  1 Play"         },
	{0x0f, 0x01, 0xc0, 0x00, "2 Coins 3 Plays"        },
	{0x0f, 0x01, 0xc0, 0x80, "1 Coin  2 Plays"        },
	
	// Dip 2
	{0   , 0xfe, 0   , 4   , "Difficulty"             },
	{0x10, 0x01, 0x03, 0x02, "Easy"                   },
	{0x10, 0x01, 0x03, 0x03, "Normal"                 },
	{0x10, 0x01, 0x03, 0x01, "Hard"                   },
	{0x10, 0x01, 0x03, 0x00, "Very Hard"              },
	
	{0   , 0xfe, 0   , 4   , "Bonus Life"             },
	{0x10, 0x01, 0x0c, 0x08, "20k  80k 300k"          },
	{0x10, 0x01, 0x0c, 0x0c, "30k 100k 400k"          },
	{0x10, 0x01, 0x0c, 0x04, "40k 200k 500k"          },
	{0x10, 0x01, 0x0c, 0x00, "50k 250k 500k"          },
	
	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x10, 0x01, 0x30, 0x10, "1"                      },
	{0x10, 0x01, 0x30, 0x00, "2"                      },
	{0x10, 0x01, 0x30, 0x30, "3"                      },
	{0x10, 0x01, 0x30, 0x20, "5"                      },
	
	{0   , 0xfe, 0   , 4   , "Monster Speed"          },
	{0x10, 0x01, 0xc0, 0x00, "Normal"                 },
	{0x10, 0x01, 0xc0, 0x40, "Medium"                 },
	{0x10, 0x01, 0xc0, 0x80, "High"                   },
	{0x10, 0x01, 0xc0, 0xc0, "Very High"              },
};

STDDIPINFO(Boblbobl)

static struct BurnDIPInfo BoblcaveDIPList[]=
{
	// Default Values
	{0x0f, 0xff, 0xff, 0xfe, NULL                     },
	{0x10, 0xff, 0xff, 0xff, NULL                     },
	
	// Dip 1
	{0   , 0xfe, 0   , 4   , "Mode"                   },
	{0x0f, 0x01, 0x05, 0x04, "Game, English"          },
	{0x0f, 0x01, 0x05, 0x05, "Game, Japanese"         },
	{0x0f, 0x01, 0x05, 0x01, "Test (Grid and Inputs)" },
	{0x0f, 0x01, 0x05, 0x00, "Test (RAM and Sound)"   },
	
	{0   , 0xfe, 0   , 2   , "Flip Screen"            },
	{0x0f, 0x01, 0x02, 0x02, "Off"                    },
	{0x0f, 0x01, 0x02, 0x00, "On"                     },
	
	{0   , 0xfe, 0   , 2   , "Demo Sounds"            },
	{0x0f, 0x01, 0x08, 0x00, "Off"                    },
	{0x0f, 0x01, 0x08, 0x08, "On"                     },
	
	{0   , 0xfe, 0   , 4   , "Coin A"                 },
	{0x0f, 0x01, 0x30, 0x10, "2 Coins 1 Play"         },
	{0x0f, 0x01, 0x30, 0x30, "1 Coin  1 Play"         },
	{0x0f, 0x01, 0x30, 0x00, "2 Coins 3 Plays"        },
	{0x0f, 0x01, 0x30, 0x20, "1 Coin  2 Plays"        },
	
	{0   , 0xfe, 0   , 4   , "Coin B"                 },
	{0x0f, 0x01, 0xc0, 0x40, "2 Coins 1 Play"         },
	{0x0f, 0x01, 0xc0, 0xc0, "1 Coin  1 Play"         },
	{0x0f, 0x01, 0xc0, 0x00, "2 Coins 3 Plays"        },
	{0x0f, 0x01, 0xc0, 0x80, "1 Coin  2 Plays"        },
	
	// Dip 2
	{0   , 0xfe, 0   , 4   , "Difficulty"             },
	{0x10, 0x01, 0x03, 0x02, "Easy"                   },
	{0x10, 0x01, 0x03, 0x03, "Normal"                 },
	{0x10, 0x01, 0x03, 0x01, "Hard"                   },
	{0x10, 0x01, 0x03, 0x00, "Very Hard"              },
	
	{0   , 0xfe, 0   , 4   , "Bonus Life"             },
	{0x10, 0x01, 0x0c, 0x08, "20k  80k 300k"          },
	{0x10, 0x01, 0x0c, 0x0c, "30k 100k 400k"          },
	{0x10, 0x01, 0x0c, 0x04, "40k 200k 500k"          },
	{0x10, 0x01, 0x0c, 0x00, "50k 250k 500k"          },
	
	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x10, 0x01, 0x30, 0x10, "1"                      },
	{0x10, 0x01, 0x30, 0x00, "2"                      },
	{0x10, 0x01, 0x30, 0x30, "3"                      },
	{0x10, 0x01, 0x30, 0x20, "5"                      },
	
	{0   , 0xfe, 0   , 4   , "Unknown"                },
	{0x10, 0x01, 0x40, 0x40, "Off"                    },
	{0x10, 0x01, 0x40, 0x00, "On"                     },

	{0   , 0xfe, 0   , 4   , "ROM Type"               },
	{0x10, 0x01, 0x80, 0x80, "IC52=512kb, IC53=none"  },
	{0x10, 0x01, 0x80, 0x00, "IC52=256kb, IC53=256kb" },
};

STDDIPINFO(Boblcave)

static struct BurnDIPInfo SboblbobDIPList[]=
{
	// Default Values
	{0x0f, 0xff, 0xff, 0xfe, NULL                     },
	{0x10, 0xff, 0xff, 0x3f, NULL                     },
	
	// Dip 1
	{0   , 0xfe, 0   , 2   , "Game"                   },
	{0x0f, 0x01, 0x01, 0x01, "Bobble Bobble"          },
	{0x0f, 0x01, 0x01, 0x00, "Super Bobble Bobble"    },
	
	{0   , 0xfe, 0   , 2   , "Flip Screen"            },
	{0x0f, 0x01, 0x02, 0x02, "Off"                    },
	{0x0f, 0x01, 0x02, 0x00, "On"                     },
	
	{0   , 0xfe, 0   , 2   , "Service Mode"           },
	{0x0f, 0x01, 0x04, 0x04, "Off"                    },
	{0x0f, 0x01, 0x04, 0x00, "On"                     },
	
	{0   , 0xfe, 0   , 2   , "Demo Sounds"            },
	{0x0f, 0x01, 0x08, 0x00, "Off"                    },
	{0x0f, 0x01, 0x08, 0x08, "On"                     },
	
	{0   , 0xfe, 0   , 4   , "Coin A"                 },
	{0x0f, 0x01, 0x30, 0x10, "2 Coins 1 Play"         },
	{0x0f, 0x01, 0x30, 0x30, "1 Coin  1 Play"         },
	{0x0f, 0x01, 0x30, 0x00, "2 Coins 3 Plays"        },
	{0x0f, 0x01, 0x30, 0x20, "1 Coin  2 Plays"        },
	
	{0   , 0xfe, 0   , 4   , "Coin B"                 },
	{0x0f, 0x01, 0xc0, 0x40, "2 Coins 1 Play"         },
	{0x0f, 0x01, 0xc0, 0xc0, "1 Coin  1 Play"         },
	{0x0f, 0x01, 0xc0, 0x00, "2 Coins 3 Plays"        },
	{0x0f, 0x01, 0xc0, 0x80, "1 Coin  2 Plays"        },
	
	// Dip 2
	{0   , 0xfe, 0   , 4   , "Difficulty"             },
	{0x10, 0x01, 0x03, 0x02, "Easy"                   },
	{0x10, 0x01, 0x03, 0x03, "Normal"                 },
	{0x10, 0x01, 0x03, 0x01, "Hard"                   },
	{0x10, 0x01, 0x03, 0x00, "Very Hard"              },
	
	{0   , 0xfe, 0   , 4   , "Bonus Life"             },
	{0x10, 0x01, 0x0c, 0x08, "20k  80k 300k"          },
	{0x10, 0x01, 0x0c, 0x0c, "30k 100k 400k"          },
	{0x10, 0x01, 0x0c, 0x04, "40k 200k 500k"          },
	{0x10, 0x01, 0x0c, 0x00, "50k 250k 500k"          },
	
	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x10, 0x01, 0x30, 0x10, "1"                      },
	{0x10, 0x01, 0x30, 0x00, "2"                      },
	{0x10, 0x01, 0x30, 0x30, "3"                      },
	{0x10, 0x01, 0x30, 0x20, "5"                      },
	
	{0   , 0xfe, 0   , 4   , "Monster Speed"          },
	{0x10, 0x01, 0xc0, 0x00, "Normal"                 },
	{0x10, 0x01, 0xc0, 0x40, "Medium"                 },
	{0x10, 0x01, 0xc0, 0x80, "High"                   },
	{0x10, 0x01, 0xc0, 0xc0, "Very High"              },
};

STDDIPINFO(Sboblbob)

static struct BurnDIPInfo DlandDIPList[]=
{
	// Default Values
	{0x0f, 0xff, 0xff, 0xff, NULL                     },
	{0x10, 0xff, 0xff, 0x3f, NULL                     },
	
	// Dip 1
	{0   , 0xfe, 0   , 2   , "Game"                   },
	{0x0f, 0x01, 0x01, 0x01, "Dream Land"             },
	{0x0f, 0x01, 0x01, 0x00, "Super Dream Land"       },

	{0   , 0xfe, 0   , 4   , "Mode"                   },
	{0x0f, 0x01, 0x05, 0x04, "Game, English"          },
	{0x0f, 0x01, 0x05, 0x05, "Game, Japanese"         },
	{0x0f, 0x01, 0x05, 0x01, "Test (Grid and Inputs)" },
	{0x0f, 0x01, 0x05, 0x00, "Test (RAM and Sound)"   },
	
	{0   , 0xfe, 0   , 2   , "Flip Screen"            },
	{0x0f, 0x01, 0x02, 0x02, "Off"                    },
	{0x0f, 0x01, 0x02, 0x00, "On"                     },

	{0   , 0xfe, 0   , 2   , "Service Mode"           },
	{0x0f, 0x01, 0x04, 0x00, "Off"                    },
	{0x0f, 0x01, 0x04, 0x04, "On"                     },

	{0   , 0xfe, 0   , 2   , "Demo Sounds"            },
	{0x0f, 0x01, 0x08, 0x00, "Off"                    },
	{0x0f, 0x01, 0x08, 0x08, "On"                     },
	
	{0   , 0xfe, 0   , 4   , "Coin A"                 },
	{0x0f, 0x01, 0x30, 0x10, "2 Coins 1 Play"         },
	{0x0f, 0x01, 0x30, 0x30, "1 Coin  1 Play"         },
	{0x0f, 0x01, 0x30, 0x00, "2 Coins 3 Plays"        },
	{0x0f, 0x01, 0x30, 0x20, "1 Coin  2 Plays"        },
	
	{0   , 0xfe, 0   , 4   , "Coin B"                 },
	{0x0f, 0x01, 0xc0, 0x40, "2 Coins 1 Play"         },
	{0x0f, 0x01, 0xc0, 0xc0, "1 Coin  1 Play"         },
	{0x0f, 0x01, 0xc0, 0x00, "2 Coins 3 Plays"        },
	{0x0f, 0x01, 0xc0, 0x80, "1 Coin  2 Plays"        },

	// Dip 2
	{0   , 0xfe, 0   , 4   , "Difficulty"             },
	{0x10, 0x01, 0x03, 0x02, "Easy"                   },
	{0x10, 0x01, 0x03, 0x03, "Normal"                 },
	{0x10, 0x01, 0x03, 0x01, "Hard"                   },
	{0x10, 0x01, 0x03, 0x00, "Very Hard"              },
	
	{0   , 0xfe, 0   , 4   , "Bonus Life"             },
	{0x10, 0x01, 0x0c, 0x08, "20k  80k 300k"          },
	{0x10, 0x01, 0x0c, 0x0c, "30k 100k 400k"          },
	{0x10, 0x01, 0x0c, 0x04, "40k 200k 500k"          },
	{0x10, 0x01, 0x0c, 0x00, "50k 250k 500k"          },
	
	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x10, 0x01, 0x30, 0x10, "1"                      },
	{0x10, 0x01, 0x30, 0x00, "2"                      },
	{0x10, 0x01, 0x30, 0x30, "3"                      },
	{0x10, 0x01, 0x30, 0x20, "100 (Cheat)"            },
	
	{0   , 0xfe, 0   , 4   , "Monster Speed"          },
	{0x10, 0x01, 0xc0, 0x00, "Normal"                 },
	{0x10, 0x01, 0xc0, 0x40, "Medium"                 },
	{0x10, 0x01, 0xc0, 0x80, "High"                   },
	{0x10, 0x01, 0xc0, 0xc0, "Very High"              },
};

STDDIPINFO(Dland)

static struct BurnInputInfo TokioInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvInputPort0 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvInputPort1 + 6,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvInputPort1 + 3,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvInputPort1 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvInputPort1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvInputPort1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvInputPort1 + 5,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvInputPort1 + 4,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvInputPort0 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvInputPort2 + 6,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvInputPort2 + 3,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvInputPort2 + 2,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvInputPort2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvInputPort2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvInputPort2 + 5,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvInputPort2 + 4,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,			"reset"		},
	{"Service",			BIT_DIGITAL,	DrvInputPort0 + 1,	"service"	},
	{"Tilt",			BIT_DIGITAL,	DrvInputPort0 + 0,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDip + 0,			"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDip + 1,			"dip"		},
};

STDINPUTINFO(Tokio)

static struct BurnDIPInfo TokioDIPList[]=
{
	{0x13, 0xff, 0xff, 0xfe, NULL					},
	{0x14, 0xff, 0xff, 0x7e, NULL					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x13, 0x01, 0x01, 0x00, "Upright"				},
	{0x13, 0x01, 0x01, 0x01, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x13, 0x01, 0x02, 0x02, "Off"					},
	{0x13, 0x01, 0x02, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x13, 0x01, 0x04, 0x04, "Off"					},
	{0x13, 0x01, 0x04, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x13, 0x01, 0x08, 0x00, "Off"					},
	{0x13, 0x01, 0x08, 0x08, "On"					},

	{0   , 0xfe, 0   ,    4, "Coin A"				},
	{0x13, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x30, 0x00, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x13, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Enemies"				},
	{0x14, 0x01, 0x01, 0x01, "Few (Easy)"			},
	{0x14, 0x01, 0x01, 0x00, "Many (Hard)"			},

	{0   , 0xfe, 0   ,    2, "Enemy Shots"			},
	{0x14, 0x01, 0x02, 0x02, "Few (Easy)"			},
	{0x14, 0x01, 0x02, 0x00, "Many (Hard)"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x14, 0x01, 0x0c, 0x0c, "100K 400K"			},
	{0x14, 0x01, 0x0c, 0x08, "200K 400K"			},
	{0x14, 0x01, 0x0c, 0x04, "300K 400K"			},
	{0x14, 0x01, 0x0c, 0x00, "400K 400K"			},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x14, 0x01, 0x30, 0x30, "3"					},
	{0x14, 0x01, 0x30, 0x20, "4"					},
	{0x14, 0x01, 0x30, 0x10, "5"					},
	{0x14, 0x01, 0x30, 0x00, "99 (Cheat)"			},

	{0   , 0xfe, 0   ,    2, "Language"				},
	{0x14, 0x01, 0x80, 0x00, "English"				},
	{0x14, 0x01, 0x80, 0x80, "Japanese"				},
};

STDDIPINFO(Tokio)

static struct BurnRomInfo BublboblRomDesc[] = {
	{ "a78-06-1.51",   		0x08000, 0x567934b6, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "a78-05-1.52",   		0x10000, 0x9f8ee242, BRF_ESS | BRF_PRG }, //  1
	
	{ "a78-08.37",     		0x08000, 0xae11a07b, BRF_ESS | BRF_PRG }, //  2	Z80 #2 Program 
	
	{ "a78-07.46",     		0x08000, 0x4f9a26e8, BRF_ESS | BRF_PRG }, //  3	Z80 #3 Program 
	
	{ "a78-01.17",     		0x01000, 0xb1bfb53d, BRF_ESS | BRF_PRG }, //  4	MCU Program 
	
	{ "a78-09.12",     		0x08000, 0x20358c22, BRF_GRA },	     	  //  5	Tiles
	{ "a78-10.13",     		0x08000, 0x930168a9, BRF_GRA },	     	  //  6
	{ "a78-11.14",     		0x08000, 0x9773e512, BRF_GRA },	     	  //  7
	{ "a78-12.15",     		0x08000, 0xd045549b, BRF_GRA },	     	  //  8
	{ "a78-13.16",     		0x08000, 0xd0af35c5, BRF_GRA },	     	  //  9
	{ "a78-14.17",     		0x08000, 0x7b5369a8, BRF_GRA },	     	  // 10
	{ "a78-15.30",     		0x08000, 0x6b61a413, BRF_GRA },	          // 11
	{ "a78-16.31",     		0x08000, 0xb5492d97, BRF_GRA },	     	  // 12
	{ "a78-17.32",     		0x08000, 0xd69762d5, BRF_GRA },	     	  // 13
	{ "a78-18.33",     		0x08000, 0x9f243b68, BRF_GRA },	     	  // 14
	{ "a78-19.34",     		0x08000, 0x66e9438c, BRF_GRA },	     	  // 15
	{ "a78-20.35",     		0x08000, 0x9ef863ad, BRF_GRA },	     	  // 16
	
	{ "a71-25.41",     		0x00100, 0x2d0f8545, BRF_GRA },	     	  // 17	PROMs
	
	{ "pal16l8.bin",   		0x00001, 0x00000000, BRF_OPT | BRF_NODUMP }, //  18	PLDs
	{ "pal16l8.bin",   		0x00001, 0x00000000, BRF_OPT | BRF_NODUMP }, //  19
	{ "pal16r4.bin",   		0x00001, 0x00000000, BRF_OPT | BRF_NODUMP }, //  20
};

STD_ROM_PICK(Bublbobl)
STD_ROM_FN(Bublbobl)

static struct BurnRomInfo BublbobluRomDesc[] = {
	{ "a78-06.51",   		0x08000, 0xa6345edd, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "a78-05.52",   		0x10000, 0xb31d2edc, BRF_ESS | BRF_PRG }, //  1
	
	{ "a78-08.37",     		0x08000, 0xd544be2e, BRF_ESS | BRF_PRG }, //  2	Z80 #2 Program 
	
	{ "a78-07.46",     		0x08000, 0x4f9a26e8, BRF_ESS | BRF_PRG }, //  3	Z80 #3 Program 
	
	{ "a78-01.17",     		0x01000, 0xb1bfb53d, BRF_ESS | BRF_PRG }, //  4	MCU Program 
	
	{ "a78-09.12",     		0x08000, 0x20358c22, BRF_GRA },	     	  //  5	Tiles
	{ "a78-10.13",     		0x08000, 0x930168a9, BRF_GRA },	     	  //  6
	{ "a78-11.14",     		0x08000, 0x9773e512, BRF_GRA },	     	  //  7
	{ "a78-12.15",     		0x08000, 0xd045549b, BRF_GRA },	     	  //  8
	{ "a78-13.16",     		0x08000, 0xd0af35c5, BRF_GRA },	     	  //  9
	{ "a78-14.17",     		0x08000, 0x7b5369a8, BRF_GRA },	     	  // 10
	{ "a78-15.30",     		0x08000, 0x6b61a413, BRF_GRA },	          // 11
	{ "a78-16.31",     		0x08000, 0xb5492d97, BRF_GRA },	     	  // 12
	{ "a78-17.32",     		0x08000, 0xd69762d5, BRF_GRA },	     	  // 13
	{ "a78-18.33",     		0x08000, 0x9f243b68, BRF_GRA },	     	  // 14
	{ "a78-19.34",     		0x08000, 0x66e9438c, BRF_GRA },	     	  // 15
	{ "a78-20.35",     		0x08000, 0x9ef863ad, BRF_GRA },	     	  // 16
	
	{ "a71-25.41",     		0x00100, 0x2d0f8545, BRF_GRA },	     	  // 17	PROMs
	
	{ "pal16l8.bin",   		0x00001, 0x00000000, BRF_OPT | BRF_NODUMP }, //  18	PLDs
	{ "pal16l8.bin",   		0x00001, 0x00000000, BRF_OPT | BRF_NODUMP }, //  19
	{ "pal16r4.bin",   		0x00001, 0x00000000, BRF_OPT | BRF_NODUMP }, //  20
};

STD_ROM_PICK(Bublboblu)
STD_ROM_FN(Bublboblu)

static struct BurnRomInfo Bublbob1RomDesc[] = {
	{ "a78-06.51",     		0x08000, 0x32c8305b, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "a78-05.52",     		0x10000, 0x53f4bc6e, BRF_ESS | BRF_PRG }, //  1
	
	{ "a78-08.37",     		0x08000, 0xae11a07b, BRF_ESS | BRF_PRG }, //  2	Z80 #2 Program 
	
	{ "a78-07.46",     		0x08000, 0x4f9a26e8, BRF_ESS | BRF_PRG }, //  3	Z80 #3 Program 
	
	{ "a78-01.17",     		0x01000, 0xb1bfb53d, BRF_ESS | BRF_PRG }, //  4	MCU Program 
	
	{ "a78-09.12",     		0x08000, 0x20358c22, BRF_GRA },	     	  //  5	Tiles
	{ "a78-10.13",     		0x08000, 0x930168a9, BRF_GRA },	     	  //  6
	{ "a78-11.14",     		0x08000, 0x9773e512, BRF_GRA },	     	  //  7
	{ "a78-12.15",     		0x08000, 0xd045549b, BRF_GRA },	     	  //  8
	{ "a78-13.16",     		0x08000, 0xd0af35c5, BRF_GRA },	     	  //  9
	{ "a78-14.17",     		0x08000, 0x7b5369a8, BRF_GRA },	     	  // 10
	{ "a78-15.30",     		0x08000, 0x6b61a413, BRF_GRA },	     	  // 11
	{ "a78-16.31",     		0x08000, 0xb5492d97, BRF_GRA },	     	  // 12
	{ "a78-17.32",     		0x08000, 0xd69762d5, BRF_GRA },	     	  // 13
	{ "a78-18.33",     		0x08000, 0x9f243b68, BRF_GRA },	     	  // 14
	{ "a78-19.34",     		0x08000, 0x66e9438c, BRF_GRA },	     	  // 15
	{ "a78-20.35",     		0x08000, 0x9ef863ad, BRF_GRA },	     	  // 16
	
	{ "a71-25.41",     		0x00100, 0x2d0f8545, BRF_GRA },	     	  // 17	PROMs
	
	{ "pal16l8.bin",   		0x00001, 0x00000000, BRF_OPT | BRF_NODUMP }, //  18	PLDs
	{ "pal16l8.bin",   		0x00001, 0x00000000, BRF_OPT | BRF_NODUMP }, //  19
	{ "pal16r4.bin",   		0x00001, 0x00000000, BRF_OPT | BRF_NODUMP }, //  20
};

STD_ROM_PICK(Bublbob1)
STD_ROM_FN(Bublbob1)

static struct BurnRomInfo BublbobrRomDesc[] = {
	{ "a78-25.51",     		0x08000, 0x2d901c9d, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "a78-24.52",     		0x10000, 0xb7afedc4, BRF_ESS | BRF_PRG }, //  1
	
	{ "a78-08.37",     		0x08000, 0xae11a07b, BRF_ESS | BRF_PRG }, //  2	Z80 #2 Program 
	
	{ "a78-07.46",     		0x08000, 0x4f9a26e8, BRF_ESS | BRF_PRG }, //  3	Z80 #3 Program 
	
	{ "a78-01.17",     		0x01000, 0xb1bfb53d, BRF_ESS | BRF_PRG }, //  4	MCU Program 
	
	{ "a78-09.12",     		0x08000, 0x20358c22, BRF_GRA },	     	  //  5	Tiles
	{ "a78-10.13",     		0x08000, 0x930168a9, BRF_GRA },	     	  //  6
	{ "a78-11.14",     		0x08000, 0x9773e512, BRF_GRA },	     	  //  7
	{ "a78-12.15",     		0x08000, 0xd045549b, BRF_GRA },	     	  //  8
	{ "a78-13.16",     		0x08000, 0xd0af35c5, BRF_GRA },	     	  //  9
	{ "a78-14.17",     		0x08000, 0x7b5369a8, BRF_GRA },	     	  // 10
	{ "a78-15.30",     		0x08000, 0x6b61a413, BRF_GRA },	     	  // 11
	{ "a78-16.31",     		0x08000, 0xb5492d97, BRF_GRA },	     	  // 12
	{ "a78-17.32",     		0x08000, 0xd69762d5, BRF_GRA },	     	  // 13
	{ "a78-18.33",     		0x08000, 0x9f243b68, BRF_GRA },	     	  // 14
	{ "a78-19.34",     		0x08000, 0x66e9438c, BRF_GRA },	     	  // 15
	{ "a78-20.35",     		0x08000, 0x9ef863ad, BRF_GRA },	     	  // 16
	
	{ "a71-25.41",     		0x00100, 0x2d0f8545, BRF_GRA },	     	  // 17	PROMs
	
	{ "pal16l8.bin",   		0x00001, 0x00000000, BRF_OPT | BRF_NODUMP }, //  18	PLDs
	{ "pal16l8.bin",   		0x00001, 0x00000000, BRF_OPT | BRF_NODUMP }, //  19
	{ "pal16r4.bin",   		0x00001, 0x00000000, BRF_OPT | BRF_NODUMP }, //  20
};

STD_ROM_PICK(Bublbobr)
STD_ROM_FN(Bublbobr)

static struct BurnRomInfo Bubbobr1RomDesc[] = {
	{ "a78-06.51",     		0x08000, 0x32c8305b, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "a78-21.52",     		0x10000, 0x2844033d, BRF_ESS | BRF_PRG }, //  1
	
	{ "a78-08.37",     		0x08000, 0xae11a07b, BRF_ESS | BRF_PRG }, //  2	Z80 #2 Program 
	
	{ "a78-07.46",     		0x08000, 0x4f9a26e8, BRF_ESS | BRF_PRG }, //  3	Z80 #3 Program 
	
	{ "a78-01.17",     		0x01000, 0xb1bfb53d, BRF_ESS | BRF_PRG }, //  4	MCU Program 
	
	{ "a78-09.12",     		0x08000, 0x20358c22, BRF_GRA },	     	  //  5	Tiles
	{ "a78-10.13",     		0x08000, 0x930168a9, BRF_GRA },	     	  //  6
	{ "a78-11.14",     		0x08000, 0x9773e512, BRF_GRA },	     	  //  7
	{ "a78-12.15",     		0x08000, 0xd045549b, BRF_GRA },	     	  //  8
	{ "a78-13.16",     		0x08000, 0xd0af35c5, BRF_GRA },	     	  //  9
	{ "a78-14.17",     		0x08000, 0x7b5369a8, BRF_GRA },	     	  // 10
	{ "a78-15.30",     		0x08000, 0x6b61a413, BRF_GRA },	     	  // 11
	{ "a78-16.31",     		0x08000, 0xb5492d97, BRF_GRA },	     	  // 12
	{ "a78-17.32",     		0x08000, 0xd69762d5, BRF_GRA },	     	  // 13
	{ "a78-18.33",     		0x08000, 0x9f243b68, BRF_GRA },	     	  // 14
	{ "a78-19.34",     		0x08000, 0x66e9438c, BRF_GRA },	     	  // 15
	{ "a78-20.35",     		0x08000, 0x9ef863ad, BRF_GRA },	     	  // 16
	
	{ "a71-25.41",     		0x00100, 0x2d0f8545, BRF_GRA },	     	  // 17	PROMs
	
	{ "pal16l8.bin",   		0x00001, 0x00000000, BRF_OPT | BRF_NODUMP }, //  18	PLDs
	{ "pal16l8.bin",   		0x00001, 0x00000000, BRF_OPT | BRF_NODUMP }, //  19
	{ "pal16r4.bin",   		0x00001, 0x00000000, BRF_OPT | BRF_NODUMP }, //  20
};

STD_ROM_PICK(Bubbobr1)
STD_ROM_FN(Bubbobr1)

static struct BurnRomInfo BoblboblRomDesc[] = {
	{ "bb3",           		0x08000, 0x01f81936, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "bb5",           		0x08000, 0x13118eb1, BRF_ESS | BRF_PRG }, //  1
	{ "bb4",           		0x08000, 0xafda99d8, BRF_ESS | BRF_PRG }, //  2
	
	{ "a78-08.37",     		0x08000, 0xae11a07b, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program 
	
	{ "a78-07.46",     		0x08000, 0x4f9a26e8, BRF_ESS | BRF_PRG }, //  4	Z80 #3 Program 
	
	{ "a78-09.12",     		0x08000, 0x20358c22, BRF_GRA },	     	  //  5	Tiles
	{ "a78-10.13",     		0x08000, 0x930168a9, BRF_GRA },	     	  //  6
	{ "a78-11.14",     		0x08000, 0x9773e512, BRF_GRA },	     	  //  7
	{ "a78-12.15",     		0x08000, 0xd045549b, BRF_GRA },	     	  //  8
	{ "a78-13.16",     		0x08000, 0xd0af35c5, BRF_GRA },	     	  //  9
	{ "a78-14.17",     		0x08000, 0x7b5369a8, BRF_GRA },	     	  // 10
	{ "a78-15.30",     		0x08000, 0x6b61a413, BRF_GRA },	     	  // 11
	{ "a78-16.31",     		0x08000, 0xb5492d97, BRF_GRA },	     	  // 12
	{ "a78-17.32",     		0x08000, 0xd69762d5, BRF_GRA },	     	  // 13
	{ "a78-18.33",     		0x08000, 0x9f243b68, BRF_GRA },	     	  // 14
	{ "a78-19.34",     		0x08000, 0x66e9438c, BRF_GRA },	     	  // 15
	{ "a78-20.35",     		0x08000, 0x9ef863ad, BRF_GRA },	     	  // 16
	
	{ "a71-25.41",     		0x00100, 0x2d0f8545, BRF_GRA },	     	  // 17 PROMs
	
	{ "pal16r4.u36",   		0x00104, 0x22fe26ac, BRF_OPT }, 	      // 18 PLDs
	{ "pal16l8.u38",   		0x00104, 0xc02d9663, BRF_OPT },	     	  // 19
	{ "pal16l8.u4",    		0x00104, 0x077d20a8, BRF_OPT },	     	  // 20
};

STD_ROM_PICK(Boblbobl)
STD_ROM_FN(Boblbobl)

static struct BurnRomInfo BbreduxRomDesc[] = {
	{ "redux_bb3",     		0x08000, 0xd51de9f3, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "redux_bb5",     		0x08000, 0xd29d3444, BRF_ESS | BRF_PRG }, //  1
	{ "redux_bb4",     		0x08000, 0x984149bd, BRF_ESS | BRF_PRG }, //  2
	
	{ "a78-08.37",     		0x08000, 0xae11a07b, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program 
	
	{ "a78-07.46",     		0x08000, 0x4f9a26e8, BRF_ESS | BRF_PRG }, //  4	Z80 #3 Program 
	
	{ "a78-09.12",     		0x08000, 0x20358c22, BRF_GRA },	     	  //  5	Tiles
	{ "a78-10.13",     		0x08000, 0x930168a9, BRF_GRA },	     	  //  6
	{ "a78-11.14",     		0x08000, 0x9773e512, BRF_GRA },	     	  //  7
	{ "a78-12.15",     		0x08000, 0xd045549b, BRF_GRA },	     	  //  8
	{ "a78-13.16",     		0x08000, 0xd0af35c5, BRF_GRA },	     	  //  9
	{ "a78-14.17",     		0x08000, 0x7b5369a8, BRF_GRA },	     	  // 10
	{ "a78-15.30",     		0x08000, 0x6b61a413, BRF_GRA },	     	  // 11
	{ "a78-16.31",     		0x08000, 0xb5492d97, BRF_GRA },	     	  // 12
	{ "a78-17.32",     		0x08000, 0xd69762d5, BRF_GRA },	     	  // 13
	{ "a78-18.33",     		0x08000, 0x9f243b68, BRF_GRA },	     	  // 14
	{ "a78-19.34",     		0x08000, 0x66e9438c, BRF_GRA },	     	  // 15
	{ "a78-20.35",     		0x08000, 0x9ef863ad, BRF_GRA },	     	  // 16
	
	{ "a71-25.41",     		0x00100, 0x2d0f8545, BRF_GRA },	     	  // 17	PROMs
	
	{ "pal16r4.u36",   		0x00104, 0x22fe26ac, BRF_OPT }, 	     //  18	PLDs
	{ "pal16l8.u38",   		0x00104, 0xc02d9663, BRF_OPT },	     	 //  19
	{ "pal16l8.u4",    		0x00104, 0x077d20a8, BRF_OPT },	     	 //  20
};

STD_ROM_PICK(Bbredux)
STD_ROM_FN(Bbredux)

static struct BurnRomInfo BublboblbRomDesc[] = {
	{ "bbaladar.3",     	0x08000, 0x31bfc6fb, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "bbaladar.5",     	0x08000, 0x16386e9a, BRF_ESS | BRF_PRG }, //  1
	{ "bbaladar.4",     	0x08000, 0x0c4bcb07, BRF_ESS | BRF_PRG }, //  2
	
	{ "a78-08.37",     		0x08000, 0xae11a07b, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program 
	
	{ "a78-07.46",     		0x08000, 0x4f9a26e8, BRF_ESS | BRF_PRG }, //  4	Z80 #3 Program 
	
	{ "a78-09.12",     		0x08000, 0x20358c22, BRF_GRA },	     	  //  5	Tiles
	{ "a78-10.13",     		0x08000, 0x930168a9, BRF_GRA },	     	  //  6
	{ "a78-11.14",     		0x08000, 0x9773e512, BRF_GRA },	     	  //  7
	{ "a78-12.15",     		0x08000, 0xd045549b, BRF_GRA },	     	  //  8
	{ "a78-13.16",     		0x08000, 0xd0af35c5, BRF_GRA },	     	  //  9
	{ "a78-14.17",     		0x08000, 0x7b5369a8, BRF_GRA },	     	  // 10
	{ "a78-15.30",     		0x08000, 0x6b61a413, BRF_GRA },	     	  // 11
	{ "a78-16.31",     		0x08000, 0xb5492d97, BRF_GRA },	     	  // 12
	{ "a78-17.32",     		0x08000, 0xd69762d5, BRF_GRA },	     	  // 13
	{ "a78-18.33",     		0x08000, 0x9f243b68, BRF_GRA },	     	  // 14
	{ "a78-19.34",     		0x08000, 0x66e9438c, BRF_GRA },	     	  // 15
	{ "a78-20.35",     		0x08000, 0x9ef863ad, BRF_GRA },	     	  // 16
	
	{ "a71-25.41",     		0x00100, 0x2d0f8545, BRF_GRA },	     	  // 17	PROMs
	
	{ "pal16r4.u36",   		0x00104, 0x22fe26ac, BRF_OPT }, 	     //  18	PLDs
	{ "pal16l8.u38",   		0x00104, 0xc02d9663, BRF_OPT },	     	 //  19
	{ "pal16l8.u4",    		0x00104, 0x077d20a8, BRF_OPT },	     	 //  20
};

STD_ROM_PICK(Bublboblb)
STD_ROM_FN(Bublboblb)

static struct BurnRomInfo SboblboblRomDesc[] = {
	{ "cpu2-3.bin",    		0x08000, 0x2d9107b6, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "bb5",           		0x08000, 0x13118eb1, BRF_ESS | BRF_PRG }, //  1
	{ "cpu2-4.bin",    		0x08000, 0x3f9fed10, BRF_ESS | BRF_PRG }, //  2
	
	{ "a78-08.37",     		0x08000, 0xae11a07b, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program 
	
	{ "a78-07.46",     		0x08000, 0x4f9a26e8, BRF_ESS | BRF_PRG }, //  4	Z80 #3 Program 

	{ "gfx11.bin",     		0x10000, 0x76f2b367, BRF_GRA },	     	  //  5	Tiles
	{ "gfx10.bin",     		0x10000, 0xd370f499, BRF_GRA },	     	  //  6
	{ "a78-13.16",     		0x08000, 0xd0af35c5, BRF_GRA },	     	  //  7
	{ "a78-14.17",     		0x08000, 0x7b5369a8, BRF_GRA },	     	  //  8
	{ "gfx8.bin",      		0x10000, 0x677840e8, BRF_GRA },	     	  //  9
	{ "gfx7.bin",      		0x10000, 0x702f61c0, BRF_GRA },	     	  // 10
	{ "a78-19.34",     		0x08000, 0x66e9438c, BRF_GRA },	     	  // 11
	{ "a78-20.35",     		0x08000, 0x9ef863ad, BRF_GRA },	     	  // 12

	{ "a71-25.41",     		0x00100, 0x2d0f8545, BRF_GRA },	     	  // 13	PROMs
};

STD_ROM_PICK(Sboblbobl)
STD_ROM_FN(Sboblbobl)

static struct BurnRomInfo SboblboblaRomDesc[] = {
	{ "1c.bin",        		0x08000, 0xf304152a, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "1a.bin",        		0x08000, 0x0865209c, BRF_ESS | BRF_PRG }, //  1
	{ "1b.bin",        		0x08000, 0x1f29b5c0, BRF_ESS | BRF_PRG }, //  2
	
	{ "1e.rom",        		0x08000, 0xae11a07b, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program 
	
	{ "1d.rom",        		0x08000, 0x4f9a26e8, BRF_ESS | BRF_PRG }, //  4	Z80 #3 Program 
	
	{ "1l.rom",        		0x08000, 0x20358c22, BRF_GRA },	     	  //  5	Tiles
	{ "1m.rom",        		0x08000, 0x930168a9, BRF_GRA },	     	  //  6
	{ "1n.rom",        		0x08000, 0x9773e512, BRF_GRA },	     	  //  7
	{ "1o.rom",        		0x08000, 0xd045549b, BRF_GRA },	     	  //  8
	{ "1p.rom",        		0x08000, 0xd0af35c5, BRF_GRA },	     	  //  9
	{ "1q.rom",        		0x08000, 0x7b5369a8, BRF_GRA },	     	  // 10
	{ "1f.rom",        		0x08000, 0x6b61a413, BRF_GRA },	     	  // 11
	{ "1g.rom",        		0x08000, 0xb5492d97, BRF_GRA },	     	  // 12
	{ "1h.rom",        		0x08000, 0xd69762d5, BRF_GRA },	     	  // 13
	{ "1i.rom",        		0x08000, 0x9f243b68, BRF_GRA },	     	  // 14
	{ "1j.rom",        		0x08000, 0x66e9438c, BRF_GRA },	     	  // 15
	{ "1k.rom",        		0x08000, 0x9ef863ad, BRF_GRA },	     	  // 16
	
	{ "a71-25.41",     		0x00100, 0x2d0f8545, BRF_GRA },	     	  // 17	PROMs
};

STD_ROM_PICK(Sboblbobla)
STD_ROM_FN(Sboblbobla)

static struct BurnRomInfo SboblboblbRomDesc[] = {
	{ "bbb-3.rom",     		0x08000, 0xf304152a, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "bb5",           		0x08000, 0x13118eb1, BRF_ESS | BRF_PRG }, //  1
	{ "bbb-4.rom",     		0x08000, 0x94c75591, BRF_ESS | BRF_PRG }, //  2
	
	{ "a78-08.37",     		0x08000, 0xae11a07b, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program 
	
	{ "a78-07.46",     		0x08000, 0x4f9a26e8, BRF_ESS | BRF_PRG }, //  4	Z80 #3 Program 
	
	{ "a78-09.12",     		0x08000, 0x20358c22, BRF_GRA },	     	  //  5	Tiles
	{ "a78-10.13",     		0x08000, 0x930168a9, BRF_GRA },	     	  //  6
	{ "a78-11.14",     		0x08000, 0x9773e512, BRF_GRA },	     	  //  7
	{ "a78-12.15",     		0x08000, 0xd045549b, BRF_GRA },	     	  //  8
	{ "a78-13.16",     		0x08000, 0xd0af35c5, BRF_GRA },	     	  //  9
	{ "a78-14.17",     		0x08000, 0x7b5369a8, BRF_GRA },	     	  // 10
	{ "a78-15.30",     		0x08000, 0x6b61a413, BRF_GRA },	     	  // 11
	{ "a78-16.31",     		0x08000, 0xb5492d97, BRF_GRA },	     	  // 12
	{ "a78-17.32",     		0x08000, 0xd69762d5, BRF_GRA },	     	  // 13
	{ "a78-18.33",     		0x08000, 0x9f243b68, BRF_GRA },	     	  // 14
	{ "a78-19.34",     		0x08000, 0x66e9438c, BRF_GRA },	     	  // 15
	{ "a78-20.35",     		0x08000, 0x9ef863ad, BRF_GRA },	     	  // 16
	
	{ "a71-25.41",     		0x00100, 0x2d0f8545, BRF_GRA },	     	  // 17	PROMs
};

STD_ROM_PICK(Sboblboblb)
STD_ROM_FN(Sboblboblb)

static struct BurnRomInfo SboblboblcRomDesc[] = {
	{ "3",     				0x08000, 0xf2d44846, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "5",           		0x08000, 0x3c5e4441, BRF_ESS | BRF_PRG }, //  1
	{ "4",     			    0x08000, 0x1f29b5c0, BRF_ESS | BRF_PRG }, //  2
	
	{ "1",     				0x08000, 0xae11a07b, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program 
	
	{ "2",     				0x08000, 0x4f9a26e8, BRF_ESS | BRF_PRG }, //  4	Z80 #3 Program 
	
	{ "12",     			0x08000, 0x20358c22, BRF_GRA },	     	  //  5	Tiles
	{ "13",     			0x08000, 0x930168a9, BRF_GRA },	     	  //  6
	{ "14",     			0x08000, 0x9773e512, BRF_GRA },	     	  //  7
	{ "15",     			0x08000, 0xd045549b, BRF_GRA },	     	  //  8
	{ "16",     			0x08000, 0xd0af35c5, BRF_GRA },	     	  //  9
	{ "17",     			0x08000, 0x7b5369a8, BRF_GRA },	     	  // 10
	{ "6",     				0x08000, 0x6b61a413, BRF_GRA },	     	  // 11
	{ "7",     				0x08000, 0xb5492d97, BRF_GRA },	     	  // 12
	{ "8",     				0x08000, 0xd69762d5, BRF_GRA },	     	  // 13
	{ "9",     				0x08000, 0x9f243b68, BRF_GRA },	     	  // 14
	{ "10",     			0x08000, 0x66e9438c, BRF_GRA },	     	  // 15
	{ "11",     			0x08000, 0x9ef863ad, BRF_GRA },	     	  // 16
	
	{ "a71-25.41",     		0x00100, 0x2d0f8545, BRF_GRA },	     	  // 17	PROMs
};

STD_ROM_PICK(Sboblboblc)
STD_ROM_FN(Sboblboblc)

static struct BurnRomInfo SboblbobldRomDesc[] = {
	{ "3.bin",     			0x08000, 0x524cdc4f, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "5.bin",           	0x08000, 0x13118eb1, BRF_ESS | BRF_PRG }, //  1
	{ "4.bin",     			0x08000, 0x13fe9baa, BRF_ESS | BRF_PRG }, //  2
	
	{ "1.bin",     			0x08000, 0xae11a07b, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program 
	
	{ "2.bin",     			0x08000, 0x4f9a26e8, BRF_ESS | BRF_PRG }, //  4	Z80 #3 Program 
	
	{ "12",     			0x08000, 0x20358c22, BRF_GRA },	     	  //  5	Tiles
	{ "13",     			0x08000, 0x930168a9, BRF_GRA },	     	  //  6
	{ "14",     			0x08000, 0x9773e512, BRF_GRA },	     	  //  7
	{ "15",     			0x08000, 0xd045549b, BRF_GRA },	     	  //  8
	{ "16",     			0x08000, 0xd0af35c5, BRF_GRA },	     	  //  9
	{ "17",     			0x08000, 0x7b5369a8, BRF_GRA },	     	  // 10
	{ "6",     				0x08000, 0x6b61a413, BRF_GRA },	     	  // 11
	{ "7",     				0x08000, 0xb5492d97, BRF_GRA },	     	  // 12
	{ "8",     				0x08000, 0xd69762d5, BRF_GRA },	     	  // 13
	{ "9",     				0x08000, 0x9f243b68, BRF_GRA },	     	  // 14
	{ "10",     			0x08000, 0x66e9438c, BRF_GRA },	     	  // 15
	{ "11",     			0x08000, 0x9ef863ad, BRF_GRA },	     	  // 16
	
	{ "a71-25.41",     		0x00100, 0x2d0f8545, BRF_GRA },	     	  // 17	PROMs
};

STD_ROM_PICK(Sboblbobld)
STD_ROM_FN(Sboblbobld)

static struct BurnRomInfo SboblbobleRomDesc[] = {
	// identical to sboblbobld but for the first program ROM
	{ "1f",     			0x08000, 0xbde89043, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "1h",           		0x08000, 0x13118eb1, BRF_ESS | BRF_PRG }, //  1
	{ "1l",     			0x08000, 0x13fe9baa, BRF_ESS | BRF_PRG }, //  2
	
	{ "b-1.3f",     		0x08000, 0xae11a07b, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program 
	
	{ "b-2.1s",     		0x08000, 0x4f9a26e8, BRF_ESS | BRF_PRG }, //  4	Z80 #3 Program 
	
	{ "b-6.6b",     		0x08000, 0x20358c22, BRF_GRA },	     	  //  5	Tiles
	{ "b-7.8b",     		0x08000, 0x930168a9, BRF_GRA },	     	  //  6
	{ "b-8.9b",     		0x08000, 0x9773e512, BRF_GRA },	     	  //  7
	{ "b-9.11b",     		0x08000, 0xd045549b, BRF_GRA },	     	  //  8
	{ "b-10.12b",     		0x08000, 0xd0af35c5, BRF_GRA },	     	  //  9
	{ "b-11.14b",     		0x08000, 0x7b5369a8, BRF_GRA },	     	  // 10
	{ "b-12.6d",     		0x08000, 0x6b61a413, BRF_GRA },	     	  // 11
	{ "b-13.8d",     		0x08000, 0xb5492d97, BRF_GRA },	     	  // 12
	{ "b-14.9d",     		0x08000, 0xd69762d5, BRF_GRA },	     	  // 13
	{ "b-15.11d",     		0x08000, 0x9f243b68, BRF_GRA },	     	  // 14
	{ "b-16.12d",     		0x08000, 0x66e9438c, BRF_GRA },	     	  // 15
	{ "b-17.14d",     		0x08000, 0x9ef863ad, BRF_GRA },	     	  // 16
	
	// not provided for this set
	{ "a71-25.41",     		0x00100, 0x2d0f8545, BRF_GRA },	     	  // 17	PROMs
};

STD_ROM_PICK(Sboblboble)
STD_ROM_FN(Sboblboble)

static struct BurnRomInfo SboblboblfRomDesc[] = {
	// single layer PCB '8001 AX'
	{ "a2.bin",    			0x08000, 0x524cdc4f, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "a4.bin",           	0x08000, 0x13118eb1, BRF_ESS | BRF_PRG }, //  1
	{ "a3.bin",    			0x08000, 0x94c75591, BRF_ESS | BRF_PRG }, //  2
	
	{ "a5.bin",     		0x08000, 0xae11a07b, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program 
	
	{ "a1.bin",     		0x08000, 0x4f9a26e8, BRF_ESS | BRF_PRG }, //  4	Z80 #3 Program 

	{ "a11.bin",     		0x10000, 0x76f2b367, BRF_GRA },	     	  //  5	Tiles
	{ "a10.bin",     		0x10000, 0xd370f499, BRF_GRA },	     	  //  6
	{ "a9.bin",     		0x08000, 0x7b5369a8, BRF_GRA },	     	  //  7
	{ "a8.bin",      		0x10000, 0x677840e8, BRF_GRA },	     	  //  8
	{ "a7.bin",      		0x10000, 0x702f61c0, BRF_GRA },	     	  //  9
	{ "a6.bin",     		0x08000, 0x9ef863ad, BRF_GRA },	     	  // 10

	{ "a71-25.41",     		0x00100, 0x2d0f8545, BRF_GRA },	     	  // 11	PROMs
};

STD_ROM_PICK(Sboblboblf)
STD_ROM_FN(Sboblboblf)

static struct BurnRomInfo Bub68705RomDesc[] = {
	{ "2.bin",         		0x08000, 0x32c8305b, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "3-1.bin",       		0x08000, 0x980c2615, BRF_ESS | BRF_PRG }, //  1
	{ "3.bin",         		0x08000, 0xe6c698f2, BRF_ESS | BRF_PRG }, //  2
	
	{ "4.bin",         		0x08000, 0xae11a07b, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program 
	
	{ "1.bin",         		0x08000, 0x4f9a26e8, BRF_ESS | BRF_PRG }, //  4	Z80 #3 Program 
	
	{ "a78-09.12",     		0x08000, 0x20358c22, BRF_GRA },	     	  //  5	Tiles
	{ "a78-10.13",     		0x08000, 0x930168a9, BRF_GRA },	     	  //  6
	{ "a78-11.14",     		0x08000, 0x9773e512, BRF_GRA },	     	  //  7
	{ "a78-12.15",     		0x08000, 0xd045549b, BRF_GRA },	     	  //  8
	{ "a78-13.16",     		0x08000, 0xd0af35c5, BRF_GRA },	     	  //  9
	{ "a78-14.17",     		0x08000, 0x7b5369a8, BRF_GRA },	     	  // 10
	{ "a78-15.30",     		0x08000, 0x6b61a413, BRF_GRA },	     	  // 11
	{ "a78-16.31",     		0x08000, 0xb5492d97, BRF_GRA },	     	  // 12
	{ "a78-17.32",     		0x08000, 0xd69762d5, BRF_GRA },	     	  // 13
	{ "a78-18.33",     		0x08000, 0x9f243b68, BRF_GRA },	     	  // 14
	{ "a78-19.34",     		0x08000, 0x66e9438c, BRF_GRA },	     	  // 15
	{ "a78-20.35",     		0x08000, 0x9ef863ad, BRF_GRA },	     	  // 16
	
	{ "a71-25.41",     		0x00100, 0x2d0f8545, BRF_GRA },	     	  // 17	PROMs
	
	{ "68705.bin",     		0x00800, 0x78caa635, BRF_ESS | BRF_PRG }, // 18	68705 Program Code
};

STD_ROM_PICK(Bub68705)
STD_ROM_FN(Bub68705)

static struct BurnRomInfo Bub68705aRomDesc[] = {
	{ "2.bin",         		0x08000, 0x32c8305b, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "3-1.bin",       		0x08000, 0x980c2615, BRF_ESS | BRF_PRG }, //  1
	{ "3.bin",         		0x08000, 0xe6c698f2, BRF_ESS | BRF_PRG }, //  2
	
	{ "4.bin",         		0x08000, 0xae11a07b, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program 
	
	{ "1.bin",         		0x08000, 0x4f9a26e8, BRF_ESS | BRF_PRG }, //  4	Z80 #3 Program 
	
	{ "a78-09.12",     		0x08000, 0x20358c22, BRF_GRA },	     	  //  5	Tiles
	{ "a78-10.13",     		0x08000, 0x930168a9, BRF_GRA },	     	  //  6
	{ "a78-11.14",     		0x08000, 0x9773e512, BRF_GRA },	     	  //  7
	{ "a78-12.15",     		0x08000, 0xd045549b, BRF_GRA },	     	  //  8
	{ "a78-13.16",     		0x08000, 0xd0af35c5, BRF_GRA },	     	  //  9
	{ "a78-14.17",     		0x08000, 0x7b5369a8, BRF_GRA },	     	  // 10
	{ "a78-15.30",     		0x08000, 0x6b61a413, BRF_GRA },	     	  // 11
	{ "a78-16.31",     		0x08000, 0xb5492d97, BRF_GRA },	     	  // 12
	{ "a78-17.32",     		0x08000, 0xd69762d5, BRF_GRA },	     	  // 13
	{ "a78-18.33",     		0x08000, 0x9f243b68, BRF_GRA },	     	  // 14
	{ "a78-19.34",     		0x08000, 0x66e9438c, BRF_GRA },	     	  // 15
	{ "a78-20.35",     		0x08000, 0x9ef863ad, BRF_GRA },	     	  // 16
	
	{ "a71-25.41",     		0x00100, 0x2d0f8545, BRF_GRA },	     	  // 17	PROMs
	
	{ "cpu68705.bin",  		0x00800, 0x32bffbf4, BRF_ESS | BRF_PRG }, // 18	68705 Program Code
};

STD_ROM_PICK(Bub68705a)
STD_ROM_FN(Bub68705a)


static struct BurnRomInfo DlandRomDesc[] = {
	{ "dl_3.u69",	   		0x08000, 0x01eb3e4f, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "dl_5.u67",	   		0x08000, 0x75740b61, BRF_ESS | BRF_PRG }, //  1
	{ "dl_4.u68",	   		0x08000, 0xc6a3776f, BRF_ESS | BRF_PRG }, //  2

	{ "dl_1.u42",      		0x08000, 0xae11a07b, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program 
	
	{ "dl_2.u74",      		0x08000, 0x4f9a26e8, BRF_ESS | BRF_PRG }, //  4	Z80 #3 Program 

	{ "dl_6.58",       		0x10000, 0x6352d3fa, BRF_GRA },	     	  //  5	Tiles
	{ "dl_7.59",       		0x10000, 0x37a38b69, BRF_GRA },	     	  //  6
	{ "dl_8.60",       		0x10000, 0x509ee5b1, BRF_GRA },	     	  //  7
	{ "dl_9.61",       		0x10000, 0xae8514d7, BRF_GRA },	     	  //  8
	{ "dl_10.62",      		0x10000, 0x6d406fb7, BRF_GRA },	     	  //  9
	{ "dl_11.63",      		0x10000, 0xbdf9c0ab, BRF_GRA },	     	  // 10

	{ "a71-25.41",     		0x00100, 0x2d0f8545, BRF_GRA },	     	  // 11	PROMs
};

STD_ROM_PICK(Dland)
STD_ROM_FN(Dland)

static struct BurnRomInfo BublcaveRomDesc[] = {
	{ "bublcave-06.51",    	0x08000, 0xe8b9af5e, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "bublcave-05.52",    	0x10000, 0xcfe14cb8, BRF_ESS | BRF_PRG }, //  1
	
	{ "bublcave-08.37",    	0x08000, 0xa9384086, BRF_ESS | BRF_PRG }, //  2	Z80 #2 Program 
	
	{ "a78-07.46",     	   	0x08000, 0x4f9a26e8, BRF_ESS | BRF_PRG }, //  3	Z80 #3 Program 
	
	{ "a78-01.17",     		0x01000, 0xb1bfb53d, BRF_ESS | BRF_PRG }, //  4	MCU Program 
	
	{ "bublcave-09.12",    	0x08000, 0xb90b7eef, BRF_GRA },	     	  //  5	Tiles
	{ "bublcave-10.13",    	0x08000, 0x4fb22f05, BRF_GRA },	     	  //  6
	{ "bublcave-11.14",     0x08000, 0x9773e512, BRF_GRA },	     	  //  7
	{ "bublcave-12.15",    	0x08000, 0xe49eb49e, BRF_GRA },	     	  //  8
	{ "bublcave-13.16",     0x08000, 0x61919734, BRF_GRA },	     	  //  9
	{ "bublcave-14.17",    	0x08000, 0x7e3a13bd, BRF_GRA },	     	  // 10
	{ "bublcave-15.30",    	0x08000, 0xc253c73a, BRF_GRA },	     	  // 11
	{ "bublcave-16.31",    	0x08000, 0xe66c92ee, BRF_GRA },	     	  // 12
	{ "bublcave-17.32",     0x08000, 0xd69762d5, BRF_GRA },	     	  // 13
	{ "bublcave-18.33",    	0x08000, 0x47ee2544, BRF_GRA },	     	  // 14
	{ "bublcave-19.34",    	0x08000, 0x1ceeb1fa, BRF_GRA },	     	  // 15
	{ "bublcave-20.35",    	0x08000, 0x64322e24, BRF_GRA },	     	  // 16
	
	{ "a71-25.41",     		0x00100, 0x2d0f8545, BRF_GRA },	     	  // 17	PROMs
};

STD_ROM_PICK(Bublcave)
STD_ROM_FN(Bublcave)

static struct BurnRomInfo BoblcaveRomDesc[] = {
	{ "lc12_bb3",     		0x08000, 0xdddc9a24, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "lc12_bb5",     		0x08000, 0x0bc4de52, BRF_ESS | BRF_PRG }, //  1
	{ "lc12_bb4",     		0x08000, 0xbd7afdf4, BRF_ESS | BRF_PRG }, //  2
	
	{ "bublcave-08.37",     0x08000, 0xa9384086, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program 
	
	{ "a78-07.46",     		0x08000, 0x4f9a26e8, BRF_ESS | BRF_PRG }, //  4	Z80 #3 Program 
	
	{ "bublcave-09.12",    	0x08000, 0xb90b7eef, BRF_GRA },	     	  //  5	Tiles
	{ "bublcave-10.13",    	0x08000, 0x4fb22f05, BRF_GRA },	     	  //  6
	{ "bublcave-11.14",     0x08000, 0x9773e512, BRF_GRA },	     	  //  7
	{ "bublcave-12.15",    	0x08000, 0xe49eb49e, BRF_GRA },	     	  //  8
	{ "bublcave-13.16",     0x08000, 0x61919734, BRF_GRA },	     	  //  9
	{ "bublcave-14.17",    	0x08000, 0x7e3a13bd, BRF_GRA },	     	  // 10
	{ "bublcave-15.30",    	0x08000, 0xc253c73a, BRF_GRA },	     	  // 11
	{ "bublcave-16.31",    	0x08000, 0xe66c92ee, BRF_GRA },	     	  // 12
	{ "bublcave-17.32",     0x08000, 0xd69762d5, BRF_GRA },	     	  // 13
	{ "bublcave-18.33",    	0x08000, 0x47ee2544, BRF_GRA },	     	  // 14
	{ "bublcave-19.34",    	0x08000, 0x1ceeb1fa, BRF_GRA },	     	  // 15
	{ "bublcave-20.35",    	0x08000, 0x64322e24, BRF_GRA },	     	  // 16
	
	{ "a71-25.41",     		0x00100, 0x2d0f8545, BRF_GRA },	     	  // 17	PROMs
	
	{ "pal16r4.u36",   		0x00104, 0x22fe26ac, BRF_OPT }, 	     //  18	PLDs
	{ "pal16l8.u38",   		0x00104, 0xc02d9663, BRF_OPT },	     	 //  19
	{ "pal16l8.u4",    		0x00104, 0x077d20a8, BRF_OPT },	     	 //  20
};

STD_ROM_PICK(Boblcave)
STD_ROM_FN(Boblcave)

static struct BurnRomInfo Bublcave11RomDesc[] = {
	{ "bublcave10-06.51",  	0x08000, 0x185cc219, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "bublcave11-05.52",  	0x10000, 0xb6b02df3, BRF_ESS | BRF_PRG }, //  1
	
	{ "bublcave11-08.37",  	0x08000, 0xc5d14e62, BRF_ESS | BRF_PRG }, //  2	Z80 #2 Program 
	
	{ "a78-07.46",         	0x08000, 0x4f9a26e8, BRF_ESS | BRF_PRG }, //  3	Z80 #3 Program 
	
	{ "a78-01.17",         	0x01000, 0xb1bfb53d, BRF_ESS | BRF_PRG }, //  4	MCU Program 
	
	{ "bublcave-09.12",    	0x08000, 0xb90b7eef, BRF_GRA },	     	  //  5	Tiles
	{ "bublcave-10.13",    	0x08000, 0x4fb22f05, BRF_GRA },	     	  //  6
	{ "bublcave-11.14",    	0x08000, 0x9773e512, BRF_GRA },	     	  //  7
	{ "bublcave-12.15",    	0x08000, 0xe49eb49e, BRF_GRA },	     	  //  8
	{ "bublcave-13.16",    	0x08000, 0x61919734, BRF_GRA },	     	  //  9
	{ "bublcave-14.17",    	0x08000, 0x7e3a13bd, BRF_GRA },	     	  // 10
	{ "bublcave-15.30",    	0x08000, 0xc253c73a, BRF_GRA },	     	  // 11
	{ "bublcave-16.31",    	0x08000, 0xe66c92ee, BRF_GRA },	     	  // 12
	{ "bublcave-17.32",    	0x08000, 0xd69762d5, BRF_GRA },	     	  // 13
	{ "bublcave-18.33",    	0x08000, 0x47ee2544, BRF_GRA },	     	  // 14
	{ "bublcave-19.34",    	0x08000, 0x1ceeb1fa, BRF_GRA },	     	  // 15
	{ "bublcave-20.35",    	0x08000, 0x64322e24, BRF_GRA },	     	  // 16
	
	{ "a71-25.41",			0x00100, 0x2d0f8545, BRF_GRA },	     	  // 17	PROMs
};

STD_ROM_PICK(Bublcave11)
STD_ROM_FN(Bublcave11)

static struct BurnRomInfo Bublcave10RomDesc[] = {
	{ "bublcave10-06.51",   0x08000, 0x185cc219, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "bublcave10-05.52",   0x10000, 0x381cdde7, BRF_ESS | BRF_PRG }, //  1
	
	{ "bublcave10-08.37",   0x08000, 0x026a68e1, BRF_ESS | BRF_PRG }, //  2	Z80 #2 Program 
	
	{ "a78-07.46",          0x08000, 0x4f9a26e8, BRF_ESS | BRF_PRG }, //  3	Z80 #3 Program 
	
	{ "a78-01.17",          0x01000, 0xb1bfb53d, BRF_ESS | BRF_PRG }, //  4	MCU Program 
	
	{ "bublcave-09.12",     0x08000, 0xb90b7eef, BRF_GRA },	     	  //  5	Tiles
	{ "bublcave-10.13",     0x08000, 0x4fb22f05, BRF_GRA },	     	  //  6
	{ "bublcave-11.14",     0x08000, 0x9773e512, BRF_GRA },	     	  //  7
	{ "bublcave-12.15",     0x08000, 0xe49eb49e, BRF_GRA },	     	  //  8
	{ "bublcave-13.16",     0x08000, 0x61919734, BRF_GRA },	     	  //  9
	{ "bublcave-14.17",     0x08000, 0x7e3a13bd, BRF_GRA },	     	  // 10
	{ "bublcave-15.30",     0x08000, 0xc253c73a, BRF_GRA },	     	  // 11
	{ "bublcave-16.31",     0x08000, 0xe66c92ee, BRF_GRA },	     	  // 12
	{ "bublcave-17.32",     0x08000, 0xd69762d5, BRF_GRA },	     	  // 13
	{ "bublcave-18.33",     0x08000, 0x47ee2544, BRF_GRA },	     	  // 14
	{ "bublcave-19.34",     0x08000, 0x1ceeb1fa, BRF_GRA },	     	  // 15
	{ "bublcave-20.35",     0x08000, 0x64322e24, BRF_GRA },	     	  // 16
	
	{ "a71-25.41",			0x00100, 0x2d0f8545, BRF_GRA },	     	  // 17	PROMs
};

STD_ROM_PICK(Bublcave10)
STD_ROM_FN(Bublcave10)

static struct BurnRomInfo BublboblpRomDesc[] = {
	{ "maincpu.ic4",   		0x08000, 0x874ddd6c, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "maincpu.ic5",   		0x08000, 0x588cc602, BRF_ESS | BRF_PRG }, //  1
	
	{ "subcpu.ic1",    		0x08000, 0xe8187e8f, BRF_ESS | BRF_PRG }, //  2	Z80 #2 Program 
	
	{ "audiocpu.ic10",		0x08000, 0xc516c26e, BRF_ESS | BRF_PRG }, //  3	Z80 #3 Program 
	
	{ "c1.ic12",     		0x08000, 0x183d378b, BRF_GRA },	     	  //  4 Tiles
	{ "c3.ic13",     		0x08000, 0x55408ff9, BRF_GRA },	     	  //  5
	{ "c5.ic14",     		0x08000, 0x12cc5949, BRF_GRA },	     	  //  6
	{ "c7.ic15",     		0x08000, 0x10e24f35, BRF_GRA },	     	  //  7
	{ "c9.ic16",     		0x08000, 0xdec95961, BRF_GRA },	     	  //  8
	{ "c11.ic17",     		0x08000, 0x1c49d228, BRF_GRA },	     	  //  9
	{ "c0.ic30",     		0x08000, 0x39d0ce8f, BRF_GRA },	          // 10
	{ "c2.ic31",     		0x08000, 0xf705a512, BRF_GRA },	     	  // 11
	{ "c4.ic32",     		0x08000, 0x151df0eb, BRF_GRA },	     	  // 12
	{ "c6.ic33",     		0x08000, 0x7b737c1e, BRF_GRA },	     	  // 13
	{ "c8.ic34",     		0x08000, 0x1320e15d, BRF_GRA },	     	  // 14
	{ "c10.ic35",     		0x08000, 0x29c41387, BRF_GRA },	     	  // 15
	
	{ "a71-25.ic41",   		0x00100, 0x2d0f8545, BRF_GRA },	     	  // 16	PROMs
	
	{ "bublboblp.pal16l8.ic19",	0x00117, 0x4e1f119c, BRF_OPT }, //  17	PLDs
};

STD_ROM_PICK(Bublboblp)
STD_ROM_FN(Bublboblp)

static struct BurnRomInfo tokioRomDesc[] = {
	{ "a71-02-1.ic4",	0x8000, 0xBB8DABD7, BRF_ESS | BRF_PRG }, //  0 Z80 #1 Program
	{ "a71-03-1.ic5",	0x8000, 0xEE49B383, BRF_ESS | BRF_PRG }, //  1
	{ "a71-04.ic6",		0x8000, 0xa0a4ce0e, BRF_ESS | BRF_PRG }, //  2
	{ "a71-05.ic7",		0x8000, 0x6da0b945, BRF_ESS | BRF_PRG }, //  3
	{ "a71-06-1.ic8",	0x8000, 0x56927b3f, BRF_ESS | BRF_PRG }, //  4

	{ "a71-01.ic1",		0x8000, 0x0867c707, BRF_ESS | BRF_PRG }, //  5 Z80 #2 Program 

	{ "a71-07.ic10",	0x8000, 0xf298cc7b, BRF_ESS | BRF_PRG }, //  6 Z80 #3 Program 

	{ "a71-08.ic12",	0x8000, 0x0439ab13, BRF_GRA },	         //  7 Tiles
	{ "a71-09.ic13",	0x8000, 0xedb3d2ff, BRF_GRA },	         //  8
	{ "a71-10.ic14",	0x8000, 0x69f0888c, BRF_GRA },	         //  9
	{ "a71-11.ic15",	0x8000, 0x4ae07c31, BRF_GRA },	         // 10
	{ "a71-12.ic16",	0x8000, 0x3f6bd706, BRF_GRA },	         // 11
	{ "a71-13.ic17",	0x8000, 0xf2c92aaa, BRF_GRA },	         // 12
	{ "a71-14.ic18",	0x8000, 0xc574b7b2, BRF_GRA },	         // 13
	{ "a71-15.ic19",	0x8000, 0x12d87e7f, BRF_GRA },	         // 14
	{ "a71-16.ic30",	0x8000, 0x0bce35b6, BRF_GRA },	         // 15
	{ "a71-17.ic31",	0x8000, 0xdeda6387, BRF_GRA },	         // 16
	{ "a71-18.ic32",	0x8000, 0x330cd9d7, BRF_GRA },	         // 17
	{ "a71-19.ic33",	0x8000, 0xfc4b29e0, BRF_GRA },	         // 18
	{ "a71-20.ic34",	0x8000, 0x65acb265, BRF_GRA },	         // 19
	{ "a71-21.ic35",	0x8000, 0x33cde9b2, BRF_GRA },	         // 20
	{ "a71-22.ic36",	0x8000, 0xfb98eac0, BRF_GRA },	         // 21
	{ "a71-23.ic37",	0x8000, 0x30bd46ad, BRF_GRA },	         // 22

	{ "a71-25.ic41",	0x0100, 0x2d0f8545, BRF_GRA },	         // 23 PROMs

	{ "a71__24.ic57",	0x0800, 0x0f4b25de, 6 | BRF_PRG},        // 24 Mcu Code

	{ "a71-26.ic19",	0x0117, 0x4e1f119c, 7 | BRF_OPT },       // 25 PLDs
};

STD_ROM_PICK(tokio)
STD_ROM_FN(tokio)

static struct BurnRomInfo tokiooRomDesc[] = {
	{ "a71-02.ic4",		0x8000, 0xd556c908, BRF_ESS | BRF_PRG }, //  0 Z80 #1 Program
	{ "a71-03.ic5",		0x8000, 0x69dacf44, BRF_ESS | BRF_PRG }, //  1
	{ "a71-04.ic6",		0x8000, 0xa0a4ce0e, BRF_ESS | BRF_PRG }, //  2
	{ "a71-05.ic7",		0x8000, 0x6da0b945, BRF_ESS | BRF_PRG }, //  3
	{ "a71-06.ic8",		0x8000, 0x447d6779, BRF_ESS | BRF_PRG }, //  4

	{ "a71-01.ic1",		0x8000, 0x0867c707, BRF_ESS | BRF_PRG }, //  5 Z80 #2 Program 

	{ "a71-07.ic10",	0x8000, 0xf298cc7b, BRF_ESS | BRF_PRG }, //  6 Z80 #3 Program 

	{ "a71-08.ic12",	0x8000, 0x0439ab13, BRF_GRA },	         //  7 Tiles
	{ "a71-09.ic13",	0x8000, 0xedb3d2ff, BRF_GRA },	         //  8
	{ "a71-10.ic14",	0x8000, 0x69f0888c, BRF_GRA },	         //  9
	{ "a71-11.ic15",	0x8000, 0x4ae07c31, BRF_GRA },	         // 10
	{ "a71-12.ic16",	0x8000, 0x3f6bd706, BRF_GRA },	         // 11
	{ "a71-13.ic17",	0x8000, 0xf2c92aaa, BRF_GRA },	         // 12
	{ "a71-14.ic18",	0x8000, 0xc574b7b2, BRF_GRA },	         // 13
	{ "a71-15.ic19",	0x8000, 0x12d87e7f, BRF_GRA },	         // 14
	{ "a71-16.ic30",	0x8000, 0x0bce35b6, BRF_GRA },	         // 15
	{ "a71-17.ic31",	0x8000, 0xdeda6387, BRF_GRA },	         // 16
	{ "a71-18.ic32",	0x8000, 0x330cd9d7, BRF_GRA },	         // 17
	{ "a71-19.ic33",	0x8000, 0xfc4b29e0, BRF_GRA },	         // 18
	{ "a71-20.ic34",	0x8000, 0x65acb265, BRF_GRA },	         // 19
	{ "a71-21.ic35",	0x8000, 0x33cde9b2, BRF_GRA },	         // 20
	{ "a71-22.ic36",	0x8000, 0xfb98eac0, BRF_GRA },	         // 21
	{ "a71-23.ic37",	0x8000, 0x30bd46ad, BRF_GRA },	         // 22

	{ "a71-25.ic41",	0x0100, 0x2d0f8545, BRF_GRA },	         // 23 PROMs

	{ "a71__24.ic57",	0x0800, 0x0f4b25de, 6 | BRF_PRG},        // 24 Mcu Code

	{ "a71-26.ic19",	0x0117, 0x4e1f119c, 7 | BRF_OPT },       // 25 PLDs
};

STD_ROM_PICK(tokioo)
STD_ROM_FN(tokioo)

static struct BurnRomInfo tokiouRomDesc[] = {
	{ "a71-27-1.ic4",	0x8000, 0x8c180896, BRF_ESS | BRF_PRG }, //  0 Z80 #1 Program Code
	{ "a71-28-1.ic5",	0x8000, 0x1b447527, BRF_ESS | BRF_PRG }, //  1
	{ "a71-04.ic6",		0x8000, 0xa0a4ce0e, BRF_ESS | BRF_PRG }, //  2
	{ "a71-05.ic7",		0x8000, 0x6da0b945, BRF_ESS | BRF_PRG }, //  3
	{ "a71-06-1.ic8",	0x8000, 0x56927b3f, BRF_ESS | BRF_PRG }, //  4

	{ "a71-01.ic1",		0x8000, 0x0867c707, BRF_ESS | BRF_PRG }, //  5 Z80 #2 Program 

	{ "a71-07.ic10",	0x8000, 0xf298cc7b, BRF_ESS | BRF_PRG }, //  6 Z80 #3 Program 

	{ "a71-08.ic12",	0x8000, 0x0439ab13, BRF_GRA },	         //  7 Tiles
	{ "a71-09.ic13",	0x8000, 0xedb3d2ff, BRF_GRA },	         //  8
	{ "a71-10.ic14",	0x8000, 0x69f0888c, BRF_GRA },	         //  9
	{ "a71-11.ic15",	0x8000, 0x4ae07c31, BRF_GRA },	         // 10
	{ "a71-12.ic16",	0x8000, 0x3f6bd706, BRF_GRA },	         // 11
	{ "a71-13.ic17",	0x8000, 0xf2c92aaa, BRF_GRA },	         // 12
	{ "a71-14.ic18",	0x8000, 0xc574b7b2, BRF_GRA },	         // 13
	{ "a71-15.ic19",	0x8000, 0x12d87e7f, BRF_GRA },	         // 14
	{ "a71-16.ic30",	0x8000, 0x0bce35b6, BRF_GRA },	         // 15
	{ "a71-17.ic31",	0x8000, 0xdeda6387, BRF_GRA },	         // 16
	{ "a71-18.ic32",	0x8000, 0x330cd9d7, BRF_GRA },	         // 17
	{ "a71-19.ic33",	0x8000, 0xfc4b29e0, BRF_GRA },	         // 18
	{ "a71-20.ic34",	0x8000, 0x65acb265, BRF_GRA },	         // 19
	{ "a71-21.ic35",	0x8000, 0x33cde9b2, BRF_GRA },	         // 20
	{ "a71-22.ic36",	0x8000, 0xfb98eac0, BRF_GRA },	         // 21
	{ "a71-23.ic37",	0x8000, 0x30bd46ad, BRF_GRA },	         // 22

	{ "a71-25.ic41",	0x0100, 0x2d0f8545, BRF_GRA },	         // 23 PROMs

	{ "a71__24.ic57",	0x0800, 0x0f4b25de, 6 | BRF_PRG},        // 24 Mcu Code

	{ "a71-26.ic19",	0x0117, 0x4e1f119c, 7 | BRF_OPT },       // 25 PLDs
};

STD_ROM_PICK(tokiou)
STD_ROM_FN(tokiou)

static struct BurnRomInfo tokiobRomDesc[] = {
	{ "2.ic4",			0x8000, 0xf583b1ef, BRF_ESS | BRF_PRG }, //  0 Z80 #1 Program Code
	{ "a71-03.ic5",		0x8000, 0x69dacf44, BRF_ESS | BRF_PRG }, //  1
	{ "a71-04.ic6",		0x8000, 0xa0a4ce0e, BRF_ESS | BRF_PRG }, //  2
	{ "a71-05.ic7",		0x8000, 0x6da0b945, BRF_ESS | BRF_PRG }, //  3
	{ "6.ic8",			0x8000, 0x1490e95b, BRF_ESS | BRF_PRG }, //  4

	{ "a71-01.ic1",		0x8000, 0x0867c707, BRF_ESS | BRF_PRG }, //  5 Z80 #2 Program 

	{ "a71-07.ic10",	0x8000, 0xf298cc7b, BRF_ESS | BRF_PRG }, //  6 Z80 #3 Program 

	{ "a71-08.ic12",	0x8000, 0x0439ab13, BRF_GRA },	         //  7 Tiles
	{ "a71-09.ic13",	0x8000, 0xedb3d2ff, BRF_GRA },	         //  8
	{ "a71-10.ic14",	0x8000, 0x69f0888c, BRF_GRA },	         //  9
	{ "a71-11.ic15",	0x8000, 0x4ae07c31, BRF_GRA },	         // 10
	{ "a71-12.ic16",	0x8000, 0x3f6bd706, BRF_GRA },	         // 11
	{ "a71-13.ic17",	0x8000, 0xf2c92aaa, BRF_GRA },	         // 12
	{ "a71-14.ic18",	0x8000, 0xc574b7b2, BRF_GRA },	         // 13
	{ "a71-15.ic19",	0x8000, 0x12d87e7f, BRF_GRA },	         // 14
	{ "a71-16.ic30",	0x8000, 0x0bce35b6, BRF_GRA },	         // 15
	{ "a71-17.ic31",	0x8000, 0xdeda6387, BRF_GRA },	         // 16
	{ "a71-18.ic32",	0x8000, 0x330cd9d7, BRF_GRA },	         // 17
	{ "a71-19.ic33",	0x8000, 0xfc4b29e0, BRF_GRA },	         // 18
	{ "a71-20.ic34",	0x8000, 0x65acb265, BRF_GRA },	         // 19
	{ "a71-21.ic35",	0x8000, 0x33cde9b2, BRF_GRA },	         // 20
	{ "a71-22.ic36",	0x8000, 0xfb98eac0, BRF_GRA },	         // 21
	{ "a71-23.ic37",	0x8000, 0x30bd46ad, BRF_GRA },	         // 22

	{ "a71-25.ic41",	0x0100, 0x2d0f8545, BRF_GRA },	         // 23 PROMs
};

STD_ROM_PICK(tokiob)
STD_ROM_FN(tokiob)


static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80Rom1             = Next; Next += 0x30000;
	DrvZ80Rom2             = Next; Next += 0x08000;
	DrvZ80Rom3             = Next; Next += 0x0a000;
	DrvProm                = Next; Next += 0x00100;

	if (DrvMCUInUse) {
		DrvMcuRom          = Next; Next += 0x01000;
	}

	RamStart               = Next;

	DrvPaletteRam          = Next; Next += 0x00200;
	DrvVideoRam            = Next; Next += 0x01d00;
	DrvZ80Ram1             = Next; Next += 0x00400;
	DrvZ80Ram3             = Next; Next += 0x01000;
	DrvSharedRam           = Next; Next += 0x01800;
	DrvMcuRam              = Next; Next += 0x000c0;
	DrvSpriteRam           = Next; Next += 0x00300;

	RamEnd                 = Next;

	DrvTiles               = Next; Next += 0x4000 * 8 * 8;
	DrvPalette             = (UINT32*)Next; Next += 0x00100 * sizeof(UINT32);

	MemEnd                 = Next;

	return 0;
}

static INT32 DrvDoReset()
{
	ZetOpen(0);
	ZetReset();
	BurnYM3526Reset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	ZetClose();

	ZetOpen(2);
	ZetReset();
	BurnYM2203Reset();
	ZetClose();

	if (DrvMCUInUse == 1) {
		M6801Open(0);
		M6801Reset();
		M6801Close();
	} else if (DrvMCUInUse == 2) {
		m67805_taito_reset();
	}

	DrvRomBank = 0;
	DrvMCUActive = 0;
	DrvVideoEnable = 0;
	DrvFlipScreen = 0;
	IC43A = 0;
	IC43B = 0;
	DrvSoundStatus = 0;
	DrvSoundStatusPending = 0;
	DrvSoundNmiEnable = 0;
	DrvSoundNmiPending = 0;
	DrvSoundLatch = 0;
	DrvSoundLatchPending = 0;
	mcu_latch = 0;
	mcu_address = 0;

	HiscoreReset();

	return 0;
}

static INT32 TokioDoReset()
{
	ZetReset(0);
	ZetReset(1);

	ZetOpen(2);
	ZetReset();
	BurnYM2203Reset();
	ZetClose();

	if (DrvMCUInUse == 2) {
		m67805_taito_reset();
	}

	DrvRomBank = 0;
	DrvVideoEnable = 1;
	DrvFlipScreen = 0;
	DrvSoundStatus = 0;
	DrvSoundNmiEnable = 0;
	DrvSoundNmiPending = 0;
	DrvSoundLatch = 0;

	HiscoreReset();

	return 0;
}

static void bank_switch()
{
	ZetMapMemory(DrvZ80Rom1 + 0x10000 + (DrvRomBank * 0x4000), 0x8000, 0xbfff, MAP_ROM);
}

static void sync_sound()
{
	INT32 cyc = ZetTotalCycles(0) / 2;
	//INT32 old = ZetTotalCycles(2);
	ZetCPUPush(2);
	BurnTimerUpdate(cyc);
	ZetCPUPop();
	//bprintf(0, _T("sync sound: %d \n"), ZetTotalCycles(2) - old);
}

static void soundlatch(UINT8 data)
{
	sync_sound();
	DrvSoundLatch = data;
	DrvSoundLatchPending = 1;
	DrvSoundNmiPending = 1;
	if (DrvSoundNmiEnable) {
		DrvSoundNmiPending = 0;
		ZetNmi(2);
	}
}

static UINT8 __fastcall main_read(UINT16 a)
{
	switch (a) {
		case 0xfa00: {
			// never gets read?
			sync_sound();
			DrvSoundStatusPending = 0;
			return DrvSoundStatus;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 #1 Read => %04X\n"), a);
		}
	}

	return 0;
}

static void __fastcall main_write(UINT16 a, UINT8 d)
{
	switch (a) {
		case 0xfa00: {
			soundlatch(d);
			return;
		}

		case 0xfa03: {
			//ZetSetRESETLine(2, (d) ? 1 : 0); // breaks sound
			return;
		}
		
		case 0xfa80: {
			// watchdog reset
			return;
		}
		case 0xfb40: {
			DrvRomBank = (d ^ 0x04) & 0x07;
			bank_switch();

			ZetSetRESETLine(1, ~d & 0x10);

			if (!(d & 0x20)) {
				if (DrvMCUInUse == 2) {
					 m67805_taito_reset();
				} else {
					M6801Open(0);
					M6801Reset();
					M6801Close();
				}
				DrvMCUActive = 0;
			} else {
				DrvMCUActive = 1;
			}

			DrvVideoEnable = d & 0x40;
			DrvFlipScreen = d & 0x80;
			return;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 #1 Write => %04X, %02X\n"), a, d);
		}
	}
}

static UINT8 __fastcall BoblboblRead1(UINT16 a)
{
	switch (a) {
		case 0xfe00: {
			return IC43A << 4;
		}
		
		case 0xfe01:
		case 0xfe02:
		case 0xfe03: {
			return BurnRandom() & 0xff;
		}
		
		case 0xfe80: {
			return IC43B << 4;
		}
		
		case 0xfe81:
		case 0xfe82:
		case 0xfe83: {
			return 0xff;
		}
		
		case 0xff00: {
			return DrvDip[0];
		}
		
		case 0xff01: {
			return DrvDip[1];
		}
		
		case 0xff02: {
			return DrvInput[0];
		}
		
		case 0xff03: {
			return DrvInput[1];
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 #1 Read => %04X\n"), a);
		}
	}

	return 0;
}

static void __fastcall BoblboblWrite1(UINT16 a, UINT8 d)
{
	switch (a) {
		case 0xfa00: {
			soundlatch(d);
			return;
		}
		
		case 0xfa03: {
			// soundcpu reset
			return;
		}
		
		case 0xfa80: {
			// nop
			return;
		}
		
		case 0xfb40: {
			DrvRomBank = (d ^ 0x04) & 0x07;
			bank_switch();

			ZetSetRESETLine(1, ~d & 0x10);

			DrvVideoEnable = d & 0x40;
			DrvFlipScreen = d & 0x80;
			return;
		}
		
		case 0xfe00:
		case 0xfe01:
		case 0xfe02:
		case 0xfe03: {
			INT32 Res = 0;

			switch (a & 0x03) {
				case 0x00: {
					if (~IC43A & 8) Res ^= 1;
					if (~IC43A & 1) Res ^= 2;
					if (~IC43A & 1) Res ^= 4;
					if (~IC43A & 2) Res ^= 4;
					if (~IC43A & 4) Res ^= 8;
					break;
				}
				
				case 0x01: {
					if (~IC43A & 8) Res ^= 1;
					if (~IC43A & 2) Res ^= 1;
					if (~IC43A & 8) Res ^= 2;
					if (~IC43A & 1) Res ^= 4;
					if (~IC43A & 4) Res ^= 8;
					break;
				}
		
				case 0x02: {
					if (~IC43A & 4) Res ^= 1;
					if (~IC43A & 8) Res ^= 2;
					if (~IC43A & 2) Res ^= 4;
					if (~IC43A & 1) Res ^= 8;
					if (~IC43A & 4) Res ^= 8;
					break;
				}
				
				case 0x03: {
					if (~IC43A & 2) Res ^= 1;
					if (~IC43A & 4) Res ^= 2;
					if (~IC43A & 8) Res ^= 2;
					if (~IC43A & 8) Res ^= 4;
					if (~IC43A & 1) Res ^= 8;
					break;
				}
			}
			
			IC43A = Res;
			return;
		}
		
		case 0xfe80:
		case 0xfe81:
		case 0xfe82:
		case 0xfe83: {
			static const INT32 XorVal[4] = { 4, 1, 8, 2 };

			IC43B = (d >> 4) ^ XorVal[a & 3];
			return;
		}
		
		case 0xff94:
		case 0xff98: {
			// nop
			return;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 #1 Write => %04X, %02X\n"), a, d);
		}
	}
}

static UINT8 __fastcall sound_read(UINT16 a)
{
	switch (a) {
		case 0x9000:
		case 0x9001:
			return BurnYM2203Read(0, a & 1);

		case 0xa000:
			return BurnYM3526Read(0);

		case 0xb000:
			DrvSoundLatchPending = 0;
			return DrvSoundLatch;

		case 0xb001:
			return 0xfc | (DrvSoundLatchPending << 1) | (DrvSoundStatusPending << 0);

		case 0xe000:
			return 0;

		default: {
			bprintf(PRINT_NORMAL, _T("Z80 #3 Read => %04X\n"), a);
		}
	}

	return 0;
}

static void __fastcall sound_write(UINT16 a, UINT8 d)
{
	switch (a) {
		case 0x9000:
		case 0x9001:
			BurnYM2203Write(0, a & 1, d);
			return;

		case 0xa000:
		case 0xa001:
			BurnYM3526Write(a & 1, d);
			return;

		case 0xb000:
			DrvSoundStatus = d;
			DrvSoundStatusPending = 1;
			return;

		case 0xb001:
			DrvSoundNmiEnable = 1;
			ZetRunEnd(); // run pending nmi in frame()!
			return;

		case 0xb002:
			DrvSoundNmiEnable = 0;
			return;

		default: {
			bprintf(PRINT_NORMAL, _T("Z80 #3 Write => %04X, %02X\n"), a, d);
		}
	}
}

static UINT8 BublboblMcuReadByte(UINT16 Address)
{
	if (Address >= 0x0040 && Address <= 0x00ff) {
		return DrvMcuRam[Address - 0x0040];
	}
	
	switch (Address) {
		case 0x00: {
			return ddr1;
		}
		
		case 0x01: {
			return ddr2;
		}
		
		case 0x02: {
			port1_in = DrvInput[0];
			return (port1_out & ddr1) | (port1_in & ~ddr1);
		}
		
		case 0x03: {
			return (port2_out & ddr2) | (port2_in & ~ddr2);
		}
		
		case 0x04: {
			return ddr3;
		}
		
		case 0x05: {
			return ddr4;
		}
		
		case 0x06: {
			return (port3_out & ddr3) | (port3_in & ~ddr3);
		}
		
		case 0x07: {
			return (port4_out & ddr4) | (port4_in & ~ddr4);
		}
	}
	
	bprintf(PRINT_NORMAL, _T("M6801 Read Byte -> %04X\n"), Address);
	
	return 0;
}

static void BublboblMcuWriteByte(UINT16 Address, UINT8 Data)
{
	if (Address >= 0x0040 && Address <= 0x00ff) {
		DrvMcuRam[Address - 0x0040] = Data;
		return;
	}
	
	if (Address > 7 && Address <= 0x001f) {
		m6803_internal_registers_w(Address, Data);
		if (Address > 7) return;
	}
	
	switch (Address) {
		case 0x00: {
			ddr1 = Data;
			return;
		}
		
		case 0x01: {
			ddr2 = Data;
			return;
		}
		
		case 0x02: {
			if ((port1_out & 0x40) && (~Data & 0x40)) {
				ZetSetVector(0, DrvZ80Ram1[0]);
				ZetSetIRQLine(0, 0, CPU_IRQSTATUS_AUTO);
			}
			
			port1_out = Data;
			return;
		}
		
		case 0x03: {
			if ((~port2_out & 0x10) && (Data & 0x10)) {
				INT32 nAddress = port4_out | ((Data & 0x0f) << 8);
				
				if (port1_out & 0x80) {
					if (nAddress == 0x0000) port3_in = DrvDip[0];
					if (nAddress == 0x0001) port3_in = DrvDip[1];
					if (nAddress == 0x0002) port3_in = DrvInput[1];
					if (nAddress == 0x0003) port3_in = DrvInput[2];
 					
					if (nAddress >= 0x0c00 && nAddress <= 0x0fff) port3_in = DrvZ80Ram1[nAddress - 0x0c00];
				} else {
					if (nAddress >= 0x0c00 && nAddress <= 0x0fff) DrvZ80Ram1[nAddress - 0x0c00] = port3_out;
				}
			}

			port2_out = Data;
			return;
		}
		
		case 0x04: {
			ddr3 = Data;
			return;
		}
		
		case 0x05: {
			ddr4 = Data;
			return;
		}
		
		case 0x06: {
			port3_out = Data;
			return;
		}
		
		case 0x07: {
			port4_out = Data;
			return;
		}
	}
	
	bprintf(PRINT_NORMAL, _T("M6801 Write Byte -> %04X, %02X\n"), Address, Data);
}

static void bublbobl_68705_portB_out(UINT8 *bytevalue)
{
	INT32 data = *bytevalue;//lazy

	if ((ddrB & 0x01) && (~data & 0x01) && (portB_out & 0x01))
	{
		portA_in = mcu_latch;
	}
	if ((ddrB & 0x02) && (data & 0x02) && (~portB_out & 0x02))
	{
		mcu_address = (mcu_address & 0xff00) | portA_out;
	}
	if ((ddrB & 0x04) && (data & 0x04) && (~portB_out & 0x04))
	{
		mcu_address = (mcu_address & 0x00ff) | ((portA_out & 0x0f) << 8);
	}
	if ((ddrB & 0x10) && (~data & 0x10) && (portB_out & 0x10))
	{
		if (data & 0x08)	/* read */
		{
			if ((mcu_address & 0x0800) == 0x0000)
			{
				switch (mcu_address & 3) {
					case 0: mcu_latch = DrvDip[0]; break;
					case 1: mcu_latch = DrvDip[1]; break;
					case 2: mcu_latch = DrvInput[1]; break;
					case 3: mcu_latch = DrvInput[2]; break;
				}
			}
			else if ((mcu_address & 0x0c00) == 0x0c00)
			{
				mcu_latch = DrvZ80Ram1[mcu_address & 0x03ff];
			}
		}
		else
		{
			if ((mcu_address & 0x0c00) == 0x0c00)
			{
				DrvZ80Ram1[mcu_address & 0x03ff] = portA_out;
			}
		}
	}
	if ((ddrB & 0x20) && (~data & 0x20) && (portB_out & 0x20))
	{
		/* hack to get random EXTEND letters (who is supposed to do this? 68705? PAL?) */
		// probably buggy bootleg? -dink
		DrvZ80Ram1[0x7c] = BurnRandom() % 6;

		ZetSetVector(0, DrvZ80Ram1[0]);
		ZetSetIRQLine(0, 0, CPU_IRQSTATUS_AUTO);
	}
}

static void bublbobl_68705_portC_in()
{
	portC_out = DrvInput[0];
	ddrC = 0xff;
}

static m68705_interface bub68705_m68705_interface = {
	NULL, bublbobl_68705_portB_out, NULL,
	NULL, NULL, NULL,
	NULL, NULL, bublbobl_68705_portC_in
};

static void tokio_68705_portA_out(UINT8 *data)
{
	from_mcu = *data;
	mcu_sent = 1;
}
	
static void tokio_68705_portC_in()
{
	portC_in = 0;

	if (!main_sent) portC_in |= 0x01;
	if (mcu_sent) portC_in |= 0x02;

	portC_in ^= 0x3; // inverted logic compared to tigerh
}

static m68705_interface tokio_m68705_interface = {
	tokio_68705_portA_out, standard_m68705_portB_out, NULL,
	NULL, NULL, NULL,
	NULL, NULL, tokio_68705_portC_in
};

static void __fastcall TokioWrite1(UINT16 a, UINT8 d)
{
	switch (a)
	{
		case 0xfa80: {
			DrvRomBank = d & 0x07;
			bank_switch();
			return;
		}

		case 0xfb00: {
			DrvFlipScreen = d & 0x80;
			return;
		}

		case 0xfb80: {
			ZetNmi(1);
			return;
		}

		case 0xfc00: {
			soundlatch(d);
			return;
		}
		
		case 0xfe00: {
			if (DrvMCUInUse == 2) {
				from_main = d;
				main_sent = 1;
				m68705SetIrqLine(0, CPU_IRQSTATUS_ACK);
			}
			return;
		}
	}
}

static UINT8 __fastcall TokioRead1(UINT16 a)
{
	switch (a)
	{
		case 0xfa03: {
			return DrvDip[0];
		}

		case 0xfa04: {
			return DrvDip[1];
		}

		case 0xfa05: {
			if (DrvMCUInUse) {
				return (DrvInput[0] & ~0x30) | ((!main_sent) ? 0x10 : 0x00) | ((!mcu_sent) ? 0x20 : 0x00);
			} else {
				return (DrvInput[0] & ~0x30);
			}
		}

		case 0xfa06: {
			return DrvInput[1];
		}

		case 0xfa07: {
			return DrvInput[2];
		}

		case 0xfc00: {
			return DrvSoundStatus;
		}

		case 0xfe00: {
			if (DrvMCUInUse == 2) {
				mcu_sent = false;
				return from_mcu;
			}
			
			return 0xbf;
		}
	}

	return 0;
}

static void __fastcall TokioSoundWrite3(UINT16 a, UINT8 d)
{
	switch (a)
	{
		case 0x9000:
			DrvSoundStatus = d;
			return;

		case 0xa000:
			DrvSoundNmiEnable = 0;
			return;

		case 0xa800:
			DrvSoundNmiEnable = 1;
			ZetRunEnd();
			return;

		case 0xb000:
		case 0xb001:
			BurnYM2203Write(0, a & 1, d);
			return;
	}
}

static UINT8 __fastcall TokioSoundRead3(UINT16 a)
{
	switch (a)
	{
		case 0x9000:
			return DrvSoundLatch;

		case 0xb000:
		case 0xb001:
			return BurnYM2203Read(0, a & 1);
	}

	return 0;
}

inline static void DrvYM2203IRQHandler(INT32, INT32 nStatus)
{
	if (ZetGetActive() == -1) return;
	ZetSetIRQLine(0, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 TilePlaneOffsets[4] = { 0, 4, 0x200000, 0x200004 };
static INT32 TileXOffsets[8]     = { 3, 2, 1, 0, 11, 10, 9, 8 };
static INT32 TileYOffsets[8]     = { 0, 16, 32, 48, 64, 80, 96, 112 };

static INT32 MachineInit()
{
	BurnAllocMemIndex();

	// Setup the Z80 emulation
	ZetInit(0);
	ZetOpen(0);
	ZetSetReadHandler(main_read);
	ZetSetWriteHandler(main_write);
	ZetMapMemory(DrvZ80Rom1,	0x0000, 0x7fff, MAP_ROM);

	DrvRomBank = 0;
	bank_switch(); // ROM @ 8000 - bfff

	ZetMapMemory(DrvVideoRam,	0xc000, 0xdcff, MAP_RAM);
	ZetMapMemory(DrvSpriteRam,	0xdd00, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvSharedRam,	0xe000, 0xf7ff, MAP_RAM);
	ZetMapMemory(DrvPaletteRam,	0xf800, 0xf9ff, MAP_RAM);
	ZetMapMemory(DrvZ80Ram1,	0xfc00, 0xffff, MAP_RAM);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80Rom2,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvSharedRam,	0xe000, 0xf7ff, MAP_RAM);
	ZetClose();

	ZetInit(2);
	ZetOpen(2);
	ZetSetReadHandler(sound_read);
	ZetSetWriteHandler(sound_write);
	ZetMapMemory(DrvZ80Rom3,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80Ram3,	0x8000, 0x8fff, MAP_RAM);
	ZetClose();

	if (DrvMCUInUse == 1) {
		M6801Init(0);
		M6801Open(0);
		M6801MapMemory(DrvMcuRom, 0xf000, 0xffff, MAP_ROM);
		M6801SetReadHandler(BublboblMcuReadByte);
		M6801SetWriteHandler(BublboblMcuWriteByte);
		M6801Close();
	} else if (DrvMCUInUse == 2) {

		m67805_taito_init(DrvMcuRom, DrvMcuRam, &bub68705_m68705_interface);
	}

	BurnYM2203Init(1, 3000000, &DrvYM2203IRQHandler, 0);
	BurnTimerAttachZet(3000000);
	BurnYM2203SetAllRoutes(0, 0.25, BURN_SND_ROUTE_BOTH);

	BurnYM3526Init(3000000, NULL, 1);
	BurnYM3526SetRoute(BURN_SND_YM3526_ROUTE, 0.50, BURN_SND_ROUTE_BOTH);

	if (BublboblCallbackFunction()) return 1;

	GenericTilesInit();

	// Reset the driver
	DrvDoReset();

	return 0;
}

static INT32 BublboblCallback()
{
	INT32 nRet = 0;
	
	DrvTempRom = (UINT8 *)BurnMalloc(0x80000);

	// Load Z80 #1 Program Roms
	nRet = BurnLoadRom(DrvZ80Rom1 + 0x00000, 0, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvZ80Rom1 + 0x10000, 1, 1); if (nRet != 0) return 1;
	
	// Load Z80 #2 Program Roms
	nRet = BurnLoadRom(DrvZ80Rom2 + 0x00000, 2, 1); if (nRet != 0) return 1;
	
	// Load Z80 #3 Program Roms
	nRet = BurnLoadRom(DrvZ80Rom3 + 0x00000, 3, 1); if (nRet != 0) return 1;
	
	// Load MCU Rom
	nRet = BurnLoadRom(DrvMcuRom  + 0x00000, 4, 1); if (nRet != 0) return 1;
	
	// Load and decode the tiles
	nRet = BurnLoadRom(DrvTempRom + 0x00000,  5, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x08000,  6, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x10000,  7, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x18000,  8, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x20000,  9, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x28000, 10, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x40000, 11, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x48000, 12, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x50000, 13, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x58000, 14, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x60000, 15, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x68000, 16, 1); if (nRet != 0) return 1;
	for (INT32 i = 0; i < 0x80000; i++) DrvTempRom[i] ^= 0xff;
	GfxDecode(0x4000, 4, 8, 8, TilePlaneOffsets, TileXOffsets, TileYOffsets, 0x80, DrvTempRom, DrvTiles);
	
	// Load the PROM
	nRet = BurnLoadRom(DrvProm + 0x00000,  17, 1); if (nRet != 0) return 1;
	
	BurnFree(DrvTempRom);
	
	return 0;
}

static INT32 BublboblInit()
{
	BublboblCallbackFunction = BublboblCallback;
	
	DrvMCUInUse = 1;
	
	return MachineInit();
}

static INT32 BoblboblCallback()
{
	INT32 nRet = 0;
	
	DrvTempRom = (UINT8 *)BurnMalloc(0x80000);

	// Load Z80 #1 Program Roms
	nRet = BurnLoadRom(DrvZ80Rom1 + 0x00000, 0, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvZ80Rom1 + 0x10000, 1, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvZ80Rom1 + 0x18000, 2, 1); if (nRet != 0) return 1;
	
	// Load Z80 #2 Program Roms
	nRet = BurnLoadRom(DrvZ80Rom2 + 0x00000, 3, 1); if (nRet != 0) return 1;
	
	// Load Z80 #3 Program Roms
	nRet = BurnLoadRom(DrvZ80Rom3 + 0x00000, 4, 1); if (nRet != 0) return 1;
	
	// Load and decode the tiles
	nRet = BurnLoadRom(DrvTempRom + 0x00000,  5, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x08000,  6, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x10000,  7, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x18000,  8, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x20000,  9, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x28000, 10, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x40000, 11, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x48000, 12, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x50000, 13, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x58000, 14, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x60000, 15, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x68000, 16, 1); if (nRet != 0) return 1;
	for (INT32 i = 0; i < 0x80000; i++) DrvTempRom[i] ^= 0xff;
	GfxDecode(0x4000, 4, 8, 8, TilePlaneOffsets, TileXOffsets, TileYOffsets, 0x80, DrvTempRom, DrvTiles);
	
	// Load the PROM
	nRet = BurnLoadRom(DrvProm + 0x00000,  17, 1); if (nRet != 0) return 1;
	
	BurnFree(DrvTempRom);
	
	ZetOpen(0);
	ZetSetReadHandler(BoblboblRead1);
	ZetSetWriteHandler(BoblboblWrite1);
	ZetMemCallback(0xfe00, 0xffff, 0);
	ZetMemCallback(0xfe00, 0xffff, 1);
	ZetMemCallback(0xfe00, 0xffff, 2);
	ZetClose();
	
	return 0;
}

static INT32 SboblboblCallback()
{
	INT32 nRet = 0;

	bublbobl2 = 1;
	
	DrvTempRom = (UINT8 *)BurnMalloc(0x80000);

	// Load Z80 #1 Program Roms
	nRet = BurnLoadRom(DrvZ80Rom1 + 0x00000, 0, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvZ80Rom1 + 0x10000, 1, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvZ80Rom1 + 0x18000, 2, 1); if (nRet != 0) return 1;
	
	// Load Z80 #2 Program Roms
	nRet = BurnLoadRom(DrvZ80Rom2 + 0x00000, 3, 1); if (nRet != 0) return 1;
	
	// Load Z80 #3 Program Roms
	nRet = BurnLoadRom(DrvZ80Rom3 + 0x00000, 4, 1); if (nRet != 0) return 1;
	
	// Load and decode the tiles
	nRet = BurnLoadRom(DrvTempRom + 0x00000,  5, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x10000,  6, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x20000,  7, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x28000,  8, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x40000,  9, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x50000, 10, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x60000, 11, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x68000, 12, 1); if (nRet != 0) return 1;
	for (INT32 i = 0; i < 0x80000; i++) DrvTempRom[i] ^= 0xff;
	GfxDecode(0x4000, 4, 8, 8, TilePlaneOffsets, TileXOffsets, TileYOffsets, 0x80, DrvTempRom, DrvTiles);
	
	// Load the PROM
	nRet = BurnLoadRom(DrvProm + 0x00000,  13, 1); if (nRet != 0) return 1;
	
	BurnFree(DrvTempRom);
	
	ZetOpen(0);
	ZetSetReadHandler(BoblboblRead1);
	ZetSetWriteHandler(BoblboblWrite1);
	ZetMemCallback(0xfe00, 0xffff, 0);
	ZetMemCallback(0xfe00, 0xffff, 1);
	ZetMemCallback(0xfe00, 0xffff, 2);
	ZetClose();
	
	return 0;
}

static INT32 SboblboblfCallback()
{
	INT32 nRet = 0;

	bublbobl2 = 1;
	
	DrvTempRom = (UINT8 *)BurnMalloc(0x80000);

	// Load Z80 #1 Program Roms
	nRet = BurnLoadRom(DrvZ80Rom1 + 0x00000, 0, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvZ80Rom1 + 0x10000, 1, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvZ80Rom1 + 0x18000, 2, 1); if (nRet != 0) return 1;
	
	// Load Z80 #2 Program Roms
	nRet = BurnLoadRom(DrvZ80Rom2 + 0x00000, 3, 1); if (nRet != 0) return 1;
	
	// Load Z80 #3 Program Roms
	nRet = BurnLoadRom(DrvZ80Rom3 + 0x00000, 4, 1); if (nRet != 0) return 1;
	
	// Load and decode the tiles
	nRet = BurnLoadRom(DrvTempRom + 0x00000,  5, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x10000,  6, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x28000,  7, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x40000,  8, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x50000,  9, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x68000, 10, 1); if (nRet != 0) return 1;
	for (INT32 i = 0; i < 0x80000; i++) DrvTempRom[i] ^= 0xff;
	GfxDecode(0x4000, 4, 8, 8, TilePlaneOffsets, TileXOffsets, TileYOffsets, 0x80, DrvTempRom, DrvTiles);
	
	// Load the PROM
	nRet = BurnLoadRom(DrvProm + 0x00000,  11, 1); if (nRet != 0) return 1;
	
	BurnFree(DrvTempRom);
	
	ZetOpen(0);
	ZetSetReadHandler(BoblboblRead1);
	ZetSetWriteHandler(BoblboblWrite1);
	ZetMemCallback(0xfe00, 0xffff, 0);
	ZetMemCallback(0xfe00, 0xffff, 1);
	ZetMemCallback(0xfe00, 0xffff, 2);
	ZetClose();
	
	return 0;
}

static INT32 BoblboblInit()
{
	BublboblCallbackFunction = BoblboblCallback;
	
	return MachineInit();
}

static INT32 SboblboblInit()
{
	BublboblCallbackFunction = SboblboblCallback;
	
	return MachineInit();
}

static INT32 SboblboblfInit()
{
	BublboblCallbackFunction = SboblboblfCallback;
	
	return MachineInit();
}

static INT32 Bub68705Callback()
{
	INT32 nRet = 0;
	
	DrvTempRom = (UINT8 *)BurnMalloc(0x80000);

	// Load Z80 #1 Program Roms
	nRet = BurnLoadRom(DrvZ80Rom1 + 0x00000, 0, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvZ80Rom1 + 0x10000, 1, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvZ80Rom1 + 0x18000, 2, 1); if (nRet != 0) return 1;
	
	// Load Z80 #2 Program Roms
	nRet = BurnLoadRom(DrvZ80Rom2 + 0x00000, 3, 1); if (nRet != 0) return 1;
	
	// Load Z80 #3 Program Roms
	nRet = BurnLoadRom(DrvZ80Rom3 + 0x00000, 4, 1); if (nRet != 0) return 1;
	
	// Load and decode the tiles
	nRet = BurnLoadRom(DrvTempRom + 0x00000,  5, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x08000,  6, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x10000,  7, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x18000,  8, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x20000,  9, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x28000, 10, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x40000, 11, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x48000, 12, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x50000, 13, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x58000, 14, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x60000, 15, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x68000, 16, 1); if (nRet != 0) return 1;

	for (INT32 i = 0x00000; i < 0x80000; i++)
		DrvTempRom[i] ^= 0xff;

	GfxDecode(0x4000, 4, 8, 8, TilePlaneOffsets, TileXOffsets, TileYOffsets, 0x80, DrvTempRom, DrvTiles);
	
	// Load the PROM
	nRet = BurnLoadRom(DrvProm + 0x00000,  17, 1); if (nRet != 0) return 1;
	
	// Load the 68705 Rom
	nRet = BurnLoadRom(DrvMcuRom + 0x00000,  18, 1); if (nRet != 0) return 1;

	BurnFree(DrvTempRom);
	
	return 0;
}

static INT32 Bub68705Init()
{
	BublboblCallbackFunction = Bub68705Callback;
	
	DrvMCUInUse = 2;
	
	return MachineInit();
}

static INT32 DlandCallback()
{
	INT32 nRet = 0;
	
	DrvTempRom = (UINT8 *)BurnMalloc(0x80000);

	// Load Z80 #1 Program Roms
	nRet = BurnLoadRom(DrvZ80Rom1 + 0x00000, 0, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvZ80Rom1 + 0x10000, 1, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvZ80Rom1 + 0x18000, 2, 1); if (nRet != 0) return 1;
	
	// Load Z80 #2 Program Roms
	nRet = BurnLoadRom(DrvZ80Rom2 + 0x00000, 3, 1); if (nRet != 0) return 1;
	
	// Load Z80 #3 Program Roms
	nRet = BurnLoadRom(DrvZ80Rom3 + 0x00000, 4, 1); if (nRet != 0) return 1;
	
	// Load and decode the tiles
	nRet = BurnLoadRom(DrvTempRom + 0x00000,  5, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x10000,  6, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x20000,  7, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x40000,  8, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x50000,  9, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x60000, 10, 1); if (nRet != 0) return 1;

	for (INT32 i = 0x00000; i < 0x40000; i++)
		DrvTempRom[i] = BITSWAP08(DrvTempRom[i],7,6,5,4,0,1,2,3) ^ 0xff;

	for (INT32 i = 0x40000; i < 0x80000; i++)
		DrvTempRom[i] = BITSWAP08(DrvTempRom[i],7,4,5,6,3,0,1,2) ^ 0xff;

	GfxDecode(0x4000, 4, 8, 8, TilePlaneOffsets, TileXOffsets, TileYOffsets, 0x80, DrvTempRom, DrvTiles);
	
	// Load the PROM
	nRet = BurnLoadRom(DrvProm + 0x00000,  11, 1); if (nRet != 0) return 1;

	BurnFree(DrvTempRom);

	ZetOpen(0);
	ZetSetReadHandler(BoblboblRead1);
	ZetSetWriteHandler(BoblboblWrite1);
	ZetMemCallback(0xfe00, 0xffff, 0);
	ZetMemCallback(0xfe00, 0xffff, 1);
	ZetMemCallback(0xfe00, 0xffff, 2);
	ZetClose();

	return 0;
}

static INT32 DlandInit()
{
	BublboblCallbackFunction = DlandCallback;
	
	return MachineInit();
}

static INT32 BublboblpInit()
{
	INT32 nRet;
	
	DrvMCUInUse = 0;
	
	BurnAllocMemIndex();

	DrvTempRom = (UINT8 *)BurnMalloc(0x80000);

	// Load Z80 #1 Program Roms
	nRet = BurnLoadRom(DrvZ80Rom1 + 0x00000, 0, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvZ80Rom1 + 0x10000, 1, 1); if (nRet != 0) return 1;
	
	// Load Z80 #2 Program Roms
	nRet = BurnLoadRom(DrvZ80Rom2 + 0x00000, 2, 1); if (nRet != 0) return 1;
	
	// Load Z80 #3 Program Roms
	nRet = BurnLoadRom(DrvZ80Rom3 + 0x00000, 3, 1); if (nRet != 0) return 1;
	
	// Load and decode the tiles
	nRet = BurnLoadRom(DrvTempRom + 0x00000,  4, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x08000,  5, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x10000,  6, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x18000,  7, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x20000,  8, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x28000,  9, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x40000, 10, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x48000, 11, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x50000, 12, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x58000, 13, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x60000, 14, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x68000, 15, 1); if (nRet != 0) return 1;
	for (INT32 i = 0; i < 0x80000; i++) DrvTempRom[i] ^= 0xff;
	GfxDecode(0x4000, 4, 8, 8, TilePlaneOffsets, TileXOffsets, TileYOffsets, 0x80, DrvTempRom, DrvTiles);
	
	// Load the PROM
	nRet = BurnLoadRom(DrvProm + 0x00000,  16, 1); if (nRet != 0) return 1;
	
	BurnFree(DrvTempRom);
	
	// Setup the Z80 emulation
	ZetInit(0);
	ZetOpen(0);
	ZetSetReadHandler(TokioRead1);
	ZetSetWriteHandler(TokioWrite1);
	ZetMapArea(0x0000, 0x7fff, 0, DrvZ80Rom1             );
	ZetMapArea(0x0000, 0x7fff, 2, DrvZ80Rom1             );
	ZetMapArea(0x8000, 0xbfff, 0, DrvZ80Rom1 + 0x10000   );
	ZetMapArea(0x8000, 0xbfff, 2, DrvZ80Rom1 + 0x10000   );
	ZetMapArea(0xc000, 0xdcff, 0, DrvVideoRam            );
	ZetMapArea(0xc000, 0xdcff, 1, DrvVideoRam            );
	ZetMapArea(0xc000, 0xdcff, 2, DrvVideoRam            );
	ZetMapArea(0xdd00, 0xdfff, 0, DrvSpriteRam           );
	ZetMapArea(0xdd00, 0xdfff, 1, DrvSpriteRam           );
	ZetMapArea(0xdd00, 0xdfff, 2, DrvSpriteRam           );
	ZetMapArea(0xe000, 0xf7ff, 0, DrvSharedRam           );
	ZetMapArea(0xe000, 0xf7ff, 1, DrvSharedRam           );
	ZetMapArea(0xe000, 0xf7ff, 2, DrvSharedRam           );
	ZetMapArea(0xf800, 0xf9ff, 0, DrvPaletteRam          );
	ZetMapArea(0xf800, 0xf9ff, 1, DrvPaletteRam          );
	ZetMapArea(0xf800, 0xf9ff, 2, DrvPaletteRam          );
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapArea(0x0000, 0x7fff, 0, DrvZ80Rom2             );
	ZetMapArea(0x0000, 0x7fff, 2, DrvZ80Rom2             );
	ZetMapArea(0x8000, 0x97ff, 0, DrvSharedRam           );
	ZetMapArea(0x8000, 0x97ff, 1, DrvSharedRam           );
	ZetMapArea(0x8000, 0x97ff, 2, DrvSharedRam           );
	ZetClose();

	ZetInit(2);
	ZetOpen(2);
	ZetSetReadHandler(TokioSoundRead3);
	ZetSetWriteHandler(TokioSoundWrite3);
	ZetMapArea(0x0000, 0x7fff, 0, DrvZ80Rom3             );
	ZetMapArea(0x0000, 0x7fff, 2, DrvZ80Rom3             );
	ZetMapArea(0x8000, 0x8fff, 0, DrvZ80Ram3             );
	ZetMapArea(0x8000, 0x8fff, 1, DrvZ80Ram3             );
	ZetMapArea(0x8000, 0x8fff, 2, DrvZ80Ram3             );	
	ZetClose();
	
	BurnYM2203Init(1, 3000000, &DrvYM2203IRQHandler, 0);
	BurnTimerAttachZet(3000000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE, 1.00, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.08, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.08, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.08, BURN_SND_ROUTE_BOTH);

	tokiosnd = 1;

	GenericTilesInit();

	DrvVideoEnable = 1;

	// Reset the driver
	TokioDoReset();

	return 0;
}

static INT32 TokioInit()
{
	INT32 nRet;
	
	if (tokiob) {
		DrvMCUInUse = 0;
	} else {
		DrvMCUInUse = 2;
	}

	BurnAllocMemIndex();

	{
		DrvTempRom = (UINT8 *)BurnMalloc(0x80000);

		// Load Z80 #1 Program Roms
		nRet = BurnLoadRom(DrvZ80Rom1 + 0x00000, 0, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(DrvZ80Rom1 + 0x10000, 1, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(DrvZ80Rom1 + 0x18000, 2, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(DrvZ80Rom1 + 0x20000, 3, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(DrvZ80Rom1 + 0x28000, 4, 1); if (nRet != 0) return 1;

		// Load Z80 #2 Program Roms
		nRet = BurnLoadRom(DrvZ80Rom2 + 0x00000, 5, 1); if (nRet != 0) return 1;
	
		// Load Z80 #3 Program Roms
		nRet = BurnLoadRom(DrvZ80Rom3 + 0x00000, 6, 1); if (nRet != 0) return 1;
	
		// Load and decode the tiles
		nRet = BurnLoadRom(DrvTempRom + 0x00000,  7, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(DrvTempRom + 0x08000,  8, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(DrvTempRom + 0x10000,  9, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(DrvTempRom + 0x18000, 10, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(DrvTempRom + 0x20000, 11, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(DrvTempRom + 0x28000, 12, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(DrvTempRom + 0x30000, 13, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(DrvTempRom + 0x38000, 14, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(DrvTempRom + 0x40000, 15, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(DrvTempRom + 0x48000, 16, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(DrvTempRom + 0x50000, 17, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(DrvTempRom + 0x58000, 18, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(DrvTempRom + 0x60000, 19, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(DrvTempRom + 0x68000, 20, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(DrvTempRom + 0x70000, 21, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(DrvTempRom + 0x78000, 22, 1); if (nRet != 0) return 1;
		for (INT32 i = 0; i < 0x80000; i++) DrvTempRom[i] ^= 0xff;
		GfxDecode(0x4000, 4, 8, 8, TilePlaneOffsets, TileXOffsets, TileYOffsets, 0x80, DrvTempRom, DrvTiles);
	
		// Load the PROM
		nRet = BurnLoadRom(DrvProm + 0x00000,  23, 1); if (nRet != 0) return 1;

		// Load MCU Rom
		if (DrvMCUInUse) BurnLoadRom(DrvMcuRom  + 0x00000, 24, 1);

		BurnFree(DrvTempRom);
	}

	// Setup the Z80 emulation
	ZetInit(0);
	ZetOpen(0);
	ZetSetReadHandler(TokioRead1);
	ZetSetWriteHandler(TokioWrite1);
	ZetMapArea(0x0000, 0x7fff, 0, DrvZ80Rom1             );
	ZetMapArea(0x0000, 0x7fff, 2, DrvZ80Rom1             );
	ZetMapArea(0x8000, 0xbfff, 0, DrvZ80Rom1 + 0x10000   );
	ZetMapArea(0x8000, 0xbfff, 2, DrvZ80Rom1 + 0x10000   );
	ZetMapArea(0xc000, 0xdcff, 0, DrvVideoRam            );
	ZetMapArea(0xc000, 0xdcff, 1, DrvVideoRam            );
	ZetMapArea(0xc000, 0xdcff, 2, DrvVideoRam            );
	ZetMapArea(0xdd00, 0xdfff, 0, DrvSpriteRam           );
	ZetMapArea(0xdd00, 0xdfff, 1, DrvSpriteRam           );
	ZetMapArea(0xdd00, 0xdfff, 2, DrvSpriteRam           );
	ZetMapArea(0xe000, 0xf7ff, 0, DrvSharedRam           );
	ZetMapArea(0xe000, 0xf7ff, 1, DrvSharedRam           );
	ZetMapArea(0xe000, 0xf7ff, 2, DrvSharedRam           );
	ZetMapArea(0xf800, 0xf9ff, 0, DrvPaletteRam          );
	ZetMapArea(0xf800, 0xf9ff, 1, DrvPaletteRam          );
	ZetMapArea(0xf800, 0xf9ff, 2, DrvPaletteRam          );
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapArea(0x0000, 0x7fff, 0, DrvZ80Rom2             );
	ZetMapArea(0x0000, 0x7fff, 2, DrvZ80Rom2             );
	ZetMapArea(0x8000, 0x97ff, 0, DrvSharedRam           );
	ZetMapArea(0x8000, 0x97ff, 1, DrvSharedRam           );
	ZetMapArea(0x8000, 0x97ff, 2, DrvSharedRam           );
	ZetClose();

	ZetInit(2);
	ZetOpen(2);
	ZetSetReadHandler(TokioSoundRead3);
	ZetSetWriteHandler(TokioSoundWrite3);
	ZetMapArea(0x0000, 0x7fff, 0, DrvZ80Rom3             );
	ZetMapArea(0x0000, 0x7fff, 2, DrvZ80Rom3             );
	ZetMapArea(0x8000, 0x8fff, 0, DrvZ80Ram3             );
	ZetMapArea(0x8000, 0x8fff, 1, DrvZ80Ram3             );
	ZetMapArea(0x8000, 0x8fff, 2, DrvZ80Ram3             );	
	ZetClose();
	
	if (DrvMCUInUse == 2) m67805_taito_init(DrvMcuRom, DrvMcuRam, &tokio_m68705_interface);
	
	BurnYM2203Init(1, 3000000, &DrvYM2203IRQHandler, 0);
	BurnTimerAttachZet(3000000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE, 1.00, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.08, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.08, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.08, BURN_SND_ROUTE_BOTH);

	tokiosnd = 1;

	GenericTilesInit();

	DrvVideoEnable = 1;

	// Reset the driver
	TokioDoReset();

	return 0;
}

static INT32 TokiobInit()
{
	tokiob = 1;

	return TokioInit();
}

static INT32 DrvExit()
{
	ZetExit();
	BurnYM2203Exit();

	if (DrvMCUInUse == 1) M6801Exit();
	if (DrvMCUInUse == 2) m6805Exit();
	
	GenericTilesExit();
	
	BurnFreeMemIndex();
	
	DrvRomBank = 0;
	DrvMCUActive = 0;
	DrvVideoEnable = 0;
	DrvFlipScreen = 0;
	IC43A = 0;
	IC43B = 0;
	DrvSoundStatus = 0;
	DrvSoundNmiEnable = 0;
	DrvSoundNmiPending = 0;
	DrvSoundLatch = 0;	
	DrvMCUInUse = 0;

	bublbobl2 = 0;
	tokiob = 0;
	tokiosnd = 0;

	mcu_latch = 0;
	mcu_address = 0;

	BublboblCallbackFunction = NULL;

	return 0;
}

static INT32 BublboblExit()
{
	BurnYM3526Exit();
	return DrvExit();
}

static inline UINT8 pal4bit(UINT8 bits)
{
	bits &= 0x0f;
	return (bits << 4) | bits;
}

inline static INT32 CalcCol(UINT16 nColour)
{
	INT32 r, g, b;

	r = pal4bit(nColour >> 12);
	g = pal4bit(nColour >>  8);
	b = pal4bit(nColour >>  4);

	return BurnHighCol(r, g, b, 0);
}

static void DrvCalcPalette()
{
	for (INT32 i = 0; i < 0x200; i += 2) {
		DrvPalette[i / 2] = CalcCol(DrvPaletteRam[i | 1] | (DrvPaletteRam[i & ~1] << 8));
	}
}

static void DrvVideoUpdate()
{
	INT32 sx, sy, xc, yc;
	INT32 GfxNum, GfxAttr, GfxOffset;
	const UINT8 *Prom;
	const UINT8 *PromLine;
	
	if (!DrvVideoEnable) return;
	
	sx = 0;

	Prom = DrvProm;
	
	for (INT32 Offset = 0; Offset < 0x300; Offset += 4) {
		if (*(UINT32 *)(&DrvSpriteRam[Offset]) == 0) continue;

		GfxNum = DrvSpriteRam[Offset + 1];
		GfxAttr = DrvSpriteRam[Offset + 3];
		PromLine = Prom + 0x80 + ((GfxNum & 0xe0) >> 1);

		GfxOffset = ((GfxNum & 0x1f) * 0x80);
		if ((GfxNum & 0xa0) == 0xa0) GfxOffset |= 0x1000;

		sy = -DrvSpriteRam[Offset + 0];

		for (yc = 0; yc < 32; yc++) {
			if (PromLine[yc / 2] & 0x08) continue;

			if (!(PromLine[yc / 2] & 0x04))	{
				sx = DrvSpriteRam[Offset + 2];
				if (GfxAttr & 0x40) sx -= 256;
			}

			for (xc = 0; xc < 2; xc++) {
				INT32 gOffs, Code, Colour, xFlip, yFlip, x, y, yPos;

				gOffs = GfxOffset + xc * 0x40 + (yc & 7) * 0x02 + (PromLine[yc / 2] & 0x03) * 0x10;
				Code = DrvVideoRam[gOffs] + 256 * (DrvVideoRam[gOffs + 1] & 0x03) + 1024 * (GfxAttr & 0x0f);
				Colour = (DrvVideoRam[gOffs + 1] & 0x3c) >> 2;
				xFlip = DrvVideoRam[gOffs + 1] & 0x40;
				yFlip = DrvVideoRam[gOffs + 1] & 0x80;
				x = sx + xc * 8;
				y = (sy + yc * 8) & 0xff;

				if (DrvFlipScreen) {
					x = 248 - x;
					y = 248 - y;
					xFlip = !xFlip;
					yFlip = !yFlip;
				}

				yPos = y - 16;

				Draw8x8MaskTile(pTransDraw, Code, x, yPos, xFlip, yFlip, Colour, 4, 15, 0, DrvTiles);
			}
		}

		sx += 16;
	}
}

static INT32 DrvDraw()
{
	DrvCalcPalette();

	BurnTransferClear(0xff);

	DrvVideoUpdate();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) DrvDoReset();

	ZetNewFrame();

	BublboblMakeInputs();

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[4] = { INT32(6000000 / 59.185608), INT32(6000000 / 59.185608), INT32(3000000 / 59.185608), (DrvMCUInUse == 2) ? (INT32(4000000 / 59.185608)) : (INT32(1000000 / 59.185608)) };
	INT32 nCyclesDone[4] = { 0, 0, 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++) {
		INT32 j = (i+224) % nInterleave;
		ZetOpen(0);
		CPU_RUN(0, Zet);
		if (j == 224 && !DrvMCUInUse) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(1);
		CPU_RUN(1, Zet);
		if (j == 224) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(2);
		CPU_RUN_TIMER(2);
		if (DrvSoundNmiPending && DrvSoundNmiEnable) {
			ZetNmi();
			DrvSoundNmiPending = 0;
		}
		ZetClose();

		if (DrvMCUInUse && DrvMCUActive) {
			if (DrvMCUInUse == 2) {
				CPU_RUN(3, m6805);
				if (j == 125) m68705SetIrqLine(0, 1); // fidgety bootleg mcu..
				if (j == 224) m68705SetIrqLine(0, 0);
			} else {
				M6801Open(0);
				CPU_RUN(3, M6801);
				if (j == 224) M6801SetIRQLine(0, CPU_IRQSTATUS_HOLD);
				M6801Close();
			}
		}

		if (i == 224 && pBurnDraw) DrvDraw();
	}

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
		BurnYM3526Update(pBurnSoundOut, nBurnSoundLen);
	}

	return 0;
}

static INT32 TokioFrame()
{
	if (DrvReset) TokioDoReset();

	ZetNewFrame();

	BublboblMakeInputs();

	INT32 nInterleave = 264*8;
	INT32 nCyclesTotal[4] = { 6000000 / 60, 6000000 / 60, 3000000 / 60, 3000000 / 60 };
	INT32 nCyclesDone[4] = { 0, 0, 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++) {
		ZetOpen(0);
		CPU_RUN(0, Zet);
		if (i == 240*8) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(1);
		CPU_RUN(1, Zet);
		if (i == 240*8) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(2);
		CPU_RUN_TIMER(2);
		if (DrvSoundNmiPending && DrvSoundNmiEnable) {
			ZetNmi();
			DrvSoundNmiPending = 0;
		}
		ZetClose();

		if (DrvMCUInUse) {
			CPU_RUN(3, m6805);
		}

		if (i == 240*8 && pBurnDraw) DrvDraw();
	}

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {			// Return minimum compatible version
		*pnMin = 0x029706;
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
		if (DrvMCUInUse == 1) M6801Scan(nAction);
		if (DrvMCUInUse == 2) m68705_taito_scan(nAction);

		BurnYM2203Scan(nAction, pnMin);

		if (tokiosnd == 0) {
			BurnYM3526Scan(nAction, pnMin);
		}

		SCAN_VAR(DrvRomBank);
		SCAN_VAR(DrvMCUActive);
		SCAN_VAR(DrvVideoEnable);
		SCAN_VAR(DrvFlipScreen);
		SCAN_VAR(IC43A);
		SCAN_VAR(IC43B);
		SCAN_VAR(DrvSoundStatus);
		SCAN_VAR(DrvSoundNmiEnable);
		SCAN_VAR(DrvSoundNmiPending);
		SCAN_VAR(DrvSoundLatch);
		SCAN_VAR(ddr1);
		SCAN_VAR(ddr2);
		SCAN_VAR(ddr3);
		SCAN_VAR(ddr4);
		SCAN_VAR(port1_in);
		SCAN_VAR(port2_in);
		SCAN_VAR(port3_in);
		SCAN_VAR(port4_in);
		SCAN_VAR(port1_out);
		SCAN_VAR(port2_out);
		SCAN_VAR(port3_out);
		SCAN_VAR(port4_out);
		SCAN_VAR(mcu_latch);
		SCAN_VAR(mcu_address);

		BurnRandomScan(nAction);
	}
	
	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		bank_switch();
		ZetClose();
	}

	return 0;
}

struct BurnDriver BurnDrvBublbobl = {
	"bublbobl", NULL, NULL, NULL, "1986",
	"Bubble Bobble (Japan, Ver 0.1)\0", NULL, "Taito Corporation", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, BublboblRomInfo, BublboblRomName, NULL, NULL, NULL, NULL, BublboblInputInfo, BublboblDIPInfo,
	BublboblInit, BublboblExit, DrvFrame, DrvDraw, DrvScan,
	NULL, 0x100, 256, 224, 4, 3
};

struct BurnDriver BurnDrvBublboblu = {
	"bublboblu", "bublbobl", NULL, NULL, "2011",
	"Bubble Bobble (Ultra Version, Hack)\0", NULL, "Hack (Penta Penguin)", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, BublbobluRomInfo, BublbobluRomName, NULL, NULL, NULL, NULL, BublboblInputInfo, BublboblDIPInfo,
	BublboblInit, BublboblExit, DrvFrame, DrvDraw, DrvScan,
	NULL, 0x100, 256, 224, 4, 3
};

struct BurnDriver BurnDrvBublbob1 = {
	"bublbobl1", "bublbobl", NULL, NULL, "1986",
	"Bubble Bobble (Japan, Ver 0.0)\0", NULL, "Taito Corporation", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, Bublbob1RomInfo, Bublbob1RomName, NULL, NULL, NULL, NULL, BublboblInputInfo, BublboblDIPInfo,
	BublboblInit, BublboblExit, DrvFrame, DrvDraw, DrvScan,
	NULL, 0x100, 256, 224, 4, 3
};

struct BurnDriver BurnDrvBublbobr = {
	"bublboblr", "bublbobl", NULL, NULL, "1986",
	"Bubble Bobble (US, Ver 5.1)\0", NULL, "Taito America Corporation (Romstar license)", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, BublbobrRomInfo, BublbobrRomName, NULL, NULL, NULL, NULL, BublboblInputInfo, BublboblDIPInfo,
	BublboblInit, BublboblExit, DrvFrame, DrvDraw, DrvScan,
	NULL, 0x100, 256, 224, 4, 3
};

struct BurnDriver BurnDrvBubbobr1 = {
	"bublboblr1", "bublbobl", NULL, NULL, "1986",
	"Bubble Bobble (US, Ver 1.0)\0", NULL, "Taito America Corporation (Romstar license)", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, Bubbobr1RomInfo, Bubbobr1RomName, NULL, NULL, NULL, NULL, BublboblInputInfo, BublboblDIPInfo,
	BublboblInit, BublboblExit, DrvFrame, DrvDraw, DrvScan,
	NULL, 0x100, 256, 224, 4, 3
};

struct BurnDriver BurnDrvBoblbobl = {
	"boblbobl", "bublbobl", NULL, NULL, "1986",
	"Bobble Bobble (bootleg of Bubble Bobble)\0", NULL, "bootleg", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, BoblboblRomInfo, BoblboblRomName, NULL, NULL, NULL, NULL, BoblboblInputInfo, BoblboblDIPInfo,
	BoblboblInit, BublboblExit, DrvFrame, DrvDraw, DrvScan,
	NULL, 0x100, 256, 224, 4, 3
};

struct BurnDriver BurnDrvBbredux = {
	"bbredux", "bublbobl", NULL, NULL, "2013",
	"Bubble Bobble ('bootleg redux' hack for Bobble Bobble PCB)\0", NULL, "bootleg (Punji)", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, BbreduxRomInfo, BbreduxRomName, NULL, NULL, NULL, NULL, BoblboblInputInfo, BoblboblDIPInfo,
	BoblboblInit, BublboblExit, DrvFrame, DrvDraw, DrvScan,
	NULL, 0x100, 256, 224, 4, 3
};

struct BurnDriver BurnDrvBublboblb = {
	"bublboblb", "bublbobl", NULL, NULL, "2013",
	"Bubble Bobble (for Bobble Bobble PCB)\0", NULL, "bootleg (Aladar)", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, BublboblbRomInfo, BublboblbRomName, NULL, NULL, NULL, NULL, BoblboblInputInfo, BoblcaveDIPInfo,
	BoblboblInit, BublboblExit, DrvFrame, DrvDraw, DrvScan,
	NULL, 0x100, 256, 224, 4, 3
};

struct BurnDriver BurnDrvSboblbobl = {
	"sboblbobl", "bublbobl", NULL, NULL, "1986",
	"Super Bobble Bobble (bootleg, set 1)\0", NULL, "bootleg (Datsu)", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, SboblboblRomInfo, SboblboblRomName, NULL, NULL, NULL, NULL, BoblboblInputInfo, BoblboblDIPInfo,
	SboblboblInit, BublboblExit, DrvFrame, DrvDraw, DrvScan,
	NULL, 0x100, 256, 224, 4, 3
};

struct BurnDriver BurnDrvSboblbobla = {
	"sboblbobla", "bublbobl", NULL, NULL, "1986",
	"Super Bobble Bobble (bootleg, set 2)\0", NULL, "bootleg", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, SboblboblaRomInfo, SboblboblaRomName, NULL, NULL, NULL, NULL, BoblboblInputInfo, BoblboblDIPInfo,
	BoblboblInit, BublboblExit, DrvFrame, DrvDraw, DrvScan,
	NULL, 0x100, 256, 224, 4, 3
};

struct BurnDriver BurnDrvSboblboblb = {
	"sboblboblb", "bublbobl", NULL, NULL, "1986",
	"Super Bobble Bobble (bootleg, set 3)\0", NULL, "bootleg", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, SboblboblbRomInfo, SboblboblbRomName, NULL, NULL, NULL, NULL, BoblboblInputInfo, SboblbobDIPInfo,
	BoblboblInit, BublboblExit, DrvFrame, DrvDraw, DrvScan,
	NULL, 0x100, 256, 224, 4, 3
};

struct BurnDriver BurnDrvSboblboblc = {
	"sboblboblc", "bublbobl", NULL, NULL, "1986",
	"Super Bubble Bobble (bootleg)\0", NULL, "bootleg", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, SboblboblcRomInfo, SboblboblcRomName, NULL, NULL, NULL, NULL, BoblboblInputInfo, SboblbobDIPInfo,
	BoblboblInit, BublboblExit, DrvFrame, DrvDraw, DrvScan,
	NULL, 0x100, 256, 224, 4, 3
};

struct BurnDriver BurnDrvSboblbobld = {
	"sboblbobld", "bublbobl", NULL, NULL, "1986",
	"Super Bobble Bobble (bootleg, set 4)\0", NULL, "bootleg", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, SboblbobldRomInfo, SboblbobldRomName, NULL, NULL, NULL, NULL, BoblboblInputInfo, SboblbobDIPInfo,
	BoblboblInit, BublboblExit, DrvFrame, DrvDraw, DrvScan,
	NULL, 0x100, 256, 224, 4, 3
};

struct BurnDriver BurnDrvSboblboble = {
	"sboblboble", "bublbobl", NULL, NULL, "1986",
	"Super Bobble Bobble (bootleg, set 5)\0", NULL, "bootleg", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, SboblbobleRomInfo, SboblbobleRomName, NULL, NULL, NULL, NULL, BoblboblInputInfo, SboblbobDIPInfo,
	BoblboblInit, BublboblExit, DrvFrame, DrvDraw, DrvScan,
	NULL, 0x100, 256, 224, 4, 3
};

struct BurnDriver BurnDrvSboblboblf = {
	"sboblboblf", "bublbobl", NULL, NULL, "1986",
	"Super Bobble Bobble (bootleg, set 6)\0", NULL, "bootleg", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, SboblboblfRomInfo, SboblboblfRomName, NULL, NULL, NULL, NULL, BoblboblInputInfo, SboblbobDIPInfo,
	SboblboblfInit, BublboblExit, DrvFrame, DrvDraw, DrvScan,
	NULL, 0x100, 256, 224, 4, 3
};

struct BurnDriver BurnDrvBub68705 = {
	"bub68705", "bublbobl", NULL, NULL, "1986",
	"Bubble Bobble (bootleg with 68705)\0", NULL, "bootleg", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, Bub68705RomInfo, Bub68705RomName, NULL, NULL, NULL, NULL, BublboblInputInfo, BublboblDIPInfo,
	Bub68705Init, BublboblExit, DrvFrame, DrvDraw, DrvScan,
	NULL, 0x100, 256, 224, 4, 3
};

struct BurnDriver BurnDrvBub68705a = {
	"bub68705a", "bublbobl", NULL, NULL, "1986",
	"Bubble Bobble (boolteg with 68705, set 2)\0", NULL, "bootleg", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, Bub68705aRomInfo, Bub68705aRomName, NULL, NULL, NULL, NULL, BublboblInputInfo, BublboblDIPInfo,
	Bub68705Init, BublboblExit, DrvFrame, DrvDraw, DrvScan,
	NULL, 0x100, 256, 224, 4, 3
};

struct BurnDriver BurnDrvDland = {
	"dland", "bublbobl", NULL, NULL, "1987",
	"Dream Land / Super Dream Land (bootleg of Bubble Bobble)\0", NULL, "bootleg", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, DlandRomInfo, DlandRomName, NULL, NULL, NULL, NULL, BoblboblInputInfo, DlandDIPInfo,
	DlandInit, BublboblExit, DrvFrame, DrvDraw, DrvScan,
	NULL, 0x100, 256, 224, 4, 3
};

struct BurnDriver BurnDrvBublcave = {
	"bublcave", "bublbobl", NULL, NULL, "2013",
	"Bubble Bobble: Lost Cave V1.2\0", NULL, "hack (Bisboch and Aladar)", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, BublcaveRomInfo, BublcaveRomName, NULL, NULL, NULL, NULL, BublboblInputInfo, BublboblDIPInfo,
	BublboblInit, BublboblExit, DrvFrame, DrvDraw, DrvScan,
	NULL, 0x100, 256, 224, 4, 3
};

struct BurnDriver BurnDrvBoblcave = {
	"boblcave", "bublbobl", NULL, NULL, "2013",
	"Bubble Bobble: Lost Cave V1.2 (for Bobble Bobble PCB)\0", NULL, "hack (Bisboch and Aladar)", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, BoblcaveRomInfo, BoblcaveRomName, NULL, NULL, NULL, NULL, BoblboblInputInfo, BoblcaveDIPInfo,
	BoblboblInit, BublboblExit, DrvFrame, DrvDraw, DrvScan,
	NULL, 0x100, 256, 224, 4, 3
};

struct BurnDriver BurnDrvBublcave11 = {
	"bublcave11", "bublbobl", NULL, NULL, "2012",
	"Bubble Bobble: Lost Cave V1.1\0", NULL, "hack (Bisboch and Aladar)", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, Bublcave11RomInfo, Bublcave11RomName, NULL, NULL, NULL, NULL, BublboblInputInfo, BublboblDIPInfo,
	BublboblInit, BublboblExit, DrvFrame, DrvDraw, DrvScan,
	NULL, 0x100, 256, 224, 4, 3
};

struct BurnDriver BurnDrvBublcave10 = {
	"bublcave10", "bublbobl", NULL, NULL, "2012",
	"Bubble Bobble: Lost Cave V1.0\0", NULL, "hack (Bisboch and Aladar)", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, Bublcave10RomInfo, Bublcave10RomName, NULL, NULL, NULL, NULL, BublboblInputInfo, BublboblDIPInfo,
	BublboblInit, BublboblExit, DrvFrame, DrvDraw, DrvScan,
	NULL, 0x100, 256, 224, 4, 3
};

struct BurnDriver BurnDrvBublboblp = {
	"bublboblp", "bublbobl", NULL, NULL, "1986",
	"Bubble Bobble (prototype on Tokio hardware)\0", NULL, "Taito Corporation", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, BublboblpRomInfo, BublboblpRomName, NULL, NULL, NULL, NULL, BublboblInputInfo, BublboblDIPInfo,
	BublboblpInit, DrvExit, TokioFrame, DrvDraw, DrvScan,
	NULL, 0x100, 256, 224, 4, 3
};

struct BurnDriver BurnDrvTokio = {
	"tokio", NULL, NULL, NULL, "1986",
	"Tokio / Scramble Formation (newer)\0", NULL, "Taito Corporation", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, tokioRomInfo, tokioRomName, NULL, NULL, NULL, NULL, TokioInputInfo, TokioDIPInfo,
	TokioInit, DrvExit, TokioFrame, DrvDraw, DrvScan,
	NULL, 0x100, 224, 256, 3, 4
};

struct BurnDriver BurnDrvTokioo = {
	"tokioo", "tokio", NULL, NULL, "1986",
	"Tokio / Scramble Formation (older)\0", NULL, "Taito Corporation", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, tokiooRomInfo, tokiooRomName, NULL, NULL, NULL, NULL, TokioInputInfo, TokioDIPInfo,
	TokioInit, DrvExit, TokioFrame, DrvDraw, DrvScan,
	NULL, 0x100, 224, 256, 3, 4
};

struct BurnDriver BurnDrvTokiou = {
	"tokiou", "tokio", NULL, NULL, "1986",
	"Tokio / Scramble Formation (US)\0", NULL, "Taito America Corporation (Romstar license)", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, tokiouRomInfo, tokiouRomName, NULL, NULL, NULL, NULL, TokioInputInfo, TokioDIPInfo,
	TokioInit, DrvExit, TokioFrame, DrvDraw, DrvScan,
	NULL, 0x100, 224, 256, 3, 4
};

struct BurnDriver BurnDrvTokiob = {
	"tokiob", "tokio", NULL, NULL, "1986",
	"Tokio / Scramble Formation (bootleg)\0", NULL, "bootleg", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, tokiobRomInfo, tokiobRomName, NULL, NULL, NULL, NULL, TokioInputInfo, TokioDIPInfo,
	TokiobInit, DrvExit, TokioFrame, DrvDraw, DrvScan,
	NULL, 0x100, 224, 256, 3, 4
};
