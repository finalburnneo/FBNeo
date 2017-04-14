// FB Alpha Shoot Out driver module
// Based on MAME driver by Ernesto Corvi, Phil Stroffolino, Bryan McPhail

#include "tiles_generic.h"
#include "m6502_intf.h"
#include "burn_ym2203.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6502ROM0;
static UINT8 *DrvM6502ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvColPROM;
static UINT8 *DrvM6502RAM0A;
static UINT8 *DrvM6502RAM0B;
static UINT8 *DrvM6502RAM1;
static UINT8 *DrvTxtRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;

static UINT8 soundlatch;
static UINT8 flipscreen;
static UINT8 bankdata;
static INT32 soundcpu_mhz;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 vblank;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvInputs[2];
static UINT8 DrvDips[2];
static UINT8 DrvReset;

static UINT8 shootoutb = 0;
static UINT8 ym2203portainit;
static UINT8 ym2203portbinit;

static struct BurnInputInfo ShootoutInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Shootout)

static struct BurnDIPInfo ShootoutDIPList[]=
{
	{0x11, 0xff, 0xff, 0xbf, NULL			},
	{0x12, 0xff, 0xff, 0x3f, NULL			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x11, 0x01, 0x03, 0x00, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0x03, 0x01, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x11, 0x01, 0x0c, 0x00, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0x0c, 0x04, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    0, "Demo Sounds"		},
	{0x11, 0x01, 0x20, 0x00, "Off"			},
	{0x11, 0x01, 0x20, 0x20, "On"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x11, 0x01, 0x40, 0x00, "Upright"		},
	{0x11, 0x01, 0x40, 0x40, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x11, 0x01, 0x80, 0x80, "Off"			},
	{0x11, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x12, 0x01, 0x03, 0x01, "1"			},
	{0x12, 0x01, 0x03, 0x03, "3"			},
	{0x12, 0x01, 0x03, 0x02, "5"			},
	{0x12, 0x01, 0x03, 0x00, "Infinite"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x12, 0x01, 0x0c, 0x0c, "20,000 Every 70,000"	},
	{0x12, 0x01, 0x0c, 0x08, "30,000 Every 80,000"	},
	{0x12, 0x01, 0x0c, 0x04, "40,000 Every 90,000"	},
	{0x12, 0x01, 0x0c, 0x00, "70,000"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x12, 0x01, 0x30, 0x30, "Easy"			},
	{0x12, 0x01, 0x30, 0x20, "Normal"		},
	{0x12, 0x01, 0x30, 0x10, "Hard"			},
	{0x12, 0x01, 0x30, 0x00, "Very Hard"		},
};

STDDIPINFO(Shootout)

static struct BurnDIPInfo ShootoujDIPList[]=
{
	{0x11, 0xff, 0xff, 0xbf, NULL			},
	{0x12, 0xff, 0xff, 0x3f, NULL			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x11, 0x01, 0x03, 0x00, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0x03, 0x01, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x11, 0x01, 0x0c, 0x00, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0x0c, 0x04, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    0, "Company Copyright"	},
	{0x11, 0x01, 0x20, 0x20, "Data East Corp"	},
	{0x11, 0x01, 0x20, 0x00, "Data East USA Inc"	},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x11, 0x01, 0x40, 0x00, "Upright"		},
	{0x11, 0x01, 0x40, 0x40, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x11, 0x01, 0x80, 0x80, "Off"			},
	{0x11, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x12, 0x01, 0x03, 0x01, "1"			},
	{0x12, 0x01, 0x03, 0x03, "3"			},
	{0x12, 0x01, 0x03, 0x02, "5"			},
	{0x12, 0x01, 0x03, 0x00, "Infinite"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x12, 0x01, 0x0c, 0x0c, "20,000 Every 50,000"	},
	{0x12, 0x01, 0x0c, 0x08, "30,000 Every 60,000"	},
	{0x12, 0x01, 0x0c, 0x04, "50,000 Every 70,000"	},
	{0x12, 0x01, 0x0c, 0x00, "70,000"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x12, 0x01, 0x30, 0x30, "Easy"			},
	{0x12, 0x01, 0x30, 0x20, "Normal"		},
	{0x12, 0x01, 0x30, 0x10, "Hard"			},
	{0x12, 0x01, 0x30, 0x00, "Very Hard"		},
};

