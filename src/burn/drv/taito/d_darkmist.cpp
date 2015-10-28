// FB Alpha The Lost Castle in Darkmist driver module
// Based on MAME driver by David Haywood, Nicola Salmoria, and Tomasz Slanina

#include "tiles_generic.h"
#include "z80_intf.h"
#include "burn_ym2151.h"
#include "t5182.h"
#include "bitswap.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvZ80ROMDec;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvGfxROM4;
static UINT8 *DrvColPROM;
static UINT16 *DrvColTable;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvScrollRAM;
static UINT8 *DrvPalRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 *sprite_bank;
static UINT8 *z80_bank;
static UINT8 *layer_enable;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[2];
static UINT8 DrvDips[3];
static UINT8 DrvReset;
static UINT8 DrvInputs[3];

static struct BurnInputInfo DarkmistInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Darkmist)

static struct BurnDIPInfo DarkmistDIPList[]=
{
	{0x11, 0xff, 0xff, 0xe7, NULL			},
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x7f, NULL			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x12, 0x01, 0x07, 0x00, "5 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x04, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x02, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x06, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x07, 0x03, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x07, 0x01, "1 Coin  5 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x12, 0x01, 0x18, 0x10, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x18, 0x18, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x18, 0x00, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0x18, 0x08, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x12, 0x01, 0x20, 0x20, "Off"			},
	{0x12, 0x01, 0x20, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x12, 0x01, 0x40, 0x40, "Off"			},
	{0x12, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x12, 0x01, 0x80, 0x80, "No"			},
	{0x12, 0x01, 0x80, 0x00, "Yes"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x13, 0x01, 0x01, 0x01, "Upright"		},
	{0x13, 0x01, 0x01, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x06, 0x06, "Easy"			},
	{0x13, 0x01, 0x06, 0x04, "Normal"		},
	{0x13, 0x01, 0x06, 0x02, "Hard"			},
	{0x13, 0x01, 0x06, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0x18, 0x18, "1"			},
	{0x13, 0x01, 0x18, 0x10, "2"			},
	{0x13, 0x01, 0x18, 0x08, "3"			},
	{0x13, 0x01, 0x18, 0x00, "4"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x13, 0x01, 0x60, 0x20, "10K / 20K"		},
	{0x13, 0x01, 0x60, 0x60, "20K / 40K"		},
	{0x13, 0x01, 0x60, 0x40, "30K / 60K"		},
	{0x13, 0x01, 0x60, 0x00, "40K / 80K"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x80, 0x80, "Off"			},
	{0x13, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Darkmist)

static void bankswitch(INT32 data)
{
	*z80_bank = data;

	INT32 nBank = (data & 0x80) ? 0x14000 : 0x10000;

	ZetMapMemory(DrvZ80ROM + nBank, 0x8000, 0xbfff, MAP_ROM);
}

static void __fastcall darkmist_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfc00) == 0xd000) {
		if (data != DrvPalRAM[address & 0x3ff]) {
			DrvPalRAM[address & 0x3ff] = data;
			DrvRecalc = 1;
		}
		return;
	}

	if ((address & 0xff80) == 0xd600) {
		t5182SharedRAM[address & 0x7f] = data;
		return;
	}

	switch (address)
	{
		case 0xc804:
			*layer_enable = data;
			bankswitch(data);
		return;

		case 0xc805:
			*sprite_bank = data;
		return;

		case 0xd680:
			ZetClose();
			ZetOpen(1);
			t5182_setirq_callback(CPU_ASSERT);
			ZetClose();
			ZetOpen(0);
		return;

		case 0xd682:
		case 0xd683:
			t5182_semaphore_main = ~address & 1;
		return;
	}
}

