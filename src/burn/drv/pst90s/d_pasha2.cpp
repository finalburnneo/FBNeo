// FinalBurn Neo Dong Sung Pasha Pasha 2 driver module
// Based on MAME driver by Pierpaolo Prazzoli

#include "tiles_generic.h"
#include "e132xs_intf.h"
#include "msm6295.h"
#include "burn_pal.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvMainROM;
static UINT8 *DrvBootROM;
static UINT8 *DrvAT89C52ROM;
static UINT8 *DrvSndROM[3];
static UINT8 *DrvMainRAM;
static UINT8 *DrvVidRAM[2][2];

static UINT8 DrvRecalc;

static INT32 rombank;
static INT32 okibank[2];
static INT32 vidrambank;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvDips[2];
static UINT32 DrvInputs[2];
static UINT8 DrvReset;

static struct BurnInputInfo Pasha2InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 8,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 9,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"	},

	{"P3 Coin",			BIT_DIGITAL,	DrvJoy1 + 10,	"p3 coin"	},
	{"P3 Start",		BIT_DIGITAL,	DrvJoy2 + 15,	"p3 start"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy2 + 12,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy2 + 13,	"p3 fire 2"	},
	{"P3 Button 3",		BIT_DIGITAL,	DrvJoy2 + 14,	"p3 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Pasha2)

static struct BurnDIPInfo Pasha2DIPList[]=
{
	{0x10, 0xff, 0xff, 0xef, NULL					},
	{0x11, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x10, 0x01, 0x18, 0x18, "1"					},
	{0x10, 0x01, 0x18, 0x10, "2"					},
	{0x10, 0x01, 0x18, 0x08, "3"					},
	{0x10, 0x01, 0x18, 0x00, "5"					},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x10, 0x01, 0x60, 0x00, "5 Coins 1 Credits"	},
	{0x10, 0x01, 0x60, 0x20, "3 Coins 1 Credits"	},
	{0x10, 0x01, 0x60, 0x40, "2 Coins 1 Credits"	},
	{0x10, 0x01, 0x60, 0x60, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    2, "Free Play"			},
	{0x10, 0x01, 0x80, 0x80, "Off"					},
	{0x10, 0x01, 0x80, 0x00, "On"					},
};

STDDIPINFO(Pasha2)

static void set_rombank(INT32 data)
{
	if ((data & 0x0800) == 0 || (data & 0x8000) == 0) return;

	rombank = data;

	INT32 bank = (data >> 12) & 7;
	if (bank > 2) bank = 2;

	E132XSMapMemory(DrvMainROM + (bank * 0x400000),			0x80000000, 0x803fffff, MAP_ROM);
}

static void set_vidrambank(INT32 bank)
{
	vidrambank = bank;

	E132XSMapMemory(DrvVidRAM[0][bank],		0x40000000, 0x4001ffff, MAP_RAM);
	E132XSMapMemory(DrvVidRAM[1][bank],		0x40020000, 0x4003ffff, MAP_ROM);
}

static void set_okibank(INT32 select, INT32 data)
{
	okibank[select] = data;

	MSM6295SetBank(select, DrvSndROM[select] + (data & 1) * 0x40000, 0, 0x3ffff);
}

static void pasha2_write_long(UINT32 address, UINT32 data)
{
	if ((address & 0xfffe0000) == 0x40020000) {
		UINT32 x = BURN_ENDIAN_SWAP_INT32(*((UINT32*)(DrvVidRAM[1][vidrambank] + (address & 0x1fffc))));
		UINT32 xm = 0;
	
		data = (data << 16) | (data >> 16);

		if ((data & 0xff000000) == 0xff000000) xm |= 0xff000000;
		if ((data & 0x00ff0000) == 0x00ff0000) xm |= 0x00ff0000;
		if ((data & 0x0000ff00) == 0x0000ff00) xm |= 0x0000ff00;
		if ((data & 0x000000ff) == 0x000000ff) xm |= 0x000000ff;
		UINT32 dm = xm ^ 0xffffffff;
		*((UINT32*)(DrvVidRAM[1][vidrambank] + (address & 0x1fffc))) = BURN_ENDIAN_SWAP_INT32((x & xm) | (data & dm));
		return;
	}

	switch (address)
	{
		case 0x40070000: // vbuff clear
		case 0x40074000: // vbuff set
			set_vidrambank((address >> 14) & 1);
		return;
	}
}

