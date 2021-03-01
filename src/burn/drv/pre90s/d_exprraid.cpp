// Final Burn Neo Data East Express raider driver module
// Based on MAME driver by Ernesto Corvi

#include "tiles_generic.h"
#include "m6502_intf.h"
#include "m6809_intf.h"
#include "burn_ym2203.h"
#include "burn_ym3526.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvMainROM;
static UINT8 *DrvSoundROM;
static UINT8 *DrvGfxROM[4];
static UINT8 *DrvColPROM;
static UINT8 *DrvMainRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvColRAM;
static UINT8 *DrvSoundRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 soundlatch;
static INT32 protection_value;
static INT32 flipscreen;
static INT32 scrolly;
static INT32 previous_coin;

static UINT8 *scrollx;
static UINT8 *bgselect;

static UINT8 *vblank;
static INT32 bootleg_type = -1;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo ExprraidInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 6,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Exprraid)

static struct BurnDIPInfo ExprraidDIPList[]=
{
	{0x12, 0xff, 0xff, 0xbf, NULL					},
	{0x13, 0xff, 0xff, 0xbf, NULL					},

	{0   , 0xfe, 0   ,    8, "Coin A"				},
	{0x12, 0x01, 0x03, 0x00, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x03, 0x03, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x03, 0x01, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x03, 0x02, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x03, 0x01, "1 Coin  4 Credits"	},
	{0x12, 0x01, 0x03, 0x00, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"				},
	{0x12, 0x01, 0x0c, 0x00, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0x0c, 0x04, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x0c, 0x00, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x0c, 0x08, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x0c, 0x04, "1 Coin  3 Credits"	},

#if 0
	{0   , 0xfe, 0   ,    2, "Coin Mode"			},
	{0x12, 0x01, 0x10, 0x10, "Mode 1"				},
	{0x12, 0x01, 0x10, 0x00, "Mode 2"				},
#endif

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x12, 0x01, 0x20, 0x20, "Off"					},
	{0x12, 0x01, 0x20, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x12, 0x01, 0x40, 0x00, "Upright"				},
	{0x12, 0x01, 0x40, 0x40, "Cocktail"				},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x13, 0x01, 0x03, 0x01, "1"					},
	{0x13, 0x01, 0x03, 0x03, "3"					},
	{0x13, 0x01, 0x03, 0x02, "5"					},
	{0x13, 0x01, 0x03, 0x00, "Infinite"				},

	{0   , 0xfe, 0   ,    2, "Bonus Life"			},
	{0x13, 0x01, 0x04, 0x00, "50k 80k"				},
	{0x13, 0x01, 0x04, 0x04, "50k only"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x13, 0x01, 0x18, 0x18, "Easy"					},
	{0x13, 0x01, 0x18, 0x10, "Normal"				},
	{0x13, 0x01, 0x18, 0x08, "Hard"					},
	{0x13, 0x01, 0x18, 0x00, "Very Hard"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x13, 0x01, 0x20, 0x00, "Off"					},
	{0x13, 0x01, 0x20, 0x20, "On"					},
};

STDDIPINFO(Exprraid)

static void exprraid_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x2000:
			M6502SetIRQLine(CPU_IRQLINE_IRQ, CPU_IRQSTATUS_NONE);
		return;

		case 0x2001:
			soundlatch = data;
			M6809SetIRQLine(CPU_IRQLINE_NMI, CPU_IRQSTATUS_AUTO);
		return;

		case 0x2002:
			flipscreen = data & 1;
		return;

		case 0x2003: // nop
		return;

		case 0x2800:
		case 0x2801:
		case 0x2802:
		case 0x2803:
			bgselect[address & 3] = data & 0x3f;
		return;

		case 0x2804:
			scrolly = data;
		return;

		case 0x2805:
		case 0x2806:
			scrollx[address - 0x2805] = data;
		return;

		case 0x2807:
			if (data == 0x80) protection_value++;
			if (data == 0x90) protection_value = 0; // reset
		return;
	}
}

static UINT8 exprraid_main_read(UINT16 address)
{
	switch (address)
	{
		case 0x1800:
			return DrvDips[0];

		case 0x1801:
			return DrvInputs[0];
			
		case 0x1802:
			return DrvInputs[1];

		case 0x1803:
			return (DrvDips[1] & ~0x40) | (DrvInputs[2] & 0x40);

		case 0x2800:
			return protection_value;

		case 0x2801:
			return 0x02; // protection status

		case 0x3800:
			return *vblank ? 0x02 : 0;
	}

	return 0;
}

static UINT8 exprraid_main_read_port(UINT16 address)
{
	switch (address)
	{
		case 0x01:
			return *vblank ? 0x02 : 0;
	}

	return 0;
}

