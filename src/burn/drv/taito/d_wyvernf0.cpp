// FB Alpha Wyvern F-0 driver module
// Based on MAME driver by Luca Elia

#include "tiles_generic.h"
#include "z80_intf.h"
#include "driver.h"
extern "C" {
#include "ay8910.h"
}
#include "msm5232.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvFgRAM;
static UINT8 *DrvBgRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvObjRAM;
static UINT8 *DrvZ80RAM1;

static UINT8 *soundlatch;
static UINT8 *flipscreen;
static UINT8 *coin_lockout;
static UINT8 *scroll;
static UINT8 *pending_nmi;
static UINT8 *nmi_enable;
static UINT8 *DrvZ80ROMBank;
static UINT8 *DrvZ80RAMBank;
static UINT8 *mcu_value;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT16 *pAY8910Buffer[6];

static UINT8  DrvJoy1[8];
static UINT8  DrvJoy2[8];
static UINT8  DrvJoy3[8];
static UINT8  DrvJoy4[8];
static UINT8  DrvJoy5[8];
static UINT8  DrvDips[3];
static UINT8  DrvInputs[5];
static UINT8  DrvReset;

static struct BurnInputInfo Wyvernf0InputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy4 + 5,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy4 + 4,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy4 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy4 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy5 + 5,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy5 + 4,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy5 + 3,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy1 + 3,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Wyvernf0)

