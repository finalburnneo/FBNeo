// FB Alpha Prehistoric Isle in 1930 driver module
// Based on MAME driver by Bryan McPhail

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "burn_ym3812.h"
#include "upd7759.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *RamStart;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvTileMapROM;
static UINT8 *DrvSndROM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvVidRAM0;
static UINT8 *DrvSprRAM;
static UINT8 *DrvVidRAM1;
static UINT8 *DrvPalRAM;
static UINT8 *DrvZ80RAM;
static UINT32 *DrvPalette;
static UINT8 *DrvTextROM;
static UINT8 *DrvSprROM;
static UINT8 *DrvFgROM;
static UINT8 *DrvBgROM;

static INT32 ControlsInvert;
static UINT16 ScrollData[4];
static INT32 FlipScreen;
static INT32 SoundLatch;
static INT32 vblank;

static INT32 nExtraCycles;

static UINT8 DrvInputPort0[8];
static UINT8 DrvInputPort1[8];
static UINT8 DrvInputPort2[8];
static UINT8 DrvDip[2];
static UINT8 DrvInput[3];
static UINT8 DrvReset;

static struct BurnInputInfo PrehisleInputList[] = {
	{"P1 Coin"           , BIT_DIGITAL  , DrvInputPort2 + 0, "p1 coin"   },
	{"P1 Start"          , BIT_DIGITAL  , DrvInputPort0 + 7, "p1 start"  },
	{"P1 Up"             , BIT_DIGITAL  , DrvInputPort0 + 0, "p1 up"     },
	{"P1 Down"           , BIT_DIGITAL  , DrvInputPort0 + 1, "p1 down"   },
	{"P1 Left"           , BIT_DIGITAL  , DrvInputPort0 + 2, "p1 left"   },
	{"P1 Right"          , BIT_DIGITAL  , DrvInputPort0 + 3, "p1 right"  },
	{"P1 Fire 1"         , BIT_DIGITAL  , DrvInputPort0 + 4, "p1 fire 1" },
	{"P1 Fire 2"         , BIT_DIGITAL  , DrvInputPort0 + 5, "p1 fire 2" },
	{"P1 Fire 3"         , BIT_DIGITAL  , DrvInputPort0 + 6, "p1 fire 3" },

	{"P2 Coin"           , BIT_DIGITAL  , DrvInputPort2 + 1, "p2 coin"   },
	{"P2 Start"          , BIT_DIGITAL  , DrvInputPort1 + 7, "p2 start"  },
	{"P2 Up"             , BIT_DIGITAL  , DrvInputPort1 + 0, "p2 up"     },
	{"P2 Down"           , BIT_DIGITAL  , DrvInputPort1 + 1, "p2 down"   },
	{"P2 Left"           , BIT_DIGITAL  , DrvInputPort1 + 2, "p2 left"   },
	{"P2 Right"          , BIT_DIGITAL  , DrvInputPort1 + 3, "p2 right"  },
	{"P2 Fire 1"         , BIT_DIGITAL  , DrvInputPort1 + 4, "p2 fire 1" },
	{"P2 Fire 2"         , BIT_DIGITAL  , DrvInputPort1 + 5, "p2 fire 2" },
	{"P2 Fire 3"         , BIT_DIGITAL  , DrvInputPort1 + 6, "p2 fire 3" },

	{"Reset"             , BIT_DIGITAL  , &DrvReset        , "reset"     },
	{"Service"           , BIT_DIGITAL  , DrvInputPort2 + 2, "service"   },
	{"Diagnostics"       , BIT_DIGITAL  , DrvInputPort2 + 3, "diag"      },
	{"Tilt"              , BIT_DIGITAL  , DrvInputPort2 + 4, "tilt"      },
	{"Dip 1"             , BIT_DIPSWITCH, DrvDip + 0       , "dip"       },
	{"Dip 2"             , BIT_DIPSWITCH, DrvDip + 1       , "dip"       },
};

STDINPUTINFO(Prehisle)

static struct BurnDIPInfo PrehisleDIPList[]=
{
	{0x16, 0xff, 0xff, 0xff, NULL                     },
	{0x17, 0xff, 0xff, 0x7f, NULL                     },

	// Dip 1
	{0   , 0xfe, 0   , 2   , "Display"                },
	{0x16, 0x01, 0x01, 0x01, "Normal"                 },
	{0x16, 0x01, 0x01, 0x00, "Inverse"                },

	{0   , 0xfe, 0   , 2   , "Level Select"           },
	{0x16, 0x01, 0x02, 0x02, "Off"                    },
	{0x16, 0x01, 0x02, 0x00, "On"                     },

	{0   , 0xfe, 0   , 2   , "Bonus"                  },
	{0x16, 0x01, 0x04, 0x04, "2nd"                    },
	{0x16, 0x01, 0x04, 0x00, "Every"                  },

