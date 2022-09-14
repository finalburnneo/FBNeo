// FB Neo Taxi "Driver Driver" Module
// Based on MAME driver by Nicola Salmoria

#include "tiles_generic.h"
#include "z80_intf.h"
#include "ay8910.h"
#include "8255ppi.h"

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
static UINT8 *DrvGfxROM3;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM0A;
static UINT8 *DrvZ80RAM0B;
static UINT8 *DrvZ80RAM1A;
static UINT8 *DrvZ80RAM1B;
static UINT8 *DrvZ80RAM2;
static UINT8 *DrvVidRAM0;
static UINT8 *DrvVidRAM1;
static UINT8 *DrvVidRAM2;
static UINT8 *DrvVidRAM3;
static UINT8 *DrvSprRAM0;
static UINT8 *DrvSprRAM1;
static UINT8 *DrvSprRAM2;
static UINT8 *DrvRadarRAM;

static UINT8 *sprite_control;
static UINT8 *scroll;

static UINT8 latchA;
static UINT8 latchB;
static UINT8 s1;
static UINT8 s2;
static UINT8 s3;
static UINT8 s4;
static UINT8 bgdisable;
static UINT8 flipscreen;

static INT32 nExtraCycles[3];

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo TaxidrivInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 0,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Taxidriv)

static struct BurnDIPInfo TaxidrivDIPList[]=
{
	{0x12, 0xff, 0xff, 0x00, NULL						},
	{0x13, 0xff, 0xff, 0x00, NULL						},
	{0x14, 0xff, 0xff, 0x00, NULL						},

	{0   , 0xfe, 0   ,    16, "Coin A"					},
	{0x12, 0x01, 0x0f, 0x0d, "4 Coins 1 Credits"		},
	{0x12, 0x01, 0x0f, 0x0a, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0x0f, 0x0e, "4 Coins 2 Credits"		},
	{0x12, 0x01, 0x0f, 0x07, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0x0f, 0x0b, "3 Coins 2 Credits"		},
	{0x12, 0x01, 0x0f, 0x00, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0x0f, 0x0c, "3 Coins 4 Credits"		},
	{0x12, 0x01, 0x0f, 0x08, "2 Coins 3 Credits"		},
	{0x12, 0x01, 0x0f, 0x01, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0x0f, 0x09, "2 Coins 5 Credits"		},
	{0x12, 0x01, 0x0f, 0x02, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0x0f, 0x03, "1 Coin  4 Credits"		},
	{0x12, 0x01, 0x0f, 0x04, "1 Coin  5 Credits"		},
	{0x12, 0x01, 0x0f, 0x05, "1 Coin  6 Credits"		},
	{0x12, 0x01, 0x0f, 0x06, "1 Coin  7 Credits"		},
	{0x12, 0x01, 0x0f, 0x0f, "Free Play"				},

