// FB Alpha One + Two driver module
// Based on MAME driver by David Haywood

#include "tiles_generic.h"
#include "z80_intf.h"
#include "burn_ym3812.h"
#include "msm6295.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM;
static UINT8 *DrvSndROM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 soundlatch;
static UINT8 nDrvBank;
static INT32 watchdog;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo OnetwoInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 4,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Onetwo)

static struct BurnDIPInfo OnetwoDIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL			},
	{0x15, 0xff, 0xff, 0x79, NULL			},

	{0   , 0xfe, 0   ,    4, "Timer"		},
	{0x14, 0x01, 0x03, 0x03, "Easy"			},
	{0x14, 0x01, 0x03, 0x02, "Normal"		},
	{0x14, 0x01, 0x03, 0x01, "Hard"			},
	{0x14, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x0c, 0x0c, "Easy"			},
	{0x14, 0x01, 0x0c, 0x08, "Normal"		},
	{0x14, 0x01, 0x0c, 0x04, "Hard"			},
	{0x14, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    16, "Coinage"		},
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
	{0x14, 0x01, 0xf0, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x14, 0x01, 0x30, 0x00, "5 Coins 1 Credits"	},
	{0x14, 0x01, 0x30, 0x10, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0x30, 0x20, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x14, 0x01, 0xc0, 0xc0, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0xc0, 0x80, "1 Coin  3 Credits"	},
	{0x14, 0x01, 0xc0, 0x40, "1 Coin  5 Credits"	},
	{0x14, 0x01, 0xc0, 0x00, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x15, 0x01, 0x02, 0x02, "Off"			},
	{0x15, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Coin Chute"		},
	{0x15, 0x01, 0x04, 0x04, "Common"		},
	{0x15, 0x01, 0x04, 0x00, "Separate"		},

	{0   , 0xfe, 0   ,    2, "Nude Pictures"	},
	{0x15, 0x01, 0x08, 0x00, "Off"			},
	{0x15, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    2, "Women Select"		},
	{0x15, 0x01, 0x10, 0x00, "No"			},
	{0x15, 0x01, 0x10, 0x10, "Yes"			},

	{0   , 0xfe, 0   ,    2, "Stop Mode (Cheat)"	},
	{0x15, 0x01, 0x20, 0x20, "Off"			},
	{0x15, 0x01, 0x20, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Play Mode"		},
	{0x15, 0x01, 0x40, 0x00, "1 Player"		},
	{0x15, 0x01, 0x40, 0x40, "2 Player"		},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x15, 0x01, 0x80, 0x00, "Off"			},
	{0x15, 0x01, 0x80, 0x80, "On"			},
};

STDDIPINFO(Onetwo)

static void bankswitch(INT32 data)
{
	nDrvBank = data & 7;

	ZetMapMemory(DrvZ80ROM0 + nDrvBank * 0x4000, 0x8000, 0xbfff, MAP_ROM);
}

static void __fastcall onetwo_main_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			watchdog = 0;
			// coin counters = data & 3
		return;

		case 0x01:
			soundlatch = data;
			ZetClose();
			ZetOpen(1);
			ZetNmi();
			ZetClose();
			ZetOpen(0);
		return;

		case 0x02:
			bankswitch(data);
		return;
	}
}

static UINT8 __fastcall onetwo_main_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
			return DrvDips[port & 1];

		case 0x02:
		case 0x03:
		case 0x04:
			return DrvInputs[(port - 2) & 0xff];
	}

	return 0;
}

static UINT8 __fastcall onetwo_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xf800:
			return soundlatch;
	}

	return 0;
}

static void __fastcall onetwo_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			BurnYM3812Write(0, 0, data);
		return;

		case 0x20:
			BurnYM3812Write(0, 1, data);
		return;

		case 0x40:
			MSM6295Command(0, data);
		return;

		case 0xc0:
			soundlatch = 0;
		return;
	}
}

static UINT8 __fastcall onetwo_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
			return BurnYM3812Read(0,0);

		case 0x40:
			return MSM6295ReadStatus(0);
	}

	return 0;
}

