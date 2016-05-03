// FB Alpha Naughty Boy driver module
// Based on MAME driver by Brad Oliver, Sal and John Bugliarisi, Paul Priest

#include "tiles_generic.h"
#include "z80_intf.h"
#include "pleiadssound.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvQuestion;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvVidRAM0;
static UINT8 *DrvVidRAM1;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 cocktail;
static UINT8 palettereg;
static UINT8 bankreg;
static UINT8 scrollreg;

static UINT8 prot_count;
static UINT8 prot_seed;
static INT32 prot_index;
static UINT32 question_offset;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[1];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;
static INT32 prevcoin;

static INT32 game_select = 0;
static INT32 vblank;

static struct BurnInputInfo NaughtybInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 right"	},
	{"P1 Button",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 1"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 right"	},
	{"P2 Button",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Naughtyb)

static struct BurnInputInfo TrvmstrInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 4"	},

	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 4"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Trvmstr)

static struct BurnDIPInfo NaughtybDIPList[]=
{
	{0x0e, 0xff, 0xff, 0x15, NULL			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0e, 0x01, 0x03, 0x00, "2"			},
	{0x0e, 0x01, 0x03, 0x01, "3"			},
	{0x0e, 0x01, 0x03, 0x02, "4"			},
	{0x0e, 0x01, 0x03, 0x03, "5"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x0e, 0x01, 0x0c, 0x00, "10000"		},
	{0x0e, 0x01, 0x0c, 0x04, "30000"		},
	{0x0e, 0x01, 0x0c, 0x08, "50000"		},
	{0x0e, 0x01, 0x0c, 0x0c, "70000"		},

	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{0x0e, 0x01, 0x30, 0x00, "2 Coins 1 Credits"	},
	{0x0e, 0x01, 0x30, 0x10, "1 Coin  1 Credits"	},
	{0x0e, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},
	{0x0e, 0x01, 0x30, 0x30, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x0e, 0x01, 0x40, 0x00, "Easy"			},
	{0x0e, 0x01, 0x40, 0x40, "Hard"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0e, 0x01, 0x80, 0x00, "Upright"		},
	{0x0e, 0x01, 0x80, 0x80, "Cocktail"		},
};

STDDIPINFO(Naughtyb)

static struct BurnDIPInfo TrvmstrDIPList[]=
{
	{0x0a, 0xff, 0xff, 0x44, NULL			},

