// FB Alpha World Rally driver module
// Based on MAME driver by Manuel Abadia, Mike Coates, Nicola Salmoria, and Miguel Angel Horna

// to do
//	hook up analog inputs (or not, since it has joystick settings)
//		note these are disabled in the dips completely!

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "mcs51.h"
#include "msm6295.h"
#include "gaelco_crypt.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvMCUROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvTransTab[2];
static UINT8 *DrvSndROM;
static UINT8 *DrvShareRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvVRegs;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 okibank;
static UINT8 flipscreen;
static UINT8 coin_lockout[2];

static INT32 transparent_select;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvDips[2];
static UINT8 DrvReset;
static UINT16 DrvInputs[2];

static struct BurnInputInfo WrallyInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 14,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 1"	},
	{"P1 Gear Shift",	BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 2"	},

	// analog input placeholder...
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 4"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 15,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 10,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy2 + 0,	"service"	},
	{"Service Mode",		BIT_DIGITAL,	DrvJoy2 + 1,	"diagnostics"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Wrally)

static struct BurnDIPInfo WrallyDIPList[]=
{
	{0x15, 0xff, 0xff, 0xdf, NULL			},
	{0x16, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x15, 0x01, 0x03, 0x03, "Normal"		},
	{0x15, 0x01, 0x03, 0x02, "Easy"			},
	{0x15, 0x01, 0x03, 0x01, "Hard"			},
	{0x15, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Number of Joysticks"	},
	{0x15, 0x01, 0x04, 0x04, "2"			},
	{0x15, 0x01, 0x04, 0x00, "1"			},

	{0   , 0xfe, 0   ,    1, "Control Configuration"},
//	digital only for now!
	{0x15, 0x01, 0x18, 0x18, "Joystick"		},
//	{0x15, 0x01, 0x18, 0x10, "Pot Wheel"		},
//	{0x15, 0x01, 0x18, 0x00, "Optical Wheel"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x15, 0x01, 0x20, 0x20, "Off"			},
	{0x15, 0x01, 0x20, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x15, 0x01, 0x40, 0x40, "Upright"		},
	{0x15, 0x01, 0x40, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x15, 0x01, 0x80, 0x80, "Off"			},
	{0x15, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x16, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x16, 0x01, 0x07, 0x00, "3 Coins 4 Credits"	},
	{0x16, 0x01, 0x07, 0x01, "2 Coins 3 Credits"	},
	{0x16, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x16, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x16, 0x01, 0x07, 0x04, "1 Coin  4 Credits"	},
	{0x16, 0x01, 0x07, 0x03, "1 Coin  5 Credits"	},
	{0x16, 0x01, 0x07, 0x02, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x16, 0x01, 0x38, 0x10, "6 Coins 1 Credits"	},
	{0x16, 0x01, 0x38, 0x18, "5 Coins 1 Credits"	},
	{0x16, 0x01, 0x38, 0x20, "4 Coins 1 Credits"	},
	{0x16, 0x01, 0x38, 0x28, "3 Coins 1 Credits"	},
	{0x16, 0x01, 0x38, 0x30, "2 Coins 1 Credits"	},
	{0x16, 0x01, 0x38, 0x08, "3 Coins 2 Credits"	},
	{0x16, 0x01, 0x38, 0x00, "4 Coins 3 Credits"	},
	{0x16, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    2, "Credit configuration"	},
	{0x16, 0x01, 0x40, 0x40, "Start 1C/Continue 1C"	},
	{0x16, 0x01, 0x40, 0x00, "Start 2C/Continue 1C"	},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x16, 0x01, 0x80, 0x80, "Off"			},
	{0x16, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Wrally)

static void oki_bankswitch(INT32 data)
{
	okibank = data;

	MSM6295SetBank(0, DrvSndROM + (data & 0x0f) * 0x10000, 0x30000, 0x3ffff);
}

static void __fastcall wrally_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfffc000) == 0x100000) {
		*((UINT16*)(DrvVidRAM + (address & 0x3ffe))) = gaelco_decrypt((address & 0x3ffe)/2, data, 0x1f, 0x522a);
		return;
	}

	switch (address)
	{
		case 0x108000:
		case 0x108002:
		case 0x108004:
		case 0x108006:
			*((UINT16*)(DrvVRegs + (address & 0x6))) = data;
		return;

		case 0x70000a:
		case 0x70001a:
			coin_lockout[(address >> 3) & 1] = ~data & 1;
		return;

		case 0x70002a:
		case 0x70003a:
			// coin counter
		return;

		case 0x70005a:
			flipscreen = data & 0x01;
		return;
	}
}