	{0   , 0xfe, 0   , 4   , "Coinage"                },
	{0x16, 0x01, 0x30, 0x30, "A 1-1 B 1-1"            },
	{0x16, 0x01, 0x30, 0x20, "A 2-1 B 1-2"            },
	{0x16, 0x01, 0x30, 0x10, "A 3-1 B 1-3"            },
	{0x16, 0x01, 0x30, 0x00, "A 4-1 B 1-4"            },

	{0   , 0xfe, 0   , 4   , "Hero"                   },
	{0x16, 0x01, 0xc0, 0x80, "2"                      },
	{0x16, 0x01, 0xc0, 0xc0, "3"                      },
	{0x16, 0x01, 0xc0, 0x40, "4"                      },
	{0x16, 0x01, 0xc0, 0x00, "5"                      },

	{0   , 0xfe, 0   , 4   , "Level"                  },
	{0x17, 0x01, 0x03, 0x02, "1 (Easy)"               },
	{0x17, 0x01, 0x03, 0x03, "2 (Standard)"           },
	{0x17, 0x01, 0x03, 0x01, "3 (Middle)"             },
	{0x17, 0x01, 0x03, 0x00, "4 (Difficult)"          },

	{0   , 0xfe, 0   , 4   , "Game Mode"              },
	{0x17, 0x01, 0x0c, 0x08, "Demo Sound Off"         },
	{0x17, 0x01, 0x0c, 0x0c, "Demo Sound On"          },
	{0x17, 0x01, 0x0c, 0x00, "Stop Video"             },
	{0x17, 0x01, 0x0c, 0x04, "Never Finish"           },

	{0   , 0xfe, 0   , 4   , "Bonus 1st/2nd"          },
	{0x17, 0x01, 0x30, 0x30, "100000/200000"          },
	{0x17, 0x01, 0x30, 0x20, "150000/300000"          },
	{0x17, 0x01, 0x30, 0x10, "300000/500000"          },
	{0x17, 0x01, 0x30, 0x00, "No Bonus"               },

	{0   , 0xfe, 0   , 2   , "Continue"               },
	{0x17, 0x01, 0x40, 0x00, "Off"                    },
	{0x17, 0x01, 0x40, 0x40, "On"                     },
};

STDDIPINFO(Prehisle)

static UINT8 __fastcall PrehisleReadByte(UINT32 address)
{
	bprintf(0, _T("rb %x\n"), address);
	return 0;
}

static void __fastcall PrehisleWriteByte(UINT32 address, UINT8 data)
{
	bprintf(0, _T("wb %x  %x\n"), address, data);
}

static UINT16 __fastcall PrehisleReadWord(UINT32 address)
{
	switch (address)
	{
		case 0x0e0010:
			return DrvInput[1];

		case 0x0e0020:
			return DrvInput[2];

		case 0x0e0040:
			return DrvInput[0] ^ ControlsInvert;

		case 0x0e0042:
			return DrvDip[0];

		case 0x0e0044:
			return (DrvDip[1] & ~0x80) | ((vblank) ? 0x00 : 0x80);
	}

	return 0;
}

static void __fastcall PrehisleWriteWord(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x0f0000:
			ScrollData[0] = data;
		return;

		case 0x0f0010:
			ScrollData[1] = data;
		return;

		case 0x0f0020:
			ScrollData[2] = data;
		return;

		case 0x0f0030:
			ScrollData[3] = data;
		return;

		case 0x0f0046:
			ControlsInvert = (data) ? 0xff : 0x00;
		return;

		case 0x0f0050:
		case 0x0f0052:
			// coin counters
		return;

		case 0x0f0060:
			FlipScreen = data & 0x01;
		return;

		case 0x0f0070:
			SoundLatch = data & 0xff;
			ZetNmi();
		return;
	}
}

static UINT8 __fastcall PrehisleZ80PortRead(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
			return BurnYM3812Read(0, 0);
	}

	return 0;
}

static void __fastcall PrehisleZ80PortWrite(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			BurnYM3812Write(0, 0, data);
		return;

		case 0x20:
			BurnYM3812Write(0, 1, data);
		return;

		case 0x40:
			UPD7759PortWrite(0,data);
			UPD7759StartWrite(0,0);
			UPD7759StartWrite(0,1);
		return;

		case 0x80:
			UPD7759ResetWrite(0,data);
		return;
	}
}

static UINT8 __fastcall PrehisleZ80Read(UINT16 address)
{
	switch (address)
	{
		case 0xf800:
			return SoundLatch;
	}

	return 0;
}

static tilemap_callback( bg )
{
	INT32 attr = (DrvTileMapROM[offs * 2] * 256) + DrvTileMapROM[offs * 2 + 1];

	TILE_SET_INFO(0, attr, attr >> 12, TILE_FLIPYX((attr >> 11) & 1)); // flipx
}

