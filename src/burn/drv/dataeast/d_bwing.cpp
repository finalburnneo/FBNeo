// FB Alpha B-Wing / Zaviga driver module
// Based on MAME driver by Acho A. Tang and Alex W. Jackson

// Note dac missing output filtering...

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "m6502_intf.h"
#include "driver.h"
extern "C" {
#include "ay8910.h"
}
#include "dac.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6809ROM0;
static UINT8 *DrvM6809ROM1;
static UINT8 *DrvM6502ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvBgRAM;
static UINT8 *DrvShareRAM;
static UINT8 *DrvM6809RAM0;
static UINT8 *DrvM6809RAM1;
static UINT8 *DrvM6502RAM;

static UINT32 *DrvPalette;
static UINT8  DrvRecalc;

static INT16 *pAY8910Buffer[6];

static UINT8 *scroll;

static UINT8 soundlatch;
static UINT8 nmimask;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[4];
static UINT8 DrvReset;

static INT32 vblank;
static INT32 screen_flipx;

static struct BurnInputInfo BwingInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 6,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy4 + 0,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Bwing)

static struct BurnDIPInfo BwingDIPList[]=
{
	{0x13, 0xff, 0xff, 0xdf, NULL			},
	{0x14, 0xff, 0xff, 0xbf, NULL			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x13, 0x01, 0x03, 0x00, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x03, 0x01, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x13, 0x01, 0x0c, 0x00, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x0c, 0x04, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    2, "Diagnostics"		},
	{0x13, 0x01, 0x10, 0x10, "Off"			},
	{0x13, 0x01, 0x10, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x13, 0x01, 0x20, 0x00, "Upright"		},
	{0x13, 0x01, 0x20, 0x20, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Invincibility"	},
	{0x13, 0x01, 0x40, 0x40, "Off"			},
	{0x13, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Infinite"		},
	{0x13, 0x01, 0x80, 0x80, "Off"			},
	{0x13, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x14, 0x01, 0x01, 0x00, "5"			},
	{0x14, 0x01, 0x01, 0x01, "3"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x14, 0x01, 0x06, 0x00, "40000"		},
	{0x14, 0x01, 0x06, 0x02, "20000 80000"		},
	{0x14, 0x01, 0x06, 0x04, "20000 60000"		},
	{0x14, 0x01, 0x06, 0x06, "20000 40000"		},

	{0   , 0xfe, 0   ,    2, "Enemy Crafts"		},
	{0x14, 0x01, 0x08, 0x00, "Hard"			},
	{0x14, 0x01, 0x08, 0x08, "Normal"		},

	{0   , 0xfe, 0   ,    2, "Enemy Missiles"	},
	{0x14, 0x01, 0x10, 0x00, "Hard"			},
	{0x14, 0x01, 0x10, 0x10, "Normal"		},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x14, 0x01, 0x20, 0x20, "Off"			},
	{0x14, 0x01, 0x20, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Country"		},
	{0x14, 0x01, 0x40, 0x00, "Japan/US"		},
	{0x14, 0x01, 0x40, 0x40, "Japan Only"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Bwing)

static void bankswitch(INT32 data)
{
	INT32 bank = (data >> 6) & 3;

	if (bank == 0)
	{
		M6809MapMemory(DrvFgRAM,		0x2000, 0x2fff, MAP_RAM);
		M6809MapMemory(DrvBgRAM,		0x3000, 0x3fff, MAP_RAM);
 	}
	else
	{
		bank = (bank - 1) * 0x2000;

		M6809MapMemory(DrvGfxRAM + bank,	0x2000, 0x3fff, MAP_RAM);
	}
}

static void control_write(INT32 cpu, INT32 offset)
{
	const INT32 lines[2][3] ={
		{ CPU_IRQLINE_IRQ, CPU_IRQLINE_FIRQ, CPU_IRQLINE_NMI },
		{ CPU_IRQLINE_FIRQ, CPU_IRQLINE_IRQ, CPU_IRQLINE_NMI }
	};

	switch (offset & 3)
	{
		case 0:
			M6809Close();
			M6809Open(cpu^1);
			M6809SetIRQLine(CPU_IRQLINE_IRQ, CPU_IRQSTATUS_ACK);
			M6809Close();
			M6809Open(cpu);
		return;

		case 1:
		case 2:
		case 3:
			M6809SetIRQLine(lines[cpu][(offset & 3)-1], CPU_IRQSTATUS_NONE);
		return;
	}
}

static void bwing_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xff00) == 0x1a00) {
		DrvPalRAM[(address & 0xff) * 2 + 0] = data;
		DrvPalRAM[(address & 0xff) * 2 + 1] = scroll[6];
		return;
	}

