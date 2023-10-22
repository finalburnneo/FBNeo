// FinalBurn Neo Xain'd Sleena driver module
// Based on MAME driver by Carlos A. Lozano, Rob Rosenbrock, Phil Stroffolino, and Bryan McPhail

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "taito_m68705.h"
#include "burn_ym2203.h"
#include "burn_pal.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6809ROM[3];
static UINT8 *DrvMcuROM;
static UINT8 *DrvGfxROM[4];
static UINT8 *DrvShareRAM;
static UINT8 *DrvCharRAM;
static UINT8 *DrvBgRAM[2];
static UINT8 *DrvSprRAM;
static UINT8 *DrvSoundRAM;
static UINT8 *DrvMcuRAM;

static UINT8 main_bank;
static UINT8 sub_bank;
static UINT16 scrollx[2];
static UINT16 scrolly[2];
static INT32 vblank;
static UINT8 soundlatch;
static UINT8 flipscreen;
static UINT8 xain_pri;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static INT32 nExtraCycles[4];

static INT32 is_bootleg;

static struct BurnInputInfo DrvInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"P3 Coin",			BIT_DIGITAL,	DrvJoy3 + 2,	"p3 coin"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Drv)

static struct BurnDIPInfo DrvDIPList[]=
{
	DIP_OFFSET(0x12)
	{0x00, 0xff, 0xff, 0x7f, NULL					},
	{0x01, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x00, 0x01, 0x03, 0x00, "2 Coins 1 Credits"	},
	{0x00, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0x03, 0x01, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin A"				},
	{0x00, 0x01, 0x0c, 0x00, "2 Coins 1 Credits"	},
	{0x00, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0x0c, 0x04, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x00, 0x01, 0x10, 0x00, "Off"					},
	{0x00, 0x01, 0x10, 0x10, "On"					},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x00, 0x01, 0x20, 0x00, "No"					},
	{0x00, 0x01, 0x20, 0x20, "Yes"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x00, 0x01, 0x40, 0x00, "Upright"				},
	{0x00, 0x01, 0x40, 0x40, "Cocktail"				},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
//	{0x00, 0x01, 0x80, 0x00, "Off"					},
//	{0x00, 0x01, 0x80, 0x80, "On"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x01, 0x01, 0x03, 0x03, "Easy"					},
	{0x01, 0x01, 0x03, 0x02, "Normal"				},
	{0x01, 0x01, 0x03, 0x01, "Hard"					},
	{0x01, 0x01, 0x03, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    4, "Game Time"			},
	{0x01, 0x01, 0x0c, 0x0c, "Slow"					},
	{0x01, 0x01, 0x0c, 0x08, "Normal"				},
	{0x01, 0x01, 0x0c, 0x04, "Fast"					},
	{0x01, 0x01, 0x0c, 0x00, "Very Fast"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x01, 0x01, 0x30, 0x30, "20k 70k and every 70k"},
	{0x01, 0x01, 0x30, 0x20, "30k 80k and every 80k"},
	{0x01, 0x01, 0x30, 0x10, "20k and 80k"			},
	{0x01, 0x01, 0x30, 0x00, "30k and 80k"			},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x01, 0x01, 0xc0, 0xc0, "3"					},
	{0x01, 0x01, 0xc0, 0x80, "4"					},
	{0x01, 0x01, 0xc0, 0x40, "6"					},
	{0x01, 0x01, 0xc0, 0x00, "Infinite (Cheat)"		},
};

STDDIPINFO(Drv)

static inline void palette_update(INT32 offset)
{
	INT32 b = BurnPalRAM[offset + 0x200] & 0x0f;
	INT32 g = BurnPalRAM[offset] >> 4;
	INT32 r = BurnPalRAM[offset] & 0x0f;

	BurnPalette[offset] = BurnHighCol(pal4bit(r),pal4bit(g),pal4bit(b),0);
}

static void main_bankswitch(INT32 data)
{
	main_bank = data;
	M6809MapMemory(DrvM6809ROM[0] + ((data & 0x08) ? 0x14000 : 0x10000), 0x4000, 0x7fff, MAP_ROM);
}

static void sync_mcu()
{
	INT32 cyc = (M6809TotalCycles() * 2) - m6805TotalCycles();
	if (cyc > 0) {
		m6805Run(cyc);
	}
}

static void sync_sound()
{
	INT32 cyc = M6809TotalCycles(0);

	M6809CPUPush(2);
	BurnTimerUpdate(cyc);
	M6809CPUPop();
}

static void xain_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfc00) == 0x3c00) {
		BurnPalRAM[address & 0x3ff] = data;
		palette_update(address & 0x1ff);
		return;
	}

	switch (address)
	{
		case 0x3a00:
		case 0x3a01:
		case 0x3a04:
		case 0x3a05:
			scrollx[((address / 4) & 1) ^ 1] = ((data << ((address & 1) * 8)) | (scrollx[((address / 4) & 1) ^ 1] & (0xff00 >> ((address & 1) * 8)))) & 0x1ff;
		return;

		case 0x3a02:
		case 0x3a03:
		case 0x3a06:
		case 0x3a07:
			scrolly[((address / 4) & 1) ^ 1] = ((data << ((address & 1) * 8)) | (scrolly[((address / 4) & 1) ^ 1] & (0xff00 >> ((address & 1) * 8)))) & 0x1ff;
		return;

		case 0x3a08:
			sync_sound();
			soundlatch = data;
			M6809SetIRQLine(2, 0, CPU_IRQSTATUS_ACK); // hold line until soundlatch is read to ensure commands are read
		return;

		case 0x3a09:
			M6809SetIRQLine(0x20, CPU_IRQSTATUS_NONE);
		return;

		case 0x3a0a:
			M6809SetIRQLine(0x01, CPU_IRQSTATUS_NONE);
		return;

		case 0x3a0b:
			M6809SetIRQLine(0x00, CPU_IRQSTATUS_NONE);
		return;

		case 0x3a0c:
			M6809SetIRQLine(1, 0x00, CPU_IRQSTATUS_ACK);
		return;

		case 0x3a0d:
			flipscreen = data & 0x01;
		return;

		case 0x3a0e:
		{
			if (is_bootleg == 0)
			{
				sync_mcu();
				standard_taito_mcu_write(data);
			}
		}
		return;

		case 0x3a0f:
			xain_pri = data & 0x07;
			main_bankswitch(data);
		return;
	}
}

