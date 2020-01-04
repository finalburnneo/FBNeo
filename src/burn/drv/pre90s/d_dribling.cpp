// FB Alpha dribling driver module
// Based on MAME driver by:Aaron Giles

#include "tiles_generic.h"
#include "z80_intf.h"
#include "watchdog.h"
#include "8255ppi.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvColPROM;
static UINT8 *DrvColRAM;
static UINT8 *DrvVidRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 shift;
static UINT8 shift_data;
static UINT8 shift_data_prev;
static UINT8 irq_mask;
static UINT8 input_mux;
static UINT8 abca;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvInputs[4];
static UINT8 DrvReset;

static struct BurnInputInfo DriblingInputList[] = {
	{"Coin",				BIT_DIGITAL,	DrvJoy4 + 4,	"p1 coin"	},
	{"Start",				BIT_DIGITAL,	DrvJoy4 + 5,	"p1 start"	},

	{"P1 Left Stick Up",	BIT_DIGITAL,	DrvJoy1 + 7,	"p1 up"		},
	{"P1 Left Stick Down",	BIT_DIGITAL,	DrvJoy1 + 6,	"p1 down"	},
	{"P1 Left Stick Left",	BIT_DIGITAL,	DrvJoy1 + 4,	"p1 left"	},
	{"P1 Left Stick Right",	BIT_DIGITAL,	DrvJoy1 + 5,	"p1 right"	},
	{"P1 Right Stick Up",	BIT_DIGITAL,	DrvJoy1 + 3,	"p3 up"		},
	{"P1 Right Stick Down",	BIT_DIGITAL,	DrvJoy1 + 2,	"p3 down"	},
	{"P1 Right Stick Left",	BIT_DIGITAL,	DrvJoy1 + 0,	"p3 left"	},
	{"P1 Right Stick Right",BIT_DIGITAL,	DrvJoy1 + 1,	"p3 right"	},
	{"P1 Button 1",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 fire 1"	},
	{"P1 Button 2",			BIT_DIGITAL,	DrvJoy3 + 1,	"p1 fire 2"	},

	{"P2 Left Stick Up",	BIT_DIGITAL,	DrvJoy2 + 7,	"p2 up"		},
	{"P2 Left Stick Down",	BIT_DIGITAL,	DrvJoy2 + 6,	"p2 down"	},
	{"P2 Left Stick Left",	BIT_DIGITAL,	DrvJoy2 + 4,	"p2 left"	},
	{"P2 Left Stick Right",	BIT_DIGITAL,	DrvJoy2 + 5,	"p2 right"	},
	{"P2 Right Stick Up",	BIT_DIGITAL,	DrvJoy2 + 3,	"p4 up"		},
	{"P2 Right Stick Down",	BIT_DIGITAL,	DrvJoy2 + 2,	"p4 down"	},
	{"P2 Right Stick Left",	BIT_DIGITAL,	DrvJoy2 + 0,	"p4 left"	},
	{"P2 Right Stick Right",BIT_DIGITAL,	DrvJoy2 + 1,	"p4 right"	},
	{"P2 Button 1",			BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 1"	},
	{"P2 Button 2",			BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 2"	},

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
};

STDINPUTINFO(Dribling)

static void __fastcall dribling_write(UINT16 address, UINT8 data)
{
	if ((address & 0xe000) == 0xc000) {
		DrvColRAM[address & 0x1f9f] = data;
		return;
	}
}

static UINT8 __fastcall dribling_read_port(UINT16 port)
{
	if (port & 0x08)
	{
		return ppi8255_r(0, port & 3);
	}

	if (port & 0x10)
	{
		return ppi8255_r(1, port & 3);
	}

	return 0xff;
}

static void __fastcall dribling_write_port(UINT16 port, UINT8 data)
{
	if (port & 0x08)
	{
		ppi8255_w(0, port & 3, data);
		return;
	}

	if (port & 0x10)
	{
		ppi8255_w(1, port & 3, data);
		return;
	}

	if (port & 0x40)
	{
		shift_data_prev = shift_data;
		shift_data = data;
		return;
	}
}

static UINT8 dsr_read() // ppi 0 port A
{
	return (shift_data << shift) | (shift_data_prev >> (8 - shift));
}

static UINT8 input_mux_read() // ppi 0 port B
{
	if ((input_mux & 0x01) == 0) return DrvInputs[0];
	if ((input_mux & 0x02) == 0) return DrvInputs[1];
	if ((input_mux & 0x04) == 0) return DrvInputs[2];

	return 0xff;
}

static void misc_write(UINT8 data) // ppi 0 port C
{
	irq_mask = data & 0x80;
	if (!irq_mask) ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);

	abca = (data & 0x20) << 1;

	input_mux = data;
}

static UINT8 coin_read() // ppi 1 port A
{
	return DrvInputs[3];
}

static void sound_write(UINT8 /*data*/) // ppi 1 port A
{

}

static void pb_write(UINT8 /*data*/)
{

}

static void shr_write(UINT8 data)
{
	if (data & 0x08)
		BurnWatchdogWrite();

	shift = data & 0x07;
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	ZetOpen(0);
	ZetReset();
	ZetClose();

	shift_data_prev = 0;
	shift_data = 0;
	shift = 0;
	irq_mask = 0;
	input_mux = 0;
	abca = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x008000;

	DrvGfxROM		= Next; Next += 0x010000;

	DrvColPROM		= Next; Next += 0x000500;

	DrvPalette		= (UINT32*)Next; Next += 0x100 * sizeof(UINT32);

	AllRam			= Next;

	DrvColRAM		= Next; Next += 0x002000;
	DrvVidRAM		= Next; Next += 0x002000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void DrvGfxExpand()
{
	UINT8 *tmp = (UINT8*)BurnMalloc(0x2000);

	memcpy (tmp, DrvGfxROM, 0x2000);

	for (INT32 i = 0; i < 0x2000*8; i++)
	{
		INT32 y = (i / 256);
		INT32 x = (i & 0xff);

		INT32 gfx = ((tmp[(i / 8)] >> (i & 7)) & 1) << 4;
		gfx |= (tmp[(x / 8) | ((y / 8) * 0x20)] & 1) << 7;

		DrvGfxROM[i] = gfx;
	}

	BurnFree(tmp);
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
		if (BurnLoadRom(DrvZ80ROM  + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x1000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x4000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x5000,  3, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x6000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM  + 0x0000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM  + 0x1000,  6, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x0000,  7, 1)) return 1;
		if (BurnLoadRomExt(DrvColPROM + 0x0400,  8, 1, LD_INVERT)) return 1;
	//	if (BurnLoadRom(DrvColPROM + 0x0500,  9, 1)) return 1; // not used

		DrvGfxExpand();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvVidRAM,		0x2000, 0x3fff, MAP_RAM);
	ZetMapMemory(DrvColRAM,		0xc000, 0xdfff, MAP_ROM);
	ZetSetWriteHandler(dribling_write);
	ZetSetOutHandler(dribling_write_port);
	ZetSetInHandler(dribling_read_port);
	ZetClose();

	BurnWatchdogInit(DrvDoReset, 180);

	ppi8255_init(2);
	ppi8255_set_read_ports(0, dsr_read, input_mux_read, NULL);
	ppi8255_set_read_ports(1, NULL, NULL, coin_read);
	ppi8255_set_write_ports(0, NULL, NULL, misc_write);
	ppi8255_set_write_ports(1, sound_write, pb_write, shr_write);

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();


	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 256; i++)
	{
		UINT8 r = (DrvColPROM[0x400 + i] >> 0) & 1;
		UINT8 g = (DrvColPROM[0x400 + i] >> 1) & 3;
		UINT8 b = (DrvColPROM[0x400 + i] >> 3) & 1;

		DrvPalette[i] = BurnHighCol(r*0xff,g*0x55,b*0xff,0);
	}
}

static void draw_layer()
{
	for (INT32 y = 40; y < 256; y++)
	{
		UINT8 *gfx = DrvGfxROM + y * 256;
		UINT16 *dst = pTransDraw + ((y - 40) * nScreenWidth);

		for (INT32 x = 0; x < 256; x++)
		{
			INT32 b5 = (x << 2) & 0x20;
			INT32 b3 = (DrvVidRAM[(x >> 3) | (y << 5)] >> (x & 7)) & 1;
			INT32 b0 = DrvColRAM[(x >> 3) | ((y >> 2) << 7)] & 7;

			dst[x] = gfx[x] | abca | b5 | (b3 << 3) | b0;
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	draw_layer();

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
		memset (DrvInputs, 0xff, 4);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}
	}

	INT32 nInterleave = 32;
	INT32 nCyclesTotal = 5000000 / 60;
	INT32 nCyclesDone = 0;

	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		INT32 nSegment = nCyclesTotal / nInterleave;

		nCyclesDone += ZetRun(nSegment);

		if (i == (240 / 8) && irq_mask)
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
	}