	{0   , 0xfe, 0   ,    4, "Screen Orientation"	},
	{0x0a, 0x01, 0x03, 0x00, "0'"			},
	{0x0a, 0x01, 0x03, 0x02, "90'"			},
	{0x0a, 0x01, 0x03, 0x01, "180'"			},
	{0x0a, 0x01, 0x03, 0x03, "270'"			},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x0a, 0x01, 0x04, 0x04, "Off"			},
	{0x0a, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Show Correct Answer"	},
	{0x0a, 0x01, 0x08, 0x08, "No"			},
	{0x0a, 0x01, 0x08, 0x00, "Yes"			},

	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{0x0a, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x0a, 0x01, 0x30, 0x00, "1 Coin  1 Credits"	},
	{0x0a, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},
	{0x0a, 0x01, 0x30, 0x30, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Number of Questions"	},
	{0x0a, 0x01, 0x40, 0x00, "5"			},
	{0x0a, 0x01, 0x40, 0x40, "7"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0a, 0x01, 0x80, 0x00, "Upright"		},
	{0x0a, 0x01, 0x80, 0x80, "Cocktail"		},
};

STDDIPINFO(Trvmstr)

static UINT8 popflame_protection_read()
{
	static const UINT8 seed[8] = { 0x78, 0x68, 0x48, 0xb8, 0x68, 0x60, 0x68, 0xe0 };

	prot_count = (prot_count + 1) & 3;

	return seed[prot_seed + prot_count] | ((prot_index < 0x89) ? 1 : 0);
}

static void popflame_protection_write(UINT8 data)
{
	if ((data & 1) && ((prot_seed & 1) == 0))
		prot_index = 0;

	if ((data & 8) && ((prot_seed & 8) == 0))
		prot_index++;

	prot_seed = (data & 0x10) >> 2;
}

static void __fastcall naughtyb_main_write(UINT16 address, UINT8 data)
{
	if (game_select == 1) { // PopFlamer protection..
		if (address >= 0xb000 && address <= 0xb0ff)	{
			return popflame_protection_write(data);
		}
	}

	switch (address & ~0x07ff)
	{
		case 0x9000:
		{
			pleiads_sound_control_c_w(address, data); // sound - control c

			cocktail = (DrvDips[0] >> 7) & (data & 1);
			palettereg = (data & 0x06) >> 1;
			bankreg = (data >> ((game_select == 1) ? 3 : 2)) & 1;
		}
		return;

		case 0x9800:
			scrollreg = data;
		return;

		case 0xa000:
			pleiads_sound_control_a_w(address, data); // sound - control a
		return;

		case 0xa800:
			pleiads_sound_control_b_w(address, data); // sound - control b  and popflame protection writes
		return;

		case 0xc000:
			question_offset = (question_offset & ~(0xff << ((address & 3) << 3))) | (data << ((address & 3) << 3));
		return;
	}
}

static UINT8 __fastcall naughtyb_main_read(UINT16 address)
{
	if (game_select == 1) { // PopFlamer protection..
		if (address == 0x9000 || address == 0x9090)
			return popflame_protection_read();
	}

	switch (address & ~0x07ff)
	{
		case 0xb000:
			return (DrvInputs[0] & 0x03) | (DrvInputs[cocktail] & 0xfc);

		case 0xb800:
			return (DrvDips[0] & 0x7f) | (vblank ? 0x80 : 0);

		case 0xc000:
			return DrvQuestion[question_offset & 0x1ffff];
	}

	return 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	if (game_select == 0 || game_select == 2) {
		naughtyb_sound_reset();
	}

	if (game_select == 1) {
		popflame_sound_reset();
	}

	DrvInputs[2] = 0xff; // default to no coin
	prevcoin = 1;

	scrollreg = 0;
	cocktail = 0;
	bankreg = 0;

	prot_count = 0;
	prot_seed = 0;
	prot_index = 0;

	question_offset = 0;

	return 0;
}

static INT32 MemIndex(INT32 select)
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x004000;

	DrvQuestion		= Next;
	if (select == 2 || select == 3)
		                    Next += 0x020000;

	DrvGfxROM0		= Next; Next += 0x008000;
	DrvGfxROM1		= Next; Next += 0x008000;

	DrvColPROM		= Next; Next += 0x000200;

	DrvPalette		= (UINT32*)Next; Next += 0x0100 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x004000;
	DrvVidRAM0		= Next; Next += 0x000800;
	DrvVidRAM1		= Next; Next += 0x000800;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[2] = { 0x8000, 0 };
	INT32 XOffs[8] = { STEP8(7,-1) };
	INT32 YOffs[8] = { STEP8(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x4000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x2000);

	GfxDecode(0x0200, 2, 8, 8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x2000);

	GfxDecode(0x0200, 2, 8, 8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM1);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit(INT32 select)
{
	AllMem = NULL;
	MemIndex(select);
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex(select);

	game_select = select;

	if (game_select == 0)
	{ // naughty boy
		if (BurnLoadRom(DrvZ80ROM  + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x0800,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x1000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x1800,  3, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x2000,  4, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x2800,  5, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x3000,  6, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x3800,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x0000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0800,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x1000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x1800, 11, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x0000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x0800, 13, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x1000, 14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x1800, 15, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x0000, 16, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0100, 17, 1)) return 1;
	}
	else if (game_select == 1)
	{ // pop flamer
		if (BurnLoadRom(DrvZ80ROM  + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x1000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x2000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x3000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x0000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x1000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x0000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x1000,  7, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x0000,  8, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0100,  9, 1)) return 1;
	}
	else if (game_select == 2)
	{ // trivia master
		if (BurnLoadRom(DrvZ80ROM  + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x1000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x2000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x0000,  3, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x1000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x0000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x1000,  6, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x0000,  7, 1)) return 1;

		if (BurnLoadRom(DrvQuestion + 0x00000,  8, 1)) return 1;
		if (BurnLoadRom(DrvQuestion + 0x04000,  9, 1)) return 1;
		if (BurnLoadRom(DrvQuestion + 0x08000, 10, 1)) return 1;
		if (BurnLoadRom(DrvQuestion + 0x0c000, 11, 1)) return 1;
		if (BurnLoadRom(DrvQuestion + 0x10000, 12, 1)) return 1;
		if (BurnLoadRom(DrvQuestion + 0x14000, 13, 1)) return 1;
		if (BurnLoadRom(DrvQuestion + 0x18000, 14, 1)) return 1;
		if (BurnLoadRom(DrvQuestion + 0x1c000, 15, 1)) return 1;
	}
	else if (game_select == 3)
	{ // trivia genius
		if (BurnLoadRom(DrvZ80ROM  + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x1000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x2000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x0000,  3, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x1000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x0000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x1000,  6, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x0000,  7, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0100,  8, 1)) return 1;

		if (BurnLoadRom(DrvQuestion + 0x00000,  9, 1)) return 1;
		if (BurnLoadRom(DrvQuestion + 0x04000, 10, 1)) return 1;
		if (BurnLoadRom(DrvQuestion + 0x08000, 11, 1)) return 1;
		if (BurnLoadRom(DrvQuestion + 0x0c000, 12, 1)) return 1;
		if (BurnLoadRom(DrvQuestion + 0x10000, 13, 1)) return 1;
		if (BurnLoadRom(DrvQuestion + 0x14000, 14, 1)) return 1;
		if (BurnLoadRom(DrvQuestion + 0x18000, 15, 1)) return 1;
		if (BurnLoadRom(DrvQuestion + 0x1c000, 16, 1)) return 1;
		game_select = 2;
	}

	DrvGfxDecode();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,		0x4000, 0x7fff, MAP_RAM);
	ZetMapMemory(DrvVidRAM0,	0x8000, 0x87ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM1,	0x8800, 0x8fff, MAP_RAM);
	ZetSetWriteHandler(naughtyb_main_write);
	ZetSetReadHandler(naughtyb_main_read);
	ZetClose();

	pleiads_sound_init(1);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	pleiads_sound_deinit();

	ZetExit();

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0;i < 0x100; i++)
	{
		INT32 bit0, bit1, r, g, b;

		bit0 = (DrvColPROM[i] >> 0) & 0x01;
		bit1 = (DrvColPROM[i+0x100] >> 0) & 0x01;
		r = bit0 * 172 + bit1 * 83;

		bit0 = (DrvColPROM[i] >> 2) & 0x01;
		bit1 = (DrvColPROM[i+0x100] >> 2) & 0x01;
		g = bit0 * 172 + bit1 * 83;

		bit0 = (DrvColPROM[i] >> 1) & 0x01;
		bit1 = (DrvColPROM[i+0x100] >> 1) & 0x01;
		b = bit0 * 172 + bit1 * 83;

		INT32 o = ((i & 0xc0) >> 1) | ((i & 0x27) << 2) | ((i & 0x18) >> 3);

		DrvPalette[o] = BurnHighCol(r,g,b,0);
	}
}

