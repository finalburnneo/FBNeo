// FB Alpha Popper driver module
// Based on MAME driver by Dirk Best

#include "tiles_generic.h"
#include "z80_intf.h"
#include "bitswap.h"
#include "ay8910.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvVidRAM;
static UINT8 *DrvAttrRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvShareRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 flipscreen;
static UINT8 nmi_enable;
static UINT8 back_color;
static UINT8 vram_bank;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvReset;
static UINT8 DrvInputs[4];

static struct BurnInputInfo PopperInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 2,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 4,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvInputs + 2,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvInputs + 3,	"dip"		},
};

STDINPUTINFO(Popper)

static struct BurnDIPInfo PopperDIPList[]=
{
	{0x10, 0xff, 0xff, 0x24, NULL			},
	{0x11, 0xff, 0xff, 0x23, NULL			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x10, 0x01, 0x03, 0x00, "1 Coin  1 Credits"	},
	{0x10, 0x01, 0x03, 0x02, "2 Coins 1 Credits"	},
	{0x10, 0x01, 0x03, 0x01, "3 Coins 1 Credits"	},
	{0x10, 0x01, 0x03, 0x03, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x10, 0x01, 0x0c, 0x00, "1 Coin  3 Credits"	},
	{0x10, 0x01, 0x0c, 0x08, "1 Coin  4 Credits"	},
	{0x10, 0x01, 0x0c, 0x04, "1 Coin  5 Credits"	},
	{0x10, 0x01, 0x0c, 0x0c, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x10, 0x01, 0x30, 0x00, "2"			},
	{0x10, 0x01, 0x30, 0x20, "3"			},
	{0x10, 0x01, 0x30, 0x10, "4"			},
	{0x10, 0x01, 0x30, 0x30, "5"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x10, 0x01, 0xc0, 0x00, "20,000 Points"	},
	{0x10, 0x01, 0xc0, 0x80, "30,000 Points"	},
	{0x10, 0x01, 0xc0, 0x40, "40,000 Points"	},
	{0x10, 0x01, 0xc0, 0xc0, "50,000 Points"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x11, 0x01, 0x01, 0x00, "Off"			},
	{0x11, 0x01, 0x01, 0x01, "On"			},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x11, 0x01, 0x04, 0x00, "Off"			},
	{0x11, 0x01, 0x04, 0x04, "On"			},

	{0   , 0xfe, 0   ,    2, "Invulnerability"	},
	{0x11, 0x01, 0x08, 0x00, "Off"			},
	{0x11, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x11, 0x01, 0x10, 0x00, "Off"			},
	{0x11, 0x01, 0x10, 0x10, "On"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x11, 0x01, 0x20, 0x00, "Cocktail"		},
	{0x11, 0x01, 0x20, 0x20, "Upright"		},

	{0   , 0xfe, 0   ,    2, "Game Repeating"	},
	{0x11, 0x01, 0x40, 0x00, "Off"			},
	{0x11, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Pause"		},
	{0x11, 0x01, 0x80, 0x00, "Off"			},
	{0x11, 0x01, 0x80, 0x80, "On"			},
};

STDDIPINFO(Popper)

static void __fastcall popper_main_write(UINT16 address, UINT8 data)
{
	// skip nop'd regions
	if (address < 0xc000) return; // nop
	if (address >= 0xe800 && address <= 0xf7ff) return; // nop

//	bprintf (0, _T("MW: %4.4x, %2.2x\n"), address, data);

	// mirroring
//	if ((address & 0xfc00) == 0xe000) address &= ~0x1ff8;
//	if (address >= 0xe400) address &= ~0x3ff;

	switch (address)
	{
		case 0xe000:
			nmi_enable = data & 1;
		return;

		case 0xe001:
			flipscreen = data ? 1 : 0;
		return;

		case 0xe002:
			back_color = ~data & 1;
		return;

		case 0xe003:
			vram_bank = data & 1;
		return;

		case 0xe004:
		case 0xe005:
		case 0xe006:
		case 0xe007:
			// intcycle - not used
		return;
	}
}

static UINT8 input_read(INT32 offset)
{
	UINT8 data = 0;

	data |= BIT(DrvInputs[2], offset + 4) << 7;
	data |= BIT(DrvInputs[2], offset + 0) << 6;
	data |= BIT(DrvInputs[3], offset + 4) << 5;
	data |= BIT(DrvInputs[3], offset + 0) << 4;

	data |= BIT(DrvInputs[0], offset + 4) << 3;
	data |= BIT(DrvInputs[0], offset + 0) << 2;
	data |= BIT(DrvInputs[1], offset + 4) << 1;
	data |= BIT(DrvInputs[1], offset + 0) << 0;

	return data;
}

