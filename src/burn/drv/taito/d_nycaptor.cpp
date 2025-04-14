// FinalBurn Neo N.Y. Captor driver module
// Based on MAME driver by Tomasz Slanina

#include "tiles_generic.h"
#include "z80_intf.h"
#include "taito_m68705.h"
#include "ay8910.h"
#include "msm5232.h"
#include "dac.h"
#include "burn_pal.h"
#include "burn_gun.h"
#include "bitswap.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvZ80ROM[3];
static UINT8 *DrvMCUROM;
static UINT8 *DrvGfxROM[2];
static UINT8 *DrvVidRAM;
static UINT8 *DrvZ80RAM[3];
static UINT8 *DrvShareRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvMCURAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 nExtraCycles[3];

static INT32 rombank;
static INT32 palettebank;
static INT32 generic_control_reg;
static INT32 gfx_control;
static INT32 character_bank;
static INT32 soundlatch[2];
static INT32 nmi_pending;
static INT32 nmi_enable;

static INT32 is_game = 0;
static INT32 spot_data;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[2];
static INT16 DrvAnalog[2];
static UINT8 DrvReset;

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}

static struct BurnInputInfo NycaptorInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 coin"		},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"		},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 1"		},
	A("P1 Gun X",       BIT_ANALOG_REL, &DrvAnalog[0],  "mouse x-axis"	),
	A("P1 Gun Y",       BIT_ANALOG_REL, &DrvAnalog[1],  "mouse y-axis"	),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"			},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 2,	"service"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 3,	"tilt"			},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"			},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"			},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"			},
};

STDINPUTINFO(Nycaptor)

static struct BurnDIPInfo NycaptorDIPList[]=
{
	{0x08, 0xff, 0xff, 0xbf, NULL						},
	{0x09, 0xff, 0xff, 0x00, NULL						},
	{0x0a, 0xff, 0xff, 0xfb, NULL						},

	{0   , 0xfe, 0   ,    4, "Bonus Life"				},
	{0x08, 0x01, 0x03, 0x02, "20k 80k 80k+"				},
	{0x08, 0x01, 0x03, 0x03, "50k 150k 200k+"			},
	{0x08, 0x01, 0x03, 0x01, "100k 300k 300k+"			},
	{0x08, 0x01, 0x03, 0x00, "150k 300k 300k+"			},

	{0   , 0xfe, 0   ,    2, "Infinite Bullets"			},
	{0x08, 0x01, 0x04, 0x04, "Off"						},
	{0x08, 0x01, 0x04, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x08, 0x01, 0x18, 0x08, "1"						},
	{0x08, 0x01, 0x18, 0x00, "2"						},
	{0x08, 0x01, 0x18, 0x18, "3"						},
	{0x08, 0x01, 0x18, 0x10, "5"						},

