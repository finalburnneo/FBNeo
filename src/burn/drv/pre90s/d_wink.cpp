// FinalBurn Neo Wink driver module
// Based on MAME driver by Nicola Salmoria, Pierpaolo Prazzoli

#include "tiles_generic.h"
#include "z80_intf.h"
#include "ay8910.h"
#include "bitswap.h"
#include "burn_gun.h" // for dial

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvZ80ROM[2];
static UINT8 *DrvGfxROM;
static UINT8 *DrvNVRAM;
static UINT8 *DrvZ80RAM[2];
static UINT8 *DrvVidRAM;
static UINT8 *DrvPalRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 tile_bank;
static INT32 soundlatch;
static INT32 sound_flag;
static INT32 nmi_enabled;

static UINT8 DrvJoy1[8];
static UINT8 DrvDips[4];
static UINT8 DrvInputs[1];
static UINT8 DrvReset;
static INT16 Analog[1];

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo WinkInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 3"	},

	A("P1 Dial", 		BIT_ANALOG_REL, &Analog[0],		"p1 x-axis"),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",			BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};

STDINPUTINFO(Wink)

static struct BurnDIPInfo WinkDIPList[]=
{
	DIP_OFFSET(0x07)
	{0x00, 0xff, 0xff, 0xfe, NULL						},
	{0x01, 0xff, 0xff, 0xd7, NULL						},
	{0x02, 0xff, 0xff, 0xff, NULL						},
	{0x03, 0xff, 0xff, 0xf7, NULL						},

	{0   , 0xfe, 0   ,    4, "Bonus Life"				},
	{0x00, 0x01, 0x11, 0x10, "60k/120k/240k/480k"		},
	{0x00, 0x01, 0x11, 0x01, "80k/160k/320k/640k"		},
	{0x00, 0x01, 0x11, 0x00, "100k/200k/400k/800k"		},
	{0x00, 0x01, 0x11, 0x11, "None"						},

	{0   , 0xfe, 0   ,    8, "Coin B"					},
	{0x00, 0x01, 0x26, 0x26, "1 Coin  1 Credits"		},
	{0x00, 0x01, 0x26, 0x24, "1 Coin  2 Credits"		},
	{0x00, 0x01, 0x26, 0x06, "1 Coin  3 Credits"		},
	{0x00, 0x01, 0x26, 0x04, "1 Coin  5 Credits"		},
	{0x00, 0x01, 0x26, 0x22, "2 Coins 1 Credits"		},
	{0x00, 0x01, 0x26, 0x20, "2 Coins 2 Credits"		},
	{0x00, 0x01, 0x26, 0x02, "2 Coins 3 Credits"		},
	{0x00, 0x01, 0x26, 0x00, "2 Coins 5 Credits"		},

	{0   , 0xfe, 0   ,    8, "Coin A"					},
	{0x00, 0x01, 0xc8, 0xc8, "1 Coin  1 Credits"		},
	{0x00, 0x01, 0xc8, 0x88, "1 Coin  2 Credits"		},
	{0x00, 0x01, 0xc8, 0xc0, "1 Coin  3 Credits"		},
	{0x00, 0x01, 0xc8, 0x80, "1 Coin  5 Credits"		},
	{0x00, 0x01, 0xc8, 0x48, "2 Coins 1 Credits"		},
	{0x00, 0x01, 0xc8, 0x08, "2 Coins 2 Credits"		},
	{0x00, 0x01, 0xc8, 0x40, "2 Coins 3 Credits"		},
	{0x00, 0x01, 0xc8, 0x00, "2 Coins 5 Credits"		},

	{0   , 0xfe, 0   ,    2, "Ball Save Barrier"		},
	{0x01, 0x01, 0x11, 0x00, "Off"						},
	{0x01, 0x01, 0x11, 0x11, "On"						},

