// FB Alpha Shinsen / Match-It driver module
// Based on MAME driver by Nicola Salmoria

// sound hardware copied from m72 driver

#include "tiles_generic.h"
#include "z80_intf.h"
#include "burn_ym2151.h"
#include "dac.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM;
static UINT8 *DrvSndROM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvVidRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvZ80RAM1;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 soundlatch;
static UINT8 bankdata;
static UINT8 gfxbank;
static UINT8 irqvector;
static UINT32 sample_address;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo ShisenInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 4,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Shisen)

static struct BurnDIPInfo ShisenDIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL					},
	{0x15, 0xff, 0xff, 0xfd, NULL					},

	{0   , 0xfe, 0   ,    4, "Timer"				},
	{0x14, 0x01, 0x03, 0x03, "Easy"					},
	{0x14, 0x01, 0x03, 0x02, "Normal"				},
	{0x14, 0x01, 0x03, 0x01, "Hard"					},
	{0x14, 0x01, 0x03, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x14, 0x01, 0x0c, 0x0c, "Easy"					},
	{0x14, 0x01, 0x0c, 0x08, "Normal"				},
	{0x14, 0x01, 0x0c, 0x04, "Hard"					},
	{0x14, 0x01, 0x0c, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    16, "Coinage"				},
	{0x14, 0x01, 0xf0, 0xa0, "6 Coins 1 Credits"	},
	{0x14, 0x01, 0xf0, 0xb0, "5 Coins 1 Credits"	},
	{0x14, 0x01, 0xf0, 0xc0, "4 Coins 1 Credits"	},
	{0x14, 0x01, 0xf0, 0xd0, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0xf0, 0x10, "8 Coins 3 Credits"	},
	{0x14, 0x01, 0xf0, 0xe0, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0xf0, 0x20, "5 Coins 3 Credits"	},
	{0x14, 0x01, 0xf0, 0x30, "3 Coins 2 Credits"	},
	{0x14, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0xf0, 0x40, "2 Coins 3 Credits"	},
	{0x14, 0x01, 0xf0, 0x90, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0xf0, 0x80, "1 Coin  3 Credits"	},
	{0x14, 0x01, 0xf0, 0x70, "1 Coin  4 Credits"	},
	{0x14, 0x01, 0xf0, 0x60, "1 Coin  5 Credits"	},
	{0x14, 0x01, 0xf0, 0x50, "1 Coin  6 Credits"	},
	{0x14, 0x01, 0xf0, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x15, 0x01, 0x01, 0x01, "Off"					},
	{0x15, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x15, 0x01, 0x02, 0x02, "Off"					},
	{0x15, 0x01, 0x02, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Nude Pictures"		},
	{0x15, 0x01, 0x08, 0x00, "Off"					},
	{0x15, 0x01, 0x08, 0x08, "On"					},

	{0   , 0xfe, 0   ,    2, "Women Select"			},
	{0x15, 0x01, 0x10, 0x00, "No"					},
	{0x15, 0x01, 0x10, 0x10, "Yes"					},

	{0   , 0xfe, 0   ,    2, "Stop Mode (Cheat)"	},
	{0x15, 0x01, 0x20, 0x20, "Off"					},
	{0x15, 0x01, 0x20, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Play Mode"			},
	{0x15, 0x01, 0x40, 0x00, "1 Player"				},
	{0x15, 0x01, 0x40, 0x40, "2 Player"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x15, 0x01, 0x80, 0x80, "Off"					},
	{0x15, 0x01, 0x80, 0x00, "On"					},
};

STDDIPINFO(Shisen)

enum { VECTOR_INIT, YM2151_ASSERT, YM2151_CLEAR, Z80_ASSERT, Z80_CLEAR };

static void setvector_callback(INT32 param)
{
	switch (param)
	{
		case VECTOR_INIT:
			irqvector = 0xff;
			break;

		case YM2151_ASSERT:
			irqvector &= 0xef;
			break;

		case YM2151_CLEAR:
			irqvector |= 0x10;
			break;

		case Z80_ASSERT:
			irqvector &= 0xdf;
			break;

		case Z80_CLEAR:
			irqvector |= 0x20;
			break;
	}

	ZetSetVector(irqvector);
	ZetSetIRQLine(0, (irqvector == 0xff) ? CPU_IRQSTATUS_NONE : CPU_IRQSTATUS_ACK);
}

static void bankswitch(INT32 data)
{
	bankdata = data;
	gfxbank = (data & 0x38) >> 3;

	ZetMapMemory(DrvZ80ROM0 + ((bankdata & 7) * 0x4000), 0x8000, 0xbfff, MAP_RAM);
}

static void __fastcall shisen_main_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			// coin counter (not emulated in FBA)
		return;

		case 0x01:
			soundlatch = data;
			setvector_callback(Z80_ASSERT);
		return;

		case 0x02:
			bankswitch(data);
		return;
	}
}