STDDIPINFO(Shootouj)

static void bankswitch(INT32 data)
{
	INT32 bank = 0x8000 + (data & 0xf) * 0x4000;

	bankdata = data;

	M6502MapMemory(DrvM6502ROM0 + bank, 0x4000, 0x7fff, MAP_ROM);
}

static void shootout_main_write(UINT16 address, UINT8 data)
{
	if (address >= 0x1004 && address <= 0x17ff) {
		DrvM6502RAM0B[address & 0x7ff] = data;
		return;
	}

	switch (address)
	{
		case 0x1000:
			bankswitch(data & 0xf);
		return;

		case 0x1001:
			flipscreen = data & 1;
		return;

		case 0x1002:
			// coin counter
		return;

		case 0x1003:
			soundlatch = data;
			M6502Close();
			M6502Open(1);
			M6502SetIRQLine(0x20, CPU_IRQSTATUS_AUTO);
			M6502Close();
			M6502Open(0);
		return;

	// shootouj
		case 0x1800:
			// coin counter
		return;

		case 0x2800:
		case 0x2801:
			BurnYM2203Write(0, address & 1, data);
		return;	
	}
}

static UINT8 shootout_main_read(UINT16 address)
{
	if (address >= 0x1004 && address <= 0x17ff) {
		return DrvM6502RAM0B[address & 0x7ff];
	}

	switch (address)
	{
		case 0x1000:
			return DrvDips[0];

		case 0x1001:
			return DrvInputs[0];

		case 0x1002:
			return DrvInputs[1];

		case 0x1003:
			return (DrvDips[1] & 0x3f) | (vblank ? 0 : 0x80);

		case 0x2800:
		case 0x2801:
			return BurnYM2203Read(0, address & 1);
	}

	return 0;
}

static void shootout_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x4000:
		case 0x4001:
			BurnYM2203Write(0, address & 1, data);
		return;

		case 0xd000:
		return;	//nop
	}
}

static UINT8 shootout_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x4000:
		case 0x4001:
			return BurnYM2203Read(0, address & 1);

		case 0xa000:
			return soundlatch;
	}

	return 0;
}

static void ym2203_write_port_A(UINT32, UINT32 data)
{
	if (ym2203portainit == 0) { // first thing written here on YM2203Reset() is 0xff, causes problems.
		ym2203portainit = 1;
		return;
	}

	bankswitch(data);
}

static void ym2203_write_port_B(UINT32, UINT32 data)
{
	if (ym2203portbinit == 0) {
		ym2203portbinit = 1;
		return;
	}

	flipscreen = data & 1;
}

inline static void DrvYM2203IRQHandler(INT32, INT32 nStatus)
{
	M6502SetIRQLine(0, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

inline static INT32 DrvSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)M6502TotalCycles() * nSoundRate / soundcpu_mhz;
}

inline static double DrvGetTime()
{
	return (double)M6502TotalCycles() / soundcpu_mhz;
}

static tilemap_callback( background )
{
	INT32 attr  = DrvVidRAM[offs + 0x400];
	INT32 code  = DrvVidRAM[offs] + ((attr & 7) << 8);
	INT32 color = attr >> 4;

	TILE_SET_INFO(0, code, color, 0);
}

