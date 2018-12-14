// FB Alpha Tutankham driver module
// Based on MAME driver by Mirko Buffoni & Rob Jarrett

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "z80_intf.h"
#include "timeplt_snd.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6809ROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvM6809RAM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvPalRAM;

static UINT32 *DrvPalette;
static UINT8  DrvRecalc;

static UINT8 scrolldata;
static UINT8 sound_mute;
static UINT8 irq_enable;
static UINT8 flipscreenx;
static UINT8 flipscreeny;
static UINT8 nRomBank;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static INT32 watchdog;

static struct BurnInputInfo TutankhmInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Left Stick Up",	BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Left Stick Down",	BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Left Stick Left",	BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Left Stick Right",	BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Right Stick Left",	BIT_DIGITAL,	DrvJoy2 + 4,	"p3 left"	},
	{"P1 Right Stick Right",BIT_DIGITAL,	DrvJoy2 + 5,	"p3 right"	},
	{"P1 Flash Bomb",	BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Left Stick Up",	BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"		},
	{"P2 Left Stick Down",	BIT_DIGITAL,	DrvJoy3 + 3,	"p2 down"	},
	{"P2 Left Stick Left",	BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"	},
	{"P2 Left Stick Right",	BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},
	{"P2 Right Stick Left",	BIT_DIGITAL,	DrvJoy3 + 4,	"p4 left"	},
	{"P2 Right Stick Right",BIT_DIGITAL,	DrvJoy3 + 5,	"p4 right"	},
	{"P2 Flash Bomb",	BIT_DIGITAL,	DrvJoy3 + 6,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Tutankhm)

static struct BurnDIPInfo TutankhmDIPList[]=
{
	{0x14, 0xff, 0xff, 0x6b, NULL			},
	{0x15, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0x03, 0x03, "3"			},
	{0x14, 0x01, 0x03, 0x01, "4"			},
	{0x14, 0x01, 0x03, 0x02, "5"			},
	{0x14, 0x01, 0x03, 0x00, "255 (Cheat)"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x14, 0x01, 0x04, 0x00, "Upright"		},
	{0x14, 0x01, 0x04, 0x04, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x14, 0x01, 0x08, 0x08, "30000"		},
	{0x14, 0x01, 0x08, 0x00, "40000"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x30, 0x30, "Easy"			},
	{0x14, 0x01, 0x30, 0x20, "Normal"		},
	{0x14, 0x01, 0x30, 0x10, "Hard"			},
	{0x14, 0x01, 0x30, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Flash Bomb"		},
	{0x14, 0x01, 0x40, 0x40, "1 per Life"		},
	{0x14, 0x01, 0x40, 0x00, "1 per Game"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,   16, "Coin A"		},
	{0x15, 0x01, 0x0f, 0x02, "4 Coins 1 Credits"	},
	{0x15, 0x01, 0x0f, 0x05, "3 Coins 1 Credits"	},
	{0x15, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x15, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x15, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x15, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x15, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x15, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x15, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x15, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x15, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x15, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x15, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x15, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x15, 0x01, 0x0f, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,   16, "Coin B"		},
	{0x15, 0x01, 0xf0, 0x20, "4 Coins 1 Credits"	},
	{0x15, 0x01, 0xf0, 0x50, "3 Coins 1 Credits"	},
	{0x15, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"	},
	{0x15, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"	},
	{0x15, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x15, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"	},
	{0x15, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"	},
	{0x15, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x15, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x15, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x15, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x15, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x15, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x15, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x15, 0x01, 0xf0, 0x00, "No Coin B"		},
};

STDDIPINFO(Tutankhm)

static void bankswitch(INT32 data)
{
	nRomBank = data;

	INT32 bank = 0x10000 + (data & 0x0f) * 0x1000;

	M6809MapMemory(DrvM6809ROM + bank, 0x9000, 0x9fff, MAP_ROM);
}

static void tutankhm_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfff0) == 0x8000) {
		DrvPalRAM[address & 0x0f] = data;
		return;
	}

	switch (address)
	{
		case 0x8100:
			scrolldata = data;
		return;

		case 0x8200:
			irq_enable = data & 1;
			if (!irq_enable) {
				M6809SetIRQLine(0, CPU_IRQSTATUS_NONE);
			}
		return;

		case 0x8202:
			// coin counter
		return;

		case 0x8204:
		return;

		case 0x8205:
			sound_mute = data & 1;
		return;

		case 0x8206:
			flipscreenx = data & 1;
		return;

		case 0x8207:
			flipscreeny = data & 1;
		return;

		case 0x8300:
			bankswitch(data);
		return;

		case 0x8600:
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;

		case 0x8700:
			TimepltSndSoundlatch(data);
		return;
	}
}

static UINT8 tutankhm_read(UINT16 address)
{
	switch (address)
	{
		case 0x8120:
			watchdog = 0;
			return 0;

		case 0x8160:
			return DrvDips[1];

		case 0x8180:
			return DrvInputs[0];

		case 0x81a0:
			return DrvInputs[1];

		case 0x81c0:
			return DrvInputs[2];

		case 0x81e0:
			return DrvDips[0];

		case 0x8200:
			return 0;
	}

	return 0;
}

static INT32 DrvDoReset(INT32 clear_ram)
{
	if (clear_ram) {
		memset(AllRam, 0, RamEnd - AllRam);
	}

	M6809Open(0);
	M6809Reset();
	bankswitch(0);
	M6809Close();

	TimepltSndReset();

	irq_enable = 0;
	sound_mute = 0;
	watchdog = 0;
	scrolldata = 0;
	flipscreenx = 0;
	flipscreeny = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6809ROM		= Next; Next += 0x020000;
	DrvZ80ROM		= Next; Next += 0x003000;

	DrvPalette		= (UINT32*)Next; Next += 0x0010 * sizeof(UINT32);

	AllRam			= Next;

	DrvVidRAM		= Next; Next += 0x008000;
	DrvM6809RAM		= Next; Next += 0x000800;
	DrvZ80RAM		= Next; Next += 0x000400;
	DrvPalRAM		= Next; Next += 0x000010;

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
		if (BurnLoadRom(DrvM6809ROM + 0x0a000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x0b000,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x0c000,  2, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x0d000,  3, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x0e000,  4, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x0f000,  5, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x10000,  6, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x11000,  7, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x12000,  8, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x13000,  9, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x14000, 10, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x15000, 11, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x16000, 12, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x17000, 13, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x18000, 14, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM   + 0x00000, 15, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM   + 0x01000, 16, 1)) return 1;
	}

	M6809Init(0);
	M6809Open(0);
	M6809MapMemory(DrvVidRAM,		0x0000, 0x7fff, MAP_RAM);
	M6809MapMemory(DrvM6809RAM,		0x8800, 0x8fff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM + 0xa000,	0xa000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(tutankhm_write);
	M6809SetReadHandler(tutankhm_read);
	M6809Close();

	TimepltSndInit(DrvZ80ROM, DrvZ80RAM, 0);
	TimepltSndSrcGain(0.55); // quench distortion when enemy spawns

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6809Exit();

	TimepltSndExit();

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteUpdate()
{
	for (INT32 i = 0; i < 0x10; i++) {
		INT32 r = (DrvPalRAM[i] >> 0) & 7;
		INT32 g = (DrvPalRAM[i] >> 3) & 7;
		INT32 b = (DrvPalRAM[i] >> 6) & 3;

		r  = (r << 5) | (r << 2) | (r >> 1);
		g  = (g << 5) | (g << 2) | (g >> 1);
		b |= (b << 6) | (b << 4) | (b << 2);

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_layer()
{
	INT32 flipx = (flipscreenx) ? 255 : 0;
	INT32 flipy = (flipscreeny) ? 255 : 0;

	for (INT32 y = 16; y < 256 - 16; y++)
	{
		UINT16 *dst = pTransDraw + (y - 16) * nScreenWidth;

		for (INT32 x = 0; x < nScreenWidth; x++)
		{
			UINT8 effx = x ^ flipx;
			UINT8 yscroll = (effx < 192) ? scrolldata : 0;
			UINT8 effy = (y ^ flipy) + yscroll;
			dst[x] = (DrvVidRAM[effy * 128 + effx / 2] >> (4 * (effx & 1))) & 0xf;
		}
	}
}

static INT32 DrvDraw()
{
//	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 0;
//	}

	draw_layer();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	watchdog++;
	if (watchdog >= 180) {
		DrvDoReset(0);
	}

	if (DrvReset) {
		DrvDoReset(1);
	}

	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 3);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		// Clear opposites
		if ((DrvInputs[1] & 0x0c) == 0) DrvInputs[1] |= 0x0c;
		if ((DrvInputs[1] & 0x03) == 0) DrvInputs[1] |= 0x03;
		if ((DrvInputs[1] & 0x30) == 0) DrvInputs[1] |= 0x30;
		if ((DrvInputs[2] & 0x0c) == 0) DrvInputs[2] |= 0x0c;
		if ((DrvInputs[2] & 0x03) == 0) DrvInputs[2] |= 0x03;
		if ((DrvInputs[2] & 0x30) == 0) DrvInputs[2] |= 0x30;
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 1536000 / 60, 1789772 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };
	INT32 nSoundBufferPos = 0;

	M6809Open(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		INT32 nSegment = (nCyclesTotal[0] * (i + 1)) / nInterleave;
		nCyclesDone[0] += M6809Run(nSegment - nCyclesDone[0]);
		if (i == (nInterleave - 1) && irq_enable && (nCurrentFrame & 1)) M6809SetIRQLine(0, CPU_IRQSTATUS_ACK);

		nSegment = (nCyclesTotal[1] * i) / nInterleave;
		nCyclesDone[1] += ZetRun(nSegment - nCyclesDone[1]);

		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			if (!sound_mute) TimepltSndUpdate(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	ZetClose();
	M6809Close();

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);

		if (sound_mute) {
			BurnSoundClear();
		} else {
			TimepltSndUpdate(pSoundBuf, nSegmentLength);
		}
	}

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction,INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029521;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		M6809Scan(nAction);
		TimepltSndScan(nAction, pnMin);

		SCAN_VAR(scrolldata);
		SCAN_VAR(sound_mute);
		SCAN_VAR(irq_enable);
		SCAN_VAR(flipscreenx);
		SCAN_VAR(flipscreeny);
		SCAN_VAR(nRomBank);
	}

	if (nAction & ACB_WRITE) {
		M6809Open(0);
		bankswitch(nRomBank);
		M6809Close();
	}

	return 0;
}


// Tutankham

static struct BurnRomInfo tutankhmRomDesc[] = {
	{ "m1.1h",	0x1000, 0xda18679f, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "m2.2h",	0x1000, 0xa0f02c85, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3j.3h",	0x1000, 0xea03a1ab, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "m4.4h",	0x1000, 0xbd06fad0, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "m5.5h",	0x1000, 0xbf9fd9b0, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "j6.6h",	0x1000, 0xfe079c5b, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "c1.1i",	0x1000, 0x7eb59b21, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "c2.2i",	0x1000, 0x6615eff3, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "c3.3i",	0x1000, 0xa10d4444, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "c4.4i",	0x1000, 0x58cd143c, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "c5.5i",	0x1000, 0xd7e7ae95, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "c6.6i",	0x1000, 0x91f62b82, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "c7.7i",	0x1000, 0xafd0a81f, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "c8.8i",	0x1000, 0xdabb609b, 1 | BRF_PRG | BRF_ESS }, // 13
	{ "c9.9i",	0x1000, 0x8ea9c6a6, 1 | BRF_PRG | BRF_ESS }, // 14

	{ "s1.7a",	0x1000, 0xb52d01fa, 2 | BRF_PRG | BRF_ESS }, // 15 Z80 Code
	{ "s2.8a",	0x1000, 0x9db5c0ce, 2 | BRF_PRG | BRF_ESS }, // 16
};

STD_ROM_PICK(tutankhm)
STD_ROM_FN(tutankhm)

struct BurnDriver BurnDrvTutankhm = {
	"tutankhm", NULL, NULL, NULL, "1982",
	"Tutankham\0", NULL, "Konami", "GX350",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 4, HARDWARE_PREFIX_KONAMI, GBF_MAZE, 0,
	NULL, tutankhmRomInfo, tutankhmRomName, NULL, NULL, NULL, NULL, TutankhmInputInfo, TutankhmDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	224, 256, 3, 4
};


// Tutankham (Stern Electronics)

static struct BurnRomInfo tutankhmsRomDesc[] = {
	{ "m1.1h",	0x1000, 0xda18679f, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "m2.2h",	0x1000, 0xa0f02c85, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3a.3h",	0x1000, 0x2d62d7b1, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "m4.4h",	0x1000, 0xbd06fad0, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "m5.5h",	0x1000, 0xbf9fd9b0, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "a6.6h",	0x1000, 0xc43b3865, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "c1.1i",	0x1000, 0x7eb59b21, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "c2.2i",	0x1000, 0x6615eff3, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "c3.3i",	0x1000, 0xa10d4444, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "c4.4i",	0x1000, 0x58cd143c, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "c5.5i",	0x1000, 0xd7e7ae95, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "c6.6i",	0x1000, 0x91f62b82, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "c7.7i",	0x1000, 0xafd0a81f, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "c8.8i",	0x1000, 0xdabb609b, 1 | BRF_PRG | BRF_ESS }, // 13
	{ "c9.9i",	0x1000, 0x8ea9c6a6, 1 | BRF_PRG | BRF_ESS }, // 14

	{ "s1.7a",	0x1000, 0xb52d01fa, 2 | BRF_PRG | BRF_ESS }, // 15 Z80 Code
	{ "s2.8a",	0x1000, 0x9db5c0ce, 2 | BRF_PRG | BRF_ESS }, // 16
};

STD_ROM_PICK(tutankhms)
STD_ROM_FN(tutankhms)

struct BurnDriver BurnDrvTutankhms = {
	"tutankhms", "tutankhm", NULL, NULL, "1982",
	"Tutankham (Stern Electronics)\0", NULL, "Konami (Stern Electronics license)", "GX350",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 4, HARDWARE_PREFIX_KONAMI, GBF_MAZE, 0,
	NULL, tutankhmsRomInfo, tutankhmsRomName, NULL, NULL, NULL, NULL, TutankhmInputInfo, TutankhmDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	224, 256, 3, 4
};
