// FB Alpha Syusse Oozumou driver module
// Based on MAME driver by Takahiro Nogi (nogi@kt.rim.or.jp)

#include "tiles_generic.h"
#include "z80_intf.h"
#include "m6502_intf.h"
#include "driver.h"
extern "C" {
#include "ay8910.h"
}
#include "dac.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv6502ROM0;
static UINT8 *Drv6502ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvColPROM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvVidRAM0;
static UINT8 *DrvVidRAM1;
static UINT8 *DrvColRAM0;
static UINT8 *DrvColRAM1;
static UINT8 *Drv6502RAM1;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT16 *pAY8910Buffer[6];

static INT32 palette_written;
static UINT8 nmi_mask;
static UINT8 soundlatch;
static UINT8 flipscreen;
static UINT8 bgscrolly;

static INT32 vblank;
static INT32 previous_coin = 0;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static struct BurnInputInfo SsozumoInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Ssozumo)

static struct BurnDIPInfo SsozumoDIPList[]=
{
	{0x11, 0xff, 0xff, 0x1f, NULL			},
	{0x12, 0xff, 0xff, 0xfd, NULL			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x11, 0x01, 0x0c, 0x00, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0x0c, 0x04, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x11, 0x01, 0x03, 0x00, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0x03, 0x01, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x11, 0x01, 0x20, 0x00, "Upright"		},
	{0x11, 0x01, 0x20, 0x20, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Controls"		},
	{0x11, 0x01, 0x40, 0x00, "Single"		},
	{0x11, 0x01, 0x40, 0x40, "Dual"			},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x12, 0x01, 0x01, 0x01, "Normal"		},
	{0x12, 0x01, 0x01, 0x00, "Hard"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x02, 0x02, "Off"			},
	{0x12, 0x01, 0x02, 0x00, "On"			},
};

STDDIPINFO(Ssozumo)

static void ssozumo_main_write(UINT16 address, UINT8 data)
{
	if (address >= 0x4050 && address <= 0x407f) {
		if (DrvPalRAM[address - 0x4050] != data) {
			palette_written = 1;
		}
		DrvPalRAM[address - 0x4050] = data;
		return;
	}

	switch (address)
	{
		case 0x4000:
			flipscreen = data >> 7;
		return;

		case 0x4010: {
			soundlatch = data;
			M6502Close();
			M6502Open(1);
			M6502SetIRQLine(0, CPU_IRQSTATUS_HOLD);
			M6502Close();
			M6502Open(0);
		}
		return;

		case 0x4020:
			bgscrolly = data;
		return;

		case 0x4030:
		return;		// nop
	}
}

static UINT8 ssozumo_main_read(UINT16 address)
{
	if (address >= 0x4050 && address <= 0x407f) {
		return DrvPalRAM[address - 0x4050];
	}

	switch (address)
	{
		case 0x4000:
			return DrvInputs[0];

		case 0x4010:
			return DrvInputs[1];

		case 0x4020:
			return DrvDips[1];

		case 0x4030:
			return (DrvDips[0] & 0x7f) | (vblank ? 0x80 : 0);
	}

	return 0;
}

static void ssozumo_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x2000:
		case 0x2001:
		case 0x2002:
		case 0x2003:
			AY8910Write((address >> 1) & 1, (~address & 1), data);
		return;

		case 0x2004:
			DACSignedWrite(0, data);
		return;

		case 0x2005:
			nmi_mask = data & 1;
		return;
	}
}

static UINT8 ssozumo_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x2007:
			return soundlatch;
	}

	return 0;
}

