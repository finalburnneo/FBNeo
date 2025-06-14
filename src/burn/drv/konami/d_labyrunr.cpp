// FB Alpha Labyrinth Runner driver module
// Based on MAME driver by Nicola Salmoria

#include "tiles_generic.h"
#include "hd6309_intf.h"
#include "burn_ym2203.h"
#include "konamiic.h"
#include "k007121.h"
#include "rectangle.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvHD6309ROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvLutPROM;
static UINT8 *DrvLookUpTable;
static UINT8 *DrvSprTranspLut;
static UINT8 *DrvHD6309RAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvVidRAM[2];
static UINT8 *DrvScrollRAM;
static UINT8 *DrvTransTab;

static UINT32 *DrvPalette;
static UINT8  DrvRecalc;

static INT32 HD6309Bank;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static INT32 watchdog = 0;

static struct BurnInputInfo LabyrunrInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Labyrunr)

static struct BurnDIPInfo LabyrunrDIPList[]=
{
	DIP_OFFSET(0x12)
	{0x00, 0xff, 0xff, 0xff, NULL					},
	{0x01, 0xff, 0xff, 0x5e, NULL					},
	{0x02, 0xff, 0xff, 0xf7, NULL					},

	{0   , 0xfe, 0   ,    16, "Coin A"				},
	{0x00, 0x01, 0x0f, 0x02, "4 Coins 1 Credits"	},
	{0x00, 0x01, 0x0f, 0x05, "3 Coins 1 Credits"	},
	{0x00, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"	},
	{0x00, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x00, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x00, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x00, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x00, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x00, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x00, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x00, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x00, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x00, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x00, 0x01, 0x0f, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    16, "Coin B"				},
	{0x00, 0x01, 0xf0, 0x20, "4 Coins 1 Credits"	},
	{0x00, 0x01, 0xf0, 0x50, "3 Coins 1 Credits"	},
	{0x00, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"	},
	{0x00, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"	},
	{0x00, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"	},
	{0x00, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"	},
	{0x00, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"	},
	{0x00, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x00, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x00, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x00, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x00, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x00, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x00, 0x01, 0xf0, 0x00, "None"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x01, 0x01, 0x03, 0x03, "2"					},
	{0x01, 0x01, 0x03, 0x02, "3"					},
	{0x01, 0x01, 0x03, 0x01, "5"					},
	{0x01, 0x01, 0x03, 0x00, "7"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x01, 0x01, 0x04, 0x00, "Upright"				},
	{0x01, 0x01, 0x04, 0x04, "Cocktail"				},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x01, 0x01, 0x18, 0x18, "30000 70000"			},
	{0x01, 0x01, 0x18, 0x10, "40000 80000"			},
	{0x01, 0x01, 0x18, 0x08, "40000"				},
	{0x01, 0x01, 0x18, 0x00, "50000"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x01, 0x01, 0x60, 0x60, "Easy"					},
	{0x01, 0x01, 0x60, 0x40, "Normal"				},
	{0x01, 0x01, 0x60, 0x20, "Hard"					},
	{0x01, 0x01, 0x60, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x01, 0x01, 0x80, 0x80, "Off"					},
	{0x01, 0x01, 0x80, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x02, 0x01, 0x01, 0x01, "Off"					},
	{0x02, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Upright Controls"		},
	{0x02, 0x01, 0x02, 0x02, "Single"				},
	{0x02, 0x01, 0x02, 0x00, "Dual"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x02, 0x01, 0x04, 0x04, "Off"					},
	{0x02, 0x01, 0x04, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x02, 0x01, 0x08, 0x08, "3 Times"				},
	{0x02, 0x01, 0x08, 0x00, "5 Times"				},
};

STDDIPINFO(Labyrunr)

static void bankswitch(INT32 data)
{
	HD6309Bank = data;
	HD6309MapMemory(DrvHD6309ROM + 0x10000 + ((HD6309Bank & 7) * 0x4000), 0x4000, 0x7fff, MAP_ROM);
}

static void labyrunr_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfff8) == 0x0000) { // 0x0000 - 0x0007
		k007121_ctrl_write(0, address & 7, data);
		return;
	}

	if (address >= 0x0020 && address <= 0x005f) {
		DrvScrollRAM[address - 0x0020] = data;
		return;
	}

	if ((address & 0xffe0) == 0x0d00) {
		K051733Write(address & 0x1f, data);
		return;
	}

	if ((address & 0xff00) == 0x1000) {	// Speed-up
		if (data != DrvPalRAM[address & 0xff]) {
			DrvPalRAM[address & 0xff] = data;
			DrvRecalc = 1;
		}
		return;
	}

	switch (address)
	{
		case 0x0800:
		case 0x0801:
		case 0x0900:
		case 0x0901:
			BurnYM2203Write((address >> 8) & 1, ~address & 1, data);
		return;

		case 0x0c00:
			bankswitch(data);
		return;

		case 0x0e00:
			watchdog = 0;
		return;
	}
}

static UINT8 labyrunr_read(UINT16 address)
{
	if ((address & 0xfff8) == 0x0000) {
		return k007121_ctrl_read(0, address & 7);
	}

	if (address >= 0x0020 && address <= 0x005f) {
		return DrvScrollRAM[address - 0x0020];
	}

	if ((address & 0xffe0) == 0x0d00) {
		return K051733Read(address & 0x1f);
	}

	switch (address)
	{
		case 0x0800:
		case 0x0801:
		case 0x0900:
		case 0x0901:
			return BurnYM2203Read((address >> 8) & 1, ~address & 1);

		case 0x0a00:
			return DrvInputs[1];

		case 0x0a01:
			return DrvInputs[0];

		case 0x0b00:
			return DrvInputs[2];
	}

	return 0;
}

static UINT8 ym2203_0_read_portA(UINT32 )
{
	return DrvDips[0];
}

static UINT8 ym2203_0_read_portB(UINT32 )
{
	return DrvDips[1];
}

static UINT8 ym2203_1_read_portB(UINT32 )
{
	return DrvDips[2];
}

template <int chip>
static tilemap_callback( tile )
{
	UINT8 ctrl_3 = k007121_ctrl_read(chip, 3);
	UINT8 ctrl_4 = k007121_ctrl_read(chip, 4);
	UINT8 ctrl_5 = k007121_ctrl_read(chip, 5);
	UINT8 ctrl_6 = k007121_ctrl_read(chip, 6);
	INT32 attr = DrvVidRAM[chip][offs];
	INT32 code = DrvVidRAM[chip][offs + 0x400];
	INT32 bit0 = (ctrl_5 >> 0) & 0x03;
	INT32 bit1 = (ctrl_5 >> 2) & 0x03;
	INT32 bit2 = (ctrl_5 >> 4) & 0x03;
	INT32 bit3 = (ctrl_5 >> 6) & 0x03;
	INT32 bank = ((attr & 0x80) >> 7) |
			((attr >> (bit0 + 2)) & 0x02) |
			((attr >> (bit1 + 1)) & 0x04) |
			((attr >> (bit2    )) & 0x08) |
			((attr >> (bit3 - 1)) & 0x10) |
			((ctrl_3 & 0x01) << 5);
	INT32 mask = (ctrl_4 & 0xf0) >> 4;

	bank = (bank & ~(mask << 1)) | ((ctrl_4 & mask) << 1);

	TILE_SET_INFO(0, code + bank * 256, ((ctrl_6 & 0x30) * 2 + 16) + (attr & 7), (chip ? 0 : TILE_GROUP((attr & 0x40) >> 6)));
}

static INT32 DrvDoReset(INT32 full_reset)
{
	if (full_reset) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	HD6309Open(0);
	HD6309Reset();
	bankswitch(0);
	BurnYM2203Reset();
	HD6309Close();

	k007121_reset();

	K051733Reset();

	watchdog = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvHD6309ROM    = Next; Next += 0x028000;

	DrvGfxROM       = Next; Next += 0x080000;

	DrvLutPROM      = Next; Next += 0x000100;

	DrvLookUpTable  = Next; Next += 0x000800;
	DrvSprTranspLut = Next; Next += 0x000800;
	DrvTransTab     = Next; Next += 0x002000;

	DrvPalette      = (UINT32*)Next; Next += 0x801 * sizeof(UINT32);

	AllRam          = Next;

	DrvHD6309RAM    = Next; Next += 0x000800;
	DrvPalRAM       = Next; Next += 0x000100;

	DrvSprRAM       = Next; Next += 0x001000;
	DrvVidRAM[0]    = Next; Next += 0x000800;
	DrvVidRAM[1]    = Next; Next += 0x000800;
	DrvScrollRAM    = Next; Next += 0x000040;

	RamEnd          = Next;
	MemEnd          = Next;

	return 0;
}

static void DrvGfxExpand(UINT8 *src, INT32 len)
{
	for (INT32 i = (len - 1) * 2; i >= 0; i-=2) {
		src[i+0] = src[i/2] >> 4;
		src[i+1] = src[i/2] & 0xf;
	}

	for (INT32 i = 0; i < len * 2; i+=0x40) {
		DrvTransTab[i/0x40] = 1;
		for (INT32 j = 0; j < 0x40; j++) {
			if (src[i+j]) {
				DrvTransTab[i/0x40] = 0;
				break;
			}
		}
	}
}

static void DrvExpandLookupTable()
{
	// Calculate color lookup tables
	for (INT32 pal = 0; pal < 8; pal+=2)
	{
		for (INT32 i = 0; i < 0x100; i++)
		{
			DrvLookUpTable[((pal+1) << 8) | i] = ((pal+1) << 4) | (i & 0xf);
			DrvLookUpTable[((pal+0) << 8) | i] = ((DrvLutPROM[i] == 0) ? 0 : ((pal << 4) | (DrvLutPROM[i] & 0x0f)));
		}
	}

	// Calculate sprite transparency lookups
	for (INT32 i = 0; i < 0x800; i++) {
		DrvSprTranspLut[i] = DrvLookUpTable[i] & 0xf;
	}
}

static INT32 CommonInit(INT32 nLoadType)
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvHD6309ROM + 0x10000,  0, 1)) return 1;
		memcpy (DrvHD6309ROM, DrvHD6309ROM + 0x18000, 0x08000);
		if (BurnLoadRom(DrvHD6309ROM + 0x18000,  1, 1)) return 1;

