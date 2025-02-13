// FB Neo Commando Driver module
// Based on MAME driver by Nicola Salmoria

#include "tiles_generic.h"
#include "z80_intf.h"
#include "burn_ym2203.h"
#include "bitswap.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM[2];
static UINT8 *DrvZ80Code;
static UINT8 *DrvGfxROM[3];
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM[2];
static UINT8 *DrvVidRAM[2];
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprBuf;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 flipscreen;
static INT32 soundlatch;
static INT32 scrollx;
static INT32 scrolly;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo CommandoInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 6,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Commando)

static struct BurnDIPInfo CommandoDIPList[]=
{
	{0x11, 0xff, 0xff, 0xff, NULL					},
	{0x12, 0xff, 0xff, 0x1f, NULL					},

	{0   , 0xfe, 0   ,    4, "Starting Area"		},
	{0x11, 0x01, 0x03, 0x03, "0 (Forest 1)"			},
	{0x11, 0x01, 0x03, 0x01, "2 (Desert 1)"			},
	{0x11, 0x01, 0x03, 0x02, "4 (Forest 2)"			},
	{0x11, 0x01, 0x03, 0x00, "6 (Desert 2)"			},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x11, 0x01, 0x0c, 0x04, "2"					},
	{0x11, 0x01, 0x0c, 0x0c, "3"					},
	{0x11, 0x01, 0x0c, 0x08, "4"					},
	{0x11, 0x01, 0x0c, 0x00, "5"					},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x11, 0x01, 0x30, 0x00, "4 Coins 1 Credits"	},
	{0x11, 0x01, 0x30, 0x20, "3 Coins 1 Credits"	},
	{0x11, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin A"				},
	{0x11, 0x01, 0xc0, 0x00, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0xc0, 0x40, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0xc0, 0x80, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    8, "Bonus Life"			},
	{0x12, 0x01, 0x07, 0x07, "10K 50K+"				},
	{0x12, 0x01, 0x07, 0x03, "10K 60K+"				},
	{0x12, 0x01, 0x07, 0x05, "20K 60K+"				},
	{0x12, 0x01, 0x07, 0x01, "20K 70K+"				},
	{0x12, 0x01, 0x07, 0x06, "30K 70K+"				},
	{0x12, 0x01, 0x07, 0x02, "30K 80K+"				},
	{0x12, 0x01, 0x07, 0x04, "40K 100K+"			},
	{0x12, 0x01, 0x07, 0x00, "None"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x12, 0x01, 0x08, 0x00, "Off"					},
	{0x12, 0x01, 0x08, 0x08, "On"					},

	{0   , 0xfe, 0   ,    2, "Difficulty"			},
	{0x12, 0x01, 0x10, 0x10, "Normal"				},
	{0x12, 0x01, 0x10, 0x00, "Difficult"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x12, 0x01, 0x20, 0x00, "Off"					},
	{0x12, 0x01, 0x20, 0x20, "On"					},

	{0   , 0xfe, 0   ,    3, "Cabinet"				},
	{0x12, 0x01, 0xc0, 0x00, "Upright"				},
	{0x12, 0x01, 0xc0, 0x40, "Upright Two Players"	},
	{0x12, 0x01, 0xc0, 0xc0, "Cocktail"				},
};

STDDIPINFO(Commando)

static void __fastcall commando_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xc800:
			soundlatch = data;
		return;

		case 0xc804:
			// coin counter = data & 3
			ZetSetRESETLine(1, data & 0x10);
			flipscreen = data & 0x80;
		return;

		case 0xc806:
			// nop
		return;

		case 0xc808:
		case 0xc809:
			scrollx = (scrollx & (0xff << ((~address & 1) * 8))) | (data << ((address & 1) * 8));
		return;

		case 0xc80a:
		case 0xc80b:
			scrolly = (scrolly & (0xff << ((~address & 1) * 8))) | (data << ((address & 1) * 8));
		return;
	}
}

static UINT8 __fastcall commando_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xc000:
			return DrvInputs[0];

		case 0xc001:
			return DrvInputs[1];

		case 0xc002:
			return DrvInputs[2];

		case 0xc003:
			return DrvDips[0];

		case 0xc004:
			return DrvDips[1];
	}

	return 0;
}

static void __fastcall commando_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x8000:
		case 0x8001:
		case 0x8002:
		case 0x8003:
			BurnYM2203Write((address / 2) & 1, address & 1, data);
		return;
	}
}

static UINT8 __fastcall commando_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x6000:
			return soundlatch;
	}

	return 0;
}

static tilemap_callback( bg )
{
	INT32 attr = DrvVidRAM[0][offs + 0x400];
	INT32 code = DrvVidRAM[0][offs] + ((attr << 2) & 0x300);

	TILE_SET_INFO(1, code, attr, TILE_FLIPYX(attr >> 4));
}