	switch (address)
	{
		case 0x1b00:
		case 0x1b01:
		case 0x1b02:
		case 0x1b03:
		case 0x1b04:
		case 0x1b05:
		case 0x1b06:
			scroll[address & 7] = data;
		return;

		case 0x1b07:
			scroll[7] = data;
			bankswitch(data);
		return;

		case 0x1c00:
		case 0x1c01:
		case 0x1c02:
		case 0x1c03:
			control_write(0, address);
		return;

		case 0x1c05:
		{
			if (data == 0x80) {
				M6809Close();
				M6809Open(1);
				M6809SetIRQLine(CPU_IRQLINE_NMI, CPU_IRQSTATUS_ACK);
				M6809Close();
				M6809Open(0);
			} else {
				soundlatch = data;
				M6502SetIRQLine(DECO16_IRQ_LINE, CPU_IRQSTATUS_AUTO);
			}
		}
		return;

		case 0x1c06: // banksel (not used)
		case 0x1c07: // swap?
		return;
	}
}

static UINT8 bwing_main_read(UINT16 address)
{
	if ((address & 0xff00) == 0x1a00) {
		return DrvPalRAM[(address & 0xff) * 2];
	}

	switch (address)
	{
		case 0x1b00:
			return DrvDips[0];

		case 0x1b01:
			return DrvDips[1];

		case 0x1b02:
			return DrvInputs[0];

		case 0x1b03:
			return DrvInputs[1];

		case 0x1b04:
			return (DrvInputs[2] & 0x7f) | (vblank ? 0x80 : 0);
	}

	return 0;
}

static void bwing_sub_write(UINT16 address, UINT8 )
{
	switch (address)
	{
		case 0x1800:
		case 0x1801:
		case 0x1802:
		case 0x1803:
			control_write(1, address);
		return;
	}
}

static UINT8 bwing_sound_read_port(UINT16 port)
{
	switch (port)
	{
		case 0x0000:
			return (vblank ? 0xff : 0);
	}

	return 0;
}

static void bwing_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x0200:
			DACWrite(0, data);
		return;

		case 0x1000:
			M6502SetIRQLine(CPU_IRQLINE_NMI, CPU_IRQSTATUS_NONE);
		return;

		case 0x2000:
			AY8910Write(0, 1, data);
		return;

		case 0x4000:
			AY8910Write(0, 0, data);
		return;

		case 0x6000:
			AY8910Write(1, 1, data);
		return;

		case 0x8000:
			AY8910Write(1, 0, data);
		return;
	
		case 0xd000:
			nmimask = data & 0x80;
		return;
	}
}

static UINT8 bwing_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xa000:
			return soundlatch;
	}

	return 0;
}

static tilemap_scan( bwing_scan_cols )
{
	return (row & 0xf) | ((col & 0xf) << 4) | ((row & 0x30) << 4) | ((col & 0x30) << 6);
}

static tilemap_callback( bglayer )
{
	TILE_SET_INFO(2, DrvBgRAM[offs] & 0x7f, DrvBgRAM[offs] >> 7, 0);
}

static tilemap_callback( fglayer )
{
	TILE_SET_INFO(1, DrvFgRAM[offs] & 0x7f, DrvFgRAM[offs] >> 7, 0);
}

static tilemap_callback( txlayer )
{
	TILE_SET_INFO(0, DrvVidRAM[offs], 0, 0);
}