		if (nLoadType == 0) {
			if (BurnLoadRom(DrvGfxROM + 0x00001,     2, 2)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x00000,     3, 2)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x20001,     4, 2)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x20000,     5, 2)) return 1;

			if (BurnLoadRom(DrvLutPROM,              6, 1)) return 1;
		} else {
			if (BurnLoadRom(DrvGfxROM + 0x00000,     2, 1)) return 1;
			BurnByteswap(DrvGfxROM, 0x40000);

			if (BurnLoadRom(DrvLutPROM,              3, 1)) return 1;
		}

		DrvGfxExpand(DrvGfxROM, 0x40000);
		DrvExpandLookupTable();
	}

	HD6309Init(0);
	HD6309Open(0);
	HD6309MapMemory(DrvPalRAM,	0x1000, 0x10ff, MAP_ROM);
	HD6309MapMemory(DrvHD6309RAM,	0x1800, 0x1fff, MAP_RAM);
	HD6309MapMemory(DrvSprRAM,	0x2000, 0x2fff, MAP_RAM);
	HD6309MapMemory(DrvVidRAM[0],	0x3000, 0x37ff, MAP_RAM);
	HD6309MapMemory(DrvVidRAM[1],	0x3800, 0x3fff, MAP_RAM);
	HD6309MapMemory(DrvHD6309ROM,	0x8000, 0xffff, MAP_ROM);
	HD6309SetWriteHandler(labyrunr_write);
	HD6309SetReadHandler(labyrunr_read);
	HD6309Close();

	BurnYM2203Init(2, 3000000, NULL, 0);
	BurnYM2203SetPorts(0, &ym2203_0_read_portA, &ym2203_0_read_portB, NULL, NULL);
	BurnYM2203SetPorts(1, NULL, &ym2203_1_read_portB, NULL, NULL);
	BurnTimerAttachHD6309(4000000);
	BurnYM2203SetAllRoutes(0, 0.80, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetAllRoutes(1, 0.80, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetPSGVolume(0, 0.80);
	BurnYM2203SetPSGVolume(1, 0.80);

	GenericTilesInit();

	k007121_init(0, (0x80000 / (8 * 8)) - 1, DrvSprRAM);

	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, tile_map_callback<0>, 8, 8, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, tile_map_callback<1>, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM, 4, 8, 8, 0x80000,     0, 0x7f);
	GenericTilemapSetOffsets(0, 40, -16, 0, -16);
	GenericTilemapSetOffsets(1, 0, -16);
	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetScrollCols(0, 32);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvInit() { return CommonInit(0); }
static INT32 DrvInit2() { return CommonInit(1); }

static INT32 DrvExit()
{
	BurnYM2203Exit();

	HD6309Exit();

	GenericTilesExit();

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteInit()
{
	UINT32 pens[0x80];

	UINT16 *pal = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x100/2; i++) {
		UINT16 p = (BURN_ENDIAN_SWAP_INT16(pal[i]) << 8) | (BURN_ENDIAN_SWAP_INT16(pal[i]) >> 8);

		INT32 b = (p >> 10) & 0x1f;
		INT32 g = (p >> 5) & 0x1f;
		INT32 r = (p >> 0) & 0x1f;

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);

		pens[i] = BurnHighCol(r,g,b,0);
	}

	for (INT32 i = 0; i < 0x800; i++) {
		DrvPalette[i] = pens[DrvLookUpTable[i]];
	}

	DrvPalette[0x800] = BurnHighCol(0,0,0,0); // black
}

