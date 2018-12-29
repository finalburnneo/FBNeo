// FB Alpha Stadium Hero driver module
// Based on MAME driver by Bryan McPhail

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "m6502_intf.h"
#include "decobac06.h"
#include "msm6295.h"
#include "burn_ym2203.h"
#include "burn_ym3812.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvM6502ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvM6502RAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvTxtRAM;
static UINT8 *DrvPalRAM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvSprRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 soundlatch;
static UINT16 DrvVidCtrl[2][4];

static INT32 vblank;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT16 DrvInputs[2];
static UINT8 DrvDips[2];
static UINT8 DrvReset;

static struct BurnInputInfo StadheroInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 4,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 15,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 6,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Stadhero)

static struct BurnDIPInfo StadheroDIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL					},
	{0x15, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    4, "Coin A"				},
	{0x14, 0x01, 0x03, 0x00, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0x03, 0x01, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x14, 0x01, 0x0c, 0x00, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0x0c, 0x04, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x14, 0x01, 0x20, 0x00, "Off"					},
	{0x14, 0x01, 0x20, 0x20, "On"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x14, 0x01, 0x40, 0x40, "Off"					},
	{0x14, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Time (1P Vs CPU)"		},
	{0x15, 0x01, 0x03, 0x02, "600"					},
	{0x15, 0x01, 0x03, 0x03, "500"					},
	{0x15, 0x01, 0x03, 0x01, "450"					},
	{0x15, 0x01, 0x03, 0x00, "400"					},

	{0   , 0xfe, 0   ,    4, "Time (1P Vs 2P)"		},
	{0x15, 0x01, 0x0c, 0x08, "270"					},
	{0x15, 0x01, 0x0c, 0x0c, "210"					},
	{0x15, 0x01, 0x0c, 0x04, "180"					},
	{0x15, 0x01, 0x0c, 0x00, "120"					},

	{0   , 0xfe, 0   ,    4, "Final Set"			},
	{0x15, 0x01, 0x30, 0x20, "3 Credits"			},
	{0x15, 0x01, 0x30, 0x30, "4 Credits"			},
	{0x15, 0x01, 0x30, 0x10, "5 Credits"			},
	{0x15, 0x01, 0x30, 0x00, "6 Credits"			},
};

STDDIPINFO(Stadhero)

static void __fastcall stadhero_main_write_word(UINT32 address, UINT16 data)
{	
	if ((address & 0xffffe8) == 0x240000) {
		DrvVidCtrl[(address>>4)&1][(address / 2) & 3] = data;
		return;
	}

	switch (address)
	{
		case 0x30c006:
			soundlatch = data & 0xff;
			M6502SetIRQLine(0x20, CPU_IRQSTATUS_AUTO); // pulse
		return;
	}
}

static void __fastcall stadhero_main_write_byte(UINT32 address, UINT8 data)
{	
	if ((address & 0xffffe8) == 0x240000) {
		bprintf (0, _T("write byte to video control!\n"));
	//	DrvVidCtrl[(address>>4)&1][(address / 2) & 3] = data;
		return;
	}

	switch (address)
	{
		case 0x30c007:
			soundlatch = data & 0xff;
			M6502SetIRQLine(0x20, CPU_IRQSTATUS_AUTO); // pulse
		return;
	}
}

static UINT16 __fastcall stadhero_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x30c000:
		case 0x30c001:
			return DrvInputs[0];

		case 0x30c004:
		case 0x30c005:
			return DrvDips[0] + (DrvDips[1] * 256);

		case 0x30c002:
		case 0x30c003:
			return (DrvInputs[1] & 0x7f) | ((DrvInputs[1] & 0x7f) << 8) | (vblank ? 0x8080 : 0);
	}

	return 0;
}

