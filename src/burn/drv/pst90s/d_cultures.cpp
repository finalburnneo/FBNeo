// FB Alpha Jibun wo Migaku Culture School Mahjong Hen driver module
// Based on MAME driver by Pierpaolo Prazzoli

#include "tiles_generic.h"
#include "z80_intf.h"
#include "msm6295.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvSndROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvPortRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 nBankData;
static UINT8 nOkiBank;
static UINT8 nIrqEnable;
static UINT8 nBgBank1;
static UINT8 nBgBank2;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvJoy5[8];
static UINT8 DrvJoy6[8];
static UINT8 DrvJoy7[8];
static UINT8 DrvDips[4];
static UINT8 DrvInputs[8];
static UINT8 DrvReset;

static struct BurnInputInfo CulturesInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy7 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy6 + 0,	"p1 start"	},
	{"A",			BIT_DIGITAL,	DrvJoy1 + 0,	"mah a"		},
	{"B",			BIT_DIGITAL,	DrvJoy1 + 1,	"mah b"		},
	{"C",			BIT_DIGITAL,	DrvJoy1 + 2,	"mah c"		},
	{"D",			BIT_DIGITAL,	DrvJoy1 + 3,	"mah d"		},
	{"E",			BIT_DIGITAL,	DrvJoy2 + 0,	"mah e"		},
	{"F",			BIT_DIGITAL,	DrvJoy2 + 1,	"mah f"		},
	{"G",			BIT_DIGITAL,	DrvJoy2 + 2,	"mah g"		},
	{"H",			BIT_DIGITAL,	DrvJoy2 + 3,	"mah h"		},
	{"I",			BIT_DIGITAL,	DrvJoy3 + 0,	"mah i"		},
	{"J",			BIT_DIGITAL,	DrvJoy3 + 1,	"mah j"		},
	{"K",			BIT_DIGITAL,	DrvJoy3 + 2,	"mah k"		},
	{"L",			BIT_DIGITAL,	DrvJoy3 + 3,	"mah l"		},
	{"M",			BIT_DIGITAL,	DrvJoy4 + 0,	"mah m"		},
	{"N",			BIT_DIGITAL,	DrvJoy4 + 1,	"mah n"		},
	{"Pon",			BIT_DIGITAL,	DrvJoy4 + 3,	"mah pon"	},
	{"Chi",			BIT_DIGITAL,	DrvJoy4 + 2,	"mah chi"	},
	{"Kan",			BIT_DIGITAL,	DrvJoy5 + 0,	"mah kan"	},
	{"Ron",			BIT_DIGITAL,	DrvJoy5 + 2,	"mah ron"	},
	{"Reach",		BIT_DIGITAL,	DrvJoy5 + 1,	"mah reach"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy7 + 5,	"service"	},
	{"Service",		BIT_DIGITAL,	DrvJoy7 + 6,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",		BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};

STDINPUTINFO(Cultures)

static struct BurnDIPInfo CulturesDIPList[]=
{
	{0x18, 0xff, 0xff, 0xf7, NULL				},
	{0x19, 0xff, 0xff, 0xff, NULL				},
	{0x1a, 0xff, 0xff, 0xff, NULL				},
	{0x1b, 0xff, 0xff, 0xfb, NULL				},

	{0   , 0xfe, 0   ,    8, "Coinage"			},
	{0x18, 0x01, 0x07, 0x00, "10 Coins / 1 Credit"		},
	{0x18, 0x01, 0x07, 0x01, "5 Coins 1 Credits"		},
	{0x18, 0x01, 0x07, 0x02, "4 Coins 1 Credits"		},
	{0x18, 0x01, 0x07, 0x03, "3 Coins 1 Credits"		},
	{0x18, 0x01, 0x07, 0x04, "2 Coins 1 Credits"		},
	{0x18, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x18, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x18, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x18, 0x01, 0x08, 0x00, "Off"				},
	{0x18, 0x01, 0x08, 0x08, "On"				},

	{0   , 0xfe, 0   ,    0, "Auto Mode After Reach"	},
	{0x19, 0x01, 0x01, 0x00, "No"				},
	{0x19, 0x01, 0x01, 0x01, "Yes"				},

	{0   , 0xfe, 0   ,    2, "Attract Mode"			},
	{0x19, 0x01, 0x02, 0x02, "Partial"			},
	{0x19, 0x01, 0x02, 0x00, "Full"				},

	{0   , 0xfe, 0   ,    2, "Open Hands After Noten"	},
	{0x19, 0x01, 0x04, 0x00, "No"				},
	{0x19, 0x01, 0x04, 0x04, "Yes"				},

	{0   , 0xfe, 0   ,    2, "Datsui Count After Continue"	},
	{0x19, 0x01, 0x08, 0x08, "Not Cleared"			},
	{0x19, 0x01, 0x08, 0x00, "Cleared"			},

	{0   , 0xfe, 0   ,    2, "Difficulty"			},
	{0x1a, 0x01, 0x03, 0x00, "Hardest"			},
	{0x1a, 0x01, 0x03, 0x01, "Hard"				},
	{0x1a, 0x01, 0x03, 0x03, "Normal"			},
	{0x1a, 0x01, 0x03, 0x02, "Easy"				},

	{0   , 0xfe, 0   ,    0, "Demo Sounds"			},
	{0x1a, 0x01, 0x04, 0x00, "Off"				},
	{0x1a, 0x01, 0x04, 0x04, "On"				},

	{0   , 0xfe, 0   ,    4, "Game Background Music"	},
	{0x1a, 0x01, 0x08, 0x00, "No"				},
	{0x1a, 0x01, 0x08, 0x08, "Yes"				},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x1b, 0x01, 0x01, 0x00, "No"				},
	{0x1b, 0x01, 0x01, 0x01, "Yes"				},

	{0   , 0xfe, 0   ,    2, "Machihai Display"		},
	{0x1b, 0x01, 0x02, 0x00, "No"				},
	{0x1b, 0x01, 0x02, 0x02, "Yes"				},

	{0   , 0xfe, 0   ,    0, "Service Mode"			},
	{0x1b, 0x01, 0x08, 0x08, "Off"				},
	{0x1b, 0x01, 0x08, 0x00, "On"				},
};