static INT32 DrvDACSync()
{
	return (INT32)(float)(nBurnSoundLen * (M6502TotalCycles() / (2000000.0000 / (nBurnFPS / 100.0000))));
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);
	memset (DrvInputs, 0xff, 4);

	M6809Open(0);
	M6809Reset();
	M6809Close();

	M6809Open(1);
	M6809Reset();
	M6809Close();

	M6502Open(0);
	M6502Reset();
	DACReset();
	M6502Close();

	AY8910Reset(0);
	AY8910Reset(1);

	soundlatch = 0;
	nmimask = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6809ROM0	= Next; Next += 0x00c000;
	DrvM6809ROM1	= Next; Next += 0x006000;
	DrvM6502ROM		= Next; Next += 0x002000;

	DrvGfxROM0		= Next; Next += 0x004000;
	DrvGfxROM1		= Next; Next += 0x020000;
	DrvGfxROM2		= Next; Next += 0x010000;

	DrvPalette		= (UINT32*)Next; Next += 0x0041 * sizeof(UINT32);

	AllRam			= Next;

	DrvFgRAM		= Next; Next += 0x001000;
	DrvBgRAM		= Next; Next += 0x001000;
	DrvGfxRAM		= Next; Next += 0x006000;
	DrvVidRAM		= Next; Next += 0x000800;
	DrvSprRAM		= Next; Next += 0x000200;
	DrvPalRAM		= Next; Next += 0x000200;
	DrvShareRAM		= Next; Next += 0x000800;
	DrvM6809RAM0	= Next; Next += 0x000800;
	DrvM6809RAM1	= Next; Next += 0x000800;
	DrvM6502RAM		= Next; Next += 0x000200;

	scroll			= Next; Next += 0x000008;

	RamEnd			= Next;

	pAY8910Buffer[0]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[1]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[2]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[3]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[4]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[5]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[2] = { 0, 0x800*8 };
	INT32 Plane1[3] = { (0x4000*8*2), (0x4000*8*1), 0 };
	INT32 XOffs[16] = { STEP8(7,-1), STEP8(128+7, -1) };
	INT32 YOffs[16] = { STEP16(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0xc000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x1000);

	GfxDecode(0x0100, 2,  8,  8, Plane0, XOffs, YOffs, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0xc000);

	GfxDecode(0x0200, 3, 16, 16, Plane1, XOffs, YOffs, 0x100, tmp, DrvGfxROM1);

	BurnFree(tmp);

	return 0;
}

static void DrvSoundRomDecode()
{
	for (INT32 i = 0; i < 0x2000; i++) { // Swap nibbles
		DrvM6502ROM[i] = (DrvM6502ROM[i] << 4) | (DrvM6502ROM[i] >> 4);
	}

	// Move vectors to correct position. Weird!
	DrvM6502ROM[0x1ff4] = DrvM6502ROM[0x1ffb] = DrvM6502ROM[0x1ff6];
	DrvM6502ROM[0x1ff5] = DrvM6502ROM[0x1ffa] = DrvM6502ROM[0x1ff7];
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
		if (BurnLoadRom(DrvM6809ROM0 + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM0 + 0x4000,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM0 + 0x8000,  2, 1)) return 1;

		if (BurnLoadRom(DrvM6809ROM1 + 0x0000,  3, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM1 + 0x2000,  4, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM1 + 0x4000,  5, 1)) return 1;

		if (BurnLoadRom(DrvM6502ROM  + 0x0000,  6, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0   + 0x0000,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1   + 0x0000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1   + 0x4000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1   + 0x8000, 10, 1)) return 1;

		DrvSoundRomDecode();
		DrvGfxDecode();
	}

	M6809Init(2);
	M6809Open(0);
	M6809MapMemory(DrvShareRAM,		0x0000, 0x07ff, MAP_RAM);
	M6809MapMemory(DrvM6809RAM0,		0x0800, 0x0fff, MAP_RAM);
	M6809MapMemory(DrvVidRAM,		0x1000, 0x17ff, MAP_RAM);
	M6809MapMemory(DrvSprRAM,		0x1800, 0x19ff, MAP_RAM);
