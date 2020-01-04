// FB Alpha Car Jamborie driver module
// Based on MAME driver by Hap

#include "tiles_generic.h"
#include "z80_intf.h"
#include "ay8910.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 bgcolor;
static UINT8 nmi_enable;
static UINT8 flipscreen;
static UINT8 soundlatch;

static struct BurnInputInfo CarjmbreInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 left"}	,
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Carjmbre)

static struct BurnDIPInfo CarjmbreDIPList[]=
{
	{0x10, 0xff, 0xff, 0x80, NULL			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x10, 0x01, 0x03, 0x02, "2 Coins 1 Credits"	},
	{0x10, 0x01, 0x03, 0x00, "1 Coin  1 Credits"	},
	{0x10, 0x01, 0x03, 0x01, "1 Coin  2 Credits"	},
	{0x10, 0x01, 0x03, 0x03, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Coin B"		},
	{0x10, 0x01, 0x04, 0x00, "1 Coin  5 Credits"	},
	{0x10, 0x01, 0x04, 0x04, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x10, 0x01, 0x18, 0x00, "3"			},
	{0x10, 0x01, 0x18, 0x08, "4"			},
	{0x10, 0x01, 0x18, 0x10, "5"			},
	{0x10, 0x01, 0x18, 0x18, "Free"			},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x10, 0x01, 0x20, 0x00, "10k then every 100k"	},
	{0x10, 0x01, 0x20, 0x20, "20k then every 100k"	},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x10, 0x01, 0x40, 0x00, "Off"			},
	{0x10, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x10, 0x01, 0x80, 0x80, "Upright"		},
	{0x10, 0x01, 0x80, 0x00, "Cocktail"		},
};

STDDIPINFO(Carjmbre)

static void __fastcall carjmbre_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x8803:
			nmi_enable = data & 1;
			ZetSetIRQLine(0x20, CPU_IRQSTATUS_NONE);
		return;

		case 0x8805:
			bgcolor = ~data & 0x3f;
		return;

		case 0x8807:
			flipscreen = data & 0x01;
		return;

		case 0xb800:
			soundlatch = data;
		return;
	}
}

static UINT8 __fastcall carjmbre_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xa000:
			return DrvInputs[0];

		case 0xa800:
			return DrvInputs[1];

		case 0xb800:
			return DrvDips[0];
	}

	return 0;
}

static void __fastcall carjmbre_sound_write(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x20:
		case 0x21:
			AY8910Write(0, port & 1, data);
		return;

		case 0x30:
		case 0x31:
			AY8910Write(1, port & 1, data);
		return;
	}
}

static UINT8 __fastcall carjmbre_sound_read(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
			return soundlatch;

		case 0x24:
			return AY8910Read(0);

		case 0x34:
			return AY8910Read(1);
	}

	return 0;
}

static tilemap_callback( bg )
{
	INT32 attr = DrvVidRAM[offs + 0x400];
	INT32 code = (DrvVidRAM[offs] & 0xff) | (attr << 1 & 0x100);
	TILE_SET_INFO(0, code, attr & 0x0f, TILE_GROUP(((code < 0x33) ? 1 : 0)));
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
	AY8910Reset(1);

	HiscoreReset();

	soundlatch = 0;
	flipscreen = 0;
	nmi_enable = 0;
	bgcolor = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x008000;
	DrvZ80ROM1		= Next; Next += 0x001000;

	DrvGfxROM0		= Next; Next += 0x008000;
	DrvGfxROM1		= Next; Next += 0x010000;

	DrvColPROM		= Next; Next += 0x000040;

	DrvPalette		= (UINT32*)Next; Next += 0x0040 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM0		= Next; Next += 0x000800;
	DrvZ80RAM1		= Next; Next += 0x000800;
	DrvVidRAM		= Next; Next += 0x000800;
	DrvSprRAM		= Next; Next += 0x000100;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}


