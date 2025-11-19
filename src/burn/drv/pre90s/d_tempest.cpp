// FB Alpha tempest driver module
// Based on MAME driver by Brad Oliver, Bernd Wiebelt, Allard van der Bas

#include "tiles_generic.h"
#include "m6502_intf.h"
#include "burn_gun.h"
#include "mathbox.h"
#include "vector.h"
#include "avgdvg.h"
#include "pokey.h"
#include "watchdog.h"
#include "earom.h"
#include "dtimer.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6502ROM;
static UINT8 *DrvAVGPROM;
static UINT8 *DrvM6502RAM;
static UINT8 *DrvVecRAM;
static UINT8 *DrvVecROM;
static UINT8 *DrvColRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 nExtraCycles;

static UINT8 DrvJoy1[8] =   { 0, 0, 0, 0, 0, 0, 0, 0 };
static UINT8 DrvJoy3[8] =   { 0, 0, 0, 0, 0, 0, 0, 0 };
static UINT8 DrvJoy4f[8] =  { 0, 0, 0, 0, 0, 0, 0, 0 };
static UINT8 DrvDips[5] =   { 0, 0, 0, 0, 0 };
static UINT8 DrvInputs[3] = { 0, 0, 0 };
static UINT8 DrvReset;

static INT16 Analog[2];

static dtimer irq_timer;

static UINT8 player = 0;

static UINT32 load_type = 0;

static INT32 scanline;

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo TempestInputList[] = {
	{"P1 Coin",		    BIT_DIGITAL,	DrvJoy1 + 2,	"p1 coin"		},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 start"		},
	{"P1 Left",		    BIT_DIGITAL,	DrvJoy4f+ 0,	"p1 left"		},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy4f+ 1,	"p1 right"		},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 fire 1"		},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 fire 2"		},
	A("P1 Spinner",     BIT_ANALOG_REL, &Analog[0],		"p1 x-axis"		),

	{"P2 Coin",		    BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"		},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 start"		},
	{"P2 Left",		    BIT_DIGITAL,	DrvJoy4f+ 2,	"p2 left"		},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy4f+ 3,	"p2 right"		},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy4f+ 4,	"p2 fire 1"		},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy4f+ 5,	"p2 fire 2"		},
	A("P2 Spinner",     BIT_ANALOG_REL, &Analog[1],		"p2 x-axis"		),

	{"Reset",		        BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Diagnostic Step",		BIT_DIGITAL,	DrvJoy1 + 5,	"service2"	},
	{"Tilt",		        BIT_DIGITAL,	DrvJoy1 + 3,	"tilt"		},
	{"Dip A",		        BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		        BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		        BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",		        BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
	{"Dip E",		        BIT_DIPSWITCH,	DrvDips + 4,	"dip"		},
};
#undef A
STDINPUTINFO(Tempest)

static struct BurnDIPInfo TempestDIPList[]=
{
	DIP_OFFSET(0x11)
	{0x00, 0xff, 0xff, 0x10, NULL				    },
	{0x01, 0xff, 0xff, 0x07, NULL				    },
	{0x02, 0xff, 0xff, 0x00, NULL				    },
	{0x03, 0xff, 0xff, 0x00, NULL				    },
	{0x04, 0xff, 0xff, 0x10, NULL				    },
	{0x05, 0xff, 0xff, 0x00, NULL				    },

	{0   , 0xfe, 0   ,    2, "Cabinet"			    },
	{0x00, 0x01, 0x10, 0x10, "Upright"			    },
	{0x00, 0x01, 0x10, 0x00, "Cocktail"			    },

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x01, 0x01, 0x03, 0x02, "Easy"				    },
	{0x01, 0x01, 0x03, 0x03, "Medium1"			    },
	{0x01, 0x01, 0x03, 0x00, "Medium2"			    },
	{0x01, 0x01, 0x03, 0x01, "Hard"					},

	{0   , 0xfe, 0   ,    2, "Rating"			    },
	{0x01, 0x01, 0x04, 0x04, "1, 3, 5, 7, 9"		},
	{0x01, 0x01, 0x04, 0x00, "tied to high score"	},

	{0   , 0xfe, 0   ,    4, "Coinage"			    },
	{0x02, 0x01, 0x03, 0x01, "2 Coins 1 Credits"	},
	{0x02, 0x01, 0x03, 0x00, "1 Coin  1 Credits"	},
	{0x02, 0x01, 0x03, 0x03, "1 Coin  2 Credits"	},
	{0x02, 0x01, 0x03, 0x02, "Free Play"			},

	{0   , 0xfe, 0   ,    4, "Right Coin"			},
	{0x02, 0x01, 0x0c, 0x00, "*1"				    },
	{0x02, 0x01, 0x0c, 0x04, "*4"				    },
	{0x02, 0x01, 0x0c, 0x08, "*5"				    },
	{0x02, 0x01, 0x0c, 0x0c, "*6"				    },

	{0   , 0xfe, 0   ,    2, "Left Coin"			},
	{0x02, 0x01, 0x10, 0x00, "*1"				    },
	{0x02, 0x01, 0x10, 0x10, "*2"				    },

	{0   , 0xfe, 0   ,    8, "Bonus Coins"			},
	{0x02, 0x01, 0xe0, 0x00, "None"				    },
	{0x02, 0x01, 0xe0, 0x80, "1 each 5"			    },
	{0x02, 0x01, 0xe0, 0x40, "1 each 4 (+Demo)"		},
	{0x02, 0x01, 0xe0, 0xa0, "1 each 3"			    },
	{0x02, 0x01, 0xe0, 0x60, "2 each 4 (+Demo)"		},
	{0x02, 0x01, 0xe0, 0x20, "1 each 2"			    },
	{0x02, 0x01, 0xe0, 0xc0, "Freeze Mode"			},
	{0x02, 0x01, 0xe0, 0xe0, "Freeze Mode"			},

	{0   , 0xfe, 0   ,    2, "Minimum"			    },
	{0x03, 0x01, 0x01, 0x00, "1 Credit"			    },
	{0x03, 0x01, 0x01, 0x01, "2 Credit"			    },

	{0   , 0xfe, 0   ,    4, "Language"			    },
	{0x03, 0x01, 0x06, 0x00, "English"			    },
	{0x03, 0x01, 0x06, 0x02, "French"			    },
	{0x03, 0x01, 0x06, 0x04, "German"			    },
	{0x03, 0x01, 0x06, 0x06, "Spanish"			    },

	{0   , 0xfe, 0   ,    8, "Bonus Life"			},
	{0x03, 0x01, 0x38, 0x08, "10000"			    },
	{0x03, 0x01, 0x38, 0x00, "20000"			    },
	{0x03, 0x01, 0x38, 0x10, "30000"			    },
	{0x03, 0x01, 0x38, 0x18, "40000"			    },
	{0x03, 0x01, 0x38, 0x20, "50000"			    },
	{0x03, 0x01, 0x38, 0x28, "60000"			    },
	{0x03, 0x01, 0x38, 0x30, "70000"			    },
	{0x03, 0x01, 0x38, 0x38, "None"				    },

	{0   , 0xfe, 0   ,    4, "Lives"			    },
	{0x03, 0x01, 0xc0, 0xc0, "2"				    },
	{0x03, 0x01, 0xc0, 0x00, "3"				    },
	{0x03, 0x01, 0xc0, 0x40, "4"				    },
	{0x03, 0x01, 0xc0, 0x80, "5"				    },

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x04, 0x01, 0x10, 0x10, "Off"  		        },
	{0x04, 0x01, 0x10, 0x00, "On"   	            },
};

