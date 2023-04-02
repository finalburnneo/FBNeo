// FB Neo redclash/zerohour driver module
// Based on MAME driver by David Haywood

// Zero Hour sample pack created by Otto_Pylotte October 2020

#include "tiles_generic.h"
#include "z80_intf.h"
#include "bitswap.h"
#include "samples.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM[6];
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 gfxbank;
static INT32 vblank;
static INT32 flipscreen;
static INT32 previous_coin;

static INT32 enablestars;
static INT32 starspeed;
static INT32 stars_offset;
static INT32 stars_state;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[2];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

// for samples..
static INT32 asteroid_hit = 0;

static struct BurnInputInfo RedclashInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Redclash)

static struct BurnDIPInfo RedclashDIPList[]=
{
	DIP_OFFSET(0x0f)
	{0x00, 0xff, 0xff, 0xf7, NULL						},
	{0x01, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    4, "Difficulty?"				},
	{0x00, 0x01, 0x03, 0x03, "Easy?"					},
	{0x00, 0x01, 0x03, 0x02, "Medium?"					},
	{0x00, 0x01, 0x03, 0x01, "Hard?"					},
	{0x00, 0x01, 0x03, 0x00, "Hardest?"					},

	{0   , 0xfe, 0   ,    2, "Unknown"					},
	{0x00, 0x01, 0x04, 0x04, "Off"						},
	{0x00, 0x01, 0x04, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x00, 0x01, 0x08, 0x00, "Upright"					},
	{0x00, 0x01, 0x08, 0x08, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "High Score"				},
	{0x00, 0x01, 0x10, 0x00, "0"						},
	{0x00, 0x01, 0x10, 0x10, "10000"					},

	{0   , 0xfe, 0   ,    2, "Unknown"					},
	{0x00, 0x01, 0x20, 0x20, "Off"						},
	{0x00, 0x01, 0x20, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x00, 0x01, 0xc0, 0x00, "1"						},
	{0x00, 0x01, 0xc0, 0xc0, "3"						},
	{0x00, 0x01, 0xc0, 0x80, "5"						},
	{0x00, 0x01, 0xc0, 0x40, "7"						},

