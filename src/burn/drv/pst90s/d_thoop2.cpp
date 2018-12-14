// FB Alpha Thunder Hoop II: TH Strikes Back driver module
// Based on MAME driver by Manuel Abadia, Peter Ferrie, and David Haywood

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "msm6295.h"
#include "burn_pal.h"
#include "watchdog.h"
#include "mcs51.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvMCUROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvSndROM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvShareRAM;
static UINT8 *DrvMCURAM;
static UINT16 *DrvVidRegs;
static UINT8 *DrvTransTab[3];

static UINT8 DrvRecalc;

static UINT8 oki_bank;

static INT32 sprite_count[5];
static INT32 sprite_table[5][512];
static INT32 transparent_select;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo Thoop2InputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy3 + 0,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy3 + 1,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Thoop2)

static struct BurnDIPInfo Thoop2DIPList[]=
{
	{0x15, 0xff, 0xff, 0xff, NULL			},
	{0x16, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x15, 0x01, 0x07, 0x02, "6 Coins 1 Credits"	},
	{0x15, 0x01, 0x07, 0x03, "5 Coins 1 Credits"	},
	{0x15, 0x01, 0x07, 0x04, "4 Coins 1 Credits"	},
	{0x15, 0x01, 0x07, 0x05, "3 Coins 1 Credits"	},
	{0x15, 0x01, 0x07, 0x06, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0x07, 0x01, "3 Coins 2 Credits"	},
	{0x15, 0x01, 0x07, 0x00, "4 Coins 3 Credits"	},
	{0x15, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x15, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},
	{0x15, 0x01, 0x38, 0x00, "3 Coins 4 Credits"	},
	{0x15, 0x01, 0x38, 0x08, "2 Coins 3 Credits"	},
	{0x15, 0x01, 0x38, 0x30, "1 Coin  2 Credits"	},
	{0x15, 0x01, 0x38, 0x28, "1 Coin  3 Credits"	},
	{0x15, 0x01, 0x38, 0x20, "1 Coin  4 Credits"	},
	{0x15, 0x01, 0x38, 0x18, "1 Coin  5 Credits"	},
	{0x15, 0x01, 0x38, 0x10, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Credit configuration"	},
	{0x15, 0x01, 0x40, 0x40, "Start 1C/Continue 1C"	},
	{0x15, 0x01, 0x40, 0x00, "Start 2C/Continue 1C"	},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x15, 0x01, 0x80, 0x80, "Off"			},
	{0x15, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x16, 0x01, 0x03, 0x03, "Normal"		},
	{0x16, 0x01, 0x03, 0x02, "Easy"			},
	{0x16, 0x01, 0x03, 0x01, "Hard"			},
	{0x16, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x16, 0x01, 0x04, 0x04, "Off"			},
	{0x16, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x16, 0x01, 0x18, 0x18, "2"			},
	{0x16, 0x01, 0x18, 0x10, "3"			},
	{0x16, 0x01, 0x18, 0x08, "4"			},
	{0x16, 0x01, 0x18, 0x00, "1"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x16, 0x01, 0x20, 0x00, "Off"			},
	{0x16, 0x01, 0x20, 0x20, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x16, 0x01, 0x80, 0x80, "Off"			},
	{0x16, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Thoop2)

static inline void set_oki_bank(INT32 data)
{
	oki_bank = data & 0xf;
	MSM6295SetBank(0, DrvSndROM + oki_bank * 0x10000, 0x30000, 0x3ffff);
}

static void __fastcall thoop2_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x108000:
		case 0x108002:
		case 0x108004:
		case 0x108006:
			DrvVidRegs[(address/2) & 3] = data;
		return;

		case 0x10800c:
			BurnWatchdogWrite();
		return;
	}
}

static void __fastcall thoop2_main_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x70000d:
			set_oki_bank(data);
		return;

		case 0x70000f:
			MSM6295Write(0, data);
		return;
	}
}

static UINT16 __fastcall thoop2_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x700004:
		case 0x700006:
		case 0x700008:
			return DrvInputs[(address - 0x700004)/2];
	}

	return 0;
}

