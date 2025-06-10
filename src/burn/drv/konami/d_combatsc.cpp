// FB Alpha Combat School driver module
// Based on MAME driver by Phil Stroffolino, Manuel Abadia

#include "tiles_generic.h"
#include "hd6309_intf.h"
#include "z80_intf.h"
#include "burn_ym2203.h"
#include "upd7759.h"
#include "konamiic.h"
#include "k007121.h"
#include "k007452.h"
#include "watchdog.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvHD6309ROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvSndROM;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvScrollRAM[2];
static UINT8 *DrvPalRAM;
static UINT8 *DrvHD6309RAM;
static UINT8 *color_table;

static UINT32 *DrvPalette;
//static UINT8 DrvRecalc;

static UINT8 soundlatch;
static UINT8 video_reg;
static UINT8 bank_data;
static UINT8 priority_select;
static UINT8 video_circuit;

static INT32 nExtraCycles;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo CombatscInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 6,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 5,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 4,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Combatsc)

static struct BurnDIPInfo CombatscDIPList[]=
{
	{0x11, 0xff, 0xff, 0xff, NULL					},
	{0x12, 0xff, 0xff, 0x60, NULL					},
	{0x13, 0xff, 0xff, 0x50, NULL					},

	{0   , 0xfe, 0   ,    16, "Coin A"				},
	{0x11, 0x01, 0x0f, 0x02, "4 Coins 1 Credits="	},
	{0x11, 0x01, 0x0f, 0x05, "3 Coins 1 Credits"	},
	{0x11, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x11, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x11, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x11, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x11, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x11, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x11, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x11, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x11, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x11, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x11, 0x01, 0x0f, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    16, "Coin B"				},
	{0x11, 0x01, 0xf0, 0x20, "4 Coins 1 Credits"	},
	{0x11, 0x01, 0xf0, 0x50, "3 Coins 1 Credits"	},
	{0x11, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"	},
	{0x11, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"	},
	{0x11, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"	},
	{0x11, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"	},
	{0x11, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x11, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x11, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x11, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x11, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x11, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x11, 0x01, 0xf0, 0x00, "None"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x12, 0x01, 0x04, 0x00, "Upright"				},
	{0x12, 0x01, 0x04, 0x04, "Cocktail"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x12, 0x01, 0x60, 0x60, "Easy"					},
	{0x12, 0x01, 0x60, 0x40, "Normal"				},
	{0x12, 0x01, 0x60, 0x20, "Difficult"			},
	{0x12, 0x01, 0x60, 0x00, "Very Difficult"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x12, 0x01, 0x80, 0x80, "Off"					},
	{0x12, 0x01, 0x80, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x13, 0x01, 0x10, 0x10, "Off"					},
	{0x13, 0x01, 0x10, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x13, 0x01, 0x40, 0x40, "Off"					},
	{0x13, 0x01, 0x40, 0x00, "On"					},
};

STDDIPINFO(Combatsc)

static void bankswitch(UINT8 data)
{
	bank_data = data; // priority = data & 0x20;

	priority_select = data & 0x20;
	video_circuit = (data >> 6) & 1;

	if (video_circuit) {
		HD6309MapMemory(DrvVidRAM + 0x2000, 0x2000, 0x3fff, MAP_RAM);
	} else {
		HD6309MapMemory(DrvVidRAM + 0x0000, 0x2000, 0x3fff, MAP_RAM);
	}

	INT32 bank = 0;

	if (data & 0x10) {
		bank += ((data >> 1) & 7) * 0x4000;
	} else {
		bank += (8 + (data & 1)) * 0x4000;
	}

	HD6309MapMemory(DrvHD6309ROM + bank, 0x4000, 0x7fff, MAP_ROM);
}

static void pf_control_write(UINT8 offset, UINT8 data)
{
	k007121_ctrl_write(video_circuit, offset, data);

	if (offset == 7)
	{
	//	m_bg_tilemap[m_video_circuit]->set_flip((data & 0x08) ? (TILEMAP_FLIPY | TILEMAP_FLIPX) : 0);
	//	if(m_video_circuit == 0)
	//	{
	//		m_textflip = (data & 0x08) == 0x08;
	//		m_textlayer->set_flip((data & 0x08) ? TILEMAP_FLIPY | TILEMAP_FLIPX : 0);
	//	}
	}
}

static void combatsc_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfff8) == 0x0000) {
		pf_control_write(address, data);
		return;
	}

	if (address >= 0x0020 && address <= 0x005f) {
		DrvScrollRAM[video_circuit][address - 0x0020] = data;
		return;
	}

	switch (address)
	{
		case 0x0200:
		case 0x0201:
		case 0x0202:
		case 0x0203:
		case 0x0204:
		case 0x0205:
		case 0x0206:
		case 0x0207:
			K007452Write(address & 7, data);
		return;

		case 0x0408:
			// coin counter
		return;

		case 0x040c:
			video_reg = data;
		return;

		case 0x410:
			bankswitch(data);
		return;

		case 0x0414:
			soundlatch = data;
		return;

		case 0x0418:
			ZetSetVector(0xff);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		return;

		case 0x041c:
			BurnWatchdogWrite();
		return;
	}
}

static UINT8 combatsc_main_read(UINT16 address)
{
	if (address >= 0x0020 && address <= 0x005f) {
		return DrvScrollRAM[video_circuit][address - 0x0020];
	}

	switch (address)
	{
		case 0x001f:
			return 0; // unk??

		case 0x0200:
		case 0x0201:
		case 0x0202:
		case 0x0203:
		case 0x0204:
		case 0x0205:
		case 0x0206:
		case 0x0207:
			return K007452Read(address & 7);

		case 0x0400:
			return DrvInputs[0];

		case 0x0401:
			return (DrvInputs[2] & 0x0f) | (DrvDips[2] & 0xf0);

		case 0x0402:
			return DrvDips[0];

		case 0x0403:
			return DrvDips[1];

		case 0x0404:
			return DrvInputs[1]; // trackball inputs here too!!
	}

	return 0;
}

static void __fastcall combatsc_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x9000:
			UPD7759StartWrite(0, data & 2);
		return;

