// FinalBurn Neo Namco System NB-1 driver module
// Based on MAME driver by Phil Stroffolino

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "m377_intf.h"
#include "c352.h"
#include "c169.h"
#include "burn_pal.h"
#include "burn_gun.h"
#include "bitswap.h"

static UINT8 *AllMem;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvMCUROM;
static UINT8 *DrvMCUData;
static UINT8 *DrvGfxROM[6];
static UINT8 *DrvSndROM;
static UINT8 *DrvNVRAM;
static UINT8 *AllRam;
static UINT8 *Drv68KRAM;
static UINT8 *DrvExtRAM;
static UINT8 *DrvShareRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprRAM2;
static UINT8 *DrvSprRAM3;
static UINT8 *DrvC123RAM;
static UINT8 *DrvC123Ctrl;
static UINT8 *DrvSprBank;
static UINT8 *DrvRozBank;
static UINT8 *DrvTileBank;
static UINT8 *DrvRozRAM;
static UINT8 *DrvUnkRegs;
static UINT8 *DrvRozCtrl;
static UINT8 *DrvPalRAMR;
static UINT8 *DrvPalRAMG;
static UINT8 *DrvPalRAMB;
static UINT16 *DrvPalRegs;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 *SpritePrio;
static UINT16 *SpriteBitmap;

static INT32 *roz_dirty_tile; // 0x10000
static UINT16 *roz_bitmap; // (256 * 8) * (256 * 8)

static INT32 min_y, min_x, max_x, max_y;
static void (*c123_tile_callback)(INT32 *tile, INT32 *mask) = NULL;
static INT32 (*c355_tile_callback)(INT32 tile) = NULL;
static void (*roz_tile_callback)(INT32 *tile, INT32 *mask_tile, INT32 which) = NULL;

static INT32 mcu_halted;
static UINT16 port6_data;
static UINT32 (*cuskey_callback)(UINT32) = NULL;
static INT32 pos_irq_level;
static INT32 unk_irq_level;
static INT32 vbl_irq_level;

static UINT16 *c355_obj_position;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvJoy4[16];
static UINT8 DrvJoy5[16];
static UINT8 DrvDips[1];
static UINT8 DrvInputs[5];
static UINT8 DrvReset;
static INT16 Analog[4];

static INT32 has_gun = 0;

static ButtonToggle service;

static INT32 nExtraCycles[2];

static UINT16 last_rand;
static INT32 timer60hz = 0;

static struct BurnInputInfo Namconb1InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy5 + 5,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy5 + 4,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"	},

	{"P3 Coin",			BIT_DIGITAL,	DrvJoy5 + 3,	"p3 coin"	},
	{"P3 Start",		BIT_DIGITAL,	DrvJoy3 + 7,	"p3 start"	},
	{"P3 Up",			BIT_DIGITAL,	DrvJoy3 + 3,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	DrvJoy3 + 2,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	DrvJoy3 + 1,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p3 fire 2"	},
	{"P3 Button 3",		BIT_DIGITAL,	DrvJoy3 + 6,	"p3 fire 3"	},

	{"P4 Coin",			BIT_DIGITAL,	DrvJoy5 + 2,	"p4 coin"	},
	{"P4 Start",		BIT_DIGITAL,	DrvJoy4 + 7,	"p4 start"	},
	{"P4 Up",			BIT_DIGITAL,	DrvJoy4 + 3,	"p4 up"		},
	{"P4 Down",			BIT_DIGITAL,	DrvJoy4 + 2,	"p4 down"	},
	{"P4 Left",			BIT_DIGITAL,	DrvJoy4 + 1,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy4 + 0,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy4 + 4,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy4 + 5,	"p4 fire 2"	},
	{"P4 Button 3",		BIT_DIGITAL,	DrvJoy4 + 6,	"p4 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy5 + 1,	"diag"		},
	{"Service",			BIT_DIGITAL,	DrvJoy5 + 7,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Namconb1)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo GunbuletInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy5 + 5,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	A("P1 Gun X",    	BIT_ANALOG_REL, &Analog[0],    	"mouse x-axis" ),
	A("P1 Gun Y",    	BIT_ANALOG_REL, &Analog[1],    	"mouse y-axis" ),

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy5 + 4,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	A("P2 Gun X",    	BIT_ANALOG_REL, &Analog[2],    	"p2 x-axis" ),
	A("P2 Gun Y",    	BIT_ANALOG_REL, &Analog[3],    	"p2 y-axis" ),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy5 + 1,	"diag"		},
	{"Service",			BIT_DIGITAL,	DrvJoy5 + 7,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};
#undef A
STDINPUTINFO(Gunbulet)

static struct BurnDIPInfo GunbuletDIPList[]=
{
	DIP_OFFSET(0x0d)
	{0x00, 0xff, 0xff, 0x41, NULL					},

	{0   , 0xfe, 0   ,    2, "Freeze Screen"		},
	{0x00, 0x01, 0x01, 0x01, "Off"					},
	{0x00, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Test switch"			},
	{0x00, 0x01, 0x40, 0x40, "Off"					},
	{0x00, 0x01, 0x40, 0x00, "On"					},
};

STDDIPINFO(Gunbulet)

static struct BurnDIPInfo Namconb1DIPList[]=
{
	DIP_OFFSET(0x27)
	{0x00, 0xff, 0xff, 0x41, NULL					},

	{0   , 0xfe, 0   ,    2, "Freeze Screen"		},
	{0x00, 0x01, 0x01, 0x01, "Off"					},
	{0x00, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Test switch"			},
	{0x00, 0x01, 0x40, 0x40, "Off"					},
	{0x00, 0x01, 0x40, 0x00, "On"					},
};

STDDIPINFO(Namconb1)

static void cpureg_write(UINT8 offset, UINT8 data)
{
	switch (offset)
	{
		case 0x01:
			SekSetIRQLine(pos_irq_level, CPU_IRQSTATUS_NONE);
			pos_irq_level = data & 0x0f;
		break;

		case 0x02:
			SekSetIRQLine(unk_irq_level, CPU_IRQSTATUS_NONE);
			unk_irq_level = data & 0x0f;
		break;

		case 0x04:
			SekSetIRQLine(vbl_irq_level, CPU_IRQSTATUS_NONE);
			vbl_irq_level = data & 0x0f;
		break;

		case 0x06:
			SekSetIRQLine(pos_irq_level, CPU_IRQSTATUS_NONE);
		break;

		case 0x07:
			SekSetIRQLine(unk_irq_level, CPU_IRQSTATUS_NONE);
		break;

		case 0x09:
			SekSetIRQLine(vbl_irq_level, CPU_IRQSTATUS_NONE);
		break;

		case 0x16: break; // nop

		case 0x18:
			if (data & 1)
			{
				mcu_halted = 0;
				M377Reset();
			}
			else
				mcu_halted = 1;

			break;
	}
}

static void b2_cpureg_write(UINT8 offset, UINT8 data)
{
	switch (offset)
	{
		case 0x00:
			SekSetIRQLine(vbl_irq_level, CPU_IRQSTATUS_NONE);
			vbl_irq_level = data & 0x0f;
		break;

		case 0x01:
			SekSetIRQLine(unk_irq_level, CPU_IRQSTATUS_NONE);
			unk_irq_level = data & 0x0f;
		break;

		case 0x02:
			SekSetIRQLine(pos_irq_level, CPU_IRQSTATUS_NONE);
			pos_irq_level = data & 0x0f;
		break;

		case 0x04:
			SekSetIRQLine(vbl_irq_level, CPU_IRQSTATUS_NONE);
		break;

		case 0x05:
			SekSetIRQLine(unk_irq_level, CPU_IRQSTATUS_NONE);
		break;

		case 0x06:
			SekSetIRQLine(pos_irq_level, CPU_IRQSTATUS_NONE);
		break;

		case 0x14: break; // WATCHDOG?

		case 0x16:
			if (data & 1)
			{
				mcu_halted = 0;
				M377Reset();
			}
			else
			{
				mcu_halted = 1;
			}

			break;
	}
}

static UINT8 namco_c116_read(UINT16 offset)
{
	UINT8 *RAM;

	offset &= 0x7fff;

	switch (offset & 0x1800)
	{
		case 0x0000:
			RAM = DrvPalRAMR;
		break;

		case 0x0800:
			RAM = DrvPalRAMG;
		break;

		case 0x1000:
			RAM = DrvPalRAMB;
		break;

		default: // case 0x1800 (internal registers)
		{
			INT32 reg = (offset & 0xf) >> 1;
			if (~offset & 1)
				return DrvPalRegs[reg] & 0xff;
			else
				return DrvPalRegs[reg] >> 8;
		}
	}

	return RAM[((offset & 0x6000) >> 2) | (offset & 0x7ff)];
}

static void namco_c116_write(UINT16 offset, UINT8 data)
{
	UINT8 *RAM;

	offset &= 0x7fff;

	switch (offset & 0x1800)
	{
		case 0x0000:
			RAM = DrvPalRAMR;
		break;

		case 0x0800:
			RAM = DrvPalRAMG;
		break;

		case 0x1000:
			RAM = DrvPalRAMB;
		break;

		default: // case 0x1800 (internal registers)
		{
			INT32 reg = (offset & 0xf) >> 1;
			if (~offset & 1)
				DrvPalRegs[reg] = (DrvPalRegs[reg] & 0xff00) | data;
			else
				DrvPalRegs[reg] = (DrvPalRegs[reg] & 0x00ff) | (data << 8);
			return;
		}
	}

	INT32 color = ((offset & 0x6000) >> 2) | (offset & 0x7ff);
	RAM[color] = data;

	DrvPalette[color] = BurnHighCol(DrvPalRAMR[color], DrvPalRAMG[color], DrvPalRAMB[color], 0);
}

static void TotalReCarl()
{
	for (INT32 color = 0; color < 0x2000; color++) {
		DrvPalette[color] = BurnHighCol(DrvPalRAMR[color], DrvPalRAMG[color], DrvPalRAMB[color], 0);
	}
}

static inline void sync_mcu()
{
	INT32 cycles = (((INT64)SekTotalCycles() * (16128000 / 2)) / 24192000) - M377TotalCycles(); // mcu is clocked at 2/3 speed of main
	if (cycles > 0) {
		if (mcu_halted) {
			M377Idle(cycles);
		} else {
			M377Run(cycles);
		}
	}
}

static void __fastcall namconb1_main_write_long(UINT32 address, UINT32 data)
{
	if ((address & 0xfffffc) == 0x1e4000) {
		// srand write (nop)
		return;
	}

	if ((address & 0xffffe0) == 0x400000) {
		return;
	}

	if ((address & 0xffffe0) == 0x6e0000) {
		// custom key writes (nop)
		return;
	}

	if ((address & 0xff8000) == 0x700000) {
		data = (data << 16) | (data >> 16);

		namco_c116_write((address & 0x7ffc) + 0, data >> 0);
		namco_c116_write((address & 0x7ffc) + 1, data >> 8);
		namco_c116_write((address & 0x7ffc) + 2, data >> 16);
		namco_c116_write((address & 0x7ffc) + 3, data >> 24);
		return;
	}
}

static void __fastcall namconb1_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfffffc) == 0x1e4000) {
		// srand write (nop)
		return;
	}

	if ((address & 0xffffe0) == 0x400000) {
		cpureg_write((address & 0x1e) + 0, data >> 0);
		cpureg_write((address & 0x1e) + 1, data >> 8);
		return;
	}

	if ((address & 0xffffe0) == 0x6e0000) {
		// custom key writes (nop)
		// 6e0002 - nebulas ray
		return;
	}

	if ((address & 0xff8000) == 0x700000) {
		namco_c116_write((address & 0x7ffe) + 0, data >> 0);
		namco_c116_write((address & 0x7ffe) + 1, data >> 8);
		return;
	}
}

static void __fastcall namconb1_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xfffffc) == 0x1e4000) {
		// srand write (nop)
		return;
	}

	if ((address & 0xffffe0) == 0x400000) {
		cpureg_write((address & 0x1f) + 0, data);
		return;
	}

	if ((address & 0xffffe0) == 0x6e0000) {
		// custom key writes (nop)
		return;
	}

	if ((address & 0xff8000) == 0x700000) {
		namco_c116_write((address & 0x7fff) ^ 0, data);
		return;
	}
}

static UINT32 __fastcall namconb1_main_read_long(UINT32 address)
{
	if ((address & 0xfffffc) == 0x1e4000) {
		return BurnRandom() | (BurnRandom() << 16);
	}

	if ((address & 0xffffe0) == 0x400000) {
		return 0xffffffff;
	}

	if ((address & 0xffffe0) == 0x6e0000) {
		if (cuskey_callback) {
			return cuskey_callback((address / 4) & 7);
		}
		return 0;
	}

	if ((address & 0xff8000) == 0x700000) {
		return namco_c116_read((address & 0x7ffc) + 0) | (namco_c116_read((address & 0x7ffc) + 1) << 8) | (namco_c116_read((address & 0x7ffc) + 2) << 16) | (namco_c116_read((address & 0x7ffc) + 3) << 24);
	}

	if ((address & 0xffffe0) == 0x100000) {
		address = (address & 0x1f) / 4;
		UINT8 gun = 0;
		switch (address & ~1) {
			case 0: gun = (UINT8)(0x0f + BurnGunReturnY(1) * 224 / 255); break;
			case 2: gun = (UINT8)(0x26 + BurnGunReturnX(1) * 288 / 314); break;
			case 4: gun = (UINT8)(0x0f + BurnGunReturnY(0) * 224 / 255); break;
			case 6: gun = (UINT8)(0x26 + BurnGunReturnX(0) * 288 / 314); break;
		}
		return (gun << 24) & 0xff000000;
	}

	return 0xff;
}

