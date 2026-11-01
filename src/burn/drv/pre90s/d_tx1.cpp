// FinalBurn Neo Tatsumi TX-1 / Buggy Boy driver module
// Based on MAME driver by Philip Bennett

#include "tiles_generic.h"
#include "nec_intf.h"
#include "z80_intf.h"
#include "ay8910.h"
#include "8255ppi.h"
#include "resnet.h"
#include "burn_gun.h"
#include "burn_shift.h"
#include "bitswap.h"
#include "stream.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvMainROM;
static UINT8 *DrvMathROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvCharROM;
static UINT8 *DrvObjROM;
static UINT8 *DrvRoadROM;
static UINT8 *DrvAuData;
static UINT8 *DrvObjMap;
static UINT8 *DrvObjLuts;
static UINT8 *DrvProms;
static UINT8 *DrvMainRAM;
static UINT8 *DrvNVRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvRCRAM;
static UINT8 *DrvMathRAM;
static UINT8 *DrvObjRAM;
static UINT8 *DrvZ80RAM;
static UINT8 *chr_bmp;
static UINT8 *obj_bmp;
static UINT8 *rod_bmp;
static UINT16 *au_data16;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT16 *vram16;
static UINT16 *rcram16;
static UINT16 *objram16;

enum { GAME_TX1 = 0, GAME_BUGGYBOY, GAME_BUGGYBOYJR };
static INT32 game_type;
static INT32 is_buggyboy;
static INT32 screen_width;

static INT32 nExtraCycles[3];
static INT32 z80_irq_cycles;
static INT32 z80_irq_period;

struct math_t
{
	UINT16 cpulatch;
	UINT16 promaddr;
	UINT16 inslatch;
	UINT32 mux;
	UINT16 ppshift;
	UINT32 i0ff;
	UINT16 retval;
	UINT16 muxlatch;
};

struct sn74s516_t
{
	INT16 X;
	INT16 Y;
	INT32 ZW32;
	INT32 code;
	INT32 state;
	INT32 ZWfl;
};

struct vregs_t
{
	UINT16 scol;
	UINT32 slock;
	UINT8  flags;

	UINT32 ba_val;
	UINT32 ba_inc;
	UINT32 bank_mode;

	UINT16 h_val;
	UINT16 h_inc;
	UINT16 h_init;

	UINT8  slin_val;
	UINT8  slin_inc;

	UINT8  wa8;
	UINT8  wa4;

	UINT16 wave_lfsr;
	UINT8  sky;
	UINT16 gas;
	UINT8  shift;
};

static math_t math;
static sn74s516_t sn74s516;
static vregs_t vregs;

// sound
struct pit8253_state
{
	UINT16 counts[3];
	INT32 idx[3];
};

static pit8253_state pit8253;
static UINT8 ay_outputa;
static UINT8 ay_outputb;
static UINT8 ppi_latch_a;
static UINT8 ppi_latch_b;
static UINT32 snd_step0;
static UINT32 snd_step1;
static UINT32 snd_step2;
static INT32 snd_pit0;
static INT32 snd_pit1;
static INT32 snd_pit2;
static INT32 eng0[4];
static INT32 eng1[4];
static INT32 eng2[4];
static INT32 noise_lfsra;
static INT32 noise_lfsrb;
static INT32 noise_lfsrc;
static INT32 noise_lfsrd;
static INT32 noise_counter;
static UINT8 ym1_outputa;
static UINT8 ym2_outputa;
static UINT8 ym2_outputb;
static double snd_gain;		// custom sound output gain (tx1 only)
static INT32 dc_last_in[2];
static INT32 dc_last_out[2];

static double weights0[4];
static double weights1[3];
static double weights2[3];
static UINT16 eng_voltages[16];
static double freq_to_step;

static Stream stream;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[4];
static UINT8 DrvReset;
static INT16 Analog[3];

static UINT8 pedal_accu[2];
static UINT8 pedal_target[2];
static INT32 pedal_cycles;

static INT32 gear_state;

#define A(a, b, c, d) { a, b, (UINT8*)(c), d }
static struct BurnInputInfo Tx1InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},

	A("P1 Steering",	BIT_ANALOG_REL,	&Analog[0],		"p1 x-axis"	),
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	A("P1 Accelerator",	BIT_ANALOG_REL,	&Analog[1],		"p1 fire 1"	),
	A("P1 Brake",		BIT_ANALOG_REL,	&Analog[2],		"p1 fire 2"	),
	{"P1 Gear Change",	BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Tx1)

