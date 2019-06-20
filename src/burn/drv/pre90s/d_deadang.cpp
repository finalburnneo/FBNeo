// FB Alpha Dead Angle driver module
// Based on MAME driver by Bryan McPhail and David Haywood

#include "tiles_generic.h"
#include "nec_intf.h"
#include "z80_intf.h"
#include "seibusnd.h"
#include "burn_ym2203.h"
#include "watchdog.h"
#include "bitswap.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvMainROM;
static UINT8 *DrvSubROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvGfxROM4;
static UINT8 *DrvBgROM;
static UINT8 *DrvFgROM;
static UINT8 *DrvSndROM0;
static UINT8 *DrvSndROM1;
static UINT8 *DrvMainRAM;
static UINT8 *DrvSubRAM;
static UINT8 *DrvShareRAM;
static UINT8 *DrvTxtRAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvScrollRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprRAMBuf;
static UINT8 *DrvPalRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 tilebank;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[2];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static struct BurnInputInfo DeadangInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Deadang)

static struct BurnDIPInfo DeadangDIPList[]=
{
	{0x11, 0xff, 0xff, 0xff, NULL					},
	{0x12, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    8, "Coin A"				},
	{0x11, 0x01, 0x07, 0x00, "5 Coins 1 Credits"	},
	{0x11, 0x01, 0x07, 0x04, "4 Coins 1 Credits"	},
	{0x11, 0x01, 0x07, 0x02, "3 Coins 1 Credits"	},
	{0x11, 0x01, 0x07, 0x06, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x07, 0x03, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x11, 0x01, 0x07, 0x01, "1 Coin  5 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x11, 0x01, 0x18, 0x10, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x18, 0x18, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x18, 0x00, "2 Coins 3 Credits"	},
	{0x11, 0x01, 0x18, 0x08, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x11, 0x01, 0x20, 0x20, "Off"					},
	{0x11, 0x01, 0x20, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x11, 0x01, 0x40, 0x40, "Off"					},
	{0x11, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x11, 0x01, 0x80, 0x80, "Upright"				},
	{0x11, 0x01, 0x80, 0x00, "Cocktail"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x12, 0x01, 0x03, 0x02, "Easy"					},
	{0x12, 0x01, 0x03, 0x03, "Normal"				},
	{0x12, 0x01, 0x03, 0x01, "Hard"					},
	{0x12, 0x01, 0x03, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x12, 0x01, 0x0c, 0x08, "20K 50K"				},
	{0x12, 0x01, 0x0c, 0x0c, "30K 100K"				},
	{0x12, 0x01, 0x0c, 0x04, "50K 150K"				},
	{0x12, 0x01, 0x0c, 0x00, "100K 200K"			},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x12, 0x01, 0x30, 0x20, "1"					},
	{0x12, 0x01, 0x30, 0x10, "2"					},
	{0x12, 0x01, 0x30, 0x30, "3"					},
	{0x12, 0x01, 0x30, 0x00, "4"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x12, 0x01, 0x40, 0x00, "Off"					},
	{0x12, 0x01, 0x40, 0x40, "On"					},

	{0   , 0xfe, 0   ,    2, "User Mode"			},
	{0x12, 0x01, 0x80, 0x00, "Japan"				},
	{0x12, 0x01, 0x80, 0x80, "Overseas"				},
};

STDDIPINFO(Deadang)

static struct BurnDIPInfo GhunterDIPList[]=
{
	{0x11, 0xff, 0xff, 0xff, NULL					},
	{0x12, 0xff, 0xff, 0xf7, NULL					},

