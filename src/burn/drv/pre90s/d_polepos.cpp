// FinalBurn Neo Namco Pole Position / Pole Position II driver module
// Based on MAME driver by Ernesto Corvi, Juergen Buchmueller, Alex Pasadyn,
// Aaron Giles, Nicola Salmoria, sound: Derrick Renaud

// dink.note: game saves hiscores in it's own nvram.  hiscore.dat hookup
// causes problems leading to no-boot situations.

#include "tiles_generic.h"
#include "z80_intf.h"
#include "z8000_intf.h"
#include "mb88xx_intf.h"
#include "burn_gun.h"
#include "stream.h"
#include "namco_snd.h"
#include "bitswap.h"
#include "burn_pal.h"
#include "burn_shift.h"
#include <math.h>

#ifndef M_PI
#define M_PI	3.14159265358979323846
#endif

#define MASTER_CLOCK		24576000
#define CPU_CLOCK			(MASTER_CLOCK / 8)		// Z80 and both Z8002: 3.072 MHz
#define LINES_PER_FRAME		264
#define CYCLES_PER_LINE		192						// 384 pixel clocks @ 6.144 MHz
#define CYCLES_PER_FRAME	(CYCLES_PER_LINE * LINES_PER_FRAME)
#define SLICES_PER_LINE		8						// Z80 / custom chip interleave
#define SLICE_CYCLES		(CYCLES_PER_LINE / SLICES_PER_LINE)
#define MCU_DIVIDER			12						// customs: 1.536 MHz / 6 = 256 kHz instruction clock
#define N06XX_TICK			64						// 06xx clock is 48 kHz = 64 Z80 cycles
#define ADC_CONV_CYCLES		(73 * 8)				// ADC0804 @ 384 kHz, 73 clocks
#define SOUND_RATE			96000					// 52xx / 54xx / engine render rate
#define ENGINE_RATE			24000					// engine sample stream rate
#define ENGINE_FILTER_RATE	48000					// the engine filters are designed for 48 kHz

enum { MCU_51XX = 0, MCU_52XX, MCU_53XX, MCU_54XX, MCU_COUNT };

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;

static UINT8 *DrvZ80ROM;
static UINT8 *DrvSub1ROM;
static UINT8 *DrvSub2ROM;
static UINT8 *DrvMCUROM[MCU_COUNT];
static UINT8 *DrvGfxChars;		// decoded 8x8 2bpp alpha
static UINT8 *DrvGfxTiles;		// decoded 8x8 2bpp background
static UINT8 *DrvGfxSprites;	// decoded 16x16 4bpp
static UINT8 *DrvGfxBigSprites;	// decoded 32x32 4bpp
static UINT8 *DrvRoadROM;
static UINT8 *DrvScaleROM;
static UINT8 *DrvColPROM;
static UINT8 *DrvWavePROM;
static UINT8 *DrvEngineROM;
static UINT8 *Drv52xxROM;
static UINT8 *DrvTempGfx;

static UINT8 *DrvNVRAM;
static UINT8 *DrvZ80RAM;		// 0x8000-0x83bf (sound memory)
static UINT8 *DrvSprRAM;		// Z8002 0x8000-0x8fff, big-endian words
static UINT8 *DrvRoadRAM;		// Z8002 0x9000-0x97ff
static UINT8 *DrvAlphaRAM;		// Z8002 0x9800-0x9fff
static UINT8 *DrvViewRAM;		// Z8002 0xa000-0xafff

static UINT32 *DrvPalette;
static UINT32 *DrvPaletteRGB;	// 128 indirect colors
static UINT16 *DrvBitmap;		// 256x256 work bitmap
static UINT16 *VertPosModifier;

static INT32 nCharMask;
static INT32 nTileMask;
static INT32 n52xxLength;
static INT32 game_has_ic25;		// Pole Position II protection chip

static UINT8 latch;				// LS259 @ 8E
static UINT16 road_vscroll;
static UINT16 bg_hscroll;
static UINT8 sub_irq_mask;

static UINT8 adc_input;
static UINT8 adc_result;
static UINT8 adc_intr;
static INT32 adc_conv_end;		// frame-relative z80 cycle, -1 = idle

static UINT8 steer_last;
static UINT8 steer_delta;
static INT16 steer_accum;
static UINT8 steer_value;

static INT16 ic25_last_result;
static INT8 ic25_last_signed;
static UINT8 ic25_last_unsigned;

static INT32 watchdog;
static INT32 nExtraCycles[3 + MCU_COUNT];	// z80, z8002 x2, 51xx/52xx/53xx/54xx
static INT32 scanline;

// Namco 06xx
static UINT8 n06xx_control;
static UINT8 n06xx_timer_state;
static UINT8 n06xx_read_stretch;
static INT32 n06xx_next;		// frame-relative z80 cycle of next toggle, -1 = stopped
static INT32 n06xx_period;
static UINT8 n06xx_nmi;

// customs
static UINT8 mcu_o_latch[MCU_COUNT];	// MB88xx O port (nibble-merged, 8-bit PLA mode)
static UINT8 n51xx_portO;
static UINT8 n51xx_rw;
static UINT8 n52xx_cmd;
static UINT16 n52xx_address;
static UINT8 n53xx_portO;
static UINT8 n54xx_cmd;

// sound
static UINT8 sound_enable;
static UINT8 dac54[3];			// 54xx outputs: 0 = O low, 1 = O high, 2 = R1
static UINT8 dac52;
static UINT8 engine_msb;
static UINT8 engine_lsb;
static UINT8 engine_enable;
static UINT32 engine_position;


struct biquad {
	double a1, a2, b0, b1, b2;
	double x1, x2, y1, y2;
};
static biquad flt54[3];			// 54xx op-amp bandpass filters
static biquad flt52_hp, flt52_lp;
static biquad flt_engine[3];
static double flt54_r1[3], flt54_rtotal[3];

static Stream stream;