	ZetClose();

	if (pBurnSoundOut) {

	}

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
		BurnWatchdogScan(nAction);
		ppi8255_scan();

		SCAN_VAR(shift_data_prev);
		SCAN_VAR(shift_data);
		SCAN_VAR(shift);
		SCAN_VAR(irq_mask);
		SCAN_VAR(input_mux);
		SCAN_VAR(abca);
	}

	return 0;
}


// Dribbling

static struct BurnRomInfo driblingRomDesc[] = {
	{ "5p.bin",			0x1000, 0x0e791947, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "5n.bin",			0x1000, 0xbd0f223a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "5l.bin",			0x1000, 0x1fccfc85, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "5k.bin",			0x1000, 0x737628c4, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "5h.bin",			0x1000, 0x30d0957f, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "3p.bin",			0x1000, 0x208971b8, 2 | BRF_GRA },           //  5 Graphics
	{ "3n.bin",			0x1000, 0x356c9803, 2 | BRF_GRA },           //  6

	{ "93453-d9.3c",	0x0400, 0xb045d005, 3 | BRF_GRA },           //  7 Graphics PROM

	{ "63s140-d8.3e",	0x0100, 0x8f1a9908, 4 | BRF_GRA },           //  8 Color PROM

	{ "tbp24s10.2d",	0x0100, 0xa17d6956, 0 | BRF_OPT },           //  9 Unknown PROM
};

