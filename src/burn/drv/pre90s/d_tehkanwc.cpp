// FB Neo - Tehkan Wolrd Cup driver
// Based on MAME driver by Ernesto Corvi

#include "tiles_generic.h"
#include "z80_intf.h"
#include "ay8910.h"
#include "msm5205.h"
#include "burn_gun.h"
#include "burnint.h"
#include "watchdog.h"


// CPU clock definitions
#define MAIN_CPU_CLOCK          (18432000 / 4)    // 4,608,000
#define SUB_CPU_CLOCK           (18432000 / 4)
#define SOUND_CPU_CLOCK         (18432000 / 4)
#define SCREEN_CLOCK	        (18432000 / 3)    // 6,144,000
#define SCREEN_TOTAL_H		     384
#define SCREEN_TOTAL_V		     264
#define SCREEN_VBEND			 16
#define CPU_CYCLES_PER_FRAME	(3 * SCREEN_TOTAL_H * SCREEN_TOTAL_V / 4)
#define AY_CLOCK                (18432000 / 12)   // 1,536,000
#define MSM5205_CLOCK 	         384000


// CPU Ids
#define CPU_MAIN 0
#define CPU_SUB 1
#define CPU_SOUND 2

static UINT8 TWCInputPort0[8]    = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 TWCInputPort1[8]    = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 TWCInputPort2[8]    = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 TWCFakeInputPort[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 TWCDip[3]           = {0, 0, 0};
static UINT8 TWCInput[3]         = {0x00, 0x00, 0x00};
static INT16 track_p1[2];
static INT16 track_p2[2];
static INT32 track_reset_p1[2]   = {0, 0};
static INT32 track_reset_p2[2]   = {0, 0};
static UINT8 TWCReset            = 0;
static UINT8 m_digits[2];
static HoldCoin<2> hold_coin;

static UINT8 *Mem                = NULL;
static UINT8 *MemEnd             = NULL;
static UINT8 *RamStart           = NULL;
static UINT8 *RamEnd             = NULL;
static UINT8 *TWCZ80Rom1         = NULL;
static UINT8 *TWCZ80Rom2         = NULL;
static UINT8 *TWCZ80Rom3         = NULL;
static UINT8 *TWCSndRom          = NULL;
static UINT8 *TWCZ80Ram1         = NULL;
static UINT8 *TWCZ80Ram2         = NULL;
static UINT8 *TWCZ80Ram3         = NULL;
static UINT8 *TWCFgVideoRam      = NULL;
static UINT8 *TWCBgVideoRam      = NULL;
static UINT8 *TWCColorRam        = NULL;
static UINT8 *TWCSpriteRam       = NULL;
static UINT8 *TWCPalRam          = NULL;
static UINT8 *TWCPalRam2         = NULL;
static UINT8 *TWCSharedRam       = NULL;
static UINT32 *TWCPalette        = NULL;
static UINT8 TWCRecalc;
static UINT8 *TWCFgTiles         = NULL;
static UINT8 *TWCBgTiles         = NULL;
static UINT8 *TWCSprites         = NULL;
static UINT8 *TWCTempGfx         = NULL;

static UINT8 TWCScrollXHi;
static UINT8 TWCScrollXLo;
static UINT8 TWCScrollY;

static UINT8 TWCSoundLatch       = 0;
static UINT8 TWCSoundLatch2      = 0;
static int   msm_data_offs       = 0;
static INT32 msm_toggle          = 0;

static UINT8 TWCFlipScreenX;
static UINT8 TWCFlipScreenY;

static INT32 has_led             = 0;

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo TehkanWCInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,		TWCInputPort2 + 0,	    "p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,		TWCInputPort2 + 2,	    "p1 start"	},
	{"P1 Button",   	BIT_DIGITAL,		TWCInputPort0 + 5,	    "p1 fire 1"	},
	A("P1 X Analog",    BIT_ANALOG_REL,     &track_p1[0],		    "p1 x-axis" ),
	A("P1 Y Analog",    BIT_ANALOG_REL,     &track_p1[1],		    "p1 y-axis" ),      // 0x0080: DOWN, 0xff80: UP
	{"P1 X Analog Inc",	BIT_DIGITAL,	    TWCFakeInputPort + 0,	"p1 right"	},
	{"P1 X Analog Dec",	BIT_DIGITAL,	    TWCFakeInputPort + 1,	"p1 left"	},
	{"P1 Y Analog Inc",	BIT_DIGITAL,	    TWCFakeInputPort + 2,	"p1 down"	},
	{"P1 Y Analog Dec",	BIT_DIGITAL,	    TWCFakeInputPort + 3,	"p1 up"		},

	{"P2 Coin",			BIT_DIGITAL,		TWCInputPort2 + 1,	    "p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,		TWCInputPort2 + 3,	    "p2 start"	},
	{"P2 Button",		BIT_DIGITAL,		TWCInputPort1 + 5,	    "p2 fire 1"	},
	A("P2 X Analog",    BIT_ANALOG_REL,     &track_p2[0],           "p2 x-axis" ),
	A("P2 Y Analog",    BIT_ANALOG_REL,     &track_p2[1],           "p2 y-axis" ),
	{"P2 X Analog Inc",	BIT_DIGITAL,	    TWCFakeInputPort + 4,	"p2 right"	},
	{"P2 X Analog Dec",	BIT_DIGITAL,	    TWCFakeInputPort + 5,	"p2 left"	},
	{"P2 Y Analog Inc",	BIT_DIGITAL,	    TWCFakeInputPort + 6,	"p2 down"	},
	{"P2 Y Analog Dec",	BIT_DIGITAL,	    TWCFakeInputPort + 7,	"p2 up"		},

	{"Reset",			BIT_DIGITAL,		&TWCReset,	            "reset"		},
	{"Dip A",			BIT_DIPSWITCH,		TWCDip + 0,             "dip"		},
	{"Dip B",			BIT_DIPSWITCH,		TWCDip + 1,             "dip"		},
	{"Dip C",			BIT_DIPSWITCH,		TWCDip + 2,             "dip"		},
};
#undef A
STDINPUTINFO(TehkanWC)

inline static void TWCMakeInputs(INT32 nInterleave)
{
	TWCInput[0] = TWCInput[1] = TWCInput[2] = 0xff;

	for (INT32 i = 0; i < 8; i++) {
		TWCInput[0] ^= (TWCInputPort0[i] & 1) << i;
		TWCInput[1] ^= (TWCInputPort1[i] & 1) << i;
		TWCInput[2] ^= (TWCInputPort2[i] & 1) << i;
	}

	hold_coin.checklow(0, TWCInput[2], 1<<0, 2);
	hold_coin.checklow(1, TWCInput[2], 1<<1, 2);

	// device, portA_reverse?, portB_reverse?
	BurnTrackballConfig(0, AXIS_NORMAL, AXIS_NORMAL);
	// From MAME: port_sensitivity=100, port_keydelta=63
	// From AdvanceMame: fake keyboard input yields a |velocity| of 63 (0x7f / 2)
	//                   In absence of keyboard input, velocity must be 0x80, the rest value
	// track_pi = {0x0, 0x0} && trac_reset_pi = {0x80, 0x80} implies no movement
	BurnTrackballFrame(0, track_p1[0], track_p1[1], 0, 0x3f);  // 0x02, 0x0f taken from konami/d_bladestl.cpp
	BurnTrackballUDLR(0, TWCFakeInputPort[3], TWCFakeInputPort[2], TWCFakeInputPort[1], TWCFakeInputPort[0]);
	BurnTrackballUpdate(0);

	BurnTrackballConfig(1, AXIS_REVERSED, AXIS_REVERSED);
	BurnTrackballFrame(1, track_p2[0], track_p2[1], 0, 0x3f);
	BurnTrackballUDLR(1, TWCFakeInputPort[7], TWCFakeInputPort[6], TWCFakeInputPort[5], TWCFakeInputPort[4]);
	BurnTrackballUpdate(1);
}

static struct BurnDIPInfo TehkanWCDIPList[]=
{
	// I copied this line directly from d_bladestl.cpp. It works, I don't know why!
	{  0x13, 0xf0, 0xff, 0xff, NULL           /*starting offset*/           },

	// Default Values
	// nOffset, nID,     nMask,   nDefault,   NULL
	{  0x00,    0xff,    0xff,    0x0f,       NULL                          },
	{  0x01,    0xff,    0xff,    0xff,       NULL                          },
	{  0x02,    0xff,    0xff,    0xff,       NULL                          },

	// Dip 1
	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       4,          "Difficulty"                  },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x00,    0x01,    0x03,    0x02,       "Easy"                        },
	{  0x00,    0x01,    0x03,    0x03,       "Normal"                      },
	{  0x00,    0x01,    0x03,    0x01,       "Hard"                        },
	{  0x00,    0x01,    0x03,    0x00,       "Very Hard"                   },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       2,          "Timer Speed"                 },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x00,    0x01,    0x04,    0x04,       "60/60"                       },
	{  0x00,    0x01,    0x04,    0x00,       "55/60"                       },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       2,          "Demo Sounds"                 },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x00,    0x01,    0x08,    0x00,       "Off"                         },
	{  0x00,    0x01,    0x08,    0x08,       "On"                          },
      
	// Dip 2
	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       8,          "Coin A"                      },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x01,    0x01,    0x07,    0x01,       "2 Coins 1 Credit"            },
	{  0x01,    0x01,    0x07,    0x07,       "1 Coin  1 Credit"            },
	{  0x01,    0x01,    0x07,    0x00,       "2 Coins 3 Credits"           },
	{  0x01,    0x01,    0x07,    0x06,       "1 Coin  2 Credits"           },
	{  0x01,    0x01,    0x07,    0x05,       "1 Coin  3 Credits"           },
	{  0x01,    0x01,    0x07,    0x04,       "1 Coin  4 Credits"           },
	{  0x01,    0x01,    0x07,    0x03,       "1 Coin  5 Credits"           },
	{  0x01,    0x01,    0x07,    0x02,       "1 Coin  6 Credits"           },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       8,          "Coin B"                      },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x01,    0x01,    0x38,    0x08,       "2 Coins 1 Credit"            },
	{  0x01,    0x01,    0x38,    0x38,       "1 Coin  1 Credit"            },
	{  0x01,    0x01,    0x38,    0x00,       "2 Coins 3 Credits"           },
	{  0x01,    0x01,    0x38,    0x30,       "1 Coin  2 Credits"           },
	{  0x01,    0x01,    0x38,    0x28,       "1 Coin  3 Credits"           },
	{  0x01,    0x01,    0x38,    0x20,       "1 Coin  4 Credits"           },
	{  0x01,    0x01,    0x38,    0x18,       "1 Coin  5 Credits"           },
	{  0x01,    0x01,    0x38,    0x10,       "1 Coin  6 Credits"           },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       4,          "Start Credits (P1&P2)/Extra" },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x01,    0x01,    0xc0,    0x80,       "1&1/200%"                    },
	{  0x01,    0x01,    0xc0,    0xc0,       "1&2/100%"                    },
	{  0x01,    0x01,    0xc0,    0x40,       "2&2/100%"                    },
	{  0x01,    0x01,    0xc0,    0x00,       "2&3/67%"                     },

	// Dip 3
	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       4, "1P Game Time"                         },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x02,    0x01,    0x03,    0x00,       "2:30"                        },
	{  0x02,    0x01,    0x03,    0x01,       "2:00"                        },
	{  0x02,    0x01,    0x03,    0x03,       "1:30"                        },
	{  0x02,    0x01,    0x03,    0x02,       "1:00"                        },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       32,         "2P Game Time"                },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x02,    0x01,    0x7c,    0x00,       "5:00/3:00 Extra"             },
	{  0x02,    0x01,    0x7c,    0x60,       "5:00/2:45 Extra"             },
	{  0x02,    0x01,    0x7c,    0x20,       "5:00/2:35 Extra"             },
	{  0x02,    0x01,    0x7c,    0x40,       "5:00/2:30 Extra"             },
	{  0x02,    0x01,    0x7c,    0x04,       "4:00/2:30 Extra"             },
	{  0x02,    0x01,    0x7c,    0x64,       "4:00/2:15 Extra"             },
	{  0x02,    0x01,    0x7c,    0x24,       "4:00/2:05 Extra"             },
	{  0x02,    0x01,    0x7c,    0x44,       "4:00/2:00 Extra"             },
	{  0x02,    0x01,    0x7c,    0x1c,       "3:30/2:15 Extra"             },
	{  0x02,    0x01,    0x7c,    0x7c,       "3:30/2:00 Extra"             },
	{  0x02,    0x01,    0x7c,    0x3c,       "3:30/1:50 Extra"             },
	{  0x02,    0x01,    0x7c,    0x5c,       "3:30/1:45 Extra"             },
	{  0x02,    0x01,    0x7c,    0x08,       "3:00/2:00 Extra"             },
	{  0x02,    0x01,    0x7c,    0x68,       "3:00/1:45 Extra"             },
	{  0x02,    0x01,    0x7c,    0x28,       "3:00/1:35 Extra"             },
	{  0x02,    0x01,    0x7c,    0x48,       "3:00/1:30 Extra"             },
	{  0x02,    0x01,    0x7c,    0x0c,       "2:30/1:45 Extra"             },
	{  0x02,    0x01,    0x7c,    0x6c,       "2:30/1:30 Extra"             },
	{  0x02,    0x01,    0x7c,    0x2c,       "2:30/1:20 Extra"             },
	{  0x02,    0x01,    0x7c,    0x4c,       "2:30/1:15 Extra"             },
	{  0x02,    0x01,    0x7c,    0x10,       "2:00/1:30 Extra"             },
	{  0x02,    0x01,    0x7c,    0x70,       "2:00/1:15 Extra"             },
	{  0x02,    0x01,    0x7c,    0x30,       "2:00/1:05 Extra"             },
	{  0x02,    0x01,    0x7c,    0x50,       "2:00/1:00 Extra"             },
	{  0x02,    0x01,    0x7c,    0x14,       "1:30/1:15 Extra"             },
	{  0x02,    0x01,    0x7c,    0x74,       "1:30/1:00 Extra"             },
	{  0x02,    0x01,    0x7c,    0x34,       "1:30/0:50 Extra"             },
	{  0x02,    0x01,    0x7c,    0x54,       "1:30/0:45 Extra"             },
	{  0x02,    0x01,    0x7c,    0x18,       "1:00/1:00 Extra"             },
	{  0x02,    0x01,    0x7c,    0x78,       "1:00/0:45 Extra"             },
	{  0x02,    0x01,    0x7c,    0x38,       "1:00/0:35 Extra"             },
	{  0x02,    0x01,    0x7c,    0x58,       "1:00/0:30 Extra"             },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       2,          "Game Type"                   },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x02,    0x01,    0x80,    0x80,       "Timer In"                    },
	{  0x02,    0x01,    0x80,    0x00,       "Credit In"                   },
};
STDDIPINFO(TehkanWC)

