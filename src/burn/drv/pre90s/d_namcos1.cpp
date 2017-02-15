// FB Alpha Namco System 1 driver module
// Based on MAME driver by Ernesto Corvi

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "m6800_intf.h"
#include "burn_ym2151.h"
#include "namco_snd.h"
#include "dac.h"
#include "driver.h"

static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *AllMem;
static UINT8 *MemEnd;

static UINT8 *DrvMainROM;
static UINT8 *DrvSoundROM;
static UINT8 *DrvMCUROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvMainRAM;
static UINT8 *DrvTriRAM;
static UINT8 *DrvSoundRAM;
static UINT8 *DrvMCURAM;
static UINT8 *DrvNVRAM;
static UINT8 *DrvPfCtrl;

static UINT8 *DrvTransTable;

static UINT8 *DrvPalRAMR;
static UINT8 *DrvPalRAMG;
static UINT8 *DrvPalRAMB;
static UINT16 *DrvPalRegs;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 sub_cpu_reset;
static UINT8 sub_cpu_in_reset;
static INT32 shared_watchdog;

static UINT32 bank_offsets[2][8];
static UINT8  sound_bank;
static UINT8  mcu_bank;

static INT32 dac0_value;
static INT32 dac1_value;
static INT32 dac0_gain;
static INT32 dac1_gain;

static INT32 buffer_sprites;
static UINT8 mcu_patch_data;
static UINT8 coin_lockout;

static UINT8 (*input_read_callback)(INT32 offset) = NULL;

static UINT8 (*key_read_callback)(INT32 offset) = NULL;
static void (*key_write_callback)(INT32 offset, UINT8 data) = NULL;

static UINT16 namcos1_key_id;
static UINT8  namcos1_key[8];
static UINT32 namcos1_key_numerator_high_word;
static UINT32 namcos1_key_quotient;
static UINT32 namcos1_key_reminder;
static INT32  namcos1_key_reg;
static INT32  namcos1_key_rng;
static INT32  namcos1_key_swap4;
static INT32  namcos1_key_bottom4;
static INT32  namcos1_key_top4;
static INT32  namcos1_key_swap4_arg;

static INT32 input_count;
static INT32 strobe_count;
static UINT8 stored_input[2];

static INT32 watchdog;
static INT32 flipscreen;
static INT32 clip_min_x;
static INT32 clip_max_x;
static INT32 clip_min_y;
static INT32 clip_max_y;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvJoy5[8];
static UINT8 DrvJoy6[8];
static UINT8 DrvJoy7[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[7];
static UINT8 DrvReset;

static INT16 Paddle[2]; // quester
static UINT8 quester = 0; // use paddle?

static UINT8 sixtyhz = 0; // some games only like 60hz
static UINT8 dac_kludge = 0; // dac is fine-tuned to each game, due to cycle-inaccuracy of our m680x cores.

static struct BurnInputInfo DrvInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 6,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Drv)

static struct BurnInputInfo Tankfrce4InputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 coin"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy4 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy4 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy4 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy4 + 4,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 coin"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy6 + 3,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy6 + 2,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy6 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy6 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy6 + 4,	"p2 fire 1"	},

	{"P3 Coin",		BIT_DIGITAL,	DrvJoy1 + 7,	"p3 coin"	},
	{"P3 Up",		BIT_DIGITAL,	DrvJoy5 + 3,	"p3 up"		},
	{"P3 Down",		BIT_DIGITAL,	DrvJoy5 + 2,	"p3 down"	},
	{"P3 Left",		BIT_DIGITAL,	DrvJoy5 + 1,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy5 + 0,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy5 + 4,	"p3 fire 1"	},

	{"P4 Coin",		BIT_DIGITAL,	DrvJoy2 + 7,	"p4 coin"	},
	{"P4 Up",		BIT_DIGITAL,	DrvJoy7 + 3,	"p4 up"		},
	{"P4 Down",		BIT_DIGITAL,	DrvJoy7 + 2,	"p4 down"	},
	{"P4 Left",		BIT_DIGITAL,	DrvJoy7 + 1,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy7 + 0,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy7 + 4,	"p4 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 6,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Tankfrce4)

static struct BurnInputInfo FaceoffInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Left Stick Up",	BIT_DIGITAL,	DrvJoy4 + 3,	"p1 up"		},
	{"P1 Left Stick Down",	BIT_DIGITAL,	DrvJoy4 + 2,	"p1 down"	},
	{"P1 Left Stick Left",	BIT_DIGITAL,	DrvJoy4 + 1,	"p1 left"	},
	{"P1 Left Stick Right",	BIT_DIGITAL,	DrvJoy4 + 0,	"p1 right"	},
	{"P3 Right Stick Up",	BIT_DIGITAL,	DrvJoy5 + 3,	"p3 up"		},
	{"P3 Right Stick Down",	BIT_DIGITAL,	DrvJoy5 + 2,	"p3 down"	},
	{"P3 Right Stick Left",	BIT_DIGITAL,	DrvJoy5 + 1,	"p3 left"	},
	{"P3 Right Stick Right",BIT_DIGITAL,	DrvJoy5 + 0,	"p3 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy4 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy5 + 4,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Left Stick Up",	BIT_DIGITAL,	DrvJoy6 + 3,	"p2 up"		},
	{"P2 Left Stick Down",	BIT_DIGITAL,	DrvJoy6 + 2,	"p2 down"	},
	{"P2 Left Stick Left",	BIT_DIGITAL,	DrvJoy6 + 1,	"p2 left"	},
	{"P2 Left Stick Right",	BIT_DIGITAL,	DrvJoy6 + 0,	"p2 right"	},
	{"P4 ight Stick Up",	BIT_DIGITAL,	DrvJoy7 + 3,	"p4 up"		},
	{"P4 ight Stick Down",	BIT_DIGITAL,	DrvJoy7 + 2,	"p4 down"	},
	{"P4 ight Stick Left",	BIT_DIGITAL,	DrvJoy7 + 1,	"p4 left"	},
	{"P4 ight Stick Right",	BIT_DIGITAL,	DrvJoy7 + 0,	"p4 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy6 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy7 + 4,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 6,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Faceoff)
static struct BurnInputInfo BerabohmInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy5 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy5 + 1,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy5 + 2,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 fire 4"	},
	{"P1 Button 5",		BIT_DIGITAL,	DrvJoy4 + 1,	"p1 fire 5"	},
	{"P1 Button 6",		BIT_DIGITAL,	DrvJoy4 + 2,	"p1 fire 6"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy7 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy7 + 1,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy7 + 2,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy6 + 0,	"p2 fire 4"	},
	{"P2 Button 5",		BIT_DIGITAL,	DrvJoy6 + 1,	"p2 fire 5"	},
	{"P2 Button 6",		BIT_DIGITAL,	DrvJoy6 + 2,	"p2 fire 6"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 6,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Berabohm)

static struct BurnDIPInfo DrvDIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Drv)

static struct BurnDIPInfo ShadowldDIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Alt. sound effects"	},
	{0x14, 0x01, 0x20, 0x20, "Off"			},
	{0x14, 0x01, 0x20, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x14, 0x01, 0x40, 0x40, "Off"			},
	{0x14, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Shadowld)

static struct BurnDIPInfo DspiritDIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL			},
	
	{0   , 0xfe, 0   ,    2, "Life"			},
	{0x14, 0x01, 0x7f, 0x7f, "2"			},
	{0x14, 0x01, 0x7f, 0x16, "3"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Dspirit)

static struct BurnDIPInfo PacmaniaDIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Auto Data Sampling"	},
	{0x14, 0x01, 0x08, 0x08, "Off"			},
	{0x14, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Kick Watchdog in IRQ"	},
	{0x14, 0x01, 0x20, 0x20, "No"			},
	{0x14, 0x01, 0x20, 0x00, "Yes"			},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x14, 0x01, 0x40, 0x40, "Off"			},
	{0x14, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Pacmania)

static struct BurnDIPInfo Galaga88DIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Auto Data Sampling"	},
	{0x14, 0x01, 0x28, 0x28, "Off"			},
	{0x14, 0x01, 0x28, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Galaga88)

static struct BurnDIPInfo Ws89DIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x14, 0x01, 0x01, 0x01, "Off"			},
	{0x14, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Draw Debug Lines"	},
	{0x14, 0x01, 0x02, 0x02, "Off"			},
	{0x14, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Ws89)

static struct BurnDIPInfo DangseedDIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Auto Data Sampling"	},
	{0x14, 0x01, 0x04, 0x04, "Off"			},
	{0x14, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Kick Watchdog in IRQ"	},
	{0x14, 0x01, 0x20, 0x20, "No"			},
	{0x14, 0x01, 0x20, 0x00, "Yes"			},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x14, 0x01, 0x40, 0x40, "Off"			},
	{0x14, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Dangseed)

static struct BurnDIPInfo Ws90DIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x14, 0x01, 0x01, 0x01, "Off"			},
	{0x14, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Draw Debug Lines"	},
	{0x14, 0x01, 0x02, 0x02, "Off"			},
	{0x14, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Ws90)

static struct BurnDIPInfo BoxyboyDIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x14, 0x01, 0x40, 0x40, "Off"			},
	{0x14, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Boxyboy)

static struct BurnDIPInfo PuzlclubDIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Auto Data Sampling"	},
	{0x14, 0x01, 0x40, 0x40, "Off"			},
	{0x14, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Puzlclub)

static struct BurnDIPInfo MmazeDIPList[]=
{
	{0x14, 0xff, 0xff, 0x91, NULL			},