static struct BurnInputInfo BuggyboyInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},

	A("P1 Steering",	BIT_ANALOG_REL,	&Analog[0],		"p1 x-axis"	),
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	A("P1 Accelerator",	BIT_ANALOG_REL,	&Analog[1],		"p1 fire 1"	),
	A("P1 Brake",		BIT_ANALOG_REL,	&Analog[2],		"p1 fire 2"	),
	{"P1 Gear Change",	BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",			BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};
#undef A

STDINPUTINFO(Buggyboy)

static struct BurnDIPInfo Tx1DIPList[]=
{
	DIP_OFFSET(0x09)
	{0x00, 0xff, 0xff, 0x80, NULL								},
	{0x01, 0xff, 0xff, 0xf3, NULL								},
	{0x02, 0xff, 0xff, 0x00, NULL								},

	{0   , 0xfe, 0   ,    4, "Game Cost"						},
	{0x00, 0x01, 0x0c, 0x00, "1 Coin Unit for 1 Credit"			},
	{0x00, 0x01, 0x0c, 0x04, "2 Coin Units for 1 Credit"		},
	{0x00, 0x01, 0x0c, 0x08, "3 Coin Units for 1 Credit"		},
	{0x00, 0x01, 0x0c, 0x0c, "4 Coin Units for 1 Credit"		},

	{0   , 0xfe, 0   ,    2, "Left Coin Mechanism"				},
	{0x00, 0x01, 0x10, 0x00, "1 Coin for 1 Coin Unit"			},
	{0x00, 0x01, 0x10, 0x10, "1 Coin for 2 Coin Units"			},

	{0   , 0xfe, 0   ,    4, "Right Coin Mechanism"				},
	{0x00, 0x01, 0x60, 0x00, "1 Coin for 1 Coin Unit"			},
	{0x00, 0x01, 0x60, 0x20, "1 Coin for 4 Coin Units"			},
	{0x00, 0x01, 0x60, 0x40, "1 Coin for 5 Coin Units"			},
	{0x00, 0x01, 0x60, 0x60, "1 Coin for 6 Coin Units"			},

	{0   , 0xfe, 0   ,    2, "Unknown"							},
	{0x00, 0x01, 0x80, 0x80, "Off"								},
	{0x00, 0x01, 0x80, 0x00, "On"								},

	{0   , 0xfe, 0   ,    8, "Difficulty"						},
	{0x01, 0x01, 0x07, 0x00, "A (Easiest)"						},
	{0x01, 0x01, 0x07, 0x01, "B"								},
	{0x01, 0x01, 0x07, 0x02, "C"								},
	{0x01, 0x01, 0x07, 0x03, "D"								},
	{0x01, 0x01, 0x07, 0x04, "E"								},
	{0x01, 0x01, 0x07, 0x05, "F"								},
	{0x01, 0x01, 0x07, 0x06, "G"								},
	{0x01, 0x01, 0x07, 0x07, "H (Hardest)"						},

	{0   , 0xfe, 0   ,    4, "Game Time"						},
	{0x01, 0x01, 0x18, 0x00, "A (Longest)"						},
	{0x01, 0x01, 0x18, 0x08, "B"								},
	{0x01, 0x01, 0x18, 0x10, "C"								},
	{0x01, 0x01, 0x18, 0x18, "D (Shortest)"						},

	{0   , 0xfe, 0   ,    8, "Bonus Adder"						},
	{0x01, 0x01, 0xe0, 0x00, "No Bonus"							},
	{0x01, 0x01, 0xe0, 0x20, "2 Coin Units for 1 Credit"		},
	{0x01, 0x01, 0xe0, 0x40, "3 Coin Units for 1 Credit"		},
	{0x01, 0x01, 0xe0, 0x60, "4 Coin Units for 1 Credit"		},
	{0x01, 0x01, 0xe0, 0x80, "5 Coin Units for 1 Credit"		},
	{0x01, 0x01, 0xe0, 0xa0, "4 Coin Units for 2 Credit"		},
	{0x01, 0x01, 0xe0, 0xc0, "Free Play"						},
	{0x01, 0x01, 0xe0, 0xe0, "No Bonus"							},

	{0   , 0xfe, 0   ,    2, "Service Mode"						},
	{0x02, 0x01, 0x04, 0x00, "Off"								},
	{0x02, 0x01, 0x04, 0x04, "On"								},
};

STDDIPINFO(Tx1)

static struct BurnDIPInfo Tx1jDIPList[]=
{
	DIP_OFFSET(0x09)
	{0x00, 0xff, 0xff, 0x00, NULL								},
	{0x01, 0xff, 0xff, 0xf3, NULL								},
	{0x02, 0xff, 0xff, 0x00, NULL								},

	{0   , 0xfe, 0   ,    8, "Coin A"							},
	{0x00, 0x01, 0x1c, 0x08, "3 Coins 1 Credits"				},
	{0x00, 0x01, 0x1c, 0x04, "2 Coins 1 Credits"				},
	{0x00, 0x01, 0x1c, 0x00, "1 Coin  1 Credits"				},
	{0x00, 0x01, 0x1c, 0x1c, "2 Coins 3 Credits"				},
	{0x00, 0x01, 0x1c, 0x0c, "1 Coin  2 Credits"				},
	{0x00, 0x01, 0x1c, 0x10, "1 Coin  3 Credits"				},
	{0x00, 0x01, 0x1c, 0x14, "1 Coin  5 Credits"				},
	{0x00, 0x01, 0x1c, 0x18, "1 Coin  6 Credits"				},

	{0   , 0xfe, 0   ,    8, "Coin B"							},
	{0x00, 0x01, 0xe0, 0x40, "3 Coins 1 Credits"				},
	{0x00, 0x01, 0xe0, 0x20, "2 Coins 1 Credits"				},
	{0x00, 0x01, 0xe0, 0x00, "1 Coin  1 Credits"				},
	{0x00, 0x01, 0xe0, 0xe0, "2 Coins 3 Credits"				},
	{0x00, 0x01, 0xe0, 0x60, "1 Coin  2 Credits"				},
	{0x00, 0x01, 0xe0, 0x80, "1 Coin  3 Credits"				},
	{0x00, 0x01, 0xe0, 0xa0, "1 Coin  5 Credits"				},
	{0x00, 0x01, 0xe0, 0xc0, "1 Coin  6 Credits"				},

	{0   , 0xfe, 0   ,    8, "Difficulty"						},
	{0x01, 0x01, 0x07, 0x00, "A (Easiest)"						},
	{0x01, 0x01, 0x07, 0x01, "B"								},
	{0x01, 0x01, 0x07, 0x02, "C"								},
	{0x01, 0x01, 0x07, 0x03, "D"								},
	{0x01, 0x01, 0x07, 0x04, "E"								},
	{0x01, 0x01, 0x07, 0x05, "F"								},
	{0x01, 0x01, 0x07, 0x06, "G"								},
	{0x01, 0x01, 0x07, 0x07, "H (Hardest)"						},

	{0   , 0xfe, 0   ,    4, "Game Time"						},
	{0x01, 0x01, 0x18, 0x00, "A (Longest)"						},
	{0x01, 0x01, 0x18, 0x08, "B"								},
	{0x01, 0x01, 0x18, 0x10, "C"								},
	{0x01, 0x01, 0x18, 0x18, "D (Shortest)"						},

	{0   , 0xfe, 0   ,    8, "Unknown"							},
	{0x01, 0x01, 0xe0, 0x00, "0"								},
	{0x01, 0x01, 0xe0, 0x20, "1"								},
	{0x01, 0x01, 0xe0, 0x40, "2"								},
	{0x01, 0x01, 0xe0, 0x60, "3"								},
	{0x01, 0x01, 0xe0, 0x80, "4"								},
	{0x01, 0x01, 0xe0, 0xa0, "5"								},
	{0x01, 0x01, 0xe0, 0xc0, "6"								},
	{0x01, 0x01, 0xe0, 0xe0, "7"								},

	{0   , 0xfe, 0   ,    2, "Service Mode"						},
	{0x02, 0x01, 0x04, 0x00, "Off"								},
	{0x02, 0x01, 0x04, 0x04, "On"								},
};

STDDIPINFO(Tx1j)

#define BUGGYBOY_COMMON_DIPS										\
	{0   , 0xfe, 0   ,    4, "Do not change DSW2 1&2"			},	\
	{0x00, 0x01, 0x03, 0x00, "0"								},	\
	{0x00, 0x01, 0x03, 0x01, "1"								},	\
	{0x00, 0x01, 0x03, 0x02, "2"								},	\
	{0x00, 0x01, 0x03, 0x03, "3"								},	\
																	\
	{0   , 0xfe, 0   ,    2, "Language"							},	\
	{0x00, 0x01, 0x04, 0x04, "English"							},	\
	{0x00, 0x01, 0x04, 0x00, "Japanese"							},	\
																	\
	{0   , 0xfe, 0   ,    2, "Do not Change DSW2 4"				},	\
	{0x00, 0x01, 0x08, 0x00, "Off"								},	\
	{0x00, 0x01, 0x08, 0x08, "On"								},	\
																	\
	{0   , 0xfe, 0   ,    4, "Time Rank"						},	\
	{0x00, 0x01, 0x30, 0x00, "A (Longest)"						},	\
	{0x00, 0x01, 0x30, 0x10, "B"								},	\
	{0x00, 0x01, 0x30, 0x20, "C"								},	\
	{0x00, 0x01, 0x30, 0x30, "D (Shortest)"						},	\
																	\
	{0   , 0xfe, 0   ,    4, "Game Rank"						},	\
	{0x00, 0x01, 0xc0, 0x00, "A (Easy)"							},	\
	{0x00, 0x01, 0xc0, 0x40, "B"								},	\
	{0x00, 0x01, 0xc0, 0x80, "C"								},	\
	{0x00, 0x01, 0xc0, 0xc0, "D (Difficult)"					},	\
																	\
	{0   , 0xfe, 0   ,    8, "Coin A"							},	\
	{0x01, 0x01, 0xe0, 0x40, "3 Coins 1 Credits"				},	\
	{0x01, 0x01, 0xe0, 0x20, "2 Coins 1 Credits"				},	\
	{0x01, 0x01, 0xe0, 0x00, "1 Coin  1 Credits"				},	\
	{0x01, 0x01, 0xe0, 0xc0, "2 Coins 3 Credits"				},	\
	{0x01, 0x01, 0xe0, 0x60, "1 Coin  2 Credits"				},	\
	{0x01, 0x01, 0xe0, 0x80, "1 Coin  5 Credits"				},	\
	{0x01, 0x01, 0xe0, 0xa0, "1 Coin  6 Credits"				},	\
	{0x01, 0x01, 0xe0, 0xe0, "Free Play"						},

#define BUGGYBOY_TAIL_DIPS											\
	{0   , 0xfe, 0   ,    8, "Do not change DSW1 1-3"			},	\
	{0x01, 0x01, 0x07, 0x00, "0"								},	\
	{0x01, 0x01, 0x07, 0x01, "1"								},	\
	{0x01, 0x01, 0x07, 0x02, "2"								},	\
	{0x01, 0x01, 0x07, 0x03, "3"								},	\
	{0x01, 0x01, 0x07, 0x04, "4"								},	\
	{0x01, 0x01, 0x07, 0x05, "5"								},	\
	{0x01, 0x01, 0x07, 0x06, "6"								},	\
	{0x01, 0x01, 0x07, 0x07, "7"								},	\
																	\
	{0   , 0xfe, 0   ,    2, "Service Mode"						},	\
	{0x02, 0x01, 0x80, 0x00, "Off"								},	\
	{0x02, 0x01, 0x80, 0x80, "On"								},	\
																	\
	{0   , 0xfe, 0   ,    9, "Sound PCB Jumper"					},	\
	{0x03, 0x01, 0xff, 0x00, "0"								},	\
	{0x03, 0x01, 0xff, 0x01, "1"								},	\
	{0x03, 0x01, 0xff, 0x02, "2"								},	\
	{0x03, 0x01, 0xff, 0x04, "3"								},	\
	{0x03, 0x01, 0xff, 0x08, "4"								},	\
	{0x03, 0x01, 0xff, 0x10, "5"								},	\
	{0x03, 0x01, 0xff, 0x20, "Speed Buggy/Data East"			},	\
	{0x03, 0x01, 0xff, 0x40, "Buggy Boy/Taito"					},	\
	{0x03, 0x01, 0xff, 0x80, "Buggy Boy/Tatsumi"				},

static struct BurnDIPInfo BuggyboyDIPList[]=
{
	DIP_OFFSET(0x0a)
	{0x00, 0xff, 0xff, 0x5f, NULL								},
	{0x01, 0xff, 0xff, 0x0f, NULL								},
	{0x02, 0xff, 0xff, 0x00, NULL								},
	{0x03, 0xff, 0xff, 0x80, NULL								},

	BUGGYBOY_COMMON_DIPS

	{0   , 0xfe, 0   ,    4, "Coin B"							},
	{0x01, 0x01, 0x18, 0x08, "2 Coins 1 Credits"				},
	{0x01, 0x01, 0x18, 0x00, "1 Coin  1 Credits"				},
	{0x01, 0x01, 0x18, 0x10, "1 Coin  2 Credits"				},
	{0x01, 0x01, 0x18, 0x18, "1 Coin  5 Credits"				},

	BUGGYBOY_TAIL_DIPS
};

STDDIPINFO(Buggyboy)

static struct BurnDIPInfo BuggyboyjrDIPList[]=
{
	DIP_OFFSET(0x0a)
	{0x00, 0xff, 0xff, 0x5f, NULL								},
	{0x01, 0xff, 0xff, 0x0f, NULL								},
	{0x02, 0xff, 0xff, 0x00, NULL								},
	{0x03, 0xff, 0xff, 0x80, NULL								},

	BUGGYBOY_COMMON_DIPS

	{0   , 0xfe, 0   ,    4, "Coin B"							},
	{0x01, 0x01, 0x18, 0x00, "2 Coins 1 Credits"				},
	{0x01, 0x01, 0x18, 0x10, "1 Coin  4 Credits"				},
	{0x01, 0x01, 0x18, 0x08, "1 Coin  5 Credits"				},
	{0x01, 0x01, 0x18, 0x18, "1 Coin  6 Credits"				},

	BUGGYBOY_TAIL_DIPS
};

STDDIPINFO(Buggyboyjr)

/*************************************
 *
 *  Arithmetic unit
 *
 *************************************/

#define INC_PROM_ADDR       ( math.promaddr = (math.promaddr + 1) & 0x1ff )
#define ROR16(val, shift)   ( (UINT16)(((UINT16)(val) >> (shift)) | ((UINT16)(val) << (16 - (shift)))) )
#define ROL16(val, shift)   ( (UINT16)(((UINT16)(val) << (shift)) | ((UINT16)(val) >> (16 - (shift)))) )
#define SWAP16(val)         ( (UINT16)((((UINT16)(val) << 8) & 0xff00) | ((UINT16)(val) >> 8)) )

static inline UINT8 reverse_nibble(UINT8 nibble)
{
	return  (nibble & 1) << 3 |
			(nibble & 2) << 1 |
			(nibble & 4) >> 1 |
			(nibble & 8) >> 3;
}

static inline UINT16 math_ram_r(INT32 offset)
{
	return DrvMathRAM[offset * 2 + 0] | (DrvMathRAM[offset * 2 + 1] << 8);
}

static inline UINT16 math_rom_r(INT32 offset) // offset in words from 0x5000
{
	return DrvMathROM[0x1000 + offset * 2 + 0] | (DrvMathROM[0x1000 + offset * 2 + 1] << 8);
}

static inline INT16 sn_Z() { return (INT16)(sn74s516.ZW32 >> 16); }
static inline UINT16 sn_W() { return (UINT16)(sn74s516.ZW32 & 0xffff); }
static inline void sn_set_Z(UINT16 z) { sn74s516.ZW32 = (INT32)(((UINT32)sn74s516.ZW32 & 0x0000ffff) | ((UINT32)z << 16)); }
static inline void sn_set_W(UINT16 w) { sn74s516.ZW32 = (INT32)(((UINT32)sn74s516.ZW32 & 0xffff0000) | w); }

// simplified state transition table, no states between final input and multiplication/division
static const UINT8 state_table[16][8] =
{
	{  4,  4,  4,  4,  5,  1,  1,  0 },
	{  4,  4,  4,  4,  5,  5,  3,  0 },
	{0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf },
	{  4,  4,  4,  4,  5,  5, 11,  0 },
	{  8,  8,  8,  8,  8,  8,  8,  8 },
	{ 10, 10, 10, 10, 10, 10, 10, 10 },
	{0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf },
	{0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf },
	{  4,  4,  4,  4,  5,  0,  1,  0 },
	{0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf },
	{  4,  4,  4,  4,  4,  5,  1,  0 },
	{  4,  4,  4,  4,  5,  5,  1,  0 },
	{0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf },
	{0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf },
	{0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf },
	{0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf },
};

static void sn74s516_multiply()
{
	sn74s516_t &sn = sn74s516;
	INT32 X = sn.X;
	INT32 Y = sn.Y;

	switch (sn.code)
	{
		case 0:
		case 0x60:
			sn.ZW32 = X * Y;
			break;
		case 2:
		case 0x62:
		case 0x6660:
			sn.ZW32 += X * Y;
			break;
		case 3:
		case 0x63:
			sn.ZW32 += -X * Y;
			break;
		case 0x61:
			sn.ZW32 = -X * Y;
			break;
		case 0x660:
			sn.ZW32 = (INT32)((UINT32)(X * Y) + ((UINT32)sn.ZW32 & 0xffff0000));
			break;
		case 0x661:
		case 0x662:
			sn.ZW32 = (INT32)((UINT32)(-X * Y) + ((UINT32)sn.ZW32 & 0xffff0000));
			break;
	}

	sn.ZWfl = 0;
}

static void sn74s516_divide()
{
	sn74s516_t &sn = sn74s516;

	if (sn.X == 0)
	{
		sn_set_Z(0xffff);
		sn_set_W(0xffff);
		sn.ZWfl = 0;
		return;
	}

	INT32 Z = 0;
	INT32 W = 0;

	switch (sn.code)
	{
		case 4:
		case 0x664:
			Z = sn.ZW32 / sn.X;
			W = sn.ZW32 % sn.X;
			break;
		case 0x6664:
			Z = sn_W() / sn.X;
			W = sn_W() % sn.X;
			break;
	}

	// divide overflow only happens during the chip test
	if (Z > 0xffff)
		Z |= 0xff00;

	sn_set_Z((UINT16)Z);
	sn_set_W((UINT16)W);
	sn.ZWfl = 0;
}

static void sn74s516_update(INT32 ins)
{
	sn74s516_t &sn = sn74s516;

	sn.state = state_table[sn.state][ins];

	if (sn.state == 4)
	{
		sn74s516_multiply();
		sn.state = 8;
	}
	else if (sn.state == 5)
	{
		sn74s516_divide();
		sn.state = 10;
	}
}

static void kick_sn74s516(UINT16 *data, INT32 ins)
{
	sn74s516_t &sn = sn74s516;

#define LOAD_X      (sn.X = *data)
#define LOAD_Y      (sn.Y = *data)
#define LOAD_Z      sn_set_Z(*data)
#define LOAD_W      sn_set_W(*data)
#define READ_ZW     *data = sn.ZWfl ? sn_W() : (UINT16)sn_Z(); \
					sn.ZWfl ^= 1;

#define UPDATE_SEQUENCE (sn.code = (sn.code << 4) | ins)
#define CLEAR_SEQUENCE  (sn.code = 0)

	switch (sn.state)
	{
		case 0:
			CLEAR_SEQUENCE;
			UPDATE_SEQUENCE;

			if (ins < 4)
			{
				LOAD_Y;
				sn74s516_update(ins);
			}
			else if (ins == 4)
			{
				sn74s516_update(ins);
			}
			else if (ins < 7)
			{
				LOAD_X;
				sn74s516_update(ins);
			}
			else if (ins == 7)
			{
				READ_ZW;
			}
			break;

		case 8:
		case 10:
			CLEAR_SEQUENCE;
			UPDATE_SEQUENCE;

			if (ins < 4)
			{
				LOAD_Y;
				sn74s516_update(ins);
			}
			else if (ins == 4 || ins == 5)
			{
				sn74s516_update(ins);
			}
			else if (ins == 6)
			{
				LOAD_X;
				sn74s516_update(ins);
			}
			else if (ins == 7)
			{
				READ_ZW;
				sn74s516_update(ins);
			}
			break;

		case 1:
			// 6666 is an incomplete state, clear it
			if (sn.code == 0x6666)
				CLEAR_SEQUENCE;

			UPDATE_SEQUENCE;
			if (ins < 4)
			{
				LOAD_Y;
				sn74s516_update(ins);
			}
			else if (ins < 6)
			{
				sn74s516_update(ins);
			}
			else if (ins == 6)
			{
				LOAD_Z;
				sn74s516_update(ins);
			}
			else if (ins == 7)
			{
				sn74s516_update(ins);
			}
			break;

		case 3:
			UPDATE_SEQUENCE;
			if (ins < 4)
			{
				LOAD_Y;
				sn74s516_update(ins);
			}
			else if (ins == 4)
			{
				LOAD_W;
				sn74s516_update(ins);
			}
			else if (ins == 5)
			{
				sn74s516_update(ins);
			}
			else if (ins == 6)
			{
				LOAD_W;
				sn74s516_update(ins);
			}
			else if (ins == 7)
			{
				READ_ZW;
				sn74s516_update(ins);
			}
			break;

		case 11:
			UPDATE_SEQUENCE;
			if (ins < 4)
			{
				LOAD_Y;
				sn74s516_update(ins);
			}
			else if (ins < 7)
			{
				sn74s516_update(ins);
			}
			else if (ins == 7)
			{
				// 6667 = load X, load Z, load W, clear Z
				sn_set_Z(0);
				sn74s516_update(ins);
			}
			break;
	}

#undef LOAD_X
#undef LOAD_Y
#undef LOAD_Z
#undef LOAD_W
#undef READ_ZW
#undef UPDATE_SEQUENCE
#undef CLEAR_SEQUENCE
}

#define LHIEN(a)    !(a & 0x80)
#define LLOEN(a)    !(a & 0x40)
#define GO_EN(a)    !(a & 0x4000)

#define SET_INS0_BIT    do { if (!(ins & 0x4) && math.i0ff) ins |= math.i0ff; } while(0)

static UINT16 pss_shift(UINT16 val, UINT16 latch, INT32 rotate)
{
	INT32 shift;

	if (latch & 0x3800)
	{
		shift = (latch >> 11) & 0x7;

		while (shift)
		{
			val = rotate ? ROR16(val, 1) : (UINT16)(val >> 1);
			shift >>= 1;
		}
	}
	else
	{
		shift = (latch >> 7) & 0xf;
		shift = reverse_nibble(shift);
		shift >>= 1;

		while (shift)
		{
			val = rotate ? ROL16(val, 1) : (UINT16)(val << 1);
			shift >>= 1;
		}
	}

	return val;
}

/***************************************************************************

  TX-1

***************************************************************************/

#define TX1_INSLD       0x100
#define TX1_CNTST       0x80
#define TX1_RADCHG      0x20
#define TX1_DSEL        0x03

enum
{
	TX1_SEL_MULEN = 0x00,
	TX1_SEL_PPSEN,
	TX1_SEL_PSSEN,
	TX1_SEL_LMSEL,
	TX1_SEL_DSELOE,
	TX1_SEL_INSCL = 0x06,
	TX1_SEL_ILDEN
};

static inline UINT16 get_datarom_addr()
{
	UINT16 addr = ((math.inslatch & 0x1c00) << 1) | (math.ppshift & 0xff);

	if ((math.inslatch >> 8) & TX1_RADCHG)
		addr |= (math.ppshift & 0x0700);
	else
		addr |= (math.promaddr << 3) & 0x0700;

	return addr & 0x3fff;
}

static inline INT32 tx1_get_o4()
{
	INT32 dsel = (math.inslatch >> 8) & TX1_DSEL;
	INT32 tfad = (math.inslatch & 0x1c00) << 1;
	INT32 sd   = math.ppshift;

	return
		(!BIT(sd, 9) && !BIT(sd,10)) ||
		( BIT(sd, 7) &&  BIT(sd,10)) ||
		(!BIT(sd, 8) &&  BIT(sd, 9)) ||
		(!BIT(sd, 7) &&  BIT(sd, 8)) ||
		!BIT(dsel, 1) || BIT(tfad, 13) || BIT(tfad, 12) || BIT(tfad, 11);
}

static UINT16 tx1_dsel_data(INT32 o4)
{
	INT32 dsel = (math.inslatch >> 8) & TX1_DSEL;

	dsel = (dsel & 2) | ((dsel & o4) ^ 1);

	switch (dsel)
	{
		case 0: return math.muxlatch;
		case 1: return au_data16[get_datarom_addr()];
		case 2: return ROL16(math.muxlatch, 4);
		default: return ROL16(SWAP16(math.muxlatch), 3);
	}
}

static void tx1_update_state()
{
	const UINT16 *prom = au_data16 + (0x8000 >> 1);

	for (;;)
	{
		INT32 go = 0;

		if (!GO_EN(math.inslatch) && GO_EN(prom[math.promaddr]))
			go = 1;
		// e.g. 120 /GO /LHIEN, 121 /GO /LLOEN: both are used
		else if ((GO_EN(math.inslatch) && GO_EN(prom[math.promaddr])) && (LHIEN(math.inslatch) && LLOEN(prom[math.promaddr])))
			go = 1;

		math.inslatch = prom[math.promaddr] & 0x7fff;
		math.mux = (math.inslatch >> 3) & 7;

		if (math.mux == TX1_SEL_INSCL)
			math.i0ff = 0;

		if (go)
		{
			INT32 ins = math.inslatch & 7;

			SET_INS0_BIT;

			if (math.mux == TX1_SEL_DSELOE)
			{
				UINT16 data = tx1_dsel_data(tx1_get_o4());

				kick_sn74s516(&data, ins);
			}
			/*
			    /TMPLD1: /LHIEN
			    /TMPLD2: /LLOEN.!O4 + (/LHIEN.O4)
			    /TMPLD3: /LLOEN
			*/
			else if (LHIEN(math.inslatch) || LLOEN(math.inslatch))
			{
				UINT16 data = 0;

				kick_sn74s516(&data, ins);

				if (LHIEN(math.inslatch) && LLOEN(math.inslatch))
				{
					math.muxlatch = data;
				}
				else if (math.mux == TX1_SEL_LMSEL) // O4 = 0
				{
					if (LLOEN(math.inslatch))
					{
						math.muxlatch &= 0x001f;
						math.muxlatch |= data & 0xffe0;
					}
					else if (LHIEN(math.inslatch))
					{
						math.muxlatch &= 0xffe0;
						math.muxlatch |= data & 0x001f;
					}
				}
				else
				{
					INT32 o4 = tx1_get_o4();

					if (LLOEN(math.inslatch))
					{
						math.muxlatch &= 0x0fff;
						math.muxlatch |= data & 0xf000;

						if (!o4)
						{
							math.muxlatch &= 0xf01f;
							math.muxlatch |= data & 0x0fe0;
						}
					}
					else if (LHIEN(math.inslatch))
					{
						math.muxlatch &= 0xffe0;
						math.muxlatch |= data & 0x001f;

						if (o4)
						{
							math.muxlatch &= 0xf01f;
							math.muxlatch |= data & 0x0fe0;
						}
					}
				}
			}
			else
			{
				if (math.mux == TX1_SEL_PPSEN)
				{
					kick_sn74s516(&math.ppshift, ins);
				}
				else
				{
					// bus pullups
					UINT16 data = 0xffff;
					kick_sn74s516(&data, ins);
				}
			}
		}

		// another instruction in the sequence?
		if (prom[math.promaddr] & 0x8000)
			break;
		else
			INC_PROM_ADDR;
	}
}

static UINT16 tx1_math_r(UINT32 offset)
{
	offset = offset << 1;

	// /MLPCS
	if (offset < 0x400)
	{
		INT32 ins;

		if (offset & 0x200)
		{
			ins = math.inslatch & 7;
			SET_INS0_BIT;
		}
		else
		{
			ins = (offset >> 1) & 7;
		}

		kick_sn74s516(&math.retval, ins);
	}
	// /PPSEN
	else if (offset < 0x800)
	{
		math.retval = 0xffff;
	}
	// /MUXCS
	else if ((offset & 0xc00) == 0xc00)
	{
		INT32 o4 = (math.mux == TX1_SEL_LMSEL) ? 0 : tx1_get_o4();

		math.retval = tx1_dsel_data(o4);

		// /SPCS region?
		if (offset < 0xe00)
		{
			if (math.mux == TX1_SEL_PPSEN)
			{
				math.ppshift = math.retval & 0x3fff;
			}
			else if (math.mux == TX1_SEL_PSSEN)
			{
				math.ppshift = math.retval;
			}

			if (math.mux != TX1_SEL_ILDEN)
			{
				INC_PROM_ADDR;
				tx1_update_state();

				return math.retval;
			}
		}
	}
	else
	{
		if (math.mux == TX1_SEL_PPSEN)
			math.retval = math.ppshift & 0x3fff;
		else
			math.retval = 0xffff; // pullups
	}

	if (offset & TX1_INSLD)
	{
		math.promaddr = (offset << 2) & 0x1ff;
		tx1_update_state();
	}
	else if (offset & TX1_CNTST)
	{
		INC_PROM_ADDR;
		tx1_update_state();
	}

	return math.retval;
}

static void tx1_math_w(UINT32 offset, UINT16 data)
{
	math.cpulatch = data;
	offset <<= 1;

	// /MLPCS
	if (offset < 0x400)
	{
		INT32 ins;

		if (offset & 0x200)
		{
			ins = math.inslatch & 7;
			SET_INS0_BIT;
		}
		else
		{
			ins = (offset >> 1) & 7;
		}

		kick_sn74s516(&math.cpulatch, ins);
	}
	// /PPSEN, input is 14 bits
	else if ((offset & 0xc00) == 0x400)
	{
		math.ppshift = math.cpulatch & 0x3fff;
	}
	// /PSSEN
	else if ((offset & 0xc00) == 0x800)
	{
		math.ppshift = pss_shift(math.ppshift, math.cpulatch, 0);
	}
	// /MUXCS
	else if ((offset & 0xc00) == 0xc00)
	{
		math.muxlatch = math.cpulatch;
	}

	if (offset & TX1_INSLD)
	{
		math.promaddr = (offset << 2) & 0x1ff;
		tx1_update_state();
	}
	else if (offset & TX1_CNTST)
	{
		INC_PROM_ADDR;
		tx1_update_state();
	}
}

static void tx1_spcs_common(INT32 shift_mask)
{
	if (math.mux == TX1_SEL_ILDEN)
	{
		math.i0ff = (math.cpulatch & (1 << 14)) ? 1 : 0;
	}
	else if (math.mux == TX1_SEL_MULEN)
	{
		INT32 ins = math.inslatch & 7;

		SET_INS0_BIT;
		kick_sn74s516(&math.cpulatch, ins);
	}
	else if (math.mux == TX1_SEL_PPSEN)
	{
		math.ppshift = math.cpulatch;
	}
	else if (math.mux == TX1_SEL_PSSEN)
	{
		math.ppshift = pss_shift(math.ppshift, math.cpulatch, 0) & shift_mask;
	}

	if (math.mux != TX1_SEL_ILDEN)
	{
		INC_PROM_ADDR;
		tx1_update_state();
	}
}

static UINT16 tx1_spcs_rom_r(UINT32 offset)
{
	math.cpulatch = math_rom_r(offset);

	tx1_spcs_common(0x7ff);

	return math.cpulatch;
}

static UINT16 tx1_spcs_ram_r(UINT32 offset)
{
	math.cpulatch = math_ram_r(offset);

	tx1_spcs_common(0x7ff);

	return math.cpulatch;
}

/***************************************************************************

  Buggy Boy

***************************************************************************/

#define BB_INSLD        0x100
#define BB_CNTST        0x80
#define BB_RADCHG       0x20
#define BB_DSEL         0x03

enum
{
	BB_MUX_MULEN = 0x00,
	BB_MUX_PPSEN,
	BB_MUX_PSSEN,
	BB_MUX_LMSEL,
	BB_MUX_DPROE,
	BB_MUX_PPOE,
	BB_MUX_INSCL,
	BB_MUX_ILDEN
};

static inline UINT16 get_bb_datarom_addr()
{
	UINT16 addr = ((math.inslatch & 0x1c00) << 1) | (math.ppshift & 0xff);

	if ((math.inslatch >> 8) & BB_RADCHG)
		addr |= (math.ppshift & 0x0700);
	else
		addr |= (math.promaddr << 3) & 0x0700;

	return addr & 0x3fff;
}

static void buggyboy_update_state()
{
	const UINT16 *prom = au_data16 + (0x8000 >> 1);

	for (;;)
	{
		INT32 go = 0;

		if (!GO_EN(math.inslatch) && GO_EN(prom[math.promaddr]))
			go = 1;
		else if ((GO_EN(math.inslatch) && GO_EN(prom[math.promaddr])) && (LHIEN(math.inslatch) && LLOEN(prom[math.promaddr])))
			go = 1;

		math.inslatch = prom[math.promaddr] & 0x7fff;
		math.mux = (math.inslatch >> 3) & 7;

		if (math.mux == BB_MUX_INSCL)
			math.i0ff = 0;

		if (go)
		{
			INT32 ins = math.inslatch & 7;

			SET_INS0_BIT;

			if (math.mux == BB_MUX_DPROE)
			{
				kick_sn74s516(&au_data16[get_bb_datarom_addr()], ins);
			}
			else if (math.mux == BB_MUX_PPOE)
			{
				kick_sn74s516(&math.ppshift, ins);
			}
			else if (LHIEN(math.inslatch) || LLOEN(math.inslatch))
			{
				UINT16 data = 0;

				kick_sn74s516(&data, ins);

				if (LHIEN(math.inslatch) && LLOEN(math.inslatch))
				{
					math.ppshift = data;
				}
				else if (math.mux == BB_MUX_LMSEL)
				{
					if (LLOEN(math.inslatch))
					{
						math.ppshift &= 0x000f;
						math.ppshift |= data & 0xfff0;
					}
					else if (LHIEN(math.inslatch))
					{
						math.ppshift &= 0xfff0;
						math.ppshift |= data & 0x000f;
					}
				}
				else
				{
					if (LLOEN(math.inslatch))
					{
						math.ppshift &= 0x0fff;
						math.ppshift |= data & 0xf000;
					}
					else if (LHIEN(math.inslatch))
					{
						math.ppshift &= 0xf000;
						math.ppshift |= data & 0x0fff;
					}
				}
			}
			else
			{
				if (math.mux == BB_MUX_PPSEN)
				{
					kick_sn74s516(&math.ppshift, ins);
				}
				else
				{
					// bus pullups
					UINT16 data = 0xffff;
					kick_sn74s516(&data, ins);
				}
			}
		}

		// handle rotation
		if (((math.inslatch >> 8) & BB_DSEL) == 1)
		{
			math.ppshift = ROR16(math.ppshift, 4);
		}
		else if (((math.inslatch >> 8) & BB_DSEL) == 2)
		{
			math.ppshift = ROL16(math.ppshift, 4);
		}

		// another instruction in the sequence?
		if (prom[math.promaddr] & 0x8000)
			break;
		else
			INC_PROM_ADDR;
	}
}

static UINT16 buggyboy_math_r(UINT32 offset)
{
	offset = offset << 1;

	// /MLPCS
	if (offset < 0x400)
	{
		INT32 ins;

		if (offset & 0x200)
		{
			ins = math.inslatch & 7;
			SET_INS0_BIT;
		}
		else
		{
			ins = (offset >> 1) & 7;
		}

		kick_sn74s516(&math.retval, ins);
	}
	// /PPSEN
	else if (offset < 0x800)
	{
		math.retval = math.ppshift;
	}
	// /DPROE
	else if ((offset & 0xc00) == 0xc00)
	{
		UINT16 addr = get_bb_datarom_addr();

		math.retval = au_data16[addr];

		if (math.mux == BB_MUX_PPSEN)
			math.ppshift = au_data16[addr];

		// /SPCS region?
		if (offset < 0xe00)
		{
			if (math.mux != BB_MUX_ILDEN)
			{
				INC_PROM_ADDR;
				buggyboy_update_state();
			}
		}
	}
	else
	{
		if (math.mux == BB_MUX_PPSEN)
			math.retval = math.ppshift;
		else
			math.retval = 0xffff; // pullups
	}

	if (offset & BB_INSLD)
	{
		math.promaddr = (offset << 2) & 0x1ff;
		buggyboy_update_state();
	}
	else if (offset & BB_CNTST)
	{
		INC_PROM_ADDR;
		buggyboy_update_state();
	}

	return math.retval;
}

static void buggyboy_math_w(UINT32 offset, UINT16 data)
{
	math.cpulatch = data;

	offset <<= 1;

	// /MLPCS
	if (offset < 0x400)
	{
		INT32 ins;

		if (offset & 0x200)
		{
			ins = math.inslatch & 7;
			SET_INS0_BIT;
		}
		else
		{
			ins = (offset >> 1) & 7;
		}

		kick_sn74s516(&math.cpulatch, ins);
	}
	// /PPSEN
	else if ((offset & 0xc00) == 0x400)
	{
		math.ppshift = math.cpulatch;
	}
	// /PSSEN
	else if ((offset & 0xc00) == 0x800)
	{
		if (((math.inslatch >> 8) & BB_DSEL) == 3)
			math.ppshift = pss_shift(math.ppshift, math.cpulatch, 1);
	}

	if (offset & BB_INSLD)
	{
		math.promaddr = (offset << 2) & 0x1ff;
		buggyboy_update_state();
	}
	else if (offset & BB_CNTST)
	{
		INC_PROM_ADDR;
		buggyboy_update_state();
	}
}

static void buggyboy_spcs_common()
{
	if (math.mux == BB_MUX_ILDEN)
	{
		math.i0ff = (math.cpulatch & (1 << 14)) ? 1 : 0;
	}
	else if (math.mux == BB_MUX_MULEN)
	{
		INT32 ins = math.inslatch & 7;

		SET_INS0_BIT;
		kick_sn74s516(&math.cpulatch, ins);
	}
	else if (math.mux == BB_MUX_PPSEN)
	{
		math.ppshift = math.cpulatch;
	}
	else if (math.mux == BB_MUX_PSSEN)
	{
		if (((math.inslatch >> 8) & BB_DSEL) == 3)
			math.ppshift = pss_shift(math.ppshift, math.cpulatch, 1);
	}

	if (math.mux != BB_MUX_ILDEN)
	{
		INC_PROM_ADDR;
		buggyboy_update_state();
	}
}

static UINT16 buggyboy_spcs_rom_r(UINT32 offset)
{
	math.cpulatch = math_rom_r(offset);

	buggyboy_spcs_common();

	return math.cpulatch;
}

static UINT16 buggyboy_spcs_ram_r(UINT32 offset)
{
	math.cpulatch = math_ram_r(offset);

	buggyboy_spcs_common();

	return math.cpulatch;
}

/*************************************
 *
 *  Video control registers
 *
 *************************************/

static void tx1_bankcs_w(UINT32 offset, UINT16 data)
{
	// AAB2 = /BASET0, AAB3 = /BASET, AAB4 = /BSET, AAB5 = /HASET, AAB6 = /HSET
	offset <<= 1;

	if (offset & 0x04)
	{
		vregs.ba_inc &= ~0x0000ffff;
		vregs.ba_inc |= data;

		if (!(offset & 2))
			vregs.ba_val &= ~0x000ffff;
	}
	if (offset & 0x08)
	{
		data &= 0xff;
		vregs.ba_inc &= ~0xffff0000;
		vregs.ba_inc |= data << 16;

		vregs.bank_mode = BIT(data, 1);

		if (!(offset & 2))
			vregs.ba_val &= ~0xffff0000;
	}
	if (!(offset & 0x10))
	{
		if (offset & 2)
			vregs.ba_val = (vregs.ba_inc + vregs.ba_val) & 0x00ffffff;
	}
	if (offset & 0x20)
	{
		vregs.h_inc = data;

		if (!(offset & 2))
			vregs.h_val = 0;
	}
	if (!(offset & 0x40))
	{
		vregs.h_val += vregs.h_inc;
	}
}

static void tx1_slincs_w(UINT32 offset, UINT16 data)
{
	if (offset == 1)
		vregs.slin_inc = data;
	else
		vregs.slin_inc = vregs.slin_val = 0;
}

static void math_set_test(INT32 state);

/*
    2400-24FF is road control (R/W)

    /GAS = 24XX:
    /BASET0 = 2400-F, 2410-F
    /BASET1 = 2420-F, 2430-F
    /BSET   = 2440-F, 2450-F
    /HASET  = 2460-F, 2470-F
    /HSET   = 2480-F, 2490-F
    /WASET  = 24A0-F, 24B0-F
    /FLAGS  = 24E0-F, 24F0-F
*/
static void buggyboy_gas_w(UINT32 offset, UINT16 data)
{
	offset <<= 1;

	switch (offset & 0xe0)
	{
		case 0x00:
		{
			vregs.ba_inc &= ~0x0000ffff;
			vregs.ba_inc |= data;

			if (!(offset & 2))
				vregs.ba_val &= ~0x0000ffff;

			break;
		}
		case 0x20:
		{
			data &= 0xff;
			vregs.ba_inc &= ~0xffff0000;
			vregs.ba_inc |= data << 16;

			vregs.bank_mode = data & 1;

			if (!(offset & 2))
				vregs.ba_val &= ~0xffff0000;

			break;
		}
		case 0x40:
		{
			if (offset & 2)
				vregs.ba_val = (vregs.ba_inc + vregs.ba_val) & 0x00ffffff;

			break;
		}
		case 0x60:
		{
			vregs.h_inc = data;
			vregs.shift = 0;

			if (!(offset & 2))
				vregs.h_val = 0;

			break;
		}
		case 0x80:
		{
			if (offset & 2)
				vregs.h_val += vregs.h_inc;
			break;
		}
		case 0xa0:
		{
			vregs.wa8 = data >> 8;
			vregs.wa4 = 0;
			break;
		}
		case 0xe0:
		{
			math_set_test(0);
			vregs.flags = data;
			break;
		}
	}

	// latched by LS373 76/77
	vregs.gas = data;
}

/*************************************
 *
 *  Layer rendering
 *
 *************************************/

#define OBJ_FRAC    16

enum
{
	TX1_RDFLAG_RVA8 = 0,
	TX1_RDFLAG_RVA9,
	TX1_RDFLAG_RVA7,
	TX1_RDFLAG_TNLF,
	TX1_RDFLAG_STLF,
	TX1_RDFLAG_SCCHGF
};

#define BB_RDFLAG_WAVE1     7
#define BB_RDFLAG_WAVE0     6
#define BB_RDFLAG_TNLMD1    5
#define BB_RDFLAG_TNLMD0    4
#define BB_RDFLAG_TNLF      3
#define BB_RDFLAG_LINF      2
#define BB_RDFLAG_RVA7      1
#define BB_RDFLAG_WANGL     0

static void tx1_draw_char(UINT8 *bitmap)
{
	INT32 x, y;
	UINT32 scroll_x;

	/* 2bpp characters */
	const UINT8 *const chars = &DrvCharROM[0];
	const UINT8 *const gfx2 = &DrvCharROM[0x4000];

	/* X scroll value is the last word in char RAM */
	scroll_x = BURN_ENDIAN_SWAP_INT16(vram16[0xfff]) & 0x3ff;

	for (y = 0; y < 240; ++y)
	{
		UINT32 d0 = 0, d1 = 0;
		UINT32 colour = 0;
		UINT32 y_offs;
		UINT32 x_offs;
		UINT32 y_gran;

		/* No y-scrolling? */
		y_offs = y;

		if ((y_offs >= 64) && (y_offs < 128))
			x_offs = vregs.slock ? scroll_x : 0;
		else
			x_offs = 0;

		y_gran = y_offs & 7;

		if (x_offs & 7)
		{
			UINT32 tilenum;
			UINT16 ram_val = BURN_ENDIAN_SWAP_INT16(vram16[((y_offs << 4) & 0xf80) + ((x_offs >> 3) & 0x7f)]);

			tilenum = (ram_val & 0x03ff) | ((ram_val & 0x8000) >> 5);
			colour = (ram_val & 0xfc00) >> 8;
			d0 = *(gfx2 + (tilenum << 3) + y_gran);
			d1 = *(chars + (tilenum << 3) + y_gran);
		}

		for (x = 0; x < 256 * 3; ++x)
		{
			UINT32 x_gran = x_offs & 7;

			if (!x_gran)
			{
				UINT32 tilenum;
				UINT16 ram_val = BURN_ENDIAN_SWAP_INT16(vram16[((y_offs << 4) & 0xf80) + ((x_offs >> 3) & 0x7f)]);

				tilenum = (ram_val & 0x03ff) | ((ram_val & 0x8000) >> 5);
				colour = (ram_val & 0xfc00) >> 8;
				d0 = *(gfx2 + (tilenum << 3) + y_gran);
				d1 = *(chars + (tilenum << 3) + y_gran);
			}

			*bitmap++ = colour |
						(((d1 >> (7 ^ x_gran)) & 1) << 1) |
						((d0 >> (7 ^ x_gran)) & 1);

			x_offs = (x_offs + 1) & 0x3ff;
		}
	}
}

#define TX1_GET_ROADPIX(NUM) \
{ \
	UINT32 addr = (rva6_0 << 5) | (road##NUM##_hcnt & 0x1f); \
	UINT8 promaddr1, promaddr2, promaddr3; \
	promaddr1 = rom_a[addr]; \
	promaddr2 = rom_b[addr]; \
	promaddr3 = rom_c[addr]; \
	pix[NUM][0][0] = prom_a[promaddr1]; pix[NUM][0][1] = prom_b[promaddr1]; pix[NUM][0][2] = prom_c[promaddr1]; \
	pix[NUM][1][0] = prom_a[promaddr2]; pix[NUM][1][1] = prom_b[promaddr2]; pix[NUM][1][2] = prom_c[promaddr2]; \
	pix[NUM][2][0] = prom_a[promaddr3]; pix[NUM][2][1] = prom_b[promaddr3]; pix[NUM][2][2] = prom_c[promaddr3]; \
	pix[NUM][3][0] = prom_a[0];         pix[NUM][3][1] = prom_b[0];         pix[NUM][3][2] = prom_c[0]; \
}

static void tx1_draw_road_pixel(int screen, UINT8 *bmpaddr,
									UINT8 apix[3], UINT8 bpix[3], UINT32 pixnuma, UINT32 pixnumb,
									UINT8 stl, UINT8 sld, UINT8 selb,
									UINT8 bnk, UINT8 rorev, UINT8 eb, UINT8 r, UINT8 delr)
{
	UINT8 a0 = BIT(apix[0], pixnuma);
	UINT8 a1 = BIT(apix[1], pixnuma);
	UINT8 a2 = BIT(apix[2], pixnuma);

	UINT8 b0 = BIT(bpix[0], pixnumb);
	UINT8 b1 = BIT(bpix[1], pixnumb);
	UINT8 b2 = BIT(bpix[2], pixnumb);

	UINT8 d3;
	UINT8 d2;
	UINT8 d1;
	UINT8 d0;
	UINT8 sel;
	UINT8 c6 = BIT(sld, 6);
	UINT8 c5 = BIT(sld, 5);
	UINT8 c4 = BIT(sld, 4);
	UINT8 c3 = BIT(sld, 2);

	UINT32 addr_offset = screen * 256;

	sel = !bnk && ( (a2 && !b0) || (!a0 && b2) || !a1 || !a2 || !b1 || !b2 );

	d3 =
		(a2 && a1 && a0 && b2 && b1 && b0 && !rorev)
		|| (a1 && b1 && !b0 && stl && !bnk && !c6)
		|| (a1 && !a0 && b1 && stl && !bnk && !c6)
		|| (a1 && !b2 && b1 && stl && !bnk && !c6)
		|| (a2 && b2 && !b1 && stl && !bnk && !c4)
		|| (a2 && !a1 && b2 && stl && !bnk && !c4)
		|| (!a2 && a1 && b2 && stl && !bnk && !c6)
		|| (a2 && !b2 && b1 && stl && !bnk && !c6)
		|| (!a2 && !a1 && stl && !bnk && !c5)
		|| (!b2 && !b1 && stl && !bnk && !c5)
		|| (!b0 && !stl && !bnk && !c3)
		|| (!b1 && !stl && !bnk && !c3)
		|| (!a0 && !stl && !bnk && !c3)
		|| (!a1 && !stl && !bnk && !c3)
		|| (!a2 && !stl && !bnk && !c3)
		|| (!b2 && !stl && !bnk && !c3)
		|| (bnk && !rorev);

	if (eb == 0)
	{
		if (sel == 1)
		{
			d2 = stl && (!a2 || !b2);
			d1 = !stl || (!a2 && !a1) || (!b1 && !b2) || (a2 && !b1) || (b2 && !a1);
			d0 = !stl
				|| (!a2 && !a0 && b1)
				|| (a1 && !b2 && !b0)
				|| (!a2 && !a1 && !a0)
				|| (!b2 && !b1 && !b0)
				|| (a2 && a1 && !b0)
				|| (!a0 && b2 && b1)
				|| (a2 && !b1 && !b0)
				|| (!a1 && !a0 && b2)
				|| (!a2 && !a0 && b2)
				|| (a2 && !b2 && !b0);
		}
		else
		{
			d2 = BIT(vregs.scol, selb ? 2 : 10);
			d1 = BIT(vregs.scol, selb ? 1 : 9);
			d0 = BIT(vregs.scol, selb ? 0 : 8);
		}
	}
	else
		d2 = d1 = d0 = 0;

	*(bmpaddr + addr_offset) = (bnk << 6) | (r << 5) | (!(sel && delr) << 4 ) | (d3 << 3) | (d2 << 2) | (d1 << 1) | d0;
}

/* This could do with a tidy up and more comments... */
static void tx1_draw_road(UINT8 *bitmap)
{
	UINT32  rva9_8;
	UINT32  rva7;
	UINT32  tnlf;
	UINT32  stlf;
	UINT32  scchgf;

	UINT32  vc = 0;

	UINT16  road0_hcnt;
	UINT8   road0_pcnt;
	UINT16  road1_hcnt;
	UINT8   road1_pcnt;
	UINT8   pix[2][4][3];

	/* Road slice map ROMs */
	const UINT8 *const rom_a = &DrvRoadROM[0];
	const UINT8 *const rom_b = &DrvRoadROM[0x2000];
	const UINT8 *const rom_c = &DrvRoadROM[0x4000];

	/* Pixel data */
	const UINT8 *const prom_a = &DrvProms[0x1100];
	const UINT8 *const prom_b = &DrvProms[0x1300];
	const UINT8 *const prom_c = &DrvProms[0x1500];
	const UINT8 *const vprom  = &DrvProms[0x1700];

	rva9_8  = (vregs.flags & 3) << 8;
	rva7    = !BIT(vregs.flags, TX1_RDFLAG_RVA7) << 7;
	tnlf    = BIT(vregs.flags, TX1_RDFLAG_TNLF);
	scchgf  = BIT(vregs.flags, TX1_RDFLAG_SCCHGF);
	stlf    = BIT(vregs.flags, TX1_RDFLAG_STLF);

	for (INT32 y = 0; y < 240; ++y)
	{
		UINT8   sld;
		UINT8   rva6_0;
		UINT16  rcrdb15_0;
		UINT16  rva_addr;
		UINT32  bank_cnt;

		UINT16  vat;
		UINT16  vp0;
		UINT16  vp1;
		UINT16  vp2;
		UINT16  vp3;
		UINT16  vp4;

		UINT32  va8, va9, va10, va11, va12, va13, va14, va15;
		UINT32  v0, v1, v2;

		UINT8   hc1_u, hc1_l;
		UINT8   hc0_u, hc0_l;

		UINT32  bnkls, bnkcs, bnkrs;
		UINT32  rl, rc, rr;

		UINT32  stl;
		UINT32  selb;
		UINT32  ebls, ebcs, ebrs;

		UINT8 *bmpaddr = bitmap + (y * 768);

		UINT32 rltmp, rctmp, rrtmp;
		UINT32 eltmp, ectmp, ertmp;

		UINT32 tmpm;
		UINT32 tmpn;
		UINT32 tmpc;
		UINT32 tmpd;

		UINT8 scrcnt0;
		UINT8 scrcnt1;

		UINT8 rorevls = 0;
		UINT8 rorevcs = 0;
		UINT8 rorevrs = 0;

		int febl[2] = { 0, 0 };
		int febr[2] = { 0, 0 };
		int febc[2] = { 0, 0 };

		/* Road vertical address */
		if (BIT(vregs.h_val, 15))
			rva6_0 = 0x7f;
		else
			rva6_0 = (~vregs.h_val >> 7) & 0x7f;

		rva_addr = rva9_8 | rva6_0;

		/* Get the stripe data also */
		sld = vprom[rva6_0] + vregs.slin_val;

		/* Load the left road h-counter */
		rcrdb15_0 = BURN_ENDIAN_SWAP_INT16(rcram16[(0x80 + rva_addr) ^ 0x7ff]);
		road0_pcnt = rcrdb15_0 & 0x7;
		road0_hcnt = (rcrdb15_0 >> 3) & 0x7f;
		road0_hcnt |= rcrdb15_0 & 0xfc00 ? 0x80 : 0;
		road0_hcnt |= (rcrdb15_0 & 0xfc00) == 0xfc00 ? 0x100 : 0x000;

		/* Load the second road h-counter */
		rcrdb15_0 = BURN_ENDIAN_SWAP_INT16(rcram16[(rva7 + rva_addr) ^ 0x7ff]);
		road1_pcnt = rcrdb15_0 & 0x7;
		road1_hcnt = (rcrdb15_0 >> 3) & 0x7f;
		road1_hcnt |= rcrdb15_0 & 0xfc00 ? 0x80 : 0;
		road1_hcnt |= (rcrdb15_0 & 0xfc00) == 0xfc00 ? 0x100 : 0x000;
		road1_hcnt |= (rcrdb15_0 & 0x800);

		scrcnt0 = (road0_hcnt >> 5) & 0xf;
		scrcnt1 = (road1_hcnt >> 5) & 0xf;

		if (road0_pcnt & 7)
		{
			TX1_GET_ROADPIX(0);

			febl[0] = (scrcnt0 + 0) & 0xc;
			febc[0] = (scrcnt0 + 1) & 0xc;
			febr[0] = (scrcnt0 + 2) & 0xc;
		}

		if (road1_pcnt & 7)
		{
			UINT32 y4 = (road1_hcnt >> 4) & 1;
			UINT32 temp = (road1_hcnt >> 5) & 0xf;
			UINT32 x, u;
			UINT32 fl11 = BIT(road1_hcnt, 15);

			TX1_GET_ROADPIX(1);

			x = (temp & 0xc);
			u = !(((temp & 1) && y4) || (temp & 2));
			rorevls = ((fl11 && !x) || (fl11 && x && u));
			febl[1] = x;

			temp += 1;
			x = (temp & 0xc);
			u = !(((temp & 1) && y4) || (temp & 2));
			rorevcs = ((fl11 && !x) || (fl11 && x && u));
			febc[1] = x;

			temp += 1;
			x = (temp & 0xc);
			u = !(((temp & 1) && y4) || (temp & 2));
			rorevrs = ((fl11 && !x) || (fl11 && x && u));
			febr[1] = x;
		}

		/* Load the bank counter with accumulator bits 14-5 */
		bank_cnt = (vregs.ba_val >> 5) & 0x3ff;

		/* Load vertical position data */
		vat = BURN_ENDIAN_SWAP_INT16(rcram16[(rva9_8 + 7) ^ 0x7f8]);
		vp0 = BURN_ENDIAN_SWAP_INT16(rcram16[(rva9_8 + 6) ^ 0x7f8]);
		vp1 = BURN_ENDIAN_SWAP_INT16(rcram16[(rva9_8 + 5) ^ 0x7f8]);
		vp2 = BURN_ENDIAN_SWAP_INT16(rcram16[(rva9_8 + 4) ^ 0x7f8]);
		vp3 = BURN_ENDIAN_SWAP_INT16(rcram16[(rva9_8 + 3) ^ 0x7f8]);
		vp4 = BURN_ENDIAN_SWAP_INT16(rcram16[(rva9_8 + 2) ^ 0x7f8]);

		/*  */
		if (y-1 == vp0) vc++;
		if (y-1 == vp1) vc++;
		if (y-1 == vp2) vc++;
		if (y-1 == vp3) vc++;
		if (y-1 == vp4) vc++;

		/* */
		va8 = BIT(vat, 8);
		va9 = BIT(vat, 9);
		va10 = BIT(vat, 10);
		va11 = BIT(vat, 11);
		va12 = BIT(vat, 12);
		va13 = BIT(vat, 13);
		va14 = BIT(vat, 14);
		va15 = BIT(vat, 15);

		v0 = BIT(vc, 0);
		v1 = BIT(vc, 1);
		v2 = BIT(vc, 2);

		/* Load the horizontal tunnel position counters */
		hc1_u = BURN_ENDIAN_SWAP_INT16(rcram16[(rva9_8 + 1) ^ 0x7f8]) >> 8;
		hc1_l = BURN_ENDIAN_SWAP_INT16(rcram16[(rva9_8 + 1) ^ 0x7f8]) & 0xff;
		hc0_u = BURN_ENDIAN_SWAP_INT16(rcram16[(rva9_8 + 0) ^ 0x7f8]) >> 8;
		hc0_l = BURN_ENDIAN_SWAP_INT16(rcram16[(rva9_8 + 0) ^ 0x7f8]) & 0xff;

		stl = !stlf || v0 || (!v2 && !scchgf) || scchgf;
		selb = (!v2 && !scchgf) || v2;

		rltmp =
		(v2 && !v0 && !va10)
		|| (!v2 && v0 && !va10)
		|| (v2 && !v0 && !va11)
		|| (v2 && !v1 && !v0)
		|| (!v2 && v0 && !va11)
		|| (!v2 && v1 && !v0)
		|| (!v2 && !v1 && v0);

		rctmp =
		(v2 && !v0 && va9 && va8)
		|| (!v2 && v0 && va9 && va8)
		|| (v2 && !v0 && !va11)
		|| (v2 && !v1 && !v0)
		|| (!v2 &&  v0 && !va11)
		|| (!v2 &&  v1 && !v0)
		|| (!v2 && !v1 &&  v0);

		rrtmp =
		(v2 && !v0 && !va11 && !va10)
		|| (!v2 && v0 && !va11 && !va10)
		|| (v2 && !v0 && va9)
		|| (!v2 && v0 && va9)
		|| (v2 && !v1 && !v0)
		|| (!v2 && v1 && !v0)
		|| (!v2 && !v1 && v0);

		ectmp =
		(v2 && !v0 && va13 && va12)
		|| (!v2 && v1 && va13 && va12)
		|| (!v2 && !v1 && v0)
		|| (v2 && !v0 && !va15)
		|| (!v2 && v1 && !va15);

		eltmp =
		(v2 && !v0 && !va14)
		|| (!v2 && v1 && !va14)
		|| (!v2 && !v1 && v0)
		|| (v2 && !v0 && !va15)
		|| (!v2 && v1 && !va15);

		ertmp =
		(v2 && !v0 && !va15 && !va14)
		|| (!v2 && v1 && !va15 && !va14)
		|| (v2 && !v0 && va13)
		|| (!v2 && v1 && va13)
		|| (!v2 && !v1 && v0);

		tmpn = (v2 && !v0) || (!v2 && v0);
		tmpm = (v2 && !v0) || (!v2 && v1);
		tmpc = (!v1 && !v0) || (v2);
		tmpd = v1 && v0 && va11;

		for (INT32 x = 0; x < 256; x++)
		{
			UINT32 pixnum0 = (road0_pcnt & 7) ^ 7;
			UINT32 pixnum1 = (road1_pcnt & 7) ^ 7;

			UINT32 cyu, cyl;
			UINT32 delrl, delrc, delrr;

			/* */
			scrcnt0 = (road0_hcnt >> 5) & 0xf;
			scrcnt1 = (road1_hcnt >> 5) & 0xf;

			/* Get new pixel data? */
			if (!(road0_pcnt & 7))
			{
				TX1_GET_ROADPIX(0);

				/* Road 0 enables */
				febl[0] = (scrcnt0 + 0) & 0xc;
				febc[0] = (scrcnt0 + 1) & 0xc;
				febr[0] = (scrcnt0 + 2) & 0xc;
			}

			if (!(road1_pcnt & 7))
			{
				UINT32 y4 = (road1_hcnt >> 4) & 1;
				UINT32 temp = (road1_hcnt >> 5) & 0xf;
				UINT32 x0, u;
				UINT32 fl11 = BIT(road1_hcnt, 15);

				TX1_GET_ROADPIX(1);

				x0 = (temp & 0xc);
				u = !(((temp & 1) && y4) || (temp & 2));
				rorevls = ((fl11 && !x0) || (fl11 && x0 && u));
				febl[1] = x0;

				temp += 1;
				x0 = (temp & 0xc);
				u = !(((temp & 1) && y4) || (temp & 2));
				rorevcs = ((fl11 && !x0) || (fl11 && x0 && u));
				febc[1] = x0;

				temp += 1;
				x0 = (temp & 0xc);
				u = !(((temp & 1) && y4) || (temp & 2));
				rorevrs = ((fl11 && !x0) || (fl11 && x0 && u));
				febr[1] = x0;
			}

			/* Road camber/banking */
			if (BIT(vregs.ba_val, 23))
			{
				bnkls = 1;
				bnkcs = 1;
				bnkrs = 1;
			}
			else if (vregs.ba_val & 0x007f8000)
			{
				bnkls = 0;
				bnkcs = 0;
				bnkrs = 0;
			}
			else
			{
				bnkls = bank_cnt < 0x400;
				bnkcs = bank_cnt < 0x300;
				bnkrs = bank_cnt < 0x200;
			}

			if (vregs.bank_mode)
			{
				bnkls ^= 1;
				bnkcs ^= 1;
				bnkrs ^= 1;
			}

			cyu = hc1_u == 0xff;
			cyl = hc1_l == 0xff;

			rl = tnlf && ( rltmp || (tmpn && (!cyl || (va8 && va9 && cyu))) );
			rc = tnlf && ( rctmp || (tmpn && ((!va10 && !cyl) || (va9 && cyu))) );
			rr = tnlf && ( rrtmp || (tmpn && ((!va11 && !cyl) || (va8 && cyu))) );

			/* Evaluates to 0 for tunnels */
			delrl = !tnlf || tmpc || ((tmpd && va10) && ((!va8 && cyl) || (!va9 && cyl) || (!cyu && !cyl)));

			delrr =
				!tnlf
				|| tmpc
				|| (v1 && v0 && va10 && !va9 && !va8 && cyl)
				|| (v1 && v0 && va10 && !va9 && cyl && !cyu)
				|| (tmpd && !va9 && !cyu)
				|| (tmpd && !va9 && !va8);

			delrc = !tnlf || tmpc || (tmpd && ((!va8 && cyl && !cyu) || (!va8 && va10 && !cyu) || (!va9 && cyl) || (va10 && !va9)));

			cyu = hc0_u == 0xff;
			cyl = hc0_l == 0xff;

			ebls = tnlf && ( eltmp || (tmpm && (!cyl || (va12 && va13 && cyu))));
			ebcs = tnlf && ( ectmp || (tmpm && ((!va14 && !cyl) || (va13 && cyu))));
			ebrs = tnlf && ( ertmp || (tmpm && ((!va15 && !cyl) || (va12 && cyu))));

			/* Carry for horizontal position counters */
			cyu = hc0_u == 0xff;
			cyl = hc0_l == 0xff;

			if (!(bnkls && !rl))
			{
				int a, b;

				if (!febl[0])
					a = (scrcnt0) & 3;
				else
					a = 3;

				if (!febl[1])
					b = (scrcnt1) & 3;
				else
					b = 3;

				tx1_draw_road_pixel(0, bmpaddr,
							&pix[0][a][0], &pix[1][b][0],
							pixnum0, pixnum1,
							stl, sld, selb, bnkls, rorevls, ebls, rl, delrl);
			}
			else
				*(bmpaddr) = (bnkls << 6) | (rl << 5);

			if (!(bnkcs && !rc))
			{
				int a, b;

				if (!febc[0])
					a = (scrcnt0 + 1) & 3;
				else
					a = 3;

				if (!febc[1])
					b = (scrcnt1 + 1) & 3;
				else
					b = 3;

				tx1_draw_road_pixel(1, bmpaddr,
							&pix[0][a][0], &pix[1][b][0],
							pixnum0, pixnum1,
							stl, sld, selb, bnkcs, rorevcs, ebcs, rc, delrc);
			}
			else
				*(bmpaddr + 256) = (bnkcs << 6) | (rc << 5);

			if (!(bnkrs && !rr))
			{
				int a, b;

				if (!febr[0])
					a = (scrcnt0 + 2) & 3;
				else
					a = 3;

				if (!febr[1])
					b = (scrcnt1 + 2) & 3;
				else
					b = 3;

				tx1_draw_road_pixel(2, bmpaddr,
							&pix[0][a][0], &pix[1][b][0],
							pixnum0, pixnum1,
							stl, sld, selb, bnkrs, rorevrs, ebrs, rr, delrr);
			}
			else
				*(bmpaddr + 512) = (bnkrs << 6) | (rr << 5);

			++bmpaddr;

			/* Update road counters */
			if (!(++road0_pcnt & 7))
				++road0_hcnt;

			if (!(++road1_pcnt & 7))
				++road1_hcnt;

			/* Increment horizontal counters (stop at 0xff) */
			if (hc0_u != 0xff) ++hc0_u;
			if (hc0_l != 0xff) ++hc0_l;
			if (hc1_u != 0xff) ++hc1_u;
			if (hc1_l != 0xff) ++hc1_l;

			/* Update bank counter */
			bank_cnt = (bank_cnt + 1) & 0x7ff;
		}

		vregs.h_val += vregs.h_inc;

		/* Finally, increment the bank accumulator */
		vregs.ba_val = (vregs.ba_val + vregs.ba_inc) & 0x00ffffff;
	}

}

/*************************************
 *
 *  Objects
 *
 *************************************/

static void tx1_draw_objects(UINT8 *bitmap)
{
	/* The many lookup table ROMs */
	const UINT8 *const ic48 = &DrvObjLuts[0];
	const UINT8 *const ic281 = &DrvObjLuts[0x2000];

	const UINT8 *const ic190 = &DrvProms[0xc00];
	const UINT8 *const ic162 = &DrvProms[0xe00];
	const UINT8 *const ic25  = &DrvProms[0x1000];

	const UINT8 *const ic106 = &DrvObjMap[0];
	const UINT8 *const ic73  = &DrvObjMap[0x4000];

	const UINT8 *const pixdata_rgn = &DrvObjROM[0];

	for (UINT32 offs = 0x0; offs < 0x300 / 2; offs += 8)
	{
		UINT32  x;
		UINT32  y;
		UINT32  gxflip;

		UINT32  x_scale;
		UINT32  x_step;
		UINT16  y_scale;
		UINT16  y_step;

		UINT8   pctmp0_7;
		UINT8   code;

		/* Check for end of object list */
		if ((BURN_ENDIAN_SWAP_INT16(objram16[offs]) & 0xff00) == 0xff00)
			break;

		/* X scale */
		x_scale = BURN_ENDIAN_SWAP_INT16(objram16[offs + 2]) & 0xff;

		/* TODO: Confirm against hardware? */
		if (x_scale == 0)
			continue;

		/* 16-bit y-scale accumulator */
		y_scale = BURN_ENDIAN_SWAP_INT16(objram16[offs + 1]);
		y_step  = BURN_ENDIAN_SWAP_INT16(objram16[offs + 3]);

		/* Object number */
		code = BURN_ENDIAN_SWAP_INT16(objram16[offs]) & 0xff;

		/* Attributes */
		pctmp0_7 = BURN_ENDIAN_SWAP_INT16(objram16[offs + 2]) >> 8;

		/* Global x-flip */
		gxflip = (pctmp0_7 & 0x80) >> 7;

		/* Add 1 to account for line buffering */
		y = (BURN_ENDIAN_SWAP_INT16(objram16[offs]) >> 8) + 1;

		for (; y < 240; ++y)
		{
			UINT32  rom_addr2   = 0;
			UINT8   ic106_data  = 0;
			UINT8   ic73_data;

			/* Are we drawing on this line? */

			/* TODO: See big lampposts. */
			if (y_scale & 0x8000)
				break;

			{
				UINT32  psa0_11;
				UINT32  ic48_addr;
				UINT32  ic48_data;
				UINT32  rom_addr;
				UINT32  x_acc;
				UINT32  newtile = 1;
				UINT32  dataend = 0;
				UINT8   data1 = 0;
				UINT8   data2 = 0;
				UINT32  xflip = 0;
				UINT32  opcd0_7 = 0;
				UINT32  lasttile = 0;

				/* Use the object code to lookup the tile sequence data in ROM */
				ic48_addr = code << 4;
				ic48_addr |= ((y_scale >> 11) & 0xf);
				ic48_data = ic48[ic48_addr];

				/* Reached the bottom of the object? (/PASS2E) */
				if (ic48_data == 0xff)
					break;

				/* Combine ROM and PROM data */
				psa0_11 = ((ic25[code] << 8) | ic48_data) & 0xfff;

				/* psa8_11 */
				rom_addr = (psa0_11 & ~0xff) << 2;

				/* Prepare the x-scaling */
				x_step = (128 << OBJ_FRAC) / x_scale;
				x_acc = (psa0_11 & 0xff) << (OBJ_FRAC + 5);

#define TX1_MASK    0xfff

				x = BURN_ENDIAN_SWAP_INT16(objram16[offs + 4]) & TX1_MASK;

				for (;;)
				{
					if (newtile)
					{
						UINT32  psbb0_12;
						UINT32  pscb0_14;
						UINT32  pscb11;
						const UINT8 *romptr;
						UINT32  ic281_addr;
						UINT32  grom_addr;
						UINT32  lut_data;
						UINT32  low_addr = ((x_acc >> (OBJ_FRAC + 3)) & TX1_MASK);

						if (gxflip)
						{
							UINT32 xor_mask;

							if (BIT(psa0_11, 11) && BIT(psa0_11, 10))
								xor_mask = 0xf;
							else if (BIT(psa0_11, 11) || BIT(psa0_11, 10) || BIT(psa0_11, 9))
								xor_mask = 0x7;
							else
								xor_mask = 0x3;

							rom_addr2 = rom_addr + (xor_mask ^ low_addr);
						}
						else
							rom_addr2 = rom_addr + low_addr;

						ic106_data = ic106[rom_addr2 & 0x3fff];

						if ((ic106_data & 0x40) && dataend)
							lasttile = 1;

						dataend |= ic106_data & 0x40;

						/* Retrieve data for an 8x8 tile */
						ic73_data = ic73[rom_addr2 & 0x3fff];

						/* This is the data from the LUT pair */
						lut_data = (ic106_data << 8) | ic73_data;
						psbb0_12 = lut_data & 0x1fff;

						pscb0_14 = (psbb0_12 & 0xc3f);

						/* Bits 9_6 are from PCTMP11-8 or PSBB9-6 */
						if (BIT(psbb0_12, 12))
							pscb0_14 |= psbb0_12 & 0x3c0;
						else
							pscb0_14 |= (pctmp0_7 & 0xf) << 6;

						if (BIT(lut_data, 13))
							pscb0_14 |= BIT(psbb0_12, 10) << 12;
						else
							pscb0_14 |= ((pctmp0_7 & 0x70) << 8);

						/* Bit 12 is bit 10 duplicated. */
						pscb0_14 &= ~(1 << 12);
						pscb0_14 |= BIT(psbb0_12, 10) << 12;

						pscb11 = BIT(pscb0_14, 11);

						/* TODO: Remove this - it's constant. */
						romptr = (pixdata_rgn + pscb11 * (0x4000 * 2));

						grom_addr = ((pscb0_14 << 3) | ((y_scale >> 8) & 7)) & 0x3fff;

						/* Get raw 8x8 2bpp pixel row data */
						data1 = *(grom_addr + romptr);
						data2 = *(grom_addr + romptr + 0x4000);

						/* Determine flip state (global XOR local) */
						xflip = gxflip ^ !BIT(lut_data, 15);

						ic281_addr = pscb0_14 & 0x3ff;
						ic281_addr |= ((pscb0_14 & 0x7000) >> 2);
						ic281_addr |= pscb11 << 13;

						opcd0_7 = ic281[ic281_addr];

						newtile = 0;
					}

					/* Draw a pixel? */
					if (x < 768)
					{
						UINT8   pix;
						UINT8   bit;

						bit = (x_acc >> OBJ_FRAC) & 7;

						if (xflip)
							bit ^= 7;

						pix = (((data1 >> bit) & 1) << 1) | ((data2 >> bit) & 1);

						/* Draw pixel, if not transparent */
						if ( !(!(opcd0_7 & 0x80) && !pix) )
						{
							UINT8 color;
							UINT32 prom_addr;

							prom_addr = ((opcd0_7 << 2) | pix) & 0x1ff;

							/* Inverted on schematic */
							if (x & 1)
								color = ~ic190[prom_addr] & 0x3f;
							else
								color = ~ic162[prom_addr] & 0x3f;

							*(bitmap + 768*y + x) = 0x40 | color;
						}
					}

					/* Check if we've stepped into a new 8x8 tile */
					if ((((x_acc + x_step) >> (OBJ_FRAC + 3)) & TX1_MASK) != ((x_acc >> (OBJ_FRAC + 3)) & TX1_MASK))
					{
						if (lasttile)
							break;

						newtile = 1;
					}

					x = (x + 1) & TX1_MASK;
					x_acc += x_step;
				}
			}
			y_scale += y_step;
		}
	}
}

static void buggyboy_draw_char(UINT8 *bitmap, bool wide)
{
	UINT32 scroll_x, scroll_y;
	UINT32 total_width;
	UINT32 x_mask;

	/* 2bpp characters */
	const UINT8 *const chars = &DrvCharROM[0];
	const UINT8 *const gfx2 = &DrvCharROM[0x4000];

	/* X/Y scroll values are the last word in char RAM */
	if (wide)
	{
		scroll_y = (BURN_ENDIAN_SWAP_INT16(vram16[0xfff]) >> 10) & 0x3f;
		scroll_x = BURN_ENDIAN_SWAP_INT16(vram16[0xfff]) & 0x3ff;
		total_width = 768;
		x_mask = 0x3ff;
	}
	else
	{
		scroll_y = (BURN_ENDIAN_SWAP_INT16(vram16[0x7ff]) >> 10) & 0x3f;
		scroll_x = BURN_ENDIAN_SWAP_INT16(vram16[0x7ff]) & 0x1ff;
		total_width = 256;
		x_mask = 0x1ff;
	}

	for (INT32 y = 0; y < 240; ++y)
	{
		UINT32 d0 = 0, d1 = 0;
		UINT32 colour = 0;
		UINT32 y_offs;
		UINT32 x_offs;
		UINT32 y_gran;

		/* There's no y-scrolling between scanlines 0 and 63 */
		if (y < 64)
			y_offs = y;
		else
		{
			y_offs = (y + (scroll_y | 0xc0) + 1) & 0xff;

			/* Clamp */
			if (y_offs < 64)
				y_offs |= 0xc0;
		}

		if ((y_offs >= 64) && (y_offs < 128))
			x_offs = scroll_x;
		else
			x_offs = 0;


		y_gran = y_offs & 7;

		if (x_offs & 7)
		{
			UINT32 tilenum;
			UINT16 ram_val;

			if (wide)
				ram_val = BURN_ENDIAN_SWAP_INT16(vram16[((y_offs << 4) & 0xf80) + ((x_offs >> 3) & 0x7f)]);
			else
				ram_val = BURN_ENDIAN_SWAP_INT16(vram16[((y_offs << 3) & 0x7c0) + ((x_offs >> 3) & 0x3f)]);

			tilenum = (ram_val & 0x03ff) | ((ram_val & 0x8000) >> 5);
			colour = (ram_val & 0xfc00) >> 8;
			d0 = *(gfx2 + (tilenum << 3) + y_gran);
			d1 = *(chars + (tilenum << 3) + y_gran);
		}

		for (INT32 x = 0; x < total_width; ++x)
		{
			UINT32 x_gran = x_offs & 7;

			if (!x_gran)
			{
				UINT32 tilenum;
				UINT16 ram_val;

				if (wide)
					ram_val = BURN_ENDIAN_SWAP_INT16(vram16[((y_offs << 4) & 0xf80) + ((x_offs >> 3) & 0x7f)]);
				else
					ram_val = BURN_ENDIAN_SWAP_INT16(vram16[((y_offs << 3) & 0x7c0) + ((x_offs >> 3) & 0x3f)]);

				tilenum = (ram_val & 0x03ff) | ((ram_val & 0x8000) >> 5);
				colour = (ram_val & 0xfc00) >> 8;
				d0 = *(gfx2 + (tilenum << 3) + y_gran);
				d1 = *(chars + (tilenum << 3) + y_gran);
			}

			*bitmap++ = colour |
						(((d1 >> (7 ^ x_gran)) & 1) << 1) |
						((d0 >> (7 ^ x_gran)) & 1);

			x_offs = (x_offs + 1) & x_mask;
		}
	}
}

static void buggyboy_get_roadpix(int screen, int ls161, UINT8 rva0_6, UINT8 sld, UINT32 *_rorev,
									UINT8 *rc0, UINT8 *rc1, UINT8 *rc2, UINT8 *rc3)
{
	/* Counter Q10-7 are added to 384 */
	UINT16 ls283_159 = (ls161 & 0x780) + 128 + (256 * screen);
	UINT32 ls283_159_co = ls283_159 & 0x800;
	UINT32 rom_flip = ls283_159 & 0x200 ? 0 : 1;
	UINT32 rom_en = !(ls283_159 & 0x400) && !(ls283_159_co ^ (ls161 & 0x800));
	UINT8 d0 = 0;
	UINT8 d1 = 0;

	/* ROM/PROM lookup tables */
	const UINT8 *const rom   = &DrvRoadROM[0];
	const UINT8 *const prom0 = &DrvRoadROM[0x4000];
	const UINT8 *const prom1 = &DrvRoadROM[0x4200];
	const UINT8 *const prom2 = &DrvRoadROM[0x4400];

	/* Latch road reverse bit */
	*_rorev = !( (rom_en && rom_flip) || (!rom_en && (ls161 & 0x4000)) );

	/* TODO: ROM data is 0xff if not enabled. */
	if (rom_en)
	{
		UINT8  rom_data;
		UINT16 prom_addr;

		/* 6 bit road horizontal address */
		UINT16 rha = (ls283_159 & 0x180) | (ls161 & 0x78);

		if (rom_flip)
			rha ^= 0x1f8;

		/* Get road chunk first */
		rom_data = rom[(1 << 13) | (rha << 4) | rva0_6];
		prom_addr = (rom_flip ? 0x80 : 0) | (rom_data & 0x7f);

		*rc0 = prom0[prom_addr];
		*rc1 = prom1[prom_addr];
		*rc2 = prom2[prom_addr];

		/* Now get the dirt chunk */
		rom_data = rom[(rha << 4) | rva0_6];
		prom_addr = 0x100 | rom_data;

		d0 = prom0[prom_addr];
		d1 = prom1[prom_addr];
	}
	else
	{
		/*
		    TODO: When ROM is not enabled, data = 0xff
		    But does anybody care?
		*/
		*rc0 = *rc1 = *rc2 = *rc3 = 0;
	}

	/* The data is mixed by two TZ0314 PALs */
	if (BIT(sld, 4))
	{
		if (BIT(sld, 5))
			d1 = ~d1;

		*rc3 = d0 & d1;

		if (rom_flip)
			*rc3 = BITSWAP08(*rc3, 0, 1, 2, 3, 4, 5, 6, 7);
	}
	else
		*rc3 = 0;
}

#define LOAD_HPOS_COUNTER(NUM)                                                  \
	ram_val = BURN_ENDIAN_SWAP_INT16(rcram16[(rva_offs + 0x1f8 + (2*NUM)) >> 1]);                \
	rcrs10 = ram_val & 0xfc00 ? 0x0400 : 0x0000;                                \
	hp = vregs.wa8 + ((BIT(ram_val, 15) << 11) | rcrs10 | (ram_val & 0x03ff));  \
	hp##NUM = hp & 0xff;                                                        \
	hp >>= 8;                                                                   \
	hps##NUM##0 = (BIT(hp, 0) || BIT(hp, 2)) && !BIT(hp, 3);                    \
	hps##NUM##1 = (BIT(hp, 1) || BIT(hp, 2)) && !BIT(hp, 3);                    \
	hps##NUM##2 = BIT(hp, 2);
#define UPDATE_HPOS(NUM)                \
	if (hp##NUM##_en)                   \
	{                                   \
		if ((hp##NUM & 0xff) == 0xff)   \
			hp##NUM##_cy = 1;           \
		else                            \
			hp##NUM = hp##NUM + 1;      \
	}
static void buggyboy_draw_road(UINT8 *bitmap)
{
	UINT16 rva_offs;
	UINT32 tnlmd0;
	UINT32 tnlmd1;
	UINT32 linf;
	UINT32 tnlf;
	UINT32 wangl;
	UINT32 tcmd;
	UINT32 wave0;
	UINT32 wave1;
	UINT32 rva20_6;

	/* ROM/PROM lookup tables */
	const UINT8 *const rcols = &DrvProms[0x1500];
	const UINT8 *const vprom = &DrvRoadROM[0x4600];

	/* Extract constant values */
	tcmd     = ((vregs.scol & 0xc000) >> 12) | ((vregs.scol & 0x00c0) >> 6);
	tnlmd0   = BIT(vregs.flags, BB_RDFLAG_TNLMD0);
	tnlmd1   = BIT(vregs.flags, BB_RDFLAG_TNLMD1);
	linf     = BIT(vregs.flags, BB_RDFLAG_LINF);
	tnlf     = BIT(vregs.flags, BB_RDFLAG_TNLF);
	wangl    = BIT(vregs.flags, BB_RDFLAG_WANGL);
	wave0    = BIT(vregs.flags, BB_RDFLAG_WAVE0);
	wave1    = BIT(vregs.flags, BB_RDFLAG_WAVE1);
	rva_offs = BIT(vregs.flags, BB_RDFLAG_RVA7) ? 0x800 : 0xc00;

	for (UINT32 y = 0; y < 240; ++y)
	{
		UINT8   rva0_6;
		UINT8   ram_addr;
		UINT16  rcrdb0_15;
		UINT16  rcrs10;
		UINT16  ls161_156_a;
		UINT16  ls161;
		UINT8   sld;
		UINT32  rva8;
		UINT32  rm0, rm1;
		UINT32  rcmd;
		UINT32  bnkls = 1;
		UINT32  bnkcs = 1;
		UINT32  bnkrs = 1;

//      UINT32  x_offs;
		UINT8 sf;

		/* Vertical positions shift register */
		UINT32  ram_val;
		UINT32  hp;
		UINT32  vp1, vp2, vp3, vp4, vp5, vp6, vp7;

		/* Horizontal positions */
		UINT32  hp0, hp1, hp2, hp3;
		UINT8   hps00, hps01, hps02;
		UINT8   hps10, hps11, hps12;
		UINT8   hps20, hps21, hps22;
		UINT8   hps30, hps31, hps32;

		/* Road pixel data planes */
		UINT8   rc0[3] = {0, 0, 0};
		UINT8   rc1[3] = {0, 0, 0};
		UINT8   rc2[3] = {0, 0, 0};
		UINT8   rc3[3] = {0, 0, 0};

		/* Horizontal position counter carry out */
		UINT8   hp0_cy = 0, hp1_cy = 0, hp2_cy = 0, hp3_cy = 0;

		UINT8   *bmpaddr = bitmap + (y * 256 * 3);

		UINT32  bank_cnt;
		UINT32  _rorevls = 0;
		UINT32  _rorevcs = 0;
		UINT32  _rorevrs = 0;

		UINT32  ic96_o17;
		UINT32  ic96_term1;
		UINT32  ic97_o12;
		UINT32  ic97_o13;
		UINT32  ic79_p19;

		rva8 = (vregs.h_val & 0x8000) || !(vregs.shift & 0x80);

		/* Get RVA0_6 from TZ113 accumulator chain @ 122/123 */
		rva0_6 = (vregs.h_val >> 7) & 0x7f;

		/* For /WAVE bit logic later */
		rva20_6 = ((rva0_6 >> 3) & 0xe) | ((rva0_6 & 2) >> 1);

		/* RVA is inverted! */
		ram_addr = (~rva0_6 & 0x7f) << 1;

		/* Get the road RAM data for this line */
		rcrdb0_15 = BURN_ENDIAN_SWAP_INT16(rcram16[(rva_offs + ram_addr) >> 1]);

		/* If 15-10 == 000000, then 0 */
		rcrs10 = rcrdb0_15 & 0xfc00 ? 0x0400 : 0x0000;

		/* If 15-10 == 111111, then 1 */
		ls161_156_a = (rcrdb0_15 & 0xfc00) == 0xfc00 ? 0x800 : 0x0000;

		/* LS161 15-bit counter chain - loaded with RAM data (bar bits 10-13) */
		ls161 =  ((rcrdb0_15 & 0x8000) >> 1) | ls161_156_a | rcrs10 | (rcrdb0_15 & 0x03ff);

		/* SLD */
		sld = (vprom[rva0_6] + vregs.slin_val) & 0x38;

		/* Determine the x-offset */
//      x_offs = ls161 & 7;

		/* Fill vertical position shift register with bits for this line */
		/* TODO; cheated slightly to shift stuff up one pixel*/
		vp1 = BURN_ENDIAN_SWAP_INT16(rcram16[(rva_offs + 0x1e2) >> 1]) >= y ? 0 : 1;
		vp2 = BURN_ENDIAN_SWAP_INT16(rcram16[(rva_offs + 0x1e4) >> 1]) >= y ? 0 : 1;
		vp3 = BURN_ENDIAN_SWAP_INT16(rcram16[(rva_offs + 0x1e6) >> 1]) >= y ? 0 : 1;
		vp4 = BURN_ENDIAN_SWAP_INT16(rcram16[(rva_offs + 0x1e8) >> 1]) >= y ? 0 : 1;
		vp5 = BURN_ENDIAN_SWAP_INT16(rcram16[(rva_offs + 0x1ea) >> 1]) >= y ? 0 : 1;
		vp6 = BURN_ENDIAN_SWAP_INT16(rcram16[(rva_offs + 0x1ec) >> 1]) >= y ? 0 : 1;
		vp7 = BURN_ENDIAN_SWAP_INT16(rcram16[(rva_offs + 0x1ee) >> 1]) >= y ? 0 : 1;

		/* Stuff */
		rm0 = vp7 ? BIT(vregs.scol, 4) : BIT(vregs.scol, 12);
		rm1 = vp7 ? BIT(vregs.scol, 5) : BIT(vregs.scol, 13);

		/* Wall/tunnel control */
		rcmd = (vp7 ? vregs.scol : vregs.scol >> 8) & 0xf;

		/* Load 'em up */
		LOAD_HPOS_COUNTER(0);
		LOAD_HPOS_COUNTER(1);
		LOAD_HPOS_COUNTER(2);
		LOAD_HPOS_COUNTER(3);

		/* Load the bank counter with accumulator bits 14-5 */
		bank_cnt = (vregs.ba_val >> 5) & 0x3ff;

		/* Have we crossed a road gfx strip boundary? */
		if (ls161 & 7)
		{
			buggyboy_get_roadpix(0, ls161, rva0_6, sld, &_rorevls, &rc0[0], &rc1[0], &rc2[0], &rc3[0]);
			buggyboy_get_roadpix(1, ls161, rva0_6, sld, &_rorevcs, &rc0[1], &rc1[1], &rc2[1], &rc3[1]);
			buggyboy_get_roadpix(2, ls161, rva0_6, sld, &_rorevrs, &rc0[2], &rc1[2], &rc2[2], &rc3[2]);
		}

		/* We can evaluate some of the pixel logic outside of the x-loop */
		ic96_term1 = !tnlf || (vp5 && vp6) || (!vp3 && vp6) || (!vp4 && vp5) || (!vp3 && !vp4);
		ic96_o17 = (vp3 && vp4) || tnlmd0 || tnlmd1 || !vp1 || !tnlf;

		ic97_o12 = (!vp1 && !vp2 && !vp6) || (!vp1 && !vp2 && vp7) || (vp4 && !vp6) || (vp4 && vp7);
		ic97_o13 = (!vp1 && !vp2 && !vp5) || (!vp1 && !vp2 && vp7) || (vp3 && !vp5) || (vp3 && vp7);

		ic79_p19 = !(!(vp5 || vp6) || vp7) && !(tnlmd0 || tnlmd1);

		for (INT32 x = 0; x < 256; ++x)
		{
			UINT32  pix;
			UINT32  hp0_en, hp1_en, hp2_en, hp3_en;

			UINT32  ic97_o17;
			UINT32  ic97_o18;
			UINT32  ic97_o19;

			UINT32  ic96_o14;
			UINT32  ic96_o15;
			UINT32  ic96_o16;

			UINT32  ic79_o15;
			UINT32  ic79_o16;
			UINT32  ic79_o17;

			UINT32  ic82_o17;
			UINT32  ic82_o16;
			UINT32  ic82_o15;
			UINT32  ic82_o14;

			UINT32  ic80_o17;
			UINT32  ic80_o16;
			UINT32  ic80_o15;
			UINT32  ic80_o14;

			UINT32  ic78_o17;
			UINT32  ic78_o16;
			UINT32  ic78_o15;
			UINT32  ic78_o14;

			UINT32  ic48_o12, ic50_o12, ic52_o12;
			UINT32  ic48_o16, ic50_o16, ic52_o16;
			UINT32  ic48_o17, ic50_o17, ic52_o17;
			UINT32  ic48_o18, ic50_o18, ic52_o18;
			UINT32  ic48_o19, ic50_o19, ic52_o19;

			UINT32  tmp;

			UINT8   px0[3];
			UINT8   px1[3];
			UINT8   px2[3];
			UINT8   px3[3];

			UINT32 lfsr = vregs.wave_lfsr;
			UINT32 wave =
						(wave0 ^ BIT(lfsr, 0))
						&& (wave1 ^ BIT(lfsr, 3))
						&& BIT(lfsr, 5)
						&& !BIT(lfsr, 15)
						&& BIT(lfsr, 11)
						&& BIT(lfsr, 13)
						&& (rva20_6 < ((lfsr >> 8) & 0xf));


			/* Strip pixel number */
			pix = (ls161 & 7) ^ 7;

			/* Horizontal position counter enables - also used as PAL inputs */
			hp0_en = !(hp0_cy || hps02);
			hp1_en = !(hp1_cy || hps12);
			hp2_en = !(hp2_cy || hps22);
			hp3_en = !(hp3_cy || hps32);

			/* Load in a new road gfx strip? */
			if (!(ls161 & 7))
			{
				buggyboy_get_roadpix(0, ls161, rva0_6, sld, &_rorevls, &rc0[0], &rc1[0], &rc2[0], &rc3[0]);
				buggyboy_get_roadpix(1, ls161, rva0_6, sld, &_rorevcs, &rc0[1], &rc1[1], &rc2[1], &rc3[1]);
				buggyboy_get_roadpix(2, ls161, rva0_6, sld, &_rorevrs, &rc0[2], &rc1[2], &rc2[2], &rc3[2]);
			}

			/* Road camber/banking */
			if (BIT(vregs.ba_val, 23))
			{
				bnkls = 1; bnkcs = 1; bnkrs = 1;
			}
			else if (vregs.ba_val & 0x007f8000)
			{
				bnkls = 0; bnkcs = 0; bnkrs = 0;
			}
			else
			{
				bnkls = bank_cnt < 0x400;
				bnkcs = bank_cnt < 0x300;
				bnkrs = bank_cnt < 0x200;
			}

			if (vregs.bank_mode)
			{
				bnkls ^= 1; bnkcs ^= 1; bnkrs ^= 1;
			}

			px0[0] = BIT(rc0[0], pix);
			px1[0] = BIT(rc1[0], pix);
			px2[0] = BIT(rc2[0], pix);
			px3[0] = BIT(rc3[0], pix);

			px0[1] = BIT(rc0[1], pix);
			px1[1] = BIT(rc1[1], pix);
			px2[1] = BIT(rc2[1], pix);
			px3[1] = BIT(rc3[1], pix);

			px0[2] = BIT(rc0[2], pix);
			px1[2] = BIT(rc1[2], pix);
			px2[2] = BIT(rc2[2], pix);
			px3[2] = BIT(rc3[2], pix);

			/*
			    Now evaluate the pixel logic for each of the three screens

			    TODO: A lot of this could be macrofied to avoid repetition.
			    Shuffling the equations around would squeeze out some extra speed.
			*/

			/* Left */
			ic96_o14 =
			ic96_term1
			|| (!hp1_en && hps10 && vp4 && !hps20 && !hps21)
			|| (!hp1_en && hps10 && vp4 && hp2_en && !hps21)
			|| (!hp1_en && hps10 && vp4 && vp6)
			|| (hps11 && vp4 && !hps20 && !hps21)
			|| (hps11 && vp4 && hp2_en && !hps21)
			|| (vp5 && !hps20 && !hps21)
			|| (!vp3 && !hps20 && !hps21)
			|| (vp5 && hp2_en && !hps21)
			|| (!vp3 && hp2_en && !hps21)
			|| (!hp1_en && hps10 && !vp4)
			|| (hps11 && vp4 && vp6)
			|| (hps11 && !vp4);

			/* Centre */
			ic96_o15 =
			ic96_term1
			|| (hps10 && hps11 && vp4 && hp2_en && !hps20)
			||(!hp1_en && hps11 && vp4 && hp2_en && !hps20)
			|| (hps10 && hps11 && vp6)
			|| (!hp1_en && hps11 && vp6)
			|| (hps10 && hps11 && vp4 && !hps21)
			|| (!hp1_en && hps11 && vp4 && !hps21)
			|| (vp5 && hp2_en && !hps20)
			|| (!vp3 && hp2_en && !hps20)
			|| (hps10 && hps11 && !vp4)
			|| (!hp1_en && hps11 && !vp4)
			|| (vp5 && !hps21)
			|| (!vp3 && !hps21);

			/* Right */
			ic96_o16 =
			ic96_term1
			|| (!hp1_en && hps10 && hps11 && vp6)
			|| (!hp1_en && hps10 && hps11 && !vp4)
			|| (!hp1_en && hps10 && hps11 && !hps20)
			|| (!hp1_en && hps10 && hps11 && hp2_en)
			|| (!hp1_en && hps10 && hps11 && !hps21)
			|| (vp5 && !hps20)
			|| (!vp3 && !hps20)
			|| (vp5 && hp2_en)
			|| (!vp3 && hp2_en)
			|| (vp5 && !hps21)
			|| (!vp3 && !hps21);

			tmp = (!vp1 && !vp2) || (vp2 && vp7);
			ic97_o17 = tmp || (!hp0_en && hps00 && vp2 && !hps30 && !hps31) || (!hp0_en && hps00 && vp2 && hp3_en && !hps31) || (hps01 && vp2 && !hps30 && !hps31) || (hps01 && vp2 && hp3_en && !hps31);
			ic97_o18 = tmp || (hps00 && hps01 && vp2 && hp3_en && !hps30) || (!hp0_en && hps01 && vp2 && hp3_en && !hps30) || (hps00 && hps01 && vp2 && !hps31) || (!hp0_en && hps01 && vp2 && !hps31);
			ic97_o19 = tmp || (!hp0_en && hps00 && hps01 && vp2 && !hps30) || (!hp0_en && hps00 && hps01 && vp2 && hp3_en) || (!hp0_en && hps00 && hps01 && vp2 && !hps31);


			/* Left */
			{
			UINT32 P4, P6, P7, P18, P19;

			P4 = !(ic96_o16 && ic96_o17 && ic97_o19);
			P6 = BIT(sld, 3);
			P7 = (!vp5 && !vp6) || vp7 || !linf;
			P19 = wangl && bnkls;

			ic78_o17 = (_rorevls && !tnlmd1 && tnlmd0) || (!_rorevls && tnlmd1 && !tnlmd0) || (_rorevls && ic97_o12) || (!_rorevls && ic97_o13);
			P18 = ic78_o17;
			ic78_o16 = P7 || (px1[0] && !px0[0] && tnlmd1 && !tnlmd0) || (px2[0] && tnlmd1 && tnlmd0);
			ic78_o15 = !tnlf || (!P4 && px0[0] && P19) || (!P4 && px1[0] && P19) || (!P4 && P18) || (!P4 && px2[0]);
			ic78_o14 = !P6 || tnlmd0 || tnlmd1 || P7;
			}

			/* Centre */
			{
			UINT32 P4, P6, P7, P18, P19;

			P4 = !(ic96_o15 && ic96_o17 && ic97_o18);
			P6 = BIT(sld, 3);
			P7 = (!vp5 && !vp6) || vp7 || !linf;
			P19 = wangl && bnkcs;

			ic80_o17 = (_rorevcs && !tnlmd1 && tnlmd0) || (!_rorevcs && tnlmd1 && !tnlmd0) || (_rorevcs && ic97_o12) || (!_rorevcs && ic97_o13);
			P18 = ic80_o17;
			ic80_o16 = P7 || (px1[1] && !px0[1] && tnlmd1 && !tnlmd0) || (px2[1] && tnlmd1 && tnlmd0);
			ic80_o15 = !tnlf || (!P4 && px0[1] && P19) || (!P4 && px1[1] && P19) || (!P4 && P18) || (!P4 && px2[1]);
			ic80_o14 = !P6 || tnlmd0 || tnlmd1 || P7;
			}

			/* Right */
			{
			UINT32 P4, P6, P7, P18, P19;

			P4 = !(ic96_o14 && ic96_o17 && ic97_o17);
			P6 = BIT(sld, 3);
			P7 = (!vp5 && !vp6) || vp7 || !linf;
			P19 = wangl && bnkrs;

			ic82_o17 = (_rorevrs && !tnlmd1 && tnlmd0) || (!_rorevrs && tnlmd1 && !tnlmd0) || (_rorevrs && ic97_o12) || (!_rorevrs && ic97_o13);
			P18 = ic82_o17;
			ic82_o16 = P7 || (px1[2] && !px0[2] && tnlmd1 && !tnlmd0) || (px2[2] && tnlmd1 && tnlmd0);
			ic82_o15 = !tnlf || (!P4 && px0[2] && P19) || (!P4 && px1[2] && P19) || (!P4 && P18) || (!P4 && px2[2]);
			ic82_o14 = !P6 || tnlmd0 || tnlmd1 || P7;
			}


			ic79_o17 = (!px2[0] && _rorevls && ic78_o15) || (tnlf && !ic97_o19) || (tnlf && ic79_p19 && px2[0] && ic78_o15);
			ic79_o16 = (!px2[1] && _rorevcs && ic80_o15) || (tnlf && !ic97_o18) || (tnlf && ic79_p19 && px2[1] && ic80_o15);
			ic79_o15 = (!px2[2] && _rorevrs && ic82_o15) || (tnlf && !ic97_o17) || (tnlf && ic79_p19 && px2[2] && ic82_o15);


			/* Left */
			{
			UINT32 P5,P7, P8;
			UINT32 rcsd0_3;
			UINT32 cprom_addr;

			P5 = BIT(tcmd, 3) ? ((!vp5 && !vp6) || vp7 || !linf) : ic78_o16;
			P7 = BIT(sld, 5);
			P8 = BIT(sld, 4);

			ic48_o19 = (px2[0] && !rva8) || !bnkls || !P5 || !ic78_o15;

			if (ic48_o19)
			{
				ic48_o12 =
				ic78_o14 &&
				(
					(px2[0] && px1[0] && px0[0] && rm1 && !rm0)
					|| (!px2[0] && px1[0] && px0[0] && !P8 && rm0)
					|| (px2[0] && px0[0] && !P7 && !rm1 && !rm0)
					|| (px2[0] && !px1[0] && px0[0] && !P7 && !rm1)
					|| (px2[0] && px1[0] && px0[0] && !P7 && !P8)
					|| (px2[0] && px1[0] && px0[0] && !P8 && rm1)
					|| (!px2[0] && !px3[0] && !rm0)
					|| (!px1[0] && !px3[0] && rm1)
					|| (!px2[0] && !px1[0] && !px3[0])
					|| (rva8)
					|| (!px0[0] && !px3[0])
					|| (!ic78_o15)
					|| (!P5)
				);

				ic48_o16 = (px2[0] && P5 && rm1) || (P5 && rva8 && ic78_o15) || (!px0[0] && P5) || !ic78_o15;
				ic48_o17 = (px0[0] && P5 && !rm0 && ic78_o15) || (!px1[0] && P5 && ic78_o15) || (P5 && rva8 && ic78_o15);
				ic48_o18 = (!px2[0] && P5 && ic78_o15) || (P5 && rva8 && ic78_o15);

				if (vp6 || ic78_o16)
				{
					if (!(ic78_o15 && P5))
						cprom_addr = (tcmd & 0x7) | (ic78_o16 << 3);
					else
						cprom_addr = rcmd;

					cprom_addr = (~cprom_addr & 0xf) << 4;
				}
				else
					cprom_addr = 0xf0;

				cprom_addr |= (ic79_o17 << 3) |
								(ic48_o18 << 2) |
								(ic48_o17 << 1) |
								ic48_o16;

				rcsd0_3 = rcols[cprom_addr] & 0xf;
				*(bmpaddr + 0) = 0x40 | (!wave << 5) | (ic48_o12 << 4) | rcsd0_3;
			}
			else
				*(bmpaddr + 0) = 0;
			}

			/* Centre */
			{
			UINT32 rcsd0_3;
			UINT32 cprom_addr;
			UINT32 P5, P7, P8;

			P5 = BIT(tcmd, 3) ? ((!vp5 && !vp6) || vp7 || !linf) : ic80_o16;
			P7 = BIT(sld, 5);
			P8 = BIT(sld, 4);

			ic50_o19 = (px2[1] && !rva8) || !bnkcs || !P5 || !ic80_o15;

			if (ic50_o19)
			{
				if (ic80_o14)
					ic50_o12 =
					(px2[1] && px1[1] && px0[1] && rm1 && !rm0)
					|| (!px2[1] && px1[1] && px0[1] && !P8 && rm0)
					|| (px2[1] && px0[1] && !P7 && !rm1 && !rm0)
					|| (px2[1] && !px1[1] && px0[1] && !P7 && !rm1)
					|| (px2[1] && px1[1] && px0[1] && !P7 && !P8)
					|| (px2[1] && px1[1] && px0[1] && !P8 && rm1)
					|| (!px2[1] && !px3[1] && !rm0)
					|| (!px1[1] && !px3[1] && rm1)
					|| (!px2[1] && !px1[1] && !px3[1])
					|| (rva8)
					|| (!px0[1] && !px3[1])
					|| (!ic80_o15)
					|| (!P5);
				else
					ic50_o12 = 0;


				ic50_o16 = (P5 && px2[1] && rm1) || (P5 && rva8 && ic80_o15) || (P5 && !px0[1]) || !ic80_o15;
				ic50_o17 = (P5 && px0[1] && !rm0 && ic80_o15) || (P5 && !px1[1] && ic80_o15) || (P5 && rva8 && ic80_o15);
				ic50_o18 = (P5 && !px2[1] && ic80_o15) || (P5 && rva8 && ic80_o15);

				if (vp6 || ic80_o16)
				{
					if (!(ic80_o15 && P5))
						cprom_addr = (tcmd & 0x7) | (ic80_o16 << 3);
					else
						cprom_addr = rcmd;

					cprom_addr = ((~cprom_addr) & 0xf) << 4;
				}
				else
					cprom_addr = 0xf0;

				cprom_addr |= (ic79_o16 << 3) |
								(ic50_o18 << 2) |
								(ic50_o17 << 1) |
								ic50_o16;

				rcsd0_3 = rcols[cprom_addr] & 0xf;
				*(bmpaddr + 256) = 0x40 | (!wave << 5) | (ic50_o12 << 4) | rcsd0_3;
			}
			else
				*(bmpaddr + 256) = 0;
			}

			/* Right */
			{
			UINT32 rcsd0_3;
			UINT32 cprom_addr;
			UINT32 P5, P7, P8;

			P5 = BIT(tcmd, 3) ? ((!vp5 && !vp6) || vp7 || !linf) : ic82_o16;
			P7 = BIT(sld, 5);
			P8 = BIT(sld, 4);

			ic52_o19 = (px2[2] && !rva8) || !bnkrs || !P5 || !ic82_o15;

			if (ic52_o19)
			{
				ic52_o12 =
				ic82_o14 &&
				(
					(px2[2] && px1[2] && px0[2] && rm1 && !rm0)
					|| (!px2[2] && px1[2] && px0[2] && !P8 && rm0)
					|| (px2[2] && px0[2] && !P7 && !rm1 && !rm0)
					|| (px2[2] && !px1[2] && px0[2] && !P7 && !rm1)
					|| (px2[2] && px1[2] && px0[2] && !P7 && !P8)
					|| (px2[2] && px1[2] && px0[2] && !P8 && rm1)
					|| (!px2[2] && !px3[2] && !rm0)
					|| (!px1[2] && !px3[2] && rm1)
					|| (!px2[2] && !px1[2] && !px3[2])
					|| (rva8)
					|| (!px0[2] && !px3[2])
					|| (!ic82_o15)
					|| (!P5)
				);

				ic52_o16 = (px2[2] && P5 && rm1) || (P5 && rva8 && ic82_o15) || (!px0[2] && P5) || !ic82_o15;
				ic52_o17 = (px0[2] && P5 && !rm0 && ic82_o15) || (!px1[2] && P5 && ic82_o15) || (P5 && rva8 && ic82_o15);
				ic52_o18 = (P5 && ic82_o15 && !px2[2]) || (P5 && ic82_o15 && rva8);

				if (vp6 || ic82_o16)
				{
					if (!(ic82_o15 && P5))
						cprom_addr = (tcmd & 0x7) | (ic82_o16 << 3);
					else
						cprom_addr = rcmd;

					cprom_addr = (~cprom_addr & 0xf) << 4;
				}
				else
					cprom_addr = 0xf0;

				cprom_addr |= (ic79_o15 << 3) |
								(ic52_o18 << 2) |
								(ic52_o17 << 1) |
								ic52_o16;

				rcsd0_3 = rcols[cprom_addr] & 0xf;

				*(bmpaddr + 512) = 0x40 | (!wave << 5) | (ic52_o12 ? 0x10 : 0) | rcsd0_3;
			}
			else
				*(bmpaddr + 512) = 0;
			}

			/* Now update counters and whatnot */
			++bmpaddr;

			UPDATE_HPOS(0);
			UPDATE_HPOS(1);
			UPDATE_HPOS(2);
			UPDATE_HPOS(3);

			/* Update the wave LFSR */
			vregs.wave_lfsr = (vregs.wave_lfsr << 1) | (BIT(vregs.wave_lfsr, 6) ^ !BIT(vregs.wave_lfsr, 15));

			/* Increment the bank counter */
			bank_cnt = (bank_cnt + 1) & 0x7ff;

			/* X pos */
			ls161 = (ls161 + 1) & 0x7fff;
		}

		/* WANGL active? Update the 8-bit counter */
		if (wangl)
		{
			if (BIT(vregs.flags, BB_RDFLAG_TNLMD0))
				--vregs.wa8;
			else
				++vregs.wa8;
		}

		/* No carry out - just increment */
		if (vregs.wa4 != 0xf)
			++vregs.wa4;
		else
		{
			/* Carry out; increment again on /TMG2S rise */
			if (wangl)
			{
				if (BIT(vregs.flags, BB_RDFLAG_TNLMD0))
					--vregs.wa8;
				else
					++vregs.wa8;
			}
			vregs.wa4 = 1;
		}

		/* Update accumulator */
		vregs.h_val += vregs.h_inc;

		/* Seems correct */
		sf = vregs.shift;

		if ((vregs.shift & 0x80) == 0)
		{
			vregs.shift <<= 1;

			if ((sf & 0x08) == 0)
				vregs.shift |= BIT(vregs.h_val, 15);
		}

		if ((sf & 0x08) && !(vregs.shift & 0x08))
			vregs.h_inc = vregs.gas;

		/* Finally, increment the banking accumulator */
		vregs.ba_val = (vregs.ba_val + vregs.ba_inc) & 0x00ffffff;
	}
}

static void buggybjr_draw_road(UINT8 *bitmap)
{
	UINT16 rva_offs;
	UINT32 tnlmd0;
	UINT32 tnlmd1;
	UINT32 linf;
	UINT32 tnlf;
	UINT32 wangl;
	UINT32 tcmd;
	UINT32 wave0;
	UINT32 wave1;
	UINT32 rva20_6;

	/* ROM/PROM lookup tables */
	const UINT8 *const rcols = &DrvProms[0x1500];
	const UINT8 *const vprom = &DrvRoadROM[0x4600];

	/* Extract constant values */
	tcmd     = ((vregs.scol & 0xc000) >> 12) | ((vregs.scol & 0x00c0) >> 6);
	tnlmd0   = BIT(vregs.flags, BB_RDFLAG_TNLMD0);
	tnlmd1   = BIT(vregs.flags, BB_RDFLAG_TNLMD1);
	linf     = BIT(vregs.flags, BB_RDFLAG_LINF);
	tnlf     = BIT(vregs.flags, BB_RDFLAG_TNLF);
	wangl    = BIT(vregs.flags, BB_RDFLAG_WANGL);
	wave0    = BIT(vregs.flags, BB_RDFLAG_WAVE0);
	wave1    = BIT(vregs.flags, BB_RDFLAG_WAVE1);
	rva_offs = BIT(vregs.flags, BB_RDFLAG_RVA7) ? 0x800 : 0xc00;

	for (UINT32 y = 0; y < 240; ++y)
	{
		UINT8   rva0_6;
		UINT8   ram_addr;
		UINT16  rcrdb0_15;
		UINT16  rcrs10;
		UINT16  ls161_156_a;
		UINT16  ls161;
		UINT8   sld;
		UINT32  rva8;
		UINT32  rm0, rm1;
		UINT32  rcmd;
		UINT32  bnkcs = 1;

//      UINT32  x_offs;
		UINT8   sf;

		/* Vertical positions shift register */
		UINT32  ram_val;
		UINT32  hp;
		UINT32  vp1, vp2, vp3, vp4, vp5, vp6, vp7;

		/* PAL outputs */
		UINT32  ic4_o12;
		UINT32  ic4_o13;
		UINT32  ic149_o15;
		UINT32  ic151_o14;

		/* Horizontal positions */
		UINT32  hp0, hp1, hp2, hp3;
		UINT8   hps00, hps01, hps02;
		UINT8   hps10, hps11, hps12;
		UINT8   hps20, hps21, hps22;
		UINT8   hps30, hps31, hps32;

		/* Road pixel data planes */
		UINT8   rc0 = 0, rc1 = 0, rc2 = 0, rc3 = 0;

		/* Horizontal position counter carry out */
		UINT8   hp0_cy = 0, hp1_cy = 0, hp2_cy = 0, hp3_cy = 0;

		UINT8   *bmpaddr = bitmap + (y * 256);

		UINT32  bank_cnt;
		UINT32  _rorevcs = 0;

		rva8 = (vregs.h_val & 0x8000) || !(vregs.shift & 0x80);

		/* Get RVA0_6 from TZ113 accumulator chain @ 122/123 */
		rva0_6 = (vregs.h_val >> 7) & 0x7f;

		/* For /WAVE bit logic later */
		rva20_6 = ((rva0_6 >> 3) & 0xe) | ((rva0_6 & 2) >> 1);

		/* RVA is inverted! */
		ram_addr = (~rva0_6 & 0x7f) << 1;

		/* Get the road RAM data for this line */
		rcrdb0_15 = BURN_ENDIAN_SWAP_INT16(rcram16[(rva_offs + ram_addr) >> 1]);

		/* If 15-10 == 000000, then 0 */
		rcrs10 = rcrdb0_15 & 0xfc00 ? 0x0400 : 0x0000;

		/* If 15-10 == 111111, then 1 */
		ls161_156_a = (rcrdb0_15 & 0xfc00) == 0xfc00 ? 0x800 : 0x0000;

		/* LS161 15-bit counter chain - loaded with RAM data (bar bits 10-13) */
		ls161 =  ((rcrdb0_15 & 0x8000) >> 1) | ls161_156_a | rcrs10 | (rcrdb0_15 & 0x03ff);

		/* SLD */
		sld = (vprom[rva0_6] + vregs.slin_val) & 0x38;

		/* Determine the x-offset */
//      x_offs = ls161 & 7;

		/* Fill vertical position shift register with bits for this line */
		/* TODO; cheated slightly to shift stuff up one pixel*/
		vp1 = BURN_ENDIAN_SWAP_INT16(rcram16[(rva_offs + 0x1e2) >> 1]) >= y ? 0 : 1;
		vp2 = BURN_ENDIAN_SWAP_INT16(rcram16[(rva_offs + 0x1e4) >> 1]) >= y ? 0 : 1;
		vp3 = BURN_ENDIAN_SWAP_INT16(rcram16[(rva_offs + 0x1e6) >> 1]) >= y ? 0 : 1;
		vp4 = BURN_ENDIAN_SWAP_INT16(rcram16[(rva_offs + 0x1e8) >> 1]) >= y ? 0 : 1;
		vp5 = BURN_ENDIAN_SWAP_INT16(rcram16[(rva_offs + 0x1ea) >> 1]) >= y ? 0 : 1;
		vp6 = BURN_ENDIAN_SWAP_INT16(rcram16[(rva_offs + 0x1ec) >> 1]) >= y ? 0 : 1;
		vp7 = BURN_ENDIAN_SWAP_INT16(rcram16[(rva_offs + 0x1ee) >> 1]) >= y ? 0 : 1;

		/* Stuff */
		rm0 = vp7 ? BIT(vregs.scol, 4) : BIT(vregs.scol, 12);
		rm1 = vp7 ? BIT(vregs.scol, 5) : BIT(vregs.scol, 13);

		/* Wall/tunnel control */
		rcmd = (vp7 ? vregs.scol : vregs.scol >> 8) & 0xf;

		/* Load 'em up */
		LOAD_HPOS_COUNTER(0);
		LOAD_HPOS_COUNTER(1);
		LOAD_HPOS_COUNTER(2);
		LOAD_HPOS_COUNTER(3);

		/* Some PAL equations that we can evaluate outside of the x-loop */
		ic4_o12 = (!vp1 && !vp2 && !vp6) || (!vp1 && !vp2 && vp7) || (vp4 && !vp6) || (vp4 && vp7);
		ic4_o13 = (!vp1 && !vp2 && !vp5) || (!vp1 && !vp2 && vp7) || (vp3 && !vp5) || (vp3 && vp7);
		ic149_o15 = (!vp5 && !vp6) || vp7 || !linf;
		ic151_o14 = !BIT(sld, 3) || tnlmd0 || tnlmd1 || ic149_o15;

		/* Load the bank counter with accumulator bits 14-5 */
		bank_cnt = (vregs.ba_val >> 5) & 0x3ff;

		/* Have we crossed a road gfx strip boundary? */
		if (ls161 & 7)
			buggyboy_get_roadpix(1, ls161, rva0_6, sld, &_rorevcs, &rc0, &rc1, &rc2, &rc3);

		for (INT32 x = 0; x < 256; ++x)
		{
			UINT32  pix;
			UINT32  hp0_en, hp1_en, hp2_en, hp3_en;
			UINT32  ic149_o16;
			UINT32  ic4_o18;
			UINT32  ic3_o15;
			UINT32  ic150_o12 = 0;
			UINT32  ic150_o16;
			UINT32  ic150_o17;
			UINT32  ic150_o18;
			UINT32  ic150_o19;
			UINT32  ic151_o15;
			UINT32  ic151_o16;
			UINT32  ic151_o17;
			UINT32  rcsd0_3 = 0;
			UINT32  sld5 = BIT(sld, 5);
			UINT32  sld4 = BIT(sld, 4);
			UINT32  mux;
			UINT32  cprom_addr;
			UINT8   px0, px1, px2, px3;

			/* Strip pixel number */
			pix = (ls161 & 7) ^ 7;

			/* Horizontal position counter enables - also used as PAL inputs */
			hp0_en = !(hp0_cy || hps02);
			hp1_en = !(hp1_cy || hps12);
			hp2_en = !(hp2_cy || hps22);
			hp3_en = !(hp3_cy || hps32);

			/* Load in a new road gfx strip? */
			if (!(ls161 & 7))
				buggyboy_get_roadpix(1, ls161, rva0_6, sld, &_rorevcs, &rc0, &rc1, &rc2, &rc3);

			/* Road camber */
			if (vregs.bank_mode == 0)
			{
				if (BIT(vregs.ba_val, 23))
					bnkcs = 1;
				else if (vregs.ba_val & 0x007f8000)
					bnkcs = 0;
				else
					bnkcs = bank_cnt < 0x300;
			}
			else
			{
				if (BIT(vregs.ba_val, 23))
					bnkcs = 0;
				else if (vregs.ba_val & 0x007f8000)
					bnkcs = 1;
				else
					bnkcs = bank_cnt >= 0x300;
			}

			px0 = BIT(rc0, pix);
			px1 = BIT(rc1, pix);
			px2 = BIT(rc2, pix);
			px3 = BIT(rc3, pix);

			/* Now go through and evaluate all the pixel logic */
			if (vp2)
				ic4_o18 = (hps00 && hps01 && hp3_en && !hps30)      ||
							(!hp0_en && hps01 && hp3_en && !hps30)  ||
							(hps00 && hps01 && !hps31)              ||
							(!hp0_en && hps01 && !hps31)                ||
							vp7;
			else
				ic4_o18 = !vp1;


			if (tnlf)
				ic3_o15 = (vp4 && !vp6 && !hp2_en && hps21)     ||
							(vp4 && !vp6 && hps20 && hps21)     ||
							(vp1 && !vp4 && !tnlmd1 && !tnlmd0) ||
							(vp1 && !vp3 && !tnlmd1 && !tnlmd0) ||
							(hp1_en && !hps10 && vp3 && !vp5)       ||
							(!hps11 && vp3 && !vp5);
			else
				ic3_o15 = !ic4_o18;

			ic151_o17 = (_rorevcs && !tnlmd1 && tnlmd0)     ||
						(!_rorevcs && tnlmd1 && !tnlmd0)    ||
						(_rorevcs && ic4_o12)               ||
						(!_rorevcs && ic4_o13);

			if (!ic3_o15)
				ic151_o15 = (px0 && (bnkcs && wangl))   ||
							(px1 && (bnkcs && wangl))   ||
							ic151_o17                   ||
							px2                         ||
							!tnlf;
			else
				ic151_o15 = !tnlf;

			ic151_o16 = (px1 && !px0 && tnlmd1 && !tnlmd0)  ||
						(px2 && tnlmd1 && tnlmd0)           ||
						ic149_o15;

			mux = BIT(tcmd, 3) ? ic149_o15 : ic151_o16;

			ic150_o19 = (px2 && !rva8)  ||
						!bnkcs          ||
						!mux            ||
						!ic151_o15;

			/* Don't calculate the pixel colour if not visible */
			if (ic150_o19)
			{
				ic149_o16 = (_rorevcs && !px2 && ic151_o15)                                 ||
							(tnlf && vp5 && !vp7 && px2 && !tnlmd0 && !tnlmd1 && ic151_o15) ||
							(tnlf && vp6 && !vp7 && px2 && !tnlmd0 && !tnlmd1 && ic151_o15) ||
							(tnlf && !ic4_o18);

				ic150_o16 = (px2 && mux && rm1)         ||
							(mux && rva8 && ic151_o15)  ||
							(!px0 && mux)               ||
							!ic151_o15;

				{
					UINT32 a = mux && ic151_o15;

					ic150_o17 = (a && !rm0 && px0)  ||
								(a && !px1)         ||
								(rva8 && a);

					ic150_o18 = (a && !px2) ||
								(rva8 && a);
				}

				if (ic151_o14)
					ic150_o12 = rva8 || !mux || !ic151_o15              ||
								(px2 && px1 && px0 && rm1 && !rm0)      ||
								(!px2 && px1 && px0 && !sld4 && rm0)    ||
								(px2 && px0 && !sld5 && !rm1 && !rm0)   ||
								(px2 && !px1 && px0 && !sld5 && !rm1)   ||
								(px2 && px1 && px0 && !sld5 && !sld4)   ||
								(px2 && px1 && px0 && !sld4 && rm1)     ||
								(!px2 && !px3 && !rm0)                  ||
								(!px1 && !px3 && rm1)                   ||
								(!px2 && !px1 && !px3)                  ||
								(!px0 && !px3);
				else
					ic150_o12 = 0;

				if (vp6 || ic151_o16)
				{
					UINT32 ic150_i5 = BIT(tcmd, 3) ? ic149_o15 : ic151_o16;

					if (!(ic151_o15 && ic150_i5))
						cprom_addr = (tcmd & 0x7) | (ic151_o16 ? 0x08 : 0);
					else
						cprom_addr = rcmd;

					/* Inverted! */
					cprom_addr = ((~cprom_addr) & 0xf) << 4;
				}
				else
					cprom_addr = 0xf0;

				cprom_addr |= (ic149_o16 ? 0x8 : 0) |
								(ic150_o18 ? 0x4 : 0) |
								(ic150_o17 ? 0x2 : 0) |
								(ic150_o16 ? 0x1 : 0);

				/* Lower four bits of colour output come from PROM BB7 @ 188 */
				rcsd0_3 = rcols[cprom_addr] & 0xf;

				{
					UINT32 lfsr = vregs.wave_lfsr;
					UINT32 wave =
								(wave0 ^ BIT(lfsr, 0))  &&
								(wave1 ^ BIT(lfsr, 3))  &&
								BIT(lfsr, 5)            &&
								!BIT(lfsr, 15)          &&
								BIT(lfsr, 11)           &&
								BIT(lfsr, 13)           &&
								(rva20_6 < ((lfsr >> 8) & 0xf));

					*bmpaddr++ = 0x40 | (wave ? 0 : 0x20) | (ic150_o12 ? 0x10 : 0) | rcsd0_3;
				}
			}
			else
				*bmpaddr++ = 0;

			/* Update the various horizontal counters */
			UPDATE_HPOS(0);
			UPDATE_HPOS(1);
			UPDATE_HPOS(2);
			UPDATE_HPOS(3);

			/* Update the LFSR */
			vregs.wave_lfsr = (vregs.wave_lfsr << 1) | (BIT(vregs.wave_lfsr, 6) ^ !BIT(vregs.wave_lfsr, 15));

			/* Increment the bank counter */
			bank_cnt = (bank_cnt + 1) & 0x7ff;

			/* X pos */
			ls161 = (ls161 + 1) & 0x7fff;
		}

		/* WANGL active? Then update the 8-bit counter */
		if (wangl)
		{
			if (BIT(vregs.flags, BB_RDFLAG_TNLMD0))
				--vregs.wa8;
			else
				++vregs.wa8;
		}

		/* No carry out - just increment */
		if (vregs.wa4 != 0xf)
			++vregs.wa4;
		else
		{
			/* Carry out; increment again on /TMG2S rise */
			if (wangl)
			{
				if (BIT(vregs.flags, BB_RDFLAG_TNLMD0))
					--vregs.wa8;
				else
					++vregs.wa8;
			}
			vregs.wa4 = 1;
		}

		/* Update accumulator */
		vregs.h_val += vregs.h_inc;

		/* Seems correct */
		sf = vregs.shift;

		if ((vregs.shift & 0x80) == 0)
		{
			vregs.shift <<= 1;

			if ((sf & 0x08) == 0)
				vregs.shift |= BIT(vregs.h_val, 15);
		}

		if ((sf & 0x08) && !(vregs.shift & 0x08))
			vregs.h_inc = vregs.gas;

		/* Finally, increment the banking accumulator */
		vregs.ba_val = (vregs.ba_val + vregs.ba_inc) & 0x00ffffff;
	}
}


/***************************************************************************

    Buggy Boy Object Drawing

    X-scaling isn't quite right but you wouldn't notice...

    -------- xxxxxxxx       Object number
    xxxxxxxx --------       Y position

    xxxxxxxx xxxxxxxx       Y scale value

    -------- xxxxxxxx       X scale
                             00 = Invisible?
                             80 = 1:1
                             FF = Double size
    xxxxxxxx --------       Attributes

    xxxxxxxx xxxxxxxx       Y scale delta

    ------xx xxxxxxxx       X position

**************************************************************************/

static void buggyboy_draw_objs(UINT8 *bitmap, bool wide)
{
	UINT32 x_mask;
	UINT32 x_stride;

	/* The many lookup table ROMs */
	const UINT8 *const bug13  = &DrvObjLuts[0];
	const UINT8 *const bug18s = &DrvObjLuts[0x2000];
	const UINT8 *const bb8    = &DrvProms[0x1600];

	const UINT8 *const bug16s = &DrvObjMap[0];
	const UINT8 *const bug17s = &DrvObjMap[0x8000];

	const UINT8 *const bb9o = &DrvProms[0x500];
	const UINT8 *const bb9e = &DrvProms[0xd00];

	const UINT8 *const pixdata_rgn = &DrvObjROM[0];

	if (wide)
	{
		x_mask = 0x7ff;
		x_stride = 768;
	}
	else
	{
		x_mask = 0x3ff;
		x_stride = 256;
	}

	for (UINT32 offs = 0; offs < 0x300 / 2; offs += 8)
	{
		UINT32  x;
		UINT32  y;
		UINT32  gxflip;

		UINT32  x_scale;
		UINT32  x_step;
		UINT16  y_scale;
		UINT16  y_step;

		UINT8   pctmp0_7;
		UINT8   code;

		/* Check for end of object list */
		if ((BURN_ENDIAN_SWAP_INT16(objram16[offs]) & 0xff00) == 0xff00)
			break;

		/* X scale */
		x_scale = BURN_ENDIAN_SWAP_INT16(objram16[offs + 2]) & 0xff;

		/* TODO: Confirm against hardware? */
		if (x_scale == 0)
			continue;

		/* 16-bit y-scale accumulator */
		y_scale = BURN_ENDIAN_SWAP_INT16(objram16[offs + 1]);
		y_step  = BURN_ENDIAN_SWAP_INT16(objram16[offs + 3]);

		/* Object number */
		code = BURN_ENDIAN_SWAP_INT16(objram16[offs]) & 0xff;

		/* Attributes */
		pctmp0_7 = BURN_ENDIAN_SWAP_INT16(objram16[offs + 2]) >> 8;

		/* Global x-flip */
		gxflip = (pctmp0_7 & 0x80) >> 7;

		/* Add 1 to account for line buffering */
		y = (BURN_ENDIAN_SWAP_INT16(objram16[offs]) >> 8) + 1;

		for (; y < 240; ++y)
		{
			UINT32  rom_addr2   = 0;
			UINT8   bug17s_data = 0;
			UINT8   bug16s_data;

			/* Are we drawing on this line? */

			// TODO: See big lampposts.
			if (y_scale & 0x8000)
				break;

			{
				UINT32  psa0_12;
				UINT32  bug13_addr;
				UINT32  bug13_data;
				UINT32  rom_addr;
				UINT32  x_acc;
				UINT32  newtile = 1;
				UINT32  dataend = 0;
				UINT8   data1 = 0;
				UINT8   data2 = 0;
				UINT32  xflip = 0;
				UINT32  opcd10_11;
				UINT32  opcd8_9;
				UINT32  opcd0_11 = 0;
				UINT32  lasttile = 0;

				/* Use the object code to lookup the tile sequence data */
				bug13_addr = code << 4;
				bug13_addr |= ((y_scale >> 11) & 0xf);
				bug13_data = bug13[bug13_addr];

				/* Reached the bottom of the object */
				if (bug13_data == 0xff)
					break;

				psa0_12  = (((code & 0x80) << 5) | ((code & 0x40) << 6)) & 0x1000;
				psa0_12 |= ((bb8[code] << 8) | bug13_data) & 0x1fff;

				/* Static part of the BUG17S/BUG16S ROM address */
				rom_addr = (psa0_12 & ~0xff) << 2;

				/* Prepare the x-scaling */
				x_step = (128 << OBJ_FRAC) / x_scale;
				x_acc = (psa0_12 & 0xff) << (OBJ_FRAC + 5);

				/* TODO Add note */
				x = BURN_ENDIAN_SWAP_INT16(objram16[offs + 4]) & x_mask;

				for (;;)
				{
					/* Get data and attributes for an 8x8 tile */
					if (newtile)
					{
						UINT32  pscb0_11;
						UINT32  psbb0_15;
						UINT32  psbb6_7;
						UINT32  rombank;
						const UINT8 *romptr;
						UINT32  bug18s_data;
						UINT32  low_addr = ((x_acc >> (OBJ_FRAC + 3)) & x_mask);

						/*
						    Objects are grouped by width (either 16, 8 or 4 tiles) in
						    the LUT ROMs. The ROM address lines therefore indicate
						    width and are used to determine the correct scan order
						    when x-flip is set.
						*/
						if (gxflip)
						{
							UINT32  xor_mask;

							if (BIT(psa0_12, 11) || !BIT(psa0_12, 12))
								xor_mask = 0xf;
							else if (!BIT(psa0_12, 9))
								xor_mask = 0x7;
							else
								xor_mask = 0x3;

							rom_addr2 = rom_addr + (low_addr ^ xor_mask);
						}
						else
							rom_addr2 = rom_addr + low_addr;

						bug17s_data = bug17s[rom_addr2 & 0x7fff];

						if ((bug17s_data & 0x40) && dataend)
							lasttile = 1;

						dataend |= (bug17s_data & 0x40);

						/* Retrieve data for an 8x8 tile */
						bug16s_data = bug16s[rom_addr2 & 0x7fff];
						psbb0_15 = (bug17s_data << 8) | bug16s_data;
						psbb6_7 = (BIT(psbb0_15, 12) ? psbb0_15 : (pctmp0_7 << 6)) & 0xc0;

						/* Form the tile ROM address */
						pscb0_11 = ((((psbb0_15 & ~0xc0) | psbb6_7) << 3) | ((y_scale >> 8) & 7)) & 0x7fff;

						/* Choose from one of three banks */
						rombank = ((BIT(pctmp0_7, 4) << 1) | BIT(psbb0_15, 13)) & 3;

						romptr = (pixdata_rgn + rombank * (0x8000 * 2));

						/* Get raw 8x8 pixel row data */
						data1 = *(pscb0_11 + romptr);
						data2 = *(pscb0_11 + romptr + 0x8000);

						/* Determine flip state (global XOR local) */
						xflip = gxflip ^ !BIT(psbb0_15, 15);

						bug18s_data = bug18s[ (BIT(pctmp0_7, 4)  << 13) |
												(BIT(psbb0_15, 13) << 12)   |
												(psbb0_15 & ~0xf0c0)        |
												psbb6_7 ];

						/* Get the colour data. Note that bits 11 and 10 are inverted */
						opcd10_11 = ((pctmp0_7 << 8) & 0xc00) ^ 0xc00;
						opcd8_9 = ((pctmp0_7 & 0x60) << 3);
						opcd0_11 = (opcd10_11 | opcd8_9 | bug18s_data) & 0xfff;

						newtile = 0;
					}

					/* Draw a pixel? */
					if (x < x_stride)
					{
						UINT8   pix;
						UINT8   bit;

						bit = (x_acc >> OBJ_FRAC) & 7;

						if (xflip)
							bit ^= 7;

						pix = (((data1 >> bit) & 1) << 1) | ((data2 >> bit) & 1);

						/* Write the pixel if not transparent */
						if (!(!(opcd0_11 & 0x80) && !pix))
						{
							UINT8 color;
							UINT32 bb9_addr;

							bb9_addr = ((opcd0_11 << 1) & 0x600) | ((opcd0_11 & 0x7f) << 2) | pix;
							color = ((opcd0_11 >> 6) & 0x30);

							/* Inverted on schematic */
							if (x & 1)
								color = ~(color | bb9o[bb9_addr]) & 0x3f;
							else
								color = ~(color | bb9e[bb9_addr]) & 0x3f;

							*(bitmap + x_stride*y + x) = 0x40 | color;
						}
					}

					/* Check if we've stepped into a new 8x8 tile */
					if ((((x_acc + x_step) >> (OBJ_FRAC + 3)) & x_mask) != ((x_acc >> (OBJ_FRAC + 3)) & x_mask))
					{
						if (lasttile)
							break;

						newtile = 1;
					}

					x = (x + 1) & x_mask;
					x_acc += x_step;
				}
			}// if (yscale)
			y_scale += y_step;
		} /* for (y) */
	}/* for (offs) */
}
static INT32 needs_update;

static void tx1_update_layers()
{
	memset(obj_bmp, 0, 768 * 240);

	tx1_draw_char(chr_bmp);
	tx1_draw_road(rod_bmp);
	tx1_draw_objects(obj_bmp);
}

static void buggyboy_update_layers()
{
	memset(obj_bmp, 0, 768 * 240);
	memset(rod_bmp, 0, 768 * 240);

	buggyboy_draw_char(chr_bmp, 1);
	buggyboy_draw_road(rod_bmp);
	buggyboy_draw_objs(obj_bmp, 1);
}

static void buggybjr_update_layers()
{
	memset(obj_bmp, 0, 256 * 240);

	buggyboy_draw_char(chr_bmp, 0);
	buggybjr_draw_road(rod_bmp);
	buggyboy_draw_objs(obj_bmp, 0);
}

// start of vblank: the layers are rendered here as drawing updates the road state
static void vblank_start()
{
	if (game_type == GAME_BUGGYBOYJR) {
		buggybjr_update_layers();
	} else if (needs_update) {
		if (game_type == GAME_TX1)
			tx1_update_layers();
		else
			buggyboy_update_layers();
	}

	vregs.slin_val += vregs.slin_inc;

	if (is_buggyboy)
		vregs.wave_lfsr = 0;

	needs_update = 1;
}

static void tx1_combine_layers(INT32 screen)
{
	const UINT8 *chr_pal = &DrvProms[0x900];

	INT32 x_offset = screen * 256;

	for (INT32 y = 0; y < 240; ++y)
	{
		UINT16 *dst = pTransDraw + y * nScreenWidth + x_offset;

		UINT32 bmp_offset = y * 768 + x_offset;

		UINT8 *chr_addr = chr_bmp + bmp_offset;
		UINT8 *rod_addr = rod_bmp + bmp_offset;
		UINT8 *obj_addr = obj_bmp + bmp_offset;

		for (INT32 x = 0; x < 256; ++x)
		{
			UINT8 out_val;
			UINT32 char_val = chr_addr[x];
			UINT32 c7 = BIT(char_val, 7);
			UINT32 c1 = BIT(char_val, 1);
			UINT32 c0 = BIT(char_val, 0);

			UINT32 road_val = rod_addr[x];
			UINT32 r6 = BIT(road_val, 6);
			UINT32 r5 = BIT(road_val, 5);

			UINT32 obj_val = obj_addr[x];
			UINT32 obj6 = BIT(obj_val, 6);

			UINT32 term1 = !(c7 && c1);
			UINT32 term2 = !(c7 && c0);
			UINT32 term3 = r5 || !r6;
			UINT32 p12 = !(term1 && term2 && term3);
			UINT32 p6 = !(obj6 && term1 && term2);
			UINT32 sel =  p12 | (p6 << 1);

			UINT32 psel =  (!(p6 && p12) << 1) | p6;

			if      (sel == 3)  out_val = ((char_val & 0xc0) >> 2) | (chr_pal[char_val] & 0xf);
			else if (sel == 2)  out_val = road_val & 0x3f;
			else                out_val = obj_val & 0x3f;

			dst[x] = (psel << 6) | out_val;
		}
	}
}

static void bb_combine_layers(INT32 screen)
{
	const UINT8 *chr_pal = &DrvProms[0x400];
	UINT32 bmp_stride;
	UINT32 x_offset;

	if (screen < 0)
	{
		bmp_stride = 256;
		x_offset = 0;
	}
	else
	{
		bmp_stride = 768;
		x_offset = 256 * screen;
	}

	for (UINT32 y = 0; y < 240; ++y)
	{
		UINT32 bmp_offset = y * bmp_stride + x_offset;

		UINT8 *chr_addr = chr_bmp + bmp_offset;
		UINT8 *rod_addr = rod_bmp + bmp_offset;
		UINT8 *obj_addr = obj_bmp + bmp_offset;

		UINT32 sky_en = BIT(vregs.sky, 7);
		UINT32 sky_val = (((vregs.sky & 0x7f) + y) >> 2) & 0x3f;

		UINT16 *dst = pTransDraw + y * nScreenWidth + x_offset;

		for (UINT32 x = 0; x < 256; ++x)
		{
			UINT32 out_val;

			UINT32 char_val = *chr_addr++;
			UINT32 char_6_7 = (char_val & 0xc0) >> 2;

			UINT32 obj_val = *obj_addr++;
			UINT32 obj6 = BIT(obj_val, 6);

			UINT32 rod_val = *rod_addr++;
			UINT32 rod6 = BIT(rod_val, 6);

			UINT32 chr = !(BIT(char_val, 7) && (char_val & 3) );

			UINT32 sel =
			(
				( BIT(obj_val, 6) && chr) ||
				( sky_en && !(char_val & 3) && (!obj6 && !rod6) )
			) ? 0 : 1;

			sel |= (!(obj6 || rod6) || !chr) ? 2 : 0;

			if      (sel == 0)  out_val = obj_val & 0x3f;
			else if (sel == 1)  out_val = rod_val & 0x3f;
			else if (sel == 2)  out_val = sky_val;
			else                out_val = char_6_7 + chr_pal[char_val];

			*dst++ = ((sel << 6) + out_val) & 0xff;
		}
	}
}

static void DrvPaletteInit()
{
	if (game_type == GAME_TX1)
	{
		static const res_net_info tx1_net_info =
		{
			RES_NET_VCC_5V | RES_NET_VIN_TTL_OUT,
			{
				{ RES_NET_AMP_NONE, 0, 0, 4, { 2200, 1000, 470, 220 } },
				{ RES_NET_AMP_NONE, 0, 0, 4, { 2200, 1000, 470, 220 } },
				{ RES_NET_AMP_NONE, 0, 0, 4, { 2200, 1000, 470, 220 } }
			}
		};

		for (INT32 i = 0; i < 256; ++i)
		{
			INT32 r = compute_res_net(DrvProms[i + 0x300] & 0xf, 0, tx1_net_info);
			INT32 g = compute_res_net(DrvProms[i + 0x400] & 0xf, 1, tx1_net_info);
			INT32 b = compute_res_net(DrvProms[i + 0x500] & 0xf, 2, tx1_net_info);

			DrvPalette[i] = BurnHighCol(r, g, b, 0);
		}
	}
	else
	{
		// RGB: 220/470/1k/2.2k ohm, brightness (bb13): 4.7k ohm
		for (INT32 i = 0; i < 0x100; i++)
		{
			INT32 bit0, bit1, bit2, bit3, bit4;

			bit0 = BIT(DrvProms[i + 0x000], 0);
			bit1 = BIT(DrvProms[i + 0x000], 1);
			bit2 = BIT(DrvProms[i + 0x000], 2);
			bit3 = BIT(DrvProms[i + 0x000], 3);
			bit4 = BIT(DrvProms[i + 0x300], 2);
			INT32 r = 0x06 * bit4 + 0x0d * bit0 + 0x1e * bit1 + 0x41 * bit2 + 0x8a * bit3;

			bit0 = BIT(DrvProms[i + 0x100], 0);
			bit1 = BIT(DrvProms[i + 0x100], 1);
			bit2 = BIT(DrvProms[i + 0x100], 2);
			bit3 = BIT(DrvProms[i + 0x100], 3);
			bit4 = BIT(DrvProms[i + 0x300], 1);
			INT32 g = 0x06 * bit4 + 0x0d * bit0 + 0x1e * bit1 + 0x41 * bit2 + 0x8a * bit3;

			bit0 = BIT(DrvProms[i + 0x200], 0);
			bit1 = BIT(DrvProms[i + 0x200], 1);
			bit2 = BIT(DrvProms[i + 0x200], 2);
			bit3 = BIT(DrvProms[i + 0x200], 3);
			bit4 = BIT(DrvProms[i + 0x300], 0);
			INT32 b = 0x06 * bit4 + 0x0d * bit0 + 0x1e * bit1 + 0x41 * bit2 + 0x8a * bit3;

			DrvPalette[i] = BurnHighCol(r, g, b, 0);
		}
	}
}

/*************************************
 *
 *  Sound
 *
 *************************************/

#define TX1_PIT_CLOCK		(6000000 / 16)
#define TX1_FRAC			30

#define TX1_SHUNT           (250.0)
#define TX1_R0              (180000.0 + TX1_SHUNT)
#define TX1_R1              (56000.0  + TX1_SHUNT)
#define TX1_R2              (22000.0  + TX1_SHUNT)
#define TX1_R               (100000.0 + TX1_SHUNT)
#define TX1_RI              (180000.0)

static const double tx1_engine_gains[16] =
{
	-( TX1_R )/TX1_RI,
	-( 1.0/(1.0/TX1_R + 1.0/TX1_R0) )/TX1_RI,
	-( 1.0/(1.0/TX1_R + 1.0/TX1_R1) )/TX1_RI,
	-( 1.0/(1.0/TX1_R + 1.0/TX1_R1 + 1.0/TX1_R0) )/TX1_RI,
	-( 1.0/(1.0/TX1_R + 1.0/TX1_R2) )/TX1_RI,
	-( 1.0/(1.0/TX1_R + 1.0/TX1_R2 + 1.0/TX1_R0) )/TX1_RI,
	-( 1.0/(1.0/TX1_R + 1.0/TX1_R2 + 1.0/TX1_R1) )/TX1_RI,
	-( 1.0/(1.0/TX1_R + 1.0/TX1_R2 + 1.0/TX1_R1 + 1.0/TX1_R0) )/TX1_RI,
	-( 1.0/(1.0/TX1_R + 1.0/TX1_SHUNT ) )/TX1_RI,
	-( 1.0/(1.0/TX1_R + 1.0/TX1_SHUNT + 1.0/TX1_R0) )/TX1_RI,
	-( 1.0/(1.0/TX1_R + 1.0/TX1_SHUNT + 1.0/TX1_R1) )/TX1_RI,
	-( 1.0/(1.0/TX1_R + 1.0/TX1_SHUNT + 1.0/TX1_R1 + 1.0/TX1_R0) )/TX1_RI,
	-( 1.0/(1.0/TX1_R + 1.0/TX1_SHUNT + 1.0/TX1_R2) )/TX1_RI,
	-( 1.0/(1.0/TX1_R + 1.0/TX1_SHUNT + 1.0/TX1_R2 + 1.0/TX1_R0) )/TX1_RI,
	-( 1.0/(1.0/TX1_R + 1.0/TX1_SHUNT + 1.0/TX1_R2 + 1.0/TX1_R1) )/TX1_RI,
	-( 1.0/(1.0/TX1_R + 1.0/TX1_SHUNT + 1.0/TX1_R2 + 1.0/TX1_R1 + 1.0/TX1_R0) )/TX1_RI
};

#define BUGGYBOY_PIT_CLOCK      (7500000 / 8)
#define BUGGYBOY_NOISE_CLOCK    (BUGGYBOY_PIT_CLOCK / 4)

#define BUGGYBOY_R1     47000.0
#define BUGGYBOY_R2     22000.0
#define BUGGYBOY_R3     10000.0
#define BUGGYBOY_R4     5600.0
#define BUGGYBOY_SHUNT  250.0

#define BUGGYBOY_R1S    (1.0/(1.0/BUGGYBOY_R1 + 1.0/BUGGYBOY_SHUNT))
#define BUGGYBOY_R2S    (1.0/(1.0/BUGGYBOY_R2 + 1.0/BUGGYBOY_SHUNT))
#define BUGGYBOY_R3S    (1.0/(1.0/BUGGYBOY_R3 + 1.0/BUGGYBOY_SHUNT))
#define BUGGYBOY_R4S    (1.0/(1.0/BUGGYBOY_R4 + 1.0/BUGGYBOY_SHUNT))

static const double bb_engine_gains[16] =
{
	-1.0/(1.0/(BUGGYBOY_R1  + BUGGYBOY_R2  + BUGGYBOY_R3  + BUGGYBOY_R4 ) + 1.0/100e3)/100e3,
	-1.0/(1.0/(BUGGYBOY_R1  + BUGGYBOY_R2  + BUGGYBOY_R3  + BUGGYBOY_R4S) + 1.0/100e3)/100e3,
	-1.0/(1.0/(BUGGYBOY_R1  + BUGGYBOY_R2  + BUGGYBOY_R3S + BUGGYBOY_R4 ) + 1.0/100e3)/100e3,
	-1.0/(1.0/(BUGGYBOY_R1  + BUGGYBOY_R2  + BUGGYBOY_R3S + BUGGYBOY_R4S) + 1.0/100e3)/100e3,
	-1.0/(1.0/(BUGGYBOY_R1  + BUGGYBOY_R2S + BUGGYBOY_R3  + BUGGYBOY_R4 ) + 1.0/100e3)/100e3,
	-1.0/(1.0/(BUGGYBOY_R1  + BUGGYBOY_R2S + BUGGYBOY_R3  + BUGGYBOY_R4S) + 1.0/100e3)/100e3,
	-1.0/(1.0/(BUGGYBOY_R1  + BUGGYBOY_R2S + BUGGYBOY_R3S + BUGGYBOY_R4 ) + 1.0/100e3)/100e3,
	-1.0/(1.0/(BUGGYBOY_R1  + BUGGYBOY_R2S + BUGGYBOY_R3S + BUGGYBOY_R4S) + 1.0/100e3)/100e3,
	-1.0/(1.0/(BUGGYBOY_R1S + BUGGYBOY_R2  + BUGGYBOY_R3  + BUGGYBOY_R4 ) + 1.0/100e3)/100e3,
	-1.0/(1.0/(BUGGYBOY_R1S + BUGGYBOY_R2  + BUGGYBOY_R3  + BUGGYBOY_R4S) + 1.0/100e3)/100e3,
	-1.0/(1.0/(BUGGYBOY_R1S + BUGGYBOY_R2  + BUGGYBOY_R3S + BUGGYBOY_R4 ) + 1.0/100e3)/100e3,
	-1.0/(1.0/(BUGGYBOY_R1S + BUGGYBOY_R2  + BUGGYBOY_R3S + BUGGYBOY_R4S) + 1.0/100e3)/100e3,
	-1.0/(1.0/(BUGGYBOY_R1S + BUGGYBOY_R2S + BUGGYBOY_R3  + BUGGYBOY_R4 ) + 1.0/100e3)/100e3,
	-1.0/(1.0/(BUGGYBOY_R1S + BUGGYBOY_R2S + BUGGYBOY_R3  + BUGGYBOY_R4S) + 1.0/100e3)/100e3,
	-1.0/(1.0/(BUGGYBOY_R1S + BUGGYBOY_R2S + BUGGYBOY_R3S + BUGGYBOY_R4 ) + 1.0/100e3)/100e3,
	-1.0/(1.0/(BUGGYBOY_R1S + BUGGYBOY_R2S + BUGGYBOY_R3S + BUGGYBOY_R4S) + 1.0/100e3)/100e3,
};

static inline INT32 combine_weights(const double *tab, INT32 w0, INT32 w1, INT32 w2)
{
	return (INT32)(tab[0] * w0 + tab[1] * w1 + tab[2] * w2 + 0.5);
}

static inline INT32 combine_weights(const double *tab, INT32 w0, INT32 w1, INT32 w2, INT32 w3)
{
	return (INT32)(tab[0] * w0 + tab[1] * w1 + tab[2] * w2 + tab[3] * w3 + 0.5);
}

static void sound_init_tables()
{
	if (game_type == GAME_TX1)
	{
		static const INT32 r0[4] = { 390000, 180000, 180000, 180000 };
		static const INT32 r1[3] = { 180000, 390000, 56000 };
		static const INT32 r2[3] = { 390000, 390000, 180000 };

		compute_resistor_weights(0, 10000, -1.0,
				4, &r0[0], weights0, 0, 0,
				3, &r1[0], weights1, 0, 0,
				3, &r2[0], weights2, 0, 0);

		freq_to_step = (double)(1 << TX1_FRAC) / (double)nBurnSoundRate;
	}
	else
	{
		static const INT32 resistors[4] = { 330000, 220000, 330000, 220000 };
		double aweights[4];
		static const INT32 tmp[16] =
		{
			0x0, 0x1, 0xe, 0xf, 0x8, 0x9, 0x6, 0x7, 0xc, 0xd, 0xe, 0xf, 0x4, 0x5, 0x6, 0x7
		};

		compute_resistor_weights(0, 16384,  -1.0,
				4,  &resistors[0], aweights, 0, 0,
				0, NULL, NULL, 0, 0,
				0, NULL, NULL, 0, 0 );

		for (INT32 i = 0; i < 16; i++)
			eng_voltages[i] = combine_weights(aweights, BIT(tmp[i], 0), BIT(tmp[i], 1), BIT(tmp[i], 2), BIT(tmp[i], 3));

		freq_to_step = (double)(1 << 24) / (double)nBurnSoundRate;
	}
}

static void sound_reset()
{
	memset(&pit8253, 0, sizeof(pit8253));
	snd_step0 = snd_step1 = snd_step2 = 0;
	snd_pit0 = snd_pit1 = snd_pit2 = 0;
	memset(eng0, 0, sizeof(eng0));
	memset(eng1, 0, sizeof(eng1));
	memset(eng2, 0, sizeof(eng2));
	ay_outputa = ay_outputb = 0;
	ppi_latch_a = ppi_latch_b = 0;
	noise_lfsra = 0;
	noise_lfsrb = 1;
	noise_lfsrc = 0;
	noise_lfsrd = 0;
	noise_counter = 0;
	ym1_outputa = ym2_outputa = ym2_outputb = 0;
	snd_gain = 1.0;
	memset(dc_last_in, 0, sizeof(dc_last_in));
	memset(dc_last_out, 0, sizeof(dc_last_out));
}

static inline void update_engine(INT32 eng[4])
{
	INT32 p0 = eng[0];
	INT32 p1 = eng[1];
	INT32 p2 = eng[2];
	INT32 p3 = eng[3];

	eng[0] = !p0;
	if (p0 && !eng[0]) eng[1] = !p1;
	eng[2] = !(p2 && !p3);
	eng[3] = !p2;
}

static inline INT16 dc_block(INT32 ch, INT32 in)
{
	INT32 out = in - dc_last_in[ch] + (INT32)(0.997 * dc_last_out[ch]);
	dc_last_in[ch] = in;
	dc_last_out[ch] = out;

	return BURN_SND_CLIP(out);
}

static void tx1_sound_render(INT16 **streams, INT32 samples)
{
	INT16 *destr = streams[0];
	INT16 *destl = streams[1];
	UINT32 step_0 = pit8253.counts[0] ? (UINT32)((TX1_PIT_CLOCK / pit8253.counts[0]) * freq_to_step) : 0;
	UINT32 step_1 = pit8253.counts[1] ? (UINT32)((TX1_PIT_CLOCK / pit8253.counts[1]) * freq_to_step) : 0;
	UINT32 step_2 = pit8253.counts[2] ? (UINT32)((TX1_PIT_CLOCK / pit8253.counts[2]) * freq_to_step) : 0;

	double gain_2 = tx1_engine_gains[ay_outputb & 0xf];
	double gain_3 = BIT(ay_outputb, 5) ? 1.0 : 1.5;

	for (INT32 i = 0; i < samples; i++)
	{
		if (snd_step0 & (1 << TX1_FRAC))
		{
			update_engine(eng0);
			snd_pit0 = combine_weights(weights0, eng0[0], eng0[1], eng0[2], eng0[3]);
			snd_step0 &= ((1 << TX1_FRAC) - 1);
		}

		if (snd_step1 & (1 << TX1_FRAC))
		{
			update_engine(eng1);
			snd_pit1 = combine_weights(weights1, eng1[0], eng1[1], eng1[3]);
			snd_step1 &= ((1 << TX1_FRAC) - 1);
		}

		if (snd_step2 & (1 << TX1_FRAC))
		{
			update_engine(eng2);
			snd_pit2 = combine_weights(weights2, eng2[0], eng2[1], eng2[3]);
			snd_step2 &= ((1 << TX1_FRAC) - 1);
		}

		INT32 out = (INT32)(((snd_pit0 + snd_pit1) * gain_3 + 2 * snd_pit2 * gain_2) * 0.20 * snd_gain);

		destr[i] = dc_block(0, out);
		destl[i] = dc_block(1, out);

		snd_step0 += step_0;
		snd_step1 += step_1;
		snd_step2 += step_2;
	}
}

static void buggyboy_sound_render(INT16 **streams, INT32 samples)
{
	INT16 *destr = streams[0];
	INT16 *destl = streams[1];
	UINT32 step_0 = pit8253.counts[0] ? (UINT32)((BUGGYBOY_PIT_CLOCK / pit8253.counts[0]) * freq_to_step) : 0;
	UINT32 step_1 = pit8253.counts[1] ? (UINT32)((BUGGYBOY_PIT_CLOCK / pit8253.counts[1]) * freq_to_step) : 0;

	double gain0;

	if (game_type == GAME_BUGGYBOYJR)
		gain0 = BIT(ym2_outputb, 3) ? 1.0 : 2.0;
	else
		gain0 = BIT(ym1_outputa, 3) ? 1.0 : 2.0;

	INT32 n1_en = BIT(ym2_outputb, 4);
	INT32 n2_en = BIT(ym2_outputb, 5);

	double gain1_l = bb_engine_gains[ym2_outputa >> 4] * 5;
	double gain1_r = bb_engine_gains[ym2_outputa & 0xf] * 5;

	INT32 noise_steps = BUGGYBOY_NOISE_CLOCK / nBurnSoundRate;

	for (INT32 i = 0; i < samples; i++)
	{
		INT32 pit0, pit1, n1, n2;
		pit0 = eng_voltages[(snd_step0 >> 24) & 0xf];
		pit1 = eng_voltages[(snd_step1 >> 24) & 0xf];

		// tyre screech noise source
		for (INT32 j = 0; j < noise_steps; ++j)
		{
			// CD4006 is a 4-4-1-4-4-1 shift register
			INT32 p13 = BIT(noise_lfsra, 3);
			INT32 p12 = BIT(noise_lfsrb, 4);
			INT32 p10 = BIT(noise_lfsrc, 3);
			INT32 p8 = BIT(noise_lfsrd, 3);

			noise_lfsra = p12 | ((noise_lfsra << 1) & 0xf);
			noise_lfsrb = (p8 ^ p12) | ((noise_lfsrb << 1) & 0x1f);
			noise_lfsrc = p13 | ((noise_lfsrc << 1) & 0xf);
			noise_lfsrd = p10 | ((noise_lfsrd << 1) & 0x1f);

			// 4040 12-bit counter is clocked on the falling edge of Q13
			if (!BIT(noise_lfsrc, 3) && p10)
				noise_counter = (noise_counter + 1) & 0x0fff;
		}

		if (n1_en)
		{
			n1 = !BIT(noise_counter, 7-1) * 16000;
			if (BIT(noise_counter, 11-1)) n1 /= 2;
		}
		else
			n1 = 8192;

		if (n2_en)
		{
			n2 = !BIT(noise_counter, 6-1) * 16000;
			if (BIT(noise_counter, 11-1)) n2 /= 2;
		}
		else
			n2 = 8192;

		INT32 l = (INT32)((n1 + n2 + (pit0 * gain0) + (pit1 * gain1_l)) * 0.20);
		INT32 r = (INT32)((n1 + n2 + (pit0 * gain0) + (pit1 * gain1_r)) * 0.20);

		destr[i] = dc_block(0, r);
		destl[i] = dc_block(1, l);

		snd_step0 += step_0;
		snd_step1 += step_1;
	}
}

static INT32 z80_cycles_per_frame;

static void sound_stream_update(INT16 **streams, INT32 samples)
{
	if (game_type == GAME_TX1)
		tx1_sound_render(streams, samples);
	else
		buggyboy_sound_render(streams, samples);
}

static void pit8253_write(UINT8 offset, UINT8 data)
{
	stream.update();

	if (offset < 3)
	{
		if (pit8253.idx[offset] == 0)
		{
			pit8253.counts[offset] = (pit8253.counts[offset] & 0xff00) | data;
			pit8253.idx[offset] = 1;
		}
		else
		{
			pit8253.counts[offset] = (pit8253.counts[offset] & 0x00ff) | (data << 8);
			pit8253.idx[offset] = 0;
		}
	}
	else
	{
		INT32 mode = (data >> 1) & 7;

		// only mode 3 is used
		if (mode == 3)
		{
			INT32 cntsel = (data >> 6) & 3;
			if (cntsel < 3)
			{
				pit8253.idx[cntsel] = 0;
				pit8253.counts[cntsel] = 0;
			}
		}
	}
}

static void tx1_ay8910_a_w(UINT32, UINT32 data)
{
	stream.update();
	ay_outputa = ~data;
}

static void tx1_ay8910_b_w(UINT32, UINT32 data)
{
	stream.update();
	ay_outputb = data ^ 0xf;
	snd_gain = BIT(ay_outputb, 4) ? 1.5 : 2.0;
}

static void bb_ym1_a_w(UINT32, UINT32 data)
{
	stream.update();
	ym1_outputa = data ^ 0xff;
}

static void bb_ym2_a_w(UINT32, UINT32 data)
{
	stream.update();
	ym2_outputa = data ^ 0xff;
}

static void bb_set_ay_gains(UINT8 data)
{
	// rear speakers are mixed into the front ones
	AY8910SetAllRoutes(0, 0.15 * ((data & 0x80) ? 1.0 : 2.0), BURN_SND_ROUTE_LEFT);
	AY8910SetAllRoutes(1, 0.15 * ((data & 0x40) ? 1.0 : 2.0), BURN_SND_ROUTE_RIGHT);
}

static void bb_ym2_b_w(UINT32, UINT32 data)
{
	stream.update();

	ym2_outputb = data ^ 0xff;

	bb_set_ay_gains(data);
}

static UINT8 bit_reverse8(UINT8 val)
{
	val = ((val & 0xF0) >>  4) | ((val & 0x0F) <<  4);
	val = ((val & 0xCC) >>  2) | ((val & 0x33) <<  2);
	val = ((val & 0xAA) >>  1) | ((val & 0x55) <<  1);

	return val;
}

/*************************************
 *
 *  Inputs
 *
 *************************************/

static UINT8 read_steering()
{
	return BurnTrackballRead(0, 0) & 0x0f;
}

static UINT8 read_pedal(INT16 port)
{
	return pedal_accu[port & 1] >> 3; // 0..ff -> 0..1f
}

static UINT16 dipswitches_r()
{
	// bit 0 is TS, acknowledged on startup
	return (DrvDips[0] | (DrvDips[1] << 8)) & 0xfffe;
}

static UINT8 tx1_ppi_porta_r()
{
	return ppi_latch_a;
}

static UINT8 tx1_ppi_portb_r()
{
	return ppi_latch_b & 0xf;
}

static UINT8 tx1_ppi_portc_r()
{
	return (DrvJoy1[0] & 1) | ((DrvJoy1[1] & 1) << 1) | (DrvDips[2] & 0x04) | (gear_state << 3);
}

static UINT8 bb_inputs_r()
{
	return (DrvJoy1[0] & 1) | ((DrvJoy1[1] & 1) << 1) | ((DrvJoy1[2] & 1) << 2) | (gear_state << 6) | (DrvDips[2] & 0x80);
}

static UINT8 bb_ppi_portc_r()
{
	return DrvDips[3];
}

static UINT8 bbjr_ay_porta_r(UINT32)
{
	return bb_inputs_r();
}

static UINT8 bbjr_ay_portb_r(UINT32)
{
	return DrvDips[3];
}

static UINT8 bb_ppi_porta_r()
{
	return bb_inputs_r();
}

/*************************************
 *
 *  Sound CPU
 *
 *************************************/

static void __fastcall tx1_sound_write(UINT16 address, UINT8 data)
{
	if (address == 0x4000) {
		ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		return;
	}

	if ((address & 0xfffc) == 0x5000) {
		ppi8255_w(0, address & 3, data);
		return;
	}

	if ((address & 0xfffc) == 0x6000) {
		pit8253_write(address & 3, data);
		return;
	}

	if ((address & 0xf000) == 0x7000) {
		// latch the analog inputs
		ppi_latch_a = ((read_pedal(1) & 0xf) << 4) | (read_pedal(0) & 0xf);
		ppi_latch_b = read_steering() & 0xf;
		return;
	}
}

static UINT8 __fastcall tx1_sound_read(UINT16 address)
{
	if ((address & 0xfffc) == 0x5000) {
		return ppi8255_r(0, address & 3);
	}

	return 0;
}

static void __fastcall tx1_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x40: AY8910Write(0, 1, data); return;
		case 0x41: AY8910Write(0, 0, data); return;
	}
}

static UINT8 __fastcall tx1_sound_read_port(UINT16)
{
	return 0;
}

static UINT8 bb_analog_r(INT32 offset)
{
	UINT8 data;

	if (offset == 0)
		data = (read_pedal(0) << 4) | read_steering();
	else
		data = read_pedal(1) << 4;

	return (game_type == GAME_BUGGYBOYJR) ? data : bit_reverse8(data);
}

static void __fastcall buggyboy_sound_write(UINT16 address, UINT8 data)
{
	if (game_type == GAME_BUGGYBOYJR)
	{
		if ((address & 0xfffc) == 0x5000) {
			pit8253_write(address & 3, data);
			return;
		}

		if (address == 0x7000) {
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
			return;
		}
	}
	else
	{
		if ((address & 0xfffc) == 0x6800) {
			ppi8255_w(0, address & 3, data);
			return;
		}

		if ((address & 0xfffc) == 0x7000) {
			pit8253_write(address & 3, data);
			return;
		}

		if (address == 0x7800) {
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
			return;
		}
	}
}

static UINT8 __fastcall buggyboy_sound_read(UINT16 address)
{
	if ((address & 0xfffe) == 0x6000) {
		return bb_analog_r(address & 1);
	}

	if (game_type != GAME_BUGGYBOYJR && (address & 0xfffc) == 0x6800) {
		return ppi8255_r(0, address & 3);
	}

	return 0;
}

static void __fastcall buggyboy_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x40: AY8910Write(0, 1, data); return;
		case 0x41: AY8910Write(0, 0, data); return;
		case 0x80: AY8910Write(1, 1, data); return;
		case 0x81: AY8910Write(1, 0, data); return;
	}
}

static UINT8 __fastcall buggyboy_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x40: return AY8910Read(0);
		case 0x80: return AY8910Read(1);
	}

	return 0;
}

