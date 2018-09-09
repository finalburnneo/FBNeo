// FB Alpha Quantum driver module
// Based on MAME driver by Hedley Rainnie, Aaron Giles, Couriersud, and Paul Forgey

// To do:
//	bug fix
//	hook up analog trackball

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "watchdog.h"
#include "avgdvg.h"
#include "vector.h"
#include "pokey.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvNVRAM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvVectorRAM;
static UINT8 *DrvColRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 DrvJoy1[8];
static UINT8 DrvDips[1];
static UINT16 DrvInputs[1];
static UINT8 DrvReset;

static INT32 avgOK = 0;

static struct BurnInputInfo QuantumInputList[] = {
	{"Coin 1",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 coin"	},
	{"Coin 2",			BIT_DIGITAL,	DrvJoy1 + 4,	"p2 coin"	},
	{"Coin 3",			BIT_DIGITAL,	DrvJoy1 + 1,	"p3 coin"	},
	{"Start 1",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 start"	},
	{"Start 2",			BIT_DIGITAL,	DrvJoy1 + 3,	"p2 start"	},

	// analog input placeholders
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 7,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Quantum)

static struct BurnDIPInfo QuantumDIPList[]=
{
	{0x09, 0xff, 0xff, 0x00, NULL				},

	{0   , 0xfe, 0   ,    4, "Coinage"			},
	{0x09, 0x01, 0xc0, 0x80, "2 Coins 1 Credits"},
	{0x09, 0x01, 0xc0, 0x00, "1 Coin  1 Credits"},
	{0x09, 0x01, 0xc0, 0xc0, "1 Coin  2 Credits"},
	{0x09, 0x01, 0xc0, 0x40, "Free Play"		},

	{0   , 0xfe, 0   ,    4, "Right Coin"		},
	{0x09, 0x01, 0x30, 0x00, "*1"				},
	{0x09, 0x01, 0x30, 0x20, "*4"				},
	{0x09, 0x01, 0x30, 0x10, "*5"				},
	{0x09, 0x01, 0x30, 0x30, "*6"				},

	{0   , 0xfe, 0   ,    2, "Left Coin"		},
	{0x09, 0x01, 0x08, 0x00, "*1"				},
	{0x09, 0x01, 0x08, 0x08, "*2"				},

