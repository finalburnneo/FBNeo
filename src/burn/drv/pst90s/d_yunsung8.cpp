// FB Alpha Yun Sung 8 Bit Games driver module
// Based on MAME driver by Luca Elia

#include "tiles_generic.h"
#include "z80_intf.h"
#include "burn_ym3812.h"
#include "msm5205.h"
#include "burn_pal.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvVidRAM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;

static UINT8 DrvRecalc;

static UINT8 bankdata[3];
static UINT8 soundlatch;
static UINT8 flipscreen;
static INT32 adpcm_toggle;
static UINT8 adpcm_data;

static UINT16 palrambank;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo RocktrisInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 fire 2"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 7,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 5,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Rocktris)

static struct BurnInputInfo CannballInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 3"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 7,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 5,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Cannball)

static struct BurnDIPInfo RocktrisDIPList[]=
{
	{0x11, 0xff, 0xff, 0xef, NULL			},
	{0x12, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x11, 0x01, 0x01, 0x01, "Off"			},
	{0x11, 0x01, 0x01, 0x00, "On"			},
	
	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x11, 0x01, 0x06, 0x00, "Easy"			},
	{0x11, 0x01, 0x06, 0x06, "Normal"		},
	{0x11, 0x01, 0x06, 0x04, "Hard"			},
	{0x11, 0x01, 0x06, 0x02, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x11, 0x01, 0x08, 0x08, "Off"			},
	{0x11, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x11, 0x01, 0x10, 0x10, "Off"			},
	{0x11, 0x01, 0x10, 0x00, "On"			},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x11, 0x01, 0xe0, 0x00, "4 Coins 1 Credits"	},
	{0x11, 0x01, 0xe0, 0x20, "3 Coins 1 Credits"	},
	{0x11, 0x01, 0xe0, 0x60, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0xe0, 0x40, "2 Coins 3 Credits"	},
	{0x11, 0x01, 0xe0, 0xc0, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"	},
	{0x11, 0x01, 0xe0, 0x80, "1 Coin  4 Credits"	},
};

STDDIPINFO(Rocktris)

static struct BurnDIPInfo MagixDIPList[]=
{
	{0x11, 0xff, 0xff, 0xef, NULL			},
	{0x12, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x11, 0x01, 0x01, 0x01, "Off"			},
	{0x11, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x11, 0x01, 0x06, 0x00, "Easy"			},
	{0x11, 0x01, 0x06, 0x06, "Normal"		},
	{0x11, 0x01, 0x06, 0x04, "Hard"			},
	{0x11, 0x01, 0x06, 0x02, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x11, 0x01, 0x08, 0x08, "Off"			},
	{0x11, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x11, 0x01, 0x10, 0x10, "Off"			},
	{0x11, 0x01, 0x10, 0x00, "On"			},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x11, 0x01, 0xe0, 0x00, "4 Coins 1 Credits"	},
	{0x11, 0x01, 0xe0, 0x20, "3 Coins 1 Credits"	},
	{0x11, 0x01, 0xe0, 0x60, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0xe0, 0x40, "2 Coins 3 Credits"	},
	{0x11, 0x01, 0xe0, 0xc0, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"	},
	{0x11, 0x01, 0xe0, 0x80, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    2, "Title"		},
	{0x12, 0x01, 0x01, 0x01, "Magix"		},
	{0x12, 0x01, 0x01, 0x00, "Rock"			},
};

STDDIPINFO(Magix)

static struct BurnDIPInfo CannballDIPList[]=
{
	{0x13, 0xff, 0xff, 0xef, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x13, 0x01, 0x01, 0x01, "Off"			},
	{0x13, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x06, 0x00, "Easy"			},
	{0x13, 0x01, 0x06, 0x06, "Normal"		},
	{0x13, 0x01, 0x06, 0x04, "Hard"			},
	{0x13, 0x01, 0x06, 0x02, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x08, 0x08, "Off"			},
	{0x13, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x10, 0x10, "Off"			},
	{0x13, 0x01, 0x10, 0x00, "On"			},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x13, 0x01, 0xe0, 0x00, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0x20, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0x60, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xe0, 0x40, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0xe0, 0xc0, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0xe0, 0x80, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    4, "Bombs"		},
	{0x14, 0x01, 0x0c, 0x04, "1"			},
	{0x14, 0x01, 0x0c, 0x08, "2"			},
	{0x14, 0x01, 0x0c, 0x0c, "3"			},
	{0x14, 0x01, 0x0c, 0x00, "4"			},
};