static struct BurnDIPInfo Wyvernf0DIPList[]=
{
	{0x15, 0xff, 0xff, 0x6f, NULL			},
	{0x16, 0xff, 0xff, 0x00, NULL			},
	{0x17, 0xff, 0xff, 0xdc, NULL			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x15, 0x01, 0x03, 0x00, "?? 0"			},
	{0x15, 0x01, 0x03, 0x01, "?? 1"			},
	{0x15, 0x01, 0x03, 0x02, "?? 2"			},
	{0x15, 0x01, 0x03, 0x03, "?? 3"			},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x15, 0x01, 0x04, 0x04, "Off"			},
	{0x15, 0x01, 0x04, 0x00, "On"			},
	
	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x15, 0x01, 0x18, 0x00, "2"			},
	{0x15, 0x01, 0x18, 0x08, "3"			},
	{0x15, 0x01, 0x18, 0x10, "4"			},
	{0x15, 0x01, 0x18, 0x18, "5"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x15, 0x01, 0x40, 0x40, "Off"			},
	{0x15, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x15, 0x01, 0x80, 0x00, "Upright"		},
	{0x15, 0x01, 0x80, 0x80, "Cocktail"		},

	{0   , 0xfe, 0   ,    16, "Coin A"		},
	{0x16, 0x01, 0x0f, 0x0f, "9 Coins 1 Credits"	},
	{0x16, 0x01, 0x0f, 0x0e, "8 Coins 1 Credits"	},
	{0x16, 0x01, 0x0f, 0x0d, "7 Coins 1 Credits"	},
	{0x16, 0x01, 0x0f, 0x0c, "6 Coins 1 Credits"	},
	{0x16, 0x01, 0x0f, 0x0b, "5 Coins 1 Credits"	},
	{0x16, 0x01, 0x0f, 0x0a, "4 Coins 1 Credits"	},
	{0x16, 0x01, 0x0f, 0x09, "3 Coins 1 Credits"	},
	{0x16, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"	},
	{0x16, 0x01, 0x0f, 0x00, "1 Coin  1 Credits"	},
	{0x16, 0x01, 0x0f, 0x01, "1 Coin  2 Credits"	},
	{0x16, 0x01, 0x0f, 0x02, "1 Coin  3 Credits"	},
	{0x16, 0x01, 0x0f, 0x03, "1 Coin  4 Credits"	},
	{0x16, 0x01, 0x0f, 0x04, "1 Coin  5 Credits"	},
	{0x16, 0x01, 0x0f, 0x05, "1 Coin  6 Credits"	},
	{0x16, 0x01, 0x0f, 0x06, "1 Coin  7 Credits"	},
	{0x16, 0x01, 0x0f, 0x07, "1 Coin  8 Credits"	},

	{0   , 0xfe, 0   ,    16, "Coin B"		},
	{0x16, 0x01, 0xf0, 0xf0, "9 Coins 1 Credits"	},
	{0x16, 0x01, 0xf0, 0xe0, "8 Coins 1 Credits"	},
	{0x16, 0x01, 0xf0, 0xd0, "7 Coins 1 Credits"	},
	{0x16, 0x01, 0xf0, 0xc0, "6 Coins 1 Credits"	},
	{0x16, 0x01, 0xf0, 0xb0, "5 Coins 1 Credits"	},
	{0x16, 0x01, 0xf0, 0xa0, "4 Coins 1 Credits"	},
	{0x16, 0x01, 0xf0, 0x90, "3 Coins 1 Credits"	},
	{0x16, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"	},
	{0x16, 0x01, 0xf0, 0x00, "1 Coin  1 Credits"	},
	{0x16, 0x01, 0xf0, 0x10, "1 Coin  2 Credits"	},
	{0x16, 0x01, 0xf0, 0x20, "1 Coin  3 Credits"	},
	{0x16, 0x01, 0xf0, 0x30, "1 Coin  4 Credits"	},
	{0x16, 0x01, 0xf0, 0x40, "1 Coin  5 Credits"	},
	{0x16, 0x01, 0xf0, 0x50, "1 Coin  6 Credits"	},
	{0x16, 0x01, 0xf0, 0x60, "1 Coin  7 Credits"	},
	{0x16, 0x01, 0xf0, 0x70, "1 Coin  8 Credits"	},

	{0   , 0xfe, 0   ,    2, "Coinage Display"	},
	{0x17, 0x01, 0x10, 0x00, "No"			},
	{0x17, 0x01, 0x10, 0x10, "Yes"			},

	{0   , 0xfe, 0   ,    2, "Copyright"		},
	{0x17, 0x01, 0x20, 0x00, "Taito Corporation"	},
	{0x17, 0x01, 0x20, 0x20, "Taito Corp. 1985"	},

	{0   , 0xfe, 0   ,    2, "Invulnerability"	},
	{0x17, 0x01, 0x40, 0x40, "Off"			},
	{0x17, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Coin Slots"		},
	{0x17, 0x01, 0x80, 0x00, "1"			},
	{0x17, 0x01, 0x80, 0x80, "2"			},
};

STDDIPINFO(Wyvernf0)

static inline void palette_update(INT32 i)
{
	INT32 r = DrvPalRAM[i+0] & 0x0f;
	INT32 g = DrvPalRAM[i+1] >> 4;
	INT32 b = DrvPalRAM[i+1] & 0x0f;

	DrvPalette[i/2] = BurnHighCol(r | (r << 4), g | (g << 4) ,b | (b << 4), 0);
}

static void rambankswitch(INT32 data)
{
	INT32 bank = (data & 0x80) ? 0x1000 : 0;

	*DrvZ80RAMBank = data;

	*coin_lockout = (~data & 0x40) ? 0xcf : 0xff;

	*flipscreen = data & 0x03;

	ZetMapMemory(DrvObjRAM + bank, 0x9000, 0x9fff, MAP_RAM);
}

static void rombankswitch(INT32 data)
{
	INT32 bank = 0x10000 + (data & 0x07) * 0x2000;

	*DrvZ80ROMBank = data;

	ZetMapMemory(DrvZ80ROM0 + bank,	0xa000, 0xbfff, MAP_ROM);
}

static void __fastcall wyvernf0_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfc00) == 0xd800) {
		DrvPalRAM[address & 0x3ff] = data;
		palette_update(address & 0x3fe);
		return;
	}

	switch (address)
	{
		case 0xd000:
		return;		// nop

		case 0xd100:
			rambankswitch(data);
		return;

		case 0xd200:
			rombankswitch(data);
		return;

		case 0xd300:
		case 0xd301:
		case 0xd302:
		case 0xd303:
			scroll[address & 0x03] = data;
		return;

		case 0xd400:
			*mcu_value = data;
		return;

		case 0xd610:
		{
			*soundlatch = data;
			if (*nmi_enable) {
				ZetClose();
				ZetOpen(1);
				ZetNmi();
				ZetClose();
				ZetOpen(0);
			} else {
				*pending_nmi = 1;
			}
		}
		return;

		case 0xdc00:
		return; // nop
	}
}

static UINT8 __fastcall wyvernf0_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xd400:
			return ((*mcu_value & 0x73) == 0x73) ? 0x42: 0;

		case 0xd401:
			return 0x03; // mcu status

		case 0xd600:
		case 0xd601:
		case 0xd602:
			return DrvDips[address - 0xd600];

		case 0xd603:
		case 0xd604:
		case 0xd605:
		case 0xd606:
		case 0xd607:
			return DrvInputs[address - 0xd603];

		case 0xd610:
			return *soundlatch;
	}

	return 0;
}

