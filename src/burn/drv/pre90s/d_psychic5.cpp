#include "tiles_generic.h"
#include "z80_intf.h"
#include "burn_ym2203.h"

// TODO
// sprite alpha-blending
// sprite optimisation
// background clipping
// background palette intensity

#define BG_SCROLLX_LSB      0x308
#define BG_SCROLLX_MSB      0x309
#define BG_SCROLLY_LSB      0x30a
#define BG_SCROLLY_MSB      0x30b
#define BG_SCREEN_MODE      0x30c
#define BG_PAL_INTENSITY_RG 0x1fe
#define BG_PAL_INTENSITY_BU 0x1ff

static UINT8 DrvInputPort0[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvInputPort1[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvInputPort2[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvDip[2]        = {0, 0};
static UINT8 DrvInput[3]      = {0x00, 0x00, 0x00};
static UINT8 DrvReset         = 0;

static UINT8 *Mem                  = NULL;
static UINT8 *MemEnd               = NULL;
static UINT8 *RamStart             = NULL;
static UINT8 *RamEnd               = NULL;
static UINT8 *DrvZ80Rom1           = NULL;
static UINT8 *DrvZ80Rom2           = NULL;
static UINT8 *DrvZ80Ram1           = NULL;
static UINT8 *DrvZ80Ram2           = NULL;
static UINT8 *DrvPagedRam          = NULL;
static UINT8 *DrvSpriteRam         = NULL;
static UINT8 *DrvChars             = NULL;
static UINT8 *DrvBgTiles           = NULL;
static UINT8 *DrvSprites           = NULL;
static UINT8 *DrvTempRom           = NULL;
static UINT32 *DrvPalette          = NULL;

static UINT8 DrvSoundLatch         = 0;
static UINT8 DrvFlipScreen         = 0;
static UINT8 DrvRomBank            = 0;
static UINT8 DrvVRamPage           = 0;
static UINT8 DrvTitlePage          = 0;
static UINT8 DrvBgStatus           = 0;
static UINT16 DrvBgScrollX         = 0;
static UINT16 DrvBgScrollY         = 0;

static INT32 nCyclesDone[2], nCyclesTotal[2];
static INT32 nCyclesSegment;

static struct BurnInputInfo DrvInputList[] =
{
	{"Coin 1"            , BIT_DIGITAL  , DrvInputPort0 + 6, "p1 coin"   },
	{"Start 1"           , BIT_DIGITAL  , DrvInputPort0 + 0, "p1 start"  },
	{"Coin 2"            , BIT_DIGITAL  , DrvInputPort0 + 7, "p2 coin"   },
	{"Start 2"           , BIT_DIGITAL  , DrvInputPort0 + 1, "p2 start"  },

	{"P1 Up"             , BIT_DIGITAL  , DrvInputPort1 + 3, "p1 up"     },
	{"P1 Down"           , BIT_DIGITAL  , DrvInputPort1 + 2, "p1 down"   },
	{"P1 Left"           , BIT_DIGITAL  , DrvInputPort1 + 1, "p1 left"   },
	{"P1 Right"          , BIT_DIGITAL  , DrvInputPort1 + 0, "p1 right"  },
	{"P1 Fire 1"         , BIT_DIGITAL  , DrvInputPort1 + 5, "p1 fire 1" },
	{"P1 Fire 2"         , BIT_DIGITAL  , DrvInputPort1 + 4, "p1 fire 2" },
	
	{"P2 Up"             , BIT_DIGITAL  , DrvInputPort2 + 3, "p2 up"     },
	{"P2 Down"           , BIT_DIGITAL  , DrvInputPort2 + 2, "p2 down"   },
	{"P2 Left"           , BIT_DIGITAL  , DrvInputPort2 + 1, "p2 left"   },
	{"P2 Right"          , BIT_DIGITAL  , DrvInputPort2 + 0, "p2 right"  },
	{"Fire 1"            , BIT_DIGITAL  , DrvInputPort2 + 4, "p2 fire 1" },
	{"Fire 2"            , BIT_DIGITAL  , DrvInputPort2 + 5, "p2 fire 2" },

	{"Reset"             , BIT_DIGITAL  , &DrvReset        , "reset"     },
	{"Dip 1"             , BIT_DIPSWITCH, DrvDip + 0       , "dip"       },
	{"Dip 2"             , BIT_DIPSWITCH, DrvDip + 1       , "dip"       },
};

STDINPUTINFO(Drv)

static inline void DrvClearOpposites(UINT8* nJoystickInputs)
{
	if ((*nJoystickInputs & 0x03) == 0x03) {
		*nJoystickInputs &= ~0x03;
	}
	if ((*nJoystickInputs & 0x0c) == 0x0c) {
		*nJoystickInputs &= ~0x0c;
	}
}

static inline void DrvMakeInputs()
{
	// Reset Inputs
	DrvInput[0] = DrvInput[1] = DrvInput[2] = 0x00;

	// Compile Digital Inputs
	for (INT32 i = 0; i < 8; i++) {
		DrvInput[0] |= (DrvInputPort0[i] & 1) << i;
		DrvInput[1] |= (DrvInputPort1[i] & 1) << i;
		DrvInput[2] |= (DrvInputPort2[i] & 1) << i;
	}

	// Clear Opposites
	DrvClearOpposites(&DrvInput[1]);
	DrvClearOpposites(&DrvInput[2]);
}

static struct BurnDIPInfo DrvDIPList[]=
{
	// Default Values
	{0x11, 0xff, 0xff, 0xf0, NULL                     },
	{0x12, 0xff, 0xff, 0xff, NULL                     },

	// Dip 1
	{0   , 0xfe, 0   , 2   , "Flip Screen"            },
	{0x11, 0x01, 0x01, 0x01, "Off"                    },
	{0x11, 0x01, 0x01, 0x00, "On"                     },
	
	{0   , 0xfe, 0   , 2   , "Difficulty"             },
	{0x11, 0x01, 0x08, 0x08, "Normal"                 },
	{0x11, 0x01, 0x08, 0x00, "Hard"                   },
	
	{0   , 0xfe, 0   , 2   , "Cabinet"                },
	{0x11, 0x01, 0x10, 0x00, "Upright"                },
	{0x11, 0x01, 0x10, 0x10, "Cocktail"               },
	
	{0   , 0xfe, 0   , 2   , "Demo Sounds"            },
	{0x11, 0x01, 0x20, 0x00, "Off"                    },
	{0x11, 0x01, 0x20, 0x20, "On"                     },
	
	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x11, 0x01, 0xc0, 0x80, "2"                      },
	{0x11, 0x01, 0xc0, 0xc0, "3"                      },
	{0x11, 0x01, 0xc0, 0x40, "4"                      },
	{0x11, 0x01, 0xc0, 0x00, "5"                      },

	// Dip 2
	{0   , 0xfe, 0   , 2   , "Invulnerability (Cheat)"},
	{0x12, 0x01, 0x01, 0x01, "Off"                    },
	{0x12, 0x01, 0x01, 0x00, "On"                     },
	
	{0   , 0xfe, 0   , 8   , "Coin A"                 },
	{0x12, 0x01, 0xe0, 0x00, "5 Coins 1 Play"         },
	{0x12, 0x01, 0xe0, 0x20, "4 Coins 1 Play"         },
	{0x12, 0x01, 0xe0, 0x40, "3 Coins 1 Play"         },
	{0x12, 0x01, 0xe0, 0x60, "2 Coins 1 Play"         },
	{0x12, 0x01, 0xe0, 0xe0, "1 Coin 1 Play"          },
	{0x12, 0x01, 0xe0, 0xc0, "1 Coin 2 Plays"         },
	{0x12, 0x01, 0xe0, 0xa0, "1 Coin 3 Plays"         },
	{0x12, 0x01, 0xe0, 0x80, "1 Coin 4 Plays"         },

	{0   , 0xfe, 0   , 8   , "Coin B"                 },
	{0x12, 0x01, 0x1c, 0x00, "5 Coins 1 Play"         },
	{0x12, 0x01, 0x1c, 0x04, "4 Coins 1 Play"         },
	{0x12, 0x01, 0x1c, 0x08, "3 Coins 1 Play"         },
	{0x12, 0x01, 0x1c, 0x0c, "2 Coins 1 Play"         },
	{0x12, 0x01, 0x1c, 0x1c, "1 Coin 1 Play"          },
	{0x12, 0x01, 0x1c, 0x18, "1 Coin 2 Plays"         },
	{0x12, 0x01, 0x1c, 0x14, "1 Coin 3 Plays"         },
	{0x12, 0x01, 0x1c, 0x10, "1 Coin 4 Plays"         },
};

STDDIPINFO(Drv)

static struct BurnRomInfo DrvRomDesc[] = {
	{ "p5d",           0x08000, 0x90259249, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "p5e",           0x10000, 0x72298f34, BRF_ESS | BRF_PRG }, //	 1
	
	{ "p5a",           0x08000, 0x50060ecd, BRF_ESS | BRF_PRG }, //  2	Z80 #2 Program
	
	{ "p5b",           0x10000, 0x7e3f87d4, BRF_GRA },	     //  3	Sprites
	{ "p5c",           0x10000, 0x8710fedb, BRF_GRA },	     //  4

	{ "p5g",           0x10000, 0xf9262f32, BRF_GRA },	     //  5	BG Tiles
	{ "p5h",           0x10000, 0xc411171a, BRF_GRA },	     //  6
	
	{ "p5f",           0x08000, 0x04d7e21c, BRF_GRA },	     //  7  FG Tiles
	
	{ "my10.7l",       0x00200, 0x6a7d13c0, BRF_OPT },	     //  8	PROMs
	{ "my09.3t",       0x00200, 0x59e44236, BRF_OPT },	     //  9
};

STD_ROM_PICK(Drv)
STD_ROM_FN(Drv)

static INT32 MemIndex()
{
	UINT8 *Next; Next = Mem;

	DrvZ80Rom1             = Next; Next += 0x20000;
	DrvZ80Rom2             = Next; Next += 0x10000;
	
	RamStart               = Next;

	DrvZ80Ram1             = Next; Next += 0x01800;
	DrvZ80Ram2             = Next; Next += 0x00800;
	DrvPagedRam            = Next; Next += 0x04000;
	DrvSpriteRam           = Next; Next += 0x00600;

	RamEnd                 = Next;

	DrvChars               = Next; Next += 1024 * 8 * 8;
	DrvBgTiles             = Next; Next += 1024 * 16 * 16;
	DrvSprites             = Next; Next += 1024 * 16 * 16;
	DrvPalette             = (UINT32*)Next; Next += 0x300 * sizeof(UINT32);

	MemEnd                 = Next;

	return 0;
}

static INT32 DrvDoReset()
{
	ZetOpen(0);
	ZetReset();
	ZetClose();
	
	ZetOpen(1);
	ZetReset();
	BurnYM2203Reset();
	ZetClose();
	
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
//	jal_blend_set(color, hi & 0x0f);
	
	DrvPalette[color] = BurnHighCol(pal4bit(lo >> 4), pal4bit(lo), pal4bit(hi >> 4), 0);
}

UINT8 __fastcall DrvZ80Read1(UINT16 a)
{
	if (a >= 0xc000 && a <= 0xdfff) {
		UINT8* PagedRAM = (UINT8*)DrvPagedRam;
		if (DrvVRamPage == 1) PagedRAM = (UINT8*)DrvPagedRam + 0x2000;
		UINT32 offset = a - 0xc000;
		
		if (DrvVRamPage == 1) {
			switch (offset) {
				case 0x00: return 0xff - DrvInput[0];
				case 0x01: return 0xff - DrvInput[1];
				case 0x02: return 0xff - DrvInput[2];
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

void __fastcall DrvZ80Write1(UINT16 a, UINT8 d)
{
	if (a >= 0xc000 && a <= 0xdfff) {
		UINT8* PagedRAM = (UINT8*)DrvPagedRam;
		if (DrvVRamPage == 1) PagedRAM = (UINT8*)DrvPagedRam + 0x2000;
		UINT32 offset = a - 0xc000;
		
		PagedRAM[offset] = d;
		
		if (offset == BG_SCROLLX_LSB || offset == BG_SCROLLX_MSB) {
			DrvBgScrollX = DrvPagedRam[0x2000 + BG_SCROLLX_LSB] | (DrvPagedRam[0x2000 + BG_SCROLLX_MSB] << 8);
		}
		
		if (offset == BG_SCROLLY_LSB || offset == BG_SCROLLY_MSB) {
			DrvBgScrollY = DrvPagedRam[0x2000 + BG_SCROLLY_LSB] | (DrvPagedRam[0x2000 + BG_SCROLLY_MSB] << 8);
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
			ZetMapArea(0x8000, 0xbfff, 0, DrvZ80Rom1 + 0x10000 + DrvRomBank * 0x4000 );
			ZetMapArea(0x8000, 0xbfff, 2, DrvZ80Rom1 + 0x10000 + DrvRomBank * 0x4000 );
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
			DrvTitlePage = d;
			return;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 #1 Write => %04X, %02X\n"), a, d);
		}
	}
}

UINT8 __fastcall DrvZ80PortRead1(UINT16 a)
{
	a &= 0xff;
	
	switch (a) {
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 #1 Port Read => %02X\n"), a);
		}
	}

	return 0;
}

void __fastcall DrvZ80PortWrite1(UINT16 a, UINT8 d)
{
	a &= 0xff;
	
	switch (a) {
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 #1 Port Write => %02X, %02X\n"), a, d);
		}
	}
}

UINT8 __fastcall DrvZ80Read2(UINT16 a)
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

void __fastcall DrvZ80Write2(UINT16 a, UINT8 d)
{
	switch (a) {
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 #2 Write => %04X, %02X\n"), a, d);
		}
	}
}

UINT8 __fastcall DrvZ80PortRead2(UINT16 a)
{
	a &= 0xff;
	
	switch (a) {
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 #2 Port Read => %02X\n"), a);
		}
	}

	return 0;
}

void __fastcall DrvZ80PortWrite2(UINT16 a, UINT8 d)
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
	if (nStatus & 1) {
		ZetSetIRQLine(0xff, ZET_IRQSTATUS_ACK);
	} else {
		ZetSetIRQLine(0,    ZET_IRQSTATUS_NONE);
	}
}

inline static INT32 DrvSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)(ZetTotalCycles() * nSoundRate / 6000000);
}

