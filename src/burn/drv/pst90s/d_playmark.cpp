#include "tiles_generic.h"
#include "m68000_intf.h"
#include "pic16c5x_intf.h"
#include "msm6295.h"

static UINT8 DrvInputPort0[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvInputPort1[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvInputPort2[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvDip[2]        = {0, 0};
static UINT8 DrvInput[3]      = {0x00, 0x00, 0x00};
static UINT8 DrvReset         = 0;

static UINT8 *Mem             = NULL;
static UINT8 *MemEnd          = NULL;
static UINT8 *RamStart        = NULL;
static UINT8 *RamEnd          = NULL;
static UINT8 *Drv68kRom       = NULL;
static UINT8 *Drv68kRam       = NULL;
static UINT8 *DrvPicRom       = NULL;
static UINT8 *DrvSpriteRam    = NULL;
static UINT8 *DrvVideo1Ram    = NULL;
static UINT8 *DrvVideo2Ram    = NULL;
static UINT8 *DrvBgVideoRam   = NULL;
static UINT8 *DrvPaletteRam   = NULL;
static UINT32 *DrvPalette     = NULL;
static UINT8 *DrvSprites      = NULL;
static UINT8 *DrvTiles        = NULL;
static UINT8 *DrvChars        = NULL;
static UINT8 *DrvTempGfx      = NULL;

static UINT16 DrvFgScrollX = 0;
static UINT16 DrvFgScrollY = 0;
static UINT16 DrvCharScrollX = 0;
static UINT16 DrvCharScrollY = 0;
static UINT16 DrvBgEnable = 0;
static UINT16 DrvBgFullSize = 0;
static UINT16 DrvBgScrollX = 0;
static UINT16 DrvBgScrollY = 0;

static UINT8 DrvSoundCommand = 0;
static UINT8 DrvSoundFlag = 0;
static UINT8 DrvOkiControl = 0;
static UINT8 DrvOkiCommand = 0;
static UINT8 DrvOldOkiBank = 0;
static UINT8 DrvOkiBank = 0;

static INT32 nCyclesDone[2], nCyclesTotal[2];
static INT32 nCyclesSegment;

static struct BurnInputInfo BigtwinInputList[] = {
	{"Coin 1"            , BIT_DIGITAL  , DrvInputPort0 + 4, "p1 coin"   },
	{"Start 1"           , BIT_DIGITAL  , DrvInputPort1 + 7, "p1 start"  },
	{"Coin 2"            , BIT_DIGITAL  , DrvInputPort0 + 5, "p2 coin"   },
	{"Start 2"           , BIT_DIGITAL  , DrvInputPort2 + 7, "p2 start"  },

	{"P1 Up"             , BIT_DIGITAL  , DrvInputPort1 + 0, "p1 up"     },
	{"P1 Down"           , BIT_DIGITAL  , DrvInputPort1 + 1, "p1 down"   },
	{"P1 Left"           , BIT_DIGITAL  , DrvInputPort1 + 2, "p1 left"   },
	{"P1 Right"          , BIT_DIGITAL  , DrvInputPort1 + 3, "p1 right"  },
	{"P1 Fire 1"         , BIT_DIGITAL  , DrvInputPort1 + 4, "p1 fire 1" },

	{"P2 Up"             , BIT_DIGITAL  , DrvInputPort2 + 0, "p2 up"     },
	{"P2 Down"           , BIT_DIGITAL  , DrvInputPort2 + 1, "p2 down"   },
	{"P2 Left"           , BIT_DIGITAL  , DrvInputPort2 + 2, "p2 left"   },
	{"P2 Right"          , BIT_DIGITAL  , DrvInputPort2 + 3, "p2 right"  },
	{"P2 Fire 1"         , BIT_DIGITAL  , DrvInputPort2 + 4, "p2 fire 1" },

	{"Reset"             , BIT_DIGITAL  , &DrvReset        , "reset"     },
	{"Dip 1"             , BIT_DIPSWITCH, DrvDip + 0       , "dip"       },
	{"Dip 2"             , BIT_DIPSWITCH, DrvDip + 1       , "dip"       },
};

STDINPUTINFO(Bigtwin)

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
	DrvInput[0] = DrvInput[1] = DrvInput[2] = 0x00;

	for (INT32 i = 0; i < 8; i++) {
		DrvInput[0] |= (DrvInputPort0[i] & 1) << i;
		DrvInput[1] |= (DrvInputPort1[i] & 1) << i;
		DrvInput[2] |= (DrvInputPort2[i] & 1) << i;
	}

	DrvClearOpposites(&DrvInput[1]);
	DrvClearOpposites(&DrvInput[2]);
}

static struct BurnDIPInfo BigtwinDIPList[]=
{
	// Default Values
	{0x0f, 0xff, 0xff, 0x4a, NULL                     },
	{0x10, 0xff, 0xff, 0xff, NULL                     },
	
	// Dip 1
	{0   , 0xfe, 0   , 2   , "Language"               },
	{0x0f, 0x01, 0x01, 0x01, "English"                },
	{0x0f, 0x01, 0x01, 0x00, "Italian"                },
	
	{0   , 0xfe, 0   , 2   , "Censor Pictures"        },
	{0x0f, 0x01, 0x04, 0x00, "No"                     },
	{0x0f, 0x01, 0x04, 0x04, "Yes"                    },
	
	{0   , 0xfe, 0   , 3   , "Difficulty"             },
	{0x0f, 0x01, 0x30, 0x30, "Normal"                 },
	{0x0f, 0x01, 0x30, 0x10, "Hard"                   },
	{0x0f, 0x01, 0x30, 0x00, "Very Hard"              },
	
	{0   , 0xfe, 0   , 2   , "Allow Continue"         },
	{0x0f, 0x01, 0x40, 0x00, "No"                     },
	{0x0f, 0x01, 0x40, 0x40, "Yes"                    },
	
	{0   , 0xfe, 0   , 2   , "Demo Sound"             },
	{0x0f, 0x01, 0x80, 0x80, "Off"                    },
	{0x0f, 0x01, 0x80, 0x00, "On"                     },

	// Dip 2
	{0   , 0xfe, 0   , 2   , "Coin Mode"              },
	{0x10, 0x01, 0x01, 0x01, "Mode 1"                 },
	{0x10, 0x01, 0x01, 0x00, "Mode 2"                 },
	
	{0   , 0xfe, 0   , 16  , "Coinage Mode 1"         },
	{0x10, 0x01, 0x1e, 0x14, "6 Coins 1 Credit"       },
	{0x10, 0x01, 0x1e, 0x16, "5 Coins 1 Credit"       },
	{0x10, 0x01, 0x1e, 0x14, "4 Coins 1 Credit"       },
	{0x10, 0x01, 0x1e, 0x18, "3 Coins 1 Credit"       },
	{0x10, 0x01, 0x1e, 0x1a, "8 Coins 3 Credits"      },
	{0x10, 0x01, 0x1e, 0x02, "2 Coins 1 Credit"       },
	{0x10, 0x01, 0x1e, 0x04, "5 Coins 1 Credits"      },
	{0x10, 0x01, 0x1e, 0x06, "3 Coins 2 Credits"      },
	{0x10, 0x01, 0x1e, 0x1e, "1 Coin  1 Credit"       },
	{0x10, 0x01, 0x1e, 0x08, "2 Coins 3 Credits"      },
	{0x10, 0x01, 0x1e, 0x12, "1 Coin  2 Credits"      },
	{0x10, 0x01, 0x1e, 0x10, "1 Coin  3 Credits"      },
	{0x10, 0x01, 0x1e, 0x0e, "1 Coin  4 Credits"      },
	{0x10, 0x01, 0x1e, 0x0c, "1 Coin  5 Credits"      },
	{0x10, 0x01, 0x1e, 0x0a, "1 Coin  6 Credits"      },
	{0x10, 0x01, 0x1e, 0x00, "Free Play"              },
	
	{0   , 0xfe, 0   , 2   , "Minimum Credits to Start" },
	{0x10, 0x01, 0x20, 0x20, "1"                      },
	{0x10, 0x01, 0x20, 0x00, "2"                      },
};

STDDIPINFO(Bigtwin)

static struct BurnRomInfo BigtwinRomDesc[] = {
	{ "2.302",         0x80000, 0xe6767f60, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "3.301",         0x80000, 0x5aba6990, BRF_ESS | BRF_PRG }, //  1	68000 Program Code
	
	{ "16c57hs.015",   0x02d4c, 0xc07e9375, BRF_ESS | BRF_PRG }, //  2 PIC16C57 HEX

	{ "4.311",         0x40000, 0x6f628fbc, BRF_GRA },			 //  3	Tiles
	{ "5.312",         0x40000, 0x6a9b1752, BRF_GRA },			 //  4	Tiles
	{ "6.313",         0x40000, 0x411cf852, BRF_GRA },			 //  5	Tiles
	{ "7.314",         0x40000, 0x635c81fd, BRF_GRA },			 //  6	Tiles
	
	{ "8.321",         0x20000, 0x2749644d, BRF_GRA },			 //  7	Sprites
	{ "9.322",         0x20000, 0x1d1897af, BRF_GRA },			 //  8	Sprites
	{ "10.323",        0x20000, 0x2a03432e, BRF_GRA },			 //  9	Sprites
	{ "11.324",        0x20000, 0x2c980c4c, BRF_GRA },			 //  10	Sprites
	
	{ "1.013",         0x40000, 0xff6671dc, BRF_SND },			 //  11	Samples
};

STD_ROM_PICK(Bigtwin)
STD_ROM_FN(Bigtwin)

static INT32 DrvDoReset()
{
	SekOpen(0);
	SekReset();
	SekClose();
	
	pic16c5xReset();
	
	MSM6295Reset(0);
	
	DrvFgScrollX = 0;
	DrvFgScrollY = 0;
	DrvCharScrollX = 0;
	DrvCharScrollY = 0;
	DrvBgEnable = 0;
	DrvBgFullSize = 0;
	DrvBgScrollX = 0;
	DrvBgScrollY = 0;
	
	DrvSoundCommand = 0;
	DrvSoundFlag = 0;
	DrvOkiControl = 0;
	DrvOkiCommand = 0;
	DrvOldOkiBank = 0;
	DrvOkiBank = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = Mem;

	Drv68kRom            = Next; Next += 0x100000;
	MSM6295ROM           = Next; Next += 0x040000;
	DrvPicRom            = Next; Next += 0x001000;

	RamStart = Next;

	Drv68kRam            = Next; Next += 0x010000;
	DrvSpriteRam         = Next; Next += 0x004000;
	DrvVideo1Ram         = Next; Next += 0x002000;
	DrvVideo2Ram         = Next; Next += 0x001000;
	DrvBgVideoRam        = Next; Next += 0x080000;
	DrvPaletteRam        = Next; Next += 0x000800;
	
	RamEnd = Next;

	DrvSprites           = Next; Next += (0x0400 * 32 * 32);
	DrvTiles             = Next; Next += (0x2000 * 16 * 16);
	DrvChars             = Next; Next += (0x2000 * 8 * 8);
	DrvPalette           = (UINT32*)Next; Next += 0x00400 * sizeof(UINT32);

	MemEnd = Next;

	return 0;
}

UINT8 __fastcall DrvReadByte(UINT32 a)
{
	switch (a) {
		case 0x700013: {
			return 0xff - DrvInput[1];
		}
		
		case 0x700015: {
			return 0xff - DrvInput[2];
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("Read byte -> %06X\n"), a);
		}
	}
	
	return 0;
}

void __fastcall DrvWriteByte(UINT32 a, UINT8 d)
{
	switch (a) {
		case 0x70001f: {
			DrvSoundCommand = d;
			DrvSoundFlag = 1;
			// space.device().execute().yield();
			INT32 cycles = (SekTotalCycles() / 4) - nCyclesDone[1];
			nCyclesDone[1] += pic16c5xRun(cycles);
			return;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("Write byte -> %06X, %02X\n"), a, d);
		}
	}
}