static void __fastcall wrally_main_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x70000d:
			oki_bankswitch(data);
		return;

		case 0x70000f:
			MSM6295Command(0, data);
		return;
	}

	bprintf (0, _T("Write byte: %5.5x, %2.2x\n"), address, data);
}

static UINT16 __fastcall wrally_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x700000:
			return (DrvDips[1] << 8) | (DrvDips[0]);

		case 0x700002:
			return DrvInputs[0];

		case 0x700004:
			return 0; // analog

		case 0x700008:
			return DrvInputs[1];

		case 0x70000e:
			return MSM6295ReadStatus(0);
	}

	return 0;
}

static UINT8 __fastcall wrally_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x700000:
		case 0x700001:
			return (DrvDips[(address & 1) ^ 1]);

		case 0x700002:
			return DrvInputs[0] >> 8;

		case 0x700003:
			return DrvInputs[0];

		case 0x700004:
		case 0x700005:
			return 0; // analog

		case 0x700008:
			return DrvInputs[1] >> 8;

		case 0x700009:
			return DrvInputs[1];

		case 0x70000e:
		case 0x70000f:
			return MSM6295ReadStatus(0);
	}

	return 0;
}

static void dallas_sharedram_write(INT32 address, UINT8 data)
{
	if (address >= MCS51_PORT_P0) return;

	DrvShareRAM[(address & 0x3fff) ^ 1] = data;
}

static UINT8 dallas_sharedram_read(INT32 address)
{
	if (address >= MCS51_PORT_P0) return 0;

	return DrvShareRAM[(address & 0x3fff) ^ 1];
}

static tilemap_callback( screen0 )
{
	UINT16 *ram = (UINT16*)(DrvVidRAM + 0x0000 + offs * 4);

	INT32 flags = TILE_FLIPYX(ram[1] >> 6) | TILE_GROUP((ram[1] >> 5) & 1);

	flags |= DrvTransTab[transparent_select][ram[0]&0x3fff] ? TILE_SKIP : 0;

	TILE_SET_INFO(0, ram[0], ram[1] & 0x1f, flags);
}

