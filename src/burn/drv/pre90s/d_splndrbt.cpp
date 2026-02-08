// FB Neo Splendor Blast driver module
// Based on MAME driver by Acho A. Tang, Nicola Salmoria

// TOdink:
//   1. scaling sprites sometimes have line inbetween when they get
//      zoomed into, needs fix - like simpsons fix.., perhaps?
//   2. hvoltage (after coin intro w/scrolling text) make sound more like pcb.

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "alpha8201.h"
#include "samples.h"
#include "ad59mc07.h"
#include "burn_pal.h"
#include "74259.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvSndROM;
static UINT8 *DrvMCUROM;
static UINT8 *DrvGfxROM[3];
static UINT8 *DrvColPROM;
static UINT8 *DrvBgScale;
static UINT8 *DrvSprScale;
static UINT8 *Drv68KRAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvBgRAM;
static UINT8 *DrvSprRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT16 scrolly;
static UINT16 scrollx;
static UINT8 bgcolor;
static INT32 flipscreen;
static INT32 fg_char_bank;

static INT32 nCyclesExtra[3];

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvDips[3];
static UINT16 DrvInputs[2];
static UINT8 DrvReset;

static struct BurnInputInfo SplndrbtInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 8,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 9,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 15,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 11,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 10,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Splndrbt)

static struct BurnInputInfo HvoltageInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 8,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 9,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 15,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 11,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 10,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Hvoltage)

#include "dip_percent_equites.inc"

static struct BurnDIPInfo SplndrbtDIPList[]=
{
	DIP_OFFSET(0x11)
	{0x00, 0xff, 0xff, 0x20, NULL							},
	{0x01, 0xff, 0xff, 0x00, NULL							},
	{0x02, 0xff, 0xff,   25, NULL							},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x00, 0x01, 0x0c, 0x04, "Easy"							},
	{0x00, 0x01, 0x0c, 0x00, "Normal"						},
	{0x00, 0x01, 0x0c, 0x08, "Hard"							},
	{0x00, 0x01, 0x0c, 0x0c, "Hardest"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x00, 0x01, 0x10, 0x10, "Off"							},
	{0x00, 0x01, 0x10, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Cabinet"						},
	{0x00, 0x01, 0x20, 0x20, "Upright"						},
	{0x00, 0x01, 0x20, 0x00, "Cocktail"						},

	{0   , 0xfe, 0   ,    4, "Coinage"						},
	{0x00, 0x01, 0xc0, 0xc0, "A 2C/1C B 3C/1C"				},
	{0x00, 0x01, 0xc0, 0x00, "A 1C/1C B 2C/1C"				},
	{0x00, 0x01, 0xc0, 0x40, "A 1C/2C B 1C/4C"				},
	{0x00, 0x01, 0xc0, 0x80, "A 1C/3C B 1C/6C"				},

	DIP_PERCENT(0x02)
};

STDDIPINFO(Splndrbt)

static struct BurnDIPInfo HvoltageDIPList[]=
{
	DIP_OFFSET(0x11)
	{0x00, 0xff, 0xff, 0x00, NULL							},
	{0x01, 0xff, 0xff, 0x40, NULL							},
	{0x02, 0xff, 0xff,   24, NULL							},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x00, 0x01, 0x0c, 0x04, "Easy"							},
	{0x00, 0x01, 0x0c, 0x00, "Normal"						},
	{0x00, 0x01, 0x0c, 0x08, "Hard"							},
	{0x00, 0x01, 0x0c, 0x0c, "Hardest"						},

	{0   , 0xfe, 0   ,    2, "Lives"						},
	{0x00, 0x01, 0x10, 0x00, "3"							},
	{0x00, 0x01, 0x10, 0x10, "5"							},

	{0   , 0xfe, 0   ,    2, "Bonus Life"					},
	{0x00, 0x01, 0x20, 0x00, "50k, 100k then every 100k"	},
	{0x00, 0x01, 0x20, 0x20, "50k, 200k then every 100k"	},

	{0   , 0xfe, 0   ,    4, "Coinage"						},
	{0x00, 0x01, 0xc0, 0xc0, "A 2C/1C B 3C/1C"				},
	{0x00, 0x01, 0xc0, 0x00, "A 1C/1C B 2C/1C"				},
	{0x00, 0x01, 0xc0, 0x40, "A 1C/2C B 1C/4C"				},
	{0x00, 0x01, 0xc0, 0x80, "A 1C/3C B 1C/6C"				},

	{0   , 0xfe, 0   ,    2, "Cabinet"						},
	{0x01, 0x01, 0x40, 0x40, "Upright"						},
	{0x01, 0x01, 0x40, 0x00, "Cocktail"						},

	DIP_PERCENT(0x02)
};

STDDIPINFO(Hvoltage)

static void mcu_sync()
{
	INT32 cyc = (INT64)SekTotalCycles(0) * (4000000/8/4) / 6000000;
	cyc -= Alpha8201TotalCycles();

	if (cyc > 0) {
		Alpha8201Run(cyc);
	}
}