static INT32 DrvGfxDecode()
{
	INT32 Plane0[2] = { RGN_FRAC(0x2000, 1, 2), 0 };
	INT32 Plane1[2] = { RGN_FRAC(0x4000, 1, 2), 0 };
	INT32 XOffs[16] = { STEP8(0,1), STEP8(256*16*8,1) };
	INT32 YOffs[16] = { STEP16(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x4000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x2000); // tiles

	GfxDecode(0x0200, 2,  8,  8, Plane0, XOffs, YOffs, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x4000); // sprites

	GfxDecode(0x0100, 2, 16, 16, Plane1, XOffs, YOffs, 0x080, tmp, DrvGfxROM1);

	BurnFree(tmp);

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
		if (BurnLoadRom(DrvZ80ROM0 + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x1000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x2000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x3000,  3, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x4000,  4, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x5000,  5, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x6000,  6, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x7000,  7, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x0000,  8, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x0000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x1000, 10, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x0000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x1000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x2000, 13, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x3000, 14, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x0000, 15, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0020, 16, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,	0x8000, 0x87ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,		0x9000, 0x97ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0x9800, 0x98ff, MAP_RAM);
	ZetSetWriteHandler(carjmbre_main_write);
	ZetSetReadHandler(carjmbre_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0x0fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,	0x2000, 0x27ff, MAP_RAM);
	ZetSetOutHandler(carjmbre_sound_write);
	ZetSetInHandler(carjmbre_sound_read);
	ZetClose();

	AY8910Init(0, 1536000, 0);
	AY8910Init(1, 1536000, 1);
	AY8910SetAllRoutes(0, 0.25, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.25, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 2,  8,  8, 0x08000, 0x000, 0x3f);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);
	GenericTilemapSetTransparent(0, 0);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	ZetExit();
	AY8910Exit(0);
	AY8910Exit(1);

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x40; i++)
	{
		INT32 b0 = ((DrvColPROM[i] >> 0) & 1);
		INT32 b1 = ((DrvColPROM[i] >> 1) & 1);
		INT32 b2 = ((DrvColPROM[i] >> 2) & 1);
		INT32 r = b2 * 1000 + b1 * 470 + b0 * 220;

		b0 = ((DrvColPROM[i] >> 3) & 1);
		b1 = ((DrvColPROM[i] >> 4) & 1);
		b2 = ((DrvColPROM[i] >> 5) & 1);
		INT32 g = b2 * 1000 + b1 * 470 + b0 * 220;

		b0 = ((DrvColPROM[i] >> 6) & 1);
		b1 = ((DrvColPROM[i] >> 7) & 1);
		INT32 b = b1 * 470 + b0 * 220;

		r = (r * 255) / (1000 + 470 + 220);
		g = (g * 255) / (1000 + 470 + 220);
		b = (b * 255) / (470 + 220);

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0x60 - 4; offs >= 0; offs -= 4)
	{
		INT32 reoffs= (offs-4+0x60)%0x60; // to get proper ordering(priority) on the ramp
		INT32 sy    = DrvSprRAM[reoffs];
		INT32 code  = DrvSprRAM[reoffs + 1];
		INT32 color = DrvSprRAM[reoffs + 2] & 0x0f;
		INT32 flipx = DrvSprRAM[reoffs + 2] & 0x40;
		INT32 flipy = DrvSprRAM[reoffs + 2] & 0x80;
		INT32 sx    = DrvSprRAM[reoffs + 3];
		if (sy < 3 || sy > 0xfc) continue; // filter "bad" sprites

		if (flipscreen)
		{
			sy += 1;
			sx = 233 - sx;
			flipx ^= 0x40;
			flipy ^= 0x80;
		}
		else
		{
			sy = 241 - sy;
			sx -= 7;
		}

		sy -= 16;

		if (flipy) {
			if (flipx) {
				Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM1);
			} else {
				Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM1);
			}
		} else {
			if (flipx) {
				Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM1);
			} else {
				Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM1);
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

	BurnTransferClear(bgcolor);

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);

	if (nBurnLayer & 2) draw_sprites();

	if (nBurnLayer & 4) GenericTilemapDraw(0, pTransDraw, TMAP_SET_GROUP(1)); // high priority text

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0x00, 2);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}

		ProcessJoystick(&DrvInputs[0], 0, 4,5,6,7, INPUT_4WAY);
		ProcessJoystick(&DrvInputs[1], 1, 4,5,6,7, INPUT_4WAY);
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 3072000 / 60, 1536000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);
		if (i == 240 && nmi_enable) ZetSetIRQLine(0x20, CPU_IRQSTATUS_ACK);
		ZetClose();

		ZetOpen(1);
		nCyclesDone[1] += ZetRun(nCyclesTotal[1] / nInterleave);
		ZetClose();
	}

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
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
		*pnMin = 0x029521;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);
		AY8910Scan(nAction, pnMin);

		SCAN_VAR(soundlatch);
		SCAN_VAR(flipscreen);
		SCAN_VAR(nmi_enable);
		SCAN_VAR(bgcolor);
	}

	return 0;
}

// Car Jamboree

static struct BurnRomInfo carjmbreRomDesc[] = {
	{ "c1",		0x1000, 0x62b21739, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 code
	{ "c2",		0x1000, 0x9ab1a0fa, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "c3",		0x1000, 0xbb29e100, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "c4",		0x1000, 0xc63d8f97, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "c5",		0x1000, 0x4d593942, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "c6",		0x1000, 0xfb576963, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "c7",		0x1000, 0x2b8c4511, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "c8",		0x1000, 0x51cc22a7, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "c15",	0x1000, 0x7d7779d1, 2 | BRF_PRG | BRF_ESS }, //  8 Z80 #1 code

	{ "c10",	0x1000, 0x75ddbe56, 3 | BRF_GRA },           //  9 Background tiles
	{ "c9",		0x1000, 0x2accb821, 3 | BRF_GRA },           // 10

	{ "c11",	0x1000, 0xd90cd126, 4 | BRF_GRA },           // 11 Sprites
	{ "c12",	0x1000, 0xb3bb39d7, 4 | BRF_GRA },           // 12
	{ "c13",	0x1000, 0x3004010b, 4 | BRF_GRA },           // 13
	{ "c14",	0x1000, 0xfb5f0d31, 4 | BRF_GRA },           // 14

	{ "c.d19",	0x0020, 0x220bceeb, 5 | BRF_GRA },           // 15 Color data
	{ "c.d18",	0x0020, 0x7b9ed1b0, 5 | BRF_GRA },           // 16
};

STD_ROM_PICK(carjmbre)
STD_ROM_FN(carjmbre)

struct BurnDriver BurnDrvCarjmbre = {
	"carjmbre", NULL, NULL, NULL, "1983",
	"Car Jamboree\0", NULL, "Omori Electric Co., Ltd.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, carjmbreRomInfo, carjmbreRomName, NULL, NULL, NULL, NULL, CarjmbreInputInfo, CarjmbreDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};
