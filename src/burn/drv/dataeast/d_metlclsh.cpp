// FB Alpha Metal Clash driver module
// Based on MAME driver by Luca Elia

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "burn_ym2203.h"
#include "burn_ym3526.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvMainROM;
static UINT8 *DrvSubROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvShareRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvBgRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 bankdata;
static UINT8 write_mask;
static UINT8 flipscreen;
static UINT8 gfxbank;
static UINT8 scrollx[2];

static INT32 vblank;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvInputs[3];
static UINT8 DrvDips[2];
static UINT8 DrvReset;

static struct BurnInputInfo MetlclshInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 6,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Metlclsh)

static struct BurnDIPInfo MetlclshDIPList[]=
{
	{0x12, 0xff, 0xff, 0x3f, NULL			},
	{0x13, 0xff, 0xff, 0xdf, NULL			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x12, 0x01, 0x01, 0x00, "2"			},
	{0x12, 0x01, 0x01, 0x01, "3"			},

	{0   , 0xfe, 0   ,    2, "Enemies Speed"	},
	{0x12, 0x01, 0x02, 0x02, "Low"			},
	{0x12, 0x01, 0x02, 0x00, "High"			},

	{0   , 0xfe, 0   ,    2, "Enemies Energy"	},
	{0x12, 0x01, 0x04, 0x04, "Low"			},
	{0x12, 0x01, 0x04, 0x00, "High"			},

	{0   , 0xfe, 0   ,    2, "Time"			},
	{0x12, 0x01, 0x08, 0x00, "75"			},
	{0x12, 0x01, 0x08, 0x08, "90"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x12, 0x01, 0x10, 0x10, "Off"			},
	{0x12, 0x01, 0x10, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{0x13, 0x01, 0x03, 0x00, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x03, 0x01, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x20, 0x20, "Off"			},
	{0x13, 0x01, 0x20, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Infinite Energy"	},
	{0x13, 0x01, 0x40, 0x40, "Off"			},
	{0x13, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Infinite Lives"	},
	{0x13, 0x01, 0x80, 0x80, "Off"			},
	{0x13, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Metlclsh)

static void metlclsh_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xc080:
		return;	//nop

		case 0xc0c2:
			M6809Close();
			M6809Open(1);
			M6809SetIRQLine(0, CPU_IRQSTATUS_ACK);
			M6809Close();
			M6809Open(0);
		return;

		case 0xc0c3:
			M6809SetIRQLine(0x20, CPU_IRQSTATUS_NONE);
		return;

		case 0xd000:
		case 0xd001:
			BurnYM2203Write(0, address & 1, data);
		return;

		case 0xe000:
		case 0xe001:
			BurnYM3526Write(address & 1, data);
		return;
	}
}

static UINT8 metlclsh_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xc000:
			return DrvDips[1];

		case 0xc001:
			return DrvInputs[0];

		case 0xc002:
			return DrvInputs[1];

		case 0xc003:
			return DrvInputs[2] | (vblank * 0x80);

		case 0xd000:
		case 0xd001:
			return BurnYM2203Read(0,address & 1);
	}
	
	return 0;
}

static void bankswitch(INT32 data)
{
	bankdata = data;

	if (data & 1)
	{
		M6809MapMemory(DrvBgRAM,		0xd000,	0xd7ff, MAP_RAM);
	}
	else
	{
		M6809MapMemory(DrvBgRAM + 0x800,	0xd000,	0xd7ff, MAP_ROM);
		M6809UnmapMemory(			0xd000, 0xd7ff, MAP_WRITE);

		write_mask = 1 << (data >> 1);
	}
}

static void metlclsh_sub_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf800) == 0xd000) {
		UINT8 v = DrvBgRAM[0x800 + (address & 0x7ff)] & ~write_mask;
		DrvBgRAM[0x800 + (address & 0x7ff)] = v | (data & write_mask);
		return;
	}

	switch (address)
	{
		case 0xc000:
			if ((data & 0x04) == 0)
				gfxbank = data & 3;
		return;

		case 0xc0c0:
			M6809Close();
			M6809Open(0);
			M6809SetIRQLine(0x20, CPU_IRQSTATUS_ACK);
			M6809Close();
			M6809Open(1);
		return;

		case 0xc0c1:
			M6809SetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;

		case 0xe301:
			flipscreen = data & 1;
		return;

		case 0xe401:
			bankswitch(data);
		return;

		case 0xe402:
		case 0xe403:
			scrollx[address & 1] = data;
		return;

		case 0xe417:
			M6809SetIRQLine(0x20, CPU_IRQSTATUS_NONE);
		return;
	}
}

