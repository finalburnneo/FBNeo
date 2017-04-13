// FB Alpha Lasso driver module
// Based on MAME driver by Phil Stroffolino, Nicola Salmoria, and Luca Elia

#include "tiles_generic.h"
#include "m6502_intf.h"
#include "z80_intf.h"
#include "sn76496.h"
#include "dac.h"
#include "driver.h"
#include "bitswap.h"
extern "C" {
#include "ay8910.h"
}

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6502ROM0;
static UINT8 *DrvM6502ROM1;
static UINT8 *DrvM6502ROM2;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvMapROM;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvM6502RAM0;
static UINT8 *DrvM6502RAM1;
static UINT8 *DrvVidRAM;
static UINT8 *DrvColRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvShareRAM;
static UINT8 *DrvBitmapRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT16 *pAY8910Buffer[6];

static UINT8 back_color;
static UINT8 soundlatch;
static UINT8 chip_data;
static UINT8 gfx_bank;
static UINT8 flipscreenx;
static UINT8 flipscreeny;
static UINT8 last_colors[3];
static UINT8 track_scroll[4];
static UINT8 track_enable;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

INT32 previous_coin = 0;

static INT32 game_select = 0; // 0 = lasso, 1 = chameleo, 2 - wwjgtin, 3 - pinbo

static struct BurnInputInfo LassoInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 7,	"p1 start"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 start"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Lasso)

static struct BurnInputInfo WwjgtinInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 6,	"p1 start"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 7,	"p2 start"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Wwjgtin)

static struct BurnInputInfo PinboInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 7,	"p1 start"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 start"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Pinbo)

static struct BurnInputInfo ChameleoInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 7,	"p1 start"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 start"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Chameleo)

static struct BurnDIPInfo LassoDIPList[]=
{
	{0x11, 0xff, 0xff, 0x81, NULL			},
	{0x12, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x11, 0x01, 0x01, 0x01, "Upright"		},
	{0x11, 0x01, 0x01, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    5, "Coin A"		},
	{0x11, 0x01, 0x0e, 0x02, "2 Coin  1 Credit"	},
	{0x11, 0x01, 0x0e, 0x00, "1 Coin  1 Credit"	},
	{0x11, 0x01, 0x0e, 0x08, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0x0e, 0x04, "1 Coin  3 Credits"	},
	{0x11, 0x01, 0x0e, 0x0c, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    3, "Lives"		},
	{0x11, 0x01, 0x30, 0x00, "3"			},
	{0x11, 0x01, 0x30, 0x10, "4"			},
	{0x11, 0x01, 0x30, 0x20, "5"			},

	{0   , 0xfe, 0   ,    2, "Coin B"		},
	{0x11, 0x01, 0x40, 0x40, "2 Coins 1 Credit"	},
	{0x11, 0x01, 0x40, 0x00, "1 Coin  1 Credit"	},

	{0   , 0xfe, 0   ,    2, "Warm-Up Instructions"	},
	{0x11, 0x01, 0x80, 0x00, "No"			},
	{0x11, 0x01, 0x80, 0x80, "Yes"			},

	{0   , 0xfe, 0   ,    2, "Warm-Up"		},
	{0x12, 0x01, 0x01, 0x01, "No"			},
	{0x12, 0x01, 0x01, 0x00, "Yes"			},

	{0   , 0xfe, 0   ,    2, "Warm-Up Language"	},
	{0x12, 0x01, 0x02, 0x00, "English"		},
	{0x12, 0x01, 0x02, 0x02, "German"		},

	{0   , 0xfe, 0   ,    2, "Invulnerability"	},
	{0x12, 0x01, 0x08, 0x00, "Off"			},
	{0x12, 0x01, 0x08, 0x08, "on"			},
};

STDDIPINFO(Lasso)

static struct BurnDIPInfo WwjgtinDIPList[]=
{
	{0x11, 0xff, 0xff, 0x00, NULL			},
	{0x12, 0xff, 0xff, 0x02, NULL			},

	{0   , 0xfe, 0   ,    5, "Coin A"		},
	{0x11, 0x01, 0x0e, 0x02, "2 Coin  1 Credit"	},
	{0x11, 0x01, 0x0e, 0x00, "1 Coin  1 Credit"	},
	{0x11, 0x01, 0x0e, 0x08, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0x0e, 0x04, "1 Coin  3 Credits"	},
	{0x11, 0x01, 0x0e, 0x0c, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x12, 0x01, 0x01, 0x00, "20k"			},
	{0x12, 0x01, 0x01, 0x01, "50k"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x02, 0x00, "Off"			},
	{0x12, 0x01, 0x02, 0x02, "On"			},
};

STDDIPINFO(Wwjgtin)

static struct BurnDIPInfo PinboDIPList[]=
{
	{0x11, 0xff, 0xff, 0x01, NULL			},
	{0x12, 0xff, 0xff, 0x0a, NULL			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x11, 0x01, 0x01, 0x01, "Upright"		},
	{0x11, 0x01, 0x01, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    5, "Coin A"		},
	{0x11, 0x01, 0x0e, 0x02, "2 Coin  1 Credit"	},
	{0x11, 0x01, 0x0e, 0x00, "1 Coin  1 Credit"	},
	{0x11, 0x01, 0x0e, 0x08, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0x0e, 0x04, "1 Coin  3 Credits"	},
	{0x11, 0x01, 0x0e, 0x0c, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x11, 0x01, 0x30, 0x00, "3"			},
	{0x11, 0x01, 0x30, 0x10, "4"			},
	{0x11, 0x01, 0x30, 0x20, "5"			},
	{0x11, 0x01, 0x30, 0x30, "70 (Cheat)"		},

	{0   , 0xfe, 0   ,    2, "Coin B"		},
	{0x11, 0x01, 0x40, 0x40, "2 Coins 1 Credit"	},
	{0x11, 0x01, 0x40, 0x00, "1 Coin  1 Credit"	},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x12, 0x01, 0x01, 0x01, "None"			},
	{0x12, 0x01, 0x01, 0x00, "500000 / 1000000"	},

	{0   , 0xfe, 0   ,    2, "Controls"		},
	{0x12, 0x01, 0x02, 0x00, "Reversed"		},
	{0x12, 0x01, 0x02, 0x02, "Normal"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x08, 0x00, "Off"			},
	{0x12, 0x01, 0x08, 0x08, "On"			},
};