inline static double DrvGetTime()
{
	return (double)ZetTotalCycles() / 6000000;
}

static INT32 DrvInit()
{
	INT32 nRet = 0, nLen;

	// Allocate and Blank all required memory
	Mem = NULL;
	MemIndex();
	nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(Mem, 0, nLen);
	MemIndex();

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
	ZetSetReadHandler(DrvZ80Read1);
	ZetSetWriteHandler(DrvZ80Write1);
	ZetSetInHandler(DrvZ80PortRead1);
	ZetSetOutHandler(DrvZ80PortWrite1);
	ZetMapArea(0x0000, 0x7fff, 0, DrvZ80Rom1             );
	ZetMapArea(0x0000, 0x7fff, 2, DrvZ80Rom1             );
	ZetMapArea(0x8000, 0xbfff, 0, DrvZ80Rom1 + 0x10000   );
	ZetMapArea(0x8000, 0xbfff, 2, DrvZ80Rom1 + 0x10000   );
	ZetMapArea(0xe000, 0xefff, 0, DrvZ80Ram1             );
	ZetMapArea(0xe000, 0xefff, 1, DrvZ80Ram1             );
	ZetMapArea(0xe000, 0xefff, 2, DrvZ80Ram1             );
	ZetMapArea(0xf200, 0xf7ff, 0, DrvSpriteRam           );
	ZetMapArea(0xf200, 0xf7ff, 1, DrvSpriteRam           );
	ZetMapArea(0xf200, 0xf7ff, 2, DrvSpriteRam           );
	ZetMapArea(0xf800, 0xffff, 0, DrvZ80Ram1 + 0x1000    );
	ZetMapArea(0xf800, 0xffff, 1, DrvZ80Ram1 + 0x1000    );
	ZetMapArea(0xf800, 0xffff, 2, DrvZ80Ram1 + 0x1000    );
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetSetReadHandler(DrvZ80Read2);
	ZetSetWriteHandler(DrvZ80Write2);
	ZetSetInHandler(DrvZ80PortRead2);
	ZetSetOutHandler(DrvZ80PortWrite2);
	ZetMapArea(0x0000, 0x7fff, 0, DrvZ80Rom2             );
	ZetMapArea(0x0000, 0x7fff, 2, DrvZ80Rom2             );
	ZetMapArea(0xc000, 0xc7ff, 0, DrvZ80Ram2             );
	ZetMapArea(0xc000, 0xc7ff, 1, DrvZ80Ram2             );
	ZetMapArea(0xc000, 0xc7ff, 2, DrvZ80Ram2             );
	ZetClose();
	
	BurnYM2203Init(2, 1500000, &DrvYM2203IRQHandler, DrvSynchroniseStream, DrvGetTime, 0);
	BurnTimerAttachZet(6000000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_YM2203_ROUTE, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_1, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_2, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_3, 0.15, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	
	BurnSetRefreshRate(54);

	// Reset the driver
	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	ZetExit();
	BurnYM2203Exit();
	
	GenericTilesExit();
	
	DrvSoundLatch = 0;
	DrvFlipScreen = 0;
	DrvRomBank = 0;
	DrvVRamPage = 0;
	DrvTitlePage = 0;
	DrvBgScrollX = 0;
	DrvBgScrollY = 0;
	DrvBgStatus = 0;
	
	BurnFree(Mem);

	return 0;
}

static void DrvRenderBgLayer()
{
	INT32 mx, my, Code, Colour, Attr, x, y, TileIndex = 0, Flip, xFlip, yFlip;
	
	for (mx = 0; mx < 64; mx++) {
		for (my = 0; my < 32; my++) {
			INT32 offs = TileIndex << 1;
			Attr = DrvPagedRam[offs + 1];
			Code = DrvPagedRam[offs] | ((Attr & 0xc0) << 2);
			Colour = Attr & 0x0f;
			Flip = (Attr & 0x30) >> 4;
			xFlip = (Flip >> 0) & 0x01;
			yFlip = (Flip >> 1) & 0x01;
			
			x = 16 * mx;
			y = 16 * my;
			
			x -= DrvBgScrollX;
			y -= DrvBgScrollY;
			
			if (x < -16) x += 1024;
			if (y < -16) y += 512;
			y -= 16;
			
			if (DrvFlipScreen) {
				x = 240 - x;
				y = 208 - y;
				xFlip = !xFlip;
				yFlip = !yFlip;
			}

			if (x > 16 && x < 240 && y > 16 && y < 208) {
				if (xFlip) {
					if (yFlip) {
						Render16x16Tile_FlipXY(pTransDraw, Code, x, y, Colour, 4, 256, DrvBgTiles);
					} else {
						Render16x16Tile_FlipX(pTransDraw, Code, x, y, Colour, 4, 256, DrvBgTiles);
					}
				} else {
					if (yFlip) {
						Render16x16Tile_FlipY(pTransDraw, Code, x, y, Colour, 4, 256, DrvBgTiles);
					} else {
						Render16x16Tile(pTransDraw, Code, x, y, Colour, 4, 256, DrvBgTiles);
					}
				}
			} else {
				if (xFlip) {
					if (yFlip) {
						Render16x16Tile_FlipXY_Clip(pTransDraw, Code, x, y, Colour, 4, 256, DrvBgTiles);
					} else {
						Render16x16Tile_FlipX_Clip(pTransDraw, Code, x, y, Colour, 4, 256, DrvBgTiles);
					}
				} else {
					if (yFlip) {
						Render16x16Tile_FlipY_Clip(pTransDraw, Code, x, y, Colour, 4, 256, DrvBgTiles);
					} else {
						Render16x16Tile_Clip(pTransDraw, Code, x, y, Colour, 4, 256, DrvBgTiles);
					}
				}
			}
			
			TileIndex++;
		}
	}
}

static void DrvRenderCharLayer()
{
	INT32 mx, my, Attr, Code, Colour, x, y, TileIndex = 0, offs = 0, Flip, xFlip, yFlip;
	
	for (mx = 0; mx < 32; mx++) {
		for (my = 0; my < 32; my++) {
			offs = TileIndex << 1;
			Attr = DrvPagedRam[0x3000 + offs + 1];
			Code = DrvPagedRam[0x3000 + offs + 0] | ((Attr & 0xc0) << 2);;
			Colour = Attr & 0x0f;
			Flip = (Attr & 0x30) >> 4;
			xFlip = (Flip >> 0) & 0x01;
			yFlip = (Flip >> 1) & 0x01;
			
			x = 8 * mx;
			y = 8 * my;
			y -= 16;
			
			if (DrvFlipScreen) {
				x = 248 - x;
				y = 216 - y;
				xFlip = !xFlip;
				yFlip = !yFlip;
			}
			
			if (x > 0 && x < 248 && y > 0 && y < 216) {
				if (xFlip) {
					if (yFlip) {
						Render8x8Tile_Mask_FlipXY(pTransDraw, Code, x, y, Colour, 4, 0x0f, 0x200, DrvChars);
					} else {
						Render8x8Tile_Mask_FlipX(pTransDraw, Code, x, y, Colour, 4, 0x0f, 0x200, DrvChars);
					}
				} else {
					if (yFlip) {
						Render8x8Tile_Mask_FlipY(pTransDraw, Code, x, y, Colour, 4, 0x0f, 0x200, DrvChars);
					} else {
						Render8x8Tile_Mask(pTransDraw, Code, x, y, Colour, 4, 0x0f, 0x200, DrvChars);
					}
				}
			} else {
				if (xFlip) {
					if (yFlip) {
						Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, Code, x, y, Colour, 4, 0x0f, 0x200, DrvChars);
					} else {
						Render8x8Tile_Mask_FlipX_Clip(pTransDraw, Code, x, y, Colour, 4, 0x0f, 0x200, DrvChars);
					}
				} else {
					if (yFlip) {
						Render8x8Tile_Mask_FlipY_Clip(pTransDraw, Code, x, y, Colour, 4, 0x0f, 0x200, DrvChars);
					} else {
						Render8x8Tile_Mask_Clip(pTransDraw, Code, x, y, Colour, 4, 0x0f, 0x200, DrvChars);
					}
				}
			}

			TileIndex++;
		}
	}
}