/*************************************
 *
 *  Main CPU
 *
 *************************************/

static void math_set_test(INT32 state)
{
	VezSetIRQLineAndVector(1, NEC_INPUT_LINE_POLL, 0xff, state);
}

static UINT16 main_read_word(UINT32 address)
{
	if (address >= 0x10000 && address < 0x20000) {
		return ZetReadByte((address - 0x10000) >> 1);
	}

	if ((address & 0xff000) == (is_buggyboy ? 0x04000 : 0x06000)) {
		return 0xffff; // crtc
	}

	if ((address & 0xffffe) == 0x0b000) {
		return dipswitches_r();
	}

	if ((address & (is_buggyboy ? 0xffffc : 0xffffe)) == 0x0f000) {
		return 0xffff; // watchdog
	}

	return 0;
}

static void main_write_word(UINT32 address, UINT16 data)
{
	if (address >= 0x10000 && address < 0x20000) {
		ZetWriteByte((address - 0x10000) >> 1, data & 0xff);
		return;
	}

	switch (address & 0xff000)
	{
		case 0x0b000:
			if (address < 0x0b002) {
				ZetSetHALT((data & 1) ? 0 : 1); // z80 busreq
			}
			return;

		case 0x0c000:
			if (address < 0x0c002) {
				vregs.scol = is_buggyboy ? data : (data & 0x0707);
			}
			return;

		case 0x0d000:
			if (address < 0x0d004) {
				tx1_slincs_w((address >> 1) & 1, data);
			}
			return;

		case 0x0e000:
			if (address < 0x0e002) {
				if (is_buggyboy)
					vregs.sky = data;
				else
					vregs.slock = data & 1;
			}
			return;

		case 0x0f000:
			if (address < (UINT32)(is_buggyboy ? 0x0f004 : 0x0f002)) {
				math_set_test(1);
			}
			return;
	}
}