static void exprraid_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x2000:
		case 0x2001:
			BurnYM2203Write(0, address & 1, data);
		return;

		case 0x4000:
		case 0x4001:
			BurnYM3526Write(address & 1, data);
		return;
	}
}

static UINT8 exprraid_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x2000:
		case 0x2001:
			return BurnYM2203Read(0, address & 1);

		case 0x4000:
		case 0x4001:
			return BurnYM3526Read(address & 1);

		case 0x6000:
			return soundlatch;
	}

	return 0;
}

static tilemap_callback( bg )
{
	offs = ((offs >> 1) & 0xf0) | (offs & 0xf) | (bgselect[((offs >> 4) & 1) | ((offs >> 8) & 2)] << 8);

	INT32 attr  = DrvGfxROM[3][offs + 0x4000];
	INT32 code  = DrvGfxROM[3][offs] + (attr * 256);
	INT32 color = attr >> 3; // & 3
	INT32 flags = TILE_FLIPYX((attr >> 2) & 1) | TILE_GROUP(attr >> 7);

	TILE_SET_INFO(2, code, color, flags);
}

static tilemap_callback( fg )
{
	INT32 attr = DrvColRAM[offs];
	INT32 code = DrvVidRAM[offs] + (attr * 256);

	TILE_SET_INFO(0, code, attr >> 4, 0);
}

