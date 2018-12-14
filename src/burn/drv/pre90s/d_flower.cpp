// FB Alpha Flower driver module
// Based on MAME driver by InsideOutBoy

#include "tiles_generic.h"
#include "z80_intf.h"
#include "flower.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvZ80ROM2;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvSndROM0;
static UINT8 *DrvSndROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAMA;
static UINT8 *DrvZ80RAMB;
static UINT8 *DrvZ80RAM2;
static UINT8 *DrvSprRAM;
static UINT8 *DrvTxtRAM;
static UINT8 *DrvBgRAM0;
static UINT8 *DrvBgRAM1;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 soundlatch;
static UINT8 scroll[2];
static UINT8 nmi_enable;
static UINT8 flipscreen;
static INT32 irq_counter;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo FlowerInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Flower)

static struct BurnDIPInfo FlowerDIPList[]=
{
	{0x12, 0xff, 0xff, 0xf8, NULL				},
	{0x13, 0xff, 0xff, 0x9d, NULL				},

	{0   , 0xfe, 0   ,    2, "Energy Decrease"		},
	{0x12, 0x01, 0x08, 0x08, "Slow"				},
	{0x12, 0x01, 0x08, 0x00, "Fast"				},

	{0   , 0xfe, 0   ,    2, "Invulnerability (Cheat)"	},
	{0x12, 0x01, 0x10, 0x10, "Off"				},
	{0x12, 0x01, 0x10, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Keep Weapons When Destroyed"	},
	{0x12, 0x01, 0x20, 0x20, "No"				},
	{0x12, 0x01, 0x20, 0x00, "Yes"				},

	{0   , 0xfe, 0   ,    2, "Difficulty"			},
	{0x12, 0x01, 0x40, 0x40, "Normal"			},
	{0x12, 0x01, 0x40, 0x00, "Hard"				},

	{0   , 0xfe, 0   ,    2, "Shot Range"			},
	{0x12, 0x01, 0x80, 0x80, "Short"			},
	{0x12, 0x01, 0x80, 0x00, "Long"				},

	{0   , 0xfe, 0   ,    8, "Lives"			},
	{0x13, 0x01, 0x07, 0x07, "1"				},
	{0x13, 0x01, 0x07, 0x06, "2"				},
	{0x13, 0x01, 0x07, 0x05, "3"				},
	{0x13, 0x01, 0x07, 0x04, "4"				},
	{0x13, 0x01, 0x07, 0x03, "5"				},
	{0x13, 0x01, 0x07, 0x02, "6"				},
	{0x13, 0x01, 0x07, 0x01, "7"				},
	{0x13, 0x01, 0x07, 0x00, "Infinite (Cheat)"		},

	{0   , 0xfe, 0   ,    4, "Coinage"			},
	{0x13, 0x01, 0x18, 0x00, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x18, 0x08, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x18, 0x18, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x18, 0x10, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x13, 0x01, 0x20, 0x00, "Upright"			},
	{0x13, 0x01, 0x20, 0x20, "Cocktail"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x13, 0x01, 0x40, 0x40, "Off"				},
	{0x13, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Bonus Life"			},
	{0x13, 0x01, 0x80, 0x80, "30k, then every 50k"		},
	{0x13, 0x01, 0x80, 0x00, "50k, then every 80k"		},
};

STDDIPINFO(Flower)

static void __fastcall flower_main_write(UINT16 address, UINT8 data)
{
	INT32 active = ZetGetActive();

	switch (address)
	{
		case 0xa000:
			// coin_lockout_w (ignore)
		return;

		case 0xa001:
			flipscreen = data ? 1 : 0;
		return;

		case 0xa002:
			if (active == 1) {
				ZetClose();
				ZetOpen(0);
			}
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			if (active == 1) {
				ZetClose();
				ZetOpen(1);
			}
		return;

		case 0xa003:
			if (active == 0) {
				ZetClose();
				ZetOpen(1);
			}
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			if (active == 0) {
				ZetClose();
				ZetOpen(0);
			}
		return;

		case 0xa004:
			// coin_counter_w
		return;

		case 0xa005:
		return;	//nop

		case 0xa400:
			soundlatch = data;
			if (nmi_enable & 1) {
				ZetClose();
				ZetOpen(2);
				ZetNmi();
				ZetClose();
				ZetOpen(active);
			}
		return;

		case 0xf200:
			scroll[0] = data;
		return;

		case 0xfa00:
			scroll[1] = data;
		return;
	}
}

static UINT8 __fastcall flower_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xa100:
			return DrvInputs[0];

		case 0xa101:
			return DrvInputs[1];

		case 0xa102:
			return (DrvInputs[2] & 0x07) | (DrvDips[0] & 0xf8);

		case 0xa103:
			return DrvDips[1];
	}