static UINT8 xain_main_read(UINT16 address)
{
	switch (address)
	{
		case 0x3a00:
			return DrvInputs[0];

		case 0x3a01:
			return DrvInputs[1];

		case 0x3a02:
			return DrvDips[0];

		case 0x3a03:
			return DrvDips[1];

		case 0x3a04:
			sync_mcu();
			return standard_taito_mcu_read();

		case 0x3a05:
		{
			UINT8 ret = DrvInputs[2] & ~0x20;

			if (is_bootleg == 0)
			{
				ret &= ~0x18;
				sync_mcu();
				if (mcu_sent == 0) ret |= 0x08;
				if (main_sent == 0) ret |= 0x10;
			}

			return ret | (vblank ? 0x20 : 0);
		}

		case 0x3a06:
		{
			if (is_bootleg == 0)
			{
				sync_mcu();

				m67805_taito_reset();
			}
			return 0xff;
		}
	}

	return 0;
}

static void sub_bankswitch(INT32 data)
{
	sub_bank = data;
	M6809MapMemory(DrvM6809ROM[1] + ((data & 0x01) ? 0x14000 : 0x10000), 0x4000, 0x7fff, MAP_ROM);
}

static void xain_sub_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x2000:
			M6809SetIRQLine(0, 0, CPU_IRQSTATUS_ACK); // maincpu irq
		return;

		case 0x2800:
			M6809SetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;

		case 0x3000:
			sub_bankswitch(data);
		return;
	}
}

static UINT8 xain_sub_read(UINT16 /*address*/)
{
	return 0;
}

static void xain_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x2800:
		case 0x2801:
			BurnYM2203Write(0, address & 1, data);
		return;

		case 0x3000:
		case 0x3001:
			BurnYM2203Write(1, address & 1, data);
		return;
	}
}

static UINT8 xain_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x1000:
			M6809SetIRQLine(0, CPU_IRQSTATUS_NONE);
			return soundlatch;
	}

	return 0;
}

static tilemap_callback( tx )
{
	INT32 attr = DrvCharRAM[offs + 0x400];
	INT32 code = DrvCharRAM[offs + 0x000] + ((attr & 0x03) << 8);
	INT32 color = attr >> 5; // & 7

	TILE_SET_INFO(0, code, color, 0);
}

static tilemap_callback( bg ) // DrvBgRAM[0], DrvGfxROM[2]
{
	INT32 attr = DrvBgRAM[0][offs + 0x400];
	INT32 code = DrvBgRAM[0][offs + 0x000] + ((attr & 0x07) << 8);
	INT32 flipx  = attr & 0x80;
	INT32 color  = (attr & 0x70) >> 4;

	TILE_SET_INFO(2, code, color, flipx ? TILE_FLIPX: 0);
}

static tilemap_callback( fg ) // DrvBgRAM[1], DrvGfxROM[1]
{
	INT32 attr = DrvBgRAM[1][offs + 0x400];
	INT32 code = DrvBgRAM[1][offs + 0x000] + ((attr & 0x07) << 8);
	INT32 flipx  = attr & 0x80;
	INT32 color  = (attr & 0x70) >> 4;

	TILE_SET_INFO(1, code, color, flipx ? TILE_FLIPX: 0);
}

static tilemap_scan( bg )
{
	return (col & 0x0f) + (((row & 0x0f) + (col & 0x10)) << 4) + ((row & 0x10) << 5);
}