static struct BurnDIPInfo TehkanWCdDIPList[]=
{
	// I copied this line directly from d_bladestl.cpp. It works, I don't know why!
	{  0x13, 0xf0, 0xff, 0xff, NULL/*starting offset*/                      },

	// Default Values
	// nOffset, nID,     nMask,   nDefault,   NULL
	{  0x00,    0xff,    0xff,    0x00,       NULL                          },
	{  0x01,    0xff,    0xff,    0xff,       NULL                          },
	{  0x02,    0xff,    0xff,    0xff,       NULL                          },

	// Dip 4 in test mode
	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       4,          "Difficulty"                  },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x00,    0x01,    0x03,    0x02,       "Easy"                        },
	{  0x00,    0x01,    0x03,    0x03,       "Normal"                      },
	{  0x00,    0x01,    0x03,    0x01,       "Hard"                        },
	{  0x00,    0x01,    0x03,    0x00,       "Very Hard"                   },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       2,          "Timer Speed"                 },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x00,    0x01,    0x04,    0x04,       "60/60"                       },
	{  0x00,    0x01,    0x04,    0x00,       "55/60"                       },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       2,          "Demo Sounds"                 },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x00,    0x01,    0x08,    0x00,       "Off"                         },
	{  0x00,    0x01,    0x08,    0x08,       "On"                          },

	// Dip 2
	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       8,          "Coin A"                      },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x01,    0x01,    0x07,    0x01,       "2 Coins 1 Credit"            },
	{  0x01,    0x01,    0x07,    0x07,       "1 Coin  1 Credit"            },
	{  0x01,    0x01,    0x07,    0x00,       "2 Coins 3 Credits"           },
	{  0x01,    0x01,    0x07,    0x06,       "1 Coin  2 Credits"           },
	{  0x01,    0x01,    0x07,    0x05,       "1 Coin  3 Credits"           },
	{  0x01,    0x01,    0x07,    0x04,       "1 Coin  4 Credits"           },
	{  0x01,    0x01,    0x07,    0x03,       "1 Coin  5 Credits"           },
	{  0x01,    0x01,    0x07,    0x02,       "1 Coin  6 Credits"           },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       8,          "Coin B"                      },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x01,    0x01,    0x38,    0x08,       "2 Coins 1 Credit"            },
	{  0x01,    0x01,    0x38,    0x38,       "1 Coin  1 Credit"            },
	{  0x01,    0x01,    0x38,    0x00,       "2 Coins 3 Credits"           },
	{  0x01,    0x01,    0x38,    0x30,       "1 Coin  2 Credits"           },
	{  0x01,    0x01,    0x38,    0x28,       "1 Coin  3 Credits"           },
	{  0x01,    0x01,    0x38,    0x20,       "1 Coin  4 Credits"           },
	{  0x01,    0x01,    0x38,    0x18,       "1 Coin  5 Credits"           },
	{  0x01,    0x01,    0x38,    0x10,       "1 Coin  6 Credits"           },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       4,          "Start Credits (P1&P2)/Extra" },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x01,    0x01,    0xc0,    0x80,       "1&1/200%"                    },
	{  0x01,    0x01,    0xc0,    0xc0,       "1&2/100%"                    },
	{  0x01,    0x01,    0xc0,    0x40,       "2&2/100%"                    },
	{  0x01,    0x01,    0xc0,    0x00,       "2&3/67%"                     },

	// Dip 3
	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       4, "1P Game Time"                         },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x02,    0x01,    0x03,    0x00,       "2:30"                        },
	{  0x02,    0x01,    0x03,    0x01,       "2:00"                        },
	{  0x02,    0x01,    0x03,    0x03,       "1:30"                        },
	{  0x02,    0x01,    0x03,    0x02,       "1:00"                        },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       32,         "2P Game Time"                },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x02,    0x01,    0x7c,    0x00,       "5:00/3:00 Extra"             },
	{  0x02,    0x01,    0x7c,    0x60,       "5:00/2:45 Extra"             },
	{  0x02,    0x01,    0x7c,    0x20,       "5:00/2:35 Extra"             },
	{  0x02,    0x01,    0x7c,    0x40,       "5:00/2:30 Extra"             },
	{  0x02,    0x01,    0x7c,    0x04,       "4:00/2:30 Extra"             },
	{  0x02,    0x01,    0x7c,    0x64,       "4:00/2:15 Extra"             },
	{  0x02,    0x01,    0x7c,    0x24,       "4:00/2:05 Extra"             },
	{  0x02,    0x01,    0x7c,    0x44,       "4:00/2:00 Extra"             },
	{  0x02,    0x01,    0x7c,    0x1c,       "3:30/2:15 Extra"             },
	{  0x02,    0x01,    0x7c,    0x7c,       "3:30/2:00 Extra"             },
	{  0x02,    0x01,    0x7c,    0x3c,       "3:30/1:50 Extra"             },
	{  0x02,    0x01,    0x7c,    0x5c,       "3:30/1:45 Extra"             },
	{  0x02,    0x01,    0x7c,    0x08,       "3:00/2:00 Extra"             },
	{  0x02,    0x01,    0x7c,    0x68,       "3:00/1:45 Extra"             },
	{  0x02,    0x01,    0x7c,    0x28,       "3:00/1:35 Extra"             },
	{  0x02,    0x01,    0x7c,    0x48,       "3:00/1:30 Extra"             },
	{  0x02,    0x01,    0x7c,    0x0c,       "2:30/1:45 Extra"             },
	{  0x02,    0x01,    0x7c,    0x6c,       "2:30/1:30 Extra"             },
	{  0x02,    0x01,    0x7c,    0x2c,       "2:30/1:20 Extra"             },
	{  0x02,    0x01,    0x7c,    0x4c,       "2:30/1:15 Extra"             },
	{  0x02,    0x01,    0x7c,    0x10,       "2:00/1:30 Extra"             },
	{  0x02,    0x01,    0x7c,    0x70,       "2:00/1:15 Extra"             },
	{  0x02,    0x01,    0x7c,    0x30,       "2:00/1:05 Extra"             },
	{  0x02,    0x01,    0x7c,    0x50,       "2:00/1:00 Extra"             },
	{  0x02,    0x01,    0x7c,    0x14,       "1:30/1:15 Extra"             },
	{  0x02,    0x01,    0x7c,    0x74,       "1:30/1:00 Extra"             },
	{  0x02,    0x01,    0x7c,    0x34,       "1:30/0:50 Extra"             },
	{  0x02,    0x01,    0x7c,    0x54,       "1:30/0:45 Extra"             },
	{  0x02,    0x01,    0x7c,    0x18,       "1:00/1:00 Extra"             },
	{  0x02,    0x01,    0x7c,    0x78,       "1:00/0:45 Extra"             },
	{  0x02,    0x01,    0x7c,    0x38,       "1:00/0:35 Extra"             },
	{  0x02,    0x01,    0x7c,    0x58,       "1:00/0:30 Extra"             },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       2,          "Game Type"                   },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x02,    0x01,    0x80,    0x80,       "Timer In"                    },
	{  0x02,    0x01,    0x80,    0x00,       "Credit In"                   },
};
STDDIPINFO(TehkanWCd)