STDDIPINFO(Tempest)

static UINT8 tempest_read(UINT16 address)
{
	if (address >= 0x60c0 && address <= 0x60cf) {
		return pokey1_r(address & 0x0f);
	}

	if (address >= 0x60d0 && address <= 0x60df) {
		return pokey2_r(address & 0x0f);
	}

	switch (address)
	{
		case 0x0c00: {
			UINT8 temp = DrvInputs[0] & 0x3f;
			if (avgdvg_done()) temp |= 0x40;
			if (M6502TotalCycles() & 0x100) temp |= 0x80;
			return temp;
		}

		case 0x0d00:
			return DrvDips[2];

		case 0x0e00:
			return DrvDips[3];

		case 0x6040:
			return mathbox_status_read();

		case 0x6050:
			return earom_read(address);

		case 0x6060:
			return mathbox_lo_read();

		case 0x6070:
			return mathbox_hi_read();
	}

	return 0;
}

static void tempest_write(UINT16 address, UINT8 data)
{
	if (address >= 0x0800 && address <= 0x080f) {
		DrvColRAM[address & 0x0f] = data;
		DrvRecalc = 1;
		return;
	}

	if (address >= 0x60c0 && address <= 0x60cf) {
		pokey1_w(address & 0x0f, data);
		return;
	}

	if (address >= 0x60d0 && address <= 0x60df) {
		pokey2_w(address & 0x0f, data);
		return;
	}
	if (address >= 0x6000 && address <= 0x603f) {
		earom_write(address & 0x3f, data);
		return;
	}

	if (address >= 0x6080 && address <= 0x609f) {
		mathbox_go_write(address & 0x1f, data);
		return;
	}

	switch (address)
	{
		case 0x4000:
			avg_set_flip_x(data & 0x08);
			avg_set_flip_y(data & 0x10);
		return;

		case 0x4800:
			avgdvg_go();
		return;

		case 0x5000:
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
			BurnWatchdogRead();
		return;

		case 0x5800:
			avgdvg_reset();
		return;

		case 0x6040:
			earom_ctrl_write(address, data);
		return;

		case 0x60e0:
			player = (data & 0x04) >> 2;
		return;
	}
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	M6502Open(0);
	M6502Reset();
	M6502Close();

	timerReset();

	irq_timer.start(1512000 / (12096000/4096/12), -1, 1, 1); // @cycle, default param, start now, re-occuring

	PokeyReset();

	BurnWatchdogReset();

	mathbox_reset();
	avgdvg_reset();

	earom_reset();

	nExtraCycles = 0;

	HiscoreReset();

	return 0;
}

static INT32 port1_read(INT32 offset)
{
	DrvInputs[1] = (DrvDips[0] & 0x10) | (BurnTrackballReadInterpolated(0, player, scanline) & 0x0f);

	return (DrvInputs[1] & (1 << (offset & 7))) ? 0 : 228;
}

