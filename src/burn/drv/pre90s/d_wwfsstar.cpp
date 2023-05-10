// FB Neo WWF Superstars driver module
// Based on MAME driver by David Haywood

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "burn_ym2151.h"
#include "msm6295.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvSndROM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvBgRAM;

static UINT32 *DrvPalette;

static UINT8 *soundlatch;
static UINT8 *flipscreen;
static UINT16 *scroll;

static UINT16 DrvInputs[3];
static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvDips[2];
static UINT8 DrvReset;

static INT32 vblank = 0;

static struct BurnInputInfo WwfsstarInputList[] = {

	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"}	,
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"P3 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p3 start"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 3,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Wwfsstar)

static struct BurnDIPInfo WwfsstarDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x13, 0x01, 0x07, 0x00, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x07, 0x01, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x07, 0x02, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0x07, 0x04, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0x07, 0x03, "1 Coin  5 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x13, 0x01, 0x38, 0x00, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x38, 0x08, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x38, 0x10, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x38, 0x30, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x38, 0x28, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0x38, 0x20, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0x38, 0x18, "1 Coin  5 Credits"	},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x80, 0x80, "Off"			},
	{0x13, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x03, 0x01, "Easy"			},
	{0x14, 0x01, 0x03, 0x03, "Normal"		},
	{0x14, 0x01, 0x03, 0x02, "Hard"			},
	{0x14, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x14, 0x01, 0x04, 0x00, "Off"			},
	{0x14, 0x01, 0x04, 0x04, "On"			},

	{0   , 0xfe, 0   ,    2, "Super Techniques"	},
	{0x14, 0x01, 0x08, 0x08, "Normal"		},
	{0x14, 0x01, 0x08, 0x00, "Hard"			},

	{0   , 0xfe, 0   ,    4, "Time"			},
	{0x14, 0x01, 0x30, 0x20, "+2:30"		},
	{0x14, 0x01, 0x30, 0x30, "Default"		},
	{0x14, 0x01, 0x30, 0x10, "-2:30"		},
	{0x14, 0x01, 0x30, 0x00, "-5:00"		},

	{0   , 0xfe, 0   ,    2, "Health For Winning"	},
	{0x14, 0x01, 0x80, 0x80, "No"			},
	{0x14, 0x01, 0x80, 0x00, "Yes"			},
};

STDDIPINFO(Wwfsstar)

static void __fastcall wwfsstar_main_write_byte(UINT32 , UINT8 )
{
}

static void __fastcall wwfsstar_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x180000:
			SekSetIRQLine(6, CPU_IRQSTATUS_NONE);
		return;

		case 0x180002:
			SekSetIRQLine(5, CPU_IRQSTATUS_NONE);
		return;

		case 0x180004:
			scroll[0] = data & 0x1ff;
		return;

		case 0x180006:
			scroll[1] = data & 0x1ff;
		return;

		case 0x180008:
			*soundlatch = data;
			ZetNmi();
		return;

		case 0x18000a:
			*flipscreen = data & 1;
		return;
	}
}

static UINT8 __fastcall wwfsstar_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x180003:
			return DrvDips[1];

		case 0x180004:
			return DrvInputs[0] >> 8;

		case 0x180005:
			return DrvInputs[0];

		case 0x180006:
			return DrvInputs[1] >> 8;

		case 0x180007:
			return DrvInputs[1];

		case 0x180008:
			return DrvInputs[2] >> 8;

		case 0x180009:
			return (DrvInputs[2] & 0xfe) | vblank;
	}

	return 0;
}

static UINT16 __fastcall wwfsstar_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x180000:
			return 0xff00 | DrvDips[0];

		case 0x180002:
			return 0xff00 | DrvDips[1];

		case 0x180004:
			return DrvInputs[0];

		case 0x180006:
			return DrvInputs[1];

		case 0x180008:
			return (DrvInputs[2] & 0xfffe) | vblank;
	}

	return 0;
}

static void __fastcall wwfsstar_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x8800:
			BurnYM2151SelectRegister(data);
		return;

		case 0x8801:
			BurnYM2151WriteRegister(data);
		return;

		case 0x9800:
			MSM6295Write(0, data);
		return;
	}
}

static UINT8 __fastcall wwfsstar_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x8800:
		case 0x8801:
			return BurnYM2151Read();

		case 0x9800:
			return MSM6295Read(0);

		case 0xa000:
			return *soundlatch;
	}

	return 0;
}

