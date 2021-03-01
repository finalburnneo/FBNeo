// FinalBurn Neo Baraduke driver module
// Based on MAME driver by Manuel Abadia and various others

#include "tiles_generic.h"
#include "m6800_intf.h"
#include "m6809_intf.h"
#include "namco_snd.h"
#include "burn_led.h"
#include "watchdog.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6809ROM;
static UINT8 *DrvHD63701ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvColPROM;
static UINT8 *DrvHD63701RAM1;
static UINT8 *DrvHD63701RAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvTxtRAM;
static UINT8 *DrvSprRAM;

static UINT32 *DrvPalette;
static UINT8  DrvRecalc;

static UINT8 *flipscreen;
static UINT8 *buffer_sprites;
static UINT8 *coin_lockout;
static UINT8 *scroll;
static UINT8 *ip_select;
static INT32 *kludge1105;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[3];
static UINT8 DrvReset;
static UINT8 DrvInputs[8];

static struct BurnInputInfo BaradukeInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 2,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 0,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Baraduke)

static struct BurnDIPInfo BaradukeDIPList[]=
{
	{0x10, 0xff, 0xff, 0xff, NULL					},
	{0x11, 0xff, 0xff, 0xff, NULL					},
	{0x12, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x10, 0x01, 0x03, 0x00, "3 Coins 1 Credits"	},
	{0x10, 0x01, 0x03, 0x01, "2 Coins 1 Credits"	},
	{0x10, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x10, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x10, 0x01, 0x04, 0x00, "Off"					},
	{0x10, 0x01, 0x04, 0x04, "On"					},