static INT32 port2_read(INT32 offset)
{
	return (DrvInputs[2] & (1 << (offset & 7))) ? 0 : 228;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6502ROM		= Next; Next += 0x010000;

	DrvAVGPROM		= Next; Next += 0x000100;

	DrvPalette		= (UINT32*)Next; Next += 0x0040 * 256 * sizeof(UINT32);

	AllRam			= Next;

	DrvM6502RAM		= Next; Next += 0x000800;
	DrvColRAM		= Next; Next += 0x000010;
	DrvVecRAM       = Next; Next += 0x001000;

	RamEnd			= Next;

	DrvVecROM       = Next; Next += 0x001000; // must(!) come after DrvVecRAM

	MemEnd			= Next;

	return 0;
}

static void irq_timer_cb(INT32 param)
{
	M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		if (load_type == 0)
		{
			if (BurnLoadRom(DrvM6502ROM + 0x9000,  0, 1)) return 1;
			if (BurnLoadRom(DrvM6502ROM + 0xa000,  1, 1)) return 1;
			if (BurnLoadRom(DrvM6502ROM + 0xb000,  2, 1)) return 1;
			if (BurnLoadRom(DrvM6502ROM + 0xc000,  3, 1)) return 1;
			if (BurnLoadRom(DrvM6502ROM + 0xd000,  4, 1)) return 1;
			if (BurnLoadRom(DrvM6502ROM + 0xf000,  4, 1)) return 1;

			if (BurnLoadRom(DrvVecROM   + 0x0000,  5, 1)) return 1;

			if (BurnLoadRom(DrvAVGPROM  + 0x0000,  6, 1)) return 1;
		}
		else if (load_type == 1)
		{
			if (BurnLoadRom(DrvM6502ROM + 0x9000,  0, 1)) return 1;
			if (BurnLoadRom(DrvM6502ROM + 0x9800,  1, 1)) return 1;
			if (BurnLoadRom(DrvM6502ROM + 0xa000,  2, 1)) return 1;
			if (BurnLoadRom(DrvM6502ROM + 0xa800,  3, 1)) return 1;
			if (BurnLoadRom(DrvM6502ROM + 0xb000,  4, 1)) return 1;
			if (BurnLoadRom(DrvM6502ROM + 0xb800,  5, 1)) return 1;
			if (BurnLoadRom(DrvM6502ROM + 0xc000,  6, 1)) return 1;
			if (BurnLoadRom(DrvM6502ROM + 0xc800,  7, 1)) return 1;
			if (BurnLoadRom(DrvM6502ROM + 0xd000,  8, 1)) return 1;
			if (BurnLoadRom(DrvM6502ROM + 0xd800,  9, 1)) return 1;
			if (BurnLoadRom(DrvM6502ROM + 0xf800,  9, 1)) return 1;

			if (BurnLoadRom(DrvVecROM   + 0x0000, 10, 1)) return 1;
			if (BurnLoadRom(DrvVecROM   + 0x0800, 11, 1)) return 1;

			if (BurnLoadRom(DrvAVGPROM  + 0x0000, 12, 1)) return 1;
		}
		else if (load_type == 2)
		{
			if (BurnLoadRom(DrvM6502ROM + 0x9000,  0, 1)) return 1;
			if (BurnLoadRom(DrvM6502ROM + 0x9800,  1, 1)) return 1;
			if (BurnLoadRom(DrvM6502ROM + 0xa000,  2, 1)) return 1;
			if (BurnLoadRom(DrvM6502ROM + 0xa800,  3, 1)) return 1;
			if (BurnLoadRom(DrvM6502ROM + 0xd800,  4, 1)) return 1;
			if (BurnLoadRom(DrvM6502ROM + 0xf800,  4, 1)) return 1;

			if (BurnLoadRom(DrvVecROM   + 0x0000,  5, 1)) return 1;
			if (BurnLoadRom(DrvVecROM   + 0x0800,  6, 1)) return 1;

			if (BurnLoadRom(DrvAVGPROM  + 0x0000,  7, 1)) return 1;
		}
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvM6502RAM,		        0x0000, 0x07ff, MAP_RAM);
	M6502MapMemory(DrvVecRAM,		        0x2000, 0x2fff, MAP_RAM);
	M6502MapMemory(DrvVecROM,               0x3000, 0x3fff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM + 0x9000,	0x9000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(tempest_write);
	M6502SetReadHandler(tempest_read);
	M6502Close();

	BurnWatchdogInit(DrvDoReset, 180);

	PokeyInit(12096000/8, 2, 1.40, 0);
	PokeySetTotalCyclesCB(M6502TotalCycles);
	PokeyPotCallback(0, 0, port1_read);
	PokeyPotCallback(0, 1, port1_read);
	PokeyPotCallback(0, 2, port1_read);
	PokeyPotCallback(0, 3, port1_read);
	PokeyPotCallback(0, 4, port1_read);
	PokeyPotCallback(0, 5, port1_read);
	PokeyPotCallback(0, 6, port1_read);
	PokeyPotCallback(0, 7, port1_read);

	PokeyPotCallback(1, 0, port2_read);
	PokeyPotCallback(1, 1, port2_read);
	PokeyPotCallback(1, 2, port2_read);
	PokeyPotCallback(1, 3, port2_read);
	PokeyPotCallback(1, 4, port2_read);
	PokeyPotCallback(1, 5, port2_read);
	PokeyPotCallback(1, 6, port2_read);
	PokeyPotCallback(1, 7, port2_read);

	avgdvg_init(USE_AVG_TEMPEST, DrvVecRAM, 0x2000, M6502TotalCycles, 580, 570);

	earom_init();

	BurnTrackballInit(2);

	timerInit();
	timerAdd(irq_timer, 0, irq_timer_cb);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	avgdvg_exit();

	PokeyExit();
	M6502Exit();
	timerExit();

	load_type = 0;

	BurnTrackballExit();
	// mathboxexit() ?
	earom_exit();

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteInit()
{
    for (INT32 i = 0; i < 0x40; i++) // color
	{
		for (INT32 j = 0; j < 256; j++) // intensity
		{
			UINT8 data = DrvColRAM[i];
			INT32 bit3 = (~data >> 3) & 1;
			INT32 bit2 = (~data >> 2) & 1;
			INT32 bit1 = (~data >> 1) & 1;
			INT32 bit0 = (~data >> 0) & 1;
			INT32 r = (bit1 * 0xee + bit0 * 0x11) * j / 255;
			INT32 g = (bit3 * 0xee) * j / 255;
			INT32 b = (bit2 * 0xee) * j / 255;

			DrvPalette[i * 256 + j] = (r << 16) | (g << 8) | b; // must be 32bit palette! -dink (see vector.cpp)
		}
	}
}


static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
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
		if (player) { // player2
			DrvJoy3[4] = DrvJoy4f[4];
			DrvJoy3[3] = DrvJoy4f[3];
		}

		memset (DrvInputs, 0xff, sizeof(DrvInputs));
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		BurnTrackballConfig(0, AXIS_NORMAL, AXIS_NORMAL);
		BurnTrackballFrame(0, Analog[0], Analog[1], 0x01, 0x0f, 20);
		BurnTrackballUDLR(0, DrvJoy4f[2], DrvJoy4f[3], DrvJoy4f[0], DrvJoy4f[1], 5);
		BurnTrackballUpdate(0);

		DrvInputs[0] = (DrvInputs[0] & 0x2f) | (DrvDips[4] & 0x10); // service mode
		DrvInputs[2] = (DrvInputs[2] & 0xf8) | (DrvDips[1] & 0x07);
	}

	INT32 nInterleave = 20;
	INT32 nCyclesTotal[2] = { 1512000 / 60, 1512000 / 60 };
	INT32 nCyclesDone[2] = { nExtraCycles };
	INT32 nSoundBufferPos = 0;

	M6502Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		scanline = i;

		CPU_RUN(0, M6502);
		CPU_RUN(1, timer);

		// Render Sound Segment
		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			pokey_update(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}
	M6502Close();

	nExtraCycles = nCyclesDone - nCyclesTotal;

	// Make sure the buffer is entirely filled.
	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			pokey_update(pSoundBuf, nSegmentLength);
		}
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
		*pnMin = 0x029722;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		M6502Scan(nAction);
		timerScan();

		avgdvg_scan(nAction, pnMin);
		mathbox_scan(nAction, pnMin);

		BurnWatchdogScan(nAction);

		pokey_scan(nAction, pnMin);

		BurnTrackballScan();

		SCAN_VAR(nExtraCycles);
	}

	earom_scan(nAction, pnMin); // here.

	return 0;
}