static void DrvYM2151IrqHandler(INT32 Irq)
{
	ZetSetIRQLine(0, (Irq) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	ZetOpen(0);
	ZetReset();
	BurnYM2151Reset();
	ZetClose();

	MSM6295Reset();

	vblank = 1;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM	= Next; Next += 0x040000;
	DrvZ80ROM	= Next; Next += 0x010000;

	DrvGfxROM0	= Next; Next += 0x040000;
	DrvGfxROM1	= Next; Next += 0x400000;
	DrvGfxROM2	= Next; Next += 0x100000;

	MSM6295ROM	= Next;
	DrvSndROM	= Next; Next += 0x040000;

	DrvPalette	= (UINT32*)Next; Next += 0x180 * sizeof(UINT32);

	AllRam		= Next;

	Drv68KRAM	= Next; Next += 0x004000;
	DrvZ80RAM	= Next; Next += 0x000800;

	DrvSprRAM	= Next; Next += 0x000400;
	DrvPalRAM	= Next; Next += 0x001000;
	DrvFgRAM	= Next; Next += 0x001000;
	DrvBgRAM	= Next; Next += 0x001000;

	soundlatch	= Next; Next += 0x000001;
	flipscreen	= Next; Next += 0x000001;

	scroll		= (UINT16*)Next; Next += 2 * sizeof(UINT16);

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[4]  = { 0x000000, 0x000002, 0x000004, 0x000006 };
	INT32 Plane1[4]  = { 0x800000, 0x800004, 0x000000, 0x000004 };
	INT32 Plane2[4]  = { 0x200000, 0x200004, 0x000000, 0x000004 };

	INT32 XOffs0[8]  = { 0x001, 0x000, 0x041, 0x040, 0x081, 0x080, 0x0c1, 0x0c0 };
	INT32 YOffs[16]  = { 0x000, 0x008, 0x010, 0x018, 0x020, 0x028, 0x030, 0x038,
			   0x040, 0x048, 0x050, 0x058, 0x060, 0x068, 0x070, 0x078 };
	INT32 XOffs1[16] = { 0x003, 0x002, 0x001, 0x000, 0x083, 0x082, 0x081, 0x080,
			   0x103, 0x102, 0x101, 0x100, 0x183, 0x182, 0x181, 0x180 };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x200000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x020000);

	GfxDecode(0x1000, 4,  8,  8, Plane0, XOffs0, YOffs, 0x100, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x200000);

	GfxDecode(0x4000, 4, 16, 16, Plane1, XOffs1, YOffs, 0x200, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x080000);

	GfxDecode(0x1000, 4, 16, 16, Plane2, XOffs1, YOffs, 0x200, tmp, DrvGfxROM2);

	BurnFree (tmp);

	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		if (strcmp(BurnDrvGetTextA(DRV_NAME), "wwfsstarb") == 0) {
			if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
			if (BurnLoadRom(Drv68KROM  + 0x020001,  1, 2)) return 1;
			if (BurnLoadRom(Drv68KROM  + 0x000000,  2, 2)) return 1;
			if (BurnLoadRom(Drv68KROM  + 0x020000,  3, 2)) return 1;

			if (BurnLoadRom(DrvZ80ROM  + 0x000000,  4, 1)) return 1;

			if (BurnLoadRom(DrvSndROM  + 0x000000,  5, 1)) return 1;
			if (BurnLoadRom(DrvSndROM  + 0x010000,  6, 1)) return 1;
			if (BurnLoadRom(DrvSndROM  + 0x020000,  7, 1)) return 1;
			if (BurnLoadRom(DrvSndROM  + 0x030000,  8, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM0 + 0x000000,  9, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x010000, 10, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM1 + 0x000000, 11, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x010000, 12, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x020000, 13, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x030000, 14, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x040000, 15, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x050000, 16, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x060000, 17, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x070000, 18, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x080000, 19, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x090000, 20, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x0a0000, 21, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x0b0000, 22, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x0c0000, 23, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x0d0000, 24, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x0e0000, 25, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x0f0000, 26, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x100000, 27, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x110000, 28, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x120000, 29, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x130000, 30, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x140000, 31, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x150000, 32, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x160000, 33, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x170000, 34, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x180000, 35, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x190000, 36, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x1a0000, 37, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x1b0000, 38, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x1c0000, 39, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x1d0000, 40, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x1e0000, 41, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x1f0000, 42, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM2 + 0x000000, 43, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x010000, 44, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x020000, 45, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x030000, 46, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x040000, 47, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x050000, 48, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x060000, 49, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x070000, 50, 1)) return 1;
		} else {
			if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
			if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;

			if (BurnLoadRom(DrvZ80ROM  + 0x000000,  2, 1)) return 1;

			if (BurnLoadRom(DrvSndROM  + 0x000000,  3, 1)) return 1;
			if (BurnLoadRom(DrvSndROM  + 0x020000,  4, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM0 + 0x000000,  5, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM1 + 0x000000,  6, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x080000,  7, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x0c0000,  8, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x100000,  9, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x180000, 10, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x1c0000, 11, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM2 + 0x000000, 12, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x040000, 13, 1)) return 1;
		}

		DrvGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(DrvFgRAM,		0x080000, 0x080fff, MAP_RAM);
	SekMapMemory(DrvBgRAM,		0x0c0000, 0x0c0fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,		0x100000, 0x1003ff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		0x140000, 0x140fff, MAP_RAM);
	SekMapMemory(Drv68KRAM,		0x1c0000, 0x1c3fff, MAP_RAM);
	SekSetReadWordHandler(0,	wwfsstar_main_read_word);
	SekSetReadByteHandler(0,	wwfsstar_main_read_byte);
	SekSetWriteWordHandler(0,	wwfsstar_main_write_word);
	SekSetWriteByteHandler(0,	wwfsstar_main_write_byte);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM, 0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM, 0x8000, 0x87ff, MAP_RAM);
	ZetSetWriteHandler(wwfsstar_sound_write);
	ZetSetReadHandler(wwfsstar_sound_read);
	ZetClose();

	BurnYM2151InitBuffered(3579545, 1, NULL, 0);
	BurnYM2151SetIrqHandler(&DrvYM2151IrqHandler);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.45, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.45, BURN_SND_ROUTE_RIGHT);
	BurnTimerAttachZet(3579545);

	MSM6295Init(0, 1056000 / 132, 1);
	MSM6295SetRoute(0, 0.47, BURN_SND_ROUTE_BOTH);

	DrvDoReset();

	GenericTilesInit();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	SekExit();
	ZetExit();

	MSM6295Exit(0);
	BurnYM2151Exit();

	BurnFree (AllMem);

	return 0;
}

