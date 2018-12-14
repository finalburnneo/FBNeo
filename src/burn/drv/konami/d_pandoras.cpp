// FB Alpha Pandora's Palace driver module
// Based on MAME driver by Manuel Abadia

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "z80_intf.h"
#include "i8039.h"
#include "ay8910.h"
#include "dac.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvMainROM;
static UINT8 *DrvSubROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvI8039ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvColRAM;
static UINT8 *DrvShareRAM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvSprRAM;

static UINT32 *DrvPalette;
static UINT8  DrvRecalc;

static UINT8 soundlatch;
static UINT8 soundlatch2;
static UINT8 i8039_status;

static UINT8 flipscreen;
static UINT8 scrolly;

static UINT8 irq_enable[2];
static UINT8 firq_trigger[2];

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static INT32 watchdog;

static struct BurnInputInfo PandorasInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Pandoras)

static struct BurnDIPInfo PandorasDIPList[]=
{
	{0x10, 0xff, 0xff, 0xff, NULL			},
	{0x11, 0xff, 0xff, 0x5b, NULL			},
	{0x12, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    16, "Coin A"		},
	{0x10, 0x01, 0x0f, 0x02, "4 Coins 1 Credits"	},
	{0x10, 0x01, 0x0f, 0x05, "3 Coins 1 Credits"	},
	{0x10, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"	},
	{0x10, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x10, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x10, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x10, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x10, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x10, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x10, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x10, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x10, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x10, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x10, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x10, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x10, 0x01, 0x0f, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    16, "Coin B"		},
	{0x10, 0x01, 0xf0, 0x20, "4 Coins 1 Credits"	},
	{0x10, 0x01, 0xf0, 0x50, "3 Coins 1 Credits"	},
	{0x10, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"	},
	{0x10, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"	},
	{0x10, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"	},
	{0x10, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x10, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"	},
	{0x10, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"	},
	{0x10, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x10, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x10, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x10, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x10, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x10, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x10, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x10, 0x01, 0xf0, 0x00, "No Coin B"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x11, 0x01, 0x03, 0x03, "3"			},
	{0x11, 0x01, 0x03, 0x02, "4"			},
	{0x11, 0x01, 0x03, 0x01, "5"			},
	{0x11, 0x01, 0x03, 0x00, "7"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x11, 0x01, 0x04, 0x00, "Upright"		},
	{0x11, 0x01, 0x04, 0x04, "Cocktail"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x11, 0x01, 0x18, 0x18, "20k and every 60k"	},
	{0x11, 0x01, 0x18, 0x10, "30k and every 70k"	},
	{0x11, 0x01, 0x18, 0x08, "20k"			},
	{0x11, 0x01, 0x18, 0x00, "30k"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x11, 0x01, 0x60, 0x60, "Easy"			},
	{0x11, 0x01, 0x60, 0x40, "Normal"		},
	{0x11, 0x01, 0x60, 0x20, "Difficult"		},
	{0x11, 0x01, 0x60, 0x00, "Very Difficult"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x11, 0x01, 0x80, 0x80, "Off"			},
	{0x11, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x12, 0x01, 0x01, 0x01, "Off"			},
	{0x12, 0x01, 0x01, 0x00, "On"			},
};

STDDIPINFO(Pandoras)

static void interrupt_control(INT32 offset, UINT8 data, INT32 cpu)
{
	switch (offset & 0x07)
	{
		case 0x00:
		{
			if (data == 0) {
				M6809Close();
				M6809Open(0);
				M6809SetIRQLine(CPU_IRQLINE_IRQ, CPU_IRQSTATUS_NONE);
				M6809Close();
				M6809Open(cpu);
			}
			irq_enable[0] = data;
		}
		return;

		case 0x02:
		case 0x03:
			// coin counters, data & 0x01
		return;

		case 0x05:
			flipscreen = data;
		return;

		case 0x06:
		{
			if (data == 0) {
				M6809Close();
				M6809Open(1);
				M6809SetIRQLine(CPU_IRQLINE_IRQ, CPU_IRQSTATUS_NONE);
				M6809Close();
				M6809Open(cpu);
			}
			irq_enable[1] = data;
		}

		case 0x07:
		{
			return;	// disable NMI. Why???

			M6809Close();
			M6809Open(1);
			M6809SetIRQLine(0x20, CPU_IRQSTATUS_AUTO);
			M6809Close();
			M6809Open(cpu);
		}
		return;
	}
}

static void pandoras_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfff8) == 0x1800) {
		interrupt_control(address, data, 0);
		return;
	}

	switch (address)
	{
		case 0x1a00:
			scrolly = data;
		return;

		case 0x1c00:
			ZetSetVector(0xff);
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;

		case 0x1e00:
			soundlatch = data;
		return;

		case 0x2000:
			if (!firq_trigger[1] && data) {
				M6809Close();
				M6809Open(1);
				M6809SetIRQLine(CPU_IRQLINE_FIRQ, CPU_IRQSTATUS_AUTO);
				M6809Close();
				M6809Open(0);
			}
			firq_trigger[1] = data;
		return;

		case 0x2001:
			watchdog = 0;
		return;
	}
}

static void pandoras_sub_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfff8) == 0x1800) {
		interrupt_control(address, data, 1);
		return;
	}

