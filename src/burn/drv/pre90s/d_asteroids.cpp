// FB Alpha 1942 driver module
// Based on MAME driver by Brad Oliver, Bernd Wiebelt, Allard van der Bas

// todo:
//   add clones
//   add lunar lander hw?

#include "tiles_generic.h"
#include "m6502_intf.h"
#include "watchdog.h"
#include "bitswap.h"
#include "avgdvg.h"
#include "vector.h"
#include "pokey.h"
#include "asteroids.h"
#include "earom.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6502ROM;
static UINT8 *DrvDgvPROM;
static UINT8 *DrvM6502RAM;
static UINT8 *DrvVectorRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 bankdata;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static INT32 avgOK = 0; // ok to run avgdvg?

static INT32 astdelux = 0;

static struct BurnInputInfo AsteroidInputList[] = {
	{"P1 Coin",		    BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 start"	},
	{"P1 Left",		    BIT_DIGITAL,	DrvJoy2 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 right"	},
	{"P1 Fire",		    BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Thrust",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},
	{"P1 Hyperspace",	BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	    "reset"		},
	{"Diag Step",	BIT_DIGITAL,	DrvJoy1 + 5,	"service2"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy1 + 6,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		}, // astdelux
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		}, // servicemode
};

STDINPUTINFO(Asteroid)

static struct BurnInputInfo AstdeluxInputList[] = {
	{"P1 Coin",		    BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 start"	},
	{"P1 Left",		    BIT_DIGITAL,	DrvJoy2 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 right"	},
	{"P1 Fire",		    BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Thrust",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},
	{"P1 Sheild",	    BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	    "reset"		},
	{"Diag Step",	BIT_DIGITAL,	DrvJoy1 + 5,	"service2"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy1 + 6,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		}, // astdelux
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		}, // servicemode
};

STDINPUTINFO(Astdelux)

#define DO 0xa    // getting tired of re-basing the dips. :P

static struct BurnDIPInfo AsteroidDIPList[]=
{
	{DO+0, 0xff, 0xff, 0x84, NULL			},
	{DO+1, 0xff, 0xff, 0x00, NULL			},
	{DO+2, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    4, "Language"		},
	{DO+0, 0x01, 0x03, 0x00, "English"		},
	{DO+0, 0x01, 0x03, 0x01, "German"		},
	{DO+0, 0x01, 0x03, 0x02, "French"		},
	{DO+0, 0x01, 0x03, 0x03, "Spanish"		},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{DO+0, 0x01, 0x04, 0x04, "3"			},
	{DO+0, 0x01, 0x04, 0x00, "4"			},

#if 0
	{0   , 0xfe, 0   ,    2, "Center Mech"		},
	{DO+0, 0x01, 0x08, 0x00, "X 1"			},
	{DO+0, 0x01, 0x08, 0x08, "X 2"			},

	{0   , 0xfe, 0   ,    4, "Right Mech"		},
	{DO+0, 0x01, 0x30, 0x00, "X 1"			},
	{DO+0, 0x01, 0x30, 0x10, "X 4"			},
	{DO+0, 0x01, 0x30, 0x20, "X 5"			},
	{DO+0, 0x01, 0x30, 0x30, "X 6"			},
#endif

	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{DO+0, 0x01, 0xc0, 0xc0, "2 Coins 1 Credits"	},
	{DO+0, 0x01, 0xc0, 0x80, "1 Coin  1 Credits"	},
	{DO+0, 0x01, 0xc0, 0x40, "1 Coin  2 Credits"	},
	{DO+0, 0x01, 0xc0, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{DO+2, 0x01, 0x80, 0x00, "Off"				},
	{DO+2, 0x01, 0x80, 0x80, "On"				},
};

STDDIPINFO(Asteroid)

static struct BurnDIPInfo AstdeluxDIPList[]=
{
	{DO+0, 0xff, 0xff, 0x00, NULL				},
	{DO+1, 0xff, 0xff, 0xfd, NULL				},
	{DO+2, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    4, "Language"			},
	{DO+0, 0x01, 0x03, 0x00, "English"			},
	{DO+0, 0x01, 0x03, 0x01, "German"			},
	{DO+0, 0x01, 0x03, 0x02, "French"			},
	{DO+0, 0x01, 0x03, 0x03, "Spanish"			},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{DO+0, 0x01, 0x0c, 0x00, "2-4"				},
	{DO+0, 0x01, 0x0c, 0x04, "3-5"				},
	{DO+0, 0x01, 0x0c, 0x08, "4-6"				},
	{DO+0, 0x01, 0x0c, 0x0c, "5-7"				},

