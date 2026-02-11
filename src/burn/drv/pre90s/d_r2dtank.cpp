// FB Neo Sigma Ent. Inc. R2D Tank driver module
// Based on driver by David Haywood & Pierpaolo Prazzoli

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "m6800_intf.h"
#include "6821pia.h"
#include "ay8910.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvM6809ROM;
static UINT8 *DrvM6800ROM;
static UINT8 *DrvM6800RAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvColRAM;
static UINT8 *DrvNVRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 soundlatch[2];
static UINT8 flipscreen;
static UINT8 ay8910_selected;
static INT32 vblank;
static INT32 nCyclesExtra[2];

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static struct BurnInputInfo R2dtankInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 6,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy1 + 2,	"diag"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(R2dtank)

static struct BurnDIPInfo R2dtankDIPList[]=
{
	DIP_OFFSET(0x10)
	{0x00, 0xff, 0xff, 0x2f, NULL						},
	{0x01, 0xff, 0xff, 0x00, NULL						},

	{0   , 0xfe, 0   ,    2, "Bonus Life"				},
	{0x00, 0x01, 0x08, 0x00, "5000"						},
	{0x00, 0x01, 0x08, 0x08, "10000"					},

	{0   , 0xfe, 0   ,    2, "Lives"					},
	{0x00, 0x01, 0x10, 0x00, "3"						},
	{0x00, 0x01, 0x10, 0x10, "4"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x00, 0x01, 0x20, 0x20, "Upright"					},
	{0x00, 0x01, 0x20, 0x00, "Cocktail"					},

	{0   , 0xfe, 0   ,    4, "Coinage"					},
	{0x00, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"		},
	{0x00, 0x01, 0xc0, 0x00, "1 Coin  1 Credits"		},
	{0x00, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"		},
	{0x00, 0x01, 0xc0, 0xc0, "Free Play"				},
};

STDDIPINFO(R2dtank)

static void r2dtank_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x8000:
		case 0x8001:
		case 0x8002:
		case 0x8003:
			pia_write(0, address & 3, ~data);
		return;

		case 0x8004:
			soundlatch[0] = ~data;
			M6800SetIRQLine(CPU_IRQLINE0, CPU_IRQSTATUS_HOLD);
		return;

		case 0xb000: // mc6845 address
		case 0xb001: // mc6845 register
		return;
	}
}

static UINT8 r2dtank_main_read(UINT16 address)
{
	switch (address)
	{
		case 0x8000:
		case 0x8001:
		case 0x8002:
		case 0x8003:
			return pia_read(0, address & 3);

		case 0x8004:
			return soundlatch[1];
	}

	return 0;
}

static void r2dtank_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xd000:
		case 0xd001:
		case 0xd002:
		case 0xd003:
			pia_write(1, address & 3, data);
		return;

		case 0xf000:
			if (M6800GetPC(-1) == 0xfb12) {
				// death anti-freeze hack
				data = 0x00;
			}

			soundlatch[1] = data;
			M6809SetIRQLine(CPU_IRQLINE0, CPU_IRQSTATUS_HOLD);
		return;
	}
}

static UINT8 r2dtank_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xd000:
		case 0xd001:
		case 0xd002:
		case 0xd003:
			return pia_read(1, address & 3);

		case 0xf000:
			return soundlatch[0];
	}

	return 0;
}