static void DrvFMIRQHandler(INT32, INT32 nStatus)
{
	M6809SetIRQLine(CPU_IRQLINE_IRQ, nStatus ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	M6502Open(0);
	M6502Reset();
	M6502Close();

	M6809Open(0);
	M6809Reset();
	BurnYM2203Reset();
	BurnYM3526Reset();
	M6809Close();

	soundlatch = 0;
	protection_value = 0;
	flipscreen = 0;
	scrolly = 0;
	previous_coin = 0x43;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvMainROM			= Next; Next += 0x00c000;
	DrvSoundROM			= Next; Next += 0x008000;

	DrvGfxROM[0]		= Next; Next += 0x010000;
	DrvGfxROM[1]		= Next; Next += 0x080000;
	DrvGfxROM[2]		= Next; Next += 0x040000;
	DrvGfxROM[3]		= Next; Next += 0x008000;

	DrvColPROM			= Next; Next += 0x000300;

	DrvPalette			= (UINT32*)Next; Next += 0x0100 * sizeof(UINT32);

	AllRam				= Next;

	DrvMainRAM			= Next; Next += 0x000600;
	DrvSprRAM			= Next; Next += 0x000400;
	DrvVidRAM			= Next; Next += 0x000400;
	DrvColRAM			= Next; Next += 0x000400;
	DrvSoundRAM			= Next; Next += 0x002000;
	
	scrollx				= Next; Next += 0x000004;
	bgselect			= Next; Next += 0x000004;

	RamEnd				= Next;
	
	vblank				= Next; Next += 0x000004;

	MemEnd				= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[2]  = { 0, 4 };
	INT32 XOffs0[8]  = { STEP4(0x2000*8,1), STEP4(0,1) };
	INT32 YOffs0[8]  = { STEP8(0,8) };

	INT32 Plane1[3]  = { 0x80000*2, 0x80000*1, 0x80000*0 };
	INT32 XOffs1[16] = { STEP8(128,1), STEP8(0,1) };
	INT32 YOffs1[16] = { STEP16(0,8) };

	INT32 Plane2[3]  = { 4, 0x10000*8+0, 0x10000*8+4 };
	INT32 Plane3[3]  = { 0, 0x11000*8+0, 0x11000*8+4 };
	INT32 XOffs2[16] = { STEP4(0,1), STEP4(0x10000,1), STEP4(0x80,1), STEP4(0x10080,1) };
	INT32 YOffs2[16] = { STEP8(0,8), STEP8(64,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x30000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM[0], 0x4000);

	GfxDecode(0x0400, 2,  8,  8, Plane0, XOffs0, YOffs0, 0x040, tmp, DrvGfxROM[0]);

	memcpy (tmp, DrvGfxROM[1], 0x30000);

	GfxDecode(0x0800, 3, 16, 16, Plane1, XOffs1, YOffs1, 0x100, tmp, DrvGfxROM[1]);

	for (INT32 i = 0x7000; i >= 0; i -= 0x1000)
	{
		memcpy (DrvGfxROM[2] + (i * 2) + 0x0000, DrvGfxROM[2] + i, 0x1000);
		memcpy (DrvGfxROM[2] + (i * 2) + 0x1000, DrvGfxROM[2] + i, 0x1000);
	}

	memcpy (tmp, DrvGfxROM[2], 0x20000);

	for (INT32 i = 0; i < 4; i++)
	{
		GfxDecode(0x0080, 3, 16, 16, Plane2, XOffs2, YOffs2, 0x100, tmp + (i * 0x4000), DrvGfxROM[2] + 0x0000 + (i * 0x10000));
		GfxDecode(0x0080, 3, 16, 16, Plane3, XOffs2, YOffs2, 0x100, tmp + (i * 0x4000), DrvGfxROM[2] + 0x8000 + (i * 0x10000));
	}

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvMainROM   + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvMainROM   + 0x04000, k++, 1)) return 1;

		if (BurnLoadRom(DrvSoundROM  + 0x00000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[0] + 0x00000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[1] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x08000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x10000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x18000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x20000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x28000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[2] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x10000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x18000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[3] + 0x00000, k++, 1)) return 1;

		if (BurnLoadRom(DrvColPROM   + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x00100, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x00200, k++, 1)) return 1;

		DrvGfxDecode();
	}

	M6502Init(0, (bootleg_type == 2 || bootleg_type == 3) ? TYPE_M6502 : TYPE_DECO16);
	M6502Open(0);
	M6502MapMemory(DrvMainRAM,		0x0000, 0x05ff, MAP_RAM);
	M6502MapMemory(DrvSprRAM,		0x0600, 0x07ff, MAP_RAM);
	M6502MapMemory(DrvVidRAM,		0x0800, 0x0bff, MAP_RAM);
	M6502MapMemory(DrvColRAM,		0x0c00, 0x0fff, MAP_RAM);
	M6502MapMemory(DrvMainROM,		0x4000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(exprraid_main_write);
	M6502SetReadHandler(exprraid_main_read);
	M6502SetReadPortHandler(exprraid_main_read_port);
	M6502Close();

	M6809Init(0);
	M6809Open(0);
	M6809MapMemory(DrvSoundRAM,		0x0000, 0x1fff, MAP_RAM);
	M6809MapMemory(DrvSoundROM,		0x8000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(exprraid_sound_write);
	M6809SetReadHandler(exprraid_sound_read);
	M6809Close();

	BurnYM2203Init(1, 1500000, NULL, 0);
	BurnTimerAttach(&M6502Config, 1500000);
	BurnYM2203SetAllRoutes(0, 0.30, BURN_SND_ROUTE_BOTH);

	BurnYM3526Init(3000000, &DrvFMIRQHandler, 1);
	BurnTimerAttachYM3526(&M6809Config, 2000000);
	BurnYM3526SetRoute(BURN_SND_YM3526_ROUTE, 0.60, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, fg_map_callback,  8,  8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 2,  8,  8, 0x10000, 0x80, 1);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 3, 16, 16, 0x80000, 0x40, 7);
	GenericTilemapSetGfx(2, DrvGfxROM[2], 3, 16, 16, 0x40000, 0x00, 3);
	GenericTilemapSetOffsets(0, -1, -8); // -1 ??
	GenericTilemapSetOffsets(1,  0, -8);
	GenericTilemapSetScrollRows(0, 2);
	GenericTilemapSetTransparent(1,0);

	if (bootleg_type == 1)
	{
		DrvMainROM[0xfff7 - 0x4000] = DrvMainROM[0xfffa - 0x4000]; // vectors
		DrvMainROM[0xfff6 - 0x4000] = DrvMainROM[0xfffb - 0x4000];
		DrvMainROM[0xfff1 - 0x4000] = DrvMainROM[0xfffc - 0x4000];
		DrvMainROM[0xfff0 - 0x4000] = DrvMainROM[0xfffd - 0x4000];
		DrvMainROM[0xfff3 - 0x4000] = DrvMainROM[0xfffe - 0x4000];
		DrvMainROM[0xfff2 - 0x4000] = DrvMainROM[0xffff - 0x4000];
	}

	if (bootleg_type == 3)
	{
		vblank = DrvMainROM + (0xffc0 - 0x4000);
	}

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6809Exit();
	M6502Exit();
	BurnYM3526Exit();
	BurnYM2203Exit();

	BurnFreeMemIndex();

	bootleg_type = -1;

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x100; i++)
	{
		INT32 bit0,bit1,bit2,bit3,r,g,b;

		bit0 = (DrvColPROM[i + 0x000] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + 0x000] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + 0x000] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + 0x000] >> 3) & 0x01;
		r = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[i + 0x100] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + 0x100] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + 0x100] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + 0x100] >> 3) & 0x01;
		g = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[i + 0x200] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + 0x200] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + 0x200] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + 0x200] >> 3) & 0x01;
		b = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0; offs < 0x200; offs += 4)
	{
		INT32 sy    = DrvSprRAM[offs + 0];
		INT32 attr  = DrvSprRAM[offs + 1];
		INT32 sx    = ((248 - DrvSprRAM[offs + 2]) & 0xff) - 8;
		INT32 code  = DrvSprRAM[offs + 3] + ((attr << 3) & 0x700);
		INT32 color = (attr & 3) | ((attr >> 1) & 4);
		INT32 flipx = attr & 4;
		INT32 flipy = 0;

		if (flipscreen)
		{
			sx = 240 - sx;
			sy = 240 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		Draw16x16MaskTile(pTransDraw, code, sx, sy - 8, flipx, flipy, color, 3, 0, 0x40, DrvGfxROM[1]);

		if (attr & 0x10)
		{
			Draw16x16MaskTile(pTransDraw, code+1, sx, sy + (flipscreen ? -24 : 8), flipx, flipy, color, 3, 0, 0x40, DrvGfxROM[1]);
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	GenericTilemapSetFlip(TMAP_GLOBAL, flipscreen ? (TMAP_FLIPX | TMAP_FLIPY) : 0);

	GenericTilemapSetScrollY(0, scrolly);
	GenericTilemapSetScrollRow(0, 0, scrollx[0]);
	GenericTilemapSetScrollRow(0, 1, scrollx[1]);

	BurnTransferClear(0);

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);
	if (nSpriteEnable & 1) draw_sprites();
	if (nBurnLayer & 2) GenericTilemapDraw(0, pTransDraw, TMAP_SET_GROUP(1));
	if (nBurnLayer & 4) GenericTilemapDraw(1, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	M6502NewFrame();
	M6809NewFrame();

	M6502Open(0);
	M6809Open(0);

	{
		previous_coin = (DrvInputs[1] >> 6) | (DrvInputs[2] & 0x40);

		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		if (previous_coin == 0x43 && ((DrvInputs[1] >> 6) | (DrvInputs[2] & 0x40)) != 0x43) {
			M6502SetIRQLine((bootleg_type == 2 || bootleg_type == 3) ? CPU_IRQLINE_NMI : CPU_IRQLINE_IRQ, CPU_IRQSTATUS_ACK);
		}

		if (bootleg_type == 2 || bootleg_type == 3)
		{
			if (previous_coin != 0x43 && ((DrvInputs[1] >> 6) | (DrvInputs[2] & 0x40)) == 0x43) {
				M6502SetIRQLine(CPU_IRQLINE_NMI, CPU_IRQSTATUS_NONE);
			}
		}
	}

	INT32 nInterleave = 262;
	INT32 nCyclesTotal[2] = { 1500000 / 60, 2000000 / 60 };

	*vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		BurnTimerUpdate(i * (nCyclesTotal[0] / nInterleave));
		BurnTimerUpdateYM3526((i + 1) * (nCyclesTotal[1] / nInterleave));

		if (i == 247) *vblank = 0xff;
	}

	BurnTimerEndFrame(nCyclesTotal[0]);
	BurnTimerEndFrameYM3526(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
		BurnYM3526Update(pBurnSoundOut, nBurnSoundLen);
	}

	M6809Close();
	M6502Close();

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029702;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		M6502Scan(nAction);
		M6809Scan(nAction);
		BurnYM2203Scan(nAction, pnMin);
		BurnYM3526Scan(nAction,pnMin);

		SCAN_VAR(soundlatch);
		SCAN_VAR(flipscreen);
		SCAN_VAR(scrolly);
		SCAN_VAR(protection_value);
		SCAN_VAR(previous_coin);
	}

	return 0;
}


