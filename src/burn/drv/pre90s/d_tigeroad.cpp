// FB Alpha Tiger Road driver module
// Based on MAME driver by Phil Stroffolino

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "mcs51.h"
#include "burn_ym2203.h"
#include "msm5205.h"
#include "bitswap.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvMCURom;
static UINT8 *DrvSndROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *Drv68KRAM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprBuf;
static UINT8 *DrvVidRAM;
static UINT8 *DrvScrollRAM;
static UINT32 *DrvPalette;
static UINT8 *DrvTransMask;

static UINT8 DrvRecalc;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvDip[2];
static UINT8 DrvReset;
static UINT16 DrvInputs[2];

static UINT8 *soundlatch;
static UINT8 *soundlatch2;
static UINT8 *flipscreen;
static UINT8 *bgcharbank;
static UINT8 *coin_lockout;
static UINT8 *last_port3; // f1dream mcu

static INT32 nF1dream = 0;
static INT32 toramich = 0;

static struct BurnInputInfo TigeroadInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 14,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 8,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 15,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 9,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 11,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 10,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 9,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 8,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDip + 0,		"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDip + 1,		"dip"		},
};

STDINPUTINFO(Tigeroad)

static struct BurnDIPInfo TigeroadDIPList[] =
{
	DIP_OFFSET(0x11)
	{0x00, 0xff, 0xff, 0xff, NULL					},
	{0x01, 0xff, 0xff, 0xfb, NULL					},

	{0   , 0xfe, 0   ,    8, "Coin B"				},
	{0x00, 0x01, 0x07, 0x00, "4 Coins 1 Credits "	},
	{0x00, 0x01, 0x07, 0x01, "3 Coins 1 Credits "	},
	{0x00, 0x01, 0x07, 0x02, "2 Coins 1 Credits "	},
	{0x00, 0x01, 0x07, 0x07, "1 Coin 1 Credits "	},
	{0x00, 0x01, 0x07, 0x06, "1 Coin 2 Credits "	},
	{0x00, 0x01, 0x07, 0x05, "1 Coin 3 Credits "	},
	{0x00, 0x01, 0x07, 0x04, "1 Coin 4 Credits "	},
	{0x00, 0x01, 0x07, 0x03, "1 Coin 6 Credits "	},

	{0   , 0xfe, 0   ,    8, "Coin A"				},
	{0x00, 0x01, 0x38, 0x00, "4 Coins 1 Credits "	},
	{0x00, 0x01, 0x38, 0x08, "3 Coins 1 Credits "	},
	{0x00, 0x01, 0x38, 0x10, "2 Coins 1 Credits "	},
	{0x00, 0x01, 0x38, 0x38, "1 Coin 1 Credits "	},
	{0x00, 0x01, 0x38, 0x30, "1 Coin 2 Credits "	},
	{0x00, 0x01, 0x38, 0x28, "1 Coin 3 Credits "	},
	{0x00, 0x01, 0x38, 0x20, "1 Coin 4 Credits "	},
	{0x00, 0x01, 0x38, 0x18, "1 Coin 6 Credits "	},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x00, 0x01, 0x40, 0x00, "On"					},
	{0x00, 0x01, 0x40, 0x40, "Off"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x00, 0x01, 0x80, 0x80, "Off"					},
	{0x00, 0x01, 0x80, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x01, 0x01, 0x03, 0x03, "3"					},
	{0x01, 0x01, 0x03, 0x02, "4"					},
	{0x01, 0x01, 0x03, 0x01, "5"					},
	{0x01, 0x01, 0x03, 0x00, "7"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x01, 0x01, 0x04, 0x00, "Upright"				},
	{0x01, 0x01, 0x04, 0x04, "Cocktail"				},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x01, 0x01, 0x18, 0x18, "20000 70000 70000"	},
	{0x01, 0x01, 0x18, 0x10, "20000 80000 80000"	},
	{0x01, 0x01, 0x18, 0x08, "30000 80000 80000"	},
	{0x01, 0x01, 0x18, 0x00, "30000 90000 90000"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x01, 0x01, 0x60, 0x20, "Very_Easy"			},
	{0x01, 0x01, 0x60, 0x40, "Easy"					},
	{0x01, 0x01, 0x60, 0x60, "Normal"				},
	{0x01, 0x01, 0x60, 0x00, "Difficult"			},

	{0   , 0xfe, 0   ,    2, "Allow_Continue"		},
	{0x01, 0x01, 0x80, 0x00, "No"					},
	{0x01, 0x01, 0x80, 0x80, "Yes"					},
};

STDDIPINFO(Tigeroad)

static struct BurnDIPInfo ToramichDIPList[] =
{
	DIP_OFFSET(0x11)
	{0x00, 0xff, 0xff, 0xff, NULL					},
	{0x01, 0xff, 0xff, 0xfb, NULL					},

	{0   , 0xfe, 0   ,    8, "Coin B"				},
	{0x00, 0x01, 0x07, 0x00, "4 Coins 1 Credits "	},
	{0x00, 0x01, 0x07, 0x01, "3 Coins 1 Credits "	},
	{0x00, 0x01, 0x07, 0x02, "2 Coins 1 Credits "	},
	{0x00, 0x01, 0x07, 0x07, "1 Coin 1 Credits "	},
	{0x00, 0x01, 0x07, 0x06, "1 Coin 2 Credits "	},
	{0x00, 0x01, 0x07, 0x05, "1 Coin 3 Credits "	},
	{0x00, 0x01, 0x07, 0x04, "1 Coin 4 Credits "	},
	{0x00, 0x01, 0x07, 0x03, "1 Coin 6 Credits "	},

	{0   , 0xfe, 0   ,    8, "Coin A"				},
	{0x00, 0x01, 0x38, 0x00, "4 Coins 1 Credits "	},
	{0x00, 0x01, 0x38, 0x08, "3 Coins 1 Credits "	},
	{0x00, 0x01, 0x38, 0x10, "2 Coins 1 Credits "	},
	{0x00, 0x01, 0x38, 0x38, "1 Coin 1 Credits "	},
	{0x00, 0x01, 0x38, 0x30, "1 Coin 2 Credits "	},
	{0x00, 0x01, 0x38, 0x28, "1 Coin 3 Credits "	},
	{0x00, 0x01, 0x38, 0x20, "1 Coin 4 Credits "	},
	{0x00, 0x01, 0x38, 0x18, "1 Coin 6 Credits "	},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x00, 0x01, 0x40, 0x00, "On"					},
	{0x00, 0x01, 0x40, 0x40, "Off"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x00, 0x01, 0x80, 0x80, "Off"					},
	{0x00, 0x01, 0x80, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x01, 0x01, 0x03, 0x03, "3"					},
	{0x01, 0x01, 0x03, 0x02, "4"					},
	{0x01, 0x01, 0x03, 0x01, "5"					},
	{0x01, 0x01, 0x03, 0x00, "7"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x01, 0x01, 0x04, 0x00, "Upright"				},
	{0x01, 0x01, 0x04, 0x04, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Bonus Life"			},
	{0x01, 0x01, 0x08, 0x08, "20000 70000 70000"	},
	{0x01, 0x01, 0x08, 0x00, "20000 80000 80000"	},

	{0   , 0xfe, 0   ,    2, "Allow Level Select"	},
	{0x01, 0x01, 0x10, 0x10, "No"					},
	{0x01, 0x01, 0x10, 0x00, "Yes"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x01, 0x01, 0x60, 0x40, "Easy"					},
	{0x01, 0x01, 0x60, 0x60, "Normal"				},
	{0x01, 0x01, 0x60, 0x20, "Difficult"			},
	{0x01, 0x01, 0x60, 0x00, "Very Difficult"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x01, 0x01, 0x80, 0x00, "No"					},
	{0x01, 0x01, 0x80, 0x80, "Yes"					},
};

