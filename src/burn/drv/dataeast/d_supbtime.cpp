// FB Alpha Super Burger Time / China Town driver module
// Based on MAME driver by Bryan McPhail

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "deco16ic.h"
#include "h6280_intf.h"
#include "burn_ym2151.h"
#include "msm6295.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvHucROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvSndROM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvHucRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvSprRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 *flipscreen;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvDips[2];
static UINT8 DrvReset;
static UINT16 DrvInputs[2];

static INT32 nCyclesExtra;

static struct BurnInputInfo SupbtimeInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 15,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Supbtime)

static struct BurnDIPInfo SupbtimeDIPList[]=
{
	DIP_OFFSET(0x011)
	{0x00, 0xff, 0xff, 0xff, NULL					},
	{0x01, 0xff, 0xff, 0xfe, NULL					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x00, 0x01, 0x02, 0x02, "Off"					},
	{0x00, 0x01, 0x02, 0x00, "On"					},

	{0   , 0xfe, 0   ,    8, "Coin B"				},
	{0x00, 0x01, 0x1c, 0x00, "3 Coins 1 Credits"	},
	{0x00, 0x01, 0x1c, 0x10, "2 Coins 1 Credits"	},
	{0x00, 0x01, 0x1c, 0x1c, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0x1c, 0x0c, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0x1c, 0x14, "1 Coin  3 Credits"	},
	{0x00, 0x01, 0x1c, 0x04, "1 Coin  4 Credits"	},
	{0x00, 0x01, 0x1c, 0x18, "1 Coin  5 Credits"	},
	{0x00, 0x01, 0x1c, 0x08, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin A"				},
	{0x00, 0x01, 0xe0, 0x00, "3 Coins 1 Credits"	},
	{0x00, 0x01, 0xe0, 0x80, "2 Coins 1 Credits"	},
	{0x00, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0xe0, 0x60, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"	},
	{0x00, 0x01, 0xe0, 0x20, "1 Coin  4 Credits"	},
	{0x00, 0x01, 0xe0, 0xc0, "1 Coin  5 Credits"	},
	{0x00, 0x01, 0xe0, 0x40, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x01, 0x01, 0x01, 0x01, "Off"					},
	{0x01, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x01, 0x01, 0x02, 0x00, "No"					},
	{0x01, 0x01, 0x02, 0x02, "Yes"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x01, 0x01, 0x30, 0x10, "Easy"					},
	{0x01, 0x01, 0x30, 0x30, "Normal"				},
	{0x01, 0x01, 0x30, 0x20, "Hard"					},
	{0x01, 0x01, 0x30, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    3, "Lives"				},
	{0x01, 0x01, 0xc0, 0x80, "1"					},
	{0x01, 0x01, 0xc0, 0x00, "2"					},
	{0x01, 0x01, 0xc0, 0xc0, "3"					},
};

STDDIPINFO(Supbtime)

static struct BurnDIPInfo ChinatwnDIPList[]=
{
	DIP_OFFSET(0x011)
	{0x00, 0xff, 0xff, 0xff, NULL					},
	{0x01, 0xff, 0xff, 0xfe, NULL					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x00, 0x01, 0x02, 0x02, "Off"					},
	{0x00, 0x01, 0x02, 0x00, "On"					},

	{0   , 0xfe, 0   ,    8, "Coin B"				},
	{0x00, 0x01, 0x1c, 0x00, "3 Coins 1 Credits"	},
	{0x00, 0x01, 0x1c, 0x10, "2 Coins 1 Credits"	},
	{0x00, 0x01, 0x1c, 0x1c, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0x1c, 0x0c, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0x1c, 0x14, "1 Coin  3 Credits"	},
	{0x00, 0x01, 0x1c, 0x04, "1 Coin  4 Credits"	},
	{0x00, 0x01, 0x1c, 0x18, "1 Coin  5 Credits"	},
	{0x00, 0x01, 0x1c, 0x08, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin A"				},
	{0x00, 0x01, 0xe0, 0x00, "3 Coins 1 Credits"	},
	{0x00, 0x01, 0xe0, 0x80, "2 Coins 1 Credits"	},
	{0x00, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0xe0, 0x60, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"	},
	{0x00, 0x01, 0xe0, 0x20, "1 Coin  4 Credits"	},
	{0x00, 0x01, 0xe0, 0xc0, "1 Coin  5 Credits"	},
	{0x00, 0x01, 0xe0, 0x40, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x01, 0x01, 0x01, 0x01, "Off"					},
	{0x01, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x01, 0x01, 0x30, 0x10, "Easy"					},
	{0x01, 0x01, 0x30, 0x30, "Normal"				},
	{0x01, 0x01, 0x30, 0x20, "Hard"					},
	{0x01, 0x01, 0x30, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    4, "Time"					},
	{0x01, 0x01, 0xc0, 0x00, "1500"					},
	{0x01, 0x01, 0xc0, 0x80, "2000"					},
	{0x01, 0x01, 0xc0, 0xc0, "2500"					},
	{0x01, 0x01, 0xc0, 0x40, "3000"					},
};

STDDIPINFO(Chinatwn)

static void __fastcall supbtime_main_write_word(UINT32 address, UINT16 data)
{
	deco16_write_control_word(0, address, 0x300000, data)

	switch (address)
	{
		case 0x100000:
		case 0x1a0000:
			deco16_soundlatch = data & 0xff;
			h6280SetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;
	}
}

static void __fastcall supbtime_main_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x100001:
		case 0x1a0001:
			deco16_soundlatch = data;
			h6280SetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;
	}
}

static UINT16 __fastcall supbtime_main_read_word(UINT32 address)
{
	deco16_read_control_word(0, address, 0x300000)

	switch (address)
	{
		case 0x180000:
			return DrvInputs[0];

		case 0x180002:
			return (DrvDips[1] << 8) | (DrvDips[0] << 0);

		case 0x180008:
			return (DrvInputs[1] & ~0x0008) | (deco16_vblank & 0x0008);

		case 0x18000a:
			return 0;

		case 0x18000c:
			SekSetIRQLine(6, CPU_IRQSTATUS_NONE);
			return 0;
	}

	return 0;
}

static UINT8 __fastcall supbtime_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x180000:
		case 0x180001:
			return DrvInputs[0] >> ((~address & 1) << 3);

		case 0x180002:
			return DrvDips[1];

		case 0x180003:
			return DrvDips[0];

		case 0x180008:
		case 0x180009:
			return (DrvInputs[1] & ~0x0008) | (deco16_vblank & 0x0008);
	}

