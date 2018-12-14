// FB Alpha IronHorse driver module
// Based on MAME driver by Mirko Buffoni

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "z80_intf.h"
#include "burn_ym2203.h"
#include "konamiic.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6809ROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvColPROM;
static UINT8 *DrvM6809RAM0;
static UINT8 *DrvColRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvM6809RAM1;
static UINT8 *DrvSprRAM0;
static UINT8 *DrvSprRAM1;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvCharBank;
static UINT8 *DrvIRQEnable;
static UINT8 *DrvScrollRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 soundlatch;
static UINT8 palettebank;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo DairesyaInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Dairesya)


static struct BurnInputInfo IronhorsInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy3 + 6,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Ironhors)

static struct BurnDIPInfo IronhorsDIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL			},
	{0x15, 0xff, 0xff, 0x5a, NULL			},
	{0x16, 0xff, 0xff, 0xfe, NULL			},

	{0   , 0xfe, 0   ,   16, "Free Play"		},
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
	{0x14, 0x01, 0x0f, 0x00, ":1,2,3,4"		},

	{0   , 0xfe, 0   ,   16, "Coin B"		},
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
	{0x15, 0x01, 0x03, 0x03, "2"			},
	{0x15, 0x01, 0x03, 0x02, "3"			},
	{0x15, 0x01, 0x03, 0x01, "5"			},
	{0x15, 0x01, 0x03, 0x00, "7"			},

//	{0   , 0xfe, 0   ,    2, "Cabinet"		},
//	{0x15, 0x01, 0x04, 0x00, "Upright"		},
//	{0x15, 0x01, 0x04, 0x04, "Cocktail"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x15, 0x01, 0x18, 0x18, "30K 70K+"		},
	{0x15, 0x01, 0x18, 0x10, "40K 80K+"		},
	{0x15, 0x01, 0x18, 0x08, "40K"			},
	{0x15, 0x01, 0x18, 0x00, "50K"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x15, 0x01, 0x60, 0x60, "Easy"			},
	{0x15, 0x01, 0x60, 0x40, "Normal"		},
	{0x15, 0x01, 0x60, 0x20, "Hard"			},
	{0x15, 0x01, 0x60, 0x00, "Very Hard"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x15, 0x01, 0x80, 0x80, "Off"			},
	{0x15, 0x01, 0x80, 0x00, "On"			},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
//	{0x16, 0x01, 0x01, 0x00, "Off"			},
//	{0x16, 0x01, 0x01, 0x01, "On"			},

	{0   , 0xfe, 0   ,    2, "Upright Controls"	},
	{0x16, 0x01, 0x02, 0x02, "Single"		},
	{0x16, 0x01, 0x02, 0x00, "Dual"			},

	{0   , 0xfe, 0   ,    2, "Button Layout"	},
	{0x16, 0x01, 0x04, 0x04, "Power Attack Squat"	},
	{0x16, 0x01, 0x04, 0x00, "Squat Attack Power"	},
};

STDDIPINFO(Ironhors)

static void ironhors_main_write(UINT16 address, UINT8 data)
{
	if (address & 0xf000) {
		return;	// nop
	}

	switch (address)
	{
		case 0x0800:
			soundlatch = data;
		return;

		case 0x0900:
			ZetSetVector(0xff);
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;

		case 0x0a00:
			palettebank = data & 0x07;
		return;

		case 0x0b00:
		//	flipscreen = data & 0x08;
		return;
	}
}

static UINT8 ironhors_main_read(UINT16 address)
{
	switch (address)
	{
		case 0x0900:
			return DrvDips[2];

		case 0x0a00:
			return DrvDips[1];

		case 0x0b00:
			return DrvDips[0];

		case 0x0b01:
			return DrvInputs[2];

		case 0x0b02:
			return DrvInputs[1];

		case 0x0b03:
			return DrvInputs[0];
	}

	return 0;
}

static UINT8 __fastcall ironhors_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x8000:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			return soundlatch;
	}

	return 0;
}

static void __fastcall ironhors_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
			BurnYM2203Write(0, port & 1, data);
		return;
	}
}

