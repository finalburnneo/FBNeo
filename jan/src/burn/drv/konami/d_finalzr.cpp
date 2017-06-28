// FB Alpha Finalizer driver module
// Based on MAME driver by Nicola Salmoria

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "i8039.h"
#include "sn76496.h"
#include "dac.h"
#include "resnet.h"

static UINT8 *AllMem;
static UINT8 *RamEnd;
static UINT8 *AllRam;
static UINT8 *MemEnd;
static UINT8 *DrvM6809ROM;
static UINT8 *DrvM6809DecROM;
static UINT8 *DrvI8039ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvColPROM;
static UINT8 *DrvColRAM0;
static UINT8 *DrvVidRAM0;
static UINT8 *DrvColRAM1;
static UINT8 *DrvVidRAM1;
static UINT8 *DrvSprRAM0;
static UINT8 *DrvSprRAM1;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 DrvInputs[3];
static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[3];
static UINT8 DrvReset;

static UINT8 scroll;
static UINT8 nmi_enable;
static UINT8 irq_enable;
static UINT8 charbank;
static UINT8 spriterambank;
static UINT8 soundlatch;
static UINT8 flipscreen;
static UINT8 i8039_t1;

static INT32 watchdog = 0;
static INT32 vblank;

static struct BurnInputInfo FinalizrInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Finalizr)

static struct BurnDIPInfo FinalizrDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x5a, NULL			},
	{0x14, 0xff, 0xff, 0xfe, NULL			},

	{0   , 0xfe, 0   ,    16, "Coin A"		},
	{0x12, 0x01, 0x0f, 0x02, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0x0f, 0x05, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x12, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x12, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x12, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x12, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x12, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x12, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x12, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x12, 0x01, 0x0f, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    16, "Coin B"		},
	{0x12, 0x01, 0xf0, 0x20, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0xf0, 0x50, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"	},
	{0x12, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"	},
	{0x12, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"	},
	{0x12, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x12, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x12, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x12, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x12, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x12, 0x01, 0xf0, 0x00, "No Coin B"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0x03, 0x03, "2"			},
	{0x13, 0x01, 0x03, 0x02, "3"			},
	{0x13, 0x01, 0x03, 0x01, "4"			},
	{0x13, 0x01, 0x03, 0x00, "7"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x13, 0x01, 0x04, 0x00, "Upright"		},
	{0x13, 0x01, 0x04, 0x04, "Cocktail"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x13, 0x01, 0x18, 0x18, "30000 150000"		},
	{0x13, 0x01, 0x18, 0x10, "50000 300000"		},
	{0x13, 0x01, 0x18, 0x08, "30000"		},
	{0x13, 0x01, 0x18, 0x00, "50000"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x60, 0x60, "Easy"			},
	{0x13, 0x01, 0x60, 0x40, "Normal"		},
	{0x13, 0x01, 0x60, 0x20, "Hard"			},
	{0x13, 0x01, 0x60, 0x00, "Very Hard"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x80, 0x80, "Off"			},
	{0x13, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x14, 0x01, 0x01, 0x00, "Off"			},
	{0x14, 0x01, 0x01, 0x01, "On"			},

	{0   , 0xfe, 0   ,    2, "Controls"		},
	{0x14, 0x01, 0x02, 0x02, "Single"		},
	{0x14, 0x01, 0x02, 0x00, "Dual"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x04, 0x04, "Off"			},
	{0x14, 0x01, 0x04, 0x00, "On"			},
};

STDDIPINFO(Finalizr)

static UINT8 finalizr_main_read(UINT16 address)
{
	switch (address)
	{
		case 0x0800:
			return DrvDips[2];

		case 0x0808:
			return DrvDips[1];

		case 0x0810:
			return (DrvInputs[0] & 0x7f) | (vblank ? 0x80 : 0);
		case 0x0811:
		case 0x0812:
			return (DrvInputs[address & 3] & 0x7f);

		case 0x0813:
			return DrvDips[0];
	}

	return 0;
}

static void finalizr_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x0001:
			scroll = data;
		return;

		case 0x0003:
			charbank = data & 0x03;
			spriterambank = data & 0x08;
		return;

		case 0x0004:
			nmi_enable = data & 0x01;
			irq_enable = data & 0x02;
			flipscreen =~data & 0x08;
		return;

		case 0x0818:
			watchdog = 0;
		return;

		case 0x0819:
			// coin counters
		return;

		case 0x081a:
			SN76496Write(0, data);
		return;

		case 0x081b: // nop
		return;

		case 0x081c:
			I8039SetIrqState(1);
		return;

		case 0x081d:
			soundlatch = data;
		return;
	}
}

