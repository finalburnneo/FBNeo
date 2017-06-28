// FB Alpha Fighting Roller driver module
// Based on MAME driver by Pierpaolo Prazzoli

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
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvVidRAM;
static UINT8 *DrvColRAM;
static UINT8 *DrvSprRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT16 *pAY8910Buffer[9];

static UINT8 sound_nmi_mask = 0;
static UINT8 soundlatch = 0;
static UINT8 nmi_mask = 0;
static UINT8 spritebank = 0;
static UINT8 charbank[2];
static UINT8 backgroundcolor = 0;
static UINT8 backgroundflip = 0;
static UINT8 backgroundpen = 0;
static UINT8 backgroundpage = 0;
static UINT8 screen_flipy = 0;
static UINT8 screen_flipx = 0;

static UINT8 set2 = 0;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvInputs[3];
static UINT8 DrvDips[3];
static UINT8 DrvReset;

static struct BurnInputInfo DrvInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"	        },
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"	        },
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Drv)

static struct BurnDIPInfo DrvDIPList[]=
{
	{0x12, 0xff, 0xff, 0x00, NULL			},
	{0x13, 0xff, 0xff, 0x00, NULL			},
	{0x14, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x12, 0x01, 0x07, 0x07, "6 Coins 1 Credit"	},
	{0x12, 0x01, 0x07, 0x06, "3 Coins 1 Credit"	},
	{0x12, 0x01, 0x07, 0x04, "2 Coins 1 Credit"	},
	{0x12, 0x01, 0x07, 0x00, "1 Coin  1 Credit"	},
	{0x12, 0x01, 0x07, 0x05, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0x07, 0x01, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x07, 0x02, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x07, 0x03, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x12, 0x01, 0x38, 0x38, "6 Coins 1 Credit"	},
	{0x12, 0x01, 0x38, 0x30, "3 Coins 1 Credit"	},
	{0x12, 0x01, 0x38, 0x20, "2 Coins 1 Credit"	},
	{0x12, 0x01, 0x38, 0x00, "1 Coin  1 Credit"	},
	{0x12, 0x01, 0x38, 0x28, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0x38, 0x08, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x38, 0x10, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x38, 0x18, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x12, 0x01, 0x40, 0x40, "Off"			},
	{0x12, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Invulnerability"	},
	{0x12, 0x01, 0x80, 0x80, "Off"			},
	{0x12, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0x03, 0x00, "2"			},
	{0x13, 0x01, 0x03, 0x01, "3"			},
	{0x13, 0x01, 0x03, 0x02, "5"			},
	{0x13, 0x01, 0x03, 0x03, "7"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x13, 0x01, 0x0c, 0x00, "20000"		},
	{0x13, 0x01, 0x0c, 0x04, "50000"		},
	{0x13, 0x01, 0x0c, 0x08, "100000"		},
	{0x13, 0x01, 0x0c, 0x0c, "None"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x30, 0x00, "A"			},
	{0x13, 0x01, 0x30, 0x10, "B"			},
	{0x13, 0x01, 0x30, 0x20, "C"			},
	{0x13, 0x01, 0x30, 0x30, "D"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x13, 0x01, 0x40, 0x00, "Upright"		},
	{0x13, 0x01, 0x40, 0x40, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x40, 0x00, "Off"			},
	{0x14, 0x01, 0x40, 0x40, "On"			},
};

STDDIPINFO(Drv)

static UINT8 __fastcall rollrace_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xd806:
			return 0; // nop

		case 0xd900:
			return 0x51; // protection?

		case 0xf800:
		case 0xf801:
		case 0xf802:
			return DrvInputs[address & 3];

		case 0xf804:
		case 0xf805:
			return DrvDips[address & 1];
	}

	return 0;
}

