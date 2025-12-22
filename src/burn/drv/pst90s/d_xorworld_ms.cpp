// FinalBurn Neo Xor World Modular System Prototype driver module
// Based on MAME driver by ?

// Notes:
//	msm5205 never seems to get written to?
//  service mode colors are borked

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "burn_ym3812.h"
#include "msm5205.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM[4];
static UINT8 *Drv68KRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvMgRAM;
static UINT8 *DrvBgRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvScroll;
static UINT8 *DrvZ80RAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 soundlatch;
static UINT8 adpcm_data;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[1];
static UINT8 DrvReset;
static UINT8 DrvInputs[3];

static struct BurnInputInfo Xorworld_msInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dips",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Xorworld_ms)

static struct BurnDIPInfo Xorworld_msDIPList[]=
{
	DIP_OFFSET(0x11)
	{0x00, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    8, "Coin A"				},
	{0x00, 0x01, 0x07, 0x00, "3 Coins 1 Credits"	},
	{0x00, 0x01, 0x07, 0x01, "2 Coins 1 Credits"	},
	{0x00, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x00, 0x01, 0x07, 0x04, "1 Coin  4 Credits"	},
	{0x00, 0x01, 0x07, 0x03, "1 Coin  5 Credits"	},
	{0x00, 0x01, 0x07, 0x02, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x00, 0x01, 0x08, 0x08, "Off"					},
	{0x00, 0x01, 0x08, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x00, 0x01, 0x10, 0x10, "Off"					},
	{0x00, 0x01, 0x10, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x00, 0x01, 0x60, 0x40, "Easy"					},
	{0x00, 0x01, 0x60, 0x60, "Normal"				},
	{0x00, 0x01, 0x60, 0x20, "Hard"					},
	{0x00, 0x01, 0x60, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x00, 0x01, 0x80, 0x80, "Off"					},
	{0x00, 0x01, 0x80, 0x00, "On"					},
};

STDDIPINFO(Xorworld_ms)

static void __fastcall xorworldms_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x40000c:
		break; // unk

		case 0x40000e:
			soundlatch = data & 0xff;
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		break;
	}
}

static void __fastcall xorworldms_main_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x40000c:
		case 0x40000d:
		break; // unk

		case 0x40000e:
		case 0x40000f:
			soundlatch = data & 0xff;
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		break;
	}
}

static UINT16 __fastcall xorworldms_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x400000:
			return DrvInputs[0];

		case 0x400002:
			return DrvInputs[1];

		case 0x400006:
			return DrvDips[0] << 8;

		case 0x400008:
			return ~0;

		case 0x40000c:
			return 0; // unk
	}

	return 0;
}

static UINT8 __fastcall xorworldms_main_read_byte(UINT32 address)
{
	switch (address & ~1)
	{
		case 0x400000:
			return DrvInputs[0];

		case 0x400002:
			return DrvInputs[1];

		case 0x400006:
			return DrvDips[0];

		case 0x400008:
			return ~0;

		case 0x40000c:
			return 0; // unk
	}

	return 0;
}

static void __fastcall xorworldms_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xe000:
			MSM5205ResetWrite(0, data & 0x80);
		return;

		case 0xe400:
			adpcm_data = data;
		return;

		case 0xe800:
		case 0xe801:
			BurnYM3812Write(0, address & 1, data);
		return;
	}
}

static UINT8 __fastcall xorworldms_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xe800:
		case 0xe801:
			return BurnYM3812Read(0, address & 1);

		case 0xf800:
			return soundlatch;
	}

	return 0;
}

static void xorworldms_adpcm_vck()
{
	MSM5205DataWrite(0, adpcm_data >> 4);
	adpcm_data <<= 4;
}

static INT32 DrvMSM5205SynchroniseStream(INT32 nSoundRate)
{
	return (INT64)ZetTotalCycles() * nSoundRate / 4000000;
}