static INT32 DrvSyncDAC()
{
	return (INT32)(float)(nBurnSoundLen * (M6502TotalCycles() / (975000.000 / (nBurnFPS / 100.000))));
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	M6502Open(0);
	M6502Reset();
	M6502Close();

	M6502Open(1);
	M6502Reset();
	DACReset();
	M6502Close();

	AY8910Reset(0);
	AY8910Reset(1);

	palette_written = 1;
	nmi_mask = 0;
	soundlatch = 0;
	flipscreen = 0;
	bgscrolly = 0;
	previous_coin = 0xc0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv6502ROM0		= Next; Next += 0x010000;
	Drv6502ROM1		= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += 0x110000;
	DrvGfxROM1		= Next; Next += 0x110000;
	DrvGfxROM2		= Next; Next += 0x150000;

	DrvColPROM		= Next; Next += 0x000080;

	DrvPalette		= (UINT32*)Next; Next += 0x0050 * sizeof(UINT32);

	AllRam			= Next;

	DrvPalRAM		= Next; Next += 0x000030;
	DrvSprRAM		= Next; Next += 0x000800;
	DrvVidRAM0		= Next; Next += 0x000200;
	DrvVidRAM1		= Next; Next += 0x000400;
	DrvColRAM0		= Next; Next += 0x000600;
	DrvColRAM1		= Next; Next += 0x000400;

	Drv6502RAM1		= Next; Next += 0x000200;

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
	INT32 Plane0[3] = { 0x20000, 0x10000, 0x00000 };
	INT32 Plane1[3] = { 0xa0000, 0x50000, 0x00000 };
	INT32 XOffsc[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };//STEP8(16,1), STEP8(0,1) };
	INT32 YOffsc[8] = { 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 };//STEP8(0,8), STEP8(64,8) };

	INT32 XOffst[16] = { 16*8 + 0, 16*8 + 1, 16*8 + 2, 16*8 + 3, 16*8 + 4, 16*8 + 5, 16*8 + 6, 16*8 + 7,
			0, 1, 2, 3, 4, 5, 6, 7 };
	INT32 YOffst[16] = { 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 };

	INT32 XOffss[16] = { 16*8 + 0, 16*8 + 1, 16*8 + 2, 16*8 + 3, 16*8 + 4, 16*8 + 5, 16*8 + 6, 16*8 + 7,
			0, 1, 2, 3, 4, 5, 6, 7 };
	INT32 YOffss[16] = { 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x1e000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x06000);

	GfxDecode(0x0400, 3,  8,  8, Plane0, XOffsc, YOffsc, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x06000);

	GfxDecode(0x0100, 3, 16, 16, Plane0, XOffst, YOffst, 0x100, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x1e000);

	GfxDecode(0x0500, 3, 16, 16, Plane1, XOffss, YOffss, 0x100, tmp, DrvGfxROM2);

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
		if (BurnLoadRom(Drv6502ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(Drv6502ROM0 + 0x02000,  1, 1)) return 1;
		if (BurnLoadRom(Drv6502ROM0 + 0x04000,  2, 1)) return 1;
		if (BurnLoadRom(Drv6502ROM0 + 0x06000,  3, 1)) return 1;
		if (BurnLoadRom(Drv6502ROM0 + 0x08000,  4, 1)) return 1;

		if (BurnLoadRom(Drv6502ROM1 + 0x00000,  5, 1)) return 1;
		if (BurnLoadRom(Drv6502ROM1 + 0x02000,  6, 1)) return 1;
		if (BurnLoadRom(Drv6502ROM1 + 0x04000,  7, 1)) return 1;
		if (BurnLoadRom(Drv6502ROM1 + 0x06000,  8, 1)) return 1;
		if (BurnLoadRom(Drv6502ROM1 + 0x08000,  9, 1)) return 1;
		if (BurnLoadRom(Drv6502ROM1 + 0x0a000, 10, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0  + 0x00000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x02000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x04000, 13, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1  + 0x00000, 14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x02000, 15, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x04000, 16, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2  + 0x00000, 17, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2  + 0x02000, 18, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2  + 0x04000, 19, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2  + 0x06000, 20, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2  + 0x08000, 21, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2  + 0x0a000, 22, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2  + 0x0c000, 23, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2  + 0x0e000, 24, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2  + 0x10000, 25, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2  + 0x12000, 26, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2  + 0x14000, 27, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2  + 0x16000, 28, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2  + 0x18000, 29, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2  + 0x1a000, 30, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2  + 0x1c000, 31, 1)) return 1;

		if (BurnLoadRom(DrvColPROM  + 0x00000, 32, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00020, 33, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00040, 34, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00060, 35, 1)) return 1;

		DrvGfxDecode();
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvSprRAM,	0x0000, 0x07ff, MAP_RAM);
	M6502MapMemory(DrvVidRAM1,	0x2000, 0x23ff, MAP_RAM);
	M6502MapMemory(DrvColRAM1,	0x2400, 0x27ff, MAP_RAM);
	M6502MapMemory(DrvVidRAM0,	0x3000, 0x31ff, MAP_RAM);
	M6502MapMemory(DrvColRAM0,	0x3200, 0x37ff, MAP_RAM);
	M6502MapMemory(Drv6502ROM0,	0x6000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(ssozumo_main_write);
	M6502SetReadHandler(ssozumo_main_read);
	M6502Close();

	M6502Init(1, TYPE_M6502);
	M6502Open(1);
	M6502MapMemory(Drv6502RAM1,	0x0000, 0x01ff, MAP_RAM);
	M6502MapMemory(Drv6502ROM1,	0x4000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(ssozumo_sound_write);
	M6502SetReadHandler(ssozumo_sound_read);

	M6502Close();

	AY8910Init(0, 1500000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910Init(1, 1500000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.30, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.30, BURN_SND_ROUTE_BOTH);

	DACInit(0, 0, 1, DrvSyncDAC);
	DACSetRoute(0, 0.30, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6502Exit();

	AY8910Exit(0);
	AY8910Exit(1);
	DACExit();

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0 ; i < 0x40 ; i++)
	{
		INT32 bit0, bit1, bit2, bit3;

		bit0 = (DrvColPROM[i+0x00] >> 0) & 0x01;
		bit1 = (DrvColPROM[i+0x00] >> 1) & 0x01;
		bit2 = (DrvColPROM[i+0x00] >> 2) & 0x01;
		bit3 = (DrvColPROM[i+0x00] >> 3) & 0x01;
		INT32 r = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[i+0x00] >> 4) & 0x01;
		bit1 = (DrvColPROM[i+0x00] >> 5) & 0x01;
		bit2 = (DrvColPROM[i+0x00] >> 6) & 0x01;
		bit3 = (DrvColPROM[i+0x00] >> 7) & 0x01;
		INT32 g = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[i+0x40] >> 0) & 0x01;
		bit1 = (DrvColPROM[i+0x40] >> 1) & 0x01;
		bit2 = (DrvColPROM[i+0x40] >> 2) & 0x01;
		bit3 = (DrvColPROM[i+0x40] >> 3) & 0x01;
		INT32 b = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void DrvPaletteUpdate()
{
	for (INT32 i = 0; i < 0x10; i++)
	{
		INT32 bit0, bit1, bit2, bit3, val;

		val = DrvPalRAM[i];
		bit0 = (val >> 0) & 0x01;
		bit1 = (val >> 1) & 0x01;
		bit2 = (val >> 2) & 0x01;
		bit3 = (val >> 3) & 0x01;
		INT32 r = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		val = DrvPalRAM[i + 0x10];
		bit0 = (val >> 0) & 0x01;
		bit1 = (val >> 1) & 0x01;
		bit2 = (val >> 2) & 0x01;
		bit3 = (val >> 3) & 0x01;
		INT32 g = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		val = DrvPalRAM[i + 0x20];
		bit0 = (val >> 0) & 0x01;
		bit1 = (val >> 1) & 0x01;
		bit2 = (val >> 2) & 0x01;
		bit3 = (val >> 3) & 0x01;
		INT32 b = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		DrvPalette[0x40 + i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_bg_layer()
{
	for (INT32 offs = 0; offs < 16 * 32; offs++)
	{
		INT32 sx = (0xf - (offs / 0x20)) * 16;
		INT32 sy = (offs & 0x1f) * 16;

		sy -= bgscrolly;
		sy -= 8; //offset

		if (sy < -15) sy += 512;

		INT32 attr  = DrvColRAM0[offs];
		INT32 code  = DrvVidRAM0[offs] + ((attr & 0x08) << 5);
		INT32 color = (attr & 0x30) >> 4;
		INT32 flipy = (offs % 32) >= 16;
		code &= 0x4ff;

		if (flipy)
		{
			Render16x16Tile_FlipY_Clip(pTransDraw, code, sx, sy, color, 3, 0x20, DrvGfxROM1);
		}
		else
		{
			Render16x16Tile_Clip(pTransDraw, code, sx, sy, color, 3, 0x20, DrvGfxROM1);
		}
	}
}

static void draw_fg_layer()
{
	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sx = 239-((offs / 0x20) * 8);
		INT32 sy = (offs & 0x1f) * 8;

		sx += 9; //offsets (8 + 1 to give the screen a 3d-effect)
		sy -= 8;

		INT32 color  = (DrvColRAM1[offs] & 0x30) >> 4;
		INT32 code  = DrvVidRAM1[offs] + 256 * (DrvColRAM1[offs] & 0x07);
		code &= 0x3ff;

		Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, DrvGfxROM0);
	}
}

static void draw_sprites()
{
	UINT8 *spriteram = DrvSprRAM + 0x780;

	for (INT32 offs = 0; offs < 0x80; offs += 4)
	{
		if (spriteram[offs] & 0x01)
		{
			INT32 code  = spriteram[offs + 1] + ((spriteram[offs] & 0xf0) << 4);
			INT32 color = (spriteram[offs] & 0x08) >> 3;
			INT32 flipx = spriteram[offs] & 0x04;
			INT32 flipy = spriteram[offs] & 0x02;
			INT32 sx    = 239 - spriteram[offs + 3];
			INT32 sy    = (240 - spriteram[offs + 2]) & 0xff;

			sy -= 8; //offset

			if (code >= 0x500) code = (code & 0xff) | (((code/0x100)%6)*0x100); // better to mod, and, or blank?

			if (flipscreen)
			{
				sx = 240 - sx;
				sy = 240 - sy;
				flipx = !flipx;
				flipy = !flipy;
			}

			if (flipy) {
				if (flipx) {
					Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0x40, DrvGfxROM2);
				} else {
					Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0x40, DrvGfxROM2);
				}
			} else {
				if (flipx) {
					Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0x40, DrvGfxROM2);
				} else {
					Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0x40, DrvGfxROM2);
				}
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc || palette_written) {
		DrvPaletteUpdate();
		palette_written = 0;
	}

	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	BurnTransferClear();

	if (nBurnLayer & 1) draw_bg_layer();
	if (nBurnLayer & 2) draw_fg_layer();
	if (nBurnLayer & 4) draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	M6502NewFrame();

	{
		memset (DrvInputs, 0xff, 2);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}

		if ((((DrvInputs[0] & 0x40) == 0x00) || ((DrvInputs[0] & 0x80) == 0x00)) && previous_coin == 0xc0) {
			M6502Open(0);
			M6502SetIRQLine(0x20, CPU_IRQSTATUS_AUTO); // nmi
			M6502Close();
		}

		previous_coin = DrvInputs[0] & 0xc0;
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 1200000 / 60, 975000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };
	INT32 nSegment = 0;

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		M6502Open(0);
		if (i == 240) {
			vblank = 1;
			M6502SetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
		nSegment = (nCyclesTotal[0] - nCyclesDone[0]) / (nInterleave - i);
		nCyclesDone[0] += M6502Run(nSegment);
		M6502Close();

		M6502Open(1);
		nCyclesDone[1] += M6502Run(nCyclesTotal[1] / nInterleave);
		if (nmi_mask && (i & 0xf) == 0xf) M6502SetIRQLine(0x20, CPU_IRQSTATUS_AUTO); // nmi (16.15 times per frame)
		M6502Close();
	}

	if (pBurnSoundOut) {
		AY8910Render(&pAY8910Buffer[0], pBurnSoundOut, nBurnSoundLen, 0);
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
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

		M6502Scan(nAction);
		DACScan(nAction, pnMin);
		AY8910Scan(nAction, pnMin);

		SCAN_VAR(palette_written);
		SCAN_VAR(nmi_mask);
		SCAN_VAR(soundlatch);
		SCAN_VAR(flipscreen);
		SCAN_VAR(bgscrolly);
	}

	return 0;
}


// Syusse Oozumou (Japan)

static struct BurnRomInfo ssozumoRomDesc[] = {
	{ "ic61.g01",	0x2000, 0x86968f46, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 #0 Code
	{ "ic60.g11",	0x2000, 0x1a5143dd, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ic59.g21",	0x2000, 0xd3df04d7, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ic58.g31",	0x2000, 0x0ee43a78, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ic57.g41",	0x2000, 0xac77aa4c, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "ic47.g50",	0x2000, 0xb64ec829, 2 | BRF_PRG | BRF_ESS }, //  5 M6502 #1 Code
	{ "ic46.g60",	0x2000, 0x630d7380, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "ic45.g70",	0x2000, 0x1854b657, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "ic44.g80",	0x2000, 0x40b9a0da, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "ic43.g90",	0x2000, 0x20262064, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "ic42.ga0",	0x2000, 0x98d7e998, 2 | BRF_PRG | BRF_ESS }, // 10

	{ "ic22.gq0",	0x2000, 0xb4c7e612, 3 | BRF_GRA },           // 11 Characters
	{ "ic23.gr0",	0x2000, 0x90bb9fda, 3 | BRF_GRA },           // 12
	{ "ic21.gs0",	0x2000, 0xd8cd5c78, 3 | BRF_GRA },           // 13

	{ "ic69.gt0",	0x2000, 0x771116ca, 4 | BRF_GRA },           // 14 Background Tiles
	{ "ic59.gu0",	0x2000, 0x68035bfd, 4 | BRF_GRA },           // 15
	{ "ic81.gv0",	0x2000, 0xcdda1f9f, 4 | BRF_GRA },           // 16

	{ "ic06.gg0",	0x2000, 0xd2342c50, 5 | BRF_GRA },           // 17 Sprites
	{ "ic05.gh0",	0x2000, 0x14a3cb10, 5 | BRF_GRA },           // 18
	{ "ic04.gi0",	0x2000, 0x169276c1, 5 | BRF_GRA },           // 19
	{ "ic03.gj0",	0x2000, 0xe71b9f28, 5 | BRF_GRA },           // 20
	{ "ic02.gk0",	0x2000, 0x6e94773c, 5 | BRF_GRA },           // 21
	{ "ic29.gl0",	0x2000, 0x40f67cc4, 5 | BRF_GRA },           // 22
	{ "ic28.gm0",	0x2000, 0x8c97b1a2, 5 | BRF_GRA },           // 23
	{ "ic27.gn0",	0x2000, 0xbe8bb3dd, 5 | BRF_GRA },           // 24
	{ "ic26.go0",	0x2000, 0x9c098a2c, 5 | BRF_GRA },           // 25
	{ "ic25.gp0",	0x2000, 0xf73f8a76, 5 | BRF_GRA },           // 26
	{ "ic44.gb0",	0x2000, 0xcdd7f2eb, 5 | BRF_GRA },           // 27
	{ "ic43.gc0",	0x2000, 0x7b4c632e, 5 | BRF_GRA },           // 28
	{ "ic42.gd0",	0x2000, 0xcd1c8fe6, 5 | BRF_GRA },           // 29
	{ "ic41.ge0",	0x2000, 0x935578d0, 5 | BRF_GRA },           // 30
	{ "ic40.gf0",	0x2000, 0x5a3bf1ba, 5 | BRF_GRA },           // 31

	{ "ic33.gz0",	0x0020, 0x523d29ad, 6 | BRF_GRA },           // 32 Color data
	{ "ic30.gz2",	0x0020, 0x0de202e1, 6 | BRF_GRA },           // 33
	{ "ic32.gz1",	0x0020, 0x6fbff4d2, 6 | BRF_GRA },           // 34
	{ "ic31.gz3",	0x0020, 0x18e7fe63, 6 | BRF_GRA },           // 35
};

STD_ROM_PICK(ssozumo)
STD_ROM_FN(ssozumo)

struct BurnDriver BurnDrvSsozumo = {
	"ssozumo", NULL, NULL, NULL, "1984",
	"Syusse Oozumou (Japan)\0", NULL, "Technos Japan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, ssozumoRomInfo, ssozumoRomName, NULL, NULL, SsozumoInputInfo, SsozumoDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x50,
	240, 256, 3, 4
};