static void __fastcall rollrace_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xd900:
			// protection?
		return;

		case 0xe800:
			soundlatch = data;
		return;

		case 0xf400:
			backgroundcolor = data;
		return;

		case 0xf801:
			backgroundpen = data;
		return;

		case 0xf802:
			backgroundpage = data & 0x1f;
			backgroundflip = (data & 0x80) >> 7;
		return;

		case 0xf803:
			screen_flipy = data & 0x01;
		return;

		case 0xfc00:
			screen_flipx = data & 0x01;
		return;

		case 0xfc01:
			nmi_mask = data & 0x01;
		return;

		case 0xfc02:
		return; // coin counter

		case 0xfc04:
		case 0xfc05:
			charbank[address & 1] = data;
		return;

		case 0xfc06:
			spritebank = data;
		return;
	}
}

static UINT8 __fastcall rollrace_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x3000:
			return soundlatch;
	}

	return 0;
}

static void __fastcall rollrace_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x3000:
			sound_nmi_mask = data & 0x01;
		return;

		case 0x4000:
		case 0x4001:
		case 0x5000:
		case 0x5001:
		case 0x6000:
		case 0x6001:
			AY8910Write((address >> 12) & 3, address & 1, data);
		return;
	}
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
	AY8910Reset(2);

	sound_nmi_mask = 0;
	soundlatch = 0;
	nmi_mask = 0;
	spritebank = 0;
	charbank[0] = 0;
	charbank[1] = 0;
	backgroundcolor = 0;
	backgroundflip = 0;
	backgroundpen = 0;
	backgroundpage = 0;
	screen_flipy = 0;
	screen_flipx = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x00a000;
	DrvZ80ROM1		= Next; Next += 0x001000;

	DrvGfxROM0		= Next; Next += 0x010000;
	DrvGfxROM1		= Next; Next += 0x010000;
	DrvGfxROM2		= Next; Next += 0x030000;
	DrvGfxROM3		= Next; Next += 0x008000;

	DrvColPROM		= Next; Next += 0x000300;

	DrvPalette		= (UINT32*)Next; Next += 0x0100 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM0		= Next; Next += 0x001000;
	DrvZ80RAM1		= Next; Next += 0x001000;
	DrvVidRAM		= Next; Next += 0x000400;
	DrvColRAM		= Next; Next += 0x000100;
	DrvSprRAM		= Next; Next += 0x000100;

	RamEnd			= Next;

	pAY8910Buffer[0]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[1]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[2]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[3]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[4]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[5]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[6]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[7]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[8]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);

	MemEnd			= Next;

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x100; i++)
	{
		INT32 bit0 = (DrvColPROM[0x000 + i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[0x000 + i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[0x000 + i] >> 2) & 0x01;
		INT32 bit3 = (DrvColPROM[0x000 + i] >> 3) & 0x01;
		INT32 r = 0x0e * bit0 + 0x1f * bit1 + 0x42 * bit2 + 0x90 * bit3;

		bit0 = (DrvColPROM[0x100 + i] >> 0) & 0x01;
		bit1 = (DrvColPROM[0x100 + i] >> 1) & 0x01;
		bit2 = (DrvColPROM[0x100 + i] >> 2) & 0x01;
		bit3 = (DrvColPROM[0x100 + i] >> 3) & 0x01;
		INT32 g = 0x0e * bit0 + 0x1f * bit1 + 0x42 * bit2 + 0x90 * bit3;

		bit0 = (DrvColPROM[0x200 + i] >> 0) & 0x01;
		bit1 = (DrvColPROM[0x200 + i] >> 1) & 0x01;
		bit2 = (DrvColPROM[0x200 + i] >> 2) & 0x01;
		bit3 = (DrvColPROM[0x200 + i] >> 3) & 0x01;
		INT32 b = 0x0e * bit0 + 0x1f * bit1 + 0x42 * bit2 + 0x90 * bit3;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[3] = { 0x2000 * 0 * 8, 0x2000 * 1 * 8, 0x2000 * 2 * 8 };
	INT32 Plane1[3] = { 0x6000 * 2 * 8, 0x6000 * 1 * 8, 0x6000 * 0 * 8 };
	INT32 XOffs0[8] = { 0, 1, 2, 3, 4, 5, 6, 7 }; //{ STEP8(0,1) };
	INT32 YOffs0[8] = { 7*8, 6*8, 5*8, 4*8, 3*8, 2*8, 1*8, 0*8  }; //{ STEP8(56,-8) };
	INT32 XOffs1[32] = { STEP8(0,1), STEP8(64,1), STEP8(128,1), STEP8(192,1) };
	INT32 YOffs1[32] = { STEP8(0,8), STEP8(256,8), STEP8(512,8), STEP8(768,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x12000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x06000);

	GfxDecode(0x0400, 3, 8, 8, Plane0, XOffs0, YOffs0, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x06000);

	GfxDecode(0x0400, 3, 8, 8, Plane0, XOffs0, YOffs0, 0x040, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x12000);

	GfxDecode(0x00c0, 3, 32, 32, Plane1, XOffs1, YOffs1, 0x400, tmp, DrvGfxROM2);

	BurnFree(tmp);

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
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x02000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x04000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x06000,  3, 1)) return 1;
		if (set2)
			if (BurnLoadRom(DrvZ80ROM0 + 0x08000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  4 + set2, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x02000,  5 + set2, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x04000,  6 + set2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  7 + set2, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x02000,  8 + set2, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x04000,  9 + set2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000, 10 + set2, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x06000, 11 + set2, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x0c000, 12 + set2, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x02000, 13 + set2, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x08000, 14 + set2, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x0e000, 15 + set2, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x04000, 16 + set2, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x0a000, 17 + set2, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x10000, 18 + set2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x00000, 19 + set2, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x02000, 20 + set2, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x04000, 21 + set2, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x06000, 22 + set2, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000, 23 + set2, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00100, 24 + set2, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00200, 25 + set2, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000, 26 + set2, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x9fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,	0xc000, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,		0xe000, 0xe3ff, MAP_RAM);
	ZetMapMemory(DrvColRAM,		0xe400, 0xe4ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xf000, 0xf0ff, MAP_RAM);
	ZetSetWriteHandler(rollrace_main_write);
	ZetSetReadHandler(rollrace_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0x0fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,	0x2000, 0x2fff, MAP_RAM);
	ZetSetWriteHandler(rollrace_sound_write);
	ZetSetReadHandler(rollrace_sound_read);
	ZetClose();

	AY8910Init(0, 1500000, nBurnSoundRate, NULL, NULL, NULL, NULL); // RIGHT SPEAKER
	AY8910Init(1, 1500000, nBurnSoundRate, NULL, NULL, NULL, NULL); // RIGHT SPEAKER
	AY8910Init(2, 1500000, nBurnSoundRate, NULL, NULL, NULL, NULL); // LEFT SPEAKER
	AY8910SetAllRoutes(0, 0.10, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.10, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(2, 0.10, BURN_SND_ROUTE_BOTH);

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
	AY8910Exit(2);

	BurnFree(AllMem);

	set2 = 0;

	return 0;
}

static void draw_road()
{
	for (INT32 offs = 0x3ff; offs >= 0; offs--)
	{
		INT32 sx, sy, flipx, flipy;

		if(!(backgroundflip))
		{
			sy = ( 31 - offs / 32 );
		}
		else
			sy = ( offs / 32 );

		sx = ( offs%32 );

		if(screen_flipx)
			sx = 31-sx;

		if(screen_flipy)
			sy = 31-sy;

		flipx = screen_flipx;
		flipy = (backgroundflip^screen_flipy);

		sx = 8 * sx;
		sy = 8 * sy;

		if (!set2) {
			sx -= 16; // offset
			sy -= 16;
		}

		INT32 code = DrvGfxROM3[offs + ( backgroundpage * 1024 )]
				+ ((( DrvGfxROM3[offs + 0x4000 + ( backgroundpage * 1024 )] & 0xc0 ) >> 6 ) * 256 );
		INT32 color = backgroundcolor & 0x1f;

		if (flipy) {
			if (flipx) {
				Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, DrvGfxROM1);
			} else {
				Render8x8Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, DrvGfxROM1);
			}
		} else {
			if (flipx) {
				Render8x8Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, DrvGfxROM1);
			} else {
				Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, DrvGfxROM1);
			}
		}
	}
}