STDDIPINFO(Pinbo)

static struct BurnDIPInfo ChameleoDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x01, NULL			},
	{0x10, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0f, 0x01, 0x01, 0x01, "Upright"		},
	{0x0f, 0x01, 0x01, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    5, "Coin A"		},
	{0x0f, 0x01, 0x0e, 0x02, "2 Coin  1 Credit"	},
	{0x0f, 0x01, 0x0e, 0x00, "1 Coin  1 Credit"	},
	{0x0f, 0x01, 0x0e, 0x08, "1 Coin  2 Credits"	},
	{0x0f, 0x01, 0x0e, 0x04, "1 Coin  3 Credits"	},
	{0x0f, 0x01, 0x0e, 0x0c, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x0f, 0x01, 0x30, 0x00, "3"			},
	{0x0f, 0x01, 0x30, 0x30, "5"			},

	{0   , 0xfe, 0   ,    2, "Coin B"		},
	{0x0f, 0x01, 0x40, 0x40, "2 Coins 1 Credit"	},
	{0x0f, 0x01, 0x40, 0x00, "1 Coin  1 Credit"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x10, 0x01, 0x08, 0x00, "Off"			},
	{0x10, 0x01, 0x08, 0x08, "On"			},
};

STDDIPINFO(Chameleo)

static void wwjgtinPaletteUpdate(); //forward.

static void lasso_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x1800:
		{
			soundlatch = data;

			if (game_select == 3) {
				ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
			} else {
				M6502Close();
				M6502Open(1);
				M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
				M6502Close();
				M6502Open(0);
			}
		}
		return;

		case 0x1801:
			back_color = data;
			wwjgtinPaletteUpdate();
		return;

		case 0x1802: {
			gfx_bank = (data >> 2) & 1;

			if (game_select == 3) {
				gfx_bank = gfx_bank | ((data & 0x08) >> 2);
			}

			if (game_select == 2) {
				gfx_bank = ((data & 0x04) ? 0 : 1) + ((data & 0x10) ? 2 : 0);
				track_enable = data & 0x08;
			}

			flipscreenx = data & 0x01;
			flipscreeny = data & 0x02;
		}
		return;

		case 0x1806:
		return; // nop

	// wwjgtin
		case 0x1c00:
		case 0x1c01:
		case 0x1c02:
			{
				last_colors[address & 3] = data;
				wwjgtinPaletteUpdate();
			}
		return;

		case 0x1c04:
		case 0x1c05:
		case 0x1c06:
		case 0x1c07:
			track_scroll[address & 3] = data;
		return;
	}
}

static UINT8 lasso_main_read(UINT16 address)
{
	switch (address)
	{
		case 0x1804:
		case 0x1805:
			return DrvInputs[address & 1];

		case 0x1806:
			return DrvDips[0];

		case 0x1807:
			return (DrvDips[1] & 0x0f) | (DrvInputs[2] ^ 0x30);
	}

	return 0;
}

static void lasso_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xb000:
			chip_data = data;
		return;

		case 0xb001:
		{
			INT32 data2 = BITSWAP08(chip_data, 0, 1, 2, 3, 4, 5, 6, 7);
			if (~data & 0x01) SN76496Write(0, data2);
			if (~data & 0x02) SN76496Write(1, data2);
		}
		return;

		case 0xb003:
			if (game_select == 2) {
				DACWrite(0, data);
			}
		return;
	}
}

static UINT8 lasso_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xb004:
			return 0x03;

		case 0xb005:
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
			return soundlatch;
	}

	return 0;
}


static void __fastcall pinbo_sound_write(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
		case 0x04:
		case 0x05:
			AY8910Write((port >> 2) & 1, port & 1, data);
		return;

		case 0x08:
		case 0x14:
		return;
	}
}

static UINT8 __fastcall pinbo_sound_read(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x02:
		case 0x06:
			return AY8910Read((port >> 2) & 1);

		case 0x08:
			//ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			return soundlatch;
	}

	return 0;
}

static INT32 DrvSyncDAC()
{
	return (INT32)(float)(nBurnSoundLen * (M6502TotalCycles() / (6000000.000 / (nBurnFPS / 100.000))));
}


static INT32 LassoDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	M6502Open(0);
	M6502Reset();
	M6502Close();

	if (game_select == 3)
	{
		ZetOpen(0);
		ZetReset();
		AY8910Reset(0);
		AY8910Reset(1);
		ZetClose();
	}
	else
	{
		M6502Open(1);
		M6502Reset();
		if (game_select == 2) DACReset();
		M6502Close();

		SN76496Reset();

		if (game_select == 0) {
			M6502Open(2);
			M6502Reset();
			M6502Close();
		}
	}

	HiscoreReset();

	track_enable = 0;
	back_color = 0;
	soundlatch = 0;
	chip_data = 0;
	gfx_bank = 0;
	flipscreenx = 0;
	flipscreeny = 0;

	memset (last_colors, 0, 3);
	memset (track_scroll, 0, 4);

	DrvInputs[2] = 0; // coin

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6502ROM0		= Next; Next += 0x010000;
	DrvZ80ROM		= Next;
	DrvM6502ROM1		= Next; Next += 0x010000;
	DrvM6502ROM2		= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += 0x020000;
	DrvGfxROM1		= Next; Next += 0x020000;
	DrvGfxROM2		= Next; Next += 0x010000; // 1. somehow these 2 wipe out the colprom below if they are set lower., in wwgjtin
	DrvMapROM		= Next; Next += 0x010000; // 2.

	DrvColPROM		= Next; Next += 0x000300;

	DrvPalette		= (UINT32*)Next; Next += 0x0140 * sizeof(UINT32);

	AllRam			= Next;

	DrvM6502RAM0		= Next; Next += 0x000800;
	DrvZ80RAM		= Next;
	DrvM6502RAM1		= Next; Next += 0x001000;
	DrvVidRAM		= Next; Next += 0x000400;
	DrvColRAM		= Next; Next += 0x000400;
	DrvSprRAM		= Next; Next += 0x000100;
	DrvShareRAM		= Next; Next += 0x000800;
	DrvBitmapRAM		= Next; Next += 0x002000;

	RamEnd			= Next;

	pAY8910Buffer[0]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[1]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[2]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[3]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[4]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[5]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);

	MemEnd			= Next;

	return 0;
}