static UINT16 __fastcall namconb1_main_read_word(UINT32 address)
{
	if ((address & 0xfffffc) == 0x1e4000) {
		return BurnRandom();
	}

	if ((address & 0xffffe0) == 0x400000) {
		return 0xffff;
	}

	if ((address & 0xffffe0) == 0x6e0000) {
		if (cuskey_callback) {
			return cuskey_callback((address / 4) & 7) >> ((~address & 2) * 8);
		}
		return 0;
	}

	if ((address & 0xff8000) == 0x700000) {
		return namco_c116_read((address & 0x7ffe) + 0) | (namco_c116_read((address & 0x7ffe) + 1) << 8);
	}

	return SekReadLong(address & ~3) >> ((~address & 2) * 8);
}

static UINT8 __fastcall namconb1_main_read_byte(UINT32 address)
{
	if ((address & 0xffffe0) == 0x400000) {
		return 0xff; // cpureg
	}

	if ((address & 0xff8000) == 0x700000) {
		return namco_c116_read((address & 0x7fff) ^ 0);
	}

	return SekReadLong(address & ~3) >> ((~address & 3) * 8);
}

static void __fastcall namconb2_main_write_long(UINT32 address, UINT32 data)
{
	if ((address & 0xfffffc) == 0x1e4000) {
		// srand write (nop)
		return;
	}

	if ((address & 0xff8000) == 0x800000) {
		data = (data << 16) | (data >> 16);

		namco_c116_write((address & 0x7ffc) + 0, data >> 0);
		namco_c116_write((address & 0x7ffc) + 1, data >> 8);
		namco_c116_write((address & 0x7ffc) + 2, data >> 16);
		namco_c116_write((address & 0x7ffc) + 3, data >> 24);
		return;
	}

	if ((address & 0xffffe0) == 0xc00000) {
		// custom key writes (nop)
		return;
	}

	if ((address & 0xffffe0) == 0xf00000) {
		return;
	}
}

static void __fastcall namconb2_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xffffe0) == 0xc00000) {
		// custom key writes (nop)
		// C00002 - OUTFOXIES
		return;
	}

	if ((address & 0xfffffc) == 0x1e4000) {
		// srand write (nop)
		return;
	}

	if ((address & 0xff8000) == 0x800000) {
		namco_c116_write((address & 0x7ffe) + 0, data >> 0);
		namco_c116_write((address & 0x7ffe) + 1, data >> 8);
		return;
	}

	if ((address & 0xffffe0) == 0xf00000) {
		return;
	}
}

static void __fastcall namconb2_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xfffffc) == 0x1e4000) {
		// srand write (nop)
		return;
	}

	if ((address & 0xff8000) == 0x800000) {
		namco_c116_write((address & 0x7fff) ^ 0, data);
		return;
	}

	if ((address & 0xffffe0) == 0xc00000) {
		// custom key writes (nop)
		return;
	}

	if ((address & 0xffffe0) == 0xf00000) {
		b2_cpureg_write((address & 0x1f) + 0, data);
		return;
	}
}

static UINT32 __fastcall namconb2_main_read_long(UINT32 address)
{
	if ((address & 0xfffffc) == 0x1e4000) {
		return BurnRandom();
	}

	if ((address & 0xff8000) == 0x800000) {
		return namco_c116_read((address & 0x7ffc) + 0) | (namco_c116_read((address & 0x7ffc) + 1) << 8) | (namco_c116_read((address & 0x7ffc) + 2) << 16) | (namco_c116_read((address & 0x7ffc) + 3) << 24);
	}

	if ((address & 0xffffe0) == 0xc00000) {
		if (cuskey_callback) {
			return cuskey_callback((address / 4) & 7);
		}
		return 0;
	}

	if ((address & 0xffffe0) == 0xf00000) {
		return 0xffffffff;
	}

	return 0;
}

static UINT16 __fastcall namconb2_main_read_word(UINT32 address)
{
	if ((address & 0xfffffc) == 0x1e4000) {
		return BurnRandom();
	}

	if ((address & 0xff8000) == 0x800000) {
		return namco_c116_read((address & 0x7ffe) + 0) | (namco_c116_read((address & 0x7ffe) + 1) << 8);
	}

	if ((address & 0xffffe0) == 0xc00000) {
		if (cuskey_callback) {
			return cuskey_callback((address / 4) & 7) >> ((~address & 2) * 8);
		}
		return 0;
	}

	if ((address & 0xffffe0) == 0xf00000) {
		return 0xffff;
	}

	return 0;
}

static UINT8 __fastcall namconb2_main_read_byte(UINT32 address)
{
	if ((address & 0xff8000) == 0x800000) {
		return namco_c116_read((address & 0x7fff) ^ 0);
	}

	if ((address & 0xffffe0) == 0xf00000) {
		return 0xff; // cpureg
	}

	return 0;
}

static void mcu_write_byte(UINT32 address, UINT8 data)
{
	if (address >= 0x4000 && address <= 0xbfff) {
		address -= 0x4000;
		if ((address == 0x6000) && (data & 0x80)) {
			M377RunEnd();
		}
		DrvShareRAM[address] = data & 0xff;
		return;
	}

	if ((address & 0xfff000) == 0x2000) {
		c352_write((address & 0xfff) / 2, data);
		return;
	}
}

static void mcu_write_word(UINT32 address, UINT16 data)
{
	if (address >= 0x4000 && address <= 0xbfff) {
		address -= 0x4000;
		if ((address == 0x6000) && (data & 0x80)) {
			M377RunEnd();
		}

		DrvShareRAM[address + 0] = (data >> 0) & 0xff;
		DrvShareRAM[address + 1] = (data >> 8) & 0xff;
		return;
	}

	if ((address & 0xfff000) == 0x2000) {
		c352_write((address & 0xfff) / 2, data);
		return;
	}
}

static UINT8 mcu_read_byte(UINT32 address)
{
	if (address >= 0x4000 && address <= 0xbfff) {
		address -= 0x4000;

		return DrvShareRAM[address];
	}

	if ((address & 0xfff000) == 0x2000) {
		return c352_read((address & 0xfff) / 2);
	}

	return 0xff;
}

static UINT16 mcu_read_word(UINT32 address)
{
	if (address >= 0x4000 && address <= 0xbfff) {
		address -= 0x4000;

		return (DrvShareRAM[address + 0] & 0xff) | ((DrvShareRAM[address + 1] << 8) & 0xff00);
	}
	if ((address & 0xfff000) == 0x2000) {
		return c352_read((address & 0xfff) / 2);
	}

	return 0xffff;
}

static void mcu_write_port(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case M37710_PORT6:
			port6_data = data;
		return;
	}
}

static UINT8 mcu_read_port(UINT32 address)
{
	if (address >= 0x10 && address <= 0x1f) {
		const UINT8 scramble[8] = { 0x40, 0x20, 0x10, 0x01, 0x02, 0x04, 0x08, 0x80 };

		return (DrvInputs[2] & scramble[(address & 0x0f) >> 1]) ? 0xff : 0x00;
	}

	switch (address)
	{
		case M37710_PORT6:
			return port6_data;

		case M37710_PORT7:
			switch (port6_data & 0xf0) {
				case 0x00: return DrvInputs[3];
				case 0x20: return (DrvDips[0] & 0x41) | (DrvInputs[4] & 0xbe);
				case 0x40: return DrvInputs[0];
				case 0x60: return DrvInputs[1];
			}
			return 0xff;
	}

	return 0x00;
}

static void ResetRozDirty()
{
	memset(roz_dirty_tile, 0xff, 256 * 256 * sizeof(INT32));
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	SekOpen(0);
	SekReset();
	SekClose();

	M377Open(0);
	M377Reset();
	M377Close();

	c352_reset();

	pos_irq_level = 0;
	unk_irq_level = 0;
	vbl_irq_level = 0;
	port6_data = 0;
	mcu_halted = 0;

	timer60hz = 16128000 / 2 / 60.340909;

	nExtraCycles[0] = nExtraCycles[1] = 0;

	ResetRozDirty();

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x200000;
	DrvMCUROM		= Next; Next += 0x004000;
	DrvMCUData		= Next; Next += 0x080000;

	DrvGfxROM[0]	= Next; Next += 0x2000000; // sprite 32x32
	DrvGfxROM[1]	= Next; Next += 0x1000000;
	DrvGfxROM[2]	= Next; Next += 0x1000000;
	DrvGfxROM[3]	= Next; Next += 0x2000000; // sprite 16x16
	DrvGfxROM[4]	= Next; Next += 0x1000000; // roz
	DrvGfxROM[5]	= Next; Next += 0x1000000; // roz mask

	DrvSndROM		= Next; Next += 0x1000000;

	DrvNVRAM		= Next; Next += 0x000800;

	DrvPalette		= (UINT32*)Next; Next += 0x2001 * sizeof(UINT32);

	AllRam			= Next;

	Drv68KRAM		= Next; Next += 0x010000;
	DrvExtRAM		= Next; Next += 0x100000 - 0x8000;
	DrvShareRAM		= Next; Next += 0x008000;
	DrvUnkRegs		= Next; Next += 0x000400;

	DrvSprRAM		= Next; Next += 0x020000;
	DrvSprRAM2		= Next; Next += 0x020000;
	DrvSprRAM3		= Next; Next += 0x020000;
	DrvC123RAM		= Next; Next += 0x010000;
	DrvC123Ctrl		= Next; Next += 0x000400;
	DrvSprBank		= Next; Next += 0x000400;
	DrvRozBank		= Next; Next += 0x000400;
	DrvTileBank		= Next; Next += 0x000400;

	DrvRozRAM		= Next; Next += 0x020000;
	DrvRozCtrl		= Next; Next += 0x000400;

	DrvPalRAMR		= Next; Next += 0x002000;
	DrvPalRAMG		= Next; Next += 0x002000;
	DrvPalRAMB		= Next; Next += 0x002000;
	DrvPalRegs		= (UINT16*)Next; Next += 0x000008 * sizeof(UINT16);

	c355_obj_position = (UINT16*)Next; Next += 0x000400;

	RamEnd			= Next;

	SpritePrio		= Next; Next += 512 * 512;
	SpriteBitmap    = (UINT16*)Next; Next += 512 * 512 * sizeof(UINT32);
	roz_dirty_tile	= (INT32*)Next; Next += 256 * 256 * sizeof(INT32);
	roz_bitmap		= (UINT16*)Next; Next += ((256 * 16) * (256 * 16) * sizeof(UINT16));

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[8] = { STEP8(0,1) };
	INT32 XOffs[16] = { STEP16(0,8) };
	INT32 YOffs[16] = { STEP16(0,128) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x1400000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM[0], 0x1400000);

	GfxDecode(0x14000, 8, 16, 16, Plane, XOffs, YOffs, 0x800, tmp, DrvGfxROM[3]);

	BurnFree(tmp);

	for (INT32 i = 0; i < 0x1400000; i++) { // MAKE 16X16->32X32
		DrvGfxROM[0][i] = DrvGfxROM[3][(i & 0xfffe0f) | ((i & 0x100) >> 4) | ((i & 0xf0) << 1)];
	}

	return 0;
}

static INT32 DrvLoadRoms(bool bLoad)
{
	char* pRomName;
	struct BurnRomInfo ri;
	UINT8 *pLoad[4] = { Drv68KROM, DrvMCUROM, DrvMCUData, Drv68KROM + 0x100000 };
	UINT8 *gLoad[6] = { DrvGfxROM[0], DrvGfxROM[1], DrvGfxROM[2], DrvGfxROM[3], DrvGfxROM[4], DrvGfxROM[5] };
	UINT8 *sLoad[1] = { DrvSndROM };

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {
		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & BRF_PRG) && (ri.nType & 0xf) == 1) {
			if (bLoad) {
				if (BurnLoadRomExt(pLoad[(ri.nType & 0xf) - 1] + 0x000002, i+0, 4, LD_GROUP(2) | LD_BYTESWAP)) return 1;
				if (BurnLoadRomExt(pLoad[(ri.nType & 0xf) - 1] + 0x000000, i+1, 4, LD_GROUP(2) | LD_BYTESWAP)) return 1;
			}
			pLoad[(ri.nType & 0xf) - 1] += ri.nLen * 2;
			i++;
			continue;
		}

		if ((ri.nType & BRF_GRA) && (ri.nType & 0xf) == 1) {
			if (bLoad) {
				if (BurnLoadRomExt(gLoad[(ri.nType & 0xf) - 1] + 0x0000000, i+0, 4, LD_GROUP(2))) return 1;
				if (BurnLoadRomExt(gLoad[(ri.nType & 0xf) - 1] + 0x0000002, i+1, 4, LD_GROUP(2))) return 1;
			}
			gLoad[(ri.nType & 0xf) - 1] += ri.nLen * 2;
			i++;
			continue;
		}

		if ((ri.nType & BRF_SND) && (ri.nType & 0xf) == 1) {
			if (bLoad) {
				if (BurnLoadRom(sLoad[(ri.nType & 0xf) - 1], i, 1)) return 1;
			}
			sLoad[(ri.nType & 0xf) - 1] += ri.nLen;
			continue;
		}

		if ((ri.nType & BRF_PRG) && (ri.nType & 0xf) > 1) {
			if (bLoad) {
				if (BurnLoadRom(pLoad[(ri.nType & 0xf) - 1], i, 1)) return 1;
			}
			pLoad[(ri.nType & 0xf) - 1] += ri.nLen;
			continue;
		}

		if ((ri.nType & BRF_PRG) && (ri.nType & 0xf) == 0) { // nvram!
			if (bLoad) {
				bprintf(0, _T("Loaded NVRAM from romset..\n"));
				if (BurnLoadRom(DrvNVRAM, i, 1)) return 1;
				BurnByteswap(DrvNVRAM, 0x800);
			}
			continue;
		}

		if ((ri.nType & BRF_GRA) && (ri.nType & 0xf) > 1) {
			if (bLoad) {
				if (BurnLoadRom(gLoad[(ri.nType & 0xf) - 1], i, 1)) return 1;
			}
			gLoad[(ri.nType & 0xf) - 1] += ri.nLen;
			continue;
		}
	}

	if (bLoad) DrvGfxDecode();

	return 0;
}

