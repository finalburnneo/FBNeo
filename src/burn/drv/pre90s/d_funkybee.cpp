// FB Alpha Funky Bee Driver Module
// Based on MAME driver by Zsolt Vasvari

#include "tiles_generic.h"
#include "z80_intf.h"
#include "ay8910.h"
#include "watchdog.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvColRAM;

static UINT32 *DrvPalette;
static UINT8  DrvRecalc;

static UINT8 scroll;
static UINT8 flipscreen;
static UINT8 gfx_bank;

static INT32 netplay_mode = 1;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvInputs[3];
static UINT8 DrvDips[2];
static UINT8 DrvReset;

static struct BurnInputInfo DrvInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,   DrvJoy1 + 0,	"p1 coin"  },
	{"P1 start",    BIT_DIGITAL,   DrvJoy1 + 3,	"p1 start" },
	{"P1 Left",		BIT_DIGITAL,   DrvJoy2 + 0, "p1 left"  },
	{"P1 Right",	BIT_DIGITAL,   DrvJoy2 + 1, "p1 right" },
	{"P1 Up",		BIT_DIGITAL,   DrvJoy2 + 2, "p1 up",   },
	{"P1 Down",		BIT_DIGITAL,   DrvJoy2 + 3, "p1 down", },
	{"P1 Button 1",	BIT_DIGITAL,   DrvJoy2 + 4,	"p1 fire 1"},

	{"P2 Coin",		BIT_DIGITAL,   DrvJoy1 + 1, "p2 coin"  },
	{"P2 start",	BIT_DIGITAL,   DrvJoy1 + 4, "p2 start" },
	{"P2 Left",		BIT_DIGITAL,   DrvJoy3 + 0, "p2 left"  },
	{"P2 Right",	BIT_DIGITAL,   DrvJoy3 + 1, "p2 right" },
	{"P2 Up",		BIT_DIGITAL,   DrvJoy3 + 2, "p2 up",   },
	{"P2 Down",		BIT_DIGITAL,   DrvJoy3 + 3, "p2 down", },
	{"P2 Button 1",	BIT_DIGITAL,   DrvJoy3 + 4,	"p2 fire 1"},

	{"Reset",		BIT_DIGITAL,  &DrvReset,	"reset"    },
	{"Dip 1",		BIT_DIPSWITCH, DrvDips + 0,	"dip"	   },
	{"Dip 2",		BIT_DIPSWITCH, DrvDips + 1,	"dip"	   },
};

STDINPUTINFO(Drv)

static struct BurnDIPInfo funkybeeDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x20, NULL				},
	{0x10, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   , 2   , "Freeze"			},
	{0x0f, 0x01, 0x20, 0x20, "Off"				},
	{0x0f, 0x01, 0x20, 0x00, "On"				},

	{0   , 0xfe, 0   , 4   , "Coin A"			},
	{0x10, 0x01, 0x03, 0x03, "1C 1C"			},
	{0x10, 0x01, 0x03, 0x02, "1C 2C"			},
	{0x10, 0x01, 0x03, 0x01, "1C 3C"			},
	{0x10, 0x01, 0x03, 0x00, "1C 4C"			},

	{0   , 0xfe, 0   , 4   , "Coin B"			},
	{0x10, 0x01, 0x0c, 0x08, "2C 1C"			},
	{0x10, 0x01, 0x0c, 0x0c, "1C 1C"			},
	{0x10, 0x01, 0x0c, 0x04, "2C 3C"			},
	{0x10, 0x01, 0x0c, 0x00, "1C 6C"			},

	{0   , 0xfe, 0   , 2   , "Lives"			},
	{0x10, 0x01, 0x30, 0x30, "3"				},
	{0x10, 0x01, 0x30, 0x20, "4"				},
	{0x10, 0x01, 0x30, 0x10, "5"				},
	{0x10, 0x01, 0x30, 0x00, "6"				},

	{0   , 0xfe, 0   , 2   , "Bonus Life"		},
	{0x10, 0x01, 0x40, 0x40, "20000"			},
	{0x10, 0x01, 0x40, 0x00, "None"				},

	{0   , 0xfe, 0   , 2   , "Cabinet"			},
	{0x10, 0x01, 0x80, 0x00, "Upright"			},
	{0x10, 0x01, 0x80, 0x80, "Cocktail"			},
};

STDDIPINFO(funkybee)