	return 0;
}

static void palette_onreset() // rainbow fill palette, fixes disappearing "Super" on first titlescreen iteration
{
	for (INT32 i = 0; i < 0x800/2; i++) {
		UINT8 r = ((i & 1) ? 0x0f : 0);
		UINT8 g = ((i & 2) ? 0x0f : 0);
		UINT8 b = ((i & 4) ? 0x0f : 0);

		*((UINT32*)(DrvPalRAM + (i * 2))) =  r | (g << 4) | (b << 8);
	}
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	palette_onreset();

	SekOpen(0);
	SekReset();
	SekClose();

	deco16SoundReset();

	deco16Reset();

	nCyclesExtra = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM	= Next; Next += 0x040000;
	DrvHucROM	= Next; Next += 0x010000;

	DrvGfxROM0	= Next; Next += 0x100000;
	DrvGfxROM1	= Next; Next += 0x100000;
	DrvGfxROM2	= Next; Next += 0x200000;

	MSM6295ROM	= Next;
	DrvSndROM	= Next; Next += 0x040000;

	DrvPalette	= (UINT32*)Next; Next += 0x0400 * sizeof(UINT32);

	AllRam		= Next;

	Drv68KRAM	= Next; Next += 0x004000;
	DrvHucRAM	= Next; Next += 0x002000;
	DrvSprRAM	= Next; Next += 0x000800;
	DrvPalRAM	= Next; Next += 0x000800;

	flipscreen	= Next; Next += 0x000001;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static INT32 DrvInit(INT32 game)
{
	BurnSetRefreshRate(58.00);

	BurnAllocMemIndex();

	{
		if (BurnLoadRom(Drv68KROM  + 0x00001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x00000,  1, 2)) return 1;

		if (BurnLoadRom(DrvHucROM  + 0x00000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00001,  4, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x00000,  5, 2)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x00000,  6, 1)) return 1;

		deco16_tile_decode(DrvGfxROM1, DrvGfxROM0, 0x080000, 1);
		deco16_tile_decode(DrvGfxROM1, DrvGfxROM1, 0x080000, 0);
		deco16_sprite_decode(DrvGfxROM2, 0x100000);
	}	

	deco16Init(1, 0, 1);
	deco16_set_graphics(DrvGfxROM0, 0x80000 * 2, DrvGfxROM1, 0x80000 * 2, NULL, 0);
	deco16_set_color_base(0, 256);
	deco16_set_color_base(1, 512);
	deco16_set_global_offsets(0, 8);

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x03ffff, MAP_ROM);
	if (game) {
		SekMapMemory(Drv68KRAM,		0x100000, 0x103fff, MAP_RAM); // super burger time
	} else {
		SekMapMemory(Drv68KRAM,		0x1a0000, 0x1a3fff, MAP_RAM); // china town
	}
	SekMapMemory(DrvSprRAM,			0x120000, 0x1207ff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x140000, 0x1407ff, MAP_RAM);
	SekMapMemory(deco16_pf_ram[0],		0x320000, 0x321fff, MAP_RAM);
	SekMapMemory(deco16_pf_ram[1],		0x322000, 0x323fff, MAP_RAM);
	SekMapMemory(deco16_pf_rowscroll[0],	0x340000, 0x340bff, MAP_RAM);
	SekMapMemory(deco16_pf_rowscroll[1],	0x342000, 0x342bff, MAP_RAM);
	SekSetWriteWordHandler(0,		supbtime_main_write_word);
	SekSetWriteByteHandler(0,		supbtime_main_write_byte);
	SekSetReadWordHandler(0,		supbtime_main_read_word);
	SekSetReadByteHandler(0,		supbtime_main_read_byte);
	SekClose();

	deco16SoundInit(DrvHucROM, DrvHucRAM, 4027500, 0, NULL, 0.45, 1023924, 0.50, 0, 0);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	deco16Exit();

	SekExit();

	deco16SoundExit();

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteRecalc()
{
	UINT16 *p = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x800 / 2; i++) {
		INT32 b = (BURN_ENDIAN_SWAP_INT16(p[i]) >> 8) & 0x0f; //Seb
		INT32 g = (BURN_ENDIAN_SWAP_INT16(p[i]) >> 4) & 0x0f; //Seb
		INT32 r = (BURN_ENDIAN_SWAP_INT16(p[i]) >> 0) & 0x0f; //Seb

		r |= r << 4;
		g |= g << 4;
		b |= b << 4;

		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}
}

