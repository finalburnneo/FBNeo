// FinalBurn Neo Splash hardware driver module
// Based on MAME driver by Manuel Abadia, David Haywood

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "burn_ym2203.h"
#include "burn_ym3812.h"
#include "msm5205.h"
#include "burn_pal.h"
#include "bitswap.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM[2];
static UINT8 *DrvZ80RAM;
static UINT8 *Drv68KRAM[3];
static UINT8 *DrvSpriteRAM;
static UINT8 *DrvVideoRAM;
static UINT8 *DrvPixelRAM;
static UINT16 *DrvBitmapSwap[8];

static UINT8 adpcm_data;
static INT32 sound_irq;
static INT32 vblank_irq;
static UINT16 bitmap_mode;
static INT32 flipflop;
static UINT8 soundlatch;
static UINT8 soundbank;

static INT32 splash = 0;
static INT32 is_nsplash = 0;
static INT32 sprite_attr2_shift = 8;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static struct BurnInputInfo SplashInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Splash)

static struct BurnDIPInfo SplashDIPList[]=
{
	DIP_OFFSET(0x11)
	{0x00, 0xff, 0xff, 0xff, NULL									},
	{0x01, 0xff, 0xff, 0xff, NULL									},

	{0   , 0xfe, 0   ,    12, "Coin A"								},
	{0x00, 0x01, 0x0f, 0x06, "5 Coins 1 Credits"					},
	{0x00, 0x01, 0x0f, 0x07, "4 Coins 1 Credits"					},
	{0x00, 0x01, 0x0f, 0x08, "3 Coins 1 Credits"					},
	{0x00, 0x01, 0x0f, 0x09, "2 Coins 1 Credits"					},
	{0x00, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"					},
	{0x00, 0x01, 0x0f, 0x05, "2 Coins 3 Credits"					},
	{0x00, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"					},
	{0x00, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"					},
	{0x00, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"					},
	{0x00, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"					},
	{0x00, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"					},
	{0x00, 0x01, 0x0f, 0x00, "1C/1C or Free Play (if Coin B too)"	},

	{0   , 0xfe, 0   ,    12, "Coin B"								},
	{0x00, 0x01, 0xf0, 0x60, "5 Coins 1 Credits"					},
	{0x00, 0x01, 0xf0, 0x70, "4 Coins 1 Credits"					},
	{0x00, 0x01, 0xf0, 0x80, "3 Coins 1 Credits"					},
	{0x00, 0x01, 0xf0, 0x90, "2 Coins 1 Credits"					},
	{0x00, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"					},
	{0x00, 0x01, 0xf0, 0x50, "2 Coins 3 Credits"					},
	{0x00, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"					},
	{0x00, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"					},
	{0x00, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"					},
	{0x00, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"					},
	{0x00, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"					},
	{0x00, 0x01, 0xf0, 0x00, "1C/1C or Free Play (if Coin A too)"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"							},
	{0x01, 0x01, 0x03, 0x02, "Easy"									},
	{0x01, 0x01, 0x03, 0x03, "Normal"								},
	{0x01, 0x01, 0x03, 0x01, "Hard"									},
	{0x01, 0x01, 0x03, 0x00, "Hardest"								},

	{0   , 0xfe, 0   ,    3, "Lives"								},
	{0x01, 0x01, 0x0c, 0x08, "1"									},
	{0x01, 0x01, 0x0c, 0x04, "2"									},
	{0x01, 0x01, 0x0c, 0x0c, "3"									},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"							},
	{0x01, 0x01, 0x10, 0x00, "Off"									},
	{0x01, 0x01, 0x10, 0x10, "On"									},

	{0   , 0xfe, 0   ,    2, "Girls"								},
	{0x01, 0x01, 0x20, 0x00, "Light"								},
	{0x01, 0x01, 0x20, 0x20, "Hard"									},

	{0   , 0xfe, 0   ,    2, "Paint Mode"							},
	{0x01, 0x01, 0x40, 0x00, "Paint again"							},
	{0x01, 0x01, 0x40, 0x40, "Normal"								},

	{0   , 0xfe, 0   ,    2, "Service Mode"							},
	{0x01, 0x01, 0x80, 0x00, "On"									},
	{0x01, 0x01, 0x80, 0x80, "Off"									},
};

STDDIPINFO(Splash)

static void __fastcall roldfrog_write_word(UINT32 address, UINT16 data)
{
	if (address < 0x400000) return; // ?

	if ((address & 0xfffff8f) == 0x84000a) {
		// int offset = (address >> 4) & 7;
		// data &= 1;
		// coin lockout & coin counter
		return;
	}

	switch (address)
	{
		case 0xe00000:
			bitmap_mode = data;
		return;

		case 0x84000e:
			soundlatch = data;
			if (!splash) ZetSetIRQLine(0x20, CPU_IRQSTATUS_ACK);
			else ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		return;
	}

	bprintf (0, _T("WW: %5.5x, %4.4x PC(%5.5x)\n"), address, data, SekGetPC(-1));
}

static void __fastcall roldfrog_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xfffff8f) == 0x84000a) {
		// int offset = (address >> 4) & 7;
		// data &= 1;
		// coin lockout & coin counter
		return;
	}

	switch (address)
	{
		case 0x84000f:
			soundlatch = data;
			if (!splash) ZetSetIRQLine(0x20, CPU_IRQSTATUS_ACK);
			else ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		return;

		case 0xe00000:
		case 0xe00001:
			bitmap_mode = data;
		return;
	}

	bprintf (0, _T("WB: %5.5x, %2.2x PC(%5.5x)\n"), address, data, SekGetPC(-1));
}

static UINT16 __fastcall roldfrog_read_word(UINT32 address)
{

//	bprintf (0, _T("RW: %5.5x PC(%5.5x)\n"), address, SekGetPC(-1));

	switch (address)
	{
		case 0x840000:
		case 0x840002:
			return DrvDips[(address / 2) & 1];

		case 0x840004:
		case 0x840006:
			return DrvInputs[(address / 2) & 1];

		case 0xa00000:
			flipflop ^= 0x0100;
			return flipflop;
	}
	return 0;
}

static UINT8 __fastcall roldfrog_read_byte(UINT32 address)
{
	switch (address & ~1)
	{
		case 0x840000:
		case 0x840002:
			return DrvDips[(address / 2) & 1];

		case 0x840004:
		case 0x840006:
			return DrvInputs[(address / 2) & 1];

		case 0xa00000:
			flipflop ^= 0x0100;
			return flipflop >> 8;
	}
	return 0;
}

static void sound_bankswitch(INT32 data)
{
	INT32 bank = soundbank = data & 0xf;
	if (bank < 2) bank = 0x08000 + (bank * 0x8000);
	else if (bank == 5) bank = 0x18000;
	else return;

	ZetMapMemory(DrvZ80ROM + bank, 0x8000, 0xffff, MAP_ROM);
}

static void roldfrog_update_irq()
{
	INT32 irq = (sound_irq ? 0x08 : 0) | ((vblank_irq) ? 0x18 : 0);

	ZetSetVector(0xc7 | irq);
	ZetSetIRQLine(0, irq ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static void __fastcall roldfrog_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x10:
		case 0x11:
			BurnYM2203Write(0, port & 1, data);
		return;

		case 0x31:
			sound_bankswitch(data);
		return;

		case 0x37:
			vblank_irq = 0;
			roldfrog_update_irq();
		return;

		case 0x40:
			ZetSetIRQLine(0x20, CPU_IRQSTATUS_NONE);
		return;
	}
}

static UINT8 __fastcall roldfrog_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x10:
		case 0x11:
			return BurnYM2203Read(0, port & 1);

		case 0x70: {
			UINT8 sl = soundlatch;
			soundlatch = 0;
			return sl;
		}

		case 0x20:
		case 0x21:
		case 0x22:
		case 0x23:
			return 0xff;
	}

	return 0;
}

static void __fastcall splash_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xd800:
			adpcm_data = data;
		return;

		case 0xe000:
			MSM5205ResetWrite(0, ~data & 0x1);
		return;

		case 0xf000:
		case 0xf001:
			BurnYM3812Write(0, address & 1, data);
		return;
	}
}

static UINT8 __fastcall splash_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xe800:
			return soundlatch;

		case 0xf000:
		case 0xf001:
			return BurnYM3812Read(0, address & 1);
	}

	return 0;
}

static void splash_adpcm_vck()
{
	MSM5205DataWrite(0, adpcm_data >> 4);
	adpcm_data <<= 4;
}

static INT32 DrvMSM5205SynchroniseStream(INT32 nSoundRate)
{
	return (INT64)ZetTotalCycles() * nSoundRate / 3750000;
}

static void DrvYM2203IRQHandler(INT32, INT32 nStatus)
{
	sound_irq = nStatus;
	roldfrog_update_irq();
}

static tilemap_callback( bg0 )
{
	INT32 attr = DrvVideoRAM[offs * 2 + 1];
	INT32 code = DrvVideoRAM[offs * 2 + 0];

	TILE_SET_INFO(0, code | 0x2000 | ((attr & 0xf) << 8), attr >> 4, 0);
}

static tilemap_callback( bg1 )
{
	INT32 attr = DrvVideoRAM[0x1000 + offs * 2 + 1];
	INT32 code = DrvVideoRAM[0x1000 + offs * 2 + 0];

	TILE_SET_INFO(1, (code >> 2) | 0xc00 | ((attr & 0xf) << 6), attr >> 4, TILE_FLIPXY(code & 3));
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	ZetOpen(0);
	ZetReset();
	if (splash) {
		BurnYM3812Reset();
		MSM5205Reset();
	} else {
		BurnYM2203Reset();
	}
	ZetClose();

	adpcm_data = 0;
	sound_irq = 0;
	vblank_irq = 0;
	bitmap_mode = 0;
	flipflop = 0x0100;
	soundlatch = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM				= Next; Next += 0x500000;
	DrvZ80ROM				= Next; Next += 0x020000;

	DrvGfxROM[0]			= Next; Next += 0x100000;
	DrvGfxROM[1]			= Next; Next += 0x100000;

	for (INT32 i = 0; i < 8; i++) {
		DrvBitmapSwap[i]	= (UINT16*)Next; Next += 0x000200;
	}

	BurnPalette				= (UINT32*)Next; Next += 0x0400 * sizeof(UINT32);

	AllRam					= Next;

	DrvZ80RAM				= Next; Next += 0x001000;

	Drv68KRAM[0]			= Next; Next += 0x000800;
	Drv68KRAM[1]			= Next; Next += 0x000800;
	Drv68KRAM[2]			= Next; Next += 0x004000;
	DrvSpriteRAM			= Next; Next += 0x001000;
	BurnPalRAM				= Next; Next += 0x001000;

	DrvVideoRAM				= Next; Next += 0x001800;
	DrvPixelRAM				= Next; Next += 0x040000;

	RamEnd					= Next;

	MemEnd					= Next;

	return 0;
}

static void BuildBitmapSwapTable(INT32 is_splash)
{
	for (INT32 i = 0; i < 0x100; i++)
	{
		DrvBitmapSwap[0][i] = 0x300 ^ i ^ (is_splash ? 0 : 0x7f);
		DrvBitmapSwap[1][i] = 0x300 ^ BITSWAP08(i,7,0,1,2,3,4,5,6);
		DrvBitmapSwap[2][i] = 0x300 ^ i ^ 0x55;
		DrvBitmapSwap[3][i] = 0x300 ^ BITSWAP08(i,7,4,6,5,1,0,3,2) ^ 0x7f;
		DrvBitmapSwap[4][i] = 0x300 ^ BITSWAP08(i,7,3,2,1,0,6,5,4);
		DrvBitmapSwap[5][i] = 0x300 ^ BITSWAP08(i,7,6,4,2,0,5,3,1);
		DrvBitmapSwap[6][i] = 0x300 ^ BITSWAP08(i,7,0,6,5,4,3,2,1) ^ 0x7f;
		DrvBitmapSwap[7][i] = 0x300 ^ BITSWAP08(i,7,4,3,2,1,0,6,5) ^ 0x55;
	}
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[4]  = { STEP4(0,0x20000*8) };
	INT32 XOffs[16] = { STEP8(0,1), STEP8(128,1) };
	INT32 YOffs[16] = { STEP16(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x80000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM[0], 0x80000);

	GfxDecode(0x4000, 4,  8,  8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM[0]);
	GfxDecode(0x1000, 4, 16, 16, Plane, XOffs, YOffs, 0x100, tmp, DrvGfxROM[1]);

	BurnFree (tmp);

	return 0;
}

static INT32 SplashInit()
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;

		if (BurnLoadRom(Drv68KROM    + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM    + 0x000000, k++, 2)) return 1;

		if (!is_nsplash) {
			if (BurnLoadRom(Drv68KROM    + 0x100001, k++, 2)) return 1;
			if (BurnLoadRom(Drv68KROM    + 0x100000, k++, 2)) return 1;
		}

		if (BurnLoadRom(Drv68KROM    + 0x200001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM    + 0x200000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM    + 0x300001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM    + 0x300000, k++, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM    + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[0] + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x020000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x040000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x060000, k++, 1)) return 1;

		DrvGfxDecode();
		BuildBitmapSwapTable(1);
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x407fff, MAP_ROM);
	SekMapMemory(Drv68KRAM[0],	0x408000, 0x4087ff, MAP_RAM); // ?
	SekMapMemory(DrvPixelRAM,	0x800000, 0x83ffff, MAP_RAM);
	SekMapMemory(DrvVideoRAM,	0x880000, 0x8817ff, MAP_RAM);
	SekMapMemory(Drv68KRAM[1],	0x881800, 0x881fff, MAP_RAM); // scroll regs at 881800-881803
	SekMapMemory(BurnPalRAM,	0x8c0000, 0x8c0fff, MAP_RAM); /* Palette is xRRRRxGGGGxBBBBx */
	SekMapMemory(DrvSpriteRAM,	0x900000, 0x900fff, MAP_RAM); // splash
//	SekMapMemory(DrvSpriteRAM,	0xd00000, 0xd00fff, MAP_RAM);
	SekMapMemory(Drv68KRAM[2],	0xffc000, 0xffffff, MAP_RAM);
	SekSetWriteWordHandler(0,	roldfrog_write_word);
	SekSetWriteByteHandler(0,	roldfrog_write_byte);
	SekSetReadWordHandler(0,	roldfrog_read_word);
	SekSetReadByteHandler(0,	roldfrog_read_byte);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0xd7ff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,		0xf800, 0xffff, MAP_RAM);
	ZetSetWriteHandler(splash_sound_write);
	ZetSetReadHandler(splash_sound_read);
	ZetClose();

	BurnYM3812Init(1, 3750000, NULL, 0);
	BurnTimerAttach(&ZetConfig, 3750000);
	BurnYM3812SetRoute(0, BURN_SND_YM3812_ROUTE, 0.80, BURN_SND_ROUTE_BOTH);

	MSM5205Init(0, DrvMSM5205SynchroniseStream, 384000, splash_adpcm_vck, MSM5205_S48_4B, 1);
	MSM5205SetRoute(0, 0.80, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg0_map_callback,   8, 8, 64, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, bg1_map_callback, 16, 16, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 4,  8,  8, 0x100000, 0x000, 0x0f);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 4, 16, 16, 0x100000, 0x000, 0x0f);
	GenericTilemapSetGfx(2, DrvGfxROM[1], 4, 16, 16, 0x100000, 0x100, 0x0f);
	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetOffsets(0, -20, -16);
	GenericTilemapSetOffsets(1, -16, -16);

	splash = 1;

	DrvDoReset();

	return 0;
}

static INT32 RoldfrogInit()
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;