static struct BurnDIPInfo GridironDIPList[]=
{
	// I copied this line directly from d_bladestl.cpp. It works, I don't know why!
	{  0x13, 0xf0, 0xff, 0xff, NULL  /*starting offset*/                    },

	// Default Values
	// nOffset, nID,     nMask,   nDefault,   NULL
	{  0x00,    0xff,    0xff,    0x00,       NULL                          },
	{  0x01,    0xff,    0xff,    0xff,       NULL                          },
	{  0x02,    0xff,    0xff,    0xff,       NULL                          },

	// Dip 2
	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       4,          "Start Credits (P1&P2)/Extra" },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x01,    0x01,    0x03,    0x01,       "1&1/200%"                    },
	{  0x01,    0x01,    0x03,    0x03,       "1&2/100%"                    },
	{  0x01,    0x01,    0x03,    0x00,       "2&1/200% (duplicate)"        },
	{  0x01,    0x01,    0x03,    0x02,       "2&2/100%"                    },

	/* This Dip Switch only has an effect in a 2 players game.
	   If offense player selects his formation before defense player,
	   defense formation time will be set to 3, 5 or 7 seconds.
	   Check code at 0x3ed9 and table at 0x3f89.
	*/
	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       4,          "Formation Time (Defense)"    },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x01,    0x01,    0x0c,    0x0c,       "Same as Offense"             },
	{  0x01,    0x01,    0x0c,    0x00,       "7"                           },
	{  0x01,    0x01,    0x0c,    0x08,       "5"                           },
	{  0x01,    0x01,    0x0c,    0x04,       "3"                           },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       4,          "Timer Speed"                 },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x01,    0x01,    0x30,    0x30,       "60/60"                       },
	{  0x01,    0x01,    0x30,    0x00,       "57/60"                       },
	{  0x01,    0x01,    0x30,    0x10,       "54/60"                       },
	{  0x01,    0x01,    0x30,    0x20,       "50/60"                       },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       4,          "Formation Time (Ofense)"     },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x01,    0x01,    0xc0,    0x00,       "25"                          },
	{  0x01,    0x01,    0xc0,    0x40,       "20"                          },
	{  0x01,    0x01,    0xc0,    0xc0,       "15"                          },
	{  0x01,    0x01,    0xc0,    0x80,       "10"                          },

	// Dip 3
	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       4,          "1P Game Time"                },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x02,    0x01,    0x03,    0x00,       "2:30"                        },
	{  0x02,    0x01,    0x03,    0x01,       "2:00"                        },
	{  0x02,    0x01,    0x03,    0x03,       "1:30"                        },
	{  0x02,    0x01,    0x03,    0x02,       "1:00"                        },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       32,         "2P Game Time"                },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x02,    0x01,    0x7c,    0x60,       "5:00/3:00 Extra"             },
	{  0x02,    0x01,    0x7c,    0x00,       "5:00/2:45 Extra"             },
	{  0x02,    0x01,    0x7c,    0x20,       "5:00/2:35 Extra"             },
	{  0x02,    0x01,    0x7c,    0x40,       "5:00/2:30 Extra"             },

	{  0x02,    0x01,    0x7c,    0x64,       "4:00/2:30 Extra"             },
	{  0x02,    0x01,    0x7c,    0x04,       "4:00/2:15 Extra"             },
	{  0x02,    0x01,    0x7c,    0x24,       "4:00/2:05 Extra"             },
	{  0x02,    0x01,    0x7c,    0x44,       "4:00/2:00 Extra"             },

	{  0x02,    0x01,    0x7c,    0x68,       "3:30/2:15 Extra"             },
	{  0x02,    0x01,    0x7c,    0x08,       "3:30/2:00 Extra"             },
	{  0x02,    0x01,    0x7c,    0x28,       "3:30/1:50 Extra"             },
	{  0x02,    0x01,    0x7c,    0x48,       "3:30/1:45 Extra"             },

	{  0x02,    0x01,    0x7c,    0x6c,       "3:00/2:00 Extra"             },
	{  0x02,    0x01,    0x7c,    0x0c,       "3:00/1:45 Extra"             },
	{  0x02,    0x01,    0x7c,    0x2c,       "3:00/1:35 Extra"             },
	{  0x02,    0x01,    0x7c,    0x4c,       "3:00/1:30 Extra"             },

	{  0x02,    0x01,    0x7c,    0x7c,       "2:30/1:45 Extra"             },
	{  0x02,    0x01,    0x7c,    0x1c,       "2:30/1:30 Extra"             },
	{  0x02,    0x01,    0x7c,    0x3c,       "2:30/1:20 Extra"             },
	{  0x02,    0x01,    0x7c,    0x5c,       "2:30/1:15 Extra"             },

	{  0x02,    0x01,    0x7c,    0x70,       "2:00/1:30 Extra"             },
	{  0x02,    0x01,    0x7c,    0x10,       "2:00/1:15 Extra"             },
	{  0x02,    0x01,    0x7c,    0x30,       "2:00/1:05 Extra"             },
	{  0x02,    0x01,    0x7c,    0x50,       "2:00/1:00 Extra"             },

	{  0x02,    0x01,    0x7c,    0x74,       "1:30/1:15 Extra"             },
	{  0x02,    0x01,    0x7c,    0x14,       "1:30/1:00 Extra"             },
	{  0x02,    0x01,    0x7c,    0x34,       "1:30/0:50 Extra"             },
	{  0x02,    0x01,    0x7c,    0x54,       "1:30/0:45 Extra"             },

	{  0x02,    0x01,    0x7c,    0x78,       "1:00/1:00 Extra"             },
	{  0x02,    0x01,    0x7c,    0x18,       "1:00/0:45 Extra"             },
	{  0x02,    0x01,    0x7c,    0x38,       "1:00/0:35 Extra"             },
	{  0x02,    0x01,    0x7c,    0x58,       "1:00/0:30 Extra"             },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       2,          "Demo Sounds"                 },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x02,    0x01,    0x80,    0x00,       "Off"                         },
	{  0x02,    0x01,    0x80,    0x80,       "On"                          },
};
STDDIPINFO(Gridiron)

static struct BurnDIPInfo TeedOffDIPList[]=
{
	// I copied this line directly from d_bladestl.cpp. It works, I don't know why!
	{  0x13, 0xf0, 0xff, 0xff, NULL/*starting offset*/                       },

	// Default Values
	// nOffset, nID,     nMask,   nDefault,   NULL
	{  0x00,    0xff,    0xff,    0x00,       NULL                          },
	{  0x01,    0xff,    0xff,    0xbf,       NULL                           },
	{  0x02,    0xff,    0xff,    0xff,       NULL                           },

	// Dip 2
	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       4,          "Coin A"                       },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x01,    0x01,    0x03,    0x02,       "2 Coins 1 Credit"             },
	{  0x01,    0x01,    0x03,    0x03,       "1 Coin 1 Credit"              },
	{  0x01,    0x01,    0x03,    0x01,       "1 Coin 2 Credits"             },
	{  0x01,    0x01,    0x03,    0x00,       "1 Coin 3 Credits"             },
	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       4,          "Coin B"                       },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x01,    0x01,    0x0c,    0x08,       "2 Coins 1 Credit"             },
	{  0x01,    0x01,    0x0c,    0x0c,       "1 Coin 1 Credit"              },
	{  0x01,    0x01,    0x0c,    0x04,       "1 Coin 2 Credits"             },
	{  0x01,    0x01,    0x0c,    0x00,       "1 Coin 3 Credits"             },
	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       4,          "Balls"                        },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x01,    0x01,    0x30,    0x30,       "5"                            },
	{  0x01,    0x01,    0x30,    0x20,       "6"                            },
	{  0x01,    0x01,    0x30,    0x10,       "7"                            },
	{  0x01,    0x01,    0x30,    0x00,       "8"                            },
	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       2,          "Cabinet"                      },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x01,    0x01,    0x40,    0x00,       "Upright"                      },
	{  0x01,    0x01,    0x40,    0x40,       "Cocktail"                     },
	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       2,          "Demo Sounds"                  },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x01,    0x01,    0x80,    0x00,       "Off"                          },
	{  0x01,    0x01,    0x80,    0x80,       "On"                           },

	// Dip 3
	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       4,          "Penalty (Over Par)"           },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x02,    0x01,    0x18,    0x10,       "1/1/2/3/4"                    },
	{  0x02,    0x01,    0x18,    0x18,       "1/2/3/3/4"                    },
	{  0x02,    0x01,    0x18,    0x08,       "1/2/3/4/4"                    },
	{  0x02,    0x01,    0x18,    0x00,       "2/3/3/4/4"                    },
	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       2,          "Bonus Balls (Multiple coins)" },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x02,    0x01,    0x20,    0x20,       "None"                         },
	{  0x02,    0x01,    0x20,    0x00,       "+1"                           },
	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       4,          "Difficulty?"                  },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x02,    0x01,    0xc0,    0x80,       "Easy"                         },
	{  0x02,    0x01,    0xc0,    0xc0,       "Normal"                       },
	{  0x02,    0x01,    0xc0,    0x40,       "Hard"                         },
	{  0x02,    0x01,    0xc0,    0x00,       "Hardest"                      },
};
STDDIPINFO(TeedOff)

static inline void data_address_w(INT32 chip, UINT8 offset, UINT8 data)
{
	AY8910Write(chip, ~offset & 1, data);
}

static void __fastcall TWCSoundWritePort(UINT16 port, UINT8 data)
{
	UINT8 offset = port & 0xff;

	switch (offset)
	{
		// MAME code uses data_address_w, which negates the port
		case 0x00:
		case 0x01:
			data_address_w(0, offset, data);
			return;

		case 0x02:
		case 0x03:
			data_address_w(1, offset, data);
			return;
	}
}