static UINT8 __fastcall ironhors_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
			return BurnYM2203Read(0, port & 1);
	}

	return 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	M6809Open(0);
	M6809Reset();
	M6809Close();

	ZetOpen(0);
	ZetReset();
	BurnYM2203Reset();
	ZetClose();

	soundlatch = 0;
	palettebank = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6809ROM		= Next; Next += 0x00c000;

	DrvZ80ROM		= Next; Next += 0x004000;

	DrvGfxROM		= Next; Next += 0x040000;

	DrvColPROM		= Next; Next += 0x000500;

	DrvPalette		= (UINT32*)Next; Next += 0x1000 * sizeof(UINT32);

	AllRam			= Next;

	DrvM6809RAM0		= Next; Next += 0x000100;
	DrvColRAM		= Next; Next += 0x000400;
	DrvVidRAM		= Next; Next += 0x000400;
	DrvM6809RAM1		= Next; Next += 0x000800;
	DrvSprRAM0		= Next; Next += 0x000800;
	DrvSprRAM1		= Next; Next += 0x000800;

	DrvZ80RAM		= Next; Next += 0x000400;

	RamEnd			= Next;

	DrvCharBank		= DrvM6809RAM0 + 0x00003; 
	DrvIRQEnable		= DrvM6809RAM0 + 0x00004;
	DrvScrollRAM		= DrvM6809RAM0 + 0x00020;

	MemEnd			= Next;

	return 0;
}

static void DrvGfxExpand(UINT8 *src, INT32 len)
{
	for (INT32 i = (len - 1) * 2; i >= 0; i-=2) {
		src[i+0] = src[i/2] >> 4;
		src[i+1] = src[i/2] & 0xf;
	}
}

static void DrvPaletteInit()
{
	UINT32 pens[0x100];

	for (INT32 i = 0; i < 0x100; i++)
	{
		INT32 bit0 = (DrvColPROM[i + 0x000] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i + 0x000] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i + 0x000] >> 2) & 0x01;
		INT32 bit3 = (DrvColPROM[i + 0x000] >> 3) & 0x01;
		INT32 r = (bit3 * 138) + (bit2 * 69) + (bit1 * 33) + (bit0 * 15);

		bit0 = (DrvColPROM[i + 0x100] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + 0x100] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + 0x100] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + 0x100] >> 3) & 0x01;
		INT32 g = (bit3 * 138) + (bit2 * 69) + (bit1 * 33) + (bit0 * 15);

		bit0 = (DrvColPROM[i + 0x200] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + 0x200] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + 0x200] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + 0x200] >> 3) & 0x01;
		INT32 b = (bit3 * 138) + (bit2 * 69) + (bit1 * 33) + (bit0 * 15);

		pens[i] = BurnHighCol(r,g,b,0);
	}

	for (INT32 i = 0; i < 0x200; i++)
	{
		for (INT32 j = 0; j < 8; j++)
		{
			UINT8 ctabentry = (j << 5) | ((~i & 0x100) >> 4) | (DrvColPROM[i+0x300] & 0x0f);
			DrvPalette[((i & 0x100) << 3) | (j << 8) | (i & 0xff)] = pens[ctabentry];
		}
	}
}

