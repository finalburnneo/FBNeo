// FB Alpha One Shot One Kill driver module
// Based on MAME driver by David Haywood and Paul Priest

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "burn_gun.h"
#include "burn_ym3812.h"
#include "msm6295.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvSndROM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvMgRAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvBgRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvZ80RAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 *DrvScroll;
static UINT8 *soundlatch;

static UINT8 soundbank;
static INT32 input_x_wobble[2] = { 0, 0 };
static INT32 input_x[2] = { 0, 0 };
static INT32 input_y[2] = { 0, 0 };
static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvInputs[3];
static UINT8 DrvDips[2];
static UINT8 DrvReset;

static INT32 uses_gun = 0;
static INT16 DrvGun0 = 0;
static INT16 DrvGun1 = 0;
static INT16 DrvGun2 = 0;
static INT16 DrvGun3 = 0;

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo OneshotInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 start"	},
	A("P1 Gun X",    	BIT_ANALOG_REL, &DrvGun0,    "mouse x-axis"	),
	A("P1 Gun Y",    	BIT_ANALOG_REL, &DrvGun1,    "mouse y-axis"	),
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 4,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 start"	},
	A("P2 Gun X",    	BIT_ANALOG_REL, &DrvGun2,    "p2 x-axis"	),
	A("P2 Gun Y",    	BIT_ANALOG_REL, &DrvGun3,    "p2 y-axis"	),
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};
#undef A
STDINPUTINFO(Oneshot)

static struct BurnInputInfo MaddonnaInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 4,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Maddonna)

static struct BurnDIPInfo OneshotDIPList[]=
{
	{0x0b, 0xff, 0xff, 0x10, NULL					},
	{0x0c, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x0b, 0x01, 0x03, 0x03, "3 Coins 1 Credits"	},
	{0x0b, 0x01, 0x03, 0x02, "2 Coins 1 Credits"	},
	{0x0b, 0x01, 0x03, 0x00, "1 Coin  1 Credits"	},
	{0x0b, 0x01, 0x03, 0x01, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x0b, 0x01, 0x10, 0x00, "Off"					},
	{0x0b, 0x01, 0x10, 0x10, "On"					},

	{0   , 0xfe, 0   ,    2, "Start Round"			},
	{0x0b, 0x01, 0x40, 0x00, "Gun Trigger"			},
	{0x0b, 0x01, 0x40, 0x40, "Start Button"			},

	{0   , 0xfe, 0   ,    2, "Gun Test"				},
	{0x0b, 0x01, 0x80, 0x00, "Off"					},
	{0x0b, 0x01, 0x80, 0x80, "On"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x0c, 0x01, 0x03, 0x01, "1"					},
	{0x0c, 0x01, 0x03, 0x02, "2"					},
	{0x0c, 0x01, 0x03, 0x00, "3"					},
	{0x0c, 0x01, 0x03, 0x03, "5"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x0c, 0x01, 0x30, 0x10, "Easy"					},
	{0x0c, 0x01, 0x30, 0x00, "Normal"				},
	{0x0c, 0x01, 0x30, 0x20, "Hard"					},
	{0x0c, 0x01, 0x30, 0x30, "Hardest"				},