static UINT8 __fastcall TWCSoundReadPort(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
			return AY8910Read(0);
		case 0x02:
			return AY8910Read(1);
	}

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = Mem;

	TWCZ80Rom1            = Next; Next += 0x0c000;
	TWCZ80Rom2            = Next; Next += 0x08000;
	TWCZ80Rom3            = Next; Next += 0x04000;
	TWCSndRom             = Next; Next += 0x08000;

	RamStart              = Next;

	TWCZ80Ram1            = Next; Next += 0x00800;
	TWCZ80Ram2            = Next; Next += 0x04800;
	TWCZ80Ram3            = Next; Next += 0x00800;

	TWCSharedRam          = Next; Next += 0x00800;

	TWCFgVideoRam         = Next; Next += 0x00400;
	TWCColorRam           = Next; Next += 0x00400;
	TWCPalRam             = Next; Next += 0x00600;
	TWCPalRam2            = Next; Next += 0x00200;
	TWCBgVideoRam         = Next; Next += 0x00800;
	TWCSpriteRam          = Next; Next += 0x00400;

	RamEnd                = Next;

	// Char Layout: 512 chars, 8x8 pixels, 4 bits/pixel
	TWCFgTiles          = Next; Next += (512 * 8 * 8 * 4);
	// Tile Layout: 1024 tiles, 16x8 pixels, 4 bits/pixel
	TWCBgTiles            = Next; Next += (1024 * 16 * 8 * 4);
	// Sprite Layout: 512 sprites, 16x16 pixels, 4 bits/pixel
	TWCSprites            = Next; Next += (512 * 16 * 16 * 4);

	// Palette format: xBGR_444 (xxxxBBBBGGGGRRRR), 768
	TWCPalette            = (UINT32*)Next; Next += 0x00300 * sizeof(UINT32);

	MemEnd                 = Next;

	return 0;
}

/*
   Gridiron Fight has a LED display on the control panel, to let each player
   choose the formation without letting the other know.
*/

static void gridiron_led0_w(UINT8 data)
{
	m_digits[0] = (data & 0x80) ? (data & 0x7f) : 0;
}

static void gridiron_led1_w(UINT8 data)
{
	m_digits[1] = (data & 0x80) ? (data & 0x7f) : 0;
}


static UINT8 track_p1_r(UINT8 axis)
{
	return BurnTrackballRead(0, axis) - track_reset_p1[axis];
}

static void track_p1_reset_w(UINT8 axis, UINT8 data)
{
	track_reset_p1[axis] = BurnTrackballRead(0, axis) + data;
}

static UINT8 track_p2_r(UINT8 axis)
{
	return BurnTrackballRead(1, axis) - track_reset_p2[axis];
}

static void track_p2_reset_w(UINT8 axis, UINT8 data)
{
	track_reset_p2[axis] = BurnTrackballRead(1, axis) + data;
}

static UINT8 __fastcall TWCMainRead(UINT16 address)
{
	switch (address) {
		case 0xda00:
			return 0x80; // teedoff_unk_r

		case 0xf800:
		case 0xf801:
			//return trackball_read_p1(address & 1);
			return track_p1_r(address & 1);

		case 0xf802:
		case 0xf806:
			return TWCInput[2];  // System

		case 0xf810:
		case 0xf811:
		//return trackball_read_p2(address & 1);
		return track_p2_r(address & 1);

		case 0xf803:
			return TWCInput[0];  // Player 1. DSW4 in test mode (tehkanwcd)

		case 0xf813:
			return TWCInput[1];  // Player 2. DSW5 in test mode (tehkanwcd)

		case 0xf820:
			return TWCSoundLatch2;

		case 0xf840:
			return TWCDip[1];	// DSW2

		case 0xf860:
			BurnWatchdogReset();
			return 0;

		case 0xf850:
			return TWCDip[2];	// DSW3

		case 0xf870:
			return TWCDip[0];	//DSW1
	}

	return 0;
}


static void sound_sync()
{
	// Both CPUs are same model with same frequency
	INT32 cyc = ZetTotalCycles(CPU_MAIN) - ZetTotalCycles(CPU_SOUND);
	if (cyc > 0) {
	  ZetRun(CPU_SOUND, cyc);
	}
}


static void __fastcall TWCMainWrite(UINT16 address, UINT8 data)
{
	switch (address) {
		// 0xc000 .. 0xcfff: Shared RAM
		// 0xd000 .. 0xd3ff: TextVideoRAM
		// 0xd400 .. 0xd7ff: ColorRAM
		// 0xd800 .. 0xddff: PaletteRAM
		// 0xde00 .. 0xdfff: Shared RAM (PaletteRAM2)
		// 0xe000 .. 0xe7ff: BGVideoRAM
		// 0xe800 .. 0xebff: SpriteRAM
		case 0xec00:
			TWCScrollXLo = data;
			return;

		case 0xec01:
			TWCScrollXHi = data;
			return;

		case 0xec02:
			TWCScrollY = data;
			return;

		case 0xf800:
		case 0xf801:
			track_p1_reset_w(address & 1, data);
			return;

		case 0xf802:
			gridiron_led0_w(data);
			return;

		case 0xf810:
		case 0xf811:
			track_p2_reset_w(address & 1, data);
		return;

		case 0xf812:
			gridiron_led1_w(data);
			return;

		case 0xf820:
			sound_sync();
			TWCSoundLatch = data;
			ZetNmi(CPU_SOUND);
			return;

		case 0xf840:
			ZetSetRESETLine(CPU_SUB, data ? Z80_CLEAR_LINE : Z80_ASSERT_LINE);
			return;

		case 0xf850:
			ZetSetRESETLine(CPU_SOUND, data ? Z80_CLEAR_LINE : Z80_ASSERT_LINE);
			MSM5205ResetWrite(0, data ? 0 : 1);
			return;

		case 0xf860:
			TWCFlipScreenX = data & 0x40;
			// updateFlip() ??
			return;

		case 0xf870:
			TWCFlipScreenY = data & 0x40;
			// updateFlip() ??
			return;
	}
}

static UINT8 __fastcall TWCSubRead(UINT16 address)
{
	switch (address) {
		case 0xf860:
			BurnWatchdogReset();
			return 0;
	}

	return 0;
}

static void __fastcall TWCSubWrite(UINT16 address, UINT8 data)
{
	switch (address) {
		case 0xec00:
			TWCScrollXHi = data;
			return;

		case 0xec01:
			TWCScrollXLo = data;
			return;

		case 0xec02:
			TWCScrollY = data;
			return;
	}
}

static UINT8 __fastcall TWCSoundRead(UINT16 address)
{
	switch (address) {
		case 0xc000:
			return TWCSoundLatch;
	}

	return 0;
}

static void __fastcall TWCSoundWrite(UINT16 address, UINT8 data)
{
	switch (address) {
		case 0x8001:

			MSM5205ResetWrite(0, data ? 0 : 1);
			return;

		case 0x8002:
		case 0x8003:
			return;  // nopw

		case 0xc000:
			TWCSoundLatch2 = data;
			return;
	}
}

// Taken from d_TWC.cpp
inline static UINT32 xBGR_444_CalcCol(UINT16 nColour)
{
	INT32 r, g, b;

	// xBGR_444, xxxxBBBBGGGGRRRR
	b = (nColour >> 8) & 0x0f;
	g = (nColour >> 4) & 0x0f;
	r = (nColour >> 0) & 0x0f;

	b = (b << 4) | b;
	g = (g << 4) | g;
	r = (r << 4) | r;

	return BurnHighCol(r, g, b, 0);
}

static INT32 TWCCalcPalette()
{
	INT32 i;   

	for (i = 0; i < 0x600; i++) {
		TWCPalette[i / 2] = xBGR_444_CalcCol(TWCPalRam[i | 1] | (TWCPalRam[i & ~1] << 8));
	}

	return 0;
}

static tilemap_callback( twc_bg )
{
	offs *= 2; // ??

	INT32 attr  = TWCBgVideoRam[offs + 1];
	INT32 code  = TWCBgVideoRam[offs] + ((attr & 0x30) << 4);
	INT32 color = attr & 0x0f;
	INT32 flags = ((attr & 0x40) ? TILE_FLIPX : 0) | ((attr & 0x80) ? TILE_FLIPY : 0);

	TILE_SET_INFO(0, code, color, flags);
}

static tilemap_callback( twc_fg )
{
	INT32 attr  = TWCColorRam[offs];
	INT32 code  = TWCFgVideoRam[offs] + ((attr & 0x10) << 4);
	INT32 color = attr & 0x0f;
	INT32 flags = ((attr & 0x40) ? TILE_FLIPX : 0) | ((attr & 0x80) ? TILE_FLIPY : 0);
	flags |= TILE_GROUP_ENABLE;
	flags |= ((attr & 0x20) ? (1 << 16) : 0);

	TILE_SET_INFO(1, code, color, flags);
}

static void TWCRenderSprites()
{
	INT32 Code, Attr, Color, fx, fy, sx, sy;

	for (INT32 Offs = 0; Offs < 0x400; Offs += 4)
	{
		Attr = TWCSpriteRam[Offs + 1];
		Code = TWCSpriteRam[Offs] + ((Attr & 0x08) << 5);
		Color = Attr & 0x07;
		fx = Attr & 0x40;
		fy = Attr & 0x80;
		sx = TWCSpriteRam[Offs + 2] + ((Attr & 0x20) << 3) - 128;
		sy = TWCSpriteRam[Offs + 3];

		if (TWCFlipScreenX)
		{
			sx = 240 - sx;
			fx = !fx;
		}

		if (TWCFlipScreenY)
		{
			sy = 240 - sy;
			fy = !fy;
		}

		Draw16x16MaskTile(pTransDraw, Code, sx, sy - SCREEN_VBEND, fx, fy, Color, 4, 0, 256, TWCSprites);
	}
}

static INT32 TWCDraw()
{
	if (TWCRecalc) {
		TWCCalcPalette();
		TWCRecalc = 1;
	}
	
	GenericTilemapSetFlip(TMAP_GLOBAL, TWCFlipScreenX * TMAP_FLIPX | TWCFlipScreenY * TMAP_FLIPY);

	GenericTilemapSetScrollY(0, TWCScrollY);
	GenericTilemapSetScrollX(0, TWCScrollXLo + 256 * TWCScrollXHi);

	BurnTransferClear();

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);
	if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 1);
	if (nSpriteEnable & 1) TWCRenderSprites();
	if (nBurnLayer & 4) GenericTilemapDraw(1, pTransDraw, 0);

	BurnTransferCopy(TWCPalette);

	return 0;
}

static INT32 TWCDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset(RamStart, 0, RamEnd - RamStart);
	}

	ZetReset(0);
	ZetReset(1);

	ZetOpen(2);
	ZetReset();
	MSM5205Reset();
	ZetClose();

	AY8910Reset(0);
	AY8910Reset(1);

	hold_coin.reset();

	TWCScrollXHi   = 0;
	TWCScrollXLo   = 0;
	TWCScrollY     = 0;
	TWCFlipScreenX = 0;
	TWCFlipScreenY = 0;

	TWCSoundLatch  = 0;
	TWCSoundLatch2 = 0;

	msm_data_offs  = 0;
	msm_toggle     = 0;

	track_reset_p1[0] = 0;
	track_reset_p1[1] = 0;
	track_reset_p2[0] = 0;
	track_reset_p2[1] = 0;

	HiscoreReset();

	return 0;
}

