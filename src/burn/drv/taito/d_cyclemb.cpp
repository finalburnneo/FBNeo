// FB Neo Cycle Maabou / Sky Destroyer driver module
// Based on MAME driver by Angelo Salese, Sasuke-Arcade

// Not-a-Bug: red hindenburg (blimp) sometimes covers end-of-round
// text window, verified w/pcb

#include "tiles_generic.h"
#include "z80_intf.h"
#include "burn_ym2203.h"
#include "burn_gun.h" // trackball
#include <math.h> // abs()

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvZ80ROM[2];
static UINT8 *DrvGfxROM[3];
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM[2];
static UINT8 *DrvVidRAM;
static UINT8 *DrvColRAM;
static UINT8 *DrvObjRAM[3];

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 mcu_rxd;
static INT32 mcu_rst;
static INT32 mcu_txd;
static INT32 mcu_packet_type;
static INT32 mcu_state;
static INT32 mcu1_rst;
static INT32 dsw_pc_hack;

static INT32 soundlatch;
static INT32 bankdata;
static INT32 flipscreen;
static INT32 sprite_page;
static INT32 display_en;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvJoy5[8];
static UINT8 DrvJoy6[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[8];
static UINT8 DrvReset;

static HoldCoin<4> hold_coin;
static INT16 Analog[2];

static INT32 nCyclesExtra;

static INT32 is_cyclemb = 0;

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo CyclembInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy6 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	A("P1 Dial", 		BIT_ANALOG_REL, &Analog[0],		"p1 x-axis"	),

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy6 + 6,	"p2 coin"	}, // fake
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy4 + 4,	"p2 fire 1"	},
	A("P2 Dial", 		BIT_ANALOG_REL, &Analog[1],		"p2 x-axis"	),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Cyclemb)

static struct BurnInputInfo SkydestInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy6 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy3 + 4,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy6 + 6,	"p2 coin"	}, // fake
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy5 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy5 + 4,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy4 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy4 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy4 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy5 + 1,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Skydest)

static struct BurnDIPInfo CyclembDIPList[]=
{
	DIP_OFFSET(0x09)
	{0x00, 0xff, 0xff, 0x0a, NULL							},
	{0x01, 0xff, 0xff, 0x1f, NULL							},
	{0x02, 0xff, 0xff, 0x00, NULL							},

	{0   , 0xfe, 0   ,    4, "Difficulty (Stage 1-3)"		},
	{0x00, 0x01, 0x03, 0x03, "Easy"							},
	{0x00, 0x01, 0x03, 0x02, "Medium"						},
	{0x00, 0x01, 0x03, 0x01, "Hard"							},
	{0x00, 0x01, 0x03, 0x00, "Hardest"						},

	{0   , 0xfe, 0   ,    4, "Difficulty (Stage 4-5)"		},
	{0x00, 0x01, 0x0c, 0x0c, "Easy"							},
	{0x00, 0x01, 0x0c, 0x08, "Medium"						},
	{0x00, 0x01, 0x0c, 0x04, "Hard"							},
	{0x00, 0x01, 0x0c, 0x00, "Hardest"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x00, 0x01, 0x10, 0x10, "Off"							},
	{0x00, 0x01, 0x10, 0x00, "On"							},

	{0   , 0xfe, 0   ,    4, "Coin A"						},
	{0x01, 0x01, 0x03, 0x03, "1 Coin  1 Credits"			},
	{0x01, 0x01, 0x03, 0x02, "1 Coin  2 Credits"			},
	{0x01, 0x01, 0x03, 0x01, "1 Coin  4 Credits"			},
	{0x01, 0x01, 0x03, 0x00, "1 Coin  5 Credits"			},