	return 0;
}

static void __fastcall flower_sound_write(UINT16 address, UINT8 data)
{
	if ((address & 0xffc0) == 0x8000) {
		flower_sound1_w(address&0x3f, data);
		return;
	}

	if ((address & 0xffc0) == 0xa000) {
		flower_sound2_w(address&0x3f, data);
		return;
	}

	switch (address)
	{
		case 0x4000:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;

		case 0x4001:
			nmi_enable = data & 1;
		return;
	}
}

static UINT8 __fastcall flower_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x6000:
			return soundlatch;
	}

	return 0;
}

static tilemap_callback( bg0 )
{
	TILE_SET_INFO(1, DrvBgRAM0[offs], DrvBgRAM0[offs + 0x100] >> 4, 0);
}

static tilemap_callback( bg1 )
{
	TILE_SET_INFO(1, DrvBgRAM1[offs], DrvBgRAM1[offs + 0x100] >> 4, 0);
}

static tilemap_callback( txt )
{
	TILE_SET_INFO(0, DrvTxtRAM[offs], DrvTxtRAM[offs + 0x400] >> 2, 0);
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

	ZetOpen(2);
	ZetReset();
	ZetClose();

	flower_sound_reset();

	nmi_enable = 0;
	flipscreen = 0;
	soundlatch = 0;
	irq_counter = 0;
	scroll[0] = scroll[1] = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x0100000;
	DrvZ80ROM1		= Next; Next += 0x0100000;
	DrvZ80ROM2		= Next; Next += 0x0100000;

	DrvGfxROM0		= Next; Next += 0x0080000;
	DrvGfxROM1		= Next; Next += 0x0100000;
	DrvGfxROM2		= Next; Next += 0x0100000;

	DrvSndROM0		= Next; Next += 0x0080000;
	DrvSndROM1		= Next; Next += 0x0080000;

	DrvColPROM		= Next; Next += 0x0003000;

	DrvPalette		= (UINT32*)Next; Next += 0x1000 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAMA		= Next; Next += 0x001e000;
	DrvZ80RAMB		= Next; Next += 0x0008000;
	DrvZ80RAM2		= Next; Next += 0x0008000;

	DrvSprRAM		= Next; Next += 0x0002000;
	DrvTxtRAM		= Next; Next += 0x0008000;
	DrvBgRAM0		= Next; Next += 0x0002000;
	DrvBgRAM1		= Next; Next += 0x0002000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[4]  = { 0, 4, 0x4000*8, 0x4000*8+4 };
	INT32 XOffs[16] = { STEP4(0,1), STEP4(8,1), STEP4(8*8*2,1), STEP4(8*8*2+8,1) };
	INT32 YOffs[16] = { STEP8(0,16), STEP8(8*8*4,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x8000);
	if (tmp == NULL) {
		return 1;
	}

	for (INT32 i = 0; i < 0x02000; i++) tmp[i] = DrvGfxROM0[i] ^ 0xff;

	GfxDecode(0x0200, 2,  8,  8, Plane, XOffs, YOffs, 0x080, tmp, DrvGfxROM0);

	for (INT32 i = 0; i < 0x08000; i++) tmp[i] = DrvGfxROM1[i] ^ 0xff;

	GfxDecode(0x0100, 4, 16, 16, Plane, XOffs, YOffs, 0x200, tmp, DrvGfxROM1);

	for (INT32 i = 0; i < 0x08000; i++) tmp[i] = DrvGfxROM2[i] ^ 0xff;

	GfxDecode(0x0100, 4, 16, 16, Plane, XOffs, YOffs, 0x200, tmp, DrvGfxROM2);

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

		if (BurnLoadRom(DrvZ80ROM1 + 0x0000,  1, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM2 + 0x0000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x0000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x0000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x2000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x4000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x6000,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x0000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x2000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x4000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x6000, 11, 1)) return 1;

		if (BurnLoadRom(DrvSndROM0 + 0x0000, 12, 1)) return 1;

		if (BurnLoadRom(DrvSndROM1 + 0x0000, 13, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x0200, 14, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0100, 15, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0000, 16, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAMA,		0xc000, 0xddff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0xde00, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,			0xe000, 0xe7ff, MAP_RAM);
	ZetMapMemory(DrvZ80RAMB,		0xe800, 0xefff, MAP_RAM);
	ZetMapMemory(DrvBgRAM0,			0xf000, 0xf1ff, MAP_RAM);
	ZetMapMemory(DrvBgRAM1,			0xf800, 0xf9ff, MAP_RAM);
	ZetSetWriteHandler(flower_main_write);
	ZetSetReadHandler(flower_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAMA,		0xc000, 0xddff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0xde00, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,			0xe000, 0xe7ff, MAP_RAM);
	ZetMapMemory(DrvZ80RAMB,		0xe800, 0xefff, MAP_RAM);
	ZetMapMemory(DrvBgRAM0,			0xf000, 0xf1ff, MAP_RAM);
	ZetMapMemory(DrvBgRAM1,			0xf800, 0xf9ff, MAP_RAM);
	ZetSetWriteHandler(flower_main_write);
	ZetSetReadHandler(flower_main_read);
	ZetClose();

	ZetInit(2);
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM2,		0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM2,		0xc000, 0xc7ff, MAP_RAM);
	ZetSetWriteHandler(flower_sound_write);
	ZetSetReadHandler(flower_sound_read);
	ZetClose();

	flower_sound_init(DrvSndROM0, DrvSndROM1);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg0_map_callback, 16, 16, 16, 16);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, bg1_map_callback, 16, 16, 16, 16);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, txt_map_callback,  8,  8, 32, 32);
	GenericTilemapInit(3, TILEMAP_SCAN_COLS, txt_map_callback,  8,  8,  2, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 2,  8,  8, 0x10000, 0, 0x3f);
	GenericTilemapSetGfx(1, DrvGfxROM2, 4, 16, 16, 0x10000, 0, 0x0f);
	GenericTilemapSetTransparent(1, 0xf);
	GenericTilemapSetTransparent(2, 0x3);
	GenericTilemapSetTransparent(3, 0x3);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	BurnFree(AllMem);

	flower_sound_exit();

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x100; i++)
	{
		INT32 r = DrvColPROM[i + 0x000] & 0xf;
		INT32 g = DrvColPROM[i + 0x100] & 0xf;
		INT32 b = DrvColPROM[i + 0x200] & 0xf;

		DrvPalette[i] = BurnHighCol(r+r*16,g+g*16,b+b*16,0);
	}
}