	switch (address)
	{
		case 0x8000:
			watchdog = 0;
		return;

		case 0xa000:
			if (!firq_trigger[0] && data) {
				M6809Close();
				M6809Open(0);
				M6809SetIRQLine(CPU_IRQLINE_FIRQ, CPU_IRQSTATUS_AUTO);
				M6809Close();
				M6809Open(1);
			}
			firq_trigger[0] = data;
		return;
	}
}

static UINT8 pandoras_sub_read(UINT16 address)
{
	switch (address)
	{
		case 0x1800:
			return DrvDips[0];

		case 0x1a00:
		case 0x1a01:
		case 0x1a02:
			return DrvInputs[address & 3];

		case 0x1a03:
			return DrvDips[2];

		case 0x1c00:
			return DrvDips[1];

		case 0x1e00:
			return 0;
	}

	return 0;
}

static void __fastcall pandoras_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x6000:
		case 0x6002:
			AY8910Write(0, (address / 2) & 1, data);
		return;

		case 0x8000:
			I8039SetIrqState(CPU_IRQSTATUS_ACK);
		return;

		case 0xa000:
			soundlatch2 = data;
		return;
	}
}

static UINT8 __fastcall pandoras_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x4000:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			return soundlatch;

		case 0x6001:
			return AY8910Read(0);
	}

	return 0;
}

static UINT8 __fastcall pandoras_i8039_read(UINT32 address)
{
	return DrvI8039ROM[address & 0x0fff];
}

static UINT8 __fastcall pandoras_i8039_read_port(UINT32 port)
{
	if ((port & 0x1ff) < 0x100) {
		return soundlatch2;
	}

	return 0;
}

static void __fastcall pandoras_i8039_write_port(UINT32 port, UINT8 data)
{
	switch (port & 0x1ff)
	{
		case I8039_p1:
			DACWrite(0, data);
		return;

		case I8039_p2:
			if ((data & 0x80) == 0) {
				I8039SetIrqState(CPU_IRQSTATUS_NONE);
			}
			i8039_status = (data >> 5) & 1;
		return;
	}
}

static UINT8 AY8910_0_port_A_Read(UINT32)
{
	return i8039_status;
}

static UINT8 AY8910_0_port_B_Read(UINT32)
{
	if (ZetGetActive() == -1) return 0;

	return (ZetTotalCycles() / 512) & 0xf;
}

static INT32 DrvSyncDAC()
{
	return (INT32)(float)(nBurnSoundLen * (ZetTotalCycles() / (1789772.0000 / (nBurnFPS / 100.0000))));
}

