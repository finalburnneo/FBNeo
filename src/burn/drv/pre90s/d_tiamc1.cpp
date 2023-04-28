// FinalBurn Neo TIA-MC1 driver module
// Based on MAME driver by Eugene Sandulenko, w/special thanks to Shiru for his standalone emulator and documentation

#include "tiles_generic.h"
#include "z80_intf.h"
#include "8255ppi.h"
#include "burn_pal.h"
#include "tiamc1_snd.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvI8080ROM;
static UINT8 *DrvSprROM;
static UINT8 *DrvTileROM;
static UINT8 *DrvI8080RAM;
static UINT8 *DrvTileRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvCharRAM;
static UINT8 *DrvCharRAMExp;

static UINT32 *DrvPalette;

static UINT8 layer_control;
static INT32 character_bank;
static UINT8 scrollx;
static UINT8 scrolly;
static UINT8 bg_color;
static INT32 update_characters;
static INT32 update_colors;

static INT32 vblank;
static UINT32 button_config = 0;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static INT16 Analog[1];

static INT32 is_gorodki = 0;
static INT32 is_kot = 0;

static INT32 nExtraCycles;

static struct BurnInputInfo DrvInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 4,	"p1 coin"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 5,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 left"}	,
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy3 + 6,	"p1 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},

	{"Service Mode",	BIT_DIGITAL,	DrvJoy2 + 7,	"diag"		},
};

STDINPUTINFO(Drv)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo GorodkiInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 4,	"p1 coin"	},
	A("P1 Stick X",     BIT_ANALOG_REL, &Analog[0],		"p1 x-axis"),
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy3 + 6,	"p1 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},

	{"Service Mode",	BIT_DIGITAL,	DrvJoy2 + 7,	"diag"		},
};
#undef A
STDINPUTINFO(Gorodki)

static void __fastcall tiamc1_videoram_write(UINT16 address, UINT8 data)
{
	if ((address & 0x0800) == 0x0000) // b000-b7ff (tiamc1) f000-f3ff (kot)
	{
		INT32 offs = address & 0x7ff;

		if (layer_control & 0x1e) { update_characters = 1; }

		if (layer_control & 0x01) DrvTileRAM[offs | 0x0000] = data;
		if (layer_control & 0x02) DrvCharRAM[offs | 0x0000] = data;
		if (layer_control & 0x04) DrvCharRAM[offs | 0x0800] = data;
		if (layer_control & 0x08) DrvCharRAM[offs | 0x1000] = data;
		if (layer_control & 0x10) DrvCharRAM[offs | 0x1800] = data;
	}
}

static void __fastcall tiamc1_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xf0)
	{
		case 0x40:
		case 0x50:
		case 0x60:
		case 0x70:
			DrvSprRAM[port & 0x3f] = ~data;
		return;

		case 0xc0:
			if ((port & 0x0f) < 4) tiamc1_sound_timer0_write(port & 3, data);
		return;

		case 0xd0:
			switch (port & 0x0c)
			{
				case 0x00:
					ppi8255_w(0, port & 3, data);
				return;

				case 0x04:
					tiamc1_sound_timer1_write(port & 3, data);
				return;

				case 0x08: // actually A
					tiamc1_sound_gate_write(data);
				return;
			}
		return;

		case 0xa0:
			BurnPalRAM[port & 0xf] = data;
			update_colors = 1;
		return;

		case 0xb0:
		{
			switch (port & 0x0f)
			{
				case 0x0c:
					scrolly = data;
				return;

				case 0x0d:
					scrollx = data;
				return;

				case 0x0e:
					layer_control = data ^ 0x1f;
				return;

				case 0x0f:
					bg_color = ((data >> 0) & 1) | ((data >> 1) & 2) | ((data >> 2) & 4) | ((data >> 3) & 8);
					update_colors = 1;
				return;
			}
		}
		return;
	}
}

static UINT8 __fastcall tiamc1_read_port(UINT16 port)
{
	switch (port & 0xfc)
	{
		case 0xd0:
			return ppi8255_r(0, port & 3);
	}

	return 0;
}

