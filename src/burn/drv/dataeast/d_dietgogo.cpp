// FB Alpha Diet Go Go driver module
// Based on MAME driver by Bryan McPhail and David Haywood

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "deco16ic.h"
#include "deco146.h"
#include "msm6295.h"
#include "burn_ym2151.h"
#include "h6280_intf.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *Drv68KCode;
static UINT8 *DrvHucROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvSndROM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvHucRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 *soundlatch;
static UINT8 *flipscreen;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvDips[2];
static UINT8 DrvReset;
static UINT16 DrvInputs[2];

static INT32 nCyclesExtra;

static struct BurnInputInfo DietgoInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 15,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 2,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Dietgo)

static struct BurnDIPInfo DietgoDIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL			},
	{0x15, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x14, 0x01, 0x07, 0x00, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0x07, 0x01, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x14, 0x01, 0x07, 0x04, "1 Coin  4 Credits"	},
	{0x14, 0x01, 0x07, 0x03, "1 Coin  5 Credits"	},
	{0x14, 0x01, 0x07, 0x02, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x14, 0x01, 0x38, 0x00, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0x38, 0x08, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x38, 0x30, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0x38, 0x28, "1 Coin  3 Credits"	},
	{0x14, 0x01, 0x38, 0x20, "1 Coin  4 Credits"	},
	{0x14, 0x01, 0x38, 0x18, "1 Coin  5 Credits"	},
	{0x14, 0x01, 0x38, 0x10, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x14, 0x01, 0x40, 0x40, "Off"			},
	{0x14, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Continue Coin"	},
	{0x14, 0x01, 0x80, 0x80, "1 Start/1 Continue"	},
	{0x14, 0x01, 0x80, 0x00, "2 Start/1 Continue"	},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x15, 0x01, 0x03, 0x01, "1"			},
	{0x15, 0x01, 0x03, 0x00, "2"			},
	{0x15, 0x01, 0x03, 0x03, "3"			},
	{0x15, 0x01, 0x03, 0x02, "4"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x15, 0x01, 0x0c, 0x08, "Easy"			},
	{0x15, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x15, 0x01, 0x0c, 0x04, "Hard"			},
	{0x15, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x15, 0x01, 0x10, 0x10, "Off"			},
	{0x15, 0x01, 0x10, 0x00, "On"			},
};

STDDIPINFO(Dietgo)

static void __fastcall dietgogo_main_write_word(UINT32 address, UINT16 data)
{
	deco16_write_control_word(0, address, 0x200000, data)

	if (address >= 0x340000 && address <= 0x343fff) {
		deco146_104_prot_ww(0, address, data);
	}
}

static void __fastcall dietgogo_main_write_byte(UINT32 address, UINT8 data)
{
	if (address >= 0x340000 && address <= 0x343fff) {
		deco146_104_prot_wb(0, address, data);
	}
}

static UINT16 __fastcall dietgogo_main_read_word(UINT32 address)
{
	if (address >= 0x340000 && address <= 0x343fff) {
		return deco146_104_prot_rw(0, address);
	}

	return 0;
}

static UINT8 __fastcall dietgogo_main_read_byte(UINT32 address)
{
	if (address >= 0x340000 && address <= 0x343fff) {
		return deco146_104_prot_rb(0, address);
	}

	return 0;
}

static INT32 dietgo_bank_callback(const INT32 bank)
{
	return ((bank >> 4) & 0x7) * 0x1000;
}

static UINT16 inputs_read()
{
	return DrvInputs[0];
}

static UINT16 system_read()
{
	return (DrvInputs[1] & 7) | deco16_vblank;
}

static UINT16 dips_read()
{
	return (DrvDips[1] << 8) | (DrvDips[0] << 0);
}

