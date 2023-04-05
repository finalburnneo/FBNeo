// FB Neo Midway 68k-based driver module
// Based on MAME driver by Aaron Giles, Bryan McPhail

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "midcsd.h"
#include "dac.h"
#include "6821pia.h"
#include "6840ptm.h"
#include "watchdog.h"
#include "burn_gun.h" // dial

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvSndROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *Drv68KRAMA;
static UINT8 *Drv68KRAMB;
static UINT8 *DrvVidRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvSndRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 sound_data;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[4];
static UINT8 DrvReset;

static ButtonToggle Diag;

static INT16 Analog[1];

static INT32 nCyclesExtra[2];

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo ZwackeryInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	A("P1 Spinner",		BIT_ANALOG_REL, &Analog[0],		"p1 x-axis"),
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy3 + 6,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy3 + 7,	"p1 fire 4"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 5,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy1 + 4,	"diag"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 6,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Zwackery)

static struct BurnDIPInfo ZwackeryDIPList[]=
{
	DIP_OFFSET(0x0f)
	{0x00, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    8, "Coinage"				},
	{0x00, 0x01, 0x07, 0x05, "6 Coins 1 Credits"	},
	{0x00, 0x01, 0x07, 0x04, "5 Coins 1 Credits"	},
	{0x00, 0x01, 0x07, 0x03, "4 Coins 1 Credits"	},
	{0x00, 0x01, 0x07, 0x02, "3 Coins 1 Credits"	},
	{0x00, 0x01, 0x07, 0x01, "2 Coins 1 Credits"	},
	{0x00, 0x01, 0x07, 0x00, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0x07, 0x07, "Free Play"			},

	{0   , 0xfe, 0   ,    8, "Buy-in"				},
	{0x00, 0x01, 0x38, 0x00, "1 coin"				},
	{0x00, 0x01, 0x38, 0x08, "2 coins"				},
	{0x00, 0x01, 0x38, 0x10, "3 coins"				},
	{0x00, 0x01, 0x38, 0x18, "4 coins"				},
	{0x00, 0x01, 0x38, 0x20, "5 coins"				},
	{0x00, 0x01, 0x38, 0x28, "6 coins"				},
	{0x00, 0x01, 0x38, 0x30, "7 coins"				},
	{0x00, 0x01, 0x38, 0x38, "None"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x00, 0x01, 0xc0, 0xc0, "Easier"				},
	{0x00, 0x01, 0xc0, 0x00, "Normal"				},
	{0x00, 0x01, 0xc0, 0x40, "Harder"				},
	{0x00, 0x01, 0xc0, 0x80, "Hardest"				},
};

STDDIPINFO(Zwackery)

static void sync_ptm()
{
	INT32 cyc = (SekTotalCycles() / 10) - ptm6840TotalCycles();
	if (cyc > 0) {
		ptm6840Run(cyc);
	}
}

static void sync_csd()
{
	INT32 cyc = ((INT64)SekTotalCycles(0) * 8000000 / 7652400) - SekTotalCycles(1);
	if (cyc > 0) {
		SekRun(1, cyc);
	}
}

static void __fastcall zwackery_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfff000) == 0xc00000) {
		*((UINT16*)(DrvSprRAM + (address & 0xffe))) = data | 0xff00;
		return;
	}

	if ((address & 0xfffff0) == 0x100000) {
		sync_ptm();
		ptm6840_write((address & 0xf) >> 1, (data >> 8) & 0xff);
		return;
	}

	if ((address & 0xfffff8) == 0x104000) {
		pia_write(0, (address / 2) & 0x03, data >> 8);
		return;
	}

	if ((address & 0xfffff8) == 0x108000) {
		pia_write(1, (address / 2) & 0x03, data & 0xff);
		return;
	}

	if ((address & 0xfffff8) == 0x10c000) {
		pia_write(2, (address / 2) & 0x03, data & 0xff);
		return;
	}

	if (address < 0x80000 || address == 0x804000) return; // bad writes?

	bprintf(0, _T("mww %x  %x\n"), address, data);
}