// Tempest (rev 3, Revised Hardware)

static struct BurnRomInfo tempestRomDesc[] = {
	{ "136002-133.d1",	0x1000, 0x1d0cc503, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "136002-134.f1",	0x1000, 0xc88e3524, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136002-235.j1",	0x1000, 0xa4b2ce3f, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136002-136.lm1",	0x1000, 0x65a9a9f9, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136002-237.p1",	0x1000, 0xde4e9e34, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "136002-138.np3",	0x1000, 0x9995256d, 1 | BRF_PRG | BRF_ESS }, //  5 vectrom

	{ "136002-125.d7",	0x0100, 0x5903af03, 2 | BRF_GRA },           //  6 avg prom

	{ "136002-126.a1",	0x0020, 0x8b04f921, 3 | BRF_GRA },           //  7 mathbox prom

	{ "136002-132.l1",	0x0100, 0x2af82e87, 4 | BRF_GRA },           //  8 user3
	{ "136002-131.k1",	0x0100, 0xb31f6e24, 4 | BRF_GRA },           //  9
	{ "136002-130.j1",	0x0100, 0x8119b847, 4 | BRF_GRA },           // 10
	{ "136002-129.h1",	0x0100, 0x09f5a4d5, 4 | BRF_GRA },           // 11
	{ "136002-128.f1",	0x0100, 0x823b61ae, 4 | BRF_GRA },           // 12
	{ "136002-127.e1",	0x0100, 0x276eadd5, 4 | BRF_GRA },           // 13
};

STD_ROM_PICK(tempest)
STD_ROM_FN(tempest)

struct BurnDriver BurnDrvTempest = {
	"tempest", NULL, NULL, NULL, "1980",
	"Tempest (rev 3, Revised Hardware)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_ACTION | GBF_VECTOR, 0,
	NULL, tempestRomInfo, tempestRomName, NULL, NULL, NULL, NULL, TempestInputInfo, TempestDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40 * 256,
	480, 640, 3, 4
};


// Tempest (rev 1, Revised Hardware)

static struct BurnRomInfo tempest1rRomDesc[] = {
	{ "136002-133.d1",	0x1000, 0x1d0cc503, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "136002-134.f1",	0x1000, 0xc88e3524, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136002-135.j1",	0x1000, 0x1ca27781, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136002-136.lm1",	0x1000, 0x65a9a9f9, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136002-137.p1",	0x1000, 0xd75fd2ef, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136002-138.np3",	0x1000, 0x9995256d, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "136002-125.d7",	0x0100, 0x5903af03, 2 | BRF_GRA },           //  6 user1