static void latch_map(INT32 address, UINT8 data)
{
	switch (address) {
		case 0: flipscreen = data; break;
		case 1: Alpha8201Start(data); break;
		case 2: Alpha8201SetBusDir(data ^ 1); break;
		case 3: fg_char_bank = data; break;
	}
}

static void __fastcall splndrbt_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xffe000) == 0x200000) {
		DrvFgRAM[(address / 2) & 0x7ff] = data & 0xff;
		return;
	}

	if ((address & 0xfff800) == 0x180000) {
		mcu_sync();
		Alpha8201WriteRam((address & 0x7ff) >> 1, data & 0xff);
		return;
	}

	if (address >= 0xc0000 && address <= 0xfcfff) {
		mcu_sync();
		ls259_write_a3((address & 0x3c000) >> 14, data);
		bprintf(0, _T("latch word %x\n"), data);
		return;
	}

	switch (address)
	{
		case 0x140000:
			AD59MC07Command(data);
		return;

		case 0x100000:
			scrollx = data;
		return;

		case 0x1c0000:
			scrolly = data;
		return;
	}

	bprintf (0, _T("MWW: %5.5x, %4.4x PC(%5.5x)\n"), address, data, SekGetPC(-1));
}

static void __fastcall splndrbt_main_write_byte(UINT32 address, UINT8 data)
{

	if ((address & 0xffe000) == 0x200000) {
		DrvFgRAM[(address / 2) & 0x7ff] = data;
		return;
	}

	if ((address & 0xfff801) == 0x180001) {
		mcu_sync();
		Alpha8201WriteRam((address & 0x7ff) >> 1, data & 0xff);
		return;
	}

	if (address >= 0xc0000 && address <= 0xfcfff) {
		if (address & 1) {
			mcu_sync();
			ls259_write_a3((address & 0x3c000) >> 14, data);
			return;
		}
	}

	switch (address)
	{
		case 0x140001:
			AD59MC07Command(data);
		return;

		case 0x0c0000:
		case 0x0e0000:
			bgcolor = data;
		return;
	}

	bprintf (0, _T("MWB: %5.5x, %2.2x PC(%5.5x)\n"), address, data, SekGetPC(-1));
}

static UINT16 __fastcall splndrbt_main_read_word(UINT32 address)
{
	SEK_DEF_READ_WORD(0, address);

	bprintf (0, _T("MRW: %5.5x PC(%5.5x)\n"), address, SekGetPC(-1));
	return 0xffff;
}

static UINT8 __fastcall splndrbt_main_read_byte(UINT32 address)
{
	if ((address & 0xfff801) == 0x180001) {
		mcu_sync();
		return Alpha8201ReadRam((address & 0x7ff) >> 1);
	}

	if ((address & 0xffe001) == 0x200001) {
		return DrvFgRAM[(address / 2) & 0x7ff];
	}

	switch (address)
	{
		case 0x080000:
			return (DrvInputs[0] >> 8);

		case 0x080001:
			return DrvInputs[0] | (DrvDips[1] & 0x40);

		case 0x0c0000:
			return (DrvInputs[1] >> 8) | DrvDips[0];

		case 0x0c0001:
			return DrvInputs[1] >> 0;

		case 0x1c0001:
			return 0xff; // watchdog, irq ack?
	}

	//bprintf (0, _T("MRB: %5.5x PC(%5.5x)\n"), address, SekGetPC(-1));
	return 0xff;
}

static tilemap_callback( fg )
{
	UINT8 attr = DrvFgRAM[offs * 2 + 1];

	TILE_SET_INFO(0, DrvFgRAM[offs * 2] | (fg_char_bank << 8), attr, (attr & 0x10) ? TILE_OPAQUE : 0); // tileinfo.flags |= TILE_FORCE_LAYER0;
}

