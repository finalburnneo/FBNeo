// Eolith 32 bits hardware: Vega system
// driver by Pierpaolo Prazzoli

#include "tiles_generic.h"
#include "e132xs_intf.h"
#include "mcs51.h"
#include "qs1000.h"
#include "burn_pal.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvMainROM;
static UINT8 *DrvBootROM;
static UINT8 *DrvQSROM;
static UINT8 *DrvSndROM;
static UINT8 *DrvNVRAM;
static UINT8 *DrvMainRAM;
static UINT8 *DrvVidRAM;

static UINT8 DrvRecalc;

static INT32 soundlatch;
static INT32 vidrambank;
static INT32 vblank;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT32 DrvInputs[2];
static UINT8 DrvReset;

static struct BurnInputInfo CrazywarInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 4,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy2 + 5,	"diag"		},
};

STDINPUTINFO(Crazywar)

static inline void DrvMCUSync()
{
	INT32 todo = ((double)E132XSTotalCycles() * (24000000 / 12) / 55000000) - mcs51TotalCycles();

	if (todo > 0) mcs51Run(todo);
}

static void vega_write_long(UINT32 address, UINT32 data)
{
	if ((address & 0xffffff00) == 0xfc000000) {
		DrvNVRAM[(address & 0xfc) / 4] = data;
		return;
	}

	if ((address & 0xfffffc00) == 0xfc200000) {
		*((UINT16*)(BurnPalRAM + ((address & 0x3fc) / 2))) = BURN_ENDIAN_SWAP_INT16(data & 0xffff);
		return;
	}

	if (address >= 0x80000000 && address <= 0x80013fff) {
		UINT32 x = BURN_ENDIAN_SWAP_INT32(*((UINT32*)(DrvVidRAM + vidrambank + (address & 0x1fffc))));
		UINT32 xm = 0;

		data = (data << 16) | (data >> 16);

		if ((data & 0xff000000) == 0xff000000) xm |= 0xff000000;
		if ((data & 0x00ff0000) == 0x00ff0000) xm |= 0x00ff0000;
		if ((data & 0x0000ff00) == 0x0000ff00) xm |= 0x0000ff00;
		if ((data & 0x000000ff) == 0x000000ff) xm |= 0x000000ff;
		UINT32 dm = xm ^ 0xffffffff;
		*((UINT32*)(DrvVidRAM + vidrambank + (address & 0x1fffc))) = BURN_ENDIAN_SWAP_INT32((x & xm) | (data & dm));
		return;
	}

	switch (address)
	{
		case 0xfc600000:
			DrvMCUSync();
			soundlatch = data & 0xff;
			qs1000_set_irq(1);
		return;

		case 0xfca00000:
			vidrambank = (data & 1) * 0x14000;
		return;
	}
}

static void vega_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xffffff00) == 0xfc000000) {
		DrvNVRAM[(address & 0xfc) / 4] = data;
		return;
	}

	if ((address & 0xfffffc00) == 0xfc200000) {
		*((UINT16*)(BurnPalRAM + ((address & 0x3fc) / 2))) = BURN_ENDIAN_SWAP_INT16(data & 0xffff);
		return;
	}

	if (address >= 0x80000000 && address <= 0x80013fff) {
		UINT16 x = BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvVidRAM + vidrambank + (address & 0x1fffe))));
		UINT16 xm = 0;
		if ((data & 0x0000ff00) == 0x0000ff00) xm |= 0x0000ff00;
		if ((data & 0x000000ff) == 0x000000ff) xm |= 0x000000ff;
		UINT32 dm = xm ^ 0xffff;
		x = (x & xm) | (data & dm);
		return;
	}

	switch (address & ~3)
	{
		case 0xfc600000:
			DrvMCUSync();
			soundlatch = data & 0xff;
			qs1000_set_irq(1);
		return;

		case 0xfca00000:
			vidrambank = (data & 1) * 0x14000;
		return;
	}
}

static void vega_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xffffff00) == 0xfc000000) {
		DrvNVRAM[(address & 0xfc) / 4] = data;
		return;
	}

	if ((address & 0xfffffc00) == 0xfc200000) {
		BurnPalRAM[((address & 0x3fc) / 2) | (address & 1)] = data;
		return;
	}

	if (address >= 0x80000000 && address <= 0x80013fff) {
		if (data != 0xff) DrvVidRAM[vidrambank + ((address & 0x1ffff) ^ 1)] = data;
		return;
	}

	switch (address & ~3)
	{
		case 0xfc600000:
			DrvMCUSync();
			soundlatch = data & 0xff;
			qs1000_set_irq(1);
		return;

		case 0xfca00000:
			vidrambank = (data & 1) * 0x14000;
		return;
	}
}