static void __fastcall zwackery_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xfff000) == 0xc00000) {
		*((UINT16*)(DrvSprRAM + (address & 0xffe))) = data | 0xff00;
		return;
	}

	if ((address & 0xfffff0) == 0x100000) {
		sync_ptm();
		ptm6840_write((address & 0xf) >> 1, data & 0xff);
		return;
	}

	if ((address & 0xfffff8) == 0x104000) {
		pia_write(0, (address / 2) & 0x03, data);
		return;
	}

	if ((address & 0xfffff8) == 0x108000) {
		pia_write(1, (address / 2) & 0x03, data);
		return;
	}

	if ((address & 0xfffff8) == 0x10c000) {
		pia_write(2, (address / 2) & 0x03, data);
		return;
	}

	bprintf(0, _T("mwb %x  %x\n"), address, data);
}

static UINT16 __fastcall zwackery_main_read_word(UINT32 address)
{
	if ((address & 0xfffff0) == 0x100000) {
		sync_ptm();
		SekCyclesBurnRun(14);
		return (ptm6840_read((address & 0xf) >> 1) << 8) | 0x00ff;
	}

	if ((address & 0xfffff8) == 0x104000) {
		UINT16 ret = pia_read(0, (address / 2) & 0x03);
		return ret | (ret << 8);
	}

	if ((address & 0xfffff8) == 0x108000) {
		UINT16 ret = pia_read(1, (address / 2) & 0x03);
		return ret | (ret << 8);
	}

	if ((address & 0xfffff8) == 0x10c000) {
		UINT16 ret = pia_read(2, (address / 2) & 0x03);
		return ret | (ret << 8);
	}

	bprintf(0, _T("mrw %x\n"), address);

	return 0xffff;
}

static UINT8 __fastcall zwackery_main_read_byte(UINT32 address)
{
	if ((address & 0xfffff0) == 0x100000) {
		sync_ptm();
		SekCyclesBurnRun(14); // pass timer test
		return ptm6840_read((address & 0xf) >> 1);
	}

	if ((address & 0xfffff8) == 0x104000) {
		return pia_read(0, (address / 2) & 0x03);
	}

	if ((address & 0xfffff8) == 0x108000) {
		return pia_read(1, (address / 2) & 0x03);
	}

	if ((address & 0xfffff8) == 0x10c000) {
		return pia_read(2, (address / 2) & 0x03);
	}

	bprintf(0, _T("mrb %x\n"), address);

	return 0xff;
}

static UINT8 pia0_read_b(UINT16 )
{
	return DrvInputs[0];
}

static void pia0_out_a(UINT16 offset, UINT8 data)
{
	if (~data & 0x80) BurnWatchdogReset();
}