static tilemap_callback( bg )
{
	UINT16 attr = *((UINT16*)(DrvBgRAM + (offs * 2)));
	const UINT16 color = attr >> 11;

	TILE_SET_INFO(1, attr, color, TILE_FLIPXY(attr >> 9));

	sTile->category = color; // fbn category == mame group(!)
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	AD59MC07Reset();

	Alpha8201Reset();
	ls259_reset();

	memset (nCyclesExtra, 0, sizeof(nCyclesExtra));

	scrolly = 0;
	scrollx = 0;
	bgcolor = 0;
	flipscreen = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM			= Next; Next += 0x010000;
	DrvSndROM			= Next; Next += 0x010000;
	DrvMCUROM			= Next; Next += 0x002000;

	DrvGfxROM[0]		= Next; Next += 0x008000;
	DrvGfxROM[1]		= Next; Next += 0x020000;
	DrvGfxROM[2]		= Next; Next += 0x020000;

	DrvColPROM			= Next; Next += 0x000800;

	DrvBgScale			= Next; Next += 0x002100;
	DrvSprScale			= Next; Next += 0x000200;

	DrvPalette			= (UINT32*)Next; Next += 0x0300 * sizeof(UINT32);

	AllRam				= Next;

	Drv68KRAM			= Next; Next += 0x001000;
	DrvFgRAM			= Next; Next += 0x000800;
	DrvBgRAM			= Next; Next += 0x001000;
	DrvSprRAM			= Next; Next += 0x000400;

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static INT32 DrvLoadROMs()
{
	char* pRomName;
	struct BurnRomInfo ri;
	UINT8 *pLoad[10] = { 0, Drv68KROM, DrvSndROM, DrvMCUROM, DrvGfxROM[0], DrvGfxROM[1], DrvGfxROM[2], DrvColPROM, DrvBgScale, DrvSprScale };
	UINT8 *tmp = (UINT8*)BurnMalloc(0x4000*2); //*2 dink debug

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++)
	{
		BurnDrvGetRomInfo(&ri, i);

		INT32 nType = ri.nType & 0xf;

		if (nType == 1)
		{
			if (BurnLoadRom(pLoad[nType] + 0, i+0, 2)) return 1;
			if (BurnLoadRom(pLoad[nType] + 1, i+1, 2)) return 1;
			pLoad[nType] += ri.nLen * 2;
			i++;
			continue;
		}

		if (nType >= 2)
		{
			if (BurnLoadRom(tmp, i, 1)) return 1;

			if ((nType == 6) && (pLoad[nType] - DrvGfxROM[nType - 4]) == 0)
			{
				memcpy (pLoad[nType] + 0x0000, tmp + 0x0000, 0x2000);
				memcpy (pLoad[nType] + 0x4000, tmp + 0x2000, 0x2000);
				pLoad[nType] += 0x4000; // gap
			}
			else
			{
				memcpy (pLoad[nType], tmp, ri.nLen);
			}
			if (nType == 3) bprintf(0, _T("MCU ROM: %x\n"), ri.nLen);
			pLoad[nType] += ri.nLen;
			continue;
		}
	}

	BurnFree(tmp);

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[2]  = { 0, 4 };
	INT32 XOffs0[8]  = { STEP4(8*8+3,-1), STEP4(0*8+3,-1) };
	INT32 YOffs0[8]  = { STEP8(0*8,8) };
	INT32 Plane1[2]  = { 0, 4 };
	INT32 XOffs1[16] = { STEP4(16*8+3,-1), STEP4(32*8+3,-1), STEP4(48*8+3,-1), STEP4(0*8+3,-1) };
	INT32 YOffs1[16] = { STEP16(0,8) };
	INT32 Plane2[3]  = { 4, ((0x10000/2)*8), ((0x10000/2)*8)+4 };
	INT32 XOffs2[32] = { STEP4(0*8+3,-1), STEP4(1*8+3,-1), STEP4(2*8+3,-1), STEP4(3*8+3,-1), STEP4(4*8+3,-1), STEP4(5*8+3,-1), STEP4(6*8+3,-1), STEP4(7*8+3,-1) };
	INT32 YOffs2[32] = { STEP16(0*8*8,8*8), STEP16(31*8*8,-8*8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x10000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM[0], 0x2000);

	GfxDecode(0x0200, 2,  8,  8, Plane0, XOffs0, YOffs0, 0x080, tmp, DrvGfxROM[0]);

	memcpy (tmp, DrvGfxROM[1], 0x8000);

	GfxDecode(0x0200, 2, 16, 16, Plane1, XOffs1, YOffs1, 0x200, tmp, DrvGfxROM[1]);

	memcpy (tmp, DrvGfxROM[2], 0x10000);

	GfxDecode(0x0080, 3, 32, 32, Plane2, XOffs2, YOffs2, 0x800, tmp, DrvGfxROM[2]);

	BurnFree(tmp);

	return 0;
}

static void unpack_block(UINT8 *rom, INT32 offset, INT32 size)
{
	for (INT32 i = 0; i < size; i++)
	{
		rom[(offset + i + size)] = (rom[(offset + i)] >> 4);
		rom[(offset + i)] &= 0x0f;
	}
}

static void unpack_region(INT32 select)
{
	unpack_block(DrvGfxROM[select], 0x0000, 0x2000);
	unpack_block(DrvGfxROM[select], 0x4000, 0x2000);
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		if (DrvLoadROMs()) return 1;
		unpack_region(2);
		DrvGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x00ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,		0x040000, 0x040fff, MAP_RAM);
	//SekMapMemory(DrvFgRAM,	0x200000, 0x200fff, MAP_RAM); // 8 bit
	SekMapMemory(DrvBgRAM,		0x400000, 0x400fff, MAP_RAM); // 0-7ff bg, 800-fff ?
	SekMapMemory(DrvSprRAM,		0x600000, 0x6003ff, MAP_RAM); // 0-1ff, 200 due to sek page size, spr1 0-ff, spr2 100-1ff
	SekSetWriteWordHandler(0,	splndrbt_main_write_word);
	SekSetWriteByteHandler(0,	splndrbt_main_write_byte);
	SekSetReadWordHandler(0,	splndrbt_main_read_word);
	SekSetReadByteHandler(0,	splndrbt_main_read_byte);
	SekClose();

	Alpha8201Init(DrvMCUROM);

	ls259_init();
	ls259_set_write_cb(latch_map);

	AD59MC07Init(DrvSndROM);

	GenericTilesInit();
	GenericTilemapSetGfx(0, DrvGfxROM[0], 2,  8,  8, 0x008000, 0x000, 0x3f);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 2, 16, 16, 0x020000, 0x100, 0x1f);
	GenericTilemapSetGfx(2, DrvGfxROM[2], 3, 32, 32, 0x020000, 0x200, 0x1f);
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_COLS, fg_map_callback,  8,  8, 32, 32);
//	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetOffsets(1, 8, -32);