	{0   , 0xfe, 0   ,    8, "Coin A"				},
	{0x11, 0x01, 0x07, 0x00, "5 Coins 1 Credits"	},
	{0x11, 0x01, 0x07, 0x04, "4 Coins 1 Credits"	},
	{0x11, 0x01, 0x07, 0x02, "3 Coins 1 Credits"	},
	{0x11, 0x01, 0x07, 0x06, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x07, 0x03, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x11, 0x01, 0x07, 0x01, "1 Coin  5 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x11, 0x01, 0x18, 0x10, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x18, 0x18, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x18, 0x00, "2 Coins 3 Credits"	},
	{0x11, 0x01, 0x18, 0x08, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x11, 0x01, 0x20, 0x20, "Off"					},
	{0x11, 0x01, 0x20, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x11, 0x01, 0x40, 0x40, "Off"					},
	{0x11, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x11, 0x01, 0x80, 0x80, "Upright"				},
	{0x11, 0x01, 0x80, 0x00, "Cocktail"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x12, 0x01, 0x03, 0x02, "Easy"					},
	{0x12, 0x01, 0x03, 0x03, "Normal"				},
	{0x12, 0x01, 0x03, 0x01, "Hard"					},
	{0x12, 0x01, 0x03, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    2, "Bonus Life"			},
	{0x12, 0x01, 0x04, 0x04, "50K 150K"				},
	{0x12, 0x01, 0x04, 0x00, "100K 200K"			},

	{0   , 0xfe, 0   ,    1, "Controller"			},
//	{0x12, 0x01, 0x08, 0x08, "Trackball"			}, // trackball doesn't work
	{0x12, 0x01, 0x08, 0x00, "Joystick"				},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x12, 0x01, 0x30, 0x20, "1"					},
	{0x12, 0x01, 0x30, 0x10, "2"					},
	{0x12, 0x01, 0x30, 0x30, "3"					},
	{0x12, 0x01, 0x30, 0x00, "4"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x12, 0x01, 0x40, 0x00, "Off"					},
	{0x12, 0x01, 0x40, 0x40, "On"					},

	{0   , 0xfe, 0   ,    2, "User Mode"			},
	{0x12, 0x01, 0x80, 0x00, "Japan"				},
	{0x12, 0x01, 0x80, 0x80, "Overseas"				},
};

STDDIPINFO(Ghunter)

static void __fastcall deadang_main_write(UINT32 address, UINT8 data)
{
	if (address >= 0x05000 && address <= 0x05fff) return; // nops
	if (address >= 0x06010 && address <= 0x07fff) return;
	if (address >= 0x08800 && address <= 0x0bfff) return;
	if (address >= 0x0d000 && address <= 0x0dfff) return;
	if (address >= 0x0e000 && address <= 0x0ffff) return;

	if (address >= 0x06000 && address <= 0x0600f) {
		if ((address & 1) == 0) seibu_main_word_write(address, data);
		return;
	}
}

static UINT8 __fastcall deadang_main_read(UINT32 address)
{
	if (address >= 0x06000 && address <= 0x0600f) {
		if ((address & 1) == 0) return seibu_main_word_read(address);
		return 0;
	}

	switch (address)
	{
		case 0x0a000:
			return DrvInputs[0];

		case 0x0a001:
			return DrvInputs[1];

		case 0x0a002:
			return DrvDips[0];

		case 0x0a003:
			return DrvDips[1];

		case 0x80000:
		case 0x80001:
			return 0x00; // trackball low

		case 0xb0000:
		case 0xb0001:
			return 0x00; // trackball high
	}

	return 0;
}

static void __fastcall deadang_sub_write(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x08000:
			tilebank = data & 1;
		return;

		case 0x0c000:
		case 0x0c001:
			BurnWatchdogWrite();
		return;
	}
}

static tilemap_scan( bg )
{
	return (col & 0xf) | ((row & 0xf) << 4) | ((col & 0x70) << 4) | ((row & 0xf0) << 7);
}

static tilemap_callback( bg )
{
	UINT16 attr = DrvBgROM[offs * 2 + 1] + (DrvBgROM[offs * 2 + 0] * 256);

	TILE_SET_INFO(3, attr, attr >> 12, 0);
}

static tilemap_callback( mg )
{
	UINT16 attr = DrvFgRAM[offs * 2 + 0] + (DrvFgRAM[offs * 2 + 1] * 256);

	TILE_SET_INFO(1, (attr & 0xfff) + (tilebank * 0x1000), attr >> 12, 0);
}

static tilemap_callback( fg )
{
	UINT16 attr = DrvFgROM[offs * 2 + 1] + (DrvFgROM[offs * 2 + 0] * 256);

	TILE_SET_INFO(2, attr, attr >> 12, 0);
}