STD_ROM_PICK(dribling)
STD_ROM_FN(dribling)

struct BurnDriver BurnDrvDribling = {
	"dribling", NULL, NULL, NULL, "1983",
	"Dribbling\0", "No sound", "Model Racing", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_MISC_PRE90S, GBF_SPORTSFOOTBALL, 0,
	NULL, driblingRomInfo, driblingRomName, NULL, NULL, NULL, NULL, DriblingInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 216, 4, 3
};


// Dribbling (Olympia)

static struct BurnRomInfo driblingoRomDesc[] = {
	{ "5p.bin",			0x1000, 0x0e791947, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "dribblng.5n",	0x1000, 0x5271e620, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "5l.bin",			0x1000, 0x1fccfc85, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "dribblng.5j",	0x1000, 0xe535ac5b, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "dribblng.5h",	0x1000, 0xe6af7264, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "3p.bin",			0x1000, 0x208971b8, 2 | BRF_GRA },           //  5 Graphics
	{ "3n.bin",			0x1000, 0x356c9803, 2 | BRF_GRA },           //  6

	{ "93453-d9.3c",	0x0400, 0xb045d005, 3 | BRF_GRA },           //  7 Graphics PROM

	{ "63s140-d8.3e",	0x0100, 0x8f1a9908, 4 | BRF_GRA },           //  8 Color PROM

	{ "tbp24s10.2d",	0x0100, 0xa17d6956, 0 | BRF_OPT },           //  9 Unknown PROM
};

STD_ROM_PICK(driblingo)
STD_ROM_FN(driblingo)

struct BurnDriver BurnDrvDriblingo = {
	"driblingo", "dribling", NULL, NULL, "1983",
	"Dribbling (Olympia)\0", "No sound", "Model Racing (Olympia license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_MISC_PRE90S, GBF_SPORTSFOOTBALL, 0,
	NULL, driblingoRomInfo, driblingoRomName, NULL, NULL, NULL, NULL, DriblingInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 216, 4, 3
};


// Dribbling (bootleg, Brazil)

static struct BurnRomInfo driblingbrRomDesc[] = {
	{ "1",				0x1000, 0x35d97f4f, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "2",				0x1000, 0xbd0f223a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3",				0x1000, 0x1fccfc85, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4",				0x1000, 0x3ed4073a, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "5",				0x1000, 0xc21a1d32, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "6",				0x1000, 0x208971b8, 2 | BRF_GRA },           //  5 Graphics
	{ "7",				0x1000, 0x356c9803, 2 | BRF_GRA },           //  6

	{ "93453-d9.3c",	0x0400, 0xb045d005, 3 | BRF_GRA },           //  7 Graphics PROM

	{ "63s140-d8.3e",	0x0100, 0x8f1a9908, 4 | BRF_GRA },           //  8 Color PROM

	{ "tbp24s10.2d",	0x0100, 0xa17d6956, 0 | BRF_OPT },           //  9 Unknown PROM
};

STD_ROM_PICK(driblingbr)
STD_ROM_FN(driblingbr)

struct BurnDriver BurnDrvDriblingbr = {
	"driblingbr", "dribling", NULL, NULL, "1983",
	"Dribbling (bootleg, Brazil)\0", "No sound", "bootleg (Videomac)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 4, HARDWARE_MISC_PRE90S, GBF_SPORTSFOOTBALL, 0,
	NULL, driblingbrRomInfo, driblingbrRomName, NULL, NULL, NULL, NULL, DriblingInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 216, 4, 3
};