static UINT8 __fastcall darkmist_main_read(UINT16 address)
{
	if ((address & 0xff80) == 0xd600) {
		return t5182SharedRAM[address & 0x7f];
	}

	switch (address)
	{
		case 0xc801:
			return DrvInputs[0];

		case 0xc802:
			return DrvInputs[1];

		case 0xc803:
			return (DrvInputs[2] & ~0xe7) | (DrvDips[0] & 0xe7);

		case 0xc806:
			return DrvDips[1];

		case 0xc807:
			return DrvDips[2];

		case 0xc808:
			return 0xff; // unknown

		case 0xd681:
			return t5182_semaphore_snd;
	}

	return 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	t5182Reset();

	DrvRecalc = 1;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM	= Next; Next += 0x018000;
	DrvZ80ROMDec	= Next; Next += 0x008000;

	t5182ROM	= Next; Next += 0x010000;

	DrvGfxROM0	= Next; Next += 0x008000;
	DrvGfxROM1	= Next; Next += 0x080000;
	DrvGfxROM2	= Next; Next += 0x080000;
	DrvGfxROM3	= Next; Next += 0x020000;
	DrvGfxROM4	= Next; Next += 0x020000;

	DrvColPROM	= Next; Next += 0x000400;
	DrvColTable	= (UINT16*)Next; Next += 0x0400 * sizeof(UINT16);
	DrvPalette	= (UINT32*)Next; Next += 0x0400 * sizeof(UINT32);

	AllRam		= Next;

	DrvPalRAM	= Next; Next += 0x000400;
	DrvScrollRAM	= Next; Next += 0x000100;
	DrvZ80RAM	= Next; Next += 0x001000;
	DrvVidRAM	= Next; Next += 0x000800;
	DrvSprRAM	= Next; Next += 0x001000;

	t5182SharedRAM	= Next; Next += 0x000100;

	layer_enable	= Next; Next += 0x000001;
	z80_bank	= Next; Next += 0x000001;
	sprite_bank	= Next; Next += 0x000001;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static void decrypt_prg()
{
	for (INT32 i = 0; i < 0x8000; i++)
	{
		UINT8 p, d;
		p = d = DrvZ80ROM[i];

		if (((i & 0x20) == 0x00) && ((i & 0x8) != 0))
			p ^= 0x20;

		if (((i & 0x20) == 0x00) && ((i & 0xa) != 0))
			d ^= 0x20;

		if (((i & 0x200) == 0x200) && ((i & 0x408) != 0))
			p ^= 0x10;

		if ((i & 0x220) != 0x200)
		{
			p = BITSWAP08(p, 7,6,5,2,3,4,1,0);
			d = BITSWAP08(d, 7,6,5,2,3,4,1,0);
		}

		DrvZ80ROM[i] = d;
		DrvZ80ROMDec[i] = p;
	}
}

static void decrypt_gfx()
{
	UINT8 *rom = DrvGfxROM0;
	INT32 size = 0x4000;
	UINT8 *buf = (UINT8*)BurnMalloc(0x40000);

	for (INT32 i = 0;i < size/2;i++)
	{
		INT32 w1;

		w1 = (rom[i + 0*size/2] << 8) + rom[i + 1*size/2];

		w1 = BITSWAP16(w1, 9,14,7,2, 6,8,3,15,10,13,5,12,0,11,4,1);

		buf[i + 0*size/2] = w1 >> 8;
		buf[i + 1*size/2] = w1 & 0xff;
	}

	for (INT32 i = 0;i < size;i++)
	{
		rom[i] = buf[BITSWAP24(i,23,22,21,20,19,18,17,16,15,14,13,12,3,2,1,11,10,9,8,0,7,6,5,4)];
	}

	rom = DrvGfxROM1;
	size = 0x40000;

	for (INT32 i = 0;i < size/2;i++)
	{
		INT32 w1;

		w1 = (rom[i + 0*size/2] << 8) + rom[i + 1*size/2];

		w1 = BITSWAP16(w1, 9,14,7,2, 6,8,3,15,10,13,5,12,0,11,4,1);

		buf[i + 0*size/2] = w1 >> 8;
		buf[i + 1*size/2] = w1 & 0xff;
	}

	for (INT32 i = 0;i < size;i++)
	{
		rom[i] = buf[BITSWAP24(i,23,22,21,20,19,18,17,16,15,14,13,5,4,3,2,12,11,10,9,8,1,0,7,6)];
	}

	rom = DrvGfxROM2;
	size = 0x40000;

	for (INT32 i = 0;i < size/2;i++)
	{
		INT32 w1;

		w1 = (rom[i + 0*size/2] << 8) + rom[i + 1*size/2];

		w1 = BITSWAP16(w1, 9,14,7,2,6,8,3,15,10,13,5,12,0,11,4,1);

		buf[i + 0*size/2] = w1 >> 8;
		buf[i + 1*size/2] = w1 & 0xff;
	}

	for (INT32 i = 0;i < size;i++)
	{
		rom[i] = buf[BITSWAP24(i,23,22,21,20,19,18,17,16,15,14,12,11,10,9,8,5,4,3,13,7,6,1,0,2)];
	}

	rom = DrvGfxROM3;
	size = 0x10000;

	memcpy( buf, rom, size );

	for (INT32 i = 0;i < size;i++)
	{
		rom[i] = buf[BITSWAP24(i,23,22,21,20,19,18,17,16,15,6,5,4,3,2,14,13,12,11,8,7,1,0,10,9)];
	}

	rom = DrvGfxROM4;
	size = 0x08000;

	memcpy( buf, rom, size );

	for (INT32 i = 0;i < size;i++)
	{
		rom[i] = buf[BITSWAP24(i,23,22,21,20,19,18,17,16,15,14,5,4,3,2,11,10,9,8,13,12,1,0,7,6)];
	}

	BurnFree(buf);
}

static void decrypt_snd()
{
	for (INT32 i = 0x8000; i < 0x10000; i++)
	{
		t5182ROM[i] = BITSWAP08(t5182ROM[i], 7,1,2,3,4,5,6,0);
	}
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[4]  = { 0, 4, RGN_FRAC(0x04000, 1,2), RGN_FRAC(0x04000, 1,2)+4 };
	INT32 Plane1[4]  = { 0, 4, RGN_FRAC(0x40000, 1,2), RGN_FRAC(0x40000, 1,2)+4 };
	INT32 Plane2[4]  = { 0, 4, RGN_FRAC(0x40000, 1,2), RGN_FRAC(0x40000, 1,2)+4 };
	INT32 XOffs[16]  = { STEP4(0,1), STEP4(8,1), STEP4(16,1), STEP4(24,1) };
	INT32 YOffs0[8]  = { STEP8(0,16) };
	INT32 YOffs1[16] = { STEP16(0,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x40000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x04000);

	GfxDecode(0x0200, 4,  8,  8, Plane0, XOffs, YOffs0, 0x080, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x40000);

	GfxDecode(0x0800, 4, 16, 16, Plane1, XOffs, YOffs1, 0x200, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x40000);

	GfxDecode(0x0800, 4, 16, 16, Plane2, XOffs, YOffs1, 0x200, tmp, DrvGfxROM2);

	BurnFree (tmp);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x400; i++)
	{
		int ctabentry;

		if (DrvColPROM[i] & 0x40)
			ctabentry = 0x100;
		else
		{
			ctabentry = (DrvColPROM[i] & 0x3f);

			switch (i & 0x300)
			{
				case 0x000:  ctabentry = ctabentry | 0x80; break;
				case 0x100:  ctabentry = ctabentry | 0x00; break;
				case 0x200:  ctabentry = ctabentry | 0x40; break;
				case 0x300:  ctabentry = ctabentry | 0xc0; break;
			}
		}

		DrvColTable[i] = ctabentry;
	}
}

static INT32 DrvInit()
{
	BurnSetRefreshRate(60.00);

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM  + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x10000,  1, 1)) return 1;

		if (BurnLoadRom(t5182ROM   + 0x00000,  2, 1)) return 1;
		if (BurnLoadRom(t5182ROM   + 0x08000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x02000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x10000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x30000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x20000,  9, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x10000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x20000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x30000, 13, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x00000, 14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x08000, 15, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM4 + 0x00000, 16, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM4 + 0x04000, 17, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000, 18, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00100, 19, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00200, 20, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00300, 21, 1)) return 1;

		decrypt_prg();
		decrypt_gfx();
		decrypt_snd();

		DrvGfxDecode();
		DrvPaletteInit();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80ROMDec,	0x0000, 0x7fff, MAP_FETCHOP);
	ZetMapMemory(DrvPalRAM,		0xd000, 0xd3ff, MAP_ROM);
	ZetMapMemory(DrvScrollRAM,	0xd400, 0xd4ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,		0xd800, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM,		0xe000, 0xefff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xf000, 0xffff, MAP_RAM);
	ZetSetWriteHandler(darkmist_main_write);
	ZetSetReadHandler(darkmist_main_read);
	ZetClose();

	t5182Init(1, 14318180/4);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	t5182Exit();

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteUpdate()
{
	UINT32 p[0x101];

	p[0x100] = 0;

	for (INT32 i = 0; i < 0x100; i++) {
		INT32 r = DrvPalRAM[i+0x200] & 0x0f;
		INT32 g = DrvPalRAM[i+0x000] >> 4;
		INT32 b = DrvPalRAM[i+0x000] & 0x0f;

		p[i] = BurnHighCol(r+(r*16), g+(g*16), b+(b*16), 0);
	}		

	for (INT32 i = 0; i < 0x400; i++) {
		DrvPalette[i] = p[DrvColTable[i]];
	}
}