//	M6809MapMemory(DrvPalRAM,		0x1a00, 0x1aff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM0,		0x4000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(bwing_main_write);
	M6809SetReadHandler(bwing_main_read);
	M6809Close();

	M6809Open(1);
	M6809MapMemory(DrvShareRAM,		0x0000, 0x07ff, MAP_RAM);
	M6809MapMemory(DrvM6809RAM1,		0x0800, 0x0fff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM1,		0xa000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(bwing_sub_write);
	M6809Close();

	M6502Init(0, TYPE_DECO16);
	M6502Open(0);
	M6502MapMemory(DrvM6502RAM,		0x0000, 0x01ff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM,		0xe000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(bwing_sound_write);
	M6502SetReadHandler(bwing_sound_read);
	M6502SetReadPortHandler(bwing_sound_read_port);
	M6502Close();

	AY8910Init(0, 1500000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910Init(1, 1500000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.20, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.20, BURN_SND_ROUTE_BOTH);

	DACInit(0, 0, 1, DrvDACSync);
	DACSetRoute(0, 0.10, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	GenericTilemapInit(0, bwing_scan_cols_map_scan, bglayer_map_callback, 16, 16, 64, 64);
	GenericTilemapInit(1, bwing_scan_cols_map_scan, fglayer_map_callback, 16, 16, 64, 64);
	GenericTilemapInit(2, scan_cols_map_scan,       txlayer_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 2,  8,  8, 0x04000, 0x00, 0);
	GenericTilemapSetGfx(1, DrvGfxROM2 + 0x00000, 3, 16, 16, 0x8000, 0x10, 1);
	GenericTilemapSetGfx(2, DrvGfxROM2 + 0x08000, 3, 16, 16, 0x8000, 0x30, 1);
	GenericTilemapSetOffsets(0, 0, -8);
	GenericTilemapSetOffsets(1, 0, -8);
	GenericTilemapSetOffsets(2, 0, -8);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetTransparent(2, 0);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6809Exit();
	M6502Exit();

	AY8910Exit(0);
	AY8910Exit(1);
	DACExit();

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteUpdate()
{
	for (INT32 i = 1; i < 0x40; i++)
	{
		UINT8 r = ~DrvPalRAM[i*2] & 7;
		UINT8 g =(~DrvPalRAM[i*2] >> 4) & 7;
		UINT8 b = ~DrvPalRAM[i*2+1] & 7;

		r = (r << 5) + (r << 2) + (r >> 1);
		g = (g << 5) + (g << 2) + (g >> 1);
		b = (b << 5) + (b << 2) + (b >> 1);

		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}

	DrvPalette[0x40] = 0; // black (unused entry)
}

static INT32 DrvGfxRamDecode()
{
	INT32 Plane[3]  = { (0x2000*8*2), (0x2000*8*1), 0 };
	INT32 XOffs[16] = { STEP8(7,-1), STEP8(128+7, -1) };
	INT32 YOffs[16] = { STEP16(0,8) };

	GfxDecode(0x0100, 3, 16, 16, Plane, XOffs, YOffs, 0x100, DrvGfxRAM, DrvGfxROM2);

	return 0;
}

static void draw_sprites(INT32 pri)
{
	for (INT32 i = 0; i < 0x200; i += 4)
	{
		INT32 attr  = DrvSprRAM[i];
		INT32 code  = DrvSprRAM[i + 1];
		INT32 sy    = DrvSprRAM[i + 2];
		INT32 sx    = DrvSprRAM[i + 3];
		INT32 color = (attr >> 3) & 1;

		if (!(attr & 1) || color != pri)
			continue;

		code += (attr << 3) & 0x100;

		sy -= (attr << 1) & 0x100;
		sx -= (attr << 2) & 0x100;

		INT32 flipx = attr & 0x04;
		INT32 flipy = ~attr & 0x02;

		if (screen_flipx)
		{
			flipx = !flipx;
			flipy = !flipy;
			sx = 240 - sx;
			sy = 240 - sy;
		}

		if (attr & 0x10) {
			RenderZoomedTile(pTransDraw, DrvGfxROM1, code, (color<<3)+0x20, 0, sx, sy - 8, flipx, flipy, 16, 16, 1<<16, 2<<16);
		} else {
			if (flipy) {
				if (flipx) {
					Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy - 8, color, 3, 0, 0x20, DrvGfxROM1);
				} else {
					Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy - 8, color, 3, 0, 0x20, DrvGfxROM1);
				}
			} else {
				if (flipx) {
					Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy - 8, color, 3, 0, 0x20, DrvGfxROM1);
				} else {
					Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy - 8, color, 3, 0, 0x20, DrvGfxROM1);
				}
			}
		}
	}
}