static void draw_sprites()
{
	UINT16 *ram = (UINT16*)DrvSprRAM;

	for (INT32 offs = 0; offs < 0x400; offs += 4)
	{
		INT32 inc, mult;

		INT32 sy     = BURN_ENDIAN_SWAP_INT16(ram[offs + 0]);
		INT32 code   = BURN_ENDIAN_SWAP_INT16(ram[offs + 1]) & 0x3fff;
		INT32 sx     = BURN_ENDIAN_SWAP_INT16(ram[offs + 2]);

		if ((sy & 0x1000) && (nCurrentFrame & 1)) continue;

		INT32 color = (sx >> 9) & 0x1f;

		INT32 flipx = sy & 0x2000;
		INT32 flipy = sy & 0x4000;
		INT32 multi = (1 << ((sy & 0x0600) >> 9)) - 1;

		sx &= 0x01ff;
		sy &= 0x01ff;
		if (sx >= 320) sx -= 512;
		if (sy >= 256) sy -= 512;
		sy = 240 - sy;
		sx = 304 - sx;

		code &= ~multi;

		if (flipy) {
			inc = -1;
		} else {
			code += multi;
			inc = 1;
		}

		if (*flipscreen)
		{
			sy = 240 - sy;
			sx = 304 - sx;
			flipx = !flipx;
			flipy = !flipy;
			mult = 16;
		}
		else
			mult = -16;

		if (sx >= 320 || sx < -15) continue;

		while (multi >= 0)
		{
			INT32 y = (sy + mult * multi) - 8;
			INT32 c = (code - multi * inc) & 0x3fff;

			Draw16x16MaskTile(pTransDraw, c, sx, y, flipx, flipy, color, 4, 0, 0, DrvGfxROM2);

			multi--;
		}
	}
}