		if (BurnLoadRom(Drv68KROM    + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM    + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM    + 0x100001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM    + 0x100000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM    + 0x200001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM    + 0x200000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM    + 0x300001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM    + 0x300000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM    + 0x400000, k++, 1)) return 1; // byteswap?

		if (BurnLoadRom(DrvZ80ROM    + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[0] + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x020000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x040000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x060000, k++, 1)) return 1;

		DrvGfxDecode();
		BuildBitmapSwapTable(0);
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x407fff, MAP_ROM);
	SekMapMemory(Drv68KRAM[0],	0x408000, 0x4087ff, MAP_RAM); // ?
	SekMapMemory(DrvPixelRAM,	0x800000, 0x83ffff, MAP_RAM);
	SekMapMemory(DrvVideoRAM,	0x880000, 0x8817ff, MAP_RAM);
	SekMapMemory(Drv68KRAM[1],	0x881800, 0x881fff, MAP_RAM); // scroll regs at 881800-881803
	SekMapMemory(BurnPalRAM,	0x8c0000, 0x8c0fff, MAP_RAM); // Palette is xRRRRxGGGGxBBBBx
//	SekMapMemory(DrvSpriteRAM,	0x900000, 0x900fff, MAP_RAM); // splash
	SekMapMemory(DrvSpriteRAM,	0xd00000, 0xd00fff, MAP_RAM);
	SekMapMemory(Drv68KRAM[2],	0xffc000, 0xffffff, MAP_RAM);
	SekSetWriteWordHandler(0,	roldfrog_write_word);
	SekSetWriteByteHandler(0,	roldfrog_write_byte);
	SekSetReadWordHandler(0,	roldfrog_read_word);
	SekSetReadByteHandler(0,	roldfrog_read_byte);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x6fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,		0x7000, 0x7fff, MAP_RAM);