	{0   , 0xfe, 0   ,    2, "Level Select"		},
	{0x14, 0x01, 0x01, 0x01, "Off"			},
	{0x14, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x14, 0x01, 0x10, 0x10, "Off"			},
	{0x14, 0x01, 0x10, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Mmaze)

static struct BurnDIPInfo BakutotuDIPList[]=
{
	{0x14, 0xff, 0xff, 0xf9, NULL			},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x14, 0x01, 0x01, 0x01, "Off"			},
	{0x14, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Sprite Viewer"	},
	{0x14, 0x01, 0x08, 0x08, "Off"			},
	{0x14, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Level Selection"	},
	{0x14, 0x01, 0x10, 0x10, "Off"			},
	{0x14, 0x01, 0x10, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Show Coordinates"	},
	{0x14, 0x01, 0x20, 0x20, "Off"			},
	{0x14, 0x01, 0x20, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Invincibility (Cheat)"},
	{0x14, 0x01, 0x40, 0x40, "Off"			},
	{0x14, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Bakutotu)

static struct BurnDIPInfo WldcourtDIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x14, 0x01, 0x01, 0x01, "Off"			},
	{0x14, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Draw Debug Lines"	},
	{0x14, 0x01, 0x7e, 0x7e, "Off"			},
	{0x14, 0x01, 0x7e, 0x5c, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Wldcourt)

static struct BurnDIPInfo Splatter3DIPList[]=
{
	{0x14, 0xff, 0xff, 0xa0+0x17, NULL		}, // 0x17 for the strange watchdog settings.

	{0   , 0xfe, 0   ,    2, "Stage Select"		},
	{0x14, 0x01, 0x20, 0x20, "Off"			},
	{0x14, 0x01, 0x20, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Splatter3)

static struct BurnDIPInfo FaceoffDIPList[]=
{
	{0x1a, 0xff, 0xff, 0x80, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x1a, 0x01, 0x80, 0x80, "Off"			},
	{0x1a, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Faceoff)

static struct BurnDIPInfo BerabohmDIPList[]=
{
	{0x1a, 0xff, 0xff, 0xa1, NULL			},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x1a, 0x01, 0x01, 0x01, "Off"			},
	{0x1a, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Invulnerability"	},
	{0x1a, 0x01, 0x20, 0x20, "Off"			},
	{0x1a, 0x01, 0x20, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x1a, 0x01, 0x80, 0x80, "Off"			},
	{0x1a, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Berabohm)

static UINT8 faceoff_inputs_read(INT32 offset)
{
	if ((offset & 1) == 0)
	{
		return (DrvInputs[0] & 0x80) | stored_input[0];
	}
	else
	{
		INT32 res = DrvInputs[1] & 0x80;

		if (++strobe_count > 8)
		{
			strobe_count = 0;
			res |= input_count;

			switch (input_count)
			{
				case 0:
					stored_input[0] = DrvInputs[3] & 0x1f;
					stored_input[1] = (DrvInputs[6] & 0x07) << 3;
				break;

				case 3:
					stored_input[0] = DrvInputs[5] & 0x1f;
				break;

				case 4:
					stored_input[0] = DrvInputs[4] & 0x1f;
					stored_input[1] = DrvInputs[6] & 0x18;
				break;

				default:
					stored_input[0] = 0x1f;
					stored_input[1] = 0x1f;
				break;
			}

			input_count = (input_count + 1) & 7;
		}
		else
		{
			res |= 0x40 | stored_input[1];
		}

		return res;
	}
}

static UINT8 berabohm_buttons_read(INT32 offset)
{
	if ((offset & 1) == 0)
	{
		INT32 inp = input_count;

		if (inp == 4)
		{
			return DrvInputs[0];
		}
		else
		{
			INT32 res = DrvInputs[3 + input_count] ^ 0xff;

			if (res & 1) res = 0x7f;        // weak
			else if (res & 2) res = 0x48;   // medium
			else if (res & 4) res = 0x40;   // strong

			return res;
		}
	}
	else
	{
		INT32 res = DrvInputs[1] & 0x8f;

		if (++strobe_count > 4)
		{
			strobe_count = 0;
			stored_input[0] ^= 0x40;
			if (stored_input[0] == 0)
			{
				input_count = (input_count + 1) % 5;
				if (input_count == 3) res |= 0x10;
			}
		}

		return res | stored_input[0];
	}
}

static UINT8 quester_paddle_read(INT32 offset)
{
	if ((offset & 1) == 0)
	{
		INT32 ret;

		if (!(strobe_count & 0x20))
			ret = (DrvInputs[0]&0x90) | (strobe_count & 0x40) | (Paddle[0]&0x0f);
		else
			ret = (DrvInputs[0]&0x90) | (strobe_count & 0x40) | (Paddle[1]&0x0f);

		strobe_count ^= 0x40;

		return ret;
	}
	else
	{
		INT32 ret;

		if (!(strobe_count & 0x20))
			ret = (DrvInputs[1]&0x90) | 0x00 | (Paddle[0]>>4);
		else
			ret = (DrvInputs[1]&0x90) | 0x20 | (Paddle[1]>>4);

		if (!(strobe_count & 0x40)) strobe_count ^= 0x20;

		return ret;
	}
}

static UINT8 key_type1_read(INT32 offset)
{
	if (offset < 3)
	{
		int d = namcos1_key[0];
		int n = (namcos1_key[1] << 8) | namcos1_key[2];
		int q,r;

		if (d)
		{
			q = n / d;
			r = n % d;
		}
		else
		{
			q = 0xffff;
			r = 0x00;
		}

		if (offset == 0) return r;
		if (offset == 1) return q >> 8;
		if (offset == 2) return q & 0xff;
	}
	else if (offset == 3)
		return namcos1_key_id;

	return 0;
}

static void key_type1_write(INT32 offset, UINT8 data)
{
	if (offset < 3)
		namcos1_key[offset] = data;
}

static UINT8 key_type2_read(INT32 offset)
{
	namcos1_key_numerator_high_word = 0;

	if (offset < 4)
	{
		if (offset == 0) return namcos1_key_reminder >> 8;
		if (offset == 1) return namcos1_key_reminder & 0xff;
		if (offset == 2) return namcos1_key_quotient >> 8;
		if (offset == 3) return namcos1_key_quotient & 0xff;
	}
	else if (offset == 4)
		return namcos1_key_id;

	return 0;
}

static void key_type2_write(INT32 offset, UINT8 data)
{
	if (offset < 5)
	{
		namcos1_key[offset] = data;

		if (offset == 3)
		{
			UINT32 d = (namcos1_key[0] << 8) | namcos1_key[1];
			UINT32 n = (namcos1_key_numerator_high_word << 16) | (namcos1_key[2] << 8) | namcos1_key[3];

			if (d)
			{
				namcos1_key_quotient = n / d;
				namcos1_key_reminder = n % d;
			}
			else
			{
				namcos1_key_quotient = 0xffff;
				namcos1_key_reminder = 0x0000;
			}

			namcos1_key_numerator_high_word = (namcos1_key[2] << 8) | namcos1_key[3];
		}
	}
}

static UINT8 key_type3_read(INT32 offset)
{
	INT32 op = (offset & 0x70) >> 4;

	if (op == namcos1_key_reg)     return namcos1_key_id;
	if (op == namcos1_key_rng)     return rand();
	if (op == namcos1_key_swap4)   return (namcos1_key[namcos1_key_swap4_arg] << 4) | (namcos1_key[namcos1_key_swap4_arg] >> 4);
	if (op == namcos1_key_bottom4) return (offset << 4) | (namcos1_key[namcos1_key_swap4_arg] & 0x0f);
	if (op == namcos1_key_top4)    return (offset << 4) | (namcos1_key[namcos1_key_swap4_arg] >> 4);

	return 0;
}

static void key_type3_write(INT32 offset, UINT8 data)
{
	namcos1_key[(offset & 0x70) >> 4] = data;
}

static void namcos1_key_init(INT32 type, UINT16 key, INT32 reg, INT32 rng, INT32 swap4_arg, INT32 swap4, INT32 bottom4, INT32 top4)
{
	switch (type)
	{
		case 1:
			key_read_callback = key_type1_read;
			key_write_callback = key_type1_write;
		break;

		case 2:
			key_read_callback = key_type2_read;
			key_write_callback = key_type2_write;
		break;

		case 3:
			key_read_callback = key_type3_read;
			key_write_callback = key_type3_write;
		break;

		default:
			bprintf (0, _T("namco_key_init called with invalid key type (%d)!\n"), type);
	}

	namcos1_key_id        = key;
	namcos1_key_reg       = reg;
	namcos1_key_rng       = rng;
	namcos1_key_swap4_arg = swap4_arg;
	namcos1_key_swap4     = swap4;
	namcos1_key_bottom4   = bottom4;
	namcos1_key_top4      = top4;
}

static UINT8 namco_c116_read(UINT16 offset)
{
	UINT8 *RAM;

	offset &= 0x7fff;

	switch (offset & 0x1800)
	{
		case 0x0000:
			RAM = DrvPalRAMR;
		break;

		case 0x0800:
			RAM = DrvPalRAMG;
		break;

		case 0x1000:
			RAM = DrvPalRAMB;
		break;

		default: // case 0x1800 (internal registers)
		{
			INT32 reg = (offset & 0xf) >> 1;
			if (offset & 1)
				return DrvPalRegs[reg] & 0xff;
			else
				return DrvPalRegs[reg] >> 8;
		}
	}

	return RAM[((offset & 0x6000) >> 2) | (offset & 0x7ff)];
}

static void namco_c116_write(UINT16 offset, UINT8 data)
{
	UINT8 *RAM;

	offset &= 0x7fff;

	switch (offset & 0x1800)
	{
		case 0x0000:
			RAM = DrvPalRAMR;
		break;

		case 0x0800:
			RAM = DrvPalRAMG;
		break;

		case 0x1000:
			RAM = DrvPalRAMB;
		break;

		default: // case 0x1800 (internal registers)
		{
			INT32 reg = (offset & 0xf) >> 1;
			if (offset & 1)
				DrvPalRegs[reg] = (DrvPalRegs[reg] & 0xff00) | data;
			else
				DrvPalRegs[reg] = (DrvPalRegs[reg] & 0x00ff) | (data << 8);
			return;
		}
	}

	int color = ((offset & 0x6000) >> 2) | (offset & 0x7ff);
	RAM[color] = data;

	DrvPalette[color] = BurnHighCol(DrvPalRAMR[color], DrvPalRAMG[color], DrvPalRAMB[color], 0);
}

static UINT8 virtual_read(UINT32 address)
{
	if (address >= 0x2e0000 && address <= 0x2e7fff) {
		return namco_c116_read(address & 0x7fff);
	}

	if (address >= 0x2f0000 && address <= 0x2f7fff) {
		return DrvVidRAM[address & 0x7fff];
	}

	if (address >= 0x2f8000 && address <= 0x2f9fff) {
		if (key_read_callback) {
			return key_read_callback(address & 0x1fff);
		}
		return 0; // no key
	}

	if (address >= 0x2fc000 && address <= 0x2fcfff) {
		return DrvSprRAM[address & 0xfff];
	}

	if (address >= 0x2fe000 && address <= 0x2fefff) {
		return namcos1_custom30_read(address & 0x3ff);
	}

	if (address >= 0x2ff000 && address <= 0x2fffff) {
		return DrvTriRAM[address & 0x7ff];
	}

	if (address >= 0x300000 && address <= 0x307fff) {
		return DrvMainRAM[address & 0x7fff];
	}

	if ((address & 0x400000) == 0x400000) {
		return DrvMainROM[address & 0x3fffff];
	}

	return 0;
}

static void virtual_write(UINT32 address, UINT8 data)
{
	if (address >= 0x2c0000 && address <= 0x2c1fff) {
		// _3dcs_w	(not used)
		return;
	}

	if (address >= 0x2e0000 && address <= 0x2e7fff) {
		namco_c116_write(address & 0x7fff, data);
		return;
	}

	if (address >= 0x2f0000 && address <= 0x2f7fff) {
		DrvVidRAM[address & 0x7fff] = data;
		return;
	}

	if (address >= 0x2f8000 && address <= 0x2f9fff) {
		if (key_write_callback) {
			key_write_callback(address & 0x1fff, data);
		}
		return;
	}

	if (address >= 0x2fc000 && address <= 0x2fcfff) {
		if (address == 0x2fcff2) buffer_sprites = 1;
		DrvSprRAM[address & 0xfff] = data;
		return;
	}

	if (address >= 0x2fd000 && address <= 0x2fdfff) {
		DrvPfCtrl[address & 0x1f] = data;
		return;
	}

	if (address >= 0x2fe000 && address <= 0x2fefff) {
		namcos1_custom30_write(address & 0x3ff, data);
		return;
	}

	if (address >= 0x2ff000 && address <= 0x2fffff) {
		DrvTriRAM[address & 0x7ff] = data;
		return;
	}

	if (address >= 0x300000 && address <= 0x307fff) {
		DrvMainRAM[address & 0x7fff] = data;
		return;
	}
}

static void subres_callback(INT32 state)
{
	if (state != sub_cpu_in_reset)
	{
		mcu_patch_data = 0;
		sub_cpu_in_reset = state;
	}

	if (state == CPU_IRQSTATUS_ACK)
	{
		M6809Close();

		M6809Open(1);
		M6809Reset();
		M6809Close();

		M6809Open(2);
		M6809Reset();
		M6809Close();

		M6809Open(0);

		HD63701Reset();
	}
}

static void bankswitch(int whichcpu, int whichbank, int a0, UINT8 data)
{
	UINT32 &bank = bank_offsets[whichcpu][whichbank];

	if (a0 == 0)
		bank = (bank & 0x1fe000) | ((data & 0x03) * 0x200000);
	else
		bank = (bank & 0x600000) | (data * 0x2000);

	INT32 mapbank = whichbank * 0x2000;

	M6809UnmapMemory(mapbank, mapbank + 0x1fff, MAP_RAM);

	if (bank >= 0x400000 && bank <= 0x7fffff) {
		M6809MapMemory(DrvMainROM + (bank & 0x3fe000), mapbank, mapbank + 0x1fff, MAP_ROM);
		return;
	}

	if (bank >= 0x2f0000 && bank <= 0x2f7fff) {
		M6809MapMemory(DrvVidRAM  + (bank & 0x006000), mapbank, mapbank + 0x1fff, MAP_RAM);
		return;
	}

	if (bank >= 0x300000 && bank <= 0x307fff) {
		M6809MapMemory(DrvMainRAM + (bank & 0x006000), mapbank, mapbank + 0x1fff, MAP_RAM);
		return;
	}
}

static void kick_watchdog(int whichcpu)
{
	static const int ALL_CPU_MASK = 7;

	shared_watchdog |= (1 << whichcpu);

	if (shared_watchdog == ALL_CPU_MASK || !sub_cpu_reset)
	{
		shared_watchdog = 0;
		watchdog = 0;
	}
}

static void register_write(int whichcpu, UINT16 offset, UINT8 data)
{
	INT32 reg = (offset >> 9) & 0xf;

	switch (reg)
	{
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
		case 7:
			bankswitch(whichcpu, reg, offset & 1, data);
		break;

		case 8:  // F000 - SUBRES (halt/reset everything but main CPU)
			if (whichcpu == 0)
			{
				sub_cpu_reset = data & 1;
				subres_callback(sub_cpu_reset ? CLEAR_LINE : ASSERT_LINE);
			}
		break;

		case 9:  // F200 - kick watchdog
			kick_watchdog(whichcpu);
		break;

		case 11: // F600 - IRQ ack
			M6809SetIRQLine(0, CPU_IRQSTATUS_NONE);
		break;

		case 12: // F800 - FIRQ ack
			M6809SetIRQLine(1, CPU_IRQSTATUS_NONE);
		break;

		case 13: // FA00 - assert FIRQ on sub CPU
			if (whichcpu == 0)
			{
				M6809Close();
				M6809Open(1);
				M6809SetIRQLine(1, CPU_IRQSTATUS_ACK);
				M6809Close();
				M6809Open(0);
			}
		break;

		case 14: // FC00 - set initial ROM bank for sub CPU
			if (whichcpu == 0)
			{
				bank_offsets[1][7] = 0x600000 | (data * 0x2000);

				M6809Close();
				M6809Open(1);
				M6809MapMemory(DrvMainROM + (bank_offsets[1][7] & 0x3fffff), 0xe000, 0xffff, MAP_ROM);
				M6809Close();
				M6809Open(0);
			}
		break;
	}
}

static inline UINT32 remap(INT32 whichcpu, UINT16 offset)
{
	return bank_offsets[whichcpu][offset>>13] | (offset & 0x1fff);
}

static void main_write(UINT16 address, UINT8 data)
{
	if (address < 0xe000)
	{
		virtual_write(remap(0, address), data);
	}
	else
	{
		register_write(0, address & 0x1fff, data);
	}
}

static UINT8 main_read(UINT16 address)
{
	return virtual_read(remap(0, address));
}

static void sub_write(UINT16 address, UINT8 data)
{
	if (address < 0xe000)
	{
		virtual_write(remap(1, address), data);
	}
	else
	{
		register_write(1, address & 0x1fff, data);
	}
}

static UINT8 sub_read(UINT16 address)
{
	return virtual_read(remap(1, address));
}

static void sound_bankswitch(INT32 data)
{
	sound_bank = data;

	INT32 bank = (data & 7) * 0x4000;

	M6809MapMemory(DrvSoundROM + bank, 0x0000, 0x3fff, MAP_ROM);
}

static void sound_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf800) == 0x5000) {
		namcos1_custom30_write(address & 0x3ff, data);
		return;
	}

	switch (address)
	{
		case 0x4000:
			BurnYM2151SelectRegister(data);
		return;

		case 0x4001:
			BurnYM2151WriteRegister(data);
		return;

		case 0xc000:
		case 0xc001:
			sound_bankswitch(data >> 4);
		return;

		case 0xd001:
			kick_watchdog(2);
		return;

		case 0xe000:
			M6809SetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;
	}
}

static UINT8 sound_read(UINT16 address)
{
	if ((address & 0xf800) == 0x5000) {
		return namcos1_custom30_read(address & 0x3ff);
	}

	switch (address)
	{
		case 0x4000:
		case 0x4001:
			return BurnYM2151ReadStatus();
	}

	return 0;
}

static void mcu_bankswitch(INT32 data)
{
	mcu_bank = data;

	INT32 bank = 0;

	switch (data & 0xfc)
	{
		case 0xf8: bank = 0;  data ^= 2; break;    // bit 2 : ROM 0, A16 is inverted
		case 0xf4: bank = 4;  break;               // bit 3 : ROM 1
		case 0xec: bank = 8;  break;               // bit 4 : ROM 2
		case 0xdc: bank = 12; break;               // bit 5 : ROM 3
		case 0xbc: bank = 16; break;               // bit 6 : ROM 4
		case 0x7c: bank = 20; break;               // bit 7 : ROM 5
		default:   bank = 0;  break;               // illegal (selects multiple ROMs at once)
	}

	bank += (data & 3);

	HD63701MapMemory(DrvMCUROM + 0x10000 + (bank * 0x8000), 0x4000, 0xbfff, MAP_ROM);
}

static void reset_dacs()
{
	dac0_value = 0;
	dac1_value = 0;
	dac0_gain = 0x80;
	dac1_gain = 0x80;
}

static void update_dacs()
{
	DACWrite16(0, (dac0_value * dac0_gain) + (dac1_value * dac1_gain));
}

static void mcu_write(UINT16 address, UINT8 data)
{
	if ((address & 0xffe0) == 0x0000) {
		m6803_internal_registers_w(address & 0x001f, data);
		return;
	}

	if ((address & 0xff80) == 0x0080) {
		DrvMCURAM[(address & 0x7f)] = data;
		return;
	}

	if ((address & 0xf800) == 0xc000) {
		if (address == 0xc000) {
			if (mcu_patch_data == 0xa6) return;
			mcu_patch_data = data;
		}
		DrvTriRAM[address & 0x7ff] = data;
		return;
	}

	switch (address)
	{
		case 0xd000:
			dac0_value = data - 0x80;
			update_dacs();
		return;

		case 0xd400:
			dac1_value = data - 0x80;
			update_dacs();
		return;

		case 0xd800:
			mcu_bankswitch(data);
		return;

		case 0xf000:
			HD63701SetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;
	}
}

static UINT8 mcu_read(UINT16 address)
{
	if ((address & 0xffe0) == 0x0000) {
		return m6803_internal_registers_r(address & 0x001f);
	}

	if ((address & 0xff80) == 0x0080) {
		return DrvMCURAM[(address & 0x7f)];
	}

	switch (address)
	{
		case 0x1000:
		case 0x1001:
			return 0xf0 | (DrvDips[0] >> 4);

		case 0x1002:
		case 0x1003:
			return 0xf0 | (DrvDips[0]);

		case 0x1400:
		case 0x1401:
			if (input_read_callback) return input_read_callback(address & 1);
			return DrvInputs[address & 1];
	}

	return 0;
}

static void mcu_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0x1ff)
	{
		case HD63701_PORT1:
			coin_lockout = (data & 1) ? 0 : 0x18;
		return;

		case HD63701_PORT2:
		{
			INT32 value = (data & 1) | ((data >> 1) & 2);
			dac0_gain = 0x20 * (value+1);

			value = (data >> 3) & 3;
			dac1_gain = 0x20 * (value+1);
			update_dacs();
		}
		return;
	}
}

static UINT8 mcu_read_port(UINT16 port)
{
	switch (port & 0x1ff)
	{
		case HD63701_PORT1:
			return (DrvInputs[2] & 0xf8) | coin_lockout; // coin

		case HD63701_PORT2:
			return 0;
	}

	return 0;
}

static INT32 DrvDACSync() // sync to hd63701 (m6800 core)
{
	return (INT32)(float)(nBurnSoundLen * (M6800TotalCycles() / (1536000.0000 / ((nBurnFPS / 100.0000) - dac_kludge))));
}

static void YM2151IrqHandler(INT32 status)
{
	if (M6809GetActive() == -1) return;

	M6809SetIRQLine(1, (status) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static void set_initial_map_banks()
{
	for (INT32 i = 0; i < 8*2; i++) {
		bank_offsets[i/8][i&7] = 0;
	}

	// default MMU setup for main CPU
	bank_offsets[0][0] = 0x0180 * 0x2000; // bank0 = 0x180(RAM) - evidence: wldcourt
	bank_offsets[0][1] = 0x0180 * 0x2000; // bank1 = 0x180(RAM) - evidence: berabohm
	bank_offsets[0][7] = 0x03ff * 0x2000; // bank7 = 0x3ff(PRG7)

	// default MMU setup for sub CPU
	bank_offsets[1][0] = 0x0180 * 0x2000; // bank0 = 0x180(RAM) - evidence: wldcourt
	bank_offsets[1][7] = 0x03ff * 0x2000; // bank7 = 0x3ff(PRG7)

	M6809Open(0);
	M6809UnmapMemory(0x0000, 0xffff, MAP_RAM);
	M6809MapMemory(DrvMainRAM + 0x000000, 0x0000, 0x1fff, MAP_RAM);
	M6809MapMemory(DrvMainRAM + 0x000000, 0x2000, 0x3fff, MAP_RAM);
	M6809MapMemory(DrvMainROM + 0x3fe000, 0xe000, 0xffff, MAP_ROM);
	M6809Close();

	M6809Open(1);
	M6809UnmapMemory(0x0000, 0xffff, MAP_RAM);
	M6809MapMemory(DrvMainRAM + 0x000000, 0x0000, 0x1fff, MAP_RAM);
	M6809MapMemory(DrvMainROM + 0x3fe000, 0xe000, 0xffff, MAP_ROM);
	M6809Close();
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	if (clear_mem) set_initial_map_banks();  // this fixes rompers boot. (relies on wdt after warning screen)

	M6809Open(0);
	M6809Reset();
	if (clear_mem) m6809_reset_hard();
	M6809Close();

	M6809Open(1);
	M6809Reset();
	if (clear_mem) m6809_reset_hard(); // this gets shadowld to reboot
	M6809Close();

	M6809Open(2);
	M6809Reset();
	if (clear_mem) {
		M6809MapMemory(NULL, 0x0000, 0x3fff, MAP_ROM);
		m6809_reset_hard();
	}
	NamcoSoundReset();
	BurnYM2151Reset();
	DACReset();
	reset_dacs();
	M6809Close();

//	HD63701Open(0);
	HD63701Reset();
	if (clear_mem) {
		HD63701MapMemory(NULL, 0x4000, 0xbfff, MAP_ROM);
		m6800_reset_hard(); // HD63701
	}
//	HD63701Close();

	sub_cpu_in_reset = 1;
	sub_cpu_reset = 0;
	shared_watchdog = 0;
	watchdog = 0;
	mcu_patch_data = 0;
	buffer_sprites = 0;
	coin_lockout = 0;

	memset (namcos1_key, 0, 8);
	namcos1_key_numerator_high_word = 0;
	namcos1_key_quotient = 0;
	namcos1_key_reminder = 0;

	input_count = 0;
	strobe_count = 0;
	stored_input[0] = stored_input[1] = 0;

	Paddle[0] = Paddle[1] = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvMainROM		= Next; Next += 0x400000;
	DrvSoundROM		= Next; Next += 0x020000;
	DrvMCUROM		= Next; Next += 0x0d0000;

	DrvGfxROM0		= Next; Next += 0x020000;
	DrvGfxROM1		= Next; Next += 0x100000;
	DrvGfxROM2		= Next; Next += 0x200000;

	DrvTransTable		= Next; Next += 0x020000 / 8;

	DrvPalette		= (UINT32*)Next; Next += 0x2001 * sizeof(UINT32);

	DrvNVRAM		= Next; Next += 0x000800;

	AllRam			= Next;

	DrvVidRAM		= Next; Next += 0x008000;
	DrvSprRAM		= Next; Next += 0x001000;
	DrvMainRAM		= Next; Next += 0x008000;
	DrvTriRAM		= Next; Next += 0x000800;
	DrvSoundRAM		= Next; Next += 0x002000;
	DrvMCURAM		= Next; Next += 0x000080;

	DrvPfCtrl		= Next; Next += 0x000020;

	DrvPalRAMR		= Next; Next += 0x002000;
	DrvPalRAMG		= Next; Next += 0x002000;
	DrvPalRAMB		= Next; Next += 0x002000;
	DrvPalRegs		= (UINT16*)Next; Next += 0x00008 * sizeof(UINT16);

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void DrvTransTableInit()
{
	for (INT32 i = 0; i < 0x20000; i+= 8) {
		DrvTransTable[i/8] = 1;

		for (INT32 j = 0; j < 8; j++) {
			if (DrvGfxROM0[i+j] != 0) {
				DrvTransTable[i/8] = 0;
			}
		}
	}
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[4]  = { STEP4(0,1) };
	INT32 XOffs[32] = { STEP16(0,4), STEP16(1024, 4) };
	INT32 YOffs[32] = { STEP16(0,64), STEP16(2048, 64) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x100000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM2, 0x100000);

	GfxDecode((0x100000 * 2) / 0x400, 4, 32, 32, Plane, XOffs, YOffs, 0x1000, tmp, DrvGfxROM2);

	BurnFree(tmp);

	return 0;
}

static void PRG7Decode()
{
	for (INT32 i = 0x380000;i < 0x400000;i++)
	{
		if ((i & 0x010000) == 0)
		{
			UINT8 t = DrvMainROM[i];
			DrvMainROM[i] = DrvMainROM[i + 0x010000];
			DrvMainROM[i + 0x010000] = t;
		}
	}
}

static void DoMirror(UINT8 *rom, INT32 size, INT32 mirror_size)
{
	for (INT32 i = mirror_size; i < size; i+=mirror_size)
	{
		memcpy (rom + i, rom, mirror_size);
	}
}

static INT32 Namcos1GetRoms()
{
	char* pRomName;
	struct BurnRomInfo ri;

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++)
	{
		BurnDrvGetRomInfo(&ri, i);

		// the rom names have the rom position in them
		INT32 whichrompos = 0;
		for (INT32 j = 0; j < (INT32)strlen(pRomName); j++) {
			if (pRomName[j] == '.') break;
			if ((pRomName[j] & 0xf8) == 0x30) { // '0', '1', '2', '3'
				whichrompos = pRomName[j] & 7;
			}
		}
		//bprintf(0, _T("[%S] rompos %X.\n"), pRomName, whichrompos);
		// pacmania has rom positions at the end of the file name
		if ((pRomName[strlen(pRomName)-1] & 0xf8) == 0x30 && pRomName[strlen(pRomName)-2] != '1') {
			whichrompos = pRomName[strlen(pRomName)-1] & 7;
		}

		if ((ri.nType & BRF_PRG) && (ri.nType & 0x0f) == 1) {
			if (BurnLoadRom(DrvSoundROM + (whichrompos * 0x10000), i, 1)) return 1;
			continue;
		}

		if ((ri.nType & BRF_PRG) && (ri.nType & 0x0f) == 2) {
			if (BurnLoadRom(DrvMainROM + (whichrompos * 0x80000), i, 1)) return 1;
			DoMirror(DrvMainROM + (whichrompos * 0x80000), 0x80000, ri.nLen);
			continue;
		}

		if ((ri.nType & BRF_PRG) && (ri.nType & 0x0f) == 3) {
			if (BurnLoadRom(DrvMCUROM, i, 1)) return 1;
			continue;
		}

		if ((ri.nType & BRF_PRG) && (ri.nType & 0x0f) == 4) {
			if (BurnLoadRom(DrvMCUROM + 0x10000 + (whichrompos * 0x20000), i, 1)) return 1;
			if (ri.nLen < 0x20000) DoMirror(DrvMCUROM + 0x10000 + (whichrompos * 0x20000), 0x20000, ri.nLen);
			continue;
		}

		if ((ri.nType & BRF_GRA) && (ri.nType & 0x0f) == 5) {
			if (BurnLoadRom(DrvGfxROM0, i, 1)) return 1;
			continue;
		}

		if ((ri.nType & BRF_GRA) && (ri.nType & 0x0f) == 6) {
			if (BurnLoadRom(DrvGfxROM1 + (whichrompos * 0x20000), i, 1)) return 1;
			if (ri.nLen < 0x20000) DoMirror(DrvGfxROM1 + (whichrompos * 0x20000), 0x20000, ri.nLen);
			continue;
		}

		if ((ri.nType & BRF_GRA) && (ri.nType & 0x0f) == 7) {
			if (BurnLoadRom(DrvGfxROM2 + (whichrompos * 0x20000), i, 1)) return 1;
			if (ri.nLen < 0x20000) DoMirror(DrvGfxROM2 + (whichrompos * 0x20000), 0x20000, ri.nLen);
			continue;
		}
	}

	DrvGfxDecode();
	DrvTransTableInit();
	PRG7Decode();

	return 0;
}

static INT32 DrvInit()
{
	//BurnSetRefreshRate((sixtyhz) ? 60.00 : 60.6060); above 60hz is a no-go, causes horrible sound issues on some systems (ym2151)
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (Namcos1GetRoms()) return 1;

	M6809Init(3);

	M6809Open(0);
	M6809SetWriteHandler(main_write);
	M6809SetReadHandler(main_read);
	M6809SetReadOpHandler(main_read);
	M6809SetReadOpArgHandler(main_read);
	M6809Close();

	M6809Open(1);
	M6809SetWriteHandler(sub_write);
	M6809SetReadHandler(sub_read);
	M6809SetReadOpHandler(sub_read);
	M6809SetReadOpArgHandler(sub_read);
	M6809Close();

	M6809Open(2);
	M6809MapMemory(DrvTriRAM,		0x7000, 0x77ff, MAP_RAM);
	M6809MapMemory(DrvSoundRAM,		0x8000, 0x9fff, MAP_RAM);
	M6809MapMemory(DrvSoundROM + 0x0000,	0xc000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(sound_write);
	M6809SetReadHandler(sound_read);
	M6809Close();

	HD63701Init(1);
//	HD63701Open(0);
	HD63701MapMemory(DrvTriRAM,		0xc000, 0xc7ff, MAP_ROM);
	HD63701MapMemory(DrvNVRAM,		0xc800, 0xcfff, MAP_RAM);
	HD63701MapMemory(DrvMCUROM + 0x0000,	0xf000, 0xffff, MAP_ROM);
//	HD63701SetReadOpHandler(mcu_read);
//	HD63701SetReadOpArgHandler(mcu_read);
	HD63701SetReadHandler(mcu_read);
	HD63701SetWriteHandler(mcu_write);
	HD63701SetWritePortHandler(mcu_write_port);
	HD63701SetReadPortHandler(mcu_read_port);
//	HD63701Close();

	BurnYM2151Init(3579580);
	BurnYM2151SetIrqHandler(&YM2151IrqHandler);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.60, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.60, BURN_SND_ROUTE_RIGHT);

	NamcoSoundInit(24000/2, 8, 1);
	NacmoSoundSetAllRoutes(0.50 * 10.0 / 16.0, BURN_SND_ROUTE_BOTH);

	DACInit(0, 0, 1, DrvDACSync);

	DACSetRoute(0, 0.50, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	input_read_callback = NULL;
	key_read_callback = NULL;
	key_write_callback = NULL;

	sixtyhz = 0;
	dac_kludge = 0;
	quester = 0;

	M6809Exit();
	HD63701Exit();

	NamcoSoundExit();
	NamcoSoundProm = NULL;

	DACExit();

	BurnYM2151Exit();

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 color = 0; color < 0x1800; color++)
	{
		DrvPalette[color] = BurnHighCol(DrvPalRAMR[color], DrvPalRAMG[color], DrvPalRAMB[color], 0);
	}
}


static void draw_layer(INT32 layer, INT32 pri_mask)
{
	INT32 layer_size[6][2] = { { 64, 64 }, { 64, 64 }, { 64, 64 }, { 64, 32 }, { 36, 28 }, { 36, 28 } };

	UINT16 ram_offset[6] = { 0x0000, 0x2000, 0x4000, 0x6000, 0x7010, 0x7810 };

	INT32 map_width = layer_size[layer][0];
	INT32 map_height = layer_size[layer][1];

	INT32 width = map_width * 8;
	INT32 height = map_height * 8;

	INT32 scrollx = 0;
	INT32 scrolly = 0;

	if (layer < 4) {
		INT32 dispx[4] = { 48, 46, 45, 44 };

		scrollx = (DrvPfCtrl[layer*4+1] + (DrvPfCtrl[layer*4+0] * 256));
		scrolly = (DrvPfCtrl[layer*4+3] + (DrvPfCtrl[layer*4+2] * 256));

		if (flipscreen) {
			scrollx = -(scrollx + 288 + dispx[layer]);
			scrolly = -(scrolly + 224 + 24);
		} else {
			scrollx =  (scrollx + dispx[layer]);
			scrolly =  (scrolly +  24);
		}

		scrollx &= 0x1ff;
		scrolly &= height - 1;
	}

	INT32 color = ((DrvPfCtrl[0x18 + layer] & 7) * 256) + 0x800;

	UINT8 *ram = DrvVidRAM + ram_offset[layer];

	for (INT32 offs = 0; offs < map_width * map_height; offs++)
	{
		INT32 sx = (offs % map_width) * 8;
		INT32 sy = (offs / map_width) * 8;

		sx -= scrollx;
		sy -= scrolly;

		if (sx < -7) sx += width;
		if (sy < -7) sy += height;

		if (sy >= nScreenHeight || sx >= nScreenWidth) continue;

		INT32 code = ram[offs * 2 + 1] + ((ram[offs * 2] & 0x3f) * 256);

		if (DrvTransTable[code] == 0)
		{
			UINT8 *gfx = DrvGfxROM1 + (code * 0x40);
			UINT8 *msk = DrvGfxROM0 + (code * 0x08);

			for (INT32 yy = 0; yy < 8; yy++, msk++, gfx+=8)
			{
				if (*msk == 0) continue; // transparent line

				if ((sy + yy) < clip_min_y || (sy + yy) >= clip_max_y) continue;

				UINT16 *dst = pTransDraw + ((sy + yy) * nScreenWidth) + sx;
				UINT8 *pri = pPrioDraw + ((sy + yy) * nScreenWidth) + sx;

				for (INT32 xx = 0; xx < 8; xx++)
				{
					if ((sx + xx) < clip_min_x || (sx + xx) >= clip_max_x) continue;

					if (*msk & (0x80 >> xx)) {
						dst[xx] = gfx[xx] + color;
						pri[xx] = pri_mask;
					}
				}
			}
		}
	}
}

static void draw_sprites()
{
	UINT8 *spriteram = DrvSprRAM + 0x800;
	const UINT8 *source = &spriteram[0x800-0x20];
	const UINT8 *finish = &spriteram[0];

	INT32 sprite_xoffs = spriteram[0x07f5] + ((spriteram[0x07f4] & 1) << 8);
	INT32 sprite_yoffs = spriteram[0x07f7];

	while (source >= finish)
	{
		static const INT32 sprite_size[4] = { 16, 8, 32, 4 };
		INT32 attr1 = source[10];
		INT32 attr2 = source[14];
		INT32 color = source[12];
		INT32 flipx = (attr1 & 0x20) >> 5;
		INT32 flipy = (attr2 & 0x01);
		INT32 sizex = sprite_size[(attr1 & 0xc0) >> 6];
		INT32 sizey = sprite_size[(attr2 & 0x06) >> 1];
		INT32 tx = (attr1 & 0x18) & (~(sizex-1));
		INT32 ty = (attr2 & 0x18) & (~(sizey-1));
		INT32 sx = source[13] + ((color & 0x01) << 8);
		INT32 sy = -source[15] - sizey;
		INT32 sprite = source[11];
		INT32 sprite_bank = attr1 & 7;
		INT32 priority = (source[14] & 0xe0) >> 5;
		UINT32 pri_mask = ((0xff << (priority + 1)) & 0xff) | (1 << 31);

		sprite += sprite_bank * 256;
		color = color >> 1;

		sx += sprite_xoffs;
		sy -= sprite_yoffs;

		if (flipscreen)
		{
			sx -= 83;
			sy -= 3;
		}
		else
		{
			sx -= 6;
			sy--;
		}

		sy++;

		{
			sx &= 0x1ff;
			sy = ((sy + 16) & 0xff) - 16;

			sy -= 15;
			sx -= 67;

			UINT8 *gfxbase = DrvGfxROM2 + (sprite * 0x400);

			color = (color * 16);

			for (INT32 y = 0; y < sizey; y++, sy++) {
				if (sy < clip_min_y || sy >= clip_max_y) continue;

				UINT16 *dst = pTransDraw + (sy * nScreenWidth);
				UINT8 *pri = pPrioDraw + (sy * nScreenWidth);

				for (INT32 x = 0; x < sizex; x++, sx++) {
					if (sx < clip_min_x || sx >= clip_max_x) continue;

					INT32 xx = x;
					INT32 yy = y;

					if (flipx) xx = (sizex - 1) - x;
					if (flipy) yy = (sizey - 1) - y;

					INT32 pxl = gfxbase[xx + tx + ((yy + ty) * 32)];

					if (pxl != 0x0f) {
						if ((pri_mask & (1 << (pri[sx] & 0x1f))) == 0) {
							if (color == 0x7f0) {
								if (dst[sx] & 0x800) dst[sx] += 0x800;
							} else {
								dst[sx] = pxl + color;
							}
						}
						pri[sx] = 0x1f;
					}
				}

				sx -= sizex;
			}
		}

		source -= 0x10;
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPalette[0x2000] = 0; // black entry
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	//BurnTransferClear();

	for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
		pTransDraw[i] = 0x2000;
		pPrioDraw[i] = 0;
	}

	flipscreen = DrvSprRAM[0x0ff6] & 1;

	clip_min_x = DrvPalRegs[0] - 73 - 1;
	clip_max_x = DrvPalRegs[1] - 73 - 1;
	clip_min_y = DrvPalRegs[2] - 16 - 0x11;
	clip_max_y = DrvPalRegs[3] - 16 - 0x11;

	if (clip_min_x < 0) clip_min_x = 0;
	if (clip_max_x > nScreenWidth) clip_max_x = nScreenWidth;
	if (clip_min_y < 0) clip_min_y = 0;
	if (clip_max_y > nScreenHeight) clip_max_y = nScreenHeight;

	if ((clip_min_x >= clip_max_x) || (clip_min_y >= clip_max_y))
	{
		BurnTransferCopy(DrvPalette);
		return 0;
	}

	for (INT32 priority = 0; priority < 8; priority++)
	{
		for (INT32 i = 0; i < 6; i++)
		{
			if (DrvPfCtrl[0x10 + i] == priority) {
				if (nBurnLayer & (1 << i)) {
					draw_layer(i, priority);
				}
			}
		}
	}

	if (nSpriteEnable & 1) {
		draw_sprites();
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset(1);
	}

	watchdog++;
	if (watchdog >= 180) {
		bprintf (0, _T("Watchdog triggered!\n"));
		DrvDoReset(0);
	}

	M6809NewFrame();
	HD63701NewFrame();

	{
		memset (DrvInputs, 0xff, 7);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
			DrvInputs[4] ^= (DrvJoy5[i] & 1) << i;
			DrvInputs[5] ^= (DrvJoy6[i] & 1) << i;
			DrvInputs[6] ^= (DrvJoy7[i] & 1) << i;
		}

		if (quester) {
			if (DrvJoy1[0]) Paddle[0] += 0x04;
			if (DrvJoy1[1]) Paddle[0] -= 0x04;
			if (Paddle[0] >= 0x100) Paddle[0] = 0;
			if (Paddle[0] < 0) Paddle[0] = 0xfc;
			if (DrvJoy2[0]) Paddle[1] += 0x04;
			if (DrvJoy2[1]) Paddle[1] -= 0x04;
			if (Paddle[1] >= 0x100) Paddle[1] = 0;
			if (Paddle[1] < 0) Paddle[1] = 0xfc;
		}
	}

	INT32 nSegment;
	INT32 nInterleave = 640; // mame interleave
	INT32 S1VBL = ((nInterleave * 240) / 256);
	INT32 nSoundBufferPos = 0;
	INT32 nCyclesTotal[4] = { (INT32)((double)1536000 / 60.6060), (INT32)((double)1536000 / 60.6060), (INT32)((double)1536000 / 60.6060), (INT32)((double)1536000 / 60.6060) };
	if (sixtyhz) {
		nCyclesTotal[0] = nCyclesTotal[1] = nCyclesTotal[2] = nCyclesTotal[3] = 1536000 / 60;
	}
	INT32 nCyclesDone[4] = { 0, 0, 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		M6809Open(0);
		nCyclesDone[0] += M6809Run(nCyclesTotal[0] / nInterleave);
		if (i == S1VBL) M6809SetIRQLine(0, CPU_IRQSTATUS_ACK);
		nSegment = M6809TotalCycles();
		M6809Close();

		if (sub_cpu_in_reset == 0)
		{
			M6809Open(1);
			nCyclesDone[1] += M6809Run(nSegment - M6809TotalCycles());
			if (i == S1VBL) M6809SetIRQLine(0, CPU_IRQSTATUS_ACK);
			M6809Close();

			M6809Open(2);
			nCyclesDone[2] += M6809Run(nSegment - M6809TotalCycles());
			if (i == S1VBL) M6809SetIRQLine(0, CPU_IRQSTATUS_ACK);
			M6809Close();

			nCyclesDone[3] += HD63701Run(nSegment - nCyclesDone[3]);
			if (i == S1VBL) HD63701SetIRQLine(0, CPU_IRQSTATUS_ACK);
		}
		else
		{
			M6809Open(1);
			nCyclesDone[1] += M6809Idle(nSegment - M6809TotalCycles());
			M6809Close();

			M6809Open(2);
			nCyclesDone[2] += M6809Idle(nSegment - M6809TotalCycles());
			M6809Close();

			// HD63701
			nCyclesDone[3] += M6800Idle(nSegment - M6800TotalCycles());
		}

		if (i == S1VBL) {
			if (buffer_sprites) {
				for (INT32 s = 0; s < 0x800; s+= 16) {
					for (INT32 j = 10; j < 16; j++) {
						DrvSprRAM[0x800 + s + j] = DrvSprRAM[0x800 + s + j - 6];
					}
				}
				buffer_sprites = 0;
			}
		}

		if ((i % 8) == 7) {
			if (pBurnSoundOut) {
				nSegment = nBurnSoundLen / (nInterleave / 8);
				M6809Open(2);
				BurnYM2151Render(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
				M6809Close();
				nSoundBufferPos += nSegment;
			}
		}
	}

	M6809Open(2);

	if (pBurnSoundOut) {
		nSegment = nBurnSoundLen - nSoundBufferPos;
		if (nSegment > 0) {
			BurnYM2151Render(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
		}
		NamcoSoundUpdateStereo(pBurnSoundOut , nBurnSoundLen);

		DACUpdate(pBurnSoundOut, nBurnSoundLen);
	}

	M6809Close();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static void save_state_set_bank()
{
	for (INT32 j = 0; j < 2; j++)
	{
		M6809Open(j);

		for (INT32 i = 0; i < 8 * 2; i++)
		{
			INT32 bank = i / 2;
			INT32 a0 = i & 1;

			UINT32 data = bank_offsets[j][bank] / ((a0) ? 0x2000 : 0x200000);

			bankswitch(j, bank, a0, data & 0xff);
		}

		M6809Close();
	}
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029707;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		M6809Scan(nAction);
		HD63701Scan(nAction);

		M6809Open(2);
		NamcoSoundScan(nAction, pnMin);
		BurnYM2151Scan(nAction);
		M6809Close();

		SCAN_VAR(bank_offsets);
		SCAN_VAR(buffer_sprites);
		SCAN_VAR(sound_bank);
		SCAN_VAR(mcu_bank);
		SCAN_VAR(sub_cpu_reset);
		SCAN_VAR(shared_watchdog);
		SCAN_VAR(mcu_patch_data);
		SCAN_VAR(sub_cpu_in_reset);
		SCAN_VAR(coin_lockout);
		SCAN_VAR(namcos1_key);
		SCAN_VAR(namcos1_key_numerator_high_word);
		SCAN_VAR(namcos1_key_quotient);
		SCAN_VAR(namcos1_key_reminder);
		SCAN_VAR(input_count);
		SCAN_VAR(strobe_count);
		SCAN_VAR(stored_input);
		SCAN_VAR(dac0_value);
		SCAN_VAR(dac1_value);
		SCAN_VAR(dac0_gain);
		SCAN_VAR(dac1_gain);
	}

	if (nAction & ACB_WRITE) {
		save_state_set_bank();

		M6809Open(2);
		sound_bankswitch(sound_bank);
		M6809Close();

		mcu_bankswitch(mcu_bank);
	}

	if (nAction & ACB_NVRAM) {
		ba.Data		= DrvNVRAM;
		ba.nLen		= 0x000800;
		ba.szName	= "NV RAM";
		BurnAcb(&ba);
	}

	return 0;
}


// Shadowland (YD3)

static struct BurnRomInfo shadowldRomDesc[] = {
	{ "yd1_s0.bin",		0x10000, 0xa9cb51fb, 1 | BRF_PRG | BRF_ESS }, //  0 Sound m6809 Code
	{ "yd1_s1.bin",		0x10000, 0x65d1dc0d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "yd1_p0.bin",		0x10000, 0x07e49883, 2 | BRF_PRG | BRF_ESS }, //  2 Main and Sub m6809 Code
	{ "yd1_p1.bin",		0x10000, 0xa8ea6bd3, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "yd1_p2.bin",		0x10000, 0x62e5bbec, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "yd1_p3.bin",		0x10000, 0xa4f27c24, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "yd1_p5.bin",		0x10000, 0x29a78bd6, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "yd3_p6.bin",		0x10000, 0x93d6811c, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "yd3_p7.bin",		0x10000, 0xf1c271a0, 2 | BRF_PRG | BRF_ESS }, //  8

	{ "cus64-64a1.mcu",	0x01000, 0xffb5c0bd, 3 | BRF_PRG | BRF_ESS }, //  9 Internal MCU Code

	{ "yd_voi-0.bin",	0x20000, 0x448bc6a1, 4 | BRF_PRG | BRF_ESS }, // 10 External MCU Code
	{ "yd_voi-1.bin",	0x20000, 0x7809035c, 4 | BRF_PRG | BRF_ESS }, // 11
	{ "yd_voi-2.bin",	0x20000, 0x73bffc16, 4 | BRF_PRG | BRF_ESS }, // 12

	{ "yd_chr-8.bin",	0x20000, 0x0c8e69d0, 5 | BRF_GRA },           // 13 Character Pixel Masks

	{ "yd_chr-0.bin",	0x20000, 0x717441dd, 6 | BRF_GRA },           // 14 Characters
	{ "yd_chr-1.bin",	0x20000, 0xc1be6e35, 6 | BRF_GRA },           // 15
	{ "yd_chr-2.bin",	0x20000, 0x2df8d8cc, 6 | BRF_GRA },           // 16
	{ "yd_chr-3.bin",	0x20000, 0xd4e15c9e, 6 | BRF_GRA },           // 17
	{ "yd_chr-4.bin",	0x20000, 0xc0041e0d, 6 | BRF_GRA },           // 18
	{ "yd_chr-5.bin",	0x20000, 0x7b368461, 6 | BRF_GRA },           // 19
	{ "yd_chr-6.bin",	0x20000, 0x3ac6a90e, 6 | BRF_GRA },           // 20
	{ "yd_chr-7.bin",	0x20000, 0x8d2cffa5, 6 | BRF_GRA },           // 21

	{ "yd_obj-0.bin",	0x20000, 0xefb8efe3, 7 | BRF_GRA },           // 22 Sprites
	{ "yd_obj-1.bin",	0x20000, 0xbf4ee682, 7 | BRF_GRA },           // 23
	{ "yd_obj-2.bin",	0x20000, 0xcb721682, 7 | BRF_GRA },           // 24
	{ "yd_obj-3.bin",	0x20000, 0x8a6c3d1c, 7 | BRF_GRA },           // 25
	{ "yd_obj-4.bin",	0x20000, 0xef97bffb, 7 | BRF_GRA },           // 26
	{ "yd3_obj5.bin",	0x10000, 0x1e4aa460, 7 | BRF_GRA },           // 27
};

STD_ROM_PICK(shadowld)
STD_ROM_FN(shadowld)

static INT32 ShadowldInit()
{
	sixtyhz = 1;
	dac_kludge = 4;

	return DrvInit();
}

struct BurnDriver BurnDrvShadowld = {
	"shadowld", NULL, NULL, NULL, "1987",
	"Shadowland (YD3)\0", NULL, "Namco", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, shadowldRomInfo, shadowldRomName, NULL, NULL, DrvInputInfo, ShadowldDIPInfo,
	ShadowldInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};


// Yokai Douchuuki (Japan, new version (YD2, Rev B))

static struct BurnRomInfo youkaidk2RomDesc[] = {
	{ "yd1_s0.bin",		0x10000, 0xa9cb51fb, 1 | BRF_PRG | BRF_ESS }, //  0 Sound m6809 Code
	{ "yd1_s1.bin",		0x10000, 0x65d1dc0d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "yd1_p0.bin",		0x10000, 0x07e49883, 2 | BRF_PRG | BRF_ESS }, //  2 Main and Sub m6809 Code
	{ "yd1_p1.bin",		0x10000, 0xa8ea6bd3, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "yd1_p2.bin",		0x10000, 0x62e5bbec, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "yd1_p3.bin",		0x10000, 0xa4f27c24, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "yd1_p5.bin",		0x10000, 0x29a78bd6, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "yd1_p6.bin",		0x10000, 0x785a2772, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "yd2_p7b.bin",	0x10000, 0xa05bf3ae, 2 | BRF_PRG | BRF_ESS }, //  8

	{ "cus64-64a1.mcu",	0x01000, 0xffb5c0bd, 3 | BRF_PRG | BRF_ESS }, //  9 Internal MCU Code

	{ "yd_voi-0.bin",	0x20000, 0x448bc6a1, 4 | BRF_PRG | BRF_ESS }, // 10 External MCU Code
	{ "yd_voi-1.bin",	0x20000, 0x7809035c, 4 | BRF_PRG | BRF_ESS }, // 11
	{ "yd_voi-2.bin",	0x20000, 0x73bffc16, 4 | BRF_PRG | BRF_ESS }, // 12

	{ "yd_chr-8.bin",	0x20000, 0x0c8e69d0, 5 | BRF_GRA },           // 13 Character Pixel Masks

	{ "yd_chr-0.bin",	0x20000, 0x717441dd, 6 | BRF_GRA },           // 14 Characters
	{ "yd_chr-1.bin",	0x20000, 0xc1be6e35, 6 | BRF_GRA },           // 15
	{ "yd_chr-2.bin",	0x20000, 0x2df8d8cc, 6 | BRF_GRA },           // 16
	{ "yd_chr-3.bin",	0x20000, 0xd4e15c9e, 6 | BRF_GRA },           // 17
	{ "yd_chr-4.bin",	0x20000, 0xc0041e0d, 6 | BRF_GRA },           // 18
	{ "yd_chr-5.bin",	0x20000, 0x7b368461, 6 | BRF_GRA },           // 19
	{ "yd_chr-6.bin",	0x20000, 0x3ac6a90e, 6 | BRF_GRA },           // 20
	{ "yd_chr-7.bin",	0x20000, 0x8d2cffa5, 6 | BRF_GRA },           // 21

	{ "yd_obj-0.bin",	0x20000, 0xefb8efe3, 7 | BRF_GRA },           // 22 Sprites
	{ "yd_obj-1.bin",	0x20000, 0xbf4ee682, 7 | BRF_GRA },           // 23
	{ "yd_obj-2.bin",	0x20000, 0xcb721682, 7 | BRF_GRA },           // 24
	{ "yd_obj-3.bin",	0x20000, 0x8a6c3d1c, 7 | BRF_GRA },           // 25
	{ "yd_obj-4.bin",	0x20000, 0xef97bffb, 7 | BRF_GRA },           // 26
};

STD_ROM_PICK(youkaidk2)
STD_ROM_FN(youkaidk2)

struct BurnDriver BurnDrvYoukaidk2 = {
	"youkaidk2", "shadowld", NULL, NULL, "1987",
	"Yokai Douchuuki (Japan, new version (YD2, Rev B))\0", NULL, "Namco", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, youkaidk2RomInfo, youkaidk2RomName, NULL, NULL, DrvInputInfo, ShadowldDIPInfo,
	ShadowldInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};


// Yokai Douchuuki (Japan, old version (YD1))

static struct BurnRomInfo youkaidk1RomDesc[] = {
	{ "yd1.sd0",		0x10000, 0xa9cb51fb, 1 | BRF_PRG | BRF_ESS }, //  0 Sound m6809 Code
	{ "yd1.sd1",		0x10000, 0x65d1dc0d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "yd1_p0.bin",		0x10000, 0x07e49883, 2 | BRF_PRG | BRF_ESS }, //  2 Main and Sub m6809 Code
	{ "yd1_p1.bin",		0x10000, 0xa8ea6bd3, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "yd1_p2.bin",		0x10000, 0x62e5bbec, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "yd1_p3.bin",		0x10000, 0xa4f27c24, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "yd1_p5.bin",		0x10000, 0x29a78bd6, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "yd1_p6.bin",		0x10000, 0x785a2772, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "yd2_p7.bin",		0x10000, 0x3d39098c, 2 | BRF_PRG | BRF_ESS }, //  8

	{ "cus64-64a1.mcu",	0x01000, 0xffb5c0bd, 3 | BRF_PRG | BRF_ESS }, //  9 Internal MCU Code

	{ "yd_voi-0.bin",	0x20000, 0x448bc6a1, 4 | BRF_PRG | BRF_ESS }, // 10 External MCU Code
	{ "yd_voi-1.bin",	0x20000, 0x7809035c, 4 | BRF_PRG | BRF_ESS }, // 11
	{ "yd_voi-2.bin",	0x20000, 0x73bffc16, 4 | BRF_PRG | BRF_ESS }, // 12

	{ "yd_chr-8.bin",	0x20000, 0x0c8e69d0, 5 | BRF_GRA },           // 13 Character Pixel Masks

	{ "yd_chr-0.bin",	0x20000, 0x717441dd, 6 | BRF_GRA },           // 14 Characters
	{ "yd_chr-1.bin",	0x20000, 0xc1be6e35, 6 | BRF_GRA },           // 15
	{ "yd_chr-2.bin",	0x20000, 0x2df8d8cc, 6 | BRF_GRA },           // 16
	{ "yd_chr-3.bin",	0x20000, 0xd4e15c9e, 6 | BRF_GRA },           // 17
	{ "yd_chr-4.bin",	0x20000, 0xc0041e0d, 6 | BRF_GRA },           // 18
	{ "yd_chr-5.bin",	0x20000, 0x7b368461, 6 | BRF_GRA },           // 19
	{ "yd_chr-6.bin",	0x20000, 0x3ac6a90e, 6 | BRF_GRA },           // 20
	{ "yd_chr-7.bin",	0x20000, 0x8d2cffa5, 6 | BRF_GRA },           // 21

	{ "yd_obj-0.bin",	0x20000, 0xefb8efe3, 7 | BRF_GRA },           // 22 Sprites
	{ "yd_obj-1.bin",	0x20000, 0xbf4ee682, 7 | BRF_GRA },           // 23
	{ "yd_obj-2.bin",	0x20000, 0xcb721682, 7 | BRF_GRA },           // 24
	{ "yd_obj-3.bin",	0x20000, 0x8a6c3d1c, 7 | BRF_GRA },           // 25
	{ "yd_obj-4.bin",	0x20000, 0xef97bffb, 7 | BRF_GRA },           // 26
};

STD_ROM_PICK(youkaidk1)
STD_ROM_FN(youkaidk1)

struct BurnDriver BurnDrvYoukaidk1 = {
	"youkaidk1", "shadowld", NULL, NULL, "1987",
	"Yokai Douchuuki (Japan, old version (YD1))\0", NULL, "Namco", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, youkaidk1RomInfo, youkaidk1RomName, NULL, NULL, DrvInputInfo, ShadowldDIPInfo,
	ShadowldInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};



// Dragon Spirit (new version (DS3))

static struct BurnRomInfo dspiritRomDesc[] = {
	{ "ds1_s0.bin",		0x10000, 0x27100065, 1 | BRF_PRG | BRF_ESS }, //  0 Sound m6809 Code
	{ "ds1_s1.bin",		0x10000, 0xb398645f, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ds1_p0.bin",		0x10000, 0xb22a2856, 2 | BRF_PRG | BRF_ESS }, //  2 Main and Sub m6809 Code
	{ "ds1_p1.bin",		0x10000, 0xf7e3298a, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "ds1_p2.bin",		0x10000, 0x3c9b0100, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "ds1_p3.bin",		0x10000, 0xc6e5954b, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "ds1_p4.bin",		0x10000, 0xf3307870, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "ds1_p5.bin",		0x10000, 0x9a3a1028, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "ds3_p6.bin",		0x10000, 0xfcc01bd1, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "ds3_p7.bin",		0x10000, 0x820bedb2, 2 | BRF_PRG | BRF_ESS }, //  9

	{ "cus64-64a1.mcu",	0x01000, 0xffb5c0bd, 3 | BRF_PRG | BRF_ESS }, // 10 Internal MCU Code

	{ "ds1_v0.bin",		0x10000, 0x313b3508, 4 | BRF_PRG | BRF_ESS }, // 11 External MCU Code
	{ "ds_voi-1.bin",	0x20000, 0x54790d4e, 4 | BRF_PRG | BRF_ESS }, // 12
	{ "ds_voi-2.bin",	0x20000, 0x05298534, 4 | BRF_PRG | BRF_ESS }, // 13
	{ "ds_voi-3.bin",	0x20000, 0x13e84c7e, 4 | BRF_PRG | BRF_ESS }, // 14
	{ "ds_voi-4.bin",	0x20000, 0x34fbb8cd, 4 | BRF_PRG | BRF_ESS }, // 15

	{ "ds_chr-8.bin",	0x20000, 0x946eb242, 5 | BRF_GRA },           // 16 Character Pixel Masks

	{ "ds_chr-0.bin",	0x20000, 0x7bf28ac3, 6 | BRF_GRA },           // 17 Characters
	{ "ds_chr-1.bin",	0x20000, 0x03582fea, 6 | BRF_GRA },           // 18
	{ "ds_chr-2.bin",	0x20000, 0x5e05f4f9, 6 | BRF_GRA },           // 19
	{ "ds_chr-3.bin",	0x20000, 0xdc540791, 6 | BRF_GRA },           // 20
	{ "ds_chr-4.bin",	0x20000, 0xffd1f35c, 6 | BRF_GRA },           // 21
	{ "ds_chr-5.bin",	0x20000, 0x8472e0a3, 6 | BRF_GRA },           // 22
	{ "ds_chr-6.bin",	0x20000, 0xa799665a, 6 | BRF_GRA },           // 23
	{ "ds_chr-7.bin",	0x20000, 0xa51724af, 6 | BRF_GRA },           // 24

	{ "ds_obj-0.bin",	0x20000, 0x03ec3076, 7 | BRF_GRA },           // 25 Sprites
	{ "ds_obj-1.bin",	0x20000, 0xe67a8fa4, 7 | BRF_GRA },           // 26
	{ "ds_obj-2.bin",	0x20000, 0x061cd763, 7 | BRF_GRA },           // 27
	{ "ds_obj-3.bin",	0x20000, 0x63225a09, 7 | BRF_GRA },           // 28
	{ "ds1_o4.bin",		0x10000, 0xa6246fcb, 7 | BRF_GRA },           // 29
};

STD_ROM_PICK(dspirit)
STD_ROM_FN(dspirit)

static INT32 DspiritInit()
{
	namcos1_key_init(1, 0x036, -1, -1, -1, -1, -1, -1);

	return DrvInit();
}

struct BurnDriver BurnDrvDspirit = {
	"dspirit", NULL, NULL, NULL, "1987",
	"Dragon Spirit (new version (DS3))\0", NULL, "Namco", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, dspiritRomInfo, dspiritRomName, NULL, NULL, DrvInputInfo, DspiritDIPInfo,
	DspiritInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	224, 288, 3, 4
};


// Dragon Spirit (DS2)

static struct BurnRomInfo dspirit2RomDesc[] = {
	{ "ds1_s0.bin",		0x10000, 0x27100065, 1 | BRF_PRG | BRF_ESS }, //  0 Sound m6809 Code
	{ "ds1_s1.bin",		0x10000, 0xb398645f, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ds1_p0.bin",		0x10000, 0xb22a2856, 2 | BRF_PRG | BRF_ESS }, //  2 Main and Sub m6809 Code
	{ "ds1_p1.bin",		0x10000, 0xf7e3298a, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "ds1_p2.bin",		0x10000, 0x3c9b0100, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "ds1_p3.bin",		0x10000, 0xc6e5954b, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "ds1_p4.bin",		0x10000, 0xf3307870, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "ds1_p5.bin",		0x10000, 0x9a3a1028, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "ds2-pr6d.s11",	0x10000, 0x5382447d, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "ds2-pr7d.t11",	0x10000, 0x80ff492a, 2 | BRF_PRG | BRF_ESS }, //  9

	{ "cus64-64a1.mcu",	0x01000, 0xffb5c0bd, 3 | BRF_PRG | BRF_ESS }, // 10 Internal MCU Code

	{ "ds1_v0.bin",		0x10000, 0x313b3508, 4 | BRF_PRG | BRF_ESS }, // 11 External MCU Code
	{ "ds_voi-1.bin",	0x20000, 0x54790d4e, 4 | BRF_PRG | BRF_ESS }, // 12
	{ "ds_voi-2.bin",	0x20000, 0x05298534, 4 | BRF_PRG | BRF_ESS }, // 13
	{ "ds_voi-3.bin",	0x20000, 0x13e84c7e, 4 | BRF_PRG | BRF_ESS }, // 14
	{ "ds_voi-4.bin",	0x20000, 0x34fbb8cd, 4 | BRF_PRG | BRF_ESS }, // 15

	{ "ds_chr-8.bin",	0x20000, 0x946eb242, 5 | BRF_GRA },           // 16 Character Pixel Masks

	{ "ds_chr-0.bin",	0x20000, 0x7bf28ac3, 6 | BRF_GRA },           // 17 Characters
	{ "ds_chr-1.bin",	0x20000, 0x03582fea, 6 | BRF_GRA },           // 18
	{ "ds_chr-2.bin",	0x20000, 0x5e05f4f9, 6 | BRF_GRA },           // 19
	{ "ds_chr-3.bin",	0x20000, 0xdc540791, 6 | BRF_GRA },           // 20
	{ "ds_chr-4.bin",	0x20000, 0xffd1f35c, 6 | BRF_GRA },           // 21
	{ "ds_chr-5.bin",	0x20000, 0x8472e0a3, 6 | BRF_GRA },           // 22
	{ "ds_chr-6.bin",	0x20000, 0xa799665a, 6 | BRF_GRA },           // 23
	{ "ds_chr-7.bin",	0x20000, 0xa51724af, 6 | BRF_GRA },           // 24

	{ "ds_obj-0.bin",	0x20000, 0x03ec3076, 7 | BRF_GRA },           // 25 Sprites
	{ "ds_obj-1.bin",	0x20000, 0xe67a8fa4, 7 | BRF_GRA },           // 26
	{ "ds_obj-2.bin",	0x20000, 0x061cd763, 7 | BRF_GRA },           // 27
	{ "ds_obj-3.bin",	0x20000, 0x63225a09, 7 | BRF_GRA },           // 28
	{ "ds1_o4.bin",		0x10000, 0xa6246fcb, 7 | BRF_GRA },           // 29
};

STD_ROM_PICK(dspirit2)
STD_ROM_FN(dspirit2)

struct BurnDriver BurnDrvDspirit2 = {
	"dspirit2", "dspirit", NULL, NULL, "1987",
	"Dragon Spirit (DS2)\0", NULL, "Namco", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, dspirit2RomInfo, dspirit2RomName, NULL, NULL, DrvInputInfo, DspiritDIPInfo,
	DspiritInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	224, 288, 3, 4
};


// Dragon Spirit (old version (DS1))

static struct BurnRomInfo dspirit1RomDesc[] = {
	{ "ds1_s0.bin",		0x10000, 0x27100065, 1 | BRF_PRG | BRF_ESS }, //  0 Sound m6809 Code
	{ "ds1_s1.bin",		0x10000, 0xb398645f, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ds1_p0.bin",		0x10000, 0xb22a2856, 2 | BRF_PRG | BRF_ESS }, //  2 Main and Sub m6809 Code
	{ "ds1_p1.bin",		0x10000, 0xf7e3298a, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "ds1_p2.bin",		0x10000, 0x3c9b0100, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "ds1_p3.bin",		0x10000, 0xc6e5954b, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "ds1_p4.bin",		0x10000, 0xf3307870, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "ds1_p5.bin",		0x10000, 0x9a3a1028, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "ds1_p6.bin",		0x10000, 0xa82737b4, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "ds1_p7.bin",		0x10000, 0xf4c0d75e, 2 | BRF_PRG | BRF_ESS }, //  9

	{ "cus64-64a1.mcu",	0x01000, 0xffb5c0bd, 3 | BRF_PRG | BRF_ESS }, // 10 Internal MCU Code

	{ "ds1_v0.bin",		0x10000, 0x313b3508, 4 | BRF_PRG | BRF_ESS }, // 11 External MCU Code
	{ "ds_voi-1.bin",	0x20000, 0x54790d4e, 4 | BRF_PRG | BRF_ESS }, // 12
	{ "ds_voi-2.bin",	0x20000, 0x05298534, 4 | BRF_PRG | BRF_ESS }, // 13
	{ "ds_voi-3.bin",	0x20000, 0x13e84c7e, 4 | BRF_PRG | BRF_ESS }, // 14
	{ "ds_voi-4.bin",	0x20000, 0x34fbb8cd, 4 | BRF_PRG | BRF_ESS }, // 15

	{ "ds_chr-8.bin",	0x20000, 0x946eb242, 5 | BRF_GRA },           // 16 Character Pixel Masks

	{ "ds_chr-0.bin",	0x20000, 0x7bf28ac3, 6 | BRF_GRA },           // 17 Characters
	{ "ds_chr-1.bin",	0x20000, 0x03582fea, 6 | BRF_GRA },           // 18
	{ "ds_chr-2.bin",	0x20000, 0x5e05f4f9, 6 | BRF_GRA },           // 19
	{ "ds_chr-3.bin",	0x20000, 0xdc540791, 6 | BRF_GRA },           // 20
	{ "ds_chr-4.bin",	0x20000, 0xffd1f35c, 6 | BRF_GRA },           // 21
	{ "ds_chr-5.bin",	0x20000, 0x8472e0a3, 6 | BRF_GRA },           // 22
	{ "ds_chr-6.bin",	0x20000, 0xa799665a, 6 | BRF_GRA },           // 23
	{ "ds_chr-7.bin",	0x20000, 0xa51724af, 6 | BRF_GRA },           // 24

	{ "ds_obj-0.bin",	0x20000, 0x03ec3076, 7 | BRF_GRA },           // 25 Sprites
	{ "ds_obj-1.bin",	0x20000, 0xe67a8fa4, 7 | BRF_GRA },           // 26
	{ "ds_obj-2.bin",	0x20000, 0x061cd763, 7 | BRF_GRA },           // 27
	{ "ds_obj-3.bin",	0x20000, 0x63225a09, 7 | BRF_GRA },           // 28
	{ "ds1_o4.bin",		0x10000, 0xa6246fcb, 7 | BRF_GRA },           // 29
};

STD_ROM_PICK(dspirit1)
STD_ROM_FN(dspirit1)

struct BurnDriver BurnDrvDspirit1 = {
	"dspirit1", "dspirit", NULL, NULL, "1987",
	"Dragon Spirit (old version (DS1))\0", NULL, "Namco", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, dspirit1RomInfo, dspirit1RomName, NULL, NULL, DrvInputInfo, DspiritDIPInfo,
	DspiritInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	224, 288, 3, 4
};


// Blazer (Japan)

static struct BurnRomInfo blazerRomDesc[] = {
	{ "bz1_s0.bin",		0x10000, 0x6c3a580b, 1 | BRF_PRG | BRF_ESS }, //  0 Sound m6809 Code

	{ "bz1_p0.bin",		0x10000, 0xa7dd195b, 2 | BRF_PRG | BRF_ESS }, //  1 Main and Sub m6809 Code
	{ "bz1_p1.bin",		0x10000, 0xc54bbbf4, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "bz1_p2.bin",		0x10000, 0x5d700aed, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "bz1_p3.bin",		0x10000, 0x81b32a1a, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "bz_prg-4.bin",	0x20000, 0x65ef6f05, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "bz_prg-5.bin",	0x20000, 0x900da191, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "bz_prg-6.bin",	0x20000, 0x81c48fc0, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "bz1_p7.bin",		0x10000, 0x2d4cbb95, 2 | BRF_PRG | BRF_ESS }, //  8

	{ "cus64-64a1.mcu",	0x01000, 0xffb5c0bd, 3 | BRF_PRG | BRF_ESS }, //  9 Internal MCU Code

	{ "bz1_v0.bin",		0x10000, 0x3d09d32e, 4 | BRF_PRG | BRF_ESS }, // 10 External MCU Code
	{ "bz_voi-1.bin",	0x20000, 0x2043b141, 4 | BRF_PRG | BRF_ESS }, // 11
	{ "bz_voi-2.bin",	0x20000, 0x64143442, 4 | BRF_PRG | BRF_ESS }, // 12
	{ "bz_voi-3.bin",	0x20000, 0x26cfc510, 4 | BRF_PRG | BRF_ESS }, // 13
	{ "bz_voi-4.bin",	0x20000, 0xd206b1bd, 4 | BRF_PRG | BRF_ESS }, // 14

	{ "bz_chr-8.bin",	0x20000, 0xdb28bfca, 5 | BRF_GRA },           // 15 Character Pixel Masks

	{ "bz_chr-0.bin",	0x20000, 0xd346ba61, 6 | BRF_GRA },           // 16 Characters
	{ "bz_chr-1.bin",	0x20000, 0xe45eb2ea, 6 | BRF_GRA },           // 17
	{ "bz_chr-2.bin",	0x20000, 0x599079ee, 6 | BRF_GRA },           // 18
	{ "bz_chr-3.bin",	0x20000, 0xd5182e36, 6 | BRF_GRA },           // 19
	{ "bz_chr-4.bin",	0x20000, 0xe788259e, 6 | BRF_GRA },           // 20
	{ "bz_chr-5.bin",	0x20000, 0x107e6814, 6 | BRF_GRA },           // 21
	{ "bz_chr-6.bin",	0x20000, 0x0312e2ba, 6 | BRF_GRA },           // 22
	{ "bz_chr-7.bin",	0x20000, 0xd9d9a90f, 6 | BRF_GRA },           // 23

	{ "bz_obj-0.bin",	0x20000, 0x22aee927, 7 | BRF_GRA },           // 24 Sprites
	{ "bz_obj-1.bin",	0x20000, 0x7cb10112, 7 | BRF_GRA },           // 25
	{ "bz_obj-2.bin",	0x20000, 0x34b23bb7, 7 | BRF_GRA },           // 26
	{ "bz_obj-3.bin",	0x20000, 0x9bc1db71, 7 | BRF_GRA },           // 27
};

STD_ROM_PICK(blazer)
STD_ROM_FN(blazer)

static INT32 BlazerInit()
{
	namcos1_key_init(1, 0x013, -1, -1, -1, -1, -1, -1);

	INT32 nRet = DrvInit();

	dac_kludge = 4;

	if (nRet == 0) {
		// transparent sprites in empty space, otherwise the explosions look bad.
		memset (DrvGfxROM2 + 0x100000, 0xf, 0x100000);
	}

	return nRet;
}

struct BurnDriver BurnDrvBlazer = {
	"blazer", NULL, NULL, NULL, "1987",
	"Blazer (Japan)\0", NULL, "Namco", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, blazerRomInfo, blazerRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	BlazerInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	224, 288, 3, 4
};


// Quester (Japan)

static struct BurnRomInfo questerRomDesc[] = {
	{ "qs1_s0.bin",		0x10000, 0xc2ef3af9, 1 | BRF_PRG | BRF_ESS }, //  0 Sound m6809 Code

	{ "qs1_p5.bin",		0x10000, 0xc8e11f30, 2 | BRF_PRG | BRF_ESS }, //  1 Main and Sub m6809 Code
	{ "qs1_p7b.bin",	0x10000, 0xf358a944, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "cus64-64a1.mcu",	0x01000, 0xffb5c0bd, 3 | BRF_PRG | BRF_ESS }, //  3 Internal MCU Code

	{ "qs1_v0.bin",		0x10000, 0x6a2f3038, 4 | BRF_PRG | BRF_ESS }, //  4 External MCU Code

	{ "qs1_c8.bin",		0x10000, 0x06730d54, 5 | BRF_GRA },           //  5 Character Pixel Masks

	{ "qs1_c0.bin",		0x20000, 0xca69bd7a, 6 | BRF_GRA },           //  6 Characters
	{ "qs1_c1.bin",		0x20000, 0xd660ba71, 6 | BRF_GRA },           //  7
	{ "qs1_c2.bin",		0x20000, 0x4686f656, 6 | BRF_GRA },           //  8

	{ "qs1_o0.bin",		0x10000, 0xe24f0bf1, 7 | BRF_GRA },           //  9 Sprites
	{ "qs1_o1.bin",		0x10000, 0xe4aab0ca, 7 | BRF_GRA },           // 10
};

STD_ROM_PICK(quester)
STD_ROM_FN(quester)

static INT32 QuesterInit()
{
	input_read_callback = quester_paddle_read;
	quester = 1;

	return DrvInit();
}

struct BurnDriver BurnDrvQuester = {
	"quester", NULL, NULL, NULL, "1987",
	"Quester (Japan)\0", NULL, "Namco", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_BREAKOUT, 0,
	NULL, questerRomInfo, questerRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo, //QuesterInputInfo, QuesterDIPInfo,
	QuesterInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	224, 288, 3, 4
};


// Quester Special Edition (Japan)

static struct BurnRomInfo questersRomDesc[] = {
	{ "qs1_s0.bin",		0x10000, 0xc2ef3af9, 1 | BRF_PRG | BRF_ESS }, //  0 Sound m6809 Code

	{ "qs2_p5.bin",		0x10000, 0x15661fe7, 2 | BRF_PRG | BRF_ESS }, //  1 Main and Sub m6809 Code
	{ "qs2_p6.bin",		0x10000, 0x19e0fc20, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "qs2_p7.bin",		0x10000, 0x4f6ad716, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "cus64-64a1.mcu",	0x01000, 0xffb5c0bd, 3 | BRF_PRG | BRF_ESS }, //  4 Internal MCU Code

	{ "qs1_v0.bin",		0x10000, 0x6a2f3038, 4 | BRF_PRG | BRF_ESS }, //  5 External MCU Code

	{ "qs1_c8.bin",		0x10000, 0x06730d54, 5 | BRF_GRA },           //  6 Character Pixel Masks

	{ "qs1_c0.bin",		0x20000, 0xca69bd7a, 6 | BRF_GRA },           //  7 Characters
	{ "qs1_c1.bin",		0x20000, 0xd660ba71, 6 | BRF_GRA },           //  8
	{ "qs1_c2.bin",		0x20000, 0x4686f656, 6 | BRF_GRA },           //  9

	{ "qs1_o0.bin",		0x10000, 0xe24f0bf1, 7 | BRF_GRA },           // 10 Sprites
	{ "qs1_o1.bin",		0x10000, 0xe4aab0ca, 7 | BRF_GRA },           // 11
};

STD_ROM_PICK(questers)
STD_ROM_FN(questers)

struct BurnDriver BurnDrvQuesters = {
	"questers", "quester", NULL, NULL, "1987",
	"Quester Special Edition (Japan)\0", NULL, "Namco", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_BREAKOUT, 0,
	NULL, questersRomInfo, questersRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo, //QuesterInputInfo, QuesterDIPInfo,
	QuesterInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	224, 288, 3, 4
};


// Pac-Mania

static struct BurnRomInfo pacmaniaRomDesc[] = {
	{ "pn2_s0.bin",		0x10000, 0xc10370fa, 1 | BRF_PRG | BRF_ESS }, //  0 Sound m6809 Code
	{ "pn2_s1.bin",		0x10000, 0xf761ed5a, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "pn_prg-6.bin",	0x20000, 0xfe94900c, 2 | BRF_PRG | BRF_ESS }, //  2 Main and Sub m6809 Code
	{ "pn2_p7.bin",		0x10000, 0x462fa4fd, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "cus64-64a1.mcu",	0x01000, 0xffb5c0bd, 3 | BRF_PRG | BRF_ESS }, //  4 Internal MCU Code

	{ "pn2_v0.bin",		0x10000, 0x1ad5788f, 4 | BRF_PRG | BRF_ESS }, //  5 External MCU Code

	{ "pn2_c8.bin",		0x10000, 0xf3afd65d, 5 | BRF_GRA },           //  6 Character Pixel Masks

	{ "pn_chr-0.bin",	0x20000, 0x7c57644c, 6 | BRF_GRA },           //  7 Characters
	{ "pn_chr-1.bin",	0x20000, 0x7eaa67ed, 6 | BRF_GRA },           //  8
	{ "pn_chr-2.bin",	0x20000, 0x27e739ac, 6 | BRF_GRA },           //  9
	{ "pn_chr-3.bin",	0x20000, 0x1dfda293, 6 | BRF_GRA },           // 10

	{ "pn_obj-0.bin",	0x20000, 0xfda57e8b, 7 | BRF_GRA },           // 11 Sprites
	{ "pnx_obj1.bin",	0x20000, 0x4c08affe, 7 | BRF_GRA },           // 12
};

STD_ROM_PICK(pacmania)
STD_ROM_FN(pacmania)

static INT32 PacmaniaInit()
{
	namcos1_key_init(2, 0x012, -1, -1, -1, -1, -1, -1);

	return DrvInit();
}

struct BurnDriver BurnDrvPacmania = {
	"pacmania", NULL, NULL, NULL, "1987",
	"Pac-Mania\0", NULL, "Namco", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, pacmaniaRomInfo, pacmaniaRomName, NULL, NULL, DrvInputInfo, PacmaniaDIPInfo,
	PacmaniaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	224, 288, 3, 4
};


// Pac-Mania (111187 sound program)

static struct BurnRomInfo pacmaniaoRomDesc[] = {
	{ "pac-mania_111187.sound0",	0x10000, 0x845d6a2e, 1 | BRF_PRG | BRF_ESS }, //  0 Sound m6809 Code
	{ "pac-mania_111187.sound1",	0x10000, 0x411bc134, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "pn_prg-6.bin",		0x20000, 0xfe94900c, 2 | BRF_PRG | BRF_ESS }, //  2 Main and Sub m6809 Code
	{ "pn2_p7.bin",			0x10000, 0x462fa4fd, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "cus64-64a1.mcu",		0x01000, 0xffb5c0bd, 3 | BRF_PRG | BRF_ESS }, //  4 Internal MCU Code

	{ "pac-mania_111187.voice0",	0x10000, 0x1ad5788f, 4 | BRF_PRG | BRF_ESS }, //  5 External MCU Code

	{ "pn1_c8.bin",			0x10000, 0xf3afd65d, 5 | BRF_GRA },           //  6 Character Pixel Masks

	{ "pn_chr-0.bin",		0x20000, 0x7c57644c, 6 | BRF_GRA },           //  7 Characters
	{ "pn_chr-1.bin",		0x20000, 0x7eaa67ed, 6 | BRF_GRA },           //  8
	{ "pn_chr-2.bin",		0x20000, 0x27e739ac, 6 | BRF_GRA },           //  9
	{ "pn_chr-3.bin",		0x20000, 0x1dfda293, 6 | BRF_GRA },           // 10

	{ "pn_obj-0.bin",		0x20000, 0xfda57e8b, 7 | BRF_GRA },           // 11 Sprites
	{ "pnx_obj1.bin",		0x20000, 0x4c08affe, 7 | BRF_GRA },           // 12
};

STD_ROM_PICK(pacmaniao)
STD_ROM_FN(pacmaniao)

struct BurnDriver BurnDrvPacmaniao = {
	"pacmaniao", "pacmania", NULL, NULL, "1987",
	"Pac-Mania (111187 sound program)\0", NULL, "Namco", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, pacmaniaoRomInfo, pacmaniaoRomName, NULL, NULL, DrvInputInfo, PacmaniaDIPInfo,
	PacmaniaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	224, 288, 3, 4
};


// Pac-Mania (Japan)

static struct BurnRomInfo pacmaniajRomDesc[] = {
	{ "pn1_s0.bin",		0x10000, 0xd5ef5eee, 1 | BRF_PRG | BRF_ESS }, //  0 Sound m6809 Code
	{ "pn1_s1.bin",		0x10000, 0x411bc134, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "pn_prg-6.bin",	0x20000, 0xfe94900c, 2 | BRF_PRG | BRF_ESS }, //  2 Main and Sub m6809 Code
	{ "pn1_p7.bin",		0x10000, 0x2aa99e2b, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "cus64-64a1.mcu",	0x01000, 0xffb5c0bd, 3 | BRF_PRG | BRF_ESS }, //  4 Internal MCU Code

	{ "pn1_v0.bin",		0x10000, 0xe2689f79, 4 | BRF_PRG | BRF_ESS }, //  5 External MCU Code

	{ "pn1_c8.bin",		0x10000, 0xf3afd65d, 5 | BRF_GRA },           //  6 Character Pixel Masks

	{ "pn_chr-0.bin",	0x20000, 0x7c57644c, 6 | BRF_GRA },           //  7 Characters
	{ "pn_chr-1.bin",	0x20000, 0x7eaa67ed, 6 | BRF_GRA },           //  8
	{ "pn_chr-2.bin",	0x20000, 0x27e739ac, 6 | BRF_GRA },           //  9
	{ "pn_chr-3.bin",	0x20000, 0x1dfda293, 6 | BRF_GRA },           // 10

	{ "pn_obj-0.bin",	0x20000, 0xfda57e8b, 7 | BRF_GRA },           // 11 Sprites
	{ "pn_obj-1.bin",	0x20000, 0x27bdf440, 7 | BRF_GRA },           // 12
};

STD_ROM_PICK(pacmaniaj)
STD_ROM_FN(pacmaniaj)

struct BurnDriver BurnDrvPacmaniaj = {
	"pacmaniaj", "pacmania", NULL, NULL, "1987",
	"Pac-Mania (Japan)\0", NULL, "Namco", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, pacmaniajRomInfo, pacmaniajRomName, NULL, NULL, DrvInputInfo, PacmaniaDIPInfo,
	PacmaniaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	224, 288, 3, 4
};


// Galaga '88

static struct BurnRomInfo galaga88RomDesc[] = {
	{ "g81_s0.bin",		0x10000, 0x164a3fdc, 1 | BRF_PRG | BRF_ESS }, //  0 Sound m6809 Code
	{ "g81_s1.bin",		0x10000, 0x16a4b784, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "g81_p0.bin",		0x10000, 0x0f0778ca, 2 | BRF_PRG | BRF_ESS }, //  2 Main and Sub m6809 Code
	{ "g81_p1.bin",		0x10000, 0xe68cb351, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "g81_p5.bin",		0x10000, 0x4fbd3f6c, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "g8x_p6.bin",		0x10000, 0x403d01c1, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "g8x_p7.bin",		0x10000, 0xdf75b7fc, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "cus64-64a1.mcu",	0x01000, 0xffb5c0bd, 3 | BRF_PRG | BRF_ESS }, //  7 Internal MCU Code

	{ "g81_v0.bin",		0x10000, 0x86921dd4, 4 | BRF_PRG | BRF_ESS }, //  8 External MCU Code
	{ "g81_v1.bin",		0x10000, 0x9c300e16, 4 | BRF_PRG | BRF_ESS }, //  9
	{ "g81_v2.bin",		0x10000, 0x5316b4b0, 4 | BRF_PRG | BRF_ESS }, // 10
	{ "g81_v3.bin",		0x10000, 0xdc077af4, 4 | BRF_PRG | BRF_ESS }, // 11
	{ "g81_v4.bin",		0x10000, 0xac0279a7, 4 | BRF_PRG | BRF_ESS }, // 12
	{ "g81_v5.bin",		0x10000, 0x014ddba1, 4 | BRF_PRG | BRF_ESS }, // 13

	{ "g8_chr-8.bin",	0x20000, 0x3862ed0a, 5 | BRF_GRA },           // 14 Character Pixel Masks

	{ "g8_chr-0.bin",	0x20000, 0x68559c78, 6 | BRF_GRA },           // 15 Characters
	{ "g8_chr-1.bin",	0x20000, 0x3dc0f93f, 6 | BRF_GRA },           // 16
	{ "g8_chr-2.bin",	0x20000, 0xdbf26f1f, 6 | BRF_GRA },           // 17
	{ "g8_chr-3.bin",	0x20000, 0xf5d6cac5, 6 | BRF_GRA },           // 18

	{ "g8_obj-0.bin",	0x20000, 0xd7112e3f, 7 | BRF_GRA },           // 19 Sprites
	{ "g8_obj-1.bin",	0x20000, 0x680db8e7, 7 | BRF_GRA },           // 20
	{ "g8_obj-2.bin",	0x20000, 0x13c97512, 7 | BRF_GRA },           // 21
	{ "g8_obj-3.bin",	0x20000, 0x3ed3941b, 7 | BRF_GRA },           // 22
	{ "g8_obj-4.bin",	0x20000, 0x370ff4ad, 7 | BRF_GRA },           // 23
	{ "g8_obj-5.bin",	0x20000, 0xb0645169, 7 | BRF_GRA },           // 24
};

STD_ROM_PICK(galaga88)
STD_ROM_FN(galaga88)

static INT32 Galaga88Init()
{
	namcos1_key_init(2, 0x031, -1, -1, -1, -1, -1, -1);

	return DrvInit();
}

struct BurnDriver BurnDrvGalaga88 = {
	"galaga88", NULL, NULL, NULL, "1987",
	"Galaga '88\0", NULL, "Namco", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, galaga88RomInfo, galaga88RomName, NULL, NULL, DrvInputInfo, Galaga88DIPInfo,
	Galaga88Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	224, 288, 3, 4
};


// Galaga '88 (Japan)

static struct BurnRomInfo galaga88jRomDesc[] = {
	{ "g81_s0.bin",		0x10000, 0x164a3fdc, 1 | BRF_PRG | BRF_ESS }, //  0 Sound m6809 Code
	{ "g81_s1.bin",		0x10000, 0x16a4b784, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "g81_p0.bin",		0x10000, 0x0f0778ca, 2 | BRF_PRG | BRF_ESS }, //  2 Main and Sub m6809 Code
	{ "g81_p1.bin",		0x10000, 0xe68cb351, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "g81_p5.bin",		0x10000, 0x4fbd3f6c, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "g81_p6.bin",		0x10000, 0xe7203707, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "g81_p7.bin",		0x10000, 0x7c10965d, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "cus64-64a1.mcu",	0x01000, 0xffb5c0bd, 3 | BRF_PRG | BRF_ESS }, //  7 Internal MCU Code

	{ "g81_v0.bin",		0x10000, 0x86921dd4, 4 | BRF_PRG | BRF_ESS }, //  8 External MCU Code
	{ "g81_v1.bin",		0x10000, 0x9c300e16, 4 | BRF_PRG | BRF_ESS }, //  9
	{ "g81_v2.bin",		0x10000, 0x5316b4b0, 4 | BRF_PRG | BRF_ESS }, // 10
	{ "g81_v3.bin",		0x10000, 0xdc077af4, 4 | BRF_PRG | BRF_ESS }, // 11
	{ "g81_v4.bin",		0x10000, 0xac0279a7, 4 | BRF_PRG | BRF_ESS }, // 12
	{ "g81_v5.bin",		0x10000, 0x014ddba1, 4 | BRF_PRG | BRF_ESS }, // 13

	{ "g8_chr-8.bin",	0x20000, 0x3862ed0a, 5 | BRF_GRA },           // 14 Character Pixel Masks

	{ "g8_chr-0.bin",	0x20000, 0x68559c78, 6 | BRF_GRA },           // 15 Characters
	{ "g8_chr-1.bin",	0x20000, 0x3dc0f93f, 6 | BRF_GRA },           // 16
	{ "g8_chr-2.bin",	0x20000, 0xdbf26f1f, 6 | BRF_GRA },           // 17
	{ "g8_chr-3.bin",	0x20000, 0xf5d6cac5, 6 | BRF_GRA },           // 18

	{ "g8_obj-0.bin",	0x20000, 0xd7112e3f, 7 | BRF_GRA },           // 19 Sprites
	{ "g8_obj-1.bin",	0x20000, 0x680db8e7, 7 | BRF_GRA },           // 20
	{ "g8_obj-2.bin",	0x20000, 0x13c97512, 7 | BRF_GRA },           // 21
	{ "g8_obj-3.bin",	0x20000, 0x3ed3941b, 7 | BRF_GRA },           // 22
	{ "g8_obj-4.bin",	0x20000, 0x370ff4ad, 7 | BRF_GRA },           // 23
	{ "g8_obj-5.bin",	0x20000, 0xb0645169, 7 | BRF_GRA },           // 24
};

STD_ROM_PICK(galaga88j)
STD_ROM_FN(galaga88j)

struct BurnDriver BurnDrvGalaga88j = {
	"galaga88j", "galaga88", NULL, NULL, "1987",
	"Galaga '88 (Japan)\0", NULL, "Namco", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, galaga88jRomInfo, galaga88jRomName, NULL, NULL, DrvInputInfo, Galaga88DIPInfo,
	Galaga88Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	224, 288, 3, 4
};


// Galaga '88 (02-03-88)

static struct BurnRomInfo galaga88aRomDesc[] = {
	{ "g81_s0.bin",		0x10000, 0x164a3fdc, 1 | BRF_PRG | BRF_ESS }, //  0 Sound m6809 Code
	{ "g81_s1.bin",		0x10000, 0x16a4b784, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "g81_p0.bin",		0x10000, 0x0f0778ca, 2 | BRF_PRG | BRF_ESS }, //  2 Main and Sub m6809 Code
	{ "g81_p1.bin",		0x10000, 0xe68cb351, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "g81_p5.bin",		0x10000, 0x4fbd3f6c, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "prg-6c.s10",		0x10000, 0xc781b8b5, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "prg-7c.t10",		0x10000, 0xd2d7e4fa, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "cus64-64a1.mcu",	0x01000, 0xffb5c0bd, 3 | BRF_PRG | BRF_ESS }, //  7 Internal MCU Code

	{ "g81_v0.bin",		0x10000, 0x86921dd4, 4 | BRF_PRG | BRF_ESS }, //  8 External MCU Code
	{ "g81_v1.bin",		0x10000, 0x9c300e16, 4 | BRF_PRG | BRF_ESS }, //  9
	{ "g81_v2.bin",		0x10000, 0x5316b4b0, 4 | BRF_PRG | BRF_ESS }, // 10
	{ "g81_v3.bin",		0x10000, 0xdc077af4, 4 | BRF_PRG | BRF_ESS }, // 11
	{ "g81_v4.bin",		0x10000, 0xac0279a7, 4 | BRF_PRG | BRF_ESS }, // 12
	{ "g81_v5.bin",		0x10000, 0x014ddba1, 4 | BRF_PRG | BRF_ESS }, // 13

	{ "g8_chr-8.bin",	0x20000, 0x3862ed0a, 5 | BRF_GRA },           // 14 Character Pixel Masks

	{ "g8_chr-0.bin",	0x20000, 0x68559c78, 6 | BRF_GRA },           // 15 Characters
	{ "g8_chr-1.bin",	0x20000, 0x3dc0f93f, 6 | BRF_GRA },           // 16
	{ "g8_chr-2.bin",	0x20000, 0xdbf26f1f, 6 | BRF_GRA },           // 17
	{ "g8_chr-3.bin",	0x20000, 0xf5d6cac5, 6 | BRF_GRA },           // 18
	{ "g8chr-7.m8",		0x20000, 0x5f655016, 6 | BRF_GRA },           // 19

	{ "g8_obj-0.bin",	0x20000, 0xd7112e3f, 7 | BRF_GRA },           // 20 Sprites
	{ "g8_obj-1.bin",	0x20000, 0x680db8e7, 7 | BRF_GRA },           // 21
	{ "g8_obj-2.bin",	0x20000, 0x13c97512, 7 | BRF_GRA },           // 22
	{ "g8_obj-3.bin",	0x20000, 0x3ed3941b, 7 | BRF_GRA },           // 23
	{ "g8_obj-4.bin",	0x20000, 0x370ff4ad, 7 | BRF_GRA },           // 24
	{ "g8_obj-5.bin",	0x20000, 0xb0645169, 7 | BRF_GRA },           // 25
};

STD_ROM_PICK(galaga88a)
STD_ROM_FN(galaga88a)

struct BurnDriver BurnDrvGalaga88a = {
	"galaga88a", "galaga88", NULL, NULL, "1987",
	"Galaga '88 (02-03-88)\0", NULL, "Namco", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, galaga88aRomInfo, galaga88aRomName, NULL, NULL, DrvInputInfo, Galaga88DIPInfo,
	Galaga88Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	224, 288, 3, 4
};


// World Stadium (Japan)

static struct BurnRomInfo wsRomDesc[] = {
	{ "ws1_snd0.bin",	0x10000, 0x45a87810, 1 | BRF_PRG | BRF_ESS }, //  0 Sound m6809 Code
	{ "ws1_snd1.bin",	0x10000, 0x31bf74c1, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ws1_prg0.bin",	0x10000, 0xb0234298, 2 | BRF_PRG | BRF_ESS }, //  2 Main and Sub m6809 Code
	{ "ws1_prg1.bin",	0x10000, 0xdfd72bed, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "ws1_prg2.bin",	0x10000, 0xbb09fa9b, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "ws1_prg7.bin",	0x10000, 0x28712eba, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "cus64-64a1.mcu",	0x01000, 0xffb5c0bd, 3 | BRF_PRG | BRF_ESS }, //  6 Internal MCU Code

	{ "ws1_voi0.bin",	0x10000, 0xf6949199, 4 | BRF_PRG | BRF_ESS }, //  7 External MCU Code
	{ "ws_voi-1.bin",	0x20000, 0x210e2af9, 4 | BRF_PRG | BRF_ESS }, //  8

	{ "ws_chr-8.bin",	0x20000, 0xd1897b9b, 5 | BRF_GRA },           //  9 Character Pixel Masks

	{ "ws_chr-0.bin",	0x20000, 0x3e3e96b4, 6 | BRF_GRA },           // 10 Characters
	{ "ws_chr-1.bin",	0x20000, 0x897dfbc1, 6 | BRF_GRA },           // 11
	{ "ws_chr-2.bin",	0x20000, 0xe142527c, 6 | BRF_GRA },           // 12
	{ "ws_chr-3.bin",	0x20000, 0x907d4dc8, 6 | BRF_GRA },           // 13
	{ "ws_chr-4.bin",	0x20000, 0xafb11e17, 6 | BRF_GRA },           // 14
	{ "ws_chr-6.bin",	0x20000, 0xa16a17c2, 6 | BRF_GRA },           // 15

	{ "ws_obj-0.bin",	0x20000, 0x12dc83a6, 7 | BRF_GRA },           // 16 Sprites
	{ "ws_obj-1.bin",	0x20000, 0x68290a46, 7 | BRF_GRA },           // 17
	{ "ws_obj-2.bin",	0x20000, 0xcd5ba55d, 7 | BRF_GRA },           // 18
	{ "ws1_obj3.bin",	0x10000, 0xf2ed5309, 7 | BRF_GRA },           // 19
};

STD_ROM_PICK(ws)
STD_ROM_FN(ws)

static INT32 WsInit()
{
	namcos1_key_init(2, 0x007, -1, -1, -1, -1, -1, -1);
	dac_kludge = 3;

	return DrvInit();
}

struct BurnDriver BurnDrvWs = {
	"ws", NULL, NULL, NULL, "1988",
	"World Stadium (Japan)\0", NULL, "Namco", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, wsRomInfo, wsRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	WsInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};


// Beraboh Man (Japan, Rev C)

static struct BurnRomInfo berabohmRomDesc[] = {
	{ "bm1_s0.bin",		0x10000, 0xd5d53cb1, 1 | BRF_PRG | BRF_ESS }, //  0 Sound m6809 Code

	{ "bm1_p0.bin",		0x20000, 0xb57ff8c1, 2 | BRF_PRG | BRF_ESS }, //  1 Main and Sub m6809 Code
	{ "bm1_p1.bin",		0x20000, 0xb15f6407, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "bm1_p4.bin",		0x20000, 0xf6cfcb8c, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "bm1-p6.bin",		0x10000, 0xa51b69a5, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "bm1_p7c.bin",	0x20000, 0x9694d7b2, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "cus64-64a1.mcu",	0x01000, 0xffb5c0bd, 3 | BRF_PRG | BRF_ESS }, //  6 Internal MCU Code

	{ "bm1_v0.bin",		0x10000, 0x4e40d0ca, 4 | BRF_PRG | BRF_ESS }, //  7 External MCU Code
	{ "bm_voi-1.bin",	0x20000, 0xbe9ce0a8, 4 | BRF_PRG | BRF_ESS }, //  8
	{ "bm1_v2.bin",		0x10000, 0x41225d04, 4 | BRF_PRG | BRF_ESS }, //  9

	{ "bm_chr-8.bin",	0x20000, 0x92860e95, 5 | BRF_GRA },           // 10 Character Pixel Masks

	{ "bm_chr-0.bin",	0x20000, 0xeda1d92e, 6 | BRF_GRA },           // 11 Characters
	{ "bm_chr-1.bin",	0x20000, 0x8ae1891e, 6 | BRF_GRA },           // 12
	{ "bm_chr-2.bin",	0x20000, 0x774cdf4e, 6 | BRF_GRA },           // 13
	{ "bm_chr-3.bin",	0x20000, 0x6d81e6c9, 6 | BRF_GRA },           // 14
	{ "bm_chr-4.bin",	0x20000, 0xf4597683, 6 | BRF_GRA },           // 15
	{ "bm_chr-5.bin",	0x20000, 0x0e0abde0, 6 | BRF_GRA },           // 16
	{ "bm_chr-6.bin",	0x20000, 0x4a61f08c, 6 | BRF_GRA },           // 17

	{ "bm_obj-0.bin",	0x20000, 0x15724b94, 7 | BRF_GRA },           // 18 Sprites
	{ "bm_obj-1.bin",	0x20000, 0x5d21f962, 7 | BRF_GRA },           // 19
	{ "bm_obj-2.bin",	0x20000, 0x5d48e924, 7 | BRF_GRA },           // 20
	{ "bm_obj-3.bin",	0x20000, 0xcbe56b7f, 7 | BRF_GRA },           // 21
	{ "bm_obj-4.bin",	0x20000, 0x76dcc24c, 7 | BRF_GRA },           // 22
	{ "bm_obj-5.bin",	0x20000, 0xfe70201d, 7 | BRF_GRA },           // 23
	{ "bm_obj-7.bin",	0x20000, 0x377c81ed, 7 | BRF_GRA },           // 24
};

STD_ROM_PICK(berabohm)
STD_ROM_FN(berabohm)

static INT32 BerabohmInit()
{
	input_read_callback = berabohm_buttons_read;
	dac_kludge = 4;

	return DrvInit();
}

struct BurnDriver BurnDrvBerabohm = {
	"berabohm", NULL, NULL, NULL, "1988",
	"Beraboh Man (Japan, Rev C)\0", NULL, "Namco", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, berabohmRomInfo, berabohmRomName, NULL, NULL, BerabohmInputInfo, BerabohmDIPInfo,
	BerabohmInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};


// Beraboh Man (Japan, Rev B)

static struct BurnRomInfo berabohmbRomDesc[] = {
	{ "bm1_s0.bin",		0x10000, 0xd5d53cb1, 1 | BRF_PRG | BRF_ESS }, //  0 Sound m6809 Code

	{ "bm1_p0.bin",		0x20000, 0xb57ff8c1, 2 | BRF_PRG | BRF_ESS }, //  1 Main and Sub m6809 Code
	{ "bm1_p1.bin",		0x20000, 0xb15f6407, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "bm1_p4.bin",		0x20000, 0xf6cfcb8c, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "bm1-p6.bin",		0x10000, 0xa51b69a5, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "bm1_p7b.bin",	0x20000, 0xe0c36ddd, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "cus64-64a1.mcu",	0x01000, 0xffb5c0bd, 3 | BRF_PRG | BRF_ESS }, //  6 Internal MCU Code

	{ "bm1_v0.bin",		0x10000, 0x4e40d0ca, 4 | BRF_PRG | BRF_ESS }, //  7 External MCU Code
	{ "bm_voi-1.bin",	0x20000, 0xbe9ce0a8, 4 | BRF_PRG | BRF_ESS }, //  8
	{ "bm1_v2.bin",		0x10000, 0x41225d04, 4 | BRF_PRG | BRF_ESS }, //  9

	{ "bm_chr-8.bin",	0x20000, 0x92860e95, 5 | BRF_GRA },           // 10 Character Pixel Masks

	{ "bm_chr-0.bin",	0x20000, 0xeda1d92e, 6 | BRF_GRA },           // 11 Characters
	{ "bm_chr-1.bin",	0x20000, 0x8ae1891e, 6 | BRF_GRA },           // 12
	{ "bm_chr-2.bin",	0x20000, 0x774cdf4e, 6 | BRF_GRA },           // 13
	{ "bm_chr-3.bin",	0x20000, 0x6d81e6c9, 6 | BRF_GRA },           // 14
	{ "bm_chr-4.bin",	0x20000, 0xf4597683, 6 | BRF_GRA },           // 15
	{ "bm_chr-5.bin",	0x20000, 0x0e0abde0, 6 | BRF_GRA },           // 16
	{ "bm_chr-6.bin",	0x20000, 0x4a61f08c, 6 | BRF_GRA },           // 17

	{ "bm_obj-0.bin",	0x20000, 0x15724b94, 7 | BRF_GRA },           // 18 Sprites
	{ "bm_obj-1.bin",	0x20000, 0x5d21f962, 7 | BRF_GRA },           // 19
	{ "bm_obj-2.bin",	0x20000, 0x5d48e924, 7 | BRF_GRA },           // 20
	{ "bm_obj-3.bin",	0x20000, 0xcbe56b7f, 7 | BRF_GRA },           // 21
	{ "bm_obj-4.bin",	0x20000, 0x76dcc24c, 7 | BRF_GRA },           // 22
	{ "bm_obj-5.bin",	0x20000, 0xfe70201d, 7 | BRF_GRA },           // 23
	{ "bm_obj-7.bin",	0x20000, 0x377c81ed, 7 | BRF_GRA },           // 24
};

STD_ROM_PICK(berabohmb)
STD_ROM_FN(berabohmb)

struct BurnDriver BurnDrvBerabohmb = {
	"berabohmb", "berabohm", NULL, NULL, "1988",
	"Beraboh Man (Japan, Rev B)\0", NULL, "Namco", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, berabohmbRomInfo, berabohmbRomName, NULL, NULL, BerabohmInputInfo, BerabohmDIPInfo,
	BerabohmInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};


// Marchen Maze (Japan)

static struct BurnRomInfo mmazeRomDesc[] = {
	{ "mm_snd-0.bin",	0x10000, 0x25d25e07, 1 | BRF_PRG | BRF_ESS }, //  0 Sound m6809 Code
	{ "mm_snd-1.bin",	0x10000, 0x2c5849c8, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "mm_prg-0.bin",	0x20000, 0xe169a911, 2 | BRF_PRG | BRF_ESS }, //  2 Main and Sub m6809 Code
	{ "mm_prg-1.bin",	0x20000, 0x6ba14e41, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "mm_prg-2.bin",	0x20000, 0x91bde09f, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "mm1_p6.bin",		0x10000, 0xeaf530d8, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "mm1_p7.bin",		0x10000, 0x085e58cc, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "cus64-64a1.mcu",	0x01000, 0xffb5c0bd, 3 | BRF_PRG | BRF_ESS }, //  7 Internal MCU Code

	{ "mm_voi-0.bin",	0x20000, 0xee974cff, 4 | BRF_PRG | BRF_ESS }, //  8 External MCU Code
	{ "mm_voi-1.bin",	0x20000, 0xd09b5830, 4 | BRF_PRG | BRF_ESS }, //  9

	{ "mm_chr-8.bin",	0x20000, 0xa3784dfe, 5 | BRF_GRA },           // 10 Character Pixel Masks

	{ "mm_chr-0.bin",	0x20000, 0x43ff2dfc, 6 | BRF_GRA },           // 11 Characters
	{ "mm_chr-1.bin",	0x20000, 0xb9b4b72d, 6 | BRF_GRA },           // 12
	{ "mm_chr-2.bin",	0x20000, 0xbee28425, 6 | BRF_GRA },           // 13
	{ "mm_chr-3.bin",	0x20000, 0xd9f41e5c, 6 | BRF_GRA },           // 14
	{ "mm_chr-4.bin",	0x20000, 0x3484f4ae, 6 | BRF_GRA },           // 15
	{ "mm_chr-5.bin",	0x20000, 0xc863deba, 6 | BRF_GRA },           // 16

	{ "mm_obj-0.bin",	0x20000, 0xd4b7e698, 7 | BRF_GRA },           // 17 Sprites
	{ "mm_obj-1.bin",	0x20000, 0x1ce49e04, 7 | BRF_GRA },           // 18
	{ "mm_obj-2.bin",	0x20000, 0x3d3d5de3, 7 | BRF_GRA },           // 19
	{ "mm_obj-3.bin",	0x20000, 0xdac57358, 7 | BRF_GRA },           // 20
};

STD_ROM_PICK(mmaze)
STD_ROM_FN(mmaze)

static INT32 MmazeInit()
{
	namcos1_key_init(2, 0x025, -1, -1, -1, -1, -1, -1);
	dac_kludge = 2;

	return DrvInit();
}

struct BurnDriver BurnDrvMmaze = {
	"mmaze", NULL, NULL, NULL, "1988",
	"Marchen Maze (Japan)\0", "Error on first boot is normal", "Namco", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, mmazeRomInfo, mmazeRomName, NULL, NULL, DrvInputInfo, MmazeDIPInfo,
	MmazeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};


// Bakutotsu Kijuutei

static struct BurnRomInfo bakutotuRomDesc[] = {
	{ "bk1_s0.bin",		0x10000, 0xc35d7df6, 1 | BRF_PRG | BRF_ESS }, //  0 Sound m6809 Code

	{ "bk_prg-0.bin",	0x20000, 0x4529c362, 2 | BRF_PRG | BRF_ESS }, //  1 Main and Sub m6809 Code
	{ "bk1_p1.bin",		0x10000, 0xd389d6d4, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "bk1_p2.bin",		0x10000, 0x7a686daa, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "bk1_p3.bin",		0x20000, 0xe608234f, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "bk1_p4.bin",		0x10000, 0x96446d48, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "bk1_p5.bin",		0x10000, 0xdceed7cb, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "bk1_p6.bin",		0x20000, 0x57a3ce42, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "bk1_prg7.bin",	0x20000, 0xfac1c1bf, 2 | BRF_PRG | BRF_ESS }, //  8

	{ "cus64-64a1.mcu",	0x01000, 0xffb5c0bd, 3 | BRF_PRG | BRF_ESS }, //  9 Internal MCU Code

	{ "bk1_v0.bin",		0x10000, 0x008e290e, 4 | BRF_PRG | BRF_ESS }, // 10 External MCU Code

	{ "bk_chr-8.bin",	0x20000, 0x6c8d4029, 5 | BRF_GRA },           // 11 Character Pixel Masks

	{ "bk_chr-0.bin",	0x20000, 0x4e011058, 6 | BRF_GRA },           // 12 Characters
	{ "bk_chr-1.bin",	0x20000, 0x496fcb9b, 6 | BRF_GRA },           // 13
	{ "bk_chr-2.bin",	0x20000, 0xdc812e28, 6 | BRF_GRA },           // 14
	{ "bk_chr-3.bin",	0x20000, 0x2b6120f4, 6 | BRF_GRA },           // 15

	{ "bk_obj-0.bin",	0x20000, 0x88c627c1, 7 | BRF_GRA },           // 16 Sprites
	{ "bk_obj-3.bin",	0x20000, 0xf7d1909a, 7 | BRF_GRA },           // 17
	{ "bk_obj-4.bin",	0x20000, 0x27ed1441, 7 | BRF_GRA },           // 18
	{ "bk_obj-3.bin",	0x20000, 0xf7d1909a, 7 | BRF_GRA },           // 19
	{ "bk_obj-4.bin",	0x20000, 0x27ed1441, 7 | BRF_GRA },           // 20
	{ "bk_obj-5.bin",	0x20000, 0x790560c0, 7 | BRF_GRA },           // 21
	{ "bk_obj-6.bin",	0x20000, 0x2cd4d2ea, 7 | BRF_GRA },           // 22
	{ "bk_obj-7.bin",	0x20000, 0x809aa0e6, 7 | BRF_GRA },           // 23
};

STD_ROM_PICK(bakutotu)
STD_ROM_FN(bakutotu)

static INT32 BakutotuInit()
{
	namcos1_key_init(2, 0x022, -1, -1, -1, -1, -1, -1);
	dac_kludge = 2;

	return DrvInit();
}

struct BurnDriver BurnDrvBakutotu = {
	"bakutotu", NULL, NULL, NULL, "1988",
	"Bakutotsu Kijuutei\0", NULL, "Namco", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, bakutotuRomInfo, bakutotuRomName, NULL, NULL, DrvInputInfo, BakutotuDIPInfo,
	BakutotuInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};


// World Court (Japan)

static struct BurnRomInfo wldcourtRomDesc[] = {
	{ "wc1_snd0.bin",	0x10000, 0x17a6505d, 1 | BRF_PRG | BRF_ESS }, //  0 Sound m6809 Code

	{ "wc1_prg6.bin",	0x10000, 0xe9216b9e, 2 | BRF_PRG | BRF_ESS }, //  1 Main and Sub m6809 Code
	{ "wc1_prg7.bin",	0x10000, 0x8a7c6cac, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "cus64-64a1.mcu",	0x01000, 0xffb5c0bd, 3 | BRF_PRG | BRF_ESS }, //  3 Internal MCU Code

	{ "wc1_voi0.bin",	0x10000, 0xb57919f7, 4 | BRF_PRG | BRF_ESS }, //  4 External MCU Code
	{ "wc1_voi1.bin",	0x20000, 0x97974b4b, 4 | BRF_PRG | BRF_ESS }, //  5

	{ "wc1_chr8.bin",	0x20000, 0x23e1c399, 5 | BRF_GRA },           //  6 Character Pixel Masks

	{ "wc1_chr0.bin",	0x20000, 0x9fb07b9b, 6 | BRF_GRA },           //  7 Characters
	{ "wc1_chr1.bin",	0x20000, 0x01bfbf60, 6 | BRF_GRA },           //  8
	{ "wc1_chr2.bin",	0x20000, 0x7e8acf45, 6 | BRF_GRA },           //  9
	{ "wc1_chr3.bin",	0x20000, 0x924e9c81, 6 | BRF_GRA },           // 10

	{ "wc1_obj0.bin",	0x20000, 0x70d562f8, 7 | BRF_GRA },           // 11 Sprites
	{ "wc1_obj1.bin",	0x20000, 0xba8b034a, 7 | BRF_GRA },           // 12
	{ "wc1_obj2.bin",	0x20000, 0xc2bd5f0f, 7 | BRF_GRA },           // 13
	{ "wc1_obj3.bin",	0x10000, 0x1aa2dbc8, 7 | BRF_GRA },           // 14
};

STD_ROM_PICK(wldcourt)
STD_ROM_FN(wldcourt)

static INT32 WldcourtInit()
{
	namcos1_key_init(1, 0x035, -1, -1, -1, -1, -1, -1);

	return DrvInit();
}

struct BurnDriver BurnDrvWldcourt = {
	"wldcourt", NULL, NULL, NULL, "1988",
	"World Court (Japan)\0", NULL, "Namco", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, wldcourtRomInfo, wldcourtRomName, NULL, NULL, DrvInputInfo, WldcourtDIPInfo,
	WldcourtInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};


// Splatter House (World, new version (SH3))

static struct BurnRomInfo splatterRomDesc[] = {
	{ "sh1_snd0b.bin",	0x10000, 0x03b47a5c, 1 | BRF_PRG | BRF_ESS }, //  0 Sound m6809 Code
	{ "sh1_snd1.bin",	0x10000, 0x8ece9e0a, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "sh1_prg0.bin",	0x10000, 0x4e07e6d9, 2 | BRF_PRG | BRF_ESS }, //  2 Main and Sub m6809 Code
	{ "sh1_prg1.bin",	0x10000, 0x7a3efe09, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "sh1_prg2.bin",	0x10000, 0x434dbe7d, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "sh1_prg3.bin",	0x10000, 0x955ce93f, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "sh1_prg4.bin",	0x10000, 0x350dee5b, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "sh3_prg5.bin",	0x10000, 0x3af893d9, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "sh3_prg6.bin",	0x10000, 0x0579e90e, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "sh3_prg7.bin",	0x10000, 0x653b4509, 2 | BRF_PRG | BRF_ESS }, //  9

	{ "cus64-64a1.mcu",	0x01000, 0xffb5c0bd, 3 | BRF_PRG | BRF_ESS }, // 10 Internal MCU Code

	{ "sh_voi-0.bin",	0x20000, 0x2199cb66, 4 | BRF_PRG | BRF_ESS }, // 11 External MCU Code
	{ "sh_voi-1.bin",	0x20000, 0x9b6472af, 4 | BRF_PRG | BRF_ESS }, // 12
	{ "sh_voi-2.bin",	0x20000, 0x25ea75b6, 4 | BRF_PRG | BRF_ESS }, // 13
	{ "sh_voi-3.bin",	0x20000, 0x5eebcdb4, 4 | BRF_PRG | BRF_ESS }, // 14

	{ "sh_chr-8.bin",	0x20000, 0x321f483b, 5 | BRF_GRA },           // 15 Character Pixel Masks

	{ "sh_chr-0.bin",	0x20000, 0x4dd2ef05, 6 | BRF_GRA },           // 16 Characters
	{ "sh_chr-1.bin",	0x20000, 0x7a764999, 6 | BRF_GRA },           // 17
	{ "sh_chr-2.bin",	0x20000, 0x6e6526ee, 6 | BRF_GRA },           // 18
	{ "sh_chr-3.bin",	0x20000, 0x8d05abdb, 6 | BRF_GRA },           // 19
	{ "sh_chr-4.bin",	0x20000, 0x1e1f8488, 6 | BRF_GRA },           // 20
	{ "sh_chr-5.bin",	0x20000, 0x684cf554, 6 | BRF_GRA },           // 21

	{ "sh_obj-0.bin",	0x20000, 0x1cedbbae, 7 | BRF_GRA },           // 22 Sprites
	{ "sh_obj-1.bin",	0x20000, 0xe56e91ee, 7 | BRF_GRA },           // 23
	{ "sh_obj-2.bin",	0x20000, 0x3dfb0230, 7 | BRF_GRA },           // 24
	{ "sh_obj-3.bin",	0x20000, 0xe4e5a581, 7 | BRF_GRA },           // 25
	{ "sh_obj-4.bin",	0x20000, 0xb2422182, 7 | BRF_GRA },           // 26
	{ "sh_obj-5.bin",	0x20000, 0x24d0266f, 7 | BRF_GRA },           // 27
	{ "sh_obj-6.bin",	0x20000, 0x80830b0e, 7 | BRF_GRA },           // 28
	{ "sh_obj-7.bin",	0x20000, 0x08b1953a, 7 | BRF_GRA },           // 29
};

STD_ROM_PICK(splatter)
STD_ROM_FN(splatter)

static INT32 SplatterInit()
{
	namcos1_key_init(3, 0x0b5, 3,  4, -1, -1, -1, -1);
	dac_kludge = 1;

	return DrvInit();
}

struct BurnDriver BurnDrvSplatter = {
	"splatter", NULL, NULL, NULL, "1988",
	"Splatter House (World, new version (SH3))\0", NULL, "Namco", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, splatterRomInfo, splatterRomName, NULL, NULL, DrvInputInfo, Splatter3DIPInfo,
	SplatterInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};


// Splatter House (World, old version (SH2))

static struct BurnRomInfo splatter2RomDesc[] = {
	{ "sh1_snd0.bin",	0x10000, 0x90abd4ad, 1 | BRF_PRG | BRF_ESS }, //  0 Sound m6809 Code
	{ "sh1_snd1.bin",	0x10000, 0x8ece9e0a, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "sh1_prg0.bin",	0x10000, 0x4e07e6d9, 2 | BRF_PRG | BRF_ESS }, //  2 Main and Sub m6809 Code
	{ "sh1_prg1.bin",	0x10000, 0x7a3efe09, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "sh1_prg2.bin",	0x10000, 0x434dbe7d, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "sh1_prg3.bin",	0x10000, 0x955ce93f, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "sh1_prg4.bin",	0x10000, 0x350dee5b, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "sh1_prg5.bin",	0x10000, 0x0187de9a, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "sh2_prg6.bin",	0x10000, 0x054d6275, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "sh2_prg7.bin",	0x10000, 0x942cb61e, 2 | BRF_PRG | BRF_ESS }, //  9

	{ "cus64-64a1.mcu",	0x01000, 0xffb5c0bd, 3 | BRF_PRG | BRF_ESS }, // 10 Internal MCU Code

	{ "sh_voi-0.bin",	0x20000, 0x2199cb66, 4 | BRF_PRG | BRF_ESS }, // 11 External MCU Code
	{ "sh_voi-1.bin",	0x20000, 0x9b6472af, 4 | BRF_PRG | BRF_ESS }, // 12
	{ "sh_voi-2.bin",	0x20000, 0x25ea75b6, 4 | BRF_PRG | BRF_ESS }, // 13
	{ "sh_voi-3.bin",	0x20000, 0x5eebcdb4, 4 | BRF_PRG | BRF_ESS }, // 14

	{ "sh_chr-8.bin",	0x20000, 0x321f483b, 5 | BRF_GRA },           // 15 Character Pixel Masks

	{ "sh_chr-0.bin",	0x20000, 0x4dd2ef05, 6 | BRF_GRA },           // 16 Characters
	{ "sh_chr-1.bin",	0x20000, 0x7a764999, 6 | BRF_GRA },           // 17
	{ "sh_chr-2.bin",	0x20000, 0x6e6526ee, 6 | BRF_GRA },           // 18
	{ "sh_chr-3.bin",	0x20000, 0x8d05abdb, 6 | BRF_GRA },           // 19
	{ "sh_chr-4.bin",	0x20000, 0x1e1f8488, 6 | BRF_GRA },           // 20
	{ "sh_chr-5.bin",	0x20000, 0x684cf554, 6 | BRF_GRA },           // 21

	{ "sh_obj-0.bin",	0x20000, 0x1cedbbae, 7 | BRF_GRA },           // 22 Sprites
	{ "sh_obj-1.bin",	0x20000, 0xe56e91ee, 7 | BRF_GRA },           // 23
	{ "sh_obj-2.bin",	0x20000, 0x3dfb0230, 7 | BRF_GRA },           // 24
	{ "sh_obj-3.bin",	0x20000, 0xe4e5a581, 7 | BRF_GRA },           // 25
	{ "sh_obj-4.bin",	0x20000, 0xb2422182, 7 | BRF_GRA },           // 26
	{ "sh_obj-5.bin",	0x20000, 0x24d0266f, 7 | BRF_GRA },           // 27
	{ "sh_obj-6.bin",	0x20000, 0x80830b0e, 7 | BRF_GRA },           // 28
	{ "sh_obj-7.bin",	0x20000, 0x08b1953a, 7 | BRF_GRA },           // 29
};

STD_ROM_PICK(splatter2)
STD_ROM_FN(splatter2)

struct BurnDriver BurnDrvSplatter2 = {
	"splatter2", "splatter", NULL, NULL, "1988",
	"Splatter House (World, old version (SH2))\0", NULL, "Namco", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, splatter2RomInfo, splatter2RomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	SplatterInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};


// Splatter House (Japan, SH1)

static struct BurnRomInfo splatterjRomDesc[] = {
	{ "sh1_snd0.bin",	0x10000, 0x90abd4ad, 1 | BRF_PRG | BRF_ESS }, //  0 Sound m6809 Code
	{ "sh1_snd1.bin",	0x10000, 0x8ece9e0a, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "sh1_prg0.bin",	0x10000, 0x4e07e6d9, 2 | BRF_PRG | BRF_ESS }, //  2 Main and Sub m6809 Code
	{ "sh1_prg1.bin",	0x10000, 0x7a3efe09, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "sh1_prg2.bin",	0x10000, 0x434dbe7d, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "sh1_prg3.bin",	0x10000, 0x955ce93f, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "sh1_prg4.bin",	0x10000, 0x350dee5b, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "sh1_prg5.bin",	0x10000, 0x0187de9a, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "sh1_prg6.bin",	0x10000, 0x97a3e664, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "sh1_prg7.bin",	0x10000, 0x24c8cbd7, 2 | BRF_PRG | BRF_ESS }, //  9

	{ "cus64-64a1.mcu",	0x01000, 0xffb5c0bd, 3 | BRF_PRG | BRF_ESS }, // 10 Internal MCU Code

	{ "sh_voi-0.bin",	0x20000, 0x2199cb66, 4 | BRF_PRG | BRF_ESS }, // 11 External MCU Code
	{ "sh_voi-1.bin",	0x20000, 0x9b6472af, 4 | BRF_PRG | BRF_ESS }, // 12
	{ "sh_voi-2.bin",	0x20000, 0x25ea75b6, 4 | BRF_PRG | BRF_ESS }, // 13
	{ "sh_voi-3.bin",	0x20000, 0x5eebcdb4, 4 | BRF_PRG | BRF_ESS }, // 14

	{ "sh_chr-8.bin",	0x20000, 0x321f483b, 5 | BRF_GRA },           // 15 Character Pixel Masks

	{ "sh_chr-0.bin",	0x20000, 0x4dd2ef05, 6 | BRF_GRA },           // 16 Characters
	{ "sh_chr-1.bin",	0x20000, 0x7a764999, 6 | BRF_GRA },           // 17
	{ "sh_chr-2.bin",	0x20000, 0x6e6526ee, 6 | BRF_GRA },           // 18
	{ "sh_chr-3.bin",	0x20000, 0x8d05abdb, 6 | BRF_GRA },           // 19
	{ "sh_chr-4.bin",	0x20000, 0x1e1f8488, 6 | BRF_GRA },           // 20
	{ "sh_chr-5.bin",	0x20000, 0x684cf554, 6 | BRF_GRA },           // 21

	{ "sh_obj-0.bin",	0x20000, 0x1cedbbae, 7 | BRF_GRA },           // 22 Sprites
	{ "sh_obj-1.bin",	0x20000, 0xe56e91ee, 7 | BRF_GRA },           // 23
	{ "sh_obj-2.bin",	0x20000, 0x3dfb0230, 7 | BRF_GRA },           // 24
	{ "sh_obj-3.bin",	0x20000, 0xe4e5a581, 7 | BRF_GRA },           // 25
	{ "sh_obj-4.bin",	0x20000, 0xb2422182, 7 | BRF_GRA },           // 26
	{ "sh_obj-5.bin",	0x20000, 0x24d0266f, 7 | BRF_GRA },           // 27
	{ "sh_obj-6.bin",	0x20000, 0x80830b0e, 7 | BRF_GRA },           // 28
	{ "sh_obj-7.bin",	0x20000, 0x08b1953a, 7 | BRF_GRA },           // 29
};

STD_ROM_PICK(splatterj)
STD_ROM_FN(splatterj)

struct BurnDriver BurnDrvSplatterj = {
	"splatterj", "splatter", NULL, NULL, "1988",
	"Splatter House (Japan, SH1)\0", NULL, "Namco", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, splatterjRomInfo, splatterjRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	SplatterInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};


// Face Off (Japan 2 Players)

static struct BurnRomInfo faceoffRomDesc[] = {
	{ "fo1_s0.bin",		0x10000, 0x9a00d97d, 1 | BRF_PRG | BRF_ESS }, //  0 Sound m6809 Code

	{ "fo1_p6.bin",		0x10000, 0xa48ee82b, 2 | BRF_PRG | BRF_ESS }, //  1 Main and Sub m6809 Code
	{ "fo1_p7.bin",		0x10000, 0x6791d221, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "cus64-64a1.mcu",	0x01000, 0xffb5c0bd, 3 | BRF_PRG | BRF_ESS }, //  3 Internal MCU Code

	{ "fo1_v0.bin",		0x10000, 0xe6edf63e, 4 | BRF_PRG | BRF_ESS }, //  4 External MCU Code
	{ "fo1_v1.bin",		0x10000, 0x132a5d90, 4 | BRF_PRG | BRF_ESS }, //  5

	{ "fo1_c8.bin",		0x10000, 0xd397216c, 5 | BRF_GRA },           //  6 Character Pixel Masks

	{ "fo1_c0.bin",		0x20000, 0x27884ac0, 6 | BRF_GRA },           //  7 Characters
	{ "fo1_c1.bin",		0x20000, 0x4d423499, 6 | BRF_GRA },           //  8
	{ "fo1_c2.bin",		0x20000, 0xd62d86f1, 6 | BRF_GRA },           //  9
	{ "fo1_c3.bin",		0x20000, 0xc2a08694, 6 | BRF_GRA },           // 10

	{ "fo1_o0.bin",		0x20000, 0x41af669d, 7 | BRF_GRA },           // 11 Sprites
	{ "fo1_o1.bin",		0x20000, 0xad5fbaa7, 7 | BRF_GRA },           // 12
	{ "fo1_o2.bin",		0x20000, 0xc1f7eb52, 7 | BRF_GRA },           // 13
	{ "fo1_o3.bin",		0x20000, 0xaa95d2e0, 7 | BRF_GRA },           // 14
	{ "fo1_o4.bin",		0x20000, 0x985f04c7, 7 | BRF_GRA },           // 15
};

STD_ROM_PICK(faceoff)
STD_ROM_FN(faceoff)

static INT32 FaceoffInit()
{
	input_read_callback = faceoff_inputs_read;
	dac_kludge = 1;

	return DrvInit();
}

struct BurnDriver BurnDrvFaceoff = {
	"faceoff", NULL, NULL, NULL, "1988",
	"Face Off (Japan 2 Players)\0", NULL, "Namco", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, faceoffRomInfo, faceoffRomName, NULL, NULL, FaceoffInputInfo, FaceoffDIPInfo,
	FaceoffInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};


// Rompers (Japan, new version (Rev B))

static struct BurnRomInfo rompersRomDesc[] = {
	{ "rp1_snd0.bin",	0x10000, 0xc7c8d649, 1 | BRF_PRG | BRF_ESS }, //  0 Sound m6809 Code

	{ "rp1_prg4.bin",	0x10000, 0x0918f06d, 2 | BRF_PRG | BRF_ESS }, //  1 Main and Sub m6809 Code
	{ "rp1_prg5.bin",	0x10000, 0x98bd4133, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "rp1prg6b.bin",	0x10000, 0x80821065, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "rp1prg7b.bin",	0x10000, 0x49d057e2, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "cus64-64a1.mcu",	0x01000, 0xffb5c0bd, 3 | BRF_PRG | BRF_ESS }, //  5 Internal MCU Code

	{ "rp_voi-0.bin",	0x20000, 0x11caef7e, 4 | BRF_PRG | BRF_ESS }, //  6 External MCU Code

	{ "rp1_chr8.bin",	0x10000, 0x69cfe46a, 5 | BRF_GRA },           //  7 Character Pixel Masks

	{ "rp_chr-0.bin",	0x20000, 0x41b10ef3, 6 | BRF_GRA },           //  8 Characters
	{ "rp_chr-1.bin",	0x20000, 0xc18cd24e, 6 | BRF_GRA },           //  9
	{ "rp_chr-2.bin",	0x20000, 0x6c9a3c79, 6 | BRF_GRA },           // 10
	{ "rp_chr-3.bin",	0x20000, 0x473aa788, 6 | BRF_GRA },           // 11

	{ "rp_obj-0.bin",	0x20000, 0x1dcbf8bb, 7 | BRF_GRA },           // 12 Sprites
	{ "rp_obj-1.bin",	0x20000, 0xcb98e273, 7 | BRF_GRA },           // 13
	{ "rp_obj-2.bin",	0x20000, 0x6ebd191e, 7 | BRF_GRA },           // 14
	{ "rp_obj-3.bin",	0x20000, 0x7c9828a1, 7 | BRF_GRA },           // 15
	{ "rp_obj-4.bin",	0x20000, 0x0348220b, 7 | BRF_GRA },           // 16
	{ "rp1_obj5.bin",	0x10000, 0x9e2ba243, 7 | BRF_GRA },           // 17
	{ "rp1_obj6.bin",	0x10000, 0x6bf2aca6, 7 | BRF_GRA },           // 18
};

STD_ROM_PICK(rompers)
STD_ROM_FN(rompers)

static INT32 RompersInit()
{
	namcos1_key_init(3, 0x0b6, 7, -1, -1, -1, -1, -1);
	sixtyhz = 1;
	dac_kludge = 3;

	return DrvInit();
}

struct BurnDriver BurnDrvRompers = {
	"rompers", NULL, NULL, NULL, "1989",
	"Rompers (Japan, new version (Rev B))\0", NULL, "Namco", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, rompersRomInfo, rompersRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	RompersInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	224, 288, 3, 4
};


// Rompers (Japan, old version)

static struct BurnRomInfo rompersoRomDesc[] = {
	{ "rp1_snd0.bin",	0x10000, 0xc7c8d649, 1 | BRF_PRG | BRF_ESS }, //  0 Sound m6809 Code

	{ "rp1_prg4.bin",	0x10000, 0x0918f06d, 2 | BRF_PRG | BRF_ESS }, //  1 Main and Sub m6809 Code
	{ "rp1_prg5.bin",	0x10000, 0x98bd4133, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "rp1_prg6.bin",	0x10000, 0xfc183345, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "rp1_prg7.bin",	0x10000, 0x8d49f28a, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "cus64-64a1.mcu",	0x01000, 0xffb5c0bd, 3 | BRF_PRG | BRF_ESS }, //  5 Internal MCU Code

	{ "rp_voi-0.bin",	0x20000, 0x11caef7e, 4 | BRF_PRG | BRF_ESS }, //  6 External MCU Code

	{ "rp1_chr8.bin",	0x10000, 0x69cfe46a, 5 | BRF_GRA },           //  7 Character Pixel Masks

	{ "rp_chr-0.bin",	0x20000, 0x41b10ef3, 6 | BRF_GRA },           //  8 Characters
	{ "rp_chr-1.bin",	0x20000, 0xc18cd24e, 6 | BRF_GRA },           //  9
	{ "rp_chr-2.bin",	0x20000, 0x6c9a3c79, 6 | BRF_GRA },           // 10
	{ "rp_chr-3.bin",	0x20000, 0x473aa788, 6 | BRF_GRA },           // 11

	{ "rp_obj-0.bin",	0x20000, 0x1dcbf8bb, 7 | BRF_GRA },           // 12 Sprites
	{ "rp_obj-1.bin",	0x20000, 0xcb98e273, 7 | BRF_GRA },           // 13
	{ "rp_obj-2.bin",	0x20000, 0x6ebd191e, 7 | BRF_GRA },           // 14
	{ "rp_obj-3.bin",	0x20000, 0x7c9828a1, 7 | BRF_GRA },           // 15
	{ "rp_obj-4.bin",	0x20000, 0x0348220b, 7 | BRF_GRA },           // 16
	{ "rp1_obj5.bin",	0x10000, 0x9e2ba243, 7 | BRF_GRA },           // 17
	{ "rp1_obj6.bin",	0x10000, 0x6bf2aca6, 7 | BRF_GRA },           // 18
};

STD_ROM_PICK(romperso)
STD_ROM_FN(romperso)

struct BurnDriver BurnDrvRomperso = {
	"romperso", "rompers", NULL, NULL, "1989",
	"Rompers (Japan, old version)\0", NULL, "Namco", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, rompersoRomInfo, rompersoRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	RompersInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	224, 288, 3, 4
};


// Blast Off (Japan)

static struct BurnRomInfo blastoffRomDesc[] = {
	{ "bo1-snd0.bin",	0x10000, 0x2ecab76e, 1 | BRF_PRG | BRF_ESS }, //  0 Sound m6809 Code
	{ "bo1-snd1.bin",	0x10000, 0x048a6af1, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "bo1_prg6.bin",	0x20000, 0xd60da63e, 2 | BRF_PRG | BRF_ESS }, //  2 Main and Sub m6809 Code
	{ "bo1prg7b.bin",	0x20000, 0xb630383c, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "cus64-64a1.mcu",	0x01000, 0xffb5c0bd, 3 | BRF_PRG | BRF_ESS }, //  4 Internal MCU Code

	{ "bo_voi-0.bin",	0x20000, 0x47065e18, 4 | BRF_PRG | BRF_ESS }, //  5 External MCU Code
	{ "bo_voi-1.bin",	0x20000, 0x0308b18e, 4 | BRF_PRG | BRF_ESS }, //  6
	{ "bo_voi-2.bin",	0x20000, 0x88cab230, 4 | BRF_PRG | BRF_ESS }, //  7

	{ "bo_chr-8.bin",	0x20000, 0xe8b5f2d4, 5 | BRF_GRA },           //  8 Character Pixel Masks

	{ "bo_chr-0.bin",	0x20000, 0xbdc0afb5, 6 | BRF_GRA },           //  9 Characters
	{ "bo_chr-1.bin",	0x20000, 0x963d2639, 6 | BRF_GRA },           // 10
	{ "bo_chr-2.bin",	0x20000, 0xacdb6894, 6 | BRF_GRA },           // 11
	{ "bo_chr-3.bin",	0x20000, 0x214ec47f, 6 | BRF_GRA },           // 12
	{ "bo_chr-4.bin",	0x20000, 0x08397583, 6 | BRF_GRA },           // 13
	{ "bo_chr-5.bin",	0x20000, 0x20402429, 6 | BRF_GRA },           // 14
	{ "bo_chr-7.bin",	0x20000, 0x4c5c4603, 6 | BRF_GRA },           // 15

	{ "bo_obj-0.bin",	0x20000, 0xb3308ae7, 7 | BRF_GRA },           // 16 Sprites
	{ "bo_obj-1.bin",	0x20000, 0xc9c93c47, 7 | BRF_GRA },           // 17
	{ "bo_obj-2.bin",	0x20000, 0xeef77527, 7 | BRF_GRA },           // 18
	{ "bo_obj-3.bin",	0x20000, 0xe3d9ed58, 7 | BRF_GRA },           // 19
	{ "bo1_obj4.bin",	0x20000, 0xc2c1b9cb, 7 | BRF_GRA },           // 20
};

STD_ROM_PICK(blastoff)
STD_ROM_FN(blastoff)

static INT32 BlastoffInit()
{
	namcos1_key_init(3, 0x0b7, 0,  7,  3,  5, -1, -1);
	dac_kludge = 1;

	return DrvInit();
}

struct BurnDriver BurnDrvBlastoff = {
	"blastoff", NULL, NULL, NULL, "1989",
	"Blast Off (Japan)\0", NULL, "Namco", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, blastoffRomInfo, blastoffRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	BlastoffInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	224, 288, 3, 4
};


// World Stadium '89 (Japan)

static struct BurnRomInfo ws89RomDesc[] = {
	{ "w91_snd0.bin",	0x10000, 0x52b84d5a, 1 | BRF_PRG | BRF_ESS }, //  0 Sound m6809 Code
	{ "ws1_snd1.bin",	0x10000, 0x31bf74c1, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ws1_prg0.bin",	0x10000, 0xb0234298, 2 | BRF_PRG | BRF_ESS }, //  2 Main and Sub m6809 Code
	{ "w91_prg1.bin",	0x10000, 0x7ad8768f, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "w91_prg2.bin",	0x10000, 0x522e5441, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "w91_prg7.bin",	0x10000, 0x611ed964, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "cus64-64a1.mcu",	0x01000, 0xffb5c0bd, 3 | BRF_PRG | BRF_ESS }, //  6 Internal MCU Code

	{ "ws1_voi0.bin",	0x10000, 0xf6949199, 4 | BRF_PRG | BRF_ESS }, //  7 External MCU Code
	{ "ws_voi-1.bin",	0x20000, 0x210e2af9, 4 | BRF_PRG | BRF_ESS }, //  8

	{ "ws_chr-8.bin",	0x20000, 0xd1897b9b, 5 | BRF_GRA },           //  9 Character Pixel Masks

	{ "ws_chr-0.bin",	0x20000, 0x3e3e96b4, 6 | BRF_GRA },           // 10 Characters
	{ "ws_chr-1.bin",	0x20000, 0x897dfbc1, 6 | BRF_GRA },           // 11
	{ "ws_chr-2.bin",	0x20000, 0xe142527c, 6 | BRF_GRA },           // 12
	{ "ws_chr-3.bin",	0x20000, 0x907d4dc8, 6 | BRF_GRA },           // 13
	{ "ws_chr-4.bin",	0x20000, 0xafb11e17, 6 | BRF_GRA },           // 14
	{ "ws_chr-6.bin",	0x20000, 0xa16a17c2, 6 | BRF_GRA },           // 15

	{ "ws_obj-0.bin",	0x20000, 0x12dc83a6, 7 | BRF_GRA },           // 16 Sprites
	{ "ws_obj-1.bin",	0x20000, 0x68290a46, 7 | BRF_GRA },           // 17
	{ "ws_obj-2.bin",	0x20000, 0xcd5ba55d, 7 | BRF_GRA },           // 18
	{ "w91_obj3.bin",	0x10000, 0x8ee76105, 7 | BRF_GRA },           // 19
};

STD_ROM_PICK(ws89)
STD_ROM_FN(ws89)

static INT32 Ws89Init()
{
	namcos1_key_init(3, 0x0b8, 2, -1, -1, -1, -1, -1);

	return DrvInit();
}

struct BurnDriver BurnDrvWs89 = {
	"ws89", "ws", NULL, NULL, "1989",
	"World Stadium '89 (Japan)\0", NULL, "Namco", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, ws89RomInfo, ws89RomName, NULL, NULL, DrvInputInfo, Ws89DIPInfo,
	Ws89Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};


// Dangerous Seed (Japan)

static struct BurnRomInfo dangseedRomDesc[] = {
	{ "dr1_snd0.bin",	0x20000, 0xbcbbb21d, 1 | BRF_PRG | BRF_ESS }, //  0 Sound m6809 Code

	{ "dr_prg-5.bin",	0x20000, 0x7986bbdd, 2 | BRF_PRG | BRF_ESS }, //  1 Main and Sub m6809 Code
	{ "dr1_prg6.bin",	0x10000, 0xcc68262b, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "dr1_prg7.bin",	0x20000, 0xd7d2f653, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "cus64-64a1.mcu",	0x01000, 0xffb5c0bd, 3 | BRF_PRG | BRF_ESS }, //  4 Internal MCU Code

	{ "dr_voi-0.bin",	0x20000, 0xde4fdc0e, 4 | BRF_PRG | BRF_ESS }, //  5 External MCU Code

	{ "dr_chr-8.bin",	0x20000, 0x0fbaa10e, 5 | BRF_GRA },           //  6 Character Pixel Masks

	{ "dr_chr-0.bin",	0x20000, 0x419bacc7, 6 | BRF_GRA },           //  7 Characters
	{ "dr_chr-1.bin",	0x20000, 0x55ce77e1, 6 | BRF_GRA },           //  8
	{ "dr_chr-2.bin",	0x20000, 0x6f913419, 6 | BRF_GRA },           //  9
	{ "dr_chr-3.bin",	0x20000, 0xfe1f1a25, 6 | BRF_GRA },           // 10
	{ "dr_chr-4.bin",	0x20000, 0xc34471bc, 6 | BRF_GRA },           // 11
	{ "dr_chr-5.bin",	0x20000, 0x715c0720, 6 | BRF_GRA },           // 12
	{ "dr_chr-6.bin",	0x20000, 0x5c1b71fa, 6 | BRF_GRA },           // 13

	{ "dr_obj-0.bin",	0x20000, 0xabb95644, 7 | BRF_GRA },           // 14 Sprites
	{ "dr_obj-1.bin",	0x20000, 0x24d6db51, 7 | BRF_GRA },           // 15
	{ "dr_obj-2.bin",	0x20000, 0x7e3a78c0, 7 | BRF_GRA },           // 16
};

STD_ROM_PICK(dangseed)
STD_ROM_FN(dangseed)

static INT32 DangseedInit()
{
	namcos1_key_init(3, 0x134, 6, -1, 5, -1,  0, 4);

	return DrvInit();
}

struct BurnDriver BurnDrvDangseed = {
	"dangseed", NULL, NULL, NULL, "1989",
	"Dangerous Seed (Japan)\0", NULL, "Namco", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, dangseedRomInfo, dangseedRomName, NULL, NULL, DrvInputInfo, DangseedDIPInfo,
	DangseedInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	224, 288, 3, 4
};


// World Stadium '90 (Japan)

static struct BurnRomInfo ws90RomDesc[] = {
	{ "w91_snd0.bin",	0x10000, 0x52b84d5a, 1 | BRF_PRG | BRF_ESS }, //  0 Sound m6809 Code
	{ "ws1_snd1.bin",	0x10000, 0x31bf74c1, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ws1_prg0.bin",	0x10000, 0xb0234298, 2 | BRF_PRG | BRF_ESS }, //  2 Main and Sub m6809 Code
	{ "w91_prg1.bin",	0x10000, 0x7ad8768f, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "w901prg2.bin",	0x10000, 0xb9e98e2f, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "w901prg7.bin",	0x10000, 0x37ae1b25, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "cus64-64a1.mcu",	0x01000, 0xffb5c0bd, 3 | BRF_PRG | BRF_ESS }, //  6 Internal MCU Code

	{ "ws1_voi0.bin",	0x10000, 0xf6949199, 4 | BRF_PRG | BRF_ESS }, //  7 External MCU Code
	{ "ws_voi-1.bin",	0x20000, 0x210e2af9, 4 | BRF_PRG | BRF_ESS }, //  8

	{ "ws_chr-8.bin",	0x20000, 0xd1897b9b, 5 | BRF_GRA },           //  9 Character Pixel Masks

	{ "ws_chr-0.bin",	0x20000, 0x3e3e96b4, 6 | BRF_GRA },           // 10 Characters
	{ "ws_chr-1.bin",	0x20000, 0x897dfbc1, 6 | BRF_GRA },           // 11
	{ "ws_chr-2.bin",	0x20000, 0xe142527c, 6 | BRF_GRA },           // 12
	{ "ws_chr-3.bin",	0x20000, 0x907d4dc8, 6 | BRF_GRA },           // 13
	{ "ws_chr-4.bin",	0x20000, 0xafb11e17, 6 | BRF_GRA },           // 14
	{ "ws_chr-6.bin",	0x20000, 0xa16a17c2, 6 | BRF_GRA },           // 15

	{ "ws_obj-0.bin",	0x20000, 0x12dc83a6, 7 | BRF_GRA },           // 16 Sprites
	{ "ws_obj-1.bin",	0x20000, 0x68290a46, 7 | BRF_GRA },           // 17
	{ "ws_obj-2.bin",	0x20000, 0xcd5ba55d, 7 | BRF_GRA },           // 18
	{ "w901obj3.bin",	0x10000, 0x7d0b8961, 7 | BRF_GRA },           // 19
};

STD_ROM_PICK(ws90)
STD_ROM_FN(ws90)

static INT32 Ws90Init()
{
	namcos1_key_init(3, 0x136, 4, -1,  7, -1,  3, -1);

	return DrvInit();
}

struct BurnDriver BurnDrvWs90 = {
	"ws90", "ws", NULL, NULL, "1990",
	"World Stadium '90 (Japan)\0", NULL, "Namco", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, ws90RomInfo, ws90RomName, NULL, NULL, DrvInputInfo, Ws90DIPInfo,
	Ws90Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};


// Pistol Daimyo no Bouken (Japan)

static struct BurnRomInfo pistoldmRomDesc[] = {
	{ "pd1_snd0.bin",	0x20000, 0x026da54e, 1 | BRF_PRG | BRF_ESS }, //  0 Sound m6809 Code

	{ "pd1_prg0.bin",	0x20000, 0x9db9b89c, 2 | BRF_PRG | BRF_ESS }, //  1 Main and Sub m6809 Code
	{ "pd1prg7b.bin",	0x20000, 0x7189b797, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "cus64-64a1.mcu",	0x01000, 0xffb5c0bd, 3 | BRF_PRG | BRF_ESS }, //  3 Internal MCU Code

	{ "pd_voi-0.bin",	0x20000, 0xad1b8128, 4 | BRF_PRG | BRF_ESS }, //  4 External MCU Code
	{ "pd_voi-1.bin",	0x20000, 0x2871c494, 4 | BRF_PRG | BRF_ESS }, //  5
	{ "pd_voi-2.bin",	0x20000, 0xe783f0c4, 4 | BRF_PRG | BRF_ESS }, //  6

	{ "pd_chr-8.bin",	0x20000, 0xa5f516db, 5 | BRF_GRA },           //  7 Character Pixel Masks

	{ "pd_chr-0.bin",	0x20000, 0xadbbaf5c, 6 | BRF_GRA },           //  8 Characters
	{ "pd_chr-1.bin",	0x20000, 0xb4e4f554, 6 | BRF_GRA },           //  9
	{ "pd_chr-2.bin",	0x20000, 0x84592540, 6 | BRF_GRA },           // 10
	{ "pd_chr-3.bin",	0x20000, 0x450bdaa9, 6 | BRF_GRA },           // 11

	{ "pd_obj-0.bin",	0x20000, 0x7269821d, 7 | BRF_GRA },           // 12 Sprites
	{ "pd_obj-1.bin",	0x20000, 0x4f9738e5, 7 | BRF_GRA },           // 13
	{ "pd_obj-2.bin",	0x20000, 0x33208776, 7 | BRF_GRA },           // 14
	{ "pd_obj-3.bin",	0x20000, 0x0dbd54ef, 7 | BRF_GRA },           // 15
	{ "pd_obj-4.bin",	0x20000, 0x58e838e2, 7 | BRF_GRA },           // 16
	{ "pd_obj-5.bin",	0x20000, 0x414f9a9d, 7 | BRF_GRA },           // 17
	{ "pd_obj-6.bin",	0x20000, 0x91b4e6e0, 7 | BRF_GRA },           // 18
	{ "pd_obj-7.bin",	0x20000, 0x00d4a8f0, 7 | BRF_GRA },           // 19
};

STD_ROM_PICK(pistoldm)
STD_ROM_FN(pistoldm)

static INT32 PistoldmInit()
{
	namcos1_key_init(3, 0x135, 1,  2,  0, -1,  4, -1);
	dac_kludge = 3;

	return DrvInit();
}

struct BurnDriver BurnDrvPistoldm = {
	"pistoldm", NULL, NULL, NULL, "1990",
	"Pistol Daimyo no Bouken (Japan)\0", NULL, "Namco", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_HORSHOOT, 0,
	NULL, pistoldmRomInfo, pistoldmRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	PistoldmInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};


// Boxy Boy (SB?)

static struct BurnRomInfo boxyboyRomDesc[] = {
	{ "sb1_snd0.bin",	0x10000, 0xbf46a106, 1 | BRF_PRG | BRF_ESS }, //  0 Sound m6809 Code

	{ "sb1_prg0.bin",	0x20000, 0x8af8cb73, 2 | BRF_PRG | BRF_ESS }, //  1 Main and Sub m6809 Code
	{ "sb1_prg1.bin",	0x20000, 0x5d1fdd94, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "sbx_prg7.bin",	0x10000, 0x7787c72e, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "cus64-64a1.mcu",	0x01000, 0xffb5c0bd, 3 | BRF_PRG | BRF_ESS }, //  4 Internal MCU Code

	{ "sb1_voi0.bin",	0x10000, 0x63d9cedf, 4 | BRF_PRG | BRF_ESS }, //  5 External MCU Code

	{ "sb1_chr8.bin",	0x10000, 0x5692b297, 5 | BRF_GRA },           //  6 Character Pixel Masks

	{ "sb1_chr0.bin",	0x20000, 0x267f1331, 6 | BRF_GRA },           //  7 Characters
	{ "sb1_chr1.bin",	0x20000, 0xe5ff61ad, 6 | BRF_GRA },           //  8
	{ "sb1_chr2.bin",	0x20000, 0x099b746b, 6 | BRF_GRA },           //  9
	{ "sb1_chr3.bin",	0x20000, 0x1551bb7c, 6 | BRF_GRA },           // 10

	{ "sb1_obj0.bin",	0x10000, 0xed810da4, 7 | BRF_GRA },           // 11 Sprites
};

STD_ROM_PICK(boxyboy)
STD_ROM_FN(boxyboy)

static INT32 BoxyboyInit()
{
	namcos1_key_init(3, 0x137, 2,  3,  0, -1,  4, -1);

	return DrvInit();
}

struct BurnDriver BurnDrvBoxyboy = {
	"boxyboy", NULL, NULL, NULL, "1990",
	"Boxy Boy (SB?)\0", NULL, "Namco", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, boxyboyRomInfo, boxyboyRomName, NULL, NULL, DrvInputInfo, BoxyboyDIPInfo,
	BoxyboyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};


// Souko Ban Deluxe (Japan, SB1)

static struct BurnRomInfo soukobdxRomDesc[] = {
	{ "sb1_snd0.bin",	0x10000, 0xbf46a106, 1 | BRF_PRG | BRF_ESS }, //  0 Sound m6809 Code

	{ "sb1_prg0.bin",	0x20000, 0x8af8cb73, 2 | BRF_PRG | BRF_ESS }, //  1 Main and Sub m6809 Code
	{ "sb1_prg1.bin",	0x20000, 0x5d1fdd94, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "sb1_prg7.bin",	0x10000, 0xc3bd418a, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "cus64-64a1.mcu",	0x01000, 0xffb5c0bd, 3 | BRF_PRG | BRF_ESS }, //  4 Internal MCU Code

	{ "sb1_voi0.bin",	0x10000, 0x63d9cedf, 4 | BRF_PRG | BRF_ESS }, //  5 External MCU Code

	{ "sb1_chr8.bin",	0x10000, 0x5692b297, 5 | BRF_GRA },           //  6 Character Pixel Masks

	{ "sb1_chr0.bin",	0x20000, 0x267f1331, 6 | BRF_GRA },           //  7 Characters
	{ "sb1_chr1.bin",	0x20000, 0xe5ff61ad, 6 | BRF_GRA },           //  8
	{ "sb1_chr2.bin",	0x20000, 0x099b746b, 6 | BRF_GRA },           //  9
	{ "sb1_chr3.bin",	0x20000, 0x1551bb7c, 6 | BRF_GRA },           // 10

	{ "sb1_obj0.bin",	0x10000, 0xed810da4, 7 | BRF_GRA },           // 11 Sprites
};

STD_ROM_PICK(soukobdx)
STD_ROM_FN(soukobdx)

struct BurnDriver BurnDrvSoukobdx = {
	"soukobdx", "boxyboy", NULL, NULL, "1990",
	"Souko Ban Deluxe (Japan, SB1)\0", NULL, "Namco", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, soukobdxRomInfo, soukobdxRomName, NULL, NULL, DrvInputInfo, BoxyboyDIPInfo,
	BoxyboyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};


// Puzzle Club (Japan prototype)

static struct BurnRomInfo puzlclubRomDesc[] = {
	{ "pc1_s0.bin",		0x10000, 0x44737c02, 1 | BRF_PRG | BRF_ESS }, //  0 Sound m6809 Code

	{ "pc1_p0.bin",		0x10000, 0x2db477c8, 2 | BRF_PRG | BRF_ESS }, //  1 Main and Sub m6809 Code
	{ "pc1_p1.bin",		0x10000, 0xdfd9108a, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "pc1_p7.bin",		0x10000, 0xf0638260, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "cus64-64a1.mcu",	0x01000, 0xffb5c0bd, 3 | BRF_PRG | BRF_ESS }, //  4 Internal MCU Code

	{ "pc1-c8.bin",		0x20000, 0x4e196bcd, 5 | BRF_GRA },           //  5 Character Pixel Masks

	{ "pc1-c0.bin",		0x20000, 0xad7b134e, 6 | BRF_GRA },           //  6 Characters
	{ "pc1-c1.bin",		0x20000, 0x10cb3207, 6 | BRF_GRA },           //  7
	{ "pc1-c2.bin",		0x20000, 0xd98d2c8f, 6 | BRF_GRA },           //  8
	{ "pc1-c3.bin",		0x20000, 0x91a61d96, 6 | BRF_GRA },           //  9
	{ "pc1-c4.bin",		0x20000, 0xf1c95296, 6 | BRF_GRA },           // 10
	{ "pc1-c5.bin",		0x20000, 0xbc443c27, 6 | BRF_GRA },           // 11
	{ "pc1-c6.bin",		0x20000, 0xec0a3dc5, 6 | BRF_GRA },           // 12
	{ "pc1-c7.bin",		0x20000, 0x00000000, 0 | BRF_NODUMP | BRF_GRA }, // 13
};

STD_ROM_PICK(puzlclub)
STD_ROM_FN(puzlclub)

static INT32 PuzlclubInit()
{
	namcos1_key_init(1, 0x035, -1, -1, -1, -1, -1, -1);

	return DrvInit();
}

struct BurnDriver BurnDrvPuzlclub = {
	"puzlclub", NULL, NULL, NULL, "1990",
	"Puzzle Club (Japan prototype)\0", NULL, "Namco", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, puzlclubRomInfo, puzlclubRomName, NULL, NULL, DrvInputInfo, PuzlclubDIPInfo,
	PuzlclubInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	224, 288, 3, 4
};


// Tank Force (US, 2 Players)

static struct BurnRomInfo tankfrceRomDesc[] = {
	{ "tf1_snd0.bin",	0x20000, 0x4d9cf7aa, 1 | BRF_PRG | BRF_ESS }, //  0 Sound m6809 Code

	{ "tf1_prg0.bin",	0x20000, 0x2ae4b9eb, 2 | BRF_PRG | BRF_ESS }, //  1 Main and Sub m6809 Code
	{ "tf1_prg1.bin",	0x20000, 0x4a8bb251, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "tf1prg7.bin",	0x20000, 0x2ec28a87, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "cus64-64a1.mcu",	0x01000, 0xffb5c0bd, 3 | BRF_PRG | BRF_ESS }, //  4 Internal MCU Code

	{ "tf1_voi0.bin",	0x20000, 0xf542676a, 4 | BRF_PRG | BRF_ESS }, //  5 External MCU Code
	{ "tf1_voi1.bin",	0x20000, 0x615d09cd, 4 | BRF_PRG | BRF_ESS }, //  6

	{ "tf1_chr8.bin",	0x20000, 0x7d53b31e, 5 | BRF_GRA },           //  7 Character Pixel Masks

	{ "tf1_chr0.bin",	0x20000, 0x9e91794e, 6 | BRF_GRA },           //  8 Characters
	{ "tf1_chr1.bin",	0x20000, 0x76e1bc56, 6 | BRF_GRA },           //  9
	{ "tf1_chr2.bin",	0x20000, 0xfcb645d9, 6 | BRF_GRA },           // 10
	{ "tf1_chr3.bin",	0x20000, 0xa8dbf080, 6 | BRF_GRA },           // 11
	{ "tf1_chr4.bin",	0x20000, 0x51fedc8c, 6 | BRF_GRA },           // 12
	{ "tf1_chr5.bin",	0x20000, 0xe6c6609a, 6 | BRF_GRA },           // 13

	{ "tf1_obj0.bin",	0x20000, 0x4bedd51a, 7 | BRF_GRA },           // 14 Sprites
	{ "tf1_obj1.bin",	0x20000, 0xdf674d6d, 7 | BRF_GRA },           // 15
};

STD_ROM_PICK(tankfrce)
STD_ROM_FN(tankfrce)

static INT32 TankfrceInit()
{
	namcos1_key_init(3, 0x0b9, 5, -1,  1, -1, 2, -1);

	return DrvInit();
}

struct BurnDriver BurnDrvTankfrce = {
	"tankfrce", NULL, NULL, NULL, "1991",
	"Tank Force (US, 2 Players)\0", NULL, "Namco", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, tankfrceRomInfo, tankfrceRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	TankfrceInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};


// Tank Force (US, 4 Players)

static struct BurnRomInfo tankfrce4RomDesc[] = {
	{ "tf1_snd0.bin",	0x20000, 0x4d9cf7aa, 1 | BRF_PRG | BRF_ESS }, //  0 Sound m6809 Code

	{ "tf1_prg0.bin",	0x20000, 0x2ae4b9eb, 2 | BRF_PRG | BRF_ESS }, //  1 Main and Sub m6809 Code
	{ "tf1_prg1.bin",	0x20000, 0x4a8bb251, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "tfu4prg7.bin",	0x20000, 0x162adea0, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "cus64-64a1.mcu",	0x01000, 0xffb5c0bd, 3 | BRF_PRG | BRF_ESS }, //  4 Internal MCU Code

	{ "tf1_voi0.bin",	0x20000, 0xf542676a, 4 | BRF_PRG | BRF_ESS }, //  5 External MCU Code
	{ "tf1_voi1.bin",	0x20000, 0x615d09cd, 4 | BRF_PRG | BRF_ESS }, //  6

	{ "tf1_chr8.bin",	0x20000, 0x7d53b31e, 5 | BRF_GRA },           //  7 Character Pixel Masks

	{ "tf1_chr0.bin",	0x20000, 0x9e91794e, 6 | BRF_GRA },           //  8 Characters
	{ "tf1_chr1.bin",	0x20000, 0x76e1bc56, 6 | BRF_GRA },           //  9
	{ "tf1_chr2.bin",	0x20000, 0xfcb645d9, 6 | BRF_GRA },           // 10
	{ "tf1_chr3.bin",	0x20000, 0xa8dbf080, 6 | BRF_GRA },           // 11
	{ "tf1_chr4.bin",	0x20000, 0x51fedc8c, 6 | BRF_GRA },           // 12
	{ "tf1_chr5.bin",	0x20000, 0xe6c6609a, 6 | BRF_GRA },           // 13

	{ "tf1_obj0.bin",	0x20000, 0x4bedd51a, 7 | BRF_GRA },           // 14 Sprites
	{ "tf1_obj1.bin",	0x20000, 0xdf674d6d, 7 | BRF_GRA },           // 15
};

STD_ROM_PICK(tankfrce4)
STD_ROM_FN(tankfrce4)

static INT32 Tankfrce4Init()
{	
	input_read_callback = faceoff_inputs_read;

	namcos1_key_init(3, 0x0b9, 5, -1,  1, -1, 2, -1);

	return DrvInit();
}

struct BurnDriver BurnDrvTankfrce4 = {
	"tankfrce4", "tankfrce", NULL, NULL, "1991",
	"Tank Force (US, 4 Players)\0", NULL, "Namco", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, tankfrce4RomInfo, tankfrce4RomName, NULL, NULL, Tankfrce4InputInfo, FaceoffDIPInfo,
	Tankfrce4Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};


// Tank Force (Japan)

static struct BurnRomInfo tankfrcejRomDesc[] = {
	{ "tf1_snd0.bin",	0x20000, 0x4d9cf7aa, 1 | BRF_PRG | BRF_ESS }, //  0 Sound m6809 Code

	{ "tf1_prg0.bin",	0x20000, 0x2ae4b9eb, 2 | BRF_PRG | BRF_ESS }, //  1 Main and Sub m6809 Code
	{ "tf1_prg1.bin",	0x20000, 0x4a8bb251, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "tf1_prg7.bin",	0x20000, 0x9dfa0dd5, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "cus64-64a1.mcu",	0x01000, 0xffb5c0bd, 3 | BRF_PRG | BRF_ESS }, //  4 Internal MCU Code

	{ "tf1_voi0.bin",	0x20000, 0xf542676a, 4 | BRF_PRG | BRF_ESS }, //  5 External MCU Code
	{ "tf1_voi1.bin",	0x20000, 0x615d09cd, 4 | BRF_PRG | BRF_ESS }, //  6

	{ "tf1_chr8.bin",	0x20000, 0x7d53b31e, 5 | BRF_GRA },           //  7 Character Pixel Masks

	{ "tf1_chr0.bin",	0x20000, 0x9e91794e, 6 | BRF_GRA },           //  8 Characters
	{ "tf1_chr1.bin",	0x20000, 0x76e1bc56, 6 | BRF_GRA },           //  9
	{ "tf1_chr2.bin",	0x20000, 0xfcb645d9, 6 | BRF_GRA },           // 10
	{ "tf1_chr3.bin",	0x20000, 0xa8dbf080, 6 | BRF_GRA },           // 11
	{ "tf1_chr4.bin",	0x20000, 0x51fedc8c, 6 | BRF_GRA },           // 12
	{ "tf1_chr5.bin",	0x20000, 0xe6c6609a, 6 | BRF_GRA },           // 13

	{ "tf1_obj0.bin",	0x20000, 0x4bedd51a, 7 | BRF_GRA },           // 14 Sprites
	{ "tf1_obj1.bin",	0x20000, 0xdf674d6d, 7 | BRF_GRA },           // 15
};

STD_ROM_PICK(tankfrcej)
STD_ROM_FN(tankfrcej)

struct BurnDriver BurnDrvTankfrcej = {
	"tankfrcej", "tankfrce", NULL, NULL, "1991",
	"Tank Force (Japan)\0", NULL, "Namco", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, tankfrcejRomInfo, tankfrcejRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	TankfrceInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};