	{0   , 0xfe, 0   ,    2, "Round Select"			},
	{0x0c, 0x01, 0x40, 0x40, "Off"					},
	{0x0c, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Free Play"			},
	{0x0c, 0x01, 0x80, 0x00, "Off"					},
	{0x0c, 0x01, 0x80, 0x80, "On"					},
};

STDDIPINFO(Oneshot)

static struct BurnDIPInfo MaddonnaDIPList[]=
{
	{0x11, 0xff, 0xff, 0x04, NULL					},
	{0x12, 0xff, 0xff, 0x1a, NULL					},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x11, 0x01, 0x03, 0x03, "3 Coins 1 Credits"	},
	{0x11, 0x01, 0x03, 0x02, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x03, 0x01, "2 Coins 2 Credits"	},
	{0x11, 0x01, 0x03, 0x00, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    2, "Girl Pictures"		},
	{0x11, 0x01, 0x04, 0x00, "Off"					},
	{0x11, 0x01, 0x04, 0x04, "On"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x11, 0x01, 0x10, 0x10, "Off"					},
	{0x11, 0x01, 0x10, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Invulnerability"		},
	{0x11, 0x01, 0x40, 0x00, "Off"					},
	{0x11, 0x01, 0x40, 0x40, "On"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x12, 0x01, 0x03, 0x00, "Easy"					},
	{0x12, 0x01, 0x03, 0x01, "Normal"				},
	{0x12, 0x01, 0x03, 0x02, "Hard"					},
	{0x12, 0x01, 0x03, 0x03, "Hardest"				},

	{0   , 0xfe, 0   ,    3, "Time Per Round"		},
	{0x12, 0x01, 0x0c, 0x08, "80 Seconds"			},
	{0x12, 0x01, 0x0c, 0x04, "90 Seconds"			},
	{0x12, 0x01, 0x0c, 0x00, "100 Seconds"			},

	{0   , 0xfe, 0   ,    2, "Lives"				},
	{0x12, 0x01, 0x10, 0x10, "3"					},
	{0x12, 0x01, 0x10, 0x00, "5"					},

	{0   , 0xfe, 0   ,    4, "Hurry Up!"			},
	{0x12, 0x01, 0xc0, 0x00, "Off"					},
	{0x12, 0x01, 0xc0, 0x40, "On - 10"				},
	{0x12, 0x01, 0xc0, 0x80, "On - 01"				},
	{0x12, 0x01, 0xc0, 0xc0, "On - 11"				},
};

STDDIPINFO(Maddonna)

static void oki_bankswitch(INT32 data)
{
	soundbank = data;

	INT32 bank = ((soundbank ^ 3) & 3) * 0x40000;

	MSM6295SetBank(0, DrvSndROM + bank, 0, 0x3ffff);
}

static void __fastcall oneshot_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x190010:
			*soundlatch = data;
		return;

		case 0x190018:
			oki_bankswitch(data);
		return;
	}
}

static void __fastcall oneshot_main_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x190010:
			*soundlatch = data;
		return;

		case 0x190018:
			oki_bankswitch(data);
		return;
	}
}

static UINT8 __fastcall oneshot_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x190002:
		case 0x190003:
			return *soundlatch;

		case 0x190026:
		case 0x190027:
			input_x_wobble[0] ^= 1;
			return input_x[0] ^ input_x_wobble[0];

		case 0x19002e:
		case 0x19002f:
			input_x_wobble[1] ^= 1;
			return input_x[1] ^ input_x_wobble[1];

		case 0x190036:
		case 0x190037:
			return input_y[0];

		case 0x19003e:
		case 0x19003f:
			return input_y[1];

		case 0x19c020:
		case 0x19c021:
			return DrvDips[0];

		case 0x19c024:
		case 0x19c025:
			return DrvDips[1];

		case 0x19c02c:
		case 0x19c02d:
			return DrvInputs[0];

		case 0x19c030:
		case 0x19c031:
			return DrvInputs[1];

		case 0x19c034:
		case 0x19c035:
			return DrvInputs[2];
	}

	return 0;
}

static UINT16 __fastcall oneshot_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x190002:
			return *soundlatch;

		case 0x190026:
			input_x_wobble[0] ^= 1;
			return input_x[0] ^ input_x_wobble[0];

		case 0x19002e:
			input_x_wobble[1] ^= 1;
			return input_x[1] ^ input_x_wobble[1];

		case 0x190036:
			return input_y[0];

		case 0x19003e:
			return input_y[1];

		case 0x19c020:
			return DrvDips[0];

		case 0x19c024:
			return DrvDips[1];

		case 0x19c02c:
			return DrvInputs[0];

		case 0x19c030:
			return DrvInputs[1];

		case 0x19c034:
			return DrvInputs[2];
	}

	return 0;
}