	{ "136002-126.a1",	0x0020, 0x8b04f921, 3 | BRF_GRA },           //  7 mathbox prom

	{ "136002-132.l1",	0x0100, 0x2af82e87, 4 | BRF_GRA },           //  8 user3
	{ "136002-131.k1",	0x0100, 0xb31f6e24, 4 | BRF_GRA },           //  9
	{ "136002-130.j1",	0x0100, 0x8119b847, 4 | BRF_GRA },           // 10
	{ "136002-129.h1",	0x0100, 0x09f5a4d5, 4 | BRF_GRA },           // 11
	{ "136002-128.f1",	0x0100, 0x823b61ae, 4 | BRF_GRA },           // 12
	{ "136002-127.e1",	0x0100, 0x276eadd5, 4 | BRF_GRA },           // 13
};

STD_ROM_PICK(tempest1r)
STD_ROM_FN(tempest1r)

struct BurnDriver BurnDrvTempest1r = {
	"tempest1r", "tempest", NULL, NULL, "1980",
	"Tempest (rev 1, Revised Hardware)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_ACTION | GBF_VECTOR, 0,
	NULL, tempest1rRomInfo, tempest1rRomName, NULL, NULL, NULL, NULL, TempestInputInfo, TempestDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40 * 256,
	480, 640, 3, 4
};

static INT32 DrvInitSmall()
{
	load_type = 1;

	return DrvInit();
}

// Tempest (rev 3)

static struct BurnRomInfo tempest3RomDesc[] = {
	{ "136002-113.d1",	0x0800, 0x65d61fe7, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "136002-114.e1",	0x0800, 0x11077375, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136002-115.f1",	0x0800, 0xf3e2827a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136002-316.h1",	0x0800, 0xaeb0f7e9, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136002-217.j1",	0x0800, 0xef2eb645, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136002-118.k1",	0x0800, 0xbeb352ab, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136002-119.lm1",	0x0800, 0xa4de050f, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136002-120.mn1",	0x0800, 0x35619648, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "136002-121.p1",	0x0800, 0x73d38e47, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "136002-222.r1",	0x0800, 0x707bd5c3, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "136002-123.np3",	0x0800, 0x29f7e937, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "136002-124.r3",	0x0800, 0xc16ec351, 1 | BRF_PRG | BRF_ESS }, // 11

	{ "136002-125.d7",	0x0100, 0x5903af03, 2 | BRF_GRA },           // 12 user1

	{ "136002-126.a1",	0x0020, 0x8b04f921, 3 | BRF_GRA },           // 13 mathbox prom

	{ "136002-132.l1",	0x0100, 0x2af82e87, 4 | BRF_GRA },           // 14 user3
	{ "136002-131.k1",	0x0100, 0xb31f6e24, 4 | BRF_GRA },           // 15
	{ "136002-130.j1",	0x0100, 0x8119b847, 4 | BRF_GRA },           // 16
	{ "136002-129.h1",	0x0100, 0x09f5a4d5, 4 | BRF_GRA },           // 17
	{ "136002-128.f1",	0x0100, 0x823b61ae, 4 | BRF_GRA },           // 18
	{ "136002-127.e1",	0x0100, 0x276eadd5, 4 | BRF_GRA },           // 19
};

STD_ROM_PICK(tempest3)
STD_ROM_FN(tempest3)

struct BurnDriver BurnDrvTempest3 = {
	"tempest3", "tempest", NULL, NULL, "1980",
	"Tempest (rev 3)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_ACTION | GBF_VECTOR, 0,
	NULL, tempest3RomInfo, tempest3RomName, NULL, NULL, NULL, NULL, TempestInputInfo, TempestDIPInfo,
	DrvInitSmall, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40 * 256,
	480, 640, 3, 4
};


// Tempest (rev 2)

static struct BurnRomInfo tempest2RomDesc[] = {
	{ "136002-113.d1",	0x0800, 0x65d61fe7, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "136002-114.e1",	0x0800, 0x11077375, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136002-115.f1",	0x0800, 0xf3e2827a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136002-116.h1",	0x0800, 0x7356896c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136002-217.j1",	0x0800, 0xef2eb645, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136002-118.k1",	0x0800, 0xbeb352ab, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136002-119.lm1",	0x0800, 0xa4de050f, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136002-120.mn1",	0x0800, 0x35619648, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "136002-121.p1",	0x0800, 0x73d38e47, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "136002-222.r1",	0x0800, 0x707bd5c3, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "136002-123.np3",	0x0800, 0x29f7e937, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "136002-124.r3",	0x0800, 0xc16ec351, 1 | BRF_PRG | BRF_ESS }, // 11