static tilemap_callback( fg )
{
	UINT16 *ram = (UINT16*)DrvFgRAM;

	TILE_SET_INFO(0, BURN_ENDIAN_SWAP_INT16(ram[offs * 2 + 0]), BURN_ENDIAN_SWAP_INT16(ram[offs * 2 + 1]), 0);
}

static tilemap_callback( mg )
{
	UINT16 *ram = (UINT16*)DrvMgRAM;

	TILE_SET_INFO(1, BURN_ENDIAN_SWAP_INT16(ram[offs * 2 + 0]), BURN_ENDIAN_SWAP_INT16(ram[offs * 2 + 1]), 0);
}

static tilemap_callback( bg )
{
	UINT16 *ram = (UINT16*)DrvBgRAM;

	TILE_SET_INFO(2, BURN_ENDIAN_SWAP_INT16(ram[offs * 2 + 0]), BURN_ENDIAN_SWAP_INT16(ram[offs * 2 + 1]), 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	ZetOpen(0);
	BurnYM3812Reset();
	MSM5205Reset();
	ZetReset();
	ZetClose();

	soundlatch = 0;
	adpcm_data = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x020000;

	DrvZ80ROM		= Next; Next += 0x010000;

	DrvGfxROM[0]	= Next; Next += 0x080000;
	DrvGfxROM[1]	= Next; Next += 0x080000;
	DrvGfxROM[2]	= Next; Next += 0x080000;
	DrvGfxROM[3]	= Next; Next += 0x080000;

	DrvPalette		= (UINT32*)Next; Next += 0x0400 * sizeof(UINT32);

	AllRam			= Next;

	Drv68KRAM		= Next; Next += 0x010000;
	DrvPalRAM		= Next; Next += 0x000800;
	DrvFgRAM		= Next; Next += 0x002000;
	DrvMgRAM		= Next; Next += 0x002000;
	DrvBgRAM		= Next; Next += 0x002000;
	DrvSprRAM		= Next; Next += 0x000800;
	DrvScroll		= Next; Next += 0x000400;
	
	DrvZ80RAM		= Next; Next += 0x000800;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Planes[4] = { STEP4(0,8) };
	INT32 XOffs[16] = { STEP8(0,1), STEP8(512,1) };
	INT32 YOffs[16] = { STEP16(0,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x100000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM[0], 0x40000);

	GfxDecode(0x2000, 4,  8,  8, Planes, XOffs, YOffs, 0x100, tmp, DrvGfxROM[0]);

	memcpy (tmp, DrvGfxROM[1], 0x40000);

	GfxDecode(0x2000, 4,  8,  8, Planes, XOffs, YOffs, 0x100, tmp, DrvGfxROM[1]);

	memcpy (tmp, DrvGfxROM[2], 0x40000);

	GfxDecode(0x0800, 4, 16, 16, Planes, XOffs, YOffs, 0x400, tmp, DrvGfxROM[2]);

	memcpy (tmp, DrvGfxROM[3], 0x40000);

	GfxDecode(0x0800, 4, 16, 16, Planes, XOffs, YOffs, 0x400, tmp, DrvGfxROM[3]);

	BurnFree (tmp);

	return 0;
}

static void descramble_16x16tiles()
{
	UINT8 *tmp = (UINT8*)BurnMalloc(0x40000);

	for (INT32 i = 0; i < 0x40000; i++)
	{
		tmp[(i & 0xff801f) | ((i & 0x6000) >> 9) | ((i & 0x1fe0) << 2)] = DrvGfxROM[2][i];
	}

	memcpy (DrvGfxROM[2], tmp, 0x40000);

	BurnFree (tmp);
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM + 0x00000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x00001, k++, 2)) return 1;

		if (BurnLoadRomExt(DrvGfxROM[0] + 0x00003, k++, 4, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[0] + 0x00002, k++, 4, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[0] + 0x00001, k++, 4, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[0] + 0x00000, k++, 4, LD_INVERT)) return 1;

		if (BurnLoadRom(DrvGfxROM[1] + 0x00003, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x00002, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x00001, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x00000, k++, 4)) return 1;

		if (BurnLoadRom(DrvGfxROM[2] + 0x00003, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x00002, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x00001, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x00000, k++, 4)) return 1;

		if (BurnLoadRomExt(DrvGfxROM[3] + 0x00003, k++, 4, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[3] + 0x00002, k++, 4, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[3] + 0x00001, k++, 4, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[3] + 0x00000, k++, 4, LD_INVERT)) return 1;

		if (BurnLoadRom(DrvZ80ROM + 0x00000, k++, 1)) return 1;

		descramble_16x16tiles();
		DrvGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x01ffff, MAP_ROM);
	SekMapMemory(DrvFgRAM,		0x080000, 0x081fff, MAP_RAM);
	SekMapMemory(DrvMgRAM,		0x090000, 0x091fff, MAP_RAM);
	SekMapMemory(DrvBgRAM,		0x0a0000, 0x0a1fff, MAP_RAM);
	SekMapMemory((UINT8*)DrvScroll,	0x0c0000, 0x0c03ff, MAP_RAM); // 0-f actually
	SekMapMemory(DrvSprRAM,		0x100000, 0x1007ff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		0x200000, 0x2007ff, MAP_RAM);
	SekMapMemory(Drv68KRAM,		0xff0000, 0xffffff, MAP_RAM);
	SekSetWriteWordHandler(0,	xorworldms_main_write_word);
	SekSetWriteByteHandler(0,	xorworldms_main_write_byte);
	SekSetReadWordHandler(0,	xorworldms_main_read_word);
	SekSetReadByteHandler(0,	xorworldms_main_read_byte);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,		0xf000, 0xf7ff, MAP_RAM);
	ZetSetWriteHandler(xorworldms_sound_write);
	ZetSetReadHandler(xorworldms_sound_read);
	ZetClose();

	BurnYM3812Init(1, 4000000, NULL, 0);
	BurnTimerAttach(&ZetConfig, 4000000);
	BurnYM3812SetRoute(0, BURN_SND_YM3812_ROUTE, 1.00, BURN_SND_ROUTE_BOTH);

	MSM5205Init(0, DrvMSM5205SynchroniseStream, 384000, xorworldms_adpcm_vck, MSM5205_S48_4B, 1);
	MSM5205SetRoute(0, 0.80, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 16, 16, 64, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, mg_map_callback,  8,  8, 64, 32);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, fg_map_callback,  8,  8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 4,  8,  8, 0x080000, 0x000, 0x1f);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 4,  8,  8, 0x080000, 0x000, 0x1f);
	GenericTilemapSetGfx(2, DrvGfxROM[2], 4, 16, 16, 0x080000, 0x000, 0x1f);
	GenericTilemapSetGfx(3, DrvGfxROM[3], 4, 16, 16, 0x080000, 0x200, 0x0f);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetTransparent(2, 0);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	BurnYM3812Exit();
	MSM5205Exit();

	ZetExit();
	SekExit();

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteUpdate()
{
	UINT16 *p = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x300; i++)
	{
		INT32 r = (p[i] >> 4) & 0xf;
		INT32 g = (p[i] >> 0) & 0xf;
		INT32 b = (p[i] >> 8) & 0xf;

		DrvPalette[i] = BurnHighCol(r+r*16, g+g*16, b+b*16, 0);
	}
}

