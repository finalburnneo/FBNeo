// FB Neo Psychic 5 driver module
// Based on MAME driver by Jarek Parchanski

#include "tiles_generic.h"
#include "z80_intf.h"
#include "burn_ym2203.h"

#define BG_SCROLLX_LSB      0x308
#define BG_SCROLLX_MSB      0x309
#define BG_SCROLLY_LSB      0x30a
#define BG_SCROLLY_MSB      0x30b
#define BG_SCREEN_MODE      0x30c
#define BG_PAL_INTENSITY_RG 0x1fe
#define BG_PAL_INTENSITY_BU 0x1ff

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80Rom1;
static UINT8 *DrvZ80Rom2;
static UINT8 *DrvZ80Ram1;
static UINT8 *DrvZ80Ram2;
static UINT8 *DrvPagedRam;
static UINT8 *DrvSpriteRam;
static UINT8 *DrvChars;
static UINT8 *DrvBgTiles;
static UINT8 *DrvSprites;
static UINT8 *DrvTempRom;
static UINT8 *DrvBlendTable;
static UINT16 *DrvTempDraw;
static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 DrvJoy0[8];
static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDip[2];
static UINT8 DrvInput[3];
static UINT8 DrvReset;

static UINT8 DrvSoundLatch;
static UINT8 DrvFlipScreen;
static UINT8 DrvRomBank;
static UINT8 DrvVRamPage;
static UINT8 DrvTitleScreen;
static UINT8 DrvBgStatus;
static UINT16 DrvBgScrollX;
static UINT16 DrvBgScrollY;
static INT32 DrvBgClipMode;
static INT32 DrvBgClipMinX;
static INT32 DrvBgClipMaxX;
static INT32 DrvBgClipMinY;
static INT32 DrvBgClipMaxY;
static UINT8 DrvBgSx1;
static UINT8 DrvBgSy1;
static UINT8 DrvBgSy2;

static struct BurnInputInfo DrvInputList[] =
{
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy0 + 6,	"p1 coin"	},
	{"P1 Start",	BIT_DIGITAL,	DrvJoy0 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Fire 1",	BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 1"	},
	{"P1 Fire 2",	BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy0 + 7,	"p2 coin"	},
	{"P2 Start",	BIT_DIGITAL,	DrvJoy0 + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",	BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Fire 1",	BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Fire 2",	BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip 1",		BIT_DIPSWITCH,	DrvDip + 0,		"dip"		},
	{"Dip 2",		BIT_DIPSWITCH,	DrvDip + 1,		"dip"		},
};

STDINPUTINFO(Drv)

static inline void DrvClearOppositesActiveLow(UINT8* nJoystickInputs)
{
	if ((*nJoystickInputs & 0x03) == 0x00) {
		*nJoystickInputs |= 0x03;
	}
	if ((*nJoystickInputs & 0x0c) == 0x00) {
		*nJoystickInputs |= 0x0c;
	}
}

static struct BurnDIPInfo DrvDIPList[]=
{
	DIP_OFFSET(0x11)
	{0x00, 0xff, 0xff, 0xe9, NULL                     },
	{0x01, 0xff, 0xff, 0xff, NULL                     },

	{0   , 0xfe, 0   , 2   , "Flip Screen"            },
	{0x00, 0x01, 0x01, 0x01, "Off"                    },
	{0x00, 0x01, 0x01, 0x00, "On"                     },

	{0   , 0xfe, 0   , 2   , "Difficulty"             },
	{0x00, 0x01, 0x08, 0x08, "Normal"                 },
	{0x00, 0x01, 0x08, 0x00, "Hard"                   },

	{0   , 0xfe, 0   , 2   , "Cabinet"                },
	{0x00, 0x01, 0x10, 0x00, "Upright"                },
	{0x00, 0x01, 0x10, 0x10, "Cocktail"               },

