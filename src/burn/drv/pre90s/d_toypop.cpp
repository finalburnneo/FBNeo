// FB Alpha "Universal System 16" hardware driver module
// Based on MAME driver by Angelo Salese

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "m68000_intf.h"
#include "namco_snd.h"
#include "namcoio.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6809ROM0;
static UINT8 *DrvM6809ROM1;
static UINT8 *Drv68KROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvSndPROM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvShareRAM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvBgVRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT16 slave_in_reset;
static UINT16 sound_in_reset;
static UINT16 palette_bank;
static UINT16 master_irq_enable;
static UINT16 slave_irq_enable;
static UINT16 flipscreen;

static UINT16 address_xor = 0; // toypop = 0x800

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvJoy5[8];
static UINT8 DrvJoy6[8];
static UINT8 DrvDips[4];
static UINT8 DrvInputs[7];
static UINT8 DrvReset;

static struct BurnInputInfo LiblrablInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy6 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy5 + 2,	"p1 start"	},
	{"P1 Left Up",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 up"		},
	{"P1 Left Down",	BIT_DIGITAL,	DrvJoy3 + 2,	"p1 down"	},
	{"P1 Left Left",	BIT_DIGITAL,	DrvJoy3 + 3,	"p1 left"	},
	{"P1 Left Right",	BIT_DIGITAL,	DrvJoy3 + 1,	"p1 right"	},
	{"P1 Right Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p3 up"		},
	{"P1 Right Down",	BIT_DIGITAL,	DrvJoy1 + 2,	"p3 down"	},
	{"P1 Right Left",	BIT_DIGITAL,	DrvJoy1 + 3,	"p3 left"	},
	{"P1 Right Right",	BIT_DIGITAL,	DrvJoy1 + 1,	"p3 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy5 + 0,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy6 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy5 + 3,	"p2 start"	},
	{"P2 Left Up",		BIT_DIGITAL,	DrvJoy4 + 0,	"p2 up"		},
	{"P2 Left Down",	BIT_DIGITAL,	DrvJoy4 + 2,	"p2 down"	},
	{"P2 Left Left",	BIT_DIGITAL,	DrvJoy4 + 3,	"p2 left"	},
	{"P2 Left Right",	BIT_DIGITAL,	DrvJoy4 + 1,	"p2 right"	},
	{"P2 Right Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p4 up"		},
	{"P2 Right Down",	BIT_DIGITAL,	DrvJoy2 + 2,	"p4 down"	},
	{"P2 Right Left",	BIT_DIGITAL,	DrvJoy2 + 3,	"p4 left"	},
	{"P2 Right Right",	BIT_DIGITAL,	DrvJoy2 + 1,	"p4 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy5 + 1,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Liblrabl)

static struct BurnInputInfo ToypopInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy6 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy5 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy5 + 0,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy6 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy5 + 3,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy5 + 1,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy6 + 3,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Toypop)

static struct BurnDIPInfo LiblrablDIPList[]=
{
	{0x17, 0xff, 0xff, 0xff, NULL				},
	{0x18, 0xff, 0xff, 0xff, NULL				},
	{0x19, 0xff, 0xff, 0x0f, NULL				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x17, 0x01, 0x03, 0x02, "1"				},
	{0x17, 0x01, 0x03, 0x00, "2"				},
	{0x17, 0x01, 0x03, 0x03, "3"				},
	{0x17, 0x01, 0x03, 0x01, "5"				},

	{0   , 0xfe, 0   ,   13, "Bonus Life"			},
	{0x17, 0x01, 0x1c, 0x1c, "40k 120k 200k 400k 600k 1m"	},
	{0x17, 0x01, 0x1c, 0x0c, "40k 140k 250k 400k 700k 1m"	},
	{0x17, 0x01, 0x1c, 0x14, "50k 150k 300k 500k 700k 1m"	},
	{0x17, 0x01, 0x1c, 0x04, "40k 120k and every 120k"	},
	{0x17, 0x01, 0x1c, 0x18, "40k 150k and every 150k"	},
	{0x17, 0x01, 0x1c, 0x08, "50k 150k 300k"		},
	{0x17, 0x01, 0x1c, 0x10, "40k 120k 200k"		},
	{0x17, 0x01, 0x1c, 0x14, "40k 120k"			},
	{0x17, 0x01, 0x1c, 0x04, "50k 150k"			},
	{0x17, 0x01, 0x1c, 0x18, "50k 150k and every 150k"	},
	{0x17, 0x01, 0x1c, 0x08, "60k 200k and every 200k"	},
	{0x17, 0x01, 0x1c, 0x10, "50k"				},
	{0x17, 0x01, 0x1c, 0x00, "None"				},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x17, 0x01, 0xe0, 0x80, "3 Coins 1 Credits"		},
	{0x17, 0x01, 0xe0, 0xc0, "2 Coins 1 Credits"		},
	{0x17, 0x01, 0xe0, 0x00, "3 Coins 2 Credits"		},
	{0x17, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"		},
	{0x17, 0x01, 0xe0, 0x40, "2 Coins 3 Credits"		},
	{0x17, 0x01, 0xe0, 0x60, "1 Coin  2 Credits"		},
	{0x17, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"		},
	{0x17, 0x01, 0xe0, 0x20, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    2, "Freeze"			},
	{0x18, 0x01, 0x01, 0x01, "Off"				},
	{0x18, 0x01, 0x01, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Rack Test"			},
	{0x18, 0x01, 0x02, 0x02, "Off"				},
	{0x18, 0x01, 0x02, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x18, 0x01, 0x04, 0x00, "Off"				},
	{0x18, 0x01, 0x04, 0x04, "On"				},

	{0   , 0xfe, 0   ,    4, "Coin B"			},
	{0x18, 0x01, 0x18, 0x00, "2 Coins 1 Credits"		},
	{0x18, 0x01, 0x18, 0x18, "1 Coin  1 Credits"		},
	{0x18, 0x01, 0x18, 0x08, "1 Coin  5 Credits"		},
	{0x18, 0x01, 0x18, 0x10, "1 Coin  7 Credits"		},

	{0   , 0xfe, 0   ,    2, "Practice"			},
	{0x18, 0x01, 0x20, 0x00, "Off"				},
	{0x18, 0x01, 0x20, 0x20, "On"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x18, 0x01, 0xc0, 0xc0, "A"				},
	{0x18, 0x01, 0xc0, 0x40, "B"				},
	{0x18, 0x01, 0xc0, 0x80, "C"				},
	{0x18, 0x01, 0xc0, 0x00, "D"				},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x19, 0x01, 0x04, 0x04, "Upright"			},
	{0x19, 0x01, 0x04, 0x00, "Cocktail"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x19, 0x01, 0x08, 0x00, "On"			},
	{0x19, 0x01, 0x08, 0x08, "Off"			},
};

STDDIPINFO(Liblrabl)

static struct BurnDIPInfo ToypopDIPList[]=
{
	{0x10, 0xff, 0xff, 0xbf, NULL				},
	{0x11, 0xff, 0xff, 0xff, NULL				},
	{0x12, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x10, 0x01, 0x03, 0x02, "1"				},
	{0x10, 0x01, 0x03, 0x01, "2"				},
	{0x10, 0x01, 0x03, 0x03, "3"				},
	{0x10, 0x01, 0x03, 0x00, "5"				},

	{0   , 0xfe, 0   ,    4, "Coin B"			},
	{0x10, 0x01, 0x0c, 0x00, "3 Coins 1 Credits"		},
	{0x10, 0x01, 0x0c, 0x04, "2 Coins 1 Credits"		},
	{0x10, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"		},
	{0x10, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin A"			},
	{0x10, 0x01, 0x30, 0x00, "3 Coins 1 Credits"		},
	{0x10, 0x01, 0x30, 0x10, "2 Coins 1 Credits"		},
	{0x10, 0x01, 0x30, 0x30, "1 Coin  1 Credits"		},
	{0x10, 0x01, 0x30, 0x20, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x10, 0x01, 0x40, 0x00, "Off"				},
	{0x10, 0x01, 0x40, 0x40, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x10, 0x01, 0x80, 0x00, "On"				},
	{0x10, 0x01, 0x80, 0x80, "Off"				},

	{0   , 0xfe, 0   ,    2, "Freeze"			},
	{0x11, 0x01, 0x01, 0x01, "Off"				},
	{0x11, 0x01, 0x01, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Level Select"			},
	{0x11, 0x01, 0x02, 0x02, "Off"				},
	{0x11, 0x01, 0x02, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "2 Players Game"		},
	{0x11, 0x01, 0x04, 0x00, "1 Credit"			},
	{0x11, 0x01, 0x04, 0x04, "2 Credits"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x11, 0x01, 0x08, 0x00, "Off"				},
	{0x11, 0x01, 0x08, 0x08, "On"				},

	{0   , 0xfe, 0   ,    2, "Entering"			},
	{0x11, 0x01, 0x10, 0x00, "Off"				},
	{0x11, 0x01, 0x10, 0x10, "On"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x11, 0x01, 0x60, 0x40, "Easy"				},
	{0x11, 0x01, 0x60, 0x60, "Normal"			},
	{0x11, 0x01, 0x60, 0x20, "Hard"				},
	{0x11, 0x01, 0x60, 0x00, "Very Hard"			},

	{0   , 0xfe, 0   ,    2, "Bonus Life"			},
	{0x11, 0x01, 0x80, 0x80, "Every 15000 points"		},
	{0x11, 0x01, 0x80, 0x00, "Every 20000 points"		},
};

STDDIPINFO(Toypop)

static void toypop_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf000) == 0x8000) {
		slave_in_reset = address & 0x800;
		if (!slave_in_reset) {
			SekReset();
		}
		return;
	}

	if ((address & 0xf000) == 0x9000) {
		sound_in_reset = address & 0x800;
		if (sound_in_reset) {
			M6809Close();
			M6809Open(1);
			M6809Reset();
			M6809Close();
			M6809Open(0);
		}
		return;
	}

	switch (address)
	{
		case 0xa000:
		case 0xa001:
			palette_bank = address & 1;
		return;
	}

	if ((address & 0xf000) == 0x6000) address ^= address_xor;

	if ((address & 0xfc00) == 0x6000) {
		namco_15xx_sharedram_write(address,data);
		return;
	}

	if ((address & 0xfff0) == 0x6800) {
		namcoio_write(0, address & 0xf, data);
		return;
	}

	if ((address & 0xfff0) == 0x6810) {
		namcoio_write(1, address & 0xf, data);
		return;
	}

	if ((address & 0xfff0) == 0x6820) {
		namcoio_write(2, address & 0xf, data);
		return;
	}

	if ((address & 0xf000) == 0x7000) {
		master_irq_enable = (address & 0x0800) ? 0 : 1;
		return;
	}
}

static UINT8 toypop_main_read(UINT16 address)
{
	if ((address & 0xf000) == 0x6000) address ^= address_xor;

	if ((address & 0xfc00) == 0x6000) {
		return namco_15xx_sharedram_read(address);
	}

	if ((address & 0xfff0) == 0x6800) {
		return namcoio_read(0, address & 0xf);
	}

	if ((address & 0xfff0) == 0x6810) {
		return namcoio_read(1, address & 0xf);
	}

	if ((address & 0xfff0) == 0x6820) {
		return namcoio_read(2, address & 0xf);
	}

	if ((address & 0xf000) == 0x7000 && (address_xor == 0x800)) { // only toypop!
		master_irq_enable = 1;
		return 0;
	}

	return 0;
}

static void __fastcall toypop_slave_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfff000) == 0x100000) {
		DrvShareRAM[(address / 2) & 0x7ff] = data & 0xff;
		return;
	}

	if ((address & 0xff8000) == 0x180000) {
		address = (address & 0x7ffe) * 2;
		*((UINT16*)(DrvBgVRAM + (address+0))) = ((data & 0x00f0) << 4) | ((data & 0x000f));
		*((UINT16*)(DrvBgVRAM + (address+2))) = ((data & 0xf000) >> 4) | ((data & 0xf00) >> 8);
		return;
	}

	if ((address & 0xf00000) == 0x300000) {
		slave_irq_enable = (address & 0x40000) ? 0 : 1;
		return;
	}
}

