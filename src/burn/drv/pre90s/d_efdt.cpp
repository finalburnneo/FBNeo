// FB Alpha El Fin Del Tiempo driver module
// Based on MAME driver by El Semi and Roberto Fresca

//
// EFDT Trivia: clearing the soundlatch/soundcontrol after AY8910Reset()
// Kills the soundcpu!
//

#include "tiles_generic.h"
#include "z80_intf.h"
#include "m6800_intf.h"
#include "ay8910.h"

static UINT8 *AllMem;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvM6802ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvM6802RAM0;
static UINT8 *DrvM6802RAM1;
static UINT8 *DrvSoundRegs;
static UINT8 *DrvVidRegs[2];

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 soundlatch;
static UINT8 soundcontrol;

static UINT16 tile_bank;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[1];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static struct BurnInputInfo EfdtInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"	},
	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 6,	"p2 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 start"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Efdt)

static struct BurnDIPInfo EfdtDIPList[]=
{
	{0x0b, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x0b, 0x01, 0x03, 0x00, "1 Coin  1 Credits"	},
	{0x0b, 0x01, 0x03, 0x01, "1 Coin  2 Credits"	},
	{0x0b, 0x01, 0x03, 0x02, "1 Coin  3 Credits"	},
	{0x0b, 0x01, 0x03, 0x03, "2 Coins 3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x0b, 0x01, 0x0c, 0x00, "1 Coin  1 Credits"	},
	{0x0b, 0x01, 0x0c, 0x04, "1 Coin  2 Credits"	},
	{0x0b, 0x01, 0x0c, 0x08, "1 Coin  3 Credits"	},
	{0x0b, 0x01, 0x0c, 0x0c, "2 Coins 3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x0b, 0x01, 0x30, 0x00, "3"					},
	{0x0b, 0x01, 0x30, 0x10, "4"					},
	{0x0b, 0x01, 0x30, 0x20, "5"					},
	{0x0b, 0x01, 0x30, 0x30, "6"					},
};

STDDIPINFO(Efdt)

static void __fastcall efdt_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfffc) == 0x8800) {
		UINT8 offset = address & 3;
		DrvSoundRegs[offset] = data;

		if (offset == 0) soundlatch = data;
		if (offset == 1 && (data & 8)) soundcontrol |= 2;
		return;
	}

	if ((address & 0xfff0) == 0xb400) {
		DrvVidRegs[0][address & 0xf] = data;
		return;
	}

	if ((address & 0xfff0) == 0xb800) {
		DrvVidRegs[1][address & 0xf] = data;
		return;
	}
}

static UINT8 __fastcall efdt_main_read(UINT16 address)
{
	if ((address & 0xfffc) == 0x8800) {
		return DrvSoundRegs[address & 3];
	}

	if ((address & 0xfc00) == 0x9000) {
		return DrvInputs[0];
	}

	if ((address & 0xfc00) == 0x9400) {
		return DrvInputs[1];
	}

	if ((address & 0xfff0) == 0xb400) {
		return DrvVidRegs[0][address & 0xf];
	}

	if ((address & 0xfff0) == 0xb800) {
		return DrvVidRegs[1][address & 0xf];
	}

	return 0x00;
}

static void efdt_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x9000:
			AY8910Write(0, 1, data);
		return;

		case 0x9200:
			AY8910Write(0, 0, data);
		return;

		case 0x9400:
			AY8910Write(1, 1, data);
		return;

		case 0x9600:
			AY8910Write(1, 0, data);
		return;
	}
}

static UINT8 efdt_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x9000:
			return AY8910Read(0);

		case 0x9400:
			return AY8910Read(1);
	}

	return 0x00;
}

static tilemap_callback( layer0 )
{
	UINT8 attr = DrvVidRAM[((offs & 0x1f) * 2) + 0x801];
	UINT8 code = DrvVidRAM[offs];

	TILE_SET_INFO(0, code + tile_bank, attr, 0);
}

static tilemap_callback( layer1 )
{
	TILE_SET_INFO(1, DrvVidRAM[offs + 0xc00], 0, 0);
}

static UINT8 ay8910_soundlatch_read(UINT32)    // 0
{
	return soundlatch;
}

static UINT8 ay8910_soundcontrol_read(UINT32)  // 1
{
	return soundcontrol;
}

static void ay8910_0_write_B(UINT32, UINT32 data)
{
	if (data & 0x04) soundcontrol &= ~2;
}