//	ZetMapMemory(DrvZ80ROM + 0x8000,	0x8000, 0xffff, MAP_ROM);
	ZetSetOutHandler(roldfrog_sound_write_port);
	ZetSetInHandler(roldfrog_sound_read_port);
	ZetClose();

	BurnYM2203Init(1, 3000000, &DrvYM2203IRQHandler, 0);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE,   0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.25, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.25, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.25, BURN_SND_ROUTE_BOTH);
	BurnTimerAttach(&ZetConfig, 3000000);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg0_map_callback,  8, 8, 64, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, bg1_map_callback, 16, 16, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 4,  8,  8, 0x100000, 0x000, 0x0f);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 4, 16, 16, 0x100000, 0x000, 0x0f);
	GenericTilemapSetGfx(2, DrvGfxROM[1], 4, 16, 16, 0x100000, 0x100, 0x0f);
	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetOffsets(0, -20, -16);
	GenericTilemapSetOffsets(1, -16, -16);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	SekExit();
	ZetExit();

	if (splash) {
		BurnYM3812Exit();
		MSM5205Exit();
	} else {
		BurnYM2203Exit();
	}

	GenericTilesExit();

	BurnFreeMemIndex();

	splash = 0;
	is_nsplash = 0;
	sprite_attr2_shift = 8;

	return 0;
}

static void draw_sprites()
{
	UINT16 *ram = (UINT16*)DrvSpriteRAM;

	for (INT32 i = 0x3fc; i >= 0; i-=4)
	{
		INT32 attr = ram[i+3] & 0xff;
		INT32 attr2 = ram[i+0x400] >> sprite_attr2_shift;
		INT32 sx = (ram[i+2] & 0xff) | ((attr2 & 0x80) << 1);
		INT32 sy = (240 - (ram[i+1] & 0xff)) & 0xff;
		INT32 code = (ram[i] & 0xff) + ((attr & 0xf) << 8);
		INT32 color = attr2 & 0x0f;
		INT32 flipx = attr & 0x40;
		INT32 flipy = attr & 0x80;

		DrawGfxMaskTile(0, 2, code, sx - 24, sy - 16, flipx, flipy, color, 0);
	}
}

static void draw_bitmap()
{
	UINT16 *swap = DrvBitmapSwap[(bitmap_mode >> 8) & 7];

	for (INT32 sy = 16; sy < 16 + nScreenHeight; sy++)
	{
		for (INT32 sx = 24; sx < nScreenWidth + 24; sx++)
		{
			pTransDraw[(sy - 16) * nScreenWidth + (sx - 24)] = swap[DrvPixelRAM[((sy * 512) + sx) * 2]];
		}
	}
}

