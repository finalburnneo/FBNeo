// FB Alpha SNK Main event driver module
// Based on MAME driver by David Haywood and Tomasz Slanina

#include "tiles_generic.h"
#include "z80_intf.h"
#include "driver.h"
extern "C" {
#include "ay8910.h"
}

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvBgRAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvSprRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT16 *pAY8910Buffer[6];

static UINT8  DrvJoy1[8];
static UINT8  DrvJoy2[8];
static UINT8  DrvJoy3[8];
static UINT8  DrvJoy4[8];
static UINT8  DrvDips[3];
static UINT8  DrvInputs[4];
static UINT8  DrvReset;

static UINT8 soundlatch;
static UINT8 flipscreen;
static UINT8 sound_cpu_busy;
static INT32 palette_offset;
static INT32 bg_tile_offset;
static INT32 sprromsize;

static INT32 game_select = 0; // 0 mainsnk, 1 = canvas

static struct BurnInputInfo MainsnkInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"},
	{"P1 Left Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Left Down",	BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"	},
	{"P1 Left Left",	BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Left Right",	BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Right Up",		BIT_DIGITAL,	DrvJoy2 + 4,	"p3 up"		},
	{"P1 Right Down",	BIT_DIGITAL,	DrvJoy2 + 5,	"p3 down"	},
	{"P1 Right Left",	BIT_DIGITAL,	DrvJoy2 + 6,	"p3 left"	},
	{"P1 Right Right",	BIT_DIGITAL,	DrvJoy2 + 7,	"p3 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy4 + 1,	"p1 fire 2"},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy4 + 2,	"p1 fire 3"},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy4 + 3,	"p1 fire 4"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"},
	{"P2 Left Up",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 up"		},
	{"P2 Left Down",	BIT_DIGITAL,	DrvJoy3 + 1,	"p2 down"	},
	{"P2 Left Left",	BIT_DIGITAL,	DrvJoy3 + 2,	"p2 left"	},
	{"P2 Left Right",	BIT_DIGITAL,	DrvJoy3 + 3,	"p2 right"	},
	{"P2 Right Up",		BIT_DIGITAL,	DrvJoy3 + 4,	"p4 up"		},
	{"P2 Right Down",	BIT_DIGITAL,	DrvJoy3 + 5,	"p4 down"	},
	{"P2 Right Left",	BIT_DIGITAL,	DrvJoy3 + 6,	"p4 left"	},
	{"P2 Right Right",	BIT_DIGITAL,	DrvJoy3 + 7,	"p4 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy4 + 4,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy4 + 5,	"p2 fire 2"},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy4 + 6,	"p2 fire 3"},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy4 + 7,	"p2 fire 4"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
};

STDINPUTINFO(Mainsnk)


static struct BurnDIPInfo MainsnkDIPList[]=
{
	{0x1e, 0xff, 0xff, 0xff, NULL		},
	{0x1f, 0xff, 0xff, 0xbf, NULL		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x1e, 0x01, 0x01, 0x01, "Off"		},
	{0x1e, 0x01, 0x01, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x1e, 0x01, 0x02, 0x02, "Off"		},
	{0x1e, 0x01, 0x02, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x1e, 0x01, 0x04, 0x04, "Off"		},
	{0x1e, 0x01, 0x04, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x1e, 0x01, 0x08, 0x08, "Off"		},
	{0x1e, 0x01, 0x08, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x1e, 0x01, 0x10, 0x10, "Off"		},
	{0x1e, 0x01, 0x10, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x1e, 0x01, 0x20, 0x20, "Off"		},
	{0x1e, 0x01, 0x20, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x1e, 0x01, 0x40, 0x40, "Off"		},
	{0x1e, 0x01, 0x40, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x1e, 0x01, 0x80, 0x80, "Off"		},
	{0x1e, 0x01, 0x80, 0x00, "On"		},

	{0   , 0xfe, 0   ,    6, "Coinage"		},
	{0x1f, 0x01, 0x07, 0x01, "3 Coins 1 Credits"		},
	{0x1f, 0x01, 0x07, 0x06, "2 Coins 1 Credits"		},
	{0x1f, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x1f, 0x01, 0x07, 0x03, "1 Coin  2 Credits"		},
	{0x1f, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x1f, 0x01, 0x07, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x1f, 0x01, 0x08, 0x08, "Easy"		},
	{0x1f, 0x01, 0x08, 0x00, "Hard"		},

	{0   , 0xfe, 0   ,    2, "Round Time"		},
	{0x1f, 0x01, 0x10, 0x10, "Normal"		},
	{0x1f, 0x01, 0x10, 0x00, "Short"		},

	{0   , 0xfe, 0   ,    4, "Game mode"		},
	{0x1f, 0x01, 0x60, 0x60, "Demo Sounds Off"		},
	{0x1f, 0x01, 0x60, 0x20, "Demo Sounds On"		},
	{0x1f, 0x01, 0x60, 0x00, "Freeze"		},
	{0x1f, 0x01, 0x60, 0x40, "Endless Game (Cheat)"		},

	{0   , 0xfe, 0   ,    2, "2 Players Game"		},
	{0x1f, 0x01, 0x80, 0x80, "1 Credit"		},
	{0x1f, 0x01, 0x80, 0x00, "2 Credits"		},
};

STDDIPINFO(Mainsnk)

static struct BurnInputInfo CanvasInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 1"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 1"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
};

STDINPUTINFO(Canvas)


static struct BurnDIPInfo CanvasDIPList[]=
{
	{0x10, 0xff, 0xff, 0xfd, NULL		},
	{0x11, 0xff, 0xff, 0x57, NULL		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x10, 0x01, 0x01, 0x01, "Off"		},
	{0x10, 0x01, 0x01, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x10, 0x01, 0x02, 0x00, "Upright"		},
	{0x10, 0x01, 0x02, 0x02, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x10, 0x01, 0x04, 0x04, "3"		},
	{0x10, 0x01, 0x04, 0x00, "5"		},

	{0   , 0xfe, 0   ,    7, "Coinage"		},
	{0x10, 0x01, 0x38, 0x10, "5 Coins 1 Credits"		},
	{0x10, 0x01, 0x38, 0x20, "3 Coins 1 Credits"		},
	{0x10, 0x01, 0x38, 0x18, "2 Coins 1 Credits"		},
	{0x10, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x10, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x10, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x10, 0x01, 0x38, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x10, 0x01, 0x40, 0x40, "Off"		},
	{0x10, 0x01, 0x40, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x10, 0x01, 0x80, 0x80, "Off"		},
	{0x10, 0x01, 0x80, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x11, 0x01, 0x01, 0x01, "Off"		},
	{0x11, 0x01, 0x01, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x11, 0x01, 0x02, 0x02, "Off"		},
	{0x11, 0x01, 0x02, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x11, 0x01, 0x04, 0x04, "Off"		},
	{0x11, 0x01, 0x04, 0x00, "On"		},

	{0   , 0xfe, 0   ,    4, "Game mode"		},
	{0x11, 0x01, 0x18, 0x18, "Demo Sounds Off"		},
	{0x11, 0x01, 0x18, 0x10, "Demo Sounds On"		},
	{0x11, 0x01, 0x18, 0x00, "Freeze"		},
	{0x11, 0x01, 0x18, 0x08, "Infinite Lives (Cheat)"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x11, 0x01, 0x20, 0x00, "Off"		},
	{0x11, 0x01, 0x20, 0x20, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x11, 0x01, 0x40, 0x40, "Off"		},
	{0x11, 0x01, 0x40, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Must Be On"		},
	{0x11, 0x01, 0x80, 0x80, "Off"		},
	{0x11, 0x01, 0x80, 0x00, "On"		},
};

STDDIPINFO(Canvas)



static void __fastcall main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xc600:
		{
			flipscreen = ~data & 0x80;
			palette_offset = ((data & 0x07) << 4);
			if (game_select) {
				bg_tile_offset = /*((data & 0x40) >> 6) | ((data & 0x30) >> 3);*/ ((data & 0x40) << 2) | ((data & 0x30) << 5);
			} else {
				bg_tile_offset = (data & 0x30) << 4;
			}
		}
		return;

		case 0xc700:
		{
			sound_cpu_busy = 1;
			soundlatch = data;
			ZetClose();
			ZetOpen(1);
			ZetNmi();
			ZetClose();
			ZetOpen(0);
		}
		return;
	}
}

