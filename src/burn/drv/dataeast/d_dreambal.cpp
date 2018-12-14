// FB Alpha Dream Ball driver module
// Based on MAME driver by David Haywood

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "deco16ic.h"
#include "deco146.h"
#include "eeprom.h"
#include "msm6295.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvSndROM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvPalRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvReset;
static UINT16 DrvInputs[2];

static struct BurnInputInfo DreambalInputList[] = {
	{"Coin 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"Coin 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 coin"	},
	{"Start / Proceed",	BIT_DIGITAL,	DrvJoy1 + 10,	"p1 start"	},

	{"Take",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
//	{"Payout",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	}, // hopper error
	{"Poker Cancel (fold)",	BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 3"	},
	{"Bet",			BIT_DIGITAL,	DrvJoy1 + 9,	"p1 fire 7"	},
	{"Hold 1 / Double up",	BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 4"	},
	{"Hold 2 /  Big",	BIT_DIGITAL,	DrvJoy1 + 11,	"p1 fire 8"	},
	{"Hold 2 / Small",	BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 5"	},
	{"Hold 4",		BIT_DIGITAL,	DrvJoy1 + 12,	"p1 fire 9"	},
	{"Hold 5",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 6"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy1 + 13,	"diag"		},
};

STDINPUTINFO(Dreambal)

static void __fastcall dreambal_main_write_word(UINT32 address, UINT16 data)
{
	deco16_write_control_word(0, address, 0x161000, data)

	if (address >= 0x160000 && address <= 0x163fff) {
		deco146_104_prot_ww(0, address, data);
		return;
	}

	switch (address)
	{
		case 0x180000:
			MSM6295Write(0,data & 0xff);
		return;

		case 0x165000:
			EEPROMSetClockLine(data & 0x02 ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
			EEPROMWriteBit(data & 0x01);
			EEPROMSetCSLine(data & 0x04 ? EEPROM_CLEAR_LINE : EEPROM_ASSERT_LINE);
		return;

		case 0x164000:
		case 0x164001:
		case 0x16c002:
		case 0x16c003:
		case 0x16c004:
		case 0x16c005:
		case 0x16c006:
		case 0x16c007:
		case 0x16c008:
		case 0x16c009:
		case 0x16c00a:
		case 0x16c00b:
		case 0x16c00c:
		case 0x16c00d:
		return;	// nop

		case 0x1a0000:
		case 0x1a0001:
		return;	//nop
	}
}

static void __fastcall dreambal_main_write_byte(UINT32 address, UINT8 data)
{
	deco16_write_control_byte(0, address, 0x161000, data)

	if (address >= 0x160000 && address <= 0x163fff) {
		deco146_104_prot_wb(0, address, data);
		return;
	}
}

static UINT16 __fastcall dreambal_main_read_word(UINT32 address)
{
	if (address >= 0x160000 && address <= 0x163fff) {
		return deco146_104_prot_rw(0, address);
	}

	switch (address)
	{
		case 0x180000:
			return MSM6295Read(0);
	}

	return 0;
}

static UINT8 __fastcall dreambal_main_read_byte(UINT32 address)
{
	if (address >= 0x160000 && address <= 0x163fff) {
		return deco146_104_prot_rb(0, address);
	}

	return 0;
}

static UINT16 deco_104_port_a_cb()
{
	return DrvInputs[0];
}

static UINT16 deco_104_port_b_cb()
{
	return DrvInputs[1] & ~8;
}

static UINT16 deco_104_port_c_cb()
{
	return (EEPROMRead() ? 0xfff7 : 0xfff6);
}

static INT32 dreambal_bank_callback(const INT32 bank)
{
	return (bank & 0x70) * 0x100;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	deco16Reset();

	EEPROMReset();

	MSM6295Reset(0);

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM	= Next; Next += 0x080000;

	DrvGfxROM0	= Next; Next += 0x100000;
	DrvGfxROM1	= Next; Next += 0x100000;

	MSM6295ROM	= Next;
	DrvSndROM	= Next; Next += 0x040000;

	DrvPalette	= (UINT32*)Next; Next += 0x0200 * sizeof(UINT32);

	AllRam		= Next;

	Drv68KRAM	= Next; Next += 0x004000;
	DrvPalRAM	= Next; Next += 0x000400;

	RamEnd		= Next;

	MemEnd		= Next;

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
		if (BurnLoadRom(Drv68KROM  + 0x00000,  0, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  1, 1)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x00000,  2, 1)) return 1;

		deco56_decrypt_gfx(DrvGfxROM1, 0x080000);

		deco16_tile_decode(DrvGfxROM1, DrvGfxROM0, 0x080000, 1);
		deco16_tile_decode(DrvGfxROM1, DrvGfxROM1, 0x080000, 0);
	}	

	deco16Init(1, 0, 1);
	deco16_set_graphics(DrvGfxROM0, 0x100000, DrvGfxROM1, 0x100000, NULL, 0);
	deco16_set_global_offsets(0, 8);
	deco16_set_bank_callback(0, dreambal_bank_callback);
	deco16_set_bank_callback(1, dreambal_bank_callback);

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(deco16_pf_ram[0],		0x100000, 0x101fff, MAP_RAM);
	SekMapMemory(deco16_pf_ram[1],		0x102000, 0x103fff, MAP_RAM);
	SekMapMemory(Drv68KRAM,			0x120000, 0x123fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x140000, 0x1403ff, MAP_RAM);
	SekSetWriteWordHandler(0,		dreambal_main_write_word);
	SekSetWriteByteHandler(0,		dreambal_main_write_byte);
	SekSetReadWordHandler(0,		dreambal_main_read_word);
	SekSetReadByteHandler(0,		dreambal_main_read_byte);
	SekClose();

	deco_104_init();
	deco_146_104_set_port_a_cb(deco_104_port_a_cb);
	deco_146_104_set_port_b_cb(deco_104_port_b_cb);
	deco_146_104_set_port_c_cb(deco_104_port_c_cb);

	EEPROMInit(&eeprom_interface_93C46);

	MSM6295Init(0, 1228800 / 132, 0);
	MSM6295SetRoute(0, 0.75, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	deco16Exit();

	EEPROMExit();

	MSM6295Exit(0);

	SekExit();

	BurnFree (AllMem);

	MSM6295ROM = NULL;

	return 0;
}

static void DrvPaletteUpdate()
{
	UINT16 *p = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x400/2; i++)
	{
		UINT8 r = (p[i] >>  0) & 0xf;
		UINT8 g = (p[i] >>  4) & 0xf;
		UINT8 b = (p[i] >>  8) & 0xf;

		DrvPalette[i] = BurnHighCol(r+r*16, g+g*16, b+b*16, 0);
	}
}

static INT32 DrvDraw()
{
//	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
//	}

	deco16_pf12_update();

	BurnTransferClear();

	if (nBurnLayer & 1) deco16_draw_layer(1, pTransDraw, 2);
	if (nBurnLayer & 2) deco16_draw_layer(0, pTransDraw, 4);

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

	SekOpen(0);
	SekRun(14000000 / 58);
	SekSetIRQLine(6, CPU_IRQSTATUS_AUTO);
	SekClose();

	if (pBurnSoundOut) {
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
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

		deco16Scan();
	}

	return 0;
}


// Dream Ball (Japan V2.4)

static struct BurnRomInfo dreambalRomDesc[] = {
	{ "mm_00-2.1c",		0x20000, 0x257f6ad1, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code

	{ "mm_01-1.12b",	0x80000, 0xdc9cc708, 2 | BRF_GRA },           //  1 Characters & Tiles (Encrypted)

	{ "mm_01-1.12f",	0x20000, 0x4f134be7, 3 | BRF_SND },           //  2 OKI M6295 Samples
};

STD_ROM_PICK(dreambal)
STD_ROM_FN(dreambal)

struct BurnDriver BurnDrvDreambal = {
	"dreambal", NULL, NULL, NULL, "1993",
	"Dream Ball (Japan V2.4)\0", NULL, "NDK / Data East", "DECO IC16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PREFIX_DATAEAST, GBF_CASINO, 0,
	NULL, dreambalRomInfo, dreambalRomName, NULL, NULL, NULL, NULL, DreambalInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	320, 240, 4, 3
};