#if 1
	// from mappy, I think it does the same thing? -dink
	GenericTilemapCategoryConfig(0, 0x20);
	for (INT32 i = 0; i < 0x20 * 4; i++) {
		GenericTilemapSetCategoryEntry(0, i / 4, i % 4, ((DrvColPROM[i + 0x300] & 0xf) == 0x0) ? 1 : 0);
	}
#endif
	BurnBitmapAllocate(1, 512, 512, true);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	SekExit();
	Alpha8201Exit();
	ls259_exit();
	AD59MC07Exit();

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 256; i++)
	{
		DrvPalette[i] = BurnHighCol(pal4bit(DrvColPROM[i]), pal4bit(DrvColPROM[i + 0x100]), pal4bit(DrvColPROM[i + 0x200]), 0);
	}

	for (INT32 i = 0; i < 0x80; i++)
	{
		DrvPalette[0x100 + i] = DrvPalette[DrvColPROM[0x300 + i] + 0x10];
	}

	for (INT32 i = 0; i < 0x100; i++)
	{
		DrvPalette[0x200 + i] = DrvPalette[DrvColPROM[0x400 + i]];
	}
}

static void draw_sprites()
{
	UINT8 * xrom = DrvSprScale;
	UINT8 * yrom = xrom + 0x100;
	UINT16 *ram[2] = { (UINT16*)DrvSprRAM, (UINT16*)(DrvSprRAM + 0x100) };

	GenericTilesGfx *pGfx = &GenericGfxData[2];

	for (INT32 offs = 0x3f; offs < 0x6f; offs += 2)
	{
		INT32 data = ram[0][offs];
		INT32 tile = data & 0x007f;
		INT32 fx = (data & 0x2000) >> 13;
		INT32 fy = (data & 0x1000) >> 12;
		INT32 scaley = (data & 0x0f00) >> 8;

		INT32 data2 = ram[0][offs + 1];
		INT32 sx = data2 & 0x00ff;
		INT32 color = (data2 & 0x1f00) >> 8;

		INT32 sy = ram[1][offs + 0] & 0x00ff;
		INT32 scalex = ram[1][offs + 1] & 0x000f;

		const UINT8 * const yromline = yrom + (scaley << 4) + (15 - scaley);
		const UINT8* const srcgfx = pGfx->gfxbase + ((tile % pGfx->code_mask) * pGfx->width * pGfx->height);
		INT32 pal = pGfx->color_offset + (color << pGfx->depth);

		sy += 46; // dink iderrknow? seems to fix alignment

		if (flipscreen)
		{
			// sx NOT inverted
			fx = fx ^ 1;
			fy = fy ^ 1;
		}
		else
		{
			sy = 256 - sy;
		}

		for (INT32 yy = 0; yy <= scaley; ++yy)
		{
			int const line = yromline[yy];
			int yhalf;

			for (yhalf = 0; yhalf < 2; ++yhalf) // top or bottom half
			{
				INT32 y = yhalf ? sy + 1 + yy : sy - yy;

				if (y >= 0 && y < nScreenHeight)
				{
					for (INT32 x = 0; x <= (scalex << 1); ++x)
					{
						INT32 bx = (sx + x) & 0xff;

						if (bx >= 0 && bx < nScreenWidth)
						{
							int xx = scalex ? (x * 29 + scalex) / (scalex << 1) + 1 : 16; // FIXME This is wrong. Should use the PROM.
							int const offset = (fx ? (31 - xx) : xx) + ((fy ^ yhalf) ? (16 + line) : (15 - line)) * pGfx->width;

							int pen = srcgfx[offset] + pal;

							if (DrvColPROM[0x200 + pen])
								pTransDraw[y * nScreenWidth + bx] = pen;
						}
					}
				}
			}
		}
	}
}