static UINT16 main_read_word_cb(UINT32 address)
{
	return main_read_word(address);
}

static void main_write_word_cb(UINT32 address, UINT16 data)
{
	main_write_word(address, data);
}

static UINT8 __fastcall main_read_byte(UINT32 address)
{
	UINT16 data = main_read_word(address & ~1);

	return (address & 1) ? (data >> 8) : (data & 0xff);
}

static void __fastcall main_write_byte(UINT32 address, UINT8 data)
{
	main_write_word(address & ~1, (address & 1) ? (data << 8) : data);
}

/*************************************
 *
 *  Math CPU
 *
 *************************************/

static UINT16 math_read_word(UINT32 address)
{
	if (address >= 0x0800 && address < 0x1000) {
		UINT32 offset = (address - 0x0800) >> 1;
		return is_buggyboy ? buggyboy_spcs_ram_r(offset) : tx1_spcs_ram_r(offset);
	}

	if (address >= 0x3000 && address < 0x4000) {
		UINT32 offset = (address - 0x3000) >> 1;
		return is_buggyboy ? buggyboy_math_r(offset) : tx1_math_r(offset);
	}

	if (address >= 0x5000 && address < 0x8000) {
		UINT32 offset = (address - 0x5000) >> 1;
		return is_buggyboy ? buggyboy_spcs_rom_r(offset) : tx1_spcs_rom_r(offset);
	}

	return 0;
}

