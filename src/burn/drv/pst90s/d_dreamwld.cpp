// FB Alpha Dream World / Baryon driver module
// Based on MAME driver by David Haywood

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "msm6295.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvSndROM0;
static UINT8 *DrvSndROM1;
static UINT8 *DrvProtData;
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprBuf;
static UINT8 *DrvSprBuf2;
static UINT8 *DrvPalRAM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvBg1RAM;
static UINT8 *DrvBg2RAM;
static UINT8 *DrvBgScrollRAM;

static UINT32 *DrvPalette;
static UINT8  DrvRecalc;

static UINT8 DrvReset;
static UINT8 DrvJoy1[32];
static UINT32 DrvInputs;
static UINT8 DrvDips[2];

static UINT8 *DrvOkiBank;
static INT32 protindex = 0;
static INT32 protsize;

static struct BurnInputInfo CommonInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 24,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 31,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 30,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 28,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 29,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 27,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 26,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 25,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 16,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy1 + 23,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy1 + 22,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy1 + 20,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 21,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 19,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 18,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 17,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Common)

static struct BurnDIPInfo DreamwldDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0x03, 0x02, "2"			},
	{0x13, 0x01, 0x03, 0x03, "3"			},
	{0x13, 0x01, 0x03, 0x01, "4"			},
	{0x13, 0x01, 0x03, 0x00, "5"			},
	
	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x13, 0x01, 0x80, 0x80, "Off"			},
	{0x13, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x14, 0x01, 0x0e, 0x00, "5 Coins 1 Credits"	},
	{0x14, 0x01, 0x0e, 0x02, "4 Coins 1 Credits"	},
	{0x14, 0x01, 0x0e, 0x04, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0x0e, 0x06, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x0e, 0x0e, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x0e, 0x0a, "2 Coins 3 Credits"	},
	{0x14, 0x01, 0x0e, 0x0c, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0x0e, 0x08, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Dreamwld)

static struct BurnDIPInfo BaryonDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0xfe, NULL			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0x03, 0x02, "2"			},
	{0x13, 0x01, 0x03, 0x03, "3"			},
	{0x13, 0x01, 0x03, 0x01, "4"			},
	{0x13, 0x01, 0x03, 0x00, "5"			},

	{0   , 0xfe, 0   ,    2, "Bomb Stock"		},
	{0x13, 0x01, 0x04, 0x04, "2"			},
	{0x13, 0x01, 0x04, 0x00, "3"			},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x13, 0x01, 0x80, 0x80, "Off"			},
	{0x13, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x14, 0x01, 0x01, 0x01, "Off"			},
	{0x14, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x14, 0x01, 0x0e, 0x00, "5 Coins 1 Credits"	},
	{0x14, 0x01, 0x0e, 0x02, "4 Coins 1 Credits"	},
	{0x14, 0x01, 0x0e, 0x04, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0x0e, 0x06, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x0e, 0x0e, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x0e, 0x0a, "2 Coins 3 Credits"	},
	{0x14, 0x01, 0x0e, 0x0c, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0x0e, 0x08, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Baryon)

static struct BurnDIPInfo RolcrushDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0xfe, NULL			},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x13, 0x01, 0x80, 0x80, "Off"			},
	{0x13, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x14, 0x01, 0x01, 0x01, "Off"			},
	{0x14, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x14, 0x01, 0x0e, 0x00, "5 Coins 1 Credits"	},
	{0x14, 0x01, 0x0e, 0x02, "4 Coins 1 Credits"	},
	{0x14, 0x01, 0x0e, 0x04, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0x0e, 0x06, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x0e, 0x0e, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x0e, 0x0a, "2 Coins 3 Credits"	},
	{0x14, 0x01, 0x0e, 0x0c, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0x0e, 0x08, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    8, "Difficulty"		},
	{0x14, 0x01, 0x70, 0x20, "Level 1"		},
	{0x14, 0x01, 0x70, 0x10, "Level 2"		},
	{0x14, 0x01, 0x70, 0x00, "Level 3"		},
	{0x14, 0x01, 0x70, 0x70, "Level 4"		},
	{0x14, 0x01, 0x70, 0x60, "Level 5"		},
	{0x14, 0x01, 0x70, 0x50, "Level 6"		},
	{0x14, 0x01, 0x70, 0x40, "Level 7"		},
	{0x14, 0x01, 0x70, 0x30, "Level 8"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Rolcrush)

