// FB Alpha Major Havoc driver module
// Based on MAME driver by Mike Appolo

#include "tiles_generic.h"
#include "m6502_intf.h"
#include "vector.h"
#include "avgdvg.h"
#include "pokey.h"
#include "tms5220.h"
#include "watchdog.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6502ROM0;
static UINT8 *DrvM6502ROM1;
static UINT8 *DrvVectorROM;
static UINT8 *DrvM6502RAM0;
static UINT8 *DrvM6502RAM1;
static UINT8 *DrvVectorRAM;
static UINT8 *DrvShareRAM;
static UINT8 *DrvColRAM;
static UINT8 *DrvNVRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 alpha_irq_clock;
static INT32 alpha_irq_clock_enable;
static INT32 alpha_rombank;
static INT32 alpha_rambank;
static INT32 alpha_xmtd;
static INT32 alpha_data;
static INT32 alpha_rcvd;

static INT32 gamma_xmtd;
static INT32 gamma_data;
static INT32 gamma_rcvd;
static INT32 gamma_irq_clock;
static INT32 gamma_halt;			// stop cpu if in reset!!

static INT32 speech_data;

static INT32 player_1;
static INT32 avgletsgo;
static INT32 nExtraCycles[2];

static UINT8 DrvDial;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo MhavocInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Left",		    BIT_DIGITAL,	DrvJoy4 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy4 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Left",		    BIT_DIGITAL,	DrvJoy4 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy4 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 5,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Mhavoc)

static struct BurnInputInfo AlphaoneInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 coin"	},
	{"P1 Left",		    BIT_DIGITAL,	DrvJoy4 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy4 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 6,	"p2 coin"	},
	{"P3 Coin",			BIT_DIGITAL,	DrvJoy2 + 5,	"p3 coin"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Alphaone)

static struct BurnDIPInfo MhavocDIPList[]=
{
	{0x0c, 0xff, 0xff, 0x00, NULL					},
	{0x0d, 0xff, 0xff, 0xff, NULL					},
	{0x0e, 0xff, 0xff, 0x03, NULL					},

	{0   , 0xfe, 0   ,    2, "Adaptive Difficulty"	},
	{0x0c, 0x01, 0x01, 0x01, "Off"					},
	{0x0c, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x0c, 0x01, 0x02, 0x02, "Off"					},
	{0x0c, 0x01, 0x02, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x0c, 0x01, 0x0c, 0x0c, "50000"				},
	{0x0c, 0x01, 0x0c, 0x00, "100000"				},
	{0x0c, 0x01, 0x0c, 0x04, "200000"				},
	{0x0c, 0x01, 0x0c, 0x08, "None"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x0c, 0x01, 0x30, 0x10, "Easy"					},
	{0x0c, 0x01, 0x30, 0x00, "Medium"				},
	{0x0c, 0x01, 0x30, 0x30, "Hard"					},
	{0x0c, 0x01, 0x30, 0x20, "Demo"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x0c, 0x01, 0xc0, 0x00, "3 (2 in Free Play)"	},
	{0x0c, 0x01, 0xc0, 0xc0, "4 (3 in Free Play)"	},
	{0x0c, 0x01, 0xc0, 0x80, "5 (4 in Free Play)"	},
	{0x0c, 0x01, 0xc0, 0x40, "6 (5 in Free Play)"	},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x0d, 0x01, 0x03, 0x02, "2 Coins 1 Credits"	},
	{0x0d, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x0d, 0x01, 0x03, 0x00, "1 Coin  2 Credits"	},
	{0x0d, 0x01, 0x03, 0x01, "Free Play"			},

	{0   , 0xfe, 0   ,    4, "Right Coin Mechanism"	},
	{0x0d, 0x01, 0x0c, 0x0c, "x1"					},
	{0x0d, 0x01, 0x0c, 0x08, "x4"					},
	{0x0d, 0x01, 0x0c, 0x04, "x5"					},
	{0x0d, 0x01, 0x0c, 0x00, "x6"					},

	{0   , 0xfe, 0   ,    2, "Left Coin Mechanism"	},
	{0x0d, 0x01, 0x10, 0x10, "x1"					},
	{0x0d, 0x01, 0x10, 0x00, "x2"					},

	{0   , 0xfe, 0   ,    5, "Bonus Credits"		},
	{0x0d, 0x01, 0xe0, 0x80, "2 each 4"				},
	{0x0d, 0x01, 0xe0, 0x40, "1 each 3"				},
	{0x0d, 0x01, 0xe0, 0xa0, "1 each 4"				},
	{0x0d, 0x01, 0xe0, 0x60, "1 each 5"				},
	{0x0d, 0x01, 0xe0, 0xe0, "None"					},

	{0   , 0xfe, 0   ,    2, "Credit to start"		},
	{0x0e, 0x01, 0x01, 0x01, "1"					},
	{0x0e, 0x01, 0x01, 0x00, "2"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"	},
	{0x0e, 0x01, 0x02, 0x02, "Off"					},
	{0x0e, 0x01, 0x02, 0x00, "On"					},
};

