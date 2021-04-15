// FinalBurn Neo Art and Magic 68K/TMS34010 hardware driver module
// Based on MAME driver by Aaron Giles, Nicola Salmoria

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "tms34_intf.h"
#include "msm6295.h"
#include "tlc34076.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvSndROM;
static UINT8 *DrvNVRAM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvTMSRAM;
static UINT8 *DrvVidRAM[2];

static INT32 tms_irq;
static INT32 hack_irq;
static INT32 oki_bank;

static INT32 blitter_xor[16];
static UINT16 blitter_data[8];
static INT32 blitter_page;
static UINT32 blitter_mask; // gfx size / 2

static UINT8 prot_input[0x10];
static UINT8 prot_output[0x10];
static UINT8 prot_input_index;
static UINT8 prot_output_index;
static UINT8 prot_output_bit;
static UINT8 prot_bit_index;
static UINT8 prot_save;
static void (*protection_callback)() = NULL;

static INT32 is_stonebal = 0;
static INT32 is_ultennis = 0;

static UINT8 DrvRecalc;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvJoy5[8];
static UINT8 DrvDips[2];
static UINT16 DrvInputs[8];
static UINT8 DrvReset;

static struct BurnInputInfo CheesechInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Cheesech)

static struct BurnInputInfo StonebalInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 3"	},

	{"P3 Coin",			BIT_DIGITAL,	DrvJoy3 + 2,	"p3 coin"	},
	{"P3 Start",		BIT_DIGITAL,	DrvJoy4 + 4,	"p3 start"	},
	{"P3 Up",			BIT_DIGITAL,	DrvJoy4 + 0,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	DrvJoy4 + 1,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	DrvJoy4 + 2,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy4 + 3,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy4 + 5,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy4 + 6,	"p3 fire 2"	},
	{"P3 Button 3",		BIT_DIGITAL,	DrvJoy4 + 7,	"p3 fire 3"	},

	{"P4 Coin",			BIT_DIGITAL,	DrvJoy3 + 3,	"p4 coin"	},
	{"P4 Start",		BIT_DIGITAL,	DrvJoy5 + 4,	"p4 start"	},
	{"P4 Up",			BIT_DIGITAL,	DrvJoy5 + 0,	"p4 up"		},
	{"P4 Down",			BIT_DIGITAL,	DrvJoy5 + 1,	"p4 down"	},
	{"P4 Left",			BIT_DIGITAL,	DrvJoy5 + 2,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy5 + 3,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy5 + 5,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy5 + 6,	"p4 fire 2"	},
	{"P4 Button 3",		BIT_DIGITAL,	DrvJoy5 + 7,	"p4 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Stonebal)

static struct BurnDIPInfo CheesechDIPList[]=
{
	{0x13, 0xff, 0xff, 0x5d, NULL					},
	{0x14, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    4, "Language"				},
	{0x13, 0x01, 0x06, 0x00, "French"				},
	{0x13, 0x01, 0x06, 0x02, "Italian"				},
	{0x13, 0x01, 0x06, 0x04, "English"				},
	{0x13, 0x01, 0x06, 0x06, "German"				},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x13, 0x01, 0x18, 0x08, "3"					},
	{0x13, 0x01, 0x18, 0x18, "4"					},
	{0x13, 0x01, 0x18, 0x00, "5"					},
	{0x13, 0x01, 0x18, 0x10, "6"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x13, 0x01, 0x20, 0x20, "Off"					},
	{0x13, 0x01, 0x20, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x13, 0x01, 0xc0, 0xc0, "Easy"					},
	{0x13, 0x01, 0xc0, 0x40, "Normal"				},
	{0x13, 0x01, 0xc0, 0x80, "Hard"					},
	{0x13, 0x01, 0xc0, 0x00, "Very Hard"			},

	{0   , 0xfe, 0   ,    8, "Right Coinage"		},
	{0x14, 0x01, 0x07, 0x02, "6 Coins 1 Credits"	},
	{0x14, 0x01, 0x07, 0x06, "5 Coins 1 Credits"	},
	{0x14, 0x01, 0x07, 0x01, "4 Coins 1 Credits"	},
	{0x14, 0x01, 0x07, 0x05, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0x07, 0x03, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x07, 0x04, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0x07, 0x00, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    8, "Left Coinage"			},
	{0x14, 0x01, 0x38, 0x00, "4 Coins 1 Credits"	},
	{0x14, 0x01, 0x38, 0x20, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x38, 0x18, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0x38, 0x28, "1 Coin  3 Credits"	},
	{0x14, 0x01, 0x38, 0x08, "1 Coin  4 Credits"	},
	{0x14, 0x01, 0x38, 0x30, "1 Coin  5 Credits"	},
	{0x14, 0x01, 0x38, 0x10, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Free Play"			},
	{0x14, 0x01, 0x40, 0x40, "Off"					},
	{0x14, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x14, 0x01, 0x80, 0x80, "Off"					},
	{0x14, 0x01, 0x80, 0x00, "On"					},
};

STDDIPINFO(Cheesech)

static struct BurnDIPInfo UltennisDIPList[]=
{
	{0x13, 0xff, 0xff, 0x5f, NULL					},
	{0x14, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    2, "Button Layout"		},
	{0x13, 0x01, 0x01, 0x01, "Triangular"			},
	{0x13, 0x01, 0x01, 0x00, "Linear"				},

	{0   , 0xfe, 0   ,    2, "Start Set At"			},
	{0x13, 0x01, 0x02, 0x00, "0-0"					},
	{0x13, 0x01, 0x02, 0x02, "4-4"					},

	{0   , 0xfe, 0   ,    2, "Sets Per Match"		},
	{0x13, 0x01, 0x04, 0x04, "1"					},
	{0x13, 0x01, 0x04, 0x00, "3"					},

	{0   , 0xfe, 0   ,    4, "Game Duration"		},
	{0x13, 0x01, 0x18, 0x18, "5 Lost Points"		},
	{0x13, 0x01, 0x18, 0x08, "6 Lost Points"		},
	{0x13, 0x01, 0x18, 0x10, "7 Lost Points"		},
	{0x13, 0x01, 0x18, 0x00, "8 Lost Points"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x13, 0x01, 0x20, 0x20, "Off"					},
	{0x13, 0x01, 0x20, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x13, 0x01, 0xc0, 0xc0, "Easy"					},
	{0x13, 0x01, 0xc0, 0x40, "Normal"				},
	{0x13, 0x01, 0xc0, 0x80, "Hard"					},
	{0x13, 0x01, 0xc0, 0x00, "Very Hard"			},

	{0   , 0xfe, 0   ,    8, "Right Coinage"		},
	{0x14, 0x01, 0x07, 0x02, "6 Coins 1 Credits"	},
	{0x14, 0x01, 0x07, 0x06, "5 Coins 1 Credits"	},
	{0x14, 0x01, 0x07, 0x01, "4 Coins 1 Credits"	},
	{0x14, 0x01, 0x07, 0x05, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0x07, 0x03, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x07, 0x04, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0x07, 0x00, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    8, "Left Coinage"			},
	{0x14, 0x01, 0x38, 0x00, "4 Coins 1 Credits"	},
	{0x14, 0x01, 0x38, 0x20, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x38, 0x18, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0x38, 0x28, "1 Coin  3 Credits"	},
	{0x14, 0x01, 0x38, 0x08, "1 Coin  4 Credits"	},
	{0x14, 0x01, 0x38, 0x30, "1 Coin  5 Credits"	},
	{0x14, 0x01, 0x38, 0x10, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Free Play"			},
	{0x14, 0x01, 0x40, 0x40, "Off"					},
	{0x14, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x14, 0x01, 0x80, 0x80, "Off"					},
	{0x14, 0x01, 0x80, 0x00, "On"					},
};

STDDIPINFO(Ultennis)

static struct BurnDIPInfo StonebalDIPList[]=
{
	{0x25, 0xff, 0xff, 0xff, NULL					},
	{0x26, 0xff, 0xff, 0xfa, NULL					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x25, 0x01, 0x01, 0x01, "Off"					},
	{0x25, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Free Play"			},
	{0x25, 0x01, 0x02, 0x02, "Off"					},
	{0x25, 0x01, 0x02, 0x00, "On"					},

	{0   , 0xfe, 0   ,    8, "Left Coinage"			},
	{0x25, 0x01, 0x1c, 0x00, "4 Coins 1 Credits"	},
	{0x25, 0x01, 0x1c, 0x04, "2 Coins 1 Credits"	},
	{0x25, 0x01, 0x1c, 0x1c, "1 Coin  1 Credits"	},
	{0x25, 0x01, 0x1c, 0x18, "1 Coin  2 Credits"	},
	{0x25, 0x01, 0x1c, 0x14, "1 Coin  3 Credits"	},
	{0x25, 0x01, 0x1c, 0x10, "1 Coin  4 Credits"	},
	{0x25, 0x01, 0x1c, 0x0c, "1 Coin  5 Credits"	},
	{0x25, 0x01, 0x1c, 0x08, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    8, "Right Coinage"		},
	{0x25, 0x01, 0xe0, 0x40, "6 Coins 1 Credits"	},
	{0x25, 0x01, 0xe0, 0x60, "5 Coins 1 Credits"	},
	{0x25, 0x01, 0xe0, 0x80, "4 Coins 1 Credits"	},
	{0x25, 0x01, 0xe0, 0xa0, "3 Coins 1 Credits"	},
	{0x25, 0x01, 0xe0, 0xc0, "2 Coins 1 Credits"	},
	{0x25, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"	},
	{0x25, 0x01, 0xe0, 0x20, "1 Coin  2 Credits"	},
	{0x25, 0x01, 0xe0, 0x00, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x26, 0x01, 0x03, 0x03, "Easy"					},
	{0x26, 0x01, 0x03, 0x02, "Normal"				},
	{0x26, 0x01, 0x03, 0x01, "Hard"					},
	{0x26, 0x01, 0x03, 0x00, "Very Hard"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x26, 0x01, 0x04, 0x04, "Off"					},
	{0x26, 0x01, 0x04, 0x00, "On"					},

	{0   , 0xfe, 0   ,    8, "Match Time"			},
	{0x26, 0x01, 0x38, 0x30, "60s"					},
	{0x26, 0x01, 0x38, 0x28, "70s"					},
	{0x26, 0x01, 0x38, 0x20, "80s"					},
	{0x26, 0x01, 0x38, 0x18, "90s"					},
	{0x26, 0x01, 0x38, 0x38, "100s"					},
	{0x26, 0x01, 0x38, 0x10, "110s"					},
	{0x26, 0x01, 0x38, 0x08, "120s"					},
	{0x26, 0x01, 0x38, 0x00, "130s"					},

	{0   , 0xfe, 0   ,    2, "Free Match Time"		},
	{0x26, 0x01, 0x40, 0x40, "Normal"				},
	{0x26, 0x01, 0x40, 0x00, "Short"				},

	{0   , 0xfe, 0   ,    2, "Game Mode"			},
	{0x26, 0x01, 0x80, 0x80, "4 Players"			},
	{0x26, 0x01, 0x80, 0x00, "2 Players"			},
};

STDDIPINFO(Stonebal)

static void ultennis_protection()
{
	switch (prot_input[0])
	{
		case 0x00:  // reset
			prot_input_index = prot_output_index = 0;
			prot_output[0] = BurnRandom();
			break;

		case 0x01:  // 01 aaaa bbbb cccc dddd (xxxx)
			if (prot_input_index == 9)
			{
				UINT16 a = prot_input[1] | (prot_input[2] << 8);
				UINT16 b = prot_input[3] | (prot_input[4] << 8);
				UINT16 c = prot_input[5] | (prot_input[6] << 8);
				UINT16 d = prot_input[7] | (prot_input[8] << 8);
				UINT16 x = a - b;
				if ((INT16)x >= 0)
					x = (x * c) >> 16;
				else
					x = -(((UINT16)-x * c) >> 16);
				x += d;
				prot_output[0] = x;
				prot_output[1] = x >> 8;
				prot_output_index = 0;
			}
			else if (prot_input_index >= 11)
				prot_input_index = 0;
			break;

		case 0x02:  // 02 aaaa bbbb cccc (xxxxxxxx)
			if (prot_input_index == 7)
			{
				UINT16 a = (INT16)(prot_input[1] | (prot_input[2] << 8));
				UINT16 b = (INT16)(prot_input[3] | (prot_input[4] << 8));
				UINT32 x = a * a * (b/2);
				prot_output[0] = x;
				prot_output[1] = x >> 8;
				prot_output[2] = x >> 16;
				prot_output[3] = x >> 24;
				prot_output_index = 0;
			}
			else if (prot_input_index >= 11)
				prot_input_index = 0;
			break;

		case 0x03:  /* 03 (xxxx) */
			if (prot_input_index == 1)
			{
				UINT16 x = prot_save;
				prot_output[0] = x;
				prot_output[1] = x >> 8;
				prot_output_index = 0;
			}
			else if (prot_input_index >= 3)
				prot_input_index = 0;
			break;

		case 0x04:  // 04 aaaa
			if (prot_input_index == 3)
			{
				UINT16 a = prot_input[1] | (prot_input[2] << 8);
				prot_save = a;
				prot_input_index = prot_output_index = 0;
			}
			break;

		default:
			prot_input_index = prot_output_index = 0;
			break;
	}
}

static void cheesech_protection()
{
	switch (prot_input[0])
	{
		case 0x00:  // reset
			prot_input_index = prot_output_index = 0;
			prot_output[0] = BurnRandom();
			break;

		case 0x01:  // 01 aaaa bbbb (xxxx)
			if (prot_input_index == 5)
			{
				UINT16 a = prot_input[1] | (prot_input[2] << 8);
				UINT16 b = prot_input[3] | (prot_input[4] << 8);
				UINT16 c = 0x4000;      // seems to be hard-coded
				UINT16 d = 0x00a0;      // seems to be hard-coded
				UINT16 x = a - b;
				if ((INT16)x >= 0)
					x = (x * c) >> 16;
				else
					x = -(((UINT16)-x * c) >> 16);
				x += d;
				prot_output[0] = x;
				prot_output[1] = x >> 8;
				prot_output_index = 0;
			}
			else if (prot_input_index >= 7)
				prot_input_index = 0;
			break;

		case 0x03:  // 03 (xxxx)
			if (prot_input_index == 1)
			{
				UINT16 x = prot_save;
				prot_output[0] = x;
				prot_output[1] = x >> 8;
				prot_output_index = 0;
			}
			else if (prot_input_index >= 3)
				prot_input_index = 0;
			break;

		case 0x04:  // 04 aaaa
			if (prot_input_index == 3)
			{
				UINT16 a = prot_input[1] | (prot_input[2] << 8);
				prot_save = a;
				prot_input_index = prot_output_index = 0;
			}
			break;

		default:
			prot_input_index = prot_output_index = 0;
			break;
	}
}

static void stonebal_protection()
{
	switch (prot_input[0])
	{
		case 0x01:  // 01 aaaa bbbb cccc dddd (xxxx)
			if (prot_input_index == 9)
			{
				UINT16 a = prot_input[1] | (prot_input[2] << 8);
				UINT16 b = prot_input[3] | (prot_input[4] << 8);
				UINT16 c = prot_input[5] | (prot_input[6] << 8);
				UINT16 d = prot_input[7] | (prot_input[8] << 8);
				UINT16 x = a - b;
				if ((INT16)x >= 0)
					x = (x * d) >> 16;
				else
					x = -(((UINT16)-x * d) >> 16);
				x += c;
				prot_output[0] = x;
				prot_output[1] = x >> 8;
				prot_output_index = 0;
			}
			else if (prot_input_index >= 11)
				prot_input_index = 0;
			break;

		case 0x02:  // 02 aaaa (xx)
			if (prot_input_index == 3)
			{
				UINT8 x = 0xa5;
				prot_output[0] = x;
				prot_output_index = 0;
			}
			else if (prot_input_index >= 4)
				prot_input_index = 0;
			break;

		case 0x03:  // 03 (xxxx)
			if (prot_input_index == 1)
			{
				UINT16 x = prot_save;
				prot_output[0] = x;
				prot_output[1] = x >> 8;
				prot_output_index = 0;
			}
			else if (prot_input_index >= 3)
				prot_input_index = 0;
			break;

		case 0x04:  // 04 aaaa
			if (prot_input_index == 3)
			{
				UINT16 a = prot_input[1] | (prot_input[2] << 8);
				prot_save = a;
				prot_input_index = prot_output_index = 0;
			}
			break;

		default:
			prot_input_index = prot_output_index = 0;
			break;
	}
}

static void protection_bit_write(INT32 offset)
{
	prot_input[prot_input_index] <<= 1;
	prot_input[prot_input_index] |= offset;

	prot_output_bit = prot_output[prot_output_index] & 0x01;
	prot_output[prot_output_index] >>= 1;

	if (++prot_bit_index == 8)
	{
		prot_input_index++;
		prot_output_index++;
		prot_bit_index = 0;

		if (protection_callback) {
			protection_callback();
		}
	}
}

static void set_oki_bank(INT32 data)
{
	if ((data & 1) == 0)
	{
		oki_bank = data & 0x10;
		INT32 bank = oki_bank ? 0x40000 : 0;

		MSM6295SetBank(0, DrvSndROM + bank, 0, 0x3ffff);
	}
}

static void sync_cpu()
{
	INT32 cyc = (((INT64)SekTotalCycles() * (40000000 / 8)) / 12500000) - TMS34010TotalCycles();
	if (cyc > 0) {
		TMS34010Run(cyc);
	}
}

static void update_irq_state();

static void hack_irq_off()
{
	if (hack_irq) {
		SekRun(1);
		hack_irq = 0;
		update_irq_state();
	}
}

static void sync_m68k()
{
	INT32 cyc = (((INT64)TMS34010TotalCycles() * 12500000) / (40000000 / 8)) - SekTotalCycles();
	if (cyc > 0) {
		hack_irq_off();
		SekRun(cyc);
	}
}

static void __fastcall artmagic_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x300000:
			set_oki_bank(data);
		return;

		case 0x300002:
			// control?
		return;

		case 0x300009:
			// coin counter
		return;

		case 0x300004:
		case 0x300006:
			protection_bit_write((address & 2) >> 1);
		return;

		case 0x340000: // stonebal
		case 0x360000:
			MSM6295Write(0, data & 0xff);
		return;

		case 0x380000:
		case 0x380002:
		case 0x380004:
		case 0x380006:
			sync_cpu();
			TMS34010HostWrite((address / 2) & 3, data);
		return;
	}
}

static void __fastcall artmagic_main_write_byte(UINT32 address, UINT8 data)
{
	artmagic_main_write_word(address & ~1, data << ((~address & 1) * 8));
}

static UINT16 __fastcall artmagic_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x30000a:
			return 0xfffc | prot_output_bit;

		case 0x300000:
			if (is_ultennis) {
				UINT32 pc = SekGetPC(-1);
				if (pc == 0x18c2 || pc == 0x18e4) {
					hack_irq = 1;
					update_irq_state();
					SekRunEnd();
				}
			}
		case 0x300002:
		case 0x300004:
		case 0x300006:
		case 0x300008:
		case 0x30000c: // stonebal
		case 0x30000e: // stonebal
			return DrvInputs[(address / 2) & 7];

		case 0x340000:
		case 0x360000:
			return MSM6295Read(0);

		case 0x380000:
		case 0x380002:
		case 0x380004:
		case 0x380006:
			sync_cpu();
			return TMS34010HostRead((address / 2) & 3);
	}

	return 0;
}