static tilemap_callback( tx )
{
	UINT16 attr = DrvTxtRAM[offs * 2 + 0] + (DrvTxtRAM[offs * 2 + 1] * 256);

	TILE_SET_INFO(0, (attr & 0xff) + ((attr & 0xc000) >> 6), attr >> 8, 0);
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	VezOpen(0);
	VezReset();
	VezClose();

	VezOpen(1);
	VezReset();
	VezClose();

	seibu_sound_reset();

	BurnWatchdogReset();

	tilebank = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvMainROM		= Next; Next += 0x040000;
	DrvSubROM		= Next; Next += 0x020000;

	SeibuZ80ROM		= Next; Next += 0x020000;
	SeibuZ80DecROM	= Next; Next += 0x020000;

	DrvGfxROM0		= Next; Next += 0x010000;
	DrvGfxROM1		= Next; Next += 0x100000;
	DrvGfxROM2		= Next; Next += 0x200000;
	DrvGfxROM3		= Next; Next += 0x080000;
	DrvGfxROM4		= Next; Next += 0x080000;

	DrvBgROM		= Next; Next += 0x010000;
	DrvFgROM		= Next; Next += 0x010000;

	SeibuADPCMData[0] 	= Next;
	DrvSndROM0		= Next; Next += 0x010000;

	SeibuADPCMData[1] 	= Next;
	DrvSndROM1		= Next; Next += 0x010000;

	DrvPalette		= (UINT32*)Next; Next += 0x0801 * sizeof(UINT32);

	AllRam			= Next;

	DrvMainRAM		= Next; Next += 0x003800;
	DrvSubRAM		= Next; Next += 0x003800;
	DrvShareRAM		= Next; Next += 0x001000;

	DrvTxtRAM		= Next; Next += 0x000800;
	DrvFgRAM		= Next; Next += 0x000800;
	DrvScrollRAM	= Next; Next += 0x000200;

	DrvSprRAM		= Next; Next += 0x000800;
	DrvSprRAMBuf	= Next; Next += 0x000800;

	DrvPalRAM		= Next; Next += 0x001000;

	SeibuZ80RAM		= Next; Next += 0x000800;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[4]  = { 0x4000*8+0, 0x4000*8+4, 0, 4 };
	INT32 Plane1[4]  = { 8, 12 ,0, 4 };
	INT32 XOffs0[8]  = { STEP4(3,-1), STEP4(8+3,-1) };
	INT32 XOffs1[16] = { STEP4(3,-1), STEP4(8*2+3,-1), STEP4(512+3,-1), STEP4(512+8*2+3,-1) };
	INT32 YOffs0[8]  = { STEP8(0,16) };
	INT32 YOffs1[16] = { STEP16(0,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x200000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x008000);

	GfxDecode(0x0400, 4,  8,  8, Plane0, XOffs0, YOffs0, 0x080, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x080000);

	GfxDecode(0x1000, 4, 16, 16, Plane1, XOffs1, YOffs1, 0x400, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x100000);

	GfxDecode(0x2000, 4, 16, 16, Plane1, XOffs1, YOffs1, 0x400, tmp, DrvGfxROM2);

	memcpy (tmp, DrvGfxROM3, 0x040000);

	GfxDecode(0x0800, 4, 16, 16, Plane1, XOffs1, YOffs1, 0x400, tmp, DrvGfxROM3);

	memcpy (tmp, DrvGfxROM4, 0x040000);

	GfxDecode(0x0800, 4, 16, 16, Plane1, XOffs1, YOffs1, 0x400, tmp, DrvGfxROM4);

	BurnFree (tmp);

	return 0;
}

static void DrvADPCMDescramble(UINT8 *src)
{
	for (INT32 i = 0; i < 0x10000; i++)
	{
		src[i] = BITSWAP08(src[i], 7,5,3,1,6,4,2,0);
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
		if (BurnLoadRom(DrvMainROM + 0x000000,  0, 2)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x000001,  1, 2)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x020000,  2, 2)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x020001,  3, 2)) return 1;

		if (BurnLoadRom(DrvSubROM  + 0x000000,  4, 2)) return 1;
		if (BurnLoadRom(DrvSubROM  + 0x000001,  5, 2)) return 1;

		if (BurnLoadRom(SeibuZ80ROM  + 0x000000,  6, 1)) return 1;
		if (BurnLoadRom(SeibuZ80ROM  + 0x010000,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x004000,  9, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000, 10, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x000000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x080000, 12, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x000000, 13, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM4 + 0x000000, 14, 1)) return 1;

		if (BurnLoadRom(DrvBgROM   + 0x000000, 15, 2)) return 1;
		if (BurnLoadRom(DrvBgROM   + 0x000001, 16, 2)) return 1;

		if (BurnLoadRom(DrvFgROM   + 0x000000, 17, 2)) return 1;
		if (BurnLoadRom(DrvFgROM   + 0x000001, 18, 2)) return 1;

		if (BurnLoadRom(DrvSndROM0 + 0x000000, 19, 1)) return 1;

		if (BurnLoadRom(DrvSndROM1 + 0x000000, 20, 1)) return 1;

		DrvGfxDecode();
		DrvADPCMDescramble(DrvSndROM0);
		DrvADPCMDescramble(DrvSndROM1);
	}

	VezInit(0, V30_TYPE);
	VezOpen(0);
	VezMapMemory(DrvMainRAM,			0x00000, 0x037ff, MAP_RAM);
	VezMapMemory(DrvSprRAM,				0x03800, 0x03fff, MAP_RAM);
	VezMapMemory(DrvShareRAM,			0x04000, 0x04fff, MAP_RAM);
	VezMapMemory(DrvTxtRAM,				0x08000, 0x087ff, MAP_RAM);
	VezMapMemory(DrvPalRAM,				0x0c000, 0x0cfff, MAP_RAM);
	VezMapMemory(DrvScrollRAM,			0x0e000, 0x0e1ff, MAP_RAM); //0-ff, page size is 200
	VezMapMemory(DrvMainROM,			0xc0000, 0xfffff, MAP_ROM);
	VezSetWriteHandler(deadang_main_write);
	VezSetReadHandler(deadang_main_read);
	VezClose();

	VezInit(1, V30_TYPE);
	VezOpen(1);
	VezMapMemory(DrvSubRAM,				0x00000, 0x037ff, MAP_RAM);
	VezMapMemory(DrvFgRAM,				0x03800, 0x03fff, MAP_RAM);
	VezMapMemory(DrvShareRAM,			0x04000, 0x04fff, MAP_RAM);
	VezMapMemory(DrvSubROM,				0xe0000, 0xfffff, MAP_ROM);
	VezSetWriteHandler(deadang_sub_write);
	VezClose();

	SeibuADPCMDataLen[0] = 0x10000;
	SeibuADPCMDataLen[1] = 0x10000;

	seibu_sound_init(2|8, 0x2000, 14318181/4, 14318181/4, 8000);
	BurnYM2203SetAllRoutes(0, 0.45, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetPSGVolume(0, 0.25);
	BurnYM2203SetAllRoutes(1, 0.45, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetPSGVolume(1, 0.25);

	BurnWatchdogInit(DrvDoReset, 180);

	GenericTilesInit();
	GenericTilemapInit(3, bg_map_scan, bg_map_callback,   16, 16, 128, 256);
	GenericTilemapInit(2, bg_map_scan, fg_map_callback,   16, 16, 128, 256);
	GenericTilemapInit(1, TILEMAP_SCAN_COLS, mg_map_callback, 16, 16,  32,  32);
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, tx_map_callback,  8,  8,  32,  32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 4,  8,  8, 0x010000, 0x200,  0xf);
	GenericTilemapSetGfx(1, DrvGfxROM2, 4, 16, 16, 0x200000, 0x400,  0xf);
	GenericTilemapSetGfx(2, DrvGfxROM3, 4, 16, 16, 0x080000, 0x100,  0xf);
	GenericTilemapSetGfx(3, DrvGfxROM4, 4, 16, 16, 0x080000, 0x000,  0xf);
	GenericTilemapSetTransparent(1, 0xf);
	GenericTilemapSetTransparent(2, 0xf);
	GenericTilemapSetTransparent(0, 0xf);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	VezExit();

	seibu_sound_exit();

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteUpdate()
{
	for (INT32 i = 0; i < 0x1000; i+=2)
	{
		UINT8 r = DrvPalRAM[i + 0] & 0xf;
		UINT8 g = DrvPalRAM[i + 0] >> 4;
		UINT8 b = DrvPalRAM[i + 1] & 0xf;

		DrvPalette[i/2] = BurnHighCol(r+r*16, g+g*16, b+b*16, 0);
	}

	DrvPalette[0x1000/2] = 0; // black
}

static void draw_sprites(INT32 /*flipscreen*/)
{
	UINT16 *ram = (UINT16*)DrvSprRAMBuf;

	for (INT32 offs = 0; offs < 0x800/2; offs+=4)
	{
		if ((ram[offs+3] & 0xff00) != 0xf00) continue;

		INT32 pri = 0;
		switch (ram[offs+2] & 0xc000)
		{
			case 0xc000: pri = 0; break;	// Unknown
			case 0x8000: pri = 0; break;	// Over all playfields
			case 0x4000: pri = 0xf0; break;	// Under top playfield
			case 0x0000: pri = 0xfc; break;	// Under middle playfield
		}

		INT32 flipx = ram[offs+0] & 0x2000;
		INT32 flipy =~ram[offs+0] & 0x4000;
		INT32 sy    = ram[offs+0] & 0x00ff;
		INT32 sx    = ram[offs+2] & 0x00ff;
		if (ram[offs + 2] & 0x100) sx = 0 - (0xff - sx);

		INT32 color = ram[offs+1] >> 12;
		INT32 code  = ram[offs+1] & 0xfff;

		if (0) { //flipscreen) {
			sx = 240 - sx;
			sy = 240 - sy;
			flipx ^= 0x2000;
			flipy ^= 0x4000;
		}

		RenderPrioSprite(pTransDraw, DrvGfxROM1, code, color*16+0x300, 0xf, sx, sy - 16, flipx, flipy, 16, 16, pri);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	}

	UINT16 *scroll_ram = (UINT16*)DrvScrollRAM;

	INT32 layer_enable = scroll_ram[0x34] ^ 0xff;
	INT32 flipscreen = (layer_enable & 0x40) ? 0 : TMAP_FLIPXY;

	GenericTilemapSetScrollY(3, ((scroll_ram[0x01]&0xf0)<<4)+((scroll_ram[0x02]&0x7f)<<1)+((scroll_ram[0x02]&0x80)>>7) );
	GenericTilemapSetScrollX(3, ((scroll_ram[0x09]&0xf0)<<4)+((scroll_ram[0x0a]&0x7f)<<1)+((scroll_ram[0x0a]&0x80)>>7) );
	GenericTilemapSetScrollY(1, ((scroll_ram[0x11]&0x10)<<4)+((scroll_ram[0x12]&0x7f)<<1)+((scroll_ram[0x12]&0x80)>>7) );
	GenericTilemapSetScrollX(1, ((scroll_ram[0x19]&0x10)<<4)+((scroll_ram[0x1a]&0x7f)<<1)+((scroll_ram[0x1a]&0x80)>>7) );
	GenericTilemapSetScrollY(2, ((scroll_ram[0x21]&0xf0)<<4)+((scroll_ram[0x22]&0x7f)<<1)+((scroll_ram[0x22]&0x80)>>7) );
	GenericTilemapSetScrollX(2, ((scroll_ram[0x29]&0xf0)<<4)+((scroll_ram[0x2a]&0x7f)<<1)+((scroll_ram[0x2a]&0x80)>>7) );

	GenericTilemapSetEnable(3, layer_enable & 0x01);
	GenericTilemapSetEnable(1, layer_enable & 0x02);
	GenericTilemapSetEnable(2, layer_enable & 0x04);
	GenericTilemapSetFlip(TMAP_GLOBAL, flipscreen);

	BurnTransferClear(0x800); // black

	if (nBurnLayer & 1) GenericTilemapDraw(3, pTransDraw, 1, 0xff);
	if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 2, 0xff);
	if (nBurnLayer & 4) GenericTilemapDraw(2, pTransDraw, 4, 0xff);

	if ((layer_enable & 0x10) && (nSpriteEnable & 1))
		draw_sprites(flipscreen);

	if (nBurnLayer & 8) GenericTilemapDraw(0, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	VezNewFrame();
	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 2);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}

		seibu_coin_input = (DrvJoy3[0] & 1) | ((DrvJoy3[1] & 1) << 1);
	}

	// any changes to this timing will pretty much break the game.
	// most bugs appear in the 3rd level, incl. guy walking off the second
	// floor into the air & foreground objects not blocking shots.  - dink

	INT32 nInterleave = 10;
	INT32 nCyclesTotal[3] = { 8000000 / 60, 8000000 / 60, 3579545 / 60 };
	INT32 nCyclesDone[3] = { 0, 0, 0 };

	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		VezOpen(0);
		CPU_RUN(0, Vez);
		if (i == 0) {
			VezSetIRQLineAndVector(0,  0xc8/4, CPU_IRQSTATUS_ACK);
			VezRun(1);
			VezSetIRQLineAndVector(0,  0xc8/4, CPU_IRQSTATUS_NONE);
		}
		if (i == 2) {
			VezSetIRQLineAndVector(0,  0xc4/4, CPU_IRQSTATUS_ACK);
			VezRun(1);
			VezSetIRQLineAndVector(0,  0xc4/4, CPU_IRQSTATUS_NONE);
		}
		VezClose();

		VezOpen(1);
		CPU_RUN(1, Vez);
		if (i == 0) {
			if (pBurnDraw) {
				DrvDraw();
			}
			memcpy(DrvSprRAMBuf, DrvSprRAM, 0x800);
			VezSetIRQLineAndVector(0,  0xc8/4, CPU_IRQSTATUS_ACK);
			VezRun(1);
			VezSetIRQLineAndVector(0,  0xc8/4, CPU_IRQSTATUS_NONE);
		}
		if (i == 2) {
			VezSetIRQLineAndVector(0,  0xc4/4, CPU_IRQSTATUS_ACK);
			VezRun(1);
			VezSetIRQLineAndVector(0,  0xc4/4, CPU_IRQSTATUS_NONE);
		}
		VezClose();

		BurnTimerUpdate(nCyclesTotal[2] * (i + 1) / nInterleave);
	}

	BurnTimerEndFrame(nCyclesTotal[2]);

	if (pBurnSoundOut) {
		seibu_sound_update(pBurnSoundOut, nBurnSoundLen);
		seibu_sound_update_cabal(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();


	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin != NULL) {
		*pnMin = 0x029719;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA)
	{
		VezScan(nAction);
		ZetScan(nAction);
		seibu_sound_scan(nAction, pnMin);

		BurnWatchdogScan(nAction);

		SCAN_VAR(tilebank);
	}

	return 0;
}


