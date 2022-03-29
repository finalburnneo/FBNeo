// FinalBurn Neo Egg Hunt driver module
// Based on MAME driver by David Haywood

#include "tiles_generic.h"
#include "z80_intf.h"
#include "msm6295.h"
#include "burn_pal.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM[2];
static UINT8 *DrvGfxROM[2];
static UINT8 *DrvSndROM;
static UINT8 *DrvZ80RAM[2];
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvColRAM;

static UINT8 soundlatch;
static UINT8 ram_bank;
static UINT8 gfx_bank;
static UINT8 oki_bank;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo EgghuntInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 6,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 5,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 fire 2"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 7,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 6,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 5,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Egghunt)

static struct BurnDIPInfo EgghuntDIPList[]=
{
	{0x10, 0xff, 0xff, 0x61, NULL					},
	{0x11, 0xff, 0xff, 0x7f, NULL					},

	{0   , 0xfe, 0   ,    2, "Debug Mode"			},
	{0x10, 0x01, 0x01, 0x01, "Off"					},
	{0x10, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    3, "Credits per Player"	},
	{0x10, 0x01, 0x60, 0x60, "1"					},
	{0x10, 0x01, 0x60, 0x40, "2"					},
	{0x10, 0x01, 0x60, 0x00, "3"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x10, 0x01, 0x80, 0x80, "Off"					},
	{0x10, 0x01, 0x80, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Censor Pictures"		},
	{0x11, 0x01, 0x80, 0x00, "No"					},
	{0x11, 0x01, 0x80, 0x80, "Yes"					},
};

STDDIPINFO(Egghunt)

static void __fastcall egghunt_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf800) == 0xc000) {
		BurnPalRAM[address & 0x7ff] = data;
		BurnPaletteWrite_xRRRRRGGGGGBBBBB(address & 0x7fe);
		return;
	}
}

static void set_ram_bank(INT32 data)
{
	ram_bank = data & 1;

	ZetMapMemory((ram_bank) ? DrvSprRAM : DrvVidRAM,	0xd000, 0xdfff, MAP_RAM);
}

static void __fastcall egghunt_main_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			set_ram_bank(data);
		return;

		case 0x01:
			gfx_bank = data & 0x33;
		return;

		case 0x03:
			soundlatch = data;
			ZetSetIRQLine(1, 0, CPU_IRQSTATUS_HOLD);
		return;
	}
}

static UINT8 __fastcall egghunt_main_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
			return DrvDips[0];

		case 0x01:
			return DrvInputs[0];

		case 0x02:
			return DrvInputs[1];

		case 0x03:
			return DrvInputs[2];

		case 0x04:
			return DrvDips[1];

		case 0x06:
			return 0xff;
	}

	return 0;
}

static void set_oki_bank(INT32 data)
{
	oki_bank = data;
	MSM6295SetBank(0, DrvSndROM + ((data & 0x10) >> 4) * 0x40000, 0x00000, 0x3ffff);
}

static void __fastcall egghunt_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xe001:
			set_oki_bank(data);
		return;

		case 0xe004:
			MSM6295Write(0, data);
		return;
	}
}

static UINT8 __fastcall egghunt_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xe000:
			return soundlatch;

		case 0xe001:
			return oki_bank;

		case 0xe004:
			return MSM6295Read(0);
	}

	return 0;
}

static tilemap_callback( bg )
{
	INT32 code = ((DrvVidRAM[offs * 2 + 1] & 0x3f) << 8) | DrvVidRAM[offs * 2];

	if ((code & 0x2000) && (gfx_bank & 0x02))  code += 0x2000 << (gfx_bank & 1);

	TILE_SET_INFO(0, code, DrvColRAM[offs], 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetReset(0);
	ZetReset(1);

	set_oki_bank(0);
	MSM6295Reset(0);

	oki_bank = 0;
	ram_bank = 0;
	gfx_bank = 0;
	soundlatch = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM[0]	= Next; Next += 0x020000;
	DrvZ80ROM[1]	= Next; Next += 0x010000;

	DrvGfxROM[0]	= Next; Next += 0x200000;
	DrvGfxROM[1]	= Next; Next += 0x100000;

	DrvSndROM		= Next; Next += 0x080000;

	BurnPalette		= (UINT32*)Next; Next += 0x0400 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM[0]	= Next; Next += 0x002000;
	DrvZ80RAM[1]	= Next; Next += 0x001000;
	DrvVidRAM		= Next; Next += 0x001000;
	DrvSprRAM		= Next; Next += 0x001000;
	DrvColRAM		= Next; Next += 0x000800;
	BurnPalRAM		= Next; Next += 0x000800;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[4]  = { 0x400000, 0x600000, 0x000000, 0x200000 };
	INT32 XOffs0[8] = { STEP8(0,1) };
	INT32 XOffs1[16]= { STEP4(4,1), STEP4(0,1), STEP4(128+4,1), STEP4(128,1) };
	INT32 YOffs[16] = { STEP16(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x100000);
	if (tmp == NULL) {
		return 1;
	}

	for (INT32 i = 0; i < 0x100000; i++) tmp[i] = DrvGfxROM[0][i] ^ 0xff;

	GfxDecode(0x08000, 4,  8,  8, Plane, XOffs0, YOffs, 0x040, tmp, DrvGfxROM[0]);

	for (INT32 i = 0; i < 0x100000; i++) tmp[i] = DrvGfxROM[1][i] ^ 0xff;

	GfxDecode(0x01000, 4, 16, 16, Plane, XOffs1, YOffs, 0x100, tmp, DrvGfxROM[1]);

	BurnFree (tmp);

	return 0;
}

static INT32 DrvInit()
{
	BurnSetRefreshRate(58.0);

	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM[1] + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[0] + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x040000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x080000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x0c0000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[1] + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x040000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x080000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x0c0000, k++, 1)) return 1;

		if (BurnLoadRom(DrvSndROM    + 0x000000, k++, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM[0],		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(BurnPalRAM,		0xc000, 0xc7ff, MAP_ROM); // handler
	ZetMapMemory(DrvColRAM,			0xc800, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM[0],		0xe000, 0xffff, MAP_RAM);
	ZetSetWriteHandler(egghunt_main_write);
	ZetSetOutHandler(egghunt_main_write_port);
	ZetSetInHandler(egghunt_main_read_port);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM[1],		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM[1],		0xF000, 0xffff, MAP_RAM);
	ZetSetWriteHandler(egghunt_sound_write);
	ZetSetReadHandler(egghunt_sound_read);
	ZetClose();

	MSM6295Init(0, 1056000 / 132, 0);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 4,  8,  8, 0x200000, 0x000, 0x3f);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 4, 16, 16, 0x100000, 0x000, 0x0f);
	GenericTilemapSetOffsets(TMAP_GLOBAL, -64, -8);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	MSM6295Exit(0);
	MSM6295ROM = NULL;

	ZetExit();

	BurnFreeMemIndex();

	return 0;
}