static INT32 DrvDraw()
{
	//if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 0;
	//}

	screen_flipx =  scroll[7] & 0x20; // flipx = 0x20, flipy = 0

	INT32 xoffset = (screen_flipx) ? -8 : 8;

	GenericTilemapSetFlip(0, (screen_flipx ? TMAP_FLIPX : TMAP_FLIPY));
	GenericTilemapSetFlip(1, (screen_flipx ? TMAP_FLIPX : TMAP_FLIPY));
	GenericTilemapSetFlip(2, (screen_flipx ? TMAP_FLIPX : TMAP_FLIPY));

	if ((scroll[7] & 3) == 0) {
		DrvGfxRamDecode();
	}

	if ((scroll[7] & 0x01) == 0 && (nSpriteEnable & 1)) {
		GenericTilemapSetScrollX(0, (((scroll[1] << 2) & 0x300) + scroll[2] + xoffset) & 0x3ff);
		GenericTilemapSetScrollY(0, (((scroll[1] << 4) & 0x300) + scroll[3]) & 0x3ff);
		GenericTilemapDraw(0, pTransDraw, 0);
	} else {
		BurnTransferClear(); // black
	}

	if (nSpriteEnable & 2) {
		draw_sprites(0);
	}

	if ((scroll[7] & 0x02) == 0 && (nSpriteEnable & 4)) {
		GenericTilemapSetScrollX(1, (((scroll[1] << 6) & 0x300) + scroll[4] + xoffset) & 0x3ff);
		GenericTilemapSetScrollY(1, (((scroll[1] << 8) & 0x300) + scroll[5]) & 0x3ff);
		GenericTilemapDraw(1, pTransDraw, 0);
	}

	if (nSpriteEnable & 8) {
		draw_sprites(1);
	}

	if (nSpriteEnable & 0x10) {
		GenericTilemapDraw(2, pTransDraw, 0);
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	M6809NewFrame();
	M6502NewFrame();

	{
		UINT8 previous_coin = DrvInputs[2];
		UINT8 previous_tilt = DrvInputs[3];

		memset (DrvInputs, 0xff, 4);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}

		if ((previous_coin & 3) != (DrvInputs[2] & 0x03)) {
			M6809Open(0);
			M6809SetIRQLine(CPU_IRQLINE_NMI, ((DrvInputs[2] & 3) == 3) ? CPU_IRQSTATUS_NONE : CPU_IRQSTATUS_ACK);
			M6809Close();
		}

		if ((previous_tilt & 1) != (DrvInputs[3] & 0x01)) {
			M6809Open(0);
			M6809SetIRQLine(CPU_IRQLINE_FIRQ, ((DrvInputs[3] & 1) == 1) ? CPU_IRQSTATUS_NONE : CPU_IRQSTATUS_ACK);
			M6809Close();
		}	
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[3] = { 2000000 / 60, 2000000 / 60, 2000000 / 60 };
	INT32 nCyclesDone[3] = { 0, 0, 0 };
	INT32 nSoundBufferPos = 0;

	M6502Open(0);

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		M6809Open(0);
		nCyclesDone[0] += M6809Run(nCyclesTotal[0] / nInterleave);
		M6809Close();

		M6809Open(1);
		nCyclesDone[1] += M6809Run(nCyclesTotal[1] / nInterleave);
		M6809Close();

		nCyclesDone[2] += M6502Run(nCyclesTotal[2] / nInterleave);
		if ((i & 0xf)==0xf && nmimask == 0) M6502SetIRQLine(CPU_IRQLINE_NMI, CPU_IRQSTATUS_ACK);

		if (i == 240) vblank = 1;

		// Render Sound Segment
		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			AY8910Render(&pAY8910Buffer[0], pSoundBuf, nSegmentLength, 0);
			nSoundBufferPos += nSegmentLength;
		}
	}

	// Make sure the buffer is entirely filled.
	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			AY8910Render(&pAY8910Buffer[0], pSoundBuf, nSegmentLength, 0);
		}
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
	}

	M6502Close();

	if (pBurnDraw) {
		DrvDraw();
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
		M6809Scan(nAction);
		M6502Scan(nAction);

		AY8910Scan(nAction, pnMin);
		DACScan(nAction, pnMin);

		SCAN_VAR(soundlatch);
		SCAN_VAR(nmimask);
	}

	if (nAction & ACB_WRITE) {
		M6809Open(0);
		bankswitch(scroll[7]);
		M6809Close();
	}

	return 0;
}