static struct BurnDIPInfo CutefghtDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0xfe, NULL			},

	{0   , 0xfe, 0   ,    4, "Ticket Payout"	},
	{0x13, 0x01, 0x60, 0x00, "No"			},
	{0x13, 0x01, 0x60, 0x20, "Little"		},
	{0x13, 0x01, 0x60, 0x60, "Normal"		},
	{0x13, 0x01, 0x60, 0x40, "Much"			},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x13, 0x01, 0x80, 0x80, "Off"			},
	{0x13, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x14, 0x01, 0x01, 0x01, "Off"			},
	{0x14, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x14, 0x01, 0x0e, 0x00, "5 Coins 1 Credits"	},
	{0x14, 0x01, 0x0e, 0x02, "4 Coins 1 Credits"	},
	{0x14, 0x01, 0x0e, 0x04, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0x0e, 0x06, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x0e, 0x0e, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x0e, 0x0a, "2 Coins 3 Credits"	},
	{0x14, 0x01, 0x0e, 0x0c, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0x0e, 0x08, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    8, "Difficulty"		},
	{0x14, 0x01, 0x70, 0x20, "Level 1"		},
	{0x14, 0x01, 0x70, 0x10, "Level 2"		},
	{0x14, 0x01, 0x70, 0x00, "Level 3"		},
	{0x14, 0x01, 0x70, 0x70, "Level 4"		},
	{0x14, 0x01, 0x70, 0x60, "Level 5"		},
	{0x14, 0x01, 0x70, 0x50, "Level 6"		},
	{0x14, 0x01, 0x70, 0x40, "Level 7"		},
	{0x14, 0x01, 0x70, 0x30, "Level 8"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Cutefght)

static struct BurnDIPInfo GaialastDIPList[]=
{
	{0x13, 0xff, 0xff, 0xf1, NULL			},
	{0x14, 0xff, 0xff, 0xfe, NULL			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0x03, 0x02, "1"			},
	{0x13, 0x01, 0x03, 0x03, "2"			},
	{0x13, 0x01, 0x03, 0x01, "3"			},
	{0x13, 0x01, 0x03, 0x00, "4"			},

	{0   , 0xfe, 0   ,    2, "Bomb Stock"		},
	{0x13, 0x01, 0x04, 0x04, "2"			},
	{0x13, 0x01, 0x04, 0x00, "3"			},

	{0   , 0xfe, 0   ,    2, "Lock Vertical Scroll"	},
	{0x13, 0x01, 0x08, 0x08, "Off"			},
	{0x13, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x13, 0x01, 0x80, 0x80, "Off"			},
	{0x13, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x14, 0x01, 0x01, 0x01, "Off"			},
	{0x14, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x14, 0x01, 0x0e, 0x00, "5 Coins 1 Credits"	},
	{0x14, 0x01, 0x0e, 0x02, "4 Coins 1 Credits"	},
	{0x14, 0x01, 0x0e, 0x04, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0x0e, 0x06, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x0e, 0x0e, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x0e, 0x0a, "2 Coins 3 Credits"	},
	{0x14, 0x01, 0x0e, 0x0c, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0x0e, 0x08, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    8, "Difficulty"		},
	{0x14, 0x01, 0x70, 0x20, "Level 1"		},
	{0x14, 0x01, 0x70, 0x10, "Level 2"		},
	{0x14, 0x01, 0x70, 0x00, "Level 3"		},
	{0x14, 0x01, 0x70, 0x70, "Level 4"		},
	{0x14, 0x01, 0x70, 0x60, "Level 5"		},
	{0x14, 0x01, 0x70, 0x50, "Level 6"		},
	{0x14, 0x01, 0x70, 0x40, "Level 7"		},
	{0x14, 0x01, 0x70, 0x30, "Level 8"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Gaialast)

inline static void palette_write(INT32 offset)
{
	offset &= 0x1ffe;

	UINT16 data = *((UINT16 *)(DrvPalRAM + offset));
	UINT8 r, g, b;
	offset >>= 1;

	r = (data >> 10) & 0x1f;
	g = (data >>  5) & 0x1f;
	b = (data >>  0) & 0x1f;

	r = (r << 3) | (r >> 2);
	g = (g << 3) | (g >> 2);
	b = (b << 3) | (b >> 2);

	DrvPalette[offset] = BurnHighCol(r, g, b, 0);
}

static void dreamwld_oki_setbank(INT32 chip, INT32 bank)
{
	DrvOkiBank[chip] = bank & 3;
	UINT8 *sound = (chip) ? DrvSndROM1 : DrvSndROM0;

	MSM6295SetBank(chip, sound + 0x30000 + (0x10000 * DrvOkiBank[chip]), 0x30000, 0x3ffff);
}

static UINT8 __fastcall dreamwld_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0xc00000:
		case 0xc00001:
		case 0xc00002:
		case 0xc00003:
			return DrvInputs >> (8 * (~address & 0x3));

		case 0xc00004:
		case 0xc00005:
		case 0xc00006:
		case 0xc00007:
			return DrvDips[address & 1];

		case 0xc00018:
			return MSM6295ReadStatus(0);

		case 0xc00028:
			return MSM6295ReadStatus(1);

		case 0xc00030:
			protindex++;
			return DrvProtData[(protindex-1) % protsize];
	}

	bprintf (PRINT_NORMAL, _T("%5.5x, rb\n"), address);

	return 0;
}

static UINT16 __fastcall dreamwld_read_word(UINT32 address)
{
	switch (address)
	{
		case 0xc00000:
		case 0xc00002:
			return DrvInputs >> (((~address >> 1) & 1) * 16);

		case 0xc00004:
		case 0xc00006:
			return (DrvDips[1] << 8) | DrvDips[0];
	}

	bprintf (PRINT_NORMAL, _T("%5.5x, rw\n"), address);

	return 0;
}