static INT32 LassoGfxDecode(INT32 gfxlen)
{
	INT32 Plane0[3] = { ((gfxlen/4) * 8) * 0, ((gfxlen/4) * 8) * 2 };
	INT32 Plane1[3] = { ((gfxlen/4) * 8) * 1, ((gfxlen/4) * 8) * 3 };
	INT32 Plane2[3] = { ((gfxlen/6) * 8) * 0, ((gfxlen/6) * 8) * 2, ((gfxlen/6) * 8) * 4 };
	INT32 Plane3[3] = { ((gfxlen/6) * 8) * 1, ((gfxlen/6) * 8) * 3, ((gfxlen/6) * 8) * 5 };
	INT32 Plane4[4] = { (0x1000*8) * 1, (0x1000*8)*3, (0x1000*8)*0, (0x1000*8)*2 };
	INT32 XOffs[16] = { STEP8(0,1), STEP8(64,1) };
	INT32 YOffs[16] = { STEP8(0,8), STEP8(128,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(gfxlen);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, gfxlen);

	if (gfxlen == 0xc000) {
		GfxDecode(((gfxlen/3)*8)/(8*8),     3,  8,  8, Plane2, XOffs, YOffs, 0x040, tmp, DrvGfxROM0);
		GfxDecode(((gfxlen/3/2)*8)/(16*16), 3, 16, 16, Plane3, XOffs, YOffs, 0x100, tmp, DrvGfxROM1);
	} else {
		GfxDecode(((gfxlen/2)*8)/(8*8),     2,  8,  8, Plane0, XOffs, YOffs, 0x040, tmp, DrvGfxROM0);
		GfxDecode(((gfxlen/2/2)*8)/(16*16), 2, 16, 16, Plane1, XOffs, YOffs, 0x100, tmp, DrvGfxROM1);
	}

	memcpy (tmp, DrvGfxROM2, 0x4000);

	GfxDecode(((gfxlen/4)*8)/(16*16),   4, 16, 16, Plane4, XOffs, YOffs, 0x100, tmp, DrvGfxROM2);
	BurnFree(tmp);

	return 0;
}

static void DrvGfxDescramble(UINT8 *src, INT32 len)
{
	INT32 Lshift = (len == 0x4000) ? 2 : 1;
	UINT8 *tmp = (UINT8*)BurnMalloc(len);

	memcpy (tmp, src, len);

	for (INT32 i = 0; i < len; i++) {
		src[((i & 0x0800) << Lshift) | ((i & 0x3000) >> 1) | (i & 0x07ff)] = tmp[i];
	}

	BurnFree (tmp);
}

static INT32 LassoInit()
{
	game_select = 0;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvM6502ROM0 + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM0 + 0x2000,  1, 1)) return 1;

		if (BurnLoadRom(DrvM6502ROM1 + 0x5000,  2, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM1 + 0x6000,  3, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM1 + 0x7000,  4, 1)) return 1;

		if (BurnLoadRom(DrvM6502ROM2 + 0x0000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x0000,   6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x2000,   7, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x0000,   8, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0020,   9, 1)) return 1;

		DrvGfxDescramble(DrvGfxROM0 + 0x0000, 0x2000);
		DrvGfxDescramble(DrvGfxROM0 + 0x2000, 0x2000);
		LassoGfxDecode(0x4000);
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvM6502RAM0,		0x0000, 0x03ff, MAP_RAM);
	M6502MapMemory(DrvVidRAM,		0x0400, 0x07ff, MAP_RAM);
	M6502MapMemory(DrvColRAM,		0x0800, 0x0bff, MAP_RAM);
	M6502MapMemory(DrvSprRAM,		0x0c00, 0x0cff, MAP_RAM);
	M6502MapMemory(DrvShareRAM,		0x1000, 0x17ff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM0,		0x8000, 0xbfff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM0,		0xc000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(lasso_main_write);
	M6502SetReadHandler(lasso_main_read);
	M6502Close();

	M6502Init(1, TYPE_M6502);
	M6502Open(1);
	M6502MapMemory(DrvM6502RAM1,		0x0000, 0x01ff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM1 + 0x1000,	0x1000, 0x7fff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM1 + 0x7000,	0xf000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(lasso_sound_write);
	M6502SetReadHandler(lasso_sound_read);
	M6502Close();

	M6502Init(2, TYPE_M6502);
	M6502Open(2);
	M6502MapMemory(DrvShareRAM,		0x0000, 0x07ff, MAP_RAM);
	M6502MapMemory(DrvBitmapRAM,		0x2000, 0x3fff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM2,		0x8000, 0x8fff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM2,		0x9000, 0x9fff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM2,		0xa000, 0xafff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM2,		0xb000, 0xbfff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM2,		0xc000, 0xcfff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM2,		0xd000, 0xdfff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM2,		0xe000, 0xefff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM2,		0xf000, 0xffff, MAP_ROM);
	M6502Close();

	SN76489Init(0, 2000000, 0);
	SN76489Init(1, 2000000, 1);
	SN76496SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);
	SN76496SetRoute(1, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	LassoDoReset();

	return 0;
}

static INT32 ChameleoInit()
{
	game_select = 1;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvM6502ROM0 + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM0 + 0x2000,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM0 + 0x4000,  2, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM0 + 0x6000,  3, 1)) return 1;

		if (BurnLoadRom(DrvM6502ROM1 + 0x1000,  4, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM1 + 0x6000,  5, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM1 + 0x7000,  6, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x0000,   7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x2000,   8, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x0000,   9, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0020,  10, 1)) return 1;

		//DrvGfxDescramble(DrvGfxROM0 + 0x0000, 0x2000);
		//DrvGfxDescramble(DrvGfxROM0 + 0x2000, 0x2000);
		UINT8 *tmp = BurnMalloc(0x4000);
		memcpy(tmp, DrvGfxROM0, 0x4000);

		memcpy(DrvGfxROM0 + 0x0800, tmp + 0x0000, 0x800);
		memcpy(DrvGfxROM0 + 0x1800, tmp + 0x0800, 0x800);
		memcpy(DrvGfxROM0 + 0x0000, tmp + 0x1000, 0x800);
		memcpy(DrvGfxROM0 + 0x1000, tmp + 0x1800, 0x800);

		memcpy(DrvGfxROM0 + 0x2800, tmp + 0x2000, 0x800);
		memcpy(DrvGfxROM0 + 0x3800, tmp + 0x2800, 0x800);
		memcpy(DrvGfxROM0 + 0x2000, tmp + 0x3000, 0x800);
		memcpy(DrvGfxROM0 + 0x3000, tmp + 0x3800, 0x800);
		BurnFree(tmp);

		LassoGfxDecode(0x4000);
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvM6502RAM0,		0x0000, 0x03ff, MAP_RAM);
	M6502MapMemory(DrvVidRAM,		0x0400, 0x07ff, MAP_RAM);
	M6502MapMemory(DrvColRAM,		0x0800, 0x0bff, MAP_RAM);
	M6502MapMemory(DrvShareRAM,		0x0c00, 0x0fff, MAP_RAM); // not shared
	M6502MapMemory(DrvSprRAM,		0x1000, 0x10ff, MAP_RAM); // 0-7f
	M6502MapMemory(DrvM6502ROM0,		0x4000, 0xbfff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM0 + 0x6000,	0xe000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(lasso_main_write);
	M6502SetReadHandler(lasso_main_read);
	M6502Close();

	M6502Init(1, TYPE_M6502);
	M6502Open(1);
	M6502MapMemory(DrvM6502RAM1,		0x0000, 0x01ff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM1 + 0x1000,	0x1000, 0x7fff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM1 + 0x7000,	0xf000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(lasso_sound_write);
	M6502SetReadHandler(lasso_sound_read);
	M6502Close();

	SN76489Init(0, 2000000, 0);
	SN76489Init(1, 2000000, 1);
	SN76496SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);
	SN76496SetRoute(1, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	LassoDoReset();

	return 0;
}

static INT32 WwjgtinInit()
{
	game_select = 2;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvM6502ROM0 + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM0 + 0x4000,  1, 1)) return 1;

		if (BurnLoadRom(DrvM6502ROM1 + 0x4000,  2, 1)) return 1;


		if (BurnLoadRom(DrvGfxROM0 + 0x0000,   3, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x4000,   4, 1)) return 1;

		UINT8 *tmp = BurnMalloc(0x8000);
		memcpy(tmp, DrvGfxROM0, 0x8000);

		memcpy(DrvGfxROM0 + 0x0000, tmp + 0x0000, 0x800);
		memcpy(DrvGfxROM0 + 0x2000, tmp + 0x0800, 0x800);
		memcpy(DrvGfxROM0 + 0x0800, tmp + 0x1000, 0x800);
		memcpy(DrvGfxROM0 + 0x2800, tmp + 0x1800, 0x800);
		memcpy(DrvGfxROM0 + 0x1000, tmp + 0x2000, 0x800);
		memcpy(DrvGfxROM0 + 0x3000, tmp + 0x2800, 0x800);
		memcpy(DrvGfxROM0 + 0x1800, tmp + 0x3000, 0x800);
		memcpy(DrvGfxROM0 + 0x3800, tmp + 0x3800, 0x800);

		memcpy(DrvGfxROM0 + 0x4000, tmp + 0x4000, 0x800);
		memcpy(DrvGfxROM0 + 0x6000, tmp + 0x4800, 0x800);
		memcpy(DrvGfxROM0 + 0x4800, tmp + 0x5000, 0x800);
		memcpy(DrvGfxROM0 + 0x6800, tmp + 0x5800, 0x800);
		memcpy(DrvGfxROM0 + 0x5000, tmp + 0x6000, 0x800);
		memcpy(DrvGfxROM0 + 0x7000, tmp + 0x6800, 0x800);
		memcpy(DrvGfxROM0 + 0x5800, tmp + 0x7000, 0x800);
		memcpy(DrvGfxROM0 + 0x7800, tmp + 0x7800, 0x800);
		BurnFree(tmp);

		if (BurnLoadRom(DrvGfxROM2 + 0x0000,   5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x2000,   6, 1)) return 1;

		if (BurnLoadRom(DrvMapROM  + 0x0000,   7, 1)) return 1;
		if (BurnLoadRom(DrvMapROM  + 0x2000,   8, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x0000,   9, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0020,  10, 1)) return 1;

		//DrvGfxDescramble(DrvGfxROM0 + 0x0000, 0x4000);
		//DrvGfxDescramble(DrvGfxROM0 + 0x4000, 0x4000);
		LassoGfxDecode(0x8000);
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvM6502RAM0,		0x0000, 0x07ff, MAP_RAM);
	M6502MapMemory(DrvVidRAM,		0x0800, 0x0bff, MAP_RAM);
	M6502MapMemory(DrvColRAM,		0x0c00, 0x0fff, MAP_RAM);
	//M6502MapMemory(DrvShareRAM,		0x0c00, 0x0fff, MAP_RAM); // not shared
	M6502MapMemory(DrvSprRAM,		0x1000, 0x10ff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM0,		0x4000, 0xbfff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM0 + 0x4000,	0xc000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(lasso_main_write);
	M6502SetReadHandler(lasso_main_read);
	M6502Close();

	M6502Init(1, TYPE_M6502);
	M6502Open(1);
	M6502MapMemory(DrvM6502RAM1,		0x0000, 0x01ff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM1 + 0x4000,	0x4000, 0x7fff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM1 + 0x4000,	0xc000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(lasso_sound_write);
	M6502SetReadHandler(lasso_sound_read);
	M6502Close();

	SN76489Init(0, 2000000, 0);
	SN76489Init(1, 2000000, 1);
	SN76496SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);
	SN76496SetRoute(1, 1.00, BURN_SND_ROUTE_BOTH);

	DACInit(0, 0, 1, DrvSyncDAC);
	DACSetRoute(0, 1.0, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	LassoDoReset();

	return 0;
}

static INT32 PinboInit()
{
	game_select = 3;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvM6502ROM0 + 0x2000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM0 + 0x6000,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM0 + 0x8000,  2, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM0 + 0xa000,  3, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM   + 0x0000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x0000,   5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x4000,   6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x8000,   7, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x0000,   8, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0100,   9, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0200,  10, 1)) return 1;

		UINT8 *tmp = BurnMalloc(0xc000);
		memcpy(tmp, DrvGfxROM0, 0xc000);

		memcpy(DrvGfxROM0 + 0x0000, tmp + 0x0000, 0x800);
		memcpy(DrvGfxROM0 + 0x2000, tmp + 0x0800, 0x800);
		memcpy(DrvGfxROM0 + 0x0800, tmp + 0x1000, 0x800);
		memcpy(DrvGfxROM0 + 0x2800, tmp + 0x1800, 0x800);
		memcpy(DrvGfxROM0 + 0x1000, tmp + 0x2000, 0x800);
		memcpy(DrvGfxROM0 + 0x3000, tmp + 0x2800, 0x800);
		memcpy(DrvGfxROM0 + 0x1800, tmp + 0x3000, 0x800);
		memcpy(DrvGfxROM0 + 0x3800, tmp + 0x3800, 0x800);

		memcpy(DrvGfxROM0 + 0x4000, tmp + 0x4000, 0x800);
		memcpy(DrvGfxROM0 + 0x6000, tmp + 0x4800, 0x800);
		memcpy(DrvGfxROM0 + 0x4800, tmp + 0x5000, 0x800);
		memcpy(DrvGfxROM0 + 0x6800, tmp + 0x5800, 0x800);
		memcpy(DrvGfxROM0 + 0x5000, tmp + 0x6000, 0x800);
		memcpy(DrvGfxROM0 + 0x7000, tmp + 0x6800, 0x800);
		memcpy(DrvGfxROM0 + 0x5800, tmp + 0x7000, 0x800);
		memcpy(DrvGfxROM0 + 0x7800, tmp + 0x7800, 0x800);

		memcpy(DrvGfxROM0 + 0x8000, tmp + 0x8000, 0x800);
		memcpy(DrvGfxROM0 + 0xa000, tmp + 0x8800, 0x800);
		memcpy(DrvGfxROM0 + 0x8800, tmp + 0x9000, 0x800);
		memcpy(DrvGfxROM0 + 0xa800, tmp + 0x9800, 0x800);
		memcpy(DrvGfxROM0 + 0x9000, tmp + 0xa000, 0x800);
		memcpy(DrvGfxROM0 + 0xb000, tmp + 0xa800, 0x800);
		memcpy(DrvGfxROM0 + 0x9800, tmp + 0xb000, 0x800);
		memcpy(DrvGfxROM0 + 0xb800, tmp + 0xb800, 0x800);
		BurnFree(tmp);