static struct BurnDIPInfo funkbeebDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x20, NULL				},
	{0x10, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   , 2   , "Freeze"			},
	{0x0f, 0x01, 0x20, 0x20, "Off"				},
	{0x0f, 0x01, 0x20, 0x00, "On"				},

	{0   , 0xfe, 0   , 4   , "Coin A"			},
	{0x10, 0x01, 0x03, 0x03, "1C 1C"			},
	{0x10, 0x01, 0x03, 0x02, "1C 2C"			},
	{0x10, 0x01, 0x03, 0x01, "1C 3C"			},
	{0x10, 0x01, 0x03, 0x00, "1C 4C"			},

	{0   , 0xfe, 0   , 4   , "Coin B"			},
	{0x10, 0x01, 0x0c, 0x08, "2C 1C"			},
	{0x10, 0x01, 0x0c, 0x0c, "1C 1C"			},
	{0x10, 0x01, 0x0c, 0x04, "2C 3C"			},
	{0x10, 0x01, 0x0c, 0x00, "1C 6C"			},

	{0   , 0xfe, 0   , 2   , "Lives"			},
	{0x10, 0x01, 0x30, 0x30, "1"				},
	{0x10, 0x01, 0x30, 0x20, "2"				},
	{0x10, 0x01, 0x30, 0x10, "3"				},
	{0x10, 0x01, 0x30, 0x00, "4" 				},

	{0   , 0xfe, 0   , 2   , "Bonus Life"		},
	{0x10, 0x01, 0x40, 0x40, "20000"			},
	{0x10, 0x01, 0x40, 0x00, "None"				},

	{0   , 0xfe, 0   , 2   , "Cabinet"			},
	{0x10, 0x01, 0x80, 0x00, "Upright"			},
	{0x10, 0x01, 0x80, 0x80, "Cocktail"			},
};

STDDIPINFO(funkbeeb)

static struct BurnDIPInfo skylancrDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x20, NULL				},
	{0x10, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   , 2   , "Freeze"			},
	{0x0f, 0x01, 0x20, 0x20, "Off"				},
	{0x0f, 0x01, 0x20, 0x00, "On"				},

	{0   , 0xfe, 0   , 4   , "Coin A"			},
	{0x10, 0x01, 0x03, 0x03, "1C 1C"			},
	{0x10, 0x01, 0x03, 0x02, "1C 2C"			},
	{0x10, 0x01, 0x03, 0x01, "1C 3C"			},
	{0x10, 0x01, 0x03, 0x00, "1C 6C"			},

	{0   , 0xfe, 0   , 4   , "Coin B"			},
	{0x10, 0x01, 0x0c, 0x08, "2C 1C"			},
	{0x10, 0x01, 0x0c, 0x0c, "1C 1C"			},
	{0x10, 0x01, 0x0c, 0x04, "2C 3C"			},
	{0x10, 0x01, 0x0c, 0x00, "1C 6C"			},

	{0   , 0xfe, 0   , 2   , "Lives"			},
	{0x10, 0x01, 0x30, 0x30, "1"				},
	{0x10, 0x01, 0x30, 0x20, "2"				},
	{0x10, 0x01, 0x30, 0x10, "3"				},
	{0x10, 0x01, 0x30, 0x00, "4"				},

	{0   , 0xfe, 0   , 2   , "Bonus Life"		},
	{0x10, 0x01, 0x40, 0x40, "20000"			},
	{0x10, 0x01, 0x40, 0x00, "None"				},

	{0   , 0xfe, 0   , 2   , "Cabinet"			},
	{0x10, 0x01, 0x80, 0x00, "Upright" 			},
	{0x10, 0x01, 0x80, 0x80, "Cocktail"			},
};

STDDIPINFO(skylancr)

static struct BurnDIPInfo skylanceDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x20, NULL				},
	{0x10, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   , 2   , "Freeze"			},
	{0x0f, 0x01, 0x20, 0x20, "Off"				},
	{0x0f, 0x01, 0x20, 0x00, "On"				},

	{0   , 0xfe, 0   , 4   , "Coin A"			},
	{0x10, 0x01, 0x03, 0x03, "1C 1C"			},
	{0x10, 0x01, 0x03, 0x02, "1C 2C"			},
	{0x10, 0x01, 0x03, 0x01, "1C 3C"			},
	{0x10, 0x01, 0x03, 0x00, "1C 6C"			},

	{0   , 0xfe, 0   , 4   , "Coin B"			},
	{0x10, 0x01, 0x0c, 0x08, "2C 1C"			},
	{0x10, 0x01, 0x0c, 0x0c, "1C 1C"			},
	{0x10, 0x01, 0x0c, 0x04, "2C 3C"			},
	{0x10, 0x01, 0x0c, 0x00, "1C 6C"			},

	{0   , 0xfe, 0   , 2   , "Lives"			},
	{0x10, 0x01, 0x30, 0x30, "3"				},
	{0x10, 0x01, 0x30, 0x20, "4"				},
	{0x10, 0x01, 0x30, 0x10, "5"				},
	{0x10, 0x01, 0x30, 0x00, "6"				},

	{0   , 0xfe, 0   , 2   , "Bonus Life"		},
	{0x10, 0x01, 0x40, 0x40, "20000 50000"		},
	{0x10, 0x01, 0x40, 0x00, "40000 70000"		},

	{0   , 0xfe, 0   , 2   , "Cabinet"			},
	{0x10, 0x01, 0x80, 0x00, "Upright"			},
	{0x10, 0x01, 0x80, 0x80, "Cocktail"			},
};