static tilemap_callback( fg )
{
	INT32 attr = (DrvVidRAM1[offs * 2 + 1] * 256) + DrvVidRAM1[offs * 2 + 0];

	TILE_SET_INFO(1, attr, attr >> 12, TILE_FLIPYX((attr >> 10) & 2)); // flipy
}

static tilemap_callback( tx )
{
	INT32 attr = (DrvVidRAM0[offs * 2 + 1] * 256) + DrvVidRAM0[offs * 2 + 0];

	TILE_SET_INFO(2, attr, attr >> 12, 0);
}

static void DrvFMIRQHandler(INT32, INT32 nStatus)
{
	ZetSetIRQLine(0, nStatus ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 DrvDoReset()
{
	memset (RamStart, 0, RamEnd - RamStart);

	SekOpen(0);
	SekReset();
	SekClose();

	ZetOpen(0);
	ZetReset();
	BurnYM3812Reset();
	UPD7759Reset();
	ZetClose();

	memset (ScrollData, 0, sizeof(ScrollData));
	ControlsInvert = 0;
	SoundLatch = 0;
	FlipScreen = 0;
	vblank = 1;

	nExtraCycles = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM			= Next; Next += 0x40000;
	DrvZ80ROM			= Next; Next += 0x10000;

	DrvTileMapROM		= Next; Next += 0x10000;

	DrvTextROM			= Next; Next += (1024 * 8 * 8);
	DrvSprROM			= Next; Next += (5120 * 16 * 16);
	DrvFgROM			= Next; Next += (2048 * 16 * 16);
	DrvBgROM			= Next; Next += (2048 * 16 * 16);

	DrvSndROM			= Next; Next += 0x20000;

	DrvPalette			= (UINT32*)Next; Next += 0x00800 * sizeof(UINT32);

	RamStart			= Next;

	Drv68KRAM			= Next; Next += 0x04000;
	DrvVidRAM0			= Next; Next += 0x00800;
	DrvSprRAM			= Next; Next += 0x00800;
	DrvVidRAM1			= Next; Next += 0x04000;
	DrvPalRAM			= Next; Next += 0x00800;
	DrvZ80RAM			= Next; Next += 0x00800;

	RamEnd = Next;

	MemEnd = Next;

	return 0;
}

static void DrvGfxDecode()
{
	INT32 Planes[4] = { STEP4(0,1) };
	INT32 XOffs[16] = { STEP8(0,4), STEP8(512,4) };
	INT32 YOffs[16] = { STEP16(0,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0xa0000);

	memcpy (tmp, DrvTextROM, 0x08000);

	GfxDecode(1024, 4,  8,  8, Planes, XOffs, YOffs, 0x100, tmp, DrvTextROM);

	memcpy (tmp, DrvBgROM, 0x40000);

	GfxDecode(2048, 4, 16, 16, Planes, XOffs, YOffs, 0x400, tmp, DrvBgROM);

	memcpy (tmp, DrvFgROM, 0x40000);

	GfxDecode(2048, 4, 16, 16, Planes, XOffs, YOffs, 0x400, tmp, DrvFgROM);

	memcpy (tmp, DrvSprROM, 0xa0000);

	GfxDecode(5120, 4, 16, 16, Planes, XOffs, YOffs, 0x400, tmp, DrvSprROM);

	BurnFree(tmp);
}

static INT32 PrehisleInit()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(Drv68KROM  + 0x00001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x00000,  1, 2)) return 1;

		if (BurnLoadRom(DrvTextROM + 0x00000,  2, 1)) return 1;

		if (BurnLoadRom(DrvBgROM   + 0x00000,  3, 1)) return 1;

		if (BurnLoadRom(DrvFgROM   + 0x00000,  4, 1)) return 1;

		if (BurnLoadRom(DrvSprROM  + 0x00000,  5, 1)) return 1;
		if (BurnLoadRom(DrvSprROM  + 0x80000,  6, 1)) return 1;

		if (BurnLoadRom(DrvTileMapROM + 0x00,  7, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x00000,  8, 1)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x00000,  9, 1)) return 1;
	}
	if (strstr(BurnDrvGetTextA(DRV_NAME), "prehislea")) 
	{
		if (BurnLoadRom(Drv68KROM  + 0x00001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x00000,  1, 2)) return 1;

		if (BurnLoadRom(DrvTextROM + 0x00000,  2, 1)) return 1;

		if (BurnLoadRom(DrvBgROM   + 0x00000,  3, 1)) return 1;
		if (BurnLoadRom(DrvBgROM   + 0x20000,  4, 1)) return 1;

		if (BurnLoadRom(DrvFgROM   + 0x00000,  5, 1)) return 1;

		if (BurnLoadRom(DrvSprROM  + 0x00000,  6, 1)) return 1;
		if (BurnLoadRom(DrvSprROM  + 0x20000,  7, 1)) return 1;
		if (BurnLoadRom(DrvSprROM  + 0x40000,  8, 1)) return 1;
		if (BurnLoadRom(DrvSprROM  + 0x60000,  9, 1)) return 1;
		if (BurnLoadRom(DrvSprROM  + 0x80000, 10, 1)) return 1;

		if (BurnLoadRom(DrvTileMapROM + 0x00, 11, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x00000, 12, 1)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x00000, 13, 1)) return 1;
	}
	if (strstr(BurnDrvGetTextA(DRV_NAME), "prehisleb"))  // bootleg
	{
		if (BurnLoadRom(Drv68KROM  + 0x00001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x00000,  1, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x20001,  2, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x20000,  3, 2)) return 1;

		if (BurnLoadRom(DrvTextROM + 0x00000,  4, 1)) return 1;

		if (BurnLoadRom(DrvBgROM   + 0x00000,  5, 1)) return 1;
		if (BurnLoadRom(DrvBgROM   + 0x10000,  6, 1)) return 1;
		if (BurnLoadRom(DrvBgROM   + 0x20000,  7, 1)) return 1;
		if (BurnLoadRom(DrvBgROM   + 0x30000,  8, 1)) return 1;

		if (BurnLoadRom(DrvFgROM   + 0x00000,  9, 1)) return 1;
		if (BurnLoadRom(DrvFgROM   + 0x10000, 10, 1)) return 1;
		if (BurnLoadRom(DrvFgROM   + 0x20000, 11, 1)) return 1;
		if (BurnLoadRom(DrvFgROM   + 0x30000, 12, 1)) return 1;

		if (BurnLoadRom(DrvSprROM  + 0x00000, 13, 1)) return 1;
		if (BurnLoadRom(DrvSprROM  + 0x10000, 14, 1)) return 1;
		if (BurnLoadRom(DrvSprROM  + 0x20000, 15, 1)) return 1;
		if (BurnLoadRom(DrvSprROM  + 0x30000, 16, 1)) return 1;
		if (BurnLoadRom(DrvSprROM  + 0x40000, 17, 1)) return 1;
		if (BurnLoadRom(DrvSprROM  + 0x50000, 18, 1)) return 1;
		if (BurnLoadRom(DrvSprROM  + 0x60000, 19, 1)) return 1;
		if (BurnLoadRom(DrvSprROM  + 0x70000, 20, 1)) return 1;
		if (BurnLoadRom(DrvSprROM  + 0x80000, 21, 1)) return 1;
		if (BurnLoadRom(DrvSprROM  + 0x90000, 22, 1)) return 1;

		if (BurnLoadRom(DrvTileMapROM + 0x00, 23, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x00000, 24, 1)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x00000, 25, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x10000, 26, 1)) return 1;
	}

	DrvGfxDecode();

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM, 	0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM, 	0x070000, 0x073fff, MAP_RAM);
	SekMapMemory(DrvVidRAM0, 	0x090000, 0x0907ff, MAP_RAM);
	SekMapMemory(DrvSprRAM, 	0x0a0000, 0x0a07ff, MAP_RAM);
	SekMapMemory(DrvVidRAM1, 	0x0b0000, 0x0b3fff, MAP_RAM);
	SekMapMemory(DrvPalRAM, 	0x0d0000, 0x0d07ff, MAP_RAM);
	SekSetReadWordHandler(0, 	PrehisleReadWord);
	SekSetWriteWordHandler(0, 	PrehisleWriteWord);
	SekSetReadByteHandler(0, 	PrehisleReadByte);
	SekSetWriteByteHandler(0, 	PrehisleWriteByte);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0xefff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,		0xf000, 0xf7ff, MAP_RAM);
	ZetSetReadHandler(PrehisleZ80Read);
	ZetSetInHandler(PrehisleZ80PortRead);
	ZetSetOutHandler(PrehisleZ80PortWrite);
	ZetClose();

	BurnYM3812Init(1, 4000000, &DrvFMIRQHandler, 0);
	BurnTimerAttach(&ZetConfig, 4000000);
	BurnYM3812SetRoute(0, BURN_SND_YM3812_ROUTE, 1.00, BURN_SND_ROUTE_BOTH);

	UPD7759Init(0, UPD7759_STANDARD_CLOCK, DrvSndROM);
	UPD7759SetRoute(0, 0.90, BURN_SND_ROUTE_BOTH);
	UPD7759SetSyncCallback(0, ZetTotalCycles, 4000000);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_COLS, bg_map_callback, 16, 16, 1024, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_COLS, fg_map_callback, 16, 16,  256, 32);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, tx_map_callback,  8,  8,   32, 32);
	GenericTilemapSetGfx(0, DrvBgROM,   4, 16, 16, 0x80000, 0x300, 0xf);
	GenericTilemapSetGfx(1, DrvFgROM,   4, 16, 16, 0x80000, 0x200, 0xf);
	GenericTilemapSetGfx(2, DrvTextROM, 4,  8,  8, 0x10000, 0x000, 0xf);
	GenericTilemapSetTransparent(1, 0xf);
	GenericTilemapSetTransparent(2, 0xf);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	BurnYM3812Exit();
	UPD7759Exit();

	SekExit();
	ZetExit();

	GenericTilesExit();

	BurnFreeMemIndex();

	return 0;
}