static INT32 TWCFrame()
{
	BurnWatchdogUpdate();

	if (TWCReset) TWCDoReset(1); // Handle reset

	// Number of interrupt slices per frame
	INT32 nInterleave = MSM5205CalcInterleave(0, SOUND_CPU_CLOCK);

	TWCMakeInputs(nInterleave); // Update inputs

	ZetNewFrame(); // Reset CPU cycle counters

	// Total cycles each CPU should run per frame
	INT32 nCyclesTotal[3] = {
	  CPU_CYCLES_PER_FRAME, // Main CPU
	  CPU_CYCLES_PER_FRAME, // Sub CPU
	  CPU_CYCLES_PER_FRAME  // Audio CPU
	};

	INT32 nCyclesDone[3] = { 0, 0, 0 }; // Cycles executed so far


	// Run CPUs in sync
	for (INT32 i = 0; i < nInterleave; i++) {
	    // Main CPU
		ZetOpen(CPU_MAIN);
		CPU_RUN(CPU_MAIN, Zet); // Run the main CPU
		ZetClose();

	    // Sub CPU
		ZetOpen(CPU_SUB);
		CPU_RUN(CPU_SUB, Zet); // Run the sub CPU
		ZetClose();

	    // Audio CPU
		ZetOpen(CPU_SOUND);

		CPU_RUN(CPU_SOUND, Zet); // Run the audio CPU (with timer synchronization)

		// Update MSM5205 sound chip
		MSM5205Update();

		ZetClose();

		if (i == nInterleave - 1) {
			ZetSetIRQLine(CPU_MAIN, 0, CPU_IRQSTATUS_HOLD);
			ZetSetIRQLine(CPU_SUB, 0, CPU_IRQSTATUS_HOLD);
			ZetSetIRQLine(CPU_SOUND, 0, CPU_IRQSTATUS_HOLD);
		}
	}

	// Update sound
	ZetOpen(CPU_SOUND);
	if (pBurnSoundOut) {
		// Update AY-8910 sound chips
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
		MSM5205Render(0, pBurnSoundOut, nBurnSoundLen);
	}
	ZetClose();
	
	// Render frame
	if (pBurnDraw) BurnDrvRedraw();

	return 0;
}


static INT32 CharPlaneOffsets[4]   = { 0, 1, 2, 3 };
static INT32 CharXOffsets[8]       = { 4, 0, 12, 8, 20, 16, 28, 24 };
static INT32 CharYOffsets[8]       = { 0, 32, 64, 96, 128, 160, 192, 224 };

static INT32 SpritePlaneOffsets[4] = { 0, 1, 2, 3 };
static INT32 SpriteXOffsets[16]    = { 	4, 	0, 	12, 	8, 	20, 	16, 	28, 	24,
	                               	256+4, 256+0, 256+12, 256+8, 256+20, 256+16, 256+28, 256+24 };
static INT32 SpriteYOffsets[16]    = { 	0, 	32, 	64, 	96, 	128, 	160, 	192, 	224,
	                               	512+0, 512+32, 512+64, 512+96, 512+128, 512+160, 512+192, 512+224 };

static INT32 TilePlaneOffsets[4]   = { 0, 1, 2, 3 };
static INT32 TileXOffsets[16]      = { 	4, 	0, 	12, 	8, 	20, 	16, 	28, 	24,
	                               	256+4, 256+0, 256+12, 256+8, 256+20, 256+16, 256+28, 256+24 };
static INT32 TileYOffsets[8]       = { 0, 32, 64, 96, 128, 160, 192, 224 };

static INT32 DrvSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)(double)ZetTotalCycles() * nSoundRate / SOUND_CPU_CLOCK;
}

static UINT8 portA_r(UINT32)
{
	return msm_data_offs & 0xff;
}

static UINT8 portB_r(UINT32)
{
	return (msm_data_offs >> 8) & 0xff;
}

static void portA_w(UINT32, UINT32 data)
{
	msm_data_offs = (msm_data_offs & 0xff00) | data;
}

static void portB_w(UINT32, UINT32 data)
{
	msm_data_offs = (msm_data_offs & 0x00ff) | (data << 8);
}


static void adpcm_int()
{	
	UINT8 msm_data = TWCSndRom[msm_data_offs & 0x7fff];

	
	if (msm_toggle == 0)
		MSM5205DataWrite(0, (msm_data >> 4) & 0x0f);
	else
	{
		MSM5205DataWrite(0, msm_data & 0x0f);
		msm_data_offs++;
	}

	msm_toggle ^= 1;
}