STDDIPINFO(skylance)

static UINT8 __fastcall funkybee_read(UINT16 address)
{
	switch (address)
	{
		case 0xf000:
			return 0;

		case 0xf800:
			BurnWatchdogWrite();
			return DrvInputs[0] | (DrvDips[0] & 0xe0);

		case 0xf801:
			return DrvInputs[1];

		case 0xf802:
			return DrvInputs[2];
	}

	return 0;
}

static void __fastcall funkybee_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xe000:
			scroll = data;
		break;

		case 0xe800:
			flipscreen = data & 1;
		break;

		case 0xe802: // coin counter
		case 0xe803:
		break;

		case 0xe805:
			gfx_bank = data & 1;
		break;

		case 0xf800:
			BurnWatchdogWrite();
		break;
	}
}

static UINT8 __fastcall funkybee_in_port(UINT16 address)
{
	switch (address & 0xff)
	{
		case 0x02:
			return AY8910Read(0);
	}

	return 0;
}

static void __fastcall funkybee_out_port(UINT16 address, UINT8 data)
{
	switch (address & 0xff)
	{
		case 0x00:
		case 0x01:
			AY8910Write(0, address & 1, data);
		break; 
	}
}

static tilemap_scan( bg )
{
	return 256 * row + col;
}

static tilemap_callback( bg )
{
	INT32 attr = DrvColRAM[offs];
	INT32 code = DrvVidRAM[offs] + ((attr & 0x80) << 1) + (gfx_bank * 0x200);
	
	TILE_SET_INFO(0, code, attr, 0);
}

static UINT8 funkybee_ay8910_read_A(UINT32)
{
	return DrvDips[1];
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	ZetOpen(0);
	ZetReset();
	ZetClose();
	
	BurnWatchdogReset();

	HiscoreReset();

	scroll = 0;
	gfx_bank = 0;
	flipscreen = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x005000;

	DrvGfxROM		= Next; Next += 0x010000;

	DrvColPROM		= Next; Next += 0x000020;

	DrvPalette		= (UINT32*)Next; Next += 0x0020 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x000800;
	DrvVidRAM		= Next; Next += 0x002000;
	DrvColRAM		= Next; Next += 0x002000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void DrvGfxDecode()
{
	INT32 Planes[2] = { 0, 4 };
	INT32 XOffs[8]  = { STEP4(0, 1), STEP4(0x40, 1) };
	INT32 YOffs[32] = { STEP8(0, 8), STEP8(0x80, 8), STEP8(0x100, 8), STEP8(0x180, 8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x4000);
	if (tmp == NULL) {
		return;
	}

	memcpy (tmp, DrvGfxROM, 0x4000);

	GfxDecode(0x400, 2, 8,  8, Planes, XOffs, YOffs, 0x080, tmp, DrvGfxROM);

	BurnFree (tmp);
}

static INT32 DrvInit(INT32 game)
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvZ80ROM  + 0x0000, k++, 1)) return 1;

		if (game) {
			if (BurnLoadRom(DrvZ80ROM  + 0x2000, k++, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM  + 0x4000, k++, 1)) return 1;
		} else {
			if (BurnLoadRom(DrvZ80ROM  + 0x1000, k++, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM  + 0x2000, k++, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM  + 0x3000, k++, 1)) return 1;
		}

		if (BurnLoadRom(DrvGfxROM  + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM  + 0x2000, k++, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x0000, k++, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,	0x0000, 0x4fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,	0x8000, 0x87ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,	0xa000, 0xbfff, MAP_RAM);
	ZetMapMemory(DrvColRAM,	0xc000, 0xdfff, MAP_RAM);
	ZetSetWriteHandler(funkybee_write);
	ZetSetReadHandler(funkybee_read);
	ZetSetOutHandler(funkybee_out_port);
	ZetSetInHandler(funkybee_in_port);
	ZetClose();
	
	BurnWatchdogInit(DrvDoReset, 180);

	AY8910Init(0, 1500000, 0);
	AY8910SetPorts(0, &funkybee_ay8910_read_A, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.50, BURN_SND_ROUTE_BOTH);
	AY8910SetBuffered(ZetTotalCycles, 3072000);

	GenericTilesInit();
	GenericTilemapInit(0, bg_map_scan, bg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM, 2, 8, 8, 0x10000, 0, 3);

	//netplay_mode = is_netgame_or_recording();

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	AY8910Exit(0);

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x20; i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 0x01;
		INT32 r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = (DrvColPROM[i] >> 3) & 0x01;
		bit1 = (DrvColPROM[i] >> 4) & 0x01;
		bit2 = (DrvColPROM[i] >> 5) & 0x01;
		INT32 g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = 0;
		bit1 = (DrvColPROM[i] >> 6) & 0x01;
		bit2 = (DrvColPROM[i] >> 7) & 0x01;
		INT32 b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0x0f; offs >= 0; offs--)
	{
		INT32 ofst  = offs + 0x1e00;
		INT32 attr  = DrvVidRAM[ofst];
		INT32 sy    = 224 - DrvColRAM[ofst];
		INT32 sx    = DrvVidRAM[ofst + 0x10];
		INT32 color = DrvColRAM[ofst + 0x10] & 3;
		INT32 code = (attr >> 2) + ((attr & 2) << 5) + (gfx_bank * 0x080);
		INT32 flipy = attr & 1;
		INT32 flipx = 0;
		
		if (flipscreen) {
			if (netplay_mode) {
				sy = (224 - 32) - sy;
				sx = (256 - 8) - sx;
				sy -= 2; // ?
			}
			else
			{
				flipx ^= 1;
				sx += 12-9;
				sy += 1;
			}
		}

		DrawCustomMaskTile(pTransDraw, 8, 32, code, sx - 12, sy, flipx, flipy, color, 2, 0, 0x10, DrvGfxROM);
	}
}