// Dead Angle

static struct BurnRomInfo deadangRomDesc[] = {
	{ "2.18h",		0x10000, 0x1bc05b7e, 0x01 | BRF_PRG | BRF_ESS }, //  0 V30 #0 Code
	{ "4.22h",		0x10000, 0x5751d4e7, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "1.18f",		0x10000, 0x8e7b15cc, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "3.21f",		0x10000, 0xe784b1fa, 0x01 | BRF_PRG | BRF_ESS }, //  3

	{ "5.6b",		0x10000, 0x9c69eb35, 0x02 | BRF_PRG | BRF_ESS }, //  4 V30 #1 Code
	{ "6.9b",		0x10000, 0x34a44ce5, 0x02 | BRF_PRG | BRF_ESS }, //  5

	{ "13.b1",		0x02000, 0x13b956fb, 0x03 | BRF_PRG | BRF_ESS }, //  6 Z80 Code (encrypted)
	{ "14.c1",		0x10000, 0x98837d57, 0x03 | BRF_PRG | BRF_ESS }, //  7

	{ "7.21j",		0x04000, 0xfe615fcd, 0x04 | BRF_GRA },           //  8 Characters
	{ "8.21l",		0x04000, 0x905d6b27, 0x04 | BRF_GRA },           //  9

	{ "l12",		0x80000, 0xc94d5cd2, 0x05 | BRF_GRA },           // 10 Sprites

