// FB Alpha Act-Fancer & Trio The Punch driver module
// Based on MAME driver by Bryan McPhail

#include "tiles_generic.h"
#include "m6502_intf.h"
#include "h6280_intf.h"
#include "burn_ym2203.h"
#include "burn_ym3812.h"
#include "msm6295.h"
#include "decobac06.h"

static UINT8 *AllMem		= NULL;
static UINT8 *AllRam		= NULL;
static UINT8 *MemEnd		= NULL;
static UINT8 *RamEnd		= NULL;
static UINT8 *Drv6280ROM	= NULL;
static UINT8 *Drv6502ROM	= NULL;
static UINT8 *DrvGfxROM0	= NULL;
static UINT8 *DrvGfxROM1	= NULL;
static UINT8 *DrvGfxROM2	= NULL;
static UINT8 *DrvPf1RAM		= NULL;
static UINT8 *DrvPf2RAM		= NULL;
static UINT8 *DrvPf1Scr		= NULL;
static UINT8 *DrvPf2Scr		= NULL;
static UINT8 *DrvSprRAM		= NULL;
static UINT8 *DrvPalRAM		= NULL;
static UINT8 *Drv6280RAM	= NULL;
static UINT8 *Drv6502RAM	= NULL;
static UINT8 *DrvSprBuf		= NULL;
static UINT8 *DrvPfCtrl[2]	= { NULL, NULL };

static UINT8 *soundlatch	= NULL;
static UINT8 *flipscreen	= NULL;

static UINT32  *DrvPalette	= NULL;
static UINT8 DrvRecalc;

static UINT8 control_select;

static INT32 vblank;
static UINT16 gfx_config[4];

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo ActfancrInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Actfancr)

static struct BurnInputInfo TriothepInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Triothep)

static struct BurnDIPInfo ActfancrDIPList[]=
{
	{0x12, 0xff, 0xff, 0x7f, NULL			},
	{0x13, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x12, 0x01, 0x03, 0x00, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x03, 0x01, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x12, 0x01, 0x0c, 0x00, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x0c, 0x04, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x20, 0x00, "Off"			},
	{0x12, 0x01, 0x20, 0x20, "On"			},

#if 0
	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x12, 0x01, 0x40, 0x40, "Off"			},
	{0x12, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x12, 0x01, 0x80, 0x00, "Upright"		},
	{0x12, 0x01, 0x80, 0x80, "Cocktail"		},
#endif

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0x03, 0x03, "3"			},
	{0x13, 0x01, 0x03, 0x02, "4"			},
	{0x13, 0x01, 0x03, 0x01, "5"			},
	{0x13, 0x01, 0x03, 0x00, "100 (Cheat)"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x0c, 0x04, "Easy"			},
	{0x13, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x13, 0x01, 0x0c, 0x08, "Hard"			},
	{0x13, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x13, 0x01, 0x20, 0x20, "800000"		},
	{0x13, 0x01, 0x20, 0x00, "None"			},
};

STDDIPINFO(Actfancr)

static struct BurnDIPInfo TriothepDIPList[]=
{
	{0x14, 0xff, 0xff, 0x7f, NULL			},
	{0x15, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x14, 0x01, 0x03, 0x00, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0x03, 0x01, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x14, 0x01, 0x0c, 0x00, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0x0c, 0x04, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x14, 0x01, 0x20, 0x00, "Off"			},
	{0x14, 0x01, 0x20, 0x20, "On"			},

#if 0
	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x14, 0x01, 0x40, 0x40, "Off"			},
	{0x14, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x14, 0x01, 0x80, 0x00, "Upright"		},
	{0x14, 0x01, 0x80, 0x80, "Cocktail"		},
#endif

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x15, 0x01, 0x03, 0x00, "8"			},
	{0x15, 0x01, 0x03, 0x01, "10"			},
	{0x15, 0x01, 0x03, 0x03, "12"			},
	{0x15, 0x01, 0x03, 0x02, "14"			},

	{0   , 0xfe, 0   ,    4, "Difficulty (Time)"	},
	{0x15, 0x01, 0x0c, 0x08, "Easy (130)"		},
	{0x15, 0x01, 0x0c, 0x0c, "Normal (100)"		},
	{0x15, 0x01, 0x0c, 0x04, "Hard (70)"		},
	{0x15, 0x01, 0x0c, 0x00, "Hardest (60)"		},

	{0   , 0xfe, 0   ,    2, "Bonus Lives"		},
	{0x15, 0x01, 0x10, 0x00, "2"			},
	{0x15, 0x01, 0x10, 0x10, "3"			},
};

STDDIPINFO(Triothep)

