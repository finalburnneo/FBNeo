// FB Alpha American Speedway driver module
// Based on MAME driver by Luca Elia

#include "tiles_generic.h"
#include "z80_intf.h"
#include "burn_ym2151.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvPalRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvColRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 soundlatch;
static UINT8 flipscreen;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvJoy5[8];
static UINT8 DrvDips[2];
static UINT8 DrvReset;
static UINT8 DrvInputs[5];

static struct BurnInputInfo AmspdwyInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"	},
	{"P1 Left",		    BIT_DIGITAL,	DrvJoy4 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy4 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 coin"	},
	{"P2 Left",		    BIT_DIGITAL,	DrvJoy5 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy5 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Amspdwy)

static struct BurnDIPInfo AmspdwyDIPList[]=
{
	{0x09, 0xff, 0xff, 0x04, NULL						},
	{0x0a, 0xff, 0xff, 0x00, NULL						},

	{0   , 0xfe, 0   ,    2, "Character Test"			},
	{0x09, 0x01, 0x01, 0x00, "Off"						},
	{0x09, 0x01, 0x01, 0x01, "On"						},

	{0   , 0xfe, 0   ,    2, "Show Arrows"				},
	{0x09, 0x01, 0x02, 0x00, "Off"						},
	{0x09, 0x01, 0x02, 0x02, "On"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x09, 0x01, 0x04, 0x00, "Off"						},
	{0x09, 0x01, 0x04, 0x04, "On"						},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x09, 0x01, 0x08, 0x00, "Off"						},
	{0x09, 0x01, 0x08, 0x08, "On"						},

	{0   , 0xfe, 0   ,    2, "Steering Test"			},
	{0x09, 0x01, 0x10, 0x00, "Off"						},
	{0x09, 0x01, 0x10, 0x10, "On"						},

	{0   , 0xfe, 0   ,    3, "Coinage"					},
	{0x0a, 0x01, 0x03, 0x03, "2 Coins 1 Credits"		},
	{0x0a, 0x01, 0x03, 0x00, "1 Coin  1 Credits"		},
	{0x0a, 0x01, 0x03, 0x01, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x0a, 0x01, 0x0c, 0x00, "Easy"						},
	{0x0a, 0x01, 0x0c, 0x04, "Normal"					},
	{0x0a, 0x01, 0x0c, 0x08, "Hard"						},
	{0x0a, 0x01, 0x0c, 0x0c, "Hardest"					},

	{0   , 0xfe, 0   ,    4, "Time To Qualify"			},
	{0x0a, 0x01, 0x30, 0x30, "20 sec"					},
	{0x0a, 0x01, 0x30, 0x20, "30 sec"					},
	{0x0a, 0x01, 0x30, 0x10, "45 sec"					},
	{0x0a, 0x01, 0x30, 0x00, "60 sec"					},
};

STDDIPINFO(Amspdwy)

static struct BurnDIPInfo AmspdwyaDIPList[]=
{
	{0x09, 0xff, 0xff, 0x04, NULL						},
	{0x0a, 0xff, 0xff, 0x00, NULL						},

	{0   , 0xfe, 0   ,    2, "Character Test"			},
	{0x09, 0x01, 0x01, 0x00, "Off"						},
	{0x09, 0x01, 0x01, 0x01, "On"						},

	{0   , 0xfe, 0   ,    2, "Show Arrows"				},
	{0x09, 0x01, 0x02, 0x00, "Off"						},
	{0x09, 0x01, 0x02, 0x02, "On"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x09, 0x01, 0x04, 0x00, "Off"						},
	{0x09, 0x01, 0x04, 0x04, "On"						},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x09, 0x01, 0x08, 0x00, "Off"						},
	{0x09, 0x01, 0x08, 0x08, "On"						},

	{0   , 0xfe, 0   ,    2, "Steering Test"			},
	{0x09, 0x01, 0x10, 0x00, "Off"						},
	{0x09, 0x01, 0x10, 0x10, "On"						},

	{0   , 0xfe, 0   ,    3, "Coinage"					},
	{0x0a, 0x01, 0x03, 0x03, "2 Coins 1 Credits"		},
	{0x0a, 0x01, 0x03, 0x00, "1 Coin  1 Credits"		},
	{0x0a, 0x01, 0x03, 0x01, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x0a, 0x01, 0x0c, 0x00, "Easy"						},
	{0x0a, 0x01, 0x0c, 0x04, "Normal"					},
	{0x0a, 0x01, 0x0c, 0x08, "Hard"						},
	{0x0a, 0x01, 0x0c, 0x0c, "Hardest"					},

	{0   , 0xfe, 0   ,    2, "Time To Qualify"			},
	{0x0a, 0x01, 0x10, 0x10, "45 sec"					},
	{0x0a, 0x01, 0x10, 0x00, "60 sec"					},
};

STDDIPINFO(Amspdwya)