static void pia0_irq(int state)
{
	state = pia_get_irq_a_state(0) | pia_get_irq_b_state(0);
	SekSetIRQLine(5, (state) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static UINT8 pia1_read_a(UINT16 )
{
	return DrvInputs[1];
}

static UINT8 pia1_read_b(UINT16 )
{
	return (DrvInputs[2] & ~0x3e) | ((BurnTrackballRead(0, 0) >> 2) & 0x3e);
}

static void pia1_out_a(UINT16 offset, UINT8 data)
{
	sound_data = data >> 4;
}

static void pia1_out_ca2(UINT16 offset, UINT8 data)
{
	sync_csd();
	csd_data_write((data << 4) | sound_data);
}

static UINT8 pia2_read_a(UINT16 )
{
	return DrvInputs[3];
}

static UINT8 pia2_read_b(UINT16 )
{
	return DrvDips[0];
}

static const pia6821_interface pia_0 = {
	NULL, pia0_read_b, // in_a, in_b
	NULL, NULL, NULL, NULL, // in_ca1, in_cb1, in_ca2, in_cb2
	pia0_out_a, NULL, NULL, NULL,	// out_a, out_b, out_ca2, out_cb2
	pia0_irq, pia0_irq	// irq_a, irq_b
};

static const pia6821_interface pia_1 = {
	pia1_read_a, pia1_read_b, // in_a, in_b
	NULL, NULL, NULL, NULL, // in_ca1, in_cb1, in_ca2, in_cb2
	pia1_out_a, NULL, pia1_out_ca2, NULL,	// out_a, out_b, out_ca2, out_cb2
	NULL, NULL	// irq_a, irq_b
};

static const pia6821_interface pia_2 = {
	pia2_read_a, pia2_read_b, // in_a, in_b
	NULL, NULL, NULL, NULL, // in_ca1, in_cb1, in_ca2, in_cb2
	NULL, NULL, NULL, NULL,	// out_a, out_b, out_ca2, out_cb2
	NULL, NULL	// irq_a, irq_b
};

static tilemap_callback( bg )
{
	UINT16 attr = *((UINT16*)(DrvVidRAM + offs * 2));

	TILE_SET_INFO(0, attr, attr >> 13, TILE_FLIPYX(attr >> 11));
}

static tilemap_callback( fg )
{
	UINT16 attr = *((UINT16*)(DrvVidRAM + offs * 2));
	INT32 color = (attr >> 13) & 7;

	TILE_SET_INFO(1, attr, color, TILE_FLIPYX(attr >> 11) | TILE_GROUP(color ? 1 : 0));
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	sound_data = 0;

	SekOpen(0);
	SekReset();
	SekClose();

	pia_reset();

	csd_reset();

	memset(nCyclesExtra, 0, sizeof(nCyclesExtra));

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x040000;
	DrvSndROM		= Next; Next += 0x008000;

	DrvGfxROM0		= Next; Next += 0x040000;
	DrvGfxROM1		= Next; Next += 0x040000;
	DrvGfxROM2		= Next; Next += 0x040000; // sprites
	DrvGfxROM3		= Next; Next += 0x008000; // color maps

	DrvPalette		= (UINT32*)Next; Next += 0x1000 * sizeof(UINT32);

	AllRam			= Next;

	Drv68KRAMA		= Next; Next += 0x001000;
	Drv68KRAMB		= Next; Next += 0x001000;
	DrvVidRAM		= Next; Next += 0x001000;
	DrvPalRAM		= Next; Next += 0x002000;
	DrvSprRAM		= Next; Next += 0x001000;
	DrvSndRAM		= Next; Next += 0x001000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[8]  = { STEP8(0,0) };
	INT32 XOffs0[16] = { STEP4(8*0+3,-1), STEP4(8*1+3,-1), STEP4(8*2+3,-1), STEP4(8*3+3,-1) };
	INT32 YOffs0[16] = { 4, 0x4000*8+4, 0, 0x4000*8+0, 36, 0x4000*8+36, 32, 0x4000*8+32, 68, 0x4000*8+68, 64, 0x4000*8+64, 100, 0x4000*8+100, 96, 0x4000*8+96 };

	INT32 L = (0x20000/4)*8;
	INT32 Plane1[4]  = { STEP4(0,1) };
	INT32 XOffs1[32] = {
		L*0+0, L*0+4, L*1+0, L*1+4, L*2+0, L*2+4, L*3+0, L*3+4,
		L*0+0+8, L*0+4+8, L*1+0+8, L*1+4+8, L*2+0+8, L*2+4+8, L*3+0+8, L*3+4+8,
		L*0+0+16, L*0+4+16, L*1+0+16, L*1+4+16, L*2+0+16, L*2+4+16, L*3+0+16, L*3+4+16,
		L*0+0+24, L*0+4+24, L*1+0+24, L*1+4+24, L*2+0+24, L*2+4+24, L*3+0+24, L*3+4+24
	};
	INT32 YOffs1[32] = { STEP32(0,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x20000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x08000);

	GfxDecode((0x08000 * 8) / (16 * 16), 8, 16, 16, Plane0, XOffs0, YOffs0, 0x080, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM2, 0x20000);

	GfxDecode((0x20000 * 2) / (32 * 32), 4, 32, 32, Plane1, XOffs1, YOffs1, 0x400, tmp, DrvGfxROM2);

	BurnFree(tmp);

	return 0;
}

static void DrvGfxColorMix() // after decode
{
	UINT8 *gfxdata0 = DrvGfxROM0;
	UINT8 *gfxdata1 = DrvGfxROM1;

	for (INT32 code = 0; code < 0x400; code++)
	{
		const UINT8 *coldata = DrvGfxROM3 + (code * 32);

		for (INT32 y = 0; y < 16; y++)
		{
			for (INT32 x = 0; x < 16; x++, gfxdata0++, gfxdata1++)
			{
				INT32 coloffs = ((y & 0x0c) | ((x >> 2) & 0x03)) * 2;
				INT32 pen0 = coldata[coloffs + 0];
				INT32 pen1 = coldata[coloffs + 1];

				INT32 tp0 = (pen0 & 0x80) ? pen0 : 0;
				INT32 tp1 = (pen1 & 0x80) ? pen1 : 0;

				*gfxdata1 = *gfxdata0 ? tp0 : tp1;
				*gfxdata0 = *gfxdata0 ? pen0 : pen1;
			}
		}
	}
}

static void zwackery_irq_cb(INT32 state)
{
	SekSetIRQLine(6, (state) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 DrvInit()
{
	BurnSetRefreshRate(30.00);

	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM  + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x008001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x008000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x010001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x010000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x018001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x018000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x020001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x020000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x028001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x028000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x030001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x030000, k++, 2)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x004001, k++, 2)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x004000, k++, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x004000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x004000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x008000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x00c000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x010000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x014000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x018000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x01c000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x000001, k++, 2)) return 1;

		DrvGfxDecode();
		DrvGfxColorMix();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x037fff, MAP_ROM);
	SekMapMemory(Drv68KRAMA,	0x080000, 0x080fff, MAP_RAM);
	SekMapMemory(Drv68KRAMB,	0x084000, 0x084fff, MAP_RAM);
	SekMapMemory(DrvVidRAM,		0x800000, 0x800fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		0x802000, 0x803fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,		0xc00000, 0xc00fff, MAP_ROM);
	SekSetWriteWordHandler(0,	zwackery_main_write_word);
	SekSetWriteByteHandler(0,	zwackery_main_write_byte);
	SekSetReadWordHandler(0,	zwackery_main_read_word);
	SekSetReadByteHandler(0,	zwackery_main_read_byte);
	SekClose();

	// pias
	pia_init();
	pia_config(0, 0, &pia_0);
	pia_config(1, 0, &pia_1);
	pia_config(2, 0, &pia_2);

	// sound
	csd_init(1, 3, DrvSndROM, DrvSndRAM); // initializes sek #1, pia #3

	// ptm
	ptm6840_init(7652400 / 10);
	ptm6840_set_irqcb(zwackery_irq_cb);

	// dial
	BurnTrackballInit(1);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, fg_map_callback, 16, 16, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 8, 16, 16, 0x40000, 0, 7);
	GenericTilemapSetGfx(1, DrvGfxROM1, 8, 16, 16, 0x40000, 0, 7);
	GenericTilemapSetTransparent(1, 0);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	SekExit();

	csd_exit();

	pia_exit();

	BurnTrackballExit();

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteUpdate()
{
	UINT16 *p = (UINT16*)DrvPalRAM;
	for (INT32 i = 0; i < 0x2000/2; i++)
	{
		UINT16 d = p[i] ^ 0xffff;

		UINT8 r = ((d >> 10) & 0x1f);
		UINT8 g = ((d >>  0) & 0x1f);
		UINT8 b = ((d >>  5) & 0x1f);

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_sprites(INT32 priority)
{
	UINT16 *spriteram = (UINT16*)DrvSprRAM;

	memset (pPrioDraw, 1, nScreenWidth * nScreenHeight);

	for (INT32 offs = 0x1000 / 2 - 4; offs >= 0; offs -= 4)
	{
		INT32 code = spriteram[offs + 2] & 0xff;
		if (code == 0) continue;

		INT32 flags = spriteram[offs + 1] & 0xff;
		INT32 color = ((~flags >> 2) & 0x0f) | ((flags & 0x02) << 3);

		if (priority == 0 && color == 7) continue;
		else if (priority == 1 && color != 7) continue;

		INT32 flipx = ~flags & 0x40;
		INT32 flipy = flags & 0x80;
		INT32 sx = (231 - (spriteram[offs + 3] & 0xff)) * 2;
		INT32 sy = (241 - (spriteram[offs] & 0xff)) * 2;

		if (sx <= -32) sx += 512;

		RenderPrioTransmaskSprite(pTransDraw, DrvGfxROM2, code, (color * 16) | 0x800, 0x0101, sx, sy, flipx, flipy, 32, 32, 0);
		RenderPrioTransmaskSprite(pTransDraw, DrvGfxROM2, code, (color * 16) | 0x800, 0xfeff, sx, sy, flipx, flipy, 32, 32, 0);
	}
}

static INT32 DrvDraw()
{
	DrvPaletteUpdate();

	BurnTransferClear();

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);

	if (nSpriteEnable & 1) draw_sprites(0);

	if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, TMAP_SET_GROUP(1));

	if (nSpriteEnable & 2) draw_sprites(1);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	SekNewFrame();
	ptm6840NewFrame();

	if (DrvReset) {
		DrvDoReset();
	}

	{
		Diag.Toggle(DrvJoy1[4]);

		DrvInputs[0] = 0xff;
		DrvInputs[1] = 0xff;
		DrvInputs[2] = 0xff;
		DrvInputs[3] = 0xff;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		BurnTrackballConfig(0, AXIS_REVERSED, AXIS_REVERSED);
		BurnTrackballFrame(0, Analog[0] * 2, 0, 0x01, 0x7f);
		BurnTrackballUpdate(0);
	}

	INT32 nInterleave = 512;
	INT32 nCyclesTotal[2] = { 7652400 / 30, 8000000 / 30 };
	INT32 nCyclesDone[2] = { 0, 0 };

	SekIdle(0, nCyclesExtra[0]); // when CPU_RUN_SYNCINT is used, we have to Idle extra cycles. (overflow from previous frame)
	ptm6840Idle(nCyclesExtra[0]);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		SekOpen(0);

		if (i == 493) { // vsync
			ptm6840_set_c1(0);
			ptm6840_set_c1(1);
			pia_set_input_ca1(0, 1);
		}
		if (i == 494) pia_set_input_ca1(0, 0);
		// hsync
		ptm6840_set_c3(0);
		ptm6840_set_c3(1);

		CPU_RUN_SYNCINT(0, Sek);
		sync_ptm();
		SekClose();

		SekOpen(1);
		CPU_RUN_SYNCINT(1, Sek);
		SekClose();
	}

	nCyclesExtra[0] = SekTotalCycles(0) - nCyclesTotal[0];

	if (pBurnSoundOut) {
		BurnSoundClear();
        DACUpdate(pBurnSoundOut, nBurnSoundLen);
        BurnSoundDCFilter();
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
		SekScan(nAction);
		pia_scan(nAction, pnMin);
		ptm6840_scan(nAction);

		csd_scan(nAction, pnMin);

		BurnTrackballScan();

		BurnWatchdogScan(nAction);

		Diag.Scan();

		SCAN_VAR(sound_data);

		SCAN_VAR(nCyclesExtra);
	}

	return 0;
}