// Express Raider (World, Rev 4)

static struct BurnRomInfo exprraidRomDesc[] = {
	{ "cz01-2e.16b",	0x4000, 0xa0ae6756, 1 | BRF_PRG | BRF_ESS }, //  0 DECO16 CPU Code
	{ "cz00-4e.15a",	0x8000, 0x910f6ccc, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "cz02-1.2a",		0x8000, 0x552e6112, 2 | BRF_GRA },           //  2 M6809 Code

	{ "cz07.5b",		0x4000, 0x686bac23, 3 | BRF_GRA },           //  3 Characters

	{ "cz09.16h",		0x8000, 0x1ed250d1, 4 | BRF_GRA },           //  4 Sprites
	{ "cz08.14h",		0x8000, 0x2293fc61, 4 | BRF_GRA },           //  5
	{ "cz13.16k",		0x8000, 0x7c3bfd00, 4 | BRF_GRA },           //  6
	{ "cz12.14k",		0x8000, 0xea2294c8, 4 | BRF_GRA },           //  7
	{ "cz11.13k",		0x8000, 0xb7418335, 4 | BRF_GRA },           //  8
	{ "cz10.11k",		0x8000, 0x2f611978, 4 | BRF_GRA },           //  9

	{ "cz04.8e",		0x8000, 0x643a1bd3, 5 | BRF_GRA },           // 10 Background Tiles
	{ "cz05.8f",		0x8000, 0xc44570bf, 5 | BRF_GRA },           // 11
	{ "cz06.8h",		0x8000, 0xb9bb448b, 5 | BRF_GRA },           // 12