static void __fastcall kot_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xf0)
	{
		case 0x00:
		case 0x10:
		case 0x20:
		case 0x30:
			DrvSprRAM[port & 0x3f] = ~data;
		return;

		case 0xc0:
			if ((port & 0x0f) < 4) tiamc1_sound_timer0_write(port & 3, data);
		return;

		case 0xd0:
			if ((port & 0x0f) < 4) ppi8255_w(0, port & 3, data);
		return;

		case 0xe0:
			BurnPalRAM[port & 0xf] = data;
			update_colors = 1;
		return;

		case 0xf0:
		{
			switch (port & 0x0f)
			{
				case 0x00:
					scrolly = data;
				return;

				case 0x04:
					scrollx = data;
				return;

				case 0x08:
					layer_control = (data ^ 1) & 1; // kot != tiamc1
					character_bank = (data >> 1) << 5;
				return;
			}
		}
		return;
	}
}

static UINT8 __fastcall kot_read_port(UINT16 port)
{
	switch (port & 0xfc)
	{
		case 0xc0:		// pit8253
			return 0xff;

		case 0xd0:
			return ppi8255_r(0, port & 3);
	}

	return 0;
}

static UINT8 ppi_port_A_read()
{
	return DrvInputs[0];
}

static UINT8 ppi_port_B_read()
{
	return DrvInputs[1];
}

static UINT8 ppi_port_C_read()
{
	return (DrvInputs[2] & 0x7f) | (vblank ? 0x80 : 0);
}