static void draw_fg_layer()
{
	UINT16 *vram = (UINT16*)DrvFgRAM;

	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sx = (offs & 0x1f) << 3;
		INT32 sy = (offs >> 5) << 3;

		INT32 attr  = vram[(offs * 2) + 0];
		INT32 code  = (vram[(offs * 2) + 1] & 0xff) | ((attr & 0x0f) << 8);
		INT32 color = (attr & 0xf0) >> 4;

		if (*flipscreen) {
			sy = (sy ^ 0xf8) - 16;
			sx = sx ^ 0xf8;

			Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0, DrvGfxROM0);
		} else {
			Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0, DrvGfxROM0);
		}
	}
}

static void draw_bg_layer()
{
	UINT16 *vram = (UINT16*)DrvBgRAM;

	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 row = offs >> 5;
		INT32 col = offs & 0x1f;

		INT32 sx = col << 4;
		INT32 sy = row << 4;

		sx -= scroll[0];
		if (sx < -15) sx += 512;
		sy -= scroll[1];
		if (sy < -15) sy += 512;

		if (sx >= nScreenWidth || sy >= nScreenHeight) continue;

		INT32 ofst = (col & 0x0f) | ((row & 0x0f) << 4) | ((col & 0x10) << 4) | ((row & 0x10) << 5);

		INT32 attr  = vram[(ofst * 2) + 0];
		INT32 code  = (vram[(ofst * 2) + 1] & 0xff) | ((attr & 0x0f) << 8);
		INT32 color = (attr &  0x70) >> 4;
		INT32 flipx = attr & 0x80;

		if (*flipscreen) {
			sy = (240 - sy) - 16;
			sx = 240 - sx;

			if (flipx) {
				Render16x16Tile_FlipY_Clip(pTransDraw, code, sx, sy, color, 4, 0x100, DrvGfxROM2); // 0 transparent?
			} else {
				Render16x16Tile_FlipXY_Clip(pTransDraw, code, sx, sy, color, 4, 0x100, DrvGfxROM2); // 0 transparent?
			}
		} else {
			if (flipx) {
				Render16x16Tile_FlipX_Clip(pTransDraw, code, sx, sy, color, 4, 0x100, DrvGfxROM2); // 0 transparent?
			} else {
				Render16x16Tile_Clip(pTransDraw, code, sx, sy, color, 4, 0x100, DrvGfxROM2); // 0 transparent?
			}
		}
	}
}