	{0   , 0xfe, 0   ,    4, "Coin B"						},
	{0x01, 0x01, 0x0c, 0x00, "5 Coins 1 Credits"			},
	{0x01, 0x01, 0x0c, 0x04, "4 Coins 1 Credits"			},
	{0x01, 0x01, 0x0c, 0x08, "3 Coins 1 Credits"			},
	{0x01, 0x01, 0x0c, 0x0c, "2 Coins 1 Credits"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"						},
	{0x01, 0x01, 0x10, 0x00, "Upright"						},
	{0x01, 0x01, 0x10, 0x10, "Cocktail"						},

	{0   , 0xfe, 0   ,    2, "Free Play"					},
	{0x02, 0x01, 0x01, 0x00, "No"							},
	{0x02, 0x01, 0x01, 0x01, "Yes"							},

	{0   , 0xfe, 0   ,    2, "Disallow Game Over (Cheat)"	},
	{0x02, 0x01, 0x02, 0x00, "No"							},
	{0x02, 0x01, 0x02, 0x02, "Yes"							},

	{0   , 0xfe, 0   ,    4, "Starting Stage"				},
	{0x02, 0x01, 0x0c, 0x00, "Stage 1"						},
	{0x02, 0x01, 0x0c, 0x04, "Stage 3"						},
	{0x02, 0x01, 0x0c, 0x08, "Stage 4"						},
	{0x02, 0x01, 0x0c, 0x0c, "Bonus Game Only"				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"					},
	{0x02, 0x01, 0x10, 0x00, "Off"							},
	{0x02, 0x01, 0x10, 0x10, "On"							},

	{0   , 0xfe, 0   ,    2, "Service Mode"					},
	{0x02, 0x01, 0x80, 0x00, "Off"							},
	{0x02, 0x01, 0x80, 0x80, "On"							},
};

STDDIPINFO(Cyclemb)

static struct BurnDIPInfo SkydestDIPList[]=
{
	DIP_OFFSET(0x11)
	{0x00, 0xff, 0xff, 0x17, NULL							},
	{0x01, 0xff, 0xff, 0x1f, NULL							},
	{0x02, 0xff, 0xff, 0x08, NULL							},

	{0   , 0xfe, 0   ,    8, "Difficulty"					},
	{0x00, 0x01, 0x07, 0x07, "1 (Easiest)"					},
	{0x00, 0x01, 0x07, 0x06, "2"							},
	{0x00, 0x01, 0x07, 0x05, "3"							},
	{0x00, 0x01, 0x07, 0x04, "4"							},
	{0x00, 0x01, 0x07, 0x03, "5"							},
	{0x00, 0x01, 0x07, 0x02, "6"							},
	{0x00, 0x01, 0x07, 0x01, "7"							},
	{0x00, 0x01, 0x07, 0x00, "8 (Hardest)"					},
															
	{0   , 0xfe, 0   ,    4, "Lives"						},
	{0x00, 0x01, 0x18, 0x18, "2"							},
	{0x00, 0x01, 0x18, 0x10, "3"							},
	{0x00, 0x01, 0x18, 0x08, "4"							},
	{0x00, 0x01, 0x18, 0x00, "5"							},

	{0   , 0xfe, 0   ,    4, "Coin A"						},
	{0x01, 0x01, 0x03, 0x03, "1 Coin  1 Credits"			},
	{0x01, 0x01, 0x03, 0x02, "1 Coin  2 Credits"			},
	{0x01, 0x01, 0x03, 0x01, "1 Coin  4 Credits"			},
	{0x01, 0x01, 0x03, 0x00, "1 Coin  5 Credits"			},

	{0   , 0xfe, 0   ,    4, "Coin B"						},
	{0x01, 0x01, 0x0c, 0x00, "5 Coins 1 Credits"			},
	{0x01, 0x01, 0x0c, 0x04, "4 Coins 1 Credits"			},
	{0x01, 0x01, 0x0c, 0x08, "3 Coins 1 Credits"			},
	{0x01, 0x01, 0x0c, 0x0c, "2 Coins 1 Credits"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"						},
	{0x01, 0x01, 0x10, 0x10, "Cocktail"						},
	{0x01, 0x01, 0x10, 0x00, "Upright"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x02, 0x01, 0x01, 0x01, "Off"							},
	{0x02, 0x01, 0x01, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Free Play"					},
	{0x02, 0x01, 0x02, 0x00, "No"							},
	{0x02, 0x01, 0x02, 0x02, "Yes"							},

	{0   , 0xfe, 0   ,    2, "Unknown"						},
	{0x02, 0x01, 0x04, 0x00, "Off"							},
	{0x02, 0x01, 0x04, 0x04, "On"							},

	{0   , 0xfe, 0   ,    2, "Flip Screen"					},
	{0x02, 0x01, 0x08, 0x08, "Off"							},
	{0x02, 0x01, 0x08, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Unknown"						},
	{0x02, 0x01, 0x10, 0x00, "Off"							},
	{0x02, 0x01, 0x10, 0x10, "On"							},

	{0   , 0xfe, 0   ,    2, "Invincibility (Cheat)"		},
	{0x02, 0x01, 0x80, 0x00, "No"							},
	{0x02, 0x01, 0x80, 0x80, "Yes"							},
};

STDDIPINFO(Skydest)

static INT8 dial_last[2] = { 0, 0 };
static INT8 dial_current[2] = { 0, 0 };
static INT8 dial_mabou[2] = { 0, 0 };
static INT8 dial_reverse[2] = { 0, 0 };

static INT8 dial_read(INT32 player)
{
	dial_last[player] = dial_current[player];

	dial_current[player] = (INT8)BurnTrackballRead(0, player);

	return dial_current[player] - dial_last[player];
}

static void update_dial(INT32 player)
{
	if (is_cyclemb) {
		INT32 dial = dial_read(player);

		if (dial < -0x1f) dial = -0x1f;
		if (dial > 0x1f) dial = 0x1f;

		if (dial != 0) {
			dial_reverse[player] = dial < 0;
			dial_mabou[player] = (dial_mabou[player] + abs(dial)) & 0x1f;
		}

	}
}

static UINT8 dial_read_mabou(INT32 player)
{
	return dial_mabou[player] + (dial_reverse[player] ? 0x80 : 0x00);
}

static UINT8 skydest_i8741_read()
{
	if (ZetGetPC(-1) == dsw_pc_hack || ZetGetPrevPC(-1) == dsw_pc_hack)
	{
		mcu_rxd = (DrvDips[0] & 0x1f) << 2; // dsw1
	}
	else if (mcu_rst)
	{
		switch (mcu_state)
		{
			case 1:
			{
				mcu_packet_type^=0x20;

				if (mcu_packet_type & 0x20)
				{
					mcu_rxd = (DrvDips[2] & 0x9f) | mcu_packet_type; // dsw3
				}
				else
				{
					mcu_rxd = (DrvInputs[0] & 0x9f) | mcu_packet_type; // system
				}
			}
			break;

			case 2:
			case 3:
			{
				mcu_packet_type^=0x20;

				INT32 select = (mcu_state & 1) * 2;

				update_dial(mcu_state & 1);

				if (mcu_packet_type & 0x20)
				{
					if (is_cyclemb) {
						mcu_rxd = (dial_read_mabou(mcu_state & 1) & 0x9f) | mcu_packet_type; // p1_1 / p2_1
					} else {
						mcu_rxd = (DrvInputs[2+select] & 0x9f) | mcu_packet_type; // p1_1 / p2_1
					}
				}
				else
				{
					mcu_rxd = (DrvInputs[1+select] & 0x1f) | mcu_packet_type; // p1_0 / p2_0
					mcu_rxd |= DrvInputs[0] & 0x80; // system
				}
			}
			break;

			default:
				mcu_rxd = 0x00;
			break;

		}

		{
			UINT8 pt = 0;
			for(INT32 i=0;i<8;i++)
			{
				if(mcu_rxd & (1 << i))
					pt++;
			}
			if((pt % 2) == 1)
				mcu_rxd|=0x40;
		}
	}

	return mcu_rxd;
}

static void skydest_i8741_write(INT32 offset, UINT8 data)
{
	if(offset == 1) //command port
	{
		switch(data)
		{
			case 0:
				mcu_rxd = 0x40;
				mcu_rst = 0;
				mcu_state = 0;
			break;

			case 1:
				mcu_rxd = 0x40;
				mcu_rst = 0;
			break;

			case 2:
				mcu_rxd = (DrvDips[1] & 0x1f) << 2; // dsw2
				mcu_rst = 0;
			break;

			case 3:
				mcu_rst = 1;
				mcu_txd = 0;
			break;
		}
	}
	else
	{
		mcu_txd = data;

		mcu1_rst = 0;

		if(mcu_txd == 0x41)
			mcu_state = 1;
		else if(mcu_txd == 0x42)
			mcu_state = 2;
		else if(mcu_txd == 0x44)
			mcu_state = 3;
		else
		{
			soundlatch = data;
		}
	}
}

static void bankswitch(INT32 data)
{
	bankdata = data;
	sprite_page = (data & 4) >> 2;

	ZetMapMemory(DrvZ80ROM[0] + 0x8000 + (data & 3) * 0x1000, 0x8000, 0x8fff, MAP_ROM);
}

static void __fastcall cyclemb_write_port(UINT16 port, UINT8 data)
{
	switch (port)
	{
		case 0xc000:
			if (!is_cyclemb) {
				flipscreen = ~data & 0x40;
			}
			bankswitch(data);
		return;

		case 0xc020:
			display_en = data & 1;
		return;

		case 0xc080: // skydest
		case 0xc081:
		case 0xc09e: // cyclemb
		case 0xc09f:
			skydest_i8741_write(port & 1, data);
		return;

		case 0xc0bf:
			if (is_cyclemb) flipscreen = data & 1;
		return;
	}
}

static UINT8 __fastcall cyclemb_read_port(UINT16 port)
{
	switch (port)
	{
		case 0xc080: // skydest
		case 0xc09e: // cyclemb
			return skydest_i8741_read();

		case 0xc081: // skydest
		case 0xc09f: // cyclemb
			return 1; // i8741 status
	}

	return 0;
}

static void __fastcall cyclemb_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
			BurnYM2203Write(0, port & 1, data);
		return;

		case 0x40:
		return; // ?

		case 0x41:
			if (data == 0xf0) {
				mcu1_rst = 1;
			}
		return;
	}
}

static UINT8 __fastcall cyclemb_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
			return BurnYM2203Read(0, port & 1);

		case 0x40: {
			if (mcu1_rst == 1) return 0x40;
			UINT8 ret = soundlatch;
			soundlatch = 0;
			return ret;
		}

		case 0x41:
			return 1; // i8741 #1 status?
	}

	return 0;
}

static tilemap_callback( cyclemb )
{
	INT32 attr  = DrvColRAM[offs];
	INT32 code  = DrvVidRAM[offs] + (attr << 8);
	INT32 color = attr ^ 0xff;

	color = (color >> 3) | ((color & 4) << 3);

	TILE_SET_INFO(0, code, color, 0);
}

static tilemap_callback( skydest )
{
	INT32 attr  = DrvColRAM[offs];
	INT32 code  = DrvVidRAM[offs] + ((attr & 3) << 8);
	INT32 color = ((attr & 0xfc) >> 2) ^ 0x3f;
	color = color ^ ((attr & 0x40) ? (DrvColRAM[0] >> 4) : 0);

	TILE_SET_INFO(0, code, color, 0);
}

static tilemap_scan( skydest )
{
	INT32 offs = (64 * row) + (col + 2);

	return offs;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	bankswitch(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	BurnYM2203Reset();
	ZetClose();

	mcu_rxd = 0;
	mcu_rst = 0;
	mcu_txd = 0;
	mcu_packet_type = 0;
	mcu_state = 0;
	mcu1_rst = 0;

	soundlatch = 0;
	flipscreen = 0;
	sprite_page = 0;

	display_en = 1;

	hold_coin.reset();

	nCyclesExtra = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM[0]		= Next; Next += 0x012000;
	DrvZ80ROM[1]		= Next; Next += 0x004000;

	DrvGfxROM[0]		= Next; Next += 0x010000;
	DrvGfxROM[1]		= Next; Next += 0x040000;
	DrvGfxROM[2]		= Next; Next += 0x040000;

	DrvColPROM			= Next; Next += 0x000200;

	DrvPalette			= (UINT32*)Next; Next += 0x0100 * sizeof(UINT32);

	AllRam				= Next;

	DrvZ80RAM[0]		= Next; Next += 0x000800;
	DrvZ80RAM[1]		= Next; Next += 0x000400;
	DrvObjRAM[0]		= Next; Next += 0x000800;
	DrvObjRAM[1]		= Next; Next += 0x000800;
	DrvObjRAM[2]		= Next; Next += 0x000800;
	DrvVidRAM			= Next; Next += 0x000800;
	DrvColRAM			= Next; Next += 0x000800;

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[2]  = { 0, 4 };
	INT32 XOffs[32] = { STEP4(0,1), STEP4(64, 1), STEP4(128,1), STEP4(128+64,1),
			   			STEP4(512,1), STEP4(512+64,1), STEP4(512+128,1), STEP4(512+128+64,1)
	};
	INT32 YOffs[32] = { STEP8(0,8), STEP8(256,8), STEP8(1024,8), STEP8(1024+256,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x10000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM[0], 0x04000);

	GfxDecode(0x0400, 2,  8,  8, Plane, XOffs, YOffs, 0x080, tmp, DrvGfxROM[0]);

	memcpy (tmp, DrvGfxROM[1], 0x10000);

	GfxDecode(0x0400, 2, 16, 16, Plane, XOffs, YOffs, 0x200, tmp, DrvGfxROM[1]);
	GfxDecode(0x0100, 2, 32, 32, Plane, XOffs, YOffs, 0x800, tmp, DrvGfxROM[2]);

	BurnFree(tmp);

	return 0;
}

static INT32 CyclembInit()
{
	BurnAllocMemIndex();

	is_cyclemb = 1;

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x2000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x4000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x6000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x8000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0xa000, k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM[1] + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[1] + 0x2000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[0] + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x2000, k++, 1)) return 1;

		memset (DrvGfxROM[1], 0xff, 0x10000);
		if (BurnLoadRom(DrvGfxROM[1] + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x6000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x8000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0xa000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0xc000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0xe000, k++, 1)) return 1;

		if (BurnLoadRom(DrvColPROM   + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x0100, k++, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM[0],	0x0000, 0x7fff, MAP_ROM);
	//	bank 8000-8fff
	bankswitch(0);
	ZetMapMemory(DrvVidRAM,		0x9000, 0x97ff, MAP_RAM);
	ZetMapMemory(DrvColRAM,		0x9800, 0x9fff, MAP_RAM);
	ZetMapMemory(DrvObjRAM[0],	0xa000, 0xa7ff, MAP_RAM);
	ZetMapMemory(DrvObjRAM[1],	0xa800, 0xafff, MAP_RAM);
	ZetMapMemory(DrvObjRAM[2],	0xb000, 0xb7ff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM[0],	0xb800, 0xbfff, MAP_RAM);
	ZetSetOutHandler(cyclemb_write_port);
	ZetSetInHandler(cyclemb_read_port);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM[1],	0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM[1],	0x6000, 0x63ff, MAP_RAM);
	ZetSetOutHandler(cyclemb_sound_write_port);
	ZetSetInHandler(cyclemb_sound_read_port);
	ZetClose();

	BurnYM2203Init(1,  1500000, NULL, 0);
	BurnTimerAttachZet(3000000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE,   0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.15, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, cyclemb_map_callback, 8, 8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 2,  8,  8, 0x10000, 0x00, 0x3f);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 2, 16, 16, 0x40000, 0x00, 0x3f);
	GenericTilemapSetGfx(2, DrvGfxROM[2], 2, 32, 32, 0x40000, 0x00, 0x3f);
	GenericTilemapSetScrollRows(0, 32);
	GenericTilemapSetOffsets(0, 0, -16);

