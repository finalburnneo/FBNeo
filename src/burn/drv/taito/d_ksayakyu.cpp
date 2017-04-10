// FB Alpha Kusayakyuu (Sandlot Baseball) driver module
// Based on MAME driver by Tomasz Slanina

#include "tiles_generic.h"
#include "z80_intf.h"
#include "driver.h"
extern "C" {
#include "ay8910.h"
}
#include "dac.h"
#include "burn_pal.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvMapROM;
static UINT8 *DrvColPROM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static INT16 *pAY8910Buffer[6];

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 bankdata;
static UINT8 soundlatch;
static UINT8 sound_status;
static UINT8 video_control;
static INT32 flipscreen;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvInputs[3];
static UINT8 DrvDips[1];
static UINT8 DrvReset;

static struct BurnInputInfo KsayakyuInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 2"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Ksayakyu)

static struct BurnDIPInfo KsayakyuDIPList[]=
{
	{0x10, 0xff, 0xff, 0x38, NULL			},

	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{0x10, 0x01, 0x03, 0x03, "2 Coins 1 Credits"	},
	{0x10, 0x01, 0x03, 0x00, "1 Coin  1 Credits"	},
	{0x10, 0x01, 0x03, 0x01, "1 Coin  2 Credits"	},
	{0x10, 0x01, 0x03, 0x02, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x10, 0x01, 0x04, 0x00, "Off"			},
	{0x10, 0x01, 0x04, 0x04, "On"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x10, 0x01, 0x08, 0x08, "Upright"		},
	{0x10, 0x01, 0x08, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    0, "Continue"		},
	{0x10, 0x01, 0x40, 0x00, "7th inning"		},
	{0x10, 0x01, 0x40, 0x40, "1st inning"		},
};

STDDIPINFO(Ksayakyu)

static void bankswitch(INT32 data)
{
	bankdata = data & 1;

	ZetMapMemory(DrvZ80ROM0 + 0x10000 + (bankdata * 0x4000), 0x4000, 0x7fff, MAP_ROM);
}

static void __fastcall ksayakyu_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xa804:
			video_control = data;
		return;

		case 0xa805:
			sound_status &= 0x7f;
			soundlatch = data | 0x80;
		return;

		case 0xa808:
			bankswitch(data);
		return;
	}
}

static UINT8 __fastcall ksayakyu_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xa800:
		case 0xa801:
		case 0xa802:
			return DrvInputs[address & 3];

		case 0xa806:
			return sound_status | 0x04;

		case 0xa803:
		case 0xa807:
			return 0; // watchdog?
	}

	return 0;
}

static void __fastcall ksayakyu_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xa002:
		case 0xa003:
			AY8910Write(0, ~address & 1, data);
		return;

		case 0xa006:
		case 0xa007:
			AY8910Write(1, ~address & 1, data);
		return;

		case 0xa008:
			DACWrite(0, data);
		return;

		case 0xa00c:
			sound_status |= 0x80;
			soundlatch = 0;
		return;

		case 0xa010:
		return; // nop
	}
}

static UINT8 __fastcall ksayakyu_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xa001:
			return AY8910Read(0);
	}

	return 0;
}

static INT32 DrvDACSync()
{
	return (INT32)(float)(nBurnSoundLen * (ZetTotalCycles() / ((18432000.0000 / 8) / (nBurnFPS / 100.0000))));
}

static tilemap_callback( background )
{
	INT32 attr  = DrvMapROM[offs * 2 + 0];
	INT32 code  = DrvMapROM[offs * 2 + 1] + ((attr & 3) << 8);

	TILE_SET_INFO(1, code, (attr >> 1) & 0x1e, (attr & 0x80) ? TILE_FLIPX : 0);
}

static tilemap_callback( foreground )
{
	INT32 attr  = DrvVidRAM[offs * 2 + 0];
	INT32 code  = DrvVidRAM[offs * 2 + 1] + ((attr & 3) << 8);

	TILE_SET_INFO(0, code, (attr >> 2) & 0xf, TILE_FLIPXY(attr >> 6));
}