	{0   , 0xfe, 0   ,    2, "Unknown"					},
	{0x01, 0x01, 0x02, 0x02, "Off"						},
	{0x01, 0x01, 0x02, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "1 Credit Award"			},
	{0x01, 0x01, 0x04, 0x04, "Yes"						},
	{0x01, 0x01, 0x04, 0x00, "No"						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x01, 0x01, 0x88, 0x88, "2"						},
	{0x01, 0x01, 0x88, 0x80, "3"						},
	{0x01, 0x01, 0x88, 0x08, "5"						},
	{0x01, 0x01, 0x88, 0x00, "7"						},

	{0   , 0xfe, 0   ,    4, "Timer Speed"				},
	{0x01, 0x01, 0x60, 0x00, "Slow"						},
	{0x01, 0x01, 0x60, 0x40, "Normal"					},
	{0x01, 0x01, 0x60, 0x60, "Fast"						},
	{0x01, 0x01, 0x60, 0x20, "Very Fast"				},

	{0   , 0xfe, 0   ,    2, "Summary"					},
	{0x03, 0x01, 0x01, 0x01, "Off"						},
	{0x03, 0x01, 0x01, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Unknown"					},
	{0x03, 0x01, 0x02, 0x02, "Off"						},
	{0x03, 0x01, 0x02, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Credit Payout"			},
	{0x03, 0x01, 0x44, 0x44, "2.5%"						},
	{0x03, 0x01, 0x44, 0x40, "5%"						},
	{0x03, 0x01, 0x44, 0x04, "10%"						},
	{0x03, 0x01, 0x44, 0x00, "20%"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x03, 0x01, 0x08, 0x08, "Off"						},
	{0x03, 0x01, 0x08, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Unknown"					},
	{0x03, 0x01, 0x20, 0x20, "Off"						},
	{0x03, 0x01, 0x20, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Reset Summary Stats"		},
	{0x03, 0x01, 0x80, 0x80, "Off"						},
	{0x03, 0x01, 0x80, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x03, 0x01, 0x10, 0x10, "Off"						},
	{0x03, 0x01, 0x10, 0x00, "On"						},
};

STDDIPINFO(Wink)

static void __fastcall wink_main_write_port(UINT16 port, UINT8 data)
{
	port &= 0xff;
	if ((port & 0xff) < 0x20) { // 0 - 1f
		DrvPalRAM[port] = data;
		return;
	}

	if ((port & 0xe0) == 0xc0) {
		// protection
		return;
	}

	switch (port & 0xff)
	{
		case 0x20:
			nmi_enabled = data & 1;
		return;

		case 0x21:	// mux
		return;

		case 0x22:
			tile_bank = data & 1;
		return;

		case 0x25:
		case 0x26:
		case 0x27:
			// coin_counter
		return;

		case 0x40:
			soundlatch = data;
		return;

		case 0x60:
			ZetSetIRQLine(1, 0, CPU_IRQSTATUS_HOLD);
		return;
	}
}

static UINT8 __fastcall wink_main_read_port(UINT16 port)
{
	if ((port & 0xe0) == 0xe0) {
		return 0x20; // protection?
	}

	switch (port & 0xff)
	{
		case 0x80:
			return BurnTrackballRead(0);

		case 0xa0:
			return DrvInputs[0];

		case 0xa4:
			return DrvDips[0];

		case 0xa8:
			return DrvDips[1];

		case 0xb0:
			return DrvDips[2];

		case 0xb4:
			return DrvDips[3];
	}

	return 0;
}

static UINT8 __fastcall wink_sound_read(UINT16 address)
{
	if (address == 0x8000) {
		return soundlatch;
	}

	return 0;
}

static void __fastcall wink_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x80:
			AY8910Write(0, (~port & 0x80) >> 7, data);
		return;
	}
}

static UINT8 __fastcall wink_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
			return AY8910Read(0);
	}

	return 0;
}

static tilemap_callback( bg )
{
	INT32 code = DrvVidRAM[offs] | (tile_bank * 0x200);
	if (offs < 0x360) code |= 0x100;

	TILE_SET_INFO(0, code, 0, 0);
}