static UINT8 __fastcall shisen_main_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
			return DrvDips[0];

		case 0x01:
			return DrvDips[1];

		case 0x02:
			return DrvInputs[0];

		case 0x03:
			return DrvInputs[1];

		case 0x04:
			return DrvInputs[2];
	}

	return 0;
}
	
static void __fastcall shisen_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
			BurnYM2151Write(port, data);
		return;

		case 0x80:
			sample_address >>= 2;
			sample_address = (sample_address & 0xff00) | (data << 0);
			sample_address <<= 2;
		return;

		case 0x81:
			sample_address >>= 2;
			sample_address = (sample_address & 0x00ff) | (data << 8);
			sample_address <<= 2;
		return;

		case 0x82:
			DACSignedWrite(0, data);
			sample_address = (sample_address + 1) & 0x3ffff;
		return;

		case 0x83:
			setvector_callback(Z80_CLEAR);
		return;
	}
}

static UINT8 __fastcall shisen_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
			return BurnYM2151Read();

		case 0x80:
			return soundlatch;

		case 0x84:
			return DrvSndROM[sample_address & 0x3ffff];
	}

	return 0;
}

static tilemap_callback( bg )
{
	INT32 attr = DrvVidRAM[offs * 2 + 1];
	INT32 code = DrvVidRAM[offs * 2 + 0] + ((attr & 0xf) * 0x100) + (gfxbank * 0x1000);

	TILE_SET_INFO(0, code, attr >> 4, 0);
}