static tilemap_callback( fg )
{
	INT32 attr = DrvVidRAM[1][offs + 0x400];
	INT32 code = DrvVidRAM[1][offs] + ((attr << 2) & 0x300);

	TILE_SET_INFO(0, code, attr, TILE_FLIPYX(attr >> 4));
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	BurnYM2203Reset();
	ZetClose();

	HiscoreReset();

	flipscreen = 0;
	soundlatch = 0;
	scrollx = 0;
	scrolly = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM[0]		= Next; Next += 0x00c000;
	DrvZ80ROM[1]		= Next; Next += 0x004000;
	DrvZ80Code			= Next; Next += 0x00c000;

	DrvGfxROM[0]		= Next; Next += 0x020000;
	DrvGfxROM[1]		= Next; Next += 0x040000;
	DrvGfxROM[2]		= Next; Next += 0x030000;

	DrvColPROM			= Next; Next += 0x000300;

	DrvPalette			= (UINT32*)Next; Next += 0x100 * sizeof(UINT32);

	AllRam				= Next;

	DrvZ80RAM[0]		= Next; Next += 0x001e00;
	DrvZ80RAM[1]		= Next; Next += 0x000800;
	DrvVidRAM[0]		= Next; Next += 0x000800;
	DrvVidRAM[1]		= Next; Next += 0x000800;
	DrvSprRAM			= Next; Next += 0x000200;
	DrvSprBuf			= Next; Next += 0x000180;

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[2]  = { 4, 0 };
	INT32 XOffs0[8]  = { STEP4(0,1), STEP4(8,1) };
	INT32 YOffs0[8]  = { STEP8(0,16) };

	INT32 Plane1[3]  = { 0x8000*8*0, 0x8000*8*1, 0x8000*8*2 };
	INT32 XOffs1[16] = { STEP8(0,1), STEP8(128,1) };
	INT32 YOffs1[16] = { STEP16(0,8) };

	INT32 Plane2[4]  = { 0xc000*8+4, 0xc000*8, 4, 0 };
	INT32 XOffs2[16] = { STEP4(0,1), STEP4(8,1), STEP4(256,1), STEP4(256+8,1) };
	INT32 YOffs2[16] = { STEP16(0,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x18000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM[0], 0x04000);

	GfxDecode(0x0800, 2,  8,  8, Plane0, XOffs0, YOffs0, 0x080, tmp, DrvGfxROM[0]);

	memcpy (tmp, DrvGfxROM[1], 0x18000);

	GfxDecode(0x0400, 3, 16, 16, Plane1, XOffs1, YOffs1, 0x100, tmp, DrvGfxROM[1]);

	memcpy (tmp, DrvGfxROM[2], 0x18000);

	GfxDecode(0x0300, 4, 16, 16, Plane2, XOffs2, YOffs2, 0x200, tmp, DrvGfxROM[2]);

	BurnFree(tmp);

	return 0;
}

static void DrvOpcodeDecode(INT32 first_opcode)
{
	for (INT32 i = first_opcode ? 0 : 1; i < 0xc000; i++) {
		DrvZ80Code[i] = BITSWAP08(DrvZ80ROM[0][i], 3, 2, 1, 4, 7, 6, 5, 0);
	}
}

static INT32 DrvInit(INT32 first_opcode, INT32 mercenario)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x00000, k++, 1)) return 1;
		if (mercenario) if (BurnLoadRom(DrvZ80ROM[0] + 0x04000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x08000, k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM[1] + 0x00000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[0] + 0x00000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[1] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x04000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x08000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x0c000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x10000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x14000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[2] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x04000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x08000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x0c000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x10000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x14000, k++, 1)) return 1;

		if (BurnLoadRom(DrvColPROM   + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x00100, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x00200, k++, 1)) return 1;

		DrvOpcodeDecode(first_opcode);
		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM[0],	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80Code,	0x0000, 0xbfff, MAP_FETCHOP);
	ZetMapMemory(DrvVidRAM[1],	0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM[0],	0xd800, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM[0],	0xe000, 0xfdff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xfe00, 0xffff, MAP_RAM);
	ZetSetWriteHandler(commando_main_write);
	ZetSetReadHandler(commando_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM[1],	0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM[1],	0x4000, 0x47ff, MAP_RAM);
	ZetSetWriteHandler(commando_sound_write);
	ZetSetReadHandler(commando_sound_read);
	ZetClose();

	BurnYM2203Init(2, 1500000, NULL, 0);
	BurnTimerAttachZet(3000000);
	BurnYM2203SetAllRoutes(0, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetAllRoutes(1, 0.15, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, fg_map_callback,  8,  8, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_COLS, bg_map_callback, 16, 16, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 2,  8,  8, 0x20000, 0xc0, 0xf);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 3, 16, 16, 0x40000, 0x00, 0xf);
	GenericTilemapSetGfx(2, DrvGfxROM[2], 4, 16, 16, 0x30000, 0x80, 0x3);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);
	GenericTilemapSetTransparent(0, 3);
	GenericTilemapBuildSkipTable(0, 0, 3);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	ZetExit();
	BurnYM2203Exit();

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x100; i++)
	{
		INT32 bit0 = (DrvColPROM[i + 0x000] >> 0) & 1;
		INT32 bit1 = (DrvColPROM[i + 0x000] >> 1) & 1;
		INT32 bit2 = (DrvColPROM[i + 0x000] >> 2) & 1;
		INT32 bit3 = (DrvColPROM[i + 0x000] >> 3) & 1;
		INT32 r = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[i + 0x100] >> 0) & 1;
		bit1 = (DrvColPROM[i + 0x100] >> 1) & 1;
		bit2 = (DrvColPROM[i + 0x100] >> 2) & 1;
		bit3 = (DrvColPROM[i + 0x100] >> 3) & 1;
		INT32 g = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[i + 0x200] >> 0) & 1;
		bit1 = (DrvColPROM[i + 0x200] >> 1) & 1;
		bit2 = (DrvColPROM[i + 0x200] >> 2) & 1;
		bit3 = (DrvColPROM[i + 0x200] >> 3) & 1;
		INT32 b = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0x180 - 4; offs >= 0; offs -= 4)
	{
		INT32 attr  = DrvSprBuf[offs + 1];
		INT32 code  = DrvSprBuf[offs + 0] + ((attr << 2) & 0x300);
		INT32 sx    = DrvSprBuf[offs + 3] - ((attr << 8) & 0x100);
		INT32 sy    = DrvSprBuf[offs + 2];
		INT32 color = (attr >> 4) & 3;
		INT32 flipx = attr & 0x04;
		INT32 flipy = attr & 0x08;

		if (code >= 0x300) continue;

		if (flipscreen)
		{
			sx = 240 - sx;
			sy = 240 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		Draw16x16MaskTile(pTransDraw, code, sx, sy - 16, flipx, flipy, color, 4, 0xf, 0x80, DrvGfxROM[2]);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	GenericTilemapSetScrollX(1, scrollx);
	GenericTilemapSetScrollY(1, scrolly);

	if (~nBurnLayer & 1) BurnTransferClear();
	if ( nBurnLayer & 1) GenericTilemapDraw(1, pTransDraw, 0);

	if (nSpriteEnable & 1) draw_sprites();

	if ( nBurnLayer & 2) GenericTilemapDraw(0, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void DrvClearOpposites(UINT8* di)
{
	if ((*di & 0x03) == 0x00) {
		*di |= 0x03;
	}

	if ((*di & 0x0c) == 0x00) {
		*di |= 0x0c;
	}
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++)
		{
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
		DrvClearOpposites(&DrvInputs[1]);
		DrvClearOpposites(&DrvInputs[2]);
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 3000000 / 60, 3000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		CPU_RUN(0, Zet);
		if (i == 240) {
			if (pBurnDraw) {
				BurnDrvRedraw();
			}
			ZetSetVector(0xd7);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
			memcpy (DrvSprBuf, DrvSprRAM, 0x180);
		}
		ZetClose();

		ZetOpen(1);
		BurnTimerUpdate((i + 1) * (nCyclesTotal[1] / nInterleave));
		if ((i & ((nInterleave / 4) - 1)) == ((nInterleave / 4) - 1)) { // 4x per frame
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
		ZetClose();
	}

	ZetOpen(1);
	BurnTimerEndFrame(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();


	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029672;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		ZetScan(nAction);

		BurnYM2203Scan(nAction, pnMin);

		SCAN_VAR(flipscreen);
		SCAN_VAR(soundlatch);
		SCAN_VAR(scrollx);
		SCAN_VAR(scrolly);
	}

	return 0;
}


// Commando (World)

static struct BurnRomInfo commandoRomDesc[] = {
	{ "cm04.9m",					0x8000, 0x8438b694, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code (Encrypted)
	{ "cm03.8m",					0x4000, 0x35486542, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "cm02.9f",					0x4000, 0xf9cc4a74, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "vt01.5d",					0x4000, 0x505726e0, 3 | BRF_GRA },           //  3 Characters

	{ "vt11.5a",					0x4000, 0x7b2e1b48, 4 | BRF_GRA },           //  4 Background Tiles
	{ "vt12.6a",					0x4000, 0x81b417d3, 4 | BRF_GRA },           //  5
	{ "vt13.7a",					0x4000, 0x5612dbd2, 4 | BRF_GRA },           //  6
	{ "vt14.8a",					0x4000, 0x2b2dee36, 4 | BRF_GRA },           //  7
	{ "vt15.9a",					0x4000, 0xde70babf, 4 | BRF_GRA },           //  8
	{ "vt16.10a",					0x4000, 0x14178237, 4 | BRF_GRA },           //  9

	{ "vt05.7e",					0x4000, 0x79f16e3d, 5 | BRF_GRA },           // 10 Sprites
	{ "vt06.8e",					0x4000, 0x26fee521, 5 | BRF_GRA },           // 11
	{ "vt07.9e",					0x4000, 0xca88bdfd, 5 | BRF_GRA },           // 12
	{ "vt08.7h",					0x4000, 0x2019c883, 5 | BRF_GRA },           // 13
	{ "vt09.8h",					0x4000, 0x98703982, 5 | BRF_GRA },           // 14
	{ "vt10.9h",					0x4000, 0xf069d2f8, 5 | BRF_GRA },           // 15

	{ "vtb1.1d",					0x0100, 0x3aba15a1, 6 | BRF_GRA },           // 16 Color PROMs
	{ "vtb2.2d",					0x0100, 0x88865754, 6 | BRF_GRA },           // 17
	{ "vtb3.3d",					0x0100, 0x4c14c3f6, 6 | BRF_GRA },           // 18

	{ "vtb4.1h",					0x0100, 0xb388c246, 0 | BRF_OPT },           // 19 Misc. PROMs
	{ "vtb5.6l",					0x0100, 0x712ac508, 0 | BRF_OPT },           // 20
	{ "vtb6.6e",					0x0100, 0x0eaf5158, 0 | BRF_OPT },           // 21
};

STD_ROM_PICK(commando)
STD_ROM_FN(commando)

static INT32 CommandoInit()
{
	return DrvInit(0,0); // first opcode NOT encrypted
}

struct BurnDriver BurnDrvCommando = {
	"commando", NULL, NULL, NULL, "1985",
	"Commando (World)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_CAPCOM_MISC, GBF_RUNGUN, 0,
	NULL, commandoRomInfo, commandoRomName, NULL, NULL, NULL, NULL, CommandoInputInfo, CommandoDIPInfo,
	CommandoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Commando (US set 1)

static struct BurnRomInfo commandouRomDesc[] = {
	{ "u4-f.9m",					0x8000, 0xa6118935, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code (Encrypted)
	{ "u3-f.8m",					0x4000, 0x24f49684, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "cm02.9f",					0x4000, 0xf9cc4a74, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "vt01.5d",					0x4000, 0x505726e0, 3 | BRF_GRA },           //  3 Characters

	{ "vt11.5a",					0x4000, 0x7b2e1b48, 4 | BRF_GRA },           //  4 Background Tiles
	{ "vt12.6a",					0x4000, 0x81b417d3, 4 | BRF_GRA },           //  5
	{ "vt13.7a",					0x4000, 0x5612dbd2, 4 | BRF_GRA },           //  6
	{ "vt14.8a",					0x4000, 0x2b2dee36, 4 | BRF_GRA },           //  7
	{ "vt15.9a",					0x4000, 0xde70babf, 4 | BRF_GRA },           //  8
	{ "vt16.10a",					0x4000, 0x14178237, 4 | BRF_GRA },           //  9

	{ "vt05.7e",					0x4000, 0x79f16e3d, 5 | BRF_GRA },           // 10 Sprites
	{ "vt06.8e",					0x4000, 0x26fee521, 5 | BRF_GRA },           // 11
	{ "vt07.9e",					0x4000, 0xca88bdfd, 5 | BRF_GRA },           // 12
	{ "vt08.7h",					0x4000, 0x2019c883, 5 | BRF_GRA },           // 13
	{ "vt09.8h",					0x4000, 0x98703982, 5 | BRF_GRA },           // 14
	{ "vt10.9h",					0x4000, 0xf069d2f8, 5 | BRF_GRA },           // 15

	{ "vtb1.1d",					0x0100, 0x3aba15a1, 6 | BRF_GRA },           // 16 Color PROMs
	{ "vtb2.2d",					0x0100, 0x88865754, 6 | BRF_GRA },           // 17
	{ "vtb3.3d",					0x0100, 0x4c14c3f6, 6 | BRF_GRA },           // 18

	{ "vtb4.1h",					0x0100, 0xb388c246, 0 | BRF_OPT },           // 19 Misc. PROMs
	{ "vtb5.6l",					0x0100, 0x712ac508, 0 | BRF_OPT },           // 20
	{ "vtb6.6e",					0x0100, 0x0eaf5158, 0 | BRF_OPT },           // 21
};

STD_ROM_PICK(commandou)
STD_ROM_FN(commandou)

struct BurnDriver BurnDrvCommandou = {
	"commandou", "commando", NULL, NULL, "1985",
	"Commando (US set 1)\0", NULL, "Capcom (Data East USA license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_CAPCOM_MISC, GBF_RUNGUN, 0,
	NULL, commandouRomInfo, commandouRomName, NULL, NULL, NULL, NULL, CommandoInputInfo, CommandoDIPInfo,
	CommandoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Senjou no Ookami

static struct BurnRomInfo commandojRomDesc[] = {
	{ "so04.9m",					0x8000, 0xd3f2bfb3, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code (Encrypted)
	{ "so03.8m",					0x4000, 0xed01f472, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "so02.9f",					0x4000, 0xca20aca5, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "vt01.5d",					0x4000, 0x505726e0, 3 | BRF_GRA },           //  3 Characters

	{ "vt11.5a",					0x4000, 0x7b2e1b48, 4 | BRF_GRA },           //  4 Background Tiles
	{ "vt12.6a",					0x4000, 0x81b417d3, 4 | BRF_GRA },           //  5
	{ "vt13.7a",					0x4000, 0x5612dbd2, 4 | BRF_GRA },           //  6
	{ "vt14.8a",					0x4000, 0x2b2dee36, 4 | BRF_GRA },           //  7
	{ "vt15.9a",					0x4000, 0xde70babf, 4 | BRF_GRA },           //  8
	{ "vt16.10a",					0x4000, 0x14178237, 4 | BRF_GRA },           //  9

	{ "vt05.7e",					0x4000, 0x79f16e3d, 5 | BRF_GRA },           // 10 Sprites
	{ "vt06.8e",					0x4000, 0x26fee521, 5 | BRF_GRA },           // 11
	{ "vt07.9e",					0x4000, 0xca88bdfd, 5 | BRF_GRA },           // 12
	{ "vt08.7h",					0x4000, 0x2019c883, 5 | BRF_GRA },           // 13
	{ "vt09.8h",					0x4000, 0x98703982, 5 | BRF_GRA },           // 14
	{ "vt10.9h",					0x4000, 0xf069d2f8, 5 | BRF_GRA },           // 15

	{ "vtb1.1d",					0x0100, 0x3aba15a1, 6 | BRF_GRA },           // 16 Color PROMs
	{ "vtb2.2d",					0x0100, 0x88865754, 6 | BRF_GRA },           // 17
	{ "vtb3.3d",					0x0100, 0x4c14c3f6, 6 | BRF_GRA },           // 18

	{ "vtb4.1h",					0x0100, 0xb388c246, 0 | BRF_OPT },           // 19 Misc. PROMs
	{ "vtb5.6l",					0x0100, 0x712ac508, 0 | BRF_OPT },           // 20
	{ "vtb6.6e",					0x0100, 0x0eaf5158, 0 | BRF_OPT },           // 21
};

STD_ROM_PICK(commandoj)
STD_ROM_FN(commandoj)

struct BurnDriver BurnDrvCommandoj = {
	"commandoj", "commando", NULL, NULL, "1985",
	"Senjou no Ookami\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_CAPCOM_MISC, GBF_RUNGUN, 0,
	NULL, commandojRomInfo, commandojRomName, NULL, NULL, NULL, NULL, CommandoInputInfo, CommandoDIPInfo,
	CommandoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Commando (bootleg set 1)

static struct BurnRomInfo commandobRomDesc[] = {
	{ "commandob_04_9m_27256.bin",	0x8000, 0x348a7654, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code (Encrypted)
	{ "cm03.8m",					0x4000, 0x35486542, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "cm02.9f",					0x4000, 0xf9cc4a74, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "vt01.5d",					0x4000, 0x505726e0, 3 | BRF_GRA },           //  3 Characters

	{ "vt11.5a",					0x4000, 0x7b2e1b48, 4 | BRF_GRA },           //  4 Background Tiles
	{ "vt12.6a",					0x4000, 0x81b417d3, 4 | BRF_GRA },           //  5
	{ "vt13.7a",					0x4000, 0x5612dbd2, 4 | BRF_GRA },           //  6
	{ "vt14.8a",					0x4000, 0x2b2dee36, 4 | BRF_GRA },           //  7
	{ "vt15.9a",					0x4000, 0xde70babf, 4 | BRF_GRA },           //  8
	{ "vt16.10a",					0x4000, 0x14178237, 4 | BRF_GRA },           //  9

	{ "vt05.7e",					0x4000, 0x79f16e3d, 5 | BRF_GRA },           // 10 Sprites
	{ "vt06.8e",					0x4000, 0x26fee521, 5 | BRF_GRA },           // 11
	{ "vt07.9e",					0x4000, 0xca88bdfd, 5 | BRF_GRA },           // 12
	{ "vt08.7h",					0x4000, 0x2019c883, 5 | BRF_GRA },           // 13
	{ "vt09.8h",					0x4000, 0x98703982, 5 | BRF_GRA },           // 14
	{ "vt10.9h",					0x4000, 0xf069d2f8, 5 | BRF_GRA },           // 15

	{ "vtb1.1d",					0x0100, 0x3aba15a1, 6 | BRF_GRA },           // 16 Color PROMs
	{ "vtb2.2d",					0x0100, 0x88865754, 6 | BRF_GRA },           // 17
	{ "vtb3.3d",					0x0100, 0x4c14c3f6, 6 | BRF_GRA },           // 18

	{ "vtb4.1h",					0x0100, 0xb388c246, 0 | BRF_OPT },           // 19 Misc. PROMs
	{ "vtb5.6l",					0x0100, 0x712ac508, 0 | BRF_OPT },           // 20
	{ "vtb6.6e",					0x0100, 0x0eaf5158, 0 | BRF_OPT },           // 21

	{ "commandob_pal16l8a.bin",		0x0104, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 22 PLDs
};

STD_ROM_PICK(commandob)
STD_ROM_FN(commandob)

static INT32 CommandobInit()
{
	return DrvInit(1,0); // first opcode IS encrypted
}

struct BurnDriver BurnDrvCommandob = {
	"commandob", "commando", NULL, NULL, "1985",
	"Commando (bootleg set 1)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED | BDF_BOOTLEG, 2, HARDWARE_PREFIX_CAPCOM_MISC, GBF_RUNGUN, 0,
	NULL, commandobRomInfo, commandobRomName, NULL, NULL, NULL, NULL, CommandoInputInfo, CommandoDIPInfo,
	CommandobInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Commando (bootleg set 2)

static struct BurnRomInfo commandob2RomDesc[] = {
	{ "10",							0x8000, 0xab5d1469, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code (Encrypted)
	{ "11",							0x4000, 0xd1a43ba1, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "8,so02.9f",					0x4000, 0xca20aca5, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "7,vt01.5d",					0x4000, 0x505726e0, 3 | BRF_GRA },           //  3 Characters

	{ "17,vt11.5a",					0x4000, 0x7b2e1b48, 4 | BRF_GRA },           //  4 Background Tiles
	{ "16,vt12.6a",					0x4000, 0x81b417d3, 4 | BRF_GRA },           //  5
	{ "15,vt13.7a",					0x4000, 0x5612dbd2, 4 | BRF_GRA },           //  6
	{ "14,vt14.8a",					0x4000, 0x2b2dee36, 4 | BRF_GRA },           //  7
	{ "13,vt15.9a",					0x4000, 0xde70babf, 4 | BRF_GRA },           //  8
	{ "12,vt16.10a",				0x4000, 0x14178237, 4 | BRF_GRA },           //  9

	{ "3,vt05.7e",					0x4000, 0x79f16e3d, 5 | BRF_GRA },           // 10 Sprites
	{ "2,vt06.8e",					0x4000, 0x26fee521, 5 | BRF_GRA },           // 11
	{ "1,vt07.9e",					0x4000, 0xca88bdfd, 5 | BRF_GRA },           // 12
	{ "6,vt08.7h",					0x4000, 0x2019c883, 5 | BRF_GRA },           // 13
	{ "5,vt09.8h",					0x4000, 0x98703982, 5 | BRF_GRA },           // 14
	{ "4,vt10.9h",					0x4000, 0xf069d2f8, 5 | BRF_GRA },           // 15

	{ "vtb1.1d",					0x0100, 0x3aba15a1, 6 | BRF_GRA },           // 16 Color PROMs
	{ "vtb2.2d",					0x0100, 0x88865754, 6 | BRF_GRA },           // 17
	{ "vtb3.3d",					0x0100, 0x4c14c3f6, 6 | BRF_GRA },           // 18

	{ "vtb4.1h",					0x0100, 0xb388c246, 0 | BRF_OPT },           // 19 Misc. PROMs
	{ "vtb5.6l",					0x0100, 0x712ac508, 0 | BRF_OPT },           // 20
	{ "vtb6.6e",					0x0100, 0x0eaf5158, 0 | BRF_OPT },           // 21

	{ "commandob2_pal16l8.bin",		0x0104, 0xbdbcaf02, 0 | BRF_OPT },           // 22 plds
};

STD_ROM_PICK(commandob2)
STD_ROM_FN(commandob2)

struct BurnDriver BurnDrvCommandob2 = {
	"commandob2", "commando", NULL, NULL, "1985",
	"Commando (bootleg set 2)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED | BDF_BOOTLEG, 2, HARDWARE_PREFIX_CAPCOM_MISC, GBF_RUNGUN, 0,
	NULL, commandob2RomInfo, commandob2RomName, NULL, NULL, NULL, NULL, CommandoInputInfo, CommandoDIPInfo,
	CommandoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Commando (bootleg set 3)

static struct BurnRomInfo commandob3RomDesc[] = {
	{ "b5.10n",						0x4000, 0xdf8f4e9a, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code (Encrypted)
	{ "b4.9n",						0x4000, 0xaca99905, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "b3.8n",						0x4000, 0x35486542, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "b2.9f",						0x4000, 0xf9cc4a74, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "b1.5d",						0x4000, 0x505726e0, 3 | BRF_GRA },           //  4 Characters

	{ "b12.5a",						0x4000, 0x7b2e1b48, 4 | BRF_GRA },           //  5 Background Tiles
	{ "b13.6a",						0x4000, 0x81b417d3, 4 | BRF_GRA },           //  6
	{ "b14.7a",						0x4000, 0x5612dbd2, 4 | BRF_GRA },           //  7
	{ "b15.8a",						0x4000, 0x2b2dee36, 4 | BRF_GRA },           //  8
	{ "b16.9a",						0x4000, 0xde70babf, 4 | BRF_GRA },           //  9
	{ "b17.10a",					0x4000, 0x14178237, 4 | BRF_GRA },           // 10

	{ "b6.7e",						0x4000, 0x79f16e3d, 5 | BRF_GRA },           // 11 Sprites
	{ "b7.8e",						0x4000, 0x26fee521, 5 | BRF_GRA },           // 12
	{ "b8.9e",						0x4000, 0xca88bdfd, 5 | BRF_GRA },           // 13
	{ "b9.7h",						0x4000, 0x2019c883, 5 | BRF_GRA },           // 14
	{ "b10.8h",						0x4000, 0x98703982, 5 | BRF_GRA },           // 15
	{ "b11.9h",						0x4000, 0xf069d2f8, 5 | BRF_GRA },           // 16

	{ "vtb1.1d",					0x0100, 0x3aba15a1, 6 | BRF_GRA },           // 17 Color PROMs
	{ "vtb2.2d",					0x0100, 0x88865754, 6 | BRF_GRA },           // 18
	{ "vtb3.3d",					0x0100, 0x4c14c3f6, 6 | BRF_GRA },           // 19

	{ "vtb4.1h",					0x0100, 0xb388c246, 0 | BRF_OPT },           // 20 Misc. PROMs
	{ "vtb5.6l",					0x0100, 0x712ac508, 0 | BRF_OPT },           // 21
	{ "vtb6.6e",					0x0100, 0x0eaf5158, 0 | BRF_OPT },           // 22
};

STD_ROM_PICK(commandob3)
STD_ROM_FN(commandob3)

static INT32 Commandob3Init()
{
	return DrvInit(0,1);
}

struct BurnDriver BurnDrvCommandob3 = {
	"commandob3", "commando", NULL, NULL, "1985",
	"Commando (bootleg set 3)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED | BDF_BOOTLEG, 2, HARDWARE_PREFIX_CAPCOM_MISC, GBF_RUNGUN, 0,
	NULL, commandob3RomInfo, commandob3RomName, NULL, NULL, NULL, NULL, CommandoInputInfo, CommandoDIPInfo,
	Commandob3Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Commando (US set 2)

static struct BurnRomInfo commandou2RomDesc[] = {
	{ "uc4.9m",						0x8000, 0x89ee8e17, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code (Encrypted)
	{ "uc3.8m",						0x4000, 0x72a1a529, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "cd02.9f",					0x4000, 0xf9cc4a74, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "vt01.5d",					0x4000, 0x505726e0, 3 | BRF_GRA },           //  3 Characters

	{ "vt11.5a",					0x4000, 0x7b2e1b48, 4 | BRF_GRA },           //  4 Background Tiles
	{ "vt12.6a",					0x4000, 0x81b417d3, 4 | BRF_GRA },           //  5
	{ "vt13.7a",					0x4000, 0x5612dbd2, 4 | BRF_GRA },           //  6
	{ "vt14.8a",					0x4000, 0x2b2dee36, 4 | BRF_GRA },           //  7
	{ "vt15.9a",					0x4000, 0xde70babf, 4 | BRF_GRA },           //  8
	{ "vt16.10a",					0x4000, 0x14178237, 4 | BRF_GRA },           //  9

	{ "vt05.7e",					0x4000, 0x79f16e3d, 5 | BRF_GRA },           // 10 Sprites
	{ "vt06.8e",					0x4000, 0x26fee521, 5 | BRF_GRA },           // 11
	{ "vt07.9e",					0x4000, 0xca88bdfd, 5 | BRF_GRA },           // 12
	{ "vt08.7h",					0x4000, 0x2019c883, 5 | BRF_GRA },           // 13
	{ "vt09.8h",					0x4000, 0x98703982, 5 | BRF_GRA },           // 14
	{ "vt10.9h",					0x4000, 0xf069d2f8, 5 | BRF_GRA },           // 15

	{ "vtb-1.1d",					0x0100, 0x3aba15a1, 6 | BRF_GRA },           // 16 Color PROMs
	{ "vtb-2.2d",					0x0100, 0x88865754, 6 | BRF_GRA },           // 17
	{ "vtb-3.3d",					0x0100, 0x4c14c3f6, 6 | BRF_GRA },           // 18

	{ "vtb-4.1h",					0x0100, 0xb388c246, 0 | BRF_OPT },           // 19 Misc. PROMs
	{ "vtb-5.6l",					0x0100, 0x712ac508, 0 | BRF_OPT },           // 20
	{ "vtb-6.6e",					0x0100, 0x0eaf5158, 0 | BRF_OPT },           // 21
};

STD_ROM_PICK(commandou2)
STD_ROM_FN(commandou2)

struct BurnDriver BurnDrvCommandou2 = {
	"commandou2", "commando", NULL, NULL, "1985",
	"Commando (US set 2)\0", NULL, "Capcom (Data East USA license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_CAPCOM_MISC, GBF_RUNGUN, 0,
	NULL, commandou2RomInfo, commandou2RomName, NULL, NULL, NULL, NULL, CommandoInputInfo, CommandoDIPInfo,
	CommandoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Space Invasion (Europe)

static struct BurnRomInfo sinvasnRomDesc[] = {
	{ "sp04.9m",					0x8000, 0x33f9601e, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code (Encrypted)
	{ "sp03.8m",					0x4000, 0xc7fb43b3, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "u2.9f",						0x4000, 0xcbf8c40e, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "u1.5d",						0x4000, 0xf477e13a, 3 | BRF_GRA },           //  3 Characters

	{ "vt11.5a",					0x4000, 0x7b2e1b48, 4 | BRF_GRA },           //  4 Background Tiles
	{ "vt12.6a",					0x4000, 0x81b417d3, 4 | BRF_GRA },           //  5
	{ "vt13.7a",					0x4000, 0x5612dbd2, 4 | BRF_GRA },           //  6
	{ "vt14.8a",					0x4000, 0x2b2dee36, 4 | BRF_GRA },           //  7
	{ "vt15.9a",					0x4000, 0xde70babf, 4 | BRF_GRA },           //  8
	{ "vt16.10a",					0x4000, 0x14178237, 4 | BRF_GRA },           //  9

	{ "u5.e7",						0x4000, 0x2a97c933, 5 | BRF_GRA },           // 10 Sprites
	{ "sp06.e8",					0x4000, 0xd7887212, 5 | BRF_GRA },           // 11
	{ "sp07.e9",					0x4000, 0x9abe7a20, 5 | BRF_GRA },           // 12
	{ "u8.h7",						0x4000, 0xd6b4aa2e, 5 | BRF_GRA },           // 13
	{ "sp09.h8",					0x4000, 0x3985b318, 5 | BRF_GRA },           // 14
	{ "sp10.h9",					0x4000, 0x3c131b0f, 5 | BRF_GRA },           // 15

	{ "vtb1.1d",					0x0100, 0x3aba15a1, 6 | BRF_GRA },           // 16 Color PROMs
	{ "vtb2.2d",					0x0100, 0x88865754, 6 | BRF_GRA },           // 17
	{ "vtb3.3d",					0x0100, 0x4c14c3f6, 6 | BRF_GRA },           // 18

	{ "vtb4.1h",					0x0100, 0xb388c246, 0 | BRF_OPT },           // 19 Misc. PROMs
	{ "vtb5.6l",					0x0100, 0x712ac508, 0 | BRF_OPT },           // 20
	{ "vtb6.6e",					0x0100, 0x0eaf5158, 0 | BRF_OPT },           // 21
};

STD_ROM_PICK(sinvasn)
STD_ROM_FN(sinvasn)

struct BurnDriver BurnDrvSinvasn = {
	"sinvasn", "commando", NULL, NULL, "1985",
	"Space Invasion (Europe)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_CAPCOM_MISC, GBF_RUNGUN, 0,
	NULL, sinvasnRomInfo, sinvasnRomName, NULL, NULL, NULL, NULL, CommandoInputInfo, CommandoDIPInfo,
	CommandoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Space Invasion (bootleg)

static struct BurnRomInfo sinvasnbRomDesc[] = {
	{ "u4",							0x8000, 0x834ba0de, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code (Encrypted)
	{ "u3",							0x4000, 0x07e4ee3a, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "u2",							0x4000, 0xcbf8c40e, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "u1",							0x4000, 0xf477e13a, 3 | BRF_GRA },           //  3 Characters

	{ "vt11.5a",					0x4000, 0x7b2e1b48, 4 | BRF_GRA },           //  4 Background Tiles
	{ "vt12.6a",					0x4000, 0x81b417d3, 4 | BRF_GRA },           //  5
	{ "vt13.7a",					0x4000, 0x5612dbd2, 4 | BRF_GRA },           //  6
	{ "vt14.8a",					0x4000, 0x2b2dee36, 4 | BRF_GRA },           //  7
	{ "vt15.9a",					0x4000, 0xde70babf, 4 | BRF_GRA },           //  8
	{ "vt16.10a",					0x4000, 0x14178237, 4 | BRF_GRA },           //  9

	{ "u5",							0x4000, 0x2a97c933, 5 | BRF_GRA },           // 10 Sprites
	{ "vt06.e8",					0x4000, 0x26fee521, 5 | BRF_GRA },           // 11
	{ "vt07.e9",					0x4000, 0xca88bdfd, 5 | BRF_GRA },           // 12
	{ "u8",							0x4000, 0xd6b4aa2e, 5 | BRF_GRA },           // 13
	{ "vt09.h8",					0x4000, 0x98703982, 5 | BRF_GRA },           // 14
	{ "vt10.h9",					0x4000, 0xf069d2f8, 5 | BRF_GRA },           // 15

	{ "vtb1.1d",					0x0100, 0x3aba15a1, 6 | BRF_GRA },           // 16 Color PROMs
	{ "vtb2.2d",					0x0100, 0x88865754, 6 | BRF_GRA },           // 17
	{ "vtb3.3d",					0x0100, 0x4c14c3f6, 6 | BRF_GRA },           // 18

	{ "vtb4.1h",					0x0100, 0xb388c246, 0 | BRF_OPT },           // 19 Misc. PROMs
	{ "vtb5.6l",					0x0100, 0x712ac508, 0 | BRF_OPT },           // 20
	{ "vtb6.6e",					0x0100, 0x0eaf5158, 0 | BRF_OPT },           // 21
};

STD_ROM_PICK(sinvasnb)
STD_ROM_FN(sinvasnb)

struct BurnDriver BurnDrvSinvasnb = {
	"sinvasnb", "commando", NULL, NULL, "1985",
	"Space Invasion (bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED | BDF_BOOTLEG, 2, HARDWARE_PREFIX_CAPCOM_MISC, GBF_RUNGUN, 0,
	NULL, sinvasnbRomInfo, sinvasnbRomName, NULL, NULL, NULL, NULL, CommandoInputInfo, CommandoDIPInfo,
	CommandobInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Mercenario (bootleg of Commando)

static struct BurnRomInfo mercenarioRomDesc[] = {
	{ "4ac.bin",					0x4000, 0x59ebf408, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code (Encrypted)
	{ "4bc.bin",					0x4000, 0xaca99905, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "b3.8n",						0x4000, 0xf998d08a, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "cm02.9f",					0x4000, 0xf9cc4a74, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "1c.5d",						0x4000, 0xfe3ebe35, 3 | BRF_GRA },           //  4 Characters

	{ "vt11.5a",					0x4000, 0x7b2e1b48, 4 | BRF_GRA },           //  5 Background Tiles
	{ "vt12.6a",					0x4000, 0x81b417d3, 4 | BRF_GRA },           //  6
	{ "vt13.7a",					0x4000, 0x5612dbd2, 4 | BRF_GRA },           //  7
	{ "vt14.8a",					0x4000, 0x2b2dee36, 4 | BRF_GRA },           //  8
	{ "vt15.9a",					0x4000, 0xde70babf, 4 | BRF_GRA },           //  9
	{ "vt16.10a",					0x4000, 0x14178237, 4 | BRF_GRA },           // 10

	{ "vt05.7e",					0x4000, 0x79f16e3d, 5 | BRF_GRA },           // 11 Sprites
	{ "vt06.8e",					0x4000, 0x26fee521, 5 | BRF_GRA },           // 12
	{ "vt07.9e",					0x4000, 0xca88bdfd, 5 | BRF_GRA },           // 13
	{ "vt08.7h",					0x4000, 0x2019c883, 5 | BRF_GRA },           // 14
	{ "vt09.8h",					0x4000, 0x98703982, 5 | BRF_GRA },           // 15
	{ "vt10.9h",					0x4000, 0xf069d2f8, 5 | BRF_GRA },           // 16

	{ "vtb1.1d",					0x0100, 0x3aba15a1, 6 | BRF_GRA },           // 17 Color PROMs
	{ "vtb2.2d",					0x0100, 0x88865754, 6 | BRF_GRA },           // 18
	{ "vtb3.3d",					0x0100, 0x4c14c3f6, 6 | BRF_GRA },           // 19

	{ "vtb4.1h",					0x0100, 0xb388c246, 0 | BRF_OPT },           // 20 Misc. PROMs
	{ "vtb5.6l",					0x0100, 0x712ac508, 0 | BRF_OPT },           // 21
	{ "vtb6.6e",					0x0100, 0x0eaf5158, 0 | BRF_OPT },           // 22
};

STD_ROM_PICK(mercenario)
STD_ROM_FN(mercenario)

static INT32 MercenarioInit()
{
	return DrvInit(1,1);
}

struct BurnDriver BurnDrvMercenario = {
	"mercenario", "commando", NULL, NULL, "1985",
	"Mercenario (bootleg of Commando)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED | BDF_BOOTLEG, 2, HARDWARE_PREFIX_CAPCOM_MISC, GBF_RUNGUN, 0,
	NULL, mercenarioRomInfo, mercenarioRomName, NULL, NULL, NULL, NULL, CommandoInputInfo, CommandoDIPInfo,
	MercenarioInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};
