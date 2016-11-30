// FB Alpha Labyrinth Runner driver module
// Based on MAME driver by Nicola Salmoria

/*
	To do:
		Timing seems a bit off (can hear some gaps in sound due to this)
		Some graphics modes may be incomplete. Need save states to fix these.
*/

#include "tiles_generic.h"
#include "hd6309_intf.h"
#include "burn_ym2203.h"
#include "konamiic.h"

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
static UINT8 *DrvVidRAM0;
static UINT8 *DrvVidRAM1;
static UINT8 *DrvScrollRAM;
static UINT8 *K007121CtrlRAM;
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

static INT32 DrvPalWritten = 0;
static INT32 watchdog = 0;

static struct BurnInputInfo LabyrunrInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
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

STDINPUTINFO(Labyrunr)

static struct BurnDIPInfo LabyrunrDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0x5a, NULL			},
	{0x15, 0xff, 0xff, 0xf7, NULL			},

	{0   , 0xfe, 0   ,    16, "Coin A"		},
	{0x13, 0x01, 0x0f, 0x02, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x0f, 0x05, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x13, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x13, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x13, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x13, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x13, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x13, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x13, 0x01, 0x0f, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    16, "Coin B"		},
	{0x13, 0x01, 0xf0, 0x20, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0xf0, 0x50, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"	},
	{0x13, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"	},
	{0x13, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"	},
	{0x13, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x13, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x13, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x13, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x13, 0x01, 0xf0, 0x00, "None"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0x03, 0x03, "2"			},
	{0x14, 0x01, 0x03, 0x02, "3"			},
	{0x14, 0x01, 0x03, 0x01, "5"			},
	{0x14, 0x01, 0x03, 0x00, "7"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x14, 0x01, 0x04, 0x00, "Upright"		},
	{0x14, 0x01, 0x04, 0x04, "Cocktail"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x14, 0x01, 0x18, 0x18, "30000 70000"		},
	{0x14, 0x01, 0x18, 0x10, "40000 80000"		},
	{0x14, 0x01, 0x18, 0x08, "40000"		},
	{0x14, 0x01, 0x18, 0x00, "50000"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x60, 0x60, "Easy"			},
	{0x14, 0x01, 0x60, 0x40, "Normal"		},
	{0x14, 0x01, 0x60, 0x20, "Hard"			},
	{0x14, 0x01, 0x60, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x15, 0x01, 0x01, 0x01, "Off"			},
	{0x15, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Upright Controls"	},
	{0x15, 0x01, 0x02, 0x02, "Single"		},
	{0x15, 0x01, 0x02, 0x00, "Dual"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x15, 0x01, 0x04, 0x04, "Off"			},
	{0x15, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x15, 0x01, 0x08, 0x08, "3 Times"		},
	{0x15, 0x01, 0x08, 0x00, "5 Times"		},
};

STDDIPINFO(Labyrunr)

static void bankswitch(INT32 data)
{
	if (data != HD6309Bank) {
		HD6309Bank = data;
		HD6309MapMemory(DrvHD6309ROM + 0x10000 + ((HD6309Bank & 7) * 0x4000), 0x4000, 0x7fff, MAP_ROM);
	}
}

static void labyrunr_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfff8) == 0x0000) {
		K007121CtrlRAM[address & 7] = data;
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
			DrvPalWritten = 1;
		}
		return;
	}

	switch (address)
	{
		case 0x0800:
			BurnYM2203Write(0, 1, data);
		return;

		case 0x0801:
			BurnYM2203Write(0, 0, data);
		return;

		case 0x0900:
			BurnYM2203Write(1, 1, data);
		return;

		case 0x0901:
			BurnYM2203Write(1, 0, data);
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
		return K007121CtrlRAM[address & 7];
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
			return BurnYM2203Read(0, 1);

		case 0x0801:
			return BurnYM2203Read(0, 0);

		case 0x0900:
			return BurnYM2203Read(1, 1);

		case 0x0901:
			return BurnYM2203Read(1, 0);

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

static INT32 DrvSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)HD6309TotalCycles() * nSoundRate / 3000000;
}