static INT32 DrvDraw()
{
	if (BurnRecalc) {
		BurnPaletteUpdate_xRRRRRGGGGGBBBBB();
		BurnRecalc = 1;
	}

	BurnTransferClear();

	UINT16 *ram = (UINT16*)Drv68KRAM[1];

	if (nBurnLayer & 1) draw_bitmap();
	else BurnTransferClear();

	GenericTilemapSetScrollY(0, ram[0]);
	GenericTilemapSetScrollY(1, ram[1]);
	if (nBurnLayer & 2) GenericTilemapDraw(1, 0, 0);
	if (nSpriteEnable & 1) draw_sprites();
	if (nBurnLayer & 4) GenericTilemapDraw(0, 0, 0);

	BurnTransferCopy(BurnPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	SekNewFrame();
	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 12000000 / 60, (splash ? 3750000 : 3000000) / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	if (splash) MSM5205NewFrame(0, 3750000, nInterleave);

	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);
		CPU_RUN_TIMER(1);

		if ((i & 0x3) == 0x0 && splash) ZetNmi();

		if (i == nInterleave - 1) {
			SekSetIRQLine(6, CPU_IRQSTATUS_AUTO);
			vblank_irq = 1;
			if (splash == 0) roldfrog_update_irq();
		}

		if (splash) MSM5205UpdateScanline(i);
	}

	if (pBurnSoundOut) {
		if (splash) {
			BurnYM3812Update(pBurnSoundOut, nBurnSoundLen);
			MSM5205Render(0, pBurnSoundOut, nBurnSoundLen);
		} else {
			BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
		}
		BurnSoundDCFilter();
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

	if (nAction & ACB_MEMORY_ROM) {
		ba.Data		= Drv68KROM;
		ba.nLen		= 0x400000;
		ba.nAddress	= 0;
		ba.szName	= "68K ROM";
		BurnAcb(&ba);

		ba.Data		= Drv68KROM + 0x400000;
		ba.nLen		= 0x0008000;
		ba.nAddress	= 0x400000;
		ba.szName	= "68K ROM-2";
		BurnAcb(&ba);
	}

	if (nAction & ACB_MEMORY_RAM) {
		ba.Data			= Drv68KRAM[0];
		ba.nLen			= 0x000800;
		ba.nAddress		= 0x408000;
		ba.szName		= "68K RAM 0";
		BurnAcb(&ba);

		ba.Data			= DrvPixelRAM;
		ba.nLen			= 0x040000;
		ba.nAddress		= 0x800000;
		ba.szName		= "Pixel RAM";
		BurnAcb(&ba);

		ba.Data			= DrvVideoRAM;
		ba.nLen			= 0x001800;
		ba.nAddress		= 0x880000;
		ba.szName		= "Video RAM";
		BurnAcb(&ba);

		ba.Data			= Drv68KRAM[1];
		ba.nLen			= 0x000800;
		ba.nAddress		= 0x881800;
		ba.szName		= "68K RAM 1";
		BurnAcb(&ba);

		ba.Data			= BurnPalRAM;
		ba.nLen			= 0x001000;
		ba.nAddress		= 0x8c0000;
		ba.szName		= "Palette RAM";
		BurnAcb(&ba);

		ba.Data			= DrvSpriteRAM;
		ba.nLen			= 0x001000;
		ba.nAddress		= 0x900000;
		ba.szName		= "Sprite RAM";
		BurnAcb(&ba);

		ba.Data			= Drv68KRAM[2];
		ba.nLen			= 0x004000;
		ba.nAddress		= 0xffc000;
		ba.szName		= "68K RAM 2";
		BurnAcb(&ba);

		ba.Data			= DrvZ80RAM;
		ba.nLen			= 0x001000;
		ba.nAddress		= 0xff8000;
		ba.szName		= "Z80 RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_VOLATILE) {

		SekScan(nAction);
		ZetScan(nAction);

		if (splash) {
			BurnYM3812Scan(nAction, pnMin);
			MSM5205Scan(nAction, pnMin);
		} else {
			BurnYM2203Scan(nAction, pnMin);
		}

		SCAN_VAR(adpcm_data);
		SCAN_VAR(sound_irq);
		SCAN_VAR(vblank_irq);
		SCAN_VAR(bitmap_mode);
		SCAN_VAR(flipflop);
		SCAN_VAR(soundlatch);

		SCAN_VAR(soundbank);
	}

	if (nAction & ACB_WRITE) {
		if (splash == 0) {
			ZetOpen(0);
			sound_bankswitch(soundbank);
			ZetClose();
		}
	}

	return 0;
}

// Splash! (ver. 1.3, checksum E7BEF3FA, World)

static struct BurnRomInfo splashRomDesc[] = {
	{ "splash_2_no_fbi_no_eeuu_27c010.bin", 0x20000, 0x5d58ce67, 1 | BRF_PRG | BRF_ESS },    //  0 68K Code
	{ "splash_6_no_fbi_no_eeuu_27c010.bin", 0x20000, 0x45f83601, 1 | BRF_PRG | BRF_ESS },    //  1
	{ "splash_3.g5",                        0x80000, 0xa4e8ed18, 1 | BRF_PRG | BRF_ESS },    //  2
	{ "splash_7.i5",                        0x80000, 0x73e1154d, 1 | BRF_PRG | BRF_ESS },    //  3
	{ "splash_4.g6",                        0x80000, 0xffd56771, 1 | BRF_PRG | BRF_ESS },    //  4
	{ "splash_8.i6",                        0x80000, 0x16e9170c, 1 | BRF_PRG | BRF_ESS },    //  5
	{ "splash_5.g8",                        0x80000, 0xdc3a3172, 1 | BRF_PRG | BRF_ESS },    //  6
	{ "splash_9.i8",                        0x80000, 0x2e23e6c3, 1 | BRF_PRG | BRF_ESS },    //  7

	{ "splash_1.c5",                        0x10000, 0x0ed7ebc9, 2 | BRF_PRG | BRF_ESS },    //  8 Z80 Code

	{ "splash_13.i17",                      0x20000, 0x028a4a68, 3 | BRF_GRA },              //  9 Graphics
	{ "splash_11.i14",                      0x20000, 0x2a8cb830, 3 | BRF_GRA },              // 10
	{ "splash_12.i16",                      0x20000, 0x21aeff2c, 3 | BRF_GRA },              // 11
	{ "splash_10.i13",                      0x20000, 0xfebb9893, 3 | BRF_GRA },              // 12

	{ "p_a1020a-pl84c.g14",                 0x00200, 0x00000000, 4 | BRF_NODUMP | BRF_OPT }, // 13 PLDs
	{ "1_gal16v8a-25lp.c13",                0x00117, 0x00000000, 4 | BRF_NODUMP | BRF_OPT }, // 14
	{ "2_gal16v8a-25lp.d5",                 0x00117, 0x00000000, 4 | BRF_NODUMP | BRF_OPT }, // 15
	{ "3_gal20v8a-25lp.f4",                 0x00157, 0x00000000, 4 | BRF_NODUMP | BRF_OPT }, // 16
};