static void copy_bg()
{
	UINT16 *src_bitmap = BurnBitmapGetPosition(1, 0, 0);
	UINT8 *flags_bitmap = BurnBitmapGetPrimapPosition(1, 0, 0);
	UINT8 *xrom = DrvBgScale;
	UINT8 *yrom = xrom + 0x2000;
	int scroll_x = scrollx;
	int scroll_y = scrolly;
	int const dinvert = flipscreen ? 0xff : 0x00;
	int src_y = 0;
	int dst_y;

	if (flipscreen)
	{
		scroll_x = -scroll_x - 8;
		scroll_y = -scroll_y;
	}

	for (dst_y = 32; dst_y < 256-32; ++dst_y)
	{
		if (dst_y >= 32 && dst_y < nScreenHeight+32)
		{
			UINT8 *romline = &xrom[(dst_y ^ dinvert) << 5];
			UINT16 *src_line = &src_bitmap[((src_y + scroll_y) & 0x1ff) * 512];
			UINT8 *flags_line = &flags_bitmap[((src_y + scroll_y) & 0x1ff) * 512];
			UINT16 * const dst_line = BurnBitmapGetPosition(0, 0, dst_y - 32);
			int dst_x = 0;
			int src_x;

			for (src_x = 0; src_x < 256 && dst_x < 128; ++src_x)
			{
				if ((romline[31 - (src_x >> 3)] >> (src_x & 7)) & 1)
				{
					int sx;

					sx = (256+128 + scroll_x + src_x) & 0x1ff;
					if (flags_line[sx] == 2) // we use pri==2 to denote opaque pixel -dink
						dst_line[128 + dst_x] = src_line[sx];

					sx = (255+128 + scroll_x - src_x) & 0x1ff;
					if (flags_line[sx] == 2) // ""
						dst_line[127 - dst_x] = src_line[sx];

					++dst_x;
				}
			}
		}

		src_y += 1 + yrom[dst_y ^ dinvert];
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	BurnTransferClear(bgcolor);

	//GenericTilemapSetScrollX(0, scrollx);
	//GenericTilemapSetScrollY(0, scrolly);

	INT32 twidth = nScreenWidth;
	INT32 theight = nScreenHeight;
	nScreenWidth = 512;
	nScreenHeight = 512;
	BurnBitmapPrimapClear(1);
	if (nBurnLayer & 1) GenericTilemapDraw(0, 1, 2);
	nScreenWidth = twidth;
	nScreenHeight = theight;

	if (nBurnLayer & 1) copy_bg();

	if ((nBurnLayer & 2) && fg_char_bank != 0) GenericTilemapDraw(1, 0, 0);

	if (nSpriteEnable & 1) draw_sprites();

	if ((nBurnLayer & 2) && fg_char_bank == 0) GenericTilemapDraw(1, 0, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	SekNewFrame();
	Alpha8201NewFrame();
	AD59MC07NewFrame();

	{
		memset (DrvInputs, 0, sizeof(DrvInputs));

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}

		AD59MC07SetRate(DrvDips[2]);
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[3] = { 6000000 / 60, 6144000 / 2 / 60, (4000000 / 8 / 4) / 60 };
	INT32 nCyclesDone[3] = { nCyclesExtra[0], nCyclesExtra[1], nCyclesExtra[2] };

	Alpha8201Idle(nCyclesExtra[2]);
	SekOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);
		CPU_RUN(1, AD59MC07);
		CPU_RUN_SYNCINT(2, Alpha8201);

		if (i == 63) SekSetIRQLine(1, CPU_IRQSTATUS_HOLD);
		if (i == 255) SekSetIRQLine(2, CPU_IRQSTATUS_HOLD);
	}

	nCyclesExtra[0] = nCyclesDone[0] - nCyclesTotal[0];
	nCyclesExtra[1] = nCyclesDone[1] - nCyclesTotal[1];
	nCyclesExtra[2] = Alpha8201TotalCycles() - nCyclesTotal[2];

	if (pBurnSoundOut) {
		AD59MC07Update(pBurnSoundOut, nBurnSoundLen);
	}

	SekClose();

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	if (pnMin) {
		*pnMin = 0x029702;
	}

	if (nAction & ACB_VOLATILE)
	{
		ScanVar(AllRam, RamEnd-AllRam, "All Ram");

		SekScan(nAction);
		Alpha8201Scan(nAction, pnMin);
		ls259_scan();
		AD59MC07Scan(nAction, pnMin);

		SCAN_VAR(scrolly);
		SCAN_VAR(scrollx);
		SCAN_VAR(fg_char_bank);
		SCAN_VAR(bgcolor);
		SCAN_VAR(flipscreen);
		SCAN_VAR(nCyclesExtra);
	}

	return 0;
}


// Splendor Blast (World, set 1)

static struct BurnRomInfo splndrbtRomDesc[] = {
	{ "1.16a",						0x4000, 0x4bf4b047, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "2.16c",						0x4000, 0x27acb656, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.15a",						0x4000, 0x5b182189, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.15c",						0x4000, 0xcde99613, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "1_v.1m",						0x2000, 0x1b3a6e42, 2 | BRF_PRG | BRF_ESS }, //  4 i8085A Code
	{ "2_v.1l",						0x2000, 0x2a618c72, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "3_v.1k",						0x2000, 0xbbee5346, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "4_v.1h",						0x2000, 0x10f45af4, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "alpha-8303_44801b42.bin",	0x2000, 0x66adcb37, 3 | BRF_PRG | BRF_ESS }, //  8 Alpha8201 Code