STDDIPINFO(Mhavoc)

static struct BurnDIPInfo MhavocpDIPList[] = {
	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x0c, 0x01, 0x0c, 0x00, "1"					},
	{0x0c, 0x01, 0x0c, 0x01, "2"					},
	{0x0c, 0x01, 0x0c, 0x02, "3"					},
	{0x0c, 0x01, 0x0c, 0x03, "4"					},
};

STDDIPINFOEXT(Mhavocp, Mhavoc, Mhavocp)

static struct BurnDIPInfo AlphaoneDIPList[]=
{
	{0x07, 0xff, 0xff, 0x10, NULL					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x07, 0x01, 0x10, 0x10, "Off"					},
	{0x07, 0x01, 0x10, 0x00, "On"					},
};

STDDIPINFO(Alphaone)

static void palette_write(INT32 i)
{
	i &= 0x1f;

	{
		UINT8 data = DrvColRAM[i];
		int bit3 = (~data >> 3) & 1;
		int bit2 = (~data >> 2) & 1;
		int bit1 = (~data >> 1) & 1;
		int bit0 = (~data >> 0) & 1;
		int r = bit3 * 0xee + bit2 * 0x11;
		int g = bit1 * 0xee;
		int b = bit0 * 0xee;
		
		for (INT32 j = 0; j < 256; j++) // intensity
		{
			INT32 r1 = (r * j) / 0xff;
			INT32 g1 = (g * j) / 0xff;
			INT32 b1 = (b * j) / 0xff;

			// must be 32-bit palette
			DrvPalette[i * 256 + j] = (r1 << 16) | (g1 << 8) | b1;
		}
	}
}

static void bankswitch(INT32 data)
{
	alpha_rombank = data & 3;

	M6502MapMemory(DrvM6502ROM0 + 0x00000 + (data & 3) * 0x2000, 0x2000, 0x3fff, MAP_ROM);
}

static void rambankswitch(INT32 data)
{
	alpha_rambank = data & 1;

	M6502MapMemory(DrvM6502RAM0 + 0x00200 + (data & 1) * 0x0800, 0x0200, 0x07ff, MAP_RAM);
	M6502MapMemory(DrvM6502RAM0 + 0x00200 + (data & 1) * 0x0800, 0x0a00, 0x0fff, MAP_RAM);
}