static void math_write_word(UINT32 address, UINT16 data)
{
	if (address >= 0x0800 && address < 0x1000) {
		DrvMathRAM[(address & 0x7fe) + 0] = data & 0xff;
		DrvMathRAM[(address & 0x7fe) + 1] = data >> 8;
		return;
	}

	if (address >= 0x3000 && address < 0x4000) {
		UINT32 offset = (address - 0x3000) >> 1;
		if (is_buggyboy) buggyboy_math_w(offset, data); else tx1_math_w(offset, data);
		return;
	}

	if (is_buggyboy)
	{
		if (address >= 0x2400 && address < 0x2500) {
			buggyboy_gas_w((address - 0x2400) >> 1, data);
		}
	}
	else
	{
		if (address >= 0x2400 && address < 0x2800) {
			tx1_bankcs_w((address - 0x2400) >> 1, data);
			return;
		}

		if (address >= 0x2800 && address < 0x2c00) {
			math_set_test(0);
			return;
		}

		if (address >= 0x2c00 && address < 0x3000) {
			vregs.flags = data & 0xff;
			return;
		}
	}
}

static UINT16 math_read_word_cb(UINT32 address)
{
	return math_read_word(address);
}

static void math_write_word_cb(UINT32 address, UINT16 data)
{
	math_write_word(address, data);
}