static UINT8 __fastcall main_read(UINT16 address)
{
	switch (address)
	{
		case 0xc000:
			return ((DrvInputs[0] ^ 0x07) & ~0x20) | (sound_cpu_busy << 5);

		case 0xc100:
		case 0xc200:
		case 0xc300:
			return DrvInputs[(address >> 8) & 3];

		case 0xc400:
		case 0xc500:
			return DrvDips[(address >> 8) & 1];
	}

	return 0;
}

static void __fastcall sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xe000:
		case 0xe001:
		case 0xe008:
		case 0xe009:
			AY8910Write((address >> 3) & 1, address & 1, data);
		return;
	}
}

static UINT8 __fastcall sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xa000:
			return soundlatch;

		case 0xc000:
			sound_cpu_busy = 0;
			return 0xff;
	}

	return 0;
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

	AY8910Reset(0);
	AY8910Reset(1);

	soundlatch = 0;
	flipscreen = 0;
	sound_cpu_busy = 0;
	palette_offset = 0;
	bg_tile_offset = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x00c000;
	DrvZ80ROM1		= Next; Next += 0x008000;

	DrvGfxROM0		= Next; Next += 0x020000;
	DrvGfxROM1		= Next; Next += 0x040000;

	DrvColPROM		= Next; Next += 0x000c00;

	DrvPalette		= (UINT32*)Next; Next += 0x0400 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM1		= Next; Next += 0x000800;
	DrvBgRAM		= Next; Next += 0x001000;
	DrvFgRAM		= Next; Next += 0x000800;
	DrvSprRAM		= Next; Next += 0x000800;

	RamEnd			= Next;

	pAY8910Buffer[0]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[1]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[2]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[3]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[4]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[5]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[4]  = { STEP4(0,1) };
	INT32 XOffs0[8]  = { 4, 0, 12, 8, 20, 16, 28, 24 };
	INT32 YOffs0[8]  = { STEP8(0,32) };
	INT32 Plane1[16] = { RGN_FRAC(sprromsize, 2,3),RGN_FRAC(sprromsize, 1,3),RGN_FRAC(sprromsize, 0,3) };
	INT32 XOffs1[16] = { 7,6,5,4,3,2,1,0, 15,14,13,12,11,10,9,8 };
	INT32 YOffs1[16] = { 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16, 8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16 };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x12000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x10000);

	GfxDecode(0x0800, 4,  8,  8, Plane0, XOffs0, YOffs0, 0x100, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x12000);

	GfxDecode(0x0300, 3, 16, 16, Plane1, XOffs1, YOffs1, 0x100, tmp, DrvGfxROM1);

	memset (DrvGfxROM1 + 0x30000, 0x7, 0x10000); // fill extra with blank tiles

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit(INT32 select)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	game_select = select;

	if (select == 0) // mainsnk
	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x02000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x04000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x06000,  3, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x08000,  4, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x0a000,  5, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  6, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x02000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x04000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x06000, 10, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x02000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x04000, 13, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x06000, 14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x08000, 15, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x0a000, 16, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x0c000, 17, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x0e000, 18, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x10000, 19, 1)) return 1;
		sprromsize = 0x12000;
		if (BurnLoadRom(DrvColPROM + 0x00000, 20, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00400, 21, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00800, 22, 1)) return 1;
	}
	else	// canvas
	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x02000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x04000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x06000,  3, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x08000,  4, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x0a000,  5, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  6, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM1 + 0x04000,  7, 1)) return 1;

		memset (DrvGfxROM0, 0xff, 0x4000);
		if (BurnLoadRom(DrvGfxROM0 + 0x04000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x08000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0c000, 10, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x02000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x04000, 13, 1)) return 1;
		/*if (BurnLoadRom(DrvGfxROM1 + 0x00000, 11, 1)) return 1;
		memcpy (DrvGfxROM1 + 0x2000, DrvGfxROM1 + 0x0000, 0x2000);
		memcpy (DrvGfxROM1 + 0x4000, DrvGfxROM1 + 0x0000, 0x2000);
		if (BurnLoadRom(DrvGfxROM1 + 0x06000, 12, 1)) return 1;
		memcpy (DrvGfxROM1 + 0x8000, DrvGfxROM1 + 0x4000, 0x2000);
		memcpy (DrvGfxROM1 + 0xa000, DrvGfxROM1 + 0x4000, 0x2000);
		if (BurnLoadRom(DrvGfxROM1 + 0x0c000, 13, 1)) return 1;
		memcpy (DrvGfxROM1 + 0xe000, DrvGfxROM1 + 0xc000, 0x2000);
		memcpy (DrvGfxROM1 + 0x10000, DrvGfxROM1 + 0xc000, 0x2000); */
		sprromsize = 0x6000;

		if (BurnLoadRom(DrvColPROM + 0x00000, 14, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00400, 15, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00800, 16, 1)) return 1;
	}

	DrvGfxDecode();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvBgRAM,		0xd800, 0xe7ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xe800, 0xefff, MAP_RAM);
	ZetMapMemory(DrvFgRAM,		0xf000, 0xf7ff, MAP_RAM);
	ZetSetWriteHandler(main_write);
	ZetSetReadHandler(main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,	0x8000, 0x87ff, MAP_RAM);
	ZetSetWriteHandler(sound_write);
	ZetSetReadHandler(sound_read);
	ZetClose();

	AY8910Init(0, 2000000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910Init(1, 2000000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.15, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.15, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	ZetExit();
	AY8910Exit(0);
	AY8910Exit(1);

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x400; i++)
	{
		INT32 bit0 = (DrvColPROM[i + 0x800] >> 3) & 0x01;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 0x01;
		INT32 bit3 = (DrvColPROM[i] >> 3) & 0x01;
		INT32 r = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[i + 0x800] >> 2) & 0x01;
		bit1 = (DrvColPROM[i + 0x400] >> 2) & 0x01;
		bit2 = (DrvColPROM[i + 0x400] >> 3) & 0x01;
		bit3 = (DrvColPROM[i] >> 0) & 0x01;
		INT32 g = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[i + 0x800] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + 0x800] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + 0x400] >> 0) & 0x01;
		bit3 = (DrvColPROM[i + 0x400] >> 1) & 0x01;
		INT32 b = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_layer_tx()
{
	for (INT32 offs = 0; offs < 36 * 28; offs++)
	{
		INT32 col = offs % 36;
		INT32 row = offs / 36;

		INT32 sx = (col * 8);
		INT32 sy = (row * 8);

		INT32 ofst = (col - 2);

		if (ofst & 0x20) {
			ofst = 0x400 + row + ((ofst & 0x1f) << 5);
		} else {
			ofst = row + (ofst << 5);
		}

		INT32 code = DrvFgRAM[ofst];

		if (ofst & 0x400) { // correct??
			Render8x8Tile_Clip(pTransDraw, code, sx, sy, 0, 4, palette_offset+0x100, DrvGfxROM0);
		} else {
			Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy, 0, 4, 0xf, palette_offset+0x100, DrvGfxROM0);
		}
	}
}