//#define DRAW_SPRITE(code, sx, sy) jal_blend_drawgfx(m_palette, bitmap, cliprect, m_gfxdecode->gfx(0), code, color, flipx, flipy, sx, sy, 15);

#define DRAW_SPRITE(Code1, sx1, sy1) \
	if (xFlip) { \
		if (yFlip) { \
			Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, Code1, sx1, sy1 - 16, Colour, 4, 0x0f, 0, DrvSprites); \
		} else { \
			Render16x16Tile_Mask_FlipX_Clip(pTransDraw, Code1, sx1, sy1 - 16, Colour, 4, 0x0f, 0, DrvSprites); \
		} \
	} else { \
		if (yFlip) { \
			Render16x16Tile_Mask_FlipY_Clip(pTransDraw, Code1, sx1, sy1 - 16, Colour, 4, 0x0f, 0, DrvSprites); \
		} else { \
			Render16x16Tile_Mask_Clip(pTransDraw, Code1, sx1, sy1 - 16, Colour, 4, 0x0f, 0, DrvSprites); \
		} \
	}

static void DrvRenderSprites()
{
	for (INT32 Offs = 0; Offs < 0x600; Offs += 16) {
		INT32 Attr  = DrvSpriteRam[Offs + 13];
		INT32 Code  = DrvSpriteRam[Offs + 14] | ((Attr & 0xc0) << 2);
		INT32 Colour = DrvSpriteRam[Offs + 15] & 0x0f;
		INT32 xFlip = Attr & 0x10;
		INT32 yFlip = Attr & 0x20;
		INT32 sx = DrvSpriteRam[Offs + 12];
		INT32 sy = DrvSpriteRam[Offs + 11];
		INT32 Size = (Attr & 0x08) ? 32 : 16;

		if (Attr & 0x01) sx -= 256;
		if (Attr & 0x04) sy -= 256;

		if (DrvFlipScreen) {
			sx = 224 - sx;
			sy = 224 - sy;
			xFlip = !xFlip;
			yFlip = !yFlip;
		}
		
		if (Size == 32) {
			INT32 x0, x1, y0, y1;

			if (xFlip) { x0 = 2; x1 = 0; }
			else { x0 = 0; x1 = 2; }

			if (yFlip) { y0 = 1; y1 = 0; }
			else { y0 = 0; y1 = 1; }

			DRAW_SPRITE(Code + x0 + y0, sx, sy)
			DRAW_SPRITE(Code + x0 + y1, sx, sy + 16)
			DRAW_SPRITE(Code + x1 + y0, sx + 16, sy)
			DRAW_SPRITE(Code + x1 + y1, sx + 16, sy + 16)
		} else {
			if (DrvFlipScreen)
				DRAW_SPRITE(Code, sx + 16, sy + 16)
			else
				DRAW_SPRITE(Code, sx, sy)
		}
	}
}