static void __fastcall dreamwld_write_byte(UINT32 address, UINT8 data)
{

	if ((address&0xffff00) == 0xb300) return; // garbage writes (cute fighter)

	switch (address)
	{
		case 0xc0000c:
		case 0xc0000f:
			dreamwld_oki_setbank(0, data & 3);
		return;

		case 0xc00018:
			MSM6295Command(0, data);
		return;

		case 0x000001:
		case 0x000002:
		case 0x00000c:
		case 0xc0fffc:
		case 0xc0fffd:
		case 0xc0fffe:
		case 0xc0ffff:
		case 0xc00010:
		case 0xc00020:
			// NOP
		return;

		case 0xc0002c:
		case 0xc0002f:
			dreamwld_oki_setbank(1, data & 3);
		return;

		case 0xc00028:
			MSM6295Command(1, data);
		return;
	}

	bprintf (PRINT_NORMAL, _T("%5.5x, %2.2x wb\n"), address, data);
}

static tilemap_callback( background )
{
	UINT16 *ram = (UINT16*)DrvBg1RAM;
	UINT32 *lnscroll = (UINT32  *)DrvBgScrollRAM;
	INT32 bank = (lnscroll[0x104] >> 9) & 0x2000;

	TILE_SET_INFO(0, (ram[offs] & 0x1fff) + bank, (ram[offs] >> 13) + 0x80, 0);
}

static tilemap_callback( foreground )
{
	UINT16 *ram = (UINT16*)DrvBg2RAM;
	UINT32 *lnscroll = (UINT32  *)DrvBgScrollRAM;
	INT32 bank = (lnscroll[0x105] >> 9) & 0x2000;

	TILE_SET_INFO(0, (ram[offs] & 0x1fff) + bank, (ram[offs] >> 13) + 0xc0, 0);
}

static INT32 DrvDoReset()
{
	DrvReset = 0;

	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	MSM6295Reset(0);
	MSM6295Reset(1); // dreamwld

	protindex = 0;

	dreamwld_oki_setbank(0, 0);
	dreamwld_oki_setbank(1, 0); // dreamwld

	return 0;
}

static void DrvGfxDecode(UINT8 *src, INT32 len)
{
	for (INT32 i = (len - 1) * 2; i >= 0; i-=2) {
		src[i + 0] = src[i/2] >> 4;
		src[i + 1] = src[i/2] & 0x0f;
	}
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM	= Next; Next += 0x0200000;

	DrvProtData	= Next; Next += 0x0001000;

	MSM6295ROM	= Next;
	DrvSndROM0	= Next; Next += 0x0080000;
	DrvSndROM1	= Next; Next += 0x0080000;

	DrvGfxROM0	= Next; Next += 0x1000000;
	DrvGfxROM1	= Next; Next += 0x0800000;
	DrvGfxROM2	= Next; Next += 0x0040000;

	DrvPalette	= (UINT32*)Next; Next += 0x02000 * sizeof(UINT32);

	AllRam		= Next;

	DrvBgScrollRAM	= Next; Next += 0x0002000;
	DrvSprRAM	= Next; Next += 0x0002000;
	DrvSprBuf	= Next; Next += 0x0002000;
	DrvSprBuf2	= Next; Next += 0x0002000;
	DrvPalRAM	= Next; Next += 0x0002000;
	DrvBg1RAM	= Next; Next += 0x0002000;
	DrvBg2RAM	= Next; Next += 0x0002000;
	Drv68KRAM	= Next; Next += 0x0020000;

	DrvOkiBank	= Next; Next += 0x0000002;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static INT32 DreamwldRomLoad()
{
	if (BurnLoadRom(Drv68KROM  + 0x000003,  0, 4)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x000002,  1, 4)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x000001,  2, 4)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x000000,  3, 4)) return 1;

	if (BurnLoadRom(DrvProtData,            5, 1)) return 1;
	protsize = 0x6c9;

	if (BurnLoadRom(DrvSndROM0,             6, 1)) return 1;
	if (BurnLoadRom(DrvSndROM1,             7, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM0,             8, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM1,             9, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM2 + 0x00000,  10, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x00001,  11, 2)) return 1;

	return 0;
}

static INT32 BaryonRomLoad()
{
	if (BurnLoadRom(Drv68KROM  + 0x000003,  0, 4)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x000001,  1, 4)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x000002,  2, 4)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x000000,  3, 4)) return 1;

	if (BurnLoadRom(DrvProtData,            5, 1)) return 1;
	protsize = 0x6bd;

	if (BurnLoadRom(DrvSndROM0,             6, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM0,             7, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x200000,  8, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM1,             9, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM2 + 0x00000,  10, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x00001,  11, 2)) return 1;

	return 0;
}

static INT32 CutefghtRomLoad()
{
	if (BurnLoadRom(Drv68KROM  + 0x000003,  0, 4)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x000001,  1, 4)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x000002,  2, 4)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x000000,  3, 4)) return 1;

	if (BurnLoadRom(DrvProtData,            5, 1)) return 1;
	protsize = 0x701;

	if (BurnLoadRom(DrvSndROM0,             6, 1)) return 1;
	if (BurnLoadRom(DrvSndROM1,             7, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM0,             8, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x200000,  9, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x400000, 10, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x600000, 11, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM1,            12, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM2 + 0x00000,  13, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x00001,  14, 2)) return 1;

	return 0;
}