	BurnTrackballInit(2);

	// hacks
	{
		dsw_pc_hack = 0x760;
		DrvZ80ROM[1][0x282] = 0x00; // Hack audio CPU
		DrvZ80ROM[1][0x283] = 0x00;
		DrvZ80ROM[1][0x284] = 0x00;
		DrvZ80ROM[1][0xa36] = 0x00;
		DrvZ80ROM[1][0xa37] = 0x00;
		DrvZ80ROM[1][0xa38] = 0x00;
	}

	DrvDoReset();

	return 0;
}

static INT32 SkydestInit()
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x2000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x4000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x6000, k++, 1)) return 1;
		memset (DrvZ80ROM[0] + 0x8000, 0xff, 0x4000); // no banked roms

		if (BurnLoadRom(DrvZ80ROM[1] + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[1] + 0x2000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[0] + 0x0000, k++, 1)) return 1;
		for (INT32 i = 0; i < 0x4000; i++) {
			DrvGfxROM[0][i] ^= 0xff;
		}

		if (BurnLoadRom(DrvGfxROM[1] + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x2000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x4000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x6000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x8000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0xa000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0xc000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0xe000, k++, 1)) return 1;

		if (BurnLoadRom(DrvColPROM   + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x0100, k++, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM[0],	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvVidRAM,		0x9000, 0x97ff, MAP_RAM);
	ZetMapMemory(DrvColRAM,		0x9800, 0x9fff, MAP_RAM);
	ZetMapMemory(DrvObjRAM[0],	0xa000, 0xa7ff, MAP_RAM);
	ZetMapMemory(DrvObjRAM[1],	0xa800, 0xafff, MAP_RAM);
	ZetMapMemory(DrvObjRAM[2],	0xb000, 0xb7ff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM[0],	0xb800, 0xbfff, MAP_RAM);
	ZetSetOutHandler(cyclemb_write_port);
	ZetSetInHandler(cyclemb_read_port);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM[1],	0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM[1],	0x6000, 0x63ff, MAP_RAM);
	ZetSetOutHandler(cyclemb_sound_write_port);
	ZetSetInHandler(cyclemb_sound_read_port);
	ZetClose();

	BurnYM2203Init(1,  1500000, NULL, 0);
	BurnTimerAttachZet(3000000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE,   0.25, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.15, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, skydest_map_scan, skydest_map_callback, 8, 8, 60, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 2,  8,  8, 0x10000, 0x00, 0x3f);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 2, 16, 16, 0x40000, 0x00, 0x3f);
	GenericTilemapSetGfx(2, DrvGfxROM[2], 2, 32, 32, 0x40000, 0x00, 0x3f);
	GenericTilemapSetScrollCols(0, 60);
	GenericTilemapSetOffsets(0, 0, -16);

	// hacks
	{
		dsw_pc_hack = 0x554;

		DrvZ80ROM[1][0x286] = 0x00; // hack audio CPU
		DrvZ80ROM[1][0x287] = 0x00;
		DrvZ80ROM[1][0x288] = 0x00;
		DrvZ80ROM[1][0xa36] = 0x00;
		DrvZ80ROM[1][0xa37] = 0x00;
		DrvZ80ROM[1][0xa38] = 0x00;
	}

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	BurnYM2203Exit();

	BurnFreeMemIndex();

	if (is_cyclemb) {
		BurnTrackballExit();
	}

	is_cyclemb = 0;

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 256; i++)
	{
		INT32 bit0 = (DrvColPROM[i + 0x100] >> 0) & 1;
		INT32 bit1 = (DrvColPROM[i + 0x100] >> 1) & 1;
		INT32 bit2 = (DrvColPROM[i + 0x100] >> 2) & 1;
		INT32 r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = (DrvColPROM[i + 0x100] >> 3) & 1;
		bit1 = (DrvColPROM[i + 0x000] >> 0) & 1;
		bit2 = (DrvColPROM[i + 0x000] >> 1) & 1;
		INT32 g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = 0;
		bit1 = (DrvColPROM[i + 0x000] >> 2) & 1;
		bit2 = (DrvColPROM[i + 0x000] >> 3) & 1;
		INT32 b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_sprites(INT32 cylemb)
{
	INT32 page = sprite_page * 0x80;

	for (INT32 i = 0 + page; i < 0x80 + page; i+=2)
	{
		INT32 sx = DrvObjRAM[1][i+1];
		INT32 sy = DrvObjRAM[1][i+0];

		if (is_cyclemb)
		{
			sy = 0xf1 - sy;
			sx = sx - 56;
			sx += (DrvObjRAM[2][i+1] & 1) << 8;
		}
		else
		{
			sx += (DrvObjRAM[2][i+1] & 1) << 8; // must be before line below!
			sx = (0x138 - sx) - 16;
			sy = sy - 1;
		}

		INT32 code  = (DrvObjRAM[0][i+0]) + ((DrvObjRAM[2][i+0] & 3) << 8);
		INT32 color = (DrvObjRAM[0][i+1] & 0x3f);
		INT32 region = ((DrvObjRAM[2][i] & 0x10) >> 4) + 1;

		if (region == 2)
		{
			code >>= 2;
			if (is_cyclemb) { sy -= 16; } else { sx -= 16; }
		}

		GenericTilesGfx *gfx = &GenericGfxData[region];

		INT32 flipx = (DrvObjRAM[2][i+0] & 4) >> 2;
		INT32 flipy = (DrvObjRAM[2][i+0] & 8) >> 3;

		if (flipscreen)
		{
			flipx = !flipx;
			flipy = !flipy;
		}

		DrawCustomMaskTile(pTransDraw, gfx->width, gfx->height, code % gfx->code_mask, sx, sy - 16, flipx, flipy, color, gfx->depth, 0, gfx->color_offset, gfx->gfxbase);
	}
}

static void cyclemb_draw_tiles()
{
	INT32 offs = 0;
	for (INT32 y = 0; y < 32; y++) {
		for (INT32 x = 0; x < 64; x++) {
			INT32 attr = DrvColRAM[offs];
			INT32 code = DrvVidRAM[offs] + ((attr & 3) << 8);
			INT32 color = ((attr >> 3) ^ 0x1f) + ((~attr & 4) << 3);

			INT32 scrollx = (DrvVidRAM[(y / 2) + ((y&1) * 0x40)] + ((DrvColRAM[(y / 2) + ((y&1) * 0x40)] & 1) << 8) + 48) & 0x1ff;

			INT32 sx = x * 8 - scrollx; // this wont work for flipscreen calc. below.
			INT32 sy = y * 8;

			if (flipscreen) {
				DrawGfxTile(0, 0, code, 504 - x * 8 - scrollx, 248 - sy - 16, 1, 1, color);
				DrawGfxTile(0, 0, code, 504 - x * 8 - scrollx + 512, 248 - sy - 16, 1, 1, color);
			} else {
				DrawGfxTile(0, 0, code, sx, sy - 16, 0, 0, color);
				DrawGfxTile(0, 0, code, sx + 512, sy - 16, 0, 0, color);
			}

			offs++;
		}
	}
}

static INT32 CyclembDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	BurnTransferClear();

	if (display_en) {
		if (nBurnLayer & 1) cyclemb_draw_tiles();
		if (nSpriteEnable & 1) draw_sprites(1);
	}

	BurnTransferFlip(flipscreen, flipscreen);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void skydest_draw_tiles()
{
	for (INT32 y = 0; y < 32; y++) {
		for (INT32 x = 2; x < 62; x++) {
			INT32 attr = DrvColRAM[x + y * 64];
			INT32 code = DrvVidRAM[x + y * 64] + ((attr & 3) << 8);
			INT32 color = (attr >> 2) ^ 0x3f;
			if (attr & 0x40) color ^= (DrvColRAM[0] & 0xf0) >> 4;

			INT32 scrollx = (DrvVidRAM[0] + ((DrvColRAM[0] & 1) << 8)) - 192;
			INT32 scrolly = DrvVidRAM[(x & 0x1f) * 64 + (x / 32)];

			INT32 sx = x * 8 + scrollx; // this wont work for flipscreen calc. below.
			INT32 sy = ((y * 8) - scrolly) & 0xff;

			if (flipscreen) {
				DrawGfxTile(0, 0, code, 504 - x * 8 + scrollx - 16, 248 - sy - 16, 1, 1, color);
				DrawGfxTile(0, 0, code, 504 - x * 8 + scrollx - 480 - 16, 248 - sy - 16, 1, 1, color);
				DrawGfxTile(0, 0, code, 504 - x * 8 + scrollx + 480 - 16, 248 - sy - 16, 1, 1, color);
			} else {
				DrawGfxTile(0, 0, code, sx - 16, sy - 16, 0, 0, color);
				DrawGfxTile(0, 0, code, sx - 480 - 16, sy - 16, 0, 0, color);
				DrawGfxTile(0, 0, code, sx + 480 - 16, sy - 16, 0, 0, color);
			}
		}
	}
}

static INT32 SkydestDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	BurnTransferClear();

	if (display_en) {
		if (nBurnLayer & 1) skydest_draw_tiles();
		if (nSpriteEnable & 1) draw_sprites(0);
	}

	BurnTransferFlip(flipscreen, flipscreen);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();

	{
		memset(DrvInputs, 0, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i; // real coin (1<<7)
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
			DrvInputs[4] ^= (DrvJoy5[i] & 1) << i;
			DrvInputs[5] ^= (DrvJoy6[i] & 1) << i; // fake coins (1<<7), (1<<6)
		}

		hold_coin.check(0, DrvInputs[5], 1<<7, 2);
		hold_coin.check(1, DrvInputs[5], 1<<6, 2);
		DrvInputs[0] |= (DrvInputs[5] & 0x80) | ((DrvInputs[5] & 0x40) << 1);

		if (is_cyclemb) {
			BurnTrackballConfig(0, AXIS_REVERSED, AXIS_REVERSED);
			BurnTrackballFrame(0, Analog[0], Analog[1], 0x01, 0x05);
			BurnTrackballUpdate(0);
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 6000000 / 60, 3000000 / 60 };
	INT32 nCyclesDone[2] = { nCyclesExtra, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		CPU_RUN(0, Zet);
		if (i == 240) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(1);
		CPU_RUN_TIMER(1);
		if (i == 240) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		if (i == 240 && pBurnDraw) {
			BurnDrvRedraw();
		}
	}

	nCyclesExtra = nCyclesDone[0] - nCyclesTotal[0];

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
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
		BurnYM2203Scan(nAction, pnMin);
		if (is_cyclemb) BurnTrackballScan();

		hold_coin.scan();

		SCAN_VAR(mcu_rxd);
		SCAN_VAR(mcu_rst);
		SCAN_VAR(mcu_txd);
		SCAN_VAR(mcu_packet_type);
		SCAN_VAR(mcu_state);
		SCAN_VAR(mcu1_rst);
		SCAN_VAR(soundlatch);
		SCAN_VAR(bankdata);
		SCAN_VAR(flipscreen);
		SCAN_VAR(sprite_page);
		SCAN_VAR(display_en);

		if (is_cyclemb) {
			SCAN_VAR(dial_last);
			SCAN_VAR(dial_current);
			SCAN_VAR(dial_mabou);
			SCAN_VAR(dial_reverse);
		}

		SCAN_VAR(nCyclesExtra);
	}

	if (nAction & ACB_WRITE)
	{
		ZetOpen(0);
		bankswitch(bankdata);
		ZetClose();
	}

	return 0;
}