static inline void palette_update(INT32 offset)
{
	offset &= 0x7fe;

	INT32 p = (DrvPalRAM[offset+0] << 0) | (DrvPalRAM[offset+1] << 8);

	INT32 r = (p >> 0) & 0x0f;
	INT32 g = (p >> 4) & 0x0f;
	INT32 b = (p >> 8) & 0x0f;

	DrvPalette[offset/2] = BurnHighCol((r << 4) | r, (g << 4) | g, (b << 4) | b, 0);
}

static void actfan_main_write(UINT32 address, UINT8 data)
{
	if ((address & 0xffffe0) == 0x060000) {
		
		DrvPfCtrl[0][address & 0x1f] = data;
		return;
	}

	if ((address & 0xffffe0) == 0x070000) {
		DrvPfCtrl[1][address & 0x1f] = data;
		return;
	}

	if ((address & 0xfff800) == 0x120000) {
		DrvPalRAM[address & 0x7ff] = data;
		palette_update(address & 0x7fe);
		return;
	}

	switch (address)
	{
		case 0x110000:
			memcpy (DrvSprBuf, DrvSprRAM, 0x800);
		return;

		case 0x150000:
			*soundlatch = data;
			M6502SetIRQLine(M6502_INPUT_LINE_NMI, CPU_IRQSTATUS_AUTO);
		return;

		case 0x160000:
		return; // ?
	}
}

static UINT8 actfan_main_read(UINT32 address)
{
	switch (address)
	{
		case 0x130000:
		case 0x130001:
			return DrvInputs[(address & 1)];

		case 0x130002:
		case 0x130003:
			return DrvDips[(address & 1)];

		case 0x140000:
		case 0x140001:
			return (DrvInputs[2] & 0x7f) | vblank;
	}

	return 0;
}

static void triothep_main_write(UINT32 address, UINT8 data)
{
	if ((address & 0xffffe0) == 0x060000) {
		DrvPfCtrl[0][address & 0x1f] = data;
		return;
	}

	if ((address & 0xffffe0) == 0x040000) {
		DrvPfCtrl[1][address & 0x1f] = data;
		return;
	}

	if ((address & 0xfff800) == 0x130000) {
		DrvPalRAM[address & 0x7ff] = data;
		palette_update(address & 0x7fe);
		return;
	}

	switch (address)
	{
		case 0x100000:
			*soundlatch = data;
			M6502SetIRQLine(M6502_INPUT_LINE_NMI, CPU_IRQSTATUS_AUTO);
		return;

		case 0x110000:
			memcpy (DrvSprBuf, DrvSprRAM, 0x800);
		return;

		case 0x1ff000:
		case 0x1ff001:
			control_select = data;
		return;

		case 0x1ff400:
		case 0x1ff401:
		case 0x1ff402:
		case 0x1ff403:
			h6280_irq_status_w(address & 3, data);
		return;

		case 0x064800: // overflow from rowscroll write?
		return;

		case 0x120800: // overflow from spriteram write?
		return;
	}
}

static UINT8 triothep_main_read(UINT32 address)
{
	switch (address)
	{
		case 0x140000:
			return 0; // nop

		case 0x1ff000:
		case 0x1ff001:
		{
			switch (control_select)
			{
				case 0: return DrvInputs[0];
				case 1: return DrvInputs[1];
				case 2: return DrvDips[0];
				case 3: return DrvDips[1];
				case 4: return (DrvInputs[2] & 0x7f) | vblank;
				default: return 0xff;
			}
		}
		return 0xff;
	}

	return 0;
}

static void Dec0_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x0800:
		case 0x0801:
			BurnYM2203Write(0, address & 1, data);
		return;

		case 0x1000:
		case 0x1001:
			BurnYM3812Write(0, address & 1, data);
		return;

		case 0x3800:
			MSM6295Write(0, data);
		return;
	}
}

static UINT8 Dec0_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x3000:
			M6502SetIRQLine(M6502_INPUT_LINE_NMI, CPU_IRQSTATUS_NONE);
			return *soundlatch;

		case 0x3800:
			return MSM6295Read(0);
	}

	return 0;
}