static UINT8 ay8910_0_portA_r(UINT32)
{
	return soundlatch;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	DACReset();
	ZetClose();

	AY8910Reset(0);
	AY8910Reset(1);

	soundlatch = 0;
	sound_status = 0xff;
	video_control = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x018000;
	DrvZ80ROM1		= Next; Next += 0x008000;

	DrvGfxROM0		= Next; Next += 0x010000;
	DrvGfxROM1		= Next; Next += 0x010000;
	DrvGfxROM2		= Next; Next += 0x010000;

	DrvMapROM		= Next; Next += 0x004000;

	DrvColPROM		= Next; Next += 0x000100;

	DrvPalette		= (UINT32*)Next; Next += 0x0100 * sizeof(UINT32);

	AllRam			= Next;

	DrvVidRAM		= Next; Next += 0x000800;
	DrvSprRAM		= Next; Next += 0x000800;
	DrvZ80RAM0		= Next; Next += 0x000800;
	DrvZ80RAM1		= Next; Next += 0x000400;

	RamEnd			= Next;

	pAY8910Buffer[0]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[1]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[2]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[3]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[4]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[5]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Planes[3]  = { 0, 0x2000 * 8, 0x2000 * 8 * 2 };
	INT32 XOffs[16]  = { STEP8(0,1), STEP8(64,1) };
	INT32 YOffs[16]  = { STEP8(0,8), STEP8(128,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x6000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x6000);

	GfxDecode(0x0400, 3,  8,  8, Planes, XOffs, YOffs, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x4000);

	GfxDecode(0x0400, 2,  8,  8, Planes, XOffs, YOffs, 0x040, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x6000);

	GfxDecode(0x0100, 3, 16, 16, Planes, XOffs, YOffs, 0x100, tmp, DrvGfxROM2);

	BurnFree (tmp);

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
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x10000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x08000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x14000,  3, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  4, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM1 + 0x02000,  5, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM1 + 0x04000,  6, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM1 + 0x06000,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x02000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x04000, 10, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x02000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x04000, 13, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000, 14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x02000, 15, 1)) return 1;

		if (BurnLoadRom(DrvMapROM  + 0x00001, 16, 2)) return 1;
		if (BurnLoadRom(DrvMapROM  + 0x00000, 17, 2)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000, 18, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80ROM0 + 0x8000,	0x8000, 0x9fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,		0xa000, 0xa7ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,			0xb000, 0xb7ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0xb800, 0xbfff, MAP_RAM);
	ZetSetWriteHandler(ksayakyu_main_write);
	ZetSetReadHandler(ksayakyu_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,		0x8000, 0x83ff, MAP_RAM);
	ZetSetWriteHandler(ksayakyu_sound_write);
	ZetSetReadHandler(ksayakyu_sound_read);
	ZetClose();

	AY8910Init(0, 18432000/16, nBurnSoundRate, &ay8910_0_portA_r, NULL, NULL, NULL);
	AY8910Init(1, 18432000/16, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.25, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.25, BURN_SND_ROUTE_BOTH);

	DACInit(0, 0, 1, DrvDACSync);
	DACSetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, background_map_callback, 8, 8, 32, 8 * 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, foreground_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 3, 8, 8, 0x10000, 0x00, 0xf);
	GenericTilemapSetGfx(1, DrvGfxROM1, 2, 8, 8, 0x10000, 0x80, 0x1f);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);
	GenericTilemapSetTransparent(1, 0);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	DACExit();
	AY8910Exit(0);
	AY8910Exit(1);

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x100; i++)
	{
		UINT8 r =  DrvColPROM[i] & 0x07;
		UINT8 g = (DrvColPROM[i] & 0x38) >> 3;
		UINT8 b =  DrvColPROM[i] >> 6;

		DrvPalette[i] = BurnHighCol(pal3bit(r), pal3bit(g), pal2bit(b), 0);
	}
}