#if 0
	ROM_LOAD( "rom7.d1",     0x8000, 0x0800, CRC(327a3c21) SHA1(e938915d28ac4ec033b20d33728788493e3f30f6) ) /* 3rd bitplane */
	ROM_CONTINUE(            0xa000, 0x0800 )
	ROM_CONTINUE(            0x8800, 0x0800 )
	ROM_CONTINUE(            0xa800, 0x0800 )
	ROM_CONTINUE(            0x9000, 0x0800 )
	ROM_CONTINUE(            0xb000, 0x0800 )
	ROM_CONTINUE(            0x9800, 0x0800 )
	ROM_CONTINUE(            0xb800, 0x0800 )
#endif
		//DrvGfxDescramble(DrvGfxROM0 + 0x0000, 0x4000);
		//DrvGfxDescramble(DrvGfxROM0 + 0x4000, 0x4000);
		//DrvGfxDescramble(DrvGfxROM0 + 0x8000, 0x4000);
		LassoGfxDecode(0xc000);
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvM6502RAM0,		0x0000, 0x03ff, MAP_RAM);
	M6502MapMemory(DrvVidRAM,		0x0400, 0x07ff, MAP_RAM);
	M6502MapMemory(DrvColRAM,		0x0800, 0x0bff, MAP_RAM);
	M6502MapMemory(DrvSprRAM,		0x1000, 0x10ff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM0 + 0x2000,	0x2000, 0x3fff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM0 + 0x6000,	0x6000, 0xbfff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM0 + 0xa000,	0xe000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(lasso_main_write);
	M6502SetReadHandler(lasso_main_read);
	M6502Close();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,	0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,	0xf000, 0xffff, MAP_RAM);
	ZetSetInHandler(pinbo_sound_read);
	ZetSetOutHandler(pinbo_sound_write);
	ZetClose();

	AY8910Init(0, 1500000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910Init(1, 1500000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.30, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.30, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	LassoDoReset();

	return 0;
}

static INT32 LassoExit()
{
	GenericTilesExit();

	M6502Exit();
	if (game_select == 3) {
		ZetExit();
		AY8910Exit(0);
		AY8910Exit(1);
	} else {
		SN76496Exit();
	}
	if (game_select == 2) DACExit();

	BurnFree(AllMem);

	return 0;
}

static UINT32 color_calculate(UINT8 d)
{
	INT32 r,g,b,b0,b1,b2;

	b0 = ((d >> 0) & 1) * 0x21;
	b1 = ((d >> 1) & 1) * 0x47;
	b2 = ((d >> 2) & 1) * 0x97;
	r = b0 + b1 + b2;

	b0 = ((d >> 3) & 1) * 0x21;
	b1 = ((d >> 4) & 1) * 0x47;
	b2 = ((d >> 5) & 1) * 0x97;
	g = b0 + b1 + b2;

	b0 = ((d >> 6) & 1) * 0x4f;
	b1 = ((d >> 7) & 1) * 0xa8;
	b = b0 + b1;

	return BurnHighCol(r,g,b,0);
}

static void wwjgtinPaletteUpdate()
{
	if (game_select != 2) return; // only for wwjgtin!

	DrvPalette[0x3d] = color_calculate(last_colors[0]);
	DrvPalette[0x3e] = color_calculate(last_colors[1]);
	DrvPalette[0x3f] = color_calculate(last_colors[2]);

	DrvPalette[0] = color_calculate(back_color);

	for (INT32 i = 0x40; i < 0x140; i++) {
		UINT8 ctabentry;

		if ((i - 0x40) & 0x03)
			ctabentry = ((((i - 0x40) & 0xf0) >> 2) + ((i - 0x40) & 0x0f)) & 0x3f;
		else
			ctabentry = 0;

		DrvPalette[i] = DrvPalette[ctabentry];
	}
}

static void LassoPaletteInit()
{
	for (INT32 i = 0; i < 0x40; i++) {
		DrvPalette[i] = color_calculate(DrvColPROM[i]);
	}

	if (game_select == 2) // wwjgtin
		wwjgtinPaletteUpdate();
}

static void PinboPaletteInit()
{
	// complete guess - iq_132!
	for (INT32 i = 0; i < 0x100; i++) {
		INT32 r = DrvColPROM[i] & 0xf;
		INT32 g = DrvColPROM[i+0x100] & 0x0f;
		INT32 b = DrvColPROM[i+0x200] & 0x0f;

		DrvPalette[i] = BurnHighCol(r+(r*16), g+(g*16), b+(b*16), 0);
	}
}

static void draw_sprites(INT32 ram_size, INT32 bpp, INT32 reverse)
{
	const UINT8 *finish, *source;
	INT32 inc;

	if (reverse)
	{
		source = DrvSprRAM;
		finish = DrvSprRAM + ram_size;
		inc = 4;
	}
	else
	{
		source = DrvSprRAM + ram_size - 4;
		finish = DrvSprRAM - 4;
		inc = -4;
	}

	while (source != finish)
	{
		INT32 sx = source[3];
		INT32 sy = source[0];
		INT32 flipx = source[1] & 0x40;
		INT32 flipy = source[1] & 0x80;

		if (flipscreenx)
		{
			sx = 240 - sx;
			flipx = !flipx;
		}

		if (flipscreeny)
			flipy = !flipy;
		else
			sy = 240 - sy;

		INT32 code = (source[1] & 0x3f) | (gfx_bank * 0x40);
		INT32 color = source[2] & 0x0f;

		if (flipy) {
			if (flipx) {
				Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy - 16, color, bpp, 0, 0, DrvGfxROM1);
			} else {
				Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy - 16, color, bpp, 0, 0, DrvGfxROM1);
			}
		} else {
			if (flipx) {
				Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy - 16, color, bpp, 0, 0, DrvGfxROM1);
			} else {
				Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy - 16, color, bpp, 0, 0, DrvGfxROM1);
			}
		}

		source += inc;
	}
}