	{ "cz03.12d",		0x8000, 0x6ce11971, 6 | BRF_GRA },           // 13 Background Tile Map

	{ "cy-17.5b",		0x0100, 0xda31dfbc, 7 | BRF_GRA },           // 14 Color Data
	{ "cy-16.6b",		0x0100, 0x51f25b4c, 7 | BRF_GRA },           // 15
	{ "cy-15.7b",		0x0100, 0xa6168d7f, 7 | BRF_GRA },           // 16
	
	{ "cy-14.9b",		0x0100, 0x52aad300, 0 | BRF_OPT },           // 17 Unknown PROM

	{ "pal16r4a.5c",	0x0104, 0xd66aaa87, 0 | BRF_OPT },           // 18 PLDs
	{ "pal16r4a.5e",	0x0104, 0x9a8766a7, 0 | BRF_OPT },           // 19
};

STD_ROM_PICK(exprraid)
STD_ROM_FN(exprraid)

struct BurnDriver BurnDrvExprraid = {
	"exprraid", NULL, NULL, NULL, "1986",
	"Express Raider (World, Rev 4)\0", NULL, "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PREFIX_DATAEAST, GBF_SCRFIGHT, 0,
	NULL, exprraidRomInfo, exprraidRomName, NULL, NULL, NULL, NULL, ExprraidInputInfo, ExprraidDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 240, 4, 3
};


// Express Raider (US, rev 5)

static struct BurnRomInfo exprraiduRomDesc[] = {
	{ "cz01-5a.16b",	0x4000, 0xdc8f9fba, 1 | BRF_PRG | BRF_ESS }, //  0 DECO16 CPU Code
	{ "cz00-5.15a",		0x8000, 0xa81290bc, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "cz02-1.2a",		0x8000, 0x552e6112, 2 | BRF_GRA },           //  2 M6809 Code

	{ "cz07.5b",		0x4000, 0x686bac23, 3 | BRF_GRA },           //  3 Characters

	{ "cz09.16h",		0x8000, 0x1ed250d1, 4 | BRF_GRA },           //  4 Sprites
	{ "cz08.14h",		0x8000, 0x2293fc61, 4 | BRF_GRA },           //  5
	{ "cz13.16k",		0x8000, 0x7c3bfd00, 4 | BRF_GRA },           //  6
	{ "cz12.14k",		0x8000, 0xea2294c8, 4 | BRF_GRA },           //  7
	{ "cz11.13k",		0x8000, 0xb7418335, 4 | BRF_GRA },           //  8
	{ "cz10.11k",		0x8000, 0x2f611978, 4 | BRF_GRA },           //  9

	{ "cz04.8e",		0x8000, 0x643a1bd3, 5 | BRF_GRA },           // 10 Background Tiles
	{ "cz05.8f",		0x8000, 0xc44570bf, 5 | BRF_GRA },           // 11
	{ "cz06.8h",		0x8000, 0xb9bb448b, 5 | BRF_GRA },           // 12

	{ "cz03.12d",		0x8000, 0x6ce11971, 6 | BRF_GRA },           // 13 Background Tile Map

	{ "cy-17.5b",		0x0100, 0xda31dfbc, 7 | BRF_GRA },           // 14 Color Data
	{ "cy-16.6b",		0x0100, 0x51f25b4c, 7 | BRF_GRA },           // 15
	{ "cy-15.7b",		0x0100, 0xa6168d7f, 7 | BRF_GRA },           // 16
	
	{ "cy-14.9b",		0x0100, 0x52aad300, 0 | BRF_OPT },           // 17 Unknown PROM

	{ "pal16r4a.5c",	0x0104, 0xd66aaa87, 0 | BRF_OPT },           // 18 PLDs
	{ "pal16r4a.5e",	0x0104, 0x9a8766a7, 0 | BRF_OPT },           // 19
};

STD_ROM_PICK(exprraidu)
STD_ROM_FN(exprraidu)

struct BurnDriver BurnDrvExprraidu = {
	"exprraidu", "exprraid", NULL, NULL, "1986",
	"Express Raider (US, rev 5)\0", NULL, "Data East USA", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_DATAEAST, GBF_SCRFIGHT, 0,
	NULL, exprraiduRomInfo, exprraiduRomName, NULL, NULL, NULL, NULL, ExprraidInputInfo, ExprraidDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 240, 4, 3
};


// Express Raider (Italy)