static UINT8 __fastcall finalizr_sound_read(UINT32 address)
{
	return DrvI8039ROM[address & 0x0fff];
}

static void __fastcall finalizr_sound_write_port(UINT32 port, UINT8 data)
{
	port &= 0x1ff;

	switch (port)
	{
		case I8039_p1:
			DACWrite(0, data);
		return;

		case I8039_p2:
			if (~data & 0x80) I8039SetIrqState(0);
		return;

		case I8039_t0:
			// clock?
		return;
	}
}

static UINT8 __fastcall finalizr_sound_read_port(UINT32 port)
{
	port &= 0x1ff;

	if (port < 0x100) {
		return soundlatch;
	}

	switch (port)
	{
		case I8039_t1:
			i8039_t1 = (i8039_t1 + 1) & 0xf;
			return (!(i8039_t1 % 3) && (i8039_t1 > 0));
	}

	return 0;
}

static INT32 DrvSyncDAC()
{
	return (INT32)(float)(nBurnSoundLen * (I8039TotalCycles() / (614400.0000 / (nBurnFPS / 100.0000))));
}

static INT32 DrvDoReset(INT32 clear_ram)
{
	if (clear_ram) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	M6809Open(0);
	M6809Reset();
	M6809Close();

	I8039Reset();
	DACReset();

	scroll = 0;
	nmi_enable = 0;
	irq_enable = 0;
	charbank = 0;
	spriterambank = 0;
	soundlatch = 0;
	i8039_t1 = 0;

	watchdog = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6809ROM		= Next; Next += 0x00c000;
	DrvM6809DecROM  = Next; Next += 0x00c000;

	DrvI8039ROM		= Next; Next += 0x001000;

	DrvGfxROM0		= Next; Next += 0x040000;

	DrvColPROM		= Next; Next += 0x000240;

	DrvPalette		= (UINT32*)Next; Next += 0x0200 * sizeof(UINT32);

	AllRam			= Next;

	DrvColRAM0		= Next; Next += 0x000400;
	DrvVidRAM0		= Next; Next += 0x000400;
	DrvColRAM1		= Next; Next += 0x000400;
	DrvVidRAM1		= Next; Next += 0x000400;

	DrvSprRAM0		= Next; Next += 0x000800;
	DrvSprRAM1		= Next; Next += 0x000800;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void FinalizrDecode()
{
	for (INT32 i = 0; i < 0xc000; i++) {
		DrvM6809DecROM[i] = DrvM6809ROM[i] ^ (((i&2)?0x80:0x20)|((i&8)?0x08:0x02));
	}
}

static void DrvGfxExpand()
{
	for (INT32 i = 0x20000-1; i >= 0; i--) {
		DrvGfxROM0[i*2+0] = DrvGfxROM0[i] >> 4;
		DrvGfxROM0[i*2+1] = DrvGfxROM0[i] & 0xf;
	}
}

static void DrvPaletteInit()
{
	UINT32 palette[0x20];

	static const int resistances[4] = { 2200, 1000, 470, 220 };
	double rweights[4], gweights[4], bweights[4];

	compute_resistor_weights(0, 0xff, -1.0,
			4, &resistances[0], rweights, 470, 0,
			4, &resistances[0], gweights, 470, 0,
			4, &resistances[0], bweights, 470, 0);

	for (INT32 i = 0; i < 0x20; i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 0x01;
		INT32 bit3 = (DrvColPROM[i] >> 3) & 0x01;

		INT32 r = combine_4_weights(rweights, bit0, bit1, bit2, bit3);

		bit0 = (DrvColPROM[i] >> 4) & 0x01;
		bit1 = (DrvColPROM[i] >> 5) & 0x01;
		bit2 = (DrvColPROM[i] >> 6) & 0x01;
		bit3 = (DrvColPROM[i] >> 7) & 0x01;

		INT32 g = combine_4_weights(rweights, bit0, bit1, bit2, bit3);

		bit0 = (DrvColPROM[i + 0x20] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + 0x20] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + 0x20] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + 0x20] >> 3) & 0x01;

		INT32 b = combine_4_weights(rweights, bit0, bit1, bit2, bit3);

		palette[i] = BurnHighCol(r,g,b,0);
	}

	for (INT32 i = 0; i < 0x200; i++) {
		DrvPalette[i] = palette[(DrvColPROM[i+0x40]&0xf)+((~i&0x100)/16)];
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

	if ((BurnDrvGetFlags() & BDF_BOOTLEG) == 0)
	{
		if (BurnLoadRom(DrvM6809ROM + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x04000,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x08000,  2, 1)) return 1;

		if (BurnLoadRom(DrvI8039ROM + 0x00000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0  + 0x00000,  4, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x00001,  5, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x08000,  6, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x08001,  7, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x10000,  8, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x10001,  9, 2)) return 1;
		memset (DrvGfxROM0 + 0x18000, 0xff, 0x08000);

		if (BurnLoadRom(DrvColPROM  + 0x00000, 10, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00020, 11, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00040, 12, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00140, 13, 1)) return 1;
	}
	else
	{
		if (BurnLoadRom(DrvM6809ROM + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x08000,  1, 1)) return 1;

		if (BurnLoadRom(DrvI8039ROM + 0x00000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0  + 0x00000,  3, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x00001,  4, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x08000,  5, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x08001,  6, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x10000,  7, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x10001,  8, 2)) return 1;
		memset (DrvGfxROM0 + 0x18000, 0xff, 0x08000);

		if (BurnLoadRom(DrvColPROM  + 0x00000,  9, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00020, 10, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00040, 11, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00140, 12, 1)) return 1;
	}

	FinalizrDecode();
	DrvGfxExpand();
	DrvPaletteInit();

	M6809Init(1);
	M6809Open(0);
	M6809MapMemory(DrvColRAM0,	0x2000, 0x23ff, MAP_RAM);
	M6809MapMemory(DrvVidRAM0,	0x2400, 0x27ff, MAP_RAM);
	M6809MapMemory(DrvColRAM1,	0x2800, 0x2bff, MAP_RAM);
	M6809MapMemory(DrvVidRAM1,	0x2c00, 0x2fff, MAP_RAM);
	M6809MapMemory(DrvSprRAM0,	0x3000, 0x37ff, MAP_RAM);
	M6809MapMemory(DrvSprRAM1,	0x3800, 0x3fff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM,	0x4000, 0xffff, MAP_READ);
	M6809MapMemory(DrvM6809DecROM,	0x4000, 0xffff, MAP_FETCH);
	M6809SetWriteHandler(finalizr_main_write);
	M6809SetReadHandler(finalizr_main_read);
	M6809Close();

	I8039Init(NULL);
	I8039SetProgramReadHandler(finalizr_sound_read);
	I8039SetCPUOpReadHandler(finalizr_sound_read);
	I8039SetCPUOpReadArgHandler(finalizr_sound_read);
	I8039SetIOReadHandler(finalizr_sound_read_port);
	I8039SetIOWriteHandler(finalizr_sound_write_port);

	SN76489AInit(0, 1536000, 0);
	SN76496SetRoute(0, 0.45, BURN_SND_ROUTE_BOTH);

	DACInit(0, 0, 1, DrvSyncDAC);
	DACSetRoute(0, 0.15, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6809Exit();
	I8039Exit();

	SN76496Exit();
	DACExit();

	BurnFree (AllMem);

	return 0;
}

static void draw_bg_layer()
{
	INT32 scrollx = scroll - 32;

	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sx = ((offs & 0x1f) * 8);
		INT32 sy = ((offs / 0x20) * 8) - 16;
		if (sy < 0 || sy >= nScreenHeight) continue;

		sx -= scrollx;

		if (sx < 25) sx += 256;
		if (sx >= nScreenWidth || sy >= nScreenHeight) continue;

		INT32 attr  = DrvColRAM0[offs];
		INT32 code  = DrvVidRAM0[offs] | ((attr & 0xc0) << 2) | (charbank << 10);
		INT32 color = attr & 0x0f;
		INT32 flipy = attr & 0x20;
		INT32 flipx = attr & 0x10;

		if (flipy) {
			if (flipx) {
				Render8x8Tile_FlipXY_Clip(pTransDraw, code, sx, sy, color, 4, 0, DrvGfxROM0);
			} else {
				Render8x8Tile_FlipY_Clip(pTransDraw, code, sx, sy, color, 4, 0, DrvGfxROM0);
			}
		} else {
			if (flipx) {
				Render8x8Tile_FlipX_Clip(pTransDraw, code, sx, sy, color, 4, 0, DrvGfxROM0);
			} else {
				Render8x8Tile_Clip(pTransDraw, code, sx, sy, color, 4, 0, DrvGfxROM0);
			}
		}
	}
}