STDDIPINFO(Cultures)

static void bankswitch(INT32 data)
{
	nBankData = data;

	ZetMapMemory(DrvZ80ROM + ((data & 0xf) * 0x4000), 0x4000, 0x7fff, MAP_ROM);

	if (data & 0x20)
	{
		ZetMapMemory(DrvPalRAM,		0x8000, 0xafff, MAP_RAM);
		ZetUnmapMemory(			0xb000, 0xbfff, MAP_RAM);
	}
	else
	{	
		ZetMapMemory(DrvVidRAM,		0x8000, 0xbfff, MAP_RAM);
	}
}

static void set_oki_bank(INT32 data)
{
	nOkiBank = data;

	MSM6295SetBank(0, DrvSndROM + ((data & 0xf) * 0x20000), 0x20000, 0x3ffff);
}

static void __fastcall cultures_write_port(UINT16 port, UINT8 data)
{
	port &= 0xff;

	if (port < 0x80 && (port & 0xf) < 3) {
		DrvPortRAM[port] = data;
		return;
	}

	switch (port)
	{
		case 0x80:
			bankswitch(data);
		return;

		case 0x90:
			nIrqEnable = data & 0x80;
		return;

		case 0xa0:
			nBgBank1 = data & 0x03;
			nBgBank2 =(data & 0x0c) >> 2;
		//	coin counter = data & 0x10
		return;

		case 0xc0:
			MSM6295Write(0, data);
		return;
	}
}

static UINT8 __fastcall cultures_read_port(UINT16 port)
{
	port &= 0xff;

	if (port < 0x80 && (port & 0xf) < 3) {
		return DrvPortRAM[port];
	}

	switch (port)
	{
		case 0xc0:
			return MSM6295Read(0);

		case 0xd0:
		case 0xd1:
		case 0xd2:
		case 0xd3:
			return DrvDips[port & 3];

		case 0xe0:
		case 0xe1:
		case 0xe2:
		case 0xe3:
		case 0xe4:
		case 0xe5:
			return DrvInputs[port & 7];

		case 0xf0:
		case 0xf1:
		case 0xf2:
		case 0xf3:
			return 0xff;

		case 0xf7:
			return DrvInputs[6];
	}

	return 0;
}