static void draw_last_little_bit(INT32 layer)
{
	for (INT32 offs = 0x100 - 1; offs >= 0; offs--)
	{
		INT32 code0  = (DrvVidRAM0[offs + 0x700] + (bankreg << 8)) & 0x1ff;
		INT32 color0 = ((DrvVidRAM0[offs + 0x700] >> 5) + (palettereg << 3)) & 0xff;

		INT32 code1  = (DrvVidRAM1[offs + 0x700] + (bankreg << 8)) & 0x1ff;
		INT32 color1 = ((DrvVidRAM1[offs + 0x700] >> 5) + (palettereg << 3)) & 0xff;

		INT32 sx = (offs & 3) * 8;
		INT32 sy = (offs / 4) * 8;

		if ((offs&3) >= 2) {
			sx -= 16;
		} else {
			sx += (34 * 8);
		}

		//if (nBurnLayer & 4) Render8x8Tile_Clip(pTransDraw, code0, sx, sy, color0, 2, 0x80, DrvGfxROM1);
		//if (nBurnLayer & 8) Render8x8Tile_Mask_Clip(pTransDraw, code0, sx, sy, color0, 2, transp, 0x80, DrvGfxROM1);
		if (nBurnLayer & 1 && layer == 0) Render8x8Tile_Clip(pTransDraw, code1, sx, sy, color1, 2, 0, DrvGfxROM0);
		if (nBurnLayer & 2 && layer == 1) Render8x8Tile_Mask_Clip(pTransDraw, code0, sx, sy, color0, 2, 0, 0x80, DrvGfxROM1);
	}
}

static void draw_layer(INT32 layer)
{
	INT32 scrx = (scrollreg - 17) & 0x1ff;

// 121 - 72

	for (INT32 offs = 0x800 - 1; offs >= 0; offs--)
	{
		INT32 sx, sy;

		INT32 code1  = (DrvVidRAM1[offs] + (bankreg << 8)) & 0x1ff;
		INT32 color1 = ((DrvVidRAM1[offs] >> 5) + (palettereg << 3)) & 0xff;

		INT32 code0  = (DrvVidRAM0[offs] + (bankreg << 8)) & 0x1ff;
		INT32 color0 = ((DrvVidRAM0[offs] >> 5) + (palettereg << 3)) & 0xff;

		if (cocktail)
		{
			if (offs < 0x700)
			{
				sx = (63 - offs) & 0x3f;
				sy = 27 - offs / 64;
			}
			else
			{
				sx = 64 + ((3 - (offs - 0x700)) & 3 );
				sy = 27 - (offs - 0x700) / 4;
			}

			sx *= 8;
			sy *= 8;


			if (nBurnLayer & 1 && layer == 0) Render8x8Tile_FlipXY_Clip(pTransDraw, code1, sx, sy, color1, 2, 0, DrvGfxROM0);
			if (nBurnLayer & 2 && layer == 1) Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, code0, sx, sy, color0, 2, 0, 0x80, DrvGfxROM1);
		}
		else
		{
			INT32 transp = 0;

			if (offs < 0x700)
			{
				sx = (offs & 0x3f) * 8;
				sy = (offs / 64) * 8;

				sx -= scrx;
				if (sx < -7) sx += 512;

			} else continue;
		//	else
		//	{
		//		sx = (64 + (offs - 0x700) & 3) * 8;
		//		sy = ((offs - 0x700) / 4) * 8;
		//		transp = 0xff;
		//	}

			if (nBurnLayer & 1 && layer == 0) Render8x8Tile_Clip(pTransDraw, code1, sx, sy, color1, 2, 0, DrvGfxROM0);
			if (nBurnLayer & 2 && layer == 1) Render8x8Tile_Mask_Clip(pTransDraw, code0, sx, sy, color0, 2, transp, 0x80, DrvGfxROM1);
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

	draw_layer(0);
	draw_layer(1);
	draw_last_little_bit(0);
	draw_last_little_bit(1);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}


	ZetOpen(0);

	{
		memset (DrvInputs, 0, 3);
		DrvInputs[2] = 0xff;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
		ProcessJoystick(&DrvInputs[0], 0, 4,5,7,6, INPUT_4WAY | INPUT_MAKEACTIVELOW);
		ProcessJoystick(&DrvInputs[1], 1, 4,5,7,6, INPUT_4WAY | INPUT_MAKEACTIVELOW);

		if ((DrvInputs[2] & 1) && (prevcoin != (DrvInputs[2] & 1))) {
			ZetNmi();
		}
		prevcoin = DrvInputs[2] & 1;
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal = 3000000 / 60;
	INT32 nCyclesDone  = 0;

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone += ZetRun(nCyclesTotal / nInterleave);

		if (i == 240) vblank = 1;
	}

	ZetClose();

	if (pBurnSoundOut) {
		pleiads_sound_update(pBurnSoundOut, nBurnSoundLen);
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

		SCAN_VAR(cocktail);
		SCAN_VAR(palettereg);
		SCAN_VAR(bankreg);
		SCAN_VAR(scrollreg);
		SCAN_VAR(prot_count);
		SCAN_VAR(prot_seed);
		SCAN_VAR(prot_index);
		SCAN_VAR(question_offset);
	}

	return 0;
}


