// FB Alpha Double Dribble driver module
// Based on MAME driver by Manuel Abadia

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "burn_ym2203.h"
#include "vlm5030.h"
#include "flt_rc.h"

static UINT8 *AllMem;
static UINT8 *RamEnd;
static UINT8 *AllRam;
static UINT8 *MemEnd;
static UINT8 *DrvM6809ROM0;
static UINT8 *DrvM6809ROM1;
static UINT8 *DrvM6809ROM2;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvSndROM;
static UINT8 *DrvColPROM;
static UINT8 *DrvBgRAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvShareRAM;
static UINT8 *DrvSndRAM;
static UINT8 *DrvSprRAM0;
static UINT8 *DrvSprRAM1;
static UINT8 *DrvVidRegs[2];

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 nExtraCycles[2];

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[9];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static UINT8 bankdata;
static INT32 watchdog;
static INT32 charbank0;
static INT32 charbank1;

static struct BurnInputInfo DdribbleInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Ddribble)

static struct BurnDIPInfo DdribbleDIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL				},
	{0x15, 0xff, 0xff, 0x20, NULL				},
	{0x16, 0xff, 0xff, 0xfd, NULL				},

	{0   , 0xfe, 0   ,   16, "Coin A"			},
	{0x14, 0x01, 0x0f, 0x02, "4 Coins 1 Credit"		},
	{0x14, 0x01, 0x0f, 0x05, "3 Coins 1 Credit"		},
	{0x14, 0x01, 0x0f, 0x08, "2 Coins 1 Credit"		},
	{0x14, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"		},
	{0x14, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"		},
	{0x14, 0x01, 0x0f, 0x0f, "1 Coin  1 Credit"		},
	{0x14, 0x01, 0x0f, 0x00, "4 Coins 5 Credits"		},
	{0x14, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"		},
	{0x14, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"		},
	{0x14, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"		},
	{0x14, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"		},
	{0x14, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"		},
	{0x14, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"		},
	{0x14, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"		},
	{0x14, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"		},
	{0x14, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"		},

	{0   , 0xfe, 0   ,   16, "Coin B"			},
	{0x14, 0x01, 0xf0, 0x20, "4 Coins 1 Credit"		},
	{0x14, 0x01, 0xf0, 0x50, "3 Coins 1 Credit"		},
	{0x14, 0x01, 0xf0, 0x80, "2 Coins 1 Credit"		},
	{0x14, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"		},
	{0x14, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"		},
	{0x14, 0x01, 0xf0, 0xf0, "1 Coin  1 Credit"		},
	{0x14, 0x01, 0xf0, 0x00, "4 Coins 5 Credits"		},
	{0x14, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"		},
	{0x14, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"		},
	{0x14, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"		},
	{0x14, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"		},
	{0x14, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"		},
	{0x14, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"		},
	{0x14, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"		},
	{0x14, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"		},
	{0x14, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x15, 0x01, 0x04, 0x00, "Upright"			},
	{0x15, 0x01, 0x04, 0x04, "Cocktail"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x15, 0x01, 0x60, 0x60, "Easy"				},
	{0x15, 0x01, 0x60, 0x40, "Normal"			},
	{0x15, 0x01, 0x60, 0x20, "Difficult"				},
	{0x15, 0x01, 0x60, 0x00, "Very Difficult"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x15, 0x01, 0x80, 0x80, "Off"				},
	{0x15, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x16, 0x01, 0x01, 0x01, "Off"				},
	{0x16, 0x01, 0x01, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x16, 0x01, 0x04, 0x04, "Off"				},
	{0x16, 0x01, 0x04, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Allow vs match with 1 Credit"	},
	{0x16, 0x01, 0x08, 0x08, "Off"				},
	{0x16, 0x01, 0x08, 0x00, "On"				},
};

STDDIPINFO(Ddribble)

static void bankswitch(INT32 data)
{
	bankdata = data & 7;

	M6809MapMemory(DrvM6809ROM0 + (bankdata * 0x2000), 0x8000, 0x9fff, MAP_ROM);
}

static void ddrible_main_write(UINT16 address, UINT8 data)
{
//	bprintf (0, _T("MW: %4.4x, %2.2x\n"), address, data);

	switch (address)
	{
		case 0x0000:
		case 0x0001:
		case 0x0002:
		case 0x0003:
		case 0x0004:
			DrvVidRegs[0][address & 7] = data;
		return;

		case 0x0800:
		case 0x0801:
		case 0x0802:
		case 0x0803:
		case 0x0804:
			DrvVidRegs[1][address & 7] = data;
		return;

		case 0x8000:
			bankswitch(data);
		return;
	}
}

static void ddribble_sub_write(UINT16 address, UINT8 data)
{
//	if (address != 0x3c00) bprintf (0, _T("SW: %4.4x, %2.2x\n"), address, data);

	switch (address)
	{
		case 0x3400:
			// coin counter
		return;

		case 0x3c00:
			watchdog = data & 0; // kill warning
		return;
	}
}

static UINT8 ddribble_sub_read(UINT16 address)
{
	//bprintf (0, _T("SBR: %4.4x\n"), address);

	switch (address)
	{
		case 0x2800:
			return DrvDips[0];

		case 0x2801:
			return DrvInputs[0];

		case 0x2802:
			return DrvInputs[1];

		case 0x2803:
			return DrvInputs[2];

		case 0x2c00:
			return DrvDips[1];

		case 0x3000:
			return DrvDips[2];
	}

	return 0;
}

static void ddribble_sound_write(UINT16 address, UINT8 data)
{
//	bprintf (0, _T("SW: %4.4x, %2.2x\n"), address, data);

	switch (address)
	{
		case 0x1000:
		case 0x1001:
			BurnYM2203Write(0, address & 1, data);
		return;

		case 0x3000:
			vlm5030_data_write(0, data);
		return;
	}
}

static UINT8 ddribble_sound_read(UINT16 address)
{
//	bprintf (0, _T("SR: %4.4x\n"), address);

	switch (address)
	{
		case 0x1000:
		case 0x1001:
			return BurnYM2203Read(0, address & 1);
	}

	return 0;
}

static tilemap_callback( foreground )
{
	UINT8  attr = DrvFgRAM[offs];
	UINT16 code = DrvFgRAM[offs + 0x400] + (((attr & 0xc0) << 2) + ((attr & 0x20) << 5) + charbank0);

	TILE_SET_INFO(0, code, 0, TILE_FLIPYX(attr >> 4));
}

static tilemap_callback( background )
{
	UINT8  attr = DrvBgRAM[offs];
	UINT16 code = DrvBgRAM[offs + 0x400] + (((attr & 0xc0) << 2) + ((attr & 0x20) << 5) + charbank1);

	TILE_SET_INFO(1, code, 0, TILE_FLIPYX(attr >> 4));
}

static tilemap_scan( tilemap )
{
	return (col & 0x1f) + ((row & 0x1f) << 5) + ((col & 0x20) << 6);
}

static UINT32 DrvVLM5030Sync(INT32 samples_rate)
{
	return (samples_rate * M6809TotalCycles()) / 25600;
}

static void ddribble_ym2203_write_portA(UINT32, UINT32 data)
{
	vlm5030_rst(0, (data >> 6) & 1);
	vlm5030_st(0, (data >> 5) & 1);
	vlm5030_vcu(0, (data >> 4) & 1);

	vlm5030_set_rom(0, DrvSndROM + (((data >> 3) & 1) * 0x10000));

	filter_rc_set_RC(0, FLT_RC_LOWPASS, 1000, 2200, 1000, (data & 4) ? CAP_N(150) : 0);
	filter_rc_set_RC(1, FLT_RC_LOWPASS, 1000, 2200, 1000, (data & 2) ? CAP_N(150) : 0);
	filter_rc_set_RC(2, FLT_RC_LOWPASS, 1000, 2200, 1000, (data & 1) ? CAP_N(150) : 0);
}

static UINT8 ddribble_ym2203_read_port_B(UINT32)
{
	return BurnRandom() & 0xff;
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem)
	{
		memset (AllRam, 0, RamEnd - AllRam);
	}

	M6809Open(0);
	M6809Reset();
	M6809Close();

	M6809Open(1);
	M6809Reset();
	M6809Close();

	M6809Open(2);
	M6809Reset();
	BurnYM2203Reset();
	M6809Close();

	vlm5030Reset(0);

	watchdog = 0;

	nExtraCycles[0] = nExtraCycles[1] = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6809ROM0		= Next; Next += 0x010000;
	DrvM6809ROM1		= Next; Next += 0x010000;
	DrvM6809ROM2		= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += 0x040000;
	DrvGfxROM1		= Next; Next += 0x080000;
	DrvGfxROM2		= Next; Next += 0x040000;
	DrvGfxROM3		= Next; Next += 0x080000;

	DrvSndROM		= Next; Next += 0x020000;

	DrvColPROM		= Next; Next += 0x000100;

	DrvPalette		= (UINT32*)Next; Next += 0x140 * sizeof(UINT32);

	AllRam			= Next;

	DrvBgRAM		= Next; Next += 0x001000;
	DrvFgRAM		= Next; Next += 0x001000;
	DrvPalRAM		= Next; Next += 0x000100;
	DrvShareRAM		= Next; Next += 0x002000;
	DrvSndRAM		= Next; Next += 0x000800;
	DrvSprRAM0		= Next; Next += 0x001000;
	DrvSprRAM1		= Next; Next += 0x001000;

	DrvVidRegs[0]		= Next; Next += 0x000008;
	DrvVidRegs[1]		= Next; Next += 0x000008;

	RamEnd			= Next;
	MemEnd			= Next;

	return 0;
}

static void DrvGfxDecode()
{
	INT32 Plane[4]  = { STEP4(0,1) };
	INT32 XOffs[16] = { STEP8(0,4), STEP8(256,4) };
	INT32 YOffs[16] = { STEP8(0,32), STEP8(512,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x80000);

	memcpy (tmp, DrvGfxROM0, 0x40000);

	GfxDecode(0x1000, 4,  8,  8, Plane, XOffs, YOffs, 0x100, tmp + 0x00000, DrvGfxROM0);
	GfxDecode(0x0400, 4, 16, 16, Plane, XOffs, YOffs, 0x400, tmp + 0x20000, DrvGfxROM2);

	memcpy (tmp, DrvGfxROM1, 0x80000);

	GfxDecode(0x2000, 4,  8,  8, Plane, XOffs, YOffs, 0x100, tmp + 0x00000, DrvGfxROM1);
	GfxDecode(0x0800, 4, 16, 16, Plane, XOffs, YOffs, 0x400, tmp + 0x40000, DrvGfxROM3);

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
		if (BurnLoadRom(DrvM6809ROM0 + 0x00000,  0, 1)) return 1;

		if (BurnLoadRom(DrvM6809ROM1 + 0x08000,  1, 1)) return 1;

		if (BurnLoadRom(DrvM6809ROM2 + 0x08000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0   + 0x00000,  3, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0   + 0x00001,  4, 2)) return 1;

		if ((BurnDrvGetFlags() & BDF_PROTOTYPE) == 0) // normal version
		{
			if (BurnLoadRom(DrvGfxROM1   + 0x00000,  5, 2)) return 1;
			if (BurnLoadRom(DrvGfxROM1   + 0x00001,  6, 2)) return 1;
			if (BurnLoadRom(DrvGfxROM1   + 0x40000,  7, 2)) return 1;
			if (BurnLoadRom(DrvGfxROM1   + 0x40001,  8, 2)) return 1;

			if (BurnLoadRom(DrvColPROM   + 0x00000,  9, 1)) return 1;

			if (BurnLoadRom(DrvSndROM    + 0x00000, 10, 1)) return 1;
		} else {
			if (BurnLoadRom(DrvGfxROM0   + 0x20000,  5, 2)) return 1;
			if (BurnLoadRom(DrvGfxROM0   + 0x20001,  6, 2)) return 1;

			if (BurnLoadRom(DrvGfxROM1   + 0x00000,  7, 2)) return 1;
			if (BurnLoadRom(DrvGfxROM1   + 0x20000,  8, 2)) return 1;
			if (BurnLoadRom(DrvGfxROM1   + 0x00001,  9, 2)) return 1;
			if (BurnLoadRom(DrvGfxROM1   + 0x20001, 10, 2)) return 1;
			if (BurnLoadRom(DrvGfxROM1   + 0x40000, 11, 2)) return 1;
			if (BurnLoadRom(DrvGfxROM1   + 0x60000, 12, 2)) return 1;
			if (BurnLoadRom(DrvGfxROM1   + 0x40001, 13, 2)) return 1;
			if (BurnLoadRom(DrvGfxROM1   + 0x60001, 14, 2)) return 1;

			if (BurnLoadRom(DrvColPROM   + 0x00000, 15, 1)) return 1;

			if (BurnLoadRom(DrvSndROM    + 0x00000, 16, 1)) return 1;
			if (BurnLoadRom(DrvSndROM    + 0x10000, 17, 1)) return 1;
		}

		DrvGfxDecode();
	}

	M6809Init(3);
	M6809Open(0);
	M6809MapMemory(DrvPalRAM,		0x1800, 0x18ff, MAP_RAM);
	M6809MapMemory(DrvFgRAM,		0x2000, 0x2fff, MAP_RAM);
	M6809MapMemory(DrvSprRAM0,		0x3000, 0x3fff, MAP_RAM);
	M6809MapMemory(DrvShareRAM,		0x4000, 0x5fff, MAP_RAM);
	M6809MapMemory(DrvBgRAM,		0x6000, 0x6fff, MAP_RAM);
	M6809MapMemory(DrvSprRAM1,		0x7000, 0x7fff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM0 + 0xa000,	0xa000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(ddrible_main_write);
	M6809Close();

	M6809Open(1);
	M6809MapMemory(DrvShareRAM,		0x0000, 0x1fff, MAP_RAM);
	M6809MapMemory(DrvSndRAM,		0x2000, 0x27ff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM1 + 0x8000,	0x8000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(ddribble_sub_write);
	M6809SetReadHandler(ddribble_sub_read);
	M6809Close();

	M6809Open(2);
	M6809MapMemory(DrvSndRAM,		0x0000, 0x07ff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM2 + 0x8000,	0x8000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(ddribble_sound_write);
	M6809SetReadHandler(ddribble_sound_read);
	M6809Close();

	BurnYM2203Init(1, 3579545, NULL, 0);
	BurnYM2203SetPorts(0, NULL, &ddribble_ym2203_read_port_B, &ddribble_ym2203_write_portA, NULL);
	BurnTimerAttachM6809(1536000);
	BurnYM2203SetAllRoutes(0, 0.25, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetPSGVolume(0, 0.08);

	vlm5030Init(0, 3579545, DrvVLM5030Sync, DrvSndROM, 0x20000, 1);
	vlm5030SetAllRoutes(0, 1.00, BURN_SND_ROUTE_BOTH);

	filter_rc_init(0, FLT_RC_LOWPASS, 1000, 2200, 1000, CAP_P(0), 0);
	filter_rc_init(1, FLT_RC_LOWPASS, 1000, 2200, 1000, CAP_N(0), 1);
	filter_rc_init(2, FLT_RC_LOWPASS, 1000, 2200, 1000, CAP_N(0), 1);
	filter_rc_set_route(0, 1.00, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(1, 1.00, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(2, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, tilemap_map_scan, foreground_map_callback, 8, 8, 64, 32);
	GenericTilemapInit(1, tilemap_map_scan, background_map_callback, 8, 8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 4, 8, 8, 0x40000, 0x30, 0);
	GenericTilemapSetGfx(1, DrvGfxROM1, 4, 8, 8, 0x80000, 0x10, 0);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);
	GenericTilemapSetTransparent(0, 0);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6809Exit();

	BurnYM2203Exit();
	vlm5030Exit();
	filter_rc_exit();

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x80; i+=2) {
		UINT16 p = (DrvPalRAM[i] << 8) | DrvPalRAM[i+1];

		UINT8 r = (p >>  0) & 0x1f;
		UINT8 g = (p >>  5) & 0x1f;
		UINT8 b = (p >> 10) & 0x1f;

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);

		DrvPalette[(i/2)] = BurnHighCol(r,g,b,0);
	}

	for (INT32 i = 0x40; i < 0x140; i++) {
		DrvPalette[i] = DrvPalette[(DrvColPROM[i-0x40] & 0xf)];
	}
}

static void draw_sprites(UINT8 *source, INT32 length, UINT8 *gfxbase, INT32 color_offset, INT32 color_mask, INT32 flipscreen)
{
	const UINT8 *finish = source + length;

	while (source < finish)
	{
		INT32 number = source[0] | ((source[1] & 0x07) << 8);
		INT32 attr = source[4];
		INT32 sx = source[3] | ((attr & 0x01) << 8);
		INT32 sy = source[2];
		INT32 flipx = attr & 0x20;
		INT32 flipy = attr & 0x40;
		INT32 color = (source[1] >> 4) & color_mask;
		INT32 width, height;

		if (flipscreen)
		{
			flipx = !flipx;
			flipy = !flipy;
			sx = 240 - sx;
			sy = 240 - sy;

			if ((attr & 0x1c) == 0x10)
			{
				sx -= 0x10;
				sy -= 0x10;
			}
		}

		switch (attr & 0x1c)
		{
			case 0x10: width = 2; height = 2; number &= ~3; break; // 32x32
			case 0x08: width = 1; height = 2; number &= ~2; break; // 16x32
			case 0x04: width = 2; height = 1; number &= ~1; break; // 32x16
			default:   width = 1; height = 1; number &= ~0; break; // 16x16
		}

		for (INT32 y = 0; y < height; y++)
		{
			for (INT32 x = 0; x < width; x++)
			{
				INT32 ex = flipx ? (width - 1 - x) : x;
				INT32 ey = flipy ? (height - 1 - y) : y;
				INT32 code = number + (ey * 2) + ex;

				if (flipy) {
					if (flipx) {
						Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx+x*16, (sy+y*16)-16, color, 4, 0, color_offset, gfxbase);
					} else {
						Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx+x*16, (sy+y*16)-16, color, 4, 0, color_offset, gfxbase);
					}
				} else {
					if (flipx) {
						Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx+x*16, (sy+y*16)-16, color, 4, 0, color_offset, gfxbase);
					} else {
						Render16x16Tile_Mask_Clip(pTransDraw, code, sx+x*16, (sy+y*16)-16, color, 4, 0, color_offset, gfxbase);
					}
				}
			}
		}

		source += 5;
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 1;
	}

	charbank0 = (DrvVidRegs[0][3] & 2) << 10;
	charbank1 = (DrvVidRegs[1][3] & 3) << 11;

	INT32 flipscreen0 = DrvVidRegs[0][4] & 8;
	INT32 flipscreen1 = DrvVidRegs[1][4] & 8;

	GenericTilemapSetFlip(0, flipscreen0 ? TMAP_FLIPXY : 0);
	GenericTilemapSetFlip(1, flipscreen1 ? TMAP_FLIPXY : 0);

	GenericTilemapSetScrollX(0, DrvVidRegs[0][1] | ((DrvVidRegs[0][2] & 0x01) << 8));
	GenericTilemapSetScrollX(1, DrvVidRegs[1][1] | ((DrvVidRegs[1][2] & 0x01) << 8));
	GenericTilemapSetScrollY(0, DrvVidRegs[0][0]);
	GenericTilemapSetScrollY(1, DrvVidRegs[1][0]);

	if (~nBurnLayer & 1) BurnTransferClear();

	if ( nBurnLayer & 1) GenericTilemapDraw(1, pTransDraw, 0);

	if ( nBurnLayer & 2) draw_sprites(DrvSprRAM0, 0x07d, DrvGfxROM2, 0x20, 0xf, flipscreen0);
	if ( nBurnLayer & 4) draw_sprites(DrvSprRAM1, 0x140, DrvGfxROM3, 0x40, 0xf, flipscreen1);

	if ( nBurnLayer & 8) GenericTilemapDraw(0, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	watchdog++;
	if (watchdog >= 180) {
		DrvDoReset(0);
	}

	if (DrvReset) {
		DrvDoReset(1);
	}

	M6809NewFrame();

	{
		memset (DrvInputs, 0xff, 3);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 MULT = 4; // needs high interleave to avoid contentions between main&sub w/shared ram
	INT32 nInterleave = 256*MULT;
	INT32 nCyclesTotal[3] =  { 1536000 / 60, 1536000 / 60, 1536000 / 60 };
	INT32 nCyclesDone[3] = { nExtraCycles[0], nExtraCycles[1], 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		M6809Open(0);
		if (i == 240*MULT && (DrvVidRegs[0][4] & 2)) M6809SetIRQLine(1, CPU_IRQSTATUS_HOLD);
		nCyclesDone[0] += M6809Run(((i + 1) * nCyclesTotal[0] / nInterleave) - nCyclesDone[0]);
		M6809Close();

		M6809Open(1);
		if (i == 240*MULT && (DrvVidRegs[1][4] & 2)) M6809SetIRQLine(1, CPU_IRQSTATUS_HOLD);
		nCyclesDone[1] += M6809Run(((i + 1) * nCyclesTotal[1] / nInterleave) - nCyclesDone[1]);
		M6809Close();

		if ((i%MULT)==0) { // soundcpu doesn't like high sync, run 256 lines instead of 256*MULT
			M6809Open(2);
			BurnTimerUpdate(((i/MULT) + 1) * nCyclesTotal[2] / (nInterleave/MULT));
			M6809Close();
		}

		if (i == 240*MULT && pBurnDraw) {
			DrvDraw();
		}
	}

	M6809Open(2);

	BurnTimerEndFrame(nCyclesTotal[2]);

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
		vlm5030Update(0, pBurnSoundOut, nBurnSoundLen);
	}

	M6809Close();

	nExtraCycles[0] = nCyclesDone[0] - nCyclesTotal[0];
	nExtraCycles[1] = nCyclesDone[1] - nCyclesTotal[1];
	
	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029704;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		M6809Scan(nAction);

		BurnYM2203Scan(nAction, pnMin);
		vlm5030Scan(nAction, pnMin);

		BurnRandomScan(nAction);

		SCAN_VAR(bankdata);
		SCAN_VAR(nExtraCycles);
	}

	if (nAction & ACB_WRITE) {
		M6809Open(0);
		bankswitch(bankdata);
		M6809Close();
	}

	return 0;
}


// Double Dribble

static struct BurnRomInfo ddribbleRomDesc[] = {
	{ "690c03.bin",		0x10000, 0x07975a58, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code

	{ "690c02.bin",		0x08000, 0xf07c030a, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 #1 Code

	{ "690b01.bin",		0x08000, 0x806b8453, 3 | BRF_PRG | BRF_ESS }, //  2 M6809 #2 Code

	{ "690a05.bin",		0x20000, 0x6a816d0d, 4 | BRF_GRA },           //  3 Characters and Sprites bank 1
	{ "690a06.bin",		0x20000, 0x46300cd0, 4 | BRF_GRA },           //  4

	{ "690a10.bin",		0x20000, 0x61efa222, 5 | BRF_GRA },           //  5 Characters and Sprites bank 2
	{ "690a09.bin",		0x20000, 0xab682186, 5 | BRF_GRA },           //  6
	{ "690a08.bin",		0x20000, 0x9a889944, 5 | BRF_GRA },           //  7
	{ "690a07.bin",		0x20000, 0xfaf81b3f, 5 | BRF_GRA },           //  8

	{ "690a11.i15",		0x00100, 0xf34617ad, 6 | BRF_GRA },           //  9 Sprite Color Look-up table

	{ "690a04.bin",		0x20000, 0x1bfeb763, 7 | BRF_GRA },           // 10 VLM Samples

	{ "pal10l8-007553.bin",	0x0002c, 0x0ae5a161, 8 | BRF_OPT },           // 11 plds
};

STD_ROM_PICK(ddribble)
STD_ROM_FN(ddribble)

struct BurnDriver BurnDrvDdribble = {
	"ddribble", NULL, NULL, NULL, "1986",
	"Double Dribble\0", NULL, "Konami", "GX690",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PREFIX_KONAMI, GBF_SPORTSMISC, 0,
	NULL, ddribbleRomInfo, ddribbleRomName, NULL, NULL, NULL, NULL, DdribbleInputInfo, DdribbleDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x140,
	256, 224, 4, 3
};


// Double Dribble (prototype?)

static struct BurnRomInfo ddribblepRomDesc[] = {
	{ "ebs_11-19.c19",	0x10000, 0x0a81c926, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code

	{ "eb_11-19.c12",	0x08000, 0x22130292, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 #1 Code

	{ "master_sound.a6",	0x08000, 0x090e3a31, 3 | BRF_PRG | BRF_ESS }, //  2 M6809 #2 Code

	{ "v1a.e12",		0x10000, 0x53724765, 4 | BRF_GRA },           //  3 Characters and Sprites bank 1
	{ "v1b.e13",		0x10000, 0xd9dc6f1a, 4 | BRF_GRA },           //  4
	{ "01a.e11",		0x10000, 0x1ae5d725, 4 | BRF_GRA },           //  5
	{ "01b.d14",		0x10000, 0x054c5242, 4 | BRF_GRA },           //  6

	{ "v2a00.i13",		0x10000, 0xa33f7d6d, 5 | BRF_GRA },           //  7 Characters and Sprites bank 2
	{ "v2a10.h13",		0x10000, 0x8fbc7454, 5 | BRF_GRA },           //  8
	{ "v2b00.i12",		0x10000, 0xe63759bb, 5 | BRF_GRA },           //  9
	{ "v2b10.h12",		0x10000, 0x8a7d4062, 5 | BRF_GRA },           // 10
	{ "02a00.i11",		0x10000, 0x6751a942, 5 | BRF_GRA },           // 11
	{ "02a10.h11",		0x10000, 0xbc5ff11c, 5 | BRF_GRA },           // 12
	{ "02b00_11-4.i8.bin",	0x10000, 0x460aa7b4, 5 | BRF_GRA },           // 13
	{ "02b10.h8",		0x10000, 0x2cc7ee28, 5 | BRF_GRA },           // 14

	{ "6301-1.i15",		0x00100, 0xf34617ad, 6 | BRF_GRA },           // 15 Sprite Color Look-up table

	{ "voice_00.e7",	0x10000, 0x8bd0fcf7, 7 | BRF_GRA },           // 16 VLM Samples
	{ "voice_10.d7",	0x10000, 0xb4c97494, 7 | BRF_GRA },           // 17
};

STD_ROM_PICK(ddribblep)
STD_ROM_FN(ddribblep)

struct BurnDriver BurnDrvDdribblep = {
	"ddribblep", "ddribble", NULL, NULL, "1986",
	"Double Dribble (prototype?)\0", NULL, "Konami", "GX690",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 2, HARDWARE_PREFIX_KONAMI, GBF_SPORTSMISC, 0,
	NULL, ddribblepRomInfo, ddribblepRomName, NULL, NULL, NULL, NULL, DdribbleInputInfo, DdribbleDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x140,
	256, 224, 4, 3
};