static void draw_fg_layer()
{
	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sx = (offs & 0x1f) * 8;
		if (sx >= 32) continue;
		INT32 sy = ((offs / 0x20) * 8) - 16;
		if (sy < 0 || sy >= nScreenHeight) continue;

		INT32 attr  = DrvColRAM1[offs];
		INT32 code  = DrvVidRAM1[offs] | ((attr & 0xc0) << 2);
		INT32 color = attr & 0x0f;
		INT32 flipy = attr & 0x20;
		INT32 flipx = attr & 0x10;

		if (flipy) {
			if (flipx) {
				Render8x8Tile_FlipXY_Clip(pTransDraw, code, sx, sy, color, 4, 0, DrvGfxROM0);
			} else {
				Render8x8Tile_FlipY_Clip(pTransDraw, code, sx, sy, color, 4, 0, DrvGfxROM0);
			}
		} else {
			if (flipx) {
				Render8x8Tile_FlipX_Clip(pTransDraw, code, sx, sy, color, 4, 0, DrvGfxROM0);
			} else {
				Render8x8Tile_Clip(pTransDraw, code, sx, sy, color, 4, 0, DrvGfxROM0);
			}
		}
	}
}

static void draw_16x16(INT32 code, INT32 color, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy)
{
	code &= 0xfff;
	sy -= 16;

	INT32 flip = ((flipx)?1:0)|((flipy)?2:0);

	for (INT32 i = 0; i < 4; i++)
	{
		if (flipy) {
			if (flipx) {
				Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, code + (flip^i), sx + (i & 1) * 8, sy + (i & 2) * 4, color, 4, 0, 0x100, DrvGfxROM0);
			} else {
				Render8x8Tile_Mask_FlipY_Clip(pTransDraw, code + (flip^i), sx + (i & 1) * 8, sy + (i & 2) * 4, color, 4, 0, 0x100, DrvGfxROM0);
			}
		} else {
			if (flipx) {
				Render8x8Tile_Mask_FlipX_Clip(pTransDraw, code + (flip^i), sx + (i & 1) * 8, sy + (i & 2) * 4, color, 4, 0, 0x100, DrvGfxROM0);
			} else {
				Render8x8Tile_Mask_Clip(pTransDraw, code + (flip^i), sx + (i & 1) * 8, sy + (i & 2) * 4, color, 4, 0, 0x100, DrvGfxROM0);
			}
		}
	}
}