// Zwackery

static struct BurnRomInfo zwackeryRomDesc[] = {
	{ "385a-42aae-jxrd.a6",		0x4000, 0x6fb9731c, 1 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "385a-42aae-kxrd.b6",		0x4000, 0x84b92555, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "385a-42aae-lxrd.a7",		0x4000, 0xe6977a2a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "385a-42aae-mxrd.b7",		0x4000, 0xf5d0a60e, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "385a-42aae-nxrd.a8",		0x4000, 0xec5841d9, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "385a-42aae-pxrd.b8",		0x4000, 0xd7d99ce0, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "385a-42aae-rxrd.a9",		0x4000, 0xb9fe7bf5, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "385a-42aae-txrd.b9",		0x4000, 0x5e261b3b, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "385a-42aae-uxrd.a10",	0x4000, 0x55e380a5, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "385a-42aae-vxrd.b10",	0x4000, 0x12249dca, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "385a-42aae-wxrd.a11",	0x4000, 0x6a39a8ca, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "385a-42aae-yxrd.b11",	0x4000, 0xad6b45bc, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "385a-42aae-zxrd.a12",	0x4000, 0xe2d25e1f, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "385a-42aae-1xrd.b12",	0x4000, 0xe131f9b8, 1 | BRF_PRG | BRF_ESS }, // 13

