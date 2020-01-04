// FBAlpha Pit 'n Run driver, based on MAME driver by Tomasz Slanina, Pierpaolo Prazzoli

// Jump Kun fully working.
//
// pitnrun todo: hook up "heed" clipping, hook up spotlight drawing

#include "tiles_generic.h"
#include "z80_intf.h"
#include "taito_m68705.h"
#include "ay8910.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvMCUROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvVidRAM0;
static UINT8 *DrvVidRAM1;
static UINT8 *DrvSprRAM;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvMCURAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 nmi_enable;
static UINT8 color_select;
static UINT8 char_bank;
static UINT8 flipscreen[2];
static UINT8 soundlatch;
static INT32 scrollx;
static INT32 scrolly;
static UINT8 heed_data[2];
static UINT8 ha_data;

static INT32 watchdog;
static INT32 watchdog_enable = 0;
static INT32 game_select = 0;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[1];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static struct BurnInputInfo PitnrunInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 1,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Pitnrun)

static struct BurnInputInfo JumpkunInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 1,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Jumpkun)

static struct BurnDIPInfo JumpkunDIPList[]=
{
	{0x0a, 0xff, 0xff, 0x01, NULL			},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x0a, 0x01, 0x07, 0x00, "2 Coins 1 Credits"	},
	{0x0a, 0x01, 0x07, 0x01, "1 Coin  1 Credits"	},
	{0x0a, 0x01, 0x07, 0x02, "1 Coin  2 Credits"	},
	{0x0a, 0x01, 0x07, 0x03, "1 Coin  3 Credits"	},
	{0x0a, 0x01, 0x07, 0x04, "1 Coin  4 Credits"	},
	{0x0a, 0x01, 0x07, 0x05, "1 Coin  5 Credits"	},
	{0x0a, 0x01, 0x07, 0x06, "1 Coin  6 Credits"	},
	{0x0a, 0x01, 0x07, 0x07, "1 Coin  7 Credits"	},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x0a, 0x01, 0x10, 0x00, "2"			},
	{0x0a, 0x01, 0x10, 0x10, "4"			},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x0a, 0x01, 0x20, 0x00, "Off"			},
	{0x0a, 0x01, 0x20, 0x20, "On"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0a, 0x01, 0x40, 0x00, "Upright"		},
	{0x0a, 0x01, 0x40, 0x40, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Invincibility (Cheat)"},
	{0x0a, 0x01, 0x80, 0x00, "Off"			},
	{0x0a, 0x01, 0x80, 0x80, "On"			},
};

STDDIPINFO(Jumpkun)

static struct BurnDIPInfo PitnrunDIPList[]=
{
	{0x09, 0xff, 0xff, 0x01, NULL			},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x09, 0x01, 0x07, 0x00, "2 Coins 1 Credits"	},
	{0x09, 0x01, 0x07, 0x01, "1 Coin  1 Credits"	},
	{0x09, 0x01, 0x07, 0x02, "1 Coin  2 Credits"	},
	{0x09, 0x01, 0x07, 0x03, "1 Coin  3 Credits"	},
	{0x09, 0x01, 0x07, 0x04, "1 Coin  4 Credits"	},
	{0x09, 0x01, 0x07, 0x05, "1 Coin  5 Credits"	},
	{0x09, 0x01, 0x07, 0x06, "1 Coin  6 Credits"	},
	{0x09, 0x01, 0x07, 0x07, "1 Coin  7 Credits"	},

	{0   , 0xfe, 0   ,    0, "Gasoline Count"	},
	{0x09, 0x01, 0x20, 0x00, "10 Up or 10 Down"	},
	{0x09, 0x01, 0x20, 0x20, "20 Up or 20 Down"	},

	{0   , 0xfe, 0   ,    0, "Cabinet"		},
	{0x09, 0x01, 0x40, 0x00, "Upright"		},
	{0x09, 0x01, 0x40, 0x40, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "No Hit (Cheat)"	},
	{0x09, 0x01, 0x80, 0x00, "Off"			},
	{0x09, 0x01, 0x80, 0x80, "On"			},
};

STDDIPINFO(Pitnrun)