	{ "136002-125.d7",	0x0100, 0x5903af03, 2 | BRF_GRA },           // 12 user1

	{ "136002-126.a1",	0x0020, 0x8b04f921, 3 | BRF_GRA },           // 13 mathbox prom

	{ "136002-132.l1",	0x0100, 0x2af82e87, 4 | BRF_GRA },           // 14 user3
	{ "136002-131.k1",	0x0100, 0xb31f6e24, 4 | BRF_GRA },           // 15
	{ "136002-130.j1",	0x0100, 0x8119b847, 4 | BRF_GRA },           // 16
	{ "136002-129.h1",	0x0100, 0x09f5a4d5, 4 | BRF_GRA },           // 17
	{ "136002-128.f1",	0x0100, 0x823b61ae, 4 | BRF_GRA },           // 18
	{ "136002-127.e1",	0x0100, 0x276eadd5, 4 | BRF_GRA },           // 19
};

STD_ROM_PICK(tempest2)
STD_ROM_FN(tempest2)

struct BurnDriver BurnDrvTempest2 = {
	"tempest2", "tempest", NULL, NULL, "1980",
	"Tempest (rev 2)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_ACTION | GBF_VECTOR, 0,
	NULL, tempest2RomInfo, tempest2RomName, NULL, NULL, NULL, NULL, TempestInputInfo, TempestDIPInfo,
	DrvInitSmall, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40 * 256,
	480, 640, 3, 4
};


// Tempest (rev 1)

static struct BurnRomInfo tempest1RomDesc[] = {
	{ "136002-113.d1",	0x0800, 0x65d61fe7, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "136002-114.e1",	0x0800, 0x11077375, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136002-115.f1",	0x0800, 0xf3e2827a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136002-116.h1",	0x0800, 0x7356896c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136002-117.j1",	0x0800, 0x55952119, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136002-118.k1",	0x0800, 0xbeb352ab, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136002-119.lm1",	0x0800, 0xa4de050f, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136002-120.mn1",	0x0800, 0x35619648, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "136002-121.p1",	0x0800, 0x73d38e47, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "136002-122.r1",	0x0800, 0x796a9918, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "136002-123.np3",	0x0800, 0x29f7e937, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "136002-124.r3",	0x0800, 0xc16ec351, 1 | BRF_PRG | BRF_ESS }, // 11

	{ "136002-125.d7",	0x0100, 0x5903af03, 2 | BRF_GRA },           // 12 user1

	{ "136002-126.a1",	0x0020, 0x8b04f921, 3 | BRF_GRA },           // 13 mathbox prom

	{ "136002-132.l1",	0x0100, 0x2af82e87, 4 | BRF_GRA },           // 14 user3
	{ "136002-131.k1",	0x0100, 0xb31f6e24, 4 | BRF_GRA },           // 15
	{ "136002-130.j1",	0x0100, 0x8119b847, 4 | BRF_GRA },           // 16
	{ "136002-129.h1",	0x0100, 0x09f5a4d5, 4 | BRF_GRA },           // 17
	{ "136002-128.f1",	0x0100, 0x823b61ae, 4 | BRF_GRA },           // 18
	{ "136002-127.e1",	0x0100, 0x276eadd5, 4 | BRF_GRA },           // 19
};

STD_ROM_PICK(tempest1)
STD_ROM_FN(tempest1)

struct BurnDriver BurnDrvTempest1 = {
	"tempest1", "tempest", NULL, NULL, "1980",
	"Tempest (rev 1)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_ACTION | GBF_VECTOR, 0,
	NULL, tempest1RomInfo, tempest1RomName, NULL, NULL, NULL, NULL, TempestInputInfo, TempestDIPInfo,
	DrvInitSmall, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40 * 256,
	480, 640, 3, 4
};


// Tempest Tubes

static struct BurnRomInfo temptubeRomDesc[] = {
	{ "136002-113.d1",	0x0800, 0x65d61fe7, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "136002-114.e1",	0x0800, 0x11077375, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136002-115.f1",	0x0800, 0xf3e2827a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136002-316.h1",	0x0800, 0xaeb0f7e9, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136002-217.j1",	0x0800, 0xef2eb645, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "tube-118.k1",	0x0800, 0xcefb03f0, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136002-119.lm1",	0x0800, 0xa4de050f, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136002-120.mn1",	0x0800, 0x35619648, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "136002-121.p1",	0x0800, 0x73d38e47, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "136002-222.r1",	0x0800, 0x707bd5c3, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "136002-123.np3",	0x0800, 0x29f7e937, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "136002-124.r3",	0x0800, 0xc16ec351, 1 | BRF_PRG | BRF_ESS }, // 11

	{ "136002-125.d7",	0x0100, 0x5903af03, 2 | BRF_GRA },           // 12 user1

	{ "136002-126.a1",	0x0020, 0x8b04f921, 3 | BRF_GRA },           // 13 mathbox prom