static void PrehisleRenderSpriteLayer()
{
	for (INT32 offs = 0x800 - 8; offs >= 0; offs -= 8)
	{
		INT32 sy = ((DrvSprRAM[offs + 1] << 8) + DrvSprRAM[offs + 0]) & 0x1ff;
		INT32 sx = ((DrvSprRAM[offs + 3] << 8) + DrvSprRAM[offs + 2]) & 0x1ff;
		if (sx & 0x100) sx = -(0xff - (sx & 0xff));
		if (sy & 0x100) sy = -(0xff - (sy & 0xff));

		INT32 Sprite	= (DrvSprRAM[offs + 5] << 8) + DrvSprRAM[offs + 4];
		INT32 Colour	= DrvSprRAM[offs + 7] >> 4;
		INT32 Priority	= (Colour < 0x4) ? 0 : 0xaaaa;
		INT32 Flipy		= Sprite & 0x8000;
		INT32 Flipx		= Sprite & 0x4000;

		Sprite &= 0x1fff;
		if (Sprite >= 0x13ff) Sprite = 0x13ff;

		RenderPrioSprite(pTransDraw, DrvSprROM, Sprite, (Colour * 0x10) + 0x100, 0xf, sx, sy - 16, Flipx, Flipy, 16, 16, Priority);
	}
}