		case 0xa000:
			UPD7759PortWrite(0, data);
		return;

		case 0xc000:
			UPD7759ResetWrite(0, data & 1);
		return;

		case 0xe000:
		case 0xe001:
			BurnYM2203Write(0, address & 1, data);
		return;
	}
}

static UINT8 __fastcall combatsc_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xb000:
			return UPD7759BusyRead(0) ? 1 : 0;

		case 0xd000:
			return soundlatch;

		case 0xe000:
		case 0xe001:
			return BurnYM2203Read(0, address & 1);
	}

	return 0;
}

static tilemap_callback( bg0 )
{
	UINT8 ctrl6 = k007121_ctrl_read(0, 6);
	UINT8 attr = DrvVidRAM[offs];
	INT32 bank = 4 * ((video_reg & 0x0f) - 1);
	if (bank < 0) bank = 0;

	if ((attr & 0xb0) == 0)
		bank = 0;   // text bank
	else
		bank += ((attr & 0x30) >> 3) | (attr >> 7);

	INT32 color = ((ctrl6 & 0x10) * 2) + (attr & 0x0f);

	INT32 code = DrvVidRAM[offs + 0x400] + (256 * bank);

	TILE_SET_INFO(0, code, color, TILE_GROUP((attr >> 6) & 1));
}

static tilemap_callback( bg1 )
{
	UINT8 ctrl6 = k007121_ctrl_read(1, 6);
	UINT8 attr = DrvVidRAM[0x2000 + offs];
	INT32 bank = 4 * ((video_reg >> 4) - 1);

	if (bank < 0) bank = 0;

	if ((attr & 0xb0) == 0)
		bank = 0;   // text bank
	else
		bank += ((attr & 0x30) >> 3) | (attr >> 7);

	INT32 color = ((ctrl6 & 0x10) * 2) + (attr & 0x0f);

	INT32 code = DrvVidRAM[0x2400 + offs] + (256 * bank);

	TILE_SET_INFO(1, code, color, TILE_GROUP((attr >> 6) & 1));
}