static INT32 DrvDraw()
{
//	if (DrvRecalc) {
		DrvPaletteRecalc();
		DrvRecalc = 0;
//	}

	deco16_pf12_update();

	for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
		pTransDraw[i] = 0x300;
	}

	if (nBurnLayer & 1) deco16_draw_layer(1, pTransDraw, 0);

	if (nBurnLayer & 2) draw_sprites();

	if (nBurnLayer & 4) deco16_draw_layer(0, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, 2 * sizeof(UINT16));
		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 232;
	INT32 nCyclesTotal[2] = { 14000000 / 58, 4027500 / 58 };
	INT32 nCyclesDone[2] = { nCyclesExtra, 0 };

	h6280NewFrame();

	SekOpen(0);
	h6280Open(0);

	deco16_vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);
		CPU_RUN_TIMER(1);

		if (i == 206) {
			deco16_vblank = 0x08;
			SekSetIRQLine(6, CPU_IRQSTATUS_ACK);
		}
	}

	h6280Close();
	SekClose();

	nCyclesExtra = nCyclesDone[0] - nCyclesTotal[0];

	if (pBurnSoundOut) {
		deco16SoundUpdate(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin != NULL) {
		*pnMin = 0x029722;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);

		deco16SoundScan(nAction, pnMin);

		deco16Scan();

		SCAN_VAR(nCyclesExtra);
	}

	return 0;
}



// Super Burger Time (World, set 1)

static struct BurnRomInfo supbtimeRomDesc[] = {
	{ "gk03",		0x20000, 0xaeaeed61, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "gk04",		0x20000, 0x2bc5a4eb, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "gc06.bin",		0x10000, 0xe0e6c0f4, 2 | BRF_PRG | BRF_ESS }, //  2 Huc6280 Code

	{ "mae02.bin",		0x80000, 0xa715cca0, 3 | BRF_GRA },           //  3 Characters and Background Tiles

	{ "mae00.bin",		0x80000, 0x30043094, 4 | BRF_GRA },           //  4 Sprites
	{ "mae01.bin",		0x80000, 0x434af3fb, 4 | BRF_GRA },           //  5

	{ "gc05.bin",		0x20000, 0x2f2246ff, 5 | BRF_SND },           //  6 OKI M6295 Samples
	
	{ "tg5.j1",			0x00104, 0x21d02af7, 0 | BRF_OPT },
	{ "tg3.c13",		0x00104, 0x3d5f0e97, 0 | BRF_OPT },
	{ "tg1.b15",		0x00104, 0x819c4522, 0 | BRF_OPT },
	{ "tg2.c12",		0x00104, 0x88f6d299, 0 | BRF_OPT },
	{ "tg0.a11",		0x00104, 0xac6aa74b, 0 | BRF_OPT },
	{ "tg4.c14",		0x00104, 0xe9ee3a67, 0 | BRF_OPT },
};

STD_ROM_PICK(supbtime)
STD_ROM_FN(supbtime)

static INT32 supbtimeInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvSupbtime = {
	"supbtime", NULL, NULL, NULL, "1990",
	"Super Burger Time (World, set 1)\0", NULL, "Data East Corporation", "DECO IC16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_PLATFORM, 0,
	NULL, supbtimeRomInfo, supbtimeRomName, NULL, NULL, NULL, NULL, SupbtimeInputInfo, SupbtimeDIPInfo,
	supbtimeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};


// Super Burger Time (World, set 2)