// B-Wings (Japan new Ver.)

static struct BurnRomInfo bwingsRomDesc[] = {
	{ "bw_bv-02-.10a",	0x4000, 0x6074a86b, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "bw_bv-01.7a",	0x4000, 0xb960c707, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "bw_bv-00-.4a",	0x4000, 0x1f83804c, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "bw_bv-06-.10d",	0x2000, 0xeca00fcb, 2 | BRF_PRG | BRF_ESS }, //  3 M6809 #1 Code
	{ "bw_bv-05-.9d",	0x2000, 0x1e393300, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "bw_bv-04-.7d",	0x2000, 0x6548c5bb, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "bw_bv-03.13a",	0x2000, 0xe8ac9379, 3 | BRF_PRG | BRF_ESS }, //  6 M6502 Code

	{ "bw_bv-10.5c",	0x1000, 0xedca6901, 4 | BRF_GRA },           //  7 Characters

	{ "bw_bv-07.1l",	0x4000, 0x3d5ab2be, 5 | BRF_GRA },           //  8 Sprites
	{ "bw_bv-08.1k",	0x4000, 0x7a585f1e, 5 | BRF_GRA },           //  9
	{ "bw_bv-09.1h",	0x4000, 0xa14c0b57, 5 | BRF_GRA },           // 10
};

STD_ROM_PICK(bwings)
STD_ROM_FN(bwings)

struct BurnDriver BurnDrvBwings = {
	"bwings", NULL, NULL, NULL, "1984",
	"B-Wings (Japan new Ver.)\0", NULL, "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_PREFIX_DATAEAST, GBF_VERSHOOT, 0,
	NULL, bwingsRomInfo, bwingsRomName, NULL, NULL, BwingInputInfo, BwingDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// B-Wings (Japan old Ver.)

static struct BurnRomInfo bwingsoRomDesc[] = {
	{ "bw_bv-02.10a",	0x4000, 0x5ce74ab5, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "bw_bv-01.7a",	0x4000, 0xb960c707, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "bw_bv-00.4a",	0x4000, 0x926bef63, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "bw_bv-06.10d",	0x2000, 0x91a21a4c, 2 | BRF_PRG | BRF_ESS }, //  3 M6809 #1 Code
	{ "bw_bv-05.9d",	0x2000, 0xf283f39a, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "bw_bv-04.7d",	0x2000, 0x29ae75b6, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "bw_bv-03.13a",	0x2000, 0xe8ac9379, 3 | BRF_PRG | BRF_ESS }, //  6 M6502 Code

	{ "bw_bv-10.5c",	0x1000, 0xedca6901, 4 | BRF_GRA },           //  7 Characters

	{ "bw_bv-07.1l",	0x4000, 0x3d5ab2be, 5 | BRF_GRA },           //  8 Sprites
	{ "bw_bv-08.1k",	0x4000, 0x7a585f1e, 5 | BRF_GRA },           //  9
	{ "bw_bv-09.1h",	0x4000, 0xa14c0b57, 5 | BRF_GRA },           // 10
};

STD_ROM_PICK(bwingso)
STD_ROM_FN(bwingso)

struct BurnDriver BurnDrvBwingso = {
	"bwingso", "bwings", NULL, NULL, "1984",
	"B-Wings (Japan old Ver.)\0", NULL, "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_PREFIX_DATAEAST, GBF_VERSHOOT, 0,
	NULL, bwingsoRomInfo, bwingsoRomName, NULL, NULL, BwingInputInfo, BwingDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// B-Wings (Alt Ver.?)

static struct BurnRomInfo bwingsaRomDesc[] = {
	{ "bw_bv-02.10a",	0x4000, 0x5ce74ab5, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "bv02.bin",		0x2000, 0x2f84654e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "bw_bv-01.7a",	0x4000, 0xb960c707, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "bv00.bin",		0x4000, 0x0bbc1222, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "bw_bv-06.10d",	0x2000, 0x91a21a4c, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 #1 Code
	{ "bw_bv-05.9d",	0x2000, 0xf283f39a, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "bw_bv-04.7d",	0x2000, 0x29ae75b6, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "bw_bv-03.13a",	0x2000, 0xe8ac9379, 3 | BRF_PRG | BRF_ESS }, //  7 M6502 Code