	{ "10.8c",						0x2000, 0x501887d4, 4 | BRF_GRA },           //  9 Characters

	{ "8.14m",						0x4000, 0xc2c86621, 5 | BRF_GRA },           // 10 Background Tiles
	{ "9.12m",						0x4000, 0x4f7da6ff, 5 | BRF_GRA },           // 11

	{ "6.18n",						0x4000, 0xaa72237f, 6 | BRF_GRA },           // 12 Sprites
	{ "5.18m",						0x4000, 0x5f618b39, 6 | BRF_GRA },           // 13
	{ "7.17m",						0x4000, 0xabdd8483, 6 | BRF_GRA },           // 14

	{ "r.3a",						0x0100, 0xca1f08ce, 7 | BRF_GRA },           // 15 Color PROMs
	{ "g.1a",						0x0100, 0x66f89177, 7 | BRF_GRA },           // 16
	{ "b.2a",						0x0100, 0xd14318bc, 7 | BRF_GRA },           // 17
	{ "2.8k",						0x0100, 0xe1770ad3, 7 | BRF_GRA },           // 18
	{ "s5.15p",						0x0100, 0x7f6cf709, 7 | BRF_GRA },           // 19

	{ "3h.bpr",						0x0020, 0x33b98466, 0 | BRF_OPT },           // 20 Unknown PROM

	{ "0.8h",						0x2000, 0x12681fb5, 8 | BRF_GRA },           // 21 Background Layer Scaling
	{ "1.9j",						0x0100, 0xf5b9b777, 8 | BRF_GRA },           // 22

	{ "4.7m",						0x0100, 0x12cbcd2c, 9 | BRF_GRA },           // 23 Sprite Scaling
	{ "s3.8l",						0x0100, 0x1314b0b5, 9 | BRF_GRA },           // 24
};

STD_ROM_PICK(splndrbt)
STD_ROM_FN(splndrbt)

struct BurnDriver BurnDrvSplndrbt = {
	"splndrbt", NULL, NULL, "ad59mc07", "1985",
	"Splendor Blast (World, set 1)\0", NULL, "Alpha Denshi Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_RACING | GBF_SHOOT, 0,
	NULL, splndrbtRomInfo, splndrbtRomName, NULL, NULL, mc07SampleInfo, mc07SampleName, SplndrbtInputInfo, SplndrbtDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	256, 192, 4, 3
};


// Splendor Blast (World, set 2)

static struct BurnRomInfo splndrbtaRomDesc[] = {
	{ "1red.16b",					0x4000, 0x3e342030, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "2red.16c",					0x4000, 0x757e270b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3red.15b",					0x4000, 0x788deb02, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4red.15c",					0x4000, 0xd02a5606, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "8v.1l",						0x4000, 0x71b2ec29, 2 | BRF_PRG | BRF_ESS }, //  4 i8085A Code
	{ "9v.1h",						0x4000, 0xe95abcb5, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "alpha-8303_44801b42.bin",	0x2000, 0x66adcb37, 3 | BRF_PRG | BRF_ESS }, //  6 Alpha8201 Code

	{ "10.8c",						0x2000, 0x501887d4, 4 | BRF_GRA },           //  7 Characters

	{ "8.14m",						0x4000, 0xc2c86621, 5 | BRF_GRA },           //  8 Background Tiles
	{ "9.12m",						0x4000, 0x4f7da6ff, 5 | BRF_GRA },           //  9

	{ "6.18n",						0x4000, 0xaa72237f, 6 | BRF_GRA },           // 10 Sprites
	{ "5.18m",						0x4000, 0x5f618b39, 6 | BRF_GRA },           // 11
	{ "7.17m",						0x4000, 0xabdd8483, 6 | BRF_GRA },           // 12

	{ "r.3a",						0x0100, 0xca1f08ce, 7 | BRF_GRA },           // 13 Color PROMs
	{ "g.1a",						0x0100, 0x66f89177, 7 | BRF_GRA },           // 14
	{ "b.2a",						0x0100, 0xd14318bc, 7 | BRF_GRA },           // 15
	{ "2.8k",						0x0100, 0xe1770ad3, 7 | BRF_GRA },           // 16
	{ "s5.15p",						0x0100, 0x7f6cf709, 7 | BRF_GRA },           // 17

	{ "3h.bpr",						0x0020, 0x33b98466, 0 | BRF_OPT },           // 18 Unknown PROM

	{ "0.8h",						0x2000, 0x12681fb5, 8 | BRF_GRA },           // 19 Background Layer Scaling
	{ "1.9j",						0x0100, 0xf5b9b777, 8 | BRF_GRA },           // 20

	{ "4.7m",						0x0100, 0x12cbcd2c, 9 | BRF_GRA },           // 21 Sprite Scaling
	{ "s3.8l",						0x0100, 0x1314b0b5, 9 | BRF_GRA },           // 22
};

STD_ROM_PICK(splndrbta)
STD_ROM_FN(splndrbta)