static tilemap_callback( foreground )
{
	INT32 attr  = DrvTxtRAM[offs + 0x400];
	INT32 code  = DrvTxtRAM[offs] + ((attr & 3) << 8);
	INT32 color = attr >> 4;

	TILE_SET_INFO(1, code, color, 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	M6502Open(0);
	M6502Reset();
	bankswitch(0);
	M6502Close();

	M6502Open(1);
	M6502Reset();
	BurnYM2203Reset();
	M6502Close();

	soundlatch = 0;
	flipscreen = 0;
	vblank = 1; // 248 on, 8 off

	ym2203portainit = 0;
	ym2203portbinit = 0;

	DrvInputs[1] = 0x3f;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6502ROM0	= Next; Next += 0x018000;

	DrvM6502ROM1	= Next; Next += 0x004000;

	DrvGfxROM0		= Next; Next += 0x010000;
	DrvGfxROM1		= Next; Next += 0x080000;
	DrvGfxROM2		= Next; Next += 0x020000;

	DrvColPROM		= Next; Next += 0x000100;

	DrvPalette		= (UINT32*)Next; Next += 0x0100 * sizeof(UINT32);

	AllRam			= Next;

	DrvM6502RAM0A	= Next; Next += 0x001000;
	DrvM6502RAM0B	= Next; Next += 0x000800;
	DrvM6502RAM1	= Next; Next += 0x000800;
	DrvTxtRAM		= Next; Next += 0x000800;
	DrvVidRAM		= Next; Next += 0x000800;
	DrvSprRAM		= Next; Next += 0x000200;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[2] = { 0, 4 };
	INT32 Plane1[3] = { 0*0x10000*8, 1*0x10000*8, 2*0x10000*8 };
	INT32 XOffs0[8] = { STEP4((0x4000*8), 1), STEP4(0,1) };
	INT32 XOffs1[16]= { STEP8(128,1), STEP8(0,1) };
	INT32 YOffs[16] = { STEP16(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x30000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x8000); // char

	GfxDecode(0x0400, 2,  8,  8, Plane0, XOffs0, YOffs, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x30000);

	GfxDecode(0x0800, 3, 16, 16, Plane1, XOffs1, YOffs, 0x100, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x8000);

	GfxDecode(0x0800, 2,  8,  8, Plane0, XOffs0, YOffs, 0x040, tmp, DrvGfxROM2);

	BurnFree(tmp);

	return 0;
}

static INT32 ShootoutInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvM6502ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM0 + 0x08000,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM0 + 0x10000,  2, 1)) return 1;

		if (BurnLoadRom(DrvM6502ROM1 + 0x00000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0   + 0x00000,  4, 1)) return 1;
		memcpy (DrvGfxROM0 + 0x4000, DrvGfxROM0 + 0x2000, 0x2000);

		if (BurnLoadRom(DrvGfxROM1   + 0x00000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1   + 0x08000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1   + 0x10000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1   + 0x18000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1   + 0x20000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1   + 0x28000, 10, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2   + 0x08000, 11, 1)) return 1;
		memcpy (DrvGfxROM2 + 0x00000, DrvGfxROM2 + 0x08000 + 0x00000, 0x02000);
		memcpy (DrvGfxROM2 + 0x04000, DrvGfxROM2 + 0x08000 + 0x02000, 0x02000);
		memcpy (DrvGfxROM2 + 0x02000, DrvGfxROM2 + 0x08000 + 0x04000, 0x02000);
		memcpy (DrvGfxROM2 + 0x06000, DrvGfxROM2 + 0x08000 + 0x06000, 0x02000);
		memset (DrvGfxROM2 + 0x08000, 0, 0x08000);

		if (BurnLoadRom(DrvColPROM   + 0x00000, 12, 1)) return 1;

		DrvGfxDecode();
	}

	M6502Init(0, TYPE_DECO222);
	M6502Open(0);
	M6502MapMemory(DrvM6502RAM0A,	0x0000, 0x0fff, MAP_RAM);
	//M6502MapMemory(DrvM6502RAM0B,	0x1000, 0x17ff, MAP_RAM); // in handler
	M6502MapMemory(DrvSprRAM,		0x1800, 0x19ff, MAP_RAM);
	M6502MapMemory(DrvTxtRAM,		0x2000, 0x27ff, MAP_RAM);
	M6502MapMemory(DrvVidRAM,		0x2800, 0x2fff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM0,	0x8000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(shootout_main_write);
	M6502SetReadHandler(shootout_main_read);
	M6502Close();

	M6502Init(1, TYPE_M6502);
	M6502Open(1);
	M6502MapMemory(DrvM6502RAM1,		0x0000, 0x07ff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM1,		0xc000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(shootout_sound_write);
	M6502SetReadHandler(shootout_sound_read);
	M6502Close();

	BurnYM2203Init(1, 1500000, &DrvYM2203IRQHandler, DrvSynchroniseStream, DrvGetTime, 0);
	BurnTimerAttachM6502(1500000);
	soundcpu_mhz = 1500000;
	BurnYM2203SetAllRoutes(0, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, background_map_callback, 8, 8, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, foreground_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM2, 2, 8, 8, 0x20000, 0x00, 0xf);
	GenericTilemapSetGfx(1, DrvGfxROM0, 2, 8, 8, 0x10000, 0x80, 0xf);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -8);
	GenericTilemapSetTransparent(1, 0);

	DrvDoReset();

	return 0;
}