static void DrvRecalcPalette()
{
	UINT16 *p = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x800; i++)
	{
		INT32 r =  BURN_ENDIAN_SWAP_INT16(p[i]) >> 12;
		INT32 g = (BURN_ENDIAN_SWAP_INT16(p[i]) >> 8) & 0x0f;
		INT32 b = (BURN_ENDIAN_SWAP_INT16(p[i]) >> 4) & 0x0f;

		r = (r * 16) + r;
		g = (g * 16) + g;
		b = (b * 16) + b;

		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}
}

static INT32 DrvDraw()
{
	DrvRecalcPalette();

	GenericTilemapSetFlip(TMAP_GLOBAL, FlipScreen ? (TMAP_FLIPY | TMAP_FLIPX) : 0);

	GenericTilemapSetScrollX(0, ScrollData[3]);
	GenericTilemapSetScrollY(0, ScrollData[2]);
	GenericTilemapSetScrollX(1, ScrollData[1]);
	GenericTilemapSetScrollY(1, ScrollData[0]);

	if (~nBurnLayer & 1) BurnTransferClear();

	if ( nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);
	if ( nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 1);

	if (nSpriteEnable & 1) PrehisleRenderSpriteLayer();

	if ( nBurnLayer & 4) GenericTilemapDraw(2, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static inline void DrvClearOpposites(UINT8* nJoystickInputs)
{
	if ((*nJoystickInputs & 0x03) == 0x00) {
		*nJoystickInputs |= 0x03;
	}
	if ((*nJoystickInputs & 0x0c) == 0x00) {
		*nJoystickInputs |= 0x0c;
	}
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	SekNewFrame();
	ZetNewFrame();

	{
		DrvInput[0] = DrvInput[1] = DrvInput[2] = 0xff;

		for (INT32 i = 0; i < 8; i++) {
			DrvInput[0] ^= (DrvInputPort0[i] & 1) << i;
			DrvInput[1] ^= (DrvInputPort1[i] & 1) << i;
			DrvInput[2] ^= (DrvInputPort2[i] & 1) << i;
		}

		DrvClearOpposites(&DrvInput[0]);
		DrvClearOpposites(&DrvInput[1]);
	}

	INT32 nInterleave = 264;
	INT32 nCyclesTotal[2] = { 9000000 / 60, 4000000 / 60 };
	INT32 nCyclesDone[2] = { nExtraCycles, 0 };

	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++) {
		CPU_RUN(0, Sek);

		if (i == (nInterleave - 1)) {
			SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
			vblank = 1;
		}

		if (i == 40) vblank = 0;

		CPU_RUN_TIMER(1);
	}

	ZetClose();
	SekClose();

	nExtraCycles = nCyclesDone[0] - nCyclesTotal[0];

	if (pBurnSoundOut) {
		BurnYM3812Update(pBurnSoundOut, nBurnSoundLen);
		UPD7759Render(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction,INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin != NULL) {
		*pnMin = 0x029521;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = RamStart;
		ba.nLen	  = RamEnd-RamStart;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}
	
	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);
		ZetScan(nAction);

		BurnYM3812Scan(nAction, pnMin);
		UPD7759Scan(nAction, pnMin);

		SCAN_VAR(ControlsInvert);
		SCAN_VAR(ScrollData);
		SCAN_VAR(SoundLatch);
		SCAN_VAR(FlipScreen);
		SCAN_VAR(nExtraCycles);
	}

	return 0;
}


