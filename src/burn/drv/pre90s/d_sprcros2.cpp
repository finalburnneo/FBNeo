// FB Alpha Super Cross II driver module
// Based on MAME driver by Angelo Salese

#include "tiles_generic.h"
#include "z80_intf.h"
#include "sn76496.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvColPROM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvBgRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvShareRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 nmi_enable[2];
static UINT8 irq_enable;
static UINT8 screen_enable;
static UINT8 z80_bank[2];
static UINT8 scrollx;
static UINT8 scrolly;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo Sprcros2InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 6,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Sprcros2)

static struct BurnDIPInfo Sprcros2DIPList[]=
{
	{0x11, 0xff, 0xff, 0xf8, NULL					},
	{0x12, 0xff, 0xff, 0x0f, NULL					},

	{0   , 0xfe, 0   ,    8, "Coinage"				},
	{0x11, 0x01, 0x07, 0x07, "3 Coins 1 Credits"	},
	{0x11, 0x01, 0x07, 0x06, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x07, 0x00, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x07, 0x01, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0x07, 0x02, "1 Coin  3 Credits"	},
	{0x11, 0x01, 0x07, 0x03, "1 Coin  4 Credits"	},
	{0x11, 0x01, 0x07, 0x04, "1 Coin  5 Credits"	},
	{0x11, 0x01, 0x07, 0x05, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x12, 0x01, 0x01, 0x01, "Upright"				},
	{0x12, 0x01, 0x01, 0x00, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x12, 0x01, 0x80, 0x00, "Off"					},
	{0x12, 0x01, 0x80, 0x80, "On"					},
};

STDDIPINFO(Sprcros2)

static void bankswitch0(INT32 data)
{
	INT32 bank = 0xc000 + (data * 0x2000);

	z80_bank[0] = data;

	ZetMapMemory(DrvZ80ROM0 + bank, 0xc000, 0xdfff, MAP_ROM);
}

static void bankswitch1(INT32 data)
{
	INT32 bank = 0xc000 + (data * 0x2000);

	z80_bank[1] = data;

	ZetMapMemory(DrvZ80ROM1 + bank, 0xc000, 0xdfff, MAP_ROM);
}

static void __fastcall sprcros2_main_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
		case 0x02:
			SN76496Write(port & 3, data);
		return;

		case 0x07:
			bankswitch0((data & 0x40) >> 6);
			nmi_enable[0] = data & 1;
			screen_enable = data & 4;
			irq_enable = data & 8;
		return;
	}
}

static UINT8 __fastcall sprcros2_main_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
			return DrvInputs[0];

		case 0x01:
			return DrvInputs[1];

		case 0x02:
			return DrvInputs[2];

		case 0x04:
			return DrvDips[0];

		case 0x05:
			return DrvDips[1];
	}

	return 0;
}

static void __fastcall sprcros2_sub_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			scrollx = data;
		return;

		case 0x01:
			scrolly = data;
		return;

		case 0x03:
			bankswitch1((data & 8) >> 3);
			nmi_enable[1] = data & 1;
		return;
	}
}

static tilemap_callback( bg )
{
	INT32 attr = DrvBgRAM[offs + 0x400];

	TILE_SET_INFO(0, DrvBgRAM[offs] + (attr << 8), attr >> 4, (attr & 0x08) ? TILE_FLIPX : 0);
}