static UINT8 __fastcall thoop2_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x700001:
		case 0x700003:
			return DrvDips[((address/2) & 1)^1];

		case 0x70000f:
			return MSM6295Read(0);
	}

	return 0;
}

static void __fastcall thoop2_palette_write_word(UINT32 address, UINT16 data)
{
	address &= 0x7fe;

	*((UINT16*)(BurnPalRAM + address)) = data;

	BurnPaletteWrite_xBBBBBGGGGGRRRRR(address);
}

static void __fastcall thoop2_palette_write_byte(UINT32 address, UINT8 data)
{
	address &= 0x7ff;

	BurnPalRAM[address ^ 1] = data;

	BurnPaletteWrite_xBBBBBGGGGGRRRRR(address);
}

static void dallas_sharedram_write(INT32 address, UINT8 data)
{
	if (address >= MCS51_PORT_P0) return;

	if (address >= 0x8000 && address <= 0xffff)
		DrvShareRAM[(address & 0x7fff) ^ 1] = data;

	if (address >= 0x10000 && address <= 0x17fff)
		DrvMCURAM[address & 0x7fff] = data;
}

static UINT8 dallas_sharedram_read(INT32 address)
{
	if (address >= MCS51_PORT_P0) return 0;

	if (address >= 0x8000 && address <= 0xffff)
		return DrvShareRAM[(address & 0x7fff) ^ 1];

	if (address >= 0x10000 && address <= 0x17fff)
		return DrvMCURAM[address & 0x7fff];

	return 0;
}

static tilemap_scan( thoop2 )
{
	// 16x16 tiles 32x32 to 8x8 tiles 64x64
	return (((row / 2) * 32) + (col / 2)) * 4 + ((col & 1) * 2) + (row & 1);
}

static tilemap_callback( screen0 )
{
	UINT16 *ram = (UINT16*)(DrvVidRAM + (offs & ~3));

	UINT16 data = ram[0];
	UINT16 attr = ram[1];

	UINT32 code = (((data & 0x03) << 14) | ((data & 0xfffc) >> 2)) << 2;
	code += (offs & 3) ^ TILE_FLIPXY(attr >> 14); // xy flipped?

	INT32 flags = TILE_GROUP((attr >> 6) & 3) | TILE_FLIPYX(attr >> 14);
	flags |= DrvTransTab[transparent_select][code] ? TILE_SKIP : 0;

	TILE_SET_INFO(0, code, attr, flags);
}

static tilemap_callback( screen1 )
{
	UINT16 *ram = (UINT16*)(DrvVidRAM + 0x1000 + (offs & ~3));

	UINT16 data = ram[0];
	UINT16 attr = ram[1];

	UINT32 code = (((data & 0x03) << 14) | ((data & 0xfffc) >> 2)) << 2;
	code += (offs & 3) ^ TILE_FLIPXY(attr >> 14); // xy flipped??

	INT32 flags = TILE_GROUP((attr >> 6) & 3) | TILE_FLIPYX(attr >> 14);
	flags |= DrvTransTab[transparent_select][code] ? TILE_SKIP : 0;

	TILE_SET_INFO(0, code, attr, flags);
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	SekOpen(0);
	SekReset();
	SekClose();

	mcs51_reset();

	MSM6295Reset(0);

	BurnWatchdogReset();

	set_oki_bank(3);

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM	= Next; Next += 0x100000;

	DrvMCUROM	= Next; Next += 0x008000;

	DrvGfxROM	= Next; Next += 0x1000000;

	MSM6295ROM	= Next;
	DrvSndROM	= Next; Next += 0x100000;

	DrvTransTab[0]	= Next; Next += 0x1000000 / 0x40;
	DrvTransTab[1]	= Next; Next += 0x1000000 / 0x40;
	DrvTransTab[2]	= Next; Next += 0x1000000 / 0x40;

	BurnPalette	= (UINT32*)Next; Next += 0x0400 * sizeof(UINT32);

	AllRam		= Next;

	BurnPalRAM	= Next; Next += 0x000800;
	DrvVidRAM	= Next; Next += 0x002000;
	Drv68KRAM	= Next; Next += 0x008000;
	DrvSprRAM	= Next; Next += 0x001000;
	DrvShareRAM	= Next; Next += 0x008000;

	DrvVidRegs	= (UINT16*)Next; Next += 0x000004 * sizeof(UINT16);

	RamEnd		= Next;

	DrvMCURAM	= Next; Next += 0x008000; // NV RAM

	MemEnd		= Next;

	return 0;
}