static void draw_sprites()
{
	UINT16 *source = (UINT16*)DrvSprRAM;
	UINT16 *finish = source + 0x3ff / 2;

	while (source < finish)
	{
		if (source[1] & 0x0001)
		{
			INT32 ypos   = (source[0] & 0xff) | ((source[1] & 0x04) << 6);
			INT32 xpos   = (source[4] & 0xff) | ((source[1] & 0x08) << 5);
			INT32 flipx  = (source[2] & 0x80 ) >> 7;
			INT32 flipy  = (source[2] & 0x40 ) >> 6;
			INT32 chain  = (source[1] & 0x02 ) >> 1;
			INT32 number = (source[3] & 0xff) | ((source[2] & 0x3f) << 8);
			INT32 color  = ((source[1] & 0xf0) + 0x80) >> 4;

			ypos    = (((256 - ypos) & 0x1ff) - 16) ;
			xpos    = (((256 - xpos) & 0x1ff) - 16);
			chain  += 1;
			number &= ~(chain - 1);

			if (*flipscreen)
			{
				flipy = !flipy;
				flipx = !flipx;
				ypos = 240 - ypos;
				xpos = 240 - xpos;
				ypos -= 16;
			}

			for (INT32 count = 0; count < chain; count++)
			{
				INT32 yy = ypos;

				if (*flipscreen) {
					if (flipy) {
						yy += (16*(chain-1))-(16*count);
					} else {
						yy += 16 * count;
					}
				} else {
					if (flipy) {
						yy -= 16 * count;
					} else {
						yy -= (16*(chain-1))-(16*count);
					}
				}

				Draw16x16MaskTile(pTransDraw, number+count, xpos, yy, flipx, flipy, color, 4, 0, 0, DrvGfxROM1);
		   }
		}

		source+=5;
	}
}

static void DrvPaletteRecalculate()
{
	UINT16 *p = (UINT16*)DrvPalRAM;
	for (INT32 i = 0; i < 0x180; i++) {
		UINT8 r = (p[i] >> 0) & 0x0f;
		UINT8 b = (p[i] >> 8) & 0x0f;
		UINT8 g = (p[i] >> 4) & 0x0f;

		r |= r << 4;
		g |= g << 4;
		b |= b << 4;

		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}
}

static INT32 DrvDraw()
{
	DrvPaletteRecalculate();

	draw_bg_layer();

	draw_sprites();

	draw_fg_layer();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static inline void interrupt(INT32 scanline)
{
	if (scanline == 0)			vblank = 0;
	if (scanline == (240-1))	vblank = 1; // must be 1 line before vbl irq to avoid deadlock after 3rd match
	if ((scanline & 0x0f) == 0)	SekSetIRQLine(5, CPU_IRQSTATUS_ACK);
	if (scanline == 240)		SekSetIRQLine(6, CPU_IRQSTATUS_ACK);
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 3 * sizeof(INT16));
		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= DrvJoy1[i] << i;
			DrvInputs[1] ^= DrvJoy2[i] << i;
			DrvInputs[2] ^= DrvJoy3[i] << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 10000000 / 60, 3579545 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++) {
		CPU_RUN(0, Sek);
		if (i >= 8 && i <= 248) interrupt(i - 8);

		if (i == 248) { // draw at vbl
			if (pBurnDraw) {
				DrvDraw();
			}
		}

		CPU_RUN_TIMER(1);
	}

	if (pBurnSoundOut) {
		BurnYM2151Render(pBurnSoundOut, nBurnSoundLen);
		MSM6295Render(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	SekClose();

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029706;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);
		ZetScan(nAction);

		BurnYM2151Scan(nAction, pnMin);
		MSM6295Scan(nAction, pnMin);

		SCAN_VAR(vblank);
	}

	return 0;
}


// WWF Superstars (Europe)

static struct BurnRomInfo wwfsstarRomDesc[] = {
	{ "24ac-0_j-1.34",	0x20000, 0xec8fd2c9, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "24ad-0_j-1.35",	0x20000, 0x54e614e4, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "24ab-0.12",		0x08000, 0x1e44f8aa, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "24a9-0.46",		0x20000, 0x703ff08f, 3 | BRF_SND },           //  3 oki
	{ "24j8-0.45",		0x20000, 0x61138487, 3 | BRF_SND },           //  4

	{ "24aa-0.58",		0x20000, 0xcb12ba40, 4 | BRF_GRA },           //  5 Characters