/*static void draw_road()
{
	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sy = (offs / 0x20) * 8;
		INT32 sx = (offs & 0x1f) * 8;

		INT32 flipy = 1;
		INT32 flipx = 0;

		if (backgroundflip == 0) { sy ^= 0xf8; flipy ^= 1; }
		if (screen_flipx) { sx ^= 0xf8; flipx ^= 1; }
		if (screen_flipy) { sy ^= 0xf8; flipy ^= 1; }

		INT32 ofst = offs + (backgroundpage * 0x400);
		INT32 code = DrvGfxROM3[ofst] + ((DrvGfxROM3[ofst + 0x4000] & 0xc0) << 2);
		INT32 color = backgroundcolor & 0x1f;

		if (flipy) {
			if (flipx) {
				Render8x8Tile_FlipXY_Clip(pTransDraw, code, sx, sy, color, 3, 0, DrvGfxROM1);
			} else {
				Render8x8Tile_FlipY_Clip(pTransDraw, code, sx, sy, color, 3, 0, DrvGfxROM1);
			}
		} else {
			if (flipx) {
				Render8x8Tile_FlipX_Clip(pTransDraw, code, sx, sy, color, 3, 0, DrvGfxROM1);
			} else {
				Render8x8Tile_Clip(pTransDraw, code, sx, sy, color, 3, 0, DrvGfxROM1);
			}
		}
	}
}*/