static INT32 RolcrushRomLoad()
{
	if (BurnLoadRom(Drv68KROM  + 0x000003,  0, 4)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x000001,  1, 4)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x000002,  2, 4)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x000000,  3, 4)) return 1;

	if (BurnLoadRom(DrvProtData,            5, 1)) return 1;
	protsize = 0x745;

	if (BurnLoadRom(DrvSndROM0,             6, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM0,             7, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM1,             8, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM2 + 0x00000,   9, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x00001,  10, 2)) return 1;

	return 0;
}

static INT32 GaialastRomLoad()
{
	if (BurnLoadRom(Drv68KROM  + 0x000003,  0, 4)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x000001,  1, 4)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x000002,  2, 4)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x000000,  3, 4)) return 1;

	if (BurnLoadRom(DrvProtData,            5, 1)) return 1;
	protsize = 0x6c9;

	if (BurnLoadRom(DrvSndROM0,             6, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM0,             7, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x200000,  8, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x400000,  9, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM1,            10, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM2 + 0x00000,  11, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x00001,  12, 2)) return 1;

	return 0;
}

static INT32 DrvInit(INT32 (*pInitCallback)())
{
	BurnSetRefreshRate(57.79);

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (pInitCallback) {
			if (pInitCallback()) return 1;
		}
			
		DrvGfxDecode(DrvGfxROM0, 0x800000);
		DrvGfxDecode(DrvGfxROM1, 0x400000);
	}

	SekInit(0, 0x68ec020);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x1fffff, MAP_ROM);
	SekMapMemory(DrvSprRAM,		0x400000, 0x401fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		0x600000, 0x601fff, MAP_RAM);
	SekMapMemory(DrvBg1RAM,		0x800000, 0x801fff, MAP_RAM);
	SekMapMemory(DrvBg2RAM,		0x802000, 0x803fff, MAP_RAM);
	SekMapMemory(DrvBgScrollRAM,	0x804000, 0x805fff, MAP_RAM);
	SekMapMemory(Drv68KRAM,		0xfe0000, 0xffffff, MAP_RAM);
	SekSetWriteByteHandler(0,	dreamwld_write_byte);
	//SekSetWriteWordHandler(0,	dreamwld_write_word);
	//SekSetWriteLongHandler(0,	dreamwld_write_long);
	SekSetReadByteHandler(0,	dreamwld_read_byte);
	SekSetReadWordHandler(0,	dreamwld_read_word);
	//SekSetReadLongHandler(0,	dreamwld_read_long);
	SekClose();

	MSM6295Init(0, 1000000 / 165, 1);
	MSM6295Init(1, 1000000 / 165, 1); // dreamwld
	MSM6295SetRoute(0, 0.45, BURN_SND_ROUTE_BOTH);
	MSM6295SetRoute(1, 0.45, BURN_SND_ROUTE_BOTH);
	MSM6295SetBank(0, DrvSndROM0, 0, 0x2ffff);
	MSM6295SetBank(1, DrvSndROM1, 0, 0x2ffff);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, background_map_callback, 16, 16, 64, 64);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, foreground_map_callback, 16, 16, 64, 64);
	GenericTilemapSetGfx(0, DrvGfxROM1, 4, 16, 16, 0x400000, 0, 0xff);
	GenericTilemapSetTransparent(1, 0);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	SekExit();

	MSM6295Exit(0);
	MSM6295Exit(1);

	GenericTilesExit();

	BurnFree (AllMem);

	return 0;
}

static void draw_layer(INT32 layer)
{
	UINT32 *scroll = (UINT32 *)DrvBgScrollRAM;
	UINT16 *lnscroll = (UINT16 *)DrvBgScrollRAM;

	INT32 ctrl = scroll[0x104 + layer] >> 16;

	INT32 scrolly = scroll[0x100 + (layer * 2)] >> 16;
	INT32 scrollx = (scroll[0x101 + (layer * 2)] >> 16) + (layer ? 5 : 3);

	GenericTilemapSetScrollY(layer, scrolly + 32);

	//bprintf (0, _T("CTRL %d %8.8x\n"), layer, scroll[0x104 + layer] >> 16);

	if ((ctrl & 0x0200) == 0x200)	// row scroll
	{
		GenericTilemapSetScrollRows(layer, 1024/16);

		scroll += layer * 0x100;

		for (INT32 y = 0; y < 256; y += 16)
		{
			GenericTilemapSetScrollRow(layer, ((y + scrolly + 32) & 0xff) / 16, scrollx + (scroll[y / 16] >> 16));
		}
	}
	else if ((ctrl & 0x0300) == 0x100)	// line scroll
	{
		GenericTilemapSetScrollRows(layer, 1024);

		scroll += layer * 0x100;

		for (INT32 y = 0; y < 256; y++)
		{
			GenericTilemapSetScrollRow(layer, (y + scrolly + 32) & 0x3ff, (scrollx + (lnscroll[(y + 32) & 0xff])));
		}
	}
	else if ((ctrl & 0x0300) == 0)		// normal scroll
	{
		GenericTilemapSetScrollRows(layer, 1);

		GenericTilemapSetScrollX(layer, scrollx);
	}

	GenericTilemapDraw(layer, pTransDraw, 0);
}

