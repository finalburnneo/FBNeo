// FB Alpha NMK16 driver module
// Based on MAME driver by Mirko Buffoni, Richard Bush, Nicola Salmoria, Bryan McPhail, David Haywood, and R. Belmont
// Also, a huge "thank you!" to JacKc for helping bug test

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "tlcs90_intf.h"
#include "seibusnd.h"
#include "bitswap.h"
#include "nmk112.h"
#include "nmk004.h"

#if 0
	hachamf - needs some protection work
#endif

static UINT8 *AllMem;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *Drv68KROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvTileROM;
static UINT8 *DrvSndROM0;
static UINT8 *DrvSndROM1;
static UINT8 *DrvPalRAM;
static UINT8 *DrvBgRAM0;
static UINT8 *DrvBgRAM1;
static UINT8 *DrvBgRAM2;
static UINT8 *DrvBgRAM3;
static UINT8 *DrvTxRAM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvSprBuf;
static UINT8 *DrvSprBuf2;
static UINT8 *DrvSprBuf3;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvScrollRAM;

static UINT32 *DrvPalette;

static UINT8 *soundlatch;
static UINT8 *soundlatch2;
static UINT8 *flipscreen;
static UINT8 *tilebank;
static UINT8 *okibank;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvDips[3] = { 0, 0, 0 };
static UINT8 LastFakeDip = 0xf7;
static UINT16 DrvInputs[3];
static UINT8 DrvReset;

static INT32 nGraphicsMask[3];
static INT32 videoshift = 0;
static INT32 input_high[2] = { 0, 0 };
static INT32 is_8bpp = 0;
static INT32 global_y_offset = 16;
static INT32 screen_flip_y = 0;
static UINT32 nNMK004CpuSpeed;
static INT32 nNMK004EnableIrq2;
static INT32 NMK004_enabled = 0;
static INT32 NMK112_enabled = 0;
static INT32 macross2_sound_enable;
static INT32 MSM6295x1_only = 0;
static INT32 MSM6295x2_only = 0;
static INT32 no_z80 = 0;
static INT32 AFEGA_SYS = 0;
static INT32 Tharriermode = 0; // use macross1/tharrier text draw & joy inputs
static INT32 Macrossmode = 0; // use macross1 text draw
static INT32 Strahlmode = 0;
static INT32 Tdragon2mode = 0; // use draw_sprites_tdragon2()
static INT32 GunnailMode = 0;
static INT32 TharrierShakey = 0; // kludge for shakey-ship on the end of level cutscene

static INT32 mustang_bg_xscroll = 0;

static struct BurnInputInfo CommonInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 11,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 10,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 9,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 8,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 13,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Common)

static struct BurnInputInfo GunnailInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 11,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 10,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 9,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 8,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 13,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Gunnail)

static struct BurnInputInfo TharrierInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy3 + 6,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy3 + 7,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 14,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 13,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 12,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 9,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 10,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Tharrier)

static struct BurnInputInfo ManyblocInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 15,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 8,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 14,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 13,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 12,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 9,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 10,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Manybloc)

static struct BurnInputInfo SsmissinInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 11,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 10,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 9,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 8,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 13,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Ssmissin)

static struct BurnInputInfo Tdragon2InputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 11,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 10,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 9,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 8,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 13,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 14,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Tdragon2)

static struct BurnInputInfo AcrobatmInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 11,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 10,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 9,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 8,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 13,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Acrobatm)

static struct BurnInputInfo DolmenInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 11,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 10,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 9,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 8,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 13,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Dolmen)

static struct BurnDIPInfo RedhawkbDIPList[]=
{
	{0x12, 0xff, 0xff, 0x00, NULL			},
	{0x13, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x02, 0x02, "Off"			},
	{0x12, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x12, 0x01, 0xc0, 0xc0, "1"			},
	{0x12, 0x01, 0xc0, 0x40, "2"			},
	{0x12, 0x01, 0xc0, 0x00, "3"			},
	{0x12, 0x01, 0xc0, 0x80, "5"			},

//	{0   , 0xfe, 0   ,    4, "Flip Screen"		},
//	{0x13, 0x01, 0x03, 0x00, "Off"			},
//	{0x13, 0x01, 0x03, 0x03, "On"			},
//	{0x13, 0x01, 0x03, 0x01, "Horizontally"		},
//	{0x13, 0x01, 0x03, 0x02, "Vertically"		},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x13, 0x01, 0x04, 0x00, "Off"			},
	{0x13, 0x01, 0x04, 0x04, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x18, 0x10, "Easy"			},
	{0x13, 0x01, 0x18, 0x00, "Normal"		},
	{0x13, 0x01, 0x18, 0x08, "Hard"			},
	{0x13, 0x01, 0x18, 0x18, "Hardest"		},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x13, 0x01, 0xe0, 0xe0, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0x60, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0xa0, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0x20, "3 Coins 2 Credits"	},
	{0x13, 0x01, 0xe0, 0x00, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xe0, 0xc0, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0xe0, 0x80, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xe0, 0x40, "1 Coin  3 Credits"	},
};

STDDIPINFO(Redhawkb)

static struct BurnDIPInfo RapheroDIPList[]=
{
	{0x14, 0xff, 0xff, 0xfd, NULL			},
	{0x15, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x01, 0x01, "Off"			},
	{0x14, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Language"		},
	{0x14, 0x01, 0x02, 0x02, "Japanese"		},
	{0x14, 0x01, 0x02, 0x00, "English"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x30, 0x10, "Easy"			},
	{0x14, 0x01, 0x30, 0x30, "Normal"		},
	{0x14, 0x01, 0x30, 0x20, "Hard"			},
	{0x14, 0x01, 0x30, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0xc0, 0x00, "1"			},
	{0x14, 0x01, 0xc0, 0x40, "2"			},
	{0x14, 0x01, 0xc0, 0xc0, "3"			},
	{0x14, 0x01, 0xc0, 0x80, "4"			},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
//	{0x15, 0x01, 0x01, 0x01, "Off"			},
//	{0x15, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x15, 0x01, 0x02, 0x00, "Off"			},
	{0x15, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x15, 0x01, 0x1c, 0x10, "4 Coins 1 Credits"	},
	{0x15, 0x01, 0x1c, 0x08, "3 Coins 1 Credits"	},
	{0x15, 0x01, 0x1c, 0x18, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0x1c, 0x1c, "1 Coin  1 Credits"	},
	{0x15, 0x01, 0x1c, 0x0c, "1 Coin  2 Credits"	},
	{0x15, 0x01, 0x1c, 0x14, "1 Coin  3 Credits"	},
	{0x15, 0x01, 0x1c, 0x04, "1 Coin  4 Credits"	},
	{0x15, 0x01, 0x1c, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x15, 0x01, 0xe0, 0x80, "4 Coins 1 Credits"	},
	{0x15, 0x01, 0xe0, 0x40, "3 Coins 1 Credits"	},
	{0x15, 0x01, 0xe0, 0xc0, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"	},
	{0x15, 0x01, 0xe0, 0x60, "1 Coin  2 Credits"	},
	{0x15, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"	},
	{0x15, 0x01, 0xe0, 0x20, "1 Coin  4 Credits"	},
	{0x15, 0x01, 0xe0, 0x00, "Free Play"		},
};

STDDIPINFO(Raphero)

static struct BurnDIPInfo BioshipDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0xff, NULL			},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
//	{0x12, 0x01, 0x01, 0x01, "Off"			},
//	{0x12, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x12, 0x01, 0x06, 0x00, "Easy"			},
	{0x12, 0x01, 0x06, 0x06, "Normal"		},
	{0x12, 0x01, 0x06, 0x02, "Hard"			},
	{0x12, 0x01, 0x06, 0x04, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x12, 0x01, 0x08, 0x08, "Off"			},
	{0x12, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x20, 0x00, "Off"			},
	{0x12, 0x01, 0x20, 0x20, "On"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x12, 0x01, 0xc0, 0x00, "2"			},
	{0x12, 0x01, 0xc0, 0xc0, "3"			},
	{0x12, 0x01, 0xc0, 0x80, "4"			},
	{0x12, 0x01, 0xc0, 0x40, "5"			},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x13, 0x01, 0x1c, 0x00, "5 Coins 1 Credits"	},
	{0x13, 0x01, 0x1c, 0x10, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x1c, 0x08, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x1c, 0x18, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x1c, 0x1c, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x1c, 0x0c, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x1c, 0x14, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0x1c, 0x04, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x13, 0x01, 0xe0, 0x00, "5 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0x80, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0x40, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0xc0, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xe0, 0x60, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0xe0, 0x20, "1 Coin  4 Credits"	},
};

STDDIPINFO(Bioship)

static struct BurnDIPInfo StrahlDIPList[]=
{
	{0x12, 0xff, 0xff, 0x7f, NULL			},
	{0x13, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x12, 0x01, 0x07, 0x00, "5 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x01, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x02, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x03, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x07, 0x04, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x12, 0x01, 0x38, 0x00, "5 Coins 1 Credits"	},
	{0x12, 0x01, 0x38, 0x08, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0x38, 0x10, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x38, 0x18, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x38, 0x30, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x38, 0x28, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x38, 0x20, "1 Coin  4 Credits"	},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
//	{0x12, 0x01, 0x40, 0x40, "Off"			},
//	{0x12, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x80, 0x80, "Off"			},
	{0x12, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0x03, 0x02, "2"			},
	{0x13, 0x01, 0x03, 0x03, "3"			},
	{0x13, 0x01, 0x03, 0x01, "4"			},
	{0x13, 0x01, 0x03, 0x00, "5"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x0c, 0x08, "Easy"			},
	{0x13, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x13, 0x01, 0x0c, 0x04, "Hard"			},
	{0x13, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x13, 0x01, 0x60, 0x40, "100k and every 200k"	},
	{0x13, 0x01, 0x60, 0x60, "200k and every 200k"	},
	{0x13, 0x01, 0x60, 0x20, "300k and every 300k"	},
	{0x13, 0x01, 0x60, 0x00, "None"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x13, 0x01, 0x80, 0x80, "Off"			},
	{0x13, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Strahl)

static struct BurnDIPInfo HachamfDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0xfd, NULL			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x12, 0x01, 0x07, 0x01, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x02, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x03, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x07, 0x04, "1 Coin  4 Credits"	},
	{0x12, 0x01, 0x07, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x12, 0x01, 0x38, 0x08, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0x38, 0x10, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x38, 0x18, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x38, 0x30, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x38, 0x28, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x38, 0x20, "1 Coin  4 Credits"	},
	{0x12, 0x01, 0x38, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x40, 0x00, "Off"			},
	{0x12, 0x01, 0x40, 0x40, "On"			},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
//	{0x13, 0x01, 0x01, 0x01, "Off"			},
//	{0x13, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Language"		},
	{0x13, 0x01, 0x02, 0x00, "English"		},
	{0x13, 0x01, 0x02, 0x02, "Japanese"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x0c, 0x04, "Easy"			},
	{0x13, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x13, 0x01, 0x0c, 0x08, "Hard"			},
	{0x13, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0xc0, 0x00, "1"			},
	{0x13, 0x01, 0xc0, 0x40, "2"			},
	{0x13, 0x01, 0xc0, 0xc0, "3"			},
	{0x13, 0x01, 0xc0, 0x80, "4"			},
};

STDDIPINFO(Hachamf)

static struct BurnDIPInfo HachamfbDIPList[]=
{
	{0x12, 0xff, 0xff, 0xfd, NULL			},
	{0x13, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Language"		},
	{0x12, 0x01, 0x02, 0x00, "English"		},
	{0x12, 0x01, 0x02, 0x02, "Japanese"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x12, 0x01, 0x0c, 0x04, "Easy"			},
	{0x12, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x12, 0x01, 0x0c, 0x08, "Hard"			},
	{0x12, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x12, 0x01, 0xc0, 0x00, "1"			},
	{0x12, 0x01, 0xc0, 0x40, "2"			},
	{0x12, 0x01, 0xc0, 0xc0, "3"			},
	{0x12, 0x01, 0xc0, 0x80, "4"			},
};

STDDIPINFO(Hachamfb)

static struct BurnDIPInfo VandykeDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x12, 0x01, 0x01, 0x00, "2"			},
	{0x12, 0x01, 0x01, 0x01, "3"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x12, 0x01, 0x08, 0x08, "Off"			},
	{0x12, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x20, 0x00, "Off"			},
	{0x12, 0x01, 0x20, 0x20, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x12, 0x01, 0xc0, 0x00, "Easy"			},
	{0x12, 0x01, 0xc0, 0xc0, "Normal"		},
	{0x12, 0x01, 0xc0, 0x40, "Hard"			},
	{0x12, 0x01, 0xc0, 0x80, "Hardest"		},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
//	{0x13, 0x01, 0x01, 0x01, "Off"			},
//	{0x13, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x13, 0x01, 0x1c, 0x00, "5 Coins 1 Credits"	},
	{0x13, 0x01, 0x1c, 0x10, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x1c, 0x08, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x1c, 0x18, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x1c, 0x1c, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x1c, 0x0c, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x1c, 0x14, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0x1c, 0x04, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x13, 0x01, 0xe0, 0x00, "5 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0x80, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0x40, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0xc0, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xe0, 0x60, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0xe0, 0x20, "1 Coin  4 Credits"	},
};

STDDIPINFO(Vandyke)


static struct BurnDIPInfo BlkheartDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0xff, NULL			},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
//	{0x12, 0x01, 0x01, 0x01, "Off"			},
//	{0x12, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x12, 0x01, 0x02, 0x02, "Off"			},
	{0x12, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x12, 0x01, 0x0c, 0x0c, "Easy"			},
	{0x12, 0x01, 0x0c, 0x08, "Normal"		},
	{0x12, 0x01, 0x0c, 0x04, "Hard"			},
	{0x12, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x12, 0x01, 0xc0, 0x40, "2"			},
	{0x12, 0x01, 0xc0, 0xc0, "3"			},
	{0x12, 0x01, 0xc0, 0x80, "4"			},
	{0x12, 0x01, 0xc0, 0x00, "5"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x02, 0x00, "Off"			},
	{0x13, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x13, 0x01, 0x1c, 0x10, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x1c, 0x08, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x1c, 0x18, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x1c, 0x1c, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x1c, 0x0c, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x1c, 0x14, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0x1c, 0x04, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0x1c, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x13, 0x01, 0xe0, 0x80, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0x40, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0xc0, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xe0, 0x60, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0xe0, 0x20, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0xe0, 0x00, "Free Play"		},
};

STDDIPINFO(Blkheart)

static struct BurnDIPInfo MacrossDIPList[]=
{
	{0x12, 0xff, 0xff, 0xf7, NULL			},
	{0x13, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x12, 0x01, 0x01, 0x01, "Off"			},
	{0x12, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x02, 0x00, "Off"			},
	{0x12, 0x01, 0x02, 0x02, "On"			},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
//	{0x12, 0x01, 0x04, 0x04, "Off"			},
//	{0x12, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Language"		},
	{0x12, 0x01, 0x08, 0x00, "English"		},
	{0x12, 0x01, 0x08, 0x08, "Japanese"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x12, 0x01, 0x30, 0x10, "Easy"			},
	{0x12, 0x01, 0x30, 0x30, "Normal"		},
	{0x12, 0x01, 0x30, 0x20, "Hard"			},
	{0x12, 0x01, 0x30, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x12, 0x01, 0xc0, 0x00, "1"			},
	{0x12, 0x01, 0xc0, 0x40, "2"			},
	{0x12, 0x01, 0xc0, 0xc0, "3"			},
	{0x12, 0x01, 0xc0, 0x80, "4"			},

	{0   , 0xfe, 0   ,    16, "Coin B"		},
	{0x13, 0x01, 0x0f, 0x04, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x0f, 0x0a, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x0f, 0x01, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x0f, 0x00, "5 Coins 3 Credits"	},
	{0x13, 0x01, 0x0f, 0x02, "3 Coins 2 Credits"	},
	{0x13, 0x01, 0x0f, 0x08, "4 Coins 3 Credits"	},
	{0x13, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x0f, 0x0c, "3 Coins 4 Credits"	},
	{0x13, 0x01, 0x0f, 0x0e, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0x0f, 0x07, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x13, 0x01, 0x0f, 0x0b, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0x0f, 0x03, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0x0f, 0x0d, "1 Coin  5 Credits"	},
	{0x13, 0x01, 0x0f, 0x05, "1 Coin  6 Credits"	},
	{0x13, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},

	{0   , 0xfe, 0   ,    16, "Coin A"		},
	{0x13, 0x01, 0xf0, 0x40, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0xf0, 0xa0, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0xf0, 0x10, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0xf0, 0x20, "3 Coins 2 Credits"	},
	{0x13, 0x01, 0xf0, 0x80, "4 Coins 3 Credits"	},
	{0x13, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xf0, 0xc0, "3 Coins 4 Credits"	},
	{0x13, 0x01, 0xf0, 0xe0, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0xf0, 0x70, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x13, 0x01, 0xf0, 0xb0, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0xf0, 0x30, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0xf0, 0xd0, "1 Coin  5 Credits"	},
	{0x13, 0x01, 0xf0, 0x50, "1 Coin  6 Credits"	},
	{0x13, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x13, 0x01, 0xf0, 0x00, "Free Play"		},
};

STDDIPINFO(Macross)

static struct BurnDIPInfo TharrierDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL				},
	{0x14, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x13, 0x01, 0x02, 0x00, "Off"				},
	{0x13, 0x01, 0x02, 0x02, "On"				},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x13, 0x01, 0x1c, 0x10, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x1c, 0x08, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x1c, 0x18, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x1c, 0x1c, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x1c, 0x0c, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x1c, 0x14, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0x1c, 0x04, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0x1c, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x13, 0x01, 0xe0, 0x80, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0xe0, 0x40, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0xe0, 0xc0, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0xe0, 0x60, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0xe0, 0x20, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0xe0, 0x00, "Free Play"			},

//	{0   , 0xfe, 0   ,    2, "Cabinet"			},
//	{0x14, 0x01, 0x01, 0x01, "Upright"			},
//	{0x14, 0x01, 0x01, 0x00, "Cocktail"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x14, 0x01, 0x0c, 0x04, "Easy"				},	
	{0x14, 0x01, 0x0c, 0x0c, "Normal"			},
	{0x14, 0x01, 0x0c, 0x08, "Hard"				},
	{0x14, 0x01, 0x0c, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x14, 0x01, 0x30, 0x30, "200k"				},
	{0x14, 0x01, 0x30, 0x20, "200k and 1 Mil"		},
	{0x14, 0x01, 0x30, 0x00, "200k, 500k & 1,2,3,5 Mil"	},
	{0x14, 0x01, 0x30, 0x10, "None"				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x14, 0x01, 0xc0, 0x40, "2"				},
	{0x14, 0x01, 0xc0, 0xc0, "3"				},
	{0x14, 0x01, 0xc0, 0x80, "4"				},
	{0x14, 0x01, 0xc0, 0x00, "5"				},
};

STDDIPINFO(Tharrier)

static struct BurnDIPInfo ManyblocDIPList[]=
{
	{0x11, 0xff, 0xff, 0x18, NULL			},
	{0x12, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    2, "Slot System"		},
	{0x11, 0x01, 0x01, 0x00, "Off"			},
	{0x11, 0x01, 0x01, 0x01, "On"			},

	{0   , 0xfe, 0   ,    2, "Explanation"		},
	{0x11, 0x01, 0x02, 0x00, "English"		},
	{0x11, 0x01, 0x02, 0x02, "Japanese"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x11, 0x01, 0x04, 0x04, "Off"			},
	{0x11, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x11, 0x01, 0x08, 0x08, "Upright"		},
	{0x11, 0x01, 0x08, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x11, 0x01, 0x10, 0x10, "Off"			},
	{0x11, 0x01, 0x10, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x11, 0x01, 0x60, 0x60, "Easy"			},
	{0x11, 0x01, 0x60, 0x00, "Normal"		},
	{0x11, 0x01, 0x60, 0x20, "Hard"			},
	{0x11, 0x01, 0x60, 0x40, "Hardest"		},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
//	{0x11, 0x01, 0x80, 0x00, "Off"			},
//	{0x11, 0x01, 0x80, 0x80, "On"			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x12, 0x01, 0x07, 0x07, "5 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x06, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x05, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x04, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x00, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x07, 0x01, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x07, 0x02, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x07, 0x03, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x12, 0x01, 0x38, 0x38, "5 Coins 1 Credits"	},
	{0x12, 0x01, 0x38, 0x30, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0x38, 0x28, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x38, 0x20, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x38, 0x00, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x38, 0x08, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x38, 0x10, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x38, 0x18, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    4, "Plate Probability"	},
	{0x12, 0x01, 0xc0, 0xc0, "Bad"			},
	{0x12, 0x01, 0xc0, 0x00, "Normal"		},
	{0x12, 0x01, 0xc0, 0x40, "Better"		},
	{0x12, 0x01, 0xc0, 0x80, "Best"			},
};

STDDIPINFO(Manybloc)

static struct BurnDIPInfo SsmissinDIPList[]=
{
	{0x11, 0xff, 0xff, 0xff, NULL			},
	{0x12, 0xff, 0xff, 0xff, NULL			},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
//	{0x11, 0x01, 0x01, 0x01, "Off"			},
//	{0x11, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x11, 0x01, 0x0c, 0x04, "Easy"			},
	{0x11, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x11, 0x01, 0x0c, 0x08, "Hard"			},
	{0x11, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x11, 0x01, 0xc0, 0x00, "1"			},
	{0x11, 0x01, 0xc0, 0x40, "2"			},
	{0x11, 0x01, 0xc0, 0xc0, "3"			},
	{0x11, 0x01, 0xc0, 0x80, "4"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x02, 0x00, "Off"			},
	{0x12, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x12, 0x01, 0xe0, 0x20, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0xe0, 0xa0, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0xe0, 0x60, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0xe0, 0xc0, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0xe0, 0x40, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0xe0, 0x80, "1 Coin  4 Credits"	},
	{0x12, 0x01, 0xe0, 0x00, "Free Play"		},
};

STDDIPINFO(Ssmissin)

static struct BurnDIPInfo Macross2DIPList[]=
{
	{0x12, 0xff, 0xff, 0xf7, NULL			},
	{0x13, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x12, 0x01, 0x01, 0x01, "Off"			},
	{0x12, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x02, 0x00, "Off"			},
	{0x12, 0x01, 0x02, 0x02, "On"			},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
//	{0x12, 0x01, 0x04, 0x04, "Off"			},
//	{0x12, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Language"		},
	{0x12, 0x01, 0x08, 0x00, "English"		},
	{0x12, 0x01, 0x08, 0x08, "Japanese"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x12, 0x01, 0x30, 0x10, "Easy"			},
	{0x12, 0x01, 0x30, 0x30, "Normal"		},
	{0x12, 0x01, 0x30, 0x20, "Hard"			},
	{0x12, 0x01, 0x30, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    16, "Coin B"		},
	{0x13, 0x01, 0x0f, 0x04, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x0f, 0x0a, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x0f, 0x01, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x0f, 0x00, "5 Coins 3 Credits"	},
	{0x13, 0x01, 0x0f, 0x02, "3 Coins 2 Credits"	},
	{0x13, 0x01, 0x0f, 0x08, "4 Coins 3 Credits"	},
	{0x13, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x0f, 0x0c, "3 Coins 4 Credits"	},
	{0x13, 0x01, 0x0f, 0x0e, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0x0f, 0x07, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x13, 0x01, 0x0f, 0x0b, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0x0f, 0x03, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0x0f, 0x0d, "1 Coin  5 Credits"	},
	{0x13, 0x01, 0x0f, 0x05, "1 Coin  6 Credits"	},
	{0x13, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},

	{0   , 0xfe, 0   ,    16, "Coin A"		},
	{0x13, 0x01, 0xf0, 0x40, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0xf0, 0xa0, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0xf0, 0x10, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0xf0, 0x20, "3 Coins 2 Credits"	},
	{0x13, 0x01, 0xf0, 0x80, "4 Coins 3 Credits"	},
	{0x13, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xf0, 0xc0, "3 Coins 4 Credits"	},
	{0x13, 0x01, 0xf0, 0xe0, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0xf0, 0x70, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x13, 0x01, 0xf0, 0xb0, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0xf0, 0x30, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0xf0, 0xd0, "1 Coin  5 Credits"	},
	{0x13, 0x01, 0xf0, 0x50, "1 Coin  6 Credits"	},
	{0x13, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x13, 0x01, 0xf0, 0x00, "Free Play"		},
};

STDDIPINFO(Macross2)

static struct BurnDIPInfo Tdragon2DIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL			},
	{0x15, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x01, 0x01, "Off"			},
	{0x14, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x14, 0x01, 0x02, 0x00, "Off"			},
	{0x14, 0x01, 0x02, 0x02, "On"			},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
//	{0x14, 0x01, 0x04, 0x04, "Off"			},
//	{0x14, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x30, 0x10, "Easy"			},
	{0x14, 0x01, 0x30, 0x30, "Normal"		},
	{0x14, 0x01, 0x30, 0x20, "Hard"			},
	{0x14, 0x01, 0x30, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0xc0, 0x00, "1"			},
	{0x14, 0x01, 0xc0, 0x40, "2"			},
	{0x14, 0x01, 0xc0, 0xc0, "3"			},
	{0x14, 0x01, 0xc0, 0x80, "4"			},

	{0   , 0xfe, 0   ,    16, "Coin B"		},
	{0x15, 0x01, 0x0f, 0x04, "4 Coins 1 Credits"	},
	{0x15, 0x01, 0x0f, 0x0a, "3 Coins 1 Credits"	},
	{0x15, 0x01, 0x0f, 0x01, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0x0f, 0x00, "5 Coins 3 Credits"	},
	{0x15, 0x01, 0x0f, 0x02, "3 Coins 2 Credits"	},
	{0x15, 0x01, 0x0f, 0x08, "4 Coins 3 Credits"	},
	{0x15, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x15, 0x01, 0x0f, 0x0c, "3 Coins 4 Credits"	},
	{0x15, 0x01, 0x0f, 0x0e, "2 Coins 3 Credits"	},
	{0x15, 0x01, 0x0f, 0x07, "1 Coin  2 Credits"	},
	{0x15, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x15, 0x01, 0x0f, 0x0b, "1 Coin  3 Credits"	},
	{0x15, 0x01, 0x0f, 0x03, "1 Coin  4 Credits"	},
	{0x15, 0x01, 0x0f, 0x0d, "1 Coin  5 Credits"	},
	{0x15, 0x01, 0x0f, 0x05, "1 Coin  6 Credits"	},
	{0x15, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},

	{0   , 0xfe, 0   ,    16, "Coin A"		},
	{0x15, 0x01, 0xf0, 0x40, "4 Coins 1 Credits"	},
	{0x15, 0x01, 0xf0, 0xa0, "3 Coins 1 Credits"	},
	{0x15, 0x01, 0xf0, 0x10, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0xf0, 0x20, "3 Coins 2 Credits"	},
	{0x15, 0x01, 0xf0, 0x80, "4 Coins 3 Credits"	},
	{0x15, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x15, 0x01, 0xf0, 0xc0, "3 Coins 4 Credits"	},
	{0x15, 0x01, 0xf0, 0xe0, "2 Coins 3 Credits"	},
	{0x15, 0x01, 0xf0, 0x70, "1 Coin  2 Credits"	},
	{0x15, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x15, 0x01, 0xf0, 0xb0, "1 Coin  3 Credits"	},
	{0x15, 0x01, 0xf0, 0x30, "1 Coin  4 Credits"	},
	{0x15, 0x01, 0xf0, 0xd0, "1 Coin  5 Credits"	},
	{0x15, 0x01, 0xf0, 0x50, "1 Coin  6 Credits"	},
	{0x15, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x15, 0x01, 0xf0, 0x00, "Free Play"		},
};

STDDIPINFO(Tdragon2)

static struct BurnDIPInfo Stagger1DIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x12, 0x01, 0x01, 0x01, "Off"			},
	{0x12, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x02, 0x00, "Off"			},
	{0x12, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x12, 0x01, 0xc0, 0x00, "1"			},
	{0x12, 0x01, 0xc0, 0x80, "2"			},
	{0x12, 0x01, 0xc0, 0xc0, "3"			},
	{0x12, 0x01, 0xc0, 0x40, "5"			},

//	{0   , 0xfe, 0   ,    4, "Flip Screen"		},
//	{0x13, 0x01, 0x03, 0x03, "Off"			},
//	{0x13, 0x01, 0x03, 0x00, "On"			},
//	{0x13, 0x01, 0x03, 0x02, "Horizontally"		},
//	{0x13, 0x01, 0x03, 0x01, "Vertically"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x18, 0x08, "Easy"			},
	{0x13, 0x01, 0x18, 0x18, "Normal"		},
	{0x13, 0x01, 0x18, 0x10, "Hard"			},
	{0x13, 0x01, 0x18, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x13, 0x01, 0xe0, 0x00, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0x80, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0x40, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0xc0, "3 Coins 2 Credits"	},
	{0x13, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xe0, 0x20, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0xe0, 0x60, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"	},
};

STDDIPINFO(Stagger1)

static struct BurnDIPInfo GrdnstrmDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x12, 0x01, 0x01, 0x01, "Off"			},
	{0x12, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x02, 0x00, "Off"			},
	{0x12, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x12, 0x01, 0x04, 0x04, "Off"			},
	{0x12, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Bombs"		},
	{0x12, 0x01, 0x08, 0x08, "2"			},
	{0x12, 0x01, 0x08, 0x00, "3"			},
	
	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x12, 0x01, 0xc0, 0x00, "1"			},
	{0x12, 0x01, 0xc0, 0x80, "2"			},
	{0x12, 0x01, 0xc0, 0xc0, "3"			},
	{0x12, 0x01, 0xc0, 0x40, "5"			},

//	{0   , 0xfe, 0   ,    2, "Mirror Screen"	},
//	{0x13, 0x01, 0x01, 0x01, "Off"			},
//	{0x13, 0x01, 0x01, 0x00, "On"			},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
//	{0x13, 0x01, 0x02, 0x02, "Off"			},
//	{0x13, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x18, 0x08, "Easy"			},
	{0x13, 0x01, 0x18, 0x18, "Normal"		},
	{0x13, 0x01, 0x18, 0x10, "Hard"			},
	{0x13, 0x01, 0x18, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x13, 0x01, 0xe0, 0x00, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0x80, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0x40, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0xc0, "3 Coins 2 Credits"	},
	{0x13, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xe0, 0x20, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0xe0, 0x60, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"	},
};

STDDIPINFO(Grdnstrm)

static struct BurnDIPInfo GrdnstrkDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x12, 0x01, 0x01, 0x01, "Off"			},
	{0x12, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x02, 0x00, "Off"			},
	{0x12, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x12, 0x01, 0x04, 0x04, "Off"			},
	{0x12, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Bombs"		},
	{0x12, 0x01, 0x08, 0x08, "2"			},
	{0x12, 0x01, 0x08, 0x00, "3"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x12, 0x01, 0xc0, 0x00, "1"			},
	{0x12, 0x01, 0xc0, 0x80, "2"			},
	{0x12, 0x01, 0xc0, 0xc0, "3"			},
	{0x12, 0x01, 0xc0, 0x40, "5"			},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
//	{0x13, 0x01, 0x01, 0x01, "Off"			},
//	{0x13, 0x01, 0x01, 0x00, "On"			},

//	{0   , 0xfe, 0   ,    2, "Mirror Screen"	},
//	{0x13, 0x01, 0x02, 0x02, "Off"			},
//	{0x13, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x18, 0x08, "Easy"			},
	{0x13, 0x01, 0x18, 0x18, "Normal"		},
	{0x13, 0x01, 0x18, 0x10, "Hard"			},
	{0x13, 0x01, 0x18, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x13, 0x01, 0xe0, 0x00, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0x80, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0x40, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0xc0, "3 Coins 2 Credits"	},
	{0x13, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xe0, 0x20, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0xe0, 0x60, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"	},
};

STDDIPINFO(Grdnstrk)

static struct BurnDIPInfo PopspopsDIPList[]=
{
	{0x12, 0xff, 0xff, 0xfa, NULL			},
	{0x13, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x02, 0x00, "Off"			},
	{0x12, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x18, 0x10, "Easy"			},
	{0x13, 0x01, 0x18, 0x18, "Normal"		},
	{0x13, 0x01, 0x18, 0x08, "Hard"			},
	{0x13, 0x01, 0x18, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x13, 0x01, 0xe0, 0x00, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0x20, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0x40, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0x60, "3 Coins 2 Credits"	},
	{0x13, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xe0, 0x80, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0xe0, 0xc0, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"	},
};

STDDIPINFO(Popspops)

static struct BurnDIPInfo Bubl2000DIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x12, 0x01, 0x0c, 0x08, "Easy"			},
	{0x12, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x12, 0x01, 0x0c, 0x04, "Hard"			},
	{0x12, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Free Credit"		},
	{0x12, 0x01, 0xc0, 0x80, "500k"			},
	{0x12, 0x01, 0xc0, 0xc0, "800k"			},
	{0x12, 0x01, 0xc0, 0x40, "1000k"		},
	{0x12, 0x01, 0xc0, 0x00, "1500k"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x02, 0x00, "Off"			},
	{0x13, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    7, "Coin B"		},
	{0x13, 0x01, 0x1c, 0x10, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x1c, 0x08, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x1c, 0x18, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x1c, 0x1c, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x1c, 0x0c, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x1c, 0x14, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0x1c, 0x04, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    7, "Coin A"		},
	{0x13, 0x01, 0xe0, 0x80, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0x40, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0xc0, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xe0, 0x60, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0xe0, 0x20, "1 Coin  4 Credits"	},
};

STDDIPINFO(Bubl2000)

static struct BurnDIPInfo MangchiDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "DSWS"			},
	{0x12, 0x01, 0x01, 0x01, "Off"			},
	{0x12, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x02, 0x00, "Off"			},
	{0x12, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    4, "Vs Rounds"		},
	{0x12, 0x01, 0x18, 0x18, "2"			},
	{0x12, 0x01, 0x18, 0x10, "3"			},
	{0x12, 0x01, 0x18, 0x08, "4"			},
	{0x12, 0x01, 0x18, 0x00, "5"			},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x12, 0x01, 0x40, 0x40, "Off"			},
	{0x12, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x18, 0x08, "Easy"			},
	{0x13, 0x01, 0x18, 0x18, "Normal"		},
	{0x13, 0x01, 0x18, 0x10, "Hard"			},
	{0x13, 0x01, 0x18, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x13, 0x01, 0xe0, 0x00, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0x20, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0x40, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0x60, "3 Coins 2 Credits"	},
	{0x13, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xe0, 0x80, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0xe0, 0xc0, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"	},
};

STDDIPINFO(Mangchi)

static struct BurnDIPInfo FirehawkDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x12, 0x01, 0x01, 0x01, "Off"			},
	{0x12, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    6, "Difficulty"		},
	{0x12, 0x01, 0x0e, 0x06, "Very Easy"		},
	{0x12, 0x01, 0x0e, 0x08, "Easy"			},
	{0x12, 0x01, 0x0e, 0x0e, "Normal"		},
	{0x12, 0x01, 0x0e, 0x02, "Hard"			},
	{0x12, 0x01, 0x0e, 0x04, "Hardest"		},
	{0x12, 0x01, 0x0e, 0x0c, "Very Hard"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x10, 0x00, "Off"			},
	{0x12, 0x01, 0x10, 0x10, "On"			},

	{0   , 0xfe, 0   ,    2, "Number of Bombs"	},
	{0x12, 0x01, 0x20, 0x20, "2"			},
	{0x12, 0x01, 0x20, 0x00, "3"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x12, 0x01, 0xc0, 0x00, "1"			},
	{0x12, 0x01, 0xc0, 0x80, "2"			},
	{0x12, 0x01, 0xc0, 0xc0, "3"			},
	{0x12, 0x01, 0xc0, 0x40, "4"			},

	{0   , 0xfe, 0   ,    2, "Region"		},
	{0x13, 0x01, 0x02, 0x02, "English"		},
	{0x13, 0x01, 0x02, 0x00, "China"		},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x13, 0x01, 0x04, 0x04, "Off"			},
	{0x13, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Continue Coins"	},
	{0x13, 0x01, 0x18, 0x18, "1 Coin"		},
	{0x13, 0x01, 0x18, 0x08, "2 Coins"		},
	{0x13, 0x01, 0x18, 0x10, "3 Coins"		},
	{0x13, 0x01, 0x18, 0x00, "4 Coins"		},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x13, 0x01, 0xe0, 0x00, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0x80, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0x40, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0xc0, "3 Coins 2 Credits"	},
	{0x13, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xe0, 0x20, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0xe0, 0x60, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"	},
};

STDDIPINFO(Firehawk)

static struct BurnDIPInfo Spec2kDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x12, 0x01, 0x01, 0x01, "Off"			},
	{0x12, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x02, 0x00, "Off"			},
	{0x12, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x12, 0x01, 0x04, 0x04, "Off"			},
	{0x12, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Number of Bombs"	},
	{0x12, 0x01, 0x08, 0x08, "2"			},
	{0x12, 0x01, 0x08, 0x00, "3"			},

	{0   , 0xfe, 0   ,    2, "Copyright Notice"	},
	{0x12, 0x01, 0x10, 0x00, "Off"			},
	{0x12, 0x01, 0x10, 0x10, "On"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x12, 0x01, 0xc0, 0x00, "1"			},
	{0x12, 0x01, 0xc0, 0x80, "2"			},
	{0x12, 0x01, 0xc0, 0xc0, "3"			},
	{0x12, 0x01, 0xc0, 0x40, "5"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x18, 0x08, "Easy"			},
	{0x13, 0x01, 0x18, 0x18, "Normal"		},
	{0x13, 0x01, 0x18, 0x10, "Hard"			},
	{0x13, 0x01, 0x18, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x13, 0x01, 0xe0, 0x00, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0x80, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0x40, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0xc0, "3 Coins 2 Credits"	},
	{0x13, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xe0, 0x20, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0xe0, 0x60, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"	},
};

STDDIPINFO(Spec2k)

static struct BurnDIPInfo TwinactnDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x02, 0x00, "Off"			},
	{0x12, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x12, 0x01, 0x1c, 0x10, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0x1c, 0x08, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x1c, 0x18, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x1c, 0x1c, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x1c, 0x0c, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x1c, 0x14, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x1c, 0x04, "1 Coin  4 Credits"	},
	{0x12, 0x01, 0x1c, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x12, 0x01, 0xe0, 0x80, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0xe0, 0x40, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0xe0, 0xc0, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0xe0, 0x60, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0xe0, 0x20, "1 Coin  4 Credits"	},
	{0x12, 0x01, 0xe0, 0x00, "Free Play"		},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
//	{0x13, 0x01, 0x01, 0x01, "Off"			},
//	{0x13, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x0c, 0x0c, "Easy"			},
	{0x13, 0x01, 0x0c, 0x04, "Normal"		},
	{0x13, 0x01, 0x0c, 0x08, "Hard"			},
	{0x13, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0xc0, 0x40, "2"			},
	{0x13, 0x01, 0xc0, 0xc0, "3"			},
	{0x13, 0x01, 0xc0, 0x80, "4"			},
	{0x13, 0x01, 0xc0, 0x00, "5"			},
};

STDDIPINFO(Twinactn)

static struct BurnDIPInfo BjtwinDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0xff, NULL			},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
//	{0x12, 0x01, 0x01, 0x01, "Off"			},
//	{0x12, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    8, "Starting level"	},
	{0x12, 0x01, 0x0e, 0x08, "Germany"		},
	{0x12, 0x01, 0x0e, 0x04, "Thailand"		},
	{0x12, 0x01, 0x0e, 0x0c, "Nevada"		},
	{0x12, 0x01, 0x0e, 0x0e, "Japan"		},
	{0x12, 0x01, 0x0e, 0x06, "Korea"		},
	{0x12, 0x01, 0x0e, 0x0a, "England"		},
	{0x12, 0x01, 0x0e, 0x02, "Hong Kong"		},
	{0x12, 0x01, 0x0e, 0x00, "China"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x12, 0x01, 0x30, 0x20, "Easy"			},
	{0x12, 0x01, 0x30, 0x30, "Normal"		},
	{0x12, 0x01, 0x30, 0x10, "Hard"			},
	{0x12, 0x01, 0x30, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x12, 0x01, 0xc0, 0x00, "1"			},
	{0x12, 0x01, 0xc0, 0x40, "2"			},
	{0x12, 0x01, 0xc0, 0xc0, "3"			},
	{0x12, 0x01, 0xc0, 0x80, "4"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x02, 0x00, "Off"			},
	{0x13, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x13, 0x01, 0x1c, 0x10, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x1c, 0x08, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x1c, 0x18, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x1c, 0x1c, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x1c, 0x0c, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x1c, 0x14, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0x1c, 0x04, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0x1c, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x13, 0x01, 0xe0, 0x80, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0x40, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0xc0, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xe0, 0x60, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0xe0, 0x20, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0xe0, 0x00, "Free Play"		},
};

STDDIPINFO(Bjtwin)

static struct BurnDIPInfo SabotenbDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0xff, NULL			},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
//	{0x12, 0x01, 0x01, 0x01, "Off"			},
//	{0x12, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Language"		},
	{0x12, 0x01, 0x02, 0x02, "Japanese"		},
	{0x12, 0x01, 0x02, 0x00, "English"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x12, 0x01, 0x0c, 0x08, "Easy"			},
	{0x12, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x12, 0x01, 0x0c, 0x04, "Hard"			},
	{0x12, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x12, 0x01, 0xc0, 0x00, "1"			},
	{0x12, 0x01, 0xc0, 0x40, "2"			},
	{0x12, 0x01, 0xc0, 0xc0, "3"			},
	{0x12, 0x01, 0xc0, 0x80, "4"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x02, 0x00, "Off"			},
	{0x13, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x13, 0x01, 0x1c, 0x10, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x1c, 0x08, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x1c, 0x18, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x1c, 0x1c, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x1c, 0x0c, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x1c, 0x14, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0x1c, 0x04, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0x1c, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x13, 0x01, 0xe0, 0x80, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0x40, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0xc0, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xe0, 0x60, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0xe0, 0x20, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0xe0, 0x00, "Free Play"		},
};

STDDIPINFO(Sabotenb)

static struct BurnDIPInfo NouryokuDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    4, "Life Decrease Speed"	},
	{0x12, 0x01, 0x03, 0x02, "Slow"			},
	{0x12, 0x01, 0x03, 0x03, "Normal"		},
	{0x12, 0x01, 0x03, 0x01, "Fast"			},
	{0x12, 0x01, 0x03, 0x00, "Very Fast"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x12, 0x01, 0x0c, 0x08, "Easy"			},
	{0x12, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x12, 0x01, 0x0c, 0x04, "Hard"			},
	{0x12, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x12, 0x01, 0x10, 0x10, "Off"			},
	{0x12, 0x01, 0x10, 0x00, "On"			},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x12, 0x01, 0xe0, 0x20, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0xe0, 0xa0, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0xe0, 0x60, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0xe0, 0x00, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0xe0, 0xc0, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0xe0, 0x40, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0xe0, 0x80, "1 Coin  4 Credits"	},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
//	{0x13, 0x01, 0x20, 0x20, "Off"			},
//	{0x13, 0x01, 0x20, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x40, 0x00, "Off"			},
	{0x13, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x13, 0x01, 0x80, 0x80, "Off"			},
	{0x13, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Nouryoku)

static struct BurnDIPInfo MustangDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x02, 0x00, "Off"			},
	{0x12, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x12, 0x01, 0x1c, 0x10, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0x1c, 0x08, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x1c, 0x18, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x1c, 0x1c, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x1c, 0x0c, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x1c, 0x14, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x1c, 0x04, "1 Coin  4 Credits"	},
	{0x12, 0x01, 0x1c, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x12, 0x01, 0xe0, 0x80, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0xe0, 0x40, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0xe0, 0xc0, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0xe0, 0x60, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0xe0, 0x20, "1 Coin  4 Credits"	},
	{0x12, 0x01, 0xe0, 0x00, "Free Play"		},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
//	{0x13, 0x01, 0x01, 0x01, "Off"			},
//	{0x13, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x0c, 0x04, "Easy"			},
	{0x13, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x13, 0x01, 0x0c, 0x08, "Hard"			},
	{0x13, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0xc0, 0x40, "2"			},
	{0x13, 0x01, 0xc0, 0xc0, "3"			},
	{0x13, 0x01, 0xc0, 0x80, "4"			},
	{0x13, 0x01, 0xc0, 0x00, "5"			},
};

STDDIPINFO(Mustang)

static struct BurnDIPInfo TdragonbDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x12, 0x01, 0x03, 0x00, "1"			},
	{0x12, 0x01, 0x03, 0x02, "2"			},
	{0x12, 0x01, 0x03, 0x03, "3"			},
	{0x12, 0x01, 0x03, 0x01, "4"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x12, 0x01, 0x30, 0x20, "Easy"			},
	{0x12, 0x01, 0x30, 0x30, "Normal"		},
	{0x12, 0x01, 0x30, 0x10, "Hard"			},
	{0x12, 0x01, 0x30, 0x00, "Hardest"		},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
//	{0x12, 0x01, 0x80, 0x80, "Off"			},
//	{0x12, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x13, 0x01, 0x07, 0x04, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x07, 0x05, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x07, 0x06, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x07, 0x03, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x07, 0x02, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0x07, 0x01, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0x07, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x13, 0x01, 0x38, 0x20, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x38, 0x28, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x38, 0x30, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x38, 0x18, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x38, 0x10, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0x38, 0x08, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0x38, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x40, 0x00, "Off"			},
	{0x13, 0x01, 0x40, 0x40, "On"			},
};

STDDIPINFO(Tdragonb)

static struct BurnDIPInfo TdragonDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0xff, NULL			},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
//	{0x12, 0x01, 0x01, 0x01, "Off"			},
//	{0x12, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x12, 0x01, 0x0c, 0x04, "Easy"			},
	{0x12, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x12, 0x01, 0x0c, 0x08, "Hard"			},
	{0x12, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x12, 0x01, 0xc0, 0x00, "1"			},
	{0x12, 0x01, 0xc0, 0x40, "2"			},
	{0x12, 0x01, 0xc0, 0xc0, "3"			},
	{0x12, 0x01, 0xc0, 0x80, "4"			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x13, 0x01, 0x07, 0x04, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x07, 0x05, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x07, 0x06, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x07, 0x03, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x07, 0x02, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0x07, 0x01, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0x07, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x13, 0x01, 0x38, 0x20, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x38, 0x28, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x38, 0x30, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x38, 0x18, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x38, 0x10, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0x38, 0x08, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0x38, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x40, 0x00, "Off"			},
	{0x13, 0x01, 0x40, 0x40, "On"			},
};

STDDIPINFO(Tdragon)

static struct BurnDIPInfo AcrobatmDIPList[]=
{
	{0x13, 0xff, 0xff, 0xfe, NULL			},
	{0x14, 0xff, 0xff, 0xf7, NULL			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x01, 0x01, "Off"			},
	{0x13, 0x01, 0x01, 0x00, "On"			},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
//	{0x13, 0x01, 0x02, 0x02, "Off"			},
//	{0x13, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x13, 0x01, 0x1c, 0x00, "5 Coins 1 Credits"	},
	{0x13, 0x01, 0x1c, 0x10, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x1c, 0x08, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x1c, 0x18, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x1c, 0x1c, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x1c, 0x0c, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x1c, 0x14, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0x1c, 0x04, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x13, 0x01, 0xe0, 0x00, "5 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0x80, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0x40, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0xc0, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xe0, 0x60, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0xe0, 0x20, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x01, 0x01, "Off"			},
	{0x14, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x14, 0x01, 0x06, 0x02, "50k and 100k"		},
	{0x14, 0x01, 0x06, 0x06, "100k and 100k"	},
	{0x14, 0x01, 0x06, 0x04, "100k and 200k"	},
	{0x14, 0x01, 0x06, 0x00, "None"			},

	{0   , 0xfe, 0   ,    2, "Language"		},
	{0x14, 0x01, 0x08, 0x00, "English"		},
	{0x14, 0x01, 0x08, 0x08, "Japanese"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x30, 0x00, "Hardest"		},
	{0x14, 0x01, 0x30, 0x10, "Easy"			},
	{0x14, 0x01, 0x30, 0x20, "Hard"			},
	{0x14, 0x01, 0x30, 0x30, "Normal"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0xc0, 0x40, "2"			},
	{0x14, 0x01, 0xc0, 0xc0, "3"			},
	{0x14, 0x01, 0xc0, 0x80, "4"			},
	{0x14, 0x01, 0xc0, 0x00, "5"			},
};

STDDIPINFO(Acrobatm)

static struct BurnDIPInfo GunnailDIPList[]=
{
	{0x12, 0xff, 0xff, 0xfd, NULL			},
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0x00, NULL			},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
//	{0x12, 0x01, 0x01, 0x01, "Off"			},
//	{0x12, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Language"		},
	{0x12, 0x01, 0x02, 0x02, "Japanese"		},
	{0x12, 0x01, 0x02, 0x00, "English"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x12, 0x01, 0x0c, 0x08, "Easy"			},
	{0x12, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x12, 0x01, 0x0c, 0x04, "Hard"			},
	{0x12, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x02, 0x00, "Off"			},
	{0x13, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x13, 0x01, 0x1c, 0x10, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x1c, 0x08, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x1c, 0x18, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x1c, 0x1c, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x1c, 0x0c, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x1c, 0x14, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0x1c, 0x04, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0x1c, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x13, 0x01, 0xe0, 0x80, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0x40, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0xc0, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xe0, 0x60, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0xe0, 0x20, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0xe0, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "trap15's sheild-warning mod" },
	{0x14, 0x01, 0x08, 0x08, "On"			},
	{0x14, 0x01, 0x08, 0x00, "Off"			},
};

STDDIPINFO(Gunnail)

static struct BurnDIPInfo DolmenDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x12, 0x01, 0x0c, 0x08, "Easy"			},
	{0x12, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x12, 0x01, 0x0c, 0x04, "Hard"			},
	{0x12, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Free Credit"		},
	{0x12, 0x01, 0xc0, 0x80, "500k"			},
	{0x12, 0x01, 0xc0, 0xc0, "800k"			},
	{0x12, 0x01, 0xc0, 0x40, "1000k"		},
	{0x12, 0x01, 0xc0, 0x00, "1500k"		},
	
	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x02, 0x00, "Off"			},
	{0x13, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    7, "Coin B"		},
	{0x13, 0x01, 0x1c, 0x10, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x1c, 0x08, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x1c, 0x18, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x1c, 0x1c, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x1c, 0x0c, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x1c, 0x14, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0x1c, 0x04, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    7, "Coin A"		},
	{0x13, 0x01, 0xe0, 0x80, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0x40, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0xc0, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xe0, 0x60, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0xe0, 0x20, "1 Coin  4 Credits"	},
};

STDDIPINFO(Dolmen)

//------------------------------------------------------------------------------------------------------------

#define STRANGE_RAM_WRITE_BYTE(value)				\
	if ((address & 0xffff0000) == value) {			\
		Drv68KRAM[(address & 0xfffe) | 0] = data;	\
		Drv68KRAM[(address & 0xfffe) | 1] = data;	\
		return;						\
	}							\

// MAKE SHORT ^^

#define STRANGE_RAM_WRITE_WORD(value)						\
	if ((address & 0xffff0000) == value) {					\
		*((UINT16*)(Drv68KRAM + (address & 0xfffe))) = BURN_ENDIAN_SWAP_INT16(data);	\
		return;								\
	}									\

#define PROT_JSR(_offs_,_protvalue_,_pc_) \
	if(nmk16_mainram[(_offs_)/2] == BURN_ENDIAN_SWAP_INT16(_protvalue_)) \
	{ \
		nmk16_mainram[(_offs_)/2] = 0xffff;  /*(MCU job done)*/ \
		nmk16_mainram[(_offs_+2-0x10)/2] = BURN_ENDIAN_SWAP_INT16(0x4ef9);/*JMP*/\
		nmk16_mainram[(_offs_+4-0x10)/2] = 0x0000;/*HI-DWORD*/\
		nmk16_mainram[(_offs_+6-0x10)/2] = BURN_ENDIAN_SWAP_INT16(_pc_);  /*LO-DWORD*/\
	} \

#define PROT_INPUT(_offs_,_protvalue_,_protinput_,_input_) \
	if(nmk16_mainram[_offs_] == BURN_ENDIAN_SWAP_INT16(_protvalue_)) \
	{\
		nmk16_mainram[_protinput_] = BURN_ENDIAN_SWAP_INT16((_input_ & 0xffff0000)>>16);\
		nmk16_mainram[_protinput_+1] = BURN_ENDIAN_SWAP_INT16(_input_ & 0x0000ffff);\
	}

//------------------------------------------------------------------------------------------------------------
// MCU simulation stuff

static INT32 prot_count = 0;

static UINT8 tharrier_mcu_r()
{
	UINT16 *nmk16_mainram = (UINT16*)Drv68KRAM;

	static const UINT8 to_main[15] =
	{
		0x82,0xc7,0x00,0x2c,0x6c,0x00,0x9f,0xc7,0x00,0x29,0x69,0x00,0x8b,0xc7,0x00
	};

	INT32 res;

	     if (SekGetPC(-1)==0x08aa) res = (BURN_ENDIAN_SWAP_INT16(nmk16_mainram[0x9064/2]))|0x20;
	else if (SekGetPC(-1)==0x08ce) res = (BURN_ENDIAN_SWAP_INT16(nmk16_mainram[0x9064/2]))|0x60;
	else if (SekGetPC(-1)==0x0332) res = (BURN_ENDIAN_SWAP_INT16(nmk16_mainram[0x90f6/2]))|0x00;
	else if (SekGetPC(-1)==0x64f4) res = (BURN_ENDIAN_SWAP_INT16(nmk16_mainram[0x90f6/2]))|0x00;
	else
	{
		res = to_main[prot_count++];
		if (prot_count > 14)
			prot_count = 0;
	}
	return res;
}

static void HachaRAMProt(INT32 offset)
{
	UINT16 *nmk16_mainram = (UINT16*)Drv68KRAM;

	switch(offset)
	{
		case 0xe058/2: PROT_INPUT(0xe058/2,0xc71f,0xe000/2,0x00080000); break;
		case 0xe182/2: PROT_INPUT(0xe182/2,0x865d,0xe004/2,0x00080002); break;
		case 0xe51e/2: PROT_INPUT(0xe51e/2,0x0f82,0xe008/2,0x00080008); break;
		case 0xe6b4/2: PROT_INPUT(0xe6b4/2,0x79be,0xe00c/2,0x0008000a); break;
		case 0xe10e/2: PROT_JSR(0xe10e,0x8007,0x870a); //870a not 9d66
			       PROT_JSR(0xe10e,0x8000,0xd9c6); break;
		case 0xe11e/2: PROT_JSR(0xe11e,0x8038,0x972a); // 972a
					  PROT_JSR(0xe11e,0x8031,0xd1f8); break;
		case 0xe12e/2: PROT_JSR(0xe12e,0x8019,0x9642); // OK-9642
					  PROT_JSR(0xe12e,0x8022,0xda06); break;
		case 0xe13e/2: PROT_JSR(0xe13e,0x802a,0x9d66); // 9d66 not 9400 - OK
					  PROT_JSR(0xe13e,0x8013,0x81aa); break;
		case 0xe14e/2: PROT_JSR(0xe14e,0x800b,0xb3f2); // b3f2 - OK
					  PROT_JSR(0xe14e,0x8004,0x8994); break;
		case 0xe15e/2: PROT_JSR(0xe15e,0x803c,0xb59e); // b59e - OK
					  PROT_JSR(0xe15e,0x8035,0x8d0c); break;
		case 0xe16e/2: PROT_JSR(0xe16e,0x801d,0x9ac2); // 9ac2 - OK
				 	  PROT_JSR(0xe16e,0x8026,0x8c36); break;
		case 0xe17e/2: PROT_JSR(0xe17e,0x802e,0xc366); // c366 - OK
					  PROT_JSR(0xe17e,0x8017,0x870a); break;
		case 0xe18e/2: PROT_JSR(0xe18e,0x8004,0xd620);       		 // unused
					  PROT_JSR(0xe18e,0x8008,0x972a); break; // unused
		case 0xe19e/2: PROT_JSR(0xe19e,0x8030,0xd9c6); // OK-d9c6
					  PROT_JSR(0xe19e,0x8039,0x9642); break;
		case 0xe1ae/2: PROT_JSR(0xe1ae,0x8011,0xd1f8); // d1f8 not c67e
					  PROT_JSR(0xe1ae,0x802a,0x9d66); break;
		case 0xe1be/2: PROT_JSR(0xe1be,0x8022,0xda06); // da06
					  PROT_JSR(0xe1be,0x801b,0xb3f2); break;
		case 0xe1ce/2: PROT_JSR(0xe1ce,0x8003,0x81aa); // 81aa
					  PROT_JSR(0xe1ce,0x800c,0xb59e); break;
		case 0xe1de/2: PROT_JSR(0xe1de,0x8034,0x8994); // 8994 - OK
					  PROT_JSR(0xe1de,0x803d,0x9ac2); break;
		case 0xe1ee/2: PROT_JSR(0xe1ee,0x8015,0x8d0c); // 8d0c not 82f6
					  PROT_JSR(0xe1ee,0x802e,0xc366); break;
		case 0xe1fe/2: PROT_JSR(0xe1fe,0x8026,0x8c36); // 8c36
					  PROT_JSR(0xe1fe,0x8016,0xd620); break;  // unused
		case 0xef00/2:
			if(nmk16_mainram[0xef00/2] == BURN_ENDIAN_SWAP_INT16(0x60fe))
			{
				nmk16_mainram[0xef00/2] = 0x0000; // this is the coin counter
				nmk16_mainram[0xef02/2] = 0x0000;
				nmk16_mainram[0xef04/2] = BURN_ENDIAN_SWAP_INT16(0x4ef9);
				nmk16_mainram[0xef06/2] = 0x0000;
				nmk16_mainram[0xef08/2] = BURN_ENDIAN_SWAP_INT16(0x7dc2);
			}
		break;
	}
}

static void tdragon_mainram_w(INT32 offset)
{
	UINT16 *nmk16_mainram = (UINT16*)Drv68KRAM;

	switch(offset)
	{
		case 0xe066/2: PROT_INPUT(0xe066/2,0xe23e,0xe000/2,0x000c0000); break;
		case 0xe144/2: PROT_INPUT(0xe144/2,0xf54d,0xe004/2,0x000c0002); break;
		case 0xe60e/2: PROT_INPUT(0xe60e/2,0x067c,0xe008/2,0x000c0008); break;
		case 0xe714/2: PROT_INPUT(0xe714/2,0x198b,0xe00c/2,0x000c000a); break;
		case 0xe70e/2: PROT_JSR(0xe70e,0x8007,0x9e22);
					  PROT_JSR(0xe70e,0x8000,0xd518); break;
		case 0xe71e/2: PROT_JSR(0xe71e,0x8038,0xaa0a);
					  PROT_JSR(0xe71e,0x8031,0x8e7c); break;
		case 0xe72e/2: PROT_JSR(0xe72e,0x8019,0xac48);
					  PROT_JSR(0xe72e,0x8022,0xd558); break;
		case 0xe73e/2: PROT_JSR(0xe73e,0x802a,0xb110);
					  PROT_JSR(0xe73e,0x8013,0x96da); break;
		case 0xe74e/2: PROT_JSR(0xe74e,0x800b,0xb9b2);
					  PROT_JSR(0xe74e,0x8004,0xa062); break;
		case 0xe75e/2: PROT_JSR(0xe75e,0x803c,0xbb4c);
					  PROT_JSR(0xe75e,0x8035,0xa154); break;
		case 0xe76e/2: PROT_JSR(0xe76e,0x801d,0xafa6);
				 	  PROT_JSR(0xe76e,0x8026,0xa57a); break;
		case 0xe77e/2: PROT_JSR(0xe77e,0x802e,0xc6a4);
					  PROT_JSR(0xe77e,0x8017,0x9e22); break;
		case 0xe78e/2: PROT_JSR(0xe78e,0x8004,0xaa0a);
					  PROT_JSR(0xe78e,0x8008,0xaa0a); break;
		case 0xe79e/2: PROT_JSR(0xe79e,0x8030,0xd518);
					  PROT_JSR(0xe79e,0x8039,0xac48); break;
		case 0xe7ae/2: PROT_JSR(0xe7ae,0x8011,0x8e7c);
					  PROT_JSR(0xe7ae,0x802a,0xb110); break;
		case 0xe7be/2: PROT_JSR(0xe7be,0x8022,0xd558);
					  PROT_JSR(0xe7be,0x801b,0xb9b2); break;
		case 0xe7ce/2: PROT_JSR(0xe7ce,0x8003,0x96da);
					  PROT_JSR(0xe7ce,0x800c,0xbb4c); break;
		case 0xe7de/2: PROT_JSR(0xe7de,0x8034,0xa062);
					  PROT_JSR(0xe7de,0x803d,0xafa6); break;
		case 0xe7ee/2: PROT_JSR(0xe7ee,0x8015,0xa154);
					  PROT_JSR(0xe7ee,0x802e,0xc6a4); break;
		case 0xe7fe/2: PROT_JSR(0xe7fe,0x8026,0xa57a);
					  PROT_JSR(0xe7fe,0x8016,0xa57a); break;
		case 0xef00/2:
			if(nmk16_mainram[0xef00/2] == BURN_ENDIAN_SWAP_INT16(0x60fe))
			{
				nmk16_mainram[0xef00/2] = 0x0000; // this is the coin counter
				nmk16_mainram[0xef02/2] = 0x0000;
				nmk16_mainram[0xef04/2] = BURN_ENDIAN_SWAP_INT16(0x4ef9);
				nmk16_mainram[0xef06/2] = 0x0000;
				nmk16_mainram[0xef08/2] = BURN_ENDIAN_SWAP_INT16(0x92f4);
			}
		break;
	}
}

static void mcu_run(UINT8 dsw_setting)
{
	static UINT8 input_pressed;
	static UINT16 coin_input;
	UINT8 dsw[2];
	static UINT8 start_helper = 0;
	static UINT8 coin_count[2],coin_count_frac[2];
	static UINT8 i;

	UINT16 *nmk16_mainram = (UINT16*)Drv68KRAM;

	if(start_helper & 1 && BURN_ENDIAN_SWAP_INT16(nmk16_mainram[0x9000/2]) & 0x0200) // start 1
	{
		nmk16_mainram[0xef00/2]--;
		start_helper = start_helper & 2;
	}
	if(start_helper & 2 && BURN_ENDIAN_SWAP_INT16(nmk16_mainram[0x9000/2]) & 0x0100) // start 2
	{
		nmk16_mainram[0xef00/2]--;
		start_helper = start_helper & 1;
	}

	if(dsw_setting) // Thunder Dragon
	{
		dsw[0] = (DrvDips[1] & 0x7);
		dsw[1] = (DrvDips[1] & 0x38) >> 3;
		for(i=0;i<2;i++)
		{
			switch(dsw[i] & 7)
			{
				case 0: nmk16_mainram[0x9000/2]|=BURN_ENDIAN_SWAP_INT16(0x4000); break; // free play
				case 1: coin_count_frac[i] = 1; coin_count[i] = 4; break;
				case 2: coin_count_frac[i] = 1; coin_count[i] = 3; break;
				case 3: coin_count_frac[i] = 1; coin_count[i] = 2; break;
				case 4: coin_count_frac[i] = 4; coin_count[i] = 1; break;
				case 5: coin_count_frac[i] = 3; coin_count[i] = 1; break;
				case 6: coin_count_frac[i] = 2; coin_count[i] = 1; break;
				case 7: coin_count_frac[i] = 1; coin_count[i] = 1; break;
			}
		}
	}
	else // Hacha Mecha Fighter
	{
		dsw[0] = (DrvDips[1] & 0x07) >> 0;
		dsw[1] = (DrvDips[1] & 0x38) >> 3;
		for(i=0;i<2;i++)
		{
			switch(dsw[i] & 7)
			{
				case 0: nmk16_mainram[0x9000/2]|=BURN_ENDIAN_SWAP_INT16(0x4000); break; // free play
				case 1: coin_count_frac[i] = 4; coin_count[i] = 1; break;
				case 2: coin_count_frac[i] = 3; coin_count[i] = 1; break;
				case 3: coin_count_frac[i] = 2; coin_count[i] = 1; break;
				case 4: coin_count_frac[i] = 1; coin_count[i] = 4; break;
				case 5: coin_count_frac[i] = 1; coin_count[i] = 3; break;
				case 6: coin_count_frac[i] = 1; coin_count[i] = 2; break;
				case 7: coin_count_frac[i] = 1; coin_count[i] = 1; break;
			}
		}
	}

	// read the coin port
	coin_input = (~(DrvInputs[0]));

	if(coin_input & 0x01) // coin 1
	{
		if((input_pressed & 0x01) == 0)
		{
			if(coin_count_frac[0] != 1)
			{
				nmk16_mainram[0xef02/2]+=coin_count[0];
				if(coin_count_frac[0] == nmk16_mainram[0xef02/2])
				{
					nmk16_mainram[0xef00/2]+=coin_count[0];
					nmk16_mainram[0xef02/2] = 0;
				}
			}
			else
				nmk16_mainram[0xef00/2]+=coin_count[0];
		}
		input_pressed = (input_pressed & 0xfe) | 1;
	}
	else
		input_pressed = (input_pressed & 0xfe);

	if(coin_input & 0x02) // coin 2
	{
		if((input_pressed & 0x02) == 0)
		{
			if(coin_count_frac[1] != 1)
			{
				nmk16_mainram[0xef02/2]+=coin_count[1];
				if(coin_count_frac[1] == nmk16_mainram[0xef02/2])
				{
					nmk16_mainram[0xef00/2]+=coin_count[1];
					nmk16_mainram[0xef02/2] = 0;
				}
			}
			else
				nmk16_mainram[0xef00/2]+=coin_count[1];
		}
		input_pressed = (input_pressed & 0xfd) | 2;
	}
	else
		input_pressed = (input_pressed & 0xfd);

	if(coin_input & 0x04) // service 1
	{
		if((input_pressed & 0x04) == 0)
			nmk16_mainram[0xef00/2]++;
		input_pressed = (input_pressed & 0xfb) | 4;
	}
	else
		input_pressed = (input_pressed & 0xfb);

	if(nmk16_mainram[0xef00/2] > 0 && nmk16_mainram[0x9000/2] & 0x8000) //enable start button
	{
		if(coin_input & 0x08) // start 1
		{
			if((input_pressed & 0x08) == 0 && (!(nmk16_mainram[0x9000/2] & 0x0200))) // start 1
				start_helper = 1;

			input_pressed = (input_pressed & 0xf7) | 8;
		}
		else
			input_pressed = (input_pressed & 0xf7);

		if(coin_input & 0x10) // start 2
		{
			if((input_pressed & 0x10) == 0 && (!(nmk16_mainram[0x9000/2] & 0x0100))) // start 2
				start_helper = (nmk16_mainram[0x9000/2] == 0x8000) ? (3) : (2);

			input_pressed = (input_pressed & 0xef) | 0x10;
		}
		else
			input_pressed = (input_pressed & 0xef);
	}
}

//-------------------------------------------------------------------------------------------------

void __fastcall tharrier_main_write_byte(UINT32 address, UINT8 data)
{
	STRANGE_RAM_WRITE_BYTE(0xf0000)
}
	
void __fastcall tharrier_main_write_word(UINT32 address, UINT16 data)
{
	STRANGE_RAM_WRITE_WORD(0xf0000)

	switch (address)
	{
		case 0x080010: // mcu write.. unused...
		return;

		case 0x08001e:
			*soundlatch = data;
		return;
	}
}

UINT8 __fastcall tharrier_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x080000:
			return DrvInputs[0] >> 8;

		case 0x080001:
			return DrvInputs[0];

		case 0x080002:
			return tharrier_mcu_r();

		case 0x080003:
			return DrvInputs[1];

		case 0x080004:
			return DrvDips[1];
	
		case 0x080005:
			return DrvDips[0];

		case 0x08000e:
		case 0x08000f:
			return *soundlatch2;

		case 0x080202:
			return DrvInputs[2] >> 8;

		case 0x080203:
			return DrvInputs[2];
	}

	return 0;
}

UINT16 __fastcall tharrier_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x080000:
			return DrvInputs[0];

		case 0x080002:
			return DrvInputs[1];

		case 0x080004:
			return (DrvDips[0] << 8) | DrvDips[1];

		case 0x08000e:
			return *soundlatch2;

		case 0x080202:
			return DrvInputs[2];
	}

	return 0;
}

void __fastcall manybloc_main_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x080014:
		case 0x080015:
	//		*flipscreen = data & 1;
		return;

		case 0x08001e:
		case 0x08001f:
			*soundlatch = data;
		return;
	}
}

void __fastcall manybloc_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x080014:
	//		*flipscreen = data & 1;
		return;

		case 0x08001e:
			*soundlatch = data;
		return;
	}
}

UINT8 __fastcall manybloc_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x080000:
			return DrvInputs[0] >> 8;

		case 0x080001:
			return DrvInputs[0];

		case 0x080002:
			return DrvInputs[1] >> 8;

		case 0x080003:
			return DrvInputs[1];

		case 0x080004:
			return DrvDips[0];

		case 0x080005:
			return DrvDips[1];

		case 0x08001e:
		case 0x08001f:
			return *soundlatch2;
	}

	return 0;
}

UINT16 __fastcall manybloc_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x080000:
			return DrvInputs[0];

		case 0x080002:
			return DrvInputs[1];

		case 0x080004:
			return (DrvDips[0] << 8) | DrvDips[1];

		case 0x08001e:
			return *soundlatch2;
	}

	return 0;
}

void __fastcall ssmissin_main_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x0c0014:
		case 0x0c0015:
	//		*flipscreen = data & 1;
		return;

		case 0x0c0018:
		case 0x0c0019:
			if (data != 0xff) {
				*tilebank = data;
			}
		return;

		case 0x0c001e:
		case 0x0c001f:
			*soundlatch = data;
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;
	}
}

void __fastcall ssmissin_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x0c0014:
	//		*flipscreen = data & 1;
		return;

		case 0x0c0018:
			if ((data & 0xff) != 0xff) {
				*tilebank = data;
			}
		return;

		case 0x0c001e:
			*soundlatch = data;
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;
	}
}

UINT8 __fastcall ssmissin_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x0c0000:
			return DrvInputs[0] >> 8;

		case 0x0c0001:
			return DrvInputs[0];

		case 0x0c0004:
			return DrvInputs[1] >> 8;

		case 0x0c0005:
			return DrvInputs[1];

		case 0x0c0006:
			return DrvDips[0];

		case 0x0c0007:
			return DrvDips[1];
	}

	return 0;
}

UINT16 __fastcall ssmissin_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x0c0000:
			return DrvInputs[0];

		case 0x0c0004:
			return DrvInputs[1];

		case 0x0c0006:
			return (DrvDips[0] << 8) | DrvDips[1];
	}

	return 0;
}

UINT8 __fastcall macross2_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x100000:
			return DrvInputs[0] >> 8;

		case 0x100001:
			return DrvInputs[0];

		case 0x100002:
			return DrvInputs[1] >> 8;

		case 0x100003:
			return DrvInputs[1];

		case 0x100008:
		case 0x100009:
			return DrvDips[0];

		case 0x10000a:
		case 0x10000b:
			return DrvDips[1];

		case 0x10000e:
		case 0x10000f:
			return *soundlatch2;
	}

	return 0;
}

UINT16 __fastcall macross2_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x100000:
			return DrvInputs[0];

		case 0x100002:
			return DrvInputs[1];

		case 0x100008:
			return (DrvDips[0] << 8) | DrvDips[0];

		case 0x10000a:
			return (DrvDips[1] << 8) | DrvDips[1];

		case 0x10000e:
			return *soundlatch2;
	}

	return 0;
}

void __fastcall macross2_main_write_byte(UINT32 address, UINT8 data)
{
	bprintf (0, _T("%x, %x wb\n"), address, data);

	switch (address)
	{
		case 0x100014:
		case 0x100015:
	//		*flipscreen = data & 1;
		return;
	}
}

void __fastcall macross2_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x100014:
	//		*flipscreen = data & 1;
		return;

		case 0x100016:
			if (data == 0) {
				if (macross2_sound_enable != 0) {
					ZetReset();
				}
			}
			macross2_sound_enable = data;
		return;

		case 0x100018:
			if ((data & 0xff) != 0xff) {
				*tilebank = data;
			}
		return;

		case 0x10001e:
			*soundlatch = data;
		return;
	}
}

void __fastcall afega_main_write_word(UINT32 address, UINT16 data)
{
	if (address & 0xfff00000) {
		SekWriteWord(address & 0xfffff, data);
		return;
	}

	STRANGE_RAM_WRITE_WORD(0xc0000)
	STRANGE_RAM_WRITE_WORD(0xf0000)

	switch (address)
	{
		case 0x080014:
	//		*flipscreen = data & 1;
		return;


		case 0x08001e:
			*soundlatch = data & 0xff;
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;

		case 0x08c000: { // for twin action twinactn background scroll
			switch (data & 0xff00) {
				case 0x0000:
					mustang_bg_xscroll = (mustang_bg_xscroll & 0x00ff) | ((data & 0x00ff)<<8);
					break;

				case 0x0100:
					mustang_bg_xscroll = (mustang_bg_xscroll & 0xff00) | (data & 0x00ff);
					break;
			}
			return;
		}
	}
}

void __fastcall afega_main_write_byte(UINT32 address, UINT8 data)
{
	if (address & 0xfff00000) {
		SekWriteByte(address & 0xfffff, data);
		return;
	}

	STRANGE_RAM_WRITE_BYTE(0xc0000)
	STRANGE_RAM_WRITE_BYTE(0xf0000)

	switch (address)
	{
		case 0x080014:
		case 0x080015:
	//		*flipscreen = data & 1;
		return;

		case 0x08001e:
		case 0x08001f:
			*soundlatch = data & 0xff;
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;
	}
}

UINT8 __fastcall afega_main_read_byte(UINT32 address)
{
	if (address & 0xfff00000) {
		return SekReadByte(address & 0xfffff);
	}

	switch (address)
	{
		case 0x080000:
			return DrvInputs[0] >> 8;

		case 0x080001:
			return DrvInputs[0];

		case 0x080002:
			return DrvInputs[1] >> 8;

		case 0x080003:
			return DrvInputs[1];

		case 0x080004:
			return DrvDips[0];

		case 0x080005:
			return DrvDips[1];

		case 0x080012:
		case 0x080013:
			return 0x01;
	}

	return 0;
}

UINT16 __fastcall afega_main_read_word(UINT32 address)
{
	if (address & 0xfff00000) {
		return SekReadWord(address & 0xfffff);
	}

	switch (address)
	{
		case 0x080000:
			return DrvInputs[0];

		case 0x080002:
			return DrvInputs[1];

		case 0x080004:
			return ((DrvDips[0] << 8) | DrvDips[1]);

		case 0x080012:
			return 0x0100;
	}

	return 0;
}

void __fastcall bjtwin_main_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x080014:
		case 0x080015:
	//		*flipscreen = data & 1;
		return;

		case 0x094000:
		case 0x094001:
			if (data != 0xff) {
				*tilebank = data;
			}
		return;
	}
}

void __fastcall bjtwin_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x080014:
	//		*flipscreen = data & 1;
		return;

		case 0x084000:
			MSM6295Command(0, data & 0xff);
		return;

		case 0x084010:
			MSM6295Command(1, data & 0xff);
		return;

		case 0x084020:
		case 0x084022:
		case 0x084024:
		case 0x084026:
		case 0x084028:
		case 0x08402a:
		case 0x08402c:
		case 0x08402e:
			NMK112_okibank_write((address >> 1) & 7, data);
		return;

		case 0x094000:
			if ((data & 0xff) != 0xff) {
				*tilebank = data;
			}
		return;
	}
}

UINT8 __fastcall bjtwin_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x080000:
			return DrvInputs[0] >> 8;

		case 0x080001:
			return DrvInputs[0];

		case 0x080002:
			return DrvInputs[1] >> 8;

		case 0x080003:
			return DrvInputs[1];

		case 0x080008:
		case 0x080009:
			return DrvDips[0];

		case 0x08000a:
		case 0x08000b:
			return DrvDips[1];

		case 0x084000:
		case 0x084001:
			return MSM6295ReadStatus(0);

		case 0x084010:
		case 0x084011:
			return MSM6295ReadStatus(1);
	}

	return 0;
}

UINT16 __fastcall bjtwin_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x080000:
			return DrvInputs[0];

		case 0x080002:
			return DrvInputs[1];

		case 0x080008:
			return 0xff00 | DrvDips[0];

		case 0x08000a:
			return 0xff00 | DrvDips[1];

		case 0x084000:
			return MSM6295ReadStatus(0);

		case 0x084010:
			return MSM6295ReadStatus(1);
	}

	return 0;
}

void __fastcall mustangb_main_write_word(UINT32 address, UINT16 data)
{
	STRANGE_RAM_WRITE_WORD(0xf0000)

	switch (address)
	{
		case 0x080014:
		case 0x0c0014:
	//		*flipscreen = data & 1;
		return;

		case 0x08001e:
		case 0x0c001e:
	//		bprintf (0, _T("%6.6x, %x,\n"), SekGetPC(-1), data);
			seibu_sound_mustb_write_word(0, data);
		return;
	}
}

void __fastcall mustangb_main_write_byte(UINT32 address, UINT8 data)
{
	STRANGE_RAM_WRITE_BYTE(0xf0000)

	switch (address)
	{
		case 0x080014:
		case 0x080015:
		case 0x0c0014:
		case 0x0c0015:
	//		*flipscreen = data & 1;
		return;

		case 0x08001e:
		case 0x08001f:
		case 0x0c001e:
		case 0x0c001f:
		//	bprintf (0, _T("%6.6x, %x,\n"), SekGetPC(-1), data);
			seibu_sound_mustb_write_word(0, data);
		return;
	}
}

UINT16 __fastcall mustangb_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x044022:
			return 0x0003;

		case 0x080000:
		case 0x0c0000:
			return DrvInputs[0];

		case 0x080002:
		case 0x0c0002:
			return DrvInputs[1];

		case 0x080004:
		case 0x0c0008:
			return (DrvDips[0] << 8) | DrvDips[1];

		case 0x0c000a:
			return 0xff00 | DrvDips[1];
	}

	return 0;
}

UINT8 __fastcall mustangb_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x044023: // tdragonb sprite fix
			return 0x03;

		case 0x080000:
		case 0x0c0000:
			return DrvInputs[0] >> 8;

		case 0x080001:
		case 0x0c0001:
			return DrvInputs[0];

		case 0x080002:
		case 0x0c0002:
			return DrvInputs[1] >> 8;

		case 0x080003:
		case 0x0c0003:
			return DrvInputs[1];

		case 0x080004:
		case 0x0c0008:
			return DrvDips[1];

		case 0x080005:
		case 0x0c0009:
			return DrvDips[0];

		case 0x0c000a:
		case 0x0c000b:
			return DrvDips[1];
	}

	return 0;
}

UINT8 __fastcall mustang_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x080000:
		case 0x080001:
			return DrvInputs[0] >> ((~address & 1) << 3);

		case 0x080002:
		case 0x080003:
			return DrvInputs[1] >> ((~address & 1) << 3);

		case 0x080004:
		case 0x080005:
			return DrvDips[address & 1];

		case 0x08000e:
		case 0x08000f:
			return NMK004Read();
	}

	return 0;
}

UINT16 __fastcall mustang_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x080000:
			return DrvInputs[0];

		case 0x080002:
			return DrvInputs[1];

		case 0x080004:
			return (DrvDips[0] << 8) | DrvDips[1];

		case 0x08000e:
			return NMK004Read();
	}

	return 0;
}

void __fastcall mustang_main_write_word(UINT32 address, UINT16 data)
{
	STRANGE_RAM_WRITE_WORD(0xf0000)

	switch (address)
	{
		case 0x080014:
	//		*flipscreen = data & 1;
		return;

		case 0x080016:
			NMK004NmiWrite(data);
		return;

		case 0x08001e:
			NMK004Write(0, data);
		return;

		case 0x08c000: {
			switch (data & 0xff00) {
				case 0x0000:
					mustang_bg_xscroll = (mustang_bg_xscroll & 0x00ff) | ((data & 0x00ff)<<8);
					break;

				case 0x0100:
					mustang_bg_xscroll = (mustang_bg_xscroll & 0xff00) | (data & 0x00ff);
					break;
			}
			return;
		}
	}
}

void __fastcall mustang_main_write_byte(UINT32 address, UINT8 data)
{
	STRANGE_RAM_WRITE_BYTE(0xf0000)
	switch (address)
	{
		case 0x080016:
		case 0x080017:
			NMK004NmiWrite(data);
		return;
		case 0x08001e:
		case 0x08001f:
			NMK004Write(0, data);
                        return;
        }
}

UINT8 __fastcall acrobatm_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x0c0000:
		case 0x0c0001:
			return DrvInputs[0] >> ((~address & 1) << 3);

		case 0x0c0002:
		case 0x0c0003:
			return DrvInputs[1] >> ((~address & 1) << 3);

		case 0x0c0008:
		case 0x0c0009:
			return DrvDips[0];

		case 0x0c000a:
		case 0x0c000b:
			return DrvDips[1];

		case 0x0c000e:
			return NMK004Read();
	}

	return 0;
}

UINT16 __fastcall acrobatm_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x0c0000:
			return DrvInputs[0];

		case 0x0c0002:
			return DrvInputs[1];

		case 0x0c0008:
			return DrvDips[0];

		case 0x0c000a:
			return DrvDips[1];

		case 0x0c000e:
			return NMK004Read();
	}

	return 0;
}

void __fastcall acrobatm_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x0c0014:
		case 0x0c0015:
	//		*flipscreen = data & 1;
		return;

		case 0x0c0016:
			NMK004NmiWrite(data);
		return;

		case 0x0c0018:
		case 0x0c0019:
			if ((data & 0xff) != 0xff) {
				*tilebank = data;
			}
		return;

		case 0x0c001e:
		case 0x0c001f:
			NMK004Write(0, data);
		return;
	}
}

void __fastcall acrobatm_main_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x0c0014:
		case 0x0c0015:
	//		*flipscreen = data & 1;
		return;

		case 0x0c0016:
		case 0x0c0017:
			NMK004NmiWrite(data);
		return;

		case 0x0c0018:
		case 0x0c0019:
			if (data != 0xff) {
				*tilebank = data;
			}
		return;

		case 0x0c001e:
		case 0x0c001f:
			NMK004Write(0, data);
		return;
	}
}

UINT8 __fastcall tdragon_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x0c0000:
		case 0x0c0001:
			return DrvInputs[0] >> ((~address & 1) << 3);

		case 0x0c0002:
		case 0x0c0003:
			return DrvInputs[1] >> ((~address & 1) << 3);

		case 0x0c0008:
		case 0x0c0009:
			return DrvDips[0];

		case 0x0c000a:
		case 0x0c000b:
			return DrvDips[1];

		case 0x0c000e:
		case 0x0c000f:
			return NMK004Read();
	}

	return 0;
}

UINT16 __fastcall tdragon_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x0c0000:
			return DrvInputs[0];

		case 0x0c0002:
			return DrvInputs[1];

		case 0x0c0008:
			return DrvDips[0];

		case 0x0c000a:
			return DrvDips[1];

		case 0x0c000e:
			return NMK004Read();
	}

	return 0;
}

void __fastcall tdragon_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xffff0000) == 0x0b0000) {
		*((UINT16*)(Drv68KRAM + (address & 0xfffe))) = BURN_ENDIAN_SWAP_INT16(data);
		tdragon_mainram_w((address >> 1) & 0x7fff);
		return;
	}

	switch (address)
	{
		case 0x0c0014:
	//		*flipscreen = data & 1;
		return;

		case 0x0c0016:
		case 0x0c0017:
			NMK004NmiWrite(data);
		return;

		case 0x0c0018:
			if ((data & 0xff) != 0xff) {
				*tilebank = data;
			}
		return;

		case 0x0c001e:
			NMK004Write(0, data);
		return;
	}
}

void __fastcall tdragon_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xffff0000) == 0x0b0000) {
		Drv68KRAM [(address & 0xffff) ^ 1] = data;
		tdragon_mainram_w((address >> 1) & 0x7fff);
		return;
	}

	switch (address)
	{
		case 0x0c0014:
		case 0x0c0015:
	//		*flipscreen = data & 1;
		return;

		case 0x0c0016:
		case 0x0c0017:
			NMK004NmiWrite(data);
		return;

		case 0x0c0018:
		case 0x0c0019:
			if (data != 0xff) {
				*tilebank = data;
			}
		return;

		case 0x0c001e:
		case 0x0c001f:
			NMK004Write(0, data);
		return;
	}
}


UINT8 __fastcall macross_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x080000:
		case 0x080001:
			return DrvInputs[0] >> ((~address & 1) << 3);

		case 0x080002:
		case 0x080003:
			return DrvInputs[1] >> ((~address & 1) << 3);

		case 0x080008:
		case 0x080009:
			return DrvDips[0];

		case 0x08000a:
		case 0x08000b:
			return DrvDips[1];

		case 0x08000e:
		case 0x08000f:
			return NMK004Read();
	}

	return 0;
}

UINT16 __fastcall macross_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x080000:
			return DrvInputs[0];

		case 0x080002:
			return DrvInputs[1];

		case 0x080008:
			return (DrvDips[0] << 8) | DrvDips[0];

		case 0x08000a:
			return (DrvDips[1] << 8) | DrvDips[1];

		case 0x08000e:
			return NMK004Read();
	}

	return 0;
}

void __fastcall macross_main_write_word(UINT32 address, UINT16 data)
{
	STRANGE_RAM_WRITE_WORD(0xf0000)

	switch (address)
	{
		case 0x080014:
	//		*flipscreen = data & 1;
		return;

		case 0x080016:
		//case 0x080017:
			NMK004NmiWrite(data);
		return;

		case 0x080018:
			if ((data & 0xff) != 0xff) {
				*tilebank = data;
			}
		return;

		case 0x08001e:
			NMK004Write(0, data);
		return;

		case 0x084000:
			if ((data & 0xff) != 0xff)
				*tilebank = data; // bioship
		return;
	}
}

void __fastcall macross_main_write_byte(UINT32 address, UINT8 data)
{
	STRANGE_RAM_WRITE_BYTE(0xf0000)

	switch (address)
	{
		case 0x080014:
		case 0x080015:
	//		*flipscreen = data & 1;
		return;

		case 0x080016:
		case 0x080017:
			NMK004NmiWrite(data);
		return;

		case 0x080018:
		case 0x080019:
			if ((data & 0xff) != 0xff) {
				*tilebank = data;
			}
		return;

		case 0x08001e:
		case 0x08001f:
			NMK004Write(0, data);
		return;

		case 0x084000:
		case 0x084001:
			if ((data & 0xff) != 0xff) {
				*tilebank = data; // bioship
			}
		return;
	}
}

void __fastcall vandykeb_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x080010:
			*((UINT16 *)(DrvScrollRAM + 0x06)) = data;
		return;

		case 0x080012:
			*((UINT16 *)(DrvScrollRAM + 0x04)) = data;
		return;

		case 0x080014:
	//		*flipscreen = data & 1;
		return;

		case 0x080018:
			if ((data & 0xff) != 0xff) {
				*tilebank = data;
			}
		return;

		case 0x08001a:
			*((UINT16 *)(DrvScrollRAM + 0x02)) = data;
		return;

		case 0x08001c:
			*((UINT16 *)(DrvScrollRAM + 0x00)) = data;
		return;

		case 0x08001e:
			// write to PIC
		return;
	}
}

void __fastcall vandykeb_main_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x080010:
			DrvScrollRAM[7] = data;
		return;

		case 0x080014:
		case 0x080015:
	//		*flipscreen = data & 1;
		return;

		case 0x080018:
		case 0x080019:
			if ((data & 0xff) != 0xff) {
				*tilebank = data;
			}
		return;

		case 0x08001a:
			DrvScrollRAM[3] = data;
		return;

		case 0x08001e:
		case 0x08001f:
			// write to PIC
		return;
	}
}

UINT8 __fastcall hachamf_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x080000:
		case 0x080001:
			return DrvInputs[0] >> ((~address & 1) << 3);

		case 0x080002:
		case 0x080003:
			return DrvInputs[1] >> ((~address & 1) << 3);

		case 0x080008:
		case 0x080009:
			return DrvDips[0];

		case 0x08000a:
		case 0x08000b:
			return DrvDips[1];

		case 0x08000e:
		case 0x08000f:
			return NMK004Read();
	}

	return 0;
}

UINT16 __fastcall hachamf_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x080000:
			return DrvInputs[0];

		case 0x080002:
			return DrvInputs[1];

		case 0x080008:
			return DrvDips[0];

		case 0x08000a:
			return DrvDips[1];

		case 0x08000e:
			return NMK004Read();
	}

	return 0;
}

void __fastcall hachamf_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xffff0000) == 0xf0000) {
		*((UINT16*)(Drv68KRAM + (address & 0xfffe))) = data;
		HachaRAMProt((address & 0xffff) >> 1);
		return;
	}

	switch (address)
	{
		case 0x080014:
	//		*flipscreen = data & 1;
		return;

		case 0x080016:
		case 0x080017:
			NMK004NmiWrite(data);
		return;

		case 0x080018:
			if ((data & 0xff) != 0xff) {
				*tilebank = data & 0xff;
			}
		return;

		case 0x08001e:
			NMK004Write(0, data);
		return;
	}
}

void __fastcall hachamf_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xffff0000) == 0xf0000) {
		Drv68KRAM[(address & 0xffff) ^ 1] = data;
		HachaRAMProt((address & 0xffff) >> 1);
		return;
	}

	switch (address)
	{
		case 0x080014:
		case 0x080015:
	//		*flipscreen = data & 1;
		return;

		case 0x080016:
		case 0x080017:
			NMK004NmiWrite(data);
		return;

		case 0x080018:
		case 0x080019:
			if ((data & 0xff) != 0xff) {
				*tilebank = data;
			}
		return;

		case 0x08001e:
		case 0x08001f:
			NMK004Write(0, data);
		return;
	}
}


//-----------------------------------------------------------------------------------------------------


static void tharrier_sound_bankswitch(UINT8 *rom, INT32 bank)
{
	if (bank < 3) {
		memcpy (rom + 0x20000, rom + 0x40000 + (bank * 0x20000), 0x20000);
	}
}

void __fastcall tharrier_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xf000:
			*soundlatch2 = data;
		return;

		case 0xf400:
			MSM6295Command(0, data);
		return;

		case 0xf500:
			MSM6295Command(1, data);
		return;

		case 0xf600:
			tharrier_sound_bankswitch(DrvSndROM0, data & 3);
		return;

		case 0xf700:
			tharrier_sound_bankswitch(DrvSndROM1, data & 3);
		return;
	}
}

UINT8 __fastcall tharrier_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xf000:
			return *soundlatch;

		case 0xf400:
			return MSM6295ReadStatus(0);

		case 0xf500:
			return MSM6295ReadStatus(1);
	}

	return 0;
}

void __fastcall tharrier_sound_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			BurnYM2203Write(0, 0, data);
		return;

		case 0x01:
			BurnYM2203Write(0, 1, data);
		return;
	}
}

UINT8 __fastcall tharrier_sound_in(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
			return BurnYM2203Read(0, 0);
	}

	return 0;
}

static void ssmissin_okibank(INT32 bank)
{
	*okibank = bank & 3;
	if (strstr(BurnDrvGetTextA(DRV_NAME), "ssmiss") || strstr(BurnDrvGetTextA(DRV_NAME), "airatt")) {
		memcpy(DrvSndROM0 + 0x20000, DrvSndROM0 + 0x40000 + (bank & 3) * 0x20000, 0x20000);
	} else { // twin action & dolmen weird banking (m_oki1->set_bank_base((data & 3) * 0x40000);)
		memcpy(DrvSndROM0, DrvSndROM1 + (bank & 3) * 0x40000, 0x40000);
	}
}

void __fastcall ssmissin_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x9000:
			ssmissin_okibank(data);
		return;

		case 0x9800:
			MSM6295Command(0, data);
		return;
	}
}

UINT8 __fastcall ssmissin_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x9800:
			return MSM6295ReadStatus(0);

		case 0xa000:
			ZetSetIRQLine(0,    CPU_IRQSTATUS_NONE);
			return *soundlatch;
	}

	return 0;
}

void __fastcall afega_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xf808:
			BurnYM2151SelectRegister(data);
		return;

		case 0xf809:
			BurnYM2151WriteRegister(data);
		return;

		case 0xf80a:
			MSM6295Command(0, data);
		return;
	}
}

UINT8 __fastcall afega_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xf800:
			ZetSetIRQLine(0,    CPU_IRQSTATUS_NONE);
			return *soundlatch;

		case 0xf808:
		case 0xf809:
			return BurnYM2151ReadStatus();

		case 0xf80a:
			return MSM6295ReadStatus(0);
	}

	return 0;
}

void __fastcall firehawk_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xfff2:
		 	if (data == 0xfe)
		 		memcpy (DrvSndROM1, DrvSndROM1 + 0x40000, 0x40000);
		 	else if(data == 0xff)
		 		memcpy (DrvSndROM1, DrvSndROM1 + 0x80000, 0x40000);
		return;

		case 0xfff8:
			MSM6295Command(1, data);
		return;

		case 0xfffa:
			MSM6295Command(0, data);
		return;
	}

	if (address >= 0xfe00) {
		DrvZ80RAM[address & 0xfff] = data;
		return;	
	}
}

UINT8 __fastcall firehawk_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xfff0:
			ZetSetIRQLine(0,    CPU_IRQSTATUS_NONE);
			return *soundlatch;

		case 0xfff8:
			return MSM6295ReadStatus(1);

		case 0xfffa:
			return MSM6295ReadStatus(0);
	}

	if (address >= 0xfe00) {
		return DrvZ80RAM[address & 0xfff];
	}

	return 0;
}

static void macross2_sound_bank(INT32 bank)
{
	bank = (bank & 7) * 0x4000;

	ZetMapArea(0x8000, 0xbfff, 0, DrvZ80ROM + 0x10000 + bank);
	ZetMapArea(0x8000, 0xbfff, 2, DrvZ80ROM + 0x10000 + bank);
}

void __fastcall macross2_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xe001:
			macross2_sound_bank(data);
		return;

		case 0xf000:
			*soundlatch2 = data;
		return;
	}
}

UINT8 __fastcall macross2_sound_read(UINT16 address)
{
	if (address == 0xf000) return *soundlatch;

	return 0;
}

void __fastcall macross2_sound_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			BurnYM2203Write(0, 0, data);
		return;

		case 0x01:
			BurnYM2203Write(0, 1, data);
		return;

		case 0x80:
			MSM6295Command(0, data);
		return;

		case 0x88:
			MSM6295Command(1, data);
		return;

		case 0x90:
		case 0x91:
		case 0x92:
		case 0x93:
		case 0x94:
		case 0x95:
		case 0x96:
		case 0x97:
			NMK112_okibank_write(port & 7, data);
		return;
	}
}

UINT8 __fastcall macross2_sound_in(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
			return BurnYM2203Read(0, 0);

		case 0x80:
			return MSM6295ReadStatus(0);

		case 0x88:
			return MSM6295ReadStatus(1);
	}

	return 0;
}

//-----------------------------------------------------------------------------------------------------

static void DrvYM2203IrqHandler(INT32, INT32 nStatus)
{
	if (ZetGetActive() == -1) return;

	if (nStatus) {
		ZetSetIRQLine(0xff, CPU_IRQSTATUS_ACK);
	} else {
		ZetSetIRQLine(0,    CPU_IRQSTATUS_NONE);
	}
}

static void DrvYM2151IrqHandler(INT32 nStatus)
{
	if (ZetGetActive() == -1) return;

	if (nStatus) {
		ZetSetIRQLine(0xff, CPU_IRQSTATUS_ACK);
	} else {
		ZetSetIRQLine(0,    CPU_IRQSTATUS_NONE);
	}
}

inline static INT32 DrvSynchroniseStream(INT32 nSoundRate) // tharrier & manybloc
{
	return (INT64)(ZetTotalCycles() * nSoundRate / 6000000);
}

inline static double DrvGetTime()
{
	return (double)ZetTotalCycles() / 6000000;
}

inline static INT32 Macross2SynchroniseStream(INT32 nSoundRate)
{
	return (INT64)(ZetTotalCycles() * nSoundRate / 4000000);
}

inline static double Macross2GetTime()
{
	return (double)ZetTotalCycles() / 4000000;
}

static void MSM6295SetInitialBanks(INT32 chips)
{
	if (chips > 0) MSM6295SetBank(0, DrvSndROM0, 0, 0x3ffff);
	if (chips > 1) MSM6295SetBank(1, DrvSndROM1, 0, 0x3ffff);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	ZetOpen(0);
	ZetReset();
	ZetClose();

	BurnYM2203Reset();
	MSM6295Reset(0);
	MSM6295Reset(1);

	MSM6295SetInitialBanks(2);

	macross2_sound_enable = -1;
	prot_count = 0;

	return 0;
}

static INT32 SmissinDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	ZetOpen(0);
	ZetReset();
	ZetClose();

	MSM6295Reset(0);

	return 0;
}

static INT32 AfegaDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	ZetOpen(0);
	ZetReset();
	ZetClose();

	BurnYM2151Reset();
	MSM6295Reset(0);
	MSM6295Reset(1);

	MSM6295SetInitialBanks(2);

	return 0;
}

static INT32 BjtwinDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	MSM6295Reset(0);
	MSM6295Reset(1);

	NMK112Reset();
	MSM6295SetInitialBanks(2);

	return 0;
}

static INT32 SeibuSoundDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	seibu_sound_reset();
	MSM6295SetInitialBanks(2);

	return 0;
}

static INT32 NMK004DoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	NMK004_reset();

	MSM6295SetInitialBanks(2);

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x080000;
	DrvZ80ROM		= Next; Next += 0x030000;

	DrvGfxROM0		= Next; Next += 0x040000;
	DrvGfxROM1		= Next; Next += 0x800000;
	DrvGfxROM2		= Next; Next += 0x800000;

	if (strcmp(BurnDrvGetTextA(DRV_NAME), "raphero") == 0 || strcmp(BurnDrvGetTextA(DRV_NAME), "arcadian") == 0) {
					Next += 0x800000;
	}

	DrvTileROM		= Next; Next += 0x020000;

	MSM6295ROM		= Next;
	DrvSndROM0		= Next; Next += 0x300000;

	if (strcmp(BurnDrvGetTextA(DRV_NAME), "raphero") == 0 || strcmp(BurnDrvGetTextA(DRV_NAME), "arcadian") == 0) {
					Next += 0x140000;
	}

	DrvSndROM1		= Next; Next += 0x300000;

	if (strcmp(BurnDrvGetTextA(DRV_NAME), "raphero") == 0 || strcmp(BurnDrvGetTextA(DRV_NAME), "arcadian") == 0) {
					Next += 0x140000;
	}

	DrvPalette		= (UINT32*)Next; Next += 0x0400 * sizeof(UINT32);

	AllRam			= Next;

	DrvPalRAM		= Next; Next += 0x000800;
	Drv68KRAM		= Next; Next += 0x010000;
	DrvBgRAM0		= Next; Next += 0x004000;
	DrvBgRAM1		= Next; Next += 0x004000;
	DrvBgRAM2		= Next; Next += 0x004000;
	DrvBgRAM3		= Next; Next += 0x004000;
	DrvTxRAM		= Next; Next += 0x001000;
	DrvScrollRAM		= Next; Next += 0x001000;

	DrvSprBuf		= Next; Next += 0x001000;
	DrvSprBuf2		= Next; Next += 0x001000;
	DrvSprBuf3		= Next; Next += 0x001000;

	DrvZ80RAM		= Next; Next += 0x002000;

	soundlatch		= Next; Next += 0x000001;
	soundlatch2		= Next; Next += 0x000001;
	flipscreen		= Next; Next += 0x000001;
	tilebank		= Next; Next += 0x000001;
	okibank			= Next; Next += 0x000001;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode(INT32 len0, INT32 len1, INT32 len2)
{
	INT32 Plane[4]  = { 0x000, 0x001, 0x002, 0x003 };
	INT32 XOffs[16] = { 0x000, 0x004, 0x008, 0x00c, 0x010, 0x014, 0x018, 0x01c,
			  0x200, 0x204, 0x208, 0x20c, 0x210, 0x214, 0x218, 0x21c };
	INT32 YOffs[16] = { 0x000, 0x020, 0x040, 0x060, 0x080, 0x0a0, 0x0c0, 0x0e0,
			  0x100, 0x120, 0x140, 0x160, 0x180, 0x1a0, 0x1c0, 0x1e0 };

	UINT8 *tmp = (UINT8*)BurnMalloc((len2 >= len1) ? len2 : len1);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, len0);

	GfxDecode((len0 * 2) / ( 8 *  8), 4,  8,  8, Plane, XOffs, YOffs, 0x100, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, len1);

	GfxDecode((len1 * 2) / (16 * 16), 4, 16, 16, Plane, XOffs, YOffs, 0x400, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, len2);

	GfxDecode((len2 * 2) / (16 * 16), 4, 16, 16, Plane, XOffs, YOffs, 0x400, tmp, DrvGfxROM2);

	nGraphicsMask[0] = ((len0 * 2) / ( 8 *  8)) - 1;
	nGraphicsMask[1] = ((len1 * 2) / (16 * 16)) - 1;
	nGraphicsMask[2] = ((len2 * 2) / (16 * 16)) - 1;

	BurnFree (tmp);

	return 0;
}

static INT32 BjtwinGfxDecode(INT32 len0, INT32 len1, INT32 len2)
{
	INT32 Plane[4]  = { 0x000, 0x001, 0x002, 0x003 };
	INT32 XOffs[16] = { 0x000, 0x004, 0x008, 0x00c, 0x010, 0x014, 0x018, 0x01c,
			  0x200, 0x204, 0x208, 0x20c, 0x210, 0x214, 0x218, 0x21c };
	INT32 YOffs[16] = { 0x000, 0x020, 0x040, 0x060, 0x080, 0x0a0, 0x0c0, 0x0e0,
			  0x100, 0x120, 0x140, 0x160, 0x180, 0x1a0, 0x1c0, 0x1e0 };

	UINT8 *tmp = (UINT8*)BurnMalloc((len2 >= len1) ? len2 : len1);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, len0);

	GfxDecode((len0 * 2) / ( 8 *  8), 4,  8,  8, Plane, XOffs, YOffs, 0x100, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, len1);

	GfxDecode((len1 * 2) / ( 8 *  8), 4,  8,  8, Plane, XOffs, YOffs, 0x100, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, len2);

	GfxDecode((len2 * 2) / (16 * 16), 4, 16, 16, Plane, XOffs, YOffs, 0x400, tmp, DrvGfxROM2);

	nGraphicsMask[0] = ((len0 * 2) / ( 8 *  8)) - 1;
	nGraphicsMask[1] = ((len1 * 2) / ( 8 *  8)) - 1;
	nGraphicsMask[2] = ((len2 * 2) / (16 * 16)) - 1;

	BurnFree (tmp);

	return 0;
}

static INT32 GrdnstrmGfxDecode(INT32 len0, INT32 len1, INT32 len2)
{
	INT32 Plane[8]  = { 0x000, 0x001, 0x002, 0x003, (len1*4), (len1*4)+1, (len1*4)+2, (len1*4)+3 };
	INT32 XOffs[16] = { 0x000, 0x004, 0x008, 0x00c, 0x010, 0x014, 0x018, 0x01c,
			  0x200, 0x204, 0x208, 0x20c, 0x210, 0x214, 0x218, 0x21c };
	INT32 YOffs[16] = { 0x000, 0x020, 0x040, 0x060, 0x080, 0x0a0, 0x0c0, 0x0e0,
			  0x100, 0x120, 0x140, 0x160, 0x180, 0x1a0, 0x1c0, 0x1e0 };

	UINT8 *tmp = (UINT8*)BurnMalloc((len2 >= len1) ? len2 : len1);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, len0);

	GfxDecode((len0 * 2) / ( 8 *  8), 4,  8,  8, Plane, XOffs, YOffs, 0x100, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, len1);

	GfxDecode((len1 * 1) / (16 * 16), 8, 16, 16, Plane, XOffs, YOffs, 0x400, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, len2);

	GfxDecode((len2 * 2) / (16 * 16), 4, 16, 16, Plane, XOffs, YOffs, 0x400, tmp, DrvGfxROM2);

	nGraphicsMask[0] = ((len0 * 2) / ( 8 *  8)) - 1;
	nGraphicsMask[1] = ((len1 * 1) / (16 * 16)) - 1;
	nGraphicsMask[2] = ((len2 * 2) / (16 * 16)) - 1;
	is_8bpp = 1;

	BurnFree (tmp);

	return 0;
}

static INT32 DrvInit(INT32 (*pLoadCallback)()) // tharrier, manybloc
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (pLoadCallback) {
		if (pLoadCallback()) return 1;
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapArea(0x0000, 0xbfff, 0, DrvZ80ROM);
	ZetMapArea(0x0000, 0xbfff, 2, DrvZ80ROM);
	ZetMapArea(0xc000, 0xc7ff, 0, DrvZ80RAM);
	ZetMapArea(0xc000, 0xc7ff, 1, DrvZ80RAM);
	ZetMapArea(0xc000, 0xc7ff, 2, DrvZ80RAM);
	ZetSetWriteHandler(tharrier_sound_write);
	ZetSetReadHandler(tharrier_sound_read);
	ZetSetOutHandler(tharrier_sound_out);
	ZetSetInHandler(tharrier_sound_in);
	ZetClose();

	BurnSetRefreshRate(56.00);

	BurnYM2203Init(1, 1500000, &DrvYM2203IrqHandler, DrvSynchroniseStream, DrvGetTime, 0);
	BurnTimerAttachZet(6000000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE, 2.00, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.50, BURN_SND_ROUTE_BOTH);

	MSM6295Init(0, 4000000 / 165, 1);
	MSM6295Init(1, 4000000 / 165, 1);
	MSM6295SetRoute(0, 0.20, BURN_SND_ROUTE_BOTH);
	MSM6295SetRoute(1, 0.20, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 BjtwinInit(INT32 (*pLoadCallback)())
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (pLoadCallback) {
		if (pLoadCallback()) return 1;
	}

	SekInit(0, 0x68000);	
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(DrvPalRAM,		0x088000, 0x0887ff, MAP_RAM);
	SekMapMemory(DrvBgRAM0,		0x09c000, 0x09cfff, MAP_RAM);
	SekMapMemory(DrvBgRAM0,		0x09d000, 0x09dfff, MAP_RAM);
	SekMapMemory(Drv68KRAM,		0x0f0000, 0x0fffff, MAP_RAM);
	SekSetWriteWordHandler(0,	bjtwin_main_write_word);
	SekSetWriteByteHandler(0,	bjtwin_main_write_byte);
	SekSetReadWordHandler(0,	bjtwin_main_read_word);
	SekSetReadByteHandler(0,	bjtwin_main_read_byte);
	SekClose();

	BurnSetRefreshRate(56.00);

	MSM6295Init(0, 4000000 / 165, 1);
	MSM6295Init(1, 4000000 / 165, 1);
	MSM6295SetRoute(0, 0.20, BURN_SND_ROUTE_BOTH);
	MSM6295SetRoute(1, 0.20, BURN_SND_ROUTE_BOTH);
	MSM6295x2_only = 1;
	no_z80 = 1;

	NMK112_init(0, DrvSndROM0, DrvSndROM1, 0x100000, 0x100000);
	NMK112_enabled = 1;

	GenericTilesInit();

	BjtwinDoReset();

	return 0;
}

static INT32 Macross2Init()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv68KROM  + 0x000000,  0, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x000000,  1, 1)) return 1;
		memcpy (DrvZ80ROM + 0x10000, DrvZ80ROM, 0x20000);

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x000000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x200000,  5, 1)) return 1;
		BurnByteswap(DrvGfxROM2, 0x400000);

		if (BurnLoadRom(DrvSndROM0 + 0x000000,  6, 1)) return 1;

		if (BurnLoadRom(DrvSndROM1 + 0x000000,  7, 1)) return 1;

		DrvGfxDecode(0x20000, 0x200000, 0x400000);
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(DrvPalRAM,		0x120000, 0x1207ff, MAP_RAM);
	SekMapMemory(DrvScrollRAM,	0x130000, 0x1307ff, MAP_RAM);
	SekMapMemory(DrvBgRAM0,		0x140000, 0x143fff, MAP_RAM);
	SekMapMemory(DrvBgRAM1,		0x144000, 0x147fff, MAP_RAM);
	SekMapMemory(DrvBgRAM2,		0x148000, 0x14bfff, MAP_RAM);
	SekMapMemory(DrvBgRAM3,		0x14c000, 0x14ffff, MAP_RAM);
	SekMapMemory(DrvTxRAM,		0x170000, 0x170fff, MAP_RAM);
	SekMapMemory(DrvTxRAM,		0x171000, 0x171fff, MAP_RAM);
	SekMapMemory(Drv68KRAM,		0x1f0000, 0x1fffff, MAP_RAM);
	SekSetWriteWordHandler(0,	macross2_main_write_word);
	SekSetWriteByteHandler(0,	macross2_main_write_byte);
	SekSetReadWordHandler(0,	macross2_main_read_word);
	SekSetReadByteHandler(0,	macross2_main_read_byte);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapArea(0x0000, 0x7fff, 0, DrvZ80ROM);
	ZetMapArea(0x0000, 0x7fff, 2, DrvZ80ROM);
	ZetMapArea(0x8000, 0xbfff, 0, DrvZ80ROM + 0x8000);
	ZetMapArea(0x8000, 0xbfff, 2, DrvZ80ROM + 0x8000);
	ZetMapArea(0xc000, 0xdfff, 0, DrvZ80RAM);
	ZetMapArea(0xc000, 0xdfff, 1, DrvZ80RAM);
	ZetMapArea(0xc000, 0xdfff, 2, DrvZ80RAM);
	ZetSetWriteHandler(macross2_sound_write);
	ZetSetReadHandler(macross2_sound_read);
	ZetSetOutHandler(macross2_sound_out);
	ZetSetInHandler(macross2_sound_in);
	ZetClose();

	BurnSetRefreshRate(56.00);

	BurnYM2203Init(1, 1500000, &DrvYM2203IrqHandler, Macross2SynchroniseStream, Macross2GetTime, 0);
	BurnTimerAttachZet(4000000);

	if (Tdragon2mode) {
		BurnYM2203SetAllRoutes(0, 3.00, BURN_SND_ROUTE_BOTH);
		BurnYM2203SetPSGVolume(0, 0.50);
	} else {
		BurnYM2203SetAllRoutes(0, 0.90, BURN_SND_ROUTE_BOTH);
	}

	MSM6295Init(0, 4000000 / 165, 1);
	MSM6295Init(1, 4000000 / 165, 1);
	MSM6295SetRoute(0, 0.20, BURN_SND_ROUTE_BOTH);
	MSM6295SetRoute(1, 0.20, BURN_SND_ROUTE_BOTH);

	if (Tdragon2mode) {
		NMK112_init(0, DrvSndROM0, DrvSndROM1, 0x200000, 0x200000);
	} else {
		NMK112_init(0, DrvSndROM0, DrvSndROM1, 0x200000, 0x100000);
	}

	NMK112_enabled = 1;

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 MSM6295x1Init(INT32  (*pLoadCallback)())
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (pLoadCallback) {
		if (pLoadCallback()) return 1;
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapArea(0x0000, 0x7fff, 0, DrvZ80ROM);
	ZetMapArea(0x0000, 0x7fff, 2, DrvZ80ROM);
	ZetMapArea(0x8000, 0x87ff, 0, DrvZ80RAM);
	ZetMapArea(0x8000, 0x87ff, 1, DrvZ80RAM);
	ZetMapArea(0x8000, 0x87ff, 2, DrvZ80RAM);
	ZetSetWriteHandler(ssmissin_sound_write);
	ZetSetReadHandler(ssmissin_sound_read);
	ZetClose();

	BurnSetRefreshRate(56.00);

	MSM6295Init(0, 1000000 / 132, 0);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	MSM6295x1_only = 1;

	GenericTilesInit();

	SmissinDoReset();

	return 0;
}

static INT32 SeibuSoundInit(INT32 (*pLoadCallback)(), INT32 type)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (pLoadCallback) {
		if (pLoadCallback()) return 1;
	}

	SekInit(0, 0x68000);	
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x03ffff, MAP_ROM);

	if (type) {	// mustangb
		SekMapMemory(DrvPalRAM,		0x088000, 0x0887ff, MAP_RAM);
		SekMapMemory(DrvScrollRAM,	0x08c000, 0x08c3ff, MAP_WRITE);
		SekMapMemory(DrvBgRAM0,		0x090000, 0x093fff, MAP_RAM);
		SekMapMemory(DrvTxRAM,		0x09c000, 0x09c7ff, MAP_RAM);
		SekMapMemory(Drv68KRAM,		0x0f0000, 0x0fffff, MAP_ROM);
	} else {	// tdragonb
		SekMapMemory(Drv68KRAM,		0x0b0000, 0x0bffff, MAP_RAM);
		SekMapMemory(DrvScrollRAM,	0x0c4000, 0x0c43ff, MAP_WRITE);
		SekMapMemory(DrvPalRAM,		0x0c8000, 0x0c87ff, MAP_RAM);
		SekMapMemory(DrvBgRAM0,		0x0cc000, 0x0cffff, MAP_RAM);
		SekMapMemory(DrvTxRAM,		0x0d0000, 0x0d07ff, MAP_RAM);
	}
	SekSetWriteWordHandler(0,	mustangb_main_write_word);
	SekSetWriteByteHandler(0,	mustangb_main_write_byte);
	SekSetReadWordHandler(0,	mustangb_main_read_word);
	SekSetReadByteHandler(0,	mustangb_main_read_byte);
	SekClose();

	BurnSetRefreshRate(56.00);

	SeibuZ80ROM = DrvZ80ROM;
	SeibuZ80RAM = DrvZ80RAM;
	seibu_sound_init(0, 0, 3579545, 3579545, 1320000 / 132);

	GenericTilesInit();

	SeibuSoundDoReset();

	return 0;
}

static INT32 AfegaInit(INT32 (*pLoadCallback)(), void (*pZ80Callback)(), INT32 pin7high)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (pLoadCallback) {
		if (pLoadCallback()) return 1;
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(DrvPalRAM,		0x088000, 0x0887ff, MAP_RAM);
	SekMapMemory(DrvBgRAM0,		0x090000, 0x093fff, MAP_RAM);
	SekMapMemory(DrvTxRAM,		0x09c000, 0x09c7ff, MAP_RAM);
	SekMapMemory(DrvScrollRAM,	0x084000, 0x0843ff, MAP_RAM);
	SekMapMemory(DrvScrollRAM,	0x08c000, 0x08c3ff, MAP_RAM);
	SekMapMemory(Drv68KRAM,		0x0c0000, 0x0cffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,		0x0f0000, 0x0fffff, MAP_ROM);
	SekSetWriteWordHandler(0,	afega_main_write_word);
	SekSetWriteByteHandler(0,	afega_main_write_byte);
	SekSetReadWordHandler(0,	afega_main_read_word);
	SekSetReadByteHandler(0,	afega_main_read_byte);
	SekClose();

	if (pZ80Callback) {
		pZ80Callback();
	}

	BurnSetRefreshRate(56.00);

	BurnYM2151Init(4000000);
	BurnYM2151SetIrqHandler(&DrvYM2151IrqHandler);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.30, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.30, BURN_SND_ROUTE_RIGHT);

	MSM6295Init(0, 1000000 / (pin7high ? 132 : 165), 1);
	MSM6295Init(1, 1000000 / (pin7high ? 132 : 165), 1);
	MSM6295SetRoute(0, 0.60, BURN_SND_ROUTE_BOTH);
	MSM6295SetRoute(1, 0.60, BURN_SND_ROUTE_BOTH);
	AFEGA_SYS = 1;

	GenericTilesInit();

	AfegaDoReset();

	return 0;
}

static INT32 NMK004Init(INT32 (*pLoadCallback)(), INT32 nCpuSpeed)
{
	BurnSetRefreshRate(56.00);

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	NMK004OKIROM0 = DrvSndROM0;
	NMK004OKIROM1 = DrvSndROM1;
	NMK004PROGROM = DrvZ80ROM;

	nNMK004CpuSpeed = nCpuSpeed;

	if (pLoadCallback) {
		pLoadCallback();
	}

	if (BurnLoadRom(NMK004PROGROM + 0x0000, 0x80, 1)) return 1; // load nmk004 rom ^^

	Strahlmode = (strncmp(BurnDrvGetTextA(DRV_NAME), "strahl", 6) == 0);
	NMK004_init();

	no_z80 = 1;
	NMK004_enabled = 1;

	GenericTilesInit();

	NMK004DoReset();

	return 0;
}

static INT32 CommonExit()
{
	GenericTilesExit();

	SekExit();

	BurnFree (AllMem);

	input_high[0] = input_high[1] = 0;
	is_8bpp = 0;
	global_y_offset = 16;
	videoshift = 0;
	screen_flip_y = 0;

	return 0;
}

static INT32 DrvExit()
{
	ZetExit();
	BurnYM2203Exit();
	MSM6295Exit(0);
	MSM6295Exit(1);
	MSM6295ROM = NULL;
	NMK112_enabled = 0;
	Tharriermode = 0;
	Macrossmode = 0;
	Strahlmode = 0;
	Tdragon2mode = 0;
	TharrierShakey = 0;

	return CommonExit();
}

static INT32 MSM6295x1Exit()
{
	ZetExit();
	MSM6295Exit(0);
	MSM6295ROM = NULL;
	MSM6295x1_only = 0;

	return CommonExit();
}

static INT32 SeibuSoundExit()
{
	GenericTilesExit();

	seibu_sound_exit();
	SekExit();

	BurnFree (AllMem);

	input_high[0] = input_high[1] = 0;
	is_8bpp = 0;
	global_y_offset = 16;
	videoshift = 0;

	return 0;
}

static INT32 AfegaExit()
{
	ZetExit();
	BurnYM2151Exit();
	MSM6295Exit(0);
	MSM6295Exit(1);
	MSM6295ROM = NULL;
	AFEGA_SYS = 0;

	return CommonExit();
}

static INT32 BjtwinExit()
{
	MSM6295Exit(0);
	MSM6295Exit(1);
	MSM6295ROM = NULL;
	MSM6295x2_only = 0;
	no_z80 = 0;
	NMK112_enabled = 0;

	return CommonExit();
}


static INT32 NMK004Exit()
{
	NMK004_exit();
	MSM6295ROM = NULL;
	no_z80 = 0;
	NMK004_enabled = 0;
	GunnailMode = 0;
	mustang_bg_xscroll = 0;

	return CommonExit();
}

//------------------------------------------------------------------------------------------------------------------

static void DrvPaletteRecalc()
{
	UINT8 r,g,b;
	UINT16 *pal = (UINT16*)DrvPalRAM;
	for (INT32 i = 0; i < 0x400; i++) {
		INT32 data = BURN_ENDIAN_SWAP_INT16(pal[i]);

		r = ((data >> 11) & 0x1e) | ((data >> 3) & 0x01);
		g = ((data >>  7) & 0x1e) | ((data >> 2) & 0x01);
		b = ((data >>  3) & 0x1e) | ((data >> 1) & 0x01);

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);

		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}
}

static void draw_sprites(INT32 flip, INT32 coloff, INT32 coland, INT32 priority)
{
	UINT16 *sprram = (Tharriermode) ? (UINT16*)DrvSprBuf3 : (UINT16*)DrvSprBuf2;

	if (Tharriermode && TharrierShakey && nCurrentFrame & 1) {
		sprram = (UINT16*)DrvSprBuf2;
	}

	for (INT32 offs = 0; offs < 0x1000/2; offs += 8)
	{
		if (BURN_ENDIAN_SWAP_INT16(sprram[offs]) & 0x0001)
		{
			INT32 sx    =  (BURN_ENDIAN_SWAP_INT16(sprram[offs+4]) & 0x01ff) + videoshift;
			INT32 sy    =  (BURN_ENDIAN_SWAP_INT16(sprram[offs+6]) & 0x01ff);
			INT32 code  =   BURN_ENDIAN_SWAP_INT16(sprram[offs+3]) & nGraphicsMask[2];
			INT32 color =   BURN_ENDIAN_SWAP_INT16(sprram[offs+7]) & coland;
			INT32 w     =  (BURN_ENDIAN_SWAP_INT16(sprram[offs+1]) & 0x000f);
			INT32 h     = ((BURN_ENDIAN_SWAP_INT16(sprram[offs+1]) & 0x00f0) >> 4);
			INT32 pri   = ((BURN_ENDIAN_SWAP_INT16(sprram[offs+0]) & 0x00c0) >> 6);
			INT32 flipy = ((BURN_ENDIAN_SWAP_INT16(sprram[offs+1]) & 0x0200) >> 9);
			INT32 flipx = ((BURN_ENDIAN_SWAP_INT16(sprram[offs+1]) & 0x0100) >> 8);

			if (!flip) flipy = flipx = 0;

			color = (color << 4) + coloff;

			INT32 delta = 16;

			if (priority != -1 && pri != priority)
				continue;

			if (*flipscreen)
			{
				sx = 368 - sx;
				sy = 240 - sy;
				delta = -16;

				flipx ^= *flipscreen;
				flipy ^= *flipscreen;
			}

			INT32 yy = h;
			sy += flipy ? (delta * h) : 0;

			do
			{
				INT32 x = sx + (flipx ? (delta * w) : 0);
				INT32 xx = w;

				do
				{
					INT32 xxx = ((x + 16) & 0x1ff) - 16;
					INT32 yyy = (sy & 0x1ff) - global_y_offset;

					if (flipy) {
						if (flipx) {
							Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, xxx, yyy, color, 0, 15, 0, DrvGfxROM2);
						} else {
							Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, xxx, yyy, color, 0, 15, 0, DrvGfxROM2);
						}
					} else {
						if (flipx) {
							Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, xxx, yyy, color, 0, 15, 0, DrvGfxROM2);
						} else {
							Render16x16Tile_Mask_Clip(pTransDraw, code, xxx, yyy, color, 0, 15, 0, DrvGfxROM2);
						}
					}
					code = (code + 1) & nGraphicsMask[2];
					x += delta * (flipx ? -1 : 1);

				} while (--xx >= 0);
				sy += delta * (flipy ? -1 : 1);

			} while (--yy >= 0);
		}
	}
}

static void draw_sprites_tdragon2(INT32 flip, INT32 coloff, INT32 coland)
{
	UINT16 *sprram;

	static INT32 bittbl[8] = {
		4, 6, 5, 7, 3, 2, 1, 0
	};

	for (INT32 i = 0; i < 0x100; i++)
	{
		INT32 spr = BITSWAP08(i, bittbl[0], bittbl[1], bittbl[2], bittbl[3], bittbl[4], bittbl[5], bittbl[6], bittbl[7]);
		sprram = (UINT16*)DrvSprBuf2 + ((spr << 4) >> 1);

		if (BURN_ENDIAN_SWAP_INT16(sprram[0]) & 0x0001)
		{
			INT32 sx    =  (BURN_ENDIAN_SWAP_INT16(sprram[4]) & 0x01ff) + videoshift;
			INT32 sy    =  (BURN_ENDIAN_SWAP_INT16(sprram[6]) & 0x01ff);
			INT32 code  =   BURN_ENDIAN_SWAP_INT16(sprram[3]) & nGraphicsMask[2];
			INT32 color =   BURN_ENDIAN_SWAP_INT16(sprram[7]) & coland;
			INT32 w     =  (BURN_ENDIAN_SWAP_INT16(sprram[1]) & 0x000f);
			INT32 h     = ((BURN_ENDIAN_SWAP_INT16(sprram[1]) & 0x00f0) >> 4);
			INT32 flipy = ((BURN_ENDIAN_SWAP_INT16(sprram[1]) & 0x0200) >> 9);
			INT32 flipx = ((BURN_ENDIAN_SWAP_INT16(sprram[1]) & 0x0100) >> 8);

			if (!flip) flipy = flipx = 0;

			color = (color << 4) + coloff;

			INT32 delta = 16;

			if (*flipscreen)
			{
				sx = 368 - sx;
				sy = 240 - sy;
				delta = -16;

				flipx ^= *flipscreen;
				flipy ^= *flipscreen;
			}

			INT32 yy = h;
			sy += flipy ? (delta * h) : 0;

			do
			{
				INT32 x = sx + (flipx ? (delta * w) : 0);
				INT32 xx = w;

				do
				{
					INT32 xxx = ((x + 16) & 0x1ff) - 16;
					INT32 yyy = (sy & 0x1ff) - global_y_offset;

					if (flipy) {
						if (flipx) {
							Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, xxx, yyy, color, 0, 15, 0, DrvGfxROM2);
						} else {
							Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, xxx, yyy, color, 0, 15, 0, DrvGfxROM2);
						}
					} else {
						if (flipx) {
							Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, xxx, yyy, color, 0, 15, 0, DrvGfxROM2);
						} else {
							Render16x16Tile_Mask_Clip(pTransDraw, code, xxx, yyy, color, 0, 15, 0, DrvGfxROM2);
						}
					}
					code = (code + 1) & nGraphicsMask[2];
					x += delta * (flipx ? -1 : 1);

				} while (--xx >= 0);
				sy += delta * (flipy ? -1 : 1);

			} while (--yy >= 0);
		}
	}
}

static void draw_macross_background(UINT8 *ram, INT32 scrollx, INT32 scrolly, INT32 coloff, INT32 t)
{
	scrolly = (scrolly + global_y_offset) & 0x1ff;
	UINT16 *vram = (UINT16*)ram;

	for (INT32 offs = 0; offs < 0x100 * 0x20; offs++)
	{
		INT32 sx = (offs & 0xff) << 4;
		INT32 sy = (offs >> 8) << 4;

		INT32 row = sy >> 4;
		INT32 col = sx >> 4;

		sx = (((sx - scrollx) + 16) & 0xfff) - 16;
		sy = (((sy - scrolly) + 16) & 0x1ff) - 16;
		if (sx >= nScreenWidth || sy >= nScreenHeight) continue;

		INT32 ofst = ((row >> 4) << 12) | (row & 0x0f) | (col << 4);

		if (is_8bpp) {
			INT32 code = BURN_ENDIAN_SWAP_INT16(vram[ofst]) & nGraphicsMask[1];

			Render16x16Tile_Clip(pTransDraw, code, sx, sy, 0, 8, coloff, DrvGfxROM1);
		} else {
			INT32 code  = ((BURN_ENDIAN_SWAP_INT16(vram[ofst]) & 0xfff) | (*tilebank << 12)) & nGraphicsMask[1];
			INT32 color =  BURN_ENDIAN_SWAP_INT16(vram[ofst]) >> 12;

			if (t) {
				Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 4, 15, coloff, DrvGfxROM1);
			} else {
				Render16x16Tile_Clip(pTransDraw, code, sx, sy, color, 4, coloff, DrvGfxROM1);
			}
		}
	}
}

static void draw_gunnail_background(UINT8 *ram)
{
	INT32 bank = (*tilebank << 12) & nGraphicsMask[1]; // good enough??
	UINT16 *vram = (UINT16*)ram;
	UINT16 *scroll = (UINT16*)DrvScrollRAM;

	for (INT32 y = 16; y < nScreenHeight + 16; y++)
	{
		INT32 yscroll = (BURN_ENDIAN_SWAP_INT16(scroll[0x100]) + BURN_ENDIAN_SWAP_INT16(scroll[0x100 | y]) + y) & 0x1ff;

		INT32 row = yscroll >> 4;
		INT32 yl = (yscroll & 0x0f) << 4;

		INT32 ofst0 = ((row >> 4) << 12) | (row & 0x0f);
		INT32 xscroll0 = BURN_ENDIAN_SWAP_INT16(scroll[0]) + BURN_ENDIAN_SWAP_INT16(scroll[y]) - videoshift;

		UINT16 *dest = pTransDraw + (y - 16) * nScreenWidth;

		for (INT32 x = 0; x < nScreenWidth + 16; x+=16)
		{
			INT32 xscroll = (x + xscroll0) & 0xfff;
			INT32 sx = x - (xscroll & 0x0f);

			INT32 ofst  = ofst0 | (xscroll & 0xff0);

			INT32 code  = (BURN_ENDIAN_SWAP_INT16(vram[ofst]) & 0xfff) | bank;
			INT32 color =  (BURN_ENDIAN_SWAP_INT16(vram[ofst]) >> 12) << 4;

			UINT8 *src = DrvGfxROM1 + (code << 8) + yl;

			for (INT32 xx = 0; xx < 16; xx++, sx++) {
				if (sx < 0 || sx >= nScreenWidth) continue;

				dest[sx] = src[xx] | color;
			}
		}
	}
}

static void draw_bjtwin_background(INT32 scrollx)
{
	UINT16 *vram = (UINT16*)DrvBgRAM0;

	for (INT32 offs = 0; offs < 64 * 32; offs++)
	{
		INT32 sx = (offs >> 5) << 3;
		INT32 sy = (offs & 0x1f) << 3;

		sy -= global_y_offset;
		sx = (((sx - scrollx) + 8) & 0x1ff) - 8;
		if (sx >= nScreenWidth || sy >= nScreenHeight) continue;

		INT32 code  = BURN_ENDIAN_SWAP_INT16(vram[offs]);
		INT32 color = code >> 12;
		INT32 bank  = code & 0x800;
		code &= 0x7ff;
		if (bank) code |= (*tilebank << 11);
		code &= nGraphicsMask[1];

		Render8x8Tile_Clip(pTransDraw, code, sx, sy, color, 4, 0, bank ? DrvGfxROM1 : DrvGfxROM0);
	}
}

static void bioship_draw_background(INT32 scrollx, INT32 scrolly)
{
	scrolly = (scrolly + global_y_offset) & 0x1ff;
	INT32 bank = *tilebank * 0x2000;
	UINT16 *tilerom = (UINT16*)DrvTileROM;

	for (INT32 offs = 0; offs < 0x1000; offs++)
	{
		INT32 sx = (offs >> 4) << 4;
		INT32 sy = (offs & 0x0f) << 4;

		sx = (((sx + 16) - scrollx) & 0xfff) - 16;

		if (sx >= nScreenWidth) continue;

		INT32 code = BURN_ENDIAN_SWAP_INT16(tilerom[offs | bank]);
		sy = (((sy + 16) - scrolly) & 0x1ff) - 16;

		if (sy < nScreenHeight) {
			Render16x16Tile_Clip(pTransDraw, code & 0xfff, sx, sy, code >> 12, 4, 0, DrvGfxROM1 + 0x100000);
		}

		code = BURN_ENDIAN_SWAP_INT16(tilerom[offs | bank | 0x1000]);
		sy = (((sy + 16) + 256) & 0x1ff) - 16;

		if (sy < nScreenHeight) {
			Render16x16Tile_Clip(pTransDraw, code & 0xfff, sx, sy, code >> 12, 4, 0, DrvGfxROM1 + 0x100000);
		}
	}
}

static void draw_tharriermacross1_text_layer(INT32 scrollx, INT32 scrolly, INT32 wide, INT32 coloff)
{
	if (nGraphicsMask[0] == 0) return;

	scrolly = (scrolly + global_y_offset) & 0x1ff;
	INT32 pf_width = (0x100 << wide);
	UINT16 *vram = (UINT16*)DrvTxRAM;

	for (INT32 offs = 0; offs < 32 * (32 << wide); offs++)
	{
		INT32 sx = (offs >> 5) << 3;
		INT32 sy = (offs & 0x1f) << 3;

		sx -= scrollx;
		if (sx < -7) sx += pf_width; // wrap
		sy -= scrolly;
		if (sy < -7) sy += 256; // wrap

		if (sx >= nScreenWidth || sy >= nScreenHeight) continue;

		INT32 code = BURN_ENDIAN_SWAP_INT16(vram[offs]);

		Render8x8Tile_Mask_Clip(pTransDraw, code & 0xfff, sx, sy, code >> 12, 4, 15, coloff, DrvGfxROM0);
	}
}

static void draw_macross_text_layer(INT32 scrollx, INT32 scrolly, INT32 wide, INT32 coloff)
{
	if (nGraphicsMask[0] == 0) return;

	scrolly = (scrolly + global_y_offset) & 0x1ff;
	INT32 wmask = (0x100 << wide) - 1;
	UINT16 *vram = (UINT16*)DrvTxRAM;

	for (INT32 offs = 0; offs < 32 * (32 << wide); offs++)
	{
		INT32 sx = (offs >> 5) << 3;
		INT32 sy = (offs & 0x1f) << 3;

		sx = (((sx - scrollx) + 8) & wmask) - 8;
		sy = (((sy - scrolly) + 8) & 0xff) - 8;

		if (sx >= nScreenWidth || sy >= nScreenHeight) continue;

		INT32 code = BURN_ENDIAN_SWAP_INT16(vram[offs]);

		Render8x8Tile_Mask_Clip(pTransDraw, code & 0xfff, sx, sy, code >> 12, 4, 15, coloff, DrvGfxROM0);
	}
}

static void draw_screen_yflip()
{
	if (!screen_flip_y) return;

	UINT16 *tmp = (UINT16*)pBurnDraw; // :D
	UINT16 *src1 = pTransDraw;
	UINT16 *src2 = pTransDraw + (nScreenHeight-1) * nScreenWidth;

	for (INT32 y = 0; y < nScreenHeight / 2; y++) {
		memcpy (tmp,  src1, nScreenWidth * 2);
		memcpy (src1, src2, nScreenWidth * 2);
		memcpy (src2, tmp,  nScreenWidth * 2);
		src1 += nScreenWidth;
		src2 -= nScreenWidth;
	}
}

static inline void common_draw(INT32 spriteflip, INT32 bgscrollx, INT32 bgscrolly, INT32 txscrollx, INT32 txscrolly, INT32 tx_coloff, INT32 wide)
{
	DrvPaletteRecalc();

	if (nBurnLayer & 1) draw_macross_background(DrvBgRAM0, bgscrollx, bgscrolly, 0, 0);

	if (spriteflip == -1 || Tharriermode) {
		if (nSpriteEnable & 1) draw_sprites((spriteflip == -1) ? 0 : 1, 0x100, 0x0f, -1); // order-based
	} else { // priority-based
		if (nSpriteEnable & 1) draw_sprites(spriteflip, 0x100, 0x0f, 3);
		if (nSpriteEnable & 2) draw_sprites(spriteflip, 0x100, 0x0f, 2);
		if (nSpriteEnable & 4) draw_sprites(spriteflip, 0x100, 0x0f, 1);
		if (nSpriteEnable & 8) draw_sprites(spriteflip, 0x100, 0x0f, 0);
	}

	if (Tharriermode || Macrossmode) { // Tharrier and Macross 1
		if (nBurnLayer & 2) draw_tharriermacross1_text_layer(txscrollx, txscrolly, wide, tx_coloff);
	} else { // Macross 2 and all the rest...
		if (nBurnLayer & 2) draw_macross_text_layer(txscrollx, txscrolly, wide, tx_coloff);
	}

	draw_screen_yflip();
	BurnTransferCopy(DrvPalette);
}

static INT32 TharrierDraw()
{
	INT32 scrollx = BURN_ENDIAN_SWAP_INT16(*((UINT16*)(Drv68KRAM + 0x9f00))) & 0xfff;

	{
		// shakey ship hack
		// f3310 & f3410 are 0x100 during the shakey transition
		UINT16 *f3310 = (UINT16*)&Drv68KRAM[0x3310>>0];
		UINT16 *f3410 = (UINT16*)&Drv68KRAM[0x3410>>0];

		TharrierShakey = (f3310[0] == 0x100 && f3410[0] == 0x100);
	}

	common_draw(1, scrollx, 0, 0, 0, 0, 0);

	return 0;
}

static INT32 ManyblocDraw()
{
	UINT16 *scroll = (UINT16*)DrvScrollRAM;
	INT32 scrollx = BURN_ENDIAN_SWAP_INT16(scroll[0x82 / 2]) & 0xfff;
	INT32 scrolly = BURN_ENDIAN_SWAP_INT16(scroll[0xc2 / 2]) & 0x1ff;

	common_draw(1, scrollx, scrolly, 0, 0, 0, 0);

	return 0;
}

static INT32 MacrossDraw()
{
	UINT16 *scroll = (UINT16*)DrvScrollRAM;
	INT32 scrollx = ((BURN_ENDIAN_SWAP_INT16(scroll[0]) & 0x0f) << 8) | (BURN_ENDIAN_SWAP_INT16(scroll[1]) & 0xff);
	INT32 scrolly = ((BURN_ENDIAN_SWAP_INT16(scroll[2]) & 0x01) << 8) | (BURN_ENDIAN_SWAP_INT16(scroll[3]) & 0xff);

	common_draw(0, scrollx, scrolly, 0, 0, 0x200, 0);

	return 0;
}

static INT32 MustangDraw()
{
	//UINT16 *scroll = (UINT16*)DrvScrollRAM;
	INT32 scrollx = mustang_bg_xscroll; //((BURN_ENDIAN_SWAP_INT16(scroll[0]) & 0x0f) << 8) | (BURN_ENDIAN_SWAP_INT16(scroll[1]) & 0xff);
	INT32 scrolly = 0; //((BURN_ENDIAN_SWAP_INT16(scroll[2]) & 0x01) << 8) | (BURN_ENDIAN_SWAP_INT16(scroll[3]) & 0xff);

	common_draw(-1, scrollx, scrolly, 0, 0, 0x200, 1);

	return 0;
}

static INT32 AcrobatmDraw()
{
	UINT16 *scroll = (UINT16*)DrvScrollRAM;
	INT32 scrollx = ((BURN_ENDIAN_SWAP_INT16(scroll[0]) & 0x0f) << 8) | (BURN_ENDIAN_SWAP_INT16(scroll[1]) & 0xff);
	INT32 scrolly = ((BURN_ENDIAN_SWAP_INT16(scroll[2]) & 0x01) << 8) | (BURN_ENDIAN_SWAP_INT16(scroll[3]) & 0xff);

	common_draw(0, scrollx, scrolly, 0, 0, 0x200, 1);

	return 0;
}

static INT32 VandykeDraw()
{
	UINT16 *scroll = (UINT16*)DrvScrollRAM;
	INT32 scrollx = ((BURN_ENDIAN_SWAP_INT16(scroll[0]) & 0x0f) << 8) | (BURN_ENDIAN_SWAP_INT16(scroll[1]) >> 8);
	INT32 scrolly = ((BURN_ENDIAN_SWAP_INT16(scroll[2]) & 0x01) << 8) | (BURN_ENDIAN_SWAP_INT16(scroll[3]) >> 8);

	common_draw(0, scrollx, scrolly, 0, 0, 0x200, 1);

	return 0;
}

static INT32 RedhawkiDraw()
{
	UINT16 *scroll = (UINT16*)DrvScrollRAM;
	INT32 scrollx = BURN_ENDIAN_SWAP_INT16(scroll[2]) & 0xff;
	INT32 scrolly = BURN_ENDIAN_SWAP_INT16(scroll[3]) & 0xff;

	common_draw(0, scrollx, scrolly, 0, 0, 0x300, 0);

	return 0;
}

static INT32 FirehawkDraw()
{
	UINT16 *scroll = (UINT16*)DrvScrollRAM;
	INT32 scrolly = (BURN_ENDIAN_SWAP_INT16(scroll[3]) + 0x100) & 0x1ff;
	INT32 scrollx = (BURN_ENDIAN_SWAP_INT16(scroll[1]) - 0x100) & 0xfff;

	common_draw(1, scrollx, scrolly, 0, 0, 0x200, 0);

	return 0;
}

static INT32 HachamfDraw()
{
	if (nNMK004CpuSpeed == 10000000) {	// hachamf
		mcu_run(0);
	} else {				// tdragon
		mcu_run(1); 
	}

	UINT16 *scroll = (UINT16*)DrvScrollRAM;
	INT32 scrollx = ((BURN_ENDIAN_SWAP_INT16(scroll[0]) & 0x0f) << 8) | (BURN_ENDIAN_SWAP_INT16(scroll[1]) & 0xff);
	INT32 scrolly = ((BURN_ENDIAN_SWAP_INT16(scroll[2]) & 0x01) << 8) | (BURN_ENDIAN_SWAP_INT16(scroll[3]) & 0xff);

	common_draw(0, scrollx, scrolly, 0, 0, 0x200, 1);

	return 0;
}

static INT32 StrahlDraw()
{
	DrvPaletteRecalc();

	UINT16 *scroll = (UINT16*)DrvScrollRAM;

	INT32 bgscrollx = ((BURN_ENDIAN_SWAP_INT16(scroll[0x000]) & 0x0f) << 8) | (BURN_ENDIAN_SWAP_INT16(scroll[0x001]) & 0xff);
	INT32 bgscrolly = ((BURN_ENDIAN_SWAP_INT16(scroll[0x002]) & 0x01) << 8) | (BURN_ENDIAN_SWAP_INT16(scroll[0x003]) & 0xff);
	INT32 fgscrollx = ((BURN_ENDIAN_SWAP_INT16(scroll[0x200]) & 0x0f) << 8) | (BURN_ENDIAN_SWAP_INT16(scroll[0x201]) & 0xff);
	INT32 fgscrolly = ((BURN_ENDIAN_SWAP_INT16(scroll[0x202]) & 0x01) << 8) | (BURN_ENDIAN_SWAP_INT16(scroll[0x203]) & 0xff);

	draw_macross_background(DrvBgRAM0, bgscrollx, bgscrolly, 0x300, 0);

	INT32 bgbank_bak = *tilebank; *tilebank = 0x01;
	draw_macross_background(DrvBgRAM1, fgscrollx, fgscrolly, 0x200, 1);
	*tilebank = bgbank_bak;

	draw_sprites(0, 0x100, 0x0f, 3);
	draw_sprites(0, 0x100, 0x0f, 2);
	draw_sprites(0, 0x100, 0x0f, 1);
	draw_sprites(0, 0x100, 0x0f, 0);

	draw_macross_text_layer(0, 0, 1, 0);

	draw_screen_yflip();
	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 Macross2Draw()
{
	videoshift = 64;
	DrvPaletteRecalc();

	UINT16 *scroll = (UINT16*)DrvScrollRAM;

	INT32 scrollx = ((BURN_ENDIAN_SWAP_INT16(scroll[0]) & 0x0f) << 8) | (BURN_ENDIAN_SWAP_INT16(scroll[1]) & 0xff);
	INT32 scrolly = ((BURN_ENDIAN_SWAP_INT16(scroll[2]) & 0x01) << 8) | (BURN_ENDIAN_SWAP_INT16(scroll[3]) & 0xff);

	switch (scroll[0] & 0x30)
	{
		case 0x00: draw_macross_background(DrvBgRAM0, (scrollx - 64) & 0xfff, scrolly, 0, 0); break;
		case 0x10: draw_macross_background(DrvBgRAM1, (scrollx - 64) & 0xfff, scrolly, 0, 0); break;
		case 0x20: draw_macross_background(DrvBgRAM2, (scrollx - 64) & 0xfff, scrolly, 0, 0); break;
		case 0x30: draw_macross_background(DrvBgRAM3, (scrollx - 64) & 0xfff, scrolly, 0, 0); break;
	}	

	if (Tdragon2mode) {
		draw_sprites_tdragon2(0, 0x100, 0x1f);
	} else {
		draw_sprites(0, 0x100, 0x1f, 3);
		draw_sprites(0, 0x100, 0x1f, 2);
		draw_sprites(0, 0x100, 0x1f, 1);
		draw_sprites(0, 0x100, 0x1f, 0);
	}

	draw_macross_text_layer(-64, 0, 1, 0x300);

	draw_screen_yflip();
	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 GunnailDraw() 
{
	DrvPaletteRecalc();

	videoshift = 64;
	UINT16 *scroll = (UINT16*)DrvScrollRAM;

	switch ((BURN_ENDIAN_SWAP_INT16(scroll[0]) >> 8) & 0x30)
	{
		case 0x00: draw_gunnail_background(DrvBgRAM0); break;
		//case 0x10: draw_gunnail_background(DrvBgRAM1); break;
		//case 0x20: draw_gunnail_background(DrvBgRAM2); break;
		//case 0x30: draw_gunnail_background(DrvBgRAM3); break;

		// GunNail only has a single ram bank. If it tries to
		// use another, just clear everything.

		case 0x10:
		case 0x20:
		case 0x30:
			BurnTransferClear();
		break;
	}	

	draw_sprites(0, 0x100, 0x0f, 3);
	draw_sprites(0, 0x100, 0x0f, 2);
	draw_sprites(0, 0x100, 0x0f, 1);
	draw_sprites(0, 0x100, 0x0f, 0);

	draw_macross_text_layer(-64, 0, 1, 0x200);

	draw_screen_yflip();
	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 RapheroDraw() 
{
	DrvPaletteRecalc();

	videoshift = 64;
	UINT16 *scroll = (UINT16*)DrvScrollRAM;

	switch ((BURN_ENDIAN_SWAP_INT16(scroll[0]) >> 8) & 0x30)
	{
		case 0x00: draw_gunnail_background(DrvBgRAM0); break;
		case 0x10: draw_gunnail_background(DrvBgRAM1); break;
		case 0x20: draw_gunnail_background(DrvBgRAM2); break;
		case 0x30: draw_gunnail_background(DrvBgRAM3); break;
	}	

	draw_sprites(0, 0x100, 0x1f, 3);
	draw_sprites(0, 0x100, 0x1f, 2);
	draw_sprites(0, 0x100, 0x1f, 1);
	draw_sprites(0, 0x100, 0x1f, 0);

	draw_macross_text_layer(-64, 0, 1, 0x300);

	draw_screen_yflip();
	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 BioshipDraw()
{
	DrvPaletteRecalc();

	UINT16 *scroll = (UINT16*)DrvScrollRAM;
	INT32 bgscrolly = (BURN_ENDIAN_SWAP_INT16(scroll[0x02]) & 0x100) | (BURN_ENDIAN_SWAP_INT16(scroll[0x03]) >> 8);
	INT32 bgscrollx = (BURN_ENDIAN_SWAP_INT16(scroll[0x00]) & 0xf00) | (BURN_ENDIAN_SWAP_INT16(scroll[0x01]) >> 8);
	INT32 fgscrolly = (BURN_ENDIAN_SWAP_INT16(scroll[0x02]) & 0x100) | (BURN_ENDIAN_SWAP_INT16(scroll[0x03]) >> 8);
	INT32 fgscrollx = (BURN_ENDIAN_SWAP_INT16(scroll[0x00]) & 0xf00) | (BURN_ENDIAN_SWAP_INT16(scroll[0x01]) >> 8);

	bioship_draw_background(bgscrollx, bgscrolly);

	INT32 bgbank_bak = *tilebank; *tilebank = 0;
	draw_macross_background(DrvBgRAM0, fgscrollx, fgscrolly, 0x100, 1);
	*tilebank = bgbank_bak;

	draw_sprites(0, 0x200, 0x0f, 3);
	draw_sprites(0, 0x200, 0x0f, 2);
	draw_sprites(0, 0x200, 0x0f, 1);
	draw_sprites(0, 0x200, 0x0f, 0);

	draw_macross_text_layer(0, 0, 1, 0x300);

	draw_screen_yflip();
	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 BjtwinDraw()
{
	videoshift = 64;

	DrvPaletteRecalc();

	draw_bjtwin_background(-64);

	draw_sprites(0, 0x100, 0x0f, 3);
	draw_sprites(0, 0x100, 0x0f, 2);
	draw_sprites(0, 0x100, 0x0f, 1);
	draw_sprites(0, 0x100, 0x0f, 0);

	draw_screen_yflip();
	BurnTransferCopy(DrvPalette);

	return 0;
}

static void AfegaCommonDraw(INT32 , INT32 xoffset, INT32 yoffset)
{
	UINT16 *scroll = (UINT16*)DrvScrollRAM;
	INT32 bgscrollx = (BURN_ENDIAN_SWAP_INT16(scroll[1]) + xoffset) & 0xfff;
	INT32 bgscrolly = (BURN_ENDIAN_SWAP_INT16(scroll[0]) + yoffset) & 0x1ff;
	INT32 txscrollx = (BURN_ENDIAN_SWAP_INT16(scroll[3]) & 0xff);
	INT32 txscrolly = (BURN_ENDIAN_SWAP_INT16(scroll[2]) & 0xff);

	common_draw(1, bgscrollx, bgscrolly, txscrollx, txscrolly, 0x200, 0);
}

static INT32 AfegaDraw()
{
	AfegaCommonDraw(1, -0x100, 0);
	return 0;
}

static INT32 RedhawkbDraw()
{
	AfegaCommonDraw(1, 0, 0x100);
	return 0;
}

static INT32 Bubl2000Draw()
{
	AfegaCommonDraw(0, -0x100, 0);
	return 0;
}

static INT32 DrvFrame() // tharrier, manybloc
{
	if (DrvReset) {
		DrvDoReset();
	}

	if (Tharriermode)
	{
		DrvInputs[0] = 0x8000;
		DrvInputs[1] = 0x0000;
		DrvInputs[2] = 0x0000;
		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		if (DrvInputs[0] & 0x0001) DrvInputs[1] ^= 0x0080;
		if (DrvInputs[0] & 0x0002) DrvInputs[1] ^= 0x0020;
		if (DrvInputs[0] & 0x0008) DrvInputs[1] ^= 0x0001;
		if (DrvInputs[0] & 0x0010) DrvInputs[1] ^= 0x0102;
	}
	else
	{
		DrvInputs[0] = ~input_high[0];
		DrvInputs[1] = ~input_high[1];
		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	ZetNewFrame();

	INT32 nSegment;
	INT32 nInterleave = 263;
	INT32 nTotalCycles[2] = { 12000000 / 56, 6000000 / 56 }; // a little oc to quench that horrible slowdown in tharrier
	INT32 nCyclesDone[2] = { 0, 0 };

	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nSegment = nTotalCycles[0] / nInterleave;
		nCyclesDone[0] += SekRun(nSegment);

		if (i == 25 || i == ((nInterleave / 2) + 25)) {
			SekSetIRQLine(1, CPU_IRQSTATUS_AUTO);
		}

		if (i == (nInterleave - 1)) SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);
		if (i == 240-1) SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);

		if (i == 241-1) { //sprdma
			memcpy (DrvSprBuf3, DrvSprBuf2, 0x1000);
			memcpy (DrvSprBuf2, Drv68KRAM + 0x8000, 0x1000);
		}

		BurnTimerUpdate((i + 1) * (nTotalCycles[1] / nInterleave));
	}

	BurnTimerEndFrame(nTotalCycles[1]);

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
		MSM6295Render(1, pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	SekClose();

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {			// Return minimum compatible version
		*pnMin = 0x029732;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = MemEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
            if (!no_z80)
                ZetScan(nAction);
            SekScan(nAction);
            if (strcmp(BurnDrvGetTextA(DRV_NAME), "raphero") == 0 || strcmp(BurnDrvGetTextA(DRV_NAME), "arcadian") == 0) {
                tlcs90Scan(nAction);
            }

            if (AFEGA_SYS) {
                if (strstr(BurnDrvGetTextA(DRV_NAME), "ssmiss") ||
                    strstr(BurnDrvGetTextA(DRV_NAME), "twinact") ||
                    strstr(BurnDrvGetTextA(DRV_NAME), "dolmen")) {
                    // Afega with no YM
                } else {
                    if (!MSM6295x2_only && !MSM6295x1_only)
                        BurnYM2151Scan(nAction); // twin action,etc dont use this
                                             // and will crash if called.
                }
            } else {
                // Everything else
                if (!MSM6295x1_only && !MSM6295x2_only)
                    BurnYM2203Scan(nAction, pnMin);
            }

            MSM6295Scan(0, nAction);
            if (!MSM6295x1_only)
                MSM6295Scan(1, nAction);

            SCAN_VAR(macross2_sound_enable);
            if (NMK004_enabled) {
                NMK004Scan(nAction, pnMin);
			}

			if (NMK112_enabled) {
				NMK112_Scan(nAction);
			}

        }
	
        if (nAction & ACB_WRITE) {
            if (MSM6295x1_only) // S.S. Mission, Dolmen & Twin Action
                ssmissin_okibank(*okibank);
        }

	return 0;
}

static INT32 SsmissinFrame()
{
	if (DrvReset) {
		SmissinDoReset();
	}

	{
		DrvInputs[0] = ~input_high[0];
		DrvInputs[1] = ~input_high[1];
		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	ZetNewFrame();

	INT32 nSegment;
	INT32 nInterleave = 256;
	INT32 nTotalCycles[2] = { 8000000 / 56, 4000000 / 56 };
	INT32 nCyclesDone[2] = { 0, 0 };

	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nSegment = nTotalCycles[0] / nInterleave;

		nCyclesDone[0] += SekRun(nSegment);

/*		if (i == (nInterleave-1) || i == ((nInterleave / 2) - 1)) {
			SekSetIRQLine(1, CPU_IRQSTATUS_AUTO);
			SekRun(0);
		}
		if (i == ((nInterleave/2)-1) && nNMK004EnableIrq2)
			SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);
		if (i == (nInterleave-1))
			SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
*/

		if (i == 25 || i == 148) { // 25, 153 in MAME
			SekSetIRQLine(1, CPU_IRQSTATUS_AUTO);
		}
		if (i == 0) {
			SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);
		}
		if (i == 235) { // 240 in MAME, but causes a missing life-bar in VanDyke.  236 causes a little flicker in the life-bar, 235 = perfect.
			SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
		}

		ZetRun(nTotalCycles[1] / nInterleave);
	}

	if (pBurnSoundOut) {
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	SekClose();

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	memcpy (DrvSprBuf2, Drv68KRAM + 0x8000, 0x1000);

	return 0;
}

static INT32 Macross2Frame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		DrvInputs[0] = ~input_high[0];
		DrvInputs[1] = ~input_high[1];
		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	ZetNewFrame();

	INT32 nSegment;
	INT32 nInterleave = 200;
	INT32 nTotalCycles[2] = { 10000000 / 56, 4000000 / 56 };
	INT32 nCyclesDone[2] = { 0, 0 };

	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nSegment = nTotalCycles[0] / nInterleave;
		nCyclesDone[0] += SekRun(nSegment);
		if (i == 1 || i == (nInterleave / 2)) {
			SekSetIRQLine(1, CPU_IRQSTATUS_AUTO);
		}
		if (i == (nInterleave-1)) {
			SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
		}

		if (macross2_sound_enable) {
			//BurnTimerUpdate((nSegment * 4) / 10);
			BurnTimerUpdate(i * (nTotalCycles[1] / nInterleave));
		}
	}

	if (macross2_sound_enable) {
		BurnTimerEndFrame(nTotalCycles[1]);
	}

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
		MSM6295Render(1, pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	SekClose();

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	memcpy (DrvSprBuf2, Drv68KRAM + 0x8000, 0x1000);

	return 0;
}

static INT32 AfegaFrame()
{
	if (DrvReset) {
		AfegaDoReset();
	}

	ZetNewFrame();

	{
		DrvInputs[0] = ~input_high[0];
		DrvInputs[1] = ~input_high[1];
		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 10;
	INT32 nSoundBufferPos = 0;
	INT32 nCyclesTotal[2] = { 12000000 / 56, 4000000 / 56 };
	INT32 nCyclesDone[2] = { 0, 0 };

	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++) {
		INT32 nSegment;

		nSegment = nCyclesTotal[0] / nInterleave;
		nCyclesDone[0] += SekRun(nSegment);
		if (i == (nInterleave / 2) - 1) SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);
		if (i == (nInterleave)     - 1) SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);

		nSegment = nCyclesTotal[1] / nInterleave;
		nCyclesDone[1] += ZetRun(nSegment);
		
		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			MSM6295Render(0, pSoundBuf, nSegmentLength);
			MSM6295Render(1, pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);

		if (nSegmentLength) {
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			MSM6295Render(0, pSoundBuf, nSegmentLength);
			MSM6295Render(1, pSoundBuf, nSegmentLength);
		}
	}

	ZetClose();
	SekClose();

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	memcpy (DrvSprBuf2, Drv68KRAM + 0x8000, 0x1000);

	return 0;
}

static INT32 BjtwinFrame()
{
	if (DrvReset) {
		BjtwinDoReset();
	}

	{
		DrvInputs[0] = ~input_high[0];
		DrvInputs[1] = ~input_high[1];
		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 10;
	INT32 nCyclesTotal[1] = { 10000000 / 56 };
	INT32 nCyclesDone[1] = { 0 };

	SekOpen(0);

	for (INT32 i = 0; i < nInterleave; i++) {
		INT32 nSegment;

		nSegment = nCyclesTotal[0] / nInterleave;
		nCyclesDone[0] += SekRun(nSegment);
		if (i == (nInterleave-1) || i == ((nInterleave / 2) - 1)) {
			SekSetIRQLine(1, CPU_IRQSTATUS_AUTO);
		}
		if (i == (nInterleave-1)) {
			SekRun(0);
			SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
		}
	}

	if (pBurnSoundOut) {
		memset (pBurnSoundOut, 0, nBurnSoundLen * 2 * sizeof(INT16));
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
		MSM6295Render(1, pBurnSoundOut, nBurnSoundLen);
	}

	SekClose();

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	memcpy (DrvSprBuf2, Drv68KRAM + 0x8000, 0x1000);

	return 0;
}

static INT32 SeibuSoundFrame()
{
	if (DrvReset) {
		SeibuSoundDoReset();
	}

	ZetNewFrame();

	{
		DrvInputs[0] = ~input_high[0];
		DrvInputs[1] = ~input_high[1];
		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 100;
	INT32 nCyclesTotal[2] = { 10000000 / 56, 3579545 / 56 };
	INT32 nCyclesDone[2] = { 0, 0 };

	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++) {
		INT32 nSegment;

		nSegment = nCyclesTotal[0] / nInterleave;
		nCyclesDone[0] += SekRun(nSegment);
		if (i == (nInterleave-1) || i == ((nInterleave / 2) - 1)) {
			SekSetIRQLine(1, CPU_IRQSTATUS_AUTO);
		}

		if (i == ((nInterleave/2)-1)) {
			SekRun(0);
			SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);
		}

		if (i == (nInterleave-1)) {
			SekRun(0);
			SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
		}

		BurnTimerUpdateYM3812(i * (nCyclesTotal[1] / nInterleave));
	}

	BurnTimerEndFrameYM3812(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		seibu_sound_update(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	SekClose();

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	memcpy (DrvSprBuf2, Drv68KRAM + 0x8000, 0x1000);

	return 0;
}

static INT32 NMK004Frame()
{
	if (DrvReset) {
		NMK004DoReset();
	}

	{
		DrvInputs[0] = ~input_high[0];
		DrvInputs[1] = ~input_high[1];
		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	if (GunnailMode && (LastFakeDip != DrvDips[2])) {
		// GunNail - Trap15's Nice Health-warning mod - lowers the volume of the
		// "low sheild" warning beeper to a more tollerable level.
		LastFakeDip = DrvDips[2];
		DrvZ80ROM[0x52e6] = (DrvDips[2] == 0x08) ? 0xCD : 0x9D;
	}

	SekNewFrame();
	tlcs90NewFrame();

	INT32 nSegment;
	INT32 nInterleave = 256;
	UINT32 nTotalCycles[2] = { nNMK004CpuSpeed / 56, 8000000 / 56 };
	INT32 nCyclesDone[2] = { 0, 0 };

	SekOpen(0);
	tlcs90Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nSegment = nTotalCycles[0] / nInterleave;

		nCyclesDone[0] += SekRun(nSegment);

		if (i == 237) { // 241 in MAME (see i == 235 comment)
			if (Strahlmode) {
				memcpy (DrvSprBuf2, Drv68KRAM + 0xf000, 0x1000);
			} else {
				memcpy (DrvSprBuf2, Drv68KRAM + 0x8000, 0x1000);
			}
		}

		if (i == 25 || i == 148) { // 25, 153 in MAME
			SekSetIRQLine(1, CPU_IRQSTATUS_AUTO);
		}
		if (i == 0) {
			SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);
		}
		if (i == 235) { // 240 in MAME, but causes a missing life-bar in VanDyke.  236 causes a little flicker in the life-bar, 235 = perfect.
			SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
		}

		nSegment = i * (nTotalCycles[1] / nInterleave);
		BurnTimerUpdate(nSegment);
	}

	BurnTimerEndFrame(nTotalCycles[1]);

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
		MSM6295Render(1, pBurnSoundOut, nBurnSoundLen);
	}

	tlcs90Close();
	SekClose();

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}


//-------------------------------------------------------------------------------------------------------------------------------------

static void decryptcode(INT32 len, INT32 a17, INT32 a16, INT32 a15, INT32 a14, INT32 a13)
{
	UINT8 *buf = (UINT8*)malloc(len);

	memcpy (buf, Drv68KROM, len);

	for (INT32 i = 0; i < len; i++) {
		Drv68KROM[i] = buf[BITSWAP24(i, 23,22,21,20,19,18,a17,a16,a15,a14,a13,12,11,10,9,8,7,6,5,4,3,2,1,0)];
	}

	if (buf) {
		free (buf);
		buf = NULL;
	}
}

static UINT32 bjtwin_address_map_bg0(UINT32 addr)
{
	return ((addr & 0x00004) >> 2) | ((addr & 0x00800) >> 10) | ((addr & 0x40000) >> 16);
}

static UINT8 decode_byte(UINT8 src, const UINT8 *bitp)
{
	UINT8 ret = 0;
	for (INT32 i=0; i<8; i++)
		ret |= (((src >> bitp[i]) & 1) << (7-i));

	return ret;
}

static UINT16 decode_word(UINT16 src, const UINT8 *bitp)
{
	UINT16 ret=0;
	for (INT32 i = 0; i < 16; i++)
		ret |= (((src >> bitp[i]) & 1) << (15-i));

	return ret;
}

static UINT32 bjtwin_address_map_sprites(UINT32 addr)
{
	return ((addr & 0x00010) >> 4) | ((addr & 0x20000) >> 16) | ((addr & 0x100000) >> 18);
}

static void decode_gfx(INT32 gfxlen0, INT32 gfxlen1)
{
	static const UINT8 decode_data_bg[8][8] =
	{
		{0x3,0x0,0x7,0x2,0x5,0x1,0x4,0x6},
		{0x1,0x2,0x6,0x5,0x4,0x0,0x3,0x7},
		{0x7,0x6,0x5,0x4,0x3,0x2,0x1,0x0},
		{0x7,0x6,0x5,0x0,0x1,0x4,0x3,0x2},
		{0x2,0x0,0x1,0x4,0x3,0x5,0x7,0x6},
		{0x5,0x3,0x7,0x0,0x4,0x6,0x2,0x1},
		{0x2,0x7,0x0,0x6,0x5,0x3,0x1,0x4},
		{0x3,0x4,0x7,0x6,0x2,0x0,0x5,0x1},
	};

	static const UINT8 decode_data_sprite[8][16] =
	{
		{0x9,0x3,0x4,0x5,0x7,0x1,0xb,0x8,0x0,0xd,0x2,0xc,0xe,0x6,0xf,0xa},
		{0x1,0x3,0xc,0x4,0x0,0xf,0xb,0xa,0x8,0x5,0xe,0x6,0xd,0x2,0x7,0x9},
		{0xf,0xe,0xd,0xc,0xb,0xa,0x9,0x8,0x7,0x6,0x5,0x4,0x3,0x2,0x1,0x0},
		{0xf,0xe,0xc,0x6,0xa,0xb,0x7,0x8,0x9,0x2,0x3,0x4,0x5,0xd,0x1,0x0},
		{0x1,0x6,0x2,0x5,0xf,0x7,0xb,0x9,0xa,0x3,0xd,0xe,0xc,0x4,0x0,0x8},
		{0x7,0x5,0xd,0xe,0xb,0xa,0x0,0x1,0x9,0x6,0xc,0x2,0x3,0x4,0x8,0xf},
		{0x0,0x5,0x6,0x3,0x9,0xb,0xa,0x7,0x1,0xd,0x2,0xe,0x4,0xc,0x8,0xf},
		{0x9,0xc,0x4,0x2,0xf,0x0,0xb,0x8,0xa,0xd,0x3,0x6,0x5,0xe,0x1,0x7},
	};

	for (INT32 A = 0; A < gfxlen0; A++) {
		DrvGfxROM1[A] = decode_byte(DrvGfxROM1[A], decode_data_bg[bjtwin_address_map_bg0(A)]);
	}

	for (INT32 A = 0; A < gfxlen1; A += 2)
	{
		UINT16 tmp = decode_word((DrvGfxROM2[A+1] << 8) | DrvGfxROM2[A], decode_data_sprite[bjtwin_address_map_sprites(A)]);
		DrvGfxROM2[A+1] = tmp >> 8;
		DrvGfxROM2[A] = tmp & 0xff;
	}
}

static void ssmissin_decode()
{
	for (INT32 A = 0; A < 0x100000; A++)
	{
		DrvGfxROM1[A] = BITSWAP08(DrvGfxROM1[A], 7, 6, 5, 3, 4, 2, 1, 0);
		DrvGfxROM2[A] = BITSWAP08(DrvGfxROM2[A], 7, 6, 5, 3, 4, 2, 1, 0);
	}
}

static void decode_tdragonb()
{
	static const UINT8 decode_data_tdragonb[16] = {
		0xe,0xc,0xa,0x8,0x7,0x5,0x3,0x1,0xf,0xd,0xb,0x9,0x6,0x4,0x2,0x0
	};

	for (INT32 A = 0; A < 0x40000; A += 2)
	{
		UINT16 tmp = decode_word((Drv68KROM[A+1] << 8) | Drv68KROM[A], decode_data_tdragonb);
		Drv68KROM[A+1] = tmp >> 8;
		Drv68KROM[A] = tmp & 0xff;
	}

	ssmissin_decode();
}

//-------------------------------------------------------------------------------------------------------------

static struct BurnRomInfo emptyRomDesc[] = {
	{ "", 0, 0, 0 },
};

// NMK004 Internal ROM

static struct BurnRomInfo nmk004RomDesc[] = {
#if !defined (ROM_VERIFY)
	{ "nmk004.bin", 		0x002000, 0x83b6f611, BRF_PRG | BRF_BIOS },	// 0x80 tlcs90 internal rom
#else
	{ "", 		0x000000, 0x00000000, BRF_PRG | BRF_BIOS },	// 0x80 tlcs90 internal rom
#endif
};

STD_ROM_PICK(nmk004)
STD_ROM_FN(nmk004)

struct BurnDriver BurnDrvnmk004 = {
	"nmk004", NULL, NULL, NULL, "1990",
	"NMK004 Internal ROM\0", "internal rom", "N M K Corporation", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_BOARDROM, 0, HARDWARE_MISC_POST90S, GBF_BIOS, 0,
	NULL, nmk004RomInfo, nmk004RomName, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, 0,
	320, 224, 4, 3
};


// Task Force Harrier

static struct BurnRomInfo tharrierRomDesc[] = {
	{ "2.bin",		0x020000, 0xf3887a44, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "3.bin",		0x020000, 0x65c247f6, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "12",			0x010000, 0xb959f837, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 code

	{ "1.bin",		0x010000, 0x005c26c3, 3 | BRF_GRA },           //  3 Characters

	{ "89050-4",	0x080000, 0x64d7d687, 4 | BRF_GRA },           //  4 Tiles

	{ "89050-13",	0x080000, 0x24db3fa4, 5 | BRF_GRA },           //  5 Sprites
	{ "89050-17",	0x080000, 0x7f715421, 5 | BRF_GRA },           //  6

	{ "89050-8",	0x080000, 0x11ee4c39, 6 | BRF_SND },           //  7 OKI1 Samples

	{ "89050-10",	0x080000, 0x893552ab, 7 | BRF_SND },           //  8 OKI2 Samples

	{ "21.bpr",		0x000100, 0xfcd5efea, 0 | BRF_OPT },           //  9 Unused proms
	{ "22.bpr",		0x000100, 0xcfdbb86c, 0 | BRF_OPT },           // 10
	{ "23.bpr",		0x000020, 0xfc3569f4, 0 | BRF_OPT },           // 11
	{ "24.bpr",		0x000100, 0xe0a009fe, 0 | BRF_OPT },           // 12
	{ "25.bpr",		0x000100, 0xe0a009fe, 0 | BRF_OPT },           // 13
	{ "26.bpr",		0x000020, 0x0cbfb33e, 0 | BRF_OPT },           // 14
};

STD_ROM_PICK(tharrier)
STD_ROM_FN(tharrier)

static INT32 TharrierLoadCallback()
{
	{
		if (BurnLoadRom(Drv68KROM  + 0x00001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x00000,  1, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x00000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000,  5, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x00001,  6, 2)) return 1;

		if (BurnLoadRom(DrvSndROM0 + 0x00000,  7, 1)) return 1;
		memmove (DrvSndROM0 + 0x40000, DrvSndROM0 + 0x20000, 0x60000);

		if (BurnLoadRom(DrvSndROM1 + 0x00000,  8, 1)) return 1;
		memmove (DrvSndROM1 + 0x40000, DrvSndROM1 + 0x20000, 0x60000);

		DrvGfxDecode(0x10000, 0x80000, 0x100000);
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(DrvPalRAM,		0x088000, 0x0883ff, MAP_RAM);
	SekMapMemory(DrvBgRAM0,		0x090000, 0x093fff, MAP_RAM);
	SekMapMemory(DrvScrollRAM,	0x09c000, 0x09c7ff, MAP_RAM);
	SekMapMemory(DrvTxRAM,		0x09d000, 0x09d7ff, MAP_RAM);
	SekMapMemory(Drv68KRAM,		0x0f0000, 0x0fffff, MAP_ROM);
	SekSetWriteWordHandler(0,	tharrier_main_write_word);
	SekSetWriteByteHandler(0,	tharrier_main_write_byte);
	SekSetReadWordHandler(0,	tharrier_main_read_word);
	SekSetReadByteHandler(0,	tharrier_main_read_byte);
	SekClose();

	return 0;
}

static INT32 TharrierInit()
{
	input_high[0] = 0x7fff;
	input_high[1] = 0xffff;
	Tharriermode = 1;
	TharrierShakey = 0;

	return DrvInit(TharrierLoadCallback);
}

struct BurnDriver BurnDrvTharrier = {
	"tharrier", NULL, NULL, NULL, "1989",
	"Task Force Harrier\0", NULL, "UPL", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, tharrierRomInfo, tharrierRomName, NULL, NULL, TharrierInputInfo, TharrierDIPInfo,
	TharrierInit, DrvExit, DrvFrame, TharrierDraw, DrvScan, NULL, 0x200,
	224, 256, 3, 4
};

// Task Force Harrier (US?)

static struct BurnRomInfo tharrieruRomDesc[] = {
	{ "2",			0x020000, 0x78923aaa, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "3",			0x020000, 0x99cea259, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "12",			0x010000, 0xb959f837, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 code

	{ "1",			0x010000, 0xc7402e4a, 3 | BRF_GRA },           //  3 Characters

	{ "89050-4",	0x080000, 0x64d7d687, 4 | BRF_GRA },           //  4 Tiles

	{ "89050-13",	0x080000, 0x24db3fa4, 5 | BRF_GRA },           //  5 Sprites
	{ "89050-17",	0x080000, 0x7f715421, 5 | BRF_GRA },           //  6

	{ "89050-8",	0x080000, 0x11ee4c39, 6 | BRF_SND },           //  7 OKI1 Samples

	{ "89050-10",	0x080000, 0x893552ab, 7 | BRF_SND },           //  8 OKI2 Samples

	{ "21.bpr",		0x000100, 0xfcd5efea, 0 | BRF_OPT },           //  9 Unused proms
	{ "22.bpr",		0x000100, 0xcfdbb86c, 0 | BRF_OPT },           // 10
	{ "23.bpr",		0x000020, 0xfc3569f4, 0 | BRF_OPT },           // 11
	{ "24.bpr",		0x000100, 0xe0a009fe, 0 | BRF_OPT },           // 12
	{ "25.bpr",		0x000100, 0xe0a009fe, 0 | BRF_OPT },           // 13
	{ "26.bpr",		0x000020, 0x0cbfb33e, 0 | BRF_OPT },           // 14
};

STD_ROM_PICK(tharrieru)
STD_ROM_FN(tharrieru)

struct BurnDriver BurnDrvTharrieru = {
	"tharrieru", "tharrier", NULL, NULL, "1989",
	"Task Force Harrier (US?)\0", NULL, "UPL (American Sammy license)", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, tharrieruRomInfo, tharrieruRomName, NULL, NULL, TharrierInputInfo, TharrierDIPInfo,
	TharrierInit, DrvExit, DrvFrame, TharrierDraw, DrvScan, NULL, 0x200,
	224, 256, 3, 4
};


// Many Block

static struct BurnRomInfo manyblocRomDesc[] = {
	{ "1-u33.bin",		0x020000, 0x07473154, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "2-u35.bin",		0x020000, 0x04acd8c1, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "3-u146.bin",		0x010000, 0x7bf5fafa, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 code

	{ "12-u39.bin",		0x010000, 0x413b5438, 3 | BRF_GRA },           //  3 Characters

	{ "5-u97.bin",		0x040000, 0x536699e6, 4 | BRF_GRA },           //  4 Ciles
	{ "4-u96.bin",		0x040000, 0x28af2640, 4 | BRF_GRA },           //  5

	{ "8-u54b.bin",		0x020000, 0x03eede77, 5 | BRF_GRA },           //  6 Sprites
	{ "10-u86b.bin",	0x020000, 0x9eab216f, 5 | BRF_GRA },           //  7
	{ "9-u53b.bin",		0x020000, 0xdfcfa040, 5 | BRF_GRA },           //  8
	{ "11-u85b.bin",	0x020000, 0xfe747dd5, 5 | BRF_GRA },           //  9

	{ "6-u131.bin",		0x040000, 0x79a4ae75, 6 | BRF_SND },           // 10 OKI1 Samples
	{ "7-u132.bin",		0x040000, 0x21db875e, 6 | BRF_SND },           // 11

	{ "u200.bpr",		0x000020, 0x1823600b, 0 | BRF_OPT },           // 12 Unused proms
	{ "u7.bpr",			0x000100, 0xcfdbb86c, 0 | BRF_OPT },           // 13
	{ "u10.bpr",		0x000200, 0x8e9b569a, 0 | BRF_OPT },           // 14
	{ "u120.bpr",		0x000100, 0x576c5984, 0 | BRF_OPT },           // 15
};

STD_ROM_PICK(manybloc)
STD_ROM_FN(manybloc)

static INT32 ManyblocLoadCallback()
{
	{
		if (BurnLoadRom(Drv68KROM  + 0x00000,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x00001,  1, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x00000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x40000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000,  6, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x00001,  7, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x40000,  8, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x40001,  9, 2)) return 1;

		if (BurnLoadRom(DrvSndROM0 + 0x00000, 10, 1)) return 1;
		if (BurnLoadRom(DrvSndROM0 + 0x60000, 11, 1)) return 1;
		memmove (DrvSndROM0 + 0x40000, DrvSndROM0 + 0x20000, 0x20000);

		DrvGfxDecode(0x10000, 0x80000, 0x80000);
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(DrvPalRAM,		0x088000, 0x0883ff, MAP_RAM);
	SekMapMemory(DrvBgRAM0,		0x090000, 0x093fff, MAP_RAM);
	SekMapMemory(DrvScrollRAM,	0x09c000, 0x09cfff, MAP_RAM);
	SekMapMemory(DrvTxRAM,		0x09d000, 0x09d7ff, MAP_RAM);
	SekMapMemory(Drv68KRAM,		0x0f0000, 0x0fffff, MAP_RAM);
	SekSetWriteWordHandler(0,	manybloc_main_write_word);
	SekSetWriteByteHandler(0,	manybloc_main_write_byte);
	SekSetReadWordHandler(0,	manybloc_main_read_word);
	SekSetReadByteHandler(0,	manybloc_main_read_byte);
	SekClose();

	return 0;
}

static INT32 ManyblocInit()
{
	global_y_offset = 8;
	input_high[0] = 0x7fff;
	input_high[1] = 0xffff;

	return DrvInit(ManyblocLoadCallback);
}

struct BurnDriver BurnDrvManybloc = {
	"manybloc", NULL, NULL, NULL, "1991",
	"Many Block\0", NULL, "Bee-Oh", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, manyblocRomInfo, manyblocRomName, NULL, NULL, ManyblocInputInfo, ManyblocDIPInfo,
	ManyblocInit, DrvExit, DrvFrame, ManyblocDraw, DrvScan, NULL, 0x200,
	240, 256, 3, 4
};


// S.S. Mission

static struct BurnRomInfo ssmissinRomDesc[] = {
	{ "ssm14.165",		0x020000, 0xeda61b74, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "ssm15.166",		0x020000, 0xaff15927, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ssm11.188",		0x008000, 0x8be6dce3, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 code

	{ "ssm16.172",		0x020000, 0x5cf6eb1f, 3 | BRF_GRA },           //  3 Characters

	{ "ssm17.147",		0x080000, 0xc9c28455, 4 | BRF_GRA },           //  4 Tiles
	{ "ssm18.148",		0x080000, 0xebfdaad6, 4 | BRF_GRA },           //  5

	{ "ssm20.34",		0x080000, 0xa0c16c4d, 5 | BRF_GRA },           //  6 Sprites
	{ "ssm19.33",		0x080000, 0xb1943657, 5 | BRF_GRA },           //  7

	{ "ssm13.190",		0x020000, 0x618f66f0, 6 | BRF_SND },           //  8 OKI1 Samples
	{ "ssm12.189",		0x080000, 0xe8219c83, 6 | BRF_SND },           //  9

	{ "ssm-pr2.113",	0x000100, 0xcfdbb86c, 0 | BRF_OPT },           // 10 Unused proms
	{ "ssm-pr1.114",	0x000200, 0xed0bd072, 0 | BRF_OPT },           // 11
};

STD_ROM_PICK(ssmissin)
STD_ROM_FN(ssmissin)

static INT32 SsmissinLoadCallback()
{
	{
		if (BurnLoadRom(Drv68KROM  + 0x00000,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x00001,  1, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x00000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x80000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00001,  6, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x00000,  7, 2)) return 1;

		if (BurnLoadRom(DrvSndROM0 + 0x00000,  8, 1)) return 1;
		if (BurnLoadRom(DrvSndROM0 + 0x40000,  9, 1)) return 1;

		ssmissin_decode();
		DrvGfxDecode(0x20000, 0x100000, 0x100000);
	}

	SekInit(0, 0x68000);	
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,		0x0b0000, 0x0bffff, MAP_RAM);
	SekMapMemory(DrvScrollRAM,	0x0c4000, 0x0c43ff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		0x0c8000, 0x0c87ff, MAP_RAM);
	SekMapMemory(DrvBgRAM0,		0x0cc000, 0x0cffff, MAP_RAM);
	SekMapMemory(DrvTxRAM,		0x0d0000, 0x0d07ff, MAP_RAM);
	SekMapMemory(DrvTxRAM,		0x0d0800, 0x0d0fff, MAP_RAM);
	SekMapMemory(DrvTxRAM,		0x0d1000, 0x0d17ff, MAP_RAM);
	SekMapMemory(DrvTxRAM,		0x0d1800, 0x0d1fff, MAP_RAM);
	SekSetWriteWordHandler(0,	ssmissin_main_write_word);
	SekSetWriteByteHandler(0,	ssmissin_main_write_byte);
	SekSetReadWordHandler(0,	ssmissin_main_read_word);
	SekSetReadByteHandler(0,	ssmissin_main_read_byte);
	SekClose();

	return 0;
}

static INT32 SsmissinInit()
{
	return MSM6295x1Init(SsmissinLoadCallback);
}

struct BurnDriver BurnDrvSsmissin = {
	"ssmissin", NULL, NULL, NULL, "1992",
	"S.S. Mission\0", NULL, "Comad", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, ssmissinRomInfo, ssmissinRomName, NULL, NULL, SsmissinInputInfo, SsmissinDIPInfo,
	SsmissinInit, MSM6295x1Exit, SsmissinFrame, AcrobatmDraw, DrvScan, NULL, 0x400,
	224, 256, 3, 4
};


// Air Attack (set 1)

static struct BurnRomInfo airattckRomDesc[] = {
	{ "uc10.bin",		0x020000, 0x1837d4ba, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "ue10.bin",		0x020000, 0x71deb9d8, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "3.su6",			0x008000, 0x3e352370, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 code

	{ "4.ul10",			0x020000, 0xe9362ab4, 3 | BRF_GRA },           //  3 Characters

	{ "9.uw9",			0x080000, 0x86e59966, 4 | BRF_GRA },           //  4 Tiles
	{ "10.ux9",			0x080000, 0x122c8d04, 4 | BRF_GRA },           //  5

	{ "8.uo82",			0x080000, 0x9a83e3d8, 5 | BRF_GRA },           //  6 Sprites
	{ "7.uo81",			0x080000, 0x3c38d671, 5 | BRF_GRA },           //  7

	{ "2.su12",			0x020000, 0x93ab615b, 6 | BRF_SND },           //  8 OKI1 Samples
	{ "1.su13",			0x080000, 0x09a836bb, 6 | BRF_SND },           //  9

	{ "82s129.ug6",		0x000100, 0xcfdbb86c, 0 | BRF_OPT },           // 10 Unused proms
	{ "82s147.uh6",		0x000200, 0xed0bd072, 0 | BRF_OPT },           // 11
};

STD_ROM_PICK(airattck)
STD_ROM_FN(airattck)

struct BurnDriver BurnDrvAirattck = {
	"airattck", NULL, NULL, NULL, "1996",
	"Air Attack (set 1)\0", NULL, "Comad", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, airattckRomInfo, airattckRomName, NULL, NULL, SsmissinInputInfo, SsmissinDIPInfo,
	SsmissinInit, MSM6295x1Exit, SsmissinFrame, AcrobatmDraw, DrvScan, NULL, 0x400,
	224, 256, 3, 4
};


// Air Attack (set 2)

static struct BurnRomInfo airattckaRomDesc[] = {
	{ "5.ue10",			0x020000, 0x6589c005, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "6.uc10",			0x020000, 0x3572baf0, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "3.su6",			0x008000, 0x3e352370, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 code

	{ "4.ul10",			0x020000, 0xe9362ab4, 3 | BRF_GRA },           //  3 Characters

	{ "9.uw9",			0x080000, 0x86e59966, 4 | BRF_GRA },           //  4 Tiles
	{ "10.ux9",			0x080000, 0x122c8d04, 4 | BRF_GRA },           //  5

	{ "8.uo82",			0x080000, 0x9a83e3d8, 5 | BRF_GRA },           //  6 Sprites
	{ "7.uo81",			0x080000, 0x3c38d671, 5 | BRF_GRA },           //  7

	{ "2.su12",			0x020000, 0x93ab615b, 6 | BRF_SND },           //  8 OKI1 Samples
	{ "1.su13",			0x080000, 0x09a836bb, 6 | BRF_SND },           //  9

	{ "82s129.ug6",		0x000100, 0xcfdbb86c, 0 | BRF_OPT },           // 10 Unused proms
	{ "82s147.uh6",		0x000200, 0xed0bd072, 0 | BRF_OPT },           // 11
};

STD_ROM_PICK(airattcka)
STD_ROM_FN(airattcka)

struct BurnDriver BurnDrvAirattcka = {
	"airattcka", "airattck", NULL, NULL, "1996",
	"Air Attack (set 2)\0", NULL, "Comad", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, airattckaRomInfo, airattckaRomName, NULL, NULL, SsmissinInputInfo, SsmissinDIPInfo,
	SsmissinInit, MSM6295x1Exit, SsmissinFrame, MacrossDraw, DrvScan, NULL, 0x400,
	224, 256, 3, 4
};


// Super Spacefortress Macross II / Chou-Jikuu Yousai Macross II

static struct BurnRomInfo macross2RomDesc[] = {
	{ "mcrs2j.3",		0x080000, 0x36a618fe, 1 | BRF_PRG | BRF_ESS }, //  0 68k code

	{ "mcrs2j.2",		0x020000, 0xb4aa8ac7, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 code

	{ "mcrs2j.1",		0x020000, 0xc7417410, 3 | BRF_GRA },           //  2 Characters

	{ "bp932an.a04",	0x200000, 0xc4d77ff0, 4 | BRF_GRA },           //  3 Tiles

	{ "bp932an.a07",	0x200000, 0xaa1b21b9, 5 | BRF_GRA },           //  4 Sprites
	{ "bp932an.a08",	0x200000, 0x67eb2901, 5 | BRF_GRA },           //  5

	{ "bp932an.a06",	0x200000, 0xef0ffec0, 6 | BRF_SND },           //  6 OKI1 Samples

	{ "bp932an.a05",	0x100000, 0xb5335abb, 7 | BRF_SND },           //  7 OKI2 Samples

	{ "mcrs2bpr.9",		0x000100, 0x435653a2, 0 | BRF_OPT },           //  8 Unused proms
	{ "mcrs2bpr.10",	0x000100, 0xe6ead349, 0 | BRF_OPT },           //  9
};

STD_ROM_PICK(macross2)
STD_ROM_FN(macross2)

struct BurnDriver BurnDrvMacross2 = {
	"macross2", NULL, NULL, NULL, "1993",
	"Super Spacefortress Macross II / Chou-Jikuu Yousai Macross II\0", NULL, "Banpresto", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_HORSHOOT, 0,
	NULL, macross2RomInfo, macross2RomName, NULL, NULL, CommonInputInfo, Macross2DIPInfo,
	Macross2Init, DrvExit, Macross2Frame, Macross2Draw, DrvScan, NULL, 0x400,
	384, 224, 4, 3
};

// Super Spacefortress Macross II / Chou-Jikuu Yousai Macross II (GAMEST review build)

static struct BurnRomInfo macross2gRomDesc[] = {
	{ "3.u11",			0x080000, 0x151f9d39, 1 | BRF_PRG | BRF_ESS }, //  0 68k code

	{ "mcrs2j.2",		0x020000, 0xb4aa8ac7, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 code

	{ "mcrs2j.1",		0x020000, 0xc7417410, 3 | BRF_GRA },           //  2 Characters

	{ "bp932an.a04",	0x200000, 0xc4d77ff0, 4 | BRF_GRA },           //  3 Tiles

	{ "bp932an.a07",	0x200000, 0xaa1b21b9, 5 | BRF_GRA },           //  4 Sprites
	{ "bp932an.a08",	0x200000, 0x67eb2901, 5 | BRF_GRA },           //  5

	{ "bp932an.a06",	0x200000, 0xef0ffec0, 6 | BRF_SND },           //  6 OKI1 Samples

	{ "bp932an.a05",	0x100000, 0xb5335abb, 7 | BRF_SND },           //  7 OKI2 Samples

	{ "mcrs2bpr.9",		0x000100, 0x435653a2, 0 | BRF_OPT },           //  8 Unused proms
	{ "mcrs2bpr.10",	0x000100, 0xe6ead349, 0 | BRF_OPT },           //  9
};

STD_ROM_PICK(macross2g)
STD_ROM_FN(macross2g)

struct BurnDriver BurnDrvMacross2g = {
	"macross2g", "macross2", NULL, NULL, "1993",
	"Super Spacefortress Macross II / Chou-Jikuu Yousai Macross II (GAMEST review build)\0", NULL, "Banpresto", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_HORSHOOT, 0,
	NULL, macross2gRomInfo, macross2gRomName, NULL, NULL, CommonInputInfo, Macross2DIPInfo,
	Macross2Init, DrvExit, Macross2Frame, Macross2Draw, DrvScan, NULL, 0x400,
	384, 224, 4, 3
};

// Thunder Dragon 2 (9th Nov. 1993)

static struct BurnRomInfo tdragon2RomDesc[] = {
	{ "6.rom",			0x080000, 0xca348caf, 1 | BRF_PRG | BRF_ESS }, //  0 68k code

	{ "5.bin",			0x020000, 0xb870be61, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 code

	{ "1.bin",			0x020000, 0xd488aafa, 3 | BRF_GRA },           //  2 Characters

	{ "ww930914.2",		0x200000, 0xf968c65d, 4 | BRF_GRA },           //  3 Tiles

	{ "ww930917.7",		0x200000, 0xb98873cb, 5 | BRF_GRA },           //  4 Sprites
	{ "ww930918.8",		0x200000, 0xbaee84b2, 5 | BRF_GRA },           //  5

	{ "ww930916.4",		0x200000, 0x07c35fe6, 6 | BRF_SND },           //  6 OKI1 Samples

	{ "ww930915.3",		0x200000, 0x82025bab, 7 | BRF_SND },           //  7 OKI2 Samples

	{ "9.bpr",			0x000100, 0x435653a2, 0 | BRF_OPT },           //  8 Unused proms
	{ "10.bpr",			0x000100, 0xe6ead349, 0 | BRF_OPT },           //  9
};

STD_ROM_PICK(tdragon2)
STD_ROM_FN(tdragon2)

static INT32 Tdragon2Init()
{
	Tdragon2mode = 1;
	return Macross2Init();
}

struct BurnDriver BurnDrvTdragon2 = {
	"tdragon2", NULL, NULL, NULL, "1993",
	"Thunder Dragon 2 (9th Nov. 1993)\0", NULL, "NMK", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, tdragon2RomInfo, tdragon2RomName, NULL, NULL, Tdragon2InputInfo, Tdragon2DIPInfo,
	Tdragon2Init, DrvExit, Macross2Frame, Macross2Draw, DrvScan, NULL, 0x400,
	224, 384, 3, 4
};


// Thunder Dragon 2 (1st Oct. 1993)

static struct BurnRomInfo tdragon2aRomDesc[] = {
	{ "6.bin",			0x080000, 0x310d6bca, 1 | BRF_PRG | BRF_ESS }, //  0 68k code

	{ "5.bin",			0x020000, 0xb870be61, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 code

	{ "1.bin",			0x020000, 0xd488aafa, 3 | BRF_GRA },           //  2 Characters

	{ "ww930914.2",		0x200000, 0xf968c65d, 4 | BRF_GRA },           //  3 Tiles
	
	{ "ww930917.7",		0x200000, 0xb98873cb, 5 | BRF_GRA },           //  4 Sprites
	{ "ww930918.8",		0x200000, 0xbaee84b2, 5 | BRF_GRA },           //  5

	{ "ww930916.4",		0x200000, 0x07c35fe6, 6 | BRF_SND },           //  6 OKI1 Samples

	{ "ww930915.3",		0x200000, 0x82025bab, 7 | BRF_SND },           //  7 OKI2 Samples

	{ "9.bpr",			0x000100, 0x435653a2, 0 | BRF_OPT },           //  8 Unused proms
	{ "10.bpr",			0x000100, 0xe6ead349, 0 | BRF_OPT },           //  9
};

STD_ROM_PICK(tdragon2a)
STD_ROM_FN(tdragon2a)

struct BurnDriver BurnDrvTdragon2a = {
	"tdragon2a", "tdragon2", NULL, NULL, "1993",
	"Thunder Dragon 2 (1st Oct. 1993)\0", NULL, "NMK", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, tdragon2aRomInfo, tdragon2aRomName, NULL, NULL, Tdragon2InputInfo, Tdragon2DIPInfo,
	Tdragon2Init, DrvExit, Macross2Frame, Macross2Draw, DrvScan, NULL, 0x400,
	224, 384, 3, 4
};


// Big Bang (9th Nov. 1993)

static struct BurnRomInfo bigbangRomDesc[] = {
	{ "eprom.3",		0x080000, 0x28e5957a, 1 | BRF_PRG | BRF_ESS }, //  0 68k code

	{ "5.bin",			0x020000, 0xb870be61, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 code

	{ "1.bin",			0x020000, 0xd488aafa, 3 | BRF_GRA },           //  2 Characters

	{ "ww930914.2",		0x200000, 0xf968c65d, 4 | BRF_GRA },           //  3 Tiles

	{ "ww930917.7",		0x200000, 0xb98873cb, 5 | BRF_GRA },           //  4 Sprites
	{ "ww930918.8",		0x200000, 0xbaee84b2, 5 | BRF_GRA },           //  5

	{ "ww930916.4",		0x200000, 0x07c35fe6, 6 | BRF_SND },           //  6 OKI1 Samples

	{ "ww930915.3",		0x200000, 0x82025bab, 7 | BRF_SND },           //  7 OKI2 Samples

	{ "9.bpr",			0x000100, 0x435653a2, 0 | BRF_OPT },           //  8 Unused proms
	{ "10.bpr",			0x000100, 0xe6ead349, 0 | BRF_OPT },           //  9
};

STD_ROM_PICK(bigbang)
STD_ROM_FN(bigbang)

struct BurnDriver BurnDrvBigbang = {
	"bigbang", "tdragon2", NULL, NULL, "1993",
	"Big Bang (9th Nov. 1993)\0", NULL, "NMK", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, bigbangRomInfo, bigbangRomName, NULL, NULL, Tdragon2InputInfo, Tdragon2DIPInfo,
	Tdragon2Init, DrvExit, Macross2Frame, Macross2Draw, DrvScan, NULL, 0x400,
	224, 384, 3, 4
};


// Stagger I (Japan)

static struct BurnRomInfo stagger1RomDesc[] = {
	{ "2.bin",		0x020000, 0x8555929b, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "3.bin",		0x020000, 0x5b0b63ac, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "1.bin",		0x010000, 0x5d8cf28e, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 code

	{ "4.bin",		0x080000, 0x46463d36, 4 | BRF_GRA },           //  3 Tiles

	{ "7.bin",		0x080000, 0x048f7683, 5 | BRF_GRA },           //  4 Characters
	{ "6.bin",		0x080000, 0x051d4a77, 5 | BRF_GRA },           //  5

	{ "5",			0x040000, 0xe911ce33, 6 | BRF_SND },           //  6 OKI1 Samples
};

STD_ROM_PICK(stagger1)
STD_ROM_FN(stagger1)

static void pAfegaZ80Callback()
{
	ZetInit(0);
	ZetOpen(0);
	ZetMapArea(0x0000, 0xefff, 0, DrvZ80ROM);
	ZetMapArea(0x0000, 0xefff, 2, DrvZ80ROM);
	ZetMapArea(0xf000, 0xf7ff, 0, DrvZ80RAM);
	ZetMapArea(0xf000, 0xf7ff, 1, DrvZ80RAM);
	ZetMapArea(0xf000, 0xf7ff, 2, DrvZ80RAM);
	ZetSetWriteHandler(afega_sound_write);
	ZetSetReadHandler(afega_sound_read);
	ZetClose();
}

static INT32 Stagger1LoadCallback()
{
	if (BurnLoadRom(Drv68KROM  + 0x00001,  0, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x00000,  1, 2)) return 1;

	if (BurnLoadRom(DrvZ80ROM  + 0x00000,  2, 1)) return 1;

	memset (DrvGfxROM0, 0xff, 0x20);

	if (BurnLoadRom(DrvGfxROM1 + 0x00000,  3, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM2 + 0x00000,  4, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x00001,  5, 2)) return 1;

	if (BurnLoadRom(DrvSndROM0 + 0x00000,  6, 1)) return 1;

	DrvGfxDecode(0x20, 0x80000, 0x100000);

	return 0;
}

static INT32 Stagger1Init()
{
	return AfegaInit(Stagger1LoadCallback, pAfegaZ80Callback, 1);
}

struct BurnDriver BurnDrvStagger1 = {
	"stagger1", NULL, NULL, NULL, "1998",
	"Stagger I (Japan)\0", NULL, "Afega", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, stagger1RomInfo, stagger1RomName, NULL, NULL, CommonInputInfo, Stagger1DIPInfo,
	Stagger1Init, AfegaExit, AfegaFrame, AfegaDraw, DrvScan, NULL, 0x300,
	224, 256, 3, 4
};


// Red Hawk (US)

static struct BurnRomInfo redhawkRomDesc[] = {
	{ "2",			0x020000, 0x3ef5f326, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "3",			0x020000, 0x9b3a10ef, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "1.bin",		0x010000, 0x5d8cf28e, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 code

	{ "4",			0x080000, 0xd6427b8a, 4 | BRF_GRA },           //  5 Tiles

	{ "7",			0x080000, 0x66a8976d, 5 | BRF_GRA },           //  3 Characters
	{ "6",			0x080000, 0x61560164, 5 | BRF_GRA },           //  4

	{ "5",			0x040000, 0xe911ce33, 6 | BRF_GRA },           //  6 OKI1 Samples
};

STD_ROM_PICK(redhawk)
STD_ROM_FN(redhawk)

static INT32 RedhawkInit()
{
	INT32 nRet = AfegaInit(Stagger1LoadCallback, pAfegaZ80Callback, 1);

	if (nRet == 0) {
		decryptcode(0x40000, 16,15,14,17,13);
	} 

	return nRet;
}

struct BurnDriver BurnDrvRedhawk = {
	"redhawk", "stagger1", NULL, NULL, "1997",
	"Red Hawk (US)\0", NULL, "Afega (New Vision Ent. license)", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, redhawkRomInfo, redhawkRomName, NULL, NULL, CommonInputInfo, Stagger1DIPInfo,
	RedhawkInit, AfegaExit, AfegaFrame, AfegaDraw, DrvScan, NULL, 0x300,
	224, 256, 3, 4
};


// Red Hawk (Excellent Co., Ltd)

static struct BurnRomInfo redhawkeRomDesc[] = {
	{ "rhawk2.bin",		0x020000, 0x6d2e23b4, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "rhawk3.bin",		0x020000, 0x5e0d6188, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "1.bin",			0x010000, 0x5d8cf28e, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 code

	{ "rhawk4.bin",		0x080000, 0xd79aa288, 4 | BRF_GRA },           //  3 Tiles

	{ "rhawk7.bin",		0x080000, 0x0264ef54, 5 | BRF_GRA },           //  4 Characters
	{ "rhawk6.bin",		0x080000, 0x3f980ab6, 5 | BRF_GRA },           //  5

	{ "5",				0x040000, 0xe911ce33, 6 | BRF_GRA },           //  6 OKI1 Samples
};

STD_ROM_PICK(redhawke)
STD_ROM_FN(redhawke)

struct BurnDriver BurnDrvRedhawke = {
	"redhawke", "stagger1", NULL, NULL, "1997",
	"Red Hawk (Excellent Co., Ltd)\0", NULL, "Afega (Excellent Co. license)", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, redhawkeRomInfo, redhawkeRomName, NULL, NULL, CommonInputInfo, Stagger1DIPInfo,
	Stagger1Init, AfegaExit, AfegaFrame, AfegaDraw, DrvScan, NULL, 0x300,
	224, 256, 3, 4
};


// Red Hawk (Korea)

static struct BurnRomInfo redhawkkRomDesc[] = {
	{ "2",				0x020000, 0x8c02e81d, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "3",				0x020000, 0xab3597ee, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "1",				0x010000, 0x5d8cf28e, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 code

	{ "4",				0x080000, 0x6255d6a1, 4 | BRF_GRA },           //  3 Tiles

	{ "7",				0x080000, 0xf4fa8211, 5 | BRF_GRA },           //  4 Characters
	{ "6",				0x080000, 0x6a0b8224, 5 | BRF_GRA },           //  5

	{ "5",				0x040000, 0xe911ce33, 6 | BRF_GRA },           //  6 OKI1 Samples
};

STD_ROM_PICK(redhawkk)
STD_ROM_FN(redhawkk)

struct BurnDriver BurnDrvRedhawkk = {
	"redhawkk", "stagger1", NULL, NULL, "1997",
	"Red Hawk (Korea)\0", NULL, "Afega Co.", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, redhawkkRomInfo, redhawkkRomName, NULL, NULL, CommonInputInfo, Stagger1DIPInfo,
	Stagger1Init, AfegaExit, AfegaFrame, AfegaDraw, DrvScan, NULL, 0x300,
	224, 256, 3, 4
};


// Red Hawk (Italy)

static struct BurnRomInfo redhawkiRomDesc[] = {
	{ "rhit-2.bin",		0x020000, 0x30cade0e, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "rhit-3.bin",		0x020000, 0x37dbb3c2, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "1.bin",			0x010000, 0x5d8cf28e, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 code

	{ "rhit-4.bin",		0x080000, 0xaafb3cc4, 4 | BRF_GRA },           //  3 Tiles

	{ "rhit-7.bin",		0x080000, 0xbcb367c7, 5 | BRF_GRA },           //  4 Characters
	{ "rhit-6.bin",		0x080000, 0x7cbd5c60, 5 | BRF_GRA },           //  5

	{ "5",				0x040000, 0xe911ce33, 6 | BRF_SND },           //  6 OKI1 Samples
};

STD_ROM_PICK(redhawki)
STD_ROM_FN(redhawki)

static INT32 RedhawkiInit()
{
	INT32 nRet = AfegaInit(Stagger1LoadCallback, pAfegaZ80Callback, 1);

	if (nRet == 0) {
		decryptcode(0x40000, 15, 16, 17, 14, 13);
	}

	return nRet;
}

struct BurnDriver BurnDrvRedhawki = {
	"redhawki", "stagger1", NULL, NULL, "1997",
	"Red Hawk (Italy)\0", NULL, "Afega (Hea Dong Corp license)", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, redhawkiRomInfo, redhawkiRomName, NULL, NULL, CommonInputInfo, Stagger1DIPInfo,
	RedhawkiInit, AfegaExit, AfegaFrame, RedhawkiDraw, DrvScan, NULL, 0x300,
	256, 224, 4, 3
};


// Red Hawk (Greece)

static struct BurnRomInfo redhawkgRomDesc[] = {
	{ "2.bin",			0x020000, 0xccd459eb, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "3.bin",			0x020000, 0x483802fd, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "1.bin",			0x010000, 0x5d8cf28e, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 code

	{ "4.bin",			0x080000, 0xaafb3cc4, 4 | BRF_GRA },           //  3 Tiles

	{ "7.bin",			0x080000, 0xa28c8454, 5 | BRF_GRA },           //  4 Characters
	{ "6.bin",			0x080000, 0x710c9e3c, 5 | BRF_GRA },           //  5

	{ "5",				0x040000, 0xe911ce33, 6 | BRF_SND },           //  6 OKI1 Samples
};

STD_ROM_PICK(redhawkg)
STD_ROM_FN(redhawkg)

static INT32 RedhawkgLoadCallback()
{
	Stagger1LoadCallback();

	UINT8 *tmp = (UINT8*)BurnMalloc(0x40000);

	memcpy (tmp, Drv68KROM, 0x40000);

	for (INT32 i = 0; i < 0x40000; i+= 0x04000)
	{
		INT32 j = ((i & 0x30000) >> 2) | ((i & 0x04000) << 3) | ((i & 0x08000) << 1);

		memcpy (Drv68KROM + j, tmp + i, 0x04000);
	}

	BurnFree(tmp);

	return 0;
}

static INT32 RedhawkgInit()
{
	return AfegaInit(RedhawkgLoadCallback, pAfegaZ80Callback, 1);
}

struct BurnDriver BurnDrvRedhawkg = {
	"redhawkg", "stagger1", NULL, NULL, "1997",
	"Red Hawk (Greece)\0", NULL, "Afega (Hea Dong Corp license)", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, redhawkgRomInfo, redhawkgRomName, NULL, NULL, CommonInputInfo, Stagger1DIPInfo,
	RedhawkgInit, AfegaExit, AfegaFrame, RedhawkiDraw, DrvScan, NULL, 0x300,
	256, 224, 4, 3
};


// Vince (Redhawk bootleg)

static struct BurnRomInfo redhawkbRomDesc[] = {
	{ "rhb-1.bin",		0x020000, 0xe733ea07, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "rhb-2.bin",		0x020000, 0xf9fa5684, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "1.bin",			0x010000, 0x5d8cf28e, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 code

	{ "rhb-5.bin",		0x080000, 0xd0eaf6f2, 4 | BRF_GRA },           //  3 Tiles

	{ "rhb-3.bin",		0x080000, 0x0318d68b, 5 | BRF_GRA },           //  4 Characters
	{ "rhb-4.bin",		0x080000, 0xba21c1ef, 5 | BRF_GRA },           //  5

	{ "5",				0x040000, 0xe911ce33, 6 | BRF_SND },           //  6 OKI1 Samples
};

STD_ROM_PICK(redhawkb)
STD_ROM_FN(redhawkb)

static INT32 RedhawkbLoadCallback()
{
	if (BurnLoadRom(Drv68KROM  + 0x00001,  0, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x00000,  1, 2)) return 1;

	if (BurnLoadRom(DrvZ80ROM  + 0x00000,  2, 1)) return 1;

	memset (DrvGfxROM0, 0xff, 0x20);

	if (BurnLoadRom(DrvGfxROM1 + 0x00000,  3, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM2 + 0x00000,  4, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x80000,  5, 1)) return 1;

	if (BurnLoadRom(DrvSndROM0 + 0x00000,  6, 1)) return 1;

	DrvGfxDecode(0x20, 0x80000, 0x100000);

	BurnByteswap(DrvGfxROM1, 0x100000);
	BurnByteswap(DrvGfxROM2, 0x200000);

	return 0;
}

static INT32 RedhawkbInit()
{
	input_high[0] = input_high[1] = 0xffff;

	return AfegaInit(RedhawkbLoadCallback, pAfegaZ80Callback, 1);
}

struct BurnDriver BurnDrvRedhawkb = {
	"redhawkb", "stagger1", NULL, NULL, "1997",
	"Vince (Redhawk bootleg)\0", NULL, "bootleg", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, redhawkbRomInfo, redhawkbRomName, NULL, NULL, CommonInputInfo, RedhawkbDIPInfo,
	RedhawkbInit, AfegaExit, AfegaFrame, RedhawkbDraw, DrvScan, NULL, 0x300,
	256, 224, 4, 3
};


// Guardian Storm (horizontal, not encrypted)

static struct BurnRomInfo grdnstrmRomDesc[] = {
	{ "afega4.u112",		0x040000, 0x2244713a, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "afega5.u107",		0x040000, 0x5815c806, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "afega7.u92",			0x010000, 0x5d8cf28e, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 code

	{ "afega1.u4",			0x010000, 0x9e7ef086, 3 | BRF_GRA },           //  3 Characters

	{ "afega_af1-b2.uc8",	0x200000, 0xd68588c2, 4 | BRF_GRA },           //  4 Tiles
	{ "afega_af1-b1.uc3",	0x200000, 0xf8b200a8, 4 | BRF_GRA },           //  5

	{ "afega3.uc13",		0x200000, 0x0218017c, 5 | BRF_GRA },           //  6 Sprites

	{ "afega1.u95",			0x040000, 0xe911ce33, 6 | BRF_SND },           //  7 OKI1 Samples
};

STD_ROM_PICK(grdnstrm)
STD_ROM_FN(grdnstrm)

static INT32 GrdnstrmLoadCallback()
{
	if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;

	if (BurnLoadRom(DrvZ80ROM  + 0x000000,  2, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x000000,  3, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM1 + 0x000000,  4, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x200000,  5, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM2 + 0x000000,  6, 1)) return 1;

	if (BurnLoadRom(DrvSndROM0 + 0x000000,  7, 1)) return 1;

	GrdnstrmGfxDecode(0x10000, 0x400000, 0x200000);

	return 0;
}

static INT32 GrdnstrmInit()
{
	screen_flip_y = 1;

	return AfegaInit(GrdnstrmLoadCallback, pAfegaZ80Callback, 1);
}

struct BurnDriver BurnDrvGrdnstrm = {
	"grdnstrm", NULL, NULL, NULL, "1998",
	"Guardian Storm (horizontal, not encrypted)\0", NULL, "Afega (Apples Industries license)", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, grdnstrmRomInfo, grdnstrmRomName, NULL, NULL, CommonInputInfo, GrdnstrmDIPInfo,
	GrdnstrmInit, AfegaExit, AfegaFrame, FirehawkDraw, DrvScan, NULL, 0x300,
	256, 224, 4, 3
};


// Sen Jing - Guardian Storm (Japan)

static struct BurnRomInfo grdnstrmjRomDesc[] = {
	{ "afega_3.u112",		0x040000, 0xe51a35fb, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "afega_4.u107",		0x040000, 0xcb10aa54, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "afega7.u92",			0x010000, 0x5d8cf28e, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 code

	{ "gst-03.u4",			0x010000, 0xa1347297, 3 | BRF_GRA },           //  3 Characters

	{ "afega_af1-b2.uc8",	0x200000, 0xd68588c2, 4 | BRF_GRA },           //  4 Tiles
	{ "afega_af1-b1.uc3",	0x200000, 0xf8b200a8, 4 | BRF_GRA },           //  5

	{ "afega_af1-sp.uc13",	0x200000, 0x7d4d4985, 5 | BRF_GRA },           //  6 Sprites

	{ "afega1.u95",			0x040000, 0xe911ce33, 6 | BRF_SND },           //  7 OKI1 Samples
};

STD_ROM_PICK(grdnstrmj)
STD_ROM_FN(grdnstrmj)

static INT32 GrdnstrmjInit()
{
	INT32 nRet = AfegaInit(GrdnstrmLoadCallback, pAfegaZ80Callback, 1);

	if (nRet == 0) {
		decryptcode(0x80000, 13, 17, 16, 15, 14);
		decryptcode(0x80000, 17, 16, 14, 15, 13);
		decryptcode(0x80000, 17, 15, 16, 14, 13);
		decryptcode(0x80000, 16, 17, 15, 14, 13);
	}

	return nRet;
}

struct BurnDriver BurnDrvGrdnstrmj = {
	"grdnstrmj", "grdnstrm", NULL, NULL, "1998",
	"Sen Jing - Guardian Storm (Japan)\0", NULL, "Afega", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, grdnstrmjRomInfo, grdnstrmjRomName, NULL, NULL, CommonInputInfo, GrdnstrkDIPInfo,
	GrdnstrmjInit, AfegaExit, AfegaFrame, AfegaDraw, DrvScan, NULL, 0x300,
	224, 256, 3, 4
};


// Jeon Sin - Guardian Storm (Korea)

static struct BurnRomInfo grdnstrmkRomDesc[] = {
	{ "gst-04.u112",		0x040000, 0x922c931a, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "gst-05.u107",		0x040000, 0xd22ca2dc, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "afega7.u92",			0x010000, 0x5d8cf28e, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 code

	{ "gst-03.u4",			0x010000, 0xa1347297, 3 | BRF_GRA },           //  3 Characters

	{ "afega_af1-b2.uc8",	0x200000, 0xd68588c2, 4 | BRF_GRA },           //  4 Tiles
	{ "afega_af1-b1.uc3",	0x200000, 0xf8b200a8, 4 | BRF_GRA },           //  5

	{ "afega_af1-sp.uc13",	0x200000, 0x7d4d4985, 5 | BRF_GRA },           //  6 Sprites

	{ "afega1.u95",			0x040000, 0xe911ce33, 6 | BRF_SND },           //  7 OKI1 Samples
};

STD_ROM_PICK(grdnstrmk)
STD_ROM_FN(grdnstrmk)

static INT32 GrdnstrmkInit()
{
	INT32 nRet = AfegaInit(GrdnstrmLoadCallback, pAfegaZ80Callback, 1);

	if (nRet == 0) {
		decryptcode(0x80000, 16,17,14,15,13);
	}

	return nRet;
}

struct BurnDriver BurnDrvGrdnstrmk = {
	"grdnstrmk", "grdnstrm", NULL, NULL, "1998",
	"Jeon Sin - Guardian Storm (Korea)\0", NULL, "Afega", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, grdnstrmkRomInfo, grdnstrmkRomName, NULL, NULL, CommonInputInfo, GrdnstrkDIPInfo,
	GrdnstrmkInit, AfegaExit, AfegaFrame, AfegaDraw, DrvScan, NULL, 0x300,
	224, 256, 3, 4
};


// Guardian Storm (vertical)

static struct BurnRomInfo grdnstrmvRomDesc[] = {
	{ "afega2.u112",	    0x040000, 0x16d41050, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "afega3.u107",		0x040000, 0x05920a99, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "afega7.u92",		    0x010000, 0x5d8cf28e, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 code

	{ "afega1.u4",			0x010000, 0x9e7ef086, 3 | BRF_GRA },           //  3 Characters

	{ "afega_af1-b2.uc8",	0x200000, 0xd68588c2, 4 | BRF_GRA },           //  4 Tiles
	{ "afega_af1-b1.uc3",	0x200000, 0xf8b200a8, 4 | BRF_GRA },           //  5

	{ "afega6.uc13",	    0x200000, 0x9b54ff84, 5 | BRF_GRA },           //  6 Sprites

	{ "afega1.u95",		    0x040000, 0xe911ce33, 6 | BRF_SND },           //  7 OKI1 Samples
};

STD_ROM_PICK(grdnstrmv)
STD_ROM_FN(grdnstrmv)

struct BurnDriver BurnDrvGrdnstrmv = {
	"grdnstrmv", "grdnstrm", NULL, NULL, "1998",
	"Guardian Storm (vertical)\0", NULL, "Afega (Apples Industries license)", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, grdnstrmvRomInfo, grdnstrmvRomName, NULL, NULL, CommonInputInfo, GrdnstrkDIPInfo,
	GrdnstrmkInit, AfegaExit, AfegaFrame, AfegaDraw, DrvScan, NULL, 0x300,
	224, 256, 3, 4
};


// Guardian Storm (Germany)

static struct BurnRomInfo grdnstrmgRomDesc[] = {
	{ "gs6_c2.uc9",         0x040000, 0xea363e4d, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
    { "gs5_c1.uc1",         0x040000, 0xc0263e4a, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "gs1_s1.uc14",		0x010000, 0x5d8cf28e, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 code

	{ "gs3_t1.uc2",			0x010000, 0x88c423ef, 3 | BRF_GRA },           //  3 Characters

	{ "gs10_cr5.uc15",	    0x080000, 0x2c8c23e3, 4 | BRF_GRA },           //  4 Tiles
	{ "gs4_cr7.uc19",	    0x080000, 0xc3f6c908, 4 | BRF_GRA },           //  5
	{ "gs8_cr1.uc6",	    0x080000, 0xdc0125f0, 4 | BRF_GRA },           //  6
	{ "gs9_cr3.uc12",	    0x080000, 0xd8a0636b, 4 | BRF_GRA },           //  7
	
	{ "gs8_br3.uc10",		0x080000, 0x7b42a57a, 5 | BRF_GRA },           //  8 Sprites
	{ "gs7_br1.uc3",		0x080000, 0xe6794265, 5 | BRF_GRA },           //  9
	{ "gs10_br4.uc11",		0x080000, 0x1d3b57e1, 5 | BRF_GRA },           // 10
	{ "gs9_br2.uc4",		0x080000, 0x4d2c220b, 5 | BRF_GRA },           // 11

	{ "gs2_s2.uc18",		0x040000, 0xe911ce33, 6 | BRF_SND },           // 12 OKI1 Samples
};

STD_ROM_PICK(grdnstrmg)
STD_ROM_FN(grdnstrmg)

static INT32 GrdnstrmgLoadCallback()
{
	if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;

	if (BurnLoadRom(DrvZ80ROM  + 0x000000,  2, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x000000,  3, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM1 + 0x000000,  4, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x080000,  5, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x100000,  6, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x180000,  7, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM2 + 0x000001,  8, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x000000,  9, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x100001, 10, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x100000, 11, 2)) return 1;

	if (BurnLoadRom(DrvSndROM0 + 0x000000, 12, 1)) return 1;

	GrdnstrmGfxDecode(0x10000, 0x400000, 0x200000);

	return 0;
}

static INT32 GrdnstrmgInit()
{
	INT32 nRet = AfegaInit(GrdnstrmgLoadCallback, pAfegaZ80Callback, 1);

	if (nRet == 0) {
		decryptcode(0x80000, 13, 17, 16, 15, 14);
		decryptcode(0x80000, 17, 16, 14, 15, 13);
		decryptcode(0x80000, 17, 15, 16, 14, 13);
		decryptcode(0x80000, 16, 17, 15, 14, 13);
	}

	return nRet;
}

struct BurnDriver BurnDrvGrdnstrmg = {
	"grdnstrmg", "grdnstrm", NULL, NULL, "1998",
	"Guardian Storm (Germany)\0", NULL, "Afega", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, grdnstrmgRomInfo, grdnstrmgRomName, NULL, NULL, CommonInputInfo, GrdnstrkDIPInfo,
	GrdnstrmgInit, AfegaExit, AfegaFrame, AfegaDraw, DrvScan, NULL, 0x300,
	224, 256, 3, 4
};


// Red Fox War Planes II (China, set 1)

static struct BurnRomInfo redfoxwp2RomDesc[] = {
	{ "u112",	        	0x040000, 0x3f31600b, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "u107",				0x040000, 0xdaa44ab4, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "u92",		    	0x010000, 0x864b55c2, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 code

	{ "u4",					0x010000, 0x19239401, 3 | BRF_GRA },           //  3 Characters

	{ "afega_af1-b2.uc8",	0x200000, 0xd68588c2, 4 | BRF_GRA },           //  4 Tiles
	{ "afega_af1-b1.uc3",	0x200000, 0xf8b200a8, 4 | BRF_GRA },           //  5

	{ "afega_af1-sp.uc13",	0x200000, 0x7d4d4985, 5 | BRF_GRA },           //  6 Sprites

	{ "afega1.u95",			0x040000, 0xe911ce33, 6 | BRF_SND },           //  7 OKI1 Samples
};

STD_ROM_PICK(redfoxwp2)
STD_ROM_FN(redfoxwp2)

struct BurnDriver BurnDrvRedfoxwp2 = {
	"redfoxwp2", "grdnstrm", NULL, NULL, "1998",
	"Red Fox War Planes II (China, set 1)\0", NULL, "Afega", "NMK16",
	L"\u7D05\u5B64\u6230\u6A5FII\0Red Fox War Planes II (China, set 1)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, redfoxwp2RomInfo, redfoxwp2RomName, NULL, NULL, CommonInputInfo, GrdnstrkDIPInfo,
	GrdnstrmkInit, AfegaExit, AfegaFrame, AfegaDraw, DrvScan, NULL, 0x300,
	224, 256, 3, 4
};


// Red Fox War Planes II (China, set 2)

static struct BurnRomInfo redfoxwp2aRomDesc[] = {
	{ "afega_4.u112",		0x040000, 0xe6e6682a, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "afega_5.u107",		0x040000, 0x2faa2ed6, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "afega_1.u92",		0x010000, 0x5d8cf28e, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 code

	{ "afega_3.u4",			0x010000, 0x64608687, 3 | BRF_GRA },           //  3 Characters

	{ "afega_af1-b2.uc8",	0x200000, 0xd68588c2, 4 | BRF_GRA },           //  4 Tiles
	{ "afega_af1-b1.uc3",	0x200000, 0xf8b200a8, 4 | BRF_GRA },           //  5

	{ "afega_af1-sp.uc13",	0x200000, 0x7d4d4985, 5 | BRF_GRA },           //  6 Sprites

	{ "afega_2.u95",		0x040000, 0xe911ce33, 6 | BRF_SND },           //  7 OKI1 Samples
};

STD_ROM_PICK(redfoxwp2a)
STD_ROM_FN(redfoxwp2a)

static INT32 Redfoxwp2Init()
{
	INT32 nRet = AfegaInit(GrdnstrmLoadCallback, pAfegaZ80Callback, 1);

	if (nRet == 0) {
		decryptcode(0x80000, 17, 16, 13, 15, 14);
		decryptcode(0x80000, 17, 16, 14, 15, 13);
		decryptcode(0x80000, 16, 17, 15, 14, 13);
	}

	return nRet;
}

struct BurnDriver BurnDrvRedfoxwp2a = {
	"redfoxwp2a", "grdnstrm", NULL, NULL, "1998",
	"Red Fox War Planes II (China, set 2)\0", NULL, "Afega", "NMK16",
	L"\u7D05\u5B64\u6230\u6A5FII\0Red Fox War Planes II (China, set 2)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, redfoxwp2aRomInfo, redfoxwp2aRomName, NULL, NULL, CommonInputInfo, GrdnstrkDIPInfo,
	Redfoxwp2Init, AfegaExit, AfegaFrame, AfegaDraw, DrvScan, NULL, 0x300,
	224, 256, 3, 4
};


// Pop's Pop's

static struct BurnRomInfo popspopsRomDesc[] = {
	{ "afega4.u112",	0x040000, 0xdb191762, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "afega5.u107",	0x040000, 0x17e0c48b, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "afega1.u92",		0x010000, 0x5d8cf28e, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 code

	{ "afega3.u4",		0x010000, 0xf39dd5d2, 3 | BRF_GRA },           //  3 Characters

	{ "afega6.uc8",		0x200000, 0x6d506c97, 4 | BRF_GRA },           //  4 Tiles
	{ "afega7.uc3",		0x200000, 0x02d7f9de, 4 | BRF_GRA },           //  5

	{ "afega2.u95",		0x040000, 0xecd8eeac, 6 | BRF_SND },           //  6 OKI1 Samples
};

STD_ROM_PICK(popspops)
STD_ROM_FN(popspops)

static INT32 PopspopsLoadCallback()
{
	if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;

	if (BurnLoadRom(DrvZ80ROM  + 0x000000,  2, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x000000,  3, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM1 + 0x000000,  4, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x200000,  5, 1)) return 1;

	memset (DrvGfxROM2, 0xff, 0x80);

	if (BurnLoadRom(DrvSndROM0 + 0x000000,  6, 1)) return 1;

	GrdnstrmGfxDecode(0x10000, 0x400000, 0x80);

	return 0;
}

static INT32 PopspopsInit()
{
	INT32 nRet = AfegaInit(PopspopsLoadCallback, pAfegaZ80Callback, 1);

	if (nRet == 0) {
		decryptcode(0x80000, 16,17,14,15,13);
	}

	return nRet;
}

struct BurnDriver BurnDrvPopspops = {
	"popspops", NULL, NULL, NULL, "1999",
	"Pop's Pop's\0", NULL, "Afega", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, popspopsRomInfo, popspopsRomName, NULL, NULL, CommonInputInfo, PopspopsDIPInfo,
	PopspopsInit, AfegaExit, AfegaFrame, Bubl2000Draw, DrvScan, NULL, 0x300,
	256, 224, 4, 3
};


// Bubble 2000

static struct BurnRomInfo bubl2000RomDesc[] = {
	{ "rom10.112",		0x020000, 0x87f960d7, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "rom11.107",		0x020000, 0xb386041a, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "rom01.92",		0x010000, 0x5d8cf28e, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 code

	{ "rom03.4",		0x010000, 0xf4c15588, 3 | BRF_GRA },           //  3 Characters

	{ "rom06.6",		0x080000, 0xac1aabf5, 4 | BRF_GRA },           //  4 Tiles
	{ "rom07.9",		0x080000, 0x69aff769, 4 | BRF_GRA },           //  5
	{ "rom13.7",		0x080000, 0x3a5b7226, 4 | BRF_GRA },           //  6
	{ "rom04.1",		0x080000, 0x46acd054, 4 | BRF_GRA },           //  7
	{ "rom05.3",		0x080000, 0x37deb6a1, 4 | BRF_GRA },           //  8
	{ "rom12.2",		0x080000, 0x1fdc59dd, 4 | BRF_GRA },           //  9

	{ "rom08.11",		0x040000, 0x519dfd82, 5 | BRF_GRA },           // 10 Sprites
	{ "rom09.14",		0x040000, 0x04fcb5c6, 5 | BRF_GRA },           // 11

	{ "rom02.95",		0x040000, 0x859a86e5, 6 | BRF_SND },           // 12 OKI1 Samples
};

STD_ROM_PICK(bubl2000)
STD_ROM_FN(bubl2000)

static INT32 Bubl2000LoadCallback()
{
	if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;

	if (BurnLoadRom(DrvZ80ROM  + 0x000000,  2, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x000000,  3, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM1 + 0x000000,  4, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x080000,  5, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x100000,  6, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x200000,  7, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x280000,  8, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x300000,  9, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM2 + 0x000000, 10, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x000001, 11, 2)) return 1;

	if (BurnLoadRom(DrvSndROM0 + 0x000000, 12, 1)) return 1;

	GrdnstrmGfxDecode(0x10000, 0x400000, 0x80000);

	return 0;
}

static INT32 Bubl2000Init()
{
	INT32 nRet = AfegaInit(Bubl2000LoadCallback, pAfegaZ80Callback, 1);

	if (nRet == 0) {
		decryptcode(0x40000, 13,14,15,16,17);
	}

	return nRet;
}

struct BurnDriver BurnDrvBubl2000 = {
	"bubl2000", NULL, NULL, NULL, "1998",
	"Bubble 2000\0", NULL, "Afega (Tuning license)", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, bubl2000RomInfo, bubl2000RomName, NULL, NULL, CommonInputInfo, Bubl2000DIPInfo,
	Bubl2000Init, AfegaExit, AfegaFrame, Bubl2000Draw, DrvScan, NULL, 0x300,
	256, 224, 4, 3
};


// Hot Bubble (Korea, with adult pictures)
/* Korean release - Nude images of women for backgrounds */

static struct BurnRomInfo hotbublRomDesc[] = {
	{ "c2.uc9",			0x040000, 0x7917b95d, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "c1.uc1",			0x040000, 0x7bb240e9, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "s1.uc14",		0x010000, 0x5d8cf28e, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 code

	{ "t1.uc2",			0x010000, 0xce683a93, 3 | BRF_GRA },           //  3 Characters

	{ "cr5.uc15",		0x080000, 0x65bd5159, 4 | BRF_GRA },           //  4 Tiles
	{ "cr7.uc19",		0x080000, 0xa89d9ce4, 4 | BRF_GRA },           //  5
	{ "cr6.uc16",		0x080000, 0x99d6523c, 4 | BRF_GRA },           //  6
	{ "cr1.uc6",		0x080000, 0xfc9101d2, 4 | BRF_GRA },           //  7
	{ "cr3.uc12",		0x080000, 0xc841a4f6, 4 | BRF_GRA },           //  8
	{ "cr2.uc7",		0x080000, 0x27ad6fc8, 4 | BRF_GRA },           //  9

	{ "br1.uc3",		0x080000, 0x6fc18de4, 5 | BRF_GRA },           // 10 Sprites
	{ "br3.uc10",		0x080000, 0xbb677240, 5 | BRF_GRA },           // 11

	{ "s2.uc18",		0x040000, 0x401c980f, 6 | BRF_SND },           // 12 OKI1 Samples
};

STD_ROM_PICK(hotbubl)
STD_ROM_FN(hotbubl)

struct BurnDriver BurnDrvHotbubl = {
	"hotbubl", "bubl2000", NULL, NULL, "1998",
	"Hot Bubble (Korea, with adult pictures)\0", NULL, "Afega (Pandora license)", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, hotbublRomInfo, hotbublRomName, NULL, NULL, CommonInputInfo, Bubl2000DIPInfo,
	Bubl2000Init, AfegaExit, AfegaFrame, Bubl2000Draw, DrvScan, NULL, 0x300,
	256, 224, 4, 3
};


// Hot Bubble (Korea)
/* Korean release - Nude images replaced with pictures of satellite dishes */

static struct BurnRomInfo hotbublaRomDesc[] = {
	{ "7_c2.uc9",		0x040000, 0x74eb11c3, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "6_c1.uc1",		0x040000, 0x7c65bf47, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "1_s1.uc14",		0x010000, 0x5d8cf28e, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 code

	{ "2_t1.uc2",		0x010000, 0xce683a93, 3 | BRF_GRA },           //  3 Characters

	{ "2_cr5.uc15",		0x080000, 0xdd7e92de, 4 | BRF_GRA },           //  4 Tiles
	{ "5_cr7.uc19",		0x080000, 0xd293f1d0, 4 | BRF_GRA },           //  5
	{ "5_cr6.uc16",		0x080000, 0x324429c5, 4 | BRF_GRA },           //  6
	{ "8_cr1.uc6",		0x080000, 0x7e2840b4, 4 | BRF_GRA },           //  7
	{ "10_cr3.uc12",	0x080000, 0x312c38d8, 4 | BRF_GRA },           //  8
	{ "9_cr2.uc7",		0x080000, 0xc5516087, 4 | BRF_GRA },           //  9

	{ "8_br1.uc3",		0x040000, 0x7e132eff, 5 | BRF_GRA },           // 10 Sprites
	{ "9_br3.uc10",		0x040000, 0x22707728, 5 | BRF_GRA },           // 11

	{ "1_s2.uc18",		0x040000, 0x401c980f, 6 | BRF_SND },           // 12 OKI1 Samples
};

STD_ROM_PICK(hotbubla)
STD_ROM_FN(hotbubla)

struct BurnDriver BurnDrvHotbubla = {
	"hotbubla", "bubl2000", NULL, NULL, "1998",
	"Hot Bubble (Korea)\0", NULL, "Afega (Pandora license)", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, hotbublaRomInfo, hotbublaRomName, NULL, NULL, CommonInputInfo, Bubl2000DIPInfo,
	Bubl2000Init, AfegaExit, AfegaFrame, Bubl2000Draw, DrvScan, NULL, 0x300,
	256, 224, 4, 3
};


// Mang-Chi

static struct BurnRomInfo mangchiRomDesc[] = {
	{ "afega9.u112",	0x040000, 0x0b1517a5, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "afega10.u107",	0x040000, 0xb1d0f33d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "sound.u92",		0x010000, 0xbec4f9aa, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 code

	{ "afega5.uc6",		0x080000, 0xc73261e0, 4 | BRF_GRA },           //  3 Tiles
	{ "afega4.uc1",		0x080000, 0x73940917, 4 | BRF_GRA },           //  4

	{ "afega6.uc11",	0x040000, 0x979efc30, 5 | BRF_GRA },           //  5 Sprites
	{ "afega7.uc14",	0x040000, 0xc5cbcc38, 5 | BRF_GRA },           //  6

	{ "afega2.u95",		0x040000, 0x78c8c1f9, 6 | BRF_SND },           //  7 OKI1 Samples
};

STD_ROM_PICK(mangchi)
STD_ROM_FN(mangchi)

static INT32 MangchiLoadCallback()
{
	if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;

	if (BurnLoadRom(DrvZ80ROM  + 0x000000,  2, 1)) return 1;

	memset (DrvGfxROM0, 0xff, 0x20);

	if (BurnLoadRom(DrvGfxROM1 + 0x000000,  3, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x080000,  4, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM2 + 0x000000,  5, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x000001,  6, 2)) return 1;

	if (BurnLoadRom(DrvSndROM0 + 0x000000,  7, 1)) return 1;

	GrdnstrmGfxDecode(0x20, 0x100000, 0x80000);

	return 0;
}

static INT32 MangchiInit()
{
	INT32 nRet = AfegaInit(MangchiLoadCallback, pAfegaZ80Callback, 1);

	if (nRet == 0) {
		decryptcode(0x80000, 13,14,15,16,17);
	}

	return nRet;
}

struct BurnDriver BurnDrvMangchi = {
	"mangchi", NULL, NULL, NULL, "2000",
	"Mang-Chi\0", NULL, "Afega", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, mangchiRomInfo, mangchiRomName, NULL, NULL, CommonInputInfo, MangchiDIPInfo,
	MangchiInit, AfegaExit, AfegaFrame, Bubl2000Draw, DrvScan, NULL, 0x300,
	256, 224, 4, 3
};


// Spectrum 2000 (vertical)

static struct BurnRomInfo spec2kRomDesc[] = {
	{ "u124",		0x040000, 0xdbd6f65d, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "u120",		0x040000, 0xbe53e243, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "u103",		0x010000, 0xf4e4fb10, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 code


	{ "u3",			0x020000, 0x921503b8, 3 | BRF_GRA },           //  3 Characters

	{ "uc3",		0x200000, 0x1d087122, 4 | BRF_GRA },           //  4 Tiles
	{ "uc2",		0x200000, 0x998dc05c, 4 | BRF_GRA },           //  5

	{ "uc1",		0x200000, 0x3139a213, 5 | BRF_GRA },           //  6 Sprites


	{ "u101",		0x040000, 0xd16aaaad, 6 | BRF_SND },           //  7 OKI1 Samples

	{ "u106",		0x080000, 0x65d61f3a, 7 | BRF_SND },           //  8 OKI2 Samples
};

STD_ROM_PICK(spec2k)
STD_ROM_FN(spec2k)

static void pFirehawkZ80Callback()
{
	ZetInit(0);
	ZetOpen(0);
	ZetMapArea(0x0000, 0xefff, 0, DrvZ80ROM);
	ZetMapArea(0x0000, 0xefff, 2, DrvZ80ROM);
	ZetMapArea(0xf000, 0xfeff, 0, DrvZ80RAM);
	ZetMapArea(0xf000, 0xfeff, 1, DrvZ80RAM);
	ZetMapArea(0xf000, 0xfeff, 2, DrvZ80RAM);
	ZetSetWriteHandler(firehawk_sound_write);
	ZetSetReadHandler(firehawk_sound_read);
	ZetClose();
}

static INT32 Spec2kLoadCallback()
{
	if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;

	if (BurnLoadRom(DrvZ80ROM  + 0x000000,  2, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x000000,  3, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM1 + 0x000000,  4, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x200000,  5, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM2 + 0x000000,  6, 1)) return 1;

	if (BurnLoadRom(DrvSndROM0 + 0x000000,  7, 1)) return 1;

	if (BurnLoadRom(DrvSndROM1 + 0x040000,  8, 1)) return 1;

	GrdnstrmGfxDecode(0x20000, 0x400000, 0x200000);

	return 0;
}

static INT32 Spec2kInit()
{
	Macrossmode = 1; // use macross1 text draw
	INT32 nRet = AfegaInit(Spec2kLoadCallback, pFirehawkZ80Callback, 1);

	if (nRet == 0) {
		decryptcode(0x80000, 17,13,14,15,16);
	}

	return nRet;
}

struct BurnDriver BurnDrvSpec2k = {
	"spec2k", NULL, NULL, NULL, "2000",
	"Spectrum 2000 (vertical)\0", NULL, "Yona Tech", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, spec2kRomInfo, spec2kRomName, NULL, NULL, CommonInputInfo, Spec2kDIPInfo,
	Spec2kInit, AfegaExit, AfegaFrame, FirehawkDraw, DrvScan, NULL, 0x300,
	224, 256, 3, 4
};


// Spectrum 2000 (horizontal, buggy) (Europe)

static struct BurnRomInfo spec2khRomDesc[] = {
	{ "yonatech5.u124",	0x040000, 0x72ab5c05, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "yonatech6.u120",	0x040000, 0x7e44bd9c, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "yonatech1.u103",	0x010000, 0xef5acda7, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 code

	{ "yonatech4.u3",	0x020000, 0x5626b08e, 3 | BRF_GRA },           //  3 Characters

	{ "u153.bin",		0x200000, 0xa00bbf8f, 4 | BRF_GRA },           //  4 Tiles
	{ "u152.bin",		0x200000, 0xf6423fab, 4 | BRF_GRA },           //  5

	{ "u154.bin",		0x200000, 0xf77b764e, 5 | BRF_GRA },           //  6 Sprites

	{ "yonatech2.u101",	0x020000, 0x4160f172, 6 | BRF_SND },           //  7 OKI1 Samples

	{ "yonatech3.u106",	0x080000, 0x6644c404, 7 | BRF_SND },           //  8 OKI2 Samples
};

STD_ROM_PICK(spec2kh)
STD_ROM_FN(spec2kh)

static INT32 Spec2khInit()
{
	screen_flip_y = 1;

	INT32 nRet = AfegaInit(Spec2kLoadCallback, pFirehawkZ80Callback, 1);

	if (nRet == 0) {
		decryptcode(0x80000, 17,13,14,15,16);
	}

	return nRet;
}

struct BurnDriver BurnDrvSpec2kh = {
	"spec2kh", "spec2k", NULL, NULL, "2000",
	"Spectrum 2000 (horizontal, buggy) (Europe)\0", NULL, "Yona Tech", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, spec2khRomInfo, spec2khRomName, NULL, NULL, CommonInputInfo, Spec2kDIPInfo,
	Spec2khInit, AfegaExit, AfegaFrame, FirehawkDraw, DrvScan, NULL, 0x300,
	256, 224, 4, 3
};


// Fire Hawk (horizontal)

static struct BurnRomInfo firehawkRomDesc[] = {
	{ "fhawk_p1.u59",	0x080000, 0xd6d71a50, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "fhawk_p2.u60",	0x080000, 0x9f35d245, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "fhawk_s1.u40",	0x020000, 0xc6609c39, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 code

	{ "fhawk_g1.uc6",	0x200000, 0x2ab0b06b, 4 | BRF_GRA },           //  3 Tiles
	{ "fhawk_g2.uc5",	0x200000, 0xd11bfa20, 4 | BRF_GRA },           //  4

	{ "fhawk_g3.uc2",	0x200000, 0xcae72ff4, 5 | BRF_GRA },           //  5 Sprites

	{ "fhawk_s2.u36",	0x040000, 0xd16aaaad, 6 | BRF_SND },           //  6 OKI1 Samples

	{ "fhawk_s3.u41",	0x040000, 0x3fdcfac2, 7 | BRF_SND },           //  7 OKI2 Samples
};

STD_ROM_PICK(firehawk)
STD_ROM_FN(firehawk)

static INT32 FirehawkLoadCallback()
{
	if (BurnLoadRom(Drv68KROM  + 0x000000,  0, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x000001,  1, 2)) return 1;

	if (BurnLoadRom(DrvZ80ROM  + 0x000000,  2, 1)) return 1;

	memset (DrvGfxROM0, 0xff, 0x20);

	if (BurnLoadRom(DrvGfxROM1 + 0x000000,  3, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x200000,  4, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM2 + 0x000000,  5, 1)) return 1;

	if (BurnLoadRom(DrvSndROM0 + 0x000000,  6, 1)) return 1;

	if (BurnLoadRom(DrvSndROM1 + 0x040000,  7, 1)) return 1;

	GrdnstrmGfxDecode(0x20, 0x400000, 0x200000);

	return 0;
}

static INT32 FirehawkInit()
{
	screen_flip_y = 1;

	return AfegaInit(FirehawkLoadCallback, pFirehawkZ80Callback, 1);
}

struct BurnDriver BurnDrvFirehawk = {
	"firehawk", "spec2k", NULL, NULL, "2001",
	"Fire Hawk (horizontal)\0", NULL, "ESD", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, firehawkRomInfo, firehawkRomName, NULL, NULL, CommonInputInfo, FirehawkDIPInfo,
	FirehawkInit, AfegaExit, AfegaFrame, FirehawkDraw, DrvScan, NULL, 0x300,
	256, 224, 4, 3
};


// Twin Action

static struct BurnRomInfo twinactnRomDesc[] = {
	{ "afega.uj13",		0x020000, 0x9187701d, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "afega.uj12",		0x020000, 0xfe8cff9c, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "afega.su6",		0x008000, 0x3a52dc88, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 code

	{ "afega.uj11",		0x020000, 0x3f439e92, 3 | BRF_GRA },           //  3 Characters

	{ "afega.ui20",		0x080000, 0x237c8f92, 4 | BRF_GRA },           //  4 Tiles

	{ "afega.ub11",		0x080000, 0x287f20d8, 5 | BRF_GRA },           //  5 Sprites
	{ "afega.ub13",		0x080000, 0xf525f819, 5 | BRF_GRA },           //  6

	{ "afega.su12",		0x020000, 0x91d665f3, 6 | BRF_SND },           //  7 OKI1 Samples
	{ "afega.su13",		0x040000, 0x30e1c306, 6 | BRF_SND },           //  8
};

STD_ROM_PICK(twinactn)
STD_ROM_FN(twinactn)

static void Twinactn68kInit()
{
	SekInit(0, 0x68000);	
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(DrvPalRAM,		0x088000, 0x0887ff, MAP_RAM);
	//SekMapMemory(DrvScrollRAM,	0x08c000, 0x08c3ff, MAP_WRITE);
	SekMapMemory(DrvBgRAM0,		0x090000, 0x093fff, MAP_RAM);
	SekMapMemory(DrvTxRAM,		0x09c000, 0x09c7ff, MAP_RAM);
	SekMapMemory(Drv68KRAM,		0x0f0000, 0x0fffff, MAP_ROM);
	SekSetWriteWordHandler(0,	afega_main_write_word);
	SekSetWriteByteHandler(0,	afega_main_write_byte);
	SekSetReadWordHandler(0,	afega_main_read_word);
	SekSetReadByteHandler(0,	afega_main_read_byte);
	SekClose();
}

static INT32 TwinactnLoadCallback()
{
	{
		if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;
	
		if (BurnLoadRom(DrvZ80ROM  + 0x000000,  2, 1)) return 1;
	
		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  3, 1)) return 1;
	
		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  4, 1)) return 1;
	
		if (BurnLoadRom(DrvGfxROM2 + 0x000000,  5, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x000001,  6, 2)) return 1;

		if (BurnLoadRom(DrvSndROM1 + 0x0a0000,  8, 1)) return 1;
		memcpy (DrvSndROM1 + 0xe0000, DrvSndROM1 + 0xc0000, 0x20000);

		if (BurnLoadRom(DrvSndROM1 + 0x000000,  7, 1)) return 1;
		memcpy (DrvSndROM1 + 0x40000, DrvSndROM1 + 0x00000, 0x20000);
		memcpy (DrvSndROM1 + 0x80000, DrvSndROM1 + 0x00000, 0x20000);
		memcpy (DrvSndROM1 + 0xc0000, DrvSndROM1 + 0x00000, 0x20000);
		memcpy (DrvSndROM1 + 0x20000, DrvSndROM1 + 0x00000, 0x20000);
		memcpy (DrvSndROM1 + 0x60000, DrvSndROM1 + 0x00000, 0x20000);

		DrvGfxDecode(0x20000, 0x80000, 0x100000);
	}

	Twinactn68kInit();

	return 0;
}

static INT32 TwinactnInit()
{
	input_high[0] = 0x0000;
	input_high[1] = 0x8080;
	nNMK004EnableIrq2 = 1;

	return MSM6295x1Init(TwinactnLoadCallback);
}

struct BurnDriver BurnDrvTwinactn = {
	"twinactn", NULL, NULL, NULL, "1995",
	"Twin Action\0", NULL, "Afega", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_HORSHOOT, 0,
	NULL, twinactnRomInfo, twinactnRomName, NULL, NULL, CommonInputInfo, TwinactnDIPInfo,
	TwinactnInit, MSM6295x1Exit, SsmissinFrame, MustangDraw, DrvScan, NULL, 0x400,
	256, 224, 4, 3
};


// Dolmen
/* Original source of the caveman concept for Bubble 2000 / Hot Bubble, much earlier and completely different hardware */

static struct BurnRomInfo dolmenRomDesc[] = {
	{ "afega8.uj3",		0x20000, 0xf1b73e4c, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "afega7.uj2",		0x20000, 0xc91bda0b, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "afega1.su6",		0x08000, 0x166b53cb, 2 | BRF_PRG | BRF_ESS }, //  2 z80 code

	{ "afega6.uj11",	0x20000, 0x13fa4415, 3 | BRF_GRA },           //  3 Characters

	{ "afega9.ui20",	0x80000, 0xb3fa7be6, 4 | BRF_GRA },           //  4 Tiles

	{ "afega4.ub11",	0x80000, 0x5a259393, 5 | BRF_GRA },           //  5 Sprites
	{ "afega5.ub13",	0x80000, 0x7f6a683d, 6 | BRF_GRA },           //  6

	{ "afega2.su12",	0x20000, 0x1a2ce1c2, 6 | BRF_SND },           //  7 OKI1 Samples
	{ "afega3.su13",	0x40000, 0xd3531018, 6 | BRF_SND },           //  8
};

STD_ROM_PICK(dolmen)
STD_ROM_FN(dolmen)

static INT32 DolmenLoadCallback()
{
	{
		if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;
	
		if (BurnLoadRom(DrvZ80ROM  + 0x000000,  2, 1)) return 1;
	
		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  3, 1)) return 1;
	
		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  4, 1)) return 1;
	
		if (BurnLoadRom(DrvGfxROM2 + 0x000000,  5, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x000001,  6, 2)) return 1;

		if (BurnLoadRom(DrvSndROM1 + 0x0a0000,  8, 1)) return 1;
		memcpy (DrvSndROM1 + 0xe0000, DrvSndROM1 + 0xc0000, 0x20000);

		if (BurnLoadRom(DrvSndROM1 + 0x000000,  7, 1)) return 1;
		memcpy (DrvSndROM1 + 0x40000, DrvSndROM1 + 0x00000, 0x20000);
		memcpy (DrvSndROM1 + 0x80000, DrvSndROM1 + 0x00000, 0x20000);
		memcpy (DrvSndROM1 + 0xc0000, DrvSndROM1 + 0x00000, 0x20000);
		memcpy (DrvSndROM1 + 0x20000, DrvSndROM1 + 0x00000, 0x20000);
		memcpy (DrvSndROM1 + 0x60000, DrvSndROM1 + 0x00000, 0x20000);

		DrvGfxDecode(0x20000, 0x80000, 0x100000);
	}

	Twinactn68kInit();

	return 0;
}

static INT32 DolmenInit()
{
	nNMK004EnableIrq2 = 1;

	return MSM6295x1Init(DolmenLoadCallback);
}

struct BurnDriver BurnDrvDolmen = {
	"dolmen", NULL, NULL, NULL, "1995",
	"Dolmen\0", NULL, "Afega", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, dolmenRomInfo, dolmenRomName, NULL, NULL, DolmenInputInfo, DolmenDIPInfo,
	DolmenInit, MSM6295x1Exit, SsmissinFrame, MacrossDraw, DrvScan, NULL, 0x400,
	256, 224, 4, 3
};


// Saboten Bombers (set 1)

static struct BurnRomInfo sabotenbRomDesc[] = {
	{ "ic76.sb1",		0x040000, 0xb2b0b2cf, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "ic75.sb2",		0x040000, 0x367e87b7, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ic35.sb3",		0x010000, 0xeb7bc99d, 3 | BRF_GRA },           //  2 Characters

	{ "ic32.sb4",		0x200000, 0x24c62205, 4 | BRF_GRA },           //  3 Tiles

	{ "ic100.sb5",		0x200000, 0xb20f166e, 5 | BRF_GRA },           //  4 Sprites

	{ "ic30.sb6",		0x100000, 0x288407af, 6 | BRF_SND },           //  5 OKI1 Samples

	{ "ic27.sb7",		0x100000, 0x43e33a7e, 7 | BRF_SND },           //  6 OKI2 Samples
};

STD_ROM_PICK(sabotenb)
STD_ROM_FN(sabotenb)

static INT32 SabotenbLoadCallback()
{
	if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x000000,  2, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM1 + 0x000000,  3, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM2 + 0x000000,  4, 1)) return 1;
	BurnByteswap(DrvGfxROM2, 0x200000);

	if (BurnLoadRom(DrvSndROM0 + 0x000000,  5, 1)) return 1;

	if (BurnLoadRom(DrvSndROM1 + 0x000000,  6, 1)) return 1;

	decode_gfx(0x200000, 0x200000);

	BjtwinGfxDecode(0x10000, 0x200000, 0x200000);

	return 0;
}

static INT32 SabotenbInit()
{
	return BjtwinInit(SabotenbLoadCallback);
}

struct BurnDriver BurnDrvSabotenb = {
	"sabotenb", NULL, NULL, NULL, "1992",
	"Saboten Bombers (set 1)\0", NULL, "NMK / Tecmo", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PLATFORM, 0,
	NULL, sabotenbRomInfo, sabotenbRomName, NULL, NULL, CommonInputInfo, SabotenbDIPInfo,
	SabotenbInit, BjtwinExit, BjtwinFrame, BjtwinDraw, DrvScan, NULL, 0x400,
	384, 224, 4, 3
};


// Saboten Bombers (set 2)

static struct BurnRomInfo sabotenbaRomDesc[] = {
	{ "sb1.76",		0x040000, 0xdf6f65e2, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "sb2.75",		0x040000, 0x0d2c1ab8, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ic35.sb3",	0x010000, 0xeb7bc99d, 3 | BRF_GRA },           //  2 Characters

	{ "ic32.sb4",	0x200000, 0x24c62205, 4 | BRF_GRA },           //  3 Tiles

	{ "ic100.sb5",	0x200000, 0xb20f166e, 5 | BRF_GRA },           //  4 Sprites

	{ "ic30.sb6",	0x100000, 0x288407af, 6 | BRF_SND },           //  5 OKI1 Samples

	{ "ic27.sb7",	0x100000, 0x43e33a7e, 7 | BRF_SND },           //  6 OKI2 Samples
};

STD_ROM_PICK(sabotenba)
STD_ROM_FN(sabotenba)

struct BurnDriver BurnDrvSabotenba = {
	"sabotenba", "sabotenb", NULL, NULL, "1992",
	"Saboten Bombers (set 2)\0", NULL, "NMK / Tecmo", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PLATFORM, 0,
	NULL, sabotenbaRomInfo, sabotenbaRomName, NULL, NULL, CommonInputInfo, SabotenbDIPInfo,
	SabotenbInit, BjtwinExit, BjtwinFrame, BjtwinDraw, DrvScan, NULL, 0x400,
	384, 224, 4, 3
};


// Cactus (bootleg of Saboten Bombers)

static struct BurnRomInfo cactusRomDesc[] = {
	{ "02.bin",		0x040000, 0x15b2ff2f, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "01.bin",		0x040000, 0x5b8ba46a, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "i03.bin",	0x010000, 0xeb7bc99d, 3 | BRF_GRA },           //  2 Characters

	{ "s-05.bin",	0x100000, 0xfce962b9, 4 | BRF_GRA },           //  3 Tiles
	{ "s-06.bin",	0x100000, 0x16768fbc, 4 | BRF_GRA },           //  4

	{ "s-03.bin",	0x100000, 0xbc1781b8, 5 | BRF_GRA },           //  5 Sprites
	{ "s-04.bin",	0x100000, 0xf823885e, 5 | BRF_GRA },           //  6

	{ "s-01.bin",	0x100000, 0x288407af, 6 | BRF_SND },           //  7 OKI1 Samples

	{ "s-02.bin",	0x100000, 0x43e33a7e, 7 | BRF_SND },           //  8 OKI2 Samples
};

STD_ROM_PICK(cactus)
STD_ROM_FN(cactus)

static INT32 CactusLoadCallback()
{
	if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x000000,  2, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM1 + 0x000000,  3, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x100000,  4, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM2 + 0x000001,  5, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x000000,  6, 2)) return 1;

	if (BurnLoadRom(DrvSndROM0 + 0x000000,  7, 1)) return 1;

	if (BurnLoadRom(DrvSndROM1 + 0x000000,  8, 1)) return 1;

	decode_gfx(0x200000, 0x200000);

	BjtwinGfxDecode(0x10000, 0x200000, 0x200000);

	return 0;
}

static INT32 CactusInit()
{
	return BjtwinInit(CactusLoadCallback);
}

struct BurnDriver BurnDrvCactus = {
	"cactus", "sabotenb", NULL, NULL, "1992",
	"Cactus (bootleg of Saboten Bombers)\0", NULL, "bootleg", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PLATFORM, 0,
	NULL, cactusRomInfo, cactusRomName, NULL, NULL, CommonInputInfo, SabotenbDIPInfo,
	CactusInit, BjtwinExit, BjtwinFrame, BjtwinDraw, DrvScan, NULL, 0x400,
	384, 224, 4, 3
};


// Bombjack Twin (set 1)

static struct BurnRomInfo bjtwinRomDesc[] = {
	{ "93087-1.bin",	0x020000, 0x93c84e2d, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "93087-2.bin",	0x020000, 0x30ff678a, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "93087-3.bin",	0x010000, 0xaa13df7c, 3 | BRF_GRA },           //  2 Characters

	{ "93087-4.bin",	0x100000, 0x8a4f26d0, 4 | BRF_GRA },           //  3 Tiles

	{ "93087-5.bin",	0x100000, 0xbb06245d, 5 | BRF_GRA },           //  4 Sprites

	{ "93087-6.bin",	0x100000, 0x372d46dd, 6 | BRF_SND },           //  5 OKI1 Samples

	{ "93087-7.bin",	0x100000, 0x8da67808, 7 | BRF_SND },           //  6 OKI2 Samples

	{ "8.bpr",			0x000100, 0x633ab1c9, 0 | BRF_OPT },           //  7 Unused proms
	{ "9.bpr",			0x000100, 0x435653a2, 0 | BRF_OPT },           //  8
};

STD_ROM_PICK(bjtwin)
STD_ROM_FN(bjtwin)

static INT32 BjtwinLoadCallback()
{
	if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x000000,  2, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM1 + 0x000000,  3, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM2 + 0x000000,  4, 1)) return 1;
	BurnByteswap(DrvGfxROM2, 0x100000);

	if (BurnLoadRom(DrvSndROM0 + 0x000000,  5, 1)) return 1;

	if (BurnLoadRom(DrvSndROM1 + 0x000000,  6, 1)) return 1;

	decode_gfx(0x100000, 0x100000);

	BjtwinGfxDecode(0x10000, 0x100000, 0x100000);

	return 0;
}

static INT32 BjtwinGameInit()
{
	return BjtwinInit(BjtwinLoadCallback);
}

struct BurnDriver BurnDrvBjtwin = {
	"bjtwin", NULL, NULL, NULL, "1993",
	"Bombjack Twin (set 1)\0", NULL, "NMK", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_PLATFORM, 0,
	NULL, bjtwinRomInfo, bjtwinRomName, NULL, NULL, CommonInputInfo, BjtwinDIPInfo,
	BjtwinGameInit, BjtwinExit, BjtwinFrame, BjtwinDraw, DrvScan, NULL, 0x400,
	224, 384, 3, 4
};


// Bombjack Twin (set 2)

static struct BurnRomInfo bjtwinaRomDesc[] = {
	{ "93087.1",		0x020000, 0xc82b3d8e, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "93087.2",		0x020000, 0x9be1ec47, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "93087-3.bin",	0x010000, 0xaa13df7c, 3 | BRF_GRA },           //  2 Characters

	{ "93087-4.bin",	0x100000, 0x8a4f26d0, 4 | BRF_GRA },           //  3 Tiles

	{ "93087-5.bin",	0x100000, 0xbb06245d, 5 | BRF_GRA },           //  4 Sprites

	{ "93087-6.bin",	0x100000, 0x372d46dd, 6 | BRF_SND },           //  5 OKI1 Samples

	{ "93087-7.bin",	0x100000, 0x8da67808, 7 | BRF_SND },           //  6 OKI2 Samples

	{ "8.bpr",			0x000100, 0x633ab1c9, 0 | BRF_OPT },           //  7 Unused proms
	{ "9.bpr",			0x000100, 0x435653a2, 0 | BRF_OPT },           //  8
};

STD_ROM_PICK(bjtwina)
STD_ROM_FN(bjtwina)

struct BurnDriver BurnDrvBjtwina = {
	"bjtwina", "bjtwin", NULL, NULL, "1993",
	"Bombjack Twin (set 2)\0", NULL, "NMK", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_PLATFORM, 0,
	NULL, bjtwinaRomInfo, bjtwinaRomName, NULL, NULL, CommonInputInfo, BjtwinDIPInfo,
	BjtwinGameInit, BjtwinExit, BjtwinFrame, BjtwinDraw, DrvScan, NULL, 0x400,
	224, 384, 3, 4
};


// Nouryoku Koujou Iinkai

static struct BurnRomInfo nouryokuRomDesc[] = {
	{ "ic76.1",		0x040000, 0x26075988, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "ic75.2",		0x040000, 0x75ab82cd, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ic35.3",		0x010000, 0x03d0c3b1, 3 | BRF_GRA },           //  2 Characters

	{ "ic32.4",		0x200000, 0x88d454fd, 4 | BRF_GRA },           //  3 Tiles

	{ "ic100.5",	0x200000, 0x24d3e24e, 5 | BRF_GRA },           //  4 Sprites

	{ "ic30.6",		0x100000, 0xfeea34f4, 6 | BRF_SND },           //  5 OKI1 Samples

	{ "ic27.7",		0x100000, 0x8a69fded, 7 | BRF_SND },           //  6 OKI2 Samples
};

STD_ROM_PICK(nouryoku)
STD_ROM_FN(nouryoku)

static INT32 NouryokuLoadCallback()
{
	if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x000000,  2, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM1 + 0x000000,  3, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM2 + 0x000000,  4, 1)) return 1;
	BurnByteswap(DrvGfxROM2, 0x200000);

	if (BurnLoadRom(DrvSndROM0 + 0x000000,  5, 1)) return 1;

	if (BurnLoadRom(DrvSndROM1 + 0x000000,  6, 1)) return 1;

	decode_gfx(0x200000, 0x200000);

	BjtwinGfxDecode(0x10000, 0x200000, 0x200000);

	return 0;
}

static INT32 NouryokuGameInit()
{
	return BjtwinInit(NouryokuLoadCallback);
}

struct BurnDriver BurnDrvNouryoku = {
	"nouryoku", NULL, NULL, NULL, "1995",
	"Nouryoku Koujou Iinkai\0", NULL, "Tecmo", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_QUIZ, 0,
	NULL, nouryokuRomInfo, nouryokuRomName, NULL, NULL, CommonInputInfo, NouryokuDIPInfo,
	NouryokuGameInit, BjtwinExit, BjtwinFrame, BjtwinDraw, DrvScan, NULL, 0x400,
	384, 224, 4, 3
};


// US AAF Mustang (25th May. 1990)

static struct BurnRomInfo mustangRomDesc[] = {
	{ "2.bin",		0x020000, 0xbd9f7c89, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "3.bin",		0x020000, 0x0eec36a5, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "90058-7",	0x010000, 0x920a93c8, 2 | BRF_PRG | BRF_ESS }, //  2 NMK004 data

	{ "90058-1",	0x020000, 0x81ccfcad, 3 | BRF_GRA },           //  3 Characters

	{ "90058-4",	0x080000, 0xa07a2002, 4 | BRF_GRA },           //  4 Tiles

	{ "90058-8",	0x080000, 0x560bff04, 5 | BRF_GRA },           //  5 Sprites
	{ "90058-9",	0x080000, 0xb9d72a03, 5 | BRF_GRA },           //  6

	{ "90058-5",	0x080000, 0xc60c883e, 6 | BRF_SND },           //  7 OKI1 Samples

	{ "90058-6",	0x080000, 0x233c1776, 7 | BRF_SND },           //  8 OKI2 Samples

	{ "10.bpr",		0x000100, 0x633ab1c9, 0 | BRF_OPT },           //  9 Unused proms
	{ "90058-11",	0x000100, 0xcfdbb86c, 0 | BRF_OPT },           // 10
};

STDROMPICKEXT(mustang, mustang, nmk004)
STD_ROM_FN(mustang)

static INT32 MustangLoadCallback()
{
	{
		if (BurnLoadRom(Drv68KROM  + 0x00001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x00000,  1, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x00000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000,  5, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x00001,  6, 2)) return 1;

		if (BurnLoadRom(DrvSndROM0 + 0x00000,  7, 1)) return 1;
		memmove (DrvSndROM0 + 0x040000, DrvSndROM0 + 0x20000, 0x60000);

		if (BurnLoadRom(DrvSndROM1 + 0x00000,  8, 1)) return 1;
		memmove (DrvSndROM1 + 0x040000, DrvSndROM1 + 0x20000, 0x60000);

		DrvGfxDecode(0x20000, 0x80000, 0x100000);
	}

	SekInit(0, 0x68000);	
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(DrvPalRAM,		0x088000, 0x0887ff, MAP_RAM);
	//SekMapMemory(DrvScrollRAM,	0x08c000, 0x08c3ff, MAP_WRITE);
	SekMapMemory(DrvBgRAM0,		0x090000, 0x093fff, MAP_RAM);
	SekMapMemory(DrvTxRAM,		0x09c000, 0x09c7ff, MAP_RAM);
	SekMapMemory(Drv68KRAM,		0x0f0000, 0x0fffff, MAP_ROM);
	SekSetWriteWordHandler(0,	mustang_main_write_word);
	SekSetWriteByteHandler(0,	mustang_main_write_byte);
	SekSetReadWordHandler(0,	mustang_main_read_word);
	SekSetReadByteHandler(0,	mustang_main_read_byte);
	SekClose();

	return 0;
}

static INT32 MustangInit()
{
	return NMK004Init(MustangLoadCallback, 10000000);
}

struct BurnDriver BurnDrvMustang = {
	"mustang", NULL, "nmk004", NULL, "1990",
	"US AAF Mustang (25th May. 1990)\0", NULL, "UPL", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_HORSHOOT, 0,
	NULL, mustangRomInfo, mustangRomName, NULL, NULL, CommonInputInfo, MustangDIPInfo,
	MustangInit, NMK004Exit, NMK004Frame, MustangDraw, DrvScan, NULL, 0x400,
	256, 224, 4, 3
};


// US AAF Mustang (25th May. 1990 / Seoul Trading)

static struct BurnRomInfo mustangsRomDesc[] = {
	{ "90058-2",	0x020000, 0x833aa458, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "90058-3",	0x020000, 0xe4b80f06, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "90058-7",	0x010000, 0x920a93c8, 2 | BRF_PRG | BRF_ESS }, //  2 NMK004 data

	{ "90058-1",	0x020000, 0x81ccfcad, 3 | BRF_GRA },           //  3 Characters

	{ "90058-4",	0x080000, 0xa07a2002, 4 | BRF_GRA },           //  4 Tiles

	{ "90058-8",	0x080000, 0x560bff04, 5 | BRF_GRA },           //  5 Sprites
	{ "90058-9",	0x080000, 0xb9d72a03, 5 | BRF_GRA },           //  6

	{ "90058-5",	0x080000, 0xc60c883e, 6 | BRF_SND },           //  7 OKI1 Samples

	{ "90058-6",	0x080000, 0x233c1776, 7 | BRF_SND },           //  8 OKI2 Samples

	{ "90058-10",	0x000100, 0xde156d99, 0 | BRF_OPT },           //  9 Unused proms
	{ "90058-11",	0x000100, 0xcfdbb86c, 0 | BRF_OPT },           // 10
};

STDROMPICKEXT(mustangs, mustangs, nmk004)
STD_ROM_FN(mustangs)

struct BurnDriver BurnDrvMustangs = {
	"mustangs", "mustang", "nmk004", NULL, "1990",
	"US AAF Mustang (25th May. 1990 / Seoul Trading)\0", NULL, "UPL (Seoul Trading license)", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_HORSHOOT, 0,
	NULL, mustangsRomInfo, mustangsRomName, NULL, NULL, CommonInputInfo, MustangDIPInfo,
	MustangInit, NMK004Exit, NMK004Frame, MustangDraw, DrvScan, NULL, 0x400,
	256, 224, 4, 3
};


// US AAF Mustang (bootleg)

static struct BurnRomInfo mustangbRomDesc[] = {
	{ "mustang.14",		0x020000, 0x13c6363b, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "mustang.13",		0x020000, 0xd8ccce31, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "mustang.16",		0x010000, 0x99ee7505, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code
	
	{ "90058-1",		0x020000, 0x81ccfcad, 3 | BRF_GRA },           //  3 Characters

	{ "90058-4",		0x080000, 0xa07a2002, 4 | BRF_GRA },           //  4 Tiles

	{ "90058-8",		0x080000, 0x560bff04, 5 | BRF_GRA },           //  5 Sprites
	{ "90058-9",		0x080000, 0xb9d72a03, 5 | BRF_GRA },           //  6

	{ "mustang.17",		0x010000, 0xf6f6c4bf, 6 | BRF_SND },           //  7 oki
};

STD_ROM_PICK(mustangb)
STD_ROM_FN(mustangb)

static INT32 MustangbLoadCallback()
{
	if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;

	if (BurnLoadRom(DrvZ80ROM  + 0x000000,  2, 1)) return 1;
	memcpy (DrvZ80ROM + 0x10000, DrvZ80ROM + 0x8000, 0x8000);
	memcpy (DrvZ80ROM + 0x18000, DrvZ80ROM + 0x0000, 0x8000);

	if (BurnLoadRom(DrvGfxROM0 + 0x000000,  3, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM1 + 0x000000,  4, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM2 + 0x000000,  5, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x000001,  6, 2)) return 1;

	if (BurnLoadRom(DrvSndROM0 + 0x000000,  7, 1)) return 1;

	DrvGfxDecode(0x20000, 0x80000, 0x100000);

	return 0;
}

static INT32 MustangbInit()
{
	return SeibuSoundInit(MustangbLoadCallback, 1);
}

struct BurnDriver BurnDrvMustangb = {
	"mustangb", "mustang", NULL, NULL, "1990",
	"US AAF Mustang (bootleg)\0", NULL, "bootleg", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_HORSHOOT, 0,
	NULL, mustangbRomInfo, mustangbRomName, NULL, NULL, CommonInputInfo, MustangDIPInfo,
	MustangbInit, SeibuSoundExit, SeibuSoundFrame, MustangDraw, DrvScan, NULL, 0x400,
	256, 224, 4, 3
};


// US AAF Mustang (TAB Austria bootleg)

static struct BurnRomInfo mustangb2RomDesc[] = {
	{ "05.bin",		0x020000, 0x13c6363b, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "04.bin",		0x020000, 0x0d06f723, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "01.bin",		0x010000, 0x90820499, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 code

	{ "06.bin",		0x020000, 0x81ccfcad, 3 | BRF_GRA },           //  3 Characters

	{ "07.bin",		0x020000, 0x5f8fdfb1, 4 | BRF_GRA },           //  4 Tiles
	{ "10.bin",		0x020000, 0x39757d6a, 4 | BRF_GRA },           //  5
	{ "08.bin",		0x020000, 0xb3dd5243, 4 | BRF_GRA },           //  6
	{ "09.bin",		0x020000, 0xc6c9752f, 4 | BRF_GRA },           //  7

	{ "18.bin",		0x020000, 0xd13f0722, 5 | BRF_GRA },           //  8 Sprites
	{ "13.bin",		0x020000, 0x54773f95, 5 | BRF_GRA },           //  9
	{ "17.bin",		0x020000, 0x87c1fb43, 5 | BRF_GRA },           // 10
	{ "14.bin",		0x020000, 0x932d3e33, 5 | BRF_GRA },           // 11
	{ "16.bin",		0x020000, 0x23d03ad5, 5 | BRF_GRA },           // 12
	{ "15.bin",		0x020000, 0xa62b2f87, 5 | BRF_GRA },           // 13
	{ "12.bin",		0x020000, 0x42a6cfc2, 5 | BRF_GRA },           // 14
	{ "11.bin",		0x020000, 0x9d3bee66, 5 | BRF_GRA },           // 15

	{ "02.bin",		0x010000, 0xf6f6c4bf, 6 | BRF_SND },           // 16 oki
};

STD_ROM_PICK(mustangb2)
STD_ROM_FN(mustangb2)

static INT32 Mustangb2LoadCallback()
{
	if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;

	if (BurnLoadRom(DrvZ80ROM  + 0x000000,  2, 1)) return 1;
	memcpy (DrvZ80ROM + 0x10000, DrvZ80ROM + 0x8000, 0x8000);
	memcpy (DrvZ80ROM + 0x18000, DrvZ80ROM + 0x8000, 0x8000);

	if (BurnLoadRom(DrvGfxROM0 + 0x000000,  3, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM1 + 0x000000,  4, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x020000,  5, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x040000,  6, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x060000,  7, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM2 + 0x000000,  8, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x000001,  9, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x040000, 10, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x040001, 11, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x080000, 12, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x080001, 13, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x0c0000, 14, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x0c0001, 15, 2)) return 1;

	if (BurnLoadRom(DrvSndROM0 + 0x000000, 16, 1)) return 1;

	DrvGfxDecode(0x20000, 0x80000, 0x100000);

	return 0;
}

static INT32 Mustangb2Init()
{
	return SeibuSoundInit(Mustangb2LoadCallback, 1);
}

struct BurnDriver BurnDrvMustangb2 = {
	"mustangb2", "mustang", NULL, NULL, "1990",
	"US AAF Mustang (TAB Austria bootleg)\0", NULL, "bootleg", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_HORSHOOT, 0,
	NULL, mustangb2RomInfo, mustangb2RomName, NULL, NULL, CommonInputInfo, MustangDIPInfo,
	Mustangb2Init, SeibuSoundExit, SeibuSoundFrame, MustangDraw, DrvScan, NULL, 0x400,
	256, 224, 4, 3
};


// Thunder Dragon (9th Jan. 1992)

static struct BurnRomInfo tdragonRomDesc[] = {
	{ "91070_68k.8",	0x020000, 0x121c3ae7, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "91070_68k.7",	0x020000, 0x6e154d8e, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "91070.1",		0x010000, 0xbf493d74, 2 | BRF_PRG | BRF_ESS }, //  2 NMK004 data

	{ "91070.6",		0x020000, 0xfe365920, 3 | BRF_GRA },           //  3 Characters

	{ "91070.5",		0x100000, 0xd0bde826, 4 | BRF_GRA },           //  4 Tiles

	{ "91070.4",		0x100000, 0x3eedc2fe, 5 | BRF_GRA },           //  5 Sprites

	{ "91070.3",		0x080000, 0xae6875a8, 6 | BRF_SND },           //  6 OKI1 Samples

	{ "91070.2",		0x080000, 0xecfea43e, 7 | BRF_SND },           //  7 OKI2 Samples

	{ "91070.9",		0x000100, 0xcfdbb86c, 0 | BRF_OPT },           //  8 Unused proms
	{ "91070.10",		0x000100, 0xe6ead349, 0 | BRF_OPT },           //  9
};

STDROMPICKEXT(tdragon, tdragon, nmk004)
STD_ROM_FN(tdragon)

static INT32 TdragonLoadCallback()
{
	{
		if (BurnLoadRom(Drv68KROM  + 0x00001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x00000,  1, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x00000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000,  5, 1)) return 1;
		BurnByteswap(DrvGfxROM2, 0x100000);

		if (BurnLoadRom(DrvSndROM0 + 0x20000,  6, 1)) return 1;
		memcpy (DrvSndROM0 + 0x000000, DrvSndROM0 + 0x20000, 0x20000);

		if (BurnLoadRom(DrvSndROM1 + 0x20000,  7, 1)) return 1;
		memcpy (DrvSndROM1 + 0x000000, DrvSndROM1 + 0x20000, 0x20000);

		DrvGfxDecode(0x20000, 0x100000, 0x100000);
	}

	{
		*((UINT16*)(Drv68KROM + 0x048a)) = BURN_ENDIAN_SWAP_INT16(0x4e71);
		*((UINT16*)(Drv68KROM + 0x04aa)) = BURN_ENDIAN_SWAP_INT16(0x4e71);
	}

	SekInit(0, 0x68000);	
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,		0x0b0000, 0x0bffff, MAP_ROM);
	SekMapMemory(DrvScrollRAM,	0x0c4000, 0x0c43ff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		0x0c8000, 0x0c87ff, MAP_RAM);
	SekMapMemory(DrvBgRAM0,		0x0cc000, 0x0cffff, MAP_RAM);
	SekMapMemory(DrvTxRAM,		0x0d0000, 0x0d07ff, MAP_RAM);
	SekSetWriteWordHandler(0,	tdragon_main_write_word);
	SekSetWriteByteHandler(0,	tdragon_main_write_byte);
	SekSetReadWordHandler(0,	tdragon_main_read_word);
	SekSetReadByteHandler(0,	tdragon_main_read_byte);
	SekClose();

	return 0;
}

static INT32 TdragonInit()
{
	return NMK004Init(TdragonLoadCallback, 8000000);
}

struct BurnDriver BurnDrvTdragon = {
	"tdragon", NULL, "nmk004", NULL, "1991",
	"Thunder Dragon (9th Jan. 1992)\0", NULL, "NMK (Tecmo license)", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, tdragonRomInfo, tdragonRomName, NULL, NULL, CommonInputInfo, TdragonDIPInfo,
	TdragonInit, NMK004Exit, NMK004Frame, HachamfDraw, DrvScan, NULL, 0x400,
	224, 256, 3, 4
};


// Thunder Dragon (4th Jun. 1991)

static struct BurnRomInfo tdragon1RomDesc[] = {
	{ "thund.8",		0x020000, 0xedd02831, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "thund.7",		0x020000, 0x52192fe5, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "91070.1",		0x010000, 0xbf493d74, 2 | BRF_PRG | BRF_ESS }, //  2 NMK004 data

	{ "91070.6",		0x020000, 0xfe365920, 3 | BRF_GRA },           //  3 Characters

	{ "91070.5",		0x100000, 0xd0bde826, 4 | BRF_GRA },           //  4 Tiles

	{ "91070.4",		0x100000, 0x3eedc2fe, 5 | BRF_GRA },           //  5 Sprites

	{ "91070.3",		0x080000, 0xae6875a8, 6 | BRF_SND },           //  6 OKI1 Samples

	{ "91070.2",		0x080000, 0xecfea43e, 7 | BRF_SND },           //  7 OKI2 Samples

	{ "91070.9",		0x000100, 0xcfdbb86c, 0 | BRF_OPT },           //  8 Unused proms
	{ "91070.10",		0x000100, 0xe6ead349, 0 | BRF_OPT },           //  9
};

STDROMPICKEXT(tdragon1, tdragon1, nmk004)
STD_ROM_FN(tdragon1)

struct BurnDriver BurnDrvTdragon1 = {
	"tdragon1", "tdragon", "nmk004", NULL, "1991",
	"Thunder Dragon (4th Jun. 1991)\0", NULL, "NMK (Tecmo license)", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, tdragon1RomInfo, tdragon1RomName, NULL, NULL, CommonInputInfo, TdragonDIPInfo,
	TdragonInit, NMK004Exit, NMK004Frame, HachamfDraw, DrvScan, NULL, 0x400,
	224, 256, 3, 4
};


// Thunder Dragon (bootleg)

static struct BurnRomInfo tdragonbRomDesc[] = {
	{ "td_04.bin",		0x020000, 0xe8a62d3e, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "td_03.bin",		0x020000, 0x2fa1aa04, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "td_02.bin",		0x010000, 0x99ee7505, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 code

	{ "td_08.bin",		0x020000, 0x5144dc69, 3 | BRF_GRA },           //  3 Characters

	{ "td_06.bin",		0x080000, 0xc1be8a4d, 4 | BRF_GRA },           //  4 Tiles
	{ "td_07.bin",		0x080000, 0x2c3e371f, 4 | BRF_GRA },           //  5

	{ "td_10.bin",		0x080000, 0xbfd0ec5d, 5 | BRF_GRA },           //  6 Sprites
	{ "td_09.bin",		0x080000, 0xb6e074eb, 5 | BRF_GRA },           //  7

	{ "td_01.bin",		0x010000, 0xf6f6c4bf, 6 | BRF_SND },           //  8 OKI1 Samples
};

STD_ROM_PICK(tdragonb)
STD_ROM_FN(tdragonb)

static INT32 TdragonbLoadCallback()
{
	if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;

	if (BurnLoadRom(DrvZ80ROM  + 0x000000,  2, 1)) return 1;
	memcpy (DrvZ80ROM + 0x10000, DrvZ80ROM + 0x8000, 0x8000);
	memcpy (DrvZ80ROM + 0x18000, DrvZ80ROM + 0x0000, 0x8000);

	if (BurnLoadRom(DrvGfxROM0 + 0x000000,  3, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM1 + 0x000000,  4, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x080000,  5, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM2 + 0x000000,  6, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x000001,  7, 2)) return 1;

	if (BurnLoadRom(DrvSndROM0 + 0x000000,  8, 1)) return 1;

	decode_tdragonb();
	DrvGfxDecode(0x20000, 0x100000, 0x100000);

	return 0;
}

static INT32 TdragonbInit()
{
	return SeibuSoundInit(TdragonbLoadCallback, 0);
}

struct BurnDriver BurnDrvTdragonb = {
	"tdragonb", "tdragon", NULL, NULL, "1991",
	"Thunder Dragon (bootleg)\0", NULL, "bootleg", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, tdragonbRomInfo, tdragonbRomName, NULL, NULL, CommonInputInfo, TdragonbDIPInfo,
	TdragonbInit, SeibuSoundExit, SeibuSoundFrame, MacrossDraw, DrvScan, NULL, 0x400,
	224, 256, 3, 4
};



// Acrobat Mission

static struct BurnRomInfo acrobatmRomDesc[] = {
	{ "02_ic100.bin",	0x020000, 0x3fe487f4, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "01_ic101.bin",	0x020000, 0x17175753, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "04_ic74.bin",	0x010000, 0x176905fb, 2 | BRF_PRG | BRF_ESS }, //  2 NMK004 data

	{ "03_ic79.bin",	0x010000, 0xd86c186e, 3 | BRF_GRA },           //  3 Characters

	{ "09_ic8.bin",		0x100000, 0x7c12afed, 4 | BRF_GRA },           //  4 Tiles

	{ "07_ic42.bin",	0x100000, 0x5672bdaa, 5 | BRF_GRA },           //  5 Sprites
	{ "08_ic29.bin",	0x080000, 0xb4c0ace3, 5 | BRF_GRA },           //  6

	{ "05_ic54.bin",	0x080000, 0x3b8c2b0e, 6 | BRF_SND },           //  7 OKI1 Samples

	{ "06_ic53.bin",	0x080000, 0xc1517cd4, 7 | BRF_SND },           //  8 OKI2 Samples

	{ "10_ic81.bin",	0x000100, 0xcfdbb86c, 0 | BRF_OPT },           //  9 Unused proms
	{ "11_ic80.bin",	0x000100, 0x633ab1c9, 0 | BRF_OPT },           // 10
};

STDROMPICKEXT(acrobatm, acrobatm, nmk004)
STD_ROM_FN(acrobatm)

static INT32 AcrobatmLoadCallback()
{
	{
		if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x000000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x000000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x100000,  6, 1)) return 1;

		if (BurnLoadRom(DrvSndROM0 + 0x020000,  7, 1)) return 1;
		memcpy (DrvSndROM0 + 0x000000, DrvSndROM0 + 0x20000, 0x20000);

		if (BurnLoadRom(DrvSndROM1 + 0x020000,  8, 1)) return 1;
		memcpy (DrvSndROM1 + 0x000000, DrvSndROM1 + 0x20000, 0x20000);

		DrvGfxDecode(0x10000, 0x100000, 0x200000);
	}

	SekInit(0, 0x68000);	
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,		0x080000, 0x08ffff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		0x0c4000, 0x0c47ff, MAP_RAM);
	SekMapMemory(DrvScrollRAM,	0x0c8000, 0x0c83ff, MAP_WRITE);
	SekMapMemory(DrvBgRAM0,		0x0cc000, 0x0cffff, MAP_RAM);
	SekMapMemory(DrvTxRAM,		0x0d4000, 0x0d47ff, MAP_RAM);
	SekSetWriteWordHandler(0,	acrobatm_main_write_word);
	SekSetWriteByteHandler(0,	acrobatm_main_write_byte);
	SekSetReadWordHandler(0,	acrobatm_main_read_word);
	SekSetReadByteHandler(0,	acrobatm_main_read_byte);
	SekClose();

	return 0;
}

static INT32 AcrobatmInit()
{
	INT32 rc = NMK004Init(AcrobatmLoadCallback, 10000000);

	if (!rc) {
		MSM6295SetRoute(1, 0.18, BURN_SND_ROUTE_BOTH);
	}

	return rc;
}

struct BurnDriver BurnDrvAcrobatm = {
	"acrobatm", NULL, "nmk004", NULL, "1991",
	"Acrobat Mission\0", NULL, "UPL (Taito license)", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, acrobatmRomInfo, acrobatmRomName, NULL, NULL, AcrobatmInputInfo, AcrobatmDIPInfo,
	AcrobatmInit, NMK004Exit, NMK004Frame, AcrobatmDraw, DrvScan, NULL, 0x300,
	224, 256, 3, 4
};


// Super Spacefortress Macross / Chou-Jikuu Yousai Macross

static struct BurnRomInfo macrossRomDesc[] = {
	{ "921a03",		0x080000, 0x33318d55, 1 | BRF_PRG | BRF_ESS }, //  0 68k code

	{ "921a02",		0x010000, 0x77c082c7, 2 | BRF_PRG | BRF_ESS }, //  1 NMK004 data

	{ "921a01",		0x020000, 0xbbd8242d, 3 | BRF_GRA },           //  2 Characters

	{ "921a04",		0x200000, 0x4002e4bb, 4 | BRF_GRA },           //  3 Tiles

	{ "921a07",		0x200000, 0x7d2bf112, 5 | BRF_GRA },           //  4 Sprites

	{ "921a05",		0x080000, 0xd5a1eddd, 6 | BRF_SND },           //  5 OKI1 Samples

	{ "921a06",		0x080000, 0x89461d0f, 7 | BRF_SND },           //  6 OKI2 Samples

	{ "921a08",		0x000100, 0xcfdbb86c, 0 | BRF_OPT },           //  7 Unused proms
	{ "921a09",		0x000100, 0x633ab1c9, 0 | BRF_OPT },           //  8
	{ "921a10",		0x000020, 0x8371e42d, 0 | BRF_OPT },           //  9
};

STDROMPICKEXT(macross, macross, nmk004)
STD_ROM_FN(macross)

static INT32 MacrossLoadCallback()
{
	{
		if (BurnLoadRom(Drv68KROM  + 0x000000,  0, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x000000,  1, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x000000,  4, 1)) return 1;
		BurnByteswap(DrvGfxROM2, 0x200000);

		if (BurnLoadRom(DrvSndROM0 + 0x020000,  5, 1)) return 1;
		memcpy (DrvSndROM0 + 0x000000, DrvSndROM0 + 0x20000, 0x20000);

		if (BurnLoadRom(DrvSndROM1 + 0x020000,  6, 1)) return 1;
		memcpy (DrvSndROM1 + 0x000000, DrvSndROM1 + 0x20000, 0x20000);

		decode_gfx(0x200000, 0x200000);
		DrvGfxDecode(0x20000, 0x200000, 0x200000);
	}

	SekInit(0, 0x68000);	
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(DrvPalRAM,		0x088000, 0x0887ff, MAP_RAM);
	SekMapMemory(DrvScrollRAM,	0x08c000, 0x08c3ff, MAP_WRITE);
	SekMapMemory(DrvBgRAM0,		0x090000, 0x093fff, MAP_RAM);
	SekMapMemory(DrvTxRAM,		0x09c000, 0x09c7ff, MAP_RAM);
	SekMapMemory(Drv68KRAM,		0x0f0000, 0x0fffff, MAP_ROM);
	SekSetWriteWordHandler(0,	macross_main_write_word);
	SekSetWriteByteHandler(0,	macross_main_write_byte);
	SekSetReadWordHandler(0,	macross_main_read_word);
	SekSetReadByteHandler(0,	macross_main_read_byte);
	SekClose();

	return 0;
}

static INT32 MacrossInit()
{
	Macrossmode = 1;
	return NMK004Init(MacrossLoadCallback, 10000000);
}

struct BurnDriver BurnDrvMacross = {
	"macross", NULL, "nmk004", NULL, "1992",
	"Super Spacefortress Macross / Chou-Jikuu Yousai Macross\0", NULL, "Banpresto", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, macrossRomInfo, macrossRomName, NULL, NULL, CommonInputInfo, MacrossDIPInfo,
	MacrossInit, NMK004Exit, NMK004Frame, MacrossDraw, DrvScan, NULL, 0x400,
	224, 256, 3, 4
};


// GunNail (28th May. 1992)

static struct BurnRomInfo gunnailRomDesc[] = {
	{ "3e.u131",		0x040000, 0x61d985b2, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "3o.u133",		0x040000, 0xf114e89c, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "92077_2.u101",	0x010000, 0xcd4e55f8, 2 | BRF_PRG | BRF_ESS }, //  2 NMK004 data

	{ "1.u21",			0x020000, 0x3d00a9f4, 3 | BRF_GRA },           //  3 Characters

	{ "92077-4.u19",	0x100000, 0xa9ea2804, 4 | BRF_GRA },           //  4 Tiles

	{ "92077-7.u134",	0x200000, 0xd49169b3, 5 | BRF_GRA },           //  5 Sprites

	{ "92077-5.u56",	0x080000, 0xfeb83c73, 6 | BRF_SND },           //  6 OKI1 Samples

	{ "92077-6.u57",	0x080000, 0x6d133f0d, 7 | BRF_SND },           //  7 OKI2 Samples

	{ "8_82s129.u35",	0x000100, 0x4299776e, 0 | BRF_OPT },           //  8 Unused proms
	{ "9_82s135.u72",	0x000100, 0x633ab1c9, 0 | BRF_OPT },           //  9
	{ "10_82s123.u96",	0x000020, 0xc60103c8, 0 | BRF_OPT },           // 10
};

STDROMPICKEXT(gunnail, gunnail, nmk004)
STD_ROM_FN(gunnail)

static INT32 GunnailLoadCallback()
{
	{
		if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x000000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x000000,  5, 1)) return 1;
		BurnByteswap(DrvGfxROM2, 0x200000);

		if (BurnLoadRom(DrvSndROM0 + 0x020000,  6, 1)) return 1;
		memcpy (DrvSndROM0 + 0x000000, DrvSndROM0 + 0x20000, 0x20000);

		if (BurnLoadRom(DrvSndROM1 + 0x020000,  7, 1)) return 1;
		memcpy (DrvSndROM1 + 0x000000, DrvSndROM1 + 0x20000, 0x20000);

		decode_gfx(0x100000, 0x200000);
		DrvGfxDecode(0x20000, 0x100000, 0x200000);
	}

	SekInit(0, 0x68000);	
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(DrvPalRAM,		0x088000, 0x0887ff, MAP_RAM);
	SekMapMemory(DrvScrollRAM,	0x08c000, 0x08c7ff, MAP_WRITE);
	SekMapMemory(DrvBgRAM0,		0x090000, 0x093fff, MAP_RAM);
	SekMapMemory(DrvTxRAM,		0x09c000, 0x09cfff, MAP_RAM);
	SekMapMemory(DrvTxRAM,		0x09d000, 0x09dfff, MAP_RAM);
	SekMapMemory(Drv68KRAM,		0x0f0000, 0x0fffff, MAP_RAM);
	SekSetWriteWordHandler(0,	macross_main_write_word);
	SekSetWriteByteHandler(0,	macross_main_write_byte);
	SekSetReadWordHandler(0,	macross_main_read_word);
	SekSetReadByteHandler(0,	macross_main_read_byte);
	SekClose();

	return 0;
}

static INT32 GunnailInit()
{
	GunnailMode = 1;
	return NMK004Init(GunnailLoadCallback, 12000000);
}

struct BurnDriver BurnDrvGunnail = {
	"gunnail", NULL, "nmk004", NULL, "1993",
	"GunNail (28th May. 1992)\0", NULL, "NMK / Tecmo", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, gunnailRomInfo, gunnailRomName, NULL, NULL, GunnailInputInfo, GunnailDIPInfo,
	GunnailInit, NMK004Exit, NMK004Frame, GunnailDraw, DrvScan, NULL, 0x400,
	224, 384, 3, 4
};


// Black Heart

static struct BurnRomInfo blkheartRomDesc[] = {
	{ "blkhrt.7",		0x020000, 0x5bd248c0, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "blkhrt.6",		0x020000, 0x6449e50d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "4.bin",			0x010000, 0x7cefa295, 2 | BRF_PRG | BRF_ESS }, //  2 NMK004 data

	{ "3.bin",			0x020000, 0xa1ab3a16, 3 | BRF_GRA },           //  3 Characters

	{ "90068-5.bin",	0x100000, 0xa1ab4f24, 4 | BRF_GRA },           //  4 Tiles

	{ "90068-8.bin",	0x100000, 0x9d3204b2, 5 | BRF_GRA },           //  5 Sprites

	{ "90068-2.bin",	0x080000, 0x3a583184, 6 | BRF_SND },           //  6 OKI1 Samples

	{ "90068-1.bin",	0x080000, 0xe7af69d2, 7 | BRF_SND },           //  7 OKI2 Samples

	{ "9.bpr",			0x000100, 0x98ed1c97, 0 | BRF_OPT },           //  8 Unused proms
	{ "10.bpr",			0x000100, 0xcfdbb86c, 0 | BRF_OPT },           //  9
};

STDROMPICKEXT(blkheart, blkheart, nmk004)
STD_ROM_FN(blkheart)

static INT32 BlkheartLoadCallback()
{
	{
		if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x000000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x000000,  5, 1)) return 1;
		BurnByteswap(DrvGfxROM2, 0x100000);

		if (BurnLoadRom(DrvSndROM0 + 0x020000,  6, 1)) return 1;
		memcpy (DrvSndROM0 + 0x000000, DrvSndROM0 + 0x20000, 0x20000);

		if (BurnLoadRom(DrvSndROM1 + 0x020000,  7, 1)) return 1;
		memcpy (DrvSndROM1 + 0x000000, DrvSndROM1 + 0x20000, 0x20000);


		DrvGfxDecode(0x20000, 0x100000, 0x100000);
	}

	SekInit(0, 0x68000);	
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(DrvPalRAM,		0x088000, 0x0887ff, MAP_RAM);
	SekMapMemory(DrvScrollRAM,	0x08c000, 0x08c3ff, MAP_WRITE);
	SekMapMemory(DrvBgRAM0,		0x090000, 0x093fff, MAP_RAM);
	SekMapMemory(DrvTxRAM,		0x09c000, 0x09c7ff, MAP_RAM);
	SekMapMemory(Drv68KRAM,		0x0f0000, 0x0fffff, MAP_ROM);
	SekSetWriteWordHandler(0,	macross_main_write_word);
	SekSetWriteByteHandler(0,	macross_main_write_byte);
	SekSetReadWordHandler(0,	macross_main_read_word);
	SekSetReadByteHandler(0,	macross_main_read_byte);
	SekClose();

	return 0;
}

static INT32 BlkheartInit()
{
	return NMK004Init(BlkheartLoadCallback, 8000000);
}

struct BurnDriver BurnDrvBlkheart = {
	"blkheart", NULL, "nmk004", NULL, "1991",
	"Black Heart\0", NULL, "UPL", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_HORSHOOT, 0,
	NULL, blkheartRomInfo, blkheartRomName, NULL, NULL, CommonInputInfo, BlkheartDIPInfo,
	BlkheartInit, NMK004Exit, NMK004Frame, MacrossDraw, DrvScan, NULL, 0x400,
	256, 224, 4, 3
};


// Black Heart (Japan)

static struct BurnRomInfo blkheartjRomDesc[] = {
	{ "7.bin",			0x020000, 0xe0a5c667, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "6.bin",			0x020000, 0x7cce45e8, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "4.bin",			0x010000, 0x7cefa295, 2 | BRF_PRG | BRF_ESS }, //  2 NMK004 data

	{ "3.bin",			0x020000, 0xa1ab3a16, 3 | BRF_GRA },           //  3 Characters

	{ "90068-5.bin",	0x100000, 0xa1ab4f24, 4 | BRF_GRA },           //  4 Tiles

	{ "90068-8.bin",	0x100000, 0x9d3204b2, 5 | BRF_GRA },           //  5 Sprites

	{ "90068-2.bin",	0x080000, 0x3a583184, 6 | BRF_SND },           //  6 OKI1 Samples

	{ "90068-1.bin",	0x080000, 0xe7af69d2, 7 | BRF_SND },           //  7 OKI2 Samples

	{ "9.bpr",			0x000100, 0x98ed1c97, 0 | BRF_OPT },           //  8 Unused proms
	{ "10.bpr",			0x000100, 0xcfdbb86c, 0 | BRF_OPT },           //  9
};

STDROMPICKEXT(blkheartj, blkheartj, nmk004)
STD_ROM_FN(blkheartj)

struct BurnDriver BurnDrvBlkheartj = {
	"blkheartj", "blkheart", "nmk004", NULL, "1991",
	"Black Heart (Japan)\0", NULL, "UPL", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_HORSHOOT, 0,
	NULL, blkheartjRomInfo, blkheartjRomName, NULL, NULL, CommonInputInfo, BlkheartDIPInfo,
	BlkheartInit, NMK004Exit, NMK004Frame, MacrossDraw, DrvScan, NULL, 0x400,
	256, 224, 4, 3
};


// Vandyke (Japan)

static struct BurnRomInfo vandykeRomDesc[] = {
	{ "vdk-1.16",		0x020000, 0xc1d01c59, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "vdk-2.15",		0x020000, 0x9d741cc2, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "vdk-4.127",		0x010000, 0xeba544f0, 2 | BRF_PRG | BRF_ESS }, //  2 NMK004 data

	{ "vdk-3.222",		0x010000, 0x5a547c1b, 3 | BRF_GRA },           //  3 Characters

	{ "vdk-01.13",		0x080000, 0x195a24be, 4 | BRF_GRA },           //  4 Tiles

	{ "vdk-07.202",		0x080000, 0x42d41f06, 5 | BRF_GRA },           //  5 Sprites
	{ "vdk-06.203",		0x080000, 0xd54722a8, 5 | BRF_GRA },           //  6
	{ "vdk-04.2-1",		0x080000, 0x0a730547, 5 | BRF_GRA },           //  7
	{ "vdk-05.3-1",		0x080000, 0xba456d27, 5 | BRF_GRA },           //  8

	{ "vdk-02.126",		0x080000, 0xb2103274, 6 | BRF_SND },           //  9 OKI1 Samples

	{ "vdk-03.165",		0x080000, 0x631776d3, 7 | BRF_SND },           // 10 OKI2 Samples

	{ "ic100.bpr",		0x000100, 0x98ed1c97, 0 | BRF_OPT },           // 11 Unused proms
	{ "ic101.bpr",		0x000100, 0xcfdbb86c, 0 | BRF_OPT },           // 12
};

STDROMPICKEXT(vandyke, vandyke, nmk004)
STD_ROM_FN(vandyke)

static INT32 VandykeLoadCallback()
{
	{
		if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x000000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x000000,  5, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x000001,  6, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x100000,  7, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x100001,  8, 2)) return 1;

		if (BurnLoadRom(DrvSndROM0 + 0x020000,  9, 1)) return 1;
		memcpy (DrvSndROM0 + 0x000000, DrvSndROM0 + 0x20000, 0x20000);

		if (BurnLoadRom(DrvSndROM1 + 0x020000, 10, 1)) return 1;
		memcpy (DrvSndROM1 + 0x000000, DrvSndROM1 + 0x20000, 0x20000);

		DrvGfxDecode(0x10000, 0x80000, 0x200000);
	}

	SekInit(0, 0x68000);	
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(DrvPalRAM,		0x088000, 0x0887ff, MAP_RAM);
	SekMapMemory(DrvScrollRAM,	0x08c000, 0x08c007, MAP_RAM);
	SekMapMemory(DrvBgRAM0,		0x090000, 0x093fff, MAP_RAM);
	SekMapMemory(DrvBgRAM1,		0x094000, 0x097fff, MAP_RAM);
	SekMapMemory(DrvTxRAM,		0x09d000, 0x09d7ff, MAP_RAM);
	SekMapMemory(Drv68KRAM,		0x0f0000, 0x0fffff, MAP_RAM);
	SekSetWriteWordHandler(0,	macross_main_write_word);
	SekSetWriteByteHandler(0,	macross_main_write_byte);
	SekSetReadWordHandler(0,	macross_main_read_word);
	SekSetReadByteHandler(0,	macross_main_read_byte);
	SekClose();

	return 0;
}

static INT32 VandykeInit()
{
	return NMK004Init(VandykeLoadCallback, 10000000);
}

struct BurnDriver BurnDrvVandyke = {
	"vandyke", NULL, "nmk004", NULL, "1990",
	"Vandyke (Japan)\0", NULL, "UPL", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, vandykeRomInfo, vandykeRomName, NULL, NULL, CommonInputInfo, VandykeDIPInfo,
	VandykeInit, NMK004Exit, NMK004Frame, VandykeDraw, DrvScan, NULL, 0x400,
	224, 256, 3, 4
};


// Vandyke (Jaleco, Set 1)

static struct BurnRomInfo vandykejalRomDesc[] = {
	{ "vdk-1.16",		0x020000, 0xc1d01c59, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "jaleco2.15",		0x020000, 0x170e4d2e, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "vdk-4.127",		0x010000, 0xeba544f0, 2 | BRF_PRG | BRF_ESS }, //  2 NMK004 data

	{ "vdk-3.222",		0x010000, 0x5a547c1b, 3 | BRF_GRA },           //  3 Characters

	{ "vdk-01.13",		0x080000, 0x195a24be, 4 | BRF_GRA },           //  4 Tiles

	{ "vdk-07.202",		0x080000, 0x42d41f06, 5 | BRF_GRA },           //  5 Sprites
	{ "vdk-06.203",		0x080000, 0xd54722a8, 5 | BRF_GRA },           //  6
	{ "vdk-04.2-1",		0x080000, 0x0a730547, 5 | BRF_GRA },           //  7
	{ "vdk-05.3-1",		0x080000, 0xba456d27, 5 | BRF_GRA },           //  8

	{ "vdk-02.126",		0x080000, 0xb2103274, 6 | BRF_SND },           //  9 OKI1 Samples

	{ "vdk-03.165",		0x080000, 0x631776d3, 7 | BRF_SND },           // 10 OKI2 Samples

	{ "ic100.bpr",		0x000100, 0x98ed1c97, 0 | BRF_OPT },           // 11 Unused proms
	{ "ic101.bpr",		0x000100, 0xcfdbb86c, 0 | BRF_OPT },           // 12
};

STDROMPICKEXT(vandykejal, vandykejal, nmk004)
STD_ROM_FN(vandykejal)

struct BurnDriver BurnDrvVandykejal = {
	"vandykejal", "vandyke", "nmk004", NULL, "1990",
	"Vandyke (Jaleco, Set 1)\0", NULL, "UPL (Jaleco license)", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, vandykejalRomInfo, vandykejalRomName, NULL, NULL, CommonInputInfo, VandykeDIPInfo,
	VandykeInit, NMK004Exit, NMK004Frame, VandykeDraw, DrvScan, NULL, 0x400,
	224, 256, 3, 4
};


// Vandyke (Jaleco, Set 2)

static struct BurnRomInfo vandykejal2RomDesc[] = {
	{ "vdk-even.16",	0x020000, 0xcde05a84, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "vdk-odd.15",		0x020000, 0x0f6fea40, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "vdk-4.127",		0x010000, 0xeba544f0, 2 | BRF_PRG | BRF_ESS }, //  2 NMK004 data

	{ "vdk-3.222",		0x010000, 0x5a547c1b, 3 | BRF_GRA },           //  3 Characters

	{ "vdk-01.13",		0x080000, 0x195a24be, 4 | BRF_GRA },           //  4 Tiles

	{ "vdk-07.202",		0x080000, 0x42d41f06, 5 | BRF_GRA },           //  5 Sprites
	{ "vdk-06.203",		0x080000, 0xd54722a8, 5 | BRF_GRA },           //  6
	{ "vdk-04.2-1",		0x080000, 0x0a730547, 5 | BRF_GRA },           //  7
	{ "vdk-05.3-1",		0x080000, 0xba456d27, 5 | BRF_GRA },           //  8

	{ "vdk-02.126",		0x080000, 0xb2103274, 6 | BRF_SND },           //  9 OKI1 Samples

	{ "vdk-03.165",		0x080000, 0x631776d3, 7 | BRF_SND },           // 10 OKI2 Samples

	{ "ic100.bpr",		0x000100, 0x98ed1c97, 0 | BRF_OPT },           // 11 Unused proms
	{ "ic101.bpr",		0x000100, 0xcfdbb86c, 0 | BRF_OPT },           // 12
};

STDROMPICKEXT(vandykejal2, vandykejal2, nmk004)
STD_ROM_FN(vandykejal2)

struct BurnDriver BurnDrvVandykejal2 = {
	"vandykejal2", "vandyke", "nmk004", NULL, "1990",
	"Vandyke (Jaleco, Set 2)\0", NULL, "UPL (Jaleco license)", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, vandykejal2RomInfo, vandykejal2RomName, NULL, NULL, CommonInputInfo, VandykeDIPInfo,
	VandykeInit, NMK004Exit, NMK004Frame, VandykeDraw, DrvScan, NULL, 0x400,
	224, 256, 3, 4
};


// Vandyke (bootleg with PIC16c57)

static struct BurnRomInfo vandykebRomDesc[] = {
	{ "2.bin",		0x020000, 0x9c269702, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "1.bin",		0x020000, 0xdd6303a1, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "pic16c57",	0x002d4c, 0xbdb3920d, 2 | BRF_PRG | BRF_ESS }, //  2 PIC16c57 code

	{ "3.bin",		0x010000, 0x5a547c1b, 3 | BRF_GRA },           //  3 Characters

	{ "4.bin",		0x040000, 0x4ba4138d, 4 | BRF_GRA },           //  4 Tiles
	{ "5.bin",		0x040000, 0x9a1ac697, 4 | BRF_GRA },           //  5

	{ "13.bin",		0x040000, 0xbb561871, 5 | BRF_GRA },           //  6 Sprites
	{ "17.bin",		0x040000, 0x346e3b66, 5 | BRF_GRA },           //  7
	{ "12.bin",		0x040000, 0xcdef9b17, 5 | BRF_GRA },           //  8
	{ "16.bin",		0x040000, 0xbeda678c, 5 | BRF_GRA },           //  9
	{ "11.bin",		0x020000, 0x823185d9, 5 | BRF_GRA },           // 10
	{ "15.bin",		0x020000, 0x149f3247, 5 | BRF_GRA },           // 11
	{ "10.bin",		0x020000, 0x388b1abc, 5 | BRF_GRA },           // 12
	{ "14.bin",		0x020000, 0x32eeba37, 5 | BRF_GRA },           // 13

	{ "9.bin",		0x020000, 0x56bf774f, 6 | BRF_SND },           // 14 OKI1 Samples
	{ "8.bin",		0x020000, 0x89851fcf, 6 | BRF_SND },           // 15
	{ "7.bin",		0x020000, 0xd7bf0f6a, 6 | BRF_SND },           // 16
	{ "6.bin",		0x020000, 0xa7fcf709, 6 | BRF_SND },           // 17
};

STD_ROM_PICK(vandykeb)
STD_ROM_FN(vandykeb)

static INT32 VandykebLoadCallback()
{
	{
		if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;

	//	if (BurnLoadRom(DrvZ80ROM  + 0x000000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x040000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x000000,  6, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x000001,  7, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x080000,  8, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x080001,  9, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x100000, 10, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x100001, 11, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x140000, 12, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x140001, 13, 2)) return 1;

	//	if (BurnLoadRom(DrvSndROM0 + 0x000000, 14, 1)) return 1;
	//	if (BurnLoadRom(DrvSndROM0 + 0x020000, 15, 1)) return 1;
	//	if (BurnLoadRom(DrvSndROM0 + 0x040000, 16, 1)) return 1;
	//	if (BurnLoadRom(DrvSndROM0 + 0x060000, 17, 1)) return 1;

		DrvGfxDecode(0x10000, 0x80000, 0x200000);
	}

	SekInit(0, 0x68000);	
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(DrvPalRAM,		0x088000, 0x0887ff, MAP_RAM);
//	SekMapMemory(DrvScrollRAM,	0x08c000, 0x08c3ff, MAP_RAM);
	SekMapMemory(DrvBgRAM0,		0x090000, 0x093fff, MAP_RAM);
	SekMapMemory(DrvTxRAM,		0x09d000, 0x09d7ff, MAP_RAM);
	SekMapMemory(Drv68KRAM,		0x0f0000, 0x0fffff, MAP_RAM);
	SekSetWriteWordHandler(0,	vandykeb_main_write_word); // different scroll regs
	SekSetWriteByteHandler(0,	vandykeb_main_write_byte);
	SekSetReadWordHandler(0,	macross_main_read_word);
	SekSetReadByteHandler(0,	macross_main_read_byte);
	SekClose();

	return 0;
}

static INT32 VandykebInit()
{
	input_high[0] = 0x0040; // or it locks up

	return NMK004Init(VandykebLoadCallback, 10000000);
}

struct BurnDriver BurnDrvVandykeb = {
	"vandykeb", "vandyke", NULL, NULL, "1990",
	"Vandyke (bootleg with PIC16c57)\0", "No sound", "[UPL] (bootleg)", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, vandykebRomInfo, vandykebRomName, NULL, NULL, CommonInputInfo, VandykeDIPInfo,
	VandykebInit, NMK004Exit, NMK004Frame, VandykeDraw, DrvScan, NULL, 0x400,
	224, 256, 3, 4
};


// Hacha Mecha Fighter (19th Sep. 1991)

static struct BurnRomInfo hachamfRomDesc[] = {
	{ "7.93",			0x020000, 0x9d847c31, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "6.94",			0x020000, 0xde6408a0, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "1.70",			0x010000, 0x9e6f48fc, 2 | BRF_PRG | BRF_ESS }, //  2 NMK004 data

	{ "5.95",			0x020000, 0x29fb04a2, 3 | BRF_GRA },           //  3 Characters

	{ "91076-4.101",	0x100000, 0xdf9653a4, 4 | BRF_GRA },           //  4 Tiles

	{ "91076-8.57",		0x100000, 0x7fd0f556, 5 | BRF_GRA },           //  5 Sprites

	{ "91076-2.46",		0x080000, 0x3f1e67f2, 6 | BRF_SND },           //  6 OKI1 Samples

	{ "91076-3.45",		0x080000, 0xb25ed93b, 7 | BRF_SND },           //  7 OKI2 Samples
};

STDROMPICKEXT(hachamf, hachamf, nmk004)
STD_ROM_FN(hachamf)

static INT32 HachamfLoadCallback()
{
	{
		if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x000000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x000000,  5, 1)) return 1;
		BurnByteswap(DrvGfxROM2, 0x100000);

		if (BurnLoadRom(DrvSndROM0 + 0x020000,  6, 1)) return 1;
		memcpy (DrvSndROM0 + 0x000000, DrvSndROM0 + 0x20000, 0x20000);

		if (BurnLoadRom(DrvSndROM1 + 0x020000,  7, 1)) return 1;
		memcpy (DrvSndROM1 + 0x000000, DrvSndROM1 + 0x20000, 0x20000);

		DrvGfxDecode(0x20000, 0x100000, 0x100000);
	}

	{
		*((UINT16*)(Drv68KROM + 0x048a)) = 0x4e71;
		*((UINT16*)(Drv68KROM + 0x04aa)) = 0x4e71;
	}

	SekInit(0, 0x68000);	
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(DrvPalRAM,		0x088000, 0x0887ff, MAP_RAM);
	SekMapMemory(DrvScrollRAM,	0x08c000, 0x08c3ff, MAP_WRITE);
	SekMapMemory(DrvBgRAM0,		0x090000, 0x093fff, MAP_RAM);
	SekMapMemory(DrvTxRAM,		0x09c000, 0x09c7ff, MAP_RAM);
	SekMapMemory(Drv68KRAM,		0x0f0000, 0x0fffff, MAP_RAM);
	SekSetWriteWordHandler(0,	hachamf_main_write_word);
	SekSetWriteByteHandler(0,	hachamf_main_write_byte);
	SekSetReadWordHandler(0,	hachamf_main_read_word);
	SekSetReadByteHandler(0,	hachamf_main_read_byte);
	SekClose();

	return 0;
}

static INT32 HachamfInit()
{
	return NMK004Init(HachamfLoadCallback, 10000000);
}

struct BurnDriver BurnDrvHachamf = {
	"hachamf", NULL, "nmk004", NULL, "1991",
	"Hacha Mecha Fighter (19th Sep. 1991)\0", "Use the unprotected bootleg, instead!", "NMK", "NMK16",
	NULL, NULL, NULL, NULL,
	0, 2, HARDWARE_MISC_POST90S, GBF_HORSHOOT, 0,
	NULL, hachamfRomInfo, hachamfRomName, NULL, NULL, CommonInputInfo, HachamfDIPInfo,
	HachamfInit, NMK004Exit, NMK004Frame, HachamfDraw, DrvScan, NULL, 0x400,
	256, 224, 4, 3
};


// Hacha Mecha Fighter (19th Sep. 1991, unprotected, bootleg Thunder Dragon conversion)

static struct BurnRomInfo hachamfbRomDesc[] = {
	{ "8.bin",			0x020000, 0x14845B65, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "7.bin",			0x020000, 0x069CA579, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "1.70",			0x010000, 0x9e6f48fc, 2 | BRF_PRG | BRF_ESS }, //  2 NMK004 data

	{ "5.95",			0x020000, 0x29fb04a2, 3 | BRF_GRA },           //  3 Characters

	{ "91076-4.101",	0x100000, 0xdf9653a4, 4 | BRF_GRA },           //  4 Tiles

	{ "91076-8.57",		0x100000, 0x7fd0f556, 5 | BRF_GRA },           //  5 Sprites

	{ "91076-2.46",		0x080000, 0x3f1e67f2, 6 | BRF_SND },           //  6 OKI1 Samples

	{ "91076-3.45",		0x080000, 0xb25ed93b, 7 | BRF_SND },           //  7 OKI2 Samples
};

STDROMPICKEXT(hachamfb, hachamfb, nmk004)
STD_ROM_FN(hachamfb)

static INT32 HachamfbLoadCallback()
{
	{
		if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x000000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x000000,  5, 1)) return 1;
		BurnByteswap(DrvGfxROM2, 0x100000);

		if (BurnLoadRom(DrvSndROM0 + 0x020000,  6, 1)) return 1;
		memcpy (DrvSndROM0 + 0x000000, DrvSndROM0 + 0x20000, 0x20000);

		if (BurnLoadRom(DrvSndROM1 + 0x020000,  7, 1)) return 1;
		memcpy (DrvSndROM1 + 0x000000, DrvSndROM1 + 0x20000, 0x20000);

		DrvGfxDecode(0x20000, 0x100000, 0x100000);
	}

	SekInit(0, 0x68000);	
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(DrvPalRAM,		0x088000, 0x0887ff, MAP_RAM);
	SekMapMemory(DrvScrollRAM,	0x08c000, 0x08c3ff, MAP_WRITE);
	SekMapMemory(DrvBgRAM0,		0x090000, 0x093fff, MAP_RAM);
	SekMapMemory(DrvTxRAM,		0x09c000, 0x09c7ff, MAP_RAM);
	SekMapMemory(Drv68KRAM,		0x0f0000, 0x0fffff, MAP_RAM);
	SekSetWriteWordHandler(0,	hachamf_main_write_word);
	SekSetWriteByteHandler(0,	hachamf_main_write_byte);
	SekSetReadWordHandler(0,	hachamf_main_read_word);
	SekSetReadByteHandler(0,	hachamf_main_read_byte);
	SekClose();

	return 0;
}

static INT32 HachamfbInit()
{
	return NMK004Init(HachamfbLoadCallback, 10000000);
}

struct BurnDriver BurnDrvHachamfb = {
	"hachamfb", "hachamf", "nmk004", NULL, "1991",
	"Hacha Mecha Fighter (19th Sep. 1991, unprotected, bootleg Thunder Dragon conversion)\0", NULL, "bootleg", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_MISC_POST90S, GBF_HORSHOOT, 0,
	NULL, hachamfbRomInfo, hachamfbRomName, NULL, NULL, CommonInputInfo, HachamfbDIPInfo,
	HachamfbInit, NMK004Exit, NMK004Frame, HachamfDraw, DrvScan, NULL, 0x400,
	256, 224, 4, 3
};


// Koutetsu Yousai Strahl (Japan set 1)

static struct BurnRomInfo strahlRomDesc[] = {
	{ "strahl-2.82",	0x020000, 0xc9d008ae, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "strahl-1.83",	0x020000, 0xafc3c4d6, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "strahl-4.66",	0x010000, 0x60a799c4, 2 | BRF_PRG | BRF_ESS }, //  2 NMK004 data

	{ "strahl-3.73",	0x010000, 0x2273b33e, 3 | BRF_GRA },           //  3 Characters

	{ "str7b2r0.275",	0x040000, 0x5769e3e1, 4 | BRF_GRA },           //  4 Tiles

	{ "strl3-01.32",	0x080000, 0xd8337f15, 5 | BRF_GRA },           //  5 Sprites
	{ "strl4-02.57",	0x080000, 0x2a38552b, 5 | BRF_GRA },           //  6
	{ "strl5-03.58",	0x080000, 0xa0e7d210, 5 | BRF_GRA },           //  7

	{ "str6b1w1.776",	0x080000, 0xbb1bb155, 9 | BRF_GRA },           //  8 Foreground Tiles

	{ "str8pmw1.540",	0x080000, 0x01d6bb6a, 6 | BRF_SND },           //  9 OKI1 Samples

	{ "str9pew1.639",	0x080000, 0x6bb3eb9f, 7 | BRF_SND },           // 10 OKI2 Samples
};

STDROMPICKEXT(strahl, strahl, nmk004)
STD_ROM_FN(strahl)

static INT32 StrahlLoadCallback()
{
	{
		if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x000000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x000000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x080000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x100000,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x080000,  8, 1)) return 1;

		if (BurnLoadRom(DrvSndROM0 + 0x100000,  9, 1)) return 1;
		memcpy (DrvSndROM0 + 0x000000, DrvSndROM0 + 0x100000, 0x20000);
		memcpy (DrvSndROM0 + 0x080000, DrvSndROM0 + 0x120000, 0x20000);
		memcpy (DrvSndROM0 + 0x060000, DrvSndROM0 + 0x140000, 0x20000);
		memcpy (DrvSndROM0 + 0x040000, DrvSndROM0 + 0x160000, 0x20000);

		if (BurnLoadRom(DrvSndROM1 + 0x100000, 10, 1)) return 1;
		memcpy (DrvSndROM1 + 0x000000, DrvSndROM1 + 0x100000, 0x20000);
		memcpy (DrvSndROM1 + 0x080000, DrvSndROM1 + 0x120000, 0x20000);
		memcpy (DrvSndROM1 + 0x060000, DrvSndROM1 + 0x140000, 0x20000);
		memcpy (DrvSndROM1 + 0x040000, DrvSndROM1 + 0x160000, 0x20000);

		DrvGfxDecode(0x10000, 0x100000, 0x200000);
	}

	SekInit(0, 0x68000);	
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(DrvScrollRAM,		0x084000, 0x0843ff, MAP_WRITE);
	SekMapMemory(DrvScrollRAM + 0x400,	0x088000, 0x0883ff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x08c000, 0x08c7ff, MAP_RAM);
	SekMapMemory(DrvBgRAM0,			0x090000, 0x093fff, MAP_RAM);
	SekMapMemory(DrvBgRAM1,			0x094000, 0x097fff, MAP_RAM);
	SekMapMemory(DrvTxRAM,			0x09c000, 0x09c7ff, MAP_RAM);
	SekMapMemory(Drv68KRAM,			0x0f0000, 0x0fffff, MAP_RAM);
	SekSetWriteWordHandler(0,		macross_main_write_word);
	SekSetWriteByteHandler(0,		macross_main_write_byte);
	SekSetReadWordHandler(0,		macross_main_read_word);
	SekSetReadByteHandler(0,		macross_main_read_byte);
	SekClose();

	return 0;
}

static INT32 StrahlInit()
{
	return NMK004Init(StrahlLoadCallback, 12000000);
}

struct BurnDriver BurnDrvStrahl = {
	"strahl", NULL, "nmk004", NULL, "1992",
	"Koutetsu Yousai Strahl (Japan set 1)\0", NULL, "UPL", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_HORSHOOT, 0,
	NULL, strahlRomInfo, strahlRomName, NULL, NULL, CommonInputInfo, StrahlDIPInfo,
	StrahlInit, NMK004Exit, NMK004Frame, StrahlDraw, DrvScan, NULL, 0x400,
	256, 224, 4, 3
};


// Koutetsu Yousai Strahl (Japan set 2)

static struct BurnRomInfo strahlaRomDesc[] = {
	{ "rom2",			0x020000, 0xf80a22ef, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "rom1",			0x020000, 0x802ecbfc, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "strahl-4.66",	0x010000, 0x60a799c4, 2 | BRF_PRG | BRF_ESS }, //  2 NMK004 data

	{ "strahl-3.73",	0x010000, 0x2273b33e, 3 | BRF_GRA },           //  3 Characters

	{ "str7b2r0.275",	0x040000, 0x5769e3e1, 4 | BRF_GRA },           //  4 Tiles

	{ "strl3-01.32",	0x080000, 0xd8337f15, 5 | BRF_GRA },           //  5 Sprites
	{ "strl4-02.57",	0x080000, 0x2a38552b, 5 | BRF_GRA },           //  6
	{ "strl5-03.58",	0x080000, 0xa0e7d210, 5 | BRF_GRA },           //  7

	{ "str6b1w1.776",	0x080000, 0xbb1bb155, 9 | BRF_GRA },           //  8 Foreground tiles

	{ "str8pmw1.540",	0x080000, 0x01d6bb6a, 6 | BRF_SND },           //  9 OKI1 Samples

	{ "str9pew1.639",	0x080000, 0x6bb3eb9f, 7 | BRF_SND },           // 10 OKI2 Samples
};

STDROMPICKEXT(strahla, strahla, nmk004)
STD_ROM_FN(strahla)

struct BurnDriver BurnDrvStrahla = {
	"strahla", "strahl", "nmk004", NULL, "1992",
	"Koutetsu Yousai Strahl (Japan set 2)\0", NULL, "UPL", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_HORSHOOT, 0,
	NULL, strahlaRomInfo, strahlaRomName, NULL, NULL, CommonInputInfo, StrahlDIPInfo,
	StrahlInit, NMK004Exit, NMK004Frame, StrahlDraw, DrvScan, NULL, 0x400,
	256, 224, 4, 3
};


// Bio-ship Paladin

static struct BurnRomInfo bioshipRomDesc[] = {
	{ "2.ic14",			0x020000, 0xacf56afb, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "1.ic15",			0x020000, 0x820ef303, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "6.ic120",		0x010000, 0x5f39a980, 2 | BRF_PRG | BRF_ESS }, //  2 NMK004 data

	{ "7",				0x010000, 0x2f3f5a10, 3 | BRF_GRA },           //  3 Characters

	{ "sbs-g_01.ic9",	0x080000, 0x21302e78, 4 | BRF_GRA },           //  4 Tiles

	{ "sbs-g_03.ic194",	0x080000, 0x60e00d7b, 5 | BRF_GRA },           //  5 Sprites

	{ "sbs-g_02.ic4",	0x080000, 0xf31eb668, 9 | BRF_GRA },           //  6 Foreground tiles

	{ "8.ic27",			0x010000, 0x75a46fea, 10 | BRF_GRA },          //  7 Tilemap roms
	{ "9.ic26",			0x010000, 0xd91448ee, 10 | BRF_GRA },          //  8

	{ "sbs-g_04.ic139",	0x080000, 0x7c74cc4e, 6 | BRF_SND },           //  9 OKI1 Samples

	{ "sbs-g_05.ic160",	0x080000, 0xf0a782e3, 7 | BRF_SND },           // 10 OKI2 Samples

	{ "82s135.ic94",	0x000100, 0x98ed1c97, 0 | BRF_OPT },           // 11 Unused proms
	{ "82s129.ic69",	0x000100, 0xcfdbb86c, 0 | BRF_OPT },           // 12
	{ "82s123.ic154",	0x000020, 0x0f789fc7, 0 | BRF_OPT },           // 13
};

STDROMPICKEXT(bioship, bioship, nmk004)
STD_ROM_FN(bioship)

static INT32 BioshipLoadCallback()
{
	{
		if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x000000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x000000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x080000,  6, 1)) return 1;

		if (BurnLoadRom(DrvTileROM + 0x000001,  7, 2)) return 1;
		if (BurnLoadRom(DrvTileROM + 0x000000,  8, 2)) return 1;

		if (BurnLoadRom(DrvSndROM0 + 0x020000,  9, 1)) return 1;
		memcpy (DrvSndROM0 + 0x000000, DrvSndROM0 + 0x20000, 0x20000);

		if (BurnLoadRom(DrvSndROM1 + 0x020000, 10, 1)) return 1;
		memcpy (DrvSndROM1 + 0x000000, DrvSndROM1 + 0x20000, 0x20000);

		DrvGfxDecode(0x10000, 0x100000, 0x80000);
	}

	SekInit(0, 0x68000);	
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(DrvPalRAM,		0x088000, 0x0887ff, MAP_RAM);
	SekMapMemory(DrvScrollRAM,	0x08c000, 0x08c3ff, MAP_RAM);
	SekMapMemory(DrvBgRAM0,		0x090000, 0x093fff, MAP_RAM);
	SekMapMemory(DrvTxRAM,		0x09c000, 0x09c7ff, MAP_RAM);
	SekMapMemory(Drv68KRAM,		0x0f0000, 0x0fffff, MAP_RAM);
	SekSetWriteWordHandler(0,	macross_main_write_word);
	SekSetWriteByteHandler(0,	macross_main_write_byte);
	SekSetReadWordHandler(0,	macross_main_read_word);
	SekSetReadByteHandler(0,	macross_main_read_byte);
	SekClose();

	return 0;
}

static INT32 BioshipInit()
{
	return NMK004Init(BioshipLoadCallback, 10000000);
}

struct BurnDriver BurnDrvBioship = {
	"bioship", NULL, "nmk004", NULL, "1990",
	"Bio-ship Paladin\0", NULL, "UPL (American Sammy license)", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_HORSHOOT, 0,
	NULL, bioshipRomInfo, bioshipRomName, NULL, NULL, CommonInputInfo, BioshipDIPInfo,
	BioshipInit, NMK004Exit, NMK004Frame, BioshipDraw, DrvScan, NULL, 0x400,
	256, 224, 4, 3
};


// Space Battle Ship Gomorrah

static struct BurnRomInfo sbsgomoRomDesc[] = {
	{ "11.ic14",		0x020000, 0x7916150b, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "10.ic15",		0x020000, 0x1d7accb8, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "6.ic120",		0x010000, 0x5f39a980, 2 | BRF_PRG | BRF_ESS }, //  2 NMK004 data

	{ "7.ic46",			0x010000, 0xf2b77f80, 3 | BRF_GRA },           //  3 Characters

	{ "sbs-g_01.ic9",	0x080000, 0x21302e78, 4 | BRF_GRA },           //  4 Tiles

	{ "sbs-g_03.ic194",	0x080000, 0x60e00d7b, 5 | BRF_GRA },           //  5 Sprites

	{ "sbs-g_02.ic4",	0x080000, 0xf31eb668, 9 | BRF_GRA },           //  6 Foreground tiles

	{ "8.ic27",			0x010000, 0x75a46fea, 10 | BRF_GRA },          //  7 Tilemap roms
	{ "9.ic26",			0x010000, 0xd91448ee, 10 | BRF_GRA },          //  8

	{ "sbs-g_04.ic139",	0x080000, 0x7c74cc4e, 6 | BRF_SND },           //  9 OKI1 Samples

	{ "sbs-g_05.ic160",	0x080000, 0xf0a782e3, 7 | BRF_SND },           // 10 OKI2 Samples

	{ "82s135.ic94",	0x000100, 0x98ed1c97, 0 | BRF_OPT },           // 11 Unused proms
	{ "82s129.ic69",	0x000100, 0xcfdbb86c, 0 | BRF_OPT },           // 12
	{ "82s123.ic154",	0x000020, 0x0f789fc7, 0 | BRF_OPT },           // 13
};

STDROMPICKEXT(sbsgomo, sbsgomo, nmk004)
STD_ROM_FN(sbsgomo)

struct BurnDriver BurnDrvSbsgomo = {
	"sbsgomo", "bioship", "nmk004", NULL, "1990",
	"Space Battle Ship Gomorrah\0", NULL, "UPL", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_HORSHOOT, 0,
	NULL, sbsgomoRomInfo, sbsgomoRomName, NULL, NULL, CommonInputInfo, BioshipDIPInfo,
	BioshipInit, NMK004Exit, NMK004Frame, BioshipDraw, DrvScan, NULL, 0x400,
	256, 224, 4, 3
};


// Rapid Hero

static struct BurnRomInfo rapheroRomDesc[] = {
	{ "rhp94099.3",		0x080000, 0xec9b4f05, 1 | BRF_PRG | BRF_ESS }, //  0 68k code

	{ "rhp94099.2",		0x020000, 0xfe01ece1, 2 | BRF_PRG | BRF_ESS }, //  1 Tmp90c841 Code

	{ "rhp94099.1",		0x020000, 0x55a7a011, 3 | BRF_GRA },           //  2 Characters

	{ "rhp94099.4",		0x200000, 0x076eee7b, 4 | BRF_GRA },           //  3 Tiles

	{ "rhp94099.8",		0x200000, 0x49892f07, 5 | BRF_GRA },           //  4 Sprites
	{ "rhp94099.9",		0x200000, 0xea2e47f0, 5 | BRF_GRA },           //  5
	{ "rhp94099.10",	0x200000, 0x512cb839, 5 | BRF_GRA },           //  6

	{ "rhp94099.6",		0x200000, 0xf1a80e5a, 7 | BRF_SND },           //  7 OKI1 Samples
	{ "rhp94099.7",		0x200000, 0x0d99547e, 7 | BRF_SND },           //  8

	{ "rhp94099.5",		0x200000, 0x515eba93, 7 | BRF_SND },           //  9 OKI2 Samples
	{ "rhp94099.6",		0x200000, 0xf1a80e5a, 7 | BRF_SND },           // 10

	{ "prom1.u19",		0x000100, 0x4299776e, 0 | BRF_OPT },           // 11 Unused proms
	{ "prom2.u53",		0x000100, 0xe6ead349, 0 | BRF_OPT },           // 12
	{ "prom3.u60",		0x000100, 0x304f98c6, 0 | BRF_OPT },           // 13
};

STD_ROM_PICK(raphero)
STD_ROM_FN(raphero)

void __fastcall raphero_main_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x100014:
		case 0x100015:
	//		*flipscreen = data & 1;
		return;

		case 0x100018:
		case 0x100019:
			if ((data & 0xff) != 0xff) {
				*tilebank = data;
			}
		return;

		case 0x10001e:
		case 0x10001f:
		//	bprintf (0, _T("write soundlatch b\n"), address);
			*soundlatch = data;
		return;
	}
}

void __fastcall raphero_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x100014:
	//		*flipscreen = data & 1;
		return;

		case 0x100018:
			if ((data & 0xff) != 0xff) {
				*tilebank = data;
			}
		return;

		case 0x10001e:
		//	bprintf (0, _T("write soundlatch w\n"), address);
			*soundlatch = data & 0xff;
		return;
	}
}

UINT8 __fastcall raphero_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x100000:
		case 0x100001:
			return DrvInputs[0] >> ((~address & 1) << 3);

		case 0x100002:
		case 0x100003:
			return DrvInputs[1] >> ((~address & 1) << 3);

		case 0x100008:
		case 0x100009:
			return DrvDips[0];

		case 0x10000a:
		case 0x10000b:
			return DrvDips[1];

		case 0x10000e:
		case 0x10000f:
		//	bprintf (0, _T("Read soundlatch2 b\n"), address);
			return *soundlatch2;
	}

	return 0;
}

UINT16 __fastcall raphero_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x100000:
			return DrvInputs[0];

		case 0x100002:
			return DrvInputs[1];

		case 0x100008:
			return DrvDips[0];

		case 0x10000a:
			return DrvDips[1];

		case 0x10000e:
		//	bprintf (0, _T("Read soundlatch2 W\n"), address);
			return *soundlatch2;
	}

	return 0;
}

static void raphero_sound_bankswitch(INT32 data)
{
	INT32 nBank = ((data & 0x07) * 0x4000) + 0x10000;

	tlcs90MapMemory(DrvZ80ROM + nBank, 0x8000, 0xbfff, MAP_ROM);
}

static void raphero_sound_write(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0xc000:
		case 0xc001:
			BurnYM2203Write(0, address & 1, data);
		return;

		case 0xc800:
			MSM6295Command(0, data);
		return;

		case 0xc808:
			MSM6295Command(1, data);
		return;

		case 0xc810:
		case 0xc811:
		case 0xc812:
		case 0xc813:
		case 0xc814:
		case 0xc815:
		case 0xc816:
		case 0xc817:
			NMK112_okibank_write(address & 7, data);
		return;

		case 0xd000:
			raphero_sound_bankswitch(data);
		return;

		case 0xd800:
			*soundlatch2 = data;
		return;
	}
}

static UINT8 raphero_sound_read(UINT32 address)
{
	switch (address)
	{
		case 0xc000:
		case 0xc001:
			return BurnYM2203Read(0, address & 1);

		case 0xc800:
			return MSM6295ReadStatus(0);

		case 0xc808:
			return MSM6295ReadStatus(1);

		case 0xd800:
			return *soundlatch;
	}

	return 0;
}

static void RapheroYM2203IrqHandler(INT32, INT32 nStatus)
{
	tlcs90SetIRQLine(0, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

inline static double RapheroGetTime()
{
	return (double)tlcs90TotalCycles() / 8000000;
}

inline static INT32 RapheroSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)(tlcs90TotalCycles() * nSoundRate / 8000000);
}

static INT32 RapheroDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	tlcs90Open(0);
	tlcs90Reset();
	tlcs90Close();

	BurnYM2203Reset();

	MSM6295Reset(0);
	MSM6295Reset(1);

	MSM6295SetInitialBanks(2);

	NMK112Reset();

	return 0;
}

static INT32 RapheroInit()
{
	BurnSetRefreshRate(56.00);

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv68KROM  + 0x000000,  0, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x000000,  1, 1)) return 1;
		memmove (DrvZ80ROM + 0x10000, DrvZ80ROM + 0x00000, 0x20000);

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  3, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x000000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x200000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x400000,  6, 1)) return 1;
		BurnByteswap(DrvGfxROM2, 0x600000);

		if (BurnLoadRom(DrvSndROM0 + 0x000000,  7, 1)) return 1;
		if (BurnLoadRom(DrvSndROM0 + 0x200000,  8, 1)) return 1;

		if (BurnLoadRom(DrvSndROM1 + 0x000000,  9, 1)) return 1;
		if (BurnLoadRom(DrvSndROM1 + 0x200000, 10, 1)) return 1;

		DrvGfxDecode(0x20000, 0x200000, 0x600000);
		memset (DrvGfxROM2 + 0xc00000, 0x0f, 0x400000);
		nGraphicsMask[2] = 0xffff;
	}

	SekInit(0, 0x68000);	
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(DrvPalRAM,		0x120000, 0x1207ff, MAP_RAM);
	SekMapMemory(DrvScrollRAM,	0x130000, 0x1307ff, MAP_RAM);
	SekMapMemory(DrvBgRAM0,		0x140000, 0x143fff, MAP_RAM);
	SekMapMemory(DrvBgRAM1,		0x144000, 0x147fff, MAP_RAM);
	SekMapMemory(DrvBgRAM2,		0x148000, 0x14bfff, MAP_RAM);
	SekMapMemory(DrvBgRAM3,		0x14c000, 0x14ffff, MAP_RAM);
	SekMapMemory(DrvTxRAM,		0x170000, 0x170fff, MAP_RAM);
	SekMapMemory(DrvTxRAM,		0x171000, 0x171fff, MAP_RAM);
	SekMapMemory(Drv68KRAM,		0x1f0000, 0x1fffff, MAP_RAM);
	SekSetWriteWordHandler(0,	raphero_main_write_word);
	SekSetWriteByteHandler(0,	raphero_main_write_byte);
	SekSetReadWordHandler(0,	raphero_main_read_word);
	SekSetReadByteHandler(0,	raphero_main_read_byte);
	SekClose();

	tlcs90Init(0, 8000000);
	tlcs90Open(0);
	tlcs90MapMemory(DrvZ80ROM,	0x0000, 0x7fff, MAP_ROM);
	tlcs90MapMemory(DrvZ80RAM,	0xe000, 0xffff, MAP_RAM);
	tlcs90SetWriteHandler(raphero_sound_write);
	tlcs90SetReadHandler(raphero_sound_read);
	tlcs90Close();

	BurnYM2203Init(1, 1500000, &RapheroYM2203IrqHandler, RapheroSynchroniseStream, RapheroGetTime, 0);
	BurnTimerAttachTlcs90(8000000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE,   1.20, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.50, BURN_SND_ROUTE_BOTH);

	MSM6295Init(0, 4000000 / 165, 1);
	MSM6295Init(1, 4000000 / 165, 1);
	MSM6295SetRoute(0, 0.10, BURN_SND_ROUTE_BOTH);
	MSM6295SetRoute(1, 0.10, BURN_SND_ROUTE_BOTH);

	NMK112_init(0, DrvSndROM0, DrvSndROM1, 0x400000, 0x400000);

	NMK112_enabled = 1;

	no_z80 = 1;

	GenericTilesInit();

	RapheroDoReset();

	return 0;
}

static INT32 RapheroExit()
{
	BurnYM2203Exit();
	MSM6295Exit(0);
	MSM6295Exit(1);
	MSM6295ROM = NULL;

	NMK004_enabled = 0;
	NMK112_enabled = 0;

	tlcs90Exit();

	return CommonExit();
}

static INT32 RapheroFrame()
{
	if (DrvReset) {
		RapheroDoReset();
	}

	{
		DrvInputs[0] = ~0;
		DrvInputs[1] = ~0;
		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	SekNewFrame();
	tlcs90NewFrame();

	INT32 nSegment;
	INT32 nInterleave = 3000;
	INT32 nTotalCycles[2] = { 14000000 / 56, 8000000 / 56 };

	SekOpen(0);
	tlcs90Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		SekRun(nTotalCycles[0] / nInterleave);

		if (i == (nInterleave-16) || i == (nInterleave/2)-16) { // ??
			SekSetIRQLine(1, CPU_IRQSTATUS_AUTO);
		}

		if (i == (nInterleave-1)) {
			SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
		}

		nSegment = (nTotalCycles[1] / nInterleave) * (i + 1);
		BurnTimerUpdate(nSegment);
	}

	BurnTimerEndFrame(nTotalCycles[1]);

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
		MSM6295Render(1, pBurnSoundOut, nBurnSoundLen);
	}

	tlcs90Close();
	SekClose();

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	memcpy (DrvSprBuf2, Drv68KRAM + 0x8000, 0x1000);

	return 0;
}

struct BurnDriver BurnDrvRaphero = {
	"raphero", "arcadian", NULL, NULL, "1994",
	"Rapid Hero\0", NULL, "Media Trading Corp", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, rapheroRomInfo, rapheroRomName, NULL, NULL, Tdragon2InputInfo, RapheroDIPInfo,
	RapheroInit, RapheroExit, RapheroFrame, RapheroDraw, DrvScan, NULL, 0x400,
	224, 384, 3, 4
};

// Arcadia

static struct BurnRomInfo arcadianRomDesc[] = {
	{ "arcadia.3",		0x080000, 0x8b46d609, 1 | BRF_PRG | BRF_ESS }, //  0 68k code

	{ "rhp94099.2",		0x020000, 0xfe01ece1, 2 | BRF_PRG | BRF_ESS }, //  1 Tmp90c841 Code

	{ "arcadia.1",		0x020000, 0x1c2c4008, 3 | BRF_GRA },           //  2 Characters

	{ "rhp94099.4",		0x200000, 0x076eee7b, 4 | BRF_GRA },           //  3 Tiles

	{ "rhp94099.8",		0x200000, 0x49892f07, 5 | BRF_GRA },           //  4 Sprites
	{ "rhp94099.9",		0x200000, 0xea2e47f0, 5 | BRF_GRA },           //  5
	{ "rhp94099.10",	0x200000, 0x512cb839, 5 | BRF_GRA },           //  6

	{ "rhp94099.6",		0x200000, 0xf1a80e5a, 7 | BRF_SND },           //  7 OKI1 Samples
	{ "rhp94099.7",		0x200000, 0x0d99547e, 7 | BRF_SND },           //  8

	{ "rhp94099.5",		0x200000, 0x515eba93, 7 | BRF_SND },           //  9 OKI2 Samples
	{ "rhp94099.6",		0x200000, 0xf1a80e5a, 7 | BRF_SND },           // 10

	{ "prom1.u19",		0x000100, 0x4299776e, 0 | BRF_OPT },           // 11 Unused proms
	{ "prom2.u53",		0x000100, 0xe6ead349, 0 | BRF_OPT },           // 12
	{ "prom3.u60",		0x000100, 0x304f98c6, 0 | BRF_OPT },           // 13
};

STD_ROM_PICK(arcadian)
STD_ROM_FN(arcadian)

struct BurnDriver BurnDrvArcadian = {
	"arcadian", NULL, NULL, NULL, "1994",
	"Arcadia (NMK)\0", NULL, "NMK", "NMK16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, arcadianRomInfo, arcadianRomName, NULL, NULL, Tdragon2InputInfo, RapheroDIPInfo,
	RapheroInit, RapheroExit, RapheroFrame, RapheroDraw, DrvScan, NULL, 0x400,
	224, 384, 3, 4
};