static void DrvYM2151IrqHandler(INT32 nStatus)
{
	setvector_callback(nStatus ? YM2151_ASSERT : YM2151_CLEAR);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	bankswitch(2);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	BurnYM2151Reset();
	setvector_callback(VECTOR_INIT);
	DACReset();
	ZetClose();
	
	sample_address = 0;
	soundlatch = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x020000;
	DrvZ80ROM1		= Next; Next += 0x010000;

	DrvGfxROM		= Next; Next += 0x200000;

	DrvSndROM		= Next; Next += 0x040000;

	DrvPalette		= (UINT32*)Next; Next += 0x0100 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM0		= Next; Next += 0x002000;
	DrvVidRAM		= Next; Next += 0x001000;
	DrvPalRAM		= Next; Next += 0x000300;

	DrvZ80RAM1		= Next; Next += 0x000300;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[4]  = { 0, 4, 0x80000*8+0, 0x80000*8+4 };
	INT32 XOffs[8] = { STEP4(0,1), STEP4(64,1) };
	INT32 YOffs[8] = { STEP8(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x100000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM, 0x100000);

	GfxDecode(0x8000, 4, 8, 8, Plane, XOffs, YOffs, 0x80, tmp, DrvGfxROM);

	BurnFree (tmp);

	return 0;
}

static INT32 DrvInit(INT32 game)
{
	BurnSetRefreshRate(55.00);

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (game == 0 || game == 1)
		{
			if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0 + 0x10000,  1, 1)) return 1;

			if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  2, 1)) return 1;

			for (INT32 i = 0; i < 16; i++) {
				if (BurnLoadRom(DrvGfxROM  + 0x10000 * i,  3 + i, 1)) return 1;
			}
			
			if (game == 1)
			{
				if (BurnLoadRom(DrvSndROM + 0x00000, 19, 1)) return 1; // matchit does not have samples
				if (BurnLoadRom(DrvSndROM + 0x10000, 20, 1)) return 1;
				if (BurnLoadRom(DrvSndROM + 0x20000, 21, 1)) return 1;
				if (BurnLoadRom(DrvSndROM + 0x30000, 22, 1)) return 1;
			}
		}
		else if (game == 2)
		{
			if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;

			if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  1, 1)) return 1;

			for (INT32 i = 0; i < 16; i++) {
				if (BurnLoadRom(DrvGfxROM + 0x10000 * i,  2 + i, 1)) return 1;
			}

			if (BurnLoadRom(DrvSndROM + 0x00000, 18, 1)) return 1;
			if (BurnLoadRom(DrvSndROM + 0x10000, 19, 1)) return 1;
			if (BurnLoadRom(DrvSndROM + 0x20000, 20, 1)) return 1;
			if (BurnLoadRom(DrvSndROM + 0x30000, 21, 1)) return 1;
		}

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvPalRAM,			0xc800, 0xcaff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,			0xd000, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM0,		0xe000, 0xffff, MAP_RAM);
	ZetSetOutHandler(shisen_main_write_port);
	ZetSetInHandler(shisen_main_read_port);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,		0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,		0xfd00, 0xffff, MAP_RAM);
	ZetSetOutHandler(shisen_sound_write_port);
	ZetSetInHandler(shisen_sound_read_port);
	ZetClose();

	BurnYM2151Init(3579545);
	BurnYM2151SetIrqHandler(&DrvYM2151IrqHandler);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.50, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.50, BURN_SND_ROUTE_RIGHT);

	DACInit(0, 0, 1, ZetTotalCycles, 3579545);
	DACSetRoute(0, 0.40, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM, 4, 8, 8, 0x200000, 0, 0xf);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	BurnYM2151Exit();
	DACExit();

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteUpdate()
{
	for (INT32 i = 0; i < 0x100; i++)
	{
		UINT8 r = DrvPalRAM[i + 0x000] & 0x1f;
		UINT8 g = DrvPalRAM[i + 0x100] & 0x1f;
		UINT8 b = DrvPalRAM[i + 0x200] & 0x1f;

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}	

static INT32 DrvDraw()
{
	//if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	//}

	GenericTilemapDraw(0, pTransDraw, 0);

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
		memset (DrvInputs, 0xff, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 128; // sound nmi
	INT32 nSoundBufferPos = 0;
	INT32 nCyclesTotal[2] = { 6000000 / 55, 3579645 / 55 };
	INT32 nCyclesDone[2] = { 0, 0 };
	
	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);
		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_AUTO);
		ZetClose();

		ZetOpen(1);
		nCyclesDone[1] += ZetRun(nCyclesTotal[1] / nInterleave);
		ZetNmi();

		if (pBurnSoundOut) {
			INT32 nSegment = nBurnSoundLen / nInterleave;
			BurnYM2151Render(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
			nSoundBufferPos += nSegment;
		}

		ZetClose();
	}

	ZetOpen(1);

	if (pBurnSoundOut) {
		INT32 nSegment = nBurnSoundLen - nSoundBufferPos;
		if (nSegment > 0) {
			BurnYM2151Render(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
		}
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029705;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		ZetScan(nAction);
		BurnYM2151Scan(nAction, pnMin);
		DACScan(nAction, pnMin);

		SCAN_VAR(sample_address);
		SCAN_VAR(soundlatch);
		SCAN_VAR(bankdata);
		SCAN_VAR(irqvector);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		bankswitch(bankdata);
		ZetClose();
	}

	return 0;
}


// Match It

static struct BurnRomInfo matchitRomDesc[] = {
	{ "2.11d",		0x10000, 0x299815f7, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "ic07.03",	0x10000, 0x0350f6e2, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ic01.01",	0x10000, 0x51b0a26c, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "ic08.04",	0x10000, 0x1c0e221c, 3 | BRF_GRA },           //  3 Graphics
	{ "ic09.05",	0x10000, 0x8a7d8284, 3 | BRF_GRA },           //  4
	{ "ic12.08",	0x10000, 0x48e1d043, 3 | BRF_GRA },           //  5
	{ "ic13.09",	0x10000, 0x3feff3f2, 3 | BRF_GRA },           //  6
	{ "ic14.10",	0x10000, 0xb76a517d, 3 | BRF_GRA },           //  7
	{ "ic15.11",	0x10000, 0x8ff5ee7a, 3 | BRF_GRA },           //  8
	{ "ic16.12",	0x10000, 0x64e5d837, 3 | BRF_GRA },           //  9
	{ "ic17.13",	0x10000, 0x02c1b2c4, 3 | BRF_GRA },           // 10
	{ "ic18.14",	0x10000, 0xf5a8370e, 3 | BRF_GRA },           // 11
	{ "ic19.15",	0x10000, 0x7a9b7671, 3 | BRF_GRA },           // 12
	{ "ic20.16",	0x10000, 0x7fb396ad, 3 | BRF_GRA },           // 13
	{ "ic21.17",	0x10000, 0xfb83c652, 3 | BRF_GRA },           // 14
	{ "ic22.18",	0x10000, 0xd8b689e9, 3 | BRF_GRA },           // 15
	{ "ic23.19",	0x10000, 0xe6611947, 3 | BRF_GRA },           // 16
	{ "ic10.06",	0x10000, 0x473b349a, 3 | BRF_GRA },           // 17
	{ "ic11.07",	0x10000, 0xd9a60285, 3 | BRF_GRA },           // 18
};

STD_ROM_PICK(matchit)
STD_ROM_FN(matchit)

static INT32 MatchitInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvMatchit = {
	"matchit", NULL, NULL, NULL, "1989",
	"Match It\0", NULL, "Tamtex", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_PUZZLE, 0,
	NULL, matchitRomInfo, matchitRomName, NULL, NULL, NULL, NULL, ShisenInputInfo, ShisenDIPInfo,
	MatchitInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	512, 256, 4, 3
};


// Sichuan II (hack, set 1)

static struct BurnRomInfo sichuan2RomDesc[] = {
	{ "ic06.06",	0x10000, 0x98a2459b, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "ic07.03",	0x10000, 0x0350f6e2, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ic01.01",	0x10000, 0x51b0a26c, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "ic08.04",	0x10000, 0x1c0e221c, 3 | BRF_GRA },           //  3 Graphics
	{ "ic09.05",	0x10000, 0x8a7d8284, 3 | BRF_GRA },           //  4
	{ "ic12.08",	0x10000, 0x48e1d043, 3 | BRF_GRA },           //  5
	{ "ic13.09",	0x10000, 0x3feff3f2, 3 | BRF_GRA },           //  6
	{ "ic14.10",	0x10000, 0xb76a517d, 3 | BRF_GRA },           //  7
	{ "ic15.11",	0x10000, 0x8ff5ee7a, 3 | BRF_GRA },           //  8
	{ "ic16.12",	0x10000, 0x64e5d837, 3 | BRF_GRA },           //  9
	{ "ic17.13",	0x10000, 0x02c1b2c4, 3 | BRF_GRA },           // 10
	{ "ic18.14",	0x10000, 0xf5a8370e, 3 | BRF_GRA },           // 11
	{ "ic19.15",	0x10000, 0x7a9b7671, 3 | BRF_GRA },           // 12
	{ "ic20.16",	0x10000, 0x7fb396ad, 3 | BRF_GRA },           // 13
	{ "ic21.17",	0x10000, 0xfb83c652, 3 | BRF_GRA },           // 14
	{ "ic22.18",	0x10000, 0xd8b689e9, 3 | BRF_GRA },           // 15
	{ "ic23.19",	0x10000, 0xe6611947, 3 | BRF_GRA },           // 16
	{ "ic10.06",	0x10000, 0x473b349a, 3 | BRF_GRA },           // 17
	{ "ic11.07",	0x10000, 0xd9a60285, 3 | BRF_GRA },           // 18

	{ "ic02.02",	0x10000, 0x92f0093d, 4 | BRF_SND },           // 19 Samples
	{ "ic03.03",	0x10000, 0x116a049c, 4 | BRF_SND },           // 20
	{ "ic04.04",	0x10000, 0x6840692b, 4 | BRF_SND },           // 21
	{ "ic05.05",	0x10000, 0x92ffe22a, 4 | BRF_SND },           // 22
};

STD_ROM_PICK(sichuan2)
STD_ROM_FN(sichuan2)

static INT32 Sichuan2Init()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvSichuan2 = {
	"sichuan2", "matchit", NULL, NULL, "1989",
	"Sichuan II (hack, set 1)\0", NULL, "hack", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_PUZZLE, 0,
	NULL, sichuan2RomInfo, sichuan2RomName, NULL, NULL, NULL, NULL, ShisenInputInfo, ShisenDIPInfo,
	Sichuan2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	512, 256, 4, 3
};


// Sichuan II (hack, set 2)

static struct BurnRomInfo sichuan2aRomDesc[] = {
	{ "sichuan.a6",	0x10000, 0xf8ac05ef, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "ic07.03",	0x10000, 0x0350f6e2, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ic01.01",	0x10000, 0x51b0a26c, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "ic08.04",	0x10000, 0x1c0e221c, 3 | BRF_GRA },           //  3 Graphics
	{ "ic09.05",	0x10000, 0x8a7d8284, 3 | BRF_GRA },           //  4
	{ "ic12.08",	0x10000, 0x48e1d043, 3 | BRF_GRA },           //  5
	{ "ic13.09",	0x10000, 0x3feff3f2, 3 | BRF_GRA },           //  6
	{ "ic14.10",	0x10000, 0xb76a517d, 3 | BRF_GRA },           //  7
	{ "ic15.11",	0x10000, 0x8ff5ee7a, 3 | BRF_GRA },           //  8
	{ "ic16.12",	0x10000, 0x64e5d837, 3 | BRF_GRA },           //  9
	{ "ic17.13",	0x10000, 0x02c1b2c4, 3 | BRF_GRA },           // 10
	{ "ic18.14",	0x10000, 0xf5a8370e, 3 | BRF_GRA },           // 11
	{ "ic19.15",	0x10000, 0x7a9b7671, 3 | BRF_GRA },           // 12
	{ "ic20.16",	0x10000, 0x7fb396ad, 3 | BRF_GRA },           // 13
	{ "ic21.17",	0x10000, 0xfb83c652, 3 | BRF_GRA },           // 14
	{ "ic22.18",	0x10000, 0xd8b689e9, 3 | BRF_GRA },           // 15
	{ "ic23.19",	0x10000, 0xe6611947, 3 | BRF_GRA },           // 16
	{ "ic10.06",	0x10000, 0x473b349a, 3 | BRF_GRA },           // 17
	{ "ic11.07",	0x10000, 0xd9a60285, 3 | BRF_GRA },           // 18

	{ "ic02.02",	0x10000, 0x92f0093d, 4 | BRF_SND },           // 19 Samples
	{ "ic03.03",	0x10000, 0x116a049c, 4 | BRF_SND },           // 20
	{ "ic04.04",	0x10000, 0x6840692b, 4 | BRF_SND },           // 21
	{ "ic05.05",	0x10000, 0x92ffe22a, 4 | BRF_SND },           // 22
};

STD_ROM_PICK(sichuan2a)
STD_ROM_FN(sichuan2a)

struct BurnDriver BurnDrvSichuan2a = {
	"sichuan2a", "matchit", NULL, NULL, "1989",
	"Sichuan II (hack, set 2)\0", NULL, "hack", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_PUZZLE, 0,
	NULL, sichuan2aRomInfo, sichuan2aRomName, NULL, NULL, NULL, NULL, ShisenInputInfo, ShisenDIPInfo,
	Sichuan2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	512, 256, 4, 3
};


// Shisensho - Joshiryo-Hen (Japan)

static struct BurnRomInfo shisenRomDesc[] = {
	{ "a-27-a.rom",	0x20000, 0xde2ecf05, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	{ "ic01.01",	0x10000, 0x51b0a26c, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code

	{ "ic08.04",	0x10000, 0x1c0e221c, 3 | BRF_GRA },           //  2 Graphics
	{ "ic09.05",	0x10000, 0x8a7d8284, 3 | BRF_GRA },           //  3
	{ "ic12.08",	0x10000, 0x48e1d043, 3 | BRF_GRA },           //  4
	{ "ic13.09",	0x10000, 0x3feff3f2, 3 | BRF_GRA },           //  5
	{ "ic14.10",	0x10000, 0xb76a517d, 3 | BRF_GRA },           //  6
	{ "ic15.11",	0x10000, 0x8ff5ee7a, 3 | BRF_GRA },           //  7
	{ "ic16.12",	0x10000, 0x64e5d837, 3 | BRF_GRA },           //  8
	{ "ic17.13",	0x10000, 0x02c1b2c4, 3 | BRF_GRA },           //  9
	{ "ic18.14",	0x10000, 0xf5a8370e, 3 | BRF_GRA },           // 10
	{ "ic19.15",	0x10000, 0x7a9b7671, 3 | BRF_GRA },           // 11
	{ "ic20.16",	0x10000, 0x7fb396ad, 3 | BRF_GRA },           // 12
	{ "ic21.17",	0x10000, 0xfb83c652, 3 | BRF_GRA },           // 13
	{ "ic22.18",	0x10000, 0xd8b689e9, 3 | BRF_GRA },           // 14
	{ "ic23.19",	0x10000, 0xe6611947, 3 | BRF_GRA },           // 15
	{ "ic10.06",	0x10000, 0x473b349a, 3 | BRF_GRA },           // 16
	{ "ic11.07",	0x10000, 0xd9a60285, 3 | BRF_GRA },           // 17

	{ "ic02.02",	0x10000, 0x92f0093d, 4 | BRF_SND },           // 18 Samples
	{ "ic03.03",	0x10000, 0x116a049c, 4 | BRF_SND },           // 19
	{ "ic04.04",	0x10000, 0x6840692b, 4 | BRF_SND },           // 20
	{ "ic05.05",	0x10000, 0x92ffe22a, 4 | BRF_SND },           // 21
};

STD_ROM_PICK(shisen)
STD_ROM_FN(shisen)

static INT32 ShisenInit()
{
	return DrvInit(2);
}

struct BurnDriver BurnDrvShisen = {
	"shisen", "matchit", NULL, NULL, "1989",
	"Shisensho - Joshiryo-Hen (Japan)\0", NULL, "Tamtex", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_PUZZLE, 0,
	NULL, shisenRomInfo, shisenRomName, NULL, NULL, NULL, NULL, ShisenInputInfo, ShisenDIPInfo,
	ShisenInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	512, 256, 4, 3
};
