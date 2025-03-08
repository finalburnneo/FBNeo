// FB Alpha Chelnov / Karnov / Wonder Planet driver module
// Based on MAME driver by Bryan McPhail

// Karnov: Game has a bug (weird!) - dink
// Explanation: Karnov level #3(?) - a snakey-thing that snakes through
// the air; if you die while this thing is still alive, the sound
// will never stop.  Even after game over.

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "m6502_intf.h"
#include "mcs51.h"
#include "burn_ym2203.h"
#include "burn_ym3526.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *Drv68KROM;
static UINT8 *Drv6502ROM;
static UINT8 *DrvMCUROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvColPROM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvPfRAM;
static UINT8 *Drv6502RAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprBuf;

static UINT32 *Palette;
static UINT32 *DrvPalette;
static UINT8  DrvRecalc;

static UINT16 *DrvScroll;

static UINT8 *soundlatch;
static UINT8 *flipscreen;

// i8751 MCU emulation (karnov, chelnov)
static UINT16 i8751RetVal;
static UINT16 i8751Command;
static UINT8 i8751PortData[4] = { 0, 0, 0, 0 };

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT16 DrvInput[3];
static UINT8 DrvDip[2];
static UINT8 DrvReset;

#ifdef BUILD_A68K
static bool bUseAsm68KCoreOldValue = false;
#endif

static INT32 vblank;

enum { KARNOV = 0, CHELNOV, WNDRPLNT };
static INT32 is_game;

static INT32 nCyclesExtra[3];

static struct BurnInputInfo KarnovInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 5,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 6,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 7,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDip + 0,		"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDip + 1,		"dip"		},
};

STDINPUTINFO(Karnov)

static struct BurnDIPInfo KarnovDIPList[]=
{
	{0x14, 0xff, 0xff, 0xaf, NULL					},
	{0x15, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    4, "Coin A"				},
	{0x14, 0x01, 0x03, 0x00, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0x03, 0x01, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x14, 0x01, 0x0c, 0x00, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0x0c, 0x04, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x14, 0x01, 0x20, 0x20, "Off"					},
	{0x14, 0x01, 0x20, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x14, 0x01, 0x40, 0x00, "Upright"				},
	{0x14, 0x01, 0x40, 0x40, "Cocktail"				},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x15, 0x01, 0x03, 0x01, "1"					},
	{0x15, 0x01, 0x03, 0x03, "3"					},
	{0x15, 0x01, 0x03, 0x02, "5"					},
	{0x15, 0x01, 0x03, 0x00, "Infinite (Cheat)"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x15, 0x01, 0x0c, 0x0c, "50 K"					},
	{0x15, 0x01, 0x0c, 0x08, "70 K"					},
	{0x15, 0x01, 0x0c, 0x04, "90 K"					},
	{0x15, 0x01, 0x0c, 0x00, "100 K"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x15, 0x01, 0x30, 0x20, "Easy"					},
	{0x15, 0x01, 0x30, 0x30, "Normal"				},
	{0x15, 0x01, 0x30, 0x10, "Hard"					},
	{0x15, 0x01, 0x30, 0x00, "Very Hard"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x15, 0x01, 0x40, 0x00, "Off"					},
	{0x15, 0x01, 0x40, 0x40, "On"					},

	{0   , 0xfe, 0   ,    2, "Timer Speed"			},
	{0x15, 0x01, 0x80, 0x80, "Normal"				},
	{0x15, 0x01, 0x80, 0x00, "Fast"					},
};

STDDIPINFO(Karnov)

static struct BurnDIPInfo WndrplntDIPList[]=
{
	{0x14, 0xff, 0xff, 0x6f, NULL					},
	{0x15, 0xff, 0xff, 0xd3, NULL					},

	{0   , 0xfe, 0   ,    4, "Coin A"				},
	{0x14, 0x01, 0x03, 0x00, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0x03, 0x01, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x14, 0x01, 0x0c, 0x00, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0x0c, 0x04, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x14, 0x01, 0x20, 0x00, "Off"					},
	{0x14, 0x01, 0x20, 0x20, "On"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x14, 0x01, 0x40, 0x40, "Off"					},
	{0x14, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x14, 0x01, 0x80, 0x00, "Upright"				},
	{0x14, 0x01, 0x80, 0x80, "Cocktail"				},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x15, 0x01, 0x03, 0x01, "1"					},
	{0x15, 0x01, 0x03, 0x03, "3"					},
	{0x15, 0x01, 0x03, 0x02, "5"					},
	{0x15, 0x01, 0x03, 0x00, "Infinite (Cheat)"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x15, 0x01, 0x10, 0x00, "No"					},
	{0x15, 0x01, 0x10, 0x10, "Yes"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x15, 0x01, 0xc0, 0x80, "Easy"					},
	{0x15, 0x01, 0xc0, 0xc0, "Normal"				},
	{0x15, 0x01, 0xc0, 0x40, "Hard"					},
	{0x15, 0x01, 0xc0, 0x00, "Hardest"				},
};

STDDIPINFO(Wndrplnt)

static struct BurnDIPInfo ChelnovDIPList[]=
{
	{0x14, 0xff, 0xff, 0x6f, NULL					},
	{0x15, 0xff, 0xff, 0xdf, NULL					},

	{0   , 0xfe, 0   ,    4, "Coin A"				},
	{0x14, 0x01, 0x03, 0x03, "1 Coin 2 Credits"		},
	{0x14, 0x01, 0x03, 0x02, "1 Coin 3 Credits"		},
	{0x14, 0x01, 0x03, 0x01, "1 Coin 4 Credits"		},
	{0x14, 0x01, 0x03, 0x00, "1 Coin 6 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x14, 0x01, 0x0c, 0x00, "4 Coins 1 Credits"	},
	{0x14, 0x01, 0x0c, 0x04, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0x0c, 0x08, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x14, 0x01, 0x20, 0x00, "Off"					},
	{0x14, 0x01, 0x20, 0x20, "On"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x14, 0x01, 0x40, 0x40, "Off"					},
	{0x14, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x14, 0x01, 0x80, 0x00, "Upright"				},
	{0x14, 0x01, 0x80, 0x80, "Cocktail"				},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x15, 0x01, 0x03, 0x01, "1"					},
	{0x15, 0x01, 0x03, 0x03, "3"					},
	{0x15, 0x01, 0x03, 0x02, "5"					},
	{0x15, 0x01, 0x03, 0x00, "Infinite (Cheat)"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x15, 0x01, 0x0c, 0x08, "Easy"					},
	{0x15, 0x01, 0x0c, 0x0c, "Normal"				},
	{0x15, 0x01, 0x0c, 0x04, "Hard"					},
	{0x15, 0x01, 0x0c, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x15, 0x01, 0x10, 0x00, "No"					},
	{0x15, 0x01, 0x10, 0x10, "Yes"					},