static void draw_sprites()
{
	for (INT32 offs = 0x1000 - 0x40; offs >= 0; offs -= 0x20)
	{
		INT32 attr  = DrvSprRAM[offs + 1];
		INT32 code  = DrvSprRAM[offs + 0] | ((attr & 0xe0) << 3);
		INT32 sx    = DrvSprRAM[offs + 3] | ((attr & 0x10) << 4);
		INT32 sy    = ((DrvSprRAM[offs + 2] + 8) & 0xff) - 8;

		if ((attr & 0xe0) && (gfx_bank & 0x20)) code += 0x400 << (gfx_bank & 1);

		DrawGfxMaskTile(0, 1, code, sx - 64, sy - 8, 0, 0, attr & 0x0f, 0xf);
	}
}

static INT32 DrvDraw()
{
	if (BurnRecalc) {
		BurnPaletteUpdate_xRRRRRGGGGGBBBBB();
		BurnRecalc = 0;
	}

	if ((nBurnLayer & 1) == 0) BurnTransferClear();

	if ((nBurnLayer & 1) != 0) GenericTilemapDraw(0, 0, 0);

	if ((nSpriteEnable & 1) != 0) draw_sprites();

	BurnTransferCopy(BurnPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 120;
	INT32 nCyclesTotal[2] = { 6000000 / 60, 6000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		CPU_RUN(0, Zet);
		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(1);
		CPU_RUN(1, Zet);
		ZetClose();
	}

	if (pBurnSoundOut) {
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin != NULL) {
		*pnMin = 0x029707;
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

		MSM6295Scan(nAction, pnMin);

		SCAN_VAR(oki_bank);
		SCAN_VAR(gfx_bank);
		SCAN_VAR(ram_bank);
		SCAN_VAR(soundlatch);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		set_ram_bank(ram_bank);
		ZetClose();

		set_oki_bank(oki_bank);
	}

	return 0;
}


// Egg Hunt

static struct BurnRomInfo egghuntRomDesc[] = {
	{ "prg.bin",	0x20000, 0xeb647145, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code

	{ "rom2.bin",	0x10000, 0x88a71bc3, 2 | BRF_ESS | BRF_PRG }, //  1 Z80 #1 Code

	{ "rom3.bin",	0x40000, 0x9d51ac49, 3 | BRF_GRA },           //  2 Tiles
	{ "rom4.bin",	0x40000, 0x41c63041, 3 | BRF_GRA },           //  3
	{ "rom5.bin",	0x40000, 0x6f96cb97, 3 | BRF_GRA },           //  4
	{ "rom6.bin",	0x40000, 0xb5a41d4b, 3 | BRF_GRA },           //  5

	{ "rom7.bin",	0x20000, 0x1b43fb57, 4 | BRF_GRA },           //  6 Sprites
	{ "rom8.bin",	0x20000, 0xf8122d0d, 4 | BRF_GRA },           //  7
	{ "rom9.bin",	0x20000, 0xdbfa0ffe, 4 | BRF_GRA },           //  8
	{ "rom10.bin",	0x20000, 0x14f5fc74, 4 | BRF_GRA },           //  9

	{ "rom1.bin",	0x80000, 0xf03589bc, 5 | BRF_SND },           // 10 Samples
};

STD_ROM_PICK(egghunt)
STD_ROM_FN(egghunt)

struct BurnDriver BurnDrvEgghunt = {
	"egghunt", NULL, NULL, NULL, "1995",
	"Egg Hunt\0", NULL, "Invi Image", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, egghuntRomInfo, egghuntRomName, NULL, NULL, NULL, NULL, EgghuntInputInfo, EgghuntDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x400,
	384, 240, 4, 3
};