static tilemap_callback( screen1 )
{
	UINT16 *ram = (UINT16*)(DrvVidRAM + 0x2000 + offs * 4);

	TILE_SET_INFO(0, ram[0], ram[1] & 0x1f, TILE_FLIPYX(ram[1] >> 6) | TILE_GROUP((ram[1] >> 5) & 1));
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	mcs51_reset();

	MSM6295Reset(0);

	oki_bankswitch(0);

	flipscreen = 0;
	coin_lockout[0] = 0;
	coin_lockout[1] = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM	= Next; Next += 0x100000;
	DrvMCUROM	= Next; Next += 0x008000;

	DrvGfxROM	= Next; Next += 0x400000;
	DrvTransTab[0]	= Next; Next += 0x004000;
	DrvTransTab[1]	= Next; Next += 0x004000;

	MSM6295ROM	= Next;
	DrvSndROM	= Next; Next += 0x100000;

	DrvPalette	= (UINT32*)Next; Next += 0x2000 * sizeof(UINT32);

	AllRam		= Next;

	DrvShareRAM	= Next; Next += 0x010000;
	DrvPalRAM	= Next; Next += 0x004000;
	DrvVidRAM	= Next; Next += 0x004000;
	DrvSprRAM	= Next; Next += 0x001000;

	DrvVRegs	= Next; Next += 0x000008;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[4]  = { (0x100000*8)+8, (0x100000*8)+0, 8, 0 };
	INT32 XOffs[16] = { STEP8(0,1), STEP8(256,1) };
	INT32 YOffs[16] = { STEP16(0,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x200000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM, 0x200000);

	GfxDecode(0x4000, 4, 16, 16, Plane, XOffs, YOffs, 0x200, tmp, DrvGfxROM);

	BurnFree (tmp);

	return 0;
}

static void DrvTransTableInit()
{
	UINT16 mask0 = 0xff01;
	UINT16 mask1 = 0x00ff;

	for (INT32 i = 0; i < 0x400000; i+= 16 * 16)
	{
		DrvTransTab[0][i/(16*16)] = 1;
		DrvTransTab[1][i/(16*16)] = 1;

		for (INT32 j = 0; j < 16 * 16; j++)
		{
			if ((mask0 & (1 << DrvGfxROM[i+j])) == 0) {
				DrvTransTab[0][i/(16*16)] = 0;
			}

			if ((mask1 & (1 << DrvGfxROM[i+j])) == 0) {
				DrvTransTab[1][i/(16*16)] = 0;
			}
		}
	}
}

static INT32 DrvInit(INT32 load)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv68KROM + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x000000,  1, 2)) return 1;

		if (BurnLoadRom(DrvMCUROM + 0x000000,  2, 1)) return 1;

		if (load == 0) // 512kb roms
		{
			if (BurnLoadRom(DrvGfxROM + 0x000000,  3, 2)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x000001,  4, 2)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x100000,  5, 2)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x100001,  6, 2)) return 1;

			if (BurnLoadRom(DrvSndROM + 0x000000,  7, 1)) return 1;
			if (BurnLoadRom(DrvSndROM + 0x080000,  8, 1)) return 1;
		}
		else // 1mb roms
		{
			if (BurnLoadRom(DrvGfxROM + 0x000000,  3, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x100000,  4, 1)) return 1;

			if (BurnLoadRom(DrvSndROM + 0x000000,  5, 1)) return 1;
		}

		DrvGfxDecode();
		DrvTransTableInit();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(DrvVidRAM,			0x100000, 0x103fff, MAP_ROM);
	SekMapMemory(DrvPalRAM,			0x200000, 0x203fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,			0x440000, 0x440fff, MAP_RAM);
	SekMapMemory(DrvShareRAM,		0xfec000, 0xfeffff, MAP_RAM);
	SekSetWriteWordHandler(0,		wrally_main_write_word);
	SekSetWriteByteHandler(0,		wrally_main_write_byte);
	SekSetReadWordHandler(0,		wrally_main_read_word);
	SekSetReadByteHandler(0,		wrally_main_read_byte);
	SekClose();

	mcs51_program_data = DrvMCUROM;
	ds5002fp_init(0x88, 0x00, 0x80);
	mcs51_set_write_handler(dallas_sharedram_write);
	mcs51_set_read_handler(dallas_sharedram_read);

	MSM6295Init(0, 1000000 / 132, 0);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, screen0_map_callback, 16, 16, 64, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, screen1_map_callback, 16, 16, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM, 4, 16, 16, 0x400000, 0, 0x1f);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetOffsets(0, -8, -16);
	GenericTilemapSetOffsets(1, -8, -16);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	MSM6295Exit(0);

	SekExit();
	mcs51_exit();

	BurnFree (AllMem);

	MSM6295ROM = NULL;

	return 0;
}