STDDIPINFO(Toramich)

static struct BurnDIPInfo F1dreamDIPList[] =
{
	DIP_OFFSET(0x11)
	{0x00, 0xff, 0xff, 0xff, NULL					},
	{0x01, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    8, "Coin B"				},
	{0x00, 0x01, 0x07, 0x00, "4 Coins 1 Credits "	},
	{0x00, 0x01, 0x07, 0x01, "3 Coins 1 Credits "	},
	{0x00, 0x01, 0x07, 0x02, "2 Coins 1 Credits "	},
	{0x00, 0x01, 0x07, 0x07, "1 Coin 1 Credits "	},
	{0x00, 0x01, 0x07, 0x06, "1 Coin 2 Credits "	},
	{0x00, 0x01, 0x07, 0x05, "1 Coin 3 Credits "	},
	{0x00, 0x01, 0x07, 0x04, "1 Coin 4 Credits "	},
	{0x00, 0x01, 0x07, 0x03, "1 Coin 6 Credits "	},

	{0   , 0xfe, 0   ,    8, "Coin A"				},
	{0x00, 0x01, 0x38, 0x00, "4 Coins 1 Credits "	},
	{0x00, 0x01, 0x38, 0x08, "3 Coins 1 Credits "	},
	{0x00, 0x01, 0x38, 0x10, "2 Coins 1 Credits "	},
	{0x00, 0x01, 0x38, 0x38, "1 Coin 1 Credits "	},
	{0x00, 0x01, 0x38, 0x30, "1 Coin 2 Credits "	},
	{0x00, 0x01, 0x38, 0x28, "1 Coin 3 Credits "	},
	{0x00, 0x01, 0x38, 0x20, "1 Coin 4 Credits "	},
	{0x00, 0x01, 0x38, 0x18, "1 Coin 6 Credits "	},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x00, 0x01, 0x40, 0x00, "On"					},
	{0x00, 0x01, 0x40, 0x40, "Off"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x00, 0x01, 0x80, 0x80, "Off"					},
	{0x00, 0x01, 0x80, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x01, 0x01, 0x03, 0x03, "3"					},
	{0x01, 0x01, 0x03, 0x02, "4"					},
	{0x01, 0x01, 0x03, 0x01, "5"					},
	{0x01, 0x01, 0x03, 0x00, "7"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x01, 0x01, 0x04, 0x00, "Upright"				},
	{0x01, 0x01, 0x04, 0x04, "Cocktail"				},

	{0   , 0xfe, 0   ,    4, "F1 Up Point"			},
	{0x01, 0x01, 0x18, 0x18, "12"					},
	{0x01, 0x01, 0x18, 0x10, "16"					},
	{0x01, 0x01, 0x18, 0x08, "18"					},
	{0x01, 0x01, 0x18, 0x00, "20"					},

	{0   , 0xfe, 0   ,    2, "Difficulty"			},
	{0x01, 0x01, 0x20, 0x20, "Normal"				},
	{0x01, 0x01, 0x20, 0x00, "Difficult"			},

	{0   , 0xfe, 0   ,    2, "Version"				},
	{0x01, 0x01, 0x40, 0x00, "International"		},
	{0x01, 0x01, 0x40, 0x40, "Japan"				},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x01, 0x01, 0x80, 0x00, "No"					},
	{0x01, 0x01, 0x80, 0x80, "Yes"					},
};

STDDIPINFO(F1dream)

static void palette_write(INT32 offset)
{
	UINT16 data = *((UINT16 *)(DrvPalRAM + offset + 0x200));

	UINT8 r = (data >> 8) & 0x0f;
	UINT8 g = (data >> 4) & 0x0f;
	UINT8 b = (data >> 0) & 0x0f;

	r |= r << 4;
	g |= g << 4;
	b |= b << 4;

	DrvPalette[offset / 2] = BurnHighCol(r, g, b, 0);
}

static void mcu_write_port(INT32 port, UINT8 data)
{
	if (port >= 0x7f0 && port <= 0x7ff) {
		UINT16* ram16 = (UINT16*)Drv68KRAM;
		ram16[(0x3fe0 / 2) + (port & 0xf)] = (ram16[(0x3fe0 / 2) + (port & 0xf)] & 0xff00) | data;
	}

	switch (port) {
		case MCS51_PORT_P1: {
			*soundlatch = data;
		}
		break;

		case MCS51_PORT_P3: {
			if (((*last_port3 & 1) ^ (data & 1)) && (~data & 1)) {
				SekSetHALT(0, 0);
			}
			*last_port3 = data;
		}
		break;
	}
}

static UINT8 mcu_read_port(INT32 port)
{
	if (port >= 0x7f0 && port <= 0x7ff) {
		UINT16* ram16 = (UINT16*)Drv68KRAM;
		return ram16[(0x3fe0 / 2) + (port & 0xf)];
	}

	return 0xff;
}

static void mcu_sync()
{
	INT32 todo = ((double)SekTotalCycles() * (10000000/12) / 10000000) - mcs51TotalCycles();

	mcs51Run((todo > 0) ? todo : 0);
}

static void __fastcall tigeroad_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0xfe4000:
			*flipscreen =  data & 0x02;
			*bgcharbank = (data & 0x04) >> 2;
			*coin_lockout = (~data & 0x30) << 1;
		return;

		case 0xfe4002:
			if (nF1dream) {
				mcu_sync();
				mcs51_set_irq_line(MCS51_INT0_LINE, CPU_IRQSTATUS_HOLD);
				SekSetHALT(1);
			} else {
				*soundlatch = data;
			}
		return;
	}
}

static void __fastcall tigeroad_write_word(UINT32 address, UINT16 data)
{
	if (address >= 0xff8200 && address <= 0xff867f) {
		*((UINT16*)(DrvPalRAM + (address - 0xff8000))) = data;

		palette_write(address - 0xff8200);

		return;
	}

	switch (address)
	{
		case 0xfe8000:
		case 0xfe8002:
			*((UINT16 *)(DrvScrollRAM + (address & 2))) = data;
		return;

		case 0xfe800e:
			// watchdog
		return;
	}
}

static UINT8 __fastcall tigeroad_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0xfe4000:
		case 0xfe4001:
			return DrvInputs[0] >> ((~address & 1) << 3);

		case 0xfe4002:
		case 0xfe4003:
			return DrvInputs[1] >> ((~address & 1) << 3);

		case 0xfe4004:
		case 0xfe4005:
			return DrvDip[~address & 1];
	}

	return 0;
}

static UINT16 __fastcall tigeroad_read_word(UINT32 address)
{
	switch (address)
	{
		case 0xfe4000:
			return DrvInputs[0];

		case 0xfe4002:
			return DrvInputs[1];

		case 0xfe4004:
			return ((DrvDip[1] << 8) | DrvDip[0]);
	}

	return 0;
}

static void __fastcall tigeroad_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x8000:
		case 0x8001:
		case 0xa000:
		case 0xa001:
			BurnYM2203Write(address / 0xa000, address & 1, data);
		return;
	}
}