	{ "16n",		0x80000, 0xfd70e1a5, 0x06 | BRF_GRA },           // 11 Midground Tiles
	{ "16r",		0x80000, 0x92f5e382, 0x06 | BRF_GRA },           // 12

	{ "11m",		0x40000, 0xa366659a, 0x07 | BRF_GRA },           // 13 Background Tiles

	{ "11k",		0x40000, 0x9cf5bcc7, 0x08 | BRF_GRA },           // 14 Foreground Tiles

	{ "10.6l",		0x08000, 0xca99176b, 0x09 | BRF_GRA },           // 15 Background Map Data
	{ "9.6m",		0x08000, 0x51d868ca, 0x09 | BRF_GRA },           // 16

	{ "12.6j",		0x08000, 0x2674d23f, 0x0A | BRF_GRA },           // 17 Foreground Map Data
	{ "11.6k",		0x08000, 0x3dd4d81d, 0x0A | BRF_GRA },           // 18

	{ "15.b11",		0x10000, 0xfabd74f2, 0x0B | BRF_SND },           // 19 Samples (Chip #1)

	{ "16.11a",		0x10000, 0xa8d46fc9, 0x0C | BRF_SND },           // 20 Samples (Chip #0)
};

STD_ROM_PICK(deadang)
STD_ROM_FN(deadang)