// Prehistoric Isle in 1930 (World, set 1)

static struct BurnRomInfo PrehisleRomDesc[] = {
	{ "gt-e2.2h",      0x20000, 0x7083245a, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "gt-e3.3h",      0x20000, 0x6d8cdf58, BRF_ESS | BRF_PRG }, //  1

	{ "gt15.b15",      0x08000, 0xac652412, BRF_GRA },			 //  2	Text Layer Tiles

	{ "pi8914.b14",    0x40000, 0x207d6187, BRF_GRA },			 //  3	Background2 Layer Tiles

	{ "pi8916.h16",    0x40000, 0x7cffe0f6, BRF_GRA },			 //  4	Background1 Layer Tiles

	{ "pi8910.k14",    0x80000, 0x5a101b0b, BRF_GRA },			 //  5	Sprite Layer Tiles
	{ "gt5.5",         0x20000, 0x3d3ab273, BRF_GRA },			 //  6

	{ "gt11.11",       0x10000, 0xb4f0fcf0, BRF_GRA },			 //  7	Background 2 TileMap

	{ "gt1.1",         0x10000, 0x80a4c093, BRF_SND },			 //  8	Z80 Program Code

	{ "gt4.4",         0x20000, 0x85dfb9ec, BRF_SND },			 //  9	ADPCM Samples
};


STD_ROM_PICK(Prehisle)
STD_ROM_FN(Prehisle)

struct BurnDriver BurnDrvPrehisle = {
	"prehisle", NULL, NULL, NULL, "1989",
	"Prehistoric Isle in 1930 (World, set 1)\0", NULL, "SNK", "Prehistoric Isle (SNK)",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, PrehisleRomInfo, PrehisleRomName, NULL, NULL, NULL, NULL, PrehisleInputInfo, PrehisleDIPInfo,
	PrehisleInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x800,
	256, 224, 4, 3
};


// Prehistoric Isle in 1930 (World, set 2)

static struct BurnRomInfo PrehisleaRomDesc[] = {
	{ "gt-e2.2h",      0x20000, 0x7083245a, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "gt-e3.3h",      0x20000, 0x6d8cdf58, BRF_ESS | BRF_PRG }, //  1

	{ "gt15.b15",      0x08000, 0xac652412, BRF_GRA },			 //  2	Text Layer Tiles

	{ "12.bin",    	   0x20000, 0xc30ff459, BRF_GRA },			 //  3	Background2 Layer Tiles
	{ "13.bin",    	   0x20000, 0xc7da6980, BRF_GRA },			 //  4	

	{ "pi8916.h16",    0x40000, 0x7cffe0f6, BRF_GRA },			 //  5	Background1 Layer Tiles

	{ "9.bin",    	   0x20000, 0x6927a073, BRF_GRA },			 //  6	Sprite Layer Tiles
	{ "8.bin",    	   0x20000, 0x5f3e0148, BRF_GRA },			 //  7
	{ "7.bin",    	   0x20000, 0x4486939d, BRF_GRA },			 //  8
	{ "6.bin",    	   0x20000, 0x099beac7, BRF_GRA },			 //  9
	{ "gt5.5",         0x20000, 0x3d3ab273, BRF_GRA },			 // 10

	{ "gt11.11",       0x10000, 0xb4f0fcf0, BRF_GRA },			 // 12	Background 2 TileMap

	{ "gt1.1",         0x10000, 0x80a4c093, BRF_SND },			 // 13	Z80 Program Code

	{ "gt4.4",         0x20000, 0x85dfb9ec, BRF_SND },			 // 14	ADPCM Samples
};


STD_ROM_PICK(Prehislea)
STD_ROM_FN(Prehislea)

struct BurnDriver BurnDrvPrehislea = {
	"prehislea", "prehisle", NULL, NULL, "1989",
	"Prehistoric Isle in 1930 (World, set 2)\0", NULL, "SNK", "Prehistoric Isle (SNK)",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, PrehisleaRomInfo, PrehisleaRomName, NULL, NULL, NULL, NULL, PrehisleInputInfo, PrehisleDIPInfo,
	PrehisleInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x800,
	256, 224, 4, 3
};


