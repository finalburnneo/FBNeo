// FB Alpha Jailbreak driver module
// Based on MAME driver by Ernesto Corvi

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "vlm5030.h"
#include "sn76496.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvM6809ROM;
static UINT8 *DrvM6809DecROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvVLMROM;
static UINT8 *DrvColPROM;
static UINT8 *DrvColRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvM6809RAM0;
static UINT8 *DrvM6809RAM1;
static UINT8 *DrvScrxRAM;
static UINT8 *DrvTransLut;

static UINT32 *DrvPalette;
static UINT8  DrvRecalc;

static UINT8 scrolldirection;
static UINT8 nmi_enable;
static UINT8 irq_enable;
static UINT8 flipscreen;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static INT32 watchdog;

static struct BurnInputInfo JailbrekInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Jailbrek)

static struct BurnDIPInfo JailbrekDIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL			},
	{0x15, 0xff, 0xff, 0x59, NULL			},
	{0x16, 0xff, 0xff, 0x0f, NULL			},

	{0   , 0xfe, 0   ,    16, "Coin A"		},
	{0x14, 0x01, 0x0f, 0x02, "4 Coins 1 Credits"	},
	{0x14, 0x01, 0x0f, 0x05, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x14, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x14, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x14, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x14, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x14, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x14, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x14, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x14, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x14, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x14, 0x01, 0x0f, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    16, "Coin B"		},
	{0x14, 0x01, 0xf0, 0x20, "4 Coins 1 Credits"	},
	{0x14, 0x01, 0xf0, 0x50, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"	},
	{0x14, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"	},
	{0x14, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"	},
	{0x14, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"	},
	{0x14, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x14, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x14, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x14, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x14, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x14, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x14, 0x01, 0xf0, 0x00, "Invalid"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x15, 0x01, 0x03, 0x03, "1"			},
	{0x15, 0x01, 0x03, 0x02, "2"			},
	{0x15, 0x01, 0x03, 0x01, "3"			},
	{0x15, 0x01, 0x03, 0x00, "5"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x15, 0x01, 0x04, 0x00, "Upright"		},
	{0x15, 0x01, 0x04, 0x04, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x15, 0x01, 0x08, 0x08, "30K 70K+"		},
	{0x15, 0x01, 0x08, 0x00, "40K 80K+"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x15, 0x01, 0x30, 0x30, "Easy"			},
	{0x15, 0x01, 0x30, 0x20, "Normal"		},
	{0x15, 0x01, 0x30, 0x10, "Difficult"		},
	{0x15, 0x01, 0x30, 0x00, "Very Difficult"	},

	{0   , 0xfe, 0   ,    0, "Demo Sounds"		},
	{0x15, 0x01, 0x80, 0x80, "Off"			},
	{0x15, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x16, 0x01, 0x01, 0x01, "Off"			},
	{0x16, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Upright Controls"	},
	{0x16, 0x01, 0x02, 0x02, "Single"		},
	{0x16, 0x01, 0x02, 0x00, "Dual"			},
};

STDDIPINFO(Jailbrek)

static void jailbrek_write(UINT16 address, UINT8 data)
{
	if ((address & 0xffc0) == 0x2000) {
		DrvScrxRAM[address & 0x3f] = data;
		return;
	}

	switch (address)
	{
		case 0x2040:
		case 0x2041: // nop
		return;

		case 0x2042:
			scrolldirection = data & 4;
		return;

		case 0x2043: // nop
		return;

		case 0x2044:
			nmi_enable = data & 0x01;
			irq_enable = data & 0x02;
	//		flipscreen = data & 0x08;
		return;

		case 0x3100:
			SN76496Write(0, data);
		return;

		case 0x3200:
		return;		// nop

		case 0x3300:
			watchdog = 0;
		return;

		case 0x4000:
			vlm5030_st(0, (data >> 1) & 1);
			vlm5030_rst(0, (data >> 2) & 1);
		return;

		case 0x5000:
			vlm5030_data_write(0, data);
		return;
	}
}

static UINT8 jailbrek_read(UINT16 address)
{
	if ((address & 0xffc0) == 0x2000) {
		return DrvScrxRAM[address & 0x3f];
	}

	switch (address)
	{
		case 0x3100:
			return DrvDips[1];

		case 0x3200:
			return DrvDips[2];

		case 0x3300:
		case 0x3301:
		case 0x3302:
			return DrvInputs[address & 3];

		case 0x3303:
			return DrvDips[0];

		case 0x6000:
			return (vlm5030_bsy(0) ? 1 : 0);
	}

	return 0;
}

static UINT32 DrvVLM5030Sync(INT32 samples_rate)
{
	return (samples_rate * M6809TotalCycles()) / 25600;
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	M6809Open(0);
	M6809Reset();
	M6809Close();

	vlm5030Reset(0);

	watchdog = 0;
	irq_enable = 0;
	nmi_enable = 0;
	flipscreen = 0;
	scrolldirection = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6809ROM		= Next; Next += 0x008000;
	DrvM6809DecROM		= Next; Next += 0x008000;

	DrvGfxROM0		= Next; Next += 0x010000;
	DrvGfxROM1		= Next; Next += 0x020000;

	DrvVLMROM		= Next; Next += 0x004000;

	DrvColPROM		= Next; Next += 0x000240;

	DrvTransLut		= Next; Next += 0x000100;

	DrvPalette		= (UINT32*)Next; Next += 0x200 * sizeof(UINT32);

	AllRam			= Next;

	DrvColRAM		= Next; Next += 0x000800;
	DrvVidRAM		= Next; Next += 0x000800;
	DrvSprRAM		= Next; Next += 0x000100;

	DrvM6809RAM0		= Next; Next += 0x000f00;
	DrvM6809RAM1		= Next; Next += 0x000100;

	DrvScrxRAM		= Next; Next += 0x000040;

	RamEnd			= Next;
	MemEnd			= Next;

	return 0;
}

static void DrvGfxExpand(UINT8 *src, INT32 len) // 64, 32
{
	for (INT32 i = (len - 1) * 2; i >= 0; i-=2) {
		src[i+0] = src[i/2] >> 4;
		src[i+1] = src[i/2] & 0xf;
	}
}

static void DrvPaletteInit()
{
	UINT32 pens[32];

	for (INT32 i = 0; i < 32; i++)
	{
		INT32 r = DrvColPROM[i + 0x00] & 0xf;
		INT32 g = DrvColPROM[i + 0x00] >> 4;
		INT32 b = DrvColPROM[i + 0x20] & 0xf;

		pens[i] = BurnHighCol(r+(r*16), g+(g*16), b+(b*16), 0);
	}

	for (INT32 i = 0; i < 0x200; i++) {
		DrvPalette[i] = pens[(DrvColPROM[i+0x40] & 0xf) | ((~i & 0x100) >> 4)];
	}
}

static void M6809Decode()
{
	for (INT32 i = 0; i < 0x8000; i++) {
		DrvM6809DecROM[i] = DrvM6809ROM[i] ^ (((i&2)?0x80:0x20)|((i&8)?0x08:0x02));
	}
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
		if (BurnDrvGetFlags() & BDF_BOOTLEG)
		{
			if (BurnLoadRom(DrvM6809ROM + 0x00000,  0, 1)) return 1;
	
			if (BurnLoadRom(DrvGfxROM0  + 0x00000,  1, 1)) return 1;
	
			if (BurnLoadRom(DrvGfxROM1  + 0x00000,  2, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1  + 0x08000,  3, 1)) return 1;
	
			if (BurnLoadRom(DrvColPROM  + 0x00000,  4, 1)) return 1;
			if (BurnLoadRom(DrvColPROM  + 0x00020,  5, 1)) return 1;
			if (BurnLoadRom(DrvColPROM  + 0x00040,  6, 1)) return 1;
			if (BurnLoadRom(DrvColPROM  + 0x00140,  7, 1)) return 1;
	
			if (BurnLoadRom(DrvVLMROM   + 0x00000,  8, 1)) return 1;
		}
		else
		{
			if (BurnLoadRom(DrvM6809ROM + 0x00000,  0, 1)) return 1;
			if (BurnLoadRom(DrvM6809ROM + 0x04000,  1, 1)) return 1;
	
			if (BurnLoadRom(DrvGfxROM0  + 0x00000,  2, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0  + 0x04000,  3, 1)) return 1;
	
			if (BurnLoadRom(DrvGfxROM1  + 0x00000,  4, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1  + 0x04000,  5, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1  + 0x08000,  6, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1  + 0x0c000,  7, 1)) return 1;
	
			if (BurnLoadRom(DrvColPROM  + 0x00000,  8, 1)) return 1;
			if (BurnLoadRom(DrvColPROM  + 0x00020,  9, 1)) return 1;
			if (BurnLoadRom(DrvColPROM  + 0x00040, 10, 1)) return 1;
			if (BurnLoadRom(DrvColPROM  + 0x00140, 11, 1)) return 1;
	
			if (BurnLoadRom(DrvVLMROM   + 0x00000, 12, 1)) return 1;
			memcpy (DrvVLMROM, DrvVLMROM + 0x2000, 0x2000);
		}

		M6809Decode();
		DrvGfxExpand(DrvGfxROM0, 0x08000);
		DrvGfxExpand(DrvGfxROM1, 0x10000);
		DrvPaletteInit();
	}

	M6809Init(1);
	M6809Open(0);
	M6809MapMemory(DrvColRAM,		0x0000, 0x07ff, MAP_RAM);
	M6809MapMemory(DrvVidRAM,		0x0800, 0x0fff, MAP_RAM);
	M6809MapMemory(DrvSprRAM,		0x1000, 0x10ff, MAP_RAM);
	M6809MapMemory(DrvM6809RAM0,		0x1100, 0x1fff, MAP_RAM);
	M6809MapMemory(DrvM6809RAM1,		0x3000, 0x30ff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM,		0x8000, 0xffff, MAP_READ);
	M6809MapMemory(DrvM6809DecROM,		0x8000, 0xffff, MAP_FETCH);
	M6809SetWriteHandler(jailbrek_write);
	M6809SetReadHandler(jailbrek_read);
	M6809Close();

	SN76489AInit(0, 1536000, 0);
	SN76496SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	vlm5030Init(0, 3579545, DrvVLM5030Sync, DrvVLMROM, 0x2000, 1);
	vlm5030SetAllRoutes(0, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6809Exit();

	vlm5030Exit();
	SN76496Exit();

	BurnFree (AllMem);

	return 0;
}

static void draw_layer()
{
	for (INT32 y = 0; y < 32; y ++)
	{
		for (INT32 x = 0; x < 32; x++)
		{
			INT32 sy, sx;

			if (scrolldirection) {			// column scroll
				sy = (y * 8) + DrvScrxRAM[x];
				sx = (x * 8);
			} else {				// row scroll
				sy = (y * 8);
				sx = (x * 8) + (DrvScrxRAM[y] + ((DrvScrxRAM[y+0x20]&1)*256));
			}

			INT32 offs = ((sx/8)&0x3f)+(((sy/8)&0x1f)*0x40);

			INT32 attr  = DrvColRAM[offs];
			INT32 code  = DrvVidRAM[offs] + ((attr & 0xc0) << 2);
			INT32 color = attr & 0x0f;

			INT32 syy = ((y*8)-(sy&7))-16;
			if (syy < -7 || syy >= nScreenHeight) continue;
			INT32 sxx = ((x*8)-(sx&7))-8;
			if (sxx < -7 || sxx >= nScreenWidth) continue;

			Render8x8Tile_Clip(pTransDraw, code, sxx, syy, color, 4, 0, DrvGfxROM0);
		}
	}
}

static void draw_sprites()
{
	for (INT32 i = 0; i < 0xc0; i += 4)
	{
		INT32 attr  = DrvSprRAM[i + 1];
		INT32 code  = DrvSprRAM[i] + ((attr & 0x40) << 2);
		INT32 color =((attr & 0x0f) * 16) | 0x100;
		INT32 flipx = attr & 0x10;
		INT32 flipy = attr & 0x20;
		INT32 sx    = DrvSprRAM[i + 2] - ((attr & 0x80) << 1);
		INT32 sy    = DrvSprRAM[i + 3];

		if (flipscreen)
		{
			sx = 240 - sx;
			sy = 240 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		sy -= 16;
		sx -= 8;

		if (sx >= nScreenWidth || sy >= nScreenHeight) continue;

		if (flipy) {
			if (flipx) {
				RenderTileTranstab(pTransDraw, DrvGfxROM1, code * 4 + 3, color, 0, sx + 0, sy + 0, flipx, flipy, 8, 8, DrvColPROM + 0x40);
				RenderTileTranstab(pTransDraw, DrvGfxROM1, code * 4 + 2, color, 0, sx + 8, sy + 0, flipx, flipy, 8, 8, DrvColPROM + 0x40);
				RenderTileTranstab(pTransDraw, DrvGfxROM1, code * 4 + 1, color, 0, sx + 0, sy + 8, flipx, flipy, 8, 8, DrvColPROM + 0x40);
				RenderTileTranstab(pTransDraw, DrvGfxROM1, code * 4 + 0, color, 0, sx + 8, sy + 8, flipx, flipy, 8, 8, DrvColPROM + 0x40);
			} else {
				RenderTileTranstab(pTransDraw, DrvGfxROM1, code * 4 + 2, color, 0, sx + 0, sy + 0, flipx, flipy, 8, 8, DrvColPROM + 0x40);
				RenderTileTranstab(pTransDraw, DrvGfxROM1, code * 4 + 3, color, 0, sx + 8, sy + 0, flipx, flipy, 8, 8, DrvColPROM + 0x40);
				RenderTileTranstab(pTransDraw, DrvGfxROM1, code * 4 + 0, color, 0, sx + 0, sy + 8, flipx, flipy, 8, 8, DrvColPROM + 0x40);
				RenderTileTranstab(pTransDraw, DrvGfxROM1, code * 4 + 1, color, 0, sx + 8, sy + 8, flipx, flipy, 8, 8, DrvColPROM + 0x40);
			}
		} else {
			if (flipx) {
				RenderTileTranstab(pTransDraw, DrvGfxROM1, code * 4 + 1, color, 0, sx + 0, sy + 0, flipx, flipy, 8, 8, DrvColPROM + 0x40);
				RenderTileTranstab(pTransDraw, DrvGfxROM1, code * 4 + 0, color, 0, sx + 8, sy + 0, flipx, flipy, 8, 8, DrvColPROM + 0x40);
				RenderTileTranstab(pTransDraw, DrvGfxROM1, code * 4 + 3, color, 0, sx + 0, sy + 8, flipx, flipy, 8, 8, DrvColPROM + 0x40);
				RenderTileTranstab(pTransDraw, DrvGfxROM1, code * 4 + 2, color, 0, sx + 8, sy + 8, flipx, flipy, 8, 8, DrvColPROM + 0x40);
			} else {
				RenderTileTranstab(pTransDraw, DrvGfxROM1, code * 4 + 0, color, 0, sx + 0, sy + 0, flipx, flipy, 8, 8, DrvColPROM + 0x40);
				RenderTileTranstab(pTransDraw, DrvGfxROM1, code * 4 + 1, color, 0, sx + 8, sy + 0, flipx, flipy, 8, 8, DrvColPROM + 0x40);
				RenderTileTranstab(pTransDraw, DrvGfxROM1, code * 4 + 2, color, 0, sx + 0, sy + 8, flipx, flipy, 8, 8, DrvColPROM + 0x40);
				RenderTileTranstab(pTransDraw, DrvGfxROM1, code * 4 + 3, color, 0, sx + 8, sy + 8, flipx, flipy, 8, 8, DrvColPROM + 0x40);
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

	if (~nBurnLayer & 1) BurnTransferClear();

	if (nBurnLayer & 1) draw_layer();
	if (nSpriteEnable & 1) draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	watchdog++;
	if (watchdog >= 120) {
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

	INT32 nInterleave = 9;
	INT32 nCyclesTotal[1] = { 1536000 / 60 };
	INT32 nCyclesDone[1] = { 0 };
	INT32 nSoundBufferPos = 0;

	M6809Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone[0] += M6809Run(nCyclesTotal[0] / nInterleave);

		if (i < 8 && nmi_enable) M6809SetIRQLine(0x20, CPU_IRQSTATUS_AUTO);

		if (i == 8 && irq_enable) M6809SetIRQLine(0, CPU_IRQSTATUS_AUTO);

		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			SN76496Update(0, pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		if (nSegmentLength) {
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			SN76496Update(0, pSoundBuf, nSegmentLength);
		}
		vlm5030Update(0, pBurnSoundOut, nBurnSoundLen);
	}

	M6809Close();

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
		M6809Scan(nAction);

		vlm5030Scan(nAction);
		SN76496Scan(nAction,pnMin);

		SCAN_VAR(scrolldirection);
		SCAN_VAR(nmi_enable);
		SCAN_VAR(irq_enable);
		SCAN_VAR(flipscreen);
	}

	return 0;
}


// Jail Break

static struct BurnRomInfo jailbrekRomDesc[] = {
	{ "507p03.11d",	0x4000, 0xa0b88dfd, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "507p02.9d",	0x4000, 0x444b7d8e, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "507l08.4f",	0x4000, 0xe3b7a226, 2 | BRF_GRA },           //  2 Tiles
	{ "507j09.5f",	0x4000, 0x504f0912, 2 | BRF_GRA },           //  3

	{ "507j04.3e",	0x4000, 0x0d269524, 3 | BRF_GRA },           //  4 Sprites
	{ "507j05.4e",	0x4000, 0x27d4f6f4, 3 | BRF_GRA },           //  5
	{ "507j06.5e",	0x4000, 0x717485cb, 3 | BRF_GRA },           //  6
	{ "507j07.3f",	0x4000, 0xe933086f, 3 | BRF_GRA },           //  7

	{ "507j10.1f",	0x0020, 0xf1909605, 4 | BRF_GRA },           //  8 Color Proms
	{ "507j11.2f",	0x0020, 0xf70bb122, 4 | BRF_GRA },           //  9
	{ "507j13.7f",	0x0100, 0xd4fe5c97, 4 | BRF_GRA },           // 10
	{ "507j12.6f",	0x0100, 0x0266c7db, 4 | BRF_GRA },           // 11

	{ "507l01.8c",	0x4000, 0x0c8a3605, 5 | BRF_SND },           // 12 VLM5030 Samples
};

STD_ROM_PICK(jailbrek)
STD_ROM_FN(jailbrek)

struct BurnDriver BurnDrvJailbrek = {
	"jailbrek", NULL, NULL, NULL, "1986",
	"Jail Break\0", NULL, "Konami", "GX507",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, jailbrekRomInfo, jailbrekRomName, NULL, NULL, JailbrekInputInfo, JailbrekDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	240, 224, 4, 3
};


// Manhattan 24 Bunsyo (Japan)

static struct BurnRomInfo manhatanRomDesc[] = {
	{ "507n03.11d",	0x4000, 0xe5039f7e, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "507n02.9d",	0x4000, 0x143cc62c, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "507j08.4f",	0x4000, 0x175e1b49, 2 | BRF_GRA },           //  2 Tiles
	{ "507j09.5f",	0x4000, 0x504f0912, 2 | BRF_GRA },           //  3

	{ "507j04.3e",	0x4000, 0x0d269524, 3 | BRF_GRA },           //  4 Sprites
	{ "507j05.4e",	0x4000, 0x27d4f6f4, 3 | BRF_GRA },           //  5
	{ "507j06.5e",	0x4000, 0x717485cb, 3 | BRF_GRA },           //  6
	{ "507j07.3f",	0x4000, 0xe933086f, 3 | BRF_GRA },           //  7

	{ "507j10.1f",	0x0020, 0xf1909605, 4 | BRF_GRA },           //  8 Color Proms
	{ "507j11.2f",	0x0020, 0xf70bb122, 4 | BRF_GRA },           //  9
	{ "507j13.7f",	0x0100, 0xd4fe5c97, 4 | BRF_GRA },           // 10
	{ "507j12.6f",	0x0100, 0x0266c7db, 4 | BRF_GRA },           // 11

	{ "507p01.8c",	0x4000, 0x973fa351, 5 | BRF_SND },           // 12 VLM5030 Samples
};

STD_ROM_PICK(manhatan)
STD_ROM_FN(manhatan)

struct BurnDriver BurnDrvManhatan = {
	"manhatan", "jailbrek", NULL, NULL, "1986",
	"Manhattan 24 Bunsyo (Japan)\0", NULL, "Konami", "GX507",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, manhatanRomInfo, manhatanRomName, NULL, NULL, JailbrekInputInfo, JailbrekDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	240, 224, 4, 3
};


// Jail Break (bootleg)

static struct BurnRomInfo jailbrekbRomDesc[] = {
	{ "1.k6",	0x8000, 0xdf0e8fc7, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code

	{ "3.h6",	0x8000, 0xbf67a8ff, 2 | BRF_GRA },           //  1 Tiles

	{ "5.f6",	0x8000, 0x081d2eea, 3 | BRF_GRA },           //  2 Sprites
	{ "4.g6",	0x8000, 0xe34b93b8, 3 | BRF_GRA },           //  3

	{ "prom.j2",	0x0020, 0xf1909605, 4 | BRF_GRA },           //  4 Color Proms
	{ "prom.i2",	0x0020, 0xf70bb122, 4 | BRF_GRA },           //  5
	{ "prom.d6",	0x0100, 0xd4fe5c97, 4 | BRF_GRA },           //  6
	{ "prom.e6",	0x0100, 0x0266c7db, 4 | BRF_GRA },           //  7

	{ "2.i6",	0x2000, 0xd91d15e3, 5 | BRF_SND },           //  8 VLM5030 Samples

	{ "k4.bin",	0x0001, 0x00000000, 6 | BRF_NODUMP },        //  9 plds
	{ "a7.bin",	0x0001, 0x00000000, 6 | BRF_NODUMP },        // 10
	{ "g9.bin",	0x0001, 0x00000000, 6 | BRF_NODUMP },        // 11
	{ "k8.bin",	0x0001, 0x00000000, 6 | BRF_NODUMP },        // 12
};

STD_ROM_PICK(jailbrekb)
STD_ROM_FN(jailbrekb)

struct BurnDriver BurnDrvJailbrekb = {
	"jailbrekb", "jailbrek", NULL, NULL, "1986",
	"Jail Break (bootleg)\0", NULL, "bootleg", "GX507",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, jailbrekbRomInfo, jailbrekbRomName, NULL, NULL, JailbrekInputInfo, JailbrekDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	240, 224, 4, 3
};