static void __fastcall pitnrun_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xa800:
		case 0xa801:
		case 0xa802:
		case 0xa803:
		case 0xa804:
		case 0xa805:
		case 0xa806:
		case 0xa807:
		return;	// analog (nop)

		case 0xb000:
			nmi_enable = data & 1;
		return;

		case 0xb001:
			color_select = data;
		return;

		case 0xb004:
		return; // color sel2?

		case 0xb005:
			char_bank = data;
		return;

		case 0xb006: // hflip
		case 0xb007: // vflip
			flipscreen[address & 1] = data;
		return;

		case 0xb800:
			soundlatch = data;
		return;

		case 0xc800:
		case 0xc801:
			scrollx = (scrollx & (0xff<<((address&1)?0:8))) | (data<<((address&1)?8:0));
		return;

		case 0xc802:
			scrolly = data;
		return;

		case 0xc804:
			standard_taito_mcu_write(data);
		return;

		case 0xc805: // h
		case 0xc806: // v
			heed_data[(address - 1) & 1] = data;
		return;

		case 0xc807:
			ha_data = data;
		return;
	}
}

static UINT8 __fastcall pitnrun_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xa800:
			return DrvInputs[0];

		case 0xb000:
			return DrvDips[0];

		case 0xb800:
			return DrvInputs[1];

		case 0xd000:
			return standard_taito_mcu_read();

		case 0xd800:
			return ~((main_sent << 1) | ((mcu_sent^1) << 0));

		case 0xf000:
			watchdog = 0;
			watchdog_enable = 1;
			return 0;
	}

	return 0;
}

static void __fastcall pitnrun_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			soundlatch = 0;
		return;

		case 0x8c:
		case 0x8d:
			AY8910Write(1, port & 1, data);
		return;

		case 0x8e:
		case 0x8f:
			AY8910Write(0, port & 1, data);
		return;

		case 0x90:
		case 0x91:
		case 0x92:
		case 0x93:
		case 0x94:
		case 0x95:
		case 0x96:
		case 0x97:
		case 0x98:
		return;	// nop
	}
}

static UINT8 __fastcall pitnrun_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x8f:
			return AY8910Read(0);
	}

	return 0;
}

static UINT16 m_address = 0;

static void m68705_portB_out(UINT8 *in_data)
{
	ZetOpen(0);

	UINT8 data = *in_data;

	if (~data & 0x02)
	{
		main_sent = 0;
		m68705SetIrqLine(0, 0 /*CLEAR_LINE*/);
		portA_in = from_main;
	}
	if (~data & 0x04)
	{
		from_mcu = portA_out;
		mcu_sent = 1;
	}
	if (~data & 0x10)
	{
		ZetWriteByte(m_address, portA_out);
	}
	if (~data & 0x20)
	{
		portA_in = ZetReadByte(m_address);
	}
	if (~data & 0x40)
	{
		m_address = (m_address & 0xff00) | portA_out;
	}
	if (~data & 0x80)
	{
		m_address = (m_address & 0x00ff) | (portA_out << 8);
	}

	ZetClose();
}

m68705_interface pitnrun_m68705_interface = {
	NULL /* portA */, m68705_portB_out          /* portB */, NULL /* portC */,
	NULL /* ddrA  */, NULL                      /* ddrB  */, NULL /* ddrC  */,
	NULL /* portA */, NULL                      /* portB */, standard_m68705_portC_in  /* portC */
};


static UINT8 AY8910_read(UINT32)
{
	return soundlatch;
}

static tilemap_callback( foreground )
{
	TILE_SET_INFO(0, DrvVidRAM0[offs], 0, 0);
}

