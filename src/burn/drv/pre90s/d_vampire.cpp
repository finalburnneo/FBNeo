// Final Burn Neo Vampire driver module
// Based on MAME driver by Tomasz Slanina

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "m6502_intf.h"
#include "ay8910.h"
#include "burn_pal.h"
#include "pit8253.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvM6809ROM;
static UINT8 *DrvM6502ROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvColPROM;
static UINT8 *DrvM6809RAM;
static UINT8 *DrvM6502RAM;

static UINT8 *blitter_slots;
static UINT16 *blitter_bitmap[2];

static UINT8 soundlatch;
static UINT8 soundnmi;

static UINT8 blitter_flags;
static UINT8 blitter_base_y;
static UINT8 blitter_base_x;
static UINT16 blitter_last_offset;

static UINT8 _4000_write;
static int ppi_output;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static struct BurnInputInfo VampireInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Vampire)

static struct BurnDIPInfo VampireDIPList[]=
{
	{0x11, 0xff, 0xff, 0xf0, NULL					},
	{0x12, 0xff, 0xff, 0x7e, NULL					},

	{0   , 0xfe, 0   ,    4, "Coin A"				},
	{0x11, 0x01, 0x03, 0x01, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x03, 0x02, "4 Coins 3 Credits"	},
	{0x11, 0x01, 0x03, 0x00, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x03, 0x03, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x11, 0x01, 0x0c, 0x08, "3 Coins 1 Credits"	},
	{0x11, 0x01, 0x0c, 0x04, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x0c, 0x0c, "4 Coins 3 Credits"	},
	{0x11, 0x01, 0x0c, 0x00, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x11, 0x01, 0x10, 0x00, "Off"					},
	{0x11, 0x01, 0x10, 0x10, "On"					},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x11, 0x01, 0x20, 0x00, "Off"					},
	{0x11, 0x01, 0x20, 0x20, "On"					},

	{0   , 0xfe, 0   ,    2, "Infinite Time"		},
	{0x11, 0x01, 0x40, 0x40, "Off"					},
	{0x11, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x11, 0x01, 0x80, 0x00, "Off"					},
	{0x11, 0x01, 0x80, 0x80, "On"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x12, 0x01, 0x01, 0x00, "Cocktail"				},
	{0x12, 0x01, 0x01, 0x01, "Upright"				},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x12, 0x01, 0x02, 0x00, "Off"					},
	{0x12, 0x01, 0x02, 0x02, "On"					},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x12, 0x01, 0x04, 0x00, "Off"					},
	{0x12, 0x01, 0x04, 0x04, "On"					},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x12, 0x01, 0x08, 0x00, "Off"					},
	{0x12, 0x01, 0x08, 0x08, "On"					},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x12, 0x01, 0x10, 0x00, "Off"					},
	{0x12, 0x01, 0x10, 0x10, "On"					},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x12, 0x01, 0x20, 0x00, "Off"					},
	{0x12, 0x01, 0x20, 0x20, "On"					},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x12, 0x01, 0x40, 0x00, "Off"					},
	{0x12, 0x01, 0x40, 0x40, "On"					},

	{0   , 0xfe, 0   ,    2, "Infinite Lives"		},
	{0x12, 0x01, 0x80, 0x00, "Off"					},
	{0x12, 0x01, 0x80, 0x80, "On"					},
};

STDDIPINFO(Vampire)

static void blit()
{
	UINT16 *ptr = blitter_bitmap[(blitter_flags & 0x10) >> 4];
	UINT32 index = blitter_last_offset >> 3;

	while (index < 0x200)
	{
		UINT8 *slot = &blitter_slots[index << 3];

		UINT32 start_offset = ((slot[0] << 8) | slot[1]) << 3;
		INT32 sy = 256 - slot[2]; // pixels
		INT32 sx = ((32 - slot[3])) << 3; // bytes (src ROM)
		UINT8 pen = slot[5]; // 0xf is max used
		UINT32 desty = slot[6];
		UINT32 destx = slot[7];
		UINT8 solid = (slot[0] & 0x80) >> 7;

		for (INT32 yy = 0; yy < sy; ++yy)
		{
			for (INT32 xx = 0; xx < sx; ++xx)
			{
				const UINT32 srcptr = start_offset + 256 * yy + xx;
				const UINT32 dstptr = (desty + blitter_base_y) * 256 + destx + xx + blitter_base_x;

				if (desty + blitter_base_y > 255) continue;
				if (destx + xx + blitter_base_x > 255) continue;

				if (dstptr < 0x10000)
				{
					UINT8 pix = (DrvGfxROM[(srcptr >> 3) & 0x3fff] & (1 << (7 - (srcptr & 7)))) | solid;
					
					if (pix)
					{
						ptr[dstptr] = pen;
					}
				}
			}

			desty += (slot[4] & 0x20) ? 1 : -1;
		}

		if (slot[4] & 0x80)
			break;

		++index;
	}
}

