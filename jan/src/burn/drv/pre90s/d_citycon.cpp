// FB Alpha City Connection driver module
// Based on MAME driver by Mirko Buffoni

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "burn_ym2203.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6809ROM0;
static UINT8 *DrvM6809ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvMapROM;
static UINT8 *DrvM6809RAM0;
static UINT8 *DrvVidRAM;
static UINT8 *DrvLineRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvM6809RAM1;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 *soundlatch;
static UINT8 bgimage;
static UINT8 flipscreen;
static UINT16 scrollx;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvReset;
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];

static struct BurnInputInfo CityconInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Citycon)

static struct BurnDIPInfo CityconDIPList[]=
{
	{0x10, 0xff, 0xff, 0x00, NULL			},
	{0x11, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x10, 0x01, 0x03, 0x00, "3"			},
	{0x10, 0x01, 0x03, 0x01, "4"			},
	{0x10, 0x01, 0x03, 0x02, "5"			},
	{0x10, 0x01, 0x03, 0x03, "Infinite (Cheat)"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x10, 0x01, 0x20, 0x20, "Off"			},
	{0x10, 0x01, 0x20, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x10, 0x01, 0x40, 0x00, "Upright"		},
	{0x10, 0x01, 0x40, 0x40, "Cocktail"		},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x11, 0x01, 0x07, 0x07, "5 Coins 1 Credits"	},
	{0x11, 0x01, 0x07, 0x06, "4 Coins 1 Credits"	},
	{0x11, 0x01, 0x07, 0x05, "3 Coins 1 Credits"	},
	{0x11, 0x01, 0x07, 0x04, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x07, 0x00, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x07, 0x01, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0x07, 0x02, "1 Coin  3 Credits"	},
	{0x11, 0x01, 0x07, 0x03, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x11, 0x01, 0x08, 0x00, "Easy"			},
	{0x11, 0x01, 0x08, 0x08, "Hard"			},
};

STDDIPINFO(Citycon)

static void citycon_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf800) == 0x2800) return;

	switch (address)
	{
		case 0x3000:
			bgimage = (data >> 4) & 0xf;
			flipscreen = data & 0x01;
		return;

		case 0x3001:
			soundlatch[0] = data;
		return;

		case 0x3002:
			soundlatch[1] = data;
		return;

		case 0x3004:
			scrollx = (scrollx & 0x00ff) | (data * 256);
		return;

		case 0x3005:
			scrollx = (scrollx & 0xff00) | data;
		return;
	}
}

static UINT8 citycon_main_read(UINT16 address)
{
	switch (address)
	{
		case 0x3000:
			return DrvInputs[flipscreen & 1];

		case 0x3001:
			return (DrvDips[0] & 0x7f) | (DrvInputs[2] & 0x80);

		case 0x3002:
			return DrvDips[1];

		case 0x3007:
			M6809SetIRQLine(0, CPU_IRQSTATUS_NONE);
			return 0;
	}
	
	return 0;
}

static void citycon_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x4000:
		case 0x4001:
			BurnYM2203Write(1, address & 1, data);
		return;

		case 0x6000:
		case 0x6001:
			BurnYM2203Write(0, address & 1, data);
		return;
	}
}

static UINT8 citycon_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x6000:
		case 0x6001:
			return BurnYM2203Read(0, address & 1);
	}

	return 0;
}

static UINT8 citycon_ym2203_portA(UINT32 )
{
	return soundlatch[0];
}

static UINT8 citycon_ym2203_portB(UINT32 )
{
	return soundlatch[1];
}

inline static INT32 DrvSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)M6809TotalCycles() * nSoundRate / 640000;
}