static void __fastcall wyvernf0_sound_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfff0) == 0xc900) {
		MSM5232Write(address & 0x0f, data);
		return;
	}

	switch (address)
	{
		case 0xc800:
		case 0xc801:
		case 0xc802:
		case 0xc803:
			AY8910Write((address/2) & 1, address & 1, data);
			if (data==0x88) { // end of sound command, turn off all channels to get rid of unwanted high pitched noises
				AY8910Write((address/2) & 1, 0, 0x08);
				AY8910Write((address/2) & 1, 1, 0x00);
				AY8910Write((address/2) & 1, 0, 0x09);
				AY8910Write((address/2) & 1, 1, 0x00);
				AY8910Write((address/2) & 1, 0, 0x0A);
				AY8910Write((address/2) & 1, 1, 0x00);
			}
		return;

		case 0xd000:
			*soundlatch = data;
		return;

		case 0xd200:
		{
			*nmi_enable = 1;

			if (*pending_nmi)
			{
				ZetNmi();
				*pending_nmi = 0;
			}
		}
		return;

		case 0xd400:
			*nmi_enable = 0;
		return;

		case 0xd600:
		return; // ??
	}
}

static UINT8 __fastcall wyvernf0_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xd000:
			return *soundlatch;
	}

	return 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	rombankswitch(0);
	rambankswitch(0);
	ZetClose();

	ZetOpen(1);
	ZetReset();
	ZetClose();

	AY8910Reset(0);
	AY8910Reset(1);

	MSM5232Reset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x020000;
	DrvZ80ROM1		= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += 0x020000;
	DrvGfxROM1		= Next; Next += 0x010000;

	DrvPalette		= (UINT32*)Next; Next += 0x0200 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM0		= Next; Next += 0x001000;
	DrvFgRAM		= Next; Next += 0x000800;
	DrvBgRAM		= Next; Next += 0x000800;
	DrvSprRAM		= Next; Next += 0x000100;
	DrvPalRAM		= Next; Next += 0x000400;
	DrvObjRAM		= Next; Next += 0x002000;

	DrvZ80RAM1		= Next; Next += 0x000800;

	soundlatch		= Next; Next += 0x000001;
	flipscreen		= Next; Next += 0x000001;
	coin_lockout		= Next; Next += 0x000001;
	pending_nmi		= Next; Next += 0x000001;
	nmi_enable		= Next; Next += 0x000001;
	scroll			= Next; Next += 0x000004;
	DrvZ80ROMBank		= Next; Next += 0x000001;
	DrvZ80RAMBank		= Next; Next += 0x000001;
	mcu_value		= Next; Next += 0x000001;

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

static void DrvGfxDecode(UINT8 *rom, INT32 len)
{
	INT32 Planes[4] = { RGN_FRAC(len,0,4), RGN_FRAC(len,1,4), RGN_FRAC(len,2,4), RGN_FRAC(len,3,4) };
	INT32 XOffs[8] = { STEP8(7,-1) };
	INT32 YOffs[8] = { STEP8(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(len);
	if (tmp == NULL) {
		return;
	}

	memcpy (tmp, rom, len);

	GfxDecode(((len/4)*8)/(8*8), 4, 8, 8, Planes, XOffs, YOffs, 0x040, tmp, rom);

	BurnFree(tmp);
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
		if (BurnLoadRom(DrvZ80ROM0 + 0x04000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x10000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x14000,  3, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x18000,  4, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x1c000,  5, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  6, 1)) return 1;
		memset (DrvZ80ROM1 + 0xe000, 0xff, 0x2000);

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x04000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x08000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0c000, 10, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x02000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x04000, 13, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x06000, 14, 1)) return 1;

		DrvGfxDecode(DrvGfxROM0, 0x10000);
		DrvGfxDecode(DrvGfxROM1, 0x08000);
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,		0x8000, 0x8fff, MAP_RAM);
	ZetMapMemory(DrvFgRAM,			0xc000, 0xc7ff, MAP_RAM);
	ZetMapMemory(DrvBgRAM,			0xc800, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0xd500, 0xd5ff, MAP_RAM);
	ZetMapMemory(DrvPalRAM,			0xd800, 0xdbff, MAP_ROM);
	ZetSetWriteHandler(wyvernf0_main_write);
	ZetSetReadHandler(wyvernf0_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,		0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,		0xc000, 0xc7ff, MAP_RAM);
	ZetMapMemory(DrvZ80ROM1 + 0xe000,	0xe000, 0xefff, MAP_ROM);
	ZetSetWriteHandler(wyvernf0_sound_write);
	ZetSetReadHandler(wyvernf0_sound_read);
	ZetClose();

	AY8910Init(0, 3000000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.14, BURN_SND_ROUTE_BOTH);

	AY8910Init(1, 3000000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(1, 0.14, BURN_SND_ROUTE_BOTH);

	MSM5232Init(2000000, 1);
	MSM5232SetCapacitors(0.39e-6, 0.39e-6, 0.39e-6, 0.39e-6, 0.39e-6, 0.39e-6, 0.39e-6, 0.39e-6);
	MSM5232SetRoute(0.50, BURN_SND_MSM5232_ROUTE_0);
	MSM5232SetRoute(0.50, BURN_SND_MSM5232_ROUTE_1);
	MSM5232SetRoute(0.50, BURN_SND_MSM5232_ROUTE_2);
	MSM5232SetRoute(0.50, BURN_SND_MSM5232_ROUTE_3);
	MSM5232SetRoute(0.50, BURN_SND_MSM5232_ROUTE_4);
	MSM5232SetRoute(0.50, BURN_SND_MSM5232_ROUTE_5);
	MSM5232SetRoute(0.50, BURN_SND_MSM5232_ROUTE_6);
	MSM5232SetRoute(0.50, BURN_SND_MSM5232_ROUTE_7);

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

	MSM5232Exit();

	BurnFree(AllMem);

	return 0;
}