STD_ROM_PICK(splash)
STD_ROM_FN(splash)

struct BurnDriver BurnDrvSplash = {
	"splash", NULL, NULL, NULL, "1992",
	"Splash! (ver. 1.3, checksum E7BEF3FA, World)\0", NULL, "Gaelco / OMK Software", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, splashRomInfo, splashRomName, NULL, NULL, NULL, NULL, SplashInputInfo, SplashDIPInfo,
	SplashInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x400,
	368, 240, 4, 3
};


// Splash! (ver. 1.3, checksum E7BEEEFA, North America)

static struct BurnRomInfo splashnaRomDesc[] = {
	{ "fbi_eeuu_2_splash_27c010.bin", 0x20000, 0x6350027d, 1 | BRF_PRG | BRF_ESS },    //  0 68K Code
	{ "fbi_eeuu_6_splash_27c010.bin", 0x20000, 0x45f83601, 1 | BRF_PRG | BRF_ESS },    //  1
	{ "splash_3.g5",                  0x80000, 0xa4e8ed18, 1 | BRF_PRG | BRF_ESS },    //  2
	{ "splash_7.i5",                  0x80000, 0x73e1154d, 1 | BRF_PRG | BRF_ESS },    //  3
	{ "splash_4.g6",                  0x80000, 0xffd56771, 1 | BRF_PRG | BRF_ESS },    //  4
	{ "splash_8.i6",                  0x80000, 0x16e9170c, 1 | BRF_PRG | BRF_ESS },    //  5
	{ "splash_5.g8",                  0x80000, 0xdc3a3172, 1 | BRF_PRG | BRF_ESS },    //  6
	{ "splash_9.i8",                  0x80000, 0x2e23e6c3, 1 | BRF_PRG | BRF_ESS },    //  7

	{ "splash_1.c5",                  0x10000, 0x0ed7ebc9, 2 | BRF_PRG | BRF_ESS },    //  8 Z80 Code

	{ "splash_13.i17",                0x20000, 0x028a4a68, 3 | BRF_GRA },              //  9 Graphics
	{ "splash_11.i14",                0x20000, 0x2a8cb830, 3 | BRF_GRA },              // 10
	{ "splash_12.i16",                0x20000, 0x21aeff2c, 3 | BRF_GRA },              // 11
	{ "splash_10.i13",                0x20000, 0xfebb9893, 3 | BRF_GRA },              // 12

	{ "p_a1020a-pl84c.g14",           0x00200, 0x00000000, 4 | BRF_NODUMP | BRF_OPT }, // 13 PLDs
	{ "1_gal16v8a-25lp.c13",          0x00117, 0x00000000, 4 | BRF_NODUMP | BRF_OPT }, // 14
	{ "2_gal16v8a-25lp.d5",           0x00117, 0x00000000, 4 | BRF_NODUMP | BRF_OPT }, // 15
	{ "3_gal20v8a-25lp.f4",           0x00157, 0x00000000, 4 | BRF_NODUMP | BRF_OPT }, // 16
};

STD_ROM_PICK(splashna)
STD_ROM_FN(splashna)

struct BurnDriver BurnDrvSplashna = {
	"splashna", "splash", NULL, NULL, "1992",
	"Splash! (ver. 1.3, checksum E7BEEEFA, North America)\0", NULL, "Gaelco / OMK Software", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, splashnaRomInfo, splashnaRomName, NULL, NULL, NULL, NULL, SplashInputInfo, SplashDIPInfo,
	SplashInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x400,
	368, 240, 4, 3
};


// Painted Lady (ver. 1.3, checksum E7BEEEFA, North America)

static struct BurnRomInfo paintladRomDesc[] = {
	{ "2.4g",              0x20000, 0xcd00864a, 1 | BRF_PRG | BRF_ESS },    //  0 68K Code
	{ "6.4i",              0x20000, 0x0f19d830, 1 | BRF_PRG | BRF_ESS },    //  1
	{ "5g",                0x80000, 0xa4e8ed18, 1 | BRF_PRG | BRF_ESS },    //  2
	{ "5i",                0x80000, 0x73e1154d, 1 | BRF_PRG | BRF_ESS },    //  3
	{ "6g",                0x80000, 0xffd56771, 1 | BRF_PRG | BRF_ESS },    //  4
	{ "6i",                0x80000, 0x16e9170c, 1 | BRF_PRG | BRF_ESS },    //  5
	{ "8g",                0x80000, 0xdc3a3172, 1 | BRF_PRG | BRF_ESS },    //  6
	{ "8i",                0x80000, 0x2e23e6c3, 1 | BRF_PRG | BRF_ESS },    //  7

	{ "5c",                0x10000, 0x0ed7ebc9, 2 | BRF_PRG | BRF_ESS },    //  8 Z80 Code

	{ "13.18i",            0x20000, 0x262ee31f, 3 | BRF_GRA },              //  9 Graphics
	{ "11.15i",            0x20000, 0x6e4d598f, 3 | BRF_GRA },              // 10
	{ "12.16i",            0x20000, 0x15761eb5, 3 | BRF_GRA },              // 11
	{ "10.13i",            0x20000, 0x92a0eff8, 3 | BRF_GRA },              // 12

	{ "a1020a-pl84c.g14",  0x00200, 0x00000000, 4 | BRF_NODUMP | BRF_OPT }, // 13 PLDs
	{ "gal16v8a-25lp.c13", 0x00117, 0x00000000, 4 | BRF_NODUMP | BRF_OPT }, // 14
	{ "gal16v8a-25lp.d5",  0x00117, 0x00000000, 4 | BRF_NODUMP | BRF_OPT }, // 15
	{ "gal20v8a-25lp.f4",  0x00157, 0x00000000, 4 | BRF_NODUMP | BRF_OPT }, // 16
};

STD_ROM_PICK(paintlad)
STD_ROM_FN(paintlad)

struct BurnDriver BurnDrvPaintlad = {
	"paintlad", "splash", NULL, NULL, "1992",
	"Painted Lady (ver. 1.3, checksum E7BEEEFA, North America)\0", NULL, "Gaelco / OMK Software", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, paintladRomInfo, paintladRomName, NULL, NULL, NULL, NULL, SplashInputInfo, SplashDIPInfo,
	SplashInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x400,
	368, 240, 4, 3
};


// New Splash (ver. 1.4, checksum A26032A3, Korea, set 1)