static void DrvDraw()
{
	/*bitmap.fill(m_palette->black_pen(), cliprect);
	if (m_bg_status & 1)
		draw_background(screen, bitmap, cliprect);
	if (!(m_title_screen & 1))
		draw_sprites(bitmap, cliprect);
	m_fg_tilemap->draw(screen, bitmap, cliprect, 0, 0);*/
	
	BurnTransferClear();
//	if (DrvBgStatus & 0x01) DrvRenderBg();

	DrvRenderBgLayer();
	DrvRenderSprites();
	DrvRenderCharLayer();
	BurnTransferCopy(DrvPalette);
}

static INT32 DrvFrame()
{
	INT32 nInterleave = 256;

	if (DrvReset) DrvDoReset();

	DrvMakeInputs();

	nCyclesTotal[0] = 6000000 / 60;
	nCyclesTotal[1] = 6000000 / 60;
	nCyclesDone[0] = nCyclesDone[1] = 0;
	
	ZetNewFrame();

	for (INT32 i = 0; i < nInterleave; i++) {
		INT32 nCurrentCPU, nNext;

		// Run Z80 #1
		nCurrentCPU = 0;
		ZetOpen(nCurrentCPU);
		nNext = (i + 1) * nCyclesTotal[nCurrentCPU] / nInterleave;
		nCyclesSegment = nNext - nCyclesDone[nCurrentCPU];
		nCyclesDone[nCurrentCPU] += ZetRun(nCyclesSegment);
		if (i == 0) {
			ZetSetVector(0xd7);
			ZetSetIRQLine(0, ZET_IRQSTATUS_AUTO);
		}
		if (i == 240) {
			ZetSetVector(0xcf);
			ZetSetIRQLine(0, ZET_IRQSTATUS_AUTO);
		}
		ZetClose();

		// Run Z80 #2
		nCurrentCPU = 1;
		ZetOpen(nCurrentCPU);
		BurnTimerUpdate(i * (nCyclesTotal[1] / nInterleave));
		ZetClose();
	}
	
	ZetOpen(1);
	BurnTimerEndFrame(nCyclesTotal[1]);
	ZetClose();
	
	// Make sure the buffer is entirely filled.
	if (pBurnSoundOut) {
		ZetOpen(1);
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
		ZetClose();
	}
	
	if (pBurnDraw) DrvDraw();

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {			// Return minimum compatible version
		*pnMin = 0x029731;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = RamStart;
		ba.nLen	  = RamEnd-RamStart;
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
		SCAN_VAR(DrvTitlePage);
		SCAN_VAR(DrvBgScrollX);
		SCAN_VAR(DrvBgScrollY);
		SCAN_VAR(DrvBgStatus);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		ZetMapArea(0x8000, 0xbfff, 0, DrvZ80Rom1 + 0x10000 + DrvRomBank * 0x4000 );
		ZetMapArea(0x8000, 0xbfff, 2, DrvZ80Rom1 + 0x10000 + DrvRomBank * 0x4000 );
		ZetClose();
	}

	return 0;
}

struct BurnDriverD BurnDrvPsychic5 = {
	"psychic5", NULL, NULL, NULL, "1987",
	"Psychic 5 (set 1)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, DrvRomInfo, DrvRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, NULL, DrvScan,
	NULL, 0x300, 224, 256, 3, 4
};