static struct BurnRomInfo exprraidiRomDesc[] = {
	{ "cz01-2e.16b",	0x4000, 0xa0ae6756, 1 | BRF_PRG | BRF_ESS }, //  0 DECO16 CPU Code
	{ "exraidi6.15a",	0x8000, 0xa3d98118, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "cz02-1.2a",		0x8000, 0x552e6112, 2 | BRF_GRA },           //  2 M6809 Code

	{ "cz07.5b",		0x4000, 0x686bac23, 3 | BRF_GRA },           //  3 Characters

	{ "cz09.16h",		0x8000, 0x1ed250d1, 4 | BRF_GRA },           //  4 Sprites
	{ "cz08.14h",		0x8000, 0x2293fc61, 4 | BRF_GRA },           //  5
	{ "cz13.16k",		0x8000, 0x7c3bfd00, 4 | BRF_GRA },           //  6
	{ "cz12.14k",		0x8000, 0xea2294c8, 4 | BRF_GRA },           //  7
	{ "cz11.13k",		0x8000, 0xb7418335, 4 | BRF_GRA },           //  8
	{ "cz10.11k",		0x8000, 0x2f611978, 4 | BRF_GRA },           //  9

	{ "cz04.8e",		0x8000, 0x643a1bd3, 5 | BRF_GRA },           // 10 Background Tiles
	{ "cz05.8f",		0x8000, 0xc44570bf, 5 | BRF_GRA },           // 11
	{ "cz06.8h",		0x8000, 0xb9bb448b, 5 | BRF_GRA },           // 12

	{ "cz03.12d",		0x8000, 0x6ce11971, 6 | BRF_GRA },           // 13 Background Tile Map

	{ "cy-17.5b",		0x0100, 0xda31dfbc, 7 | BRF_GRA },           // 14 Color Data
	{ "cy-16.6b",		0x0100, 0x51f25b4c, 7 | BRF_GRA },           // 15
	{ "cy-15.7b",		0x0100, 0xa6168d7f, 7 | BRF_GRA },           // 16
	
	{ "cy-14.9b",		0x0100, 0x52aad300, 0 | BRF_OPT },           // 17 Unknown PROM

	{ "pal16r4a.5c",	0x0104, 0xd66aaa87, 0 | BRF_OPT },           // 18 PLDs
	{ "pal16r4a.5e",	0x0104, 0x9a8766a7, 0 | BRF_OPT },           // 19
};

STD_ROM_PICK(exprraidi)
STD_ROM_FN(exprraidi)

struct BurnDriver BurnDrvExprraidi = {
	"exprraidi", "exprraid", NULL, NULL, "1986",
	"Express Raider (Italy)\0", NULL, "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_DATAEAST, GBF_SCRFIGHT, 0,
	NULL, exprraidiRomInfo, exprraidiRomName, NULL, NULL, NULL, NULL, ExprraidInputInfo, ExprraidDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 240, 4, 3
};


// Western Express (Japan, rev 4)

static struct BurnRomInfo wexpressRomDesc[] = {
	{ "cy01-2.16b",		0x4000, 0xa0ae6756, 1 | BRF_PRG | BRF_ESS }, //  0 DECO16 CPU Code
	{ "cy00-4.15a",		0x8000, 0xc66d4dd3, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "cy02-1.2a",		0x8000, 0x552e6112, 2 | BRF_GRA },           //  2 M6809 Code

	{ "cz07.5b",		0x4000, 0x686bac23, 3 | BRF_GRA },           //  3 Characters

	{ "cz09.16h",		0x8000, 0x1ed250d1, 4 | BRF_GRA },           //  4 Sprites
	{ "cz08.14h",		0x8000, 0x2293fc61, 4 | BRF_GRA },           //  5
	{ "cz13.16k",		0x8000, 0x7c3bfd00, 4 | BRF_GRA },           //  6
	{ "cz12.14k",		0x8000, 0xea2294c8, 4 | BRF_GRA },           //  7
	{ "cz11.13k",		0x8000, 0xb7418335, 4 | BRF_GRA },           //  8
	{ "cz10.11k",		0x8000, 0x2f611978, 4 | BRF_GRA },           //  9

	{ "cy04.8e",		0x8000, 0xf2e93ff0, 5 | BRF_GRA },           // 10 Background Tiles
	{ "cy05.8f",		0x8000, 0xc44570bf, 5 | BRF_GRA },           // 11
	{ "cy06.8h",		0x8000, 0xc3a56de5, 5 | BRF_GRA },           // 12

	{ "cy03.12d",		0x8000, 0x242e3e64, 6 | BRF_GRA },           // 13 Background Tile Map

	{ "cy-17.5b",		0x0100, 0xda31dfbc, 7 | BRF_GRA },           // 14 Color Data
	{ "cy-16.6b",		0x0100, 0x51f25b4c, 7 | BRF_GRA },           // 15
	{ "cy-15.7b",		0x0100, 0xa6168d7f, 7 | BRF_GRA },           // 16
	
