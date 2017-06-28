// FB Alpha Juno First driver module
// Based on MAME driver by Chris Hardy

#include "tiles_generic.h"
#include "z80_intf.h"
#include "m6809_intf.h"
#include "i8039.h"
#include "driver.h"
#include "flt_rc.h"
#include "dac.h"
extern "C" {
#include "ay8910.h"
}

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *RamEnd;
static UINT8 *AllRam;
static UINT8 *DrvM6809ROM;
static UINT8 *DrvM6809Dec;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvI8039ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvVidRAM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvM6809RAM;
static UINT8 *DrvPalRAM;

static UINT8 *blitterdata;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT16 *pAY8910Buffer[3];

static UINT8 soundlatch;
static UINT8 soundlatch2;
static UINT8 i8039_status;
static UINT8 irq_enable;
static UINT8 irq_toggle;
static UINT8 scroll;
static UINT8 flipscreen;
static UINT8 previous_sound_irq;
static UINT8 bankdata;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;
static INT32 watchdog;

static struct BurnInputInfo JunofrstInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Junofrst)

static struct BurnDIPInfo JunofrstDIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL			},
	{0x15, 0xff, 0xff, 0x7b, NULL			},

	{0   , 0xfe, 0   ,    16, "Coin A"		},
	{0x14, 0x01, 0x0f, 0x02, "4 Coins 1 Credits"	},
	{0x14, 0x01, 0x0f, 0x05, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x14, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x14, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x14, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x14, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x14, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x14, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x14, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x14, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x14, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x14, 0x01, 0x0f, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    16, "Coin B"		},
	{0x14, 0x01, 0xf0, 0x20, "4 Coins 1 Credits"	},
	{0x14, 0x01, 0xf0, 0x50, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"	},
	{0x14, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"	},
	{0x14, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"	},
	{0x14, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"	},
	{0x14, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x14, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x14, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x14, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x14, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x14, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x14, 0x01, 0xf0, 0x00, "No Coin B"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x15, 0x01, 0x03, 0x03, "3"			},
	{0x15, 0x01, 0x03, 0x02, "4"			},
	{0x15, 0x01, 0x03, 0x01, "5"			},
	{0x15, 0x01, 0x03, 0x00, "256 (Cheat)"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x15, 0x01, 0x04, 0x00, "Upright"		},
	{0x15, 0x01, 0x04, 0x04, "Cocktail"		},

	{0   , 0xfe, 0   ,    8, "Difficulty"		},
	{0x15, 0x01, 0x70, 0x70, "1 (Easiest)"		},
	{0x15, 0x01, 0x70, 0x60, "2"			},
	{0x15, 0x01, 0x70, 0x50, "3"			},
	{0x15, 0x01, 0x70, 0x40, "4"			},
	{0x15, 0x01, 0x70, 0x30, "5"			},
	{0x15, 0x01, 0x70, 0x20, "6"			},
	{0x15, 0x01, 0x70, 0x10, "7"			},
	{0x15, 0x01, 0x70, 0x00, "8 (Hardest)"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x15, 0x01, 0x80, 0x80, "Off"			},
	{0x15, 0x01, 0x80, 0x00, "On"			}
};

STDDIPINFO(Junofrst)

static void blitter_write(INT32 offset, UINT8 data)
{
	blitterdata[offset] = data;

	if (offset == 3)
	{
		UINT8 *gfx_rom = DrvGfxROM0;

		UINT16 src = ((blitterdata[2] << 8) | blitterdata[3]) & 0xfffc;
		UINT16 dest = (blitterdata[0] << 8) | blitterdata[1];

		INT32 copy = blitterdata[3] & 0x01;

		for (INT32 i = 0; i < 16; i++)
		{
			for (INT32 j = 0; j < 16; j++)
			{
				UINT8 gfxdata = (gfx_rom[src / 2] >> ((~src & 1) * 4)) & 0xf;

				src += 1;

				if (gfxdata)
				{
					if (copy == 0) gfxdata = 0;

					if (dest & 1)
						DrvVidRAM[dest / 2] = (DrvVidRAM[dest / 2] & 0x0f) + (gfxdata << 4);
					else
						DrvVidRAM[dest / 2] = (DrvVidRAM[dest / 2] & 0xf0) + (gfxdata);
				}

				dest += 1;
			}

			dest += 240;
		}
	}
}

static void bankswitch(INT32 data)
{
	bankdata = data;

	M6809MapMemory(DrvM6809ROM + 0x10000 + ((data & 0xf) * 0x1000), 0x9000, 0x9fff, MAP_READ);
	M6809MapMemory(DrvM6809Dec + 0x10000 + ((data & 0xf) * 0x1000), 0x9000, 0x9fff, MAP_FETCH);
}

static void junofrst_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfff0) == 0x8000) {
		DrvPalRAM[address & 0xf] = data;
		return;
	}

	switch (address)
	{
		case 0x8030:
			irq_enable = data & 1;
			if (irq_enable == 0) M6809SetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;

		case 0x8031:
		case 0x8032:
			// coin counter
		return;

		case 0x8033:
			scroll = data;
		return;

		case 0x8034:
		case 0x8035:
			flipscreen = data & 1;
		return;

		case 0x8040:
			if (previous_sound_irq == 0 && data == 1) {
				ZetSetVector(0xff);
				ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
			}
			previous_sound_irq = data;
		return;

		case 0x8050:
			soundlatch = data;
		return;

		case 0x8060:
			bankswitch(data);
		return;

		case 0x8070:
		case 0x8071:
		case 0x8072:
		case 0x8073:
			blitter_write(address & 3, data);
		return;
	}
}