static void soundlatch_write(UINT16 data)
{
	deco16_soundlatch = data & 0xff;
	h6280SetIRQLine(0, CPU_IRQSTATUS_ACK);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

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

	Drv68KROM	= Next; Next += 0x080000;
	Drv68KCode	= Next; Next += 0x080000;
	DrvHucROM	= Next; Next += 0x010000;

	DrvGfxROM0	= Next; Next += 0x200000;
	DrvGfxROM1	= Next; Next += 0x200000;
	DrvGfxROM2	= Next; Next += 0x400000;

	MSM6295ROM	= Next;
	DrvSndROM	= Next; Next += 0x080000;

	DrvPalette	= (UINT32*)Next; Next += 0x0300 * sizeof(UINT32);

	AllRam		= Next;

	Drv68KRAM	= Next; Next += 0x010000;
	DrvSprRAM	= Next; Next += 0x000800;
	DrvPalRAM	= Next; Next += 0x000c00;
	DrvHucRAM	= Next; Next += 0x002000;

	soundlatch	= Next; Next += 0x000001;
	flipscreen	= Next; Next += 0x000001;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static INT32 DrvInit()
{
	BurnSetRefreshRate(58.00);

	BurnAllocMemIndex();

	{
		if (BurnLoadRom(Drv68KROM  + 0x00000,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x00001,  1, 2)) return 1;

		if (BurnLoadRom(DrvHucROM  + 0x00000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000,  4, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x00001,  5, 2)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x00000,  6, 1)) return 1;

		deco102_decrypt_cpu(Drv68KROM, Drv68KCode, 0x80000, 0xe9ba, 0x01, 0x19);

		deco56_decrypt_gfx(DrvGfxROM1, 0x100000);

		deco16_tile_decode(DrvGfxROM1, DrvGfxROM0, 0x100000, 1);
		deco16_tile_decode(DrvGfxROM1, DrvGfxROM1, 0x100000, 0);
		deco16_sprite_decode(DrvGfxROM2, 0x200000);
	}	

	deco16Init(1, 0, 1);
	deco16_set_graphics(DrvGfxROM0, 0x100000 * 2, DrvGfxROM1, 0x100000 * 2, NULL, 0);
	deco16_set_global_offsets(0, 8);
	deco16_set_bank_callback(0, dietgo_bank_callback);
	deco16_set_bank_callback(1, dietgo_bank_callback);

	// 146_104 prot
	deco_104_init();
	deco_146_104_set_port_a_cb(inputs_read); // inputs
	deco_146_104_set_port_b_cb(system_read); // system
	deco_146_104_set_port_c_cb(dips_read); // dips
	deco_146_104_set_soundlatch_cb(soundlatch_write);
	deco_146_104_set_interface_scramble_interleave();
	deco_146_104_set_use_magic_read_address_xor(1);

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x07ffff, MAP_READ);
	SekMapMemory(Drv68KCode,		0x000000, 0x07ffff, MAP_FETCH);
	SekMapMemory(deco16_pf_ram[0],		0x210000, 0x211fff, MAP_RAM);
	SekMapMemory(deco16_pf_ram[1],		0x212000, 0x213fff, MAP_RAM);
	SekMapMemory(deco16_pf_rowscroll[0],	0x220000, 0x2207ff, MAP_RAM);
	SekMapMemory(deco16_pf_rowscroll[1],	0x222000, 0x2227ff, MAP_RAM);
	SekMapMemory(DrvSprRAM,			0x280000, 0x2807ff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x300000, 0x300bff, MAP_RAM);
	SekMapMemory(Drv68KRAM,			0x380000, 0x38ffff, MAP_RAM);
	SekSetWriteWordHandler(0,		dietgogo_main_write_word);
	SekSetWriteByteHandler(0,		dietgogo_main_write_byte);
	SekSetReadWordHandler(0,		dietgogo_main_read_word);
	SekSetReadByteHandler(0,		dietgogo_main_read_byte);
	SekClose();

	deco16SoundInit(DrvHucROM, DrvHucRAM, 2685000, 0, NULL, 0.15, 1006875, 0.50, 0, 0);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	deco16Exit();
	deco16SoundExit();

	SekExit();

	BurnFreeMemIndex();

	return 0;
}

static void draw_sprites()
{
	UINT16 *ram = (UINT16*)DrvSprRAM;

	for (INT32 offs = 0; offs < 0x400; offs += 4)
	{
		if (BURN_ENDIAN_SWAP_INT16(ram[offs + 1]) == 0) continue;

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
			Draw16x16MaskTile(pTransDraw, code - multi * inc, sx, (sy + mult * multi) - 8, flipx, flipy, color, 4, 0, 0x200, DrvGfxROM2);

			multi--;
		}
	}
}

static INT32 DrvDraw()
{
//	if (DrvRecalc) {
		deco16_palette_recalculate(DrvPalette, DrvPalRAM);
		DrvRecalc = 0;
//	}

	deco16_pf12_update();

	BurnTransferClear(0x100);

	if (nBurnLayer & 1) deco16_draw_layer(1, pTransDraw, DECO16_LAYER_OPAQUE);

	if (nBurnLayer & 2) deco16_draw_layer(0, pTransDraw, 0);

	if (nBurnLayer & 4) draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	h6280NewFrame();

	{
		memset (DrvInputs, 0xff, 2 * sizeof(UINT16));
		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 232; //256;
	INT32 nCyclesTotal[2] = { 14000000 / 58, 2685000 / 58 };
	INT32 nCyclesDone[2] = { nCyclesExtra, 0 };

	SekOpen(0);
	h6280Open(0);

	deco16_vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);
		CPU_RUN_TIMER(1);

		if (i == 208) deco16_vblank = 0x08;
	}

	SekSetIRQLine(6, CPU_IRQSTATUS_AUTO);

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


