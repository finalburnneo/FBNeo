// FinalBurn Neo Tube Panic driver module
// Based on MAME 0.128 driver by Jarek Burczynski

#include "tiles_generic.h"
#include "z80_intf.h"
#include "m6800_intf.h"
#include "ay8910.h"
#include "msm5205.h"
#include "resnet.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvZ80ROM[3];
static UINT8 *DrvMCUROM;
static UINT8 *DrvGfxROM[3];
static UINT8 *DrvColPROM;
static UINT8 *DrvBgRAM;
static UINT8 *DrvSprColRAM;
static UINT8 *DrvTxtRAM;
static UINT8 *DrvZ80RAM[3];
static UINT8 *DrvShareRAM[2];
static UINT8 *DrvFrameBuffers;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 soundlatch;
static INT32 color_A4;
static INT32 background_romsel;
static INT32 ls175_b7;
static INT32 ls175_e8;
static INT32 romEF_addr;
static INT32 HINV;
static INT32 VINV;
static INT32 XSize;
static INT32 YSize;
static INT32 mark_1;
static INT32 mark_2;
static INT32 ls273_g6;
static INT32 ls273_j6;
static INT32 romHI_addr_mid;
static INT32 romHI_addr_msb;
static INT32 romD_addr;
static INT32 E16_add_b;
static INT32 colorram_addr_hi;
static INT32 framebuffer_select;
static INT32 sprite_timer;

static INT32 page;
static INT32 ls377_data;
static INT32 ls377;
static INT32 ls74;

static INT32 rjammer = 0;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo TubepInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"		},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"		},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"			},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"		},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"		},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"		},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"		},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"		},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"		},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"			},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"		},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"		},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"		},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"		},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"			},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"			},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"			},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"			},
};

STDINPUTINFO(Tubep)

static struct BurnInputInfo RjammerInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"		},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"		},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"			},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"		},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 left"		},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 right"		},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 1"		},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 2"		},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 3"		},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"		},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 start"		},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"			},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"		},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 left"		},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 right"		},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 1"		},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 2"		},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 3"		},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"			},
	{"Service 1",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy3 + 3,	"diag"			},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"			},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"			},
};

STDINPUTINFO(Rjammer)

static struct BurnDIPInfo TubepDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x7f, NULL					},
	{0x10, 0xff, 0xff, 0x6e, NULL					},
	{0x11, 0xff, 0xff, 0x7e, NULL					},

	{0   , 0xfe, 0   ,    8, "Coin A"				},
	{0x0f, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x0f, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x0f, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x0f, 0x01, 0x07, 0x04, "1 Coin  4 Credits"	},
	{0x0f, 0x01, 0x07, 0x03, "1 Coin  5 Credits"	},
	{0x0f, 0x01, 0x07, 0x02, "1 Coin  6 Credits"	},
	{0x0f, 0x01, 0x07, 0x01, "1 Coin  7 Credits"	},
	{0x0f, 0x01, 0x07, 0x00, "1 Coin  8 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"				},
	{0x0f, 0x01, 0x38, 0x00, "8 Coins 1 Credits"	},
	{0x0f, 0x01, 0x38, 0x08, "7 Coins 1 Credits"	},
	{0x0f, 0x01, 0x38, 0x10, "6 Coins 1 Credits"	},
	{0x0f, 0x01, 0x38, 0x18, "5 Coins 1 Credits"	},
	{0x0f, 0x01, 0x38, 0x20, "4 Coins 1 Credits"	},
	{0x0f, 0x01, 0x38, 0x28, "3 Coins 1 Credits"	},
	{0x0f, 0x01, 0x38, 0x30, "2 Coins 1 Credits"	},
	{0x0f, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x10, 0x01, 0x03, 0x03, "2"					},
	{0x10, 0x01, 0x03, 0x02, "3"					},
	{0x10, 0x01, 0x03, 0x01, "4"					},
	{0x10, 0x01, 0x03, 0x00, "5"					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x10, 0x01, 0x0c, 0x0c, "40000"				},
	{0x10, 0x01, 0x0c, 0x08, "50000"				},
	{0x10, 0x01, 0x0c, 0x04, "60000"				},
	{0x10, 0x01, 0x0c, 0x00, "80000"				},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x10, 0x01, 0x10, 0x00, "Upright"				},
	{0x10, 0x01, 0x10, 0x10, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Service mode"			},
	{0x10, 0x01, 0x20, 0x20, "Off"					},
	{0x10, 0x01, 0x20, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x11, 0x01, 0x01, 0x01, "Off"					},
	{0x11, 0x01, 0x01, 0x00, "On"					},
};

STDDIPINFO(Tubep)

static struct BurnDIPInfo TubepbDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x7f, NULL					},
	{0x10, 0xff, 0xff, 0x6e, NULL					},
	{0x11, 0xff, 0xff, 0x7e, NULL					},

	{0   , 0xfe, 0   ,    8, "Coin A"				},
	{0x0f, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x0f, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x0f, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x0f, 0x01, 0x07, 0x04, "1 Coin  4 Credits"	},
	{0x0f, 0x01, 0x07, 0x03, "1 Coin  5 Credits"	},
	{0x0f, 0x01, 0x07, 0x02, "1 Coin  6 Credits"	},
	{0x0f, 0x01, 0x07, 0x01, "1 Coin  7 Credits"	},
	{0x0f, 0x01, 0x07, 0x00, "1 Coin  8 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"				},
	{0x0f, 0x01, 0x38, 0x00, "8 Coins 1 Credits"	},
	{0x0f, 0x01, 0x38, 0x08, "7 Coins 1 Credits"	},
	{0x0f, 0x01, 0x38, 0x10, "6 Coins 1 Credits"	},
	{0x0f, 0x01, 0x38, 0x18, "5 Coins 1 Credits"	},
	{0x0f, 0x01, 0x38, 0x20, "4 Coins 1 Credits"	},
	{0x0f, 0x01, 0x38, 0x28, "3 Coins 1 Credits"	},
	{0x0f, 0x01, 0x38, 0x30, "2 Coins 1 Credits"	},
	{0x0f, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x10, 0x01, 0x03, 0x03, "2"					},
	{0x10, 0x01, 0x03, 0x02, "3"					},
	{0x10, 0x01, 0x03, 0x01, "4"					},
	{0x10, 0x01, 0x03, 0x00, "5"					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x10, 0x01, 0x0c, 0x0c, "10000"				},
	{0x10, 0x01, 0x0c, 0x08, "20000"				},
	{0x10, 0x01, 0x0c, 0x04, "30000"				},
	{0x10, 0x01, 0x0c, 0x00, "40000"				},

	{0   , 0xfe, 0   ,    2, "Service mode"			},
	{0x10, 0x01, 0x20, 0x20, "Off"					},
	{0x10, 0x01, 0x20, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x11, 0x01, 0x01, 0x01, "Off"					},
	{0x11, 0x01, 0x01, 0x00, "On"					},
};

STDDIPINFO(Tubepb)