UINT16 __fastcall DrvReadWord(UINT32 a)
{
	switch (a) {
		case 0x700010: {
			return 0xffff - DrvInput[0];
		}
		
		case 0x70001a: {
			return 0xff00 | DrvDip[0];
		}
		
		case 0x70001c: {
			return 0xff00 | DrvDip[1];
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("Read Word -> %06X\n"), a);
		}
	}
	
	return 0;
}

void __fastcall DrvWriteWord(UINT32 a, UINT16 d)
{
	if (a >= 0x501000 && a <= 0x501fff) {
		// unused ram???
		return;
	}
	
	if (a >= 0x504000 && a <= 0x50ffff) {
		// unused ram???
		return;
	}
	
	switch (a) {
		case 0x304000: {
			// irq ack???
			return;
		}
		
		case 0x510000: {
			DrvCharScrollX = (d + 2) & 0x1ff;
			return;
		}
		
		case 0x510002: {
			DrvCharScrollY = d & 0xff;
			return;
		}
		
		case 0x510004: {
			DrvBgScrollX = -(d + 4);
			return;
		}
		
		case 0x510006: {
			DrvBgScrollY = -d & 0x1ff;
			DrvBgEnable = d & 0x200;
			DrvBgFullSize = d & 0x400;	
			return;
		}
		
		case 0x510008: {
			DrvFgScrollX = (d + 6) & 0x1ff;
			return;
		}
		
		case 0x51000a: {
			DrvFgScrollY = d & 0x1ff;
			return;
		}
		
		case 0x51000c: {
			// nop???
			return;
		}
		
		case 0xe00000: {
			// ???
			return;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("Write word -> %06X, %04X\n"), a, d);
		}
	}
}