static void mhavoc_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xffe0) == 0x1400) {
		DrvColRAM[address & 0x1f] = data;
		palette_write(address);
		return;
	}
	
	switch (address)
	{
		case 0x1200:
		return; // nop

		case 0x1600:
		{
			player_1 = (data >> 5) & 1;
			gamma_halt = ~data & 0x08;

			if (gamma_halt)
			{
				M6502Close();
				M6502Open(1);
				M6502Reset();
				M6502Close();
				M6502Open(0);

				alpha_rcvd = 0;
				alpha_xmtd = 0;
				gamma_rcvd = 0;
				gamma_xmtd = 0;
			}

		}
		return;

		case 0x1640:
			avgdvg_go();
			avgletsgo = 1;
		return;

		case 0x1680:
			BurnWatchdogRead();
		return;

		case 0x16c0:
			avgdvg_reset();
		return;

		case 0x1700:
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
			alpha_irq_clock = 0;
			alpha_irq_clock_enable = 1;
		return;

		case 0x1740:
			bankswitch(data);
		return;

		case 0x1780:
			rambankswitch(data);
		return;

		case 0x17c0:
		{
			gamma_rcvd = 0;
			alpha_xmtd = 1;
			alpha_data = data;

			M6502Close();
			M6502Open(1);
			M6502SetIRQLine(0x20, CPU_IRQSTATUS_AUTO);
			M6502Close();
			M6502Open(0);
		}
		return;
	}
}

static UINT8 mhavoc_main_read(UINT16 address)
{
	if ((address & 0xffe0) == 0x1400) {
		return DrvColRAM[address & 0x1f];
	}

	switch (address)
	{
		case 0x1000:
			alpha_rcvd = 1;
			gamma_xmtd = 0;
			return gamma_data;

		case 0x1200: // in0
		{
			UINT8 ret = (DrvInputs[0] & 0x30);
			ret |= avgdvg_done() ? 0x01 : 0;
			ret |= (M6502TotalCycles() & 0x400) ? 0 : 0x02;
			ret |= gamma_xmtd ? 0x04 : 0;
			ret |= gamma_rcvd ? 0x08 : 0;
			ret |= (player_1) ? (DrvDips[2] << 6) : (DrvInputs[2] << 6);
			return ret; 
		}
	}

	return 0;
}

static void mhavoc_sub_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf800) == 0x2000) {
		quad_pokey_w(address & 0x3f, data);
		return;
	}

	switch (address & ~0x7ff)
	{
		case 0x4000:
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
			gamma_irq_clock = 0;
		return;

		case 0x4800:
			// coin counters
		return;

		case 0x5000:
			alpha_rcvd = 0;
			gamma_xmtd = 1;
			gamma_data = data;
		return;
    }

    switch (address)
    {
		case 0x5800:
			speech_data = data;
		return;

        case 0x5900:
			tms5220_write(speech_data);
		return;
	}
}

static UINT8 mhavoc_sub_read(UINT16 address)
{
//	bprintf (0, _T("SR: %4.4x\n"), address);
	
	if ((address & 0xf800) == 0x2000) {
		return quad_pokey_r(address & 0x3f);
	}

	switch (address & ~0x7ff)
	{
		case 0x2800: // in1
		{
			UINT8 ret = (DrvInputs[1] & 0xf8);
			ret |= alpha_xmtd ? 0x01 : 0;
			ret |= alpha_rcvd ? 0x02 : 0;
            ret |= tms5220_ready() ? 0x00 : 0x04;
			return ret;
		}

		case 0x3000:
			gamma_rcvd = 1;
			alpha_xmtd = 0;
			return alpha_data;

		case 0x3800:
		{
			return DrvDial;
		}

		case 0x4000:
			return DrvDips[1];
	}

	return 0;
}

static void alphaone_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xffe0) == 0x1020) {
		INT32 num = (address >> 3) & 1;
		address = (address & 7) | ((address & 0x10) >> 1);
		pokey_write(num, address, data);
		return;
	}

	if ((address & 0xffe0) == 0x10e0) {
		DrvColRAM[address & 0x1f] = data;
		palette_write(address);
		return;
	}

	switch (address)
	{
		case 0x1040:
		return; // nop

		case 0x10a0:
			// coin counter
			// leds
		return;

		case 0x10a4:
			avgdvg_go();
			avgletsgo = 1;
		return;

		case 0x10a8:
			BurnWatchdogRead();
		return;

		case 0x10b0:
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
			alpha_irq_clock = 0;
			alpha_irq_clock_enable = 1;
		return;

		case 0x10b4:
			bankswitch(data);
		return;

		case 0x10b8:
			rambankswitch(data);
		return;
	}
}

