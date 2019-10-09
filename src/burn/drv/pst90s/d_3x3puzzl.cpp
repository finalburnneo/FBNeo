// FB Alpha Aquarium driver module
// Based on MAME driver by David Haywood

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "msm6295.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvSndROM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvVidRAM0;
static UINT8 *DrvVidRAM1;
static UINT8 *DrvVidRAM2;
static UINT8 *DrvVidBuf0;
static UINT8 *DrvVidBuf1;
static UINT8 *DrvVidBuf2;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT16 scrollx;
static UINT16 scrolly;
static UINT16 graphics_control;

static INT32 vblank;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT16 DrvInputs[2];
static UINT8 DrvDips[2];
static UINT8 DrvReset;

static struct BurnInputInfo Puzzle3x3InputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy2 + 2,	"diag"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Puzzle3x3)

static struct BurnInputInfo CasanovaInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Casanova)

static struct BurnDIPInfo Puzzle3x3DIPList[]=
{
	{0x10, 0xff, 0xff, 0xff, NULL			},
	{0x11, 0xff, 0xff, 0xfb, NULL			},

	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{0x11, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0x03, 0x01, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x03, 0x00, "3 Coins 1 Credits"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x11, 0x01, 0x04, 0x04, "Off"			},
	{0x11, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x11, 0x01, 0x18, 0x18, "Normal"		},
	{0x11, 0x01, 0x18, 0x10, "Easy"			},
	{0x11, 0x01, 0x18, 0x08, "Easiest"		},
	{0x11, 0x01, 0x18, 0x00, "Hard"			},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x11, 0x01, 0x20, 0x20, "Off"			},
	{0x11, 0x01, 0x20, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Free Play / Debug mode"},
	{0x11, 0x01, 0x40, 0x40, "Off"			},
	{0x11, 0x01, 0x40, 0x00, "On"			},
};

STDDIPINFO(Puzzle3x3)

static struct BurnDIPInfo CasanovaDIPList[]=
{
	{0x10, 0xff, 0xff, 0xff, NULL			},
	{0x11, 0xff, 0xff, 0xff, NULL			},


	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{0x11, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x03, 0x01, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x03, 0x00, "3 Coins 1 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x11, 0x01, 0x0c, 0x08, "Easy"			},
	{0x11, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x11, 0x01, 0x0c, 0x04, "Hard"			},
	{0x11, 0x01, 0x0c, 0x00, "Very Hard"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x11, 0x01, 0x10, 0x10, "Off"			},
	{0x11, 0x01, 0x10, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Dip Info"		},
	{0x11, 0x01, 0x80, 0x80, "Off"			},
	{0x11, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Casanova)

static void set_okibank()
{
	MSM6295SetBank(0, DrvSndROM + ((graphics_control & 6) * 0x20000), 0, 0x3ffff);
}

static void __fastcall puzzle3x3_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x400000:
			scrollx = data;
		return;

		case 0x480000:
			scrolly = data;
		return;

		case 0x700000:
			MSM6295Write(0, data & 0xff);
		return;

		case 0x800000:
			graphics_control = data;
			set_okibank();
		return;
	}
}

static void __fastcall puzzle3x3_write_byte(UINT32 address, UINT8 data)
{
	bprintf (0, _T("WB: %5.5x, %2.2x\n"), address, data);
}

static UINT16 __fastcall puzzle3x3_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x280000:
			return vblank ? 0xffff : 0;

		case 0x500000:
			return DrvInputs[0];

		case 0x580000:
			return DrvInputs[1];

		case 0x600000:
			return DrvDips[0] + (DrvDips[1] * 256);

		case 0x700000:
			return MSM6295Read(0);
	}

	return 0;
}

static UINT8 __fastcall puzzle3x3_read_byte(UINT32 address)
{
	bprintf (0, _T("RB: %5.5x\n"), address);

	return 0;
}

static tilemap_callback( layer0 )
{
	UINT16 *ram = (UINT16*)DrvVidBuf0;

	TILE_SET_INFO(0, ram[offs], 0, 0);
}

static tilemap_callback( layer1 )
{
	UINT16 *ram = (UINT16*)DrvVidBuf1;

	TILE_SET_INFO(1, ram[offs], 0, 0);
}