	{0   , 0xfe, 0   ,    2, "Freeze"				},
	{0x15, 0x01, 0x40, 0x40, "Off"					},
	{0x15, 0x01, 0x40, 0x00, "On"					},
};

STDDIPINFO(Chelnov)

static struct BurnDIPInfo ChelnovuDIPList[]=
{
	{0x14, 0xff, 0xff, 0x6f, NULL					},
	{0x15, 0xff, 0xff, 0xdf, NULL					},

	{0   , 0xfe, 0   ,    4, "Coin A"				},
	{0x14, 0x01, 0x03, 0x00, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0x03, 0x01, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x14, 0x01, 0x0c, 0x00, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0x0c, 0x04, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x14, 0x01, 0x20, 0x00, "Off"					},
	{0x14, 0x01, 0x20, 0x20, "On"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x14, 0x01, 0x40, 0x40, "Off"					},
	{0x14, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x14, 0x01, 0x80, 0x00, "Upright"				},
	{0x14, 0x01, 0x80, 0x80, "Cocktail"				},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x15, 0x01, 0x03, 0x01, "1"					},
	{0x15, 0x01, 0x03, 0x03, "3"					},
	{0x15, 0x01, 0x03, 0x02, "5"					},
	{0x15, 0x01, 0x03, 0x00, "Infinite (Cheat)"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x15, 0x01, 0x0c, 0x08, "Easy"					},
	{0x15, 0x01, 0x0c, 0x0c, "Normal"				},
	{0x15, 0x01, 0x0c, 0x04, "Hard"					},
	{0x15, 0x01, 0x0c, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x15, 0x01, 0x10, 0x00, "No"					},
	{0x15, 0x01, 0x10, 0x10, "Yes"					},

	{0   , 0xfe, 0   ,    2, "Freeze"				},
	{0x15, 0x01, 0x40, 0x40, "Off"					},
	{0x15, 0x01, 0x40, 0x00, "On"					},
};

STDDIPINFO(Chelnovu)

// i8751 MCU (karnov, chelnov)
static void DrvMCUReset(); // forward
static void DrvMCUSync(); // ""

static UINT8 mcu_read_port(INT32 port)
{
	if (!(port >= MCS51_PORT_P0 && port <= MCS51_PORT_P3))
		return 0xff;

	switch (port & 0x3) {
		case 0: return i8751PortData[0];
		case 1: return i8751PortData[1];
		case 3: return DrvInput[2];
	}

	return 0xff;
}

static void mcu_write_port(INT32 port, UINT8 data)
{
	if (!(port >= MCS51_PORT_P0 && port <= MCS51_PORT_P3))
		return;

	port &= 0x3;

	if (port == 2)
	{
		if (~data & 0x1 && i8751PortData[2] & 0x1) {
			mcs51_set_irq_line(MCS51_INT0_LINE, CPU_IRQSTATUS_NONE);
		}

		if (~data & 0x2 && i8751PortData[2] & 0x2) {
			mcs51_set_irq_line(MCS51_INT1_LINE, CPU_IRQSTATUS_NONE);
		}

		if (~data & 0x4 && i8751PortData[2] & 0x4) {
			SekSetIRQLine(6, CPU_IRQSTATUS_ACK);
		}

		if (~data & 0x10 && i8751PortData[2] & 0x10) {
			i8751PortData[0] = i8751Command;
		}

		if (~data & 0x20 && i8751PortData[2] & 0x20) {
			i8751PortData[1] = i8751Command >> 8;
		}

		if (~data & 0x40 && i8751PortData[2] & 0x40) {
			i8751RetVal = (i8751RetVal & 0xff00) | (i8751PortData[0]);
		}

		if (~data & 0x80 && i8751PortData[2] & 0x80) {
			i8751RetVal = (i8751RetVal & 0xff) | (i8751PortData[1] << 8);
		}
	}

	i8751PortData[port] = data;
}

static void DrvMCUInit()
{
	mcs51_init();
	mcs51_set_program_data(DrvMCUROM);
	mcs51_set_write_handler(mcu_write_port);
	mcs51_set_read_handler(mcu_read_port);

	DrvMCUReset();
}

static void DrvMCUExit() {
	mcs51_exit();
}

static INT32 DrvMCURun(INT32 cycles)
{
	return mcs51Run(cycles);
}

static INT32 DrvMCUScan(INT32 nAction)
{
	mcs51_scan(nAction);

	SCAN_VAR(i8751RetVal);
	SCAN_VAR(i8751Command);
	SCAN_VAR(i8751PortData);

	return 0;
}

static void DrvMCUWrite(UINT16 data)
{
	DrvMCUSync();

	i8751Command = data;

	mcs51_set_irq_line(MCS51_INT1_LINE, CPU_IRQSTATUS_HOLD);
}

static void DrvMCUSync()
{
	INT32 todo = ((double)SekTotalCycles() * (8000000/12) / 10000000) - mcs51TotalCycles();

	if (todo > 0) {
		DrvMCURun(todo);
	}
}

static void DrvMCUReset()
{
	i8751RetVal = i8751Command = 0;
	memset(&i8751PortData, 0, sizeof(i8751PortData));
	mcs51_reset();
}