static void draw_8x8(INT32 code, INT32 color, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy)
{
	code &= 0xfff;
	sy -= 16;

	for (INT32 i = 0; i < 4; i++)
	{
		if (flipy) {
			if (flipx) {
				Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0x100, DrvGfxROM0);
			} else {
				Render8x8Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0x100, DrvGfxROM0);
			}
		} else {
			if (flipx) {
				Render8x8Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0x100, DrvGfxROM0);
			} else {
				Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0x100, DrvGfxROM0);
			}
		}
	}
}

static void draw_sprites()
{
	UINT8 *sr = spriterambank ? DrvSprRAM1 : DrvSprRAM0;

	for (INT32 offs = 0; offs <= 0x200 - 5; offs += 5)
	{
		INT32 sx    = 32 + 1 + sr[offs + 3] - ((sr[offs + 4] & 0x01) << 8);
		INT32 sy    = sr[offs + 2];
		INT32 flipx = sr[offs + 4] & 0x20;
		INT32 flipy = sr[offs + 4] & 0x40;
		INT32 code  = sr[offs] + ((sr[offs + 1] & 0x0f) << 8);
		INT32 color = ((sr[offs + 1] & 0xf0) >> 4);
		INT32 size  = sr[offs + 4] & 0x1c;

		if (size >= 0x10)
		{
			if (flipscreen)
			{
				sx = 256 - sx;
				sy = 224 - sy;
				flipx = !flipx;
				flipy = !flipy;
			}

			draw_16x16(code*4+0, color, flipx ? sx + 16 : sx, flipy ? sy + 16 : sy, flipx, flipy);
			draw_16x16(code*4+4, color, flipx ? sx : sx + 16, flipy ? sy + 16 : sy, flipx, flipy);
			draw_16x16(code*4+8, color, flipx ? sx + 16: sx , flipy ? sy : sy + 16, flipx, flipy);
			draw_16x16(code*4+12, color, flipx ? sx : sx + 16, flipy ? sy : sy + 16, flipx, flipy);
		}
		else
		{
			if (flipscreen)
			{
				sx = ((size & 0x08) ? 280: 272) - sx;
				sy = ((size & 0x04) ? 248: 240) - sy;
				flipx = !flipx;
				flipy = !flipy;
			}

			if (size == 0x00)
			{
				draw_16x16(code*4, color, sx, sy, flipx, flipy);
			}
			else
			{
				code = ((code & 0x3ff) << 2) | ((code & 0xc00) >> 10);

				if (size == 0x04)
				{
					draw_8x8(code &~1, color, flipx ? sx + 8 : sx, sy, flipx, flipy);
					draw_8x8(code | 1, color, flipx ? sx : sx + 8, sy, flipx, flipy);
				}
				else if (size == 0x08)
				{
					draw_8x8(code &~2, color, sx, flipy ? sy + 8 : sy, flipx, flipy);
					draw_8x8(code | 2, color, sx, flipy ? sy : sy + 8, flipx, flipy);
				}
				else if (size == 0x0c)
				{
					draw_8x8(code, color, sx, sy, flipx, flipy);
				}
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

	if (nBurnLayer & 1) draw_bg_layer();
	if (nBurnLayer & 2) draw_sprites();
	if (nBurnLayer & 4) draw_fg_layer();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	watchdog++;
	if (watchdog >= 180) {
		DrvDoReset(0);
	}

	if (DrvReset) {
		DrvDoReset(1);
	}

	M6809NewFrame();
	I8039NewFrame();

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
	INT32 nCyclesTotal[2] = { 1536000 / 60, 9216000 / 15 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	M6809Open(0);

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone[0] += M6809Run(nCyclesTotal[0] / nInterleave);
		if (i == 240 && irq_enable) {
			M6809SetIRQLine(0, CPU_IRQSTATUS_ACK);
			M6809Run(100);
			M6809SetIRQLine(0, CPU_IRQSTATUS_NONE);
		}

		if ((i % 32) == 31 && nmi_enable) M6809SetIRQLine(0x20, CPU_IRQSTATUS_AUTO);

		if (i == 240) vblank = 1; // ?

		INT32 nSegment = (i + 1) * nCyclesTotal[1] / nInterleave;
		nCyclesDone[1] += I8039Run(nSegment - nCyclesDone[1]);

		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			SN76496Update(0, pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	M6809Close();

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		SN76496Update(0, pSoundBuf, nSegmentLength);
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction,INT32 *pnMin)
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
		M6809Scan(nAction);
		I8039Scan(nAction,pnMin);

		DACScan(nAction,pnMin);
		SN76496Scan(nAction,pnMin);

		SCAN_VAR(scroll);
		SCAN_VAR(nmi_enable);
		SCAN_VAR(irq_enable);
		SCAN_VAR(charbank);
		SCAN_VAR(spriterambank);
		SCAN_VAR(soundlatch);
		SCAN_VAR(flipscreen);
		SCAN_VAR(i8039_t1);
	}

	return 0;
}


// Finalizer - Super Transformation

static struct BurnRomInfo finalizrRomDesc[] = {
	{ "523k01.9c",		0x4000, 0x716633cb, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "523k02.12c",		0x4000, 0x1bccc696, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "523k03.13c",		0x4000, 0xc48927c6, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "d8749hd.bin",	0x0800, 0x978dfc33, 2 | BRF_PRG | BRF_ESS }, //  3 I8039 Code

	{ "523h04.5e",		0x4000, 0xc056d710, 3 | BRF_GRA },           //  4 Sprites & tiles
	{ "523h07.5f",		0x4000, 0x50e512ba, 3 | BRF_GRA },           //  5
	{ "523h05.6e",		0x4000, 0xae0d0f76, 3 | BRF_GRA },           //  6
	{ "523h08.6f",		0x4000, 0x79f44e17, 3 | BRF_GRA },           //  7
	{ "523h06.7e",		0x4000, 0xd2db9689, 3 | BRF_GRA },           //  8
	{ "523h09.7f",		0x4000, 0x8896dc85, 3 | BRF_GRA },           //  9

	{ "523h10.2f",		0x0020, 0xec15dd15, 4 | BRF_GRA },           // 10 Color Proms
	{ "523h11.3f",		0x0020, 0x54be2e83, 4 | BRF_GRA },           // 11
	{ "523h13.11f",		0x0100, 0x4e0647a0, 4 | BRF_GRA },           // 12
	{ "523h12.10f",		0x0100, 0x53166a2a, 4 | BRF_GRA },           // 13
};

STD_ROM_PICK(finalizr)
STD_ROM_FN(finalizr)

struct BurnDriver BurnDrvFinalizr = {
	"finalizr", NULL, NULL, NULL, "1985",
	"Finalizer - Super Transformation\0", NULL, "Konami", "GX523",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_PREFIX_KONAMI, GBF_VERSHOOT, 0,
	NULL, finalizrRomInfo, finalizrRomName, NULL, NULL, FinalizrInputInfo, FinalizrDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 272, 3, 4
};


// Finalizer - Super Transformation (bootleg)

static struct BurnRomInfo finalizrbRomDesc[] = {
	{ "finalizr.5",		0x8000, 0xa55e3f14, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "finalizr.6",		0x4000, 0xce177f6e, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "d8749hd.bin",	0x0800, 0x978dfc33, 2 | BRF_PRG | BRF_ESS }, //  2 I8039 Code

	{ "523h04.5e",		0x4000, 0xc056d710, 3 | BRF_GRA },           //  3 Sprites & tiles
	{ "523h07.5f",		0x4000, 0x50e512ba, 3 | BRF_GRA },           //  4
	{ "523h05.6e",		0x4000, 0xae0d0f76, 3 | BRF_GRA },           //  5
	{ "523h08.6f",		0x4000, 0x79f44e17, 3 | BRF_GRA },           //  6
	{ "523h06.7e",		0x4000, 0xd2db9689, 3 | BRF_GRA },           //  7
	{ "523h09.7f",		0x4000, 0x8896dc85, 3 | BRF_GRA },           //  8

	{ "523h10.2f",		0x0020, 0xec15dd15, 4 | BRF_GRA },           //  9 Color Proms
	{ "523h11.3f",		0x0020, 0x54be2e83, 4 | BRF_GRA },           // 10
	{ "523h13.11f",		0x0100, 0x4e0647a0, 4 | BRF_GRA },           // 11
	{ "523h12.10f",		0x0100, 0x53166a2a, 4 | BRF_GRA },           // 12
};

STD_ROM_PICK(finalizrb)
STD_ROM_FN(finalizrb)

struct BurnDriver BurnDrvFinalizrb = {
	"finalizrb", "finalizr", NULL, NULL, "1985",
	"Finalizer - Super Transformation (bootleg)\0", NULL, "bootleg", "GX523",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_PREFIX_KONAMI, GBF_VERSHOOT, 0,
	NULL, finalizrbRomInfo, finalizrbRomName, NULL, NULL, FinalizrInputInfo, FinalizrDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 272, 3, 4
};
