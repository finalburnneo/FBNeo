// FB Alpha Atari Food Fight driver module
// Based on MAME driver by Aaron Giles

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "pokey.h"
#include "watchdog.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvNVRAM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvPalRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 analog_select;
static UINT8 irq_vector;
static UINT8 flipscreen;

static INT32 nExtraCycles;

static UINT8 DrvJoy1[8];
static UINT8 DrvInputs[1];
static UINT8 DrvDips[2];
static UINT8 DrvReset;
static INT32 DrvAnalogPort0 = 0;
static INT32 DrvAnalogPort1 = 0;
static INT32 DrvAnalogPort2 = 0;
static INT32 DrvAnalogPort3 = 0;

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo FoodfInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 start"	},
	A("P1 Stick X",     BIT_ANALOG_REL, &DrvAnalogPort0,"p1 x-axis"),
	A("P1 Stick Y",     BIT_ANALOG_REL, &DrvAnalogPort1,"p1 y-axis"),
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 start"	},
	A("P2 Stick X",     BIT_ANALOG_REL, &DrvAnalogPort2,"p2 x-axis"),
	A("P2 Stick Y",     BIT_ANALOG_REL, &DrvAnalogPort3,"p2 y-axis"),
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};
#undef A

STDINPUTINFO(Foodf)

static struct BurnDIPInfo FoodfDIPList[]=
{
	{0x0b, 0xff, 0xff, 0x00, NULL					},
	{0x0c, 0xff, 0xff, 0x80, NULL					},

	{0   , 0xfe, 0   ,    5, "Bonus Coins"			},
	{0x0b, 0x01, 0x07, 0x00, "None"					},
	{0x0b, 0x01, 0x07, 0x05, "1 for every 2"		},
	{0x0b, 0x01, 0x07, 0x02, "1 for every 4"		},
	{0x0b, 0x01, 0x07, 0x01, "1 for every 5"		},
	{0x0b, 0x01, 0x07, 0x06, "2 for every 4"		},

	{0   , 0xfe, 0   ,    2, "Coin A"				},
	{0x0b, 0x01, 0x08, 0x00, "1 Coin  1 Credits"	},
	{0x0b, 0x01, 0x08, 0x08, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x0b, 0x01, 0x30, 0x00, "1 Coin  1 Credits"	},
	{0x0b, 0x01, 0x30, 0x20, "1 Coin  4 Credits"	},
	{0x0b, 0x01, 0x30, 0x10, "1 Coin  5 Credits"	},
	{0x0b, 0x01, 0x30, 0x30, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x0b, 0x01, 0xc0, 0x80, "2 Coins 1 Credits"	},
	{0x0b, 0x01, 0xc0, 0x00, "1 Coin  1 Credits"	},
	{0x0b, 0x01, 0xc0, 0xc0, "1 Coin  2 Credits"	},
	{0x0b, 0x01, 0xc0, 0x40, "Free Play"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x0c, 0x01, 0x80, 0x00, "On"					},
	{0x0c, 0x01, 0x80, 0x80, "Off"					},
};

STDDIPINFO(Foodf)

static inline void update_interrupts()
{
	if (irq_vector)
		SekSetIRQLine(irq_vector, CPU_IRQSTATUS_ACK);
	else
		SekSetIRQLine(7, CPU_IRQSTATUS_NONE);
}

static void __fastcall foodf_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfffe00) == 0x900000) {
		DrvNVRAM[(address >> 1) & 0xff] = data;
		return;
	}

	if ((address & 0xffffe0) == 0xa40000) {
		pokey2_w((address & 0x1f) >> 1, data & 0xff);
		return;
	}

	if ((address & 0xffffe0) == 0xa80000) {
		pokey1_w((address & 0x1f) >> 1, data & 0xff);
		return;
	}

	if ((address & 0xffffe0) == 0xac0000) {
		pokey3_w((address & 0x1f) >> 1, data & 0xff);
		return;
	}

	switch (address & ~0x023ff8)
	{
		case 0x944000:
		case 0x944001:
		case 0x944002:
		case 0x944003:
		case 0x944004:
		case 0x944005:
		case 0x944006:
		case 0x944007:
			analog_select = (~address / 2) & 3;
		return;
	}

	switch (address)
	{
		case 0x948000:
			flipscreen = data & 0x01;
			if ((data & 0x04) == 0) {
				irq_vector &= ~1;
				update_interrupts();
			}
			if ((data & 0x08) == 0) {
				irq_vector &= ~2;
				update_interrupts();
			}
			// coin counters = data & 0xc0;
		return;

		case 0x954000:
			// nop
		return;

		case 0x958000:
			BurnWatchdogReset();
		return;
	}
}