static tilemap_callback( background )
{
	TILE_SET_INFO(1, DrvVidRAM1[offs] + (char_bank * 256), color_select&1, 0);
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset(AllRam, 0, RamEnd - AllRam);
	}

	ZetOpen(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	ZetClose();

	m67805_taito_reset();

	AY8910Reset(0);
	AY8910Reset(1);

	nmi_enable = 0;
	color_select = 0;
	char_bank = 0;
	flipscreen[0] = flipscreen[1] = 0;
	soundlatch = 0;
	scrollx = 0;
	scrolly = 0;
	heed_data[0] = heed_data[1] = 0;
	ha_data = 0;

	watchdog = 0;
	watchdog_enable = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0	= Next; Next += 0x0010000;
	DrvZ80ROM1	= Next; Next += 0x0010000;

	DrvMCUROM	= Next; Next += 0x0008000;

	DrvGfxROM0	= Next; Next += 0x0020000;
	DrvGfxROM1	= Next; Next += 0x0020000;
	DrvGfxROM2	= Next; Next += 0x0020000;
	DrvGfxROM3	= Next; Next += 0x0020000;

	DrvColPROM	= Next; Next += 0x0000600;

	DrvPalette	= (UINT32*)Next; Next += 0x00800 * sizeof(UINT32);

	AllRam		= Next;

	DrvZ80RAM0	= Next; Next += 0x0000800;
	DrvVidRAM0	= Next; Next += 0x0001000;
	DrvVidRAM1	= Next; Next += 0x0001000;
	DrvSprRAM	= Next; Next += 0x0001000;
	DrvZ80RAM1	= Next; Next += 0x0000400;

	DrvMCURAM	= Next; Next += 0x0000800;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static void DrvGfxDecode()
{
	static INT32 Plane0[4]  = { RGN_FRAC(0x4000, 1, 2), RGN_FRAC(0x4000, 1, 2) + 4, 0, 4 };
	static INT32 XOffs0[8]  = { STEP4(0, 1), STEP4(8, 1) };
	static INT32 YOffs0[8]  = { STEP8(0, 16) };

	static INT32 Plane1[3]  = { 0, RGN_FRAC(0x6000, 1, 3), RGN_FRAC(0x6000, 2, 3) };
	static INT32 XOffs1[16] = { STEP8(0, 1), STEP8(64, 1) };
	static INT32 YOffs1[16] = { STEP8(0, 8), STEP8(128, 8) };

	UINT8 *tmp = (UINT8 *)BurnMalloc(0x6000);
	if (tmp == NULL) {
		return;
	}

	memcpy (tmp, DrvGfxROM0, 0x6000);

	GfxDecode(0x0100, 3, 16, 16, Plane1, XOffs1, YOffs1, 0x100, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x4000);

	GfxDecode(0x0200, 4,  8,  8, Plane0, XOffs0, YOffs0, 0x080, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x4000);

	GfxDecode(0x0200, 4,  8,  8, Plane0, XOffs0, YOffs0, 0x080, tmp, DrvGfxROM2);

	BurnFree (tmp);
}

static INT32 DrvInit(INT32 game)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	game_select = game;

	if (game_select == 0)
	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x2000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x4000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x6000,  3, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x0000,  4, 1)) return 1;

		if (BurnLoadRom(DrvMCUROM  + 0x0000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x0000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x2000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x4000,  8, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x0000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x2000, 10, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x0000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x2000, 12, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x0000, 13, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x0000, 14, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0020, 15, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0040, 16, 1)) return 1;
	}

	if (game_select == 1)
	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x2000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x4000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x6000,  3, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x0000,  4, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM1 + 0x1000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x0000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x2000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x4000,  8, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x0000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x2000, 10, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x0000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x2000, 12, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x0000, 13, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0020, 14, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0040, 15, 1)) return 1;
	}

	DrvGfxDecode();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,	0x8000, 0x87ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM0,	0x8800, 0x8fff, MAP_RAM);
	ZetMapMemory(DrvVidRAM1,	0x9000, 0x9fff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xa000, 0xa0ff, MAP_RAM);
	ZetSetWriteHandler(pitnrun_main_write);
	ZetSetReadHandler(pitnrun_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,	0x3800, 0x3bff, MAP_RAM);
	ZetSetOutHandler(pitnrun_sound_write_port);
	ZetSetInHandler(pitnrun_sound_read_port);
	ZetClose();

	m67805_taito_init(DrvMCUROM, DrvMCURAM, &pitnrun_m68705_interface);

	AY8910Init(0, 1536000, 0);
	AY8910Init(1, 1536000, 1);
	AY8910SetPorts(0, &AY8910_read, &AY8910_read, NULL, NULL);
	AY8910SetPorts(1, &AY8910_read, &AY8910_read, NULL, NULL);
	AY8910SetAllRoutes(0, 0.15, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.15, BURN_SND_ROUTE_BOTH);
	AY8910SetBuffered(ZetTotalCycles, 2500000);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, foreground_map_callback, 8, 8,  32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, background_map_callback, 8, 8, 128, 32);
	GenericTilemapSetGfx(0, DrvGfxROM2, 4, 8, 8, 0x2000*2, 0x40, 1);
	GenericTilemapSetGfx(1, DrvGfxROM1, 4, 8, 8, 0x4000*2, 0x20, 1);
	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	m67805_taito_exit();
	AY8910Exit(0);
	AY8910Exit(1);

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x60; i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 0x01;
		UINT8 r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		bit0 = (DrvColPROM[i] >> 3) & 0x01;
		bit1 = (DrvColPROM[i] >> 4) & 0x01;
		bit2 = (DrvColPROM[i] >> 5) & 0x01;
		UINT8 g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		bit0 = 0;
		bit1 = (DrvColPROM[i] >> 6) & 0x01;
		bit2 = (DrvColPROM[i] >> 7) & 0x01;
		UINT8 b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}

	for (INT32 i = 32; i < 48; i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 0x01;
		UINT8 r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		bit0 = (DrvColPROM[i] >> 3) & 0x01;
		bit1 = (DrvColPROM[i] >> 4) & 0x01;
		bit2 = (DrvColPROM[i] >> 5) & 0x01;
		UINT8 g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		bit0 = 0;
		bit1 = (DrvColPROM[i] >> 6) & 0x01;
		bit2 = (DrvColPROM[i] >> 7) & 0x01;
		UINT8 b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		r/=3;
		g/=3;
		b/=3;

		DrvPalette[i + 16] = BurnHighCol(r,g,b,0);
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0 ; offs < 0x100; offs+=4)
	{
		INT32 color = DrvSprRAM[offs+2] & 0x03;
		INT32 sy    = 256-DrvSprRAM[offs+0] - 16;
		INT32 sx    = DrvSprRAM[offs+3] + 1;
		INT32 flipy = DrvSprRAM[offs+1] & 0x80;
		INT32 flipx = DrvSprRAM[offs+1] & 0x40;
		INT32 code  =(DrvSprRAM[offs + 1] & 0x3f) | ((DrvSprRAM[offs + 2] & 0x80) >> 1) | ((DrvSprRAM[offs + 2] & 0x40) << 1);

		if (flipscreen[0])
		{
			sx = 256 - sx;
			flipx = !flipx;
		}
		if (flipscreen[1])
		{
			sy = 240 - sy;
			flipy = !flipy;
		}

		if (flipy) {
			if (flipx) {
				Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy - 16, color, 3, 0, 0, DrvGfxROM0);
			} else {
				Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy - 16, color, 3, 0, 0, DrvGfxROM0);
			}
		} else {
			if (flipx) {
				Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy - 16, color, 3, 0, 0, DrvGfxROM0);
			} else {
				Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy - 16, color, 3, 0, 0, DrvGfxROM0);
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	BurnTransferClear();

	GenericTilemapSetFlip(TMAP_GLOBAL, (flipscreen[0] ? TMAP_FLIPX : 0) | (flipscreen[1] ? TMAP_FLIPY : 0));

	if ((ha_data & 0x04) == 0) {
		GenericTilemapSetScrollX(1, scrollx);
		GenericTilemapSetScrollY(1, scrolly);
		if (nBurnLayer & 1) GenericTilemapDraw(1, pTransDraw, 0);
	}

	if (nSpriteEnable & 1) draw_sprites();

	if (nBurnLayer & 2) GenericTilemapDraw(0, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (watchdog_enable) watchdog++;
	if (watchdog >= 180) {
		DrvDoReset(0);
	}

	if (DrvReset) {
		DrvDoReset(1);
	}

	ZetNewFrame();

	{
		memset (DrvInputs, 0, 2);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[3] = { 3072000 / 60, 2500000 / 60, 3072000 / 60 };
	INT32 nCyclesDone[3] = { 0, 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		CPU_RUN(0, Zet);
		if (i == (nInterleave - 1) && nmi_enable) ZetNmi(); 
		ZetClose();

		ZetOpen(1);
		CPU_RUN(1, Zet);
		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_AUTO);
		ZetClose();

		if (game_select == 0) {
			m6805Open(0);
			CPU_RUN(2, m6805);
			m6805Close();
		}
	}

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
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
		m68705_taito_scan(nAction);

		AY8910Scan(nAction, pnMin);

		SCAN_VAR(nmi_enable);
		SCAN_VAR(color_select);
		SCAN_VAR(char_bank);
		SCAN_VAR(flipscreen);
		SCAN_VAR(soundlatch);
		SCAN_VAR(scrollx);
		SCAN_VAR(scrolly);
		SCAN_VAR(heed_data);
		SCAN_VAR(ha_data);
	}

	return 0;
}