static void __fastcall oneshot_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xe000:
		case 0xe001:
			BurnYM3812Write(0, address & 1, data);
		return;

		case 0xe010:
			MSM6295Write(0, data);
		return;
	}
}

static UINT8 __fastcall oneshot_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xe000:
		case 0xe001:
			return BurnYM3812Read(0, address & 1);

		case 0xe010:
			return MSM6295Read(0);
	}

	return 0;
}

static tilemap_callback( background )
{
	UINT16 *ram = (UINT16*)DrvBgRAM;

	TILE_SET_INFO(0, ram[offs * 2 + 1], 0, 0);
}

static tilemap_callback( midground )
{
	UINT16 *ram = (UINT16*)DrvMgRAM;

	TILE_SET_INFO(1, ram[offs * 2 + 1], 0, 0);
}

static tilemap_callback( foreground )
{
	UINT16 *ram = (UINT16*)DrvFgRAM;

	TILE_SET_INFO(2, ram[offs * 2 + 1], 0, 0);
}

static void DrvYM3812IrqHandler(INT32, INT32 nStatus)
{
	ZetSetIRQLine(0, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	ZetOpen(0);
	ZetReset();
	oki_bankswitch(3);
	MSM6295Reset(0);
	BurnYM3812Reset();
	ZetClose();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x040000;
	DrvZ80ROM		= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += 0x400000;
	DrvGfxROM1		= Next; Next += 0x400000;

	MSM6295ROM		= Next;
	DrvSndROM		= Next; Next += 0x100000;

	DrvPalette		= (UINT32*)Next; Next += 0x401 * sizeof(UINT32);

	AllRam			= Next;

	Drv68KRAM		= Next; Next += 0x008000;
	DrvMgRAM		= Next; Next += 0x001000;
	DrvFgRAM		= Next; Next += 0x001000;
	DrvBgRAM		= Next; Next += 0x001000;
	DrvSprRAM		= Next; Next += 0x001000;
	DrvPalRAM		= Next; Next += 0x000800;
	DrvScroll		= Next; Next += 0x000400; // 1024 is minimum 68k page size

	soundlatch		= Next; // soundlatch is first byte of z80 ram
	DrvZ80RAM		= Next; Next += 0x000800;

	RamEnd			= Next;
	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[8]  = { STEP8(0,(0x80000*8)) };
	INT32 XOffs[16] = { STEP8(0,1), STEP8(64,1) };
	INT32 YOffs[16] = { STEP8(0,8), STEP8(128,8) };
	
	UINT8 *tmp = (UINT8*)BurnMalloc(0x400000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x400000);

	GfxDecode(0x04000, 8, 16, 16, Plane, XOffs, YOffs, 0x100, tmp, DrvGfxROM0);
	GfxDecode(0x10000, 8,  8,  8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM1);

	BurnFree (tmp);

	return 0;
}

static INT32 DrvInit(INT32 game_select)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x000000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  3, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x080000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x100000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x180000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x200000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x280000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x300000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x380000, 10, 1)) return 1;

		if (game_select == 0)
		{
			if (BurnLoadRom(DrvSndROM + 0x000000, 11, 1)) return 1;
			if (BurnLoadRom(DrvSndROM + 0x080000, 12, 1)) return 1;
		}

		DrvGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,		0x080000, 0x087fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		0x0c0000, 0x0c07ff, MAP_RAM);
	SekMapMemory(DrvSprRAM,		0x120000, 0x120fff, MAP_RAM);
	SekMapMemory(DrvMgRAM,		0x180000, 0x180fff, MAP_RAM);
	SekMapMemory(DrvFgRAM,		0x181000, 0x181fff, MAP_RAM);
	SekMapMemory(DrvBgRAM,		0x182000, 0x182fff, MAP_RAM);
	SekMapMemory(DrvScroll,		0x188000, 0x1883ff, MAP_WRITE); //0-f
	SekSetWriteWordHandler(0,	oneshot_main_write_word);
	SekSetWriteByteHandler(0,	oneshot_main_write_byte);
	SekSetReadWordHandler(0,	oneshot_main_read_word);
	SekSetReadByteHandler(0,	oneshot_main_read_byte);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,		0x8000, 0x87ff, MAP_RAM);
	ZetSetWriteHandler(oneshot_sound_write);
	ZetSetReadHandler(oneshot_sound_read);
	ZetClose();

	BurnYM3812Init(1, 3500000, &DrvYM3812IrqHandler, 0);
	BurnTimerAttachYM3812(&ZetConfig, 5000000);
	BurnYM3812SetRoute(0, BURN_SND_YM3812_ROUTE, 1.00, BURN_SND_ROUTE_BOTH);

	MSM6295Init(0, 1056000 / 132, 1);
	MSM6295SetRoute(0, (game_select ? 0 : 1.00), BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, background_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, midground_map_callback,  16, 16, 32, 32);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, foreground_map_callback, 16, 16, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 8, 16, 16, 0x400000, 0x000, 0);
	GenericTilemapSetGfx(1, DrvGfxROM0, 8, 16, 16, 0x400000, 0x200, 0);
	GenericTilemapSetGfx(2, DrvGfxROM0, 8, 16, 16, 0x400000, 0x300, 0);
	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetTransparent(2, 0);

	if (uses_gun)
		BurnGunInit(2, true);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	MSM6295Exit(0);
	BurnYM3812Exit();

	SekExit();
	ZetExit();

	if (uses_gun) {
		BurnGunExit();
		uses_gun = 0;
	}

	GenericTilesExit();

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteUpdate()
{
	UINT16 *p = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x800/2; i++)
	{
		INT32 r = (p[i] >>  0) & 0x1f;
		INT32 g = (p[i] >>  5) & 0x1f;
		INT32 b = (p[i] >> 10) & 0x1f;

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_sprites()
{
	UINT16 *source = (UINT16*)DrvSprRAM;

	for (INT32 i = 0; i < 0x1000 / 2; i+=4)
	{
		if (source[i + 0] == 0x0001)
			break;

		INT32 num   = (source[i + 1]);
		INT32 xsize = (source[i + 2] & 0x000f) + 1;
		INT32 ysize = (source[i + 3] & 0x000f) + 1;

		INT32 xpos  = (source[i + 2] >> 7) - 8;
		INT32 ypos  = (source[i + 3] >> 7) - 6;

		for (INT32 blockx = 0; blockx < xsize; blockx++)
		{
			for (INT32 blocky = 0; blocky < ysize; blocky++)
			{			
				Render8x8Tile_Mask_Clip(pTransDraw, num + (blocky * xsize) + blockx, xpos + blockx * 8,         ypos + blocky * 8, 0, 8, 0, 0x100, DrvGfxROM1);
				Render8x8Tile_Mask_Clip(pTransDraw, num + (blocky * xsize) + blockx, xpos + blockx * 8 - 0x200, ypos + blocky * 8, 0, 8, 0, 0x100, DrvGfxROM1);
			}
		}
	}
}

static INT32 OneshotDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	}

	BurnTransferClear(0x400);

	UINT16 *scroll = (UINT16*)DrvScroll;

	GenericTilemapSetScrollX(1, scroll[0] - 0x1f5);
	GenericTilemapSetScrollY(1, scroll[1]);

	BurnTransferClear();
	
	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);
	if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 0);
	if (nSpriteEnable & 1) draw_sprites();
	if (nBurnLayer & 4) GenericTilemapDraw(2, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	for (INT32 i = 0; i < nBurnGunNumPlayers; i++) {
		BurnGunDrawTarget(i, BurnGunX[i] >> 8, BurnGunY[i] >> 8);
	}

	return 0;
}

