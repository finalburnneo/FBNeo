// FinalBurn Neo Mag Max driver module
// Based on MAME driver by Takahiro Nogi

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "ay8910.h"
#include "burn_pal.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM[4];
static UINT8 *DrvColPROM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvZ80RAM;

static INT32 vreg;
static INT32 scrollx;
static INT32 scrolly;
static UINT8 soundlatch;
static INT32 sound_flip_data;
static INT32 sound_flip_clear;
static UINT8 ay_gain;

static INT32 speed_toggle;

static INT32 flipscreen;
static INT32 prom_table[256];

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo MagmaxInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},

	{"Speed Up",		BIT_DIGITAL,	DrvJoy1 + 5,	"speedup"	},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 4,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Magmax)

static struct BurnDIPInfo MagmaxDIPList[]=
{
	{0x11, 0xff, 0xff, 0xff, NULL					},
	{0x12, 0xff, 0xff, 0xff, NULL					},
	{0x13, 0xff, 0xff, 0x20, NULL					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x11, 0x01, 0x03, 0x03, "3"					},
	{0x11, 0x01, 0x03, 0x02, "4"					},
	{0x11, 0x01, 0x03, 0x01, "5"					},
	{0x11, 0x01, 0x03, 0x00, "6"					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x11, 0x01, 0x0c, 0x0c, "30k 80k 50k+"			},
	{0x11, 0x01, 0x0c, 0x08, "50k 120k 70k+"		},
	{0x11, 0x01, 0x0c, 0x04, "70k 160k 90k+"		},
	{0x11, 0x01, 0x0c, 0x00, "90k 200k 110k+"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x11, 0x01, 0x10, 0x00, "Off"					},
	{0x11, 0x01, 0x10, 0x10, "On"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x11, 0x01, 0x20, 0x00, "Upright"				},
	{0x11, 0x01, 0x20, 0x20, "Cocktail"				},

	{0   , 0xfe, 0   ,    4, "Coin A"				},
	{0x12, 0x01, 0x03, 0x01, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x03, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x12, 0x01, 0x0c, 0x00, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x0c, 0x04, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0x0c, 0x0c, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x0c, 0x08, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Difficulty"			},
	{0x12, 0x01, 0x10, 0x10, "Easy"					},
	{0x12, 0x01, 0x10, 0x00, "Hard"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x12, 0x01, 0x20, 0x20, "Off"					},
	{0x12, 0x01, 0x20, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Debug Mode"			},
	{0x12, 0x01, 0x80, 0x80, "No"					},
	{0x12, 0x01, 0x80, 0x00, "Yes"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x13, 0x01, 0x20, 0x20, "No"					},
	{0x13, 0x01, 0x20, 0x00, "Yes"					},
};

STDDIPINFO(Magmax)

static void __fastcall magmax_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x30010:
			vreg = data;
		return;

		case 0x30012:
			scrollx = data;
		return;

		case 0x30014:
			scrolly = data;
		return;

		case 0x3001c:
			soundlatch = data;
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;

		case 0x3001e:
			SekSetIRQLine(1, CPU_IRQSTATUS_NONE);
		return;
	}
}

static void __fastcall magmax_main_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x3001d:
			soundlatch = data;
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;
	}
}

static UINT16 __fastcall magmax_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x30000:
			return (DrvInputs[0] & ~0x20) | (speed_toggle ? 0 : 0x20);

		case 0x30002:
			return DrvInputs[1];

		case 0x30004:
			return (DrvInputs[2] & ~0x20) | (DrvDips[2] & 0x20);

		case 0x30006:
			return (DrvDips[1] << 8) + (DrvDips[0]);
	}

	return 0;
}

static UINT8 __fastcall magmax_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x30001:
			return DrvInputs[0];

		case 0x30003:
			return DrvInputs[1];

		case 0x30005:
			return (DrvInputs[2] & ~0x20) | (DrvDips[2] & 0x20);

		case 0x30006:
			return DrvDips[1];

		case 0x30007:
			return DrvDips[0];
	}

	return 0;
}

static void __fastcall magmax_sound_write(UINT16 address, UINT8 data)
{
	if ((address & 0x6000) == 0x4000) {
		ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;
	}
}

static UINT8 __fastcall magmax_sound_read(UINT16 address)
{
	if ((address & 0x6000) == 0x4000) {
		ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
		return 0;
	}

	return 0;
}