static UINT8 __fastcall popper_main_read(UINT16 address)
{
	// skip nop'd regions
	if (address < 0xc000) return 0; // nop
	if (address >= 0xe800 && address <= 0xf7ff) return 0; // nop

//	if (address != 0xfc00) bprintf (0, _T("MR: %4.4x\n"), address);

	// mirroring
	if (address < 0xe400) address &= ~0x03fc;
	if (address >= 0xe400) address &= ~0x3ff;

	switch (address)
	{
		case 0xe000:
		case 0xe001:
		case 0xe002:
		case 0xe003:
		{
			return input_read(address & 3);

			UINT8 ret = 0;

			for (INT32 i = 0; i < 8; i++) {
				ret |= ((DrvInputs[i/2] >> (((i & 1) * 4) + (address & 3))) & 1) << i;
			}

			return ret;
		}

		case 0xe400:
			ZetNmi(1);
			return 0;

		case 0xf800:
			ZetReset(1);
			return 0;

		case 0xfc00: //	watchdog_clear (not used)
			return 0;
	}

	return 0;
}

static void __fastcall popper_sound_write(UINT16 address, UINT8 data)
{
	if (address < 0x8000 || address >= 0xe000) return; // nop

//	bprintf (0, _T("SW: %4.4x, %2.2x\n"), address, data);

	if ((address & 0xe000) == 0x8000) {
		if ((address & 3) == 3) {
			AY8910Reset(0);
			AY8910Reset(1);
		}

		AY8910Write(0, address & 1, data);
		return;
	}

	if ((address & 0xe000) == 0xa000) {
		AY8910Write(1, address & 1, data);
		return;
	}
}

static tilemap_callback( layer0 )
{
	INT32 code = DrvVidRAM[offs] + (vram_bank * 256);
	INT32 attr = DrvAttrRAM[offs];
	INT32 color = (back_color * 0x10) | ((attr >> 3) & 0x0e);

	INT32 group = ((attr & 0x80) >> 7) && (attr & 0x70);

	TILE_SET_INFO(0, code, color, TILE_GROUP(group));
}

static tilemap_callback( layer1 )
{
	INT32 code = DrvVidRAM[offs] + (vram_bank * 256);
	INT32 attr = DrvAttrRAM[offs];

	TILE_SET_INFO(1, code, attr & 0x0f, TILE_GROUP((attr & 0x80) >> 7));
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetReset(0);
	ZetReset(1);

	AY8910Reset(0);
	AY8910Reset(1);

	flipscreen = 0;
	nmi_enable = 0;
	vram_bank = 0;
	back_color = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x006000;
	DrvZ80ROM1		= Next; Next += 0x001000;

	DrvGfxROM0		= Next; Next += 0x008000;
	DrvGfxROM1		= Next; Next += 0x008000;
	DrvGfxROM2		= Next; Next += 0x010000;

	DrvColPROM		= Next; Next += 0x000040;

	DrvPalette		= (UINT32*)Next; Next += 0x040 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM0		= Next; Next += 0x001000;
	DrvVidRAM		= Next; Next += 0x000600;
	DrvAttrRAM		= Next; Next += 0x000600;
	DrvSprRAM		= Next; Next += 0x000800;
	DrvShareRAM		= Next; Next += 0x000800;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}