static INT32 MaddonnaDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	}

	BurnTransferClear(0x400);

	UINT16 *scroll = (UINT16*)DrvScroll;

	GenericTilemapSetScrollY(1, scroll[1]);

	GenericTilemapDraw(1, pTransDraw, 0);
	GenericTilemapDraw(2, pTransDraw, 0);
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
		memset (DrvInputs, 0, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= DrvJoy1[i] << i;
			DrvInputs[1] ^= DrvJoy2[i] << i;
			DrvInputs[2] ^= DrvJoy3[i] << i;
		}

		if (uses_gun) {
			input_x[0] = ((BurnGunReturnX(0) & 0xff) * 320 / 256) + 30;
			input_y[0] = ((BurnGunReturnY(0) & 0xff) * 240 / 256) - 10;
			if (input_y[0] < 0) input_y[0] = 0;

			input_x[1] = ((BurnGunReturnX(1) & 0xff) * 320 / 256) + 20;
			input_y[1] = ((BurnGunReturnY(1) & 0xff) * 240 / 256) + 00;

			BurnGunMakeInputs(0, (INT16)DrvGun0, (INT16)DrvGun1);
			BurnGunMakeInputs(1, (INT16)DrvGun2, (INT16)DrvGun3);
		}

	}

	INT32 nInterleave = 10;
	INT32 nCyclesTotal[2] = { 12000000 / 60, 5000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++) {
		nCyclesDone[0] += SekRun(nCyclesTotal[0] / nInterleave);
		if (i == (nInterleave - 1)) SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);

		BurnTimerUpdateYM3812((1 + i) * nCyclesTotal[1] / nInterleave);
	}

	BurnTimerEndFrameYM3812(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		BurnYM3812Update(pBurnSoundOut, nBurnSoundLen);
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	SekClose();

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

		SekScan(nAction);
		ZetScan(nAction);

		if (uses_gun) {
			BurnGunScan();
		}

		BurnYM3812Scan(nAction, pnMin);
		MSM6295Scan(nAction, pnMin);

		SCAN_VAR(soundbank);
	}

	if (nAction & ACB_WRITE) {
		oki_bankswitch(soundbank);
	}

	return 0;
}