static UINT8 junofrst_main_read(UINT16 address)
{
	switch (address)
	{
		case 0x8010:
			return DrvDips[1];

		case 0x801c:
			watchdog = 0;
			return 0;

		case 0x8020:
			return DrvInputs[0];

		case 0x8024:
			return DrvInputs[1];

		case 0x8028:
			return DrvInputs[2];

		case 0x802c:
			return DrvDips[0];
	}

	return 0;
}

static void __fastcall junofrst_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x4000:
			AY8910Write(0, 0, data);
		return;

		case 0x4002:
			AY8910Write(0, 1, data);
		return;

		case 0x5000:
			soundlatch2 = data;
		return;

		case 0x6000:
			I8039SetIrqState(1);
		return;
	}
}

static UINT8 __fastcall junofrst_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x3000:
			return soundlatch;

		case 0x4001:
			return AY8910Read(0);
	}

	return 0;
}

static UINT8 __fastcall junofrst_i8039_read(UINT32 address)
{
	return DrvI8039ROM[address & 0x0fff];
}

static void __fastcall junofrst_i8039_write_port(UINT32 port, UINT8 data)
{
	switch (port & 0x1ff)
	{
		case I8039_p1:
			DACWrite(0, data);
		return;

		case I8039_p2:
			if ((data & 0x80) == 0)
				I8039SetIrqState(0);
			i8039_status = (data & 0x70) >> 4;
		return;
	}
}

static UINT8 __fastcall junofrst_i8039_read_port(UINT32 port)
{
	if ((port & 0x1ff) < 0x100) {
		return soundlatch2;
	}

	return 0;
}

static UINT8 AY8910_0_portA(UINT32)
{
	return (((ZetTotalCycles() / 512) & 0xf) * 0x10) + i8039_status;
}

static void filter_write(INT32 num, UINT8 d)
{
	INT32 C = 0;
	if (d & 2) C += 220000;
	if (d & 1) C +=  47000;

	filter_rc_set_RC(num, FLT_RC_LOWPASS, 1000, 2200, 200, CAP_P(C));
}

static void AY8910_0_portBwrite(UINT32 /*port*/, UINT32 data)
{
	if (ZetGetActive() == -1) return;

	filter_write(0, (data >> 0) & 3);
	filter_write(1, (data >> 2) & 3);
	filter_write(2, (data >> 4) & 3);
}