static void Dec0YM3812IRQHandler(INT32, INT32 nStatus)
{
	if (nStatus) {
		M6502SetIRQLine(M6502_IRQ_LINE, CPU_IRQSTATUS_ACK);
	} else {
		M6502SetIRQLine(M6502_IRQ_LINE, CPU_IRQSTATUS_NONE);
	}
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	h6280Open(0);
	h6280Reset();
	h6280Close();

	M6502Open(0);
	M6502Reset();
	M6502Close();

	MSM6295Reset(0);
	BurnYM2203Reset();
	BurnYM3812Reset();

	control_select = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv6280ROM	= Next; Next += 0x040000;
	Drv6502ROM	= Next; Next += 0x010000;

	DrvGfxROM0	= Next; Next += 0x040000;
	DrvGfxROM1	= Next; Next += 0x100000;
	DrvGfxROM2	= Next; Next += 0x080000;

	MSM6295ROM	= Next; Next += 0x040000;

	DrvPalette	= (UINT32*)Next; Next += 0x0400 * sizeof(INT32);

	AllRam		= Next;

	Drv6280RAM	= Next; Next += 0x004000;
	Drv6502RAM	= Next; Next += 0x000800;

	DrvSprRAM	= Next; Next += 0x000800;
	DrvPalRAM	= Next; Next += 0x000800;

	DrvSprBuf	= Next; Next += 0x000800;

	soundlatch 	= Next; Next += 0x000001;
	flipscreen	= Next; Next += 0x000001;

	DrvPf1RAM	= Next; Next += 0x002000;
	DrvPf2RAM	= Next; Next += 0x002000;

	DrvPf1Scr	= Next; Next += 0x000800;
	DrvPf2Scr	= Next; Next += 0x000800;

	DrvPfCtrl[0]	= Next; Next += 0x000020;
	DrvPfCtrl[1]	= Next; Next += 0x000020;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[4] = { 0x08000*8, 0x18000*8, 0x00000*8, 0x10000*8 };
	INT32 Plane1[4] = { 0x00000*8, 0x18000*8, 0x30000*8, 0x48000*8 };
	INT32 Plane2[4] = { 0x00000*8, 0x10000*8, 0x20000*8, 0x30000*8 };
	INT32 XOffs[16] = { STEP8(16*8, 1), STEP8(0, 1) };
	INT32 YOffs[16] = { STEP16(0, 8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x60000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x20000);

	GfxDecode(0x1000, 4,  8,  8, Plane0, XOffs + 8, YOffs, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x60000);

	GfxDecode(0x0c00, 4, 16, 16, Plane1, XOffs + 0, YOffs, 0x100, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x40000);

	GfxDecode(0x0800, 4, 16, 16, Plane2, XOffs + 0, YOffs, 0x100, tmp, DrvGfxROM2);

	BurnFree (tmp);

	return 0;
}

static void Dec0SoundInit()
{
	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(Drv6502RAM,		0x0000, 0x07ff, MAP_RAM);
	M6502MapMemory(Drv6502ROM + 0x4000,	0x4000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(Dec0_sound_write);
	M6502SetReadHandler(Dec0_sound_read);
	M6502Close();
	
	BurnYM2203Init(1, 1500000, NULL, 0);
	BurnTimerAttachH6280(7159066);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE, 0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.90, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.90, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.90, BURN_SND_ROUTE_BOTH);

	BurnYM3812Init(1, 3000000, &Dec0YM3812IRQHandler, 1);
	BurnTimerAttachYM3812(&M6502Config, 1500000);
	BurnYM3812SetRoute(0, BURN_SND_YM3812_ROUTE, 0.90, BURN_SND_ROUTE_BOTH);

	MSM6295Init(0, 1024188 / 132, 1);
	MSM6295SetRoute(0, 0.85, BURN_SND_ROUTE_BOTH);
}

static INT32 ActfanInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv6280ROM + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(Drv6280ROM + 0x10000,  1, 1)) return 1;
		if (BurnLoadRom(Drv6280ROM + 0x20000,  2, 1)) return 1;

		if (BurnLoadRom(Drv6502ROM + 0x08000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x10000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x10000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x18000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x28000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x30000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x40000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x48000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x58000, 13, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000, 14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x10000, 15, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x20000, 16, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x30000, 17, 1)) return 1;

		if (BurnLoadRom(MSM6295ROM + 0x00000, 18, 1)) return 1;

		if (DrvGfxDecode()) return 1;
	}

	h6280Init(0);
	h6280Open(0);
	h6280MapMemory(Drv6280ROM,		0x000000, 0x03ffff, MAP_ROM);
	h6280MapMemory(DrvPf1RAM,		0x062000, 0x063fff, MAP_RAM);
	h6280MapMemory(DrvPf2RAM,		0x072000, 0x073fff, MAP_RAM);
	h6280MapMemory(DrvSprRAM,		0x100000, 0x1007ff, MAP_RAM);
	h6280MapMemory(DrvPalRAM,		0x120000, 0x1205ff, MAP_ROM);
	h6280MapMemory(Drv6280RAM,		0x1f0000, 0x1f3fff, MAP_RAM);
	h6280SetWriteHandler(actfan_main_write);
	h6280SetReadHandler(actfan_main_read);
	h6280Close();

	Dec0SoundInit();

	gfx_config[0] = 0; // character color offset
	gfx_config[1] = 0x200; // sprite color offset
	gfx_config[2] = 0x100; // bg color offset
	gfx_config[3] = 2; // wide 2

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 TriothepInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv6280ROM + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(Drv6280ROM + 0x20000,  1, 1)) return 1;
		if (BurnLoadRom(Drv6280ROM + 0x30000,  2, 1)) return 1;

		if (BurnLoadRom(Drv6502ROM + 0x00000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x10000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x10000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x18000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x28000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x30000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x40000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x48000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x58000, 13, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000, 14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x10000, 15, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x20000, 16, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x30000, 17, 1)) return 1;

		if (BurnLoadRom(MSM6295ROM + 0x00000, 18, 1)) return 1;

		if (DrvGfxDecode()) return 1;
	}

	h6280Init(0);
	h6280Open(0);
	h6280MapMemory(Drv6280ROM,		0x000000, 0x03ffff, MAP_ROM);
	h6280MapMemory(DrvPf2RAM,		0x044000, 0x045fff, MAP_RAM);
	h6280MapMemory(DrvPf2Scr,		0x046000, 0x0467ff, MAP_RAM);
	h6280MapMemory(DrvPf1RAM,		0x064000, 0x0647ff, MAP_RAM);
	h6280MapMemory(DrvPf1Scr,		0x066000, 0x0667ff, MAP_RAM);
	h6280MapMemory(DrvSprRAM,		0x120000, 0x1207ff, MAP_RAM);
	h6280MapMemory(DrvPalRAM,		0x130000, 0x1305ff, MAP_ROM);
	h6280MapMemory(Drv6280RAM,		0x1f0000, 0x1f3fff, MAP_RAM);
	h6280SetWriteHandler(triothep_main_write);
	h6280SetReadHandler(triothep_main_read);
	h6280Close();

	Dec0SoundInit();

	gfx_config[0] = 0; // character color offset
	gfx_config[1] = 0x100; // sprite color offset
	gfx_config[2] = 0x200; // bg color offset
	gfx_config[3] = 0; // wide 0

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	BurnYM2203Exit();
	BurnYM3812Exit();
	MSM6295Exit(0);

	h6280Exit();
	M6502Exit();

	BurnFree (AllMem);

	return 0;
}