static tilemap_callback( txt )
{
	TILE_SET_INFO(0, DrvVidRAM[offs + 0xc00], DrvVidRAM[offs + 0x800] & 0xf, 0);
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	HD6309Open(0);
	HD6309Reset();
	bankswitch(0);
	HD6309Close();

	ZetOpen(0);
	ZetReset();
	ZetClose();

	UPD7759Reset();
	BurnYM2203Reset();

	BurnWatchdogReset();

	k007121_reset();

	K007452Reset();

	soundlatch = 0;
	video_reg = 0;

	nExtraCycles = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvHD6309ROM		= Next; Next += 0x030000;
	DrvZ80ROM			= Next; Next += 0x008000;

	DrvGfxROM0			= Next; Next += 0x100000;
	DrvGfxROM1			= Next; Next += 0x100000;

	DrvSndROM			= Next; Next += 0x020000;

	DrvColPROM			= Next; Next += 0x000400;

	color_table			= Next; Next += 0x000800;

	DrvPalette			= (UINT32*)Next; Next += 0x800 * sizeof(UINT32);

	AllRam				= Next;

	DrvZ80RAM			= Next; Next += 0x001000;
	DrvVidRAM			= Next; Next += 0x004000;
	DrvScrollRAM[0]		= Next; Next += 0x000040;
	DrvScrollRAM[1]		= Next; Next += 0x000040;
	DrvPalRAM			= Next; Next += 0x000100;
	DrvHD6309RAM		= Next; Next += 0x001800;

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static void DrvColorTableInit()
{
	for (INT32 pal = 0; pal < 8; pal++)
	{
		INT32 clut = 1;
#if 0
		if (pal & 4) {
			if (pal & 2) {
				clut |= 2;
			} else {
				clut  = 2;
			}
		}
#endif
		switch (pal)
		{
			default:
			case 0: /* other sprites */
			case 2: /* other sprites(alt) */
			clut = 1;   /* 0 is wrong for Firing Range III targets */
			break;

			case 4: /* player sprites */
			case 6: /* player sprites(alt) */
			clut = 2;
			break;

			case 1: /* background */
			case 3: /* background(alt) */
			clut = 1;
			break;

			case 5: /* foreground tiles */
			case 7: /* foreground tiles(alt) */
			clut = 3;
			break;
		}

		for (INT32 i = 0; i < 0x100; i++)
		{
			UINT8 ctabentry;

			if (((pal & 0x01) == 0) && (DrvColPROM[(clut << 8) | i] == 0))
				ctabentry = 0;
			else
				ctabentry = (pal << 4) | (DrvColPROM[(clut << 8) | i] & 0x0f);

			color_table[(pal << 8) | i] = ctabentry;
		}
	}
}

static void DrvGfxExpand(UINT8 *src, INT32 len)
{
	for (INT32 i = (len - 1) * 2; i >= 0; i-=2) {
		src[i+0] = src[i/2] >> 4;
		src[i+1] = src[i/2] & 0xf;
	}
}

static INT32 DrvInit()
{
	GenericTilesInit();

	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvHD6309ROM + 0x20000,  0, 1)) return 1;
		if (BurnLoadRom(DrvHD6309ROM + 0x00000,  1, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM    + 0x00000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0   + 0x00000,  3, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0   + 0x00001,  4, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM1   + 0x00000,  5, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1   + 0x00001,  6, 2)) return 1;

		if (BurnLoadRom(DrvColPROM   + 0x00000,  7, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x00100,  8, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x00200,  9, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x00300, 10, 1)) return 1;

		if (BurnLoadRom(DrvSndROM    + 0x00000, 11, 1)) return 1;

		DrvGfxExpand(DrvGfxROM0, 0x80000);
		DrvGfxExpand(DrvGfxROM1, 0x80000);
		DrvColorTableInit();
	}

	HD6309Init(0);
	HD6309Open(0);
	HD6309MapMemory(DrvPalRAM,					0x0600, 0x06ff, MAP_RAM);
	HD6309MapMemory(DrvHD6309RAM,				0x0800, 0x1fff, MAP_RAM);
	HD6309MapMemory(DrvVidRAM,					0x2000, 0x3fff, MAP_RAM);
	HD6309MapMemory(DrvHD6309ROM  + 0x28000,	0x8000, 0xffff, MAP_ROM);
	HD6309SetWriteHandler(combatsc_main_write);
	HD6309SetReadHandler(combatsc_main_read);
	HD6309Close();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x7fff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM,			0x8000, 0x87ff, MAP_RAM);
	ZetSetWriteHandler(combatsc_sound_write);
	ZetSetReadHandler(combatsc_sound_read);
	ZetClose();

	BurnWatchdogInit(DrvDoReset, 180);

	BurnYM2203Init(1, 3000000, NULL, 0);
	BurnTimerAttachZet(3579545);
	BurnYM2203SetAllRoutes(0, 0.45, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetPSGVolume(0, 0.13);

	k007121_init(0, (0x100000 / (8 * 8)) - 1, DrvVidRAM + 0x1000);
	k007121_init(1, (0x100000 / (8 * 8)) - 1, DrvVidRAM + 0x3000);

	UPD7759Init(0, UPD7759_STANDARD_CLOCK, DrvSndROM);
	UPD7759SetRoute(0, 0.70, BURN_SND_ROUTE_BOTH);
	UPD7759SetSyncCallback(0, ZetTotalCycles, 3579545);

	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg0_map_callback, 8, 8, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, bg1_map_callback, 8, 8, 32, 32);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, txt_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 4, 8, 8, 0x100000, 0x100, 0x7f);
	GenericTilemapSetGfx(1, DrvGfxROM1, 4, 8, 8, 0x100000, 0x500, 0x7f);
	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetTransparent(2, 0);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	KonamiICExit();

	HD6309Exit();
	ZetExit();

	UPD7759Exit();
	BurnYM2203Exit();

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteUpdate()
{
	UINT32 pal[0x80];

	for (INT32 i = 0; i < 0x100; i+=2)
	{
		// xBBBBBGGGGGRRRRR
		UINT16 p = (DrvPalRAM[i+1] * 256) + DrvPalRAM[i + 0];
		UINT8 r = (p & 0x1f);
		UINT8 g = (p >> 5) & 0x1f;
		UINT8 b = (p >> 10) & 0x1f;

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2); 

		pal[i/2] = BurnHighCol(r,g,b,0);
	}

	for (INT32 i = 0; i < 0x800; i++)
	{
		DrvPalette[i] = pal[color_table[i]];
	}
}