struct BurnDriver BurnDrvSplndrbta = {
	"splndrbta", "splndrbt", NULL, "ad59mc07", "1985",
	"Splendor Blast (World, set 2)\0", NULL, "Alpha Denshi Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_RACING | GBF_SHOOT, 0,
	NULL, splndrbtaRomInfo, splndrbtaRomName, NULL, NULL, mc07SampleInfo, mc07SampleName, SplndrbtInputInfo, SplndrbtDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x280,
	256, 192, 4, 3
};


// Splendor Blast (World, set 3)

static struct BurnRomInfo splndrbtbRomDesc[] = {
	{ "1blue.16a",					0x4000, 0xf8507502, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "2blue.16c",					0x4000, 0x8969bd04, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3blue.15a",					0x4000, 0xbce26d4f, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4blue.15c",					0x4000, 0x5715ec1b, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "1_v.1m",						0x2000, 0x1b3a6e42, 2 | BRF_PRG | BRF_ESS }, //  4 i8085A Code
	{ "2_v.1l",						0x2000, 0x2a618c72, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "3_v.1k",						0x2000, 0xbbee5346, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "4_v.1h",						0x2000, 0x10f45af4, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "alpha-8303_44801b42.bin",	0x2000, 0x66adcb37, 3 | BRF_PRG | BRF_ESS }, //  8 Alpha8201 Code

	{ "10.8c",						0x2000, 0x501887d4, 4 | BRF_GRA },           //  9 Characters

	{ "8.14m",						0x4000, 0xc2c86621, 5 | BRF_GRA },           // 10 Background Tiles
	{ "9.12m",						0x4000, 0x4f7da6ff, 5 | BRF_GRA },           // 11

	{ "6.18n",						0x4000, 0xaa72237f, 6 | BRF_GRA },           // 12 Sprites
	{ "5.18m",						0x4000, 0x5f618b39, 6 | BRF_GRA },           // 13
	{ "7.17m",						0x4000, 0xabdd8483, 6 | BRF_GRA },           // 14

	{ "r.3a",						0x0100, 0xca1f08ce, 7 | BRF_GRA },           // 15 Color PROMs
	{ "g.1a",						0x0100, 0x66f89177, 7 | BRF_GRA },           // 16
	{ "b.2a",						0x0100, 0xd14318bc, 7 | BRF_GRA },           // 17
	{ "2.8k",						0x0100, 0xe1770ad3, 7 | BRF_GRA },           // 18
	{ "s5.15p",						0x0100, 0x7f6cf709, 7 | BRF_GRA },           // 19

	{ "3h.bpr",						0x0020, 0x33b98466, 0 | BRF_OPT },           // 20 Unknown PROM

	{ "0.8h",						0x2000, 0x12681fb5, 8 | BRF_GRA },           // 21 Background Layer Scaling
	{ "1.9j",						0x0100, 0xf5b9b777, 8 | BRF_GRA },           // 22

	{ "4.7m",						0x0100, 0x12cbcd2c, 9 | BRF_GRA },           // 23 Sprite Scaling
	{ "s3.8l",						0x0100, 0x1314b0b5, 9 | BRF_GRA },           // 24
};

STD_ROM_PICK(splndrbtb)
STD_ROM_FN(splndrbtb)

struct BurnDriver BurnDrvSplndrbtb = {
	"splndrbtb", "splndrbt", NULL, "ad59mc07", "1985",
	"Splendor Blast (World, set 3)\0", NULL, "Alpha Denshi Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_RACING | GBF_SHOOT, 0,
	NULL, splndrbtbRomInfo, splndrbtbRomName, NULL, NULL, mc07SampleInfo, mc07SampleName, SplndrbtInputInfo, SplndrbtDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x280,
	256, 192, 4, 3
};


// Splendor Blast II (World)

static struct BurnRomInfo splndrbt2RomDesc[] = {
	{ "1.a16",						0x4000, 0x0fd3121d, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "2.c16",						0x4000, 0x227d8a1b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.a15",						0x4000, 0x936f7cc9, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.c15",						0x4000, 0x3ff7c7b5, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "s1.m1",						0x2000, 0x045eac1b, 2 | BRF_PRG | BRF_ESS }, //  4 i8085A Code
	{ "s2.l1",						0x2000, 0x65a3d094, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "s3.k1",						0x2000, 0x980d38be, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "s4.h1",						0x2000, 0x10f45af4, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "s5.f1",						0x2000, 0x0d76cac0, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "s6.e1",						0x2000, 0xbc65d469, 2 | BRF_PRG | BRF_ESS }, //  9

	{ "alpha-8303_44801b42.bin",	0x2000, 0x66adcb37, 3 | BRF_PRG | BRF_ESS }, // 10 Alpha8201 Code

	{ "5.b8",						0x2000, 0x77a5dc55, 4 | BRF_GRA },           // 11 Characters

	{ "8.m13",						0x4000, 0xc2c86621, 5 | BRF_GRA },           // 12 Background Tiles
	{ "9.m12",						0x4000, 0x4f7da6ff, 5 | BRF_GRA },           // 13