static void draw_bg_layer()
{
	INT32 scrollx = ((DrvScrollRAM[0x02]<<1)&0xfe) + (DrvScrollRAM[0x02]>>7) + ((DrvScrollRAM[0x01]&0xf0)<<4) + ((DrvScrollRAM[0x01]&0x01)<<12);
	INT32 scrolly = ((DrvScrollRAM[0x06]<<1)&0xfe) + (DrvScrollRAM[0x06]>>7) + ((DrvScrollRAM[0x05]&0x30)<<4);

	scrolly = (scrolly + 16) & 0x3ff;

#if 1
	INT32 scrx0 = scrollx >> 4;
	INT32 scry0 = scrolly >> 4;
	INT32 scrx1 = scrollx & 0x0f;
	INT32 scry1 = scrolly & 0x0f;
	INT32 scrx2 = scrx1 ? 1 : 0;
	INT32 scry2 = scry1 ? 1 : 0;

	for (INT32 y = 0; y < 14 + scry2; y++)
	{
		INT32 sy = (((scry0 + y) & 0x3f) * 0x200);

		for (INT32 x = 0; x < 16 + scrx2; x++)
		{
			INT32 offs = sy + ((scrx0 + x) & 0x1ff);

			INT32 attr  = DrvGfxROM3[offs + 0x8000];
			INT32 code  = DrvGfxROM3[offs] + ((attr & 0x03) * 256);
			INT32 color = attr >> 4;

			if (y == 0 || y >= 14 || x == 0 || x >= 16)
				Render16x16Tile_Clip(pTransDraw, code, (x*16) - scrx1, (y*16) - scry1, color, 4, 0x000, DrvGfxROM1);
			else
				Render16x16Tile(pTransDraw, code, (x*16) - scrx1, (y*16) - scry1, color, 4, 0x000, DrvGfxROM1);
		}
	}
#else
	for (INT32 offs = 0; offs < 512 * 64; offs++)
	{
		INT32 sx = (offs & 0x1ff) * 16;
		INT32 sy = (offs / 0x200) * 16;

		sx -= scrollx;
		if (sx < -15) sx += 512 * 16;
		sy -= scrolly;
		if (sy < -15) sy += 64 * 16;

		if (sx >= nScreenWidth || sy >= nScreenHeight) continue;

		INT32 attr  = DrvGfxROM3[offs + 0x8000];
		INT32 code  = DrvGfxROM3[offs] + ((attr & 0x03) * 256);
		INT32 color = attr >> 4;

		Render16x16Tile_Clip(pTransDraw, code, sx, sy, color, 4, 0x000, DrvGfxROM1);
	}
#endif
}