	{0   , 0xfe, 0   ,    2, "Free Play"				},
	{0x08, 0x01, 0x20, 0x20, "Off"						},
	{0x08, 0x01, 0x20, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Allow Continue"			},
	{0x08, 0x01, 0x40, 0x40, "Off"						},
	{0x08, 0x01, 0x40, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x08, 0x01, 0x80, 0x00, "Off"						},
	{0x08, 0x01, 0x80, 0x80, "On"						},

	{0   , 0xfe, 0   ,    16, "Coin A"					},
	{0x09, 0x01, 0x0f, 0x0f, "9 Coins 1 Credits"		},
	{0x09, 0x01, 0x0f, 0x0e, "8 Coins 1 Credits"		},
	{0x09, 0x01, 0x0f, 0x0d, "7 Coins 1 Credits"		},
	{0x09, 0x01, 0x0f, 0x0c, "6 Coins 1 Credits"		},
	{0x09, 0x01, 0x0f, 0x0b, "5 Coins 1 Credits"		},
	{0x09, 0x01, 0x0f, 0x0a, "4 Coins 1 Credits"		},
	{0x09, 0x01, 0x0f, 0x09, "3 Coins 1 Credits"		},
	{0x09, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"		},
	{0x09, 0x01, 0x0f, 0x00, "1 Coin  1 Credits"		},
	{0x09, 0x01, 0x0f, 0x01, "1 Coin  2 Credits"		},
	{0x09, 0x01, 0x0f, 0x02, "1 Coin  3 Credits"		},
	{0x09, 0x01, 0x0f, 0x03, "1 Coin  4 Credits"		},
	{0x09, 0x01, 0x0f, 0x04, "1 Coin  5 Credits"		},
	{0x09, 0x01, 0x0f, 0x05, "1 Coin  6 Credits"		},
	{0x09, 0x01, 0x0f, 0x06, "1 Coin  7 Credits"		},
	{0x09, 0x01, 0x0f, 0x07, "1 Coin  8 Credits"		},

	{0   , 0xfe, 0   ,    16, "Coin B"					},
	{0x09, 0x01, 0xf0, 0xf0, "9 Coins 1 Credits"		},
	{0x09, 0x01, 0xf0, 0xe0, "8 Coins 1 Credits"		},
	{0x09, 0x01, 0xf0, 0xd0, "7 Coins 1 Credits"		},
	{0x09, 0x01, 0xf0, 0xc0, "6 Coins 1 Credits"		},
	{0x09, 0x01, 0xf0, 0xb0, "5 Coins 1 Credits"		},
	{0x09, 0x01, 0xf0, 0xa0, "4 Coins 1 Credits"		},
	{0x09, 0x01, 0xf0, 0x90, "3 Coins 1 Credits"		},
	{0x09, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"		},
	{0x09, 0x01, 0xf0, 0x00, "1 Coin  1 Credits"		},
	{0x09, 0x01, 0xf0, 0x10, "1 Coin  2 Credits"		},
	{0x09, 0x01, 0xf0, 0x20, "1 Coin  3 Credits"		},
	{0x09, 0x01, 0xf0, 0x30, "1 Coin  4 Credits"		},
	{0x09, 0x01, 0xf0, 0x40, "1 Coin  5 Credits"		},
	{0x09, 0x01, 0xf0, 0x50, "1 Coin  6 Credits"		},
	{0x09, 0x01, 0xf0, 0x60, "1 Coin  7 Credits"		},
	{0x09, 0x01, 0xf0, 0x70, "1 Coin  8 Credits"		},

	{0   , 0xfe, 0   ,    2, "Freeze"					},
	{0x0a, 0x01, 0x01, 0x01, "Off"						},
	{0x0a, 0x01, 0x01, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Training Spot"			},
	{0x0a, 0x01, 0x02, 0x00, "No"						},
	{0x0a, 0x01, 0x02, 0x02, "Yes"						},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x0a, 0x01, 0x0c, 0x0c, "Easy"						},
	{0x0a, 0x01, 0x0c, 0x08, "Normal"					},
	{0x0a, 0x01, 0x0c, 0x04, "Hard"						},
	{0x0a, 0x01, 0x0c, 0x00, "Hardest"					},

	{0   , 0xfe, 0   ,    2, "Coinage Display"			},
	{0x0a, 0x01, 0x10, 0x00, "No"						},
	{0x0a, 0x01, 0x10, 0x10, "Yes"						},

	{0   , 0xfe, 0   ,    2, "Reset Damage"				},
	{0x0a, 0x01, 0x20, 0x20, "Every Stage"				},
	{0x0a, 0x01, 0x20, 0x00, "Every 4 Stages"			},

	{0   , 0xfe, 0   ,    2, "No Hit (Cheat)"			},
	{0x0a, 0x01, 0x40, 0x40, "Off"						},
	{0x0a, 0x01, 0x40, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Coin Slots"				},
	{0x0a, 0x01, 0x80, 0x00, "1"						},
	{0x0a, 0x01, 0x80, 0x80, "2"						},
};

STDDIPINFO(Nycaptor)

static struct BurnDIPInfo ColtDIPList[] = {
	{0,	0xFE, 0,	4,  "Lives"							},
	{0x08,	0x01, 0x18,	0x08, "1"						},
	{0x08,	0x01, 0x18,	0x10, "2"						},
	{0x08,	0x01, 0x18,	0x18, "3"						},
	{0x08,	0x01, 0x18,	0x00, "100"						},
};

STDDIPINFOEXT(Colt, Nycaptor, Colt)

static struct BurnDIPInfo CyclshtgDIPList[]=
{
	{0x08, 0xff, 0xff, 0xff, NULL						},
	{0x09, 0xff, 0xff, 0xfb, NULL						},
	{0x0a, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x08, 0x01, 0x04, 0x04, "Off"						},
	{0x08, 0x01, 0x04, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x08, 0x01, 0x08, 0x00, "Off"						},
	{0x08, 0x01, 0x08, 0x08, "On"						},

	{0   , 0xfe, 0   ,    4, "Coin A"					},
	{0x08, 0x01, 0x30, 0x10, "2 Coins 1 Credits"		},
	{0x08, 0x01, 0x30, 0x30, "1 Coin  1 Credits"		},
	{0x08, 0x01, 0x30, 0x00, "2 Coins 3 Credits"		},
	{0x08, 0x01, 0x30, 0x20, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"					},
	{0x08, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"		},
	{0x08, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"		},
	{0x08, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"		},
	{0x08, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x09, 0x01, 0x03, 0x02, "Easy"						},
	{0x09, 0x01, 0x03, 0x03, "Medium"					},
	{0x09, 0x01, 0x03, 0x01, "Hard"						},
	{0x09, 0x01, 0x03, 0x00, "Hardest"					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"				},
	{0x09, 0x01, 0x0c, 0x0c, "150k 350k 200k+"			},
	{0x09, 0x01, 0x0c, 0x08, "200k 500k 300k+"			},
	{0x09, 0x01, 0x0c, 0x04, "300k 700k 400k+"			},
	{0x09, 0x01, 0x0c, 0x00, "400k 900k 500k+"			},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x09, 0x01, 0x30, 0x00, "1"						},
	{0x09, 0x01, 0x30, 0x30, "3"						},
	{0x09, 0x01, 0x30, 0x10, "4"						},
	{0x09, 0x01, 0x30, 0x20, "5"						},

	{0   , 0xfe, 0   ,    2, "Reset Damage (Cheat)"		},
	{0x09, 0x01, 0x80, 0x80, "Off"						},
	{0x09, 0x01, 0x80, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Infinite Bullets"			},
	{0x0a, 0x01, 0x10, 0x10, "Off"						},
	{0x0a, 0x01, 0x10, 0x00, "On"						},
};

STDDIPINFO(Cyclshtg)

static struct BurnDIPInfo BronxDIPList[] = {
	{0	,	0xFE, 0,	4,  "Lives"						},
	{0x09,	0x01, 0x30,	0x00, "1"						},
	{0x09,	0x01, 0x30,	0x30, "2"						},
	{0x09,	0x01, 0x30,	0x10, "4"						},
	{0x09,	0x01, 0x30,	0x20, "5"						},
};

STDDIPINFOEXT(Bronx, Cyclshtg, Bronx)

static UINT8 read_gun_y()
{
	INT32 y = BurnGunReturnY(0);
	INT32 deviate = 0;

	//curvature correction (y-axis) - dink july 2021
	//  0 - 120
	//150 - 255

	if (y > 150) {
		deviate = (y - 150) / 8;
		deviate = -deviate;
	} else if (y < 120) {
		deviate = (120 - y) / 8;
	}

	return y + deviate - 8;
}

static void set_rombank(INT32 data)
{
	rombank = data;

	INT32 bank = data & 3;
	ZetMapMemory(DrvZ80ROM[0] + 0x10000 + bank * 0x4000, 0x8000, 0xbfff, MAP_ROM);
}

static void set_palettebank(INT32 data)
{
	palettebank = data & 1;

	INT32 bank = palettebank * 0x100;

	ZetMapMemory(DrvPalRAM + 0x000 + bank,				0xdd00, 0xddff, MAP_RAM);
	ZetMapMemory(DrvPalRAM + 0x200 + bank,				0xde00, 0xdeff, MAP_RAM);

	ZetCPUPush(ZetGetActive() ^ 1);
	ZetMapMemory(DrvPalRAM + 0x000 + bank,				0xdd00, 0xddff, MAP_RAM);
	ZetMapMemory(DrvPalRAM + 0x200 + bank,				0xde00, 0xdeff, MAP_RAM);
	ZetCPUPop();
}

static void __fastcall nycaptor_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xd000:
			if (BurnDrvGetFlags() & BDF_BOOTLEG) return;
			standard_taito_mcu_write(data);
		return;

		case 0xd001:
			ZetSetHALT(1, (data) ? 1 : 0);
		return;

		case 0xd002:
			generic_control_reg = data;
			if ((BurnDrvGetFlags() & BDF_BOOTLEG) || is_game != 0) {
				set_rombank((data >> 2) & 3); // cyclshtg, bronx
			} else {
				set_rombank((data >> 3) & 1); // nycaptor, colt
			}

			ZetSetRESETLine(1, ~data & 2);
		return;

		case 0xd400:
			soundlatch[0] = data | 0x100;
			if (nmi_enable == 0) {
				nmi_pending = 1;
			} else {
				ZetNmi(2);
			}
		return;

		case 0xd403:
			ZetSetRESETLine(2, data & 1);
		return;

		case 0xdf03:
			gfx_control		= data;
			character_bank	= (data & 0x18) >> 3;
			set_palettebank((data & 0x20) >> 5);
		return;
	}
}

static UINT8 __fastcall nycaptor_main_read(UINT16 address)
{
	switch (address)
	{
	// main only
		case 0xd000: {
			if (BurnDrvGetFlags() & BDF_BOOTLEG) return 7;
			return standard_taito_mcu_read();
		}

		case 0xd002:
			return generic_control_reg;

		case 0xd400:
			soundlatch[1] &= 0xff;
			return soundlatch[1];

		case 0xd401:
			return BurnRandom(); // nop

	// main and sub
		case 0xd800:
			return DrvDips[0];

		case 0xd801:
			return DrvDips[1];

		case 0xd802:
			return DrvDips[2];

		case 0xd803:
			return DrvInputs[0];

		case 0xd804:
			return DrvInputs[1];

	// main only
		case 0xd805:
			if (BurnDrvGetFlags() & BDF_BOOTLEG) return (ZetGetActive() ? BurnRandom() : 0xff);
			return mcu_sent ? 0x02 : 0;

		case 0xd806:
			return ((soundlatch[0] & 0x100) ? 1 : 0) | ((soundlatch[1] & 0x100) ? 2 : 0);

		case 0xd807:
			if (BurnDrvGetFlags() & BDF_BOOTLEG) return 0xff;
			return main_sent ? 0 : 0x01;

	// sub only
		case 0xdf00:
			return (BurnGunReturnX(0) + 0x27) | 1;

		case 0xdf01:
			return read_gun_y();

		case 0xdf02:
			return 1; // b

	// main and sub
		case 0xdf03:
			return gfx_control;
	}

	return 0;
}

static UINT8 __fastcall bronx_port_read(UINT16 port)
{
	if (port < 0x8000) {
		return DrvZ80ROM[1][0x8000 + port];
	}

	return 0;
}

static void __fastcall nycaptor_sound_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf000) == 0xe000) return; // nop

	if ((address & 0xfff0) == 0xc900) {
		MSM5232Write(address & 0xf, data);
		return;
	}

	switch (address)
	{
		case 0xc800:
		case 0xc801:
		case 0xc802:
		case 0xc803:
			AY8910Write((address / 2) & 1, address & 1, data);
		return;

		case 0xca00:
		case 0xcb00:
		case 0xcc00:
		return; // nop

		case 0xd000:
			soundlatch[1] = data | 0x100;
		return;

		case 0xd200:
			nmi_enable = 1;
			if (nmi_pending) {
				ZetNmi();
				nmi_pending = 0;
			}
		return;

		case 0xd400:
			nmi_enable = 0;
		return;

		case 0xd600:
			DACWrite(0, data);
		return;
	}
}

static UINT8 __fastcall nycaptor_sound_read(UINT16 address)
{
	if ((address & 0xf000) == 0xe000) return 0; // nop

	switch (address)
	{
		case 0xd000:
			soundlatch[0] &= 0xff;
			return soundlatch[0];

		case 0xd200:
			return 0; // nop
	}

	return 0;
}

static tilemap_callback( bg )
{
	INT32 attr  = DrvVidRAM[offs * 2 + 1];
	INT32 code  = DrvVidRAM[offs * 2 + 0] | ((attr & 0xc0) << 2) | (character_bank * 0x400);
	INT32 color = attr & 0x0f;
	INT32 cat   =(attr & 0x30) >> 4;

	INT32 group = 0;

	switch (spot_data % 4)
	{
		case 0:
			if (color == 0x06) group = 1;
		break;

		case 1:
			if (color == 0x0c) group = 2;
			if (cat == 0x02) group = 3;
			if (code == 0xe09 || (code >= 0xe47 && code <= 0xe4f)) {cat = 3; group = 1;} // spot 6 intro fix
		break;

		case 3:
			if (color == 0x08) group = 2;
		break;
	}

	TILE_SET_INFO(0, code, color, TILE_GROUP(cat));
	sTile->category = group; // note: our category == mame group
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	set_rombank(0);
	set_palettebank(0);
	ZetClose();

	ZetReset(1);
	ZetReset(2);

	m67805_taito_reset();

	AY8910Reset(0);
	AY8910Reset(1);
	MSM5232Reset();

	DACReset();

	rombank = 0;
	palettebank = 0;
	generic_control_reg = 0;
	gfx_control = 0;
	character_bank = 0;
	soundlatch[0] = 0;
	soundlatch[1] = 0;
	nmi_pending = 0;
	nmi_enable = 0;

	nExtraCycles[0] = nExtraCycles[1] = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM[0]		= Next; Next += 0x028000;
	DrvZ80ROM[1]		= Next; Next += 0x010000;
	DrvZ80ROM[2]		= Next; Next += 0x010000;

	DrvMCUROM			= Next; Next += 0x000800;

	DrvGfxROM[0]		= Next; Next += 0x040000;
	DrvGfxROM[1]		= Next; Next += 0x040000;

	DrvPalette			= (UINT32*)Next; Next += 0x0200 * sizeof(UINT32);

	AllRam				= Next;

	DrvVidRAM			= Next; Next += 0x001000;
	DrvZ80RAM[2]		= Next; Next += 0x000800;
	DrvShareRAM			= Next; Next += 0x002000;
	DrvPalRAM			= Next; Next += 0x000400;
	DrvSprRAM			= Next; Next += 0x000100;

	DrvMCURAM			= Next; Next += 0x000080;

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static void DrvPrgDecode() // bootlegs
{
	for (INT32 i = 0; i < 0x20000; i++) {
		DrvZ80ROM[0][i] = BITSWAP08(DrvZ80ROM[0][i], 0, 1, 2, 3, 4, 5, 6, 7);
	}
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[4]  = { 0x10000*8+0, 0x10000*8+4, 0, 4 };
	INT32 XOffs[16] = { STEP4(3,-1), STEP4(8+3,-1), STEP4(128+3,-1), STEP4(128+8+3,-1) };
	INT32 YOffs[16] = { STEP8(0,16), STEP8(256,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x20000);
	if (tmp == NULL) {
		return 1;
	}

	for (INT32 i = 0; i < 0x20000; i++) tmp[i] = DrvGfxROM[0][i] ^ 0xff;

	GfxDecode(0x1000, 4,  8,  8, Plane, XOffs, YOffs, 0x080, tmp, DrvGfxROM[0]);
	GfxDecode(0x0400, 4, 16, 16, Plane, XOffs, YOffs, 0x200, tmp, DrvGfxROM[1]);

	BurnFree (tmp);

	return 0;
}

static INT32 DrvInit(INT32 game_select)
{
	BurnAllocMemIndex();

	INT32 k = 0;

	if (game_select == 0) // nycaptor
	{
		if (BurnLoadRom(DrvZ80ROM[0] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x04000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x10000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x14000, k++, 1)) return 1;
		memcpy (DrvZ80ROM[0] + 0x18000, DrvZ80ROM[0] + 0x10000, 0x08000);

		if (BurnLoadRom(DrvZ80ROM[1] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[1] + 0x04000, k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM[2] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[2] + 0x04000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[0] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x04000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x08000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x0c000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x10000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x14000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x18000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x1c000, k++, 1)) return 1;

		if (BurnLoadRom(DrvMCUROM    + 0x00000, k++, 1)) return 1;

		is_game = 0x00;
	}

	if (game_select == 1) // colt
	{
		if (BurnLoadRom(DrvZ80ROM[0] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x04000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x10000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x18000, k++, 1)) return 1;
		memcpy (DrvZ80ROM[0] + 0x14000, DrvZ80ROM[0] + 0x10000, 0x04000);
		memcpy (DrvZ80ROM[0] + 0x1c000, DrvZ80ROM[0] + 0x18000, 0x04000);

		if (BurnLoadRom(DrvZ80ROM[1] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[1] + 0x04000, k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM[2] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[2] + 0x04000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[0] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x04000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x08000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x0c000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x10000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x14000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x18000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x1c000, k++, 1)) return 1;

		DrvPrgDecode();

		is_game = 0x00;
	}

	if (game_select == 2) // bronx
	{
		if (BurnLoadRom(DrvZ80ROM[0] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x04000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x10000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x18000, k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM[1] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[1] + 0x04000, k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM[1] + 0x08000, k++, 1)) return 1; // SUB IO

		if (BurnLoadRom(DrvZ80ROM[2] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[2] + 0x04000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[0] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x04000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x08000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x0c000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x10000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x14000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x18000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x1c000, k++, 1)) return 1;

		DrvPrgDecode();

		is_game = 0x30;
	}

	if (game_select == 3) // cyclesht
	{
		if (BurnLoadRom(DrvZ80ROM[0] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x04000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x10000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x14000, k++, 1)) return 1;
		//memcpy (DrvZ80ROM[0] + 0x18000, DrvZ80ROM[0] + 0x10000, 0x08000);

		if (BurnLoadRom(DrvZ80ROM[1] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[1] + 0x04000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[1] + 0x08000, k++, 1)) return 1; // SUB IO

		if (BurnLoadRom(DrvZ80ROM[2] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[2] + 0x04000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[0] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x04000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x08000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x0c000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x10000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x14000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x18000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x1c000, k++, 1)) return 1;

		//if (BurnLoadRom(DrvMCUROM    + 0x00000, k++, 1)) return 1;

		is_game = 0x30;

		return 1; // unsupported for now
	}

	DrvGfxDecode();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM[0],			0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvVidRAM,				0xc000, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,				0xdc00, 0xdcff, MAP_RAM); // 00-9f sprite, a0-bf scroll c0+ nop
	ZetMapMemory(DrvShareRAM,			0xe000, 0xffff, MAP_RAM); // banked
	ZetSetWriteHandler(nycaptor_main_write);
	ZetSetReadHandler(nycaptor_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM[1],			0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvVidRAM,				0xc000, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,				0xdc00, 0xdcff, MAP_RAM); // 00-9f sprite, a0-bf scroll c0+ nop
	ZetMapMemory(DrvShareRAM,			0xe000, 0xffff, MAP_RAM);
	ZetSetWriteHandler(nycaptor_main_write);
	ZetSetReadHandler(nycaptor_main_read);
	ZetSetInHandler(bronx_port_read);
	ZetClose();

	ZetInit(2);
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM[2],			0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM[2],			0xc000, 0xc7ff, MAP_RAM);
	ZetSetWriteHandler(nycaptor_sound_write);
	ZetSetReadHandler(nycaptor_sound_read);
	ZetClose();

	m67805_taito_init(DrvMCUROM, DrvMCURAM, &standard_m68705_interface);

	AY8910Init(0, 2000000, 0);
	AY8910Init(1, 2000000, 0);
	AY8910SetAllRoutes(0, 0.05, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.05, BURN_SND_ROUTE_BOTH);

	MSM5232Init(2000000, 1);
	MSM5232SetCapacitors(1e-6, 1e-6, 1e-6, 1e-6, 1e-6, 1e-6, 1e-6, 1e-6);
	MSM5232SetRoute(1.00, BURN_SND_MSM5232_ROUTE_0);
	MSM5232SetRoute(1.00, BURN_SND_MSM5232_ROUTE_1);
	MSM5232SetRoute(1.00, BURN_SND_MSM5232_ROUTE_2);
	MSM5232SetRoute(1.00, BURN_SND_MSM5232_ROUTE_3);
	MSM5232SetRoute(1.00, BURN_SND_MSM5232_ROUTE_4);
	MSM5232SetRoute(1.00, BURN_SND_MSM5232_ROUTE_5);
	MSM5232SetRoute(1.00, BURN_SND_MSM5232_ROUTE_6);
	MSM5232SetRoute(1.00, BURN_SND_MSM5232_ROUTE_7);

	DACInit(0, 0, 1, ZetTotalCycles, 4000000);
	DACSetRoute(0, 0.40, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 4,  8,  8, 0x40000, 0x000, 0xf);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 4, 16, 16, 0x40000, 0x100, 0xf);
	GenericTilemapSetScrollCols(0, 32);
	GenericTilemapSetTransSplit(0, 0, 0xf800, 0x07ff);
	GenericTilemapSetTransSplit(0, 1, 0xfe00, 0x01ff);
	GenericTilemapSetTransSplit(0, 2, 0xfffc, 0x0003);
	GenericTilemapSetTransSplit(0, 3, 0xfff0, 0x000f);

	BurnGunInit(1, true);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	m67805_taito_exit();

	AY8910Exit(0);
	AY8910Exit(1);
	MSM5232Exit();
	DACExit();

	BurnGunExit();

	BurnFreeMemIndex();

	is_game = 0;

	return 0;
}

static void DrvPaletteUpdate()
{
	for (INT32 i = 0; i < 0x200; i++)
	{
		DrvPalette[i] = BurnHighCol(pal4bit(DrvPalRAM[i]), pal4bit(DrvPalRAM[i] >> 4), pal4bit(DrvPalRAM[i + 0x200]), 0);
	}
}

static void draw_sprites(INT32 pri)
{
	for (INT32 i = 0; i < 0x20; i++)
	{
		INT32 pr = DrvSprRAM[0x9f - i];
		INT32 priority = (pr & 0xe0) >> 5;

		if (priority == pri)
		{
			INT32 offs	= (pr & 0x1f) * 4;

			INT32 sy	= DrvSprRAM[offs + 0];
			INT32 attr	= DrvSprRAM[offs + 1];
			INT32 code	= DrvSprRAM[offs + 2] + ((attr & 0x10) << 4);
			INT32 sx	= DrvSprRAM[offs + 3];
			INT32 color	= attr & 0x0f;
			INT32 flipx = attr & 0x40;
			INT32 flipy = attr & 0x80;

			DrawGfxMaskTile(0, 1, code, sx, (240 - sy) - 16, flipx, flipy, color, 0xf);
			DrawGfxMaskTile(0, 1, code, sx - 256, (240 - sy) - 16, flipx, flipy, color, 0xf);
		}
	}
}

static UINT8 packedbcd_to_dec(UINT8 pbcd)
{
	return ((pbcd >> 4) & 0xf) * 10 + (pbcd & 0xf);
}

static void Update_Spot()
{
	spot_data = 0;

	if (is_game == 0) { // nycaptor
		// get the scene from ram, each scene (spot) uses a different priority system
		spot_data = packedbcd_to_dec(DrvShareRAM[0x296]);
		if (spot_data > 0) spot_data--;
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1; // force update
	}

	BurnTransferClear();

	for (INT32 i = 0; i < 32; i++) {
		GenericTilemapSetScrollCol(0, i, DrvSprRAM[0xa0 + i] + 0x10);
	}

	switch (spot_data & 3)
	{
		case 0:
			if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, TMAP_DRAWLAYER1 | TMAP_SET_GROUP(3));
			if (nSpriteEnable & 0x10) draw_sprites(6);
			if (nBurnLayer & 2) GenericTilemapDraw(0, pTransDraw, TMAP_DRAWLAYER0 | TMAP_SET_GROUP(3));
			if (nBurnLayer & 4) GenericTilemapDraw(0, pTransDraw, TMAP_DRAWLAYER1 | TMAP_SET_GROUP(2));
			if (nBurnLayer & 8) GenericTilemapDraw(0, pTransDraw, TMAP_DRAWLAYER0 | TMAP_SET_GROUP(2));
			if (nSpriteEnable & 1) GenericTilemapDraw(0, pTransDraw, TMAP_DRAWLAYER1 | TMAP_SET_GROUP(1));
			if (nSpriteEnable & 0x20) draw_sprites(3);
			if (nSpriteEnable & 2) GenericTilemapDraw(0, pTransDraw, TMAP_DRAWLAYER0 | TMAP_SET_GROUP(1));
			if (nSpriteEnable & 0x40) draw_sprites(0);
			if (nSpriteEnable & 0x40) draw_sprites(2);
			if (nSpriteEnable & 4) GenericTilemapDraw(0, pTransDraw, TMAP_DRAWLAYER1 | TMAP_SET_GROUP(0));
			if (nSpriteEnable & 0x80) draw_sprites(1);
			if (nSpriteEnable & 8) GenericTilemapDraw(0, pTransDraw, TMAP_DRAWLAYER0 | TMAP_SET_GROUP(0));
		break;

		case 1:
			if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, TMAP_DRAWLAYER1 | TMAP_SET_GROUP(3));
			if (nBurnLayer & 2) GenericTilemapDraw(0, pTransDraw, TMAP_DRAWLAYER0 | TMAP_SET_GROUP(3));
			if (nSpriteEnable & 0x10) draw_sprites(3);
			if (nSpriteEnable & 0x20) draw_sprites(2);
			if (nBurnLayer & 4) GenericTilemapDraw(0, pTransDraw, TMAP_DRAWLAYER1 | TMAP_SET_GROUP(2));
			if (nBurnLayer & 8) GenericTilemapDraw(0, pTransDraw, TMAP_DRAWLAYER1 | TMAP_SET_GROUP(1));
			if (nSpriteEnable & 0x40) draw_sprites(1);
			if (nSpriteEnable & 1) GenericTilemapDraw(0, pTransDraw, TMAP_DRAWLAYER0 | TMAP_SET_GROUP(1));
			if (nSpriteEnable & 2) GenericTilemapDraw(0, pTransDraw, TMAP_DRAWLAYER0 | TMAP_SET_GROUP(2));
			if (nSpriteEnable & 0x80) draw_sprites(0);
			if (nSpriteEnable & 4) GenericTilemapDraw(0, pTransDraw, TMAP_DRAWLAYER1 | TMAP_SET_GROUP(0));
			if (nSpriteEnable & 8) GenericTilemapDraw(0, pTransDraw, TMAP_DRAWLAYER0 | TMAP_SET_GROUP(0));
		break;

		case 2:
			GenericTilemapDraw(0, pTransDraw, TMAP_DRAWLAYER1 | TMAP_SET_GROUP(3));
			GenericTilemapDraw(0, pTransDraw, TMAP_DRAWLAYER0 | TMAP_SET_GROUP(3));
			GenericTilemapDraw(0, pTransDraw, TMAP_DRAWLAYER1 | TMAP_SET_GROUP(1));
			draw_sprites(1);
			GenericTilemapDraw(0, pTransDraw, TMAP_DRAWLAYER0 | TMAP_SET_GROUP(1));
			GenericTilemapDraw(0, pTransDraw, TMAP_DRAWLAYER1 | TMAP_SET_GROUP(2));
			GenericTilemapDraw(0, pTransDraw, TMAP_DRAWLAYER0 | TMAP_SET_GROUP(2));
			draw_sprites(0);
			GenericTilemapDraw(0, pTransDraw, TMAP_DRAWLAYER1 | TMAP_SET_GROUP(0));
			GenericTilemapDraw(0, pTransDraw, TMAP_DRAWLAYER0 | TMAP_SET_GROUP(0));
		break;

		case 3:
			GenericTilemapDraw(0, pTransDraw, TMAP_DRAWLAYER1 | TMAP_SET_GROUP(1));
			draw_sprites(1);
			GenericTilemapDraw(0, pTransDraw, TMAP_DRAWLAYER0 | TMAP_SET_GROUP(1));
			draw_sprites(0);
			GenericTilemapDraw(0, pTransDraw, TMAP_DRAWLAYER1 | TMAP_SET_GROUP(0));
			GenericTilemapDraw(0, pTransDraw, TMAP_DRAWLAYER0 | TMAP_SET_GROUP(0));
		break;
	}

	BurnTransferCopy(DrvPalette);

	BurnGunDrawTargets();

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}