static void vampire_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf000) == 0x1000) {
		blitter_last_offset = address & 0xfff;
		if ((blitter_flags & 0x40) == 0) {
			blitter_slots[blitter_last_offset] = data;
		}
		return;
	}

	switch (address)
	{
		case 0x07f4:
			soundlatch = data;
			soundnmi = 0x00;
			M6502SetIRQLine(0x20, CPU_IRQSTATUS_ACK);
		return;

		case 0x07f8:
			blitter_flags = data;
		return;

		case 0x07fa:
			blitter_base_y = data;
		return;

		case 0x07fb:
			blitter_base_x = data;
			blit();
			M6809SetIRQLine(0, CPU_IRQSTATUS_HOLD);
		return;
	}
}

static UINT8 vampire_read(UINT16 address)
{
	if ((address & 0xf000) == 0x1000) {
		return blitter_slots[address & 0xfff];
	}

	switch (address)
	{
		case 0x07f0:
			return DrvInputs[0];

		case 0x7f1:
			return DrvInputs[1];

		case 0x07f2:
			return DrvDips[0];

		case 0x07f3:
			return DrvDips[1];

		case 0x07f5:
			return soundnmi;

		case 0x0814:
			return BurnRandom() & 0xff;

		case 0x0815:
			return BurnRandom() & 0xff;
	}

	return 0;
}

static void sync_pit()
{
	INT32 cyc = (M6502TotalCycles() / 7) - pit8253TotalCycles();
	if (cyc > 0)  {
		pit8253Run(cyc);
	}
}

static void vampire_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x4000:
			_4000_write = data;
		return;

		case 0x8000:
			M6502SetIRQLine(0x20, CPU_IRQSTATUS_NONE);
			soundnmi = 0x80;
		return;

		case 0xa000:
		case 0xa001:
			AY8910Write(0, ~address & 1, data);
		return;

		case 0xa008:
		case 0xa009:
		case 0xa00a:
		case 0xa00b:
			sync_pit();
			pit8253_write(address & 3, data);
			return;
	}
}
int onemore = 0;
static UINT8 vampire_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x4000:
			return _4000_write;

		case 0x6000:
			return soundlatch;

		case 0xa008:
		case 0xa009:
		case 0xa00a:
		case 0xa00b:
			sync_pit();
			return pit8253_read(address & 3);

		case 0xa00c: {
			sync_pit();
			return ppi_output << 6;
		}
	}

	return 0;
}

static void pit_cb0(INT32 state)
{
	//bprintf(0, _T("cb0: %x\n"), state);
	ppi_output = state;
}

static void pit_cb1(INT32 state)
{
	//bprintf(0, _T("cb1: %x\n"), state);
}