// One Shot One Kill

static struct BurnRomInfo oneshotRomDesc[] = {
	{ "1shot-u.a24",	0x20000, 0x0ecd33da, 1 | BRF_PRG | BRF_ESS }, //  0 68K code
	{ "1shot-u.a22",	0x20000, 0x26c3ae2d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "1shot.ua2",		0x10000, 0xf655b80e, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 code

	{ "1shot-ui.16a",	0x80000, 0xf765f9a2, 3 | BRF_GRA },           //  3 Graphics
	{ "1shot-ui.13a",	0x80000, 0x3361b5d8, 3 | BRF_GRA },           //  4
	{ "1shot-ui.11a",	0x80000, 0x8f8bd027, 3 | BRF_GRA },           //  5
	{ "1shot-ui.08a",	0x80000, 0x254b1701, 3 | BRF_GRA },           //  6
	{ "1shot-ui.16",	0x80000, 0xff246b27, 3 | BRF_GRA },           //  7
	{ "1shot-ui.13",	0x80000, 0x80342e83, 3 | BRF_GRA },           //  8
	{ "1shot-ui.11",	0x80000, 0xb8938345, 3 | BRF_GRA },           //  9
	{ "1shot-ui.08",	0x80000, 0xc9953bef, 3 | BRF_GRA },           // 10

	{ "1shot.u15",		0x80000, 0xe3759a47, 4 | BRF_SND },           // 11 Samples
	{ "1shot.u14",		0x80000, 0x222e33f8, 4 | BRF_SND },           // 12

	{ "1shot.mb",		0x10000, 0x6b213183, 0 | BRF_OPT },           // 13 Unknown
};

STD_ROM_PICK(oneshot)
STD_ROM_FN(oneshot)

static INT32 OneshotInit()
{
	uses_gun = 1;

	return DrvInit(0);
}

struct BurnDriver BurnDrvOneshot = {
	"oneshot", NULL, NULL, NULL, "1996",
	"One Shot One Kill\0", NULL, "Promat", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, oneshotRomInfo, oneshotRomName, NULL, NULL, NULL, NULL, OneshotInputInfo, OneshotDIPInfo,
	OneshotInit, DrvExit, DrvFrame, OneshotDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};


// Mad Donna (set 1)

static struct BurnRomInfo maddonnaRomDesc[] = {
	{ "maddonna.b16",	0x20000, 0x643f9054, 1 | BRF_PRG | BRF_ESS }, //  0 68K code
	{ "maddonna.b15",	0x20000, 0xe36c0e26, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "x13.ua2",		0x10000, 0xf2080071, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 code

	{ "maddonna.b5",	0x80000, 0x838d3244, 3 | BRF_GRA },           //  3 Graphics
	{ "maddonna.b7",	0x80000, 0x4920d2ec, 3 | BRF_GRA },           //  4
	{ "maddonna.b9",	0x80000, 0x3a8a3feb, 3 | BRF_GRA },           //  5
	{ "maddonna.b11",	0x80000, 0x6f9b7fdf, 3 | BRF_GRA },           //  6
	{ "maddonna.b6",	0x80000, 0xb02e9e0e, 3 | BRF_GRA },           //  7
	{ "maddonna.b8",	0x80000, 0x03f1de40, 3 | BRF_GRA },           //  8
	{ "maddonna.b10",	0x80000, 0x87936423, 3 | BRF_GRA },           //  9
	{ "maddonna.b12",	0x80000, 0x879ab23c, 3 | BRF_GRA },           // 10

	{ "x1",				0x10000, 0x6b213183, 0 | BRF_OPT },           // 11 Unknown
};

STD_ROM_PICK(maddonna)
STD_ROM_FN(maddonna)

static INT32 MaddonnaInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvMaddonna = {
	"maddonna", NULL, NULL, NULL, "1995",
	"Mad Donna (set 1)\0", NULL, "Tuning", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MAZE, 0,
	NULL, maddonnaRomInfo, maddonnaRomName, NULL, NULL, NULL, NULL, MaddonnaInputInfo, MaddonnaDIPInfo,
	MaddonnaInit, DrvExit, DrvFrame, MaddonnaDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};


// Mad Donna (set 2)

static struct BurnRomInfo maddonnbRomDesc[] = {
	{ "maddonnb.b16",	0x20000, 0x00000000, 1 | BRF_NODUMP | BRF_PRG | BRF_ESS }, //  0 68K code
	{ "maddonnb.b15",	0x20000, 0x00000000, 1 | BRF_NODUMP | BRF_PRG | BRF_ESS }, //  1