static void draw_text()
{
	INT32 bank = charbank[0] | (charbank[1] << 1);

	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sx =  (offs & 0x1f);
		INT32 sy = (((offs / 0x20) * 8) + DrvColRAM[2 * sx]) & 0xff;

		INT32 code  = DrvVidRAM[offs] + (bank * 0x100);
		INT32 color = DrvColRAM[sx * 2 + 1] & 0x1f;

		if (!screen_flipy) sy = (248 - sy) & 0xff;
		if ( screen_flipx) sx = 31 - sx;

		if (!set2)
			sx -= 2; // offset
		sy -= 2*8; // offset

		if (screen_flipy) {
			if (screen_flipx) {
				Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, code, sx * 8, sy, color, 3, 0, 0, DrvGfxROM0);
			} else {
				Render8x8Tile_Mask_FlipY_Clip(pTransDraw, code, sx * 8, sy, color, 3, 0, 0, DrvGfxROM0);
			}
		} else {
			if (screen_flipx) {
				Render8x8Tile_Mask_FlipX_Clip(pTransDraw, code, sx * 8, sy, color, 3, 0, 0, DrvGfxROM0);
			} else {
				Render8x8Tile_Mask_Clip(pTransDraw, code, sx * 8, sy, color, 3, 0, 0, DrvGfxROM0);
			}
		}
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0x80-4 ; offs >=0x0 ; offs -= 4)
	{
		INT32 flipy = screen_flipy;
		INT32 flipx = screen_flipx;
		INT32 attr = DrvSprRAM[offs+1];
		INT32 color = DrvSprRAM[offs+2] & 0x1f;
		INT32 sy = DrvSprRAM[offs] - 16;
		INT32 sx = DrvSprRAM[offs+3] - 16;
		if (!set2) {
			sx += 16; //offset
			sy -= 16;
		}

		if (sx && sy)
		{
			if (screen_flipx) sx = 224 - sx;
			if (screen_flipy) sy = 224 - sy;

			if (attr & 0x80) flipy ^= 1;

			INT32 bank = ((attr & 0x40) >> 6) & 1;
			if (bank) bank += spritebank;

			INT32 code = (attr & 0x3f) + (bank * 0x40);

			if (flipy == 0) {
				if (flipx) {
					Render32x32Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, DrvGfxROM2);
				} else {
					Render32x32Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, DrvGfxROM2);
				}
			} else {
				if (flipx) {
					Render32x32Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, DrvGfxROM2);
				} else {
					Render32x32Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, DrvGfxROM2);
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

	for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
		pTransDraw[i] = backgroundpen;
	}

	if (nBurnLayer & 2) draw_road();
	if (nBurnLayer & 4) draw_sprites();
	if (nBurnLayer & 8) draw_text();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{	memset (DrvInputs, 0, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
		DrvInputs[2] = (DrvInputs[2] & ~0x40) | (DrvDips[2] & 0x40);
	}

	INT32 nInterleave = 16;
	INT32 nCyclesTotal[2] = { 3000000 / 60, 1500000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		INT32 nSegment = nCyclesTotal[0] / nInterleave;

		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nSegment);
		if (nmi_mask && i == (nInterleave - 1)) ZetNmi();
		ZetClose();

		ZetOpen(1);
		nCyclesDone[1] += ZetRun(nSegment / 2);
		if (sound_nmi_mask && (i % (nInterleave / 4)) == ((nInterleave / 4) - 1)) ZetNmi();
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

		SCAN_VAR(sound_nmi_mask);
		SCAN_VAR(soundlatch);
		SCAN_VAR(nmi_mask);
		SCAN_VAR(spritebank);
		SCAN_VAR(charbank);
		SCAN_VAR(backgroundcolor);
		SCAN_VAR(backgroundflip);
		SCAN_VAR(backgroundpen);
		SCAN_VAR(backgroundpage);
		SCAN_VAR(screen_flipy);
		SCAN_VAR(screen_flipx);
	}

	return 0;
}