static tilemap_callback( bg0 )
{
	INT32 code = DrvVidRAM[offs * 2] + (DrvVidRAM[offs * 2 + 1] * 256);

	TILE_SET_INFO(0, code, code / 0x1000, 0);
}

static tilemap_callback( bg1 )
{
	UINT16 *map = (UINT16*)DrvGfxROM1;

	UINT16 code = map[0x100000 + nBgBank1 * 0x40000 + offs];

	code = (code << 8) | (code >> 8); //?

	TILE_SET_INFO(1, code, code / 0x1000, 0);
}

static tilemap_callback( bg2 )
{
	UINT16 *map = (UINT16*)DrvGfxROM2;

	UINT16 code = map[0x100000 + nBgBank2 * 0x40000 + offs];

	code = (code << 8) | (code >> 8);//?

	TILE_SET_INFO(2, code, code / 0x1000, 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	bankswitch(1|(0<<4));
	ZetReset();
	ZetClose();

	MSM6295Reset(0);
	set_oki_bank(1);

	nIrqEnable = 0;
	nBgBank1 = 0;
	nBgBank2 = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM	= Next; Next += 0x040000;

	DrvGfxROM0	= Next; Next += 0x400000;
	DrvGfxROM1	= Next; Next += 0x400000;
	DrvGfxROM2	= Next; Next += 0x400000;

	MSM6295ROM	= Next;
	DrvSndROM	= Next; Next += 0x200000;

	DrvPalette	= (UINT32*)Next; Next += 0x1800 * sizeof(UINT32);

	AllRam		= Next;

	DrvZ80RAM	= Next; Next += 0x004000;
	DrvVidRAM	= Next; Next += 0x004000;
	DrvPalRAM	= Next; Next += 0x003000;
	DrvPortRAM	= Next; Next += 0x000080;

	RamEnd		= Next;

	MemEnd		= Next;

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
		if (BurnLoadRom(DrvZ80ROM  + 0x000000,  0, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  1, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x200000,  2, 1)) return 1;
		BurnByteswap(DrvGfxROM0, 0x400000);

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  3, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x200000,  4, 1)) return 1;
		BurnByteswap(DrvGfxROM1, 0x400000);

		if (BurnLoadRom(DrvGfxROM2 + 0x000000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x200000,  6, 1)) return 1;
		BurnByteswap(DrvGfxROM2, 0x400000);

		if (BurnLoadRom(DrvSndROM  + 0x000000,  7, 1)) return 1;
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,		0xc000, 0xffff, MAP_RAM);
	ZetSetOutHandler(cultures_write_port);
	ZetSetInHandler(cultures_read_port);
	ZetClose();

	MSM6295Init(0, 2000000 / 132, 0);
	MSM6295SetRoute(0, 0.30, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg0_map_callback, 8, 8,  64, 128);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, bg1_map_callback, 8, 8, 512, 512);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, bg2_map_callback, 8, 8, 512, 512);
	GenericTilemapSetGfx(0, DrvGfxROM0, 8, 8, 8, 0x400000, 0x0000, 0xf);
	GenericTilemapSetGfx(1, DrvGfxROM1, 8, 8, 8, 0x400000, 0x1000, 0x7);
	GenericTilemapSetGfx(2, DrvGfxROM2, 8, 8, 8, 0x400000, 0x1000, 0x7);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 502, 256);
	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetTransparent(1, 0);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	MSM6295Exit(0);

	ZetExit();

	BurnFree (AllMem);

	MSM6295ROM = NULL;

	return 0;
}

