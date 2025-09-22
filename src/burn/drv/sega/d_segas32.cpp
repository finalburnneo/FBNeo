// FinalBurn Neo Sega System 32 driver module
// Based on MAME driver by Aaron Giles

/*
 :exceptions:
darkedge	- background behind clouds (same in MAME)
titlef	 (m)- broken background layer! (same in MAME)
as1			- (not worth adding)

 :note:
slipstrm	- gets hung booting if irq changes during RMW operation (in handler)
              solution:	delay to end of opcode.
*/

//#define LOG_RW

#include "tiles_generic.h"
#include "v60_intf.h"
#include "nec_intf.h"
#include "z80_intf.h"
#include "eeprom.h"
#include "burn_ym2612.h"
#include "rf5c68.h"
#include "multipcm.h"
#include "burn_pal.h"
#include "bitswap.h"
#include "burn_gun.h"
#include "burn_shift.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvV60ROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvV25ROM;
static UINT8 *DrvGfxROM[2];
static UINT8 *DrvPCMROM;
static UINT8 *DrvEEPROM;
static UINT8 *DrvV60RAM;
static UINT8 *DrvPalRAM[2];
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT32 *DrvSprRAM32;
static UINT8 *DrvShareRAM;
static UINT8 *DrvCommsRAM;
static UINT8 *DrvV25RAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 nExtraCycles[2];

static INT32 m_prev_bgstartx[512];
static INT32 m_prev_bgendx[512];
static INT32 m_bgcolor_line[512];

static UINT16 solid_ffff[512*2];
static UINT16 solid_0000[512*2];

static INT32 timer_0_cycles;
static INT32 timer_1_cycles;

#define TIMER_0_CLOCK		((((is_multi32) ? 40000000 : 32215900) / 2) / 2048)
#define TIMER_1_CLOCK		((50000000 / 16) / 256)

#define MAIN_IRQ_VBSTART	0
#define MAIN_IRQ_VBSTOP		1
#define MAIN_IRQ_SOUND		2
#define MAIN_IRQ_TIMER0		3
#define MAIN_IRQ_TIMER1		4
#define SOUND_IRQ_YM3438	0
#define SOUND_IRQ_V60		1

#define sign_extend(f, frombits) ((INT32)((f) << (32 - (frombits))) >> (32 - (frombits)))

static UINT8 transparent_check[32][256];

struct extents_list
{
	UINT8	scan_extent[256];
	UINT16	extent[32][16];
};

static UINT16 mixer_control[2][0x40];
static UINT8 sprite_control[8];
static UINT8 sprite_control_latched[8];
static UINT8 sprite_render_count;
static UINT8 v60_irq_control[0x10];
static INT32 v60_irq_vector = 0;

static UINT16 analog_value[4];
static INT32 analog_bank;

static UINT8 sound_irq_control[0x10];
static UINT8 sound_irq_input;
static UINT8 sound_dummy_data;
static INT32 sound_bank;
static UINT8 pcm_bankdata;

static UINT16 misc_io_data[2][16];
static void (*custom_io_write_0)(UINT32 offset, UINT16 data, UINT16 mem_mask) = NULL;
static UINT16 (*custom_io_read_0)(UINT32 offset) = NULL;

static INT32 system32_tilebank_external;
static UINT16 system32_displayenable[2];

static UINT32 (*memory_protection_read)(UINT32 offset, UINT32 mem_mask) = NULL;
static void (*memory_protection_write)(UINT32 offset, UINT32 data, UINT32 mem_mask) = NULL;
static UINT16 (*protection_a00000_read)(UINT32 offset) = NULL;
static void (*protection_a00000_write)(UINT32 offset, UINT32 data, UINT32 mem_mask) = NULL;

static void (*system32_prot_vblank)() = NULL;

static INT32 sprite_length; // 0 - fill in w/romsize, !0 fixed romsize -dink
static INT32 graphics_length[2];
static INT32 use_v25 = 0;
static INT32 screen_vflip = 0;
static INT32 is_multi32 = 0;
static INT32 is_scross = 0;
static INT32 is_radm = 0;
static INT32 is_radr = 0;
static INT32 is_sonic = 0;
static INT32 is_slipstrm = 0;
static INT32 is_ga2_spidman = 0;

static INT32 can_modechange = 0;
static INT32 fake_wide_screen = 0;

struct cache_entry
{
	cache_entry	*	next;
	INT32			tmap;
	UINT8			page;
	UINT8			bank;
	UINT8			dirty;
};

static cache_entry tmap_cache[32];
static cache_entry *cache_head;
static cache_entry *tilemap_cache;
static UINT16 *bitmaps[32];

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvJoy4[16];
static UINT8 DrvJoy5[16];
static UINT8 DrvJoy6[16];
static UINT8 DrvJoy7[16];
static UINT8 DrvJoy8[16];
static UINT8 DrvJoy9[16];
static UINT8 DrvJoy10[16];
static UINT8 DrvJoy11[16];
static UINT8 DrvJoy12[16];
static UINT8 DrvJoy13[16];
static UINT8 DrvJoy14[16];
static UINT8 DrvJoy15[16];
static UINT8 DrvJoy16[16];
static UINT8 DrvJoyX1[16];
static UINT8 DrvJoyX2[16];
static UINT8 DrvJoyX3[16];
static UINT8 DrvJoyX4[16];
static UINT8 DrvJoyF[16]; // fake inputs
static UINT16 DrvInputs[16];
static UINT16 DrvExtra[4];
static UINT8 DrvDips[2];
static UINT8 DrvReset;
static INT16 Analog[6];
static UINT8 sonic_delta[6];

static INT32 Radm_analog_adder = 0;
static INT32 Radm_analog_target = 0;

// forwards
static void RadmAnalogTick();
static INT32 MultiScreenCheck();
static INT32 SingleScreenModeChangeCheck();

static INT32 has_gun = 0;
static INT32 clr_opposites = 0;
static INT32 opaquey_hack = 0;

static struct BurnInputInfo ArabfgtInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy5 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy5 + 4,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy5 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy5 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 2"	},

	{"P3 Start",		BIT_DIGITAL,	DrvJoyX3 + 0,	"p3 start"	},
	{"P3 Up",			BIT_DIGITAL,	DrvJoyX1 + 5,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	DrvJoyX1 + 4,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	DrvJoyX1 + 7,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoyX1 + 6,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoyX1 + 0,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoyX1 + 1,	"p3 fire 2"	},

	{"P4 Start",		BIT_DIGITAL,	DrvJoyX3 + 1,	"p4 start"	},
	{"P4 Up",			BIT_DIGITAL,	DrvJoyX2 + 5,	"p4 up"		},
	{"P4 Down",			BIT_DIGITAL,	DrvJoyX2 + 4,	"p4 down"	},
	{"P4 Left",			BIT_DIGITAL,	DrvJoyX2 + 7,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoyX2 + 6,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoyX2 + 0,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoyX2 + 1,	"p4 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy5 + 1,	"diag"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy5 + 0,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy6 + 4,	"service"	},
	{"Service 3",		BIT_DIGITAL,	DrvJoy6 + 5,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Arabfgt)

static struct BurnInputInfo ArabfgtuInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoyX3 + 3,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy5 + 4,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoyX3 + 2,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy5 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 2"	},

	{"P3 Coin",			BIT_DIGITAL,	DrvJoy5 + 3,	"p3 coin"	},
	{"P3 Start",		BIT_DIGITAL,	DrvJoyX3 + 0,	"p3 start"	},
	{"P3 Up",			BIT_DIGITAL,	DrvJoyX1 + 5,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	DrvJoyX1 + 4,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	DrvJoyX1 + 7,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoyX1 + 6,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoyX1 + 0,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoyX1 + 1,	"p3 fire 2"	},

	{"P4 Coin",			BIT_DIGITAL,	DrvJoy5 + 2,	"p4 coin"	},
	{"P4 Start",		BIT_DIGITAL,	DrvJoyX3 + 1,	"p4 start"	},
	{"P4 Up",			BIT_DIGITAL,	DrvJoyX2 + 5,	"p4 up"		},
	{"P4 Down",			BIT_DIGITAL,	DrvJoyX2 + 4,	"p4 down"	},
	{"P4 Left",			BIT_DIGITAL,	DrvJoyX2 + 7,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoyX2 + 6,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoyX2 + 0,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoyX2 + 1,	"p4 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy5 + 1,	"diag"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy5 + 0,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy6 + 4,	"service"	},
	{"Service 3",		BIT_DIGITAL,	DrvJoy6 + 5,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Arabfgtu)

static struct BurnInputInfo BrivalInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy5 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy5 + 4,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 right"	},
	{"P1 Weak Punch",	BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Medium Punch",	BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},
	{"P1 Strong Punch",	BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 3"	},
	{"P1 Weak Kick",	BIT_DIGITAL,	DrvJoyX2 + 5,	"p1 fire 4"	},
	{"P1 Medium Kick",	BIT_DIGITAL,	DrvJoyX2 + 4,	"p1 fire 5"	},
	{"P1 Strong Kick",	BIT_DIGITAL,	DrvJoyX2 + 7,	"p1 fire 6"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy5 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy5 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 right"	},
	{"P2 Weak Punch",	BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 1"	},
	{"P2 Medium Punch",	BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 2"	},
	{"P2 Strong Punch",	BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 3"	},
	{"P2 Weak Kick",	BIT_DIGITAL,	DrvJoyX2 + 0,	"p2 fire 4"	},
	{"P2 Medium Kick",	BIT_DIGITAL,	DrvJoyX2 + 1,	"p2 fire 5"	},
	{"P2 Strong Kick",	BIT_DIGITAL,	DrvJoyX2 + 2,	"p2 fire 6"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy5 + 1,	"diag"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy5 + 0,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy6 + 4,	"service"	},
	{"Service 3",		BIT_DIGITAL,	DrvJoy6 + 5,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Brival)

static struct BurnInputInfo Ga2InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy5 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy5 + 4,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy5 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy5 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 3"	},

	{"P3 Coin",			BIT_DIGITAL,	DrvJoyF + 2,	"p3 coin"	},
	{"P3 Start",		BIT_DIGITAL,	DrvJoyX3 + 0,	"p3 start"	},
	{"P3 Up",			BIT_DIGITAL,	DrvJoyX1 + 5,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	DrvJoyX1 + 4,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	DrvJoyX1 + 7,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoyX1 + 6,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoyX1 + 0,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoyX1 + 1,	"p3 fire 2"	},
	{"P3 Button 3",		BIT_DIGITAL,	DrvJoyX1 + 2,	"p3 fire 3"	},

	{"P4 Coin",			BIT_DIGITAL,	DrvJoyF + 3,	"p4 coin"	},
	{"P4 Start",		BIT_DIGITAL,	DrvJoyX3 + 1,	"p4 start"	},
	{"P4 Up",			BIT_DIGITAL,	DrvJoyX2 + 5,	"p4 up"		},
	{"P4 Down",			BIT_DIGITAL,	DrvJoyX2 + 4,	"p4 down"	},
	{"P4 Left",			BIT_DIGITAL,	DrvJoyX2 + 7,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoyX2 + 6,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoyX2 + 0,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoyX2 + 1,	"p4 fire 2"	},
	{"P4 Button 3",		BIT_DIGITAL,	DrvJoyX2 + 2,	"p4 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy5 + 1,	"diag"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy5 + 0,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy6 + 4,	"service"	},
	{"Service 3",		BIT_DIGITAL,	DrvJoy6 + 5,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Ga2)

static struct BurnInputInfo Ga2uInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoyX3 + 3,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy5 + 4,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoyX3 + 2,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy5 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 3"	},

	{"P3 Coin",			BIT_DIGITAL,	DrvJoy5 + 3,	"p3 coin"	},
	{"P3 Start",		BIT_DIGITAL,	DrvJoyX3 + 0,	"p3 start"	},
	{"P3 Up",			BIT_DIGITAL,	DrvJoyX1 + 5,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	DrvJoyX1 + 4,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	DrvJoyX1 + 7,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoyX1 + 6,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoyX1 + 0,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoyX1 + 1,	"p3 fire 2"	},
	{"P3 Button 3",		BIT_DIGITAL,	DrvJoyX1 + 2,	"p3 fire 3"	},

	{"P4 Coin",			BIT_DIGITAL,	DrvJoy5 + 2,	"p4 coin"	},
	{"P4 Start",		BIT_DIGITAL,	DrvJoyX3 + 1,	"p4 start"	},
	{"P4 Up",			BIT_DIGITAL,	DrvJoyX2 + 5,	"p4 up"		},
	{"P4 Down",			BIT_DIGITAL,	DrvJoyX2 + 4,	"p4 down"	},
	{"P4 Left",			BIT_DIGITAL,	DrvJoyX2 + 7,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoyX2 + 6,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoyX2 + 0,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoyX2 + 1,	"p4 fire 2"	},
	{"P4 Button 3",		BIT_DIGITAL,	DrvJoyX2 + 2,	"p4 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy5 + 1,	"diag"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy5 + 0,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy6 + 4,	"service"	},
	{"Service 3",		BIT_DIGITAL,	DrvJoy6 + 5,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Ga2u)

static struct BurnInputInfo DarkedgeInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy5 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy5 + 4,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoyX2 + 5,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoyX2 + 4,	"p1 fire 4"	},
	{"P1 Button 5",		BIT_DIGITAL,	DrvJoyX2 + 7,	"p1 fire 5"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy5 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy5 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoyX2 + 0,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoyX2 + 1,	"p2 fire 4"	},
	{"P2 Button 5",		BIT_DIGITAL,	DrvJoyX2 + 2,	"p2 fire 5"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy5 + 1,	"diag"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy5 + 0,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy6 + 4,	"service"	},
	{"Service 3",		BIT_DIGITAL,	DrvJoy6 + 5,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Darkedge)

static struct BurnInputInfo SpidmanInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy5 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy5 + 4,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy5 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy5 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 2"	},

	{"P3 Coin",			BIT_DIGITAL,	DrvJoyF + 2,	"p3 coin"	},
	{"P3 Start",		BIT_DIGITAL,	DrvJoyX3 + 0,	"p3 start"	},
	{"P3 Up",			BIT_DIGITAL,	DrvJoyX1 + 5,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	DrvJoyX1 + 4,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	DrvJoyX1 + 7,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoyX1 + 6,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoyX1 + 0,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoyX1 + 1,	"p3 fire 2"	},

	{"P4 Coin",			BIT_DIGITAL,	DrvJoyF + 3,	"p4 coin"	},
	{"P4 Start",		BIT_DIGITAL,	DrvJoyX3 + 1,	"p4 start"	},
	{"P4 Up",			BIT_DIGITAL,	DrvJoyX2 + 5,	"p4 up"		},
	{"P4 Down",			BIT_DIGITAL,	DrvJoyX2 + 4,	"p4 down"	},
	{"P4 Left",			BIT_DIGITAL,	DrvJoyX2 + 7,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoyX2 + 6,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoyX2 + 0,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoyX2 + 1,	"p4 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy5 + 1,	"diag"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy5 + 0,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy6 + 4,	"service"	},
	{"Service 3",		BIT_DIGITAL,	DrvJoy6 + 5,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Spidman)

static struct BurnInputInfo SpidmanuInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoyX3 + 3,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy5 + 4,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoyX3 + 2,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy5 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 2"	},

	{"P3 Coin",			BIT_DIGITAL,	DrvJoy5 + 3,	"p3 coin"	},
	{"P3 Start",		BIT_DIGITAL,	DrvJoyX3 + 0,	"p3 start"	},
	{"P3 Up",			BIT_DIGITAL,	DrvJoyX1 + 5,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	DrvJoyX1 + 4,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	DrvJoyX1 + 7,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoyX1 + 6,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoyX1 + 0,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoyX1 + 1,	"p3 fire 2"	},

	{"P4 Coin",			BIT_DIGITAL,	DrvJoy5 + 2,	"p4 coin"	},
	{"P4 Start",		BIT_DIGITAL,	DrvJoyX3 + 1,	"p4 start"	},
	{"P4 Up",			BIT_DIGITAL,	DrvJoyX2 + 5,	"p4 up"		},
	{"P4 Down",			BIT_DIGITAL,	DrvJoyX2 + 4,	"p4 down"	},
	{"P4 Left",			BIT_DIGITAL,	DrvJoyX2 + 7,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoyX2 + 6,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoyX2 + 0,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoyX2 + 1,	"p4 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy5 + 1,	"diag"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy5 + 0,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy6 + 4,	"service"	},
	{"Service 3",		BIT_DIGITAL,	DrvJoy6 + 5,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Spidmanu)

static struct BurnInputInfo DbzvrvsInputList[] = { // & svf/jleague
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy5 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy5 + 4,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy5 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy5 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy5 + 1,	"diag"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy5 + 0,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy6 + 4,	"service"	},
	{"Service 3",		BIT_DIGITAL,	DrvJoy6 + 5,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Dbzvrvs)

static struct BurnInputInfo HoloInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy5 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy5 + 4,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy5 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy5 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy5 + 1,	"diag"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy5 + 0,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy6 + 4,	"service"	},
	{"Service 3",		BIT_DIGITAL,	DrvJoy6 + 5,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Holo)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo ArescueInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy5 + 2,	"p1 coin"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},
	A("P1 Left / Right",BIT_ANALOG_REL, &Analog[0],		"p1 x-axis"	),
	A("P1 Up / Down",	BIT_ANALOG_REL, &Analog[1],		"p1 y-axis"	),
	A("P1 Throttle",	BIT_ANALOG_REL, &Analog[2],		"p1 z-axis"	),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy5 + 1,	"diag"		},
	{"Service",			BIT_DIGITAL,	DrvJoy5 + 0,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Arescue)

static struct BurnInputInfo JparkInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy5 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy5 + 4,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	A("P1 Gun X",    	BIT_ANALOG_REL, &Analog[0],    	"mouse x-axis" ),
	A("P1 Gun Y",    	BIT_ANALOG_REL, &Analog[1],    	"mouse y-axis" ),

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy5 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy5 + 5,	"p2 start"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 1"	},
	A("P2 Gun X",    	BIT_ANALOG_REL, &Analog[2],    	"p2 x-axis" ),
	A("P2 Gun Y",    	BIT_ANALOG_REL, &Analog[3],    	"p2 y-axis" ),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy5 + 1,	"diag"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy5 + 0,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy6 + 4,	"service"	},
	{"Service 3",		BIT_DIGITAL,	DrvJoy6 + 5,	"service"	},
	{"Tilt",			BIT_DIGITAL,	DrvJoy3 + 3,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Jpark)

static struct BurnInputInfo Alien3InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy5 + 3,	"p1 coin"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},
	A("P1 Gun X",    	BIT_ANALOG_REL, &Analog[0],    	"mouse x-axis" ),
	A("P1 Gun Y",    	BIT_ANALOG_REL, &Analog[1],    	"mouse y-axis" ),

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy5 + 2,	"p2 coin"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 2"	},
	A("P2 Gun X",    	BIT_ANALOG_REL, &Analog[2],    	"p2 x-axis" ),
	A("P2 Gun Y",    	BIT_ANALOG_REL, &Analog[3],    	"p2 y-axis" ),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy5 + 1,	"diag"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy5 + 0,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy6 + 4,	"service"	},
	{"Service 3",		BIT_DIGITAL,	DrvJoy6 + 5,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Alien3)

static struct BurnInputInfo F1enInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy5 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy5 + 4,	"p1 start"	},
	{"P1 Shift Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Shift Down",	BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},
	A("P1 Steering",	BIT_ANALOG_REL, &Analog[0],		"p1 x-axis"	),
	A("P1 Accelerate",	BIT_ANALOG_REL, &Analog[1],		"p1 y-axis"	),
	A("P1 Brake",		BIT_ANALOG_REL, &Analog[2],		"p1 z-axis"	),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy5 + 1,	"diag"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy5 + 0,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy6 + 4,	"service"	},
	{"Service 3",		BIT_DIGITAL,	DrvJoy6 + 5,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(F1en)

static struct BurnInputInfo F1lapInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy5 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy5 + 4,	"p1 start"	},
	{"P1 Shift Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Shift Down",	BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},
	{"P1 Overtake",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 3"	},
	A("P1 Steering",	BIT_ANALOG_REL, &Analog[0],		"p1 x-axis"	),
	A("P1 Accelerate",	BIT_ANALOG_REL, &Analog[1],		"p1 y-axis"	),
	A("P1 Brake",		BIT_ANALOG_REL, &Analog[2],		"p1 z-axis"	),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy5 + 1,	"diag"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy5 + 0,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy6 + 4,	"service"	},
	{"Service 3",		BIT_DIGITAL,	DrvJoy6 + 5,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(F1lap)

static struct BurnInputInfo Kokoroj2InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy5 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy5 + 4,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 4"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy5 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy5 + 5,	"p2 start"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 fire 4"	},

	{"P3 Start",		BIT_DIGITAL,	DrvJoy5 + 6,	"p3 start"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p3 fire 2"	},
	{"P3 Button 3",		BIT_DIGITAL,	DrvJoy2 + 2,	"p3 fire 3"	},
	{"P3 Button 4",		BIT_DIGITAL,	DrvJoy2 + 3,	"p3 fire 4"	},

	{"P4 Start",		BIT_DIGITAL,	DrvJoy5 + 7,	"p4 start"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p4 fire 2"	},
	{"P4 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p4 fire 3"	},
	{"P4 Button 4",		BIT_DIGITAL,	DrvJoy2 + 7,	"p4 fire 4"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy5 + 1,	"diag"		},
	{"Service",			BIT_DIGITAL,	DrvJoy5 + 0,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Kokoroj2)

static struct BurnInputInfo RadmInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy5 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy5 + 4,	"p1 start"	},
	{"P1 Headlights",	BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 1"	},
	{"P1 Wipers",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"	},
	A("P1 Steering",	BIT_ANALOG_REL, &Analog[0],		"p1 x-axis"	),
	A("P1 Accelerate",	BIT_ANALOG_REL, &Analog[1],		"p1 y-axis"	),
	A("P1 Brake",		BIT_ANALOG_REL, &Analog[2],		"p1 z-axis"	),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy5 + 1,	"diag"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy5 + 0,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy6 + 4,	"service"	},
	{"Service 3",		BIT_DIGITAL,	DrvJoy6 + 5,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Radm)

static struct BurnInputInfo RadrInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy5 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy5 + 4,	"p1 start"	},
	{"P1 Shift",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	A("P1 Steering",	BIT_ANALOG_REL, &Analog[0],		"p1 x-axis"	),
	A("P1 Accelerate",	BIT_ANALOG_REL, &Analog[1],		"p1 y-axis"	),
	A("P1 Brake",		BIT_ANALOG_REL, &Analog[2],		"p1 z-axis"	),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy5 + 1,	"service"	},
	{"Service 1",		BIT_DIGITAL,	DrvJoy5 + 0,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy6 + 4,	"service"	},
	{"Service 3",		BIT_DIGITAL,	DrvJoy6 + 5,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Radr)

static struct BurnInputInfo SlipstrmInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy5 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy5 + 4,	"p1 start"	},
	{"P1 Shift",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	A("P1 Steering",	BIT_ANALOG_REL, &Analog[0],		"p1 x-axis"	),
	A("P1 Accelerate",	BIT_ANALOG_REL, &Analog[1],		"p1 y-axis"	),
	A("P1 Brake",		BIT_ANALOG_REL, &Analog[2],		"p1 z-axis"	),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy5 + 1,	"diag"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy5 + 0,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy6 + 4,	"service"	},
	{"Service 3",		BIT_DIGITAL,	DrvJoy6 + 5,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Slipstrm)

static struct BurnInputInfo SonicInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy5 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy5 + 4,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	A("P1 Trackball X",	BIT_ANALOG_REL, &Analog[0],    	"p1 x-axis" ),
	A("P1 Trackball Y",	BIT_ANALOG_REL, &Analog[1],    	"p1 y-axis" ),

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy5 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy5 + 5,	"p2 start"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 1"	},
	A("P2 Trackball X",	BIT_ANALOG_REL, &Analog[2],    	"p2 x-axis" ),
	A("P2 Trackball Y",	BIT_ANALOG_REL, &Analog[3],    	"p2 y-axis" ),

	{"P3 Coin",			BIT_DIGITAL,	DrvJoy5 + 6,	"p3 coin"	},
	{"P3 Start",		BIT_DIGITAL,	DrvJoy5 + 7,	"p3 start"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy1 + 2,	"p3 fire 1"	},
	A("P3 Trackball X",	BIT_ANALOG_REL, &Analog[4],    	"p3 x-axis" ),
	A("P3 Trackball Y",	BIT_ANALOG_REL, &Analog[5],    	"p3 y-axis" ),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy5 + 1,	"diag"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy5 + 0,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy6 + 4,	"service"	},
	{"Service 3",		BIT_DIGITAL,	DrvJoy6 + 5,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Sonic)

static struct BurnInputInfo HarddunkInputList[] = { // hand crafted
	{"Coin 1",			BIT_DIGITAL,	DrvJoy5 + 2,	"p1 coin"	},
	{"Coin 2",			BIT_DIGITAL,	DrvJoy5 + 3,	"p2 coin"	},

	{"P1 Start",		BIT_DIGITAL,	DrvJoy5 + 4,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 4"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy5 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 4"	},

	{"P3 Start",		BIT_DIGITAL,	DrvJoyX3 + 0,	"p3 start"	},
	{"P3 Up",			BIT_DIGITAL,	DrvJoyX1 + 5,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	DrvJoyX1 + 4,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	DrvJoyX1 + 7,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoyX1 + 6,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoyX1 + 0,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoyX1 + 1,	"p3 fire 2"	},
	{"P3 Button 3",		BIT_DIGITAL,	DrvJoyX1 + 2,	"p3 fire 3"	},
	{"P3 Button 4",		BIT_DIGITAL,	DrvJoyX1 + 3,	"p3 fire 4"	},

	{"P4 Start",		BIT_DIGITAL,	DrvJoy13 + 0,	"p4 start"	},
	{"P4 Up",			BIT_DIGITAL,	DrvJoy8 + 5,	"p4 up"		},
	{"P4 Down",			BIT_DIGITAL,	DrvJoy8 + 4,	"p4 down"	},
	{"P4 Left",			BIT_DIGITAL,	DrvJoy8 + 7,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy8 + 6,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy8 + 0,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy8 + 1,	"p4 fire 2"	},
	{"P4 Button 3",		BIT_DIGITAL,	DrvJoy8 + 2,	"p4 fire 3"	},
	{"P4 Button 4",		BIT_DIGITAL,	DrvJoy8 + 3,	"p4 fire 4"	},

	{"P5 Start",		BIT_DIGITAL,	DrvJoy13 + 1,	"p5 start"	},
	{"P5 Up",			BIT_DIGITAL,	DrvJoy9 + 5,	"p5 up"		},
	{"P5 Down",			BIT_DIGITAL,	DrvJoy9 + 4,	"p5 down"	},
	{"P5 Left",			BIT_DIGITAL,	DrvJoy9 + 7,	"p5 left"	},
	{"P5 Right",		BIT_DIGITAL,	DrvJoy9 + 6,	"p5 right"	},
	{"P5 Button 1",		BIT_DIGITAL,	DrvJoy9 + 0,	"p5 fire 1"	},
	{"P5 Button 2",		BIT_DIGITAL,	DrvJoy9 + 1,	"p5 fire 2"	},
	{"P5 Button 3",		BIT_DIGITAL,	DrvJoy9 + 2,	"p5 fire 3"	},
	{"P5 Button 4",		BIT_DIGITAL,	DrvJoy9 + 3,	"p5 fire 4"	},

	{"P6 Start",		BIT_DIGITAL,	DrvJoyX3 + 1,	"p6 start"	},
	{"P6 Up",			BIT_DIGITAL,	DrvJoyX2 + 5,	"p6 up"		},
	{"P6 Down",			BIT_DIGITAL,	DrvJoyX2 + 4,	"p6 down"	},
	{"P6 Left",			BIT_DIGITAL,	DrvJoyX2 + 7,	"p6 left"	},
	{"P6 Right",		BIT_DIGITAL,	DrvJoyX2 + 6,	"p6 right"	},
	{"P6 Button 1",		BIT_DIGITAL,	DrvJoyX2 + 0,	"p6 fire 1"	},
	{"P6 Button 2",		BIT_DIGITAL,	DrvJoyX2 + 1,	"p6 fire 2"	},
	{"P6 Button 3",		BIT_DIGITAL,	DrvJoyX2 + 2,	"p6 fire 3"	},
	{"P6 Button 4",		BIT_DIGITAL,	DrvJoyX2 + 3,	"p6 fire 4"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy5 + 1,	"diag"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy5 + 0,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy6 + 4,	"service"	},
	{"Service 3",		BIT_DIGITAL,	DrvJoy6 + 5,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Harddunk)

static struct BurnInputInfo OrunnersInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy5 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy5 + 4,	"p1 start"	},
	{"P1 Gear Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Gear Down",	BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},
	{"P1 Music",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 3"	},
	{"P1 Music -",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 4"	},
	{"P1 Music +",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 fire 5"	},
	A("P1 Steering",	BIT_ANALOG_REL, &Analog[0],		"p1 x-axis"	),
	A("P1 Accelerate",	BIT_ANALOG_REL, &Analog[1],		"p1 y-axis"	),
	A("P1 Brake",		BIT_ANALOG_REL, &Analog[2],		"p1 z-axis"	),

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy13 + 2,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy13 + 4,	"p2 start"	},
	{"P2 Gear Up",		BIT_DIGITAL,	DrvJoy9 + 0,	"p2 fire 1"	},
	{"P2 Gear Down",	BIT_DIGITAL,	DrvJoy9 + 1,	"p2 fire 2"	},
	{"P2 Music",		BIT_DIGITAL,	DrvJoy10 + 0,	"p2 fire 3"	},
	{"P2 Music -",		BIT_DIGITAL,	DrvJoy10 + 1,	"p2 fire 4"	},
	{"P2 Music +",		BIT_DIGITAL,	DrvJoy10 + 2,	"p2 fire 5"	},
	A("P2 Steering",	BIT_ANALOG_REL, &Analog[3],		"p2 x-axis"	),
	A("P2 Accelerate",	BIT_ANALOG_REL, &Analog[4],		"p2 y-axis"	),
	A("P2 Brake",		BIT_ANALOG_REL, &Analog[5],		"p2 z-axis"	),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy5 + 1,	"diag"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy5 + 0,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy6 + 4,	"service"	},
	{"Service 3",		BIT_DIGITAL,	DrvJoy6 + 5,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Orunners)

static struct BurnInputInfo ScrossInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy5 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy5 + 4,	"p1 start"	},
	{"P1 Attack",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Wheelie",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},
	{"P1 Brake",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 3"	},
	A("P1 Steering",	BIT_ANALOG_REL, &Analog[0],		"p1 x-axis"	),
	A("P1 Accelerate",	BIT_ANALOG_REL, &Analog[1],		"p1 y-axis"	),

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy13 + 2,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy13 + 4,	"p2 start"	},
	{"P2 Attack",		BIT_DIGITAL,	DrvJoy9 + 0,	"p2 fire 1"	},
	{"P2 Wheelie",		BIT_DIGITAL,	DrvJoy9 + 1,	"p2 fire 2"	},
	{"P2 Brake",		BIT_DIGITAL,	DrvJoy9 + 2,	"p2 fire 3"	},
	A("P2 Steering",	BIT_ANALOG_REL, &Analog[2],		"p2 x-axis"	),
	A("P2 Accelerate",	BIT_ANALOG_REL, &Analog[3],		"p2 y-axis"	),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy5 + 1,	"diag"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy5 + 0,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy6 + 4,	"service"	},
	{"Service 3",		BIT_DIGITAL,	DrvJoy6 + 5,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Scross)

static struct BurnInputInfo TitlefInputList[] = {
	{"P1 Coin",				BIT_DIGITAL,	DrvJoy5 + 2,	"p1 coin"	},
	{"P1 Start",			BIT_DIGITAL,	DrvJoy5 + 4,	"p1 start"	},
	{"P1 Left Stick Up",	BIT_DIGITAL,	DrvJoy1 + 5,	"p1 up"		},
	{"P1 Left Stick Down",	BIT_DIGITAL,	DrvJoy1 + 4,	"p1 down"	},
	{"P1 Left Stick Left",	BIT_DIGITAL,	DrvJoy1 + 7,	"p1 left"	},
	{"P1 Left Stick Right",	BIT_DIGITAL,	DrvJoy1 + 6,	"p1 right"	},
	{"P1 Right Stick Up",	BIT_DIGITAL,	DrvJoy2 + 5,	"p3 up"		},
	{"P1 Right Stick Down",	BIT_DIGITAL,	DrvJoy2 + 4,	"p3 down"	},
	{"P1 Right Stick Left",	BIT_DIGITAL,	DrvJoy2 + 7,	"p3 left"	},
	{"P1 Right Stick Right",BIT_DIGITAL,	DrvJoy2 + 6,	"p3 right"	},

	{"P2 Coin",				BIT_DIGITAL,	DrvJoy13 + 2,	"p2 coin"	},
	{"P2 Start",			BIT_DIGITAL,	DrvJoy5 + 5,	"p2 start"	},
	{"P2 Left Stick Up",	BIT_DIGITAL,	DrvJoy9 + 5,	"p2 up"		},
	{"P2 Left Stick Down",	BIT_DIGITAL,	DrvJoy9 + 4,	"p2 down"	},
	{"P2 Left Stick Left",	BIT_DIGITAL,	DrvJoy9 + 7,	"p2 left"	},
	{"P2 Left Stick Right",	BIT_DIGITAL,	DrvJoy9 + 6,	"p2 right"	},
	{"P2 Right Stick Up",	BIT_DIGITAL,	DrvJoy10 + 5,	"p4 up"		},
	{"P2 Right Stick Down",	BIT_DIGITAL,	DrvJoy10 + 4,	"p4 down"	},
	{"P2 Right Stick Left",	BIT_DIGITAL,	DrvJoy10 + 7,	"p4 left"	},
	{"P2 Right Stick Right",BIT_DIGITAL,	DrvJoy10 + 6,	"p4 right"	},

	{"P3 Start",			BIT_DIGITAL,	DrvJoy13 + 4,	"p3 start"	},
	{"P4 Start",			BIT_DIGITAL,	DrvJoy13 + 5,	"p4 start"	},

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",		BIT_DIGITAL,	DrvJoy5 + 1,	"diag"		},
	{"Service 1",			BIT_DIGITAL,	DrvJoy5 + 0,	"service"	},
	{"Service 2",			BIT_DIGITAL,	DrvJoy6 + 4,	"service"	},
	{"Service 3",			BIT_DIGITAL,	DrvJoy6 + 5,	"service"	},
	{"Dip A",				BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",				BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Titlef)

static struct BurnDIPInfo Kokoroj2DIPList[]=
{
	{0x19, 0xff, 0xff, 0x07, NULL					},

	{0   , 0xfe, 0   ,    2, "CD & Printer"			},
	{0x19, 0x01, 0x08, 0x00, "Off"					},
	{0x19, 0x01, 0x08, 0x08, "On"					},
};

STDDIPINFO(Kokoroj2)

static struct BurnDIPInfo RadrDIPList[]=
{
	DIP_OFFSET(0x0b)
	{0x00, 0xff, 0xff, 0x0f, NULL					},
	{0x01, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    2, "Transmission"			},
	{0x00, 0x01, 0x08, 0x08, "Manual"				},
	{0x00, 0x01, 0x08, 0x00, "Automatic"			},

	{0   , 0xfe, 0   ,    2, "Steering Response"	},
	{0x01, 0x01, 0x10, 0x10, "Linear"				},
	{0x01, 0x01, 0x10, 0x00, "Logarithmic"			},
};

STDDIPINFO(Radr)

#define DEFAULT_UNUSED_DIPS_MS(setname, offs)			\
static struct BurnDIPInfo setname##DIPList[]=			\
{														\
	DIP_OFFSET(offs)									\
	{0x00, 0xff, 0xff, 0x0f, NULL					},	\
	{0x01, 0xff, 0xff, 0x00, NULL					},	\
														\
	{0   , 0xfe, 0   ,    2, "Unused"				},	\
	{0x00, 0x01, 0x0f, 0x0f, "Off"					},	\
	{0x00, 0x01, 0x0f, 0x00, "On"					},	\
														\
	{0   , 0xfe, 0   ,    2, "Multi-Screen Mode"	},	\
	{0x01, 0x01, 0x01, 0x01, "Disabled"				},	\
	{0x01, 0x01, 0x01, 0x00, "Enabled"				},	\
};														\
														\
STDDIPINFO(setname)

#define DEFAULT_UNUSED_DIPS_MS_WHEEL(setname, offs)		\
static struct BurnDIPInfo setname##DIPList[]=			\
{														\
	DIP_OFFSET(offs)									\
	{0x00, 0xff, 0xff, 0x0f, NULL					},	\
	{0x01, 0xff, 0xff, 0x00, NULL					},	\
														\
	{0   , 0xfe, 0   ,    2, "Unused"				},	\
	{0x00, 0x01, 0x0f, 0x0f, "Off"					},	\
	{0x00, 0x01, 0x0f, 0x00, "On"					},	\
														\
	{0   , 0xfe, 0   ,    2, "Multi-Screen Mode"	},	\
	{0x01, 0x01, 0x01, 0x01, "Disabled"				},	\
	{0x01, 0x01, 0x01, 0x00, "Enabled"				},	\
														\
	{0   , 0xfe, 0   ,    2, "Steering Response"	},  \
	{0x01, 0x01, 0x10, 0x10, "Linear"				},  \
	{0x01, 0x01, 0x10, 0x00, "Logarithmic"			},  \
};														\
														\
STDDIPINFO(setname)

#define DEFAULT_UNUSED_DIPS(setname, offs)				\
static struct BurnDIPInfo setname##DIPList[]=			\
{														\
	DIP_OFFSET(offs)									\
	{0x00, 0xff, 0xff, 0x0f, NULL					},	\
														\
	{0   , 0xfe, 0   ,    2, "Unused"				},	\
	{0x00, 0x01, 0x0f, 0x0f, "Off"					},	\
	{0x00, 0x01, 0x0f, 0x00, "On"					},	\
};														\
														\
STDDIPINFO(setname)

#define DEFAULT_UNUSED_DIPS_WHEEL(setname, offs)		\
static struct BurnDIPInfo setname##DIPList[]=			\
{														\
	DIP_OFFSET(offs)									\
	{0x00, 0xff, 0xff, 0x0f, NULL					},	\
	{0x01, 0xff, 0xff, 0x00, NULL					},	\
														\
	{0   , 0xfe, 0   ,    2, "Unused"				},	\
	{0x00, 0x01, 0x0f, 0x0f, "Off"					},	\
	{0x00, 0x01, 0x0f, 0x00, "On"					},	\
														\
	{0   , 0xfe, 0   ,    2, "Steering Response"	},  \
	{0x01, 0x01, 0x10, 0x10, "Linear"				},  \
	{0x01, 0x01, 0x10, 0x00, "Logarithmic"			},  \
};														\
														\
STDDIPINFO(setname)

DEFAULT_UNUSED_DIPS(Arabfgt, 0x23)
DEFAULT_UNUSED_DIPS(Arabfgtu, 0x25)
DEFAULT_UNUSED_DIPS(Brival, 0x1d)
DEFAULT_UNUSED_DIPS(Ga2, 0x29)
DEFAULT_UNUSED_DIPS(Ga2u, 0x29)
DEFAULT_UNUSED_DIPS(Darkedge, 0x1b)
DEFAULT_UNUSED_DIPS(Spidman, 0x25)
DEFAULT_UNUSED_DIPS(Alien3, 0x0f)
DEFAULT_UNUSED_DIPS(Jpark, 0x10)
DEFAULT_UNUSED_DIPS(Arescue, 0x09)
DEFAULT_UNUSED_DIPS(Holo, 0x15)
DEFAULT_UNUSED_DIPS(Dbzvrvs, 0x17)
DEFAULT_UNUSED_DIPS(Spidmanu, 0x25)
DEFAULT_UNUSED_DIPS(Sonic, 0x14)
DEFAULT_UNUSED_DIPS_WHEEL(Slipstrm, 0x0b)
DEFAULT_UNUSED_DIPS_WHEEL(Radm, 0x0c)
DEFAULT_UNUSED_DIPS_WHEEL(F1en, 0x0c)
DEFAULT_UNUSED_DIPS_WHEEL(F1lap, 0x0d)
DEFAULT_UNUSED_DIPS_MS_WHEEL(Orunners, 0x19)
DEFAULT_UNUSED_DIPS_MS(Harddunk, 0x3d)
DEFAULT_UNUSED_DIPS_MS(Scross, 0x13)
DEFAULT_UNUSED_DIPS_MS(Titlef, 0x1b)

static INT32 irq_callback(INT32 /*state*/)
{
	v60SetIRQLine(0, CPU_IRQSTATUS_NONE);
	return v60_irq_vector;
}

static void update_irq_state()
{
	UINT8 effirq = v60_irq_control[7] & ~v60_irq_control[6] & 0x1f;
	INT32 vector;

	for (vector = 0; vector < 5; vector++)
		if (effirq & (1 << vector))
		{
			v60_irq_vector = vector;
			v60SetIRQLine(0, CPU_IRQSTATUS_ACK);
			break;
		}

	if (vector == 5) {
		v60SetIRQLine(0, CPU_IRQSTATUS_NONE);
	}
}

static void signal_v60_irq(INT32 which)
{
	for (INT32 i = 0; i < 5; i++)
		if (v60_irq_control[i] == which)
			v60_irq_control[7] |= 1 << i;
	update_irq_state();
}

static void sync_sound()
{
	INT32 cyc = ((INT64)v60TotalCycles() * 8053975 / ((is_multi32) ? 20000000 : 16107950)) - ZetTotalCycles();
	if (cyc > 0) {
		BurnTimerUpdate(ZetTotalCycles() + cyc);
	}
}

static void update_sound_irq_state()
{
	INT32 vector;
	UINT8 effirq = sound_irq_input & ~sound_irq_control[3] & 0x07;

	for (vector = 0; vector < 3; vector++)
		if (effirq & (1 << vector))
		{
			ZetSetVector(2 * vector);
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
			break;
		}

	if (vector == 3)
		ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
}

static void signal_sound_irq(INT32 which)
{
	for (INT32 i = 0; i < 3; i++)
		if (sound_irq_control[i] == which)
			sound_irq_input |= 1 << i;

	update_sound_irq_state();
}

static void clear_sound_irq(INT32 which)
{
	for (INT32 i = 0; i < 3; i++)
		if (sound_irq_control[i] == which)
			sound_irq_input &= ~(1 << i);

	update_sound_irq_state();
}

static void sound_int_control_lo_write(UINT16 offset, UINT8 data)
{
	if (offset & 1)
	{
		sound_irq_input &= data;
		update_sound_irq_state();
	}

	if (offset & 4)
		signal_v60_irq(MAIN_IRQ_SOUND);
}


static void sound_int_control_hi_write(UINT16 offset, UINT8 data)
{
	sound_irq_control[offset] = data;
	update_sound_irq_state();
}

static INT32 delayed_int = 0;

static void int_control_write(UINT32 offset, UINT8 data)
{
	INT32 duration;

	switch (offset)
	{
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
			v60_irq_control[offset] = data;
			break;

		case 5:
			v60_irq_control[offset] = data;
			break;

		case 6:
			v60_irq_control[offset] = data;
			delayed_int = 1;
			v60RunEnd();
			break;

		case 7:
			v60_irq_control[offset] &= data;
			delayed_int = 1;
			v60RunEnd();
			break;

		case 8:
		case 9:
			v60_irq_control[offset] = data;
			duration = v60_irq_control[8] + ((v60_irq_control[9] << 8) & 0xf00);
			if (duration)
			{
				timer_0_cycles = (INT32)((double)((1.0 / TIMER_0_CLOCK) * duration) * ((is_multi32?40000000:32215900)/2));
			}
			break;

		case 10:
		case 11:
			v60_irq_control[offset] = data;
			duration = v60_irq_control[10] + ((v60_irq_control[11] << 8) & 0xf00);
			if (duration)
			{
				timer_1_cycles = (INT32)((double)((1.0 / TIMER_1_CLOCK) * duration) * ((is_multi32?40000000:32215900)/2));
			}
			break;

		case 12:
		case 13:
		case 14:
		case 15:
			sync_sound();
			signal_sound_irq(SOUND_IRQ_V60);
			break;
	}
}

inline UINT16 xBBBBBGGGGGRRRRR_to_xBGRBBBBGGGGRRRR(UINT16 value)
{
	INT32 r = (value >> 0) & 0x1f;
	INT32 g = (value >> 5) & 0x1f;
	INT32 b = (value >> 10) & 0x1f;
	value = (value & 0x8000) | ((b & 0x01) << 14) | ((g & 0x01) << 13) | ((r & 0x01) << 12);
	value |= ((b & 0x1e) << 7) | ((g & 0x1e) << 3) | ((r & 0x1e) >> 1);
	return value;
}

inline UINT16 xBGRBBBBGGGGRRRR_to_xBBBBBGGGGGRRRRR(UINT16 value)
{
	INT32 r = ((value >> 12) & 0x01) | ((value << 1) & 0x1e);
	INT32 g = ((value >> 13) & 0x01) | ((value >> 3) & 0x1e);
	INT32 b = ((value >> 14) & 0x01) | ((value >> 7) & 0x1e);
	return (value & 0x8000) | (b << 10) | (g << 5) | (r << 0);
}

static void paletteram_write(INT32 which, UINT32 offset, UINT16 data, UINT16 mem_mask)
{
	INT32 convert = (offset & 0x4000);
	offset &= 0x3fff;

	UINT16 *ram = (UINT16*)DrvPalRAM[which];

	UINT16 value = BURN_ENDIAN_SWAP_INT16(ram[offset]);
	if (convert) value = xBBBBBGGGGGRRRRR_to_xBGRBBBBGGGGRRRR(value);
	value = (value & ~mem_mask) | (data & mem_mask);
	if (convert) value = xBGRBBBBGGGGRRRR_to_xBBBBBGGGGGRRRRR(value);
	ram[offset] = BURN_ENDIAN_SWAP_INT16(value);

	if (mixer_control[which][0x4e/2] & 0x0880)
	{
		offset ^= 0x2000;
		value = BURN_ENDIAN_SWAP_INT16(ram[offset]);
		if (convert) value = xBBBBBGGGGGRRRRR_to_xBGRBBBBGGGGRRRR(value);
		value = (value & ~mem_mask) | (data & mem_mask);
		if (convert) value = xBGRBBBBGGGGRRRR_to_xBBBBBGGGGGRRRRR(value);
		ram[offset] = BURN_ENDIAN_SWAP_INT16(value);
	}
}

static UINT16 paletteram_read(INT32 which, UINT32 offset)
{
	INT32 convert = (offset & 0x4000);
	offset &= 0x3fff;

	UINT16 *ram = (UINT16*)DrvPalRAM[which];

	if (!convert)
		return BURN_ENDIAN_SWAP_INT16(ram[offset]);
	else
		return xBBBBBGGGGGRRRRR_to_xBGRBBBBGGGGRRRR(BURN_ENDIAN_SWAP_INT16(ram[offset]));
}

static UINT8 sprite_control_read(UINT32 offset)
{
	switch (offset)
	{
		case 0:
			return 0xfc | 0;

		case 1:
			return 0xfc | 1;

		case 2:
			return 0xfc | sprite_control_latched[2];

		case 3:
			return 0xfc | sprite_control_latched[3];

		case 4:
			return 0xfc | sprite_control_latched[4];

		case 5:
			return 0xfc | sprite_control_latched[5];

		case 6:
			return 0xfc | (sprite_control_latched[6] & 1);

		case 7:
			return 0xfc;
	}

	return 0xff;
}

static void io_chip_write(INT32 which, UINT32 offset, UINT16 data, UINT16 /*mem_mask*/)
{
	offset &= 0x0f;
	misc_io_data[which][offset] = data & 0xff;

	switch (offset)
	{
		case 0x03:
			if (which == 0) {
				EEPROMWrite(data & 0x40, data & 0x20, data & 0x80);
			}
			break;

		case 0x07:
			if (which == 0) { system32_tilebank_external = data; }
			else { EEPROMWrite(data & 0x40, data & 0x20, data & 0x80); } // multi32
			break;

		case 0x0e:
			system32_displayenable[which] = data & 0x02;
			if (which == 0) {
				sync_sound();
				ZetSetRESETLine(~data & 4);
			}
			break;
	}
}

static UINT16 io_chip_read(INT32 which, UINT32 offset)
{
	offset &= 0x0f;

	switch (offset)
	{
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
		case 0x04:
		case 0x05:
		case 0x06:
		case 0x07:
		{
			if (misc_io_data[which][0x0f] & (1 << offset))
				return misc_io_data[which][offset];

			INT32 ret = DrvInputs[offset + (which * 8)];
			if (which == 0 && offset == 5) ret = (ret & 0x70) | (DrvDips[0] & 0xf) | (EEPROMRead() ? 0x80 : 0); // service 34_A
			if (which == 1 && offset == 5) ret = (ret & 0x70) | (DrvDips[0] & 0xf) | (EEPROMRead() ? 0x80 : 0);	// service 34_B (really should have own dips?)
			return ret;
		}

		case 0x08:
			return 'S';
		case 0x09:
			return 'E';
		case 0x0a:
			return 'G';
		case 0x0b:
			return 'A';

		case 0x0c:
		case 0x0e:
			return misc_io_data[which][0x0e];

		case 0x0d:
		case 0x0f:
			return misc_io_data[which][0x0f];
	}
	return 0xffff;
}

static inline void mark_dirty(UINT32 offset)
{
	if (offset < (0x1ff00 >> 1)) {
		for (cache_entry *entry = cache_head; entry != NULL; entry = entry->next) {
			if (entry->page == (offset >> 9)) {
				entry->dirty = 1;
				GenericTilemapSetTileDirty(entry->tmap, offset & 0x1ff);
			}
		}
	}
}

static void system32_main_write_word(UINT32 address, UINT16 data)
{
#ifdef LOG_RW
	if ((address & 0xff0000) != 0x600000 && (address & 0xfe00000) != 0x400000 && (address & 0xffff80) != 0x610000 && (address & 0xffe000) != 0x700000) bprintf (0, _T("MWW: %5.5x %4.4x\n"), address, data);
#endif

	if ((address & 0xff0000) == 0x200000) {
		INT32 offset = (address & 0xfffe) / 2;
		UINT16 *ram = (UINT16*)DrvV60RAM;
		ram[offset] = BURN_ENDIAN_SWAP_INT16(data);
		if (memory_protection_write) {
			memory_protection_write(offset, data, 0xffff);
		}
		return;
	}

	if ((address & 0xf00000) == 0x300000) {
		UINT16 *ram = (UINT16*)DrvVidRAM;
		ram[(address & 0x1ffff) >> 1] = BURN_ENDIAN_SWAP_INT16(data);
		mark_dirty((address & 0x1ffff) >> 1);
		return;
	}

	if ((address & 0xfe0000) == 0x400000) {
		INT32 offset = (address & 0x1fffe) / 2;
		UINT16 *ram = (UINT16*)DrvSprRAM;
		ram[offset] = BURN_ENDIAN_SWAP_INT16(data);
		DrvSprRAM32[(address >> 2) & 0x7fff] = BURN_ENDIAN_SWAP_INT32((BURN_ENDIAN_SWAP_INT16(ram[offset|1]) >> 8) | ((BURN_ENDIAN_SWAP_INT16(ram[offset|1]) << 8) & 0xff00) | ((BURN_ENDIAN_SWAP_INT16(ram[offset & ~1]) << 8) & 0xff0000) | (BURN_ENDIAN_SWAP_INT16(ram[offset & ~1]) << 24));
		return;
	}

	if ((address & 0xf00000) == 0x500000) {
		INT32 offset = (address & 0xe) / 2;
		sprite_control[offset] = data & 0xff;
		return;
	}

	if ((address & 0xff0000) == 0x600000) {
		INT32 offset = (address & 0xfffe) / 2;
		paletteram_write(0, offset, data, 0xffff);
		return;
	}

	if ((address & 0xffff80) == 0x610000) {
		INT32 offset = (address & 0x7e) / 2;
		mixer_control[0][offset] = data;
		return;
	}

	if ((address & 0xff0000) == 0x680000) {
		INT32 offset = (address & 0xfffe) / 2;
		paletteram_write(1, offset, data, 0xffff);
		return;
	}

	if ((address & 0xffff80) == 0x690000) {
		INT32 offset = (address & 0x7e) / 2;
		mixer_control[1][offset] = data;
		return;
	}

	if ((address & 0xffe000) == 0x700000) {
		INT32 offset = (address & 0x1ffe) / 2;
		DrvShareRAM[offset*2+0] = data & 0xff;
		DrvShareRAM[offset*2+1] = data >> 8;
		return;
	}

	if ((address & 0xfff000) == 0x810000) {
		INT32 offset = (address & 0xffe) / 2;
		UINT16 *ram = (UINT16*)DrvCommsRAM;
		ram[offset] = BURN_ENDIAN_SWAP_INT16(data);
		return;
	}

	if ((address & 0xf00000) == 0xa00000) {
		if (protection_a00000_write) {
			protection_a00000_write((address & 0xffffe) / 2, data, 0xffff);
		}
		return; // protection write!
	}

	if ((address & 0xffffe0) == 0xc00000) {
		INT32 offset = (address & 0x1e) / 2;
		io_chip_write(0, offset, data, 0xffff);
		return;
	}

	if ((address & 0xffffc0) == 0xc00040) {
		INT32 offset = (address & 0x3e) / 2;
		if (custom_io_write_0) {
			custom_io_write_0(offset, data, 0xffff);
		}
		return;
	}

	if ((address & 0xffffe0) == 0xc80000) {
		INT32 offset = (address & 0x1e) / 2;
		io_chip_write(1, offset, data, 0xffff);
		return;
	}

	if ((address & 0xfffff0) == 0xd00000) {
		INT32 offset = (address & 0x0e) / 2;
		int_control_write(offset * 2 + 0, data & 0xff);
		int_control_write(offset * 2 + 1, data >> 8);
		return;
	}

	if ((address & 0xf80000) == 0xd80000) {
		// random number gen writes (not used)
		return;
	}

	bprintf (0, _T("MWW: %5.5x %4.4x\n"), address, data);
}

static void system32_main_write_byte(UINT32 address, UINT8 data)
{
#ifdef LOG_RW
	bprintf (0, _T("MWB: %5.5x, %2.2x\n"), address, data);
#endif

	INT32 shift = ((address & 1) << 3);
	UINT16 value = data << shift;
	UINT16 mask = 0xff << shift;

	if ((address & 0xff0000) == 0x200000) {
		DrvV60RAM[address & 0xffff] = data;
		if (memory_protection_write) {
			memory_protection_write((address & 0xfffe) / 2, value, mask);
		}
		return;
	}

	if ((address & 0xf00000) == 0x300000) {
		DrvVidRAM[address & 0x1ffff] = data;
		mark_dirty((address & 0x1ffff) >> 1);
		return;
	}

	if ((address & 0xfe0000) == 0x400000) {
		INT32 offset = address & 0x1ffff;
		DrvSprRAM[offset] = data;
		offset /= 2;
		UINT16 *ram = (UINT16*)DrvSprRAM;
		DrvSprRAM32[offset / 2] = BURN_ENDIAN_SWAP_INT32((BURN_ENDIAN_SWAP_INT16(ram[offset | 1]) >> 8) | ((BURN_ENDIAN_SWAP_INT16(ram[offset | 1]) << 8) & 0xff00) | ((BURN_ENDIAN_SWAP_INT16(ram[offset & ~1]) << 8) & 0xff0000) | (BURN_ENDIAN_SWAP_INT16(ram[offset & ~1]) << 24));
		return;
	}

	if ((address & 0xf00000) == 0x500000) {
		INT32 offset = (address & 0xe) / 2;
		if ((address & 1) == 0) sprite_control[offset] = data & 0xff;
		return;
	}

	if ((address & 0xff0000) == 0x600000) {
		INT32 offset = (address & 0xfffe) / 2;
		paletteram_write(0, offset, value, mask);
		return;
	}

	if ((address & 0xffff80) == 0x610000) {
		INT32 offset = (address & 0x7e) / 2;
		mixer_control[0][offset] = (mixer_control[0][offset] & ~mask) | (value & mask);
		return;
	}

	if ((address & 0xff0000) == 0x680000) {
		INT32 offset = (address & 0xfffe) / 2;
		paletteram_write(1, offset, value, mask);
		return;
	}

	if ((address & 0xffff80) == 0x690000) {
		INT32 offset = (address & 0x7e) / 2;
		mixer_control[1][offset] = (mixer_control[1][offset] & ~mask) | (value & mask);
		return;
	}

	if ((address & 0xffe000) == 0x700000) {
		DrvShareRAM[address & 0x1fff] = data;
		return;
	}

	if ((address & 0xfff000) == 0x810000) {
		DrvCommsRAM[address & 0xfff] = data;
		if (address == 0x810048) DrvCommsRAM[(address & 0xfff) + 1] = data;
		return;
	}

	if ((address & 0xf00000) == 0xa00000) {
		if (protection_a00000_write) {
			protection_a00000_write((address & 0xffffe) / 2, value, mask);
		}
		return; // protection write!
	}

	if ((address & 0xffffe0) == 0xc00000) {
		INT32 offset = (address & 0x1e) / 2;
		if ((address & 1) == 0) io_chip_write(0, offset, value, mask); // ok
		return;
	}

	if ((address & 0xffffc0) == 0xc00040) {
		INT32 offset = (address & 0x3e) / 2;
		if (custom_io_write_0) {
			custom_io_write_0(offset, value, mask);
		}
		return;
	}

	if ((address & 0xffffe0) == 0xc80000) {
		INT32 offset = (address & 0x1e) / 2;
		if ((address & 1) == 0) io_chip_write(1, offset, value, mask); // ok
		return;
	}

	if ((address & 0xfffff0) == 0xd00000) {
		int_control_write(address & 0xf, data);
		return;
	}

	if ((address & 0xf00000) == 0xe00000) return; // nop?

	bprintf (0, _T("MWB: %5.5x, %2.2x\n"), address, data);
}

static void system32_main_write_long(UINT32 address, UINT32 data)
{
	system32_main_write_word((address & ~1) + 0, data & 0xffff); // seems right
	system32_main_write_word((address & ~1) + 2, data >> 16);
}

static UINT16 system32_main_read_word(UINT32 address)
{
#ifdef LOG_RW
	if ((address & 0xff0000) != 0x600000) bprintf (0, _T("MRW: %5.5x\n"), address);
#endif

	if ((address & 0xff0000) == 0x200000) {
		INT32 offset = (address & 0xfffe) / 2;
		UINT16 *ram = (UINT16*)DrvV60RAM;
		if (memory_protection_read) {
			return memory_protection_read(offset, 0xffff);
		}
		return BURN_ENDIAN_SWAP_INT16(ram[offset]);
	}

	if ((address & 0xf00000) == 0x300000) {
		UINT16 *ram = (UINT16*)DrvVidRAM;
		return BURN_ENDIAN_SWAP_INT16(ram[(address & 0x1ffff) >> 1]);
	}

	if ((address & 0xf00000) == 0x500000) {
		INT32 offset = (address & 0xe) / 2;
		return sprite_control_read(offset);
	}

	if ((address & 0xff0000) == 0x600000) {
		INT32 offset = (address & 0xfffe) / 2;
		return paletteram_read(0, offset);
	}

	if ((address & 0xffff80) == 0x610000) {
		INT32 offset = (address & 0x7e) / 2;
		return mixer_control[0][offset];
	}

	if ((address & 0xff0000) == 0x680000) {
		INT32 offset = (address & 0xfffe) / 2;
		return paletteram_read(1, offset);
	}

	if ((address & 0xffff80) == 0x690000) {
		INT32 offset = (address & 0x7e) / 2;
		return mixer_control[1][offset];
	}

	if ((address & 0xffe000) == 0x700000) {
		INT32 offset = (address & 0x1ffe) / 2;
		return DrvShareRAM[offset*2+0] | (DrvShareRAM[offset*2+1] << 8);
	}

	if ((address & 0xfff000) == 0x810000) {
		INT32 offset = (address & 0xffe) / 2;
		//bprintf (0, _T("Comms Read: %5.5x\n"), offset);
		UINT16 *ram = (UINT16*)DrvCommsRAM;
		if (offset == 0) return 0;
		if (offset == 7) return 0x0100; // ?
		return BURN_ENDIAN_SWAP_INT16(ram[offset]);
	}

	if ((address & 0xfffffc) == 0x818000) {
		return 0; // <-- 0 master, 1 slave
	}

	if ((address & 0xf00000) == 0xa00000) {
		INT32 offset = (address & 0xffffe) / 2;
		if (protection_a00000_read) {
			return protection_a00000_read(offset);
		}
//		return 0xffff; // let fall through to unmapped
	}

	if ((address & 0xffffe0) == 0xc00000) {
		INT32 offset = (address & 0x1e) / 2;
		return io_chip_read(0, offset);
	}

	if ((address & 0xffffc0) == 0xc00040) {
		INT32 offset = (address & 0x3e) / 2;
		if (custom_io_read_0) return custom_io_read_0(offset);
//		return 0xffff; // let fall through to unmapped
	}

	if ((address & 0xffffe0) == 0xc80000) {
		INT32 offset = (address & 0x1e) / 2;
		return io_chip_read(1, offset);
	}

	if ((address & 0xfffff0) == 0xd00000) {
		return 0xffff;	// interrupt controller status
	}

	if ((address & 0xf80000) == 0xd80000) {
		return BurnRandom();
	}

	bprintf (0, _T("MRW: %5.5x\n"), address);

	return 0xffff;
}

static UINT8 system32_main_read_byte(UINT32 address)
{
	if ((address & 0xff0000) == 0x200000) {
		INT32 offset = (address & 0xfffe) / 2;
		if (memory_protection_read) {
			return memory_protection_read(offset, 0xff << ((address & 1) * 8));
		}
		return DrvV60RAM[address & 0xffff];
	}

#ifdef LOG_RW
	bprintf (0, _T("MRB: %5.5x\n"), address);
#endif

	return system32_main_read_word(address & ~1) >> ((address & 1) * 8);
}

static UINT32 system32_main_read_long(UINT32 address)
{
#ifdef LOG_RW
	bprintf (0, _T("RL: %8.8x\n"), address);
#endif
	return system32_main_read_word(address & ~1) | (system32_main_read_word((address & ~1) + 2) << 16); // ??
}


static void bankswitch(INT32 which, INT32 , INT32 data)
{
	INT32 bank = which ? ((sound_bank & 0x3f) | ((data & 0x04) << 4) | ((data & 0x03) << 7)) : ((sound_bank & ~0x3f) | (data & 0x3f));
	sound_bank = bank;

	ZetMapMemory(DrvZ80ROM + sound_bank * 0x2000, 0xa000, 0xbfff, MAP_ROM);
}

static void __fastcall system32_sound_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfff0) == 0xc000) {
		RF5C68PCMRegWrite(address & 0x000f, data);
		return;
	}

	if ((address & 0xf000) == 0xd000) {
		RF5C68PCMWrite(address & 0x0fff, data);
		return;
	}
}

static UINT8 __fastcall system32_sound_read(UINT16 address)
{
	if ((address & 0xf000) == 0xd000) {
		return RF5C68PCMRead(address & 0x0fff);
	}

	return 0;
}

static void __fastcall system32_sound_write_port(UINT16 port, UINT8 data)
{
	if ((port & 0xe0) == 0x80) {
		BurnYM3438Write((port >> 4) & 1, port & 3, data);
		return;
	}

	if ((port & 0xe0) == 0xa0) {
		bankswitch((port >> 4) & 1, port & 0xf, data);
		return;
	}

	if ((port & 0xf0) == 0xc0) {
		sound_int_control_lo_write(port & 0xf, data);
		return;
	}

	if ((port & 0xf8) == 0xd0) {
		sound_int_control_hi_write(port & 3, data);
		return;
	}

	if ((port & 0xff) == 0xf1) {
		sound_dummy_data = data;
		return;
	}
}

static UINT8 __fastcall system32_sound_read_port(UINT16 port)
{
	if ((port & 0xe0) == 0x80) {
		return BurnYM3438Read((port >> 4) & 1, port & 3);
	}

	if ((port & 0xff) == 0xf1) {
		return sound_dummy_data;
	}

	return 0xff;
}

static void pcm_bankswitch(UINT8 data)
{
	pcm_bankdata = data;
	if (is_scross) {
		MultiPCMSetBank((data & 7) * 0x80000, (data & 7) * 0x80000);
	} else {
		MultiPCMSetBank(((data >> 3) & 7) * 0x80000, (data & 7) * 0x80000);
	}
}

static void __fastcall multi32_sound_write(UINT16 address, UINT8 data)
{
	if ((address & 0xe000) == 0xc000) {
		MultiPCMWrite(address & 0x1fff, data);
		return;
	}
}

static UINT8 __fastcall multi32_sound_read(UINT16 address)
{
	if ((address & 0xe000) == 0xc000) {
		return MultiPCMRead(); // address masked by & 0x7ff in core
	}

	return 0;
}

static void __fastcall multi32_sound_write_port(UINT16 port, UINT8 data)
{
	if ((port & 0xf0) == 0x90) return; // not used

	if ((port & 0xf0) == 0xb0) {
		pcm_bankswitch(data);
		return;
	}

	system32_sound_write_port(port, data);
}

static UINT8 __fastcall multi32_sound_read_port(UINT16 port)
{
	if ((port & 0xf0) == 0x90) return 0xff; // second ym not used

	return system32_sound_read_port(port);
}

static tilemap_callback( layer )
{
	UINT16 *ram = (UINT16*)DrvVidRAM;
	UINT16 data = BURN_ENDIAN_SWAP_INT16(ram[((tilemap_cache->page & 0x7f) << 9) | offs]);

	TILE_SET_INFO(0, (tilemap_cache->bank << 13) | (data & 0x1fff), (data >> 4) & 0x1ff, (data >> 14) & 3);
}

static void tilemap_configure_allocate()
{
	GenericTilesInit();
	GenericTilemapSetGfx(0, DrvGfxROM[0], 4, 16, 16, graphics_length[0], 0, 0x3ff);

	if (has_gun) {
		BurnGunInit(2, false);
	}

	memset (solid_ffff, 0xff, sizeof(solid_ffff));
	memset (solid_0000, 0x00, sizeof(solid_0000));

	cache_head = NULL;

	for (INT32 tmap = 0; tmap < 32; tmap++)
	{
		cache_entry *entry = &tmap_cache[tmap];

		entry->tmap = 0;
		entry->page = 0xff;
		entry->bank = 0;
		entry->next = cache_head;
		entry->tmap = tmap;
		entry->dirty = 1;

		GenericTilemapInit(tmap, TILEMAP_SCAN_ROWS, layer_map_callback, 16, 16, 32, 16);
		GenericTilemapUseDirtyTiles(tmap);

		BurnBitmapAllocate(32 + tmap, 512, 256, true); // each tmap gets a draw-buffer (speedup)

		cache_head = entry;
	}

	for (INT32 bitmap = 0; bitmap < 1+4+12+2; bitmap++)
	{
		BurnBitmapAllocate(1+bitmap, 512, 256, true);
		bitmaps[bitmap] = BurnBitmapGetBitmap(1+bitmap);
	}

	if (nScreenWidth >= 640) BurnBitmapAllocate(31, 512, 512, false); // extra bitmap for multi32

	BurnShiftInit(SHIFT_POSITION_BOTTOM_RIGHT, SHIFT_COLOR_GREEN, 80); // slipstrm
}

inline static void DrvFMIRQHandler(INT32 chip, INT32 state)
{
	if (chip != 0) return; // only chip #0 can generate irq's
	if (state)
		signal_sound_irq(SOUND_IRQ_YM3438);
	else
		clear_sound_irq(SOUND_IRQ_YM3438);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	v60Open(0);
	v60_irq_vector = 0;
	v60Reset();
	v60SetIRQLine(0, CPU_IRQSTATUS_NONE);
	v60Close();

	if (use_v25) {
		VezOpen(0);
		VezReset();
		VezClose();
	}

	ZetOpen(0);
	sound_bank = 0; // before bankswitch! -dink
	bankswitch(0,0,0);
	ZetReset();
	BurnYM3438Reset();
	if (is_multi32) {
		MultiPCMReset();
	} else {
		RF5C68PCMReset();
	}
	ZetClose();

	EEPROMReset();
	if (EEPROMAvailable() == 0) {
		EEPROMFill(DrvEEPROM, 0, 0x80);
	}

	BurnShiftReset();

	memset (DrvShareRAM, 0xff, 0x2000);

	memset (m_prev_bgstartx, 0xff, sizeof(m_prev_bgstartx));
	memset (m_prev_bgendx, 0xff, sizeof(m_prev_bgendx));
	memset (m_bgcolor_line, 0xff, sizeof(m_bgcolor_line));

	memset (v60_irq_control, 0xff, sizeof(v60_irq_control));
	v60_irq_vector = 0;
	memset (sound_irq_control, 0, sizeof(sound_irq_control));
	sound_irq_input = 0;
	sound_dummy_data = 0;
	pcm_bankdata = 0;

	memset (mixer_control, 0xff, sizeof(mixer_control));
	memset (sprite_control, 0, sizeof(sprite_control));
	memset (sprite_control_latched, 0, sizeof(sprite_control_latched));
	memset (analog_value, 0, sizeof(analog_value));
	analog_bank = 0;
	memset (misc_io_data, 0, sizeof(misc_io_data));
	memset (system32_displayenable, 0, sizeof(system32_displayenable));

	sprite_render_count = 0;
	system32_tilebank_external = 0;

	Radm_analog_adder = Radm_analog_target = 0;

	timer_0_cycles = timer_1_cycles = -1;

	nExtraCycles[0] = nExtraCycles[1] = 0;

	if (is_multi32) {
		MultiScreenCheck();
	} else {
		SingleScreenModeChangeCheck();
	}

	BurnRandomSetSeed(0xbeef1eaf);

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvV60ROM		= Next; Next += 0x200000;
	DrvZ80ROM		= Next; Next += 0x400000;
	DrvV25ROM		= Next; Next += 0x010000;

	DrvGfxROM[0]	= Next; Next += graphics_length[0];
	DrvGfxROM[1]	= Next; Next += graphics_length[1];

	DrvPCMROM		= Next; Next += 0x600000;

	DrvEEPROM		= Next; Next += 0x000080;

	DrvPalette		= (UINT32*)Next; Next += 0xc000 * sizeof(UINT32);

	AllRam			= Next;

	DrvV60RAM		= Next; Next += 0x020000;
	DrvPalRAM[0]	= Next; Next += 0x008000;
	DrvPalRAM[1]	= Next; Next += 0x008000;
	DrvVidRAM		= Next; Next += 0x020000;
	DrvSprRAM		= Next; Next += 0x020000;
	DrvSprRAM32		= (UINT32*)Next; Next += 0x020000;
	DrvShareRAM		= Next; Next += 0x002000;

	DrvCommsRAM		= Next; Next += 0x001000;

	DrvV25RAM		= Next; Next += 0x010000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void decrypt_protrom()
{
	for (INT32 i = 0; i < 0x10000; i++) {
		DrvV25RAM[i] = DrvV25ROM[BITSWAP16(i, 14, 11, 15, 12, 13, 4, 3, 7, 5, 10, 2, 8, 9, 6, 1, 0)];
	}

	memcpy(DrvV25ROM, DrvV25RAM, 0x10000);
}

static UINT16 extra_custom_io_read(UINT32 offset)
{
	switch (offset)
	{
		case 0x20/2:
		case 0x22/2:
		case 0x24/2:
		case 0x26/2:
			return DrvExtra[offset & 3];
	}

	return 0xffff;
}

static UINT16 analog_custom_io_read(UINT32 offset)
{
	if (is_radm) RadmAnalogTick();

	switch (offset)
	{
		case 0x10/2:
		case 0x12/2:
		case 0x14/2:
		case 0x16/2:
			UINT16 result = analog_value[offset & 3] | 0x7f;
			analog_value[offset & 3] <<= 1;
			return result;
	}

	return 0xffff;
}

static void analog_custom_io_write(UINT32 offset, UINT16, UINT16)
{
	switch (offset)
	{
		case 0x10/2:
		case 0x12/2:
		case 0x14/2:
		case 0x16/2:
			analog_value[offset & 3] = 0;
			return;
	}
}

// set in frame depending on DIP.
static UINT8 *CURVE = NULL;
static UINT8 CURVE_Linear[0x100]; // calculated in CurveInitSet()
static UINT8 CURVE_Logy[] = {
	0x00, 0x01, 0x13, 0x1d, 0x25, 0x2b, 0x2f, 0x33, 0x37, 0x3a, 0x3d, 0x3f, 0x41, 0x44, 0x46, 0x47,
	0x49, 0x4b, 0x4c, 0x4d, 0x4f, 0x50, 0x51, 0x52, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x59, 0x5a,
	0x5b, 0x5c, 0x5d, 0x5d, 0x5e, 0x5f, 0x60, 0x60, 0x61, 0x62, 0x62, 0x63, 0x63, 0x64, 0x65, 0x65,
	0x66, 0x66, 0x67, 0x67, 0x68, 0x68, 0x69, 0x69, 0x6a, 0x6a, 0x6b, 0x6b, 0x6c, 0x6c, 0x6c, 0x6d,
	0x6d, 0x6e, 0x6e, 0x6e, 0x6f, 0x6f, 0x70, 0x70, 0x70, 0x71, 0x71, 0x71, 0x72, 0x72, 0x72, 0x73,
	0x73, 0x73, 0x74, 0x74, 0x74, 0x75, 0x75, 0x75, 0x76, 0x76, 0x76, 0x76, 0x77, 0x77, 0x77, 0x78,
	0x78, 0x78, 0x78, 0x79, 0x79, 0x79, 0x79, 0x7a, 0x7a, 0x7a, 0x7a, 0x7b, 0x7b, 0x7b, 0x7b, 0x7c,
	0x7c, 0x7c, 0x7c, 0x7d, 0x7d, 0x7d, 0x7d, 0x7d, 0x7e, 0x7e, 0x7e, 0x7e, 0x7f, 0x7f, 0x7f, 0x80,
	0x80, 0x81, 0x81, 0x81, 0x82, 0x82, 0x82, 0x82, 0x83, 0x83, 0x83, 0x83, 0x83, 0x84, 0x84, 0x84,
	0x84, 0x85, 0x85, 0x85, 0x85, 0x86, 0x86, 0x86, 0x86, 0x87, 0x87, 0x87, 0x87, 0x88, 0x88, 0x88,
	0x88, 0x89, 0x89, 0x89, 0x8a, 0x8a, 0x8a, 0x8a, 0x8b, 0x8b, 0x8b, 0x8c, 0x8c, 0x8c, 0x8d, 0x8d,
	0x8d, 0x8e, 0x8e, 0x8e, 0x8f, 0x8f, 0x8f, 0x90, 0x90, 0x90, 0x91, 0x91, 0x92, 0x92, 0x92, 0x93,
	0x93, 0x94, 0x94, 0x94, 0x95, 0x95, 0x96, 0x96, 0x97, 0x97, 0x98, 0x98, 0x99, 0x99, 0x9a, 0x9a,
	0x9b, 0x9b, 0x9c, 0x9d, 0x9d, 0x9e, 0x9e, 0x9f, 0xa0, 0xa0, 0xa1, 0xa2, 0xa3, 0xa3, 0xa4, 0xa5,
	0xa6, 0xa7, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xae, 0xaf, 0xb0, 0xb1, 0xb3, 0xb4, 0xb5, 0xb7,
	0xb9, 0xba, 0xbc, 0xbf, 0xc1, 0xc3, 0xc6, 0xc9, 0xcd, 0xd1, 0xd5, 0xdb, 0xe3, 0xed, 0xff, 0xff };

static void CurveInitSet()
{
	if (CURVE == NULL) {
		for (INT32 i = 0; i < 0x100; i++) {
			CURVE_Linear[i] = i;
		}
	}
	CURVE = (DrvDips[1] & 0x10) ? CURVE_Linear : CURVE_Logy;
}

static void arescue_custom_io_write(UINT32 offset, UINT16, UINT16)
{
	switch (offset)
	{
		case 0x10/2: analog_value[offset & 3] = ProcessAnalog(Analog[0], 1, INPUT_DEADZONE, 0x00, 0xff); break;
		case 0x12/2: analog_value[offset & 3] = ProcessAnalog(Analog[1], 0, INPUT_DEADZONE, 0x00, 0xff); break;
		case 0x14/2: analog_value[offset & 3] = ProcessAnalog(Analog[2], 0, INPUT_DEADZONE | INPUT_LINEAR | INPUT_MIGHTBEDIGITAL, 0x00, 0xff); break;
		case 0x16/2: analog_value[offset & 3] = 0; break;
	}
}

static void alien3_custom_io_write(UINT32 offset, UINT16, UINT16)
{
	switch (offset)
	{
		case 0x10/2:
		case 0x14/2: {
			analog_value[offset & 3] = BurnGunReturnX((offset & 2) >> 1);
			break;
		}
		case 0x12/2:
		case 0x16/2: {
			analog_value[offset & 3] = BurnGunReturnY((offset & 2) >> 1);
			break;
		}
	}
}

// f1en, f1lap, radr, slipstrm
static void f1en_custom_io_write(UINT32 offset, UINT16, UINT16)
{
	switch (offset)
	{
		case 0x10/2: analog_value[offset & 3] = CURVE[ProcessAnalog(Analog[0], 0, INPUT_DEADZONE, 0x00, 0xff)]; break;
		case 0x12/2: analog_value[offset & 3] = ProcessAnalog(Analog[1], 0, INPUT_DEADZONE | INPUT_LINEAR | INPUT_MIGHTBEDIGITAL, 0x00, 0xff); break;
		case 0x14/2: analog_value[offset & 3] = ProcessAnalog(Analog[2], 0, INPUT_DEADZONE | INPUT_LINEAR | INPUT_MIGHTBEDIGITAL, 0x00, 0xff); break;
		case 0x16/2: analog_value[offset & 3] = 0; break;
	}
}

// radm: same as f1en_*, but with a steering accumulator to deal with buggy steering
// From d_ybrd.cpp: (same problem, different game)
// "Power Drift gets confused if there is a large change between steering
// values - making the car very difficult to control.  To fix this, we use
// a simple target/adder system to pseudo-interpolate the inbetween values
// during the frame.  PdriftMakeInputs() -- called at the start of the frame
// sets the target value."  -dink May 30, 2019

static void RadmAnalogTick()
{
	if (Radm_analog_adder > Radm_analog_target)
		Radm_analog_adder--;
	else if (Radm_analog_adder < Radm_analog_target)
		Radm_analog_adder++;
	else Radm_analog_adder = Radm_analog_target;
}

static void radm_custom_io_write(UINT32 offset, UINT16, UINT16)
{
	switch (offset)
	{
		case 0x10/2: analog_value[offset & 3] = Radm_analog_adder; break;
		case 0x12/2: analog_value[offset & 3] = ProcessAnalog(Analog[1], 0, INPUT_DEADZONE | INPUT_LINEAR | INPUT_MIGHTBEDIGITAL, 0x00, 0xff); break;
		case 0x14/2: analog_value[offset & 3] = ProcessAnalog(Analog[2], 0, INPUT_DEADZONE | INPUT_LINEAR | INPUT_MIGHTBEDIGITAL, 0x00, 0xff); break;
		case 0x16/2: analog_value[offset & 3] = 0; break;
	}

	RadmAnalogTick();
}

static void scross_custom_io_write(UINT32 offset, UINT16, UINT16)
{
	switch (offset)
	{
		case 0x10/2: analog_value[offset & 3] = ProcessAnalog(Analog[0], 1, INPUT_DEADZONE, 0x00, 0xff); break;
		case 0x12/2: analog_value[offset & 3] = ProcessAnalog(Analog[1], 0, INPUT_DEADZONE | INPUT_LINEAR | INPUT_MIGHTBEDIGITAL, 0x00, 0xff); break;
		case 0x14/2: analog_value[offset & 3] = ProcessAnalog(Analog[2], 1, INPUT_DEADZONE, 0x00, 0xff); break;
		case 0x16/2: analog_value[offset & 3] = ProcessAnalog(Analog[3], 0, INPUT_DEADZONE | INPUT_LINEAR | INPUT_MIGHTBEDIGITAL, 0x00, 0xff); break;
	}
}

static void jpark_custom_io_write(UINT32 offset, UINT16, UINT16)
{
	// jpark has 1/2 speed zones to deal with how the guns are placed on the cab.
	// It makes the movement seem linear on-cab, but makes for feisty emulation. -dink
	switch (offset)
	{
		case 0x10/2:   // p1: 3f - 90 - c0.  3f-90 = 1/2 speed zone
		case 0x14/2: { // p2: 3f - 70 - c0.  70-c0 = 1/2 speed zone
			INT32 Player = (offset & 2) >> 1;
			INT32 Sz[2][2] = { {0x55, 0x90}, {0xaa, 0x70} };
			INT32 t = BurnGunReturnX(Player);

			if (t >= Sz[Player][0]) {
				analog_value[offset & 3] = scalerange(t, Sz[Player][0], 0xff, Sz[Player][1], 0xc1);
			} else {
				analog_value[offset & 3] = scalerange(t, 0x00, Sz[Player][0], 0x3f, Sz[Player][1]);
			}
		}
		break;

		case 0x12/2:
		case 0x16/2:
			analog_value[offset & 3] = BurnGunReturnY((offset&2)>>1);
			analog_value[offset & 3] = scalerange(analog_value[offset & 3], 0x00, 0xff, 0x3f, 0xc1);
			break;
	}
}

static void mirror_this(UINT8 *rom, UINT32 length, UINT32 space)
{
	if (length >= space) return;
	INT32 t = 0;
	for (INT32 i = 0; i < 32; i++) {
		if (length & (1 << i)) t++;
	}
	if (t > 1) return; // sonic

	for (UINT32 i = length; i < space; i += length) {
		memmove (rom + i, rom, length);
	}
}

static INT32 DrvLoadRoms(bool bLoad)
{
	char* pRomName;
	struct BurnRomInfo ri;
	UINT8 *pLoad[8] = { DrvV60ROM, DrvV60ROM + 0x100000, DrvZ80ROM, DrvV25ROM, DrvGfxROM[0], DrvGfxROM[1], DrvPCMROM };

	if (bLoad) {
		memset(DrvGfxROM[1], 0xff, graphics_length[1]); // 0xff fill empty space! (sprite region size > sprite rom size)
	}

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++)
	{
		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & BRF_PRG) && (ri.nType & 7) == 1)
		{
			if (bLoad) {
				bprintf (0, _T("PRG0: %5.5x, %d\n"), pLoad[0] - DrvV60ROM, i);
				if (BurnLoadRom(pLoad[0], i, 1)) return 1;
				mirror_this(pLoad[0], ri.nLen, 0x100000 - (pLoad[0] - DrvV60ROM));
			}
			pLoad[0] += 0x080000;
			continue;
		}

		if ((ri.nType & BRF_PRG) && (ri.nType & 7) == 2)
		{
			if (bLoad) {
				bprintf (0, _T("PRG1: %5.5x, %d\n"), pLoad[1] - DrvV60ROM, i);
				if (BurnLoadRom(pLoad[1]+0, i+0, 2)) return 1;
				if (BurnLoadRom(pLoad[1]+1, i+1, 2)) return 1;
				mirror_this(pLoad[1], ri.nLen * 2, 0x100000);
			}
			i++;
			pLoad[1] += 0x100000; // not necessary
			continue;
		}

		if ((ri.nType & BRF_PRG) && (ri.nType & 7) == 5) // v70 code loaded this way
		{
			if (bLoad) {
				bprintf (0, _T("PRG1: %5.5x, %d\n"), pLoad[1] - DrvV60ROM, i);
				if (BurnLoadRomExt(pLoad[0]+0, i+0, 4, LD_GROUP(2))) return 1;
				if (BurnLoadRomExt(pLoad[0]+2, i+1, 4, LD_GROUP(2))) return 1;
				mirror_this(pLoad[0], ri.nLen * 2, 0x100000);
			}
			i++;
			pLoad[0] += 0x100000;
			continue;
		}

		if ((ri.nType & BRF_PRG) && (ri.nType & 7) == 3)
		{
			if (bLoad) {
				bprintf (0, _T("Z80 : %5.5x, %d\n"), pLoad[2] - DrvZ80ROM, i);
				if (BurnLoadRom(pLoad[2], i, 1)) return 1;
				mirror_this(pLoad[2], ri.nLen, 0x100000);
			}
			pLoad[2] += 0x100000;
			continue;
		}

		if ((ri.nType & BRF_PRG) && (ri.nType & 7) == 4) // v25
		{
			if (bLoad) {
				bprintf (0, _T("V25 : %5.5x, %d\n"), pLoad[3] - DrvV25ROM, i);
				if (BurnLoadRom(pLoad[3], i, 1)) return 1;
			}
			pLoad[3] += ri.nLen; // not necessary
			continue;
		}

		if ((ri.nType & BRF_PRG) && (ri.nType & 7) == 7) // eeprom
		{
			if (bLoad) {
				bprintf (0, _T("EEPROM : %d\n"), i);
				if (BurnLoadRom(DrvEEPROM, i, 1)) return 1;
			}
			continue;
		}

		if ((ri.nType & BRF_GRA) && (ri.nType & 7) == 1)
		{
			if (bLoad) {
				bprintf (0, _T("GFX0A: %5.5x, %d\n"), pLoad[4] - DrvGfxROM[0], i);
				if (BurnLoadRomExt(pLoad[4] + 0, i+0, 4, LD_GROUP(2))) return 1;
				if (BurnLoadRomExt(pLoad[4] + 2, i+1, 4, LD_GROUP(2))) return 1;
			}
			i++;
			pLoad[4] += ri.nLen * 2;
			continue;
		}

		if ((ri.nType & BRF_GRA) && (ri.nType & 7) == 2)
		{
			if (bLoad) {
				bprintf (0, _T("GFX1A: %5.5x, %d\n"), pLoad[5] - DrvGfxROM[1], i);
				if (BurnLoadRomExt(pLoad[5] + 2, i+0, 8, LD_GROUP(2) | LD_BYTESWAP)) return 1;
				if (BurnLoadRomExt(pLoad[5] + 0, i+1, 8, LD_GROUP(2) | LD_BYTESWAP)) return 1;
				if (BurnLoadRomExt(pLoad[5] + 6, i+2, 8, LD_GROUP(2) | LD_BYTESWAP)) return 1;
				if (BurnLoadRomExt(pLoad[5] + 4, i+3, 8, LD_GROUP(2) | LD_BYTESWAP)) return 1;
			}
			i+=3;
			pLoad[5] += is_scross ? 0x800000 : (ri.nLen * 4);
			continue;
		}

		if ((ri.nType & BRF_GRA) && (ri.nType & 7) == 3)
		{
			if (bLoad) {
				bprintf (0, _T("GFX0B: %5.5x, %d\n"), pLoad[4] - DrvGfxROM[0], i);
				if (BurnLoadRom(pLoad[4] + 0, i+0, 4)) return 1;
				if (BurnLoadRom(pLoad[4] + 1, i+1, 4)) return 1;
				if (BurnLoadRom(pLoad[4] + 2, i+2, 4)) return 1;
				if (BurnLoadRom(pLoad[4] + 3, i+3, 4)) return 1;
			}
			i+=3;
			pLoad[4] += ri.nLen * 4;
			continue;
		}

		if ((ri.nType & BRF_GRA) && (ri.nType & 7) == 4)
		{
			if (bLoad) {
				bprintf (0, _T("GFX1B: %5.5x, %d\n"), pLoad[5] - DrvGfxROM[1], i);
				for (INT32 j = 0; j < 8; j++) {
					if (BurnLoadRom(pLoad[5] + (j ^ 3), i+j, 8)) return 1;
				}
			}
			i+=7;
			pLoad[5] += ri.nLen * 8;
			bprintf(0, _T("1b: loaded %x\n"), ri.nLen * 8);
			continue;
		}

		if ((ri.nType & BRF_SND) && (ri.nType & 7) == 1)
		{
			if (bLoad) {
				bprintf (0, _T("SND: %5.5x, %d\n"), pLoad[6] - DrvPCMROM, i);
				if (BurnLoadRom(pLoad[6], i, 1)) return 1;
			}
			pLoad[6] += 0x200000;
			continue;
		}
	}

	if (bLoad) {
		if (graphics_length[0] == 0x200) memset (DrvGfxROM[0], 0xff, 0x100); // holo

		BurnNibbleExpand(DrvGfxROM[0], NULL, graphics_length[0] / 2, 0, 0);
	} else {
		graphics_length[0] = (pLoad[4] - DrvGfxROM[0]) * 2;
		if (graphics_length[0] == 0) { // holo
			graphics_length[0] = 0x200;
		}
		graphics_length[1] = pLoad[5] - DrvGfxROM[1];
		if (sprite_length != 0) { // some games have a fixed region size that is greater than the romsize, this is important for proper sprite banking. -dink
			graphics_length[1] = sprite_length;
		}
		bprintf (0, _T("Graphics len: %5.5x, %5.5x\n"), graphics_length[0], graphics_length[1]);
	}

	return 0;
}

static void system32_v60_map()
{
	v60Init();
	v60Open(0);
	v60MapMemory(DrvV60ROM,			0x000000, 0x1fffff, MAP_ROM);
	for (INT32 i = 0; i < 0x100000; i+=0x10000) {
		v60MapMemory(DrvV60RAM,			0x200000 + i, 0x20ffff + i, MAP_RAM);
	}
	for (INT32 i = 0; i < 0x100000; i+=0x20000) { // mapped in handler
//		v60MapMemory(DrvVidRAM,			0x300000 + i, 0x31ffff + i, MAP_RAM);
	}
	for (INT32 i = 0; i < 0x100000; i+=0x20000) {
		v60MapMemory(DrvSprRAM,			0x400000 + i, 0x41ffff + i, MAP_ROM); // writes in handler
	}
	v60MapMemory(DrvCommsRAM,		0x810000, 0x810fff, MAP_ROM); // writes in handler
	v60MapMemory(DrvV60ROM,			0xf00000, 0xffffff, MAP_ROM);
	v60SetWriteWordHandler(system32_main_write_word);
	v60SetWriteByteHandler(system32_main_write_byte);
	v60SetReadWordHandler(system32_main_read_word);
	v60SetReadByteHandler(system32_main_read_byte);
	v60SetIRQCallback(irq_callback);
	v60Close();

	EEPROMInit(&eeprom_interface_93C46); // initialize eeprom too.
}

static void system32_v70_map()
{
	v70Init();
	v60Open(0);
	v60SetAddressMask(0xffffff);
	v60MapMemory(DrvV60ROM,			0x000000, 0x1fffff, MAP_ROM);
	for (INT32 i = 0; i < 0x100000; i+=0x20000) {
		v60MapMemory(DrvV60RAM,			0x200000 + i, 0x21ffff + i, MAP_RAM);
	}
	for (INT32 i = 0; i < 0x100000; i+=0x20000) { // mapped in handler
//		v60MapMemory(DrvVidRAM,			0x300000 + i, 0x31ffff + i, MAP_RAM);
	}
	for (INT32 i = 0; i < 0x100000; i+=0x20000) {
		v60MapMemory(DrvSprRAM,			0x400000 + i, 0x41ffff + i, MAP_ROM); // writes in handler
	}
	v60MapMemory(DrvCommsRAM,		0x810000, 0x810fff, MAP_ROM); // writes in handler
	v60MapMemory(DrvV60ROM,			0xf00000, 0xffffff, MAP_ROM);
	v60SetWriteLongHandler(system32_main_write_long);
	v60SetWriteWordHandler(system32_main_write_word);
	v60SetWriteByteHandler(system32_main_write_byte);
	v60SetReadLongHandler(system32_main_read_long);
	v60SetReadWordHandler(system32_main_read_word);
	v60SetReadByteHandler(system32_main_read_byte);
	v60SetIRQCallback(irq_callback);
	v60Close();

	EEPROMInit(&eeprom_interface_93C46); // initialize eeprom too.
}

static void system32_sound_init()
{
	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,				0x0000, 0x9fff, MAP_ROM);
	ZetMapMemory(DrvShareRAM,			0xe000, 0xffff, MAP_RAM);
	ZetSetWriteHandler(system32_sound_write);
	ZetSetReadHandler(system32_sound_read);
	ZetSetOutHandler(system32_sound_write_port);
	ZetSetInHandler(system32_sound_read_port);
	ZetClose();

	BurnYM3438Init(2, 8053975, &DrvFMIRQHandler, 0);
	BurnTimerAttach(&ZetConfig, 8053975);
	BurnYM3438SetAllRoutes(0, 0.20, BURN_SND_ROUTE_BOTH);
	BurnYM3438SetAllRoutes(1, 0.20, BURN_SND_ROUTE_BOTH);

	RF5C68PCMInit(12500000, ZetTotalCycles, 8053975, 1);
	RF5C68PCMSetAllRoutes(0.55, BURN_SND_ROUTE_BOTH);
}

static void multi32_sound_init()
{
	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x9fff, MAP_ROM);
	ZetMapMemory(DrvShareRAM,	0xe000, 0xffff, MAP_RAM);
	ZetSetWriteHandler(multi32_sound_write);
	ZetSetReadHandler(multi32_sound_read);
	ZetSetOutHandler(multi32_sound_write_port);
	ZetSetInHandler(multi32_sound_read_port);
	ZetClose();

	BurnYM3438Init(1, 8053975, &DrvFMIRQHandler, 0);
	BurnTimerAttach(&ZetConfig, 8053975);
	BurnYM3438SetAllRoutes(0, 0.40, BURN_SND_ROUTE_BOTH);

	MultiPCMInit(32215900 / 4, DrvPCMROM, 1);
	MultiPCMSetVolume(0.35);
}

static void v25_prot_write(UINT32 offset, UINT32 data, UINT32 /*mask*/)
{
	DrvV25RAM[offset] = data;
}

static UINT16 v25_prot_read(UINT32 offset)
{
	return DrvV25RAM[offset] | (DrvV25RAM[offset] << 8);
}

static void v25_protection_init(UINT8 *table)
{
	VezInit(0, V25_TYPE, 10000000);
	VezOpen(0);
	VezMapMemory(DrvV25ROM, 0x00000, 0x0ffff, MAP_ROM);
	VezMapMemory(DrvV25RAM, 0x10000, 0x1ffff, MAP_RAM);
	VezMapMemory(DrvV25ROM, 0xf0000, 0xfffff, MAP_ROM);
	VezSetDecode(table);
	VezClose();

	decrypt_protrom();

	protection_a00000_write = v25_prot_write;
	protection_a00000_read = v25_prot_read;

	use_v25 = 1;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	if (has_gun) {
		BurnGunExit();
	}
	if (is_sonic) {
		BurnTrackballExit();
	}
	v60Exit();
	if (use_v25) VezExit();
	ZetExit();
	BurnYM3438Exit();
	if (is_multi32) {
		MultiPCMExit();
	} else {
		RF5C68PCMExit();
	}
	EEPROMExit();

	BurnShiftExit();

	use_v25 = 0;
	protection_a00000_write = NULL;
	protection_a00000_read = NULL;
	memory_protection_read = NULL;
	memory_protection_write = NULL;
	custom_io_read_0 = NULL;
	custom_io_write_0 = NULL;
	system32_prot_vblank = NULL;
	BurnFreeMemIndex();

	screen_vflip = 0;
	is_multi32 = 0;
	is_radm = 0;
	is_radr = 0;
	is_scross = 0;
	is_sonic = 0;
	is_slipstrm = 0;
	is_ga2_spidman = 0;
	has_gun = 0;
	fake_wide_screen = 0;
	can_modechange = 0;
	clr_opposites = 0;

	opaquey_hack = 0;

	CURVE = NULL;

	return 0;
}

#define MIXER_LAYER_TEXT        0
#define MIXER_LAYER_NBG0        1
#define MIXER_LAYER_NBG1        2
#define MIXER_LAYER_NBG2        3
#define MIXER_LAYER_NBG3        4
#define MIXER_LAYER_BITMAP      5
#define MIXER_LAYER_SPRITES     6
#define MIXER_LAYER_BACKGROUND  7
#define MIXER_LAYER_SPRITES_2   8   /* semi-kludge to have a frame buffer for sprite backlayer */
#define MIXER_LAYER_MULTISPR    9
#define MIXER_LAYER_MULTISPR_2  10


static INT32 compute_clipping_extents(INT32 enable, INT32 clipout, INT32 clipmask, const clip_struct &cliprect, extents_list *list)
{
	UINT16 *m_videoram = (UINT16*)DrvVidRAM;

	INT32 flip = (BURN_ENDIAN_SWAP_INT16(m_videoram[0x1ff00/2]) >> 9) & 1;
	clip_struct tempclip;
	clip_struct clips[5];
	INT32 sorted[5];
	INT32 i, j, y;

	/* expand our cliprect to exclude the bottom-right */
	tempclip = cliprect;
	tempclip.nMaxx++;
	tempclip.nMaxy++;

	/* create the 0th entry */
	list->extent[0][0] = tempclip.nMinx;
	list->extent[0][1] = tempclip.nMaxx;

	/* simple case if not enabled */
	if (!enable)
	{
		memset(&list->scan_extent[tempclip.nMiny], 0, sizeof(list->scan_extent[0]) * (tempclip.nMaxy - tempclip.nMiny));
		return 1;
	}

	/* extract the from videoram into locals, and apply the cliprect */
	for (i = 0; i < 5; i++)
	{
		if (!flip)
		{
			clips[i].nMinx = BURN_ENDIAN_SWAP_INT16(m_videoram[0x1ff60/2 + i * 4]) & 0x1ff;
			clips[i].nMiny = BURN_ENDIAN_SWAP_INT16(m_videoram[0x1ff62/2 + i * 4]) & 0x0ff;
			clips[i].nMaxx = (BURN_ENDIAN_SWAP_INT16(m_videoram[0x1ff64/2 + i * 4]) & 0x1ff) + 1;
			clips[i].nMaxy = (BURN_ENDIAN_SWAP_INT16(m_videoram[0x1ff66/2 + i * 4]) & 0x0ff) + 1;
		}
		else
		{
			clip_struct visarea;
			visarea.nMinx = 0;
			visarea.nMaxx = ((nScreenWidth) - 1);
			visarea.nMiny = 0;
			visarea.nMaxy = ((nScreenHeight) - 1);

			clips[i].nMaxx = (visarea.nMaxx + 1) - (BURN_ENDIAN_SWAP_INT16(m_videoram[0x1ff60/2 + i * 4]) & 0x1ff);
			clips[i].nMaxy = (visarea.nMaxy + 1) - (BURN_ENDIAN_SWAP_INT16(m_videoram[0x1ff62/2 + i * 4]) & 0x0ff);
			clips[i].nMinx = (visarea.nMaxx + 1) - ((BURN_ENDIAN_SWAP_INT16(m_videoram[0x1ff64/2 + i * 4]) & 0x1ff) + 1);
			clips[i].nMiny = (visarea.nMaxy + 1) - ((BURN_ENDIAN_SWAP_INT16(m_videoram[0x1ff66/2 + i * 4]) & 0x0ff) + 1);
		}

		if (clips[i].nMiny < tempclip.nMiny) clips[i].nMiny = tempclip.nMiny;
		if (clips[i].nMaxy > tempclip.nMaxy) clips[i].nMaxy = tempclip.nMaxy;
		if (clips[i].nMinx < tempclip.nMinx) clips[i].nMinx = tempclip.nMinx;
		if (clips[i].nMaxx > tempclip.nMaxx) clips[i].nMaxx = tempclip.nMaxx;
		sorted[i] = i;
	}

	/* bubble sort them by nMinx */
	for (i = 0; i < 5; i++)
		for (j = i + 1; j < 5; j++)
			if (clips[sorted[i]].nMinx > clips[sorted[j]].nMinx) { INT32 temp = sorted[i]; sorted[i] = sorted[j]; sorted[j] = temp; }

	/* create all valid extent combinations */
	for (i = 1; i < 32; i++)
		if (i & clipmask)
		{
			UINT16 *extent = &list->extent[i][0];

			/* start off with an entry at tempclip.nMinx */
			*extent++ = tempclip.nMinx;

			/* loop in sorted order over extents */
			for (j = 0; j < 5; j++)
				if (i & (1 << sorted[j]))
				{
					const clip_struct &cur = clips[sorted[j]];

					/* see if this intersects our last extent */
					if (extent != &list->extent[i][1] && cur.nMinx <= extent[-1])
					{
						if (cur.nMaxx > extent[-1])
							extent[-1] = cur.nMaxx;
					}

					/* otherwise, just append to the list */
					else
					{
						*extent++ = cur.nMinx;
						*extent++ = cur.nMaxx;
					}
				}

			/* append an ending entry */
			*extent++ = tempclip.nMaxx;
		}

	/* loop over scanlines and build extents */
	for (y = tempclip.nMiny; y < tempclip.nMaxy; y++)
	{
		INT32 sect = 0;

		/* figure out all the clips that intersect this scanline */
		for (i = 0; i < 5; i++)
			if ((clipmask & (1 << i)) && y >= clips[i].nMiny && y < clips[i].nMaxy)
				sect |= 1 << i;
		list->scan_extent[y] = sect;
	}

	return clipout;
}

static INT32 find_cache_entry(INT32 page, INT32 bank)
{
	cache_entry *entry, *prev;

	prev = NULL;
	entry = cache_head;

	while (1)
	{
		if (entry->page == page && entry->bank == bank)
		{
			if (prev)
			{
				prev->next = entry->next;
				entry->next = cache_head;
				cache_head = entry;
			}
			return entry->tmap;
		}

		if (entry->next == NULL)
			break;
		prev = entry;
		entry = entry->next;
	}

	entry->page = page;
	entry->bank = bank;
	entry->dirty = 1;
	GenericTilemapAllTilesDirty(entry->tmap);

	prev->next = entry->next;
	entry->next = cache_head;
	cache_head = entry;

	return entry->tmap;
}

static void compute_tilemap_flips(INT32 bgnum, INT32 &flipx, INT32 &flipy)
{
	UINT16 *ram = (UINT16*)DrvVidRAM;

	// determine flip bits
	INT32 global_flip    = (BURN_ENDIAN_SWAP_INT16(ram[0x1ff00 / 2]) >> 9) & 1;
	INT32 layer_flip     = (BURN_ENDIAN_SWAP_INT16(ram[0x1ff00 / 2]) >> bgnum) & 1;
	INT32 prohibit_flipy = (BURN_ENDIAN_SWAP_INT16(ram[0x1ff00 / 2]) >> 8) & 1;

	flipx = (layer_flip) ? !global_flip : global_flip;

	flipy = (layer_flip && !prohibit_flipy) ? !global_flip : global_flip;
}

static void get_tilemaps(INT32 bgnum, INT32 *tilemaps)
{
	INT32 tilebank, page;

	UINT16 *ram = (UINT16*)DrvVidRAM;

	/* determine the current tilebank */
	if (is_multi32)
		tilebank = (system32_tilebank_external >> (2*bgnum)) & 3;
	else
		tilebank = ((system32_tilebank_external & 1) << 1) | ((BURN_ENDIAN_SWAP_INT16(ram[0x1ff00/2]) & 0x400) >> 10);

	page = (BURN_ENDIAN_SWAP_INT16(ram[0x1ff40/2 + 2 * bgnum + 0]) >> 0) & 0x7f;
	tilemaps[0] = find_cache_entry(page, tilebank);

	page = (BURN_ENDIAN_SWAP_INT16(ram[0x1ff40/2 + 2 * bgnum + 0]) >> 8) & 0x7f;
	tilemaps[1] = find_cache_entry(page, tilebank);

	page = (BURN_ENDIAN_SWAP_INT16(ram[0x1ff40/2 + 2 * bgnum + 1]) >> 0) & 0x7f;
	tilemaps[2] = find_cache_entry(page, tilebank);

	page = (BURN_ENDIAN_SWAP_INT16(ram[0x1ff40/2 + 2 * bgnum + 1]) >> 8) & 0x7f;
	tilemaps[3] = find_cache_entry(page, tilebank);
}


static void update_tilemap_zoom(clip_struct cliprect, UINT16 *ram, INT32 destbmp, INT32 bgnum)
{
	INT32 tilemaps[4];

	get_tilemaps(bgnum, tilemaps);

	for (INT32 i = 0; i < 4; i++) {
		tilemap_cache = &tmap_cache[tilemaps[i]];

		if (tilemap_cache->dirty) {
			tilemap_cache->dirty = 0;
			GenericTilemapDraw(tilemap_cache->tmap, 32 + tilemap_cache->tmap, 0);
		}
	}

	INT32 opaque = 0;
	INT32 flipx, flipy;

	// todo determine flipping
	compute_tilemap_flips(bgnum, flipx, flipy);

	INT32 clipenable = (BURN_ENDIAN_SWAP_INT16(ram[0x1ff02/2]) >> (11 + bgnum)) & 1;
	INT32 clipout = (BURN_ENDIAN_SWAP_INT16(ram[0x1ff02/2]) >> (6 + bgnum)) & 1;
	INT32 clips = (BURN_ENDIAN_SWAP_INT16(ram[0x1ff06/2]) >> (4 * bgnum)) & 0x0f;
	extents_list clip_extents;
	INT32 clipdraw_start = compute_clipping_extents(clipenable, clipout, clips, cliprect, &clip_extents);

	INT32 dstxstep = BURN_ENDIAN_SWAP_INT16(ram[0x1ff50/2 + 2 * bgnum]) & 0xfff;
	INT32 dstystep;
	if (BURN_ENDIAN_SWAP_INT16(ram[0x1ff00/2]) & 0x4000)
		dstystep = BURN_ENDIAN_SWAP_INT16(ram[0x1ff52/2 + 2 * bgnum]) & 0xfff;
	else
		dstystep = dstxstep;

	/* clamp the zoom factors */
	if (dstxstep < 0x80)
		dstxstep = 0x80;
	if (dstystep < 0x80)
		dstystep = 0x80;

	UINT32 srcxstep = (0x200 << 20) / dstxstep;
	UINT32 srcystep = (0x200 << 20) / dstystep;

	UINT32 srcx_start = (BURN_ENDIAN_SWAP_INT16(ram[0x1ff12/2 + 4 * bgnum]) & 0x3ff) << 20;
	srcx_start += (BURN_ENDIAN_SWAP_INT16(ram[0x1ff10/2 + 4 * bgnum]) & 0xff00) << 4;
	UINT32 srcy = (BURN_ENDIAN_SWAP_INT16(ram[0x1ff16/2 + 4 * bgnum]) & 0x1ff) << 20;
	srcy += (BURN_ENDIAN_SWAP_INT16(ram[0x1ff14/2 + 4 * bgnum]) & 0xfe00) << 4;

	srcx_start -= sign_extend(BURN_ENDIAN_SWAP_INT16(ram[0x1ff30/2 + 2 * bgnum]), (dstxstep != 0x200) ? 10 : 9) * srcxstep;
	srcy -= sign_extend(BURN_ENDIAN_SWAP_INT16(ram[0x1ff32/2 + 2 * bgnum]), (dstystep != 0x200) ? 10 : 9) * srcystep;

	srcx_start += cliprect.nMinx * srcxstep;
	srcy += cliprect.nMiny * srcystep;

	if (flipy)
	{
	//	const rectangle &visarea = screen.visible_area();

		srcy += (cliprect.nMaxy - 2 * cliprect.nMiny) * srcystep;
		srcystep = -srcystep;
	}

	if (flipx)
	{
	//	const rectangle &visarea = screen.visible_area();

		srcx_start += (cliprect.nMaxx - 2 * cliprect.nMinx) * srcxstep;
		srcxstep = -srcxstep;
	}

	for (INT32 y = cliprect.nMiny; y <= cliprect.nMaxy; y++)
	{
		UINT16 const *extents = &clip_extents.extent[clip_extents.scan_extent[y]][0];
		UINT16 *const dst = BurnBitmapGetPosition(destbmp+5, 0, y);
		INT32 clipdraw = clipdraw_start;

		if (clipdraw || extents[1] <= cliprect.nMaxx)
		{
			INT32 transparent = 0;

			UINT16 const *tm0 = BurnBitmapGetBitmap(tmap_cache[tilemaps[(((srcy >> 27) & 2) + 0)]].tmap + 32 );
			UINT16 const *tm1 = BurnBitmapGetBitmap(tmap_cache[tilemaps[(((srcy >> 27) & 2) + 1)]].tmap + 32 );
			UINT16 const *src[2] = { &tm0[((srcy >> 20) & 0xff) * 512], &tm1[((srcy >> 20) & 0xff) * 512] };

			UINT32 srcx = srcx_start;
			while (1)
			{
				if (clipdraw)
				{
					for (INT32 x = extents[0]; x < extents[1]; x++)
					{
						UINT16 pix = src[(srcx >> 29) & 1][(srcx >> 20) & 0x1ff];
						srcx += srcxstep;
						if ((pix & 0x0f) == 0 && !opaque)
							pix = 0, transparent++;
						dst[x] = BURN_ENDIAN_SWAP_INT16(pix);
					}
				}
				else
				{
					INT32 pixels = extents[1] - extents[0];
					memset(&dst[extents[0]], 0, pixels * sizeof(dst[0]));
					transparent += pixels;
					srcx += srcxstep * pixels;
				}

				if (extents[1] > cliprect.nMaxx)
					break;

				clipdraw = !clipdraw;
				extents++;
			}
			transparent_check[destbmp+5][y] = (transparent == cliprect.nMaxx - cliprect.nMinx + 1);
		}
		else
			transparent_check[destbmp+5][y] = 1;

		srcy += srcystep;
	}
}

static void update_tilemap_rowscroll(clip_struct cliprect, UINT16 *m_videoram, INT32 destbmp, INT32 bgnum)
{
	INT32 tilemaps[4];
	get_tilemaps(bgnum, tilemaps);

	for (INT32 i = 0; i < 4; i++) {
		tilemap_cache = &tmap_cache[tilemaps[i]];

		if (tilemap_cache->dirty) {
			tilemap_cache->dirty = 0;
			GenericTilemapDraw(tilemap_cache->tmap, 32 + tilemap_cache->tmap, 0);
		}
	}

	INT32 opaque = (opaquey_hack) ? ((m_videoram[0x1ff8e/2] >> (8 + bgnum)) & 1) : 0;
	INT32 flipx, flipy;

	compute_tilemap_flips(bgnum, flipx, flipy);

	INT32 clipenable = (BURN_ENDIAN_SWAP_INT16(m_videoram[0x1ff02/2]) >> (11 + bgnum)) & 1;
	INT32 clipout = (BURN_ENDIAN_SWAP_INT16(m_videoram[0x1ff02/2]) >> (6 + bgnum)) & 1;
	INT32 clips = (BURN_ENDIAN_SWAP_INT16(m_videoram[0x1ff06/2]) >> (4 * bgnum)) & 0x0f;
	extents_list clip_extents;
	INT32 clipdraw_start = compute_clipping_extents(clipenable, clipout, clips, cliprect, &clip_extents);

	INT32 rowscroll = (BURN_ENDIAN_SWAP_INT16(m_videoram[0x1ff04/2]) >> (bgnum - 2)) & 1;
	INT32 rowselect = (BURN_ENDIAN_SWAP_INT16(m_videoram[0x1ff04/2]) >> bgnum) & 1;
	if ((BURN_ENDIAN_SWAP_INT16(m_videoram[0x1ff04/2]) >> (bgnum + 2)) & 1)
		rowscroll = rowselect = 0;

	UINT16 const *const table = &m_videoram[(BURN_ENDIAN_SWAP_INT16(m_videoram[0x1ff04/2]) >> 10) * 0x400];

	INT32 xscroll = (BURN_ENDIAN_SWAP_INT16(m_videoram[0x1ff12/2 + 4 * bgnum]) & 0x3ff) - (BURN_ENDIAN_SWAP_INT16(m_videoram[0x1ff30/2 + 2 * bgnum]) & 0x1ff);
	INT32 yscroll = (BURN_ENDIAN_SWAP_INT16(m_videoram[0x1ff16/2 + 4 * bgnum]) & 0x1ff);

	for (INT32 y = cliprect.nMiny; y <= cliprect.nMaxy; y++)
	{
		UINT16 const *extents = &clip_extents.extent[clip_extents.scan_extent[y]][0];
		UINT16 *const dst = BurnBitmapGetPosition(destbmp+5, 0, y);
		INT32 clipdraw = clipdraw_start;

		/* optimize for the case where we are clipped out */
		if (clipdraw || extents[1] <= cliprect.nMaxx)
		{
			INT32 transparent = 0;
			INT32 srcxstep;

			/* if we're not flipped, things are straightforward */
			INT32 srcx;
			if (!flipx)
			{
				srcx = cliprect.nMinx + xscroll;
				srcxstep = 1;
			}
			else
			{
				srcx = cliprect.nMaxx + xscroll;
				srcxstep = -1;
			}

			INT32 srcy;
			INT32 ylookup;
			if (!flipy)
			{
				srcy = yscroll + y;
				ylookup = y;
			}
			else
			{
				srcy = yscroll + cliprect.nMaxy /*visarea.nMaxy*/ - y;
				ylookup = cliprect.nMaxy - y;
			}

			/* apply row scroll/select */
			if (rowscroll)
				srcx += BURN_ENDIAN_SWAP_INT16(table[0x000 + 0x100 * (bgnum - 2) + ylookup]) & 0x3ff;
			if (rowselect)
				srcy = (yscroll + BURN_ENDIAN_SWAP_INT16(table[0x200 + 0x100 * (bgnum - 2) + ylookup])) & 0x1ff;

			/* look up the pages and get their source pixmaps */
			UINT16 const *tm0 = BurnBitmapGetBitmap(tmap_cache[tilemaps[((srcy >> 7) & 2) + 0]].tmap + 32 );
			UINT16 const *tm1 = BurnBitmapGetBitmap(tmap_cache[tilemaps[((srcy >> 7) & 2) + 1]].tmap + 32 );
			UINT16 const *src[2] = { &tm0[(srcy & 0xff) * 512], &tm1[(srcy & 0xff) * 512] };

			/* loop over extents */
			while (1)
			{
				/* if we're drawing on this extent, draw it */
				if (clipdraw)
				{
					for (INT32 x = extents[0]; x < extents[1]; x++, srcx += srcxstep)
					{
						UINT16 pix = src[(srcx >> 9) & 1][srcx & 0x1ff];
						if ((pix & 0x0f) == 0 && !opaque)
							pix = 0, transparent++;
						dst[x] = BURN_ENDIAN_SWAP_INT16(pix);
					}
				}

				/* otherwise, clear to zero */
				else
				{
					INT32 pixels = extents[1] - extents[0];
					memset(&dst[extents[0]], 0, pixels * sizeof(dst[0]));
					transparent += pixels;
					srcx += srcxstep * pixels;
				}

				/* stop at the end */
				if (extents[1] > cliprect.nMaxx)
					break;

				/* swap states and advance to the next extent */
				clipdraw = !clipdraw;
				extents++;
			}

			transparent_check[destbmp+5][y] = (transparent == cliprect.nMaxx - cliprect.nMinx + 1);
		}
		else
			transparent_check[destbmp+5][y] = 1;
	}
}

static void update_tilemap_text(clip_struct cliprect, UINT16 *ram, INT32 destbmp)
{
	INT32 width, height;

	INT32 flip = (BURN_ENDIAN_SWAP_INT16(ram[0x1ff00/2]) >> 9) & 1;

	UINT16 const *const tilebase = &ram[((BURN_ENDIAN_SWAP_INT16(ram[0x1ff5c/2]) >> 4) & 0x1f) * 0x800];
	UINT16 const *const gfxbase = &ram[(BURN_ENDIAN_SWAP_INT16(ram[0x1ff5c/2]) & 7) * 0x2000];

	BurnBitmapGetDimensions(destbmp+5, &width, &height);

	/* compute start/end tile numbers */
	INT32 startx = cliprect.nMinx / 8;
	INT32 starty = cliprect.nMiny / 8;
	INT32 endx = cliprect.nMaxx / 8;
	INT32 endy = cliprect.nMaxy / 8;

	// sonic and dbzvrvs boot up in 416px mode to display a disclaimer, then
	// switch to 320px for the game.  If we move the text over 45px, it will
	// look perfect and avoid having to switch modes. -dink
	INT32 wide_offs = (fake_wide_screen) ? 5 : 0; // "disclaimer" text offset (5*8)+5
	/* loop over tiles */
	for (INT32 y = starty; y <= endy; y++)
		for (INT32 x = startx; x <= endx; x++)
		{
			INT32 tile = BURN_ENDIAN_SWAP_INT16(tilebase[y * 64 + (x + wide_offs)]);
			UINT16 const *src = &gfxbase[(tile & 0x1ff) * 16];
			INT32 color = (tile & 0xfe00) >> 5;

			/* non-flipped case */
			if (!flip)
			{
				UINT16 *dst = BurnBitmapGetPosition(destbmp+5, (x > 1) ? ((x * 8) - (wide_offs * fake_wide_screen)) : (x * 8), y * 8);

				/* loop over rows */
				for (INT32 iy = 0; iy < 8; iy++)
				{
					INT32 pixels = BURN_ENDIAN_SWAP_INT16(*src++);
					INT32 pix;

					pix = (pixels >> 4) & 0x0f;
					if (pix)
						pix |= color;
					dst[0] = BURN_ENDIAN_SWAP_INT16(pix);

					pix = (pixels >> 0) & 0x0f;
					if (pix)
						pix |= color;
					dst[1] = BURN_ENDIAN_SWAP_INT16(pix);

					pix = (pixels >> 12) & 0x0f;
					if (pix)
						pix |= color;
					dst[2] = BURN_ENDIAN_SWAP_INT16(pix);

					pix = (pixels >> 8) & 0x0f;
					if (pix)
						pix |= color;
					dst[3] = BURN_ENDIAN_SWAP_INT16(pix);

					pixels = BURN_ENDIAN_SWAP_INT16(*src++);

					pix = (pixels >> 4) & 0x0f;
					if (pix)
						pix |= color;
					dst[4] = BURN_ENDIAN_SWAP_INT16(pix);

					pix = (pixels >> 0) & 0x0f;
					if (pix)
						pix |= color;
					dst[5] = BURN_ENDIAN_SWAP_INT16(pix);

					pix = (pixels >> 12) & 0x0f;
					if (pix)
						pix |= color;
					dst[6] = BURN_ENDIAN_SWAP_INT16(pix);

					pix = (pixels >> 8) & 0x0f;
					if (pix)
						pix |= color;
					dst[7] = BURN_ENDIAN_SWAP_INT16(pix);

					dst += width;
				}
			}

			/* flipped case */
			else
			{
				INT32 effdstx = (width - 1) - x * 8;
				INT32 effdsty = (height - 1) - y * 8;
				UINT16 *dst = BurnBitmapGetPosition(destbmp+5, effdstx, effdsty);

				/* loop over rows */
				for (INT32 iy = 0; iy < 8; iy++)
				{
					INT32 pixels = BURN_ENDIAN_SWAP_INT16(*src++);
					INT32 pix;

					pix = (pixels >> 4) & 0x0f;
					if (pix)
						pix |= color;
					dst[0] = BURN_ENDIAN_SWAP_INT16(pix);

					pix = (pixels >> 0) & 0x0f;
					if (pix)
						pix |= color;
					dst[-1] = BURN_ENDIAN_SWAP_INT16(pix);

					pix = (pixels >> 12) & 0x0f;
					if (pix)
						pix |= color;
					dst[-2] = BURN_ENDIAN_SWAP_INT16(pix);

					pix = (pixels >> 8) & 0x0f;
					if (pix)
						pix |= color;
					dst[-3] = BURN_ENDIAN_SWAP_INT16(pix);

					pixels = BURN_ENDIAN_SWAP_INT16(*src++);

					pix = (pixels >> 4) & 0x0f;
					if (pix)
						pix |= color;
					dst[-4] = BURN_ENDIAN_SWAP_INT16(pix);

					pix = (pixels >> 0) & 0x0f;
					if (pix)
						pix |= color;
					dst[-5] = BURN_ENDIAN_SWAP_INT16(pix);

					pix = (pixels >> 12) & 0x0f;
					if (pix)
						pix |= color;
					dst[-6] = BURN_ENDIAN_SWAP_INT16(pix);

					pix = (pixels >> 8) & 0x0f;
					if (pix)
						pix |= color;
					dst[-7] = BURN_ENDIAN_SWAP_INT16(pix);

					dst -= width;
				}
			}
		}
}

static void update_background(clip_struct cliprect, UINT16 *ram, INT32 destbmp)
{
	for (INT32 y = cliprect.nMiny; y <= cliprect.nMaxy; y++)
	{
		UINT16 *const dst = BurnBitmapGetPosition(destbmp+5, 0, y);
		INT32 color;

		/* determine the color */
		if (BURN_ENDIAN_SWAP_INT16(ram[0x1ff5e/2]) & 0x8000)
		{
			// line color select (bank wraps at 511, confirmed by arabfgt and kokoroj2)
			INT32 yoffset = (BURN_ENDIAN_SWAP_INT16(ram[0x1ff5e/2]) + y) & 0x1ff;
			color = (BURN_ENDIAN_SWAP_INT16(ram[0x1ff5e/2]) & 0x1e00) + yoffset;
		}
		else
			color = BURN_ENDIAN_SWAP_INT16(ram[0x1ff5e/2]) & 0x1e00;

		/* if the color doesn't match, fill */
		if ((m_bgcolor_line[y & 0x1ff] != color) || (m_prev_bgstartx[y & 0x1ff] != cliprect.nMinx) || (m_prev_bgendx[y & 0x1ff] != cliprect.nMaxx))
		{
			for (INT32 x = cliprect.nMinx; x <= cliprect.nMaxx; x++)
				dst[x] = BURN_ENDIAN_SWAP_INT16(color);

			m_prev_bgstartx[y & 0x1ff] = cliprect.nMinx;
			m_prev_bgendx[y & 0x1ff] = cliprect.nMaxx;
			m_bgcolor_line[y & 0x1ff] = color;
		}
	}
}

static void update_bitmap(clip_struct cliprect, UINT16 *ram, INT32 destbmp)
{
	INT32 bpp = (BURN_ENDIAN_SWAP_INT16(ram[0x1ff00/2]) & 0x0800) ? 8 : 4;

	/* determine the clipping */
	INT32 clipenable = (BURN_ENDIAN_SWAP_INT16(ram[0x1ff02/2]) >> 15) & 1;
	INT32 clipout = (BURN_ENDIAN_SWAP_INT16(ram[0x1ff02/2]) >> 10) & 1;
	INT32 clips = 0x10;
	extents_list clip_extents;
	INT32 clipdraw_start = compute_clipping_extents(clipenable, clipout, clips, cliprect, &clip_extents);

	/* determine x/y scroll */
	INT32 xscroll = BURN_ENDIAN_SWAP_INT16(ram[0x1ff88/2]) & 0x1ff;
	INT32 yscroll = BURN_ENDIAN_SWAP_INT16(ram[0x1ff8a/2]) & 0x1ff;
	INT32 color = (BURN_ENDIAN_SWAP_INT16(ram[0x1ff8c/2]) << 4) & 0x1fff0 & ~((1 << bpp) - 1);

	/* loop over target rows */
	for (INT32 y = cliprect.nMiny; y <= cliprect.nMaxy; y++)
	{
		UINT16 const *extents = &clip_extents.extent[clip_extents.scan_extent[y]][0];
		UINT16 *const dst = BurnBitmapGetPosition(destbmp+5, 0, y);
		INT32 clipdraw = clipdraw_start;

		if (clipdraw || extents[1] <= cliprect.nMaxx)
		{
			INT32 transparent = 0;

			/* loop over extents */
			while (1)
			{
				/* if we're drawing on this extent, draw it */
				if (clipdraw)
				{
					/* 8bpp mode case */
					if (bpp == 8)
					{
						UINT8 const *src = (UINT8 *)&ram[512/2 * ((y + yscroll) & 0xff)];
						for (INT32 x = extents[0]; x < extents[1]; x++)
						{
							INT32 effx = (x + xscroll) & 0x1ff;
							INT32 pix = BURN_ENDIAN_SWAP_INT16(src[(effx)]) + color;
							if ((pix & 0xff) == 0)
								pix = 0, transparent++;
							dst[x] = BURN_ENDIAN_SWAP_INT16(pix);
						}
					}

					/* 4bpp mode case */
					else
					{
						UINT16 const *src = &ram[512/4 * ((y + yscroll) & 0x1ff)];
						for (INT32 x = extents[0]; x < extents[1]; x++)
						{
							INT32 effx = (x + xscroll) & 0x1ff;
							INT32 pix = ((BURN_ENDIAN_SWAP_INT16(src[effx / 4]) >> (4 * (effx & 3))) & 0x0f) + color;
							if ((pix & 0x0f) == 0)
								pix = 0, transparent++;
							dst[x] = BURN_ENDIAN_SWAP_INT16(pix);
						}
					}
				}

				/* otherwise, clear to zero */
				else
				{
					INT32 pixels = extents[1] - extents[0];
					memset(&dst[extents[0]], 0, pixels * sizeof(dst[0]));
					transparent += pixels;
				}

				/* stop at the end */
				if (extents[1] > cliprect.nMaxx)
					break;

				/* swap states and advance to the next extent */
				clipdraw = !clipdraw;
				extents++;
			}

			transparent_check[destbmp+5][y] = (transparent == cliprect.nMaxx - cliprect.nMinx + 1);
		}
		else
			transparent_check[destbmp+5][y] = 1;
	}
}

static UINT8 update_tilemaps(clip_struct cliprect)
{
	UINT16 *ram = (UINT16*)DrvVidRAM;

	INT32 enable0 = !(BURN_ENDIAN_SWAP_INT16(ram[0x1ff02/2]) & 0x0001) && !(BURN_ENDIAN_SWAP_INT16(ram[0x1ff8e/2]) & 0x0002);
	INT32 enable1 = !(BURN_ENDIAN_SWAP_INT16(ram[0x1ff02/2]) & 0x0002) && !(BURN_ENDIAN_SWAP_INT16(ram[0x1ff8e/2]) & 0x0004);
	INT32 enable2 = !(BURN_ENDIAN_SWAP_INT16(ram[0x1ff02/2]) & 0x0004) && !(BURN_ENDIAN_SWAP_INT16(ram[0x1ff8e/2]) & 0x0008) && !(BURN_ENDIAN_SWAP_INT16(ram[0x1ff00/2]) & 0x1000);
	INT32 enable3 = !(BURN_ENDIAN_SWAP_INT16(ram[0x1ff02/2]) & 0x0008) && !(BURN_ENDIAN_SWAP_INT16(ram[0x1ff8e/2]) & 0x0010) && !(BURN_ENDIAN_SWAP_INT16(ram[0x1ff00/2]) & 0x2000);
	INT32 enablet = !(BURN_ENDIAN_SWAP_INT16(ram[0x1ff02/2]) & 0x0010) && !(BURN_ENDIAN_SWAP_INT16(ram[0x1ff8e/2]) & 0x0001);
	INT32 enableb = !(BURN_ENDIAN_SWAP_INT16(ram[0x1ff02/2]) & 0x0020) && !(BURN_ENDIAN_SWAP_INT16(ram[0x1ff8e/2]) & 0x0020);

	if (enable0) update_tilemap_zoom(cliprect, ram, 		MIXER_LAYER_NBG0, 0);		// MIXER_LAYER_NBG0
	if (enable1) update_tilemap_zoom(cliprect, ram, 		MIXER_LAYER_NBG1, 1);		// MIXER_LAYER_NBG1
	if (enable2) update_tilemap_rowscroll(cliprect, ram,	MIXER_LAYER_NBG2, 2);		// MIXER_LAYER_NBG2
	if (enable3) update_tilemap_rowscroll(cliprect, ram,	MIXER_LAYER_NBG3, 3);		// MIXER_LAYER_NBG3
	if (enablet) update_tilemap_text(cliprect, ram, 		MIXER_LAYER_TEXT);			// MIXER_LAYER_TEXT
	if (enableb) update_bitmap(cliprect, ram, 				MIXER_LAYER_BITMAP);		// MIXER_LAYER_BITMAP
	update_background(cliprect, ram, 						MIXER_LAYER_BACKGROUND);	// MIXER_LAYER_BACKGROUND

	return (enablet << 0) | (enable0 << 1) | (enable1 << 2) | (enable2 << 3) | (enable3 << 4) | (enableb << 5);
}

static void sprite_erase_buffer()
{
	BurnBitmapFill(MIXER_LAYER_SPRITES+5, 0xffff);

	if (is_multi32) BurnBitmapFill(MIXER_LAYER_MULTISPR+5, 0xffff);
}

static void sprite_swap_buffers()
{
	clip_struct *clip = BurnBitmapClipDims(MIXER_LAYER_SPRITES+5);

	UINT16 *src = BurnBitmapGetPosition(MIXER_LAYER_SPRITES+5, 0, 0);
	UINT16 *dst = BurnBitmapGetPosition(MIXER_LAYER_SPRITES_2+5, 0, 0);

	for (INT32 i = 0; i < clip->nMaxx * clip->nMaxy; i++)
	{
		INT32 t = dst[i];
		dst[i] = src[i];
		src[i] = t;
	}

	if (is_multi32)
	{
		clip = BurnBitmapClipDims(MIXER_LAYER_MULTISPR+5);
		src = BurnBitmapGetPosition(MIXER_LAYER_MULTISPR+5, 0, 0);
		dst = BurnBitmapGetPosition(MIXER_LAYER_MULTISPR_2+5, 0, 0);

		for (INT32 i = 0; i < clip->nMaxx * clip->nMaxy; i++)
		{
			INT32 t = dst[i];
			dst[i] = src[i];
			src[i] = t;
		}
	}

	memcpy(sprite_control_latched, sprite_control, sizeof(sprite_control_latched));
}

#define sprite_draw_pixel_16(trans)                                         \
	/* only draw if onscreen, not 0 or 15 */                                \
	if (x >= clipin.nMinx && x <= clipin.nMaxx &&                           \
		(!do_clipout || x < clipout.nMinx || x > clipout.nMaxx) &&          \
		pix != trans)                                                       \
	{                                                                       \
		if (!indirect)                                                      \
		{                                                                   \
			if (pix != 0)                                                   \
			{                                                               \
				if (!shadow)                                                \
					dest[x] = color | pix;                                  \
				else                                                        \
					dest[x] &= 0x7fff;                                      \
			}                                                               \
		}                                                                   \
		else                                                                \
		{                                                                   \
			INT32 indpix = indtable[pix];                                     \
			if ((indpix & transmask) != transmask)                          \
			{                                                               \
				if (!shadow)                                                \
					dest[x] = indpix;                                       \
				else                                                        \
					dest[x] &= 0x7fff;                                      \
			}                                                               \
		}                                                                   \
	}

#define sprite_draw_pixel_256(trans)                                        \
	/* only draw if onscreen, not 0 or 15 */                                \
	if (x >= clipin.nMinx && x <= clipin.nMaxx &&                           \
		(!do_clipout || x < clipout.nMinx || x > clipout.nMaxx) &&          \
		pix != trans)                                                       \
	{                                                                       \
		if (!indirect)                                                      \
		{                                                                   \
			if (pix != 0)                                                   \
			{                                                               \
				if (!shadow)                                                \
					dest[x] = color | pix;                                  \
				else                                                        \
					dest[x] &= 0x7fff;                                      \
			}                                                               \
		}                                                                   \
		else                                                                \
		{                                                                   \
			INT32 indpix = (indtable[pix >> 4]) | (pix & 0x0f);               \
			if ((indpix & transmask) != transmask)                          \
			{                                                               \
				if (!shadow)                                                \
					dest[x] = indpix;                                       \
				else                                                        \
					dest[x] &= 0x7fff;                                      \
			}                                                               \
		}                                                                   \
	}

static INT32 draw_one_sprite(UINT16 const *data, INT32 xoffs, INT32 yoffs, const clip_struct clipin, const clip_struct clipout)
{
	static const INT32 transparency_masks[4][4] =
	{
		{ 0x7fff, 0x3fff, 0x1fff, 0x0fff },
		{ 0x3fff, 0x1fff, 0x0fff, 0x07ff },
		{ 0x3fff, 0x1fff, 0x0fff, 0x07ff },
		{ 0x1fff, 0x0fff, 0x07ff, 0x03ff }
	};

	UINT16 *m_spriteram = (UINT16*)DrvSprRAM;
	INT32 select = (!is_multi32 || !(BURN_ENDIAN_SWAP_INT16(data[3]) & 0x0800)) ? MIXER_LAYER_SPRITES_2 : MIXER_LAYER_MULTISPR_2;

	UINT16 *bitmap = BurnBitmapGetPosition(5 + select, 0, 0);
	UINT8 numbanks = (graphics_length[1] / 4) >> 20; // size in 32bit dwords, not bytes! -dink

	INT32 indirect = BURN_ENDIAN_SWAP_INT16(data[0]) & 0x2000;
	INT32 indlocal = BURN_ENDIAN_SWAP_INT16(data[0]) & 0x1000;
	INT32 shadow   = (BURN_ENDIAN_SWAP_INT16(data[0]) & 0x0800) && (sprite_control_latched[0x0a/2] & 1);
	INT32 fromram  = BURN_ENDIAN_SWAP_INT16(data[0]) & 0x0400;
	INT32 bpp8     = BURN_ENDIAN_SWAP_INT16(data[0]) & 0x0200;
	INT32 transp   = (BURN_ENDIAN_SWAP_INT16(data[0]) & 0x0100) ? 0 : (bpp8 ? 0xff : 0x0f);
	INT32 flipy    = BURN_ENDIAN_SWAP_INT16(data[0]) & 0x0080;
	INT32 flipx    = BURN_ENDIAN_SWAP_INT16(data[0]) & 0x0040;
	INT32 offsety  = BURN_ENDIAN_SWAP_INT16(data[0]) & 0x0020;
	INT32 offsetx  = BURN_ENDIAN_SWAP_INT16(data[0]) & 0x0010;
	INT32 adjusty  = (BURN_ENDIAN_SWAP_INT16(data[0]) >> 2) & 3;
	INT32 adjustx  = (BURN_ENDIAN_SWAP_INT16(data[0]) >> 0) & 3;
	INT32 srch     = (BURN_ENDIAN_SWAP_INT16(data[1]) >> 8);
	INT32 srcw     = bpp8 ? (BURN_ENDIAN_SWAP_INT16(data[1]) & 0x3f) : ((BURN_ENDIAN_SWAP_INT16(data[1]) >> 1) & 0x3f);
	INT32 bank     = is_multi32 ?
					((BURN_ENDIAN_SWAP_INT16(data[3]) & 0x2000) >> 13) | ((BURN_ENDIAN_SWAP_INT16(data[3]) & 0x8000) >> 14) :
					((BURN_ENDIAN_SWAP_INT16(data[3]) & 0x0800) >> 11) | ((BURN_ENDIAN_SWAP_INT16(data[3]) & 0x4000) >> 13);
	INT32 dsth     = BURN_ENDIAN_SWAP_INT16(data[2]) & 0x3ff;
	INT32 dstw     = BURN_ENDIAN_SWAP_INT16(data[3]) & 0x3ff;
	INT32 ypos     = (INT16)(BURN_ENDIAN_SWAP_INT16(data[4]) << 4) >> 4;
	INT32 xpos     = (INT16)(BURN_ENDIAN_SWAP_INT16(data[5]) << 4) >> 4;
	UINT32 addr  = BURN_ENDIAN_SWAP_INT16(data[6]) | ((BURN_ENDIAN_SWAP_INT16(data[2]) & 0xf000) << 4);
	INT32 color    = 0x8000 | (BURN_ENDIAN_SWAP_INT16(data[7]) & (bpp8 ? 0x7f00 : 0x7ff0));
	INT32 hzoom, vzoom;
	INT32 xdelta = 1, ydelta = 1;
	INT32 xtarget, ytarget, yacc = 0, pix, transmask;
	const UINT32 *spritedata;
	UINT32 addrmask;
	UINT16 indtable[16];

	/* if hidden, or top greater than/equal to bottom, or invalid bank, punt */
	if (srcw == 0 || srch == 0 || dstw == 0 || dsth == 0)
		goto bail;

	/* determine the transparency mask for pixels */
	transmask = transparency_masks[sprite_control_latched[0x08/2] & 3][sprite_control_latched[0x0a/2] & 3];
	if (bpp8)
		transmask &= 0xfff0;

	/* create the local palette for the indirect case */
	if (indirect)
	{
		UINT16 const *src = indlocal ? &data[8] : &m_spriteram[8 * (BURN_ENDIAN_SWAP_INT16(data[7]) & 0x1fff)];
		for (INT32 x = 0; x < 16; x++)
			indtable[x] = (BURN_ENDIAN_SWAP_INT16(src[x]) & (bpp8 ? 0xfff0 : 0xffff)) | ((sprite_control_latched[0x0a/2] & 1) ? 0x8000 : 0x0000);
	}

	/* clamp to within the memory region size */
	if (fromram)
	{
		spritedata = DrvSprRAM32;
		addrmask = (0x20000 / 4) - 1;
	}
	else
	{
		UINT32 *sprite_region = (UINT32*)DrvGfxROM[1];
		if (numbanks)
			bank %= numbanks;
		spritedata = &sprite_region[bank << 20];
		addrmask = 0xfffff;
	}

	/* compute X/Y deltas */
	hzoom = (((bpp8 ? 4 : 8) * srcw) << 16) / dstw;
	vzoom = (srch << 16) / dsth;

	/* adjust the starting X position */
	if (offsetx)
		xpos += xoffs;
	switch (adjustx)
	{
		case 0:
		case 3: xpos -= (dstw - 1) / 2;     break;
		case 1: xpos -= dstw - 1;           break;
		case 2:                             break;
	}

	/* adjust the starting Y position */
	if (offsety)
		ypos += yoffs;
	switch (adjusty)
	{
		case 0:
		case 3: ypos -= (dsth - 1) / 2;     break;
		case 1: ypos -= dsth - 1;           break;
		case 2:                             break;
	}

	/* adjust for flipping */
	if (flipx)
	{
		xpos += dstw - 1;
		xdelta = -1;
	}
	if (flipy)
	{
		ypos += dsth - 1;
		ydelta = -1;
	}

	/* compute target X,Y positions for loops */
	xtarget = xpos + xdelta * dstw;
	ytarget = ypos + ydelta * dsth;

	/* adjust target x for clipping */
	if (xdelta > 0 && xtarget > clipin.nMaxx)
	{
		xtarget = clipin.nMaxx + 1;
		if (xpos >= xtarget)
			goto bail;
	}
	if (xdelta < 0 && xtarget < clipin.nMinx)
	{
		xtarget = clipin.nMinx - 1;
		if (xpos <= xtarget)
			goto bail;
	}

	/* loop from top to bottom */
	for (INT32 y = ypos; y != ytarget; y += ydelta)
	{
		/* skip drawing if not within the inclusive cliprect */
		if (y >= clipin.nMiny && y <= clipin.nMaxy)
		{
			INT32 do_clipout = (y >= clipout.nMiny && y < clipout.nMaxy);
			UINT16 *const dest = bitmap + y * 512;
			INT32 xacc = 0;

			/* 4bpp case */
			if (!bpp8)
			{
				/* start at the word before because we preincrement below */
				UINT32 curaddr = addr - 1;
				for (INT32 x = xpos; x != xtarget; )
				{
					UINT32 pixels = BURN_ENDIAN_SWAP_INT32(spritedata[++curaddr & addrmask]);

					/* draw four pixels */
					pix = (pixels >> 28) & 0xf; while (xacc < 0x10000 && x != xtarget) { sprite_draw_pixel_16(transp)  x += xdelta; xacc += hzoom; } xacc -= 0x10000;
					pix = (pixels >> 24) & 0xf; while (xacc < 0x10000 && x != xtarget) { sprite_draw_pixel_16(0);      x += xdelta; xacc += hzoom; } xacc -= 0x10000;
					pix = (pixels >> 20) & 0xf; while (xacc < 0x10000 && x != xtarget) { sprite_draw_pixel_16(0);      x += xdelta; xacc += hzoom; } xacc -= 0x10000;
					pix = (pixels >> 16) & 0xf; while (xacc < 0x10000 && x != xtarget) { sprite_draw_pixel_16(0);      x += xdelta; xacc += hzoom; } xacc -= 0x10000;
					pix = (pixels >> 12) & 0xf; while (xacc < 0x10000 && x != xtarget) { sprite_draw_pixel_16(0);      x += xdelta; xacc += hzoom; } xacc -= 0x10000;
					pix = (pixels >>  8) & 0xf; while (xacc < 0x10000 && x != xtarget) { sprite_draw_pixel_16(0);      x += xdelta; xacc += hzoom; } xacc -= 0x10000;
					pix = (pixels >>  4) & 0xf; while (xacc < 0x10000 && x != xtarget) { sprite_draw_pixel_16(0);      x += xdelta; xacc += hzoom; } xacc -= 0x10000;
					pix = (pixels >>  0) & 0xf; while (xacc < 0x10000 && x != xtarget) { sprite_draw_pixel_16(transp); x += xdelta; xacc += hzoom; } xacc -= 0x10000;

					/* check for end code */
					if (transp != 0 && pix == 0x0f)
						break;
				}
			}

			/* 8bpp case */
			else
			{
				/* start at the word before because we preincrement below */
				UINT32 curaddr = addr - 1;
				for (INT32 x = xpos; x != xtarget; )
				{
					UINT32 pixels = BURN_ENDIAN_SWAP_INT32(spritedata[++curaddr & addrmask]);

					/* draw four pixels */
					pix = (pixels >> 24) & 0xff; while (xacc < 0x10000 && x != xtarget) { sprite_draw_pixel_256(transp); x += xdelta; xacc += hzoom; } xacc -= 0x10000;
					pix = (pixels >> 16) & 0xff; while (xacc < 0x10000 && x != xtarget) { sprite_draw_pixel_256(0);      x += xdelta; xacc += hzoom; } xacc -= 0x10000;
					pix = (pixels >>  8) & 0xff; while (xacc < 0x10000 && x != xtarget) { sprite_draw_pixel_256(0);      x += xdelta; xacc += hzoom; } xacc -= 0x10000;
					pix = (pixels >>  0) & 0xff; while (xacc < 0x10000 && x != xtarget) { sprite_draw_pixel_256(transp); x += xdelta; xacc += hzoom; } xacc -= 0x10000;

					/* check for end code */
					if (transp != 0 && pix == 0xff)
						break;
				}
			}
		}

		/* accumulate zoom factors; if we carry into the high bit, skip an extra row */
		yacc += vzoom;
		addr += srcw * (yacc >> 16);
		yacc &= 0xffff;
	}

bail:
	return (indirect && indlocal) ? 2 : 0;
}

static void sprite_render_list()
{
	UINT16 *m_spriteram = (UINT16*)DrvSprRAM;
	clip_struct outerclip, clipin, clipout;
	INT32 xoffs = 0, yoffs = 0;
	INT32 numentries = 0;
	INT32 spritenum = 0;

	/* compute the outer clip */
	outerclip.nMinx = outerclip.nMiny = 0;
	outerclip.nMaxx = (sprite_control_latched[0x0c/2] & 1) ? 415 : 319;
	outerclip.nMaxy = 223;

	/* initialize the cliprects */
	clipin = outerclip;
	clipout.nMinx = clipout.nMiny = 0;
	clipout.nMaxx = clipout.nMaxy = -1;

	/* now draw */
	while (numentries++ < 0x20000/16)
	{
		/* top two bits are a command */
		UINT16 const *const sprite = &m_spriteram[8 * (spritenum & 0x1fff)];
		switch (BURN_ENDIAN_SWAP_INT16(sprite[0]) >> 14)
		{
			/* command 0 = draw sprite */
			case 0:
				spritenum += 1 + draw_one_sprite(sprite, xoffs, yoffs, clipin, clipout);
				break;

			/* command 1 = set clipping */
			case 1:

				/* set the inclusive cliprect */
				if (BURN_ENDIAN_SWAP_INT16(sprite[0]) & 0x1000)
				{
					clipin.nMiny = (INT16)(BURN_ENDIAN_SWAP_INT16(sprite[0]) << 4) >> 4;
					clipin.nMaxy = (INT16)(BURN_ENDIAN_SWAP_INT16(sprite[1]) << 4) >> 4;
					clipin.nMinx = (INT16)(BURN_ENDIAN_SWAP_INT16(sprite[2]) << 4) >> 4;
					clipin.nMaxx = (INT16)(BURN_ENDIAN_SWAP_INT16(sprite[3]) << 4) >> 4;

					if (clipin.nMiny < outerclip.nMiny) clipin.nMiny = outerclip.nMiny;
					if (clipin.nMinx < outerclip.nMinx) clipin.nMinx = outerclip.nMinx;
					if (clipin.nMaxy > outerclip.nMaxy) clipin.nMaxy = outerclip.nMaxy;
					if (clipin.nMaxx > outerclip.nMaxx) clipin.nMaxx = outerclip.nMaxx;
				}

				/* set the exclusive cliprect */
				if (BURN_ENDIAN_SWAP_INT16(sprite[0]) & 0x2000)
				{
					clipout.nMiny = (INT16)(BURN_ENDIAN_SWAP_INT16(sprite[4]) << 4) >> 4;
					clipout.nMaxy = (INT16)(BURN_ENDIAN_SWAP_INT16(sprite[5]) << 4) >> 4;
					clipout.nMinx = (INT16)(BURN_ENDIAN_SWAP_INT16(sprite[6]) << 4) >> 4;
					clipout.nMaxx = (INT16)(BURN_ENDIAN_SWAP_INT16(sprite[7]) << 4) >> 4;
				}

				/* advance to the next entry */
				spritenum++;
				break;

			/* command 2 = jump to position, and set X offset */
			case 2:

				/* set the global offset */
				if (BURN_ENDIAN_SWAP_INT16(sprite[0]) & 0x2000)
				{
					yoffs = (INT16)(BURN_ENDIAN_SWAP_INT16(sprite[1]) << 4) >> 4;
					xoffs = (INT16)(BURN_ENDIAN_SWAP_INT16(sprite[2]) << 4) >> 4;
				}
				spritenum = BURN_ENDIAN_SWAP_INT16(sprite[0]) & 0x1fff;
				break;

			/* command 3 = done */
			case 3:
				numentries = 0x20000/16;
				break;
		}
	}
}

static void update_sprites()
{
	if (!(sprite_control[3] & 2))
	{
		if (sprite_render_count-- == 0)
		{
			sprite_control[0] = 3;
			sprite_render_count = sprite_control[3] & 1;
		}
	}

	if (sprite_control[0] & 2) {
		sprite_erase_buffer();
	}

	if (sprite_control[0] & 1)
	{
		sprite_swap_buffers();
		sprite_render_list();
	}

	sprite_control[0] = 0;
}

static inline UINT8 compute_color_offsets(INT32 which, INT32 layerbit, INT32 layerflag)
{
	INT32 mode = ((mixer_control[which][0x3e/2] & 0x8000) >> 14) | (layerbit & 1);

	switch (mode)
	{
		case 0:
		case 3:
		default:
			return !layerflag;

		case 1:
			/* fix me -- these are grayscale modes */
			return 2;

		case 2:
			return (!layerflag) ? 2 : 0;
	}
}

static inline UINT16 compute_sprite_blend(UINT8 encoding)
{
	INT32 value = encoding & 0xf;

	switch ((encoding >> 4) & 3)
	{
		case 0:     return 1 << value;

		case 1:     return (1 << value) | ((1 << value) - 1);

		case 2:     return ~((1 << value) - 1) & 0xffff;

		/* blend always */
		default:
		case 3:     return 0xffff;
	}
}

static inline UINT16 *get_layer_scanline(INT32 layer, INT32 scanline)
{
	if (transparent_check[layer+5][scanline])
		return (layer == MIXER_LAYER_SPRITES) ? solid_ffff : solid_0000;

	// layer disable
	if (layer < 7 && ~nSpriteEnable & (1 << layer)) return (layer == MIXER_LAYER_SPRITES) ? solid_ffff : solid_0000;

	return BurnBitmapGetPosition(5 + layer, 0, scanline);
}

static void mix_all_layers(INT32 which, INT32 xoffs, const clip_struct cliprect, UINT8 enablemask)
{
	UINT16 *m_paletteram = (UINT16*)DrvPalRAM[which];
	INT32 blendenable = mixer_control[which][0x4e/2] & 0x0800;
	INT32 blendfactor = (mixer_control[which][0x4e/2] >> 8) & 7;
	struct mixer_layer_info
	{
		UINT16      palbase;            /* palette base from control reg */
		UINT16      sprblendmask;       /* mask of sprite priorities this layer blends with */
		UINT8       blendmask;          /* mask of layers this layer blends with */
		UINT8       index;              /* index of this layer (MIXER_LAYER_XXX) */
		UINT8       effpri;             /* effective priority = (priority << 3) | layer_priority */
		UINT8       mixshift;           /* shift from control reg */
		UINT8       coloroffs;          /* color offset index */
	} layerorder[16][8], layersort[8];

	/* if we are the second monitor on multi32, swap in the proper sprite bank */
//	if (which == 1)
//	{
//		std::swap(m_layer_data[MIXER_LAYER_SPRITES].bitmap, m_layer_data[MIXER_LAYER_MULTISPR].bitmap);
//		std::swap(m_layer_data[MIXER_LAYER_SPRITES].transparent, m_layer_data[MIXER_LAYER_MULTISPR].transparent);
//		std::swap(m_layer_data[MIXER_LAYER_SPRITES].num, m_layer_data[MIXER_LAYER_MULTISPR].num);
//	}

	/* extract the RGB offsets */
	INT32 rgboffs[3][3];
	rgboffs[0][0] = (INT8)(mixer_control[which][0x40/2] << 2) >> 2;
	rgboffs[0][1] = (INT8)(mixer_control[which][0x42/2] << 2) >> 2;
	rgboffs[0][2] = (INT8)(mixer_control[which][0x44/2] << 2) >> 2;
	rgboffs[1][0] = (INT8)(mixer_control[which][0x46/2] << 2) >> 2;
	rgboffs[1][1] = (INT8)(mixer_control[which][0x48/2] << 2) >> 2;
	rgboffs[1][2] = (INT8)(mixer_control[which][0x4a/2] << 2) >> 2;
	rgboffs[2][0] = 0;
	rgboffs[2][1] = 0;
	rgboffs[2][2] = 0;

	/* determine the sprite grouping parameters first */
	UINT8 sprgroup_shift, sprgroup_mask, sprgroup_or;
	switch (mixer_control[which][0x4c/2] & 0x0f)
	{
		default:
		case 0x0:   sprgroup_shift = 14;    sprgroup_mask = 0x00;   sprgroup_or = 0x01; break;
		case 0x1:   sprgroup_shift = 14;    sprgroup_mask = 0x01;   sprgroup_or = 0x02; break;
		case 0x2:   sprgroup_shift = 13;    sprgroup_mask = 0x03;   sprgroup_or = 0x04; break;
		case 0x3:   sprgroup_shift = 12;    sprgroup_mask = 0x07;   sprgroup_or = 0x08; break;

		case 0x4:   sprgroup_shift = 14;    sprgroup_mask = 0x01;   sprgroup_or = 0x00; break;
		case 0x5:   sprgroup_shift = 13;    sprgroup_mask = 0x03;   sprgroup_or = 0x00; break;
		case 0x6:   sprgroup_shift = 12;    sprgroup_mask = 0x07;   sprgroup_or = 0x00; break;
		case 0x7:   sprgroup_shift = 11;    sprgroup_mask = 0x0f;   sprgroup_or = 0x00; break;

		case 0x8:   sprgroup_shift = 14;    sprgroup_mask = 0x01;   sprgroup_or = 0x00; break;
		case 0x9:   sprgroup_shift = 13;    sprgroup_mask = 0x03;   sprgroup_or = 0x00; break;
		case 0xa:   sprgroup_shift = 12;    sprgroup_mask = 0x07;   sprgroup_or = 0x00; break;
		case 0xb:   sprgroup_shift = 11;    sprgroup_mask = 0x0f;   sprgroup_or = 0x00; break;

		case 0xc:   sprgroup_shift = 13;    sprgroup_mask = 0x01;   sprgroup_or = 0x00; break;
		case 0xd:   sprgroup_shift = 12;    sprgroup_mask = 0x03;   sprgroup_or = 0x00; break;
		case 0xe:   sprgroup_shift = 11;    sprgroup_mask = 0x07;   sprgroup_or = 0x00; break;
		case 0xf:   sprgroup_shift = 10;    sprgroup_mask = 0x0f;   sprgroup_or = 0x00; break;
	}
	INT32 sprshadowmask = (mixer_control[which][0x4c/2] & 0x04) ? 0x8000 : 0x0000;
	INT32 sprpixmask = ((1 << sprgroup_shift) - 1) & 0x3fff;
	INT32 sprshadow = 0x7ffe & sprpixmask;

	/* extract info about TEXT, NBG0-3, and BITMAP layers, which all follow the same pattern */
	INT32 numlayers = 0;
	for (INT32 laynum = MIXER_LAYER_TEXT; laynum <= MIXER_LAYER_BITMAP; laynum++)
	{
		INT32 priority = mixer_control[which][0x20/2 + laynum] & 0x0f;
		if ((enablemask & (1 << laynum)) && priority != 0)
		{
			layersort[numlayers].index = laynum;
			layersort[numlayers].effpri = (priority << 3) | (6 - laynum);
			layersort[numlayers].palbase = (mixer_control[which][0x20/2 + laynum] & 0x00f0) << 6;
			layersort[numlayers].mixshift = (mixer_control[which][0x20/2 + laynum] >> 8) & 3;
			layersort[numlayers].blendmask = blendenable ? ((mixer_control[which][0x30/2 + laynum] >> 6) & 0xff) : 0;
			layersort[numlayers].sprblendmask = compute_sprite_blend(mixer_control[which][0x30/2 + laynum] & 0x3f);
			layersort[numlayers].coloroffs = compute_color_offsets(which, (mixer_control[which][0x3e/2] >> laynum) & 1, (mixer_control[which][0x30/2 + laynum] >> 14) & 1);
			numlayers++;
		}
	}

	/* extract info about the BACKGROUND layer */
	layersort[numlayers].index = MIXER_LAYER_BACKGROUND;
	layersort[numlayers].effpri = (1 << 3) | 0;
	layersort[numlayers].palbase = (mixer_control[which][0x2c/2] & 0x00f0) << 6;
	layersort[numlayers].mixshift = (mixer_control[which][0x2c/2] >> 8) & 3;
	layersort[numlayers].blendmask = 0;
	layersort[numlayers].sprblendmask = 0;
	layersort[numlayers].coloroffs = compute_color_offsets(which, (mixer_control[which][0x3e/2] >> 8) & 1, (mixer_control[which][0x3e/2] >> 14) & 1);
	numlayers++;

	/* now bubble sort the list by effective priority */
	for (INT32 laynum = 0; laynum < numlayers; laynum++)
		for (INT32 i = laynum + 1; i < numlayers; i++)
			if (layersort[i].effpri > layersort[laynum].effpri)
			{
				mixer_layer_info temp = layersort[i];
				layersort[i] = layersort[laynum];
				layersort[laynum] = temp;
			}

	/* for each possible sprite group, insert the sprites into the list at the appropriate point */
	for (INT32 groupnum = 0; groupnum <= sprgroup_mask; groupnum++)
	{
		INT32 effgroup = sprgroup_or | groupnum;
		INT32 priority = mixer_control[which][0x00/2 + effgroup] & 0x0f;
		INT32 effpri = (priority << 3) | 7;
		INT32 sprindex = numlayers;
		INT32 dstnum = 0;

		/* make a copy of the sorted list, finding a location for the sprite entry */
		for (INT32 laynum = 0; laynum < numlayers; laynum++)
		{
			if (effpri > layersort[laynum].effpri && sprindex == numlayers)
				sprindex = dstnum++;
			layerorder[groupnum][dstnum++] = layersort[laynum];
		}

		/* build the sprite entry */
		layerorder[groupnum][sprindex].index = MIXER_LAYER_SPRITES;
		layerorder[groupnum][sprindex].effpri = effpri;
		if ((mixer_control[which][0x4c/2] & 3) != 3)
			layerorder[groupnum][sprindex].palbase = (mixer_control[which][0x00/2 + effgroup] & 0x00f0) << 6;
		else
			layerorder[groupnum][sprindex].palbase = (mixer_control[which][0x4c/2] & 0x00f0) << 6;
		layerorder[groupnum][sprindex].mixshift = (mixer_control[which][0x00/2 + effgroup] >> 8) & 3;
		layerorder[groupnum][sprindex].blendmask = 0;
		layerorder[groupnum][sprindex].sprblendmask = 0;
		layerorder[groupnum][sprindex].coloroffs = compute_color_offsets(which, (mixer_control[which][0x3e/2] >> 6) & 1, (mixer_control[which][0x4c/2] >> 15) & 1);
	}

	/* based on the sprite controller flip bits, the data is scanned to us in different */
	/* directions; account for this */
	INT32 sprx_start, sprdx;
	if (sprite_control_latched[0x04/2] & 1)
	{
		sprx_start = cliprect.nMaxx;
		sprdx = -1;
	}
	else
	{
		sprx_start = cliprect.nMinx;
		sprdx = 1;
	}

	INT32 spry, sprdy;
	if (sprite_control_latched[0x04/2] & 2)
	{
		spry = cliprect.nMaxy;
		sprdy = -1;
	}
	else
	{
		spry = cliprect.nMiny;
		sprdy = 1;
	}

	/* loop over rows */
	for (INT32 y = cliprect.nMiny; y <= cliprect.nMaxy; y++, spry += sprdy)
	{
		UINT16 *dest = pTransDraw + (y * nScreenWidth) + xoffs;
		UINT16 *layerbase[8];

		/* get the starting address for each layer */
		layerbase[MIXER_LAYER_TEXT] = get_layer_scanline(MIXER_LAYER_TEXT, y);
		layerbase[MIXER_LAYER_NBG0] = get_layer_scanline(MIXER_LAYER_NBG0, y);
		layerbase[MIXER_LAYER_NBG1] = get_layer_scanline(MIXER_LAYER_NBG1, y);
		layerbase[MIXER_LAYER_NBG2] = get_layer_scanline(MIXER_LAYER_NBG2, y);
		layerbase[MIXER_LAYER_NBG3] = get_layer_scanline(MIXER_LAYER_NBG3, y);
		layerbase[MIXER_LAYER_BITMAP] = get_layer_scanline(MIXER_LAYER_BITMAP, y);
		layerbase[MIXER_LAYER_SPRITES] = get_layer_scanline(which ? MIXER_LAYER_MULTISPR : MIXER_LAYER_SPRITES, spry); // works ok instead of swap?
		layerbase[MIXER_LAYER_BACKGROUND] = get_layer_scanline(MIXER_LAYER_BACKGROUND, y);

		/* loop over columns */
		for (INT32 x = cliprect.nMinx, sprx = sprx_start; x <= cliprect.nMaxx; x++, sprx += sprdx)
		{
			mixer_layer_info const *first;
			INT32 laynum, firstpix;
			INT32 shadow = 0;

			/* first grab the current sprite pixel and determine the group */
			INT32 sprpix = layerbase[MIXER_LAYER_SPRITES][sprx];
			INT32 sprgroup = (sprpix >> sprgroup_shift) & sprgroup_mask;

			/* now scan the layers to find the topmost non-transparent pixel */
			for (first = &layerorder[sprgroup][0]; ; first++)
			{
				laynum = first->index;

				/* non-sprite layers are treated similarly */
				if (laynum != MIXER_LAYER_SPRITES)
				{
					firstpix = BURN_ENDIAN_SWAP_INT16(layerbase[laynum][x]) & 0x1fff;
					if (firstpix != 0 || laynum == MIXER_LAYER_BACKGROUND)
						break;
				}

				/* sprite layers are special */
				else
				{
					firstpix = sprpix;
					shadow = ~firstpix & sprshadowmask;
					if ((firstpix & 0x7fff) != 0x7fff)
					{
						firstpix &= sprpixmask;
						if ((firstpix & 0x7ffe) != sprshadow)
							break;
						shadow = 1;
					}
				}
			}

			/* adjust the first pixel */
			firstpix = BURN_ENDIAN_SWAP_INT16(m_paletteram[(first->palbase + ((firstpix >> first->mixshift) & 0xfff0) + (firstpix & 0x0f)) & 0x3fff]);

			/* compute R, G, B */
			INT32 const *rgbdelta = &rgboffs[first->coloroffs][0];
			INT32 r = ((firstpix >>  0) & 0x1f) + rgbdelta[0];
			INT32 g = ((firstpix >>  5) & 0x1f) + rgbdelta[1];
			INT32 b = ((firstpix >> 10) & 0x1f) + rgbdelta[2];

			/* if there are potential blends, keep looking */
			if (first->blendmask != 0)
			{
				mixer_layer_info const *second;
				INT32 secondpix;

				/* now scan the layers to find the topmost non-transparent pixel */
				for (second = first + 1; ; second++)
				{
					laynum = second->index;

					/* non-sprite layers are treated similarly */
					if (laynum != MIXER_LAYER_SPRITES)
					{
						secondpix = BURN_ENDIAN_SWAP_INT16(layerbase[laynum][x]) & 0x1fff;
						if (secondpix != 0 || laynum == MIXER_LAYER_BACKGROUND)
							break;
					}

					/* sprite layers are special */
					else
					{
						secondpix = sprpix;
						shadow = ~secondpix & sprshadowmask;
						if ((secondpix & 0x7fff) != 0x7fff)
						{
							secondpix &= sprpixmask;
							if ((secondpix & 0x7ffe) != sprshadow)
								break;
							shadow = 1;
						}
					}
				}

				/* are we blending with that layer? */
				if ((first->blendmask & (1 << laynum)) &&
					(laynum != MIXER_LAYER_SPRITES || (first->sprblendmask & (1 << sprgroup))))
				{
					/* adjust the second pixel */
					secondpix = BURN_ENDIAN_SWAP_INT16(m_paletteram[(second->palbase + ((secondpix >> second->mixshift) & 0xfff0) + (secondpix & 0x0f)) & 0x3fff]);

					/* compute first RGB */
					r *= 7 - blendfactor;
					g *= 7 - blendfactor;
					b *= 7 - blendfactor;

					/* add in second RGB */
					rgbdelta = &rgboffs[second->coloroffs][0];
					r += (((secondpix >>  0) & 0x1f) + rgbdelta[0]) * (blendfactor + 1);
					g += (((secondpix >>  5) & 0x1f) + rgbdelta[1]) * (blendfactor + 1);
					b += (((secondpix >> 10) & 0x1f) + rgbdelta[2]) * (blendfactor + 1);

					/* shift off the extra bits */
					r >>= 3;
					g >>= 3;
					b >>= 3;
				}
			}

			/* apply shadow/hilight */
			if (shadow)
			{
				r >>= 1;
				g >>= 1;
				b >>= 1;
			}

			/* clamp and combine */
			if (r > 31)
				firstpix = 31 << 10;
			else if (r > 0)
				firstpix = r << 10;
			else
				firstpix = 0;

			if (g > 31)
				firstpix |= 31 << 5;
			else if (g > 0)
				firstpix |= g << 5;

			if (b > 31)
				firstpix |= 31 << 0;
			else if (b > 0)
				firstpix |= b << 0;
			dest[x] = firstpix;
		}
	}

	/* if we are the second monitor on multi32, swap back the sprite layer */
//	if (which == 1)
//	{
//		std::swap(m_layer_data[MIXER_LAYER_SPRITES].bitmap, m_layer_data[MIXER_LAYER_MULTISPR].bitmap);
//		std::swap(m_layer_data[MIXER_LAYER_SPRITES].transparent, m_layer_data[MIXER_LAYER_MULTISPR].transparent);
//		std::swap(m_layer_data[MIXER_LAYER_SPRITES].num, m_layer_data[MIXER_LAYER_MULTISPR].num);
//	}
}

static void draw_screen(INT32 which)
{
	clip_struct cliprect;
	cliprect.nMinx = 0;
	cliprect.nMaxx = (nScreenWidth >= 640 ? nScreenWidth/2 : nScreenWidth) - 1; // on dual-screen, we are only supposed to draw one side
	cliprect.nMiny = 0;
	cliprect.nMaxy = nScreenHeight - 1;

	BurnTransferClear(0);

	if (!system32_displayenable[which]) {
		return;
	}

	UINT16 enablemask = update_tilemaps(cliprect);
	mix_all_layers(which, 0, cliprect, enablemask);
}

static INT32 SingleScreenModeChangeCheck()
{
	// widescreen (416px) mode is used to display the disclaimer as the game
	// boots up (sonic, dbzvrvs + orunnersj), after the disclaimer, it switches
	// to 320px and stays there until switched off and restarted again. To
	// avoid having change modes, we center the disclaimer text on the 320px
	// screen.  ref. fake_wide_screen & notes in update_tilemap_text().
	UINT16 *vidram = (UINT16*)DrvVidRAM;
	INT32 screensize = (BURN_ENDIAN_SWAP_INT16(vidram[0x1ff00/2]) & 0x8000) ? 416 : 320;
	fake_wide_screen = (BURN_ENDIAN_SWAP_INT16(vidram[0x1ff00/2]) & 0x8000 && nScreenWidth != 416 && !can_modechange) ? 1 : 0;

	// Rad Rally and Slip Stream actually need to change resolutions
	if (can_modechange && screensize != nScreenWidth)
	{
		BurnTransferSetDimensions(screensize, 224);
		GenericTilesSetClipRaw(0, screensize, 0, 224);
		BurnDrvSetVisibleSize(screensize, 224);
		ReinitialiseVideo(); // re-inits video subsystem (pBurnDraw)
		BurnTransferRealloc(); // re-inits pTransDraw

		if (is_slipstrm || is_radr) {
			BurnShiftScreenSizeChanged();
		}

		return 1; // don't draw this time around
	}

	return 0;
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x8000; i++) {
			DrvPalette[i] = BurnHighCol(pal5bit(i >> 10), pal5bit(i >> 5), pal5bit(i), 0);
		}
		DrvRecalc = 0;
	}

	if (SingleScreenModeChangeCheck()) {
		return 1; // if mode changed, don't draw this time around
	}

	draw_screen(0);

	if (screen_vflip) {
		BurnTransferFlip(0, 1);
	}

	BurnTransferCopy(DrvPalette);

	if (is_slipstrm || is_radr) {
		BurnShiftRenderDoubleSize();
	}

	return 0;
}

static INT32 MultiScreenCheck()
{
	INT32 screensize = (DrvDips[1] & 1) ? 320 : 640;
	if (screensize != nScreenWidth)
	{
		BurnTransferSetDimensions(screensize, 224);
		GenericTilesSetClipRaw(0, screensize, 0, 224);
		BurnDrvSetVisibleSize(screensize, 224);
		if (screensize == 320) {
			BurnDrvSetAspect(4, 3);
			MultiPCMSetMono(1);
		} else {
			BurnDrvSetAspect(8, 3);
			MultiPCMSetMono(0);
		}
		ReinitialiseVideo(); // re-inits video subsystem (pBurnDraw)
		BurnTransferRealloc(); // re-inits pTransDraw

		return 1; // don't draw this time around
	}

	return 0;
}

static INT32 MultiDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x8000; i++) {
			DrvPalette[i] = BurnHighCol(pal5bit(i >> 10), pal5bit(i >> 5), pal5bit(i), 0);
		}
		DrvRecalc = 0;
	}

	if (MultiScreenCheck()) {
		return 1; // if mode changed, don't draw this time around
	}

	// orunnersj uses wide-mode for disclaimer
	UINT16 *vidram = (UINT16*)DrvVidRAM;
	fake_wide_screen = (BURN_ENDIAN_SWAP_INT16(vidram[0x1ff00/2]) & 0x8000) ? 2 : 0;

	if (~DrvDips[1] & 1) {
		draw_screen(1);

		for (INT32 y = 0; y < 224; y++)
		{
			memcpy (BurnBitmapGetPosition(31, 0, y), BurnBitmapGetPosition(0, 0, y), 320 * sizeof(UINT16));
		}
	}

	draw_screen(0);

	if (~DrvDips[1] & 1) {
		for (INT32 y = 0; y < 224; y++)
		{
			memcpy (BurnBitmapGetPosition(0, 320, y), BurnBitmapGetPosition(31, 0, y), 320 * sizeof(UINT16));
		}
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void clear_opposites(UINT16 &i)
{
	if ((i & 0x30) == 0x00) i |= 0x30;
	if ((i & 0xc0) == 0x00) i |= 0xc0;
}

static void check_opposites()
{
	switch (clr_opposites) {
		case 2:
			clear_opposites(DrvInputs[0]);
			clear_opposites(DrvInputs[1]);
			break;
		case 4:
			clear_opposites(DrvInputs[0]);
			clear_opposites(DrvInputs[1]);
			clear_opposites(DrvExtra[0]);
			clear_opposites(DrvExtra[1]);
			break;
		case 6:
			clear_opposites(DrvInputs[0]);
			clear_opposites(DrvInputs[1]);
			clear_opposites(DrvExtra[0]);
			clear_opposites(DrvExtra[1]);
			clear_opposites(DrvInputs[7]);
			clear_opposites(DrvInputs[8]);
			break;
		case 0x2f: // titlef
			clear_opposites(DrvInputs[0]);
			clear_opposites(DrvInputs[1]);
			clear_opposites(DrvInputs[6]);
			clear_opposites(DrvInputs[7]);
			break;
	}
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	v60NewFrame();
	if (use_v25) VezNewFrame();
	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));
		memset (DrvExtra, 0xff, sizeof(DrvExtra));

		if (is_scross) { // button 2 (wheelie) is active high
			DrvInputs[0] = 0xfd;
			DrvInputs[8] = 0xfd;
		}
		if (is_ga2_spidman) { // fake p3,p4 coin slots
			if (DrvJoyF[2]) DrvJoy5[2] = 1;
			if (DrvJoyF[3]) DrvJoy5[3] = 1;
		}
		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[ 0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[ 1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[ 2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[ 3] ^= (DrvJoy4[i] & 1) << i;
			DrvInputs[ 4] ^= (DrvJoy5[i] & 1) << i;
			DrvInputs[ 5] ^= (DrvJoy6[i] & 1) << i;
			DrvInputs[ 6] ^= (DrvJoy7[i] & 1) << i;
			DrvInputs[ 7] ^= (DrvJoy8[i] & 1) << i;
			DrvInputs[ 8] ^= (DrvJoy9[i] & 1) << i;
			DrvInputs[ 9] ^= (DrvJoy10[i] & 1) << i;
			DrvInputs[10] ^= (DrvJoy11[i] & 1) << i;
			DrvInputs[11] ^= (DrvJoy12[i] & 1) << i;
			DrvInputs[12] ^= (DrvJoy13[i] & 1) << i;
			DrvInputs[13] ^= (DrvJoy14[i] & 1) << i;
			DrvInputs[14] ^= (DrvJoy15[i] & 1) << i;
			DrvInputs[15] ^= (DrvJoy16[i] & 1) << i;

			DrvExtra[0] ^= (DrvJoyX1[i] & 1) << i;
			DrvExtra[1] ^= (DrvJoyX2[i] & 1) << i;
			DrvExtra[2] ^= (DrvJoyX3[i] & 1) << i;
			DrvExtra[3] ^= (DrvJoyX4[i] & 1) << i;
		}

		check_opposites();

		CurveInitSet();

		if (is_slipstrm || is_radr) { // gear shifter stuff
			BurnShiftInputCheckToggle(DrvJoy1[0]);

			DrvInputs[0] = (DrvInputs[0] & ~1) | !bBurnShiftStatus;
		}

		if (has_gun) {
			BurnGunMakeInputs(0, Analog[0], Analog[1]);
			BurnGunMakeInputs(1, Analog[2], Analog[3]);
		}

		if (is_radm) {
			Radm_analog_target = CURVE[ProcessAnalog(Analog[0], 0, INPUT_DEADZONE, 0x00, 0xff)];
		}

		if (is_sonic) {
			BurnTrackballConfig(0, AXIS_REVERSED, AXIS_NORMAL);
			BurnTrackballFrame(0, Analog[0], Analog[1], 0x01, 0x20);
			BurnTrackballUpdate(0);

			BurnTrackballConfig(1, AXIS_REVERSED, AXIS_NORMAL);
			BurnTrackballFrame(1, Analog[2], Analog[3], 0x01, 0x20);
			BurnTrackballUpdate(1);

			BurnTrackballConfig(2, AXIS_REVERSED, AXIS_NORMAL);
			BurnTrackballFrame(2, Analog[4], Analog[5], 0x01, 0x20);
			BurnTrackballUpdate(2);
		}
	}

	INT32 nInterleave = 262;
	INT32 nCyclesTotal[3] = { ((is_multi32) ? 20000000 : 16107950) / 60, 10000000 / 60, 8053975 / 60 };
	INT32 nCyclesDone[3] = { nExtraCycles[0], nExtraCycles[1], 0 };

	v60Open(0);
	if (use_v25) VezOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		INT32 scanline = (i + 224) % 262;

		INT32 cyc = v60TotalCycles();

		if (scanline == 224) {
			signal_v60_irq(MAIN_IRQ_VBSTART);

			if (system32_prot_vblank)
				system32_prot_vblank();
		}

		if (scanline == 0) {
			signal_v60_irq(MAIN_IRQ_VBSTOP);
		}

		if (scanline == 1) {
			update_sprites();
		}

		do {
			delayed_int = 0;
			CPU_RUN(0, v60);
			if (delayed_int) {
				update_irq_state();
			}
		} while (delayed_int == 1);

		cyc = v60TotalCycles() - cyc; // cycles ran
		if (timer_0_cycles > 0) {
			timer_0_cycles -= cyc;
			if (timer_0_cycles <= 0) {
				signal_v60_irq(MAIN_IRQ_TIMER0);
				timer_0_cycles = -1;
			}
		}

		if (timer_1_cycles > 0) {
			timer_1_cycles -= cyc;
			if (timer_1_cycles <= 0) {
				signal_v60_irq(MAIN_IRQ_TIMER1);
				timer_1_cycles = -1;
			}
		}

		if (use_v25) CPU_RUN(1, Vez);

		CPU_RUN_TIMER(2);
	}

	ZetClose();
	if (use_v25) VezClose();
	v60Close();

	if (pBurnSoundOut) {
		BurnYM3438Update(pBurnSoundOut, nBurnSoundLen);
		if (is_multi32) {
			MultiPCMUpdate(pBurnSoundOut, nBurnSoundLen);
		} else {
			RF5C68PCMUpdate(pBurnSoundOut, nBurnSoundLen);
		}
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	nExtraCycles[0] = nCyclesDone[0] - nCyclesTotal[0];
	nExtraCycles[1] = (use_v25) ? (nCyclesDone[1] - nCyclesTotal[1]) : 0;

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin != NULL) {
		*pnMin = 0x029698;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		v60Scan(nAction);
		ZetScan(nAction);

		BurnYM3438Scan(nAction, pnMin);
		if (is_multi32) {
			MultiPCMScan(nAction, pnMin);
		} else {
			RF5C68PCMScan(nAction, pnMin);
		}
		EEPROMScan(nAction, pnMin);
		if (use_v25) VezScan(nAction);

		if (has_gun) BurnGunScan();
		if (is_sonic) BurnTrackballScan();

		if (is_slipstrm || is_radr) BurnShiftScan(nAction);

		SCAN_VAR(Radm_analog_adder);
		SCAN_VAR(Radm_analog_target);
		SCAN_VAR(mixer_control);
		SCAN_VAR(sprite_control);
		SCAN_VAR(sprite_control_latched);
		SCAN_VAR(sprite_render_count);
		SCAN_VAR(v60_irq_control);
		SCAN_VAR(v60_irq_vector);

		SCAN_VAR(analog_value);
		SCAN_VAR(analog_bank);

		SCAN_VAR(sound_irq_control);
		SCAN_VAR(sound_irq_input);
		SCAN_VAR(sound_dummy_data);
		SCAN_VAR(sound_bank);
		SCAN_VAR(pcm_bankdata);

		SCAN_VAR(misc_io_data);

		SCAN_VAR(timer_0_cycles);
		SCAN_VAR(timer_1_cycles);

		SCAN_VAR(system32_displayenable);
		SCAN_VAR(system32_tilebank_external);

		SCAN_VAR(nExtraCycles);

		SCAN_VAR(sonic_delta);

		BurnRandomScan(nAction);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		ZetMapMemory(DrvZ80ROM + sound_bank * 0x2000, 0xa000, 0xbfff, MAP_ROM);
		ZetClose();

		// force sprite update
		sprite_erase_buffer();
		sprite_swap_buffers();
		sprite_render_list();

		if (is_multi32) {
			pcm_bankswitch(pcm_bankdata);
		}
	}

	return 0;
}


// Air Rescue (World)

static struct BurnRomInfo arescueRomDesc[] = {
	{ "epr-14542.ic13",				0x020000, 0x6d39fc18, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "epr-14541.ic6",				0x020000, 0x5c3f2ec5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-14509.ic14",				0x080000, 0xdaa5a356, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-14508.ic7",				0x080000, 0x6702c14d, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-14513.ic35",				0x040000, 0xf9a884cd, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #0 Code
	{ "mpr-14512.ic31",				0x080000, 0x9da48051, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "mpr-14511.ic26",				0x080000, 0x074c53cc, 3 | BRF_PRG | BRF_ESS }, //  6
	{ "mpr-14510.ic22",				0x080000, 0x5ea6d52d, 3 | BRF_PRG | BRF_ESS }, //  7

	{ "mpr-14499.ic38",				0x080000, 0xfadc4b2b, 3 | BRF_GRA },           //  8 Main Layer Tiles
	{ "mpr-14498.ic34",				0x080000, 0xd4a764bd, 3 | BRF_GRA },           //  9
	{ "mpr-14497.ic29",				0x080000, 0xebd7ed17, 3 | BRF_GRA },           // 10
	{ "mpr-14496.ic25",				0x080000, 0x737da16c, 3 | BRF_GRA },           // 11

	{ "mpr-14507.ic36",				0x100000, 0xdb3f59ec, 4 | BRF_GRA },           // 12 Main Sprites
	{ "mpr-14506.ic32",				0x100000, 0x5dd8fb6b, 4 | BRF_GRA },           // 13
	{ "mpr-14505.ic27",				0x100000, 0xbe142c1f, 4 | BRF_GRA },           // 14
	{ "mpr-14504.ic23",				0x100000, 0x46dd038d, 4 | BRF_GRA },           // 15
	{ "mpr-14503.ic37",				0x100000, 0x90556aca, 4 | BRF_GRA },           // 16
	{ "mpr-14502.ic33",				0x100000, 0x988555a9, 4 | BRF_GRA },           // 17
	{ "mpr-14501.ic28",				0x100000, 0x4662bb41, 4 | BRF_GRA },           // 18
	{ "mpr-14500.ic24",				0x100000, 0x0a064e9b, 4 | BRF_GRA },           // 19

	{ "d7725.01",					0x002800, 0xa7ec5644, 0 | BRF_PRG | BRF_ESS }, // 20 DSP Code

	{ "epr-14542.ic13",				0x020000, 0x6d39fc18, 0 | BRF_PRG | BRF_ESS }, // 21 V60 #1 Code (Sub)
	{ "epr-14541.ic6",				0x020000, 0x5c3f2ec5, 0 | BRF_PRG | BRF_ESS }, // 22
	{ "epr-14509.ic14",				0x080000, 0xdaa5a356, 0 | BRF_PRG | BRF_ESS }, // 23
	{ "epr-14508.ic7",				0x080000, 0x6702c14d, 0 | BRF_PRG | BRF_ESS }, // 24

	{ "epr-14513.ic35",				0x040000, 0xf9a884cd, 0 | BRF_PRG | BRF_ESS }, // 25 Z80 #1 Code (Sub)
	{ "mpr-14512.ic31",				0x080000, 0x9da48051, 0 | BRF_PRG | BRF_ESS }, // 26
	{ "mpr-14511.ic26",				0x080000, 0x074c53cc, 0 | BRF_PRG | BRF_ESS }, // 27
	{ "mpr-14510.ic22",				0x080000, 0x5ea6d52d, 0 | BRF_PRG | BRF_ESS }, // 28

	{ "mpr-14496.ic25",				0x080000, 0x737da16c, 0 | BRF_GRA },           // 29 Sub Layer Tiles
	{ "mpr-14497.ic29",				0x080000, 0xebd7ed17, 0 | BRF_GRA },           // 30
	{ "mpr-14498.ic34",				0x080000, 0xd4a764bd, 0 | BRF_GRA },           // 31
	{ "mpr-14499.ic38",				0x080000, 0xfadc4b2b, 0 | BRF_GRA },           // 32

	{ "mpr-14500.ic24",				0x100000, 0x0a064e9b, 0 | BRF_GRA },           // 33 Sub Sprites
	{ "mpr-14501.ic28",				0x100000, 0x4662bb41, 0 | BRF_GRA },           // 34
	{ "mpr-14502.ic33",				0x100000, 0x988555a9, 0 | BRF_GRA },           // 35
	{ "mpr-14503.ic37",				0x100000, 0x90556aca, 0 | BRF_GRA },           // 36
	{ "mpr-14504.ic23",				0x100000, 0x46dd038d, 0 | BRF_GRA },           // 37
	{ "mpr-14505.ic27",				0x100000, 0xbe142c1f, 0 | BRF_GRA },           // 38
	{ "mpr-14506.ic32",				0x100000, 0x5dd8fb6b, 0 | BRF_GRA },           // 39
	{ "mpr-14507.ic36",				0x100000, 0xdb3f59ec, 0 | BRF_GRA },           // 40
};

STD_ROM_PICK(arescue)
STD_ROM_FN(arescue)

static UINT16 arescue_dsp_read(UINT32 offset)
{
	if (offset > 3) return 0;

	UINT16 *dsp_io = (UINT16*)DrvV25RAM; // re-use v25 ram

	if (offset == 2)
	{
		switch (BURN_ENDIAN_SWAP_INT16(dsp_io[0]))
		{
			case 3:
				dsp_io[0] = BURN_ENDIAN_SWAP_INT16(0x8000);
				dsp_io[1] = BURN_ENDIAN_SWAP_INT16(0x0001);
			break;

			case 6:
				dsp_io[0] = BURN_ENDIAN_SWAP_INT16(BURN_ENDIAN_SWAP_INT16(dsp_io[1]) << 2);
			break;
		}
	}

	return BURN_ENDIAN_SWAP_INT16(dsp_io[offset]);
}

static void arescue_dsp_write(UINT32 offset, UINT32 data, UINT32 mem_mask)
{
	UINT16 *dsp_io = (UINT16*)DrvV25RAM; // re-use v25 ram

	dsp_io[offset] = BURN_ENDIAN_SWAP_INT16((BURN_ENDIAN_SWAP_INT16(dsp_io[offset]) & ~mem_mask) | (data & mem_mask));
}

static INT32 ArescueInit()
{
	sprite_length = 0x800000;
	DrvLoadRoms(false);
	BurnAllocMemIndex();
	if (DrvLoadRoms(true)) return 1;

	system32_v60_map();
	system32_sound_init();
	tilemap_configure_allocate();

	v60Open(0);
	v60MapMemory(NULL,	0x810000, 0x8107ff, MAP_READ);	// unmap writes to this range to allow protection writes (page is 800)
	v60Close();

	protection_a00000_write = arescue_dsp_write;
	protection_a00000_read = arescue_dsp_read;

	custom_io_write_0  = arescue_custom_io_write;
	custom_io_read_0 = analog_custom_io_read;

	DrvDoReset();

	return 0;
}

struct BurnDriver BurnDrvArescue = {
	"arescue", NULL, NULL, NULL, "1992",
	"Air Rescue (World)\0", NULL, "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 1, HARDWARE_SEGA_SYSTEM32, GBF_SHOOT, 0,
	NULL, arescueRomInfo, arescueRomName, NULL, NULL, NULL, NULL, ArescueInputInfo, ArescueDIPInfo,
	ArescueInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};


// Air Rescue (US)

static struct BurnRomInfo arescueuRomDesc[] = {
	{ "epr-14540.ic13",				0x020000, 0xc2b4e5d0, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "epr-14539.ic6",				0x020000, 0x1a1b5532, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-14509.ic14",				0x080000, 0xdaa5a356, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-14508.ic7",				0x080000, 0x6702c14d, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-14513.ic35",				0x040000, 0xf9a884cd, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #0 Code
	{ "mpr-14512.ic31",				0x080000, 0x9da48051, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "mpr-14511.ic26",				0x080000, 0x074c53cc, 3 | BRF_PRG | BRF_ESS }, //  6
	{ "mpr-14510.ic22",				0x080000, 0x5ea6d52d, 3 | BRF_PRG | BRF_ESS }, //  7

	{ "mpr-14499.ic38",				0x080000, 0xfadc4b2b, 3 | BRF_GRA },           //  8 Main Layer Tiles
	{ "mpr-14498.ic34",				0x080000, 0xd4a764bd, 3 | BRF_GRA },           //  9
	{ "mpr-14497.ic29",				0x080000, 0xebd7ed17, 3 | BRF_GRA },           // 10
	{ "mpr-14496.ic25",				0x080000, 0x737da16c, 3 | BRF_GRA },           // 11

	{ "mpr-14507.ic36",				0x100000, 0xdb3f59ec, 4 | BRF_GRA },           // 12 Main Sprites
	{ "mpr-14506.ic32",				0x100000, 0x5dd8fb6b, 4 | BRF_GRA },           // 13
	{ "mpr-14505.ic27",				0x100000, 0xbe142c1f, 4 | BRF_GRA },           // 14
	{ "mpr-14504.ic23",				0x100000, 0x46dd038d, 4 | BRF_GRA },           // 15
	{ "mpr-14503.ic37",				0x100000, 0x90556aca, 4 | BRF_GRA },           // 16
	{ "mpr-14502.ic33",				0x100000, 0x988555a9, 4 | BRF_GRA },           // 17
	{ "mpr-14501.ic28",				0x100000, 0x4662bb41, 4 | BRF_GRA },           // 18
	{ "mpr-14500.ic24",				0x100000, 0x0a064e9b, 4 | BRF_GRA },           // 19

	{ "d7725.01",					0x002800, 0xa7ec5644, 0 | BRF_PRG | BRF_ESS }, // 20 DSP Code

	{ "epr-14540.ic13",				0x020000, 0xc2b4e5d0, 0 | BRF_PRG | BRF_ESS }, // 21 V60 #1 Code (Sub)
	{ "epr-14539.ic6",				0x020000, 0x1a1b5532, 0 | BRF_PRG | BRF_ESS }, // 22
	{ "epr-14509.ic14",				0x080000, 0xdaa5a356, 0 | BRF_PRG | BRF_ESS }, // 23
	{ "epr-14508.ic7",				0x080000, 0x6702c14d, 0 | BRF_PRG | BRF_ESS }, // 24

	{ "epr-14513.ic35",				0x040000, 0xf9a884cd, 0 | BRF_PRG | BRF_ESS }, // 25 Z80 #1 Code (Sub)
	{ "mpr-14512.ic31",				0x080000, 0x9da48051, 0 | BRF_PRG | BRF_ESS }, // 26
	{ "mpr-14511.ic26",				0x080000, 0x074c53cc, 0 | BRF_PRG | BRF_ESS }, // 27
	{ "mpr-14510.ic22",				0x080000, 0x5ea6d52d, 0 | BRF_PRG | BRF_ESS }, // 28

	{ "mpr-14496.ic25",				0x080000, 0x737da16c, 0 | BRF_GRA },           // 29 Sub Layer Tiles
	{ "mpr-14497.ic29",				0x080000, 0xebd7ed17, 0 | BRF_GRA },           // 30
	{ "mpr-14498.ic34",				0x080000, 0xd4a764bd, 0 | BRF_GRA },           // 31
	{ "mpr-14499.ic38",				0x080000, 0xfadc4b2b, 0 | BRF_GRA },           // 32

	{ "mpr-14500.ic24",				0x100000, 0x0a064e9b, 0 | BRF_GRA },           // 33 Sub Sprites
	{ "mpr-14501.ic28",				0x100000, 0x4662bb41, 0 | BRF_GRA },           // 34
	{ "mpr-14502.ic33",				0x100000, 0x988555a9, 0 | BRF_GRA },           // 35
	{ "mpr-14503.ic37",				0x100000, 0x90556aca, 0 | BRF_GRA },           // 36
	{ "mpr-14504.ic23",				0x100000, 0x46dd038d, 0 | BRF_GRA },           // 37
	{ "mpr-14505.ic27",				0x100000, 0xbe142c1f, 0 | BRF_GRA },           // 38
	{ "mpr-14506.ic32",				0x100000, 0x5dd8fb6b, 0 | BRF_GRA },           // 39
	{ "mpr-14507.ic36",				0x100000, 0xdb3f59ec, 0 | BRF_GRA },           // 40
};

STD_ROM_PICK(arescueu)
STD_ROM_FN(arescueu)

struct BurnDriver BurnDrvArescueu = {
	"arescueu", "arescue", NULL, NULL, "1992",
	"Air Rescue (US)\0", NULL, "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 1, HARDWARE_SEGA_SYSTEM32, GBF_SHOOT, 0,
	NULL, arescueuRomInfo, arescueuRomName, NULL, NULL, NULL, NULL, ArescueInputInfo, ArescueDIPInfo,
	ArescueInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};


// Air Rescue (Japan)

static struct BurnRomInfo arescuejRomDesc[] = {
	{ "epr-14515.ic13",				0x020000, 0xfb5eefbd, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "epr-14514.ic6",				0x020000, 0xebf6dfc5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-14509.ic14",				0x080000, 0xdaa5a356, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-14508.ic7",				0x080000, 0x6702c14d, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-14513.ic35",				0x040000, 0xf9a884cd, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #0 Code
	{ "mpr-14512.ic31",				0x080000, 0x9da48051, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "mpr-14511.ic26",				0x080000, 0x074c53cc, 3 | BRF_PRG | BRF_ESS }, //  6
	{ "mpr-14510.ic22",				0x080000, 0x5ea6d52d, 3 | BRF_PRG | BRF_ESS }, //  7

	{ "mpr-14499.ic38",				0x080000, 0xfadc4b2b, 3 | BRF_GRA },           //  8 Main Layer Tiles
	{ "mpr-14498.ic34",				0x080000, 0xd4a764bd, 3 | BRF_GRA },           //  9
	{ "mpr-14497.ic29",				0x080000, 0xebd7ed17, 3 | BRF_GRA },           // 10
	{ "mpr-14496.ic25",				0x080000, 0x737da16c, 3 | BRF_GRA },           // 11

	{ "mpr-14507.ic36",				0x100000, 0xdb3f59ec, 4 | BRF_GRA },           // 12 Main Sprites
	{ "mpr-14506.ic32",				0x100000, 0x5dd8fb6b, 4 | BRF_GRA },           // 13
	{ "mpr-14505.ic27",				0x100000, 0xbe142c1f, 4 | BRF_GRA },           // 14
	{ "mpr-14504.ic23",				0x100000, 0x46dd038d, 4 | BRF_GRA },           // 15
	{ "mpr-14503.ic37",				0x100000, 0x90556aca, 4 | BRF_GRA },           // 16
	{ "mpr-14502.ic33",				0x100000, 0x988555a9, 4 | BRF_GRA },           // 17
	{ "mpr-14501.ic28",				0x100000, 0x4662bb41, 4 | BRF_GRA },           // 18
	{ "mpr-14500.ic24",				0x100000, 0x0a064e9b, 4 | BRF_GRA },           // 19

	{ "d7725.01",					0x002800, 0xa7ec5644, 0 | BRF_PRG | BRF_ESS }, // 20 DSP Code

	{ "epr-14515.ic13",				0x020000, 0xfb5eefbd, 0 | BRF_PRG | BRF_ESS }, // 21 V60 #1 Code (Sub)
	{ "epr-14514.ic6",				0x020000, 0xebf6dfc5, 0 | BRF_PRG | BRF_ESS }, // 22
	{ "epr-14509.ic14",				0x080000, 0xdaa5a356, 0 | BRF_PRG | BRF_ESS }, // 23
	{ "epr-14508.ic7",				0x080000, 0x6702c14d, 0 | BRF_PRG | BRF_ESS }, // 24

	{ "epr-14513.ic35",				0x040000, 0xf9a884cd, 0 | BRF_PRG | BRF_ESS }, // 25 Z80 #1 Code (Sub)
	{ "mpr-14512.ic31",				0x080000, 0x9da48051, 0 | BRF_PRG | BRF_ESS }, // 26
	{ "mpr-14511.ic26",				0x080000, 0x074c53cc, 0 | BRF_PRG | BRF_ESS }, // 27
	{ "mpr-14510.ic22",				0x080000, 0x5ea6d52d, 0 | BRF_PRG | BRF_ESS }, // 28

	{ "mpr-14496.ic25",				0x080000, 0x737da16c, 0 | BRF_GRA },           // 29 Sub Layer Tiles
	{ "mpr-14497.ic29",				0x080000, 0xebd7ed17, 0 | BRF_GRA },           // 30
	{ "mpr-14498.ic34",				0x080000, 0xd4a764bd, 0 | BRF_GRA },           // 31
	{ "mpr-14499.ic38",				0x080000, 0xfadc4b2b, 0 | BRF_GRA },           // 32

	{ "mpr-14500.ic24",				0x100000, 0x0a064e9b, 0 | BRF_GRA },           // 33 Sub Sprites
	{ "mpr-14501.ic28",				0x100000, 0x4662bb41, 0 | BRF_GRA },           // 34
	{ "mpr-14502.ic33",				0x100000, 0x988555a9, 0 | BRF_GRA },           // 35
	{ "mpr-14503.ic37",				0x100000, 0x90556aca, 0 | BRF_GRA },           // 36
	{ "mpr-14504.ic23",				0x100000, 0x46dd038d, 0 | BRF_GRA },           // 37
	{ "mpr-14505.ic27",				0x100000, 0xbe142c1f, 0 | BRF_GRA },           // 38
	{ "mpr-14506.ic32",				0x100000, 0x5dd8fb6b, 0 | BRF_GRA },           // 39
	{ "mpr-14507.ic36",				0x100000, 0xdb3f59ec, 0 | BRF_GRA },           // 40
};

STD_ROM_PICK(arescuej)
STD_ROM_FN(arescuej)

struct BurnDriver BurnDrvArescuej = {
	"arescuej", "arescue", NULL, NULL, "1992",
	"Air Rescue (Japan)\0", NULL, "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 1, HARDWARE_SEGA_SYSTEM32, GBF_SHOOT, 0,
	NULL, arescuejRomInfo, arescuejRomName, NULL, NULL, NULL, NULL, ArescueInputInfo, ArescueDIPInfo,
	ArescueInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};


// Arabian Fight (World)

static struct BurnRomInfo arabfgtRomDesc[] = {
	{ "epr-14609.ic8",				0x020000, 0x6a43c7fb, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "epr-14592.ic18",				0x040000, 0xf7dff316, 2 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-14591.ic9",				0x040000, 0xbbd940fb, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "epr-14596.ic36",				0x020000, 0xbd01faec, 3 | BRF_PRG | BRF_ESS }, //  3 Z80 #0 Code
	{ "mpr-14595f.ic35",			0x100000, 0x5173d1af, 3 | BRF_PRG | BRF_ESS }, //  4
	{ "mpr-14594f.ic34",			0x100000, 0x01777645, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "mpr-14593f.ic24",			0x100000, 0xaa037047, 3 | BRF_PRG | BRF_ESS }, //  6

	{ "epr-14468-01.u3",			0x010000, 0xc3c591e4, 4 | BRF_PRG | BRF_ESS }, //  7 V25 Code (Protection)

	{ "mpr-14599f.ic14",			0x200000, 0x94f1cf10, 1 | BRF_GRA },           //  8 Main Layer Tiles
	{ "mpr-14598f.ic5",				0x200000, 0x010656f3, 1 | BRF_GRA },           //  9

	{ "mpr-14600f.ic32",			0x200000, 0xe860988a, 2 | BRF_GRA },           // 10 Main Sprites
	{ "mpr-14602.ic30",				0x200000, 0x64524e4d, 2 | BRF_GRA },           // 11
	{ "mpr-14604.ic28",				0x200000, 0x5f8d5167, 2 | BRF_GRA },           // 12
	{ "mpr-14606.ic26",				0x200000, 0x7047f437, 2 | BRF_GRA },           // 13
	{ "mpr-14601f.ic31",			0x200000, 0xa2f3bb32, 2 | BRF_GRA },           // 14
	{ "mpr-14603.ic29",				0x200000, 0xf6ce494b, 2 | BRF_GRA },           // 15
	{ "mpr-14605.ic27",				0x200000, 0xaaf52697, 2 | BRF_GRA },           // 16
	{ "mpr-14607.ic25",				0x200000, 0xb70b0735, 2 | BRF_GRA },           // 17
};

STD_ROM_PICK(arabfgt)
STD_ROM_FN(arabfgt)

#define xxxx 0x00
static UINT8 arf_opcode_table[256] = {
		xxxx,xxxx,0x43,xxxx,xxxx,xxxx,0x83,xxxx,xxxx,xxxx,0xEA,xxxx,xxxx,0xBC,0x73,xxxx,
		xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,
		0x3A,xxxx,xxxx,0xBE,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,0x80,xxxx,
		xxxx,0xB5,xxxx,xxxx,xxxx,xxxx,xxxx,0x26,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,
		xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,0xE8,0x8D,xxxx,0x8B,xxxx,
		xxxx,xxxx,xxxx,0xFA,xxxx,0x8A,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,
		xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,
		xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,0xBA,0x88,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,
		xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,
		xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,0xBB,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,
		xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,0x75,xxxx,0xBF,
		xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,0x03,0x3B,0x8E,0x74,xxxx,xxxx,0x81,xxxx,
		xxxx,xxxx,xxxx,0xC3,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,
		xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,0xB9,0xB2,xxxx,xxxx,xxxx,xxxx,0x49,xxxx,xxxx,
		xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,0xEB,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,
		xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,0x02,0xB8
};
#undef xxxx

static INT32 ArabfgtInit()
{
	sprite_length = 0;
	DrvLoadRoms(false);
	BurnAllocMemIndex();
	if (DrvLoadRoms(true)) return 1;

	system32_v60_map();
	system32_sound_init();
	tilemap_configure_allocate();

	v25_protection_init(arf_opcode_table);

	custom_io_read_0 = extra_custom_io_read;

	clr_opposites = 4;

	DrvDoReset();

	return 0;
}

struct BurnDriver BurnDrvArabfgt = {
	"arabfgt", NULL, NULL, NULL, "1991",
	"Arabian Fight (World)\0", "imperfect graphics", "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 4, HARDWARE_SEGA_SYSTEM32, GBF_SCRFIGHT, 0,
	NULL, arabfgtRomInfo, arabfgtRomName, NULL, NULL, NULL, NULL, ArabfgtInputInfo, ArabfgtDIPInfo,
	ArabfgtInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};


// Arabian Fight (US)

static struct BurnRomInfo arabfgtuRomDesc[] = {
	{ "epr-14608.ic8",				0x020000, 0xcd5efba9, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "epr-14592.ic18",				0x040000, 0xf7dff316, 2 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-14591.ic9",				0x040000, 0xbbd940fb, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "epr-14596.ic36",				0x020000, 0xbd01faec, 3 | BRF_PRG | BRF_ESS }, //  3 Z80 #0 Code
	{ "mpr-14595f.ic35",			0x100000, 0x5173d1af, 3 | BRF_PRG | BRF_ESS }, //  4
	{ "mpr-14594f.ic34",			0x100000, 0x01777645, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "mpr-14593f.ic24",			0x100000, 0xaa037047, 3 | BRF_PRG | BRF_ESS }, //  6

	{ "epr-14468-01.u3",			0x010000, 0xc3c591e4, 4 | BRF_PRG | BRF_ESS }, //  7 V25 Code (Protection)

	{ "mpr-14599f.ic14",			0x200000, 0x94f1cf10, 1 | BRF_GRA },           //  8 Main Layer Tiles
	{ "mpr-14598f.ic5",				0x200000, 0x010656f3, 1 | BRF_GRA },           //  9

	{ "mpr-14600f.ic32",			0x200000, 0xe860988a, 2 | BRF_GRA },           // 10 Main Sprites
	{ "mpr-14602.ic30",				0x200000, 0x64524e4d, 2 | BRF_GRA },           // 11
	{ "mpr-14604.ic28",				0x200000, 0x5f8d5167, 2 | BRF_GRA },           // 12
	{ "mpr-14606.ic26",				0x200000, 0x7047f437, 2 | BRF_GRA },           // 13
	{ "mpr-14601f.ic31",			0x200000, 0xa2f3bb32, 2 | BRF_GRA },           // 14
	{ "mpr-14603.ic29",				0x200000, 0xf6ce494b, 2 | BRF_GRA },           // 15
	{ "mpr-14605.ic27",				0x200000, 0xaaf52697, 2 | BRF_GRA },           // 16
	{ "mpr-14607.ic25",				0x200000, 0xb70b0735, 2 | BRF_GRA },           // 17
};

STD_ROM_PICK(arabfgtu)
STD_ROM_FN(arabfgtu)

struct BurnDriver BurnDrvArabfgtu = {
	"arabfgtu", "arabfgt", NULL, NULL, "1991",
	"Arabian Fight (US)\0", "imperfect graphics", "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_SEGA_SYSTEM32, GBF_SCRFIGHT, 0,
	NULL, arabfgtuRomInfo, arabfgtuRomName, NULL, NULL, NULL, NULL, ArabfgtuInputInfo, ArabfgtuDIPInfo,
	ArabfgtInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};


// Arabian Fight (Japan)

static struct BurnRomInfo arabfgtjRomDesc[] = {
	{ "epr-14597.ic8",				0x020000, 0x7a6fe222, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "epr-14592.ic18",				0x040000, 0xf7dff316, 2 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-14591.ic9",				0x040000, 0xbbd940fb, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "epr-14596.ic36",				0x020000, 0xbd01faec, 3 | BRF_PRG | BRF_ESS }, //  3 Z80 #0 Code
	{ "mpr-14595f.ic35",			0x100000, 0x5173d1af, 3 | BRF_PRG | BRF_ESS }, //  4
	{ "mpr-14594f.ic34",			0x100000, 0x01777645, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "mpr-14593f.ic24",			0x100000, 0xaa037047, 3 | BRF_PRG | BRF_ESS }, //  6

	{ "epr-14468-01.u3",			0x010000, 0xc3c591e4, 4 | BRF_PRG | BRF_ESS }, //  7 V25 Code (Protection)

	{ "mpr-14599f.ic14",			0x200000, 0x94f1cf10, 1 | BRF_GRA },           //  8 Main Layer Tiles
	{ "mpr-14598f.ic5",				0x200000, 0x010656f3, 1 | BRF_GRA },           //  9

	{ "mpr-14600f.ic32",			0x200000, 0xe860988a, 2 | BRF_GRA },           // 10 Main Sprites
	{ "mpr-14602.ic30",				0x200000, 0x64524e4d, 2 | BRF_GRA },           // 11
	{ "mpr-14604.ic28",				0x200000, 0x5f8d5167, 2 | BRF_GRA },           // 12
	{ "mpr-14606.ic26",				0x200000, 0x7047f437, 2 | BRF_GRA },           // 13
	{ "mpr-14601f.ic31",			0x200000, 0xa2f3bb32, 2 | BRF_GRA },           // 14
	{ "mpr-14603.ic29",				0x200000, 0xf6ce494b, 2 | BRF_GRA },           // 15
	{ "mpr-14605.ic27",				0x200000, 0xaaf52697, 2 | BRF_GRA },           // 16
	{ "mpr-14607.ic25",				0x200000, 0xb70b0735, 2 | BRF_GRA },           // 17
};

STD_ROM_PICK(arabfgtj)
STD_ROM_FN(arabfgtj)

struct BurnDriver BurnDrvArabfgtj = {
	"arabfgtj", "arabfgt", NULL, NULL, "1991",
	"Arabian Fight (Japan)\0", "imperfect graphics", "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_SEGA_SYSTEM32, GBF_SCRFIGHT, 0,
	NULL, arabfgtjRomInfo, arabfgtjRomName, NULL, NULL, NULL, NULL, ArabfgtInputInfo, ArabfgtDIPInfo,
	ArabfgtInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};


// Golden Axe: The Revenge of Death Adder (World, Rev B)

static struct BurnRomInfo ga2RomDesc[] = {
	{ "epr-14961b.ic17",			0x020000, 0xd9cd8885, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "epr-14958b.ic8",				0x020000, 0x0be324a3, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-15148b.ic18",			0x040000, 0xc477a9fd, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-15147b.ic9",				0x040000, 0x1bb676ea, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-14945.ic36",				0x010000, 0x4781d4cb, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #0 Code
	{ "mpr-14944.ic35",				0x100000, 0xfd4d4b86, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "mpr-14943.ic34",				0x100000, 0x24d40333, 3 | BRF_PRG | BRF_ESS }, //  6
	{ "mpr-14942.ic24",				0x100000, 0xa89b0e90, 3 | BRF_PRG | BRF_ESS }, //  7

	{ "epr-14468-02.u3",			0x010000, 0x77634daa, 4 | BRF_PRG | BRF_ESS }, //  8 V25 Code (Protection)

	{ "mpr-14948.ic14",				0x200000, 0x75050d4a, 1 | BRF_GRA },           //  9 Main Layer Tiles
	{ "mpr-14947.ic5",				0x200000, 0xb53e62f4, 1 | BRF_GRA },           // 10

	{ "mpr-14949.ic32",				0x200000, 0x152c716c, 2 | BRF_GRA },           // 11 Main Sprites
	{ "mpr-14951.ic30",				0x200000, 0xfdb1a534, 2 | BRF_GRA },           // 12
	{ "mpr-14953.ic28",				0x200000, 0x33bd1c15, 2 | BRF_GRA },           // 13
	{ "mpr-14955.ic26",				0x200000, 0xe42684aa, 2 | BRF_GRA },           // 14
	{ "mpr-14950.ic31",				0x200000, 0x15fd0026, 2 | BRF_GRA },           // 15
	{ "mpr-14952.ic29",				0x200000, 0x96f96613, 2 | BRF_GRA },           // 16
	{ "mpr-14954.ic27",				0x200000, 0x39b2ac9e, 2 | BRF_GRA },           // 17
	{ "mpr-14956.ic25",				0x200000, 0x298fca50, 2 | BRF_GRA },           // 18
};

STD_ROM_PICK(ga2)
STD_ROM_FN(ga2)

#define xxxx 0x00
static UINT8 ga2_opcode_table[256] = {
		xxxx,xxxx,0xEA,xxxx,xxxx,0x8B,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,
		xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,0xFA,
		xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,0x3B,xxxx,0x49,xxxx,xxxx,xxxx,
		xxxx,xxxx,xxxx,xxxx,xxxx,0xE8,xxxx,xxxx,0x75,xxxx,xxxx,xxxx,xxxx,0x3A,xxxx,xxxx,
		xxxx,xxxx,xxxx,xxxx,0x8D,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,0xBF,xxxx,0x88,xxxx,
		xxxx,xxxx,xxxx,0x81,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,
		xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,
		0x02,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,0xBC,
		xxxx,xxxx,xxxx,0x8A,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,0x83,xxxx,xxxx,xxxx,xxxx,xxxx,
		xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,0xB8,0x26,xxxx,
		xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,0xB5,xxxx,0xEB,
		xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,0xB2,xxxx,xxxx,xxxx,xxxx,
		xxxx,xxxx,xxxx,0xC3,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,
		xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,0xB9,0xBB,xxxx,0x43,xxxx,xxxx,xxxx,xxxx,
		xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,
		xxxx,xxxx,0x8E,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,xxxx,0xBE,xxxx,0x80,xxxx,xxxx
};
#undef xxxx

static INT32 Ga2Init()
{
	is_ga2_spidman = 1;
	sprite_length = 0;
	DrvLoadRoms(false);
	BurnAllocMemIndex();
	if (DrvLoadRoms(true)) return 1;

	system32_v60_map();
	system32_sound_init();
	tilemap_configure_allocate();

	v25_protection_init(ga2_opcode_table);
	custom_io_read_0 = extra_custom_io_read;

	clr_opposites = 4;

	DrvDoReset();

	return 0;
}

struct BurnDriver BurnDrvGa2 = {
	"ga2", NULL, NULL, NULL, "1992",
	"Golden Axe: The Revenge of Death Adder (World, Rev B)\0", NULL, "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_SEGA_SYSTEM32, GBF_SCRFIGHT, 0,
	NULL, ga2RomInfo, ga2RomName, NULL, NULL, NULL, NULL, Ga2InputInfo, Ga2DIPInfo,
	Ga2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};


// Golden Axe: The Revenge of Death Adder (US, Rev A)

static struct BurnRomInfo ga2uRomDesc[] = {
	{ "epr-14960a.ic17",			0x020000, 0x87182fea, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "epr-14957a.ic8",				0x020000, 0xab787cf4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-15146a.ic18",			0x040000, 0x7293d5c3, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-15145a.ic9",				0x040000, 0x0da61782, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-14945.ic36",				0x010000, 0x4781d4cb, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #0 Code
	{ "mpr-14944.ic35",				0x100000, 0xfd4d4b86, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "mpr-14943.ic34",				0x100000, 0x24d40333, 3 | BRF_PRG | BRF_ESS }, //  6
	{ "mpr-14942.ic24",				0x100000, 0xa89b0e90, 3 | BRF_PRG | BRF_ESS }, //  7

	{ "epr-14468-02.u3",			0x010000, 0x77634daa, 4 | BRF_PRG | BRF_ESS }, //  8 V25 Code (Protection)

	{ "mpr-14948.ic14",				0x200000, 0x75050d4a, 1 | BRF_GRA },           //  9 Main Layer Tiles
	{ "mpr-14947.ic5",				0x200000, 0xb53e62f4, 1 | BRF_GRA },           // 10

	{ "mpr-14949.ic32",				0x200000, 0x152c716c, 2 | BRF_GRA },           // 11 Main Sprites
	{ "mpr-14951.ic30",				0x200000, 0xfdb1a534, 2 | BRF_GRA },           // 12
	{ "mpr-14953.ic28",				0x200000, 0x33bd1c15, 2 | BRF_GRA },           // 13
	{ "mpr-14955.ic26",				0x200000, 0xe42684aa, 2 | BRF_GRA },           // 14
	{ "mpr-14950.ic31",				0x200000, 0x15fd0026, 2 | BRF_GRA },           // 15
	{ "mpr-14952.ic29",				0x200000, 0x96f96613, 2 | BRF_GRA },           // 16
	{ "mpr-14954.ic27",				0x200000, 0x39b2ac9e, 2 | BRF_GRA },           // 17
	{ "mpr-14956.ic25",				0x200000, 0x298fca50, 2 | BRF_GRA },           // 18
};

STD_ROM_PICK(ga2u)
STD_ROM_FN(ga2u)

struct BurnDriver BurnDrvGa2u = {
	"ga2u", "ga2", NULL, NULL, "1992",
	"Golden Axe: The Revenge of Death Adder (US, Rev A)\0", NULL, "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_SEGA_SYSTEM32, GBF_SCRFIGHT, 0,
	NULL, ga2uRomInfo, ga2uRomName, NULL, NULL, NULL, NULL, Ga2uInputInfo, Ga2uDIPInfo,
	Ga2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};


// Golden Axe: The Revenge of Death Adder (Japan)

static struct BurnRomInfo ga2jRomDesc[] = {
	{ "epr-14959.ic17",				0x020000, 0xf1929177, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "epr-14946.ic8",				0x020000, 0xeacafe94, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-14941.ic18",				0x040000, 0x0ffb8203, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-14940.ic9",				0x040000, 0x3b5b3084, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-14945.ic36",				0x010000, 0x4781d4cb, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #0 Code
	{ "mpr-14944.ic35",				0x100000, 0xfd4d4b86, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "mpr-14943.ic34",				0x100000, 0x24d40333, 3 | BRF_PRG | BRF_ESS }, //  6
	{ "mpr-14942.ic24",				0x100000, 0xa89b0e90, 3 | BRF_PRG | BRF_ESS }, //  7

	{ "epr-14468-02.u3",			0x010000, 0x77634daa, 4 | BRF_PRG | BRF_ESS }, //  8 V25 Code (Protection)

	{ "mpr-14948.ic14",				0x200000, 0x75050d4a, 1 | BRF_GRA },           //  9 Main Layer Tiles
	{ "mpr-14947.ic5",				0x200000, 0xb53e62f4, 1 | BRF_GRA },           // 10

	{ "mpr-14949.ic32",				0x200000, 0x152c716c, 2 | BRF_GRA },           // 11 Main Sprites
	{ "mpr-14951.ic30",				0x200000, 0xfdb1a534, 2 | BRF_GRA },           // 12
	{ "mpr-14953.ic28",				0x200000, 0x33bd1c15, 2 | BRF_GRA },           // 13
	{ "mpr-14955.ic26",				0x200000, 0xe42684aa, 2 | BRF_GRA },           // 14
	{ "mpr-14950.ic31",				0x200000, 0x15fd0026, 2 | BRF_GRA },           // 15
	{ "mpr-14952.ic29",				0x200000, 0x96f96613, 2 | BRF_GRA },           // 16
	{ "mpr-14954.ic27",				0x200000, 0x39b2ac9e, 2 | BRF_GRA },           // 17
	{ "mpr-14956.ic25",				0x200000, 0x298fca50, 2 | BRF_GRA },           // 18
};

STD_ROM_PICK(ga2j)
STD_ROM_FN(ga2j)

struct BurnDriver BurnDrvGa2j = {
	"ga2j", "ga2", NULL, NULL, "1992",
	"Golden Axe: The Revenge of Death Adder (Japan)\0", NULL, "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_SEGA_SYSTEM32, GBF_SCRFIGHT, 0,
	NULL, ga2jRomInfo, ga2jRomName, NULL, NULL, NULL, NULL, Ga2InputInfo, Ga2DIPInfo,
	Ga2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};


// Dark Edge (World)

static struct BurnRomInfo darkedgeRomDesc[] = {
	{ "epr-15246.ic8",				0x080000, 0xc0bdceeb, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code

	{ "epr-15243.ic36",				0x020000, 0x08ca5f11, 3 | BRF_PRG | BRF_ESS }, //  1 Z80 #0 Code
	{ "mpr-15242.ic35",				0x100000, 0xffb7d917, 3 | BRF_PRG | BRF_ESS }, //  2
	{ "mpr-15241.ic34",				0x100000, 0x8eccc4fe, 3 | BRF_PRG | BRF_ESS }, //  3
	{ "mpr-15240.ic24",				0x100000, 0x867d59e8, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "mpr-15248.ic14",				0x080000, 0x185b308b, 1 | BRF_GRA },           //  5 Main Layer Tiles
	{ "mpr-15247.ic5",				0x080000, 0xbe21548c, 1 | BRF_GRA },           //  6

	{ "mpr-15249.ic32",				0x200000, 0x2b4371a8, 2 | BRF_GRA },           //  7 Main Sprites
	{ "mpr-15251.ic30", 			0x200000, 0xefe2d689, 2 | BRF_GRA },           //  8
	{ "mpr-15253.ic28",				0x200000, 0x8356ed01, 2 | BRF_GRA },           //  9
	{ "mpr-15255.ic26",				0x200000, 0xff04a5b0, 2 | BRF_GRA },           // 10
	{ "mpr-15250.ic31",				0x200000, 0xc5cab71a, 2 | BRF_GRA },           // 11
	{ "mpr-15252.ic29",				0x200000, 0xf8885fea, 2 | BRF_GRA },           // 12
	{ "mpr-15254.ic27",				0x200000, 0x7765424b, 2 | BRF_GRA },           // 13
	{ "mpr-15256.ic25",				0x200000, 0x44c36b62, 2 | BRF_GRA },           // 14
};

STD_ROM_PICK(darkedge)
STD_ROM_FN(darkedge)

static void darkedge_fd1149_vblank()
{
	v60WriteWord(0x20f072, 0);
	v60WriteWord(0x20f082, 0);

	if (v60ReadByte(0x20a12c) != 0)
	{
		v60WriteByte(0x20a12c, v60ReadByte(0x20a12c) - 1);

		if(v60ReadByte(0x20a12c) == 0)
			v60WriteByte(0x20a12e, 1);
	}
}

static INT32 DarkedgeInit()
{
	sprite_length = 0;
	DrvLoadRoms(false);
	BurnAllocMemIndex();
	if (DrvLoadRoms(true)) return 1;

	system32_v60_map();
	system32_sound_init();
	tilemap_configure_allocate();

	custom_io_read_0 = extra_custom_io_read; // CORRECT
	system32_prot_vblank = darkedge_fd1149_vblank;

	clr_opposites = 2;

	opaquey_hack = 1;

	DrvDoReset();

	return 0;
}

struct BurnDriver BurnDrvDarkedge = {
	"darkedge", NULL, NULL, NULL, "1992",
	"Dark Edge (World)\0", "Background GFX Issues", "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SYSTEM32, GBF_VSFIGHT, 0,
	NULL, darkedgeRomInfo, darkedgeRomName, NULL, NULL, NULL, NULL, DarkedgeInputInfo, DarkedgeDIPInfo,
	DarkedgeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};


// Dark Edge (Japan)

static struct BurnRomInfo darkedgejRomDesc[] = {
	{ "epr-15244.ic8",				0x080000, 0x0db138cb, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code

	{ "epr-15243.ic36",				0x020000, 0x08ca5f11, 3 | BRF_PRG | BRF_ESS }, //  1 Z80 #0 Code
	{ "mpr-15242.ic35",				0x100000, 0xffb7d917, 3 | BRF_PRG | BRF_ESS }, //  2
	{ "mpr-15241.ic34",				0x100000, 0x8eccc4fe, 3 | BRF_PRG | BRF_ESS }, //  3
	{ "mpr-15240.ic24",				0x100000, 0x867d59e8, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "mpr-15248.ic14",				0x080000, 0x185b308b, 1 | BRF_GRA },           //  5 Main Layer Tiles
	{ "mpr-15247.ic5",				0x080000, 0xbe21548c, 1 | BRF_GRA },           //  6

	{ "mpr-15249.ic32",				0x200000, 0x2b4371a8, 2 | BRF_GRA },           //  7 Main Sprites
	{ "mpr-15251.ic30",				0x200000, 0xefe2d689, 2 | BRF_GRA },           //  8
	{ "mpr-15253.ic28",				0x200000, 0x8356ed01, 2 | BRF_GRA },           //  9
	{ "mpr-15255.ic26",				0x200000, 0xff04a5b0, 2 | BRF_GRA },           // 10
	{ "mpr-15250.ic31",				0x200000, 0xc5cab71a, 2 | BRF_GRA },           // 11
	{ "mpr-15252.ic29",				0x200000, 0xf8885fea, 2 | BRF_GRA },           // 12
	{ "mpr-15254.ic27",				0x200000, 0x7765424b, 2 | BRF_GRA },           // 13
	{ "mpr-15256.ic25",				0x200000, 0x44c36b62, 2 | BRF_GRA },           // 14
};

STD_ROM_PICK(darkedgej)
STD_ROM_FN(darkedgej)

struct BurnDriver BurnDrvDarkedgej = {
	"darkedgej", "darkedge", NULL, NULL, "1992",
	"Dark Edge (Japan)\0", NULL, "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SYSTEM32, GBF_VSFIGHT, 0,
	NULL, darkedgejRomInfo, darkedgejRomName, NULL, NULL, NULL, NULL, DarkedgeInputInfo, DarkedgeDIPInfo,
	DarkedgeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};


// Spider-Man: The Videogame (World)

static struct BurnRomInfo spidmanRomDesc[] = {
	{ "epr-14307.ic13",				0x020000, 0xd900219c, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "epr-14306.ic7",				0x020000, 0x64379dc6, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-14281.ic14",				0x020000, 0x8a746c42, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-14280.ic6",				0x020000, 0x3c8148f7, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-14285.ic35",				0x040000, 0x25aefad6, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #0 Code
	{ "mpr-14284.ic31",				0x080000, 0x760542d4, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "mpr-14283.ic26",				0x080000, 0xc863a91c, 3 | BRF_PRG | BRF_ESS }, //  6
	{ "mpr-14282.ic22",				0x080000, 0xea20979e, 3 | BRF_PRG | BRF_ESS }, //  7

	{ "mpr-14291-s.ic38",			0x100000, 0x490f95a1, 3 | BRF_GRA },           //  8 Main Layer Tiles
	{ "mpr-14290-s.ic34",			0x100000, 0xa144162d, 3 | BRF_GRA },           //  9
	{ "mpr-14289-s.ic29",			0x100000, 0x38570582, 3 | BRF_GRA },           // 10
	{ "mpr-14288-s.ic25",			0x100000, 0x3188b636, 3 | BRF_GRA },           // 11

	{ "mpr-14299-h.ic36",			0x100000, 0xce59231b, 4 | BRF_GRA },           // 12 Main Sprites
	{ "mpr-14298-h.ic32",			0x100000, 0x2745c84c, 4 | BRF_GRA },           // 13
	{ "mpr-14297-h.ic27",			0x100000, 0x29cb9450, 4 | BRF_GRA },           // 14
	{ "mpr-14296-h.ic23",			0x100000, 0x9d8cbd31, 4 | BRF_GRA },           // 15
	{ "mpr-14295-h.ic37",			0x100000, 0x29591f50, 4 | BRF_GRA },           // 16
	{ "mpr-14294-h.ic33",			0x100000, 0xfa86b794, 4 | BRF_GRA },           // 17
	{ "mpr-14293-s.ic28",			0x100000, 0x52899269, 4 | BRF_GRA },           // 18
	{ "mpr-14292-s.ic24",			0x100000, 0x41f71443, 4 | BRF_GRA },           // 19
};

STD_ROM_PICK(spidman)
STD_ROM_FN(spidman)

static INT32 SpidmanInit()
{
	is_ga2_spidman = 1;
	sprite_length = 0;
	DrvLoadRoms(false);
	BurnAllocMemIndex();
	if (DrvLoadRoms(true)) return 1;

	system32_v60_map();
	system32_sound_init();
	tilemap_configure_allocate();

	custom_io_read_0 = extra_custom_io_read; // CORRECT

	clr_opposites = 4;

	DrvDoReset();

	return 0;
}

struct BurnDriver BurnDrvSpidman = {
	"spidman", NULL, NULL, NULL, "1991",
	"Spider-Man: The Videogame (World)\0", NULL, "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 4, HARDWARE_SEGA_SYSTEM32, GBF_SCRFIGHT | GBF_PLATFORM, 0,
	NULL, spidmanRomInfo, spidmanRomName, NULL, NULL, NULL, NULL, SpidmanInputInfo, SpidmanDIPInfo,
	SpidmanInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};


// Spider-Man: The Videogame (US, Rev A)

static struct BurnRomInfo spidmanuRomDesc[] = {
	{ "epr-14303a.ic13",			0x020000, 0x7f1bd28f, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "epr-14302a.ic7",				0x020000, 0xd954c40a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-14281.ic14",				0x020000, 0x8a746c42, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-14280.ic6",				0x020000, 0x3c8148f7, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-14285.ic35",				0x040000, 0x25aefad6, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #0 Code
	{ "mpr-14284.ic31",				0x080000, 0x760542d4, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "mpr-14283.ic26",				0x080000, 0xc863a91c, 3 | BRF_PRG | BRF_ESS }, //  6
	{ "mpr-14282.ic22",				0x080000, 0xea20979e, 3 | BRF_PRG | BRF_ESS }, //  7

	{ "mpr-14291-s.ic38",			0x100000, 0x490f95a1, 3 | BRF_GRA },           //  8 Main Layer Tiles
	{ "mpr-14290-s.ic34",			0x100000, 0xa144162d, 3 | BRF_GRA },           //  9
	{ "mpr-14289-s.ic29",			0x100000, 0x38570582, 3 | BRF_GRA },           // 10
	{ "mpr-14288-s.ic25",			0x100000, 0x3188b636, 3 | BRF_GRA },           // 11

	{ "mpr-14299-h.ic36",			0x100000, 0xce59231b, 4 | BRF_GRA },           // 12 Main Sprites
	{ "mpr-14298-h.ic32",			0x100000, 0x2745c84c, 4 | BRF_GRA },           // 13
	{ "mpr-14297-h.ic27",			0x100000, 0x29cb9450, 4 | BRF_GRA },           // 14
	{ "mpr-14296-h.ic23",			0x100000, 0x9d8cbd31, 4 | BRF_GRA },           // 15
	{ "mpr-14295-h.ic37",			0x100000, 0x29591f50, 4 | BRF_GRA },           // 16
	{ "mpr-14294-h.ic33",			0x100000, 0xfa86b794, 4 | BRF_GRA },           // 17
	{ "mpr-14293-s.ic28",			0x100000, 0x52899269, 4 | BRF_GRA },           // 18
	{ "mpr-14292-s.ic24",			0x100000, 0x41f71443, 4 | BRF_GRA },           // 19
};

STD_ROM_PICK(spidmanu)
STD_ROM_FN(spidmanu)

struct BurnDriver BurnDrvSpidmanu = {
	"spidmanu", "spidman", NULL, NULL, "1991",
	"Spider-Man: The Videogame (US, Rev A)\0", NULL, "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_SEGA_SYSTEM32, GBF_SCRFIGHT | GBF_PLATFORM, 0,
	NULL, spidmanuRomInfo, spidmanuRomName, NULL, NULL, NULL, NULL, SpidmanuInputInfo, SpidmanuDIPInfo,
	SpidmanInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};


// Spider-Man: The Videogame (Japan)

static struct BurnRomInfo spidmanjRomDesc[] = {
	{ "epr-14287.ic13",				0x020000, 0x403ccdc9, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "epr-14286.ic7",				0x020000, 0x5c2b4e2c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-14281.ic14",				0x020000, 0x8a746c42, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-14280.ic6",				0x020000, 0x3c8148f7, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-14285.ic35",				0x040000, 0x25aefad6, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #0 Code
	{ "mpr-14284.ic31",				0x080000, 0x760542d4, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "mpr-14283.ic26",				0x080000, 0xc863a91c, 3 | BRF_PRG | BRF_ESS }, //  6
	{ "mpr-14282.ic22",				0x080000, 0xea20979e, 3 | BRF_PRG | BRF_ESS }, //  7

	{ "mpr-14291-s.ic38",			0x100000, 0x490f95a1, 3 | BRF_GRA },           //  8 Main Layer Tiles
	{ "mpr-14290-s.ic34",			0x100000, 0xa144162d, 3 | BRF_GRA },           //  9
	{ "mpr-14289-s.ic29",			0x100000, 0x38570582, 3 | BRF_GRA },           // 10
	{ "mpr-14288-s.ic25",			0x100000, 0x3188b636, 3 | BRF_GRA },           // 11

	{ "mpr-14299-h.ic36",			0x100000, 0xce59231b, 4 | BRF_GRA },           // 12 Main Sprites
	{ "mpr-14298-h.ic32",			0x100000, 0x2745c84c, 4 | BRF_GRA },           // 13
	{ "mpr-14297-h.ic27",			0x100000, 0x29cb9450, 4 | BRF_GRA },           // 14
	{ "mpr-14296-h.ic23",			0x100000, 0x9d8cbd31, 4 | BRF_GRA },           // 15
	{ "mpr-14295-h.ic37",			0x100000, 0x29591f50, 4 | BRF_GRA },           // 16
	{ "mpr-14294-h.ic33",			0x100000, 0xfa86b794, 4 | BRF_GRA },           // 17
	{ "mpr-14293-s.ic28",			0x100000, 0x52899269, 4 | BRF_GRA },           // 18
	{ "mpr-14292-s.ic24",			0x100000, 0x41f71443, 4 | BRF_GRA },           // 19
};

STD_ROM_PICK(spidmanj)
STD_ROM_FN(spidmanj)

struct BurnDriver BurnDrvSpidmanj = {
	"spidmanj", "spidman", NULL, NULL, "1991",
	"Spider-Man: The Videogame (Japan)\0", NULL, "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_SEGA_SYSTEM32, GBF_SCRFIGHT | GBF_PLATFORM, 0,
	NULL, spidmanjRomInfo, spidmanjRomName, NULL, NULL, NULL, NULL, SpidmanInputInfo, SpidmanDIPInfo,
	SpidmanInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};


// Dragon Ball Z V.R.V.S. (Japan, Rev A)

static struct BurnRomInfo dbzvrvsRomDesc[] = {
	{ "epr-16543",					0x080000, 0x7b9bc6f5, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "epr-16542a",					0x080000, 0x6449ab22, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "epr-16541",					0x040000, 0x1d61d836, 3 | BRF_PRG | BRF_ESS }, //  2 Z80 #0 Code
	{ "mpr-16540",					0x100000, 0xb6f9bb43, 3 | BRF_PRG | BRF_ESS }, //  3
	{ "mpr-16539",					0x100000, 0x38c26418, 3 | BRF_PRG | BRF_ESS }, //  4
	{ "mpr-16538",					0x100000, 0x4d402c31, 3 | BRF_PRG | BRF_ESS }, //  5

	{ "mpr-16545",					0x100000, 0x51748bac, 1 | BRF_GRA },           //  6 Main Layer Tiles
	{ "mpr-16544",					0x100000, 0xf6c93dfc, 1 | BRF_GRA },           //  7

	{ "mpr-16546",					0x200000, 0x96f4be31, 2 | BRF_GRA },           //  8 Main Sprites
	{ "mpr-16548",					0x200000, 0x00377f59, 2 | BRF_GRA },           //  9
	{ "mpr-16550",					0x200000, 0x168e8966, 2 | BRF_GRA },           // 10
	{ "mpr-16552",					0x200000, 0xa31dae31, 2 | BRF_GRA },           // 11
	{ "mpr-16547",					0x200000, 0x50d328ed, 2 | BRF_GRA },           // 12
	{ "mpr-16549",					0x200000, 0xa5802e9f, 2 | BRF_GRA },           // 13
	{ "mpr-16551",					0x200000, 0xdede05fc, 2 | BRF_GRA },           // 14
	{ "mpr-16553",					0x200000, 0xc0a43009, 2 | BRF_GRA },           // 15
};

STD_ROM_PICK(dbzvrvs)
STD_ROM_FN(dbzvrvs)

static void dbzvrvs_prot_write(UINT32, UINT32, UINT32)
{
	v60WriteWord(0x2080c8, v60ReadWord(0x200044));
}

static INT32 DbzvrvsInit()
{
	sprite_length = 0;
	DrvLoadRoms(false);
	BurnAllocMemIndex();
	if (DrvLoadRoms(true)) return 1;

	system32_v60_map();
	system32_sound_init();
	tilemap_configure_allocate();

	protection_a00000_write = dbzvrvs_prot_write;

	clr_opposites = 2;

	DrvDoReset();

	return 0;
}

struct BurnDriver BurnDrvDbzvrvs = {
	"dbzvrvs", NULL, NULL, NULL, "1994",
	"Dragon Ball Z: V.R. V.S. (Japan, Rev A)\0", NULL, "Sega / Banpresto", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SYSTEM32, GBF_VSFIGHT, 0,
	NULL, dbzvrvsRomInfo, dbzvrvsRomName, NULL, NULL, NULL, NULL, DbzvrvsInputInfo, DbzvrvsDIPInfo,
	DbzvrvsInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};


// Holosseum (US, Rev A)

static struct BurnRomInfo holoRomDesc[] = {
	{ "epr-14977a",					0x020000, 0xe0d7e288, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "epr-14976a",					0x020000, 0xe56f13be, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-15011",					0x020000, 0xb9f59f59, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-15010",					0x020000, 0x0c09c57b, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-14965",					0x020000, 0x3a918cfe, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #0 Code
	{ "mpr-14964",					0x100000, 0x7ff581d5, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "mpr-14963",					0x100000, 0x0974a60e, 3 | BRF_PRG | BRF_ESS }, //  6
	{ "mpr-14962",					0x100000, 0x6b2e694e, 3 | BRF_PRG | BRF_ESS }, //  7

	{ "mpr-14973",					0x100000, 0xb3c3ff6b, 4 | BRF_GRA },           //  8 Main Sprites
	{ "mpr-14972",					0x100000, 0x0c161374, 4 | BRF_GRA },           //  9
	{ "mpr-14971",					0x100000, 0xdfcf6fdf, 4 | BRF_GRA },           // 10
	{ "mpr-14970",					0x100000, 0xcae3a745, 4 | BRF_GRA },           // 11
	{ "mpr-14969",					0x100000, 0xc06b7c15, 4 | BRF_GRA },           // 12
	{ "mpr-14968",					0x100000, 0xf413894a, 4 | BRF_GRA },           // 13
	{ "mpr-14967",					0x100000, 0x5377fce0, 4 | BRF_GRA },           // 14
	{ "mpr-14966",					0x100000, 0xdffba2e9, 4 | BRF_GRA },           // 15
};

STD_ROM_PICK(holo)
STD_ROM_FN(holo)

static INT32 HoloInit()
{
	sprite_length = 0;
	DrvLoadRoms(false);
	BurnAllocMemIndex();
	if (DrvLoadRoms(true)) return 1;

	system32_v60_map();
	system32_sound_init();
	tilemap_configure_allocate();

	screen_vflip = 1;

	clr_opposites = 2;

	DrvDoReset();

	return 0;
}

struct BurnDriver BurnDrvHolo = {
	"holo", NULL, NULL, NULL, "1992",
	"Holosseum (US, Rev A)\0", NULL, "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SYSTEM32, GBF_VSFIGHT, 0,
	NULL, holoRomInfo, holoRomName, NULL, NULL, NULL, NULL, HoloInputInfo, HoloDIPInfo,
	HoloInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};


// SegaSonic The Hedgehog (Japan, rev. C)

static struct BurnRomInfo sonicRomDesc[] = {
	{ "epr-15787c.ic17",			0x020000, 0x25e3c27e, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "epr-15786c.ic8",				0x020000, 0xefe9524c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-15781c.ic18",			0x040000, 0x65b06c25, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-15780c.ic9",				0x040000, 0x2db66fd2, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-15785.ic36",				0x040000, 0x0fe7422e, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #0 Code
	{ "mpr-15784.ic35",				0x100000, 0x42f06714, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "mpr-15783.ic34",				0x100000, 0xe4220eea, 3 | BRF_PRG | BRF_ESS }, //  6
	{ "mpr-15782.ic33",				0x100000, 0xcf56b5a0, 3 | BRF_PRG | BRF_ESS }, //  7

	{ "mpr-15789.ic14",				0x100000, 0x4378f12b, 1 | BRF_GRA },           //  8 Main Layer Tiles
	{ "mpr-15788.ic5",				0x100000, 0xa6ed5d7a, 1 | BRF_GRA },           //  9

	{ "mpr-15790.ic32",				0x200000, 0xc69d51b1, 2 | BRF_GRA },           // 10 Main Sprites
	{ "mpr-15792.ic30",				0x200000, 0x1006bb67, 2 | BRF_GRA },           // 11
	{ "mpr-15794.ic28",				0x200000, 0x8672b480, 2 | BRF_GRA },           // 12
	{ "mpr-15796.ic26",				0x200000, 0x95b8011c, 2 | BRF_GRA },           // 13
	{ "mpr-15791.ic31",				0x100000, 0x42217066, 2 | BRF_GRA },           // 14
	{ "mpr-15793.ic29",				0x100000, 0x75bafe55, 2 | BRF_GRA },           // 15
	{ "mpr-15795.ic27",				0x100000, 0x7f3dad30, 2 | BRF_GRA },           // 16
	{ "mpr-15797.ic25",				0x100000, 0x013c6cab, 2 | BRF_GRA },           // 17
};

STD_ROM_PICK(sonic)
STD_ROM_FN(sonic)

static void sonic_tbupdate()
{
	BurnTrackballUpdate(0);
	BurnTrackballUpdate(1);
	BurnTrackballUpdate(2);
}

static UINT16 sonic_custom_io_read(UINT32 offset)
{
	UINT8 tb[6] = { BurnTrackballRead(0, 0), BurnTrackballRead(0, 1), BurnTrackballRead(1, 0), BurnTrackballRead(1, 1), BurnTrackballRead(2, 0), BurnTrackballRead(2, 1) };

	if (offset == 0) {
		sonic_tbupdate();
		sonic_tbupdate();
	}

	switch (offset)
	{
		case 0x00/2:
		case 0x04/2:
		case 0x08/2:
		case 0x0c/2:
		case 0x10/2:
		case 0x14/2:
			return tb[offset/2] - sonic_delta[offset/2];
	}

	return 0xffff;
}

static void sonic_custom_io_write(UINT32 offset, UINT16, UINT16)
{
	UINT8 tb[6] = { BurnTrackballRead(0, 0), BurnTrackballRead(0, 1), BurnTrackballRead(1, 0), BurnTrackballRead(1, 1), BurnTrackballRead(2, 0), BurnTrackballRead(2, 1) };
	if (offset == 0) {
		sonic_tbupdate();
		sonic_tbupdate();
	}

	switch (offset)
	{
		case 0x00/2:
		case 0x08/2:
		case 0x10/2:
			sonic_delta[offset/2 + 0] = tb[offset/2 + 0];
			sonic_delta[offset/2 + 1] = tb[offset/2 + 1];
			return;
	}
}

static void sonic_prot_write(UINT32 offset, UINT32 data, UINT32 mask)
{
	UINT16 *ram = (UINT16*)DrvV60RAM;
	ram[offset] = BURN_ENDIAN_SWAP_INT16((BURN_ENDIAN_SWAP_INT16(ram[offset]) & ~mask) | (data & mask));

	if (offset == (0xe5c4/2))
	{
		UINT16 level = 0;
		if (ram[0xE5C4 / 2] == 0)
		{
			level = 7;
		}
		else
		{
			level =  *((DrvV60ROM + 0x263a) + (BURN_ENDIAN_SWAP_INT16(ram[0xE5C4 / 2]) * 2) - 1);
			level |= *((DrvV60ROM + 0x263a) + (BURN_ENDIAN_SWAP_INT16(ram[0xE5C4 / 2]) * 2) - 2) << 8;
		}
		ram[0xF06E/2] = BURN_ENDIAN_SWAP_INT16(level);
		ram[0xf0bc/2] = 0;
		ram[0xf0be/2] = 0;
	}
}

static INT32 SonicInit()
{
	is_sonic = 1;

	sprite_length = 0x1000000;
	DrvLoadRoms(false);
	BurnAllocMemIndex();
	if (DrvLoadRoms(true)) return 1;

	system32_v60_map();
	system32_sound_init();
	tilemap_configure_allocate();

	v60Open(0);
	v60MapMemory(NULL,	0x20e000, 0x20e7ff, MAP_WRITE);	// unmap writes to this range to allow protection writes (page is 800)
	v60Close();

	memory_protection_write = sonic_prot_write;

	custom_io_read_0 = sonic_custom_io_read;
	custom_io_write_0 = sonic_custom_io_write;

	BurnTrackballInit(3);

	DrvDoReset();

	return 0;
}

struct BurnDriver BurnDrvSonic = {
	"sonic", NULL, NULL, NULL, "1992",
	"SegaSonic The Hedgehog (Japan, rev. C)\0", NULL, "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 3, HARDWARE_SEGA_SYSTEM32, GBF_PLATFORM, 0,
	NULL, sonicRomInfo, sonicRomName, NULL, NULL, NULL, NULL, SonicInputInfo, SonicDIPInfo,
	SonicInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};


// SegaSonic The Hedgehog (Japan, prototype)

static struct BurnRomInfo sonicpRomDesc[] = {
	{ "sonpg0.bin",					0x020000, 0xda05dcbb, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "sonpg1.bin",					0x020000, 0xc57dc5c5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sonpd0.bin",					0x040000, 0xa7da7546, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "sonpd1.bin",					0x040000, 0xc30e4c70, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "sonsnd0.bin",				0x040000, 0x569c8d4b, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #0 Code
	{ "sonsnd1.bin",				0x100000, 0xf4fa5a21, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "sonsnd2.bin",				0x100000, 0xe1bd45a5, 3 | BRF_PRG | BRF_ESS }, //  6
	{ "sonsnd3.bin",				0x100000, 0xcf56b5a0, 3 | BRF_PRG | BRF_ESS }, //  7

	{ "sonscl0.bin",				0x080000, 0x445e31b9, 3 | BRF_GRA },           //  8 Main Layer Tiles
	{ "sonscl1.bin",				0x080000, 0x3d234181, 3 | BRF_GRA },           //  9
	{ "sonscl2.bin",				0x080000, 0xa5de28b2, 3 | BRF_GRA },           // 10
	{ "sonscl3.bin",				0x080000, 0x7ce7554b, 3 | BRF_GRA },           // 11

	{ "sonobj0.bin",				0x100000, 0xceea18e3, 4 | BRF_GRA },           // 12 Main Sprites
	{ "sonobj1.bin",				0x100000, 0x6bbc226b, 4 | BRF_GRA },           // 13
	{ "sonobj2.bin",				0x100000, 0xfcd5ef0e, 4 | BRF_GRA },           // 14
	{ "sonobj3.bin",				0x100000, 0xb99b42ab, 4 | BRF_GRA },           // 15
	{ "sonobj4.bin",				0x100000, 0xc7ec1456, 4 | BRF_GRA },           // 16
	{ "sonobj5.bin",				0x100000, 0xbd5da27f, 4 | BRF_GRA },           // 17
	{ "sonobj6.bin",				0x100000, 0x313c92d1, 4 | BRF_GRA },           // 18
	{ "sonobj7.bin",				0x100000, 0x3784c507, 4 | BRF_GRA },           // 19
};

STD_ROM_PICK(sonicp)
STD_ROM_FN(sonicp)

static INT32 SonicpInit()
{
	is_sonic = 1;

	sprite_length = 0;
	DrvLoadRoms(false);
	BurnAllocMemIndex();
	if (DrvLoadRoms(true)) return 1;

	system32_v60_map();
	system32_sound_init();
	tilemap_configure_allocate();

	custom_io_read_0 = sonic_custom_io_read;
	custom_io_write_0 = sonic_custom_io_write;

	BurnTrackballInit(3);

	DrvDoReset();

	return 0;
}

struct BurnDriver BurnDrvSonicp = {
	"sonicp", "sonic", NULL, NULL, "1992",
	"SegaSonic The Hedgehog (Japan, prototype)\0", NULL, "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 3, HARDWARE_SEGA_SYSTEM32, GBF_PLATFORM, 0,
	NULL, sonicpRomInfo, sonicpRomName, NULL, NULL, NULL, NULL, SonicInputInfo, SonicDIPInfo,
	SonicpInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};


// Alien3: The Gun (World)

static struct BurnRomInfo alien3RomDesc[] = {
	{ "epr-15943.ic17",				0x040000, 0xac4591aa, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "epr-15942.ic8",				0x040000, 0xa1e1d0ec, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mpr-15855.ic18",				0x080000, 0xa6fadabe, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "mpr-15854.ic9",				0x080000, 0xd1aec392, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-15859a.ic36",			0x040000, 0x91b55bd0, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #0 Code
	{ "mpr-15858.ic35",				0x100000, 0x2eb64c10, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "mpr-15857.ic34",				0x100000, 0x915c56df, 3 | BRF_PRG | BRF_ESS }, //  6
	{ "mpr-15856.ic24",				0x100000, 0xa5ef4f1f, 3 | BRF_PRG | BRF_ESS }, //  7

	{ "mpr-15863.ic14",				0x200000, 0x9d36b645, 1 | BRF_GRA },           //  8 Main Layer Tiles
	{ "mpr-15862.ic5",				0x200000, 0x9e277d25, 1 | BRF_GRA },           //  9

	{ "mpr-15864.ic32",				0x200000, 0x58207157, 2 | BRF_GRA },           // 10 Main Sprites
	{ "mpr-15866.ic30",				0x200000, 0x9c53732c, 2 | BRF_GRA },           // 11
	{ "mpr-15868.ic28",				0x200000, 0x62d556e8, 2 | BRF_GRA },           // 12
	{ "mpr-15870.ic26",				0x200000, 0xd31c0400, 2 | BRF_GRA },           // 13
	{ "mpr-15865.ic31",				0x200000, 0xdd64f87b, 2 | BRF_GRA },           // 14
	{ "mpr-15867.ic29",				0x200000, 0x8cf9cb11, 2 | BRF_GRA },           // 15
	{ "mpr-15869.ic27",				0x200000, 0xdd4b137f, 2 | BRF_GRA },           // 16
	{ "mpr-15871.ic25",				0x200000, 0x58eb10ae, 2 | BRF_GRA },           // 17

	{ "93c45_eeprom.ic76",			0x000080, 0x6e1d9df3, 7 | BRF_PRG | BRF_ESS }, // 18 Default EEPROM
};

STD_ROM_PICK(alien3)
STD_ROM_FN(alien3)

static INT32 Alien3Init()
{
	sprite_length = 0;
	DrvLoadRoms(false);
	BurnAllocMemIndex();
	if (DrvLoadRoms(true)) return 1;

	has_gun = 1;

	system32_v60_map();
	system32_sound_init();
	tilemap_configure_allocate();

	custom_io_write_0  = alien3_custom_io_write;
	custom_io_read_0 = analog_custom_io_read;

	DrvDoReset();

	return 0;
}

struct BurnDriver BurnDrvAlien3 = {
	"alien3", NULL, NULL, NULL, "1993",
	"Alien3: The Gun (World)\0", NULL, "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM32, GBF_SHOOT, 0,
	NULL, alien3RomInfo, alien3RomName, NULL, NULL, NULL, NULL, Alien3InputInfo, Alien3DIPInfo,
	Alien3Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};


// Alien3: The Gun (US, Rev A)

static struct BurnRomInfo alien3uRomDesc[] = {
	{ "epr-15941.ic17",				0x040000, 0xbf8c257f, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "epr-15940a.ic8",				0x040000, 0x8840b51e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mpr-15855.ic18",				0x080000, 0xa6fadabe, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "mpr-15854.ic9",				0x080000, 0xd1aec392, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-15859a.ic36",			0x040000, 0x91b55bd0, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #0 Code
	{ "mpr-15858.ic35",				0x100000, 0x2eb64c10, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "mpr-15857.ic34",				0x100000, 0x915c56df, 3 | BRF_PRG | BRF_ESS }, //  6
	{ "mpr-15856.ic24",				0x100000, 0xa5ef4f1f, 3 | BRF_PRG | BRF_ESS }, //  7

	{ "mpr-15863.ic14",				0x200000, 0x9d36b645, 1 | BRF_GRA },           //  8 Main Layer Tiles
	{ "mpr-15862.ic5",				0x200000, 0x9e277d25, 1 | BRF_GRA },           //  9

	{ "mpr-15864.ic32",				0x200000, 0x58207157, 2 | BRF_GRA },           // 10 Main Sprites
	{ "mpr-15866.ic30",				0x200000, 0x9c53732c, 2 | BRF_GRA },           // 11
	{ "mpr-15868.ic28",				0x200000, 0x62d556e8, 2 | BRF_GRA },           // 12
	{ "mpr-15870.ic26",				0x200000, 0xd31c0400, 2 | BRF_GRA },           // 13
	{ "mpr-15865.ic31",				0x200000, 0xdd64f87b, 2 | BRF_GRA },           // 14
	{ "mpr-15867.ic29",				0x200000, 0x8cf9cb11, 2 | BRF_GRA },           // 15
	{ "mpr-15869.ic27",				0x200000, 0xdd4b137f, 2 | BRF_GRA },           // 16
	{ "mpr-15871.ic25",				0x200000, 0x58eb10ae, 2 | BRF_GRA },           // 17

	{ "93c45_eeprom.ic76",			0x000080, 0x6e1d9df3, 7 | BRF_PRG | BRF_ESS }, // 18 Default EEPROM
};

STD_ROM_PICK(alien3u)
STD_ROM_FN(alien3u)

struct BurnDriver BurnDrvAlien3u = {
	"alien3u", "alien3", NULL, NULL, "1993",
	"Alien3: The Gun (US, Rev A)\0", NULL, "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM32, GBF_SHOOT, 0,
	NULL, alien3uRomInfo, alien3uRomName, NULL, NULL, NULL, NULL, Alien3InputInfo, Alien3DIPInfo,
	Alien3Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};


// Alien3: The Gun (Japan)

static struct BurnRomInfo alien3jRomDesc[] = {
	{ "epr-15861.ic17",				0x040000, 0xe970603f, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "epr-15860.ic8",				0x040000, 0x9af0d996, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mpr-15855.ic18",				0x080000, 0xa6fadabe, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "mpr-15854.ic9",				0x080000, 0xd1aec392, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-15859.ic36",				0x040000, 0x7eeda800, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #0 Code
	{ "mpr-15858.ic35",				0x100000, 0x2eb64c10, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "mpr-15857.ic34",				0x100000, 0x915c56df, 3 | BRF_PRG | BRF_ESS }, //  6
	{ "mpr-15856.ic24",				0x100000, 0xa5ef4f1f, 3 | BRF_PRG | BRF_ESS }, //  7

	{ "mpr-15863.ic14",				0x200000, 0x9d36b645, 1 | BRF_GRA },           //  8 Main Layer Tiles
	{ "mpr-15862.ic5",				0x200000, 0x9e277d25, 1 | BRF_GRA },           //  9

	{ "mpr-15864.ic32",				0x200000, 0x58207157, 2 | BRF_GRA },           // 10 Main Sprites
	{ "mpr-15866.ic30",				0x200000, 0x9c53732c, 2 | BRF_GRA },           // 11
	{ "mpr-15868.ic28",				0x200000, 0x62d556e8, 2 | BRF_GRA },           // 12
	{ "mpr-15870.ic26",				0x200000, 0xd31c0400, 2 | BRF_GRA },           // 13
	{ "mpr-15865.ic31",				0x200000, 0xdd64f87b, 2 | BRF_GRA },           // 14
	{ "mpr-15867.ic29",				0x200000, 0x8cf9cb11, 2 | BRF_GRA },           // 15
	{ "mpr-15869.ic27",				0x200000, 0xdd4b137f, 2 | BRF_GRA },           // 16
	{ "mpr-15871.ic25",				0x200000, 0x58eb10ae, 2 | BRF_GRA },           // 17

	{ "93c45_eeprom.ic76",			0x000080, 0x6e1d9df3, 7 | BRF_PRG | BRF_ESS }, // 18 Default EEPROM
};

STD_ROM_PICK(alien3j)
STD_ROM_FN(alien3j)

struct BurnDriver BurnDrvAlien3j = {
	"alien3j", "alien3", NULL, NULL, "1993",
	"Alien3: The Gun (Japan)\0", NULL, "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM32, GBF_SHOOT, 0,
	NULL, alien3jRomInfo, alien3jRomName, NULL, NULL, NULL, NULL, Alien3InputInfo, Alien3DIPInfo,
	Alien3Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};


// Burning Rival (World)

static struct BurnRomInfo brivalRomDesc[] = {
	{ "epr-15722.ic8",				0x020000, 0x138141c0, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "epr-15723.ic18",				0x080000, 0x4ff40d39, 2 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-15724.ic9",				0x080000, 0x3ff8a052, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "epr-15725.ic36",				0x020000, 0xea1407d7, 3 | BRF_PRG | BRF_ESS }, //  3 Z80 #0 Code
	{ "mpr-15627.ic35",				0x100000, 0x8a8388c5, 3 | BRF_PRG | BRF_ESS }, //  4
	{ "mpr-15626.ic34",				0x100000, 0x83306d1e, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "mpr-15625.ic24",				0x100000, 0x3ce82932, 3 | BRF_PRG | BRF_ESS }, //  6

	{ "mpr-15629.ic14",				0x200000, 0x2c8dd96d, 1 | BRF_GRA },           //  7 Main Layer Tiles
	{ "mpr-15628.ic5",				0x200000, 0x58d4ca40, 1 | BRF_GRA },           //  8

	{ "mpr-15637.ic32",				0x200000, 0xb6cf2f05, 2 | BRF_GRA },           //  9 Main Sprites
	{ "mpr-15635.ic30",				0x200000, 0x70f2eb2b, 2 | BRF_GRA },           // 10
	{ "mpr-15633.ic28",				0x200000, 0x005dfed5, 2 | BRF_GRA },           // 11
	{ "mpr-15631.ic26",				0x200000, 0xc35e2f21, 2 | BRF_GRA },           // 12
	{ "mpr-15636.ic31",				0x200000, 0xd81ca97b, 2 | BRF_GRA },           // 13
	{ "mpr-15634.ic29",				0x200000, 0xb0c6c52a, 2 | BRF_GRA },           // 14
	{ "mpr-15632.ic27",				0x200000, 0x8476e52b, 2 | BRF_GRA },           // 15
	{ "mpr-15630.ic25",				0x200000, 0xbf7dd2f6, 2 | BRF_GRA },           // 16
};

STD_ROM_PICK(brival)
STD_ROM_FN(brival)

static UINT32 brival_protection_read(UINT32 offset, UINT32 mem_mask)
{
	if (mem_mask == 0xffff) // only trap on word-wide reads
	{
		switch (offset)		// 0x20ba00 & ~0x7ff, 0x20ba07 | 0x7ff
		{
			case 0xba00/2:
			case 0xba04/2:
			case 0xba06/2:
				return 0;
		}
	}

	return BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvV60RAM + offset * 2)));
}

// really weird... mame has this function, it copies strings to 'protram', but never maps the protram anywhere to read it?
// should it be mapped to a00000 - a00fff ??

static void brival_protection_write(UINT32 offset, UINT32 data, UINT32 mask)
{
	static const INT32 protAddress[6][2] = {
		{ 0x109517, 0x00/2 },
		{ 0x109597, 0x10/2 },
		{ 0x109597, 0x20/2 },
		{ 0x109597, 0x30/2 },
		{ 0x109597, 0x40/2 },
		{ 0x109617, 0x50/2 },
	};
	char ret[32];
	INT32 curProtType;
	UINT8 *ROM = DrvV60ROM;
	UINT16 *protram = (UINT16*)DrvV25RAM;

	protram[offset] = (protram[offset] & ~mask) | (data & mask);

	switch (offset)
	{
		case 0x800/2:
			curProtType = 0;
			break;
		case 0x802/2:
			curProtType = 1;
			break;
		case 0x804/2:
			curProtType = 2;
			break;
		case 0x806/2:
			curProtType = 3;
			break;
		case 0x808/2:
			curProtType = 4;
			break;
		case 0x80a/2:
			curProtType = 5;
			break;
		default:
			if (offset >= 0xa00/2 && offset < 0xc00/2)
				return;
			return;
	}

	memcpy(ret, &ROM[protAddress[curProtType][0]], 16);
	ret[16] = '\0';

	memcpy(&protram[protAddress[curProtType][1]], ret, 16);
}

static INT32 BrivalInit()
{
	sprite_length = 0;
	DrvLoadRoms(false);
	BurnAllocMemIndex();
	if (DrvLoadRoms(true)) return 1;

	system32_v60_map();
	system32_sound_init();
	tilemap_configure_allocate();

	v60Open(0);
	v60MapMemory(NULL,	0x20ba00 & ~0x7ff, 0x20ba07 | 0x7ff, MAP_READ);	// unmap writes to this range to allow protection writes (page is 800)
	v60MapMemory(DrvV25RAM, 0xa00000, 0xa00fff, MAP_ROM); // ??
	v60Close();

	memory_protection_read = brival_protection_read;
	protection_a00000_write = brival_protection_write;

	custom_io_read_0 = extra_custom_io_read;

	clr_opposites = 2;

	DrvDoReset();

	return 0;
}

struct BurnDriver BurnDrvBrival = {
	"brival", NULL, NULL, NULL, "1992",
	"Burning Rival (World)\0", NULL, "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM32, GBF_VSFIGHT, 0,
	NULL, brivalRomInfo, brivalRomName, NULL, NULL, NULL, NULL, BrivalInputInfo, BrivalDIPInfo,
	BrivalInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	416, 224, 4, 3
};


// Burning Rival (Japan)

static struct BurnRomInfo brivaljRomDesc[] = {
	{ "epr-15720.ic8",				0x020000, 0x0d182d78, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "epr-15723.ic18",				0x080000, 0x4ff40d39, 2 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-15724.ic9",				0x080000, 0x3ff8a052, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "epr-15725.ic36",				0x020000, 0xea1407d7, 3 | BRF_PRG | BRF_ESS }, //  3 Z80 #0 Code
	{ "mpr-15627.ic35",				0x100000, 0x8a8388c5, 3 | BRF_PRG | BRF_ESS }, //  4
	{ "mpr-15626.ic34",				0x100000, 0x83306d1e, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "mpr-15625.ic24",				0x100000, 0x3ce82932, 3 | BRF_PRG | BRF_ESS }, //  6

	{ "mpr-15629.ic14",				0x200000, 0x2c8dd96d, 1 | BRF_GRA },           //  7 Main Layer Tiles
	{ "mpr-15628.ic5",				0x200000, 0x58d4ca40, 1 | BRF_GRA },           //  8

	{ "mpr-15637.ic32",				0x200000, 0xb6cf2f05, 2 | BRF_GRA },           //  9 Main Sprites
	{ "mpr-15635.ic30",				0x200000, 0x70f2eb2b, 2 | BRF_GRA },           // 10
	{ "mpr-15633.ic28",				0x200000, 0x005dfed5, 2 | BRF_GRA },           // 11
	{ "mpr-15631.ic26",				0x200000, 0xc35e2f21, 2 | BRF_GRA },           // 12
	{ "mpr-15636.ic31",				0x200000, 0xd81ca97b, 2 | BRF_GRA },           // 13
	{ "mpr-15634.ic29",				0x200000, 0xb0c6c52a, 2 | BRF_GRA },           // 14
	{ "mpr-15632.ic27",				0x200000, 0x8476e52b, 2 | BRF_GRA },           // 15
	{ "mpr-15630.ic25",				0x200000, 0xbf7dd2f6, 2 | BRF_GRA },           // 16
};

STD_ROM_PICK(brivalj)
STD_ROM_FN(brivalj)

struct BurnDriver BurnDrvBrivalj = {
	"brivalj", "brival", NULL, NULL, "1992",
	"Burning Rival (Japan)\0", NULL, "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM32, GBF_VSFIGHT, 0,
	NULL, brivaljRomInfo, brivaljRomName, NULL, NULL, NULL, NULL, BrivalInputInfo, BrivalDIPInfo,
	BrivalInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	416, 224, 4, 3
};


// F1 Super Lap (World)

static struct BurnRomInfo f1lapRomDesc[] = {
	{ "epr-15598.ic17",				0x020000, 0x9feab7cd, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "epr-15611.ic8",				0x020000, 0x0d8c97c2, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-15596.ic18",				0x040000, 0x20e92909, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-15597.ic9",				0x040000, 0xcd1ccddb, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-15592.ic36",				0x020000, 0x7c055cc8, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #0 Code
	{ "mpr-15593.ic35",				0x100000, 0xe7300441, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "mpr-15594.ic34",				0x100000, 0x7f4ca3bb, 3 | BRF_PRG | BRF_ESS }, //  6
	{ "mpr-15595.ic24",				0x100000, 0x3fbdad9a, 3 | BRF_PRG | BRF_ESS }, //  7

	{ "mpr-15608.ic14",				0x200000, 0x64462c69, 1 | BRF_GRA },           //  8 Main Layer Tiles
	{ "mpr-15609.ic5",				0x200000, 0xd586e455, 1 | BRF_GRA },           //  9

	{ "mpr-15600.ic32",				0x200000, 0xd2698d23, 2 | BRF_GRA },           // 10 Main Sprites
	{ "mpr-15602.ic30",				0x200000, 0x1674764d, 2 | BRF_GRA },           // 11
	{ "mpr-15604.ic28",				0x200000, 0x1552bbb9, 2 | BRF_GRA },           // 12
	{ "mpr-15606.ic26",				0x200000, 0x2b4f5265, 2 | BRF_GRA },           // 13
	{ "mpr-15601.ic31",				0x200000, 0x31a8f40a, 2 | BRF_GRA },           // 14
	{ "mpr-15603.ic29",				0x200000, 0x3805ecbc, 2 | BRF_GRA },           // 15
	{ "mpr-15605.ic27",				0x200000, 0xcbdbf35e, 2 | BRF_GRA },           // 16
	{ "mpr-15607.ic25",				0x200000, 0x6c8817c9, 2 | BRF_GRA },           // 17

	{ "15612",						0x020000, 0x9d204617, 0 | BRF_PRG | BRF_ESS },           // 18 Comms Board ROM
};

STD_ROM_PICK(f1lap)
STD_ROM_FN(f1lap)

static void f1lap_fd1149_vblank()
{
	v60WriteByte(0x20F7C6, 0);

	if (v60ReadByte(0x20EE81) == 0xff)
		v60WriteByte(0x20EE81, 0);
}

static INT32 F1lapInit()
{
	sprite_length = 0;
	DrvLoadRoms(false);
	BurnAllocMemIndex();
	if (DrvLoadRoms(true)) return 1;

	system32_v60_map();
	system32_sound_init();
	tilemap_configure_allocate();

	custom_io_write_0  = f1en_custom_io_write;
	custom_io_read_0 = analog_custom_io_read;
	system32_prot_vblank = f1lap_fd1149_vblank;

	DrvDoReset();

	return 0;
}

struct BurnDriver BurnDrvF1lap = {
	"f1lap", NULL, NULL, NULL, "1993",
	"F1 Super Lap (World)\0", NULL, "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_SYSTEM32, GBF_RACING, 0,
	NULL, f1lapRomInfo, f1lapRomName, NULL, NULL, NULL, NULL, F1lapInputInfo, F1lapDIPInfo,
	F1lapInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};


// F1 Super Lap (World, Unprotected)

static struct BurnRomInfo f1laptRomDesc[] = {
	{ "epr-15598.ic17",				0x020000, 0x9feab7cd, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "epr-15611t.ic8",				0x020000, 0x7a57b792, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-15596t.ic18",			0x040000, 0x20e92909, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-15597t.ic9",				0x040000, 0xcd1ccddb, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-15592.ic36",				0x020000, 0x7c055cc8, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #0 Code
	{ "mpr-15593.ic35",				0x100000, 0xe7300441, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "mpr-15594.ic34",				0x100000, 0x7f4ca3bb, 3 | BRF_PRG | BRF_ESS }, //  6
	{ "mpr-15595.ic24",				0x100000, 0x3fbdad9a, 3 | BRF_PRG | BRF_ESS }, //  7

	{ "mpr-15608.ic14",				0x200000, 0x64462c69, 1 | BRF_GRA },           //  8 Main Layer Tiles
	{ "mpr-15609.ic5",				0x200000, 0xd586e455, 1 | BRF_GRA },           //  9

	{ "mpr-15600.ic32",				0x200000, 0xd2698d23, 2 | BRF_GRA },           // 10 Main Sprites
	{ "mpr-15602.ic30",				0x200000, 0x1674764d, 2 | BRF_GRA },           // 11
	{ "mpr-15604.ic28",				0x200000, 0x1552bbb9, 2 | BRF_GRA },           // 12
	{ "mpr-15606.ic26",				0x200000, 0x2b4f5265, 2 | BRF_GRA },           // 13
	{ "mpr-15601.ic31",				0x200000, 0x31a8f40a, 2 | BRF_GRA },           // 14
	{ "mpr-15603.ic29",				0x200000, 0x3805ecbc, 2 | BRF_GRA },           // 15
	{ "mpr-15605.ic27",				0x200000, 0xcbdbf35e, 2 | BRF_GRA },           // 16
	{ "mpr-15607.ic25",				0x200000, 0x6c8817c9, 2 | BRF_GRA },           // 17

	{ "15612",						0x020000, 0x9d204617, 0 | BRF_PRG | BRF_ESS }, // 18 Comms Board ROM
};

STD_ROM_PICK(f1lapt)
STD_ROM_FN(f1lapt)

struct BurnDriver BurnDrvF1lapt = {
	"f1lapt", "f1lap", NULL, NULL, "1993",
	"F1 Super Lap (World, Unprotected)\0", NULL, "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SYSTEM32, GBF_RACING, 0,
	NULL, f1laptRomInfo, f1laptRomName, NULL, NULL, NULL, NULL, F1lapInputInfo, F1lapDIPInfo,
	F1lapInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};


// F1 Super Lap (Japan)

static struct BurnRomInfo f1lapjRomDesc[] = {
	{ "epr-15598.ic17",				0x020000, 0x9feab7cd, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "epr-15599.ic8",				0x020000, 0x5c5ac112, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-15596.ic18",				0x040000, 0x20e92909, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-15597.ic9",				0x040000, 0xcd1ccddb, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-15592.ic36",				0x020000, 0x7c055cc8, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #0 Code
	{ "mpr-15593.ic35",				0x100000, 0xe7300441, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "mpr-15594.ic34",				0x100000, 0x7f4ca3bb, 3 | BRF_PRG | BRF_ESS }, //  6
	{ "mpr-15595.ic24",				0x100000, 0x3fbdad9a, 3 | BRF_PRG | BRF_ESS }, //  7

	{ "mpr-15608.ic14",				0x200000, 0x64462c69, 1 | BRF_GRA },           //  8 Main Layer Tiles
	{ "mpr-15609.ic5",				0x200000, 0xd586e455, 1 | BRF_GRA },           //  9

	{ "mpr-15600.ic32",				0x200000, 0xd2698d23, 2 | BRF_GRA },           // 10 Main Sprites
	{ "mpr-15602.ic30",				0x200000, 0x1674764d, 2 | BRF_GRA },           // 11
	{ "mpr-15604.ic28",				0x200000, 0x1552bbb9, 2 | BRF_GRA },           // 12
	{ "mpr-15606.ic26",				0x200000, 0x2b4f5265, 2 | BRF_GRA },           // 13
	{ "mpr-15601.ic31",				0x200000, 0x31a8f40a, 2 | BRF_GRA },           // 14
	{ "mpr-15603.ic29",				0x200000, 0x3805ecbc, 2 | BRF_GRA },           // 15
	{ "mpr-15605.ic27",				0x200000, 0xcbdbf35e, 2 | BRF_GRA },           // 16
	{ "mpr-15607.ic25",				0x200000, 0x6c8817c9, 2 | BRF_GRA },           // 17

	{ "15612",						0x020000, 0x9d204617, 0 | BRF_PRG | BRF_ESS }, // 18 Comms Board ROM
};

STD_ROM_PICK(f1lapj)
STD_ROM_FN(f1lapj)

struct BurnDriver BurnDrvF1lapj = {
	"f1lapj", "f1lap", NULL, NULL, "1993",
	"F1 Super Lap (Japan)\0", NULL, "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SYSTEM32, GBF_RACING, 0,
	NULL, f1lapjRomInfo, f1lapjRomName, NULL, NULL, NULL, NULL, F1lapInputInfo, F1lapDIPInfo,
	F1lapInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};


// F1 Exhaust Note (World, Rev A)

static struct BurnRomInfo f1enRomDesc[] = {
	{ "epr-14452a.ic6",				0x020000, 0xb5b4a9d9, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "epr-14445.ic14",				0x040000, 0xd06261ab, 2 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-14444.ic7",				0x040000, 0x07724354, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "epr-14449.ic35",				0x020000, 0x2d29699c, 3 | BRF_PRG | BRF_ESS }, //  3 Z80 #0 Code
	{ "epr-14448.ic31",				0x080000, 0x87ca1e8d, 3 | BRF_PRG | BRF_ESS }, //  4
	{ "epr-14447.ic26",				0x080000, 0xdb1cfcbd, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "epr-14446.ic22",				0x080000, 0x646ec2cb, 3 | BRF_PRG | BRF_ESS }, //  6

	{ "mpr-14362.ic38",				0x040000, 0xfb1c4e79, 3 | BRF_GRA },           //  7 Main Layer Tiles
	{ "mpr-14361.ic34",				0x040000, 0xe3204bda, 3 | BRF_GRA },           //  8
	{ "mpr-14360.ic29",				0x040000, 0xc5e8da79, 3 | BRF_GRA },           //  9
	{ "mpr-14359.ic25",				0x040000, 0x70305c68, 3 | BRF_GRA },           // 10

	{ "mpr-14370.ic36",				0x080000, 0xfda78289, 4 | BRF_GRA },           // 11 Main Sprites
	{ "mpr-14369.ic32",				0x080000, 0x7765116d, 4 | BRF_GRA },           // 12
	{ "mpr-14368.ic27",				0x080000, 0x5744a30e, 4 | BRF_GRA },           // 13
	{ "mpr-14367.ic23",				0x080000, 0x77bb9003, 4 | BRF_GRA },           // 14
	{ "mpr-14366.ic37",				0x080000, 0x21078e83, 4 | BRF_GRA },           // 15
	{ "mpr-14365.ic33",				0x080000, 0x36913790, 4 | BRF_GRA },           // 16
	{ "mpr-14364.ic28",				0x080000, 0x0fa12ecd, 4 | BRF_GRA },           // 17
	{ "mpr-14363.ic24",				0x080000, 0xf3427a56, 4 | BRF_GRA },           // 18

	{ "epr-14452a.ic6",				0x020000, 0xb5b4a9d9, 0 | BRF_PRG | BRF_ESS }, // 19 V60 #1 Code (sub)
	{ "epr-14445.ic14",				0x040000, 0xd06261ab, 0 | BRF_PRG | BRF_ESS }, // 20
	{ "epr-14444.ic7",				0x040000, 0x07724354, 0 | BRF_PRG | BRF_ESS }, // 21

	{ "epr-14449.ic35",				0x020000, 0x2d29699c, 0 | BRF_PRG | BRF_ESS }, // 22 Z80 #1 Code (sub)
	{ "epr-14448.ic31",				0x080000, 0x87ca1e8d, 0 | BRF_PRG | BRF_ESS }, // 23
	{ "epr-14447.ic26",				0x080000, 0xdb1cfcbd, 0 | BRF_PRG | BRF_ESS }, // 24
	{ "epr-14446.ic22",				0x080000, 0x646ec2cb, 0 | BRF_PRG | BRF_ESS }, // 25

	{ "mpr-14362.ic38",				0x040000, 0xfb1c4e79, 0 | BRF_GRA },           // 26 Sub Layer Tiles
	{ "mpr-14361.ic34",				0x040000, 0xe3204bda, 0 | BRF_GRA },           // 27
	{ "mpr-14360.ic29",				0x040000, 0xc5e8da79, 0 | BRF_GRA },           // 28
	{ "mpr-14359.ic25",				0x040000, 0x70305c68, 0 | BRF_GRA },           // 29

	{ "mpr-14370.ic36",				0x080000, 0xfda78289, 0 | BRF_GRA },           // 30 Sub Sprites
	{ "mpr-14369.ic32",				0x080000, 0x7765116d, 0 | BRF_GRA },           // 31
	{ "mpr-14368.ic27",				0x080000, 0x5744a30e, 0 | BRF_GRA },           // 32
	{ "mpr-14367.ic23",				0x080000, 0x77bb9003, 0 | BRF_GRA },           // 33
	{ "mpr-14366.ic37",				0x080000, 0x21078e83, 0 | BRF_GRA },           // 34
	{ "mpr-14365.ic33",				0x080000, 0x36913790, 0 | BRF_GRA },           // 35
	{ "mpr-14364.ic28",				0x080000, 0x0fa12ecd, 0 | BRF_GRA },           // 36
	{ "mpr-14363.ic24",				0x080000, 0xf3427a56, 0 | BRF_GRA },           // 37
};

STD_ROM_PICK(f1en)
STD_ROM_FN(f1en)

static INT32 F1enInit()
{
	sprite_length = 0x800000;
	DrvLoadRoms(false);
	BurnAllocMemIndex();
	if (DrvLoadRoms(true)) return 1;

	system32_v60_map();
	system32_sound_init();
	tilemap_configure_allocate();

	custom_io_write_0  = f1en_custom_io_write;
	custom_io_read_0 = analog_custom_io_read;

	DrvDoReset();

	return 0;
}

struct BurnDriver BurnDrvF1en = {
	"f1en", NULL, NULL, NULL, "1991",
	"F1 Exhaust Note (World, Rev A)\0", NULL, "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 1, HARDWARE_SEGA_SYSTEM32, GBF_RACING, 0,
	NULL, f1enRomInfo, f1enRomName, NULL, NULL, NULL, NULL, F1enInputInfo, F1enDIPInfo,
	F1enInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};


// F1 Exhaust Note (US, Rev A)

static struct BurnRomInfo f1enuRomDesc[] = {
	{ "epr-14451a.ic6",				0x020000, 0xe17259c9, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "epr-14445.ic14",				0x040000, 0xd06261ab, 2 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-14444.ic7",				0x040000, 0x07724354, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "epr-14449.ic35",				0x020000, 0x2d29699c, 3 | BRF_PRG | BRF_ESS }, //  3 Z80 #0 Code
	{ "epr-14448.ic31",				0x080000, 0x87ca1e8d, 3 | BRF_PRG | BRF_ESS }, //  4
	{ "epr-14447.ic26",				0x080000, 0xdb1cfcbd, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "epr-14446.ic22",				0x080000, 0x646ec2cb, 3 | BRF_PRG | BRF_ESS }, //  6

	{ "mpr-14362.ic38",				0x040000, 0xfb1c4e79, 3 | BRF_GRA },           //  7 Main Layer Tiles
	{ "mpr-14361.ic34",				0x040000, 0xe3204bda, 3 | BRF_GRA },           //  8
	{ "mpr-14360.ic29",				0x040000, 0xc5e8da79, 3 | BRF_GRA },           //  9
	{ "mpr-14359.ic25",				0x040000, 0x70305c68, 3 | BRF_GRA },           // 10

	{ "mpr-14370.ic36",				0x080000, 0xfda78289, 4 | BRF_GRA },           // 11 Main Sprites
	{ "mpr-14369.ic32",				0x080000, 0x7765116d, 4 | BRF_GRA },           // 12
	{ "mpr-14368.ic27",				0x080000, 0x5744a30e, 4 | BRF_GRA },           // 13
	{ "mpr-14367.ic23",				0x080000, 0x77bb9003, 4 | BRF_GRA },           // 14
	{ "mpr-14366.ic37",				0x080000, 0x21078e83, 4 | BRF_GRA },           // 15
	{ "mpr-14365.ic33",				0x080000, 0x36913790, 4 | BRF_GRA },           // 16
	{ "mpr-14364.ic28",				0x080000, 0x0fa12ecd, 4 | BRF_GRA },           // 17
	{ "mpr-14363.ic24",				0x080000, 0xf3427a56, 4 | BRF_GRA },           // 18

	{ "epr-14451a.ic6",				0x020000, 0xe17259c9, 0 | BRF_PRG | BRF_ESS }, // 19 V60 #1 Code (sub)
	{ "epr-14445.ic14",				0x040000, 0xd06261ab, 0 | BRF_PRG | BRF_ESS }, // 20
	{ "epr-14444.ic7",				0x040000, 0x07724354, 0 | BRF_PRG | BRF_ESS }, // 21

	{ "epr-14449.ic35",				0x020000, 0x2d29699c, 0 | BRF_PRG | BRF_ESS }, // 22 Z80 #1 Code (sub)
	{ "epr-14448.ic31",				0x080000, 0x87ca1e8d, 0 | BRF_PRG | BRF_ESS }, // 23
	{ "epr-14447.ic26",				0x080000, 0xdb1cfcbd, 0 | BRF_PRG | BRF_ESS }, // 24
	{ "epr-14446.ic22",				0x080000, 0x646ec2cb, 0 | BRF_PRG | BRF_ESS }, // 25

	{ "mpr-14362.ic38",				0x040000, 0xfb1c4e79, 0 | BRF_GRA },           // 26 Sub Layer Tiles
	{ "mpr-14361.ic34",				0x040000, 0xe3204bda, 0 | BRF_GRA },           // 27
	{ "mpr-14360.ic29",				0x040000, 0xc5e8da79, 0 | BRF_GRA },           // 28
	{ "mpr-14359.ic25",				0x040000, 0x70305c68, 0 | BRF_GRA },           // 29

	{ "mpr-14370.ic36",				0x080000, 0xfda78289, 0 | BRF_GRA },           // 30 Sub Sprites
	{ "mpr-14369.ic32",				0x080000, 0x7765116d, 0 | BRF_GRA },           // 31
	{ "mpr-14368.ic27",				0x080000, 0x5744a30e, 0 | BRF_GRA },           // 32
	{ "mpr-14367.ic23",				0x080000, 0x77bb9003, 0 | BRF_GRA },           // 33
	{ "mpr-14366.ic37",				0x080000, 0x21078e83, 0 | BRF_GRA },           // 34
	{ "mpr-14365.ic33",				0x080000, 0x36913790, 0 | BRF_GRA },           // 35
	{ "mpr-14364.ic28",				0x080000, 0x0fa12ecd, 0 | BRF_GRA },           // 36
	{ "mpr-14363.ic24",				0x080000, 0xf3427a56, 0 | BRF_GRA },           // 37
};

STD_ROM_PICK(f1enu)
STD_ROM_FN(f1enu)

struct BurnDriver BurnDrvF1enu = {
	"f1enu", "f1en", NULL, NULL, "1991",
	"F1 Exhaust Note (US, Rev A)\0", NULL, "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 1, HARDWARE_SEGA_SYSTEM32, GBF_RACING, 0,
	NULL, f1enuRomInfo, f1enuRomName, NULL, NULL, NULL, NULL, F1enInputInfo, F1enDIPInfo,
	F1enInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};


// F1 Exhaust Note (Japan, Rev A)

static struct BurnRomInfo f1enjRomDesc[] = {
	{ "epr-14450a.ic6",				0x020000, 0x10f62723, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "epr-14445.ic14",				0x040000, 0xd06261ab, 2 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-14444.ic7",				0x040000, 0x07724354, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "epr-14449.ic35",				0x020000, 0x2d29699c, 3 | BRF_PRG | BRF_ESS }, //  3 Z80 #0 Code
	{ "epr-14448.ic31",				0x080000, 0x87ca1e8d, 3 | BRF_PRG | BRF_ESS }, //  4
	{ "epr-14447.ic26",				0x080000, 0xdb1cfcbd, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "epr-14446.ic22",				0x080000, 0x646ec2cb, 3 | BRF_PRG | BRF_ESS }, //  6

	{ "mpr-14362.ic38",				0x040000, 0xfb1c4e79, 3 | BRF_GRA },           //  7 Main Layer Tiles
	{ "mpr-14361.ic34",				0x040000, 0xe3204bda, 3 | BRF_GRA },           //  8
	{ "mpr-14360.ic29",				0x040000, 0xc5e8da79, 3 | BRF_GRA },           //  9
	{ "mpr-14359.ic25",				0x040000, 0x70305c68, 3 | BRF_GRA },           // 10

	{ "mpr-14370.ic36",				0x080000, 0xfda78289, 4 | BRF_GRA },           // 11 Main Sprites
	{ "mpr-14369.ic32",				0x080000, 0x7765116d, 4 | BRF_GRA },           // 12
	{ "mpr-14368.ic27",				0x080000, 0x5744a30e, 4 | BRF_GRA },           // 13
	{ "mpr-14367.ic23",				0x080000, 0x77bb9003, 4 | BRF_GRA },           // 14
	{ "mpr-14366.ic37",				0x080000, 0x21078e83, 4 | BRF_GRA },           // 15
	{ "mpr-14365.ic33",				0x080000, 0x36913790, 4 | BRF_GRA },           // 16
	{ "mpr-14364.ic28",				0x080000, 0x0fa12ecd, 4 | BRF_GRA },           // 17
	{ "mpr-14363.ic24",				0x080000, 0xf3427a56, 4 | BRF_GRA },           // 18

	{ "epr-14450a.ic6",				0x020000, 0x10f62723, 0 | BRF_PRG | BRF_ESS }, // 19 V60 #1 Code (sub)
	{ "epr-14445.ic14",				0x040000, 0xd06261ab, 0 | BRF_PRG | BRF_ESS }, // 20
	{ "epr-14444.ic7",				0x040000, 0x07724354, 0 | BRF_PRG | BRF_ESS }, // 21

	{ "epr-14449.ic35",				0x020000, 0x2d29699c, 0 | BRF_PRG | BRF_ESS }, // 22 Z80 #1 Code (sub)
	{ "epr-14448.ic31",				0x080000, 0x87ca1e8d, 0 | BRF_PRG | BRF_ESS }, // 23
	{ "epr-14447.ic26",				0x080000, 0xdb1cfcbd, 0 | BRF_PRG | BRF_ESS }, // 24
	{ "epr-14446.ic22",				0x080000, 0x646ec2cb, 0 | BRF_PRG | BRF_ESS }, // 25

	{ "mpr-14362.ic38",				0x040000, 0xfb1c4e79, 0 | BRF_GRA },           // 26 Sub Layer Tiles
	{ "mpr-14361.ic34",				0x040000, 0xe3204bda, 0 | BRF_GRA },           // 27
	{ "mpr-14360.ic29",				0x040000, 0xc5e8da79, 0 | BRF_GRA },           // 28
	{ "mpr-14359.ic25",				0x040000, 0x70305c68, 0 | BRF_GRA },           // 29

	{ "mpr-14370.ic36",				0x080000, 0xfda78289, 0 | BRF_GRA },           // 30 Sub Sprites
	{ "mpr-14369.ic32",				0x080000, 0x7765116d, 0 | BRF_GRA },           // 31
	{ "mpr-14368.ic27",				0x080000, 0x5744a30e, 0 | BRF_GRA },           // 32
	{ "mpr-14367.ic23",				0x080000, 0x77bb9003, 0 | BRF_GRA },           // 33
	{ "mpr-14366.ic37",				0x080000, 0x21078e83, 0 | BRF_GRA },           // 34
	{ "mpr-14365.ic33",				0x080000, 0x36913790, 0 | BRF_GRA },           // 35
	{ "mpr-14364.ic28",				0x080000, 0x0fa12ecd, 0 | BRF_GRA },           // 36
	{ "mpr-14363.ic24",				0x080000, 0xf3427a56, 0 | BRF_GRA },           // 37
};

STD_ROM_PICK(f1enj)
STD_ROM_FN(f1enj)

struct BurnDriver BurnDrvF1enj = {
	"f1enj", "f1en", NULL, NULL, "1991",
	"F1 Exhaust Note (Japan, Rev A)\0", NULL, "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 1, HARDWARE_SEGA_SYSTEM32, GBF_RACING, 0,
	NULL, f1enjRomInfo, f1enjRomName, NULL, NULL, NULL, NULL, F1enInputInfo, F1enDIPInfo,
	F1enInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};


// Jurassic Park (World, Rev A)

static struct BurnRomInfo jparkRomDesc[] = {
	{ "epr-16402a.ic8",				0x080000, 0xc70db239, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "epr-16395.ic18",				0x080000, 0xac5a01d6, 2 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-16394.ic9",				0x080000, 0xc08c3a8a, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "epr-16399.ic36",				0x040000, 0xb09b2fe3, 3 | BRF_PRG | BRF_ESS }, //  3 Z80 #0 Code
	{ "mpr-16398.ic35",				0x100000, 0xfa710ca6, 3 | BRF_PRG | BRF_ESS }, //  4
	{ "mpr-16397.ic34",				0x100000, 0x6e96e0be, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "mpr-16396.ic24",				0x100000, 0xf69a2dc4, 3 | BRF_PRG | BRF_ESS }, //  6

	{ "mpr-16404.ic14",				0x200000, 0x11283807, 1 | BRF_GRA },           //  7 Main Layer Tiles
	{ "mpr-16403.ic5",				0x200000, 0x02530a9b, 1 | BRF_GRA },           //  8

	{ "mpr-16405.ic32",				0x200000, 0xb425f182, 2 | BRF_GRA },           //  9 Main Sprites
	{ "mpr-16407.ic30",				0x200000, 0xbc49ffd9, 2 | BRF_GRA },           // 10
	{ "mpr-16409.ic28",				0x200000, 0xfe73660d, 2 | BRF_GRA },           // 11
	{ "mpr-16411.ic26",				0x200000, 0x71cabbc5, 2 | BRF_GRA },           // 12
	{ "mpr-16406.ic31",				0x200000, 0xb9ed73d6, 2 | BRF_GRA },           // 13
	{ "mpr-16408.ic29",				0x200000, 0x7b2f476b, 2 | BRF_GRA },           // 14
	{ "mpr-16410.ic27",				0x200000, 0x49c8f952, 2 | BRF_GRA },           // 15
	{ "mpr-16412.ic25",				0x200000, 0x105dc26e, 2 | BRF_GRA },           // 16

	{ "epr-13908.xx",				0x008000, 0x6228c1d2, 0 | BRF_PRG | BRF_ESS }, // 17 Cabinet Movement CPU (what CPU?)
};

STD_ROM_PICK(jpark)
STD_ROM_FN(jpark)

// remove drive board check
static void jpark_hack()
{
	UINT16 *mem = (UINT16*)DrvV60ROM;

	mem[0xC15A8/2] = BURN_ENDIAN_SWAP_INT16(0xCD70);
	mem[0xC15AA/2] = BURN_ENDIAN_SWAP_INT16(0xD8CD);
}

static INT32 JparkInit()
{
	sprite_length = 0;
	DrvLoadRoms(false);
	BurnAllocMemIndex();
	if (DrvLoadRoms(true)) return 1;

	has_gun = 1;

	system32_v60_map();
	system32_sound_init();
	tilemap_configure_allocate();

	custom_io_write_0 = jpark_custom_io_write;
	custom_io_read_0  = analog_custom_io_read;

	jpark_hack();

	DrvDoReset();

	return 0;
}

struct BurnDriver BurnDrvJpark = {
	"jpark", NULL, NULL, NULL, "1993",
	"Jurassic Park (World, Rev A)\0", "Small GFX Issues", "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SYSTEM32, GBF_SHOOT, 0,
	NULL, jparkRomInfo, jparkRomName, NULL, NULL, NULL, NULL, JparkInputInfo, JparkDIPInfo,
	JparkInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};


// Jurassic Park (Japan, Rev A, Deluxe)

static struct BurnRomInfo jparkjRomDesc[] = {
	{ "epr-16400a.ic8",				0x080000, 0x1e03dbfe, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "epr-16395.ic18",				0x080000, 0xac5a01d6, 2 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-16394.ic9",				0x080000, 0xc08c3a8a, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "epr-16399.ic36",				0x040000, 0xb09b2fe3, 3 | BRF_PRG | BRF_ESS }, //  3 Z80 #0 Code
	{ "mpr-16398.ic35",				0x100000, 0xfa710ca6, 3 | BRF_PRG | BRF_ESS }, //  4
	{ "mpr-16397.ic34",				0x100000, 0x6e96e0be, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "mpr-16396.ic24",				0x100000, 0xf69a2dc4, 3 | BRF_PRG | BRF_ESS }, //  6

	{ "mpr-16404.ic14",				0x200000, 0x11283807, 1 | BRF_GRA },           //  7 Main Layer Tiles
	{ "mpr-16403.ic5",				0x200000, 0x02530a9b, 1 | BRF_GRA },           //  8

	{ "mpr-16405.ic32",				0x200000, 0xb425f182, 2 | BRF_GRA },           //  9 Main Sprites
	{ "mpr-16407.ic30",				0x200000, 0xbc49ffd9, 2 | BRF_GRA },           // 10
	{ "mpr-16409.ic28",				0x200000, 0xfe73660d, 2 | BRF_GRA },           // 11
	{ "mpr-16411.ic26",				0x200000, 0x71cabbc5, 2 | BRF_GRA },           // 12
	{ "mpr-16406.ic31",				0x200000, 0xb9ed73d6, 2 | BRF_GRA },           // 13
	{ "mpr-16408.ic29",				0x200000, 0x7b2f476b, 2 | BRF_GRA },           // 14
	{ "mpr-16410.ic27",				0x200000, 0x49c8f952, 2 | BRF_GRA },           // 15
	{ "mpr-16412.ic25",				0x200000, 0x105dc26e, 2 | BRF_GRA },           // 16

	{ "epr-13908.xx",				0x008000, 0x6228c1d2, 0 | BRF_PRG | BRF_ESS }, // 17 Cabinet Movement CPU (what CPU?)
};

STD_ROM_PICK(jparkj)
STD_ROM_FN(jparkj)

struct BurnDriver BurnDrvJparkj = {
	"jparkj", "jpark", NULL, NULL, "1993",
	"Jurassic Park (Japan, Rev A, Deluxe)\0", "Small GFX Issues", "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SYSTEM32, GBF_SHOOT, 0,
	NULL, jparkjRomInfo, jparkjRomName, NULL, NULL, NULL, NULL, JparkInputInfo, JparkDIPInfo,
	JparkInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};


// Jurassic Park (Japan, Deluxe)

static struct BurnRomInfo jparkjaRomDesc[] = {
	{ "epr-16400.ic8",				0x080000, 0x321c3411, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "epr-16395.ic18",				0x080000, 0xac5a01d6, 2 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-16394.ic9",				0x080000, 0xc08c3a8a, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "epr-16399.ic36",				0x040000, 0xb09b2fe3, 3 | BRF_PRG | BRF_ESS }, //  3 Z80 #0 Code
	{ "mpr-16398.ic35",				0x100000, 0xfa710ca6, 3 | BRF_PRG | BRF_ESS }, //  4
	{ "mpr-16397.ic34",				0x100000, 0x6e96e0be, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "mpr-16396.ic24",				0x100000, 0xf69a2dc4, 3 | BRF_PRG | BRF_ESS }, //  6

	{ "mpr-16404.ic14",				0x200000, 0x11283807, 1 | BRF_GRA },           //  7 Main Layer Tiles
	{ "mpr-16403.ic5",				0x200000, 0x02530a9b, 1 | BRF_GRA },           //  8

	{ "mpr-16405.ic32",				0x200000, 0xb425f182, 2 | BRF_GRA },           //  9 Main Sprites
	{ "mpr-16407.ic30",				0x200000, 0xbc49ffd9, 2 | BRF_GRA },           // 10
	{ "mpr-16409.ic28",				0x200000, 0xfe73660d, 2 | BRF_GRA },           // 11
	{ "mpr-16411.ic26",				0x200000, 0x71cabbc5, 2 | BRF_GRA },           // 12
	{ "mpr-16406.ic31",				0x200000, 0xb9ed73d6, 2 | BRF_GRA },           // 13
	{ "mpr-16408.ic29",				0x200000, 0x7b2f476b, 2 | BRF_GRA },           // 14
	{ "mpr-16410.ic27",				0x200000, 0x49c8f952, 2 | BRF_GRA },           // 15
	{ "mpr-16412.ic25",				0x200000, 0x105dc26e, 2 | BRF_GRA },           // 16

	{ "epr-13908.xx",				0x008000, 0x6228c1d2, 0 | BRF_PRG | BRF_ESS }, // 17 Cabinet Movement CPU (what CPU?)
};

STD_ROM_PICK(jparkja)
STD_ROM_FN(jparkja)

struct BurnDriver BurnDrvJparkja = {
	"jparkja", "jpark", NULL, NULL, "1993",
	"Jurassic Park (Japan, Deluxe)\0", "Small GFX Issues", "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SYSTEM32, GBF_SHOOT, 0,
	NULL, jparkjaRomInfo, jparkjaRomName, NULL, NULL, NULL, NULL, JparkInputInfo, JparkDIPInfo,
	JparkInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};


// Jurassic Park (Japan, Rev A, Conversion)

static struct BurnRomInfo jparkjcRomDesc[] = {
	{ "epr-16400a.ic8",				0x080000, 0x1e03dbfe, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "epr-16395.ic18",				0x080000, 0xac5a01d6, 2 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-16394.ic9",				0x080000, 0xc08c3a8a, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "epr-16630.ic36",				0x040000, 0x955855eb, 3 | BRF_PRG | BRF_ESS }, //  3 Z80 #0 Code
	{ "mpr-16398.ic35",				0x100000, 0xfa710ca6, 3 | BRF_PRG | BRF_ESS }, //  4
	{ "mpr-16397.ic34",				0x100000, 0x6e96e0be, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "mpr-16396.ic24",				0x100000, 0xf69a2dc4, 3 | BRF_PRG | BRF_ESS }, //  6

	{ "mpr-16404.ic14",				0x200000, 0x11283807, 1 | BRF_GRA },           //  7 Main Layer Tiles
	{ "mpr-16403.ic5",				0x200000, 0x02530a9b, 1 | BRF_GRA },           //  8

	{ "mpr-16405.ic32",				0x200000, 0xb425f182, 2 | BRF_GRA },           //  9 Main Sprites
	{ "mpr-16407.ic30",				0x200000, 0xbc49ffd9, 2 | BRF_GRA },           // 10
	{ "mpr-16409.ic28",				0x200000, 0xfe73660d, 2 | BRF_GRA },           // 11
	{ "mpr-16411.ic26",				0x200000, 0x71cabbc5, 2 | BRF_GRA },           // 12
	{ "mpr-16406.ic31",				0x200000, 0xb9ed73d6, 2 | BRF_GRA },           // 13
	{ "mpr-16408.ic29",				0x200000, 0x7b2f476b, 2 | BRF_GRA },           // 14
	{ "mpr-16410.ic27",				0x200000, 0x49c8f952, 2 | BRF_GRA },           // 15
	{ "mpr-16412.ic25",				0x200000, 0x105dc26e, 2 | BRF_GRA },           // 16
};

STD_ROM_PICK(jparkjc)
STD_ROM_FN(jparkjc)

struct BurnDriver BurnDrvJparkjc = {
	"jparkjc", "jpark", NULL, NULL, "1993",
	"Jurassic Park (Japan, Rev A, Conversion)\0", NULL, "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SYSTEM32, GBF_SHOOT, 0,
	NULL, jparkjcRomInfo, jparkjcRomName, NULL, NULL, NULL, NULL, JparkInputInfo, JparkDIPInfo,
	JparkInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};


// Rad Mobile (World)

static struct BurnRomInfo radmRomDesc[] = {
	{ "epr-13693.ic21",				0x020000, 0x3f09a211, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "epr-13525.ic37",				0x080000, 0x62ad83a0, 2 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-13526.ic38",				0x080000, 0x59ea372a, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "epr-13527.ic9",				0x020000, 0xa2e3fbbe, 3 | BRF_PRG | BRF_ESS }, //  3 Z80 #0 Code
	{ "epr-13523.ic14",				0x080000, 0xd5563697, 3 | BRF_PRG | BRF_ESS }, //  4
	{ "epr-13699.ic20",				0x080000, 0x33fd2913, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "epr-13523.ic22",				0x080000, 0xd5563697, 3 | BRF_PRG | BRF_ESS }, //  6

	{ "mpr-13519.ic3",				0x080000, 0xbedc9534, 3 | BRF_GRA },           //  7 Main Layer Tiles
	{ "mpr-13520.ic7",				0x080000, 0x3532e91a, 3 | BRF_GRA },           //  8
	{ "mpr-13521.ic12",				0x080000, 0xe9bca903, 3 | BRF_GRA },           //  9
	{ "mpr-13522.ic18",				0x080000, 0x25e04648, 3 | BRF_GRA },           // 10

	{ "mpr-13511.ic1",				0x100000, 0xf8f15b11, 4 | BRF_GRA },           // 11 Main Sprites
	{ "mpr-13512.ic5",				0x100000, 0xd0be34a6, 4 | BRF_GRA },           // 12
	{ "mpr-13513.ic10",				0x100000, 0xfeef1982, 4 | BRF_GRA },           // 13
	{ "mpr-13514.ic16",				0x100000, 0xd0f9ebd1, 4 | BRF_GRA },           // 14
	{ "mpr-13515.ic2",				0x100000, 0x77bf2387, 4 | BRF_GRA },           // 15
	{ "mpr-13516.ic6",				0x100000, 0x8c4bc62d, 4 | BRF_GRA },           // 16
	{ "mpr-13517.ic11",				0x100000, 0x1d7d84a7, 4 | BRF_GRA },           // 17
	{ "mpr-13518.ic17",				0x100000, 0x9ea4b15d, 4 | BRF_GRA },           // 18

	{ "epr-13686.bin",				0x008000, 0x317a2857, 0 | BRF_PRG | BRF_ESS }, // 19 Cabinet Movement CPU? (what CPU?)

	{ "eeprom-radm.ic76",			0x000080, 0xb1737c06, 7 | BRF_PRG | BRF_ESS }, // 20 Default EEPROM
};

STD_ROM_PICK(radm)
STD_ROM_FN(radm)

static INT32 RadmInit()
{
	is_radm = 1;
	sprite_length = 0;
	DrvLoadRoms(false);
	BurnAllocMemIndex();
	if (DrvLoadRoms(true)) return 1;

	system32_v60_map();
	system32_sound_init();
	tilemap_configure_allocate();

	custom_io_write_0  = radm_custom_io_write;
	custom_io_read_0 = analog_custom_io_read;

	DrvDoReset();

	return 0;
}

struct BurnDriver BurnDrvRadm = {
	"radm", NULL, NULL, NULL, "1990",
	"Rad Mobile (World)\0", NULL, "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_SYSTEM32, GBF_RACING, 0,
	NULL, radmRomInfo, radmRomName, NULL, NULL, NULL, NULL, RadmInputInfo, RadmDIPInfo,
	RadmInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};


// Rad Mobile (US)

static struct BurnRomInfo radmuRomDesc[] = {
	{ "epr-13690.ic21",				0x020000, 0x21637dec, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "epr-13525.ic37",				0x080000, 0x62ad83a0, 2 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-13526.ic38",				0x080000, 0x59ea372a, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "epr-13527.ic9",				0x020000, 0xa2e3fbbe, 3 | BRF_PRG | BRF_ESS }, //  3 Z80 #0 Code
	{ "epr-13523.ic14",				0x080000, 0xd5563697, 3 | BRF_PRG | BRF_ESS }, //  4
	{ "epr-13699.ic20",				0x080000, 0x33fd2913, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "epr-13523.ic22",				0x080000, 0xd5563697, 3 | BRF_PRG | BRF_ESS }, //  6

	{ "mpr-13519.ic3",				0x080000, 0xbedc9534, 3 | BRF_GRA },           //  7 Main Layer Tiles
	{ "mpr-13520.ic7",				0x080000, 0x3532e91a, 3 | BRF_GRA },           //  8
	{ "mpr-13521.ic12",				0x080000, 0xe9bca903, 3 | BRF_GRA },           //  9
	{ "mpr-13522.ic18",				0x080000, 0x25e04648, 3 | BRF_GRA },           // 10

	{ "mpr-13511.ic1",				0x100000, 0xf8f15b11, 4 | BRF_GRA },           // 11 Main Sprites
	{ "mpr-13512.ic5",				0x100000, 0xd0be34a6, 4 | BRF_GRA },           // 12
	{ "mpr-13513.ic10",				0x100000, 0xfeef1982, 4 | BRF_GRA },           // 13
	{ "mpr-13514.ic16",				0x100000, 0xd0f9ebd1, 4 | BRF_GRA },           // 14
	{ "mpr-13515.ic2",				0x100000, 0x77bf2387, 4 | BRF_GRA },           // 15
	{ "mpr-13516.ic6",				0x100000, 0x8c4bc62d, 4 | BRF_GRA },           // 16
	{ "mpr-13517.ic11",				0x100000, 0x1d7d84a7, 4 | BRF_GRA },           // 17
	{ "mpr-13518.ic17",				0x100000, 0x9ea4b15d, 4 | BRF_GRA },           // 18

	{ "epr-13686.bin",				0x008000, 0x317a2857, 0 | BRF_PRG | BRF_ESS }, // 19 Cabinet Movement CPU? (what CPU?)

	{ "eeprom-radm.ic76",			0x000080, 0xb1737c06, 7 | BRF_PRG | BRF_ESS }, // 20 Default EEPROM
};

STD_ROM_PICK(radmu)
STD_ROM_FN(radmu)

struct BurnDriver BurnDrvRadmu = {
	"radmu", "radm", NULL, NULL, "1990",
	"Rad Mobile (US)\0", NULL, "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SYSTEM32, GBF_RACING, 0,
	NULL, radmuRomInfo, radmuRomName, NULL, NULL, NULL, NULL, RadmInputInfo, RadmDIPInfo,
	RadmInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};


// Rad Rally (World)

static struct BurnRomInfo radrRomDesc[] = {
	{ "epr-14241.ic21",				0x020000, 0x59a5f63d, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "epr-14106.ic37",				0x080000, 0xe73c63bf, 2 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-14107.ic38",				0x080000, 0x832f797a, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "epr-14108.ic9",				0x020000, 0x38a99b4d, 3 | BRF_PRG | BRF_ESS }, //  3 Z80 #0 Code
	{ "epr-14109.ic14",				0x080000, 0xd5563697, 3 | BRF_PRG | BRF_ESS }, //  4
	{ "epr-14110.ic20",				0x080000, 0x33fd2913, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "epr-14237.ic22",				0x080000, 0x0a4b4b29, 3 | BRF_PRG | BRF_ESS }, //  6

	{ "epr-14102.ic3",				0x040000, 0x5626e80f, 3 | BRF_GRA },           //  7 Main Layer Tiles
	{ "epr-14103.ic7",				0x040000, 0x08c7e804, 3 | BRF_GRA },           //  8
	{ "epr-14104.ic12",				0x040000, 0xb0173646, 3 | BRF_GRA },           //  9
	{ "epr-14105.ic18",				0x040000, 0x614843b6, 3 | BRF_GRA },           // 10

	{ "mpr-13511.ic1",				0x100000, 0xf8f15b11, 4 | BRF_GRA },           // 11 Main Sprites
	{ "mpr-13512.ic5",				0x100000, 0xd0be34a6, 4 | BRF_GRA },           // 12
	{ "mpr-13513.ic10",				0x100000, 0xfeef1982, 4 | BRF_GRA },           // 13
	{ "mpr-13514.ic16",				0x100000, 0xd0f9ebd1, 4 | BRF_GRA },           // 14
	{ "mpr-13515.ic2",				0x100000, 0x77bf2387, 4 | BRF_GRA },           // 15
	{ "mpr-13516.ic6",				0x100000, 0x8c4bc62d, 4 | BRF_GRA },           // 16
	{ "mpr-13517.ic11",				0x100000, 0x1d7d84a7, 4 | BRF_GRA },           // 17
	{ "mpr-13518.ic17",				0x100000, 0x9ea4b15d, 4 | BRF_GRA },           // 18

	{ "epr-14084.17",				0x008000, 0xf14ed074, 0 | BRF_PRG | BRF_ESS }, // 19 Cabinet Movement CPU? (what CPU?)

	{ "eeprom-radr.ic76",			0x000080, 0x602032c6, 7 | BRF_PRG | BRF_ESS }, // 20 Default EEPROM
};

STD_ROM_PICK(radr)
STD_ROM_FN(radr)

static INT32 RadrInit()
{
	is_radr = 1;

	can_modechange = 1;

	sprite_length = 0;
	DrvLoadRoms(false);
	BurnAllocMemIndex();
	if (DrvLoadRoms(true)) return 1;

	system32_v60_map();
	system32_sound_init();
	tilemap_configure_allocate();

	custom_io_write_0  = f1en_custom_io_write;
	custom_io_read_0 = analog_custom_io_read;

	opaquey_hack = 1;

	DrvDoReset();

	return 0;
}

struct BurnDriver BurnDrvRadr = {
	"radr", NULL, NULL, NULL, "1991",
	"Rad Rally (World)\0", "Small GFX Issues", "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_SYSTEM32, GBF_RACING, 0,
	NULL, radrRomInfo, radrRomName, NULL, NULL, NULL, NULL, RadrInputInfo, RadrDIPInfo,
	RadrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	416, 224, 4, 3
};


// Rad Rally (US)

static struct BurnRomInfo radruRomDesc[] = {
	{ "epr-14240.ic21",				0x020000, 0x8473e7ab, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "epr-14106.ic37",				0x080000, 0xe73c63bf, 2 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-14107.ic38",				0x080000, 0x832f797a, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "epr-14108.ic9",				0x020000, 0x38a99b4d, 3 | BRF_PRG | BRF_ESS }, //  3 Z80 #0 Code
	{ "epr-14109.ic14",				0x080000, 0xd5563697, 3 | BRF_PRG | BRF_ESS }, //  4
	{ "epr-14110.ic20",				0x080000, 0x33fd2913, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "epr-14237.ic22",				0x080000, 0x0a4b4b29, 3 | BRF_PRG | BRF_ESS }, //  6

	{ "epr-14102.ic3",				0x040000, 0x5626e80f, 3 | BRF_GRA },           //  7 Main Layer Tiles
	{ "epr-14103.ic7",				0x040000, 0x08c7e804, 3 | BRF_GRA },           //  8
	{ "epr-14104.ic12",				0x040000, 0xb0173646, 3 | BRF_GRA },           //  9
	{ "epr-14105.ic16",				0x040000, 0x614843b6, 3 | BRF_GRA },           // 10

	{ "mpr-13511.ic1",				0x100000, 0xf8f15b11, 4 | BRF_GRA },           // 11 Main Sprites
	{ "mpr-13512.ic5",				0x100000, 0xd0be34a6, 4 | BRF_GRA },           // 12
	{ "mpr-13513.ic10",				0x100000, 0xfeef1982, 4 | BRF_GRA },           // 13
	{ "mpr-13514.ic16",				0x100000, 0xd0f9ebd1, 4 | BRF_GRA },           // 14
	{ "mpr-13515.ic2",				0x100000, 0x77bf2387, 4 | BRF_GRA },           // 15
	{ "mpr-13516.ic6",				0x100000, 0x8c4bc62d, 4 | BRF_GRA },           // 16
	{ "mpr-13517.ic11",				0x100000, 0x1d7d84a7, 4 | BRF_GRA },           // 17
	{ "mpr-13518.ic17",				0x100000, 0x9ea4b15d, 4 | BRF_GRA },           // 18

	{ "epr-14084.17",				0x008000, 0xf14ed074, 0 | BRF_PRG | BRF_ESS }, // 19 Cabinet Movement CPU? (what CPU?)

	{ "eeprom-radr.ic76",			0x000080, 0x602032c6, 7 | BRF_PRG | BRF_ESS }, // 20 Default EEPROM
};

STD_ROM_PICK(radru)
STD_ROM_FN(radru)

struct BurnDriver BurnDrvRadru = {
	"radru", "radr", NULL, NULL, "1991",
	"Rad Rally (US)\0", "Small GFX Issues", "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SYSTEM32, GBF_RACING, 0,
	NULL, radruRomInfo, radruRomName, NULL, NULL, NULL, NULL, RadrInputInfo, RadrDIPInfo,
	RadrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	416, 224, 4, 3
};


// Rad Rally (Japan)

static struct BurnRomInfo radrjRomDesc[] = {
	{ "epr-14111.ic21",				0x020000, 0x7adc6d17, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "epr-14106.ic37",				0x080000, 0xe73c63bf, 2 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-14107.ic38",				0x080000, 0x832f797a, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "epr-14108.ic9",				0x020000, 0x38a99b4d, 3 | BRF_PRG | BRF_ESS }, //  3 Z80 #0 Code
	{ "epr-14109.ic14",				0x080000, 0xd5563697, 3 | BRF_PRG | BRF_ESS }, //  4
	{ "epr-14110.ic20",				0x080000, 0x33fd2913, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "epr-14237.ic22",				0x080000, 0x0a4b4b29, 3 | BRF_PRG | BRF_ESS }, //  6

	{ "epr-14102.ic3",				0x040000, 0x5626e80f, 3 | BRF_GRA },           //  7 Main Layer Tiles
	{ "epr-14103.ic7",				0x040000, 0x08c7e804, 3 | BRF_GRA },           //  8
	{ "epr-14104.ic12",				0x040000, 0xb0173646, 3 | BRF_GRA },           //  9
	{ "epr-14105.ic16",				0x040000, 0x614843b6, 3 | BRF_GRA },           // 10

	{ "mpr-13511.ic1",				0x100000, 0xf8f15b11, 4 | BRF_GRA },           // 11 Main Sprites
	{ "mpr-13512.ic5",				0x100000, 0xd0be34a6, 4 | BRF_GRA },           // 12
	{ "mpr-13513.ic10",				0x100000, 0xfeef1982, 4 | BRF_GRA },           // 13
	{ "mpr-13514.ic16",				0x100000, 0xd0f9ebd1, 4 | BRF_GRA },           // 14
	{ "mpr-13515.ic2",				0x100000, 0x77bf2387, 4 | BRF_GRA },           // 15
	{ "mpr-13516.ic6",				0x100000, 0x8c4bc62d, 4 | BRF_GRA },           // 16
	{ "mpr-13517.ic11",				0x100000, 0x1d7d84a7, 4 | BRF_GRA },           // 17
	{ "mpr-13518.ic17",				0x100000, 0x9ea4b15d, 4 | BRF_GRA },           // 18

	{ "epr-14084.17",				0x008000, 0xf14ed074, 0 | BRF_PRG | BRF_ESS }, // 19 Cabinet Movement CPU? (what CPU?)

	{ "eeprom-radr.ic76",			0x000080, 0x602032c6, 7 | BRF_PRG | BRF_ESS }, // 20 Default EEPROM
};

STD_ROM_PICK(radrj)
STD_ROM_FN(radrj)

struct BurnDriver BurnDrvRadrj = {
	"radrj", "radr", NULL, NULL, "1991",
	"Rad Rally (Japan)\0", "Small GFX Issues", "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SYSTEM32, GBF_RACING, 0,
	NULL, radrjRomInfo, radrjRomName, NULL, NULL, NULL, NULL, RadrInputInfo, RadrDIPInfo,
	RadrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	416, 224, 4, 3
};


// Slip Stream (Brazil 950515)

static struct BurnRomInfo slipstrmRomDesc[] = {
	{ "s32b_prg01.ic6",				0x080000, 0x7d066307, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "s32_dat00.ic14",				0x080000, 0xc3ff6309, 2 | BRF_PRG | BRF_ESS }, //  1
	{ "s32_dat01.ic7",				0x080000, 0x0e605c81, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "s32_snd00.ic35",				0x020000, 0x0fee2278, 3 | BRF_PRG | BRF_ESS }, //  3 Z80 #0 Code
	{ "s32_snd01.ic31",				0x080000, 0xae7be5f2, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "s32_scr00.ic38",				0x080000, 0x3cbb2d0b, 3 | BRF_GRA },           //  5 Main Layer Tiles
	{ "s32_scr01.ic34",				0x080000, 0x4167be55, 3 | BRF_GRA },           //  6
	{ "s32_scr02.ic29",				0x080000, 0x52c4bb85, 3 | BRF_GRA },           //  7
	{ "s32_scr03.ic25",				0x080000, 0x4948604a, 3 | BRF_GRA },           //  8

	{ "s32_obj00.ic36",				0x080000, 0xcffe9e0d, 4 | BRF_GRA },           //  9 Main Sprites
	{ "s32_obj01.ic32",				0x080000, 0x4ebd1383, 4 | BRF_GRA },           // 10
	{ "s32_obj02.ic27",				0x080000, 0xb3cf4fe2, 4 | BRF_GRA },           // 11
	{ "s32_obj03.ic23",				0x080000, 0xc6345391, 4 | BRF_GRA },           // 12
	{ "s32_obj04.ic37",				0x080000, 0x2de4288e, 4 | BRF_GRA },           // 13
	{ "s32_obj05.ic33",				0x080000, 0x6cfb74fb, 4 | BRF_GRA },           // 14
	{ "s32_obj06.ic28",				0x080000, 0x53234bf4, 4 | BRF_GRA },           // 15
	{ "s32_obj07.ic24",				0x080000, 0x22c129cf, 4 | BRF_GRA },           // 16
};

STD_ROM_PICK(slipstrm)
STD_ROM_FN(slipstrm)

static INT32 SlipstrmInit()
{
	can_modechange = 1;

	is_slipstrm = 1;

	sprite_length = 0;
	DrvLoadRoms(false);
	BurnAllocMemIndex();
	if (DrvLoadRoms(true)) return 1;

	system32_v60_map();
	system32_sound_init();
	tilemap_configure_allocate();

	custom_io_write_0  = f1en_custom_io_write;
	custom_io_read_0 = analog_custom_io_read;

	DrvDoReset();

	return 0;
}

struct BurnDriver BurnDrvSlipstrm = {
	"slipstrm", NULL, NULL, NULL, "1995",
	"Slip Stream (Brazil 950515)\0", NULL, "Capcom", "Sega System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 1, HARDWARE_SEGA_SYSTEM32, GBF_RACING, 0,
	NULL, slipstrmRomInfo, slipstrmRomName, NULL, NULL, NULL, NULL, SlipstrmInputInfo, SlipstrmDIPInfo,
	SlipstrmInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	416, 224, 4, 3
};


// Slip Stream (Hispanic 950515)

static struct BurnRomInfo slipstrmhRomDesc[] = {
	{ "s32h_prg01.ic6",				0x080000, 0xab778297, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "s32_dat00.ic14",				0x080000, 0xc3ff6309, 2 | BRF_PRG | BRF_ESS }, //  1
	{ "s32_dat01.ic7",				0x080000, 0x0e605c81, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "s32_snd00.ic35",				0x020000, 0x0fee2278, 3 | BRF_PRG | BRF_ESS }, //  3 Z80 #0 Code
	{ "s32_snd01.ic31",				0x080000, 0xae7be5f2, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "s32_scr00.ic38",				0x080000, 0x3cbb2d0b, 3 | BRF_GRA },           //  5 Main Layer Tiles
	{ "s32_scr01.ic34",				0x080000, 0x4167be55, 3 | BRF_GRA },           //  6
	{ "s32_scr02.ic29",				0x080000, 0x52c4bb85, 3 | BRF_GRA },           //  7
	{ "s32_scr03.ic25",				0x080000, 0x4948604a, 3 | BRF_GRA },           //  8

	{ "s32_obj00.ic36",				0x080000, 0xcffe9e0d, 4 | BRF_GRA },           //  9 Main Sprites
	{ "s32_obj01.ic32",				0x080000, 0x4ebd1383, 4 | BRF_GRA },           // 10
	{ "s32_obj02.ic27",				0x080000, 0xb3cf4fe2, 4 | BRF_GRA },           // 11
	{ "s32_obj03.ic23",				0x080000, 0xc6345391, 4 | BRF_GRA },           // 12
	{ "s32_obj04.ic37",				0x080000, 0x2de4288e, 4 | BRF_GRA },           // 13
	{ "s32_obj05.ic33",				0x080000, 0x6cfb74fb, 4 | BRF_GRA },           // 14
	{ "s32_obj06.ic28",				0x080000, 0x53234bf4, 4 | BRF_GRA },           // 15
	{ "s32_obj07.ic24",				0x080000, 0x22c129cf, 4 | BRF_GRA },           // 16
};

STD_ROM_PICK(slipstrmh)
STD_ROM_FN(slipstrmh)

struct BurnDriver BurnDrvSlipstrmh = {
	"slipstrmh", "slipstrm", NULL, NULL, "1995",
	"Slip Stream (Hispanic 950515)\0", NULL, "Capcom", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 1, HARDWARE_SEGA_SYSTEM32, GBF_RACING, 0,
	NULL, slipstrmhRomInfo, slipstrmhRomName, NULL, NULL, NULL, NULL, SlipstrmInputInfo, SlipstrmDIPInfo,
	SlipstrmInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	416, 224, 4, 3
};


// Super Visual Football: European Sega Cup (Rev A)

static struct BurnRomInfo svfRomDesc[] = {
	{ "epr-16872a.ic17",			0x020000, 0x1f383b00, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "epr-16871a.ic8",				0x020000, 0xf7061bd7, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-16865.ic18",				0x080000, 0x9198ca9f, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-16864.ic9",				0x080000, 0x201a940e, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-16866.ic36",				0x020000, 0x74431350, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #0 Code
	{ "mpr-16779.ic35",				0x100000, 0x7055e859, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "mpr-16778.ic34",				0x100000, 0xfeedaecf, 3 | BRF_PRG | BRF_ESS }, //  6
	{ "mpr-16777.ic24",				0x100000, 0x14b5d5df, 3 | BRF_PRG | BRF_ESS }, //  7

	{ "mpr-16784.ic14",				0x100000, 0x4608efe2, 1 | BRF_GRA },           //  8 Main Layer Tiles
	{ "mpr-16783.ic5",				0x100000, 0x042eabe7, 1 | BRF_GRA },           //  9

	{ "mpr-16785.ic32",				0x200000, 0x51f775ce, 2 | BRF_GRA },           // 10 Main Sprites
	{ "mpr-16787.ic30",				0x200000, 0xdee7a204, 2 | BRF_GRA },           // 11
	{ "mpr-16789.ic28",				0x200000, 0x6b6c8ad3, 2 | BRF_GRA },           // 12
	{ "mpr-16791.ic26",				0x200000, 0x4f7236da, 2 | BRF_GRA },           // 13
	{ "mpr-16860.ic31",				0x200000, 0x578a7325, 2 | BRF_GRA },           // 14
	{ "mpr-16861.ic29",				0x200000, 0xd79c3f73, 2 | BRF_GRA },           // 15
	{ "mpr-16862.ic27",				0x200000, 0x00793354, 2 | BRF_GRA },           // 16
	{ "mpr-16863.ic25",				0x200000, 0x42338226, 2 | BRF_GRA },           // 17
};

STD_ROM_PICK(svf)
STD_ROM_FN(svf)

static INT32 SvfInit()
{
	sprite_length = 0;
	DrvLoadRoms(false);
	BurnAllocMemIndex();
	if (DrvLoadRoms(true)) return 1;

	system32_v60_map();
	system32_sound_init();
	tilemap_configure_allocate();

	custom_io_write_0 = analog_custom_io_write;
	custom_io_read_0 = analog_custom_io_read;

	clr_opposites = 2;

	DrvDoReset();

	return 0;
}

struct BurnDriver BurnDrvSvf = {
	"svf", NULL, NULL, NULL, "1994",
	"Super Visual Football: European Sega Cup (Rev A)\0", NULL, "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SYSTEM32, GBF_SPORTSFOOTBALL, 0,
	NULL, svfRomInfo, svfRomName, NULL, NULL, NULL, NULL, DbzvrvsInputInfo, DbzvrvsDIPInfo,
	SvfInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};


// Super Visual Football: European Sega Cup

static struct BurnRomInfo svfoRomDesc[] = {
	{ "epr-16872.ic17",				0x020000, 0x654d8c95, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "epr-16871.ic8",				0x020000, 0xe0d0cac0, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-16865.ic18",				0x080000, 0x9198ca9f, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-16864.ic9",				0x080000, 0x201a940e, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-16866.ic36",				0x020000, 0x74431350, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #0 Code
	{ "mpr-16779.ic35",				0x100000, 0x7055e859, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "mpr-16778.ic34",				0x100000, 0xfeedaecf, 3 | BRF_PRG | BRF_ESS }, //  6
	{ "mpr-16777.ic24",				0x100000, 0x14b5d5df, 3 | BRF_PRG | BRF_ESS }, //  7

	{ "mpr-16784.ic14",				0x100000, 0x4608efe2, 1 | BRF_GRA },           //  8 Main Layer Tiles
	{ "mpr-16783.ic5",				0x100000, 0x042eabe7, 1 | BRF_GRA },           //  9

	{ "mpr-16785.ic32",				0x200000, 0x51f775ce, 2 | BRF_GRA },           // 10 Main Sprites
	{ "mpr-16787.ic30",				0x200000, 0xdee7a204, 2 | BRF_GRA },           // 11
	{ "mpr-16789.ic28",				0x200000, 0x6b6c8ad3, 2 | BRF_GRA },           // 12
	{ "mpr-16791.ic26",				0x200000, 0x4f7236da, 2 | BRF_GRA },           // 13
	{ "mpr-16860.ic31",				0x200000, 0x578a7325, 2 | BRF_GRA },           // 14
	{ "mpr-16861.ic29",				0x200000, 0xd79c3f73, 2 | BRF_GRA },           // 15
	{ "mpr-16862.ic27",				0x200000, 0x00793354, 2 | BRF_GRA },           // 16
	{ "mpr-16863.ic25",				0x200000, 0x42338226, 2 | BRF_GRA },           // 17
};

STD_ROM_PICK(svfo)
STD_ROM_FN(svfo)

struct BurnDriver BurnDrvSvfo = {
	"svfo", "svf", NULL, NULL, "1994",
	"Super Visual Football: European Sega Cup\0", NULL, "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SYSTEM32, GBF_SPORTSFOOTBALL, 0,
	NULL, svfoRomInfo, svfoRomName, NULL, NULL, NULL, NULL, DbzvrvsInputInfo, DbzvrvsDIPInfo,
	SvfInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};


// Super Visual Soccer: Sega Cup (US, Rev A)

static struct BurnRomInfo svsRomDesc[] = {
	{ "epr-16883a.ic17",			0x020000, 0xe1c0c3ce, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "epr-16882a.ic8",				0x020000, 0x1161bbbe, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-16865.ic18",				0x080000, 0x9198ca9f, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-16864.ic9",				0x080000, 0x201a940e, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-16868.ic36",				0x040000, 0x47aa4ec7, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #0 Code
	{ "mpr-16779.ic35",				0x100000, 0x7055e859, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "mpr-16778.ic34",				0x100000, 0xfeedaecf, 3 | BRF_PRG | BRF_ESS }, //  6
	{ "mpr-16777.ic24",				0x100000, 0x14b5d5df, 3 | BRF_PRG | BRF_ESS }, //  7

	{ "mpr-16784.ic14",				0x100000, 0x4608efe2, 1 | BRF_GRA },           //  8 Main Layer Tiles
	{ "mpr-16783.ic5",				0x100000, 0x042eabe7, 1 | BRF_GRA },           //  9

	{ "mpr-16785.ic32",				0x200000, 0x51f775ce, 2 | BRF_GRA },           // 10 Main Sprites
	{ "mpr-16787.ic30",				0x200000, 0xdee7a204, 2 | BRF_GRA },           // 11
	{ "mpr-16789.ic28",				0x200000, 0x6b6c8ad3, 2 | BRF_GRA },           // 12
	{ "mpr-16791.ic26",				0x200000, 0x4f7236da, 2 | BRF_GRA },           // 13
	{ "mpr-16860.ic31",				0x200000, 0x578a7325, 2 | BRF_GRA },           // 14
	{ "mpr-16861.ic29",				0x200000, 0xd79c3f73, 2 | BRF_GRA },           // 15
	{ "mpr-16862.ic27",				0x200000, 0x00793354, 2 | BRF_GRA },           // 16
	{ "mpr-16863.ic25",				0x200000, 0x42338226, 2 | BRF_GRA },           // 17
};

STD_ROM_PICK(svs)
STD_ROM_FN(svs)

struct BurnDriver BurnDrvSvs = {
	"svs", "svf", NULL, NULL, "1994",
	"Super Visual Soccer: Sega Cup (US, Rev A)\0", NULL, "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SYSTEM32, GBF_SPORTSFOOTBALL, 0,
	NULL, svsRomInfo, svsRomName, NULL, NULL, NULL, NULL, DbzvrvsInputInfo, DbzvrvsDIPInfo,
	SvfInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};


// The J.League 1994 (Japan, Rev A)

static struct BurnRomInfo jleagueRomDesc[] = {
	{ "epr-16782a.ic17",			0x020000, 0xb399ac47, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "epr-16781a.ic8",				0x020000, 0xe6d80225, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-16776.ic18",				0x080000, 0xe8694626, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-16775.ic9",				0x080000, 0xe81e2c3d, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-16780.ic36",				0x040000, 0x47aa4ec7, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #0 Code
	{ "mpr-16779.ic35",				0x100000, 0x7055e859, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "mpr-16778.ic34",				0x100000, 0xfeedaecf, 3 | BRF_PRG | BRF_ESS }, //  6
	{ "mpr-16777.ic24",				0x100000, 0x14b5d5df, 3 | BRF_PRG | BRF_ESS }, //  7

	{ "mpr-16784.ic14",				0x100000, 0x4608efe2, 1 | BRF_GRA },           //  8 Main Layer Tiles
	{ "mpr-16783.ic5",				0x100000, 0x042eabe7, 1 | BRF_GRA },           //  9

	{ "mpr-16785.ic32",				0x200000, 0x51f775ce, 2 | BRF_GRA },           // 10 Main Sprites
	{ "mpr-16787.ic30",				0x200000, 0xdee7a204, 2 | BRF_GRA },           // 11
	{ "mpr-16789.ic28",				0x200000, 0x6b6c8ad3, 2 | BRF_GRA },           // 12
	{ "mpr-16791.ic26",				0x200000, 0x4f7236da, 2 | BRF_GRA },           // 13
	{ "mpr-16786.ic31",				0x200000, 0xd08a2d32, 2 | BRF_GRA },           // 14
	{ "mpr-16788.ic29",				0x200000, 0xcd9d3605, 2 | BRF_GRA },           // 15
	{ "mpr-16790.ic27",				0x200000, 0x2ea75746, 2 | BRF_GRA },           // 16
	{ "mpr-16792.ic25",				0x200000, 0x9f416072, 2 | BRF_GRA },           // 17
};

STD_ROM_PICK(jleague)
STD_ROM_FN(jleague)

static void jleague_protection_write(UINT32 offset, UINT32 data, UINT32 mem_mask)
{
	UINT16 *ram = (UINT16*)DrvV60RAM;

	ram[offset] = BURN_ENDIAN_SWAP_INT16((BURN_ENDIAN_SWAP_INT16(ram[offset]) & ~mem_mask) | (data & mem_mask));

	switch (offset)
	{
		case 0xf700/2:
			v60WriteByte(0x20f708, v60ReadWord(0x7bbc0 + data*2));
		break;

		case 0xf704/2:
			v60WriteByte(0x200016, data & 0xff);
		break;
	}
}

static INT32 jleagueInit()
{
	sprite_length = 0;
	DrvLoadRoms(false);
	BurnAllocMemIndex();
	if (DrvLoadRoms(true)) return 1;

	system32_v60_map();
	system32_sound_init();
	tilemap_configure_allocate();

	v60Open(0);
	v60MapMemory(NULL,	0x20f700 & ~0x7ff, 0x20f705 | 0x7ff, MAP_WRITE);
	v60Close();

	protection_a00000_write = jleague_protection_write;

	custom_io_write_0 = analog_custom_io_write;
	custom_io_read_0 = analog_custom_io_read;

	clr_opposites = 2;

	DrvDoReset();

	return 0;
}

struct BurnDriver BurnDrvJleague = {
	"jleague", "svf", NULL, NULL, "1994",
	"The J.League 1994 (Japan, Rev A)\0", NULL, "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SYSTEM32, GBF_SPORTSFOOTBALL, 0,
	NULL, jleagueRomInfo, jleagueRomName, NULL, NULL, NULL, NULL, DbzvrvsInputInfo, DbzvrvsDIPInfo,
	jleagueInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};


// The J.League 1994 (Japan)

static struct BurnRomInfo jleagueoRomDesc[] = {
	{ "epr-16782.ic17",				0x020000, 0xf0278944, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "epr-16781.ic8",				0x020000, 0x7df9529b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-16776.ic18",				0x080000, 0xe8694626, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-16775.ic9",				0x080000, 0xe81e2c3d, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-16780.ic36",				0x040000, 0x47aa4ec7, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #0 Code
	{ "mpr-16779.ic35",				0x100000, 0x7055e859, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "mpr-16778.ic34",				0x100000, 0xfeedaecf, 3 | BRF_PRG | BRF_ESS }, //  6
	{ "mpr-16777.ic24",				0x100000, 0x14b5d5df, 3 | BRF_PRG | BRF_ESS }, //  7

	{ "mpr-16784.ic14",				0x100000, 0x4608efe2, 1 | BRF_GRA },           //  8 Main Layer Tiles
	{ "mpr-16783.ic5",				0x100000, 0x042eabe7, 1 | BRF_GRA },           //  9

	{ "mpr-16785.ic32",				0x200000, 0x51f775ce, 2 | BRF_GRA },           // 10 Main Sprites
	{ "mpr-16787.ic30",				0x200000, 0xdee7a204, 2 | BRF_GRA },           // 11
	{ "mpr-16789.ic28",				0x200000, 0x6b6c8ad3, 2 | BRF_GRA },           // 12
	{ "mpr-16791.ic26",				0x200000, 0x4f7236da, 2 | BRF_GRA },           // 13
	{ "mpr-16786.ic31",				0x200000, 0xd08a2d32, 2 | BRF_GRA },           // 14
	{ "mpr-16788.ic29",				0x200000, 0xcd9d3605, 2 | BRF_GRA },           // 15
	{ "mpr-16790.ic27",				0x200000, 0x2ea75746, 2 | BRF_GRA },           // 16
	{ "mpr-16792.ic25",				0x200000, 0x9f416072, 2 | BRF_GRA },           // 17
};

STD_ROM_PICK(jleagueo)
STD_ROM_FN(jleagueo)

struct BurnDriver BurnDrvJleagueo = {
	"jleagueo", "svf", NULL, NULL, "1994",
	"The J.League 1994 (Japan)\0", NULL, "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SYSTEM32, GBF_MISC, 0,
	NULL, jleagueoRomInfo, jleagueoRomName, NULL, NULL, NULL, NULL, DbzvrvsInputInfo, DbzvrvsDIPInfo,
	jleagueInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};


// Soreike Kokology (Rev A)

static struct BurnRomInfo kokorojRomDesc[] = {
	{ "epr-15524a.ic8",				0x020000, 0x3b4cc0cf, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "epr-15521a.ic18",			0x080000, 0x24c02cf9, 2 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-15520a.ic9",				0x080000, 0xbfbae875, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "epr-15523.ic36",				0x020000, 0xb3b9d29c, 3 | BRF_PRG | BRF_ESS }, //  3 Z80 #0 Code
	{ "mpr-15522.ic35",				0x080000, 0xfb68a351, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "mpr-15526.ic14",				0x100000, 0xf6907c13, 1 | BRF_GRA },           //  5 Main Layer Tiles
	{ "mpr-15525.ic5",				0x100000, 0x8c0c876f, 1 | BRF_GRA },           //  6

	{ "mpr-15527.ic32",				0x200000, 0x132f91c6, 2 | BRF_GRA },           //  7 Main Sprites
	{ "mpr-15529.ic30",				0x200000, 0xc1b826f7, 2 | BRF_GRA },           //  8
	{ "mpr-15531.ic28",				0x200000, 0xd624e05f, 2 | BRF_GRA },           //  9
	{ "mpr-15533.ic26",				0x200000, 0xaff0e9a8, 2 | BRF_GRA },           // 10
	{ "mpr-15528.ic31",				0x200000, 0x2e4bc090, 2 | BRF_GRA },           // 11
	{ "mpr-15530.ic29",				0x200000, 0x307877a8, 2 | BRF_GRA },           // 12
	{ "mpr-15532.ic27",				0x200000, 0x923ba3e5, 2 | BRF_GRA },           // 13
	{ "mpr-15534.ic25",				0x200000, 0x4fa5c56d, 2 | BRF_GRA },           // 14
};

STD_ROM_PICK(kokoroj)
STD_ROM_FN(kokoroj)

static INT32 KokorojInit()
{
	sprite_length = 0;
	DrvLoadRoms(false);
	BurnAllocMemIndex();
	if (DrvLoadRoms(true)) return 1;

	system32_v60_map();
	system32_sound_init();
	tilemap_configure_allocate();

	custom_io_write_0 = analog_custom_io_write;
	custom_io_read_0 = analog_custom_io_read;

	DrvDoReset();

	return 0;
}

struct BurnDriver BurnDrvKokoroj = {
	"kokoroj", NULL, NULL, NULL, "1992",
	"Soreike Kokology (Rev A)\0", NULL, "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_SEGA_SYSTEM32, GBF_MISC, 0,
	NULL, kokorojRomInfo, kokorojRomName, NULL, NULL, NULL, NULL, Kokoroj2InputInfo, Kokoroj2DIPInfo,
	KokorojInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	416, 224, 4, 3
};


// Soreike Kokology

static struct BurnRomInfo kokorojaRomDesc[] = {
	{ "epr-15524.ic8",				0x020000, 0x135640f6, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "epr-15521.ic18",				0x080000, 0xb0a80786, 2 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-15520.ic9",				0x080000, 0xf2a87e48, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "epr-15523.ic36",				0x020000, 0xba852239, 3 | BRF_PRG | BRF_ESS }, //  3 Z80 #0 Code
	{ "mpr-15522.ic35",				0x080000, 0xfb68a351, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "mpr-15526.ic14",				0x100000, 0xf6907c13, 1 | BRF_GRA },           //  5 Main Layer Tiles
	{ "mpr-15525.ic5",				0x100000, 0x8c0c876f, 1 | BRF_GRA },           //  6

	{ "mpr-15527.ic32",				0x200000, 0x132f91c6, 2 | BRF_GRA },           //  7 Main Sprites
	{ "mpr-15529.ic30",				0x200000, 0xc1b826f7, 2 | BRF_GRA },           //  8
	{ "mpr-15531.ic28",				0x200000, 0xd624e05f, 2 | BRF_GRA },           //  9
	{ "mpr-15533.ic26",				0x200000, 0xaff0e9a8, 2 | BRF_GRA },           // 10
	{ "mpr-15528.ic31",				0x200000, 0x2e4bc090, 2 | BRF_GRA },           // 11
	{ "mpr-15530.ic29",				0x200000, 0x307877a8, 2 | BRF_GRA },           // 12
	{ "mpr-15532.ic27",				0x200000, 0x923ba3e5, 2 | BRF_GRA },           // 13
	{ "mpr-15534.ic25",				0x200000, 0x4fa5c56d, 2 | BRF_GRA },           // 14
};

STD_ROM_PICK(kokoroja)
STD_ROM_FN(kokoroja)

struct BurnDriver BurnDrvKokoroja = {
	"kokoroja", "kokoroj", NULL, NULL, "1992",
	"Soreike Kokology\0", NULL, "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_SEGA_SYSTEM32, GBF_MISC, 0,
	NULL, kokorojaRomInfo, kokorojaRomName, NULL, NULL, NULL, NULL, Kokoroj2InputInfo, Kokoroj2DIPInfo,
	KokorojInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	416, 224, 4, 3
};


// Soreike Kokology Vol. 2 - Kokoro no Tanteikyoku

static struct BurnRomInfo kokoroj2RomDesc[] = {
	{ "epr-16186.ic8",				0x020000, 0x8c3afb6e, 1 | BRF_PRG | BRF_ESS }, //  0 V60 #0 Code
	{ "epr-16183.ic18",				0x080000, 0x4844432f, 2 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-16182.ic9",				0x080000, 0xa27f5f5f, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "epr-16185.ic36",				0x020000, 0xafb97c4d, 3 | BRF_PRG | BRF_ESS }, //  3 Z80 #0 Code
	{ "mpr-16184.ic35",				0x100000, 0xd7a19751, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "mpr-16188.ic14",				0x200000, 0x83a450ab, 1 | BRF_GRA },           //  5 Main Layer Tiles
	{ "mpr-16187.ic5",				0x200000, 0x98b62f8b, 1 | BRF_GRA },           //  6

	{ "mpr-16189.ic32",				0x200000, 0x0937f713, 2 | BRF_GRA },           //  7 Main Sprites
	{ "mpr-16191.ic30",				0x200000, 0xcfef4aaa, 2 | BRF_GRA },           //  8
	{ "mpr-16193.ic28",				0x200000, 0xa0706e4e, 2 | BRF_GRA },           //  9
	{ "mpr-16195.ic26",				0x200000, 0xa4ddcd61, 2 | BRF_GRA },           // 10
	{ "mpr-16190.ic31",				0x200000, 0x528d408e, 2 | BRF_GRA },           // 11
	{ "mpr-16192.ic29",				0x200000, 0xefaa93d1, 2 | BRF_GRA },           // 12
	{ "mpr-16194.ic27",				0x200000, 0x39b5efe7, 2 | BRF_GRA },           // 13
	{ "mpr-16196.ic25",				0x200000, 0xb8e22e05, 2 | BRF_GRA },           // 14
};

STD_ROM_PICK(kokoroj2)
STD_ROM_FN(kokoroj2)

struct BurnDriver BurnDrvKokoroj2 = {
	"kokoroj2", NULL, NULL, NULL, "1993",
	"Soreike Kokology Vol. 2 - Kokoro no Tanteikyoku\0", NULL, "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_SEGA_SYSTEM32, GBF_SPORTSMISC, 0,
	NULL, kokoroj2RomInfo, kokoroj2RomName, NULL, NULL, NULL, NULL, Kokoroj2InputInfo, Kokoroj2DIPInfo,
	KokorojInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	416, 224, 4, 3
};


// Hard Dunk (World)

static struct BurnRomInfo harddunkRomDesc[] = {
	{ "epr-16512.ic37",				0x040000, 0x1a7de085, 5 | BRF_PRG | BRF_ESS }, //  0 V70 #0 Code
	{ "epr-16513.ic40",				0x040000, 0x603dee75, 5 | BRF_PRG | BRF_ESS }, //  1

	{ "epr-16505.ic31",				0x020000, 0xeeb90a07, 3 | BRF_PRG | BRF_ESS }, //  2 Z80 #0 Code

	{ "mpr-16503.ic3",				0x080000, 0xac1b6f1a, 1 | BRF_GRA },           //  3 Main Layer Tiles
	{ "mpr-16504.ic11",				0x080000, 0x7c61fcd8, 1 | BRF_GRA },           //  4

	{ "mpr-16495.ic14",				0x200000, 0x6e5f26be, 2 | BRF_GRA },           //  5 Main Sprites
	{ "mpr-16497.ic15",				0x200000, 0x42ab5859, 2 | BRF_GRA },           //  6
	{ "mpr-16499.ic10",				0x200000, 0xa290ea36, 2 | BRF_GRA },           //  7
	{ "mpr-16501.ic38",				0x200000, 0xf1566620, 2 | BRF_GRA },           //  8
	{ "mpr-16496.ic22",				0x200000, 0xd9d27247, 2 | BRF_GRA },           //  9
	{ "mpr-16498.ic23",				0x200000, 0xc022a991, 2 | BRF_GRA },           // 10
	{ "mpr-16500.ic18",				0x200000, 0x452c0be3, 2 | BRF_GRA },           // 11
	{ "mpr-16502.ic41",				0x200000, 0xffc3147e, 2 | BRF_GRA },           // 12

	{ "mpr-16506.1",				0x200000, 0xe779f5ed, 1 | BRF_SND },           // 13 PCM Samples
	{ "mpr-16507.2",				0x200000, 0x31e068d3, 1 | BRF_SND },           // 14
};

STD_ROM_PICK(harddunk)
STD_ROM_FN(harddunk)

static INT32 HarddunkInit()
{
	is_multi32 = 1;

	sprite_length = 0;
	DrvLoadRoms(false);
	BurnAllocMemIndex();
	if (DrvLoadRoms(true)) return 1;

	system32_v70_map();
	multi32_sound_init();
	tilemap_configure_allocate();

	custom_io_read_0 = extra_custom_io_read;

	clr_opposites = 6;

	DrvDoReset();

	return 0;
}

struct BurnDriver BurnDrvHarddunk = {
	"harddunk", NULL, NULL, NULL, "1994",
	"Hard Dunk (World)\0", NULL, "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 6, HARDWARE_SEGA_SYSTEM32, GBF_SPORTSMISC, 0,
	NULL, harddunkRomInfo, harddunkRomName, NULL, NULL, NULL, NULL, HarddunkInputInfo, HarddunkDIPInfo,
	HarddunkInit, DrvExit, DrvFrame, MultiDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 224, 8, 3
};


// Hard Dunk (Japan)

static struct BurnRomInfo harddunkjRomDesc[] = {
	{ "epr-16508.ic37",				0x040000, 0xb3713be5, 5 | BRF_PRG | BRF_ESS }, //  0 V70 #0 Code
	{ "epr-16509.ic40",				0x040000, 0x603dee75, 5 | BRF_PRG | BRF_ESS }, //  1

	{ "epr-16505.ic31",				0x020000, 0xeeb90a07, 3 | BRF_PRG | BRF_ESS }, //  2 Z80 #0 Code

	{ "mpr-16503.ic3",				0x080000, 0xac1b6f1a, 1 | BRF_GRA },           //  3 Main Layer Tiles
	{ "mpr-16504.ic11",				0x080000, 0x7c61fcd8, 1 | BRF_GRA },           //  4

	{ "mpr-16495.ic14",				0x200000, 0x6e5f26be, 2 | BRF_GRA },           //  5 Main Sprites
	{ "mpr-16497.ic15",				0x200000, 0x42ab5859, 2 | BRF_GRA },           //  6
	{ "mpr-16499.ic10",				0x200000, 0xa290ea36, 2 | BRF_GRA },           //  7
	{ "mpr-16501.ic38",				0x200000, 0xf1566620, 2 | BRF_GRA },           //  8
	{ "mpr-16496.ic22",				0x200000, 0xd9d27247, 2 | BRF_GRA },           //  9
	{ "mpr-16498.ic23",				0x200000, 0xc022a991, 2 | BRF_GRA },           // 10
	{ "mpr-16500.ic18",				0x200000, 0x452c0be3, 2 | BRF_GRA },           // 11
	{ "mpr-16502.ic41",				0x200000, 0xffc3147e, 2 | BRF_GRA },           // 12

	{ "mpr-16506.ic1",				0x200000, 0xe779f5ed, 1 | BRF_SND },           // 13 PCM Samples
	{ "mpr-16507.ic2",				0x200000, 0x31e068d3, 1 | BRF_SND },           // 14
};

STD_ROM_PICK(harddunkj)
STD_ROM_FN(harddunkj)

struct BurnDriver BurnDrvHarddunkj = {
	"harddunkj", "harddunk", NULL, NULL, "1994",
	"Hard Dunk (Japan)\0", NULL, "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 6, HARDWARE_SEGA_SYSTEM32, GBF_SPORTSMISC, 0,
	NULL, harddunkjRomInfo, harddunkjRomName, NULL, NULL, NULL, NULL, HarddunkInputInfo, HarddunkDIPInfo,
	HarddunkInit, DrvExit, DrvFrame, MultiDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 224, 8, 3
};


// OutRunners (World)

static struct BurnRomInfo orunnersRomDesc[] = {
	{ "epr-15620.ic37",				0x020000, 0x84f5ad92, 5 | BRF_PRG | BRF_ESS }, //  0 V70 #0 Code
	{ "epr-15621.ic40",				0x020000, 0xd98b765a, 5 | BRF_PRG | BRF_ESS }, //  1
	{ "mpr-15538.ic36",				0x080000, 0x93958820, 5 | BRF_PRG | BRF_ESS }, //  2
	{ "mpr-15539.ic39",				0x080000, 0x219760fa, 5 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-15550.ic31",				0x080000, 0x0205d2ed, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #0 Code

	{ "mpr-15548.ic3",				0x200000, 0xb6470a66, 1 | BRF_GRA },           //  5 Main Layer Tiles
	{ "mpr-15549.ic11",				0x200000, 0x81d12520, 1 | BRF_GRA },           //  6

	{ "mpr-15540.ic14",				0x200000, 0xa10d72b4, 2 | BRF_GRA },           //  7 Main Sprites
	{ "mpr-15542.ic15",				0x200000, 0x40952374, 2 | BRF_GRA },           //  8
	{ "mpr-15544.ic10",				0x200000, 0x39e3df45, 2 | BRF_GRA },           //  9
	{ "mpr-15546.ic38",				0x200000, 0xe3fcc12c, 2 | BRF_GRA },           // 10
	{ "mpr-15541.ic22",				0x200000, 0xa2003c2d, 2 | BRF_GRA },           // 11
	{ "mpr-15543.ic23",				0x200000, 0x933e8e7b, 2 | BRF_GRA },           // 12
	{ "mpr-15545.ic18",				0x200000, 0x53dd0235, 2 | BRF_GRA },           // 13
	{ "mpr-15547.ic41",				0x200000, 0xedcb2a43, 2 | BRF_GRA },           // 14

	{ "mpr-15551.ic1",				0x200000, 0x4894bc73, 1 | BRF_SND },           // 15 PCM Samples
	{ "mpr-15552.ic2",				0x200000, 0x1c4b5e73, 1 | BRF_SND },           // 16
};

STD_ROM_PICK(orunners)
STD_ROM_FN(orunners)

static void orunners_custom_io_write(UINT32 offset, UINT16 data, UINT16)
{
	switch (offset)
	{
		case 0x10/2:
		case 0x12/2:
		case 0x14/2:
		case 0x16/2: {
			switch (analog_bank * 4 + (offset & 3)) {
				case 0: analog_value[offset & 3] = ProcessAnalog(Analog[0], 0, INPUT_DEADZONE, 0x00, 0xff); break;
				case 1: analog_value[offset & 3] = ProcessAnalog(Analog[1], 0, INPUT_DEADZONE | INPUT_LINEAR | INPUT_MIGHTBEDIGITAL, 0x00, 0xff); break;
				case 2: analog_value[offset & 3] = ProcessAnalog(Analog[2], 0, INPUT_DEADZONE | INPUT_LINEAR | INPUT_MIGHTBEDIGITAL, 0x00, 0xff); break;
				case 3: analog_value[offset & 3] = ProcessAnalog(Analog[3], 0, INPUT_DEADZONE, 0x00, 0xff); break;
				case 6: analog_value[offset & 3] = ProcessAnalog(Analog[4], 0, INPUT_DEADZONE | INPUT_LINEAR | INPUT_MIGHTBEDIGITAL, 0x00, 0xff); break;
				case 7: analog_value[offset & 3] = ProcessAnalog(Analog[5], 0, INPUT_DEADZONE | INPUT_LINEAR | INPUT_MIGHTBEDIGITAL, 0x00, 0xff); break;
			}
			//analog_value[offset & 3] = 0; //input_port_read_safe(space->machine, names[analog_bank * 4 + (offset & 3)], 0);
			return;
		}

		case 0x20/2:
			analog_bank = data & 1;
			return;
	}
}

static INT32 OrunnersInit()
{
	is_multi32 = 1;

	sprite_length = 0;
	DrvLoadRoms(false);
	BurnAllocMemIndex();
	if (DrvLoadRoms(true)) return 1;

	system32_v70_map();
	multi32_sound_init();
	tilemap_configure_allocate();

	custom_io_write_0  = orunners_custom_io_write;
	custom_io_read_0 = analog_custom_io_read;

	DrvDoReset();

	return 0;
}

struct BurnDriver BurnDrvOrunners = {
	"orunners", NULL, NULL, NULL, "1992",
	"OutRunners (World)\0", NULL, "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM32, GBF_RACING, 0,
	NULL, orunnersRomInfo, orunnersRomName, NULL, NULL, NULL, NULL, OrunnersInputInfo, OrunnersDIPInfo,
	OrunnersInit, DrvExit, DrvFrame, MultiDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 224, 8, 3
};


// OutRunners (US)

static struct BurnRomInfo orunnersuRomDesc[] = {
	{ "epr-15618.ic37",				0x020000, 0x25647f76, 5 | BRF_PRG | BRF_ESS }, //  0 V70 #0 Code
	{ "epr-15619.ic40",				0x020000, 0x2a558f95, 5 | BRF_PRG | BRF_ESS }, //  1
	{ "mpr-15538.ic36",				0x080000, 0x93958820, 5 | BRF_PRG | BRF_ESS }, //  2
	{ "mpr-15539.ic39",				0x080000, 0x219760fa, 5 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-15550.ic31",				0x080000, 0x0205d2ed, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #0 Code

	{ "mpr-15548.ic3",				0x200000, 0xb6470a66, 1 | BRF_GRA },           //  5 Main Layer Tiles
	{ "mpr-15549.ic11",				0x200000, 0x81d12520, 1 | BRF_GRA },           //  6

	{ "mpr-15540.ic14",				0x200000, 0xa10d72b4, 2 | BRF_GRA },           //  7 Main Sprites
	{ "mpr-15542.ic15",				0x200000, 0x40952374, 2 | BRF_GRA },           //  8
	{ "mpr-15544.ic10",				0x200000, 0x39e3df45, 2 | BRF_GRA },           //  9
	{ "mpr-15546.ic38",				0x200000, 0xe3fcc12c, 2 | BRF_GRA },           // 10
	{ "mpr-15541.ic22",				0x200000, 0xa2003c2d, 2 | BRF_GRA },           // 11
	{ "mpr-15543.ic23",				0x200000, 0x933e8e7b, 2 | BRF_GRA },           // 12
	{ "mpr-15545.ic18",				0x200000, 0x53dd0235, 2 | BRF_GRA },           // 13
	{ "mpr-15547.ic41",				0x200000, 0xedcb2a43, 2 | BRF_GRA },           // 14

	{ "mpr-15551.ic1",				0x200000, 0x4894bc73, 1 | BRF_SND },           // 15 PCM Samples
	{ "mpr-15552.ic2",				0x200000, 0x1c4b5e73, 1 | BRF_SND },           // 16
};

STD_ROM_PICK(orunnersu)
STD_ROM_FN(orunnersu)

struct BurnDriver BurnDrvOrunnersu = {
	"orunnersu", "orunners", NULL, NULL, "1992",
	"OutRunners (US)\0", NULL, "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM32, GBF_RACING, 0,
	NULL, orunnersuRomInfo, orunnersuRomName, NULL, NULL, NULL, NULL, OrunnersInputInfo, OrunnersDIPInfo,
	OrunnersInit, DrvExit, DrvFrame, MultiDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 224, 8, 3
};


// OutRunners (Japan)

static struct BurnRomInfo orunnersjRomDesc[] = {
	{ "epr-15616.ic37",				0x020000, 0xfb550545, 5 | BRF_PRG | BRF_ESS }, //  0 V70 #0 Code
	{ "epr-15617.ic40",				0x020000, 0x6bb741e0, 5 | BRF_PRG | BRF_ESS }, //  1
	{ "mpr-15538.ic36",				0x080000, 0x93958820, 5 | BRF_PRG | BRF_ESS }, //  2
	{ "mpr-15539.ic39",				0x080000, 0x219760fa, 5 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-15550.ic31",				0x080000, 0x0205d2ed, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #0 Code

	{ "mpr-15548.ic3",				0x200000, 0xb6470a66, 1 | BRF_GRA },           //  5 Main Layer Tiles
	{ "mpr-15549.ic11",				0x200000, 0x81d12520, 1 | BRF_GRA },           //  6

	{ "mpr-15540.ic14",				0x200000, 0xa10d72b4, 2 | BRF_GRA },           //  7 Main Sprites
	{ "mpr-15542.ic15",				0x200000, 0x40952374, 2 | BRF_GRA },           //  8
	{ "mpr-15544.ic10",				0x200000, 0x39e3df45, 2 | BRF_GRA },           //  9
	{ "mpr-15546.ic38",				0x200000, 0xe3fcc12c, 2 | BRF_GRA },           // 10
	{ "mpr-15541.ic22",				0x200000, 0xa2003c2d, 2 | BRF_GRA },           // 11
	{ "mpr-15543.ic23",				0x200000, 0x933e8e7b, 2 | BRF_GRA },           // 12
	{ "mpr-15545.ic18",				0x200000, 0x53dd0235, 2 | BRF_GRA },           // 13
	{ "mpr-15547.ic41",				0x200000, 0xedcb2a43, 2 | BRF_GRA },           // 14

	{ "mpr-15551.ic1",				0x200000, 0x4894bc73, 1 | BRF_SND },           // 15 PCM Samples
	{ "mpr-15552.ic2",				0x200000, 0x1c4b5e73, 1 | BRF_SND },           // 16
};

STD_ROM_PICK(orunnersj)
STD_ROM_FN(orunnersj)

struct BurnDriver BurnDrvOrunnersj = {
	"orunnersj", "orunners", NULL, NULL, "1992",
	"OutRunners (Japan)\0", NULL, "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM32, GBF_RACING, 0,
	NULL, orunnersjRomInfo, orunnersjRomName, NULL, NULL, NULL, NULL, OrunnersInputInfo, OrunnersDIPInfo,
	OrunnersInit, DrvExit, DrvFrame, MultiDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 224, 8, 3
};


// Stadium Cross (World)

static struct BurnRomInfo scrossRomDesc[] = {
	{ "epr-15093.ic37",				0x040000, 0x2adc7a4b, 5 | BRF_PRG | BRF_ESS }, //  0 V70 #0 Code
	{ "epr-15094.ic40",				0x040000, 0xbbb0ae73, 5 | BRF_PRG | BRF_ESS }, //  1
	{ "mpr-15018.ic36",				0x080000, 0x3a98385e, 5 | BRF_PRG | BRF_ESS }, //  2
	{ "mpr-15019.ic39",				0x080000, 0x8bf4ac83, 5 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-15192.ic31",				0x020000, 0x7524290b, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #0 Code

	{ "mpr-15020.ic3",				0x100000, 0xde47006a, 1 | BRF_GRA },           //  5 Main Layer Tiles
	{ "mpr-15021.ic11",				0x100000, 0x3677db02, 1 | BRF_GRA },           //  6

	{ "mpr-15022.ic14",				0x100000, 0xbaee6fd5, 2 | BRF_GRA },           //  7 Main Sprites
	{ "mpr-15024.ic15",				0x100000, 0xb9f339e2, 2 | BRF_GRA },           //  8
	{ "mpr-15026.ic10",				0x100000, 0xb72e8df6, 2 | BRF_GRA },           //  9
	{ "mpr-15028.ic38",				0x100000, 0x183f6eb0, 2 | BRF_GRA },           // 10
	{ "mpr-15023.ic22",				0x100000, 0x230735ed, 2 | BRF_GRA },           // 11
	{ "mpr-15025.ic23",				0x100000, 0xda4315cb, 2 | BRF_GRA },           // 12
	{ "mpr-15027.ic18",				0x100000, 0xb765efb8, 2 | BRF_GRA },           // 13
	{ "mpr-15029.ic41",				0x100000, 0xcf8e3b2b, 2 | BRF_GRA },           // 14

	{ "mpr-15031.ic1",				0x100000, 0x6af139dc, 1 | BRF_SND },           // 15 PCM Samples
	{ "mpr-15032.ic2",				0x100000, 0x915d6096, 1 | BRF_SND },           // 16

	{ "epr-15033.ic17",				0x020000, 0xdc19ac00, 0 | BRF_PRG | BRF_ESS }, // 17 Comms Board ROM
};

STD_ROM_PICK(scross)
STD_ROM_FN(scross)

static INT32 ScrossInit()
{
	is_multi32 = 1;
	is_scross = 1;

	sprite_length = 0;
	DrvLoadRoms(false);
	BurnAllocMemIndex();
	if (DrvLoadRoms(true)) return 1;

	system32_v70_map();
	multi32_sound_init();
	tilemap_configure_allocate();

	custom_io_write_0  = scross_custom_io_write;
	custom_io_read_0 = analog_custom_io_read;

	DrvDoReset();

	return 0;
}

struct BurnDriver BurnDrvScross = {
	"scross", NULL, NULL, NULL, "1992",
	"Stadium Cross (World)\0", NULL, "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM32, GBF_RACING, 0,
	NULL, scrossRomInfo, scrossRomName, NULL, NULL, NULL, NULL, ScrossInputInfo, ScrossDIPInfo,
	ScrossInit, DrvExit, DrvFrame, MultiDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 224, 8, 3
};


// Stadium Cross (World, linkable)

static struct BurnRomInfo scrossaRomDesc[] = {
	{ "ic37",						0x040000, 0x240a7655, 5 | BRF_PRG | BRF_ESS }, //  0 V70 #0 Code
	{ "ic40",						0x040000, 0x3a073060, 5 | BRF_PRG | BRF_ESS }, //  1
	{ "mpr-15018.ic36",				0x080000, 0x3a98385e, 5 | BRF_PRG | BRF_ESS }, //  2
	{ "mpr-15019.ic39",				0x080000, 0x8bf4ac83, 5 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-15192.ic31",				0x020000, 0x7524290b, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #0 Code

	{ "mpr-15020.ic3",				0x100000, 0xde47006a, 1 | BRF_GRA },           //  5 Main Layer Tiles
	{ "mpr-15021.ic11",				0x100000, 0x3677db02, 1 | BRF_GRA },           //  6

	{ "mpr-15022.ic14",				0x100000, 0xbaee6fd5, 2 | BRF_GRA },           //  7 Main Sprites
	{ "mpr-15024.ic15",				0x100000, 0xb9f339e2, 2 | BRF_GRA },           //  8
	{ "mpr-15026.ic10",				0x100000, 0xb72e8df6, 2 | BRF_GRA },           //  9
	{ "mpr-15028.ic38",				0x100000, 0x183f6eb0, 2 | BRF_GRA },           // 10
	{ "mpr-15023.ic22",				0x100000, 0x230735ed, 2 | BRF_GRA },           // 11
	{ "mpr-15025.ic23",				0x100000, 0xda4315cb, 2 | BRF_GRA },           // 12
	{ "mpr-15027.ic18",				0x100000, 0xb765efb8, 2 | BRF_GRA },           // 13
	{ "mpr-15029.ic41",				0x100000, 0xcf8e3b2b, 2 | BRF_GRA },           // 14

	{ "mpr-15031.ic1",				0x100000, 0x6af139dc, 1 | BRF_SND },           // 15 PCM Samples
	{ "mpr-15032.ic2",				0x100000, 0x915d6096, 1 | BRF_SND },           // 16

	{ "epr-15033.ic17",				0x020000, 0xdc19ac00, 0 | BRF_PRG | BRF_ESS }, // 17 Comms Board ROM
};

STD_ROM_PICK(scrossa)
STD_ROM_FN(scrossa)

struct BurnDriver BurnDrvScrossa = {
	"scrossa", "scross", NULL, NULL, "1992",
	"Stadium Cross (World, linkable)\0", NULL, "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM32, GBF_RACING, 0,
	NULL, scrossaRomInfo, scrossaRomName, NULL, NULL, NULL, NULL, ScrossInputInfo, ScrossDIPInfo,
	ScrossInit, DrvExit, DrvFrame, MultiDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 224, 8, 3
};


// Stadium Cross (US)

static struct BurnRomInfo scrossuRomDesc[] = {
	{ "epr-15091.ic37",				0x040000, 0x2c572293, 5 | BRF_PRG | BRF_ESS }, //  0 V70 #0 Code
	{ "epr-15092.ic40",				0x040000, 0x6e3e175a, 5 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-15018.ic36",				0x080000, 0x3a98385e, 5 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-15019.ic39",				0x080000, 0x8bf4ac83, 5 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-15192.ic31",				0x020000, 0x7524290b, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #0 Code

	{ "mpr-15020.ic3",				0x100000, 0xde47006a, 1 | BRF_GRA },           //  5 Main Layer Tiles
	{ "mpr-15021.ic11",				0x100000, 0x3677db02, 1 | BRF_GRA },           //  6

	{ "mpr-15022.ic14",				0x100000, 0xbaee6fd5, 2 | BRF_GRA },           //  7 Main Sprites
	{ "mpr-15024.ic15",				0x100000, 0xb9f339e2, 2 | BRF_GRA },           //  8
	{ "mpr-15026.ic10",				0x100000, 0xb72e8df6, 2 | BRF_GRA },           //  9
	{ "mpr-15028.ic38",				0x100000, 0x183f6eb0, 2 | BRF_GRA },           // 10
	{ "mpr-15023.ic22",				0x100000, 0x230735ed, 2 | BRF_GRA },           // 11
	{ "mpr-15025.ic23",				0x100000, 0xda4315cb, 2 | BRF_GRA },           // 12
	{ "mpr-15027.ic18",				0x100000, 0xb765efb8, 2 | BRF_GRA },           // 13
	{ "mpr-15029.ic41",				0x100000, 0xcf8e3b2b, 2 | BRF_GRA },           // 14

	{ "mpr-15031.ic1",				0x100000, 0x6af139dc, 1 | BRF_SND },           // 15 PCM Samples
	{ "mpr-15032.ic2",				0x100000, 0x915d6096, 1 | BRF_SND },           // 16

	{ "epr-15033.ic17",				0x020000, 0xdc19ac00, 0 | BRF_PRG | BRF_ESS }, // 17 Comms Board ROM
};

STD_ROM_PICK(scrossu)
STD_ROM_FN(scrossu)

struct BurnDriver BurnDrvScrossu = {
	"scrossu", "scross", NULL, NULL, "1992",
	"Stadium Cross (US)\0", NULL, "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM32, GBF_RACING, 0,
	NULL, scrossuRomInfo, scrossuRomName, NULL, NULL, NULL, NULL, ScrossInputInfo, ScrossDIPInfo,
	ScrossInit, DrvExit, DrvFrame, MultiDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 224, 8, 3
};


// Title Fight (World)

static struct BurnRomInfo titlefRomDesc[] = {
	{ "epr-15388.ic37",				0x040000, 0xdb1eefbd, 5 | BRF_PRG | BRF_ESS }, //  0 V70 #0 Code
	{ "epr-15389.ic40",				0x040000, 0xda9f60a3, 5 | BRF_PRG | BRF_ESS }, //  1

	{ "epr-15384.ic31",				0x020000, 0x0f7d208d, 3 | BRF_PRG | BRF_ESS }, //  2 Z80 #0 Code

	{ "mpr-15381.ic3",				0x200000, 0x162cc4d6, 1 | BRF_GRA },           //  3 Main Layer Tiles
	{ "mpr-15382.ic11",				0x200000, 0xfd03a130, 1 | BRF_GRA },           //  4

	{ "mpr-15379.ic14",				0x200000, 0xe5c74b11, 2 | BRF_GRA },           //  5 Main Sprites
	{ "mpr-15375.ic15",				0x200000, 0x046a9b50, 2 | BRF_GRA },           //  6
	{ "mpr-15371.ic10",				0x200000, 0x999046c6, 2 | BRF_GRA },           //  7
	{ "mpr-15373.ic38",				0x200000, 0x9b3294d9, 2 | BRF_GRA },           //  8
	{ "mpr-15380.ic22",				0x200000, 0x6ea0e58d, 2 | BRF_GRA },           //  9
	{ "mpr-15376.ic23",				0x200000, 0xde3e05c5, 2 | BRF_GRA },           // 10
	{ "mpr-15372.ic18",				0x200000, 0xc187c36a, 2 | BRF_GRA },           // 11
	{ "mpr-15374.ic41",				0x200000, 0xe026aab0, 2 | BRF_GRA },           // 12

	{ "mpr-15385.ic1",				0x200000, 0x5a9b0aa0, 1 | BRF_SND },           // 13 PCM Samples
};

STD_ROM_PICK(titlef)
STD_ROM_FN(titlef)

static INT32 TitlefInit()
{
	is_multi32 = 1;

	sprite_length = 0;
	DrvLoadRoms(false);
	BurnAllocMemIndex();
	if (DrvLoadRoms(true)) return 1;

	system32_v70_map();
	multi32_sound_init();
	tilemap_configure_allocate();

	clr_opposites = 0x2f;

	DrvDoReset();

	return 0;
}

struct BurnDriver BurnDrvTitlef = {
	"titlef", NULL, NULL, NULL, "1992",
	"Title Fight (World)\0", "Background GFX Issues", "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_SEGA_SYSTEM32, GBF_VSFIGHT, 0,
	NULL, titlefRomInfo, titlefRomName, NULL, NULL, NULL, NULL, TitlefInputInfo, TitlefDIPInfo,
	TitlefInit, DrvExit, DrvFrame, MultiDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 224, 8, 3
};


// Title Fight (US)

static struct BurnRomInfo titlefuRomDesc[] = {
	{ "epr-15386.ic37",				0x040000, 0xe36e2516, 5 | BRF_PRG | BRF_ESS }, //  0 V70 #0 Code
	{ "epr-15387.ic40",				0x040000, 0xe63406d3, 5 | BRF_PRG | BRF_ESS }, //  1

	{ "epr-15384.ic31",				0x020000, 0x0f7d208d, 3 | BRF_PRG | BRF_ESS }, //  2 Z80 #0 Code

	{ "mpr-15381.ic3",				0x200000, 0x162cc4d6, 1 | BRF_GRA },           //  3 Main Layer Tiles
	{ "mpr-15382.ic11",				0x200000, 0xfd03a130, 1 | BRF_GRA },           //  4

	{ "mpr-15379.ic14",				0x200000, 0xe5c74b11, 2 | BRF_GRA },           //  5 Main Sprites
	{ "mpr-15375.ic15",				0x200000, 0x046a9b50, 2 | BRF_GRA },           //  6
	{ "mpr-15371.ic10",				0x200000, 0x999046c6, 2 | BRF_GRA },           //  7
	{ "mpr-15373.ic38",				0x200000, 0x9b3294d9, 2 | BRF_GRA },           //  8
	{ "mpr-15380.ic22",				0x200000, 0x6ea0e58d, 2 | BRF_GRA },           //  9
	{ "mpr-15376.ic23",				0x200000, 0xde3e05c5, 2 | BRF_GRA },           // 10
	{ "mpr-15372.ic18",				0x200000, 0xc187c36a, 2 | BRF_GRA },           // 11
	{ "mpr-15374.ic41",				0x200000, 0xe026aab0, 2 | BRF_GRA },           // 12

	{ "mpr-15385.ic1",				0x200000, 0x5a9b0aa0, 1 | BRF_SND },           // 13 PCM Samples
};

STD_ROM_PICK(titlefu)
STD_ROM_FN(titlefu)

struct BurnDriver BurnDrvTitlefu = {
	"titlefu", "titlef", NULL, NULL, "1992",
	"Title Fight (US)\0", "Background GFX Issues", "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_SEGA_SYSTEM32, GBF_VSFIGHT, 0,
	NULL, titlefuRomInfo, titlefuRomName, NULL, NULL, NULL, NULL, TitlefInputInfo, TitlefDIPInfo,
	TitlefInit, DrvExit, DrvFrame, MultiDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 224, 8, 3
};


// Title Fight (Japan)

static struct BurnRomInfo titlefjRomDesc[] = {
	{ "epr-15377.ic37",				0x040000, 0x1868403c, 5 | BRF_PRG | BRF_ESS }, //  0 V70 #0 Code
	{ "epr-15378.ic40",				0x040000, 0x44487b0a, 5 | BRF_PRG | BRF_ESS }, //  1

	{ "epr-15384.ic31",				0x020000, 0x0f7d208d, 3 | BRF_PRG | BRF_ESS }, //  2 Z80 #0 Code

	{ "mpr-15381.ic3",				0x200000, 0x162cc4d6, 1 | BRF_GRA },           //  3 Main Layer Tiles
	{ "mpr-15382.ic11",				0x200000, 0xfd03a130, 1 | BRF_GRA },           //  4

	{ "mpr-15379.ic14",				0x200000, 0xe5c74b11, 2 | BRF_GRA },           //  5 Main Sprites
	{ "mpr-15375.ic15",				0x200000, 0x046a9b50, 2 | BRF_GRA },           //  6
	{ "mpr-15371.ic10",				0x200000, 0x999046c6, 2 | BRF_GRA },           //  7
	{ "mpr-15373.ic38",				0x200000, 0x9b3294d9, 2 | BRF_GRA },           //  8
	{ "mpr-15380.ic22",				0x200000, 0x6ea0e58d, 2 | BRF_GRA },           //  9
	{ "mpr-15376.ic23",				0x200000, 0xde3e05c5, 2 | BRF_GRA },           // 10
	{ "mpr-15372.ic18",				0x200000, 0xc187c36a, 2 | BRF_GRA },           // 11
	{ "mpr-15374.ic41",				0x200000, 0xe026aab0, 2 | BRF_GRA },           // 12

	{ "mpr-15385.ic1",				0x200000, 0x5a9b0aa0, 1 | BRF_SND },           // 13 PCM Samples
};

STD_ROM_PICK(titlefj)
STD_ROM_FN(titlefj)

struct BurnDriver BurnDrvTitlefj = {
	"titlefj", "titlef", NULL, NULL, "1992",
	"Title Fight (Japan)\0", "Background GFX Issues", "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_SEGA_SYSTEM32, GBF_VSFIGHT, 0,
	NULL, titlefjRomInfo, titlefjRomName, NULL, NULL, NULL, NULL, TitlefInputInfo, TitlefDIPInfo,
	TitlefInit, DrvExit, DrvFrame, MultiDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 224, 8, 3
};


// AS-1 Controller

static struct BurnRomInfo as1RomDesc[] = {
	{ "epr15420.ic37",				0x020000, 0x1f9747b0, 5 | BRF_PRG | BRF_ESS }, //  0 V70 #0 Code
	{ "epr15421.ic40",				0x020000, 0xaa96422a, 5 | BRF_PRG | BRF_ESS }, //  1

	{ "epr15367.ic31",				0x020000, 0x0220f078, 3 | BRF_PRG | BRF_ESS }, //  2 Z80 #0 Code

	{ "epr15422.ic3",				0x080000, 0x6c61ba6d, 1 | BRF_GRA },           //  3 Main Layer Tiles
	{ "epr15423.ic11",				0x080000, 0x5178912a, 1 | BRF_GRA },           //  4
};

STD_ROM_PICK(as1)
STD_ROM_FN(as1)

static INT32 As1Init()
{
	is_multi32 = 1;

	sprite_length = 0;
	DrvLoadRoms(false);
	BurnAllocMemIndex();
	if (DrvLoadRoms(true)) return 1;

	system32_v70_map();
	multi32_sound_init();
	tilemap_configure_allocate();

	DrvDoReset();

	return 0;
}

struct BurnDriverX BurnDrvAs1 = {
	"as1", NULL, NULL, NULL, "1993",
	"AS-1 Controller\0", NULL, "Sega", "System 32",
	NULL, NULL, NULL, NULL,
	0, 2, HARDWARE_SEGA_SYSTEM32, GBF_MISC, 0,
	NULL, as1RomInfo, as1RomName, NULL, NULL, NULL, NULL, NULL, NULL,
	As1Init, DrvExit, DrvFrame, MultiDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 224, 8, 3
};