// Cycle Maabou (Japan)

static struct BurnRomInfo cyclembRomDesc[] = {
	{ "p0_1.1a",	0x2000, 0xa1588264, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "p0_2.1b",	0x2000, 0x04141837, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "p0_3.1c",	0x2000, 0xa9dd4b22, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "p0_4.1e",	0x2000, 0x456a30df, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "p0_5.1f",	0x2000, 0xa3b9c297, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "p0_6.1h",	0x2000, 0xec76a0a6, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "p0_15.10c",	0x2000, 0x9cc52c32, 2 | BRF_PRG | BRF_ESS }, //  6 Z80 #1 Code
	{ "p0_16.10d",	0x2000, 0x8d03227e, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "p0_21.3e",	0x2000, 0xa7dab6d8, 3 | BRF_GRA },           //  8 Characters
	{ "p0_20.3d",	0x2000, 0x53e3a36e, 3 | BRF_GRA },           //  9

	{ "p0_7.1k",	0x2000, 0x6507d23f, 4 | BRF_GRA },           // 10 Sprites
	{ "p0_10.1n",	0x2000, 0xa98415db, 4 | BRF_GRA },           // 11
	{ "p0_11.1r",	0x2000, 0x626556fe, 4 | BRF_GRA },           // 12
	{ "p0_12.1s",	0x2000, 0x1e08902c, 4 | BRF_GRA },           // 13
	{ "p0_13.1t",	0x2000, 0x086639c1, 4 | BRF_GRA },           // 14
	{ "p0_14.1u",	0x2000, 0x3f5fe2b6, 4 | BRF_GRA },           // 15