static void __fastcall toypop_slave_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xfff001) == 0x100001) {
		DrvShareRAM[(address / 2) & 0x7ff] = data;
		return;
	}

	if ((address & 0xf00000) == 0x300000) {
		slave_irq_enable = (address & 0x40000) ? 0 : 1;
		return;
	}
}

static UINT16 __fastcall toypop_slave_read_word(UINT32 address)
{
	if ((address & 0xfff000) == 0x100000) {
		return DrvShareRAM[(address / 2) & 0x7ff];
	}

	if ((address & 0xff8000) == 0x180000) {
		UINT16 *p = (UINT16*)DrvBgVRAM;
		UINT16 ret;
		ret  = (p[(address & 0x7ffe)+0] & 0xf00) >> 4;
		ret |= (p[(address & 0x7ffe)+0] & 0x00f) >> 0;
		ret |= (p[(address & 0x7ffe)+1] & 0xf00) << 4;
		ret |= (p[(address & 0x7ffe)+1] & 0x00f) << 8;

		return ret;
	}

	return 0;
}

static UINT8 __fastcall toypop_slave_read_byte(UINT32 address)
{
	if ((address & 0xfff001) == 0x100001) {
		return DrvShareRAM[(address / 2) & 0x7ff];
	}

	return 0;
}