static UINT32 vega_read_long(UINT32 address)
{
	if ((address & 0xffffff00) == 0xfc000000) {
		return DrvNVRAM[(address & 0xfc) / 4];
	}

	if ((address & 0xfffffc00) == 0xfc200000) {
		return BURN_ENDIAN_SWAP_INT16(*((UINT16*)(BurnPalRAM + ((address & 0x3fc) / 2))));
	}

	if (address >= 0x80000000 && address <= 0x80013fff) {
		return DrvVidRAM[vidrambank + ((address & 0x1fffc)/4)];
	}

	switch (address)
	{
		case 0xfcc00000:
			if (vblank == 0 && E132XSGetPC(0) == 0x8cf8) {
				E132XSBurnCycles(100);
			}
			return (DrvInputs[1] & ~0x40) | (vblank ? 0 : 0x40);

		case 0xfce00000:
			return DrvInputs[0];
	}

	return 0;
}

static UINT16 vega_read_word(UINT32 address)
{
	if ((address & 0xffffff00) == 0xfc000000) {
		return DrvNVRAM[(address & 0xfc) / 4];
	}

	if ((address & 0xfffffc00) == 0xfc200000) {
		return BURN_ENDIAN_SWAP_INT16(*((UINT16*)(BurnPalRAM + ((address & 0x3fc) / 2))));
	}

	if (address >= 0x80000000 && address <= 0x80013fff) {
		return DrvVidRAM[vidrambank + ((address & 0x1fffc)/4)];
	}

	switch (address & ~3)
	{
		case 0xfcc00000:
			if (vblank == 0 && E132XSGetPC(0) == 0x8cf8) {
				E132XSBurnCycles(100);
			}
			return (DrvInputs[1] & ~0x40) | (vblank ? 0 : 0x40);

		case 0xfce00000:
			return DrvInputs[0];
	}

	return 0;
}

static UINT8 vega_read_byte(UINT32 address)
{
	if ((address & 0xffffff00) == 0xfc000000) {
		return DrvNVRAM[(address & 0xfc) / 4];
	}

	if ((address & 0xfffffc00) == 0xfc200000) {
		return BurnPalRAM[((address & 0x3fc) / 2) | (address & 1)];
	}

	if (address >= 0x80000000 && address <= 0x80013fff) {
		return DrvVidRAM[vidrambank + ((address & 0x1fffc)/4)];
	}

	switch (address)
	{
		case 0xfcc00000:
			if (vblank == 0 && E132XSGetPC(0) == 0x8cf8) {
				E132XSBurnCycles(100);
			}
			return (DrvInputs[1] & ~0x40) | (vblank ? 0x40 : 0);

		case 0xfce00000:
			return DrvInputs[0];
	}

	return 0;
}

static void qs1000_p3_write(UINT8 data)
{
	qs1000_set_bankedrom(DrvQSROM + (data & 7) * 0x10000);

	if (~data & 0x20)
		qs1000_set_irq(0);
}

static UINT8 qs1000_p1_read()
{
	return soundlatch & 0xff;
}

static void DrvInitNVRAM()
{
	UINT8 nvdef[] = {
		0x43,0x72,0x61,0x7a,0x79,0x20,0x57,0x61,0x72,0x20,0x62,0x79,0x20,0x53,0x68,0x69,
		0x6e,0x20,0x42,0x6f,0x6e,0x67,0x4b,0x65,0x75,0x6e,0x00,0x02,0x00,0x01,0x01,0x00
	};

	memcpy(DrvNVRAM, nvdef, 0x20);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	E132XSOpen(0);
	E132XSReset();
	E132XSClose();

	qs1000_reset();

	vidrambank = 0;
	soundlatch = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvMainROM	= Next; Next += 0x2000000;
	DrvBootROM	= Next; Next += 0x080000;

	DrvQSROM	= Next; Next += 0x080000;

	DrvSndROM	= Next; Next += 0x1000000;

	DrvNVRAM	= Next; Next += 0x000040;

	BurnPalette	= (UINT32*)Next; Next += 0x100 * sizeof(UINT32);

	AllRam		= Next;

	DrvMainRAM	= Next; Next += 0x200000;
	DrvVidRAM	= Next; Next += 0x028000;
	BurnPalRAM	= Next; Next += 0x000200;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRomExt(DrvBootROM + 0x0000000, k++, 1, LD_BYTESWAP)) return 1;

		if (BurnLoadRomExt(DrvMainROM + 0x0000000, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvMainROM + 0x0000002, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvMainROM + 0x0400000, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvMainROM + 0x0400002, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvMainROM + 0x0800000, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvMainROM + 0x0800002, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvMainROM + 0x0c00000, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvMainROM + 0x0c00002, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvMainROM + 0x1000000, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvMainROM + 0x1000002, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvMainROM + 0x1400000, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvMainROM + 0x1400002, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvMainROM + 0x1800000, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvMainROM + 0x1800002, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvMainROM + 0x1c00000, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvMainROM + 0x1c00002, k++, 4, LD_GROUP(2))) return 1;

		if (BurnLoadRomExt(DrvQSROM   + 0x0000000, k++, 1, 0)) return 1;

		if (BurnLoadRomExt(DrvSndROM  + 0x0000000, k++, 1, 0)) return 1;
		if (BurnLoadRomExt(DrvSndROM  + 0x0200000, k++, 1, 0)) return 1;
	}

	E132XSInit(0, TYPE_GMS30C2132, 55000000);
	E132XSOpen(0);
	E132XSMapMemory(DrvMainRAM,			0x00000000, 0x001fffff, MAP_RAM);