static void draw_sprites()
{
	INT32 base_color = ((k007121_ctrl_read(0, 6) & 0x30) << 1);
	INT32 global_x_offset = (k007121_ctrl_read(0, 7)&0x08) ? 16 : 40;
	INT32 global_y_offset = (k007121_ctrl_read(0, 7)&0x08) ? -16 : 16;
	UINT32 pri_mask = (k007121_ctrl_read(0, 3) & 0x20) >> 4;

	k007121_draw(0, pTransDraw, DrvGfxROM, DrvSprTranspLut, base_color, global_x_offset, global_y_offset, 0, pri_mask, 0);
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvRecalc = 0;
		DrvPaletteInit();
	}

	UINT8 ctrl_0 = k007121_ctrl_read(0, 0);
	UINT8 ctrl_1 = k007121_ctrl_read(0, 1);
	UINT8 ctrl_2 = k007121_ctrl_read(0, 2);
	UINT8 ctrl_3 = k007121_ctrl_read(0, 3);
	UINT8 flipscreen = k007121_ctrl_read(0, 7)&0x08;

	rectangle visarea;
	GenericTilesGetClip(&visarea.min_x, &visarea.max_x, &visarea.min_y, &visarea.max_y);

	BurnTransferClear(0x800);

	GenericTilemapSetFlip(0, flipscreen ? TMAP_FLIPXY : 0);
	GenericTilemapSetFlip(1, flipscreen ? TMAP_FLIPXY : 0);

	if (~ctrl_3 & 0x20)
	{
		rectangle clip[2];
		clip[0] = clip[1] = visarea;

		if (flipscreen)
		{
			clip[0].max_x -= 40;
			clip[1].min_x = clip[1].max_x - 39;
		}
		else
		{
			clip[0].min_x += 40;
			clip[1].max_x = 39;
		}

		GenericTilemapSetScrollX(0, ctrl_0);
		GenericTilemapSetScrollX(1, 0);

		for (INT32 i = 0; i < 32; i++)
		{
			if ((ctrl_1 & 6) == 6)
				GenericTilemapSetScrollCol(0, (i + 2) & 0x1f, ctrl_2 + DrvScrollRAM[i]);
			else
				GenericTilemapSetScrollCol(0, (i + 2) & 0x1f, ctrl_2);
		}


		GenericTilesSetClip(clip[0].min_x, clip[0].max_x, clip[0].min_y, clip[0].max_y);
		if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, TMAP_DRAWOPAQUE | TMAP_SET_GROUP(0));
		if (nSpriteEnable & 1) draw_sprites();
		if (nBurnLayer & 2) GenericTilemapDraw(0, pTransDraw, TMAP_DRAWOPAQUE | TMAP_SET_GROUP(1));
		GenericTilesClearClip();

		GenericTilesSetClip(clip[1].min_x, clip[1].max_x, clip[1].min_y, clip[1].max_y);
		if (nBurnLayer & 4) GenericTilemapDraw(1, pTransDraw, TMAP_DRAWOPAQUE);
		GenericTilesClearClip();
	}
	else
	{
		rectangle clip[3];
		clip[0] = clip[1] = clip[2] = visarea;

		INT32 use_clip2[2] = { 0, 0 };

		if (ctrl_1 & 1)
		{
			if (ctrl_0 < 40)
			{
				use_clip2[0] = 1;
				clip[1].min_x = 40 - ctrl_0;
			}

			clip[0].min_x = clip[0].max_x - ctrl_0 + 8;
			clip[1].max_x = clip[1].max_x - ctrl_0 + 8;
		}
		else
		{
			if (ctrl_0 < 40)
			{
				use_clip2[1] = 1;
				clip[0].min_x = 40 - ctrl_0;
			}

			clip[0].max_x = clip[0].max_x - ctrl_0 + 8;
			clip[1].min_x = clip[1].max_x - ctrl_0 + 8;
		}

		if (use_clip2[0] || use_clip2[1])
			clip[2].max_x = 40 - ctrl_0 - 8;

		for (INT32 i = 0; i < 3; i++)
		{
			if (flipscreen)
			{
				INT32 max_x = visarea.max_x - clip[i].min_x;
				INT32 min_x = visarea.max_x - clip[i].max_x;
				clip[i].max_x = max_x;
				clip[i].min_x = min_x;
			}
		}

		GenericTilemapSetScrollX(0, ctrl_0);
		GenericTilemapSetScrollX(1, ctrl_0);

		INT32 clip_idx;
		clip_idx = use_clip2[0] ? 2 : 0;
		GenericTilesSetClip(clip[clip_idx].min_x, clip[clip_idx].max_x, clip[clip_idx].min_y, clip[clip_idx].max_y);
		if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, TMAP_SET_GROUP(0));
		if (nSpriteEnable & 1) draw_sprites();
		if (nBurnLayer & 2) GenericTilemapDraw(0, pTransDraw, TMAP_SET_GROUP(1));
		GenericTilesClearClip();

		clip_idx = use_clip2[1] ? 2 : 0;
		GenericTilesSetClip(clip[clip_idx].min_x, clip[clip_idx].max_x, clip[clip_idx].min_y, clip[clip_idx].max_y);
		if (nBurnLayer & 4) GenericTilemapDraw(1, pTransDraw, TMAP_DRAWOPAQUE);
		GenericTilesClearClip();
	}

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

	HD6309NewFrame();

	{
		memset (DrvInputs, 0xff, 3);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		// Clear opposites
		if ((DrvInputs[0] & 0x0c) == 0) DrvInputs[0] |= 0x0c;
		if ((DrvInputs[0] & 0x03) == 0) DrvInputs[0] |= 0x03;
		if ((DrvInputs[1] & 0x0c) == 0) DrvInputs[1] |= 0x0c;
		if ((DrvInputs[1] & 0x03) == 0) DrvInputs[1] |= 0x03;
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[1] = { 4000000 / 60 };

	HD6309Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN_TIMER(0);

		if ((i & 0x3f) == 0x00 && (k007121_ctrl_read(0, 7) & 0x01)) HD6309SetIRQLine(0x20, CPU_IRQSTATUS_AUTO);

		if (i == nInterleave-1) {
			if (k007121_ctrl_read(0, 7) & 0x02) HD6309SetIRQLine(0x00, CPU_IRQSTATUS_HOLD);
			k007121_buffer(0);
		}
	}

	HD6309Close();

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
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
		HD6309Scan(nAction);

		BurnYM2203Scan(nAction, pnMin);

		SCAN_VAR(HD6309Bank);
		SCAN_VAR(watchdog);

		k007121_scan(nAction);

		K051733Scan(nAction);
	}

	if (nAction & ACB_WRITE) {
		HD6309Open(0);
		bankswitch(HD6309Bank);
		HD6309Close();
	}

	return 0;
}