static void DrvYM3526IrqHandler(INT32, INT32 nStatus)
{
	if (M6809GetActive() == -1) return;

	M6809SetIRQLine(0, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static tilemap_scan( bg )
{
	return  (row & 7) + ((row & ~7) << 4) + ((col & 0xf) << 3) + ((col & ~0xf) << 4);
}

static tilemap_callback( bg )
{
	TILE_SET_INFO(0, DrvBgRAM[offs] + (gfxbank * 0x80), 0, 0);
}

static tilemap_callback( fg )
{
	UINT8 attr = DrvFgRAM[offs + 0x400];

	TILE_SET_INFO(1, DrvFgRAM[offs] + (attr * 256), attr / 0x20, TILE_GROUP(attr / 0x80));
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	M6809Open(0);
	M6809Reset();
	BurnYM2203Reset();
	BurnYM3526Reset();
	M6809Close();

	M6809Open(1);
	bankswitch(1);
	M6809Reset();
	M6809Close();

	gfxbank = 0;
	flipscreen = 0;
	scrollx[0] = scrollx[1] = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next;		Next = AllMem;

	DrvMainROM		= Next; Next += 0x00c000;
	DrvSubROM		= Next; Next += 0x008000;

	DrvGfxROM0		= Next; Next += 0x040000;
	DrvGfxROM1		= Next; Next += 0x020000;
	DrvGfxROM2		= Next; Next += 0x010000;

	DrvPalette		= (UINT32*)Next; Next += 0x30 * sizeof(UINT32);

	AllRam			= Next;

	DrvShareRAM		= Next; Next += 0x002000;
	DrvPalRAM		= Next; Next += 0x000200;
	DrvFgRAM		= Next; Next += 0x000800;
	DrvSprRAM		= Next; Next += 0x000200;
	DrvBgRAM		= Next; Next += 0x001000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[3]  = { 0x8000*8*2, 0x8000*8*1, 0x8000*8*0 };
	INT32 XOffs0[16] = { STEP8(8*8*2,1), STEP8(8*8*0,1) };
	INT32 YOffs0[16] = { STEP8(8*8*0,8), STEP8(8*8*1,8) };

	INT32 Plane1[3]  = { 0x4000*8*2, 0x4000*8*1, 0x4000*8*0 };
	INT32 XOffs1[16] = { STEP8(8*8*0+7,-1), STEP8(8*8*2+7,-1) };
	INT32 YOffs1[16] = { STEP8(8*8*0,8), STEP8(8*8*1,8) };

	INT32 Plane2[2] = { 0, 4 };
	INT32 XOffs2[8] = { STEP4((0x2000*8),1), STEP4(0,1) };
	INT32 YOffs2[8] = { STEP8(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x18000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x18000);

	GfxDecode(0x0400, 3, 16, 16, Plane0, XOffs0, YOffs0, 0x100, tmp, DrvGfxROM0);

	for (INT32 i = 0; i < 16; i++) {
		memcpy (tmp + (((i & 3) << 2) | (i >> 2)) * 0x1000, DrvGfxROM1 + i * 0x1000, 0x1000);
	}

	GfxDecode(0x0200, 3, 16, 16, Plane1, XOffs1, YOffs1, 0x100, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2 + 0x04000, 0x04000);

	GfxDecode(0x0400, 2,  8,  8, Plane2, XOffs2, YOffs2, 0x040, tmp, DrvGfxROM2);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit()
{
//	BurnSetRefreshRate(58.00); // locks up with this...

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvMainROM + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x0a000,  1, 1)) return 1;

		if (BurnLoadRom(DrvSubROM  + 0x00000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  3, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x08000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x10000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x08000,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000,  8, 1)) return 1;

		DrvGfxDecode();
	}

	M6809Init(2);
	M6809Open(0);
	M6809MapMemory(DrvMainROM + 0x0000,	0x0000, 0x7fff, MAP_ROM);
	M6809MapMemory(DrvShareRAM,		0x8000, 0x9fff, MAP_RAM);
	M6809MapMemory(DrvMainROM + 0xa000,	0xa000, 0xbfff, MAP_ROM);
	M6809MapMemory(DrvPalRAM + 0x000,	0xc800, 0xc8ff, MAP_RAM);
	M6809MapMemory(DrvPalRAM + 0x100,	0xcc00, 0xccff, MAP_RAM);
	M6809MapMemory(DrvFgRAM,		0xd800,	0xdfff, MAP_RAM);
	M6809MapMemory(DrvSprRAM,		0xe800, 0xe9ff, MAP_RAM);
	M6809MapMemory(DrvMainROM + 0x7f00,	0xff00, 0xffff, MAP_ROM);
	M6809SetWriteHandler(metlclsh_main_write);
	M6809SetReadHandler(metlclsh_main_read);
	M6809Close();

	M6809Open(1);
	M6809MapMemory(DrvSubROM + 0x0000,	0x0000, 0x7fff, MAP_ROM);
	M6809MapMemory(DrvShareRAM,		0x8000, 0x9fff, MAP_RAM);
	M6809MapMemory(DrvBgRAM,		0xd000,	0xd7ff, MAP_RAM);
	M6809MapMemory(DrvSubROM + 0x7f00,	0xff00, 0xffff, MAP_ROM);
	M6809SetWriteHandler(metlclsh_sub_write);
	M6809SetReadHandler(metlclsh_main_read);
	M6809Close();

	BurnYM3526Init(3000000, &DrvYM3526IrqHandler, 0);
	BurnTimerAttachYM3526(&M6809Config, 1500000);
	BurnYM3526SetRoute(BURN_SND_YM3526_ROUTE, 0.50, BURN_SND_ROUTE_BOTH);

	BurnYM2203Init(1, 1500000, NULL, 1);
	BurnTimerAttachM6809(1500000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE, 0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.10, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.10, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.10, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, bg_map_scan,        bg_map_callback, 16, 16, 32, 16);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS,  fg_map_callback,  8,  8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM1, 3, 16, 16, 0x20000, 0x10, 0x00);	
	GenericTilemapSetGfx(1, DrvGfxROM2, 2,  8,  8, 0x10000, 0x20, 0x03);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);
	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetTransparent(1, 0);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6809Exit();

	BurnYM2203Exit();
	BurnYM3526Exit();

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteUpdate()
{
	for (INT32 i = 0; i < 0x30; i++)
	{
		UINT8 r = DrvPalRAM[0x000 + i] & 0xf;
		UINT8 g = DrvPalRAM[0x000 + i] >> 4;
		UINT8 b = DrvPalRAM[0x100 + i] & 0xf;

		DrvPalette[i] = BurnHighCol(r+r*16,g+g*16,b+b*16, 0);
	}
}