static void draw_sprites(INT32 sprite_x_offset)
{
	if (sprite_x_offset & 0x800) sprite_x_offset -= 0x1000;

	UINT16 *ram = (UINT16*)DrvSprRAM;

	for (INT32 i = 0x200 - 2; i >= 0; i -= 2)
	{
		UINT16 attr0 = ram[i + 0];
		UINT16 attr1 = ram[i + 1];
		UINT16 attr2 = ram[i + 0x200];

		INT32 sy = attr0 & 0x00ff;

		if (sy == 0x00f8)
			continue;

		INT32 sx = ((attr1 & 0xff00) >> 8) | ((attr2 & 0x8000) >> 7);

		sy = (0xff - sy) | ((attr2 & 0x4000) >> 6);

		INT32 code = ((attr0 & 0xff00) >> 8) | ((attr1 & 0x003f) << 8);
		INT32 color = (attr2 & 0x0f00) >> 8;

		INT32 flipx = attr1 & 0x0040;
		INT32 flipy = attr1 & 0x0080;

		DrawGfxMaskTile(0, 3, code, sx - 16 - (128 + sprite_x_offset), sy - 14, flipx, flipy, color, 15);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1; // force always update
	}

	UINT16 *scr = (UINT16*)DrvScroll;

	GenericTilemapSetScrollX(0, 128 - scr[6]);
	GenericTilemapSetScrollY(0, -scr[7]);
	GenericTilemapSetScrollX(1, 128 - scr[0]);
	GenericTilemapSetScrollY(1, -scr[1]);
	GenericTilemapSetScrollX(2, 128 - scr[4]);
	GenericTilemapSetScrollY(2, -scr[5]);

	if (~nBurnLayer & 1) BurnTransferClear();

	if ( nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);
	if ( nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 0);
	if ( nBurnLayer & 4) GenericTilemapDraw(2, pTransDraw, 0);

	if ( nSpriteEnable & 1) draw_sprites(scr[2] & 0xfff);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	SekNewFrame();
	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 262;
	INT32 nCyclesTotal[2] = { 10000000 / 60, 4000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	SekOpen(0);
	ZetOpen(0);

	MSM5205NewFrame(0, 4000000, nInterleave);

	for (INT32 i = 0; i < nInterleave; i++) {
		CPU_RUN(0, Sek);
		if (i == (nInterleave - 1)) SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);

		CPU_RUN_TIMER(1);
		if ((i%10) == 9) ZetNmi();

		MSM5205UpdateScanline(i);
	}

	if (pBurnSoundOut) {
		BurnYM3812Update(pBurnSoundOut, nBurnSoundLen);
		MSM5205Render(0, pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	SekClose();

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	if (pnMin != NULL) {
		*pnMin = 0x029521;
	}

	if (nAction & ACB_MEMORY_RAM) {
		ScanVar(AllRam, RamEnd-AllRam, "All Ram");
	}

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);
		ZetScan(nAction);

		BurnYM3812Scan(nAction, pnMin);
		MSM5205Scan(nAction, pnMin);

		SCAN_VAR(adpcm_data);
		SCAN_VAR(soundlatch);

	}

	return 0;
}