	{0   , 0xfe, 0   , 2   , "Demo Sounds"            },
	{0x00, 0x01, 0x20, 0x00, "Off"                    },
	{0x00, 0x01, 0x20, 0x20, "On"                     },

	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x00, 0x01, 0xc0, 0x80, "2"                      },
	{0x00, 0x01, 0xc0, 0xc0, "3"                      },
	{0x00, 0x01, 0xc0, 0x40, "4"                      },
	{0x00, 0x01, 0xc0, 0x00, "5"                      },

	{0   , 0xfe, 0   , 2   , "Invulnerability (Cheat)"},
	{0x01, 0x01, 0x01, 0x01, "Off"                    },
	{0x01, 0x01, 0x01, 0x00, "On"                     },

	{0   , 0xfe, 0   , 8   , "Coin A"                 },
	{0x01, 0x01, 0xe0, 0x00, "5 Coins 1 Play"         },
	{0x01, 0x01, 0xe0, 0x20, "4 Coins 1 Play"         },
	{0x01, 0x01, 0xe0, 0x40, "3 Coins 1 Play"         },
	{0x01, 0x01, 0xe0, 0x60, "2 Coins 1 Play"         },
	{0x01, 0x01, 0xe0, 0xe0, "1 Coin 1 Play"          },
	{0x01, 0x01, 0xe0, 0xc0, "1 Coin 2 Plays"         },
	{0x01, 0x01, 0xe0, 0xa0, "1 Coin 3 Plays"         },
	{0x01, 0x01, 0xe0, 0x80, "1 Coin 4 Plays"         },

	{0   , 0xfe, 0   , 8   , "Coin B"                 },
	{0x01, 0x01, 0x1c, 0x00, "5 Coins 1 Play"         },
	{0x01, 0x01, 0x1c, 0x04, "4 Coins 1 Play"         },
	{0x01, 0x01, 0x1c, 0x08, "3 Coins 1 Play"         },
	{0x01, 0x01, 0x1c, 0x0c, "2 Coins 1 Play"         },
	{0x01, 0x01, 0x1c, 0x1c, "1 Coin 1 Play"          },
	{0x01, 0x01, 0x1c, 0x18, "1 Coin 2 Plays"         },
	{0x01, 0x01, 0x1c, 0x14, "1 Coin 3 Plays"         },
	{0x01, 0x01, 0x1c, 0x10, "1 Coin 4 Plays"         },
};

STDDIPINFO(Drv)

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80Rom1             = Next; Next += 0x20000;
	DrvZ80Rom2             = Next; Next += 0x10000;

	AllRam                 = Next;

	DrvZ80Ram1             = Next; Next += 0x01800;
	DrvZ80Ram2             = Next; Next += 0x00800;
	DrvPagedRam            = Next; Next += 0x04000;
	DrvSpriteRam           = Next; Next += 0x00600;
	DrvBlendTable	       = Next; Next += 0x00300;

	RamEnd                 = Next;

	DrvChars               = Next; Next += 1024 * 8 * 8;
	DrvBgTiles             = Next; Next += 1024 * 16 * 16;
	DrvSprites             = Next; Next += 1024 * 16 * 16;
	DrvPalette             = (UINT32*)Next; Next += 0x301 * sizeof(UINT32);

	DrvTempDraw	           = (UINT16*)Next; Next += 224 * 256 * sizeof(UINT16);

	MemEnd                 = Next;

	return 0;
}

static void bankswitch_main()
{
	ZetMapMemory(DrvZ80Rom1 + 0x10000 + DrvRomBank * 0x4000, 0x8000, 0xbfff, MAP_ROM);
}