UINT8 PlaymarkSoundReadPort(UINT16 Port)
{
	switch (Port) {
		case 0x01: {
			UINT8 Data = 0;

			if ((DrvOkiControl & 0x38) == 0x30) {
				Data = DrvSoundCommand;
			} else {
				if ((DrvOkiControl & 0x38) == 0x28) {
					Data = MSM6295ReadStatus(0) & 0x0f;
				}
			}

			return Data;
		}
		
		case 0x02: {
			if (DrvSoundFlag) {
				DrvSoundFlag = 0;
				return 0x00;
			}
			return 0x40;
		}
		
		case 0x10: {
			return 0;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("Sound Read Port %x\n"), Port);
		}
	}

	return 0;
}

void PlaymarkSoundWritePort(UINT16 Port, UINT8 Data)
{
	switch (Port & 0xff) {
		case 0x01: {
			DrvOkiCommand = Data;
			return;
		}
		
		case 0x02: {
			DrvOkiControl = Data;
			bprintf(PRINT_NORMAL, _T("Oki Control %x\n"), DrvOkiControl);

			if ((Data & 0x38) == 0x18) {
				MSM6295Command(0, DrvOkiCommand);
				bprintf(PRINT_NORMAL, _T("Play %x\n"), DrvOkiCommand);
			}
			return;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("Sound Write Port %x, %x\n"), Port, Data);
		}
	}
}