static struct BurnRomInfo nsplashkrRomDesc[] = {
	{ "new_splash_cor_2_27c010.bin", 0x20000, 0xeadf12dd, 1 | BRF_PRG | BRF_ESS },    //  0 68K Code
	{ "new_splash_cor_6_27c010.bin", 0x20000, 0x5a70d95b, 1 | BRF_PRG | BRF_ESS },    //  1
	{ "new_splash_cor_4_27c040.bin", 0x80000, 0x0f71b5c5, 1 | BRF_PRG | BRF_ESS },    //  2
	{ "new_splash_cor_8_27c040.bin", 0x80000, 0xccc0d187, 1 | BRF_PRG | BRF_ESS },    //  3
	{ "new_splash_cor_5_27c040.bin", 0x80000, 0xe71f8536, 1 | BRF_PRG | BRF_ESS },    //  4
	{ "new_splash_cor_9_27c040.bin", 0x80000, 0xa50537cc, 1 | BRF_PRG | BRF_ESS },    //  5
	
	{ "splash_1.c5",                 0x10000, 0x0ed7ebc9, 2 | BRF_PRG | BRF_ESS },    //  8 Z80 Code

	{ "new_splash_cor_13_27c010.bin",0x20000, 0x72ef9811, 3 | BRF_GRA },              //  9 Graphics
	{ "new_splash_cor_11_27c010.bin",0x20000, 0xeffa8d57, 3 | BRF_GRA },              // 10
	{ "new_splash_cor_12_27c010.bin",0x20000, 0x0f2d8006, 3 | BRF_GRA },              // 11
	{ "new_splash_cor_10_27c010.bin",0x20000, 0xcb4bdf04, 3 | BRF_GRA },              // 12
};

STD_ROM_PICK(nsplashkr)
STD_ROM_FN(nsplashkr)

static INT32 NsplashInit()
{
	is_nsplash = 1;

	return SplashInit();
}

struct BurnDriver BurnDrvNsplashkr = {
	"nsplashkr", "splash", NULL, NULL, "1992",
	"New Splash (ver. 1.4, checksum A26032A3, Korea, set 1)\0", NULL, "Gaelco / OMK Software (Windial license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, nsplashkrRomInfo, nsplashkrRomName, NULL, NULL, NULL, NULL, SplashInputInfo, SplashDIPInfo,
	NsplashInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x400,
	368, 240, 4, 3
};


// New Splash (ver. 1.4, checksum A26032A3, Korea, set 2)

static struct BurnRomInfo nsplashkraRomDesc[] = {
	{ "demo_2_set_1_27c010.bin", 	 0x20000, 0x9f2d1cd5, 1 | BRF_PRG | BRF_ESS },    //  0 68K Code
	{ "demo_6_set_1_27c010.bin", 	 0x20000, 0x8faf055e, 1 | BRF_PRG | BRF_ESS },    //  1
	{ "new_splash_cor_4_27c040.bin", 0x80000, 0x0f71b5c5, 1 | BRF_PRG | BRF_ESS },    //  2
	{ "new_splash_cor_8_27c040.bin", 0x80000, 0xccc0d187, 1 | BRF_PRG | BRF_ESS },    //  3
	{ "new_splash_cor_5_27c040.bin", 0x80000, 0xe71f8536, 1 | BRF_PRG | BRF_ESS },    //  4
	{ "new_splash_cor_9_27c040.bin", 0x80000, 0xa50537cc, 1 | BRF_PRG | BRF_ESS },    //  5
	
	{ "splash_1.c5",                 0x10000, 0x0ed7ebc9, 2 | BRF_PRG | BRF_ESS },    //  8 Z80 Code

	{ "new_splash_cor_13_27c010.bin",0x20000, 0x72ef9811, 3 | BRF_GRA },              //  9 Graphics
	{ "new_splash_cor_11_27c010.bin",0x20000, 0xeffa8d57, 3 | BRF_GRA },              // 10
	{ "new_splash_cor_12_27c010.bin",0x20000, 0x0f2d8006, 3 | BRF_GRA },              // 11
	{ "new_splash_cor_10_27c010.bin",0x20000, 0xcb4bdf04, 3 | BRF_GRA },              // 12
};

STD_ROM_PICK(nsplashkra)
STD_ROM_FN(nsplashkra)

struct BurnDriver BurnDrvNsplashkra = {
	"nsplashkra", "splash", NULL, NULL, "1992",
	"New Splash (ver. 1.4, checksum A26032A3, Korea, set 2)\0", NULL, "Gaelco / OMK Software (Windial license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, nsplashkraRomInfo, nsplashkraRomName, NULL, NULL, NULL, NULL, SplashInputInfo, SplashDIPInfo,
	NsplashInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x400,
	368, 240, 4, 3
};


// Splash! (ver. 1.3, checksum E7BEEBFA, Korea)

static struct BurnRomInfo splashkrRomDesc[] = {
	{ "windial_2_splash_27c010.bin", 0x20000, 0x55a97ae2, 1 | BRF_PRG | BRF_ESS },    //  0 68K Code
	{ "windial_6_splash_27c010.bin", 0x20000, 0x45f83601, 1 | BRF_PRG | BRF_ESS },    //  1
	{ "sp_kor_3_27c040.bin",         0x80000, 0x70a97c7b, 1 | BRF_PRG | BRF_ESS },    //  2
	{ "sp_kor_7_27c040.bin",         0x80000, 0x1dee35cb, 1 | BRF_PRG | BRF_ESS },    //  3
	{ "sp_kor_4_27c040.bin",         0x80000, 0xa21e7485, 1 | BRF_PRG | BRF_ESS },    //  4
	{ "sp_kor_8_27c040.bin",         0x80000, 0xa11a4340, 1 | BRF_PRG | BRF_ESS },    //  5
	{ "sp_kor_5_27c040.bin",         0x80000, 0xb043a71b, 1 | BRF_PRG | BRF_ESS },    //  6
	{ "sp_kor_9_27c040.bin",         0x80000, 0x57ec94bd, 1 | BRF_PRG | BRF_ESS },    //  7

	{ "splash_1.c5",                 0x10000, 0x0ed7ebc9, 2 | BRF_PRG | BRF_ESS },    //  8 Z80 Code

	{ "splash_13.i17",               0x20000, 0x028a4a68, 3 | BRF_GRA },              //  9 Graphics
	{ "splash_11.i14",               0x20000, 0x2a8cb830, 3 | BRF_GRA },              // 10
	{ "splash_12.i16",               0x20000, 0x21aeff2c, 3 | BRF_GRA },              // 11
	{ "splash_10.i13",               0x20000, 0xfebb9893, 3 | BRF_GRA },              // 12

