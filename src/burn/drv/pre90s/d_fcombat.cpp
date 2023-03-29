// FB Neo Jaleco Field Combat driver module
// Based on MAME driver by Tomasz Slanina
// Color fixes (sprite & bg/terrain) by hap (Oct. 2022)

#include "tiles_generic.h"
#include "z80_intf.h"
#include "ay8910.h"
#include "bitswap.h"
#include "resnet.h"

static UINT8 *AllMem;
static UINT8 *RamEnd;
static UINT8 *AllRam;
static UINT8 *MemEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvBgROM;
static UINT8 *DrvTerrain;
static UINT8 *DrvColPROM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 fcombat_tx;
static UINT8 fcombat_ty;
static UINT8 fcombat_sh;
static UINT16 fcombat_sv;
static UINT8 soundlatch;
static UINT8 video_regs;
static UINT8 char_palette;
static UINT8 char_bank;
static UINT8 sprite_bank;
static UINT8 cocktail_flip;

static INT32 vblank;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo FcombatInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 2"	},
	
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Fcombat)

static struct BurnDIPInfo FcombatDIPList[]=
{
	DIP_OFFSET(0x11)
	{0x00, 0xff, 0xff, 0xa2, NULL				},
	{0x01, 0xff, 0xff, 0xf0, NULL				},

	{0   , 0xfe, 0   ,    6, "Lives"			},
	{0x00, 0x01, 0x07, 0x00, "1"				},
	{0x00, 0x01, 0x07, 0x01, "2"				},
	{0x00, 0x01, 0x07, 0x02, "3"				},
	{0x00, 0x01, 0x07, 0x03, "4"				},
	{0x00, 0x01, 0x07, 0x04, "5"				},
	{0x00, 0x01, 0x07, 0x07, "Infinite (Cheat)"	},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x00, 0x01, 0x18, 0x00, "10000"			},
	{0x00, 0x01, 0x18, 0x08, "20000"			},
	{0x00, 0x01, 0x18, 0x10, "30000"			},
	{0x00, 0x01, 0x18, 0x18, "40000"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x00, 0x01, 0x80, 0x00, "Upright"			},
	{0x00, 0x01, 0x80, 0x80, "Cocktail"			},

	{0   , 0xfe, 0   ,    4, "Coinage"			},
	{0x01, 0x01, 0x0c, 0x00, "1 Coin  1 Credits"},
	{0x01, 0x01, 0x0c, 0x04, "1 Coin  2 Credits"},
	{0x01, 0x01, 0x0c, 0x08, "1 Coin  3 Credits"},
	{0x01, 0x01, 0x0c, 0x0c, "1 Coin  4 Credits"},
};

STDDIPINFO(Fcombat)

static void __fastcall fcombat_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xe800:
			cocktail_flip = data & 1;
			char_palette = (data >> 1) & 3;
			char_bank = (data >> 3) & 1;
			sprite_bank = (data & 0xc0) >> 6;
		return;

		case 0xe900:
			fcombat_sh = data;
		return;

		case 0xea00:
			fcombat_sv = (fcombat_sv & 0xff00) + data;
		return;

		case 0xeb00:
			fcombat_sv = (fcombat_sv & 0x00ff) + (data * 256);
		return;

		case 0xec00:
			fcombat_tx = data;
		return;

		case 0xed00:
			fcombat_ty = data;
		return;

		case 0xee00:
		return; // nop

		case 0xef00:
			soundlatch = data;
		return;
	}
}

static UINT8 __fastcall fcombat_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xe000:
			return DrvInputs[cocktail_flip];

		case 0xe100:
			return DrvDips[0];

		case 0xe200:
			return (DrvDips[1] & 0xfe) | (vblank ? 1 :0);

		case 0xe300: {
			INT32 wx = ((fcombat_tx * 1) + fcombat_sh) / 16;
			INT32 wy = ((fcombat_ty * 2) + fcombat_sv) / 16;
			return DrvTerrain[wx * 0x200 + wy];
		}

		case 0xe400:
			return 0xff;
	}

	return 0;
}

static void __fastcall fcombat_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x8002:
		case 0x8003:
		case 0xa002:
		case 0xa003:
		case 0xc002:
		case 0xc003:
			AY8910Write((address >> 13) & 3, ~address & 1, data);
		return;
	}
}

static UINT8 __fastcall fcombat_sound_read(UINT16 address)
{
	switch (address & ~1)
	{
		case 0x6000:
			return soundlatch;

		case 0x8000:
		case 0xa000:
		case 0xc000:
			return AY8910Read((address >> 13) & 3);
	}

	return 0;
}