static void ay8910_1_write_B(UINT32, UINT32 data)
{
	soundcontrol = data;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	M6800Open(0);
	M6800Reset();
	M6800Close();

	soundlatch = 0;   // AY8910Reset() fills them in - keep above!
	soundcontrol = 0;

	AY8910Reset(0);
	AY8910Reset(1);

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x0080000;
	DrvM6802ROM		= Next; Next += 0x0020000;

	DrvGfxROM0		= Next; Next += 0x0200000;
	DrvGfxROM1		= Next; Next += 0x0040000;

	DrvColPROM		= Next; Next += 0x0001000;

	DrvPalette		= (UINT32*)Next; Next += 0x00400 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x0008000;
	DrvVidRAM		= Next; Next += 0x0010000;
	DrvM6802RAM0	= Next; Next += 0x0001000;
	DrvM6802RAM1	= Next; Next += 0x0004000;
	DrvSoundRegs	= Next; Next += 0x0000040;
	DrvVidRegs[0]	= Next; Next += 0x0000100;
	DrvVidRegs[1]	= Next; Next += 0x0000100;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[3]  = { 0, 0x3000*8, 0x6000*8 };
	INT32 XOffs[16] = { STEP8(0,1) };
	INT32 YOffs[16] = { STEP8(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x90000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x9000);

	GfxDecode(0x0600, 3,  8,  8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x0800);

	GfxDecode(0x0100, 1,  8,  8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM1);

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
		if (BurnLoadRom(DrvZ80ROM	+ 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM	+ 0x01000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM	+ 0x02000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM	+ 0x03000,  3, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM	+ 0x04000,  4, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM	+ 0x05000,  5, 1)) return 1;

		if (BurnLoadRom(DrvM6802ROM	+ 0x00000,  6, 1)) return 1;
		if (BurnLoadRom(DrvM6802ROM	+ 0x01000,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0	+ 0x02000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0	+ 0x01000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0	+ 0x00000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0	+ 0x05000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0	+ 0x04000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0	+ 0x03000, 13, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0	+ 0x08000, 14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0	+ 0x07000, 15, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0	+ 0x06000, 16, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1	+ 0x00000, 17, 1)) return 1;

		if (BurnLoadRom(DrvColPROM	+ 0x00000, 18, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,			0x8000, 0x87ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,			0xa000, 0xafff, MAP_RAM);
	ZetSetWriteHandler(efdt_main_write);
	ZetSetReadHandler(efdt_main_read);
	ZetClose();

	M6800Init(0); // M6802!
	M6800Open(0);
	M6800MapMemory(DrvM6802RAM0,		0x0000, 0x00ff, MAP_RAM);
	M6800MapMemory(DrvM6802RAM1,		0x8000, 0x83ff, MAP_RAM);
	M6800MapMemory(DrvM6802ROM,			0xe000, 0xffff, MAP_ROM);
	M6800SetWriteHandler(efdt_sound_write);
	M6800SetReadHandler(efdt_sound_read);
	M6800Close();

	AY8910Init(0, 1789750, 0);
	AY8910SetPorts(0, &ay8910_soundlatch_read,   &ay8910_soundcontrol_read,
				      NULL,                      &ay8910_0_write_B);
	AY8910SetAllRoutes(0, 0.15, BURN_SND_ROUTE_BOTH);

	AY8910Init(1, 1789750, 1);
	AY8910SetPorts(1, &ay8910_soundcontrol_read, &ay8910_soundcontrol_read,
				      NULL,                      &ay8910_1_write_B);
	AY8910SetAllRoutes(1, 0.15, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, layer0_map_callback, 8, 8, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, layer1_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 3, 8, 8, 0x18000, 0x00, 7);
	GenericTilemapSetGfx(1, DrvGfxROM1, 1, 8, 8, 0x04000, 0x38, 0);
	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetScrollCols(0, 32);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	M6800Exit();

	AY8910Exit(0);
	AY8910Exit(1);

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 64; i++)
	{
		if (i & 7)
		{
			UINT8 r = (DrvColPROM[i] >> 3) & 7;
			UINT8 g = (DrvColPROM[i] >> 0) & 7;
			UINT8 b = (DrvColPROM[i] >> 6) & 3;

			r = (r << 5) | (r >> 2);
			g = (g << 5) | (g >> 2);
			b = (b << 6) | (b << 4) | (b << 2) | (b << 0);

			DrvPalette[i] = BurnHighCol(r,g,b,0);
		}
	}
}

static void draw_sprites()
{	
	for (INT32 i = 0; i < 32; i+=4)
	{
		INT32 sy = DrvVidRAM[0x840 + i];
		INT32 sx = DrvVidRAM[0x843 + i];

		if (sy == 0 && sx == 0)
			continue;

		UINT8 color = DrvVidRAM[0x842 + i] & 7;
		UINT16 code = DrvVidRAM[0x841 + i];

		INT32 flipy = code & 0x80;
		INT32 flipx = code & 0x40;
		INT32 flip = code >> 6;

		sy = 256 - sy - 31;

		code = ((code << 2) & 0xfc) | tile_bank;

		Draw8x8MaskTile(pTransDraw, code + (0 ^ flip), sx + 0, sy + 0, flipx, flipy, color, 3, 0, 0, DrvGfxROM0);
		Draw8x8MaskTile(pTransDraw, code + (1 ^ flip), sx + 8, sy + 0, flipx, flipy, color, 3, 0, 0, DrvGfxROM0);
		Draw8x8MaskTile(pTransDraw, code + (2 ^ flip), sx + 0, sy + 8, flipx, flipy, color, 3, 0, 0, DrvGfxROM0);
		Draw8x8MaskTile(pTransDraw, code + (3 ^ flip), sx + 8, sy + 8, flipx, flipy, color, 3, 0, 0, DrvGfxROM0);
	}
}

static void draw_bullets()
{
	for (INT32 i = 0; i < 32; i+=4)
	{
		INT32 sy = 256 - DrvVidRAM[0x861 + i] - 0x10;
		INT32 sx = 256 - DrvVidRAM[0x863 + i] - 8;

		if (sy >= 0 && sx >= 0 && sx < nScreenWidth && sy < nScreenHeight) {
			pTransDraw[sy * nScreenWidth + sx] = 7;
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	tile_bank = ((DrvVidRegs[0][4] == 0xff) ? (DrvVidRegs[0][7] & 7) : 1) << 8;

	for (INT32 i = 0; i < 32; i++) {
		GenericTilemapSetScrollCol(0, i, DrvVidRAM[0x800 + (i * 2)]);
	}

	BurnTransferClear();

	if (nBurnLayer & 1) GenericTilemapDraw(1, pTransDraw, 0);
	if (nBurnLayer & 2) GenericTilemapDraw(0, pTransDraw, 0);

	if (nSpriteEnable & 1) draw_sprites();
	if (nSpriteEnable & 2) draw_bullets();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		DrvInputs[0] = 0xff;
		DrvInputs[1] = 0xff;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}

		DrvInputs[1] = (DrvInputs[1] & 0xc0) | DrvDips[0];

	}

	INT32 nInterleave = 256;
	INT32 m6800I = nInterleave / 7;

	INT32 nCyclesTotal[2] = { 3072000 / 60, 3579500 / 4 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };
	INT32 nSoundBufferPos = 0;

	ZetOpen(0);
	M6800Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone[0] += ZetRun(((i + 1) * nCyclesTotal[0] / nInterleave) - nCyclesDone[0]);

		if (i == 240) ZetNmi();

		nCyclesDone[1] += M6800Run(((i + 1) * nCyclesTotal[1] / nInterleave) - nCyclesDone[1]);
		if (i%m6800I == m6800I-1) {
			M6800SetIRQLine(0, CPU_IRQSTATUS_HOLD); // should be 7.27 per frame (we do 7.)
		}

		// Render Sound Segment
		if (pBurnSoundOut && (i%8)==7) {
			INT32 nSegmentLength = nBurnSoundLen / (nInterleave/8);
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			AY8910Render(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	// Make sure the buffer is entirely filled.
	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			AY8910Render(pSoundBuf, nSegmentLength);
		}
	}

	M6800Close();
	ZetClose();

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
		M6800Scan(nAction);
		AY8910Scan(nAction, pnMin);

		SCAN_VAR(soundcontrol);
		SCAN_VAR(soundlatch);
	}

	return 0;
}