inline static void DrvYM2203IRQHandler(INT32, INT32 nStatus)
{
	M6809SetIRQLine(0x01, ((nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE));
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	M6809Open(0);
	M6809Reset();
	main_bankswitch(0);
	M6809Close();

	M6809Open(1);
	M6809Reset();
	sub_bankswitch(0);
	M6809Close();

	M6809Open(2);
	M6809Reset();
	BurnYM2203Reset();
	M6809Close();

	m67805_taito_reset();

	scrollx[1] = 0;
	scrolly[1] = 0;
	scrollx[0] = 0;
	scrolly[0] = 0;
	vblank = 0;
	soundlatch = 0;
	flipscreen = 0;
	xain_pri = 0;

	nExtraCycles[0] = nExtraCycles[1] = nExtraCycles[2] = nExtraCycles[3] = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6809ROM[0]	= Next; Next += 0x018000;
	DrvM6809ROM[1]	= Next; Next += 0x018000;
	DrvM6809ROM[2]	= Next; Next += 0x010000;
	DrvMcuROM		= Next; Next += 0x000800;

	DrvGfxROM[0]	= Next; Next += 0x010000;
	DrvGfxROM[1]	= Next; Next += 0x080000;
	DrvGfxROM[2]	= Next; Next += 0x080000;
	DrvGfxROM[3]	= Next; Next += 0x080000;

	BurnPalette		= (UINT32*)Next; Next += 0x0200 * sizeof(UINT32);

	AllRam			= Next;

	DrvShareRAM		= Next; Next += 0x002000;
	DrvCharRAM		= Next; Next += 0x000800;
	DrvBgRAM[0]		= Next; Next += 0x000800;
	DrvBgRAM[1]		= Next; Next += 0x000800;
	DrvSprRAM		= Next; Next += 0x000200;
	BurnPalRAM		= Next; Next += 0x000400;
	DrvSoundRAM		= Next; Next += 0x000800;
	DrvMcuRAM		= Next; Next += 0x000080;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[4]  = { 0, 2, 4, 6 };
	INT32 Plane1[4]  = { RGN_FRAC(0x40000, 1,2)+0, RGN_FRAC(0x40000, 1,2)+4, 0, 4 };
	INT32 XOffs0[8]  = { STEP2(0*8+1,-1), STEP2(8*8+1,-1), STEP2(16*8+1,-1), STEP2(24*8+1,-1) };
	INT32 YOffs0[8]  = { STEP8(0,8) };
	INT32 XOffs1[16] = { STEP4(0*8+3,-1), STEP4(16*8+3,-1), STEP4(32*8+3,-1), STEP4(48*8+3,-1) };
	INT32 YOffs1[16] = { STEP16(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x40000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM[0], 0x08000);

	GfxDecode(0x0400, 4,  8,  8, Plane0, XOffs0, YOffs0, 0x100, tmp, DrvGfxROM[0]);

	memcpy (tmp, DrvGfxROM[1], 0x40000);

	GfxDecode(0x0800, 4, 16, 16, Plane1, XOffs1, YOffs1, 0x200, tmp, DrvGfxROM[1]);

	memcpy (tmp, DrvGfxROM[2], 0x40000);

	GfxDecode(0x0800, 4, 16, 16, Plane1, XOffs1, YOffs1, 0x200, tmp, DrvGfxROM[2]);

	memcpy (tmp, DrvGfxROM[3], 0x40000);

	GfxDecode(0x0800, 4, 16, 16, Plane1, XOffs1, YOffs1, 0x200, tmp, DrvGfxROM[3]);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	BurnSetRefreshRate(57.44);

	is_bootleg = (BurnDrvGetFlags() & BDF_BOOTLEG) ? 1 : 0;

	{
		if (BurnLoadRom(DrvM6809ROM[0] + 0x08000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM[0] + 0x10000,  1, 1)) return 1;

		if (BurnLoadRom(DrvM6809ROM[1] + 0x08000,  2, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM[1] + 0x10000,  3, 1)) return 1;

		if (BurnLoadRom(DrvM6809ROM[2] + 0x08000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[0] + 0x000000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[1] + 0x000000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x008000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x010000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x018000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x020000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x028000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x030000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x038000, 13, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[2] + 0x000000, 14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x008000, 15, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x010000, 16, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x020000, 17, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x028000, 18, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x030000, 19, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[3] + 0x000000, 20, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[3] + 0x008000, 21, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[3] + 0x010000, 22, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[3] + 0x018000, 23, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[3] + 0x020000, 24, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[3] + 0x028000, 25, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[3] + 0x030000, 26, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[3] + 0x038000, 27, 1)) return 1;

		if (is_bootleg == 0) {
			if (BurnLoadRom(DrvMcuROM    + 0x000000, 29, 1)) return 1;
		}

		DrvGfxDecode();
	}

	M6809Init(0);
	M6809Open(0);
	M6809MapMemory(DrvShareRAM,		0x0000, 0x1fff, MAP_RAM);
	M6809MapMemory(DrvCharRAM,		0x2000, 0x27ff, MAP_RAM);
	M6809MapMemory(DrvBgRAM[1],		0x2800, 0x2fff, MAP_RAM);
	M6809MapMemory(DrvBgRAM[0],		0x3000, 0x37ff, MAP_RAM);
	M6809MapMemory(DrvSprRAM,		0x3800, 0x39ff, MAP_RAM);
	M6809MapMemory(BurnPalRAM,		0x3c00, 0x3fff, MAP_ROM);
	M6809MapMemory(DrvM6809ROM[0] + 0x8000,	0x8000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(xain_main_write);
	M6809SetReadHandler(xain_main_read);
	M6809Close();

	M6809Init(1);
	M6809Open(1);
	M6809MapMemory(DrvShareRAM,		0x0000, 0x1fff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM[1] + 0x8000,	0x8000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(xain_sub_write);
	M6809SetReadHandler(xain_sub_read);
	M6809Close();

	M6809Init(2);
	M6809Open(2);
	M6809MapMemory(DrvSoundRAM,		0x0000, 0x07ff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM[2] + 0x4000,	0x4000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(xain_sound_write);
	M6809SetReadHandler(xain_sound_read);
	M6809Close();

	m67805_taito_init(DrvMcuROM, DrvMcuRAM, &standard_m68705_interface);

	BurnYM2203Init(2,  6000000/2, &DrvYM2203IRQHandler, 0);
	BurnTimerAttachM6809(1500000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE,   0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.40, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_YM2203_ROUTE,   0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_1, 0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_2, 0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_3, 0.40, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, tx_map_callback, 8, 8, 32, 32);
	GenericTilemapInit(1, bg_map_scan, bg_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(2, bg_map_scan, fg_map_callback, 16, 16, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 4,  8,  8, 0x010000, 0x000, 0x07);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 4, 16, 16, 0x080000, 0x100, 0x07);
	GenericTilemapSetGfx(2, DrvGfxROM[2], 4, 16, 16, 0x080000, 0x180, 0x07);
	GenericTilemapSetGfx(3, DrvGfxROM[3], 4, 16, 16, 0x080000, 0x080, 0x07);
	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetTransparent(2, 0);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -8);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6809Exit();
	m67805_taito_exit();

	BurnYM2203Exit();

	BurnFreeMemIndex();

	return 0;
}

static void draw_sprites()
{
	for (INT32 offs = 0; offs < 0x180; offs += 4)
	{
		INT32 attr = DrvSprRAM[offs + 1];
		INT32 code = DrvSprRAM[offs + 2] | ((attr & 7) << 8);
		INT32 sx   = 238 - DrvSprRAM[offs + 3];
		INT32 sy   = 240 - DrvSprRAM[offs + 0];
		
		INT32 color = (attr & 0x38) >> 3;
		if (sx <= -7) sx += 256;
		if (sy <= -7) sy += 256;
		INT32 flipx = attr & 0x40;
		INT32 flipy = 0;

		if (attr & 0x80) // double-height
		{
			DrawGfxMaskTile(0, 3, code, sx, (flipy ? sy+16:sy-16)-8, flipx, flipy, color, 0);
			code++;
		}

		DrawGfxMaskTile(0, 3, code, sx, sy - 8, flipx, flipy, color, 0);
	}
}

static INT32 DrvDraw()
{
	if (BurnRecalc) {
		for (INT32 i = 0; i < 0x200; i++) {
			palette_update(i);
		}
		BurnRecalc = 0;
	}

	BurnTransferClear();

	GenericTilemapSetScrollX(1, scrollx[0]);
	GenericTilemapSetScrollY(1, scrolly[0]);
	GenericTilemapSetScrollX(2, scrollx[1]);
	GenericTilemapSetScrollY(2, scrolly[1]);

	const INT32 lut[8][4] = { { 0, 1, 2, 3 }, { 1, 0, 2, 3 }, { 3, 0, 2, 1 },
							  { 3, 1, 2, 0 }, { 0, 3, 2, 1 }, { 1, 3, 2, 0 },
							  { 0, 2, 1, 3 }, { 1, 2, 0, 3 } };

	for (INT32 i = 0; i < 4; i++)
	{
		if ((lut[xain_pri & 0x07][i] == 0) && (nBurnLayer & 1)) GenericTilemapDraw(1, 0, (i == 0) ? TMAP_FORCEOPAQUE : 0);
		if ((lut[xain_pri & 0x07][i] == 1) && (nBurnLayer & 2)) GenericTilemapDraw(2, 0, (i == 0) ? TMAP_FORCEOPAQUE : 0);
		if ((lut[xain_pri & 0x07][i] == 2) && (nSpriteEnable & 1)) draw_sprites();
		if ((lut[xain_pri & 0x07][i] == 3) && (nBurnLayer & 4)) GenericTilemapDraw(0, 0, (i == 0) ? TMAP_FORCEOPAQUE : 0);
	}

	BurnTransferCopy(BurnPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	M6809NewFrame();
	m6805NewFrame();

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 272*8;
	INT32 nCyclesTotal[4] = { (INT32)((double)1500000 / 57.44), (INT32)((double)1500000 / 57.44), (INT32)((double)1500000 / 57.44), (INT32)((double)3000000 / 57.44) };
	INT32 nCyclesDone[4] = { nExtraCycles[0], nExtraCycles[1], 0, 0 };

	m6805Open(0);
	m6805Idle(nExtraCycles[3]);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		M6809Open(0);
		if ((i&0x7) == 7) {
			INT32 scanline = i / 8;

			// IRQ1: 0-240 (240 inclusive), every 16 scanlines + once at 264
			if ( (scanline < 241 && (scanline%16) == 0) || scanline == 264) {
				M6809SetIRQLine(0x01, CPU_IRQSTATUS_ACK);
			}
			if (scanline == 8) {
				vblank = 0;
			}
			if (scanline == 239) {
				vblank = 1;
			}
			if (scanline == 240) {
				M6809SetIRQLine(0x20, CPU_IRQSTATUS_ACK);

				if (pBurnDraw) {
					BurnDrvRedraw();
				}
			}
		}
		CPU_RUN(0, M6809);
		M6809Close();

		M6809Open(1);
		CPU_RUN(1, M6809);
		M6809Close();

		CPU_RUN_SYNCINT(3, m6805); // sync internally because m6805Run() in handler!

		M6809Open(2);
		CPU_RUN_TIMER(2);
		M6809Close();
	}

	nExtraCycles[0] = nCyclesDone[0] - nCyclesTotal[0];
	nExtraCycles[1] = nCyclesDone[1] - nCyclesTotal[1];
	nExtraCycles[3] = m6805TotalCycles() - nCyclesTotal[3];

	m6805Close();

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029722;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		M6809Scan(nAction);
		m68705_taito_scan(nAction);

		BurnYM2203Scan(nAction, pnMin);

		SCAN_VAR(main_bank);
		SCAN_VAR(sub_bank);
		SCAN_VAR(scrollx);
		SCAN_VAR(scrolly);
		SCAN_VAR(soundlatch);
		SCAN_VAR(flipscreen);
		SCAN_VAR(xain_pri);

		SCAN_VAR(nExtraCycles);
	}

	if (nAction & ACB_WRITE)
	{
		M6809Open(0);
		main_bankswitch(main_bank);
		M6809Close();

		M6809Open(1);
		sub_bankswitch(sub_bank);
		M6809Close();
	}

	return 0;
}


// Xain'd Sleena (World)

static struct BurnRomInfo xsleenaRomDesc[] = {
	{ "p9-08.ic66",		0x8000, 0x5179ae3f, 1 | BRF_PRG | BRF_ESS }, //  0 Master M6809 Code
	{ "pa-09.ic65",		0x8000, 0x10a7c800, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "p1-0.ic29",		0x8000, 0xa1a860e2, 2 | BRF_PRG | BRF_ESS }, //  2 Slave M6809 code
	{ "p0-0.ic15",		0x8000, 0x948b9757, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "p2-0.ic49",		0x8000, 0xa5318cb8, 3 | BRF_PRG | BRF_ESS }, //  4 Sound M6809 Code

	{ "pb-0.ic24",		0x8000, 0x83c00dd8, 4 | BRF_GRA },           //  5 Character

	{ "pk-0.ic136",		0x8000, 0x11eb4247, 5 | BRF_GRA },           //  6 Background Layer 1 tiles
	{ "pl-0.ic135",		0x8000, 0x422b536e, 5 | BRF_GRA },           //  7
	{ "pm-0.ic134",		0x8000, 0x828c1b0c, 5 | BRF_GRA },           //  8
	{ "pn-0.ic133",		0x8000, 0xd37939e0, 5 | BRF_GRA },           //  9
	{ "pc-0.ic114",		0x8000, 0x8f0aa1a7, 5 | BRF_GRA },           // 10
	{ "pd-0.ic113",		0x8000, 0x45681910, 5 | BRF_GRA },           // 11
	{ "pe-0.ic112",		0x8000, 0xa8eeabc8, 5 | BRF_GRA },           // 12
	{ "pf-0.ic111",		0x8000, 0xe59a2f27, 5 | BRF_GRA },           // 13

	{ "p5-0.ic44",		0x8000, 0x5c6c453c, 6 | BRF_GRA },           // 14 Background Layer 0 tiles
	{ "p4-0.ic45",		0x8000, 0x59d87a9a, 6 | BRF_GRA },           // 15
	{ "p3-0.ic46",		0x8000, 0x84884a2e, 6 | BRF_GRA },           // 16
	{ "p6-0.ic43",		0x8000, 0x8d637639, 6 | BRF_GRA },           // 17
	{ "p7-0.ic42",		0x8000, 0x71eec4e6, 6 | BRF_GRA },           // 18
	{ "p8-0.ic41",		0x8000, 0x7fc9704f, 6 | BRF_GRA },           // 19

	{ "po-0.ic131",		0x8000, 0x252976ae, 7 | BRF_GRA },           // 20 Sprite tiles
	{ "pp-0.ic130",		0x8000, 0xe6f1e8d5, 7 | BRF_GRA },           // 21
	{ "pq-0.ic129",		0x8000, 0x785381ed, 7 | BRF_GRA },           // 22
	{ "pr-0.ic128",		0x8000, 0x59754e3d, 7 | BRF_GRA },           // 23
	{ "pg-0.ic109",		0x8000, 0x4d977f33, 7 | BRF_GRA },           // 24
	{ "ph-0.ic108",		0x8000, 0x3f3b62a0, 7 | BRF_GRA },           // 25
	{ "pi-0.ic107",		0x8000, 0x76641ee3, 7 | BRF_GRA },           // 26
	{ "pj-0.ic106",		0x8000, 0x37671f36, 7 | BRF_GRA },           // 27

	{ "pt-0.ic59",		0x0100, 0xfed32888, 8 | BRF_GRA | BRF_OPT }, // 28 Priority Prom

	{ "pz-0.113",		0x0800, 0xa432a907, 9 | BRF_PRG | BRF_ESS }, // 29 M68705 Code
};

STD_ROM_PICK(xsleena)
STD_ROM_FN(xsleena)

struct BurnDriver BurnDrvXsleena = {
	"xsleena", NULL, NULL, NULL, "1986",
	"Xain'd Sleena (World)\0", NULL, "Technos Japan (Taito license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TECHNOS, GBF_RUNGUN, 0,
	NULL, xsleenaRomInfo, xsleenaRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x200,
	256, 240, 4, 3
};


// Xain'd Sleena (Japan)

static struct BurnRomInfo xsleenajRomDesc[] = {
	{ "p9-01.ic66",		0x8000, 0x370164be, 1 | BRF_PRG | BRF_ESS }, //  0 Master M6809 Code
	{ "pa-0.ic65",		0x8000, 0xd22bf859, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "p1-0.ic29",		0x8000, 0xa1a860e2, 2 | BRF_PRG | BRF_ESS }, //  2 Slave M6809 code
	{ "p0-0.ic15",		0x8000, 0x948b9757, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "p2-0.ic49",		0x8000, 0xa5318cb8, 3 | BRF_PRG | BRF_ESS }, //  4 Sound M6809 Code

	{ "pb-0.ic24",		0x8000, 0x83c00dd8, 4 | BRF_GRA },           //  5 Character

	{ "pk-0.ic136",		0x8000, 0x11eb4247, 5 | BRF_GRA },           //  6 Background Layer 1 tiles
	{ "pl-0.ic135",		0x8000, 0x422b536e, 5 | BRF_GRA },           //  7
	{ "pm-0.ic134",		0x8000, 0x828c1b0c, 5 | BRF_GRA },           //  8
	{ "pn-0.ic133",		0x8000, 0xd37939e0, 5 | BRF_GRA },           //  9
	{ "pc-0.ic114",		0x8000, 0x8f0aa1a7, 5 | BRF_GRA },           // 10
	{ "pd-0.ic113",		0x8000, 0x45681910, 5 | BRF_GRA },           // 11
	{ "pe-0.ic112",		0x8000, 0xa8eeabc8, 5 | BRF_GRA },           // 12
	{ "pf-0.ic111",		0x8000, 0xe59a2f27, 5 | BRF_GRA },           // 13

	{ "p5-0.ic44",		0x8000, 0x5c6c453c, 6 | BRF_GRA },           // 14 Background Layer 0 tiles
	{ "p4-0.ic45",		0x8000, 0x59d87a9a, 6 | BRF_GRA },           // 15
	{ "p3-0.ic46",		0x8000, 0x84884a2e, 6 | BRF_GRA },           // 16
	{ "p6-0.ic43",		0x8000, 0x8d637639, 6 | BRF_GRA },           // 17
	{ "p7-0.ic42",		0x8000, 0x71eec4e6, 6 | BRF_GRA },           // 18
	{ "p8-0.ic41",		0x8000, 0x7fc9704f, 6 | BRF_GRA },           // 19

	{ "po-0.ic131",		0x8000, 0x252976ae, 7 | BRF_GRA },           // 20 Sprite tiles
	{ "pp-0.ic130",		0x8000, 0xe6f1e8d5, 7 | BRF_GRA },           // 21
	{ "pq-0.ic129",		0x8000, 0x785381ed, 7 | BRF_GRA },           // 22
	{ "pr-0.ic128",		0x8000, 0x59754e3d, 7 | BRF_GRA },           // 23
	{ "pg-0.ic109",		0x8000, 0x4d977f33, 7 | BRF_GRA },           // 24
	{ "ph-0.ic108",		0x8000, 0x3f3b62a0, 7 | BRF_GRA },           // 25
	{ "pi-0.ic107",		0x8000, 0x76641ee3, 7 | BRF_GRA },           // 26
	{ "pj-0.ic106",		0x8000, 0x37671f36, 7 | BRF_GRA },           // 27

	{ "pt-0.ic59",		0x0100, 0xfed32888, 8 | BRF_GRA | BRF_OPT }, // 28 Priority Prom

	{ "pz-0.113",		0x0800, 0xa432a907, 9 | BRF_PRG | BRF_ESS }, // 29 M68705 Code
};

STD_ROM_PICK(xsleenaj)
STD_ROM_FN(xsleenaj)

struct BurnDriver BurnDrvXsleenaj = {
	"xsleenaj", "xsleena", NULL, NULL, "1986",
	"Xain'd Sleena (Japan)\0", NULL, "Technos Japan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TECHNOS, GBF_RUNGUN, 0,
	NULL, xsleenajRomInfo, xsleenajRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x200,
	256, 240, 4, 3
};


// Solar-Warrior (US)

static struct BurnRomInfo solrwarrRomDesc[] = {
	{ "p9-02.ic66",		0x8000, 0x8ff372a8, 1 | BRF_PRG | BRF_ESS }, //  0 Master M6809 code
	{ "pa-03.ic65",		0x8000, 0x154f946f, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "p1-02.ic29",		0x8000, 0xf5f235a3, 2 | BRF_PRG | BRF_ESS }, //  2 Slave M6809 code
	{ "p0-02.ic133",	0x8000, 0x51ae95ae, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "p2-0.ic49",		0x8000, 0xa5318cb8, 3 | BRF_PRG | BRF_ESS }, //  4 Sound M6809 Code

	{ "pb-0.ic24",		0x8000, 0x83c00dd8, 4 | BRF_GRA },           //  5 Character

	{ "pk-0.ic136",		0x8000, 0x11eb4247, 5 | BRF_GRA },           //  6 Background Layer 1 tiles
	{ "pl-0.ic135",		0x8000, 0x422b536e, 5 | BRF_GRA },           //  7
	{ "pm-0.ic134",		0x8000, 0x828c1b0c, 5 | BRF_GRA },           //  8
	{ "pn-02.ic133",	0x8000, 0xd2ed6f94, 5 | BRF_GRA },           //  9
	{ "pc-0.ic114",		0x8000, 0x8f0aa1a7, 5 | BRF_GRA },           // 10
	{ "pd-0.ic113",		0x8000, 0x45681910, 5 | BRF_GRA },           // 11
	{ "pe-0.ic112",		0x8000, 0xa8eeabc8, 5 | BRF_GRA },           // 12
	{ "pf-02.ic111",	0x8000, 0x6e627a77, 5 | BRF_GRA },           // 13

	{ "p5-0.ic44",		0x8000, 0x5c6c453c, 6 | BRF_GRA },           // 14 Background Layer 0 tiles
	{ "p4-0.ic45",		0x8000, 0x59d87a9a, 6 | BRF_GRA },           // 15
	{ "p3-0.ic46",		0x8000, 0x84884a2e, 6 | BRF_GRA },           // 16
	{ "p6-0.ic43",		0x8000, 0x8d637639, 6 | BRF_GRA },           // 17
	{ "p7-0.ic42",		0x8000, 0x71eec4e6, 6 | BRF_GRA },           // 18
	{ "p8-0.ic41",		0x8000, 0x7fc9704f, 6 | BRF_GRA },           // 19

	{ "po-0.ic131",		0x8000, 0x252976ae, 7 | BRF_GRA },           // 20 Sprite tiles
	{ "pp-0.ic130",		0x8000, 0xe6f1e8d5, 7 | BRF_GRA },           // 21
	{ "pq-0.ic129",		0x8000, 0x785381ed, 7 | BRF_GRA },           // 22
	{ "pr-0.ic128",		0x8000, 0x59754e3d, 7 | BRF_GRA },           // 23
	{ "pg-0.ic109",		0x8000, 0x4d977f33, 7 | BRF_GRA },           // 24
	{ "ph-0.ic108",		0x8000, 0x3f3b62a0, 7 | BRF_GRA },           // 25
	{ "pi-0.ic107",		0x8000, 0x76641ee3, 7 | BRF_GRA },           // 26
	{ "pj-0.ic106",		0x8000, 0x37671f36, 7 | BRF_GRA },           // 27

	{ "pt-0.ic59",		0x0100, 0xfed32888, 8 | BRF_GRA | BRF_OPT }, // 28 Priority Prom

	{ "pz-0.113",		0x0800, 0xa432a907, 9 | BRF_PRG | BRF_ESS }, // 29 M68705 Code
};

STD_ROM_PICK(solrwarr)
STD_ROM_FN(solrwarr)

struct BurnDriver BurnDrvSolrwarr = {
	"solrwarr", "xsleena", NULL, NULL, "1986",
	"Solar-Warrior (US)\0", NULL, "Technos Japan (Taito / Memetron license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TECHNOS, GBF_RUNGUN, 0,
	NULL, solrwarrRomInfo, solrwarrRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x200,
	256, 240, 4, 3
};


// Xain'd Sleena (bootleg, set 1)

static struct BurnRomInfo xsleenabRomDesc[] = {
	{ "1.rom",			0x8000, 0x79f515a7, 1 | BRF_PRG | BRF_ESS }, //  0 Master M6809 Code
	{ "pa-0.ic65",		0x8000, 0xd22bf859, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "p1-0.ic29",		0x8000, 0xa1a860e2, 2 | BRF_PRG | BRF_ESS }, //  2 Slave M6809 code
	{ "p0-0.ic15",		0x8000, 0x948b9757, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "p2-0.ic49",		0x8000, 0xa5318cb8, 3 | BRF_PRG | BRF_ESS }, //  4 Sound M6809 Code

	{ "pb-0.ic24",		0x8000, 0x83c00dd8, 4 | BRF_GRA },           //  5 Character

	{ "pk-0.ic136",		0x8000, 0x11eb4247, 5 | BRF_GRA },           //  6 Background Layer 1 tiles
	{ "pl-0.ic135",		0x8000, 0x422b536e, 5 | BRF_GRA },           //  7
	{ "pm-0.ic134",		0x8000, 0x828c1b0c, 5 | BRF_GRA },           //  8
	{ "pn-0.ic133",		0x8000, 0xd37939e0, 5 | BRF_GRA },           //  9
	{ "pc-0.ic114",		0x8000, 0x8f0aa1a7, 5 | BRF_GRA },           // 10
	{ "pd-0.ic113",		0x8000, 0x45681910, 5 | BRF_GRA },           // 11
	{ "pe-0.ic112",		0x8000, 0xa8eeabc8, 5 | BRF_GRA },           // 12
	{ "pf-0.ic111",		0x8000, 0xe59a2f27, 5 | BRF_GRA },           // 13

	{ "p5-0.ic44",		0x8000, 0x5c6c453c, 6 | BRF_GRA },           // 14 Background Layer 0 tiles
	{ "p4-0.ic45",		0x8000, 0x59d87a9a, 6 | BRF_GRA },           // 15
	{ "p3-0.ic46",		0x8000, 0x84884a2e, 6 | BRF_GRA },           // 16
	{ "p6-0.ic43",		0x8000, 0x8d637639, 6 | BRF_GRA },           // 17
	{ "p7-0.ic42",		0x8000, 0x71eec4e6, 6 | BRF_GRA },           // 18
	{ "p8-0.ic41",		0x8000, 0x7fc9704f, 6 | BRF_GRA },           // 19

	{ "po-0.ic131",		0x8000, 0x252976ae, 7 | BRF_GRA },           // 20 Sprite tiles
	{ "pp-0.ic130",		0x8000, 0xe6f1e8d5, 7 | BRF_GRA },           // 21
	{ "pq-0.ic129",		0x8000, 0x785381ed, 7 | BRF_GRA },           // 22
	{ "pr-0.ic128",		0x8000, 0x59754e3d, 7 | BRF_GRA },           // 23
	{ "pg-0.ic109",		0x8000, 0x4d977f33, 7 | BRF_GRA },           // 24
	{ "ph-0.ic108",		0x8000, 0x3f3b62a0, 7 | BRF_GRA },           // 25
	{ "pi-0.ic107",		0x8000, 0x76641ee3, 7 | BRF_GRA },           // 26
	{ "pj-0.ic106",		0x8000, 0x37671f36, 7 | BRF_GRA },           // 27

	{ "pt-0.ic59",		0x0100, 0xfed32888, 8 | BRF_GRA | BRF_OPT }, // 28 Priority Prom
};

STD_ROM_PICK(xsleenab)
STD_ROM_FN(xsleenab)

struct BurnDriver BurnDrvXsleenab = {
	"xsleenab", "xsleena", NULL, NULL, "1986",
	"Xain'd Sleena (bootleg, set 1)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TECHNOS, GBF_RUNGUN, 0,
	NULL, xsleenabRomInfo, xsleenabRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x200,
	256, 240, 4, 3
};


// Xain'd Sleena (bootleg, bugfixed)
// newer bootleg, fixes some of the issues with the other one
// f205v id 1016

static struct BurnRomInfo xsleenabaRomDesc[] = {
	{ "xs87b-10.7d",	0x8000, 0x3d5f9fb4, 1 | BRF_PRG | BRF_ESS }, //  0 Master M6809 Code
	{ "xs87b-11.7c",	0x8000, 0x81c80d54, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "p1-0.ic29",		0x8000, 0xa1a860e2, 2 | BRF_PRG | BRF_ESS }, //  2 Slave M6809 code
	{ "p0-0.ic15",		0x8000, 0x948b9757, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "p2-0.ic49",		0x8000, 0xa5318cb8, 3 | BRF_PRG | BRF_ESS }, //  4 Sound M6809 Code

	{ "pb-0.ic24",		0x8000, 0x83c00dd8, 4 | BRF_GRA },           //  5 Character

	{ "pk-0.ic136",		0x8000, 0x11eb4247, 5 | BRF_GRA },           //  6 Background Layer 1 tiles
	{ "pl-0.ic135",		0x8000, 0x422b536e, 5 | BRF_GRA },           //  7
	{ "pm-0.ic134",		0x8000, 0x828c1b0c, 5 | BRF_GRA },           //  8
	{ "pn-0.ic133",		0x8000, 0xd37939e0, 5 | BRF_GRA },           //  9
	{ "pc-0.ic114",		0x8000, 0x8f0aa1a7, 5 | BRF_GRA },           // 10
	{ "pd-0.ic113",		0x8000, 0x45681910, 5 | BRF_GRA },           // 11
	{ "pe-0.ic112",		0x8000, 0xa8eeabc8, 5 | BRF_GRA },           // 12
	{ "pf-0.ic111",		0x8000, 0xe59a2f27, 5 | BRF_GRA },           // 13

	{ "p5-0.ic44",		0x8000, 0x5c6c453c, 6 | BRF_GRA },           // 14 Background Layer 0 tiles
	{ "p4-0.ic45",		0x8000, 0x59d87a9a, 6 | BRF_GRA },           // 15
	{ "p3-0.ic46",		0x8000, 0x84884a2e, 6 | BRF_GRA },           // 16
	{ "p6-0.ic43",		0x8000, 0x8d637639, 6 | BRF_GRA },           // 17
	{ "p7-0.ic42",		0x8000, 0x71eec4e6, 6 | BRF_GRA },           // 18
	{ "p8-0.ic41",		0x8000, 0x7fc9704f, 6 | BRF_GRA },           // 19

	{ "po-0.ic131",		0x8000, 0x252976ae, 7 | BRF_GRA },           // 20 Sprite tiles
	{ "pp-0.ic130",		0x8000, 0xe6f1e8d5, 7 | BRF_GRA },           // 21
	{ "pq-0.ic129",		0x8000, 0x785381ed, 7 | BRF_GRA },           // 22
	{ "pr-0.ic128",		0x8000, 0x59754e3d, 7 | BRF_GRA },           // 23
	{ "pg-0.ic109",		0x8000, 0x4d977f33, 7 | BRF_GRA },           // 24
	{ "ph-0.ic108",		0x8000, 0x3f3b62a0, 7 | BRF_GRA },           // 25
	{ "pi-0.ic107",		0x8000, 0x76641ee3, 7 | BRF_GRA },           // 26
	{ "pj-0.ic106",		0x8000, 0x37671f36, 7 | BRF_GRA },           // 27

	{ "pt-0.ic59",		0x0100, 0xfed32888, 8 | BRF_GRA | BRF_OPT }, // 28 Priority Prom
};

STD_ROM_PICK(xsleenaba)
STD_ROM_FN(xsleenaba)

struct BurnDriver BurnDrvXsleenaba = {
	"xsleenaba", "xsleena", NULL, NULL, "1987",
	"Xain'd Sleena (bootleg, bugfixed)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TECHNOS, GBF_RUNGUN, 0,
	NULL, xsleenabaRomInfo, xsleenabaRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x200,
	256, 240, 4, 3
};


// Xain'd Sleena (bootleg, set 2)

static struct BurnRomInfo xsleenabbRomDesc[] = {
	{ "ic66",		0x8000, 0xf91cae92, 1 | BRF_PRG | BRF_ESS }, //  0 Master M6809 Code
	{ "ic65",		0x8000, 0xd22bf859, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ic29",		0x8000, 0xa1a860e2, 2 | BRF_PRG | BRF_ESS }, //  2 Slave M6809 code
	{ "ic15",		0x8000, 0x948b9757, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "ic49",		0x8000, 0xa5318cb8, 3 | BRF_PRG | BRF_ESS }, //  4 Sound M6809 Code

	{ "ic24",		0x8000, 0x83c00dd8, 4 | BRF_GRA },           //  5 Character

	{ "ic136",		0x8000, 0x11eb4247, 5 | BRF_GRA },           //  6 Background Layer 1 tiles
	{ "ic135",		0x8000, 0x422b536e, 5 | BRF_GRA },           //  7
	{ "ic134",		0x8000, 0x828c1b0c, 5 | BRF_GRA },           //  8
	{ "ic133",		0x8000, 0xd37939e0, 5 | BRF_GRA },           //  9
	{ "ic114",		0x8000, 0x8f0aa1a7, 5 | BRF_GRA },           // 10
	{ "ic113",		0x8000, 0x45681910, 5 | BRF_GRA },           // 11
	{ "ic112",		0x8000, 0xa8eeabc8, 5 | BRF_GRA },           // 12
	{ "ic111",		0x8000, 0xe59a2f27, 5 | BRF_GRA },           // 13

	{ "ic44",		0x8000, 0x5c6c453c, 6 | BRF_GRA },           // 14 Background Layer 0 tiles
	{ "ic45",		0x8000, 0x59d87a9a, 6 | BRF_GRA },           // 15
	{ "ic46",		0x8000, 0x84884a2e, 6 | BRF_GRA },           // 16
	{ "ic43",		0x8000, 0x8d637639, 6 | BRF_GRA },           // 17
	{ "ic42",		0x8000, 0x71eec4e6, 6 | BRF_GRA },           // 18
	{ "ic41",		0x8000, 0x7fc9704f, 6 | BRF_GRA },           // 19

	{ "ic131",		0x8000, 0x252976ae, 7 | BRF_GRA },           // 20 Sprite tiles
	{ "ic130",		0x8000, 0xe6f1e8d5, 7 | BRF_GRA },           // 21
	{ "ic129",		0x8000, 0x785381ed, 7 | BRF_GRA },           // 22
	{ "ic128",		0x8000, 0x59754e3d, 7 | BRF_GRA },           // 23
	{ "ic109",		0x8000, 0x4d977f33, 7 | BRF_GRA },           // 24
	{ "ic108",		0x8000, 0x3f3b62a0, 7 | BRF_GRA },           // 25
	{ "ic107",		0x8000, 0x76641ee3, 7 | BRF_GRA },           // 26
	{ "ic106",		0x8000, 0x37671f36, 7 | BRF_GRA },           // 27

	{ "pt-0.ic59",	0x0100, 0xfed32888, 8 | BRF_GRA | BRF_OPT }, // 28 Priority Prom
};

STD_ROM_PICK(xsleenabb)
STD_ROM_FN(xsleenabb)

struct BurnDriver BurnDrvXsleenabb = {
	"xsleenabb", "xsleena", NULL, NULL, "1987",
	"Xain'd Sleena (bootleg, set 2)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TECHNOS, GBF_RUNGUN, 0,
	NULL, xsleenabbRomInfo, xsleenabbRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x200,
	256, 240, 4, 3
};