static void draw_layer(UINT8 *ram, INT32 color_offset, UINT8 scrollx, UINT8 scrolly)
{
	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sx = (offs & 0x1f) * 8;
		INT32 sy = (offs / 0x20) * 8;

		sx -= scrollx;
		if (sx < -7) sx += 256;
		sy -= scrolly;
		if (sy < -7) sy += 256;

		if (sy >= 224) continue;

		INT32 code = ((ram[offs * 2 + 1] & 0x03) << 8) + ram[offs * 2];

		INT32 color = (code >> 12) & 0x07;
		INT32 flipx = (code >> 15) & 0x01;
		INT32 flipy = (code >> 14) & 0x01;

		if (flipy) {
			if (flipx) {
				Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, code & 0x3ff, sx, sy, color, 4, 0, color_offset, DrvGfxROM1);
			} else {
				Render8x8Tile_Mask_FlipY_Clip(pTransDraw, code & 0x3ff, sx, sy, color, 4, 0, color_offset, DrvGfxROM1);
			}
		} else {
			if (flipx) {
				Render8x8Tile_Mask_FlipX_Clip(pTransDraw, code & 0x3ff, sx, sy, color, 4, 0, color_offset, DrvGfxROM1);
			} else {
				Render8x8Tile_Mask_Clip(pTransDraw, code & 0x3ff, sx, sy, color, 4, 0, color_offset, DrvGfxROM1);
			}
		}
	}
}