	{0   , 0xfe, 0   ,    2, "Minimum Plays"		},
	{DO+0, 0x01, 0x10, 0x00, "1"				},
	{DO+0, 0x01, 0x10, 0x10, "2"				},

	{0   , 0xfe, 0   ,    2, "Difficulty"			},
	{DO+0, 0x01, 0x20, 0x00, "Hard"				},
	{DO+0, 0x01, 0x20, 0x20, "Easy"				},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{DO+0, 0x01, 0xc0, 0x00, "10000"			},
	{DO+0, 0x01, 0xc0, 0x40, "12000"			},
	{DO+0, 0x01, 0xc0, 0x80, "15000"			},
	{DO+0, 0x01, 0xc0, 0xc0, "None"				},

	{0   , 0xfe, 0   ,    4, "Coinage"			},
	{DO+1, 0x01, 0x03, 0x00, "2 Coins 1 Credits"		},
	{DO+1, 0x01, 0x03, 0x01, "1 Coin  1 Credits"		},
	{DO+1, 0x01, 0x03, 0x02, "1 Coin  2 Credits"		},
	{DO+1, 0x01, 0x03, 0x03, "Free Play"			},

	{0   , 0xfe, 0   ,    4, "Right Coin"			},
	{DO+1, 0x01, 0x0c, 0x00, "X 6"				},
	{DO+1, 0x01, 0x0c, 0x04, "X 5"				},
	{DO+1, 0x01, 0x0c, 0x08, "X 4"				},
	{DO+1, 0x01, 0x0c, 0x0c, "X 1"				},

	{0   , 0xfe, 0   ,    2, "Center Coin"			},
	{DO+1, 0x01, 0x10, 0x00, "X 2"				},
	{DO+1, 0x01, 0x10, 0x10, "X 1"				},

	{0   , 0xfe, 0   ,    5, "Bonus Coins"			},
	{DO+1, 0x01, 0xe0, 0x60, "1 Coin Each 5 Coins"		},
	{DO+1, 0x01, 0xe0, 0x80, "2 Coins Each 4 Coins"		},
	{DO+1, 0x01, 0xe0, 0xa0, "1 Coin Each 4 Coins"		},
	{DO+1, 0x01, 0xe0, 0xc0, "1 Coin Each 2 Coins"		},
	{DO+1, 0x01, 0xe0, 0xe0, "None"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{DO+2, 0x01, 0x80, 0x00, "Off"				},
	{DO+2, 0x01, 0x80, 0x80, "On"				},
};

STDDIPINFO(Astdelux)

static void bankswitch(INT32 data)
{
	bankdata = data;
	INT32 bank = (astdelux) ? (data >> 7) & 1 : (data >> 2) & 1;
	if (bank == 0) {
		M6502MapMemory(DrvM6502RAM + 0x200,	0x0200, 0x02ff, MAP_RAM);
		M6502MapMemory(DrvM6502RAM + 0x300,	0x0300, 0x03ff, MAP_RAM);
	} else {
		M6502MapMemory(DrvM6502RAM + 0x300,	0x0200, 0x02ff, MAP_RAM);
		M6502MapMemory(DrvM6502RAM + 0x200,	0x0300, 0x03ff, MAP_RAM);
	}
}

static void asteroid_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x3000:
			avgdvg_go();
			avgOK = 1;
		return;