static void common_mcu_init()
{
	M377Init(0, M37702);
	M377Open(0);
	M377MapMemory(DrvShareRAM,	0x004000, 0x00bfff, MAP_ROM);
	M377MapMemory(DrvMCUROM,	0x00c000, 0x00ffff, MAP_ROM);
	M377MapMemory(DrvMCUData,	0x200000, 0x27ffff, MAP_ROM);
	M377SetWritePortHandler(mcu_write_port);
	M377SetReadPortHandler(mcu_read_port);
	M377SetWriteByteHandler(mcu_write_byte);
	M377SetWriteWordHandler(mcu_write_word);
	M377SetReadByteHandler(mcu_read_byte);
	M377SetReadWordHandler(mcu_read_word);
	M377Close();

	c352_init(48384000 / 2, 288, DrvSndROM, 0x1000000, 0);
	c352_set_sync(M377TotalCycles, (16128000 / 2));
}

static INT32 outfxies_sprite_bank_callback(INT32 tile)
{
	INT32 bank = DrvSprBank[8 + (((tile >> 11) & 7) ^ 1)] & 0x5f;
	bank = (bank & 0x19) | ((bank & 2) << 1) | ((bank & 0x44) >> 1);
	return (tile & 0x7ff) | (bank << 11);
}

static void outfxies_tile_bank_callback(INT32 *tile, INT32 *mask)
{
	*mask = *tile; // not swapped
	*tile = (*tile & 0xfebf) | ((*tile & 0x100) >> 2) | ((*tile & 0x40) << 2);
}

static void outfxies_roz_tile_callback(INT32 *tile, INT32 *mask_tile, INT32 which)
{
	INT32 bank = DrvRozBank[((which * 8) + (*tile >> 11)) ^ 1];
	INT32 code = (*tile & 0x7ff) | (bank << 11);
	*mask_tile = code;
	*tile = (code & 0x7ffaf) | ((code & 0x40) >> 2) | ((code & 0x10) << 2);
}

static INT32 machbrkr_sprite_bank_callback(INT32 tile)
{
	INT32 bank = DrvSprBank[8 + (((tile >> 11) & 7) ^ 1)];
	bank = (bank & 0x1f) | ((bank & 0x40) >> 1);
	return (tile & 0x7ff) | (bank << 11);
}

static void machbrkr_tile_bank_callback(INT32 *tile, INT32 *mask)
{
	INT32 bank = DrvTileBank[((*tile >> 13) + 8) ^ 1];
	*mask = (*tile & 0x1fff) | (bank << 13);
	*tile = *mask;
}

static void machbrkr_roz_tile_callback(INT32 *tile, INT32 *mask_tile, INT32 which)
{
	INT32 bank = DrvRozBank[((which * 8) + (*tile >> 11)) ^ 1];
	INT32 code = (*tile & 0x7ff) | (bank << 11);
	*mask_tile = code;
	*tile = code;
}

static INT32 b1_sprite_bank_callback(INT32 tile)
{
	UINT16 *sprbank = (UINT16*)DrvSprBank;

	return (tile & 0x7ff) | (sprbank[(tile >> 11) & 7] << 11);
}

static void b1_tile_bank_callback(INT32 *tile, INT32 *mask)
{
	*mask = *tile;
}

static INT32 b1_common_init()
{
	BurnAllocMemIndex();

	if (DrvLoadRoms(true)) return 1;

	SekInit(0, 0x68ec020);
	SekOpen(0);
	SekMapMemory(Drv68KROM,					0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,					0x1c0000, 0x1cffff, MAP_RAM);
	SekMapMemory(DrvShareRAM,				0x200000, 0x207fff, MAP_RAM);
	SekMapMemory(DrvExtRAM,					0x208000, 0x2fffff, MAP_RAM);
	SekMapMemory(DrvNVRAM,					0x580000, 0x5807ff, MAP_RAM); // parallel eeprom
	SekMapMemory(DrvSprRAM,					0x600000, 0x61ffff, MAP_RAM);
	SekMapMemory((UINT8*)c355_obj_position,	0x620000, 0x6203ff, MAP_RAM); // 0-7 (rest is due to page size!)
	SekMapMemory(DrvC123RAM,				0x640000, 0x64ffff, MAP_RAM);
	SekMapMemory(DrvC123Ctrl,				0x660000, 0x6603ff, MAP_RAM); // 0-3f (rest is due to page size!)
	SekMapMemory(DrvSprBank,				0x680000, 0x6803ff, MAP_RAM); // 0-f (rest is due to page size!)
	SekSetWriteLongHandler(0,				namconb1_main_write_long);
	SekSetWriteWordHandler(0,				namconb1_main_write_word);
	SekSetWriteByteHandler(0,				namconb1_main_write_byte);
	SekSetReadLongHandler(0,				namconb1_main_read_long);
	SekSetReadWordHandler(0,				namconb1_main_read_word);
	SekSetReadByteHandler(0,				namconb1_main_read_byte);
	SekClose();

	common_mcu_init();

	GenericTilesInit();

	c355_tile_callback = b1_sprite_bank_callback;
	c123_tile_callback = b1_tile_bank_callback;

	DrvDoReset(1);

	return 0;
}

static INT32 b2_common_init()
{
	BurnAllocMemIndex();

	if (DrvLoadRoms(true)) return 1;

	SekInit(0, 0x68ec020);
	SekOpen(0);
	SekMapMemory(Drv68KROM,					0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,					0x1c0000, 0x1cffff, MAP_RAM);
	SekMapMemory(DrvShareRAM,				0x200000, 0x207fff, MAP_RAM);
	SekMapMemory(DrvExtRAM,					0x208000, 0x2fffff, MAP_RAM);
	SekMapMemory(Drv68KROM + 0x100000,		0x400000, 0x4fffff, MAP_ROM);
	SekMapMemory(DrvSprRAM,					0x600000, 0x61ffff, MAP_RAM);
	SekMapMemory((UINT8*)c355_obj_position,	0x620000, 0x6203ff, MAP_RAM); // 0-7 (rest is due to page size!)
	SekMapMemory(DrvUnkRegs,				0x640000, 0x6403ff, MAP_RAM); // 0-f (rest is due to page size!)
	SekMapMemory(DrvC123RAM,				0x680000, 0x68ffff, MAP_RAM);
	SekMapMemory(DrvC123Ctrl,				0x6c0000, 0x6c03ff, MAP_RAM); // 0-3f (rest is due to page size!)
	SekMapMemory(DrvRozRAM,					0x700000, 0x71ffff, MAP_RAM);
	SekMapMemory(DrvRozCtrl,				0x740000, 0x7403ff, MAP_RAM); // 0-1f (rest is due to page size!)
	SekMapMemory(DrvSprBank,				0x900000, 0x9003ff, MAP_RAM); // 0-f (rest is due to page size!)		- sprite bank is 8-f!!!
	SekMapMemory(DrvTileBank,				0x940000, 0x9403ff, MAP_RAM); // 0-f (rest is due to page size!)
	SekMapMemory(DrvRozBank,				0x980000, 0x9803ff, MAP_RAM); // 0-f (rest is due to page size!)
	SekMapMemory(DrvNVRAM,					0xa00000, 0xa007ff, MAP_RAM); // parallel eeprom
	SekSetWriteLongHandler(0,				namconb2_main_write_long);
	SekSetWriteWordHandler(0,				namconb2_main_write_word);
	SekSetWriteByteHandler(0,				namconb2_main_write_byte);
	SekSetReadLongHandler(0,				namconb2_main_read_long);
	SekSetReadWordHandler(0,				namconb2_main_read_word);
	SekSetReadByteHandler(0,				namconb2_main_read_byte);
	SekClose();

	common_mcu_init();

	GenericTilesInit();
	c169_roz_init(DrvRozRAM, DrvRozCtrl, roz_bitmap);

	DrvDoReset(1);

	return 0;
}

static UINT16 guaranteed_rand()
{
	INT32 trand;

	do { trand = BurnRandom() & 0xffff; } while (trand == last_rand);
	last_rand = trand;

	return trand;
}

static UINT32 nebulray_cuskey_callback(UINT32 offset)
{
	switch (offset)
	{
		case 1: return 0x016e;
		case 3: return guaranteed_rand();
	}

	return 0;
}

static INT32 NebulrayInit()
{
	cuskey_callback = nebulray_cuskey_callback;

	return b1_common_init();
}

static INT32 PtblankInit()
{
	has_gun = 1;
	BurnGunInit(2, true);

	// no protection key

	return b1_common_init();
}

static UINT32 gslgr94u_cuskey_callback(UINT32 offset)
{
	switch (offset)
	{
		case 0: return 0x0167;
		case 1: return guaranteed_rand() << 16;
	}

	return 0;
}

static UINT32 gslgr94j_cuskey_callback(UINT32 offset)
{
	switch (offset)
	{
		case 1: return 0;
		case 3: return (0x0171 << 16) | guaranteed_rand();
	}

	return 0;
}

static UINT32 sws95_cuskey_callback(UINT32 offset)
{
	switch (offset)
	{
		case 0: return 0x0189;
		case 1: return guaranteed_rand() << 16;
	}

	return 0;
}

static UINT32 sws96_cuskey_callback(UINT32 offset)
{
	switch (offset)
	{
		case 0: return 0x01aa << 16;
		case 4: return guaranteed_rand() << 16;
	}

	return 0;
}

static UINT32 sws97_cuskey_callback(UINT32 offset)
{
	switch (offset)
	{
		case 2: return 0x1b2 << 16;
		case 5: return guaranteed_rand() << 16;
	}

	return 0;
}

static UINT32 vshoot_cuskey_callback(UINT32 offset)
{
	switch (offset)
	{
		case 2: return guaranteed_rand() << 16;
		case 3: return 0x0170 << 16;
	}

	return 0;
}

static INT32 B1SportsInit(UINT32 (*cuskey)(UINT32))
{
	cuskey_callback = cuskey;

	return b1_common_init();
}

static INT32 VshootInit()
{
	cuskey_callback = vshoot_cuskey_callback;

	return b1_common_init();
}

static UINT32 outfxies_cuskey_callback(UINT32 offset)
{
	switch (offset)
	{
		case 0: return 0x0186;
		case 1: return guaranteed_rand() << 16;
	}

	return 0;
}

static INT32 OutfxiesInit()
{
	cuskey_callback = outfxies_cuskey_callback;
	c355_tile_callback = outfxies_sprite_bank_callback;
	c123_tile_callback = outfxies_tile_bank_callback;
	roz_tile_callback = outfxies_roz_tile_callback;

	return b2_common_init();
}

static INT32 OutfxiesjaInit()
{
	cuskey_callback = outfxies_cuskey_callback;
	c355_tile_callback = machbrkr_sprite_bank_callback;
	c123_tile_callback = machbrkr_tile_bank_callback;
	roz_tile_callback = machbrkr_roz_tile_callback;

	INT32 rc = b2_common_init();
	if (!rc) { // shuffle soundrom (c352)
		UINT8 *tmp = (UINT8*)BurnMalloc(0x1000000);
		memcpy(tmp, DrvSndROM, 0x400000);

		memcpy(DrvSndROM + 0x000000, tmp + 0x000000, 0x200000);
		memcpy(DrvSndROM + 0x400000, tmp + 0x000000, 0x200000);
		memcpy(DrvSndROM + 0x800000, tmp + 0x200000, 0x200000);
		memcpy(DrvSndROM + 0xc00000, tmp + 0x200000, 0x200000);

		BurnFree(tmp);
	}

	return rc;
}

static INT32 MachbrkrInit()
{
	cuskey_callback = NULL;
	c355_tile_callback = machbrkr_sprite_bank_callback;
	c123_tile_callback = machbrkr_tile_bank_callback;
	roz_tile_callback = machbrkr_roz_tile_callback;

	INT32 rc = b2_common_init();
	if (!rc) { // shuffle soundrom (c352)
		UINT8 *tmp = (UINT8*)BurnMalloc(0x1000000);
		memcpy(tmp, DrvSndROM, 0x400000);

		memcpy(DrvSndROM + 0x000000, tmp + 0x000000, 0x200000);
		memcpy(DrvSndROM + 0x400000, tmp + 0x000000, 0x200000);
		memcpy(DrvSndROM + 0x800000, tmp + 0x200000, 0x200000);
		memcpy(DrvSndROM + 0xc00000, tmp + 0x200000, 0x200000);

		BurnFree(tmp);
	}

	return rc;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	SekExit();
	M377Exit();
	c352_exit();

	BurnFreeMemIndex();

	cuskey_callback = NULL;
	c123_tile_callback = NULL;
	c355_tile_callback = NULL;
	roz_tile_callback = NULL;

	if (has_gun) {
		BurnGunExit();
	}

	has_gun = 0;

	return 0;
}

static inline UINT16 get_palette_register(INT32 reg)
{
	return DrvPalRegs[reg];
}

#define restore_XY_clip(); \
	min_x = oldmin_x; \
	max_x = oldmax_x; \
	min_y = oldmin_y; \
	max_y = oldmax_y;

static inline void adjust_clip()
{
	if (min_x >= nScreenWidth) min_x = nScreenWidth-1;
	if (min_x < 2) min_x = 0; // weird. outfoxies, mach breakers clips the first column?
	if (max_x >= nScreenWidth) max_x = nScreenWidth-1;
	if (max_x < 0) max_x = 0;
	if (min_y >= nScreenHeight) min_y = nScreenHeight-1;
	if (min_y < 0) min_y = 0;
	if (max_y >= nScreenHeight) max_y = nScreenHeight-1;
	if (max_y < 0) max_y = 0;
}

static void apply_clip()
{
	min_x = get_palette_register(0) - 0x4a;
	max_x = get_palette_register(1) - 0x4a - 1;
	min_y = get_palette_register(2) - 0x21;
	max_y = get_palette_register(3) - 0x21 - 1;

	adjust_clip();
	//bprintf (0, _T("Clip: %d, %d, %d, %d\n"), min_x, max_x, min_y, max_y);

	GenericTilesSetClip(min_x, max_x, min_y, max_y);
}