	{ "c951.114",		0x80000, 0xfa76d1f0, 5 | BRF_GRA },           //  6 Sprites
	{ "24j4-0.115",		0x40000, 0xc4a589a3, 5 | BRF_GRA },           //  7
	{ "24j5-0.116",		0x40000, 0xd6bca436, 5 | BRF_GRA },           //  8
	{ "c950.117",		0x80000, 0xcca5703d, 5 | BRF_GRA },           //  9
	{ "24j2-0.118",		0x40000, 0xdc1b7600, 5 | BRF_GRA },           // 10
	{ "24j3-0.119",		0x40000, 0x3ba12d43, 5 | BRF_GRA },           // 11

	{ "24j7-0.113",		0x40000, 0xe0a1909e, 6 | BRF_GRA },           // 12 Tiles
	{ "24j6-0.112",		0x40000, 0x77932ef8, 6 | BRF_GRA },           // 13
};

STD_ROM_PICK(wwfsstar)
STD_ROM_FN(wwfsstar)

struct BurnDriver BurnDrvWwfsstar = {
	"wwfsstar", NULL, NULL, NULL, "1989",
	"WWF Superstars (Europe)\0", NULL, "Technos Japan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TECHNOS, GBF_VSFIGHT, 0,
	NULL, wwfsstarRomInfo, wwfsstarRomName, NULL, NULL, NULL, NULL, WwfsstarInputInfo, WwfsstarDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x180,
	256, 240, 4, 3
};


// WWF Superstars (US revision 7)

static struct BurnRomInfo wwfsstaru7RomDesc[] = {
	{ "24ac-06.34",		0x20000, 0x924a50e4, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "24ad-07.35",		0x20000, 0x9a76a50e, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "24ab-0.12",		0x08000, 0x1e44f8aa, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "24a9-0.46",		0x20000, 0x703ff08f, 3 | BRF_SND },           //  3 oki
	{ "24j8-0.45",		0x20000, 0x61138487, 3 | BRF_SND },           //  4

	{ "24aa-0.58",		0x20000, 0xcb12ba40, 4 | BRF_GRA },           //  5 Characters

	{ "c951.114",		0x80000, 0xfa76d1f0, 5 | BRF_GRA },           //  6 Sprites
	{ "24j4-0.115",		0x40000, 0xc4a589a3, 5 | BRF_GRA },           //  7
	{ "24j5-0.116",		0x40000, 0xd6bca436, 5 | BRF_GRA },           //  8
	{ "c950.117",		0x80000, 0xcca5703d, 5 | BRF_GRA },           //  9
	{ "24j2-0.118",		0x40000, 0xdc1b7600, 5 | BRF_GRA },           // 10
	{ "24j3-0.119",		0x40000, 0x3ba12d43, 5 | BRF_GRA },           // 11

	{ "24j7-0.113",		0x40000, 0xe0a1909e, 6 | BRF_GRA },           // 12 Tiles
	{ "24j6-0.112",		0x40000, 0x77932ef8, 6 | BRF_GRA },           // 13
};

STD_ROM_PICK(wwfsstaru7)
STD_ROM_FN(wwfsstaru7)

struct BurnDriver BurnDrvWwfsstaru7 = {
	"wwfsstaru7", "wwfsstar", NULL, NULL, "1989",
	"WWF Superstars (US revision 7)\0", NULL, "Technos Japan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TECHNOS, GBF_VSFIGHT, 0,
	NULL, wwfsstaru7RomInfo, wwfsstaru7RomName, NULL, NULL, NULL, NULL, WwfsstarInputInfo, WwfsstarDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x180,
	256, 240, 4, 3
};


// WWF Superstars (US revision 6)

static struct BurnRomInfo wwfsstaru6RomDesc[] = {
	{ "24ac-06.34",		0x20000, 0x924a50e4, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "24ad-06.35",		0x20000, 0xd32eee6d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "24ab-0.12",		0x08000, 0x1e44f8aa, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "24a9-0.46",		0x20000, 0x703ff08f, 3 | BRF_SND },           //  3 oki
	{ "24j8-0.45",		0x20000, 0x61138487, 3 | BRF_SND },           //  4

	{ "24aa-0.58",		0x20000, 0xcb12ba40, 4 | BRF_GRA },           //  5 Characters

	{ "c951.114",		0x80000, 0xfa76d1f0, 5 | BRF_GRA },           //  6 Sprites
	{ "24j4-0.115",		0x40000, 0xc4a589a3, 5 | BRF_GRA },           //  7
	{ "24j5-0.116",		0x40000, 0xd6bca436, 5 | BRF_GRA },           //  8
	{ "c950.117",		0x80000, 0xcca5703d, 5 | BRF_GRA },           //  9
	{ "24j2-0.118",		0x40000, 0xdc1b7600, 5 | BRF_GRA },           // 10
	{ "24j3-0.119",		0x40000, 0x3ba12d43, 5 | BRF_GRA },           // 11