static INT32 DrvDraw()
{
	DrvPaletteUpdate();

	for (INT32 i = 0; i < 2; i++)
	{
		if (k007121_ctrl_read(i, 1) & 2)
		{
			GenericTilemapSetScrollRows(i, 32);
			GenericTilemapSetScrollX(i, 0);

			for (INT32 j = 0; j < 32; j++)
			{
				GenericTilemapSetScrollRow(i, j, DrvScrollRAM[i][j]);
			}
		}
		else
		{
			GenericTilemapSetScrollRows(i, 1);
			GenericTilemapSetScrollX(i, k007121_ctrl_read(i, 0) | (k007121_ctrl_read(i, 1) << 8));
		}

		GenericTilemapSetScrollY(i, k007121_ctrl_read(i, 2));
	}

	INT32 color0 = (k007121_ctrl_read(0, 6) & 0x10) << 1;
	INT32 color1 = (k007121_ctrl_read(1, 6) & 0x10) << 1;

	BurnTransferClear();

	if (priority_select == 0)
	{
		if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, TMAP_DRAWOPAQUE | TMAP_SET_GROUP(0) | 4);
		if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, TMAP_DRAWOPAQUE | TMAP_SET_GROUP(1) | 8);
		if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, TMAP_SET_GROUP(0) | 1);
		if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, TMAP_SET_GROUP(1) | 2);

		if (nSpriteEnable & 2) k007121_draw(1, pTransDraw, DrvGfxROM1, color_table, (0x40 + color1), 0, 16, 0, 0x0f00, 0x0000);
		if (nSpriteEnable & 1) k007121_draw(0, pTransDraw, DrvGfxROM0, color_table, (0x00 + color0), 0, 16, 0, 0x4444, 0x0000);
	}
	else
	{
		if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, TMAP_DRAWOPAQUE | TMAP_SET_GROUP(0) | 1);
		if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, TMAP_DRAWOPAQUE | TMAP_SET_GROUP(1) | 2);

		if (nSpriteEnable & 2) k007121_draw(1, pTransDraw, DrvGfxROM1, color_table, (0x40 + color1), 0, 16, 0, 0x0f00, 0x0000);
		if (nSpriteEnable & 1) k007121_draw(0, pTransDraw, DrvGfxROM0, color_table, (0x00 + color0), 0, 16, 0, 0x4444, 0x0000);

		if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, TMAP_SET_GROUP(1) | 4);
		if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, TMAP_SET_GROUP(0) | 8);
	}

	{
		INT32 flags = (k007121_ctrl_read(0, 1) & 8) ? TMAP_FORCEOPAQUE : 0;

		for (INT32 i = 2; i < 30; i++)
		{
			INT32 line_enable = (DrvScrollRAM[video_circuit][0x20 + i]) ? 1 : 0;

			if (line_enable) {
				GenericTilesSetClip(-1, -1, (i - 2) * 8, ((i - 2) * 8) + 8);
				if (nBurnLayer & 4) GenericTilemapDraw(2, pTransDraw, 0 | flags);
				GenericTilesClearClip();
			}
		}
	}

	if (k007121_ctrl_read(0, 3) & 0x40)
	{
		for (INT32 y = 0; y < nScreenHeight; y++)
		{
			UINT16 *ldst = pTransDraw + (y * nScreenWidth);
			UINT16 *rdst = ldst + (nScreenWidth - 8);

			for (INT32 x = 0; x < 8; x++)
			{
				ldst[x] = rdst[x] = 0;
			}
		}
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 12000000 / 4 / 60, 3579545 / 60 };
	INT32 nCyclesDone[2] = { nExtraCycles, 0 };

	ZetOpen(0);
	HD6309Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, HD6309);

		if (i == 240) {
			HD6309SetIRQLine(HD6309_IRQ_LINE, CPU_IRQSTATUS_HOLD);
			k007121_buffer(0);
			k007121_buffer(1);

			if (pBurnDraw) {
				DrvDraw();
			}
		}

		CPU_RUN_TIMER(1);
	}

	HD6309Close();
	ZetClose();

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
		UPD7759Render(pBurnSoundOut, nBurnSoundLen);
	}

	nExtraCycles = nCyclesDone[0] - nCyclesTotal[0];

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029704;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		HD6309Scan(nAction);
		ZetScan(nAction);

		BurnWatchdogScan(nAction);

		k007121_scan(nAction);

		K007452Scan(nAction);

		BurnYM2203Scan(nAction, pnMin);
		UPD7759Scan(nAction, pnMin);

		SCAN_VAR(soundlatch);
		SCAN_VAR(video_reg);
		SCAN_VAR(bank_data);
		SCAN_VAR(nExtraCycles);
	}

	if (nAction & ACB_WRITE) {
		HD6309Open(0);
		bankswitch(bank_data);
		HD6309Close();
	}

	return 0;
}