static tilemap_callback( layer2 )
{
	UINT16 *ram = (UINT16*)DrvVidBuf2;

	TILE_SET_INFO(2, ram[offs], 0, 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	MSM6295Reset(0);

	scrollx = 0;
	scrolly = 0;
	graphics_control = 0;
	set_okibank();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM	= Next; Next += 0x080000;

	DrvGfxROM0	= Next; Next += 0x400000;
	DrvGfxROM1	= Next; Next += 0x200000;
	DrvGfxROM2	= Next; Next += 0x200000;

	MSM6295ROM	= Next;
	DrvSndROM	= Next; Next += 0x100000; // ?

	DrvPalette	= (UINT32*)Next; Next += 0x0300 * sizeof(UINT32);

	AllRam		= Next;

	Drv68KRAM	= Next; Next += 0x010000;
	DrvPalRAM	= Next; Next += 0x000800;
	DrvVidRAM0	= Next; Next += 0x000800;
	DrvVidRAM1	= Next; Next += 0x001000;
	DrvVidRAM2	= Next; Next += 0x001000;
	DrvVidBuf0	= Next; Next += 0x000800;
	DrvVidBuf1	= Next; Next += 0x001000;
	DrvVidBuf2	= Next; Next += 0x001000;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static void DrvVideoInit()
{
	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, layer0_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, layer1_map_callback,  8,  8, 64, 32);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, layer2_map_callback,  8,  8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 8, 16, 16, 0x400000, 0x000, 0);
	GenericTilemapSetGfx(1, DrvGfxROM1, 8,  8,  8, 0x200000, 0x100, 0);
	GenericTilemapSetGfx(2, DrvGfxROM2, 8,  8,  8, 0x200000, 0x200, 0);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetTransparent(2, 0);
}

static INT32 DrvInit(INT32 game_select)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (game_select == 0) // 3x3 Puzzle
	{
		if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  2, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000001,  3, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000002,  4, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000003,  5, 4)) return 1;
		memcpy (DrvGfxROM0 + 0x200000, DrvGfxROM0, 0x200000); // mirror

		if (BurnLoadRom(DrvGfxROM1 + 0x000003,  6, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x000002,  7, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x000001,  8, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  9, 4)) return 1;
		memcpy (DrvGfxROM1 + 0x080000, DrvGfxROM1, 0x080000); // mirror
		memcpy (DrvGfxROM1 + 0x100000, DrvGfxROM1, 0x080000);
		memcpy (DrvGfxROM1 + 0x180000, DrvGfxROM1, 0x080000);

		if (BurnLoadRom(DrvGfxROM2 + 0x000003, 10, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x000002, 11, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x000001, 12, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x000000, 13, 4)) return 1;
		memcpy (DrvGfxROM2 + 0x080000, DrvGfxROM2, 0x080000); // mirror
		memcpy (DrvGfxROM2 + 0x100000, DrvGfxROM2, 0x080000);
		memcpy (DrvGfxROM2 + 0x180000, DrvGfxROM2, 0x080000);

		if (BurnLoadRom(DrvSndROM  + 0x000000, 14, 1)) return 1;
	}
	else // Casanova
	{
		if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  2, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000001,  3, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000002,  4, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000003,  5, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x200000,  6, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x200001,  7, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x200002,  8, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x200003,  9, 4)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000003, 10, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x000002, 11, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x000001, 12, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x000000, 13, 4)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x000003, 14, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x000002, 15, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x000001, 16, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x000000, 17, 4)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x000000, 18, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x080000, 19, 1)) return 1;
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,		0x100000, 0x10ffff, MAP_RAM);
	SekMapMemory(DrvVidRAM0,	0x200000, 0x2007ff, MAP_RAM);
	SekMapMemory(DrvVidRAM1,	0x201000, 0x201fff, MAP_RAM);
	SekMapMemory(DrvVidRAM2,	0x202000, 0x202fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		0x300000, 0x3007ff, MAP_RAM); // 0-600
	SekSetWriteWordHandler(0,	puzzle3x3_write_word);
	SekSetWriteByteHandler(0,	puzzle3x3_write_byte);
	SekSetReadWordHandler(0,	puzzle3x3_read_word);
	SekSetReadByteHandler(0,	puzzle3x3_read_byte);
	SekClose();

	MSM6295Init(0, 1000000 / 132, 0);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	DrvVideoInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	MSM6295Exit(0);

	SekExit();

	BurnFree (AllMem);

	MSM6295ROM = NULL;

	return 0;
}