static INT32 ShootoujInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvM6502ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM0 + 0x08000,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM0 + 0x10000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0   + 0x00000,  3, 1)) return 1;
		memcpy (DrvGfxROM0 + 0x4000, DrvGfxROM0 + 0x2000, 0x2000);

		if (BurnLoadRom(DrvGfxROM1   + 0x00000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1   + 0x10000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1   + 0x20000,  6, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2   + 0x08000,  7, 1)) return 1;
		memcpy (DrvGfxROM2 + 0x00000, DrvGfxROM2 + 0x08000 + 0x00000, 0x02000);
		memcpy (DrvGfxROM2 + 0x04000, DrvGfxROM2 + 0x08000 + 0x02000, 0x02000);
		memcpy (DrvGfxROM2 + 0x02000, DrvGfxROM2 + 0x08000 + 0x04000, 0x02000);
		memcpy (DrvGfxROM2 + 0x06000, DrvGfxROM2 + 0x08000 + 0x06000, 0x02000);
		memset (DrvGfxROM2 + 0x08000, 0, 0x08000);

		if (BurnLoadRom(DrvColPROM   + 0x00000,  8, 1)) return 1;

		DrvGfxDecode();
	}

	M6502Init(0, (shootoutb) ? TYPE_DECO222 : TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvM6502RAM0A,	0x0000, 0x0fff, MAP_RAM);
	M6502MapMemory(DrvSprRAM,		0x2000, 0x21ff, MAP_RAM);
	M6502MapMemory(DrvTxtRAM,		0x3000, 0x37ff, MAP_RAM);
	M6502MapMemory(DrvVidRAM,		0x3800, 0x3fff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM0,	0x8000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(shootout_main_write);
	M6502SetReadHandler(shootout_main_read);
	M6502Close();

	// Fake so we can re-use functions
	M6502Init(1, TYPE_M6502);
	M6502Open(1);
	M6502MapMemory(DrvM6502ROM0,	0x8000, 0xffff, MAP_ROM);
	M6502Close();

	BurnYM2203Init(1, 1500000, &DrvYM2203IRQHandler, DrvSynchroniseStream, DrvGetTime, 0);
	BurnYM2203SetPorts(0, NULL, NULL, &ym2203_write_port_A, &ym2203_write_port_B);
	BurnTimerAttachM6502(2000000);
	soundcpu_mhz = 2000000;
	BurnYM2203SetAllRoutes(0, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, background_map_callback, 8, 8, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, foreground_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM2, 2, 8, 8, 0x20000, 0x00, 0xf);
	GenericTilemapSetGfx(1, DrvGfxROM0, 2, 8, 8, 0x10000, 0x80, 0xf);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -8);
	GenericTilemapSetTransparent(1, 0);

	DrvDoReset();

	return 0;
}