static void __fastcall tigeroad_sound_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x7f:
			*soundlatch2 = data;
		return;
	}
}

static UINT8 __fastcall tigeroad_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x8000:
		case 0xa000:
			return BurnYM2203Read(address / 0xa000, 0);

		case 0xe000:
			return *soundlatch;
	}

	return 0;
}

static void __fastcall tigeroad_sample_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x01:
			MSM5205ResetWrite(0, (data >> 7) & 1);
			MSM5205DataWrite(0, data);
			MSM5205VCLKWrite(0, 1);
			MSM5205VCLKWrite(0, 0);
		return;
	}
}

static UINT8 __fastcall tigeroad_sample_in(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
			return *soundlatch2;
	}

	return 0;
}

static void TigeroadIRQHandler(INT32, INT32 nStatus)
{
	ZetSetIRQLine(0, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

inline static INT32 DrvMSM5205SynchroniseStream(INT32 nSoundRate)
{
	return (INT64)((double)ZetTotalCycles() * nSoundRate / 3579545);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekReset(0);

	ZetOpen(0);
	ZetReset();
	BurnYM2203Reset();
	ZetClose();

	if (nF1dream) {
		mcs51_reset();
	}

	if (toramich) {
		ZetReset(1);

		MSM5205Reset();
	}

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM	= Next; Next += 0x040000;
	DrvZ80ROM	= Next; Next += 0x008000;
	DrvMCURom   = Next; Next += 0x010000;
	DrvSndROM	= Next; Next += 0x010000;

	DrvGfxROM0	= Next; Next += 0x020000;
	DrvGfxROM1	= Next; Next += 0x200000;
	DrvGfxROM2	= Next; Next += 0x100000;
	DrvGfxROM3	= Next; Next += 0x008000;

	DrvPalette	= (UINT32*)Next; Next += 0x0240 * sizeof(UINT32);

	DrvTransMask= Next; Next += 0x000010;

	AllRam		= Next;

	Drv68KRAM	= Next; Next += 0x004000;
	DrvPalRAM	= Next; Next += 0x000800;
	DrvVidRAM	= Next; Next += 0x000800;
	DrvSprRAM	= Next; Next += 0x001400;
	DrvSprBuf	= Next; Next += 0x000500;

	DrvZ80RAM	= Next; Next += 0x000800;

	DrvScrollRAM= Next; Next += 0x000004;

	soundlatch	= Next; Next += 0x000001;
	soundlatch2	= Next; Next += 0x000001;
	flipscreen	= Next; Next += 0x000001;
	bgcharbank	= Next; Next += 0x000001;
	coin_lockout= Next; Next += 0x000001;
	last_port3	= Next; Next += 0x000001;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[4]  = { ((0x20000 * 8) * 4) + 4, ((0x20000 * 8) * 4) + 0, 4, 0 };
	INT32 XOffs0[32] = { STEP4(0, 1), STEP4(8, 1), STEP4(64*8, 1), STEP4(64*8+8, 1),
						 STEP4(2*64*8, 1), STEP4(2*64*8+8, 1), STEP4(3*64*8, 1), STEP4(3*64*8+8, 1) };
	INT32 YOffs0[32] = { STEP32(0, 16) };

	INT32 Plane1[4]  = { (0x20000*8)*3, (0x20000*8)*2, (0x20000*8)*1, (0x20000*8)*0 };
	INT32 XOffs1[16] = { STEP8(0, 1), STEP8(16*8, 1) };
	INT32 YOffs1[16] = { STEP16(0, 8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x100000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x008000);

	GfxDecode(0x0800, 2,  8,  8, Plane0 + 2, XOffs0, YOffs0, 0x080, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x100000);

	GfxDecode(0x0800, 4, 32, 32, Plane0 + 0, XOffs0, YOffs0, 0x800, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x080000);

	GfxDecode(0x1000, 4, 16, 16, Plane1 + 0, XOffs1, YOffs1, 0x100, tmp, DrvGfxROM2);

	for (INT32 i = 0; i < 16; i++) {
		DrvTransMask[i] = (0xfe00 >> i) & 1;
	}

	BurnFree (tmp);

	return 0;
}

static INT32 DrvInit(INT32 (*pInitCallback)())
{
	BurnAllocMemIndex();

	{
		if (pInitCallback()) return 1;

		DrvGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(DrvSprRAM,		0xfe0800, 0xfe1bff, MAP_RAM);
	SekMapMemory(DrvVidRAM,		0xfec000, 0xfec7ff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		0xff8000, 0xff87ff, MAP_ROM);
	SekMapMemory(Drv68KRAM,		0xffc000, 0xffffff, MAP_RAM);
	SekSetWriteByteHandler(0,	tigeroad_write_byte);
	SekSetWriteWordHandler(0,	tigeroad_write_word);
	SekSetReadByteHandler(0,	tigeroad_read_byte);
	SekSetReadWordHandler(0,	tigeroad_read_word);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM, 0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM, 0xc000, 0xc7ff, MAP_RAM);
	ZetSetWriteHandler(tigeroad_sound_write);
	ZetSetReadHandler(tigeroad_sound_read);
	ZetSetOutHandler(tigeroad_sound_out);
	ZetClose();

	if (toramich) {
		ZetInit(1);
		ZetOpen(1);
		ZetMapMemory(DrvSndROM, 0x0000, 0xffff, MAP_ROM);
		ZetSetOutHandler(tigeroad_sample_out);
		ZetSetInHandler(tigeroad_sample_in);
		ZetClose();
	}

	BurnYM2203Init(2, 3579545, &TigeroadIRQHandler, 0);
	BurnTimerAttachZet(3579545);
	BurnYM2203SetAllRoutes(0, 0.25, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetAllRoutes(1, 0.25, BURN_SND_ROUTE_BOTH);
	if (!toramich) { // toramich has a low psg volume by default
		BurnYM2203SetPSGVolume(0, 0.11);
		BurnYM2203SetPSGVolume(1, 0.11);
	}

	if (toramich) {
		MSM5205Init(0, DrvMSM5205SynchroniseStream, 384000, NULL, MSM5205_SEX_4B, 1);
		MSM5205SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);
	}

	if (nF1dream) {
		mcs51_init();
		mcs51_set_program_data(DrvMCURom);
		mcs51_set_write_handler(mcu_write_port);
		mcs51_set_read_handler(mcu_read_port);
	}

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	BurnYM2203Exit();
	if (toramich) MSM5205Exit();

	GenericTilesExit();

	SekExit();
	ZetExit();
	if (nF1dream) {
		mcs51_exit();
	}
	BurnFree (AllMem);

	nF1dream = 0;
	toramich = 0;

	return 0;
}

static void draw_sprites()
{
	UINT16 *source = (UINT16*)DrvSprBuf;

	for (INT32 offs = (0x500 - 8) / 2; offs >= 0; offs -=4)
	{
		INT32 tile_number = BURN_ENDIAN_SWAP_INT16(source[offs + 0]);

		if (tile_number != 0xfff) {
			INT32 attr = BURN_ENDIAN_SWAP_INT16(source[offs + 1]);
			INT32 sy = BURN_ENDIAN_SWAP_INT16(source[offs + 2]) & 0x1ff;
			INT32 sx = BURN_ENDIAN_SWAP_INT16(source[offs + 3]) & 0x1ff;

			INT32 flipx = attr & 0x02;
			INT32 flipy = attr & 0x01;
			INT32 color = (attr >> 2) & 0x0f;

			if (sx > 0x100) sx -= 0x200;
			if (sy > 0x100) sy -= 0x200;

			if (*flipscreen)
			{
				sx = 240 - sx;
				sy = 240 - sy;
				flipx = !flipx;
				flipy = !flipy;
			}

			sy = (240 - sy) - 16;


			Draw16x16MaskTile(pTransDraw, tile_number, sx, sy, flipx, flipy, color, 4, 15, 0x100, DrvGfxROM2);
		}
	}
}

static void draw_32x32_mask_tile(INT32 sx, INT32 sy, INT32 code, INT32 color, INT32 flipx, INT32 flipy)
{
	UINT8 *src = DrvGfxROM1 + (code * 0x400) + (flipy * 0x3e0);
	UINT16 *dst;

	INT32 increment = flipy ? -32 : 32;

	for (INT32 y = 0; y < 32; y++, sy++)
	{
		if (sy >= nScreenHeight) break;

		if (sy >= 0)
		{
			dst = pTransDraw + (sy * nScreenWidth);

			if (flipx) {
				for (INT32 x = 31; x >= 0; x--)
				{
					if ((sx+x) < 0 || (sx+x) >= nScreenWidth) continue;

					if (DrvTransMask[src[x^0x1f]])
						dst[sx+x] = src[x^0x1f] | color;
				}
			} else {
				for (INT32 x = 0; x < 32; x++)
				{
					if ((sx+x) < 0 || (sx+x) >= nScreenWidth) continue;

					if (DrvTransMask[src[x]])
						dst[sx+x] = src[x] | color;
				}
			}
		}

		src += increment;
	}
}

static void draw_background(INT32 priority)
{
	INT32 scrollx = *((UINT16*)(DrvScrollRAM + 0));
	INT32 scrolly = *((UINT16*)(DrvScrollRAM + 2));

	scrollx &= 0xfff;

	scrolly = 0 - scrolly;
	scrolly -= 0x100;
	scrolly &= 0xfff;

	for (INT32 y = 0; y < 8+1; y++)
	{
		for (INT32 x = 0; x < 8+1; x++)
		{
			INT32 sx = (x + (scrollx >> 5)) & 0x7f;
			INT32 sy = (y + (scrolly >> 5)) & 0x7f;

			INT32 ofst = ((sx & 7) << 1) + (((127 - sy) & 7) << 4) + ((sx >> 3) << 7) + (((127 - sy) >> 3) << 11);

			INT32 attr = DrvGfxROM3[ofst + 1];
			INT32 prio = attr & 0x10;
			if (priority && !prio) continue;
	
			INT32 data = DrvGfxROM3[ofst];
			INT32 code = data + ((attr & 0xc0) << 2) + (*bgcharbank << 10);
			INT32 color = attr & 0x0f;
			INT32 flipx = attr & 0x20;
			INT32 flipy = 0;

			sx = (x << 5) - (scrollx & 0x1f);
			sy = (y << 5) - (scrolly & 0x1f);

			if (*flipscreen) {
				flipx ^= 0x20;
				flipy ^= 1;

				sx = 224 - sx;
				sy = 224 - sy;
			}

			if (!priority) {
				Draw32x32Tile(pTransDraw, code, sx, sy - 16, flipx, flipy, color, 4, 0, DrvGfxROM1);
			} else {
				draw_32x32_mask_tile(sx, sy - 16, code, color << 4, flipx, flipy);
			}
		}
	}
}

static void draw_text_layer()
{
	UINT16 *vram = (UINT16*)DrvVidRAM;

	for (INT32 offs = 0x40; offs < 0x3c0; offs++)
	{
		INT32 sx = (offs & 0x1f) << 3;
		INT32 sy = (offs >> 5) << 3;

		INT32 data = BURN_ENDIAN_SWAP_INT16(vram[offs]);
		INT32 attr = data >> 8;
		INT32 code = (data & 0xff) + ((attr & 0xc0) << 2) + ((attr & 0x20) << 5);
		if (code == 0x400) continue;

		INT32 color = attr & 0x0f;
		INT32 flipx = attr & 0x10;
		INT32 flipy = 0;

		if (*flipscreen) {
			sx ^= 0xf8;
			sy ^= 0xf8;
			flipx ^= 0x10;
			flipy ^= 0x01;
		}

		if (flipy) {
			if (flipx) {
				Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy - 16, color, 2, 3, 0x200, DrvGfxROM0);
			} else {
				Render8x8Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy - 16, color, 2, 3, 0x200, DrvGfxROM0);
			}
		} else {
			if (flipx) {
				Render8x8Tile_Mask_FlipX_Clip(pTransDraw, code, sx - 16, sy, color, 2, 3, 0x200, DrvGfxROM0);
			} else {
				Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy - 16, color, 2, 3, 0x200, DrvGfxROM0);
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x240 * 2; i+=2) {
			palette_write(i);
		}
		DrvRecalc = 0;
	}

	BurnTransferClear();

	if (nBurnLayer & 1) draw_background(0);
	if (nSpriteEnable & 1) draw_sprites();
	if (nBurnLayer & 2) draw_background(1);
	if (nBurnLayer & 4) draw_text_layer();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		DrvInputs[0] = 0xffff;
		DrvInputs[1] = 0xffff;

		for (INT32 i = 0; i < 16; i++)
		{
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}

		DrvInputs[1] |= *coin_lockout << 8;
	}

	INT32 nCyclesTotal[4] = { 10000000 / 60, 3579545 / 60, 3579545 / 60, 10000000 / 60 / 12 };
	INT32 nCyclesDone[4] = { 0, 0, 0, 0 };

	SekNewFrame();
	ZetNewFrame();
	if (nF1dream) mcs51NewFrame();

	INT32 nInterleave = 67*4; //268 for ez-irq of toramich second z80

	if (toramich) { MSM5205NewFrame(0, 10000000, nInterleave); }

	for (INT32 i = 0; i < nInterleave; i++) {
		SekOpen(0);
		CPU_RUN(0, Sek);
		if (i == (nInterleave - 1)) SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);
		SekClose();

		ZetOpen(0);
		CPU_RUN_TIMER(1);
		ZetClose();

		if (toramich) {
			ZetOpen(1);
			CPU_RUN(2, Zet);
			MSM5205Update();
			if ((i&3) == 3) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
			ZetClose();
		}

		if (nF1dream) {
			CPU_RUN_SYNCINT(3, mcs51);
		}
	}

	if (pBurnSoundOut) {
		ZetOpen(0);
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
		ZetClose();
		if (toramich) {
			ZetOpen(1);
			MSM5205Render(0, pBurnSoundOut, nBurnSoundLen);
			ZetClose();
		}
	}

	if (pBurnDraw) {
		DrvDraw();
	}

	memcpy (DrvSprBuf, DrvSprRAM, 0x500);

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
		SekScan(nAction);
		ZetScan(nAction);

		if (nF1dream) {
			mcs51_scan(nAction);
		}

		BurnYM2203Scan(nAction, pnMin);
		if (toramich) {
			MSM5205Scan(nAction, pnMin);
		}
	}

	return 0;
}