static tilemap_callback( fg )
{
	INT32 attr = DrvFgRAM[offs + 0x400];
	INT32 color = attr >> 2; // 0x3f

	TILE_SET_INFO(1, DrvFgRAM[offs] + (attr << 8), color, (color < 0x30) ? TILE_OPAQUE : 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	bankswitch0(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	bankswitch1(0);
	ZetReset();
	ZetClose();

	nmi_enable[0] = 0;
	nmi_enable[1] = 0;
	irq_enable = 0;
	scrollx = 0;
	scrolly = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x010000;
	DrvZ80ROM1		= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += 0x020000;
	DrvGfxROM1		= Next; Next += 0x020000;
	DrvGfxROM2		= Next; Next += 0x010000;

	DrvColPROM		= Next; Next += 0x000420;

	DrvPalette		= (UINT32*)Next; Next += 0x0300 * sizeof(UINT32);

	AllRam			= Next;

	DrvFgRAM		= Next; Next += 0x000800;
	DrvBgRAM		= Next; Next += 0x001800;
	DrvSprRAM		= Next; Next += 0x001000;
	DrvShareRAM		= Next; Next += 0x000800;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode() // 0, 100, 200
{
	INT32 Plane0[3] = { 0x4000*0*8, 0x4000*1*8, 0x4000*2*8 }; // iq_132 gfx_8x8x3_planar
	INT32 XOffs0[8] = { STEP8(0,1) };
	INT32 YOffs0[8] = { STEP8(0,8) };

	INT32 Plane1[3]  = { 0x4000*0*8, 0x4000*1*8, 0x4000*2*8 };
	INT32 XOffs1[32] = { STEP8(0,1), STEP8(256,1), STEP8(512,1), STEP8(768,1) };
	INT32 YOffs1[32] = { STEP16(0,8), STEP16(128,8) };

	INT32 Plane2[2] = { 0, 4 };
	INT32 XOffs2[8] = { STEP4(64,1), STEP4(0,1) };
	INT32 YOffs2[8] = { STEP8(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0xc000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0xc000);

	GfxDecode(0x0800, 3,  8,  8, Plane0, XOffs0, YOffs0, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0xc000);

	GfxDecode(0x0080, 3, 32, 32, Plane1, XOffs1, YOffs1, 0x400, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x4000);

	GfxDecode(0x0400, 2,  8,  8, Plane2, XOffs2, YOffs2, 0x080, tmp, DrvGfxROM2);

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
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x04000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x08000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x0c000,  3, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  4, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM1 + 0x04000,  5, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM1 + 0x08000,  6, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM1 + 0x0c000,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x04000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x08000, 10, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x04000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x08000, 13, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000, 14, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000, 15, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00020, 16, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00120, 17, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00220, 18, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00320, 19, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvFgRAM,			0xe000, 0xe7ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0xe800, 0xf7ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,		0xf800, 0xffff, MAP_RAM);
	ZetSetOutHandler(sprcros2_main_write_port);
	ZetSetInHandler(sprcros2_main_read_port);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,		0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvBgRAM,			0xe000, 0xf7ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,		0xf800, 0xffff, MAP_RAM);
	ZetSetOutHandler(sprcros2_sub_write_port);
	ZetClose();

	SN76489Init(0, 2500000, 0);
	SN76489Init(1, 2500000, 1);
	SN76489Init(2, 2500000, 1);
	SN76496SetRoute(0, 0.30, BURN_SND_ROUTE_BOTH);
	SN76496SetRoute(1, 0.30, BURN_SND_ROUTE_BOTH);
	SN76496SetRoute(2, 0.30, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, fg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 3, 8, 8, 0x20000, 0x000, 0x0f);
	GenericTilemapSetGfx(1, DrvGfxROM2, 2, 8, 8, 0x10000, 0x200, 0x3f);
	GenericTilemapSetOffsets(TMAP_GLOBAL, -8, -16);
	GenericTilemapSetTransparent(1, 0);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	SN76496Exit();

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	UINT32 pal[0x20];

	for (INT32 i = 0; i < 0x20; i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 0x01;
		INT32 r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = (DrvColPROM[i] >> 3) & 0x01;
		bit1 = (DrvColPROM[i] >> 4) & 0x01;
		bit2 = (DrvColPROM[i] >> 5) & 0x01;
		INT32 g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = (DrvColPROM[i] >> 6) & 0x01;
		bit1 = (DrvColPROM[i] >> 7) & 0x01;
		INT32 b = 0x47 * bit0 + 0xb8 * bit1;

		pal[i] = BurnHighCol(r,g,b,0);
	}

	for (INT32 i = 0; i < 0x100; i++)
	{
		DrvPalette[i] = pal[(DrvColPROM[0x020 + i] & 0x0f) | ((DrvColPROM[i + 0x120] & 0x01) << 4)];
	}

	for (INT32 i = 0x100; i < 0x300; i++)
	{
		DrvPalette[i] = pal[DrvColPROM[0x120 + i] & 0x1f];
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0x40-4; offs>=0;offs -=4)
	{
		INT32 sy	= (224-DrvSprRAM[offs + 2]) & 0xff;
		INT32 sx	= DrvSprRAM[offs + 3];
		INT32 code	= DrvSprRAM[offs + 0];
		INT32 color	=(DrvSprRAM[offs + 1] & 0x38) >> 3;
		INT32 flipx	= DrvSprRAM[offs + 1] & 0x02;

		if (flipx) {
			Render32x32Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0x100, DrvGfxROM1);
		} else {
			Render32x32Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0x100, DrvGfxROM1);
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	if (1) // iq_132 screen_enable)
	{
		GenericTilemapSetScrollX(0, scrollx);
		GenericTilemapSetScrollY(0, scrolly);

		GenericTilemapDraw(0, pTransDraw, 0);
		draw_sprites();
		GenericTilemapDraw(1, pTransDraw, 0);
	}
	else
	{
		BurnTransferClear();
	}

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
	}

	INT32 nInterleave = 262;
	INT32 nCyclesTotal[2] = { 3500000 / 60, 3500000 / 60 }; // oc'd +1mhz to avoid bg desynchs
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(((i + 1) * nCyclesTotal[0] / nInterleave) - nCyclesDone[0]);
		if (i == 232 && nmi_enable[0]) ZetNmi();
		if (i == 0 && irq_enable) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(1);
		nCyclesDone[1] += ZetRun(((i + 1) * nCyclesTotal[1] / nInterleave) - nCyclesDone[1]);
		if (i == 232 && nmi_enable[1]) ZetNmi();
		ZetClose();
	}

	if (pBurnSoundOut) {
		SN76496Update(0, pBurnSoundOut, nBurnSoundLen);
		SN76496Update(1, pBurnSoundOut, nBurnSoundLen);
		SN76496Update(2, pBurnSoundOut, nBurnSoundLen);
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
		SN76496Scan(nAction, pnMin);

		SCAN_VAR(scrollx);
		SCAN_VAR(scrolly);
		SCAN_VAR(nmi_enable);
		SCAN_VAR(irq_enable);
		SCAN_VAR(z80_bank);
	}

	if (nAction & ACB_WRITE)
	{
		ZetOpen(0);
		bankswitch0(z80_bank[0]);
		ZetClose();

		ZetOpen(1);
		bankswitch1(z80_bank[1]);
		ZetClose();
	}

	return 0;
}