static INT32 ShootoutbInit()
{
	shootoutb = 1;
	return ShootoujInit();
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6502Exit();
	BurnYM2203Exit();

	BurnFree (AllMem);

	shootoutb = 0;

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 256; i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 0x01;
		INT32 r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = (DrvColPROM[i] >> 3) & 0x01;
		bit1 = (DrvColPROM[i] >> 4) & 0x01;
		bit2 = (DrvColPROM[i] >> 5) & 0x01;
		INT32 g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = 0;
		bit1 = (DrvColPROM[i] >> 6) & 0x01;
		bit2 = (DrvColPROM[i] >> 7) & 0x01;
		INT32 b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_sprites(INT32 bank_bits)
{
	const UINT8 *source = DrvSprRAM+127*4;

	bool m_bFlicker = (nCurrentFrame & 1) != 0;

	for(INT32 count=0; count<128; count++)
	{
		int attributes = source[1];

		if ( attributes & 0x01 ){
			if( m_bFlicker || (attributes&0x02)==0 ){
				int priority_mask = (attributes&0x08)?0x2:0;
				int sx = (240 - source[2])&0xff;
				int sy = (240 - source[0])&0xff;
				int vx, vy;
				int number = source[3] | ((attributes<<bank_bits)&0x700);
				int flipx = (attributes & 0x04);
				int flipy = 0;

				if (flipscreen) {
					flipx = !flipx;
					flipy = !flipy;
				}

				if( attributes & 0x10 ){ // 2x height
					number = number&(~1);
					sy -= 16;

					vx = sx;
					vy = sy;
					if (flipscreen) {
						vx = 240 - vx;
						vy = 240 - vy;
					}

					RenderPrioSprite(pTransDraw, DrvGfxROM1, number, 0x40, 0, vx, vy - 8, flipx, flipy, 16, 16, priority_mask);

					number++;
					sy += 16;
				}

				vx = sx;
				vy = sy;
				if (flipscreen) {
					vx = 240 - vx;
					vy = 240 - vy;
				}

				RenderPrioSprite(pTransDraw, DrvGfxROM1, number, 0x40, 0, vx, vy - 8, flipx, flipy, 16, 16, priority_mask);
			}
		}
		source -= 4;
	}
}

static INT32 DrvDraw(INT32 bits)
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	BurnTransferClear();

	GenericTilemapSetFlip(TMAP_GLOBAL, flipscreen ? TMAP_FLIPXY : 0);

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0); // bg
	if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 1); // fg

	if (nSpriteEnable & 1) draw_sprites(bits);
	
	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 ShootoutDraw()
{
	return DrvDraw(3);
}

static INT32 ShootoujDraw()
{
	return DrvDraw(2);
}

static INT32 ShootoutFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	M6502NewFrame();

	{
		INT32 previous_coin = DrvInputs[1] & 0xc0;

		DrvInputs[0] = 0xff;
		DrvInputs[1] = 0x3f;
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}


		if ((DrvInputs[1] & 0xc0) && (DrvInputs[1] & 0xc0) != previous_coin) {
			M6502Open(0);
			M6502SetIRQLine(0x20, CPU_IRQSTATUS_AUTO);
			M6502Close();
		}
	}

	INT32 nInterleave = 262;
	INT32 nCyclesTotal[2] =  { 2000000 / 60, 1500000 / 60 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		if (i == 248) vblank = 1;
		if (i == 8) vblank = 0;

		M6502Open(0);
		M6502Run(nCyclesTotal[0] / nInterleave);
		M6502Close();

		M6502Open(1);
		BurnTimerUpdate((i + 1) * (nCyclesTotal[1] / nInterleave));
		M6502Close();
	}

	M6502Open(1);
	BurnTimerEndFrame(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
	}

	M6502Close();
	
	if (pBurnDraw) {
		ShootoutDraw();
	}

	return 0;
}