static void draw_sprites()
{
	UINT16 *source   = (UINT16*)DrvSprBuf2;
	UINT16 *finish   = source + 0x2000/2;
	UINT16 *redirect = (UINT16 *)DrvGfxROM2;

	while (source < finish)
	{
		INT32 xpos, ypos, tileno, code, colour, xflip, yflip;
		INT32 xsize, ysize, xinc, yinc, xct, yct;

		xpos   = (source[1]&0x01ff);
		ypos   = (source[0]&0x01ff);
		xsize  = (source[1]&0x0e00) >> 9;
		ysize  = (source[0]&0x0e00) >> 9;
		tileno = (source[3]&0xffff);
		if (source[2]&1) tileno += 0x10000; // fix sprites in cute fighter -dink
		colour = (source[2]&0x3f00) >> 8;
		xflip  = (source[2]&0x4000);
		yflip  = (source[2]&0x8000);

		xinc = 16;
		yinc = 16;

		if (xflip)
		{
			xinc = -16;
			xpos += 16*xsize;
		}

		if (yflip)
		{
			yinc = -16;
			ypos += 16 * ysize;
		}

		xpos -= 16;

		for (yct=0;yct<ysize+1;yct++)
		{
			for (xct=0;xct<xsize+1;xct++)
			{
				code = redirect[tileno];

				if (yflip) {
					if (xflip) {
						Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, xpos+xct*xinc,         ypos+yct*yinc,         colour, 4, 0, 0, DrvGfxROM0);
						Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, (xpos+xct*xinc)-0x200, ypos+yct*yinc,         colour, 4, 0, 0, DrvGfxROM0);
						Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, (xpos+xct*xinc)-0x200, (ypos+yct*yinc)-0x200, colour, 4, 0, 0, DrvGfxROM0);
						Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, xpos+xct*xinc,         (ypos+yct*yinc)-0x200, colour, 4, 0, 0, DrvGfxROM0);
					} else {
						Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, xpos+xct*xinc,         ypos+yct*yinc,         colour, 4, 0, 0, DrvGfxROM0);
						Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, (xpos+xct*xinc)-0x200, ypos+yct*yinc,         colour, 4, 0, 0, DrvGfxROM0);
						Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, (xpos+xct*xinc)-0x200, (ypos+yct*yinc)-0x200, colour, 4, 0, 0, DrvGfxROM0);
						Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, xpos+xct*xinc,         (ypos+yct*yinc)-0x200, colour, 4, 0, 0, DrvGfxROM0);
					}
				} else {
					if (xflip) {
						Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, xpos+xct*xinc,         ypos+yct*yinc,         colour, 4, 0, 0, DrvGfxROM0);
						Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, (xpos+xct*xinc)-0x200, ypos+yct*yinc,         colour, 4, 0, 0, DrvGfxROM0);
						Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, (xpos+xct*xinc)-0x200, (ypos+yct*yinc)-0x200, colour, 4, 0, 0, DrvGfxROM0);
						Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, xpos+xct*xinc,         (ypos+yct*yinc)-0x200, colour, 4, 0, 0, DrvGfxROM0);
					} else {
						Render16x16Tile_Mask_Clip(pTransDraw, code, xpos+xct*xinc,         ypos+yct*yinc,         colour, 4, 0, 0, DrvGfxROM0);
						Render16x16Tile_Mask_Clip(pTransDraw, code, (xpos+xct*xinc)-0x200, ypos+yct*yinc,         colour, 4, 0, 0, DrvGfxROM0);
						Render16x16Tile_Mask_Clip(pTransDraw, code, (xpos+xct*xinc)-0x200, (ypos+yct*yinc)-0x200, colour, 4, 0, 0, DrvGfxROM0);
						Render16x16Tile_Mask_Clip(pTransDraw, code, xpos+xct*xinc,         (ypos+yct*yinc)-0x200, colour, 4, 0, 0, DrvGfxROM0);
					}
				}

				tileno++;
			}
		}

		source += 4;
	}
}