	{0   , 0xfe, 0   ,    4, "Coin A"				},
	{0x10, 0x01, 0x18, 0x00, "3 Coins 1 Credits"	},
	{0x10, 0x01, 0x18, 0x08, "2 Coins 1 Credits"	},
	{0x10, 0x01, 0x18, 0x18, "1 Coin  1 Credits"	},
	{0x10, 0x01, 0x18, 0x10, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x10, 0x01, 0x60, 0x40, "2"					},
	{0x10, 0x01, 0x60, 0x60, "3"					},
	{0x10, 0x01, 0x60, 0x20, "4"					},
	{0x10, 0x01, 0x60, 0x00, "5"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x10, 0x01, 0x80, 0x80, "Off"					},
	{0x10, 0x01, 0x80, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x11, 0x01, 0x02, 0x02, "Off"					},
	{0x11, 0x01, 0x02, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Freeze"				},
	{0x11, 0x01, 0x04, 0x04, "Off"					},
	{0x11, 0x01, 0x04, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Round Select"			},
	{0x11, 0x01, 0x08, 0x08, "Off"					},
	{0x11, 0x01, 0x08, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x11, 0x01, 0x30, 0x20, "Easy"					},
	{0x11, 0x01, 0x30, 0x30, "Normal"				},
	{0x11, 0x01, 0x30, 0x10, "Hard"					},
	{0x11, 0x01, 0x30, 0x00, "Very Hard"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x11, 0x01, 0xc0, 0x80, "Every 10k"			},
	{0x11, 0x01, 0xc0, 0xc0, "10k And Every 20k"	},
	{0x11, 0x01, 0xc0, 0x40, "Every 20k"			},
	{0x11, 0x01, 0xc0, 0x00, "None"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x12, 0x01, 0x02, 0x02, "Upright"				},
	{0x12, 0x01, 0x02, 0x00, "Cocktail"				},
};

STDDIPINFO(Baraduke)

static struct BurnDIPInfo MetrocrsDIPList[]=
{
	{0x10, 0xff, 0xff, 0xff, NULL					},
	{0x11, 0xff, 0xff, 0xff, NULL					},
	{0x12, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x10, 0x01, 0x03, 0x00, "3 Coins 1 Credits"	},
	{0x10, 0x01, 0x03, 0x01, "2 Coins 1 Credits"	},
	{0x10, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x10, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x10, 0x01, 0x04, 0x00, "No"					},
	{0x10, 0x01, 0x04, 0x04, "Yes"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x10, 0x01, 0x18, 0x10, "Easy"					},
	{0x10, 0x01, 0x18, 0x18, "Normal"				},
	{0x10, 0x01, 0x18, 0x08, "Hard"					},
	{0x10, 0x01, 0x18, 0x00, "Very Hard"			},

	{0   , 0xfe, 0   ,    4, "Coin A"				},
	{0x10, 0x01, 0x60, 0x00, "3 Coins 1 Credits"	},
	{0x10, 0x01, 0x60, 0x20, "2 Coins 1 Credits"	},
	{0x10, 0x01, 0x60, 0x60, "1 Coin  1 Credits"	},
	{0x10, 0x01, 0x60, 0x40, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x10, 0x01, 0x80, 0x80, "Off"					},
	{0x10, 0x01, 0x80, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Freeze"				},
	{0x11, 0x01, 0x20, 0x20, "Off"					},
	{0x11, 0x01, 0x20, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Round Select"			},
	{0x11, 0x01, 0x40, 0x40, "Off"					},
	{0x11, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x11, 0x01, 0x80, 0x00, "Off"					},
	{0x11, 0x01, 0x80, 0x80, "On"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x12, 0x01, 0x02, 0x02, "Upright"				},
	{0x12, 0x01, 0x02, 0x00, "Cocktail"				},
};

STDDIPINFO(Metrocrs)

static void baraduke_main_write(UINT16 address, UINT8 data)
{
	if (address < 0x2000) {
		DrvSprRAM[address & 0x1fff] = data;
		if (address == 0x1ff2) buffer_sprites[0] = 1;
		return;
	}

	if ((address & 0xfc00) == 0x4000) {
		namcos1_custom30_write(address & 0x3ff, data);
		return;
	}

	switch (address)
	{
		case 0x8000:
			BurnWatchdogWrite();
		return;

		case 0x8800:
			M6809SetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;

		case 0xb000:
		case 0xb001:
		case 0xb002:
		case 0xb004:
		case 0xb005:
		case 0xb006:
			scroll[address & 0x07] = data;
		return;
	}
}

static UINT8 baraduke_main_read(UINT16 address)
{
	if ((address & 0xfc00) == 0x4000) {
		return namcos1_custom30_read(address & 0x3ff);
	}

	return 0;
}

static void baraduke_mcu_write(UINT16 address, UINT8 data)
{
	if ((address & 0xffe0) == 0x0000) {
		m6803_internal_registers_w(address & 0x1f, data);
		return;
	}

	if ((address & 0xff80) == 0x0080) {
		DrvHD63701RAM1[address & 0x7f] = data;
		return;
	}

	if ((address & 0xfc00) == 0x1000) {
		namcos1_custom30_write(address & 0x3ff, data);
		return;
	}
}

static UINT8 baraduke_mcu_read(UINT16 address)
{
	if ((address & 0xffe0) == 0x0000) {
		return m6803_internal_registers_r(address & 0x1f);
	}

	if ((address & 0xff80) == 0x0080) {
		return DrvHD63701RAM1[address & 0x7f];
	}

	if (address == 0x1105) {
		return ((*kludge1105)++ >> 4) & 0xff;
	}

	if ((address & 0xfc00) == 0x1000) {
		return namcos1_custom30_read(address & 0x3ff);
	}

	return 0;
}

static void baraduke_mcu_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0x1ff)
	{
		case HD63701_PORT1:
		{
			switch (data & 0xe0) {
				case 0x60:
					*ip_select = data & 0x07;
				return;

				case 0xc0:
					*coin_lockout = ~data & 0x01;
					// coin counters 0 -> data & 0x02, 1 -> data & 0x04
				return;
			}
		}
		return;

		case HD63701_PORT2:
			BurnLEDSetStatus(0, data & 0x08);
			BurnLEDSetStatus(1, data & 0x10);
		return;
	}
}

static UINT8 baraduke_mcu_read_port(UINT16 port)
{
	switch (port & 0x1ff)
	{
		case HD63701_PORT1:
			return DrvInputs[*ip_select];

		case HD63701_PORT2:
			return 0xff;
	}

	return 0;
}

static tilemap_scan( fg )
{
	INT32 offs = 0;

	row += 2;
	col -= 2;
	if (col & 0x20) {
		offs = row + ((col & 0x1f) << 5);
	} else {
		offs = col + (row << 5);
	}

	return offs;
}

static tilemap_callback( fg )
{
	INT32 code  = DrvTxtRAM[offs + 0x0000];
	INT32 color = DrvTxtRAM[offs + 0x0400] & 0x7f;

	TILE_SET_INFO(2, code, color, 0);
}

static tilemap_callback( bg0 )
{
	INT32 attr = DrvVidRAM[offs * 2 + 0x0001];
	INT32 code = DrvVidRAM[offs * 2 + 0x0000] + (attr << 8);

	TILE_SET_INFO(0, code, attr, 0);
}

static tilemap_callback( bg1 )
{
	INT32 attr = DrvVidRAM[offs * 2 + 0x1001];
	INT32 code = DrvVidRAM[offs * 2 + 0x1000] + (attr << 8);

	TILE_SET_INFO(1, code, attr, 0);
}

static INT32 DrvDoReset(INT32 ClearRAM)
{
	if (ClearRAM) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	M6809Open(0);
	M6809Reset();
	M6809Close();

	HD63701Open(0);
	HD63701Reset();
	HD63701Close();

	BurnWatchdogReset();

	NamcoSoundReset();

	BurnLEDReset();

	BurnLEDSetFlipscreen(1);

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6809ROM		= Next; Next += 0x010000;
	DrvHD63701ROM	= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += 0x008000;
	DrvGfxROM1		= Next; Next += 0x020000;
	DrvGfxROM2		= Next; Next += 0x020000;

	DrvColPROM		= Next; Next += 0x001000;

	DrvPalette		= (UINT32*)Next; Next += 0x0800 * sizeof(UINT32);

	AllRam			= Next;

	DrvHD63701RAM1	= Next; Next += 0x000080;
	DrvHD63701RAM	= Next; Next += 0x000800;

	DrvVidRAM		= Next; Next += 0x002000;
	DrvTxtRAM		= Next; Next += 0x000800;
	DrvSprRAM		= Next; Next += 0x002000;

	kludge1105      = (INT32*)Next; Next += 0x000004;
	coin_lockout	= Next; Next += 0x000001;
	ip_select		= Next; Next += 0x000001;
	buffer_sprites	= Next; Next += 0x000001;
	flipscreen		= Next; Next += 0x000001;

	scroll			= Next; Next += 0x000008;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void DrvGfxExpand()
{
	UINT8 *rom = DrvGfxROM1 + 0x8000;
	for (INT32 i = 0x2000; i < 0x4000; i++)
	{
		rom[i + 0x2000] = rom[i]; // c000
		rom[i + 0x4000] = rom[i] << 4; // e000
	}
	for (INT32 i = 0; i < 0x2000; i++)
	{
		rom[i + 0x2000] = rom[i] << 4; // a000
	}
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[2]  = { 0, 4 };
	INT32 Plane1[3]  = { 0x8000 * 8, 0, 4 };
	INT32 Plane2[4]  = { STEP4(0,1) };
	INT32 XOffs0[8]  = { STEP4(64,1), STEP4(0,1) };
	INT32 XOffs1[8]  = { STEP4(0,1), STEP4(8,1) };
	INT32 XOffs2[16] = { STEP16(0,4) };
	INT32 YOffs0[8]  = { STEP8(0,8) };
	INT32 YOffs1[8]  = { STEP8(0,16) };
	INT32 YOffs2[16] = { STEP16(0,64) }; 

	UINT8 *tmp = (UINT8*)BurnMalloc(0x10000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x02000);

	GfxDecode(0x0200, 2,  8,  8, Plane0, XOffs0, YOffs0, 0x080, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x10000);

	GfxDecode(0x0400, 3,  8,  8, Plane1, XOffs1, YOffs1, 0x080, tmp + 0x0000, DrvGfxROM1 + 0x00000);
	GfxDecode(0x0400, 3,  8,  8, Plane1, XOffs1, YOffs1, 0x080, tmp + 0x4000, DrvGfxROM1 + 0x10000);

	memcpy (tmp, DrvGfxROM2, 0x10000);

	GfxDecode(0x0200, 4, 16, 16, Plane2, XOffs2, YOffs2, 0x400, tmp, DrvGfxROM2);

	BurnFree (tmp);

	return 0;
}

static INT32 DrvInit(INT32 type)
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvM6809ROM    + 0x06000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM    + 0x08000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM    + 0x0c000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvHD63701ROM  + 0x08000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvHD63701ROM  + 0x0f000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0     + 0x00000,  k++, 1)) return 1;