static void draw_sprites()
{
	INT32 offs = 0;
	while (offs < 0x800)
	{
		INT32 incy, mult;

		INT32 sy     = DrvSprBuf[offs + 0] + (DrvSprBuf[offs + 1] << 8);
		INT32 sx     = DrvSprBuf[offs + 4] + (DrvSprBuf[offs + 5] << 8);
		INT32 enable = sy & 0x8000;
		INT32 color  = sx >> 12;
		INT32 flash  = sx & 0x0800;

		INT32 flipx  = sy & 0x2000;
		INT32 flipy  = sy & 0x4000;
		INT32 h      = (1 << ((sy & 0x1800) >> 11));
		INT32 w      = (1 << ((sy & 0x0600) >>  9));

		sx = sx & 0x01ff;
		sy = sy & 0x01ff;
		if (sx >= 256) sx -= 512;
		if (sy >= 256) sy -= 512;
		sx = 240 - sx;
		sy = 240 - sy;

		if (0) //flipscreen
		{
			sy = 240 - sy;
			sx = 240 - sx;
			if (flipx) flipx = 0; else flipx = 1;
			if (flipy) flipy = 0; else flipy = 1;
			mult = 16;
		}
		else
			mult = -16;

		for (INT32 x = 0; x < w; x++)
		{
			if (enable && (!flash || (nCurrentFrame & 1)))
			{
				INT32 code = DrvSprBuf[offs + 2] + (DrvSprBuf[offs + 3] << 8);

				code &= ~(h-1);

				code %= 0xc00; //&= 0xfff; // right?

				if (flipy)
					incy = -1;
				else
				{
					code += h-1;
					incy = 1;
				}

				for (INT32 y = 0; y < h; y++)
				{
					if (flipy) {
						if (flipx) {
							Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code - y * incy, sx + (mult * x), sy + (mult * y) - 8, color, 4, 0, gfx_config[1], DrvGfxROM1);
						} else {
							Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code - y * incy, sx + (mult * x), sy + (mult * y) - 8, color, 4, 0, gfx_config[1], DrvGfxROM1);
						}
					} else {
						if (flipx) {
							Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code - y * incy, sx + (mult * x), sy + (mult * y) - 8, color, 4, 0, gfx_config[1], DrvGfxROM1);
						} else {
							Render16x16Tile_Mask_Clip(pTransDraw, code - y * incy, sx + (mult * x), sy + (mult * y) - 8, color, 4, 0, gfx_config[1], DrvGfxROM1);
						}
					}
				}
			}

			offs += 8;
			if (offs >= 0x800) return;
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x600; i+=2) {
			palette_update(i);
		}
		DrvRecalc = 0;
	}

	UINT16 control[2][2][4];
	UINT16 *pf_control0 = (UINT16*)DrvPfCtrl[0];
	UINT16 *pf_control1 = (UINT16*)DrvPfCtrl[1];

	control[0][0][0] = pf_control0[0];
	control[0][0][2] = pf_control0[2];
	control[0][0][3] = pf_control0[3];
	control[0][1][0] = pf_control0[8];
	control[0][1][1] = pf_control0[9];
	control[1][0][0] = pf_control1[0];
	control[1][0][2] = pf_control1[2];
	control[1][0][3] = pf_control1[3];
	control[1][1][0] = pf_control1[8];
	control[1][1][1] = pf_control1[9];

	bac06_depth = 4;
	bac06_yadjust = 8;

	if ((nBurnLayer & 1) == 0) {
		BurnTransferClear();
	} else {
		bac06_draw_layer(DrvPf1RAM, control[0], DrvPf1Scr, NULL, DrvGfxROM2, gfx_config[2], 0x7ff, DrvGfxROM2, gfx_config[2], 0x7ff, 2, 1);
	}

	if (nBurnLayer & 2) draw_sprites();

	if (nBurnLayer & 4) {
		bac06_draw_layer(DrvPf2RAM, control[1], NULL,      NULL, DrvGfxROM0, gfx_config[0], 0xfff, DrvGfxROM0, gfx_config[0], 0xfff, 0, 0);
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	h6280NewFrame();
	M6502NewFrame();

	{
		memset (DrvInputs, 0xff, 3);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		// clear opposites
		if ((DrvInputs[0] & 0x03) == 0) DrvInputs[0] |= 0x03;
		if ((DrvInputs[0] & 0x0c) == 0) DrvInputs[0] |= 0x0c;
		if ((DrvInputs[1] & 0x03) == 0) DrvInputs[1] |= 0x03;
		if ((DrvInputs[1] & 0x0c) == 0) DrvInputs[1] |= 0x0c;
	}

	INT32 nInterleave = 32;
	INT32 nCyclesTotal[2] = { 7159066 / 60, 1500000 / 60 };

	h6280Open(0);
	M6502Open(0);

	vblank = 0x80;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		BurnTimerUpdate(i * (nCyclesTotal[0] / nInterleave));	// h6280
		if (i == 1) vblank = 0;
		if (i == 30) {
			vblank = 0x80;
			h6280SetIRQLine(0, CPU_IRQSTATUS_AUTO);
		}

		BurnTimerUpdateYM3812(i * (nCyclesTotal[1] / nInterleave)); // m6502
	}

	BurnTimerEndFrame(nCyclesTotal[0]); // h6280
	BurnTimerEndFrameYM3812(nCyclesTotal[1]); // m6502

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
		BurnYM3812Update(pBurnSoundOut, nBurnSoundLen);
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
	}

	M6502Close();
	h6280Close();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029721;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		h6280Scan(nAction);
		M6502Scan(nAction);

		BurnYM2203Scan(nAction, pnMin);
		BurnYM3812Scan(nAction, pnMin);
		MSM6295Scan(nAction, pnMin);

		SCAN_VAR(control_select);
	}

	return 0;
}