static void karnov_control_w(INT32 offset, INT32 data)
{
	switch (offset<<1)
	{
		case 0:
			SekSetIRQLine(6, CPU_IRQSTATUS_NONE);
			return;

		case 2:
			*soundlatch = data;
			M6502SetIRQLine(M6502_INPUT_LINE_NMI, CPU_IRQSTATUS_AUTO);
			break;

		case 4:
			memcpy (DrvSprBuf, DrvSprRAM, 0x1000);
			break;

		case 6:
			DrvMCUWrite(data);
			break;

		case 8:
			DrvScroll[0] = data;
			*flipscreen = data >> 15;
			break;

		case 0xa:
			DrvScroll[1] = data;
			break;

		case 0xc: // ??
			break;

		case 0xe:
			SekSetIRQLine(7, CPU_IRQSTATUS_NONE);
			break;
	}
}

static UINT16 karnov_control_r(INT32 offset)
{
	switch (offset<<1)
	{
		case 0:
			return DrvInput[0];
		case 2:
			return DrvInput[1] ^ vblank;
		case 4:
			return (DrvDip[1] << 8) | DrvDip[0];
		case 6:
			DrvMCUSync();
			return i8751RetVal;
	}

	return ~0;
}

//------------------------------------------------------------------------------------------

static void __fastcall karnov_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfff800) == 0x0a1800) {
		UINT16 *ptr = (UINT16*)DrvPfRAM;

		INT32 offset = (address >> 1) & 0x3ff;
		offset = ((offset & 0x1f) << 5) | ((offset & 0x3e0) >> 5);

		ptr[offset] = BURN_ENDIAN_SWAP_INT16(data);
		return;
	}

	if ((address & 0xfffff0) == 0x0c0000) {
		karnov_control_w((address >> 1) & 0x07, data);
		return;
	}
}

static void __fastcall karnov_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xfff800) == 0x0a1800) {
		INT32 offset = (address >> 1) & 0x3ff;
		offset = ((offset & 0x1f) << 5) | ((offset & 0x3e0) >> 5);

		DrvPfRAM[(offset << 1) | (~address & 1)] = data;
		return;
	}

	if ((address & 0xfffff0) == 0x0c0000) {
		karnov_control_w((address >> 1) & 0x07, data);

	}
}

static UINT16 __fastcall karnov_main_read_word(UINT32 address)
{
	if ((address & 0xfffff0) == 0x0c0000) {
		return karnov_control_r((address >> 1) & 7);
	}

	return 0;
}

static UINT8 __fastcall karnov_main_read_byte(UINT32 address)
{
	if ((address & 0xfffff0) == 0x0c0000) {
		return karnov_control_r((address >> 1) & 7) >> ((~address & 1) << 3);
	}

	return 0;
}

static void karnov_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x1000:
		case 0x1001:
			BurnYM2203Write(0, address & 1, data);
		return;

		case 0x1800:
		case 0x1801:
			BurnYM3526Write(address & 1, data);
		return;
	}
}

static UINT8 karnov_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x0800:
			return *soundlatch;
	}

	return 0;
}

static void DrvYM3526FMIRQHandler(INT32, INT32 nStatus)
{
	M6502SetIRQLine(M6502_IRQ_LINE, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	M6502Open(0);

	SekReset();
	M6502Reset();

	DrvMCUReset();

	BurnYM3526Reset();
	BurnYM2203Reset();

	M6502Close();
	SekClose();

	memset(nCyclesExtra, 0, sizeof(nCyclesExtra));

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x060000;
	Drv6502ROM		= Next; Next += 0x010000;
	DrvMCUROM       = Next; Next += 0x001000;

	DrvGfxROM0		= Next; Next += 0x020000;
	DrvGfxROM1		= Next; Next += 0x080000;
	DrvGfxROM2		= Next; Next += 0x100000;

	DrvColPROM		= Next; Next += 0x000800;

	Palette			= (UINT32*)Next; Next += 0x0300 * sizeof(UINT32);
	DrvPalette		= (UINT32*)Next; Next += 0x0300 * sizeof(UINT32);

	AllRam			= Next;

	Drv68KRAM		= Next; Next += 0x004000;
	DrvPfRAM		= Next; Next += 0x000800;
	Drv6502RAM		= Next; Next += 0x000800;
	DrvVidRAM		= Next; Next += 0x000800;
	DrvSprRAM		= Next; Next += 0x001000;
	DrvSprBuf		= Next; Next += 0x001000;

	soundlatch		= Next; Next += 0x000001;
	flipscreen		= Next; Next += 0x000001;

	DrvScroll		= (UINT16*)Next; Next += 0x0002 * sizeof(UINT16);

	RamEnd			= Next;
	MemEnd			= Next;

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x300; i++)
	{
		INT32 bit0,bit1,bit2,bit3,r,g,b;

		bit0 = (DrvColPROM[0x000 + i] >> 0) & 0x01;
		bit1 = (DrvColPROM[0x000 + i] >> 1) & 0x01;
		bit2 = (DrvColPROM[0x000 + i] >> 2) & 0x01;
		bit3 = (DrvColPROM[0x000 + i] >> 3) & 0x01;
		r = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[0x000 + i] >> 4) & 0x01;
		bit1 = (DrvColPROM[0x000 + i] >> 5) & 0x01;
		bit2 = (DrvColPROM[0x000 + i] >> 6) & 0x01;
		bit3 = (DrvColPROM[0x000 + i] >> 7) & 0x01;
		g = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[0x400 + i] >> 0) & 0x01;
		bit1 = (DrvColPROM[0x400 + i] >> 1) & 0x01;
		bit2 = (DrvColPROM[0x400 + i] >> 2) & 0x01;
		bit3 = (DrvColPROM[0x400 + i] >> 3) & 0x01;
		b = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		Palette[i] = (r << 16) | (g << 8) | b;
		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[3] = { 0x6000*8,0x4000*8,0x2000*8 };
	INT32 Plane1[4] = { 0x60000*8,0x00000*8,0x20000*8,0x40000*8 };
	INT32 XOffs[16] = { 16*8, 1+(16*8), 2+(16*8), 3+(16*8), 4+(16*8), 5+(16*8), 6+(16*8), 7+(16*8), 0,1,2,3,4,5,6,7 };
	INT32 YOffs[16] = { 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 ,8*8,9*8,10*8,11*8,12*8,13*8,14*8,15*8 };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x80000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x08000);

	GfxDecode(0x0400, 3,  8,  8, Plane0, XOffs + 8, YOffs, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x80000);

	GfxDecode(0x0800, 4, 16, 16, Plane1, XOffs + 0, YOffs, 0x100, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x80000);

	GfxDecode(0x1000, 4, 16, 16, Plane1, XOffs + 0, YOffs, 0x100, tmp, DrvGfxROM2);

	BurnFree (tmp);

	return 0;
}