static UINT8 __fastcall artmagic_main_read_byte(UINT32 address)
{
	return artmagic_main_read_word(address & ~1) >> ((~address & 1) * 8);
}

static inline UINT16 *address_to_vram(UINT32 *address)
{
	UINT16 *vram[2] = { (UINT16*)DrvVidRAM[0], (UINT16*)DrvVidRAM[1] };

	UINT32 original = *address;
	*address = TOWORD(original & 0x001fffff);

	if (original < 0x001fffff)
		return vram[0];
	else if (original >= 0x00400000 && original < 0x005fffff)
		return vram[1];

	return NULL;
}

static void execute_blit()
{
	UINT16 *blitter_base = (UINT16*)DrvGfxROM;
	UINT16 *vram[2] = { (UINT16*)DrvVidRAM[0], (UINT16*)DrvVidRAM[1] };

	UINT16 *dest = vram[blitter_page ^ 1];
	INT32 offset = ((blitter_data[1] & 0xff) << 16) | blitter_data[0];
	INT32 color = (blitter_data[1] >> 4) & 0xf0;
	INT32 x = (INT16)blitter_data[2];
	INT32 y = (INT16)blitter_data[3];
	INT32 maskx = blitter_data[6] & 0xff;
	INT32 masky = blitter_data[6] >> 8;
	INT32 w = ((blitter_data[7] & 0xff) + 1) * 4;
	INT32 h = (blitter_data[7] >> 8) + 1;
	INT32 j;

	INT32 last = 0;
	INT32 sy = y;
	for (INT32 i = 0; i < h; i++)
	{
		if ((i & 1) || !((masky << ((i/2) & 7)) & 0x80))
		{
			if (sy >= 0 && sy < 256)
			{
				INT32 tsy = sy * 0x200;
				INT32 sx = x;

				last = 0;
				if (i == 0) /* first line */
				{
					/* ultennis, stonebal */
					last ^= (blitter_data[7] & 0x0001);
					if (is_stonebal)
						last ^= ((blitter_data[0] & 0x0020) >> 3);
					else    /* ultennis */
						last ^= (((blitter_data[0] + 1) & 0x0040) >> 4);

					/* cheesech */
					last ^= ((blitter_data[7] & 0x0400) >> 9);
					last ^= ((blitter_data[0] & 0x2000) >> 10);
				}
				else    /* following lines */
				{
					INT32 val = BURN_ENDIAN_SWAP_INT16(blitter_base[offset & blitter_mask]);

					/* ultennis, stonebal */
					last ^= 4;
					last ^= ((val & 0x0400) >> 8);
					last ^= ((val & 0x5000) >> 12);

					/* cheesech */
					last ^= 8;
					last ^= ((val & 0x0800) >> 8);
					last ^= ((val & 0xa000) >> 12);
				}

				for (j = 0; j < w; j += 4)
				{
					UINT16 val = BURN_ENDIAN_SWAP_INT16(blitter_base[(offset + j/4) & blitter_mask]);
					if (sx < 508)
					{
						if (h == 1 && is_stonebal)
							last = ((val) >>  0) & 0xf;
						else
							last = ((val ^ blitter_xor[last]) >>  0) & 0xf;
						if (!((maskx << ((j/2) & 7)) & 0x80))
						{
							if (last && sx >= 0 && sx < 512)
								dest[tsy + sx] = color | (last);
							sx++;
						}

						if (h == 1 && is_stonebal)
							last = ((val) >>  4) & 0xf;
						else
							last = ((val ^ blitter_xor[last]) >>  4) & 0xf;
						{
							if (last && sx >= 0 && sx < 512)
								dest[tsy + sx] = color | (last);
							sx++;
						}

						if (h == 1 && is_stonebal)
							last = ((val) >>  8) & 0xf;
						else
							last = ((val ^ blitter_xor[last]) >>  8) & 0xf;
						if (!((maskx << ((j/2) & 7)) & 0x40))
						{
							if (last && sx >= 0 && sx < 512)
								dest[tsy + sx] = color | (last);
							sx++;
						}

						if (h == 1 && is_stonebal)
							last = ((val) >> 12) & 0xf;
						else
							last = ((val ^ blitter_xor[last]) >> 12) & 0xf;
						{
							if (last && sx >= 0 && sx < 512)
								dest[tsy + sx] = color | (last);
							sx++;
						}
					}
				}
			}
			sy++;
		}
		offset += w/4;
	}
}