static INT32 DrvInit()
{
	BurnSetRefreshRate(30.0);

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvM6809ROM + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x08000,  1, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM   + 0x00000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM   + 0x00000,  3, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM   + 0x00001,  4, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM   + 0x10000,  5, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM   + 0x10001,  6, 2)) return 1;

		if (BurnLoadRom(DrvColPROM  + 0x00000,  7, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00100,  8, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00200,  9, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00300, 10, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00400, 11, 1)) return 1;

		DrvGfxExpand(DrvGfxROM, 0x20000);
		DrvPaletteInit();
	}

	M6809Init(0);
	M6809Open(0);
	M6809MapMemory(DrvM6809RAM0,		0x0000, 0x00ff, MAP_RAM);
	M6809MapMemory(DrvColRAM,		0x2000, 0x23ff, MAP_RAM);
	M6809MapMemory(DrvVidRAM,		0x2400, 0x27ff, MAP_RAM);
	M6809MapMemory(DrvM6809RAM1,		0x2800, 0x2fff, MAP_RAM);
	M6809MapMemory(DrvSprRAM1,		0x3000, 0x37ff, MAP_RAM);
	M6809MapMemory(DrvSprRAM0,		0x3800, 0x3fff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM,		0x4000, 0xffff, MAP_RAM);
	M6809SetWriteHandler(ironhors_main_write);
	M6809SetReadHandler(ironhors_main_read);
	M6809Close();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,			0x4000, 0x43ff, MAP_RAM);
	ZetSetReadHandler(ironhors_sound_read);
	ZetSetOutHandler(ironhors_sound_write_port);
	ZetSetInHandler(ironhors_sound_read_port);
	ZetClose();

	BurnYM2203Init(1, 3072000, NULL, 0);
	BurnTimerAttachZet(3072000);
	BurnYM2203SetAllRoutes(0, 1.00, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetPSGVolume(0, 1.00);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	BurnYM2203Exit();

	M6809Exit();
	ZetExit();

	GenericTilesExit();

	BurnFree (AllMem);

	return 0;
}

static void draw_layer()
{
	INT32 bank = DrvCharBank[0] & 3;

	for (INT32 offs = 32*2; offs < (32 * 32) - (32 * 2); offs++)
	{
		INT32 sx = (offs & 0x1f) * 8;
		INT32 sy = (offs / 0x20) * 8;

		sx -= (DrvScrollRAM[(sy/8)] + 8) & 0xff;
		if (sx < -7) sx += 256;

		INT32 attr  = DrvColRAM[offs];
		INT32 code  = DrvVidRAM[offs] + ((attr & 0x40) << 2) + ((attr & 0x20) << 4) + (bank << 10);
		INT32 color = (attr & 0x0f) + (palettebank << 4);
		INT32 flipx = (attr & 0x10);
		INT32 flipy = (attr & 0x20);

		if (flipy) {
			if (flipx) {
				Render8x8Tile_FlipXY_Clip(pTransDraw, code, sx, sy - 16, color, 4, 0, DrvGfxROM);
			} else {
				Render8x8Tile_FlipY_Clip(pTransDraw, code, sx, sy - 16, color, 4, 0, DrvGfxROM);
			}
		} else {
			if (flipx) {
				Render8x8Tile_FlipX_Clip(pTransDraw, code, sx, sy - 16, color, 4, 0, DrvGfxROM);
			} else {
				Render8x8Tile_Clip(pTransDraw, code, sx, sy - 16, color, 4, 0, DrvGfxROM);
			}
		}
	}
}

static inline void draw_single_sprite(INT32 code, INT32 color, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy)
{
	sy -= 16;
	sx -= 8;

	if (sx < -7 || sy < -7 || sx >= nScreenWidth || sy >= nScreenHeight) return;

	if (flipy) {
		if (flipx) {
			Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0x800, DrvGfxROM);
		} else {
			Render8x8Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0x800, DrvGfxROM);
		}
	} else {
		if (flipx) {
			Render8x8Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0x800, DrvGfxROM);
		} else {
			Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0x800, DrvGfxROM);
		}
	}
}

