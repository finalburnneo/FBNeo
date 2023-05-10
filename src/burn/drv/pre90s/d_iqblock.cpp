// FB Alpha IGS IQ Block / Grand Tour driver module
// Based on MAME driver by Nicola Salmoria and Ernesto Corvi

#include "tiles_generic.h"
#include "z80_intf.h"
#include "burn_ym2413.h"
#include "8255ppi.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvBgRAM;
static UINT8 *DrvFgScroll;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 video_enable;

static INT32 protection_address;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo IqblockInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 fire 4"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 fire 4"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Iqblock)

static struct BurnDIPInfo IqblockDIPList[]=
{
	{0x15, 0xff, 0xff, 0xff, NULL					},
	{0x16, 0xff, 0xff, 0xfe, NULL					},

	{0   , 0xfe, 0   ,    4, "Helps"				},
	{0x15, 0x01, 0x0c, 0x0c, "1"					},
	{0x15, 0x01, 0x0c, 0x08, "2"					},
	{0x15, 0x01, 0x0c, 0x04, "3"					},
	{0x15, 0x01, 0x0c, 0x00, "4"					},

	{0   , 0xfe, 0   ,    8, "Coin A"				},
	{0x15, 0x01, 0x70, 0x00, "4 Coins 1 Credits"	},
	{0x15, 0x01, 0x70, 0x10, "3 Coins 1 Credits"	},
	{0x15, 0x01, 0x70, 0x20, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0x70, 0x70, "1 Coin  1 Credits"	},
	{0x15, 0x01, 0x70, 0x60, "1 Coin  2 Credits"	},
	{0x15, 0x01, 0x70, 0x50, "1 Coin  3 Credits"	},
	{0x15, 0x01, 0x70, 0x40, "1 Coin  4 Credits"	},
	{0x15, 0x01, 0x70, 0x30, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds?"			},
	{0x16, 0x01, 0x01, 0x01, "Off"					},
	{0x16, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Free Play"			},
	{0x16, 0x01, 0x02, 0x02, "Off"					},
	{0x16, 0x01, 0x02, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x16, 0x01, 0x80, 0x80, "Off"					},
	{0x16, 0x01, 0x80, 0x00, "On"					},
};

STDDIPINFO(Iqblock)

static struct BurnDIPInfo GrndtourDIPList[]=
{
	{0x15, 0xff, 0xff, 0xdf, NULL					},
	{0x16, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    8, "Coinage"				},
	{0x15, 0x01, 0x1c, 0x00, "4 Coins 1 Credits"	},
	{0x15, 0x01, 0x1c, 0x04, "3 Coins 1 Credits"	},
	{0x15, 0x01, 0x1c, 0x08, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0x1c, 0x1c, "1 Coin  1 Credits"	},
	{0x15, 0x01, 0x1c, 0x18, "1 Coin  2 Credits"	},
	{0x15, 0x01, 0x1c, 0x14, "1 Coin  3 Credits"	},
	{0x15, 0x01, 0x1c, 0x10, "1 Coin  4 Credits"	},
	{0x15, 0x01, 0x1c, 0x0c, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Attract Music"		},
	{0x15, 0x01, 0x20, 0x20, "Off"					},
	{0x15, 0x01, 0x20, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Free Play"			},
	{0x15, 0x01, 0x40, 0x40, "Off"					},
	{0x15, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Gal Images"			},
	{0x15, 0x01, 0x80, 0x00, "No"					},
	{0x15, 0x01, 0x80, 0x80, "Yes"					},

	{0   , 0xfe, 0   ,    2, "Allow P2 to Join / Always Split Screen"	},
	{0x16, 0x01, 0x01, 0x01, "Off"					},
	{0x16, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Test Mode"			},
	{0x16, 0x01, 0xc0, 0xc0, "Off"					},
	{0x16, 0x01, 0xc0, 0x80, "Background Test"		},
	{0x16, 0x01, 0xc0, 0x40, "Input Test"			},
	{0x16, 0x01, 0xc0, 0x00, "Level Test / Debug Mode (use 5 above switches)"	},
};

STDDIPINFO(Grndtour)

static void __fastcall iqblock_write(UINT16 address, UINT8 data)
{
	if (address >= 0xf000) {
		address &= 0xfff;
		DrvZ80RAM[address] = data;
		if (address == protection_address) {
			DrvZ80RAM[address - 0xa] = data;
			DrvZ80RAM[address + 0x1] = data;
		}
		return;
	}
}

static void __fastcall iqblock_write_port(UINT16 address, UINT8 data)
{
	if ((address & 0xfc00) == 0x2000) {
		DrvPalRAM[(address & 0x3ff) + 0x000] = data;
		return;
	}

	if ((address & 0xfc00) == 0x2800) {
		DrvPalRAM[(address & 0x3ff) + 0x400] = data;
		return;
	}

	if ((address & 0xffc0) == 0x6000) {
		DrvFgScroll[address & 0x3f] = data;
		return;
	}

	if ((address & 0xfe00) == 0x6800) {
		DrvFgRAM[address & 0x1ff] = data;
		return;
	}

	if ((address & 0xf000) == 0x7000) {
		DrvBgRAM[address & 0xfff] = data;
		return;
	}

	switch (address)
	{
		case 0x5080:
		case 0x5081:
		case 0x5082:
		case 0x5083:
			ppi8255_w(0, address & 3, data);
		return;

		case 0x50b0:
		case 0x50b1:
			BurnYM2413Write(address & 1, data);
		return;

		case 0x50c0:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;
	}
}

static UINT8 __fastcall iqblock_read_port(UINT16 address)
{
	if ((address & 0xf000) == 0x7000) {
		return DrvBgRAM[address & 0xfff];
	}

	if (address >= 0x8000) {
		return DrvZ80ROM[0x8000 + address];
	}

	switch (address)
	{
		case 0x5080:
		case 0x5081:
		case 0x5082:
		case 0x5083:
			return ppi8255_r(0, address & 3);

		case 0x5090:
			return DrvDips[0];

		case 0x50a0:
			return DrvDips[1];
	}

	return 0;
}

static UINT8 ppi_readport_A()
{
	return DrvInputs[0];
}

static UINT8 ppi_readport_B()
{
	return DrvInputs[1];
}

static UINT8 ppi_readport_C()
{
	return DrvInputs[2];
}

static void ppi_writeport_C(UINT8 data)
{
	video_enable = data & 0x20;
	// coin counter = data & 0x40
}

static tilemap_callback( bg_type0 )
{
	UINT16 code = DrvBgRAM[offs] + (DrvBgRAM[offs + 0x800] * 256);

	TILE_SET_INFO(0, code & 0x3fff, ((code >> 12) & 0xc)+3, 0);
}

static tilemap_callback( bg_type1 )
{
	UINT16 code = DrvBgRAM[offs] + (DrvBgRAM[offs + 0x800] * 256);

	TILE_SET_INFO(0, code & 0x1fff, ((code >> 12) & 0xe)+1, 0);
}

static tilemap_callback( fg )
{
	UINT8 code = DrvFgRAM[offs];

	TILE_SET_INFO(1, code, (code >> 7) * 3, 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	BurnYM2413Reset();
	
	HiscoreReset();

	video_enable = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM			= Next; Next += 0x018000;

	DrvGfxROM0			= Next; Next += 0x100000;
	DrvGfxROM1			= Next; Next += 0x010000;

	DrvPalette			= (UINT32*)Next; Next += 0x0400 * sizeof(UINT32);

	AllRam				= Next;

	DrvZ80RAM			= Next; Next += 0x001000;
	DrvPalRAM			= Next; Next += 0x000800;
	DrvFgRAM			= Next; Next += 0x000200;
	DrvBgRAM			= Next; Next += 0x001000;
	DrvFgScroll			= Next; Next += 0x000040;

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static INT32 DrvGfxDecode() // 0, 100
{
	INT32 Plane0[6] = { 8, 0, 0x40000*8+8, 0x40000*8+0, 0x40000*8*2+8, 0x40000*8*2+0 };
	INT32 Plane1[6] = { 8, 0, 0x4000*8+8, 0x4000*8+0 };
	INT32 XOffs[8]  = { STEP8(0,1) };
	INT32 YOffs[32] = { STEP32(0,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0xc0000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0xc0000);

	GfxDecode(0x4000, 6, 8,  8, Plane0, XOffs, YOffs, 0x080, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x08000);

	GfxDecode(0x0100, 4, 8, 32, Plane1, XOffs, YOffs, 0x200, tmp, DrvGfxROM1);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit(void (*decode)(), UINT16 prot_address, INT32 video_type)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvZ80ROM  + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x10000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x40000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x80000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x04000, k++, 1)) return 1;

		DrvGfxDecode();
		if (decode) decode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0xefff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,			0xf000, 0xffff, MAP_ROM);
	ZetSetWriteHandler(iqblock_write);
	ZetSetOutHandler(iqblock_write_port);
	ZetSetInHandler(iqblock_read_port);
	ZetClose();

	ppi8255_init(2);
	ppi8255_set_read_ports(0, ppi_readport_A, ppi_readport_B, ppi_readport_C);
	ppi8255_set_write_ports(0, NULL, NULL, ppi_writeport_C);

	BurnYM2413Init(3579545);
	BurnYM2413SetAllRoutes(1.00, BURN_SND_ROUTE_BOTH);

	protection_address = prot_address & 0xfff;

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, (video_type) ? bg_type1_map_callback : bg_type0_map_callback, 8,  8, 64, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, fg_map_callback, 8, 32, 64,  8);
	GenericTilemapSetGfx(0, DrvGfxROM0, 6, 8,  8, (video_type) ? 0x80000 : 0x100000, 0, 0xf);
	GenericTilemapSetGfx(1, DrvGfxROM1, 4, 8, 32, 0x008000, 0, 0x3);
	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetScrollCols(1, 64);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	BurnYM2413Exit();
	ppi8255_exit();

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteUpdate()
{
	for (INT32 i = 0; i < 0x400; i++)
	{
		UINT16 p = DrvPalRAM[i] + (DrvPalRAM[0x400 + i] << 8);

		UINT8 r = (p >>  0) & 0x1f;
		UINT8 g = (p >>  5) & 0x1f;
		UINT8 b = (p >> 10) & 0x1f;

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static INT32 DrvDraw()
{
	//if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	//}

	if (video_enable)
	{
		for (INT32 i = 0; i < 64; i++) {
			GenericTilemapSetScrollCol(1, i, DrvFgScroll[i]);
		}

		if ((nBurnLayer & 1) == 0) BurnTransferClear();
		if ((nBurnLayer & 1) == 1) GenericTilemapDraw(1, pTransDraw, 0);
		if ((nBurnLayer & 2) == 2) GenericTilemapDraw(0, pTransDraw, 0);
	}
	else
	{
		BurnTransferClear();
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nSoundBufferPos = 0;
	INT32 nCyclesTotal[1] = { 6000000 / 60 };
	INT32 nCyclesDone[1] = { 0 };

	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);

		if ((i & 0x1f) == 0x10) {
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		}
		if ((i & 0x1f) == 0) {
			ZetNmi();
		}

		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			BurnYM2413Render(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	ZetClose();

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		BurnYM2413Render(pSoundBuf, nSegmentLength);
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
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
		ppi8255_scan();
		BurnYM2413Scan(nAction, pnMin);

		SCAN_VAR(video_enable);
	}

	return 0;
}


// IQ-Block

static struct BurnRomInfo iqblockRomDesc[] = {
	{ "u7.v5",		0x10000, 0x811f306e, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "u8.6",		0x08000, 0x2651bc27, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "u28.1",		0x20000, 0xec4b64b4, 2 | BRF_GRA },           //  2 Background Tiles
	{ "u27.2",		0x20000, 0x74aa3de3, 2 | BRF_GRA },           //  3
	{ "u26.3",		0x20000, 0x2896331b, 2 | BRF_GRA },           //  4

	{ "u25.4",		0x04000, 0x8fc222af, 3 | BRF_GRA },           //  5 Foreground Tiles
	{ "u24.5",		0x04000, 0x61050e1e, 3 | BRF_GRA },           //  6
};

STD_ROM_PICK(iqblock)
STD_ROM_FN(iqblock)

static void iqblock_decode()
{
	for (INT32 i = 0; i < 0xf000; i++) {
		if ((i & 0x0282) != 0x0282) DrvZ80ROM[i] ^= 0x01;
		if ((i & 0x0940) == 0x0940) DrvZ80ROM[i] ^= 0x02;
		if ((i & 0x0090) == 0x0010) DrvZ80ROM[i] ^= 0x20;
	}
}

static INT32 iqblockInit()
{
	return DrvInit(iqblock_decode, 0xfe26, 1);
}

struct BurnDriver BurnDrvIqblock = {
	"iqblock", NULL, NULL, NULL, "1993",
	"IQ-Block\0", NULL, "IGS", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, iqblockRomInfo, iqblockRomName, NULL, NULL, NULL, NULL, IqblockInputInfo, IqblockDIPInfo,
	iqblockInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	512, 240, 4, 3
};


// Grand Tour

static struct BurnRomInfo grndtourRomDesc[] = {
	{ "grand7.u7",	0x10000, 0x95cac31e, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "grand6.u8",	0x08000, 0x4c634b86, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "grand1.u28",	0x40000, 0xde85c664, 2 | BRF_GRA },           //  2 Background Tiles
	{ "grand2.u27",	0x40000, 0x8456204e, 2 | BRF_GRA },           //  3
	{ "grand3.u26",	0x40000, 0x77632917, 2 | BRF_GRA },           //  4

	{ "grand4.u25",	0x04000, 0x48d09746, 3 | BRF_GRA },           //  5 Foreground Tiles
	{ "grand5.u24",	0x04000, 0xf896efb2, 3 | BRF_GRA },           //  6
};

STD_ROM_PICK(grndtour)
STD_ROM_FN(grndtour)

static void grndtour_decode()
{
	for (INT32 i = 0; i < 0xf000; i++) {
		if ((i & 0x0282) != 0x0282) DrvZ80ROM[i] ^= 0x01;
		if ((i & 0x0940) == 0x0940) DrvZ80ROM[i] ^= 0x02;
		if ((i & 0x0060) == 0x0040) DrvZ80ROM[i] ^= 0x20;
	}
}

static INT32 grndtourInit()
{
	return DrvInit(grndtour_decode, 0xfe39, 0);
}

struct BurnDriver BurnDrvGrndtour = {
	"grndtour", NULL, NULL, NULL, "1993",
	"Grand Tour\0", NULL, "IGS", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, grndtourRomInfo, grndtourRomName, NULL, NULL, NULL, NULL, IqblockInputInfo, GrndtourDIPInfo,
	grndtourInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	512, 240, 4, 3
};