static void draw_layer_with_masking_by_line(INT32 layer, INT32 color, INT32 line, INT32 priority)
{
	if (line < min_y || line > max_y) return;

	if (layer >= 6) return;

	if ((nSpriteEnable & (1<<layer)) == 0) return;

	INT32 x_offset_table[6] = { 44+4, 44+2, 44+1, 44+0, 0, 0 };
	INT32 offset[6] = { 0, 0x2000, 0x4000, 0x6000, 0x8010, 0x8810 };

	UINT16 *ctrl = (UINT16*)DrvC123Ctrl;
	UINT16 *ram = (UINT16*)(DrvC123RAM + offset[layer]);

	// layer dis-enable
	if (ctrl[0x10 + layer] & (1<<3)) return;

	INT32 sizex = (layer < 4) ? 64 : 36;
	INT32 sizey = (layer < 4) ? 64 : 28;

	INT32 sizex_full = sizex * 8;
	INT32 sizey_full = sizey * 8;

	INT32 flipscreen = (BURN_ENDIAN_SWAP_INT16(ctrl[1]) & 0x8000) ? 0xffff : 0;

	INT32 x_offset = x_offset_table[layer];
	INT32 y_offset = (layer < 4) ? 24 : 0;

	INT32 scrollx = ((BURN_ENDIAN_SWAP_INT16(ctrl[1 + layer * 4]) + x_offset) ^ flipscreen) % sizex_full;
	INT32 scrolly = ((BURN_ENDIAN_SWAP_INT16(ctrl[3 + layer * 4]) + y_offset) ^ flipscreen) % sizey_full;

	if (flipscreen) {
		scrolly += 256 + 16;
		scrollx += 256;

		scrollx %= sizex_full;
		scrolly %= sizey_full;
	}

	if (layer >= 4) {
		scrollx = scrolly = 0;
	}

	color = ((color & 7) * 256) + 0x1000;

	INT32 sy = (scrolly + line) % sizey_full;

	UINT16 *dst = pTransDraw + (line * nScreenWidth);
	UINT8 *pri = pPrioDraw + (line * nScreenWidth);

	for (INT32 x = 0; x < nScreenWidth + 7; x+=8)
	{
		INT32 sx = (scrollx + x) % sizex_full;

		INT32 offs = (sx / 8) + ((sy / 8) * sizex);

		INT32 code = BURN_ENDIAN_SWAP_INT16(ram[offs]), mask_code = 0;
		c123_tile_callback(&code, &mask_code);

		UINT8 *gfx = DrvGfxROM[1] + (code * 8 * 8);
		UINT8 *msk = DrvGfxROM[2] + (mask_code * 8);

		gfx += (sy & 7) * 8;
		msk += (sy & 7);

		INT32 zx = x - (sx & 7);

		for (INT32 xx = 0; xx < 8; xx++, zx++)
		{
			if (zx < min_x || zx > max_x) continue;

			if (*msk & (0x80 >> xx))
			{
				dst[zx] = gfx[xx] + color;
				pri[zx] = (priority & 0x1000) ? ((priority * 2) & 0xff) : (priority & 0xff);
			}
		}
	}
}

static void draw_layer_line(INT32 line, INT32 pri)
{
	UINT16 *ctrl = (UINT16*)DrvC123Ctrl;

	for (INT32 i = 0; i < 6; i++)
	{
		if((BURN_ENDIAN_SWAP_INT16(ctrl[0x10+i]) & 0xf) == (pri & 0xf))
		{
			INT32 layer_color = BURN_ENDIAN_SWAP_INT16(ctrl[0x18 + i]);
			if (nSpriteEnable & (1 << i)) draw_layer_with_masking_by_line(i, layer_color, line, pri);
		}
	}
}

static void zdrawgfxzoom(UINT8 *gfx, INT32 tile_size, UINT32 code, UINT32 color, INT32 flipx, INT32 flipy, INT32 sx, INT32 sy, INT32 scalex, INT32 scaley, INT32 priority, INT32 is_c355)
{
	if (!scalex || !scaley) return;

	if (!max_x && !max_y) return; //nothing to draw (zdrawgfxzoom draws a 1x1 pixel at 0,0 if max_x and max_y are 0)

	const UINT8 *source_base = gfx + (code * tile_size * tile_size);
	INT32 sprite_screen_height = (scaley*tile_size+0x8000)>>16;
	INT32 sprite_screen_width = (scalex*tile_size+0x8000)>>16;
	if (sprite_screen_width && sprite_screen_height)
	{
		INT32 dx = (tile_size<<16)/sprite_screen_width;
		INT32 dy = (tile_size<<16)/sprite_screen_height;

		INT32 ex = sx+sprite_screen_width;
		INT32 ey = sy+sprite_screen_height;

		INT32 x_index_base;
		INT32 y_index;

		if( flipx )
		{
			x_index_base = (sprite_screen_width-1)*dx;
			dx = -dx;
		}
		else
		{
			x_index_base = 0;
		}

		if( flipy )
		{
			y_index = (sprite_screen_height-1)*dy;
			dy = -dy;
		}
		else
		{
			y_index = 0;
		}

		if( sx < min_x)
		{ // clip left
			INT32 pixels = min_x-sx;
			sx += pixels;
			x_index_base += pixels*dx;
		}
		if( sy < min_y )
		{ // clip top
			INT32 pixels = min_y-sy;
			sy += pixels;
			y_index += pixels*dy;
		}
		if( ex > max_x+1 )
		{ // clip right
			INT32 pixels = ex-max_x-1;
			ex -= pixels;
		}
		if( ey > max_y+1 )
		{ // clip bottom
			INT32 pixels = ey-max_y-1;
			ey -= pixels;
		}

		if (!(ex > sx)) return;

		for (INT32 y = sy; y < ey; y++)
		{
			const UINT8 *source = source_base + (y_index>>16) * tile_size;
			UINT16 *dest = SpriteBitmap + y * nScreenWidth;
			UINT8 *pri2 = SpritePrio + y * nScreenWidth;
			INT32 x, x_index = x_index_base;

			for (x = sx; x < ex; x++)
			{
				INT32 c = source[x_index>>16];
				if (c != 0xff)
				{
					dest[x] = c | color;

					pri2[x] = priority;
				}
				x_index += dx;
			}
			y_index += dy;
		}
	}
}

static void c355_obj_draw_sprite(const UINT16 *pSource, INT32 zpos)
{
	UINT16 *spriteram16 = (UINT16*)DrvSprRAM3;
	const UINT16 *spriteformat16 = &spriteram16[0x4000/2];
	const UINT16 *spritetile16   = &spriteram16[0x8000/2];

	UINT16 palette     = BURN_ENDIAN_SWAP_INT16(pSource[6]);

	INT32 pri = (palette >> 4) & 0xf;

	UINT16 linkno      = BURN_ENDIAN_SWAP_INT16(pSource[0]);
	UINT16 offset      = BURN_ENDIAN_SWAP_INT16(pSource[1]);
	INT32 hpos         = BURN_ENDIAN_SWAP_INT16(pSource[2]);
	INT32 vpos         = BURN_ENDIAN_SWAP_INT16(pSource[3]);
	UINT16 hsize       = BURN_ENDIAN_SWAP_INT16(pSource[4]);
	UINT16 vsize       = BURN_ENDIAN_SWAP_INT16(pSource[5]);

	if (linkno*4>=0x4000/2) return;

	INT32 xscroll = (INT16)c355_obj_position[1];
	INT32 yscroll = (INT16)c355_obj_position[0];
	xscroll &= 0x1ff; if( xscroll & 0x100 ) xscroll |= ~0x1ff;
	yscroll &= 0x1ff; if( yscroll & 0x100 ) yscroll |= ~0x1ff;
	xscroll += 0x26;
	yscroll += 0x19;

	hpos -= xscroll;
	vpos -= yscroll;
	const UINT16 *pWinAttr = &spriteram16[0x2400/2+((palette>>8)&0xf)*4];

	INT32 oldmin_x = min_x;
	INT32 oldmax_x = max_x;
	INT32 oldmin_y = min_y;
	INT32 oldmax_y = max_y;
	min_x = BURN_ENDIAN_SWAP_INT16(pWinAttr[0]) - xscroll;
	max_x = BURN_ENDIAN_SWAP_INT16(pWinAttr[1]) - xscroll - 1; // -dink (machbrkr clipping player attract pic)
	min_y = BURN_ENDIAN_SWAP_INT16(pWinAttr[2]) - yscroll;
	max_y = BURN_ENDIAN_SWAP_INT16(pWinAttr[3]) - yscroll;
	adjust_clip(); // make sane

	if (min_x < oldmin_x) min_x = oldmin_x;
	if (max_x > oldmax_x) max_x = oldmax_x;
	if (min_y < oldmin_y) min_y = oldmin_y;
	if (max_y > oldmax_y) max_y = oldmax_y;

	hpos&=0x7ff; if( hpos&0x400 ) hpos |= ~0x7ff; /* sign extend */
	vpos&=0x7ff; if( vpos&0x400 ) vpos |= ~0x7ff; /* sign extend */

	INT32 tile_index      = BURN_ENDIAN_SWAP_INT16(spriteformat16[linkno*4+0]);
	INT32 format          = BURN_ENDIAN_SWAP_INT16(spriteformat16[linkno*4+1]);
	INT32 dx              = BURN_ENDIAN_SWAP_INT16(spriteformat16[linkno*4+2]);
	INT32 dy              = BURN_ENDIAN_SWAP_INT16(spriteformat16[linkno*4+3]);
	INT32 num_cols        = (format>>4)&0xf;
	INT32 num_rows        = (format)&0xf;

	if( num_cols == 0 ) num_cols = 0x10;
	INT32 flipx = (hsize&0x8000)?1:0;
	hsize &= 0x3ff;
	if( hsize == 0 ) { restore_XY_clip(); return; }
	UINT32 zoomx = (hsize<<16)/(num_cols*16);
	dx = (dx*zoomx+0x8000)>>16;
	if( flipx )
	{
		hpos += dx;
	}
	else
	{
		hpos -= dx;
	}

	if( num_rows == 0 ) num_rows = 0x10;
	INT32 flipy = (vsize&0x8000)?1:0;
	vsize &= 0x3ff;
	if( vsize == 0 ) { restore_XY_clip(); return; }
	UINT32 zoomy = (vsize<<16)/(num_rows*16);
	dy = (dy*zoomy+0x8000)>>16;
	if( flipy )
	{
		vpos += dy;
	}
	else
	{
		vpos -= dy;
	}

	INT32 color = palette&0xf;

	UINT32 source_height_remaining = num_rows*16;
	UINT32 screen_height_remaining = vsize;
	INT32 sy = vpos;
	for(INT32 row=0; row<num_rows; row++)
	{
		INT32 tile_screen_height = 16*screen_height_remaining/source_height_remaining;
		zoomy = (screen_height_remaining<<16)/source_height_remaining;
		if( flipy )
		{
			sy -= tile_screen_height;
		}
		UINT32 source_width_remaining = num_cols*16;
		UINT32 screen_width_remaining = hsize;
		INT32 sx = hpos;
		for(INT32 col=0; col<num_cols; col++)
		{
			INT32 tile_screen_width = 16*screen_width_remaining/source_width_remaining;
			zoomx = (screen_width_remaining<<16)/source_width_remaining;
			if( flipx )
			{
				sx -= tile_screen_width;
			}
			INT32 tile = BURN_ENDIAN_SWAP_INT16(spritetile16[tile_index++]);

			if( (tile&0x8000)==0 )
			{
				INT32 size = 0;

				tile = c355_tile_callback(tile);

				zdrawgfxzoom( size ? DrvGfxROM[0] : DrvGfxROM[3], size ? 32 : 16, tile + offset, color * 256, flipx,flipy, sx, sy, zoomx, zoomy, pri, 1);
			}

			if( !flipx )
			{
				sx += tile_screen_width;
			}
			screen_width_remaining -= tile_screen_width;
			source_width_remaining -= 16;
		}
		if( !flipy )
		{
			sy += tile_screen_height;
		}
		screen_height_remaining -= tile_screen_height;
		source_height_remaining -= 16;
	}

	restore_XY_clip();
}

static void c355_obj_draw_list(const UINT16 *pSpriteList16, const UINT16 *pSpriteTable)
{
	for (INT32 i = 0; i < 256; i++)
	{
		UINT16 which = BURN_ENDIAN_SWAP_INT16(pSpriteList16[i]);
		c355_obj_draw_sprite(&pSpriteTable[(which & 0xff) * 8], i);
		if (which & 0x100) break;
	}
}

static void c355_predraw_sprites()
{
	UINT16 *m_c355_obj_ram = (UINT16*)DrvSprRAM3;

	memset(SpritePrio, 0, nScreenWidth * nScreenHeight);
	memset(SpriteBitmap, 0xff, nScreenWidth * nScreenHeight * sizeof(UINT32));

	c355_obj_draw_list(&m_c355_obj_ram[0x02000/2], &m_c355_obj_ram[0x00000/2]);
	c355_obj_draw_list(&m_c355_obj_ram[0x14000/2], &m_c355_obj_ram[0x10000/2]);
}

static void c355_draw_sprite_line(INT32 line, INT32 priority)
{
	if (line < min_y || line > max_y) return;

	UINT16 *src = SpriteBitmap + line * nScreenWidth;
	UINT16 *dest = pTransDraw + line * nScreenWidth;
	UINT8 *pri = SpritePrio + line * nScreenWidth;

	for (INT32 x = 0; x < nScreenWidth; x++) {
		if (x < min_x || x > max_x) continue;

		if (pri[x] == priority && src[x] != 0xffff) {
			if ((src[x] & 0xff) != 0xff) {
				if (src[x] == 0xffe) { // apply palette offset (shadow palette)
					if (dest[x] & 0x1000)
						dest[x] |= 0x800; // gslgr94u, sws97, j-league needs this
					else
						dest[x] = 0x2000; // black pen
				} else {
					dest[x] = src[x];
				}
			}
		}
	}
}