static void draw_sprites()
{
	UINT8 *spriteram = ((DrvCharBank[0] & 8) ? DrvSprRAM0 : DrvSprRAM1);

	for (INT32 offs = 0; offs < 0x100; offs += 5)
	{
		INT32 sx    = spriteram[offs + 3];
		INT32 sy    = spriteram[offs + 2];
		INT32 code  = (spriteram[offs + 0] << 2) + ((spriteram[offs + 1] & 0x03) << 10) + ((spriteram[offs + 1] & 0x0c) >> 2);
		INT32 color = ((spriteram[offs + 1] & 0xf0) >> 4) + (palettebank << 4);
		INT32 flipx = spriteram[offs + 4] & 0x20;
		INT32 flipy = spriteram[offs + 4] & 0x40;

		switch (spriteram[offs + 4] & 0x0c)
		{
			case 0x00: // 16x16
				draw_single_sprite((code & ~3)|0, color, (flipx ? (sx + 8) : sx), (flipy ? (sy + 8) : sy), flipx, flipy);
				draw_single_sprite((code & ~3)|1, color, (flipx ? sx : (sx + 8)), (flipy ? (sy + 8) : sy), flipx, flipy);
				draw_single_sprite((code & ~3)|2, color, (flipx ? (sx + 8) : sx), (flipy ? sy : (sy + 8)), flipx, flipy);
				draw_single_sprite((code & ~3)|3, color, (flipx ? sx : (sx + 8)), (flipy ? sy : (sy + 8)), flipx, flipy);
			break;

			case 0x04: // 16x8
				draw_single_sprite(code & ~1, color, (flipx ? (sx + 8) : sx), sy, flipx, flipy);
				draw_single_sprite(code |  1, color, (flipx ? sx : (sx + 8)), sy, flipx, flipy);
			break;

			case 0x08: // 8x16
				draw_single_sprite(code & ~2, color, sx, (flipy ? (sy + 8) : sy), flipx, flipy);
				draw_single_sprite(code |  2, color, sx, (flipy ? sy : (sy + 8)), flipx, flipy);
			break;

			case 0x0c: // 8x8
				draw_single_sprite(code, color, sx, sy, flipx, flipy);
			break;
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	draw_layer();
	draw_sprites();

	BurnTransferCopy(DrvPalette);

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
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		// Clear opposites
		if ((DrvInputs[1] & 0x0c) == 0) DrvInputs[1] |= 0x0c;
		if ((DrvInputs[1] & 0x03) == 0) DrvInputs[1] |= 0x03;
		if ((DrvInputs[2] & 0x0c) == 0) DrvInputs[2] |= 0x0c;
		if ((DrvInputs[2] & 0x03) == 0) DrvInputs[2] |= 0x03;
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 3072000 / 30, 3072000 / 30 };

	M6809Open(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		M6809Run(nCyclesTotal[0] / nInterleave);

		if (i == 240 && (DrvIRQEnable[0] & 0x04)) M6809SetIRQLine(1, CPU_IRQSTATUS_AUTO); // firq
		if ((i&0x3f)==0 && (DrvIRQEnable[0] & 0x01)) M6809SetIRQLine(0x20, CPU_IRQSTATUS_AUTO); // nmi

		BurnTimerUpdate((i+1) * (nCyclesTotal[1] / nInterleave));
	}

	BurnTimerEndFrame(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	M6809Close();

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

		ZetOpen(0);
		BurnYM2203Scan(nAction, pnMin);
		ZetClose();

		SCAN_VAR(soundlatch);
		SCAN_VAR(palettebank);
	}

	return 0;
}


// Iron Horse

static struct BurnRomInfo ironhorsRomDesc[] = {
	{ "560_k03.13c",	0x8000, 0x395351b4, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "560_k02.12c",	0x4000, 0x1cff3d59, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "560_h01.10c",	0x4000, 0x2b17930f, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "560_h06.08f",	0x8000, 0xf21d8c93, 3 | BRF_GRA },           //  3 Graphics Tiles
	{ "560_h05.07f",	0x8000, 0x60107859, 3 | BRF_GRA },           //  4
	{ "560_h07.09f",	0x8000, 0xc761ec73, 3 | BRF_GRA },           //  5
	{ "560_h04.06f",	0x8000, 0xc1486f61, 3 | BRF_GRA },           //  6

	{ "03f_h08.bin",	0x0100, 0x9f6ddf83, 4 | BRF_GRA },           //  7 Color PROMs
	{ "04f_h09.bin",	0x0100, 0xe6773825, 4 | BRF_GRA },           //  8
	{ "05f_h10.bin",	0x0100, 0x30a57860, 4 | BRF_GRA },           //  9
	{ "10f_h12.bin",	0x0100, 0x5eb33e73, 4 | BRF_GRA },           // 10
	{ "10f_h11.bin",	0x0100, 0xa63e37d8, 4 | BRF_GRA },           // 11
};

STD_ROM_PICK(ironhors)
STD_ROM_FN(ironhors)

struct BurnDriver BurnDrvIronhors = {
	"ironhors", NULL, NULL, NULL, "1986",
	"Iron Horse\0", NULL, "Konami", "GX560",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_RUNGUN, 0,
	NULL, ironhorsRomInfo, ironhorsRomName, NULL, NULL, NULL, NULL, IronhorsInputInfo, IronhorsDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	240, 224, 4, 3
};


// Dai Ressya Goutou (Japan)

static struct BurnRomInfo dairesyaRomDesc[] = {
	{ "560-k03.13c",	0x8000, 0x2ac6103b, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "560-k02.12c",	0x4000, 0x07bc13a9, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "560-j01.10c",	0x4000, 0xa203b223, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "560-j06.8f",		0x8000, 0xa6e8248d, 3 | BRF_GRA },           //  3 Graphics Tiles
	{ "560-j05.7f",		0x8000, 0xf75893d4, 3 | BRF_GRA },           //  4
	{ "560-k07.9f",		0x8000, 0xc8a1b840, 3 | BRF_GRA },           //  5
	{ "560-k04.6f",		0x8000, 0xc883d856, 3 | BRF_GRA },           //  6

	{ "03f_h08.bin",	0x0100, 0x9f6ddf83, 4 | BRF_GRA },           //  7 Color PROMs
	{ "04f_h09.bin",	0x0100, 0xe6773825, 4 | BRF_GRA },           //  8
	{ "05f_h10.bin",	0x0100, 0x30a57860, 4 | BRF_GRA },           //  9
	{ "10f_h12.bin",	0x0100, 0x5eb33e73, 4 | BRF_GRA },           // 10
	{ "10f_h11.bin",	0x0100, 0xa63e37d8, 4 | BRF_GRA },           // 11
};

STD_ROM_PICK(dairesya)
STD_ROM_FN(dairesya)

struct BurnDriver BurnDrvDairesya = {
	"dairesya", "ironhors", NULL, NULL, "1986",
	"Dai Ressya Goutou (Japan)\0", NULL, "Konami (Kawakusu license)", "GX560",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_RUNGUN, 0,
	NULL, dairesyaRomInfo, dairesyaRomName, NULL, NULL, NULL, NULL, DairesyaInputInfo, IronhorsDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	240, 224, 4, 3
};


// Far West

static struct BurnRomInfo farwestRomDesc[] = {
	{ "ironhors.008",	0x4000, 0xb1c8246c, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "ironhors.009",	0x8000, 0xea34ecfc, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ironhors.007",	0x2000, 0x471182b7, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "ironhors.010",	0x4000, 0xa28231a6, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 Code

	{ "ironhors.005",	0x8000, 0xf77e5b83, 3 | BRF_GRA },           //  4 Background Tiles
	{ "ironhors.006",	0x8000, 0x7bbc0b51, 3 | BRF_GRA },           //  5

	{ "ironhors.001",	0x4000, 0xa8fc21d3, 4 | BRF_GRA },           //  6 Sprites
	{ "ironhors.002",	0x4000, 0x9c1e5593, 4 | BRF_GRA },           //  7
	{ "ironhors.003",	0x4000, 0x3a0bf799, 4 | BRF_GRA },           //  8
	{ "ironhors.004",	0x4000, 0x1fab18a3, 4 | BRF_GRA },           //  9

	{ "ironcol.003",	0x0100, 0x3e3fca11, 5 | BRF_GRA },           // 10 Color PROMs
	{ "ironcol.001",	0x0100, 0xdfb13014, 5 | BRF_GRA },           // 11
	{ "ironcol.002",	0x0100, 0x77c88430, 5 | BRF_GRA },           // 12
	{ "10f_h12.bin",	0x0100, 0x5eb33e73, 5 | BRF_GRA },           // 13
	{ "ironcol.005",	0x0100, 0x15077b9c, 5 | BRF_GRA },           // 14
};

STD_ROM_PICK(farwest)
STD_ROM_FN(farwest)

struct BurnDriverD BurnDrvFarwest = {
	"farwest", "ironhors", NULL, NULL, "1986",
	"Far West\0", NULL, "bootleg?", "GX560",
	NULL, NULL, NULL, NULL,
	BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_RUNGUN, 0,
	NULL, farwestRomInfo, farwestRomName, NULL, NULL, NULL, NULL, IronhorsInputInfo, IronhorsDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	240, 224, 4, 3
};