// Naughty Boy

static struct BurnRomInfo naughtybRomDesc[] = {
	{ "1.30",		0x0800, 0xf6e1178e, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "2.29",		0x0800, 0xb803eb8c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.28",		0x0800, 0x004d0ba7, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.27",		0x0800, 0x3c7bcac6, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "5.26",		0x0800, 0xea80f39b, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "6.25",		0x0800, 0x66d9f942, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "7.24",		0x0800, 0x00caf9be, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "8.23",		0x0800, 0x17c3b6fb, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "15.44",		0x0800, 0xd692f9c7, 2 | BRF_GRA },           //  8 Background Tiles
	{ "16.43",		0x0800, 0xd3ba8b27, 2 | BRF_GRA },           //  9
	{ "13.46",		0x0800, 0xc1669cd5, 2 | BRF_GRA },           // 10
	{ "14.45",		0x0800, 0xeef2c8e5, 2 | BRF_GRA },           // 11

	{ "11.48",		0x0800, 0x75ec9710, 3 | BRF_GRA },           // 12 Foreground Tiles
	{ "12.47",		0x0800, 0xef0706c3, 3 | BRF_GRA },           // 13
	{ "9.50",		0x0800, 0x8c8db764, 3 | BRF_GRA },           // 14
	{ "10.49",		0x0800, 0xc97c97b9, 3 | BRF_GRA },           // 15

	{ "6301-1.63",		0x0100, 0x98ad89a1, 4 | BRF_GRA },           // 16 Color Proms
	{ "6301-1.64",		0x0100, 0x909107d4, 4 | BRF_GRA },           // 17
};

STD_ROM_PICK(naughtyb)
STD_ROM_FN(naughtyb)

static INT32 naughtybInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvNaughtyb = {
	"naughtyb", NULL, NULL, NULL, "1982",
	"Naughty Boy\0", NULL, "Jaleco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, naughtybRomInfo, naughtybRomName, NULL, NULL, NaughtybInputInfo, NaughtybDIPInfo,
	naughtybInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 288, 3, 4
};


// Naughty Boy (bootleg)

static struct BurnRomInfo naughtybaRomDesc[] = {
	{ "91",			0x0800, 0x42b14bc7, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "92",			0x0800, 0xa24674b4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.28",		0x0800, 0x004d0ba7, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.27",		0x0800, 0x3c7bcac6, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "95",			0x0800, 0xe282f1b8, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "96",			0x0800, 0x61178ff2, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "97",			0x0800, 0x3cafde88, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "8.23",		0x0800, 0x17c3b6fb, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "15.44",		0x0800, 0xd692f9c7, 2 | BRF_GRA },           //  8 Background Tiles
	{ "16.43",		0x0800, 0xd3ba8b27, 2 | BRF_GRA },           //  9
	{ "13.46",		0x0800, 0xc1669cd5, 2 | BRF_GRA },           // 10
	{ "14.45",		0x0800, 0xeef2c8e5, 2 | BRF_GRA },           // 11

	{ "11.48",		0x0800, 0x75ec9710, 3 | BRF_GRA },           // 12 Foreground Tiles
	{ "12.47",		0x0800, 0xef0706c3, 3 | BRF_GRA },           // 13
	{ "9.50",		0x0800, 0x8c8db764, 3 | BRF_GRA },           // 14
	{ "10.49",		0x0800, 0xc97c97b9, 3 | BRF_GRA },           // 15

	{ "6301-1.63",		0x0100, 0x98ad89a1, 4 | BRF_GRA },           // 16 Color Proms
	{ "6301-1.64",		0x0100, 0x909107d4, 4 | BRF_GRA },           // 17
};

STD_ROM_PICK(naughtyba)
STD_ROM_FN(naughtyba)

struct BurnDriver BurnDrvNaughtyba = {
	"naughtyba", "naughtyb", NULL, NULL, "1982",
	"Naughty Boy (bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, naughtybaRomInfo, naughtybaRomName, NULL, NULL, NaughtybInputInfo, NaughtybDIPInfo,
	naughtybInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 288, 3, 4
};


// Naughty Boy (Cinematronics)