// El Fin Del Tiempo

static struct BurnRomInfo efdtRomDesc[] = {
	{ "22805.b10",			0x1000, 0x7e27df91, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "22806.b9",			0x1000, 0x00cad810, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "22807.b8",			0x1000, 0x8e51af2b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "22808.b7",			0x1000, 0x932bd16d, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "22801.a10",			0x1000, 0xea646049, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "22802.a9",			0x1000, 0x74457952, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "1811.d8",			0x1000, 0x0ff5d0c2, 2 | BRF_PRG | BRF_ESS }, //  6 M6802 Code
	{ "1812.d7",			0x1000, 0x48e5a4ac, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "12822.j3",			0x1000, 0xf4d28a60, 3 | BRF_GRA },           //  8 Foreground & Sprite Tiles
	{ "12821.j4",			0x1000, 0xb7ef75a6, 3 | BRF_GRA },           //  9
	{ "12820.j5",			0x1000, 0x70126c8d, 3 | BRF_GRA },           // 10
	{ "12819.j6",			0x1000, 0x2987b5b6, 3 | BRF_GRA },           // 11
	{ "12818.j7",			0x1000, 0xe0a61419, 3 | BRF_GRA },           // 12
	{ "12817.j8",			0x1000, 0x856a2537, 3 | BRF_GRA },           // 13
	{ "12816.j9",			0x1000, 0x69664044, 3 | BRF_GRA },           // 14
	{ "12815.j10",			0x1000, 0xabe7a7b6, 3 | BRF_GRA },           // 15
	{ "12814.j11",			0x1000, 0x6c06f746, 3 | BRF_GRA },           // 16

	{ "12813.h10",			0x0800, 0xea03c5a8, 4 | BRF_GRA },           // 17 Background Tiles

	{ "ft1-tbp28l22n.bin",	0x0100, 0x04d7b68c, 5 | BRF_GRA },           // 18 Color data
};

STD_ROM_PICK(efdt)
STD_ROM_FN(efdt)

struct BurnDriver BurnDrvEfdt = {
	"efdt", NULL, NULL, NULL, "1981",
	"El Fin Del Tiempo\0", NULL, "Niemer", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, efdtRomInfo, efdtRomName, NULL, NULL, NULL, NULL, EfdtInputInfo, EfdtDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};