// Pit & Run - F-1 Race (set 1)

static struct BurnRomInfo pitnrunRomDesc[] = {
	{ "pr12",		0x2000, 0x587a7b85, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "pr11",		0x2000, 0x270cd6dd, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "pr10",		0x2000, 0x65d92d89, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "pr9",		0x2000, 0x3155286d, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "pr13",		0x1000, 0xfc8fd05c, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code

	{ "a11_17.3a",		0x0800, 0xe7d5d6e1, 3 | BRF_PRG | BRF_ESS }, //  5 M68705 Code

	{ "pr1",		0x2000, 0xc3b3131e, 4 | BRF_GRA },           //  6 Sprites
	{ "pr2",		0x2000, 0x2fa1682a, 4 | BRF_GRA },           //  7
	{ "pr3",		0x2000, 0xe678fe39, 4 | BRF_GRA },           //  8

	{ "pr4",		0x2000, 0xfbae3504, 5 | BRF_GRA },           //  9 Background Tiles
	{ "pr5",		0x2000, 0xc9177180, 5 | BRF_GRA },           // 10

	{ "pr6",		0x1000, 0xc53cb897, 6 | BRF_GRA },           // 11 Foreground Tiles
	{ "pr7",		0x1000, 0x7cdf9a55, 6 | BRF_GRA },           // 12

