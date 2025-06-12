// FB Alpha Contra driver module
// Based on MAME driver by Carlos A. Lozano, Phil Stroffolino, Jose T. Gomez, and Eric Hustvedt

#include "tiles_generic.h"
#include "burn_ym2151.h"
#include "m6809_intf.h"
#include "hd6309_intf.h"
#include "k007121.h"
#include "k007452.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvHD6309ROM0;
static UINT8 *DrvM6809ROM0;
static UINT8 *DrvHD6309RAM0;
static UINT8 *DrvHD6309RAM1;
static UINT8 *DrvM6809RAM0;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvPROMs;
static UINT8 *DrvColTable;
static UINT8 *DrvPalRAM;
static UINT8 *DrvFgCRAM;
static UINT8 *DrvFgVRAM;
static UINT8 *DrvTxCRAM;
static UINT8 *DrvTxVRAM;
static UINT8 *DrvBgCRAM;
static UINT8 *DrvBgVRAM;
static UINT8 *DrvSprRAM[2];
static UINT32 *DrvPalette;
static UINT32 *Palette;
static UINT8  DrvRecalc;

static INT32 nExtraCycles;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvInputs[3];
static UINT8 DrvDip[3];
static UINT8 DrvReset;

static UINT8 soundlatch;
static UINT8 nBankData;

static struct BurnInputInfo DrvInputList[] =
{
	{"P1 Coin"           , BIT_DIGITAL  , DrvJoy1 + 0, "p1 coin"   },
	{"P1 Start"          , BIT_DIGITAL  , DrvJoy1 + 3, "p1 start"  },
	{"P1 Left"           , BIT_DIGITAL  , DrvJoy2 + 0, "p1 left"   },
	{"P1 Right"          , BIT_DIGITAL  , DrvJoy2 + 1, "p1 right"  },
	{"P1 Up"             , BIT_DIGITAL  , DrvJoy2 + 2, "p1 up"     },
	{"P1 Down"           , BIT_DIGITAL  , DrvJoy2 + 3, "p1 down"   },
	{"P1 Fire 1"         , BIT_DIGITAL  , DrvJoy2 + 4, "p1 fire 1" },
	{"P1 Fire 2"         , BIT_DIGITAL  , DrvJoy2 + 5, "p1 fire 2" },

	{"P2 Coin"           , BIT_DIGITAL  , DrvJoy1 + 1, "p2 coin"   },
	{"P2 Start"          , BIT_DIGITAL  , DrvJoy1 + 4, "p2 start"  },
	{"P2 Left"           , BIT_DIGITAL  , DrvJoy3 + 0, "p2 left"   },
	{"P2 Right"          , BIT_DIGITAL  , DrvJoy3 + 1, "p2 right"  },
	{"P2 Up"             , BIT_DIGITAL  , DrvJoy3 + 2, "p2 up"     },
	{"P2 Down"           , BIT_DIGITAL  , DrvJoy3 + 3, "p2 down"   },
	{"P2 Fire 1"         , BIT_DIGITAL  , DrvJoy3 + 4, "p2 fire 1" },
	{"P2 Fire 2"         , BIT_DIGITAL  , DrvJoy3 + 5, "p2 fire 2" },

	{"Reset"             , BIT_DIGITAL  , &DrvReset,   "reset"     },
	{"Service"           , BIT_DIGITAL  , DrvJoy1 + 2, "service"   },
	{"Dip 1"             , BIT_DIPSWITCH, DrvDip + 0,  "dip"       },
	{"Dip 2"             , BIT_DIPSWITCH, DrvDip + 1,  "dip"       },
	{"Dip 3"             , BIT_DIPSWITCH, DrvDip + 2,  "dip"       },
};

STDINPUTINFO(Drv)