// Combat School (joystick)

static struct BurnRomInfo combatscRomDesc[] = {
	{ "611g01.rom",			0x10000, 0x857ffffe, 1 | BRF_PRG | BRF_ESS }, 	//  0 HD6309 Code
	{ "611g02.rom",			0x20000, 0x9ba05327, 1 | BRF_PRG | BRF_ESS }, 	//  1

	{ "611g03.rom",			0x08000, 0x2a544db5, 2 | BRF_PRG | BRF_ESS }, 	//  2 Z80 Code

	{ "611g07.rom",			0x40000, 0x73b38720, 3 | BRF_GRA },           	//  3 Graphics Chip #0 Tiles
	{ "611g08.rom",			0x40000, 0x46e7d28c, 3 | BRF_GRA },           	//  4

	{ "611g11.rom",			0x40000, 0x69687538, 4 | BRF_GRA },           	//  5 Graphics Chip #1 Tiles
	{ "611g12.rom",			0x40000, 0x9c6bf898, 4 | BRF_GRA },           	//  6

	{ "611g06.h14",			0x00100, 0xf916129a, 5 | BRF_GRA },           	//  7 Color Data
	{ "611g05.h15",			0x00100, 0x207a7b07, 5 | BRF_GRA },           	//  8
	{ "611g10.h6",			0x00100, 0xf916129a, 5 | BRF_GRA },           	//  9
	{ "611g09.h7",			0x00100, 0x207a7b07, 5 | BRF_GRA },           	// 10

	{ "611g04.rom",			0x20000, 0x2987e158, 6 | BRF_SND },          	// 11 UPD Samples

	{ "ampal16l8.e7",		0x00104, 0x300a9936, 0 | BRF_OPT },          	// 12 Unused PLDs
	{ "pal16r6.16d",		0x00104, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },// 13
	{ "pal20l8.8h",			0x00144, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },// 14
};

STD_ROM_PICK(combatsc)
STD_ROM_FN(combatsc)

struct BurnDriver BurnDrvCombatsc = {
	"combatsc", NULL, NULL, NULL, "1988",
	"Combat School (joystick)\0", NULL, "Konami", "GX611",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_SHOOT, 0,
	NULL, combatscRomInfo, combatscRomName, NULL, NULL, NULL, NULL, CombatscInputInfo, CombatscDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x800,
	256, 224, 4, 3
};
