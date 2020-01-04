// FB Alpha Monkey Magic driver module
// Based on MAME driver by Dirk Best

#include "tiles_generic.h"
#include "z80_intf.h"
#include "samples.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvI8085ROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvColROM;
static UINT8 *DrvI8085RAM;
static UINT8 *DrvVidRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 ball_pos[2];
static UINT8 video_color;
static UINT8 prev_audio;
static INT32 vblank;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[1];
static UINT8 DrvInputs[1];

static INT32 Paddle = 0;

static UINT8 DrvReset;

static struct BurnInputInfo MmagicInputList[] = {
	{"Coin",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 coin"	},
	{"Start 1",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 start"	},
	{"Start 2",		BIT_DIGITAL,	DrvJoy1 + 2,	"p2 start"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"   },
	{"P1 Right",	BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"  },
	{"P1 Button 1", BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dips",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Mmagic)

static struct BurnDIPInfo MmagicDIPList[]=
{
	{0x07, 0xff, 0xff, 0xff, NULL		},

	{0   , 0xfe, 0   ,    2, "Service Mode"	},
	{0x07, 0x01, 0x01, 0x01, "Off"		},
	{0x07, 0x01, 0x01, 0x00, "On"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"	},
	{0x07, 0x01, 0x06, 0x00, "30000"	},
	{0x07, 0x01, 0x06, 0x02, "20000"	},
	{0x07, 0x01, 0x06, 0x04, "15000"	},
	{0x07, 0x01, 0x06, 0x06, "10000"	},

	{0   , 0xfe, 0   ,    4, "Lives"	},
	{0x07, 0x01, 0x18, 0x00, "6"		},
	{0x07, 0x01, 0x18, 0x08, "5"		},
	{0x07, 0x01, 0x18, 0x10, "4"		},
	{0x07, 0x01, 0x18, 0x18, "3"		},
};

STDDIPINFO(Mmagic)

static void __fastcall mmagic_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x8002:
		case 0x8003:
			ball_pos[address & 1] = data;
		return;
	}
}

static UINT8 __fastcall mmagic_read(UINT16 address)
{
	switch (address)
	{
		case 0x8004:
			return vblank ? 0xff : 0xfe;
	}

	return 0;
}

static void audio_write(UINT8 data)
{
	if (data != prev_audio)
	{
		if ((data & 0x80) == 0)
		{
			BurnSamplePlay(~prev_audio & 7);
		}

		prev_audio = data;
	}
}

static void __fastcall mmagic_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x80:
			video_color = data;
		return;

		case 0x81:
			audio_write(data);
		return;
	}
}

static UINT8 __fastcall mmagic_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x85:
			return Paddle & 0xff;

		case 0x86:
			return DrvInputs[0];

		case 0x87:
			return DrvDips[0];
	}

	return 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	BurnSampleReset();

	ball_pos[0] = 0;
	ball_pos[1] = 0;
	prev_audio = 0;
	video_color = 0;
	Paddle = 0x70; // middle

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvI8085ROM		= Next; Next += 0x001400;

	DrvGfxROM		= Next; Next += 0x000600;
	DrvColROM		= Next; Next += 0x000200;

	DrvPalette		= (UINT32*)Next; Next += 0x0102 * sizeof(UINT32);

	AllRam			= Next;

	DrvI8085RAM		= Next; Next += 0x000200;
	DrvVidRAM		= Next; Next += 0x000200;

	RamEnd			= Next;

	MemEnd			= Next;

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
		if (BurnLoadRom(DrvI8085ROM + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvI8085ROM + 0x0400,  1, 1)) return 1;
		if (BurnLoadRom(DrvI8085ROM + 0x0800,  2, 1)) return 1;
		if (BurnLoadRom(DrvI8085ROM + 0x0c00,  3, 1)) return 1;
		if (BurnLoadRom(DrvI8085ROM + 0x1000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM   + 0x0000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM   + 0x0200,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM   + 0x0400,  7, 1)) return 1;

		if (BurnLoadRom(DrvColROM   + 0x0000,  8, 1)) return 1;
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvI8085ROM,	0x0000, 0x13ff, MAP_ROM);
	ZetMapMemory(DrvI8085RAM,	0x2000, 0x21ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,		0x3000, 0x31ff, MAP_RAM);
	ZetSetWriteHandler(mmagic_write);
	ZetSetReadHandler(mmagic_read);
	ZetSetOutHandler(mmagic_write_port);
	ZetSetInHandler(mmagic_read_port);
	ZetClose();