// Xor World (Modular System, prototype, set 1)

static struct BurnRomInfo xorwldmsRomDesc[] = {
	{ "mod_6-1_xo_608a_27c512.ic8",		0x10000, 0xcebcd3e7, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "mod_6-1_xo_617a_27c512.ic17",	0x10000, 0x47bae292, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "mod_8-2_27c512.ic15",			0x10000, 0x01d16cac, 2 | BRF_GRA },           //  2 Foreground Tiles
	{ "mod_8-2_27c512.ic22",			0x10000, 0x3aadacaf, 2 | BRF_GRA },           //  3
	{ "mod_8-2_27c512.ic30",			0x10000, 0xfa75826e, 2 | BRF_GRA },           //  4
	{ "mod_8-2_27c512.ic37",			0x10000, 0x157832ed, 2 | BRF_GRA },           //  5

	{ "mod_8-2_27c512.ic11",			0x10000, 0x93373f1f, 3 | BRF_GRA },           //  6 Midground Tiles
	{ "mod_8-2_27c512.ic18",			0x10000, 0x7015d8ff, 3 | BRF_GRA },           //  7
	{ "mod_8-2_27c512.ic26",			0x10000, 0x4be8db92, 3 | BRF_GRA },           //  8
	{ "mod_8-2_27c512.ic33",			0x10000, 0xc29cd83b, 3 | BRF_GRA },           //  9