// Act-Fancer Cybernetick Hyper Weapon (World revision 3)

static struct BurnRomInfo actfancrRomDesc[] = {
	{ "fe08-3.bin", 0x10000, 0x35f1999d, 1 | BRF_PRG | BRF_ESS }, //  0 h6280 Code
	{ "fe09-3.bin",	0x10000, 0xd21416ca, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "fe10-3.bin",	0x10000, 0x85535fcc, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "17-1",	0x08000, 0x289ad106, 2 | BRF_PRG | BRF_ESS }, //  3 m6502 Code

	{ "15",		0x10000, 0xa1baf21e, 3 | BRF_GRA },           //  4 Characters
	{ "16",		0x10000, 0x22e64730, 3 | BRF_GRA },           //  5

	{ "02",		0x10000, 0xb1db0efc, 4 | BRF_GRA },           //  6 Sprites
	{ "03",		0x08000, 0xf313e04f, 4 | BRF_GRA },           //  7
	{ "06",		0x10000, 0x8cb6dd87, 4 | BRF_GRA },           //  8
	{ "07",		0x08000, 0xdd345def, 4 | BRF_GRA },           //  9
	{ "00",		0x10000, 0xd50a9550, 4 | BRF_GRA },           // 10
	{ "01",		0x08000, 0x34935e93, 4 | BRF_GRA },           // 11
	{ "04",		0x10000, 0xbcf41795, 4 | BRF_GRA },           // 12
	{ "05",		0x08000, 0xd38b94aa, 4 | BRF_GRA },           // 13

	{ "14",		0x10000, 0xd6457420, 5 | BRF_GRA },           // 14 Background Layer
	{ "12",		0x10000, 0x08787b7a, 5 | BRF_GRA },           // 15
	{ "13",		0x10000, 0xc30c37dc, 5 | BRF_GRA },           // 16
	{ "11",		0x10000, 0x1f006d9f, 5 | BRF_GRA },           // 17