static tilemap_callback( bg )
{
	TILE_SET_INFO(0, DrvTileRAM[offs | ((layer_control & 0x80) << 3)] + character_bank, 0, 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	ppi8255_reset();

	layer_control = 0;
	character_bank = 0;
	scrollx = 0;
	scrolly = 0;
	bg_color = 0;
	update_characters = 0;
	update_colors = 0;

	nExtraCycles = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvI8080ROM		= Next; Next += 0x010000;

	DrvTileROM		= Next; Next += 0x010000;
	DrvSprROM		= Next; Next += 0x010000;

	DrvPalette		= (UINT32*)Next; Next += 0x0100 * sizeof(UINT32);
	BurnPalette		= (UINT32*)Next; Next += 0x0020 * sizeof(UINT32);

	AllRam			= Next;

	DrvI8080RAM		= Next; Next += 0x002000;

	DrvTileRAM		= Next; Next += 0x000800;

	DrvSprRAM       = Next; Next += 0x000040;
	DrvCharRAM		= Next; Next += 0x002000;
	BurnPalRAM		= Next; Next += 0x000010;

	RamEnd			= Next;

	DrvCharRAMExp	= Next; Next += 0x004000;

	MemEnd			= Next;

	return 0;
}

static void DrvGfxDecode()
{
	INT32 Planes[4]  = { STEP4(0x10000*3,-0x10000) };
	INT32 XOffs[16]  = { STEP8(0,1), STEP8(0x8000,1) };
	INT32 YOffs[16]  = { STEP16(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x8000);
	if (tmp == NULL) {
		return;
	}

	memcpy (tmp, DrvTileROM, 0x8000);

	GfxDecode(0x0400, 4,  8,  8, Planes, XOffs, YOffs, 0x40, tmp, DrvTileROM);

	memcpy (tmp, DrvSprROM,  0x8000);

	GfxDecode(0x0100, 4, 16, 16, Planes, XOffs, YOffs, 0x80, tmp, DrvSprROM);

	BurnFree (tmp);
}

static void DrvPaletteInit()
{
	static float g_v[8] = { 1.2071F, 0.9971F, 0.9259F, 0.7159F, 0.4912F, 0.2812F, 0.2100F, 0.0000F };
	static float r_v[8] = { 1.5937F, 1.3125F, 1.1562F, 0.8750F, 0.7187F, 0.4375F, 0.2812F, 0.0000F };
	static float b_v[4] = { 1.3523F, 0.8750F, 0.4773F, 0.0000F };

	for (INT32 col = 0; col < 256; col++)
	{
		INT32 ir = (col >> 3) & 7;
		INT32 ig = col & 7;
		INT32 ib = (col >> 6) & 3;
		float tcol = 255.0f * r_v[ir] / r_v[0];
		INT32 r = 255 - (int(tcol) & 255);
		tcol = 255.0f * g_v[ig] / g_v[0];
		INT32 g = 255 - (int(tcol) & 255);
		tcol = 255.0f * b_v[ib] / b_v[0];
		INT32 b = 255 - (int(tcol) & 255);

		DrvPalette[col] = (r << 16) | (g << 8) | b;
	}
}

static INT32 DrvInit(INT32 romcount)
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (romcount == 6)
		{
			if (BurnLoadRom(DrvI8080ROM + 0x00000, k++, 1)) return 1;
			if (BurnLoadRom(DrvI8080ROM + 0x02000, k++, 1)) return 1;
			if (BurnLoadRom(DrvI8080ROM + 0x04000, k++, 1)) return 1;
			if (BurnLoadRom(DrvI8080ROM + 0x06000, k++, 1)) return 1;
			if (BurnLoadRom(DrvI8080ROM + 0x08000, k++, 1)) return 1;
			if (BurnLoadRom(DrvI8080ROM + 0x0c000, k++, 1)) return 1;
		}
		else if (romcount == 4)
		{
			if (BurnLoadRom(DrvI8080ROM + 0x00000, k++, 1)) return 1;
			if (BurnLoadRom(DrvI8080ROM + 0x02000, k++, 1)) return 1;
			if (BurnLoadRom(DrvI8080ROM + 0x04000, k++, 1)) return 1;
			if (BurnLoadRom(DrvI8080ROM + 0x06000, k++, 1)) return 1;
		}
		else if (romcount == 3)
		{
			if (BurnLoadRom(DrvI8080ROM + 0x00000, k++, 1)) return 1;
			if (BurnLoadRom(DrvI8080ROM + 0x02000, k++, 1)) return 1;
			if (BurnLoadRom(DrvI8080ROM + 0x0c000, k++, 1)) return 1;
		}

		if (BurnLoadRom(DrvSprROM   + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x02000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x04000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x06000, k++, 1)) return 1;

		DrvGfxDecode();
		DrvPaletteInit();
	}

	ZetInit(0); // i8080
	ZetOpen(0);
	ZetMapMemory(DrvI8080ROM,	0x0000, 0xdfff, MAP_ROM);
	ZetMapMemory(DrvI8080RAM,	0xe000, 0xffff, MAP_RAM);
	ZetSetWriteHandler(tiamc1_videoram_write);
	ZetSetOutHandler(tiamc1_write_port);
	ZetSetInHandler(tiamc1_read_port);
	ZetClose();

	ppi8255_init(1);
	ppi8255_set_read_ports(0, ppi_port_A_read, ppi_port_B_read, ppi_port_C_read);
	// write C = coin counter

	tiamc1_sound_init();

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 32, 32);
	GenericTilesSetGfx(0, DrvCharRAMExp, 4,   8,   8, 0x004000, 0x10, 0x0f);
	GenericTilesSetGfx(1, DrvSprROM,     4,  16,  16, 0x010000, 0x00, 0x0f);
	GenericTilemapSetOffsets(0, 4, 0);

	button_config = 0x0a0000;

	DrvDoReset();

	return 0;
}