static void draw_sprites()
{
	UINT8 *source = DrvSprRAM + 0x200;
	UINT8 *finish = source - 0x200;

	source -= 8;

	while (source >= finish)
	{
		INT32 sy   = 256-32-source[0]+1;
		INT32 sx   = (source[4]|(source[5]<<8))-55;
		INT32 code  = source[1] & 0x3f;
		INT32 color = source[6] >> 4;
		INT32 flipy = source[1] & 0x80;
		INT32 flipx = source[1] & 0x40;
		INT32 size  = source[3];
		INT32 xsize = ((size & 0x08)>>3)+1;
		INT32 ysize = ((size & 0x80)>>7)+1;

		if (ysize==2) sy -= 16;

		code |= ((source[2] & 0x01) << 6) | ((source[2] & 0x08) << 4);

		if (flipscreen)
		{
			flipx = !flipx;
			flipy = !flipy;
			sx = sx+16;
			sy = 250-sy;

			if (ysize==2) sy += 16;
		}

		for (INT32 xblock = 0; xblock<xsize; xblock++)
		{
			INT32 xoffs=!flipx ? (xblock*8) : ((xsize-xblock-1)*8);
			INT32 zoomx=((size&7)+1)<<13;
			INT32 zoomy=((size&0x70)+0x10)<<9;
			INT32 xblocksizeinpixels=(zoomx*16)>>16;
			INT32 yblocksizeinpixels=(zoomy*16)>>16;

			for (INT32 yblock = 0; yblock<ysize; yblock++)
			{
				INT32 yoffs=!flipy ? yblock : (ysize-yblock-1);
				INT32 sxoffs=(16-xblocksizeinpixels)/2;
				INT32 syoffs=(16-yblocksizeinpixels)/2;
				if (xblock) sxoffs+=xblocksizeinpixels;
				if (yblock) syoffs+=yblocksizeinpixels;

				RenderZoomedTile(pTransDraw, DrvGfxROM1, code+yoffs+xoffs, color*16, 0xf, sx+sxoffs, sy+syoffs, flipx, flipy, 16, 16, zoomx, zoomy);
			}
		}
		source -= 8;
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	BurnTransferClear();

	GenericTilemapSetScrollY(0, scroll[0] + 16);
	GenericTilemapSetScrollY(1, scroll[1] + 16);
	GenericTilemapSetScrollY(2, 16);
	GenericTilemapSetScrollY(3, 16);

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);
	if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 0);

	if (nSpriteEnable & 1) draw_sprites();

	if (nBurnLayer & 4) GenericTilemapDraw(2, pTransDraw, 0);

	GenericTilesSetClip(nScreenWidth-16, nScreenWidth, -1, -1);
	if (nBurnLayer & 8) GenericTilemapDraw(3, pTransDraw, 0);
	GenericTilesClearClip();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();

	{
		INT32 prev = DrvInputs[2] & 1;

		memset (DrvInputs, 0xff, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		if (prev != (DrvInputs[2] & 1)) {
			ZetOpen(0);
			ZetNmi();
			ZetClose();
		}
	}

	INT32 nInterleave = 100;
	INT32 nCyclesTotal[3] = { 4608000 / 60, 4608000 / 60, 4608000 / 60 };
	INT32 nCyclesDone[3] = { 0, 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);
		if (i == 90) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(1);
		nCyclesDone[1] += ZetRun(nCyclesTotal[1] / nInterleave);
		if (i == 90 || i == 40) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(2);
		nCyclesDone[2] += ZetRun(nCyclesTotal[2] / nInterleave);
		if ((irq_counter % 67) == 0) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		irq_counter++;
	}

	if (pBurnSoundOut) {
		flower_sound_update(pBurnSoundOut, nBurnSoundLen);
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
		flower_sound_scan();

		SCAN_VAR(scroll);
		SCAN_VAR(soundlatch);
		SCAN_VAR(nmi_enable);
		SCAN_VAR(flipscreen);
		SCAN_VAR(irq_counter);
	}

	return 0;
}