// Tiger Road (US) * ECT program roms *
/* N86614A-5 + N86614B-6 board combo - ECT program roms */

static struct BurnRomInfo tigeroadRomDesc[] = {
	{ "tre_02.6j",	0x20000, 0xc394add0, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "tre_04.6k",	0x20000, 0x73bfbf4a, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "tru_05.12k",	0x08000, 0xf9a7c9bf, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "tr_01.10d",	0x08000, 0x74a9f08c, 3 | BRF_GRA },           //  3 Character Tiles

	{ "tr-01a.3f",	0x20000, 0xa8aa2e59, 4 | BRF_GRA },           //  4 Background Tiles
	{ "tr-04a.3h",	0x20000, 0x8863a63c, 4 | BRF_GRA },           //  5
	{ "tr-02a.3j",	0x20000, 0x1a2c5f89, 4 | BRF_GRA },           //  6
	{ "tr-05.3l",	0x20000, 0x5bf453b3, 4 | BRF_GRA },           //  7
	{ "tr-03a.2f",	0x20000, 0x1e0537ea, 4 | BRF_GRA },           //  8
	{ "tr-06a.2h",	0x20000, 0xb636c23a, 4 | BRF_GRA },           //  9
	{ "tr-07a.2j",	0x20000, 0x5f907d4d, 4 | BRF_GRA },           // 10
	{ "tr_08.2l",	0x20000, 0xadee35e2, 4 | BRF_GRA },           // 11 EPROM