// Trick Trap (World?)

static struct BurnRomInfo tricktrpRomDesc[] = {
	{ "771e04",			0x10000, 0xba2c7e20, 1 | BRF_PRG | BRF_ESS }, //  0 HD6309 Code
	{ "771e03",			0x10000, 0xd0d68036, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "771e01a",		0x10000, 0x103ffa0d, 2 | BRF_GRA },           //  2 Graphics Tiles
	{ "771e01c",		0x10000, 0xcfec5be9, 2 | BRF_GRA },           //  3
	{ "771d01b",		0x10000, 0x07f2a71c, 2 | BRF_GRA },           //  4
	{ "771d01d",		0x10000, 0xf6810a49, 2 | BRF_GRA },           //  5

	{ "771d02.08d",		0x00100, 0x3d34bb5a, 3 | BRF_GRA },           ///  6 Sprite Color Lookup Tables
};

STD_ROM_PICK(tricktrp)
STD_ROM_FN(tricktrp)

struct BurnDriver BurnDrvTricktrp = {
	"tricktrp", NULL, NULL, NULL, "1987",
	"Trick Trap (World?)\0", NULL, "Konami", "GX771",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_RUNGUN, 0,
	NULL, tricktrpRomInfo, tricktrpRomName, NULL, NULL, NULL, NULL, LabyrunrInputInfo, LabyrunrDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	224, 280, 3, 4
};