static UINT16 artmagic_blitter_read(UINT32 )
{
	return 0xffef | (blitter_page << 4);
}

static void artmagic_blitter_write(UINT32 address, UINT16 data)
{
	INT32 offset = (address >> 4) & 7;
	blitter_data[offset] = data;

	switch (offset) {
		case 3: execute_blit(); break;
		case 4: blitter_page = (data >> 1) & 1; break;
	}
}

static void artmagic_palette_write(UINT32 address, UINT16 data)
{
	tlc34076_write((address >> 4) & 0xf, data);
}

static UINT16 artmagic_palette_read(UINT32 address)
{
	return tlc34076_read((address >> 4) & 0xf);
}

static void artmagic_to_shiftreg(UINT32 address, UINT16 *shiftreg)
{
	UINT16 *vram = address_to_vram(&address);
	if (vram) {
		memcpy(shiftreg, &vram[address], 0x400);
	}
}

static void artmagic_from_shiftreg(UINT32 address, UINT16 *shiftreg)
{
	UINT16 *vram = address_to_vram(&address);
	if (vram) {
		memcpy(&vram[address], shiftreg, 0x400);
	}
}

static INT32 scanline_callback(INT32 line, TMS34010Display *params)
{
	line -= params->veblnk; // top clipping
	if (line < 0 || line >= nScreenHeight) return 0;

	UINT32 offset = (params->rowaddr << 12) & 0x7ff000;
	UINT16 *vram = address_to_vram(&offset);
	if (!vram) return 0;
	vram += offset;
	UINT16 *dest = (UINT16*)pTransDraw + (line * nScreenWidth);
	INT32 coladdr = params->coladdr << 1;

	for (INT32 x = params->heblnk; x < params->hsblnk; x++) {
		INT32 ex = x - params->heblnk;
		if (ex >= 0 && ex < nScreenWidth) {
			dest[ex] = vram[coladdr++ & 0x1ff] & 0xff;
		}
	}

	return 0;
}