static struct BurnRomInfo naughtybcRomDesc[] = {
	{ "nb1ic30",		0x0800, 0x3f482fa3, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "nb2ic29",		0x0800, 0x7ddea141, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "nb3ic28",		0x0800, 0x8c72a069, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "nb4ic27",		0x0800, 0x30feae51, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "nb5ic26",		0x0800, 0x05242fd0, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "nb6ic25",		0x0800, 0x7a12ffea, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "nb7ic24",		0x0800, 0x9cc287df, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "nb8ic23",		0x0800, 0x4d84ff2c, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "15.44",		0x0800, 0xd692f9c7, 2 | BRF_GRA },           //  8 Background Tiles
	{ "16.43",		0x0800, 0xd3ba8b27, 2 | BRF_GRA },           //  9
	{ "13.46",		0x0800, 0xc1669cd5, 2 | BRF_GRA },           // 10
	{ "14.45",		0x0800, 0xeef2c8e5, 2 | BRF_GRA },           // 11

	{ "nb11ic48",		0x0800, 0x23271a13, 3 | BRF_GRA },           // 12 Foreground Tiles
	{ "12.47",		0x0800, 0xef0706c3, 3 | BRF_GRA },           // 13
	{ "nb9ic50",		0x0800, 0xd6949c27, 3 | BRF_GRA },           // 14
	{ "10.49",		0x0800, 0xc97c97b9, 3 | BRF_GRA },           // 15

	{ "6301-1.63",		0x0100, 0x98ad89a1, 4 | BRF_GRA },           // 16 Color Proms
	{ "6301-1.64",		0x0100, 0x909107d4, 4 | BRF_GRA },           // 17
};

STD_ROM_PICK(naughtybc)
STD_ROM_FN(naughtybc)

struct BurnDriver BurnDrvNaughtybc = {
	"naughtybc", "naughtyb", NULL, NULL, "1982",
	"Naughty Boy (Cinematronics)\0", NULL, "Jaleco (Cinematronics license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, naughtybcRomInfo, naughtybcRomName, NULL, NULL, NaughtybInputInfo, NaughtybDIPInfo,
	naughtybInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 288, 3, 4
};


// Pop Flamer (protected)

static struct BurnRomInfo popflameRomDesc[] = {
	{ "ic86.bin",		0x1000, 0x06397a4b, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "ic80.pop",		0x1000, 0xb77abf3d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ic94.bin",		0x1000, 0xae5248ae, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ic100.pop",		0x1000, 0xf9f2343b, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "ic13.pop",		0x1000, 0x2367131e, 2 | BRF_GRA },           //  4 Background Tiles
	{ "ic3.pop",		0x1000, 0xdeed0a8b, 2 | BRF_GRA },           //  5

	{ "ic29.pop",		0x1000, 0x7b54f60f, 3 | BRF_GRA },           //  6 Foreground Tiles
	{ "ic38.pop",		0x1000, 0xdd2d9601, 3 | BRF_GRA },           //  7

	{ "ic54",		0x0100, 0x236bc771, 4 | BRF_GRA },           //  8 Color Proms
	{ "ic53",		0x0100, 0x6e66057f, 4 | BRF_GRA },           //  9
};

STD_ROM_PICK(popflame)
STD_ROM_FN(popflame)

static INT32 popflameInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvPopflame = {
	"popflame", NULL, NULL, NULL, "1982",
	"Pop Flamer (protected)\0", NULL, "Jaleco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, popflameRomInfo, popflameRomName, NULL, NULL, NaughtybInputInfo, NaughtybDIPInfo,
	popflameInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 288, 3, 4
};


// Pop Flamer (not protected)

static struct BurnRomInfo popflameaRomDesc[] = {
	{ "ic86.pop",		0x1000, 0x5e32bbdf, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "ic80.pop",		0x1000, 0xb77abf3d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ic94.pop",		0x1000, 0x945a3c0f, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ic100.pop",		0x1000, 0xf9f2343b, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "ic13.pop",		0x1000, 0x2367131e, 2 | BRF_GRA },           //  4 Background Tiles
	{ "ic3.pop",		0x1000, 0xdeed0a8b, 2 | BRF_GRA },           //  5

	{ "ic29.pop",		0x1000, 0x7b54f60f, 3 | BRF_GRA },           //  6 Foreground Tiles
	{ "ic38.pop",		0x1000, 0xdd2d9601, 3 | BRF_GRA },           //  7

	{ "ic54",		0x0100, 0x236bc771, 4 | BRF_GRA },           //  8 Color Proms
	{ "ic53",		0x0100, 0x6e66057f, 4 | BRF_GRA },           //  9
};

STD_ROM_PICK(popflamea)
STD_ROM_FN(popflamea)

struct BurnDriver BurnDrvPopflamea = {
	"popflamea", "popflame", NULL, NULL, "1982",
	"Pop Flamer (not protected)\0", NULL, "Jaleco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, popflameaRomInfo, popflameaRomName, NULL, NULL, NaughtybInputInfo, NaughtybDIPInfo,
	popflameInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 288, 3, 4
};


// Pop Flamer (hack?)