static INT32 KotInit()
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvI8080ROM + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvI8080ROM + 0x02000, k++, 1)) return 1;
		if (BurnLoadRom(DrvI8080ROM + 0x04000, k++, 1)) return 1;

		if (BurnLoadRom(DrvSprROM   + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x02000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x04000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x06000, k++, 1)) return 1;

		if (BurnLoadRom(DrvTileROM  + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvTileROM  + 0x02000, k++, 1)) return 1;
		if (BurnLoadRom(DrvTileROM  + 0x04000, k++, 1)) return 1;
		if (BurnLoadRom(DrvTileROM  + 0x06000, k++, 1)) return 1;

		DrvGfxDecode();
		DrvPaletteInit();
	}

	ZetInit(0); // i8080
	ZetOpen(0);
	ZetMapMemory(DrvI8080ROM,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvI8080RAM,	0xc000, 0xcfff, MAP_RAM);
	ZetSetWriteHandler(tiamc1_videoram_write);
	ZetSetOutHandler(kot_write_port);
	ZetSetInHandler(kot_read_port);
	ZetClose();

	ppi8255_init(1);
	ppi8255_set_read_ports(0, ppi_port_A_read, ppi_port_B_read, ppi_port_C_read);
	// write C = coin counter

	tiamc1_sound_init_kot();

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 32, 32);
	GenericTilesSetGfx(0, DrvTileROM, 4,   8,   8, 0x010000, 0x10, 0x0f);
	GenericTilesSetGfx(1, DrvSprROM,  4,  16,  16, 0x010000, 0x00, 0x0f);
	GenericTilemapSetOffsets(0, 4, 0);

	button_config = 0x6affff;

	is_kot = 1;

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	ppi8255_exit();

	tiamc1_sound_exit();

	BurnFreeMemIndex();

	is_gorodki = 0;
	is_kot = 0;

	return 0;
}

static void DrvCharDecode()
{
	INT32 Planes[4] = { STEP4(0x4000*3,-0x4000) };
	INT32 XOffs[8]  = { STEP8(0,1) };
	INT32 YOffs[8]  = { STEP8(0,8) };

	GfxDecode(0x0100, 4, 8, 8, Planes, XOffs, YOffs, 0x40, DrvCharRAM, DrvCharRAMExp);
}

static void DrawSprites()
{
	for (INT32 offs = 0; offs < 16; offs++)
	{
		if (DrvSprRAM[offs | 0x30] & 1)
		{
			INT32 sy    = DrvSprRAM[offs | 0x00];
			INT32 sx    = DrvSprRAM[offs | 0x10];
			INT32 code  = DrvSprRAM[offs | 0x20];
			INT32 flipx = DrvSprRAM[offs | 0x30] & 0x08;
			INT32 flipy = DrvSprRAM[offs | 0x30] & 0x02;

			DrawGfxMaskTile(0, 1, code, sx, sy, flipx, flipy, 0, 0xf);
		}
	}
}