static UINT8 alphaone_main_read(UINT16 address)
{
	if ((address & 0xffe0) == 0x1020) {
		INT32 num = (address >> 3) & 1;
		address = (address & 7) | ((address & 0x10) >> 1);
		return pokey_read(num,address); // dual
	}

	switch (address)
	{
		case 0x1040:
		{
			UINT8 ret = (DrvInputs[0] & 0x80) | 0x7c;
			ret |= avgdvg_done() ? 0x01 : 0;
			ret |= (M6502TotalCycles() & 0x400) ? 0 : 0x02;
			return ret;
		}

		case 0x1060:
		{
			UINT8 ret = DrvInputs[1] & ~0x10;
			ret |= (DrvDips[0] & 0x10);
			return ret;
		}

		case 0x1080:
		{
			return DrvDial;
		}
	}

	return 0;
}

static INT32 port0_read(INT32 /*offset*/)
{
	return DrvDips[0];
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	M6502Open(0);
	bankswitch(0);
	rambankswitch(0);
	M6502Reset();
	M6502Close();

	if (nM6502Count > 1) {
		M6502Open(1);
		M6502Reset();
		M6502Close();
	}

	BurnWatchdogReset();

	avgdvg_reset();

	PokeyReset();
	tms5220_reset();

	alpha_xmtd = 0;
	alpha_data = 0;
	alpha_rcvd = 0;
	alpha_irq_clock = 0;
	alpha_irq_clock_enable = 1;

	gamma_xmtd = 0;
	gamma_data = 0;
	gamma_rcvd = 0;
	gamma_irq_clock = 0;
	gamma_halt = 0;

	player_1 = 0;
	speech_data = 0;
	avgletsgo = 0;
	nExtraCycles[0] = 0;
	nExtraCycles[1] = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6502ROM0		= Next; Next += 0x010000;
	DrvM6502ROM1		= Next; Next += 0x004000;

	DrvPalette			= (UINT32*)Next; Next += 0x002000 * sizeof(UINT32);

	DrvNVRAM			= Next; Next += 0x000200;

	AllRam				= Next;

	DrvM6502RAM0		= Next; Next += 0x001000;
	DrvM6502RAM1		= Next; Next += 0x000800;
	DrvColRAM			= Next; Next += 0x000020;
	DrvShareRAM			= Next; Next += 0x000800;
	DrvVectorRAM		= Next; Next += 0x001000;

	RamEnd				= Next;

	DrvVectorROM      	= Next; Next += 0x00f000; // must(!) come after DrvVecRAM

	MemEnd				= Next;

	return 0;
}