static UINT8 __fastcall math_read_byte(UINT32 address)
{
	UINT16 data = math_read_word(address & ~1);

	return (address & 1) ? (data >> 8) : (data & 0xff);
}

static void __fastcall math_write_byte(UINT32 address, UINT8 data)
{
	if (address >= 0x0800 && address < 0x1000) {
		DrvMathRAM[address & 0x7ff] = data;
		return;
	}

	math_write_word(address & ~1, (address & 1) ? (data << 8) : data);
}

/*************************************
 *
 *  Machine
 *
 *************************************/

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	VezOpen(0);
	VezReset();
	VezClose();

	VezOpen(1);
	VezReset();
	VezClose();

	ZetOpen(0);
	ZetReset();
	ZetSetHALT(0);
	AY8910Reset(0);
	if (is_buggyboy) {
		AY8910Reset(1);
		bb_set_ay_gains(0xff);
	}
	ZetClose();

	if (game_type != GAME_BUGGYBOYJR) {
		ppi8255_reset();
	}

	memset(&math, 0, sizeof(math));
	memset(&sn74s516, 0, sizeof(sn74s516));
	memset(&vregs, 0, sizeof(vregs));

	BurnShiftReset();
	sound_reset();

	needs_update = 0;
	z80_irq_cycles = 0;
	pedal_cycles = 0;

	memset(nExtraCycles, 0, sizeof(nExtraCycles));

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvMainROM		= Next; Next += 0x020000;
	DrvMathROM		= Next; Next += 0x004000;
	DrvZ80ROM		= Next; Next += 0x004000;

	DrvCharROM		= Next; Next += 0x008000;
	DrvObjROM		= Next; Next += 0x040000;
	DrvRoadROM		= Next; Next += 0x008000;
	DrvAuData		= Next; Next += 0x010000;
	DrvObjMap		= Next; Next += 0x010000;
	DrvObjLuts		= Next; Next += 0x006000;
	DrvProms		= Next; Next += 0x002000;

	au_data16		= (UINT16*)Next; Next += 0x008000 * sizeof(UINT16);

	DrvPalette		= (UINT32*)Next; Next += 0x0100 * sizeof(UINT32);

	DrvNVRAM		= Next; Next += 0x004000;

	AllRam			= Next;

	DrvMainRAM		= Next; Next += 0x002000;
	DrvVidRAM		= Next; Next += 0x002000;
	DrvRCRAM		= Next; Next += 0x001000;
	DrvMathRAM		= Next; Next += 0x000800;
	DrvObjRAM		= Next; Next += 0x000400;
	DrvZ80RAM		= Next; Next += 0x000800;

	RamEnd			= Next;

	chr_bmp			= Next; Next += 768 * 240;
	obj_bmp			= Next; Next += 768 * 240;
	rod_bmp			= Next; Next += 768 * 240;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvLoadRoms()
{
	char* pRomName;
	struct BurnRomInfo ri;
	INT32 k = 0;

	if (game_type == GAME_TX1)
	{
		if (BurnLoadRom(DrvMainROM + 0x00000, k++, 2)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x00001, k++, 2)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x08000, k++, 2)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x08001, k++, 2)) return 1;

		if (BurnLoadRom(DrvMathROM + 0x00000, k++, 2)) return 1;
		if (BurnLoadRom(DrvMathROM + 0x00001, k++, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x00000, k++, 1)) return 1;

		if (BurnLoadRom(DrvCharROM + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvCharROM + 0x04000, k++, 1)) return 1;

		for (INT32 i = 0; i < 4; i++) {
			if (BurnLoadRom(DrvObjROM + i * 0x4000, k++, 1)) return 1;
		}

		for (INT32 i = 0; i < 3; i++) {
			if (BurnLoadRom(DrvRoadROM + i * 0x2000, k++, 1)) return 1;
		}

		if (BurnLoadRom(DrvAuData  + 0x00000, k++, 2)) return 1;
		if (BurnLoadRom(DrvAuData  + 0x00001, k++, 2)) return 1;
		if (BurnLoadRom(DrvAuData  + 0x08000, k++, 2)) return 1;
		if (BurnLoadRom(DrvAuData  + 0x08001, k++, 2)) return 1;

		if (BurnLoadRom(DrvObjMap  + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvObjMap  + 0x04000, k++, 1)) return 1;

		if (BurnLoadRom(DrvObjLuts + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvObjLuts + 0x02000, k++, 1)) return 1;

		// rgb palettes, char colour tables, object colour/tile tables, road graphics and stripes
		static const INT32 prom_offsets[19] = {
			0x0000, 0x0100, 0x0200, 0x0300, 0x0400, 0x0500, 0x0600, 0x0700, 0x0800,
			0x0900, 0x0a00, 0x0b00, 0x0c00, 0x0e00, 0x1000, 0x1100, 0x1300, 0x1500, 0x1700
		};

		for (INT32 i = 0; i < 19; i++) {
			if (BurnLoadRom(DrvProms + prom_offsets[i], k++, 1)) return 1;
		}
	}
	else if (game_type == GAME_BUGGYBOY)
	{
		if (BurnLoadRom(DrvMainROM + 0x00000, k++, 2)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x00001, k++, 2)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x10000, k++, 2)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x10001, k++, 2)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x18000, k++, 2)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x18001, k++, 2)) return 1;

		if (BurnLoadRom(DrvMathROM + 0x00000, k++, 2)) return 1;
		if (BurnLoadRom(DrvMathROM + 0x00001, k++, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x00000, k++, 1)) return 1;

		if (BurnLoadRom(DrvCharROM + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvCharROM + 0x04000, k++, 1)) return 1;

		for (INT32 i = 0; i < 12; i++) {
			if (BurnLoadRom(DrvObjROM + i * 0x4000, k++, 1)) return 1;
		}

		memcpy (DrvObjROM + 0x34000, DrvObjROM + 0x20000, 0x4000);
		memcpy (DrvObjROM + 0x30000, DrvObjROM + 0x24000, 0x4000);
		memcpy (DrvObjROM + 0x3c000, DrvObjROM + 0x28000, 0x4000);
		memcpy (DrvObjROM + 0x38000, DrvObjROM + 0x2c000, 0x4000);

		if (BurnLoadRom(DrvRoadROM + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvRoadROM + 0x02000, k++, 1)) return 1;
		if (BurnLoadRom(DrvRoadROM + 0x04000, k++, 1)) return 1;
		if (BurnLoadRom(DrvRoadROM + 0x04200, k++, 1)) return 1;
		if (BurnLoadRom(DrvRoadROM + 0x04400, k++, 1)) return 1;
		if (BurnLoadRom(DrvRoadROM + 0x04600, k++, 1)) return 1;

		if (BurnLoadRom(DrvAuData  + 0x00000, k++, 2)) return 1;
		if (BurnLoadRom(DrvAuData  + 0x00001, k++, 2)) return 1;
		if (BurnLoadRom(DrvAuData  + 0x08000, k++, 2)) return 1;
		if (BurnLoadRom(DrvAuData  + 0x08001, k++, 2)) return 1;

		for (INT32 i = 0; i < 4; i++) {
			if (BurnLoadRom(DrvObjMap + i * 0x4000, k++, 1)) return 1;
		}

		if (BurnLoadRom(DrvObjLuts + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvObjLuts + 0x02000, k++, 1)) return 1;

		// three copies of each rgbi / char / road prom, one per screen
		static const INT32 prom_offsets[22] = {
			0x000, 0x100, 0x200, 0x300, 0x000, 0x100, 0x200, 0x300, 0x000, 0x100, 0x200, 0x300,
			0x400, 0x400, 0x400, 0x500, 0xd00, 0x1500, 0x1500, 0x1500, 0x1600, -1
		};

		for (INT32 i = 0; prom_offsets[i] >= 0; i++) {
			if (BurnLoadRom(DrvProms + prom_offsets[i], k++, 1)) return 1;
		}
	}
	else // GAME_BUGGYBOYJR
	{
		if (BurnLoadRom(DrvMainROM + 0x00000, k++, 2)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x00001, k++, 2)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x10000, k++, 2)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x10001, k++, 2)) return 1;

		if (BurnLoadRom(DrvMathROM + 0x00000, k++, 2)) return 1;
		if (BurnLoadRom(DrvMathROM + 0x00001, k++, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x00000, k++, 1)) return 1;

		if (BurnLoadRom(DrvCharROM + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvCharROM + 0x04000, k++, 1)) return 1;

		for (INT32 i = 0; i < 6; i++) {
			if (BurnLoadRom(DrvObjROM + i * 0x8000, k++, 1)) return 1;
		}

		if (BurnLoadRom(DrvRoadROM + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvRoadROM + 0x04000, k++, 1)) return 1;
		if (BurnLoadRom(DrvRoadROM + 0x04200, k++, 1)) return 1;
		if (BurnLoadRom(DrvRoadROM + 0x04400, k++, 1)) return 1;
		if (BurnLoadRom(DrvRoadROM + 0x04600, k++, 1)) return 1;

		if (BurnLoadRom(DrvAuData  + 0x00000, k++, 2)) return 1;
		if (BurnLoadRom(DrvAuData  + 0x00001, k++, 2)) return 1;
		if (BurnLoadRom(DrvAuData  + 0x08000, k++, 2)) return 1;
		if (BurnLoadRom(DrvAuData  + 0x08001, k++, 2)) return 1;

		if (BurnLoadRom(DrvObjMap  + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvObjMap  + 0x08000, k++, 1)) return 1;

		if (BurnLoadRom(DrvObjLuts + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvObjLuts + 0x02000, k++, 1)) return 1;

		static const INT32 prom_offsets[9] = { 0x000, 0x100, 0x200, 0x300, 0x400, 0x500, 0xd00, 0x1600, 0x1500 };

		for (INT32 i = 0; i < 9; i++) {
			if (BurnLoadRom(DrvProms + prom_offsets[i], k++, 1)) return 1;
		}
	}

	// sanity check: the whole rom list has been used, bar the undumped pals
	while (BurnDrvGetRomInfo(&ri, k) == 0 && BurnDrvGetRomName(&pRomName, k, 0) == 0) {
		if ((ri.nType & BRF_NODUMP) == 0) {
			bprintf(PRINT_ERROR, _T("tx1: rom %d not loaded!\n"), k);
		}
		k++;
	}

	for (INT32 i = 0; i < 0x8000; i++) {
		au_data16[i] = DrvAuData[i * 2 + 0] | (DrvAuData[i * 2 + 1] << 8);
	}

	return 0;
}