static void pit_cb2(INT32 state)
{
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	M6809Open(0);
	M6809Reset();
	M6809Close();

	M6502Open(0);
	M6502Reset();
	M6502Close();

	AY8910Reset(0);

	soundlatch = 0;
	soundnmi = 0;

	blitter_flags = 0;
	blitter_base_y = 0;
	blitter_base_x = 0;
	blitter_last_offset = 0;

	_4000_write = 0;

	ppi_output = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6809ROM			= Next; Next += 0x008000;
	DrvM6502ROM			= Next; Next += 0x001000;

	DrvGfxROM			= Next; Next += 0x004000;

	DrvColPROM			= Next; Next += 0x000200;

	BurnPalette			= (UINT32*)Next; Next += 0x0100 * sizeof(UINT32);

	AllRam				= Next;

	DrvM6809RAM			= Next; Next += 0x002000;
	DrvM6502RAM			= Next; Next += 0x000400;

	blitter_slots		= Next; Next += 0x001000;

	blitter_bitmap[0]	= (UINT16*)Next; Next += 0x020000;
	blitter_bitmap[1]	= (UINT16*)Next; Next += 0x020000;

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvM6809ROM + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x2000, k++, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x4000, k++, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x6000, k++, 1)) return 1;

		if (BurnLoadRom(DrvM6502ROM + 0x0000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM   + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM   + 0x2000, k++, 1)) return 1;

		if (BurnLoadRom(DrvColPROM  + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x0100, k++, 1)) return 1;

		for (INT32 i = 0; i < 0x100; i++) {
			DrvColPROM[i] = (DrvColPROM[i] & 0xf) | (DrvColPROM[0x100 + i] << 4);
		}

		// fix vectors
		memcpy (DrvM6809ROM + 0x7ff6, DrvM6809ROM + 0x7fe0, 4);
	}

	M6809Init(0);
	M6809Open(0);
	M6809MapMemory(DrvM6809RAM,	0x2000, 0x3fff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM,	0x8000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(vampire_write);
	M6809SetReadHandler(vampire_read);
	M6809Close();

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvM6502RAM,	0x0000, 0x03ff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM,	0xf000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(vampire_sound_write);
	M6502SetReadHandler(vampire_sound_read);
	M6502Close();

	AY8910Init(0, 1000000, 0);
	AY8910SetAllRoutes(0, 0.30, BURN_SND_ROUTE_BOTH);
	AY8910SetBuffered(M6502TotalCycles, 1000000);
	pit8253_init(2000000, 2000000, 2000000, pit_cb0, pit_cb1, pit_cb2);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	M6809Exit();
	M6502Exit();
	AY8910Exit(0);
	pit8253_exit();

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 256; ++i)
	{
		INT32 r = ((DrvColPROM[i] >> 2) & 0x07) * 36;
		INT32 g = ((DrvColPROM[i] >> 0) & 0x03) * 85;
		INT32 b = ((DrvColPROM[i] >> 5) & 0x07) * 36;

		BurnPalette[i] = BurnHighCol(r, g, b, 0);
	}
}

static INT32 DrvDraw()
{
	if (BurnRecalc) {
		DrvPaletteInit();
		BurnRecalc = 0;
	}

	UINT16 *pTemp = pTransDraw;
	pTransDraw = blitter_bitmap[(~blitter_flags & 0x10) >> 4] + (256 * 16);

	BurnTransferCopy(BurnPalette);

	pTransDraw = pTemp;

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	pit8253NewFrame();
	M6502NewFrame();

	{
		memset (DrvInputs, 0, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[3] = { 2000000 / 60, 2000000 / 60, 2000000 / 60 };
	INT32 nCyclesDone[3] = { 0, 0, 0 };

	M6809Open(0);
	M6502Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, M6809);

		if (i == 239) {
			if (pBurnDraw) {
				BurnDrvRedraw();
			}
			M6809SetIRQLine(1, CPU_IRQSTATUS_HOLD);
		}

		CPU_RUN(1, M6502);

		//CPU_RUN(2, pit8253); (syncing via callbacks instead)
	}

	M6502Close();
	M6809Close();

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
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
		M6502Scan(nAction);
		AY8910Scan(nAction, pnMin);


		SCAN_VAR(soundlatch);
		SCAN_VAR(soundnmi);

		SCAN_VAR(blitter_flags);
		SCAN_VAR(blitter_base_y);
		SCAN_VAR(blitter_base_x);
		SCAN_VAR(blitter_last_offset);

		SCAN_VAR(_4000_write);
	}

	return 0;
}

// Vampire (prototype?)

static struct BurnRomInfo vampireRomDesc[] = {
	{ "h1.1h",			0x2000, 0x7e69ff9b, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "h2.2h",			0x2000, 0xe94155f8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "h3.3h",			0x2000, 0xce27dd90, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "h4.4h",			0x2000, 0xa25f00bc, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "s_9d.9d",		0x1000, 0xe13a7aef, 2 | BRF_PRG | BRF_ESS }, //  4 M6502 Code

	{ "cg_p1.6e",		0x2000, 0x042661a4, 3 | BRF_GRA },           //  5 Blitter Data
	{ "cg_p2.7e",		0x2000, 0xe9dd9dff, 3 | BRF_GRA },           //  6

	{ "tb24s10.16a",	0x0100, 0xbc60a2eb, 4 | BRF_GRA },           //  7 Color Data
	{ "tb24s10.16b",	0x0100, 0xaa6b627b, 4 | BRF_GRA },           //  8
};

STD_ROM_PICK(vampire)
STD_ROM_FN(vampire)

struct BurnDriver BurnDrvVampire = {
	"vampire", NULL, NULL, NULL, "1983",
	"Vampire (prototype?)\0", "imperfect sound & gfx", "Entertainment Enterprises, Ltd.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_PROTOTYPE, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, vampireRomInfo, vampireRomName, NULL, NULL, NULL, NULL, VampireInputInfo, VampireDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	224, 256, 3, 4
};