static void __fastcall foodf_write_byte(UINT32 address, UINT8 data)
{
	foodf_write_word(address&~1, data);
}

static INT32 dip_read(INT32 offset)
{
	return ((DrvDips[0] >> (offset & 7))&1) << 7;
}

static UINT16 analog_read()
{
	INT32 analog[4] = { DrvAnalogPort0, DrvAnalogPort2, DrvAnalogPort1, DrvAnalogPort3 };
#if 0
	switch (analog_select) {
		case 0: bprintf(0, _T("p1 X: %02X\n"), ProcessAnalog(analog[analog_select], 1, 1, 0x00, 0xff)); break;
		//case 1: bprintf(0, _T("p2 X: %02X\n"), ProcessAnalog(analog[analog_select], 1, 1, 0x00, 0xff)); break;
		case 2: bprintf(0, _T("p1 Y: %02X\n"), ProcessAnalog(analog[analog_select], 1, 1, 0x00, 0xff)); break;
		//case 3: bprintf(0, _T("p2 Y: %02X\n"), ProcessAnalog(analog[analog_select], 1, 1, 0x00, 0xff)); break;
	}
#endif
	return ProcessAnalog(analog[analog_select], 1, 1, 0x00, 0xff);
}

static UINT8 __fastcall foodf_read_byte(UINT32 address)
{
	bprintf(0, _T("read byte %X\n"), address);
	return 0;
}

static UINT16 __fastcall foodf_read_word(UINT32 address)
{
	if ((address & 0xfffe00) == 0x900000) {
		return DrvNVRAM[(address / 2) & 0xff] | 0xfff0;
	}

	if ((address & 0xffffe0) == 0xa40000) {
		return pokey2_r((address & 0x1f) >> 1);
	}

	if ((address & 0xffffe0) == 0xa80000) {
		return pokey1_r((address & 0x1f) >> 1);
	}

	if ((address & 0xffffe0) == 0xac0000) {
		return pokey3_r((address & 0x1f) >> 1);
	}

	switch (address & ~0x023ff8)
	{
		case 0x940000:
		case 0x940001:
		case 0x940002:
		case 0x940003:
		case 0x940004:
		case 0x940005:
		case 0x940006:
		case 0x940007:
			return analog_read();
	}

	switch (address)
	{
		case 0x948000:
		case 0x948001:
			return (DrvInputs[0] & 0x7f) | (DrvDips[1] & 0x80);

		case 0x94c000:
		case 0x94c001:
			return 0; // nop

		case 0x958000:
		case 0x958001:
			return BurnWatchdogRead();
	}

	return 0;
}

static tilemap_callback( bg )
{
	UINT8 attr = DrvVidRAM[offs * 2 + 1];
	UINT16 code = DrvVidRAM[offs * 2 + 0] + ((attr & 0x80) << 1);

	TILE_SET_INFO(0, code, attr, flipscreen ? (TILE_FLIPX|TILE_FLIPY) : 0);
}