STDDIPINFO(Cannball)

static inline void palette_write(INT32 offset)
{
	UINT16 c = BurnPalRAM[offset + 0] + (BurnPalRAM[offset + 1] * 256);

	BurnPalette[offset/2] = BurnHighCol(pal5bit(c), pal5bit(c >> 5), pal5bit(c >> 10), 0);
}

static void bankswitch(INT32 data)
{
	bankdata[0] = data;

	ZetMapMemory(DrvZ80ROM0 + ((data & 7) * 0x4000), 0x8000, 0xbfff, MAP_ROM);
}

static void rambank(INT32 data)
{
	bankdata[1] = data;

	palrambank = (data & 2) * 0x400;

	ZetMapMemory(BurnPalRAM + palrambank          , 0xc000, 0xc7ff, MAP_ROM);
	ZetMapMemory(DrvVidRAM + ((data & 1) * 0x1800), 0xc800, 0xdfff, MAP_RAM);
}

static void __fastcall yunsung8_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf800) == 0xc000) {
		INT32 offset = palrambank + (address & 0x7ff);
		BurnPalRAM[offset] = data;
		palette_write(offset & ~1);
		return;
	}

	switch (address)
	{
		case 0x0001:
			bankswitch(data);
		return;
	}
}

static void __fastcall yunsung8_main_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			rambank(data);
		return;

		case 0x01:
			bankswitch(data);
		return;

		case 0x02:
			soundlatch = data;
			ZetSetIRQLine(1, 0, CPU_IRQSTATUS_ACK);
		return;

		case 0x06:
			flipscreen = data & 0x01;
		return;

		case 0x07:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;
	}
}

static UINT8 __fastcall yunsung8_main_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
		case 0x02:
			return DrvInputs[port & 3];

		case 0x03:
		case 0x04:
			return DrvDips[(port - 1) & 1];
	}

	return 0;
}

static void sound_bankswitch(INT32 data)
{
	bankdata[2] = data;

	MSM5205ResetWrite(0, (data >> 5) & 1);

	ZetMapMemory(DrvZ80ROM1 + ((data & 7) * 0x4000), 0x8000, 0xbfff, MAP_ROM);
}

static void __fastcall yunsung8_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xe000:
			sound_bankswitch(data);
		return;

		case 0xe400:
			adpcm_data = ((data&0xf)<<4) | ((data >>4)&0xf);
		return;

		case 0xec00:
		case 0xec01:
			BurnYM3812Write(0, address & 1, data);
		return;
	}
}

static UINT8 __fastcall yunsung8_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xf800:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			return soundlatch;
	}

	return 0;
}

static tilemap_callback( background )
{
	UINT16 code = DrvVidRAM[0x2000 + (offs * 2)] + (DrvVidRAM[0x2001 + (offs * 2)] * 256);
	UINT8 color = DrvVidRAM[0x1800 + offs];

	TILE_SET_INFO(0, code, color, 0);
}

static tilemap_callback( foreground )
{
	UINT16 code = DrvVidRAM[0x0800 + (offs * 2)] + (DrvVidRAM[0x0801 + (offs * 2)] * 256);
	UINT8 color = DrvVidRAM[0x0000 + offs];

	TILE_SET_INFO(1, code, color, 0);
}

static void DrvMSM5205Int()
{
	MSM5205DataWrite(0, adpcm_data >> 4);
	adpcm_data <<= 4;
	adpcm_toggle = !adpcm_toggle;

	if (adpcm_toggle) ZetNmi();
}

inline static INT32 DrvMSM5205SynchroniseStream(INT32 nSoundRate)
{
	return (INT64)((double)ZetTotalCycles() * nSoundRate) / 4000000;
}