static void draw_layer_bg()
{
	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sy = (offs & 0x1f) * 8;
		INT32 sx = (offs / 0x20) * 8;
		sx += 16;

		INT32 code = DrvBgRAM[offs] | bg_tile_offset;

		Render8x8Tile_Clip(pTransDraw, code, sx, sy, 0, 4, palette_offset+0x100, DrvGfxROM0);
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0; offs < 25*4; offs+=4)
	{
		INT32 attr = DrvSprRAM[offs+3];
		INT32 code = DrvSprRAM[offs+1];
		code |= attr << 4 & 0x300;
		INT32 sy = DrvSprRAM[offs+0];
		INT32 sx = 288-16-DrvSprRAM[offs+2];
		INT32 color = attr & 0x0f;
		INT32 flipx = 0;
		INT32 flipy = 0;

		if (sy > 240) sy -= 256;

		if (flipscreen)
		{
			sx = 288-16 - sx;
			sy = 224-16 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		if (flipy) {
			if (flipx) {
				Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 3, 7, 0, DrvGfxROM1);
			} else {
				Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 3, 7, 0, DrvGfxROM1);
			}
		} else {
			if (flipx) {
				Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 3, 7, 0, DrvGfxROM1);
			} else {
				Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 3, 7, 0, DrvGfxROM1);
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

	if (~nBurnLayer & 1) BurnTransferClear();

	if (nBurnLayer & 1) draw_layer_bg();
	if (nSpriteEnable & 1) draw_sprites();
	if (nBurnLayer & 2) draw_layer_tx();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs,0xff,4);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}
	}

	INT32 nInterleave = 16;
	INT32 nCyclesTotal[2] = { 3360000 / 60, 4000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };


	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);
		if (i == 15) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(1);
		nCyclesDone[1] += ZetRun(nCyclesTotal[1] / nInterleave);
		if (i == 3 || i == 7 || i == 13 || i == 15) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();
	}

	if (pBurnSoundOut) {
		AY8910Render(&pAY8910Buffer[0], pBurnSoundOut, nBurnSoundLen, 0);
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
		AY8910Scan(nAction, pnMin);

		SCAN_VAR(soundlatch);
		SCAN_VAR(flipscreen);
		SCAN_VAR(sound_cpu_busy);
		SCAN_VAR(palette_offset);
		SCAN_VAR(bg_tile_offset);
	}

	return 0;
}