static void __fastcall magmax_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
		case 0x04:
		case 0x05:
			AY8910Write((port / 2) & 3, port & 1, data);
		return;
	}
}

static UINT8 __fastcall magmax_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x06:
			return (soundlatch << 1) | sound_flip_data;
	}

	return 0;
}

static void ay8910_write_B(UINT32, UINT32 data)
{
	sound_flip_clear = data & 1;
	if (sound_flip_clear == 0)
		sound_flip_data = 0;
}

static void ay8910_write_A(UINT32, UINT32 data)
{
	if (data & 0x8000) ay_gain = 0xff; // state load bypass (return)
	data &= 0xff;

	if (data == ay_gain) return;

	ay_gain = data;

	float gain = (data & 1) ? 0.50 : 0.25;
	AY8910SetRoute(0, BURN_SND_AY8910_ROUTE_1, gain, BURN_SND_ROUTE_BOTH);

	gain = (data & 2) ? 0.22 : 0.11;
	AY8910SetRoute(0, BURN_SND_AY8910_ROUTE_2, gain, BURN_SND_ROUTE_BOTH);
	AY8910SetRoute(0, BURN_SND_AY8910_ROUTE_3, gain, BURN_SND_ROUTE_BOTH);
	AY8910SetRoute(1, BURN_SND_AY8910_ROUTE_1, gain, BURN_SND_ROUTE_BOTH);
	AY8910SetRoute(1, BURN_SND_AY8910_ROUTE_2, gain, BURN_SND_ROUTE_BOTH);

	gain = (data & 4) ? 0.22 : 0.11;
	AY8910SetRoute(1, BURN_SND_AY8910_ROUTE_3, gain, BURN_SND_ROUTE_BOTH);
	AY8910SetRoute(2, BURN_SND_AY8910_ROUTE_1, gain, BURN_SND_ROUTE_BOTH);

	gain = (data & 8) ? 0.22 : 0.11;
	AY8910SetRoute(2, BURN_SND_AY8910_ROUTE_2, gain, BURN_SND_ROUTE_BOTH);
	AY8910SetRoute(2, BURN_SND_AY8910_ROUTE_3, gain, BURN_SND_ROUTE_BOTH);
}