static tilemap_callback( bg )
{
	TILE_SET_INFO(0, DrvBgROM[offs], (DrvBgROM[offs] >> 2 & 3) | (DrvBgROM[offs] >> 4 & 0xc), 0);
}

static tilemap_callback( tx )
{
	UINT8 attr = DrvVidRAM[offs];

	TILE_SET_INFO(1, attr + (char_bank * 256), (attr >> 4) + (char_palette * 16), 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	ZetClose();

	AY8910Reset(0);
	AY8910Reset(1);
	AY8910Reset(2);

	DrvInputs[2] = 0; // coin

	fcombat_tx = 0;
	fcombat_ty = 0;
	fcombat_sh = 0;
	fcombat_sv = 0;
	soundlatch = 0;
	video_regs = 0;
	char_palette = 0;
	char_bank = 0;
	cocktail_flip = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x008000;
	DrvZ80ROM1		= Next; Next += 0x004000;

	DrvGfxROM0		= Next; Next += 0x008000;
	DrvGfxROM1		= Next; Next += 0x030000;
	DrvGfxROM2		= Next; Next += 0x010000;

	DrvBgROM		= Next; Next += 0x004000;
	DrvTerrain		= Next; Next += 0x004000;
	DrvColPROM      = Next; Next += 0x000800;

	DrvPalette		= (UINT32*)Next; Next += 0x0300 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM0		= Next; Next += 0x000800;
	DrvZ80RAM1		= Next; Next += 0x000800;

	DrvVidRAM		= Next; Next += 0x000800;
	DrvSprRAM		= Next; Next += 0x000100;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void decode_gfx0()
{
	UINT8 *tmp = (UINT8*)BurnMalloc(0x2000);

	for (INT32 i = 0; i < 0x2000; i++) {
		INT32 j = (i & 0x1f00) | BITSWAP08(i, 4, 3, 2, 1, 7, 6, 5, 0);
		tmp[j] = DrvGfxROM0[i];
	}

	memcpy (DrvGfxROM0, tmp, 0x2000);

	BurnFree (tmp);
}

static void decode_gfx12(UINT8 *gfx, INT32 len)
{
	UINT8 *tmp = (UINT8*)BurnMalloc(len*2);

	for (INT32 i = 0; i < len; i++) {
		INT32 j = BITSWAP16(i, 15, 14, 12, 11, 10, 9, 13, 4, 3, 2, 8, 7, 6, 5, 1, 0);
		tmp[j] = gfx[i];
	}

	memcpy (gfx, tmp, len);

	BurnFree (tmp);
}

static void decode_others(UINT8 *gfx, INT32 len)
{

    UINT8 *tmp = (UINT8*)BurnMalloc(len);

	for (INT32 i = 0; i < len; i++) {
		INT32 j = BITSWAP16(i, 15, 14, 12,  11, 10,  9, 8,  13, 7, 6, 5, 4, 3, 2, 1, 0);
		tmp[j] = gfx[i];
	}

	memcpy (gfx, tmp, len);

	BurnFree (tmp);
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[2]   = { 0, 4 };
	INT32 XOffs[16]  = { STEP4(8*0+3,-1), STEP4(8*1+3,-1), STEP4(8*2+3,-1), STEP4(8*3+3,-1) };
	INT32 YOffs0[8]  = { STEP8(0,16) };
	INT32 YOffs1[16] = { STEP16(0,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0xc000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x2000);

	GfxDecode(0x0200, 2,  8,  8, Plane, XOffs, YOffs0, 0x080, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0xc000);

	GfxDecode(0x0300, 2, 16, 16, Plane, XOffs, YOffs1, 0x200, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x4000);

	GfxDecode(0x0100, 2, 16, 16, Plane, XOffs, YOffs1, 0x200, tmp, DrvGfxROM2);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x04000,  1, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x04000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x08000,  6, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000,  7, 1)) return 1;

		if (BurnLoadRom(DrvBgROM   + 0x00000,  8, 1)) return 1;

		if (BurnLoadRom(DrvTerrain + 0x00000,  9, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000, 10, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00100, 11, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00200, 12, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00300, 13, 1)) return 1;

		decode_gfx0();
		decode_gfx12(DrvGfxROM1,  0xc000);
		decode_gfx12(DrvGfxROM2,  0x4000);
		decode_others(DrvBgROM,   0x4000);
		decode_others(DrvTerrain, 0x4000);
		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,		0xc000, 0xc7ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,			0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0xd800, 0xd8ff, MAP_RAM);
	ZetSetWriteHandler(fcombat_main_write);
	ZetSetReadHandler(fcombat_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,		0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,		0x4000, 0x47ff, MAP_RAM);
	ZetSetWriteHandler(fcombat_sound_write);
	ZetSetReadHandler(fcombat_sound_read);
	ZetClose();

	AY8910Init(0, 1500000, 0);
	AY8910Init(1, 1500000, 1);
	AY8910Init(2, 1500000, 1);
	AY8910SetAllRoutes(0, 0.12, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.12, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(2, 0.12, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 16, 16, 512, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, tx_map_callback,  8,  8,  64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM2, 2, 16, 16, 0x10000, 0x200, 0x3f);
	GenericTilemapSetGfx(1, DrvGfxROM0, 2,  8,  8, 0x08000, 0x000, 0x3f);
	GenericTilemapSetOffsets(TMAP_GLOBAL, -96, -16);
	GenericTilemapSetTransparent(1, 0);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	AY8910Exit(0);
	AY8910Exit(1);
	AY8910Exit(2);
	HiscoreReset();

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteInit()
{
	UINT32 tab[0x20];

	INT32 resistances_rg[3] = { 1000, 470, 220 };
	INT32 resistances_b [2] = { 470, 220 };
	double rweights[3], gweights[3], bweights[2];

	compute_resistor_weights(0, 255, -1.0,
							 3, &resistances_rg[0], rweights, 0, 0,
							 3, &resistances_rg[0], gweights, 0, 0,
							 2, &resistances_b[0],  bweights, 0, 0);

	for (INT32 i = 0; i < 0x20; i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 1;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 1;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 1;
		INT32 r = combine_3_weights(rweights, bit0, bit1, bit2);

		bit0 = (DrvColPROM[i] >> 3) & 1;
		bit1 = (DrvColPROM[i] >> 4) & 1;
		bit2 = (DrvColPROM[i] >> 5) & 1;
		INT32 g = combine_3_weights(gweights, bit0, bit1, bit2);

		bit0 = (DrvColPROM[i] >> 6) & 1;
		bit1 = (DrvColPROM[i] >> 7) & 1;
		INT32 b = combine_2_weights(bweights, bit0, bit1);

		tab[i] = BurnHighCol(r, g, b, 0);
	}

	for (INT32 i = 0; i < 0x200; i++)
	{
		DrvPalette[i] = tab[(DrvColPROM[0x100 + ( (i & 0x1c0) | ((i & 3) << 4) | ((i >> 2) & 0x0f))] & 0x0f) | 0x10];
	}

	for (INT32 i = 0x200; i < 0x300; i++)
	{
		DrvPalette[i] = tab[DrvColPROM[0x100 + ( (i & 0x3c0) | ((i & 3) << 4) | ((i >> 2) & 0x0f))] & 0x0f];
	}
}

static void draw_sprites()
{
	for (INT32 i = 0; i < 0x100; i += 4)
	{
		INT32 flags = DrvSprRAM[i + 0];
		INT32 y = (256 - DrvSprRAM[i + 1]) - 16;
		INT32 code = DrvSprRAM[i + 2] + ((flags & 0x20) << 3);
		INT32 x = (DrvSprRAM[i + 3] * 2 + (flags & 0x01) + 52) - ((cocktail_flip) ? -(12*8) : (12*8));

		INT32 xflip = flags & 0x80;
		INT32 yflip = flags & 0x40;
		INT32 wide = flags & 0x08;

		INT32 color = ((flags >> 1) & 0x03) | ((code >> 5) & 0x04) | (code & 0x08) | ((code >> 4) & 0x10);

		if ((code & 0x108) == 0x108)
		{
			const INT32 mask[4] = { 0x308, 0x300, 0x08, 0x00 };
			code ^= mask[sprite_bank];
		}

		INT32 code2 = code;

		if (cocktail_flip)
		{
			x = 64 * 8 - 16 - x + 2;
			y = 32 * 8 - 16 - y + 2;
			if (wide) y -= 16;
			if (flags&0x10) y -= 48;
			y -= 32; // ?
			xflip = !xflip;
			yflip = !yflip;
		}

		if (wide)
		{
			if (yflip)
				code |= 0x10, code2 &= ~0x10;
			else
				code &= ~0x10, code2 |= 0x10;

			Draw16x16MaskTile(pTransDraw, code2, x, y + 16, xflip, yflip, color, 2, 0, 0x100, DrvGfxROM1);
		}

		if(flags&0x10)
		{
			if (yflip) {
				Draw16x16MaskTile(pTransDraw, code, x, y + 48, xflip, yflip, color, 2, 0, 0x100, DrvGfxROM1);
				Draw16x16MaskTile(pTransDraw, code2 + 16, x, y + 32, xflip, yflip, color, 2, 0, 0x100, DrvGfxROM1);
				Draw16x16MaskTile(pTransDraw, code2 + 32, x, y + 16, xflip, yflip, color, 2, 0, 0x100, DrvGfxROM1);
				Draw16x16MaskTile(pTransDraw, code2 + 48, x, y + 0, xflip, yflip, color, 2, 0, 0x100, DrvGfxROM1);
				continue;
			} else {
				Draw16x16MaskTile(pTransDraw, code2 + 16, x, y + 16, xflip, yflip, color, 2, 0, 0x100, DrvGfxROM1);
				Draw16x16MaskTile(pTransDraw, code2 + 32, x, y + 32, xflip, yflip, color, 2, 0, 0x100, DrvGfxROM1);
				Draw16x16MaskTile(pTransDraw, code2 + 48, x, y + 48, xflip, yflip, color, 2, 0, 0x100, DrvGfxROM1);
			}
		}

		Draw16x16MaskTile(pTransDraw, code, x, y, xflip, yflip, color, 2, 0, 0x100, DrvGfxROM1);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	BurnTransferClear();

	GenericTilemapSetFlip(TMAP_GLOBAL, (cocktail_flip) ? TMAP_FLIPXY : 0);
	GenericTilemapSetScrollY(0, fcombat_sh);
	if (cocktail_flip) {
		GenericTilemapSetScrollX(0, fcombat_sv + 8 - 2);
	} else {
		GenericTilemapSetScrollX(0, fcombat_sv - 8);
	}

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);

	if (nSpriteEnable & 1) draw_sprites();

	if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 0);

	BurnTransferFlip(cocktail_flip, cocktail_flip); // unflip coctail for netplay/etc
	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		INT32 previous_coin = DrvInputs[2];

		DrvInputs[0] = 0xff;
		DrvInputs[1] = 0xff;
		DrvInputs[2] = 0x00;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		if (DrvInputs[2] != previous_coin) {
			ZetSetIRQLine(0, 0x20, DrvInputs[2] ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 3333333 / 60, 3333333 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		if (i == 0) vblank = 1;
		if (i == 32) vblank = 0;
		ZetOpen(0);
		CPU_RUN(0, Zet);
		ZetClose();

		ZetOpen(1);
		CPU_RUN(1, Zet);
		ZetClose();
	}

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		DrvDraw();
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

		ZetScan(nAction);
		AY8910Scan(nAction, pnMin);

		SCAN_VAR(fcombat_tx);
		SCAN_VAR(fcombat_ty);
		SCAN_VAR(fcombat_sh);
		SCAN_VAR(fcombat_sv);
		SCAN_VAR(soundlatch);
		SCAN_VAR(video_regs);
		SCAN_VAR(char_palette);
		SCAN_VAR(char_bank);
		SCAN_VAR(sprite_bank);
		SCAN_VAR(cocktail_flip);
	}

	return 0;
}


// Field Combat

static struct BurnRomInfo fcombatRomDesc[] = {
	{ "fcombat2.t9",	0x4000, 0x30cb0c14, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "fcombat3.10t",	0x4000, 0xe8511da0, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "fcombat1.t5",	0x4000, 0xa0cc1216, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "fcombat7.l11",	0x2000, 0x401061b5, 3 | BRF_GRA },           //  3 Characters

	{ "fcombat8.d10",	0x4000, 0xe810941e, 4 | BRF_GRA },           //  4 Sprites
	{ "fcombat9.d11",	0x4000, 0xf95988e6, 4 | BRF_GRA },           //  5
	{ "fcomba10.d12",	0x4000, 0x908f154c, 4 | BRF_GRA },           //  6

	{ "fcombat6.f3",	0x4000, 0x97282729, 5 | BRF_GRA },           //  7 Background Tiles

	{ "fcombat5.l3",	0x4000, 0x96194ca7, 6 | BRF_GRA },           //  8 Background Map

	{ "fcombat4.p3",	0x4000, 0xefe098ab, 7 | BRF_GRA },           //  9 Terrain Data

	{ "fcprom_a.c2",	0x0020, 0x7ac480f0, 8 | BRF_GRA },           // 10 Color data
	{ "fcprom_d.k12",	0x0100, 0x9a348250, 8 | BRF_GRA },           // 11
	{ "fcprom_c.a9",	0x0100, 0x768ac120, 8 | BRF_GRA },           // 12
	{ "fcprom_b.c4",	0x0100, 0xac9049f6, 8 | BRF_GRA },           // 13
};

STD_ROM_PICK(fcombat)
STD_ROM_FN(fcombat)

struct BurnDriver BurnDrvFcombat = {
	"fcombat", NULL, NULL, NULL, "1985",
	"Field Combat\0", NULL, "Jaleco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, fcombatRomInfo, fcombatRomName, NULL, NULL, NULL, NULL, FcombatInputInfo, FcombatDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	224, 320, 3, 4
};