static INT32 DrvDoReset(INT32 full_reset)
{
	if (full_reset) {
		memset(AllRam, 0, RamEnd - AllRam);

		analog_select = 0;
		irq_vector = 0;
		flipscreen = 0;
	}

	SekOpen(0);
	SekReset();
	SekClose();

	BurnWatchdogReset();

	nExtraCycles = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += 0x008000;
	DrvGfxROM1		= Next; Next += 0x010000;

	DrvPalette		= (UINT32*)Next; Next += 0x100 * sizeof(UINT32);

	DrvNVRAM		= Next; Next += 0x000100;

	AllRam			= Next;

	Drv68KRAM		= Next; Next += 0x008000;
	DrvVidRAM		= Next; Next += 0x001000;
	DrvSprRAM		= Next; Next += 0x001000;
	DrvPalRAM		= Next; Next += 0x000400;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[2] = { 0, 4 };
	INT32 Plane1[2] = { 0x2000*8, 0 };
	INT32 XOffs0[8] = { STEP4(64,1), STEP4(0,1) };
	INT32 XOffs1[16]= { STEP8(128,1), STEP8(0,1) };
	INT32 YOffs[16] = { STEP16(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x4000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x2000);

	GfxDecode(0x0200, 2,  8,  8, Plane0, XOffs0, YOffs, 0x080, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x4000);

	GfxDecode(0x0100, 2, 16, 16, Plane1, XOffs1, YOffs, 0x100, tmp, DrvGfxROM1);

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
		if (BurnLoadRom(Drv68KROM + 0x000000,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x000001,  1, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x004000,  2, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x004001,  3, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x008000,  4, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x008001,  5, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x00c000,  6, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x00c001,  7, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  8, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x02000, 10, 1)) return 1;

		if (BurnLoadRom(DrvNVRAM + 0x0000000, 11, 1)) return 1;

		DrvGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x00ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,			0x014000, 0x01bfff, MAP_RAM);
	SekMapMemory(DrvSprRAM,			0x01c000, 0x01cfff, MAP_RAM); // 0-ff
	SekMapMemory(DrvVidRAM,			0x800000, 0x8007ff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x950000, 0x9503ff, MAP_RAM); // 0-1ff
	SekSetWriteWordHandler(0,		foodf_write_word);
	SekSetReadWordHandler(0,		foodf_read_word);
	SekSetWriteByteHandler(0,		foodf_write_byte);
	SekSetReadByteHandler(0,		foodf_read_byte);
	SekClose();

	BurnWatchdogInit(DrvDoReset, 180);

	PokeyInit(604800, 3, 1.00, 0);

	PokeyPotCallback(0, 0, dip_read);
	PokeyPotCallback(0, 1, dip_read);
	PokeyPotCallback(0, 2, dip_read);
	PokeyPotCallback(0, 3, dip_read);
	PokeyPotCallback(0, 4, dip_read);
	PokeyPotCallback(0, 5, dip_read);
	PokeyPotCallback(0, 6, dip_read);
	PokeyPotCallback(0, 7, dip_read);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_COLS, bg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 2, 8, 8, 0x8000, 0, 0x3f);
	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetScrollX(0, -8);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	SekExit();

	PokeyExit();

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteUpdate()
{
	for (INT32 i = 0; i < 0x100; i++)
	{
		UINT16 p = *((UINT16*)(DrvPalRAM + i * 2));

		INT32 bit0 = (p >> 0) & 1;
		INT32 bit1 = (p >> 1) & 1;
		INT32 bit2 = (p >> 2) & 1;
		UINT8 r = (bit2 * 15089 + bit1 * 7091 + bit0 * 3320) / 100;

		bit0 = (p >> 3) & 1;
		bit1 = (p >> 4) & 1;
		bit2 = (p >> 5) & 1;
		UINT8 g = (bit2 * 15089 + bit1 * 7091 + bit0 * 3320) / 100;

		bit0 = (p >> 6) & 1;
		bit1 = (p >> 7) & 1;
		UINT8 b = (bit1 * 17370 + bit0 * 8130) / 100;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_sprites()
{
	UINT16 *ram = (UINT16*)DrvSprRAM;

	for (INT32 offs = 0x80-2; offs >= 0x20; offs -= 2)
	{
		UINT16 data1 = ram[offs];
		UINT16 data2 = ram[offs+1];

		INT32 code	= (data1 & 0xff);
		INT32 color	= (data1 >> 8) & 0x1f;
		INT32 sx	= (data2 >> 8) & 0xff;
		INT32 sy	= (0xff - data2 - 16) & 0xff;
		INT32 flipx	= (data1 & 0x8000);
		INT32 flipy	= (data1 & 0x4000);
		INT32 pri	= (data1 >> 12) & 2;

		if (flipscreen)
		{
			sy = (224 - 16) - sy;
			sx = (256 - 16) - sx;
			flipx ^= 0x8000;
			flipy ^= 0x4000;
		}

		RenderPrioSprite(pTransDraw, DrvGfxROM1, code, color << 2, 0, sx,       sy, flipx, flipy, 16, 16, pri);
		RenderPrioSprite(pTransDraw, DrvGfxROM1, code, color << 2, 0, sx - 256, sy, flipx, flipy, 16, 16, pri);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1; // force update
	}

	BurnTransferClear();

	GenericTilemapDraw(0, pTransDraw, 0 | TMAP_FORCEOPAQUE);
	GenericTilemapDraw(0, pTransDraw, 1);

	draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	{
		DrvInputs[0] = 0xff;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
		}
	}

	INT32 nInterleave = 259; // really
	INT32 nCyclesTotal[1] = { 6048000 / 60 };
	INT32 nCyclesDone[1] = { nExtraCycles };

	SekOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone[0] += SekRun((nCyclesTotal[0] * (i + 1) / nInterleave) - nCyclesDone[0]);

		if ((i & 0x3f) == 0 && i <= 192) {
			irq_vector |= 1;
			update_interrupts();
		}
		if (i == 224) {
			irq_vector |= 2;
			update_interrupts();
		}
	}

	SekClose();

	nExtraCycles = nCyclesDone[0] - nCyclesTotal[0];

	if (pBurnSoundOut) {
		pokey_update(pBurnSoundOut, nBurnSoundLen);
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

		SekScan(nAction);

		pokey_scan(nAction, pnMin);

		BurnWatchdogScan(nAction);

		SCAN_VAR(analog_select);
		SCAN_VAR(irq_vector);
		SCAN_VAR(flipscreen);
		SCAN_VAR(nExtraCycles);
	}

	if (nAction & ACB_NVRAM) {
		ba.Data		= DrvNVRAM;
		ba.nLen		= 0x00400;
		ba.nAddress	= 0;
		ba.szName	= "NV RAM";
		BurnAcb(&ba);
	}

	return 0;
}