static tilemap_callback( fg )
{
	INT32 code = DrvVidRAM[offs * 2 + 0]; // was + 1?? -dink

	TILE_SET_INFO(0, code, 0, 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekReset(0);

	ZetReset(0);

	AY8910Reset(0);
	AY8910Reset(1);
	AY8910Reset(2);

	vreg = 0;
	scrollx = 0;
	scrolly = 0;
	soundlatch = 0;
	sound_flip_data = 0;
	sound_flip_clear = 0;
	speed_toggle = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x014000;

	DrvZ80ROM		= Next; Next += 0x004000;

	DrvGfxROM[0]	= Next; Next += 0x004000;
	DrvGfxROM[1]	= Next; Next += 0x020000;
	DrvGfxROM[2]	= Next; Next += 0x010000;
	DrvGfxROM[3]	= Next; Next += 0x000200;

	DrvColPROM		= Next; Next += 0x000400;

	BurnPalette		= (UINT32*)Next; Next += 0x210 * sizeof(UINT32);

	AllRam			= Next;

	Drv68KRAM		= Next; Next += 0x001000;
	DrvVidRAM		= Next; Next += 0x000800;
	DrvSprRAM		= Next; Next += 0x000400;

	DrvZ80RAM		= Next; Next += 0x001000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void DrvGfxDecode()
{
	INT32 PlaneOffsets[4]  = { STEP4(0,1) };
	INT32 CharXOffsets[8]    = { 4, 0, 12, 8, 20, 16, 28, 24 };
	INT32 SpriteXOffsets[16] = { 4, 0, 4+512*64*8, 0+512*64*8, 12, 8, 12+512*64*8, 8+512*64*8, 20, 16, 20+512*64*8, 16+512*64*8, 28, 24, 28+512*64*8, 24+512*64*8 };
	INT32 YOffsets[16]      = { STEP16(0,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x10000);
	if (tmp == NULL) return;

	memcpy (tmp, DrvGfxROM[0], 0x2000);

	GfxDecode(0x100, 4, 8, 8, PlaneOffsets, CharXOffsets, YOffsets, 0x100, tmp, DrvGfxROM[0]);

	memcpy (tmp, DrvGfxROM[1], 0xe000);

	GfxDecode(0x200, 4, 16, 16, PlaneOffsets, SpriteXOffsets, YOffsets, 0x200, tmp, DrvGfxROM[1]);

	BurnFree (tmp);
}

static void DrvInitPROMTable()
{
	for (int i = 0; i < 256; i++)
	{
		INT32 v = (DrvGfxROM[3][i] << 4) + DrvGfxROM[3][i + 0x100];
		prom_table[i] = ((v&0x1f)<<8) | ((v&0x10)<<10) | ((v&0xe0)>>1);
	}
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
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM    + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM    + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM    + 0x008000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM    + 0x008001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM    + 0x010000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM    + 0x010001, k++, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM    + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM    + 0x002000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[0] + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[1] + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x002000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x004000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x008000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x00a000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x00c000, k++, 1)) return 1;

		// user1
		if (BurnLoadRom(DrvGfxROM[2] + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x004000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x006000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x008000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x00a000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x00c000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x00e000, k++, 1)) return 1;

		// user2 - Bg Control data
		if (BurnLoadRom(DrvGfxROM[3] + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[3] + 0x000100, k++, 1)) return 1;

		if (BurnLoadRom(DrvColPROM   + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x000100, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x000200, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x000300, k++, 1)) return 1;

		DrvGfxDecode();
		DrvInitPROMTable();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x013fff, MAP_ROM);
	SekMapMemory(Drv68KRAM,			0x018000, 0x018fff, MAP_RAM);
	SekMapMemory(DrvVidRAM,			0x020000, 0x0207ff, MAP_RAM);
	SekMapMemory(DrvSprRAM,			0x028000, 0x0283ff, MAP_RAM); // 0-1ff
	SekSetWriteWordHandler(0,		magmax_main_write_word);
	SekSetWriteByteHandler(0,		magmax_main_write_byte);
	SekSetReadWordHandler(0,		magmax_main_read_word);
	SekSetReadByteHandler(0,		magmax_main_read_byte);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,			0x6000, 0x67ff, MAP_RAM);
	ZetSetWriteHandler(magmax_sound_write);
	ZetSetReadHandler(magmax_sound_read);
	ZetSetOutHandler(magmax_sound_write_port);
	ZetSetInHandler(magmax_sound_read_port);
	ZetClose();

	AY8910Init(0, 1250000, 0);
	AY8910Init(1, 1250000, 0);
	AY8910Init(2, 1250000, 0);
	AY8910SetAllRoutes(0, 0.15, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.15, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(2, 0.15, BURN_SND_ROUTE_BOTH);
	AY8910SetPorts(0, NULL, NULL, &ay8910_write_A, &ay8910_write_B);
	AY8910SetBuffered(ZetTotalCycles, 2500000);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, fg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 4,  8,  8, 0x04000, 0x00, 0x1);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 4, 16, 16, 0x20000, 0x10, 0xf);
	GenericTilemapSetOffsets(0, 0, -16);
	GenericTilemapSetTransparent(0, 0xf);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	SekExit();
	ZetExit();
	AY8910Exit(0);
	AY8910Exit(1);
	AY8910Exit(2);

	BurnFree(AllMem);

	return 0;
}

static void BurnPaletteInit()
{
	for (INT32 i = 0; i < 0x100; i++)
	{
		INT32 r = pal4bit(DrvColPROM[0x000 + i]);
		INT32 g = pal4bit(DrvColPROM[0x100 + i]);
		INT32 b = pal4bit(DrvColPROM[0x200 + i]);

		BurnPalette[256 + 16 + i] = BurnHighCol(r, g, b, 0);
	}

	for (INT32 i = 0; i < 16; i++)
	{
		BurnPalette[i] = BurnPalette[256 + 16 + i];
	}

	for (INT32 i = 0; i < 256; i++)
	{
		UINT8 v = (DrvColPROM[0x300 + i] & 0xf) | 0x110;
		BurnPalette[i + 16] = BurnPalette[256 + 16 + v];
	}
}