static void DrvFMIRQHandler(int, INT32 nStatus)
{
	ZetSetIRQLine(0, ((nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE));
}

static INT32 DrvSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)ZetTotalCycles() * nSoundRate / 4000000;
}

static INT32 DrvDoReset(INT32 full_reset)
{
	if (full_reset) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	ZetOpen(0);
	ZetReset();
	bankswitch(0);
	ZetClose();

	ZetOpen(1);
	ZetReset();
	BurnYM3812Reset();
	MSM6295Reset(0);
	ZetClose();

	soundlatch = 0;
	watchdog = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0	= Next; Next += 0x020000;
	DrvZ80ROM1	= Next; Next += 0x010000;

	DrvGfxROM	= Next; Next += 0x200000;

	MSM6295ROM	= Next;
	DrvSndROM	= Next; Next += 0x040000;

	DrvPalette	= (UINT32*)Next; Next += 0x0080 * sizeof(UINT32);

	AllRam		= Next;

	DrvPalRAM	= Next; Next += 0x000200;
	DrvFgRAM	= Next; Next += 0x001000;
	DrvZ80RAM0	= Next; Next += 0x002000;
	DrvZ80RAM1	= Next; Next += 0x000400;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	UINT8 *tmp = (UINT8*)malloc(0x180000);
	if (tmp == NULL) {
		return 1;
	}

	static INT32 Planes[6] = { 0x800000, 0x800000+4, 0, 0+4, 0x400000, 0x400000+4 };
	static INT32 XOffs[8]  = { STEP4(0,1), STEP4(64,1) };
	static INT32 YOffs[8]  = { STEP8(0,8) };

	memcpy (tmp, DrvGfxROM, 0x180000);

	GfxDecode(0x8000, 6, 8, 8, Planes, XOffs, YOffs, 0x080, tmp, DrvGfxROM);

	free (tmp);

	return 0;
}

static INT32 DrvInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)malloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x000000,  0, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x000000,  1, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM  + 0x000000,  2, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM  + 0x080000,  3, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM  + 0x100000,  4, 1)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x000000,  5, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvPalRAM,		0xc800, 0xc8ff, MAP_RAM); // 0-7f
	ZetMapMemory(DrvPalRAM + 0x100,	0xc900, 0xc9ff, MAP_RAM); // 0-7f
	ZetMapMemory(DrvFgRAM,		0xd000, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM0,	0xe000, 0xffff, MAP_RAM);
	ZetSetOutHandler(onetwo_main_write_port);
	ZetSetInHandler(onetwo_main_read_port);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0x5fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,	0xf000, 0xf7ff, MAP_RAM);
	ZetSetReadHandler(onetwo_sound_read);
	ZetSetOutHandler(onetwo_sound_write_port);
	ZetSetInHandler(onetwo_sound_read_port);
	ZetClose();

	BurnYM3812Init(1, 4000000, &DrvFMIRQHandler, &DrvSynchroniseStream, 0);
	BurnTimerAttachZetYM3812(4000000);
	BurnYM3812SetRoute(0, BURN_SND_YM3812_ROUTE, 1.00, BURN_SND_ROUTE_BOTH);
	BurnYM3812SetRoute(1, BURN_SND_YM3812_ROUTE, 1.00, BURN_SND_ROUTE_BOTH);

	MSM6295Init(0, 1056000 * 2 / 132, 1);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	MSM6295Exit(0);
	BurnYM3812Exit();

	ZetExit();

	free (AllMem);
	AllMem = NULL;

	return 0;
}