// Fighting Roller

static struct BurnRomInfo fightrolRomDesc[] = {
	{ "4.8k",        0x2000, 0xefa2f430, 0 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "5.8h",        0x2000, 0x2497d9f6, 0 | BRF_PRG | BRF_ESS }, //  1
	{ "6.8f",        0x2000, 0xf39727b9, 0 | BRF_PRG | BRF_ESS }, //  2
	{ "7.8d",        0x2000, 0xee65b728, 0 | BRF_PRG | BRF_ESS }, //  3

	{ "3.7m",        0x2000, 0xca4f353c, 1 | BRF_GRA },           //  4 Text Tiles
	{ "2.8m",        0x2000, 0x93786171, 1 | BRF_GRA },           //  5
	{ "1.9m",        0x2000, 0xdc072be1, 1 | BRF_GRA },           //  6

	{ "6.20k",       0x2000, 0x003d7515, 2 | BRF_GRA },           //  7 Road Tiles
	{ "7.18k",       0x2000, 0x27843afa, 2 | BRF_GRA },           //  8
	{ "5.20f",       0x2000, 0x51dd0108, 2 | BRF_GRA },           //  9

	{ "8.17m",       0x2000, 0x08ad783e, 3 | BRF_GRA },           // 10 Sprites
	{ "9.17r",       0x2000, 0x69b23461, 3 | BRF_GRA },           // 11
	{ "10.17t",      0x2000, 0xba6ccd8c, 3 | BRF_GRA },           // 12
	{ "11.18m",      0x2000, 0x06a5d849, 3 | BRF_GRA },           // 13
	{ "12.18r",      0x2000, 0x569815ef, 3 | BRF_GRA },           // 14
	{ "13.18t",      0x2000, 0x4f8af872, 3 | BRF_GRA },           // 15
	{ "14.19m",      0x2000, 0x93f3c649, 3 | BRF_GRA },           // 16
	{ "15.19r",      0x2000, 0x5b3d87e4, 3 | BRF_GRA },           // 17
	{ "16.19u",      0x2000, 0xa2c24b64, 3 | BRF_GRA },           // 18

	{ "1.17a",       0x2000, 0xf0fa72fc, 4 | BRF_GRA },           // 19 Road Map
	{ "3.18b",       0x2000, 0x954268f7, 4 | BRF_GRA },           // 20
	{ "2.17d",       0x2000, 0x2e38bb0e, 4 | BRF_GRA },           // 21
	{ "4.18d",       0x2000, 0x3d9e16ab, 4 | BRF_GRA },           // 22