	{ "18",		0x10000, 0x5c55b242, 6 | BRF_SND },           // 18 Samples
};

STD_ROM_PICK(actfancr)
STD_ROM_FN(actfancr)

struct BurnDriver BurnDrvActfancr = {
	"actfancr", NULL, NULL, NULL, "1989",
	"Act-Fancer Cybernetick Hyper Weapon (World revision 3)\0", NULL, "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PREFIX_DATAEAST, GBF_HORSHOOT, 0,
	NULL, actfancrRomInfo, actfancrRomName, NULL, NULL, NULL, NULL, ActfancrInputInfo, ActfancrDIPInfo,
	ActfanInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	256, 240, 4, 3
};


// Act-Fancer Cybernetick Hyper Weapon (World revision 1)

static struct BurnRomInfo actfancr1RomDesc[] = {
	{ "08-1",	0x10000, 0x3bf214a4, 1 | BRF_PRG | BRF_ESS }, //  0 h6280 Code
	{ "09-1",	0x10000, 0x13ae78d5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "10",		0x10000, 0xcabad137, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "17-1",	0x08000, 0x289ad106, 2 | BRF_PRG | BRF_ESS }, //  3 m6502 Code

	{ "15",		0x10000, 0xa1baf21e, 3 | BRF_GRA },           //  4 Characters
	{ "16",		0x10000, 0x22e64730, 3 | BRF_GRA },           //  5

	{ "02",		0x10000, 0xb1db0efc, 4 | BRF_GRA },           //  6 Sprites
	{ "03",		0x08000, 0xf313e04f, 4 | BRF_GRA },           //  7
	{ "06",		0x10000, 0x8cb6dd87, 4 | BRF_GRA },           //  8
	{ "07",		0x08000, 0xdd345def, 4 | BRF_GRA },           //  9
	{ "00",		0x10000, 0xd50a9550, 4 | BRF_GRA },           // 10
	{ "01",		0x08000, 0x34935e93, 4 | BRF_GRA },           // 11
	{ "04",		0x10000, 0xbcf41795, 4 | BRF_GRA },           // 12
	{ "05",		0x08000, 0xd38b94aa, 4 | BRF_GRA },           // 13

	{ "14",		0x10000, 0xd6457420, 5 | BRF_GRA },           // 14 Background Layer
	{ "12",		0x10000, 0x08787b7a, 5 | BRF_GRA },           // 15
	{ "13",		0x10000, 0xc30c37dc, 5 | BRF_GRA },           // 16
	{ "11",		0x10000, 0x1f006d9f, 5 | BRF_GRA },           // 17

	{ "18",		0x10000, 0x5c55b242, 6 | BRF_SND },           // 18 Samples
};

STD_ROM_PICK(actfancr1)
STD_ROM_FN(actfancr1)

struct BurnDriver BurnDrvActfancr1 = {
	"actfancr1", "actfancr", NULL, NULL, "1989",
	"Act-Fancer Cybernetick Hyper Weapon (World revision 1)\0", NULL, "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_DATAEAST, GBF_HORSHOOT, 0,
	NULL, actfancr1RomInfo, actfancr1RomName, NULL, NULL, NULL, NULL, ActfancrInputInfo, ActfancrDIPInfo,
	ActfanInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	256, 240, 4, 3
};


// Act-Fancer Cybernetick Hyper Weapon (World revision 2)

static struct BurnRomInfo actfancr2RomDesc[] = {
	{ "fe08-2.bin",	0x10000, 0x0d36fbfa, 1 | BRF_PRG | BRF_ESS }, //  0 h6280 Code
	{ "fe09-2.bin",	0x10000, 0x27ce2bb1, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "10",		0x10000, 0xcabad137, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "17-1",	0x08000, 0x289ad106, 2 | BRF_PRG | BRF_ESS }, //  3 m6502 Code

	{ "15",		0x10000, 0xa1baf21e, 3 | BRF_GRA },           //  4 Characters
	{ "16",		0x10000, 0x22e64730, 3 | BRF_GRA },           //  5

	{ "02",		0x10000, 0xb1db0efc, 4 | BRF_GRA },           //  6 Sprites
	{ "03",		0x08000, 0xf313e04f, 4 | BRF_GRA },           //  7
	{ "06",		0x10000, 0x8cb6dd87, 4 | BRF_GRA },           //  8
	{ "07",		0x08000, 0xdd345def, 4 | BRF_GRA },           //  9
	{ "00",		0x10000, 0xd50a9550, 4 | BRF_GRA },           // 10
	{ "01",		0x08000, 0x34935e93, 4 | BRF_GRA },           // 11
	{ "04",		0x10000, 0xbcf41795, 4 | BRF_GRA },           // 12
	{ "05",		0x08000, 0xd38b94aa, 4 | BRF_GRA },           // 13