		if (is_game == 0) { // nycaptor/colt
			BurnGunMakeInputs(0, DrvAnalog[0], DrvAnalog[1]);
		} else {
			BurnGunMakeInputs(0, DrvAnalog[1], -DrvAnalog[0]);
		}

		DrvInputs[0] ^= 0x30;
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[4] = { 4000000 / 60, 4000000 / 60, 4000000 / 60, 2000000 / 60 };
	INT32 nCyclesDone[4] = { nExtraCycles[0], nExtraCycles[1], nExtraCycles[2], 0 };

	m6805Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		CPU_RUN(0, Zet);
		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(1);
		CPU_RUN(1, Zet);
		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(2);
		CPU_RUN(2, Zet);
		if (i == (nInterleave / 2) - 1) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		if (i == (nInterleave / 1) - 1) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		if ((BurnDrvGetFlags() & BDF_BOOTLEG) == 0) {
			CPU_RUN(3, m6805);
		}
	}

	ZetOpen(2);

	if (pBurnSoundOut) {
        AY8910Render(pBurnSoundOut, nBurnSoundLen);
        MSM5232Update(pBurnSoundOut, nBurnSoundLen);
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
		BurnSoundDCFilter();
    }

	m6805Close();
	ZetClose();

	nExtraCycles[0] = nCyclesDone[0] - nCyclesTotal[0];
	nExtraCycles[1] = nCyclesDone[1] - nCyclesTotal[1];
	nExtraCycles[2] = nCyclesDone[2] - nCyclesTotal[2];

	Update_Spot();

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
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

		ZetScan(nAction);

		m68705_taito_scan(nAction);
		AY8910Scan(nAction, pnMin);
		MSM5232Scan(nAction, pnMin);
		DACScan(nAction, pnMin);
		BurnGunScan();

		BurnRandomScan(nAction);

		SCAN_VAR(rombank);
		SCAN_VAR(palettebank);
		SCAN_VAR(generic_control_reg);
		SCAN_VAR(gfx_control);
		SCAN_VAR(character_bank);
		SCAN_VAR(soundlatch);
		SCAN_VAR(nmi_pending);
		SCAN_VAR(nmi_enable);

		SCAN_VAR(nExtraCycles);
	}

	if (nAction & ACB_WRITE)
	{
		ZetOpen(0);
		set_rombank(rombank);
		set_palettebank(palettebank);
		ZetClose();
	}

	return 0;
}