	{ "cy-14.9b",		0x0100, 0x52aad300, 0 | BRF_OPT },           // 17 Unknown PROM

	{ "pal16r4a.5c",	0x0104, 0xd66aaa87, 0 | BRF_OPT },           // 18 PLDs
	{ "pal16r4a.5e",	0x0104, 0x9a8766a7, 0 | BRF_OPT },           // 19
};

STD_ROM_PICK(wexpress)
STD_ROM_FN(wexpress)

struct BurnDriver BurnDrvWexpress = {
	"wexpress", "exprraid", NULL, NULL, "1986",
	"Western Express (Japan, rev 4)\0", NULL, "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_DATAEAST, GBF_SCRFIGHT, 0,
	NULL, wexpressRomInfo, wexpressRomName, NULL, NULL, NULL, NULL, ExprraidInputInfo, ExprraidDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 240, 4, 3
};


// Western Express (bootleg set 1)

static struct BurnRomInfo wexpressb1RomDesc[] = {
	{ "2.16b",			0x4000, 0xea5e5a8f, 1 | BRF_PRG | BRF_ESS }, //  0 DECO16 CPU Code
	{ "1.15a",			0x8000, 0xa7daae12, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "cy02-1.2a",		0x8000, 0x552e6112, 2 | BRF_GRA },           //  2 M6809 Code

	{ "cz07.5b",		0x4000, 0x686bac23, 3 | BRF_GRA },           //  3 Characters

	{ "cz09.16h",		0x8000, 0x1ed250d1, 4 | BRF_GRA },           //  4 Sprites
	{ "cz08.14h",		0x8000, 0x2293fc61, 4 | BRF_GRA },           //  5
	{ "cz13.16k",		0x8000, 0x7c3bfd00, 4 | BRF_GRA },           //  6
	{ "cz12.14k",		0x8000, 0xea2294c8, 4 | BRF_GRA },           //  7
	{ "cz11.13k",		0x8000, 0xb7418335, 4 | BRF_GRA },           //  8
	{ "cz10.11k",		0x8000, 0x2f611978, 4 | BRF_GRA },           //  9

	{ "cy04.8e",		0x8000, 0xf2e93ff0, 5 | BRF_GRA },           // 10 Background Tiles
	{ "cy05.8f",		0x8000, 0xc44570bf, 5 | BRF_GRA },           // 11
	{ "cy06.8h",		0x8000, 0xc3a56de5, 5 | BRF_GRA },           // 12

	{ "cy03.12d",		0x8000, 0x242e3e64, 6 | BRF_GRA },           // 13 Background Tile Map

	{ "cy-17.5b",		0x0100, 0xda31dfbc, 7 | BRF_GRA },           // 14 Color Data
	{ "cy-16.6b",		0x0100, 0x51f25b4c, 7 | BRF_GRA },           // 15
	{ "cy-15.7b",		0x0100, 0xa6168d7f, 7 | BRF_GRA },           // 16
	
	{ "cy-14.9b",		0x0100, 0x52aad300, 0 | BRF_OPT },           // 17 Unknown PROM

	{ "pal16r4a.5c",	0x0104, 0xd66aaa87, 0 | BRF_OPT },           // 18 PLDs
	{ "pal16r4a.5e",	0x0104, 0x9a8766a7, 0 | BRF_OPT },           // 19
};

STD_ROM_PICK(wexpressb1)
STD_ROM_FN(wexpressb1)

static INT32 Wexpressb1Init()
{
	bootleg_type = 1;
	return DrvInit();
}

struct BurnDriver BurnDrvWexpressb1 = {
	"wexpressb1", "exprraid", NULL, NULL, "1986",
	"Western Express (bootleg set 1)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_PREFIX_DATAEAST, GBF_SCRFIGHT, 0,
	NULL, wexpressb1RomInfo, wexpressb1RomName, NULL, NULL, NULL, NULL, ExprraidInputInfo, ExprraidDIPInfo,
	Wexpressb1Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 240, 4, 3
};


// Western Express (bootleg set 2)

static struct BurnRomInfo wexpressb2RomDesc[] = {
	{ "wexpress.3",		0x4000, 0xb4dd0fa4, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "wexpress.1",		0x8000, 0xe8466596, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "cy02-1.2a",		0x8000, 0x552e6112, 2 | BRF_GRA },           //  2 M6809 Code

	{ "cz07.5b",		0x4000, 0x686bac23, 3 | BRF_GRA },           //  3 Characters