static void DrvTransTableInit()
{
	INT32 tile_size = 8 * 8;
	UINT16 mask[3] = { 0xff01, 0x00ff, 0x0001 };

	for (INT32 i = 0; i < 0x1000000; i+= tile_size)
	{
		for (INT32 k = 0; k < 3; k++)
		{
			DrvTransTab[k][i/tile_size] = 1;

			for (INT32 j = 0; j < tile_size; j++)
			{
				if ((mask[k] & (1 << DrvGfxROM[i+j])) == 0) {
					DrvTransTab[k][i / tile_size] = 0;
					break;
				}
			}
		}
	}
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[4]  = { 0*0x400000*8+8, 0*0x400000*8, 1*0x400000*8+8, 1*0x400000*8 };
	INT32 XOffs[16] = { STEP8(0,1), STEP8(256,1) };
	INT32 YOffs[16] = { STEP16(0,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x800000);

	memcpy (tmp, DrvGfxROM, 0x800000);

	GfxDecode(0x40000, 4, 8, 8, Plane, XOffs, YOffs, 0x080, tmp, DrvGfxROM);

	BurnFree(tmp);

	return 0;
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
		if (BurnLoadRom(Drv68KROM + 0x0000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x0000000,  1, 2)) return 1;

		if (BurnLoadRom(DrvMCUROM + 0x0000000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM + 0x0000000,  3, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x0400000,  4, 1)) return 1;

		if (BurnLoadRom(DrvSndROM + 0x0000000,  5, 1)) return 1;

		DrvGfxDecode();
		DrvTransTableInit();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(DrvVidRAM,		0x100000, 0x101fff, MAP_RAM);
	SekMapMemory(BurnPalRAM,	0x200000, 0x2007ff, MAP_RAM);
	SekMapMemory(DrvSprRAM,		0x440000, 0x440fff, MAP_RAM);
	SekMapMemory(Drv68KRAM,		0xfe0000, 0xfe7fff, MAP_RAM);
	SekMapMemory(DrvShareRAM,	0xfe8000, 0xfeffff, MAP_RAM);
	SekSetWriteWordHandler(0,	thoop2_main_write_word);
	SekSetWriteByteHandler(0,	thoop2_main_write_byte);
	SekSetReadWordHandler(0,	thoop2_main_read_word);
	SekSetReadByteHandler(0,	thoop2_main_read_byte);

	SekMapHandler(1,		0x200000, 0x2007ff, MAP_WRITE);
	SekSetWriteWordHandler(1,	thoop2_palette_write_word);
	SekSetWriteByteHandler(1,	thoop2_palette_write_byte);
	SekClose();

	mcs51_program_data = DrvMCUROM;
	ds5002fp_init(0x79, 0x00, 0x80);
	mcs51_set_write_handler(dallas_sharedram_write);
	mcs51_set_read_handler(dallas_sharedram_read);

	MSM6295Init(0, 1056000 / 132, 0);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	BurnWatchdogInit(DrvDoReset, 180);

	GenericTilesInit();
	GenericTilemapInit(0, thoop2_map_scan, screen0_map_callback, 8, 8, 64, 64);
	GenericTilemapInit(1, thoop2_map_scan, screen1_map_callback, 8, 8, 64, 64);
	GenericTilemapSetGfx(0, DrvGfxROM, 4, 8, 8, 0x800000*2, 0, 0x3f);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	MSM6295Exit(0);
	MSM6295ROM = NULL;

	SekExit();

	mcs51_exit();

	BurnFree (AllMem);

	return 0;
}

static void sort_sprites()
{
	UINT16 *spriteram = (UINT16*)DrvSprRAM;

	sprite_count[0] = 0;
	sprite_count[1] = 0;
	sprite_count[2] = 0;
	sprite_count[3] = 0;
	sprite_count[4] = 0;

	for (INT32 i = 3; i < (0x1000 - 6)/2; i += 4){
		INT32 color = (spriteram[i+2] & 0x7e00) >> 9;
		INT32 priority = (spriteram[i] & 0x3000) >> 12;

		if (color >= 0x38){
			sprite_table[4][sprite_count[4]] = i;
			sprite_count[4]++;
		}

		sprite_table[priority][sprite_count[priority]] = i;
		sprite_count[priority]++;
	}
}

static void draw_sprites(int pri)
{
	UINT16 *spriteram = (UINT16*)DrvSprRAM;

	static const INT32 x_offset[2] = {0x0,0x2};
	static const INT32 y_offset[2] = {0x0,0x1};

	for (INT32 j = 0; j < sprite_count[pri]; j++)
	{
		INT32 i = sprite_table[pri][j];
		INT32 sx = spriteram[i+2] & 0x01ff;
		INT32 sy = (240 - (spriteram[i] & 0x00ff)) & 0x00ff;
		INT32 number = spriteram[i+3];
		INT32 color = (spriteram[i+2] & 0x7e00) >> 9;
		INT32 attr = (spriteram[i] & 0xfe00) >> 9;

		INT32 xflip = attr & 0x20;
		INT32 yflip = attr & 0x40;
		INT32 spr_size;

		number |= ((number & 0x03) << 16);

		if (attr & 0x04){
			spr_size = 1;
		} else {
			spr_size = 2;
			number &= (~3);
		}

		for (INT32 y = 0; y < spr_size; y++){
			for (INT32 x = 0; x < spr_size; x++){
				INT32 ex = xflip ? (spr_size-1-x) : x;
				INT32 ey = yflip ? (spr_size-1-y) : y;

				INT32 code = number + x_offset[ex] + y_offset[ey];

				if (DrvTransTab[2][code]) continue;

				INT32 sxx = sx-0x0f+x*8;
				INT32 syy = (sy+y*8)-16;

				if (sxx < -15 || sxx >= nScreenWidth || syy < -15 || syy >= nScreenHeight) continue;

				if (yflip) {
					if (xflip) {
						Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, code, sxx, syy, color, 4, 0, 0, DrvGfxROM);
					} else {
						Render8x8Tile_Mask_FlipY_Clip(pTransDraw, code, sxx, syy, color, 4, 0, 0, DrvGfxROM);
					}
				} else {
					if (xflip) {
						Render8x8Tile_Mask_FlipX_Clip(pTransDraw, code, sxx, syy, color, 4, 0, 0, DrvGfxROM);
					} else {
						Render8x8Tile_Mask_Clip(pTransDraw, code, sxx, syy, color, 4, 0, 0, DrvGfxROM);
					}
				}
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		BurnPaletteUpdate_xBBBBBGGGGGRRRRR();
		DrvRecalc = 0;
	}

	GenericTilemapSetScrollY(0, DrvVidRegs[0]);
	GenericTilemapSetScrollX(0, DrvVidRegs[1] + 4);
	GenericTilemapSetScrollY(1, DrvVidRegs[2]);
	GenericTilemapSetScrollX(1, DrvVidRegs[3]);

	BurnTransferClear();

	sort_sprites();

	for (INT32 priority = 3; priority >= 0; priority--)
	{
		transparent_select = 1;
		GenericTilemapSetTransMask(1, 0, 0x00ff);
		if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, TMAP_SET_GROUP(priority));

		GenericTilemapSetTransMask(0, 0, 0x00ff);
		if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, TMAP_SET_GROUP(priority));

		draw_sprites(priority);

		transparent_select = 0;
		GenericTilemapSetTransMask(1, 0, 0xff01);
		if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, TMAP_SET_GROUP(priority));

		GenericTilemapSetTransMask(0, 0, 0xff01);
		if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, TMAP_SET_GROUP(priority));
	}

	draw_sprites(4);

	BurnTransferCopy(BurnPalette);

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	SekNewFrame();

	{
		memset (DrvInputs, 0xff, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 12000000 / 60, 12000000 / 12 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	SekOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		INT32 nNext = (i + 1) * nCyclesTotal[0] / nInterleave;
		nCyclesDone[0] += SekRun(nNext - nCyclesDone[0]);

		if (i == 232) SekSetIRQLine(6, CPU_IRQSTATUS_AUTO);

		nCyclesDone[1] += mcs51Run((SekTotalCycles() / 12) - nCyclesDone[1]);
	}

	SekClose();

	if (pBurnSoundOut) {
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		DrvDraw();
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
		ba.Data		= AllRam;
		ba.nLen		= RamEnd - AllRam;
		ba.nAddress	= 0;
		ba.szName	= "All RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_NVRAM) {
		ba.Data		= DrvMCURAM;
		ba.nLen		= 0x008000;
		ba.nAddress	= 0;
		ba.szName	= "MCU RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);

		MSM6295Scan(nAction, pnMin);

		mcs51_scan(nAction);

		BurnWatchdogScan(nAction);

		SCAN_VAR(oki_bank);
	}

	if (nAction & ACB_WRITE) {
		set_oki_bank(oki_bank);
	}

	return 0;
}