// Food Fight (rev 3)

static struct BurnRomInfo foodfRomDesc[] = {
	{ "136020-301.8c",	0x2000, 0xdfc3d5a8, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "136020-302.9c",	0x2000, 0xef92dc5c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136020-303.8d",	0x2000, 0x64b93076, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136020-204.9d",	0x2000, 0xea596480, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136020-305.8e",	0x2000, 0xe6cff1b1, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136020-306.9e",	0x2000, 0x95159a3e, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136020-307.8f",	0x2000, 0x17828dbb, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136020-208.9f",	0x2000, 0x608690c9, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "136020-109.6lm",	0x2000, 0xc13c90eb, 2 | BRF_GRA },           //  8 Background Tiles

	{ "136020-110.4e",	0x2000, 0x8870e3d6, 3 | BRF_GRA },           //  9 Sprites
	{ "136020-111.4d",	0x2000, 0x84372edf, 3 | BRF_GRA },           // 10

	{ "foodf.nv",		0x0100, 0xa4186b13, 4 | BRF_PRG },           // 11 NV RAM Settings

	{ "136020-112.2p",	0x0100, 0x0aa962d6, 0 | BRF_OPT },           // 12 PROM (not used)
};

STD_ROM_PICK(foodf)
STD_ROM_FN(foodf)

struct BurnDriver BurnDrvFoodf = {
	"foodf", NULL, NULL, NULL, "1982",
	"Food Fight (rev 3)\0", NULL, "General Computer Corporation (Atari license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, foodfRomInfo, foodfRomName, NULL, NULL, NULL, NULL, FoodfInputInfo, FoodfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 224, 4, 3
};


// Food Fight (rev 2)

static struct BurnRomInfo foodf2RomDesc[] = {
	{ "136020-201.8c",	0x2000, 0x4ee52d73, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "136020-202.9c",	0x2000, 0xf8c4b977, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136020-203.8d",	0x2000, 0x0e9f99a3, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136020-104.9d",	0x2000, 0xf667374c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136020-205.8e",	0x2000, 0x1edd05b5, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136020-206.9e",	0x2000, 0xbb8926b3, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136020-207.8f",	0x2000, 0xc7383902, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136020-208.9f",	0x2000, 0x608690c9, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "136020-109.6lm",	0x2000, 0xc13c90eb, 2 | BRF_GRA },           //  8 Background Tiles