	{ "p0_3.11t",	0x0100, 0xbe89c1f7, 5 | BRF_GRA },           // 16 Color PROMs
	{ "p0_4.11u",	0x0100, 0x4886d832, 5 | BRF_GRA },           // 17

	{ "p1.2e",		0x0020, 0x6297104c, 0 | BRF_PRG | BRF_OPT},  // 18 Timing PROMs
	{ "p2.4e",		0x0020, 0x70a09cc5, 0 | BRF_PRG | BRF_OPT }, // 19
};

STD_ROM_PICK(cyclemb)
STD_ROM_FN(cyclemb)

struct BurnDriver BurnDrvCyclemb = {
	"cyclemb", NULL, NULL, NULL, "1984",
	"Cycle Maabou (Japan)\0", NULL, "Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, cyclembRomInfo, cyclembRomName, NULL, NULL, NULL, NULL, CyclembInputInfo, CyclembDIPInfo,
	CyclembInit, DrvExit, DrvFrame, CyclembDraw, DrvScan, &DrvRecalc, 0x100,
	256, 224, 4, 3
};


// Sky Destroyer (Japan)

static struct BurnRomInfo skydestRomDesc[] = {
	{ "pd1-1.1a",	0x2000, 0x78951c75, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "pd0-2.1b",	0x2000, 0xda2d48cd, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "pd0-3.1c",	0x2000, 0x28ef8eda, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "pd1-4.1e",	0x2000, 0xb8ec9938, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "pd0-15.10c",	0x2000, 0xf8b3d3f7, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code
	{ "pd0-16.10d",	0x2000, 0x19ce8106, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "pd0-20.1h",	0x4000, 0x8b2137f2, 3 | BRF_GRA },           //  6 Characters