	{0   , 0xfe, 0   ,    5, "Bonus Coins"		},
	{0x09, 0x01, 0x07, 0x00, "None"				},
	{0x09, 0x01, 0x07, 0x01, "1 each 5"			},
	{0x09, 0x01, 0x07, 0x02, "1 each 4"			},
	{0x09, 0x01, 0x07, 0x05, "1 each 3"			},
	{0x09, 0x01, 0x07, 0x06, "2 each 4"			},
};

STDDIPINFO(Quantum)

static void DrvPaletteWrite(INT32 i, UINT8 data)
{
	DrvColRAM[i] = data;
	UINT32 *dst = DrvPalette + i * 256;
	
	for (INT32 j = 0; j < 256; j++) // intensity
	{
		int bit3 = (~data >> 3) & 1;
		int bit2 = (~data >> 2) & 1;
		int bit1 = (~data >> 1) & 1;
		int bit0 = (~data >> 0) & 1;
		int r = bit3 * 0xee;
		int g = bit1 * 0xee + bit0 * 0x11;
		int b = bit2 * 0xee;

		r = (r * j) / 255;
		g = (g * j) / 255;
		b = (b * j) / 255;

		dst[j] = (r << 16) | (g << 8) | b; // must be 32bit palette! -dink (see vector.cpp)
	}
}

static void __fastcall quantum_write_word(UINT32 address, UINT16 data)
{	
	if ((address & 0xffffc0) == 0x840000) {
		pokey_write((address / 0x20) & 1, address / 2, data);
		return;
	}

	if ((address & 0xffffe0) == 0x950000) {
		address = (address / 2) & 0xf;
		UINT8 p = DrvColRAM[address];
		if (p != (data & 0xff)) {
			DrvPaletteWrite(address,data);
		}
		return;
	}

	switch (address)
	{
		case 0x958000:
		case 0x958001:
			// coin counter 0 = data & 2, 1 = data & 1
			// NVRAM Store = data & 0x04
			// led outputs = data & 0x30
			avg_set_flip_x(data & 0x40);
			avg_set_flip_y(data & 0x80);
		return;

		case 0x960000:
		case 0x960001:
			// NVRAM Recall
		return;

		case 0x968000:
		case 0x968001:
			avgdvg_reset();
		return;

		case 0x970000:
		case 0x970001:
			avgdvg_go();
			avgOK = 1;
		return;

		case 0x978000:
		case 0x978001:
			BurnWatchogWrite();
		return;
	}
}

static void __fastcall quantum_write_byte(UINT32 address, UINT8 data)
{	
	if ((address & 0xffffc0) == 0x840000) {
		pokey_write((address / 0x20) & 1, address / 2, data);
		return;
	}
	
	if ((address & 0xffffe0) == 0x950000) {
		address = (address / 2) & 0xf;
		UINT8 p = DrvColRAM[address];
		if (p != data) {
			DrvPaletteWrite(address,data);
		}
		return;
	}

	switch (address)
	{
		case 0x958000:
		case 0x958001:
			// coin counter 0 = data & 2, 1 = data & 1
			// NVRAM Store = data & 0x04
			// led outputs = data & 0x30
			avg_set_flip_x(data & 0x40);
			avg_set_flip_y(data & 0x80);
		return;

		case 0x960000:
		case 0x960001:
			// NVRAM Recall
		return;

		case 0x968000:
		case 0x968001:
			avgdvg_reset();
		return;

		case 0x970000:
		case 0x970001:
			avgdvg_go();
			avgOK = 1;
		return;

		case 0x978000:
		case 0x978001:
			BurnWatchdogWrite();
		return;
	}
}

static UINT16 __fastcall quantum_read_word(UINT32 address)
{	
	if ((address & 0xffffc0) == 0x840000) {
		return pokey_read((address / 0x20) & 1, address / 2);
	}

	switch (address)
	{
		case 0x940000:
		case 0x940001:
			return 0; // (ioport("TRACKY")->read() << 4) | ioport("TRACKX")->read();

		case 0x948000:
		case 0x948001:
			return DrvInputs[0] | (avgdvg_done() ? 1 : 0);

		case 0x978000:
		case 0x978001:
			return 0; // nop
	}

	return 0;
}

static UINT8 __fastcall quantum_read_byte(UINT32 address)
{	
	if ((address & 0xffffc0) == 0x840000) {
		return pokey_read((address / 0x20) & 1, address / 2);
	}

	switch (address)
	{
		case 0x940000:
		case 0x940001:
			return 0; // (ioport("TRACKY")->read() << 4) | ioport("TRACKX")->read();

		case 0x948000:
			return 0xff;
			
		case 0x948001:
			return (DrvInputs[0] & 0xfe) | (avgdvg_done() ? 1 : 0);

		case 0x978000:
		case 0x978001:
			return 0; // nop
	}

	return 0;
}

static INT32 allpot_read_0(INT32 /*offset*/)
{
	return DrvDips[0];
}

static INT32 allpot_read_1(INT32 /*offset*/)
{
	return 0;
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	SekOpen(0);
	SekReset();
	SekClose();

	BurnWatchdogReset();
	vector_reset();
	avgdvg_reset();

	avgOK = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x014000;

	DrvPalette		= (UINT32*)Next; Next += 0x1000 * sizeof(UINT32);

	DrvNVRAM		= Next; Next += 0x000400; // really 200

	AllRam			= Next;

	DrvVectorRAM	= Next; Next += 0x002000;
	Drv68KRAM		= Next; Next += 0x005000;
	DrvColRAM		= Next; Next += 0x000010;

	RamEnd			= Next;

	MemEnd			= Next;

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

	memset (DrvNVRAM, 0xff, 0x200);

	{
		if (BurnLoadRom(Drv68KROM + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x000000,  1, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x004001,  2, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x004000,  3, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x008001,  4, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x008000,  5, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x00c001,  6, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x00c000,  7, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x010001,  8, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x010000,  9, 2)) return 1;
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x013fff, MAP_ROM);
	SekMapMemory(Drv68KRAM,			0x018000, 0x01cfff, MAP_RAM);
	SekMapMemory(DrvVectorRAM,		0x800000, 0x801fff, MAP_RAM);
	SekMapMemory(DrvNVRAM,			0x900000, 0x9003ff, MAP_RAM); // 0-1ff
//	SekMapMemory(DrvColRAM,			0x950000, 0x95001f, MAP_WRITE);
	SekSetWriteWordHandler(0,		quantum_write_word);
	SekSetWriteByteHandler(0,		quantum_write_byte);
	SekSetReadWordHandler(0,		quantum_read_word);
	SekSetReadByteHandler(0,		quantum_read_byte);
	SekClose();

	vector_init();
//	vector_set_scale(900, 600); // wrong?
//	vector_set_offsets(11, 119); // wrong?
	avg_quantum_start(DrvVectorRAM, SekTotalCycles); // wrong!

	PokeyInit(600000, 2, 5.00, 0);
	PokeySetTotalCyclesCB(SekTotalCycles);
	PokeyAllPotCallback(0, allpot_read_0);
	PokeyAllPotCallback(1, allpot_read_1);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	SekExit();
	PokeyExit();
	vector_exit();

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteUpdate()
{
    for (INT32 i = 0; i < 0x10; i++) // color
	{
		UINT8 data = DrvColRAM[i];
		for (INT32 j = 0; j < 256; j++) // intensity
		{
			int bit3 = (~data >> 3) & 1;
			int bit2 = (~data >> 2) & 1;
			int bit1 = (~data >> 1) & 1;
			int bit0 = (~data >> 0) & 1;
			int r = bit3 * 0xee;
			int g = bit1 * 0xee + bit0 * 0x11;
			int b = bit2 * 0xee;

			r = (r * j) / 255;
			g = (g * j) / 255;
			b = (b * j) / 255;

			DrvPalette[i * 256 + j] = (r << 16) | (g << 8) | b; // must be 32bit palette! -dink (see vector.cpp)
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	}

	if (avgOK) avgdvg_go();
	draw_vector(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	{
		DrvInputs[0] = 0xfffe;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
		}
	}

	INT32 nInterleave = 4; // irq is 4.10 / frame
	INT32 nCyclesTotal[1] = { 6048000 / 60 };
	INT32 nCyclesDone[1] = { 0 };

	SekOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone[0] += SekRun(nCyclesTotal[0] / nInterleave);
		SekSetIRQLine(1, CPU_IRQSTATUS_AUTO);
	}

	if (pBurnSoundOut) {
		pokey_update(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	SekClose();
	
	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029722;
	}
	
	if (nAction & ACB_MEMORY_ROM) {	
		ba.Data		= Drv68KROM;
		ba.nLen		= 0x14000;
		ba.nAddress	= 0;
		ba.szName	= "68K ROM";
		BurnAcb(&ba);
	}
	
	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = DrvColRAM;
		ba.nLen	  = 0x0020;
		ba.szName = "Color Ram";
		ba.nAddress	= 0x950000;
		BurnAcb(&ba);
		
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = DrvVectorRAM;
		ba.nLen	  = 0x2000;
		ba.szName = "Vector Ram";
		ba.nAddress	= 0x800000;
		BurnAcb(&ba);
		
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = Drv68KRAM;
		ba.nLen	  = 0x4000;
		ba.szName = "68K Ram";
		ba.nAddress	= 0x018000;
		BurnAcb(&ba);
	}

	if (nAction & ACB_NVRAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = DrvNVRAM;
		ba.nLen	  = 0x200;
		ba.szName = "NV Ram";
		ba.nAddress	= 0x900000;
		BurnAcb(&ba);
	}
	
	if (nAction & ACB_VOLATILE) {
		SekScan(nAction);

		BurnWatchdogScan(nAction);

		SCAN_VAR(avgOK);

		pokey_scan(nAction, pnMin);
	}

	if (nAction & ACB_WRITE) {
		if (avgOK) {
			avgdvg_go();
		}
	}

	return 0;
}


// Quantum (rev 2)

static struct BurnRomInfo quantumRomDesc[] = {
	{ "136016.201",		0x2000, 0x7e7be63a, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "136016.206",		0x2000, 0x2d8f5759, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136016.102",		0x2000, 0x408d34f4, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136016.107",		0x2000, 0x63154484, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136016.203",		0x2000, 0xbdc52fad, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136016.208",		0x2000, 0xdab4066b, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136016.104",		0x2000, 0xbf271e5c, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136016.109",		0x2000, 0xd2894424, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "136016.105",		0x2000, 0x13ec512c, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "136016.110",		0x2000, 0xacb50363, 1 | BRF_PRG | BRF_ESS }, //  9

	{ "136002-125.6h",	0x0100, 0x5903af03, 2 | BRF_GRA },           // 10 AVG PROM

	{ "cf2038n.1b",		0x00eb, 0xb372fa4f, 3 | BRF_OPT },           // 11 PLDs
};

STD_ROM_PICK(quantum)
STD_ROM_FN(quantum)

struct BurnDriver BurnDrvQuantum = {
	"quantum", NULL, NULL, NULL, "1982",
	"Quantum (rev 2)\0", NULL, "General Computer Corporation (Atari license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, quantumRomInfo, quantumRomName, NULL, NULL, QuantumInputInfo, QuantumDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	600, 900, 3, 4
};


// Quantum (rev 1)

static struct BurnRomInfo quantum1RomDesc[] = {
	{ "136016.101",		0x2000, 0x5af0bd5b, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "136016.106",		0x2000, 0xf9724666, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136016.102",		0x2000, 0x408d34f4, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136016.107",		0x2000, 0x63154484, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136016.103",		0x2000, 0x948f228b, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136016.108",		0x2000, 0xe4c48e4e, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136016.104",		0x2000, 0xbf271e5c, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136016.109",		0x2000, 0xd2894424, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "136016.105",		0x2000, 0x13ec512c, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "136016.110",		0x2000, 0xacb50363, 1 | BRF_PRG | BRF_ESS }, //  9

	{ "136002-125.6h",	0x0100, 0x5903af03, 2 | BRF_GRA },           // 10 AVG PROM

	{ "cf2038n.1b",		0x00eb, 0xb372fa4f, 3 | BRF_OPT },           // 11 PLDs
};

STD_ROM_PICK(quantum1)
STD_ROM_FN(quantum1)

struct BurnDriver BurnDrvQuantum1 = {
	"quantum1", "quantum", NULL, NULL, "1982",
	"Quantum (rev 1)\0", NULL, "General Computer Corporation (Atari license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, quantum1RomInfo, quantum1RomName, NULL, NULL, QuantumInputInfo, QuantumDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	600, 900, 3, 4
};


// Quantum (prototype)

static struct BurnRomInfo quantumpRomDesc[] = {
	{ "quantump.2e",	0x2000, 0x176d73d3, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "quantump.3e",	0x2000, 0x12fc631f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "quantump.2f",	0x2000, 0xb64fab48, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "quantump.3f",	0x2000, 0xa52a9433, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "quantump.2h",	0x2000, 0x5b29cba3, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "quantump.3h",	0x2000, 0xc64fc03a, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "quantump.2k",	0x2000, 0x854f9c09, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "quantump.3k",	0x2000, 0x1aac576c, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "quantump.2l",	0x2000, 0x1285b5e7, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "quantump.3l",	0x2000, 0xe19de844, 1 | BRF_PRG | BRF_ESS }, //  9

	{ "136002-125.6h",	0x0100, 0x5903af03, 2 | BRF_GRA },           // 10 AVG PROM

	{ "cf2038n.1b",		0x00eb, 0xb372fa4f, 3 | BRF_OPT },           // 11 PLDs
};

STD_ROM_PICK(quantump)
STD_ROM_FN(quantump)

struct BurnDriver BurnDrvQuantump = {
	"quantump", "quantum", NULL, NULL, "1982",
	"Quantum (prototype)\0", NULL, "General Computer Corporation (Atari license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, quantumpRomInfo, quantumpRomName, NULL, NULL, QuantumInputInfo, QuantumDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	600, 900, 3, 4
};