static void pasha2_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfffe0000) == 0x40020000) {
		UINT16 x = BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvVidRAM[1][vidrambank] + (address & 0x1fffe))));
		UINT16 xm = 0;
		if ((data & 0x0000ff00) == 0x0000ff00) xm |= 0x0000ff00;
		if ((data & 0x000000ff) == 0x000000ff) xm |= 0x000000ff;
		UINT32 dm = xm ^ 0xffff;
		*((UINT16*)(DrvVidRAM[1][vidrambank] + (address & 0x1fffe))) = BURN_ENDIAN_SWAP_INT16((x & xm) | (data & dm));
		return;
	}

	switch (address & ~3)
	{
		case 0x40070000: // vbuff clear
		case 0x40074000: // vbuff set
			set_vidrambank((address >> 14) & 1);
		return;
	}
}

static void pasha2_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xfffe0000) == 0x40020000) {
		if (data != 0xff) DrvVidRAM[1][vidrambank][((address & 0x1ffff) ^ 1)] = data; 
		return;
	}

	switch (address & ~3)
	{
		case 0x40070000: // vbuff clear
		case 0x40074000: // vbuff set
			set_vidrambank((address >> 14) & 1);
		return;
	}
}

static void pasha2_io_write(UINT32 address, UINT32 data)
{
	switch (address)
	{
		case 0x20:
			// lamps
		return;

		case 0xc0:
			set_rombank(data);
		return;

		case 0xe0:
			MSM6295Write(0, data);
		return;

		case 0xe4:
			MSM6295Write(1, data);
		return;

		case 0xe8:
			set_okibank(0, data);
		return;

		case 0xec:
			set_okibank(1, data);
		return;
	}
}

static UINT32 pasha2_io_read(UINT32 address)
{
	switch (address)
	{
		case 0x40:
			return DrvInputs[0];

		case 0x60:
			return DrvDips[0] + (DrvDips[1] << 8);

		case 0x80:
			return DrvInputs[1];

		case 0xe0:
			return MSM6295Read(0);

		case 0xe4:
			return MSM6295Read(1);
	}

	return 0;
}

static inline void do_speedhack(UINT32 address)
{
	E132XSBurnCycles(1600); // wait states....:)
	if (address == 0x95744) {
		if (E132XSGetPC(0) == 0x8302) {
			E132XSBurnUntilInt(); // kill cpu until interrupt
		}
	}
}

static UINT32 pasha2_read_long(UINT32 address)
{
	if (address < 0x200000) {
		do_speedhack(address);
		UINT32 ret = BURN_ENDIAN_SWAP_INT32(*((UINT32*)(DrvMainRAM + address)));
		return (ret << 16) | (ret >> 16);
	}

	return 0;
}

static UINT16 pasha2_read_word(UINT32 address)
{
	if (address < 0x200000) {
		do_speedhack(address);
		return BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvMainRAM + address)));
	}

	return 0;
}