static UINT8 ay8910_read_A(UINT32)
{
	return sound_flag;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	ZetClose();

	AY8910Reset(0);

	soundlatch = 0;
	sound_flag = 0;
	tile_bank = 0;
	nmi_enabled = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM[0]	= Next; Next += 0x008000;
	DrvZ80ROM[1]	= Next; Next += 0x002000;

	DrvGfxROM		= Next; Next += 0x010000;

	DrvPalette		= (UINT32*)Next; Next += 0x0010 * sizeof(UINT32);

	DrvNVRAM		= Next; Next += 0x000800;

	AllRam			= Next;

	DrvZ80RAM[0]	= Next; Next += 0x000800;
	DrvZ80RAM[1]	= Next; Next += 0x000400;
	DrvVidRAM		= Next; Next += 0x000400;
	DrvPalRAM		= Next; Next += 0x000020;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[3] = { 0, 0x2000*8*1, 0x2000*8*2 };
	INT32 XOffs[8] = { STEP8(0,1) };
	INT32 YOffs[8] = { STEP8(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x6000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM, 0x6000);

	GfxDecode(0x0400, 3, 8, 8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM);

	BurnFree(tmp);

	return 0;
}

static void DrvZ80Decode()
{
	UINT8 *tmp = (UINT8*)BurnMalloc(0x8000);

	memcpy (tmp, DrvZ80ROM[0], 0x8000);

	for (INT32 i = 0x0000; i < 0x2000; i++)
	{
		DrvZ80ROM[0][i + 0x0000] = tmp[BITSWAP16(i + 0x0000,15,14,13,11,12, 7,9,8,10,6,4,5,1,2,3,0)];
		DrvZ80ROM[0][i + 0x2000] = tmp[BITSWAP16(i + 0x2000,15,14,13,10, 7,12,9,8,11,6,3,1,5,2,4,0)];
		DrvZ80ROM[0][i + 0x4000] = tmp[BITSWAP16(i + 0x4000,15,14,13, 7,10,11,9,8,12,6,1,3,4,2,5,0)];
		DrvZ80ROM[0][i + 0x6000] = tmp[BITSWAP16(i + 0x6000,15,14,13,11,12, 7,9,8,10,6,4,5,1,2,3,0)];
	}

	for (INT32 i = 0; i < 0x8000; i++)
	{
		DrvZ80ROM[0][i] += BITSWAP08((i&0xff),7,5,3,1,6,4,2,0);
	}

	BurnFree(tmp);
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	memset (DrvNVRAM, 0xff, 0x800);

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x04000, k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM[1] + 0x00000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM    + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM    + 0x02000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM    + 0x04000, k++, 1)) return 1;

		DrvZ80Decode();
		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM[0],		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM[0],		0x8000, 0x87ff, MAP_RAM);
	ZetMapMemory(DrvNVRAM,			0x9000, 0x97ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,			0xa000, 0xa3ff, MAP_RAM);
	ZetSetOutHandler(wink_main_write_port);
	ZetSetInHandler(wink_main_read_port);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM[1],		0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM[1],		0x4000, 0x43ff, MAP_RAM);
	ZetSetReadHandler(wink_sound_read);
	ZetSetOutHandler(wink_sound_write_port);
	ZetSetInHandler(wink_sound_read_port);
	ZetClose();

	AY8910Init(0, 1500000, 0);
	AY8910SetPorts(0, &ay8910_read_A, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 1.00, BURN_SND_ROUTE_BOTH);
	AY8910SetBuffered(ZetTotalCycles, 3000000);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM, 3, 8, 8, 0x10000, 0, 0);
	GenericTilemapSetOffsets(0, 0, -16);

	BurnTrackballInit(1); // dial
	BurnTrackballConfigStartStopPoints(0, 0x4f, 0xcf, 0, 0);
	BurnTrackballSetResetDefault(0x4f + (0xcf - 0x4f) /2); // center it
	BurnTrackballReadReset();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	ZetExit();
	AY8910Exit(0);
    BurnTrackballExit();

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteUpdate()
{
	for (INT32 i = 0; i < 32; i+=2)
	{
		UINT8 r = (DrvPalRAM[i + 0] & 0xf0) >> 4;
		UINT8 g = DrvPalRAM[i + 0] & 0x0f;
		UINT8 b = DrvPalRAM[i + 1] & 0x0f;

		DrvPalette[i/2] = BurnHighCol(r+(r*16), g+g*16, b+b*16, 0);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc && 1) {
		DrvPaletteUpdate();
		DrvRecalc = 1; // force
	}

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
		DrvInputs[0] = 0xff;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
		}
		BurnTrackballConfig(0, AXIS_REVERSED, AXIS_REVERSED);
		BurnTrackballFrame(0, Analog[0], 0, 1, 0xf);
		BurnTrackballUpdate(0);
	}

	INT32 nInterleave = 260;
	INT32 nCyclesTotal[2] = { 3000000 / 60, 3000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		CPU_RUN(0, Zet);
		if (i == (nInterleave - 1) && nmi_enabled) ZetNmi();
		ZetClose();

		ZetOpen(1);
		CPU_RUN(1, Zet);
		sound_flag ^= 0x80; // 260 * 60 = 15600hz
		ZetClose();
	}

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	if (pnMin) {
		*pnMin = 0x029702;
	}

	if (nAction & ACB_VOLATILE) {
		ScanVar(AllRam, RamEnd - AllRam, "All Ram");

		ZetScan(nAction);
		AY8910Scan(nAction, pnMin);
		BurnTrackballScan();

		SCAN_VAR(soundlatch);
		SCAN_VAR(sound_flag);
		SCAN_VAR(tile_bank);
	}

	if (nAction & ACB_NVRAM) {
		ScanVar(DrvNVRAM, 0x800, "NV RAM");
	}

	return 0;
}