static INT32 DrvSyncDAC()
{
	return (INT32)(float)(nBurnSoundLen * (ZetTotalCycles() / (1789750.0000 / (nBurnFPS / 100.0000))));
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);

		soundlatch = 0;
		soundlatch2 = 0;
		i8039_status = 0;
		irq_enable = 0;
		irq_toggle = 0;
		scroll = 0;
		flipscreen = 0;
		previous_sound_irq = 0;
	}

	M6809Open(0);
	bankswitch(0);
	M6809Reset();
	M6809Close();

	ZetOpen(0);
	ZetReset();
	ZetClose();

	I8039Reset();

	DACReset();

	AY8910Reset(0);

	watchdog = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6809ROM		= Next; Next += 0x020000;
	DrvM6809Dec		= Next; Next += 0x020000;

	DrvZ80ROM		= Next; Next += 0x001000;

	DrvI8039ROM		= Next; Next += 0x001000;

	DrvGfxROM0		= Next; Next += 0x008000;

	DrvPalette		= (UINT32*)Next; Next += 0x0010 * sizeof(UINT32);

	AllRam			= Next;

	DrvVidRAM		= Next; Next += 0x008000;
	DrvZ80RAM		= Next; Next += 0x000400;
	DrvPalRAM       = Next; Next += 0x000010;

	DrvM6809RAM		= Next;	Next += 0x000f00;

	blitterdata		= Next; Next += 0x000004;

	RamEnd			= Next;

	pAY8910Buffer[0]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[1]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[2]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);

	MemEnd			= Next;

	return 0;
}