static UINT8 pasha2_read_byte(UINT32 address)
{
	if (address < 0x200000) {
		do_speedhack(address);
		return DrvMainRAM[address^1];
	}

	return 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	E132XSOpen(0);
	set_vidrambank(0);
	set_rombank(0);
	E132XSReset();
	E132XSClose();

	set_okibank(0,0);
	set_okibank(1,0);
	MSM6295Reset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvBootROM		= Next; Next += 0x080000;
	DrvMainROM		= Next; Next += 0xc00000;

	DrvAT89C52ROM	= Next; Next += 0x002000;

	DrvSndROM[0]	= Next; Next += 0x080000;
	DrvSndROM[1]	= Next; Next += 0x080000;
	DrvSndROM[2]	= Next; Next += 0x080000;

	BurnPalette		= (UINT32*)Next; Next += 0x200 * sizeof(UINT32);

	AllRam			= Next;

	DrvMainRAM		= Next; Next += 0x200000;

	DrvVidRAM[0][0]	= Next; Next += 0x020000;
	DrvVidRAM[0][1]	= Next; Next += 0x020000;
	DrvVidRAM[1][0]	= Next; Next += 0x020000;
	DrvVidRAM[1][1]	= Next; Next += 0x020000;

	BurnPalRAM		= Next; Next += 0x000400;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRomExt(DrvBootROM + 0x0000000, k++, 1, LD_BYTESWAP)) return 1;

		if (BurnLoadRomExt(DrvMainROM + 0x0000001, k++, 2, 0)) return 1;
		if (BurnLoadRomExt(DrvMainROM + 0x0000000, k++, 2, 0)) return 1;
		if (BurnLoadRomExt(DrvMainROM + 0x0400001, k++, 2, 0)) return 1;
		if (BurnLoadRomExt(DrvMainROM + 0x0400000, k++, 2, 0)) return 1;
		memset (DrvMainROM + 0x800000, 0xff, 0x400000);

		if (BurnLoadRomExt(DrvAT89C52ROM + 0x000000, k++, 1, 0)) return 1;

		if (BurnLoadRomExt(DrvSndROM[2] + 0x0000000, k++, 1, 0)) return 1;

		if (BurnLoadRomExt(DrvSndROM[0] + 0x0000000, k++, 1, 0)) return 1;

		if (BurnLoadRomExt(DrvSndROM[1] + 0x0000000, k++, 1, 0)) return 1;
	}

	E132XSInit(0, TYPE_E116XT, 80000000);
	E132XSOpen(0);
	E132XSMapMemory(DrvMainRAM,			0x00000000, 0x001fffff, MAP_RAM);
	E132XSMapMemory(DrvVidRAM[0][0],	0x40000000, 0x4001ffff, MAP_RAM);
	E132XSMapMemory(DrvVidRAM[1][0],	0x40020000, 0x4003ffff, MAP_ROM);
	E132XSMapMemory(DrvMainROM,			0x80000000, 0x803fffff, MAP_ROM);
	E132XSMapMemory(BurnPalRAM,			0xe0000000, 0xe00003ff, MAP_RAM);
	E132XSMapMemory(DrvBootROM,			0xfff80000, 0xffffffff, MAP_ROM);
	E132XSSetWriteLongHandler(pasha2_write_long);
	E132XSSetWriteWordHandler(pasha2_write_word);
	E132XSSetWriteByteHandler(pasha2_write_byte);
	E132XSSetIOWriteHandler(pasha2_io_write);
	E132XSSetIOReadHandler(pasha2_io_read);

	// Speed hack
	E132XSMapMemory(NULL,				0x00095744 & ~0xfff, 0x00095744 | 0xfff, MAP_ROM); // unmap
	E132XSSetReadLongHandler(pasha2_read_long);
	E132XSSetReadWordHandler(pasha2_read_word);
	E132XSSetReadByteHandler(pasha2_read_byte);
	E132XSClose();

	MSM6295Init(0, 1000000 / MSM6295_PIN7_HIGH, 0);
	MSM6295Init(1, 1000000 / MSM6295_PIN7_HIGH, 0);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);
	MSM6295SetRoute(1, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	E132XSExit();

	MSM6295Exit();

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteUpdate()
{
	UINT16 *p = (UINT16*)BurnPalRAM;
	
	for (INT32 i = 0; i < 0x100; i++)
	{
		UINT16 color = (BURN_ENDIAN_SWAP_INT16(p[i]) >> 8) | (BURN_ENDIAN_SWAP_INT16(p[i + 0x100]) & 0xff00);
	
		BurnPalette[i * 2 + 0] = BurnHighCol(pal5bit(color), pal5bit(color >> 5), pal5bit(color >> 10), 0);

		color = (BURN_ENDIAN_SWAP_INT16(p[i]) & 0xff) | (BURN_ENDIAN_SWAP_INT16(p[i + 0x100]) << 8);
	
		BurnPalette[i * 2 + 1] = BurnHighCol(pal5bit(color), pal5bit(color >> 5), pal5bit(color >> 10), 0);
	}
}

static void draw_bitmap()
{
	UINT8 *src0 = DrvVidRAM[0][vidrambank ^ 1];
	UINT8 *src1 = DrvVidRAM[1][vidrambank ^ 1];
	UINT16 *dst = pTransDraw;

	for (INT32 y = 0; y < nScreenHeight; y++)
	{
		for (INT32 x = 0; x < nScreenWidth; x++)
		{
			INT32 pxl = src1[x ^ 1];
			if (pxl) {
				dst[x] = pxl;
			} else {
				dst[x] = src0[x ^ 1] | 0x100;
			}
		}

		src0 += 512;
		src1 += 512;
		dst += nScreenWidth;
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1; // force update
	}

	draw_bitmap();

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

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 10;
	INT32 nCyclesTotal[1] = { 80000000 / 60 };
	INT32 nCyclesDone[1] = { 0 };

	E132XSOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, E132XS);
	}

	E132XSSetIRQLine(0, CPU_IRQSTATUS_HOLD);
	E132XSClose();

	if (pBurnSoundOut) {
		MSM6295Render(pBurnSoundOut, nBurnSoundLen);
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
		E132XSScan(nAction);

		MSM6295Scan(nAction, pnMin);

		SCAN_VAR(okibank);
		SCAN_VAR(vidrambank);
		SCAN_VAR(rombank);
	}

	if (nAction & ACB_WRITE) {
		set_vidrambank(vidrambank);
		set_rombank(rombank);
		set_okibank(0, okibank[0]);
		set_okibank(1, okibank[1]);
	}

	return 0;
}