static void draw_sprites()
{
	for (INT32 i = 0x400-4; i >= 0; i -= 4)
	{
		INT32 code  = DrvSprRAM[i + 0];
		INT32 sy    = DrvSprRAM[i + 1];
		INT32 sx    = DrvSprRAM[i + 2];
		INT32 attr  = DrvSprRAM[i + 3];
		INT32 color = (attr >> 3) & 0xf;
		INT32 flipx = code & 0x80;
		INT32 flipy = 1;

		if (flipscreen)
		{
			sx = 240 - sx;
			sy = 240 - sy;
			flipx ^= 0x80;
			flipy ^= 1;
		}	

		if (flipy) {
			if (flipx) {
				Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code & 0x7f, sx, sy - 16, color, 3, 0, 0, DrvGfxROM2);
			} else {
				Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code & 0x7f, sx, sy - 16, color, 3, 0, 0, DrvGfxROM2);
			}
		} else {
			if (flipx) {
				Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code & 0x7f, sx, sy - 16, color, 3, 0, 0, DrvGfxROM2);
			} else {
				Render16x16Tile_Mask_Clip(pTransDraw, code & 0x7f, sx, sy - 16, color, 3, 0, 0, DrvGfxROM2);
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

	flipscreen = video_control & 4;

	BurnTransferClear();

	if (flipscreen) {
		GenericTilemapSetFlip(TMAP_GLOBAL, ((video_control & 2) ? TMAP_FLIPY : (TMAP_FLIPY | TMAP_FLIPX)) ^ TMAP_FLIPY);
	} else {
		GenericTilemapSetFlip(TMAP_GLOBAL, ((video_control & 2) ? TMAP_FLIPX : 0) ^ TMAP_FLIPY);
	}

	if ((video_control & 1) && (nBurnLayer & 1))
	{
		GenericTilemapSetScrollY(0, ((video_control & 0xe0) << 3));

		GenericTilemapDraw(0, pTransDraw, 0);
	}
	else
	{
		BurnTransferClear();
	}

	if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 0);

	if (nBurnLayer & 4) draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		DrvInputs[2] = (DrvInputs[2] & 0x80) | (DrvDips[0] & 0x7f);
	}

	ZetNewFrame();

	INT32 nSegment;
	INT32 nInterleave = 256;
	INT32 nTotalCycles[2] = { (18432000 / 8) / 60, (18432000 / 8) / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nSegment = (nTotalCycles[0] - nCyclesDone[0]) / (nInterleave - i);
		nCyclesDone[0] += ZetRun(nSegment);
		if (i == 240) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(1);
		nSegment = (nTotalCycles[1] - nCyclesDone[1]) / (nInterleave - i);
		nCyclesDone[1] += ZetRun(nSegment);
		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();
	}

	if (pBurnSoundOut) {
		AY8910Render(&pAY8910Buffer[0], pBurnSoundOut, nBurnSoundLen, 0);
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
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
		*pnMin = 0x029698;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);
		AY8910Scan(nAction, pnMin);
		DACScan(nAction, pnMin);

		SCAN_VAR(soundlatch);
		SCAN_VAR(sound_status);
		SCAN_VAR(bankdata);
		SCAN_VAR(video_control);
	}

	if (nAction & ACB_WRITE)
	{
		ZetOpen(0);
		bankswitch(bankdata);
		ZetClose();
	}

	return 0;
}


// Kusayakyuu

static struct BurnRomInfo ksayakyuRomDesc[] = {
	{ "1.3a",	0x4000, 0x6607976d, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "2.3b",	0x4000, 0xa289de5c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "4.3d",	0x2000, 0xdb0ca023, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "3.3c",	0x4000, 0xbb4104a5, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "8.5l",	0x2000, 0x3fbae535, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code
	{ "7.5j",	0x2000, 0xad242eda, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "6.5h",	0x2000, 0x17986662, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "5.5f",	0x2000, 0xb0b21817, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "9.3j",	0x2000, 0xef8411dd, 3 | BRF_GRA },           //  8 Foreground Tiles
	{ "10.3k",	0x2000, 0x1bdee573, 3 | BRF_GRA },           //  9
	{ "11.3l",	0x2000, 0xc5859887, 3 | BRF_GRA },           // 10

	{ "14.9j",	0x2000, 0x982d06f0, 4 | BRF_GRA },           // 11 Sprites
	{ "15.9k",	0x2000, 0xdc126df9, 4 | BRF_GRA },           // 12
	{ "16.9m",	0x2000, 0x574a172d, 4 | BRF_GRA },           // 13

	{ "17.9n",	0x2000, 0xa4c4e4ce, 5 | BRF_GRA },           // 14 Background Tiles
	{ "18.9r",	0x2000, 0x9d75b104, 5 | BRF_GRA },           // 15

	{ "13.7r",	0x2000, 0x0acb8c61, 6 | BRF_GRA },           // 16 Background Map
	{ "12.7n",	0x2000, 0xae8d6ed2, 6 | BRF_GRA },           // 17

	{ "9f.bin",	0x0100, 0xff71b27f, 7 | BRF_GRA },           // 18 Color data
};

STD_ROM_PICK(ksayakyu)
STD_ROM_FN(ksayakyu)

struct BurnDriver BurnDrvKsayakyu = {
	"ksayakyu", NULL, NULL, NULL, "1985",
	"Kusayakyuu\0", NULL, "Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_SPORTSMISC, 0,
	NULL, ksayakyuRomInfo, ksayakyuRomName, NULL, NULL, KsayakyuInputInfo, KsayakyuDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 224, 4, 3
};