static void M6809Decode()
{
	for (INT32 i = 0; i < 0x20000; i++)
	{
		DrvM6809Dec[i] = DrvM6809ROM[i] ^ ((i & 2) ? 0x80 : 0x20) ^ ((i & 8) ? 0x08 : 0x02);
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
		if (BurnLoadRom(DrvM6809ROM + 0x0a000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x0c000,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x0e000,  2, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x10000,  3, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x12000,  4, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x14000,  5, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x16000,  6, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x18000,  7, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x1a000,  8, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM   + 0x00000,  9, 1)) return 1;

		if (BurnLoadRom(DrvI8039ROM + 0x00000, 10, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0  + 0x00000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x02000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x04000, 13, 1)) return 1;

		M6809Decode();
	}

	M6809Init(1);
	M6809Open(0);
	M6809MapMemory(DrvVidRAM,		0x0000, 0x7fff, MAP_RAM);
	M6809MapMemory(DrvM6809RAM,		0x8100, 0x8fff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM + 0xa000,	0xa000, 0xffff, MAP_READ);
	M6809MapMemory(DrvM6809Dec + 0xa000,	0xa000, 0xffff, MAP_FETCH);
	M6809SetWriteHandler(junofrst_main_write);
	M6809SetReadHandler(junofrst_main_read);
	M6809Close();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x0fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,			0x2000, 0x23ff, MAP_RAM);
	ZetSetWriteHandler(junofrst_sound_write);
	ZetSetReadHandler(junofrst_sound_read);
	ZetClose();

	I8039Init(NULL);
	I8039SetProgramReadHandler(junofrst_i8039_read);
	I8039SetCPUOpReadHandler(junofrst_i8039_read);
	I8039SetCPUOpReadArgHandler(junofrst_i8039_read);
	I8039SetIOReadHandler(junofrst_i8039_read_port);
	I8039SetIOWriteHandler(junofrst_i8039_write_port);

	DACInit(0, 0, 1, DrvSyncDAC);
	DACSetRoute(0, 0.50, BURN_SND_ROUTE_BOTH);

	AY8910Init(0, 1789750, nBurnSoundRate, AY8910_0_portA, NULL, NULL, &AY8910_0_portBwrite);
	AY8910SetAllRoutes(0, 0.15, BURN_SND_ROUTE_BOTH);

	filter_rc_init(0, FLT_RC_LOWPASS, 1000, 2200, 200, CAP_P(0), 0);
	filter_rc_init(1, FLT_RC_LOWPASS, 1000, 2200, 200, CAP_P(0), 1);
	filter_rc_init(2, FLT_RC_LOWPASS, 1000, 2200, 200, CAP_P(0), 1);
	filter_rc_set_src_gain(0, 1.00);
	filter_rc_set_src_gain(1, 1.00);
	filter_rc_set_src_gain(2, 1.00);
	filter_rc_set_route(0, 0.30, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(1, 0.30, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(2, 0.30, BURN_SND_ROUTE_BOTH);

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

	DACExit();

	AY8910Exit(0);
	filter_rc_exit();

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteUpdate()
{
	for (INT32 i = 0; i < 16; i++)		// BBGGGRRR
	{
		INT32 r = (DrvPalRAM[i] & 0x07);
		INT32 g = (DrvPalRAM[i] & 0x38) >> 3;
		INT32 b = (DrvPalRAM[i] & 0xc0) >> 6;

		r  = (r << 5) + (r << 2) + (r >> 1);
		g  = (g << 5) + (g << 2) + (g >> 1);
		b += (b << 2) + (b << 4) + (b << 6);

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_bitmap()
{
	int xorx = flipscreen ? 255 : 0;
	int xory = flipscreen ? 255 : 0;

	for (int y = 16; y < 240; y++)
	{
		UINT16 *dst = pTransDraw + ((y - 16) * nScreenWidth);

		for (int x = 0; x < 256; x++)
		{
			UINT8 effx = x ^ xorx;
			UINT8 effy = (y ^ xory) + ((effx < 192) ? scroll : 0);

			dst[x] = (DrvVidRAM[effy * 128 + effx / 2] >> (4 * (effx % 2))) & 0xf;
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	}

	draw_bitmap();

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

	{
		memset (DrvInputs, 0xff, 3);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
 			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	ZetNewFrame();
	I8039NewFrame();

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[3] = { 1536000 / 60, 1789750 / 60, 8000000 / 15 / 60 };
	INT32 nCyclesDone[3] = { 0, 0, 0 };
	INT32 nSoundBufferPos = 0;

	M6809Open(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		INT32 nNext;

		nNext = (nCyclesTotal[0] * (i + 1)) / nInterleave;
		nCyclesDone[0] += M6809Run(nNext - nCyclesDone[0]);
		if (i == 240-1 && (irq_toggle ^= 1) && irq_enable) {
			M6809SetIRQLine(0, CPU_IRQSTATUS_ACK);
		}

		nNext = (nCyclesTotal[1] * (i + 1)) / nInterleave;
		nCyclesDone[1] += ZetRun(nNext - nCyclesDone[1]);

		nNext = (nCyclesTotal[2] * (i + 1)) / nInterleave;
		nCyclesDone[2] += I8039Run(nNext - nCyclesDone[2]);

		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			AY8910Render(&pAY8910Buffer[0], pSoundBuf, nSegmentLength, 0);
			nSoundBufferPos += nSegmentLength;
			filter_rc_update(0, pAY8910Buffer[0], pSoundBuf, nSegmentLength);
			filter_rc_update(1, pAY8910Buffer[1], pSoundBuf, nSegmentLength);
			filter_rc_update(2, pAY8910Buffer[2], pSoundBuf, nSegmentLength);
		}
	}

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			AY8910Render(&pAY8910Buffer[0], pSoundBuf, nSegmentLength, 0);

			filter_rc_update(0, pAY8910Buffer[0], pSoundBuf, nSegmentLength);
			filter_rc_update(1, pAY8910Buffer[1], pSoundBuf, nSegmentLength);
			filter_rc_update(2, pAY8910Buffer[2], pSoundBuf, nSegmentLength);
		}
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
	}

	M6809Close();
	ZetClose();

	if (pBurnDraw) {
		DrvDraw();
	}

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
		ZetScan(nAction);
		M6809Scan(nAction);
		I8039Scan(nAction,pnMin);

		AY8910Scan(nAction, pnMin);
		DACScan(nAction, pnMin);

		SCAN_VAR(soundlatch);
		SCAN_VAR(soundlatch2);
		SCAN_VAR(i8039_status);
		SCAN_VAR(irq_enable);
		SCAN_VAR(irq_toggle);
		SCAN_VAR(scroll);
		SCAN_VAR(flipscreen);
		SCAN_VAR(previous_sound_irq);
		SCAN_VAR(bankdata);
	}

	if (nAction & ACB_WRITE) {
		M6809Open(0);
		bankswitch(bankdata);
		M6809Close();
	}

	return 0;
}


// Juno First

static struct BurnRomInfo junofrstRomDesc[] = {
	{ "jfa_b9.bin",		0x2000, 0xf5a7ab9d, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "jfb_b10.bin",	0x2000, 0xf20626e0, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "jfc_a10.bin",	0x2000, 0x1e7744a7, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "jfc1_a4.bin",	0x2000, 0x03ccbf1d, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "jfc2_a5.bin",	0x2000, 0xcb372372, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "jfc3_a6.bin",	0x2000, 0x879d194b, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "jfc4_a7.bin",	0x2000, 0xf28af80b, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "jfc5_a8.bin",	0x2000, 0x0539f328, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "jfc6_a9.bin",	0x2000, 0x1da2ad6e, 1 | BRF_PRG | BRF_ESS }, //  8

	{ "jfs1_j3.bin",	0x1000, 0x235a2893, 2 | BRF_PRG | BRF_ESS }, //  9 Z80 Code

	{ "jfs2_p4.bin",	0x1000, 0xd0fa5d5f, 3 | BRF_PRG | BRF_ESS }, // 10 I8039 Code

	{ "jfs3_c7.bin",	0x2000, 0xaeacf6db, 4 | BRF_GRA },           // 11 Blitter data
	{ "jfs4_d7.bin",	0x2000, 0x206d954c, 4 | BRF_GRA },           // 12
	{ "jfs5_e7.bin",	0x2000, 0x1eb87a6e, 4 | BRF_GRA },           // 13
};

STD_ROM_PICK(junofrst)
STD_ROM_FN(junofrst)

struct BurnDriver BurnDrvJunofrst = {
	"junofrst", NULL, NULL, NULL, "1983",
	"Juno First\0", NULL, "Konami", "GX310",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_PREFIX_KONAMI, GBF_VERSHOOT, 0,
	NULL, junofrstRomInfo, junofrstRomName, NULL, NULL, JunofrstInputInfo, JunofrstDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	224, 256, 3, 4
};


// Juno First (Gottlieb)

static struct BurnRomInfo junofrstgRomDesc[] = {
	{ "jfg_a.9b",		0x2000, 0x8f77d1c5, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "jfg_b.10b",		0x2000, 0xcd645673, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "jfg_c.10a",		0x2000, 0x47852761, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "jfgc1.4a",		0x2000, 0x90a05ae6, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "jfc2_a5.bin",	0x2000, 0xcb372372, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "jfc3_a6.bin",	0x2000, 0x879d194b, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "jfgc4.7a",		0x2000, 0xe8864a43, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "jfc5_a8.bin",	0x2000, 0x0539f328, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "jfc6_a9.bin",	0x2000, 0x1da2ad6e, 1 | BRF_PRG | BRF_ESS }, //  8

	{ "jfs1_j3.bin",	0x1000, 0x235a2893, 2 | BRF_PRG | BRF_ESS }, //  9 Z80 Code

	{ "jfs2_p4.bin",	0x1000, 0xd0fa5d5f, 3 | BRF_PRG | BRF_ESS }, // 10 I8039 Code

	{ "jfs3_c7.bin",	0x2000, 0xaeacf6db, 4 | BRF_GRA },           // 11 Blitter data
	{ "jfs4_d7.bin",	0x2000, 0x206d954c, 4 | BRF_GRA },           // 12
	{ "jfs5_e7.bin",	0x2000, 0x1eb87a6e, 4 | BRF_GRA },           // 13
};

STD_ROM_PICK(junofrstg)
STD_ROM_FN(junofrstg)

struct BurnDriver BurnDrvJunofrstg = {
	"junofrstg", "junofrst", NULL, NULL, "1983",
	"Juno First (Gottlieb)\0", NULL, "Konami (Gottlieb license)", "GX310",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_PREFIX_KONAMI, GBF_VERSHOOT, 0,
	NULL, junofrstgRomInfo, junofrstgRomName, NULL, NULL, JunofrstInputInfo, JunofrstDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	224, 256, 3, 4
};