static INT32 DrvCharPlaneOffsets[4]   = { 0x600000, 0x400000, 0x200000, 0 };
static INT32 DrvCharXOffsets[8]       = { 0, 1, 2, 3, 4, 5, 6, 7 };
static INT32 DrvCharYOffsets[8]       = { 0, 8, 16, 24, 32, 40, 48, 56 };
static INT32 DrvTilePlaneOffsets[4]   = { 0x600000, 0x400000, 0x200000, 0 };
static INT32 DrvTileXOffsets[16]      = { 0, 1, 2, 3, 4, 5, 6, 7, 128, 129, 130, 131, 132, 133, 134, 135 };
static INT32 DrvTileYOffsets[16]      = { 0, 8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 88, 96, 104, 112, 120 };
static INT32 DrvSpritePlaneOffsets[4] = { 0x300000, 0x200000, 0x100000, 0 };
static INT32 DrvSpriteXOffsets[32]    = { 0, 1, 2, 3, 4, 5, 6, 7, 128, 129, 130, 131, 132, 133, 134, 135, 256, 257, 258, 259, 260, 261, 262, 263, 384, 385, 386, 387, 388, 389, 390, 391 };
static INT32 DrvSpriteYOffsets[32]    = { 0, 8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 88, 96, 104, 112, 120, 512, 520, 528, 536, 544, 552, 560, 568, 576, 584, 592, 600, 608, 616, 624, 632 };