static void main_cpu_irq(INT32 )
{
	INT32 combined_state = pia_get_irq_a_state(0) | pia_get_irq_b_state(0) | pia_get_irq_a_state(1) | pia_get_irq_b_state(1);

	M6809SetIRQLine(CPU_IRQLINE0, combined_state ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static UINT8 pia0_port_A_read(UINT16 )
{
	return (DrvInputs[0] & 0x7f) | (vblank << 7);
}

static UINT8 pia0_port_B_read(UINT16 )
{
	return DrvInputs[1];
}

static void pia0_cb2_write(UINT16, UINT8 data)
{
	flipscreen = !data;
}

static UINT8 pia1_port_A_read(UINT16 )
{
	if (ay8910_selected & 0x10) return AY8910Read(1);
	if (ay8910_selected & 0x08) return AY8910Read(0);

	return 0;
}

static void pia1_port_A_write(UINT16, UINT8 data)
{
	if (ay8910_selected & 0x08) AY8910Write(0, (~ay8910_selected >> 2) & 1, data);
	if (ay8910_selected & 0x10) AY8910Write(1, (~ay8910_selected >> 2) & 1, data);
}

static void pia1_port_B_write(UINT16, UINT8 data)
{
	ay8910_selected = data;
}

static UINT8 AY8910_0_portA_read(UINT32)
{
	return DrvDips[1];
}

static UINT8 AY8910_1_portA_read(UINT32)
{
	return DrvInputs[1];
}

static UINT8 AY8910_1_portB_read(UINT32)
{
	return DrvDips[0];
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	M6809Open(0);
	M6809Reset();
	M6809Close();

	M6800Open(0);
	M6800Reset();
	M6800Close();

	AY8910Reset(0);
	AY8910Reset(1);

	pia_reset();

	soundlatch[0] = 0;
	soundlatch[1] = 0;
	flipscreen = 0;
	ay8910_selected = 0;
	nCyclesExtra[0] = 0;
	nCyclesExtra[1] = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6809ROM		= Next; Next += 0x010000;
	DrvM6800ROM		= Next; Next += 0x010000;

	DrvPalette		= (UINT32*)Next; Next += 0x0008 * sizeof(UINT32);

	AllRam			= Next;

	DrvM6800RAM		= Next; Next += 0x000100; // 0x80, but map granularity for m6800 is 0x100

	DrvVidRAM		= Next; Next += 0x004000;
	DrvColRAM		= Next; Next += 0x004000;

	RamEnd			= Next;

	DrvNVRAM		= Next; Next += 0x000100;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvM6809ROM + 0xc800, k++, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0xd000, k++, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0xe000, k++, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0xf000, k++, 1)) return 1;

		if (BurnLoadRom(DrvM6800ROM + 0xf800, k++, 1)) return 1;
	}

	M6809Init(0);
	M6809Open(0);
	M6809MapMemory(DrvVidRAM,				0x0000, 0x3fff, MAP_RAM);
	M6809MapMemory(DrvColRAM,				0x4000, 0x7fff, MAP_RAM);
	M6809MapMemory(DrvNVRAM,				0xc000, 0xc0ff, MAP_RAM); // 0-7 actually
	M6809MapMemory(DrvM6809ROM + 0xc800,	0xc800, 0xffff, MAP_ROM);
	M6809SetWriteHandler(r2dtank_main_write);
	M6809SetReadHandler(r2dtank_main_read);
	M6809Close();

	M6800Init(0);
	M6800Open(0);
	M6800MapMemory(DrvM6800RAM, 			0x0000, 0x007f, MAP_RAM);
	M6800MapMemory(DrvM6800ROM + 0xf800,	0xf800, 0xffff, MAP_ROM);
	M6800SetWriteHandler(r2dtank_sound_write);
	M6800SetReadHandler(r2dtank_sound_read);
	M6800Close();

	{
		static pia6821_interface main_pia = {
			pia0_port_A_read, pia0_port_B_read, NULL, NULL, NULL, NULL,
			NULL, NULL, NULL, pia0_cb2_write, main_cpu_irq, main_cpu_irq
		};
		static pia6821_interface sound_pia = {
			pia1_port_A_read, NULL, NULL, NULL, NULL, NULL,
			pia1_port_A_write, pia1_port_B_write, NULL, NULL, main_cpu_irq, main_cpu_irq
		};

		pia_init();
		pia_config(0, 0, &main_pia);
		pia_config(1, 0, &sound_pia);
	}

	AY8910Init(0, 3579545 / 4, 0);
	AY8910Init(1, 3579545 / 4, 1);
	AY8910SetAllRoutes(0, 0.25, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.25, BURN_SND_ROUTE_BOTH);
	AY8910SetPorts(0, &AY8910_0_portA_read, NULL, NULL, NULL);
	AY8910SetPorts(1, &AY8910_1_portA_read, &AY8910_1_portB_read, NULL, NULL);
	AY8910SetBuffered(M6800TotalCycles, 3579545 / 4);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6809Exit();
	M6800Exit();
	pia_exit();

	AY8910Exit(0);
	AY8910Exit(1);

	BurnFreeMemIndex();

	return 0;
}