static INT32 DrvDraw()
{
	if (update_colors || BurnRecalc) {
		for (INT32 i = 0; i < 16; i++) {
			UINT32 p = DrvPalette[BurnPalRAM[i]];
			UINT32 b = DrvPalette[BurnPalRAM[i | bg_color]];
			BurnPalette[i + 0x00] = BurnHighCol(p >> 16, (p >> 8) & 0xff, p & 0xff, 0);
			BurnPalette[i + 0x10] = BurnHighCol(b >> 16, (b >> 8) & 0xff, b & 0xff, 0);
		}
		update_colors = BurnRecalc = 0;
	}

	if (update_characters) {
		DrvCharDecode();
		update_characters = 0;
	}

	GenericTilemapSetScrollX(0, scrollx);
	GenericTilemapSetScrollY(0, scrolly);

	GenericTilemapDraw(0, pTransDraw, 0);

	DrawSprites();

	BurnTransferCopy(BurnPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();

	{
		for (INT32 i = 0; i < 3; i++) {
			DrvInputs[i] = button_config >> (i * 8);
		}

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		if (is_gorodki) {
			DrvInputs[0] = ProcessAnalog(Analog[0], 1, 1, 0x60, 0xd0);
		}
	}

	INT32 nInterleave = 312;
	INT32 nCyclesTotal[1] = { 1750000 / 60 };
	INT32 nCyclesDone[1] = { nExtraCycles };

	vblank = 1;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		CPU_RUN(0, Zet);
		ZetClose();

		if (i == 56) vblank = 0;
	}

	nExtraCycles = nCyclesDone[0] - nCyclesTotal[0];

	if (pBurnSoundOut) {
		tiamc1_sound_update(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
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

		ppi8255_scan();

		tiamc1_sound_scan(nAction, pnMin);

		SCAN_VAR(layer_control);
		SCAN_VAR(character_bank);
		SCAN_VAR(scrollx);
		SCAN_VAR(scrolly);
		SCAN_VAR(bg_color);
		SCAN_VAR(update_characters);
		SCAN_VAR(update_colors);

		SCAN_VAR(nExtraCycles);
	}

	if (nAction & ACB_WRITE && is_kot == 0) {
		update_characters = 1;
	}

	return 0;
}


// Konek-Gorbunok

static struct BurnRomInfo konekRomDesc[] = {
	{ "g1.d17",			0x2000, 0xf41d82c9, 1 | BRF_PRG | BRF_ESS }, //  0 i8080 Code
	{ "g2.d17",			0x2000, 0xb44e7491, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "g3.d17",			0x2000, 0x91301282, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "g4.d17",			0x2000, 0x3ff0c20b, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "g5.d17",			0x2000, 0xe3196d30, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "g7.d17",			0x2000, 0xfe4e9fdd, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "a2.b07",			0x2000, 0x9eed06ee, 2 | BRF_GRA },           //  6 Sprites
	{ "a3.g07",			0x2000, 0xeeff9b77, 2 | BRF_GRA },           //  7
	{ "a5.l07",			0x2000, 0xfff9e089, 2 | BRF_GRA },           //  8
	{ "a6.r07",			0x2000, 0x092e8ee2, 2 | BRF_GRA },           //  9

	{ "prom100.e10",	0x0100, 0x00000000, 3 | BRF_NODUMP | BRF_GRA },           // 10 PROMs
	{ "prom101.a01",	0x0100, 0x00000000, 3 | BRF_NODUMP | BRF_GRA },           // 11
	{ "prom102.b03",	0x0080, 0x00000000, 3 | BRF_NODUMP | BRF_GRA },           // 12
	{ "prom102.b06",	0x0080, 0x00000000, 3 | BRF_NODUMP | BRF_GRA },           // 13
	{ "prom102.b05",	0x0100, 0x00000000, 3 | BRF_NODUMP | BRF_GRA },           // 14
};

STD_ROM_PICK(konek)
STD_ROM_FN(konek)

static INT32 KonekInit()
{
	return DrvInit(6);
}

struct BurnDriver BurnDrvKonek = {
	"konek", NULL, NULL, NULL, "198?",
	"Konek-Gorbunok\0", NULL, "Terminal", "TIA-MC1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, konekRomInfo, konekRomName, NULL, NULL, NULL, NULL, DrvInputInfo, NULL,
	KonekInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x20,
	256, 256, 4, 3
};


// S.O.S.

static struct BurnRomInfo sostermRomDesc[] = {
	{ "04.1g",			0x2000, 0xd588081e, 1 | BRF_PRG | BRF_ESS }, //  0 i8080 Code
	{ "05.2g",			0x2000, 0xb44e7491, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "06.3g",			0x2000, 0x34dacde6, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "07.4g",			0x2000, 0x9f6f8cdd, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "08.5g",			0x2000, 0x25e70da4, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "10.7g",			0x2000, 0x22bc9997, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "00.2a",			0x2000, 0xa1c7f07a, 2 | BRF_GRA },           //  6 Sprites
	{ "01.3a",			0x2000, 0x788b4036, 2 | BRF_GRA },           //  7
	{ "02.5a",			0x2000, 0x9506cf9b, 2 | BRF_GRA },           //  8
	{ "03.6a",			0x2000, 0x5a0c14e1, 2 | BRF_GRA },           //  9

	{ "prom100.e10",	0x0100, 0x00000000, 3 | BRF_NODUMP | BRF_GRA },           // 10 PROMs
	{ "prom101.a01",	0x0100, 0x00000000, 3 | BRF_NODUMP | BRF_GRA },           // 11
	{ "prom102.b03",	0x0080, 0x00000000, 3 | BRF_NODUMP | BRF_GRA },           // 12
	{ "prom102.b06",	0x0080, 0x00000000, 3 | BRF_NODUMP | BRF_GRA },           // 13
	{ "prom102.b05",	0x0100, 0x00000000, 3 | BRF_NODUMP | BRF_GRA },           // 14
};

STD_ROM_PICK(sosterm)
STD_ROM_FN(sosterm)

struct BurnDriver BurnDrvSosterm = {
	"sosterm", NULL, NULL, NULL, "198?",
	"S.O.S.\0", NULL, "Terminal", "TIA-MC1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, sostermRomInfo, sostermRomName, NULL, NULL, NULL, NULL, DrvInputInfo, NULL,
	KonekInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x20,
	256, 256, 4, 3
};


// Snezhnaja Koroleva

static struct BurnRomInfo korolevaRomDesc[] = {
	{ "04.1g",			0x2000, 0xc3701225, 1 | BRF_PRG | BRF_ESS }, //  0 i8080 Code
	{ "05.2g",			0x2000, 0x1b3742ce, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "06.3g",			0x2000, 0x48074786, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "07.4g",			0x2000, 0x41a4adb5, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "08.5g",			0x2000, 0x8f379d95, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "10.7g",			0x2000, 0x397f41f8, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "00.2a",			0x2000, 0x6f39f8be, 2 | BRF_GRA },           //  6 Sprites
	{ "01.3a",			0x2000, 0x7bdfdd19, 2 | BRF_GRA },           //  7
	{ "02.5a",			0x2000, 0x97770b0f, 2 | BRF_GRA },           //  8
	{ "03.6a",			0x2000, 0x9b0a686a, 2 | BRF_GRA },           //  9

	{ "prom100.e10",	0x0100, 0x00000000, 3 | BRF_NODUMP | BRF_GRA },           // 10 PROMs
	{ "prom101.a01",	0x0100, 0x00000000, 3 | BRF_NODUMP | BRF_GRA },           // 11
	{ "prom102.b03",	0x0080, 0x00000000, 3 | BRF_NODUMP | BRF_GRA },           // 12
	{ "prom102.b06",	0x0080, 0x00000000, 3 | BRF_NODUMP | BRF_GRA },           // 13
	{ "prom102.b05",	0x0100, 0x00000000, 3 | BRF_NODUMP | BRF_GRA },           // 14
};

STD_ROM_PICK(koroleva)
STD_ROM_FN(koroleva)

struct BurnDriver BurnDrvKoroleva = {
	"koroleva", NULL, NULL, NULL, "198?",
	"Snezhnaja Koroleva\0", NULL, "Terminal", "TIA-MC1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, korolevaRomInfo, korolevaRomName, NULL, NULL, NULL, NULL, DrvInputInfo, NULL,
	KonekInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x20,
	256, 256, 4, 3
};


// Billiard

static struct BurnRomInfo bilyardRomDesc[] = {
	{ "04.1g",			0x2000, 0xa44f913d, 1 | BRF_PRG | BRF_ESS }, //  0 i8080 Code
	{ "05.2g",			0x2000, 0x6e41219f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "10.7g",			0x2000, 0x173adb85, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "00.2a",			0x2000, 0x6f72e043, 2 | BRF_GRA },           //  3 Sprites
	{ "01.3a",			0x2000, 0xdaddbbb5, 2 | BRF_GRA },           //  4
	{ "02.5a",			0x2000, 0x3d744d33, 2 | BRF_GRA },           //  5
	{ "03.6a",			0x2000, 0x8bfc0b15, 2 | BRF_GRA },           //  6

	{ "prom100.e10",	0x0100, 0x00000000, 3 | BRF_NODUMP | BRF_GRA },           //  7 PROMs
	{ "prom101.a01",	0x0100, 0x00000000, 3 | BRF_NODUMP | BRF_GRA },           //  8
	{ "prom102.b03",	0x0080, 0x00000000, 3 | BRF_NODUMP | BRF_GRA },           //  9
	{ "prom102.b06",	0x0080, 0x00000000, 3 | BRF_NODUMP | BRF_GRA },           // 10
	{ "prom102.b05",	0x0100, 0x00000000, 3 | BRF_NODUMP | BRF_GRA },           // 11
};

STD_ROM_PICK(bilyard)
STD_ROM_FN(bilyard)

static INT32 BilyardInit()
{
	return DrvInit(3);
}

struct BurnDriver BurnDrvBilyard = {
	"bilyard", NULL, NULL, NULL, "198?",
	"Billiard\0", NULL, "Terminal", "TIA-MC1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, bilyardRomInfo, bilyardRomName, NULL, NULL, NULL, NULL, DrvInputInfo, NULL,
	BilyardInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x20,
	256, 256, 4, 3
};


// Gorodki

static struct BurnRomInfo gorodkiRomDesc[] = {
	{ "70.1g",			0x2000, 0xbd3eb624, 1 | BRF_PRG | BRF_ESS }, //  0 i8080 Code
	{ "71.2g",			0x2000, 0x5a9ebd8d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "72.3g",			0x2000, 0xedcc5c13, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "73.4g",			0x2000, 0xae69b9f3, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "66.2a",			0x2000, 0xb3dd4dec, 2 | BRF_GRA },           //  4 Sprites
	{ "67.3a",			0x2000, 0xc94f5579, 2 | BRF_GRA },           //  5
	{ "68.5a",			0x2000, 0x0d64708d, 2 | BRF_GRA },           //  6
	{ "69.6a",			0x2000, 0x57c8ae81, 2 | BRF_GRA },           //  7

	{ "prom100.e10",	0x0100, 0x00000000, 3 | BRF_NODUMP | BRF_GRA },           //  8 PROMs
	{ "prom101.a01",	0x0100, 0x00000000, 3 | BRF_NODUMP | BRF_GRA },           //  9
	{ "prom102.b03",	0x0080, 0x00000000, 3 | BRF_NODUMP | BRF_GRA },           // 10
	{ "prom102.b06",	0x0080, 0x00000000, 3 | BRF_NODUMP | BRF_GRA },           // 11
	{ "prom102.b05",	0x0100, 0x00000000, 3 | BRF_NODUMP | BRF_GRA },           // 12
};

STD_ROM_PICK(gorodki)
STD_ROM_FN(gorodki)

static INT32 GorodkiInit()
{
	is_gorodki = 1;

	return DrvInit(4);
}

struct BurnDriver BurnDrvGorodki = {
	"gorodki", NULL, NULL, NULL, "198?",
	"Gorodki\0", NULL, "Terminal", "TIA-MC1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, gorodkiRomInfo, gorodkiRomName, NULL, NULL, NULL, NULL, GorodkiInputInfo, NULL, // wrong input!
	GorodkiInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x20,
	256, 256, 4, 3
};


// Kot-Rybolov (Terminal)

static struct BurnRomInfo kotRomDesc[] = {
	{ "854.6",			0x2000, 0x44e5e8fc, 1 | BRF_PRG | BRF_ESS }, //  0 i8080 Code
	{ "855.7",			0x2000, 0x0bb2e4b2, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "856.8",			0x2000, 0x9180c98f, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "850.5",			0x2000, 0x5dc3a102, 2 | BRF_GRA },           //  3 Sprites
	{ "851.6",			0x2000, 0x7db239a0, 2 | BRF_GRA },           //  4
	{ "852.7",			0x2000, 0xc7700f88, 2 | BRF_GRA },           //  5
	{ "853.8",			0x2000, 0xb94bf1af, 2 | BRF_GRA },           //  6

	{ "846.1",			0x2000, 0x42447f4a, 3 | BRF_GRA },           //  7 Tiles
	{ "847.2",			0x2000, 0x99ada5e8, 3 | BRF_GRA },           //  8
	{ "848.3",			0x2000, 0xa124cff4, 3 | BRF_GRA },           //  9
	{ "849.4",			0x2000, 0x5d27fda6, 3 | BRF_GRA },           // 10
};

STD_ROM_PICK(kot)
STD_ROM_FN(kot)

struct BurnDriver BurnDrvKot = {
	"kot", NULL, NULL, NULL, "198?",
	"Kot-Rybolov (Terminal)\0", NULL, "Terminal", "TIA-MC1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_MISC_PRE90S, GBF_VSFIGHT, 0,
	NULL, kotRomInfo, kotRomName, NULL, NULL, NULL, NULL, DrvInputInfo, NULL,
	KotInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x20,
	256, 256, 4, 3
};