static struct BurnDIPInfo RjammerDIPList[]=
{
	{0x15, 0xff, 0xff, 0x7e, NULL					},
	{0x16, 0xff, 0xff, 0x7e, NULL					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x15, 0x01, 0x01, 0x00, "Upright"				},
	{0x15, 0x01, 0x01, 0x01, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Unused"				},
	{0x15, 0x01, 0x02, 0x02, "Off"					},
	{0x15, 0x01, 0x02, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x15, 0x01, 0x0c, 0x00, "3 Coins 1 Credits"	},
	{0x15, 0x01, 0x0c, 0x04, "2 Coins 3 Credits"	},
	{0x15, 0x01, 0x0c, 0x0c, "1 Coin  3 Credits"	},
	{0x15, 0x01, 0x0c, 0x08, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin A"				},
	{0x15, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x15, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},
	{0x15, 0x01, 0x30, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x16, 0x01, 0x01, 0x01, "Off"					},
	{0x16, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Bonus Time"			},
	{0x16, 0x01, 0x02, 0x02, "100"					},
	{0x16, 0x01, 0x02, 0x00, "200"					},

	{0   , 0xfe, 0   ,    4, "Clear Men"			},
	{0x16, 0x01, 0x0c, 0x0c, "20"					},
	{0x16, 0x01, 0x0c, 0x08, "30"					},
	{0x16, 0x01, 0x0c, 0x04, "40"					},
	{0x16, 0x01, 0x0c, 0x00, "50"					},

	{0   , 0xfe, 0   ,    2, "Difficulty"			},
	{0x16, 0x01, 0x10, 0x10, "Easy"					},
	{0x16, 0x01, 0x10, 0x00, "Hard"					},

	{0   , 0xfe, 0   ,    2, "Time"					},
	{0x16, 0x01, 0x20, 0x20, "40"					},
	{0x16, 0x01, 0x20, 0x00, "50"					},
};

STDDIPINFO(Rjammer)

static void __fastcall tubep_main_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x80:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;

		case 0xb0:
		case 0xb1:
			// coin counters
		return;

		case 0xb2: // n/c?
		case 0xb3: // n/c?
		case 0xb4: // n/c?
		return;

		case 0xb5:
	//		flipscreen = data & 1;
		return;

		case 0xb6:
			background_romsel = data & 1;
		return;

		case 0xb7:
			color_A4 = (data & 1) << 4;
		return;

		case 0xd0:
			soundlatch = data | 0x80;
		return;
	}
}

static UINT8 __fastcall tubep_main_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x80:
			return DrvDips[0];

		case 0x90:
			return DrvDips[1];

		case 0xa0:
			return (DrvDips[2] & ~0x20) | ((soundlatch & 0x80) ? 0x00 : 0x20);

		case 0xb0:
			return DrvInputs[2]; // sy

		case 0xc0:
			return DrvInputs[1]; // p2

		case 0xd0:
			return DrvInputs[0]; // p1
	}

	return 0;
}

static void __fastcall tubep_sub_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xa000:
			ls175_b7 = ((data & 0x0f) ^ 0x0f) | 0xf0;
		return;

		case 0xc000:
			ls175_e8 = ((data & 0x0f) ^ 0x0f);
		return;
	}
}

static void __fastcall tubep_sub_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0xb6:
		return; // nop

		case 0x7f:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;
	}
}

static UINT8 __fastcall tubep_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xd000:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			return 0;
	}

	return 0;
}

static void __fastcall tubep_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
		case 0x04:
		case 0x05:
			AY8910Write((port / 2) & 3, port & 1, data);
		return;

		case 0x07:
			soundlatch &= 0x7f;
		return;
	}
}

static UINT8 __fastcall tubep_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x06:
		{
			return soundlatch;
		}
	}

	return 0;
}

static void draw_sprite()
{
	UINT8 * romCxx  = DrvGfxROM[1];
	UINT8 * romD10  = romCxx+0x10000;
	UINT8 * romEF13 = romCxx+0x12000;
	UINT8 * romHI2  = romCxx+0x14000;

	UINT8 * active_fb = DrvFrameBuffers + framebuffer_select*256*256;

	for (UINT32 YDOT=0; (YDOT^YSize) != 0x00; YDOT++)
	{
		UINT32 ls273_e12 = romD10[ romD_addr | YDOT ] & 0x7f;
		UINT32 romEF_addr_now = romEF_addr | ls273_e12;
		UINT32 E16_add_a = romEF13[ romEF_addr_now ] |
							((romEF13[0x1000 + romEF_addr_now ]&0x0f)<<8);
		UINT32 F16_add_b = E16_add_a + E16_add_b;

		UINT32 romHI_addr = (YDOT) | (romHI_addr_mid) | (((romHI_addr_msb + 0x800) )&0x1800);
		UINT32 ls273_g4 = romHI2[ romHI_addr ];
		UINT32 ls273_j4 = romHI2[0x2000+ romHI_addr ];
		UINT32 ls86_gh5 = ls273_g4 ^ VINV;
		UINT32 ls86_ij5 = ls273_j4 ^ VINV;

		UINT32 ls157_gh7= ls273_g6 | (mark_2);
		UINT32 ls157_ij7= ls273_j6 | (mark_1);
		UINT32 ls283_gh8= (VINV & 1) + ls86_gh5 + ((ls86_gh5 & 0x80)<<1) + ls157_gh7;
		UINT32 ls283_ij8= (VINV & 1) + ls86_ij5 + ((ls86_ij5 & 0x80)<<1) + ls157_ij7;

		UINT32 ls273_g9 = ls283_gh8;
		UINT32 ls273_j9 = ls283_ij8;

		for (UINT32 XDOT=0; (XDOT^XSize) != 0x00; XDOT++)
		{
			UINT32 romD10_out = romD10[ romD_addr | XDOT ];
			UINT32 F16_add_a = (romD10_out & 0x7e) >>1;
			UINT32 romCxx_addr = (F16_add_a + F16_add_b ) & 0xffff;
			UINT32 romCxx_out = romCxx[ romCxx_addr ];

			UINT32 colorram_addr_lo = (romD10_out&1) ? (romCxx_out>>4)&0x0f: (romCxx_out>>0)&0x0f;

			UINT8 sp_data = DrvSprColRAM[ colorram_addr_hi | colorram_addr_lo ] & 0x0f; /* 2114 4-bit RAM */

			romHI_addr = (XDOT) | (romHI_addr_mid) | (romHI_addr_msb);
			ls273_g4 = romHI2[ romHI_addr ];
			ls273_j4 = romHI2[0x2000+ romHI_addr ];
			ls86_gh5 = ls273_g4 ^ HINV;
			ls86_ij5 = ls273_j4 ^ HINV;

			ls157_gh7= ls273_g9;
			ls157_ij7= ls273_j9;
			ls283_gh8= (HINV & 1) + ls86_gh5 + ((ls86_gh5 & 0x80)<<1) + ls157_gh7;
			ls283_ij8= (HINV & 1) + ls86_ij5 + ((ls86_ij5 & 0x80)<<1) + ls157_ij7;

			if ( !((ls283_gh8&256) | (ls283_ij8&256)) ) /* skip wrapped sprite area - PAL12L6 (PLA019 in Roller Jammer schematics)*/
			{
				if ( active_fb[ (ls283_gh8&255) + (ls283_ij8&255)*256] == 0x0f )
					active_fb[ (ls283_gh8&255) + (ls283_ij8&255)*256] = sp_data;
			}
		}
	}
}

static void tubep_mcu_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x2000:
			romEF_addr = (0x010 | (data & 0x0f))<<7;
			HINV = (data & 0x10) ? 0xff: 0x00;
			VINV = (data & 0x20) ? 0xff: 0x00;
		return;

		case 0x2001:
			XSize = data & 0x7f;
			mark_2 = (data&0x80)<<1;
		return;

		case 0x2002:
			YSize = data & 0x7f;
			mark_1 = (data&0x80)<<1;
		return;

		case 0x2003:
			ls273_g6 = data;
		return;

		case 0x2004:
			ls273_j6 = data;
		return;

		case 0x2005:
			romHI_addr_mid = (data & 0x0f)<<7;
			romHI_addr_msb = (data & 0x30)<<7;
		return;

		case 0x2006:
			romD_addr = (data & 0x3f)<<7;
		return;

		case 0x2007:
			E16_add_b = ((data & 0xff) << 0) | (E16_add_b & 0xff00);
		return;

		case 0x2008:
			E16_add_b = ((data & 0xff) << 8) | (E16_add_b & 0x00ff);
		return;

		case 0x2009:
			colorram_addr_hi = (data & 0x3f) << 4;
			NSC8105SetIRQLine(0, CPU_IRQSTATUS_NONE);
			sprite_timer = (XSize + 1) * (YSize + 1);
			draw_sprite();
			NSC8105RunEnd(); // end early to re-trigger timer
		return;
	}
}