static void DrvPaletteUpdate()
{
	UINT16 *p = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x600/2; i++)
	{
		UINT8 r = (p[i] & 0x001f);
		UINT8 g = (p[i] & 0x03e0) >> 5;
		UINT8 b = (p[i] & 0x7c00) >> 10;

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1; // force always update
	}

	INT32 screensize = (graphics_control & 0x10) ? 512 : 320;

	if (screensize != nScreenWidth)
	{
		GenericTilesExit();
		BurnDrvSetVisibleSize(screensize, 240);
		Reinitialise();
		DrvVideoInit(); // incredibly slow!

		return 1; // don't draw this time around
	}

	GenericTilemapSetScrollX(0, scrollx);
	GenericTilemapSetScrollY(0, scrolly);

	if ((nBurnLayer & 1) == 0) BurnTransferClear();

	if ((nBurnLayer & 1) != 0) GenericTilemapDraw(0, pTransDraw, 0);
	if ((nBurnLayer & 2) != 0) GenericTilemapDraw(1, pTransDraw, 0);
	if ((nBurnLayer & 4) != 0) GenericTilemapDraw(2, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, 2 * sizeof(UINT16));

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	vblank = 0;
	SekOpen(0);
	SekRun(((10000000 / 60) * 15) / 16);
	SekSetIRQLine(4, CPU_IRQSTATUS_ACK); // OR AUTO?
	vblank = 1;
	if (pBurnDraw) {
		DrvDraw();
	}

	if (graphics_control & 0x20)
	{
		memcpy (DrvVidBuf0, DrvVidRAM0, 0x0800);
		memcpy (DrvVidBuf1, DrvVidRAM1, 0x1000);
		memcpy (DrvVidBuf2, DrvVidRAM2, 0x1000);
	}

	SekRun(((10000000 / 60) *  1) / 16);
	SekSetIRQLine(4, CPU_IRQSTATUS_NONE);
	SekClose();

	if (pBurnSoundOut) {
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
	}

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
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);

		MSM6295Scan(nAction, pnMin);

		SCAN_VAR(scrollx);
		SCAN_VAR(scrolly);
		SCAN_VAR(graphics_control);
	}

	if (nAction & ACB_WRITE) {
		set_okibank();
	}

	return 0;
}


// 3X3 Puzzle (Enterprise)

static struct BurnRomInfo puzzl3x3RomDesc[] = {
	{ "1.bin",		0x20000, 0xe9c39ee7, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "2.bin",		0x20000, 0x524963be, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "3.bin",		0x80000, 0x53c2aa6a, 2 | BRF_GRA },           //  2 Background Tiles
	{ "4.bin",		0x80000, 0xfb0b76fd, 2 | BRF_GRA },           //  3
	{ "5.bin",		0x80000, 0xb6c1e108, 2 | BRF_GRA },           //  4
	{ "6.bin",		0x80000, 0x47cb0e8e, 2 | BRF_GRA },           //  5

	{ "7.bin",		0x20000, 0x45b1f58b, 3 | BRF_GRA },           //  6 Midground Tiles
	{ "8.bin",		0x20000, 0xc0d404a7, 3 | BRF_GRA },           //  7
	{ "9.bin",		0x20000, 0x6b303aa9, 3 | BRF_GRA },           //  8
	{ "10.bin",		0x20000, 0x6d0107bc, 3 | BRF_GRA },           //  9

	{ "11.bin",		0x20000, 0xe124c0b5, 4 | BRF_GRA },           // 10 Foreground Tiles
	{ "12.bin",		0x20000, 0xae4a8707, 4 | BRF_GRA },           // 11
	{ "13.bin",		0x20000, 0xf06925d1, 4 | BRF_GRA },           // 12
	{ "14.bin",		0x20000, 0x07252636, 4 | BRF_GRA },           // 13

	{ "15.bin",		0x80000, 0xd3aff355, 5 | BRF_SND },           // 14 Samples
};

STD_ROM_PICK(puzzl3x3)
STD_ROM_FN(puzzl3x3)

static INT32 Puzzl3x3Init()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvPuzzl3x3 = {
	"3x3puzzl", NULL, NULL, NULL, "1998",
	"3X3 Puzzle (Enterprise)\0", NULL, "Ace Enterprise", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, puzzl3x3RomInfo, puzzl3x3RomName, NULL, NULL, NULL, NULL, Puzzle3x3InputInfo, Puzzle3x3DIPInfo,
	Puzzl3x3Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	512, 240, 4, 3
};


// 3X3 Puzzle (Normal)

static struct BurnRomInfo puzzl3x3aRomDesc[] = {
	{ "1a.bin",		0x20000, 0x425c5896, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "2a.bin",		0x20000, 0x4db710b7, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "3a.bin",		0x80000, 0x33bff952, 2 | BRF_GRA },           //  2 Background Tiles
	{ "4a.bin",		0x80000, 0x222996d8, 2 | BRF_GRA },           //  3
	{ "5a.bin",		0x80000, 0x5b209844, 2 | BRF_GRA },           //  4
	{ "6a.bin",		0x80000, 0xf1136bba, 2 | BRF_GRA },           //  5