	{ "p_a1020a-pl84c.g14",          0x00200, 0x00000000, 4 | BRF_NODUMP | BRF_OPT }, // 13 PLDs
	{ "1_gal16v8a-25lp.c13",         0x00117, 0x00000000, 4 | BRF_NODUMP | BRF_OPT }, // 14
	{ "2_gal16v8a-25lp.d5",          0x00117, 0x00000000, 4 | BRF_NODUMP | BRF_OPT }, // 15
	{ "3_gal20v8a-25lp.f4",          0x00157, 0x00000000, 4 | BRF_NODUMP | BRF_OPT }, // 16
};

STD_ROM_PICK(splashkr)
STD_ROM_FN(splashkr)

struct BurnDriver BurnDrvSplashkr = {
	"splashkr", "splash", NULL, NULL, "1992",
	"Splash! (ver. 1.3, checksum E7BEEBFA, Korea)\0", NULL, "Gaelco / OMK Software (Windial license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, splashkrRomInfo, splashkrRomName, NULL, NULL, NULL, NULL, SplashInputInfo, SplashDIPInfo,
	SplashInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x400,
	368, 240, 4, 3
};


// Splash! (ver. 1.1, checksum 4697D2BF, non North America)

static struct BurnRomInfo splash11RomDesc[] = {
	{ "sp_2_de3d_27c010.bin",0x20000, 0xca3bc3a6, 1 | BRF_PRG | BRF_ESS },    //  0 68K Code
	{ "sp_6_f9af_27c010.bin",0x20000, 0x9d727e1c, 1 | BRF_PRG | BRF_ESS },    //  1
	{ "splash_3.g5",         0x80000, 0xa4e8ed18, 1 | BRF_PRG | BRF_ESS },    //  2
	{ "splash_7.i5",         0x80000, 0x73e1154d, 1 | BRF_PRG | BRF_ESS },    //  3
	{ "splash_4.g6",         0x80000, 0xffd56771, 1 | BRF_PRG | BRF_ESS },    //  4
	{ "splash_8.i6",         0x80000, 0x16e9170c, 1 | BRF_PRG | BRF_ESS },    //  5
	{ "splash_5.g8",         0x80000, 0xdc3a3172, 1 | BRF_PRG | BRF_ESS },    //  6
	{ "splash_9.i8",         0x80000, 0x2e23e6c3, 1 | BRF_PRG | BRF_ESS },    //  7

	{ "splash_1.c5",         0x10000, 0x0ed7ebc9, 2 | BRF_PRG | BRF_ESS },    //  8 Z80 Code

	{ "splash_13.i17",       0x20000, 0x028a4a68, 3 | BRF_GRA },              //  9 Graphics
	{ "splash_11.i14",       0x20000, 0x2a8cb830, 3 | BRF_GRA },              // 10
	{ "splash_12.i16",       0x20000, 0x21aeff2c, 3 | BRF_GRA },              // 11
	{ "splash_10.i13",       0x20000, 0xfebb9893, 3 | BRF_GRA },              // 12

	{ "p_a1020a-pl84c.g14",  0x00200, 0x00000000, 4 | BRF_NODUMP | BRF_OPT }, // 13 PLDs
	{ "1_gal16v8a-25lp.c13", 0x00117, 0x00000000, 4 | BRF_NODUMP | BRF_OPT }, // 14
	{ "2_gal16v8a-25lp.d5",  0x00117, 0x00000000, 4 | BRF_NODUMP | BRF_OPT }, // 15
	{ "3_gal20v8a-25lp.f4",  0x00157, 0x00000000, 4 | BRF_NODUMP | BRF_OPT }, // 16
};

STD_ROM_PICK(splash11)
STD_ROM_FN(splash11)

struct BurnDriver BurnDrvSplash11 = {
	"splash11", "splash", NULL, NULL, "1992",
	"Splash! (ver. 1.1, checksum 4697D2BF, non North America)\0", NULL, "Gaelco / OMK Software", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, splash11RomInfo, splash11RomName, NULL, NULL, NULL, NULL, SplashInputInfo, SplashDIPInfo,
	SplashInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x400,
	368, 240, 4, 3
};


// Splash! (ver. 1.2, checksum 5071804D, non North America)

static struct BurnRomInfo splash12RomDesc[] = {
	{ "splash_2.g3",         0x20000, 0xb38fda40, 1 | BRF_PRG | BRF_ESS },    //  0 68K Code
	{ "splash_6.i3",         0x20000, 0x02359c47, 1 | BRF_PRG | BRF_ESS },    //  1
	{ "splash_3.g5",         0x80000, 0xa4e8ed18, 1 | BRF_PRG | BRF_ESS },    //  2
	{ "splash_7.i5",         0x80000, 0x73e1154d, 1 | BRF_PRG | BRF_ESS },    //  3
	{ "splash_4.g6",         0x80000, 0xffd56771, 1 | BRF_PRG | BRF_ESS },    //  4
	{ "splash_8.i6",         0x80000, 0x16e9170c, 1 | BRF_PRG | BRF_ESS },    //  5
	{ "splash_5.g8",         0x80000, 0xdc3a3172, 1 | BRF_PRG | BRF_ESS },    //  6
	{ "splash_9.i8",         0x80000, 0x2e23e6c3, 1 | BRF_PRG | BRF_ESS },    //  7

	{ "splash_1.c5",         0x10000, 0x0ed7ebc9, 2 | BRF_PRG | BRF_ESS },    //  8 Z80 Code

	{ "splash_13.i17",       0x20000, 0x028a4a68, 3 | BRF_GRA },              //  9 Graphics
	{ "splash_11.i14",       0x20000, 0x2a8cb830, 3 | BRF_GRA },              // 10
	{ "splash_12.i16",       0x20000, 0x21aeff2c, 3 | BRF_GRA },              // 11
	{ "splash_10.i13",       0x20000, 0xfebb9893, 3 | BRF_GRA },              // 12

	{ "p_a1020a-pl84c.g14",  0x00200, 0x00000000, 4 | BRF_NODUMP | BRF_OPT }, // 13 PLDs
	{ "1_gal16v8a-25lp.c13", 0x00117, 0x00000000, 4 | BRF_NODUMP | BRF_OPT }, // 14
	{ "2_gal16v8a-25lp.d5",  0x00117, 0x00000000, 4 | BRF_NODUMP | BRF_OPT }, // 15
	{ "3_gal20v8a-25lp.f4",  0x00157, 0x00000000, 4 | BRF_NODUMP | BRF_OPT }, // 16
};

STD_ROM_PICK(splash12)
STD_ROM_FN(splash12)

struct BurnDriver BurnDrvSplash12 = {
	"splash12", "splash", NULL, NULL, "1992",
	"Splash! (ver. 1.2, checksum 5071804D, non North America)\0", NULL, "Gaelco / OMK Software", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, splash12RomInfo, splash12RomName, NULL, NULL, NULL, NULL, SplashInputInfo, SplashDIPInfo,
	SplashInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x400,
	368, 240, 4, 3
};