static INT32 ShootoujFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	M6502NewFrame();

	{
		INT32 previous_coin = DrvInputs[1] & 0xc0;

		DrvInputs[0] = 0xff;
		DrvInputs[1] = 0x3f;
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}

		if ((DrvInputs[1] & 0xc0) && (DrvInputs[1] & 0xc0) != previous_coin) {
			M6502Open(0);
			M6502SetIRQLine(0x20, CPU_IRQSTATUS_AUTO);
			M6502Close();
		}
	}

	INT32 nInterleave = 262;
	INT32 nCyclesTotal[1] =  { 2000000 / 60 };

	M6502Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		if (i == 248) vblank = 1;
		if (i == 8) vblank = 0;

		BurnTimerUpdate((i + 1) * (nCyclesTotal[0] / nInterleave));
	}

	BurnTimerEndFrame(nCyclesTotal[0]);

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
	}

	M6502Close();
	
	if (pBurnDraw) {
		ShootoujDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {			// Return minimum compatible version
		*pnMin = 0x029719;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		M6502Scan(nAction);

		BurnYM2203Scan(nAction, pnMin);

		SCAN_VAR(soundlatch);
		SCAN_VAR(flipscreen);
		SCAN_VAR(bankdata);
	}

	if (nAction & ACB_WRITE) {
		M6502Open(0);
		bankswitch(bankdata);
		M6502Close();
	}

	return 0;
}


// Shoot Out (US)

static struct BurnRomInfo shootoutRomDesc[] = {
	{ "cu00.b1",		0x8000, 0x090edeb6, 1 | BRF_PRG | BRF_ESS }, //  0 Encrypted M6502 Code (main)
	{ "cu02.c3",		0x8000, 0x2a913730, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "cu01.c1",		0x4000, 0x8843c3ae, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "cu09.j1",		0x4000, 0xc4cbd558, 2 | BRF_PRG | BRF_ESS }, //  3 M6502 Code (sound)
	
	{ "cu11.h19",		0x4000, 0xeff00460, 3 | BRF_GRA },           //  4 Foreground Tiles

	{ "cu04.c7",		0x8000, 0xceea6b20, 4 | BRF_GRA },           //  5 Sprites
	{ "cu03.c5",		0x8000, 0xb786bb3e, 4 | BRF_GRA },           //  6
	{ "cu06.c10",		0x8000, 0x2ec1d17f, 4 | BRF_GRA },           //  7
	{ "cu05.c9",		0x8000, 0xdd038b85, 4 | BRF_GRA },           //  8
	{ "cu08.c13",		0x8000, 0x91290933, 4 | BRF_GRA },           //  9
	{ "cu07.c12",		0x8000, 0x19b6b94f, 4 | BRF_GRA },           // 10

	{ "cu10.h17",		0x8000, 0x3854c877, 5 | BRF_GRA },           // 11 Background Tiles

	{ "gb08.k10",		0x0100, 0x509c65b6, 6 | BRF_GRA },           // 12 Color data

	{ "gb09.k6",		0x0100, 0xaa090565, 0 | BRF_OPT },           // 13
};

STD_ROM_PICK(shootout)
STD_ROM_FN(shootout)

struct BurnDriver BurnDrvShootout = {
	"shootout", NULL, NULL, NULL, "1985",
	"Shoot Out (US)\0", NULL, "Data East USA", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PREFIX_DATAEAST, GBF_SHOOT, 0,
	NULL, shootoutRomInfo, shootoutRomName, NULL, NULL, ShootoutInputInfo, ShootoutDIPInfo,
	ShootoutInit, DrvExit, ShootoutFrame, ShootoutDraw, DrvScan, &DrvRecalc, 0x100,
	256, 240, 4, 3
};