struct BurnDriver BurnDrvDeadang = {
	"deadang", NULL, NULL, NULL, "1988",
	"Dead Angle\0", NULL, "Seibu Kaihatsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, deadangRomInfo, deadangRomName, NULL, NULL, NULL, NULL, DeadangInputInfo, DeadangDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x801,
	256, 224, 4, 3
};


// Lead Angle (Japan)

static struct BurnRomInfo leadangRomDesc[] = {
	{ "2.18h",		0x10000, 0x611247e0, 0x01 | BRF_PRG | BRF_ESS }, //  0 V30 #0 Code
	{ "4.22h",		0x10000, 0x348c1201, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "1.18f",		0x10000, 0xfb952d71, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "3.22f",		0x10000, 0x2271c6df, 0x01 | BRF_PRG | BRF_ESS }, //  3

	{ "5.6b",		0x10000, 0x9c69eb35, 0x02 | BRF_PRG | BRF_ESS }, //  4 V30 #1 Code
	{ "6.9b",		0x10000, 0x34a44ce5, 0x02 | BRF_PRG | BRF_ESS }, //  5

	{ "13.b1",		0x02000, 0x13b956fb, 0x03 | BRF_PRG | BRF_ESS }, //  6 Z80 Code (encrypted)
	{ "14.c1",		0x10000, 0x98837d57, 0x03 | BRF_PRG | BRF_ESS }, //  7