static void draw_background()
{
	UINT16 bg_data;
	INT32 scroll_h = scrollx & 0x3fff;
	INT32 scroll_v = scrolly & 0xff;
	UINT8 *m_rom18B = DrvGfxROM[2];

	for (INT32 v = 2*8; v < 30*8; v++)
	{
		UINT32 map_v_scr_100 =   (scroll_v + v) & 0x100;
		UINT32 rom18D_addr   =  ((scroll_v + v) & 0xf8)     + (map_v_scr_100<<5);
		UINT32 rom15F_addr   = (((scroll_v + v) & 0x07)<<2) + (map_v_scr_100<<5);
		UINT32 map_v_scr_1fe_6 =((scroll_v + v) & 0x1fe)<<6;
		UINT32 pen_base = 0x110 + 0x20 + (map_v_scr_100>>1);

		INT32 eff_y = (flipscreen) ? ((v ^ 0xff) - 16) * 256 : (v - 16) * 256;

		for (INT32 h = 0; h < 256; h++)
		{
			UINT32 LS283 = scroll_h + h;

			if (!map_v_scr_100)
			{
				if (h & 0x80)
					LS283 = LS283 + (m_rom18B[ map_v_scr_1fe_6 + (h ^ 0xff) ] ^ 0xff);
				else
					LS283 = LS283 + m_rom18B[ map_v_scr_1fe_6 + h ] + 0xff01;
			}

			UINT32 prom_data = prom_table[ (LS283 >> 6) & 0xff ];

			rom18D_addr &= 0x20f8;
			rom18D_addr += (prom_data & 0x1f00) + ((LS283 & 0x38) >>3);

			rom15F_addr &= 0x201c;
			rom15F_addr += (m_rom18B[0x4000 + rom18D_addr ]<<5) + ((LS283 & 0x6)>>1);
			rom15F_addr += (prom_data & 0x4000);

			UINT32 graph_color = (prom_data & 0x0070);

			UINT32 graph_data = m_rom18B[0x8000 + rom15F_addr];
			if ((LS283 & 1))
				graph_data >>= 4;
			graph_data &= 0x0f;

			bg_data = pen_base + graph_color + graph_data;

			INT32 over_sprites = 1 << (map_v_scr_100 && ((graph_data & 0x0c)==0x0c));
			INT32 eff_x = (flipscreen) ? (h ^ 0xff) : h;

			if (v >= 16 && v < 240 && h >= 0 && h < 256) {
				pTransDraw[eff_y + eff_x] = bg_data;
				pPrioDraw [eff_y + eff_x] = over_sprites;
			}
		}
	}
}

static void draw_sprites()
{
	UINT16 *ram = (UINT16*)DrvSprRAM;

	for (INT32 offs = 0x100 - 4; offs >= 0; offs -= 4)
	{
		INT32 sy = ram[offs] & 0xff;
		if (sy == 0) continue;

		INT32 code = ram[offs + 1] & 0xff;

		if (code & 0x80) {
			code += (vreg & 0x30) * 0x8;
		}

		INT32 attr = ram[offs + 2] & 0xff;
		INT32 color = (attr & 0xf0) >> 4;
		INT32 flipx = attr & 0x04;
		INT32 flipy = attr & 0x08;

		INT32 sx = (ram[offs + 3] & 0xff) - 0x80 + 0x100 * (attr & 0x01);
		sy = 239 - sy;

		if (flipscreen)
		{
			sx = 255-16 - sx;
			sy = 239 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		RenderPrioMaskTranstabSpriteOffset(pTransDraw, DrvGfxROM[1], code, (color << 4), 0x0f, sx, sy - 16, flipx, flipy, 16, 16, DrvColPROM + 0x300, 0x10, 4);
	}
}

static INT32 DrvDraw()
{
	if (BurnRecalc) {
		BurnPaletteInit();
		BurnRecalc = 0;
	}

	flipscreen = vreg & 0x04;

	GenericTilemapSetFlip(TMAP_GLOBAL, flipscreen ? (TMAP_FLIPY | TMAP_FLIPX) : 0);

	BurnTransferClear();

	if ((nBurnLayer & 1) && (vreg & 0x40) == 0) draw_background();

	if (nSpriteEnable & 1) draw_sprites();

	if ( nBurnLayer & 4) GenericTilemapDraw(0, pTransDraw, 0);

	BurnTransferFlip(flipscreen, flipscreen); // unflip coctail

	BurnTransferCopy(BurnPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		UINT8 prev = DrvInputs[0];
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		if ((prev & 0x20) && (~DrvInputs[0] & 0x20)) {
			speed_toggle ^= 1;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 8000000 / 60, 2500000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);
		CPU_RUN(1, Zet);

		if (i == 64 || i == 192) {
			if (sound_flip_clear != 0) sound_flip_data = 1;
		}

		if (i == 239) SekSetIRQLine(1, CPU_IRQSTATUS_ACK);
	}

	ZetClose();
	SekClose();

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
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
		*pnMin = 0x029702;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		SekScan(nAction);
		ZetScan(nAction);
		AY8910Scan(nAction, pnMin);

		SCAN_VAR(vreg);
		SCAN_VAR(scrollx);
		SCAN_VAR(scrolly);
		SCAN_VAR(soundlatch);
		SCAN_VAR(sound_flip_data);
		SCAN_VAR(sound_flip_clear);
		SCAN_VAR(ay_gain);

		SCAN_VAR(speed_toggle);
	}

	if (nAction & ACB_WRITE) {
		ay8910_write_A(0, ay_gain | 0x8000);
	}

	return 0;
}