static double DrvGetTime()
{
	return (double)HD6309TotalCycles() / 3000000;
}

static INT32 DrvDoReset(INT32 full_reset)
{
	if (full_reset) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	HD6309Open(0);
	HD6309Reset();
	HD6309Bank = -1;
	bankswitch(0);
	BurnYM2203Reset();
	HD6309Close();

	K051733Reset();

	DrvPalWritten = 1;
	watchdog = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvHD6309ROM		= Next; Next += 0x028000;

	DrvGfxROM		= Next; Next += 0x080000;

	DrvLutPROM		= Next; Next += 0x000100;

	DrvLookUpTable		= Next; Next += 0x000800;
	DrvSprTranspLut		= Next; Next += 0x000800;
	DrvTransTab		= Next; Next += 0x002000;

	DrvPalette		= (UINT32*)Next; Next += 0x801 * sizeof(UINT32);

	AllRam			= Next;

	DrvHD6309RAM		= Next; Next += 0x000800;
	DrvPalRAM		= Next; Next += 0x000100;

	DrvSprRAM		= Next; Next += 0x001000;
	DrvVidRAM1		= Next; Next += 0x000800;
	DrvVidRAM0		= Next; Next += 0x000800;
	DrvScrollRAM		= Next; Next += 0x000040;

	K007121CtrlRAM		= Next; Next += 0x000008;

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
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

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
	HD6309MapMemory(DrvVidRAM0,	0x3000, 0x37ff, MAP_RAM);
	HD6309MapMemory(DrvVidRAM1,	0x3800, 0x3fff, MAP_RAM);
	HD6309MapMemory(DrvHD6309ROM,	0x8000, 0xffff, MAP_ROM);
	HD6309SetWriteHandler(labyrunr_write);
	HD6309SetReadHandler(labyrunr_read);
	HD6309Close();

	BurnYM2203Init(2, 3000000, NULL, DrvSynchroniseStream, DrvGetTime, 0);
	BurnYM2203SetPorts(0, &ym2203_0_read_portA, &ym2203_0_read_portB, NULL, NULL);
	BurnYM2203SetPorts(1, NULL, &ym2203_1_read_portB, NULL, NULL);
	BurnTimerAttachHD6309(3000000);
	BurnYM2203SetAllRoutes(0, 0.80, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetAllRoutes(1, 0.80, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetPSGVolume(0, 0.80);
	BurnYM2203SetPSGVolume(1, 0.80);

	GenericTilesInit();

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

	BurnFree (AllMem);

	return 0;
}

static void draw_layer(INT32 layer)
{
	UINT8 *vidram;

	INT32 c1 = K007121CtrlRAM[1];
	INT32 c3 = K007121CtrlRAM[3];
	INT32 c4 = K007121CtrlRAM[4];
	INT32 c5 = K007121CtrlRAM[5];
	INT32 c6 = K007121CtrlRAM[6];

	INT32 b0 = ((c5 >> 0) & 3) + 2;
	INT32 b1 = ((c5 >> 2) & 3) + 1;
	INT32 b2 = ((c5 >> 4) & 3) + 0;
	INT32 b3 = ((c5 >> 6) & 3) - 1;

	INT32 xscroll_enable = 1;
	INT32 yscroll_enable = ~c3 & 0x20;

	INT32 scrollx = K007121CtrlRAM[0];
	INT32 scrolly = K007121CtrlRAM[2];

	INT32 mask = c4 >> 4;

	INT32 opaque = 0;
	INT32 min_x = 0;
	INT32 max_x = nScreenWidth;
	INT32 x_offset = 0;

	if (layer == 1)
	{
		vidram = DrvVidRAM1;

		if (yscroll_enable)
		{
			opaque = 1;
			yscroll_enable = xscroll_enable = 0;
			max_x = 40;
		}
		else
		{
			scrollx -= 40;

			if (c1 & 0x01)
			{
				if (scrollx < 40) min_x = (40 - scrollx) - 1;
				max_x = nScreenWidth - scrollx + 8;
			}
			else
			{
				min_x = nScreenWidth - scrollx + 8;
				max_x = nScreenWidth;
			}
		}
	}
	else
	{
		x_offset = 40;
		min_x = 40;
		vidram = DrvVidRAM0;

		if (yscroll_enable) {
			opaque = 1;
			max_x = 264;	// ?? why?
		}
		else
		{
			if (c1 & 0x01)
			{
				min_x = (nScreenWidth - scrollx + 8) - 1;
				max_x = nScreenWidth;

			}
			else
			{
				if (scrollx < 40) min_x = (40 - scrollx) - 1;
				max_x = nScreenWidth - scrollx + 8;
			}
		}
	}

	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sx = (offs & 0x1f) * 8;
		INT32 sy = (offs / 0x20) * 8;

		if (yscroll_enable) {
			sy -= scrolly;
			if (sy < -7) sy += 256;
		}

		if (xscroll_enable) {
			sx -= scrollx;
			if (sx < -7) sx += 256;
		}

		sx += x_offset;
		sy -= 16;

		if (sx <= (min_x-8) || sx >= max_x) continue;

		INT32 attr = vidram[offs];
		INT32 code = vidram[offs + 0x400];

		INT32 bank = ((attr >> 7) & 1) | ((attr >> b0) & 2) | ((attr >> b1) & 4) | ((attr >> b2) & 8) | ((attr >> b3) & 0x10) | ((c3 & 1) << 5);

		code += ((bank & ~(mask << 1)) | ((c4 & mask) << 1)) << 8;

		INT32 color = ((c6 & 0x30) << 1) | (attr & 7) | 0x10;

		{
			if (DrvTransTab[code&0x1fff] && !opaque) continue;

			UINT8 *gfx = DrvGfxROM + (code & 0x1fff) * 0x40;
			color<<=4;

			for (INT32 y = 0; y < 8; y++) {
				for (INT32 x = 0; x < 8; x++) {
					if ((sy+y) < 0 || (sy+y) >= nScreenHeight || (sx+x) < min_x || (sx+x) >= max_x) continue;

					INT32 pxl = gfx[(y*8)+x];
			
					if (pxl || opaque) {
						pTransDraw[(sy+y)*nScreenWidth+(sx+x)] = pxl+color;
					}
				}
			}
		}
	}
}

static void k007121_sprites_draw(UINT8 *gfx, UINT32 */*palette*/, UINT8 *source, int base_color, int global_x_offset, int bank_base, UINT32 /*pri_mask*/)
{
	INT32 flipscreen = K007121CtrlRAM[7] & 0x08;

	for (INT32 i = 0; i < 0x40; i++)
	{
		int number = source[0];
		int sprite_bank = source[1] & 0x0f;
		int sx = source[3];
		int sy = source[2];
		int attr = source[4];
		int xflip = source[4] & 0x10;
		int yflip = source[4] & 0x20;
		int color = base_color + ((source[1] & 0xf0) >> 4);
		int width, height;
		static const int x_offset[4] = {0x0,0x1,0x4,0x5};
		static const int y_offset[4] = {0x0,0x2,0x8,0xa};
		int x,y, ex, ey, flipx, flipy, destx, desty;

		if (attr & 0x01) sx -= 256;
		if (sy >= 240) sy -= 256;

		number += ((sprite_bank & 0x3) << 8) + ((attr & 0xc0) << 4);
		number = number << 2;
		number += (sprite_bank >> 2) & 3;

		{
			number += bank_base;

			switch (attr & 0xe)
			{
				case 0x06: width = height = 1; break;
				case 0x04: width = 1; height = 2; number &= (~2); break;
				case 0x02: width = 2; height = 1; number &= (~1); break;
				case 0x00: width = height = 2; number &= (~3); break;
				case 0x08: width = height = 4; number &= (~3); break;
				default: width = 1; height = 1;
			}

			for (y = 0; y < height; y++)
			{
				for (x = 0; x < width; x++)
				{
					ex = xflip ? (width - 1 - x) : x;
					ey = yflip ? (height - 1 - y) : y;

					if (flipscreen)
					{
						flipx = !xflip;
						flipy = !yflip;
						destx = 248 - (sx + x * 8);
						desty = 248 - (sy + y * 8);
					}
					else
					{
						flipx = xflip;
						flipy = yflip;
						destx = global_x_offset + sx + x * 8;
						desty = sy + y * 8;
					}

					if (!DrvTransTab[(number + x_offset[ex] + y_offset[ey])&0x1fff])
						RenderTileTranstab(pTransDraw, gfx, (number + x_offset[ex] + y_offset[ey])&0x1fff, color<<4, 0, destx, desty - 16, flipx, flipy, 8, 8, DrvSprTranspLut);
				}
			}
		}

		source += 5;
	}
}

static void DrvPaletteInit()
{
	UINT32 pens[0x80];

	UINT16 *pal = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x100/2; i++) {
		UINT16 p = (pal[i] << 8) | (pal[i] >> 8);

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

static INT32 DrvDraw()
{
	if (DrvPalWritten || DrvRecalc) {
		DrvRecalc = 0;
		DrvPalWritten = 0;
		DrvPaletteInit();
	}

	for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
		pTransDraw[i] = 0x800;
	}

	INT32 c3 = K007121CtrlRAM[3] & 0x40;

	if (nBurnLayer & 1) draw_layer(0);
	if (nSpriteEnable & 1 &&  c3) k007121_sprites_draw(DrvGfxROM, DrvPalette, DrvSprRAM, (K007121CtrlRAM[6] & 0x30) * 2, 40,0, (K007121CtrlRAM[3] & 0x40) >> 5);
	if (nBurnLayer & 2) draw_layer(1);
	if (nSpriteEnable & 1 && !c3) k007121_sprites_draw(DrvGfxROM, DrvPalette, DrvSprRAM, (K007121CtrlRAM[6] & 0x30) * 2, 40,0, (K007121CtrlRAM[3] & 0x40) >> 5);

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
	INT32 nCyclesTotal = 3000000 / 60;

	HD6309Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		BurnTimerUpdate((i+1) * (nCyclesTotal / nInterleave));

		// the timings of these is extremely important.. and currently wrong.
		if ((i & 0x3f) == 0x3f && (K007121CtrlRAM[7] & 0x01)) HD6309SetIRQLine(0x20, CPU_IRQSTATUS_AUTO);

		if (i == 253 && (K007121CtrlRAM[7] & 0x02)) HD6309SetIRQLine(0x00, CPU_IRQSTATUS_AUTO);
	}

	BurnTimerEndFrame(nCyclesTotal);

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
	}

	HD6309Close();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction,INT32 *pnMin)
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
	}

	if (nAction & ACB_WRITE) {
		INT32 bank = HD6309Bank;
		HD6309Bank = -1;

		HD6309Open(0);
		bankswitch(bank);
		HD6309Close();
	}

	return 0;
}