	{ "24j7-0.113",		0x40000, 0xe0a1909e, 6 | BRF_GRA },           // 12 Tiles
	{ "24j6-0.112",		0x40000, 0x77932ef8, 6 | BRF_GRA },           // 13
};

STD_ROM_PICK(wwfsstaru6)
STD_ROM_FN(wwfsstaru6)

struct BurnDriver BurnDrvWwfsstaru6 = {
	"wwfsstaru6", "wwfsstar", NULL, NULL, "1989",
	"WWF Superstars (US revision 6)\0", NULL, "Technos Japan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TECHNOS, GBF_VSFIGHT, 0,
	NULL, wwfsstaru6RomInfo, wwfsstaru6RomName, NULL, NULL, NULL, NULL, WwfsstarInputInfo, WwfsstarDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x180,
	256, 240, 4, 3
};


// WWF Superstars (US revision 4)

static struct BurnRomInfo wwfsstaru4RomDesc[] = {
	{ "24ac-04.34",		0x20000, 0xee9b850e, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "24ad-04.35",		0x20000, 0x057c2eef, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "24ab-0.12",		0x08000, 0x1e44f8aa, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "24a9-0.46",		0x20000, 0x703ff08f, 3 | BRF_SND },           //  3 oki
	{ "24j8-0.45",		0x20000, 0x61138487, 3 | BRF_SND },           //  4

	{ "24aa-0.58",		0x20000, 0xcb12ba40, 4 | BRF_GRA },           //  5 Characters

	{ "c951.114",		0x80000, 0xfa76d1f0, 5 | BRF_GRA },           //  6 Sprites
	{ "24j4-0.115",		0x40000, 0xc4a589a3, 5 | BRF_GRA },           //  7
	{ "24j5-0.116",		0x40000, 0xd6bca436, 5 | BRF_GRA },           //  8
	{ "c950.117",		0x80000, 0xcca5703d, 5 | BRF_GRA },           //  9
	{ "24j2-0.118",		0x40000, 0xdc1b7600, 5 | BRF_GRA },           // 10
	{ "24j3-0.119",		0x40000, 0x3ba12d43, 5 | BRF_GRA },           // 11

	{ "24j7-0.113",		0x40000, 0xe0a1909e, 6 | BRF_GRA },           // 12 Tiles
	{ "24j6-0.112",		0x40000, 0x77932ef8, 6 | BRF_GRA },           // 13
};

STD_ROM_PICK(wwfsstaru4)
STD_ROM_FN(wwfsstaru4)

struct BurnDriver BurnDrvWwfsstaru4 = {
	"wwfsstaru4", "wwfsstar", NULL, NULL, "1989",
	"WWF Superstars (US revision 4)\0", NULL, "Technos Japan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TECHNOS, GBF_VSFIGHT, 0,
	NULL, wwfsstaru4RomInfo, wwfsstaru4RomName, NULL, NULL, NULL, NULL, WwfsstarInputInfo, WwfsstarDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x180,
	256, 240, 4, 3
};


// WWF Superstars (Japan)

static struct BurnRomInfo wwfsstarjRomDesc[] = {
	{ "24ac-0_j-1_japan.34",0x20000, 0xf872e968, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "24ad-0_j-1_japan.35",0x20000, 0xc70bcd23, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "24ab-0.12",		0x08000, 0x1e44f8aa, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "24a9-0.46",		0x20000, 0x703ff08f, 3 | BRF_SND },           //  3 oki
	{ "24j8-0.45",		0x20000, 0x61138487, 3 | BRF_SND },           //  4

	{ "24aa-0_j.58",	0x20000, 0xb9201b36, 4 | BRF_GRA },           //  5 Characters

	{ "c951.114",		0x80000, 0xfa76d1f0, 5 | BRF_GRA },           //  6 Sprites
	{ "24j4-0.115",		0x40000, 0xc4a589a3, 5 | BRF_GRA },           //  7
	{ "24j5-0.116",		0x40000, 0xd6bca436, 5 | BRF_GRA },           //  8
	{ "c950.117",		0x80000, 0xcca5703d, 5 | BRF_GRA },           //  9
	{ "24j2-0.118",		0x40000, 0xdc1b7600, 5 | BRF_GRA },           // 10
	{ "24j3-0.119",		0x40000, 0x3ba12d43, 5 | BRF_GRA },           // 11