static INT32 DrvInit(INT32 mcuid)
{
	BurnAllocMemIndex();

	is_game = mcuid;

	{
		if (BurnLoadRom(Drv68KROM + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x000000,  1, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x020001,  2, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x020000,  3, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x040001,  4, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x040000,  5, 2)) return 1;

		if (BurnLoadRom(Drv6502ROM + 0x08000,  6, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x20000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x40000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x60000, 11, 1)) return 1;

		if (is_game == CHELNOV) {
			if (BurnLoadRom(DrvGfxROM2 + 0x00000, 12, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x20000, 13, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x40000, 14, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x60000, 15, 1)) return 1;

			if (BurnLoadRom(DrvColPROM + 0x00000, 16, 1)) return 1;
			if (BurnLoadRom(DrvColPROM + 0x00400, 17, 1)) return 1;

			if (BurnLoadRom(DrvMCUROM  + 0x00000, 18, 1)) return 1;
		} else { // karnov, wndrplnt
			if (BurnLoadRom(DrvGfxROM2 + 0x00000, 12, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x10000, 13, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x20000, 14, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x30000, 15, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x40000, 16, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x50000, 17, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x60000, 18, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x70000, 19, 1)) return 1;

			if (BurnLoadRom(DrvColPROM + 0x00000, 20, 1)) return 1;
			if (BurnLoadRom(DrvColPROM + 0x00400, 21, 1)) return 1;

			if (BurnLoadRom(DrvMCUROM  + 0x00000, 22, 1)) return 1;
		}

		DrvPaletteInit();
		DrvGfxDecode();
	}

#ifdef BUILD_A68K
	// These games really don't like the ASM core, so disable it for now
	// and restore it on exit.
	if (bBurnUseASMCPUEmulation) {
		bUseAsm68KCoreOldValue = bBurnUseASMCPUEmulation;
		bBurnUseASMCPUEmulation = false;
	}
#endif

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x05ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,		0x060000, 0x063fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,		0x080000, 0x080fff, MAP_RAM);
	SekMapMemory(DrvVidRAM,		0x0a0000, 0x0a07ff, MAP_RAM);
	SekMapMemory(DrvVidRAM,		0x0a0800, 0x0a0fff, MAP_RAM);
	SekMapMemory(DrvPfRAM,		0x0a1000, 0x0a17ff, MAP_WRITE);
	SekSetWriteWordHandler(0,	karnov_main_write_word);
	SekSetWriteByteHandler(0,	karnov_main_write_byte);
	SekSetReadWordHandler(0,	karnov_main_read_word);
	SekSetReadByteHandler(0,	karnov_main_read_byte);
	SekClose();

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(Drv6502RAM,		0x0000, 0x05ff, MAP_RAM);
	M6502MapMemory(Drv6502ROM + 0x8000,	0x8000, 0xffff, MAP_ROM);
	M6502SetReadHandler(karnov_sound_read);
	M6502SetWriteHandler(karnov_sound_write);
	M6502Close();

	DrvMCUInit();

	BurnYM3526Init(3000000, &DrvYM3526FMIRQHandler, 0);
	BurnTimerAttach(&M6502Config, 1500000);
	BurnYM3526SetRoute(BURN_SND_YM3526_ROUTE, 1.00, BURN_SND_ROUTE_BOTH);

	BurnYM2203Init(1, 1500000, NULL, 1);
	BurnYM2203SetAllRoutes(0, 0.25, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	SekExit();
	M6502Exit();

	DrvMCUExit();

	BurnYM3526Exit();
	BurnYM2203Exit();

	BurnFreeMemIndex();

#ifdef BUILD_A68K
	if (bUseAsm68KCoreOldValue) {
		bBurnUseASMCPUEmulation = true;
	}
#endif

	return 0;
}

static void draw_txt_layer()
{
	UINT16 *vram = (UINT16*)DrvVidRAM;
	for (INT32 offs = 0x20; offs < 0x3e0; offs++)
	{
		INT32 sx = (offs & 0x1f) << 3;
		INT32 sy = (offs >> 5) << 3;

		if (is_game == WNDRPLNT) {
			INT32 t = sx;
			sx = sy;
			sy = t;
		}

		if (*flipscreen) {
			sy ^= 0xf8;
			sx ^= 0xf8;
		}

		if (is_game == WNDRPLNT) {
			sy -= 8;
		}

		INT32 code  = BURN_ENDIAN_SWAP_INT16(vram[offs]) & 0x0fff;
		INT32 color = BURN_ENDIAN_SWAP_INT16(vram[offs]) >> 14;

		if (code == 0) continue;

		Draw8x8MaskTile(pTransDraw, code, sx, sy, *flipscreen, *flipscreen, color, 3, 0, 0, DrvGfxROM0);
	}
}

static void draw_bg_layer()
{
	INT32 scrollx = DrvScroll[0] & 0x1ff;
	INT32 scrolly = DrvScroll[1] & 0x1ff;

	UINT16 *vram = (UINT16*)DrvPfRAM;

	for (INT32 offs = 0; offs < 0x400; offs++)
	{
		INT32 sx = (offs & 0x1f) << 4;
		INT32 sy = (offs >> 5) << 4;

		sy -= scrolly;
		if (sy < -15) sy+=512;
		sx -= scrollx;
		if (sx < -15) sx+=512;

		if (sx >= nScreenWidth || sy >= nScreenHeight) continue;

		INT32 attr = BURN_ENDIAN_SWAP_INT16(vram[offs]);
		INT32 code = attr & 0x7ff;
		INT32 color= attr >> 12;

		if (*flipscreen) {
			Render16x16Tile_FlipXY_Clip(pTransDraw, code, 240 - sx, (240 - sy) - 8, color, 4, 0x200, DrvGfxROM1);
		} else {
			Render16x16Tile_Clip(pTransDraw, code, sx, sy, color, 4, 0x200, DrvGfxROM1);
		}
	}
}