static INT32 DrvSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)ZetTotalCycles() * nSoundRate / 4000000;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	bankswitch(2);
	rambank(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	sound_bankswitch(0);
	ZetReset();
	BurnYM3812Reset();
	MSM5205Reset();
	ZetClose();

	soundlatch = 0;
	flipscreen = 0;
	adpcm_toggle = 0;
	adpcm_data = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x020000;
	DrvZ80ROM1		= Next; Next += 0x020000;

	DrvGfxROM0		= Next; Next += 0x200000;
	DrvGfxROM1		= Next; Next += 0x080000;

	BurnPalette		= (UINT32*)Next; Next += 0x800 * sizeof(UINT32);

	AllRam			= Next;

	DrvVidRAM		= Next; Next += 0x003000;
	BurnPalRAM		= Next; Next += 0x001000;
	BurnPalRAM		= Next; Next += 0x001000;
	DrvZ80RAM0		= Next; Next += 0x002000;
	DrvZ80RAM1		= Next; Next += 0x000800;

	RamEnd			= Next;
	MemEnd			= Next;

	return 0;
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
		if (BurnLoadRom(DrvZ80ROM0 + 0x000000,  0, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x000000,  1, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  2, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000001,  3, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000002,  4, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000003,  5, 4)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000001,  6, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  7, 2)) return 1;

		for (INT32 i = 0x40000-1; i >= 0; i--) {
			DrvGfxROM1[i*2+1] = DrvGfxROM1[i] >> 4;
			DrvGfxROM1[i*2+0] = DrvGfxROM1[i] & 0xf;
		}
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,	0xe000, 0xffff, MAP_RAM);
	ZetSetWriteHandler(yunsung8_main_write);
	ZetSetOutHandler(yunsung8_main_write_port);
	ZetSetInHandler(yunsung8_main_read_port);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvVidRAM,		0xc000, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM1,	0xf000, 0xf7ff, MAP_RAM);
	ZetSetWriteHandler(yunsung8_sound_write);
	ZetSetReadHandler(yunsung8_sound_read);
	ZetClose();

	BurnYM3812Init(1, 4000000, NULL, &DrvSynchroniseStream, 0);
	BurnTimerAttachYM3812(&ZetConfig, 4000000);
	BurnYM3812SetRoute(0, BURN_SND_YM3812_ROUTE, 1.00, BURN_SND_ROUTE_BOTH);

	MSM5205Init(0, DrvMSM5205SynchroniseStream, 400000, DrvMSM5205Int, MSM5205_S96_4B, 1);
	MSM5205SetRoute(0, 0.80, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, background_map_callback, 8, 8, 64, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, foreground_map_callback, 8, 8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 8, 8, 8, 0x200000, 0, 0x07);
	GenericTilemapSetGfx(1, DrvGfxROM1, 4, 8, 8, 0x080000, 0, 0x3f);
	GenericTilemapSetOffsets(TMAP_GLOBAL, -64, -8);
	GenericTilemapSetTransparent(1, 0);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	MSM5205Exit();
	BurnYM3812Exit();

	ZetExit();
	GenericTilesExit();

	BurnFree (AllMem);

	return 0;
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x1000; i+=2) {
			palette_write(i);
		}
		DrvRecalc = 0;
	}

	GenericTilemapSetFlip(TMAP_GLOBAL, flipscreen ? TMAP_FLIPXY : 0);

	INT32 layerctrl = (~(bankdata[0] & 0x30) >> 4);

	if (layerctrl & 1) {
		GenericTilemapDraw(0, pTransDraw, 0);
	} else {
		BurnTransferClear();
	}

	if (layerctrl & 2) {
		GenericTilemapDraw(1, pTransDraw, 0);
	}
	BurnTransferCopy(BurnPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= DrvJoy1[i] << i;
			DrvInputs[1] ^= DrvJoy2[i] << i;
			DrvInputs[2] ^= DrvJoy3[i] << i;
		}

		// Cannon Ball (vertical) needs this
		if (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL) {
			DrvInputs[0] &= ~0x40;
		}
	}

	INT32 nCyclesTotal[2] = { 8000000 / 60, 4000000 / 60 };
	INT32 nInterleave = MSM5205CalcInterleave(0, 4000000);
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);
		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		ZetClose();

		ZetOpen(1);
		BurnTimerUpdateYM3812((1 + i) * (nCyclesTotal[1] / nInterleave));
		MSM5205Update();
		ZetClose();
	}

	ZetOpen(1);
	
	BurnTimerEndFrameYM3812(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		BurnYM3812Update(pBurnSoundOut, nBurnSoundLen);
		MSM5205Render(0, pBurnSoundOut, nBurnSoundLen);
	}

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
		*pnMin = 0x029707;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);

		BurnYM3812Scan(nAction, pnMin);

		SCAN_VAR(bankdata);
		SCAN_VAR(flipscreen);
		SCAN_VAR(soundlatch);
		SCAN_VAR(adpcm_toggle);
		SCAN_VAR(adpcm_data);
	}

	if (nAction & ACB_WRITE)
	{
		ZetOpen(0);
		bankswitch(bankdata[0]);
		rambank(bankdata[1]);
		ZetClose();

		ZetOpen(1);
		sound_bankswitch(bankdata[2]);
		ZetClose();
	}

	return 0;
}