		case 0x3200:
			bankswitch(data);
		return;

		case 0x3400:
			BurnWatchogWrite();
		return;

		case 0x3600:
			asteroid_explode_w(data);
		return;

		case 0x3a00:
			asteroid_thump_w(data);
		return;

		case 0x3c00:
		case 0x3c01:
		case 0x3c02:
		case 0x3c03:
		case 0x3c04:
		case 0x3c05:
			asteroid_sounds_w(address & 7, data);
		return;
	}
}

static void astdelux_write(UINT16 address, UINT8 data)
{
	if (address >= 0x2c00 && address <= 0x2c0f) {
		pokey1_w(address & 0x0f, data);
		return;
	}

	if (address >= 0x3200 && address <= 0x323f) {
		earom_write(address & 0x3f, data);
		return;
	}

	switch (address)
	{
		case 0x3000:
			avgdvg_go();
			avgOK = 1;
		return;

		case 0x3c04:
			bankswitch(data);
		return;

		case 0x3400:
			BurnWatchogWrite();
		return;

		case 0x3600:
			asteroid_explode_w(data);
		return;

		case 0x3a00:
			earom_ctrl_write(address, data);
		return;

		case 0x3c03:
			astdelux_sounds_w(data);
		return;

		case 0x3e00:
			// noise_reset_w
		return;
	}
}

static UINT8 asteroid_read(UINT16 address)
{
	switch (address & ~7)
	{
		case 0x2000:
		{
			UINT8 ret = (DrvInputs[0] & ~0x06);

			ret |= ((M6502TotalCycles() & 0x100) ? 0x02 : 0);
			ret |= (avgdvg_done() ? 0x00 : 0x04);

			return (ret & (1 << (address & 7))) ? 0x80 : 0x7f;
		}

		case 0x2400:
			return (DrvInputs[1] & (1 << (address & 7))) ? 0x80 : 0x7f;
	}

	switch (address & ~3)
	{
		case 0x2800:
			return 0xfc | ((DrvDips[0] >> ((~address & 3) * 2)) & 3);;
	}

	return 0;
}

static UINT8 astdelux_read(UINT16 address)
{
	if (address >= 0x2c00 && address <= 0x2c0f) {
		return pokey1_r(address & 0x0f);
	}

	if (address >= 0x2c40 && address <= 0x2c7f) {
		return earom_read(address);
	}

	switch (address & ~7)
	{
		case 0x2000:
		{
			UINT8 ret = (DrvInputs[0] & ~0x06);

			ret |= ((M6502TotalCycles() & 0x100) ? 0x02 : 0);
			ret |= (avgdvg_done() ? 0x00 : 0x04);

			return (ret & (1 << (address & 7))) ? 0x80 : 0x7f;
		}

		case 0x2400:
			return (DrvInputs[1] & (1 << (address & 7))) ? 0x80 : 0x7f;
	}

	switch (address & ~3)
	{
		case 0x2800:
			return 0xfc | ((DrvDips[0] >> ((~address & 3) * 2)) & 3);;
	}

	return 0;
}

static INT32 allpot_read(INT32 offset)
{
	return DrvDips[1];
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	M6502Open(0);
	bankswitch(0);
	M6502Reset();
	M6502Close();

	BurnWatchdogReset();
	vector_reset();
	avgdvg_reset();

	earom_reset();

	avgOK = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6502ROM		= Next; Next += 0x010000;

	DrvDgvPROM		= Next; Next += 0x000100;

	DrvPalette		= (UINT32*)Next; Next += 0x2 * 256 * sizeof(UINT32);

	AllRam			= Next;

	DrvM6502RAM		= Next; Next += 0x000800;

	RamEnd			= Next;

	MemEnd			= Next;

	DrvVectorRAM	= DrvM6502ROM + 0x4000;

	return 0;
}

static INT32 DrvInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvM6502ROM + 0x6800,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x7000,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x7800,  2, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x5000,  3, 1)) return 1;