// Labyrinth Runner (Japan)

static struct BurnRomInfo labyrunrRomDesc[] = {
	{ "771j04.10f",		0x10000, 0x354a41d0, 1 | BRF_PRG | BRF_ESS }, //  0 HD6309 Code
	{ "771j03.08f",		0x10000, 0x12b49044, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "771d01.14a",		0x40000, 0x15c8f5f9, 2 | BRF_GRA },           //  2 Graphics Tiles

	{ "771d02.08d",		0x00100, 0x3d34bb5a, 3 | BRF_GRA },           //  3 Sprite Color Lookup Tables
};

STD_ROM_PICK(labyrunr)
STD_ROM_FN(labyrunr)

struct BurnDriver BurnDrvLabyrunr = {
	"labyrunr", "tricktrp", NULL, NULL, "1987",
	"Labyrinth Runner (Japan)\0", NULL, "Konami", "GX771",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_RUNGUN, 0,
	NULL, labyrunrRomInfo, labyrunrRomName, NULL, NULL, NULL, NULL, LabyrunrInputInfo, LabyrunrDIPInfo,
	DrvInit2, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	224, 280, 3, 4
};


// Labyrinth Runner (World Ver. K)

static struct BurnRomInfo labyrunrkRomDesc[] = {
	{ "771k04.10f",		0x10000, 0x9816ab35, 1 | BRF_PRG | BRF_ESS }, //  0 HD6309 Code
	{ "771k03.8f",		0x10000, 0x48d732ae, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "771d01a.13a",	0x10000, 0x0cd1ed1a, 2 | BRF_GRA },           //  2 Graphics Tiles
	{ "771d01c.13a",	0x10000, 0xd75521fe, 2 | BRF_GRA },           //  3
	{ "771d01b",		0x10000, 0x07f2a71c, 2 | BRF_GRA },           //  4
	{ "771d01d",		0x10000, 0xf6810a49, 2 | BRF_GRA },           //  5