// Super Cross II (Japan, set 1)

static struct BurnRomInfo sprcros2RomDesc[] = {
	{ "scm-03.10g",	0x4000, 0xb9757908, 1 | BRF_GRA },           //  0 Z80 #0 Code
	{ "scm-02.10j",	0x4000, 0x849c5c87, 1 | BRF_GRA },           //  1
	{ "scm-01.10k",	0x4000, 0x385a62de, 1 | BRF_GRA },           //  2
	{ "scm-00.10l",	0x4000, 0x13fa3684, 1 | BRF_GRA },           //  3

	{ "scs-30.5f",	0x4000, 0xc0a40e41, 2 | BRF_GRA },           //  4 Z80 #1 Code
	{ "scs-29.5h",	0x4000, 0x83d49fa5, 2 | BRF_GRA },           //  5
	{ "scs-28.5j",	0x4000, 0x480d351f, 2 | BRF_GRA },           //  6
	{ "scs-27.5k",	0x4000, 0x2cf720cb, 2 | BRF_GRA },           //  7

	{ "scs-26.4b",	0x4000, 0xf958b56d, 3 | BRF_GRA },           //  8 Background Tiles
	{ "scs-25.4c",	0x4000, 0xd6fd7ba5, 3 | BRF_GRA },           //  9
	{ "scs-24.4e",	0x4000, 0x87783c36, 3 | BRF_GRA },           // 10

	{ "scm-23.5b",	0x4000, 0xab42f8e3, 4 | BRF_GRA },           // 11 Sprites
	{ "scm-22.5e",	0x4000, 0x0cad254c, 4 | BRF_GRA },           // 12
	{ "scm-21.5g",	0x4000, 0xb6b68998, 4 | BRF_GRA },           // 13