static void lasso_draw_layer(INT32 bpp, INT32 bank_type)
{
	for (INT32 offs = (2 * 32); offs < (32 * 32) - (2 * 32); offs++)
	{
		INT32 sx = (offs & 0x1f) * 8;
		INT32 sy =((offs / 0x20) * 8) - 16;

		INT32 code  = DrvVidRAM[offs];
		INT32 color = DrvColRAM[offs];

		if (bank_type) {
			code |= (color & 0x30) << 4;
		} else {
			code |= (gfx_bank << 8);
		}

		Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy, color & 0xf, bpp, 0, 0, DrvGfxROM0);
	}
}

static void wwjgtin_draw_track_layer()
{
	INT32 scrollx = (track_scroll[0] + (track_scroll[1] * 256)) & 0x7ff;
	INT32 scrolly = (track_scroll[2] + (track_scroll[3] * 256) + 16) & 0x3ff;

	INT32 yy = scrolly & 0xf;
	INT32 xx = scrollx & 0xf;

	for (INT32 y = 0; y < 16 * (16 + 1); y+=16)
	{
		INT32 sy = ((y + scrolly) & 0x3f0) << 3;

		for (INT32 x = 0; x < 16 * (16 + 1); x += 16)
		{
			INT32 offs = sy + (((x + scrollx) >> 4) & 0x7f);

			INT32 code  = DrvMapROM[offs];
			INT32 color =(DrvMapROM[offs + 0x2000] & 0x0f) + (0x40/16);

			Render16x16Tile_Mask_Clip(pTransDraw, code, x - xx, y - yy, color, 4, 0, 0, DrvGfxROM2);
		}
	}
}