// Pasha Pasha 2

static struct BurnRomInfo pasha2RomDesc[] = {
	{ "pp2.u3",			0x080000, 0x1c701273, 1 | BRF_PRG | BRF_ESS }, //  0 E116XT Boot Code

	{ "pp2-u2.u101",	0x200000, 0x85c4a2d0, 2 | BRF_PRG | BRF_ESS }, //  1 E116XT Main Code (Banked)
	{ "pp2-u1.u101",	0x200000, 0x96cbd04e, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "pp2-u2.u102",	0x200000, 0x2097d88c, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "pp2-u1.u102",	0x200000, 0x7a3492fb, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "89c52.bin",		0x002000, 0x9ce43ce4, 3 | BRF_PRG | BRF_ESS }, //  5 AT89C52 Code

	{ "pp2.um2",		0x080000, 0x86814b37, 4 | BRF_SND },           //  6 SAM9773 Audio Data

	{ "pp2.um51",		0x080000, 0x3b1b1a30, 5 | BRF_SND },           //  7 MSM6295 Chip#0 Samples

	{ "pp2.um53",		0x080000, 0x8a29ad03, 6 | BRF_SND },           //  8 MSM6295 Chip#1 Samples
};

STD_ROM_PICK(pasha2)
STD_ROM_FN(pasha2)

struct BurnDriver BurnDrvPasha2 = {
	"pasha2", NULL, NULL, NULL, "1998",
	"Pasha Pasha 2\0", "Incomplete sound", "Dong Sung", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MINIGAMES, 0,
	NULL, pasha2RomInfo, pasha2RomName, NULL, NULL, NULL, NULL, Pasha2InputInfo, Pasha2DIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	384, 240, 4, 3
};