static void predraw_c169_roz_bitmap()
{
	UINT16 *ram = (UINT16*)DrvRozRAM;

	for (INT32 offs = 0; offs < 256 * 256; offs++)
	{
		INT32 sx = (offs & 0xff);
		INT32 sy = (offs / 0x100);

		INT32 ofst;
		if (sx >= 128) {
			ofst = (sx & 0x7f) + ((sy + 0x100) * 0x80);
		} else {
			ofst = sx + (sy * 0x80);
		}

		INT32 code = BURN_ENDIAN_SWAP_INT16(ram[ofst]) & 0x3fff;
		INT32 code_mask = code;
		roz_tile_callback(&code, &code_mask, 0); // iq_132

		if (code == BURN_ENDIAN_SWAP_INT16(roz_dirty_tile[ofst])) {
			continue;
		}
		roz_dirty_tile[ofst] = code;

		sx *= 16;
		sy *= 16;

		UINT8 *gfx = DrvGfxROM[4] + (code * 0x100);
		UINT8 *msk = DrvGfxROM[5] + (code_mask * 0x020);

		UINT16 *dst = roz_bitmap + (sy * (256 * 16)) + sx;

		for (INT32 y = 0; y < 16; y++, gfx+=16,msk+=2)
		{
			for (INT32 x = 0; x < 16; x++)
			{
				if (msk[x/8] & (0x80 >> (x & 7)))
				{
					dst[x] = BURN_ENDIAN_SWAP_INT16(gfx[x]) + 0x1800;
				} else {
					dst[x] = BURN_ENDIAN_SWAP_INT16(0x8000);
				}
			}

			dst += (256 * 16);
		}
	}
}

static void DrvDrawBegin()
{
	if (DrvRecalc) {
		TotalReCarl();
		DrvRecalc = 0;
	}

	if (roz_tile_callback) predraw_c169_roz_bitmap();

	apply_clip();

	BurnTransferClear(0x2000);

	if (nBurnLayer & 1) c355_predraw_sprites();
}

static void DrvDrawLine(INT32 line)
{
	if (roz_tile_callback) {
		for (INT32 pri = 0; pri < 16; pri++)
		{
			if (nBurnLayer & 2) c169_roz_draw(pri, line);
			if ((pri & 1) == 0) draw_layer_line(line, pri / 2);
			if (nBurnLayer & 1) c355_draw_sprite_line(line, pri);
		}
	} else {
		for (INT32 pri = 0; pri < 8; pri++)
		{
			draw_layer_line(line, pri);
			if (nBurnLayer & 1) c355_draw_sprite_line(line, pri);
		}
	}
}

static void DrvDrawEnd()
{
	BurnTransferCopy(DrvPalette);

	if (has_gun) BurnGunDrawTargets();
}

static INT32 DrvDraw()
{
	DrvDrawBegin();

	for (INT32 i = 0; i < nScreenHeight; i++) {
		DrvDrawLine(i);
	}

	DrvDrawEnd();

	return 0;
}

static void cancel_opposing(UINT8 *inp)
{
	if ((*inp & 0x03) == 0x00) *inp |= 0x03;
	if ((*inp & 0x0c) == 0x00) *inp |= 0x0c;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset(1);
	}

	SekNewFrame();
	M377NewFrame();

	{
		service.Toggle(DrvJoy5[1]); // toggle-check service mode

		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
			DrvInputs[4] ^= (DrvJoy5[i] & 1) << i;
		}

		if (has_gun) {
			BurnGunMakeInputs(0, Analog[0], Analog[1]);
			BurnGunMakeInputs(1, Analog[2], Analog[3]);
		} else {
			cancel_opposing(&DrvInputs[0]);
			cancel_opposing(&DrvInputs[1]);
			cancel_opposing(&DrvInputs[2]);
			cancel_opposing(&DrvInputs[3]);
		}
	}

	INT32 mult = 4;
	INT32 nInterleave = 264 * mult;
	INT32 nCyclesTotal[2] = { (INT32)(24192000 / 59.659091),  (INT32)(16128000 / 2 / 59.659091) };
	INT32 nCyclesDone[2] = { nExtraCycles[0], nExtraCycles[1] };

	SekOpen(0);
	M377Open(0);
	M377Idle(nExtraCycles[1]); // since we use CPU_RUN_SYNCINT

	if (pBurnDraw) {
		DrvDrawBegin();
	}

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);

		INT32 cyc_then = M377TotalCycles();

		if (mcu_halted == 0) {
			CPU_RUN_SYNCINT(1, M377);
		} else {
			CPU_IDLE_SYNCINT(1, M377);
		}

		timer60hz -= M377TotalCycles() - cyc_then;
		if (timer60hz < 0) {
			timer60hz += 16128000 / 2 / 60.340909; // 60.00 + (60.00 - 59.659091)   -dink
			M377SetIRQLine(M37710_LINE_IRQ0, CPU_IRQSTATUS_HOLD);
			M377SetIRQLine(M37710_LINE_IRQ2, CPU_IRQSTATUS_HOLD);
		}

		if (((i&(mult-1))==0) && i/mult == (get_palette_register(5) - 32) && pos_irq_level) {
			SekSetIRQLine(pos_irq_level, CPU_IRQSTATUS_ACK);
		}

		if (pBurnDraw && ((i&(mult-1))==1) && i/mult < nScreenHeight) {
			DrvDrawLine(i/mult);
		}

		if (i == 224*mult) {
			// 2-frame sprite buffer
			memcpy(DrvSprRAM3, DrvSprRAM2, 0x20000);
			memcpy(DrvSprRAM2, DrvSprRAM,  0x20000);

			if (vbl_irq_level) SekSetIRQLine(vbl_irq_level, CPU_IRQSTATUS_ACK);

			if (pBurnDraw) {
				DrvDrawEnd();
			}
		}
	}

	if (pBurnSoundOut) {
		c352_update(pBurnSoundOut, nBurnSoundLen);
	}

	nExtraCycles[0] = nCyclesDone[0] - nCyclesTotal[0];
	nExtraCycles[1] = M377TotalCycles() - nCyclesTotal[1];

	M377Close();
	SekClose();

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin =  0x029702;
	}
	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_NVRAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data		= DrvNVRAM;
		ba.nLen		= 0x00800;
		ba.szName	= "NV RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);
		M377Scan(nAction);

		c352_scan(nAction, pnMin);

		SCAN_VAR(mcu_halted);
		SCAN_VAR(port6_data);
		SCAN_VAR(pos_irq_level);
		SCAN_VAR(unk_irq_level);
		SCAN_VAR(vbl_irq_level);

		SCAN_VAR(timer60hz);

		SCAN_VAR(last_rand);

		service.Scan();
		if (has_gun) BurnGunScan();

		BurnRandomScan(nAction);

		SCAN_VAR(nExtraCycles);
	}

	if (nAction & ACB_WRITE && ~nAction & ACB_RUNAHEAD) {
		ResetRozDirty();
	}

 	return 0;
}

static struct BurnRomInfo emptyRomDesc[] = { { "", 0, 0, 0 }, }; // for bios handling

static struct BurnRomInfo namcoc75RomDesc[] = {
	{ "c75.bin", 		0x004000, 0x42f539a5, 2 | BRF_BIOS | BRF_PRG }, // C75 Internal ROM
};

STD_ROM_PICK(namcoc75)
STD_ROM_FN(namcoc75)

static INT32 namcoc75Init() {
	return 1;
}

struct BurnDriver BurnDrvnamcoc75 = {
	"namcoc75", NULL, NULL, NULL, "1994",
	"Namco C75 (M37702) (Bios)\0", "BIOS only", "Namco", "NB-1 / NB-2",
	NULL, NULL, NULL, NULL,
	BDF_BOARDROM, 0, HARDWARE_MISC_POST90S, GBF_BIOS, 0,
	NULL, namcoc75RomInfo, namcoc75RomName, NULL, NULL, NULL, NULL, Namconb1InputInfo, Namconb1DIPInfo,
	namcoc75Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	224, 288, 3, 4
};


// Nebulas Ray (World, NR2)

static struct BurnRomInfo nebulrayRomDesc[] = {
	{ "nr2_mprl.15b",	0x080000, 0x0431b6d4, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "nr2_mpru.13b",	0x080000, 0x049b97cb, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "nr1-spr0",		0x020000, 0x1cc2b44b, 3 | BRF_PRG | BRF_ESS }, //  2 C75 Data

	{ "nr1-voi0",		0x200000, 0x332d5e26, 1 | BRF_SND },           //  3 C352 Samples

	{ "nr1obj0l",		0x200000, 0x0e99ef46, 1 | BRF_GRA },           //  4 C355 Sprite Data
	{ "nr1obj0u",		0x200000, 0xfb82a881, 1 | BRF_GRA },           //  5
	{ "nr1obj1l",		0x200000, 0xf7a898f0, 1 | BRF_GRA },           //  6
	{ "nr1obj1u",		0x200000, 0x49d9dbd7, 1 | BRF_GRA },           //  7
	{ "nr1obj2l",		0x200000, 0xb39871d1, 1 | BRF_GRA },           //  8
	{ "nr1obj2u",		0x200000, 0x8c8205b1, 1 | BRF_GRA },           //  9
	{ "nr1obj3l",		0x200000, 0xc90d13ae, 1 | BRF_GRA },           // 10
	{ "nr1obj3u",		0x200000, 0xd5918c9e, 1 | BRF_GRA },           // 11

	{ "nr1-chr0",		0x100000, 0x8d5b54ea, 2 | BRF_GRA },           // 12 C123 Tiles
	{ "nr1-chr1",		0x100000, 0xcd21630c, 2 | BRF_GRA },           // 13
	{ "nr1-chr2",		0x100000, 0x70a11023, 2 | BRF_GRA },           // 14
	{ "nr1-chr3",		0x100000, 0x8f4b1d51, 2 | BRF_GRA },           // 15

	{ "nr1-sha0",		0x080000, 0xca667e13, 3 | BRF_GRA },           // 16 C123 Tile Mask

	{ "c366.bin",		0x000020, 0x8c96f31d, 0 | BRF_OPT },           // 17 Unknown PROM
};

STDROMPICKEXT(nebulray, nebulray, namcoc75)
STD_ROM_FN(nebulray)

struct BurnDriver BurnDrvNebulray = {
	"nebulray", NULL, "namcoc75", NULL, "1994",
	"Nebulas Ray (World, NR2)\0", NULL, "Namco", "NB-1 / NB-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, nebulrayRomInfo, nebulrayRomName, NULL, NULL, NULL, NULL, Namconb1InputInfo, Namconb1DIPInfo,
	NebulrayInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	224, 288, 3, 4
};


// Nebulas Ray (Japan, NR1)

static struct BurnRomInfo nebulrayjRomDesc[] = {
	{ "nr1_mprl.15b",	0x080000, 0xfae5f62c, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "nr1_mpru.13b",	0x080000, 0x42ef71f9, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "nr1-spr0",		0x020000, 0x1cc2b44b, 3 | BRF_PRG | BRF_ESS }, //  2 c75data

	{ "nr1-voi0",		0x200000, 0x332d5e26, 1 | BRF_SND },           //  3 c352

	{ "nr1obj0l",		0x200000, 0x0e99ef46, 1 | BRF_GRA },           //  4 c355spr
	{ "nr1obj0u",		0x200000, 0xfb82a881, 1 | BRF_GRA },           //  5
	{ "nr1obj1l",		0x200000, 0xf7a898f0, 1 | BRF_GRA },           //  6
	{ "nr1obj1u",		0x200000, 0x49d9dbd7, 1 | BRF_GRA },           //  7
	{ "nr1obj2l",		0x200000, 0xb39871d1, 1 | BRF_GRA },           //  8
	{ "nr1obj2u",		0x200000, 0x8c8205b1, 1 | BRF_GRA },           //  9
	{ "nr1obj3l",		0x200000, 0xc90d13ae, 1 | BRF_GRA },           // 10
	{ "nr1obj3u",		0x200000, 0xd5918c9e, 1 | BRF_GRA },           // 11

	{ "nr1-chr0",		0x100000, 0x8d5b54ea, 2 | BRF_GRA },           // 12 c123tmap
	{ "nr1-chr1",		0x100000, 0xcd21630c, 2 | BRF_GRA },           // 13
	{ "nr1-chr2",		0x100000, 0x70a11023, 2 | BRF_GRA },           // 14
	{ "nr1-chr3",		0x100000, 0x8f4b1d51, 2 | BRF_GRA },           // 15

	{ "nr1-sha0",		0x080000, 0xca667e13, 3 | BRF_GRA },           // 16 c123tmap:mask

	{ "c366.bin",		0x000020, 0x8c96f31d, 0 | BRF_OPT },           // 17 Unknown PROM
};

STDROMPICKEXT(nebulrayj, nebulrayj, namcoc75)
STD_ROM_FN(nebulrayj)

struct BurnDriver BurnDrvNebulrayj = {
	"nebulrayj", "nebulray", "namcoc75", NULL, "1994",
	"Nebulas Ray (Japan, NR1)\0", NULL, "Namco", "NB-1 / NB-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, nebulrayjRomInfo, nebulrayjRomName, NULL, NULL, NULL, NULL, Namconb1InputInfo, Namconb1DIPInfo,
	NebulrayInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	224, 288, 3, 4
};


// Point Blank (World, GN2 Rev B, set 1)

static struct BurnRomInfo ptblankRomDesc[] = {
	{ "gn2_mprlb.15b",	0x080000, 0xfe2d9425, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "gn2_mprub.13b",	0x080000, 0x3bf4985a, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "gn1-spr0.5b",	0x080000, 0x71773811, 3 | BRF_PRG | BRF_ESS }, //  2 c75data

	{ "gn1-voi0.5j",	0x200000, 0x05477eb7, 1 | BRF_SND },           //  3 c352

	{ "gn1obj0l.ic1",	0x200000, 0x06722dc8, 1 | BRF_GRA },           //  4 c355spr
	{ "gn1obj0u.ic2",	0x200000, 0xfcefc909, 1 | BRF_GRA },           //  5
	{ "gn1obj1l.ic3",	0x200000, 0x48468df7, 1 | BRF_GRA },           //  6
	{ "gn1obj1u.ic4",	0x200000, 0x3109a071, 1 | BRF_GRA },           //  7