static INT32 DrvInit(INT32 type)
{
	game_type = type;
	is_buggyboy = (type != GAME_TX1);
	screen_width = (type == GAME_BUGGYBOYJR) ? 256 : 768;

	BurnAllocMemIndex();

	if (DrvLoadRoms()) return 1;

	vram16 = (UINT16*)DrvVidRAM;
	rcram16 = (UINT16*)DrvRCRAM;
	objram16 = (UINT16*)DrvObjRAM;

	BurnSetRefreshRate(is_buggyboy ? (6000000.0 / (384 * 289)) : (6000000.0 / (384 * 264)));

	VezInit(0, i8086_TYPE);
	VezOpen(0);
	if (is_buggyboy) {
		VezMapMemory(DrvNVRAM,				0x00000, 0x03fff, MAP_RAM);
		VezMapMemory(DrvVidRAM,				0x08000, (game_type == GAME_BUGGYBOYJR) ? 0x08fff : 0x09fff, MAP_RAM);
		VezMapMemory(DrvRCRAM,				0x0a000, 0x0afff, MAP_RAM);
		VezMapMemory(DrvMainROM,			0x20000, 0x2ffff, MAP_ROM);
		VezMapMemory(DrvMainROM + 0x10000,	0xf0000, 0xfffff, MAP_ROM);
	} else {
		VezMapMemory(DrvMainRAM + 0x0000,	0x00000, 0x00fff, MAP_RAM);
		VezMapMemory(DrvMainRAM + 0x0000,	0x01000, 0x01fff, MAP_RAM);
		VezMapMemory(DrvMainRAM + 0x1000,	0x02000, 0x02fff, MAP_RAM);
		VezMapMemory(DrvMainRAM + 0x1000,	0x03000, 0x03fff, MAP_RAM);
		VezMapMemory(DrvNVRAM,				0x04000, 0x04fff, MAP_RAM);
		VezMapMemory(DrvNVRAM,				0x05000, 0x05fff, MAP_RAM);
		VezMapMemory(DrvVidRAM,				0x08000, 0x09fff, MAP_RAM);
		VezMapMemory(DrvRCRAM,				0x0a000, 0x0afff, MAP_RAM);
		// rom is mirrored on address bits 16, 18 and 19
		static const INT32 mirrors[8] = { 0x20000, 0x30000, 0x60000, 0x70000, 0xa0000, 0xb0000, 0xe0000, 0xf0000 };
		for (INT32 i = 0; i < 8; i++) {
			VezMapMemory(DrvMainROM,		mirrors[i], mirrors[i] + 0xffff, MAP_ROM);
		}
	}
	VezSetReadHandler(main_read_byte);
	VezSetWriteHandler(main_write_byte);
	VezSetReadWordHandler(main_read_word_cb);
	VezSetWriteWordHandler(main_write_word_cb);
	VezClose();

	VezInit(1, i8086_TYPE);
	VezOpen(1);
	VezMapMemory(DrvMathRAM,				0x00000, 0x007ff, MAP_RAM);
	VezMapMemory(DrvRCRAM,					0x01000, 0x01fff, MAP_RAM);
	VezMapMemory(DrvObjRAM,					0x02000, 0x023ff, MAP_RAM);
	VezMapMemory(DrvMathROM,				0x04000, 0x04fff, MAP_ROM);
	// 0x5000-0x7fff reads have side effects on the arithmetic unit
	VezMapMemory(DrvMathROM + 0x1000,		0x05000, 0x07fff, MAP_FETCH);
	for (INT32 i = 0x08000; i < 0x100000; i += 0x8000) {
		VezMapMemory(DrvMathROM,			i + 0x4000, i + 0x7fff, MAP_ROM);
	}
	VezSetReadHandler(math_read_byte);
	VezSetWriteHandler(math_write_byte);
	VezSetReadWordHandler(math_read_word_cb);
	VezSetWriteWordHandler(math_write_word_cb);
	VezClose();

	ZetInit(0);
	ZetOpen(0);
	if (is_buggyboy) {
		ZetMapMemory(DrvZ80ROM,				0x0000, 0x3fff, MAP_ROM);
		ZetMapMemory(DrvZ80RAM,				0x4000, 0x47ff, MAP_RAM);
		ZetMapMemory(DrvZ80RAM,				0xc000, 0xc7ff, MAP_RAM); // ts
		ZetSetWriteHandler(buggyboy_sound_write);
		ZetSetReadHandler(buggyboy_sound_read);
		ZetSetOutHandler(buggyboy_sound_write_port);
		ZetSetInHandler(buggyboy_sound_read_port);
	} else {
		ZetMapMemory(DrvZ80ROM,				0x0000, 0x1fff, MAP_ROM);
		ZetMapMemory(DrvZ80RAM,				0x3000, 0x37ff, MAP_RAM);
		ZetMapMemory(DrvZ80RAM,				0x3800, 0x3fff, MAP_RAM);
		ZetMapMemory(DrvZ80RAM,				0xb000, 0xb7ff, MAP_RAM); // ts
		ZetMapMemory(DrvZ80RAM,				0xb800, 0xbfff, MAP_RAM);
		ZetSetWriteHandler(tx1_sound_write);
		ZetSetReadHandler(tx1_sound_read);
		ZetSetOutHandler(tx1_sound_write_port);
		ZetSetInHandler(tx1_sound_read_port);
	}
	ZetClose();

	z80_cycles_per_frame = (INT32)((is_buggyboy ? 3750000 : 3000000) / (nBurnFPS / 100.0));
	z80_irq_period = 8192; // tx1: 6mhz / 4 / 2048 / 2, buggy boy: 7.5mhz / 2 / 4 / 2048

	if (game_type == GAME_TX1) {
		ppi8255_init(1);
		ppi8255_set_read_ports(0, tx1_ppi_porta_r, tx1_ppi_portb_r, tx1_ppi_portc_r);

		AY8910Init(0, 6000000 / 8, 0);
		AY8910SetPorts(0, NULL, NULL, tx1_ay8910_a_w, tx1_ay8910_b_w);
		AY8910SetAllRoutes(0, 0.35, BURN_SND_ROUTE_BOTH);
		AY8910SetBuffered(ZetTotalCycles, 3000000);
	} else {
		if (game_type == GAME_BUGGYBOY) {
			ppi8255_init(1);
			ppi8255_set_read_ports(0, bb_ppi_porta_r, NULL, bb_ppi_portc_r);
		}

		AY8910Init(0, 7500000 / 4, 0);
		AY8910Init(1, 7500000 / 4, 1);
		if (game_type == GAME_BUGGYBOYJR) {
			AY8910SetPorts(0, bbjr_ay_porta_r, bbjr_ay_portb_r, NULL, NULL);
		} else {
			AY8910SetPorts(0, NULL, NULL, bb_ym1_a_w, NULL);
		}
		AY8910SetPorts(1, NULL, NULL, bb_ym2_a_w, bb_ym2_b_w);
		bb_set_ay_gains(0xff);
		AY8910SetBuffered(ZetTotalCycles, 3750000);
	}

	stream.init(nBurnSoundRate, nBurnSoundRate, 2, 1, sound_stream_update);
	stream.set_buffered(ZetTotalCycles, (game_type == GAME_TX1) ? 3000000 : 3750000);

	sound_init_tables();

	BurnTrackballInit(1);

	BurnShiftInit(SHIFT_POSITION_BOTTOM_RIGHT, SHIFT_COLOR_RED, 80);

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	VezExit();
	ZetExit();

	AY8910Exit(0);
	if (is_buggyboy) AY8910Exit(1);

	if (game_type != GAME_BUGGYBOYJR) {
		ppi8255_exit();
	}

	stream.exit();

	BurnTrackballExit();

	BurnShiftExit();

	BurnFreeMemIndex();

	return 0;
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	if (game_type == GAME_TX1) {
		for (INT32 i = 0; i < 3; i++) tx1_combine_layers(i);
	} else if (game_type == GAME_BUGGYBOY) {
		for (INT32 i = 0; i < 3; i++) bb_combine_layers(i);
	} else {
		bb_combine_layers(-1);
	}

	BurnTransferCopy(DrvPalette);

	BurnShiftRenderDoubleSize();

	return 0;
}

static void pedal_accum_u_later()
{
	for (INT32 i = 0; i < 2; i++) {
		if (pedal_accu[i] != pedal_target[i]) {
			if (pedal_accu[i] > pedal_target[i]) {
				pedal_accu[i]--;
			}
			else if (pedal_accu[i] < pedal_target[i]) {
				pedal_accu[i]++;
			}
		}
	}
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset(1);
	}

	VezNewFrame();
	ZetNewFrame();

	{
		DrvJoy2[4] = BurnShiftInputCheckToggle(DrvJoy2[4]);
		gear_state = bBurnShiftStatus ^ 1;

		BurnTrackballConfig(0, AXIS_NORMAL, AXIS_NORMAL);
		BurnTrackballFrame(0, Analog[0], 0, 1, 5);
		BurnTrackballUDLR(0, 0, 0, DrvJoy2[2], DrvJoy2[3]);
		BurnTrackballUpdate(0);

		pedal_target[0] = ProcessAnalog(Analog[1], 0, INPUT_DEADZONE | INPUT_LINEAR | INPUT_MIGHTBEDIGITAL, 0x00, 0xff);
		pedal_target[1] = ProcessAnalog(Analog[2], 0, INPUT_DEADZONE | INPUT_LINEAR | INPUT_MIGHTBEDIGITAL, 0x00, 0xff);
	}

	INT32 nLines = is_buggyboy ? 289 : 264;
	INT32 nInterleave = nLines * 4;
	INT32 nCyclesTotal[3] = { (INT32)(5000000 / (nBurnFPS / 100.0)), (INT32)(5000000 / (nBurnFPS / 100.0)), z80_cycles_per_frame };
	INT32 nCyclesDone[3] = { nExtraCycles[0], nExtraCycles[1], nExtraCycles[2] };

	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		if (i == 240 * 4) {
			vblank_start();
		}

		VezOpen(0);
		if (i == 239 * 4 + 1) {
			VezSetIRQLineAndVector(0, 0xff, CPU_IRQSTATUS_ACK);
		}
		CPU_RUN(0, Vez);
		VezClose();

		VezOpen(1);
		CPU_RUN(1, Vez);
		VezClose();

		INT32 z80_before = nCyclesDone[2];
		CPU_RUN(2, Zet);
		z80_irq_cycles += nCyclesDone[2] - z80_before;

		while (z80_irq_cycles >= z80_irq_period) {
			z80_irq_cycles -= z80_irq_period;
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}

		pedal_cycles++;
		if (pedal_cycles >= 10) {
			pedal_cycles -= 10;
			pedal_accum_u_later();
		}
	}

	ZetClose();

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
		stream.render(pBurnSoundOut, nBurnSoundLen);
	}

	nExtraCycles[0] = nCyclesDone[0] - nCyclesTotal[0];
	nExtraCycles[1] = nCyclesDone[1] - nCyclesTotal[1];
	nExtraCycles[2] = nCyclesDone[2] - nCyclesTotal[2];

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	if (pnMin) {
		*pnMin = 0x029702;
	}

	if (nAction & ACB_MEMORY_RAM) {
		ScanVar(AllRam, RamEnd - AllRam, "All Ram");
	}

	if (nAction & ACB_NVRAM) {
		ScanVar(DrvNVRAM, is_buggyboy ? 0x4000 : 0x1000, "NV RAM");
	}

	if (nAction & ACB_DRIVER_DATA) {
		VezScan(nAction);
		ZetScan(nAction);

		AY8910Scan(nAction, pnMin);
		if (game_type != GAME_BUGGYBOYJR) ppi8255_scan();
		BurnTrackballScan();

		SCAN_VAR(math);
		SCAN_VAR(sn74s516);
		SCAN_VAR(vregs);
		SCAN_VAR(needs_update);

		SCAN_VAR(pit8253);
		SCAN_VAR(ay_outputa);
		SCAN_VAR(ay_outputb);
		SCAN_VAR(ppi_latch_a);
		SCAN_VAR(ppi_latch_b);
		SCAN_VAR(snd_step0);
		SCAN_VAR(snd_step1);
		SCAN_VAR(snd_step2);
		SCAN_VAR(snd_pit0);
		SCAN_VAR(snd_pit1);
		SCAN_VAR(snd_pit2);
		SCAN_VAR(eng0);
		SCAN_VAR(eng1);
		SCAN_VAR(eng2);
		SCAN_VAR(noise_lfsra);
		SCAN_VAR(noise_lfsrb);
		SCAN_VAR(noise_lfsrc);
		SCAN_VAR(noise_lfsrd);
		SCAN_VAR(noise_counter);
		SCAN_VAR(ym1_outputa);
		SCAN_VAR(ym2_outputa);
		SCAN_VAR(ym2_outputb);
		SCAN_VAR(snd_gain);

		SCAN_VAR(z80_irq_cycles);
		SCAN_VAR(pedal_accu);
		SCAN_VAR(pedal_target);
		SCAN_VAR(pedal_cycles);

		SCAN_VAR(nExtraCycles);
	}

	if (nAction & ACB_WRITE) {
		if (is_buggyboy) {
			bb_set_ay_gains(ym2_outputb ^ 0xff);
		}
	}

	return 0;
}

static INT32 Tx1Init()
{
	return DrvInit(GAME_TX1);
}

static INT32 BuggyboyInit()
{
	return DrvInit(GAME_BUGGYBOY);
}

static INT32 BuggyboyjrInit()
{
	return DrvInit(GAME_BUGGYBOYJR);
}


// TX-1 (World)

static struct BurnRomInfo tx1RomDesc[] = {
	{ "8412-136027-244.22",    0x04000, 0x2e9cefa2, 1 | BRF_PRG | BRF_ESS }, //  0 Main 8086 Code
	{ "8412-136027-245.29",    0x04000, 0xade7895c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "8412-136027-249.45",    0x04000, 0x9bcb82db, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "8412-136027-250.54",    0x04000, 0xc8c9368f, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "8411-136027-152.146",   0x02000, 0xb65eeea2, 2 | BRF_PRG | BRF_ESS }, //  4 Math 8086 Code
	{ "8411-136027-151.132",   0x02000, 0x0d63dadc, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "8411-136027-157.11",    0x02000, 0x10ae3075, 3 | BRF_PRG | BRF_ESS }, //  6 Z80 Code
	{ "8411-136027-156.204",   0x04000, 0x60f3c616, 4 | BRF_GRA },  //  7 Characters
	{ "8411-136027-155.174",   0x04000, 0xe59a6b72, 4 | BRF_GRA },  //  8
	{ "8411-136027-114.203",   0x04000, 0xfc91328b, 5 | BRF_GRA },  //  9 Objects
	{ "8411-136027-116.258",   0x04000, 0x5745f671, 5 | BRF_GRA },  // 10
	{ "8411-136027-115.173",   0x04000, 0x720e5873, 5 | BRF_GRA },  // 11
	{ "8411-136027-117.232",   0x04000, 0x3c68d0bc, 5 | BRF_GRA },  // 12
	{ "8411-136027-146.56",    0x02000, 0x5635b8c1, 6 | BRF_GRA },  // 13 Road Data
	{ "8411-136027-147.66",    0x02000, 0x03d83cf8, 6 | BRF_GRA },  // 14
	{ "8411-136027-148.76",    0x02000, 0xad56013a, 6 | BRF_GRA },  // 15
	{ "8411-136027-153.184",   0x04000, 0xacf754e8, 7 | BRF_PRG | BRF_ESS }, // 16 Arithmetic Unit Data & PROMs
	{ "8411-136027-154.185",   0x04000, 0xf89d3e20, 7 | BRF_PRG | BRF_ESS }, // 17
	{ "136027-143.ic223",      0x00200, 0x22c77af6, 7 | BRF_PRG | BRF_ESS }, // 18
	{ "136027-142.ic213",      0x00200, 0xf6b8b70b, 7 | BRF_PRG | BRF_ESS }, // 19
	{ "8411-136027-119.106",   0x04000, 0x88eec0fb, 8 | BRF_GRA },  // 20 Object Tile Map
	{ "8411-136027-120.73",    0x04000, 0x407cbe65, 8 | BRF_GRA },  // 21
	{ "8411-136027-113.48",    0x02000, 0x4b3d7956, 9 | BRF_GRA },  // 22 Object LUTs
	{ "8411-136027-118.281",   0x04000, 0xde418dc7, 9 | BRF_GRA },  // 23
	{ "136027-133.57",         0x00100, 0x3b387d01, 10 | BRF_GRA }, // 24 Color & Misc PROMs
	{ "136027-134.58",         0x00100, 0xf6f4d7d9, 10 | BRF_GRA }, // 25
	{ "136027-135.59",         0x00100, 0x824e7532, 10 | BRF_GRA }, // 26
	{ "136027-133.36",         0x00100, 0x3b387d01, 10 | BRF_GRA }, // 27
	{ "136027-134.37",         0x00100, 0xf6f4d7d9, 10 | BRF_GRA }, // 28
	{ "136027-135.38",         0x00100, 0x824e7532, 10 | BRF_GRA }, // 29
	{ "136027-133.8",          0x00100, 0x3b387d01, 10 | BRF_GRA }, // 30
	{ "136027-134.9",          0x00100, 0xf6f4d7d9, 10 | BRF_GRA }, // 31
	{ "136027-135.10",         0x00100, 0x824e7532, 10 | BRF_GRA }, // 32
	{ "136027-124.85",         0x00100, 0x5aeef5cc, 10 | BRF_GRA }, // 33
	{ "136027-124.116",        0x00100, 0x5aeef5cc, 10 | BRF_GRA }, // 34
	{ "136027-124.148",        0x00100, 0x5aeef5cc, 10 | BRF_GRA }, // 35
	{ "136027-136.276",        0x00200, 0x7b675b8b, 10 | BRF_GRA }, // 36
	{ "136027-136.277",        0x00200, 0x7b675b8b, 10 | BRF_GRA }, // 37
	{ "136027-123.25",         0x00100, 0x616a7a85, 10 | BRF_GRA }, // 38
	{ "136027-138.ic33",       0x00200, 0xfafb6917, 10 | BRF_GRA }, // 39
	{ "136027-139.ic40",       0x00200, 0x93deb894, 10 | BRF_GRA }, // 40
	{ "136027-140.ic49",       0x00200, 0xaa5ed232, 10 | BRF_GRA }, // 41
	{ "136027-141.ic50",       0x00200, 0x6b424cea, 10 | BRF_GRA }, // 42
};

STD_ROM_PICK(tx1)
STD_ROM_FN(tx1)

struct BurnDriver BurnDrvTx1 = {
	"tx1", NULL, NULL, NULL, "1983",
	"TX-1 (World)\0", NULL, "Tatsumi (Atari/Namco/Taito license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, tx1RomInfo, tx1RomName, NULL, NULL, NULL, NULL, Tx1InputInfo, Tx1DIPInfo,
	Tx1Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	768, 240, 12, 3
};

// TX-1 (Japan rev. B)