static void DrvPaletteUpdate()
{
	UINT16 *p = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x4000/2; i++)
	{
		UINT16 x = BURN_ENDIAN_SWAP_INT16(p[i]);

		INT32 b = (x >> 8) & 0xf;
		INT32 r = (x >> 4) & 0xf;
		INT32 g = (x >> 0) & 0xf;

		DrvPalette[i] = BurnHighCol(r+r*16, g+g*16, b+b*16, 0);
	}
}

static void draw_sprites(INT32 priority)
{
	UINT16 *m_spriteram = (UINT16*)DrvSprRAM;

	for (INT32 i = 6/2; i < (0x1000 - 6)/2; i += 4)
	{
		INT32 sx = (m_spriteram[i+2] & 0x03ff) - 8;
		INT32 sy = (240 - (m_spriteram[i] & 0x00ff)) & 0x00ff;
		INT32 number = m_spriteram[i+3] & 0x3fff;
		INT32 color = (m_spriteram[i+2] & 0x7c00) >> 10;
		INT32 attr = (m_spriteram[i] & 0xfe00) >> 9;

		INT32 xflip = attr & 0x20;
		INT32 yflip = attr & 0x40;
		INT32 color_effect = (color & 0x10);
		INT32 high_priority = number >= 0x3700;
		color &= 0x0f;

		if (high_priority != priority) continue;

		if (flipscreen) {
			sy += 248;
		}

		sy -= 16;

		if (!color_effect) {
			if (yflip) {
				if (xflip) {
					Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, number, sx - 0x0f, sy, color, 4, 0, 0x200, DrvGfxROM);
				} else {
					Render16x16Tile_Mask_FlipY_Clip(pTransDraw, number, sx - 0x0f, sy, color, 4, 0, 0x200, DrvGfxROM);
				}
			} else {
				if (xflip) {
					Render16x16Tile_Mask_FlipX_Clip(pTransDraw, number, sx - 0x0f, sy, color, 4, 0, 0x200, DrvGfxROM);
				} else {
					Render16x16Tile_Mask_Clip(pTransDraw, number, sx - 0x0f, sy, color, 4, 0, 0x200, DrvGfxROM);
				}
			}
		} else {
			UINT8 *gfx_src = DrvGfxROM + ((number & 0x3fff) * 16 * 16);

			for (INT32 py = 0; py < 16; py++)
			{
				INT32 ypos = ((sy + py) & 0x1ff);
				UINT16 *srcy = pTransDraw + (ypos * nScreenWidth);

				INT32 gfx_py = yflip ? (16 - 1 - py) : py;

				if ((ypos < 0) || (ypos >= nScreenHeight)) continue;

				for (INT32 px = 0; px < 16; px++)
				{
					INT32 xpos = (((sx + px) & 0x3ff) - 0x0f) & 0x3ff;
					UINT16 *pixel = srcy + xpos;
					INT32 src_color = *pixel;

					INT32 gfx_px = xflip ? (16 - 1 - px) : px;
					INT32 gfx_pen = gfx_src[16*gfx_py + gfx_px];

					if ((gfx_pen < 8) || (gfx_pen >= 16)) continue;
					if ((xpos < 0) || (xpos >= nScreenWidth)) continue;

					*pixel = src_color + (gfx_pen - 8) * 0x400;
				}
			}
		}
	}
}