static INT32 DrvInit()
{
	INT32 nRet = 0, nLen;
	
	Mem = NULL;
	MemIndex();
	nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(Mem, 0, nLen);
	MemIndex();

	DrvTempGfx = (UINT8*)BurnMalloc(0x100000);
	
	nRet = BurnLoadRom(Drv68kRom  + 0x00001, 0, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68kRom  + 0x00000, 1, 2); if (nRet != 0) return 1;
	
	if (BurnLoadPicROM(DrvPicRom, 2, 0x2d4c)) return 1;
	
	nRet = BurnLoadRom(DrvTempGfx + 0x00000, 3, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempGfx + 0x40000, 4, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempGfx + 0x80000, 5, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempGfx + 0xc0000, 6, 1); if (nRet != 0) return 1;
	GfxDecode(0x2000, 4, 16, 16, DrvTilePlaneOffsets, DrvTileXOffsets, DrvTileYOffsets, 0x100, DrvTempGfx, DrvTiles);
	GfxDecode(0x2000, 4, 8, 8, DrvCharPlaneOffsets, DrvCharXOffsets, DrvCharYOffsets, 0x100, DrvTempGfx, DrvChars);

	memset(DrvTempGfx, 0, 0x100000);
	nRet = BurnLoadRom(DrvTempGfx + 0x00000, 7, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempGfx + 0x20000, 8, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempGfx + 0x40000, 9, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempGfx + 0x60000, 10, 1); if (nRet != 0) return 1;
	GfxDecode(0x400, 4, 32, 32, DrvSpritePlaneOffsets, DrvSpriteXOffsets, DrvSpriteYOffsets, 0x400, DrvTempGfx, DrvSprites);
	BurnFree(DrvTempGfx);

	nRet = BurnLoadRom(MSM6295ROM, 11, 1); if (nRet != 0) return 1;
	
	BurnSetRefreshRate(58.0);
	
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68kRom       , 0x000000, 0x0fffff, SM_ROM);
	SekMapMemory(DrvSpriteRam    , 0x440000, 0x4403ff, SM_RAM);
	SekMapMemory(DrvVideo2Ram    , 0x500000, 0x500fff, SM_RAM);
	SekMapMemory(DrvVideo1Ram    , 0x502000, 0x503fff, SM_RAM);
	SekMapMemory(DrvBgVideoRam   , 0x600000, 0x67ffff, SM_RAM);
	SekMapMemory(DrvPaletteRam   , 0x780000, 0x7807ff, SM_RAM);
	SekMapMemory(Drv68kRam       , 0xff0000, 0xffffff, SM_RAM);
	SekSetReadByteHandler(0, DrvReadByte);
	SekSetReadWordHandler(0, DrvReadWord);
	SekSetWriteByteHandler(0, DrvWriteByte);
	SekSetWriteWordHandler(0, DrvWriteWord);
	SekClose();
	
	pic16c5xInit(0x16C57, DrvPicRom);
	pPic16c5xReadPort = PlaymarkSoundReadPort;
	pPic16c5xWritePort = PlaymarkSoundWritePort;
	
	MSM6295Init(0, 1000000 / 132, 1);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);
	
	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	SekExit();
	pic16c5xExit();
	MSM6295Exit(0);
	
	GenericTilesExit();
	
	BurnFree(Mem);
	
	DrvFgScrollX = 0;
	DrvFgScrollY = 0;
	DrvCharScrollX = 0;
	DrvCharScrollY = 0;
	DrvBgEnable = 0;
	DrvBgFullSize = 0;
	DrvBgScrollX = 0;
	DrvBgScrollY = 0;
	
	DrvSoundCommand = 0;
	DrvSoundFlag = 0;
	DrvOkiControl = 0;
	DrvOkiCommand = 0;
	DrvOldOkiBank = 0;
	DrvOkiBank = 0;
	
	return 0;
}

static inline UINT8 pal5bit(UINT8 bits)
{
	bits &= 0x1f;
	return (bits << 3) | (bits >> 2);
}

static inline UINT32 CalcCol(UINT16 nColour)
{
	INT32 r, g, b;
	
	r = (nColour >> 11) & 0x1e;
	g = (nColour >>  7) & 0x1e;
	b = (nColour >>  3) & 0x1e;

	r |= ((nColour & 0x08) >> 3);
	g |= ((nColour & 0x04) >> 2);
	b |= ((nColour & 0x02) >> 1);

	return BurnHighCol(pal5bit(r), pal5bit(g), pal5bit(b), 0);
}

static void DrvCalcPalette()
{
	INT32 i;
	UINT16* ps;
	UINT32* pd;

	for (i = 0, ps = (UINT16*)DrvPaletteRam, pd = DrvPalette; i < 0x400; i++, ps++, pd++) {
		*pd = CalcCol(*ps);
	}
}