static void toypop_sound_write(UINT16 address, UINT8 data)
{
	if (address <= 0x03ff) {
		namco_15xx_sharedram_write(address,data);
		return;
	}
}

static UINT8 toypop_sound_read(UINT16 address)
{
	if (address <= 0x03ff) {
		return namco_15xx_sharedram_read(address);
	}

	return 0;
}

static tilemap_scan( foreground )
{
	INT32 offs;

	row += 2;
	col -= 2;
	if (col & 0x20)
		offs = row + ((col & 0x1f) << 5);
	else
		offs = col + (row << 5);

	return offs;
}

static tilemap_callback( foreground )
{
	UINT8 attr = DrvFgRAM[offs + 0x400] & 0x3f;
	UINT8 code = DrvFgRAM[offs] & 0x1ff;

	TILE_SET_INFO(0, code, attr + palette_bank * 0x40, 0);
}

static UINT8 nio0_i0(UINT8) { return DrvInputs[5] & 0xf; } // COINS
static UINT8 nio0_i1(UINT8) { return DrvInputs[0] & 0xf; } // P1_RIGHT
static UINT8 nio0_i2(UINT8) { return DrvInputs[1] & 0xf; } // P2_RIGHT
static UINT8 nio0_i3(UINT8) { return DrvInputs[4] & 0xf; } // BUTTONS