// Diet Go Go (Euro v1.1 1992.09.26 v3)

static struct BurnRomInfo dietgoRomDesc[] = {
	{ "jy_00-3.4h",			0x040000, 0xa863ad0c, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "jy_01-3.5h",			0x040000, 0xef243eda, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "jy_02.14m",			0x010000, 0x4e3492a5, 2 | BRF_PRG | BRF_ESS }, //  2 Huc6280 Code

	{ "may-00.10a",			0x100000, 0x234d1f8d, 3 | BRF_GRA },           //  3 Characters & Background Tiles

	{ "may-01.14a",			0x100000, 0x2da57d04, 4 | BRF_GRA },           //  4 Sprites
	{ "may-02.16a",			0x100000, 0x3a66a713, 4 | BRF_GRA },           //  5

	{ "may-03.11l",			0x080000, 0xb6e42bae, 5 | BRF_SND },           //  6 OKI M6295 Samples

	{ "pal16l8b_vd-00.6h",	0x000104, 0x00000000, 6 | BRF_NODUMP },        //  7 PLDs
	{ "pal16l8b_vd-01.7h",	0x000104, 0x00000000, 6 | BRF_NODUMP },        //  8
	{ "pal16r6a_vd-02.11h",	0x000104, 0x00000000, 6 | BRF_NODUMP },        //  9
};

STD_ROM_PICK(dietgo)
STD_ROM_FN(dietgo)

struct BurnDriver BurnDrvDietgo = {
	"dietgo", NULL, NULL, NULL, "1992",
	"Diet Go Go (Euro v1.1 1992.09.26 v3)\0", NULL, "Data East Corporation", "DECO IC16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_PLATFORM, 0,
	NULL, dietgoRomInfo, dietgoRomName, NULL, NULL, NULL, NULL, DietgoInputInfo, DietgoDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	320, 240, 4, 3
};


// Diet Go Go (Euro v1.1 1992.09.26 v2)

static struct BurnRomInfo dietgoeRomDesc[] = {
	{ "jy_00-2.4h",			0x040000, 0x014dcf62, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "jy_01-2.5h",			0x040000, 0x793ebd83, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "jy_02.14m",			0x010000, 0x4e3492a5, 2 | BRF_PRG | BRF_ESS }, //  2 Huc6280 Code

	{ "may-00.10a",			0x100000, 0x234d1f8d, 3 | BRF_GRA },           //  3 Characters & Background Tiles

	{ "may-01.14a",			0x100000, 0x2da57d04, 4 | BRF_GRA },           //  4 Sprites
	{ "may-02.16a",			0x100000, 0x3a66a713, 4 | BRF_GRA },           //  5

	{ "may-03.11l",			0x080000, 0xb6e42bae, 5 | BRF_SND },           //  6 OKI M6295 Samples

	{ "pal16l8b_vd-00.6h",	0x000104, 0x00000000, 6 | BRF_NODUMP },        //  7 PLDs
	{ "pal16l8b_vd-01.7h",	0x000104, 0x00000000, 6 | BRF_NODUMP },        //  8
	{ "pal16r6a_vd-02.11h",	0x000104, 0x00000000, 6 | BRF_NODUMP },        //  9
};

STD_ROM_PICK(dietgoe)
STD_ROM_FN(dietgoe)

struct BurnDriver BurnDrvDietgoe = {
	"dietgoe", "dietgo", NULL, NULL, "1992",
	"Diet Go Go (Euro v1.1 1992.09.26 v2)\0", NULL, "Data East Corporation", "DECO IC16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_PLATFORM, 0,
	NULL, dietgoeRomInfo, dietgoeRomName, NULL, NULL, NULL, NULL, DietgoInputInfo, DietgoDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	320, 240, 4, 3
};


// Diet Go Go (Euro v1.1 1992.08.04)

static struct BurnRomInfo dietgoeaRomDesc[] = {
	{ "jy_00-1.4h",			0x040000, 0x8bce137d, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "jy_01-1.5h",			0x040000, 0xeca50450, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "jy_02.14m",			0x010000, 0x4e3492a5, 2 | BRF_PRG | BRF_ESS }, //  2 Huc6280 Code

	{ "may-00.10a",			0x100000, 0x234d1f8d, 3 | BRF_GRA },           //  3 Characters & Background Tiles

	{ "may-01.14a",			0x100000, 0x2da57d04, 4 | BRF_GRA },           //  4 Sprites
	{ "may-02.16a",			0x100000, 0x3a66a713, 4 | BRF_GRA },           //  5

	{ "may-03.11l",			0x080000, 0xb6e42bae, 5 | BRF_SND },           //  6 OKI M6295 Samples