	BurnSampleInit(0);
	BurnSampleSetAllRoutesAllSamples(0.20, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

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
	for (INT32 i = 0; i < 8; i++)
	{
		DrvPalette[i] = BurnHighCol((i&1)?0xff:0, (i&2)?0xff:0, (i&4)?0xff:0, 0);
	}
}

static void draw_layer()
{
	INT32 base_color = (video_color & 0x40) << 1;

	for (INT32 y = 0; y < 192 / 12; y++)
	{
		for (INT32 x = 0; x < 256 / 8; x++)
		{
			UINT8 code = DrvVidRAM[(y * 32) + x] & 0x7f;
			UINT8 color = (DrvColROM[code | base_color] ^ 0xff) & 7;

			for (INT32 tx = 0; tx < 12; tx++)
			{
				UINT8 gfx = DrvGfxROM[(code << 4) + tx];

				UINT16 *dest = pTransDraw + (y * 12 + tx) * nScreenWidth + (x * 8);

				dest[0] = (gfx & 0x10) ? 0 : color;
				dest[1] = (gfx & 0x20) ? 0 : color;
				dest[2] = (gfx & 0x40) ? 0 : color;
				dest[3] = (gfx & 0x80) ? 0 : color;

				dest[4] = (gfx & 0x01) ? 0 : color;
				dest[5] = (gfx & 0x02) ? 0 : color;
				dest[6] = (gfx & 0x04) ? 0 : color;
				dest[7] = (gfx & 0x08) ? 0 : color;
			}
		}
	}
}

static void draw_ball()
{
	if (ball_pos[0] != 0xff)
	{
		INT32 sx = ball_pos[0] - 4 + 1;
		INT32 sy = ((ball_pos[1] >> 4) * 12 + (ball_pos[1] & 0x0f)) - 4 + 1;

		for (INT32 y = 0; y < 4; y++)
		{
			if ((sy + y) < 0 || (sy + y) >= nScreenHeight) continue;

			for (INT32 x = 0; x < 4; x++)
			{
				if ((sx + x) < 0 || (sx + x) >= nScreenWidth) continue;

				pTransDraw[(sy + y) * nScreenWidth + sx + x] = 7;
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

	draw_layer();
	draw_ball();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		DrvInputs[0] = 0xff;
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
		}

		// Paddle stuff
		if (DrvJoy2[0]) Paddle -= 8;
		if (DrvJoy2[1]) Paddle += 8;
		if (Paddle < 0) Paddle = 0;
		if (Paddle > 0xd8) Paddle = 0xd8;
	}

	INT32 nInterleave = 262;
	INT32 nCyclesTotal = 6144000 / 60;
	INT32 nCyclesDone = 0;

	ZetOpen(0);

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		INT32 nSegment = nCyclesTotal / nInterleave;

		nCyclesDone += ZetRun(nSegment);

		if (i == 192) vblank = 1;
	}

	ZetClose();

	if (pBurnSoundOut) {
		BurnSampleRender(pBurnSoundOut, nBurnSoundLen);
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

		ZetScan(nAction);
		BurnSampleScan(nAction, pnMin);

		SCAN_VAR(ball_pos);
		SCAN_VAR(prev_audio);
		SCAN_VAR(video_color);
		SCAN_VAR(Paddle);
	}

	return 0;
}

static struct BurnSampleInfo MmagicSampleDesc[] = {
	{ "4",   SAMPLE_NOLOOP },
	{ "3",   SAMPLE_NOLOOP },
	{ "5",   SAMPLE_NOLOOP },
	{ "2",   SAMPLE_NOLOOP },
	{ "2-2", SAMPLE_NOLOOP },
	{ "6",   SAMPLE_NOLOOP },
	{ "6-2", SAMPLE_NOLOOP },
	{ "1",   SAMPLE_NOLOOP },
	{ "", 0 }
};

STD_SAMPLE_PICK(Mmagic)
STD_SAMPLE_FN(Mmagic)

// Monkey Magic

static struct BurnRomInfo mmagicRomDesc[] = {
	{ "1ai.2a",	0x0400, 0xec772e2e, 1 | BRF_PRG | BRF_ESS }, //  0 I8085A Code
	{ "2ai.3a",	0x0400, 0xe5d482ca, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3ai.4a",	0x0400, 0xe8d38deb, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4ai.45a",	0x0400, 0x3048bd6c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "5ai.5a",	0x0400, 0x2cab8f04, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "6h.6hi",	0x0200, 0xb6321b6f, 2 | BRF_GRA },           //  5 Tiles
	{ "7h.7hi",	0x0200, 0x9ec0e82c, 2 | BRF_GRA },           //  6
	{ "6j.6jk",	0x0200, 0x7ce83302, 2 | BRF_GRA },           //  7

	{ "7j.7jk",	0x0200, 0xb7eb8e1c, 3 | BRF_GRA },           //  8 Colors
};

STD_ROM_PICK(mmagic)
STD_ROM_FN(mmagic)

struct BurnDriver BurnDrvMmagic = {
	"mmagic", NULL, NULL, "mmagic", "1979",
	"Monkey Magic\0", NULL, "Nintendo", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_BREAKOUT, 0,
	NULL, mmagicRomInfo, mmagicRomName, NULL, NULL, MmagicSampleInfo, MmagicSampleName, MmagicInputInfo, MmagicDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 8,
	192, 256, 3, 4
};