static struct BurnDIPInfo DrvDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL },
	{0x13, 0xff, 0xff, 0x5a, NULL },
	{0x14, 0xff, 0xff, 0xff, NULL },

	{0x12, 0xfe,    0,   16, "Coin A"  },
	{0x12, 0x01, 0x0f, 0x02, "4 Coins 1 Credit"  },
	{0x12, 0x01, 0x0f, 0x05, "3 Coins 1 Credit"  },
	{0x12, 0x01, 0x0f, 0x08, "2 Coins 1 Credit"  },
	{0x12, 0x01, 0x0f, 0x04, "3 Coins 2 Credits" },
	{0x12, 0x01, 0x0f, 0x01, "4 Coins 3 Credits" },
	{0x12, 0x01, 0x0f, 0x0f, "1 Coin  1 Credit"  },
	{0x12, 0x01, 0x0f, 0x03, "3 Coins 4 Credits" },
	{0x12, 0x01, 0x0f, 0x07, "2 Coins 3 Credits" },
	{0x12, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits" },
	{0x12, 0x01, 0x0f, 0x06, "2 Coins 5 Credits" },
	{0x12, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits" },
	{0x12, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits" },
	{0x12, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits" },
	{0x12, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits" },
	{0x12, 0x01, 0x0f, 0x09, "1 Coin  7 Credits" },
	{0x12, 0x01, 0x0f, 0x00, "Free Play"         },

	{0x12, 0xfe,    0,   15, "Coin B"  },
	{0x12, 0x01, 0xf0, 0x20, "4 Coins 1 Credit"  },
	{0x12, 0x01, 0xf0, 0x50, "3 Coins 1 Credit"  },
	{0x12, 0x01, 0xf0, 0x80, "2 Coins 1 Credit"  },
	{0x12, 0x01, 0xf0, 0x40, "3 Coins 2 Credits" },
	{0x12, 0x01, 0xf0, 0x10, "4 Coins 3 Credits" },
	{0x12, 0x01, 0xf0, 0xf0, "1 Coin  1 Credit"  },
	{0x12, 0x01, 0xf0, 0x30, "3 Coins 4 Credits" },
	{0x12, 0x01, 0xf0, 0x70, "2 Coins 3 Credits" },
	{0x12, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits" },
	{0x12, 0x01, 0xf0, 0x60, "2 Coins 5 Credits" },
	{0x12, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits" },
	{0x12, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits" },
	{0x12, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits" },
	{0x12, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits" },
	{0x12, 0x01, 0xf0, 0x90, "1 Coin  7 Credits" },

	{0x13, 0xfe,    0,    4, "Lives"  },
	{0x13, 0x01, 0x03, 0x03, "2" },
	{0x13, 0x01, 0x03, 0x02, "3" },
	{0x13, 0x01, 0x03, 0x01, "5" }, // 4 in test mode
	{0x13, 0x01, 0x03, 0x00, "7" },

	{0x13, 0xfe,    0,    4, "Bonus Life"  },
	{0x13, 0x01, 0x18, 0x18, "30000 70000" },
	{0x13, 0x01, 0x18, 0x10, "40000 80000" },
	{0x13, 0x01, 0x18, 0x08, "40000" },
	{0x13, 0x01, 0x18, 0x00, "50000" },

	{0x13, 0xfe,    0,    4, "Difficulty"  },
	{0x13, 0x01, 0x60, 0x60, "Easy" },
	{0x13, 0x01, 0x60, 0x40, "Normal" },
	{0x13, 0x01, 0x60, 0x20, "Hard" },
	{0x13, 0x01, 0x60, 0x00, "Hardest" },

	{0x13, 0xfe,    0,    2, "Demo Sounds"  },
	{0x13, 0x01, 0x80, 0x80, "Off" },
	{0x13, 0x01, 0x80, 0x00, "On" },

	{0x14, 0xfe,    0,    2, "Flip Screen"  },
	{0x14, 0x01, 0x01, 0x01, "Off" },
	{0x14, 0x01, 0x01, 0x00, "On" },

	{0x14, 0xfe,    0,    2, "Service Mode" },
	{0x14, 0x01, 0x04, 0x04, "Off" },
	{0x14, 0x01, 0x04, 0x00, "On" },

	{0x14, 0xfe,    0,    2, "Sound"  },
	{0x14, 0x01, 0x08, 0x00, "Mono" },
	{0x14, 0x01, 0x08, 0x08, "Stereo" },
};

STDDIPINFO(Drv)

static struct BurnDIPInfo CabinetDIPList[]=
{

	{0x13, 0xfe,    0,    2, "Cabinet"  },
	{0x13, 0x01, 0x04, 0x00, "Upright" },
	{0x13, 0x01, 0x04, 0x04, "Cocktail" },

	{0x14, 0xfe,    0,    2, "Upright Controls"  },
	{0x14, 0x02, 0x02, 0x02, "Single" },
	{0x13, 0x00, 0x04, 0x00, NULL},
	{0x14, 0x02, 0x02, 0x00, "Dual" },
	{0x13, 0x00, 0x04, 0x00, NULL},
};

STDDIPINFOEXT(Gryzor, Drv, Cabinet)

static void contra_K007121_ctrl_0_w(INT32 offset, INT32 data)
{
	k007121_ctrl_write(0, offset, data);
}

static void contra_K007121_ctrl_1_w(INT32 offset, INT32 data)
{
	k007121_ctrl_write(1, offset, data);
}

void contra_bankswitch_w(INT32 data)
{
	INT32 bankaddress = 0x10000 + (data & 0x0f) * 0x2000;

	if (bankaddress < 0x28000) {
		nBankData = data & 0x0f;
		HD6309MapMemory(DrvHD6309ROM0 + bankaddress, 0x6000, 0x7fff, MAP_ROM);
	}
}

static void sound_sync()
{
	INT32 cyc = HD6309TotalCycles() - M6809TotalCycles();
	if (cyc > 0) {
		BurnTimerUpdate(M6809TotalCycles() + cyc);
	}
}

UINT8 DrvContraHD6309ReadByte(UINT16 address)
{
	switch (address)
	{
		case 0x0008:
		case 0x0009:
		case 0x000A:
		case 0x000B:
		case 0x000C:
		case 0x000D:
		case 0x000E:
		case 0x000F:
			return K007452Read(address & 7);

		case 0x0010:
		case 0x0011:
		case 0x0012:
			return DrvInputs[address & 3];

		case 0x0014:
		case 0x0015:
		case 0x0016:
			return DrvDip[address & 3];
	}

	return 0;
}

void DrvContraHD6309WriteByte(UINT16 address, UINT8 data)
{
	if ((address & 0xff00) == 0x0c00) {
		INT32 offset = address & 0xff;

		DrvPalRAM[offset] = data;

		UINT16 col = DrvPalRAM[offset & ~1] | (DrvPalRAM[offset | 1] << 8);

		UINT8 r, g, b;

		r = (col >> 0) & 0x1f;
		r = (r << 3) | (r >> 2);

		g = (col >> 5) & 0x1f;
		g = (g << 3) | (g >> 2);

		b = (col >> 10) & 0x1f;
		b = (b << 3) | (b >> 2);

		UINT32 color = (r << 16) | (g << 8) | b;

		DrvRecalc = 1;
		Palette[offset >> 1] = color;

		return;
	}

	switch (address)
	{
		case 0x0000:
		case 0x0001:
		case 0x0002:
		case 0x0003:
		case 0x0004:
		case 0x0005:
		case 0x0006:
		case 0x0007:
			contra_K007121_ctrl_0_w(address, data);
		return;

		case 0x0008:
		case 0x0009:
		case 0x000A:
		case 0x000B:
		case 0x000C:
		case 0x000D:
		case 0x000E:
		case 0x000F:
			K007452Write(address & 7, data);
		return;

		case 0x0018:
			// coin counter
		return;

		case 0x001a:
			sound_sync();
			M6809SetIRQLine(0, CPU_IRQSTATUS_HOLD);
		return;

		case 0x001c:
			sound_sync();
			soundlatch = data;
		return;

		case 0x0060:
		case 0x0061:
		case 0x0062:
		case 0x0063:
		case 0x0064:
		case 0x0065:
		case 0x0066:
		case 0x0067:
			contra_K007121_ctrl_1_w(address, data);
		return;

		case 0x7000:
			contra_bankswitch_w(data);
		return;
	}
}

UINT8 DrvContraM6809SoundReadByte(UINT16 address)
{
	switch (address)
	{
		case 0x0000:
			return soundlatch;

		case 0x2001:
			return BurnYM2151Read();
	}

	return 0;
}

void DrvContraM6809SoundWriteByte(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x2000:
		case 0x2001:
			BurnYM2151Write(address & 1, data);
		return;

		case 0x4000:
			// nop
		return;
	}
}

static tilemap_callback( fg )
{
	offs &= 0x3ff;
	INT32 attr = DrvFgCRAM[offs];
	UINT8 ctrl_3 = k007121_ctrl_read(0, 3);
	UINT8 ctrl_4 = k007121_ctrl_read(0, 4);
	UINT8 ctrl_5 = k007121_ctrl_read(0, 5);
	UINT8 ctrl_6 = k007121_ctrl_read(0, 6);
	INT32 bit0 = (ctrl_5 >> 0) & 0x03;
	INT32 bit1 = (ctrl_5 >> 2) & 0x03;
	INT32 bit2 = (ctrl_5 >> 4) & 0x03;
	INT32 bit3 = (ctrl_5 >> 6) & 0x03;
	INT32 bank = ((attr & 0x80) >> 7) |
			((attr >> (bit0+2)) & 0x02) |
			((attr >> (bit1+1)) & 0x04) |
			((attr >> (bit2  )) & 0x08) |
			((attr >> (bit3-1)) & 0x10) |
			((ctrl_3 & 0x01) << 5);

	INT32 mask = (ctrl_4 & 0xf0) >> 4;

	bank = (bank & ~(mask << 1)) | ((ctrl_4 & mask) << 1);

	TILE_SET_INFO(0, DrvFgVRAM[offs] + (bank * 256), ((ctrl_6 & 0x30) * 2 + 16) + (attr & 7), 0);
}

static tilemap_callback( bg )
{
	offs &= 0x3ff;
	INT32 attr = DrvBgCRAM[offs];
	UINT8 ctrl_3 = k007121_ctrl_read(1, 3);
	UINT8 ctrl_4 = k007121_ctrl_read(1, 4);
	UINT8 ctrl_5 = k007121_ctrl_read(1, 5);
	UINT8 ctrl_6 = k007121_ctrl_read(1, 6);
	INT32 bit0 = (ctrl_5 >> 0) & 0x03;
	INT32 bit1 = (ctrl_5 >> 2) & 0x03;
	INT32 bit2 = (ctrl_5 >> 4) & 0x03;
	INT32 bit3 = (ctrl_5 >> 6) & 0x03;
	INT32 bank = ((attr & 0x80) >> 7) |
			((attr >> (bit0+2)) & 0x02) |
			((attr >> (bit1+1)) & 0x04) |
			((attr >> (bit2  )) & 0x08) |
			((attr >> (bit3-1)) & 0x10) |
			((ctrl_3 & 0x01) << 5);

	INT32 mask = (ctrl_4 & 0xf0) >> 4;

	bank = (bank & ~(mask << 1)) | ((ctrl_4 & mask) << 1);

	TILE_SET_INFO(1, DrvBgVRAM[offs] + (bank * 256), ((ctrl_6 & 0x30) * 2 + 16) + (attr & 7), 0);
}

static tilemap_callback( tx )
{
	offs &= 0x3ff;
	INT32 attr = DrvTxCRAM[offs];
	UINT8 ctrl_5 = k007121_ctrl_read(0, 5);
	UINT8 ctrl_6 = k007121_ctrl_read(0, 6);
	INT32 bit0 = (ctrl_5 >> 0) & 0x03;
	INT32 bit1 = (ctrl_5 >> 2) & 0x03;
	INT32 bit2 = (ctrl_5 >> 4) & 0x03;
	INT32 bit3 = (ctrl_5 >> 6) & 0x03;
	INT32 bank = ((attr & 0x80) >> 7) |
			((attr >> (bit0+2)) & 0x02) |
			((attr >> (bit1+1)) & 0x04) |
			((attr >> (bit2  )) & 0x08) |
			((attr >> (bit3-1)) & 0x10);

	TILE_SET_INFO(0, DrvTxVRAM[offs] + (bank * 256), ((ctrl_6 & 0x30) * 2 + 16) + (attr & 7), 0);
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvHD6309ROM0	= Next; Next += 0x030000;
	DrvM6809ROM0	= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += 0x100000;
	DrvGfxROM1		= Next; Next += 0x100000;

	DrvPROMs		= Next; Next += 0x000400;

	DrvColTable		= Next; Next += 0x001000;

	DrvPalette		= (UINT32*)Next; Next += 0x01000 * sizeof(UINT32);

	AllRam			= Next;

	DrvHD6309RAM0	= Next; Next += 0x001000;
	DrvHD6309RAM1	= Next; Next += 0x001800;
	DrvM6809RAM0	= Next; Next += 0x000800;
	DrvPalRAM		= Next; Next += 0x000100;
	DrvFgCRAM		= Next; Next += 0x000400;
	DrvFgVRAM		= Next; Next += 0x000400;
	DrvTxCRAM		= Next; Next += 0x000400;
	DrvTxVRAM		= Next; Next += 0x000400;
	DrvBgCRAM		= Next; Next += 0x000400;
	DrvBgVRAM		= Next; Next += 0x000400;
	DrvSprRAM[0]	= Next; Next += 0x001000;
	DrvSprRAM[1]	= Next; Next += 0x001000;

	Palette			= (UINT32*)Next; Next += 0x00080 * sizeof(UINT32);

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void DrvGfxExpand(UINT8 *src, INT32 len)
{
	for (INT32 i = (len - 1) * 2; i >= 0; i-=2) {
		src[i+0] = src[i/2] >> 4;
		src[i+1] = src[i/2] & 0xf;
	}
}

static INT32 DrvColorTableInit()
{
	for (INT32 chip = 0; chip < 2; chip++)
	{
		for (INT32 pal = 0; pal < 8; pal++)
		{
			INT32 clut = ((chip << 1) | (pal & 1)) << 8;

			for (INT32 i = 0; i < 0x100; i++)
			{
				UINT8 ctabentry;

				if (((pal & 0x01) == 0) && (DrvPROMs[clut | i] == 0))
					ctabentry = 0;
				else
					ctabentry = (pal << 4) | (DrvPROMs[clut | i] & 0x0f);

				DrvColTable[(chip << 11) | (pal << 8) | i] = ctabentry;
			}
		}
	}

	return 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	HD6309Open(0);
	HD6309Reset();
	HD6309Close();

	M6809Open(0);
	M6809Reset();
	BurnYM2151Reset();
	M6809Close();

	k007121_reset();

	K007452Reset();

	soundlatch = 0;
	nBankData = 0;
	nExtraCycles = 0;

	HiscoreReset();

	return 0;
}

static INT32 CommonInit(INT32 (*pRomLoad)())
{
	BurnAllocMemIndex();

	{
		if (pRomLoad) {
			if (pRomLoad()) return 1;
		}

		DrvGfxExpand(DrvGfxROM0, 0x80000);
		DrvGfxExpand(DrvGfxROM1, 0x80000);

		DrvColorTableInit();
	}

	HD6309Init(0);
	HD6309Open(0);
	HD6309MapMemory(DrvPalRAM,		0x0c00, 0x0cff, MAP_ROM);
	HD6309MapMemory(DrvHD6309RAM0,	0x1000, 0x1fff, MAP_RAM);
	HD6309MapMemory(DrvFgCRAM,		0x2000, 0x23ff, MAP_RAM);
	HD6309MapMemory(DrvFgVRAM,		0x2400, 0x27ff, MAP_RAM);
	HD6309MapMemory(DrvTxCRAM,		0x2800, 0x2bff, MAP_RAM);
	HD6309MapMemory(DrvTxVRAM,		0x2c00, 0x2fff, MAP_RAM);
	HD6309MapMemory(DrvSprRAM[0],	0x3000, 0x3fff, MAP_RAM);
	HD6309MapMemory(DrvBgCRAM,		0x4000, 0x43ff, MAP_RAM);
	HD6309MapMemory(DrvBgVRAM,		0x4400, 0x47ff, MAP_RAM);
	HD6309MapMemory(DrvHD6309RAM1,	0x4800, 0x4fff, MAP_RAM);
	HD6309MapMemory(DrvSprRAM[1],	0x5000, 0x5fff, MAP_RAM);
//	HD6309MapMemory(DrvHD6309ROM0 + 0x10000, 	0x6000, 0x7fff, MAP_ROM);
	HD6309MapMemory(DrvHD6309ROM0 + 0x08000,	0x8000, 0xffff, MAP_ROM);
	HD6309SetReadHandler(DrvContraHD6309ReadByte);
	HD6309SetWriteHandler(DrvContraHD6309WriteByte);
	HD6309Close();

	M6809Init(0);
	M6809Open(0);
	M6809MapMemory(DrvM6809RAM0, 		0x6000, 0x67ff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM0 + 0x08000,	0x8000, 0xffff, MAP_ROM);
	M6809SetReadHandler(DrvContraM6809SoundReadByte);
	M6809SetWriteHandler(DrvContraM6809SoundWriteByte);
	M6809Close();

	BurnYM2151InitBuffered(3579545, 1, NULL, 0);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.60, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.60, BURN_SND_ROUTE_RIGHT);
	BurnTimerAttachM6809(3000000);

	GenericTilesInit();

	k007121_init(0, (0x100000 / (8 * 8)) - 1, DrvSprRAM[0]);
	k007121_init(1, (0x100000 / (8 * 8)) - 1, DrvSprRAM[1]);

	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, fg_map_callback, 8, 8, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 32, 32);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, tx_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 4, 8, 8, 0x100000,     0, 0x7f);
	GenericTilemapSetGfx(1, DrvGfxROM1, 4, 8, 8, 0x100000, 0x800, 0x7f);
	GenericTilemapSetOffsets(0, 40, -16);
	GenericTilemapSetOffsets(1, 40, -16);
	GenericTilemapSetOffsets(2, 0, -16);
	GenericTilemapSetTransparent(0, 0);

	DrvDoReset();

	return 0;
}

static INT32 CommonRomLoad()
{
	if (BurnLoadRom(DrvHD6309ROM0 + 0x20000,  0, 1)) return 1;
	memcpy (DrvHD6309ROM0 + 0x08000, DrvHD6309ROM0 + 0x28000, 0x08000);
	if (BurnLoadRom(DrvHD6309ROM0 + 0x10000,  1, 1)) return 1;

	if (BurnLoadRom(DrvM6809ROM0 + 0x08000,  2, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x000000,   3, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x000001,   4, 2)) return 1;

	if (BurnLoadRom(DrvGfxROM1 + 0x000000,   5, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x000001,   6, 2)) return 1;

	if (BurnLoadRom(DrvPROMs   + 0x000000,   7, 1)) return 1;
	if (BurnLoadRom(DrvPROMs   + 0x000100,   8, 1)) return 1;
	if (BurnLoadRom(DrvPROMs   + 0x000200,   9, 1)) return 1;
	if (BurnLoadRom(DrvPROMs   + 0x000300,  10, 1)) return 1;

	return 0;
}

static INT32 BootlegRomLoad()
{
	if (BurnLoadRom(DrvHD6309ROM0 + 0x20000,  0, 1)) return 1;
	memcpy (DrvHD6309ROM0 + 0x08000, DrvHD6309ROM0 + 0x28000, 0x08000);
	if (BurnLoadRom(DrvHD6309ROM0 + 0x10000,  1, 1)) return 1;

	if (BurnLoadRom(DrvM6809ROM0 + 0x08000,  2, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM0   + 0x00000,  3, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM0   + 0x10000,  4, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM0   + 0x20000,  5, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM0   + 0x30000,  6, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM0   + 0x40000,  7, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM0   + 0x50000,  8, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM0   + 0x60000,  9, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM0   + 0x70000, 10, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM1   + 0x00000, 11, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1   + 0x10000, 12, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1   + 0x20000, 13, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1   + 0x30000, 14, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1   + 0x40000, 15, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1   + 0x50000, 16, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1   + 0x60000, 17, 1)) return 1;
//	70000-7ffff empty

	if (BurnLoadRom(DrvPROMs   + 0x000000,  18, 1)) return 1;
	if (BurnLoadRom(DrvPROMs   + 0x000100,  19, 1)) return 1;
	if (BurnLoadRom(DrvPROMs   + 0x000200,  20, 1)) return 1;
	if (BurnLoadRom(DrvPROMs   + 0x000300,  21, 1)) return 1;

	return 0;
}

static INT32 ContraeRomLoad()
{
	if (BurnLoadRom(DrvHD6309ROM0 + 0x20000,  0, 1)) return 1;
	memcpy (DrvHD6309ROM0 + 0x08000, DrvHD6309ROM0 + 0x28000, 0x08000);
	if (BurnLoadRom(DrvHD6309ROM0 + 0x10000,  1, 1)) return 1;

	if (BurnLoadRom(DrvM6809ROM0 + 0x08000, 2, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x000000,  3, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x020000,  4, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x040000,  5, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x060000,  6, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x000001,  7, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x020001,  8, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x040001,  9, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x060001, 10, 2)) return 1;
	
	if (BurnLoadRom(DrvGfxROM1 + 0x000000, 11, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x020000, 12, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x040000, 13, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x060000, 14, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x000001, 15, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x020001, 16, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x040001, 17, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x060001, 18, 2)) return 1;

	if (BurnLoadRom(DrvPROMs   + 0x000000, 19, 1)) return 1;
	if (BurnLoadRom(DrvPROMs   + 0x000100, 20, 1)) return 1;
	if (BurnLoadRom(DrvPROMs   + 0x000200, 21, 1)) return 1;
	if (BurnLoadRom(DrvPROMs   + 0x000300, 22, 1)) return 1;

	return 0;
}

static INT32 DrvInit() { return CommonInit(CommonRomLoad); }
static INT32 BootInit() { return CommonInit(BootlegRomLoad); }
static INT32 ContraeInit() { return CommonInit(ContraeRomLoad); }

static INT32 DrvExit()
{
	GenericTilesExit();

	HD6309Exit();
	M6809Exit();
	BurnYM2151Exit();

	BurnFreeMemIndex();

	return 0;
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x1000; i++) {
			INT32 rgb = Palette[DrvColTable[i]];
			DrvPalette[i] = BurnHighCol((rgb >> 16)&0xff, (rgb >> 8)&0xff, rgb&0xff, 0);
		}
		DrvRecalc = 0;
	}

	if (~nBurnLayer & 1) BurnTransferClear();

	UINT8 ctrl_1_0 = k007121_ctrl_read(0, 0);
	UINT8 ctrl_1_2 = k007121_ctrl_read(0, 2);
	UINT8 ctrl_2_0 = k007121_ctrl_read(1, 0);
	UINT8 ctrl_2_2 = k007121_ctrl_read(1, 2);
	UINT8 flipscreen1 = k007121_ctrl_read(0, 7)&0x08;
	UINT8 flipscreen2 = k007121_ctrl_read(1, 7)&0x08;

	GenericTilemapSetFlip(0, flipscreen1 ? TMAP_FLIPXY : 0);
	GenericTilemapSetFlip(1, flipscreen2 ? TMAP_FLIPXY : 0);
	GenericTilemapSetFlip(2, flipscreen1 ? TMAP_FLIPXY : 0);

	GenericTilemapSetScrollX(0, ctrl_1_0);
	GenericTilemapSetScrollY(0, ctrl_1_2);
	GenericTilemapSetScrollX(1, ctrl_2_0);
	GenericTilemapSetScrollY(1, ctrl_2_2);

	if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 0);
	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);

	INT32 base_color0 = (k007121_ctrl_read(0, 6) & 0x30) << 1;
	INT32 base_color1 = (k007121_ctrl_read(1, 6) & 0x30) << 1;
	if (nSpriteEnable & 1) k007121_draw(0, pTransDraw, DrvGfxROM0, DrvColTable, base_color0, 40, 16, 0, -1, 0x0000);
	if (nSpriteEnable & 2) k007121_draw(1, pTransDraw, DrvGfxROM1, DrvColTable, base_color1, 40, 16, 0, -1, 0x0800);

	GenericTilesSetClip((flipscreen1 ? 240 : -1), (flipscreen1 ? -1 : 40), -1, -1);
	if (nBurnLayer & 4) GenericTilemapDraw(2, pTransDraw, 0);
	GenericTilesClearClip();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	HD6309NewFrame();
	M6809NewFrame();

	{
		DrvInputs[0] = DrvInputs[1] = DrvInputs[2] = 0xff;

		for (INT32 i = 0 ; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		// Clear opposites
		if ((DrvInputs[1] & 0x03) == 0) DrvInputs[1] |= 0x03;
		if ((DrvInputs[1] & 0x0c) == 0) DrvInputs[1] |= 0x0c;
		if ((DrvInputs[2] & 0x03) == 0) DrvInputs[2] |= 0x03;
		if ((DrvInputs[2] & 0x0c) == 0) DrvInputs[2] |= 0x0c;
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] =  { 3000000 / 60, 3000000 / 60 };
	INT32 nCyclesDone[2] =  { nExtraCycles, 0 };

	HD6309Open(0);
	M6809Open(0);

	for (INT32 i = 0; i < nInterleave; i++) {
		CPU_RUN(0, HD6309);

		if (i == 240) {
			if (k007121_ctrl_read(0, 7) & 0x02) HD6309SetIRQLine(0, CPU_IRQSTATUS_HOLD);
			k007121_buffer(0);
			k007121_buffer(1);
		}

		CPU_RUN_TIMER(1);
	}

	nExtraCycles = nCyclesDone[0] - nCyclesTotal[0];

	M6809Close();
	HD6309Close();

	if (pBurnSoundOut) {
		M6809Open(0);
		BurnYM2151Render(pBurnSoundOut, nBurnSoundLen);
		M6809Close();
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
		*pnMin = 0x029696;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		HD6309Scan(nAction);
		M6809Scan(nAction);

		k007121_scan(nAction);
		K007452Scan(nAction);

		BurnYM2151Scan(nAction, pnMin);

		SCAN_VAR(soundlatch);
		SCAN_VAR(nBankData);
		SCAN_VAR(nExtraCycles);

		if (nAction & ACB_WRITE) {
			HD6309Open(0);
			contra_bankswitch_w(nBankData);
			HD6309Close();
		}
	}

	return 0;
}


// Contra (US / Asia, set 1)

static struct BurnRomInfo contraRomDesc[] = {
	{ "633m03.18a",		0x10000, 0xd045e1da, 1 | BRF_PRG  | BRF_ESS }, //  0 m6809 #0 Code
	{ "633i02.17a",		0x10000, 0xb2f7bd9a, 1 | BRF_PRG  | BRF_ESS }, //  1

	{ "633e01.12a",		0x08000, 0xd1549255, 2 | BRF_PRG  | BRF_ESS }, //  2 m6809 #1 Code

	{ "633e04.7d",		0x40000, 0x14ddc542, 3 | BRF_GRA },            //  3 Chip 0 Tiles
	{ "633e05.7f",		0x40000, 0x42185044, 3 | BRF_GRA },            //  4

	{ "633e06.16d",		0x40000, 0x9cf6faae, 4 | BRF_GRA },            //  5 Chip 1 Tiles
	{ "633e07.16f",		0x40000, 0xf2d06638, 4 | BRF_GRA },            //  6

	{ "633e08.10g",		0x00100, 0x9f0949fa, 5 | BRF_GRA },            //  7 Color Proms
	{ "633e09.12g",		0x00100, 0x14ca5e19, 5 | BRF_GRA },            //  8
	{ "633f10.18g",		0x00100, 0x2b244d84, 5 | BRF_GRA },            //  9
	{ "633f11.20g",		0x00100, 0x14ca5e19, 5 | BRF_GRA },            // 10
	
	{ "007766.20d.bin", 0x00001, 0x00000000, 0 | BRF_NODUMP }, 		   // 11 Pals
};

STD_ROM_PICK(contra)
STD_ROM_FN(contra)

struct BurnDriver BurnDrvContra = {
	"contra", NULL, NULL, NULL, "1987",
	"Contra (US / Asia, set 1)\0", NULL, "Konami", "GX633",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_RUNGUN, 0,
	NULL, contraRomInfo, contraRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 280, 3, 4
};


// Contra (US / Asia, set 2)

static struct BurnRomInfo contra1RomDesc[] = {
	{ "633i03.18a",		0x10000, 0x7fc0d8cf, 1 | BRF_PRG  | BRF_ESS }, //  0 m6809 #0 Code
	{ "633i02.17a",		0x10000, 0xb2f7bd9a, 1 | BRF_PRG  | BRF_ESS }, //  1

	{ "633e01.12a",		0x08000, 0xd1549255, 2 | BRF_PRG  | BRF_ESS }, //  2 m6809 #1 Code

	{ "633e04.7d",		0x40000, 0x14ddc542, 3 | BRF_GRA },            //  3 Chip 0 Tiles
	{ "633e05.7f",		0x40000, 0x42185044, 3 | BRF_GRA },            //  4

	{ "633e06.16d",		0x40000, 0x9cf6faae, 4 | BRF_GRA },            //  5 Chip 1 Tiles
	{ "633e07.16f",		0x40000, 0xf2d06638, 4 | BRF_GRA },            //  6

	{ "633e08.10g",		0x00100, 0x9f0949fa, 5 | BRF_GRA },            //  7 Color Proms
	{ "633e09.12g",		0x00100, 0x14ca5e19, 5 | BRF_GRA },            //  8
	{ "633f10.18g",		0x00100, 0x2b244d84, 5 | BRF_GRA },            //  9
	{ "633f11.20g",		0x00100, 0x14ca5e19, 5 | BRF_GRA },            // 10
	
	{ "007766.20d.bin", 0x00001, 0x00000000, 0 | BRF_NODUMP }, 		   // 11 Pals
};

STD_ROM_PICK(contra1)
STD_ROM_FN(contra1)

struct BurnDriver BurnDrvContra1 = {
	"contra1", "contra", NULL, NULL, "1987",
	"Contra (US / Asia, set 2)\0", NULL, "Konami", "GX633",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_RUNGUN, 0,
	NULL, contra1RomInfo, contra1RomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 280, 3, 4
};


// Contra (US / Asia, set 3)

static struct BurnRomInfo contraeRomDesc[] = {
	{ "633_e03.18a",	0x10000, 0x7ebdb314, 1 | BRF_PRG  | BRF_ESS }, //  0 m6809 #0 Code
	{ "633_e02.17a",	0x10000, 0x9d5ebe66, 1 | BRF_PRG  | BRF_ESS }, //  1

	{ "633e01.12a",		0x08000, 0xd1549255, 2 | BRF_PRG  | BRF_ESS }, //  2 m6809 #1 Code

	// this PCB used official Konami riser-boards in place of the mask roms
	{ "633_e04_a.7d",	0x10000, 0xe027f330, 3 | BRF_GRA },            //  3 Chip 0 Tiles
	{ "633_e04_b.7d",	0x10000, 0xa71230f5, 3 | BRF_GRA },            //  4
	{ "633_e04_c.7d",	0x10000, 0x0b103d01, 3 | BRF_GRA },            //  5
	{ "633_e04_d.7d",	0x10000, 0xab3faa60, 3 | BRF_GRA },            //  6
	{ "633_e05_a.7f",	0x10000, 0x81a70a77, 3 | BRF_GRA },            //  7
	{ "633_e05_b.7f",	0x10000, 0x55556f29, 3 | BRF_GRA },            //  8
	{ "633_e05_c.7f",	0x10000, 0xacba86bf, 3 | BRF_GRA },            //  9
	{ "633_e05_d.7f",	0x10000, 0x59cf234d, 3 | BRF_GRA },            // 10

	{ "633_e06_a.16d",	0x10000, 0x030079c5, 4 | BRF_GRA },            // 11 Chip 1 Tiles
	{ "633_e06_b.16d",	0x10000, 0xe17d5807, 4 | BRF_GRA },            // 12
	{ "633_e06_c.16d",	0x10000, 0x7d6a28cd, 4 | BRF_GRA },            // 13
	{ "633_e06_d.16d",	0x10000, 0x83db378f, 4 | BRF_GRA },            // 14 
	{ "633_e07_a.16f",	0x10000, 0x8fcd40e5, 4 | BRF_GRA },            // 15
	{ "633_e07_b.16f",	0x10000, 0x694e306e, 4 | BRF_GRA },            // 16
	{ "633_e07_c.16f",	0x10000, 0xfb33f3ff, 4 | BRF_GRA },            // 17
	{ "633_e07_d.16f",	0x10000, 0xcfab0988, 4 | BRF_GRA },            // 18

	{ "633e08.10g",		0x00100, 0x9f0949fa, 5 | BRF_GRA },            // 19 Color Proms
	{ "633e09.12g",		0x00100, 0x14ca5e19, 5 | BRF_GRA },            // 20
	{ "633e10.18g",		0x00100, 0xe782c494, 5 | BRF_GRA },            // 21
	{ "633e11.20g",		0x00100, 0x14ca5e19, 5 | BRF_GRA },            // 22
	
	{ "007766.20d.bin", 0x00001, 0x00000000, 0 | BRF_NODUMP }, 		   // 23 Pals
};

STD_ROM_PICK(contrae)
STD_ROM_FN(contrae)

struct BurnDriver BurnDrvContrae = {
	"contrae", "contra", NULL, NULL, "1987",
	"Contra (US / Asia, set 3)\0", NULL, "Konami", "GX633",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_RUNGUN, 0,
	NULL, contraeRomInfo, contraeRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	ContraeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 280, 3, 4
};


// Contra (Japan, set 1)

static struct BurnRomInfo contrajRomDesc[] = {
	{ "633n03.18a",		0x10000, 0xfedab568, 1 | BRF_PRG  | BRF_ESS }, //  0 m6809 #0 Code
	{ "633k02.17a",		0x10000, 0x5d5f7438, 1 | BRF_PRG  | BRF_ESS }, //  1

	{ "633e01.12a",		0x08000, 0xd1549255, 2 | BRF_PRG  | BRF_ESS }, //  2 m6809 #1 Code

	{ "633e04.7d",		0x40000, 0x14ddc542, 3 | BRF_GRA },            //  3 Chip 0 Tiles
	{ "633e05.7f",		0x40000, 0x42185044, 3 | BRF_GRA },            //  4

	{ "633e06.16d",		0x40000, 0x9cf6faae, 4 | BRF_GRA },            //  5 Chip 1 Tiles
	{ "633e07.16f",		0x40000, 0xf2d06638, 4 | BRF_GRA },            //  6

	{ "633e08.10g",		0x00100, 0x9f0949fa, 5 | BRF_GRA },            //  7 Color Proms
	{ "633e09.12g",		0x00100, 0x14ca5e19, 5 | BRF_GRA },            //  8
	{ "633f10.18g",		0x00100, 0x2b244d84, 5 | BRF_GRA },            //  9
	{ "633f11.20g",		0x00100, 0x14ca5e19, 5 | BRF_GRA },            // 10
	
	{ "007766.20d.bin", 0x00001, 0x00000000, 0 | BRF_NODUMP }, 		   // 11 Pals
};

STD_ROM_PICK(contraj)
STD_ROM_FN(contraj)

struct BurnDriver BurnDrvContraj = {
	"contraj", "contra", NULL, NULL, "1987",
	"Contra (Japan, set 1)\0", NULL, "Konami", "GX633",
	L"\u9B42\u6597\u7F85 \u30B3\u30F3\u30C8\u30E9 (Japan, set 1)\0Contra\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_RUNGUN, 0,
	NULL, contrajRomInfo, contrajRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 280, 3, 4
};


// Contra (Japan, set 2)

static struct BurnRomInfo contraj1RomDesc[] = {
	{ "633k03.18a",		0x10000, 0xbdb9196d, 1 | BRF_PRG  | BRF_ESS }, //  0 m6809 #0 Code
	{ "633k02.17a",		0x10000, 0x5d5f7438, 1 | BRF_PRG  | BRF_ESS }, //  1

	{ "633e01.12a",		0x08000, 0xd1549255, 2 | BRF_PRG  | BRF_ESS }, //  2 m6809 #1 Code

	{ "633e04.7d",		0x40000, 0x14ddc542, 3 | BRF_GRA },            //  3 Chip 0 Tiles
	{ "633e05.7f",		0x40000, 0x42185044, 3 | BRF_GRA },            //  4

	{ "633e06.16d",		0x40000, 0x9cf6faae, 4 | BRF_GRA },            //  5 Chip 1 Tiles
	{ "633e07.16f",		0x40000, 0xf2d06638, 4 | BRF_GRA },            //  6

	{ "633e08.10g",		0x00100, 0x9f0949fa, 5 | BRF_GRA },            //  7 Color Proms
	{ "633e09.12g",		0x00100, 0x14ca5e19, 5 | BRF_GRA },            //  8
	{ "633f10.18g",		0x00100, 0x2b244d84, 5 | BRF_GRA },            //  9
	{ "633f11.20g",		0x00100, 0x14ca5e19, 5 | BRF_GRA },            // 10
	
	{ "007766.20d.bin", 0x00001, 0x00000000, 0 | BRF_NODUMP }, 		   // 11 Pals
};

STD_ROM_PICK(contraj1)
STD_ROM_FN(contraj1)

struct BurnDriver BurnDrvContraj1 = {
	"contraj1", "contra", NULL, NULL, "1987",
	"Contra (Japan, set 2)\0", NULL, "Konami", "GX633",
	L"\u9B42\u6597\u7F85 \u30B3\u30F3\u30C8\u30E9 (Japan, set 2)\0Contra\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_RUNGUN, 0,
	NULL, contraj1RomInfo, contraj1RomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 280, 3, 4
};


// Gryzor (set 1)

static struct BurnRomInfo gryzorRomDesc[] = {
	{ "633j03.18a",		0x10000, 0x20919162, 1 | BRF_PRG  | BRF_ESS }, //  0 m6809 #0 Code
	{ "633j02.17a",		0x10000, 0xb5922f9a, 1 | BRF_PRG  | BRF_ESS }, //  1

	{ "633e01.12a",		0x08000, 0xd1549255, 2 | BRF_PRG  | BRF_ESS }, //  2 m6809 #1 Code

	{ "633e04.7d",		0x40000, 0x14ddc542, 3 | BRF_GRA },            //  3 Chip 0 Tiles
	{ "633e05.7f",		0x40000, 0x42185044, 3 | BRF_GRA },            //  4

	{ "633e06.16d",		0x40000, 0x9cf6faae, 4 | BRF_GRA },            //  5 Chip 1 Tiles
	{ "633e07.16f",		0x40000, 0xf2d06638, 4 | BRF_GRA },            //  6

	{ "633e08.10g",		0x00100, 0x9f0949fa, 5 | BRF_GRA },            //  7 Color Proms
	{ "633e09.12g",		0x00100, 0x14ca5e19, 5 | BRF_GRA },            //  8
	{ "633f10.18g",		0x00100, 0x2b244d84, 5 | BRF_GRA },            //  9
	{ "633f11.20g",		0x00100, 0x14ca5e19, 5 | BRF_GRA },            // 10
	
	{ "007766.20d.bin", 0x00001, 0x00000000, 0 | BRF_NODUMP }, 		   // 11 Pals
};

STD_ROM_PICK(gryzor)
STD_ROM_FN(gryzor)

struct BurnDriver BurnDrvGryzor = {
	"gryzor", "contra", NULL, NULL, "1987",
	"Gryzor (set 1)\0", NULL, "Konami", "GX633",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_RUNGUN, 0,
	NULL, gryzorRomInfo, gryzorRomName, NULL, NULL, NULL, NULL, DrvInputInfo, GryzorDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 280, 3, 4
};


// Gryzor (set 2)

static struct BurnRomInfo gryzor1RomDesc[] = {
	{ "633g2.18a",		0x10000, 0x92ca77bd, 1 | BRF_PRG  | BRF_ESS }, //  0 m6809 #0 Code
	{ "633g3.17a",		0x10000, 0xbbd9e95e, 1 | BRF_PRG  | BRF_ESS }, //  1

	{ "633e01.12a",		0x08000, 0xd1549255, 2 | BRF_PRG  | BRF_ESS }, //  2 m6809 #1 Code

	{ "633e04.7d",		0x40000, 0x14ddc542, 3 | BRF_GRA },            //  3 Chip 0 Tiles
	{ "633e05.7f",		0x40000, 0x42185044, 3 | BRF_GRA },            //  4

	{ "633e06.16d",		0x40000, 0x9cf6faae, 4 | BRF_GRA },            //  5 Chip 1 Tiles
	{ "633e07.16f",		0x40000, 0xf2d06638, 4 | BRF_GRA },            //  6

	{ "633e08.10g",		0x00100, 0x9f0949fa, 5 | BRF_GRA },            //  7 Color Proms
	{ "633e09.12g",		0x00100, 0x14ca5e19, 5 | BRF_GRA },            //  8
	{ "633f10.18g",		0x00100, 0x2b244d84, 5 | BRF_GRA },            //  9
	{ "633f11.20g",		0x00100, 0x14ca5e19, 5 | BRF_GRA },            // 10
	
	{ "007766.20d.bin", 0x00001, 0x00000000, 0 | BRF_NODUMP }, 		   // 11 Pals
};

STD_ROM_PICK(gryzor1)
STD_ROM_FN(gryzor1)

struct BurnDriver BurnDrvGryzor1 = {
	"gryzor1", "contra", NULL, NULL, "1987",
	"Gryzor (set 2)\0", NULL, "Konami", "GX633",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_RUNGUN, 0,
	NULL, gryzor1RomInfo, gryzor1RomName, NULL, NULL, NULL, NULL, DrvInputInfo, GryzorDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 280, 3, 4
};


// Contra (bootleg)

static struct BurnRomInfo contrabRomDesc[] = {
	{ "3.ic20",			0x10000, 0xd045e1da, 1 | BRF_PRG  | BRF_ESS }, //  0 m6809 #0 Code
	{ "1.ic19",			0x10000, 0xb2f7bd9a, 1 | BRF_PRG  | BRF_ESS }, //  1

	{ "3.ic63",			0x08000, 0xd1549255, 2 | BRF_PRG  | BRF_ESS }, //  2 m6809 #1 Code

	/* bootleg versions use smaller gfx ROMs, but the data is the same */
	{ "7.rom",			0x10000, 0x57f467d2, 3 | BRF_GRA },            //  3 Chip 0 Tiles
	{ "10.rom",			0x10000, 0xe6db9685, 3 | BRF_GRA },            //  4
	{ "9.rom",			0x10000, 0x875c61de, 3 | BRF_GRA },            //  5
	{ "8.rom",			0x10000, 0x642765d6, 3 | BRF_GRA },            //  6
	{ "15.rom",			0x10000, 0xdaa2324b, 3 | BRF_GRA },            //  7
	{ "16.rom",			0x10000, 0xe27cc835, 3 | BRF_GRA },            //  8
	{ "17.rom",			0x10000, 0xce4330b9, 3 | BRF_GRA },            //  9
	{ "18.rom",			0x10000, 0x1571ce42, 3 | BRF_GRA },            // 10

	/* bootleg versions use smaller gfx ROMs, but the data is the same */
	{ "4.rom",			0x10000, 0x2cc7e52c, 4 | BRF_GRA },            // 11 Chip 1 Tiles
	{ "5.rom",			0x10000, 0xe01a5b9c, 4 | BRF_GRA },            // 12
	{ "6.rom",			0x10000, 0xaeea6744, 4 | BRF_GRA },            // 13
	{ "14.rom",			0x10000, 0x765afdc7, 4 | BRF_GRA },            // 14
	{ "11.rom",			0x10000, 0xbd9ba92c, 4 | BRF_GRA },            // 15
	{ "12.rom",			0x10000, 0xd0be7ec2, 4 | BRF_GRA },            // 16
	{ "13.rom",			0x10000, 0x2b513d12, 4 | BRF_GRA },            // 17

	{ "633e08.10g",		0x00100, 0x9f0949fa, 5 | BRF_GRA },            // 18 Color Proms
	{ "633e09.12g",		0x00100, 0x14ca5e19, 5 | BRF_GRA },            // 19
	{ "633f10.18g",		0x00100, 0x2b244d84, 5 | BRF_GRA },            // 20
	{ "633f11.20g",		0x00100, 0x14ca5e19, 5 | BRF_GRA },            // 21

	{ "conprom.53",		0x00100, 0x05a1da7e, 0 | BRF_OPT },            // 22 unknown
};

STD_ROM_PICK(contrab)
STD_ROM_FN(contrab)

struct BurnDriver BurnDrvContrab = {
	"contrab", "contra", NULL, NULL, "1987",
	"Contra (bootleg)\0", NULL, "bootleg", "GX633",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_RUNGUN, 0,
	NULL, contrabRomInfo, contrabRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	BootInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 280, 3, 4
};


// Contra (Japan bootleg, set 1)

static struct BurnRomInfo contrabjRomDesc[] = {
	{ "2.2k",			0x10000, 0xfedab568, 1 | BRF_PRG  | BRF_ESS }, //  0 m6809 #0 Code
	{ "1.2h",			0x10000, 0x5d5f7438, 1 | BRF_PRG  | BRF_ESS }, //  1

	{ "a3.4p",			0x08000, 0xd1549255, 2 | BRF_PRG  | BRF_ESS }, //  2 m6809 #1 Code

	/* bootleg versions use smaller gfx ROMs, but the data is the same */
	{ "a7.14f",			0x10000, 0x57f467d2, 3 | BRF_GRA },            //  3 Chip 0 Tiles
	{ "a10.14l",		0x10000, 0xe6db9685, 3 | BRF_GRA },            //  4
	{ "a9.14k",			0x10000, 0x875c61de, 3 | BRF_GRA },            //  5
	{ "a8.14h",			0x10000, 0x642765d6, 3 | BRF_GRA },            //  6
	{ "a15.14r",		0x10000, 0xdaa2324b, 3 | BRF_GRA },            //  7
	{ "a16.14t",		0x10000, 0xe27cc835, 3 | BRF_GRA },            //  8
	{ "a17.14v",		0x10000, 0xce4330b9, 3 | BRF_GRA },            //  9
	{ "a18.14w",		0x10000, 0x1571ce42, 3 | BRF_GRA },            // 10

	/* bootleg versions use smaller gfx ROMs, but the data is the same */
	{ "a4.14a",			0x10000, 0x2cc7e52c, 4 | BRF_GRA },            // 11 Chip 1 Tiles
	{ "a5.14c",			0x10000, 0xe01a5b9c, 4 | BRF_GRA },            // 12
	{ "e6.14d",			0x10000, 0xaeea6744, 4 | BRF_GRA },            // 13
	{ "a14.14q",		0x10000, 0x765afdc7, 4 | BRF_GRA },            // 14
	{ "a11.14m",		0x10000, 0xbd9ba92c, 4 | BRF_GRA },            // 15
	{ "a12.14n",		0x10000, 0xd0be7ec2, 4 | BRF_GRA },            // 16
	{ "a13.14p",		0x10000, 0x2b513d12, 4 | BRF_GRA },            // 17

	{ "633e08.10g",		0x00100, 0x9f0949fa, 5 | BRF_GRA },            // 18 Color Proms
	{ "633e09.12g",		0x00100, 0x14ca5e19, 5 | BRF_GRA },            // 19
	{ "633f10.18g",		0x00100, 0x2b244d84, 5 | BRF_GRA },            // 20
	{ "633f11.20g",		0x00100, 0x14ca5e19, 5 | BRF_GRA },            // 21
};

STD_ROM_PICK(contrabj)
STD_ROM_FN(contrabj)

struct BurnDriver BurnDrvContrabj = {
	"contrabj", "contra", NULL, NULL, "1987",
	"Contra (Japan bootleg, set 1)\0", NULL, "bootleg", "GX633",
	L"\u9B42\u6597\u7F85 \u30B3\u30F3\u30C8\u30E9 (Japan bootleg, set 1)\0Contra\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_RUNGUN, 0,
	NULL, contrabjRomInfo, contrabjRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	BootInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 280, 3, 4
};


// Contra (Japan bootleg, set 2)

static struct BurnRomInfo contrabj1RomDesc[] = {
	{ "2__,contrabtj2.2k",	0x10000, 0xbdb9196d, 1 | BRF_PRG  | BRF_ESS }, //  0 m6809 #0 Code
	{ "1.2h",				0x10000, 0x5d5f7438, 1 | BRF_PRG  | BRF_ESS }, //  1

	{ "a3.4p",			0x08000, 0xd1549255, 2 | BRF_PRG  | BRF_ESS }, //  2 m6809 #1 Code

	/* bootleg versions use smaller gfx ROMs, but the data is the same */
	{ "a7.14f",			0x10000, 0x57f467d2, 3 | BRF_GRA },            //  3 Chip 0 Tiles
	{ "a10.14l",		0x10000, 0xe6db9685, 3 | BRF_GRA },            //  4
	{ "a9.14k",			0x10000, 0x875c61de, 3 | BRF_GRA },            //  5
	{ "a8.14h",			0x10000, 0x642765d6, 3 | BRF_GRA },            //  6
	{ "a15.14r",		0x10000, 0xdaa2324b, 3 | BRF_GRA },            //  7
	{ "a16.14t",		0x10000, 0xe27cc835, 3 | BRF_GRA },            //  8
	{ "a17.14v",		0x10000, 0xce4330b9, 3 | BRF_GRA },            //  9
	{ "a18.14w",		0x10000, 0x1571ce42, 3 | BRF_GRA },            // 10

	/* bootleg versions use smaller gfx ROMs, but the data is the same */
	{ "a4.14a",			0x10000, 0x2cc7e52c, 4 | BRF_GRA },            // 11 Chip 1 Tiles
	{ "a5.14c",			0x10000, 0xe01a5b9c, 4 | BRF_GRA },            // 12
	{ "e6.14d",			0x10000, 0xaeea6744, 4 | BRF_GRA },            // 13
	{ "a14.14q",		0x10000, 0x765afdc7, 4 | BRF_GRA },            // 14
	{ "a11.14m",		0x10000, 0xbd9ba92c, 4 | BRF_GRA },            // 15
	{ "a12.14n",		0x10000, 0xd0be7ec2, 4 | BRF_GRA },            // 16
	{ "a13.14p",		0x10000, 0x2b513d12, 4 | BRF_GRA },            // 17

	{ "633e08.10g",		0x00100, 0x9f0949fa, 5 | BRF_GRA },            // 18 Color Proms
	{ "633e09.12g",		0x00100, 0x14ca5e19, 5 | BRF_GRA },            // 19
	{ "633f10.18g",		0x00100, 0x2b244d84, 5 | BRF_GRA },            // 20
	{ "633f11.20g",		0x00100, 0x14ca5e19, 5 | BRF_GRA },            // 21
};

STD_ROM_PICK(contrabj1)
STD_ROM_FN(contrabj1)

struct BurnDriver BurnDrvContrabj1 = {
	"contrabj1", "contra", NULL, NULL, "1987",
	"Contra (Japan bootleg, set 2)\0", NULL, "bootleg", "GX633",
	L"\u9B42\u6597\u7F85 \u30B3\u30F3\u30C8\u30E9 (Japan bootleg, set 2)\0Contra\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_RUNGUN, 0,
	NULL, contrabj1RomInfo, contrabj1RomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	BootInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 280, 3, 4
};