	{ "7.22k",		0x04000, 0x490701e7, 0x04 | BRF_GRA },           //  8 Characters
	{ "8.22l",		0x04000, 0x18024c5e, 0x04 | BRF_GRA },           //  9

	{ "l12",		0x80000, 0xc94d5cd2, 0x05 | BRF_GRA },           // 10 Sprites

	{ "16n",		0x80000, 0xfd70e1a5, 0x06 | BRF_GRA },           // 11 Background Tiles
	{ "16r",		0x80000, 0x92f5e382, 0x06 | BRF_GRA },           // 12

	{ "11m",		0x40000, 0xa366659a, 0x07 | BRF_GRA },           // 13 Midground Tiles

	{ "11k",		0x40000, 0x9cf5bcc7, 0x08 | BRF_GRA },           // 14 Foreground Tiles

	{ "10.6l",		0x08000, 0xca99176b, 0x09 | BRF_GRA },           // 15 Background Map Data
	{ "9.6m",		0x08000, 0x51d868ca, 0x09 | BRF_GRA },           // 16

	{ "12.6j",		0x08000, 0x2674d23f, 0x0A | BRF_GRA },           // 17 Foreground Map Data
	{ "11.6k",		0x08000, 0x3dd4d81d, 0x0A | BRF_GRA },           // 18

	{ "15.b11",		0x10000, 0xfabd74f2, 0x0B | BRF_SND },           // 19 Samples (Chip #0)

	{ "16.11a",		0x10000, 0xa8d46fc9, 0x0C | BRF_SND },           // 20 Samples (Chip #1)
};

STD_ROM_PICK(leadang)
STD_ROM_FN(leadang)

struct BurnDriver BurnDrvLeadang = {
	"leadang", "deadang", NULL, NULL, "1988",
	"Lead Angle (Japan)\0", NULL, "Seibu Kaihatsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, leadangRomInfo, leadangRomName, NULL, NULL, NULL, NULL, DeadangInputInfo, DeadangDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x801,
	256, 224, 4, 3
};


// Gang Hunter / Dead Angle

static struct BurnRomInfo ghunterRomDesc[] = {
	{ "2.19h",		0x10000, 0x5a511500, 0x01 | BRF_PRG | BRF_ESS }, //  0 V30 #0 Code
	{ "4.22h",		0x10000, 0xdf5704f4, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "1.19f",		0x10000, 0x30deb018, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "3.22f",		0x10000, 0x95f587c5, 0x01 | BRF_PRG | BRF_ESS }, //  3

	{ "5.6b",		0x10000, 0xc40bb5e5, 0x02 | BRF_PRG | BRF_ESS }, //  4 V30 #1 Code
	{ "6.10b",		0x10000, 0x373f86a7, 0x02 | BRF_PRG | BRF_ESS }, //  5

	{ "13.b1",		0x02000, 0x13b956fb, 0x03 | BRF_PRG | BRF_ESS }, //  6 Z80 Code (encrypted)
	{ "14.c1",		0x10000, 0x98837d57, 0x03 | BRF_PRG | BRF_ESS }, //  7

	{ "7.22k",		0x04000, 0x490701e7, 0x04 | BRF_GRA },           //  8 Characters
	{ "8.22l",		0x04000, 0x18024c5e, 0x04 | BRF_GRA },           //  9

	{ "l12",		0x80000, 0xc94d5cd2, 0x05 | BRF_GRA },           // 10 Sprites

	{ "16n",		0x80000, 0xfd70e1a5, 0x06 | BRF_GRA },           // 11 Background Tiles
	{ "16r",		0x80000, 0x92f5e382, 0x06 | BRF_GRA },           // 12

	{ "11m",		0x40000, 0xa366659a, 0x07 | BRF_GRA },           // 13 Midground Tiles

	{ "11k",		0x40000, 0x9cf5bcc7, 0x08 | BRF_GRA },           // 14 Foreground Tiles