// N.Y. Captor (rev 2)

static struct BurnRomInfo nycaptorRomDesc[] = {
	{ "a50_04",			0x4000, 0x33d971a3, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "a50_03",			0x4000, 0x8557fa44, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a50_02",			0x4000, 0x9697b898, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "a50_01",			0x4000, 0x0965f84a, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "a50_05-2",		0x4000, 0x7796655a, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code
	{ "a50_06",			0x4000, 0x450d6783, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "a50_15",			0x4000, 0xf8a604e5, 3 | BRF_PRG | BRF_ESS }, //  6 Z80 #2 Code
	{ "a50_16",			0x4000, 0xfc24e11d, 3 | BRF_PRG | BRF_ESS }, //  7

	{ "a50_07",			0x4000, 0xb3b330cf, 4 | BRF_GRA },           //  8 Graphics
	{ "a50_09",			0x4000, 0x22e2232e, 4 | BRF_GRA },           //  9
	{ "a50_11",			0x4000, 0x2c04ad4f, 4 | BRF_GRA },           // 10
	{ "a50_13",			0x4000, 0x9940e78d, 4 | BRF_GRA },           // 11
	{ "a50_08",			0x4000, 0x3c31be14, 4 | BRF_GRA },           // 12
	{ "a50_10",			0x4000, 0xce84dc5a, 4 | BRF_GRA },           // 13
	{ "a50_12",			0x4000, 0x3fb4cfa3, 4 | BRF_GRA },           // 14
	{ "a50_14",			0x4000, 0x24b2f1bf, 4 | BRF_GRA },           // 15