	{ "pal16l8b_vd-00.6h",	0x000104, 0x00000000, 6 | BRF_NODUMP },        //  7 PLDs
	{ "pal16l8b_vd-01.7h",	0x000104, 0x00000000, 6 | BRF_NODUMP },        //  8
	{ "pal16r6a_vd-02.11h",	0x000104, 0x00000000, 6 | BRF_NODUMP },        //  9
};

STD_ROM_PICK(dietgoea)
STD_ROM_FN(dietgoea)

struct BurnDriver BurnDrvDietgoea = {
	"dietgoea", "dietgo", NULL, NULL, "1992",
	"Diet Go Go (Euro v1.1 1992.08.04)\0", NULL, "Data East Corporation", "DECO IC16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_PLATFORM, 0,
	NULL, dietgoeaRomInfo, dietgoeaRomName, NULL, NULL, NULL, NULL, DietgoInputInfo, DietgoDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	320, 240, 4, 3
};


// Diet Go Go (USA v1.1 1992.09.26)

static struct BurnRomInfo dietgouRomDesc[] = {
	{ "jx_00-.4h",			0x040000, 0x1a9de04f, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "jx_01-.5h",			0x040000, 0x79c097c8, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "jx_02.14m",			0x010000, 0x4e3492a5, 2 | BRF_PRG | BRF_ESS }, //  2 Huc6280 Code

	{ "may-00.10a",			0x100000, 0x234d1f8d, 3 | BRF_GRA },           //  3 Characters & Background Tiles

	{ "may-01.14a",			0x100000, 0x2da57d04, 4 | BRF_GRA },           //  4 Sprites
	{ "may-02.16a",			0x100000, 0x3a66a713, 4 | BRF_GRA },           //  5

	{ "may-03.11l",			0x080000, 0xb6e42bae, 5 | BRF_SND },           //  6 OKI M6295 Samples

	{ "pal16l8b_vd-00.6h",	0x000104, 0x00000000, 6 | BRF_NODUMP },        //  7 PLDs
	{ "pal16l8b_vd-01.7h",	0x000104, 0x00000000, 6 | BRF_NODUMP },        //  8
	{ "pal16r6a_vd-02.11h",	0x000104, 0x00000000, 6 | BRF_NODUMP },        //  9
};

STD_ROM_PICK(dietgou)
STD_ROM_FN(dietgou)

struct BurnDriver BurnDrvDietgou = {
	"dietgou", "dietgo", NULL, NULL, "1992",
	"Diet Go Go (USA v1.1 1992.09.26)\0", NULL, "Data East Corporation", "DECO IC16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_PLATFORM, 0,
	NULL, dietgouRomInfo, dietgouRomName, NULL, NULL, NULL, NULL, DietgoInputInfo, DietgoDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	320, 240, 4, 3
};


// Diet Go Go (Japan v1.1 1992.09.26)

static struct BurnRomInfo dietgojRomDesc[] = {
	{ "jw_00-2.4h",			0x040000, 0xe6ba6c49, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "jw_01-2.5h",			0x040000, 0x684a3d57, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "jw_02.14m",			0x010000, 0x4e3492a5, 2 | BRF_PRG | BRF_ESS }, //  2 Huc6280 Code

	{ "may-00.10a",			0x100000, 0x234d1f8d, 3 | BRF_GRA },           //  3 Characters & Background Tiles

	{ "may-01.14a",			0x100000, 0x2da57d04, 4 | BRF_GRA },           //  4 Sprites
	{ "may-02.16a",			0x100000, 0x3a66a713, 4 | BRF_GRA },           //  5

	{ "may-03.11l",			0x080000, 0xb6e42bae, 5 | BRF_SND },           //  6 OKI M6295 Samples
	
	{ "pal16l8b_vd-00.6h",	0x000104, 0x00000000, 6 | BRF_NODUMP },        //  7 PLDs
	{ "pal16l8b_vd-01.7h",	0x000104, 0x00000000, 6 | BRF_NODUMP },        //  8
	{ "pal16r6a_vd-02.11h",	0x000104, 0x00000000, 6 | BRF_NODUMP },        //  9
};

STD_ROM_PICK(dietgoj)
STD_ROM_FN(dietgoj)

struct BurnDriver BurnDrvDietgoj = {
	"dietgoj", "dietgo", NULL, NULL, "1992",
	"Diet Go Go (Japan v1.1 1992.09.26)\0", NULL, "Data East Corporation", "DECO IC16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_PLATFORM, 0,
	NULL, dietgojRomInfo, dietgojRomName, NULL, NULL, NULL, NULL, DietgoInputInfo, DietgoDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	320, 240, 4, 3
};