// Splash! (ver. 1.0, checksum 2FC9AE1D, non North America)

static struct BurnRomInfo splash10RomDesc[] = {
	{ "splash10_2.g3",       0x20000, 0x38ba6632, 1 | BRF_PRG | BRF_ESS },    //  0 68K Code
	{ "splash10_6.i3",       0x20000, 0x0edc3373, 1 | BRF_PRG | BRF_ESS },    //  1
	{ "splash_3.g5",         0x80000, 0xa4e8ed18, 1 | BRF_PRG | BRF_ESS },    //  2
	{ "splash_7.i5",         0x80000, 0x73e1154d, 1 | BRF_PRG | BRF_ESS },    //  3
	{ "splash_4.g6",         0x80000, 0xffd56771, 1 | BRF_PRG | BRF_ESS },    //  4
	{ "splash_8.i6",         0x80000, 0x16e9170c, 1 | BRF_PRG | BRF_ESS },    //  5
	{ "splash_5.g8",         0x80000, 0xdc3a3172, 1 | BRF_PRG | BRF_ESS },    //  6
	{ "splash_9.i8",         0x80000, 0x2e23e6c3, 1 | BRF_PRG | BRF_ESS },    //  7

	{ "splash_1.c5",         0x10000, 0x0ed7ebc9, 2 | BRF_PRG | BRF_ESS },    //  8 Z80 Code

	{ "splash_13.i17",       0x20000, 0x028a4a68, 3 | BRF_GRA },              //  9 Graphics
	{ "splash_11.i14",       0x20000, 0x2a8cb830, 3 | BRF_GRA },              // 10
	{ "splash_12.i16",       0x20000, 0x21aeff2c, 3 | BRF_GRA },              // 11
	{ "splash_10.i13",       0x20000, 0xfebb9893, 3 | BRF_GRA },              // 12

	{ "p_a1020a-pl84c.g14",  0x00200, 0x00000000, 4 | BRF_NODUMP | BRF_OPT }, // 13 PLDs
	{ "1_gal16v8a-25lp.c13", 0x00117, 0x00000000, 4 | BRF_NODUMP | BRF_OPT }, // 14
	{ "2_gal16v8a-25lp.d5",  0x00117, 0x00000000, 4 | BRF_NODUMP | BRF_OPT }, // 15
	{ "3_gal20v8a-25lp.f4",  0x00157, 0x00000000, 4 | BRF_NODUMP | BRF_OPT }, // 16
};

STD_ROM_PICK(splash10)
STD_ROM_FN(splash10)

static INT32 Splash10Init()
{
	sprite_attr2_shift = 0;
	return SplashInit();
}

struct BurnDriver BurnDrvSplash10 = {
	"splash10", "splash", NULL, NULL, "1992",
	"Splash! (ver. 1.0, checksum 2FC9AE1D, non North America)\0", NULL, "Gaelco / OMK Software", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, splash10RomInfo, splash10RomName, NULL, NULL, NULL, NULL, SplashInputInfo, SplashDIPInfo,
	Splash10Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x400,
	368, 240, 4, 3
};


// The Return of Lady Frog (set 1)

static struct BurnRomInfo roldfrogRomDesc[] = {
	{ "roldfrog.002", 0x80000, 0x724cf022, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "roldfrog.006", 0x80000, 0xe52a7ae2, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "roldfrog.003", 0x80000, 0xa1d49967, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "roldfrog.007", 0x80000, 0xe5805c4e, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "roldfrog.004", 0x80000, 0x709281f5, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "roldfrog.008", 0x80000, 0x39adcba4, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "roldfrog.005", 0x80000, 0xb683160c, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "roldfrog.009", 0x80000, 0xe475fb76, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "protdata.bin", 0x08000, 0xecaa8dd1, 1 | BRF_PRG | BRF_ESS }, //  8

	{ "roldfrog.001", 0x20000, 0xba9eb1c6, 2 | BRF_PRG | BRF_ESS }, //  9 Z80 Code

	{ "roldfrog.010", 0x20000, 0x51fd0e1a, 3 | BRF_GRA },           // 10 Graphics
	{ "roldfrog.011", 0x20000, 0x610bf6f3, 3 | BRF_GRA },           // 11
	{ "roldfrog.012", 0x20000, 0x466ede67, 3 | BRF_GRA },           // 12
	{ "roldfrog.013", 0x20000, 0xfad3e8be, 3 | BRF_GRA },           // 13
};

STD_ROM_PICK(roldfrog)
STD_ROM_FN(roldfrog)

struct BurnDriver BurnDrvRoldfrog = {
	"roldfrog", NULL, NULL, NULL, "1993",
	"The Return of Lady Frog (set 1)\0", NULL, "Microhard", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, roldfrogRomInfo, roldfrogRomName, NULL, NULL, NULL, NULL, SplashInputInfo, SplashDIPInfo,
	RoldfrogInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x400,
	368, 240, 4, 3
};


// The Return of Lady Frog (set 2)

static struct BurnRomInfo roldfrogaRomDesc[] = {
	{ "roldfrog.002", 0x80000, 0x724cf022, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "roldfrog.006", 0x80000, 0xe52a7ae2, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "roldfrog.003", 0x80000, 0xa1d49967, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "roldfrog.007", 0x80000, 0xe5805c4e, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "roldfrog.004", 0x80000, 0x709281f5, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "roldfrog.008", 0x80000, 0x39adcba4, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "roldfrog.005", 0x80000, 0xb683160c, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "9",            0x80000, 0xfd515b58, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "protdata.bin", 0x08000, 0xecaa8dd1, 1 | BRF_PRG | BRF_ESS }, //  8

	{ "roldfrog.001", 0x20000, 0xba9eb1c6, 2 | BRF_PRG | BRF_ESS }, //  9 Z80 Code

	{ "roldfrog.010", 0x20000, 0x51fd0e1a, 3 | BRF_GRA },           // 10 Graphics
	{ "roldfrog.011", 0x20000, 0x610bf6f3, 3 | BRF_GRA },           // 11
	{ "roldfrog.012", 0x20000, 0x466ede67, 3 | BRF_GRA },           // 12
	{ "roldfrog.013", 0x20000, 0xfad3e8be, 3 | BRF_GRA },           // 13
};

STD_ROM_PICK(roldfroga)
STD_ROM_FN(roldfroga)

struct BurnDriver BurnDrvRoldfroga = {
	"roldfroga", "roldfrog", NULL, NULL, "1993",
	"The Return of Lady Frog (set 2)\0", NULL, "Microhard", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, roldfrogaRomInfo, roldfrogaRomName, NULL, NULL, NULL, NULL, SplashInputInfo, SplashDIPInfo,
	RoldfrogInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x400,
	368, 240, 4, 3
};

