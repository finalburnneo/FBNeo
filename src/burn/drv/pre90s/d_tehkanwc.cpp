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


// Memory Modes
#define MEM_MODE_READ  0
#define MEM_MODE_WRITE 1
#define MEM_MODE_FETCH 2
#define MEM_MODE_ARG   3


static UINT8 p1_button           = 0;
static UINT8 p2_button           = 0;
static UINT8 system_inputs[8]    = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvFakeInputPort[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvDip[3]           = {0, 0, 0};
static UINT8 DrvInputP1But       = 0;
static UINT8 DrvInputP2But       = 0;
static UINT8 DrvInputSystem      = 0;
static INT16 track_p1[2];
static INT16 track_p2[2];
static INT32 track_reset_p1[2]   = {0, 0};
static INT32 track_reset_p2[2]   = {0, 0};
static UINT8 DrvReset            = 0;
static UINT8 m_digits[2];
static HoldCoin<2> hold_coin;
static INT32 nExtraCycles[3];

static UINT8 *AllMem             = NULL;
static UINT8 *MemEnd             = NULL;
static UINT8 *RAMStart           = NULL;
static UINT8 *RAMEnd             = NULL;
static UINT8 *DrvZ80MainROM      = NULL;
static UINT8 *DrvZ80SubROM       = NULL;
static UINT8 *DrvZ80SndROM       = NULL;
static UINT8 *DrvSndDataROM      = NULL;
static UINT8 *DrvZ80MainRAM      = NULL;
static UINT8 *DrvZ80SubRAM       = NULL;
static UINT8 *DrvZ80SndRAM       = NULL;
static UINT8 *DrvVidFgCodesRAM   = NULL;
static UINT8 *DrvVidBgRAM        = NULL;
static UINT8 *DrvVidFgAttrRAM    = NULL;
static UINT8 *DrvVidSpriteRAM    = NULL;
static UINT8 *DrvVidPalRAM       = NULL;
static UINT8 *DrvVidUnusedPalRAM = NULL;
static UINT8 *DrvSharedRAM       = NULL;
static UINT32 *DrvPalette        = NULL;
static UINT8 DrvPalRecalc;
static UINT8 *DrvVidFgROM        = NULL;
static UINT8 *DrvVidBgROM        = NULL;
static UINT8 *DrvVidSpriteROM    = NULL;
static UINT8 *TWCTempGfx         = NULL;

static UINT8 DrvScrollXHi        = 0;
static UINT8 DrvScrollXLo        = 0;
static UINT8 DrvScrollY          = 0;

static UINT8 DrvSoundLatch       = 0;
static UINT8 DrvSoundLatch2      = 0;
static int   msm_data_offs       = 0;
static INT32 msm_toggle          = 0;

static UINT8 DrvFlipScreenX      = 0;
static UINT8 DrvFlipScreenY      = 0;

static INT32 isGridiron          = 0;
static INT32 isTeedoff           = 0;

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo TehkanWCInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,		system_inputs + 0,	    "p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,		system_inputs + 2,	    "p1 start"	},
	{"P1 Button",   	BIT_DIGITAL,		&p1_button,	            "p1 fire 1"	},
	A("P1 Trackball X", BIT_ANALOG_REL,     &track_p1[0],		    "p1 x-axis" ),
	A("P1 Trackball Y", BIT_ANALOG_REL,     &track_p1[1],		    "p1 y-axis" ),
	{"P1 Right",	    BIT_DIGITAL,	    DrvFakeInputPort + 0,	"p1 right"	},
	{"P1 Left",	        BIT_DIGITAL,	    DrvFakeInputPort + 1,	"p1 left"	},
	{"P1 Down",	        BIT_DIGITAL,	    DrvFakeInputPort + 2,	"p1 down"	},
	{"P1 Up",	        BIT_DIGITAL,	    DrvFakeInputPort + 3,	"p1 up"		},

	{"P2 Coin",			BIT_DIGITAL,		system_inputs + 1,	    "p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,		system_inputs + 3,	    "p2 start"	},
	{"P2 Button",		BIT_DIGITAL,		&p2_button,	            "p2 fire 1"	},
	A("P2 Trackball X", BIT_ANALOG_REL,     &track_p2[0],           "p2 x-axis" ),
	A("P2 Trackball Y", BIT_ANALOG_REL,     &track_p2[1],           "p2 y-axis" ),
	{"P2 Right",	    BIT_DIGITAL,	    DrvFakeInputPort + 4,	"p2 right"	},
	{"P2 Left",	        BIT_DIGITAL,	    DrvFakeInputPort + 5,	"p2 left"	},
	{"P2 Down",	        BIT_DIGITAL,	    DrvFakeInputPort + 6,	"p2 down"	},
	{"P2 UP",	        BIT_DIGITAL,	    DrvFakeInputPort + 7,	"p2 up"		},

	{"Reset",			BIT_DIGITAL,		&DrvReset,	            "reset"		},
	{"Dip A",			BIT_DIPSWITCH,		DrvDip + 0,             "dip"		},
	{"Dip B",			BIT_DIPSWITCH,		DrvDip + 1,             "dip"		},
	{"Dip C",			BIT_DIPSWITCH,		DrvDip + 2,             "dip"		},
};
#undef A
STDINPUTINFO(TehkanWC)

inline static void DrvMakeInputs()
{
	DrvInputSystem = 0xff;
	DrvInputP1But = DrvInputP2But = 0;

	DrvInputP1But |= p1_button << 5;
	DrvInputP2But |= p2_button << 5;

	for (INT32 i = 0; i < 8; i++) {
		DrvInputSystem ^= (system_inputs[i] & 1) << i;
	}

	hold_coin.checklow(0, DrvInputSystem, 1<<0, 2);
	hold_coin.checklow(1, DrvInputSystem, 1<<1, 2);

	// device, portA_reverse?, portB_reverse?
	BurnTrackballConfig(0, AXIS_NORMAL, AXIS_NORMAL);
	// From MAME: port_sensitivity=100, port_keydelta=63
	// From AdvanceMame: fake keyboard input yields a |velocity| of 0x3f (0x7f / 2)
	//                   In absence of keyboard input, velocity must be 0x80, the rest value
	// track_pi = {0x0, 0x0} && trac_reset_pi = {0x80, 0x80} implies no movement

	const INT32 TB_SCALE[2] = { 6, 4 };

	BurnTrackballFrame(0, track_p1[0]*TB_SCALE[isTeedoff], track_p1[1]*TB_SCALE[isTeedoff], 0x2, 0x3f);  // 0x3f taken from advancemame driver
	BurnTrackballUDLR(0, DrvFakeInputPort[3], DrvFakeInputPort[2], DrvFakeInputPort[1], DrvFakeInputPort[0], 0x3f);
	BurnTrackballUpdate(0);

	BurnTrackballConfig(1, AXIS_REVERSED, AXIS_REVERSED);
	BurnTrackballFrame(1, track_p2[0]*TB_SCALE[isTeedoff], track_p2[1]*TB_SCALE[isTeedoff], 0x2, 0x3f);
	BurnTrackballUDLR(1, DrvFakeInputPort[7], DrvFakeInputPort[6], DrvFakeInputPort[5], DrvFakeInputPort[4], 0x3f);
	BurnTrackballUpdate(1);
}

static struct BurnDIPInfo TehkanWCDIPList[]=
{
	DIP_OFFSET(0x13)   // starting offset

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
	DIP_OFFSET(0x13)   // starting offset

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
	DIP_OFFSET(0x13)   // starting offset

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
	DIP_OFFSET(0x13)   // starting offset

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

static void __fastcall DrvSndWritePort(UINT16 port, UINT8 data)
{
	UINT8 offset = port & 0xff;

	switch (offset)
	{
		// MAME code uses data_address_w, which negates the port
		case 0x00:
		case 0x01:
			AY8910Write(0, ~offset & 1, data);
			return;

		case 0x02:
		case 0x03:
			AY8910Write(1, ~offset & 1, data);
			return;
	}
}

static UINT8 __fastcall DrvSndReadPort(UINT16 port)
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
	UINT8 *Next; Next = AllMem;;

	DrvZ80MainROM         = Next; Next += 0x0c000;
	DrvZ80SubROM          = Next; Next += 0x08000;
	DrvZ80SndROM          = Next; Next += 0x04000;
	DrvSndDataROM         = Next; Next += 0x08000;

	// Char Layout: 512 chars, 8x8 pixels, 4 bits/pixel
	DrvVidFgROM           = Next; Next += (512 * 8 * 8 * 4);
	// Tile Layout: 1024 tiles, 16x8 pixels, 4 bits/pixel
	DrvVidBgROM           = Next; Next += (1024 * 16 * 8 * 4);
	// Sprite Layout: 512 sprites, 16x16 pixels, 4 bits/pixel
	DrvVidSpriteROM       = Next; Next += (512 * 16 * 16 * 4);

	// Palette format: xBGR_444 (xxxxBBBBGGGGRRRR), 768
	DrvPalette            = (UINT32*)Next; Next += 0x00300 * sizeof(UINT32);

	RAMStart              = Next;

	DrvZ80MainRAM         = Next; Next += 0x00800;
	DrvZ80SubRAM          = Next; Next += 0x04800;
	DrvZ80SndRAM          = Next; Next += 0x00800;

	DrvSharedRAM          = Next; Next += 0x00800;

	DrvVidFgCodesRAM      = Next; Next += 0x00400;
	DrvVidFgAttrRAM       = Next; Next += 0x00400;
	DrvVidPalRAM          = Next; Next += 0x00600;
	DrvVidUnusedPalRAM    = Next; Next += 0x00200;
	DrvVidBgRAM           = Next; Next += 0x00800;
	DrvVidSpriteRAM       = Next; Next += 0x00400;

	RAMEnd                = Next;
	MemEnd                = Next;

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

static UINT8 __fastcall DrvMainRead(UINT16 address)
{
	if (address == 0xda00)
		return 0x80; // teedoff_unk_r

	if (address >= 0xd800 && address <= 0xddff) {
		return DrvVidPalRAM[address & 0x5ff];
	}

	switch (address) {

		case 0xf800:
		case 0xf801:
			return track_p1_r(address & 1);

		case 0xf802:
		case 0xf806:
			return DrvInputSystem;  // System

		case 0xf810:
		case 0xf811:
		return track_p2_r(address & 1);

		case 0xf803:
			return 0xff - DrvInputP1But;  // Player 1. DSW4 in test mode (tehkanwcd)

		case 0xf813:
			return 0xff - DrvInputP2But;  // Player 2. DSW5 in test mode (tehkanwcd)

		case 0xf820:
			return DrvSoundLatch2;

		case 0xf840:
			return DrvDip[1];	// DSW2

		case 0xf860:
			BurnWatchdogReset();
			return 0;

		case 0xf850:
			return DrvDip[2];	// DSW3

		case 0xf870:
			return DrvDip[0];	//DSW1
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


static void __fastcall DrvMainWrite(UINT16 address, UINT8 data)
{
	// videoram
	if (address >= 0xd000 && address <= 0xd3ff) {
		DrvVidFgCodesRAM[address & 0x3ff] = data;
		return;
	}

	// colorram
	if (address >= 0xd400 && address <= 0xd7ff) {
		DrvVidFgAttrRAM[address & 0x3ff] = data;
		return;
	}

	// videoram2
	if (address >= 0xe000 && address <= 0xe7ff) {
		DrvVidBgRAM[address & 0x7ff] = data;
		return;
	}

	switch (address) {

		case 0xec00:
			DrvScrollXLo = data;
			return;

		case 0xec01:
			DrvScrollXHi = data;
			return;

		case 0xec02:
			DrvScrollY = data;
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
			DrvSoundLatch = data;
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
			DrvFlipScreenX = data & 0x40;
			return;

		case 0xf870:
			DrvFlipScreenY = data & 0x40;
			return;
	}
}

static UINT8 __fastcall DrvSubRead(UINT16 address)
{
	switch (address) {
		case 0xf860:
			BurnWatchdogReset();
			return 0;
	}

	return 0;
}

static void __fastcall DrvSubWrite(UINT16 address, UINT8 data)
{
	// videoram
	if (address >= 0xd000 && address <= 0xd3ff) {
		DrvVidFgCodesRAM[address & 0x3ff] = data;
		return;
	}

	// colorram
	if (address >= 0xd400 && address <= 0xd7ff) {
		DrvVidFgAttrRAM[address & 0x3ff] = data;
		return;
	}

	// videoram2
	if (address >= 0xe000 && address <= 0xe7ff) {
		DrvVidBgRAM[address & 0x7ff] = data;
		return;
	}

	switch (address) {
		case 0xec00:
			DrvScrollXLo = data;
			return;

		case 0xec01:
			DrvScrollXHi = data;
			return;

		case 0xec02:
			DrvScrollY = data;
			return;
	}
}

static UINT8 __fastcall DrvSndRead(UINT16 address)
{
	switch (address) {
		case 0xc000:
			return DrvSoundLatch;
	}

	return 0;
}

static void __fastcall DrvSndWrite(UINT16 address, UINT8 data)
{
	switch (address) {

		case 0x8001:
			MSM5205ResetWrite(0, data ? 0 : 1);
			return;

		case 0x8002:
		case 0x8003:
			return;  // nopw

		case 0xc000:
			DrvSoundLatch2 = data;
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

static INT32 DrvCalcPalette()
{
	INT32 i;

	for (i = 0; i < 0x600; i++) {
		DrvPalette[i / 2] = xBGR_444_CalcCol(DrvVidPalRAM[i | 1] | (DrvVidPalRAM[i & ~1] << 8));
	}

	return 0;
}

static tilemap_callback( twc_bg )
{
	offs *= 2; // ??

	INT32 attr  = DrvVidBgRAM[offs + 1];
	INT32 code  = DrvVidBgRAM[offs] + ((attr & 0x30) << 4);
	INT32 color = attr & 0x0f;
	INT32 flags = ((attr & 0x40) ? TILE_FLIPX : 0) | ((attr & 0x80) ? TILE_FLIPY : 0);

	TILE_SET_INFO(0, code, color, flags);
}

static tilemap_callback( twc_fg )
{
	INT32 attr  = DrvVidFgAttrRAM[offs];
	INT32 code  = DrvVidFgCodesRAM[offs] + ((attr & 0x10) << 4);
	INT32 color = attr & 0x0f;
	INT32 flags = ((attr & 0x40) ? TILE_FLIPX : 0) | ((attr & 0x80) ? TILE_FLIPY : 0) | TILE_GROUP((attr >> 5) & 1);

	TILE_SET_INFO(1, code, color, flags);
}

static void DrvRenderSprites()
{
	INT32 Code, Attr, Color, fx, fy, sx, sy;

	for (INT32 Offs = 0; Offs < 0x400; Offs += 4)
	{
		Attr = DrvVidSpriteRAM[Offs + 1];
		Code = DrvVidSpriteRAM[Offs] + ((Attr & 0x08) << 5);
		Color = Attr & 0x07;
		fx = Attr & 0x40;
		fy = Attr & 0x80;
		sx = DrvVidSpriteRAM[Offs + 2] + ((Attr & 0x20) << 3) - 128;
		sy = DrvVidSpriteRAM[Offs + 3];

		if (DrvFlipScreenX)
		{
			sx = 240 - sx;
			fx = !fx;
		}

		if (DrvFlipScreenY)
		{
			sy = 240 - sy;
			fy = !fy;
		}

		Draw16x16MaskTile(pTransDraw, Code, sx, sy - SCREEN_VBEND, fx, fy, Color, 4, 0, 256, DrvVidSpriteROM);
	}
}

static INT32 DrvDraw()
{
	if (DrvPalRecalc) {
		DrvCalcPalette();
		DrvPalRecalc = 1;
	}

	GenericTilemapSetFlip(TMAP_GLOBAL, DrvFlipScreenX * TMAP_FLIPX | DrvFlipScreenY * TMAP_FLIPY);

	GenericTilemapSetScrollY(0, DrvScrollY);
	GenericTilemapSetScrollX(0, DrvScrollXLo + 256 * DrvScrollXHi);

	BurnTransferClear();

	if (nBurnLayer & 2) GenericTilemapDraw(0, pTransDraw, 0);
	if (nBurnLayer & 4) GenericTilemapDraw(1, pTransDraw, TMAP_SET_GROUP(1));
	if (nSpriteEnable & 1) DrvRenderSprites();
	if (nBurnLayer & 8) GenericTilemapDraw(1, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset(RAMStart, 0, RAMEnd - RAMStart);
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

	DrvScrollXHi   = 0;
	DrvScrollXLo   = 0;
	DrvScrollY     = 0;
	DrvFlipScreenX = 0;
	DrvFlipScreenY = 0;

	DrvSoundLatch  = 0;
	DrvSoundLatch2 = 0;

	msm_data_offs  = 0;
	msm_toggle     = 0;

	m_digits[0] = 0;
	m_digits[1] = 0;

	track_reset_p1[0] = 0;
	track_reset_p1[1] = 0;
	track_reset_p2[0] = 0;
	track_reset_p2[1] = 0;

	memset(nExtraCycles, 0, sizeof(nExtraCycles));

	HiscoreReset();

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) DrvDoReset(1); // Handle reset

	// Number of interrupt slices per frame
	INT32 nInterleave = MSM5205CalcInterleave(0, SOUND_CPU_CLOCK);

	DrvMakeInputs(); // Update inputs

	ZetNewFrame(); // Reset CPU cycle counters

	// Total cycles each CPU should run per frame
	INT32 nCyclesTotal[3] = { ((double)MAIN_CPU_CLOCK * 100 / nBurnFPS), ((double)SUB_CPU_CLOCK * 100 / nBurnFPS), ((double)SOUND_CPU_CLOCK * 100 / nBurnFPS) };

	// Cycles executed so far
	INT32 nCyclesDone[3] = { nExtraCycles[0], nExtraCycles[1], nExtraCycles[2] };

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

	// Let the next frame know if too many cycles were run
	nExtraCycles[0] = nCyclesDone[0] - nCyclesTotal[0];
	nExtraCycles[1] = nCyclesDone[1] - nCyclesTotal[1];
	nExtraCycles[2] = nCyclesDone[2] - nCyclesTotal[2];

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
	UINT8 msm_data = DrvSndDataROM[msm_data_offs & 0x7fff];

	if (msm_toggle == 0)
		MSM5205DataWrite(0, (msm_data >> 4) & 0x0f);
	else
	{
		MSM5205DataWrite(0, msm_data & 0x0f);
		msm_data_offs++;
	}

	msm_toggle ^= 1;
}

static INT32 DrvInit()
{
	// Set refresh rate
	BurnSetRefreshRate(60.606061);

	INT32 nRet = 0;

	// --------------------- Memory Init
	BurnAllocMemIndex();


	// ----------------------------- ROMs Loading
	TWCTempGfx = (UINT8*)BurnMalloc(0x10000);
	if (TWCTempGfx == NULL) return 1;

	nRet = BurnLoadRom(DrvZ80MainROM + 0x00000,  0,  1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvZ80MainROM + 0x04000,  1,  1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvZ80MainROM + 0x08000,  2,  1); if (nRet != 0) return 1;

	nRet = BurnLoadRom(DrvZ80SubROM + 0x00000,  3,  1); if (nRet != 0) return 1;

	nRet = BurnLoadRom(DrvZ80SndROM + 0x00000,  4,  1); if (nRet != 0) return 1;

	memset(TWCTempGfx, 0, 0x4000);
	nRet = BurnLoadRom(TWCTempGfx + 0x00000,  5, 1); if (nRet != 0) return 1;
	GfxDecode(512, 4, 8, 8, CharPlaneOffsets, CharXOffsets, CharYOffsets, 0x100, TWCTempGfx, DrvVidFgROM);

	memset(TWCTempGfx, 0, 0x10000);

	if (isGridiron == 0) {
		nRet = BurnLoadRom(TWCTempGfx + 0x00000,  6, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(TWCTempGfx + 0x08000,  7, 1); if (nRet != 0) return 1;
		GfxDecode(512, 4, 16, 16, SpritePlaneOffsets, SpriteXOffsets, SpriteYOffsets, 0x400, TWCTempGfx, DrvVidSpriteROM);

		memset(TWCTempGfx, 0, 0x10000);
		nRet = BurnLoadRom(TWCTempGfx + 0x00000,  8, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(TWCTempGfx + 0x08000,  9, 1); if (nRet != 0) return 1;
		GfxDecode(1024, 4, 16, 8, TilePlaneOffsets, TileXOffsets, TileYOffsets, 0x200, TWCTempGfx, DrvVidBgROM);

		BurnFree(TWCTempGfx);

		nRet = BurnLoadRom(DrvSndDataROM + 0x00000, 10, 1); if (nRet !=0) return 1;
	}
	else {
		nRet = BurnLoadRom(TWCTempGfx + 0x00000,  6, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(TWCTempGfx + 0x04000,  7, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(TWCTempGfx + 0x08000,  8, 1); if (nRet != 0) return 1;
		GfxDecode(512, 4, 16, 16, SpritePlaneOffsets, SpriteXOffsets, SpriteYOffsets, 0x400, TWCTempGfx, DrvVidSpriteROM);

		memset(TWCTempGfx, 0, 0x10000);
		nRet = BurnLoadRom(TWCTempGfx + 0x00000,  9, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(TWCTempGfx + 0x04000,  10, 1); if (nRet != 0) return 1;
		GfxDecode(1024, 4, 16, 8, TilePlaneOffsets, TileXOffsets, TileYOffsets, 0x200, TWCTempGfx, DrvVidBgROM);

		BurnFree(TWCTempGfx);

		nRet = BurnLoadRom(DrvSndDataROM, 11, 1); if (nRet !=0) return 1;	
	}


	// --------------------- CPUS Init
	// Main CPU
	ZetInit(CPU_MAIN);
	ZetOpen(CPU_MAIN);

	// ROM
	ZetMapMemory(DrvZ80MainROM, 0x0000, 0xbfff, MAP_ROM);

	// RAM
	ZetMapMemory(DrvZ80MainRAM, 0xc000, 0xc7ff, MAP_RAM);

	// Shared Memory
	// shareram
	ZetMapMemory(DrvSharedRAM, 0xc800, 0xcfff, MAP_RAM);

	// videoram
	ZetMapArea(0xd000, 0xd3ff, MEM_MODE_READ, DrvVidFgCodesRAM);
	ZetMapArea(0xd000, 0xd3ff, MEM_MODE_FETCH, DrvVidFgCodesRAM);

	// colorram
	ZetMapArea(0xd400, 0xd7ff, MEM_MODE_READ, DrvVidFgAttrRAM);
	ZetMapArea(0xd400, 0xd7ff, MEM_MODE_FETCH, DrvVidFgAttrRAM);

	// palette
	ZetMapArea(0xd800, 0xddff, MEM_MODE_WRITE, DrvVidPalRAM);
	ZetMapArea(0xd800, 0xddff, MEM_MODE_FETCH, DrvVidPalRAM);

	// palette2
	ZetMapMemory(DrvVidUnusedPalRAM, 0xde00, 0xdfff, MAP_RAM);

	// videoram2
	ZetMapArea(0xe000, 0xe7ff, MEM_MODE_READ, DrvVidBgRAM);
	ZetMapArea(0xe000, 0xe7ff, MEM_MODE_FETCH, DrvVidBgRAM);

	// spriteram
	ZetMapMemory(DrvVidSpriteRAM, 0xe800, 0xebff, MAP_RAM);

	ZetSetReadHandler(DrvMainRead);
	ZetSetWriteHandler(DrvMainWrite);
	ZetClose();


	// Graphics "sub" CPU
	ZetInit(CPU_SUB);
	ZetOpen(CPU_SUB);

	// ROM
	ZetMapMemory(DrvZ80SubROM, 0x0000, 0x7fff, MAP_ROM);

	// RAM
	ZetMapMemory(DrvZ80SubRAM, 0x8000, 0xc7ff, MAP_RAM);

	// Shared Memory
	// shareram
	ZetMapMemory(DrvSharedRAM, 0xc800, 0xcfff, MAP_RAM);

	// videoram
	ZetMapArea(0xd000, 0xd3ff, MEM_MODE_READ, DrvVidFgCodesRAM);
	ZetMapArea(0xd000, 0xd3ff, MEM_MODE_FETCH, DrvVidFgCodesRAM);

	// colorram
	ZetMapArea(0xd400, 0xd7ff, MEM_MODE_READ, DrvVidFgAttrRAM);
	ZetMapArea(0xd400, 0xd7ff, MEM_MODE_FETCH, DrvVidFgAttrRAM);

	// palette
	ZetMapMemory(DrvVidPalRAM, 0xd800, 0xddff, MAP_RAM);

	// palette2
	ZetMapMemory(DrvVidUnusedPalRAM, 0xde00, 0xdfff, MAP_RAM);

	// videoram2
	ZetMapArea(0xe000, 0xe7ff, MEM_MODE_READ, DrvVidBgRAM);
	ZetMapArea(0xe000, 0xe7ff, MEM_MODE_FETCH, DrvVidBgRAM);

	// spriteram
	ZetMapMemory(DrvVidSpriteRAM, 0xe800, 0xebff, MAP_RAM);

	ZetSetReadHandler(DrvSubRead);
	ZetSetWriteHandler(DrvSubWrite);
	ZetClose();


	// Sound CPU
	ZetInit(CPU_SOUND);
	ZetOpen(CPU_SOUND);

	// ROM
	ZetMapMemory(DrvZ80SndROM, 0x0000, 0x3fff, MAP_ROM);

	// RAM
	ZetMapMemory(DrvZ80SndRAM, 0x4000, 0x47ff, MAP_RAM);

	ZetSetReadHandler(DrvSndRead);
	ZetSetWriteHandler(DrvSndWrite);
	ZetSetOutHandler(DrvSndWritePort);
	ZetSetInHandler(DrvSndReadPort);
	ZetClose();


	// --------------------------------- Initialize sound chips
	AY8910Init(0, AY_CLOCK, 0);
	AY8910Init(1, AY_CLOCK, 1);
	AY8910SetPorts(0, NULL, NULL, &portA_w, &portB_w);
	AY8910SetPorts(1, &portA_r, &portB_r, NULL, NULL);
	AY8910SetAllRoutes(0, 0.25, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.25, BURN_SND_ROUTE_BOTH);
	AY8910SetBuffered(ZetTotalCycles, SOUND_CPU_CLOCK);

	MSM5205Init(0, DrvSynchroniseStream, MSM5205_CLOCK, adpcm_int, MSM5205_S48_4B, 1);
	MSM5205SetRoute(0, 0.45, BURN_SND_ROUTE_BOTH);


	// ----------------------------------- Initialize Background and Foreground
	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, twc_bg_map_callback, 16,  8, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, twc_fg_map_callback,  8,  8, 32, 32);
	GenericTilemapSetGfx(0, DrvVidBgROM, 4, 16,  8, 0x80000, 0x200, 0xf);
	GenericTilemapSetGfx(1, DrvVidFgROM, 4,  8,  8, 0x20000, 0x000, 0xf);
	GenericTilemapSetOffsets(TMAP_GLOBAL,  0, -16);
	GenericTilemapSetTransparent(1, 0);


	BurnWatchdogInit(DrvDoReset, 180);

	// Initialize analog controls for player 1 and player 2
	BurnTrackballInit(2);

	DrvDoReset(1);

	return 0;
}

static INT32 GridironInit()
{
	INT32 nRet = 0;
	isGridiron = 1;

	nRet = DrvInit();

	return nRet;
}

static INT32 TeedOffInit()
{
	INT32 nRet = 0;
	isTeedoff = 1;

	nRet = DrvInit();

	return nRet;
}

static INT32 DrvExit()
{
	ZetExit();
	GenericTilesExit();
	AY8910Exit(0);
	AY8910Exit(1);
	MSM5205Exit();
	BurnTrackballExit();

	BurnFreeMemIndex();

	DrvScrollXHi   = 0;
	DrvScrollXLo   = 0;
	DrvScrollY     = 0;
	DrvSoundLatch  = 0;
	DrvSoundLatch2 = 0;
	msm_data_offs  = 0;
	msm_toggle     = 0;
	DrvFlipScreenX = 0;
	DrvFlipScreenY = 0;
	isGridiron     = 0;
	isTeedoff      = 0;

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin != NULL) {
		*pnMin = 0x029721;  // Copied from d_wc90.cpp!!
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = RAMStart;
		ba.nLen	  = RAMEnd - RAMStart;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		ZetScan(nAction);
		AY8910Scan(nAction, pnMin);
		MSM5205Scan(nAction, pnMin);

		SCAN_VAR(DrvScrollXLo);
		SCAN_VAR(DrvScrollXLo);
		SCAN_VAR(DrvScrollY);
		SCAN_VAR(DrvSoundLatch);
		SCAN_VAR(DrvSoundLatch2);
		SCAN_VAR(DrvFlipScreenX);
		SCAN_VAR(DrvFlipScreenY);
		SCAN_VAR(msm_data_offs);
		SCAN_VAR(msm_toggle);
		SCAN_VAR(m_digits);
		SCAN_VAR(track_reset_p1);
		SCAN_VAR(track_reset_p2);

		SCAN_VAR(nExtraCycles);

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
	DrvInit,				// Init
	DrvExit,				// Exit
	DrvFrame,				// Frame
	DrvDraw,				// Redraw
	DrvScan,				// Area Scan
	&DrvPalRecalc,				// Recalc Palettes: Set to 1 if the palette needs to be fully re-calculated
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
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED | BDF_CLONE,
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
	DrvInit,				// Init
	DrvExit,				// Exit
	DrvFrame,				// Frame
	DrvDraw,				// Redraw
	DrvScan,				// Area Scan
	&DrvPalRecalc,			// Recalc Palettes: Set to 1 if the palette needs to be fully re-calculated
	0x300,					// Number of Palette Entries
	256,					// Screen width
	224,					// Screen height
	4,						// Screen x aspect
	3 						// Screen y aspect
};
struct BurnDriver BurnDrvTehkanWCc = {
	"tehkanwcc",			// The filename of the zip file (without extension)
	"tehkanwc",				// The filename of the parent (without extension, NULL if not applicable)
	NULL,					// The filename of the board ROMs (without extension, NULL if not applicable)
	NULL,					// The filename of the samples zip file (without extension, NULL if not applicable)
	"1985",
	"Tehkan World Cup (set 3, bootleg)\0",	// Full Name A
	NULL,					// Comment A
	"bootleg",				// Manufacturer A
	"Miscellaneous",		// System A
	NULL,					// Full Name W
	NULL,					// Comment W
	NULL,					// Manufacturer W
	NULL,					// System W
	// Flags
	BDF_GAME_NOT_WORKING | BDF_HISCORE_SUPPORTED | BDF_CLONE,
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
	DrvInit,				// Init
	DrvExit,				// Exit
	DrvFrame,				// Frame
	DrvDraw,				// Redraw
	DrvScan,				// Area Scan
	&DrvPalRecalc,			// Recalc Palettes: Set to 1 if the palette needs to be fully re-calculated
	0x300,					// Number of Palette Entries
	256,					// Screen width
	224,					// Screen height
	4,						// Screen x aspect
	3 						// Screen y aspect
};
struct BurnDriver BurnDrvTehkanWCd = {
	"tehkanwcd",			// The filename of the zip file (without extension)
	"tehkanwc",				// The filename of the parent (without extension, NULL if not applicable)
	NULL,					// The filename of the board ROMs (without extension, NULL if not applicable)
	NULL,					// The filename of the samples zip file (without extension, NULL if not applicable)
	"1985",
	"Tehkan World Cup (set 4, earlier)\0",	// Full Name A
	NULL,					// Comment A
	"Tehkan",				// Manufacturer A
	"Miscellaneous",		// System A
	NULL,					// Full Name W
	NULL,					// Comment W
	NULL,					// Manufacturer W
	NULL,					// System W
	// Flags
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED | BDF_CLONE,
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
	DrvInit,				// Init
	DrvExit,				// Exit
	DrvFrame,				// Frame
	DrvDraw,				// Redraw
	DrvScan,				// Area Scan
	&DrvPalRecalc,			// Recalc Palettes: Set to 1 if the palette needs to be fully re-calculated
	0x300,					// Number of Palette Entries
	256,					// Screen width
	224,					// Screen height
	4,						// Screen x aspect
	3 						// Screen y aspect
};

struct BurnDriver BurnDrvTehkanWCh = {
	"tehkanwch",			// The filename of the zip file (without extension)
	"tehkanwc",				// The filename of the parent (without extension, NULL if not applicable)
	NULL,					// The filename of the board ROMs (without extension, NULL if not applicable)
	NULL,					// The filename of the samples zip file (without extension, NULL if not applicable)
	"1986",
	"Tehkan World Cup (1986 year hack)\0",	// Full Name A
	NULL,					// Comment A
	"hack",					// Manufacturer A
	"Miscellaneous",		// System A
	NULL,					// Full Name W
	NULL,					// Comment W
	NULL,					// Manufacturer W
	NULL,					// System W
	// Flags
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED | BDF_CLONE,
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
	DrvInit,				// Init
	DrvExit,				// Exit
	DrvFrame,				// Frame
	DrvDraw,				// Redraw
	DrvScan,				// Area Scan
	&DrvPalRecalc,			// Recalc Palettes: Set to 1 if the palette needs to be fully re-calculated
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
	"Gridiron Fight\0",		// Full Name A
	NULL,					// Comment A
	"Tehkan",				// Manufacturer A
	"Miscellaneous",		// System A
	NULL,					// Full Name W
	NULL,					// Comment W
	NULL,					// Manufacturer W
	NULL,					// System W
	// Flags
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED,
	2,						// Players: Max number of players a game supports (so we can remove single player games from netplay)
	HARDWARE_MISC_PRE90S, 	// Hardware: Which type of hardware the game runs on
	GBF_SPORTSMISC,			// Genre
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
	GridironInit,			// Init
	DrvExit,				// Exit
	DrvFrame,				// Frame
	DrvDraw,				// Redraw
	DrvScan,				// Area Scan
	&DrvPalRecalc,			// Recalc Palettes: Set to 1 if the palette needs to be fully re-calculated
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
	NULL,					// Comment A
	"Tehkan",				// Manufacturer A
	"Miscellaneous",		// System A
	NULL,					// Full Name W
	NULL,					// Comment W
	NULL,					// Manufacturer W
	NULL,					// System W
	// Flags
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED,
	2,						// Players: Max number of players a game supports (so we can remove single player games from netplay)
	HARDWARE_MISC_PRE90S, 	// Hardware: Which type of hardware the game runs on
	GBF_SPORTSMISC,			// Genre
	0,						// Family
	NULL,					// Function to get possible zip names
	TeedOffRomInfo,			// Function to get the length and crc of each rom
	TeedOffRomName,			// Function to get the possible names for each rom
	NULL,					// Function to get hdd info
	NULL,					// Function to get the possible names for each hdd
	NULL,					// Function to get the sample flags
	NULL,					// Function to get the possible names for each sample
	TehkanWCInputInfo,		// Function to get the input info for the game
	TeedOffDIPInfo,			// Function to get the input info for the game
	TeedOffInit,			// Init
	DrvExit,				// Exit
	DrvFrame,				// Frame
	DrvDraw,				// Redraw
	DrvScan,				// Area Scan
	&DrvPalRecalc,			// Recalc Palettes: Set to 1 if the palette needs to be fully re-calculated
	0x300,					// Number of Palette Entries
	224,					// Screen width
	256,					// Screen height
	3,						// Screen x aspect
	4 						// Screen y aspect
};

struct BurnDriver BurnDrvTeedOffj = {
	"teedoffj",				// The filename of the zip file (without extension)
	"teedoff",				// The filename of the parent (without extension, NULL if not applicable)
	NULL,					// The filename of the board ROMs (without extension, NULL if not applicable)
	NULL,					// The filename of the samples zip file (without extension, NULL if not applicable)
	"1986",
	"Tee'd Off (Japan)\0",	// Full Name A
	NULL,					// Comment A
	"Tehkan",				// Manufacturer A
	"Miscellaneous",		// System A
	NULL,					// Full Name W
	NULL,					// Comment W
	NULL,					// Manufacturer W
	NULL,					// System W
	// Flags
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_CLONE,
	2,						// Players: Max number of players a game supports (so we can remove single player games from netplay)
	HARDWARE_MISC_PRE90S, 	// Hardware: Which type of hardware the game runs on
	GBF_SPORTSMISC,			// Genre
	0,						// Family
	NULL,					// Function to get possible zip names
	TeedOffjRomInfo,		// Function to get the length and crc of each rom
	TeedOffjRomName,		// Function to get the possible names for each rom
	NULL,					// Function to get hdd info
	NULL,					// Function to get the possible names for each hdd
	NULL,					// Function to get the sample flags
	NULL,					// Function to get the possible names for each sample
	TehkanWCInputInfo,		// Function to get the input info for the game
	TeedOffDIPInfo,			// Function to get the input info for the game
	DrvInit,				// Init
	DrvExit,				// Exit
	DrvFrame,				// Frame
	DrvDraw,				// Redraw
	DrvScan,				// Area Scan
	&DrvPalRecalc,			// Recalc Palettes: Set to 1 if the palette needs to be fully re-calculated
	0x300,					// Number of Palette Entries
	224,					// Screen width
	256,					// Screen height
	3,						// Screen x aspect
	4 						// Screen y aspect
};