static INT32 DrvDoReset()
{
	memset(AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	DrvRomBank = 0;
	bankswitch_main();
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	BurnYM2203Reset();
	ZetClose();

	DrvSoundLatch = 0;
	DrvFlipScreen = 0;
	DrvRomBank = 0;
	DrvVRamPage = 0;
	DrvTitleScreen = 0;
	DrvBgStatus = 0;
	DrvBgScrollX = 0;
	DrvBgScrollY = 0;
	DrvBgClipMode = 0;
	DrvBgClipMinX = 0;
	DrvBgClipMaxX = 0;
	DrvBgClipMinY = 0;
	DrvBgClipMaxY = 0;
	DrvBgSx1 = 0;
	DrvBgSy1 = 0;
	DrvBgSy2 = 0;

	return 0;
}

static inline UINT8 pal4bit(UINT8 bits)
{
	bits &= 0x0f;
	return (bits << 4) | bits;
}

static void DrvChangePalette(INT32 color, UINT32 offset)
{
	UINT8 lo = DrvPagedRam[0x2400 + (offset & ~1)];
	UINT8 hi = DrvPagedRam[0x2400 + (offset | 1)];

	DrvBlendTable[color] = hi & 0x0f;

	DrvPalette[color] = BurnHighCol(pal4bit(lo >> 4), pal4bit(lo), pal4bit(hi >> 4), 0);
}

static void DrvRecalcPalette()
{
	for (INT32 offset = 0x400; offset <= 0x5ff; offset++) {
		// Sprite color
		DrvChangePalette(((offset >> 1) & 0xff) + 0x000, offset - 0x400);
	}

	for (INT32 offset = 0x800; offset <= 0x9ff; offset++) {
		// BG color
		DrvChangePalette(((offset >> 1) & 0xff) + 0x100, offset - 0x400);
	}

	for (INT32 offset = 0xa00; offset <= 0xbff; offset++) {
		// Text color
		DrvChangePalette(((offset >> 1) & 0xff) + 0x200, offset - 0x400);
	}

	DrvRecalc = 0;
}

static UINT8 __fastcall DrvZ80Read1(UINT16 a)
{
	if (a >= 0xc000 && a <= 0xdfff) {
		UINT8* PagedRAM = (UINT8*)DrvPagedRam;
		if (DrvVRamPage == 1) PagedRAM = (UINT8*)DrvPagedRam + 0x2000;
		UINT32 offset = a & 0x1fff;

		if (DrvVRamPage == 1) {
			switch (offset) {
				case 0x00: return DrvInput[0];
				case 0x01: return DrvInput[1];
				case 0x02: return DrvInput[2];
				case 0x03: return DrvDip[0];
				case 0x04: return DrvDip[1];
			}
		}

		return PagedRAM[offset];
	}

	switch (a) {
		case 0xf001: {
			// nop
			return 0;
		}

		default: {
			bprintf(PRINT_NORMAL, _T("Z80 #1 Read => %04X\n"), a);
		}
	}

	return 0;
}

static void __fastcall DrvZ80Write1(UINT16 a, UINT8 d)
{
	if (a >= 0xc000 && a <= 0xdfff) {
		UINT8* PagedRAM = (UINT8*)DrvPagedRam;
		if (DrvVRamPage == 1) PagedRAM = (UINT8*)DrvPagedRam + 0x2000;
		UINT32 offset = a & 0x1fff;

		PagedRAM[offset] = d;

		if (offset == BG_SCROLLX_LSB || offset == BG_SCROLLX_MSB) {
			DrvBgScrollX = (DrvPagedRam[0x2000 + BG_SCROLLX_LSB] + ((DrvPagedRam[0x2000 + BG_SCROLLX_MSB] & 0x03) << 8)) & 0x3ff;
		}

		if (offset == BG_SCROLLY_LSB || offset == BG_SCROLLY_MSB) {
			DrvBgScrollY = (DrvPagedRam[0x2000 + BG_SCROLLY_LSB] + ((DrvPagedRam[0x2000 + BG_SCROLLY_MSB] & 0x01) << 8)) & 0x1ff;
		}

		if (offset == BG_SCREEN_MODE) {
			DrvBgStatus = DrvPagedRam[0x2000 + BG_SCREEN_MODE];
		}

		if (offset >= 0x400 && offset <= 0x5ff) {
			// Sprite color
			DrvChangePalette(((offset >> 1) & 0xff) + 0x000, offset - 0x400);
		}

		if (offset >= 0x800 && offset <= 0x9ff) {
			// BG color
			DrvChangePalette(((offset >> 1) & 0xff) + 0x100, offset - 0x400);
		}

		if (offset >= 0xa00 && offset <= 0xbff) {
			// Text color
			DrvChangePalette(((offset >> 1) & 0xff) + 0x200, offset - 0x400);
		}

		return;
	}

	if (a >= 0xf006 && a <= 0xf1ff) return; // nop

	switch (a) {
		case 0xf000: {
			DrvSoundLatch = d;
			return;
		}

		case 0xf001: {
			DrvFlipScreen = d & 0x80;
			return;
		}

		case 0xf002: {
			DrvRomBank = d & 0x03;
			bankswitch_main();
			return;
		}

		case 0xf003: {
			DrvVRamPage = d & 0x01;
			return;
		}

		case 0xf004: {
			// nop
			return;
		}

		case 0xf005: {
			DrvTitleScreen = d & 0x01;
			return;
		}

		default: {
			bprintf(PRINT_NORMAL, _T("Z80 #1 Write => %04X, %02X\n"), a, d);
		}
	}
}

static UINT8 __fastcall DrvZ80Read2(UINT16 a)
{
	switch (a) {
		case 0xe000: {
			return DrvSoundLatch;
		}

		default: {
			bprintf(PRINT_NORMAL, _T("Z80 #2 Read => %04X\n"), a);
		}
	}

	return 0;
}

static void __fastcall DrvZ80Write2(UINT16 a, UINT8 d)
{
	switch (a) {
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 #2 Write => %04X, %02X\n"), a, d);
		}
	}
}

static void __fastcall DrvZ80PortWrite2(UINT16 a, UINT8 d)
{
	a &= 0xff;

	switch (a) {
		case 0x00: {
			BurnYM2203Write(0, 0, d);
			return;
		}

		case 0x01: {
			BurnYM2203Write(0, 1, d);
			return;
		}

		case 0x80: {
			BurnYM2203Write(1, 0, d);
			return;
		}

		case 0x81: {
			BurnYM2203Write(1, 1, d);
			return;
		}

		default: {
			bprintf(PRINT_NORMAL, _T("Z80 #2 Port Write => %02X, %02X\n"), a, d);
		}
	}
}

