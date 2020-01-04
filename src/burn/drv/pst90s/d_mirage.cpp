// FB Alpha Mirage Youjuu Mahjongden  driver module
// Based on MAME driver by Angelo Salese

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "deco16ic.h"
#include "msm6295.h"
#include "eeprom.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvSndROM[2];
static UINT8 *Drv68KRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprBuf;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 flipscreen;

static UINT16 mux_data;
static UINT8 oki_banks[2];

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvJoy5[8];
static UINT8 DrvJoy6[8];
static UINT8 DrvReset;
static UINT16 DrvInputs[6];

static struct BurnInputInfo MirageInputList[] = {
	{"Coin 1",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 coin"	},
	{"Coin 2",		BIT_DIGITAL,	DrvJoy1 + 2,	"p2 coin"	},
	{"P1 Start",	BIT_DIGITAL,	DrvJoy2 + 5,	"p1 start"	},
	{"A",			BIT_DIGITAL,	DrvJoy2 + 0,	"mah a"		},
	{"B",			BIT_DIGITAL,	DrvJoy5 + 0,	"mah b"		},
	{"C",			BIT_DIGITAL,	DrvJoy3 + 0,	"mah c"		},
	{"D",			BIT_DIGITAL,	DrvJoy6 + 0,	"mah d"		},
	{"E",			BIT_DIGITAL,	DrvJoy2 + 1,	"mah e"		},
	{"F",			BIT_DIGITAL,	DrvJoy4 + 3,	"mah f"		},
	{"G",			BIT_DIGITAL,	DrvJoy3 + 1,	"mah g"		},
	{"H",			BIT_DIGITAL,	DrvJoy6 + 1,	"mah h"		},
	{"I",			BIT_DIGITAL,	DrvJoy2 + 2,	"mah i"		},
	{"J",			BIT_DIGITAL,	DrvJoy5 + 2,	"mah j"		},
	{"K",			BIT_DIGITAL,	DrvJoy2 + 4,	"mah k"		},
	{"L",			BIT_DIGITAL,	DrvJoy6 + 2,	"mah l"		},
	{"M",			BIT_DIGITAL,	DrvJoy2 + 3,	"mah m"		},
	{"N",			BIT_DIGITAL,	DrvJoy5 + 3,	"mah n"		},
	{"Pon",			BIT_DIGITAL,	DrvJoy6 + 3,	"mah pon"	},
	{"Chi",			BIT_DIGITAL,	DrvJoy3 + 3,	"mah chi"	},
	{"Kan",			BIT_DIGITAL,	DrvJoy2 + 4,	"mah kan"	},
	{"Ron",			BIT_DIGITAL,	DrvJoy3 + 4,	"mah ron"	},
	{"Reach",		BIT_DIGITAL,	DrvJoy5 + 4,	"mah reach"	},
	{"Flip Flip",	BIT_DIGITAL,	DrvJoy4 + 3,	"mah ff"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 0,	"service"	},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 3,	"diag"		},
};

STDINPUTINFO(Mirage)

static void oki_bankswitch(INT32 chip, UINT16 data)
{
	oki_banks[chip] = data;

	MSM6295SetBank(chip, DrvSndROM[chip] + (data * 0x40000), 0, 0x3ffff);
}