	{ "gn1-chr0.8j",	0x100000, 0xa5c61246, 2 | BRF_GRA },           //  8 c123tmap
	{ "gn1-chr1.9j",	0x100000, 0xc8c59772, 2 | BRF_GRA },           //  9
	{ "gn1-chr2.10j",	0x100000, 0xdc96d999, 2 | BRF_GRA },           // 10
	{ "gn1-chr3.11j",	0x100000, 0x4352c308, 2 | BRF_GRA },           // 11

	{ "gn1-sha0.5m",	0x080000, 0x86d4ff85, 3 | BRF_GRA },           // 12 c123tmap:mask

	{ "eeprom",			0x000800, 0x95760d0f, 0 | BRF_PRG | BRF_ESS }, // 13 eeprom
};

STDROMPICKEXT(ptblank, ptblank, namcoc75)
STD_ROM_FN(ptblank)

struct BurnDriver BurnDrvPtblank = {
	"ptblank", NULL, "namcoc75", NULL, "1994",
	"Point Blank (World, GN2 Rev B, set 1)\0", NULL, "Namco", "NB-1 / NB-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, ptblankRomInfo, ptblankRomName, NULL, NULL, NULL, NULL, GunbuletInputInfo, GunbuletDIPInfo,
	PtblankInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};


// Point Blank (World, GN2 Rev B, set 2)

static struct BurnRomInfo ptblankaRomDesc[] = {
	{ "nr3_spr0.15b",	0x080000, 0xfe2d9425, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "nr2_spr0.13b",	0x080000, 0x3bf4985a, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "nr1_spr0.5b",	0x080000, 0xa0bde3fb, 3 | BRF_PRG | BRF_ESS }, //  2 c75data

	{ "nr4_spr0.5j",	0x200000, 0x05477eb7, 1 | BRF_SND },           //  3 c352

	{ "gn1obj0l.ic1",	0x200000, 0x06722dc8, 1 | BRF_GRA },           //  4 c355spr
	{ "gn1obj0u.ic2",	0x200000, 0xfcefc909, 1 | BRF_GRA },           //  5
	{ "gn1obj1l.ic3",	0x200000, 0x48468df7, 1 | BRF_GRA },           //  6
	{ "gn1obj1u.ic4",	0x200000, 0x3109a071, 1 | BRF_GRA },           //  7

	{ "nr5_spr0.8j",	0x100000, 0xa5c61246, 2 | BRF_GRA },           //  8 c123tmap
	{ "nr6_spr0.9j",	0x100000, 0xc8c59772, 2 | BRF_GRA },           //  9
	{ "nr7_spr0.10j",	0x100000, 0xdc96d999, 2 | BRF_GRA },           // 10
	{ "nr8_spr0.11j",	0x100000, 0x4352c308, 2 | BRF_GRA },           // 11

	{ "nr9_spr0.5m",	0x080000, 0x86d4ff85, 3 | BRF_GRA },           // 12 c123tmap:mask

	{ "eeprom",			0x000800, 0x95760d0f, 0 | BRF_PRG | BRF_ESS }, // 13 eeprom
};

STDROMPICKEXT(ptblanka, ptblanka, namcoc75)
STD_ROM_FN(ptblanka)

struct BurnDriver BurnDrvPtblanka = {
	"ptblanka", "ptblank", "namcoc75", NULL, "1994",
	"Point Blank (World, GN2 Rev B, set 2)\0", NULL, "Namco", "NB-1 / NB-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, ptblankaRomInfo, ptblankaRomName, NULL, NULL, NULL, NULL, GunbuletInputInfo, GunbuletDIPInfo,
	PtblankInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};


// Gun Bullet (World, GN3 Rev B)

static struct BurnRomInfo gunbuletwRomDesc[] = {
	{ "gn3_mprlb.15b",	0x080000, 0x9260fce5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "gn3_mprub.13b",	0x080000, 0x6c1ac697, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "gn1-spr0.5b",	0x080000, 0x71773811, 3 | BRF_PRG | BRF_ESS }, //  2 c75data

	{ "gn1-voi0.5j",	0x200000, 0x05477eb7, 1 | BRF_SND },           //  3 c352

	{ "gn1obj0l.ic1",	0x200000, 0x06722dc8, 1 | BRF_GRA },           //  4 c355spr
	{ "gn1obj0u.ic2",	0x200000, 0xfcefc909, 1 | BRF_GRA },           //  5
	{ "gn1obj1l.ic3",	0x200000, 0x48468df7, 1 | BRF_GRA },           //  6
	{ "gn1obj1u.ic4",	0x200000, 0x3109a071, 1 | BRF_GRA },           //  7

	{ "gn1-chr0.8j",	0x100000, 0xa5c61246, 2 | BRF_GRA },           //  8 c123tmap
	{ "gn1-chr1.9j",	0x100000, 0xc8c59772, 2 | BRF_GRA },           //  9
	{ "gn1-chr2.10j",	0x100000, 0xdc96d999, 2 | BRF_GRA },           // 10
	{ "gn1-chr3.11j",	0x100000, 0x4352c308, 2 | BRF_GRA },           // 11

	{ "gn1-sha0.5m",	0x080000, 0x86d4ff85, 3 | BRF_GRA },           // 12 c123tmap:mask

	{ "eeprom",			0x000800, 0x95760d0f, 0 | BRF_PRG | BRF_ESS }, // 13 eeprom
};

STDROMPICKEXT(gunbuletw, gunbuletw, namcoc75)
STD_ROM_FN(gunbuletw)

struct BurnDriver BurnDrvGunbuletw = {
	"gunbuletw", "ptblank", "namcoc75", NULL, "1994",
	"Gun Bullet (World, GN3 Rev B)\0", NULL, "Namco", "NB-1 / NB-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, gunbuletwRomInfo, gunbuletwRomName, NULL, NULL, NULL, NULL, GunbuletInputInfo, GunbuletDIPInfo,
	PtblankInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};


// Gun Bullet (Japan, GN1)

static struct BurnRomInfo gunbuletjRomDesc[] = {
	{ "gn1_mprl.15b",	0x080000, 0xf99e309e, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "gn1_mpru.13b",	0x080000, 0x72a4db07, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "gn1_spr0.5b",	0x020000, 0x6836ba38, 2 | BRF_PRG | BRF_ESS }, //  2 c75data

	{ "gn1-voi0.5j",	0x200000, 0x05477eb7, 1 | BRF_SND },           //  3 c352

	{ "gn1obj0l.ic1",	0x200000, 0x06722dc8, 1 | BRF_GRA },           //  4 c355spr
	{ "gn1obj0u.ic2",	0x200000, 0xfcefc909, 1 | BRF_GRA },           //  5
	{ "gn1obj1l.ic3",	0x200000, 0x48468df7, 1 | BRF_GRA },           //  6
	{ "gn1obj1u.ic4",	0x200000, 0x3109a071, 1 | BRF_GRA },           //  7

	{ "gn1-chr0.8j",	0x100000, 0xa5c61246, 2 | BRF_GRA },           //  8 c123tmap
	{ "gn1-chr1.9j",	0x100000, 0xc8c59772, 2 | BRF_GRA },           //  9
	{ "gn1-chr2.10j",	0x100000, 0xdc96d999, 2 | BRF_GRA },           // 10
	{ "gn1-chr3.11j",	0x100000, 0x4352c308, 2 | BRF_GRA },           // 11

	{ "gn1-sha0.5m",	0x080000, 0x86d4ff85, 3 | BRF_GRA },           // 12 c123tmap:mask

	{ "eeprom",			0x000800, 0x95760d0f, 0 | BRF_PRG | BRF_ESS }, // 13 eeprom
};

STDROMPICKEXT(gunbuletj, gunbuletj, namcoc75)
STD_ROM_FN(gunbuletj)

struct BurnDriver BurnDrvGunbuletj = {
	"gunbuletj", "ptblank", "namcoc75", NULL, "1994",
	"Gun Bullet (Japan, GN1)\0", NULL, "Namco", "NB-1 / NB-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, gunbuletjRomInfo, gunbuletjRomName, NULL, NULL, NULL, NULL, GunbuletInputInfo, GunbuletDIPInfo,
	PtblankInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};


// Great Sluggers (Japan)

static struct BurnRomInfo gslugrsjRomDesc[] = {
	{ "gs1mprl.15b",	0x080000, 0x1e6c3626, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "gs1mpru.13b",	0x080000, 0xef355179, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "gs1spr0.5b",		0x080000, 0x561ea20f, 3 | BRF_PRG | BRF_ESS }, //  2 c75data

	{ "gs1voi-0.5j",	0x200000, 0x6f8262aa, 1 | BRF_SND },           //  3 c352

	{ "gs1obj-0.ic1",	0x200000, 0x9a55238f, 1 | BRF_GRA },           //  4 c355spr
	{ "gs1obj-1.ic2",	0x200000, 0x31c66f76, 1 | BRF_GRA },           //  5

	{ "gs1chr-0.8j",	0x100000, 0xe7ced86a, 2 | BRF_GRA },           //  6 c123tmap
	{ "gs1chr-1.9j",	0x100000, 0x1fe46749, 2 | BRF_GRA },           //  7
	{ "gs1chr-2.10j",	0x100000, 0xf53afa20, 2 | BRF_GRA },           //  8
	{ "gs1chr-3.11j",	0x100000, 0xb149d7da, 2 | BRF_GRA },           //  9

	{ "gs1sha-0.5m",	0x080000, 0x8a2832fe, 3 | BRF_GRA },           // 10 c123tmap:mask
};

STDROMPICKEXT(gslugrsj, gslugrsj, namcoc75)
STD_ROM_FN(gslugrsj)

static INT32 GslugrsjInit()
{
	return B1SportsInit(gslgr94u_cuskey_callback);
}

struct BurnDriver BurnDrvGslugrsj = {
	"gslugrsj", NULL, "namcoc75", NULL, "1993",
	"Great Sluggers (Japan)\0", NULL, "Namco", "NB-1 / NB-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, gslugrsjRomInfo, gslugrsjRomName, NULL, NULL, NULL, NULL, Namconb1InputInfo, Namconb1DIPInfo,
	GslugrsjInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};


// Great Sluggers '94

static struct BurnRomInfo gslgr94uRomDesc[] = {
	{ "gse2mprl.15b",	0x080000, 0xa514349c, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "gse2mpru.13b",	0x080000, 0xb6afd238, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "gse2spr0.bin",	0x020000, 0x17e87cfc, 3 | BRF_PRG | BRF_ESS }, //  2 c75data

	{ "gse-voi0.bin",	0x200000, 0xd3480574, 1 | BRF_SND },           //  3 c352

	{ "gseobj0l.bin",	0x200000, 0x531520ca, 1 | BRF_GRA },           //  4 c355spr
	{ "gseobj0u.bin",	0x200000, 0xfcc1283c, 1 | BRF_GRA },           //  5

	{ "gse-chr0.bin",	0x100000, 0x9314085d, 2 | BRF_GRA },           //  6 c123tmap
	{ "gse-chr1.bin",	0x100000, 0xc128a887, 2 | BRF_GRA },           //  7
	{ "gse-chr2.bin",	0x100000, 0x48f0a311, 2 | BRF_GRA },           //  8
	{ "gse-chr3.bin",	0x100000, 0xadbd1f88, 2 | BRF_GRA },           //  9

	{ "gse-sha0.bin",	0x080000, 0x6b2beabb, 3 | BRF_GRA },           // 10 c123tmap:mask
};

STDROMPICKEXT(gslgr94u, gslgr94u, namcoc75)
STD_ROM_FN(gslgr94u)

static INT32 Gslgr94uInit()
{
	return B1SportsInit(gslgr94u_cuskey_callback);
}

struct BurnDriver BurnDrvGslgr94u = {
	"gslgr94u", NULL, "namcoc75", NULL, "1994",
	"Great Sluggers '94\0", NULL, "Namco", "NB-1 / NB-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, gslgr94uRomInfo, gslgr94uRomName, NULL, NULL, NULL, NULL, Namconb1InputInfo, Namconb1DIPInfo,
	Gslgr94uInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};


// Great Sluggers '94 (Japan)

static struct BurnRomInfo gslgr94jRomDesc[] = {
	{ "gs41mprl.15b",	0x080000, 0x5759bdb5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "gs41mpru.13b",	0x080000, 0x78bde1e7, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "gs41spr0.5b",	0x080000, 0x3e2b6d55, 3 | BRF_PRG | BRF_ESS }, //  2 c75data

	{ "gs4voi0.5j",		0x200000, 0xc3053a90, 1 | BRF_SND },           //  3 c352

	{ "gs4obj0l.bin",	0x200000, 0x3b499da0, 1 | BRF_GRA },           //  4 c355spr
	{ "gs4obj0u.bin",	0x200000, 0x80016b50, 1 | BRF_GRA },           //  5
	{ "gs4obj1l.bin",	0x200000, 0x1f4847a7, 1 | BRF_GRA },           //  6
	{ "gs4obj1u.bin",	0x200000, 0x49bc48cd, 1 | BRF_GRA },           //  7

	{ "gs4chr0.8j",		0x100000, 0x8c6c682e, 2 | BRF_GRA },           //  8 c123tmap
	{ "gs4chr1.9j",		0x100000, 0x523989f7, 2 | BRF_GRA },           //  9
	{ "gs4chr2.10j",	0x100000, 0x37569559, 2 | BRF_GRA },           // 10
	{ "gs4chr3.11j",	0x100000, 0x73ca58f6, 2 | BRF_GRA },           // 11

	{ "gs4sha0.5m",		0x080000, 0x40e7e6a5, 3 | BRF_GRA },           // 12 c123tmap:mask
};

STDROMPICKEXT(gslgr94j, gslgr94j, namcoc75)
STD_ROM_FN(gslgr94j)

static INT32 Gslgr94jInit()
{
	return B1SportsInit(gslgr94j_cuskey_callback);
}

struct BurnDriver BurnDrvGslgr94j = {
	"gslgr94j", "gslgr94u", "namcoc75", NULL, "1994",
	"Great Sluggers '94 (Japan)\0", NULL, "Namco", "NB-1 / NB-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, gslgr94jRomInfo, gslgr94jRomName, NULL, NULL, NULL, NULL, Namconb1InputInfo, Namconb1DIPInfo,
	Gslgr94jInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};