static void DrvRenderFgLayer()
{
	INT32 mx, my, Code, Colour, x, y, TileIndex = 0;
	
	UINT16 *VideoRam = (UINT16*)DrvVideo2Ram;
	
	for (my = 0; my < 32; my++) {
		for (mx = 0; mx < 32; mx++) {
			Code = BURN_ENDIAN_SWAP_INT16(VideoRam[(TileIndex * 2)+ 0]);
			Colour = BURN_ENDIAN_SWAP_INT16(VideoRam[(TileIndex * 2)+ 1]);
			
			x = 16 * mx;
			y = 16 * my;
			
			x -= DrvFgScrollX;
			y -= DrvFgScrollY;
			if (x < -16) x += 512;
			if (y < -16) y += 512;
			
			y -= 16;

			if (x > 16 && x < 304 && y > 16 && y < 224) {
				Render16x16Tile(pTransDraw, Code, x, y, Colour, 4, 0, DrvTiles);
			} else {
				Render16x16Tile_Clip(pTransDraw, Code, x, y, Colour, 4, 0, DrvTiles);
			}
			
			TileIndex++;
		}
	}
}

static void DrvRenderCharLayer()
{
	INT32 mx, my, Code, Colour, x, y, TileIndex = 0;
	
	UINT16 *VideoRam = (UINT16*)DrvVideo1Ram;
	
	for (my = 0; my < 32; my++) {
		for (mx = 0; mx < 64; mx++) {
			Code = BURN_ENDIAN_SWAP_INT16(VideoRam[(TileIndex * 2)+ 0]);
			Colour = BURN_ENDIAN_SWAP_INT16(VideoRam[(TileIndex * 2)+ 1]);
			
			x = 8 * mx;
			y = 8 * my;
			
			x -= DrvCharScrollX;
			y -= DrvCharScrollY;
			if (x < -8) x += 512;
			if (y < -8) y += 256;
			
			y -= 16;

			if (x > 8 && x < 312 && y > 8 && y < 232) {
				Render8x8Tile_Mask(pTransDraw, Code, x, y, Colour, 4, 0, 0x80, DrvChars);
			} else {
				Render8x8Tile_Mask_Clip(pTransDraw, Code, x, y, Colour, 4, 0, 0x80, DrvChars);
			}
			
			TileIndex++;
		}
	}
}

static void DrvRenderSprites(INT32 CodeShift)
{
	INT32 Offs, StartOffset = 0x4000 / 2 - 4;
	INT32 Height = 32;
	INT32 ColourDiv = 0x10 / 16;
	UINT16 *SpriteRam = (UINT16*)DrvSpriteRam;

	for (Offs = 4; Offs < 0x4000 / 2; Offs += 4) {
		if (SpriteRam[Offs + 3 - 4] == 0x2000) {
			StartOffset = Offs - 4;
			break;
		}
	}

	for (Offs = StartOffset; Offs >= 4; Offs -= 4) {
		INT32 sx, sy, Code, Colour, xFlip, Pri;

		sy = SpriteRam[Offs + 3 - 4]; 

		xFlip = sy & 0x4000;
		sx = (SpriteRam[Offs + 1] & 0x01ff) - 16 - 7;
		sy = (256 - 8 - Height - sy) & 0xff;
		Code = SpriteRam[Offs + 2] >> CodeShift;
		Colour = ((SpriteRam[Offs + 1] & 0x3e00) >> 9) / ColourDiv;
		Pri = (SpriteRam[Offs + 1] & 0x8000) >> 15;

		if(!Pri && (Colour & 0x0c) == 0x0c)	Pri = 2;
			
		sy -= 16;

		/*pdrawgfx_transpen(bitmap,cliprect,machine().gfx[0],
					code,
					color,
					flipx,0,
					sx + m_xoffset,sy + m_yoffset,
					machine().priority_bitmap,m_pri_masks[pri],0);*/
					
		// other games need priority support adding
					
		if (sx > 16 && sx < 304 && sy > 16 && sy < 224) {
			if (xFlip) {
				Render32x32Tile_Mask_FlipX(pTransDraw, Code, sx, sy, Colour, 4, 0, 0x200, DrvSprites);
			} else {
				Render32x32Tile_Mask(pTransDraw, Code, sx, sy, Colour, 4, 0, 0x200, DrvSprites);
			}
		} else {
			if (xFlip) {
				Render32x32Tile_Mask_FlipX_Clip(pTransDraw, Code, sx, sy, Colour, 4, 0, 0x200, DrvSprites);
			} else {
				Render32x32Tile_Mask_Clip(pTransDraw, Code, sx, sy, Colour, 4, 0, 0x200, DrvSprites);
			}
		}
	}
}