	{ "7.bin",		0x20000, 0x45b1f58b, 3 | BRF_GRA },           //  6 Midground Tiles
	{ "8.bin",		0x20000, 0xc0d404a7, 3 | BRF_GRA },           //  7
	{ "9.bin",		0x20000, 0x6b303aa9, 3 | BRF_GRA },           //  8
	{ "10.bin",		0x20000, 0x6d0107bc, 3 | BRF_GRA },           //  9

	{ "11.bin",		0x20000, 0xe124c0b5, 4 | BRF_GRA },           // 10 Foreground Tiles
	{ "12.bin",		0x20000, 0xae4a8707, 4 | BRF_GRA },           // 11
	{ "13.bin",		0x20000, 0xf06925d1, 4 | BRF_GRA },           // 12
	{ "14.bin",		0x20000, 0x07252636, 4 | BRF_GRA },           // 13

	{ "15.bin",		0x80000, 0xd3aff355, 5 | BRF_SND },           // 14 Samples
};

STD_ROM_PICK(puzzl3x3a)
STD_ROM_FN(puzzl3x3a)

struct BurnDriver BurnDrvPuzzl3x3a = {
	"3x3puzzla", "3x3puzzl", NULL, NULL, "1998",
	"3X3 Puzzle (Normal)\0", NULL, "Ace Enterprise", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, puzzl3x3aRomInfo, puzzl3x3aRomName, NULL, NULL, NULL, NULL, Puzzle3x3InputInfo, Puzzle3x3DIPInfo,
	Puzzl3x3Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	512, 240, 4, 3
};


// Casanova

static struct BurnRomInfo casanovaRomDesc[] = {
	{ "casanova.u8",	0x40000, 0x9df77f4b, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "casanova.u7",	0x40000, 0x869c2bf2, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "casanova.u23",	0x80000, 0x4bd4e5b1, 2 | BRF_GRA },           //  2 Background Tiles
	{ "casanova.u25",	0x80000, 0x5461811b, 2 | BRF_GRA },           //  3
	{ "casanova.u27",	0x80000, 0xdd178379, 2 | BRF_GRA },           //  4
	{ "casanova.u29",	0x80000, 0x36469f9e, 2 | BRF_GRA },           //  5
	{ "casanova.u81",	0x80000, 0x9eafd37d, 2 | BRF_GRA },           //  6
	{ "casanova.u83",	0x80000, 0x9d4ce407, 2 | BRF_GRA },           //  7
	{ "casanova.u85",	0x80000, 0x113c6e3a, 2 | BRF_GRA },           //  8
	{ "casanova.u87",	0x80000, 0x61bd80f8, 2 | BRF_GRA },           //  9

	{ "casanova.u45",	0x80000, 0x530d78bc, 3 | BRF_GRA },           // 10 Midground Tiles
	{ "casanova.u43",	0x80000, 0x1462d7d6, 3 | BRF_GRA },           // 11
	{ "casanova.u41",	0x80000, 0x95f67e82, 3 | BRF_GRA },           // 12
	{ "casanova.u39",	0x80000, 0x97d4095a, 3 | BRF_GRA },           // 13

	{ "casanova.u54",	0x80000, 0xe60bf0db, 4 | BRF_GRA },           // 14 Foreground Tiles
	{ "casanova.u52",	0x80000, 0x708f779c, 4 | BRF_GRA },           // 15
	{ "casanova.u50",	0x80000, 0xc73b5e98, 4 | BRF_GRA },           // 16
	{ "casanova.u48",	0x80000, 0xaf9f59c5, 4 | BRF_GRA },           // 17 

	{ "casanova.su2",	0x80000, 0x84a8320e, 5 | BRF_SND },           // 18 Samples
	{ "casanova.su3",	0x40000, 0x334a2d1a, 5 | BRF_SND },           // 19
};

STD_ROM_PICK(casanova)
STD_ROM_FN(casanova)

static INT32 CasanovaInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvCasanova = {
	"casanova", NULL, NULL, NULL, "199?",
	"Casanova\0", NULL, "Promat", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, casanovaRomInfo, casanovaRomName, NULL, NULL, NULL, NULL, CasanovaInputInfo, CasanovaDIPInfo,
	CasanovaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	512, 240, 4, 3
};