static INT32 MhavocInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvVectorROM + 0x00000,  0, 1)) return 1;

		if (BurnLoadRom(DrvM6502ROM0 + 0x08000,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM0 + 0x0c000,  2, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM0 + 0x00000,  3, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM0 + 0x04000,  4, 1)) return 1;

		if (BurnLoadRom(DrvVectorROM + 0x07000,  5, 1)) return 1;
		if (BurnLoadRom(DrvVectorROM + 0x0b000,  6, 1)) return 1;

		if (BurnLoadRom(DrvM6502ROM1 + 0x00000,  7, 1)) return 1;
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvM6502RAM0,				0x0000, 0x0fff, MAP_RAM);
	M6502MapMemory(DrvShareRAM,					0x1800, 0x1fff, MAP_RAM);
	M6502MapMemory(DrvVectorRAM,				0x4000, 0x4fff, MAP_RAM);
	M6502MapMemory(DrvVectorROM,				0x5000, 0x6fff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM0 + 0x08000,		0x8000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(mhavoc_main_write);
	M6502SetReadHandler(mhavoc_main_read);
	M6502Close();

	M6502Init(1, TYPE_M6502);
	M6502Open(1);	
	M6502MapMemory(DrvM6502RAM1,				0x0000, 0x07ff, MAP_RAM);
	M6502MapMemory(DrvM6502RAM1,				0x0800, 0x0fff, MAP_RAM);
	M6502MapMemory(DrvM6502RAM1,				0x1000, 0x17ff, MAP_RAM);
	M6502MapMemory(DrvM6502RAM1,				0x1800, 0x1fff, MAP_RAM);
	for (INT32 i = 0; i < 0x2000; i+=0x200) {
		M6502MapMemory(DrvNVRAM,				0x6000 + i, 0x61ff + i, MAP_RAM);
	}
	M6502MapMemory(DrvM6502ROM1 + 0x0000, 		0x8000, 0xbfff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM1 + 0x0000, 		0xc000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(mhavoc_sub_write);
	M6502SetReadHandler(mhavoc_sub_read);
	M6502Close();

	BurnWatchdogInit(DrvDoReset, 180);

	PokeyInit(12096000/8, 4, 0.50, 0);
	PokeySetTotalCyclesCB(M6502TotalCycles);
	PokeyAllPotCallback(0, port0_read);

	tms5220_init(); // mhavocrv
	tms5220_set_frequency(555555);

	avgdvg_init(USE_AVG_MHAVOC, DrvVectorRAM, 0x4000, M6502TotalCycles, 300, 260);

	memset(DrvNVRAM, 0xff, 0x200);

	DrvDoReset(1);

	return 0;
}

static INT32 AlphaoneInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvVectorROM + 0x00000,  0, 1)) return 1;

		if (BurnLoadRom(DrvM6502ROM0 + 0x08000,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM0 + 0x0a000,  2, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM0 + 0x0c000,  3, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM0 + 0x0e000,  4, 1)) return 1;
		memcpy (DrvM6502ROM0 + 0x0f000, DrvM6502ROM0 + 0x0e000, 0x01000);
		if (BurnLoadRom(DrvM6502ROM0 + 0x00000,  5, 1)) return 1;

		if (BurnLoadRom(DrvVectorROM + 0x07000,  6, 1)) return 1;
		if (BurnLoadRom(DrvVectorROM + 0x0b000,  7, 1)) return 1;
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvM6502RAM0,				0x0000, 0x0fff, MAP_RAM);
	M6502MapMemory(DrvShareRAM,					0x1800, 0x1fff, MAP_RAM);
	M6502MapMemory(DrvVectorRAM,				0x4000, 0x4fff, MAP_RAM);
	M6502MapMemory(DrvVectorROM,				0x5000, 0x6fff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM0 + 0x08000,		0x8000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(alphaone_main_write);
	M6502SetReadHandler(alphaone_main_read);
	M6502Close();

	BurnWatchdogInit(DrvDoReset, 180);

	PokeyInit(1250000, 2, 0.50, 0);
	PokeySetTotalCyclesCB(M6502TotalCycles);

	tms5220_init(); // not in this set
	tms5220_set_frequency(555555);

	avgdvg_init(USE_AVG_MHAVOC, DrvVectorRAM, 0x4000, M6502TotalCycles, 580, 500);

	memset(DrvNVRAM, 0xff, 0x200);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	avgdvg_exit();

	tms5220_exit();
	PokeyExit();
	M6502Exit();

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteUpdate()
{
	for (INT32 i = 0; i < 0x20; i++)
	{
		UINT8 data = DrvColRAM[i];
		int bit3 = (~data >> 3) & 1;
		int bit2 = (~data >> 2) & 1;
		int bit1 = (~data >> 1) & 1;
		int bit0 = (~data >> 0) & 1;
		int r = bit3 * 0xee + bit2 * 0x11;
		int g = bit1 * 0xee;
		int b = bit0 * 0xee;
		
		for (INT32 j = 0; j < 256; j++) // intensity
		{
			INT32 r1 = (r * j) / 0xff;
			INT32 g1 = (g * j) / 0xff;
			INT32 b1 = (b * j) / 0xff;

			DrvPalette[i * 256 + j] = (r1 << 16) | (g1 << 8) | b1; // must be 32bit palette! -dink (see vector.cpp)
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 0;
	}

	draw_vector(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset(1);
	}

	BurnWatchdogUpdate();

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		if (DrvJoy4[0]) DrvDial += 40; // p1
		if (DrvJoy4[1]) DrvDial -= 40;

		if (DrvJoy4[2]) DrvDial += 40; // p2
		if (DrvJoy4[3]) DrvDial -= 40;
	}

	INT32 nInterleave = (nM6502Count > 1) ? 300 : 100; // irq ~100x per frame
	INT32 nCyclesTotal[2] = { 2500000 / 50, 1250000 / 50 };
	INT32 nCyclesDone[2] = { nExtraCycles[0], nExtraCycles[1] };
	INT32 nSoundBufferPos = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		M6502Open(0);
		nCyclesDone[0] += M6502Run((nCyclesTotal[0] * (i + 1) / nInterleave) - nCyclesDone[0]);

		if (alpha_irq_clock_enable && ((i%3)==2))
		{
			alpha_irq_clock++;
			if ((alpha_irq_clock & 0x0c) == 0x0c)
			{
				M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
				alpha_irq_clock_enable = 0;
			}
		}

		M6502Close();

		if (nM6502Count > 1)
		{
			M6502Open(1);
		
			if (gamma_halt) {
				nCyclesDone[1] += (nCyclesTotal[1] * (i + 1) / nInterleave) - nCyclesDone[1];
				M6502Idle((nCyclesTotal[1] * (i + 1) / nInterleave) - nCyclesDone[1]);
			} else {
				nCyclesDone[1] += M6502Run((nCyclesTotal[1] * (i + 1) / nInterleave) - nCyclesDone[1]);
			}

			if ((i%3)==2) {
				gamma_irq_clock++;
				M6502SetIRQLine(0, (gamma_irq_clock & 0x08) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
			}

			M6502Close();
		}
	
		// Render Sound Segment
		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			pokey_update(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}

	}

	nExtraCycles[0] = nCyclesDone[0] - nCyclesTotal[0];
	nExtraCycles[1] = nCyclesDone[1] - nCyclesTotal[1];

	// Make sure the buffer is entirely filled.
	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			pokey_update(pSoundBuf, nSegmentLength);
		}
		tms5220_update(pBurnSoundOut, nBurnSoundLen);
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
		*pnMin = 0x029722;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		M6502Scan(nAction);

		avgdvg_scan(nAction, pnMin);
		BurnWatchdogScan(nAction);

		pokey_scan(nAction, pnMin);
		tms5220_scan(nAction, pnMin);

		SCAN_VAR(nExtraCycles);
		SCAN_VAR(alpha_irq_clock);
		SCAN_VAR(alpha_irq_clock_enable);
		SCAN_VAR(alpha_rombank);
		SCAN_VAR(alpha_rambank);
		SCAN_VAR(alpha_xmtd);
		SCAN_VAR(alpha_data);
		SCAN_VAR(alpha_rcvd);
		SCAN_VAR(gamma_xmtd);
		SCAN_VAR(gamma_data);
		SCAN_VAR(gamma_rcvd);
		SCAN_VAR(gamma_irq_clock);
		SCAN_VAR(gamma_halt);
		SCAN_VAR(player_1);
		SCAN_VAR(speech_data);
	}

	if (nAction & ACB_NVRAM) {
		ba.Data		= DrvNVRAM;
		ba.nLen		= 0x000200;
		ba.szName	= "NV RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_WRITE)
	{
		M6502Open(0);
		bankswitch(alpha_rombank);
		rambankswitch(alpha_rambank);
		M6502Close();
	}

	return 0;
}