static inline void sprite_routine(INT32 code, INT32 sx, INT32 sy, INT32 color, INT32 fy, INT32 fx)
{
	Draw16x16MaskTile(pTransDraw, code, sx, sy, fx, fy, color, 4, 0, 0x100, DrvGfxROM2);
}

static void draw_sprites()
{
	UINT16 *ram = (UINT16*)DrvSprBuf;

	for (INT32 offs = 0; offs < 0x800; offs+=4)
	{
		INT32 y = BURN_ENDIAN_SWAP_INT16(ram[offs]);
		INT32 x = BURN_ENDIAN_SWAP_INT16(ram[offs + 2]) & 0x1ff;
		if (~y & 0x8000) continue;
		y &= 0x1ff;

		INT32 sprite = BURN_ENDIAN_SWAP_INT16(ram[offs + 3]);
		INT32 color = sprite >> 12;
		sprite &= 0xfff;

		INT32 flipx = BURN_ENDIAN_SWAP_INT16(ram[offs + 1]);
		INT32 flipy = flipx & 0x02;
		INT32 extra = flipx & 0x10;
		flipx &= 0x04;

		x = (x + 16) & 0x1ff;
		y = (y + 16 + extra) & 0x1ff;
		x = 256 - x;
		y = 256 - y;

		if (*flipscreen) {
			y = 240 - y;
			x = 240 - x;
			flipx ^= 0x04;
			flipy ^= 0x02;
			if (extra) y -= 16;
			y -= 8;
		}

		INT32 sprite2 = sprite + 1;
		if (extra && flipy) {
			sprite2--;
			sprite++;
		}

		sprite_routine(sprite, x, y, color, flipy, flipx);

		if (extra) sprite_routine(sprite2, x, y + 16, color, flipy, flipx);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x300; i++) {
			UINT8 r = Palette[i] >> 16;
			UINT8 g = Palette[i] >> 8;
			UINT8 b = Palette[i] >> 0;

			DrvPalette[i] = BurnHighCol(r, g, b, 0);
		}
		DrvRecalc = 0;
	}

	draw_bg_layer();
	draw_sprites();
	draw_txt_layer();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	SekNewFrame();
	M6502NewFrame();
	mcs51NewFrame();

	{
		UINT16 prev_coin = DrvInput[2];
		memset (DrvInput, 0xff, sizeof(DrvInput));

		for (INT32 i = 0; i < 16; i++) {
			DrvInput[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInput[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInput[2] ^= (DrvJoy3[i] & 1) << i;
		}

		if ((DrvInput[2] ^ prev_coin) & 0xe0 && DrvInput[2] != 0xff) {
			// signal coin -> mcu
			mcs51_set_irq_line(MCS51_INT0_LINE, CPU_IRQSTATUS_ACK);
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[3] = { 10000000 / 60, 1500000 / 60, 8000000 / 60 / 12 };
	INT32 nCyclesDone[3] = { nCyclesExtra[0], 0, nCyclesExtra[2] };

	M6502Open(0);
	SekOpen(0);

	vblank = 0x80;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		if (i == 240) {
			vblank = 0x00;
			SekSetIRQLine(7, CPU_IRQSTATUS_AUTO);

			if (pBurnDraw) {
				DrvDraw();
			}
		}

		CPU_RUN(0, Sek);

		CPU_RUN_TIMER(1);

		CPU_RUN(2, DrvMCU);
	}

	SekClose();
	M6502Close();

	nCyclesExtra[0] = nCyclesDone[0] - nCyclesTotal[0];
	nCyclesExtra[2] = nCyclesDone[2] - nCyclesTotal[2];

	if (pBurnSoundOut) {
		BurnYM3526Update(pBurnSoundOut, nBurnSoundLen);
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin != NULL) {
		*pnMin = 0x029707;
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
		M6502Scan(nAction);

		DrvMCUScan(nAction);

		BurnYM3526Scan(nAction, pnMin);
		BurnYM2203Scan(nAction, pnMin);

		SCAN_VAR(nCyclesExtra);
	}

	return 0;
}


// Karnov (US, rev 6)
/* DE-0248-3 main board, DE-259-0 sub/rom board */

static struct BurnRomInfo karnovRomDesc[] = {
	{ "dn08-6.j15",		0x10000, 0x4c60837f, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "dn11-6.j20",		0x10000, 0xcd4abb99, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "dn07-.j14",		0x10000, 0xfc14291b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "dn10-.j18",		0x10000, 0xa4a34e37, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "dn06-5.j13",		0x10000, 0x29d64e42, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "dn09-5.j17",		0x10000, 0x072d7c49, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "dn05-5.f3",		0x08000, 0xfa1a31a8, 2 | BRF_PRG | BRF_ESS }, //  6 m6502 Code

	{ "dn00-.c5",		0x08000, 0x0ed77c6d, 3 | BRF_GRA },           //  7 Characters

	{ "dn04-.d18",		0x10000, 0xa9121653, 4 | BRF_GRA },           //  8 Tiles
	{ "dn01-.c15",		0x10000, 0x18697c9e, 4 | BRF_GRA },           //  9
	{ "dn03-.d15",		0x10000, 0x90d9dd9c, 4 | BRF_GRA },           // 10
	{ "dn02-.c18",		0x10000, 0x1e04d7b9, 4 | BRF_GRA },           // 11

	{ "dn12-.f8",		0x10000, 0x9806772c, 5 | BRF_GRA },           // 12 Sprites
	{ "dn14-5.f11",		0x08000, 0xac9e6732, 5 | BRF_GRA },           // 13
	{ "dn13-.f9",		0x10000, 0xa03308f9, 5 | BRF_GRA },           // 14
	{ "dn15-5.f12",		0x08000, 0x8933fcb8, 5 | BRF_GRA },           // 15
	{ "dn16-.f13",		0x10000, 0x55e63a11, 5 | BRF_GRA },           // 16
	{ "dn17-5.f15",		0x08000, 0xb70ae950, 5 | BRF_GRA },           // 17
	{ "dn18-.f16",		0x10000, 0x2ad53213, 5 | BRF_GRA },           // 18
	{ "dn19-5.f18",		0x08000, 0x8fd4fa40, 5 | BRF_GRA },           // 19

	{ "dn-21.k8",		0x00400, 0xaab0bb93, 6 | BRF_GRA },           // 20 Color Color Proms
	{ "dn-20.l6",		0x00400, 0x02f78ffb, 6 | BRF_GRA },           // 21

	{ "dn-5.k14",  		0x01000, 0xd056de4e, 7 | BRF_PRG }, 		  // 22 i8751 microcontroller
};

STD_ROM_PICK(karnov)
STD_ROM_FN(karnov)

static INT32 KarnovInit()
{
	return DrvInit(KARNOV);
}

struct BurnDriver BurnDrvKarnov = {
	"karnov", NULL, NULL, NULL, "1987",
	"Karnov (US, rev 6)\0", NULL, "Data East USA", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_RUNGUN, 0,
	NULL, karnovRomInfo, karnovRomName, NULL, NULL, NULL, NULL, KarnovInputInfo, KarnovDIPInfo,
	KarnovInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x300, 256, 248, 4, 3
};


// Karnov (US, rev 5)
/* DE-0248-3 main board, DE-259-0 sub/rom board */

static struct BurnRomInfo karnovaRomDesc[] = {
	{ "dn08-5.j15",		0x10000, 0xdb92c264, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "dn11-5.j20",		0x10000, 0x05669b4b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "dn07-.j14",		0x10000, 0xfc14291b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "dn10-.j18",		0x10000, 0xa4a34e37, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "dn06-5.j13",		0x10000, 0x29d64e42, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "dn09-5.j17",		0x10000, 0x072d7c49, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "dn05-5.f3",		0x08000, 0xfa1a31a8, 2 | BRF_PRG | BRF_ESS }, //  6 m6502 Code

	{ "dn00-.c5",		0x08000, 0x0ed77c6d, 3 | BRF_GRA },           //  7 Characters

	{ "dn04-.d18",		0x10000, 0xa9121653, 4 | BRF_GRA },           //  8 Tiles
	{ "dn01-.c15",		0x10000, 0x18697c9e, 4 | BRF_GRA },           //  9
	{ "dn03-.d15",		0x10000, 0x90d9dd9c, 4 | BRF_GRA },           // 10
	{ "dn02-.c18",		0x10000, 0x1e04d7b9, 4 | BRF_GRA },           // 11

	{ "dn12-.f8",		0x10000, 0x9806772c, 5 | BRF_GRA },           // 12 Sprites
	{ "dn14-5.f11",		0x08000, 0xac9e6732, 5 | BRF_GRA },           // 13
	{ "dn13-.f9",		0x10000, 0xa03308f9, 5 | BRF_GRA },           // 14
	{ "dn15-5.f12",		0x08000, 0x8933fcb8, 5 | BRF_GRA },           // 15
	{ "dn16-.f13",		0x10000, 0x55e63a11, 5 | BRF_GRA },           // 16
	{ "dn17-5.f15",		0x08000, 0xb70ae950, 5 | BRF_GRA },           // 17
	{ "dn18-.f16",		0x10000, 0x2ad53213, 5 | BRF_GRA },           // 18
	{ "dn19-5.f18",		0x08000, 0x8fd4fa40, 5 | BRF_GRA },           // 19

	{ "dn-21.k8",		0x00400, 0xaab0bb93, 6 | BRF_GRA },           // 20 Color Color Proms
	{ "dn-20.l6",		0x00400, 0x02f78ffb, 6 | BRF_GRA },           // 21

	{ "dn-5.k14",  		0x01000, 0xd056de4e, 7 | BRF_PRG }, 		  // 22 i8751 microcontroller
};

STD_ROM_PICK(karnova)
STD_ROM_FN(karnova)

struct BurnDriver BurnDrvKarnova = {
	"karnova", "karnov", NULL, NULL, "1987",
	"Karnov (US, rev 5)\0", NULL, "Data East USA", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_RUNGUN, 0,
	NULL, karnovaRomInfo, karnovaRomName, NULL, NULL, NULL, NULL, KarnovInputInfo, KarnovDIPInfo,
	KarnovInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x300, 256, 248, 4, 3
};


// Karnov (Japan)
/* DE-0248-3 main board, DE-259-0 sub/rom board */

static struct BurnRomInfo karnovjRomDesc[] = {
	{ "dn08-.j15",		0x10000, 0x3e17e268, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "dn11-.j20",		0x10000, 0x417c936d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "dn07-.j14",		0x10000, 0xfc14291b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "dn10-.j18",		0x10000, 0xa4a34e37, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "dn06-.j13",		0x10000, 0xc641e195, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "dn09-.j17",		0x10000, 0xd420658d, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "dn05-.f3",		0x08000, 0x7c9158f1, 2 | BRF_PRG | BRF_ESS }, //  6 m6502 Code

	{ "dn00-.c5",		0x08000, 0x0ed77c6d, 3 | BRF_GRA },           //  7 Characters

	{ "dn04-.d18",		0x10000, 0xa9121653, 4 | BRF_GRA },           //  8 Tiles
	{ "dn01-.c15",		0x10000, 0x18697c9e, 4 | BRF_GRA },           //  9
	{ "dn03-.d15",		0x10000, 0x90d9dd9c, 4 | BRF_GRA },           // 10
	{ "dn02-.c18",		0x10000, 0x1e04d7b9, 4 | BRF_GRA },           // 11

	{ "dn12-.f8",		0x10000, 0x9806772c, 5 | BRF_GRA },           // 12 Sprites
	{ "kar14.f11",		0x08000, 0xc6b39595, 5 | BRF_GRA },           // 13
	{ "dn13-.f9",		0x10000, 0xa03308f9, 5 | BRF_GRA },           // 14
	{ "kar15.f12",		0x08000, 0x2f72cac0, 5 | BRF_GRA },           // 15
	{ "dn16-.f13",		0x10000, 0x55e63a11, 5 | BRF_GRA },           // 16
	{ "kar17.f15",		0x08000, 0x7851c70f, 5 | BRF_GRA },           // 17
	{ "dn18-.f16",		0x10000, 0x2ad53213, 5 | BRF_GRA },           // 18
	{ "kar19.f18",		0x08000, 0x7bc174bb, 5 | BRF_GRA },           // 19

	{ "dn-21.k8",		0x00400, 0xaab0bb93, 6 | BRF_GRA },           // 20 Color Proms
	{ "dn-20.l6",		0x00400, 0x02f78ffb, 6 | BRF_GRA },           // 21

	{ "dn-3.k14",  0x01000, 0x5a8c4d28,  7 | BRF_PRG }, 	  // 22 i8751 microcontroller
};

STD_ROM_PICK(karnovj)
STD_ROM_FN(karnovj)

struct BurnDriver BurnDrvKarnovj = {
	"karnovj", "karnov", NULL, NULL, "1987",
	"Karnov (Japan)\0", NULL, "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_RUNGUN, 0,
	NULL, karnovjRomInfo, karnovjRomName, NULL, NULL, NULL, NULL, KarnovInputInfo, KarnovDIPInfo,
	KarnovInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x300, 256, 248, 4, 3
};


// Wonder Planet (Japan)

static struct BurnRomInfo wndrplntRomDesc[] = {
	{ "ea08.j16",		0x10000, 0xb0578a14, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "ea11.j19",		0x10000, 0x271edc6c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ea07.j14",		0x10000, 0x7095a7d5, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ea10.j18",		0x10000, 0x81a96475, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ea06.j13",		0x10000, 0x5951add3, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "ea09.j17",		0x10000, 0xc4b3cb1e, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "ea05.f3",		0x08000, 0x8dbb6231, 2 | BRF_PRG | BRF_ESS }, //  6 m6502 Code

	{ "ea00.c5",		0x08000, 0x9f3cac4c, 3 | BRF_GRA },           //  7 Characters

	{ "ea04.d18",		0x10000, 0x7d701344, 4 | BRF_GRA },           //  8 Tiles
	{ "ea01.c18",		0x10000, 0x18df55fb, 4 | BRF_GRA },           //  9
	{ "ea03.d15",		0x10000, 0x922ef050, 4 | BRF_GRA },           // 10
	{ "ea02.c18",		0x10000, 0x700fde70, 4 | BRF_GRA },           // 11

	{ "ea12.f8",		0x10000, 0xa6d4e99d, 5 | BRF_GRA },           // 12 Sprites
	{ "ea14.f9",		0x10000, 0x915ffdc9, 5 | BRF_GRA },           // 13
	{ "ea13.f13",		0x10000, 0xcd839f3a, 5 | BRF_GRA },           // 14
	{ "ea15.f15",		0x10000, 0xa1f14f16, 5 | BRF_GRA },           // 15
	{ "ea16.bin",		0x10000, 0x7a1d8a9c, 5 | BRF_GRA },           // 16
	{ "ea17.bin",		0x10000, 0x21a3223d, 5 | BRF_GRA },           // 17
	{ "ea18.bin",		0x10000, 0x3fb2cec7, 5 | BRF_GRA },           // 18
	{ "ea19.bin",		0x10000, 0x87cf03b5, 5 | BRF_GRA },           // 19

	{ "ea-21.k8",		0x00400, 0xc8beab49, 6 | BRF_GRA },           // 20 Color Proms
	{ "ea-20.l6",		0x00400, 0x619f9d1e, 6 | BRF_GRA },           // 21

	{ "ea.k14",  		0x01000, 0xb481f6a9, 7 | BRF_PRG | BRF_ESS }, // 22 i8751 microcontroller
};

STD_ROM_PICK(wndrplnt)
STD_ROM_FN(wndrplnt)

static INT32 WndrplntInit()
{
	return DrvInit(WNDRPLNT);
}

struct BurnDriver BurnDrvWndrplnt = {
	"wndrplnt", NULL, NULL, NULL, "1987",
	"Wonder Planet (Japan)\0", NULL, "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_VERSHOOT, 0,
	NULL, wndrplntRomInfo, wndrplntRomName, NULL, NULL, NULL, NULL, KarnovInputInfo, WndrplntDIPInfo,
	WndrplntInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x300, 248, 256, 3, 4
};


// Atomic Runner Chelnov (World)
/* DE-0248-1 main board, DE-259-0 sub/rom board */

static struct BurnRomInfo chelnovRomDesc[] = {
	{ "ee08-e.j16",		0x10000, 0x8275cc3a, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "ee11-e.j19",		0x10000, 0x889e40a0, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ee07.j14",		0x10000, 0x51465486, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ee10.j18",		0x10000, 0xd09dda33, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ee06-e.j13",		0x10000, 0x55acafdb, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "ee09-e.j17",		0x10000, 0x303e252c, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "ee05-.f3",		0x08000, 0x6a8936b4, 2 | BRF_PRG | BRF_ESS }, //  6 m6502 Code

	{ "ee00-e.c5",		0x08000, 0xe06e5c6b, 3 | BRF_GRA },           //  7 Characters

	{ "ee04-.d18",		0x10000, 0x96884f95, 4 | BRF_GRA },           //  8 Tiles
	{ "ee01-.c15",		0x10000, 0xf4b54057, 4 | BRF_GRA },           //  9
	{ "ee03-.d15",		0x10000, 0x7178e182, 4 | BRF_GRA },           // 10
	{ "ee02-.c18",		0x10000, 0x9d7c45ae, 4 | BRF_GRA },           // 11

	{ "ee12-.f8",		0x10000, 0x9b1c53a5, 5 | BRF_GRA },           // 12 Sprites
	{ "ee13-.f9",		0x10000, 0x72b8ae3e, 5 | BRF_GRA },           // 13
	{ "ee14-.f13",		0x10000, 0xd8f4bbde, 5 | BRF_GRA },           // 14
	{ "ee15-.f15",		0x10000, 0x81e3e68b, 5 | BRF_GRA },           // 15

	{ "ee-17.k8",		0x00400, 0xb1db6586, 6 | BRF_GRA },           // 16 Color Proms
	{ "ee-16.l6",		0x00400, 0x41816132, 6 | BRF_GRA },           // 17

	{ "ee-e.k14",  		0x01000, 0xb7045395, 7 | BRF_PRG },			  // 18 i8751 microcontroller
};

STD_ROM_PICK(chelnov)
STD_ROM_FN(chelnov)

static INT32 ChelnovInit()
{
	return DrvInit(CHELNOV);
}

struct BurnDriver BurnDrvChelnov = {
	"chelnov", NULL, NULL, NULL, "1988",
	"Atomic Runner Chelnov (World)\0", NULL, "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_RUNGUN, 0,
	NULL, chelnovRomInfo, chelnovRomName, NULL, NULL, NULL, NULL, KarnovInputInfo, ChelnovDIPInfo,
	ChelnovInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x300, 256, 248, 4, 3
};


// Atomic Runner Chelnov (US)
/* DE-0248-1 main board, DE-259-0 sub/rom board */

static struct BurnRomInfo chelnovuRomDesc[] = {
	{ "ee08-a.j15",		0x10000, 0x2f2fb37b, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "ee11-a.j20",		0x10000, 0xf306d05f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ee07-a.j14",		0x10000, 0x9c69ed56, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ee10-a.j18",		0x10000, 0xd5c5fe4b, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ee06-e.j13",		0x10000, 0x55acafdb, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "ee09-e.j17",		0x10000, 0x303e252c, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "ee05-.f3",		0x08000, 0x6a8936b4, 2 | BRF_PRG | BRF_ESS }, //  6 m6502 Code

	{ "ee00-e.c5",		0x08000, 0xe06e5c6b, 3 | BRF_GRA },           //  7 Characters

	{ "ee04-.d18",		0x10000, 0x96884f95, 4 | BRF_GRA },           //  8 Tiles
	{ "ee01-.c15",		0x10000, 0xf4b54057, 4 | BRF_GRA },           //  9
	{ "ee03-.d15",		0x10000, 0x7178e182, 4 | BRF_GRA },           // 10
	{ "ee02-.c18",		0x10000, 0x9d7c45ae, 4 | BRF_GRA },           // 11

	{ "ee12-.f8",		0x10000, 0x9b1c53a5, 5 | BRF_GRA },           // 12 Sprites
	{ "ee13-.f9",		0x10000, 0x72b8ae3e, 5 | BRF_GRA },           // 13
	{ "ee14-.f13",		0x10000, 0xd8f4bbde, 5 | BRF_GRA },           // 14
	{ "ee15-.f15",		0x10000, 0x81e3e68b, 5 | BRF_GRA },           // 15

	{ "ee-17.k8",		0x00400, 0xb1db6586, 6 | BRF_GRA },           // 16 Color Proms
	{ "ee-16.l6",		0x00400, 0x41816132, 6 | BRF_GRA },           // 17

	{ "ee-a.k14",  		0x01000, 0x95ea1e7b, 7 | BRF_PRG },			  // 18 i8751 microcontroller
};

STD_ROM_PICK(chelnovu)
STD_ROM_FN(chelnovu)

struct BurnDriver BurnDrvChelnovu = {
	"chelnovu", "chelnov", NULL, NULL, "1988",
	"Atomic Runner Chelnov (US)\0", NULL, "Data East USA", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_RUNGUN, 0,
	NULL, chelnovuRomInfo, chelnovuRomName, NULL, NULL, NULL, NULL, KarnovInputInfo, ChelnovuDIPInfo,
	ChelnovInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x300, 256, 248, 4, 3
};


// Atomic Runner Chelnov (Japan)
/* DE-0248-1 main board, DE-259-0 sub/rom board */

static struct BurnRomInfo chelnovjRomDesc[] = {
	{ "ee08-1.j15",		0x10000, 0x1978cb52, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "ee11-1.j20",		0x10000, 0xe0ed3d99, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ee07.j14",		0x10000, 0x51465486, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ee10.j18",		0x10000, 0xd09dda33, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ee06.j13",		0x10000, 0xcd991507, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "ee09.j17",		0x10000, 0x977f601c, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "ee05.f3",		0x08000, 0x6a8936b4, 2 | BRF_PRG | BRF_ESS }, //  6 m6502 Code

	{ "ee00.c5",		0x08000, 0x1abf2c6d, 3 | BRF_GRA },           //  7 Characters

	{ "ee04-.d18",		0x10000, 0x96884f95, 4 | BRF_GRA },           //  8 Tiles
	{ "ee01-.c15",		0x10000, 0xf4b54057, 4 | BRF_GRA },           //  9
	{ "ee03-.d15",		0x10000, 0x7178e182, 4 | BRF_GRA },           // 10
	{ "ee02-.c18",		0x10000, 0x9d7c45ae, 4 | BRF_GRA },           // 11

	{ "ee12-.f8",		0x10000, 0x9b1c53a5, 5 | BRF_GRA },           // 12 Sprites
	{ "ee13-.f9",		0x10000, 0x72b8ae3e, 5 | BRF_GRA },           // 13
	{ "ee14-.f13",		0x10000, 0xd8f4bbde, 5 | BRF_GRA },           // 14
	{ "ee15-.f15",		0x10000, 0x81e3e68b, 5 | BRF_GRA },           // 15

	{ "ee-17.k8",		0x00400, 0xb1db6586, 6 | BRF_GRA },           // 16 Color Proms
	{ "ee-16.l6",		0x00400, 0x41816132, 6 | BRF_GRA },           // 17

	{ "ee.k14",  		0x01000, 0xb3dc380c, 7 | BRF_PRG },			  // 18 i8751 microcontroller
};

STD_ROM_PICK(chelnovj)
STD_ROM_FN(chelnovj)

struct BurnDriver BurnDrvChelnovj = {
	"chelnovj", "chelnov", NULL, NULL, "1988",
	"Atomic Runner Chelnov (Japan)\0", NULL, "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_RUNGUN, 0,
	NULL, chelnovjRomInfo, chelnovjRomName, NULL, NULL, NULL, NULL, KarnovInputInfo, ChelnovuDIPInfo,
	ChelnovInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x300, 256, 248, 4, 3
};