	{ "bw_bv-10.5c",	0x1000, 0xedca6901, 4 | BRF_GRA },           //  8 Characters

	{ "bw_bv-07.1l",	0x4000, 0x3d5ab2be, 5 | BRF_GRA },           //  9 Sprites
	{ "bw_bv-08.1k",	0x4000, 0x7a585f1e, 5 | BRF_GRA },           // 10
	{ "bw_bv-09.1h",	0x4000, 0xa14c0b57, 5 | BRF_GRA },           // 11
};

STD_ROM_PICK(bwingsa)
STD_ROM_FN(bwingsa)

struct BurnDriver BurnDrvBwingsa = {
	"bwingsa", "bwings", NULL, NULL, "1984",
	"B-Wings (Alt Ver.?)\0", NULL, "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_PREFIX_DATAEAST, GBF_VERSHOOT, 0,
	NULL, bwingsaRomInfo, bwingsaRomName, NULL, NULL, BwingInputInfo, BwingDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Zaviga

static struct BurnRomInfo zavigaRomDesc[] = {
	{ "as04.10a",		0x4000, 0xb79f5da2, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "as02.7a",		0x4000, 0x6addd16a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "as00.4a",		0x4000, 0xc6ae4af0, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "as08.10d",		0x2000, 0xb6187b3a, 2 | BRF_PRG | BRF_ESS }, //  3 M6809 #1 Code
	{ "as07.9d",		0x2000, 0xdc1170e3, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "as06.7d",		0x2000, 0xba888f84, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "as05.13a",		0x2000, 0xafe9b0ac, 3 | BRF_PRG | BRF_ESS }, //  6 M6502 Code

	{ "as14.5c",		0x1000, 0x62132c1d, 4 | BRF_GRA },           //  7 Characters

	{ "as11.1l",		0x4000, 0xaa84af24, 5 | BRF_GRA },           //  8 Sprites
	{ "as12.1k",		0x4000, 0x84af9041, 5 | BRF_GRA },           //  9
	{ "as13.1h",		0x4000, 0x15d0922b, 5 | BRF_GRA },           // 10
};

STD_ROM_PICK(zaviga)
STD_ROM_FN(zaviga)

struct BurnDriver BurnDrvZaviga = {
	"zaviga", NULL, NULL, NULL, "1984",
	"Zaviga\0", NULL, "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_PREFIX_DATAEAST, GBF_VERSHOOT, 0,
	NULL, zavigaRomInfo, zavigaRomName, NULL, NULL, BwingInputInfo, BwingDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Zaviga (Japan)

static struct BurnRomInfo zavigajRomDesc[] = {
	{ "as04.10a",		0x4000, 0xb79f5da2, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "as02.7a",		0x4000, 0x6addd16a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "as00.4a",		0x4000, 0xc6ae4af0, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "as08.10d",		0x2000, 0xb6187b3a, 2 | BRF_PRG | BRF_ESS }, //  3 M6809 #1 Code
	{ "as07.9d",		0x2000, 0xdc1170e3, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "as06-.7d",		0x2000, 0xb02d270c, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "as05.13a",		0x2000, 0xafe9b0ac, 3 | BRF_PRG | BRF_ESS }, //  6 M6502 Code

	{ "as14.5c",		0x1000, 0x62132c1d, 4 | BRF_GRA },           //  7 Characters

	{ "as11.1l",		0x4000, 0xaa84af24, 5 | BRF_GRA },           //  8 Sprites
	{ "as12.1k",		0x4000, 0x84af9041, 5 | BRF_GRA },           //  9
	{ "as13.1h",		0x4000, 0x15d0922b, 5 | BRF_GRA },           // 10
};

STD_ROM_PICK(zavigaj)
STD_ROM_FN(zavigaj)

struct BurnDriver BurnDrvZavigaj = {
	"zavigaj", "zaviga", NULL, NULL, "1984",
	"Zaviga (Japan)\0", NULL, "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_PREFIX_DATAEAST, GBF_VERSHOOT, 0,
	NULL, zavigajRomInfo, zavigajRomName, NULL, NULL, BwingInputInfo, BwingDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};