	{ "tr-09a.3b",	0x20000, 0x3d98ad1e, 5 | BRF_GRA },           // 12 Sprites
	{ "tr-10a.2b",	0x20000, 0x8f6f03d7, 5 | BRF_GRA },           // 13
	{ "tr-11a.3d",	0x20000, 0xcd9152e5, 5 | BRF_GRA },           // 14
	{ "tr-12a.2d",	0x20000, 0x7d8a99d0, 5 | BRF_GRA },           // 15

	{ "tr_13.7l",	0x08000, 0xa79be1eb, 6 | BRF_GRA },           // 16 Background Tilemaps

	{ "tr.9e",		0x00100, 0xec80ae36, 7 | BRF_GRA | BRF_OPT }, // 17 Priority Proms (unused)
};

STD_ROM_PICK(tigeroad)
STD_ROM_FN(tigeroad)

static INT32 TigeroadLoadRoms()
{
	if (BurnLoadRom(Drv68KROM + 0x000001,  0, 2)) return 1;
	if (BurnLoadRom(Drv68KROM + 0x000000,  1, 2)) return 1;

	if (BurnLoadRom(DrvZ80ROM + 0x000000,  2, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x00000,  3, 1)) return 1;

	for (INT32 i = 0; i < 8; i++) {
		if (BurnLoadRom(DrvGfxROM1 + i * 0x20000,  4 + i, 1)) return 1;
	}

	for (INT32 i = 0; i < 4; i++) {
		if (BurnLoadRom(DrvGfxROM2 + i * 0x20000, 12 + i, 1)) return 1;
	}

	if (BurnLoadRom(DrvGfxROM3 + 0x00000, 16, 1)) return 1;

	if (toramich) if (BurnLoadRom(DrvSndROM + 0x000000, 18, 1)) return 1;

	return 0;
}

static INT32 TigeroadInit()
{
	return DrvInit(TigeroadLoadRoms);
}

struct BurnDriver BurnDrvTigeroad = {
	"tigeroad", NULL, NULL, NULL, "1987",
	"Tiger Road (US)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_SCRFIGHT | GBF_PLATFORM, 0,
	NULL, tigeroadRomInfo, tigeroadRomName, NULL, NULL, NULL, NULL, TigeroadInputInfo, TigeroadDIPInfo,
	TigeroadInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x240,
	256, 224, 4, 3
};


// Tiger Road (US, Romstar license) * US ROMSTAR program roms *
/* N86614A-5 + N86614B-6 board combo - US ROMSTAR program roms */

static struct BurnRomInfo tigeroaduRomDesc[] = {
	{ "tru_02.6j",	0x20000, 0x8d283a95, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "tru_04.6k",	0x20000, 0x72e2ef20, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "tru_05.12k",	0x08000, 0xf9a7c9bf, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code
	
	{ "tr_01.10d",	0x08000, 0x74a9f08c, 3 | BRF_GRA },           //  3 Character Tiles

	{ "tr-01a.3f",	0x20000, 0xa8aa2e59, 4 | BRF_GRA },           //  4 Background Tiles
	{ "tr-04a.3h",	0x20000, 0x8863a63c, 4 | BRF_GRA },           //  5
	{ "tr-02a.3j",	0x20000, 0x1a2c5f89, 4 | BRF_GRA },           //  6
	{ "tr-05.3l",	0x20000, 0x5bf453b3, 4 | BRF_GRA },           //  7
	{ "tr-03a.2f",	0x20000, 0x1e0537ea, 4 | BRF_GRA },           //  8
	{ "tr-06a.2h",	0x20000, 0xb636c23a, 4 | BRF_GRA },           //  9
	{ "tr-07a.2j",	0x20000, 0x5f907d4d, 4 | BRF_GRA },           // 10
	{ "tr_08.2l",	0x20000, 0xadee35e2, 4 | BRF_GRA },           // 11 EPROM

	{ "tr-09a.3b",	0x20000, 0x3d98ad1e, 5 | BRF_GRA },           // 12 Sprites
	{ "tr-10a.2b",	0x20000, 0x8f6f03d7, 5 | BRF_GRA },           // 13
	{ "tr-11a.3d",	0x20000, 0xcd9152e5, 5 | BRF_GRA },           // 14
	{ "tr-12a.2d",	0x20000, 0x7d8a99d0, 5 | BRF_GRA },           // 15

	{ "tr_13.7l",	0x08000, 0xa79be1eb, 6 | BRF_GRA },           // 16 Background Tilemaps

	{ "tr.9e",		0x00100, 0xec80ae36, 7 | BRF_GRA | BRF_OPT }, // 17 Priority Proms (unused)
};

STD_ROM_PICK(tigeroadu)
STD_ROM_FN(tigeroadu)

struct BurnDriver BurnDrvTigeroadu = {
	"tigeroadu", "tigeroad", NULL, NULL, "1987",
	"Tiger Road (US, Romstar license)\0", NULL, "Capcom (Romstar license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_SCRFIGHT | GBF_PLATFORM, 0,
	NULL, tigeroaduRomInfo, tigeroaduRomName, NULL, NULL, NULL, NULL, TigeroadInputInfo, TigeroadDIPInfo,
	TigeroadInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x240,
	256, 224, 4, 3
};


// Tora e no Michi (Japan)
/* N86614A-5 + N86614B-6 board combo */

static struct BurnRomInfo toramichRomDesc[] = {
	{ "tr_02.6j",	0x20000, 0xb54723b1, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "tr_04.6k",	0x20000, 0xab432479, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "tr_05.12k",	0x08000, 0x3ebe6e62, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "tr_01.10d",	0x08000, 0x74a9f08c, 3 | BRF_GRA },           //  3 Character Tiles

	{ "tr-01a.3f",	0x20000, 0xa8aa2e59, 4 | BRF_GRA },           //  4 Background Tiles
	{ "tr-04a.3h",	0x20000, 0x8863a63c, 4 | BRF_GRA },           //  5
	{ "tr-02a.3j",	0x20000, 0x1a2c5f89, 4 | BRF_GRA },           //  6
	{ "tr-05.3l",	0x20000, 0x5bf453b3, 4 | BRF_GRA },           //  7
	{ "tr-03a.2f",	0x20000, 0x1e0537ea, 4 | BRF_GRA },           //  8
	{ "tr-06a.2h",	0x20000, 0xb636c23a, 4 | BRF_GRA },           //  9
	{ "tr-07a.2j",	0x20000, 0x5f907d4d, 4 | BRF_GRA },           // 10
	{ "tr_08.2l",	0x20000, 0xadee35e2, 4 | BRF_GRA },           // 11 EPROM