static void draw_layer()
{
	for (INT32 offs = 0; offs < 0x1c00; offs++)
	{
		UINT8 data0 = DrvVidRAM[0x0400+offs];
		UINT8 data1 = DrvColRAM[0x0400+offs];

		INT32 y = offs / 0x20;
		INT32 x = (offs & 0x1f) << 3;

		for (INT32 i = 0; i < 8; i++, x++, data0 <<= 1)
		{
			pTransDraw[y * nScreenWidth + x] = (data0 & 0x80) ? ((data1 & 0xe0) >> 5) : ((data1 & 0x0e)>> 1);
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 8; i++) {
			DrvPalette[i] = BurnHighCol(((i >> 2) & 1) * 0xff, ((i >> 1) & 1) * 0xff, (i & 1) * 0xff, 0);
		}

		DrvRecalc = 0;
	}

	draw_layer();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	M6809NewFrame();
	M6800NewFrame();

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 276;
	INT32 nCyclesTotal[2] = { (INT32)(2800000 / 56.36), (INT32)(3579545 / 4 / 56.36) };
	INT32 nCyclesDone[2] = { nCyclesExtra[0], nCyclesExtra[1] };

	M6809Open(0);
	M6800Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		vblank = (i >= 224) ? 0 : 1;
		pia_set_input_ca1(0, vblank);

		CPU_RUN(0, M6809);
		CPU_RUN(1, M6800);
	}

	M6800Close();
	M6809Close();

	nCyclesExtra[0] = nCyclesDone[0] - nCyclesTotal[0];
	nCyclesExtra[1] = nCyclesDone[1] - nCyclesTotal[1];

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	if (pnMin) {
		*pnMin = 0x029702;
	}

	if (nAction & ACB_VOLATILE) {
		ScanVar(AllRam, RamEnd - AllRam, "All Ram");

		M6809Scan(nAction);
		M6800Scan(nAction);

		AY8910Scan(nAction, pnMin);

		pia_scan(nAction, pnMin);

		SCAN_VAR(soundlatch);
		SCAN_VAR(flipscreen);
		SCAN_VAR(ay8910_selected);
		SCAN_VAR(nCyclesExtra);
	}

	if (nAction & ACB_NVRAM) {
		ScanVar(DrvNVRAM, 8, "NV RAM");
	}

	return 0;
}


// R2D Tank

static struct BurnRomInfo r2dtankRomDesc[] = {
	{ "r2d1.1c",	0x0800, 0x20606a0f, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "r2d2.1a",	0x1000, 0x7561c67f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "r2d3.2c",	0x1000, 0xfc53c538, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "r2d4.2a",	0x1000, 0x56636225, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "r2d5.7l",	0x0800, 0xc49bed15, 2 | BRF_PRG | BRF_ESS }, //  4 M6802 Code
};

STD_ROM_PICK(r2dtank)
STD_ROM_FN(r2dtank)

struct BurnDriver BurnDrvR2dtank = {
	"r2dtank", NULL, NULL, NULL, "1980",
	"R2D Tank\0", NULL, "Sigma Enterprises Inc.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_ACTION, 0,
	NULL, r2dtankRomInfo, r2dtankRomName, NULL, NULL, NULL, NULL, R2dtankInputInfo, R2dtankDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 8,
	224, 256, 3, 4
};