	{ "pr8",		0x2000, 0x8e346d10, 7 | BRF_GRA },           // 13 Spot graphics

	{ "clr.1",		0x0020, 0x643012f4, 8 | BRF_GRA },           // 14 Color data
	{ "clr.2",		0x0020, 0x50705f02, 8 | BRF_GRA },           // 15
	{ "clr.3",		0x0020, 0x25e70e5e, 8 | BRF_GRA },           // 16
};

STD_ROM_PICK(pitnrun)
STD_ROM_FN(pitnrun)

static INT32 PitnrunInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvPitnrun = {
	"pitnrun", NULL, NULL, NULL, "1984",
	"Pit & Run - F-1 Race (set 1)\0", "Missing analog sounds and some gfx effects", "Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_PREFIX_TAITO, GBF_RACING, 0,
	NULL, pitnrunRomInfo, pitnrunRomName, NULL, NULL, NULL, NULL, PitnrunInputInfo, PitnrunDIPInfo,
	PitnrunInit, DrvExit, DrvFrame, DrvDraw, NULL, &DrvRecalc, 0,
	224, 256, 3, 4
};


// Pit & Run - F-1 Race (set 2)

static struct BurnRomInfo pitnrunaRomDesc[] = {
	{ "pr_12-1.5d",		0x2000, 0x2539aec3, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "pr_11-1.5c",		0x2000, 0x818a49f8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "pr_10-1.5b",		0x2000, 0x69b3a864, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "pr_9-1.5a",		0x2000, 0xba0c4093, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "pr-13",		0x1000, 0x32a18d3b, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code

	{ "a11_17.3a",		0x0800, 0xe7d5d6e1, 3 | BRF_PRG | BRF_ESS }, //  5 M68705 Code