	{ "tbp24s10.7u", 0x0100, 0x9d199d33, 5 | BRF_GRA },           // 23 Color PROMs
	{ "tbp24s10.7t", 0x0100, 0xc0426582, 5 | BRF_GRA },           // 24
	{ "tbp24s10.6t", 0x0100, 0xc096e05c, 5 | BRF_GRA },           // 25

	{ "8.6f",        0x1000, 0x6ec3c545, 6 | BRF_PRG | BRF_ESS }, // 26 Z80 #1 Code
};

STD_ROM_PICK(fightrol)
STD_ROM_FN(fightrol)

struct BurnDriver BurnDrvFightrol = {
	"fightrol", NULL, NULL, NULL, "1983",
	"Fighting Roller\0", NULL, "Kaneko (Taito license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_16BIT_ONLY, 2, HARDWARE_TAITO_MISC, GBF_MISC, 0,
	NULL, fightrolRomInfo, fightrolRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 240, 3, 4
};

//  Roller Aces (set 1)

static struct BurnRomInfo RollaceRomDesc[] = {

	{ "w1.8k", 0x2000, 0xc0bd3cf3, BRF_ESS | BRF_PRG },           //  0 Z80 #0 Code
	{ "w2.8h", 0x2000, 0xc1900a75, BRF_ESS | BRF_PRG },           //  1
	{ "w3.8f", 0x2000, 0x16ceced6, BRF_ESS | BRF_PRG },           //  2
	{ "w4.8d", 0x2000, 0xae826a96, BRF_ESS | BRF_PRG },           //  3

	{ "w3.7m", 0x2000, 0xf9970aae, BRF_GRA },                     //  4 Text Tiles
	{ "w2.8m", 0x2000, 0x80573091, BRF_GRA },                     //  5
	{ "w1.9m", 0x2000, 0xb37effd8, BRF_GRA },                     //  6

	{ "6.20k", 0x2000, 0x003d7515, BRF_GRA },                     //  7 Road Tiles
	{ "7.18k", 0x2000, 0x27843afa, BRF_GRA },                     //  8
	{ "5.20f", 0x2000, 0x51dd0108, BRF_GRA },                     //  9

	{ "w8.17m", 0x2000, 0xe2afe3a3, BRF_GRA },                    //  10 Sprites
	{ "w9.17p", 0x2000, 0x8a8e6b62, BRF_GRA },                    //  11
	{ "w10.17t", 0x2000, 0x70bf7b23, BRF_GRA },                   //  12
	{ "11.18m", 0x2000, 0x06a5d849, BRF_GRA },                    //  13
	{ "12.18r", 0x2000, 0x569815ef, BRF_GRA },                    //  14
	{ "13.18t", 0x2000, 0x4f8af872, BRF_GRA },                    //  15
	{ "14.19m", 0x2000, 0x93f3c649, BRF_GRA },                    //  16
	{ "15.19r", 0x2000, 0x5b3d87e4, BRF_GRA },                    //  17
	{ "16.19u", 0x2000, 0xa2c24b64, BRF_GRA },                    //  18

	{ "1.17a", 0x2000, 0xf0fa72fc, BRF_GRA },                     //  19 Road Map
	{ "3.18b", 0x2000, 0x954268f7, BRF_GRA },                     //  20
	{ "2.17d", 0x2000, 0x2e38bb0e, BRF_GRA },                     //  21
	{ "4.18d", 0x2000, 0x3d9e16ab, BRF_GRA },                     //  22

	{ "tbp24s10.7u", 0x100, 0x9d199d33, BRF_GRA },                //  23 Color PROMs
	{ "tbp24s10.7t", 0x100, 0xc0426582, BRF_GRA },                //  24
	{ "tbp24s10.6t", 0x100, 0xc096e05c, BRF_GRA },                //  25

	{ "8.6f", 0x1000, 0x6ec3c545, BRF_ESS | BRF_PRG },            //  26 Z80 #1 Code
};

STD_ROM_PICK(Rollace)
STD_ROM_FN(Rollace)