	{ "tr-09a.3b",	0x20000, 0x3d98ad1e, 5 | BRF_GRA },           // 12 Sprites
	{ "tr-10a.2b",	0x20000, 0x8f6f03d7, 5 | BRF_GRA },           // 13
	{ "tr-11a.3d",	0x20000, 0xcd9152e5, 5 | BRF_GRA },           // 14
	{ "tr-12a.2d",	0x20000, 0x7d8a99d0, 5 | BRF_GRA },           // 15

	{ "tr_13.7l",	0x08000, 0xa79be1eb, 6 | BRF_GRA },           // 16 Background Tilemaps

	{ "tr.9e",		0x00100, 0xec80ae36, 7 | BRF_GRA | BRF_OPT }, // 17 Priority Proms (unused)

	{ "tr_03.11j",	0x10000, 0xea1807ef, 8 | BRF_PRG | BRF_ESS }, // 18 Sample Z80 Code
};

STD_ROM_PICK(toramich)
STD_ROM_FN(toramich)

static INT32 ToramichInit()
{
	toramich = 1;

	return DrvInit(TigeroadLoadRoms);
}

struct BurnDriver BurnDrvToramich = {
	"toramich", "tigeroad", NULL, NULL, "1987",
	"Tora e no Michi (Japan)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_SCRFIGHT | GBF_PLATFORM, 0,
	NULL, toramichRomInfo, toramichRomName, NULL, NULL, NULL, NULL, TigeroadInputInfo, ToramichDIPInfo,
	ToramichInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x240,
	256, 224, 4, 3
};


// Tiger Road (US bootleg)

static struct BurnRomInfo tigeroadbRomDesc[] = {
	{ "cpu.ic18",		0x10000, 0x14c87e07, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "cpu.ic5",		0x10000, 0x0904254c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "cpu.ic19",		0x10000, 0xcedb1f46, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "cpu.ic6",		0x10000, 0xe117f0b1, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "cpu.ic12",		0x08000, 0xf9a7c9bf, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "cpu.ic60",		0x08000, 0x74a9f08c, 3 | BRF_GRA },           //  5 Character Tiles

	{ "gfx.ic84",		0x10000, 0x3db68b96, 4 | BRF_GRA },           //  6 Background Tiles
	{ "gfx.ic82",		0x10000, 0xa12fa19d, 4 | BRF_GRA },           //  7
	{ "gfx.ic115",		0x10000, 0xc9c396aa, 4 | BRF_GRA },           //  8
	{ "gfx.ic113",		0x10000, 0x6bfc90a4, 4 | BRF_GRA },           //  9
	{ "gfx.ic132",		0x10000, 0xdccf34bb, 4 | BRF_GRA },           // 10
	{ "gfx.ic130",		0x10000, 0xa1cee4cd, 4 | BRF_GRA },           // 11
	{ "gfx.ic171",		0x10000, 0x7266e3ad, 4 | BRF_GRA },           // 12
	{ "gfx.ic169",		0x10000, 0x5ec867a6, 4 | BRF_GRA },           // 13
	{ "gfx.ic83",		0x10000, 0x95c69541, 4 | BRF_GRA },           // 14
	{ "gfx.ic81",		0x10000, 0xecb67157, 4 | BRF_GRA },           // 15
	{ "gfx.ic114",		0x10000, 0x53f24910, 4 | BRF_GRA },           // 16
	{ "gfx.ic112",		0x10000, 0x5a309d8b, 4 | BRF_GRA },           // 17
	{ "gfx.ic131",		0x10000, 0x710feda8, 4 | BRF_GRA },           // 18
	{ "gfx.ic129",		0x10000, 0x24b08a7e, 4 | BRF_GRA },           // 19
	{ "gfx.ic170",		0x10000, 0x3f7539cc, 4 | BRF_GRA },           // 20
	{ "gfx.ic168",		0x10000, 0xe2e053cb, 4 | BRF_GRA },           // 21

	{ "gfx.ic51",		0x10000, 0x298c40a7, 5 | BRF_GRA },           // 22 Sprites
	{ "gfx.ic52",		0x10000, 0x39866869, 5 | BRF_GRA },           // 23 
	{ "gfx.ic4",		0x10000, 0x4a89c11d, 5 | BRF_GRA },           // 24
	{ "gfx.ic5",		0x10000, 0xb0b94294, 5 | BRF_GRA },           // 25
	{ "gfx.ic49",		0x10000, 0x7db7f0f1, 5 | BRF_GRA },           // 26
	{ "gfx.ic50",		0x10000, 0xf48f94e1, 5 | BRF_GRA },           // 27
	{ "gfx.ic2",		0x10000, 0xa0b4615c, 5 | BRF_GRA },           // 28
	{ "gfx.ic3",		0x10000, 0xf956392e, 5 | BRF_GRA },           // 29

	{ "gfx.ic175",		0x08000, 0xa79be1eb, 6 | BRF_GRA },           // 30 Background Tilemaps

	{ "82s129.ic74",	0x00100, 0xec80ae36, 7 | BRF_GRA | BRF_OPT }, // 31 Priority Proms (unused)
};

STD_ROM_PICK(tigeroadb)
STD_ROM_FN(tigeroadb)

static INT32 TigeroadbLoadRoms()
{
	if (BurnLoadRom(Drv68KROM + 0x000001,  0, 2)) return 1;
	if (BurnLoadRom(Drv68KROM + 0x000000,  1, 2)) return 1;
	if (BurnLoadRom(Drv68KROM + 0x020001,  2, 2)) return 1;
	if (BurnLoadRom(Drv68KROM + 0x020000,  3, 2)) return 1;

	if (BurnLoadRom(DrvZ80ROM + 0x000000,  4, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x00000,  5, 1)) return 1;

	for (INT32 i = 0; i < 16; i++) {
		if (BurnLoadRom(DrvGfxROM1 + i * 0x10000,  6 + i, 1)) return 1;
	}

	if (BurnLoadRom(DrvGfxROM2 + 0x60000, 22, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x40000, 23, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x20000, 24, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x00000, 25, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x70000, 26, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x50000, 27, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x30000, 28, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x10000, 29, 1)) return 1;
	
	if (BurnLoadRom(DrvGfxROM3 + 0x00000, 30, 1)) return 1;

	for (INT32 i = 0; i < 0x80000; i++) {
    	DrvGfxROM2[i] = BITSWAP08(DrvGfxROM2[i], 4, 5, 6, 7, 0, 1, 2, 3);
    }

	return 0;
}

static INT32 TigeroadbInit()
{
	return DrvInit(TigeroadbLoadRoms);
}

struct BurnDriver BurnDrvTigeroadb = {
	"tigeroadb", "tigeroad", NULL, NULL, "1987",
	"Tiger Road (US bootleg, set 1)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_SCRFIGHT | GBF_PLATFORM, 0,
	NULL, tigeroadbRomInfo, tigeroadbRomName, NULL, NULL, NULL, NULL, TigeroadInputInfo, TigeroadDIPInfo,
	TigeroadbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x240,
	256, 224, 4, 3
};


// Tiger Road (US bootleg, set 2)

static struct BurnRomInfo tigeroadb2RomDesc[] = {
	{ "2.bin",		0x10000, 0x14c87e07, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "5.bin",		0x10000, 0x0904254c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.bin",		0x10000, 0xcedb1f46, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "6.bin",		0x10000, 0xe117f0b1, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "4.bin",		0x08000, 0xf9a7c9bf, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "1.bin",		0x08000, 0x74a9f08c, 3 | BRF_GRA },           //  5 Character Tiles