static void update_irq_state()
{
	SekSetVIRQLine(4, tms_irq  ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
	SekSetVIRQLine(5, hack_irq ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static void m68k_gen_int(INT32 state)
{
	sync_m68k();
	tms_irq = state;
	update_irq_state();
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	TMS34010Open(0);
	TMS34010Reset();
	TMS34010Close();

	set_oki_bank(0);
	MSM6295Reset(0);

	tlc34076_reset(6);

	tms_irq = 0;
	hack_irq = 0;

	memset (blitter_data, 0, sizeof(blitter_data));
	blitter_page = 0;

	memset (prot_input, 0, sizeof(prot_input));
	memset (prot_output, 0, sizeof(prot_output));
	prot_input_index = 0;
	prot_output_index = 0;
	prot_output_bit = 0;
	prot_bit_index = 0;
	prot_save = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x080000;

	DrvGfxROM		= Next; Next += 0x400000;

	DrvSndROM		= Next; Next += 0x080000;

	pBurnDrvPalette	= (UINT32*)Next; Next += 0x0100 * sizeof(UINT32);

	DrvNVRAM		= Next; Next += 0x001000;

	AllRam			= Next;

	Drv68KRAM		= Next; Next += 0x080000;
	DrvTMSRAM		= Next; Next += 0x040000 * 2; // TMS: size in words
	DrvVidRAM[0]	= Next; Next += 0x020000 * 2;
	DrvVidRAM[1]	= Next; Next += 0x020000 * 2;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void decrypt_ultennis()
{
	for (INT32 i = 0; i < 16; i++)
	{
		blitter_xor[i] = 0x0462;
		if (i & 1) blitter_xor[i] ^= 0x0011;
		if (i & 2) blitter_xor[i] ^= 0x2200;
		if (i & 4) blitter_xor[i] ^= 0x4004;
		if (i & 8) blitter_xor[i] ^= 0x0880;
	}
}

static void decrypt_cheesech()
{
	for (INT32 i = 0; i < 16; i++)
	{
		blitter_xor[i] = 0x0891;
		if (i & 1) blitter_xor[i] ^= 0x1100;
		if (i & 2) blitter_xor[i] ^= 0x0022;
		if (i & 4) blitter_xor[i] ^= 0x0440;
		if (i & 8) blitter_xor[i] ^= 0x8008;
	}
}

static INT32 DrvInit(INT32 game_select)
{
	BurnSetRefreshRate(49.76);

	BurnAllocMemIndex();

	INT32 k = 0;

	switch (game_select)
	{
		case 0: // cheesech
		{
			if (BurnLoadRom(Drv68KROM + 0x000001, k++, 2)) return 1;
			if (BurnLoadRom(Drv68KROM + 0x000000, k++, 2)) return 1;

			if (BurnLoadRom(DrvGfxROM + 0x000000, k++, 2)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x000001, k++, 2)) return 1;

			if (BurnLoadRom(DrvSndROM + 0x000000, k++, 1)) return 1;

			decrypt_cheesech();
			protection_callback = cheesech_protection;
			blitter_mask = (0x100000 / 2) - 1;
		}
		break;

		case 1: // ultennis
		{
			if (BurnLoadRom(Drv68KROM + 0x000001, k++, 2)) return 1;
			if (BurnLoadRom(Drv68KROM + 0x000000, k++, 2)) return 1;

			if (BurnLoadRom(DrvGfxROM + 0x000000, k++, 1)) return 1;

			if (BurnLoadRom(DrvSndROM + 0x000000, k++, 1)) return 1;
			memcpy (DrvSndROM + 0x40000, DrvSndROM, 0x40000); // no banks

			decrypt_ultennis();
			protection_callback = ultennis_protection;
			blitter_mask = (0x200000 / 2) - 1;
			is_ultennis = 1;
		}
		break;

		case 2: // stonebal
		{
			if (BurnLoadRom(Drv68KROM + 0x000001, k++, 2)) return 1;
			if (BurnLoadRom(Drv68KROM + 0x000000, k++, 2)) return 1;

			if (BurnLoadRom(DrvGfxROM + 0x000000, k++, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x200000, k++, 1)) return 1;

			if (BurnLoadRom(DrvSndROM + 0x000000, k++, 1)) return 1;

			decrypt_ultennis();
			protection_callback = stonebal_protection;
			blitter_mask = (0x400000 / 2) - 1;
			is_stonebal = 1;
		}
		break;
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x07ffff, MAP_ROM);
	if (is_stonebal) {
		SekMapMemory(Drv68KRAM,			0x200000, 0x27ffff, MAP_RAM);
		SekMapMemory(DrvNVRAM,			0x280000, 0x280fff, MAP_RAM);
	} else {
		SekMapMemory(Drv68KRAM,			0x220000, 0x23ffff, MAP_RAM);
		SekMapMemory(DrvNVRAM,			0x240000, 0x240fff, MAP_RAM);
	}
	SekSetWriteWordHandler(0,				artmagic_main_write_word);
	SekSetWriteByteHandler(0,				artmagic_main_write_byte);
	SekSetReadWordHandler(0,				artmagic_main_read_word);
	SekSetReadByteHandler(0,				artmagic_main_read_byte);
	SekClose();

	TMS34010Init(0);
	TMS34010Open(0);
	TMS34010MapMemory(DrvVidRAM[0],	0x00000000, 0x001fffff, MAP_READ | MAP_WRITE);
	TMS34010MapMemory(DrvVidRAM[1],	0x00400000, 0x005fffff, MAP_READ | MAP_WRITE);
	if (is_stonebal) {
		TMS34010MapMemory(DrvTMSRAM,	0xffc00000, 0xffffffff, MAP_READ | MAP_WRITE);
	} else {
		TMS34010MapMemory(DrvTMSRAM,	0xffe00000, 0xffffffff, MAP_READ | MAP_WRITE);
	}
	TMS34010SetHandlers(1, 			artmagic_blitter_read, artmagic_blitter_write);
	TMS34010MapHandler(1, 			0x00800000, 0x0080007f, MAP_READ | MAP_WRITE);
	TMS34010SetHandlers(2, 			artmagic_palette_read, artmagic_palette_write);
	TMS34010MapHandler(2, 			0x00c00000, 0x00c000ff, MAP_READ | MAP_WRITE);

	TMS34010SetPixClock(40000000 / 6, 1);
	TMS34010SetCpuCyclesPerFrame((INT32)(40000000 / 8 / 49.76));
	TMS34010SetToShift(artmagic_to_shiftreg);
	TMS34010SetFromShift(artmagic_from_shiftreg);
	TMS34010SetOutputINT(m68k_gen_int);
	TMS34010SetHaltOnReset(1);
	TMS34010SetScanlineRender(scanline_callback);
	TMS34010Close();

	MSM6295Init(0, (40000000 / 3 / 10) / MSM6295_PIN7_LOW, 0);
	MSM6295SetRoute(0, 0.65, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	memset(DrvNVRAM, 0xff, 0x1000); // default nvram fill

	return 0;
}

static INT32 DrvExit()
{
	SekExit();
	MSM6295Exit();
	TMS34010Exit();

	GenericTilesExit();

	BurnFreeMemIndex();

	is_stonebal = 0;
	is_ultennis = 0;

	return 0;
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		tlc34076_recalc_palette();
		DrvRecalc = 0;
	}

	BurnTransferCopy(pBurnDrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		DrvInputs[0] = 0xff00;					// 300000 - p1
		DrvInputs[1] = 0xff00;					// 300002 - p2
		DrvInputs[2] = 0xff00 | DrvDips[0];		// 300004 - dsw1
		DrvInputs[3] = 0xff00 | DrvDips[1];		// 300006 - dsw2
		DrvInputs[4] = 0xfff0;					// 300008 - coin
		DrvInputs[5] = 0xfffc;					// 30000a - protection read
		DrvInputs[6] = 0xff00;					// 30000c - p3
		DrvInputs[7] = 0xff00;					// 30000e - p4

		for (INT32 i = 0; i < 8; i++)
		{
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i; // p1
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i; // p2
			DrvInputs[4] ^= (DrvJoy3[i] & 1) << i; // coin
			DrvInputs[6] ^= (DrvJoy4[i] & 1) << i; // p3
			DrvInputs[7] ^= (DrvJoy5[i] & 1) << i; // p4
		}
	}

	TMS34010NewFrame();
	SekNewFrame();

	INT32 nInterleave = 312;
	INT32 nCyclesTotal[2] = { (INT32)(12500000 / 49.76), (INT32)(40000000 / 8 / 49.76) };
	INT32 nCyclesDone[2] = { 0, 0 };

	SekOpen(0);
	TMS34010Open(0);

	for (INT32 i = 0; i < nInterleave; i++) {
		hack_irq_off();
		CPU_RUN_SYNCINT(0, Sek);
		CPU_RUN_SYNCINT(1, TMS34010);

		TMS34010GenerateScanline(i);
    }

	TMS34010Close();
	SekClose();

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	if (pBurnSoundOut) {
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
    }

    return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029704;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);
		TMS34010Scan(nAction);

		tlc34076_Scan(nAction);

		MSM6295Scan(nAction, pnMin);

		BurnRandomScan(nAction);

		SCAN_VAR(tms_irq);
		SCAN_VAR(hack_irq);

		SCAN_VAR(blitter_data);
		SCAN_VAR(blitter_page);

		SCAN_VAR(prot_input);
		SCAN_VAR(prot_output);
		SCAN_VAR(prot_input_index);
		SCAN_VAR(prot_output_index);
		SCAN_VAR(prot_output_bit);
		SCAN_VAR(prot_bit_index);
		SCAN_VAR(prot_save);
	}

	if (nAction & ACB_NVRAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data		= DrvNVRAM;
		ba.nLen		= 0x1000;
		ba.szName	= "NV RAM";
		BurnAcb(&ba);
	}

	return 0;
}


// Cheese Chase

static struct BurnRomInfo cheesechRomDesc[] = {
	{ "u102",								0x40000, 0x1d6e07c5, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "u101",								0x40000, 0x30ae9f95, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "u134",								0x80000, 0x354ba4a6, 2 | BRF_GRA },           //  2 Blitter Data
	{ "u135",								0x80000, 0x97348681, 2 | BRF_GRA },           //  3

	{ "u151",								0x80000, 0x65d5ebdb, 3 | BRF_SND },           //  4 MSM6295 Samples (Banked)
};

STD_ROM_PICK(cheesech)
STD_ROM_FN(cheesech)

static INT32 CheesechInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvCheesech = {
	"cheesech", NULL, NULL, NULL, "1994",
	"Cheese Chase\0", NULL, "Art & Magic", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MAZE, 0,
	NULL, cheesechRomInfo, cheesechRomName, NULL, NULL, NULL, NULL, CheesechInputInfo, CheesechDIPInfo,
	CheesechInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 256, 4, 3
};


// Ultimate Tennis

static struct BurnRomInfo ultennisRomDesc[] = {
	{ "a+m001b1093_13b_u102.u102",			0x040000, 0xec31385e, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "a+m001b1093_12b_u101.u101",			0x040000, 0x08a7f655, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "a+m-001-01-a.ic133",					0x200000, 0x29d9204d, 2 | BRF_GRA },           //  2 Blitter Data

	{ "a+m001b1093_14a_u151.u151",			0x040000, 0x4e19ca89, 3 | BRF_SND },           //  3 MSM6295 Samples
};

STD_ROM_PICK(ultennis)
STD_ROM_FN(ultennis)

static INT32 UltennisInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvUltennis = {
	"ultennis", NULL, NULL, NULL, "1993",
	"Ultimate Tennis\0", NULL, "Art & Magic", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, ultennisRomInfo, ultennisRomName, NULL, NULL, NULL, NULL, CheesechInputInfo, UltennisDIPInfo,
	UltennisInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 256, 4, 3
};


// Ultimate Tennis (v 1.4, Japan)

static struct BurnRomInfo ultennisjRomDesc[] = {
	{ "a+m001d0194-13c-u102-japan.u102",	0x040000, 0x65cee452, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "a+m001d0194-12c-u101-japan.u101",	0x040000, 0x5f4b0ca0, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "a+m-001-01-a.ic133",					0x200000, 0x29d9204d, 2 | BRF_GRA },           //  2 Blitter Data

	{ "a+m001c1293-14a-u151.u151",			0x040000, 0x4e19ca89, 3 | BRF_SND },           //  3 MSM6295 Samples
};

STD_ROM_PICK(ultennisj)
STD_ROM_FN(ultennisj)

struct BurnDriver BurnDrvUltennisj = {
	"ultennisj", "ultennis", NULL, NULL, "1993",
	"Ultimate Tennis (v 1.4, Japan)\0", NULL, "Art & Magic (Banpresto license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, ultennisjRomInfo, ultennisjRomName, NULL, NULL, NULL, NULL, CheesechInputInfo, UltennisDIPInfo,
	UltennisInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 256, 4, 3
};


// Stone Ball (4 Players, v1-20 13/12/1994)

static struct BurnRomInfo stonebalRomDesc[] = {
	{ "u102",								0x040000, 0x712feda1, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "u101",								0x040000, 0x4f1656a9, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "u1600.bin",							0x200000, 0xd2ffe9ff, 2 | BRF_GRA },           //  2 Blitter Data
	{ "u1601.bin",							0x200000, 0xdbe893f0, 2 | BRF_GRA },           //  3

	{ "sb_snd_9-9-94.u1801",				0x080000, 0xd98f7378, 3 | BRF_SND },           //  4 MSM6295 Samples (Banked)
};

STD_ROM_PICK(stonebal)
STD_ROM_FN(stonebal)

static INT32 StonebalInit()
{
	return DrvInit(2);
}

struct BurnDriver BurnDrvStonebal = {
	"stonebal", NULL, NULL, NULL, "1994",
	"Stone Ball (4 Players, v1-20 13/12/1994)\0", NULL, "Art & Magic", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, stonebalRomInfo, stonebalRomName, NULL, NULL, NULL, NULL, StonebalInputInfo, StonebalDIPInfo,
	StonebalInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 256, 4, 3
};


// Stone Ball (2 Players, v1-20 7/11/1994)

static struct BurnRomInfo stonebal2RomDesc[] = {
	{ "u102.bin",							0x040000, 0xb3c4f64f, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "u101.bin",							0x040000, 0xfe373f74, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "u1600.bin",							0x200000, 0xd2ffe9ff, 2 | BRF_GRA },           //  2 Blitter Data
	{ "u1601.bin",							0x200000, 0xdbe893f0, 2 | BRF_GRA },           //  3

	{ "sb_snd_9-9-94.u1801",				0x080000, 0xd98f7378, 3 | BRF_SND },           //  4 MSM6295 Samples (Banked)
};

STD_ROM_PICK(stonebal2)
STD_ROM_FN(stonebal2)

struct BurnDriver BurnDrvStonebal2 = {
	"stonebal2", "stonebal", NULL, NULL, "1994",
	"Stone Ball (2 Players, v1-20 7/11/1994)\0", NULL, "Art & Magic", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, stonebal2RomInfo, stonebal2RomName, NULL, NULL, NULL, NULL, StonebalInputInfo, StonebalDIPInfo,
	StonebalInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 256, 4, 3
};


// Stone Ball (2 Players, v1-20 21/10/1994)

static struct BurnRomInfo stonebal2oRomDesc[] = {
	{ "sb_o_2p_24-10.u102",					0x040000, 0xab58c6b2, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "sb_e_2p_24-10.u101",					0x040000, 0xea967835, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "u1600.bin",							0x200000, 0xd2ffe9ff, 2 | BRF_GRA },           //  2 Blitter Data
	{ "u1601.bin",							0x200000, 0xdbe893f0, 2 | BRF_GRA },           //  3

	{ "sb_snd_9-9-94.u1801",				0x080000, 0xd98f7378, 3 | BRF_SND },           //  4 MSM6295 Samples (Banked)
};

STD_ROM_PICK(stonebal2o)
STD_ROM_FN(stonebal2o)

struct BurnDriver BurnDrvStonebal2o = {
	"stonebal2o", "stonebal", NULL, NULL, "1994",
	"Stone Ball (2 Players, v1-20 21/10/1994)\0", NULL, "Art & Magic", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, stonebal2oRomInfo, stonebal2oRomName, NULL, NULL, NULL, NULL, StonebalInputInfo, StonebalDIPInfo,
	StonebalInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 256, 4, 3
};