	{0   , 0xfe, 0   ,    16, "Coin B"					},
	{0x12, 0x01, 0xf0, 0xd0, "4 Coins 1 Credits"		},
	{0x12, 0x01, 0xf0, 0xa0, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0xf0, 0xe0, "4 Coins 2 Credits"		},
	{0x12, 0x01, 0xf0, 0x70, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0xf0, 0xb0, "3 Coins 2 Credits"		},
	{0x12, 0x01, 0xf0, 0x00, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0xf0, 0xc0, "3 Coins 4 Credits"		},
	{0x12, 0x01, 0xf0, 0x80, "2 Coins 3 Credits"		},
	{0x12, 0x01, 0xf0, 0x10, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0xf0, 0x90, "2 Coins 5 Credits"		},
	{0x12, 0x01, 0xf0, 0x20, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0xf0, 0x30, "1 Coin  4 Credits"		},
	{0x12, 0x01, 0xf0, 0x40, "1 Coin  5 Credits"		},
	{0x12, 0x01, 0xf0, 0x50, "1 Coin  6 Credits"		},
	{0x12, 0x01, 0xf0, 0x60, "1 Coin  7 Credits"		},
	{0x12, 0x01, 0xf0, 0xf0, "Free Play"				},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x13, 0x01, 0x03, 0x00, "3"						},
	{0x13, 0x01, 0x03, 0x01, "4"						},
	{0x13, 0x01, 0x03, 0x02, "5"						},
	{0x13, 0x01, 0x03, 0x03, "255 (Cheat)"				},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x13, 0x01, 0x04, 0x04, "Upright"					},
	{0x13, 0x01, 0x04, 0x00, "Cocktail"					},

	{0   , 0xfe, 0   ,    8, "Fuel Consumption"			},
	{0x14, 0x01, 0x07, 0x00, "Slowest"					},
	{0x14, 0x01, 0x07, 0x01, "2"						},
	{0x14, 0x01, 0x07, 0x02, "3"						},
	{0x14, 0x01, 0x07, 0x03, "4"						},
	{0x14, 0x01, 0x07, 0x04, "5"						},
	{0x14, 0x01, 0x07, 0x05, "6"						},
	{0x14, 0x01, 0x07, 0x06, "7"						},
	{0x14, 0x01, 0x07, 0x07, "Fastest"					},
};

STDDIPINFO(Taxidriv)

static void __fastcall taxidriv_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xf400:
		case 0xf401:
		case 0xf402:
		case 0xf403:
			ppi8255_w(0, address & 3, data);
		return;

		case 0xf480:
		case 0xf481:
		case 0xf482:
		case 0xf483:
			ppi8255_w(2, address & 3, data);
		return;

		case 0xf500:
		case 0xf501:
		case 0xf502:
		case 0xf503:
			ppi8255_w(3, address & 3, data);
		return;

		case 0xf580:
		case 0xf581:
		case 0xf582:
		case 0xf583:
			ppi8255_w(4, address & 3, data);
		return;

		case 0xf584:
		case 0xf780:
		case 0xf781:
		return; // nop?

		case 0xf782:
		case 0xf783:
		case 0xf784:
		case 0xf785:
		case 0xf786:
		case 0xf787:
			scroll[address - 0xf782] = data;
		return;
	}

	bprintf(0, _T("wb  %x  %x\n"), address, data);
}

static UINT8 __fastcall taxidriv_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xf400:
		case 0xf401:
		case 0xf402:
		case 0xf403:
			return ppi8255_r(0, address & 3);

		case 0xf480:
		case 0xf481:
		case 0xf482:
		case 0xf483:
			return ppi8255_r(2, address & 3);

		case 0xf500:
		case 0xf501:
		case 0xf502:
		case 0xf503:
			return ppi8255_r(3, address & 3);

		case 0xf580:
		case 0xf581:
		case 0xf582:
		case 0xf583:
			return ppi8255_r(4, address & 3);
	}

	bprintf(0, _T("rb  %x\n"), address);

	return 0;
}

static void __fastcall taxidriv_sub_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xa000:
		case 0xa001:
		case 0xa002:
		case 0xa003:
			ppi8255_w(1, address & 3, data);
		return;
	}

	bprintf(0, _T("sub wb  %x  %x\n"), address, data);
}

static UINT8 __fastcall taxidriv_sub_read(UINT16 address)
{
	switch (address)
	{
		case 0xa000:
		case 0xa001:
		case 0xa002:
		case 0xa003:
			return ppi8255_r(1, address & 3);

		case 0xe000:
			return DrvDips[0];

		case 0xe001:
			return DrvDips[1];

		case 0xe002:
			return DrvDips[2];

		case 0xe003:
			return DrvInputs[0];

		case 0xe004:
			return DrvInputs[1];
	}

	bprintf(0, _T("sub rb  %x\n"), address);

	return 0;
}

static void __fastcall taxidriv_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
			AY8910Write((port>>1)&1, port & 1, data);
		return;
	}

	bprintf(0, _T("sound wp  %x  %x\n"), port, data);
}