		if (BurnLoadRom(DrvDgvPROM  + 0x0000,  4, 1)) return 1;
	}

	if (DrvVectorRAM != DrvM6502ROM+0x4000) bprintf(0, _T("VectorRAM not hooked up right.\n"));

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502SetAddressMask(0x7fff);
	M6502MapMemory(DrvM6502RAM,		        0x0000, 0x01ff, MAP_RAM);
	bankswitch(0);                       // 0x0200, 0x03ff
	M6502MapMemory(DrvVectorRAM,		    0x4000, 0x47ff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM + 0x5000,	0x5000, 0x57ff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM + 0x6800,	0x6800, 0x7fff, MAP_ROM);
	M6502SetWriteHandler(asteroid_write);
	M6502SetReadHandler(asteroid_read);
	M6502Close();

	BurnWatchdogInit(DrvDoReset, 180);

	vector_init();
	vector_set_scale(1044, 788);
	vector_set_offsets(11, 119);

	dvg_asteroids_start(DrvVectorRAM);

	asteroid_sound_init();

	DrvDoReset(1);


	return 0;
}

static INT32 AstdeluxInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvM6502ROM + 0x6000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x6800,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x7000,  2, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x7800,  3, 1)) return 1;

		if (BurnLoadRom(DrvM6502ROM + 0x4800,  4, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x5000,  5, 1)) return 1;

		if (BurnLoadRom(DrvDgvPROM  + 0x0000,  6, 1)) return 1;
	}

	if (DrvVectorRAM != DrvM6502ROM+0x4000) bprintf(0, _T("VectorRAM not hooked up right.\n"));

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502SetAddressMask(0x7fff);
	M6502MapMemory(DrvM6502RAM,		        0x0000, 0x01ff, MAP_RAM);
	bankswitch(0);                       // 0x0200, 0x03ff
	M6502MapMemory(DrvVectorRAM,		    0x4000, 0x47ff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM + 0x4800,	0x4800, 0x57ff, MAP_ROM); // Vector ROM
	M6502MapMemory(DrvM6502ROM + 0x6000,	0x6000, 0x7fff, MAP_ROM); // Main ROM
	M6502SetWriteHandler(astdelux_write);
	M6502SetReadHandler(astdelux_read);
	M6502Close();

	BurnWatchdogInit(DrvDoReset, 180);

	vector_init();
	vector_set_scale(1044, 788);
	vector_set_offsets(11, 119);

	dvg_asteroids_start(DrvVectorRAM);

	asteroid_sound_init();

	astdelux = 1;

	earom_init();

	PokeyInit(12096000/8, 1, 2.40, 1);
	PokeySetTotalCyclesCB(M6502TotalCycles);
	PokeyAllPotCallback(0, allpot_read);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	M6502Exit();
	vector_exit();
	asteroid_sound_exit();

	BurnFree(AllMem);

	if (astdelux) {
		earom_exit();
		PokeyExit();
	}

	astdelux = 0;

	return 0;
}