static UINT8 engine_sub;		// 96 kHz samples into the current 24 kHz engine sample
static double engine_prev, engine_cur;
static double v54_table[3][16];
static double v52_table[16];

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[3];
static UINT8 DrvReset;
static UINT8 DrvInput0;
static INT16 DrvAnalogWheel;
static INT16 DrvAnalogAccel;
static INT16 DrvAnalogBrake;

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo PoleposInputList[] = {
	{"P1 Coin",				BIT_DIGITAL,	DrvJoy1 + 4,	"p1 coin"		},
	{"P2 Coin",				BIT_DIGITAL,	DrvJoy1 + 5,	"p2 coin"		},

	A("P1 Wheel",			BIT_ANALOG_REL,	&DrvAnalogWheel,	"p1 x-axis"),
	{"P1 Left",				BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"		},
	{"P1 Right",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"		},
	A("P1 Accelerator",		BIT_ANALOG_REL,	&DrvAnalogAccel,	"p1 fire 1"),
	A("P1 Brake",			BIT_ANALOG_REL,	&DrvAnalogBrake,	"p1 fire 2"),
	{"P1 Gear Shift",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 3"		},

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"			},
	{"Service",				BIT_DIGITAL,	DrvJoy1 + 6,	"service"		},
	{"Dip A",				BIT_DIPSWITCH,	DrvDips + 0,	"dip"			},
	{"Dip B",				BIT_DIPSWITCH,	DrvDips + 1,	"dip"			},
	{"Dip C",				BIT_DIPSWITCH,	DrvDips + 2,	"dip"			},
};
#undef A

STDINPUTINFO(Polepos)

#define SERVICE_DIP(ofs)												\
	{0   , 0xfe, 0   ,    2, "Service Mode"						},	\
	{ofs , 0x01, 0x80, 0x80, "Off"								},	\
	{ofs , 0x01, 0x80, 0x00, "On"								},

static struct BurnDIPInfo PoleposDIPList[]=
{
	{0x0a, 0xf0, 0xff, 0xff, NULL						},
	{0x00, 0xff, 0xff, 0xff, NULL						},
	{0x01, 0xff, 0xff, 0x6b, NULL						},
	{0x02, 0xff, 0xff, 0x80, NULL						},

	{0   , 0xfe, 0   ,    8, "Coin A"					},
	{0x00, 0x01, 0x07, 0x05, "3 Coins 1 Credit"			},
	{0x00, 0x01, 0x07, 0x03, "2 Coins 1 Credit"			},
	{0x00, 0x01, 0x07, 0x07, "1 Coin  1 Credit"			},
	{0x00, 0x01, 0x07, 0x04, "2 Coins 3 Credits"		},
	{0x00, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x00, 0x01, 0x07, 0x02, "1 Coin  3 Credits"		},
	{0x00, 0x01, 0x07, 0x00, "1 Coin  5 Credits"		},
	{0x00, 0x01, 0x07, 0x01, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"					},
	{0x00, 0x01, 0x18, 0x10, "2 Coins 1 Credit"			},
	{0x00, 0x01, 0x18, 0x18, "1 Coin  1 Credit"			},
	{0x00, 0x01, 0x18, 0x00, "2 Coins 3 Credits"		},
	{0x00, 0x01, 0x18, 0x08, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    4, "Game Time"				},
	{0x00, 0x01, 0x60, 0x60, "90 secs."					},
	{0x00, 0x01, 0x60, 0x20, "100 secs."				},
	{0x00, 0x01, 0x60, 0x40, "110 secs."				},
	{0x00, 0x01, 0x60, 0x00, "120 secs."				},

	{0   , 0xfe, 0   ,    2, "Racing Laps"				},
	{0x00, 0x01, 0x80, 0x80, "3"						},
	{0x00, 0x01, 0x80, 0x00, "4"						},

	{0   , 0xfe, 0   ,    8, "Extended Rank"			},
	{0x01, 0x01, 0x07, 0x07, "A"						},
	{0x01, 0x01, 0x07, 0x03, "B"						},
	{0x01, 0x01, 0x07, 0x05, "C"						},
	{0x01, 0x01, 0x07, 0x01, "D"						},
	{0x01, 0x01, 0x07, 0x06, "E"						},
	{0x01, 0x01, 0x07, 0x02, "F"						},
	{0x01, 0x01, 0x07, 0x04, "G"						},
	{0x01, 0x01, 0x07, 0x00, "H"						},

	{0   , 0xfe, 0   ,    8, "Practice Rank"			},
	{0x01, 0x01, 0x38, 0x38, "A"						},
	{0x01, 0x01, 0x38, 0x18, "B"						},
	{0x01, 0x01, 0x38, 0x28, "C"						},
	{0x01, 0x01, 0x38, 0x08, "D"						},
	{0x01, 0x01, 0x38, 0x30, "E"						},
	{0x01, 0x01, 0x38, 0x10, "F"						},
	{0x01, 0x01, 0x38, 0x20, "G"						},
	{0x01, 0x01, 0x38, 0x00, "H"						},

	{0   , 0xfe, 0   ,    2, "Unknown (Speed Unit?)"	},
	{0x01, 0x01, 0x40, 0x40, "Off"						},
	{0x01, 0x01, 0x40, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x01, 0x01, 0x80, 0x80, "Off"						},
	{0x01, 0x01, 0x80, 0x00, "On"						},

	SERVICE_DIP(0x02)
};

STDDIPINFO(Polepos)

static struct BurnDIPInfo PoleposaDIPList[]=
{
	{0x0a, 0xf0, 0xff, 0xff, NULL						},
	{0x00, 0xff, 0xff, 0xff, NULL						},
	{0x01, 0xff, 0xff, 0x74, NULL						},
	{0x02, 0xff, 0xff, 0x80, NULL						},

	{0   , 0xfe, 0   ,    8, "Coin A"					},
	{0x00, 0x01, 0xe0, 0xc0, "3 Coins 1 Credit"			},
	{0x00, 0x01, 0xe0, 0x20, "2 Coins 1 Credit"			},
	{0x00, 0x01, 0xe0, 0x40, "3 Coins 2 Credits"		},
	{0x00, 0x01, 0xe0, 0x80, "4 Coins 3 Credits"		},
	{0x00, 0x01, 0xe0, 0xe0, "1 Coin  1 Credit"			},
	{0x00, 0x01, 0xe0, 0x60, "1 Coin  2 Credits"		},
	{0x00, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"		},
	{0x00, 0x01, 0xe0, 0x00, "Free Play"				},

	{0   , 0xfe, 0   ,    4, "Coin B"					},
	{0x00, 0x01, 0x18, 0x08, "2 Coins 1 Credit"			},
	{0x00, 0x01, 0x18, 0x10, "3 Coins 2 Credits"		},
	{0x00, 0x01, 0x18, 0x00, "4 Coins 3 Credits"		},
	{0x00, 0x01, 0x18, 0x18, "1 Coin  1 Credit"			},

	{0   , 0xfe, 0   ,    4, "Game Time"				},
	{0x00, 0x01, 0x06, 0x06, "90 secs."					},
	{0x00, 0x01, 0x06, 0x02, "100 secs."				},
	{0x00, 0x01, 0x06, 0x04, "110 secs."				},
	{0x00, 0x01, 0x06, 0x00, "120 secs."				},

	{0   , 0xfe, 0   ,    2, "Racing Laps"				},
	{0x00, 0x01, 0x01, 0x01, "3"						},
	{0x00, 0x01, 0x01, 0x00, "4"						},

	{0   , 0xfe, 0   ,    8, "Practice Rank"			},
	{0x01, 0x01, 0xe0, 0xe0, "A"						},
	{0x01, 0x01, 0xe0, 0x60, "B"						},
	{0x01, 0x01, 0xe0, 0xa0, "C"						},
	{0x01, 0x01, 0xe0, 0x20, "D"						},
	{0x01, 0x01, 0xe0, 0xc0, "E"						},
	{0x01, 0x01, 0xe0, 0x40, "F"						},
	{0x01, 0x01, 0xe0, 0x80, "G"						},
	{0x01, 0x01, 0xe0, 0x00, "H"						},

	{0   , 0xfe, 0   ,    8, "Extended Rank"			},
	{0x01, 0x01, 0x1c, 0x1c, "A"						},
	{0x01, 0x01, 0x1c, 0x0c, "B"						},
	{0x01, 0x01, 0x1c, 0x14, "C"						},
	{0x01, 0x01, 0x1c, 0x04, "D"						},
	{0x01, 0x01, 0x1c, 0x18, "E"						},
	{0x01, 0x01, 0x1c, 0x08, "F"						},
	{0x01, 0x01, 0x1c, 0x10, "G"						},
	{0x01, 0x01, 0x1c, 0x00, "H"						},

	{0   , 0xfe, 0   ,    2, "Speed Unit"				},
	{0x01, 0x01, 0x02, 0x00, "mph"						},
	{0x01, 0x01, 0x02, 0x02, "km/h"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x01, 0x01, 0x01, 0x01, "Off"						},
	{0x01, 0x01, 0x01, 0x00, "On"						},

	SERVICE_DIP(0x02)
};

STDDIPINFO(Poleposa)

#define POLEPOS2_DIPS(speed_unit_default)					\
	{0x0a, 0xf0, 0xff, 0xff, NULL						},	\
	{0x00, 0xff, 0xff, 0xfb | (speed_unit_default), NULL},	\
	{0x01, 0xff, 0xff, 0xff, NULL						},	\
	{0x02, 0xff, 0xff, 0x80, NULL						},	\
															\
	{0   , 0xfe, 0   ,    8, "Coin A"					},	\
	{0x00, 0x01, 0xe0, 0xc0, "3 Coins 1 Credit"			},	\
	{0x00, 0x01, 0xe0, 0x20, "2 Coins 1 Credit"			},	\
	{0x00, 0x01, 0xe0, 0x40, "3 Coins 2 Credits"		},	\
	{0x00, 0x01, 0xe0, 0x80, "4 Coins 3 Credits"		},	\
	{0x00, 0x01, 0xe0, 0xe0, "1 Coin  1 Credit"			},	\
	{0x00, 0x01, 0xe0, 0x60, "1 Coin  2 Credits"		},	\
	{0x00, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"		},	\
	{0x00, 0x01, 0xe0, 0x00, "Free Play"				},	\
															\
	{0   , 0xfe, 0   ,    4, "Coin B"					},	\
	{0x00, 0x01, 0x18, 0x08, "2 Coins 1 Credit"			},	\
	{0x00, 0x01, 0x18, 0x10, "3 Coins 2 Credits"		},	\
	{0x00, 0x01, 0x18, 0x00, "4 Coins 3 Credits"		},	\
	{0x00, 0x01, 0x18, 0x18, "1 Coin  1 Credit"			},	\
															\
	{0   , 0xfe, 0   ,    2, "Speed Unit"				},	\
	{0x00, 0x01, 0x04, 0x00, "mph"						},	\
	{0x00, 0x01, 0x04, 0x04, "km/h"						},	\
															\
	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},	\
	{0x00, 0x01, 0x02, 0x00, "Off"						},	\
	{0x00, 0x01, 0x02, 0x02, "On"						},	\
															\
	{0   , 0xfe, 0   ,    2, "Freeze"					},	\
	{0x00, 0x01, 0x01, 0x01, "Off"						},	\
	{0x00, 0x01, 0x01, 0x00, "On"						},	\
															\
	{0   , 0xfe, 0   ,    2, "Game Time"				},	\
	{0x01, 0x01, 0x80, 0x80, "90 secs."					},	\
	{0x01, 0x01, 0x80, 0x00, "120 secs."				},	\
															\
	{0   , 0xfe, 0   ,    4, "Practice Rank"			},	\
	{0x01, 0x01, 0x60, 0x20, "A"						},	\
	{0x01, 0x01, 0x60, 0x60, "B"						},	\
	{0x01, 0x01, 0x60, 0x40, "C"						},	\
	{0x01, 0x01, 0x60, 0x00, "D"						},	\
															\
	{0   , 0xfe, 0   ,    4, "Extended Rank"			},	\
	{0x01, 0x01, 0x18, 0x08, "A"						},	\
	{0x01, 0x01, 0x18, 0x18, "B"						},	\
	{0x01, 0x01, 0x18, 0x10, "C"						},	\
	{0x01, 0x01, 0x18, 0x00, "D"						},	\
															\
	{0   , 0xfe, 0   ,    4, "Goal"						},	\
	{0x01, 0x01, 0x06, 0x02, "3"						},	\
	{0x01, 0x01, 0x06, 0x06, "4"						},	\
	{0x01, 0x01, 0x06, 0x04, "5"						},	\
	{0x01, 0x01, 0x06, 0x00, "6"						},	\
															\
	{0   , 0xfe, 0   ,    2, "Speed"					},	\
	{0x01, 0x01, 0x01, 0x01, "Average"					},	\
	{0x01, 0x01, 0x01, 0x00, "High"						},	\
															\
	SERVICE_DIP(0x02)

// Pole Position II (Atari / bootleg): mph by default
static struct BurnDIPInfo Polepos2DIPList[]=
{
	POLEPOS2_DIPS(0x00)
};

STDDIPINFO(Polepos2)

// Pole Position II (Japan): km/h by default
static struct BurnDIPInfo Polepos2jDIPList[]=
{
	POLEPOS2_DIPS(0x04)
};

STDDIPINFO(Polepos2j)

// shared 16-bit RAM is stored big-endian, the Z80 sees the low byte of each word
#define WORD16(ram, offs)	((UINT16)(((ram)[(offs) * 2] << 8) | (ram)[(offs) * 2 + 1]))
#define LOW8(ram, offs)		((ram)[(offs) * 2 + 1])

static inline INT32 z80_now()
{
	return ZetTotalCycles(0);
}

static void sync_mcus(INT32 z80_cycle)
{
	INT32 target = z80_cycle / MCU_DIVIDER;

	for (INT32 i = 0; i < MCU_COUNT; i++)
	{
		mb88xxOpen(i);
		INT32 todo = target - mb88xxTotalCycles();
		if (todo > 0) mb88xxRun(todo);
		mb88xxClose();
	}
}

static void mcu_set_irq(INT32 chip, INT32 state)
{
	mb88xxOpen(chip);
	mb88xxSetIRQLine(0, state ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
	mb88xxClose();
}

static UINT8 mcu_merge_o(INT32 chip, UINT8 data)
{
	INT32 shift = (data & 0x10) ? 4 : 0;
	mcu_o_latch[chip] = (mcu_o_latch[chip] & ~(0x0f << shift)) | ((data & 0x0f) << shift);
	return shift;
}

static void sound_sync();

// 51xx - coins, credits and the IN0/DSWB switches
static UINT8 n51xx_K_r()  { return (n51xx_rw << 3) | (n51xx_portO & 0x07); }
static UINT8 n51xx_R0_r() { return DrvDips[1] & 0x0f; }
static UINT8 n51xx_R1_r() { return DrvDips[1] >> 4; }
static UINT8 n51xx_R2_r() { return DrvInput0 & 0x0f; }
static UINT8 n51xx_R3_r() { return DrvInput0 >> 4; }
static void n51xx_O_w(UINT8 data) { mcu_merge_o(MCU_51XX, data); n51xx_portO = mcu_o_latch[MCU_51XX]; }
static void n51xx_P_w(UINT8) { }	// coin counters

// 52xx - sample player
static UINT8 n52xx_K_r()  { return n52xx_cmd & 0x0f; }
static UINT8 n52xx_rom_r()
{
	return (n52xx_address < n52xxLength) ? Drv52xxROM[n52xx_address] : 0xff;
}
static UINT8 n52xx_R0_r() { return n52xx_rom_r() & 0x0f; }
static UINT8 n52xx_R1_r() { return n52xx_rom_r() >> 4; }
static void n52xx_R2_w(UINT8 data) { n52xx_address = (n52xx_address & 0xfff0) | ((data & 0x0f) << 0); }
static void n52xx_R3_w(UINT8 data) { n52xx_address = (n52xx_address & 0xff0f) | ((data & 0x0f) << 4); }
static void n52xx_O_w(UINT8 data)
{
	mcu_merge_o(MCU_52XX, data);
	n52xx_address = (n52xx_address & 0x00ff) | (mcu_o_latch[MCU_52XX] << 8);
}
static void n52xx_P_w(UINT8 data)
{
	sound_sync();
	dac52 = data & 0x0f;
}
static INT32 n52xx_SI_r() { return 1; }	// pulled to +5V

// 53xx - steering wheel and DSWA
static void steering_update()
{
	steer_accum += (INT8)(steer_value - steer_last) * 2;
	steer_last = steer_value;

	if (steer_accum < 0) {
		steer_delta = 0;
		steer_accum++;
	} else if (steer_accum > 0) {
		steer_delta = 1;
		steer_accum--;
	}
}

static UINT8 n53xx_K_r()  { return 0; }		// hardwired to 0
static UINT8 n53xx_R0_r() { steering_update(); return steer_accum & 1; }
static UINT8 n53xx_R1_r() { return steer_delta; }
static UINT8 n53xx_R2_r() { return DrvDips[0] & 0x0f; }
static UINT8 n53xx_R3_r() { return DrvDips[0] >> 4; }
static void n53xx_O_w(UINT8 data) { mcu_merge_o(MCU_53XX, data); n53xx_portO = mcu_o_latch[MCU_53XX]; }

// 54xx - explosion / skid noise
static UINT8 n54xx_K_r()  { return n54xx_cmd >> 4; }
static UINT8 n54xx_R0_r() { return n54xx_cmd & 0x0f; }
static void n54xx_O_w(UINT8 data)
{
	sound_sync();
	INT32 shift = mcu_merge_o(MCU_54XX, data);
	dac54[shift ? 1 : 0] = data & 0x0f;
}
static void n54xx_R1_w(UINT8 data)
{
	sound_sync();
	dac54[2] = data & 0x0f;
}

static void mcu_init()
{
	static const INT32 types[MCU_COUNT] = { 8843, 8843, 8843, 8844 };

	for (INT32 i = 0; i < MCU_COUNT; i++) {
		mb88xxInit(i, types[i], DrvMCUROM[i]);
		mb88xxOpen(i);
		switch (i)
		{
			case MCU_51XX:
				mb88xxSetReadWriteFunction(NULL, n51xx_K_r, 'k', 0);
				mb88xxSetReadWriteFunction(NULL, n51xx_R0_r, 'r', 0);
				mb88xxSetReadWriteFunction(NULL, n51xx_R1_r, 'r', 1);
				mb88xxSetReadWriteFunction(NULL, n51xx_R2_r, 'r', 2);
				mb88xxSetReadWriteFunction(NULL, n51xx_R3_r, 'r', 3);
				mb88xxSetReadWriteFunction(n51xx_O_w, NULL, 'o', 0);
				mb88xxSetReadWriteFunction(n51xx_P_w, NULL, 'p', 0);
			break;

			case MCU_52XX:
				mb88xxSetReadWriteFunction(NULL, n52xx_K_r, 'k', 0);
				mb88xxSetReadWriteFunction(n52xx_O_w, NULL, 'o', 0);
				mb88xxSetReadWriteFunction(n52xx_P_w, NULL, 'p', 0);
				mb88xxSetReadWriteFunction(NULL, n52xx_R0_r, 'r', 0);
				mb88xxSetReadWriteFunction(NULL, n52xx_R1_r, 'r', 1);
				mb88xxSetReadWriteFunction(n52xx_R2_w, NULL, 'r', 2);
				mb88xxSetReadWriteFunction(n52xx_R3_w, NULL, 'r', 3);
				mb88xxSetSerialReadWriteFunction(NULL, n52xx_SI_r);
			break;

			case MCU_53XX:
				mb88xxSetReadWriteFunction(NULL, n53xx_K_r, 'k', 0);
				mb88xxSetReadWriteFunction(NULL, n53xx_R0_r, 'r', 0);
				mb88xxSetReadWriteFunction(NULL, n53xx_R1_r, 'r', 1);
				mb88xxSetReadWriteFunction(NULL, n53xx_R2_r, 'r', 2);
				mb88xxSetReadWriteFunction(NULL, n53xx_R3_r, 'r', 3);
				mb88xxSetReadWriteFunction(n53xx_O_w, NULL, 'o', 0);
			break;

			case MCU_54XX:
				mb88xxSetReadWriteFunction(NULL, n54xx_K_r, 'k', 0);
				mb88xxSetReadWriteFunction(n54xx_O_w, NULL, 'o', 0);
				mb88xxSetReadWriteFunction(NULL, n54xx_R0_r, 'r', 0);
				mb88xxSetReadWriteFunction(n54xx_R1_w, NULL, 'r', 1);
			break;
		}
		mb88xxClose();
	}
}

// IOSEL: the reset line of all four customs is active low
static void mcu_set_reset(INT32 iosel)
{
	for (INT32 i = 0; i < MCU_COUNT; i++) {
		mb88xxOpen(i);
		mb88xxSetRESETLine(iosel ? 0 : 1);
		mb88xxClose();
	}
}

static void n06xx_set_nmi(INT32 state)
{
	if (n06xx_nmi != state) {
		n06xx_nmi = state;
		ZetSetIRQLine(0, CPU_IRQLINE_NMI, state ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
	}
}

static void n06xx_toggle()
{
	n06xx_timer_state ^= 1;

	if (n06xx_timer_state) {
		n51xx_rw = (n06xx_control >> 4) & 1;	// only the 51xx has an R/W input
	}

	n06xx_set_nmi(n06xx_timer_state && !n06xx_read_stretch);
	n06xx_read_stretch = 0;

	for (INT32 i = 0; i < 4; i++) {
		static const INT32 chip[4] = { MCU_51XX, MCU_53XX, MCU_52XX, MCU_54XX };
		mcu_set_irq(chip[i], ((n06xx_control >> i) & 1) && n06xx_timer_state);
	}
}

static void n06xx_update(INT32 z80_cycle)
{
	while (n06xx_next >= 0 && n06xx_next <= z80_cycle)
	{
		sync_mcus(n06xx_next);
		n06xx_toggle();
		n06xx_next += n06xx_period;
	}
}

static UINT8 n06xx_data_r()
{
	sync_mcus(z80_now());

	if (~n06xx_control & 0x10) return 0;	// read in write mode

	UINT8 result = 0xff;
	if (n06xx_control & 0x01) result &= n51xx_portO;
	if (n06xx_control & 0x02) result &= n53xx_portO;
	return result;
}

static void n06xx_data_w(UINT8 data)
{
	sync_mcus(z80_now());

	if (n06xx_control & 0x10) return;		// write in read mode

	if (n06xx_control & 0x01) n51xx_portO = data;
	if (n06xx_control & 0x04) n52xx_cmd = data;
	if (n06xx_control & 0x08) n54xx_cmd = data;
}

static void n06xx_ctrl_w(UINT8 data)
{
	INT32 now = z80_now();
	sync_mcus(now);

	n06xx_control = data;

	if ((n06xx_control & 0xe0) == 0)
	{
		n06xx_next = -1;
		n06xx_timer_state = 0;
		n06xx_set_nmi(0);
		for (INT32 i = 0; i < MCU_COUNT; i++) mcu_set_irq(i, 0);
	}
	else
	{
		if (n06xx_control & 0x10) {
			n06xx_set_nmi(0);
			n06xx_read_stretch = 1;
		} else {
			n06xx_read_stretch = 0;
		}

		INT32 divisor = 1 << ((n06xx_control & 0xe0) >> 5);
		n06xx_period = (N06XX_TICK / 2) * divisor;			// half period of clock / divisor
		n06xx_next = ((now / N06XX_TICK) + 1) * N06XX_TICK;	// first edge on the next 06xx clock
	}
}

static void engine_lsb_w(UINT8 data);
static void engine_msb_w(UINT8 data);

static void latch_w(INT32 bit, INT32 state)
{
	UINT8 old = latch;
	latch = (latch & ~(1 << bit)) | (state << bit);
	if (old == latch) return;

	switch (bit)
	{
		case 0:
			if (!state) ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
		break;

		case 1:
			sync_mcus(z80_now());
			mcu_set_reset(state);
		break;

		case 2:
			sound_sync();
			sound_enable = state;
			namco_15xx_sound_enable(state);
			if (!state) {
				engine_lsb_w(0);
				engine_msb_w(0);
			}
		break;

		case 3:
			adc_input = state;
		break;

		case 4:
		case 5:
			Z8002Open(bit - 4);
			Z8002SetRESETLine(state ? 0 : 1);
			Z8002Close();
		break;

		case 6:
		break;

		case 7:
		break;
	}
}

static UINT8 analog_read()
{
	if (adc_input & 1) {
		return scalerange(ProcessAnalog(DrvAnalogAccel, 0, INPUT_DEADZONE | INPUT_LINEAR | INPUT_MIGHTBEDIGITAL, 0x00, 0xff), 0, 0xff, 0, 0x90);
	}
	return scalerange(ProcessAnalog(DrvAnalogBrake, 0, INPUT_DEADZONE | INPUT_LINEAR | INPUT_MIGHTBEDIGITAL, 0x00, 0xff), 0, 0xff, 0, 0x90);
}

static void adc_update(INT32 now)
{
	if (adc_conv_end >= 0 && now >= adc_conv_end) {
		adc_result = analog_read();
		adc_intr = 1;
		adc_conv_end = -1;
	}
}

static UINT8 __fastcall polepos_z80_in(UINT16 port)
{
	if ((port & 0xff) == 0x00) {
		adc_update(z80_now());
		adc_intr = 0;
		return adc_result;
	}
	return 0;
}

static void __fastcall polepos_z80_out(UINT16 port, UINT8)
{
	if ((port & 0xff) == 0x00) {
		adc_update(z80_now());
		adc_intr = 0;
		if (adc_conv_end < 0) adc_conv_end = z80_now() + ADC_CONV_CYCLES;
	}
}

static UINT8 __fastcall polepos_z80_read(UINT16 address)
{
	if (address >= 0x4000 && address <= 0x47ff) return LOW8(DrvSprRAM, address & 0x7ff);
	if (address >= 0x4800 && address <= 0x4bff) return LOW8(DrvRoadRAM, address & 0x3ff);
	if (address >= 0x4c00 && address <= 0x4fff) return LOW8(DrvAlphaRAM, address & 0x3ff);
	if (address >= 0x5000 && address <= 0x57ff) return LOW8(DrvViewRAM, address & 0x7ff);

	if ((address & 0xf000) == 0x8000) {	// page 0x83 of each mirror
		if ((address & 0xff) < 0xc0) return DrvZ80RAM[0x300 | (address & 0xff)];
		return namco_polepos_read(address & 0x3f);
	}

	if ((address & 0xf000) == 0x9000) {
		if (address & 0x100) return n06xx_control;
		return n06xx_data_r();
	}

	if ((address & 0xf000) == 0xa000) {
		if ((address & 0x300) == 0x000) {	// READY
			INT32 now = z80_now();
			adc_update(now);
			UINT8 ret = 0xff;
			if ((now / CYCLES_PER_LINE) >= 128) ret ^= 0x02;	// 128V
			if (adc_intr) ret ^= 0x08;							// ADC end flag
			return ret;
		}
		return 0;
	}

	return 0;
}

static void __fastcall polepos_z80_write(UINT16 address, UINT8 data)
{
	if (address >= 0x4000 && address <= 0x47ff) { LOW8(DrvSprRAM, address & 0x7ff) = data; return; }
	if (address >= 0x4800 && address <= 0x4bff) { LOW8(DrvRoadRAM, address & 0x3ff) = data; return; }
	if (address >= 0x4c00 && address <= 0x4fff) { LOW8(DrvAlphaRAM, address & 0x3ff) = data; return; }
	if (address >= 0x5000 && address <= 0x57ff) { LOW8(DrvViewRAM, address & 0x7ff) = data; return; }

	if ((address & 0xf000) == 0x8000) {
		if ((address & 0xff) < 0xc0) {
			DrvZ80RAM[0x300 | (address & 0xff)] = data;
		} else {
			namco_polepos_write(address & 0x3f, data);
		}
		return;
	}

	if ((address & 0xf000) == 0x9000) {
		if (address & 0x100) {
			n06xx_ctrl_w(data);
		} else {
			n06xx_data_w(data);
		}
		return;
	}

	if ((address & 0xf000) == 0xa000) {
		switch (address & 0x300)
		{
			case 0x000: latch_w(address & 7, data & 1); return;
			case 0x100: watchdog = 0; return;
			case 0x200: engine_lsb_w(data); return;
			case 0x300: engine_msb_w(data); return;
		}
	}
}

static UINT16 polepos2_ic25_r(UINT32 address)
{
	INT32 offset = ((address - 0x4000) >> 1) & 0x1ff;
	INT32 result;

	if (offset < 0x100)
	{
		ic25_last_signed = offset & 0xff;
		result = ic25_last_result & 0xff;
	}
	else
	{
		ic25_last_unsigned = offset & 0xff;
		result = (ic25_last_result >> 8) & 0xff;
		ic25_last_result = (INT8)ic25_last_signed * (UINT8)ic25_last_unsigned;
	}

	return result | (result << 8);
}

static UINT16 z8002_read_word(UINT32 address)
{
	if (game_has_ic25 && Z8002GetActive() == 0 && address >= 0x4000 && address <= 0x5fff) {
		return polepos2_ic25_r(address);
	}
	return 0;
}

static UINT8 z8002_read_byte(UINT32 address)
{
	UINT16 w = z8002_read_word(address & ~1);
	return (address & 1) ? (w & 0xff) : (w >> 8);
}

static void z8002_nvi_enable_w(UINT16 data)
{
	INT32 which = Z8002GetActive();
	data &= 1;

	sub_irq_mask = data;
	if (!data) Z8002SetIRQLine(Z8002_INPUT_LINE_NVI, CPU_IRQSTATUS_NONE);
	(void)which;
}

static void z8002_write(UINT32 address, UINT16 data, UINT16 mask)
{
	if (address >= 0x6000 && address <= 0x7fff) {
		z8002_nvi_enable_w(data);
		return;
	}

	if (address >= 0xc000) {
		if ((address & 0x0100) == 0x0000) {
			bg_hscroll = (bg_hscroll & ~mask) | (data & mask);
		} else {
			road_vscroll = (road_vscroll & ~mask) | (data & mask);
		}
	}
}

static void z8002_write_word(UINT32 address, UINT16 data)
{
	z8002_write(address, data, 0xffff);
}

static void z8002_write_byte(UINT32 address, UINT8 data)
{
	UINT16 data16 = data | (data << 8);
	z8002_write(address & ~1, data16, (address & 1) ? 0x00ff : 0xff00);
}

// sound
static INT32 sound_sync_cycles()
{
	if (mb88xxGetActive() != -1) return mb88xxTotalCycles() * MCU_DIVIDER;
	return ZetTotalCycles(0);
}

static void sound_sync()
{
	stream.update();
}

static void engine_lsb_w(UINT8 data)
{
	sound_sync();
	engine_lsb = data & 62;
	engine_enable = data & 1;
}

static void engine_msb_w(UINT8 data)
{
	sound_sync();
	engine_msb = data & 63;
}

#define RES_K(r)	((r) * 1e3)
#define CAP_U(c)	((c) * 1e-6)

#define FILTER_LOWPASS	0
#define FILTER_HIGHPASS	1
#define FILTER_BANDPASS	2

static void filter2_setup(biquad *f, INT32 type, double fc, double d, double gain, double rate = SOUND_RATE)
{
	double two_over_T = 2.0 * rate;
	double two_over_T_squared = two_over_T * two_over_T;
	double wc = rate * 2.0 * tan(M_PI * fc / rate);
	double wc_squared = wc * wc;
	double den = two_over_T_squared + d * wc * two_over_T + wc_squared;

	f->a1 = 2.0 * (-two_over_T_squared + wc_squared) / den;
	f->a2 = (two_over_T_squared - d * wc * two_over_T + wc_squared) / den;

	switch (type)
	{
		case FILTER_LOWPASS:
			f->b0 = f->b2 = wc_squared / den;
			f->b1 = 2.0 * f->b0;
		break;
		case FILTER_BANDPASS:
			f->b0 = d * wc * two_over_T / den;
			f->b1 = 0.0;
			f->b2 = -f->b0;
		break;
		case FILTER_HIGHPASS:
			f->b0 = f->b2 = two_over_T_squared / den;
			f->b1 = -2.0 * f->b0;
		break;
	}

	f->b0 *= gain;
	f->b1 *= gain;
	f->b2 *= gain;
	f->x1 = f->x2 = f->y1 = f->y2 = 0;
}

static inline double filter2_step(biquad *f, double x0)
{
	double y0 = -f->a1 * f->y1 - f->a2 * f->y2 + f->b0 * x0 + f->b1 * f->x1 + f->b2 * f->x2;
	f->x2 = f->x1;
	f->x1 = x0;
	f->y2 = f->y1;
	f->y1 = y0;
	return y0;
}

// op-amp band-pass filter, single amp with feedback (r2 not used)
static void opamp_bandpass_1m_setup(INT32 n, double r1, double r3, double rF, double c1, double c2)
{
	double rTotal = 1.0 / (1.0 / r1 + 1.0 / r3);
	double fc = 1.0 / (2 * M_PI * sqrt(rTotal * rF * c1 * c2));
	double d = (c1 + c2) / sqrt(rF / rTotal * c1 * c2);
	double gain = -rF / rTotal * c2 / (c1 + c2);

	filter2_setup(&flt54[n], FILTER_BANDPASS, fc, d, gain);
	flt54_r1[n] = r1;
	flt54_rtotal[n] = rTotal;
}

static void engine_bandpass_setup(biquad *f, double r1, double r2, double r3, double c1, double c2)
{
	double gain, r_in;

	if (r2 == 0) {
		gain = 1;
		r_in = r1;
	} else {
		gain = r2 / (r1 + r2);
		r_in = 1.0 / (1.0 / r1 + 1.0 / r2);
	}

	double fc = 1.0 / (2 * M_PI * sqrt(r_in * r3 * c1 * c2));
	double d = (c1 + c2) / sqrt(r3 / r_in * c1 * c2);
	gain *= -r3 / r_in * c2 / (c1 + c2);

	filter2_setup(f, FILTER_BANDPASS, fc, d, gain, ENGINE_FILTER_RATE);
}

#define POLEPOS_VREF			(5.0 * (RES_K(1) / (RES_K(1.5) + RES_K(1))))
#define OP_AMP_VP_RAIL_OFFSET	1.5

static double dac_r1(UINT8 data, const double *r, double v)
{
	double g = 0, gsum = 0;
	for (INT32 i = 0; i < 4; i++) {
		gsum += 1.0 / r[i];
		if (data & (1 << i)) g += 1.0 / r[i];
	}
	return v * g / gsum;
}

static const double dac54_r[4] = { RES_K(47), RES_K(22), RES_K(10), RES_K(4.7) };
static const double dac52_r[4] = { RES_K(100), RES_K(47), RES_K(22), RES_K(10) };

// engine volumes
#define POLEPOS_R166		1000.0
#define POLEPOS_R167		2200.0
#define POLEPOS_R168		4700.0
#define POLEPOS_R166_SHUNT	(1.0 / (1.0 / POLEPOS_R166 + 1.0 / 250))
#define POLEPOS_R167_SHUNT	(1.0 / (1.0 / POLEPOS_R166 + 1.0 / 250))
#define POLEPOS_R168_SHUNT	(1.0 / (1.0 / POLEPOS_R166 + 1.0 / 250))

static const double engine_volume_table[8] =
{
	(POLEPOS_R168_SHUNT + POLEPOS_R167_SHUNT + POLEPOS_R166_SHUNT + 2200) / 10000,
	(POLEPOS_R168_SHUNT + POLEPOS_R167_SHUNT + POLEPOS_R166       + 2200) / 10000,
	(POLEPOS_R168_SHUNT + POLEPOS_R167       + POLEPOS_R166_SHUNT + 2200) / 10000,
	(POLEPOS_R168_SHUNT + POLEPOS_R167       + POLEPOS_R166       + 2200) / 10000,
	(POLEPOS_R168       + POLEPOS_R167_SHUNT + POLEPOS_R166_SHUNT + 2200) / 10000,
	(POLEPOS_R168       + POLEPOS_R167_SHUNT + POLEPOS_R166       + 2200) / 10000,
	(POLEPOS_R168       + POLEPOS_R167       + POLEPOS_R166_SHUNT + 2200) / 10000,
	(POLEPOS_R168       + POLEPOS_R167       + POLEPOS_R166       + 2200) / 10000
};

static const double engine_r_filt_out[3] = { RES_K(4.7), RES_K(7.5), RES_K(10) };
#define ENGINE_R_FILT_TOTAL	(1.0 / (1.0 / RES_K(4.7) + 1.0 / RES_K(7.5) + 1.0 / RES_K(10)))

static void sound_init()
{
	double dac54_rpar = 1.0 / (1.0 / RES_K(47) + 1.0 / RES_K(22) + 1.0 / RES_K(10) + 1.0 / RES_K(4.7));

	// 54xx channels CHANL1 (R1 port), CHANL2 (O high), CHANL3 (O low)
	opamp_bandpass_1m_setup(0, dac54_rpar + RES_K(22), RES_K(12), RES_K(120), CAP_U(0.0022), CAP_U(0.0022));
	opamp_bandpass_1m_setup(1, dac54_rpar + RES_K(15), RES_K(15), RES_K(120), CAP_U(0.022),  CAP_U(0.022));
	opamp_bandpass_1m_setup(2, dac54_rpar + RES_K(22), RES_K(22), RES_K(180), CAP_U(0.047),  CAP_U(0.047));

	// 52xx: SPICE derived equivalent filter
	filter2_setup(&flt52_hp, FILTER_HIGHPASS, 100,  1.0 / 0.3, 1.0);
	filter2_setup(&flt52_lp, FILTER_LOWPASS,  1200, 1.0 / 0.8, 1.0);

	// engine
	engine_bandpass_setup(&flt_engine[0], RES_K(220), RES_K(33), RES_K(390), CAP_U(.01),   CAP_U(.01));
	engine_bandpass_setup(&flt_engine[1], RES_K(150), RES_K(22), RES_K(330), CAP_U(.0047), CAP_U(.0047));
	filter2_setup(&flt_engine[2], FILTER_HIGHPASS, 950, 1.0 / .707, 1, ENGINE_FILTER_RATE);

	// the engine filters are designed for 48 kHz but stepped at the engine
	// stream's 24 kHz, so their corners land at half the design frequency

	// 54xx / 52xx DAC voltages, as filter inputs
	for (INT32 v = 0; v < 16; v++)
	{
		double v54 = dac_r1(v, dac54_r, 4.0);
		for (INT32 k = 0; k < 3; k++) {
			v54_table[k][v] = (v54 - POLEPOS_VREF) / flt54_r1[k] * flt54_rtotal[k];
		}
		v52_table[v] = dac_r1(v, dac52_r, 4.0) - POLEPOS_VREF;
	}
}

static void sound_reset_filters()
{
	for (INT32 i = 0; i < 3; i++) {
		flt54[i].x1 = flt54[i].x2 = flt54[i].y1 = flt54[i].y2 = 0;
		flt_engine[i].x1 = flt_engine[i].x2 = flt_engine[i].y1 = flt_engine[i].y2 = 0;
	}
	flt52_hp.x1 = flt52_hp.x2 = flt52_hp.y1 = flt52_hp.y2 = 0;
	flt52_lp.x1 = flt52_lp.x2 = flt52_lp.y1 = flt52_lp.y2 = 0;

	engine_sub = 0;
	engine_prev = engine_cur = 0;
}

// one 24 kHz engine sample
static double engine_sample()
{
	if (!engine_enable) return 0;	// the stream (and its filters) stay idle

	UINT32 eclock = (CPU_CLOCK / 16) * ((engine_msb + 1) * 64 + engine_lsb + 1) / (64 * 64);
	UINT32 estep = (eclock << 12) / ENGINE_RATE;
	INT32 eslot = (engine_msb >> 3) & 7;
	const UINT8 *ebase = DrvEngineROM + eslot * 0x800;

	double x0 = (3.4 / 255 * ebase[(engine_position >> 12) & 0x7ff] - 2) * engine_volume_table[eslot];
	double i_total = 0;
	for (INT32 k = 0; k < 3; k++) {
		double y0 = filter2_step(&flt_engine[k], x0);
		// the op-amps clip at 0V / 3.5V, i.e. -2V / +1.5V around vRef
		if (y0 > 1.5) y0 = 1.5;
		if (y0 < -2) y0 = -2;
		i_total += y0 / engine_r_filt_out[k];
	}
	engine_position += estep;

	return i_total * ENGINE_R_FILT_TOTAL / 2;
}

static void polepos_sound_render(INT16 **streams, INT32 samples)
{
	INT16 *outl = streams[0];
	INT16 *outr = streams[1];

	// the DACs hold their level for this whole block (stream.update() runs before every write)
	double a54[3];
	for (INT32 k = 0; k < 3; k++) a54[k] = v54_table[k][dac54[2 - k]];
	double a52 = v52_table[dac52];

	for (INT32 i = 0; i < samples; i++)
	{
		// 54xx (CHANL1 <- R1, CHANL2 <- O high, CHANL3 <- O low) and 52xx discrete outputs
		double disc = 0;
		for (INT32 k = 0; k < 3; k++)
		{
			biquad *f = &flt54[k];
			double v_out = -f->a1 * f->y1 - f->a2 * f->y2 + f->b0 * a54[k] + f->b1 * f->x1 + f->b2 * f->x2 + POLEPOS_VREF;
			f->x2 = f->x1;
			f->x1 = a54[k];
			f->y2 = f->y1;
			if (v_out > 5.0 - OP_AMP_VP_RAIL_OFFSET) v_out = 5.0 - OP_AMP_VP_RAIL_OFFSET;
			if (v_out < 0.0) v_out = 0.0;
			f->y1 = v_out - POLEPOS_VREF;
			disc += v_out - POLEPOS_VREF;
		}
		{
			double v = filter2_step(&flt52_hp, a52);
			v = filter2_step(&flt52_lp, v) * 0.5;
			if (v < 0) v = 0;
			if (v > 5.0 - OP_AMP_VP_RAIL_OFFSET - POLEPOS_VREF) v = 5.0 - OP_AMP_VP_RAIL_OFFSET - POLEPOS_VREF;
			disc += v;
		}
		disc *= (32767.0 / 2) / 32768.0 * 0.90;

		// engine: 24 kHz, linearly interpolated up to the render rate
		if (engine_sub == 0) {
			engine_prev = engine_cur;
			engine_cur = engine_sample();
		}
		double eng = (engine_prev + (engine_cur - engine_prev) * engine_sub / (SOUND_RATE / ENGINE_RATE)) * 0.90 * 0.77;
		engine_sub = (engine_sub + 1) % (SOUND_RATE / ENGINE_RATE);

		INT32 out = BURN_SND_CLIP((INT32)((disc + eng) * 32767.0));
		outl[i] = out;
		outr[i] = out;
	}
}

static void DrvPaletteInit()
{
	const UINT8 *prom = DrvColPROM;

	// 128 indirect colors: 4-bit DACs, 2.2k / 1k / 470 / 220 ohm
	for (INT32 i = 0; i < 128; i++)
	{
		INT32 c[3];
		for (INT32 k = 0; k < 3; k++) {
			UINT8 d = prom[k * 0x100 + i];
			c[k] = 0x0e * BIT(d, 0) + 0x1f * BIT(d, 1) + 0x43 * BIT(d, 2) + 0x8f * BIT(d, 3);
		}
		DrvPaletteRGB[i] = (c[0] << 16) | (c[1] << 8) | c[2];
	}

	// 136014-142/143/144: vertical position modifiers for the road
	for (INT32 i = 0; i < 256; i++) {
		VertPosModifier[i] = prom[0x500 + i] + (prom[0x600 + i] << 4) + (prom[0x700 + i] << 8);
	}
}

static inline UINT32 indirect_pen(INT32 pen)
{
	const UINT8 *prom = DrvColPROM;
	INT32 color;

	if (pen < 0x200) {			// alpha (0x100+ = 128V half)
		color = prom[0x300 + (pen & 0xff)];
		color = (color != 15) ? (((pen & 0x100) ? 0x60 : 0x20) + color) : 0x2f;
	} else if (pen < 0x300) {	// background
		color = prom[0x400 + (pen & 0xff)];
	} else if (pen < 0xb00) {	// sprites (0x700+ = 128V half)
		INT32 i = (pen - 0x300) & 0x3ff;
		color = prom[0xc00 + i];
		color = (color != 15) ? (((pen >= 0x700) ? 0x50 : 0x10) + color) : 0x1f;
	} else {					// road
		color = 0x40 + prom[0x800 + ((pen - 0xb00) & 0x3ff)];
	}

	return DrvPaletteRGB[color & 0x7f];
}

static void DrvRecalcPalette()
{
	for (INT32 i = 0; i < 0x0f00; i++) {
		UINT32 rgb = indirect_pen(i);
		DrvPalette[i] = BurnHighCol((rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff, 0);
	}
}

static void draw_background()
{
	// 64x16 tiles, column major, top half of the screen only
	for (INT32 y = 0; y < 128; y++)
	{
		UINT16 *dst = DrvBitmap + y * 256;
		INT32 row = y >> 3;

		for (INT32 x = 0; x < 256; x++)
		{
			INT32 sx = (x + bg_hscroll) & 0x1ff;
			INT32 tile_index = (sx >> 3) * 16 + row;
			UINT16 word = WORD16(DrvViewRAM, tile_index);
			INT32 code = ((word & 0xff) | ((word & 0x4000) >> 6)) & nTileMask;
			INT32 color = (word & 0x3f00) >> 8;
			UINT8 pix = DrvGfxTiles[(code << 6) + ((y & 7) << 3) + (sx & 7)];
			dst[x] = 0x200 + (color << 2) + pix;
		}
	}
}

static void draw_road()
{
	const UINT8 *road_control = DrvRoadROM;
	const UINT8 *road_bits1 = DrvRoadROM + 0x2000;
	const UINT8 *road_bits2 = DrvRoadROM + 0x4000;

	for (INT32 y = 128; y < 256; y++)
	{
		UINT16 scanline_buf[256 + 8];
		UINT16 *dest = scanline_buf;

		// vertical position modifier plus the vertical scroll selects the road memory entry
		INT32 yoffs = ((VertPosModifier[y] + road_vscroll) >> 3) & 0x1ff;
		INT32 roadpal = WORD16(DrvRoadRAM, yoffs) & 15;
		INT32 pen_base = 0x0b00 + (roadpal << 6);

		INT32 xoffs = WORD16(DrvRoadRAM, 0x380 + (y & 0x7f)) & 0x3ff;
		INT32 xscroll = xoffs & 7;
		xoffs &= ~7;

		for (INT32 x = 0; x < 256 / 8 + 1; x++, xoffs += 8)
		{
			if (xoffs & 0x200)
			{
				// road roms disabled
				for (INT32 i = 0; i < 8; i++) *dest++ = pen_base | 0;
			}
			else
			{
				INT32 romoffs = ((y & 0x07f) << 6) + ((xoffs & 0x1f8) >> 3);
				INT32 control = road_control[romoffs];
				INT32 bits1 = road_bits1[romoffs];
				INT32 bits2 = road_bits2[(romoffs & 0xfff) | ((romoffs & 0x1000) >> 1)];

				INT32 roadval = control & 0x3f;
				INT32 carin = control >> 7;

				for (INT32 i = 8; i > 0; i--)
				{
					INT32 bits = BIT(bits1, i) + (BIT(bits2, i) << 1);
					if (!carin && bits) bits++;
					*dest++ = pen_base | (roadval & 0x3f);
					roadval += bits;
				}
			}
		}

		memcpy(DrvBitmap + y * 256, scanline_buf + xscroll, 256 * sizeof(UINT16));
	}
}

static void zoom_sprite(INT32 big, UINT32 code, UINT32 color, INT32 flipx, INT32 sx, INT32 sy, INT32 sizex, INT32 sizey)
{
	const INT32 width = big ? 32 : 16;
	const UINT8 *gfxdata = big ? (DrvGfxBigSprites + (code & 0x7f) * 32 * 32) : (DrvGfxSprites + (code & 0x7f) * 16 * 16);
	const UINT8 *colprom = DrvColPROM + 0xc00 + (color & 0x3f) * 16;
	const INT32 coloroffs = 0x300 + color * 16;
	const INT32 offsxor = flipx ? (big ? 0x1f : 0x0f) : 0;

	for (INT32 y = 0; y <= sizey; y++)
	{
		INT32 yy = (sy + y) & 0x1ff;

		if (yy >= 0x10 && yy < 0xf0)
		{
			INT32 dy = DrvScaleROM[(y << 6) + sizey] & 0x1f;
			INT32 xx = sx & 0x3ff;
			INT32 siz = 0;
			INT32 offs = 0;

			if (!big) dy >>= 1;
			const UINT8 *src = gfxdata + dy * width;

			for (INT32 x = (big ? 0x40 : 0x20); x > 0; x--)
			{
				if (xx < 0x100)
				{
					INT32 pen = src[(offs / 2) ^ offsxor];

					if (colprom[pen] != 15) {
						DrvBitmap[yy * 256 + xx] = pen + coloroffs;
					}
				}
				offs++;

				siz = siz + 1 + sizex;
				if (siz & 0x40)
				{
					siz &= 0x3f;
					xx = (xx + 1) & 0x3ff;
				}
			}
		}
	}
}

static void draw_sprites()
{
	for (INT32 i = 0; i < 64; i++)
	{
		UINT16 pos0 = WORD16(DrvSprRAM, 0x380 + i * 2 + 0);
		UINT16 pos1 = WORD16(DrvSprRAM, 0x380 + i * 2 + 1);
		UINT16 siz0 = WORD16(DrvSprRAM, 0x780 + i * 2 + 0);
		UINT16 siz1 = WORD16(DrvSprRAM, 0x780 + i * 2 + 1);

		INT32 sx = (pos1 & 0x3ff) - 0x40 + 4;
		INT32 sy = 512 - (pos0 & 0x1ff) + 1;	// sprites are buffered and delayed by one scanline
		INT32 sizex = (siz1 & 0x3f00) >> 8;
		INT32 sizey = (siz0 & 0x3f00) >> 8;
		INT32 code = siz0 & 0x7f;
		INT32 flipx = BIT(siz0, 7);
		INT32 color = siz1 & 0x3f;

		// 128V input to the palette PROM
		if (sy >= 128) color |= 0x40;

		zoom_sprite(BIT(siz0, 15), code, color, flipx, sx, sy, sizex, sizey);
	}
}

static void draw_alpha()
{
	const INT32 chacl = BIT(latch, 7);

	for (INT32 tile_index = 0; tile_index < 32 * 32; tile_index++)
	{
		UINT16 word = WORD16(DrvAlphaRAM, tile_index);
		INT32 code = (word & 0xff) | ((word & 0x4000) >> 6);
		INT32 color = (word & 0x3f00) >> 8;

		// without CHACL only the bottom 8 bits (the z80's view) are used
		if (chacl == 0) {
			code &= 0xff;
			color = 0;
		}
		code &= nCharMask;

		INT32 colorbase = color << 2;
		if (tile_index >= 32 * 16) colorbase |= 0x100;	// 128V input to the palette PROM

		const UINT8 *gfx = DrvGfxChars + (code << 6);
		const UINT8 *lut = DrvColPROM + 0x300 + (color << 2);
		INT32 sx = (tile_index & 0x1f) << 3;
		INT32 sy = (tile_index >> 5) << 3;

		for (INT32 y = 0; y < 8; y++)
		{
			UINT16 *dst = DrvBitmap + (sy + y) * 256 + sx;
			for (INT32 x = 0; x < 8; x++)
			{
				INT32 pix = gfx[(y << 3) + x];
				if (lut[pix] != 15) dst[x] = colorbase + pix;
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (BurnRecalc) {
		DrvRecalcPalette();
		BurnRecalc = 0;
	}

	for (INT32 i = 0; i < 256 * 256; i++) DrvBitmap[i] = 0x200;

	if (nBurnLayer & 1) draw_background();
	if (nBurnLayer & 2) draw_road();
	if (nSpriteEnable & 1) draw_sprites();
	if (nBurnLayer & 4) draw_alpha();

	// visible area is lines 16-239
	for (INT32 y = 0; y < nScreenHeight; y++) {
		memcpy(pTransDraw + y * nScreenWidth, DrvBitmap + (y + 16) * 256, nScreenWidth * sizeof(UINT16));
	}

	BurnTransferCopy(DrvPalette);

	BurnShiftRender();

	return 0;
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset(AllRam, 0, RamEnd - AllRam);
	}

	latch = 0;
	road_vscroll = 0;
	bg_hscroll = 0;
	sub_irq_mask = 0;
	adc_input = 0;
	adc_result = 0;
	adc_intr = 0;
	adc_conv_end = -1;
	steer_last = steer_delta = 0;
	steer_accum = 0;
	ic25_last_result = 0;
	ic25_last_signed = 0;
	ic25_last_unsigned = 0;
	watchdog = 0;
	memset(nExtraCycles, 0, sizeof(nExtraCycles));

	n06xx_control = 0;
	n06xx_timer_state = 0;
	n06xx_read_stretch = 0;
	n06xx_next = -1;
	n06xx_period = 0;
	n06xx_nmi = 0;

	memset(mcu_o_latch, 0, sizeof(mcu_o_latch));
	n51xx_portO = n51xx_rw = 0;
	n52xx_cmd = 0;
	n52xx_address = 0;
	n53xx_portO = 0;
	n54xx_cmd = 0;

	sound_enable = 0;
	memset(dac54, 0, sizeof(dac54));
	dac52 = 0;
	engine_msb = engine_lsb = engine_enable = 0;
	engine_position = 0;
	sound_reset_filters();

	ZetOpen(0);
	ZetReset();
	NamcoSoundReset();
	for (INT32 i = 0; i < 0x40; i++) namco_polepos_write(i, 0);	// clear the registers (buffered: needs the z80 open)
	namco_15xx_sound_enable(0);
	ZetClose();

	BurnShiftReset();	// LO gear

	// the LS259 powers up cleared: both Z8002 and the customs are held in reset
	for (INT32 i = 0; i < 2; i++) {
		Z8002Open(i);
		Z8002Reset();
		Z8002SetRESETLine(1);
		Z8002SetIRQLine(Z8002_INPUT_LINE_NVI, CPU_IRQSTATUS_NONE);
		Z8002Close();
	}

	for (INT32 i = 0; i < MCU_COUNT; i++) {
		mb88xxOpen(i);
		mb88xxReset();
		mb88xxSetRESETLine(1);
		mb88xxSetIRQLine(0, CPU_IRQSTATUS_NONE);
		mb88xxClose();
	}

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM			= Next; Next += 0x003000;
	DrvSub1ROM			= Next; Next += 0x008000;
	DrvSub2ROM			= Next; Next += 0x008000;
	for (INT32 i = 0; i < MCU_COUNT; i++) {
		DrvMCUROM[i]	= Next; Next += 0x000400;
	}

	DrvGfxChars			= Next; Next += 0x200 * 8 * 8;
	DrvGfxTiles			= Next; Next += 0x200 * 8 * 8;
	DrvGfxSprites		= Next; Next += 0x080 * 16 * 16;
	DrvGfxBigSprites	= Next; Next += 0x080 * 32 * 32;

	DrvRoadROM			= Next; Next += 0x005000;
	DrvScaleROM			= Next; Next += 0x001000;
	DrvColPROM			= Next; Next += 0x001040;
	DrvWavePROM			= Next; Next += 0x000100;
	DrvEngineROM		= Next; Next += 0x004000;
	Drv52xxROM			= Next; Next += 0x008000;

	DrvPalette			= (UINT32*)Next; Next += 0x0f00 * sizeof(UINT32);
	DrvPaletteRGB		= (UINT32*)Next; Next += 0x0080 * sizeof(UINT32);
	VertPosModifier		= (UINT16*)Next; Next += 0x0100 * sizeof(UINT16);
	DrvBitmap			= (UINT16*)Next; Next += 256 * 256 * sizeof(UINT16);

	DrvNVRAM			= Next; Next += 0x000800;

	AllRam				= Next;

	DrvZ80RAM			= Next; Next += 0x000400;
	DrvSprRAM			= Next; Next += 0x001000;
	DrvRoadRAM			= Next; Next += 0x000800;
	DrvAlphaRAM			= Next; Next += 0x000800;
	DrvViewRAM			= Next; Next += 0x001000;

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static INT32 DrvLoadRoms(INT32 *char_len, INT32 *tile_len)
{
	char *pRomName;
	struct BurnRomInfo ri;

	UINT8 *gfx_chars   = DrvTempGfx + 0x00000;
	UINT8 *gfx_tiles   = DrvTempGfx + 0x02000;
	UINT8 *gfx_sprites = DrvTempGfx + 0x04000;
	UINT8 *gfx_big     = DrvTempGfx + 0x08000;

	INT32 z80 = 0, sub1 = 0, sub2 = 0, chars = 0, tiles = 0, spr = 0, big_lo = 0, big_hi = 0x8000;
	INT32 road = 0, prom = 0, engine = 0, s52 = 0;

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++)
	{
		BurnDrvGetRomInfo(&ri, i);
		INT32 type = ri.nType & 0x1f;
		if (type == 0 || (ri.nType & BRF_OPT)) continue;

		switch (type)
		{
			case 1:  if (BurnLoadRom(DrvZ80ROM + z80, i, 1)) return 1; z80 += ri.nLen; break;

			case 2:	// Z8002 #1: odd (low) byte rom followed by the even (high) byte rom
				if (BurnLoadRom(DrvSub1ROM + sub1 + 1, i + 0, 2)) return 1;
				if (BurnLoadRom(DrvSub1ROM + sub1 + 0, i + 1, 2)) return 1;
				sub1 += ri.nLen * 2; i++;
			break;

			case 19:	// Z8002 #1, lone even byte rom (bootleg ic25 replacement)
				if (BurnLoadRom(DrvSub1ROM + sub1 + 0, i, 2)) return 1;
				sub1 += ri.nLen * 2;
			break;

			case 3:
				if (BurnLoadRom(DrvSub2ROM + sub2 + 1, i + 0, 2)) return 1;
				if (BurnLoadRom(DrvSub2ROM + sub2 + 0, i + 1, 2)) return 1;
				sub2 += ri.nLen * 2; i++;
			break;

			case 4:  if (BurnLoadRom(gfx_chars + chars, i, 1)) return 1; chars += ri.nLen; break;
			case 5:  if (BurnLoadRom(gfx_tiles + tiles, i, 1)) return 1; tiles += ri.nLen; break;
			case 6:  if (BurnLoadRom(gfx_sprites + spr, i, 1)) return 1; spr += ri.nLen; break;
			case 7:  if (BurnLoadRom(gfx_big + big_lo, i, 1)) return 1; big_lo += ri.nLen; break;
			case 8:  if (BurnLoadRom(gfx_big + big_hi, i, 1)) return 1; big_hi += ri.nLen; break;
			case 9:  if (BurnLoadRom(DrvRoadROM + road, i, 1)) return 1; road += ri.nLen; break;
			case 10: if (BurnLoadRom(DrvScaleROM, i, 1)) return 1; break;
			case 11: if (BurnLoadRom(DrvColPROM + prom, i, 1)) return 1; prom += ri.nLen; break;
			case 12: if (BurnLoadRom(DrvWavePROM, i, 1)) return 1; break;
			case 13: if (BurnLoadRom(DrvEngineROM + engine, i, 1)) return 1; engine += ri.nLen; break;
			case 14: if (BurnLoadRom(Drv52xxROM + s52, i, 1)) return 1; s52 += ri.nLen; break;
			case 15: case 16: case 17: case 18:
				if (BurnLoadRom(DrvMCUROM[type - 15], i, 1)) return 1;
			break;
		}
	}

	*char_len = chars;
	*tile_len = tiles;

	return 0;
}

static void DrvGfxDecode(INT32 char_len, INT32 tile_len)
{
	INT32 Plane2[2]  = { 0, 4 };
	INT32 XOffs2[8]  = { 0, 1, 2, 3, 8*8+0, 8*8+1, 8*8+2, 8*8+3 };
	INT32 YOffs2[8]  = { STEP8(0, 8) };

	INT32 PlaneS[4]  = { 0, 4, 0x2000*8+0, 0x2000*8+4 };
	INT32 XOffsS[16] = { 0, 1, 2, 3, 8, 9, 10, 11, 16, 17, 18, 19, 24, 25, 26, 27 };
	INT32 YOffsS[16] = { STEP16(0, 32) };

	INT32 PlaneB[4]  = { 0, 4, 0x8000*8+0, 0x8000*8+4 };
	INT32 XOffsB[32] = { 0, 1, 2, 3, 8, 9, 10, 11, 16, 17, 18, 19, 24, 25, 26, 27,
						 32, 33, 34, 35, 40, 41, 42, 43, 48, 49, 50, 51, 56, 57, 58, 59 };
	INT32 YOffsB[32] = { STEP32(0, 64) };

	nCharMask = (char_len / 16) - 1;
	nTileMask = (tile_len / 16) - 1;

	GfxDecode(char_len / 16, 2,  8,  8, Plane2, XOffs2, YOffs2, 0x080, DrvTempGfx + 0x00000, DrvGfxChars);
	GfxDecode(tile_len / 16, 2,  8,  8, Plane2, XOffs2, YOffs2, 0x080, DrvTempGfx + 0x02000, DrvGfxTiles);
	GfxDecode(0x80,          4, 16, 16, PlaneS, XOffsS, YOffsS, 0x200, DrvTempGfx + 0x04000, DrvGfxSprites);
	GfxDecode(0x80,          4, 32, 32, PlaneB, XOffsB, YOffsB, 0x800, DrvTempGfx + 0x08000, DrvGfxBigSprites);
}

static INT32 DrvInit(INT32 has_ic25, INT32 len_52xx)
{
	BurnAllocMemIndex();

	DrvTempGfx = (UINT8*)BurnMalloc(0x18000);
	memset(DrvTempGfx, 0, 0x18000);

	INT32 char_len = 0, tile_len = 0;
	if (DrvLoadRoms(&char_len, &tile_len)) {
		BurnFree(DrvTempGfx);
		return 1;
	}

	DrvGfxDecode(char_len, tile_len);
	BurnFree(DrvTempGfx);

	DrvPaletteInit();

	game_has_ic25 = has_ic25;
	n52xxLength = len_52xx;

	memset(DrvNVRAM, 0xff, 0x800);	// battery backed ram defaults to all 1s

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,				0x0000, 0x2fff, MAP_ROM);
	ZetMapMemory(DrvNVRAM,				0x3000, 0x37ff, MAP_RAM);
	ZetMapMemory(DrvNVRAM,				0x3800, 0x3fff, MAP_RAM);
	for (INT32 i = 0x8000; i < 0x9000; i += 0x400) {
		ZetMapMemory(DrvZ80RAM,			i, i + 0x2ff, MAP_RAM);	// page +0x300 also holds the sound registers
	}
	ZetSetWriteHandler(polepos_z80_write);
	ZetSetReadHandler(polepos_z80_read);
	ZetSetOutHandler(polepos_z80_out);
	ZetSetInHandler(polepos_z80_in);
	ZetClose();

	for (INT32 i = 0; i < 2; i++)
	{
		UINT8 *rom = i ? DrvSub2ROM : DrvSub1ROM;

		Z8002Init(i);
		Z8002Open(i);
		if (i == 0 && game_has_ic25) {
			Z8002MapMemory(rom,				0x0000, 0x7fff, MAP_FETCH);
			Z8002MapMemory(rom,				0x0000, 0x3fff, MAP_READ);
			Z8002MapMemory(rom + 0x6000,	0x6000, 0x7fff, MAP_READ);
		} else {
			Z8002MapMemory(rom,				0x0000, 0x7fff, MAP_READ | MAP_FETCH);
		}
		Z8002MapMemory(DrvSprRAM,			0x8000, 0x8fff, MAP_RAM);
		Z8002MapMemory(DrvRoadRAM,			0x9000, 0x97ff, MAP_RAM);
		Z8002MapMemory(DrvAlphaRAM,			0x9800, 0x9fff, MAP_RAM);
		Z8002MapMemory(DrvViewRAM,			0xa000, 0xafff, MAP_RAM);
		Z8002SetReadWordHandler(z8002_read_word);
		Z8002SetReadByteHandler(z8002_read_byte);
		Z8002SetWriteWordHandler(z8002_write_word);
		Z8002SetWriteByteHandler(z8002_write_byte);
		Z8002Close();
	}

	mcu_init();

	sound_init();
	stream.init(SOUND_RATE, nBurnSoundRate, 2, 0, polepos_sound_render);
	stream.set_volume(1.00);
	stream.set_buffered(sound_sync_cycles, CPU_CLOCK);

	NamcoSoundProm = DrvWavePROM;
	NamcoSoundInit(MASTER_CLOCK / 512, 8, 1);	// 48 kHz, 8 voices, front + rear mixed down to stereo
	NamcoSoundSetStereo(1);
	NamcoSoundSetAllRoutes(0.80, BURN_SND_ROUTE_BOTH);
	NamcoSoundSetBuffered(ZetTotalCycles, CPU_CLOCK);

	BurnTrackballInit(1);

	BurnShiftInit(SHIFT_POSITION_BOTTOM_RIGHT, SHIFT_COLOR_GREEN, 80);

	BurnSetRefreshRate((double)(MASTER_CLOCK / 4) / (384 * LINES_PER_FRAME));	// 60.606 Hz

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static INT32 PoleposInit()   { return DrvInit(0, 0x8000); }
static INT32 PoleposAInit()  { return DrvInit(0, 0x6000); }
static INT32 Polepos2Init()  { return DrvInit(1, 0x8000); }
static INT32 Polepos2aInit() { return DrvInit(1, 0x6000); }
static INT32 Polepos2bInit() { return DrvInit(0, 0x6000); }

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	Z8002Exit();
	mb88xxExit();

	stream.exit();
	NamcoSoundExit();
	BurnTrackballExit();
	BurnShiftExit();

	BurnFreeMemIndex();

	return 0;
}

static void DrvMakeInputs()
{
	UINT8 gear = BurnShiftInputCheckToggle(DrvJoy1[1]);	// press to change gear

	DrvInput0 = 0xff;
	if (gear)       DrvInput0 &= ~0x02;
	if (DrvJoy1[4]) DrvInput0 &= ~0x10;
	if (DrvJoy1[5]) DrvInput0 &= ~0x20;
	if (DrvJoy1[6]) DrvInput0 &= ~0x40;
	DrvInput0 = (DrvInput0 & 0x7f) | (DrvDips[2] & 0x80);

	// steering: a free-spinning encoder in the cabinet; the analog axis
	BurnTrackballConfig(0, AXIS_NORMAL, AXIS_NORMAL);
	BurnTrackballFrame(0, DrvAnalogWheel/2, 0, 0x00, 0x08);
	BurnTrackballUDLR(0, 0, 0, DrvJoy2[0], DrvJoy2[1], 4);
	BurnTrackballUpdate(0);
	steer_value = BurnTrackballRead(0, 0);
}

static inline void update_start_input()
{
	DrvInput0 = (DrvInput0 & ~0x04) | (BIT(latch, 6) << 2);
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset(1);
	}

	if (++watchdog >= 16 * 4) {	// 16 vblanks on hardware; be lenient while the cpus sync up
		bprintf(0, _T("Pole Position: watchdog reset\n"));
		DrvDoReset(0);
	}

	ZetNewFrame();
	Z8002NewFrame();
	mb88xxNewFrame();

	DrvMakeInputs();

	INT32 nInterleave = LINES_PER_FRAME;
	INT32 nCyclesTotal[3] = { CYCLES_PER_FRAME, CYCLES_PER_FRAME, CYCLES_PER_FRAME };	// z80, z8002 #1, z8002 #2
	INT32 nCyclesDone[3] = { nExtraCycles[0], nExtraCycles[1], nExtraCycles[2] };

	ZetIdle(0, nExtraCycles[0]);
	for (INT32 i = 0; i < 2; i++) {
		Z8002Open(i);
		Z8002Idle(nExtraCycles[1 + i]);
		Z8002Close();
	}
	for (INT32 i = 0; i < MCU_COUNT; i++) {
		mb88xxOpen(i);
		mb88xxIdle(nExtraCycles[3 + i]);
		mb88xxClose();
	}

	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		scanline = i;

		update_start_input();

		if ((scanline == 64 || scanline == 192) && BIT(latch, 0)) {
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		}

		if (scanline == 240) {
			if (pBurnDraw) {
				BurnDrvRedraw();
			}
			if (sub_irq_mask) {
				for (INT32 j = 0; j < 2; j++) {
					Z8002Open(j);
					Z8002SetIRQLine(Z8002_INPUT_LINE_NVI, CPU_IRQSTATUS_ACK);
					Z8002Close();
				}
			}
			sync_mcus(ZetTotalCycles());
			mb88xxOpen(MCU_51XX);
			mb88xx_clock_write(0);
			mb88xxClose();
		}
		if (scanline == 16) {
			sync_mcus(ZetTotalCycles());
			mb88xxOpen(MCU_51XX);
			mb88xx_clock_write(1);
			mb88xxClose();
		}

		// z80 and the customs, need more sync
		for (INT32 s = 0; s < SLICES_PER_LINE; s++)
		{
			INT32 z80_target = ((i * SLICES_PER_LINE) + s + 1) * nCyclesTotal[0] / (nInterleave * SLICES_PER_LINE);

			// run the z80, stopping at every 06xx edge
			while (ZetTotalCycles() < z80_target)
			{
				INT32 stop = z80_target;
				if (n06xx_next >= 0 && n06xx_next < stop) stop = n06xx_next;

				INT32 todo = stop - ZetTotalCycles();
				if (todo > 0) ZetRun(todo);

				n06xx_update(ZetTotalCycles());
			}

			sync_mcus(z80_target);
		}
		nCyclesDone[0] = ZetTotalCycles();

		// the two Z8002s only talk to the rest through shared ram and their irqs
		for (INT32 j = 0; j < 2; j++) {
			Z8002Open(j);
			CPU_RUN_SYNCINT(1 + j, Z8002);
			Z8002Close();
		}
	}

	ZetClose();

	nExtraCycles[0] = nCyclesDone[0] - nCyclesTotal[0];
	nExtraCycles[1] = nCyclesDone[1] - nCyclesTotal[1];
	nExtraCycles[2] = nCyclesDone[2] - nCyclesTotal[2];
	for (INT32 i = 0; i < MCU_COUNT; i++) {
		mb88xxOpen(i);
		nExtraCycles[3 + i] = mb88xxTotalCycles() - (CYCLES_PER_FRAME / MCU_DIVIDER);
		mb88xxClose();
	}

	// keep the 06xx and adc timers frame relative
	if (n06xx_next >= 0) n06xx_next -= CYCLES_PER_FRAME;
	if (adc_conv_end >= 0) {
		adc_conv_end -= CYCLES_PER_FRAME;
		if (adc_conv_end < 0) adc_conv_end = 0;
	}

	if (pBurnSoundOut) {
		stream.render(pBurnSoundOut, nBurnSoundLen);
		NamcoSoundUpdate(pBurnSoundOut, nBurnSoundLen);
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029702;
	}

	if (nAction & ACB_VOLATILE)
	{
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);
		Z8002Scan(nAction);
		mb88xxScan(nAction);

		BurnTrackballScan();
		BurnShiftScan(nAction);
		NamcoSoundScan(nAction, NULL);

		SCAN_VAR(latch);
		SCAN_VAR(road_vscroll);
		SCAN_VAR(bg_hscroll);
		SCAN_VAR(sub_irq_mask);
		SCAN_VAR(adc_input);
		SCAN_VAR(adc_result);
		SCAN_VAR(adc_intr);
		SCAN_VAR(adc_conv_end);
		SCAN_VAR(steer_last);
		SCAN_VAR(steer_delta);
		SCAN_VAR(steer_accum);
		SCAN_VAR(ic25_last_result);
		SCAN_VAR(ic25_last_signed);
		SCAN_VAR(ic25_last_unsigned);
		SCAN_VAR(watchdog);
		SCAN_VAR(nExtraCycles);

		SCAN_VAR(n06xx_control);
		SCAN_VAR(n06xx_timer_state);
		SCAN_VAR(n06xx_read_stretch);
		SCAN_VAR(n06xx_next);
		SCAN_VAR(n06xx_period);
		SCAN_VAR(n06xx_nmi);

		SCAN_VAR(mcu_o_latch);
		SCAN_VAR(n51xx_portO);
		SCAN_VAR(n51xx_rw);
		SCAN_VAR(n52xx_cmd);
		SCAN_VAR(n52xx_address);
		SCAN_VAR(n53xx_portO);
		SCAN_VAR(n54xx_cmd);

		SCAN_VAR(sound_enable);
		SCAN_VAR(dac54);
		SCAN_VAR(dac52);
		SCAN_VAR(engine_msb);
		SCAN_VAR(engine_lsb);
		SCAN_VAR(engine_enable);
		SCAN_VAR(engine_position);
		SCAN_VAR(engine_sub);
		SCAN_VAR(engine_prev);
		SCAN_VAR(engine_cur);
		SCAN_VAR(flt54);
		SCAN_VAR(flt52_hp);
		SCAN_VAR(flt52_lp);
		SCAN_VAR(flt_engine);
	}

	if (nAction & ACB_NVRAM)
	{
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = DrvNVRAM;
		ba.nLen	  = 0x800;
		ba.szName = "NV RAM";
		BurnAcb(&ba);
	}

	return 0;
}

// Pole Position (World)

static struct BurnRomInfo poleposRomDesc[] = {
	{ "pp3_9.6h",          0x2000, 0xc0511173, 1 | BRF_PRG | BRF_ESS  }, //  0 Z80 Code
	{ "pp1_10b.5h",        0x1000, 0x7174bcb7, 1 | BRF_PRG | BRF_ESS  }, //  1 
	{ "pp3_1.8m",          0x2000, 0x65c1c2c2, 2 | BRF_PRG | BRF_ESS  }, //  2 Z8002 #1 Code (odd)
	{ "pp3_2.8l",          0x2000, 0xfafb9049, 2 | BRF_PRG | BRF_ESS  }, //  3 Z8002 #1 Code (even)
	{ "pp3_5.4m",          0x2000, 0x46e5c99a, 3 | BRF_PRG | BRF_ESS  }, //  4 Z8002 #2 Code (odd)
	{ "pp3_6.4l",          0x2000, 0xacc1ebc3, 3 | BRF_PRG | BRF_ESS  }, //  5 Z8002 #2 Code (even)
	{ "pp3_28.1f",         0x1000, 0x2e77187e, 4 | BRF_GRA            }, //  6 Alpha Characters
	{ "pp1_29.1e",         0x1000, 0x706e888a, 5 | BRF_GRA            }, //  7 Background Tiles
	{ "pp3_25.1n",         0x2000, 0xb52c086b, 6 | BRF_GRA            }, //  8 Small Sprites
	{ "pp3_26.1m",         0x2000, 0xd24a5707, 6 | BRF_GRA            }, //  9 
	{ "pp1_17.5n",         0x2000, 0x2e134b46, 7 | BRF_GRA            }, // 10 Big Sprites (planes 0+1)
	{ "pp1_19.4n",         0x2000, 0x43ff83e1, 7 | BRF_GRA            }, // 11 
	{ "pp1_21.3n",         0x2000, 0x5f958eb4, 7 | BRF_GRA            }, // 12 
	{ "pp1_18.5m",         0x2000, 0x6f9997d2, 8 | BRF_GRA            }, // 13 Big Sprites (planes 2+3)
	{ "pp1_20.4m",         0x2000, 0xec18075b, 8 | BRF_GRA            }, // 14 
	{ "pp1_22.3m",         0x2000, 0x1d2f30b1, 8 | BRF_GRA            }, // 15 
	{ "pp1_30.3a",         0x2000, 0xee6b3315, 9 | BRF_GRA            }, // 16 Road Data
	{ "pp1_31.2a",         0x2000, 0x6d1e7042, 9 | BRF_GRA            }, // 17 
	{ "pp1_32.1a",         0x1000, 0x4e97f101, 9 | BRF_GRA            }, // 18 
	{ "pp1_27.1l",         0x1000, 0xa61bff15, 10 | BRF_GRA           }, // 19 Sprite Scaling LUT
	{ "pp1-7.8l",          0x0100, 0xf07ff2ad, 11 | BRF_GRA           }, // 20 Color / Lookup PROMs
	{ "pp1-8.9l",          0x0100, 0xadbde7d7, 11 | BRF_GRA           }, // 21 
	{ "pp1-9.10l",         0x0100, 0xddac786a, 11 | BRF_GRA           }, // 22 
	{ "pp2-10.2h",         0x0100, 0x1e8d0491, 11 | BRF_GRA           }, // 23 
	{ "pp1-11.4d",         0x0100, 0x0e4fe8a0, 11 | BRF_GRA           }, // 24 
	{ "pp1-15.9a",         0x0100, 0x2d502464, 11 | BRF_GRA           }, // 25 
	{ "pp1-16.10a",        0x0100, 0x027aa62c, 11 | BRF_GRA           }, // 26 
	{ "pp1-17.11a",        0x0100, 0x1f8d0df3, 11 | BRF_GRA           }, // 27 
	{ "pp1-12.3c",         0x0400, 0x7afc7cfc, 11 | BRF_GRA           }, // 28 
	{ "pp3-6.6m",          0x0400, 0x63fb6057, 11 | BRF_GRA           }, // 29 
	{ "pp1-13.8e",         0x0020, 0x4330a51b, 11 | BRF_GRA           }, // 30 
	{ "pp1-14.9e",         0x0020, 0x4330a51b, 11 | BRF_GRA           }, // 31 
	{ "pp1-5.3b",          0x0100, 0x8568decc, 12 | BRF_SND           }, // 32 Namco Wave PROM
	{ "pp1_15.6a",         0x2000, 0xb5ad4d5f, 13 | BRF_SND           }, // 33 Engine Sound
	{ "pp1_16.5a",         0x2000, 0x8fdd2f6f, 13 | BRF_SND           }, // 34 
	{ "pp2_11.2e",         0x2000, 0x5b4cf05e, 14 | BRF_SND           }, // 35 52xx Samples
	{ "pp2_12.2f",         0x2000, 0x32b694c2, 14 | BRF_SND           }, // 36 
	{ "pp2_13.1e",         0x2000, 0x8842138a, 14 | BRF_SND           }, // 37 
	{ "pp1-4.9h",          0x0100, 0x2401c817, 0 | BRF_OPT            }, // 38 unused

	{ "51xx.bin",          0x0400, 0xc2f57ef8, 15 | BRF_PRG | BRF_ESS }, // 39 51xx MCU
	{ "52xx.bin",          0x0400, 0x3257d11e, 16 | BRF_PRG | BRF_ESS }, // 40 52xx MCU
	{ "53xx.bin",          0x0400, 0xb326fecb, 17 | BRF_PRG | BRF_ESS }, // 41 53xx MCU
	{ "54xx.bin",          0x0400, 0xee7357e0, 18 | BRF_PRG | BRF_ESS }, // 42 54xx MCU
};

STD_ROM_PICK(polepos)
STD_ROM_FN(polepos)

struct BurnDriver BurnDrvPolepos = {
	"polepos", NULL, NULL, NULL, "1982",
	"Pole Position (World)\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, poleposRomInfo, poleposRomName, NULL, NULL, NULL, NULL, PoleposInputInfo, PoleposaDIPInfo,
	PoleposInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x0f00,
	256, 224, 4, 3
};

// Pole Position (Japan)

static struct BurnRomInfo poleposjRomDesc[] = {
	{ "pp1_9b.6h",         0x2000, 0x94436b70, 1 | BRF_PRG | BRF_ESS  }, //  0 Z80 Code
	{ "pp1_10b.5h",        0x1000, 0x7174bcb7, 1 | BRF_PRG | BRF_ESS  }, //  1 
	{ "pp1_1b.8m",         0x2000, 0x361c56dd, 2 | BRF_PRG | BRF_ESS  }, //  2 Z8002 #1 Code (odd)
	{ "pp1_2b.8l",         0x2000, 0x582b530a, 2 | BRF_PRG | BRF_ESS  }, //  3 Z8002 #1 Code (even)
	{ "pp1_5b.4m",         0x2000, 0x5cdf5294, 3 | BRF_PRG | BRF_ESS  }, //  4 Z8002 #2 Code (odd)
	{ "pp1_6b.4l",         0x2000, 0x81696272, 3 | BRF_PRG | BRF_ESS  }, //  5 Z8002 #2 Code (even)
	{ "pp1_28.1f",         0x1000, 0x5b277daf, 4 | BRF_GRA            }, //  6 Alpha Characters
	{ "pp1_29.1e",         0x1000, 0x706e888a, 5 | BRF_GRA            }, //  7 Background Tiles
	{ "pp1_25.1n",         0x2000, 0xac8e28c1, 6 | BRF_GRA            }, //  8 Small Sprites
	{ "pp1_26.1m",         0x2000, 0x94443079, 6 | BRF_GRA            }, //  9 
	{ "pp1_17.5n",         0x2000, 0x2e134b46, 7 | BRF_GRA            }, // 10 Big Sprites (planes 0+1)
	{ "pp1_19.4n",         0x2000, 0x43ff83e1, 7 | BRF_GRA            }, // 11 
	{ "pp1_21.3n",         0x2000, 0x5f958eb4, 7 | BRF_GRA            }, // 12 
	{ "pp1_18.5m",         0x2000, 0x6f9997d2, 8 | BRF_GRA            }, // 13 Big Sprites (planes 2+3)
	{ "pp1_20.4m",         0x2000, 0xec18075b, 8 | BRF_GRA            }, // 14 
	{ "pp1_22.3m",         0x2000, 0x1d2f30b1, 8 | BRF_GRA            }, // 15 
	{ "pp1_30.3a",         0x2000, 0xee6b3315, 9 | BRF_GRA            }, // 16 Road Data
	{ "pp1_31.2a",         0x2000, 0x6d1e7042, 9 | BRF_GRA            }, // 17 
	{ "pp1_32.1a",         0x1000, 0x4e97f101, 9 | BRF_GRA            }, // 18 
	{ "pp1_27.1l",         0x1000, 0xa61bff15, 10 | BRF_GRA           }, // 19 Sprite Scaling LUT
	{ "pp1-7.8l",          0x0100, 0xf07ff2ad, 11 | BRF_GRA           }, // 20 Color / Lookup PROMs
	{ "pp1-8.9l",          0x0100, 0xadbde7d7, 11 | BRF_GRA           }, // 21 
	{ "pp1-9.10l",         0x0100, 0xddac786a, 11 | BRF_GRA           }, // 22 
	{ "pp1-10.2h",         0x0100, 0x1e8d0491, 11 | BRF_GRA           }, // 23 
	{ "pp1-11.4d",         0x0100, 0x0e4fe8a0, 11 | BRF_GRA           }, // 24 
	{ "pp1-15.9a",         0x0100, 0x2d502464, 11 | BRF_GRA           }, // 25 
	{ "pp1-16.10a",        0x0100, 0x027aa62c, 11 | BRF_GRA           }, // 26 
	{ "pp1-17.11a",        0x0100, 0x1f8d0df3, 11 | BRF_GRA           }, // 27 
	{ "pp1-12.3c",         0x0400, 0x7afc7cfc, 11 | BRF_GRA           }, // 28 
	{ "pp1-6.6m",          0x0400, 0x2f1079ee, 11 | BRF_GRA           }, // 29 
	{ "pp1-13.8e",         0x0020, 0x4330a51b, 11 | BRF_GRA           }, // 30 
	{ "pp1-14.9e",         0x0020, 0x4330a51b, 11 | BRF_GRA           }, // 31 
	{ "pp1-5.3b",          0x0100, 0x8568decc, 12 | BRF_SND           }, // 32 Namco Wave PROM
	{ "pp1_15.6a",         0x2000, 0xb5ad4d5f, 13 | BRF_SND           }, // 33 Engine Sound
	{ "pp1_16.5a",         0x2000, 0x8fdd2f6f, 13 | BRF_SND           }, // 34 
	{ "pp1_11.2e",         0x2000, 0x45b9bfeb, 14 | BRF_SND           }, // 35 52xx Samples
	{ "pp1_12.2f",         0x2000, 0xa31b4be5, 14 | BRF_SND           }, // 36 
	{ "pp1_13.1e",         0x2000, 0xa4237466, 14 | BRF_SND           }, // 37 
	{ "pp1_14.1f",         0x2000, 0x944580f9, 14 | BRF_SND           }, // 38 
	{ "pp1-4.9h",          0x0100, 0x2401c817, 0 | BRF_OPT            }, // 39 unused

	{ "51xx.bin",          0x0400, 0xc2f57ef8, 15 | BRF_PRG | BRF_ESS }, // 40 51xx MCU
	{ "52xx.bin",          0x0400, 0x3257d11e, 16 | BRF_PRG | BRF_ESS }, // 41 52xx MCU
	{ "53xx.bin",          0x0400, 0xb326fecb, 17 | BRF_PRG | BRF_ESS }, // 42 53xx MCU
	{ "54xx.bin",          0x0400, 0xee7357e0, 18 | BRF_PRG | BRF_ESS }, // 43 54xx MCU
};

STD_ROM_PICK(poleposj)
STD_ROM_FN(poleposj)

struct BurnDriver BurnDrvPoleposj = {
	"poleposj", "polepos", NULL, NULL, "1982",
	"Pole Position (Japan)\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, poleposjRomInfo, poleposjRomName, NULL, NULL, NULL, NULL, PoleposInputInfo, PoleposDIPInfo,
	PoleposInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x0f00,
	256, 224, 4, 3
};

// Pole Position (Atari version 1)

static struct BurnRomInfo poleposa1RomDesc[] = {
	{ "136014-105.7h",     0x2000, 0xc918c043, 1 | BRF_PRG | BRF_ESS  }, //  0 Z80 Code
	{ "136014-116.7f",     0x1000, 0x7174bcb7, 1 | BRF_PRG | BRF_ESS  }, //  1 
	{ "136014-101.3l",     0x2000, 0x8c2cf172, 2 | BRF_PRG | BRF_ESS  }, //  2 Z8002 #1 Code (odd)
	{ "136014-102.4l",     0x2000, 0x51018857, 2 | BRF_PRG | BRF_ESS  }, //  3 Z8002 #1 Code (even)
	{ "136014-103.3e",     0x2000, 0xaf4fc019, 3 | BRF_PRG | BRF_ESS  }, //  4 Z8002 #2 Code (odd)
	{ "136014-104.4e",     0x2000, 0xba0045f3, 3 | BRF_PRG | BRF_ESS  }, //  5 Z8002 #2 Code (even)
	{ "136014-132.7n",     0x1000, 0xa949aa85, 4 | BRF_GRA            }, //  6 Alpha Characters
	{ "136014-133.6n",     0x1000, 0x706e888a, 5 | BRF_GRA            }, //  7 Background Tiles
	{ "136014-125.13n",    0x2000, 0xe7a09c93, 6 | BRF_GRA            }, //  8 Small Sprites
	{ "136014-126.12n",    0x2000, 0xdee7d687, 6 | BRF_GRA            }, //  9 
	{ "136014-119.13j",    0x2000, 0x2e134b46, 7 | BRF_GRA            }, // 10 Big Sprites (planes 0+1)
	{ "136014-121.13k",    0x2000, 0xa7e3a1c6, 7 | BRF_GRA            }, // 11 
	{ "136014-123.13l",    0x2000, 0x8992d381, 7 | BRF_GRA            }, // 12 
	{ "136014-120.12j",    0x2000, 0x6f9997d2, 8 | BRF_GRA            }, // 13 Big Sprites (planes 2+3)
	{ "136014-122.12k",    0x2000, 0x6c5c6e68, 8 | BRF_GRA            }, // 14 
	{ "136014-124.12l",    0x2000, 0x111896ad, 8 | BRF_GRA            }, // 15 
	{ "136014-127.2l",     0x2000, 0xee6b3315, 9 | BRF_GRA            }, // 16 Road Data
	{ "136014-128.2m",     0x2000, 0x6d1e7042, 9 | BRF_GRA            }, // 17 
	{ "136014-134.2n",     0x1000, 0x4e97f101, 9 | BRF_GRA            }, // 18 
	{ "136014-131.11n",    0x1000, 0x5921777f, 10 | BRF_GRA           }, // 19 Sprite Scaling LUT
	{ "136014-137.11e",    0x0100, 0xf07ff2ad, 11 | BRF_GRA           }, // 20 Color / Lookup PROMs
	{ "136014-138.11d",    0x0100, 0xadbde7d7, 11 | BRF_GRA           }, // 21 
	{ "136014-139.11c",    0x0100, 0xddac786a, 11 | BRF_GRA           }, // 22 
	{ "136014-140.8m",     0x0100, 0x1e8d0491, 11 | BRF_GRA           }, // 23 
	{ "136014-141.5k",     0x0100, 0x0e4fe8a0, 11 | BRF_GRA           }, // 24 
	{ "136014-142.2d",     0x0100, 0x2d502464, 11 | BRF_GRA           }, // 25 
	{ "136014-143.2c",     0x0100, 0x027aa62c, 11 | BRF_GRA           }, // 26 
	{ "136014-144.2b",     0x0100, 0x1f8d0df3, 11 | BRF_GRA           }, // 27 
	{ "136014-145.4l",     0x0400, 0x7afc7cfc, 11 | BRF_GRA           }, // 28 
	{ "136014-146.12h",    0x0400, 0xca4ba741, 11 | BRF_GRA           }, // 29 
	{ "136014-135.6e",     0x0020, 0x4330a51b, 11 | BRF_GRA           }, // 30 
	{ "136014-136.6d",     0x0020, 0x4330a51b, 11 | BRF_GRA           }, // 31 
	{ "136014-118.11d",    0x0100, 0x8568decc, 12 | BRF_SND           }, // 32 Namco Wave PROM
	{ "136014-110.12f",    0x2000, 0xb5ad4d5f, 13 | BRF_SND           }, // 33 Engine Sound
	{ "136014-111.12e",    0x2000, 0x8fdd2f6f, 13 | BRF_SND           }, // 34 
	{ "136014-106.9c",     0x2000, 0x5b4cf05e, 14 | BRF_SND           }, // 35 52xx Samples
	{ "136014-117.7l",     0x0100, 0x2401c817, 0 | BRF_OPT            }, // 36 unused
	{ "137316-001.2n",     0x0034, 0xdd37bd15, 0 | BRF_OPT            }, // 37 
	{ "137316-001.5c",     0x0034, 0xdd37bd15, 0 | BRF_OPT            }, // 38 
	{ "137279-001.7c",     0x00eb, 0x4ddc51ba, 0 | BRF_OPT            }, // 39 

	{ "51xx.bin",          0x0400, 0xc2f57ef8, 15 | BRF_PRG | BRF_ESS }, // 40 51xx MCU
	{ "52xx.bin",          0x0400, 0x3257d11e, 16 | BRF_PRG | BRF_ESS }, // 41 52xx MCU
	{ "53xx.bin",          0x0400, 0xb326fecb, 17 | BRF_PRG | BRF_ESS }, // 42 53xx MCU
	{ "54xx.bin",          0x0400, 0xee7357e0, 18 | BRF_PRG | BRF_ESS }, // 43 54xx MCU
};

STD_ROM_PICK(poleposa1)
STD_ROM_FN(poleposa1)

struct BurnDriver BurnDrvPoleposa1 = {
	"poleposa1", "polepos", NULL, NULL, "1982",
	"Pole Position (Atari version 1)\0", NULL, "Namco (Atari license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, poleposa1RomInfo, poleposa1RomName, NULL, NULL, NULL, NULL, PoleposInputInfo, PoleposaDIPInfo,
	PoleposAInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x0f00,
	256, 224, 4, 3
};

// Pole Position (Atari version 1 on Namco PCB)

static struct BurnRomInfo poleposa1nRomDesc[] = {
	{ "pp2_9.6h",          0x2000, 0xc918c043, 1 | BRF_PRG | BRF_ESS  }, //  0 Z80 Code
	{ "pp2_10.5h",         0x1000, 0x7174bcb7, 1 | BRF_PRG | BRF_ESS  }, //  1 
	{ "pp2_1.8m",          0x2000, 0x8c2cf172, 2 | BRF_PRG | BRF_ESS  }, //  2 Z8002 #1 Code (odd)
	{ "pp2_2.8l",          0x2000, 0x51018857, 2 | BRF_PRG | BRF_ESS  }, //  3 Z8002 #1 Code (even)
	{ "pp2_5.4m",          0x2000, 0xaf4fc019, 3 | BRF_PRG | BRF_ESS  }, //  4 Z8002 #2 Code (odd)
	{ "pp2_6.4l",          0x2000, 0xba0045f3, 3 | BRF_PRG | BRF_ESS  }, //  5 Z8002 #2 Code (even)
	{ "pp2_28.1f",         0x1000, 0xa949aa85, 4 | BRF_GRA            }, //  6 Alpha Characters
	{ "pp1_29.1e",         0x1000, 0x706e888a, 5 | BRF_GRA            }, //  7 Background Tiles
	{ "pp2_25.1n",         0x2000, 0x34c2d310, 6 | BRF_GRA            }, //  8 Small Sprites
	{ "pp2_26.1m",         0x2000, 0xba3005f3, 6 | BRF_GRA            }, //  9 
	{ "pp2_17.5n",         0x2000, 0x2e134b46, 7 | BRF_GRA            }, // 10 Big Sprites (planes 0+1)
	{ "pp2_19.4n",         0x2000, 0x1c5c05b7, 7 | BRF_GRA            }, // 11 
	{ "pp2_21.3n",         0x2000, 0x020f4de5, 7 | BRF_GRA            }, // 12 
	{ "pp2_18.5m",         0x2000, 0x6f9997d2, 8 | BRF_GRA            }, // 13 Big Sprites (planes 2+3)
	{ "pp2_20.4m",         0x2000, 0xb8a0411c, 8 | BRF_GRA            }, // 14 
	{ "pp2_22.3m",         0x2000, 0x4fbf3e94, 8 | BRF_GRA            }, // 15 
	{ "pp1_30.3a",         0x2000, 0xee6b3315, 9 | BRF_GRA            }, // 16 Road Data
	{ "pp1_31.2a",         0x2000, 0x6d1e7042, 9 | BRF_GRA            }, // 17 
	{ "pp1_32.1a",         0x1000, 0x4e97f101, 9 | BRF_GRA            }, // 18 
	{ "pp1_27.1l",         0x1000, 0xa61bff15, 10 | BRF_GRA           }, // 19 Sprite Scaling LUT
	{ "pp1-7.8l",          0x0100, 0xf07ff2ad, 11 | BRF_GRA           }, // 20 Color / Lookup PROMs
	{ "pp1-8.9l",          0x0100, 0xadbde7d7, 11 | BRF_GRA           }, // 21 
	{ "pp1-9.10l",         0x0100, 0xddac786a, 11 | BRF_GRA           }, // 22 
	{ "pp2-10.2h",         0x0100, 0x1e8d0491, 11 | BRF_GRA           }, // 23 
	{ "pp1-11.4d",         0x0100, 0x0e4fe8a0, 11 | BRF_GRA           }, // 24 
	{ "pp1-15.9a",         0x0100, 0x2d502464, 11 | BRF_GRA           }, // 25 
	{ "pp1-16.10a",        0x0100, 0x027aa62c, 11 | BRF_GRA           }, // 26 
	{ "pp1-17.11a",        0x0100, 0x1f8d0df3, 11 | BRF_GRA           }, // 27 
	{ "pp1-12.3c",         0x0400, 0x7afc7cfc, 11 | BRF_GRA           }, // 28 
	{ "pp2-6.6m",          0x0400, 0xb448c934, 11 | BRF_GRA           }, // 29 
	{ "pp1-13.8e",         0x0020, 0x4330a51b, 11 | BRF_GRA           }, // 30 
	{ "pp1-14.9e",         0x0020, 0x4330a51b, 11 | BRF_GRA           }, // 31 
	{ "136014-118.11d",    0x0100, 0x8568decc, 12 | BRF_SND           }, // 32 Namco Wave PROM
	{ "136014-110.12f",    0x2000, 0xb5ad4d5f, 13 | BRF_SND           }, // 33 Engine Sound
	{ "136014-111.12e",    0x2000, 0x8fdd2f6f, 13 | BRF_SND           }, // 34 
	{ "136014-106.9c",     0x2000, 0x5b4cf05e, 14 | BRF_SND           }, // 35 52xx Samples
	{ "136014-117.7l",     0x0100, 0x2401c817, 0 | BRF_OPT            }, // 36 unused
	{ "137316-001.2n",     0x0034, 0xdd37bd15, 0 | BRF_OPT            }, // 37 
	{ "137316-001.5c",     0x0034, 0xdd37bd15, 0 | BRF_OPT            }, // 38 
	{ "137279-001.7c",     0x00eb, 0x4ddc51ba, 0 | BRF_OPT            }, // 39 

	{ "51xx.bin",          0x0400, 0xc2f57ef8, 15 | BRF_PRG | BRF_ESS }, // 40 51xx MCU
	{ "52xx.bin",          0x0400, 0x3257d11e, 16 | BRF_PRG | BRF_ESS }, // 41 52xx MCU
	{ "53xx.bin",          0x0400, 0xb326fecb, 17 | BRF_PRG | BRF_ESS }, // 42 53xx MCU
	{ "54xx.bin",          0x0400, 0xee7357e0, 18 | BRF_PRG | BRF_ESS }, // 43 54xx MCU
};

STD_ROM_PICK(poleposa1n)
STD_ROM_FN(poleposa1n)

struct BurnDriver BurnDrvPoleposa1n = {
	"poleposa1n", "polepos", NULL, NULL, "1982",
	"Pole Position (Atari version 1 on Namco PCB)\0", NULL, "Namco (Atari license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, poleposa1nRomInfo, poleposa1nRomName, NULL, NULL, NULL, NULL, PoleposInputInfo, PoleposaDIPInfo,
	PoleposAInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x0f00,
	256, 224, 4, 3
};

// Pole Position (Atari version 2)

static struct BurnRomInfo poleposa2RomDesc[] = {
	{ "136014-105.7h",     0x2000, 0xc918c043, 1 | BRF_PRG | BRF_ESS  }, //  0 Z80 Code
	{ "136014-116.7f",     0x1000, 0x7174bcb7, 1 | BRF_PRG | BRF_ESS  }, //  1 
	{ "136014-101.3l",     0x2000, 0x8c2cf172, 2 | BRF_PRG | BRF_ESS  }, //  2 Z8002 #1 Code (odd)
	{ "136014-102.4l",     0x2000, 0x51018857, 2 | BRF_PRG | BRF_ESS  }, //  3 Z8002 #1 Code (even)
	{ "136014-203.3e",     0x2000, 0xeedea6e7, 3 | BRF_PRG | BRF_ESS  }, //  4 Z8002 #2 Code (odd)
	{ "136014-204.4e",     0x2000, 0xc52c98ed, 3 | BRF_PRG | BRF_ESS  }, //  5 Z8002 #2 Code (even)
	{ "136014-132.7n",     0x1000, 0xa949aa85, 4 | BRF_GRA            }, //  6 Alpha Characters
	{ "136014-133.6n",     0x1000, 0x706e888a, 5 | BRF_GRA            }, //  7 Background Tiles
	{ "136014-125.13n",    0x2000, 0xe7a09c93, 6 | BRF_GRA            }, //  8 Small Sprites
	{ "136014-126.12n",    0x2000, 0xdee7d687, 6 | BRF_GRA            }, //  9 
	{ "136014-119.13j",    0x2000, 0x2e134b46, 7 | BRF_GRA            }, // 10 Big Sprites (planes 0+1)
	{ "136014-121.13k",    0x2000, 0xa7e3a1c6, 7 | BRF_GRA            }, // 11 
	{ "136014-123.13l",    0x2000, 0x8992d381, 7 | BRF_GRA            }, // 12 
	{ "136014-120.12j",    0x2000, 0x6f9997d2, 8 | BRF_GRA            }, // 13 Big Sprites (planes 2+3)
	{ "136014-122.12k",    0x2000, 0x6c5c6e68, 8 | BRF_GRA            }, // 14 
	{ "136014-124.12l",    0x2000, 0x111896ad, 8 | BRF_GRA            }, // 15 
	{ "136014-127.2l",     0x2000, 0xee6b3315, 9 | BRF_GRA            }, // 16 Road Data
	{ "136014-128.2m",     0x2000, 0x6d1e7042, 9 | BRF_GRA            }, // 17 
	{ "136014-134.2n",     0x1000, 0x4e97f101, 9 | BRF_GRA            }, // 18 
	{ "136014-231.11n",    0x1000, 0xa61bff15, 10 | BRF_GRA           }, // 19 Sprite Scaling LUT
	{ "136014-137.11e",    0x0100, 0xf07ff2ad, 11 | BRF_GRA           }, // 20 Color / Lookup PROMs
	{ "136014-138.11d",    0x0100, 0xadbde7d7, 11 | BRF_GRA           }, // 21 
	{ "136014-139.11c",    0x0100, 0xddac786a, 11 | BRF_GRA           }, // 22 
	{ "136014-140.8m",     0x0100, 0x1e8d0491, 11 | BRF_GRA           }, // 23 
	{ "136014-141.5k",     0x0100, 0x0e4fe8a0, 11 | BRF_GRA           }, // 24 
	{ "136014-142.2d",     0x0100, 0x2d502464, 11 | BRF_GRA           }, // 25 
	{ "136014-143.2c",     0x0100, 0x027aa62c, 11 | BRF_GRA           }, // 26 
	{ "136014-144.2b",     0x0100, 0x1f8d0df3, 11 | BRF_GRA           }, // 27 
	{ "136014-145.4l",     0x0400, 0x7afc7cfc, 11 | BRF_GRA           }, // 28 
	{ "136014-146.12h",    0x0400, 0xca4ba741, 11 | BRF_GRA           }, // 29 
	{ "136014-135.6e",     0x0020, 0x4330a51b, 11 | BRF_GRA           }, // 30 
	{ "136014-136.6d",     0x0020, 0x4330a51b, 11 | BRF_GRA           }, // 31 
	{ "136014-118.11d",    0x0100, 0x8568decc, 12 | BRF_SND           }, // 32 Namco Wave PROM
	{ "136014-110.12f",    0x2000, 0xb5ad4d5f, 13 | BRF_SND           }, // 33 Engine Sound
	{ "136014-111.12e",    0x2000, 0x8fdd2f6f, 13 | BRF_SND           }, // 34 
	{ "136014-106.9c",     0x2000, 0x5b4cf05e, 14 | BRF_SND           }, // 35 52xx Samples
	{ "136014-117.7l",     0x0100, 0x2401c817, 0 | BRF_OPT            }, // 36 unused
	{ "137316-001.2n",     0x0034, 0xdd37bd15, 0 | BRF_OPT            }, // 37 
	{ "137316-001.5c",     0x0034, 0xdd37bd15, 0 | BRF_OPT            }, // 38 
	{ "137279-001.7c",     0x00eb, 0x4ddc51ba, 0 | BRF_OPT            }, // 39 

	{ "51xx.bin",          0x0400, 0xc2f57ef8, 15 | BRF_PRG | BRF_ESS }, // 40 51xx MCU
	{ "52xx.bin",          0x0400, 0x3257d11e, 16 | BRF_PRG | BRF_ESS }, // 41 52xx MCU
	{ "53xx.bin",          0x0400, 0xb326fecb, 17 | BRF_PRG | BRF_ESS }, // 42 53xx MCU
	{ "54xx.bin",          0x0400, 0xee7357e0, 18 | BRF_PRG | BRF_ESS }, // 43 54xx MCU
};

STD_ROM_PICK(poleposa2)
STD_ROM_FN(poleposa2)

struct BurnDriver BurnDrvPoleposa2 = {
	"poleposa2", "polepos", NULL, NULL, "1982",
	"Pole Position (Atari version 2)\0", NULL, "Namco (Atari license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, poleposa2RomInfo, poleposa2RomName, NULL, NULL, NULL, NULL, PoleposInputInfo, PoleposaDIPInfo,
	PoleposAInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x0f00,
	256, 224, 4, 3
};

// Top Racer (with MB8841 + MB8842, 1984)

static struct BurnRomInfo topracerRomDesc[] = {
	{ "pp1_9b.6h",         0x2000, 0x94436b70, 1 | BRF_PRG | BRF_ESS  }, //  0 Z80 Code
	{ "136014.116",        0x1000, 0x7174bcb7, 1 | BRF_PRG | BRF_ESS  }, //  1 
	{ "tr1b.bin",          0x2000, 0x127f0750, 2 | BRF_PRG | BRF_ESS  }, //  2 Z8002 #1 Code (odd)
	{ "tr2b.bin",          0x2000, 0x6bd4ff6b, 2 | BRF_PRG | BRF_ESS  }, //  3 Z8002 #1 Code (even)
	{ "tr5b.bin",          0x2000, 0x4e5f7b9c, 3 | BRF_PRG | BRF_ESS  }, //  4 Z8002 #2 Code (odd)
	{ "tr6b.bin",          0x2000, 0x9d038ada, 3 | BRF_PRG | BRF_ESS  }, //  5 Z8002 #2 Code (even)
	{ "tr28.bin",          0x1000, 0xb8217c96, 4 | BRF_GRA            }, //  6 Alpha Characters
	{ "tr29.bin",          0x1000, 0xc6e15c21, 5 | BRF_GRA            }, //  7 Background Tiles
	{ "trus25.bin",        0x2000, 0x9e1a9c3b, 6 | BRF_GRA            }, //  8 Small Sprites
	{ "trus26.bin",        0x2000, 0x3b39a176, 6 | BRF_GRA            }, //  9 
	{ "pp17.bin",          0x2000, 0x613ab0df, 7 | BRF_GRA            }, // 10 Big Sprites (planes 0+1)
	{ "tr19.bin",          0x2000, 0xf8e7f551, 7 | BRF_GRA            }, // 11 
	{ "tr21.bin",          0x2000, 0x17c798b0, 7 | BRF_GRA            }, // 12 
	{ "pp18.bin",          0x2000, 0x5fd933e3, 8 | BRF_GRA            }, // 13 Big Sprites (planes 2+3)
	{ "tr20.bin",          0x2000, 0x7053e219, 8 | BRF_GRA            }, // 14 
	{ "tr22.bin",          0x2000, 0x5fe9b365, 8 | BRF_GRA            }, // 15 
	{ "136014.158",        0x2000, 0xee6b3315, 9 | BRF_GRA            }, // 16 Road Data
	{ "136014.159",        0x2000, 0x6d1e7042, 9 | BRF_GRA            }, // 17 
	{ "136014.134",        0x1000, 0x4e97f101, 9 | BRF_GRA            }, // 18 
	{ "136014.231",        0x1000, 0xa61bff15, 10 | BRF_GRA           }, // 19 Sprite Scaling LUT
	{ "136014.137",        0x0100, 0xf07ff2ad, 11 | BRF_GRA           }, // 20 Color / Lookup PROMs
	{ "136014.138",        0x0100, 0xadbde7d7, 11 | BRF_GRA           }, // 21 
	{ "136014.139",        0x0100, 0xddac786a, 11 | BRF_GRA           }, // 22 
	{ "7052-10.h15",       0x0100, 0x5af3f710, 11 | BRF_GRA           }, // 23 
	{ "7052-11.j15",       0x0100, 0x8c90e36e, 11 | BRF_GRA           }, // 24 
	{ "136014.142",        0x0100, 0x2d502464, 11 | BRF_GRA           }, // 25 
	{ "136014.143",        0x0100, 0x027aa62c, 11 | BRF_GRA           }, // 26 
	{ "136014.144",        0x0100, 0x1f8d0df3, 11 | BRF_GRA           }, // 27 
	{ "136014.145",        0x0400, 0x7afc7cfc, 11 | BRF_GRA           }, // 28 
	{ "7122.e7",           0x0400, 0x2f1079ee, 11 | BRF_GRA           }, // 29 
	{ "136014.135",        0x0020, 0x4330a51b, 11 | BRF_GRA           }, // 30 
	{ "136014.136",        0x0020, 0x4330a51b, 11 | BRF_GRA           }, // 31 
	{ "136014.118",        0x0100, 0x8568decc, 12 | BRF_SND           }, // 32 Namco Wave PROM
	{ "136014.110",        0x2000, 0xb5ad4d5f, 13 | BRF_SND           }, // 33 Engine Sound
	{ "136014.111",        0x2000, 0x8fdd2f6f, 13 | BRF_SND           }, // 34 
	{ "136014.106",        0x2000, 0x5b4cf05e, 14 | BRF_SND           }, // 35 52xx Samples
	{ "7052-4.c14",        0x0100, 0x0e742cb1, 0 | BRF_OPT            }, // 36 unused

	{ "51xx.bin",          0x0400, 0xc2f57ef8, 15 | BRF_PRG | BRF_ESS }, // 37 51xx MCU
	{ "52xx.bin",          0x0400, 0x3257d11e, 16 | BRF_PRG | BRF_ESS }, // 38 52xx MCU
	{ "53xx.bin",          0x0400, 0xb326fecb, 17 | BRF_PRG | BRF_ESS }, // 39 53xx MCU
	{ "54xx.bin",          0x0400, 0xee7357e0, 18 | BRF_PRG | BRF_ESS }, // 40 54xx MCU
};

STD_ROM_PICK(topracer)
STD_ROM_FN(topracer)

struct BurnDriver BurnDrvTopracer = {
	"topracer", "polepos", NULL, NULL, "1984",
	"Top Racer (with MB8841 + MB8842, 1984)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 1, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, topracerRomInfo, topracerRomName, NULL, NULL, NULL, NULL, PoleposInputInfo, PoleposDIPInfo,
	PoleposAInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x0f00,
	256, 224, 4, 3
};

// Top Racer (with MB8841 + MB8842, 1983)

static struct BurnRomInfo topraceraRomDesc[] = {
	{ "tr9.f17",           0x2000, 0x94436b70, 1 | BRF_PRG | BRF_ESS  }, //  0 Z80 Code
	{ "tr10.f16",          0x1000, 0x7174bcb7, 1 | BRF_PRG | BRF_ESS  }, //  1 
	{ "tr1b.f11",          0x2000, 0x127f0750, 2 | BRF_PRG | BRF_ESS  }, //  2 Z8002 #1 Code (odd)
	{ "tr2b.f8",           0x2000, 0x6bd4ff6b, 2 | BRF_PRG | BRF_ESS  }, //  3 Z8002 #1 Code (even)
	{ "tr5b.f7",           0x2000, 0x4e5f7b9c, 3 | BRF_PRG | BRF_ESS  }, //  4 Z8002 #2 Code (odd)
	{ "tr6b.f5",           0x2000, 0xb3641d0c, 3 | BRF_PRG | BRF_ESS  }, //  5 Z8002 #2 Code (even)
	{ "tr28.j9",           0x1000, 0xb8217c96, 4 | BRF_GRA            }, //  6 Alpha Characters
	{ "tr29.k9",           0x1000, 0xc6e15c21, 5 | BRF_GRA            }, //  7 Background Tiles
	{ "tr25.d5",           0x2000, 0x9e1a9c3b, 6 | BRF_GRA            }, //  8 Small Sprites
	{ "tr26.d8",           0x2000, 0x3b39a176, 6 | BRF_GRA            }, //  9 
	{ "tr17.a5",           0x2000, 0x613ab0df, 7 | BRF_GRA            }, // 10 Big Sprites (planes 0+1)
	{ "tr19.b5",           0x2000, 0xf8e7f551, 7 | BRF_GRA            }, // 11 
	{ "tr21.c5",           0x2000, 0x17c798b0, 7 | BRF_GRA            }, // 12 
	{ "tr18.a8",           0x2000, 0x5fd933e3, 8 | BRF_GRA            }, // 13 Big Sprites (planes 2+3)
	{ "tr20.b8",           0x2000, 0x7053e219, 8 | BRF_GRA            }, // 14 
	{ "tr22.c8",           0x2000, 0x5fe9b365, 8 | BRF_GRA            }, // 15 
	{ "tr30.b15",          0x2000, 0xee6b3315, 9 | BRF_GRA            }, // 16 Road Data
	{ "tr31.a15",          0x2000, 0x6d1e7042, 9 | BRF_GRA            }, // 17 
	{ "tr32.c15",          0x1000, 0x4e97f101, 9 | BRF_GRA            }, // 18 
	{ "tr27.d3",           0x1000, 0xa61bff15, 10 | BRF_GRA           }, // 19 Sprite Scaling LUT
	{ "7052-7.k21",        0x0100, 0xf07ff2ad, 11 | BRF_GRA           }, // 20 Color / Lookup PROMs
	{ "7052-8.k20",        0x0100, 0xadbde7d7, 11 | BRF_GRA           }, // 21 
	{ "7052-9.k19",        0x0100, 0xddac786a, 11 | BRF_GRA           }, // 22 
	{ "7052-10.h15",       0x0100, 0x5af3f710, 11 | BRF_GRA           }, // 23 
	{ "7052-11.j15",       0x0100, 0x8c90e36e, 11 | BRF_GRA           }, // 24 
	{ "7052-15.d1",        0x0100, 0x2d502464, 11 | BRF_GRA           }, // 25 
	{ "7052-16.d2",        0x0100, 0x027aa62c, 11 | BRF_GRA           }, // 26 
	{ "7052-17.d3",        0x0100, 0x1f8d0df3, 11 | BRF_GRA           }, // 27 
	{ "7122.a19",          0x0400, 0x7afc7cfc, 11 | BRF_GRA           }, // 28 
	{ "7122.e7",           0x0400, 0x2f1079ee, 11 | BRF_GRA           }, // 29 
	{ "7051-13.l7",        0x0020, 0x4330a51b, 11 | BRF_GRA           }, // 30 
	{ "7051-14.l8",        0x0020, 0x4330a51b, 11 | BRF_GRA           }, // 31 
	{ "7052-5.e9",         0x0100, 0x8568decc, 12 | BRF_SND           }, // 32 Namco Wave PROM
	{ "tr15.a8",           0x2000, 0xb5ad4d5f, 13 | BRF_SND           }, // 33 Engine Sound
	{ "tr16.b9",           0x2000, 0x8fdd2f6f, 13 | BRF_SND           }, // 34 
	{ "tr11.b1",           0x2000, 0x5b4cf05e, 14 | BRF_SND           }, // 35 52xx Samples
	{ "7052-4.c14",        0x0100, 0x0e742cb1, 0 | BRF_OPT            }, // 36 unused

	{ "51xx.bin",          0x0400, 0xc2f57ef8, 15 | BRF_PRG | BRF_ESS }, // 37 51xx MCU
	{ "52xx.bin",          0x0400, 0x3257d11e, 16 | BRF_PRG | BRF_ESS }, // 38 52xx MCU
	{ "53xx.bin",          0x0400, 0xb326fecb, 17 | BRF_PRG | BRF_ESS }, // 39 53xx MCU
	{ "54xx.bin",          0x0400, 0xee7357e0, 18 | BRF_PRG | BRF_ESS }, // 40 54xx MCU
};

STD_ROM_PICK(topracera)
STD_ROM_FN(topracera)

struct BurnDriver BurnDrvTopracera = {
	"topracera", "polepos", NULL, NULL, "1983",
	"Top Racer (with MB8841 + MB8842, 1983)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 1, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, topraceraRomInfo, topraceraRomName, NULL, NULL, NULL, NULL, PoleposInputInfo, PoleposDIPInfo,
	PoleposAInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x0f00,
	256, 224, 4, 3
};

// Speed Up (Spanish bootleg of Pole Position)

static struct BurnRomInfo ppspeedRomDesc[] = {
	{ "tr9b.bin",          0x2000, 0x538bd0cb, 1 | BRF_PRG | BRF_ESS  }, //  0 Z80 Code
	{ "tr10.bin",          0x1000, 0x7174bcb7, 1 | BRF_PRG | BRF_ESS  }, //  1 
	{ "tr1b.bin",          0x2000, 0x127f0750, 2 | BRF_PRG | BRF_ESS  }, //  2 Z8002 #1 Code (odd)
	{ "tr2b.bin",          0x2000, 0x6bd4ff6b, 2 | BRF_PRG | BRF_ESS  }, //  3 Z8002 #1 Code (even)
	{ "tr5b.bin",          0x2000, 0x4e5f7b9c, 3 | BRF_PRG | BRF_ESS  }, //  4 Z8002 #2 Code (odd)
	{ "tr6b.bin",          0x2000, 0xb3641d0c, 3 | BRF_PRG | BRF_ESS  }, //  5 Z8002 #2 Code (even)
	{ "tr28.bin",          0x1000, 0xcd80b4c3, 4 | BRF_GRA            }, //  6 Alpha Characters
	{ "tr29.bin",          0x1000, 0xc6e15c21, 5 | BRF_GRA            }, //  7 Background Tiles
	{ "tr25.bin",          0x2000, 0xf44d33c1, 6 | BRF_GRA            }, //  8 Small Sprites
	{ "tr26.bin",          0x2000, 0x87e8482d, 6 | BRF_GRA            }, //  9 
	{ "tr17.bin",          0x2000, 0x613ab0df, 7 | BRF_GRA            }, // 10 Big Sprites (planes 0+1)
	{ "tr19.bin",          0x2000, 0x1ea04ccd, 7 | BRF_GRA            }, // 11 
	{ "tr21.bin",          0x2000, 0xbd7b4a62, 7 | BRF_GRA            }, // 12 
	{ "tr18.bin",          0x2000, 0x5fd933e3, 8 | BRF_GRA            }, // 13 Big Sprites (planes 2+3)
	{ "tr20.bin",          0x2000, 0xc572c6ed, 8 | BRF_GRA            }, // 14 
	{ "tr22.bin",          0x2000, 0xdb1bcdd8, 8 | BRF_GRA            }, // 15 
	{ "tr30.bin",          0x2000, 0xee6b3315, 9 | BRF_GRA            }, // 16 Road Data
	{ "tr31.bin",          0x2000, 0x6d1e7042, 9 | BRF_GRA            }, // 17 
	{ "tr32.bin",          0x1000, 0x4e97f101, 9 | BRF_GRA            }, // 18 
	{ "tr27.bin",          0x1000, 0xa61bff15, 10 | BRF_GRA           }, // 19 Sprite Scaling LUT
	{ "tr15.bin",          0x2000, 0xb5ad4d5f, 13 | BRF_SND           }, // 20 Engine Sound
	{ "tr16.bin",          0x2000, 0x8fdd2f6f, 13 | BRF_SND           }, // 21 
	{ "7052-7.k21",        0x0100, 0xf07ff2ad, 11 | BRF_GRA           }, // 22 Color / Lookup PROMs
	{ "7052-8.k20",        0x0100, 0xadbde7d7, 11 | BRF_GRA           }, // 23 
	{ "7052-9.k19",        0x0100, 0xddac786a, 11 | BRF_GRA           }, // 24 
	{ "7052-10.h15",       0x0100, 0x5af3f710, 11 | BRF_GRA           }, // 25 
	{ "7052-11.j15",       0x0100, 0x8c90e36e, 11 | BRF_GRA           }, // 26 
	{ "7052-15.d1",        0x0100, 0x2d502464, 11 | BRF_GRA           }, // 27 
	{ "7052-16.d2",        0x0100, 0x027aa62c, 11 | BRF_GRA           }, // 28 
	{ "7052-17.d3",        0x0100, 0x1f8d0df3, 11 | BRF_GRA           }, // 29 
	{ "7122.a19",          0x0400, 0x7afc7cfc, 11 | BRF_GRA           }, // 30 
	{ "7122.e7",           0x0400, 0x2f1079ee, 11 | BRF_GRA           }, // 31 
	{ "7051-13.l7",        0x0020, 0x4330a51b, 11 | BRF_GRA           }, // 32 
	{ "7051-14.l8",        0x0020, 0x4330a51b, 11 | BRF_GRA           }, // 33 
	{ "7052-5.e9",         0x0100, 0x8568decc, 12 | BRF_SND           }, // 34 Namco Wave PROM
	{ "tr11.b1",           0x2000, 0x5b4cf05e, 14 | BRF_SND           }, // 35 52xx Samples
	{ "7052-4.c14",        0x0100, 0x0e742cb1, 0 | BRF_OPT            }, // 36 unused

	{ "51xx.bin",          0x0400, 0xc2f57ef8, 15 | BRF_PRG | BRF_ESS }, // 37 51xx MCU
	{ "52xx.bin",          0x0400, 0x3257d11e, 16 | BRF_PRG | BRF_ESS }, // 38 52xx MCU
	{ "53xx.bin",          0x0400, 0xb326fecb, 17 | BRF_PRG | BRF_ESS }, // 39 53xx MCU
	{ "54xx.bin",          0x0400, 0xee7357e0, 18 | BRF_PRG | BRF_ESS }, // 40 54xx MCU
};

STD_ROM_PICK(ppspeed)
STD_ROM_FN(ppspeed)

struct BurnDriver BurnDrvPpspeed = {
	"ppspeed", "polepos", NULL, NULL, "1983",
	"Speed Up (Spanish bootleg of Pole Position)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 1, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, ppspeedRomInfo, ppspeedRomName, NULL, NULL, NULL, NULL, PoleposInputInfo, PoleposDIPInfo,
	PoleposAInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x0f00,
	256, 224, 4, 3
};

// Pole Position II (Japan)

static struct BurnRomInfo polepos2RomDesc[] = {
	{ "pp4_9.6h",          0x2000, 0xbcf87004, 1 | BRF_PRG | BRF_ESS  }, //  0 Z80 Code
	{ "pp4_10.5h",         0x1000, 0xa9d4c380, 1 | BRF_PRG | BRF_ESS  }, //  1 
	{ "pp4_1.8m",          0x2000, 0x3f6ac294, 2 | BRF_PRG | BRF_ESS  }, //  2 Z8002 #1 Code (odd)
	{ "pp4_2.8l",          0x2000, 0x51b9a669, 2 | BRF_PRG | BRF_ESS  }, //  3 Z8002 #1 Code (even)
	{ "pp4_5.4m",          0x2000, 0xc3053cae, 3 | BRF_PRG | BRF_ESS  }, //  4 Z8002 #2 Code (odd)
	{ "pp4_6.4l",          0x2000, 0x38d04e0f, 3 | BRF_PRG | BRF_ESS  }, //  5 Z8002 #2 Code (even)
	{ "pp4_7.3m",          0x1000, 0xad1c8994, 3 | BRF_PRG | BRF_ESS  }, //  6 Z8002 #2 Code (odd)
	{ "pp4_8.3l",          0x1000, 0xef25a2ee, 3 | BRF_PRG | BRF_ESS  }, //  7 Z8002 #2 Code (even)
	{ "pp4_28.1f",         0x2000, 0x280dde7d, 4 | BRF_GRA            }, //  8 Alpha Characters
	{ "pp4_29.1e",         0x2000, 0xec3ec6e6, 5 | BRF_GRA            }, //  9 Background Tiles
	{ "pp4_25.1n",         0x2000, 0xfd098e65, 6 | BRF_GRA            }, // 10 Small Sprites
	{ "pp4_26.1m",         0x2000, 0x35ac62b3, 6 | BRF_GRA            }, // 11 
	{ "pp1_17.5n",         0x2000, 0x2e134b46, 7 | BRF_GRA            }, // 12 Big Sprites (planes 0+1)
	{ "pp1_19.4n",         0x2000, 0x43ff83e1, 7 | BRF_GRA            }, // 13 
	{ "pp1_21.3n",         0x2000, 0x5f958eb4, 7 | BRF_GRA            }, // 14 
	{ "pp4_23.2n",         0x2000, 0x9e056fcd, 7 | BRF_GRA            }, // 15 
	{ "pp1_18.5m",         0x2000, 0x6f9997d2, 8 | BRF_GRA            }, // 16 Big Sprites (planes 2+3)
	{ "pp1_20.4m",         0x2000, 0xec18075b, 8 | BRF_GRA            }, // 17 
	{ "pp1_22.3m",         0x2000, 0x1d2f30b1, 8 | BRF_GRA            }, // 18 
	{ "pp4_24.2m",         0x2000, 0x795268cf, 8 | BRF_GRA            }, // 19 
	{ "pp1_30.3a",         0x2000, 0xee6b3315, 9 | BRF_GRA            }, // 20 Road Data
	{ "pp1_31.2a",         0x2000, 0x6d1e7042, 9 | BRF_GRA            }, // 21 
	{ "pp1_32.1a",         0x1000, 0x4e97f101, 9 | BRF_GRA            }, // 22 
	{ "pp1_27.1l",         0x1000, 0xa61bff15, 10 | BRF_GRA           }, // 23 Sprite Scaling LUT
	{ "pp4-7.8l",          0x0100, 0x16d69c31, 11 | BRF_GRA           }, // 24 Color / Lookup PROMs
	{ "pp4-8.9l",          0x0100, 0x07340311, 11 | BRF_GRA           }, // 25 
	{ "pp4-9.10l",         0x0100, 0x1efc84d7, 11 | BRF_GRA           }, // 26 
	{ "pp4-10.2h",         0x0100, 0x064d51a0, 11 | BRF_GRA           }, // 27 
	{ "pp4-11.4d",         0x0100, 0x7880c5af, 11 | BRF_GRA           }, // 28 
	{ "pp1-15.9a",         0x0100, 0x2d502464, 11 | BRF_GRA           }, // 29 
	{ "pp1-16.10a",        0x0100, 0x027aa62c, 11 | BRF_GRA           }, // 30 
	{ "pp1-17.11a",        0x0100, 0x1f8d0df3, 11 | BRF_GRA           }, // 31 
	{ "pp4-12.3c",         0x0400, 0x8b270902, 11 | BRF_GRA           }, // 32 
	{ "pp4-6.6m",          0x0400, 0x647212b5, 11 | BRF_GRA           }, // 33 
	{ "pp1-13.8e",         0x0020, 0x4330a51b, 11 | BRF_GRA           }, // 34 
	{ "pp1-14.9e",         0x0020, 0x4330a51b, 11 | BRF_GRA           }, // 35 
	{ "pp1-5.3b",          0x0100, 0x8568decc, 12 | BRF_SND           }, // 36 Namco Wave PROM
	{ "pp4_15.6a",         0x2000, 0x7d93bc1c, 13 | BRF_SND           }, // 37 Engine Sound
	{ "pp4_16.5a",         0x2000, 0x7d93bc1c, 13 | BRF_SND           }, // 38 
	{ "pp1_11.2e",         0x2000, 0x45b9bfeb, 14 | BRF_SND           }, // 39 52xx Samples
	{ "pp1_13.1e",         0x2000, 0xa4237466, 14 | BRF_SND           }, // 40 
	{ "pp1_12.2f",         0x2000, 0xa31b4be5, 14 | BRF_SND           }, // 41 
	{ "pp1_14.1f",         0x2000, 0x944580f9, 14 | BRF_SND           }, // 42 
	{ "pp1-4.9h",          0x0100, 0x2401c817, 0 | BRF_OPT            }, // 43 unused

	{ "51xx.bin",          0x0400, 0xc2f57ef8, 15 | BRF_PRG | BRF_ESS }, // 44 51xx MCU
	{ "52xx.bin",          0x0400, 0x3257d11e, 16 | BRF_PRG | BRF_ESS }, // 45 52xx MCU
	{ "53xx.bin",          0x0400, 0xb326fecb, 17 | BRF_PRG | BRF_ESS }, // 46 53xx MCU
	{ "54xx.bin",          0x0400, 0xee7357e0, 18 | BRF_PRG | BRF_ESS }, // 47 54xx MCU
};

STD_ROM_PICK(polepos2)
STD_ROM_FN(polepos2)

struct BurnDriver BurnDrvPolepos2 = {
	"polepos2", NULL, NULL, NULL, "1983",
	"Pole Position II (Japan)\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, polepos2RomInfo, polepos2RomName, NULL, NULL, NULL, NULL, PoleposInputInfo, Polepos2jDIPInfo,
	Polepos2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x0f00,
	256, 224, 4, 3
};

// Pole Position II (Atari)

static struct BurnRomInfo polepos2aRomDesc[] = {
	{ "136014-180.7h",     0x2000, 0xf85212c4, 1 | BRF_PRG | BRF_ESS  }, //  0 Z80 Code
	{ "136014-183.7f",     0x1000, 0xa9d4c380, 1 | BRF_PRG | BRF_ESS  }, //  1 
	{ "136014-176.3l",     0x2000, 0x8aeaec98, 2 | BRF_PRG | BRF_ESS  }, //  2 Z8002 #1 Code (odd)
	{ "136014-177.4l",     0x2000, 0x7051df35, 2 | BRF_PRG | BRF_ESS  }, //  3 Z8002 #1 Code (even)
	{ "136014-178.3e",     0x2000, 0xeac35cfa, 3 | BRF_PRG | BRF_ESS  }, //  4 Z8002 #2 Code (odd)
	{ "136014-179.4e",     0x2000, 0x613e917d, 3 | BRF_PRG | BRF_ESS  }, //  5 Z8002 #2 Code (even)
	{ "136014-184.3d",     0x2000, 0xd893c4ed, 3 | BRF_PRG | BRF_ESS  }, //  6 Z8002 #2 Code (odd)
	{ "136014-185.4d",     0x2000, 0x899de75e, 3 | BRF_PRG | BRF_ESS  }, //  7 Z8002 #2 Code (even)
	{ "136014-172.7n",     0x2000, 0xfbe5e72f, 4 | BRF_GRA            }, //  8 Alpha Characters
	{ "136014-173.6n",     0x2000, 0xec3ec6e6, 5 | BRF_GRA            }, //  9 Background Tiles
	{ "136014-170.13n",    0x2000, 0x455d79a0, 6 | BRF_GRA            }, // 10 Small Sprites
	{ "136014-171.12n",    0x2000, 0x78372b81, 6 | BRF_GRA            }, // 11 
	{ "136014-119.13j",    0x2000, 0x2e134b46, 7 | BRF_GRA            }, // 12 Big Sprites (planes 0+1)
	{ "136014-166.13k",    0x2000, 0x2b0517bd, 7 | BRF_GRA            }, // 13 
	{ "136014-168.13l",    0x2000, 0x4d7916d9, 7 | BRF_GRA            }, // 14 
	{ "136014-175.13m",    0x2000, 0xbd6df480, 7 | BRF_GRA            }, // 15 
	{ "136014-120.12j",    0x2000, 0x6f9997d2, 8 | BRF_GRA            }, // 16 Big Sprites (planes 2+3)
	{ "136014-167.12k",    0x2000, 0x411e21b5, 8 | BRF_GRA            }, // 17 
	{ "136014-169.12l",    0x2000, 0x662ff24b, 8 | BRF_GRA            }, // 18 
	{ "136014-174.12m",    0x2000, 0xf0c571dc, 8 | BRF_GRA            }, // 19 
	{ "136014-127.2l",     0x2000, 0xee6b3315, 9 | BRF_GRA            }, // 20 Road Data
	{ "136014-128.2m",     0x2000, 0x6d1e7042, 9 | BRF_GRA            }, // 21 
	{ "136014-134.2n",     0x1000, 0x4e97f101, 9 | BRF_GRA            }, // 22 
	{ "136014-231.11n",    0x1000, 0xa61bff15, 10 | BRF_GRA           }, // 23 Sprite Scaling LUT
	{ "136014-186.11e",    0x0100, 0x16d69c31, 11 | BRF_GRA           }, // 24 Color / Lookup PROMs
	{ "136014-187.11d",    0x0100, 0x07340311, 11 | BRF_GRA           }, // 25 
	{ "136014-188.11c",    0x0100, 0x1efc84d7, 11 | BRF_GRA           }, // 26 
	{ "136014-189.8m",     0x0100, 0x064d51a0, 11 | BRF_GRA           }, // 27 
	{ "136014-190.5k",     0x0100, 0x7880c5af, 11 | BRF_GRA           }, // 28 
	{ "136014-142.2d",     0x0100, 0x2d502464, 11 | BRF_GRA           }, // 29 
	{ "136014-143.2c",     0x0100, 0x027aa62c, 11 | BRF_GRA           }, // 30 
	{ "136014-144.2b",     0x0100, 0x1f8d0df3, 11 | BRF_GRA           }, // 31 
	{ "136014-191.4l",     0x0400, 0x8b270902, 11 | BRF_GRA           }, // 32 
	{ "136014-192.12h",    0x0400, 0xcaddb0b0, 11 | BRF_GRA           }, // 33 
	{ "136014-135.6e",     0x0020, 0x4330a51b, 11 | BRF_GRA           }, // 34 
	{ "136014-136.6d",     0x0020, 0x4330a51b, 11 | BRF_GRA           }, // 35 
	{ "136014-118.11d",    0x0100, 0x8568decc, 12 | BRF_SND           }, // 36 Namco Wave PROM
	{ "136014-181.12f",    0x2000, 0x7d93bc1c, 13 | BRF_SND           }, // 37 Engine Sound
	{ "136014-182.12e",    0x2000, 0x7d93bc1c, 13 | BRF_SND           }, // 38 
	{ "136014-106.9c",     0x2000, 0x5b4cf05e, 14 | BRF_SND           }, // 39 52xx Samples
	{ "136014-117.7l",     0x0100, 0x2401c817, 0 | BRF_OPT            }, // 40 unused
	{ "137316-001.2n",     0x0034, 0xdd37bd15, 0 | BRF_OPT            }, // 41 
	{ "137316-001.5c",     0x0034, 0xdd37bd15, 0 | BRF_OPT            }, // 42 
	{ "137279-001.7c",     0x00eb, 0x4ddc51ba, 0 | BRF_OPT            }, // 43 

	{ "51xx.bin",          0x0400, 0xc2f57ef8, 15 | BRF_PRG | BRF_ESS }, // 44 51xx MCU
	{ "52xx.bin",          0x0400, 0x3257d11e, 16 | BRF_PRG | BRF_ESS }, // 45 52xx MCU
	{ "53xx.bin",          0x0400, 0xb326fecb, 17 | BRF_PRG | BRF_ESS }, // 46 53xx MCU
	{ "54xx.bin",          0x0400, 0xee7357e0, 18 | BRF_PRG | BRF_ESS }, // 47 54xx MCU
};

STD_ROM_PICK(polepos2a)
STD_ROM_FN(polepos2a)

struct BurnDriver BurnDrvPolepos2a = {
	"polepos2a", "polepos2", NULL, NULL, "1983",
	"Pole Position II (Atari)\0", NULL, "Namco (Atari license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, polepos2aRomInfo, polepos2aRomName, NULL, NULL, NULL, NULL, PoleposInputInfo, Polepos2DIPInfo,
	Polepos2aInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x0f00,
	256, 224, 4, 3
};

// Pole Position II (bootleg)

static struct BurnRomInfo polepos2bRomDesc[] = {
	{ "136014.180",        0x2000, 0xf85212c4, 1 | BRF_PRG | BRF_ESS  }, //  0 Z80 Code
	{ "136014.183",        0x1000, 0xa9d4c380, 1 | BRF_PRG | BRF_ESS  }, //  1 
	{ "3lcpu.rom",         0x2000, 0xcf95a6b7, 2 | BRF_PRG | BRF_ESS  }, //  2 Z8002 #1 Code (odd)
	{ "4lcpu.rom",         0x2000, 0x643483f7, 2 | BRF_PRG | BRF_ESS  }, //  3 Z8002 #1 Code (even)
	{ "cpu-4k.rom",        0x1000, 0x97a496b3, 19 | BRF_PRG | BRF_ESS }, //  4 Z8002 #1 Code (even only)
	{ "136014.178",        0x2000, 0xeac35cfa, 3 | BRF_PRG | BRF_ESS  }, //  5 Z8002 #2 Code (odd)
	{ "136014.179",        0x2000, 0x613e917d, 3 | BRF_PRG | BRF_ESS  }, //  6 Z8002 #2 Code (even)
	{ "136014.184",        0x2000, 0xd893c4ed, 3 | BRF_PRG | BRF_ESS  }, //  7 Z8002 #2 Code (odd)
	{ "136014.185",        0x2000, 0x899de75e, 3 | BRF_PRG | BRF_ESS  }, //  8 Z8002 #2 Code (even)
	{ "136014.172",        0x2000, 0xfbe5e72f, 4 | BRF_GRA            }, //  9 Alpha Characters
	{ "136014.173",        0x2000, 0xec3ec6e6, 5 | BRF_GRA            }, // 10 Background Tiles
	{ "136014.170",        0x2000, 0x455d79a0, 6 | BRF_GRA            }, // 11 Small Sprites
	{ "136014.171",        0x2000, 0x78372b81, 6 | BRF_GRA            }, // 12 
	{ "136014.119",        0x2000, 0x2e134b46, 7 | BRF_GRA            }, // 13 Big Sprites (planes 0+1)
	{ "136014.166",        0x2000, 0x2b0517bd, 7 | BRF_GRA            }, // 14 
	{ "136014.168",        0x2000, 0x4d7916d9, 7 | BRF_GRA            }, // 15 
	{ "136014.175",        0x2000, 0xbd6df480, 7 | BRF_GRA            }, // 16 
	{ "136014.120",        0x2000, 0x6f9997d2, 8 | BRF_GRA            }, // 17 Big Sprites (planes 2+3)
	{ "136014.167",        0x2000, 0x411e21b5, 8 | BRF_GRA            }, // 18 
	{ "136014.169",        0x2000, 0x662ff24b, 8 | BRF_GRA            }, // 19 
	{ "136014.174",        0x2000, 0xf0c571dc, 8 | BRF_GRA            }, // 20 
	{ "136014.127",        0x2000, 0xee6b3315, 9 | BRF_GRA            }, // 21 Road Data
	{ "136014.128",        0x2000, 0x6d1e7042, 9 | BRF_GRA            }, // 22 
	{ "136014.134",        0x1000, 0x4e97f101, 9 | BRF_GRA            }, // 23 
	{ "136014.231",        0x1000, 0xa61bff15, 10 | BRF_GRA           }, // 24 Sprite Scaling LUT
	{ "136014.186",        0x0100, 0x16d69c31, 11 | BRF_GRA           }, // 25 Color / Lookup PROMs
	{ "136014.187",        0x0100, 0x07340311, 11 | BRF_GRA           }, // 26 
	{ "136014.188",        0x0100, 0x1efc84d7, 11 | BRF_GRA           }, // 27 
	{ "136014.189",        0x0100, 0x064d51a0, 11 | BRF_GRA           }, // 28 
	{ "136014.190",        0x0100, 0x7880c5af, 11 | BRF_GRA           }, // 29 
	{ "136014.142",        0x0100, 0x2d502464, 11 | BRF_GRA           }, // 30 
	{ "136014.143",        0x0100, 0x027aa62c, 11 | BRF_GRA           }, // 31 
	{ "136014.144",        0x0100, 0x1f8d0df3, 11 | BRF_GRA           }, // 32 
	{ "136014.191",        0x0400, 0x8b270902, 11 | BRF_GRA           }, // 33 
	{ "136014.192",        0x0400, 0xcaddb0b0, 11 | BRF_GRA           }, // 34 
	{ "136014.135",        0x0020, 0x4330a51b, 11 | BRF_GRA           }, // 35 
	{ "136014.136",        0x0020, 0x4330a51b, 11 | BRF_GRA           }, // 36 
	{ "136014.118",        0x0100, 0x8568decc, 12 | BRF_SND           }, // 37 Namco Wave PROM
	{ "136014.181",        0x2000, 0x7d93bc1c, 13 | BRF_SND           }, // 38 Engine Sound
	{ "136014.182",        0x2000, 0x7d93bc1c, 13 | BRF_SND           }, // 39 
	{ "136014.106",        0x2000, 0x5b4cf05e, 14 | BRF_SND           }, // 40 52xx Samples
	{ "136014.117",        0x0100, 0x2401c817, 0 | BRF_OPT            }, // 41 unused

	{ "51xx.bin",          0x0400, 0xc2f57ef8, 15 | BRF_PRG | BRF_ESS }, // 42 51xx MCU
	{ "52xx.bin",          0x0400, 0x3257d11e, 16 | BRF_PRG | BRF_ESS }, // 43 52xx MCU
	{ "53xx.bin",          0x0400, 0xb326fecb, 17 | BRF_PRG | BRF_ESS }, // 44 53xx MCU
	{ "54xx.bin",          0x0400, 0xee7357e0, 18 | BRF_PRG | BRF_ESS }, // 45 54xx MCU
};

STD_ROM_PICK(polepos2b)
STD_ROM_FN(polepos2b)

struct BurnDriver BurnDrvPolepos2b = {
	"polepos2b", "polepos2", NULL, NULL, "1983",
	"Pole Position II (bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 1, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, polepos2bRomInfo, polepos2bRomName, NULL, NULL, NULL, NULL, PoleposInputInfo, Polepos2DIPInfo,
	Polepos2bInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x0f00,
	256, 224, 4, 3
};
