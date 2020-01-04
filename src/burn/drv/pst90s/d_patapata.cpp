// FB Alpha Pata Pata Panic driver module
// Based on MAME driver by David Haywood

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "msm6295.h"
#include "nmk112.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvSndROM0;
static UINT8 *DrvSndROM1;
static UINT8 *Drv68KRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvBgRAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvVidRegs;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[2];
static UINT8 DrvReset;
static UINT8 DrvInputs[2];

static UINT8 flipscreen;

static INT32 fg_bank;

static struct BurnInputInfo PatapataInputList[] = {
	{"Coin 1",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 coin"	},
	{"Coin 2",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 coin"	},
	{"Start 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 start"	},
	{"Start 2",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 start"	},
	{"Left Stick Down",	BIT_DIGITAL,	DrvJoy2 + 0,	"p1 down"	},
	{"Left Stick Up",	BIT_DIGITAL,	DrvJoy2 + 1,	"p1 up"		},
	{"Right Stick Down",	BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"	},
	{"Right Stick Up",	BIT_DIGITAL,	DrvJoy2 + 3,	"p2 up"		},

	{"In Game Reset",	BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"Hopper",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Patapata)

static struct BurnDIPInfo PatapataDIPList[]=
{
	{0x0c, 0xff, 0xff, 0xff, NULL			},
	{0x0d, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x0c, 0x01, 0x01, 0x01, "Off"			},
	{0x0c, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x0d, 0x01, 0x02, 0x02, "On"			},
	{0x0d, 0x01, 0x02, 0x00, "Off"			},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x0d, 0x01, 0x1c, 0x1c, "1 Coin  1 Credits"	},
	{0x0d, 0x01, 0x1c, 0x0c, "1 Coin  2 Credits"	},
	{0x0d, 0x01, 0x1c, 0x14, "1 Coin  3 Credits"	},
	{0x0d, 0x01, 0x1c, 0x04, "1 Coin  4 Credits"	},
	{0x0d, 0x01, 0x1c, 0x18, "1 Coin  5 Credits"	},
	{0x0d, 0x01, 0x1c, 0x08, "1 Coin  6 Credits"	},
	{0x0d, 0x01, 0x1c, 0x10, "1 Coin  7 Credits"	},
	{0x0d, 0x01, 0x1c, 0x00, "1 Coin  8 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x0d, 0x01, 0xe0, 0x00, "5 Coins 1 Credits"	},
	{0x0d, 0x01, 0xe0, 0x80, "4 Coins 1 Credits"	},
	{0x0d, 0x01, 0xe0, 0x40, "3 Coins 1 Credits"	},
	{0x0d, 0x01, 0xe0, 0xc0, "2 Coins 1 Credits"	},
	{0x0d, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"	},
	{0x0d, 0x01, 0xe0, 0x60, "1 Coin  2 Credits"	},
	{0x0d, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"	},
	{0x0d, 0x01, 0xe0, 0x20, "1 Coin  4 Credits"	},
};

STDDIPINFO(Patapata)

static void __fastcall patapata_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x100014:
			flipscreen = data & 0x01;
		return;

		case 0x150000:
		case 0x150010:
			MSM6295Write((address / 0x10) & 1, data & 0xff);
		return;

		case 0x150020:
		case 0x150022:
		case 0x150024:
		case 0x150026:
		case 0x150028:
		case 0x15002a:
		case 0x15002c:
		case 0x15002e:
			NMK112_okibank_write((address / 2) & 7, data & 0xff);
		return;
	}
}

static UINT16 __fastcall patapata_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x100000:
		case 0x100002:
			return DrvInputs[(address / 2) & 1];

		case 0x100008:
		case 0x10000a:
			return DrvDips[(address / 2) & 1];

		case 0x150000:
		case 0x150010:
			return MSM6295Read((address / 0x10) & 1);
	}

	return 0;
}

static tilemap_scan( patapata )
{
	return (col & 0xff) * 16 + (row & 0xf) + ((row & 0x10)<<8) + ((col & 0x300) << 5);
}

static tilemap_callback( bg )
{
	UINT16 code = *((UINT16*)(DrvBgRAM + (offs * 2)));

	TILE_SET_INFO(0, code, code >> 12, 0);
}

static tilemap_callback( fg )
{
	UINT16 code = *((UINT16*)(DrvFgRAM + (offs * 2)));

	TILE_SET_INFO(1, (code & 0xfff) | fg_bank, code >> 12, 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	MSM6295Reset();
	NMK112Reset();

	flipscreen = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM	= Next; Next += 0x080000;

	DrvGfxROM0	= Next; Next += 0x100000;
	DrvGfxROM1	= Next; Next += 0x300000;

	MSM6295ROM	= Next;
	DrvSndROM0	= Next; Next += 0x100000;
	DrvSndROM1	= Next; Next += 0x100000;

	DrvPalette	= (UINT32*)Next; Next += 0x0300 * sizeof(UINT32);

	AllRam		= Next;

	Drv68KRAM	= Next; Next += 0x010000;
	DrvPalRAM	= Next; Next += 0x000800;
	DrvFgRAM	= Next; Next += 0x010000;
	DrvBgRAM	= Next; Next += 0x010000;
	DrvVidRegs	= Next; Next += 0x000400;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[4]  = { STEP4(0,1) };
	INT32 XOffs[16] = { STEP8(0,4), STEP8(512,4) };
	INT32 YOffs[16] = { STEP16(0,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x180000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x080000);

	GfxDecode(0x1000, 4, 16, 16, Plane, XOffs, YOffs, 0x400, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x180000);

	GfxDecode(0x3000, 4, 16, 16, Plane, XOffs, YOffs, 0x400, tmp, DrvGfxROM1);

	memset (DrvGfxROM1 + 0x300000, 0xf, 0x100000);

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
		if (BurnLoadRom(Drv68KROM  + 0x000000,   0, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000001,   1, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000000,   2, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000001,   3, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x000000,   4, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x100001,   5, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x100000,   6, 2)) return 1;

		if (BurnLoadRom(DrvSndROM0 + 0x000000,   7, 1)) return 1;
		if (BurnLoadRom(DrvSndROM0 + 0x080000,   8, 1)) return 1;

		if (BurnLoadRom(DrvSndROM1 + 0x000000,   9, 1)) return 1;
		if (BurnLoadRom(DrvSndROM1 + 0x080000,  10, 1)) return 1;

		DrvGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(DrvVidRegs,	0x110000, 0x1103ff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		0x120000, 0x1207ff, MAP_RAM);
	SekMapMemory(DrvFgRAM,		0x130000, 0x13ffff, MAP_RAM);
	SekMapMemory(DrvBgRAM,		0x140000, 0x14ffff, MAP_RAM);
	SekMapMemory(Drv68KRAM,		0x180000, 0x18ffff, MAP_RAM);
	SekSetWriteWordHandler(0,	patapata_write_word);
	SekSetReadWordHandler(0,	patapata_read_word);
	SekClose();

	MSM6295Init(0, 4000000 / 165, 0);
	MSM6295Init(1, 4000000 / 165, 1);
	MSM6295SetRoute(0, 0.40, BURN_SND_ROUTE_BOTH);
	MSM6295SetRoute(1, 0.40, BURN_SND_ROUTE_BOTH);

	NMK112_init(0, DrvSndROM0, DrvSndROM1, 0x100000, 0x100000);

	GenericTilesInit();
	GenericTilemapInit(0, patapata_map_scan, bg_map_callback, 16, 16, 1024, 32);
	GenericTilemapInit(1, patapata_map_scan, fg_map_callback, 16, 16, 1024, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 4, 16, 16, 0x100000, 0x000, 0xf);
	GenericTilemapSetGfx(1, DrvGfxROM1, 4, 16, 16, 0x300000, 0x100, 0xf);
	GenericTilemapSetTransparent(1, 0xf);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	MSM6295Exit();
	SekExit();

	BurnFree (AllMem);

	MSM6295ROM = NULL;

	return 0;
}

static void DrvPaletteUpdate()
{
	UINT16 *p = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x300; i++)
	{
		UINT8 r = ((p[i] >> 11) & 0x1e) | ((p[i] >> 3) & 1);
		UINT8 g = ((p[i] >>  7) & 0x1e) | ((p[i] >> 2) & 1);
		UINT8 b = ((p[i] >>  3) & 0x1e) | ((p[i] >> 1) & 1);

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	}

	UINT16 *regs = (UINT16*)DrvVidRegs;

	fg_bank = (regs[4] & 0x3) << 12;

	INT32 scrollx = (regs[2] - 0xff0) & 0xfff;
	INT32 scrolly = (regs[3] - 0x7b0) & 0xfff;
	if (scrolly & 0x200) scrollx += 0x1000;

	GenericTilemapSetScrollX(0, scrollx);
	GenericTilemapSetScrollY(0, scrolly & 0x1ff);

	scrollx = (regs[0] - 0xff0) & 0xfff;
	scrolly = (regs[1] - 0x7b0) & 0xfff;
	if (scrolly & 0x200) scrollx += 0x1000;

	GenericTilemapSetScrollX(1, scrollx);
	GenericTilemapSetScrollY(1, scrolly & 0x1ff);

	GenericTilemapSetEnable(1, (fg_bank == 3) ? 0 : 1);

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);

	if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, 2);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	SekOpen(0);
	SekRun((16000000 / 60) / 2);
	SekSetIRQLine(1, CPU_IRQSTATUS_AUTO);
	SekRun((16000000 / 60) / 2);
	SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
	SekClose();

	if (pBurnSoundOut) {
		MSM6295Render(pBurnSoundOut, nBurnSoundLen);
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
		*pnMin = 0x029698;
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

		MSM6295Scan(nAction, pnMin);

		NMK112_Scan(nAction);

		SCAN_VAR(flipscreen);
	}

	return 0;
}


// Pata Pata Panic

static struct BurnRomInfo patapataRomDesc[] = {
	{ "rw-93085-7.u132",	0x80000, 0xf8084e30, 1 | BRF_PRG | BRF_ESS }, //  0 68K code