	{ "136002-132.l1",	0x0100, 0x2af82e87, 4 | BRF_GRA },           // 14 user3
	{ "136002-131.k1",	0x0100, 0xb31f6e24, 4 | BRF_GRA },           // 15
	{ "136002-130.j1",	0x0100, 0x8119b847, 4 | BRF_GRA },           // 16
	{ "136002-129.h1",	0x0100, 0x09f5a4d5, 4 | BRF_GRA },           // 17
	{ "136002-128.f1",	0x0100, 0x823b61ae, 4 | BRF_GRA },           // 18
	{ "136002-127.e1",	0x0100, 0x276eadd5, 4 | BRF_GRA },           // 19
};

STD_ROM_PICK(temptube)
STD_ROM_FN(temptube)

struct BurnDriver BurnDrvTemptube = {
	"temptube", "tempest", NULL, NULL, "1980",
	"Tempest Tubes\0", NULL, "hack (Duncan Brown)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_ACTION | GBF_VECTOR, 0,
	NULL, temptubeRomInfo, temptubeRomName, NULL, NULL, NULL, NULL, TempestInputInfo, TempestDIPInfo,
	DrvInitSmall, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40 * 256,
	480, 640, 3, 4
};


// TempEd

static struct BurnRomInfo tempedRomDesc[] = {
	{ "136002-113.d1",	0x0800, 0x65d61fe7, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "136002-114.e1",	0x0800, 0x11077375, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136002-115.f1",	0x0800, 0xf3e2827a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136002-316.h1",	0x0800, 0xaeb0f7e9, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136002-217.j1",	0x0800, 0xef2eb645, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "temped.118",		0x0800, 0x4825ee42, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136002-119.lm1",	0x0800, 0xa4de050f, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136002-120.mn1",	0x0800, 0x35619648, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "136002-121.p1",	0x0800, 0x73d38e47, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "136002-222.r1",	0x0800, 0x707bd5c3, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "136002-123.np3",	0x0800, 0x29f7e937, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "136002-124.r3",	0x0800, 0xc16ec351, 1 | BRF_PRG | BRF_ESS }, // 11

	{ "136002-125.d7",	0x0100, 0x5903af03, 2 | BRF_GRA },           // 12 user1

	{ "136002-126.a1",	0x0020, 0x8b04f921, 3 | BRF_GRA },           // 13 mathbox prom

	{ "136002-132.l1",	0x0100, 0x2af82e87, 4 | BRF_GRA },           // 14 user3
	{ "136002-131.k1",	0x0100, 0xb31f6e24, 4 | BRF_GRA },           // 15
	{ "136002-130.j1",	0x0100, 0x8119b847, 4 | BRF_GRA },           // 16
	{ "136002-129.h1",	0x0100, 0x09f5a4d5, 4 | BRF_GRA },           // 17
	{ "136002-128.f1",	0x0100, 0x823b61ae, 4 | BRF_GRA },           // 18
	{ "136002-127.e1",	0x0100, 0x276eadd5, 4 | BRF_GRA },           // 19
};

STD_ROM_PICK(temped)
STD_ROM_FN(temped)

struct BurnDriver BurnDrvTemped = {
	"temped", "tempest", NULL, NULL, "2000",
	"TempEd\0", NULL, "hack", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HACK, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_ACTION | GBF_VECTOR, 0,
	NULL, tempedRomInfo, tempedRomName, NULL, NULL, NULL, NULL, TempestInputInfo, TempestDIPInfo,
	DrvInitSmall, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40 * 256,
	480, 640, 3, 4
};


// Tempest All Levels

static struct BurnRomInfo tempallRomDesc[] = {
	{ "tempall.113",	0x0800, 0xa9000137, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "136002-114.e1",	0x0800, 0x11077375, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136002-115.f1",	0x0800, 0xf3e2827a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136002-316.h1",	0x0800, 0xaeb0f7e9, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136002-217.j1",	0x0800, 0xef2eb645, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136002.118.k1",	0x0800, 0xbeb352ab, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136002-119.lm1",	0x0800, 0xa4de050f, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136002-120.mn1",	0x0800, 0x35619648, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "136002-121.p1",	0x0800, 0x73d38e47, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "136002-222.r1",	0x0800, 0x707bd5c3, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "136002-123.np3",	0x0800, 0x29f7e937, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "136002-124.r3",	0x0800, 0xc16ec351, 1 | BRF_PRG | BRF_ESS }, // 11

	{ "136002-125.d7",	0x0100, 0x5903af03, 2 | BRF_GRA },           // 12 user1

	{ "136002-126.a1",	0x0020, 0x8b04f921, 3 | BRF_GRA },           // 13 mathbox prom