	{ "385a-22aae-aamd.u7",		0x2000, 0x5501f54b, 2 | BRF_PRG | BRF_ESS }, // 14 Cheap Squeak Deluxe 68K Code
	{ "385a-22aae-bamd.u17",	0x2000, 0x2e482580, 2 | BRF_PRG | BRF_ESS }, // 15
	{ "385a-22aae-camd.u8",		0x2000, 0x13366575, 2 | BRF_PRG | BRF_ESS }, // 16
	{ "385a-22aae-damd.u18",	0x2000, 0xbcfe5820, 2 | BRF_PRG | BRF_ESS }, // 17

	{ "385a-42aae-2xrd.1h",		0x4000, 0xa7237eb1, 3 | BRF_GRA },           // 18 Background/Foreground Tiles
	{ "385a-42aae-3xrd.1g",		0x4000, 0x626cc69b, 3 | BRF_GRA },           // 19

	{ "385a-42aae-axrd.6h",		0x4000, 0xa51158dc, 4 | BRF_GRA },           // 20 Sprites
	{ "385a-42aae-exrd.7h",		0x4000, 0x941feecf, 4 | BRF_GRA },           // 21
	{ "385a-42aae-bxrd.6j",		0x4000, 0xf3eef316, 4 | BRF_GRA },           // 22
	{ "385a-42aae-fxrd.7j",		0x4000, 0xa8a34033, 4 | BRF_GRA },           // 23
	{ "385a-42aae-cxrd.10h",	0x4000, 0xa99daea6, 4 | BRF_GRA },           // 24
	{ "385a-42aae-gxrd.11h",	0x4000, 0xc1a767fb, 4 | BRF_GRA },           // 25
	{ "385a-42aae-dxrd.10j",	0x4000, 0x4dd04376, 4 | BRF_GRA },           // 26
	{ "385a-42aae-hxrd.11j",	0x4000, 0xe8c6a880, 4 | BRF_GRA },           // 27