static void draw_fg_layer()
{
	INT32 scrollx = ((DrvScrollRAM[0x0a]<<1)&0xfe) + (DrvScrollRAM[0x0a]>>7) + ((DrvScrollRAM[0x09]&0x30)<<4);
	INT32 scrolly = ((DrvScrollRAM[0x0e]<<1)&0xfe) + (DrvScrollRAM[0x0e]>>7) + ((DrvScrollRAM[0x0d]&0xf0)<<4);

	scrolly = (scrolly + 16) & 0xfff;

#if 1
	INT32 scrx0 = scrollx >> 4;
	INT32 scry0 = scrolly >> 4;
	INT32 scrx1 = scrollx & 0x0f;
	INT32 scry1 = scrolly & 0x0f;
	INT32 scrx2 = scrx1 ? 1 : 0;
	INT32 scry2 = scry1 ? 1 : 0;

	for (INT32 y = 0; y < 14 + scry2; y++)
	{
		INT32 sy = (((scry0 + y) & 0xff) * 0x40);

		for (INT32 x = 0; x < 16 + scrx2; x++)
		{
			INT32 offs = sy + ((scrx0 + x) & 0x3f);

			INT32 attr  = DrvGfxROM4[offs + 0x4000];
			INT32 code  = DrvGfxROM4[offs] + ((attr & 0x03) * 256) + 0x400;
			INT32 color = attr >> 4;

			if (y == 0 || y >= 14 || x == 0 || x >= 16)
				Render16x16Tile_Mask_Clip(pTransDraw, code, (x*16) - scrx1, (y*16) - scry1, color, 4, 0, 0x100, DrvGfxROM1);
			else
				Render16x16Tile_Mask(pTransDraw, code, (x*16) - scrx1, (y*16) - scry1, color, 4, 0, 0x100, DrvGfxROM1);
		}
	}
#else
	for (INT32 offs = 0; offs < 64 * 256; offs++)
	{
		INT32 sx = (offs & 0x3f) * 16;
		INT32 sy = (offs / 0x40) * 16;

		sx -= scrollx;
		if (sx < -15) sx += 64 * 16;
		sy -= scrolly;
		if (sy < -15) sy += 256 * 16;

		if (sx >= nScreenWidth || sy >= nScreenHeight) continue;

		INT32 attr  = DrvGfxROM4[offs + 0x4000];
		INT32 code  = DrvGfxROM4[offs] + ((attr & 0x03) * 256) + 0x400;
		INT32 color = attr >> 4;

		Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0x100, DrvGfxROM1);
	}