static void draw_fg_layer()
{
	for (INT32 offs = 0x1f; offs >= 0; offs--)
	{
		INT32 sy    = (flipscreen) ? (248 - (offs * 8) - 32) : (offs * 8);
		INT32 sx    = flipscreen ? DrvVidRAM[0x1f1f] : DrvVidRAM[0x1f10];
		INT32 code  = DrvVidRAM[0x1c00 + offs] + (gfx_bank * 0x200);
		INT32 color = DrvColRAM[0x1f10] & 0x03;
		INT32 flip  = flipscreen;

		if (netplay_mode && flipscreen) {
			flip = 0;
			sy = (224 - 8) - sy;
			sx = (256 - 8) - sx + 4;
		}

		Draw8x8MaskTile(pTransDraw, code, sx - 12, sy, flip, flip, color, 2, 0, 0, DrvGfxROM);

		code  = DrvVidRAM[0x1d00 + offs] + (gfx_bank * 0x200);
		color = DrvColRAM[0x1f11] & 0x03;
		sx = flipscreen ? DrvVidRAM[0x1f1e] : DrvVidRAM[0x1f11];

		if (netplay_mode && flipscreen) {
			sx = (256 - 8) - sx + 4;
		}

		Draw8x8MaskTile(pTransDraw, code, sx - 12, sy, flip, flip, color, 2, 0, 0, DrvGfxROM);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	GenericTilemapSetFlip(0, (!netplay_mode && flipscreen) ? (TMAP_FLIPX | TMAP_FLIPY) : 0);
	GenericTilemapSetScrollX(0, flipscreen ? -(scroll - 8) : (scroll + 12));	
	GenericTilemapDraw(0, pTransDraw, 0);
	
	draw_sprites();
	draw_fg_layer();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();
	
	if (DrvReset) {
		DrvDoReset(1);
	}

	{
		memset (DrvInputs, 0, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}
	ZetNewFrame();
	ZetOpen(0);
	ZetRun(3072000 / 60);
	ZetSetIRQLine(0, CPU_IRQSTATUS_AUTO);
	ZetClose();

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction,INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029521;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);
		AY8910Scan(nAction, pnMin);
		BurnWatchdogScan(nAction);

		SCAN_VAR(scroll);
		SCAN_VAR(gfx_bank);
		SCAN_VAR(flipscreen);
	}

	return 0;
}


// Funky Bee