static INT32 DrvDraw()
{
//	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1; // always update
//	}

	UINT16 *vregs = (UINT16*)DrvVRegs;

	if (flipscreen)
	{
		GenericTilemapSetFlip(TMAP_GLOBAL, TMAP_FLIPXY);

		GenericTilemapSetScrollY(0, 248 - vregs[0]);
		GenericTilemapSetScrollX(0, 1024 - vregs[1] - 4);
		GenericTilemapSetScrollY(1, 248 - vregs[2]);
		GenericTilemapSetScrollX(1, 1024 - vregs[3]);
	}
	else
	{
		GenericTilemapSetScrollY(0, vregs[0]);
		GenericTilemapSetScrollX(0, vregs[1] + 4);
		GenericTilemapSetScrollY(1, vregs[2]);
		GenericTilemapSetScrollX(1, vregs[3]);
	}

	GenericTilemapDraw(1, pTransDraw, 0 | TMAP_FORCEOPAQUE);

	transparent_select = 0;
	GenericTilemapSetTransMask(0, 0xff01);
	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0 | TMAP_SET_GROUP(0));

	transparent_select = 1;
	GenericTilemapSetTransMask(0, 0x00ff);
	if (nBurnLayer & 2) GenericTilemapDraw(0, pTransDraw, 0 | TMAP_SET_GROUP(0));

	if (nBurnLayer & 4) GenericTilemapDraw(1, pTransDraw, 0 | TMAP_SET_GROUP(1));

	transparent_select = 0;
	GenericTilemapSetTransMask(0, 0xff01);
	if (nBurnLayer & 8) GenericTilemapDraw(0, pTransDraw, 0 | TMAP_SET_GROUP(1));

	if (nSpriteEnable & 1) draw_sprites(0);

	transparent_select = 1;
	GenericTilemapSetTransMask(0, 0x00ff);
	if (nBurnLayer & 8) GenericTilemapDraw(0, pTransDraw, 0 | TMAP_SET_GROUP(1));

	if (nSpriteEnable & 2) draw_sprites(1);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	SekNewFrame();

	{
		DrvInputs[0] = 0xffef;
		DrvInputs[1] = 0xffff;

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}

		if (coin_lockout[0]) DrvInputs[0] |= (1 << 6);
		if (coin_lockout[1]) DrvInputs[0] |= (1 << 7);
	}

	INT32 nSegment;
	INT32 nInterleave = 640; // mame
	INT32 nCyclesTotal[2] = { 12000000 / 60, 12000000 / 12 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };
	
	SekOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nSegment = (nCyclesTotal[0] - nCyclesDone[0]) / (nInterleave - i);
		nCyclesDone[0] += SekRun(nSegment);
		if (i == (nInterleave - 1)) SekSetIRQLine(6, CPU_IRQSTATUS_AUTO);

		nCyclesDone[1] += mcs51Run((SekTotalCycles()/12) - nCyclesDone[1]);
	}

	if (pBurnSoundOut) {
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
	}

	SekClose();

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
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);
		mcs51_scan(nAction);

		MSM6295Scan(0, nAction);

		SCAN_VAR(flipscreen);
		SCAN_VAR(okibank);
	}

	if (nAction & ACB_WRITE) {
		oki_bankswitch(okibank);
	}

	return 0;
}


// World Rally (Version 1.0, Checksum 0E56)

static struct BurnRomInfo wrallyRomDesc[] = {
	{ "worldr17.c23",	0x80000, 0x050f5629, 1 | BRF_PRG | BRF_ESS },    //  0 m68K code
	{ "worldr16.c22",	0x80000, 0x9e0d126c, 1 | BRF_PRG | BRF_ESS },    //  1

	{ "wrdallas.bin",	0x08000, 0x547d1768, 2 | BRF_PRG | BRF_ESS },    //  2 DS5002FP code

	{ "worldr21.i13",	0x80000, 0xb7fddb12, 3 | BRF_GRA },              //  3 Graphics
	{ "worldr20.i11",	0x80000, 0x58b2809a, 3 | BRF_GRA },              //  4
	{ "worldr19.i09",	0x80000, 0x018b35bb, 3 | BRF_GRA },              //  5
	{ "worldr18.i07",	0x80000, 0xb37c807e, 3 | BRF_GRA },              //  6

	{ "worldr14.c01",	0x80000, 0xe931c2ee, 4 | BRF_SND },              //  7 OKI Samples
	{ "worldr15.c03",	0x80000, 0x11f0fe2c, 4 | BRF_SND },              //  8