INT32 DrvInitmainsnk()
{
	return DrvInit(0);
}

INT32 DrvInitcanvas()
{
	return DrvInit(1);
}

// Main Event (1984)

static struct BurnRomInfo mainsnkRomDesc[] = {
	{ "snk.p01",	0x2000, 0x00db1ca2, 1 | BRF_ESS | BRF_PRG }, //  0 maincpu
	{ "snk.p02",	0x2000, 0xdf5c86b5, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "snk.p03",	0x2000, 0x5c2b7bca, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "snk.p04",	0x2000, 0x68b4b2a1, 1 | BRF_ESS | BRF_PRG }, //  3
	{ "snk.p05",	0x2000, 0x580a29b4, 1 | BRF_ESS | BRF_PRG }, //  4
	{ "snk.p06",	0x2000, 0x5f8a60a2, 1 | BRF_ESS | BRF_PRG }, //  5

	{ "snk.p07",	0x4000, 0x4208391e, 2 | BRF_ESS | BRF_PRG }, //  6 audiocpu

	{ "snk.p12",	0x2000, 0xecf87eb7, 3 | BRF_GRA }, //  7 gfx1
	{ "snk.p11",	0x2000, 0x3f6bc5ba, 3 | BRF_GRA }, //  8
	{ "snk.p10",	0x2000, 0xb5147a96, 3 | BRF_GRA }, //  9
	{ "snk.p09",	0x2000, 0x0ebcf837, 3 | BRF_GRA }, // 10