static void draw_sprites(INT32 is_foreground)
{
	UINT8 *RAM = DrvSprRAM + (is_foreground ? 0x80 : 0);

	for (INT32 offs = 0; offs < 0x100 / 2; offs += 4)
	{
		INT32 sx = RAM[offs + 3] - ((RAM[offs + 2] & 0x80) << 1);
		INT32 sy = 256 - 8 - RAM[offs + 0] - 23;

		INT32 flipx = RAM[offs + 2] & 0x40;
		INT32 flipy = RAM[offs + 1] & 0x80;

		if (*flipscreen & 0x01)
		{
			flipx = !flipx;
			sx = 256 - 8 - sx - 3 * 8;
		}
		if (*flipscreen & 0x02)
		{
			flipy = !flipy;
			sy = 256 - 8 - sy - 3 * 8;
		}

		INT32 code  = (RAM[offs + 1] & 0x7f) + (is_foreground ? 0x80 : 0);
		INT32 color = (RAM[offs + 2] & 0x0f) + (is_foreground ? 0x10 : 0);

		for (INT32 y = 0; y < 4; y++)
		{
			for (INT32 x = 0; x < 4; x++)
			{
				INT32 objoffs = code * 0x20 + (x + y * 4) * 2;

				INT32 sxx = sx + (flipx ? 3-x : x) * 8;
				INT32 syy = sy + (flipy ? 3-y : y) * 8;

				INT32 code1 = ((DrvObjRAM[objoffs + 1] & 0x07) << 8) + DrvObjRAM[objoffs];

				if (flipy) {
					if (flipx) {
						Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, code1, sxx, syy - 16, color, 4, 0, 0, DrvGfxROM0);
					} else {
						Render8x8Tile_Mask_FlipY_Clip(pTransDraw, code1, sxx, syy - 16, color, 4, 0, 0, DrvGfxROM0);
					}
				} else {
					if (flipx) {
						Render8x8Tile_Mask_FlipX_Clip(pTransDraw, code1, sxx, syy - 16, color, 4, 0, 0, DrvGfxROM0);
					} else {
						Render8x8Tile_Mask_Clip(pTransDraw, code1, sxx, syy - 16, color, 4, 0, 0, DrvGfxROM0);
					}
				}
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x400; i+=2) {
			palette_update(i);
		}
		DrvRecalc = 0;
	}

	BurnTransferClear();

	draw_layer(DrvBgRAM, 0x00, scroll[2] - 0x12, scroll[3] + 0x10);
	draw_sprites(0);
	draw_sprites(1);
	draw_layer(DrvFgRAM, 0x80, scroll[0] - 0x10, scroll[1] + 0x10);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0, 5);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
			DrvInputs[4] ^= (DrvJoy5[i] & 1) << i;
		}

		DrvInputs[0] = (DrvInputs[0] & *coin_lockout);// | 0xc0;
	}

	INT32 nInterleave = 10;
	INT32 nCyclesTotal[2] = { 6000000 / 60, 4000000 / 60 };
	INT32 nCyclesDone[2]  = { 0 , 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		INT32 nSegment = nCyclesTotal[0] / nInterleave;
		nCyclesDone[0] += ZetRun(nSegment);
		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_AUTO);
		ZetClose();

		ZetOpen(1);
		nSegment = nCyclesTotal[1] / nInterleave;
		nCyclesDone[1] += ZetRun(nSegment);
		if (i == (nInterleave - 1) || i == (nInterleave / 2) - 1) ZetSetIRQLine(0, CPU_IRQSTATUS_AUTO);
		ZetClose();
	}

	if (pBurnSoundOut) {
		AY8910Render(&pAY8910Buffer[0], pBurnSoundOut, nBurnSoundLen, 0);
		MSM5232Update(pBurnSoundOut, nBurnSoundLen);
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
		MSM5232Scan(nAction, pnMin);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		rambankswitch(*DrvZ80RAMBank);
		rombankswitch(*DrvZ80ROMBank);
		ZetClose();

		DrvRecalc = 1;
	}

	return 0;
}


// Wyvern F-0

static struct BurnRomInfo wyvernf0RomDesc[] = {
	{ "a39_01-1.ic37",	0x4000, 0xa94887ec, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "a39_02-1.ic36",	0x4000, 0x171cfdbe, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a39_03.ic35",	0x4000, 0x50314281, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "a39_04.ic34",	0x4000, 0x7a225bf9, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "a39_05.ic33",	0x4000, 0x41f21a67, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "a39_06.ic32",	0x4000, 0xdeb2d850, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "a39_16.ic26",	0x4000, 0x5a681fb4, 2 | BRF_PRG | BRF_ESS }, //  6 Z80 #0 Code

	{ "a39_11.ic99",	0x4000, 0xaf70e1dc, 3 | BRF_GRA },           //  7 Sprites
	{ "a39_10.ic78",	0x4000, 0xa84380fb, 3 | BRF_GRA },           //  8
	{ "a39_09.ic96",	0x4000, 0xc0cee243, 3 | BRF_GRA },           //  9
	{ "a39_08.ic75",	0x4000, 0x0ad69501, 3 | BRF_GRA },           // 10

	{ "a39_14.ic99",	0x2000, 0x90a66147, 4 | BRF_GRA },           // 11 Tiles
	{ "a39_14.ic73",	0x2000, 0xa31f3507, 4 | BRF_GRA },           // 12
	{ "a39_13.ic100",	0x2000, 0xbe708238, 4 | BRF_GRA },           // 13
	{ "a39_12.ic74",	0x2000, 0x1cc389de, 4 | BRF_GRA },           // 14

	{ "a39_mcu.icxx",	0x0800, 0x00000000, 4 | BRF_OPT | BRF_PRG | BRF_NODUMP }, //  15 MCU Code (not dumped)
};

STD_ROM_PICK(wyvernf0)
STD_ROM_FN(wyvernf0)

struct BurnDriver BurnDrvWyvernf0 = {
	"wyvernf0", NULL, NULL, NULL, "1985",
	"Wyvern F-0\0", NULL, "Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2,  HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, wyvernf0RomInfo, wyvernf0RomName, NULL, NULL, Wyvernf0InputInfo, Wyvernf0DIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};