	{ "14",		0x10000, 0xd6457420, 5 | BRF_GRA },           // 14 Background Layer
	{ "12",		0x10000, 0x08787b7a, 5 | BRF_GRA },           // 15
	{ "13",		0x10000, 0xc30c37dc, 5 | BRF_GRA },           // 16
	{ "11",		0x10000, 0x1f006d9f, 5 | BRF_GRA },           // 17

	{ "18",		0x10000, 0x5c55b242, 6 | BRF_SND },           // 18 Samples
};

STD_ROM_PICK(actfancr2)
STD_ROM_FN(actfancr2)

struct BurnDriver BurnDrvActfancr2 = {
	"actfancr2", "actfancr", NULL, NULL, "1989",
	"Act-Fancer Cybernetick Hyper Weapon (World revision 2)\0", NULL, "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_DATAEAST, GBF_HORSHOOT, 0,
	NULL, actfancr2RomInfo, actfancr2RomName, NULL, NULL, NULL, NULL, ActfancrInputInfo, ActfancrDIPInfo,
	ActfanInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	256, 240, 4, 3
};


// Act-Fancer Cybernetick Hyper Weapon (Japan revision 1)

static struct BurnRomInfo actfancrjRomDesc[] = {
	{ "fd08-1.bin",	0x10000, 0x69004b60, 1 | BRF_PRG | BRF_ESS }, //  0 h6280 Code
	{ "fd09-1.bin",	0x10000, 0xa455ae3e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "10",		0x10000, 0xcabad137, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "17-1",	0x08000, 0x289ad106, 2 | BRF_PRG | BRF_ESS }, //  3 m6502 Code

	{ "15",		0x10000, 0xa1baf21e, 3 | BRF_GRA },           //  4 Characters
	{ "16",		0x10000, 0x22e64730, 3 | BRF_GRA },           //  5

	{ "02",		0x10000, 0xb1db0efc, 4 | BRF_GRA },           //  6 Sprites
	{ "03",		0x08000, 0xf313e04f, 4 | BRF_GRA },           //  7
	{ "06",		0x10000, 0x8cb6dd87, 4 | BRF_GRA },           //  8
	{ "07",		0x08000, 0xdd345def, 4 | BRF_GRA },           //  9
	{ "00",		0x10000, 0xd50a9550, 4 | BRF_GRA },           // 10
	{ "01",		0x08000, 0x34935e93, 4 | BRF_GRA },           // 11
	{ "04",		0x10000, 0xbcf41795, 4 | BRF_GRA },           // 12
	{ "05",		0x08000, 0xd38b94aa, 4 | BRF_GRA },           // 13

	{ "14",		0x10000, 0xd6457420, 5 | BRF_GRA },           // 14 Background Layer
	{ "12",		0x10000, 0x08787b7a, 5 | BRF_GRA },           // 15
	{ "13",		0x10000, 0xc30c37dc, 5 | BRF_GRA },           // 16
	{ "11",		0x10000, 0x1f006d9f, 5 | BRF_GRA },           // 17

	{ "18",		0x10000, 0x5c55b242, 6 | BRF_SND },           // 18 Samples
};

STD_ROM_PICK(actfancrj)
STD_ROM_FN(actfancrj)

struct BurnDriver BurnDrvActfancrj = {
	"actfancrj", "actfancr", NULL, NULL, "1989",
	"Act-Fancer Cybernetick Hyper Weapon (Japan revision 1)\0", NULL, "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_DATAEAST, GBF_HORSHOOT, 0,
	NULL, actfancrjRomInfo, actfancrjRomName, NULL, NULL, NULL, NULL, ActfancrInputInfo, ActfancrDIPInfo,
	ActfanInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	256, 240, 4, 3
};


// Trio The Punch - Never Forget Me... (World)

static struct BurnRomInfo triothepRomDesc[] = {
	{ "fg-16.bin",	0x20000, 0x7238355a, 1 | BRF_PRG | BRF_ESS }, //  0 h6280 Code
	{ "fg-15.bin",	0x10000, 0x1c0551ab, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "fg-14.bin",	0x10000, 0x4ba7de4a, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "fg-18.bin",	0x10000, 0x9de9ee63, 2 | BRF_PRG | BRF_ESS }, //  3 m6502 Code

	{ "fg-12.bin",	0x10000, 0x15fb49f2, 3 | BRF_GRA },           //  4 Characters
	{ "fg-13.bin",	0x10000, 0xe20c9623, 3 | BRF_GRA },           //  5