static INT32 DrvDraw()
{
	for (INT32 i = 0; i < 0x2000; i+=2) {
		palette_write(i);
	}

	DrvPalette[0] = BurnHighCol(0xff, 0, 0xff, 0);

	BurnTransferClear();

	if (nBurnLayer &  1) draw_layer(0);
	if (nBurnLayer &  2) draw_layer(1);

	if (nSpriteEnable & 1) draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (&DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 32; i++) {
			DrvInputs ^= (DrvJoy1[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal = (16000000 * 100) / 5779;
	INT32 nCyclesDone = 0;

	SekOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		INT32 nSegment = (nCyclesTotal - nCyclesDone) / (nInterleave - i);
		nCyclesDone += SekRun(nSegment);

		if (i == 224) {
			SekSetIRQLine(4, CPU_IRQSTATUS_ACK); // Oddity: _AUTO doesn't work here..
			nCyclesDone += SekRun(50);
			SekSetIRQLine(4, CPU_IRQSTATUS_NONE);
		}
	}

	SekClose();
	
	if (pBurnSoundOut) {
		memset (pBurnSoundOut, 0, nBurnSoundLen * sizeof(INT16) * 2);
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
		MSM6295Render(1, pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		DrvDraw();
	}

	memcpy (DrvSprBuf2, DrvSprBuf, 0x2000);
	memcpy (DrvSprBuf,  DrvSprRAM, 0x2000);

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029740;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);

		MSM6295Scan(0, nAction);
		MSM6295Scan(1, nAction);

		if (nAction & ACB_WRITE) {
			dreamwld_oki_setbank(0, DrvOkiBank[0]);
			dreamwld_oki_setbank(1, DrvOkiBank[1]);
		}
	}

	return 0;
}


// Baryon - Future Assault (set 1)

static struct BurnRomInfo baryonRomDesc[] = {
	{ "2_semicom",		0x040000, 0x42b14a6c, 1 | BRF_PRG | BRF_ESS }, //  0 68ec020 Code
	{ "4_semicom",		0x040000, 0x6c1cdad0, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3_semicom",		0x040000, 0x0ae6d86e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "5_semicom",		0x040000, 0x15917c9d, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "87c52.mcu",		0x010000, 0x00000000, 2 | BRF_NODUMP },        //  4 MCU code

	{ "protdata.bin",	0x0006bd, 0x117f32a8, 3 | BRF_PRG | BRF_ESS }, //  5 Protection data

	{ "1_semicom",		0x080000, 0xe0349074, 4 | BRF_SND },           //  6 Oki #0 samples

	{ "10_semicom",		0x200000, 0x28bf828f, 5 | BRF_GRA },           //  7 Sprites
	{ "11_semicom",		0x200000, 0xd0ff1bc6, 5 | BRF_GRA },           //  8

	{ "8_semicom",		0x200000, 0x684012e6, 6 | BRF_GRA },           //  9 Background Tiles

	{ "6_semicom",		0x020000, 0xfdbb08b0, 7 | BRF_GRA },           // 10 Sprite Lookup Table
	{ "7_semicom",		0x020000, 0xc9d20480, 7 | BRF_GRA },           // 11

	{ "9_semicom",		0x010000, 0x0da8db45, 8 | BRF_OPT },           // 12 Unknown
};

STD_ROM_PICK(baryon)
STD_ROM_FN(baryon)

static INT32 BaryonInit()
{
	return DrvInit(BaryonRomLoad);
}

struct BurnDriver BurnDrvBaryon = {
	"baryon", NULL, NULL, NULL, "1997",
	"Baryon - Future Assault (set 1)\0", NULL, "SemiCom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, baryonRomInfo, baryonRomName, NULL, NULL, CommonInputInfo, BaryonDIPInfo,
	BaryonInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 304, 3, 4
};


// Baryon - Future Assault (set 2)

static struct BurnRomInfo baryonaRomDesc[] = {
	{ "3.bin",		0x040000, 0x046d4231, 1 | BRF_PRG | BRF_ESS }, //  0 68ec020 Code
	{ "4.bin",		0x040000, 0x59e0df20, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "5.bin",		0x040000, 0x63d5e7cb, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "6.bin",		0x040000, 0xabccbb3d, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "87c52.mcu",		0x010000, 0x00000000, 2 | BRF_NODUMP },        //  4 MCU code

	{ "protdata.bin",	0x0006bd, 0x117f32a8, 3 | BRF_PRG | BRF_ESS }, //  5 Protection data

	{ "1.bin",		0x080000, 0xe0349074, 4 | BRF_SND },           //  6 Oki #0 samples

	{ "9.bin",		0x200000, 0x28bf828f, 5 | BRF_GRA },           //  7 Sprites
	{ "11.bin",		0x200000, 0xd0ff1bc6, 5 | BRF_GRA },           //  8

	{ "2.bin",		0x200000, 0x684012e6, 6 | BRF_GRA },           //  9 Background Tiles

	{ "8.bin",		0x020000, 0xfdbb08b0, 7 | BRF_GRA },           // 10 Sprite Lookup Table
	{ "10.bin",		0x020000, 0xc9d20480, 7 | BRF_GRA },           // 11

	{ "7.bin",		0x010000, 0x0da8db45, 8 | BRF_OPT },           // 12 Unknown
};

STD_ROM_PICK(baryona)
STD_ROM_FN(baryona)
struct BurnDriver BurnDrvBaryona = {
	"baryona", "baryon", NULL, NULL, "1997",
	"Baryon - Future Assault (set 2)\0", NULL, "SemiCom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, baryonaRomInfo, baryonaRomName, NULL, NULL, CommonInputInfo, BaryonDIPInfo,
	BaryonInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 304, 3, 4
};


// Cute Fighter

static struct BurnRomInfo cutefghtRomDesc[] = {
	{ "3_semicom",		0x080000, 0xe7e7a866, 1 | BRF_PRG | BRF_ESS }, //  0 68ec020 Code
	{ "5_semicom",		0x080000, 0xc14fd5dc, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "4_semicom",		0x080000, 0x476a3bf5, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "6_semicom",		0x080000, 0x47440088, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "87c52.mcu",		0x010000, 0x00000000, 2 | BRF_NODUMP },        //  4 MCU code

	{ "protdata.bin",	0x000701, 0x764c3c0e, 3 | BRF_GRA },           //  5 Protection data

	{ "2_semicom",		0x080000, 0x694ddaf9, 4 | BRF_SND },           //  6 Oki #0 samples

	{ "1_semicom",		0x080000, 0xfa3b6890, 5 | BRF_SND },           //  7 Oki #1 samples

	{ "10_semicom",		0x200000, 0x62bf1e6e, 6 | BRF_GRA },           //  8 Sprites
	{ "11_semicom",		0x200000, 0x796f23a7, 6 | BRF_GRA },           //  9
	{ "13_semicom",		0x200000, 0x24222b3c, 6 | BRF_GRA },           // 10
	{ "14_semicom",		0x200000, 0x385b69d7, 6 | BRF_GRA },           // 11

	{ "12_semicom",		0x200000, 0x45d29c22, 7 | BRF_GRA },           // 12 Background Tiles

	{ "7_semicom",		0x020000, 0x39454102, 8 | BRF_GRA },           // 13 Sprite Lookup Table
	{ "8_semicom",		0x020000, 0xfccb1b13, 8 | BRF_GRA },           // 14

	{ "9_semicom",		0x010000, 0x0da8db45, 9 | BRF_OPT },           // 15 unknown
};

STD_ROM_PICK(cutefght)
STD_ROM_FN(cutefght)

static INT32 CutefghtInit()
{
	return DrvInit(CutefghtRomLoad);
}

struct BurnDriver BurnDrvCutefght = {
	"cutefght", NULL, NULL, NULL, "1998",
	"Cute Fighter\0", NULL, "SemiCom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, cutefghtRomInfo, cutefghtRomName, NULL, NULL, CommonInputInfo, CutefghtDIPInfo,
	CutefghtInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	304, 224, 4, 3
};


// Rolling Crush (version 1.07.E - 1999/02/11, Trust license)

static struct BurnRomInfo rolcrushRomDesc[] = {
	{ "mx27c2000_2.bin",	0x040000, 0x5eb24adb, 1 | BRF_PRG | BRF_ESS }, //  0 68ec020 Code
	{ "mx27c2000_4.bin",	0x040000, 0xc47f0540, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mx27c2000_1.bin",	0x040000, 0xa37e15b2, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "mx27c2000_3.bin",	0x040000, 0x7af59294, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "87c52.mcu",		0x010000, 0x00000000, 2 | BRF_NODUMP },        //  4 MCU code

	{ "protdata.bin",	0x000745, 0x06b8a880, 3 | BRF_GRA },           //  5 Protection data

	{ "mx27c4000_5.bin",	0x080000, 0x7afa6adb, 4 | BRF_SND },           //  6 Oki #0 samples

	{ "m27c160.8.bin",	0x200000, 0xa509bc36, 5 | BRF_GRA },           //  7 Sprites

	{ "m27c160.10.bin",	0x200000, 0x739b0cb0, 6 | BRF_GRA },           //  8 Background Tiles

	{ "tms27c010_7.bin",	0x020000, 0x4cb84384, 7 | BRF_GRA },           //  9 Sprite Lookup Table
	{ "tms27c010_6.bin",	0x020000, 0x0c9d197a, 7 | BRF_GRA },           // 10

	{ "mx27c512.9.bin",	0x010000, 0x0da8db45, 8 | BRF_OPT },           // 11 unknown
};

STD_ROM_PICK(rolcrush)
STD_ROM_FN(rolcrush)

static INT32 RolcrushInit()
{
	return DrvInit(RolcrushRomLoad);
}

struct BurnDriver BurnDrvRolcrush = {
	"rolcrush", NULL, NULL, NULL, "1999",
	"Rolling Crush (version 1.07.E - 1999/02/11, Trust license)\0", NULL, "SemiCom / Exit (Trust license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, rolcrushRomInfo, rolcrushRomName, NULL, NULL, CommonInputInfo, RolcrushDIPInfo,
	RolcrushInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	304, 224, 4, 3
};


// Rolling Crush (version 1.03.E - 1999/01/29)

static struct BurnRomInfo rolcrushaRomDesc[] = {
	{ "2",			0x040000, 0x7b291ba9, 1 | BRF_PRG | BRF_ESS }, //  0 68ec020 Code
	{ "4",			0x040000, 0xb6afbc05, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "1",			0x040000, 0xef23ccf3, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "3",			0x040000, 0xecb2f9da, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "87c52.mcu",		0x010000, 0x00000000, 2 | BRF_NODUMP },        //  4 MCU code

	{ "protdata.bin",	0x000745, 0x06b8a880, 3 | BRF_GRA },           //  5 Protection data

	{ "5",			0x080000, 0x7afa6adb, 4 | BRF_SND },           //  6 Oki #0 samples

	{ "8",			0x200000, 0x01446191, 5 | BRF_GRA },           //  7 Sprites

	{ "10",			0x200000, 0x8cb75392, 6 | BRF_GRA },           //  8 Background Tiles

	{ "7",			0x020000, 0x23d641e4, 7 | BRF_GRA },           //  9 Sprite Lookup Table
	{ "6",			0x020000, 0x5934dac9, 7 | BRF_GRA },           // 10

	{ "9",			0x010000, 0x0da8db45, 8 | BRF_OPT },           // 11 unknown
};

STD_ROM_PICK(rolcrusha)
STD_ROM_FN(rolcrusha)

struct BurnDriver BurnDrvRolcrusha = {
	"rolcrusha", "rolcrush", NULL, NULL, "1999",
	"Rolling Crush (version 1.03.E - 1999/01/29)\0", NULL, "SemiCom / Exit", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, rolcrushaRomInfo, rolcrushaRomName, NULL, NULL, CommonInputInfo, RolcrushDIPInfo,
	RolcrushInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	304, 224, 4, 3
};


// Gaia - The Last Choice of Earth

static struct BurnRomInfo gaialastRomDesc[] = {
	{ "2",			0x040000, 0x549e594a, 1 | BRF_PRG | BRF_ESS }, //  0 68ec020 Code
	{ "4",			0x040000, 0x10cc2dee, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3",			0x040000, 0xa8e845d8, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "5",			0x040000, 0xc55f6f11, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "87c52.mcu",		0x010000, 0x00000000, 2 | BRF_NODUMP },        //  4 MCU code

	{ "protdata.bin",	0x0006c9, 0xd3403b7b, 3 | BRF_GRA },           //  Protection data

	{ "1",			0x080000, 0x2dbad410, 4 | BRF_SND },           //  6 Oki #0 samples

	{ "10",			0x200000, 0x5822ef93, 5 | BRF_GRA },           //  7 Sprites
	{ "11",			0x200000, 0xf4f5770d, 5 | BRF_GRA },           //  8
	{ "12",			0x200000, 0xa1f04571, 5 | BRF_GRA },           //  9

	{ "8",			0x200000, 0x32d16985, 6 | BRF_GRA },           // 10 Background Tiles

	{ "6",			0x020000, 0x5c82feed, 7 | BRF_GRA },           // 11 Sprite Lookup Table
	{ "7",			0x020000, 0x9d7f04ae, 7 | BRF_GRA },           // 12

	{ "9",			0x010000, 0x0da8db45, 8 | BRF_OPT },           // 13 unknown
};

STD_ROM_PICK(gaialast)
STD_ROM_FN(gaialast)

static INT32 GaialastInit()
{
	return DrvInit(GaialastRomLoad);
}

struct BurnDriver BurnDrvGaialast = {
	"gaialast", NULL, NULL, NULL, "1999",
	"Gaia - The Last Choice of Earth\0", NULL, "SemiCom / XESS", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, gaialastRomInfo, gaialastRomName, NULL, NULL, CommonInputInfo, GaialastDIPInfo,
	GaialastInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	304, 224, 4, 3
};


// Dream World

static struct BurnRomInfo dreamwldRomDesc[] = {
	{ "1.bin",		0x040000, 0x35c94ee5, 1 | BRF_PRG | BRF_ESS }, //  0 68ec020 Code
	{ "2.bin",		0x040000, 0x5409e7fc, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.bin",		0x040000, 0xe8f7ae78, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.bin",		0x040000, 0x3ef5d51b, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "87c52.mcu",		0x010000, 0x00000000, 2 | BRF_NODUMP },        //  4 MCU code

	{ "protdata.bin",	0x0006c9, 0xf284b2fd, 3 | BRF_PRG | BRF_ESS }, //  5 Protection data

	{ "5.bin",		0x080000, 0x9689570a, 4 | BRF_SND },           //  6 Oki #0 samples

	{ "6.bin",		0x080000, 0xc8b91f30, 5 | BRF_SND },           //  7 Oki #1 samples

	{ "9.bin",		0x200000, 0xfa84e3af, 6 | BRF_GRA },           //  8 Sprites

	{ "10.bin",		0x200000, 0x3553e4f5, 7 | BRF_GRA },           //  9 Background Tiles

	{ "8.bin",		0x020000, 0x8d570df6, 8 | BRF_GRA },           // 10 Sprite Lookup Table
	{ "7.bin",		0x020000, 0xa68bf35f, 8 | BRF_GRA },           // 11

	{ "11.bin",		0x010000, 0x0da8db45, 9 | BRF_OPT },           // 12 Unknown
};

STD_ROM_PICK(dreamwld)
STD_ROM_FN(dreamwld)

static INT32 DreamwldInit()
{
	return DrvInit(DreamwldRomLoad);
}

struct BurnDriver BurnDrvDreamwld = {
	"dreamwld", NULL, NULL, NULL, "2000",
	"Dream World\0", NULL, "SemiCom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MAZE, 0,
	NULL, dreamwldRomInfo, dreamwldRomName, NULL, NULL, CommonInputInfo, DreamwldDIPInfo,
	DreamwldInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	304, 224, 4, 3
};