inline static double DrvGetTime()
{
	return (double)M6809TotalCycles() / 640000.0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	M6809Open(0);
	M6809Reset();
	M6809Close();

	M6809Open(1);
	M6809Reset();
	M6809Close();

	BurnYM2203Reset();

	bgimage = 0;
	flipscreen = 0;
	scrollx = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next;		Next = AllMem;

	DrvM6809ROM0	= Next; Next += 0x00c000;
	DrvM6809ROM1	= Next; Next += 0x008000;

	DrvGfxROM0		= Next; Next += 0x0004000;
	DrvGfxROM1		= Next; Next += 0x0008000;
	DrvGfxROM2		= Next; Next += 0x0040000;

	DrvMapROM		= Next; Next += 0x00e0000;

	DrvPalette		= (UINT32*)Next; Next += 0x0800 * sizeof(UINT32);

	AllRam			= Next;

	DrvM6809RAM0	= Next; Next += 0x001000;
	DrvVidRAM		= Next; Next += 0x001000;
	DrvLineRAM		= Next; Next += 0x000100;
	DrvSprRAM		= Next; Next += 0x000100;
	DrvPalRAM		= Next; Next += 0x000500;
	DrvM6809RAM1	= Next; Next += 0x001000;

	soundlatch		= Next; Next += 0x000002;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void DrvGfx0Expand()
{
	UINT8 *rom = DrvGfxROM0;

	for (INT32 i = 0x0fff; i >= 0; i--)
	{
		rom[3 * i] = rom[i];
		rom[3 * i + 1] = 0;
		rom[3 * i + 2] = 0;
		INT32 mask = rom[i] | (rom[i] << 4) | (rom[i] >> 4);
		if (i & 0x01) rom[3 * i + 1] |= mask & 0xf0;
		if (i & 0x02) rom[3 * i + 1] |= mask & 0x0f;
		if (i & 0x04) rom[3 * i + 2] |= mask & 0xf0;
	}
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[5] = { 16, 12, 8, 4, 0 };
	INT32 Plane1[4] = { 4, 0, 0x10004, 0x10000 };
	INT32 Plane2[4] = { 4, 0, 0x60004, 0x60000 };
	INT32 XOffs0[8] = { STEP4(0,1), STEP4(0xc000,1) };
	INT32 YOffs0[8] = { STEP8(0,24) };
	INT32 XOffs1[8] = { STEP4(0,1), STEP4(0x4000,1) };
	INT32 YOffs1[16]= { STEP16(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x20000);
	if (tmp == NULL) {
		return 1;
	}

	DrvGfx0Expand();

	memcpy (tmp, DrvGfxROM0, 0x03000);

	GfxDecode(0x0100, 5,  8,  8, Plane0, XOffs0, YOffs0, 0x0c0, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x04000);

	GfxDecode(0x080, 4,  8, 16, Plane1, XOffs1, YOffs1, 0x080, tmp, DrvGfxROM1);
	GfxDecode(0x080, 4,  8, 16, Plane1, XOffs1, YOffs1, 0x080, tmp + 0x1000, DrvGfxROM1 + (8 * 16 * 0x80));

	memcpy (tmp, DrvGfxROM2, 0x20000);

	GfxDecode(0x0100, 4,  8,  8, Plane2, XOffs1, YOffs1, 0x040, tmp + 0x00000, DrvGfxROM2 + (8 * 8 * 0x100 * 0));
	GfxDecode(0x0100, 4,  8,  8, Plane2, XOffs1, YOffs1, 0x040, tmp + 0x01000, DrvGfxROM2 + (8 * 8 * 0x100 * 1));
	GfxDecode(0x0100, 4,  8,  8, Plane2, XOffs1, YOffs1, 0x040, tmp + 0x02000, DrvGfxROM2 + (8 * 8 * 0x100 * 2));
	GfxDecode(0x0100, 4,  8,  8, Plane2, XOffs1, YOffs1, 0x040, tmp + 0x03000, DrvGfxROM2 + (8 * 8 * 0x100 * 3));
	GfxDecode(0x0100, 4,  8,  8, Plane2, XOffs1, YOffs1, 0x040, tmp + 0x04000, DrvGfxROM2 + (8 * 8 * 0x100 * 4));
	GfxDecode(0x0100, 4,  8,  8, Plane2, XOffs1, YOffs1, 0x040, tmp + 0x05000, DrvGfxROM2 + (8 * 8 * 0x100 * 5));
	GfxDecode(0x0100, 4,  8,  8, Plane2, XOffs1, YOffs1, 0x040, tmp + 0x06000, DrvGfxROM2 + (8 * 8 * 0x100 * 6));
	GfxDecode(0x0100, 4,  8,  8, Plane2, XOffs1, YOffs1, 0x040, tmp + 0x07000, DrvGfxROM2 + (8 * 8 * 0x100 * 7));
	GfxDecode(0x0100, 4,  8,  8, Plane2, XOffs1, YOffs1, 0x040, tmp + 0x08000, DrvGfxROM2 + (8 * 8 * 0x100 * 8));
	GfxDecode(0x0100, 4,  8,  8, Plane2, XOffs1, YOffs1, 0x040, tmp + 0x09000, DrvGfxROM2 + (8 * 8 * 0x100 * 9));
	GfxDecode(0x0100, 4,  8,  8, Plane2, XOffs1, YOffs1, 0x040, tmp + 0x0a000, DrvGfxROM2 + (8 * 8 * 0x100 * 10));
	GfxDecode(0x0100, 4,  8,  8, Plane2, XOffs1, YOffs1, 0x040, tmp + 0x0b000, DrvGfxROM2 + (8 * 8 * 0x100 * 11));

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
		if (BurnLoadRom(DrvM6809ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM0 + 0x04000,  1, 1)) return 1;

		if (BurnLoadRom(DrvM6809ROM1 + 0x00000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0   + 0x00000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1   + 0x00000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1   + 0x02000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2   + 0x00000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2   + 0x08000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2   + 0x0c000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2   + 0x14000,  9, 1)) return 1;

		if (BurnLoadRom(DrvMapROM    + 0x00000, 10, 1)) return 1;
		if (BurnLoadRom(DrvMapROM    + 0x08000, 11, 1)) return 1;
		if (BurnLoadRom(DrvMapROM    + 0x0c000, 12, 1)) return 1;

		DrvGfxDecode();
	}

	M6809Init(2);
	M6809Open(0);
	M6809MapMemory(DrvM6809RAM0,	0x0000, 0x0fff, MAP_RAM);
	M6809MapMemory(DrvVidRAM,	0x1000, 0x1fff, MAP_RAM);
	M6809MapMemory(DrvLineRAM,	0x2000, 0x20ff, MAP_RAM);
	M6809MapMemory(DrvSprRAM,	0x2800, 0x28ff, MAP_RAM);
	M6809MapMemory(DrvPalRAM,	0x3800, 0x3cff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM0,	0x4000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(citycon_main_write);
	M6809SetReadHandler(citycon_main_read);
	M6809Close();

	M6809Open(1);
	M6809MapMemory(DrvM6809RAM1,	0x0000, 0x0fff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM1,	0x8000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(citycon_sound_write);
	M6809SetReadHandler(citycon_sound_read);
	M6809Close();

	BurnYM2203Init(2, 1250000, NULL, DrvSynchroniseStream, DrvGetTime, 0);
	BurnYM2203SetPorts(0, &citycon_ym2203_portA, &citycon_ym2203_portB, NULL, NULL);
	BurnTimerAttachM6809(640000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE, 0.40, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.25, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.25, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.25, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_YM2203_ROUTE, 0.40, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_1, 0.25, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_2, 0.25, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_3, 0.25, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6809Exit();

	BurnYM2203Exit();

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteRecalc()
{
	for (INT32 i = 0; i < 0x500 / 2; i++) {
		INT32 b = DrvPalRAM[i*2+1] >> 4;
		INT32 g = DrvPalRAM[i*2+0] & 0x0f;
		INT32 r = DrvPalRAM[i*2+0] >> 4;

		DrvPalette[i] = BurnHighCol(r+r*16, g+g*16, b+b*16, 0);
	}

	for (INT32 i = 0; i < 0x100; i++) {
		INT32 index = DrvLineRAM[i];

		for (INT32 j = 0; j < 4; j++) {
			DrvPalette[0x400 + (i * 4) + j] = DrvPalette[0x200 + ((index*4+j)&0x7f)];
		}
	}
}

static void draw_fg_layer()
{
	UINT16 xscroll = scrollx & 0x3ff;

	for (INT32 offs = 0; offs < 128 * 32; offs++)
	{
		INT32 sx = (offs & 0x7f) * 8;
		INT32 sy = (offs / 0x80) * 8;

		sy -= 16; // offsets
		sx -= 8;

		if (sy >= (6 * 8)) sx -= xscroll;
		if (sx < -7) sx += 1024;
		if (sx >= nScreenWidth) continue;

		INT32 ofst = (offs & 0x1f) + (((offs/0x80) + (offs & 0x60)) * 0x20);
		INT32 code = DrvVidRAM[ofst];

		Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy, ofst & 0x3e0, 0/*5*/, 0, 0x400, DrvGfxROM0);
	}
}

static void draw_bg_layer()
{
	UINT16 xscroll = (scrollx / 2) & 0x3ff;

	for (INT32 offs = 0; offs < 128 * 32; offs++)
	{
		INT32 sx = (offs & 0x7f) * 8;
		INT32 sy = (offs / 0x80) * 8;

		sy -= 16;

		sx -= xscroll;
		if (sx < -7) sx += 1024;
		if (sx >= nScreenWidth) continue;

		INT32 ofst = (offs & 0x1f) + (((offs/0x80) + (offs & 0x60)) * 0x20);

		INT32 code  = DrvMapROM[0x1000 * bgimage + ofst] + (bgimage * 0x100);
		INT32 color = DrvMapROM[0xc000 + code] & 0x0f;

		Render8x8Tile_Clip(pTransDraw, code, sx, sy, color, 4, 0x100, DrvGfxROM2);
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0x100 - 4; offs >= 0; offs -= 4)
	{
		INT32 sx = DrvSprRAM[offs + 3];
		INT32 sy = 239 - DrvSprRAM[offs];
		INT32 flipx = ~DrvSprRAM[offs + 2] & 0x10;

		sy -= (16 - 2);
		sx -= 8;

		if (flipscreen)
		{
			sx = 240 - sx;
			sy = 238 - sy;
			flipx = !flipx;
		}

		INT32 code = DrvSprRAM[offs + 1];
		INT32 color = DrvSprRAM[offs + 2] & 0x0f;

		if (flipscreen) {
			if (flipx) {
				RenderCustomTile_Mask_FlipXY_Clip(pTransDraw, 8, 16, code, sx, sy, color, 4, 0, 0, DrvGfxROM1);
			} else {
				RenderCustomTile_Mask_FlipY_Clip(pTransDraw, 8, 16, code, sx, sy, color, 4, 0, 0, DrvGfxROM1);
			}
		} else {
			if (flipx) {
				RenderCustomTile_Mask_FlipX_Clip(pTransDraw, 8, 16, code, sx, sy, color, 4, 0, 0, DrvGfxROM1);
			} else {
				RenderCustomTile_Mask_Clip(pTransDraw, 8, 16, code, sx, sy, color, 4, 0, 0, DrvGfxROM1);
			}
		}
	}
}

static INT32 DrvDraw()
{
	DrvPaletteRecalc();

	draw_bg_layer();
	draw_fg_layer();
	draw_sprites();

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
		memset (DrvInputs, 0xff, 3);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 10;
	INT32 nCyclesTotal[2] =  { 2048000 / 60, 640000 / 60 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		M6809Open(0);	
		M6809Run(nCyclesTotal[0] / nInterleave);
		if (i == (nInterleave - 1)) M6809SetIRQLine(0, CPU_IRQSTATUS_ACK);
		M6809Close();

		M6809Open(1);
		BurnTimerUpdate(i * (nCyclesTotal[1] / nInterleave));
		if (i == (nInterleave - 1)) M6809SetIRQLine(0, CPU_IRQSTATUS_AUTO);
		M6809Close();
	}

	M6809Open(1);

	BurnTimerEndFrame(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
	}

	M6809Close();
	
	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction,INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029706;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		M6809Scan(nAction);

		BurnYM2203Scan(nAction, pnMin);

		SCAN_VAR(bgimage);
		SCAN_VAR(flipscreen);
		SCAN_VAR(scrollx);
	}

	return 0;
}


// City Connection (set 1)

static struct BurnRomInfo cityconRomDesc[] = {
	{ "c10",	0x4000, 0xae88b53c, 1 | BRF_PRG | BRF_ESS }, //  0 m6809 #0 Code
	{ "c11",	0x8000, 0x139eb1aa, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "c1",		0x8000, 0x1fad7589, 2 | BRF_PRG | BRF_ESS }, //  2 m6809 #1 Code

	{ "c4",		0x2000, 0xa6b32fc6, 3 | BRF_GRA },           //  3 Characters

	{ "c12",	0x2000, 0x08eaaccd, 4 | BRF_GRA },           //  4 Background Tiles
	{ "c13",	0x2000, 0x1819aafb, 4 | BRF_GRA },           //  5

	{ "c9",		0x8000, 0x8aeb47e6, 5 | BRF_GRA },           //  6 Sprites
	{ "c8",		0x4000, 0x0d7a1eeb, 5 | BRF_GRA },           //  7
	{ "c6",		0x8000, 0x2246fe9d, 5 | BRF_GRA },           //  8
	{ "c7",		0x4000, 0xe8b97de9, 5 | BRF_GRA },           //  9

	{ "c2",		0x8000, 0xf2da4f23, 6 | BRF_GRA },           // 10 Background Tile Map
	{ "c3",		0x4000, 0x7ef3ac1b, 6 | BRF_GRA },           // 11
	{ "c5",		0x2000, 0xc03d8b1b, 6 | BRF_GRA },           // 12
};

STD_ROM_PICK(citycon)
STD_ROM_FN(citycon)

struct BurnDriver BurnDrvCitycon = {
	"citycon", NULL, NULL, NULL, "1985",
	"City Connection (set 1)\0", NULL, "Jaleco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, cityconRomInfo, cityconRomName, NULL, NULL, CityconInputInfo, CityconDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 224, 4, 3
};


// City Connection (set 2)

static struct BurnRomInfo cityconaRomDesc[] = {
	{ "c10",	0x4000, 0xae88b53c, 1 | BRF_PRG | BRF_ESS }, //  0 m6809 #0 Code
	{ "c11b",	0x8000, 0xd64af468, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "c1",		0x8000, 0x1fad7589, 2 | BRF_PRG | BRF_ESS }, //  2 m6809 #1 Code

	{ "c4",		0x2000, 0xa6b32fc6, 3 | BRF_GRA },           //  3 Characters

	{ "c12",	0x2000, 0x08eaaccd, 4 | BRF_GRA },           //  4 Background Tiles
	{ "c13",	0x2000, 0x1819aafb, 4 | BRF_GRA },           //  5

	{ "c9",		0x8000, 0x8aeb47e6, 5 | BRF_GRA },           //  6 Sprites
	{ "c8",		0x4000, 0x0d7a1eeb, 5 | BRF_GRA },           //  7
	{ "c6",		0x8000, 0x2246fe9d, 5 | BRF_GRA },           //  8
	{ "c7",		0x4000, 0xe8b97de9, 5 | BRF_GRA },           //  9

	{ "c2",		0x8000, 0xf2da4f23, 6 | BRF_GRA },           // 10 Background Tile Map
	{ "c3",		0x4000, 0x7ef3ac1b, 6 | BRF_GRA },           // 11
	{ "c5",		0x2000, 0xc03d8b1b, 6 | BRF_GRA },           // 12
};

STD_ROM_PICK(citycona)
STD_ROM_FN(citycona)

struct BurnDriver BurnDrvCitycona = {
	"citycona", "citycon", NULL, NULL, "1985",
	"City Connection (set 2)\0", NULL, "Jaleco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, cityconaRomInfo, cityconaRomName, NULL, NULL, CityconInputInfo, CityconDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 224, 4, 3
};


// Cruisin

static struct BurnRomInfo cruisinRomDesc[] = {
	{ "cr10",	0x4000, 0xcc7c52f3, 1 | BRF_PRG | BRF_ESS }, //  0 m6809 #0 Code
	{ "cr11",	0x8000, 0x5422f276, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "c1",		0x8000, 0x1fad7589, 2 | BRF_PRG | BRF_ESS }, //  2 m6809 #1 Code

	{ "cr4",	0x2000, 0x8cd0308e, 3 | BRF_GRA },           //  3 Characters

	{ "c12",	0x2000, 0x08eaaccd, 4 | BRF_GRA },           //  4 Background Tiles
	{ "c13",	0x2000, 0x1819aafb, 4 | BRF_GRA },           //  5

	{ "c9",		0x8000, 0x8aeb47e6, 5 | BRF_GRA },           //  6 Sprites
	{ "c8",		0x4000, 0x0d7a1eeb, 5 | BRF_GRA },           //  7
	{ "c6",		0x8000, 0x2246fe9d, 5 | BRF_GRA },           //  8
	{ "c7",		0x4000, 0xe8b97de9, 5 | BRF_GRA },           //  9

	{ "c2",		0x8000, 0xf2da4f23, 6 | BRF_GRA },           // 10 Background Tile Map
	{ "c3",		0x4000, 0x7ef3ac1b, 6 | BRF_GRA },           // 11
	{ "c5",		0x2000, 0xc03d8b1b, 6 | BRF_GRA },           // 12
};

STD_ROM_PICK(cruisin)
STD_ROM_FN(cruisin)

struct BurnDriver BurnDrvCruisin = {
	"cruisin", "citycon", NULL, NULL, "1985",
	"Cruisin\0", NULL, "Jaleco (Kitkorp license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, cruisinRomInfo, cruisinRomName, NULL, NULL, CityconInputInfo, CityconDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 224, 4, 3
};