static void __fastcall rjammer_main_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0xd0:
		case 0xd1:
			// coin counters
		return;

		case 0xd2: // n/c?
		case 0xd3: // n/c?
		case 0xd4: // n/c?
		return;

		case 0xd5:
	//		flipscreen = data & 1;
		return;

		case 0xd6: // n/c?
		case 0xb7: // n/c?
		return;

		case 0xe0:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;

		case 0xf0:
			soundlatch = data;
			ZetNmi(2);
		return;
	}
}

static UINT8 __fastcall rjammer_main_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
			return DrvDips[1];

		case 0x80:
			return DrvDips[1];

		case 0x90:
			return DrvDips[0];

		case 0xa0:
			return DrvInputs[2]; // sy

		case 0xb0:
			return DrvInputs[0]; // p1

		case 0xc0:
			return DrvInputs[1]; // p2
	}

	return 0;
}

static void __fastcall rjammer_sub_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0xb0:
			page = (data & 1) * 0x200;
		return;

		case 0xd0:
			ls377_data = data;
		return;
	}
}

static void __fastcall rjammer_sound_write_port(UINT16 port, UINT8 data)
{
	ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
	switch (port & 0xff)
	{
		case 0x10:
			MSM5205ResetWrite(0, ~data & 1);
		return;

		case 0x18:
			MSM5205PlaymodeWrite(0, (data & 1) ? MSM5205_S48_4B : MSM5205_S64_4B);
		return;

		case 0x80:
			ls377 = data;
			ls74 = 0;
		return;

		case 0x90:
		case 0x91:
		case 0x92:
		case 0x93:
		case 0x94:
		case 0x95:
			AY8910Write((port / 2) & 3, port & 1, data);
		return;

		case 0x96: // MSM5205 - intensity control (nop)
		return;
	}
}

static UINT8 __fastcall rjammer_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
		{
			return soundlatch;
		}
	}

	return 0;
}

static void rjammer_adpcm_vck()
{
	if (ls74 == 0)
	{
		MSM5205DataWrite(0, ls377 & 0x0f);
	}
	else
	{
		MSM5205DataWrite(0, (ls377 >> 4) & 0x0f);
		ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
	}
	ls74 ^= 1;
}