// Mag Max

static struct BurnRomInfo magmaxRomDesc[] = {
	{ "1.3b",		0x4000, 0x33793cbb, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "6.3d",		0x4000, 0x677ef450, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "2.5b",		0x4000, 0x1a0c84df, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "7.5d",		0x4000, 0x01c35e95, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "3.6b",		0x2000, 0xd06e6cae, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "8.6d",		0x2000, 0x790a82be, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "15.17b",		0x2000, 0x19e7b983, 2 | BRF_PRG | BRF_ESS }, //  6 Z80 Code
	{ "16.18b",		0x2000, 0x055e3126, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "23.15g",		0x2000, 0xa7471da2, 3 | BRF_GRA },           //  8 Characters

	{ "17.3e",		0x2000, 0x8e305b2e, 4 | BRF_GRA },           //  9 Sprites
	{ "18.5e",		0x2000, 0x14c55a60, 4 | BRF_GRA },           // 10
	{ "19.6e",		0x2000, 0xfa4141d8, 4 | BRF_GRA },           // 11
	{ "20.3g",		0x2000, 0x6fa3918b, 4 | BRF_GRA },           // 12
	{ "21.5g",		0x2000, 0xdd52eda4, 4 | BRF_GRA },           // 13
	{ "22.6g",		0x2000, 0x4afc98ff, 4 | BRF_GRA },           // 14

	{ "4.18b",		0x2000, 0x1550942e, 5 | BRF_GRA },           // 15 Background Data
	{ "5.20b",		0x2000, 0x3b93017f, 5 | BRF_GRA },           // 16
	{ "9.18d",		0x2000, 0x9ecc9ab8, 5 | BRF_GRA },           // 17
	{ "10.20d",		0x2000, 0xe2ff7293, 5 | BRF_GRA },           // 18
	{ "11.15f",		0x2000, 0x91f3edb6, 5 | BRF_GRA },           // 19
	{ "12.17f",		0x2000, 0x99771eff, 5 | BRF_GRA },           // 20
	{ "13.18f",		0x2000, 0x75f30159, 5 | BRF_GRA },           // 21
	{ "14.20f",		0x2000, 0x96babcba, 5 | BRF_GRA },           // 22

	{ "mag_b.14d",	0x0100, 0xa0fb7297, 6 | BRF_GRA },           // 23 Background Control PROMs
	{ "mag_c.15d",	0x0100, 0xd84a6f78, 6 | BRF_GRA },           // 24

	{ "mag_e.10f",	0x0100, 0x75e4f06a, 7 | BRF_GRA },           // 25 Color PROMs
	{ "mag_d.10e",	0x0100, 0x34b6a6e3, 7 | BRF_GRA },           // 26
	{ "mag_a.10d",	0x0100, 0xa7ea7718, 7 | BRF_GRA },           // 27
	{ "mag_g.2e",	0x0100, 0x830be358, 7 | BRF_GRA },           // 28

	{ "mag_f.13b",	0x0100, 0x4a6f9a6d, 0 | BRF_OPT },           // 29 Video Control Data
};

STD_ROM_PICK(magmax)
STD_ROM_FN(magmax)

struct BurnDriver BurnDrvMagmax = {
	"magmax", NULL, NULL, NULL, "1985",
	"Mag Max\0", NULL, "Nichibutsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, magmaxRomInfo, magmaxRomName, NULL, NULL, NULL, NULL, MagmaxInputInfo, MagmaxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x210,
	256, 224, 4, 3
};