	{ "mod_8-2_27c512.ic13",			0x10000, 0x56a683fc, 4 | BRF_GRA },           // 10 Background Tiles
	{ "mod_8-2_27c512.ic20",			0x10000, 0x950090e6, 4 | BRF_GRA },           // 11
	{ "mod_8-2_27c512.ic28",			0x10000, 0xca950a11, 4 | BRF_GRA },           // 12
	{ "mod_8-2_27c512.ic35",			0x10000, 0x5e7582f9, 4 | BRF_GRA },           // 13

	{ "mod_5-1_27c512.ic3",				0x10000, 0x26b2181e, 5 | BRF_GRA },           // 14 Sprites
	{ "mod_5-1_27c512.ic12",			0x10000, 0xb2bdb8d1, 5 | BRF_GRA },           // 15
	{ "mod_5-1_27c512.ic18",			0x10000, 0xd0c7a07b, 5 | BRF_GRA },           // 16
	{ "mod_5-1_27c512.ic24",			0x10000, 0xfd2cbaed, 5 | BRF_GRA },           // 17

	{ "mod_9-2_27c512.ic6",				0x10000, 0xc722165b, 6 | BRF_PRG | BRF_ESS }, // 18 Z80 Code

	{ "mod_51-3_502_82s129a.ic10",		0x00100, 0x15085e44, 0 | BRF_OPT },           // 19 PROMs
	{ "mod_5-1_52fx_gal16v8.ic8",		0x00117, 0xf3d686c9, 0 | BRF_OPT },           // 20 PLDs
	{ "mod_5-1_51fx_gal16v8.ic9",		0x00117, 0x0070e8b9, 0 | BRF_OPT },           // 21
	{ "mod_6-1_gal16v8.ic7",			0x00117, 0x470a0194, 0 | BRF_OPT },           // 22
	{ "mod_6-1_palce16v8.ic13",			0x00117, 0x0b90f141, 0 | BRF_OPT },           // 23
	{ "mod_7-4_gal20v8.ic7",			0x00157, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 24
	{ "mod_7-4_gal16v8.ic9",			0x00117, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 25
	{ "mod_7-4_gal20v8.ic44",			0x00157, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 26
	{ "mod_7-4_gal20v8.ic54",			0x00157, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 27
	{ "mod_7-4_gal16v8.ic55",			0x00117, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 28
	{ "mod_7-4_gal16v8.ic59",			0x00117, 0x55b86dd5, 0 | BRF_OPT },           // 29
	{ "mod_9-2_gal16v8.ic10",			0x00117, 0x23070765, 0 | BRF_OPT },           // 30
	{ "mod_9-2_gal20v8.ic18",			0x00157, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 31
	{ "mod_9-2_gal16v8.ic42",			0x00117, 0xd62f0a0d, 0 | BRF_OPT },           // 32
	{ "mod_51-3_503_gal16v8.ic46",		0x00117, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 33
};

STD_ROM_PICK(xorwldms)
STD_ROM_FN(xorwldms)

struct BurnDriver BurnDrvXorwldms = {
	"xorwldms", NULL, NULL, NULL, "1990",
	"Xor World (Modular System, prototype, set 1)\0", NULL, "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, xorwldmsRomInfo, xorwldmsRomName, NULL, NULL, NULL, NULL, Xorworld_msInputInfo, Xorworld_msDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	256, 240, 4, 3
};


// Xor World (Modular System, prototype, set 2)

static struct BurnRomInfo xorwldmsaRomDesc[] = {
	{ "xo_608_27c512.bin",				0x10000, 0x12ed0ab0, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "xo_617_27c512.bin",				0x10000, 0xfea2750f, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "mod_8-2_27c512.ic15",			0x10000, 0x01d16cac, 2 | BRF_GRA },           //  2 Foreground Tiles
	{ "mod_8-2_27c512.ic22",			0x10000, 0x3aadacaf, 2 | BRF_GRA },           //  3
	{ "mod_8-2_27c512.ic30",			0x10000, 0xfa75826e, 2 | BRF_GRA },           //  4
	{ "mod_8-2_27c512.ic37",			0x10000, 0x157832ed, 2 | BRF_GRA },           //  5

	{ "mod_8-2_27c512.ic11",			0x10000, 0x93373f1f, 3 | BRF_GRA },           //  6 Midground Tiles
	{ "mod_8-2_27c512.ic18",			0x10000, 0x7015d8ff, 3 | BRF_GRA },           //  7
	{ "mod_8-2_27c512.ic26",			0x10000, 0x4be8db92, 3 | BRF_GRA },           //  8
	{ "mod_8-2_27c512.ic33",			0x10000, 0xc29cd83b, 3 | BRF_GRA },           //  9

	{ "mod_8-2_27c512.ic13",			0x10000, 0x56a683fc, 4 | BRF_GRA },           // 10 Background Tiles
	{ "mod_8-2_27c512.ic20",			0x10000, 0x950090e6, 4 | BRF_GRA },           // 11
	{ "mod_8-2_27c512.ic28",			0x10000, 0xca950a11, 4 | BRF_GRA },           // 12
	{ "mod_8-2_27c512.ic35",			0x10000, 0x5e7582f9, 4 | BRF_GRA },           // 13

	{ "mod_5-1_27c512.ic3",				0x10000, 0x26b2181e, 5 | BRF_GRA },           // 14 Sprites
	{ "mod_5-1_27c512.ic12",			0x10000, 0xb2bdb8d1, 5 | BRF_GRA },           // 15
	{ "mod_5-1_27c512.ic18",			0x10000, 0xd0c7a07b, 5 | BRF_GRA },           // 16
	{ "mod_5-1_27c512.ic24",			0x10000, 0xfd2cbaed, 5 | BRF_GRA },           // 17

	{ "mod_9-2_27c512.ic6",				0x10000, 0xc722165b, 6 | BRF_PRG | BRF_ESS }, // 18 Z80 Code

	{ "mod_51-3_502_82s129a.ic10",		0x00100, 0x15085e44, 0 | BRF_OPT },           // 19 PROMs
	{ "mod_5-1_52fx_gal16v8.ic8",		0x00117, 0xf3d686c9, 0 | BRF_OPT },           // 20 PLDs
	{ "mod_5-1_51fx_gal16v8.ic9",		0x00117, 0x0070e8b9, 0 | BRF_OPT },           // 21
	{ "mod_6-1_gal16v8.ic7",			0x00117, 0x470a0194, 0 | BRF_OPT },           // 22
	{ "mod_6-1_palce16v8.ic13",			0x00117, 0x0b90f141, 0 | BRF_OPT },           // 23
	{ "mod_7-4_gal20v8.ic7",			0x00157, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 24
	{ "mod_7-4_gal16v8.ic9",			0x00117, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 25
	{ "mod_7-4_gal20v8.ic44",			0x00157, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 26
	{ "mod_7-4_gal20v8.ic54",			0x00157, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 27
	{ "mod_7-4_gal16v8.ic55",			0x00117, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 28
	{ "mod_7-4_gal16v8.ic59",			0x00117, 0x55b86dd5, 0 | BRF_OPT },           // 29
	{ "mod_9-2_gal16v8.ic10",			0x00117, 0x23070765, 0 | BRF_OPT },           // 30
	{ "mod_9-2_gal20v8.ic18",			0x00157, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 31
	{ "mod_9-2_gal16v8.ic42",			0x00117, 0xd62f0a0d, 0 | BRF_OPT },           // 32
	{ "mod_51-3_503_gal16v8.ic46",		0x00117, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 33
};

STD_ROM_PICK(xorwldmsa)
STD_ROM_FN(xorwldmsa)

struct BurnDriver BurnDrvXorwldmsa = {
	"xorwldmsa", "xorwldms", NULL, NULL, "1990",
	"Xor World (Modular System, prototype, set 2)\0", NULL, "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, xorwldmsaRomInfo, xorwldmsaRomName, NULL, NULL, NULL, NULL, Xorworld_msInputInfo, Xorworld_msDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	256, 240, 4, 3
};