static UINT8 __fastcall stadhero_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x30c000:
			return DrvInputs[0] >> 8;

		case 0x30c001:
			return DrvInputs[0];

		case 0x30c004:
			return DrvDips[1];

		case 0x30c005:
			return DrvDips[0];

		case 0x30c002:
		case 0x30c003:
			return (DrvInputs[1] & 0x7f) | (vblank ? 0x80 : 0);
	}

	return 0;
}

static void stadhero_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x0800:
		case 0x0801:
			BurnYM2203Write(0, address & 1, data);
		return;

		case 0x1000:
		case 0x1001:
			BurnYM3812Write(0, address & 1, data);
		return;

		case 0x3800:
			MSM6295Write(0, data);
	}
}

static UINT8 stadhero_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x3000:
			return soundlatch;

		case 0x3800:
			return MSM6295Read(0);
	}

	return 0;
}

static void DrvYM3812FMIRQHandler(INT32, INT32 nStatus)
{
	if (M6502GetActive() == -1) return;

	M6502SetIRQLine(0, nStatus ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static tilemap_callback( txt )
{
	UINT16 *ram = (UINT16*)DrvTxtRAM;

	UINT16 attr = ram[offs];

	TILE_SET_INFO(0, attr, attr >> 12, 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	BurnYM2203Reset();
	SekClose();

	M6502Open(0);
	M6502Reset();
	BurnYM3812Reset();
	M6502Close();

	MSM6295Reset(0);

	soundlatch = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x020000;
	DrvM6502ROM		= Next; Next += 0x008000;

	DrvGfxROM0		= Next; Next += 0x040000;
	DrvGfxROM1		= Next; Next += 0x080000;
	DrvGfxROM2		= Next; Next += 0x100000;

	MSM6295ROM		= Next; Next += 0x040000;

	DrvPalette		= (UINT32*)Next; Next += 0x400 * sizeof(UINT32);

	AllRam			= Next;

	DrvM6502RAM		= Next; Next += 0x000600;
	DrvVidRAM		= Next; Next += 0x002000;
	DrvTxtRAM		= Next; Next += 0x000800;
	DrvPalRAM		= Next; Next += 0x000800;
	Drv68KRAM		= Next; Next += 0x00c000;
	DrvSprRAM		= Next; Next += 0x000800;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[3] = { 0x8000*0*8, 0x8000*1*8, 0x8000*2*8 };
	INT32 Plane1[3] = { 0x10000*2*8, 0x10000*1*8, 0x10000*0*8 };
	INT32 Plane2[4] = { STEP4(0x60000*8, -(0x20000*8)) };
	INT32 XOffs[16] = { STEP8(128,1), STEP8(0,1) };
	INT32 YOffs[16] = { STEP16(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x80000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x18000);

	GfxDecode(0x1000, 3,  8,  8, Plane0, XOffs + 8, YOffs, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x30000);

	GfxDecode(0x0800, 3, 16, 16, Plane1, XOffs + 0, YOffs, 0x100, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x80000);

	GfxDecode(0x1000, 4, 16, 16, Plane2, XOffs + 0, YOffs, 0x100, tmp, DrvGfxROM2);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit()
{
	BurnSetRefreshRate(58.00);

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;

		if (BurnLoadRom(DrvM6502ROM  + 0x0000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  3, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x010000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x010000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x020000,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x000000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x010000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x020000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x030000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x040000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x050000, 13, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x060000, 14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x070000, 15, 1)) return 1;

		if (BurnLoadRom(MSM6295ROM + 0x000000, 16, 1)) return 1;

		DrvGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x01ffff, MAP_ROM);
	SekMapMemory(DrvTxtRAM,			0x200000, 0x2007ff, MAP_RAM);
	SekMapMemory(DrvVidRAM,			0x260000, 0x261fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x310000, 0x3107ff, MAP_RAM);
	SekMapMemory(Drv68KRAM,			0xff8000, 0xffbfff, MAP_RAM);
	SekMapMemory(DrvSprRAM,			0xffc000, 0xffc7ff, MAP_RAM);
	SekMapMemory(DrvSprRAM,			0xffc800, 0xffcfff, MAP_RAM);
	SekSetWriteWordHandler(0,		stadhero_main_write_word);
	SekSetWriteByteHandler(0,		stadhero_main_write_byte);
	SekSetReadWordHandler(0,		stadhero_main_read_word);
	SekSetReadByteHandler(0,		stadhero_main_read_byte);
	SekClose();

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvM6502RAM,		0x0000, 0x05ff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM,		0x8000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(stadhero_sound_write);
	M6502SetReadHandler(stadhero_sound_read);
	M6502Close();

	BurnYM3812Init(1, 3000000, &DrvYM3812FMIRQHandler, 0);
	BurnTimerAttachYM3812(&M6502Config, 1500000);
	BurnYM3812SetRoute(0, BURN_SND_YM3812_ROUTE, 0.80, BURN_SND_ROUTE_BOTH);

	BurnYM2203Init(1, 1500000, NULL, 1);
	BurnTimerAttach(&SekConfig, 10000000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE, 0.30, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.43, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.43, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.43, BURN_SND_ROUTE_BOTH);

	MSM6295Init(0, 1056000 / MSM6295_PIN7_HIGH, 1);
	MSM6295SetRoute(0, 0.80, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, txt_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 3, 8, 8, 0x40000, 0, 0xf);
	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetOffsets(0, 0, -8);

	bac06_depth = 3;
	bac06_yadjust = 8;

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	SekExit();
	M6502Exit();

	BurnYM2203Exit();
	BurnYM3812Exit();
	MSM6295Exit();

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteUpdate()
{
	UINT16 *p = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x800 / 2; i++)
	{
		UINT8 r = (p[i] & 0x00f);
		UINT8 g = (p[i] & 0x0f0);
		UINT8 b = (p[i] >> 8) & 0xf;

		DrvPalette[i] = BurnHighCol(r+r*16, g+g/16, b+b*16, 0);
	}
}

static void draw_sprites()
{
	UINT16 *spriteram = (UINT16*)DrvSprRAM;

	for (INT32 offs = 0; offs < 0x800/2; offs+=4)
	{
		INT32 incy;
		INT32 sy = spriteram[offs];
		INT32 sx = spriteram[offs + 2];
		INT32 color = sx >> 12;
		INT32 flash = sx & 0x800;
		INT32 flipx = sy & 0x2000;
		INT32 flipy = sy & 0x4000;
		INT32 h = (1 << ((sy & 0x1800) >> 11));
		INT32 w = (1 << ((sy & 0x0600) >>  9));

		sx = sx & 0x01ff;
		sy = sy & 0x01ff;
		if (sx >= 256) sx -= 512;
		if (sy >= 256) sy -= 512;
		sx = 240 - sx;
		sy = 240 - sy;

		INT32 mult = -16;

		if ((spriteram[offs] & 0x8000) == 0) {
			continue;
		}

		for (INT32 x = 0; x < w; x++)
		{
			INT32 code = (spriteram[offs + 1] & 0x1fff) & ~(h - 1);

			if (spriteram[offs] & 0x4000)
				incy = -1;
			else
			{
				code += h - 1;
				incy = 1;
			}

			for (INT32 y = 0; y < h; y++)
			{
				if (!flash || (nCurrentFrame & 1))
				{
					Draw16x16MaskTile(pTransDraw, (code - y * incy) & 0xfff, sx + (mult * x), (sy + (mult * y)) - 8, flipx, flipy, color, 4, 0, 0x100, DrvGfxROM2);
				}
			}
		}
	}
}

static INT32 DrvDraw()
{
//	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
//	}

	BurnTransferClear();

	bac06_draw_layer(DrvVidRAM, DrvVidCtrl, NULL, NULL, DrvGfxROM0, 0, 0xfff, DrvGfxROM1, 0x200, 0x7ff, 2, 1);

	draw_sprites();
	
	GenericTilemapDraw(0, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	SekNewFrame();
	M6502NewFrame();

	{
		memset (DrvInputs, 0xff, 4);

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 32;
	INT32 nCyclesTotal[2] = { 10000000 / 58, 1500000 / 58 };

	SekOpen(0);
	M6502Open(0);

	vblank = 1;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		if (i == 1) vblank = 0;
		if (i == 30) {
			vblank = 1;
			SekSetIRQLine(5, CPU_IRQSTATUS_AUTO);
		}
		
		BurnTimerUpdate((i + 1) * (nCyclesTotal[0] / nInterleave));
		BurnTimerUpdateYM3812((i + 1) * (nCyclesTotal[1] / nInterleave));
	}

	BurnTimerEndFrame(nCyclesTotal[0]);
	BurnTimerEndFrameYM3812(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		BurnYM3812Update(pBurnSoundOut, nBurnSoundLen);
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
	}

	M6502Close();
	SekClose();

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
		SekScan(nAction);
		M6502Scan(nAction);

		BurnYM2203Scan(nAction, pnMin);
		BurnYM3812Scan(nAction, pnMin);
		MSM6295Scan(nAction, pnMin);
		
		SCAN_VAR(soundlatch);
		SCAN_VAR(DrvVidCtrl);
	}

	return 0;
}


// Stadium Hero (Japan)

static struct BurnRomInfo stadheroRomDesc[] = {
	{ "ef15.9a",	0x10000, 0xbbba364e, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "ef13.4e",	0x10000, 0x97c6717a, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ef18.7f",	0x08000, 0x20fd9668, 2 | BRF_PRG | BRF_ESS }, //  2 audiocpu

	{ "ef08.2j",	0x10000, 0xe84752fe, 3 | BRF_GRA },           //  3 gfx1
	{ "ef09.4j",	0x08000, 0x2ade874d, 3 | BRF_GRA },           //  4

	{ "ef11.13j",	0x10000, 0xaf563e96, 4 | BRF_GRA },           //  5 gfx2
	{ "ef10.11j",	0x10000, 0xdca3d599, 4 | BRF_GRA },           //  6
	{ "ef12.14j",	0x10000, 0x9a1bf51c, 4 | BRF_GRA },           //  7

	{ "ef00.2a",	0x10000, 0x94ed257c, 5 | BRF_GRA },           //  8 gfx3
	{ "ef01.4a",	0x10000, 0x6eb9a721, 5 | BRF_GRA },           //  9
	{ "ef02.5a",	0x10000, 0x850cb771, 5 | BRF_GRA },           // 10
	{ "ef03.7a",	0x10000, 0x24338b96, 5 | BRF_GRA },           // 11
	{ "ef04.8a",	0x10000, 0x9e3d97a7, 5 | BRF_GRA },           // 12
	{ "ef05.9a",	0x10000, 0x88631005, 5 | BRF_GRA },           // 13
	{ "ef06.11a",	0x10000, 0x9f47848f, 5 | BRF_GRA },           // 14
	{ "ef07.12a",	0x10000, 0x8859f655, 5 | BRF_GRA },           // 15

	{ "ef17.1e",	0x10000, 0x07c78358, 6 | BRF_SND },           // 16 oki

	{ "ef19.3d",	0x00200, 0x852ff668, 7 | BRF_GRA },           // 17 proms
};

STD_ROM_PICK(stadhero)
STD_ROM_FN(stadhero)

struct BurnDriver BurnDrvStadhero = {
	"stadhero", NULL, NULL, NULL, "1988",
	"Stadium Hero (Japan)\0", NULL, "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, stadheroRomInfo, stadheroRomName, NULL, NULL, NULL, NULL, StadheroInputInfo, StadheroDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 240, 4, 3
};