#endif
}

static void draw_txt_layer()
{
	for (INT32 offs = 32 * 2; offs < (32 * 32) - (32 * 2); offs++)
	{
		INT32 sx = (offs & 0x1f) * 8;
		INT32 sy = (offs / 0x20) * 8;

		INT32 attr  = DrvVidRAM[offs + 0x400];
		INT32 code  = DrvVidRAM[offs] + ((attr & 0x01) * 256);
		INT32 color = attr >> 1;

		if (code == 0) continue; // not thoroughly tested...

		Render8x8Tile_Mask(pTransDraw, code, sx, sy - 16, color, 4, 0, 0x300, DrvGfxROM0);
	}
}

static void draw_sprites()
{
	for (INT32 i = 0; i < 0x1000; i+=0x20)
	{
		INT32 sx    = DrvSprRAM[i + 0x03];
		INT32 sy    = DrvSprRAM[i + 0x02];
		INT32 attr  = DrvSprRAM[i + 0x01];
		INT32 code  = DrvSprRAM[i + 0x00];
		INT32 color = (attr & 0x1e) >> 1;
		INT32 flipy =  attr & 0x40;
		INT32 flipx =  attr & 0x80;

		//if (attr & 0x20) code += *sprite_bank * 256;
		if (attr & 0x20) code += (*sprite_bank & 0x07) * 256;

		if (attr & 0x01) color = rand() & 0x0f; // ?

		if (flipy) {
			if (flipx) {
				Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy - 16, color, 4, 0, 0x200, DrvGfxROM2);
			} else {
				Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy - 16, color, 4, 0, 0x200, DrvGfxROM2);
			}
		} else {
			if (flipx) {
				Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy - 16, color, 4, 0, 0x200, DrvGfxROM2);
			} else {
				Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy - 16, color, 4, 0, 0x200, DrvGfxROM2);
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvRecalc = 0;
		DrvPaletteUpdate();
	}

	if ((*layer_enable & 0x04) == 0) {
		BurnTransferClear();
	}

	if (*layer_enable & 0x04) draw_bg_layer();
	if (*layer_enable & 0x02) draw_fg_layer();
	if (*layer_enable & 0x01) draw_sprites();
	if (*layer_enable & 0x10) draw_txt_layer();

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

		t5182_coin_input = (DrvJoy4[0] << 0) | (DrvJoy4[1] << 1);
	}

	INT32 nSegment;
	INT32 nInterleave = 16;
	INT32 nSoundBufferPos = 0;
	INT32 nCyclesTotal[2] = { 4000000 / 60, 63079 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);

		nSegment = ((nCyclesTotal[0] / nInterleave) * (i + 1)) - ZetTotalCycles();

		nCyclesDone[0] += ZetRun(nSegment);

		if (i == 0) {
			ZetSetVector(0x08);
			ZetSetIRQLine(0, CPU_IRQSTATUS_AUTO);
		}

		if (i == 15) {
			ZetSetVector(0x10);
			ZetSetIRQLine(0, CPU_IRQSTATUS_AUTO);
		}

		ZetClose();

		ZetOpen(1); // t5182

		nSegment = ((nCyclesTotal[1] / nInterleave) * (i + 1)) - ZetTotalCycles();

		nCyclesDone[1] += ZetRun(nSegment);

		if (pBurnSoundOut) {
			nSegment = nBurnSoundLen / nInterleave;

			BurnYM2151Render(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);

			nSoundBufferPos += nSegment;
		}

		ZetClose();
	}

	ZetOpen(1); // t5182

	if (pBurnSoundOut) {
		nSegment = nBurnSoundLen - nSoundBufferPos;
		if (nSegment > 0) {
			BurnYM2151Render(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
		}
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
	
	if (pnMin != NULL) {
		*pnMin = 0x029729;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		ZetScan(nAction);

		t5182Scan(nAction);
	}

	if (nAction & ACB_WRITE) {
		DrvRecalc = 1;
	}

	return 0;
}



// The Lost Castle In Darkmist

static struct BurnRomInfo darkmistRomDesc[] = {
	{ "dm_15.rom",		0x08000, 0x21e6503c, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "dm_16.rom",		0x08000, 0x094579d9, 1 | BRF_PRG | BRF_ESS }, //  1

#if !defined (ROM_VERIFY)
	{ "t5182.rom",		0x02000, 0xd354c8fc, 2 | BRF_PRG | BRF_ESS }, //  2 t5182 (Z80 #1) Code
#else
	{ "",				0x00000, 0x00000000, 2 | BRF_PRG | BRF_ESS }, //  2 t5182 (Z80 #1) Code
#endif

	{ "dm_17.rom",		0x08000, 0x7723dcae, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "dm_13.rom",		0x02000, 0x38bb38d9, 3 | BRF_GRA },           //  4 Characters
	{ "dm_14.rom",		0x02000, 0xac5a31f3, 3 | BRF_GRA },           //  5

	{ "dm_05.rom",		0x10000, 0xca79a738, 4 | BRF_GRA },           //  6 Background & foreground tiles
	{ "dm_01.rom",		0x10000, 0x652aee6b, 4 | BRF_GRA },           //  7
	{ "dm_06.rom",		0x10000, 0x9629ed2c, 4 | BRF_GRA },           //  8
	{ "dm_02.rom",		0x10000, 0xe2dd15aa, 4 | BRF_GRA },           //  9

	{ "dm_09.rom",		0x10000, 0x52154b50, 5 | BRF_GRA },           // 10 Sprites
	{ "dm_11.rom",		0x08000, 0x3118e2f9, 5 | BRF_GRA },           // 11
	{ "dm_10.rom",		0x10000, 0x34fd52b5, 5 | BRF_GRA },           // 12
	{ "dm_12.rom",		0x08000, 0xcc4b9839, 5 | BRF_GRA },           // 13

	{ "dm_03.rom",		0x08000, 0x60b40c2a, 6 | BRF_GRA },           // 14 Background tile map
	{ "dm_04.rom",		0x08000, 0xd47b8cd9, 6 | BRF_GRA },           // 15

	{ "dm_07.rom",		0x04000, 0x889b1277, 7 | BRF_GRA },           // 16 Foreground tile map
	{ "dm_08.rom",		0x04000, 0xf76f6f46, 7 | BRF_GRA },           // 17

	{ "63s281n.m7",		0x00100, 0x897ef49f, 8 | BRF_GRA },           // 18 Color Proms
	{ "63s281n.d7",		0x00100, 0xa9975a96, 8 | BRF_GRA },           // 19
	{ "63s281n.f11",	0x00100, 0x8096b206, 8 | BRF_GRA },           // 20
	{ "63s281n.j15",	0x00100, 0x2ea780a4, 8 | BRF_GRA },           // 21

	{ "63s281n.l1",		0x00100, 0x208d17ca, 0 | BRF_OPT },           // 22 Unknown Proms
	{ "82s129.d11",		0x00100, 0x866eab0e, 0 | BRF_OPT },           // 23
};

STD_ROM_PICK(darkmist)
STD_ROM_FN(darkmist)

struct BurnDriver BurnDrvDarkmist = {
	"darkmist", NULL, NULL, NULL, "1986",
	"The Lost Castle In Darkmist\0", NULL, "Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, darkmistRomInfo, darkmistRomName, NULL, NULL, DarkmistInputInfo, DarkmistDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	224, 256, 3, 4
};


