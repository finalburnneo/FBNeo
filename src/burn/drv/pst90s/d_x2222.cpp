// FinalBurn Neo Oriental Soft X2222 Prototype Driver module
// Based on MAME driver by David Haywood

#include "tiles_generic.h"
#include "e132xs_intf.h"
#include "burn_pal.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvBootROM;
static UINT8 *DrvGfxROM[8];
static UINT8 *DrvNVRAM;
static UINT8 *DrvMainRAM;
static UINT8 *DrvVidRAM;

static INT32 scrollx[3];
static INT32 scrolly[3];
static INT32 flipper = 0;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT16 DrvInputs[3];
static UINT8 DrvDips[1];
static UINT8 DrvReset;

static INT32 nCyclesExtra;

static struct BurnInputInfo X2222InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(X2222)

static struct BurnDIPInfo X2222DIPList[]=
{
	{0x10, 0xff, 0xff, 0x0d, NULL					},

	{0   , 0xfe, 0   ,    2, "Free Play"			},
	{0x10, 0x01, 0x01, 0x01, "Off"					},
	{0x10, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x10, 0x01, 0x06, 0x00, "Easy"					},
	{0x10, 0x01, 0x06, 0x04, "Normal"				},
	{0x10, 0x01, 0x06, 0x02, "Hard"					},
	{0x10, 0x01, 0x06, 0x06, "Very Hard"			},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x10, 0x01, 0x18, 0x00, "1"					},
	{0x10, 0x01, 0x18, 0x08, "2"					},
	{0x10, 0x01, 0x18, 0x10, "3"					},
	{0x10, 0x01, 0x18, 0x18, "4"					},

	{0   , 0xfe, 0   ,    8, "Coin A"				},
	{0x10, 0x01, 0xe0, 0x20, "2 Coins 1 Credits"	},
	{0x10, 0x01, 0xe0, 0x00, "1 Coin  1 Credits"	},
	{0x10, 0x01, 0xe0, 0x40, "2"					},
	{0x10, 0x01, 0xe0, 0x60, "3"					},
	{0x10, 0x01, 0xe0, 0x80, "4"					},
	{0x10, 0x01, 0xe0, 0xa0, "5"					},
	{0x10, 0x01, 0xe0, 0xc0, "6"					},
	{0x10, 0x01, 0xe0, 0xe0, "7"					},
};

STDDIPINFO(X2222)

static void x2222_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x4fa00000:
			scrolly[2] = data;
		return;

		case 0x4fb00000:
			scrollx[2] = data;
		return;

		case 0x4fc00000:
			scrolly[1] = data;
		return;

		case 0x4fd00000:
			scrollx[1] = data;
		return;

		case 0x4fe00000:
			scrolly[0] = data;
		return;

		case 0x4ff00000:
			scrollx[0] = data;
		return;
	}
}

static inline void speedhack_callback(UINT32 address)
{
	if (address == 0x7ffac) {
		if (E132XSGetPC(0) == 0x22064) {
			E132XSBurnCycles(50);
		}
	}
	if (address == 0x84e3c) {
		if (E132XSGetPC(0) == 0x23f44) {
			E132XSBurnCycles(50);
		}
	}
}

static UINT32 x2222_read_long(UINT32 address)
{
	if (address < 0x400000) {
		speedhack_callback(address);
		UINT32 ret = *((UINT32*)(DrvMainRAM + address));
		return (ret << 16) | (ret >> 16);
	}

	return 0;
}

static UINT16 x2222_read_word(UINT32 address)
{
	if (address < 0x400000) {
		speedhack_callback(address);
		return *((UINT16*)(DrvMainRAM + address));
	}

	return 0;
}

static UINT8 x2222_read_byte(UINT32 address)
{
	if (address < 0x400000) {
		speedhack_callback(address);
		return DrvMainRAM[address^1];
	}

	return 0;
}

static void x2222_io_write(UINT32 address, UINT32 data)
{
	switch (address)
	{
		case 0x4028:
			// sound ? (only ever writes 0 & 80)
		return;
	}
}