	{ "136002-132.l1",	0x0100, 0x2af82e87, 4 | BRF_GRA },           // 14 user3
	{ "136002-131.k1",	0x0100, 0xb31f6e24, 4 | BRF_GRA },           // 15
	{ "136002-130.j1",	0x0100, 0x8119b847, 4 | BRF_GRA },           // 16
	{ "136002-129.h1",	0x0100, 0x09f5a4d5, 4 | BRF_GRA },           // 17
	{ "136002-128.f1",	0x0100, 0x823b61ae, 4 | BRF_GRA },           // 18
	{ "136002-127.e1",	0x0100, 0x276eadd5, 4 | BRF_GRA },           // 19
};

STD_ROM_PICK(tempall)
STD_ROM_FN(tempall)

struct BurnDriver BurnDrvTempall = {
	"tempall", "tempest", NULL, NULL, "2000",
	"Tempest All Levels\0", NULL, "hack", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HACK, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_ACTION | GBF_VECTOR, 0,
	NULL, tempallRomInfo, tempallRomName, NULL, NULL, NULL, NULL, TempestInputInfo, TempestDIPInfo,
	DrvInitSmall, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40 * 256,
	480, 640, 3, 4
};


// Tempest Twisted

static struct BurnRomInfo temptwstRomDesc[] = {
	{ "tempall.113",	0x0800, 0xa9000137, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "136002-114.e1",	0x0800, 0x11077375, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136002-115.f1",	0x0800, 0xf3e2827a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136002-316.h1",	0x0800, 0xaeb0f7e9, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136002-217.j1",	0x0800, 0xef2eb645, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "temptwst.118",	0x0800, 0xf30afe2d, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "temptwst.119",	0x0800, 0x571b1799, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136002-120.mn1",	0x0800, 0x35619648, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "temptwst.121",	0x0800, 0x67619e6e, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "136002-222.r1",	0x0800, 0x707bd5c3, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "136002-123.np3",	0x0800, 0x29f7e937, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "136002-124.r3",	0x0800, 0xc16ec351, 1 | BRF_PRG | BRF_ESS }, // 11

	{ "136002-125.d7",	0x0100, 0x5903af03, 2 | BRF_GRA },           // 12 user1

	{ "136002-126.a1",	0x0020, 0x8b04f921, 3 | BRF_GRA },           // 13 mathbox prom

	{ "136002-132.l1",	0x0100, 0x2af82e87, 4 | BRF_GRA },           // 14 user3
	{ "136002-131.k1",	0x0100, 0xb31f6e24, 4 | BRF_GRA },           // 15
	{ "136002-130.j1",	0x0100, 0x8119b847, 4 | BRF_GRA },           // 16
	{ "136002-129.h1",	0x0100, 0x09f5a4d5, 4 | BRF_GRA },           // 17
	{ "136002-128.f1",	0x0100, 0x823b61ae, 4 | BRF_GRA },           // 18
	{ "136002-127.e1",	0x0100, 0x276eadd5, 4 | BRF_GRA },           // 19
};

STD_ROM_PICK(temptwst)
STD_ROM_FN(temptwst)

struct BurnDriver BurnDrvTemptwst = {
	"temptwst", "tempest", NULL, NULL, "2000",
	"Tempest Twisted\0", NULL, "Twisty", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HACK, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_ACTION | GBF_VECTOR, 0,
	NULL, temptwstRomInfo, temptwstRomName, NULL, NULL, NULL, NULL, TempestInputInfo, TempestDIPInfo,
	DrvInitSmall, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40 * 256,
	480, 640, 3, 4
};


// Vector Breakout [h]

static struct BurnRomInfo vbrakoutRomDesc[] = {
	{ "vbrakout.113",	0x0800, 0x6fd3efe5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "vbrakout.114",	0x0800, 0x9974b9a5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "vbrakout.115",	0x0800, 0x44d611d8, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "vbrakout.116",	0x0800, 0xcd58fc11, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "vbrakout.122",	0x0800, 0x1ae2dd53, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136002-123.np3",	0x0800, 0x29f7e937, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136002-124.r3",	0x0800, 0xc16ec351, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "136002-125.d7",	0x0100, 0x5903af03, 2 | BRF_GRA },           //  7 user1

	{ "136002-126.a1",	0x0020, 0x8b04f921, 3 | BRF_GRA },           //  8 mathbox prom

	{ "136002-132.l1",	0x0100, 0x2af82e87, 4 | BRF_GRA },           //  9 user3
	{ "136002-131.k1",	0x0100, 0xb31f6e24, 4 | BRF_GRA },           // 10
	{ "136002-130.j1",	0x0100, 0x8119b847, 4 | BRF_GRA },           // 11
	{ "136002-129.h1",	0x0100, 0x09f5a4d5, 4 | BRF_GRA },           // 12
	{ "136002-128.f1",	0x0100, 0x823b61ae, 4 | BRF_GRA },           // 13
	{ "136002-127.e1",	0x0100, 0x276eadd5, 4 | BRF_GRA },           // 14
};

STD_ROM_PICK(vbrakout)
STD_ROM_FN(vbrakout)

static INT32 vbrakoutInit()
{
	load_type = 2;

	return DrvInit();
}

struct BurnDriver BurnDrvVbrakout = {
	"vbrakout", "tempest", NULL, NULL, "1999",
	"Vector Breakout [h]\0", NULL, "Clay Cowgill", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HACK, 2, HARDWARE_MISC_PRE90S, GBF_BREAKOUT | GBF_VECTOR, 0,
	NULL, vbrakoutRomInfo, vbrakoutRomName, NULL, NULL, NULL, NULL, TempestInputInfo, TempestDIPInfo,
	vbrakoutInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40 * 256,
	480, 640, 3, 4
};