static void DrvPaletteUpdate()
{
	UINT8 r,g,b;
	UINT16 *p = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x3000/2; i++)
	{
		UINT16 data = BURN_ENDIAN_SWAP_INT16(p[i]);

		r = ((data >> 7) & 0x1e) | ((data >> 14) & 0x01);
		g = ((data >> 3) & 0x1e) | ((data >> 13) & 0x01);
		b = ((data << 1) & 0x1e) | ((data >> 12) & 0x01);

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);

		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	}

	UINT8 *r = DrvPortRAM;

	INT32 bg0_flip = ((r[0x23] & 1) ? TMAP_FLIPX : 0) | ((r[0x33] & 1) ? TMAP_FLIPY : 0);
	INT32 bg1_flip = ((r[0x43] & 1) ? TMAP_FLIPX : 0) | ((r[0x53] & 1) ? TMAP_FLIPY : 0);
	INT32 bg2_flip = ((r[0x63] & 1) ? TMAP_FLIPX : 0) | ((r[0x73] & 1) ? TMAP_FLIPY : 0);

	GenericTilemapSetFlip(0, bg0_flip);
	GenericTilemapSetFlip(1, bg1_flip);
	GenericTilemapSetFlip(2, bg2_flip);

	GenericTilemapSetScrollX(0, (r[0x22] * 256) + r[0x20]);
	GenericTilemapSetScrollY(0, (r[0x32] * 256) + r[0x30]);
	GenericTilemapSetScrollX(1, (r[0x42] * 256) + r[0x40]);
	GenericTilemapSetScrollY(1, (r[0x52] * 256) + r[0x50]);
	GenericTilemapSetScrollX(2, (r[0x62] * 256) + r[0x60]);
	GenericTilemapSetScrollY(2, (r[0x72] * 256) + r[0x70]);

	GenericTilemapDraw(2, pTransDraw, 0);
	GenericTilemapDraw(0, pTransDraw, 0);
	GenericTilemapDraw(1, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, 8); 
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
			DrvInputs[4] ^= (DrvJoy5[i] & 1) << i;
			DrvInputs[5] ^= (DrvJoy6[i] & 1) << i;
			DrvInputs[6] ^= (DrvJoy7[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[1] = { 8000000 / 60 };
	INT32 nCyclesDone[1] = { 0 };

	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);

		if (i == (nInterleave - 1) && nIrqEnable)
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
	}

	ZetClose();

	if (pBurnSoundOut) {
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
	}

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
		ZetScan(nAction);

		MSM6295Scan(nAction, pnMin);

		SCAN_VAR(nBankData);
		SCAN_VAR(nOkiBank);
		SCAN_VAR(nIrqEnable);
		SCAN_VAR(nBgBank1);
		SCAN_VAR(nBgBank2);
	}

	if (nAction & ACB_WRITE)
	{
		ZetOpen(0);
		bankswitch(nBankData);
		set_oki_bank(nOkiBank);
		ZetClose();
	}

	return 0;
}


// Jibun wo Migaku Culture School Mahjong Hen

static struct BurnRomInfo culturesRomDesc[] = {
	{ "ma01.u12",	0x040000, 0xf57417b3, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code

	{ "bg0c.u45",	0x200000, 0xad2e1263, 2 | BRF_GRA },           //  1 Layer 0 Tiles
	{ "bg0c2.u46",	0x100000, 0x97c71c09, 2 | BRF_GRA },           //  2

	{ "bg2c.u68",	0x200000, 0xfa598644, 3 | BRF_GRA },           //  3 Layer 1 Tiles & Map
	{ "bg1t.u67",	0x100000, 0xd2e594ee, 3 | BRF_GRA },           //  4

	{ "bg1c.u80",	0x200000, 0x9ab99bd9, 4 | BRF_GRA },           //  5 Layer 2 Tiles & Map
	{ "bg2t.u79",	0x100000, 0x0610a79f, 4 | BRF_GRA },           //  6

	{ "pcm.u87",	0x200000, 0x84206475, 5 | BRF_SND },           //  7 Samples
};

STD_ROM_PICK(cultures)
STD_ROM_FN(cultures)

struct BurnDriver BurnDrvCultures = {
	"cultures", NULL, NULL, NULL, "1994",
	"Jibun wo Migaku Culture School Mahjong Hen\0", NULL, "Face", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MAHJONG, 0,
	NULL, culturesRomInfo, culturesRomName, NULL, NULL, NULL, NULL, CulturesInputInfo, CulturesDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	384, 240, 4, 3
};