	{ "15.bin",		0x10000, 0x3db68b96, 4 | BRF_GRA },           //  6 Background Tiles
	{ "17.bin",		0x10000, 0xa12fa19d, 4 | BRF_GRA },           //  7 
	{ "19.bin",		0x10000, 0xc9c396aa, 4 | BRF_GRA },           //  8 
	{ "21.bin",		0x10000, 0x6bfc90a4, 4 | BRF_GRA },           //  9 
	{ "23.bin",		0x10000, 0xdccf34bb, 4 | BRF_GRA },           // 10
	{ "25.bin",		0x10000, 0xa1cee4cd, 4 | BRF_GRA },           // 11
	{ "28.bin",		0x10000, 0x7266e3ad, 4 | BRF_GRA },           // 12
	{ "30.bin",		0x10000, 0x5ec867a6, 4 | BRF_GRA },           // 13
	{ "16.bin",		0x10000, 0x95c69541, 4 | BRF_GRA },           // 14 
	{ "18.bin",		0x10000, 0xecb67157, 4 | BRF_GRA },           // 15  
	{ "20.bin",		0x10000, 0x53f24910, 4 | BRF_GRA },           // 16 
	{ "22.bin",		0x10000, 0x5a309d8b, 4 | BRF_GRA },           // 17 
	{ "24.bin",		0x10000, 0x710feda8, 4 | BRF_GRA },           // 18
	{ "26.bin",		0x10000, 0x24b08a7e, 4 | BRF_GRA },           // 19
	{ "29.bin",		0x10000, 0x3f7539cc, 4 | BRF_GRA },           // 20
	{ "31.bin",		0x10000, 0xe2e053cb, 4 | BRF_GRA },           // 21

	{ "7.bin",		0x10000, 0x12437511, 5 | BRF_GRA },           // 22 Sprites
	{ "9.bin",		0x10000, 0x9207b4eb, 5 | BRF_GRA },           // 23 
	{ "8.bin",		0x10000, 0x1e8eb4be, 5 | BRF_GRA },           // 24
	{ "10.bin",		0x10000, 0x486a7528, 5 | BRF_GRA },           // 25
	{ "11.bin",		0x10000, 0xffb2c34c, 5 | BRF_GRA },           // 26
	{ "13.bin",		0x10000, 0x312a40e9, 5 | BRF_GRA },           // 27
	{ "12.bin",		0x10000, 0xe5039e3b, 5 | BRF_GRA },           // 28
	{ "14.bin",		0x10000, 0x5e564954, 5 | BRF_GRA },           // 29

	{ "27.bin",		0x08000, 0xa79be1eb, 6 | BRF_GRA },           // 30 Background Tilemaps

	{ "trprom.bin",	0x00100, 0xec80ae36, 7 | BRF_GRA | BRF_OPT }, // 31 Priority Proms (unused)
};

STD_ROM_PICK(tigeroadb2)
STD_ROM_FN(tigeroadb2)

static INT32 Tigeroadb2LoadRoms()
{
	if (BurnLoadRom(Drv68KROM + 0x000001,  0, 2)) return 1;
	if (BurnLoadRom(Drv68KROM + 0x000000,  1, 2)) return 1;
	if (BurnLoadRom(Drv68KROM + 0x020001,  2, 2)) return 1;
	if (BurnLoadRom(Drv68KROM + 0x020000,  3, 2)) return 1;

	if (BurnLoadRom(DrvZ80ROM + 0x000000,  4, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x00000,  5, 1)) return 1;

	for (INT32 i = 0; i < 16; i++) {
		if (BurnLoadRom(DrvGfxROM1 + i * 0x10000,  6 + i, 1)) return 1;
	}

	for (INT32 i = 0; i < 8; i++) {
		if (BurnLoadRom(DrvGfxROM2 + i * 0x10000, 22 + i, 1)) return 1;
	}

	if (BurnLoadRom(DrvGfxROM3 + 0x00000, 30, 1)) return 1;

	return 0;
}

static INT32 Tigeroadb2Init()
{
	return DrvInit(Tigeroadb2LoadRoms);
}

struct BurnDriver BurnDrvTigeroadb2 = {
	"tigeroadb2", "tigeroad", NULL, NULL, "1987",
	"Tiger Road (US bootleg, set 2)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_SCRFIGHT | GBF_PLATFORM, 0,
	NULL, tigeroadb2RomInfo, tigeroadb2RomName, NULL, NULL, NULL, NULL, TigeroadInputInfo, TigeroadDIPInfo,
	Tigeroadb2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x240,
	256, 224, 4, 3
};


// F-1 Dream
/* N86614A-5 + N86614B-6 board combo */

static struct BurnRomInfo f1dreamRomDesc[] = {
	{ "f1_02.6j",	0x20000, 0x3c2ec697, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "f1_03.6k",	0x20000, 0x85ebad91, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "f1_04.12k",	0x08000, 0x4b9a7524, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "f1_01.10d",	0x08000, 0x361caf00, 3 | BRF_GRA },           //  3 Character Tiles

	{ "f1_12.3f",	0x10000, 0xbc13e43c, 4 | BRF_GRA },           //  4 Background Tiles
	{ "f1_10.1f",	0x10000, 0xf7617ad9, 4 | BRF_GRA },           //  5
	{ "f1_14.3h",	0x10000, 0xe33cd438, 4 | BRF_GRA },           //  6
	{ "f1_11.2f",	0x10000, 0x4aa49cd7, 4 | BRF_GRA },           //  7
	{ "f1_09.17f",	0x10000, 0xca622155, 4 | BRF_GRA },           //  8
	{ "f1_13.2h",	0x10000, 0x2a63961e, 4 | BRF_GRA },           //  9

	{ "f1_06.3b",	0x10000, 0x5e54e391, 5 | BRF_GRA },           // 10 Sprites
	{ "f1_05.2b",	0x10000, 0xcdd119fd, 5 | BRF_GRA },           // 11
	{ "f1_08.3d",	0x10000, 0x811f2e22, 5 | BRF_GRA },           // 12
	{ "f1_07.2d",	0x10000, 0xaa9a1233, 5 | BRF_GRA },           // 13

	{ "f1_15.7l",	0x08000, 0x978758b7, 6 | BRF_GRA },           // 14 Background Tilemaps

	{ "tr.9e",		0x00100, 0xec80ae36, 7 | BRF_GRA | BRF_OPT }, // 15 Priority Proms (unused)
	