	{ "8.n18",						0x4000, 0x15b8277b, 6 | BRF_GRA },           // 14 Sprites
	{ "5.m18",						0x4000, 0x5f618b39, 6 | BRF_GRA },           // 15
	{ "7.m17",						0x4000, 0xabdd8483, 6 | BRF_GRA },           // 16

	{ "r.3a",						0x0100, 0xca1f08ce, 7 | BRF_GRA },           // 17 Color PROMs
	{ "g.1a",						0x0100, 0x66f89177, 7 | BRF_GRA },           // 18
	{ "b.2a",						0x0100, 0xd14318bc, 7 | BRF_GRA },           // 19
	{ "2.8k",						0x0100, 0xe1770ad3, 7 | BRF_GRA },           // 20
	{ "s5.15p",						0x0100, 0x7f6cf709, 7 | BRF_GRA },           // 21

	{ "3h.bpr",						0x0020, 0x33b98466, 0 | BRF_OPT },           // 22 Unknown PROM

	{ "0.h7",						0x2000, 0x12681fb5, 8 | BRF_GRA },           // 23 Background Layer Scaling
	{ "1.9j",						0x0100, 0xf5b9b777, 8 | BRF_GRA },           // 24

	{ "4.7m",						0x0100, 0x12cbcd2c, 9 | BRF_GRA },           // 25 Sprite Scaling
	{ "s3.8l",						0x0100, 0x1314b0b5, 9 | BRF_GRA },           // 26
};

STD_ROM_PICK(splndrbt2)
STD_ROM_FN(splndrbt2)

struct BurnDriver BurnDrvSplndrbt2 = {
	"splndrbt2", NULL, NULL, "ad59mc07", "1985",
	"Splendor Blast II (World)\0", NULL, "Alpha Denshi Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_RACING | GBF_SHOOT, 0,
	NULL, splndrbt2RomInfo, splndrbt2RomName, NULL, NULL, mc07SampleInfo, mc07SampleName, SplndrbtInputInfo, SplndrbtDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x280,
	256, 192, 4, 3
};


// High Voltage (World)

static struct BurnRomInfo hvoltageRomDesc[] = {
	{ "1.16a",						0x4000, 0x82606e3b, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "2.16c",						0x4000, 0x1d74fef2, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.15a",						0x4000, 0x677abe14, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.15c",						0x4000, 0x8aab5a20, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "5_v.1l",						0x4000, 0xed9bb6ea, 2 | BRF_PRG | BRF_ESS }, //  4 i8085A Code
	{ "6_v.1h",						0x4000, 0xe9542211, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "7_v.1e",						0x4000, 0x44d38554, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "alpha-8505_44801c57.bin",	0x2000, 0x1f5a1405, 3 | BRF_PRG | BRF_ESS }, //  7 Alpha8201 Code

	{ "5.8c",						0x2000, 0x656d53cd, 4 | BRF_GRA },           //  8 Characters

	{ "9.14m",						0x4000, 0x506a0989, 5 | BRF_GRA },           //  9 Background Tiles
	{ "10.12m",						0x4000, 0x98f87d4f, 5 | BRF_GRA },           // 10

	{ "8.18n",						0x4000, 0x725acae5, 6 | BRF_GRA },           // 11 Sprites
	{ "6.18m",						0x4000, 0x9baf2c68, 6 | BRF_GRA },           // 12
	{ "7.17m",						0x4000, 0x12d25fb1, 6 | BRF_GRA },           // 13

	{ "r.3a",						0x0100, 0x98eccbf6, 7 | BRF_GRA },           // 14 Color PROMs
	{ "g.1a",						0x0100, 0xfab2ed23, 7 | BRF_GRA },           // 15
	{ "b.2a",						0x0100, 0x7274961b, 7 | BRF_GRA },           // 16
	{ "2.8k",						0x0100, 0x685f4e44, 7 | BRF_GRA },           // 17
	{ "s5.15p",						0x0100, 0xb09bcc73, 7 | BRF_GRA },           // 18

	{ "3h.bpr",						0x0020, 0x33b98466, 0 | BRF_OPT },           // 19 Unknown PROM

	{ "0.8h",						0x2000, 0x12681fb5, 8 | BRF_GRA },           // 20 Background Layer Scaling
	{ "1.9j",						0x0100, 0xf5b9b777, 8 | BRF_GRA },           // 21

	{ "4.7m",						0x0100, 0x12cbcd2c, 9 | BRF_GRA },           // 22 Sprite Scaling
	{ "3.8l",						0x0100, 0x1314b0b5, 9 | BRF_GRA },           // 23
};

STD_ROM_PICK(hvoltage)
STD_ROM_FN(hvoltage)

struct BurnDriver BurnDrvHvoltage = {
	"hvoltage", NULL, NULL, "ad59mc07", "1985",
	"High Voltage (World)\0", NULL, "Alpha Denshi Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, hvoltageRomInfo, hvoltageRomName, NULL, NULL, mc07SampleInfo, mc07SampleName, HvoltageInputInfo, HvoltageDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x280,
	256, 192, 4, 3
};