	{ "cz09.16h",		0x8000, 0x1ed250d1, 4 | BRF_GRA },           //  4 Sprites
	{ "cz08.14h",		0x8000, 0x2293fc61, 4 | BRF_GRA },           //  5
	{ "cz13.16k",		0x8000, 0x7c3bfd00, 4 | BRF_GRA },           //  6
	{ "cz12.14k",		0x8000, 0xea2294c8, 4 | BRF_GRA },           //  7
	{ "cz11.13k",		0x8000, 0xb7418335, 4 | BRF_GRA },           //  8
	{ "cz10.11k",		0x8000, 0x2f611978, 4 | BRF_GRA },           //  9

	{ "cy04.8e",		0x8000, 0xf2e93ff0, 5 | BRF_GRA },           // 10 Background Tiles
	{ "cy05.8f",		0x8000, 0xc44570bf, 5 | BRF_GRA },           // 11
	{ "cy06.8h",		0x8000, 0xc3a56de5, 5 | BRF_GRA },           // 12

	{ "cy03.12d",		0x8000, 0x242e3e64, 6 | BRF_GRA },           // 13 Background Tile Map

	{ "cy-17.5b",		0x0100, 0xda31dfbc, 7 | BRF_GRA },           // 14 Color Data
	{ "cy-16.6b",		0x0100, 0x51f25b4c, 7 | BRF_GRA },           // 15
	{ "cy-15.7b",		0x0100, 0xa6168d7f, 7 | BRF_GRA },           // 16
	
	{ "cy-14.9b",		0x0100, 0x52aad300, 0 | BRF_OPT },           // 17 Unknown PROM
};

STD_ROM_PICK(wexpressb2)
STD_ROM_FN(wexpressb2)

static INT32 Wexpressb2Init()
{
	bootleg_type = 2;
	return DrvInit();
}

struct BurnDriver BurnDrvWexpressb2 = {
	"wexpressb2", "exprraid", NULL, NULL, "1986",
	"Western Express (bootleg set 2)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_PREFIX_DATAEAST, GBF_SCRFIGHT, 0,
	NULL, wexpressb2RomInfo, wexpressb2RomName, NULL, NULL, NULL, NULL, ExprraidInputInfo, ExprraidDIPInfo,
	Wexpressb2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 240, 4, 3
};


// Western Express (bootleg set 3)

static struct BurnRomInfo wexpressb3RomDesc[] = {
	{ "s2.16b",			0x4000, 0x40d70fcb, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "s1.15a",			0x8000, 0x7c573824, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "cy02-1.2a",		0x8000, 0x552e6112, 2 | BRF_GRA },           //  2 M6809 Code

	{ "cz07.5b",		0x4000, 0x686bac23, 3 | BRF_GRA },           //  3 Characters

	{ "cz09.16h",		0x8000, 0x1ed250d1, 4 | BRF_GRA },           //  4 Sprites
	{ "cz08.14h",		0x8000, 0x2293fc61, 4 | BRF_GRA },           //  5
	{ "cz13.16k",		0x8000, 0x7c3bfd00, 4 | BRF_GRA },           //  6
	{ "cz12.14k",		0x8000, 0xea2294c8, 4 | BRF_GRA },           //  7
	{ "cz11.13k",		0x8000, 0xb7418335, 4 | BRF_GRA },           //  8
	{ "cz10.11k",		0x8000, 0x2f611978, 4 | BRF_GRA },           //  9

	{ "cy04.8e",		0x8000, 0xf2e93ff0, 5 | BRF_GRA },           // 10 Background Tiles
	{ "cy05.8f",		0x8000, 0xc44570bf, 5 | BRF_GRA },           // 11
	{ "cy06.8h",		0x8000, 0xc3a56de5, 5 | BRF_GRA },           // 12

	{ "3.12d",			0x8000, 0x242e3e64, 6 | BRF_GRA },           // 13 Background Tile Map

	{ "cy-17.5b",		0x0100, 0xda31dfbc, 7 | BRF_GRA },           // 14 Color Data
	{ "cy-16.6b",		0x0100, 0x51f25b4c, 7 | BRF_GRA },           // 15
	{ "cy-15.7b",		0x0100, 0xa6168d7f, 7 | BRF_GRA },           // 16
	
	{ "cy-14.9b",		0x0100, 0x52aad300, 0 | BRF_OPT },           // 17 Unknown PROM
};

STD_ROM_PICK(wexpressb3)
STD_ROM_FN(wexpressb3)

static INT32 Wexpressb3Init()
{
	bootleg_type = 3;
	return DrvInit();
}

struct BurnDriver BurnDrvWexpressb3 = {
	"wexpressb3", "exprraid", NULL, NULL, "1986",
	"Western Express (bootleg set 3)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_PREFIX_DATAEAST, GBF_SCRFIGHT, 0,
	NULL, wexpressb3RomInfo, wexpressb3RomName, NULL, NULL, NULL, NULL, ExprraidInputInfo, ExprraidDIPInfo,
	Wexpressb3Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 240, 4, 3
};