static UINT8 __fastcall taxidriv_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x01:
		case 0x03:
			return AY8910Read((port>>1)&1);

	}

	bprintf(0, _T("sound rp  %x\n"), port);

	return 0;
}

static UINT8 ppi0_port_A_read()
{
	return latchA;
}

static UINT8 ppi0_port_C_read()
{
	return (s1 << 7);
}

static void ppi0_port_B_write(UINT8 data)
{
	latchB = data;
}

static void ppi0_port_C_write(UINT8 data)
{
	s2 = data & 1;
	bgdisable = data & 2;
	flipscreen = data & 8;
}

static UINT8 ppi1_port_B_read()
{
	return latchB;
}

static UINT8 ppi1_port_C_read()
{
	return (s2 << 7) | (s4 << 6) | ((DrvInputs[2] & 1) << 4);
}

static void ppi1_port_A_write(UINT8 data)
{
	latchA = data;
}

static void ppi1_port_C_write(UINT8 data)
{
	s1 = data & 1;
	s3 = (data & 2) >> 1;
}

#define ppi_sprctrl(PPINUM, PORTNUM, CTRLNUM)	\
static void ppi##PPINUM##_port_##PORTNUM##_write(UINT8 data)	{	\
	sprite_control[CTRLNUM] = data;									\
}

ppi_sprctrl(2, A, 0)
ppi_sprctrl(2, B, 1)
ppi_sprctrl(2, C, 2)
ppi_sprctrl(3, A, 3)
ppi_sprctrl(3, B, 4)
ppi_sprctrl(3, C, 5)
ppi_sprctrl(4, A, 6)
ppi_sprctrl(4, B, 7)
ppi_sprctrl(4, C, 8)

static UINT8 AY8910_0_port_A_read(UINT32)
{
	return latchA;
}

static UINT8 AY8910_1_port_A_read(UINT32)
{
	return s3;
}

static void ay8910_0_port_B_write(UINT32, UINT32 data)
{
	s4 = data & 1;
}

static tilemap_callback( bg )
{
	TILE_SET_INFO(0, DrvVidRAM3[offs], 0, 0);
}

static tilemap_callback( mg )
{
	TILE_SET_INFO(1, DrvVidRAM2[offs] + (DrvVidRAM2[offs + 0x400] * 256), 0, 0);
}

static tilemap_callback( fg )
{
	TILE_SET_INFO(2, DrvVidRAM1[offs], 0, 0);
}

static tilemap_callback( tx )
{
	TILE_SET_INFO(3, DrvVidRAM0[offs], 0, 0);
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

	ppi8255_reset();

	AY8910Reset(0);
	AY8910Reset(1);

	memset (sprite_control, 0, 9);
	memset (scroll, 0, 6);
	latchA = 0;
	latchB = 0;
	s1 = 0;
	s2 = 0;
	s3 = 0;
	s4 = 0;
	bgdisable = 0;
	flipscreen = 0;

	nExtraCycles[0] = nExtraCycles[1] = nExtraCycles[2] = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x008000;
	DrvZ80ROM1		= Next; Next += 0x004000;
	DrvZ80ROM2		= Next; Next += 0x002000;

	DrvGfxROM0		= Next; Next += 0x004000;
	DrvGfxROM1		= Next; Next += 0x004000;
	DrvGfxROM2		= Next; Next += 0x00c000;
	DrvGfxROM3		= Next; Next += 0x004000;

	DrvColPROM		= Next; Next += 0x000020;

	DrvPalette		= (UINT32*)Next; Next += 0x0010 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM0A		= Next; Next += 0x004000;
	DrvZ80RAM0B		= Next; Next += 0x000800;
	DrvZ80RAM1A		= Next; Next += 0x000800;
	DrvZ80RAM1B		= Next; Next += 0x000800;
	DrvZ80RAM2		= Next; Next += 0x000400;

	DrvVidRAM0		= Next; Next += 0x000400;
	DrvVidRAM1		= Next; Next += 0x000400;
	DrvVidRAM2		= Next; Next += 0x000800;
	DrvVidRAM3		= Next; Next += 0x000400;

	DrvSprRAM0		= Next; Next += 0x000800;
	DrvSprRAM1		= Next; Next += 0x000800;
	DrvSprRAM2		= Next; Next += 0x000800;

	DrvRadarRAM		= Next; Next += 0x000800;

	sprite_control	= Next; Next += 0x000010;
	scroll			= Next; Next += 0x000006;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void DrvGfxExpand(UINT8 *src, INT32 len, INT32 swap)
{
	for (INT32 i = (len - 1); i>= 0; i--)
	{
		INT32 data = src[i];
		src[i*2+(0^swap)] = data & 0xf;
		src[i*2+(1^swap)] = data >> 4;
	}
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x2000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x4000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x6000,  3, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x0000,  4, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM1 + 0x2000,  5, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM2 + 0x0000,  6, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x0000,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x0000,  8, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x0000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x2000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x4000, 11, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x0000, 12, 1)) return 1;