static INT32 CharPlaneOffsets[4]    = { 0, 1, 2, 3 };
static INT32 CharXOffsets[8]        = { 0, 4, 8, 12, 16, 20, 24, 28 };
static INT32 CharYOffsets[8]        = { 0, 32, 64, 96, 128, 160, 192, 224 };
static INT32 SpritePlaneOffsets[4]  = { 0, 1, 2, 3 };
static INT32 SpriteXOffsets[16]     = { 0, 4, 8, 12, 16, 20, 24, 28, 512, 516, 520, 524, 528, 532, 536, 540 };
static INT32 SpriteYOffsets[16]     = { 0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 480 };

inline static void DrvYM2203IRQHandler(INT32, INT32 nStatus)
{
	ZetSetIRQLine(0, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 DrvInit()
{
	INT32 nRet = 0;

	BurnSetRefreshRate(54);

	BurnAllocMemIndex();

	DrvTempRom = (UINT8 *)BurnMalloc(0x20000);

	// Load Z80 #1 Program Roms
	nRet = BurnLoadRom(DrvZ80Rom1 + 0x00000, 0, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvZ80Rom1 + 0x10000, 1, 1); if (nRet != 0) return 1;

	// Load Z80 #2 Program Roms
	nRet = BurnLoadRom(DrvZ80Rom2 + 0x00000, 2, 1); if (nRet != 0) return 1;

	// Load and decode the sprites
	nRet = BurnLoadRom(DrvTempRom + 0x00000, 3, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x10000, 4, 1); if (nRet != 0) return 1;
	GfxDecode(1024, 4, 16, 16, SpritePlaneOffsets, SpriteXOffsets, SpriteYOffsets, 0x400, DrvTempRom, DrvSprites);

	// Load and decode the bg tiles
	memset(DrvTempRom, 0, 0x20000);
	nRet = BurnLoadRom(DrvTempRom + 0x00000,  5, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x10000,  6, 1); if (nRet != 0) return 1;
	GfxDecode(1024, 4, 16, 16, SpritePlaneOffsets, SpriteXOffsets, SpriteYOffsets, 0x400, DrvTempRom, DrvBgTiles);

	// Load and decode the chars
	memset(DrvTempRom, 0, 0x20000);
	nRet = BurnLoadRom(DrvTempRom, 7, 1); if (nRet != 0) return 1;
	GfxDecode(1024, 4, 8, 8, CharPlaneOffsets, CharXOffsets, CharYOffsets, 0x100, DrvTempRom, DrvChars);

	BurnFree(DrvTempRom);

	// Setup the Z80 emulation
	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80Rom1 + 0x00000, 0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80Rom1 + 0x10000, 0x8000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80Ram1          , 0xe000, 0xefff, MAP_RAM);
	ZetMapMemory(DrvSpriteRam        , 0xf200, 0xf7ff, MAP_RAM);
	ZetMapMemory(DrvZ80Ram1 + 0x1000 , 0xf800, 0xffff, MAP_RAM);
	ZetSetReadHandler(DrvZ80Read1);
	ZetSetWriteHandler(DrvZ80Write1);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80Rom2, 0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80Ram2, 0xc000, 0xc7ff, MAP_RAM);
	ZetSetReadHandler(DrvZ80Read2);
	ZetSetWriteHandler(DrvZ80Write2);
	ZetSetOutHandler(DrvZ80PortWrite2);
	ZetClose();

	BurnYM2203Init(2, 1500000, &DrvYM2203IRQHandler, 0);
	BurnTimerAttachZet(5000000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE, 0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.08, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.08, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.08, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_YM2203_ROUTE, 0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_1, 0.08, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_2, 0.08, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_3, 0.08, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	// Reset the driver
	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	ZetExit();
	BurnYM2203Exit();

	GenericTilesExit();

	BurnFreeMemIndex();

	return 0;
}

static inline UINT32 DrvBlendFunction(UINT32 dest, UINT32 addMe, UINT8 alpha)
{
	INT32 r = dest >> 16;
	INT32 g = (dest >> 8) & 0xff;
	INT32 b = dest & 0xff;

	UINT8 ir = addMe >> 16;
	UINT8 ig = addMe >> 8;
	UINT8 ib = addMe;

	if (alpha & 4) { r -= ir; if (r < 0) r = 0; } else { r += ir; if (r > 255) r = 255; }
	if (alpha & 2) { g -= ig; if (g < 0) g = 0; } else { g += ig; if (g > 255) g = 255; }
	if (alpha & 1) { b -= ib; if (b < 0) b = 0; } else { b += ib; if (b > 255) b = 255; }

	return (r << 16) | (g << 8) | b;
}

static void DrvSetBgColorIntensity()
{
	UINT16 intensity = DrvPagedRam[0x2400 + BG_PAL_INTENSITY_BU] | (DrvPagedRam[0x2400 + BG_PAL_INTENSITY_RG] << 8);

	UINT8 ir = pal4bit(intensity >> 12);
	UINT8 ig = pal4bit(intensity >>  8);
	UINT8 ib = pal4bit(intensity >>  4);
	UINT8 ix = intensity & 0x0f;

	UINT32 irgb = (ir << 16) + (ig << 8) + (ib);

	for (INT32 i = 0; i < 0x100; i++)
	{
		UINT8 lo = DrvPagedRam[0x2800+i*2];
		UINT8 hi = DrvPagedRam[0x2800+i*2+1];

		UINT8 r = pal4bit(lo >> 4);
		UINT8 g = pal4bit(lo);
		UINT8 b = pal4bit(hi >> 4);

		if (DrvBgStatus & 2)
		{
			UINT8 val = (r + g + b) / 3;
			UINT32 c = DrvBlendFunction((val<<16)+(val<<8)+val,irgb,ix);
			DrvPalette[0x100+i] = BurnHighCol(c>>16, (c>>8)&0xff, c&0xff, 0);
		}
		else
		{
			if (!(DrvTitleScreen & 1))
			{
				UINT32 c = DrvBlendFunction((r<<16)+(g<<8)+b,irgb,ix);
				DrvPalette[0x100+i] = BurnHighCol(c>>16, (c>>8)&0xff, c&0xff, 0);
			}
		}
	}
}

static void DrvEnableBgClipMode()
{
	DrvBgClipMinX = 0;
	DrvBgClipMinY = 0;
	DrvBgClipMaxX = nScreenWidth;
	DrvBgClipMaxY = nScreenHeight;

	if (!(DrvTitleScreen & 1))
	{
		DrvBgClipMode = 0;
		DrvBgSx1 = DrvBgSy1 = DrvBgSy2 = 0;
	}
	else
	{
		INT32 sy1_old = DrvBgSy1;
		INT32 sx1_old = DrvBgSx1;
		INT32 sy2_old = DrvBgSy2;

		DrvBgSy1 = DrvSpriteRam[11];
		DrvBgSx1 = DrvSpriteRam[12];
		DrvBgSy2 = DrvSpriteRam[11+128];

		switch (DrvBgClipMode)
		{
			case  0: case  4: if (sy1_old != DrvBgSy1) DrvBgClipMode++; break;
			case  2: case  6: if (sy2_old != DrvBgSy2) DrvBgClipMode++; break;
			case  8: case 10:
			case 12: case 14: if (sx1_old != DrvBgSx1) DrvBgClipMode++; break;
			case  1: case  5: if (DrvBgSy1 == 0xf0) DrvBgClipMode++; break;
			case  3: case  7: if (DrvBgSy2 == 0xf0) DrvBgClipMode++; break;
			case  9: case 11: if (DrvBgSx1 == 0xf0) DrvBgClipMode++; break;
			case 13: case 15: if (sx1_old == 0xf0) DrvBgClipMode++;
			case 16: if (DrvBgSy1 != 0x00) DrvBgClipMode = 0; break;
		}

		switch (DrvBgClipMode)
		{
			case  0: case  4: case  8: case 12: case 16: // bg "off" mode
				DrvBgClipMinX = 0;
				DrvBgClipMinY = 0;
				DrvBgClipMaxX = 0;
				DrvBgClipMaxY = 0;
			break;

			case  1: DrvBgClipMinY = DrvBgSy1; break;
			case  3: DrvBgClipMaxY = DrvBgSy2; break;
			case  5: DrvBgClipMaxY = DrvBgSy1; break;
			case  7: DrvBgClipMinY = DrvBgSy2; break;
			case  9: case 15: DrvBgClipMinX = DrvBgSx1; break;
			case 11: case 13: DrvBgClipMaxX = DrvBgSx1; break;
		}

		if (DrvBgClipMinY < 0) DrvBgClipMinY = 0;
		if (DrvBgClipMaxY > nScreenHeight) DrvBgClipMaxY = nScreenHeight;
		if (DrvBgClipMinX < 0) DrvBgClipMinX = 0;
		if (DrvBgClipMaxX > nScreenWidth) DrvBgClipMaxX = nScreenWidth;
	}
}

static void DrvRenderBgLayer()
{
	INT32 mx, my, Code, Color, Attr, x, y, TileIndex = 0, Flip, flipx, flipy;

	DrvSetBgColorIntensity();

	for (mx = 0; mx < 64; mx++) {
		for (my = 0; my < 32; my++) {
			INT32 offs = TileIndex << 1;
			Attr = DrvPagedRam[offs + 1];
			Code = DrvPagedRam[offs] | ((Attr & 0xc0) << 2);
			Color = Attr & 0x0f;
			Flip = (Attr & 0x30) >> 4;
			flipx = (Flip >> 0) & 0x01;
			flipy = (Flip >> 1) & 0x01;

			x = 16 * mx;
			y = 16 * my;

			x -= DrvBgScrollX & 0x3ff;
			y -= DrvBgScrollY & 0x1ff;

			if (x < -16) x += 1024;
			if (y < -16) y += 512;
			y -= 16;

			if (DrvFlipScreen) {
				x = 240 - x;
				y = 208 - y;
				flipx = !flipx;
				flipy = !flipy;
			}

			UINT8 *gfx = DrvBgTiles + (Code * 0x100);
			INT32 flip = ((flipx) ? 0x0f : 0) + ((flipy) ? 0xf0 : 0);
			Color = (Color * 16) + 0x100;

			for (INT32 sy = 0; sy < 16; sy++) {
				if ((sy + y) >= DrvBgClipMinY && (sy + y) < DrvBgClipMaxY) {
					for (INT32 sx = 0; sx < 16; sx++) {
						if ((sx + x) >= DrvBgClipMinX && (sx + x) < DrvBgClipMaxX) {
							pTransDraw[((sy+y)*nScreenWidth) + (sx+x)] = gfx[((sy*16)+sx)^flip] + Color;
						}
					}
				}
			}

			TileIndex++;
		}
	}
}

static void DrvRenderCharLayer()
{
	INT32 mx, my, Attr, Code, Color, x, y, TileIndex = 0, offs = 0, Flip, flipx, flipy;

	for (mx = 0; mx < 32; mx++) {
		for (my = 0; my < 32; my++) {
			offs = TileIndex << 1;
			Attr = DrvPagedRam[0x3000 + offs + 1];
			Code = DrvPagedRam[0x3000 + offs + 0] | ((Attr & 0xc0) << 2);
			Color = Attr & 0x0f;
			Flip = (Attr & 0x30) >> 4;
			flipx = (Flip >> 0) & 0x01;
			flipy = (Flip >> 1) & 0x01;

			x = 8 * mx;
			y = 8 * my;
			y -= 16;

			if (DrvFlipScreen) {
				x = 248 - x;
				y = 216 - y;
				flipx = !flipx;
				flipy = !flipy;
			}

			Draw8x8MaskTile(pTransDraw, Code, x, y, flipx, flipy, Color, 4, 0x0f, 0x200, DrvChars);

			TileIndex++;
		}
	}
}

static void DrvDrawBlendGfx(UINT8 *gfxbase,UINT32 code,UINT32 color,INT32 flipx,INT32 flipy,INT32 offsx,INT32 offsy,INT32 transparent_color)
{
	color *= 16;

	const UINT8 *alpha = DrvBlendTable + color;
	const UINT8 *source_base = gfxbase + (code * 16 * 16);

	INT32 xinc = flipx ? -1 : 1;
	INT32 yinc = flipy ? -1 : 1;

	INT32 x_index_base = flipx ? 16-1 : 0;
	INT32 y_index = flipy ? 16-1 : 0;

	INT32 sx = offsx;
	INT32 sy = offsy;
	INT32 ex = sx + 16;
	INT32 ey = sy + 16;

	INT32 min_x = 0;
	INT32 min_y = 0;
	INT32 max_x = (nScreenWidth - 1);
	INT32 max_y = (nScreenHeight - 1);

	if (sx < min_x)
	{
		int pixels = min_x-sx;
		sx += pixels;
		x_index_base += xinc*pixels;
	}

	if (sy < min_y)
	{
		int pixels = min_y-sy;
		sy += pixels;
		y_index += yinc*pixels;
	}

	if (ex > max_x+1) ex = max_x+1;
	if (ey > max_y+1) ey = max_y+1;

	if (ex > sx)
	{
		for (INT32 y = sy; y < ey; y++)
		{
			const UINT8 *source = source_base + y_index*16;

			UINT16 *dest = pTransDraw + (y * nScreenWidth);
			UINT16 *dst2 = DrvTempDraw + (y * nScreenWidth);

			INT32 x_index = x_index_base;

			for (INT32 x = sx; x < ex; x++)
			{
				int c = source[x_index];
				if (c != transparent_color)
				{
					if (alpha[c] & 8)
					{
						dest[x] += 0x8000;
						dst2[x] = c + color + (alpha[c] * 0x0400);
					}
					else
					{
						dest[x] = c + color;
					}
				}
				x_index += xinc;
			}
			y_index += yinc;
		}
	}
}

static void DrvRenderSprites()
{
	for (INT32 Offs = 0; Offs < 0x600; Offs += 16) {
		INT32 Attr  = DrvSpriteRam[Offs + 13];
		INT32 Code  = DrvSpriteRam[Offs + 14] | ((Attr & 0xc0) << 2);
		INT32 Color = DrvSpriteRam[Offs + 15] & 0x0f;
		INT32 flipx = Attr & 0x10;
		INT32 flipy = Attr & 0x20;
		INT32 sx = DrvSpriteRam[Offs + 12];
		INT32 sy = DrvSpriteRam[Offs + 11];
		INT32 Size = (Attr & 0x08) ? 32 : 16;

		if (Attr & 0x01) sx -= 256;
		if (Attr & 0x04) sy -= 256;

		if (DrvFlipScreen) {
			sx = 224 - sx;
			sy = 224 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		if (Size == 32) {
			INT32 x0, x1, y0, y1;

			if (flipx) { x0 = 2; x1 = 0; }
			else { x0 = 0; x1 = 2; }

			if (flipy) { y0 = 1; y1 = 0; }
			else { y0 = 0; y1 = 1; }

			DrvDrawBlendGfx(DrvSprites, Code + x0 + y0, Color, flipx, flipy, sx, sy - 16, 15);
			DrvDrawBlendGfx(DrvSprites, Code + x0 + y1, Color, flipx, flipy, sx, sy, 15);
			DrvDrawBlendGfx(DrvSprites, Code + x1 + y0, Color, flipx, flipy, sx + 16, sy - 16, 15);
			DrvDrawBlendGfx(DrvSprites, Code + x1 + y1, Color, flipx, flipy, sx + 16, sy, 15);
		} else {
			if (DrvFlipScreen)
				DrvDrawBlendGfx(DrvSprites, Code, Color, flipx, flipy, sx + 16, sy, 15);
			else
				DrvDrawBlendGfx(DrvSprites, Code, Color, flipx, flipy, sx, sy-16, 15);
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvRecalcPalette();
	}

	DrvPalette[0x300] = 0; // black

	BurnTransferClear(0x300);

	if (DrvBgStatus & 0x01)
		if (nBurnLayer & 1) DrvRenderBgLayer();

	if (!(DrvTitleScreen & 0x01))
		if (nSpriteEnable & 1) DrvRenderSprites();

	if (nBurnLayer & 2) DrvRenderCharLayer();

	if (nBurnBpp > 2)
	{
		UINT16* pSrc = pTransDraw;
		UINT16* pAlp = DrvTempDraw;
		UINT8* pDest = pBurnDraw;

		if (nBurnBpp == 3) {
			for (INT32 y = 0; y < nScreenHeight; y++, pSrc += nScreenWidth, pAlp += nScreenWidth, pDest += nBurnPitch) {
				for (INT32 x = 0; x < nScreenWidth; x++)
				{
					UINT32 c = pSrc[x];

					if (c & 0x8000) {
						c = DrvBlendFunction(DrvPalette[c&0x3ff], DrvPalette[pAlp[x]&0x3ff], pAlp[x] / 0x0400);
					} else {
						c = DrvPalette[c];
					}

					*(pDest + (x * 3) + 0) = c & 0xFF;
					*(pDest + (x * 3) + 1) = (c >> 8) & 0xFF;
					*(pDest + (x * 3) + 2) = c >> 16;

				}
			}
		} else { // nBurnBpp == 4
			for (INT32 y = 0; y < nScreenHeight; y++, pSrc += nScreenWidth, pAlp += nScreenWidth, pDest += nBurnPitch) {
				for (INT32 x = 0; x < nScreenWidth; x++)
				{
					if (pSrc[x] & 0x8000) {
						((UINT32*)pDest)[x] = DrvBlendFunction(DrvPalette[pSrc[x]&0x3ff], DrvPalette[pAlp[x]&0x3ff], pAlp[x] / 0x0400);
					} else {
						((UINT32*)pDest)[x] = DrvPalette[pSrc[x]];
					}
				}
			}
		}
	} else { // skip alpha for now on anything less than 24-bit
		for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
			if (pTransDraw[i] & 0x8000) {
				pTransDraw[i] = DrvTempDraw[i] & 0x3ff;
			} else {
				pTransDraw[i] &= 0x3ff;
			}
		}

		BurnTransferCopy(DrvPalette);
	}

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) DrvDoReset();

	{
		memset(DrvInput, 0xff, sizeof(DrvInput));

		for (INT32 i = 0; i < 8; i++) {
			DrvInput[0] ^= (DrvJoy0[i] & 1) << i;
			DrvInput[1] ^= (DrvJoy1[i] & 1) << i;
			DrvInput[2] ^= (DrvJoy2[i] & 1) << i;
		}

		DrvClearOppositesActiveLow(&DrvInput[1]);
		DrvClearOppositesActiveLow(&DrvInput[2]);
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 6000000 / 54, 5000000 / 54 };
	INT32 nCyclesDone[2] = { 0, 0 };

	ZetNewFrame();

	for (INT32 i = 0; i < nInterleave; i++) {
		ZetOpen(0);
		CPU_RUN(0, Zet);
		if (i == 16) {
			ZetSetVector(0xcf);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
		if (i == nInterleave - 1) {
			ZetSetVector(0xd7);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
		ZetClose();

		ZetOpen(1);
		CPU_RUN_TIMER(1);
		ZetClose();
	}

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
	}

	DrvEnableBgClipMode(); // must run every frame, drawn or not.

	if (pBurnDraw) DrvDraw();

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
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		ZetScan(nAction);

		BurnYM2203Scan(nAction, pnMin);

		SCAN_VAR(DrvSoundLatch);
		SCAN_VAR(DrvFlipScreen);
		SCAN_VAR(DrvRomBank);
		SCAN_VAR(DrvVRamPage);
		SCAN_VAR(DrvTitleScreen);
		SCAN_VAR(DrvBgScrollX);
		SCAN_VAR(DrvBgScrollY);
		SCAN_VAR(DrvBgStatus);
		SCAN_VAR(DrvBgClipMode);
		SCAN_VAR(DrvBgClipMinX);
		SCAN_VAR(DrvBgClipMaxX);
		SCAN_VAR(DrvBgClipMinY);
		SCAN_VAR(DrvBgClipMaxY);
		SCAN_VAR(DrvBgSx1);
		SCAN_VAR(DrvBgSy1);
		SCAN_VAR(DrvBgSy2);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		bankswitch_main();
		ZetClose();
	}

	return 0;
}

static struct BurnRomInfo DrvRomDesc[] = {
// "Oversea's version V2.00 CHANGED BY TAMIO NAKASATO" text present in ROM, various modifications (English names, more complete attract demo etc.)
	{ "myp5d",         0x08000, 0x1d40a8c7, BRF_ESS | BRF_PRG }, //  0  Z80 #1 Program Code
	{ "myp5e",         0x10000, 0x2fa7e8c0, BRF_ESS | BRF_PRG }, //	 1

	{ "myp5a",         0x10000, 0x6efee094, BRF_ESS | BRF_PRG }, //  2  Z80 #2 Program

	{ "p5b",           0x10000, 0x7e3f87d4, BRF_GRA },	         //  3  Sprites
	{ "p5c",           0x10000, 0x8710fedb, BRF_GRA },	         //  4

	{ "myp5g",         0x10000, 0x617b074b, BRF_GRA },	         //  5  BG Tiles
	{ "myp5h",         0x10000, 0xa9dfbe67, BRF_GRA },	         //  6

	{ "p5f",           0x08000, 0x04d7e21c, BRF_GRA },	         //  7  FG Tiles

	{ "my10.7l",       0x00200, 0x6a7d13c0, BRF_OPT },	         //  8  PROMs
	{ "my09.3t",       0x00400, 0x59e44236, BRF_OPT },	         //  9
};

STD_ROM_PICK(Drv)
STD_ROM_FN(Drv)

struct BurnDriver BurnDrvPsychic5 = {
	"psychic5", NULL, NULL, NULL, "1987",
	"Psychic 5 (World)\0", NULL, "Jaleco / NMK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, DrvRomInfo, DrvRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x300, 224, 256, 3, 4
};

static struct BurnRomInfo Psychic5jRomDesc[] = {
	{ "4.7a",		0x08000, 0x90259249, BRF_ESS | BRF_PRG }, //  0  Z80 #1 Program Code
	{ "5.7c",		0x10000, 0x72298f34, BRF_ESS | BRF_PRG }, //  1

	{ "1.2b",		0x10000, 0x6efee094, BRF_ESS | BRF_PRG }, //  2  Z80 #2 Program

	{ "2.4p",		0x10000, 0x7e3f87d4, BRF_GRA },	          //  3  Sprites
	{ "3.4r",		0x10000, 0x8710fedb, BRF_GRA },	          //  4

	{ "7.2k",		0x10000, 0xf9262f32, BRF_GRA },	          //  5  BG Tiles
	{ "8.2m",		0x10000, 0xc411171a, BRF_GRA },	          //  6

	{ "6.5f",		0x08000, 0x04d7e21c, BRF_GRA },	          //  7  FG Tiles

	{ "my10.7l",	0x00200, 0x6a7d13c0, BRF_OPT },	          //  8  PROMs
	{ "my09.3t",	0x00400, 0x59e44236, BRF_OPT },	          //  9
};

STD_ROM_PICK(Psychic5j)
STD_ROM_FN(Psychic5j)

struct BurnDriver BurnDrvPsychic5j = {
	"psychic5j", "psychic5", NULL, NULL, "1987",
	"Psychic 5 (Japan)\0", NULL, "Jaleco / NMK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, Psychic5jRomInfo, Psychic5jRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x300, 224, 256, 3, 4
};