static void draw_single_sprite(INT32 code, INT32 color, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy)
{
	sy -= 16;

	if (flipy) {
		if (flipx) {
			Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, DrvGfxROM0);
		} else {
			Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, DrvGfxROM0);
		}
	} else {
		if (flipx) {
			Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, DrvGfxROM0);
		} else {
			Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, DrvGfxROM0);
		}
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0; offs < 0x200; offs += 4)
	{
		INT32 attr = DrvSprRAM[offs];
		if ((attr & 0x01) == 0) continue;

		INT32 flipy = (attr & 0x02);
		INT32 flipx = (attr & 0x04);
		INT32 color = (attr & 0x08) >> 3;
		INT32 sizey = (attr & 0x10);  // double height
		INT32 code = ((attr & 0x60) << 3) + DrvSprRAM[offs + 1];

		INT32 sx = 240 - DrvSprRAM[offs + 3];
		if (sx < -7) sx += 256;

		INT32 sy = 240 - DrvSprRAM[offs + 2];

		if (flipscreen)
		{
			sx = 240 - sx;  flipx = !flipx;
			sy = 240 - sy;  flipy = !flipy;
			if (sizey) sy += 16;
			if (sy > 240) sy -= 256;
		}

		for (INT32 wrapy = 0; wrapy <= 256; wrapy += 256)
		{
			if (sizey)
			{
				draw_single_sprite(code & ~1, color, sx, sy + (flipy ? 0 : -16) + wrapy, flipx, flipy);
				draw_single_sprite(code |  1, color, sx, sy + (flipy ? -16 : 0) + wrapy, flipx, flipy);
			}
			else
			{
				draw_single_sprite(code, color, sx, sy + wrapy, flipx, flipy);
			}
		}
	}
}