	{ "pd0-7.1k",	0x2000, 0x83137d42, 4 | BRF_GRA },           //  7 Sprites
	{ "pd1-8.1l",	0x2000, 0xb810858b, 4 | BRF_GRA },           //  8
	{ "pd0-9.1m",	0x2000, 0x6f558bee, 4 | BRF_GRA },           //  9
	{ "pd1-10.1n",	0x2000, 0x5840b5b5, 4 | BRF_GRA },           // 10
	{ "pd0-11.1r",	0x2000, 0x29e5fce4, 4 | BRF_GRA },           // 11
	{ "pd0-12.1s",	0x2000, 0x06234942, 4 | BRF_GRA },           // 12
	{ "pd1-13.1t",	0x2000, 0x3cca5b95, 4 | BRF_GRA },           // 13
	{ "pd0-14.1u",	0x2000, 0x7ef05b01, 4 | BRF_GRA },           // 14

	{ "green.11t",	0x0100, 0xf803beb7, 5 | BRF_GRA },           // 15 Color PROMs
	{ "red.11u",	0x0100, 0x24b7b6f3, 5 | BRF_GRA },           // 16

	{ "p1.2e",		0x0020, 0x00000000, 6 | BRF_NODUMP | BRF_PRG | BRF_OPT },           // 17 Timing PROMs
	{ "p0.4e",		0x0020, 0x00000000, 6 | BRF_NODUMP | BRF_PRG | BRF_OPT },           // 18

	{ "blue.4j",	0x0100, 0x34579681, 7 | BRF_OPT },           // 19 Unknown PROM
};

STD_ROM_PICK(skydest)
STD_ROM_FN(skydest)

struct BurnDriver BurnDrvSkydest = {
	"skydest", NULL, NULL, NULL, "1985",
	"Sky Destroyer (Japan)\0", NULL, "Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, skydestRomInfo, skydestRomName, NULL, NULL, NULL, NULL, SkydestInputInfo, SkydestDIPInfo,
	SkydestInit, DrvExit, DrvFrame, SkydestDraw, DrvScan, &DrvRecalc, 0x100,
	256, 224, 4, 3
};