static void DrvRenderBitmap()
{
	INT32 Colour;
//	UINT8 *Pri;
	UINT16 *VideoRam = (UINT16*)DrvBgVideoRam;
	INT32 Count = 0;
	
	for (INT32 y = 0; y < 512; y++) {
		for (INT32 x = 0; x < 512; x++) {
			Colour = VideoRam[Count] & 0xff;

			if (Colour) {
				if (DrvBgFullSize) {
					INT32 yPlot = (y - 16 + DrvBgScrollY) & 0x1ff;
					INT32 xPlot = (x + DrvBgScrollX) & 0x1ff;
					
					if (xPlot >= 0 && xPlot < 320 && yPlot >= 0 && yPlot < 240) {
						pTransDraw[(yPlot * nScreenWidth) + xPlot] = 0x100 + Colour;
					}
					
//					pri = &machine().priority_bitmap.pix8((y + m_bgscrolly) & 0x1ff);
//					pri[(x + m_bgscrollx) & 0x1ff] |= 2;
				} else {
					// 50% size
					if(!(x % 2) && !(y % 2)) {
						// untested for now
						INT32 yPlot = ((y / 2) - 16 + DrvBgScrollY) & 0x1ff;
						INT32 xPlot = ((x / 2) + DrvBgScrollX) & 0x1ff;
					
						if (xPlot >= 0 && xPlot < 320 && yPlot >= 0 && yPlot < 240) {
							pTransDraw[(yPlot * nScreenWidth) + xPlot] = 0x100 + Colour;
						}
					
//						bitmap.pix16((y / 2 + m_bgscrolly) & 0x1ff, (x / 2 + m_bgscrollx) & 0x1ff) = 0x100 + color;

//						pri = &machine().priority_bitmap.pix8((y / 2 + m_bgscrolly) & 0x1ff);
//						pri[(x / 2 + m_bgscrollx) & 0x1ff] |= 2;
					}
				}
			}

			Count++;
		}
	}
}

static void DrvRender()
{
	BurnTransferClear();
	DrvCalcPalette();
	DrvRenderFgLayer();
	if (DrvBgEnable) DrvRenderBitmap();
	DrvRenderSprites(4);
	DrvRenderCharLayer();	
	BurnTransferCopy(DrvPalette);
}

static INT32 DrvFrame()
{
	INT32 nInterleave = 100;
	INT32 nSoundBufferPos = 0;

	if (DrvReset) DrvDoReset();

	DrvMakeInputs();

	nCyclesTotal[0] = (INT32)((double)12000000 / 58.0);
	nCyclesTotal[1] = (INT32)((double)3000000 / 58.0);
	nCyclesDone[0] = nCyclesDone[1] = 0;

	SekNewFrame();

	SekOpen(0);
	for (INT32 i = 0; i < nInterleave; i++) {
		INT32 nCurrentCPU, nNext;

		nCurrentCPU = 0;
		nNext = (i + 1) * nCyclesTotal[nCurrentCPU] / nInterleave;
		nCyclesSegment = nNext - nCyclesDone[nCurrentCPU];
		nCyclesDone[nCurrentCPU] += SekRun(nCyclesSegment);
		if (i == 90) SekSetIRQLine(2, SEK_IRQSTATUS_AUTO);
		
		nCurrentCPU = 1;
		nNext = (i + 1) * nCyclesTotal[nCurrentCPU] / nInterleave;
		nCyclesSegment = nNext - nCyclesDone[nCurrentCPU];
		nCyclesDone[nCurrentCPU] += pic16c5xRun(nCyclesSegment);
		
		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			MSM6295Render(0, pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}
	SekClose();
	
	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);

		if (nSegmentLength) {
			MSM6295Render(0, pSoundBuf, nSegmentLength);
		}
	}

	if (pBurnDraw) DrvRender();

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin != NULL) {
		*pnMin = 0x02729;
	}

	if (nAction & ACB_MEMORY_RAM) {								// Scan all memory, devices & variables
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = RamStart;
		ba.nLen	  = RamEnd - RamStart;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}
	
	return 0;
}

struct BurnDriver BurnDrvBigtwin = {
	"bigtwin", NULL, NULL, NULL, "1995",
	"Big Twin\0", "No Sound", "SemiCom", "Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, BigtwinRomInfo, BigtwinRomName, NULL, NULL, BigtwinInputInfo, BigtwinDIPInfo,
	DrvInit, DrvExit, DrvFrame, NULL, DrvScan,
	NULL, 0x400, 320, 240, 4, 3
};