	{ "fg-11.bin",	0x10000, 0x1143ebd7, 4 | BRF_GRA },           //  6 Sprites
	{ "fg-10.bin",	0x08000, 0x4b6b477a, 4 | BRF_GRA },           //  7
	{ "fg-09.bin",	0x10000, 0x6bf6c803, 4 | BRF_GRA },           //  8
	{ "fg-08.bin",	0x08000, 0x1391e445, 4 | BRF_GRA },           //  9
	{ "fg-03.bin",	0x10000, 0x3d3ca9ad, 4 | BRF_GRA },           // 10
	{ "fg-02.bin",	0x08000, 0x6b9d24ce, 4 | BRF_GRA },           // 11
	{ "fg-01.bin",	0x10000, 0x4987f7ac, 4 | BRF_GRA },           // 12
	{ "fg-00.bin",	0x08000, 0x41232442, 4 | BRF_GRA },           // 13

	{ "fg-04.bin",	0x10000, 0x7cea3c87, 5 | BRF_GRA },           // 14 Background Layer
	{ "fg-06.bin",	0x10000, 0x5e7f3e8f, 5 | BRF_GRA },           // 15
	{ "fg-05.bin",	0x10000, 0x8bb13f05, 5 | BRF_GRA },           // 16
	{ "fg-07.bin",	0x10000, 0x0d7affc3, 5 | BRF_GRA },           // 17

	{ "fg-17.bin",	0x10000, 0xf0ab0d05, 6 | BRF_SND },           // 18 Samples
};

STD_ROM_PICK(triothep)
STD_ROM_FN(triothep)

struct BurnDriver BurnDrvTriothep = {
	"triothep", NULL, NULL, NULL, "1989",
	"Trio The Punch - Never Forget Me... (World)\0", NULL, "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PREFIX_DATAEAST, GBF_SCRFIGHT, 0,
	NULL, triothepRomInfo, triothepRomName, NULL, NULL, NULL, NULL, TriothepInputInfo, TriothepDIPInfo,
	TriothepInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	256, 240, 4, 3
};


// Trio The Punch - Never Forget Me... (Japan)

static struct BurnRomInfo triothepjRomDesc[] = {
	{ "ff-16.bin",	0x20000, 0x84d7e1b6, 1 | BRF_PRG | BRF_ESS }, //  0 h6280 Code
	{ "ff-15.bin",	0x10000, 0x6eada47c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ff-14.bin",	0x10000, 0x4ba7de4a, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "ff-18.bin",	0x10000, 0x9de9ee63, 2 | BRF_PRG | BRF_ESS }, //  3 m6502 Code

	{ "ff-12.bin",	0x10000, 0x15fb49f2, 3 | BRF_GRA },           //  4 Characters
	{ "ff-13.bin",	0x10000, 0xe20c9623, 3 | BRF_GRA },           //  5

	{ "ff-11.bin",	0x10000, 0x19e885c7, 4 | BRF_GRA },           //  6 Sprites
	{ "ff-10.bin",	0x08000, 0x4b6b477a, 4 | BRF_GRA },           //  7
	{ "ff-09.bin",	0x10000, 0x79c6bc0e, 4 | BRF_GRA },           //  8
	{ "ff-08.bin",	0x08000, 0x1391e445, 4 | BRF_GRA },           //  9
	{ "ff-03.bin",	0x10000, 0xb36ad42d, 4 | BRF_GRA },           // 10
	{ "ff-02.bin",	0x08000, 0x6b9d24ce, 4 | BRF_GRA },           // 11
	{ "ff-01.bin",	0x10000, 0x68d80a66, 4 | BRF_GRA },           // 12
	{ "ff-00.bin",	0x08000, 0x41232442, 4 | BRF_GRA },           // 13

	{ "ff-04.bin",	0x10000, 0x7cea3c87, 5 | BRF_GRA },           // 14 Background Layer
	{ "ff-06.bin",	0x10000, 0x5e7f3e8f, 5 | BRF_GRA },           // 15
	{ "ff-05.bin",	0x10000, 0x8bb13f05, 5 | BRF_GRA },           // 16
	{ "ff-07.bin",	0x10000, 0x0d7affc3, 5 | BRF_GRA },           // 17

	{ "ff-17.bin",	0x10000, 0xf0ab0d05, 6 | BRF_SND },           // 18 Samples
};

STD_ROM_PICK(triothepj)
STD_ROM_FN(triothepj)

struct BurnDriver BurnDrvTriothepj = {
	"triothepj", "triothep", NULL, NULL, "1989",
	"Trio The Punch - Never Forget Me... (Japan)\0", NULL, "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_DATAEAST, GBF_SCRFIGHT, 0,
	NULL, triothepjRomInfo, triothepjRomName, NULL, NULL, NULL, NULL, TriothepInputInfo, TriothepDIPInfo,
	TriothepInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	256, 240, 4, 3
};