	{0   , 0xfe, 0   ,	 16, "Coin A"					},
	{0x01, 0x01, 0x0f, 0x04, "6 Coins 1 Credits"		},
	{0x01, 0x01, 0x0f, 0x05, "5 Coins 1 Credits"		},
	{0x01, 0x01, 0x0f, 0x06, "4 Coins 1 Credits"		},
	{0x01, 0x01, 0x0f, 0x08, "3 Coins 1 Credits"		},
	{0x01, 0x01, 0x0f, 0x0a, "2 Coins 1 Credits"		},
	{0x01, 0x01, 0x0f, 0x07, "3 Coins 2 Credits"		},
	{0x01, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"		},
	{0x01, 0x01, 0x0f, 0x09, "2 Coins 3 Credits"		},
	{0x01, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"		},
	{0x01, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"		},
	{0x01, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"		},
	{0x01, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"		},
	{0x01, 0x01, 0x0f, 0x03, "1 Coin  6 Credits"		},
	{0x01, 0x01, 0x0f, 0x02, "1 Coin  7 Credits"		},
	{0x01, 0x01, 0x0f, 0x01, "1 Coin  8 Credits"		},
	{0x01, 0x01, 0x0f, 0x00, "1 Coin  9 Credits"		},

	{0   , 0xfe, 0   ,   16, "Coin B"					},
	{0x01, 0x01, 0xf0, 0x40, "6 Coins 1 Credits"		},
	{0x01, 0x01, 0xf0, 0x50, "5 Coins 1 Credits"		},
	{0x01, 0x01, 0xf0, 0x60, "4 Coins 1 Credits"		},
	{0x01, 0x01, 0xf0, 0x80, "3 Coins 1 Credits"		},
	{0x01, 0x01, 0xf0, 0xa0, "2 Coins 1 Credits"		},
	{0x01, 0x01, 0xf0, 0x70, "3 Coins 2 Credits"		},
	{0x01, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x01, 0x01, 0xf0, 0x90, "2 Coins 3 Credits"		},
	{0x01, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"		},
	{0x01, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"		},
	{0x01, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"		},
	{0x01, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"		},
	{0x01, 0x01, 0xf0, 0x30, "1 Coin  6 Credits"		},
	{0x01, 0x01, 0xf0, 0x20, "1 Coin  7 Credits"		},
	{0x01, 0x01, 0xf0, 0x10, "1 Coin  8 Credits"		},
	{0x01, 0x01, 0xf0, 0x00, "1 Coin  9 Credits"		},
};

STDDIPINFO(Redclash)

static struct BurnDIPInfo ZerohourDIPList[]=
{
	DIP_OFFSET(0x0f)
	{0x00, 0xff, 0xff, 0xc7, NULL						},
	{0x01, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x00, 0x01, 0x08, 0x00, "Upright"					},
	{0x00, 0x01, 0x08, 0x08, "Cocktail"					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"				},
	{0x00, 0x01, 0x30, 0x00, "No Bonus"					},
	{0x00, 0x01, 0x30, 0x30, "5000"						},
	{0x00, 0x01, 0x30, 0x20, "8000"						},
	{0x00, 0x01, 0x30, 0x10, "10000"					},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x00, 0x01, 0xc0, 0x00, "2"						},
	{0x00, 0x01, 0xc0, 0xc0, "3"						},
	{0x00, 0x01, 0xc0, 0x80, "4"						},
	{0x00, 0x01, 0xc0, 0x40, "5"						},

	{0   , 0xfe, 0   ,   10, "Coin A"					},
	{0x01, 0x01, 0x0f, 0x06, "4 Coins 1 Credits"		},
	{0x01, 0x01, 0x0f, 0x08, "3 Coins 1 Credits"		},
	{0x01, 0x01, 0x0f, 0x0a, "2 Coins 1 Credits"		},
	{0x01, 0x01, 0x0f, 0x07, "3 Coins 2 Credits"		},
	{0x01, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"		},
	{0x01, 0x01, 0x0f, 0x09, "2 Coins 3 Credits"		},
	{0x01, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"		},
	{0x01, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"		},
	{0x01, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"		},
	{0x01, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"		},

	{0   , 0xfe, 0   ,   10, "Coin B"					},
	{0x01, 0x01, 0xf0, 0x60, "4 Coins 1 Credits"		},
	{0x01, 0x01, 0xf0, 0x80, "3 Coins 1 Credits"		},
	{0x01, 0x01, 0xf0, 0xa0, "2 Coins 1 Credits"		},
	{0x01, 0x01, 0xf0, 0x70, "3 Coins 2 Credits"		},
	{0x01, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x01, 0x01, 0xf0, 0x90, "2 Coins 3 Credits"		},
	{0x01, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"		},
	{0x01, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"		},
	{0x01, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"		},
	{0x01, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"		},
};

STDDIPINFO(Zerohour)

static void zerohour_snd_play(INT32 bank, INT32 address, UINT8 data)
{
	address &= 7;

	if (bank == 0)
	{
		switch (data) {
			case 0: // stop
				switch (address) { // can stop
					case 6: // enemy fire
						BurnSampleStop(address);
				}
				break;
			case 1: // play
				switch (address) { // retrig?
					case 7: // background sound loop
						if (BurnSampleGetStatus(address) != SAMPLE_PLAYING)
							BurnSamplePlay(address);
						return;
					case 1: // asteroid hit
						// don't play if red asteroid sound is playing!
						// game triggers 2 (red asteroid) + 1 (this/asteroid) for 2 (red asteroid).
						if (BurnSampleGetStatus(12) != SAMPLE_PLAYING) {
							// every other hit varies between 2 sounds
							asteroid_hit ^= 1;
							BurnSamplePlay(1 + asteroid_hit);
						}
						return;
					case 2: // red asteroid hit
						BurnSamplePlay(12);
						return;
					default:
						BurnSamplePlay(address);
						return;
				}
				break;

		}
	}
	else
	{
		if (address == 0x02 && data == 0xff) data = 1;

		address = 8 + address - 1;
		switch (data) {
			case 0:
				switch (address) { // can stop
					case 9: // background loop
						BurnSampleSetLoop(address, 0);
						BurnSampleStop(address);
						return;
					case 8: // thrust
						BurnSampleStop(address);
				}
				break;
			case 1:
				switch (address) { // can retrig?
					case 9: // background loop
						BurnSampleStop(address);
						BurnSampleSetLoop(address, 1);
						BurnSamplePlay(address);
						return;
					case 8: // thrust
					case 10:// bonus score accumulation
						if (BurnSampleGetStatus(address) != SAMPLE_PLAYING)
							BurnSamplePlay(address);
						return;
					default:
						BurnSamplePlay(address);
						return;
				}
				break;
		}
	}
}

static void __fastcall zerohour_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x5000: // Zero Hour - Fire
		case 0x5001: // Zero Hour - Asteroid Hit 1  ??
		case 0x5002: // Zero Hour - Asteroid Hit 2  ??
		case 0x5003: // Zero Hour - Enemy Ship Descends ???
		case 0x5004: // Zero Hour - Enemy Shield Hit
		case 0x5005: // Zero Hour - Crash and Die
		case 0x5006: // Zero Hour - Enemy Fire
		case 0x5007: // Zero Hour - Bonus Round Time Running Out Loop
			zerohour_snd_play(0, address&7, data);
		return; // discrete sound board

		case 0x5800:
			starspeed = (starspeed & ~1) | (data & 1) << 0;
		return;

		case 0x5801: // Zero Hour - Thrust
		case 0x5802: // Zero Hour - Background Sound Loop (0xff = on, 0 = off)
		case 0x5803: // Zero Hour - Bonus Round Score Accumulate Loop
		case 0x5804: // Zero Hour - Credit
			zerohour_snd_play(1, address&7, data);
		return; // discrete sound board

		case 0x5805:
		case 0x5806:
			starspeed = (starspeed & ~(1 << (address - 0x5804))) | (data & 1) << (address - 0x5804);
		return;

		case 0x5807:
			flipscreen = data & 0x01;
		return;

		case 0x7000:
			enablestars = 1; // star reset
			stars_offset = 0; // right?
			stars_state = 0;
		return;

		case 0x7800:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;
	}
}

static UINT8 __fastcall zerohour_read(UINT16 address)
{
	switch (address)
	{
		case 0x4800:
			return DrvInputs[0];

		case 0x4801:
			return DrvInputs[1] ^ (vblank ? 0x80 : 0x40);

		case 0x4802:
			return DrvDips[0];

		case 0x4803:
			return DrvDips[1];
	}

	return 0;
}

static void __fastcall redclash_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x5801:
			gfxbank = data & 0x01;
		return;

		case 0x5802:
		case 0x5803:
		case 0x5804:
		return; // nop

		case 0x5000:
		case 0x5001:
		case 0x5002:
		case 0x5003:
		case 0x5004:
		case 0x5005:
		case 0x5006:
		case 0x5007:
		return; // sound board(?)
	}

	zerohour_write(address, data);
}

static tilemap_callback( fg )
{
	INT32 code =  DrvVidRAM[offs];
	INT32 color = (DrvVidRAM[offs] & 0x70) >> 4;

	// score panel colors are determined differently: P1=5, TOP=4, P2=7
	if ((offs & 0x1f) > 0x1b)
		color = (((offs >> 5) + 12) & 0x1f) >> 3 ^ 7;

	TILE_SET_INFO(0, code, color, 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetReset(0);

	BurnSampleReset();

	flipscreen = 0;
	gfxbank = 0;
	previous_coin = 0;


	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM			= Next; Next += 0x003000;

	DrvGfxROM[0]		= Next; Next += 0x002000;
	DrvGfxROM[1]		= Next; Next += 0x008000;
	DrvGfxROM[2]		= Next; Next += 0x008000;
	DrvGfxROM[3]		= Next; Next += 0x008000;
	DrvGfxROM[4]		= Next; Next += 0x008000;

	DrvColPROM			= Next; Next += 0x000040;

	DrvPalette			= (UINT32*)Next; Next += 0x0081 * sizeof(UINT32);

	AllRam				= Next;

	DrvZ80RAM			= Next; Next += 0x000800;
	DrvVidRAM			= Next; Next += 0x000400;
	DrvSprRAM			= Next; Next += 0x000400;

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[2]  = { 64, 0 };
	INT32 Plane1[2]  = { 1, 0 };
	INT32 XOffs0[8]  = { STEP8(7,-1) };
	INT32 YOffs0[8]  = { STEP8(0,8) };
	INT32 XOffs1[8]  = { STEP8(0, 2) };
	INT32 YOffs1[8]  = { STEP8(7*16,-16) };
	INT32 XOffs2[16] = { STEP8(24*2,2), STEP8(8*64+24*2,2) };
	INT32 YOffs2[16] = { STEP8(23*64,-64), STEP8(7*64,-64) };
	INT32 XOffs3[24] = { STEP8(0,2), STEP8(8*2,2), STEP8(16*2,2) };
	INT32 YOffs3[24] = { STEP8(23*64,-64), STEP8(15*64,-64), STEP8(7*64,-64) };
	INT32 XOffs4[16] = { STEP8(0,2), STEP8(8*2,2) };
	INT32 YOffs4[16] = { STEP8(15*64,-64), STEP8(7*64,-64) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x2000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM[0], 0x0800);

	GfxDecode(0x0080, 2,  8,  8, Plane0, XOffs0, YOffs0, 0x080, tmp, DrvGfxROM[0]);

	for (INT32 i = 0; i < 0x2000; i++) {
		tmp[i] = DrvGfxROM[2][(i & ~0x003e) | ((i & 0x0e) << 2) | ((i & 0x30) >> 3)];
	}

	GfxDecode(0x0200, 2,  8,  8, Plane1, XOffs1, YOffs1, 0x080, tmp, DrvGfxROM[1]);

	memcpy (tmp, DrvGfxROM[2], 0x2000);

	GfxDecode(0x0020, 2, 16, 16, Plane1, XOffs2, YOffs2, 0x800, tmp, DrvGfxROM[2]);
	GfxDecode(0x0020, 2, 24, 24, Plane1, XOffs3, YOffs3, 0x800, tmp, DrvGfxROM[3]);
	GfxDecode(0x0040, 2, 16, 16, Plane1, XOffs4, YOffs4, 0x400, tmp + 0, DrvGfxROM[4]);
	GfxDecode(0x0040, 2, 16, 16, Plane1, XOffs4, YOffs4, 0x400, tmp + 4, DrvGfxROM[4] + 0x4000);

	BurnFree(tmp);

	return 0;
}

static void SamplesInit()
{
	BurnUpdateProgress(0.0, _T("Loading samples..."), 0);
	bBurnSampleTrimSampleEnd = 1;
	BurnSampleInit(0);
	BurnSampleSetAllRoutesAllSamples(0.10, BURN_SND_ROUTE_BOTH);

	if (BurnSampleGetStatus(0) == -1) { // Samples not found
		BurnSampleSetAllRoutesAllSamples(0.00, BURN_SND_ROUTE_BOTH);
	} else {
		bprintf(0, _T("Using SFX samples!\n"));
	}
	BurnSampleSetBuffered(ZetTotalCycles, 4000000); // SetBuffered() after GetStatus() to avoid warning
}

static INT32 ZerohourInit()
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvZ80ROM    + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM    + 0x0800, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM    + 0x1000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM    + 0x1800, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM    + 0x2000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM    + 0x2800, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[0] + 0x0000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[2] + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x0800, k++, 1)) return 1;

		if (BurnLoadRom(DrvColPROM   + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x0020, k++, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x2fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,		0x3000, 0x37ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0x3800, 0x3bff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,		0x4000, 0x43ff, MAP_RAM);
	ZetSetWriteHandler(zerohour_write);
	ZetSetReadHandler(zerohour_read);
	ZetClose();

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, fg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0],  2,  8,  8, 0x02000, 0x000, 0x07);
	GenericTilemapSetGfx(1, DrvGfxROM[1],  2,  8,  8, 0x08000, 0x020, 0x0f);
	GenericTilemapSetGfx(2, DrvGfxROM[2],  2, 16, 16, 0x08000, 0x020, 0x0f);
	GenericTilemapSetGfx(3, DrvGfxROM[3],  2, 24, 24, 0x08000, 0x020, 0x0f);
	GenericTilemapSetGfx(4, DrvGfxROM[4],  2, 16, 16, 0x08000, 0x020, 0x0f);
	GenericTilemapSetOffsets(0, 0, -32);
	GenericTilemapSetTransparent(0, 0);

	SamplesInit();

	DrvDoReset();

	return 0;
}

static INT32 RedclashInit()
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvZ80ROM    + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM    + 0x0800, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM    + 0x1000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM    + 0x1800, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM    + 0x2000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM    + 0x2800, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[0] + 0x0000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[1] + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x1000, k++, 1)) return 1;
		memcpy (DrvGfxROM[2] + 0x0000, DrvGfxROM[1] + 0x0000, 0x0800);
		memcpy (DrvGfxROM[2] + 0x1000, DrvGfxROM[1] + 0x0800, 0x0800);
		memcpy (DrvGfxROM[2] + 0x0800, DrvGfxROM[1] + 0x1000, 0x0800);
		memcpy (DrvGfxROM[2] + 0x1800, DrvGfxROM[1] + 0x1800, 0x0800);

		if (BurnLoadRom(DrvColPROM   + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x0020, k++, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x2fff, MAP_ROM);
	ZetMapMemory(DrvVidRAM,		0x4000, 0x43ff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM,		0x6000, 0x67ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0x6800, 0x6bff, MAP_RAM);
	ZetSetWriteHandler(redclash_write);
	ZetSetReadHandler(zerohour_read);
	ZetClose();

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, fg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0],  2,  8,  8, 0x02000, 0x000, 0x07);
	GenericTilemapSetGfx(1, DrvGfxROM[1],  2,  8,  8, 0x08000, 0x020, 0x0f);
	GenericTilemapSetGfx(2, DrvGfxROM[2],  2, 16, 16, 0x07e00, 0x020, 0x0f);
	GenericTilemapSetGfx(3, DrvGfxROM[3],  2, 24, 24, 0x08000, 0x020, 0x0f);
	GenericTilemapSetGfx(4, DrvGfxROM[4],  2, 16, 16, 0x08000, 0x020, 0x0f);
	GenericTilemapSetOffsets(0, -8, -32);
	GenericTilemapSetTransparent(0, 0);

	SamplesInit();

	DrvDoReset();

	return 0;
}

static INT32 RedclashtInit()
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvZ80ROM    + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM    + 0x1000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM    + 0x2000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[0] + 0x0000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[1] + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x1000, k++, 1)) return 1;
		memcpy (DrvGfxROM[2] + 0x0000, DrvGfxROM[1] + 0x0000, 0x0800);
		memcpy (DrvGfxROM[2] + 0x1000, DrvGfxROM[1] + 0x0800, 0x0800);
		memcpy (DrvGfxROM[2] + 0x0800, DrvGfxROM[1] + 0x1000, 0x0800);
		memcpy (DrvGfxROM[2] + 0x1800, DrvGfxROM[1] + 0x1800, 0x0800);

		if (BurnLoadRom(DrvColPROM   + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x0020, k++, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x2fff, MAP_ROM);
	ZetMapMemory(DrvVidRAM,		0x4000, 0x43ff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM,		0x6000, 0x67ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0x6800, 0x6bff, MAP_RAM);
	ZetSetWriteHandler(redclash_write);
	ZetSetReadHandler(zerohour_read);
	ZetClose();

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, fg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0],  2,  8,  8, 0x02000, 0x000, 0x07);
	GenericTilemapSetGfx(1, DrvGfxROM[1],  2,  8,  8, 0x08000, 0x020, 0x0f);
	GenericTilemapSetGfx(2, DrvGfxROM[2],  2, 16, 16, 0x07e00, 0x020, 0x0f);
	GenericTilemapSetGfx(3, DrvGfxROM[3],  2, 24, 24, 0x08000, 0x020, 0x0f);
	GenericTilemapSetGfx(4, DrvGfxROM[4],  2, 16, 16, 0x08000, 0x020, 0x0f);
	GenericTilemapSetOffsets(0, -8, -32);
	GenericTilemapSetTransparent(0, 0);

	SamplesInit();

	DrvDoReset();

	return 0;
}


static INT32 DrvExit()
{
	GenericTilesExit();
	ZetExit();
	BurnSampleExit();

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	UINT32 color_lut[32];

	for (INT32 i = 0; i < 0x20; i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 1;
		INT32 bit1 = (DrvColPROM[i] >> 5) & 1;
		INT32 r = 0x47 * bit0 + 0x97 * bit1;

		bit0 = (DrvColPROM[i] >> 2) & 1;
		bit1 = (DrvColPROM[i] >> 6) & 1;
		INT32 g = 0x47 * bit0 + 0x97 * bit1;

		bit0 = (DrvColPROM[i] >> 4) & 1;
		bit1 = (DrvColPROM[i] >> 7) & 1;
		INT32  b = 0x47 * bit0 + 0x97 * bit1;

		color_lut[i] = BurnHighCol(r,g,b,0);

		r = ((i & 0x08) ? 0x97 : 0) + ((i & 0x10) ? 0x47 : 0);
		g = ((i & 0x02) ? 0x97 : 0) + ((i & 0x04) ? 0x47 : 0);
		b = ((i & 0x01) ? 0x47 : 0);

		DrvPalette[0x60 + i] = BurnHighCol(r,g,b,0);
	}

	for (INT32 i = 0; i < 0x20; i++)
	{
		INT32 j = BITSWAP08(DrvColPROM[0x20 + i], 4, 5, 6, 7, 0, 1, 2, 3);
		DrvPalette[0x00 + i] = color_lut[((i << 3) & 0x18) | ((i >> 2) & 0x07)]; // characters
		DrvPalette[0x20 + i] = color_lut[j & 0xf];	// sprites
		DrvPalette[0x40 + i] = color_lut[j >> 4];	// ""
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0x400 - 0x20; offs >= 0; offs -= 0x20)
	{
		INT32 i = 0;
		while (i < 0x20 && DrvSprRAM[offs + i] != 0) i += 4;

		while (i > 0)
		{
			i -= 4;
			if ((DrvSprRAM[offs + i] & 0x80) == 0) continue;

			INT32 attr  =  DrvSprRAM[offs + i + 1];
			INT32 color =  DrvSprRAM[offs + i + 2] & 0x27;
			color = ((color & 0x20) >> 2) | (color & 0x7);
			INT32 sx    =  DrvSprRAM[offs + i + 3];
			INT32 sy    = (DrvSprRAM[offs + i + 0] & 0x07) + (offs / 4);
			INT32 size  = (DrvSprRAM[offs + i + 0] & 0x18) >> 3;
			INT32 gfxno = 0, code = 0;
			if (size == 0) continue;

			switch (size)
			{
				case 3: gfxno = 3; code = (attr >> 4) + (gfxbank << 4); break;
				case 2: gfxno = (DrvSprRAM[offs + i + 0] & 0x20) ? 4 : 2;
				code = (DrvSprRAM[offs + i + 0] & 0x20) ?
					((attr >> 3) + ((attr & 0x02) << 5) + (gfxbank << 5)) :
					((attr >> 4) + (gfxbank << 4));
				break;
				case 1: gfxno = 1; code = attr; break;
			}

			DrawGfxMaskTile(0, gfxno, code, sx - 8, sy - 48, 0, 0, color, 0);
			DrawGfxMaskTile(0, gfxno, code, sx - (256 + 8), sy - 48, 0, 0, color, 0); // wrap
		}
	}
}

static void draw_bullets()
{
	for (INT32 offs = 0; offs < 0x20; offs++)
	{
		INT32 sx = 8 * offs;// + (DrvVidRAM[offs] & 0x07);
		INT32 sy = 0xff - DrvVidRAM[offs + 0x20];

		// width and color are from the same bitfield
		INT32 width = 8 - (DrvVidRAM[offs] >> 5 & 6);
		INT32 color = (DrvVidRAM[offs] >> 3 & 0x10) | 5;

		if (flipscreen)
		{
			sx = 240 - sx;
		}

		sx -= 8;
		sx -= (DrvVidRAM[offs] >> 3) & 7;
		sy -= 32;

		for (INT32 y = 0; y < 2; y++)
			for (INT32 x = 0; x < width; x++)
			{
				INT32 syy = sy - y;
				INT32 sxx = sx + x;
				if (sxx >= 0 && syy >= 0 && sxx < nScreenWidth && syy < nScreenHeight) {
					pTransDraw[syy * nScreenWidth + sxx] = color;
			}
		}
	}
}

static void redclash_update_stars()
{
	static INT32 count = 0;

	if (enablestars == 0)
		return;

	count = (count + 1) & 1;

	if (count == 0)
	{
		stars_offset += ((starspeed << 1) - 0x09);
		stars_offset &= 0xffff;
		stars_state = 0;
	}
	else
	{
		stars_state = 0x1fc71;
	}
}

static void redclash_draw_stars(INT32 palette_offset, INT32 sraider, INT32 firstx, INT32 lastx)
{
	UINT8 tempbit, feedback, star_color, hcond,vcond;

	if (enablestars == 0)
		return;

	UINT32 state = stars_state;

	for (INT32 i = 0; i < 256 * 256; i++)
	{
		INT32 xloc =  (stars_offset + i) & 0xff;
		INT32 yloc = ((stars_offset + i) >> 8) & 0xff;

		xloc -=  8;
		yloc -= 32;

		if ((state & 0x10000) == 0)
			tempbit = 1;
		else
			tempbit = 0;

		if ((state & 0x00020) != 0)
			feedback = tempbit ^ 1;
		else
			feedback = tempbit ^ 0;

		hcond = ((xloc + 8) & 0x10) >> 4;

		if (sraider)
			vcond = 1;
		else
			vcond = yloc & 0x01;

		if (xloc >= 0 && xloc < nScreenWidth &&
			yloc >= 0 && yloc < nScreenHeight)
		{
			if ((hcond ^ vcond) == 0)
			{
				if (((state & 0x000ff) == 0x000ff) && (feedback == 0))
				{
					if ((xloc >= firstx) && (xloc <= lastx))
					{
						star_color = (state >> 9) & 0x1f;
						pTransDraw[(yloc * nScreenWidth) + xloc] = palette_offset + star_color;
					}
				}
			}
		}

		state = ((state << 1) & 0x1fffe) | feedback;
	}
}


static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	BurnTransferClear(0x80); // black

	if (nBurnLayer & 1) redclash_draw_stars(0x60, true, 0x00, 0xff);
	if (nBurnLayer & 2) draw_bullets();
	if (nSpriteEnable & 1) draw_sprites();

	if (nBurnLayer & 4) GenericTilemapDraw(0, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		INT32 coin = (DrvJoy3[0] & 1) | ((DrvJoy3[1] & 1) << 1);

		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}

		ZetOpen(0);
		if ((previous_coin & 1) == 1 && (coin & 1) == 0) ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		if ((previous_coin & 2) == 2 && (coin & 2) == 0) ZetNmi();
		ZetClose();

		previous_coin = coin;
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[1] = { 4000000 / 60 };
	INT32 nCyclesDone[1] = { 0 };

	ZetNewFrame();

	vblank = 0;

	ZetOpen(0);
	for (INT32 i = 0; i < nInterleave; i++) {
		if (i == 224) vblank = 1;

		CPU_RUN(0, Zet);
	}
	ZetClose();

	if (pBurnSoundOut) {
		BurnSampleRender(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	redclash_update_stars();

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

		ZetScan(nAction);

		BurnSampleScan(nAction, pnMin);

		SCAN_VAR(flipscreen);
		SCAN_VAR(gfxbank);
		SCAN_VAR(previous_coin);

		SCAN_VAR(asteroid_hit);

		SCAN_VAR(enablestars);
		SCAN_VAR(starspeed);
		SCAN_VAR(stars_offset);
		SCAN_VAR(stars_state);
	}

	return 0;
}

static struct BurnSampleInfo zerohourSampleDesc[] = {
	{ "Zero Hour - Fire", SAMPLE_NOLOOP },
	{ "Zero Hour - Asteroid Hit 1",	SAMPLE_NOLOOP },
	{ "Zero Hour - Asteroid Hit 2",	SAMPLE_NOLOOP },
	{ "Zero Hour - Enemy Ship Descends", SAMPLE_NOLOOP },
	{ "Zero Hour - Enemy Shield Hit", SAMPLE_NOLOOP },
	{ "Zero Hour - Crash and Die", SAMPLE_NOLOOP },
	{ "Zero Hour - Enemy Fire",	SAMPLE_NOLOOP },
	{ "Zero Hour - Bonus Round Time Running Out Loop", SAMPLE_NOLOOP },

	{ "Zero Hour - Thrust",	SAMPLE_NOLOOP },
	{ "Zero Hour - Background Sound Loop", SAMPLE_NOLOOP },
	{ "Zero Hour - Bonus Round Score Accumulate Loop", SAMPLE_NOLOOP },
	{ "Zero Hour - Credit",	SAMPLE_NOLOOP },
	{ "Zero Hour - Asteroid Hit Red", SAMPLE_NOLOOP }, // this is a placeholder synthesized by dink May 27, 2021
	{ "", 0 }
};

STD_SAMPLE_PICK(zerohour)
STD_SAMPLE_FN(zerohour)


// Zero Hour (set 1)

static struct BurnRomInfo zerohourRomDesc[] = {
	{ "ze1_1.c8",		0x0800, 0x0dff4b48, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "ze2_2.c7",		0x0800, 0xcf41b6ac, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ze3_3.c6",		0x0800, 0x5ef48b67, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ze4_4.c5",		0x0800, 0x25c5872d, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ze5_5.c4",		0x0800, 0xd7ce3add, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "ze6_6.c3",		0x0800, 0x8a93ae6e, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "z9",				0x0800, 0x17ae6f13, 2 | BRF_GRA },           //  6 Characters

	{ "z7",				0x0800, 0x4c12f59d, 3 | BRF_GRA },           //  7 Sprites
	{ "z8",				0x0800, 0x6b9a6b6e, 3 | BRF_GRA },           //  8

	{ "z1.ic2",			0x0020, 0xb55aee56, 4 | BRF_GRA },           //  9 Color PROMs
	{ "z2.n2",			0x0020, 0x9adabf46, 4 | BRF_GRA },           // 10
	{ "z3.u6",			0x0020, 0x27fa3a50, 0 | BRF_OPT },           // 11
};

STD_ROM_PICK(zerohour)
STD_ROM_FN(zerohour)

struct BurnDriver BurnDrvZerohour = {
	"zerohour", NULL, NULL, "zerohour", "1980",
	"Zero Hour (set 1)\0", NULL, "Universal", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, zerohourRomInfo, zerohourRomName, NULL, NULL, zerohourSampleInfo, zerohourSampleName, RedclashInputInfo, ZerohourDIPInfo,
	ZerohourInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	192, 248, 3, 4
};


// Zero Hour (set 2)

static struct BurnRomInfo zerohouraRomDesc[] = {
	{ "z1_1.c8",		0x0800, 0x77418b5d, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "z2_2.c7",		0x0800, 0x9b23b5ac, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "z3_3.c6",		0x0800, 0x7aa25c95, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "z4_4.c5",		0x0800, 0xb0a26dea, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "z5_5.c4",		0x0800, 0xd7ce3add, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "z6_6.c3",		0x0800, 0x8a93ae6e, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "z9",				0x0800, 0x17ae6f13, 2 | BRF_GRA },           //  6 Characters

	{ "z7",				0x0800, 0x4c12f59d, 3 | BRF_GRA },           //  7 Sprites
	{ "z8",				0x0800, 0x6b9a6b6e, 3 | BRF_GRA },           //  8

	{ "z1.ic2",			0x0020, 0xb55aee56, 4 | BRF_GRA },           //  9 Color PROMs
	{ "z2.n2",			0x0020, 0x9adabf46, 4 | BRF_GRA },           // 10
	{ "z3.u6",			0x0020, 0x27fa3a50, 0 | BRF_OPT },           // 11
};

STD_ROM_PICK(zerohoura)
STD_ROM_FN(zerohoura)

struct BurnDriver BurnDrvZerohoura = {
	"zerohoura", "zerohour", NULL, "zerohour", "1980",
	"Zero Hour (set 2)\0", NULL, "Universal", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, zerohouraRomInfo, zerohouraRomName, NULL, NULL, zerohourSampleInfo, zerohourSampleName, RedclashInputInfo, ZerohourDIPInfo,
	ZerohourInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	192, 248, 3, 4
};


// Zero Hour (Inder)

static struct BurnRomInfo zerohouriRomDesc[] = {
	{ "zhi.c8",			0x0800, 0x0dff4b48, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "zhi.c7",			0x0800, 0xdcfefb4c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "zhi.c6",			0x0800, 0xddc66d36, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "zhi.c5",			0x0800, 0x25c5872d, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "zhi.c4",			0x0800, 0xd7ce3add, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "zhi.c3",			0x0800, 0x29dee5e4, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "zhi.k7",			0x0800, 0x17ae6f13, 2 | BRF_GRA },           //  6 Characters

	{ "zhi.k4",			0x0800, 0x4c12f59d, 3 | BRF_GRA },           //  7 Sprites
	{ "zhi.k5",			0x0800, 0x6b9a6b6e, 3 | BRF_GRA },           //  8

	{ "z1.ic2",			0x0020, 0xb55aee56, 4 | BRF_GRA },           //  9 Color PROMs
	{ "z2.n2",			0x0020, 0x9adabf46, 4 | BRF_GRA },           // 10
	{ "z3.u6",			0x0020, 0x27fa3a50, 0 | BRF_OPT },           // 11
};

STD_ROM_PICK(zerohouri)
STD_ROM_FN(zerohouri)

struct BurnDriver BurnDrvZerohouri = {
	"zerohouri", "zerohour", NULL, "zerohour", "1980",
	"Zero Hour (Inder)\0", NULL, "bootleg (Inder SA)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, zerohouriRomInfo, zerohouriRomName, NULL, NULL, zerohourSampleInfo, zerohourSampleName, RedclashInputInfo, ZerohourDIPInfo,
	ZerohourInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	192, 248, 3, 4
};


// Red Clash

static struct BurnRomInfo redclashRomDesc[] = {
	{ "rc1.8c",			0x0800, 0xfd90622a, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "rc2.7c",			0x0800, 0xc8f33440, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rc3.6c",			0x0800, 0x2172b1e9, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rc4.5c",			0x0800, 0x55c0d1b5, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "rc5.4c",			0x0800, 0x744b5261, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "rc6.3c",			0x0800, 0xfa507e17, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "rc6.12a",		0x0800, 0xda9bbcc2, 2 | BRF_GRA },           //  6 Characters

	{ "rc4.4m",			0x1000, 0x483a1293, 3 | BRF_GRA },           //  7 Sprites
	{ "rc5.5m",			0x1000, 0xc45d9601, 3 | BRF_GRA },           //  8

	{ "1.12f",			0x0020, 0x43989681, 4 | BRF_GRA },           //  9 Color PROMs
	{ "2.4a",			0x0020, 0x9adabf46, 4 | BRF_GRA },           // 10
	{ "3.11e",			0x0020, 0x27fa3a50, 0 | BRF_OPT },           // 11
};

STD_ROM_PICK(redclash)
STD_ROM_FN(redclash)

struct BurnDriver BurnDrvRedclash = {
	"redclash", NULL, NULL, NULL, "1981",
	"Red Clash\0", "NO Sound", "Kaneko", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, redclashRomInfo, redclashRomName, NULL, NULL, NULL, NULL, RedclashInputInfo, RedclashDIPInfo,
	RedclashInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	192, 248, 3, 4
};


// Red Clash (Tehkan, set 1)

static struct BurnRomInfo redclashtRomDesc[] = {
	{ "11.11c",			0x1000, 0x695e070e, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "13.7c",			0x1000, 0xc2090318, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "12.9c",			0x1000, 0xb60e5ada, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "6.12a",			0x0800, 0xda9bbcc2, 2 | BRF_GRA },           //  3 Characters

	{ "14.3e",			0x1000, 0x483a1293, 3 | BRF_GRA },           //  4 Sprites
	{ "15.3d",			0x1000, 0xc45d9601, 3 | BRF_GRA },           //  5

	{ "1.12f",			0x0020, 0x43989681, 4 | BRF_GRA },           //  6 Color PROMs
	{ "2.4a",			0x0020, 0x9adabf46, 4 | BRF_GRA },           //  7
	{ "3.11e",			0x0020, 0x27fa3a50, 0 | BRF_OPT },           //  8
};

STD_ROM_PICK(redclasht)
STD_ROM_FN(redclasht)

struct BurnDriver BurnDrvRedclasht = {
	"redclasht", "redclash", NULL, NULL, "1981",
	"Red Clash (Tehkan, set 1)\0", "NO Sound", "Kaneko (Tehkan license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, redclashtRomInfo, redclashtRomName, NULL, NULL, NULL, NULL, RedclashInputInfo, RedclashDIPInfo,
	RedclashtInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	192, 248, 3, 4
};


// Red Clash (Tehkan, set 2)

static struct BurnRomInfo redclashtaRomDesc[] = {
	{ "rc1.11c",		0x1000, 0x5b62ff5a, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "rc3.7c",			0x1000, 0x409c4ee7, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rc2.9c",			0x1000, 0x5f215c9a, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "rc6.12a",		0x0800, 0xda9bbcc2, 2 | BRF_GRA },           //  3 Characters

	{ "rc4.3e",			0x1000, 0x64ca8b63, 3 | BRF_GRA },           //  4 Sprites
	{ "rc5.3d",			0x1000, 0xfce610a2, 3 | BRF_GRA },           //  5

	{ "1.12f",			0x0020, 0x43989681, 4 | BRF_GRA },           //  6 Color PROMs
	{ "2.4a",			0x0020, 0x9adabf46, 4 | BRF_GRA },           //  7
	{ "3.11e",			0x0020, 0x27fa3a50, 0 | BRF_OPT },           //  8
};

STD_ROM_PICK(redclashta)
STD_ROM_FN(redclashta)

struct BurnDriver BurnDrvRedclashta = {
	"redclashta", "redclash", NULL, NULL, "1981",
	"Red Clash (Tehkan, set 2)\0", "NO Sound", "Kaneko (Tehkan license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, redclashtaRomInfo, redclashtaRomName, NULL, NULL, NULL, NULL, RedclashInputInfo, RedclashDIPInfo,
	RedclashtInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	192, 248, 3, 4
};


// Red Clash (Suntronics)

static struct BurnRomInfo redclashsRomDesc[] = {
	{ "1.11c",			0x1000, 0x62275f85, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "3.7c",			0x1000, 0xc2090318, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "2.9c",			0x1000, 0xb60e5ada, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "6.a12",			0x0800, 0xda9bbcc2, 2 | BRF_GRA },           //  3 Characters

	{ "4.3e",			0x1000, 0x483a1293, 3 | BRF_GRA },           //  4 Sprites
	{ "5.3d",			0x1000, 0xc45d9601, 3 | BRF_GRA },           //  5

	{ "1.12f",			0x0020, 0x43989681, 4 | BRF_GRA },           //  6 Color PROMs
	{ "2.4a",			0x0020, 0x9adabf46, 4 | BRF_GRA },           //  7
	{ "3.11e",			0x0020, 0x27fa3a50, 0 | BRF_OPT },           //  8
};

STD_ROM_PICK(redclashs)
STD_ROM_FN(redclashs)

struct BurnDriver BurnDrvRedclashs = {
	"redclashs", "redclash", NULL, NULL, "1982",
	"Red Clash (Suntronics)\0", "NO Sound", "Kaneko (Suntronics license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, redclashsRomInfo, redclashsRomName, NULL, NULL, NULL, NULL, RedclashInputInfo, RedclashDIPInfo,
	RedclashtInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	192, 248, 3, 4
};