	{ "f1.9j",   	0x01000, 0xc8e6075c, 0 | BRF_PRG | BRF_ESS }, // 16 Protection MCU
};

STD_ROM_PICK(f1dream)
STD_ROM_FN(f1dream)

static INT32 F1dreamLoadRoms()
{
	if (BurnLoadRom(Drv68KROM + 0x000001,  0, 2)) return 1;
	if (BurnLoadRom(Drv68KROM + 0x000000,  1, 2)) return 1;

	if (BurnLoadRom(DrvZ80ROM + 0x000000,  2, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x00000,  3, 1)) return 1;

	for (INT32 i = 0; i < 3; i++) {
		if (BurnLoadRom(DrvGfxROM1 + 0x00000 + i * 0x10000,  4 + i, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x80000 + i * 0x10000,  7 + i, 1)) return 1;
	}

	for (INT32 i = 0; i < 4; i++) {
		if (BurnLoadRom(DrvGfxROM2 + i * 0x20000, 10 + i, 1)) return 1;
	}

	if (BurnLoadRom(DrvGfxROM3 + 0x00000, 14, 1)) return 1;

	if (BurnLoadRom(DrvMCURom  + 0x00000, 16, 1)) return 1;

	return 0;
}

static INT32 F1dreamInit()
{
	nF1dream = 1;

	return DrvInit(F1dreamLoadRoms);
}

struct BurnDriver BurnDrvF1dream = {
	"f1dream", NULL, NULL, NULL, "1988",
	"F-1 Dream\0", NULL, "Capcom (Romstar license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_RACING, 0,
	NULL, f1dreamRomInfo, f1dreamRomName, NULL, NULL, NULL, NULL, TigeroadInputInfo, F1dreamDIPInfo,
	F1dreamInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x240,
	256, 224, 4, 3
};


// F-1 Dream (bootleg, set 1)

static struct BurnRomInfo f1dreambRomDesc[] = {
	{ "f1d_04.bin",	0x10000, 0x903febad, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "f1d_05.bin",	0x10000, 0x666fa2a7, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "f1d_02.bin",	0x10000, 0x98973c4c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "f1d_03.bin",	0x10000, 0x3d21c78a, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "12k_04.bin",	0x08000, 0x4b9a7524, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "10d_01.bin",	0x08000, 0x361caf00, 3 | BRF_GRA },           //  5 Character Tiles

	{ "03f_12.bin",	0x10000, 0xbc13e43c, 4 | BRF_GRA },           //  6 Background Tiles
	{ "01f_10.bin",	0x10000, 0xf7617ad9, 4 | BRF_GRA },           //  7
	{ "03h_14.bin",	0x10000, 0xe33cd438, 4 | BRF_GRA },           //  8
	{ "02f_11.bin",	0x10000, 0x4aa49cd7, 4 | BRF_GRA },           //  9
	{ "17f_09.bin",	0x10000, 0xca622155, 4 | BRF_GRA },           // 10
	{ "02h_13.bin",	0x10000, 0x2a63961e, 4 | BRF_GRA },           // 11

	{ "03b_06.bin",	0x10000, 0x5e54e391, 5 | BRF_GRA },           // 12 Sprites
	{ "02b_05.bin",	0x10000, 0xcdd119fd, 5 | BRF_GRA },           // 13
	{ "03d_08.bin",	0x10000, 0x811f2e22, 5 | BRF_GRA },           // 14
	{ "02d_07.bin",	0x10000, 0xaa9a1233, 5 | BRF_GRA },           // 15

	{ "07l_15.bin",	0x08000, 0x978758b7, 6 | BRF_GRA },           // 16 Background Tilemaps

	{ "09e_tr.bin",	0x00100, 0xec80ae36, 7 | BRF_GRA | BRF_OPT }, // 17 Priority Proms (unused)
};

STD_ROM_PICK(f1dreamb)
STD_ROM_FN(f1dreamb)

static INT32 F1dreambLoadRoms()
{
	if (BurnLoadRom(Drv68KROM + 0x000001,  0, 2)) return 1;
	if (BurnLoadRom(Drv68KROM + 0x000000,  1, 2)) return 1;
	if (BurnLoadRom(Drv68KROM + 0x020001,  2, 2)) return 1;
	if (BurnLoadRom(Drv68KROM + 0x020000,  3, 2)) return 1;

	if (BurnLoadRom(DrvZ80ROM + 0x000000,  4, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x00000,  5, 1)) return 1;

	for (INT32 i = 0; i < 3; i++) {
		if (BurnLoadRom(DrvGfxROM1 + 0x00000 + i * 0x10000,  6 + i, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x80000 + i * 0x10000,  9 + i, 1)) return 1;
	}

	for (INT32 i = 0; i < 4; i++) {
		if (BurnLoadRom(DrvGfxROM2 + i * 0x20000, 12 + i, 1)) return 1;
	}

	if (BurnLoadRom(DrvGfxROM3 + 0x00000, 16, 1)) return 1;

	return 0;
}

static INT32 F1dreambInit()
{
	return DrvInit(F1dreambLoadRoms);
}

struct BurnDriver BurnDrvF1dreamb = {
	"f1dreamb", "f1dream", NULL, NULL, "1988",
	"F-1 Dream (bootleg, set 1)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_RACING, 0,
	NULL, f1dreambRomInfo, f1dreambRomName, NULL, NULL, NULL, NULL, TigeroadInputInfo, F1dreamDIPInfo,
	F1dreambInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x240,
	256, 224, 4, 3
};


// F-1 Dream (bootleg, set 2)

static struct BurnRomInfo f1dreambaRomDesc[] = {
	{ "3.bin",		0x10000, 0xbdfbbbec, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "5.bin",		0x10000, 0xcc47cfb2, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "2.bin",		0x10000, 0xa34f63fb, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.bin",		0x10000, 0xf98db083, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "12k_04.bin",	0x08000, 0x4b9a7524, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "10d_01.bin",	0x08000, 0x361caf00, 3 | BRF_GRA },           //  5 Character Tiles

	{ "03f_12.bin",	0x10000, 0xbc13e43c, 4 | BRF_GRA },           //  6 Background Tiles
	{ "01f_10.bin",	0x10000, 0xf7617ad9, 4 | BRF_GRA },           //  7
	{ "03h_14.bin",	0x10000, 0xe33cd438, 4 | BRF_GRA },           //  8
	{ "02f_11.bin",	0x10000, 0x4aa49cd7, 4 | BRF_GRA },           //  9
	{ "17f_09.bin",	0x10000, 0xca622155, 4 | BRF_GRA },           // 10
	{ "02h_13.bin",	0x10000, 0x2a63961e, 4 | BRF_GRA },           // 11

	{ "03b_06.bin",	0x10000, 0x5e54e391, 5 | BRF_GRA },           // 12 Sprites
	{ "02b_05.bin",	0x10000, 0xcdd119fd, 5 | BRF_GRA },           // 13
	{ "03d_08.bin",	0x10000, 0x811f2e22, 5 | BRF_GRA },           // 14
	{ "02d_07.bin",	0x10000, 0xaa9a1233, 5 | BRF_GRA },           // 15

	{ "07l_15.bin",	0x08000, 0x978758b7, 6 | BRF_GRA },           // 16 Background Tilemaps

	{ "09e_tr.bin",	0x00100, 0xec80ae36, 7 | BRF_GRA | BRF_OPT }, // 17 Priority Proms (unused)
};

STD_ROM_PICK(f1dreamba)
STD_ROM_FN(f1dreamba)

struct BurnDriver BurnDrvF1dreamba = {
	"f1dreamba", "f1dream", NULL, NULL, "1988",
	"F-1 Dream (bootleg, set 2)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_RACING, 0,
	NULL, f1dreambaRomInfo, f1dreambaRomName, NULL, NULL, NULL, NULL, TigeroadInputInfo, F1dreamDIPInfo,
	F1dreambInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x240,
	256, 224, 4, 3
};