	{ "x13.ua2",		0x10000, 0xf2080071, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 code

	{ "x5.16a",			0x80000, 0x1aae0ad3, 3 | BRF_GRA },           //  3 Graphics
	{ "x7.13a",			0x80000, 0x39d13e25, 3 | BRF_GRA },           //  4
	{ "x9.11a",			0x80000, 0x2027faeb, 3 | BRF_GRA },           //  5
	{ "x11.08a",		0x80000, 0x4afcfba6, 3 | BRF_GRA },           //  6
	{ "x6.16",			0x80000, 0x7b893e78, 3 | BRF_GRA },           //  7
	{ "x8.13",			0x80000, 0xfed90a1f, 3 | BRF_GRA },           //  8
	{ "x10.11",			0x80000, 0x479d718c, 3 | BRF_GRA },           //  9
	{ "x12.08",			0x80000, 0xd56ca9f8, 3 | BRF_GRA },           // 10

	{ "x1",				0x10000, 0x6b213183, 0 | BRF_OPT },           // 11 Unknown
};

STD_ROM_PICK(maddonnb)
STD_ROM_FN(maddonnb)

struct BurnDriverD BurnDrvMaddonnb = {
	"maddonnb", "maddonna", NULL, NULL, "1995",
	"Mad Donna (set 2)\0", NULL, "Tuning", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MAZE, 0,
	NULL, maddonnbRomInfo, maddonnbRomName, NULL, NULL, NULL, NULL, MaddonnaInputInfo, MaddonnaDIPInfo,
	MaddonnaInit, DrvExit, DrvFrame, MaddonnaDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};