// Prehistoric Isle in 1930 (US)

static struct BurnRomInfo PrehisluRomDesc[] = {
	{ "gt-u2.2h",      0x20000, 0xa14f49bb, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "gt-u3.3h",      0x20000, 0xf165757e, BRF_ESS | BRF_PRG }, //  1

	{ "gt15.b15",      0x08000, 0xac652412, BRF_GRA },			 //  2	Text Layer Tiles

	{ "pi8914.b14",    0x40000, 0x207d6187, BRF_GRA },			 //  3	Background2 Layer Tiles

	{ "pi8916.h16",    0x40000, 0x7cffe0f6, BRF_GRA },			 //  4	Background1 Layer Tiles

	{ "pi8910.k14",    0x80000, 0x5a101b0b, BRF_GRA },			 //  5	Sprite Layer Tiles
	{ "gt5.5",         0x20000, 0x3d3ab273, BRF_GRA },			 //  6

	{ "gt11.11",       0x10000, 0xb4f0fcf0, BRF_GRA },			 //  7	Background 2 TileMap

	{ "gt1.1",         0x10000, 0x80a4c093, BRF_SND },			 //  8	Z80 Program Code

	{ "gt4.4",         0x20000, 0x85dfb9ec, BRF_SND },			 //  9	ADPCM Samples
};


STD_ROM_PICK(Prehislu)
STD_ROM_FN(Prehislu)

struct BurnDriver BurnDrvPrehislu = {
	"prehisleu", "prehisle", NULL, NULL, "1989",
	"Prehistoric Isle in 1930 (US)\0", NULL, "SNK", "Prehistoric Isle (SNK)",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, PrehisluRomInfo, PrehisluRomName, NULL, NULL, NULL, NULL, PrehisleInputInfo, PrehisleDIPInfo,
	PrehisleInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x800,
	256, 224, 4, 3
};


// Wonsido 1930's (Korea)

static struct BurnRomInfo PrehislkRomDesc[] = {
	{ "gt-k2.2h",      0x20000, 0xf2d3544d, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "gt-k3.3h",      0x20000, 0xebf7439b, BRF_ESS | BRF_PRG }, //  1

	{ "gt15k.b15",     0x08000, 0x4cad1451, BRF_GRA },			 //  2	Text Layer Tiles

	{ "pi8914.b14",    0x40000, 0x207d6187, BRF_GRA },			 //  3	Background2 Layer Tiles

	{ "pi8916.h16",    0x40000, 0x7cffe0f6, BRF_GRA },			 //  4	Background1 Layer Tiles

	{ "pi8910.k14",    0x80000, 0x5a101b0b, BRF_GRA },			 //  5	Sprite Layer Tiles
	{ "gt5.5",         0x20000, 0x3d3ab273, BRF_GRA },			 //  6

	{ "gt11.11",       0x10000, 0xb4f0fcf0, BRF_GRA },			 //  7	Background 2 TileMap

	{ "gt1.1",         0x10000, 0x80a4c093, BRF_SND },			 //  8	Z80 Program Code

	{ "gt4.4",         0x20000, 0x85dfb9ec, BRF_SND },			 //  9	ADPCM Samples
};


STD_ROM_PICK(Prehislk)
STD_ROM_FN(Prehislk)

struct BurnDriver BurnDrvPrehislk = {
	"prehislek", "prehisle", NULL, NULL, "1989",
	"Wonsido 1930's (Korea)\0", NULL, "SNK (Victor license)", "Prehistoric Isle (SNK)",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, PrehislkRomInfo, PrehislkRomName, NULL, NULL, NULL, NULL, PrehisleInputInfo, PrehisleDIPInfo,
	PrehisleInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x800,
	256, 224, 4, 3
};


// Genshitou 1930's (Japan)

static struct BurnRomInfo GensitouRomDesc[] = {
	{ "gt-j2.2h",      0x20000, 0xa2da0b6b, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "gt-j3.3h",      0x20000, 0xc1a0ae8e, BRF_ESS | BRF_PRG }, //  1

	{ "gt15.b15",      0x08000, 0xac652412, BRF_GRA },			 //  2	Text Layer Tiles

	{ "pi8914.b14",    0x40000, 0x207d6187, BRF_GRA },			 //  3	Background2 Layer Tiles

	{ "pi8916.h16",    0x40000, 0x7cffe0f6, BRF_GRA },			 //  4	Background1 Layer Tiles

	{ "pi8910.k14",    0x80000, 0x5a101b0b, BRF_GRA },			 //  5	Sprite Layer Tiles
	{ "gt5.5",         0x20000, 0x3d3ab273, BRF_GRA },			 //  6

	{ "gt11.11",       0x10000, 0xb4f0fcf0, BRF_GRA },			 //  7	Background 2 TileMap