	{ "scm-20.5k",	0x4000, 0x67a099a6, 5 | BRF_GRA },           // 14 Foreground Tiles

	{ "sc-64.6a",	0x0020, 0x336dd1c0, 6 | BRF_GRA },           // 15 Color data
	{ "sc-63.3b",	0x0100, 0x9034a059, 6 | BRF_GRA },           // 16
	{ "sc-62.3a",	0x0100, 0x3c78a14f, 6 | BRF_GRA },           // 17
	{ "sc-61.5a",	0x0100, 0x2f71185d, 6 | BRF_GRA },           // 18
	{ "sc-60.4k",	0x0100, 0xd7a4e57d, 6 | BRF_GRA },           // 19
};

STD_ROM_PICK(sprcros2)
STD_ROM_FN(sprcros2)

struct BurnDriver BurnDrvSprcros2 = {
	"sprcros2", NULL, NULL, NULL, "1986",
	"Super Cross II (Japan, set 1)\0", NULL, "GM Shoji", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, sprcros2RomInfo, sprcros2RomName, NULL, NULL, NULL, NULL, Sprcros2InputInfo, Sprcros2DIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	240, 224, 4, 3
};


// Super Cross II (Japan, set 2)

static struct BurnRomInfo sprcros2aRomDesc[] = {
	{ "15.bin",		0x4000, 0xb9d02558, 1 | BRF_GRA },           //  0 Z80 #0 Code
	{ "scm-02.10j",	0x4000, 0x849c5c87, 1 | BRF_GRA },           //  1
	{ "scm-01.10k",	0x4000, 0x385a62de, 1 | BRF_GRA },           //  2
	{ "scm-00.10l",	0x4000, 0x13fa3684, 1 | BRF_GRA },           //  3

	{ "scs-30.5f",	0x4000, 0xc0a40e41, 2 | BRF_GRA },           //  4 Z80 #1 Code
	{ "scs-29.5h",	0x4000, 0x83d49fa5, 2 | BRF_GRA },           //  5
	{ "scs-28.5j",	0x4000, 0x480d351f, 2 | BRF_GRA },           //  6
	{ "scs-27.5k",	0x4000, 0x2cf720cb, 2 | BRF_GRA },           //  7

	{ "scs-26.4b",	0x4000, 0xf958b56d, 3 | BRF_GRA },           //  8 Background Tiles
	{ "scs-25.4c",	0x4000, 0xd6fd7ba5, 3 | BRF_GRA },           //  9
	{ "scs-24.4e",	0x4000, 0x87783c36, 3 | BRF_GRA },           // 10

	{ "scm-23.5b",	0x4000, 0xab42f8e3, 4 | BRF_GRA },           // 11 Sprites
	{ "scm-22.5e",	0x4000, 0x0cad254c, 4 | BRF_GRA },           // 12
	{ "scm-21.5g",	0x4000, 0xb6b68998, 4 | BRF_GRA },           // 13

	{ "scm-20.5k",	0x4000, 0x67a099a6, 5 | BRF_GRA },           // 14 Foreground Tiles

	{ "sc-64.6a",	0x0020, 0x336dd1c0, 6 | BRF_GRA },           // 15 Color data
	{ "sc-63.3b",	0x0100, 0x9034a059, 6 | BRF_GRA },           // 16
	{ "sc-62.3a",	0x0100, 0x3c78a14f, 6 | BRF_GRA },           // 17
	{ "sc-61.5a",	0x0100, 0x2f71185d, 6 | BRF_GRA },           // 18
	{ "sc-60.4k",	0x0100, 0xd7a4e57d, 6 | BRF_GRA },           // 19
};

STD_ROM_PICK(sprcros2a)
STD_ROM_FN(sprcros2a)

struct BurnDriver BurnDrvSprcros2a = {
	"sprcros2a", "sprcros2", NULL, NULL, "1986",
	"Super Cross II (Japan, set 2)\0", NULL, "GM Shoji", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, sprcros2aRomInfo, sprcros2aRomName, NULL, NULL, NULL, NULL, Sprcros2InputInfo, Sprcros2DIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	240, 224, 4, 3
};