	{ "snk.p13",	0x2000, 0x2eb624a4, 4 | BRF_GRA }, // 11 gfx2
	{ "snk.p16",	0x2000, 0xdc502869, 4 | BRF_GRA }, // 12
	{ "snk.p19",	0x2000, 0x58d566a1, 4 | BRF_GRA }, // 13
	{ "snk.p14",	0x2000, 0xbb927d82, 4 | BRF_GRA }, // 14
	{ "snk.p17",	0x2000, 0x66f60c32, 4 | BRF_GRA }, // 15
	{ "snk.p20",	0x2000, 0xd12c6333, 4 | BRF_GRA }, // 16
	{ "snk.p15",	0x2000, 0xd242486d, 4 | BRF_GRA }, // 17
	{ "snk.p18",	0x2000, 0x838b12a3, 4 | BRF_GRA }, // 18
	{ "snk.p21",	0x2000, 0x8961a51e, 4 | BRF_GRA }, // 19

	{ "main3.bin",	0x0800, 0x78b29dde, 5 | BRF_GRA }, // 20 proms
	{ "main2.bin",	0x0800, 0x7c314c93, 5 | BRF_GRA }, // 21
	{ "main1.bin",	0x0800, 0xdeb895c4, 5 | BRF_GRA }, // 22
};