static struct BurnRomInfo supbtimeaRomDesc[] = {
	{ "3.11f",		0x20000, 0x98b5f263, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "4.12f",		0x20000, 0x937e68b9, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "gc06.bin",		0x10000, 0xe0e6c0f4, 2 | BRF_PRG | BRF_ESS }, //  2 Huc6280 Code

	{ "mae02.bin",		0x80000, 0xa715cca0, 3 | BRF_GRA },           //  3 Characters and Background Tiles

	{ "mae00.bin",		0x80000, 0x30043094, 4 | BRF_GRA },           //  4 Sprites
	{ "mae01.bin",		0x80000, 0x434af3fb, 4 | BRF_GRA },           //  5

	{ "gc05.bin",		0x20000, 0x2f2246ff, 5 | BRF_SND },           //  6 OKI M6295 Samples
};

STD_ROM_PICK(supbtimea)
STD_ROM_FN(supbtimea)

struct BurnDriver BurnDrvSupbtimea = {
	"supbtimea", "supbtime", NULL, NULL, "1990",
	"Super Burger Time (World, set 2)\0", NULL, "Data East Corporation", "DECO IC16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_PLATFORM, 0,
	NULL, supbtimeaRomInfo, supbtimeaRomName, NULL, NULL, NULL, NULL, SupbtimeInputInfo, SupbtimeDIPInfo,
	supbtimeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};


// Super Burger Time (Japan)

static struct BurnRomInfo supbtimejRomDesc[] = {
	{ "gc03.bin",		0x20000, 0xb5621f6a, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "gc04.bin",		0x20000, 0x551b2a0c, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "gc06.bin",		0x10000, 0xe0e6c0f4, 2 | BRF_PRG | BRF_ESS }, //  2 Huc6280 Code

	{ "mae02.bin",		0x80000, 0xa715cca0, 3 | BRF_GRA },           //  3 Characters and Background Tiles

	{ "mae00.bin",		0x80000, 0x30043094, 4 | BRF_GRA },           //  4 Sprites
	{ "mae01.bin",		0x80000, 0x434af3fb, 4 | BRF_GRA },           //  5

	{ "gc05.bin",		0x20000, 0x2f2246ff, 5 | BRF_SND },           //  6 OKI M6295 Samples
};

STD_ROM_PICK(supbtimej)
STD_ROM_FN(supbtimej)

struct BurnDriver BurnDrvSupbtimej = {
	"supbtimej", "supbtime", NULL, NULL, "1990",
	"Super Burger Time (Japan)\0", NULL, "Data East Corporation", "DECO IC16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_PLATFORM, 0,
	NULL, supbtimejRomInfo, supbtimejRomName, NULL, NULL, NULL, NULL, SupbtimeInputInfo, SupbtimeDIPInfo,
	supbtimeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};


// China Town (Japan)

static struct BurnRomInfo chinatwnRomDesc[] = {
	{ "gv_00-.f11",		0x20000, 0x2ea7ea5d, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "gv_01-.f13",		0x20000, 0xbcab03c7, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "gv_02-.f16",		0x10000, 0x95151d84, 2 | BRF_PRG | BRF_ESS }, //  2 audiocpu

	{ "mak-02.h2",		0x80000, 0x745b2c50, 3 | BRF_GRA },           //  3 gfx1

	{ "mak-00.a2",		0x80000, 0x18e8cc1b, 4 | BRF_GRA },           //  4 gfx2
	{ "mak-01.a4",		0x80000, 0xd88ebda8, 4 | BRF_GRA },           //  5

	{ "gv_03-.j14",		0x20000, 0x948faf92, 5 | BRF_SND },           //  6 oki
};

STD_ROM_PICK(chinatwn)
STD_ROM_FN(chinatwn)

static INT32 chinatwnInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvChinatwn = {
	"chinatwn", NULL, NULL, NULL, "1991",
	"China Town (Japan)\0", NULL, "Data East Corporation", "DECO IC16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_PUZZLE, 0,
	NULL, chinatwnRomInfo, chinatwnRomName, NULL, NULL, NULL, NULL, SupbtimeInputInfo, ChinatwnDIPInfo,
	chinatwnInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};