static void DrvPaletteUpdate()
{
	for (INT32 i = 0; i < 0x80; i++)
	{
		INT32 r = DrvPalRAM[i + 0x000] & 0x1f;
		INT32 g = DrvPalRAM[i + 0x100] & 0x1f;
		INT32 b =((DrvPalRAM[i + 0x000] >> 2) & 0x18) | ((DrvPalRAM[i + 0x100] >> 5) & 0x7);

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_layer()
{
	for (INT32 offs = 0; offs < 64 * 32; offs++)
	{
		INT32 attr  = DrvFgRAM[offs * 2 + 1];
		INT32 code  = DrvFgRAM[offs * 2 + 0] + ((attr & 0x7f) << 8);
		INT32 color = attr >> 7;

		Render8x8Tile(pTransDraw, code, (offs & 0x3f) * 8, (offs / 0x40) * 8, color, 6, 0, DrvGfxROM);
	}
}

static INT32 DrvDraw()
{
//	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1; // always
//	}

	for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
		pTransDraw[i] = 0x0100;
	}

	draw_layer();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	watchdog++;
	if (watchdog > 180) {
		DrvDoReset(0);
	}

	if (DrvReset) {
		DrvDoReset(1);
	}

	{
		memset (DrvInputs, 0xff, 3);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	ZetNewFrame();

	INT32 nInterleave = 16;
	INT32 nTotalCycles[2] = { 4000000 / 60, 4000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun((nTotalCycles[0] - nCyclesDone[0]) / (nInterleave - i));
		if (i == (nInterleave-1)) ZetSetIRQLine(0, CPU_IRQSTATUS_AUTO);
		ZetClose();

		ZetOpen(1);
		BurnTimerUpdateYM3812((i + 1) * (nTotalCycles[1] / nInterleave));
		ZetClose();
	}

	ZetOpen(1);

	BurnTimerEndFrameYM3812(nTotalCycles[1]);

	if (pBurnSoundOut) {
		BurnYM3812Update(pBurnSoundOut, nBurnSoundLen);
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();

	if (pBurnDraw) {
		DrvDraw();
	}

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
		ZetScan(nAction);

		ZetOpen(1);
		BurnYM3812Scan(nAction, pnMin);
		MSM6295Scan(0, nAction);
		ZetClose();

		SCAN_VAR(soundlatch);
		SCAN_VAR(nDrvBank);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(1);
		bankswitch(nDrvBank);
		ZetClose();
	}

	return 0;
}


// One + Two

static struct BurnRomInfo onetwoRomDesc[] = {
	{ "maincpu",	0x20000, 0x83431e6e, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	{ "sound_prog",	0x10000, 0x90aba4f3, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code

	{ "3_graphics",	0x80000, 0xc72ff3a0, 3 | BRF_GRA },           //  2 Characters
	{ "4_graphics",	0x80000, 0x0ca40557, 3 | BRF_GRA },           //  3
	{ "5_graphics",	0x80000, 0x664b6679, 3 | BRF_GRA },           //  4

	{ "sample",	0x40000, 0xb10d3132, 4 | BRF_SND },           //  5 Samples
};

STD_ROM_PICK(onetwo)
STD_ROM_FN(onetwo)

struct BurnDriver BurnDrvOnetwo = {
	"onetwo", NULL, NULL, NULL, "1997",
	"One + Two\0", NULL, "Barko", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, onetwoRomInfo, onetwoRomName, NULL, NULL, OnetwoInputInfo, OnetwoDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	512, 256, 4, 3
};


// One + Two (earlier)

static struct BurnRomInfo onetwoeRomDesc[] = {
	{ "main_prog",	0x20000, 0x6c1936e9, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	{ "sound_prog",	0x10000, 0x90aba4f3, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code

	{ "3_grfx",	0x80000, 0x0f9f39ff, 3 | BRF_GRA },           //  2 Characters
	{ "4_grfx",	0x80000, 0x2b0e0564, 3 | BRF_GRA },           //  3
	{ "5_grfx",	0x80000, 0x69807a9b, 3 | BRF_GRA },           //  4

	{ "sample",	0x40000, 0xb10d3132, 4 | BRF_SND },           //  5 Samples
};

STD_ROM_PICK(onetwoe)
STD_ROM_FN(onetwoe)

struct BurnDriver BurnDrvOnetwoe = {
	"onetwoe", "onetwo", NULL, NULL, "1997",
	"One + Two (earlier)\0", NULL, "Barko", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, onetwoeRomInfo, onetwoeRomName, NULL, NULL, OnetwoInputInfo, OnetwoDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	512, 256, 4, 3
};