	{ "a50_17",			0x0800, 0x69fe08dc, 5 | BRF_PRG | BRF_ESS }, // 16 MCU Code (M68705)
};

STD_ROM_PICK(nycaptor)
STD_ROM_FN(nycaptor)

static INT32 NycaptorInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvNycaptor = {
	"nycaptor", NULL, NULL, NULL, "1985",
	"N.Y. Captor (rev 2)\0", NULL, "Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_SHOOT, 0,
	NULL, nycaptorRomInfo, nycaptorRomName, NULL, NULL, NULL, NULL, NycaptorInputInfo, NycaptorDIPInfo,
	NycaptorInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// Colt

static struct BurnRomInfo coltRomDesc[] = {
	{ "04.bin",			0x4000, 0xdc61fdb2, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "03.bin",			0x4000, 0x5835b8b1, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "02.bin",			0x4000, 0x89c99a28, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "01.bin",			0x4000, 0x9b0948f3, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "05.bin",			0x4000, 0x2b6e017a, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code
	{ "a50_06",			0x4000, 0x450d6783, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "a50_15",			0x4000, 0xf8a604e5, 3 | BRF_PRG | BRF_ESS }, //  6 Z80 #2 Code
	{ "a50_16",			0x4000, 0xfc24e11d, 3 | BRF_PRG | BRF_ESS }, //  7

	{ "a50_07",			0x4000, 0xb3b330cf, 4 | BRF_GRA },           //  8 Graphics
	{ "a50_09",			0x4000, 0x22e2232e, 4 | BRF_GRA },           //  9
	{ "a50_11",			0x4000, 0x2c04ad4f, 4 | BRF_GRA },           // 10
	{ "a50_13",			0x4000, 0x9940e78d, 4 | BRF_GRA },           // 11
	{ "08.bin",			0x4000, 0x9c0689f3, 4 | BRF_GRA },           // 12
	{ "a50_10",			0x4000, 0xce84dc5a, 4 | BRF_GRA },           // 13
	{ "a50_12",			0x4000, 0x3fb4cfa3, 4 | BRF_GRA },           // 14
	{ "a50_14",			0x4000, 0x24b2f1bf, 4 | BRF_GRA },           // 15
};

STD_ROM_PICK(colt)
STD_ROM_FN(colt)

static INT32 ColtInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvColt = {
	"colt", "nycaptor", NULL, NULL, "1986",
	"Colt\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_TAITO_MISC, GBF_SHOOT, 0,
	NULL, coltRomInfo, coltRomName, NULL, NULL, NULL, NULL, NycaptorInputInfo, ColtDIPInfo,
	ColtInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// Cycle Shooting

static struct BurnRomInfo cyclshtgRomDesc[] = {
	{ "a97_01.i17",		0x4000, 0x686fac1a, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "a97_02.i16",		0x4000, 0x48a812f9, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a97_03.u15",		0x4000, 0x67ad3067, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "a97_04.u14",		0x4000, 0x804e6445, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "a97_05.u22",		0x4000, 0xfdc36c4f, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code
	{ "a80_06.u23",		0x4000, 0x2769c5ab, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "a97_06.i24",		0x4000, 0xc0473a54, 2 | BRF_PRG | BRF_ESS }, //  6 Z80 #1 Port Data

	{ "a80_16.i26",		0x4000, 0xce171a48, 3 | BRF_PRG | BRF_ESS }, //  7 Z80 #2 Code
	{ "a80_17.i25",		0x4000, 0xa90b7bbc, 3 | BRF_PRG | BRF_ESS }, //  8

	{ "a80_11.u11",		0x4000, 0x29e1293b, 4 | BRF_GRA },           //  9 Graphics
	{ "a80_10.u10",		0x4000, 0x345f576c, 4 | BRF_GRA },           // 10
	{ "a80_09.u9",		0x4000, 0x3ef06dff, 4 | BRF_GRA },           // 11
	{ "a97_07.u8",		0x4000, 0x8f2baf57, 4 | BRF_GRA },           // 12
	{ "a80_15.u39",		0x4000, 0x2cefb47d, 4 | BRF_GRA },           // 13
	{ "a80_14.u34",		0x4000, 0x91642de8, 4 | BRF_GRA },           // 14
	{ "a80_13.u33",		0x4000, 0x96a67c6b, 4 | BRF_GRA },           // 15
	{ "a80_12.u23",		0x4000, 0x9ff04c85, 4 | BRF_GRA },           // 16

	{ "a80_18",	 		0x0800, 0x00000000, 5 | BRF_NODUMP | BRF_PRG | BRF_ESS },           // 17 MCU Code (M68705)
};

STD_ROM_PICK(cyclshtg)
STD_ROM_FN(cyclshtg)

static INT32 CyclshtgInit()
{
	return DrvInit(3);
}

struct BurnDriver BurnDrvCyclshtg = {
	"cyclshtg", NULL, NULL, NULL, "1986",
	"Cycle Shooting\0", "Not Emulated, GFX/layer priority issues", "Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_TAITO_MISC, GBF_SHOOT, 0,
	NULL, cyclshtgRomInfo, cyclshtgRomName, NULL, NULL, NULL, NULL, NycaptorInputInfo, CyclshtgDIPInfo,
	CyclshtgInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Bronx

static struct BurnRomInfo bronxRomDesc[] = {
	{ "1.bin",			0x4000, 0x399b5063, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "2.bin",			0x4000, 0x3b5f9756, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.bin",			0x8000, 0xd2ffd3ce, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.bin",			0x8000, 0x20cf148d, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "5.bin",			0x4000, 0x19a1c665, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code
	{ "6.bin",			0x4000, 0x2769c5ab, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "7.bin",			0x8000, 0x463f9f62, 2 | BRF_PRG | BRF_ESS }, //  6 Z80 #1 Port Data

	{ "a80_16.i26",		0x4000, 0xce171a48, 3 | BRF_PRG | BRF_ESS }, //  7 Z80 #2 Code
	{ "a80_17.i25",		0x4000, 0xa90b7bbc, 3 | BRF_PRG | BRF_ESS }, //  8

	{ "a80_11.u11",		0x4000, 0x29e1293b, 4 | BRF_GRA },           //  9 Graphics
	{ "a80_10.u10",		0x4000, 0x345f576c, 4 | BRF_GRA },           // 10
	{ "a80_09.u9",		0x4000, 0x3ef06dff, 4 | BRF_GRA },           // 11
	{ "8.bin",			0x4000, 0x2b778d24, 4 | BRF_GRA },           // 12
	{ "a80_15.u39",		0x4000, 0x2cefb47d, 4 | BRF_GRA },           // 13
	{ "a80_14.u34",		0x4000, 0x91642de8, 4 | BRF_GRA },           // 14
	{ "a80_13.u33",		0x4000, 0x96a67c6b, 4 | BRF_GRA },           // 15
	{ "a80_12.u23",		0x4000, 0x9ff04c85, 4 | BRF_GRA },           // 16
};

STD_ROM_PICK(bronx)
STD_ROM_FN(bronx)

static INT32 BronxInit()
{
	return DrvInit(2);
}

struct BurnDriver BurnDrvBronx = {
	"bronx", "cyclshtg", NULL, NULL, "1986",
	"Bronx\0", "GFX/layer priority issues", "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_SHOOT, 0,
	NULL, bronxRomInfo, bronxRomName, NULL, NULL, NULL, NULL, NycaptorInputInfo, BronxDIPInfo,
	BronxInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};