// Shoot Out (Japan)

static struct BurnRomInfo shootoutjRomDesc[] = {
	{ "cg02.bin",		0x8000, 0x8fc5d632, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code (main)
	{ "cg00.bin",		0x8000, 0xef6ced1e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "cg01.bin",		0x4000, 0x74cf11ca, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "cu11.h19",		0x4000, 0xeff00460, 2 | BRF_GRA },           //  3 Foreground Tiles

	{ "cg03.bin",		0x8000, 0x5252ec19, 3 | BRF_GRA },           //  4 Sprites
	{ "cg04.bin",		0x8000, 0xdb06cfe9, 3 | BRF_GRA },           //  5
	{ "cg05.bin",		0x8000, 0xd634d6b8, 3 | BRF_GRA },           //  6

	{ "cu10.h17",		0x8000, 0x3854c877, 4 | BRF_GRA },           //  7 Background Tiles

	{ "gb08.k10",		0x0100, 0x509c65b6, 5 | BRF_GRA },           //  8 Color data

	{ "gb09.k6",		0x0100, 0xaa090565, 0 | BRF_OPT },           //  9
};

STD_ROM_PICK(shootoutj)
STD_ROM_FN(shootoutj)

struct BurnDriver BurnDrvShootoutj = {
	"shootoutj", "shootout", NULL, NULL, "1985",
	"Shoot Out (Japan)\0", NULL, "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_DATAEAST, GBF_SHOOT, 0,
	NULL, shootoutjRomInfo, shootoutjRomName, NULL, NULL, ShootoutInputInfo, ShootoujDIPInfo,
	ShootoujInit, DrvExit, ShootoujFrame, ShootoujDraw, DrvScan, &DrvRecalc, 0x100,
	256, 240, 4, 3
};


// Shoot Out (Korean Bootleg)

static struct BurnRomInfo shootoutbRomDesc[] = {
	{ "shootout.006",	0x8000, 0x2c054888, 1 | BRF_PRG | BRF_ESS }, //  0 Encrypted M6502 Code (main)
	{ "shootout.008",	0x8000, 0x9651b656, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "cg01.bin",		0x4000, 0x74cf11ca, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "cu11.h19",		0x4000, 0xeff00460, 2 | BRF_GRA },           //  3 Foreground Tiles

	{ "shootout.005",	0x8000, 0xe6357ba3, 3 | BRF_GRA },           //  4 Sprites
	{ "shootout.004",	0x8000, 0x7f422c93, 3 | BRF_GRA },           //  5
	{ "shootout.003",	0x8000, 0xeea94535, 3 | BRF_GRA },           //  6

	{ "cu10.h17",		0x8000, 0x3854c877, 4 | BRF_GRA },           //  7 Background Tiles

	{ "gb08.k10",		0x0100, 0x509c65b6, 5 | BRF_GRA },           //  8 Color data

	{ "gb09.k6",		0x0100, 0xaa090565, 0 | BRF_OPT },           //  9
	{ "shootclr.003",	0x0020, 0x6b0c2942, 0 | BRF_OPT },           // 10
};

STD_ROM_PICK(shootoutb)
STD_ROM_FN(shootoutb)

struct BurnDriver BurnDrvShootoutb = {
	"shootoutb", "shootout", NULL, NULL, "1985",
	"Shoot Out (Korean Bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_PREFIX_DATAEAST, GBF_SHOOT, 0,
	NULL, shootoutbRomInfo, shootoutbRomName, NULL, NULL, ShootoutInputInfo, ShootoujDIPInfo,
	ShootoutbInit, DrvExit, ShootoujFrame, ShootoutDraw, DrvScan, &DrvRecalc, 0x100,
	256, 240, 4, 3
};