// Wink (set 1)

static struct BurnRomInfo winkRomDesc[] = {
	{ "midcoin-wink00.rom",		0x4000, 0x044f82d6, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "midcoin-wink01.rom",		0x4000, 0xacb0a392, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "midcoin-wink05.rom",		0x2000, 0xc6c9d9cf, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "midcoin-wink02.rom",		0x2000, 0xd1cd9d06, 3 | BRF_GRA },           //  3 Graphics
	{ "midcoin-wink03.rom",		0x2000, 0x2346f50c, 3 | BRF_GRA },           //  4
	{ "midcoin-wink04.rom",		0x2000, 0x06dd229b, 3 | BRF_GRA },           //  5
};

STD_ROM_PICK(wink)
STD_ROM_FN(wink)

struct BurnDriver BurnDrvWink = {
	"wink", NULL, NULL, NULL, "1985",
	"Wink (set 1)\0", NULL, "Midcoin", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_BALLPADDLE, 0,
	NULL, winkRomInfo, winkRomName, NULL, NULL, NULL, NULL, WinkInputInfo, WinkDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	256, 256, 4, 3
};


// Wink (set 2)

static struct BurnRomInfo winkaRomDesc[] = {
	{ "wink0.bin",				0x4000, 0x554d86e5, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "wink1.bin",				0x4000, 0x9d8ad539, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "wink5.bin",				0x2000, 0xc6c9d9cf, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "wink2.bin",				0x2000, 0xd1cd9d06, 3 | BRF_GRA },           //  3 Graphics
	{ "wink3.bin",				0x2000, 0x2346f50c, 3 | BRF_GRA },           //  4
	{ "wink4.bin",				0x2000, 0x06dd229b, 3 | BRF_GRA },           //  5
};

STD_ROM_PICK(winka)
STD_ROM_FN(winka)

struct BurnDriver BurnDrvWinka = {
	"winka", "wink", NULL, NULL, "1985",
	"Wink (set 2)\0", NULL, "Midcoin", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_BALLPADDLE, 0,
	NULL, winkaRomInfo, winkaRomName, NULL, NULL, NULL, NULL, WinkInputInfo, WinkDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	256, 256, 4, 3
};