// Flower (US)

static struct BurnRomInfo flowerRomDesc[] = {
	{ "1.5j",	0x8000, 0xa4c3af78, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	{ "2.5f",	0x8000, 0x7c7ee2d8, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code

	{ "3.d9",	0x4000, 0x8866c2b0, 3 | BRF_PRG | BRF_ESS }, //  2 Z80 #2 Code

	{ "10.13e",	0x2000, 0x62f9b28c, 4 | BRF_GRA },           //  3 Foreground Tiles

	{ "14.19e",	0x2000, 0x11b491c5, 5 | BRF_GRA },           //  4 Sprites
	{ "13.17e",	0x2000, 0xea743986, 5 | BRF_GRA },           //  5
	{ "12.16e",	0x2000, 0xe3779f7f, 5 | BRF_GRA },           //  6
	{ "11.14e",	0x2000, 0x8801b34f, 5 | BRF_GRA },           //  7

	{ "8.10e",	0x2000, 0xf85eb20f, 6 | BRF_GRA },           //  8 Background Tiles
	{ "6.7e",	0x2000, 0x3e97843f, 6 | BRF_GRA },           //  9
	{ "9.12e",	0x2000, 0xf1d9915e, 6 | BRF_GRA },           // 10
	{ "15.9e",	0x2000, 0x1cad9f72, 6 | BRF_GRA },           // 11

	{ "4.12a",	0x8000, 0x851ed9fd, 7 | BRF_GRA },           // 12 Sound Chip #0 Data

	{ "5.16a",	0x4000, 0x42fa2853, 8 | BRF_GRA },           // 13 Sound Chip #1 Data

	{ "82s129.k1",	0x0100, 0xd311ed0d, 9 | BRF_GRA },           // 14 Color Data
	{ "82s129.k2",	0x0100, 0xababb072, 9 | BRF_GRA },           // 15
	{ "82s129.k3",	0x0100, 0x5aab7b41, 9 | BRF_GRA },           // 16

	{ "82s147.d7",	0x0200, 0xf0dbb2a7, 0 | BRF_OPT },           // 17 Unknown PROMs
	{ "82s147.j18",	0x0200, 0xd7de0860, 0 | BRF_OPT },           // 18
	{ "82s123.k7",	0x0020, 0xea9c65e4, 0 | BRF_OPT },           // 19
	{ "82s129.a1",	0x0100, 0xc8dad3fc, 0 | BRF_OPT },           // 20
};

STD_ROM_PICK(flower)
STD_ROM_FN(flower)

struct BurnDriver BurnDrvFlower = {
	"flower", NULL, NULL, NULL, "1986",
	"Flower (US)\0", NULL, "Clarue (Komax license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, flowerRomInfo, flowerRomName, NULL, NULL, NULL, NULL, FlowerInputInfo, FlowerDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	272, 224, 4, 3
};


// Flower (Japan)

static struct BurnRomInfo flowerjRomDesc[] = {
	{ "1",		0x8000, 0x63a2ef04, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	{ "2.5f",	0x8000, 0x7c7ee2d8, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code

	{ "3.d9",	0x4000, 0x8866c2b0, 3 | BRF_PRG | BRF_ESS }, //  2 Z80 #2 Code

	{ "10.13e",	0x2000, 0x62f9b28c, 4 | BRF_GRA },           //  3 Foreground Tiles

	{ "14.19e",	0x2000, 0x11b491c5, 5 | BRF_GRA },           //  4 Sprites
	{ "13.17e",	0x2000, 0xea743986, 5 | BRF_GRA },           //  5
	{ "12.16e",	0x2000, 0xe3779f7f, 5 | BRF_GRA },           //  6
	{ "11.14e",	0x2000, 0x8801b34f, 5 | BRF_GRA },           //  7

	{ "8.10e",	0x2000, 0xf85eb20f, 6 | BRF_GRA },           //  8 Background Tiles
	{ "6.7e",	0x2000, 0x3e97843f, 6 | BRF_GRA },           //  9
	{ "9.12e",	0x2000, 0xf1d9915e, 6 | BRF_GRA },           // 10
	{ "7.9e",	0x2000, 0xe350f36c, 6 | BRF_GRA },           // 11

	{ "4.12a",	0x8000, 0x851ed9fd, 7 | BRF_GRA },           // 12 Sound Chip #0 Data

	{ "5.16a",	0x4000, 0x42fa2853, 8 | BRF_GRA },           // 13 Sound Chip #1 Data

	{ "82s129.k1",	0x0100, 0xd311ed0d, 9 | BRF_GRA },           // 14 Color Data
	{ "82s129.k2",	0x0100, 0xababb072, 9 | BRF_GRA },           // 15
	{ "82s129.k3",	0x0100, 0x5aab7b41, 9 | BRF_GRA },           // 16

	{ "82s147.d7",	0x0200, 0xf0dbb2a7, 0 | BRF_OPT },           // 17 Unknown PROMs
	{ "82s147.j18",	0x0200, 0xd7de0860, 0 | BRF_OPT },           // 18
	{ "82s123.k7",	0x0020, 0xea9c65e4, 0 | BRF_OPT },           // 19
	{ "82s129.a1",	0x0100, 0xc8dad3fc, 0 | BRF_OPT },           // 20
};

STD_ROM_PICK(flowerj)
STD_ROM_FN(flowerj)

struct BurnDriver BurnDrvFlowerj = {
	"flowerj", "flower", NULL, NULL, "1986",
	"Flower (Japan)\0", NULL, "Clarue (Sega / Alpha Denshi Co. license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, flowerjRomInfo, flowerjRomName, NULL, NULL, NULL, NULL, FlowerInputInfo, FlowerDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	272, 224, 4, 3
};