	{ "24j7-0.113",		0x40000, 0xe0a1909e, 6 | BRF_GRA },           // 12 Tiles
	{ "24j6-0.112",		0x40000, 0x77932ef8, 6 | BRF_GRA },           // 13
};

STD_ROM_PICK(wwfsstarj)
STD_ROM_FN(wwfsstarj)

struct BurnDriver BurnDrvWwfsstarj = {
	"wwfsstarj", "wwfsstar", NULL, NULL, "1989",
	"WWF Superstars (Japan)\0", NULL, "Technos Japan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TECHNOS, GBF_VSFIGHT, 0,
	NULL, wwfsstarjRomInfo, wwfsstarjRomName, NULL, NULL, NULL, NULL, WwfsstarInputInfo, WwfsstarDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x180,
	256, 240, 4, 3
};


// WWF Superstars (Canada)

static struct BurnRomInfo wwfsstarcRomDesc[] = {
	{ "24ac-0c.ic34",	0x20000, 0x3e5eebf3, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "24ad-0c.ic35",	0x20000, 0x154bf54c, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "24ab-0.12",		0x08000, 0x1e44f8aa, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "24a9-0.46",		0x20000, 0x703ff08f, 3 | BRF_SND },           //  3 oki
	{ "24j8-0.45",		0x20000, 0x61138487, 3 | BRF_SND },           //  4

	{ "24aa-0.58",		0x20000, 0xcb12ba40, 4 | BRF_GRA },           //  5 Characters

	{ "c951.114",		0x80000, 0xfa76d1f0, 5 | BRF_GRA },           //  6 Sprites
	{ "24j4-0.115",		0x40000, 0xc4a589a3, 5 | BRF_GRA },           //  7
	{ "24j5-0.116",		0x40000, 0xd6bca436, 5 | BRF_GRA },           //  8
	{ "c950.117",		0x80000, 0xcca5703d, 5 | BRF_GRA },           //  9
	{ "24j2-0.118",		0x40000, 0xdc1b7600, 5 | BRF_GRA },           // 10
	{ "24j3-0.119",		0x40000, 0x3ba12d43, 5 | BRF_GRA },           // 11

	{ "24j7-0.113",		0x40000, 0xe0a1909e, 6 | BRF_GRA },           // 12 Tiles
	{ "24j6-0.112",		0x40000, 0x77932ef8, 6 | BRF_GRA },           // 13
};

STD_ROM_PICK(wwfsstarc)
STD_ROM_FN(wwfsstarc)

struct BurnDriver BurnDrvWwfsstarc = {
	"wwfsstarc", "wwfsstar", NULL, NULL, "1989",
	"WWF Superstars (Canada)\0", NULL, "Technos Japan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TECHNOS, GBF_VSFIGHT, 0,
	NULL, wwfsstarcRomInfo, wwfsstarcRomName, NULL, NULL, NULL, NULL, WwfsstarInputInfo, WwfsstarDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x180,
	256, 240, 4, 3
};


// WWF Superstars (bootleg)

static struct BurnRomInfo wwfsstarbRomDesc[] = {
	{ "wwfs08.bin",		0x10000, 0x621df265, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "wwfs10.bin",		0x10000, 0xa3382dfe, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "wwfs07.bin",		0x10000, 0x369559e6, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "wwfs09.bin",		0x10000, 0x8cbcd5aa, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "wwfs01.bin",		0x08000, 0x1e44f8aa, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "wwfs02.bin",		0x10000, 0x6e63c457, 3 | BRF_SND },           //  5 oki
	{ "wwfs04.bin",		0x10000, 0xd7018a9c, 3 | BRF_SND },           //  6
	{ "wwfs03.bin",		0x10000, 0x8a35a20e, 3 | BRF_SND },           //  7
	{ "wwfs05.bin",		0x10000, 0x6df08962, 3 | BRF_SND },           //  8

	{ "wwfs06.bin",		0x10000, 0x154ca5ce, 4 | BRF_GRA },           //  9 Characters
	{ "wwfs11.bin",		0x10000, 0x3d4684dc, 4 | BRF_GRA },           // 10