STD_ROM_PICK(mainsnk)
STD_ROM_FN(mainsnk)

struct BurnDriver BurnDrvMainsnk = {
	"mainsnk", NULL, NULL, NULL, "1984",
	"Main Event (1984)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, mainsnkRomInfo, mainsnkRomName, NULL, NULL, MainsnkInputInfo, MainsnkDIPInfo,
	DrvInitmainsnk, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	288, 216, 4, 3
};


// Canvas Croquis

static struct BurnRomInfo canvasRomDesc[] = {
	{ "cc_p1.a2",		0x2000, 0xfa7109e1, 1 | BRF_ESS | BRF_PRG }, //  0 maincpu
	{ "cc_p2.a3",		0x2000, 0x8b8beb34, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "cc_p3.a4",		0x2000, 0xea342f87, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "cc_p4.a5",		0x2000, 0x9cf35d98, 1 | BRF_ESS | BRF_PRG }, //  3
	{ "cc_p5.a7",		0x2000, 0xc5ef1eda, 1 | BRF_ESS | BRF_PRG }, //  4
	{ "cc_p6.a8",		0x2000, 0x7b1dd7fc, 1 | BRF_ESS | BRF_PRG }, //  5

	{ "cc_p7.h2",		0x4000, 0x029b5ea0, 2 | BRF_ESS | BRF_PRG }, //  6 audiocpu
	{ "cc_p8.f2",		0x2000, 0x0f0368ce, 2 | BRF_ESS | BRF_PRG }, //  7

	{ "cc_p11.c2",		0x4000, 0x4c8c2156, 3 | BRF_GRA }, //  8 gfx1
	{ "cc_p10.b2",		0x4000, 0x3c0a4eeb, 3 | BRF_GRA }, //  9
	{ "cc_p9.a2",		0x4000, 0xb58c5f24, 3 | BRF_GRA }, // 10

	{ "cc_p12.j8",		0x2000, 0x9003a979, 4 | BRF_GRA }, // 11 gfx2
	{ "cc_p13.j5",		0x2000, 0xa52cd549, 4 | BRF_GRA }, // 12
	{ "cc_p14.j2",		0x2000, 0xedc6a1e8, 4 | BRF_GRA }, // 13

	{ "cc_bprom3.j8",	0x0400, 0x21f72498, 5 | BRF_GRA }, // 14 proms
	{ "cc_bprom2.j9",	0x0400, 0x19efe7df, 5 | BRF_GRA }, // 15
	{ "cc_bprom1.j10",	0x0400, 0xfbbbf911, 5 | BRF_GRA }, // 16
};

STD_ROM_PICK(canvas)
STD_ROM_FN(canvas)

struct BurnDriver BurnDrvCanvas = {
	"canvas", NULL, NULL, NULL, "1985",
	"Canvas Croquis\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, canvasRomInfo, canvasRomName, NULL, NULL, CanvasInputInfo, CanvasDIPInfo,
	DrvInitcanvas, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	288, 216, 4, 3
};