static struct BurnRomInfo funkybeeRomDesc[] = {
	{ "funkybee.1",		0x1000, 0x3372cb33, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "funkybee.3",		0x1000, 0x7bf7c62f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "funkybee.2",		0x1000, 0x8cc0fe8e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "funkybee.4",		0x1000, 0x1e1aac26, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "funkybee.5",		0x2000, 0x86126655, 2 | BRF_GRA },			 //  4 Graphics tiles
	{ "funkybee.6",		0x2000, 0x5fffd323, 2 | BRF_GRA },			 //  5

	{ "funkybee.clr",	0x0020, 0xe2cf5fe2, 3 | BRF_GRA },			 //  6 Color prom
};

STD_ROM_PICK(funkybee)
STD_ROM_FN(funkybee)

static INT32 funkybeeInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvfunkybee = {
	"funkybee", NULL, NULL, NULL, "1982",
	"Funky Bee\0", NULL, "Orca", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, funkybeeRomInfo, funkybeeRomName, NULL, NULL, NULL, NULL, DrvInputInfo, funkybeeDIPInfo,
	funkybeeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x20,
	224, 236, 3, 4
};


// Funky Bee (bootleg, harder)

static struct BurnRomInfo funkbeebRomDesc[] = {
	{ "senza_orca.fb1",	0x1000, 0x7f2e7f85, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "funkybee.3",		0x1000, 0x7bf7c62f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "funkybee.2",		0x1000, 0x8cc0fe8e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "senza_orca.fb4",	0x1000, 0x53c2db3b, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "funkybee.5",		0x2000, 0x86126655, 2 | BRF_GRA },			 //  4 Graphics tiles
	{ "funkybee.6",		0x2000, 0x5fffd323, 2 | BRF_GRA },			 //  5

	{ "funkybee.clr",	0x0020, 0xe2cf5fe2, 3 | BRF_GRA },			 //  6 Color prom
};

STD_ROM_PICK(funkbeeb)
STD_ROM_FN(funkbeeb)

struct BurnDriver BurnDrvfunkbeeb = {
	"funkybeeb", "funkybee", NULL, NULL, "1982",
	"Funky Bee (bootleg, harder)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, funkbeebRomInfo, funkbeebRomName, NULL, NULL, NULL, NULL, DrvInputInfo, funkbeebDIPInfo,
	funkybeeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x20,
	224, 236, 3, 4
};


// Sky Lancer

static struct BurnRomInfo skylancrRomDesc[] = {
	{ "1sl.5a",			0x2000, 0xe80b315e, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "2sl.5c",			0x2000, 0x9d70567b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3sl.5d",			0x2000, 0x64c39457, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "4sl.6a",			0x2000, 0x9b4469a5, 2 | BRF_GRA },			 //  3 Graphics tiles
	{ "5sl.6c",			0x2000, 0x29afa134, 2 | BRF_GRA },			 //  4

	{ "18s030.1a",		0x0020, 0xe645bacb, 3 | BRF_GRA },			 //  5 Color prom
};

STD_ROM_PICK(skylancr)
STD_ROM_FN(skylancr)

static INT32 skylancrInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvskylancr = {
	"skylancr", NULL, NULL, NULL, "1983",
	"Sky Lancer\0", NULL, "Orca", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, skylancrRomInfo, skylancrRomName, NULL, NULL, NULL, NULL, DrvInputInfo, skylancrDIPInfo,
	skylancrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x20,
	224, 236, 3, 4
};


// Sky Lancer (Esco Trading Co license)

static struct BurnRomInfo skylanceRomDesc[] = {
	{ "1.5a",			0x2000, 0x82d55824, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "2.5c",			0x2000, 0xdff3a682, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.5d",			0x1000, 0x7c006ee6, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "4.6a",			0x2000, 0x0f8ede07, 2 | BRF_GRA },			 //  3 Graphics tiles
	{ "5.6b",			0x2000, 0x24cec070, 2 | BRF_GRA },			 //  4

	{ "18s030.1a",		0x0020, 0xe645bacb, 3 | BRF_GRA },			 //  5 Color prom
};

STD_ROM_PICK(skylance)
STD_ROM_FN(skylance)

struct BurnDriver BurnDrvskylance = {
	"skylancre", "skylancr", NULL, NULL, "1983",
	"Sky Lancer (Esco Trading Co license)\0", NULL, "Orca (Esco Trading Co license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, skylanceRomInfo, skylanceRomName, NULL, NULL, NULL, NULL, DrvInputInfo, skylanceDIPInfo,
	skylancrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x20,
	224, 236, 3, 4
};