// TH Strikes Back (Non North America, Version 1.0, Checksum 020E0867)
/* REF.940411 PCB */

static struct BurnRomInfo thoop2RomDesc[] = {
	{ "th2c23.c23",				0x080000, 0x3e465753, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "th2c22.c22",				0x080000, 0x837205b7, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "thoop2_ds5002fp.bin",	0x008000, 0x6881384d, 2 | BRF_PRG | BRF_ESS }, //  2 MCU Code

	{ "th2-h8.h8",				0x400000, 0x60328a11, 3 | BRF_GRA },           //  3 Graphics
	{ "th2-h12.h12",			0x400000, 0xb25c2d3e, 3 | BRF_GRA },           //  4

	{ "th2-c1.c1",				0x100000, 0x8fac8c30, 4 | BRF_SND },           //  5 Samples
};

STD_ROM_PICK(thoop2)
STD_ROM_FN(thoop2)

struct BurnDriver BurnDrvThoop2 = {
	"thoop2", NULL, NULL, NULL, "1994",
	"TH Strikes Back (Non North America, Version 1.0, Checksum 020E0867)\0", NULL, "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_RUNGUN, 0,
	NULL, thoop2RomInfo, thoop2RomName, NULL, NULL, NULL, NULL, Thoop2InputInfo, Thoop2DIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};