static void CPUsInit()
{
	// Main CPU
	ZetInit(CPU_MAIN);
	ZetOpen(CPU_MAIN);
	ZetMapMemory(TWCZ80Rom1,    0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(TWCZ80Ram1,    0xc000, 0xc7ff, MAP_RAM);
	ZetMapMemory(TWCSharedRam,  0xc800, 0xcfff, MAP_RAM);
	ZetMapMemory(TWCFgVideoRam, 0xd000, 0xd3ff, MAP_RAM);
	ZetMapMemory(TWCColorRam,   0xd400, 0xd7ff, MAP_RAM);
	ZetMapMemory(TWCPalRam,     0xd800, 0xddff, MAP_RAM);
	ZetMapMemory(TWCPalRam2,    0xde00, 0xdfff, MAP_RAM);
	ZetMapMemory(TWCBgVideoRam, 0xe000, 0xe7ff, MAP_RAM);
	ZetMapMemory(TWCSpriteRam,  0xe800, 0xebff, MAP_RAM);
	ZetSetReadHandler(TWCMainRead);
	ZetSetWriteHandler(TWCMainWrite);
	ZetClose();

	// Graphics "sub" CPU
	ZetInit(CPU_SUB);
	ZetOpen(CPU_SUB);
	ZetMapMemory(TWCZ80Rom2,    0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(TWCZ80Ram2,    0x8000, 0xc7ff, MAP_RAM);
	ZetMapMemory(TWCSharedRam,  0xc800, 0xcfff, MAP_RAM);
	ZetMapMemory(TWCFgVideoRam, 0xd000, 0xd3ff, MAP_RAM);
	ZetMapMemory(TWCColorRam,   0xd400, 0xd7ff, MAP_RAM);
	ZetMapMemory(TWCPalRam,     0xd800, 0xddff, MAP_RAM);
	ZetMapMemory(TWCPalRam2,    0xde00, 0xdfff, MAP_RAM);
	ZetMapMemory(TWCBgVideoRam, 0xe000, 0xe7ff, MAP_RAM);
	ZetMapMemory(TWCSpriteRam,  0xe800, 0xebff, MAP_RAM);
	ZetSetReadHandler(TWCSubRead);
	ZetSetWriteHandler(TWCSubWrite);
	ZetClose();


	// Sound CPU
	ZetInit(CPU_SOUND);
	ZetOpen(CPU_SOUND);
	ZetMapMemory(TWCZ80Rom3, 0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(TWCZ80Ram3, 0x4000, 0x47ff, MAP_RAM);
	ZetSetReadHandler(TWCSoundRead);
	ZetSetWriteHandler(TWCSoundWrite);
	ZetSetOutHandler(TWCSoundWritePort);
	ZetSetInHandler(TWCSoundReadPort);
	ZetClose();
}

static void GFXInit()
{
	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, twc_bg_map_callback, 16,  8, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, twc_fg_map_callback,  8,  8, 32, 32);
	GenericTilemapSetGfx(0, TWCBgTiles, 4, 16,  8, 0x80000, 0x200, 0xf);
	GenericTilemapSetGfx(1, TWCFgTiles, 4,  8,  8, 0x20000, 0x000, 0xf);
	GenericTilemapSetOffsets(TMAP_GLOBAL,  0, -16);
	GenericTilemapSetTransparent(1,0);
}

static INT32 CommonRomLoad()
{
	INT32 nRet = 0, nLen;

	Mem = NULL;
	MemIndex();
	nLen = MemEnd - (UINT8 *)0;

	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;

	memset(Mem, 0, nLen);
	MemIndex();

	TWCTempGfx = (UINT8*)BurnMalloc(0x10000);
	if (TWCTempGfx == NULL) return 1;

	nRet = BurnLoadRom(TWCZ80Rom1 + 0x00000,  0,  1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(TWCZ80Rom1 + 0x04000,  1,  1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(TWCZ80Rom1 + 0x08000,  2,  1); if (nRet != 0) return 1;

	nRet = BurnLoadRom(TWCZ80Rom2 + 0x00000,  3,  1); if (nRet != 0) return 1;

	nRet = BurnLoadRom(TWCZ80Rom3 + 0x00000,  4,  1); if (nRet != 0) return 1;

	memset(TWCTempGfx, 0, 0x4000);
	nRet = BurnLoadRom(TWCTempGfx + 0x00000,  5, 1); if (nRet != 0) return 1;
	GfxDecode(512, 4, 8, 8, CharPlaneOffsets, CharXOffsets, CharYOffsets, 0x100, TWCTempGfx, TWCFgTiles);

	memset(TWCTempGfx, 0, 0x10000);
	nRet = BurnLoadRom(TWCTempGfx + 0x00000,  6, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(TWCTempGfx + 0x08000,  7, 1); if (nRet != 0) return 1;
	GfxDecode(512, 4, 16, 16, SpritePlaneOffsets, SpriteXOffsets, SpriteYOffsets, 0x400, TWCTempGfx, TWCSprites);

	memset(TWCTempGfx, 0, 0x10000);
	nRet = BurnLoadRom(TWCTempGfx + 0x00000,  8, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(TWCTempGfx + 0x08000,  9, 1); if (nRet != 0) return 1;
	GfxDecode(1024, 4, 16, 8, TilePlaneOffsets, TileXOffsets, TileYOffsets, 0x200, TWCTempGfx, TWCBgTiles);

	BurnFree(TWCTempGfx);

	nRet = BurnLoadRom(TWCSndRom, 10, 1); if (nRet !=0) return 1;

	return 0;
}

static INT32 GridironInit()
{
	INT32 nRet = 0, nLen;
	has_led = 1;

	Mem = NULL;
	MemIndex();
	nLen = MemEnd - (UINT8 *)0;

	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;

	memset(Mem, 0, nLen);
	MemIndex();

	TWCTempGfx = (UINT8*)BurnMalloc(0x10000);
	if (TWCTempGfx == NULL) return 1;

	nRet = BurnLoadRom(TWCZ80Rom1 + 0x00000,  0,  1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(TWCZ80Rom1 + 0x04000,  1,  1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(TWCZ80Rom1 + 0x08000,  2,  1); if (nRet != 0) return 1;

	nRet = BurnLoadRom(TWCZ80Rom2 + 0x00000,  3,  1); if (nRet != 0) return 1;

	nRet = BurnLoadRom(TWCZ80Rom3 + 0x00000,  4,  1); if (nRet != 0) return 1;

	memset(TWCTempGfx, 0, 0x4000);
	nRet = BurnLoadRom(TWCTempGfx + 0x00000,  5, 1); if (nRet != 0) return 1;
	GfxDecode(512, 4, 8, 8, CharPlaneOffsets, CharXOffsets, CharYOffsets, 0x100, TWCTempGfx, TWCFgTiles);

	memset(TWCTempGfx, 0, 0x10000);
	nRet = BurnLoadRom(TWCTempGfx + 0x00000,  6, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(TWCTempGfx + 0x04000,  7, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(TWCTempGfx + 0x08000,  8, 1); if (nRet != 0) return 1;
	GfxDecode(512, 4, 16, 16, SpritePlaneOffsets, SpriteXOffsets, SpriteYOffsets, 0x400, TWCTempGfx, TWCSprites);

	memset(TWCTempGfx, 0, 0x10000);
	nRet = BurnLoadRom(TWCTempGfx + 0x00000,  9, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(TWCTempGfx + 0x04000,  10, 1); if (nRet != 0) return 1;
	GfxDecode(1024, 4, 16, 8, TilePlaneOffsets, TileXOffsets, TileYOffsets, 0x200, TWCTempGfx, TWCBgTiles);

	BurnFree(TWCTempGfx);

	nRet = BurnLoadRom(TWCSndRom, 11, 1); if (nRet !=0) return 1;

	CPUsInit();
	GFXInit();

	BurnWatchdogInit(TWCDoReset, 180);

	// Initialize sound chips
	AY8910Init(0, AY_CLOCK, 0);
	AY8910Init(1, AY_CLOCK, 1);
	AY8910SetPorts(0, NULL, NULL, &portA_w, &portB_w);
	AY8910SetPorts(1, &portA_r, &portB_r, NULL, NULL);
	AY8910SetAllRoutes(0, 0.25, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.25, BURN_SND_ROUTE_BOTH);
	AY8910SetBuffered(ZetTotalCycles, SOUND_CPU_CLOCK);

	MSM5205Init(0, DrvSynchroniseStream, MSM5205_CLOCK, adpcm_int, MSM5205_S48_4B, 1);
	MSM5205SetRoute(0, 0.45, BURN_SND_ROUTE_BOTH);

	// Initialize analog controls for player 1 and player 2
	BurnTrackballInit(2);

	TWCDoReset(1);

	return 0;
}

static INT32 TehkanWCInit()
{
	has_led = 0;

	if(CommonRomLoad() != 0) return 1;

	CPUsInit();
	GFXInit();

	BurnWatchdogInit(TWCDoReset, 180);

	// Initialize sound chips
	AY8910Init(0, AY_CLOCK, 0);
	AY8910Init(1, AY_CLOCK, 1);
	AY8910SetPorts(0, NULL, NULL, &portA_w, &portB_w);
	AY8910SetPorts(1, &portA_r, &portB_r, NULL, NULL);
	AY8910SetAllRoutes(0, 0.25, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.25, BURN_SND_ROUTE_BOTH);
	AY8910SetBuffered(ZetTotalCycles, SOUND_CPU_CLOCK);

	MSM5205Init(0, DrvSynchroniseStream, MSM5205_CLOCK, adpcm_int, MSM5205_S48_4B, 1);
	MSM5205SetRoute(0, 0.45, BURN_SND_ROUTE_BOTH);

	// Initialize analog controls for player 1 and player 2
	BurnTrackballInit(2);

	TWCDoReset(1);

	return 0;
}

static INT32 TehkanWCbInit()
{
	has_led = 0;

	if(CommonRomLoad() != 0) return 1;

	CPUsInit();
	GFXInit();

	BurnWatchdogInit(TWCDoReset, 180);

	// Initialize sound chips
	AY8910Init(0, AY_CLOCK, 0);
	AY8910Init(1, AY_CLOCK, 1);
	AY8910SetPorts(0, NULL, NULL, &portA_w, &portB_w);
	AY8910SetPorts(1, &portA_r, &portB_r, NULL, NULL);
	AY8910SetAllRoutes(0, 0.25, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.25, BURN_SND_ROUTE_BOTH);
	AY8910SetBuffered(ZetTotalCycles, SOUND_CPU_CLOCK);

	MSM5205Init(0, DrvSynchroniseStream, MSM5205_CLOCK, adpcm_int, MSM5205_S48_4B, 1);
	MSM5205SetRoute(0, 0.45, BURN_SND_ROUTE_BOTH);

	// Initialize analog controls for player 1 and player 2
	BurnTrackballInit(2);

	TWCDoReset(1);

	return 0;
}

static INT32 TWCExit()
{
	ZetExit();
	GenericTilesExit();
	AY8910Exit(0);
	AY8910Exit(1);
	MSM5205Exit();
	BurnTrackballExit();

	BurnFree(Mem);
	
	TWCScrollXLo = 0;
	TWCScrollXLo = 0;
	TWCScrollY = 0;
	TWCSoundLatch  = 0;
	TWCSoundLatch2 = 0;

	return 0;
}

static INT32 TWCScan(INT32 nAction,INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin != NULL) {
		*pnMin = 0x029721;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = RamStart;
		ba.nLen	  = RamEnd - RamStart;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		ZetScan(nAction);
		AY8910Scan(nAction, pnMin);
		
		MSM5205Scan(nAction, pnMin);

		SCAN_VAR(TWCScrollXLo);
		SCAN_VAR(TWCScrollXLo);
		SCAN_VAR(TWCScrollY);
		SCAN_VAR(TWCSoundLatch);
		SCAN_VAR(TWCSoundLatch2);
		SCAN_VAR(TWCFlipScreenX);
		SCAN_VAR(TWCFlipScreenY);
		SCAN_VAR(msm_data_offs);
		SCAN_VAR(msm_toggle);
		SCAN_VAR(has_led);
		SCAN_VAR(track_reset_p1[0]);
		SCAN_VAR(track_reset_p1[1]);
		SCAN_VAR(track_reset_p2[0]);
		SCAN_VAR(track_reset_p2[1]);

		BurnTrackballScan();

		BurnWatchdogScan(nAction);

		hold_coin.scan();
	}

	return 0;
}

static struct BurnRomInfo TehkanWCRomDesc[] = {
	{ "twc-1.bin",  	0x04000, 0x34d6d5ff, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "twc-2.bin",  	0x04000, 0x7017a221, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "twc-3.bin",  	0x04000, 0x8b662902, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code

	{ "twc-4.bin",  	0x08000, 0x70a9f883, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program Code

	{ "twc-6.bin",  	0x04000, 0xe3112be2, BRF_ESS | BRF_PRG }, //  4	Z80 #3 Program Code

	{ "twc-12.bin", 	0x04000, 0xa9e274f8, BRF_GRA },           //  5 Fg Tiles

	{ "twc-8.bin",		0x08000, 0x055a5264, BRF_GRA },           //  6 Sprites
	{ "twc-7.bin",		0x08000, 0x59faebe7, BRF_GRA },           //  7 Sprites

	{ "twc-11.bin", 	0x08000, 0x669389fc, BRF_GRA },           //  8 Bg Tiles
	{ "twc-9.bin",		0x08000, 0x347ef108, BRF_GRA },           //  9 Bg Tiles

	{ "twc-5.bin",		0x04000, 0x444b5544, BRF_SND },           //  10 ADPCM Samples
};

STD_ROM_PICK(TehkanWC)
STD_ROM_FN(TehkanWC)

static struct BurnRomInfo TehkanWCbRomDesc[] = {
	{ "e-1.3-18.ic32",  	0x04000, 0xac9d851b, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "e-2.3-17.ic31",  	0x04000, 0x65b53d99, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "e-3.3-15.ic30",  	0x04000, 0x12064bfc, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code

	{ "e-4.9-17.ic100",  	0x08000, 0x70a9f883, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program Code

	{ "e-6.8-3.ic83",   	0x04000, 0xe3112be2, BRF_ESS | BRF_PRG }, //  4	Z80 #3 Program Code

	{ "e-12.8c.ic233",  	0x04000, 0xa9e274f8, BRF_GRA },           //  5 Fg Tiles

	{ "e-8.5n.ic191",		0x08000, 0x055a5264, BRF_GRA },           //  6 Sprites
	{ "e-7.5r.ic193",		0x08000, 0x59faebe7, BRF_GRA },           //  7 Sprites

	{ "e-11.8k.ic238",  	0x08000, 0x669389fc, BRF_GRA },           //  8 Bg Tiles
	{ "e-9.8n.ic240",		0x08000, 0x347ef108, BRF_GRA },           //  9 Bg Tiles

	{ "e-5.4-3.ic35",		0x04000, 0x444b5544, BRF_SND },           //  10 ADPCM Samples
};

STD_ROM_PICK(TehkanWCb)
STD_ROM_FN(TehkanWCb)

/* only rom e1 is changed from above bootleg */
static struct BurnRomInfo TehkanWCcRomDesc[] = {
	{ "e1.bin",  	0x04000, 0x7aaaddef, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "e2.bin",  	0x04000, 0x65b53d99, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "e3.bin",  	0x04000, 0x12064bfc, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code

	{ "e4.bin",  	0x08000, 0x70a9f883, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program Code

	{ "e6.bin",   	0x04000, 0xe3112be2, BRF_ESS | BRF_PRG }, //  4	Z80 #3 Program Code

	{ "e12.bin",  	0x04000, 0xa9e274f8, BRF_GRA },           //  5 Fg Tiles

	{ "e8.bin",		0x08000, 0x055a5264, BRF_GRA },           //  6 Sprites
	{ "e7.bin",		0x08000, 0x59faebe7, BRF_GRA },           //  7 Sprites

	{ "e11.bin",  	0x08000, 0x669389fc, BRF_GRA },           //  8 Bg Tiles
	{ "e9.bin",		0x08000, 0x347ef108, BRF_GRA },           //  9 Bg Tiles

	{ "e5.bin",		0x04000, 0x444b5544, BRF_SND },           //  10 ADPCM Samples
};

STD_ROM_PICK(TehkanWCc)
STD_ROM_FN(TehkanWCc)

// from a 2-PCB set labeled "A-32302 Tehkan" and "B-32302 Tehkan"
static struct BurnRomInfo TehkanWCdRomDesc[] = {
	{ "1.bin",      	0x04000, 0x2218d00f, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "2.bin",      	0x04000, 0xdbb39858, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "3.bin",      	0x04000, 0x9c69c64a, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code

	{ "4.bin",      	0x08000, 0x19533319, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program Code

	{ "twc-6.bin",  	0x04000, 0xe3112be2, BRF_ESS | BRF_PRG }, //  4	Z80 #3 Program Code

	{ "twc-12.bin", 	0x04000, 0xa9e274f8, BRF_GRA },           //  5 Fg Tiles

	{ "twc-8.bin",		0x08000, 0x055a5264, BRF_GRA },           //  6 Sprites
	{ "twc-7.bin",		0x08000, 0x59faebe7, BRF_GRA },           //  7 Sprites

	{ "twc-11.bin", 	0x08000, 0x669389fc, BRF_GRA },           //  8 Bg Tiles
	{ "twc-9.bin",		0x08000, 0x347ef108, BRF_GRA },           //  9 Bg Tiles

	{ "twc-5.bin",		0x04000, 0x444b5544, BRF_SND },           //  10 ADPCM Samples
};

STD_ROM_PICK(TehkanWCd)
STD_ROM_FN(TehkanWCd)

/* Just a year hack to put "1986" plus some other small changes, but this set has been found on different bootleg TWC PCBs. */
static struct BurnRomInfo TehkanWChRomDesc[] = {
	{ "worldcup_3.bin",      	0x04000, 0xdd3f789b, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "worldcup_2.bin",      	0x04000, 0x7017a221, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "worldcup_1.bin",      	0x04000, 0x8b662902, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code

	{ "worldcup_6.bin",      	0x08000, 0x70a9f883, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program Code

	{ "worldcup_5.bin", 	 	0x04000, 0xe3112be2, BRF_ESS | BRF_PRG }, //  4	Z80 #3 Program Code

	{ "worldcup_9.bin", 		0x04000, 0xa9e274f8, BRF_GRA },           //  5 Fg Tiles

	{ "worldcup_7.bin",			0x08000, 0x055a5264, BRF_GRA },           //  6 Sprites
	{ "worldcup_8.bin",			0x08000, 0x59faebe7, BRF_GRA },           //  7 Sprites

	{ "worldcup_10.bin", 		0x08000, 0x669389fc, BRF_GRA },           //  8 Bg Tiles
	{ "worldcup_11.bin",		0x08000, 0x4ea7586f, BRF_GRA },           //  9 Bg Tiles

	{ "worldcup_4.bin",			0x04000, 0x444b5544, BRF_SND },           //  10 ADPCM Samples
};

STD_ROM_PICK(TehkanWCh)
STD_ROM_FN(TehkanWCh)

static struct BurnRomInfo GridironRomDesc[] = {
	{ "gfight1.bin",      	0x04000, 0x51612741, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "gfight2.bin",      	0x04000, 0xa678db48, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "gfight3.bin",      	0x04000, 0x8c227c33, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code

	{ "gfight4.bin",      	0x04000, 0x8821415f, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program Code

	{ "gfight5.bin", 	 	0x04000, 0x92ca3c07, BRF_ESS | BRF_PRG }, //  4	Z80 #3 Program Code

	{ "gfight7.bin", 		0x04000, 0x04390cca, BRF_GRA },           //  5 Fg Tiles

	{ "gfight8.bin",		0x04000, 0x5de6a70f, BRF_GRA },           //  6 Sprites
	{ "gfight9.bin",		0x04000, 0xeac9dc16, BRF_GRA },           //  7 Sprites
	{ "gfight10.bin",		0x04000, 0x61d0690f, BRF_GRA },           //  8 Sprites
	/* 0c000-0ffff empty */

	{ "gfight11.bin", 		0x04000, 0x80b09c03, BRF_GRA },           //  9 Bg Tiles
	{ "gfight12.bin",		0x04000, 0x1b615eae, BRF_GRA },           //  10 Bg Tiles
	/* 08000-0ffff empty */

	{ "gfight6.bin",		0x04000, 0xd05d463d, BRF_SND },           //  10 ADPCM Samples
};

STD_ROM_PICK(Gridiron)
STD_ROM_FN(Gridiron)

static struct BurnRomInfo TeedOffRomDesc[] = {
	{ "1_m5m27c128_dip28.4a",      	0x04000, 0x0e18f6ee, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "2_m5m27c128_dip28.4b",      	0x04000, 0x70635a77, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "3_m5m27c128_dip28.4d",      	0x04000, 0x2c765def, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code

	{ "4_hn27256g@dip28.9c",      	0x08000, 0xa21315bf, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program Code

	{ "6_m5m27c128_dip28.8r", 	 	0x04000, 0xf87a43f5, BRF_ESS | BRF_PRG }, //  4	Z80 #3 Program Code

	{ "12_m5m27c128_dip28.8u", 		0x04000, 0x4f44622c, BRF_GRA },           //  5 Fg Tiles

	{ "8_hn27256g_dip28.5j",		0x08000, 0x363bd1ba, BRF_GRA },           //  6 Sprites
	{ "7_hn27256g_dip28.5e",		0x08000, 0x6583fa5b, BRF_GRA },           //  7 Sprites

	{ "11_hn27256g_dip28.8m", 		0x08000, 0x1ec00cb5, BRF_GRA },           //  8 Bg Tiles
	{ "9_hn27256g_dip28.8j",		0x08000, 0xa14347f0, BRF_GRA },           //  9 Bg Tiles

	{ "5_m5m27c256k_dip28.4r",		0x08000, 0x90141093, BRF_SND },           //  10 ADPCM Samples
};

STD_ROM_PICK(TeedOff)
STD_ROM_FN(TeedOff)

static struct BurnRomInfo TeedOffjRomDesc[] = {
	{ "to-1.4a",    			  	0x04000, 0xcc2aebc5, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "to-2.4b",    			  	0x04000, 0xf7c9f138, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "to-3.4d",    			  	0x04000, 0xa0f0a6da, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code

	{ "to-4.9c",    			  	0x08000, 0xe922cbd2, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program Code

	{ "to-6.8r", 				 	0x04000, 0xd8dfe1c8, BRF_ESS | BRF_PRG }, //  4	Z80 #3 Program Code

	{ "12_m5m27c128_dip28.8u", 		0x04000, 0x4f44622c, BRF_GRA },           //  5 Fg Tiles

	{ "8_hn27256g_dip28.5j",		0x08000, 0x363bd1ba, BRF_GRA },           //  6 Sprites
	{ "7_hn27256g_dip28.5e",		0x08000, 0x6583fa5b, BRF_GRA },           //  7 Sprites

	{ "11_hn27256g_dip28.8m", 		0x08000, 0x1ec00cb5, BRF_GRA },           //  8 Bg Tiles
	{ "9_hn27256g_dip28.8j",		0x08000, 0xa14347f0, BRF_GRA },           //  9 Bg Tiles

	{ "to-5.4r",					0x08000, 0xe5e4246b, BRF_SND },           //  10 ADPCM Samples
};

STD_ROM_PICK(TeedOffj)
STD_ROM_FN(TeedOffj)

struct BurnDriver BurnDrvTehkanWC = {
	"tehkanwc",				// The filename of the zip file (without extension)
	NULL,					// The filename of the parent (without extension, NULL if not applicable)
	NULL,					// The filename of the board ROMs (without extension, NULL if not applicable)
	NULL,					// The filename of the samples zip file (without extension, NULL if not applicable)
	"1985",
	"Tehkan World Cup (set 1)\0",	// Full Name A
	NULL,							// Comment A
	"Tehkan",						// Manufacturer A
	"Miscellaneous",				// System A
	NULL,					// Full Name W
	NULL,					// Comment W
	NULL,					// Manufacturer W
	NULL,					// System W
	// Flags
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED,
	2,						// Players: Max number of players a game supports (so we can remove single player games from netplay)
	HARDWARE_MISC_PRE90S, 	// Hardware: Which type of hardware the game runs on
	GBF_SPORTSFOOTBALL,		// Genre
	0,						// Family
	NULL,					// Function to get possible zip names
	TehkanWCRomInfo,		// Function to get the length and crc of each rom
	TehkanWCRomName,		// Function to get the possible names for each rom
	NULL,					// Function to get hdd info
	NULL,					// Function to get the possible names for each hdd
	NULL,					// Function to get the sample flags
	NULL,					// Function to get the possible names for each sample
	TehkanWCInputInfo,		// Function to get the input info for the game
	TehkanWCDIPInfo,		// Function to get the input info for the game
	TehkanWCInit,				// Init
	TWCExit,				// Exit
	TWCFrame,				// Frame
	TWCDraw,				// Redraw
	TWCScan,				// Area Scan
	&TWCRecalc,				// Recalc Palettes: Set to 1 if the palette needs to be fully re-calculated
	0x300,					// Number of Palette Entries
	256,					// Screen width
	224,					// Screen height
	4,						// Screen x aspect
	3 						// Screen y aspect
};

struct BurnDriver BurnDrvTehkanWCb = {
	"tehkanwcb",				// The filename of the zip file (without extension)
	"tehkanwc",					// The filename of the parent (without extension, NULL if not applicable)
	NULL,					// The filename of the board ROMs (without extension, NULL if not applicable)
	NULL,					// The filename of the samples zip file (without extension, NULL if not applicable)
	"1985",
	"Tehkan World Cup (set 2, bootleg?)\0",	// Full Name A
	NULL,							// Comment A
	"Tehkan",						// Manufacturer A
	"Miscellaneous",				// System A
	NULL,					// Full Name W
	NULL,					// Comment W
	NULL,					// Manufacturer W
	NULL,					// System W
	// Flags
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED,
	2,						// Players: Max number of players a game supports (so we can remove single player games from netplay)
	HARDWARE_MISC_PRE90S, 	// Hardware: Which type of hardware the game runs on
	GBF_SPORTSFOOTBALL,		// Genre
	0,						// Family
	NULL,					// Function to get possible zip names
	TehkanWCbRomInfo,		// Function to get the length and crc of each rom
	TehkanWCbRomName,		// Function to get the possible names for each rom
	NULL,					// Function to get hdd info
	NULL,					// Function to get the possible names for each hdd
	NULL,					// Function to get the sample flags
	NULL,					// Function to get the possible names for each sample
	TehkanWCInputInfo,		// Function to get the input info for the game
	TehkanWCDIPInfo,		// Function to get the input info for the game
	TehkanWCbInit,				// Init
	TWCExit,				// Exit
	TWCFrame,				// Frame
	TWCDraw,				// Redraw
	TWCScan,				// Area Scan
	&TWCRecalc,				// Recalc Palettes: Set to 1 if the palette needs to be fully re-calculated
	0x300,					// Number of Palette Entries
	256,					// Screen width
	224,					// Screen height
	4,						// Screen x aspect
	3 						// Screen y aspect
};
struct BurnDriver BurnDrvTehkanWCc = {
	"tehkanwcc",				// The filename of the zip file (without extension)
	"tehkanwc",					// The filename of the parent (without extension, NULL if not applicable)
	NULL,					// The filename of the board ROMs (without extension, NULL if not applicable)
	NULL,					// The filename of the samples zip file (without extension, NULL if not applicable)
	"1985",
	"Tehkan World Cup (set 3, bootleg)\0",	// Full Name A
	NULL,							// Comment A
	"bootleg",						// Manufacturer A
	"Miscellaneous",				// System A
	NULL,					// Full Name W
	NULL,					// Comment W
	NULL,					// Manufacturer W
	NULL,					// System W
	// Flags
	BDF_GAME_NOT_WORKING | BDF_HISCORE_SUPPORTED,
	2,						// Players: Max number of players a game supports (so we can remove single player games from netplay)
	HARDWARE_MISC_PRE90S, 	// Hardware: Which type of hardware the game runs on
	GBF_SPORTSFOOTBALL,		// Genre
	0,						// Family
	NULL,					// Function to get possible zip names
	TehkanWCcRomInfo,		// Function to get the length and crc of each rom
	TehkanWCcRomName,		// Function to get the possible names for each rom
	NULL,					// Function to get hdd info
	NULL,					// Function to get the possible names for each hdd
	NULL,					// Function to get the sample flags
	NULL,					// Function to get the possible names for each sample
	TehkanWCInputInfo,		// Function to get the input info for the game
	TehkanWCDIPInfo,		// Function to get the input info for the game
	TehkanWCInit,				// Init
	TWCExit,				// Exit
	TWCFrame,				// Frame
	TWCDraw,				// Redraw
	TWCScan,				// Area Scan
	&TWCRecalc,				// Recalc Palettes: Set to 1 if the palette needs to be fully re-calculated
	0x300,					// Number of Palette Entries
	256,					// Screen width
	224,					// Screen height
	4,						// Screen x aspect
	3 						// Screen y aspect
};
struct BurnDriver BurnDrvTehkanWCd = {
	"tehkanwcd",				// The filename of the zip file (without extension)
	"tehkanwc",					// The filename of the parent (without extension, NULL if not applicable)
	NULL,					// The filename of the board ROMs (without extension, NULL if not applicable)
	NULL,					// The filename of the samples zip file (without extension, NULL if not applicable)
	"1985",
	"Tehkan World Cup (set 4, earlier)\0",	// Full Name A
	NULL,							// Comment A
	"Tehkan",						// Manufacturer A
	"Miscellaneous",				// System A
	NULL,					// Full Name W
	NULL,					// Comment W
	NULL,					// Manufacturer W
	NULL,					// System W
	// Flags
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED,
	2,						// Players: Max number of players a game supports (so we can remove single player games from netplay)
	HARDWARE_MISC_PRE90S, 	// Hardware: Which type of hardware the game runs on
	GBF_SPORTSFOOTBALL,		// Genre
	0,						// Family
	NULL,					// Function to get possible zip names
	TehkanWCdRomInfo,		// Function to get the length and crc of each rom
	TehkanWCdRomName,		// Function to get the possible names for each rom
	NULL,					// Function to get hdd info
	NULL,					// Function to get the possible names for each hdd
	NULL,					// Function to get the sample flags
	NULL,					// Function to get the possible names for each sample
	TehkanWCInputInfo,		// Function to get the input info for the game
	TehkanWCdDIPInfo,		// Function to get the input info for the game
	TehkanWCInit,				// Init
	TWCExit,				// Exit
	TWCFrame,				// Frame
	TWCDraw,				// Redraw
	TWCScan,				// Area Scan
	&TWCRecalc,				// Recalc Palettes: Set to 1 if the palette needs to be fully re-calculated
	0x300,					// Number of Palette Entries
	256,					// Screen width
	224,					// Screen height
	4,						// Screen x aspect
	3 						// Screen y aspect
};

struct BurnDriver BurnDrvTehkanWCh = {
	"tehkanwch",				// The filename of the zip file (without extension)
	"tehkanwc",					// The filename of the parent (without extension, NULL if not applicable)
	NULL,					// The filename of the board ROMs (without extension, NULL if not applicable)
	NULL,					// The filename of the samples zip file (without extension, NULL if not applicable)
	"1986",
	"Tehkan World Cup (1986 year hack)\0",	// Full Name A
	NULL,							// Comment A
	"hack",						// Manufacturer A
	"Miscellaneous",				// System A
	NULL,					// Full Name W
	NULL,					// Comment W
	NULL,					// Manufacturer W
	NULL,					// System W
	// Flags
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED,
	2,						// Players: Max number of players a game supports (so we can remove single player games from netplay)
	HARDWARE_MISC_PRE90S, 	// Hardware: Which type of hardware the game runs on
	GBF_SPORTSFOOTBALL,		// Genre
	0,						// Family
	NULL,					// Function to get possible zip names
	TehkanWChRomInfo,		// Function to get the length and crc of each rom
	TehkanWChRomName,		// Function to get the possible names for each rom
	NULL,					// Function to get hdd info
	NULL,					// Function to get the possible names for each hdd
	NULL,					// Function to get the sample flags
	NULL,					// Function to get the possible names for each sample
	TehkanWCInputInfo,		// Function to get the input info for the game
	TehkanWCDIPInfo,		// Function to get the input info for the game
	TehkanWCInit,				// Init
	TWCExit,				// Exit
	TWCFrame,				// Frame
	TWCDraw,				// Redraw
	TWCScan,				// Area Scan
	&TWCRecalc,				// Recalc Palettes: Set to 1 if the palette needs to be fully re-calculated
	0x300,					// Number of Palette Entries
	256,					// Screen width
	224,					// Screen height
	4,						// Screen x aspect
	3 						// Screen y aspect
};

struct BurnDriver BurnDrvGridiron = {
	"gridiron",				// The filename of the zip file (without extension)
	NULL,					// The filename of the parent (without extension, NULL if not applicable)
	NULL,					// The filename of the board ROMs (without extension, NULL if not applicable)
	NULL,					// The filename of the samples zip file (without extension, NULL if not applicable)
	"1985",
	"Gridiron Fight\0",	// Full Name A
	NULL,							// Comment A
	"Tehkan",						// Manufacturer A
	"Miscellaneous",				// System A
	NULL,					// Full Name W
	NULL,					// Comment W
	NULL,					// Manufacturer W
	NULL,					// System W
	// Flags
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED,
	2,						// Players: Max number of players a game supports (so we can remove single player games from netplay)
	HARDWARE_MISC_PRE90S, 	// Hardware: Which type of hardware the game runs on
	GBF_SPORTSMISC,		// Genre
	0,						// Family
	NULL,					// Function to get possible zip names
	GridironRomInfo,		// Function to get the length and crc of each rom
	GridironRomName,		// Function to get the possible names for each rom
	NULL,					// Function to get hdd info
	NULL,					// Function to get the possible names for each hdd
	NULL,					// Function to get the sample flags
	NULL,					// Function to get the possible names for each sample
	TehkanWCInputInfo,		// Function to get the input info for the game
	GridironDIPInfo,		// Function to get the input info for the game
	GridironInit,				// Init
	TWCExit,				// Exit
	TWCFrame,				// Frame
	TWCDraw,				// Redraw
	TWCScan,				// Area Scan
	&TWCRecalc,				// Recalc Palettes: Set to 1 if the palette needs to be fully re-calculated
	0x300,					// Number of Palette Entries
	256,					// Screen width
	224,					// Screen height
	4,						// Screen x aspect
	3 						// Screen y aspect
};

struct BurnDriver BurnDrvTeedOff = {
	"teedoff",				// The filename of the zip file (without extension)
	NULL,					// The filename of the parent (without extension, NULL if not applicable)
	NULL,					// The filename of the board ROMs (without extension, NULL if not applicable)
	NULL,					// The filename of the samples zip file (without extension, NULL if not applicable)
	"1987",
	"Tee'd Off (World)\0",	// Full Name A
	NULL,							// Comment A
	"Tehkan",						// Manufacturer A
	"Miscellaneous",				// System A
	NULL,					// Full Name W
	NULL,					// Comment W
	NULL,					// Manufacturer W
	NULL,					// System W
	// Flags
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED,
	2,						// Players: Max number of players a game supports (so we can remove single player games from netplay)
	HARDWARE_MISC_PRE90S, 	// Hardware: Which type of hardware the game runs on
	GBF_SPORTSMISC,		// Genre
	0,						// Family
	NULL,					// Function to get possible zip names
	TeedOffRomInfo,		// Function to get the length and crc of each rom
	TeedOffRomName,		// Function to get the possible names for each rom
	NULL,					// Function to get hdd info
	NULL,					// Function to get the possible names for each hdd
	NULL,					// Function to get the sample flags
	NULL,					// Function to get the possible names for each sample
	TehkanWCInputInfo,		// Function to get the input info for the game
	TeedOffDIPInfo,		// Function to get the input info for the game
	TehkanWCInit,				// Init
	TWCExit,				// Exit
	TWCFrame,				// Frame
	TWCDraw,				// Redraw
	TWCScan,				// Area Scan
	&TWCRecalc,				// Recalc Palettes: Set to 1 if the palette needs to be fully re-calculated
	0x300,					// Number of Palette Entries
	224,					// Screen width
	256,					// Screen height
	3,						// Screen x aspect
	4 						// Screen y aspect
};

struct BurnDriver BurnDrvTeedOffj = {
	"teedoffj",				// The filename of the zip file (without extension)
	"teedoff",					// The filename of the parent (without extension, NULL if not applicable)
	NULL,					// The filename of the board ROMs (without extension, NULL if not applicable)
	NULL,					// The filename of the samples zip file (without extension, NULL if not applicable)
	"1986",
	"Tee'd Off (Japan)\0",	// Full Name A
	NULL,							// Comment A
	"Tehkan",						// Manufacturer A
	"Miscellaneous",				// System A
	NULL,					// Full Name W
	NULL,					// Comment W
	NULL,					// Manufacturer W
	NULL,					// System W
	// Flags
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_CLONE,
	2,						// Players: Max number of players a game supports (so we can remove single player games from netplay)
	HARDWARE_MISC_PRE90S, 	// Hardware: Which type of hardware the game runs on
	GBF_SPORTSMISC,		// Genre
	0,						// Family
	NULL,					// Function to get possible zip names
	TeedOffjRomInfo,		// Function to get the length and crc of each rom
	TeedOffjRomName,		// Function to get the possible names for each rom
	NULL,					// Function to get hdd info
	NULL,					// Function to get the possible names for each hdd
	NULL,					// Function to get the sample flags
	NULL,					// Function to get the possible names for each sample
	TehkanWCInputInfo,		// Function to get the input info for the game
	TeedOffDIPInfo,		// Function to get the input info for the game
	TehkanWCInit,				// Init
	TWCExit,				// Exit
	TWCFrame,				// Frame
	TWCDraw,				// Redraw
	TWCScan,				// Area Scan
	&TWCRecalc,				// Recalc Palettes: Set to 1 if the palette needs to be fully re-calculated
	0x300,					// Number of Palette Entries
	224,					// Screen width
	256,					// Screen height
	3,						// Screen x aspect
	4 						// Screen y aspect
};