	{ "tibpal20l8-25cnt.b23", 0x104, 0x00000000, 5 | BRF_NODUMP | BRF_OPT }, //  9 plds
	{ "gal16v8-25lnc.h21",	  0x004, 0x00000000, 5 | BRF_NODUMP | BRF_OPT }, // 10
	{ "tibpal20l8-25cnt.h15", 0x104, 0x00000000, 5 | BRF_NODUMP | BRF_OPT }, // 11
	{ "pal16r4-e2.bin",	  0x104, 0x15fee75c, 5 | BRF_OPT },              // 12
	{ "pal16r8-b15.bin",	  0x104, 0xb50337a6, 5 | BRF_OPT },              // 13
};

STD_ROM_PICK(wrally)
STD_ROM_FN(wrally)

static INT32 WrallyInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvWrally = {
	"wrally", NULL, NULL, NULL, "1993",
	"World Rally (Version 1.0, Checksum 0E56)\0", NULL, "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_RACING, 0,
	NULL, wrallyRomInfo, wrallyRomName, NULL, NULL, WrallyInputInfo, WrallyDIPInfo,
	WrallyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	368, 232, 4, 3
};


// World Rally (Version 1.0, Checksum 3873)

static struct BurnRomInfo wrallyaRomDesc[] = {
	{ "c23.bin",		0x80000, 0x8b7d93c3, 1 | BRF_PRG | BRF_ESS },    //  0 m68K code
	{ "c22.bin",		0x80000, 0x56da43b6, 1 | BRF_PRG | BRF_ESS },    //  1

	{ "wrdallas.bin",	0x08000, 0x547d1768, 2 | BRF_PRG | BRF_ESS },    //  2 DS5002FP code
   
	{ "worldr21.i13",	0x80000, 0xb7fddb12, 3 | BRF_GRA },              //  3 Graphics
	{ "worldr20.i11",	0x80000, 0x58b2809a, 3 | BRF_GRA },              //  4
	{ "worldr19.i09",	0x80000, 0x018b35bb, 3 | BRF_GRA },              //  5
	{ "worldr18.i07",	0x80000, 0xb37c807e, 3 | BRF_GRA },              //  6

	{ "worldr14.c01",	0x80000, 0xe931c2ee, 4 | BRF_SND },              //  7 OKI Samples
	{ "worldr15.c03",	0x80000, 0x11f0fe2c, 4 | BRF_SND },              //  8

	{ "tibpal20l8-25cnt.b23", 0x104, 0x00000000, 5 | BRF_NODUMP | BRF_OPT }, //  9 plds
	{ "gal16v8-25lnc.h21",	  0x004, 0x00000000, 5 | BRF_NODUMP | BRF_OPT }, // 10
	{ "tibpal20l8-25cnt.h15", 0x104, 0x00000000, 5 | BRF_NODUMP | BRF_OPT }, // 11
	{ "pal16r4-e2.bin",	  0x104, 0x15fee75c, 5 | BRF_OPT },              // 12
	{ "pal16r8-b15.bin",	  0x104, 0xb50337a6, 5 | BRF_OPT },              // 13
};

STD_ROM_PICK(wrallya)
STD_ROM_FN(wrallya)

struct BurnDriver BurnDrvWrallya = {
	"wrallya", "wrally", NULL, NULL, "1993",
	"World Rally (Version 1.0, Checksum 3873)\0", NULL, "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_RACING, 0,
	NULL, wrallyaRomInfo, wrallyaRomName, NULL, NULL, WrallyInputInfo, WrallyDIPInfo,
	WrallyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	368, 232, 4, 3
};


// World Rally (Version 1.0, Checksum 8AA2)

static struct BurnRomInfo wrallybRomDesc[] = {
	{ "rally_c23.c23",	0x080000, 0xddd6f833, 1 | BRF_PRG | BRF_ESS },    //  0 m68K code
	{ "rally_c22.c22",	0x080000, 0x59a0d35c, 1 | BRF_PRG | BRF_ESS },    //  1