	{ "10.6l",		0x08000, 0xca99176b, 0x09 | BRF_GRA },           // 15 Background Map Data
	{ "9.6m",		0x08000, 0x51d868ca, 0x09 | BRF_GRA },           // 16

	{ "12.6j",		0x08000, 0x2674d23f, 0x0A | BRF_GRA },           // 17 Foreground Map Data
	{ "11.6k",		0x08000, 0x3dd4d81d, 0x0A | BRF_GRA },           // 18

	{ "15.b11",		0x10000, 0xfabd74f2, 0x0B | BRF_SND },           // 19 Samples (Chip #0)

	{ "16.11a",		0x10000, 0xa8d46fc9, 0x0C | BRF_SND },           // 20 Samples (Chip #1)
};

STD_ROM_PICK(ghunter)
STD_ROM_FN(ghunter)

struct BurnDriver BurnDrvGhunter = {
	"ghunter", "deadang", NULL, NULL, "1988",
	"Gang Hunter / Dead Angle\0", NULL, "Seibu Kaihatsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, ghunterRomInfo, ghunterRomName, NULL, NULL, NULL, NULL, DeadangInputInfo, GhunterDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x801,
	256, 224, 4, 3
};


// Gang Hunter / Dead Angle (Spain)

static struct BurnRomInfo ghuntersRomDesc[] = {
	{ "ggh-2.h18",	0x10000, 0x7ccc6fee, 0x01 | BRF_PRG | BRF_ESS }, //  0 V30 #0 Code
	{ "ggh-4.h22",	0x10000, 0xd1f23ad7, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "ggh-1.f18",	0x10000, 0x0d6ff111, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "ggh-3.f22",	0x10000, 0x66dec38d, 0x01 | BRF_PRG | BRF_ESS }, //  3

	{ "ggh-5.b6",	0x10000, 0x1f612f3b, 0x02 | BRF_PRG | BRF_ESS }, //  4 V30 #1 Code
	{ "ggh-6.b10",	0x10000, 0x63e18e56, 0x02 | BRF_PRG | BRF_ESS }, //  5

	{ "13.b1",		0x02000, 0x13b956fb, 0x03 | BRF_PRG | BRF_ESS }, //  6 Z80 Code (encrypted)
	{ "14.c1",		0x10000, 0x98837d57, 0x03 | BRF_PRG | BRF_ESS }, //  7

	{ "7.21j",		0x04000, 0xfe615fcd, 0x04 | BRF_GRA },           //  8 Characters
	{ "8.21l",		0x04000, 0x905d6b27, 0x04 | BRF_GRA },           //  9

	{ "l12",		0x80000, 0xc94d5cd2, 0x05 | BRF_GRA },           // 10 Sprites

	{ "16n",		0x80000, 0xfd70e1a5, 0x06 | BRF_GRA },           // 11 Background Tiles
	{ "16r",		0x80000, 0x92f5e382, 0x06 | BRF_GRA },           // 12

	{ "11m",		0x40000, 0xa366659a, 0x07 | BRF_GRA },           // 13 Midground Tiles

	{ "11k",		0x40000, 0x9cf5bcc7, 0x08 | BRF_GRA },           // 14 Foreground Tiles

	{ "10.6l",		0x08000, 0xca99176b, 0x09 | BRF_GRA },           // 15 Background Map Data
	{ "9.6m",		0x08000, 0x51d868ca, 0x09 | BRF_GRA },           // 16

	{ "12.6j",		0x08000, 0x2674d23f, 0x0A | BRF_GRA },           // 17 Foreground Map Data
	{ "11.6k",		0x08000, 0x3dd4d81d, 0x0A | BRF_GRA },           // 18

	{ "15.b11",		0x10000, 0xfabd74f2, 0x0B | BRF_SND },           // 19 Samples (Chip #0)

	{ "16.11a",		0x10000, 0xa8d46fc9, 0x0C | BRF_SND },           // 20 Samples (Chip #1)
};

STD_ROM_PICK(ghunters)
STD_ROM_FN(ghunters)

struct BurnDriver BurnDrvGhunters = {
	"ghunters", "deadang", NULL, NULL, "1988",
	"Gang Hunter / Dead Angle (Spain)\0", NULL, "Seibu Kaihatsu (Segasa/Sonic license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, ghuntersRomInfo, ghuntersRomName, NULL, NULL, NULL, NULL, DeadangInputInfo, GhunterDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x801,
	256, 224, 4, 3
};