	{ "136020-110.4e",	0x2000, 0x8870e3d6, 3 | BRF_GRA },           //  9 Sprites
	{ "136020-111.4d",	0x2000, 0x84372edf, 3 | BRF_GRA },           // 10

	{ "foodf.nv",		0x0100, 0xa4186b13, 4 | BRF_PRG },           // 11 NV RAM Settings

	{ "136020-112.2p",	0x0100, 0x0aa962d6, 0 | BRF_OPT },           // 12 PROM (not used)
};

STD_ROM_PICK(foodf2)
STD_ROM_FN(foodf2)

struct BurnDriver BurnDrvFoodf2 = {
	"foodf2", "foodf", NULL, NULL, "1982",
	"Food Fight (rev 2)\0", NULL, "General Computer Corporation (Atari license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, foodf2RomInfo, foodf2RomName, NULL, NULL, NULL, NULL, FoodfInputInfo, FoodfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 224, 4, 3
};


// Food Fight (rev 1)

static struct BurnRomInfo foodf1RomDesc[] = {
	{ "136020-101.8c",	0x2000, 0x06d0ede0, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "136020-102.9c",	0x2000, 0xca6390a4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136020-103.8d",	0x2000, 0x36e89e89, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136020-104.9d",	0x2000, 0xf667374c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136020-105.8e",	0x2000, 0xa8c22e50, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136020-106.9e",	0x2000, 0x13e013c4, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136020-107.8f",	0x2000, 0x8a3f7ca4, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136020-108.9f",	0x2000, 0xd4244e12, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "136020-109.6lm",	0x2000, 0xc13c90eb, 2 | BRF_GRA },           //  8 Background Tiles

	{ "136020-110.4e",	0x2000, 0x8870e3d6, 3 | BRF_GRA },           //  9 Sprites
	{ "136020-111.4d",	0x2000, 0x84372edf, 3 | BRF_GRA },           // 10

	{ "foodf.nv",		0x0100, 0xa4186b13, 4 | BRF_PRG },           // 11 NV RAM Settings

	{ "136020-112.2p",	0x0100, 0x0aa962d6, 0 | BRF_OPT },           // 12 PROM (not used)
};

STD_ROM_PICK(foodf1)
STD_ROM_FN(foodf1)

struct BurnDriver BurnDrvFoodf1 = {
	"foodf1", "foodf", NULL, NULL, "1982",
	"Food Fight (rev 1)\0", NULL, "General Computer Corporation (Atari license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, foodf1RomInfo, foodf1RomName, NULL, NULL, NULL, NULL, FoodfInputInfo, FoodfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 224, 4, 3
};


// Food Fight (cocktail)

static struct BurnRomInfo foodfcRomDesc[] = {
	{ "136020-113.8c",	0x2000, 0x193a299f, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "136020-114.9c",	0x2000, 0x33ed6bbe, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136020-115.8d",	0x2000, 0x64b93076, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136020-116.9d",	0x2000, 0xea596480, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136020-117.8e",	0x2000, 0x12a55db6, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136020-118.9e",	0x2000, 0xe6d451d4, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136020-119.8f",	0x2000, 0x455cc891, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136020-120.9f",	0x2000, 0x34173910, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "136020-109.6lm",	0x2000, 0xc13c90eb, 2 | BRF_GRA },           //  8 Background Tiles

	{ "136020-110.4e",	0x2000, 0x8870e3d6, 3 | BRF_GRA },           //  9 Sprites
	{ "136020-111.4d",	0x2000, 0x84372edf, 3 | BRF_GRA },           // 10

	{ "foodfc.nv",		0x0100, 0xc1385dab, 4 | BRF_PRG },           // 11 NV RAM Settings

	{ "136020-112.2p",	0x0100, 0x0aa962d6, 0 | BRF_OPT },           // 12 PROM (not used)
};

STD_ROM_PICK(foodfc)
STD_ROM_FN(foodfc)

struct BurnDriver BurnDrvFoodfc = {
	"foodfc", "foodf", NULL, NULL, "1982",
	"Food Fight (cocktail)\0", NULL, "General Computer Corporation (Atari license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, foodfcRomInfo, foodfcRomName, NULL, NULL, NULL, NULL, FoodfInputInfo, FoodfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 224, 4, 3
};