// Magix / Rock

static struct BurnRomInfo magixRomDesc[] = {
	{ "yunsung8.07",	0x20000, 0xd4d0b68b, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	{ "yunsung8.08",	0x20000, 0x6fd60be9, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code

	{ "yunsung8.04",	0x80000, 0x0a100d2b, 3 | BRF_GRA },           //  2 Background Tiles
	{ "yunsung8.03",	0x80000, 0xc8cb0373, 3 | BRF_GRA },           //  3
	{ "yunsung8.02",	0x80000, 0x09efb8e5, 3 | BRF_GRA },           //  4
	{ "yunsung8.01",	0x80000, 0x4590d782, 3 | BRF_GRA },           //  5

	{ "yunsung8.05",	0x20000, 0x862d378c, 4 | BRF_GRA },           //  6 Foreground Tiles
	{ "yunsung8.06",	0x20000, 0x8b2ab901, 4 | BRF_GRA },           //  7
};

STD_ROM_PICK(magix)
STD_ROM_FN(magix)

struct BurnDriver BurnDrvMagix = {
	"magix", NULL, NULL, NULL, "1995",
	"Magix / Rock\0", NULL, "Yun Sung", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, magixRomInfo, magixRomName, NULL, NULL, NULL, NULL, RocktrisInputInfo, MagixDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	384, 240, 4, 3
};


// Magix / Rock (no copyright message)

static struct BurnRomInfo magixbRomDesc[] = {
	{ "8.bin",		0x20000, 0x3b92020f, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	{ "9.bin",		0x20000, 0x6fd60be9, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code

	{ "1.bin",		0x80000, 0x0a100d2b, 3 | BRF_GRA },           //  2 Background Tiles
	{ "2.bin",		0x80000, 0xc8cb0373, 3 | BRF_GRA },           //  3
	{ "3.bin",		0x80000, 0x09efb8e5, 3 | BRF_GRA },           //  4
	{ "4.bin",		0x80000, 0x4590d782, 3 | BRF_GRA },           //  5

	{ "5.bin",		0x20000, 0x11b99819, 4 | BRF_GRA },           //  6 Foreground Tiles
	{ "6.bin",		0x20000, 0x361a864c, 4 | BRF_GRA },           //  7
};

STD_ROM_PICK(magixb)
STD_ROM_FN(magixb)

struct BurnDriver BurnDrvMagixb = {
	"magixb", "magix", NULL, NULL, "1995",
	"Magix / Rock (no copyright message)\0", NULL, "Yun Sung", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, magixbRomInfo, magixbRomName, NULL, NULL, NULL, NULL, RocktrisInputInfo, MagixDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	384, 240, 4, 3
};


// Cannon Ball (Yun Sung, horizontal)

static struct BurnRomInfo cannballRomDesc[] = {
	{ "cannball.07",	0x20000, 0x17db56b4, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	{ "cannball.08",	0x20000, 0x11403875, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code

	{ "cannball.01",	0x40000, 0x2d7785e4, 3 | BRF_GRA },           //  2 Background Tiles
	{ "cannball.02",	0x40000, 0x24df387e, 3 | BRF_GRA },           //  3
	{ "cannball.03",	0x40000, 0x4d62f192, 3 | BRF_GRA },           //  4
	{ "cannball.04",	0x40000, 0x37cf8b12, 3 | BRF_GRA },           //  5

	{ "cannball.05",	0x20000, 0x87c1f1fa, 4 | BRF_GRA },           //  6 Foreground Tiles
	{ "cannball.06",	0x20000, 0xe722bee8, 4 | BRF_GRA },           //  7
};

STD_ROM_PICK(cannball)
STD_ROM_FN(cannball)

struct BurnDriver BurnDrvCannball = {
	"cannball", NULL, NULL, NULL, "1995",
	"Cannon Ball (Yun Sung, horizontal)\0", NULL, "Yun Sung / Soft Vision", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, cannballRomInfo, cannballRomName, NULL, NULL, NULL, NULL, CannballInputInfo, CannballDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	384, 240, 4, 3
};


// Cannon Ball (Yun Sung, vertical)

static struct BurnRomInfo cannballvRomDesc[] = {
	{ "yunsung1",		0x20000, 0xf7398b0d, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	{ "yunsung8",		0x20000, 0x11403875, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code

	{ "yunsung7",		0x80000, 0xa5f1a648, 3 | BRF_GRA },           //  2 Background Tiles
	{ "yunsung6",		0x80000, 0x8baa686e, 3 | BRF_GRA },           //  3
	{ "yunsung5",		0x80000, 0xa7f2ce51, 3 | BRF_GRA },           //  4
	{ "yunsung4",		0x80000, 0x74bef793, 3 | BRF_GRA },           //  5

	{ "yunsung3",		0x20000, 0x8217abbe, 4 | BRF_GRA },           //  6 Foreground Tiles
	{ "yunsung2",		0x20000, 0x76de1045, 4 | BRF_GRA },           //  7
};

STD_ROM_PICK(cannballv)
STD_ROM_FN(cannballv)

struct BurnDriver BurnDrvCannballv = {
	"cannballv", "cannball", NULL, NULL, "1995",
	"Cannon Ball (Yun Sung, vertical)\0", NULL, "Yun Sung / J&K Production", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, cannballvRomInfo, cannballvRomName, NULL, NULL, NULL, NULL, CannballInputInfo, CannballDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 384, 3, 4
};


// Rock Tris

static struct BurnRomInfo rocktrisRomDesc[] = {
	{ "cpu07.bin",		0x20000, 0x46e3b79c, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	{ "cpu08.bin",		0x20000, 0x3a78a4cf, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code

	{ "gfx4.bin",		0x80000, 0xabb49cac, 3 | BRF_GRA },           //  2 Background Tiles
	{ "gfx3.bin",		0x80000, 0x70a6ad52, 3 | BRF_GRA },           //  3
	{ "gfx2.bin",		0x80000, 0xfcc9ec97, 3 | BRF_GRA },           //  4
	{ "gfx1.bin",		0x80000, 0x4295034d, 3 | BRF_GRA },           //  5

	{ "gfx5.bin",		0x20000, 0x058ee379, 4 | BRF_GRA },           //  6 Foreground Tiles
	{ "gfx6.bin",		0x20000, 0x593cbd39, 4 | BRF_GRA },           //  7
};

STD_ROM_PICK(rocktris)
STD_ROM_FN(rocktris)

struct BurnDriver BurnDrvRocktris = {
	"rocktris", NULL, NULL, NULL, "1994",
	"Rock Tris\0", NULL, "Yun Sung", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, rocktrisRomInfo, rocktrisRomName, NULL, NULL, NULL, NULL, RocktrisInputInfo, RocktrisDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	384, 240, 4, 3
};