static INT32 DrvGfxDecode()
{
	INT32 Plane0[2] = { 0, 4 };
	INT32 Plane2[2] = { 0, 0x2000 * 8 };
	INT32 XOffs0[8] = { STEP4(8,1), STEP4(0,1) };
	INT32 XOffs2[16]= { STEP8(8,1), STEP8(0,1) };
	INT32 YOffs[16] = { STEP16(0,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x4000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x2000);

	GfxDecode(0x0200, 1,  8,  8, Plane0, XOffs0, YOffs, 0x080, tmp, DrvGfxROM0);
	GfxDecode(0x0200, 2,  8,  8, Plane0, XOffs0, YOffs, 0x080, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x4000);

	GfxDecode(0x0100, 2, 16, 16, Plane2, XOffs2, YOffs, 0x100, tmp, DrvGfxROM2);

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
		if (BurnLoadRom(DrvZ80ROM0 + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x2000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x4000,  2, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x0000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x0000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x0000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x2000,  6, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x0000,  7, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0020,  8, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0x5fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,		0xc000, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,			0xc100, 0xc6ff, MAP_RAM);
	ZetMapMemory(DrvAttrRAM,		0xc900, 0xceff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,		0xd800, 0xdfff, MAP_RAM);
	ZetSetWriteHandler(popper_main_write);
	ZetSetReadHandler(popper_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,		0x0000, 0x0fff, MAP_ROM);
	ZetMapMemory(DrvShareRAM,		0xc000, 0xc7ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,		0xc800, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,		0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,		0xd800, 0xdfff, MAP_RAM);
	ZetSetWriteHandler(popper_sound_write);
	ZetClose();

	AY8910Init(0, 1536000, 0);
	AY8910Init(1, 1536000, 1);
	AY8910SetAllRoutes(0, 0.25, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.25, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_COLS, layer0_map_callback, 8, 8, 48, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_COLS, layer1_map_callback, 8, 8, 48, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 1, 8, 8, 0x08000, 0, 0x1f);
	GenericTilemapSetGfx(1, DrvGfxROM1, 2, 8, 8, 0x08000, 0, 0x0f);
	GenericTilemapSetTransparent(0, 1);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetOffsets(TMAP_GLOBAL, -48, -16);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	AY8910Exit(0);
	AY8910Exit(1);

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x40; i++)
	{
		INT32 b0 = ((DrvColPROM[i] >> 0) & 1);
		INT32 b1 = ((DrvColPROM[i] >> 1) & 1);
		INT32 b2 = ((DrvColPROM[i] >> 2) & 1);
		INT32 r = b2 * 1000 + b1 * 470 + b0 * 220; 

		b0 = ((DrvColPROM[i] >> 3) & 1);
		b1 = ((DrvColPROM[i] >> 4) & 1);
		b2 = ((DrvColPROM[i] >> 5) & 1);
		INT32 g = b2 * 1000 + b1 * 470 + b0 * 220;

		b0 = ((DrvColPROM[i] >> 6) & 1);
		b1 = ((DrvColPROM[i] >> 7) & 1);
		INT32 b = b1 * 470 + b0 * 220;

		r = (r * 255) / (1000 + 470 + 220);
		g = (g * 255) / (1000 + 470 + 220);
		b = (b * 255) / (470 + 220);
 
		// + normalization -- colors wrong!

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0; offs < 0x800; offs += 4)
	{
		INT32 sx = DrvSprRAM[offs + 3];
		INT32 sy = DrvSprRAM[offs + 0];

		if (((sy + (flipscreen ? 2 : 0)) >> 4) != ((~(offs >> 7)) & 0x0f))
			continue;

		INT32 code  = DrvSprRAM[offs + 1];
		INT32 color = DrvSprRAM[offs + 2] & 0x0f;
		INT32 flipx = DrvSprRAM[offs + 2] & 0x40;
		INT32 flipy = DrvSprRAM[offs + 2] & 0x80;

		sx += 64;
		sy = 240 - sy;

		if (flipscreen)
		{
			flipx = !flipx;
			flipy = !flipy;

			sx = 232 - sx;
			sx += 128;
			sy = (240 + 2) - sy;

			// need to adjust x & y for video offsets?
		} else {
			sx -= 48;
			sy -= 16;
		}

		if (flipy) {
			if (flipx) {
				Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM2);
			} else {
				Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM2);
			}
		} else {
			if (flipx) {
				Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM2);
			} else {
				Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM2);
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	BurnTransferClear();

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, TMAP_SET_GROUP(0));
	if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, TMAP_SET_GROUP(0));

	if (nSpriteEnable & 1) draw_sprites();

	if (nBurnLayer & 4) GenericTilemapDraw(0, pTransDraw, TMAP_SET_GROUP(1));
	if (nBurnLayer & 8) GenericTilemapDraw(1, pTransDraw, TMAP_SET_GROUP(1));

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	if (nCurrentFrame & 1) ZetNewFrame();

	{
		DrvInputs[0] = 0;
		DrvInputs[1] = 0;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 264;
	INT32 nCyclesTotal[2] = { 3072000 / 60, 3072000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);
		INT32 cycles = ZetTotalCycles();
		if (nmi_enable && i == 240) ZetNmi();
		ZetClose();

		ZetOpen(1);
		nCyclesDone[1] += ZetRun(cycles - ZetTotalCycles());
		ZetSetIRQLine(0, ((i & 0x1f) == 0) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE); 
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

		SCAN_VAR(flipscreen);
		SCAN_VAR(nmi_enable);
		SCAN_VAR(back_color);
		SCAN_VAR(vram_bank);
	}

	return 0;
}


// Popper

static struct BurnRomInfo popperRomDesc[] = {
	{ "p1",		0x2000, 0x56881b70, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 code
	{ "p2",		0x2000, 0xa054d9d2, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "p3",		0x2000, 0x6201928a, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "p0",		0x1000, 0xef5f7c5b, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 code

	{ "p4",		0x2000, 0x86203349, 3 | BRF_GRA },           //  4 Tiles

	{ "p5",		0x2000, 0xa21ac194, 4 | BRF_GRA },           //  5 Sprites
	{ "p6",		0x2000, 0xd99fa790, 4 | BRF_GRA },           //  6

	{ "p.m3",	0x0020, 0x713217aa, 5 | BRF_GRA },           //  7 Color data
	{ "p.m4",	0x0020, 0x384de5c1, 5 | BRF_GRA },           //  8
};

STD_ROM_PICK(popper)
STD_ROM_FN(popper)

struct BurnDriver BurnDrvPopper = {
	"popper", NULL, NULL, NULL, "1983",
	"Popper\0", NULL, "Omori", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, popperRomInfo, popperRomName, NULL, NULL, NULL, NULL, PopperInputInfo, PopperDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 280, 3, 4
};