// Trick Trap (World?)

static struct BurnRomInfo tricktrpRomDesc[] = {
	{ "771e04",		0x10000, 0xba2c7e20, 1 | BRF_PRG | BRF_ESS }, //  0 HD6309 Code
	{ "771e03",		0x10000, 0xd0d68036, 1 | BRF_PRG | BRF_ESS }, //  1

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
	"Trick Trap (World?)\0", "Graphics issues in the ending credits.", "Konami", "GX771",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_SHOOT, 0,
	NULL, tricktrpRomInfo, tricktrpRomName, NULL, NULL, LabyrunrInputInfo, LabyrunrDIPInfo,
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
	"Labyrinth Runner (Japan)\0", "Graphics issues in the ending credits.", "Konami", "GX771",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_SHOOT, 0,
	NULL, labyrunrRomInfo, labyrunrRomName, NULL, NULL, LabyrunrInputInfo, LabyrunrDIPInfo,
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
	"Labyrinth Runner (World Ver. K)\0", "Graphics issues in the ending credits.", "Konami", "GX771",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_SHOOT, 0,
	NULL, labyrunrkRomInfo, labyrunrkRomName, NULL, NULL, LabyrunrInputInfo, LabyrunrDIPInfo,
	DrvInit, DrvExit, DrvFrame,DrvDraw, DrvScan, &DrvRecalc, 0x800,
	224, 280, 3, 4
};