	{ "wrdallas.bin",	0x008000, 0x547d1768, 2 | BRF_PRG | BRF_ESS },    //  2 DS5002FP code

	{ "rally h-12.h12",	0x100000, 0x3353dc00, 3 | BRF_GRA },              //  3 Graphics
	{ "rally h-8.h8",	0x100000, 0x58dcd024, 3 | BRF_GRA },              //  4

	{ "sound c-1.c1",	0x100000, 0x2d69c9b8, 4 | BRF_SND },              //  5 OKI Samples

	{ "tibpal20l8-25cnt.b23",  0x104, 0x00000000, 5 | BRF_NODUMP | BRF_OPT }, //  6 plds
	{ "gal16v8-25lnc.h21",	   0x004, 0x00000000, 5 | BRF_NODUMP | BRF_OPT }, //  7
	{ "tibpal20l8-25cnt.h15",  0x104, 0x00000000, 5 | BRF_NODUMP | BRF_OPT }, //  8
	{ "pal16r4-e2.bin",	   0x104, 0x15fee75c, 5 | BRF_OPT },              //  9
	{ "pal16r8-b15.bin",	   0x104, 0xb50337a6, 5 | BRF_OPT },              // 10
};

STD_ROM_PICK(wrallyb)
STD_ROM_FN(wrallyb)

static INT32 WrallybInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvWrallyb = {
	"wrallyb", "wrally", NULL, NULL, "1993",
	"World Rally (Version 1.0, Checksum 8AA2)\0", NULL, "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_RACING, 0,
	NULL, wrallybRomInfo, wrallybRomName, NULL, NULL, WrallyInputInfo, WrallyDIPInfo,
	WrallybInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	368, 232, 4, 3
};


// World Rally (US, 930217)

static struct BurnRomInfo wrallyatRomDesc[] = {
	{ "rally.c23",		0x080000, 0x366595ad, 1 | BRF_PRG | BRF_ESS },    //  0 m68K code
	{ "rally.c22",		0x080000, 0x0ad4ec6f, 1 | BRF_PRG | BRF_ESS },    //  1

	{ "wrdallas.bin",	0x008000, 0x547d1768, 2 | BRF_PRG | BRF_ESS },    //  2 DS5002FP code

	{ "rally h-12.h12",	0x100000, 0x3353dc00, 3 | BRF_GRA },              //  3 Graphics
	{ "rally h-8.h8",	0x100000, 0x58dcd024, 3 | BRF_GRA },              //  4

	{ "sound c-1.c1",	0x100000, 0x2d69c9b8, 4 | BRF_SND },              //  5 OKI Samples

	{ "tibpal20l8-25cnt.b23",  0x104, 0x00000000, 5 | BRF_NODUMP | BRF_OPT }, //  6 plds
	{ "gal16v8-25lnc.h21",	   0x004, 0x00000000, 5 | BRF_NODUMP | BRF_OPT }, //  7
	{ "tibpal20l8-25cnt.h15",  0x104, 0x00000000, 5 | BRF_NODUMP | BRF_OPT }, //  8
	{ "pal16r4-e2.bin",	   0x104, 0x15fee75c, 5 | BRF_OPT },              //  9
	{ "pal16r8-b15.bin",	   0x104, 0xb50337a6, 5 | BRF_OPT },              // 10
};

STD_ROM_PICK(wrallyat)
STD_ROM_FN(wrallyat)

struct BurnDriver BurnDrvWrallyat = {
	"wrallyat", "wrally", NULL, NULL, "1993",
	"World Rally (US, 930217)\0", NULL, "Gaelco (Atari license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_RACING, 0,
	NULL, wrallyatRomInfo, wrallyatRomName, NULL, NULL, WrallyInputInfo, WrallyDIPInfo,
	WrallybInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	368, 232, 4, 3
};