static INT32 DrvDraw()
{
	DrvPaletteUpdate();

	BurnTransferClear(0x10);

	GenericTilemapSetFlip(1, flipscreen ? (TMAP_FLIPX|TMAP_FLIPY):0);

	GenericTilemapDraw(1, pTransDraw, TMAP_SET_GROUP(1));

	if (scrollx[0] & 0x08)
	{
		INT32 scroll = scrollx[1] + ((scrollx[0] & 0x02) << 7);
		GenericTilemapSetFlip(0, flipscreen ? (TMAP_FLIPX|TMAP_FLIPY) : TMAP_FLIPX);
		GenericTilemapSetScrollX(0, flipscreen ? -scroll : scroll);
		GenericTilemapDraw(0, pTransDraw, 0);
	}

	draw_sprites();

	GenericTilemapDraw(1, pTransDraw, TMAP_SET_GROUP(0));

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	M6809NewFrame();

	{
		UINT8 prev[2];
		prev[0] = DrvInputs[1] & 0xc0;
		prev[1] = DrvInputs[2] & 0x40;

		memset (DrvInputs, 0xff, 3);

		DrvInputs[2] = (DrvDips[0] & 0x1f) | 0x40;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		// coins trigger nmi on sub cpu
		M6809Open(1);
		if (prev[0] == 0xc0 && (DrvInputs[1] & 0xc0) != 0xc0) {
			M6809SetIRQLine(0x20, CPU_IRQSTATUS_ACK);
		}
		if (prev[1] == 0x40 && (DrvInputs[2] & 0x40) != 0x40) {
			M6809SetIRQLine(0x20, CPU_IRQSTATUS_ACK);
		}
		M6809Close();
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] =  { (1500000 * 100) / nBurnFPS, (1500000 * 100) / nBurnFPS };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		vblank = (i >= 240) ? 1 : 0;

		M6809Open(0);	
		BurnTimerUpdateYM3526((i + 1) * (nCyclesTotal[0] / nInterleave));
		M6809Close();

		M6809Open(1);
		BurnTimerUpdate((i + 1) * (nCyclesTotal[1] / nInterleave));
		M6809Close();
	}

	M6809Open(1);
	BurnTimerEndFrame(nCyclesTotal[1]);
	M6809Close();

	M6809Open(0);
	BurnTimerEndFrameYM3526(nCyclesTotal[0]);

	if (pBurnSoundOut) {
		BurnYM3526Update(pBurnSoundOut, nBurnSoundLen);
		M6809Close();

		M6809Open(1);
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
	}

	M6809Close();
	
	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029707;
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
		M6809Scan(nAction);

		M6809Open(0);
		BurnYM3526Scan(nAction, pnMin);
		BurnYM2203Scan(nAction, pnMin);
		M6809Close();

		SCAN_VAR(flipscreen);
		SCAN_VAR(scrollx);
		SCAN_VAR(bankdata);
		SCAN_VAR(gfxbank);
	}

	if (nAction & ACB_WRITE)
	{
		M6809Open(1);
		bankswitch(bankdata);
		M6809Close();
	}

	return 0;
}


// Metal Clash (Japan)

static struct BurnRomInfo metlclshRomDesc[] = {
	{ "cs04.bin",	0x8000, 0xc2cc79a6, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "cs00.bin",	0x2000, 0xaf0f2998, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "cs03.bin",	0x8000, 0x51c4720c, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 #1 Code

	{ "cs06.bin",	0x8000, 0x9f61403f, 3 | BRF_GRA },           //  3 Sprites
	{ "cs07.bin",	0x8000, 0xd0610ea5, 3 | BRF_GRA },           //  4
	{ "cs08.bin",	0x8000, 0xa8b02125, 3 | BRF_GRA },           //  5

	{ "cs01.bin",	0x8000, 0x9c72343d, 4 | BRF_GRA },           //  6 Background Tiles
	{ "cs02.bin",	0x8000, 0x3674673e, 4 | BRF_GRA },           //  7

	{ "cs05.bin",	0x8000, 0xf90c9c6b, 5 | BRF_GRA },           //  8 Foreground Tiles

	{ "82s123.prm",	0x0020, 0x6844cc88, 0 | BRF_OPT },           //  9 Unknown PROM
};

STD_ROM_PICK(metlclsh)
STD_ROM_FN(metlclsh)

struct BurnDriver BurnDrvMetlclsh = {
	"metlclsh", NULL, NULL, NULL, "1985",
	"Metal Clash (Japan)\0", NULL, "Data East", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PREFIX_DATAEAST, GBF_SCRFIGHT, 0,
	NULL, metlclshRomInfo, metlclshRomName, NULL, NULL, NULL, NULL, MetlclshInputInfo, MetlclshDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x30,
	256, 224, 4, 3
};