static UINT32 x2222_io_read(UINT32 address)
{
	switch (address)
	{
		case 0x4000:
			return DrvInputs[0] + (DrvInputs[0] << 16);

		case 0x4004:
			return DrvInputs[1] + (DrvInputs[1] << 16);

		case 0x4008:
			return DrvInputs[2] + (DrvInputs[2] << 16);

		case 0x4010:
			return DrvDips[0] + (DrvDips[0] << 16);

		case 0x4034:
			flipper = flipper ^ 0x00800080;
			return flipper;
	}

	return 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	E132XSOpen(0);
	E132XSReset();
	E132XSClose();

	memset (scrollx, 0, sizeof(scrollx));
	memset (scrolly, 0, sizeof(scrolly));

	nCyclesExtra = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvBootROM		= Next; Next += 0x100000;

	DrvGfxROM[0]	= Next; Next += 0x800000;
	DrvGfxROM[1]	= Next; Next += 0x200000;
	DrvGfxROM[2]	= Next; Next += 0x200000;
	DrvGfxROM[3]	= Next; Next += 0x200000;
	DrvGfxROM[4]	= Next; Next += 0x800000;
	DrvGfxROM[5]	= Next; Next += 0x200000;
	DrvGfxROM[6]	= Next; Next += 0x200000;
	DrvGfxROM[7]	= Next; Next += 0x200000;

	DrvNVRAM		= Next; Next += 0x002000;

	BurnPalette		= (UINT32*)Next; Next += 0x10000 * sizeof(UINT32);

	AllRam			= Next;

	DrvMainRAM		= Next; Next += 0x400000;
	DrvVidRAM		= Next; Next += 0x004000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void rearrange_sprite_data()
{
	UINT8 *ROM = (UINT8*)BurnMalloc(0x1000000);
	UINT32 *NEW = (UINT32*)DrvGfxROM[0];
	UINT32 *NEW2 = (UINT32*)DrvGfxROM[4];

	for (INT32 i = 0; i < 8; i++)
	{
		BurnLoadRom(ROM + i * 0x200000, 2 + i, 1); // load roms
	}

	for (INT32 i = 0; i < 0x200000; i++)
	{
		NEW[i]  = (ROM[(i * 2) + 0xc00000] << 24) | (ROM[(i * 2) + 0x800000] << 16) | (ROM[(i * 2) + 0x400000] << 8) | (ROM[(i * 2) + 0x000000] << 0);
		NEW2[i] = (ROM[(i * 2) + 0xc00001] << 24) | (ROM[(i * 2) + 0x800001] << 16) | (ROM[(i * 2) + 0x400001] << 8) | (ROM[(i * 2) + 0x000001] << 0);
	}

	BurnFree(ROM);
}

static void rearrange_tile_data(INT32 gfx, INT32 dst0, INT32 dst1)
{
	UINT8 *ROM = (UINT8*)BurnMalloc(0x400000);
	UINT32 *NEW = (UINT32*)DrvGfxROM[dst0];
	UINT32 *NEW2 = (UINT32*)DrvGfxROM[dst1];

	BurnLoadRom(ROM + 0, gfx + 0, 2);
	BurnLoadRom(ROM + 1, gfx + 1, 2);

	for (INT32 i = 0; i < 0x80000; i++)
	{
		NEW[i]  = (ROM[(i * 8) + 0x000000] << 0) | (ROM[(i * 8) + 0x000001] << 8) | (ROM[(i * 8) + 0x000004] << 16) | (ROM[(i * 8) + 0x000005] << 24);
		NEW2[i] = (ROM[(i * 8) + 0x000002] << 0) | (ROM[(i * 8) + 0x000003] << 8) | (ROM[(i * 8) + 0x000006] << 16) | (ROM[(i * 8) + 0x000007] << 24);
	}

	BurnFree (ROM);
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRomExt(DrvBootROM, 0, 1, LD_BYTESWAP)) return 1;

//		rom 1 not used?

		rearrange_sprite_data();
		rearrange_tile_data(10+0, 0+1, 4+1);
		rearrange_tile_data(10+2, 0+2, 4+2);
		rearrange_tile_data(10+4, 0+3, 4+3);
	}

	E132XSInit(0, TYPE_E132XT, 64000000);
	E132XSOpen(0);
	E132XSMapMemory(DrvMainRAM,			0x00000000, 0x003fffff, MAP_RAM);
	E132XSMapMemory(DrvVidRAM,			0x80000000, 0x80003fff, MAP_RAM);
	E132XSMapMemory(DrvNVRAM,			0xffc00000, 0xffc01fff, MAP_RAM);
	E132XSMapMemory(DrvBootROM,			0xfff00000, 0xffffffff, MAP_ROM);
	E132XSSetWriteWordHandler(x2222_write_word);
	E132XSSetIOWriteHandler(x2222_io_write);
	E132XSSetIOReadHandler(x2222_io_read);

	// Speed hacks
	E132XSMapMemory(NULL,	0x7ffac & ~0xfff, 0x84e3c | 0xfff, MAP_ROM);
	E132XSSetReadLongHandler(x2222_read_long);
	E132XSSetReadWordHandler(x2222_read_word);
	E132XSSetReadByteHandler(x2222_read_byte);

	GenericTilesInit();
	GenericTilemapSetGfx(0, DrvGfxROM[1], 8, 32, 32, 0x200000, 0, 0);
	GenericTilemapSetGfx(1, DrvGfxROM[2], 8, 32, 32, 0x200000, 0, 0);
	GenericTilemapSetGfx(2, DrvGfxROM[3], 8, 32, 32, 0x200000, 0, 0);
	GenericTilemapSetGfx(3, DrvGfxROM[0], 8, 16, 16, 0x800000, 0, 0);
	GenericTilemapSetGfx(4, DrvGfxROM[4], 8, 16, 16, 0x800000, 0, 0);
	GenericTilemapSetGfx(5, DrvGfxROM[5], 8, 32, 32, 0x200000, 0, 0);
	GenericTilemapSetGfx(6, DrvGfxROM[6], 8, 32, 32, 0x200000, 0, 0);
	GenericTilemapSetGfx(7, DrvGfxROM[7], 8, 32, 32, 0x200000, 0, 0);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	E132XSExit();

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x10000; i++)	// BGR565
	{
		INT32 r = pal5bit((i >>  0) & 0x1f);
		INT32 g = (i >>  5) & 0x3f; // 6BIT
		INT32 b = pal5bit((i >> 11) & 0x1f);

		g = (g << 2) | (g >> 4);

		BurnPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_16bpp_tile(INT32 gfx0, INT32 gfx1, UINT32 code, INT32 sx, INT32 sy)
{
	GenericTilesGfx *pGfx0 = &GenericGfxData[gfx0];
	GenericTilesGfx *pGfx1 = &GenericGfxData[gfx1];

	UINT8 *gfxbase0 = pGfx0->gfxbase + ((code % pGfx0->code_mask) * pGfx0->width * pGfx0->height);
	UINT8 *gfxbase1 = pGfx1->gfxbase + ((code % pGfx1->code_mask) * pGfx1->width * pGfx1->height);

	for (INT32 y = 0; y < pGfx0->height; y++)
	{
		for (INT32 x = 0; x < pGfx0->width; x++)
		{
			UINT16 pixel = gfxbase0[(y * pGfx0->width) + x] + (gfxbase1[(y * pGfx1->width) + x] << 8);

			if (pixel)
			{
				INT32 xx = sx + x;
				INT32 yy = sy + y;

				if (xx >= 0 && xx < nScreenWidth && yy >= 0 && yy < nScreenHeight)
				{
					pTransDraw[yy * nScreenWidth + xx] = pixel;
				}
			}
		}
	}
}

static void draw_bg(INT32 map, UINT32 *ram)
{
	INT32 xscroll = scrollx[map] & 0x1ff;
	INT32 yscroll = scrolly[map] & 0x1ff;
	INT32 basey = yscroll >> 5;
	INT32 basex = xscroll >> 5;

	for (INT32 y=0; y < 13; y++, basey++)
	{
		for (INT32 x = 0; x < 16; x++, basex++)
		{
			INT32 code = ((ram[(basex&0x0f)+((basey&0x0f)*0x10)]) >> 0) & 0x0fff;

			draw_16bpp_tile(map, map + 5, code, (x*32)-(xscroll&0x1f), (y*32)-(yscroll&0x1f));
		}
	}
}

static INT32 DrvDraw()
{
	if (BurnRecalc) {
		DrvPaletteInit();
		BurnRecalc = 0;
	}

	BurnTransferClear();

	UINT32 *vram = (UINT32*)DrvVidRAM;

	draw_bg(2, vram + 0x800/4);
	draw_bg(1, vram + 0x400/4);
	draw_bg(0, vram + 0x000/4); 

	for (INT32 i = 0x0000 / 4; i < 0x4000 / 4; i += 4)
	{
		INT32 code = (vram[i + 0] >> 16) & 0xffff;
		INT32 sx   = (vram[i + 1] >> 16) & 0x01ff;
		INT32 sy   = (vram[i + 2] >> 16) & 0x00ff;

		draw_16bpp_tile(3, 4, code, sx, sy);
		draw_16bpp_tile(3, 4, code, sx, sy - 256);
		draw_16bpp_tile(3, 4, code, sx - 512, sy);
		draw_16bpp_tile(3, 4, code, sx - 512, sy - 256);
	}

	BurnTransferCopy(BurnPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	E132XSNewFrame();

	E132XSOpen(0);
	E132XSRun((64000000 / 60) - nCyclesExtra);
	E132XSSetIRQLine(0, CPU_IRQSTATUS_HOLD);
	nCyclesExtra = E132XSTotalCycles() - (64000000 / 60);
	E132XSClose();

	if (pBurnSoundOut) {


	}

	if (pBurnDraw) {
		BurnDrvRedraw();
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
		E132XSScan(nAction);

		SCAN_VAR(scrollx);
		SCAN_VAR(scrolly);

		SCAN_VAR(nCyclesExtra);
	}

	if (nAction & ACB_NVRAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = DrvNVRAM;
		ba.nLen	  = 0x2000;
		ba.szName = "NV Ram";
		BurnAcb(&ba);
	}

	return 0;
}


// X2222 (final debug?)

static struct BurnRomInfo x2222RomDesc[] = {
	{ "test.bin",		0x100000, 0x6260421e, 1 | BRF_PRG | BRF_ESS }, //  0 E132XT Code

	{ "test.hye",		0x112dda, 0xc1142b2f, 2 | BRF_PRG | BRF_ESS }, //  1 Misc. Code?

	{ "spr11.bin",		0x200000, 0x1d15b444, 3 | BRF_GRA },           //  2 Sprites
	{ "spr21.bin",		0x1b8b00, 0x1c392be2, 3 | BRF_GRA },           //  3
	{ "spr12.bin",		0x200000, 0x73225936, 3 | BRF_GRA },           //  4
	{ "spr22.bin",		0x1b8b00, 0xcf7ebfa1, 3 | BRF_GRA },           //  5
	{ "spr13.bin",		0x200000, 0x52595c51, 3 | BRF_GRA },           //  6
	{ "spr23.bin",		0x1b8b00, 0xd894461e, 3 | BRF_GRA },           //  7
	{ "spr14.bin",		0x200000, 0xf6cd6599, 3 | BRF_GRA },           //  8
	{ "spr24.bin",		0x1b8b00, 0x9542cb08, 3 | BRF_GRA },           //  9

	{ "bg31.bin",		0x11ac00, 0x12e67bc2, 4 | BRF_GRA },           // 10 Background Graphics (Layer 0)
	{ "bg32.bin",		0x11ac00, 0x95afa0da, 4 | BRF_GRA },           // 11

	{ "bg21.bin",		0x1c8400, 0xa10220f8, 5 | BRF_GRA },           // 12 Background Graphics (Layer 1)
	{ "bg22.bin",		0x1c8400, 0x966f7c1d, 5 | BRF_GRA },           // 13

	{ "bg11.bin",		0x1bc800, 0x68975462, 6 | BRF_GRA },           // 14 Background Graphics (Layer 2)
	{ "bg12.bin",		0x1bc800, 0xfeef1240, 6 | BRF_GRA },           // 15

	{ "x2222_sound",	0x080000, 0x00000000, 7 | BRF_SND | BRF_NODUMP },           // 16 Samples
};

STD_ROM_PICK(x2222)
STD_ROM_FN(x2222)

struct BurnDriver BurnDrvX2222 = {
	"x2222", NULL, NULL, NULL, "2000",
	"X2222 (final debug?)\0", "Prototype - No Sound, Incomplete game", "Oriental Soft / Promat", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_PROTOTYPE, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, x2222RomInfo, x2222RomName, NULL, NULL, NULL, NULL, X2222InputInfo, X2222DIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x10000,
	240, 320, 3, 4
};


// X2222 (5-level prototype)

static struct BurnRomInfo x2222oRomDesc[] = {
	{ "older.bin",		0x080000, 0xd12817bc, 1 | BRF_PRG | BRF_ESS }, //  0 E132XT Code

	{ "older.hye",		0x10892f, 0xcf3a004e, 2 | BRF_PRG | BRF_ESS }, //  1 Misc. Code?

	{ "spr11.bin",		0x200000, 0x1d15b444, 3 | BRF_GRA },           //  2 Sprites
	{ "spr21.bin",		0x1b8b00, 0x1c392be2, 3 | BRF_GRA },           //  3
	{ "spr12.bin",		0x200000, 0x73225936, 3 | BRF_GRA },           //  4
	{ "spr22.bin",		0x1b8b00, 0xcf7ebfa1, 3 | BRF_GRA },           //  5
	{ "spr13.bin",		0x200000, 0x52595c51, 3 | BRF_GRA },           //  6
	{ "spr23.bin",		0x1b8b00, 0xd894461e, 3 | BRF_GRA },           //  7
	{ "spr14.bin",		0x200000, 0xf6cd6599, 3 | BRF_GRA },           //  8
	{ "spr24.bin",		0x1b8b00, 0x9542cb08, 3 | BRF_GRA },           //  9

	{ "bg31.bin",		0x11ac00, 0x12e67bc2, 4 | BRF_GRA },           // 10 Background Graphics (Layer 0)
	{ "bg32.bin",		0x11ac00, 0x95afa0da, 4 | BRF_GRA },           // 11

	{ "bg21.bin",		0x1c8400, 0xa10220f8, 5 | BRF_GRA },           // 12 Background Graphics (Layer 1)
	{ "bg22.bin",		0x1c8400, 0x966f7c1d, 5 | BRF_GRA },           // 13

	{ "bg11.bin",		0x1bc800, 0x68975462, 6 | BRF_GRA },           // 14 Background Graphics (Layer 2)
	{ "bg12.bin",		0x1bc800, 0xfeef1240, 6 | BRF_GRA },           // 15

	{ "x2222_sound",	0x080000, 0x00000000, 7 | BRF_SND | BRF_NODUMP },           // 16 Samples
};

STD_ROM_PICK(x2222o)
STD_ROM_FN(x2222o)

static INT32 x2222oInit()
{
	INT32 nRet = DrvInit();

	if (nRet == 0) {
		memcpy (DrvBootROM + 0x80000, DrvBootROM, 0x80000);
	}

	return nRet;
}

struct BurnDriver BurnDrvX2222o = {
	"x2222o", "x2222", NULL, NULL, "2000",
	"X2222 (5-level prototype)\0", "Prototype - No Sound, Incomplete game", "Oriental Soft / Promat", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_PROTOTYPE, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, x2222oRomInfo, x2222oRomName, NULL, NULL, NULL, NULL, X2222InputInfo, X2222DIPInfo,
	x2222oInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x10000,
	240, 320, 3, 4
};