inline static INT32 DrvMSM5205SynchroniseStream(INT32 nSoundRate)
{
	return (INT64)((double)ZetTotalCycles() * nSoundRate / 2496000);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetReset(0);
	ZetReset(1);
	ZetReset(2);

	NSC8105Open(0);
	NSC8105Reset();
	NSC8105Close();

	AY8910Reset(0);
	AY8910Reset(1);
	AY8910Reset(2);

	if (rjammer) MSM5205Reset();

	soundlatch = 0;
	color_A4 = 0;
	background_romsel = 0;
	ls175_b7 = 0;
	ls175_e8 = 0;
	romEF_addr = 0;
	HINV = 0;
	VINV = 0;
	XSize = 0;
	YSize = 0;
	mark_1 = 0;
	mark_2 = 0;
	ls273_g6 = 0;
	ls273_j6 = 0;
	romHI_addr_mid = 0;
	romHI_addr_msb = 0;
	romD_addr = 0;
	E16_add_b = 0;
	colorram_addr_hi = 0;
	framebuffer_select = 0;

	page = 0;
	ls377_data = 0;
	ls377 = 0;
	ls74 = 0;

	sprite_timer = -1;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM[0]		= Next; Next += 0x010000;
	DrvZ80ROM[1]		= Next; Next += 0x010000;
	DrvZ80ROM[2]		= Next; Next += 0x008000;
	DrvMCUROM			= Next; Next += 0x010000;

	DrvGfxROM[0]		= Next; Next += 0x00c000;
	DrvGfxROM[1]		= Next; Next += 0x018000;
	DrvGfxROM[2]		= Next; Next += 0x001000;

	DrvColPROM			= Next; Next += 0x000040;

	DrvPalette			= (UINT32*)Next; Next += 0x4040 * sizeof(UINT32);

	AllRam				= Next;

	DrvBgRAM			= Next; Next += 0x000800;
	DrvSprColRAM		= Next; Next += 0x000400;
	DrvTxtRAM			= Next; Next += 0x000800;
	DrvZ80RAM[0]		= Next; Next += 0x000800;
	DrvZ80RAM[1]		= Next; Next += 0x000800;
	DrvZ80RAM[2]		= Next; Next += 0x000800;
	DrvShareRAM[0]		= Next; Next += 0x000800;
	DrvShareRAM[1]		= Next; Next += 0x000800;

	DrvFrameBuffers		= Next; Next += 0x020000;

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static INT32 TubepInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

 	{
 		INT32 k = 0;
 		if (BurnLoadRom(DrvZ80ROM[0] + 0x00000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvZ80ROM[0] + 0x02000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvZ80ROM[0] + 0x04000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvZ80ROM[0] + 0x06000, k++, 1)) return 1;

 		if (BurnLoadRom(DrvZ80ROM[1] + 0x00000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvZ80ROM[1] + 0x02000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvZ80ROM[1] + 0x04000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvZ80ROM[1] + 0x06000, k++, 1)) return 1;

 		if (BurnLoadRom(DrvZ80ROM[2] + 0x00000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvZ80ROM[2] + 0x02000, k++, 1)) return 1;

 		if (BurnLoadRom(DrvMCUROM    + 0x0c000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvMCUROM    + 0x0e000, k++, 1)) return 1;

 		if (BurnLoadRom(DrvGfxROM[0] + 0x00000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[0] + 0x02000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[0] + 0x04000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[0] + 0x06000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[0] + 0x08000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[0] + 0x0a000, k++, 1)) return 1;

 		if (BurnLoadRom(DrvGfxROM[1] + 0x00000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[1] + 0x02000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[1] + 0x04000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[1] + 0x06000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[1] + 0x08000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[1] + 0x0a000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[1] + 0x0c000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[1] + 0x0e000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[1] + 0x10000, k++, 1)) return 1;
 		memcpy (DrvGfxROM[1] + 0x11000, DrvGfxROM[1] + 0x10000, 0x1000);
 		if (BurnLoadRom(DrvGfxROM[1] + 0x12000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[1] + 0x13000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[1] + 0x14000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[1] + 0x16000, k++, 1)) return 1;

 		if (BurnLoadRom(DrvGfxROM[2] + 0x00000, k++, 1)) return 1;

 		if (BurnLoadRom(DrvColPROM   + 0x00000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvColPROM   + 0x00020, k++, 1)) return 1;
 	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM[0],				0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM[0],				0xa000, 0xa7ff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,					0xc000, 0xc7ff, MAP_WRITE); // read too?
	ZetMapMemory(DrvShareRAM[0],			0xe000, 0xe7ff, MAP_WRITE);
	ZetMapMemory(DrvBgRAM,					0xe800, 0xebff, MAP_WRITE);
	ZetSetOutHandler(tubep_main_write_port);
	ZetSetInHandler(tubep_main_read_port);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM[1],				0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvShareRAM[0],			0xe000, 0xe7ff, MAP_RAM);
	ZetMapMemory(DrvBgRAM,					0xe800, 0xebff, MAP_WRITE);
	ZetMapMemory(DrvSprColRAM,				0xf000, 0xf3ff, MAP_WRITE);
	ZetMapMemory(DrvShareRAM[1],			0xf800, 0xffff, MAP_RAM);
	ZetSetWriteHandler(tubep_sub_write);
	ZetSetOutHandler(tubep_sub_write_port);
	ZetClose();

	ZetInit(2);
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM[2],				0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM[2],				0xe000, 0xe7ff, MAP_RAM);
	ZetSetReadHandler(tubep_sound_read);
	ZetSetOutHandler(tubep_sound_write_port);
	ZetSetInHandler(tubep_sound_read_port);
	ZetClose();

	NSC8105Init(0);
	NSC8105Open(0);
	NSC8105MapMemory(DrvSprColRAM,			0x0000, 0x03ff, MAP_RAM);
	NSC8105MapMemory(DrvShareRAM[1],		0x0800, 0x0fff, MAP_RAM);
	NSC8105MapMemory(DrvMCUROM + 0xc000,	0xc000, 0xffff, MAP_ROM);
	NSC8105SetWriteHandler(tubep_mcu_write);
	NSC8105Close();

	AY8910Init(0, 1248000, 0);
	AY8910Init(1, 1248000, 0);
	AY8910Init(2, 1248000, 0);
	AY8910SetAllRoutes(0, 0.15, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.15, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(2, 0.15, BURN_SND_ROUTE_BOTH);
	AY8910SetBuffered(ZetTotalCycles, 2496000);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 TubepbInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

 	{
 		INT32 k = 0;
 		if (BurnLoadRom(DrvZ80ROM[0] + 0x00000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvZ80ROM[0] + 0x01000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvZ80ROM[0] + 0x02000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvZ80ROM[0] + 0x03000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvZ80ROM[0] + 0x04000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvZ80ROM[0] + 0x05000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvZ80ROM[0] + 0x06000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvZ80ROM[0] + 0x07000, k++, 1)) return 1;

 		if (BurnLoadRom(DrvZ80ROM[1] + 0x00000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvZ80ROM[1] + 0x01000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvZ80ROM[1] + 0x02000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvZ80ROM[1] + 0x03000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvZ80ROM[1] + 0x04000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvZ80ROM[1] + 0x05000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvZ80ROM[1] + 0x06000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvZ80ROM[1] + 0x07000, k++, 1)) return 1;

 		if (BurnLoadRom(DrvZ80ROM[2] + 0x00000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvZ80ROM[2] + 0x02000, k++, 1)) return 1;

 		if (BurnLoadRom(DrvMCUROM    + 0x0c000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvMCUROM    + 0x0e000, k++, 1)) return 1;

 		if (BurnLoadRom(DrvGfxROM[0] + 0x00000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[0] + 0x02000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[0] + 0x04000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[0] + 0x06000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[0] + 0x08000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[0] + 0x0a000, k++, 1)) return 1;

 		if (BurnLoadRom(DrvGfxROM[1] + 0x00000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[1] + 0x01000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[1] + 0x02000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[1] + 0x03000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[1] + 0x04000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[1] + 0x05000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[1] + 0x06000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[1] + 0x07000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[1] + 0x08000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[1] + 0x09000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[1] + 0x0a000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[1] + 0x0b000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[1] + 0x0c000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[1] + 0x0d000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[1] + 0x0e000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[1] + 0x0f000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[1] + 0x10000, k++, 1)) return 1;
 		memcpy (DrvGfxROM[1] + 0x11000, DrvGfxROM[1] + 0x10000, 0x1000);
 		if (BurnLoadRom(DrvGfxROM[1] + 0x12000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[1] + 0x13000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[1] + 0x14000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[1] + 0x16000, k++, 1)) return 1;

 		if (BurnLoadRom(DrvGfxROM[2] + 0x00000, k++, 1)) return 1;

 		if (BurnLoadRom(DrvColPROM   + 0x00000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvColPROM   + 0x00020, k++, 1)) return 1;
 	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM[0],				0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM[0],				0xa000, 0xa7ff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,					0xc000, 0xc7ff, MAP_WRITE); // read too?
	ZetMapMemory(DrvShareRAM[0],			0xe000, 0xe7ff, MAP_WRITE);
	ZetMapMemory(DrvBgRAM,					0xe800, 0xebff, MAP_WRITE);
	ZetSetOutHandler(tubep_main_write_port);
	ZetSetInHandler(tubep_main_read_port);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM[1],				0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvShareRAM[0],			0xe000, 0xe7ff, MAP_RAM);
	ZetMapMemory(DrvBgRAM,					0xe800, 0xebff, MAP_WRITE);
	ZetMapMemory(DrvSprColRAM,				0xf000, 0xf3ff, MAP_WRITE);
	ZetMapMemory(DrvShareRAM[1],			0xf800, 0xffff, MAP_RAM);
	ZetSetWriteHandler(tubep_sub_write);
	ZetSetOutHandler(tubep_sub_write_port);
	ZetClose();

	ZetInit(2);
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM[2],				0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM[2],				0xe000, 0xe7ff, MAP_RAM);
	ZetSetReadHandler(tubep_sound_read);
	ZetSetOutHandler(tubep_sound_write_port);
	ZetSetInHandler(tubep_sound_read_port);
	ZetClose();

	M6800Init(0);
	M6800Open(0);
	M6800MapMemory(DrvSprColRAM,			0x0000, 0x03ff, MAP_RAM);
	M6800MapMemory(DrvShareRAM[1],			0x0800, 0x0fff, MAP_RAM);
	M6800MapMemory(DrvMCUROM + 0xc000,		0xc000, 0xffff, MAP_ROM);
	M6800SetWriteHandler(tubep_mcu_write);
	M6800Close(); // from now on, this cpu will be accessed via NSC8105* function aliases.

	AY8910Init(0, 1248000, 0);
	AY8910Init(1, 1248000, 0);
	AY8910Init(2, 1248000, 0);
	AY8910SetAllRoutes(0, 0.15, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.15, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(2, 0.15, BURN_SND_ROUTE_BOTH);
	AY8910SetBuffered(ZetTotalCycles, 2496000);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 RjammerInit()
{
	rjammer = 1;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

 	{
 		INT32 k = 0;
 		if (BurnLoadRom(DrvZ80ROM[0] + 0x00000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvZ80ROM[0] + 0x02000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvZ80ROM[0] + 0x04000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvZ80ROM[0] + 0x08000, k++, 1)) return 1;
		memcpy (DrvZ80ROM[0] + 0x6000, DrvZ80ROM[0] + 0xa000, 0x2000); // swap halves

 		if (BurnLoadRom(DrvZ80ROM[1] + 0x00000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvZ80ROM[1] + 0x02000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvZ80ROM[1] + 0x04000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvZ80ROM[1] + 0x06000, k++, 1)) return 1;

 		if (BurnLoadRom(DrvZ80ROM[2] + 0x00000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvZ80ROM[2] + 0x02000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvZ80ROM[2] + 0x04000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvZ80ROM[2] + 0x06000, k++, 1)) return 1;

 		if (BurnLoadRom(DrvMCUROM    + 0x0c000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvMCUROM    + 0x0e000, k++, 1)) return 1;

 		if (BurnLoadRom(DrvGfxROM[0] + 0x00000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[0] + 0x01000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[0] + 0x03000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[0] + 0x05000, k++, 1)) return 1;

 		if (BurnLoadRom(DrvGfxROM[1] + 0x00000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[1] + 0x02000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[1] + 0x04000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[1] + 0x06000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[1] + 0x08000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[1] + 0x0a000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[1] + 0x0c000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[1] + 0x0e000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[1] + 0x10000, k++, 1)) return 1;
 		memcpy (DrvGfxROM[1] + 0x11000, DrvGfxROM[1] + 0x10000, 0x1000);
 		if (BurnLoadRom(DrvGfxROM[1] + 0x12000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[1] + 0x13000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[1] + 0x14000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvGfxROM[1] + 0x16000, k++, 1)) return 1;

 		if (BurnLoadRom(DrvGfxROM[2] + 0x00000, k++, 1)) return 1;

 		if (BurnLoadRom(DrvColPROM   + 0x00000, k++, 1)) return 1;
 		if (BurnLoadRom(DrvColPROM   + 0x00020, k++, 1)) return 1;
 	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM[0],				0x0000, 0x9fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM[0],				0xa000, 0xa7ff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,					0xc000, 0xc7ff, MAP_WRITE); // read too?
	ZetMapMemory(DrvShareRAM[0],			0xe000, 0xe7ff, MAP_RAM);
	ZetSetOutHandler(rjammer_main_write_port);
	ZetSetInHandler(rjammer_main_read_port);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM[1],				0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM[1],				0xa000, 0xa7ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM[0],			0xe000, 0xe7ff, MAP_RAM);
	ZetMapMemory(DrvBgRAM,					0xe800, 0xefff, MAP_RAM);
	ZetMapMemory(DrvShareRAM[1],			0xf800, 0xffff, MAP_RAM);
	ZetSetOutHandler(rjammer_sub_write_port);
	ZetClose();

	ZetInit(2);
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM[2],				0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM[2],				0xe000, 0xe7ff, MAP_RAM);
	ZetSetReadHandler(tubep_sound_read);
	ZetSetOutHandler(rjammer_sound_write_port);
	ZetSetInHandler(rjammer_sound_read_port);
	ZetClose();

	NSC8105Init(0);
	NSC8105Open(0);
	NSC8105MapMemory(DrvSprColRAM,			0x0000, 0x03ff, MAP_RAM);
	NSC8105MapMemory(DrvShareRAM[1],		0x0800, 0x0fff, MAP_RAM);
	NSC8105MapMemory(DrvMCUROM + 0xc000,	0xc000, 0xffff, MAP_ROM);
	NSC8105SetWriteHandler(tubep_mcu_write);
	NSC8105Close();

	AY8910Init(0, 1248000, 1);
	AY8910Init(1, 1248000, 1);
	AY8910Init(2, 1248000, 1);
	AY8910SetAllRoutes(0, 0.10, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.10, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(2, 0.10, BURN_SND_ROUTE_BOTH);
	AY8910SetBuffered(ZetTotalCycles, 2496000);

	MSM5205Init(0, DrvMSM5205SynchroniseStream, 384000, rjammer_adpcm_vck, MSM5205_S48_4B, 0);
	MSM5205SetRoute(0, 1.10, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	NSC8105Exit();

	AY8910Exit(0);
	AY8910Exit(1);
	AY8910Exit(2);

	if (rjammer) MSM5205Exit();

	BurnFree(AllMem);

	rjammer = 0;

	return 0;
}

static void CommonPaletteInit()
{
	for (INT32 i = 0; i < 64; i++)
	{
		INT32 bit0, bit1, bit2;

		bit0 = (DrvColPROM[i] >> 0) & 1;
		bit1 = (DrvColPROM[i] >> 1) & 1;
		bit2 = (DrvColPROM[i] >> 2) & 1;
		INT32 r = (bit2 * 1000) + (bit1 * 470) + (bit0 * 220);

		bit0 = (DrvColPROM[i] >> 3) & 1;
		bit1 = (DrvColPROM[i] >> 4) & 1;
		bit2 = (DrvColPROM[i] >> 5) & 1;
		INT32 g = (bit2 * 1000) + (bit1 * 470) + (bit0 * 220);

		bit0 = (DrvColPROM[i] >> 6) & 1;
		bit1 = (DrvColPROM[i] >> 7) & 1;
		INT32 b = (bit1 * 470) + (bit0 * 220);

		DrvPalette[i] = BurnHighCol((r * 255) / 1690, (g * 255) / 1690, (b * 255) / 690, 0);
	}
}

static void TubepPaletteInit()
{
	INT32 resistors_0[6] = { 33000, 15000, 8200, 4700, 2200, 1000 };
	INT32 resistors_1[6] = { 15000,  8200, 4700, 2200, 1000,  470 };
	INT32 resistors_2[6] = {  8200,  4700, 2200, 1000,  470,  220 };

	INT32 active_resistors_r[3 * 6];
	INT32 active_resistors_g[3 * 6];
	INT32 active_resistors_b[2 * 6];

	for (int i = 0; i < 6; i++)
	{
		active_resistors_r[ 0+i] = resistors_0[i];
		active_resistors_r[ 6+i] = resistors_1[i];
		active_resistors_r[12+i] = resistors_2[i];
		active_resistors_g[ 0+i] = resistors_0[i];
		active_resistors_g[ 6+i] = resistors_1[i];
		active_resistors_g[12+i] = resistors_2[i];
		active_resistors_b[ 0+i] = resistors_1[i];
		active_resistors_b[ 6+i] = resistors_2[i];
	}

	double weights_r[3 * 6];
	double weights_g[3 * 6];
	double weights_b[2 * 6];
	compute_resistor_weights(0, 255, -1.0,
				3*6,    active_resistors_r, weights_r,  470,    0,
				3*6,    active_resistors_g, weights_g,  470,    0,
				2*6,    active_resistors_b, weights_b,  470,    0);

	for (INT32 i = 0; i < 256; i++)
	{
		for (INT32 sh = 0; sh < 0x40; sh++)
		{
			INT32 j     = i;          /* active low */
			INT32 shade = sh^0x3f;    /* negated outputs */

			INT32 bits_r[3 * 6];
			INT32 bits_g[3 * 6];
			INT32 bits_b[2 * 6];
			for (INT32 c = 0; c < 6; c++)
			{
				bits_r[0 + c] = (shade>>c) & ((~j >> 0) & 1);
				bits_r[6 + c] = (shade>>c) & ((~j >> 1) & 1);
				bits_r[12+ c] = (shade>>c) & ((~j >> 2) & 1);
				bits_g[0 + c] = (shade>>c) & ((~j >> 3) & 1);
				bits_g[6 + c] = (shade>>c) & ((~j >> 4) & 1);
				bits_g[12+ c] = (shade>>c) & ((~j >> 5) & 1);
				bits_b[0 + c] = (shade>>c) & ((~j >> 6) & 1);
				bits_b[6 + c] = (shade>>c) & ((~j >> 7) & 1);
			}

			double out;

			out = 0.0;
			for (INT32 c = 0; c < 3*6; c++)   out += weights_r[c] * bits_r[c];
			INT32 r = (INT32)(out + 0.5);

			out = 0.0;
			for (INT32 c = 0; c < 3*6; c++)   out += weights_g[c] * bits_g[c];
			INT32 g = (INT32)(out + 0.5);

			out = 0.0;
			for (INT32 c = 0; c < 2*6; c++)   out += weights_b[c] * bits_b[c];
			INT32 b = (INT32)(out + 0.5);

			DrvPalette[64 + (i * 0x40) + sh] = BurnHighCol(r,g,b,0);
		}
	}
}

static void screen_update_tubep(UINT32 scanline)
{
	if (scanline < 16 || scanline >= (nScreenHeight + 16) || !pBurnDraw) return;

	INT32 DISP_ = framebuffer_select^1;

	UINT32 pen_base = 64;

	UINT8 *text_gfx_base = DrvGfxROM[2];
	UINT8 *romBxx = DrvGfxROM[0] + 0x2000*background_romsel;

	UINT16 *dest = pTransDraw + (scanline - 16) * nScreenWidth;

	UINT32 v = scanline;
	{
		UINT8 *active_fb = DrvFrameBuffers + v*256 + (DISP_*256*256);
		UINT32 sp_data0=0,sp_data1=0,sp_data2=0;


		// It appears there is a 1 pixel delay when renderer switches from background to sprite/text,
		// this causes text and sprite layers to draw a drop shadow with 1 dot width to the left.
		// See the gameplay video on the PCB. https://www.youtube.com/watch?v=xxONzbUOOsw
		bool prev_text_or_sprite_pixel = true;

		for (UINT32 h = 0*8; h < 32*8; h++)
		{
			bool draw_text_or_sprite_pixel = false;

			sp_data2 = sp_data1;
			sp_data1 = sp_data0;
			sp_data0 = active_fb[h];

			INT32 text_offs = ((v >> 3) << 6) | ((h >> 3) << 1);
			UINT8 text_code = DrvTxtRAM[text_offs];
			UINT8 text_gfx_data = text_gfx_base[(text_code << 3) | (v & 0x07)];

			if (text_gfx_data & (0x80 >> (h & 0x07))) {
				dest[h] = (DrvTxtRAM[text_offs + 1] & 0x0f) | color_A4;
				draw_text_or_sprite_pixel = true;
			}
			else
			{
				UINT32 bg_data;
				UINT32 sp_data;

				UINT32 romB_addr = (((h>>1)&0x3f)^((h&0x80)?0x00:0x3f)) | (((v&0x7f)^((v&0x80)?0x00:0x7f))<<6);

				UINT8  rom_select = (h&0x01) ^ (((h&0x80)>>7)^1);

				UINT8 romB_data_h = romBxx[ 0x4000 + 0x4000*rom_select + romB_addr ];

				UINT32 VR_addr = ((romB_data_h + ls175_b7) & 0xfe) << 2;

				UINT8 xor_logic = (((h^v)&0x80)>>7) ^ (background_romsel & (((v&0x80)>>7)^1));

				UINT8 romB_data_l = romBxx[ romB_addr ] ^ (xor_logic?0xff:0x00);

				UINT8 ls157_b11 = (romB_data_l >> ((rom_select==0)?4:0))&0x0f;

				UINT8 ls283_b12 = (ls157_b11 + ls175_e8) & 0x0f;

				VR_addr |= (ls283_b12>>1);

				bg_data = DrvBgRAM[ VR_addr ];

				romB_data_h>>=2;

				if ((sp_data0 != 0x0f) && (sp_data1 == 0x0f) && (sp_data2 != 0x0f))
					sp_data = sp_data2;
				else
					sp_data = sp_data1;

				if (sp_data != 0x0f) {
					bg_data = DrvColPROM[(0x20 + sp_data) | color_A4];
					draw_text_or_sprite_pixel = true;
				}
				dest[h] = pen_base + bg_data*64 + romB_data_h;
			}

			// text and sprite drop shadow
			if (draw_text_or_sprite_pixel && !prev_text_or_sprite_pixel && h > 0)
				dest[h - 1] = 0x00;
			prev_text_or_sprite_pixel = draw_text_or_sprite_pixel;
		}
	}
}

static void screen_update_rjammer(INT32 scanline)
{
	if (scanline < 16 || scanline >= (nScreenHeight + 16) || !pBurnDraw) return;

	INT32 DISP_ = framebuffer_select^1;

	UINT8 *text_gfx_base = DrvGfxROM[2];
	UINT8 *rom13D  = DrvGfxROM[0];
	UINT8 *rom11BD = rom13D+0x1000;
	UINT8 *rom19C  = rom13D+0x5000;

	UINT32 v = scanline;
	UINT8 *active_fb = DrvFrameBuffers + v*256 +(DISP_*256*256);
	UINT16 *dest = pTransDraw + (v - 16) * nScreenWidth;
	{
		UINT32 sp_data0=0,sp_data1=0,sp_data2=0;
		UINT8 pal14h4_pin19;
		UINT8 pal14h4_pin18;
		UINT8 pal14h4_pin13;

		UINT32 addr = (v*2) | page;
		UINT32 ram_data = DrvBgRAM[ addr ] + 256*(DrvBgRAM[ addr+1 ]&0x2f);

		addr = (v>>3) | ((ls377_data&0x1f)<<5);
		pal14h4_pin13 = (rom19C[addr] >> ((v&7)^7) ) &1;
		pal14h4_pin19 = (ram_data>>13) & 1;

		for (UINT32 h = 0*8; h < 32*8; h++)
		{
			UINT32 text_offs;
			UINT8 text_code;
			UINT8 text_gfx_data;

			sp_data2 = sp_data1;
			sp_data1 = sp_data0;
			sp_data0 = active_fb[h];

			text_offs = ((v >> 3) << 6) | ((h >> 3) << 1);
			text_code = DrvTxtRAM[text_offs];
			text_gfx_data = text_gfx_base[(text_code << 3) | (v & 0x07)];

			if (text_gfx_data & (0x80 >> (h & 0x07)))
				dest[h] = 0x10 | (DrvTxtRAM[text_offs + 1] & 0x0f);
			else
			{
				UINT32 sp_data;

				if ((sp_data0 != 0x0f) && (sp_data1 == 0x0f) && (sp_data2 != 0x0f))
					sp_data = sp_data2;
				else
					sp_data = sp_data1;

				if (sp_data != 0x0f)
					dest[h] = 0x00 + sp_data;
				else
				{
					UINT32 bg_data;
					UINT8 color_bank;

					UINT32 ls283 = (ram_data & 0xfff) + h;
					UINT32 rom13D_addr = ((ls283>>4)&0x00f) | (v&0x0f0) | (ls283&0xf00);
					UINT32 rom13D_data = rom13D[ rom13D_addr ] & 0x7f;
					UINT32 rom11BD_addr = (rom13D_data<<7) | ((v&0x0f)<<3) | ((ls283>>1)&0x07);
					UINT32 rom11_data = rom11BD[ rom11BD_addr];

					if ((ls283&1)==0)
						bg_data = rom11_data & 0x0f;
					else
						bg_data = (rom11_data>>4) & 0x0f;

					addr = (h>>3) | (ls377_data<<5);
					pal14h4_pin18 = (rom19C[addr] >> ((h&7)^7) ) &1;

					color_bank =  (pal14h4_pin13 & ((bg_data&0x08)>>3) & ((bg_data&0x04)>>2) & (((bg_data&0x02)>>1)^1) &  (bg_data&0x01)    )
								| (pal14h4_pin18 & ((bg_data&0x08)>>3) & ((bg_data&0x04)>>2) &  ((bg_data&0x02)>>1)    & ((bg_data&0x01)^1) )
								| (pal14h4_pin19);
					dest[h] = 0x20 + color_bank*0x10 + bg_data;
				}
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		CommonPaletteInit();
		if (!rjammer) TubepPaletteInit();
		DrvRecalc = 0;
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void sync_spritemcu()
{
	INT32 cyc = ((ZetTotalCycles() * 6) / 4) - NSC8105TotalCycles();
	INT32 torun = cyc;
	INT32 ran = 0;

	while (cyc > 0) {
		if (sprite_timer > 0 && sprite_timer < cyc) {
			torun = sprite_timer + 1;
		} else {
			torun = cyc;
		}

		ran = NSC8105Run(torun);
		cyc -= ran;

		if (sprite_timer > 0)
		{
			sprite_timer -= ran;
			if (sprite_timer <= 0)
			{
				NSC8105SetIRQLine(0, CPU_IRQSTATUS_ACK);
			}
		}
	}
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();
	NSC8105NewFrame();

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 264;
	INT32 nCyclesTotal[4] = { 4000000 / 60, 4000000 / 60, 2496000 / 60, 6000000 / 60 };
	INT32 nCyclesDone[4] = { 0, 0, 0, 0 };

	if (rjammer) MSM5205NewFrame(0, 2496000, nInterleave);

	NSC8105Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		CPU_RUN(0, Zet);
		if (i == 239) ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		sync_spritemcu();
		ZetClose();

		ZetOpen(1);
		CPU_RUN(1, Zet);
		if (i ==  15) ZetSetIRQLine(0, (rjammer) ? CPU_IRQSTATUS_HOLD : CPU_IRQSTATUS_ACK);
		ZetClose();

		ZetOpen(2);
		CPU_RUN(2, Zet);
		if ((i == 63 || i == 191) && !rjammer) ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		if (rjammer) MSM5205UpdateScanline(i);
		ZetClose();

		if (i == 15) {
			framebuffer_select = framebuffer_select ^ 1;
			memset (DrvFrameBuffers + framebuffer_select * 0x10000, 0x0f, 0x10000);
			NSC8105SetIRQLine(NSC8105_INPUT_LINE_NMI, CPU_IRQSTATUS_ACK);
		}
		if (i == 239) {
			NSC8105SetIRQLine(NSC8105_INPUT_LINE_NMI, CPU_IRQSTATUS_NONE);
		}

		if (rjammer)
			screen_update_rjammer(i);
		else
			screen_update_tubep(i);
	}

	NSC8105Close();

	if (pBurnSoundOut) {
		if (rjammer) {
			ZetOpen(2);
			MSM5205Render(0, pBurnSoundOut, nBurnSoundLen);
			ZetClose();
			BurnSoundDCFilter(); // deal with rjammer's ugly msm5205 waveform
		}
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029702;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);
		NSC8105Scan(nAction);
		AY8910Scan(nAction, pnMin);
		if (rjammer) MSM5205Scan(nAction, pnMin);

		SCAN_VAR(soundlatch);
		SCAN_VAR(color_A4);
		SCAN_VAR(background_romsel);
		SCAN_VAR(ls175_b7);
		SCAN_VAR(ls175_e8);
		SCAN_VAR(romEF_addr);
		SCAN_VAR(HINV);
		SCAN_VAR(VINV);
		SCAN_VAR(XSize);
		SCAN_VAR(YSize);
		SCAN_VAR(mark_1);
		SCAN_VAR(mark_2);
		SCAN_VAR(ls273_g6);
		SCAN_VAR(ls273_j6);
		SCAN_VAR(romHI_addr_mid);
		SCAN_VAR(romHI_addr_msb);
		SCAN_VAR(romD_addr);
		SCAN_VAR(E16_add_b);
		SCAN_VAR(colorram_addr_hi);
		SCAN_VAR(framebuffer_select);
		SCAN_VAR(sprite_timer);
		SCAN_VAR(page);
		SCAN_VAR(ls377_data);
		SCAN_VAR(ls377);
		SCAN_VAR(ls74);
	}

	return 0;
}


// Tube Panic

static struct BurnRomInfo tubepRomDesc[] = {
	{ "tp-p.5",			0x2000, 0xd5e0cc2f, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "tp-p.6",			0x2000, 0x97b791a0, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tp-p.7",			0x2000, 0xadd9983e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "tp-p.8",			0x2000, 0xb3793cb5, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "tp-p.1",			0x2000, 0xb4020fcc, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code
	{ "tp-p.2",			0x2000, 0xa69862d6, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "tp-p.3",			0x2000, 0xf1d86e00, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "tp-p.4",			0x2000, 0x0a1027bc, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "tp-s.1",			0x2000, 0x78964fcc, 3 | BRF_PRG | BRF_ESS }, //  8 Z80 #2 Code
	{ "tp-s.2",			0x2000, 0x61232e29, 3 | BRF_PRG | BRF_ESS }, //  9

	{ "tp-g5.e1",		0x2000, 0x9f375b27, 4 | BRF_PRG | BRF_ESS }, // 10 MCU Code
	{ "tp-g6.d1",		0x2000, 0x3ea127b8, 4 | BRF_PRG | BRF_ESS }, // 11

	{ "tp-b.1",			0x2000, 0xfda355e0, 5 | BRF_GRA },           // 12 Background Data
	{ "tp-b.2",			0x2000, 0xcbe30149, 5 | BRF_GRA },           // 13
	{ "tp-b.3",			0x2000, 0xf5d118e7, 5 | BRF_GRA },           // 14
	{ "tp-b.4",			0x2000, 0x01952144, 5 | BRF_GRA },           // 15
	{ "tp-b.5",			0x2000, 0x4dabea43, 5 | BRF_GRA },           // 16
	{ "tp-b.6",			0x2000, 0x01952144, 5 | BRF_GRA },           // 17

	{ "tp-c.1",			0x2000, 0xec002af2, 6 | BRF_GRA },           // 18 Sprites
	{ "tp-c.2",			0x2000, 0xc44f7128, 6 | BRF_GRA },           // 19
	{ "tp-c.3",			0x2000, 0x4146b0c9, 6 | BRF_GRA },           // 20
	{ "tp-c.4",			0x2000, 0x552b58cf, 6 | BRF_GRA },           // 21
	{ "tp-c.5",			0x2000, 0x2bb481d7, 6 | BRF_GRA },           // 22
	{ "tp-c.6",			0x2000, 0xc07a4338, 6 | BRF_GRA },           // 23
	{ "tp-c.7",			0x2000, 0x87b8700a, 6 | BRF_GRA },           // 24
	{ "tp-c.8",			0x2000, 0xa6497a03, 6 | BRF_GRA },           // 25
	{ "tp-g4.d10",		0x1000, 0x40a1fe00, 6 | BRF_GRA },           // 26
	{ "tp-g1.e13",		0x1000, 0x4a7407a2, 6 | BRF_GRA },           // 27
	{ "tp-g2.f13",		0x1000, 0xf0b26c2e, 6 | BRF_GRA },           // 28
	{ "tp-g7.h2",		0x2000, 0x105cb9e4, 6 | BRF_GRA },           // 29
	{ "tp-g8.i2",		0x2000, 0x27e5e6c1, 6 | BRF_GRA },           // 30

	{ "tp-g3.c10",		0x1000, 0x657a465d, 7 | BRF_GRA },           // 31 Characters

	{ "tp-2.c12",		0x0020, 0xac7e582f, 8 | BRF_GRA },           // 32 Color PROMs
	{ "tp-1.c13",		0x0020, 0xcd0910d6, 8 | BRF_GRA },           // 33
};

STD_ROM_PICK(tubep)
STD_ROM_FN(tubep)

struct BurnDriver BurnDrvTubep = {
	"tubep", NULL, NULL, NULL, "1984",
	"Tube Panic\0", NULL, "Nichibutsu / Fujitek", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, tubepRomInfo, tubepRomName, NULL, NULL, NULL, NULL, TubepInputInfo, TubepDIPInfo,
	TubepInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4020,
	256, 224, 4, 3
};


// Tube Panic (bootleg)

static struct BurnRomInfo tubepbRomDesc[] = {
	{ "a15.bin",		0x1000, 0x806370a8, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "a16.bin",		0x1000, 0x0917fb76, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a13.bin",		0x1000, 0x6e4bb47e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "a14.bin",		0x1000, 0x3df78441, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "a11.bin",		0x1000, 0x2b557e49, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "a12.bin",		0x1000, 0xd04a548e, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "a9.bin",			0x1000, 0xa20de3d1, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "a10.bin",		0x1000, 0x033ba70c, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "a1.bin",			0x1000, 0x8a68523d, 2 | BRF_PRG | BRF_ESS }, //  8 Z80 #1 Code
	{ "a2.bin",			0x1000, 0xd15a8645, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "a3.bin",			0x1000, 0x7acf777c, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "a4.bin",			0x1000, 0x8f2bed23, 2 | BRF_PRG | BRF_ESS }, // 11
	{ "a5.bin",			0x1000, 0x8ba045f0, 2 | BRF_PRG | BRF_ESS }, // 12
	{ "a6.bin",			0x1000, 0x8672ab0f, 2 | BRF_PRG | BRF_ESS }, // 13
	{ "a7.bin",			0x1000, 0x417dd321, 2 | BRF_PRG | BRF_ESS }, // 14
	{ "a8.bin",			0x1000, 0xd26ab4c0, 2 | BRF_PRG | BRF_ESS }, // 15

	{ "15.bin",			0x2000, 0x78964fcc, 3 | BRF_PRG | BRF_ESS }, // 16 Z80 #2 Code
	{ "16.bin",			0x2000, 0x61232e29, 3 | BRF_PRG | BRF_ESS }, // 17

	{ "5.bin",			0x2000, 0x9f375b27, 4 | BRF_PRG | BRF_ESS }, // 18 MCU Code
	{ "6.bin",			0x2000, 0x46a273b5, 4 | BRF_PRG | BRF_ESS }, // 19

	{ "9.bin",			0x2000, 0xfda355e0, 5 | BRF_GRA },           // 20 Background Data
	{ "10.bin",			0x2000, 0x0ccb23b0, 5 | BRF_GRA },           // 21
	{ "11.bin",			0x2000, 0xf5d118e7, 5 | BRF_GRA },           // 22
	{ "12.bin",			0x2000, 0x01952144, 5 | BRF_GRA },           // 23
	{ "13.bin",			0x2000, 0x4dabea43, 5 | BRF_GRA },           // 24
	{ "14.bin",			0x2000, 0x01952144, 5 | BRF_GRA },           // 25

	{ "d1.bin",			0x1000, 0x702348d7, 6 | BRF_GRA },           // 26 Sprites
	{ "d2.bin",			0x1000, 0x47601e8b, 6 | BRF_GRA },           // 27
	{ "d3.bin",			0x1000, 0xcaad3ee2, 6 | BRF_GRA },           // 28
	{ "d4.bin",			0x1000, 0xba5d8666, 6 | BRF_GRA },           // 29
	{ "d5.bin",			0x1000, 0xcc709b7f, 6 | BRF_GRA },           // 30
	{ "d6.bin",			0x1000, 0xb9be626a, 6 | BRF_GRA },           // 31
	{ "d7.bin",			0x1000, 0x934e09d4, 6 | BRF_GRA },           // 32
	{ "d8.bin",			0x1000, 0x7e1970a0, 6 | BRF_GRA },           // 33
	{ "d15.bin",		0x1000, 0xf1f15364, 6 | BRF_GRA },           // 34
	{ "d16.bin",		0x1000, 0x05c52829, 6 | BRF_GRA },           // 35
	{ "d13.bin",		0x1000, 0x7c0b9e16, 6 | BRF_GRA },           // 36
	{ "d14.bin",		0x1000, 0x81b31170, 6 | BRF_GRA },           // 37
	{ "d11.bin",		0x1000, 0x9e07ef70, 6 | BRF_GRA },           // 38
	{ "d12.bin",		0x1000, 0x77e72279, 6 | BRF_GRA },           // 39
	{ "d9.bin",			0x1000, 0x7a0edea8, 6 | BRF_GRA },           // 40
	{ "d10.bin",		0x1000, 0x0c1c2cb1, 6 | BRF_GRA },           // 41
	{ "4.bin",			0x1000, 0x40a1fe00, 6 | BRF_GRA },           // 42
	{ "1.bin",			0x1000, 0x4a7407a2, 6 | BRF_GRA },           // 43
	{ "2.bin",			0x1000, 0xf0b26c2e, 6 | BRF_GRA },           // 44
	{ "7.bin",			0x2000, 0x105cb9e4, 6 | BRF_GRA },           // 45
	{ "8.bin",			0x2000, 0x27e5e6c1, 6 | BRF_GRA },           // 46

	{ "3.bin",			0x1000, 0x657a465d, 7 | BRF_GRA },           // 47 Characters

	{ "prom6331.b",		0x0020, 0xac7e582f, 8 | BRF_GRA },           // 48 Color PROMs
	{ "prom6331.a",		0x0020, 0xcd0910d6, 8 | BRF_GRA },           // 49
};

STD_ROM_PICK(tubepb)
STD_ROM_FN(tubepb)

struct BurnDriver BurnDrvTubepb = {
	"tubepb", "tubep", NULL, NULL, "1984",
	"Tube Panic (bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, tubepbRomInfo, tubepbRomName, NULL, NULL, NULL, NULL, TubepInputInfo, TubepbDIPInfo,
	TubepbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4020,
	256, 224, 4, 3
};


// Roller Jammer

static struct BurnRomInfo rjammerRomDesc[] = {
	{ "tp-p.1",			0x2000, 0x93eeed67, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "tp-p.2",			0x2000, 0xed2830c4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tp-p.3",			0x2000, 0xe29f25e3, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "tp-p.4",			0x4000, 0x6ed71fbc, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "tp-p.8",			0x2000, 0x388b9c66, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code
	{ "tp-p.7",			0x2000, 0x595030bb, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "tp-p.6",			0x2000, 0xb5aa0f89, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "tp-p.5",			0x2000, 0x56eae9ac, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "tp-b1.6d",		0x2000, 0xb1c2525c, 3 | BRF_PRG | BRF_ESS }, //  8 Z80 #2 Code
	{ "tp-s3.4d",		0x2000, 0x90c9d0b9, 3 | BRF_PRG | BRF_ESS }, //  9
	{ "tp-s2.2d",		0x2000, 0x444b6a1d, 3 | BRF_PRG | BRF_ESS }, // 10
	{ "tp-s1.1d",		0x2000, 0x391097cd, 3 | BRF_PRG | BRF_ESS }, // 11

	{ "tp-g7.e1",		0x2000, 0x9f375b27, 4 | BRF_PRG | BRF_ESS }, // 12 MCU Code
	{ "tp-g8.d1",		0x2000, 0x2e619fec, 4 | BRF_PRG | BRF_ESS }, // 13

	{ "tp-b3.13d",		0x1000, 0xb80ef399, 5 | BRF_GRA },           // 14 Background Data
	{ "tp-b5.11b",		0x2000, 0x0f260bfe, 5 | BRF_GRA },           // 15
	{ "tp-b2.11d",		0x2000, 0x8cd2c917, 5 | BRF_GRA },           // 16
	{ "tp-b4.19c",		0x2000, 0x6600f306, 5 | BRF_GRA },           // 17

	{ "tp-c.8",			0x2000, 0x9f31ecb5, 6 | BRF_GRA },           // 18 Sprites
	{ "tp-c.7",			0x2000, 0xcbf093f1, 6 | BRF_GRA },           // 19
	{ "tp-c.6",			0x2000, 0x11f9752b, 6 | BRF_GRA },           // 20
	{ "tp-c.5",			0x2000, 0x513f8777, 6 | BRF_GRA },           // 21
	{ "tp-c.1",			0x2000, 0xef573117, 6 | BRF_GRA },           // 22
	{ "tp-c.2",			0x2000, 0x1d29f1e6, 6 | BRF_GRA },           // 23
	{ "tp-c.3",			0x2000, 0x086511a7, 6 | BRF_GRA },           // 24
	{ "tp-c.4",			0x2000, 0x49f372ea, 6 | BRF_GRA },           // 25
	{ "tp-g3.d10",		0x1000, 0x1f2abec5, 6 | BRF_GRA },           // 26
	{ "tp-g2.e13",		0x1000, 0x4a7407a2, 6 | BRF_GRA },           // 27
	{ "tp-g1.f13",		0x1000, 0xf0b26c2e, 6 | BRF_GRA },           // 28
	{ "tp-g6.h2",		0x2000, 0x105cb9e4, 6 | BRF_GRA },           // 29
	{ "tp-g5.i2",		0x2000, 0x27e5e6c1, 6 | BRF_GRA },           // 30

	{ "tp-g4.c10",		0x1000, 0x99e72549, 7 | BRF_GRA },           // 31 Characters

	{ "16b",			0x0020, 0x9a12873a, 8 | BRF_GRA },           // 32 Color PROMs
	{ "16a",			0x0020, 0x90222a71, 8 | BRF_GRA },           // 33
};

STD_ROM_PICK(rjammer)
STD_ROM_FN(rjammer)

struct BurnDriver BurnDrvRjammer = {
	"rjammer", NULL, NULL, NULL, "1984",
	"Roller Jammer\0", NULL, "Nichibutsu / Alice", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, rjammerRomInfo, rjammerRomName, NULL, NULL, NULL, NULL, RjammerInputInfo, RjammerDIPInfo,
	RjammerInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	256, 224, 4, 3
};