// Major Havoc (rev 3)

static struct BurnRomInfo mhavocRomDesc[] = {
	{ "136025.210",		0x2000, 0xc67284ca, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 #0 Code & Vector Data
	{ "136025.216",		0x4000, 0x522a9cc0, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136025.217",		0x4000, 0xea3d6877, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136025.215",		0x4000, 0xa4d380ca, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136025.318",		0x4000, 0xba935067, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136025.106",		0x4000, 0x2ca83c76, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136025.107",		0x4000, 0x5f81c5f3, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "136025.108",		0x4000, 0x93faf210, 2 | BRF_PRG | BRF_ESS }, //  7 M6502 #1 Code

	{ "036408-01.b1",	0x0100, 0x5903af03, 3 | BRF_GRA },           //  8 AVG PROM
};

STD_ROM_PICK(mhavoc)
STD_ROM_FN(mhavoc)

struct BurnDriver BurnDrvMhavoc = {
	"mhavoc", NULL, NULL, NULL, "1983",
	"Major Havoc (rev 3)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, mhavocRomInfo, mhavocRomName, NULL, NULL, NULL, NULL, MhavocInputInfo, MhavocDIPInfo,
	MhavocInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	800, 600, 4, 3
};


// Major Havoc (rev 2)

static struct BurnRomInfo mhavoc2RomDesc[] = {
	{ "136025.110",		0x2000, 0x16eef583, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 #0 Code & Vector Data
	{ "136025.103",		0x4000, 0xbf192284, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136025.104",		0x4000, 0x833c5d4e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136025.101",		0x4000, 0x2b3b591f, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136025.109",		0x4000, 0x4d766827, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136025.106",		0x4000, 0x2ca83c76, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136025.107",		0x4000, 0x5f81c5f3, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "136025.108",		0x4000, 0x93faf210, 2 | BRF_PRG | BRF_ESS }, //  7 M6502 #1 Code

	{ "036408-01.b1",	0x0100, 0x5903af03, 3 | BRF_GRA },           //  8 AVG PROM
};

STD_ROM_PICK(mhavoc2)
STD_ROM_FN(mhavoc2)

struct BurnDriver BurnDrvMhavoc2 = {
	"mhavoc2", "mhavoc", NULL, NULL, "1983",
	"Major Havoc (rev 2)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, mhavoc2RomInfo, mhavoc2RomName, NULL, NULL, NULL, NULL, MhavocInputInfo, MhavocDIPInfo,
	MhavocInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	800, 600, 4, 3
};


// Major Havoc - Return to Vax

static struct BurnRomInfo mhavocrvRomDesc[] = {
	{ "136025.210",		0x2000, 0xc67284ca, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 #0 Code & Vector Data
	{ "136025.916",		0x4000, 0x1255bd7f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136025.917",		0x4000, 0x21889079, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136025.915",		0x4000, 0x4c7235dc, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136025.918",		0x4000, 0x84735445, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136025.106",		0x4000, 0x2ca83c76, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136025.907",		0x4000, 0x4deea2c9, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "136025.908",		0x4000, 0xc52ec664, 2 | BRF_PRG | BRF_ESS }, //  7 M6502 #1 Code

	{ "036408-01.b1",	0x0100, 0x5903af03, 3 | BRF_GRA },           //  8 AVG PROM
};

STD_ROM_PICK(mhavocrv)
STD_ROM_FN(mhavocrv)

struct BurnDriver BurnDrvMhavocrv = {
	"mhavocrv", "mhavoc", NULL, NULL, "2006",
	"Major Havoc - Return to Vax\0", NULL, "hack (JMA)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PLATFORM, 0,
	NULL, mhavocrvRomInfo, mhavocrvRomName, NULL, NULL, NULL, NULL, MhavocInputInfo, MhavocDIPInfo,
	MhavocInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	800, 600, 4, 3
};


// Major Havoc (prototype)

static struct BurnRomInfo mhavocpRomDesc[] = {
	{ "136025.010",		0x2000, 0x3050c0e6, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 #0 Code & Vector Data
	{ "136025.016",		0x4000, 0x94caf6c0, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136025.017",		0x4000, 0x05cba70a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136025.015",		0x4000, 0xc567c11b, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136025.018",		0x4000, 0xa8c35ccd, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136025.006",		0x4000, 0xe272ed41, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136025.007",		0x4000, 0xe152c9d8, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "136025.008",		0x4000, 0x22ea7399, 2 | BRF_PRG | BRF_ESS }, //  7 M6502 #1 Code

	{ "036408-01.b1",	0x0100, 0x5903af03, 3 | BRF_GRA },           //  8 AVG PROM
};

STD_ROM_PICK(mhavocp)
STD_ROM_FN(mhavocp)

struct BurnDriver BurnDrvMhavocp = {
	"mhavocp", "mhavoc", NULL, NULL, "1983",
	"Major Havoc (prototype)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, mhavocpRomInfo, mhavocpRomName, NULL, NULL, NULL, NULL, MhavocInputInfo, MhavocpDIPInfo,
	MhavocInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	800, 600, 4, 3
};



// Alpha One (prototype, 3 lives)

static struct BurnRomInfo alphaoneRomDesc[] = {
	{ "vec5000.tw",		0x1000, 0x2a4c149f, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code & Vector Data
	{ "8000.tw",		0x2000, 0x962d4da2, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a000.tw",		0x2000, 0xf739a791, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "twjk1.bin",		0x2000, 0x1ead0b34, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "e000.tw",		0x1000, 0x6b1d7d2b, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "page01.tw",		0x4000, 0xcbf3b05a, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "vec_pg01.tw",	0x4000, 0xe392a94d, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "vec_pg23.tw",	0x4000, 0x1ff74292, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "036408-01.b1",	0x0100, 0x5903af03, 3 | BRF_GRA },           //  8 AVG PROM
};

STD_ROM_PICK(alphaone)
STD_ROM_FN(alphaone)

struct BurnDriver BurnDrvAlphaone = {
	"alphaone", "mhavoc", NULL, NULL, "1983",
	"Alpha One (prototype, 3 lives)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, alphaoneRomInfo, alphaoneRomName, NULL, NULL, NULL, NULL, AlphaoneInputInfo, AlphaoneDIPInfo,
	AlphaoneInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	800, 600, 4, 3
};


// Alpha One (prototype, 5 lives)

static struct BurnRomInfo alphaoneaRomDesc[] = {
	{ "vec5000.tw",		0x1000, 0x2a4c149f, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code & Vector Data
	{ "8000.tw",		0x2000, 0x962d4da2, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a000.tw",		0x2000, 0xf739a791, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "c000.tw",		0x2000, 0xf21fb1ac, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "e000.tw",		0x1000, 0x6b1d7d2b, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "page01.tw",		0x4000, 0xcbf3b05a, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "vec_pg01.tw",	0x4000, 0xe392a94d, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "vec_pg23.tw",	0x4000, 0x1ff74292, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "136002-125.6c",	0x0100, 0x5903af03, 3 | BRF_GRA },           //  8 AVG PROM
};

STD_ROM_PICK(alphaonea)
STD_ROM_FN(alphaonea)

struct BurnDriver BurnDrvAlphaonea = {
	"alphaonea", "mhavoc", NULL, NULL, "1983",
	"Alpha One (prototype, 5 lives)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, alphaoneaRomInfo, alphaoneaRomName, NULL, NULL, NULL, NULL, AlphaoneInputInfo, AlphaoneDIPInfo,
	AlphaoneInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	800, 600, 4, 3
};