// Super World Stadium '95 (Japan)

static struct BurnRomInfo sws95RomDesc[] = {
	{ "ss51mprl.bin",	0x080000, 0xc9e0107d, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "ss51mpru.bin",	0x080000, 0x0d93d261, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ss51spr0.bin",	0x080000, 0x71cb12f5, 3 | BRF_PRG | BRF_ESS }, //  2 c75data

	{ "ss51voi0.bin",	0x200000, 0x2740ec72, 1 | BRF_SND },           //  3 c352

	{ "ss51ob0l.bin",	0x200000, 0xe0395694, 1 | BRF_GRA },           //  4 c355spr
	{ "ss51ob0u.bin",	0x200000, 0xb0745ca0, 1 | BRF_GRA },           //  5

	{ "ss51chr0.bin",	0x100000, 0x86dd3280, 2 | BRF_GRA },           //  6 c123tmap
	{ "ss51chr1.bin",	0x100000, 0x2ba0fb9e, 2 | BRF_GRA },           //  7
	{ "ss51chr2.bin",	0x100000, 0xca0e6c1a, 2 | BRF_GRA },           //  8
	{ "ss51chr3.bin",	0x100000, 0x73ca58f6, 2 | BRF_GRA },           //  9

	{ "ss51sha0.bin",	0x080000, 0x3bf4d081, 3 | BRF_GRA },           // 10 c123tmap:mask
};

STDROMPICKEXT(sws95, sws95, namcoc75)
STD_ROM_FN(sws95)

static INT32 Sws95Init()
{
	return B1SportsInit(sws95_cuskey_callback);
}

struct BurnDriver BurnDrvSws95 = {
	"sws95", NULL, "namcoc75", NULL, "1995",
	"Super World Stadium '95 (Japan)\0", NULL, "Namco", "NB-1 / NB-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, sws95RomInfo, sws95RomName, NULL, NULL, NULL, NULL, Namconb1InputInfo, Namconb1DIPInfo,
	Sws95Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};


// Super World Stadium '96 (Japan)

static struct BurnRomInfo sws96RomDesc[] = {
	{ "ss61mprl.bin",	0x080000, 0x06f55e73, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "ss61mpru.bin",	0x080000, 0x0abdbb83, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ss61spr0.bin",	0x080000, 0x71cb12f5, 3 | BRF_PRG | BRF_ESS }, //  2 c75data

	{ "ss61voi0.bin",	0x200000, 0x2740ec72, 1 | BRF_SND },           //  3 c352

	{ "ss61ob0l.bin",	0x200000, 0x579b19d4, 1 | BRF_GRA },           //  4 c355spr
	{ "ss61ob0u.bin",	0x200000, 0xa69bbd9e, 1 | BRF_GRA },           //  5

	{ "ss61chr0.bin",	0x100000, 0x9d2ae07b, 2 | BRF_GRA },           //  6 c123tmap
	{ "ss61chr1.bin",	0x100000, 0x4dc75da6, 2 | BRF_GRA },           //  7
	{ "ss61chr2.bin",	0x100000, 0x1240704b, 2 | BRF_GRA },           //  8
	{ "ss61chr3.bin",	0x100000, 0x066581d4, 2 | BRF_GRA },           //  9

	{ "ss61sha0.bin",	0x080000, 0xfceaa19c, 3 | BRF_GRA },           // 10 c123tmap:mask
};

STDROMPICKEXT(sws96, sws96, namcoc75)
STD_ROM_FN(sws96)

static INT32 Sws96Init()
{
	return B1SportsInit(sws96_cuskey_callback);
}

struct BurnDriver BurnDrvSws96 = {
	"sws96", NULL, "namcoc75", NULL, "1996",
	"Super World Stadium '96 (Japan)\0", NULL, "Namco", "NB-1 / NB-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, sws96RomInfo, sws96RomName, NULL, NULL, NULL, NULL, Namconb1InputInfo, Namconb1DIPInfo,
	Sws96Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};


// Super World Stadium '97 (Japan)

static struct BurnRomInfo sws97RomDesc[] = {
	{ "ss71mprl.bin",	0x080000, 0xbd60b50e, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "ss71mpru.bin",	0x080000, 0x3444f5a8, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ss71spr0.bin",	0x080000, 0x71cb12f5, 3 | BRF_PRG | BRF_ESS }, //  2 c75data

	{ "ss71voi0.bin",	0x200000, 0x2740ec72, 1 | BRF_SND },           //  3 c352

	{ "ss71ob0l.bin",	0x200000, 0x9559ad44, 1 | BRF_GRA },           //  4 c355spr
	{ "ss71ob0u.bin",	0x200000, 0x4df4a722, 1 | BRF_GRA },           //  5

	{ "ss71chr0.bin",	0x100000, 0xbd606356, 2 | BRF_GRA },           //  6 c123tmap
	{ "ss71chr1.bin",	0x100000, 0x4dc75da6, 2 | BRF_GRA },           //  7
	{ "ss71chr2.bin",	0x100000, 0x1240704b, 2 | BRF_GRA },           //  8
	{ "ss71chr3.bin",	0x100000, 0x066581d4, 2 | BRF_GRA },           //  9

	{ "ss71sha0.bin",	0x080000, 0xbe8c2758, 3 | BRF_GRA },           // 10 c123tmap:mask
};

STDROMPICKEXT(sws97, sws97, namcoc75)
STD_ROM_FN(sws97)

static INT32 Sws97Init()
{
	return B1SportsInit(sws97_cuskey_callback);
}

struct BurnDriver BurnDrvSws97 = {
	"sws97", NULL, "namcoc75", NULL, "1997",
	"Super World Stadium '97 (Japan)\0", NULL, "Namco", "NB-1 / NB-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, sws97RomInfo, sws97RomName, NULL, NULL, NULL, NULL, Namconb1InputInfo, Namconb1DIPInfo,
	Sws97Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};


// J-League Soccer V-Shoot (Japan)

static struct BurnRomInfo vshootRomDesc[] = {
	{ "vsj1mprl.15b",	0x080000, 0x83a60d92, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "vsj1mpru.13b",	0x080000, 0xc63eb92d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "vsj1spr0.5b",	0x080000, 0xb0c71aa6, 3 | BRF_PRG | BRF_ESS }, //  2 c75data

	{ "vsjvoi-0.5j",	0x200000, 0x0528c9ed, 1 | BRF_SND },           //  3 c352

	{ "vsjobj0l.ic1",	0x200000, 0xe134faa7, 1 | BRF_GRA },           //  4 c355spr
	{ "vsjobj0u.ic2",	0x200000, 0x974d0714, 1 | BRF_GRA },           //  5
	{ "vsjobj1l.ic3",	0x200000, 0xba46f967, 1 | BRF_GRA },           //  6
	{ "vsjobj1u.ic4",	0x200000, 0x09da7e9c, 1 | BRF_GRA },           //  7

	{ "vsjchr-0.8j",	0x100000, 0x2af8ba7c, 2 | BRF_GRA },           //  8 c123tmap
	{ "vsjchr-1.9j",	0x100000, 0xb789d53e, 2 | BRF_GRA },           //  9
	{ "vsjchr-2.10j",	0x100000, 0x7ef80758, 2 | BRF_GRA },           // 10
	{ "vsjchr-3.11j",	0x100000, 0x73ca58f6, 2 | BRF_GRA },           // 11

	{ "vsjsha-0.5m",	0x080000, 0x78335ea4, 3 | BRF_GRA },           // 12 c123tmap:mask
};

STDROMPICKEXT(vshoot, vshoot, namcoc75)
STD_ROM_FN(vshoot)

struct BurnDriver BurnDrvVshoot = {
	"vshoot", NULL, "namcoc75", NULL, "1994",
	"J-League Soccer V-Shoot (Japan)\0", NULL, "Namco", "NB-1 / NB-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SPORTSFOOTBALL, 0,
	NULL, vshootRomInfo, vshootRomName, NULL, NULL, NULL, NULL, Namconb1InputInfo, Namconb1DIPInfo,
	VshootInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};


// The Outfoxies (World, OU2)

static struct BurnRomInfo outfxiesRomDesc[] = {
	{ "ou2_mprl.11c",	0x080000, 0xf414a32e, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "ou2_mpru.11d",	0x080000, 0xab5083fb, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ou1spr0.5b",		0x080000, 0x60cee566, 3 | BRF_PRG | BRF_ESS }, //  2 sndcpu

	{ "ou1voi0.6n",		0x200000, 0x2d8fb271, 1 | BRF_SND },           //  3 c352

	{ "ou1shas.12s",	0x200000, 0x9bcb0397, 3 | BRF_GRA },           //  4 c123tmap:mask

	{ "ou1shar.18s",	0x200000, 0xfbb48194, 6 | BRF_GRA },           //  5 c169roz:mask

	{ "ou1obj0l.4c",	0x200000, 0x1b4f7184, 1 | BRF_GRA },           //  6 c355spr
	{ "ou1obj0u.8c",	0x200000, 0xd0a69794, 1 | BRF_GRA },           //  7
	{ "ou1obj1l.4b",	0x200000, 0x48a93e84, 1 | BRF_GRA },           //  8
	{ "ou1obj1u.8b",	0x200000, 0x999de386, 1 | BRF_GRA },           //  9
	{ "ou1obj2l.4a",	0x200000, 0x30386cd0, 1 | BRF_GRA },           // 10
	{ "ou1obj2u.8a",	0x200000, 0xccada5f8, 1 | BRF_GRA },           // 11
	{ "ou1obj3l.6c",	0x200000, 0x5f41b44e, 1 | BRF_GRA },           // 12
	{ "ou1obj3u.9c",	0x200000, 0xbc852c8e, 1 | BRF_GRA },           // 13
	{ "ou1obj4l.6b",	0x200000, 0x99a5f9d7, 1 | BRF_GRA },           // 14
	{ "ou1obj4u.9b",	0x200000, 0x70ecaabb, 1 | BRF_GRA },           // 15

	{ "ou1-rot0.3d",	0x200000, 0xa50c67c8, 5 | BRF_GRA },           // 16 c169roz
	{ "ou1-rot1.3c",	0x200000, 0x14866780, 5 | BRF_GRA },           // 17
	{ "ou1-rot2.3b",	0x200000, 0x55ccf3af, 5 | BRF_GRA },           // 18

	{ "ou1-scr0.1d",	0x200000, 0xb3b3f2e9, 2 | BRF_GRA },           // 19 c123tmap

	{ "ou1dat0.20a",	0x080000, 0x1a49aead, 4 | BRF_PRG | BRF_ESS }, // 20 data
	{ "ou1dat1.20b",	0x080000, 0x63bb119d, 4 | BRF_PRG | BRF_ESS }, // 21
};

STDROMPICKEXT(outfxies, outfxies, namcoc75)
STD_ROM_FN(outfxies)

struct BurnDriver BurnDrvOutfxies = {
	"outfxies", NULL, "namcoc75", NULL, "1994",
	"The Outfoxies (World, OU2)\0", NULL, "Namco", "NB-1 / NB-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_RUNGUN, 0,
	NULL, outfxiesRomInfo, outfxiesRomName, NULL, NULL, NULL, NULL, Namconb1InputInfo, Namconb1DIPInfo,
	OutfxiesInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};


// The Outfoxies (Japan, OU1)

static struct BurnRomInfo outfxiesjRomDesc[] = {
	{ "ou1_mprl.11c",	0x080000, 0xd3b9e530, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "ou1_mpru.11d",	0x080000, 0xd98308fb, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ou1spr0.5b",		0x080000, 0x60cee566, 3 | BRF_PRG | BRF_ESS }, //  2 c75data

	{ "ou1voi0.6n",		0x200000, 0x2d8fb271, 1 | BRF_SND },           //  3 c352

	{ "ou1shas.12s",	0x200000, 0x9bcb0397, 3 | BRF_GRA },           //  4 c123tmap:mask

	{ "ou1shar.18s",	0x200000, 0xfbb48194, 6 | BRF_GRA },           //  5 c169roz:mask

	{ "ou1obj0l.4c",	0x200000, 0x1b4f7184, 1 | BRF_GRA },           //  6 c355spr
	{ "ou1obj0u.8c",	0x200000, 0xd0a69794, 1 | BRF_GRA },           //  7
	{ "ou1obj1l.4b",	0x200000, 0x48a93e84, 1 | BRF_GRA },           //  8
	{ "ou1obj1u.8b",	0x200000, 0x999de386, 1 | BRF_GRA },           //  9
	{ "ou1obj2l.4a",	0x200000, 0x30386cd0, 1 | BRF_GRA },           // 10
	{ "ou1obj2u.8a",	0x200000, 0xccada5f8, 1 | BRF_GRA },           // 11
	{ "ou1obj3l.6c",	0x200000, 0x5f41b44e, 1 | BRF_GRA },           // 12
	{ "ou1obj3u.9c",	0x200000, 0xbc852c8e, 1 | BRF_GRA },           // 13
	{ "ou1obj4l.6b",	0x200000, 0x99a5f9d7, 1 | BRF_GRA },           // 14
	{ "ou1obj4u.9b",	0x200000, 0x70ecaabb, 1 | BRF_GRA },           // 15

	{ "ou1-rot0.3d",	0x200000, 0xa50c67c8, 5 | BRF_GRA },           // 16 c169roz
	{ "ou1-rot1.3c",	0x200000, 0x14866780, 5 | BRF_GRA },           // 17
	{ "ou1-rot2.3b",	0x200000, 0x55ccf3af, 5 | BRF_GRA },           // 18

	{ "ou1-scr0.1d",	0x200000, 0xb3b3f2e9, 2 | BRF_GRA },           // 19 c123tmap