	{ "385a-42aae-5xrd.1f",		0x4000, 0xa0dfcd7e, 5 | BRF_GRA },           // 28 Background Color Look-up Tables
	{ "385a-42aae-4xrd.1e",		0x4000, 0xab504dc8, 5 | BRF_GRA },           // 29

	{ "pal.d5",					0x0001, 0x00000000, 6 | BRF_NODUMP | BRF_OPT },           // 30 PLDs
	{ "pal.d2",					0x0001, 0x00000000, 6 | BRF_NODUMP | BRF_OPT },           // 31
	{ "pal.d4",					0x0001, 0x00000000, 6 | BRF_NODUMP | BRF_OPT },           // 32
	{ "pal.d3",					0x0001, 0x00000000, 6 | BRF_NODUMP | BRF_OPT },           // 33
	{ "pal.e6",					0x0001, 0x00000000, 6 | BRF_NODUMP | BRF_OPT },           // 34
	{ "pal.f8",					0x0001, 0x00000000, 6 | BRF_NODUMP | BRF_OPT },           // 35
	{ "pal.a5",					0x0001, 0x00000000, 6 | BRF_NODUMP | BRF_OPT },           // 36
	{ "pal.1f",					0x0001, 0x00000000, 6 | BRF_NODUMP | BRF_OPT },           // 37
	{ "pal.1d",					0x0001, 0x00000000, 6 | BRF_NODUMP | BRF_OPT },           // 38
	{ "pal.1c",					0x0001, 0x00000000, 6 | BRF_NODUMP | BRF_OPT },           // 39
	{ "pal.5c",					0x0001, 0x00000000, 6 | BRF_NODUMP | BRF_OPT },           // 40
	{ "pal.5j",					0x0001, 0x00000000, 6 | BRF_NODUMP | BRF_OPT },           // 41
};

STD_ROM_PICK(zwackery)
STD_ROM_FN(zwackery)

struct BurnDriver BurnDrvZwackery = {
	"zwackery", NULL, NULL, NULL, "1984",
	"Zwackery\0", NULL, "Bally Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, zwackeryRomInfo, zwackeryRomName, NULL, NULL, NULL, NULL, ZwackeryInputInfo, ZwackeryDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	512, 480, 4, 3
};