	{ "wwfs39.bin",		0x10000, 0xd807b09a, 5 | BRF_GRA },           // 11 Sprites
	{ "wwfs38.bin",		0x10000, 0xd8ea94d3, 5 | BRF_GRA },           // 12
	{ "wwfs37.bin",		0x10000, 0x5e8d7407, 5 | BRF_GRA },           // 13
	{ "wwfs36.bin",		0x10000, 0x9005e942, 5 | BRF_GRA },           // 14
	{ "wwfs43.bin",		0x10000, 0xaafc4a38, 5 | BRF_GRA },           // 15
	{ "wwfs42.bin",		0x10000, 0xe48b88fb, 5 | BRF_GRA },           // 16
	{ "wwfs41.bin",		0x10000, 0xed7f69d5, 5 | BRF_GRA },           // 17
	{ "wwfs40.bin",		0x10000, 0x4d75fd89, 5 | BRF_GRA },           // 18
	{ "wwfs19.bin",		0x10000, 0x7426d444, 5 | BRF_GRA },           // 19
	{ "wwfs18.bin",		0x10000, 0xaf11ad2a, 5 | BRF_GRA },           // 20
	{ "wwfs17.bin",		0x10000, 0xef12069f, 5 | BRF_GRA },           // 21
	{ "wwfs16.bin",		0x10000, 0x08343e7f, 5 | BRF_GRA },           // 22
	{ "wwfs15.bin",		0x10000, 0xaac5a928, 5 | BRF_GRA },           // 23
	{ "wwfs14.bin",		0x10000, 0x67eb7bea, 5 | BRF_GRA },           // 24
	{ "wwfs13.bin",		0x10000, 0x970b6e76, 5 | BRF_GRA },           // 25
	{ "wwfs12.bin",		0x10000, 0x242caff5, 5 | BRF_GRA },           // 26
	{ "wwfs27.bin",		0x10000, 0xf3eb8ab9, 5 | BRF_GRA },           // 27
	{ "wwfs26.bin",		0x10000, 0x2ca91eaf, 5 | BRF_GRA },           // 28
	{ "wwfs25.bin",		0x10000, 0xbbf69c6a, 5 | BRF_GRA },           // 29
	{ "wwfs24.bin",		0x10000, 0x76b08bcd, 5 | BRF_GRA },           // 30
	{ "wwfs23.bin",		0x10000, 0x681f5b5e, 5 | BRF_GRA },           // 31
	{ "wwfs22.bin",		0x10000, 0x81fe1bf7, 5 | BRF_GRA },           // 32
	{ "wwfs21.bin",		0x10000, 0xc52eee5e, 5 | BRF_GRA },           // 33
	{ "wwfs20.bin",		0x10000, 0xb2a8050e, 5 | BRF_GRA },           // 34
	{ "wwfs35.bin",		0x10000, 0x9d648d82, 5 | BRF_GRA },           // 35
	{ "wwfs34.bin",		0x10000, 0x742a79db, 5 | BRF_GRA },           // 36
	{ "wwfs33.bin",		0x10000, 0xf6923db6, 5 | BRF_GRA },           // 37
	{ "wwfs32.bin",		0x10000, 0x9becd621, 5 | BRF_GRA },           // 38
	{ "wwfs31.bin",		0x10000, 0xf94c74d5, 5 | BRF_GRA },           // 39
	{ "wwfs30.bin",		0x10000, 0x94094518, 5 | BRF_GRA },           // 40
	{ "wwfs29.bin",		0x10000, 0x7b5b9d83, 5 | BRF_GRA },           // 41
	{ "wwfs28.bin",		0x10000, 0x70fda626, 5 | BRF_GRA },           // 42

	{ "wwfs51.bin",		0x10000, 0x51157385, 6 | BRF_GRA },           // 43 Tiles
	{ "wwfs50.bin",		0x10000, 0x7fc79df5, 6 | BRF_GRA },           // 44
	{ "wwfs49.bin",		0x10000, 0xa14076b0, 6 | BRF_GRA },           // 45
	{ "wwfs48.bin",		0x10000, 0x251372fd, 6 | BRF_GRA },           // 46
	{ "wwfs47.bin",		0x10000, 0x6fd7b6ea, 6 | BRF_GRA },           // 47
	{ "wwfs46.bin",		0x10000, 0x985e5180, 6 | BRF_GRA },           // 48
	{ "wwfs45.bin",		0x10000, 0xb2fad792, 6 | BRF_GRA },           // 49
	{ "wwfs44.bin",		0x10000, 0x4f965fa9, 6 | BRF_GRA },           // 50
};

STD_ROM_PICK(wwfsstarb)
STD_ROM_FN(wwfsstarb)

struct BurnDriver BurnDrvWwfsstarb = {
	"wwfsstarb", "wwfsstar", NULL, NULL, "1989",
	"WWF Superstars (bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_TECHNOS, GBF_VSFIGHT, 0,
	NULL, wwfsstarbRomInfo, wwfsstarbRomName, NULL, NULL, NULL, NULL, WwfsstarInputInfo, WwfsstarDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x180,
	256, 240, 4, 3
};