	{ "771d02.08d",		0x00100, 0x3d34bb5a, 3 | BRF_GRA },           //  6 Sprite Color Lookup Tables
};

STD_ROM_PICK(labyrunrk)
STD_ROM_FN(labyrunrk)

struct BurnDriver BurnDrvLabyrunrk = {
	"labyrunrk", "tricktrp", NULL, NULL, "1987",
	"Labyrinth Runner (World Ver. K)\0", NULL, "Konami", "GX771",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_RUNGUN, 0,
	NULL, labyrunrkRomInfo, labyrunrkRomName, NULL, NULL, NULL, NULL, LabyrunrInputInfo, LabyrunrDIPInfo,
	DrvInit, DrvExit, DrvFrame,DrvDraw, DrvScan, &DrvRecalc, 0x800,
	224, 280, 3, 4
};


// Labyrinth Runner (World Ver. F)

static struct BurnRomInfo labyrunrfRomDesc[] = {
	{ "771k04.10f",		0x10000, 0x86a36806, 1 | BRF_PRG | BRF_ESS }, //  0 HD6309 Code
	{ "771k03.8f",		0x10000, 0x6c073295, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "771d01a.13a",	0x10000, 0x0cd1ed1a, 2 | BRF_GRA },           //  2 Graphics Tiles
	{ "771d01c.13a",	0x10000, 0xd75521fe, 2 | BRF_GRA },           //  3
	{ "771d01b",		0x10000, 0x07f2a71c, 2 | BRF_GRA },           //  4
	{ "771d01d",		0x10000, 0xf6810a49, 2 | BRF_GRA },           //  5

	{ "771d02.08d",		0x00100, 0x3d34bb5a, 3 | BRF_GRA },           //  6 Sprite Color Lookup Tables
};

STD_ROM_PICK(labyrunrf)
STD_ROM_FN(labyrunrf)

struct BurnDriver BurnDrvLabyrunrf = {
	"labyrunrf", "tricktrp", NULL, NULL, "1987",
	"Labyrinth Runner (World Ver. F)\0", NULL, "Konami", "GX771",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_RUNGUN, 0,
	NULL, labyrunrfRomInfo, labyrunrfRomName, NULL, NULL, NULL, NULL, LabyrunrInputInfo, LabyrunrDIPInfo,
	DrvInit, DrvExit, DrvFrame,DrvDraw, DrvScan, &DrvRecalc, 0x800,
	224, 280, 3, 4
};