		memset (DrvGfxROM1         	   + 0x00000,  0xff, 0xc000);
		if (BurnLoadRom(DrvGfxROM1     + 0x00000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1     + 0x04000,  k++, 1)) return 1;

		if (type == 0) {
			if (BurnLoadRom(DrvGfxROM1 + 0x08000,  k++, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM2 + 0x00000,  k++, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x04000,  k++, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x08000,  k++, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x0c000,  k++, 1)) return 1;
		} else {
			if (BurnLoadRom(DrvGfxROM2 + 0x00000,  k++, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x04000,  k++, 1)) return 1;
		}

		if (BurnLoadRom(DrvColPROM     + 0x00000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM     + 0x00800,  k++, 1)) return 1;

		DrvGfxExpand();
		DrvGfxDecode();
	}

	M6809Init(0);
	M6809Open(0);
	M6809MapMemory(DrvSprRAM,					0x0000, 0x1fff, MAP_READ | MAP_FETCH);
	M6809MapMemory(DrvSprRAM,					0x0000, 0x1eff, MAP_WRITE);
	M6809MapMemory(DrvVidRAM,					0x2000, 0x3fff, MAP_RAM);
	M6809MapMemory(DrvTxtRAM,					0x4800, 0x4fff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM + 0x06000,		0x6000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(baraduke_main_write);
	M6809SetReadHandler(baraduke_main_read);
	M6809Close();

	HD63701Init(0);
	HD63701Open(0);
	HD63701MapMemory(DrvHD63701ROM + 0x8000,	0x8000, 0xbfff, MAP_ROM);
	HD63701MapMemory(DrvHD63701RAM,				0xc000, 0xc7ff, MAP_RAM);
	HD63701MapMemory(DrvHD63701ROM + 0xf000,	0xf000, 0xffff, MAP_ROM);
	HD63701SetReadHandler(baraduke_mcu_read);
	HD63701SetWriteHandler(baraduke_mcu_write);
	HD63701SetReadPortHandler(baraduke_mcu_read_port);
	HD63701SetWritePortHandler(baraduke_mcu_write_port);
	HD63701Close();

	BurnWatchdogInit(DrvDoReset, 180);

	NamcoSoundInit(49152000/2048, 8, 0);
	NamcoSoundSetAllRoutes(0.50, BURN_SND_ROUTE_BOTH);
	NamcoSoundSetBuffered(HD63701TotalCycles, 1536000);

	BurnLEDInit(2, LED_POSITION_BOTTOM_RIGHT, LED_SIZE_5x5, LED_COLOR_GREEN, 100);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg0_map_callback, 8, 8, 64, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, bg1_map_callback, 8, 8, 64, 32);
	GenericTilemapInit(2, fg_map_scan,       fg_map_callback,  8, 8, 36, 28);
	GenericTilemapSetGfx(0, DrvGfxROM1 + 0x00000, 3,  8,  8, 0x10000, 0, 0xff);
	GenericTilemapSetGfx(1, DrvGfxROM1 + 0x10000, 3,  8,  8, 0x10000, 0, 0xff);
	GenericTilemapSetGfx(2, DrvGfxROM0 + 0x00000, 4,  8,  8, 0x08000, 0, 0x7f); // 2bpp, but use 4bpp as depth
	GenericTilemapSetGfx(3, DrvGfxROM2 + 0x00000, 4, 16, 16, 0x20000, 0, 0x7f);
	GenericTilemapSetTransparent(0, 7);
	GenericTilemapSetTransparent(1, 7);
	GenericTilemapSetTransparent(2, 3);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	BurnLEDExit();

	NamcoSoundExit();

	M6809Exit();
	HD63701Exit();

	BurnFreeMemIndex();

	NamcoSoundProm = NULL;

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x800; i++)
	{
		INT32 bit0 = (DrvColPROM[0x800 + i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[0x800 + i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[0x800 + i] >> 2) & 0x01;
		INT32 bit3 = (DrvColPROM[0x800 + i] >> 3) & 0x01;
		INT32 r = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[0x000 + i] >> 0) & 0x01;
		bit1 = (DrvColPROM[0x000 + i] >> 1) & 0x01;
		bit2 = (DrvColPROM[0x000 + i] >> 2) & 0x01;
		bit3 = (DrvColPROM[0x000 + i] >> 3) & 0x01;
		INT32 g = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[0x000 + i] >> 4) & 0x01;
		bit1 = (DrvColPROM[0x000 + i] >> 5) & 0x01;
		bit2 = (DrvColPROM[0x000 + i] >> 6) & 0x01;
		bit3 = (DrvColPROM[0x000 + i] >> 7) & 0x01;
		INT32 b = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_sprites(INT32 prio)
{
	UINT8 *src = DrvSprRAM + 0x1800;
	INT32 sprite_xoffs = src[0x07f5] - ((src[0x07f4] & 0x01) << 8);
	INT32 sprite_yoffs = src[0x07f7];

	for (INT32 offs = 0; offs < 0x800 - 16; offs += 16, src += 16)
	{
		INT32 priority = src[10] & 0x01;

		if (priority == prio)
		{
			INT32 attr1  = src[10];
			INT32 sprite = src[11] << 2;
			INT32 color  = src[12];
			INT32 sx     = src[13] + ((color & 0x01) << 8) + sprite_xoffs;
			INT32 attr2  = src[14];
			INT32 sy     = (240 - src[15]) - sprite_yoffs;
			INT32 flipx  = (attr1 & 0x20) >> 5;
			INT32 flipy  = (attr2 & 0x01) >> 0;
			INT32 sizex  = (attr1 & 0x80) >> 7;
			INT32 sizey  = (attr2 & 0x04) >> 2;

			if ((attr1 & 0x10) && !sizex) sprite += 1;
			if ((attr2 & 0x10) && !sizey) sprite += 2;

			sy -= 16 * sizey;

			if (*flipscreen)
			{
				sx = 499 - 16 * sizex - sx;
				sy =(240 - 16 * sizey - sy) ;
				flipx ^= 1;
				flipy ^= 1;
			}

			for (INT32 y = 0; y <= sizey; y++)
			{
				for (INT32 x = 0; x <= sizex; x++)
				{
					INT32 code = (sprite + (y ^ (sizey * flipy)) * 2 + (x ^ (sizex * flipx))) & 0x1ff;

					DrawGfxMaskTile(0, 3, code, ((sx + 16 * x) & 0x1ff) - 71, ((sy + 16 * y) & 0xff) + 1 + (*flipscreen ? -32 : 0), flipx, flipy, color >> 1, 0xf);
				}
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	*flipscreen = DrvSprRAM[0x1ff6] & 0x01;

	BurnLEDSetFlipscreen(*flipscreen);
	GenericTilemapSetFlip(TMAP_GLOBAL, *flipscreen ? (TMAP_FLIPX|TMAP_FLIPY) : 0);

	INT32 priority = ((scroll[0] & 0x0e) == 0x0c) ? 1 : 0;
	
	INT32 scrollx0 = scroll[1] + ((scroll[0] & 0x01) << 8);
	INT32 scrollx1 = scroll[5] + ((scroll[4] & 0x01) << 8);
	INT32 scrolly0 = scroll[2];
	INT32 scrolly1 = scroll[6];

	GenericTilemapSetScrollX(0, *flipscreen ? -(scrollx0 - 227+26) : (scrollx0+26));
	GenericTilemapSetScrollX(1, *flipscreen ? -(scrollx1 - 227+24) : (scrollx1+24));
	GenericTilemapSetScrollY(0, *flipscreen ? -(scrolly0 - 9 + 16) : (scrolly0+9));
	GenericTilemapSetScrollY(1, *flipscreen ? -(scrolly1 - 9 + 16) : (scrolly1+9));

	if (~nBurnLayer & 1) BurnTransferClear();

	if ( nBurnLayer & 1) GenericTilemapDraw(0 ^ priority, 0, TMAP_DRAWOPAQUE);

	if ( nSpriteEnable & 1) draw_sprites(0);

	if ( nBurnLayer & 2) GenericTilemapDraw(1 ^ priority, 0, 0);

	if ( nSpriteEnable & 2) draw_sprites(1);

	if ( nBurnLayer & 4) GenericTilemapDraw(2, 0, 0);

	BurnTransferCopy(DrvPalette);

	BurnLEDRender();

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	{
		memset (DrvInputs, 0xff, 8);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[4] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[5] ^= (DrvJoy3[i] & 1) << i;
 			DrvInputs[6] ^= (DrvJoy2[i] & 1) << i;
		}

		DrvInputs[0] = ((DrvDips[0] & 0xf8) >> 3);
		DrvInputs[1] = ((DrvDips[0] & 0x07) << 2) | ((DrvDips[1] & 0xc0) >> 6);
		DrvInputs[2] = ((DrvDips[1] & 0x3e) >> 1);
		DrvInputs[3] =  (DrvDips[2] & 0x0f) | ((DrvDips[0] & 0x01) << 4);

		if (*coin_lockout) DrvInputs[4] |= 0x06;
	}

	M6809NewFrame();
	HD63701NewFrame();

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 1536000 / 60, 1536000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	M6809Open(0);
	HD63701Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, M6809);
		if (i == (nInterleave - 1)) {
			M6809SetIRQLine(0, CPU_IRQSTATUS_ACK);
		}

		CPU_RUN(1, HD63701);
		if (i == (nInterleave - 1)) {
			HD63701SetIRQLine(0, CPU_IRQSTATUS_AUTO);
		}
	}

	HD63701Close();
	M6809Close();

	if (pBurnSoundOut) {
		NamcoSoundUpdate(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	{
		if (buffer_sprites[0] == 0) return 0;

		for (INT32 i = 0; i < 0x800; i += 16) {
			memcpy (DrvSprRAM + 0x180a + i, DrvSprRAM + 0x1804 + i, 6);
		}

		buffer_sprites[0] = 0;
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029707;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		M6809Scan(nAction);
		HD63701Scan(nAction);

		BurnWatchdogScan(nAction);

		NamcoSoundScan(nAction, pnMin);

		BurnLEDScan(nAction, pnMin);
	}

	return 0;
}


// Alien Sector

static struct BurnRomInfo aliensecRomDesc[] = {
	{ "bd1_3.9c",		0x2000, 0xea2ea790, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "bd2_1.9a",		0x4000, 0x9a0a9a87, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "bd2_2.9b",		0x4000, 0x383e5458, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "bd1_4.3b",		0x4000, 0xabda0fe7, 2 | BRF_PRG | BRF_ESS }, //  3 HD63701 Code
	{ "cus60-60a1.mcu",	0x1000, 0x076ea82a, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "bd1_5.3j",		0x2000, 0x706b7fee, 3 | BRF_GRA },           //  5 Characters

	{ "bd2_8.4p",		0x4000, 0x432bd7d9, 4 | BRF_GRA },           //  6 Background Tiles
	{ "bd1_7.4n",		0x4000, 0x0d7ebec9, 4 | BRF_GRA },           //  7
	{ "bd2_6.4m",		0x4000, 0xf4c1df60, 4 | BRF_GRA },           //  8

	{ "bd1_9.8k",		0x4000, 0x87a29acc, 5 | BRF_GRA },           //  9 Sprites
	{ "bd1_10.8l",		0x4000, 0x72b6d20c, 5 | BRF_GRA },           // 10
	{ "bd1_11.8m",		0x4000, 0x3076af9c, 5 | BRF_GRA },           // 11
	{ "bd1_12.8n",		0x4000, 0x8b4c09a3, 5 | BRF_GRA },           // 12

	{ "bd1-1.1n",		0x0800, 0x0d78ebc6, 6 | BRF_GRA },           // 13 Color PROMs
	{ "bd1-2.2m",		0x0800, 0x03f7241f, 6 | BRF_GRA },           // 14
};

STD_ROM_PICK(aliensec)
STD_ROM_FN(aliensec)

static INT32 AlienInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvAliensec = {
	"aliensec", NULL, NULL, NULL, "1985",
	"Alien Sector\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, aliensecRomInfo, aliensecRomName, NULL, NULL, NULL, NULL, BaradukeInputInfo, BaradukeDIPInfo,
	AlienInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	288, 224, 4, 3
};


// Baraduke

static struct BurnRomInfo baradukeRomDesc[] = {
	{ "bd1_3.9c",		0x2000, 0xea2ea790, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "bd1_1.9a",		0x4000, 0x4e9f2bdc, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "bd1_2.9b",		0x4000, 0x40617fcd, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "bd1_4b.3b",		0x4000, 0xa47ecd32, 2 | BRF_PRG | BRF_ESS }, //  3 HD63701 Code
	{ "cus60-60a1.mcu",	0x1000, 0x076ea82a, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "bd1_5.3j",		0x2000, 0x706b7fee, 3 | BRF_GRA },           //  5 Characters

	{ "bd1_8.4p",		0x4000, 0xb0bb0710, 4 | BRF_GRA },           //  6 Background Tiles
	{ "bd1_7.4n",		0x4000, 0x0d7ebec9, 4 | BRF_GRA },           //  7
	{ "bd1_6.4m",		0x4000, 0xe5da0896, 4 | BRF_GRA },           //  8

	{ "bd1_9.8k",		0x4000, 0x87a29acc, 5 | BRF_GRA },           //  9 Sprites
	{ "bd1_10.8l",		0x4000, 0x72b6d20c, 5 | BRF_GRA },           // 10
	{ "bd1_11.8m",		0x4000, 0x3076af9c, 5 | BRF_GRA },           // 11
	{ "bd1_12.8n",		0x4000, 0x8b4c09a3, 5 | BRF_GRA },           // 12

	{ "bd1-1.1n",		0x0800, 0x0d78ebc6, 6 | BRF_GRA },           // 13 Color PROMs
	{ "bd1-2.2m",		0x0800, 0x03f7241f, 6 | BRF_GRA },           // 14
};

STD_ROM_PICK(baraduke)
STD_ROM_FN(baraduke)

struct BurnDriver BurnDrvBaraduke = {
	"baraduke", "aliensec", NULL, NULL, "1985",
	"Baraduke\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, baradukeRomInfo, baradukeRomName, NULL, NULL, NULL, NULL, BaradukeInputInfo, BaradukeDIPInfo,
	AlienInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	288, 224, 4, 3
};


// Metro-Cross (set 1)

static struct BurnRomInfo metrocrsRomDesc[] = {
	{ "mc1-3.9c",		0x2000, 0x3390b33c, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "mc1-1.9a",		0x4000, 0x10b0977e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mc1-2.9b",		0x4000, 0x5c846f35, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "mc1-4.3b",		0x2000, 0x9c88f898, 2 | BRF_PRG | BRF_ESS }, //  3 HD63701 Code
	{ "cus60-60a1.mcu",	0x1000, 0x076ea82a, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "mc1-5.3j",		0x2000, 0x9b5ea33a, 3 | BRF_GRA },           //  5 Characters

	{ "mc1-7.4p",		0x4000, 0xc9dfa003, 4 | BRF_GRA },           //  6 Background Tiles
	{ "mc1-6.4n",		0x4000, 0x9686dc3c, 4 | BRF_GRA },           //  7

	{ "mc1-8.8k",		0x4000, 0x265b31fa, 5 | BRF_GRA },           //  8 Sprites
	{ "mc1-9.8l",		0x4000, 0x541ec029, 5 | BRF_GRA },           //  9

	{ "mc1-1.1n",		0x0800, 0x32a78a8b, 6 | BRF_GRA },           // 10 Color PROMs
	{ "mc1-2.2m",		0x0800, 0x6f4dca7b, 6 | BRF_GRA },           // 11
};

STD_ROM_PICK(metrocrs)
STD_ROM_FN(metrocrs)

static INT32 MetroInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvMetrocrs = {
	"metrocrs", NULL, NULL, NULL, "1985",
	"Metro-Cross (set 1)\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, metrocrsRomInfo, metrocrsRomName, NULL, NULL, NULL, NULL, BaradukeInputInfo, MetrocrsDIPInfo,
	MetroInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	288, 224, 4, 3
};


// Metro-Cross (set 2)

static struct BurnRomInfo metrocrsaRomDesc[] = {
	{ "mc2-3.9b",		0x2000, 0xffe08075, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "mc2-1.9a",		0x4000, 0x05a239ea, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mc2-2.9a",		0x4000, 0xdb9b0e6d, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "mc1-4.3b",		0x2000, 0x9c88f898, 2 | BRF_PRG | BRF_ESS }, //  3 HD63701 Code
	{ "cus60-60a1.mcu",	0x1000, 0x076ea82a, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "mc1-5.3j",		0x2000, 0x9b5ea33a, 3 | BRF_GRA },           //  5 Characters

	{ "mc1-7.4p",		0x4000, 0xc9dfa003, 4 | BRF_GRA },           //  6 Background Tiles
	{ "mc1-6.4n",		0x4000, 0x9686dc3c, 4 | BRF_GRA },           //  7

	{ "mc1-8.8k",		0x4000, 0x265b31fa, 5 | BRF_GRA },           //  8 Sprites
	{ "mc1-9.8l",		0x4000, 0x541ec029, 5 | BRF_GRA },           //  9

	{ "mc1-1.1n",		0x0800, 0x32a78a8b, 6 | BRF_GRA },           // 10 Color PROMs
	{ "mc1-2.2m",		0x0800, 0x6f4dca7b, 6 | BRF_GRA },           // 11
};

STD_ROM_PICK(metrocrsa)
STD_ROM_FN(metrocrsa)

struct BurnDriver BurnDrvMetrocrsa = {
	"metrocrsa", "metrocrs", NULL, NULL, "1985",
	"Metro-Cross (set 2)\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, metrocrsaRomInfo, metrocrsaRomName, NULL, NULL, NULL, NULL, BaradukeInputInfo, MetrocrsDIPInfo,
	MetroInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	288, 224, 4, 3
};