static UINT8 steering_read(INT32 select)
{
	UINT8  left[4] = { 0x1c, 0x12, 0x1d, 0x17 };
	UINT8 right[4] = { 0x09, 0x0f, 0x0c, 0x05 };

	UINT8 steer;

	steer  = ((DrvInputs[3 + select] & 1) ? ( left[nCurrentFrame & 3]) : 0x00);
	steer |= ((DrvInputs[3 + select] & 2) ? (right[nCurrentFrame & 3]) : 0x00);

	return steer | DrvInputs[select];
}

static void __fastcall amspdwy_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xa000:
		return;	//nop

		case 0xa400:
			flipscreen ^= 1;
		return;

		case 0xb000:
		return;	//nop

		case 0xb400:
			soundlatch = data;
			ZetNmi(1);
		return;
	}
}

static UINT8 __fastcall amspdwy_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xa000:
			return DrvDips[0];

		case 0xa400:
			return DrvDips[1];

		case 0xa800:
			return steering_read(0);

		case 0xac00:
			return steering_read(1);

		case 0xb400:
			return (BurnYM2151Read() & ~0x30) | (~DrvInputs[2] & 0x30);
	}

	return 0;
}

static UINT8 __fastcall amspdwy_main_read_port(UINT16 port)
{
	if (port < 0x8000) return DrvZ80ROM0[0x8000 + port];

	return 0;
}

static void __fastcall amspdwy_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x8000: // nop
		return;

		case 0xa000:
		case 0xa001:
			BurnYM2151Write(address, data);
		return;
	}
}

static UINT8 __fastcall amspdwy_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x9000:
			return soundlatch;

		case 0xa000:
		case 0xa001:
			return BurnYM2151Read();

		case 0xffff:
			return 0; // nop
	}

	return 0;
}

static tilemap_callback( layer )
{
	INT32 attr = DrvColRAM[offs];
	INT32 code = DrvVidRAM[offs] + ((attr << 5) & 0x300);

	TILE_SET_INFO(0, code, attr, 0);
}

static tilemap_scan( layer )
{
	return (col * 32) + (32 - row - 1);
}

static void DrvYM2151IrqHandler(INT32 nStatus)
{
	if (ZetGetActive() == -1) return;

	ZetSetIRQLine(0,(nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetReset(0);

	ZetOpen(1);
	ZetReset();
	BurnYM2151Reset();
	ZetClose();

	soundlatch = 0;
	flipscreen = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0	= Next; Next += 0x010000;
	DrvZ80ROM1	= Next; Next += 0x008000;

	DrvGfxROM	= Next; Next += 0x010000;

	DrvPalette	= (UINT32*)Next; Next += 0x020 * sizeof(UINT32);

	AllRam		= Next;

	DrvZ80RAM0	= Next; Next += 0x000800;
	DrvZ80RAM1	= Next; Next += 0x002000;
	DrvPalRAM	= Next; Next += 0x000100;
	DrvSprRAM	= Next; Next += 0x000100;
	DrvVidRAM	= Next; Next += 0x000400;
	DrvColRAM	= Next; Next += 0x000800;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[2]  = { 0, 0x2000*8 };
	INT32 XOffs[16] = { STEP8(0,1) };
	INT32 YOffs[16] = { STEP8(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x4000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM, 0x4000);

	GfxDecode(0x400, 2, 8, 8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM);

	BurnFree (tmp);

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
		if (BurnLoadRom(DrvZ80ROM0 + 0x8000,  1, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x0000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM  + 0x0000,  3, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM  + 0x1000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM  + 0x2000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM  + 0x3000,  6, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvPalRAM,			0x8000, 0x80ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,			0x9000, 0x93ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,			0x9400, 0x97ff, MAP_RAM);
	ZetMapMemory(DrvColRAM,			0x9800, 0x9fff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0xc000, 0xc0ff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM0,		0xe000, 0xe7ff, MAP_RAM);
	ZetSetWriteHandler(amspdwy_main_write);
	ZetSetReadHandler(amspdwy_main_read);
	ZetSetInHandler(amspdwy_main_read_port);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,		0xc000, 0xdfff, MAP_RAM);
	ZetSetWriteHandler(amspdwy_sound_write);
	ZetSetReadHandler(amspdwy_sound_read);
	ZetClose();

	BurnYM2151Init(3000000);
	BurnYM2151SetIrqHandler(&DrvYM2151IrqHandler);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.55, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.45, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, layer_map_scan, layer_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM, 2, 8, 8, 0x10000, 0, 7);
	GenericTilemapSetOffsets(0, 0, -16);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	BurnYM2151Exit();;

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteUpdate()
{
	for (INT32 i = 0; i < 0x20; i++)
	{
		UINT8 p = DrvPalRAM[i] ^ 0xff;
		UINT8 r = (p >> 0) & 7;
		UINT8 g = (p >> 3) & 7;
		UINT8 b = (p >> 6) & 3;

		r = (r << 5) | (r << 2) | (r >> 1);
		g = (g << 5) | (g << 2) | (g >> 1);
		b = (b << 6) | (b << 4) | (b << 2) | (b << 0);

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_sprites()
{
	for (INT32 i = 0; i < 0x100 ; i += 4)
	{
		INT32 attr  = DrvSprRAM[i + 3];
		INT32 code  = DrvSprRAM[i + 2] + ((attr << 5) & 0x100);
		INT32 sx    = DrvSprRAM[i + 1];
		INT32 sy    = DrvSprRAM[i + 0];
		INT32 flipx = attr & 0x80;
		INT32 flipy = attr & 0x40;

		if (flipscreen)
		{
			sx = 255 - sx - 8;
			sy = 223 - sy - 8;
			flipx = !flipx;
			flipy = !flipy;
		}

		if (flipy) {
			if (flipx) {
				Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, attr & 0x7, 2, 0, 0, DrvGfxROM);
			} else {
				Render8x8Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, attr & 0x7, 2, 0, 0, DrvGfxROM);
			}
		} else {
			if (flipx) {
				Render8x8Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, attr & 0x7, 2, 0, 0, DrvGfxROM);
			} else {
				Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy, attr & 0x7, 2, 0, 0, DrvGfxROM);
			}
		}
	}
}

static INT32 DrvDraw()
{
	//if (DrvRecalc) {
		DrvPaletteUpdate();
	//	DrvRecalc = 1;
	//}

	GenericTilemapSetFlip(0, flipscreen);

	GenericTilemapDraw(0, pTransDraw, 0);
	draw_sprites();

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
		memset (DrvInputs, 0, 5);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
			DrvInputs[4] ^= (DrvJoy5[i] & 1) << i;
		}
	}

	INT32 nSegment;
	INT32 nInterleave = 256;
	INT32 nSoundBufferPos = 0;
	INT32 nCyclesTotal[2] = { 3000000 / 60, 3000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(((nCyclesTotal[0] * (i + 1)) / nInterleave) - nCyclesDone[0]);
		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(1);
		nCyclesDone[1] += ZetRun(((nCyclesTotal[1] * (i + 1)) / nInterleave) - nCyclesDone[1]);

		if (pBurnSoundOut) {
			nSegment = nBurnSoundLen / nInterleave;
			BurnYM2151Render(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
			nSoundBufferPos += nSegment;
		}

		ZetClose();
	}

	ZetOpen(1);

	if (pBurnSoundOut) {
		nSegment = nBurnSoundLen - nSoundBufferPos;
		if (nSegment > 0) {
			BurnYM2151Render(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
		}
	}

	ZetClose();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029707;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);
		BurnYM2151Scan(nAction, pnMin);

		SCAN_VAR(soundlatch);
		SCAN_VAR(flipscreen);
	}

	return 0;
}