static struct BurnRomInfo popflamebRomDesc[] = {
	{ "popflama.30",	0x1000, 0xa9bb0e8a, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "popflama.28",	0x1000, 0xdebe6d03, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "popflama.26",	0x1000, 0x09df0d4d, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "popflama.24",	0x1000, 0xf399d553, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "ic13.pop",		0x1000, 0x2367131e, 2 | BRF_GRA },           //  4 Background Tiles
	{ "ic3.pop",		0x1000, 0xdeed0a8b, 2 | BRF_GRA },           //  5

	{ "ic29.pop",		0x1000, 0x7b54f60f, 3 | BRF_GRA },           //  6 Foreground Tiles
	{ "ic38.pop",		0x1000, 0xdd2d9601, 3 | BRF_GRA },           //  7

	{ "ic54",		0x0100, 0x236bc771, 4 | BRF_GRA },           //  8 Color Proms
	{ "ic53",		0x0100, 0x6e66057f, 4 | BRF_GRA },           //  9
};

STD_ROM_PICK(popflameb)
STD_ROM_FN(popflameb)

struct BurnDriver BurnDrvPopflameb = {
	"popflameb", "popflame", NULL, NULL, "1982",
	"Pop Flamer (hack?)\0", NULL, "Jaleco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, popflamebRomInfo, popflamebRomName, NULL, NULL, NaughtybInputInfo, NaughtybDIPInfo,
	popflameInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 288, 3, 4
};




// Pop Flamer (bootleg on Naughty Boy PCB)

static struct BurnRomInfo popflamenRomDesc[] = {
	{ "pfb2-1.bin",		0x0800, 0x88cd3faa, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "pfb2-2.bin",		0x0800, 0xa09892e8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "pfb2-3.bin",		0x0800, 0x99fca5ed, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "pfb2-4.bin",		0x0800, 0xc8d254e0, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "pfb2-5.bin",		0x0800, 0xd89710d5, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "pfb2-6.bin",		0x0800, 0xb6cec1aa, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "pfb2-7.bin",		0x0800, 0x1cf8b5c4, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "pfb2-8.bin",		0x0800, 0xa63feeff, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "pfb2-15.bin",	0x0800, 0x3d8b8f6f, 2 | BRF_GRA },           //  8 Background Tiles
	{ "pfb2-16.bin",	0x0800, 0x75f0308b, 2 | BRF_GRA },           //  9
	{ "pfb2-13.bin",	0x0800, 0x42fc5bac, 2 | BRF_GRA },           // 10
	{ "pfb2-14.bin",	0x0800, 0xfefada6e, 2 | BRF_GRA },           // 11

	{ "pfb2-11.bin",	0x0800, 0x8ccdcc01, 3 | BRF_GRA },           // 12 Foreground Tiles
	{ "pfb2-12.bin",	0x0800, 0x49e04ddb, 3 | BRF_GRA },           // 13
	{ "pfb2-9.bin",		0x0800, 0x32debf48, 3 | BRF_GRA },           // 14
	{ "pfb2-10.bin",	0x0800, 0x7fe61ed3, 3 | BRF_GRA },           // 15

	{ "ic54",		0x0100, 0x236bc771, 4 | BRF_GRA },           // 16 Color Proms
	{ "ic53",		0x0100, 0x6e66057f, 4 | BRF_GRA },           // 17
};

STD_ROM_PICK(popflamen)
STD_ROM_FN(popflamen)

struct BurnDriver BurnDrvPopflamen = {
	"popflamen", "popflame", NULL, NULL, "1982",
	"Pop Flamer (bootleg on Naughty Boy PCB)\0", NULL, "Jaleco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, popflamenRomInfo, popflamenRomName, NULL, NULL, NaughtybInputInfo, NaughtybDIPInfo,
	naughtybInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 288, 3, 4
};


// Trivia Master (set 1)

static struct BurnRomInfo trvmstrRomDesc[] = {
	{ "ic30.bin",		0x1000, 0x4ccd0537, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "ic28.bin",		0x1000, 0x782a2b8c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ic26.bin",		0x1000, 0x1362010a, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "ic44.bin",		0x1000, 0xdac8cff7, 2 | BRF_GRA },           //  3 Background Tiles
	{ "ic46.bin",		0x1000, 0xa97ab879, 2 | BRF_GRA },           //  4

	{ "ic48.bin",		0x1000, 0x79952015, 3 | BRF_GRA },           //  5 Foreground Tiles
	{ "ic50.bin",		0x1000, 0xf09da428, 3 | BRF_GRA },           //  6

	{ "ic64.bin",		0x0100, 0xe9915da8, 4 | BRF_GRA },           //  7 Color Proms