	{ "pr-1.1k",		0x2000, 0xc3b3131e, 4 | BRF_GRA },           //  6 Sprites
	{ "pr-2.1m",		0x2000, 0x2fa1682a, 4 | BRF_GRA },           //  7
	{ "pr-3.1n",		0x2000, 0xe678fe39, 4 | BRF_GRA },           //  8

	{ "pr-4.6d",		0x2000, 0xfbae3504, 5 | BRF_GRA },           //  9 Background Tiles
	{ "pr-5.6f",		0x2000, 0xc9177180, 5 | BRF_GRA },           // 10

	{ "pr-6.3m",		0x1000, 0xc53cb897, 6 | BRF_GRA },           // 11 Foreground Tiles
	{ "pr-7.3p",		0x1000, 0x7cdf9a55, 6 | BRF_GRA },           // 12

	{ "pr-8.4j",		0x2000, 0x8e346d10, 7 | BRF_GRA },           // 13 Spot graphics

	{ "clr.1",		0x0020, 0x643012f4, 8 | BRF_GRA },           // 14 Color data
	{ "clr.2",		0x0020, 0x50705f02, 8 | BRF_GRA },           // 15
	{ "clr.3",		0x0020, 0x25e70e5e, 8 | BRF_GRA },           // 16
};

STD_ROM_PICK(pitnruna)
STD_ROM_FN(pitnruna)

struct BurnDriver BurnDrvPitnruna = {
	"pitnruna", "pitnrun", NULL, NULL, "1984",
	"Pit & Run - F-1 Race (set 2)\0", "Missing analog sounds and some gfx effects", "Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_PREFIX_TAITO, GBF_RACING, 0,
	NULL, pitnrunaRomInfo, pitnrunaRomName, NULL, NULL, NULL, NULL, PitnrunInputInfo, PitnrunDIPInfo,
	PitnrunInit, DrvExit, DrvFrame, DrvDraw, NULL, &DrvRecalc, 0,
	224, 256, 3, 4
};


// Jump Kun (prototype)

static struct BurnRomInfo jumpkunRomDesc[] = {
	{ "pr1.5d.2764",	0x2000, 0xb0eabe9f, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "pr2.5c.2764",	0x2000, 0xd9240413, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "pr3.5b.2764",	0x2000, 0x105e3fec, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "pr4.5a.2764",	0x2000, 0x3a17ca88, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "snd1.2732",		0x1000, 0x1290f316, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code
	{ "snd2.2732",		0x1000, 0xec5e4489, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "obj1.1k.2764",	0x2000, 0x8929abfd, 3 | BRF_GRA },           //  6 Sprites
	{ "obj2.1m.2764",	0x2000, 0xc7bf5819, 3 | BRF_GRA },           //  7
	{ "obj3.1n.2764",	0x2000, 0x5eeec986, 3 | BRF_GRA },           //  8

	{ "chr1.6d.2764",	0x2000, 0x3c93d4ee, 4 | BRF_GRA },           //  9 Background Tiles
	{ "chr2.6f.2764",	0x2000, 0x154fad33, 4 | BRF_GRA },           // 10

	{ "bsc2.3m.2764",	0x1000, 0x25445f17, 5 | BRF_GRA },           // 11 Foreground Tiles
	{ "bsc1.3p.2764",	0x1000, 0x39ca2c37, 5 | BRF_GRA },           // 12

	{ "8h.82s123.bin",	0x0020, 0xe54a6fe6, 6 | BRF_GRA },           // 13 Color data
	{ "8l.82s123.bin",	0x0020, 0x624830d5, 6 | BRF_GRA },           // 14
	{ "8j.82s123.bin",	0x0020, 0x223a6990, 6 | BRF_GRA },           // 15
};

STD_ROM_PICK(jumpkun)
STD_ROM_FN(jumpkun)

static INT32 JumpkunInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvJumpkun = {
	"jumpkun", NULL, NULL, NULL, "1984",
	"Jump Kun (prototype)\0", NULL, "Kaneko", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_PREFIX_TAITO, GBF_PLATFORM, 0,
	NULL, jumpkunRomInfo, jumpkunRomName, NULL, NULL, NULL, NULL, JumpkunInputInfo, JumpkunDIPInfo,
	JumpkunInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	224, 256, 3, 4
};