struct BurnDriver BurnDrvRollace = {
	"rollace", "fightrol", NULL, NULL, "1983",
	"Roller Aces (set 1)\0", NULL, "Kaneko (Williams license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE  | BDF_ORIENTATION_VERTICAL | BDF_16BIT_ONLY, 2, HARDWARE_TAITO_MISC, GBF_MISC, 0,
	NULL, RollaceRomInfo, RollaceRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 240, 3, 4
};

INT32 Rollace2DrvInit()
{
	set2 = 1;

	return DrvInit();
}

//  Roller Aces (set 2)

static struct BurnRomInfo Rollace2RomDesc[] = {

	{ "8k.764", 0x2000, 0xa7abff82, BRF_ESS | BRF_PRG },         //  0 Z80 #0 Code
	{ "8h.764", 0x2000, 0x9716ba03, BRF_ESS | BRF_PRG },         //  1
	{ "8f.764", 0x2000, 0x3eadb0e8, BRF_ESS | BRF_PRG },         //  2
	{ "8d.764", 0x2000, 0xbaac14db, BRF_ESS | BRF_PRG },         //  3
	{ "8c.764", 0x2000, 0xb418ce84, BRF_ESS | BRF_PRG },         //  4

	{ "7m.764", 0x2000, 0x8b9b27af, BRF_GRA },                   //  5 Text Tiles
	{ "8m.764", 0x2000, 0x2dfc38f2, BRF_GRA },                   //  6
	{ "9m.764", 0x2000, 0x2e3a825b, BRF_GRA },                   //  7

	{ "6.20k", 0x2000, 0x003d7515, BRF_GRA },                    //  8 Road Tiles
	{ "7.18k", 0x2000, 0x27843afa, BRF_GRA },                    //  9
	{ "5.20f", 0x2000, 0x51dd0108, BRF_GRA },                    //  10

	{ "17n.764", 0x2000, 0x3365703c, BRF_GRA },                  //  11 Sprites
	{ "9.17r", 0x2000, 0x69b23461, BRF_GRA },                    //  12
	{ "17t.764", 0x2000, 0x5e84cc9b, BRF_GRA },                  //  13
	{ "11.18m", 0x2000, 0x06a5d849, BRF_GRA },                   //  14
	{ "12.18r", 0x2000, 0x569815ef, BRF_GRA },                   //  15
	{ "13.18t", 0x2000, 0x4f8af872, BRF_GRA },                   //  16
	{ "14.19m", 0x2000, 0x93f3c649, BRF_GRA },                   //  17
	{ "15.19r", 0x2000, 0x5b3d87e4, BRF_GRA },                   //  18
	{ "16.19u", 0x2000, 0xa2c24b64, BRF_GRA },                   //  19

	{ "1.17a", 0x2000, 0xf0fa72fc, BRF_GRA },                    //  20 Road Map
	{ "3.18b", 0x2000, 0x954268f7, BRF_GRA },                    //  21
	{ "17d.764", 0x2000, 0x32e69320, BRF_GRA },                  //  22
	{ "4.18d", 0x2000, 0x3d9e16ab, BRF_GRA },                    //  23

	{ "tbp24s10.7u", 0x100, 0x9d199d33, BRF_GRA },               //  24 Color PROMs
	{ "tbp24s10.7t", 0x100, 0xc0426582, BRF_GRA },               //  25
	{ "tbp24s10.6t", 0x100, 0xc096e05c, BRF_GRA },               //  26

	{ "8.6f", 0x1000, 0x6ec3c545, BRF_ESS | BRF_PRG },           //  27 Z80 #1 Code
};

STD_ROM_PICK(Rollace2)
STD_ROM_FN(Rollace2)

struct BurnDriver BurnDrvRollace2 = {
	"rollace2", "fightrol", NULL, NULL, "1983",
	"Roller Aces (set 2)\0", NULL, "Kaneko (Williams license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_16BIT_ONLY, 2, HARDWARE_TAITO_MISC, GBF_MISC, 0,
	NULL, Rollace2RomInfo, Rollace2RomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	Rollace2DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 240, 3, 4
};