static UINT8 nio1_i0(UINT8) { return DrvDips[0] >> 4; }
static UINT8 nio1_i1(UINT8) { return DrvDips[1] & 0xf; }
static UINT8 nio1_i2(UINT8) { return DrvDips[1] >> 4; }
static UINT8 nio1_i3(UINT8) { return DrvDips[0] & 0xf; }
static void nio1_o0(UINT8, UINT8 data) { flipscreen = data; }

static UINT8 nio2_i1(UINT8) { return DrvInputs[2] & 0xf; } // P1_LEFT
static UINT8 nio2_i2(UINT8) { return DrvInputs[3] & 0xf; } // P2_LEFT
static UINT8 nio2_i3(UINT8) { return DrvDips[2] & 0xf; } // SERVICE

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	M6809Open(0);
	M6809Reset();
	M6809Close();

	SekOpen(0);
	SekReset();
	SekClose();

	M6809Open(1);
	M6809Reset();
	NamcoSoundReset();
	M6809Close();

	namcoio_reset(0);
	namcoio_reset(1);
	namcoio_reset(2);

	slave_in_reset = 1;	// start with slave turned off
	sound_in_reset = 1;	// start with sound turned off
	palette_bank = 0;
	master_irq_enable = 0;
	slave_irq_enable = 0;
	flipscreen = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6809ROM0	= Next; Next += 0x008000;
	DrvM6809ROM1	= Next; Next += 0x002000;
	Drv68KROM		= Next; Next += 0x008000;

	DrvGfxROM0		= Next; Next += 0x008000;
	DrvGfxROM1		= Next; Next += 0x010000;

	DrvColPROM		= Next; Next += 0x000600;

	NamcoSoundProm	= Next;
	DrvSndPROM		= Next; Next += 0x000100;

	DrvPalette		= (UINT32*)Next; Next += 0x0320 * sizeof(UINT32);

	AllRam			= Next;

	DrvFgRAM		= Next; Next += 0x002000;
	DrvShareRAM		= Next; Next += 0x000800;
	Drv68KRAM		= Next; Next += 0x040000;
	DrvBgVRAM		= Next; Next += 0x050000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[2]  = { 0, 4 };
	INT32 XOffs0[8] = { STEP4(64,1), STEP4(0,1) };
	INT32 XOffs1[16]= { STEP4(0,1), STEP4(64,1), STEP4(128,1), STEP4(192,1) };
	INT32 YOffs[16] = { STEP8(0,8), STEP8(256,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x4000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x2000);

	GfxDecode(0x0200, 2,  8,  8, Plane, XOffs0, YOffs, 0x080, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x4000);

	GfxDecode(0x0100, 2, 16, 16, Plane, XOffs1, YOffs, 0x200, tmp, DrvGfxROM1);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit(INT32 addr_xor)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvM6809ROM0 + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM0 + 0x4000,  1, 1)) return 1;

		if (BurnLoadRom(DrvM6809ROM1 + 0x0000,  2, 1)) return 1;

		if (BurnLoadRom(Drv68KROM    + 0x0001,  3, 2)) return 1;
		if (BurnLoadRom(Drv68KROM    + 0x0000,  4, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM0   + 0x0000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1   + 0x0000,  6, 1)) return 1;

		if (BurnLoadRom(DrvColPROM   + 0x0000,  7, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x0100,  8, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x0200,  9, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x0300, 10, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x0400, 11, 1)) return 1;

		if (BurnLoadRom(DrvSndPROM   + 0x0000, 12, 1)) return 1;

		DrvGfxDecode();
	}

	address_xor = addr_xor;

	M6809Init(0);
	M6809Open(0);
	M6809MapMemory(DrvFgRAM,		0x0000, 0x1fff, MAP_RAM);
	M6809MapMemory(DrvShareRAM,		0x2800, 0x2fff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM0,	0x8000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(toypop_main_write);
	M6809SetReadHandler(toypop_main_read);
	M6809Close();

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x007fff, MAP_ROM);
	SekMapMemory(Drv68KRAM,		0x080000, 0x0bffff, MAP_RAM);
	SekMapMemory(DrvBgVRAM,		0x190000, 0x1dffff, MAP_RAM);
	SekSetWriteWordHandler(0,	toypop_slave_write_word);
	SekSetWriteByteHandler(0,	toypop_slave_write_byte);
	SekSetReadWordHandler(0,	toypop_slave_read_word);
	SekSetReadByteHandler(0,	toypop_slave_read_byte);
	SekClose();

	M6809Init(1);
	M6809Open(1);
	M6809MapMemory(DrvM6809ROM1,	0xe000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(toypop_sound_write);
	M6809SetReadHandler(toypop_sound_read);
	M6809Close();

	NamcoSoundInit(24000, 8, 0);
	NamcoSoundSetAllRoutes(0.50 * 10.0 / 16.0, BURN_SND_ROUTE_BOTH);

	namcoio_init(0, NAMCO58xx, nio0_i0, nio0_i1, nio0_i2, nio0_i3, NULL,    NULL);
	namcoio_init(1, NAMCO56xx, nio1_i0, nio1_i1, nio1_i2, nio1_i3, nio1_o0, NULL);
	namcoio_init(2, NAMCO56xx, NULL,    nio2_i1, nio2_i2, nio2_i3, NULL,    NULL);

	GenericTilesInit();
	GenericTilemapInit(0, foreground_map_scan, foreground_map_callback, 8, 8, 36, 28);
	GenericTilemapSetGfx(0, DrvGfxROM0, 2, 8, 8, 0x8000, 0, 0x7f);
	GenericTilemapSetTransparent(0, 0);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6809Exit();
	SekExit();

	NamcoSoundExit();
	NamcoSoundProm = NULL;

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	UINT32 pal[0x100];

	for (INT32 i = 0;i < 256;i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 0x01;
		INT32 bit3 = (DrvColPROM[i] >> 3) & 0x01;
		INT32 r = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[i+0x100] >> 0) & 0x01;
		bit1 = (DrvColPROM[i+0x100] >> 1) & 0x01;
		bit2 = (DrvColPROM[i+0x100] >> 2) & 0x01;
		bit3 = (DrvColPROM[i+0x100] >> 3) & 0x01;
		INT32 g = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[i+0x200] >> 0) & 0x01;
		bit1 = (DrvColPROM[i+0x200] >> 1) & 0x01;
		bit2 = (DrvColPROM[i+0x200] >> 2) & 0x01;
		bit3 = (DrvColPROM[i+0x200] >> 3) & 0x01;
		INT32 b = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		pal[i] = BurnHighCol(r,g,b,0);
	}

	for (INT32 i = 0;i < 256;i++)
	{
		DrvPalette[i + 0x000] = pal[(DrvColPROM[i+0x300] & 0xf) + 0x70];
		DrvPalette[i + 0x100] = pal[(DrvColPROM[i+0x300] & 0xf) + 0xf0];
		DrvPalette[i + 0x200] = pal[(DrvColPROM[i+0x500])];
	}

	for (INT32 i = 0;i < 16;i++)
	{
		DrvPalette[i + 0x300] = pal[i + 0x60];
		DrvPalette[i + 0x310] = pal[i + 0xe0];
	}
}