//	E132XSMapMemory(DrvVidRAM,			0x80000000, 0x80013fff, MAP_RAM); // banked
//	E132XSMapMemory(BurnPalRAM,			0xfc200000, 0xfc2003ff, MAP_RAM); // palette &= 0xffff
	E132XSMapMemory(DrvMainROM,			0xfd000000, 0xfeffffff, MAP_ROM);
	E132XSMapMemory(DrvBootROM,			0xfff80000, 0xffffffff, MAP_ROM);
	E132XSSetWriteLongHandler(vega_write_long);
	E132XSSetWriteWordHandler(vega_write_word);
	E132XSSetWriteByteHandler(vega_write_byte);
	E132XSSetReadLongHandler(vega_read_long);
	E132XSSetReadWordHandler(vega_read_word);
	E132XSSetReadByteHandler(vega_read_byte);
	E132XSClose();

	qs1000_init(DrvQSROM, DrvSndROM, 0x1000000);
	qs1000_set_write_handler(3, qs1000_p3_write);
	qs1000_set_read_handler(1, qs1000_p1_read);
	qs1000_set_volume(0.75);

	GenericTilesInit();

	DrvDoReset();
	DrvInitNVRAM();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	E132XSExit();

	qs1000_exit();

	BurnFreeMemIndex();

	return 0;
}

static void draw_bitmap()
{
	UINT8 *ram = DrvVidRAM + (vidrambank ^ 0x14000);

	for (INT32 offs = 0; offs < 240 * 320; offs++)
	{
		pTransDraw[offs] = ram[offs ^ 1];
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		BurnPaletteUpdate_xRRRRRGGGGGBBBBB();
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

	E132XSNewFrame();
	mcs51NewFrame();

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 262;
	INT32 nCyclesTotal[2] = { 55000000 / 60, 24000000 / 12 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	E132XSOpen(0);
	mcs51Open(0);

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, E132XS);

		CPU_RUN_SYNCINT(1, mcs51);

		if (i == 239) vblank = 1;
	}

	if (pBurnSoundOut) {
		qs1000_update(pBurnSoundOut, nBurnSoundLen);
	}

	mcs51Close();
	E132XSClose();

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
		mcs51_scan(nAction);

		qs1000_scan(nAction, pnMin);

		SCAN_VAR(vidrambank);
		SCAN_VAR(soundlatch);
	}

	if (nAction & ACB_NVRAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = DrvNVRAM;
		ba.nLen	  = 0x40;
		ba.szName = "NV RAM";
		BurnAcb(&ba);
	}

	return 0;
}


// Crazy War

static struct BurnRomInfo crazywarRomDesc[] = {
	{ "u7",				0x080000, 0x697c2505, 1 | BRF_PRG | BRF_ESS }, //  0 GMS30C2132 Boot ROM

	{ "00",				0x200000, 0xfbb917ae, 2 | BRF_PRG | BRF_ESS }, //  1 GMS30C2132 Main ROM
	{ "01",				0x200000, 0x59308556, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "02",				0x200000, 0x34813167, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "03",				0x200000, 0x7fcb0a53, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "04",				0x200000, 0xf8eb8ce5, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "05",				0x200000, 0x14d854df, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "06",				0x200000, 0x31c67f0a, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "07",				0x200000, 0xdddf93d2, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "08",				0x200000, 0xdc37bcb9, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "09",				0x200000, 0x86ba59cc, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "10",				0x200000, 0x524bf126, 2 | BRF_PRG | BRF_ESS }, // 11
	{ "11",				0x200000, 0x613b2764, 2 | BRF_PRG | BRF_ESS }, // 12
	{ "12",				0x200000, 0x3c81d117, 2 | BRF_PRG | BRF_ESS }, // 13
	{ "13",				0x200000, 0xb86545a0, 2 | BRF_PRG | BRF_ESS }, // 14
	{ "14",				0x200000, 0x38ede322, 2 | BRF_PRG | BRF_ESS }, // 15
	{ "15",				0x200000, 0xd35e630a, 2 | BRF_PRG | BRF_ESS }, // 16

	{ "bgm.u84",		0x080000, 0x13aa7778, 3 | BRF_PRG | BRF_ESS }, // 17 QS1000 Code

	{ "effect.u85",		0x100000, 0x9159fcc6, 4 | BRF_GRA },           // 18 QS1000 Samples
	{ "qs1001a.u86",	0x080000, 0xd13c6407, 4 | BRF_GRA },           // 19
};

STD_ROM_PICK(crazywar)
STD_ROM_FN(crazywar)

struct BurnDriver BurnDrvCrazywar = {
	"crazywar", NULL, NULL, NULL, "2002",
	"Crazy War\0", NULL, "Eolith", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SHOOT | GBF_MAZE, 0,
	NULL, crazywarRomInfo, crazywarRomName, NULL, NULL, NULL, NULL, CrazywarInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 240, 4, 3
};