	{ "sport_lo.u2",	0x4000, 0x24f30489, 5 | BRF_GRA },           //  8 Question Data
	{ "sport_hi.u1",	0x4000, 0xd64a7480, 5 | BRF_GRA },           //  9
	{ "etain_lo.u4",	0x4000, 0xa2af9709, 5 | BRF_GRA },           // 10
	{ "etain_hi.u3",	0x4000, 0x82a60dea, 5 | BRF_GRA },           // 11
	{ "sex_lo.u6",		0x4000, 0xf2ecfa88, 5 | BRF_GRA },           // 12
	{ "sex_hi.u5",		0x4000, 0xde4a6c4b, 5 | BRF_GRA },           // 13
	{ "scien_lo.u8",	0x4000, 0x01a01ff1, 5 | BRF_GRA },           // 14
	{ "scien_hi.u7",	0x4000, 0x0bc68078, 5 | BRF_GRA },           // 15
};

STD_ROM_PICK(trvmstr)
STD_ROM_FN(trvmstr)

static INT32 trvmstrInit()
{
	return DrvInit(2);
}

struct BurnDriver BurnDrvTrvmstr = {
	"trvmstr", NULL, NULL, NULL, "1985",
	"Trivia Master (set 1)\0", NULL, "Enerdyne Technologies Inc.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_QUIZ, 0,
	NULL, trvmstrRomInfo, trvmstrRomName, NULL, NULL, TrvmstrInputInfo, TrvmstrDIPInfo,
	trvmstrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 288, 3, 4
};


// Trivia Master (set 2)

static struct BurnRomInfo trvmstraRomDesc[] = {
	{ "ic30a.bin",		0x2000, 0x4c175c45, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "ic28a.bin",		0x2000, 0x3a8ca87d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ic26a.bin",		0x2000, 0x3c655400, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "ic44.bin",		0x1000, 0xdac8cff7, 2 | BRF_GRA },           //  3 Background Tiles
	{ "ic46.bin",		0x1000, 0xa97ab879, 2 | BRF_GRA },           //  4

	{ "ic48.bin",		0x1000, 0x79952015, 3 | BRF_GRA },           //  5 Foreground Tiles
	{ "ic50.bin",		0x1000, 0xf09da428, 3 | BRF_GRA },           //  6

	{ "ic64.bin",		0x0100, 0xe9915da8, 4 | BRF_GRA },           //  7 Color Proms

	{ "enter_lo.u2",	0x4000, 0xa65b8f83, 5 | BRF_GRA },           //  8 Question Data
	{ "enter_hi.u1",	0x4000, 0xcaede447, 5 | BRF_GRA },           //  9
	{ "sports_lo.u4",	0x4000, 0xd5317b26, 5 | BRF_GRA },           // 10
	{ "sports_hi.u3",	0x4000, 0x9f706db2, 5 | BRF_GRA },           // 11
	{ "sex2_lo.u6",		0x4000, 0xb73f2e31, 5 | BRF_GRA },           // 12
	{ "sex2_hi.u5",		0x4000, 0xbf654110, 5 | BRF_GRA },           // 13
	{ "comic_lo.u8",	0x4000, 0x109bd359, 5 | BRF_GRA },           // 14
	{ "comic_hi.u7",	0x4000, 0x8e8b5f71, 5 | BRF_GRA },           // 15
};

STD_ROM_PICK(trvmstra)
STD_ROM_FN(trvmstra)

struct BurnDriver BurnDrvTrvmstra = {
	"trvmstra", "trvmstr", NULL, NULL, "1985",
	"Trivia Master (set 2)\0", NULL, "Enerdyne Technologies Inc.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_QUIZ, 0,
	NULL, trvmstraRomInfo, trvmstraRomName, NULL, NULL, TrvmstrInputInfo, TrvmstrDIPInfo,
	trvmstrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 288, 3, 4
};


// Trivia Master (set 3)

static struct BurnRomInfo trvmstrbRomDesc[] = {
	{ "ic30b.bin",		0x1000, 0xd3eb4197, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "ic28b.bin",		0x1000, 0x70322d65, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ic26b.bin",		0x1000, 0x31dfa9cf, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "ic44.bin",		0x1000, 0xdac8cff7, 2 | BRF_GRA },           //  3 Background Tiles
	{ "ic46.bin",		0x1000, 0xa97ab879, 2 | BRF_GRA },           //  4

	{ "ic48.bin",		0x1000, 0x79952015, 3 | BRF_GRA },           //  5 Foreground Tiles
	{ "ic50.bin",		0x1000, 0xf09da428, 3 | BRF_GRA },           //  6

	{ "ic64.bin",		0x0100, 0xe9915da8, 4 | BRF_GRA },           //  7 Color Proms

	{ "earlytv_lo.u2",	0x4000, 0xdbfce45f, 5 | BRF_GRA },           //  8 Question Data
	{ "earlytv_hi.u1",	0x4000, 0xc8f5a02d, 5 | BRF_GRA },           //  9
	{ "sex_lo.u4",		0x4000, 0x27a4ff7a, 5 | BRF_GRA },           // 10
	{ "sex_hi.u3",		0x4000, 0xde84fc2f, 5 | BRF_GRA },           // 11
	{ "rock_lo.u6",		0x4000, 0xec1df27b, 5 | BRF_GRA },           // 12
	{ "rock_hi.u5",		0x4000, 0x8a4eccc9, 5 | BRF_GRA },           // 13
	{ "lifesci_lo.u8",	0x4000, 0xb58ba9eb, 5 | BRF_GRA },           // 14
	{ "lifesci_hi.u7",	0x4000, 0xe03dfa12, 5 | BRF_GRA },           // 15
};

STD_ROM_PICK(trvmstrb)
STD_ROM_FN(trvmstrb)

struct BurnDriver BurnDrvTrvmstrb = {
	"trvmstrb", "trvmstr", NULL, NULL, "1985",
	"Trivia Master (set 3)\0", NULL, "Enerdyne Technologies Inc.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_QUIZ, 0,
	NULL, trvmstrbRomInfo, trvmstrbRomName, NULL, NULL, TrvmstrInputInfo, TrvmstrDIPInfo,
	trvmstrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 288, 3, 4
};


// Trivia Master (set 4)

static struct BurnRomInfo trvmstrcRomDesc[] = {
	{ "jaleco.30",		0x1000, 0x9a80c5a7, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "jaleco.28",		0x1000, 0x70322d65, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "jaleco.26",		0x1000, 0x3431a2ba, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "jaleco.44",		0x1000, 0xdac8cff7, 2 | BRF_GRA },           //  3 Background Tiles
	{ "jaleco.46",		0x1000, 0xa97ab879, 2 | BRF_GRA },           //  4