static void lasso_draw_bitmap()
{
	INT32 inc = (flipscreenx) ? -1 : 1;

	for (INT32 offs = (16 * 32); offs < (256 * 32) - (16 * 32); offs++)
	{
		UINT8 y = (offs / 0x20) - 16;
		UINT8 x = (offs & 0x1f) * 8;
		if (flipscreeny) y = ~y;
		if (flipscreenx) x = ~x;

		if (/*(y < 0) ||*/ (y >= nScreenHeight) || /*(x < 0) ||*/ (x >= nScreenWidth))
			continue;

		UINT8 data = DrvBitmapRAM[offs];
		if (data == 0) continue;

		UINT16 *dst = pTransDraw + (y * nScreenWidth);

		for (INT32 bit = 0; bit < 8; bit++, data<<=1, x+=inc)
		{
			if (data & 0x80) // need to clip?
				dst[x] = 0x3f;
		}
	}
}

static INT32 get_black_palette_entry()
{
	for (INT32 i = 0; i < BurnDrvGetPaletteEntries(); i++) {
		if (DrvPalette[i] == 0) {
			return i;
		}
	}
	return BurnDrvGetPaletteEntries() - 1; // give up.
}

static INT32 WwjgtinDraw()
{
	if (DrvRecalc) {
		LassoPaletteInit();
		wwjgtinPaletteUpdate();
		DrvRecalc = 0;
	}

	DrvPalette[0] = color_calculate(back_color);

	BurnTransferClear();

	if (track_enable) {
		wwjgtin_draw_track_layer();
	} else {
		INT32 fill_color = get_black_palette_entry();
		for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
			pTransDraw[i] = fill_color;
		}
	}

	draw_sprites(0x100, 2, 1);
	lasso_draw_layer(2, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 LassoDraw()
{
	if (DrvRecalc) {
		LassoPaletteInit();
		DrvRecalc = 0;
	}

	DrvPalette[0] = color_calculate(back_color);

	BurnTransferClear();

	lasso_draw_layer(2,0);
	if (game_select == 0) lasso_draw_bitmap();
	draw_sprites(0x80, 2, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 PinboDraw()
{
	if (DrvRecalc) {
		PinboPaletteInit();
		DrvRecalc = 0;
	}

	DrvPalette[0] = color_calculate(back_color);

	BurnTransferClear();

	lasso_draw_layer(3,1);
	draw_sprites(0x100, 3, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 LassoFrame()
{
	if (DrvReset) {
		LassoDoReset();
	}

	M6502NewFrame();

	{
		memset (DrvInputs, 0, 3);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
		if ((DrvInputs[2] & 0x30) != previous_coin) {
			M6502Open(0);
			M6502SetIRQLine(0x20, (DrvInputs[2] & 0x30) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
			M6502Close();
			previous_coin = DrvInputs[2] & 0x30;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[3] = { (11289000/16) / 60, 600000 / 60, (11289000/16) / 60 };
	INT32 nCyclesDone[3] = { 0, 0, 0 };
	INT32 nSoundBufferPos = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		M6502Open(0);
		nCyclesDone[0] += M6502Run(nCyclesTotal[0] / nInterleave);
		if (i == 240) M6502SetIRQLine(0, CPU_IRQSTATUS_AUTO);
		M6502Close();

		M6502Open(1);
		nCyclesDone[1] += M6502Run(nCyclesTotal[1] / nInterleave);
		M6502Close();

		if (game_select == 0) {
			M6502Open(2);
			nCyclesDone[2] += M6502Run(nCyclesTotal[2] / nInterleave);
			M6502Close();
		}

		// Render Sound Segment
		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			SN76496Update(0, pSoundBuf, nSegmentLength);
			SN76496Update(1, pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	M6502Open(1);

	// Make sure the buffer is entirely filled.
	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			SN76496Update(0, pSoundBuf, nSegmentLength);
			SN76496Update(1, pSoundBuf, nSegmentLength);
		}
		if (game_select == 2) DACUpdate(pBurnSoundOut, nBurnSoundLen);
	}

	M6502Close();

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 PinboFrame()
{
	if (DrvReset) {
		LassoDoReset();
	}

	{
		memset (DrvInputs, 0, 3);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
		if ((DrvInputs[2] & 0x30) != previous_coin) {
			M6502Open(0);
			M6502SetIRQLine(0x20, (DrvInputs[2] & 0x30) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
			M6502Close();
			previous_coin = DrvInputs[2] & 0x30;
		}
	}

	INT32 nInterleave = 100;
	INT32 nCyclesTotal[2] = { 750000 / 60, 3000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	M6502Open(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone[0] += M6502Run(nCyclesTotal[0] / nInterleave);
		if (i == (nInterleave - 1)) M6502SetIRQLine(0, CPU_IRQSTATUS_AUTO);

		nCyclesDone[1] += ZetRun(nCyclesTotal[1] / nInterleave);
	}

	ZetClose();
	M6502Close();

	if (pBurnSoundOut) {
		AY8910Render(&pAY8910Buffer[0], pBurnSoundOut, nBurnSoundLen, 0);
	}

	if (pBurnDraw) {
		PinboDraw();
	}

	return 0;
}

static INT32 LassoScan(INT32 nAction, INT32 *pnMin)
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

		M6502Scan(nAction);
		if (game_select == 3) {
			ZetScan(nAction);
			AY8910Scan(nAction, pnMin);
		} else {
			SN76496Scan(nAction,pnMin);
		}
		if (game_select == 2) DACScan(nAction, pnMin);

		SCAN_VAR(back_color);
		SCAN_VAR(soundlatch);
		SCAN_VAR(chip_data);
		SCAN_VAR(gfx_bank);
		SCAN_VAR(flipscreenx);
		SCAN_VAR(flipscreeny);
		SCAN_VAR(track_enable);

		SCAN_VAR(last_colors);
		SCAN_VAR(track_scroll);
		SCAN_VAR(track_enable);
	}

	return 0;
}


// Lasso

static struct BurnRomInfo lassoRomDesc[] = {
	{ "wm3",	0x2000, 0xf93addd6, 0 | BRF_PRG | BRF_ESS }, //  0 - M6502 #0 Code
	{ "wm4",	0x2000, 0x77719859, 0 | BRF_PRG | BRF_ESS }, //  1

	{ "wmc",	0x1000, 0x8b4eb242, 1 | BRF_PRG | BRF_ESS }, //  2 - M6502 #1 Code
	{ "wmb",	0x1000, 0x4658bcb9, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "wma",	0x1000, 0x2e7de3e9, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "wm5",	0x1000, 0x7dc3ff07, 2 | BRF_PRG | BRF_ESS }, //  5 - M6502 #2 Code

	{ "wm1",	0x2000, 0x7db77256, 3 | BRF_GRA },           //  6 - Graphics Tiles
	{ "wm2",	0x2000, 0x9e7d0b6f, 3 | BRF_GRA },           //  7

	{ "82s123.69",	0x0020, 0x1eabb04d, 6 | BRF_GRA },           //  8 - Color PROM
	{ "82s123.70",	0x0020, 0x09060f8c, 6 | BRF_GRA },           //  9
};

STD_ROM_PICK(lasso)
STD_ROM_FN(lasso)

struct BurnDriver BurnDrvLasso = {
	"lasso", NULL, NULL, NULL, "1982",
	"Lasso\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_16BIT_ONLY | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, lassoRomInfo, lassoRomName, NULL, NULL, LassoInputInfo, LassoDIPInfo,
	LassoInit, LassoExit, LassoFrame, LassoDraw, LassoScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Chameleon

static struct BurnRomInfo chameleoRomDesc[] = {
	{ "chamel4.bin", 0x2000, 0x97379c47, 0 | BRF_PRG | BRF_ESS }, //  0 - M6502 #0 Code
	{ "chamel5.bin", 0x2000, 0x0a2cadfd, 0 | BRF_PRG | BRF_ESS }, //  1
	{ "chamel6.bin", 0x2000, 0xb023c354, 0 | BRF_PRG | BRF_ESS }, //  2
	{ "chamel7.bin", 0x2000, 0xa5a03375, 0 | BRF_PRG | BRF_ESS }, //  3

	{ "chamel3.bin", 0x1000, 0x52eab9ec, 1 | BRF_PRG | BRF_ESS }, //  4 - M6502 #1 Code
	{ "chamel2.bin", 0x1000, 0x81dcc49c, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "chamel1.bin", 0x1000, 0x96031d3b, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "chamel8.bin", 0x2000, 0xdc67916b, 3 | BRF_GRA },           //  7 - Graphics Tiles
	{ "chamel9.bin", 0x2000, 0x6b559bf1, 3 | BRF_GRA },           //  8

	{ "chambprm.bin",0x0020, 0xe3ad76df, 6 | BRF_GRA },           //  9 - Color PROM
	{ "chamaprm.bin",0x0020, 0xc7063b54, 6 | BRF_GRA },           // 10
};

STD_ROM_PICK(chameleo)
STD_ROM_FN(chameleo)

struct BurnDriver BurnDrvChameleo = {
	"chameleo", NULL, NULL, NULL, "1983",
	"Chameleon\0", NULL, "Jaleco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, chameleoRomInfo, chameleoRomName, NULL, NULL, ChameleoInputInfo, ChameleoDIPInfo,
	ChameleoInit, LassoExit, LassoFrame, LassoDraw, LassoScan, &DrvRecalc, 0x40,
	256, 224, 4, 3
};


// Wai Wai Jockey Gate-In!

static struct BurnRomInfo wwjgtinRomDesc[] = {
	{ "ic2.6",	0x4000, 0x744ba45b, 0 | BRF_PRG | BRF_ESS }, //  0 - M6502 #0 Code
	{ "ic5.5",	0x4000, 0xaf751614, 0 | BRF_PRG | BRF_ESS }, //  1

	{ "ic59.9",	0x4000, 0x2ecb4d98, 1 | BRF_PRG | BRF_ESS }, //  2 - M6502 #1 Code

	{ "ic81.7",	0x4000, 0xa27f1a63, 3 | BRF_GRA },           //  3 - Graphics Tiles
	{ "ic82.8",	0x4000, 0xea2862b3, 3 | BRF_GRA },           //  4

	{ "ic47.3",	0x2000, 0x40594c59, 4 | BRF_GRA },           //  5 - Background Tiles
	{ "ic46.4",	0x2000, 0xd1921348, 4 | BRF_GRA },           //  6

	{ "ic48.2",	0x2000, 0xa4a7df77, 5 | BRF_GRA },           //  7 - Background Tile Map
	{ "ic49.1",	0x2000, 0xe480fbba, 5 | BRF_GRA },           //  8

	{ "2.bpr",	0x0020, 0x79adda5d, 6 | BRF_GRA },           //  9 - Color PROM
	{ "1.bpr",	0x0020, 0xc1a93cc8, 6 | BRF_GRA },           // 10
};

STD_ROM_PICK(wwjgtin)
STD_ROM_FN(wwjgtin)

struct BurnDriver BurnDrvWwjgtin = {
	"wwjgtin", NULL, NULL, NULL, "1984",
	"Wai Wai Jockey Gate-In!\0", NULL, "Jaleco / Casio", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, wwjgtinRomInfo, wwjgtinRomName, NULL, NULL, WwjgtinInputInfo, WwjgtinDIPInfo,
	WwjgtinInit, LassoExit, LassoFrame, WwjgtinDraw, LassoScan, &DrvRecalc, 0x140,
	256, 224, 4, 3
};


// Photo Finish (bootleg?)

static struct BurnRomInfo photofRomDesc[] = {
	{ "ic2.bin",	0x4000, 0x4d960b54, 0 | BRF_PRG | BRF_ESS }, //  0 - M6502 #0 Code
	{ "ic6.bin",	0x4000, 0xa4ad21dc, 0 | BRF_PRG | BRF_ESS }, //  1

	{ "ic59.bin",	0x4000, 0x2ecb4d98, 1 | BRF_PRG | BRF_ESS }, //  2 - M6502 #1 Code

	{ "ic81.bin",	0x4000, 0x0f170253, 3 | BRF_GRA },           //  3 - Graphics Tiles
	{ "ic82.bin",	0x4000, 0xc4cadee9, 3 | BRF_GRA },           //  4

	{ "3-ic47.bin",	0x2000, 0x40594c59, 4 | BRF_GRA },           //  6 - Background Tiles
	{ "4-ic46.bin",	0x2000, 0xd1921348, 4 | BRF_GRA },           //  7

	{ "2-ic48.bin",	0x2000, 0xa4a7df77, 5 | BRF_GRA },           //  8 - Background Tile Map
	{ "1-ic49.bin",	0x2000, 0xe480fbba, 5 | BRF_GRA },           //  9

	{ "2.bpr",	0x0020, 0x79adda5d, 6 | BRF_GRA },           // 10 - Color PROM
	{ "1.bpr",	0x0020, 0xc1a93cc8, 6 | BRF_GRA },           // 11
};

STD_ROM_PICK(photof)
STD_ROM_FN(photof)

struct BurnDriver BurnDrvPhotof = {
	"photof", "wwjgtin", NULL, NULL, "1991",
	"Photo Finish (bootleg?)\0", NULL, "Jaleco / Casio", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_16BIT_ONLY | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, photofRomInfo, photofRomName, NULL, NULL, WwjgtinInputInfo, WwjgtinDIPInfo,
	WwjgtinInit, LassoExit, LassoFrame, WwjgtinDraw, LassoScan, &DrvRecalc, 0x140,
	256, 224, 4, 3
};


// Pinbo (set 1)

static struct BurnRomInfo pinboRomDesc[] = {
	{ "rom2.b7",	0x2000, 0x9a185338, 0 | BRF_PRG | BRF_ESS }, //  0 - M6502 Code
	{ "rom3.e7",	0x2000, 0x1cd1b3bd, 0 | BRF_PRG | BRF_ESS }, //  1
	{ "rom4.h7",	0x2000, 0xba043fa7, 0 | BRF_PRG | BRF_ESS }, //  2
	{ "rom5.j7",	0x2000, 0xe71046c4, 0 | BRF_PRG | BRF_ESS }, //  3

	{ "rom1.s8",	0x2000, 0xca45a1be, 0 | BRF_PRG | BRF_ESS }, //  4 - Z80 Code

	{ "rom6.a1",	0x4000, 0x74fe8e98, 3 | BRF_GRA },           //  5 - Graphics Tiles
	{ "rom8.c1",	0x4000, 0x5a800fe7, 3 | BRF_GRA },           //  6
	{ "rom7.d1",	0x4000, 0x327a3c21, 3 | BRF_GRA },           //  7

	{ "red.l10",	0x0100, 0xe6c9ba52, 6 | BRF_GRA },           //  8 - Color PROM
	{ "green.k10",	0x0100, 0x1bf2d335, 6 | BRF_GRA },           //  9
	{ "blue.n10",	0x0100, 0xe41250ad, 6 | BRF_GRA },           // 10
};

STD_ROM_PICK(pinbo)
STD_ROM_FN(pinbo)

struct BurnDriver BurnDrvPinbo = {
	"pinbo", NULL, NULL, NULL, "1984",
	"Pinbo (set 1)\0", NULL, "Jaleco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_16BIT_ONLY | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, pinboRomInfo, pinboRomName, NULL, NULL, PinboInputInfo, PinboDIPInfo,
	PinboInit, LassoExit, PinboFrame, PinboDraw, LassoScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};
