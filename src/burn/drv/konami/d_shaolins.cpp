// FB Alpha Shaolin's Road driver module
// Based on MAME driver by Allard Van Der Bas

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "sn76496.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvM6809ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvM6809RAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvColRAM;

static UINT8 *flipscreen;
static UINT8 *nmi_enable;
static UINT8 *palette_bank;
static UINT16 *scroll;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvInputs[3];
static UINT8 DrvDips[3];
static UINT8 DrvReset;

static INT32 watchdog;

static struct BurnInputInfo ShaolinsInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Shaolins)

static struct BurnDIPInfo ShaolinsDIPList[]=
{
	{0x12, 0xff, 0xff, 0x5a, NULL						},
	{0x13, 0xff, 0xff, 0xff, NULL						},
	{0x14, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x12, 0x01, 0x03, 0x03, "2"						},
	{0x12, 0x01, 0x03, 0x02, "3"						},
	{0x12, 0x01, 0x03, 0x01, "5"						},
	{0x12, 0x01, 0x03, 0x00, "7"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x12, 0x01, 0x04, 0x00, "Upright"					},
	{0x12, 0x01, 0x04, 0x04, "Cocktail"					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"				},
	{0x12, 0x01, 0x18, 0x18, "30000 and every 70000"	},
	{0x12, 0x01, 0x18, 0x10, "40000 and every 80000"	},
	{0x12, 0x01, 0x18, 0x08, "40000"					},
	{0x12, 0x01, 0x18, 0x00, "50000"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x12, 0x01, 0x60, 0x60, "Easy"						},
	{0x12, 0x01, 0x60, 0x40, "Medium"					},
	{0x12, 0x01, 0x60, 0x20, "Hard"						},
	{0x12, 0x01, 0x60, 0x00, "Hardest"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x12, 0x01, 0x80, 0x80, "Off"						},
	{0x12, 0x01, 0x80, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Flip Screen"				},
	{0x13, 0x01, 0x01, 0x01, "Off"						},
	{0x13, 0x01, 0x01, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Upright Controls"			},
	{0x13, 0x01, 0x02, 0x02, "Single"					},
	{0x13, 0x01, 0x02, 0x00, "Dual"						},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x13, 0x01, 0x04, 0x04, "Off"						},
	{0x13, 0x01, 0x04, 0x00, "On"						},

	{0   , 0xfe, 0   ,    16, "Coin A"					},
	{0x14, 0x01, 0x0f, 0x02, "4 Coins 1 Credits"		},
	{0x14, 0x01, 0x0f, 0x05, "3 Coins 1 Credits"		},
	{0x14, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"		},
	{0x14, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"		},
	{0x14, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"		},
	{0x14, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"		},
	{0x14, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"		},
	{0x14, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"		},
	{0x14, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"		},
	{0x14, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"		},
	{0x14, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"		},
	{0x14, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"		},
	{0x14, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"		},
	{0x14, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"		},
	{0x14, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"		},
	{0x14, 0x01, 0x0f, 0x00, "Free Play"				},

	{0   , 0xfe, 0   ,    15, "Coin B"					},
	{0x14, 0x01, 0xf0, 0x20, "4 Coins 1 Credits"		},
	{0x14, 0x01, 0xf0, 0x50, "3 Coins 1 Credits"		},
	{0x14, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"		},
	{0x14, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"		},
	{0x14, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"		},
	{0x14, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x14, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"		},
	{0x14, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"		},
	{0x14, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"		},
	{0x14, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"		},
	{0x14, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"		},
	{0x14, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"		},
	{0x14, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"		},
	{0x14, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"		},
	{0x14, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"		},
};

STDDIPINFO(Shaolins)

static void shaolins_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x0000:
		//	*flipscreen = data & 0x01;
			*nmi_enable = data & 0x02;
		return;

		case 0x0100:
			watchdog = 0;
		return;

		case 0x0300:
			SN76496Write(0, data);
		return;

		case 0x0400:
			SN76496Write(1, data);
		return;

		case 0x0800:
		case 0x1000:
		return;		// nop

		case 0x1800:
			*palette_bank = data & 0x07;
		return;

		case 0x2000:
			scroll[0] = data + 1;
		return;
	}
}

static UINT8 shaolins_read(UINT16 address)
{
	switch (address)
	{
		case 0x0500:
			return DrvDips[0];

		case 0x0600:
			return DrvDips[1];

		case 0x0700:
		case 0x0701:
		case 0x0702:
			return DrvInputs[address & 3];

		case 0x0703:
			return DrvDips[2];
	}

	return 0;
}

static INT32 DrvDoReset(INT32 clear_ram)
{
	if (clear_ram) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	M6809Open(0);
	M6809Reset();
	M6809Close();

	watchdog = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6809ROM		= Next; Next += 0x00c000;

	DrvGfxROM0		= Next; Next += 0x008000;
	DrvGfxROM1		= Next; Next += 0x010000;

	DrvColPROM		= Next; Next += 0x000500;

	DrvPalette		= (UINT32*)Next; Next += 0x1000 * sizeof(UINT32);

	AllRam			= Next;

	DrvM6809RAM		= Next; Next += 0x000900;
	DrvSprRAM		= Next; Next += 0x000300;
	DrvColRAM		= Next; Next += 0x000400;
	DrvVidRAM		= Next; Next += 0x000400;

	scroll			= (UINT16*)Next; Next += 0x000001 * sizeof(UINT16);
	flipscreen		= Next; Next += 0x000001;
	palette_bank	= Next; Next += 0x000001;
	nmi_enable		= Next; Next += 0x000001;

	RamEnd			= Next;
	MemEnd			= Next;

	return 0;
}

static void DrvGfxDecode()
{
	INT32 Plane0[4] = { 512*16*8+4, 512*16*8+0, 4, 0 };
	INT32 Plane1[4] = { 256*64*8+4, 256*64*8+0, 4, 0 };
	INT32 XOffs[16] = { STEP4(0,1), STEP4(64,1), STEP4(128,1), STEP4(192,1) };
	INT32 YOffs[16] = { STEP8(0,8), STEP8(256,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x8000);

	memcpy (tmp, DrvGfxROM0, 0x04000);

	GfxDecode(0x200, 4,  8,  8, Plane0, XOffs, YOffs, 0x080, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x8000);

	GfxDecode(0x100, 4, 16, 16, Plane1, XOffs, YOffs, 0x200, tmp, DrvGfxROM1);

	BurnFree(tmp);
}

static void DrvPaletteInit()
{
	UINT32 pens[0x100];

	INT32 weights[4] = { 2200, 1000, 470, 220 };

	{
		INT32 total = 0;
		for (INT32 i = 0; i < 4; i++) {
			total += weights[i];
		}

		for (INT32 i = 0; i < 4; i++) {
			weights[i] = (((((INT32)weights[i] * 10000) / total) * 255) + 5000) / 10000;
		}
	}

	for (INT32 i = 0; i < 0x100; i++)
	{
		INT32 bit0 = (DrvColPROM[i + 0x000] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i + 0x000] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i + 0x000] >> 2) & 0x01;
		INT32 bit3 = (DrvColPROM[i + 0x000] >> 3) & 0x01;
		INT32 r = (weights[3] * bit0) + (weights[2] * bit1) + (weights[1] * bit2) + (weights[0] * bit3);

		bit0 = (DrvColPROM[i + 0x100] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + 0x100] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + 0x100] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + 0x100] >> 3) & 0x01;
		INT32 g = (weights[3] * bit0) + (weights[2] * bit1) + (weights[1] * bit2) + (weights[0] * bit3);

		bit0 = (DrvColPROM[i + 0x200] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + 0x200] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + 0x200] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + 0x200] >> 3) & 0x01;
		INT32 b = (weights[3] * bit0) + (weights[2] * bit1) + (weights[1] * bit2) + (weights[0] * bit3);

		pens[i] = BurnHighCol(r,g,b,0);
	}

	for (INT32 i = 0; i < 0x200; i++)
	{
		for (INT32 j = 0; j < 8; j++)
		{
			UINT8 ctabentry = (j << 5) | ((~i & 0x100) >> 4) | (DrvColPROM[0x300+i] & 0x0f);
			DrvPalette[((i & 0x100) << 3) | (j << 8) | (i & 0xff)] = pens[ctabentry];
		}
	}
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
		if (BurnLoadRom(DrvM6809ROM + 0x2000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x4000,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x8000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0  + 0x0000,  3, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x2000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1  + 0x0000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x4000,  6, 1)) return 1;

		if (BurnLoadRom(DrvColPROM  + 0x0000,  7, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x0100,  8, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x0200,  9, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x0300, 10, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x0400, 11, 1)) return 1;

		DrvGfxDecode();
		DrvPaletteInit();
	}

	M6809Init(0);
	M6809Open(0);
	M6809MapMemory(DrvM6809RAM,	0x2800, 0x30ff, MAP_RAM);
	M6809MapMemory(DrvSprRAM,	0x3100, 0x33ff, MAP_RAM);
	M6809MapMemory(DrvColRAM,	0x3800, 0x3bff, MAP_RAM);
	M6809MapMemory(DrvVidRAM,	0x3c00, 0x3fff, MAP_RAM);

	M6809MapMemory(DrvM6809ROM,	0x4000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(shaolins_write);
	M6809SetReadHandler(shaolins_read);
	M6809Close();

	SN76489AInit(0, 1536000, 0);
	SN76496SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	SN76489AInit(1, 3072000, 1);
	SN76496SetRoute(1, 1.00, BURN_SND_ROUTE_BOTH);

    SN76496SetBuffered(M6809TotalCycles, 1536000);

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6809Exit();
	SN76496Exit();

	BurnFree (AllMem);

	return 0;
}

static void draw_layer()
{
	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sx = (offs & 0x1f) * 8;
		INT32 sy = ((offs / 0x20) * 8);

		if (sx >= 32) sy -= scroll[0] & 0xff;
		if (sy < -7) sy += 256;
		sy -= 16;

		INT32 attr  = DrvColRAM[offs];
		INT32 code  = DrvVidRAM[offs] | ((attr & 0x40) << 2);
		INT32 color =(attr & 0x0f) + (*palette_bank << 4);
		INT32 flipy = attr & 0x20;

		Draw8x8Tile(pTransDraw, code, sx, sy, 0, flipy, color, 4, 0, DrvGfxROM0);
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0x300 - 0x20; offs >= 0; offs -= 0x20)
	{
		if (!DrvSprRAM[offs] || !DrvSprRAM[offs + 6]) continue;

		INT32 sy = 248 - DrvSprRAM[offs + 4];
		INT32 sx = 240 - DrvSprRAM[offs + 6];
		INT32 code = DrvSprRAM[offs + 8];
		INT32 attr = DrvSprRAM[offs + 9];
		INT32 color = (attr & 0x0f) + (*palette_bank << 4);
		INT32 flipx = ~attr & 0x40;
		INT32 flipy =  attr & 0x80;

		if (*flipscreen)
		{
			sx = 240 - sx;
			sy = 248 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		Draw16x16MaskTile(pTransDraw, code, sx, sy - 16, flipx, flipy, color, 4, 0, 0x800, DrvGfxROM1);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}
	BurnTransferClear();

	if (nBurnLayer    & 1) draw_layer();
	if (nSpriteEnable & 1) draw_sprites();

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

	{
		memset (DrvInputs, 0xff, 3);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	M6809NewFrame();

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[1] = { 1536000 / 60 };
	INT32 nCyclesDone[1] = { 0 };

	M6809Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, M6809);

		if (*nmi_enable && (i & 0x1f) == 0)
			M6809SetIRQLine(0x20, CPU_IRQSTATUS_AUTO); // 480x/second (8x/frame)

		if (i == 240) M6809SetIRQLine(0, CPU_IRQSTATUS_HOLD);
	}

    if (pBurnSoundOut) {
        SN76496Update(pBurnSoundOut, nBurnSoundLen);
    }

	M6809Close();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029705;
	}

    if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		M6809Scan(nAction);

		SN76496Scan(nAction, pnMin);
	}

	return 0;
}


// Kicker

static struct BurnRomInfo kickerRomDesc[] = {
	{ "477-l03.d9",		0x2000, 0x2598dfdd, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "477-l04.d10",	0x4000, 0x0cf0351a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "477-l05.d11",	0x4000, 0x654037f8, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "477-k06.a10",	0x2000, 0x4d156afc, 2 | BRF_GRA | BRF_ESS }, //  3 Characters
	{ "477-k07.a11",	0x2000, 0xff6ca5df, 2 | BRF_GRA | BRF_ESS }, //  4

	{ "477-k02.h15",	0x4000, 0xb94e645b, 3 | BRF_GRA | BRF_ESS }, //  5 Sprites
	{ "477-k01.h14",	0x4000, 0x61bbf797, 3 | BRF_GRA | BRF_ESS }, //  6

	{ "477j10.a12",		0x0100, 0xb09db4b4, 4 | BRF_GRA | BRF_ESS }, //  7 Color PROMs
	{ "477j11.a13",		0x0100, 0x270a2bf3, 4 | BRF_GRA | BRF_ESS }, //  8
	{ "477j12.a14",		0x0100, 0x83e95ea8, 4 | BRF_GRA | BRF_ESS }, //  9
	{ "477j09.b8",		0x0100, 0xaa900724, 4 | BRF_GRA | BRF_ESS }, // 10
	{ "477j08.f16",		0x0100, 0x80009cf5, 4 | BRF_GRA | BRF_ESS }, // 11
};

STD_ROM_PICK(kicker)
STD_ROM_FN(kicker)

struct BurnDriver BurnDrvKicker = {
	"kicker", NULL, NULL, NULL, "1985",
	"Kicker\0", NULL, "Konami", "GX477",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, kickerRomInfo, kickerRomName, NULL, NULL, NULL, NULL, ShaolinsInputInfo, ShaolinsDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 256, 3, 4
};


// Shao-lin's Road (set 1)

static struct BurnRomInfo shaolinsRomDesc[] = {
	{ "477-l03.d9",		0x2000, 0x2598dfdd, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "477-l04.d10",	0x4000, 0x0cf0351a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "477-l05.d11",	0x4000, 0x654037f8, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "shaolins.a10",	0x2000, 0xff18a7ed, 2 | BRF_GRA | BRF_ESS }, //  3 Characters
	{ "shaolins.a11",	0x2000, 0x5f53ae61, 2 | BRF_GRA | BRF_ESS }, //  4

	{ "477-k02.h15",	0x4000, 0xb94e645b, 3 | BRF_GRA | BRF_ESS }, //  5 Sprites
	{ "477-k01.h14",	0x4000, 0x61bbf797, 3 | BRF_GRA | BRF_ESS }, //  6

	{ "477j10.a12",		0x0100, 0xb09db4b4, 4 | BRF_GRA | BRF_ESS }, //  7 Color PROMs
	{ "477j11.a13",		0x0100, 0x270a2bf3, 4 | BRF_GRA | BRF_ESS }, //  8
	{ "477j12.a14",		0x0100, 0x83e95ea8, 4 | BRF_GRA | BRF_ESS }, //  9
	{ "477j09.b8",		0x0100, 0xaa900724, 4 | BRF_GRA | BRF_ESS }, // 10
	{ "477j08.f16",		0x0100, 0x80009cf5, 4 | BRF_GRA | BRF_ESS }, // 11
};

STD_ROM_PICK(shaolins)
STD_ROM_FN(shaolins)

struct BurnDriver BurnDrvShaolins = {
	"shaolins", "kicker", NULL, NULL, "1985",
	"Shao-lin's Road (set 1)\0", NULL, "Konami", "GX477",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, shaolinsRomInfo, shaolinsRomName, NULL, NULL, NULL, NULL, ShaolinsInputInfo, ShaolinsDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 256, 3, 4
};


// Shao-lin's Road (set 2)

static struct BurnRomInfo shaolinbRomDesc[] = {
	{ "3.h4",		0x2000, 0x2598dfdd, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "4.i4",		0x4000, 0x0cf0351a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "5.j4",		0x4000, 0x654037f8, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "6.i1",		0x2000, 0xff18a7ed, 2 | BRF_GRA | BRF_ESS }, //  3 Characters
	{ "7.j1",		0x4000, 0xd9a7cff6, 2 | BRF_GRA | BRF_ESS }, //  4

	{ "2.m8",		0x4000, 0x560521c7, 3 | BRF_GRA | BRF_ESS }, //  5 Sprites
	{ "1.l8",		0x4000, 0xa79959b2, 3 | BRF_GRA | BRF_ESS }, //  6

	{ "3.k1",		0x0100, 0xb09db4b4, 4 | BRF_GRA | BRF_ESS }, //  7 Color PROMs
	{ "4.l1",		0x0100, 0x270a2bf3, 4 | BRF_GRA | BRF_ESS }, //  8
	{ "5.m1",		0x0100, 0x83e95ea8, 4 | BRF_GRA | BRF_ESS }, //  9
	{ "2.g2",		0x0100, 0xaa900724, 4 | BRF_GRA | BRF_ESS }, // 10
	{ "1.o6",		0x0100, 0x80009cf5, 4 | BRF_GRA | BRF_ESS }, // 11
};

STD_ROM_PICK(shaolinb)
STD_ROM_FN(shaolinb)

struct BurnDriver BurnDrvShaolinb = {
	"shaolinb", "kicker", NULL, NULL, "1985",
	"Shao-lin's Road (set 2)\0", NULL, "Konami", "GX477",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, shaolinbRomInfo, shaolinbRomName, NULL, NULL, NULL, NULL, ShaolinsInputInfo, ShaolinsDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 256, 3, 4
};