	{ "gt1.1",         0x10000, 0x80a4c093, BRF_SND },			 //  8	Z80 Program Code

	{ "gt4.4",         0x20000, 0x85dfb9ec, BRF_SND },			 //  9	ADPCM Samples
};


STD_ROM_PICK(Gensitou)
STD_ROM_FN(Gensitou)

struct BurnDriver BurnDrvGensitou = {
	"gensitou", "prehisle", NULL, NULL, "1989",
	"Genshitou 1930's (Japan)\0", NULL, "SNK", "Prehistoric Isle (SNK)",
	L"Genshi-Tou 1930's (Japan)\0\u539F\u59CB\u5CF6 1930's\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, GensitouRomInfo, GensitouRomName, NULL, NULL, NULL, NULL, PrehisleInputInfo, PrehisleDIPInfo,
	PrehisleInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x800,
	256, 224, 4, 3
};


// Prehistoric Isle in 1930 (World, bootleg)

static struct BurnRomInfo PrehislbRomDesc[] = {
	// world bootleg using 64k*8 UVEPROMs, program and sound unchanged, sprites and background tilemaps altered
	{ "u_h1.bin",      0x10000, 0x04c1703b, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "u_h3.bin",      0x10000, 0x62f04cd1, BRF_ESS | BRF_PRG }, //  1
	{ "u_j2.bin",      0x10000, 0x7b12501d, BRF_ESS | BRF_PRG }, //  2
	{ "u_j3.bin",      0x10000, 0x2a86f7c4, BRF_ESS | BRF_PRG }, //  3

	{ "l_a17.bin",     0x08000, 0xac652412, BRF_GRA },			 //  4	Text Layer Tiles

	{ "l_b17.bin",     0x10000, 0x65a22ffc, BRF_GRA },			 //  5	Background2 Layer Tiles
	{ "l_b16.bin",     0x10000, 0xb1e1f527, BRF_GRA },			 //  6
	{ "l_b14.bin",     0x10000, 0x28e94d40, BRF_GRA },			 //  7
	{ "l_b13.bin",     0x10000, 0x4dbb557a, BRF_GRA },			 //  8

	{ "l_h17.bin",     0x10000, 0x79c42316, BRF_GRA },			 //  9	Background1 Layer Tiles
	{ "l_h15.bin",     0x10000, 0x50e31fb0, BRF_GRA },			 // 10
	{ "l_f17.bin",     0x10000, 0x2af1739d, BRF_GRA },			 // 11
	{ "l_f15.bin",     0x10000, 0xcac11327, BRF_GRA },			 // 12
	
	{ "u_k12.bin",     0x10000, 0x4b0215f0, BRF_GRA },			 // 13	Sprite Layer Tiles
	{ "u_k13.bin",     0x10000, 0x68b8a698, BRF_GRA },			 // 14
	{ "u_j4.bin",      0x10000, 0x06ce7b57, BRF_GRA },			 // 15
	{ "u_j5.bin",      0x10000, 0x2ee8b401, BRF_GRA },			 // 16
	{ "u_j7.bin",      0x10000, 0x35656cbc, BRF_GRA },			 // 17
	{ "u_j8.bin",      0x10000, 0x1e7e9336, BRF_GRA },			 // 18
	{ "u_j10.bin",     0x10000, 0x785bf046, BRF_GRA },			 // 19
	{ "u_j11.bin",     0x10000, 0xc306b9fa, BRF_GRA },			 // 20
	{ "u_j12.bin",     0x10000, 0x5ba5bbed, BRF_GRA },			 // 21
	{ "u_j13.bin",     0x10000, 0x007dee47, BRF_GRA },			 // 22 - modified by bootleggers
	
	{ "l_a6.bin",      0x10000, 0xe2b9a44b, BRF_GRA },			 // 23	Background 2 TileMap - modified by bootleggers

	{ "u_e12.bin",     0x10000, 0x80a4c093, BRF_SND },			 // 24	Z80 Program Code

	{ "u_f14.bin",     0x10000, 0x2fb32933, BRF_SND },			 // 25	ADPCM Samples
	{ "u_j14.bin",     0x10000, 0x32d5f7c9, BRF_SND },			 // 26
};


STD_ROM_PICK(Prehislb)
STD_ROM_FN(Prehislb)

struct BurnDriver BurnDrvPrehislb = {
	"prehisleb", "prehisle", NULL, NULL, "1989",
	"Prehistoric Isle in 1930 (World, bootleg)\0", NULL, "bootleg", "Prehistoric Isle (SNK)",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, PrehislbRomInfo, PrehislbRomName, NULL, NULL, NULL, NULL, PrehisleInputInfo, PrehisleDIPInfo,
	PrehisleInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x800,
	256, 224, 4, 3
};