static void draw_bg_layer()
{
	const UINT16 pal_base = 0x300 + (palette_bank * 16);
	const UINT32 src_base = 0x200/2;
	const UINT32 src_pitch = 288 / 2;
	UINT16 *ram = (UINT16*)DrvBgVRAM;

	for (INT32 y = 0; y < nScreenHeight; ++y)
	{
		UINT16 *src = &ram[y * src_pitch + src_base];
		UINT16 *dst = pTransDraw + (((flipscreen) ? (nScreenHeight - 1 - y) : y) * nScreenWidth);

		for (INT32 x = 0; x < nScreenWidth; x += 2)
		{
			UINT32 srcpix = *src++;
			*dst++ = ((srcpix >> 8) & 0xf) + pal_base;
			*dst++ = (srcpix & 0xf) + pal_base;
		}
	}
}

static void draw_sprites()
{
	UINT8 *base_spriteram = (DrvFgRAM + 0x800);
	const UINT16 bank1 = 0x0800;
	const UINT16 bank2 = 0x1000;

	for (INT32 count = 0x780; count < 0x800; count+=2)
	{
		bool enabled = (base_spriteram[count+bank2+1] & 2) == 0;

		if (enabled == false)
			continue;

		UINT8 tile = base_spriteram[count];
		UINT8 color = base_spriteram[count+1];
		INT32 x = base_spriteram[count+bank1+1] + (base_spriteram[count+bank2+1] << 8);
		x -= 71;

		INT32 y = 224 - (base_spriteram[count+bank1+0] + 7);
		bool fx = (base_spriteram[count+bank2] & 1) == 1;
		bool fy = (base_spriteram[count+bank2] & 2) == 2;
		UINT8 width = ((base_spriteram[count+bank2] & 4) >> 2) + 1;
		UINT8 height = ((base_spriteram[count+bank2] & 8) >> 3) + 1;

		if (height == 2) y -=16;

		for (INT32 yi = 0; yi < height; yi++)
		{
			for (INT32 xi = 0; xi < width; xi++)
			{
				UINT16 code = tile + (xi ^ ((width - 1) & fx)) + yi * 2;

				RenderTileTranstabOffset(pTransDraw, DrvGfxROM1, code, (color << 2), 0xff, x + xi*16, y + yi *16, fx, fy, 16, 16, DrvColPROM + 0x500, 0x200);
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	BurnTransferClear();

	GenericTilemapSetFlip(0, flipscreen);

	if (nBurnLayer & 1) draw_bg_layer();
	if (nBurnLayer & 2) GenericTilemapDraw(0, pTransDraw, 0);
	if (nSpriteEnable & 1) draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void master_scanline_callback(INT32 scanline)
{
	if (scanline == 224 && master_irq_enable)
		M6809SetIRQLine(0, CPU_IRQSTATUS_HOLD);

	if (scanline == 0)
	{
		if (!namcoio_read_reset_line(0))
			namcoio_run(0);

		if (!namcoio_read_reset_line(1))
			namcoio_run(1);

		if (!namcoio_read_reset_line(2))
			namcoio_run(2);
	}
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	M6809NewFrame();
	SekNewFrame();

	{
		memset (DrvInputs, 0xff, 6);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
			DrvInputs[4] ^= (DrvJoy5[i] & 1) << i;
			DrvInputs[5] ^= (DrvJoy6[i] & 1) << i;
		}
	}

	INT32 nInterleave = 264;
	INT32 nCyclesTotal[3] = { (6144000 / 4) / 60, 6144000 / 60, (6144000 / 4) / 60 };
	INT32 nCyclesDone[3] = { 0, 0, 0 };

	SekOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		M6809Open(0);
		master_scanline_callback(i);
		CPU_RUN(0, M6809);
		M6809Close();

		if (slave_in_reset) {
			CPU_IDLE(1, Sek);
		} else {
			CPU_RUN(1, Sek);
			if (i == 223 && slave_irq_enable) SekSetIRQLine(6, CPU_IRQSTATUS_AUTO);
		}

		if (sound_in_reset) {
			CPU_IDLE(2, M6809);
		} else {
			M6809Open(1);
			CPU_RUN(2, M6809);
			if (i == 223) M6809SetIRQLine(0, CPU_IRQSTATUS_HOLD);
			M6809Close();
		}
	}

	SekClose();

	if (pBurnSoundOut) {
		NamcoSoundUpdate(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		DrvDraw();
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

		M6809Scan(nAction);
		SekScan(nAction);
		NamcoSoundScan(nAction, pnMin);

		namcoio_scan(0);
		namcoio_scan(1);
		namcoio_scan(2);

		SCAN_VAR(slave_in_reset);
		SCAN_VAR(sound_in_reset);
		SCAN_VAR(palette_bank);
		SCAN_VAR(master_irq_enable);
		SCAN_VAR(slave_irq_enable);
		SCAN_VAR(flipscreen);
	}

	return 0;
}


// Libble Rabble

static struct BurnRomInfo liblrablRomDesc[] = {
	{ "5b.rom",		0x4000, 0xda7a93c2, 1 | BRF_GRA },           //  0 M6809 Code (master)
	{ "5c.rom",		0x4000, 0x6cae25dc, 1 | BRF_GRA },           //  1

	{ "2c.rom",		0x2000, 0x7c09e50a, 2 | BRF_GRA },           //  2 M6809 Code (sound)

	{ "8c.rom",		0x4000, 0xa00cd959, 3 | BRF_GRA },           //  3 M68000 Code (slave)
	{ "10c.rom",	0x4000, 0x09ce209b, 3 | BRF_GRA },           //  4

	{ "5p.rom",		0x2000, 0x3b4937f0, 4 | BRF_GRA },           //  5 Foreground Tiles

	{ "9t.rom",		0x4000, 0xa88e24ca, 5 | BRF_GRA },           //  6 Sprites

	{ "lr1-3.1r",	0x0100, 0xf3ec0d07, 6 | BRF_GRA },           //  7 Color Data
	{ "lr1-2.1s",	0x0100, 0x2ae4f702, 6 | BRF_GRA },           //  8
	{ "lr1-1.1t",	0x0100, 0x7601f208, 6 | BRF_GRA },           //  9
	{ "lr1-5.5l",	0x0100, 0x940f5397, 6 | BRF_GRA },           // 10
	{ "lr1-6.2p",	0x0200, 0xa6b7f850, 6 | BRF_GRA },           // 11

	{ "lr1-4.3d",	0x0100, 0x16a9166a, 7 | BRF_GRA },           // 12 Sound data
};

STD_ROM_PICK(liblrabl)
STD_ROM_FN(liblrabl)

static INT32 LiblrablInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvLiblrabl = {
	"liblrabl", NULL, NULL, NULL, "1983",
	"Libble Rabble\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, liblrablRomInfo, liblrablRomName, NULL, NULL, NULL, NULL, LiblrablInputInfo, LiblrablDIPInfo,
	LiblrablInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x320,
	288, 224, 4, 3
};


// Toypop

static struct BurnRomInfo toypopRomDesc[] = {
	{ "tp1-2.5b",	0x4000, 0x87469620, 1 | BRF_GRA },           //  0 M6809 Code (master)
	{ "tp1-1.5c",	0x4000, 0xdee2fd6e, 1 | BRF_GRA },           //  1

	{ "tp1-3.2c",	0x2000, 0x5f3bf6e2, 2 | BRF_GRA },           //  2 M6809 Code (sound)

	{ "tp1-4.8c",	0x4000, 0x76997db3, 3 | BRF_GRA },           //  3 M68000 Code (slave)
	{ "tp1-5.10c",	0x4000, 0x37de8786, 3 | BRF_GRA },           //  4

	{ "tp1-7.5p",	0x2000, 0x95076f9e, 4 | BRF_GRA },           //  5 Foreground Tiles

	{ "tp1-6.9t",	0x4000, 0x481ffeaf, 5 | BRF_GRA },           //  6 Sprites

	{ "tp1-3.1r",	0x0100, 0xcfce2fa5, 6 | BRF_GRA },           //  7 Color Data
	{ "tp1-2.1s",	0x0100, 0xaeaf039d, 6 | BRF_GRA },           //  8
	{ "tp1-1.1t",	0x0100, 0x08e7cde3, 6 | BRF_GRA },           //  9
	{ "tp1-4.5l",	0x0100, 0x74138973, 6 | BRF_GRA },           // 10
	{ "tp1-5.2p",	0x0200, 0x4d77fa5a, 6 | BRF_GRA },           // 11

	{ "tp1-6.3d",	0x0100, 0x16a9166a, 7 | BRF_GRA },           // 12 Sound data
};

STD_ROM_PICK(toypop)
STD_ROM_FN(toypop)

static INT32 ToypopInit()
{
	return DrvInit(0x800);
}

struct BurnDriver BurnDrvToypop = {
	"toypop", NULL, NULL, NULL, "1986",
	"Toypop\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_PUZZLE, 0,
	NULL, toypopRomInfo, toypopRomName, NULL, NULL, NULL, NULL, ToypopInputInfo, ToypopDIPInfo,
	ToypopInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x320,
	288, 224, 4, 3
};