// American Speedway (set 1)

static struct BurnRomInfo amspdwyRomDesc[] = {
	{ "game5807.u33",	0x8000, 0x88233b59, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "trks6092.u34",	0x8000, 0x74a4e7b7, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "audi9463.u2",	0x8000, 0x61b0467e, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "hilo9b3c.5a",	0x1000, 0xf50f864c, 3 | BRF_GRA },           //  3 Graphics
	{ "hihie12a.4a",	0x1000, 0x3d7497f3, 3 | BRF_GRA },           //  4
	{ "lolo1d51.1a",	0x1000, 0x58701c1c, 3 | BRF_GRA },           //  5
	{ "lohi4644.2a",	0x1000, 0xa1d802b1, 3 | BRF_GRA },           //  6
};

STD_ROM_PICK(amspdwy)
STD_ROM_FN(amspdwy)

struct BurnDriver BurnDrvAmspdwy = {
	"amspdwy", NULL, NULL, NULL, "1987",
	"American Speedway (set 1)\0", NULL, "Enerdyne Technologies Inc.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, amspdwyRomInfo, amspdwyRomName, NULL, NULL, NULL, NULL, AmspdwyInputInfo, AmspdwyDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x20,
	256, 224, 4, 3
};


// American Speedway (set 2)

static struct BurnRomInfo amspdwyaRomDesc[] = {
	{ "game.u33",		0x8000, 0xfacab102, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "trks6092.u34",	0x8000, 0x74a4e7b7, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "audi9463.u2",	0x8000, 0x61b0467e, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "hilo9b3c.5a",	0x1000, 0xf50f864c, 3 | BRF_GRA },           //  3 Graphics
	{ "hihie12a.4a",	0x1000, 0x3d7497f3, 3 | BRF_GRA },           //  4
	{ "lolo1d51.1a",	0x1000, 0x58701c1c, 3 | BRF_GRA },           //  5
	{ "lohi4644.2a",	0x1000, 0xa1d802b1, 3 | BRF_GRA },           //  6
};

STD_ROM_PICK(amspdwya)
STD_ROM_FN(amspdwya)

struct BurnDriver BurnDrvAmspdwya = {
	"amspdwya", "amspdwy", NULL, NULL, "1987",
	"American Speedway (set 2)\0", NULL, "Enerdyne Technologies Inc.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, amspdwyaRomInfo, amspdwyaRomName, NULL, NULL, NULL, NULL, AmspdwyInputInfo, AmspdwyaDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x20,
	256, 224, 4, 3
};