static INT32 DrvDoReset(INT32 clear_ram)
{
	if (clear_ram) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	M6809Open(0);
	M6809Reset();
	M6809Close();

	M6809Open(1);
	M6809Reset();
	M6809Close();

	ZetOpen(0);
	ZetReset();
	AY8910Reset(0);
	ZetClose();

	I8039Open(0);
	I8039Reset();
	I8039Close();

	DACReset();

	scrolly = 0;
	flipscreen = 0;

	irq_enable[0] = 0;
	irq_enable[1] = 0;
	firq_trigger[0] = 0;
	firq_trigger[1] = 0;

	soundlatch = 0;
	soundlatch2 = 0;
	i8039_status = 0;

	watchdog = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvMainROM		= Next; Next += 0x008000;
	DrvSubROM		= Next; Next += 0x002000;
	DrvZ80ROM		= Next; Next += 0x002000;
	DrvI8039ROM		= Next; Next += 0x002000;

	DrvGfxROM0		= Next; Next += 0x00c000;
	DrvGfxROM1		= Next; Next += 0x008000;

	DrvColPROM		= Next; Next += 0x000220;

	DrvPalette		= (UINT32*)Next; Next += 0x0200 * sizeof(UINT32);

	AllRam			= Next;

	DrvShareRAM		= Next; Next += 0x000800;
	DrvSprRAM		= Next; Next += 0x001000;
	DrvColRAM		= Next; Next += 0x000400;
	DrvVidRAM		= Next; Next += 0x000400;
	DrvZ80RAM		= Next; Next += 0x000400;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void DrvPaletteInit()
{
	UINT32 pens[32];

	for (INT32 i = 0; i < 0x20; i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 0x01;
		INT32 r = bit2 * 151 + bit1 * 71 + bit0 * 33;

		bit0 = (DrvColPROM[i] >> 3) & 0x01;
		bit1 = (DrvColPROM[i] >> 4) & 0x01;
		bit2 = (DrvColPROM[i] >> 5) & 0x01;
		INT32 g = bit2 * 151 + bit1 * 71 + bit0 * 33;

		bit0 = (DrvColPROM[i] >> 6) & 0x01;
		bit1 = (DrvColPROM[i] >> 7) & 0x01;
		INT32 b = bit1 * 174 + bit0 * 81;

		pens[i] = BurnHighCol(r,g,b,0);
	}

	for (INT32 i = 0; i < 0x200; i++) {
		DrvPalette[i] = pens[(DrvColPROM[i+0x020] & 0x0f) | ((i&0x100)>>4)];
	}
}

static void DrvGfxExpand(UINT8 *rom, UINT32 len)
{
	for (INT32 i = len-1; i >= 0; i--) {
		rom[i*2+0] = rom[i] >> 4;
		rom[i*2+1] = rom[i] & 0xf;
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
		if (BurnLoadRom(DrvMainROM  + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvMainROM  + 0x2000,  1, 1)) return 1;
		if (BurnLoadRom(DrvMainROM  + 0x4000,  2, 1)) return 1;
		if (BurnLoadRom(DrvMainROM  + 0x6000,  3, 1)) return 1;

		if (BurnLoadRom(DrvSubROM   + 0x0000,  4, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM   + 0x0000,  5, 1)) return 1;

		if (BurnLoadRom(DrvI8039ROM + 0x0000,  6, 1)) return 1;
	
		if (BurnLoadRom(DrvGfxROM0  + 0x0000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x2000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x4000,  9, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1  + 0x0000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x2000, 11, 1)) return 1;

		if (BurnLoadRom(DrvColPROM  + 0x0000, 12, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x0020, 13, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x0120, 14, 1)) return 1;

		DrvGfxExpand(DrvGfxROM0, 0x6000);
		DrvGfxExpand(DrvGfxROM1, 0x4000);
		DrvPaletteInit();
	}

	M6809Init(0);
	M6809Open(0);
	M6809MapMemory(DrvSprRAM,		0x0000, 0x0fff, MAP_RAM);
	M6809MapMemory(DrvColRAM,		0x1000, 0x13ff, MAP_RAM);
	M6809MapMemory(DrvVidRAM,		0x1400, 0x17ff, MAP_RAM);	
//	M6809MapMemory(DrvMainROM + 0x8000,	0x4000, 0x5fff, MAP_ROM);
	M6809MapMemory(DrvShareRAM,		0x6000, 0x67ff, MAP_RAM);
	M6809MapMemory(DrvMainROM,		0x8000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(pandoras_main_write);
//	M6809SetReadHandler(pandoras_main_read);
	M6809Close();

	M6809Init(1);
	M6809Open(1);
	M6809MapMemory(DrvSprRAM,		0x0000, 0x0fff, MAP_RAM);
	M6809MapMemory(DrvColRAM,		0x1000, 0x13ff, MAP_RAM);
	M6809MapMemory(DrvVidRAM,		0x1400, 0x17ff, MAP_RAM);
	M6809MapMemory(DrvShareRAM,		0xc000, 0xc7ff, MAP_RAM);
	M6809MapMemory(DrvSubROM,		0xe000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(pandoras_sub_write);
	M6809SetReadHandler(pandoras_sub_read);
	M6809Close();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,			0x2000, 0x23ff, MAP_RAM);
	ZetSetWriteHandler(pandoras_sound_write);
	ZetSetReadHandler(pandoras_sound_read);
	ZetClose();

	I8039Init(0);
	I8039Open(0);
	I8039SetProgramReadHandler(pandoras_i8039_read);
	I8039SetCPUOpReadHandler(pandoras_i8039_read);
	I8039SetCPUOpReadArgHandler(pandoras_i8039_read);
	I8039SetIOReadHandler(pandoras_i8039_read_port);
	I8039SetIOWriteHandler(pandoras_i8039_write_port);
	I8039Close();

	AY8910Init(0, 1789772, 0);
	AY8910SetPorts(0, &AY8910_0_port_A_Read, &AY8910_0_port_B_Read, NULL, NULL);
	AY8910SetAllRoutes(0, 0.40, BURN_SND_ROUTE_BOTH);

	DACInit(0, 0, 1, DrvSyncDAC);
	DACSetRoute(0, 0.25, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6809Exit();
	ZetExit();
	I8039Exit();

	AY8910Exit(0);
	DACExit();

	BurnFree (AllMem);

	return 0;
}

static void draw_layer(INT32 priority)
{
	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 attr  = DrvColRAM[offs];
		INT32 prio  = (attr >> 5) & 1;
		if (prio != priority) continue;

		INT32 code  = DrvVidRAM[offs] + ((attr & 0x10) << 4);
		INT32 color = attr & 0x0f;
		INT32 flipy = attr & 0x80;
		INT32 flipx = attr & 0x40;

		INT32 sx = (offs & 0x1f) * 8;
		INT32 sy = ((offs / 32) * 8) - scrolly;

		if (sy < -7) sy += 256;

		if (flipscreen)
		{
			sy = 248 - sy;
			sx = 248 - sx;
			flipx = !flipx;
			flipy = !flipy;
		}

		if (flipy) {
			if (flipx) {
				Render8x8Tile_FlipXY_Clip(pTransDraw, code, sx, sy - 16, color, 4, 0x100, DrvGfxROM1);
			} else {
				Render8x8Tile_FlipY_Clip(pTransDraw, code, sx, sy - 16, color, 4, 0x100, DrvGfxROM1);
			}
		} else {
			if (flipx) {
				Render8x8Tile_FlipX_Clip(pTransDraw, code, sx, sy - 16, color, 4, 0x100, DrvGfxROM1);
			} else {
				Render8x8Tile_Clip(pTransDraw, code, sx, sy - 16, color, 4, 0x100, DrvGfxROM1);
			}
		}
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0; offs < 0x100; offs += 4)
	{
		INT32 sx    = DrvSprRAM[0x800 + offs + 1];
		INT32 sy    = 240 - DrvSprRAM[0x800 + offs];
		INT32 color = DrvSprRAM[0x800 + offs + 3] & 0x0f;
		INT32 flipx = DrvSprRAM[0x800 + offs + 3] & 0x40;
		INT32 flipy = DrvSprRAM[0x800 + offs + 3] & 0x80;
		INT32 code  = DrvSprRAM[0x800 + offs + 2];

		RenderTileTranstab(pTransDraw, DrvGfxROM0, code, color * 16, 0, sx, sy - 16, flipx, flipy, 16, 16, DrvColPROM + 0x020);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	draw_layer(1);
	draw_sprites();
	draw_layer(0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	watchdog++;
	if (watchdog >= 180) {
		DrvDoReset(0);
	//	bprintf (0, _T("Watchdog triggered!\n"));
	}

	if (DrvReset) {
		DrvDoReset(1);
	}

	M6809NewFrame();
	ZetNewFrame();
	I8039NewFrame();

	{
		memset (DrvInputs, 0xff, 3);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
 			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nSoundBufferPos = 0;
	INT32 nInterleave = 100;
	INT32 nCyclesTotal[4] = { 3072000 / 60, 3072000 / 60, 1789772 / 60, 477272 / 60 };
	INT32 nCyclesDone[4] = { 0, 0, 0, 0 };

	ZetOpen(0);
	I8039Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		M6809Open(0);
		INT32 nSegment = (nCyclesTotal[0] * (i + 1)) / nInterleave;
		nCyclesDone[0] += M6809Run(nSegment - nCyclesDone[0]);
		if (i == (nInterleave - 1) && irq_enable[0]) M6809SetIRQLine(0, CPU_IRQSTATUS_ACK);
		nSegment = M6809TotalCycles();
		M6809Close();

		M6809Open(1);
		nCyclesDone[1] += M6809Run(nSegment - M6809TotalCycles());
		if (i == (nInterleave - 1) && irq_enable[1]) M6809SetIRQLine(0, CPU_IRQSTATUS_ACK);
		M6809Close();

		nSegment = (nCyclesTotal[2] * (i + 1)) / nInterleave;
		nCyclesDone[2] += ZetRun(nSegment - nCyclesDone[2]);

		nSegment = (nCyclesTotal[3] * (i + 1)) / nInterleave;
		nCyclesDone[3] += I8039Run(nSegment - nCyclesDone[3]);

		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			AY8910Render(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			AY8910Render(pSoundBuf, nSegmentLength);
		}

		DACUpdate(pBurnSoundOut, nBurnSoundLen);
	}

	I8039Close();
	ZetClose();

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
		ZetScan(nAction);
		I8039Scan(nAction,pnMin);

		AY8910Scan(nAction, pnMin);
		DACScan(nAction, pnMin);

		SCAN_VAR(soundlatch);
		SCAN_VAR(soundlatch2);
		SCAN_VAR(flipscreen);
		SCAN_VAR(scrolly);

		SCAN_VAR(irq_enable[0]);
		SCAN_VAR(irq_enable[1]);
		SCAN_VAR(firq_trigger[0]);
		SCAN_VAR(firq_trigger[1]);
	}

	return 0;
}


// Pandora's Palace

static struct BurnRomInfo pandorasRomDesc[] = {
	{ "pand_j13.cpu",	0x2000, 0x7a0fe9c5, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "pand_j12.cpu",	0x2000, 0x7dc4bfe1, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "pand_j10.cpu",	0x2000, 0xbe3af3b7, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "pand_j9.cpu",	0x2000, 0xe674a17a, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "pand_j5.cpu",	0x2000, 0x4aab190b, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 #1 Code

	{ "pand_6c.snd",	0x2000, 0x0c1f109d, 3 | BRF_PRG | BRF_ESS }, //  5 Z80 Code

	{ "pand_7e.snd",	0x2000, 0x1071c1ba, 4 | BRF_GRA },           //  6 i8039 Code

	{ "pand_j18.cpu",	0x2000, 0x99a696c5, 5 | BRF_GRA },           //  7 Sprites
	{ "pand_j17.cpu",	0x2000, 0x38a03c21, 5 | BRF_GRA },           //  8
	{ "pand_j16.cpu",	0x2000, 0xe0708a78, 5 | BRF_GRA },           //  9

	{ "pand_a18.cpu",	0x2000, 0x23706d4a, 6 | BRF_GRA },           // 10 Characters
	{ "pand_a19.cpu",	0x2000, 0xa463b3f9, 6 | BRF_GRA },           // 11

	{ "pandora.2a",		0x0020, 0x4d56f939, 7 | BRF_GRA },           // 12 Color PROMs
	{ "pandora.17g",	0x0100, 0xc1a90cfc, 7 | BRF_GRA },           // 13
	{ "pandora.16b",	0x0100, 0xc89af0c3, 7 | BRF_GRA },           // 14
};

STD_ROM_PICK(pandoras)
STD_ROM_FN(pandoras)

struct BurnDriver BurnDrvPandoras = {
	"pandoras", NULL, NULL, NULL, "1984",
	"Pandora's Palace\0", NULL, "Konami / Interlogic", "GX328",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_PLATFORM, 0,
	NULL, pandorasRomInfo, pandorasRomName, NULL, NULL, NULL, NULL, PandorasInputInfo, PandorasDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};