// TH Strikes Back (Non North America, Version 1.0, Checksum 020EB356)
/* REF.940411 PCB */

static struct BurnRomInfo thoop2aRomDesc[] = {
	{ "3.c23",					0x080000, 0x6cd4a8dc, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "2.c22",					0x080000, 0x59ba9b43, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "thoop2_ds5002fp.bin",	0x008000, 0x6881384d, 2 | BRF_PRG | BRF_ESS }, //  2 MCU Code

	{ "th2-h8.h8",				0x400000, 0x60328a11, 3 | BRF_GRA },           //  3 Graphics
	{ "th2-h12.h12",			0x400000, 0xb25c2d3e, 3 | BRF_GRA },           //  4

	{ "th2-c1.c1",				0x100000, 0x8fac8c30, 4 | BRF_SND },           //  5 Samples
};

STD_ROM_PICK(thoop2a)
STD_ROM_FN(thoop2a)

struct BurnDriver BurnDrvThoop2a = {
	"thoop2a", "thoop2", NULL, NULL, "1994",
	"TH Strikes Back (Non North America, Version 1.0, Checksum 020EB356)\0", NULL, "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_RUNGUN, 0,
	NULL, thoop2aRomInfo, thoop2aRomName, NULL, NULL, NULL, NULL, Thoop2InputInfo, Thoop2DIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};