	{ "rw-93085-9.u5",	0x40000, 0xc2e243ff, 2 | BRF_GRA },           //  1 Background Tiles
	{ "rw-93085-10.u15",	0x40000, 0x546be459, 2 | BRF_GRA },           //  2

	{ "rw-93085-17.u9",	0x80000, 0xe19afa04, 3 | BRF_GRA },           //  3 Foreground Tiles
	{ "rw-93085-18.u19",	0x80000, 0x5cf4582e, 3 | BRF_GRA },           //  4
	{ "rw-93085-19.u19",	0x40000, 0xdfd7bdcf, 3 | BRF_GRA },           //  5
	{ "rw-93085-20.u20",	0x40000, 0xdd821f74, 3 | BRF_GRA },           //  6

	{ "rw-93085-5.u22",	0x80000, 0x0c0d2835, 4 | BRF_SND },           //  7 OKI #0 Samples
	{ "rw-93085-6.u23",	0x80000, 0x882c25d0, 4 | BRF_SND },           //  8

	{ "rw-93085-1.u3",	0x80000, 0xd9776d50, 5 | BRF_SND },           //  9 OKI #1 Samples
	{ "rw-93085-2.u4",	0x80000, 0x3698fafa, 5 | BRF_SND },           // 10

	{ "n82s131n.u119",	0x00200, 0x33f63fc8, 0 | BRF_OPT },           // 11 Unused PROMs
	{ "n82s135n.u137",	0x00100, 0xcb90eedc, 0 | BRF_OPT },           // 12
};

STD_ROM_PICK(patapata)
STD_ROM_FN(patapata)

struct BurnDriver BurnDrvPatapata = {
	"patapata", NULL, NULL, NULL, "1993",
	"Pata Pata Panic\0", NULL, "Atlus", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MINIGAMES, 0,
	NULL, patapataRomInfo, patapataRomName, NULL, NULL, NULL, NULL, PatapataInputInfo, PatapataDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	480, 352, 4, 3
};