//		if (BurnLoadRom(DrvGfxROM4 + 0x0000, 13, 1)) return 1; // ??
//		if (BurnLoadRom(DrvGfxROM4 + 0x2000, 14, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x0000, 15, 1)) return 1;

		DrvGfxExpand(DrvGfxROM0, 0x2000, 0);
		DrvGfxExpand(DrvGfxROM1, 0x2000, 0);
		DrvGfxExpand(DrvGfxROM2, 0x6000, 0);
		DrvGfxExpand(DrvGfxROM3, 0x2000, 0);
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0A,		0x8000, 0xbfff, MAP_RAM);
	ZetMapMemory(DrvRadarRAM,		0xc000, 0xc7ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM0,		0xc800, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM1,		0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM2,		0xd800, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvVidRAM1,		0xe000, 0xe3ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM2,		0xe400, 0xebff, MAP_RAM);
	ZetMapMemory(DrvVidRAM0,		0xec00, 0xefff, MAP_RAM);
	ZetMapMemory(DrvVidRAM3,		0xf000, 0xf3ff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM0B,		0xf800, 0xffff, MAP_RAM);
	ZetSetWriteHandler(taxidriv_main_write);
	ZetSetReadHandler(taxidriv_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,		0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1A,		0x6000, 0x67ff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM1B,		0x8000, 0x87ff, MAP_RAM);
	ZetSetWriteHandler(taxidriv_sub_write);
	ZetSetReadHandler(taxidriv_sub_read);
	ZetClose();

	ZetInit(2);
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM2,		0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM2,		0xfc00, 0xffff, MAP_RAM);
	ZetSetOutHandler(taxidriv_sound_write_port);
	ZetSetInHandler(taxidriv_sound_read_port);
	ZetClose();

	ppi8255_init(5);
	ppi8255_set_read_ports(0, ppi0_port_A_read, NULL, ppi0_port_C_read);
	ppi8255_set_write_ports(0, NULL, ppi0_port_B_write, ppi0_port_C_write);
	ppi8255_set_read_ports(1, NULL, ppi1_port_B_read, ppi1_port_C_read);
	ppi8255_set_write_ports(1, ppi1_port_A_write, NULL, ppi1_port_C_write);
	ppi8255_set_write_ports(2, ppi2_port_A_write, ppi2_port_B_write, ppi2_port_C_write);
	ppi8255_set_write_ports(3, ppi3_port_A_write, ppi3_port_B_write, ppi3_port_C_write);
	ppi8255_set_write_ports(4, ppi4_port_A_write, ppi4_port_B_write, ppi4_port_C_write);

	AY8910Init(0, 1250000, 0);
	AY8910Init(1, 1250000, 0);
	AY8910SetPorts(0, &AY8910_0_port_A_read, NULL, NULL, &ay8910_0_port_B_write);
	AY8910SetPorts(1, &AY8910_1_port_A_read, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.12, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.12, BURN_SND_ROUTE_BOTH);
	AY8910SetBuffered(ZetTotalCycles, 4000000);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, mg_map_callback, 8, 8, 32, 32);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, fg_map_callback, 8, 8, 32, 32);
	GenericTilemapInit(3, TILEMAP_SCAN_ROWS, tx_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM3, 4, 8, 8, 0x4000, 0, 0);
	GenericTilemapSetGfx(1, DrvGfxROM2, 4, 8, 8, 0xc000, 0, 0);
	GenericTilemapSetGfx(2, DrvGfxROM1, 4, 8, 8, 0x4000, 0, 0);
	GenericTilemapSetGfx(3, DrvGfxROM0, 4, 8, 8, 0x4000, 0, 0);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetTransparent(2, 0);
	GenericTilemapSetTransparent(3, 0);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -8);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	ppi8255_exit();

	AY8910Exit(0);
	AY8910Exit(1);

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x10; i++) {
		INT32 bit0 = (DrvColPROM[i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 0x01;
		INT32 r = 0x55 * bit0 + 0xaa * bit1;

		bit0 = (DrvColPROM[i] >> 2) & 0x01;
		bit1 = (DrvColPROM[i] >> 3) & 0x01;
		INT32 g = 0x55 * bit0 + 0xaa * bit1;

		bit0 = (DrvColPROM[i] >> 4) & 0x01;
		bit1 = (DrvColPROM[i] >> 5) & 0x01;
		INT32 b = 0x55 * bit0 + 0xaa * bit1;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_bitmap(UINT8 *ram, INT32 length, INT32 colshift, UINT16 scrollx, UINT16 scrolly)
{
	for (INT32 offs = 0; offs < length; offs++)
	{
		INT32 sx = (((offs / 2) & 0x3f) - scrollx) & 0x1ff;
		INT32 sy = ((offs / 0x80) - scrolly) & 0x1ff;

		INT32 color = (ram[offs/4] >> ((offs & 3) * 2)) & 3;

		if (color && sx >= 0 && sx < nScreenWidth && sy >= 0 && sy < nScreenHeight)
		{
			pTransDraw[sy * nScreenWidth + sx] = color << colshift;
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	if (bgdisable)
	{
		BurnTransferClear();
	}
	else
	{
		INT32 scrollx, scrolly, enable;

		//GenericTilemapSetFlip(TMAP_GLOBAL, flipscreen ? TMAP_FLIPXY : 0);

		GenericTilemapSetScrollX(0, scroll[0]);
		GenericTilemapSetScrollY(0, scroll[1]);
		GenericTilemapSetScrollX(1, scroll[2]);
		GenericTilemapSetScrollY(1, scroll[3]);

		if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);
		if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 0);

		scrollx = (sprite_control[0] - ((sprite_control[2] & 1) * 256)) & 0x1ff;
		scrolly = (sprite_control[1] - ((sprite_control[2] & 2) * 128)) & 0x1ff;
		enable = sprite_control[2] & 4;

		if (enable && nSpriteEnable & 1) draw_bitmap(DrvSprRAM0, 0x1000, 0, scrollx, scrolly);

		scrollx = (sprite_control[3] - ((sprite_control[5] & 1) * 256)) & 0x1ff;
		scrolly = (sprite_control[4] - ((sprite_control[5] & 2) * 128)) & 0x1ff;
		enable = sprite_control[5] & 4;

		if (enable && nSpriteEnable & 2) draw_bitmap(DrvSprRAM1, 0x1000, 0, scrollx, scrolly);

		scrollx = (sprite_control[6] - ((sprite_control[8] & 1) * 256)) & 0x1ff;
		scrolly = (sprite_control[7] - ((sprite_control[8] & 2) * 128)) & 0x1ff;
		enable = sprite_control[8] & 4;

		if (enable && nSpriteEnable & 4) draw_bitmap(DrvSprRAM2, 0x1000, 0, scrollx, scrolly);

		if (nBurnLayer & 4) GenericTilemapDraw(2, pTransDraw, 0);

		if (nSpriteEnable & 8) draw_bitmap(DrvRadarRAM, 0x2000, 1, 0, 0);
	}

	if (nBurnLayer & 8) GenericTilemapDraw(3, pTransDraw, 0);

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
		DrvInputs[0] = 0;
		DrvInputs[1] = 0;
		DrvInputs[2] = 1;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[3] = { 4000000 / 60, 4000000 / 60, 4000000 / 60 };
	INT32 nCyclesDone[3] = { nExtraCycles[0], nExtraCycles[1], nExtraCycles[2] };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		CPU_RUN(0, Zet);
		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(1);
		CPU_RUN(1, Zet);
		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(2);
		CPU_RUN(2, Zet);
		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();
	}

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
	}

	if (bgdisable) { // needs to happen even if drawing is disabled! (runahead, etc)
		memset (scroll, 0, 4);
		sprite_control[2] = sprite_control[5] = sprite_control[8] = 0;
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	nExtraCycles[0] = nCyclesDone[0] - nCyclesTotal[0];
	nExtraCycles[1] = nCyclesDone[1] - nCyclesTotal[1];
	nExtraCycles[2] = nCyclesDone[2] - nCyclesTotal[2];

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
		AY8910Scan(nAction, pnMin);
		ppi8255_scan();

		SCAN_VAR(latchA);
		SCAN_VAR(latchB);
		SCAN_VAR(s1);
		SCAN_VAR(s2);
		SCAN_VAR(s3);
		SCAN_VAR(s4);
		SCAN_VAR(bgdisable);
		SCAN_VAR(flipscreen);

		SCAN_VAR(nExtraCycles);
	}

	return 0;
}


// Taxi Driver

static struct BurnRomInfo taxidrivRomDesc[] = {
	{ "1.ic87",			0x2000, 0x6b2424e9, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "2.ic86",			0x2000, 0x15111229, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.ic85",			0x2000, 0xa7782eee, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.ic84",			0x2000, 0x8eb0b16b, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "8.ic4",			0x2000, 0x9f9a3865, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code
	{ "9.ic5",			0x2000, 0xb28b766c, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "7.ic14",			0x2000, 0x2b4cbfe6, 3 | BRF_PRG | BRF_ESS }, //  6 Z80 #2 Code

	{ "5.m.ic68",		0x2000, 0xa3aa5f2f, 4 | BRF_GRA },           //  7 Text Tayer Tiles

	{ "6.1.ic35",		0x2000, 0xbfddd550, 5 | BRF_GRA },           //  8 Foreground Tiles

	{ "11.30.ic87",		0x2000, 0x7485eaea, 6 | BRF_GRA },           //  9 Midground Tiles
	{ "14.31.ic110",	0x2000, 0x0d99a33e, 6 | BRF_GRA },           // 10
	{ "15.32.ic111",	0x2000, 0x410fdf7c, 6 | BRF_GRA },           // 11

	{ "10.40.ic99",		0x2000, 0xc370b177, 7 | BRF_GRA },           // 12 Background Tiles

	{ "12.21.ic88",		0x2000, 0x684b7bb0, 0 | BRF_OPT },           // 13 Unused Tiles
	{ "13.20.ic89",		0x2000, 0xd1ef110e, 0 | BRF_OPT },           // 14

	{ "tbp18s030.ic2",	0x0020, 0xc366a9c5, 8 | BRF_GRA },           // 15 Color Ddata
};

STD_ROM_PICK(taxidriv)
STD_ROM_FN(taxidriv)

struct BurnDriver BurnDrvTaxidriv = {
	"taxidriv", NULL, NULL, NULL, "1984",
	"Taxi Driver\0", NULL, "Graphic Techno", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, taxidrivRomInfo, taxidrivRomName, NULL, NULL, NULL, NULL, TaxidrivInputInfo, TaxidrivDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 16,
	208, 256, 3, 4
};