static struct BurnRomInfo tx1jbRomDesc[] = {
	{ "tx1_1b.ic22",           0x04000, 0x3dadb8fb, 1 | BRF_PRG | BRF_ESS }, //  0 Main 8086 Code
	{ "tx1_2b.ic29",           0x04000, 0x6b5a8f23, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tx1_3b.ic45",           0x04000, 0x3a030996, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "tx1_4b.ic54",           0x04000, 0x7aed82ed, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "tx1_9b.ic146",          0x02000, 0xaf677ff4, 2 | BRF_PRG | BRF_ESS }, //  4 Math 8086 Code
	{ "tx1_8b.ic132",          0x02000, 0xdd4356f8, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "tx1_22h.ic9",           0x02000, 0x66376232, 3 | BRF_PRG | BRF_ESS }, //  6 Z80 Code
	{ "tx1_21a.ic204",         0x04000, 0xcd3441ad, 4 | BRF_GRA },  //  7 Characters
	{ "tx1_20a.ic174",         0x04000, 0xdbe595fc, 4 | BRF_GRA },  //  8
	{ "tx1_16b.ic203",         0x04000, 0x1141c965, 5 | BRF_GRA },  //  9 Objects
	{ "tx1_18b.ic258",         0x04000, 0x0ad36d68, 5 | BRF_GRA },  // 10
	{ "tx1_15b.ic173",         0x04000, 0x30d1a8d5, 5 | BRF_GRA },  // 11
	{ "tx1_17b.ic232",         0x04000, 0x364bb354, 5 | BRF_GRA },  // 12
	{ "tx1_5a.ic56",           0x02000, 0x5635b8c1, 6 | BRF_GRA },  // 13 Road Data
	{ "tx1_6a.ic66",           0x02000, 0x03d83cf8, 6 | BRF_GRA },  // 14
	{ "tx1_7a.ic76",           0x02000, 0xad56013a, 6 | BRF_GRA },  // 15
	{ "tx1_10b.ic184",         0x04000, 0xacf754e8, 7 | BRF_PRG | BRF_ESS }, // 16 Arithmetic Unit Data & PROMs
	{ "tx1_11b.ic185",         0x04000, 0xf89d3e20, 7 | BRF_PRG | BRF_ESS }, // 17
	{ "xb02b.ic223",           0x00200, 0x22c77af6, 7 | BRF_PRG | BRF_ESS }, // 18
	{ "xb01b.ic213",           0x00200, 0xf6b8b70b, 7 | BRF_PRG | BRF_ESS }, // 19
	{ "tx1_14b.ic106",         0x04000, 0x68c63d6e, 8 | BRF_GRA },  // 20 Object Tile Map
	{ "tx1_13b.ic73",          0x04000, 0xb0c581b2, 8 | BRF_GRA },  // 21
	{ "tx1_12b.ic48",          0x02000, 0x4b3d7956, 9 | BRF_GRA },  // 22 Object LUTs
	{ "tx1_19b.ic281",         0x04000, 0xcb250de6, 9 | BRF_GRA },  // 23
	{ "xb05a.ic57",            0x00100, 0x3b387d01, 10 | BRF_GRA }, // 24 Color & Misc PROMs
	{ "xb06a.ic58",            0x00100, 0xf6f4d7d9, 10 | BRF_GRA }, // 25
	{ "xb07a.ic59",            0x00100, 0x824e7532, 10 | BRF_GRA }, // 26
	{ "xb05a.ic36",            0x00100, 0x3b387d01, 10 | BRF_GRA }, // 27
	{ "xb06a.ic37",            0x00100, 0xf6f4d7d9, 10 | BRF_GRA }, // 28
	{ "xb07a.ic38",            0x00100, 0x824e7532, 10 | BRF_GRA }, // 29
	{ "xb05a.ic8",             0x00100, 0x3b387d01, 10 | BRF_GRA }, // 30
	{ "xb06a.ic9",             0x00100, 0xf6f4d7d9, 10 | BRF_GRA }, // 31
	{ "xb07a.ic10",            0x00100, 0x824e7532, 10 | BRF_GRA }, // 32
	{ "xb08.ic85",             0x00100, 0x5aeef5cc, 10 | BRF_GRA }, // 33
	{ "xb08.ic116",            0x00100, 0x5aeef5cc, 10 | BRF_GRA }, // 34
	{ "xb08.ic148",            0x00100, 0x5aeef5cc, 10 | BRF_GRA }, // 35
	{ "xb04a.ic276",           0x00200, 0x92bf5533, 10 | BRF_GRA }, // 36
	{ "xb04a.ic277",           0x00200, 0x92bf5533, 10 | BRF_GRA }, // 37
	{ "xb03a.ic25",            0x00100, 0x616a7a85, 10 | BRF_GRA }, // 38
	{ "xb09.ic33",             0x00200, 0xfafb6917, 10 | BRF_GRA }, // 39
	{ "xb10.ic40",             0x00200, 0x93deb894, 10 | BRF_GRA }, // 40
	{ "xb11.ic49",             0x00200, 0xaa5ed232, 10 | BRF_GRA }, // 41
	{ "xb12.ic50",             0x00200, 0x6b424cea, 10 | BRF_GRA }, // 42
};

STD_ROM_PICK(tx1jb)
STD_ROM_FN(tx1jb)

struct BurnDriver BurnDrvTx1jb = {
	"tx1jb", "tx1", NULL, NULL, "1983",
	"TX-1 (Japan rev. B)\0", NULL, "Tatsumi", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, tx1jbRomInfo, tx1jbRomName, NULL, NULL, NULL, NULL, Tx1InputInfo, Tx1jDIPInfo,
	Tx1Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	768, 240, 12, 3
};

// TX-1 (Japan rev. C)

static struct BurnRomInfo tx1jcRomDesc[] = {
	{ "tx1_1c.ic22",           0x04000, 0xeedcee83, 1 | BRF_PRG | BRF_ESS }, //  0 Main 8086 Code
	{ "tx1_2c.ic29",           0x04000, 0x294bf5bf, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tx1_3c.ic45",           0x04000, 0x21a8aa55, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "tx1_4c.ic54",           0x04000, 0x15bb8ef2, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "tx1_9c.ic146",          0x02000, 0xb65eeea2, 2 | BRF_PRG | BRF_ESS }, //  4 Math 8086 Code
	{ "tx1_8c.ic132",          0x02000, 0x0d63dadc, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "8411-136027-157.11",    0x02000, 0x10ae3075, 3 | BRF_PRG | BRF_ESS }, //  6 Z80 Code
	{ "tx1_21a.ic204",         0x04000, 0xcd3441ad, 4 | BRF_GRA },  //  7 Characters
	{ "tx1_20a.ic174",         0x04000, 0xdbe595fc, 4 | BRF_GRA },  //  8
	{ "tx1_16b.ic203",         0x04000, 0x1141c965, 5 | BRF_GRA },  //  9 Objects
	{ "tx1_18b.ic258",         0x04000, 0x0ad36d68, 5 | BRF_GRA },  // 10
	{ "tx1_15b.ic173",         0x04000, 0x30d1a8d5, 5 | BRF_GRA },  // 11
	{ "tx1_17b.ic232",         0x04000, 0x364bb354, 5 | BRF_GRA },  // 12
	{ "tx1_5a.ic56",           0x02000, 0x5635b8c1, 6 | BRF_GRA },  // 13 Road Data
	{ "tx1_6a.ic66",           0x02000, 0x03d83cf8, 6 | BRF_GRA },  // 14
	{ "tx1_7a.ic76",           0x02000, 0xad56013a, 6 | BRF_GRA },  // 15
	{ "tx1_10b.ic184",         0x04000, 0xacf754e8, 7 | BRF_PRG | BRF_ESS }, // 16 Arithmetic Unit Data & PROMs
	{ "tx1_11b.ic185",         0x04000, 0xf89d3e20, 7 | BRF_PRG | BRF_ESS }, // 17
	{ "xb02b.ic223",           0x00200, 0x22c77af6, 7 | BRF_PRG | BRF_ESS }, // 18
	{ "xb01b.ic213",           0x00200, 0xf6b8b70b, 7 | BRF_PRG | BRF_ESS }, // 19
	{ "tx1_14b.ic106",         0x04000, 0x68c63d6e, 8 | BRF_GRA },  // 20 Object Tile Map
	{ "tx1_13b.ic73",          0x04000, 0xb0c581b2, 8 | BRF_GRA },  // 21
	{ "tx1_12b.ic48",          0x02000, 0x4b3d7956, 9 | BRF_GRA },  // 22 Object LUTs
	{ "tx1_19b.ic281",         0x04000, 0xcb250de6, 9 | BRF_GRA },  // 23
	{ "xb05a.ic57",            0x00100, 0x3b387d01, 10 | BRF_GRA }, // 24 Color & Misc PROMs
	{ "xb06a.ic58",            0x00100, 0xf6f4d7d9, 10 | BRF_GRA }, // 25
	{ "xb07a.ic59",            0x00100, 0x824e7532, 10 | BRF_GRA }, // 26
	{ "xb05a.ic36",            0x00100, 0x3b387d01, 10 | BRF_GRA }, // 27
	{ "xb06a.ic37",            0x00100, 0xf6f4d7d9, 10 | BRF_GRA }, // 28
	{ "xb07a.ic38",            0x00100, 0x824e7532, 10 | BRF_GRA }, // 29
	{ "xb05a.ic8",             0x00100, 0x3b387d01, 10 | BRF_GRA }, // 30
	{ "xb06a.ic9",             0x00100, 0xf6f4d7d9, 10 | BRF_GRA }, // 31
	{ "xb07a.ic10",            0x00100, 0x824e7532, 10 | BRF_GRA }, // 32
	{ "xb08.ic85",             0x00100, 0x5aeef5cc, 10 | BRF_GRA }, // 33
	{ "xb08.ic116",            0x00100, 0x5aeef5cc, 10 | BRF_GRA }, // 34
	{ "xb08.ic148",            0x00100, 0x5aeef5cc, 10 | BRF_GRA }, // 35
	{ "xb04a.ic276",           0x00200, 0x92bf5533, 10 | BRF_GRA }, // 36
	{ "xb04a.ic277",           0x00200, 0x92bf5533, 10 | BRF_GRA }, // 37
	{ "xb03a.ic25",            0x00100, 0x616a7a85, 10 | BRF_GRA }, // 38
	{ "xb09.ic33",             0x00200, 0xfafb6917, 10 | BRF_GRA }, // 39
	{ "xb10.ic40",             0x00200, 0x93deb894, 10 | BRF_GRA }, // 40
	{ "xb11.ic49",             0x00200, 0xaa5ed232, 10 | BRF_GRA }, // 41
	{ "xb12.ic50",             0x00200, 0x6b424cea, 10 | BRF_GRA }, // 42
};

STD_ROM_PICK(tx1jc)
STD_ROM_FN(tx1jc)

struct BurnDriver BurnDrvTx1jc = {
	"tx1jc", "tx1", NULL, NULL, "1983",
	"TX-1 (Japan rev. C)\0", NULL, "Tatsumi", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, tx1jcRomInfo, tx1jcRomName, NULL, NULL, NULL, NULL, Tx1InputInfo, Tx1jDIPInfo,
	Tx1Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	768, 240, 12, 3
};

// Buggy Boy/Speed Buggy (cockpit, rev. D)

static struct BurnRomInfo buggyboyRomDesc[] = {
	{ "bug1a.230",             0x08000, 0x92797c25, 1 | BRF_PRG | BRF_ESS }, //  0 Main 8086 Code
	{ "bug4a.173",             0x08000, 0x40ce3930, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "bug2d.231",             0x04000, 0xf67bd593, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "bug5d.174",             0x04000, 0xd0dd6ffc, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "bug3b.232",             0x04000, 0x43cce3f0, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "bug6b.175",             0x04000, 0x8f000dfa, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "bug8a.061",             0x02000, 0x512291cd, 2 | BRF_PRG | BRF_ESS }, //  6 Math 8086 Code
	{ "bug7a.060",             0x02000, 0xd24dfdef, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "bug35.11",              0x04000, 0x7aa16e9e, 3 | BRF_PRG | BRF_ESS }, //  8 Z80 Code
	{ "bug34a",                0x04000, 0x8c64eb3c, 4 | BRF_GRA },  //  9 Characters
	{ "bug33a.168",            0x04000, 0xb500d683, 4 | BRF_GRA },  // 10
	{ "bug26.275",             0x04000, 0xfc6fce64, 5 | BRF_GRA },  // 11 Objects
	{ "bug27.245",             0x04000, 0x54b9a9dd, 5 | BRF_GRA },  // 12
	{ "bug19.274",             0x04000, 0x3392c2ef, 5 | BRF_GRA },  // 13
	{ "bug20.244",             0x04000, 0x9ee4c236, 5 | BRF_GRA },  // 14
	{ "bug28.243",             0x04000, 0xfeef6c27, 5 | BRF_GRA },  // 15
	{ "bug29.212",             0x04000, 0xf570e00b, 5 | BRF_GRA },  // 16
	{ "bug21.242",             0x04000, 0x088fef40, 5 | BRF_GRA },  // 17
	{ "bug22.211",             0x04000, 0x5ec02630, 5 | BRF_GRA },  // 18
	{ "bug30.186",             0x04000, 0x5c2ecabf, 5 | BRF_GRA },  // 19
	{ "bug32.158",             0x04000, 0x125461f2, 5 | BRF_GRA },  // 20
	{ "bug23.185",             0x04000, 0xcafb4d4a, 5 | BRF_GRA },  // 21
	{ "bug25.157",             0x04000, 0x80c4e045, 5 | BRF_GRA },  // 22
	{ "bug12.58",              0x02000, 0xbd34d55c, 6 | BRF_GRA },  // 23 Road Data
	{ "bug11.57",              0x02000, 0xa44d43eb, 6 | BRF_GRA },  // 24
	{ "bb3.137",               0x00200, 0xad76f3fb, 6 | BRF_GRA },  // 25
	{ "bb4.138",               0x00200, 0xe4ca4ea0, 6 | BRF_GRA },  // 26
	{ "bb5.139",               0x00200, 0xe2577a9a, 6 | BRF_GRA },  // 27
	{ "bb6.94",                0x00200, 0xad43e02a, 6 | BRF_GRA },  // 28
	{ "bug9.170",              0x04000, 0x7d84135b, 7 | BRF_PRG | BRF_ESS }, // 29 Arithmetic Unit Data & PROMs
	{ "bug10.171",             0x04000, 0xb518dd6f, 7 | BRF_PRG | BRF_ESS }, // 30
	{ "bb1.245",               0x00200, 0x0ddbd36d, 7 | BRF_PRG | BRF_ESS }, // 31
	{ "bb2.220",               0x00200, 0x71d47de1, 7 | BRF_PRG | BRF_ESS }, // 32
	{ "bug16.210",             0x04000, 0x8b64409e, 8 | BRF_GRA },  // 33 Object Tile Map
	{ "bug14.209",             0x04000, 0x4e765282, 8 | BRF_GRA },  // 34
	{ "bug17.182",             0x04000, 0xa5d84df6, 8 | BRF_GRA },  // 35
	{ "bug15.181",             0x04000, 0xd519de10, 8 | BRF_GRA },  // 36
	{ "bug13.124",             0x02000, 0x53604d7a, 9 | BRF_GRA },  // 37 Object LUTs
	{ "bug18.156",             0x04000, 0xe58321a6, 9 | BRF_GRA },  // 38
	{ "bb10.191",              0x00100, 0xf2368398, 10 | BRF_GRA }, // 39 Color & Misc PROMs
	{ "bb11.192",              0x00100, 0xbf77f624, 10 | BRF_GRA }, // 40
	{ "bb12.193",              0x00100, 0x10a2e8d1, 10 | BRF_GRA }, // 41
	{ "bb13.194",              0x00100, 0x40d10dfa, 10 | BRF_GRA }, // 42
	{ "bb10.104",              0x00100, 0xf2368398, 10 | BRF_GRA }, // 43
	{ "bb11.105",              0x00100, 0xbf77f624, 10 | BRF_GRA }, // 44
	{ "bb12.106",              0x00100, 0x10a2e8d1, 10 | BRF_GRA }, // 45
	{ "bb13.107",              0x00100, 0x40d10dfa, 10 | BRF_GRA }, // 46
	{ "bb10.49",               0x00100, 0xf2368398, 10 | BRF_GRA }, // 47
	{ "bb11.50",               0x00100, 0xbf77f624, 10 | BRF_GRA }, // 48
	{ "bb12.51",               0x00100, 0x10a2e8d1, 10 | BRF_GRA }, // 49
	{ "bb13.52",               0x00100, 0x40d10dfa, 10 | BRF_GRA }, // 50
	{ "bb14.199",              0x00100, 0x0b821e0b, 10 | BRF_GRA }, // 51
	{ "bb14.197",              0x00100, 0x0b821e0b, 10 | BRF_GRA }, // 52
	{ "bb14.137",              0x00100, 0x0b821e0b, 10 | BRF_GRA }, // 53
	{ "bb9.271",               0x00800, 0x6fc807d1, 10 | BRF_GRA }, // 54
	{ "bb9.238",               0x00800, 0x6fc807d1, 10 | BRF_GRA }, // 55
	{ "bb7.16",                0x00100, 0xb57b609f, 10 | BRF_GRA }, // 56
	{ "bb7.18",                0x00100, 0xb57b609f, 10 | BRF_GRA }, // 57
	{ "bb7.20",                0x00100, 0xb57b609f, 10 | BRF_GRA }, // 58
	{ "bb8.152",               0x00100, 0x2330ff4f, 10 | BRF_GRA }, // 59
};

STD_ROM_PICK(buggyboy)
STD_ROM_FN(buggyboy)

struct BurnDriver BurnDrvBuggyboy = {
	"buggyboy", NULL, NULL, NULL, "1985",
	"Buggy Boy/Speed Buggy (cockpit, rev. D)\0", NULL, "Tatsumi", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, buggyboyRomInfo, buggyboyRomName, NULL, NULL, NULL, NULL, BuggyboyInputInfo, BuggyboyDIPInfo,
	BuggyboyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	768, 240, 12, 3
};

// Buggy Boy/Speed Buggy (cockpit, rev. B)

static struct BurnRomInfo buggyboybRomDesc[] = {
	{ "bug1a.230",             0x08000, 0x92797c25, 1 | BRF_PRG | BRF_ESS }, //  0 Main 8086 Code
	{ "bug4a.173",             0x08000, 0x40ce3930, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "bug2b.231",             0x04000, 0x44ebf66d, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "bug5b.174",             0x04000, 0xd02dd360, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "bug3b.232",             0x04000, 0x43cce3f0, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "bug6b.175",             0x04000, 0x8f000dfa, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "bug8a.061",             0x02000, 0x512291cd, 2 | BRF_PRG | BRF_ESS }, //  6 Math 8086 Code
	{ "bug7a.060",             0x02000, 0xd24dfdef, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "bug35.11",              0x04000, 0x7aa16e9e, 3 | BRF_PRG | BRF_ESS }, //  8 Z80 Code
	{ "bug34a",                0x04000, 0x8c64eb3c, 4 | BRF_GRA },  //  9 Characters
	{ "bug33a.168",            0x04000, 0xb500d683, 4 | BRF_GRA },  // 10
	{ "bug26.275",             0x04000, 0xfc6fce64, 5 | BRF_GRA },  // 11 Objects
	{ "bug27.245",             0x04000, 0x54b9a9dd, 5 | BRF_GRA },  // 12
	{ "bug19.274",             0x04000, 0x3392c2ef, 5 | BRF_GRA },  // 13
	{ "bug20.244",             0x04000, 0x9ee4c236, 5 | BRF_GRA },  // 14
	{ "bug28.243",             0x04000, 0xfeef6c27, 5 | BRF_GRA },  // 15
	{ "bug29.212",             0x04000, 0xf570e00b, 5 | BRF_GRA },  // 16
	{ "bug21.242",             0x04000, 0x088fef40, 5 | BRF_GRA },  // 17
	{ "bug22.211",             0x04000, 0x5ec02630, 5 | BRF_GRA },  // 18
	{ "bug30.186",             0x04000, 0x5c2ecabf, 5 | BRF_GRA },  // 19
	{ "bug32.158",             0x04000, 0x125461f2, 5 | BRF_GRA },  // 20
	{ "bug23.185",             0x04000, 0xcafb4d4a, 5 | BRF_GRA },  // 21
	{ "bug25.157",             0x04000, 0x80c4e045, 5 | BRF_GRA },  // 22
	{ "bug12.58",              0x02000, 0xbd34d55c, 6 | BRF_GRA },  // 23 Road Data
	{ "bug11.57",              0x02000, 0xa44d43eb, 6 | BRF_GRA },  // 24
	{ "bb3.137",               0x00200, 0xad76f3fb, 6 | BRF_GRA },  // 25
	{ "bb4.138",               0x00200, 0xe4ca4ea0, 6 | BRF_GRA },  // 26
	{ "bb5.139",               0x00200, 0xe2577a9a, 6 | BRF_GRA },  // 27
	{ "bb6.94",                0x00200, 0xad43e02a, 6 | BRF_GRA },  // 28
	{ "bug9.170",              0x04000, 0x7d84135b, 7 | BRF_PRG | BRF_ESS }, // 29 Arithmetic Unit Data & PROMs
	{ "bug10.171",             0x04000, 0xb518dd6f, 7 | BRF_PRG | BRF_ESS }, // 30
	{ "bb1.245",               0x00200, 0x0ddbd36d, 7 | BRF_PRG | BRF_ESS }, // 31
	{ "bb2.220",               0x00200, 0x71d47de1, 7 | BRF_PRG | BRF_ESS }, // 32
	{ "bug16.210",             0x04000, 0x8b64409e, 8 | BRF_GRA },  // 33 Object Tile Map
	{ "bug14.209",             0x04000, 0x4e765282, 8 | BRF_GRA },  // 34
	{ "bug17.182",             0x04000, 0xa5d84df6, 8 | BRF_GRA },  // 35
	{ "bug15.181",             0x04000, 0xd519de10, 8 | BRF_GRA },  // 36
	{ "bug13.124",             0x02000, 0x53604d7a, 9 | BRF_GRA },  // 37 Object LUTs
	{ "bug18.156",             0x04000, 0xe58321a6, 9 | BRF_GRA },  // 38
	{ "bb10.191",              0x00100, 0xf2368398, 10 | BRF_GRA }, // 39 Color & Misc PROMs
	{ "bb11.192",              0x00100, 0xbf77f624, 10 | BRF_GRA }, // 40
	{ "bb12.193",              0x00100, 0x10a2e8d1, 10 | BRF_GRA }, // 41
	{ "bb13.194",              0x00100, 0x40d10dfa, 10 | BRF_GRA }, // 42
	{ "bb10.104",              0x00100, 0xf2368398, 10 | BRF_GRA }, // 43
	{ "bb11.105",              0x00100, 0xbf77f624, 10 | BRF_GRA }, // 44
	{ "bb12.106",              0x00100, 0x10a2e8d1, 10 | BRF_GRA }, // 45
	{ "bb13.107",              0x00100, 0x40d10dfa, 10 | BRF_GRA }, // 46
	{ "bb10.49",               0x00100, 0xf2368398, 10 | BRF_GRA }, // 47
	{ "bb11.50",               0x00100, 0xbf77f624, 10 | BRF_GRA }, // 48
	{ "bb12.51",               0x00100, 0x10a2e8d1, 10 | BRF_GRA }, // 49
	{ "bb13.52",               0x00100, 0x40d10dfa, 10 | BRF_GRA }, // 50
	{ "bb14.199",              0x00100, 0x0b821e0b, 10 | BRF_GRA }, // 51
	{ "bb14.197",              0x00100, 0x0b821e0b, 10 | BRF_GRA }, // 52
	{ "bb14.137",              0x00100, 0x0b821e0b, 10 | BRF_GRA }, // 53
	{ "bb9.271",               0x00800, 0x6fc807d1, 10 | BRF_GRA }, // 54
	{ "bb9.238",               0x00800, 0x6fc807d1, 10 | BRF_GRA }, // 55
	{ "bb7.16",                0x00100, 0xb57b609f, 10 | BRF_GRA }, // 56
	{ "bb7.18",                0x00100, 0xb57b609f, 10 | BRF_GRA }, // 57
	{ "bb7.20",                0x00100, 0xb57b609f, 10 | BRF_GRA }, // 58
	{ "bb8.152",               0x00100, 0x2330ff4f, 10 | BRF_GRA }, // 59
};

STD_ROM_PICK(buggyboyb)
STD_ROM_FN(buggyboyb)

struct BurnDriver BurnDrvBuggyboyb = {
	"buggyboyb", "buggyboy", NULL, NULL, "1986",
	"Buggy Boy/Speed Buggy (cockpit, rev. B)\0", NULL, "Tatsumi", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, buggyboybRomInfo, buggyboybRomName, NULL, NULL, NULL, NULL, BuggyboyInputInfo, BuggyboyDIPInfo,
	BuggyboyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	768, 240, 12, 3
};

// Buggy Boy Junior/Speed Buggy (upright)

static struct BurnRomInfo buggyboyjrRomDesc[] = {
	{ "bug1a.214",             0x08000, 0x92797c25, 1 | BRF_PRG | BRF_ESS }, //  0 Main 8086 Code
	{ "bug4a.175",             0x08000, 0x40ce3930, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "bug2s.213",             0x08000, 0xabcc8ad2, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "bug5s.174",             0x08000, 0x5e352d8d, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "bug8s.26",              0x02000, 0xefd66282, 2 | BRF_PRG | BRF_ESS }, //  4 Math 8086 Code
	{ "bug7s.25",              0x02000, 0xbd75b5eb, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "bug35s.21",             0x04000, 0x65d9af57, 3 | BRF_PRG | BRF_ESS }, //  6 Z80 Code
	{ "bug34s.46",             0x04000, 0x8ea8fec4, 4 | BRF_GRA },  //  7 Characters
	{ "bug33s.47",             0x04000, 0x459c2b03, 4 | BRF_GRA },  //  8
	{ "bug26s.147",            0x08000, 0x14033710, 5 | BRF_GRA },  //  9 Objects
	{ "bug19s.144",            0x08000, 0x838e0697, 5 | BRF_GRA },  // 10
	{ "bug28s.146",            0x08000, 0x8b47d227, 5 | BRF_GRA },  // 11
	{ "bug21s.143",            0x08000, 0x876a5666, 5 | BRF_GRA },  // 12
	{ "bug30s.145",            0x08000, 0x11d8e2a8, 5 | BRF_GRA },  // 13
	{ "bug23s.142",            0x08000, 0x015db5d8, 5 | BRF_GRA },  // 14
	{ "bug11s.225",            0x04000, 0x771af4e1, 6 | BRF_GRA },  // 15 Road Data
	{ "bb3s.195",              0x00200, 0x2ab3d5ff, 6 | BRF_GRA },  // 16
	{ "bb4s.193",              0x00200, 0x630f68a4, 6 | BRF_GRA },  // 17
	{ "bb5s.194",              0x00200, 0x65925c9e, 6 | BRF_GRA },  // 18
	{ "bb6.224",               0x00200, 0xad43e02a, 6 | BRF_GRA },  // 19
	{ "bug9.138",              0x04000, 0x7d84135b, 7 | BRF_PRG | BRF_ESS }, // 20 Arithmetic Unit Data & PROMs
	{ "bug10.95",              0x04000, 0xb518dd6f, 7 | BRF_PRG | BRF_ESS }, // 21
	{ "bb1.163",               0x00200, 0x0ddbd36d, 7 | BRF_PRG | BRF_ESS }, // 22
	{ "bb2.162",               0x00200, 0x71d47de1, 7 | BRF_PRG | BRF_ESS }, // 23
	{ "bug16s.139",            0x08000, 0x1903a9ad, 8 | BRF_GRA },  // 24 Object Tile Map
	{ "bug17s.140",            0x08000, 0x82cabdd4, 8 | BRF_GRA },  // 25
	{ "bug13.32",              0x02000, 0x53604d7a, 9 | BRF_GRA },  // 26 Object LUTs
	{ "bug18s.141",            0x04000, 0x67786327, 9 | BRF_GRA },  // 27
	{ "bb10.41",               0x00100, 0xf2368398, 10 | BRF_GRA }, // 28 Color & Misc PROMs
	{ "bb11.40",               0x00100, 0xbf77f624, 10 | BRF_GRA }, // 29
	{ "bb12.39",               0x00100, 0x10a2e8d1, 10 | BRF_GRA }, // 30
	{ "bb13.42",               0x00100, 0x40d10dfa, 10 | BRF_GRA }, // 31
	{ "bb14.19",               0x00100, 0x0b821e0b, 10 | BRF_GRA }, // 32
	{ "bb9.190",               0x00800, 0x6fc807d1, 10 | BRF_GRA }, // 33
	{ "bb9.162",               0x00800, 0x6fc807d1, 10 | BRF_GRA }, // 34
	{ "bb8.31",                0x00100, 0x2330ff4f, 10 | BRF_GRA }, // 35
	{ "bb7.188",               0x00100, 0xb57b609f, 10 | BRF_GRA }, // 36
	{ "pal10l8cn.ic16",        0x00001, 0x00000000, 11 | BRF_NODUMP }, // 37 PALs
	{ "pal10h8cn.ic82",        0x00001, 0x00000000, 11 | BRF_NODUMP }, // 38
	{ "pal14h4cn.ic83",        0x00001, 0x00000000, 11 | BRF_NODUMP }, // 39
	{ "pal16r4a-2cn.ic83",     0x00001, 0x00000000, 11 | BRF_NODUMP }, // 40
	{ "pal12l6cn.ic87",        0x00001, 0x00000000, 11 | BRF_NODUMP }, // 41
	{ "pal10l8cn.ic88",        0x00001, 0x00000000, 11 | BRF_NODUMP }, // 42
	{ "pal14h4cn.ic149",       0x00001, 0x00000000, 11 | BRF_NODUMP }, // 43
	{ "pal16l8cj.ic150",       0x00001, 0x00000000, 11 | BRF_NODUMP }, // 44
	{ "pal14l4cn.ic151",       0x00001, 0x00000000, 11 | BRF_NODUMP }, // 45
	{ "pal14l4cn.ic167",       0x00001, 0x00000000, 11 | BRF_NODUMP }, // 46
	{ "pal14h4cn.ic229",       0x00001, 0x00000000, 11 | BRF_NODUMP }, // 47
	{ "pal14h4cn.ic230",       0x00001, 0x00000000, 11 | BRF_NODUMP }, // 48
};

STD_ROM_PICK(buggyboyjr)
STD_ROM_FN(buggyboyjr)

struct BurnDriver BurnDrvBuggyboyjr = {
	"buggyboyjr", "buggyboy", NULL, NULL, "1986",
	"Buggy Boy Junior/Speed Buggy (upright)\0", NULL, "Tatsumi", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, buggyboyjrRomInfo, buggyboyjrRomName, NULL, NULL, NULL, NULL, BuggyboyInputInfo, BuggyboyjrDIPInfo,
	BuggyboyjrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 240, 4, 3
};