static void DrvPaletteInit()
{
	UINT32 colors[2] = { 0x000000, 0xffffff };

    for (INT32 i = 0; i < 0x2; i++) // color
	{
		for (INT32 j = 0; j < 256; j++) // intensity
		{
			int r = (colors[i] >> 16) & 0xff;
			int g = (colors[i] >> 8) & 0xff;
			int b = (colors[i] >> 0) & 0xff;

			r = (r * j) / 255;
			g = (g * j) / 255;
			b = (b * j) / 255;

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
	DrvPaletteInit();

	draw_vector(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	{
		memset (DrvInputs, 0, 2);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}

		DrvInputs[0] = (DrvInputs[0] & ~0x80) | (DrvDips[2] & 0x80); // service mode
	}

	INT32 nInterleave = 256; // nmi is 4x per frame!
	INT32 nCyclesTotal = (1512000 * 100) / 6152; // 61.5234375 hz
	INT32 nCyclesDone  = 0;
	INT32 nSoundBufferPos = 0;

	M6502Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone += M6502Run(nCyclesTotal / nInterleave);

		if ((i % 64) == 63 && (DrvInputs[0] & 0x80) == 0)
			M6502SetIRQLine(0x20, CPU_IRQSTATUS_AUTO);

		// Render Sound Segment
		if (pBurnSoundOut && i&1) {
			INT32 nSegmentLength = nBurnSoundLen / (nInterleave/2);
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			if (astdelux)
				astdelux_sound_update(pSoundBuf, nSegmentLength);
			else
				asteroid_sound_update(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	M6502Close();

	// Make sure the buffer is entirely filled.
	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			if (astdelux)
				astdelux_sound_update(pSoundBuf, nSegmentLength);
			else
				asteroid_sound_update(pSoundBuf, nSegmentLength);
		}

		if (astdelux)
			pokey_update(0, pBurnSoundOut, nBurnSoundLen);
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

		memset(&ba, 0, sizeof(ba));
		ba.Data	  = DrvVectorRAM;
		ba.nLen	  = 0x1000;
		ba.szName = "Vector Ram";
		BurnAcb(&ba);

		M6502Scan(nAction);

		BurnWatchdogScan(nAction);

		SCAN_VAR(avgOK);
		SCAN_VAR(bankdata);

		if (astdelux)
			pokey_scan(nAction, pnMin);
	}

	if (astdelux)
		earom_scan(nAction, pnMin); // here.

	if (nAction & ACB_WRITE) {
		M6502Open(0);
		bankswitch(bankdata);
		M6502Close();

		if (avgOK) {
			avgdvg_go();
		}
	}

	return 0;
}

// Asteroids (rev 4)

static struct BurnRomInfo asteroidRomDesc[] = {
	{ "035145-04e.ef2",		0x0800, 0xb503eaf7, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "035144-04e.h2",		0x0800, 0x25233192, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "035143-02.j2",		0x0800, 0x312caa02, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "035127-02.np3",		0x0800, 0x8b71fd9e, 1 | BRF_PRG | BRF_ESS }, //  3 Vector ROM

	{ "034602-01.c8",		0x0100, 0x97953db8, 2 | BRF_GRA },           //  4 DVG PROM
};

STD_ROM_PICK(asteroid)
STD_ROM_FN(asteroid)

struct BurnDriver BurnDrvAsteroid = {
	"asteroid", NULL, NULL, NULL, "1979",
	"Asteroids (rev 4)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, asteroidRomInfo, asteroidRomName, NULL, NULL, AsteroidInputInfo, AsteroidDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 2 * 256,
	600, 500, 4, 3
};

// Asteroids Deluxe (rev 3)

static struct BurnRomInfo astdeluxRomDesc[] = {
	{ "036430-02.d1",	0x0800, 0xa4d7a525, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "036431-02.ef1",	0x0800, 0xd4004aae, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "036432-02.fh1",	0x0800, 0x6d720c41, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "036433-03.j1",	0x0800, 0x0dcc0be6, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "036800-02.r2",	0x0800, 0xbb8cabe1, 1 | BRF_PRG | BRF_ESS }, //  4 Vector ROM
	{ "036799-01.np2",	0x0800, 0x7d511572, 1 | BRF_PRG | BRF_ESS }, //  5 Vector ROM 2/2

	{ "034602-01.c8",	0x0100, 0x97953db8, 2 | BRF_GRA },           //  6 DVG PROM
};

STD_ROM_PICK(astdelux)
STD_ROM_FN(astdelux)

struct BurnDriver BurnDrvAstdelux = {
	"astdelux", NULL, NULL, NULL, "1980",
	"Asteroids Deluxe (rev 3)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, astdeluxRomInfo, astdeluxRomName, NULL, NULL, AstdeluxInputInfo, AstdeluxDIPInfo,
	AstdeluxInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 2 * 256,
	600, 500, 4, 3
};