	{ "ou1dat0.20a",	0x080000, 0x1a49aead, 4 | BRF_PRG | BRF_ESS }, // 20 data
	{ "ou1dat1.20b",	0x080000, 0x63bb119d, 4 | BRF_PRG | BRF_ESS }, // 21
};

STDROMPICKEXT(outfxiesj, outfxiesj, namcoc75)
STD_ROM_FN(outfxiesj)

struct BurnDriver BurnDrvOutfxiesj = {
	"outfxiesj", "outfxies", "namcoc75", NULL, "1994",
	"The Outfoxies (Japan, OU1)\0", NULL, "Namco", "NB-1 / NB-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_RUNGUN, 0,
	NULL, outfxiesjRomInfo, outfxiesjRomName, NULL, NULL, NULL, NULL, Namconb1InputInfo, Namconb1DIPInfo,
	OutfxiesInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};


// The Outfoxies (Japan, OU1, alternate GFX ROMs)
// this set uses different "c355spr", "c169roz" and "c123tmap" region ROMs. The rest of the ROMs is identical. It was found on 2 different PCB sets.

static struct BurnRomInfo outfxiesjaRomDesc[] = {
	{ "ou1_mprl.11c",	0x080000, 0xd3b9e530, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "ou1_mpru.11d",	0x080000, 0xd98308fb, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ou1spr0.5b",		0x080000, 0x60cee566, 3 | BRF_PRG | BRF_ESS }, //  2 c75data

	{ "ou1voi0.6n",		0x200000, 0x2d8fb271, 1 | BRF_SND },           //  3 c352

	{ "ou1shas.12s",	0x200000, 0x9bcb0397, 3 | BRF_GRA },           //  4 c123tmap:mask

	{ "ou1shar.18s",	0x200000, 0xfbb48194, 6 | BRF_GRA },           //  5 c169roz:mask

	{ "ou1obj0l.4c",	0x200000, 0xecf135e4, 1 | BRF_GRA },           //  6 c355spr
	{ "ou1obj0u.8c",	0x200000, 0x229fe774, 1 | BRF_GRA },           //  7
	{ "ou1obj1l.4b",	0x200000, 0x0b9d060e, 1 | BRF_GRA },           //  8
	{ "ou1obj1u.8b",	0x200000, 0x34a0feca, 1 | BRF_GRA },           //  9
	{ "ou1obj2l.4a",	0x200000, 0x128119c4, 1 | BRF_GRA },           // 10
	{ "ou1obj2u.8a",	0x200000, 0xce74c385, 1 | BRF_GRA },           // 11
	{ "ou1obj3l.6c",	0x200000, 0x39d9aa54, 1 | BRF_GRA },           // 12
	{ "ou1obj3u.9c",	0x200000, 0x022f4a73, 1 | BRF_GRA },           // 13
	{ "ou1obj4l.6b",	0x200000, 0xb26fbb92, 1 | BRF_GRA },           // 14
	{ "ou1obj4u.9b",	0x200000, 0xad99607d, 1 | BRF_GRA },           // 15

	{ "ou1-rot0.3d",	0x200000, 0x30511ffb, 5 | BRF_GRA },           // 16 c169roz
	{ "ou1-rot1.3c",	0x200000, 0xf4b61c22, 5 | BRF_GRA },           // 17
	{ "ou1-rot2.3b",	0x200000, 0xd48b29d8, 5 | BRF_GRA },           // 18

	{ "ou1-scr0.1d",	0x200000, 0x692b63f8, 2 | BRF_GRA },           // 19 c123tmap

	{ "ou1dat0.20a",	0x080000, 0x1a49aead, 4 | BRF_PRG | BRF_ESS }, // 20 data
	{ "ou1dat1.20b",	0x080000, 0x63bb119d, 4 | BRF_PRG | BRF_ESS }, // 21
};

STDROMPICKEXT(outfxiesja, outfxiesja, namcoc75)
STD_ROM_FN(outfxiesja)

struct BurnDriver BurnDrvOutfxiesja = {
	"outfxiesja", "outfxies", "namcoc75", NULL, "1994",
	"The Outfoxies (Japan, OU1, alternate GFX ROMs)\0", NULL, "Namco", "NB-1 / NB-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_RUNGUN, 0,
	NULL, outfxiesjaRomInfo, outfxiesjaRomName, NULL, NULL, NULL, NULL, Namconb1InputInfo, Namconb1DIPInfo,
	OutfxiesjaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};


// The Outfoxies (Korea?)

static struct BurnRomInfo outfxiesaRomDesc[] = {
	{ "mprl.11c",		0x080000, 0x22cd638d, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "mpru.11d",		0x080000, 0xa50f1cf9, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ou1spr0.5b",		0x080000, 0x60cee566, 3 | BRF_PRG | BRF_ESS }, //  2 c75data

	{ "ou1voi0.6n",		0x200000, 0x2d8fb271, 1 | BRF_SND },           //  3 c352

	{ "ou1shas.12s",	0x200000, 0x9bcb0397, 3 | BRF_GRA },           //  4 c123tmap:mask

	{ "ou1shar.18s",	0x200000, 0xfbb48194, 6 | BRF_GRA },           //  5 c169roz:mask

	{ "ou1obj0l.4c",	0x200000, 0x1b4f7184, 1 | BRF_GRA },           //  6 c355spr
	{ "ou1obj0u.8c",	0x200000, 0xd0a69794, 1 | BRF_GRA },           //  7
	{ "ou1obj1l.4b",	0x200000, 0x48a93e84, 1 | BRF_GRA },           //  8
	{ "ou1obj1u.8b",	0x200000, 0x999de386, 1 | BRF_GRA },           //  9
	{ "ou1obj2l.4a",	0x200000, 0x30386cd0, 1 | BRF_GRA },           // 10
	{ "ou1obj2u.8a",	0x200000, 0xccada5f8, 1 | BRF_GRA },           // 11
	{ "ou1obj3l.6c",	0x200000, 0x5f41b44e, 1 | BRF_GRA },           // 12
	{ "ou1obj3u.9c",	0x200000, 0xbc852c8e, 1 | BRF_GRA },           // 13
	{ "ou1obj4l.6b",	0x200000, 0x99a5f9d7, 1 | BRF_GRA },           // 14
	{ "ou1obj4u.9b",	0x200000, 0x70ecaabb, 1 | BRF_GRA },           // 15

	{ "ou1-rot0.3d",	0x200000, 0xa50c67c8, 5 | BRF_GRA },           // 16 c169roz
	{ "ou1-rot1.3c",	0x200000, 0x14866780, 5 | BRF_GRA },           // 17
	{ "ou1-rot2.3b",	0x200000, 0x55ccf3af, 5 | BRF_GRA },           // 18

	{ "ou1-scr0.1d",	0x200000, 0xb3b3f2e9, 2 | BRF_GRA },           // 19 c123tmap

	{ "ou1dat0.20a",	0x080000, 0x1a49aead, 4 | BRF_PRG | BRF_ESS }, // 20 data
	{ "ou1dat1.20b",	0x080000, 0x63bb119d, 4 | BRF_PRG | BRF_ESS }, // 21
};

STDROMPICKEXT(outfxiesa, outfxiesa, namcoc75)
STD_ROM_FN(outfxiesa)

struct BurnDriver BurnDrvOutfxiesa = {
	"outfxiesa", "outfxies", "namcoc75", NULL, "1994",
	"The Outfoxies (Korea?)\0", NULL, "Namco", "NB-1 / NB-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_RUNGUN, 0,
	NULL, outfxiesaRomInfo, outfxiesaRomName, NULL, NULL, NULL, NULL, Namconb1InputInfo, Namconb1DIPInfo,
	OutfxiesInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};


// Mach Breakers (World, MB2)

static struct BurnRomInfo machbrkrRomDesc[] = {
	{ "mb2_mprl.11c",	0x080000, 0x81e2c566, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "mb2_mpru.11d",	0x080000, 0xe8ccec89, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "mb1_spr0.5b",	0x080000, 0xd10f6272, 3 | BRF_PRG | BRF_ESS }, //  2 c75data

	{ "mb1_voi0.6n",	0x200000, 0xd363ca3b, 1 | BRF_SND },           //  3 c352
	{ "mb1_voi1.6p",	0x200000, 0x7e1c2603, 1 | BRF_SND },           //  4

	{ "mb1_shas.12s",	0x100000, 0xc51c614b, 3 | BRF_GRA },           //  5 c123tmap:mask

	{ "mb1_shar.18s",	0x080000, 0xd9329b10, 6 | BRF_GRA },           //  6 c169roz:mask

	{ "mb1obj0l.4c",	0x200000, 0x056e6b1c, 1 | BRF_GRA },           //  7 c355spr
	{ "mb1obj0u.8c",	0x200000, 0xe19b1714, 1 | BRF_GRA },           //  8
	{ "mb1obj1l.4b",	0x200000, 0xaf69f7f1, 1 | BRF_GRA },           //  9
	{ "mb1obj1u.8b",	0x200000, 0xe8ff9082, 1 | BRF_GRA },           // 10
	{ "mb1obj2l.4a",	0x200000, 0x3a5c7379, 1 | BRF_GRA },           // 11
	{ "mb1obj2u.8a",	0x200000, 0xb59cf5e0, 1 | BRF_GRA },           // 12
	{ "mb1obj3l.6c",	0x200000, 0x9a765d58, 1 | BRF_GRA },           // 13
	{ "mb1obj3u.9c",	0x200000, 0x5329c693, 1 | BRF_GRA },           // 14
	{ "mb1obj4l.6b",	0x200000, 0xa650b05e, 1 | BRF_GRA },           // 15
	{ "mb1obj4u.9b",	0x200000, 0x6d0c37e9, 1 | BRF_GRA },           // 16

	{ "mb1_rot0.3d",	0x200000, 0xbc353630, 5 | BRF_GRA },           // 17 c169roz
	{ "mb1_rot1.3c",	0x200000, 0xcf7688cb, 5 | BRF_GRA },           // 18

	{ "mb1_scr0.1d",	0x200000, 0xc678d5f3, 2 | BRF_GRA },           // 19 c123tmap
	{ "mb1_scr1.1c",	0x200000, 0xfb2b1939, 2 | BRF_GRA },           // 20
	{ "mb1_scr2.1b",	0x200000, 0x0e6097a5, 2 | BRF_GRA },           // 21

	{ "mb1_dat0.20a",	0x080000, 0xfb2e3cd1, 4 | BRF_PRG | BRF_ESS }, // 22 data
};

STDROMPICKEXT(machbrkr, machbrkr, namcoc75)
STD_ROM_FN(machbrkr)

struct BurnDriver BurnDrvMachbrkr = {
	"machbrkr", NULL, "namcoc75", NULL, "1995",
	"Mach Breakers (World, MB2)\0", NULL, "Namco", "NB-1 / NB-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 4, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, machbrkrRomInfo, machbrkrRomName, NULL, NULL, NULL, NULL, Namconb1InputInfo, Namconb1DIPInfo,
	MachbrkrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};


// Mach Breakers - Numan Athletics 2 (Japan, MB1)

static struct BurnRomInfo machbrkrjRomDesc[] = {
	{ "mb1_mprl.11c",	0x080000, 0x86cf0644, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "mb1_mpru.11d",	0x080000, 0xfb1ff916, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "mb1_spr0.5b",	0x080000, 0xd10f6272, 3 | BRF_PRG | BRF_ESS }, //  2 c75data

	{ "mb1_voi0.6n",	0x200000, 0xd363ca3b, 1 | BRF_SND },           //  3 c352
	{ "mb1_voi1.6p",	0x200000, 0x7e1c2603, 1 | BRF_SND },           //  4

	{ "mb1_shas.12s",	0x100000, 0xc51c614b, 3 | BRF_GRA },           //  5 c123tmap:mask

	{ "mb1_shar.18s",	0x080000, 0xd9329b10, 6 | BRF_GRA },           //  6 c169roz:mask

	{ "mb1obj0l.4c",	0x200000, 0x056e6b1c, 1 | BRF_GRA },           //  7 c355spr
	{ "mb1obj0u.8c",	0x200000, 0xe19b1714, 1 | BRF_GRA },           //  8
	{ "mb1obj1l.4b",	0x200000, 0xaf69f7f1, 1 | BRF_GRA },           //  9
	{ "mb1obj1u.8b",	0x200000, 0xe8ff9082, 1 | BRF_GRA },           // 10
	{ "mb1obj2l.4a",	0x200000, 0x3a5c7379, 1 | BRF_GRA },           // 11
	{ "mb1obj2u.8a",	0x200000, 0xb59cf5e0, 1 | BRF_GRA },           // 12
	{ "mb1obj3l.6c",	0x200000, 0x9a765d58, 1 | BRF_GRA },           // 13
	{ "mb1obj3u.9c",	0x200000, 0x5329c693, 1 | BRF_GRA },           // 14
	{ "mb1obj4l.6b",	0x200000, 0xa650b05e, 1 | BRF_GRA },           // 15
	{ "mb1obj4u.9b",	0x200000, 0x6d0c37e9, 1 | BRF_GRA },           // 16

	{ "mb1_rot0.3d",	0x200000, 0xbc353630, 5 | BRF_GRA },           // 17 c169roz
	{ "mb1_rot1.3c",	0x200000, 0xcf7688cb, 5 | BRF_GRA },           // 18

	{ "mb1_scr0.1d",	0x200000, 0xc678d5f3, 2 | BRF_GRA },           // 19 c123tmap
	{ "mb1_scr1.1c",	0x200000, 0xfb2b1939, 2 | BRF_GRA },           // 20
	{ "mb1_scr2.1b",	0x200000, 0x0e6097a5, 2 | BRF_GRA },           // 21

	{ "mb1_dat0.20a",	0x080000, 0xfb2e3cd1, 4 | BRF_PRG | BRF_ESS }, // 22 data
};

STDROMPICKEXT(machbrkrj, machbrkrj, namcoc75)
STD_ROM_FN(machbrkrj)

struct BurnDriver BurnDrvMachbrkrj = {
	"machbrkrj", "machbrkr", "namcoc75", NULL, "1995",
	"Mach Breakers - Numan Athletics 2 (Japan, MB1)\0", NULL, "Namco", "NB-1 / NB-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, machbrkrjRomInfo, machbrkrjRomName, NULL, NULL, NULL, NULL, Namconb1InputInfo, Namconb1DIPInfo,
	MachbrkrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};