static void __fastcall mirage_write_word(UINT32 address, UINT16 data)
{
	deco16_write_control_word(0, address, 0x168000, data)

	if ((address & 0xfffff0) == 0x168000) {
		deco16_pf_control[1][(address / 2) & 7] = data;
		return;
	}

	if ((address & 0xfffff0) == 0x140000) { // sfx
		MSM6295Write(1, data & 0xff); // ?
		return;
	}

	if ((address & 0xfffff0) == 0x150000) { // obj
		MSM6295Write(0, data & 0xff); // ?
		return;
	}

	switch (address)
	{
		case 0x160000:
		case 0x160001:
		case 0x16a000:
		case 0x16a001:
		case 0x16e000:
		case 0x16e001:
		return; // nop

		case 0x16c000:
		case 0x16c001:
			oki_bankswitch(1, data & 3);
		return;

		case 0x16c002:
		case 0x16c003:
			oki_bankswitch(0, data & 7);
			EEPROMSetClockLine(data & 0x20 ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
			EEPROMWriteBit((data >> 4) & 0x01);
			EEPROMSetCSLine(data & 0x40 ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
		return;

		case 0x16c004:
		case 0x16c005:
			mux_data = data & 0x1f;
		return;
	}
}

static void __fastcall mirage_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xfffff0) == 0x168000) {
	//	deco16_pf_control[1][(address / 2) & 7] = data;
		return;
	}

	if ((address & 0xfffff0) == 0x140000) { // sfx
		MSM6295Write(1, data); // ?
		return;
	}

	if ((address & 0xfffff0) == 0x150000) { // obj
		MSM6295Write(0, data); // ?
		return;
	}

	switch (address)
	{
		case 0x160000:
		case 0x160001:
		case 0x16a000:
		case 0x16a001:
		case 0x16e000:
		case 0x16e001:
		return; // nop

		case 0x16c000:
		case 0x16c001:
			oki_bankswitch(1, data & 3);
		return;

		case 0x16c002:
		case 0x16c003:
			oki_bankswitch(0, data & 7);
			EEPROMSetClockLine(data & 0x20 ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
			EEPROMWriteBit((data >> 4) & 0x01);
			EEPROMSetCSLine(data & 0x40 ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
		return;

		case 0x16c004:
		case 0x16c005:
			mux_data = data & 0x1f;
		return;
	}
}

static UINT16 __fastcall mirage_read_word(UINT32 address)
{
	if ((address & 0xfffff0) == 0x140000) { // sfx
		return MSM6295Read(1);
	}

	if ((address & 0xfffff0) == 0x150000) { // obj
		return MSM6295Read(0);
	}

	switch (address)
	{
		case 0x16c006:
		case 0x16c007:
			for (INT32 i = 0; i < 5; i++) {
				if (mux_data & (1 << i)) return DrvInputs[1 + i];
			}
			return 0xff;

		case 0x16e002:
		case 0x16e003:
			return (DrvInputs[0] & ~0x30) | (deco16_vblank ? 0x10 : 0) | (EEPROMRead() ? 0x20 : 0);
	}

	return 0;
}

static UINT8 __fastcall mirage_read_byte(UINT32 address)
{
	if ((address & 0xfffff0) == 0x140000) { // sfx
		return MSM6295Read(1);
	}

	if ((address & 0xfffff0) == 0x150000) { // obj
		return MSM6295Read(0);
	}

	switch (address)
	{
		case 0x16c006:
		case 0x16c007:
			for (INT32 i = 0; i < 5; i++) {
				if (mux_data & (1 << i)) return DrvInputs[1 + i];
			}
			return ~0;

		case 0x16e002:
			return 0xff;

		case 0x16e003:
			return (DrvInputs[0] & ~0x30) | (deco16_vblank ? 0x10 : 0) | (EEPROMRead() ? 0x20 : 0);
	}

	return 0;
}

static INT32 bank_callback(const INT32 bank)
{
	return (bank & 0x70) * 0x100;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	EEPROMReset();

	oki_bankswitch(0,0);
	oki_bankswitch(1,0);
	MSM6295Reset();

	deco16Reset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x080000;

	DrvGfxROM0		= Next; Next += 0x200000;
	DrvGfxROM1		= Next; Next += 0x200000;
	DrvGfxROM2		= Next; Next += 0x800000;

	DrvSndROM[0]	= Next; Next += 0x200000;
	DrvSndROM[1]	= Next; Next += 0x100000;

	DrvPalette		= (UINT32*)Next; Next += 0x0400 * sizeof(UINT32);

	AllRam			= Next;

	Drv68KRAM		= Next; Next += 0x004000;
	DrvSprRAM		= Next; Next += 0x000800;
	DrvPalRAM		= Next; Next += 0x000800;
	DrvSprBuf		= Next; Next += 0x000800;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvInit()
{
	BurnSetRefreshRate(58.00);

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv68KROM    + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM    + 0x000000,  1, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM1   + 0x000000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2   + 0x000001,  3, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2   + 0x000000,  4, 2)) return 1;

		if (BurnLoadRom(DrvSndROM[0] + 0x000000,  5, 1)) return 1;

		if (BurnLoadRom(DrvSndROM[1] + 0x000000,  6, 1)) return 1;

		for (INT32 i = 0; i < 0x80000; i++) { // swap middle 512k chunks
			INT32 t = DrvSndROM[0][0x080000 + i];
			DrvSndROM[0][0x080000 + i] = DrvSndROM[0][0x100000 + i];
			DrvSndROM[0][0x100000 + i] = t;
		}

		deco56_decrypt_gfx(DrvGfxROM1, 0x100000);
		deco16_tile_decode(DrvGfxROM1, DrvGfxROM0, 0x100000, 1);
		deco16_tile_decode(DrvGfxROM1, DrvGfxROM1, 0x100000, 0);
		deco16_sprite_decode(DrvGfxROM2, 0x400000);
	}

	deco16Init(1, 0, 1);
	deco16_set_graphics(DrvGfxROM0, 0x200000, DrvGfxROM1, 0x200000, DrvGfxROM1, 0x200000);
	deco16_set_transparency_mask(0, 0xf);
	deco16_set_transparency_mask(1, 0xf);
	deco16_set_bank_callback(0, bank_callback);
	deco16_set_bank_callback(1, bank_callback);
	deco16_set_global_offsets(0, 8);

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,					0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(deco16_pf_ram[0],			0x100000, 0x101fff, MAP_RAM);
	SekMapMemory(deco16_pf_ram[1],			0x102000, 0x103fff, MAP_RAM);
	SekMapMemory(deco16_pf_rowscroll[0],	0x110000, 0x110bff, MAP_RAM);
	SekMapMemory(deco16_pf_rowscroll[1],	0x112000, 0x112bff, MAP_RAM);
	SekMapMemory(DrvSprRAM,					0x120000, 0x1207ff, MAP_RAM);
	SekMapMemory(DrvPalRAM,					0x130000, 0x1307ff, MAP_RAM);
	SekMapMemory(Drv68KRAM,					0x170000, 0x173fff, MAP_RAM);
	SekSetWriteWordHandler(0,				mirage_write_word);
	SekSetWriteByteHandler(0,				mirage_write_byte);
	SekSetReadWordHandler(0,				mirage_read_word);
	SekSetReadByteHandler(0,				mirage_read_byte);
	SekClose();

	EEPROMInit(&eeprom_interface_93C46);

	MSM6295Init(0, 2000000 / 132, 0);
	MSM6295Init(1, 1000000 / 132, 1);
	MSM6295SetRoute(0, 0.30, BURN_SND_ROUTE_BOTH);
	MSM6295SetRoute(1, 0.70, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	deco16Exit();

	MSM6295Exit();

	EEPROMExit();

	SekExit();

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteUpdate()
{
	UINT16 *p = (UINT16*)DrvPalRAM;
	
	for (INT32 i = 0; i < 0x800/2; i++)
	{
		INT32 r = (p[i] >>  0) & 0x1f;
		INT32 g = (p[i] >>  5) & 0x1f;
		INT32 b = (p[i] >> 10) & 0x1f;

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);
		
		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_sprites()
{
	UINT16 *spriteram = (UINT16*)DrvSprRAM;

	for (INT32 offs = 0x400-4; offs >= 0; offs -= 4)
	{
		INT32 inc, mult;
		INT32 y = spriteram[offs];
		INT32 flash = y & 0x1000;

		if (!(flash && (nCurrentFrame & 1)))
		{
			INT32 sprite = spriteram[offs + 1] & 0x7fff;
			INT32 w = y & 0x0800;
			INT32 x = spriteram[offs + 2];

			INT32 colour = (x >> 9) & 0x3f;

			INT32 pri = (y >> 15) & 1;
			if (pri == 0) pri = 0; // over everything
			if (pri == 1) pri = 2; // under text

			INT32 fx = y & 0x2000;
			INT32 fy = y & 0x4000;

			INT32 multi = (1 << ((y & 0x0600) >> 9)) - 1;

			x &= 0x1ff;
			y &= 0x1ff;
			if (x >= 320) x -= 512;
			if (y >= 256) y -= 512;
			y = 240 - y;
			x = 304 - x;

			sprite &= ~multi;

			if (fy)
				inc = -1;
			else
			{
				sprite += multi;
				inc = 1;
			}

			if (flipscreen)
			{
				y = 240 - y;
				if (fy) fy = 0; else fy = 1;
				mult = 16;
				x = 304 - x;
				if (fx) fx = 0; else fx = 1;
			}
			else
				mult = -16;

			INT32 mult2 = multi + 1;

			while (multi >= 0)
			{
				INT32 ypos = y + mult * multi;

				deco16_draw_prio_sprite(pTransDraw, DrvGfxROM2, sprite - multi * inc, (colour * 16) + 0x200, x, ypos, fx, fy, pri, -1);

				if (w)
				{
					deco16_draw_prio_sprite(pTransDraw, DrvGfxROM2, (sprite - multi * inc) - mult2, (colour * 16) + 0x200, !flipscreen ? x - 16 : x + 16, ypos, fx, fy, pri, -1);
				}

				multi--;
			}
		}
	}
}

static INT32 DrvDraw()
{
//	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
//	}

 	flipscreen = deco16_pf_control[1][0] & 0x80;

	deco16_pf12_update();

	if ((nBurnLayer & 1) == 0) BurnTransferClear(0x100);

	if (nBurnLayer & 1) deco16_draw_layer(1, pTransDraw, 0 | DECO16_LAYER_OPAQUE);

	if (nBurnLayer & 4) deco16_draw_layer(0, pTransDraw, 1);

	if (nSpriteEnable & 1) draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, 6 * sizeof(UINT16));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
			DrvInputs[4] ^= (DrvJoy5[i] & 1) << i;
			DrvInputs[5] ^= (DrvJoy6[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[1] = { 14000000 / 58 };
	INT32 nCyclesDone[1] = { 0 };

	SekOpen(0);

	deco16_vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone[0] += SekRun(nCyclesTotal[0] / nInterleave);

		if (i == 240) {
			deco16_vblank = 0x08;
			SekSetIRQLine(6, CPU_IRQSTATUS_AUTO);
		}
	}

	SekClose();

	if (pBurnSoundOut) {
		BurnSoundClear();
		MSM6295Render(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	memcpy (DrvSprBuf, DrvSprRAM, 0x800);

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029682;
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

		deco16Scan();

		MSM6295Scan(nAction, pnMin);

		SCAN_VAR(mux_data);
		SCAN_VAR(oki_banks);
	}

	if (nAction & ACB_WRITE)
	{
		oki_bankswitch(0,oki_banks[0]);
		oki_bankswitch(1,oki_banks[1]);
	}

	return 0;
}


// Mirage Youjuu Mahjongden (Japan)

static struct BurnRomInfo mirageRomDesc[] = {
	{ "mr_00-.2a",	0x040000, 0x3a53f33d, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "mr_01-.3a",	0x040000, 0xa0b758aa, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "mbl-00.7a",	0x100000, 0x2e258b7b, 2 | BRF_GRA },           //  2 Tiles (8x8 & 16x16, 4bpp, encrypted)

	{ "mbl-01.11a",	0x200000, 0x895be69a, 3 | BRF_GRA },           //  3 Sprites
	{ "mbl-02.12a",	0x200000, 0x474f6104, 3 | BRF_GRA },           //  4

	{ "mbl-03.10a",	0x200000, 0x4a599703, 4 | BRF_SND },           //  5 OKI #0 Samples

	{ "mbl-04.12k",	0x100000, 0xb533123d, 5 | BRF_SND },           //  6 OKI #1 Samples
};

STD_ROM_PICK(mirage)
STD_ROM_FN(mirage)

struct BurnDriver BurnDrvMirage = {
	"mirage", NULL, NULL, NULL, "1994",
	"Mirage Youjuu Mahjongden (Japan)\0", NULL, "Mitchell", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MAHJONG, 0,
	NULL, mirageRomInfo, mirageRomName, NULL, NULL, NULL, NULL, MirageInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};