	{ "jaleco.48",		0x1000, 0x79952015, 3 | BRF_GRA },           //  5 Foreground Tiles
	{ "jaleco.50",		0x1000, 0xf09da428, 3 | BRF_GRA },           //  6

	{ "jaleco.64",		0x0100, 0xe9915da8, 4 | BRF_GRA },           //  7 Color Proms

	{ "jaleco.2",		0x4000, 0x1ad4c446, 5 | BRF_GRA },           //  8 Question Data
	{ "jaleco.1",		0x4000, 0x9c308849, 5 | BRF_GRA },           //  9
	{ "jaleco.4",		0x4000, 0x38dd45cd, 5 | BRF_GRA },           // 10
	{ "jaleco.3",		0x4000, 0x83b5465b, 5 | BRF_GRA },           // 11
	{ "jaleco.6",		0x4000, 0x4a2263a7, 5 | BRF_GRA },           // 12
	{ "jaleco.5",		0x4000, 0xbd31f382, 5 | BRF_GRA },           // 13
	{ "jaleco.8",		0x4000, 0xb73f2e31, 5 | BRF_GRA },           // 14
	{ "jaleco.7",		0x4000, 0xbf654110, 5 | BRF_GRA },           // 15
};

STD_ROM_PICK(trvmstrc)
STD_ROM_FN(trvmstrc)

struct BurnDriver BurnDrvTrvmstrc = {
	"trvmstrc", "trvmstr", NULL, NULL, "1985",
	"Trivia Master (set 4)\0", NULL, "Enerdyne Technologies Inc.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_QUIZ, 0,
	NULL, trvmstrcRomInfo, trvmstrcRomName, NULL, NULL, TrvmstrInputInfo, TrvmstrDIPInfo,
	trvmstrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 288, 3, 4
};


// Trivia Genius

static struct BurnRomInfo trvgnsRomDesc[] = {
	{ "trvgns.30",		0x1000, 0xa17f172c, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "trvgns.28",		0x1000, 0x681a1bff, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "trvgns.26",		0x1000, 0x5b4068b8, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "trvgns.44",		0x1000, 0xcd67f2cb, 2 | BRF_GRA },           //  3 Background Tiles
	{ "trvgns.46",		0x1000, 0xf4021941, 2 | BRF_GRA },           //  4

	{ "trvgns.48",		0x1000, 0x6d05845e, 3 | BRF_GRA },           //  5 Foreground Tiles
	{ "trvgns.50",		0x1000, 0xac292be8, 3 | BRF_GRA },           //  6

	{ "82s129.ic63.bin",	0x0100, 0x8ab6076a, 4 | BRF_GRA },           //  7 Color Proms
	{ "82s129.ic64.bin",	0x0100, 0xc766c54a, 4 | BRF_GRA },           //  8

	{ "trvgns.u2",		0x4000, 0x109bd359, 5 | BRF_GRA },           //  9 Question Data
	{ "trvgns.u1",		0x4000, 0x8e8b5f71, 5 | BRF_GRA },           // 10
	{ "trvgns.u4",		0x4000, 0xb73f2e31, 5 | BRF_GRA },           // 11
	{ "trvgns.u3",		0x4000, 0xbf654110, 5 | BRF_GRA },           // 12
	{ "trvgns.u6",		0x4000, 0x4a2263a7, 5 | BRF_GRA },           // 13
	{ "trvgns.u5",		0x4000, 0xbd31f382, 5 | BRF_GRA },           // 14
	{ "trvgns.u8",		0x4000, 0xdbfce45f, 5 | BRF_GRA },           // 15
	{ "trvgns.u7",		0x4000, 0xc8f5a02d, 5 | BRF_GRA },           // 16
};

STD_ROM_PICK(trvgns)
STD_ROM_FN(trvgns)

static INT32 trvgnsInit()
{
	return DrvInit(3);
}

struct BurnDriver BurnDrvTrvgns = {
	"trvgns", "trvmstr", NULL, NULL, "1985",
	"Trivia Genius\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_QUIZ, 0,
	NULL, trvgnsRomInfo, trvgnsRomName, NULL, NULL, TrvmstrInputInfo, TrvmstrDIPInfo,
	trvgnsInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 288, 3, 4
};

