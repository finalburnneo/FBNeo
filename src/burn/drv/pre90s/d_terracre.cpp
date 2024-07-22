// FB Neo Terra Cresta driver module
// Based on MAME driver by Carlos A. Lozano

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "burn_pal.h"
#include "burn_ym3526.h"
#include "burn_ym2203.h"
#include "dac.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvZ80ROM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvProtROM;
static UINT8 *DrvColPROM;
static UINT8 *DrvSprPalBank;
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprBuf;
static UINT8 *DrvBgRAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvGfxROM[3];

static UINT16 scrollx;
static UINT16 scrolly;
static UINT16 flipscreen;
static UINT8  soundlatch;
static UINT8  prot_command;
static UINT16 prot_data_address;
static UINT16 prot_subtract_address;

static UINT8 has_ym2203 = 0;
static UINT8 is_horekid = 0;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[3];
static UINT8 DrvInput[3];
static UINT8 DrvReset;

static INT32 nCyclesExtra;

static struct BurnInputInfo TerracreInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 4,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Terracre)

static struct BurnInputInfo AmazonInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 4,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Amazon)

static struct BurnDIPInfo TerracreDIPList[]=
{
	DIP_OFFSET(0x12)
	{0x00, 0xff, 0xff, 0x20, NULL						},
	{0x01, 0xff, 0xff, 0xdf, NULL						},
	{0x02, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x00, 0x01, 0x20, 0x20, "Off"						},
	{0x00, 0x01, 0x20, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x01, 0x01, 0x03, 0x03, "3"						},
	{0x01, 0x01, 0x03, 0x02, "4"						},
	{0x01, 0x01, 0x03, 0x01, "5"						},
	{0x01, 0x01, 0x03, 0x00, "6"						},

	{0   , 0xfe, 0   ,    4, "Bonus Life"				},
	{0x01, 0x01, 0x0c, 0x0c, "20k then every 60k"		},
	{0x01, 0x01, 0x0c, 0x08, "30k then every 70k"		},
	{0x01, 0x01, 0x0c, 0x04, "40k then every 80k"		},
	{0x01, 0x01, 0x0c, 0x00, "50k then every 90k"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x01, 0x01, 0x10, 0x00, "Off"						},
	{0x01, 0x01, 0x10, 0x10, "On"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x01, 0x01, 0x20, 0x00, "Upright"					},
	{0x01, 0x01, 0x20, 0x20, "Cocktail"					},

	{0   , 0xfe, 0   ,    4, "Coin A"					},
	{0x02, 0x01, 0x03, 0x01, "2 Coins 1 Credits"		},
	{0x02, 0x01, 0x03, 0x03, "1 Coin  1 Credits"		},
	{0x02, 0x01, 0x03, 0x02, "1 Coin  2 Credits"		},
	{0x02, 0x01, 0x03, 0x00, "Free Play"				},

	{0   , 0xfe, 0   ,    4, "Coin B"					},
	{0x02, 0x01, 0x0c, 0x00, "3 Coins 1 Credits"		},
	{0x02, 0x01, 0x0c, 0x04, "2 Coins 3 Credits"		},
	{0x02, 0x01, 0x0c, 0x0c, "1 Coin  3 Credits"		},
	{0x02, 0x01, 0x0c, 0x08, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    2, "Difficulty"				},
	{0x02, 0x01, 0x10, 0x10, "Easy"						},
	{0x02, 0x01, 0x10, 0x00, "Hard"						},

	{0   , 0xfe, 0   ,    2, "Flip Screen"				},
	{0x02, 0x01, 0x20, 0x20, "Off"						},
	{0x02, 0x01, 0x20, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Complete Invulnerability"	},
	{0x02, 0x01, 0x40, 0x40, "Off"						},
	{0x02, 0x01, 0x40, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Base Ship Invulnerability"},
	{0x02, 0x01, 0x80, 0x80, "Off"						},
	{0x02, 0x01, 0x80, 0x00, "On"						},
};

STDDIPINFO(Terracre)

static struct BurnDIPInfo AmazonDIPList[]=
{
	DIP_OFFSET(0x12)
	{0x00, 0xff, 0xff, 0x20, NULL						},
	{0x01, 0xff, 0xff, 0xdf, NULL						},
	{0x02, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x00, 0x01, 0x20, 0x20, "Off"						},
	{0x00, 0x01, 0x20, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x01, 0x01, 0x03, 0x03, "3"						},
	{0x01, 0x01, 0x03, 0x02, "4"						},
	{0x01, 0x01, 0x03, 0x01, "5"						},
	{0x01, 0x01, 0x03, 0x00, "6"						},

	{0   , 0xfe, 0   ,    4, "Bonus Life"				},
	{0x01, 0x01, 0x0c, 0x0c, "20k then every 40k"		},
	{0x01, 0x01, 0x0c, 0x08, "50k then every 40k"		},
	{0x01, 0x01, 0x0c, 0x04, "20k then every 70k"		},
	{0x01, 0x01, 0x0c, 0x00, "50k then every 70k"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x01, 0x01, 0x10, 0x00, "Off"						},
	{0x01, 0x01, 0x10, 0x10, "On"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x01, 0x01, 0x20, 0x00, "Upright"					},
	{0x01, 0x01, 0x20, 0x20, "Cocktail"					},

	{0   , 0xfe, 0   ,    4, "Coin A"					},
	{0x02, 0x01, 0x03, 0x01, "2 Coins 1 Credits"		},
	{0x02, 0x01, 0x03, 0x03, "1 Coin  1 Credits"		},
	{0x02, 0x01, 0x03, 0x02, "1 Coin  2 Credits"		},
	{0x02, 0x01, 0x03, 0x00, "Free Play"				},

	{0   , 0xfe, 0   ,    4, "Coin B"					},
	{0x02, 0x01, 0x0c, 0x00, "3 Coins 1 Credits"		},
	{0x02, 0x01, 0x0c, 0x08, "1 Coin  1 Credits"		},
	{0x02, 0x01, 0x0c, 0x04, "2 Coins 3 Credits"		},
	{0x02, 0x01, 0x0c, 0x0c, "1 Coin  3 Credits"		},

	{0   , 0xfe, 0   ,    2, "Difficulty"				},
	{0x02, 0x01, 0x10, 0x10, "Easy"						},
	{0x02, 0x01, 0x10, 0x00, "Hard"						},

	{0   , 0xfe, 0   ,    2, "Flip Screen"				},
	{0x02, 0x01, 0x20, 0x20, "Off"						},
	{0x02, 0x01, 0x20, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Level"					},
	{0x02, 0x01, 0x40, 0x40, "Low"						},
	{0x02, 0x01, 0x40, 0x00, "High"						},

	{0   , 0xfe, 0   ,    2, "Sprite Test"				},
	{0x02, 0x01, 0x80, 0x80, "Off"						},
	{0x02, 0x01, 0x80, 0x00, "On"						},
};

STDDIPINFO(Amazon)

static struct BurnDIPInfo HorekidDIPList[]=
{
	DIP_OFFSET(0x12)
	{0x00, 0xff, 0xff, 0x20, NULL						},
	{0x01, 0xff, 0xff, 0xcf, NULL						},
	{0x02, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x00, 0x01, 0x20, 0x20, "Off"						},
	{0x00, 0x01, 0x20, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x01, 0x01, 0x03, 0x03, "3"						},
	{0x01, 0x01, 0x03, 0x02, "4"						},
	{0x01, 0x01, 0x03, 0x01, "5"						},
	{0x01, 0x01, 0x03, 0x00, "6"						},

	{0   , 0xfe, 0   ,    4, "Bonus Life"				},
	{0x01, 0x01, 0x0c, 0x0c, "20k then every 60k"		},
	{0x01, 0x01, 0x0c, 0x08, "50k then every 60k"		},
	{0x01, 0x01, 0x0c, 0x04, "20k then every 90k"		},
	{0x01, 0x01, 0x0c, 0x00, "50k then every 90k"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x01, 0x01, 0x10, 0x10, "Off"						},
	{0x01, 0x01, 0x10, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x01, 0x01, 0x20, 0x00, "Upright"					},
	{0x01, 0x01, 0x20, 0x20, "Cocktail"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x01, 0x01, 0xc0, 0xc0, "Easy"						},
	{0x01, 0x01, 0xc0, 0x80, "Normal"					},
	{0x01, 0x01, 0xc0, 0x40, "Hard"						},
	{0x01, 0x01, 0xc0, 0x00, "Hardest"					},

	{0   , 0xfe, 0   ,    4, "Coin A"					},
	{0x02, 0x01, 0x03, 0x01, "2 Coins 1 Credits"		},
	{0x02, 0x01, 0x03, 0x03, "1 Coin  1 Credits"		},
	{0x02, 0x01, 0x03, 0x02, "1 Coin  2 Credits"		},
	{0x02, 0x01, 0x03, 0x00, "Free Play"				},

	{0   , 0xfe, 0   ,    4, "Coin B"					},
	{0x02, 0x01, 0x0c, 0x04, "2 Coins 1 Credits"		},
	{0x02, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"		},
	{0x02, 0x01, 0x0c, 0x00, "2 Coins 3 Credits"		},
	{0x02, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"				},
	{0x02, 0x01, 0x20, 0x20, "Off"						},
	{0x02, 0x01, 0x20, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Debug Mode"				},
	{0x02, 0x01, 0xc0, 0xc0, "Off"						},
	{0x02, 0x01, 0xc0, 0x80, "On"						},
};

STDDIPINFO(Horekid)

static UINT16 __fastcall TerracreCommonReadWord(UINT32 address)
{
	switch (address)
	{
		case 0x0:
			return DrvInput[0];

		case 0x2:
			return DrvInput[1];

		case 0x4:
			return ((DrvInput[2] & ~0x20) | (DrvDips[0] & 0x20)) << 8;

		case 0x6:
			return (DrvDips[2] << 8) | DrvDips[1];
	}

	return 0;
}

static void __fastcall TerracreCommonWriteWord(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x0:
			flipscreen = data & 0x04;
		return;

		case 0x2:
			scrollx = data;
		return;

		case 0x4:
			scrolly = data;
		return;

		case 0xa: // ???
		return;

		case 0xc:
			soundlatch = (data << 1) | 1;
		return;

		case 0xe: // ???
			return;
	}
}

static UINT16 __fastcall main_read_word(UINT32 address)
{
	if ((address & 0xfffff0) == 0x24000) { // terracre
		return TerracreCommonReadWord(address & 0xf);
	}

	if ((address & 0xfffff0) == 0x44000) { // amazon, horekid
		return TerracreCommonReadWord((address ^ (is_horekid ? 0x06 : 0x00)) & 0xf);
	}

	return 0;
}

static void __fastcall main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfffff0) == 0x26000) { // terracre
		TerracreCommonWriteWord(address & 0xf, data);
	}

	if ((address & 0xfffff0) == 0x46000) { // amazon, horekid
		TerracreCommonWriteWord(address & 0xf, data);
	}
}

static UINT8 __fastcall amazon_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x070001:
		{
			if (prot_command == 0x37)
			{
				return DrvProtROM[prot_data_address & 0x1fff] - ((0x43 - DrvProtROM[prot_subtract_address & 0x1fff]) & 0xff);
			}

			return 0;
		}
	}

	return 0;
}

static void __fastcall amazon_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x070001:
		{
			switch (prot_command)
			{
				case 0x33:
				case 0x34:
					prot_data_address = (data << ((prot_command & 1) * 8)) | (prot_data_address & (0xff << ((~prot_command & 1) * 8)));
				break;

				case 0x35:
				case 0x36:
					prot_subtract_address = (data << ((prot_command & 1) * 8)) | (prot_subtract_address & (0xff << ((~prot_command & 1) * 8)));
				break;
			}

			return;
		}

		case 0x070003:
			prot_command = data;
		return;
	}
}

static UINT8 __fastcall sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x04:
			soundlatch = 0;
			return 0;

		case 0x06:
			return soundlatch;
	}

	return 0;
}

static void __fastcall sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
			if (has_ym2203) {
				BurnYM2203Write(0, port & 1, data);
			} else {
				BurnYM3526Write(port & 1, data);
			}
		return;

		case 0x02:
		case 0x03:
			DACSignedWrite(port & 1, data);
		return;
	}
}

static tilemap_callback( tx )
{
	UINT16 *ram = (UINT16*)DrvFgRAM;
	INT32 attr = ram[offs];

	TILE_SET_INFO(0, attr, 0, 0);
}

static tilemap_callback( bg )
{
	UINT16 *ram = (UINT16*)DrvBgRAM;
	INT32 attr = ram[offs];

	TILE_SET_INFO(1, attr, attr >> 11, 0);
}

static tilemap_callback( bgX )
{
	UINT16 *ram = (UINT16*)(Drv68KROM + 0x18000);
	INT32 attr = ram[offs];

	TILE_SET_INFO(1, attr, attr >> 11, 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	ZetOpen(0);
	ZetReset();
	(has_ym2203) ? BurnYM2203Reset() : BurnYM3526Reset();
	ZetClose();

	DACReset();

	scrollx = 0;
	scrolly = 0;
	flipscreen = 0;
	soundlatch = 0;
	prot_command = 0;
	prot_data_address = 0;
	prot_subtract_address = 0;

	nCyclesExtra = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM			= Next; Next += 0x020000;
	DrvZ80ROM			= Next; Next += 0x00c000;
	DrvColPROM			= Next; Next += 0x000400;
	DrvSprPalBank		= Next; Next += 0x000100;
	DrvProtROM			= Next; Next += 0x002000;

	DrvGfxROM[0]		= Next; Next += 0x004000;
	DrvGfxROM[1]		= Next; Next += 0x040000;
	DrvGfxROM[2]		= Next; Next += 0x040000;

	AllRam				= Next;

	Drv68KRAM           = Next; Next += 0x001000;
	DrvSprRAM           = Next; Next += 0x002000;
	DrvSprBuf     		= Next; Next += 0x000200;
	DrvBgRAM          	= Next; Next += 0x001000;
	DrvFgRAM          	= Next; Next += 0x001000;
	DrvZ80RAM			= Next; Next += 0x001000;

	RamEnd				= Next;

	BurnPalette			= (UINT32*)Next; Next += 0x1110 * sizeof(UINT32);

	MemEnd				= Next;

	return 0;
}

static INT32 DrvLoadRoms()
{
	char* pRomName;
	struct BurnRomInfo ri;
	UINT8 *pLoad[9] = { 0, Drv68KROM, DrvZ80ROM, DrvGfxROM[0], DrvGfxROM[1], DrvGfxROM[2], DrvColPROM, DrvSprPalBank, DrvProtROM };

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {

		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & 0xf) == 1 || (ri.nType & 0xf) == 5) {
			if (BurnLoadRom(pLoad[ri.nType & 0xf] + 0, i + 1, 2)) return 1;
			if (BurnLoadRom(pLoad[ri.nType & 0xf] + 1, i + 0, 2)) return 1;
			pLoad[ri.nType & 0xf] += ri.nLen * 2;
			i++;
			continue;
		}

		if ((ri.nType & 0xf) >= 2) {
			if (BurnLoadRom(pLoad[ri.nType & 0xf], i + 0, 1)) return 1;
			pLoad[ri.nType & 0xf] += ri.nLen;
			continue;
		}
	}

	BurnNibbleExpand(DrvGfxROM[0], NULL, 0x02000, 1, 0);
	BurnNibbleExpand(DrvGfxROM[1], NULL, 0x20000, 1, 0);
	BurnByteswap(DrvGfxROM[2], 0x20000);
	BurnNibbleExpand(DrvGfxROM[2], NULL, 0x20000, 1, 0);

	return 0;
}

static void DrvSoundInit()
{
	ZetInit(0);
	ZetOpen(0);
	ZetSetInHandler(sound_read_port);
	ZetSetOutHandler(sound_write_port);
	ZetMapMemory(DrvZ80ROM, 0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM, 0xc000, 0xcfff, MAP_RAM);
	ZetClose();

	if (has_ym2203) {
		BurnYM2203Init(1, 4000000, NULL, 0);
		BurnTimerAttachZet(4000000);
		BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE, 0.40, BURN_SND_ROUTE_BOTH);
		BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.20, BURN_SND_ROUTE_BOTH);
		BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.20, BURN_SND_ROUTE_BOTH);
		BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.20, BURN_SND_ROUTE_BOTH);
	} else {
		BurnYM3526Init(4000000, NULL, 0);
		BurnTimerAttach(&ZetConfig, 4000000);
		BurnYM3526SetRoute(BURN_SND_YM3526_ROUTE, 1.00, BURN_SND_ROUTE_BOTH);
	}

	DACInit(0, 0, 1, ZetTotalCycles, 4000000);
	DACInit(1, 0, 1, ZetTotalCycles, 4000000);
	DACSetRoute(0, 0.40, BURN_SND_ROUTE_BOTH);
	DACSetRoute(1, 0.40, BURN_SND_ROUTE_BOTH);
}

static INT32 TerracreInit()
{
	BurnAllocMemIndex();

	if (DrvLoadRoms()) return 1;

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM, 		0x000000, 0x01ffff, MAP_ROM);
	SekMapMemory(DrvSprRAM, 		0x020000, 0x021fff, MAP_RAM);
	SekMapMemory(DrvBgRAM, 			0x022000, 0x022fff, MAP_RAM);
	SekMapMemory(DrvFgRAM, 			0x028000, 0x0287ff, MAP_RAM);
	SekSetReadWordHandler(0, 		main_read_word);
	SekSetWriteWordHandler(0, 		main_write_word);
	SekClose();

	DrvSoundInit();

	GenericTilesInit();
	GenericTilesSetGfx(0, DrvGfxROM[0], 4,  8,  8, 0x04000, 0x1100, 0x00);
	GenericTilesSetGfx(1, DrvGfxROM[1], 4, 16, 16, 0x20000, 0x1000, 0x0f);
	GenericTilesSetGfx(2, DrvGfxROM[2], 4, 16, 16, 0x20000, 0x0000, 0xff);
	GenericTilemapInit(0, TILEMAP_SCAN_COLS, tx_map_callback,  8,  8, 64, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_COLS, bg_map_callback, 16, 16, 64, 32);
	GenericTilemapInit(2, TILEMAP_SCAN_COLS, bgX_map_callback, 16, 16, 128, 64);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);
	GenericTilemapSetTransparent(0, 0xf);

	DrvDoReset();

	return 0;
}

static INT32 AmazonInit()
{
	BurnAllocMemIndex();

	if (DrvLoadRoms()) return 1;

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM, 		0x000000, 0x01ffff, MAP_ROM);
	SekMapMemory(DrvSprRAM, 		0x040000, 0x040fff, MAP_RAM);
	SekMapMemory(DrvBgRAM,			0x042000, 0x042fff, MAP_RAM);
	SekMapMemory(DrvFgRAM, 			0x050000, 0x050fff, MAP_RAM);
	SekSetReadWordHandler(0, 		main_read_word);
	SekSetWriteWordHandler(0, 		main_write_word);
	SekSetReadByteHandler(0, 		amazon_read_byte);
	SekSetWriteByteHandler(0, 		amazon_write_byte);
	SekClose();

	DrvSoundInit();

	GenericTilesInit();
	GenericTilesSetGfx(0, DrvGfxROM[0], 4,  8,  8, 0x04000, 0x1100, 0x00);
	GenericTilesSetGfx(1, DrvGfxROM[1], 4, 16, 16, 0x40000, 0x1000, 0x0f);
	GenericTilesSetGfx(2, DrvGfxROM[2], 4, 16, 16, (is_horekid) ? 0x40000 : 0x20000, 0x0000, 0xff);
	GenericTilemapInit(0, TILEMAP_SCAN_COLS, tx_map_callback,  8,  8, 64, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_COLS, bg_map_callback, 16, 16, 64, 32);
	GenericTilemapInit(2, TILEMAP_SCAN_COLS, bgX_map_callback, 16, 16, 128, 64);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);
	GenericTilemapSetTransparent(0, 0xf);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	SekExit();
	ZetExit();

	(has_ym2203) ? BurnYM2203Exit() : BurnYM3526Exit();
	DACExit();

	GenericTilesExit();

	BurnFreeMemIndex();

	has_ym2203 = 0;
	is_horekid = 0;

	return 0;
}

static void DrvCalcPalette()
{
	UINT32 PalLookup[0x100];

	for (INT32 i = 0; i < 0x100; i++) {
		PalLookup[i] = BurnHighCol(pal4bit(DrvColPROM[i + 0x000]), pal4bit(DrvColPROM[i + 0x100]), pal4bit(DrvColPROM[i + 0x200]), 0);
	}

	for (INT32 i = 0; i < 0x10; i++) {
		BurnPalette[0x1100+i] = PalLookup[i];
	}

	for (INT32 i = 0; i < 0x100; i++) {
		BurnPalette[0x1000 + i] = PalLookup[0xc0 | (i & 0x0f) | ((i >> ((i & 0x08) ? 2 : 0)) & 0x30)];
	}

	for (INT32 i = 0; i < 0x1000; i++) {
		UINT8 ctabentry;
		INT32 i_swapped = ((i & 0x0f) << 8) | ((i & 0xff0) >> 4);

		if (i & 0x80) {
			ctabentry = 0x80 | ((i & 0x0c) << 2) | (DrvColPROM[0x300 + (i >> 4)] & 0x0f);
		} else {
			ctabentry = 0x80 | ((i & 0x03) << 4) | (DrvColPROM[0x300 + (i >> 4)] & 0x0f);
		}

		BurnPalette[i_swapped] = PalLookup[ctabentry];
	}
}

static void draw_sprites()
{
	UINT16 *spr16 = (UINT16*)DrvSprBuf;
	INT32 TransparentPen = (is_horekid) ? 0x0f : 0;

	for (INT32 i = 0; i < 0x200; i += 8, spr16 += 4)
	{
		INT32 code = spr16[1] & 0xff;
		INT32 attr = spr16[2];
		INT32 flip_x = attr & 0x04;
		INT32 flip_y = attr & 0x08;
		INT32 color = (attr & 0xf0) >> 4;
		INT32 sx = (spr16[3] & 0xff) - 0x80 + 256 * (attr & 1);
		INT32 sy = 240 - (spr16[0] & 0xff);

		if (is_horekid)
		{
			code  |= ((attr & 0x02) << 8) | ((attr & 0x10) << 4);
			color = (color & 0x0e) + ((DrvSprPalBank[((code & 0xfc) >> 1) | ((attr & 0x02) << 6) | ((attr & 0x10) >> 4)] & 0x0f) << 4);
		}
		else
		{
			code  |= ((attr & 0x02) << 7);
			color += ((DrvSprPalBank[(code >> 1) & 0xff] & 0x0f) << 4);
		}

		if (flipscreen)
		{
			sx = 240 - sx;
			sy = 240 - sy;
			flip_x = !flip_x;
			flip_y = !flip_y;
		}

		DrawGfxMaskTile(0, 2, code, sx, sy - 16, flip_x, flip_y, color, TransparentPen);
	}
}

static INT32 DrvDraw()
{
	if (BurnRecalc) {
		DrvCalcPalette();
		BurnRecalc = 0;
	}

	BurnTransferClear();

	GenericTilemapSetScrollX(1, scrollx);
	GenericTilemapSetScrollY(1, scrolly);

	if (nBurnLayer & 0x01 && !(scrollx & 0x2000)) GenericTilemapDraw(1, 0, 0);

	if (nSpriteEnable & 0x01) draw_sprites();

	if (nBurnLayer & 0x02 && !(scrollx & 0x1000)) GenericTilemapDraw(0, 0, 0);

	if (~nBurnLayer & 0x04) GenericTilemapDumpToBitmap();

	BurnTransferCopy(BurnPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		DrvInput[0] = DrvInput[1] = 0xff;
		DrvInput[2] = 0xdf;

		for (INT32 i = 0; i < 8; i++) {
			DrvInput[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInput[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInput[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nCyclesTotal[2] = { 8000000 / 60, 4000000 / 60 };
	INT32 nCyclesDone[2] = { nCyclesExtra, 0 };
	INT32 nInterleave = 130;

	SekNewFrame();
	ZetNewFrame();

	for (INT32 i = 0; i < nInterleave; i++)
	{
		SekOpen(0);
		CPU_RUN(0, Sek);
		if (i == (nInterleave - 1)) SekSetIRQLine(1, CPU_IRQSTATUS_AUTO);
		SekClose();

		ZetOpen(0);
		CPU_RUN_TIMER(0);
		ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();
	}

	nCyclesExtra = nCyclesDone - nCyclesTotal;

	if (pBurnSoundOut) {
		if (has_ym2203) {
			BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
		} else {
			BurnYM3526Update(pBurnSoundOut, nBurnSoundLen);
		}
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	memcpy (DrvSprBuf, DrvSprRAM, 0x00200);

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	if (pnMin != NULL) {
		*pnMin = 0x029719;
	}

	if (nAction & ACB_MEMORY_RAM) {
		ScanVar(AllRam, RamEnd-AllRam, "All Ram");
	}

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);
		ZetScan(nAction);

		if (has_ym2203) {
			BurnYM2203Scan(nAction, pnMin);
		} else {
			BurnYM3526Scan(nAction, pnMin);
		}

		DACScan(nAction, pnMin);

		SCAN_VAR(scrollx);
		SCAN_VAR(scrolly);
		SCAN_VAR(flipscreen);
		SCAN_VAR(soundlatch);
		SCAN_VAR(prot_command);
		SCAN_VAR(prot_data_address);
		SCAN_VAR(prot_subtract_address);

		SCAN_VAR(nCyclesExtra);
	}

	return 0;
}


// Terra Cresta (YM3526 set 1)

static struct BurnRomInfo DrvRomDesc[] = {
	{ "3.4d",      		0x08000, 0xcb36240e, 1 | BRF_ESS | BRF_PRG },	 //  0 68000 Program Code
	{ "1.4b",      		0x08000, 0x60932770, 1 | BRF_ESS | BRF_PRG },	 //  1
	{ "4.6d",      		0x08000, 0x19387586, 1 | BRF_ESS | BRF_PRG },	 //  2
	{ "2.6b",      		0x08000, 0x539352f2, 1 | BRF_ESS | BRF_PRG },	 //  3

	{ "11.15b",    		0x04000, 0x604c3b11, 2 | BRF_ESS | BRF_PRG },	 //  4 Z80 Program Code
	{ "12.17b",    		0x04000, 0xaffc898d, 2 | BRF_ESS | BRF_PRG },	 //  5
	{ "13.18b",    		0x04000, 0x302dc0ab, 2 | BRF_ESS | BRF_PRG },	 //  6

	{ "14.16g",    		0x02000, 0x591a3804, 3 | BRF_GRA },				 //  7 Chars

	{ "5.15f",     		0x08000, 0x984a597f, 4 | BRF_GRA },				 //  8 Tiles
	{ "6.17f",     		0x08000, 0x30e297ff, 4 | BRF_GRA },				 //  9

	{ "7.6e",      		0x04000, 0xbcf7740b, 5 | BRF_GRA },				 // 10 Sprites
	{ "9.6g",      		0x04000, 0x4a9ec3e6, 5 | BRF_GRA },				 // 11
	{ "8.7e",      		0x04000, 0xa70b565c, 5 | BRF_GRA },				 // 12
	{ "10.7g",     		0x04000, 0x450749fc, 5 | BRF_GRA },				 // 13

	{ "3.10f",     		0x00100, 0xce07c544, 6 | BRF_GRA },				 // 14 PROMs
	{ "2.11f",     		0x00100, 0x566d323a, 6 | BRF_GRA },				 // 15
	{ "1.12f",     		0x00100, 0x7ea63946, 6 | BRF_GRA },				 // 16
	{ "4.2g",      		0x00100, 0x08609bad, 6 | BRF_GRA },				 // 17

	{ "5.4e",      		0x00100, 0x2c43991f, 7 | BRF_GRA },				 // 18 Sprite Palette Bank

	{ "11e",			0x00104, 0x00000000, 0 | BRF_OPT | BRF_NODUMP }, // 19 plds
	{ "12a",			0x00104, 0x00000000, 0 | BRF_OPT | BRF_NODUMP }, // 20
	{ "12f",			0x00104, 0x00000000, 0 | BRF_OPT | BRF_NODUMP }, // 21
};

STD_ROM_PICK(Drv)
STD_ROM_FN(Drv)

struct BurnDriver BurnDrvTerracre = {
	"terracre", NULL, NULL, NULL, "1985",
	"Terra Cresta (YM3526 set 1)\0", NULL, "Nichibutsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, DrvRomInfo, DrvRomName, NULL, NULL, NULL, NULL, TerracreInputInfo, TerracreDIPInfo,
	TerracreInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&BurnRecalc, 0x1110, 224, 256, 3, 4
};


// Terra Cresta (YM3526 set 2)

static struct BurnRomInfo DrvoRomDesc[] = {
	// older pcb TC-1A & TC-2A and oldest ROM-set with glitches in the music when the ship appears
	{ "5.4d",     		0x04000, 0x8119f06e, 1 | BRF_ESS | BRF_PRG },	 //  0 68000 Program Code
	{ "1.4b",     		0x04000, 0x76f17479, 1 | BRF_ESS | BRF_PRG },	 //  1
	{ "6.6d",     		0x04000, 0xca4852f6, 1 | BRF_ESS | BRF_PRG },	 //  2
	{ "2.6b",     		0x04000, 0xba4b5822, 1 | BRF_ESS | BRF_PRG },	 //  3
	{ "7.7d",     		0x04000, 0x029d59d9, 1 | BRF_ESS | BRF_PRG },	 //  4
	{ "3.7b",     		0x04000, 0xd0771bba, 1 | BRF_ESS | BRF_PRG },	 //  5
	{ "8.9d",     		0x04000, 0x5a672942, 1 | BRF_ESS | BRF_PRG },	 //  6
	{ "4.9b",     		0x04000, 0x69227b56, 1 | BRF_ESS | BRF_PRG },	 //  7

	{ "11.15b",    		0x04000, 0x604c3b11, 2 | BRF_ESS | BRF_PRG },	 //  8 Z80 Program Code
	{ "12.17b",    		0x04000, 0xaffc898d, 2 | BRF_ESS | BRF_PRG },	 //  9
	{ "13.18b",    		0x04000, 0x302dc0ab, 2 | BRF_ESS | BRF_PRG },	 // 10

	{ "18.16g",    		0x02000, 0x591a3804, 3 | BRF_GRA },				 // 11 Chars

	{ "9.15f",    		0x08000, 0x984a597f, 4 | BRF_GRA },				 // 12 Tiles
	{ "10.17f",    		0x08000, 0x30e297ff, 4 | BRF_GRA },				 // 13

	{ "14.6e",     		0x04000, 0xbcf7740b, 5 | BRF_GRA },				 // 14 Sprites
	{ "16.6g",     		0x04000, 0x4a9ec3e6, 5 | BRF_GRA },				 // 15
	{ "15.7e",     		0x04000, 0xa70b565c, 5 | BRF_GRA },				 // 16
	{ "17.7g",     		0x04000, 0x450749fc, 5 | BRF_GRA },				 // 17

	{ "3.10f",  		0x00100, 0xce07c544, 6 | BRF_GRA },				 // 18 PROMs
	{ "2.11f",  		0x00100, 0x566d323a, 6 | BRF_GRA },				 // 19
	{ "1.12f",  		0x00100, 0x7ea63946, 6 | BRF_GRA },				 // 20
	{ "4.2g",   		0x00100, 0x08609bad, 6 | BRF_GRA },				 // 21

	{ "5.4e",   		0x00100, 0x2c43991f, 7 | BRF_GRA },				 // 22 Sprite Palette Bank

	{ "cp1408.11e",		0x00104, 0x00000000, 0 | BRF_OPT | BRF_NODUMP }, // 23 plds
	{ "tc1411.12a",		0x00104, 0x00000000, 0 | BRF_OPT | BRF_NODUMP }, // 24
	{ "tc1412.12f",		0x00104, 0x00000000, 0 | BRF_OPT | BRF_NODUMP }, // 25
};

STD_ROM_PICK(Drvo)
STD_ROM_FN(Drvo)

struct BurnDriver BurnDrvTerracreo = {
	"terracreo", "terracre", NULL, NULL, "1985",
	"Terra Cresta (YM3526 set 2)\0", NULL, "Nichibutsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, DrvoRomInfo, DrvoRomName, NULL, NULL, NULL, NULL, TerracreInputInfo, TerracreDIPInfo,
	TerracreInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&BurnRecalc, 0x1110, 224, 256, 3, 4
};


// Terra Cresta (YM3526 set 3)

// older pcb TC-1A & TC-2A and oldest ROM-set with glitches in the music when the ship appears
static struct BurnRomInfo DrvaRomDesc[] = {
	// older pcb TC-1A & TC-2A, the only difference is one sound rom
	{ "5.4d",     		0x04000, 0x8119f06e, 1 | BRF_ESS | BRF_PRG },	 //  0 68000 Program Code
	{ "1.4b",     		0x04000, 0x76f17479, 1 | BRF_ESS | BRF_PRG },	 //  1
	{ "6.6d",     		0x04000, 0xca4852f6, 1 | BRF_ESS | BRF_PRG },	 //  2
	{ "2.6b",     		0x04000, 0xba4b5822, 1 | BRF_ESS | BRF_PRG },	 //  3
	{ "7.7d",     		0x04000, 0x029d59d9, 1 | BRF_ESS | BRF_PRG },	 //  4
	{ "3.7b",     		0x04000, 0xd0771bba, 1 | BRF_ESS | BRF_PRG },	 //  5
	{ "8.9d",     		0x04000, 0x5a672942, 1 | BRF_ESS | BRF_PRG },	 //  6
	{ "4.9b",     		0x04000, 0x69227b56, 1 | BRF_ESS | BRF_PRG },	 //  7

	{ "11.15b",    		0x04000, 0x604c3b11, 2 | BRF_ESS | BRF_PRG },	 //  8 Z80 Program Code
	{ "12.17b",         0x04000, 0x9e9b3808, 2 | BRF_ESS | BRF_PRG },	 //  9
	{ "13.18b",    		0x04000, 0x302dc0ab, 2 | BRF_ESS | BRF_PRG },	 // 10

	{ "18.16g",    		0x02000, 0x591a3804, 3 | BRF_GRA },				 // 11 Chars

	{ "9.15f",    		0x08000, 0x984a597f, 4 | BRF_GRA },				 // 12 Tiles
	{ "10.17f",    		0x08000, 0x30e297ff, 4 | BRF_GRA },				 // 13

	{ "14.6e",     		0x04000, 0xbcf7740b, 5 | BRF_GRA },				 // 14 Sprites
	{ "16.6g",     		0x04000, 0x4a9ec3e6, 5 | BRF_GRA },				 // 15
	{ "15.7e",     		0x04000, 0xa70b565c, 5 | BRF_GRA },				 // 16
	{ "17.7g",     		0x04000, 0x450749fc, 5 | BRF_GRA },				 // 17

	{ "3.10f",  		0x00100, 0xce07c544, 6 | BRF_GRA },				 // 18 PROMs
	{ "2.11f",  		0x00100, 0x566d323a, 6 | BRF_GRA },				 // 19
	{ "1.12f",  		0x00100, 0x7ea63946, 6 | BRF_GRA },				 // 20
	{ "4.2g",   		0x00100, 0x08609bad, 6 | BRF_GRA },				 // 21

	{ "5.4e",   		0x00100, 0x2c43991f, 7 | BRF_GRA },				 // 22 Sprite Palette Bank

	{ "cp1408.11e",		0x00104, 0x00000000, 0 | BRF_OPT | BRF_NODUMP }, // 23 plds
	{ "tc1411.12a",		0x00104, 0x00000000, 0 | BRF_OPT | BRF_NODUMP }, // 24
	{ "tc1412.12f",		0x00104, 0x00000000, 0 | BRF_OPT | BRF_NODUMP }, // 25
};

STD_ROM_PICK(Drva)
STD_ROM_FN(Drva)

struct BurnDriver BurnDrvTerracrea = {
	"terracrea", "terracre", NULL, NULL, "1985",
	"Terra Cresta (YM3526 set 3)\0", NULL, "Nichibutsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, DrvaRomInfo, DrvaRomName, NULL, NULL, NULL, NULL, TerracreInputInfo, TerracreDIPInfo,
	TerracreInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&BurnRecalc, 0x1110, 224, 256, 3, 4
};


// Terra Cresta (YM2203)

static struct BurnRomInfo DrvnRomDesc[] = {
	// 'n' for OPN(YM2203), newer than YM3526 sets because it uses a daughterboard to convert the existing YM3526 socket to a YM2203
	{ "5.4d",     		0x04000, 0x8119f06e, 1 | BRF_ESS | BRF_PRG },	 //  0 68000 Program Code
	{ "1.4b",     		0x04000, 0x76f17479, 1 | BRF_ESS | BRF_PRG },	 //  1
	{ "6.6d",     		0x04000, 0xca4852f6, 1 | BRF_ESS | BRF_PRG },	 //  2
	{ "2.6b",     		0x04000, 0xba4b5822, 1 | BRF_ESS | BRF_PRG },	 //  3
	{ "7.7d",     		0x04000, 0x029d59d9, 1 | BRF_ESS | BRF_PRG },	 //  4
	{ "3.7b",     		0x04000, 0xd0771bba, 1 | BRF_ESS | BRF_PRG },	 //  5
	{ "8.9d",     		0x04000, 0x5a672942, 1 | BRF_ESS | BRF_PRG },	 //  6
	{ "4.9b",     		0x04000, 0x69227b56, 1 | BRF_ESS | BRF_PRG },	 //  7

	{ "11.15b",  		0x04000, 0x790ddfa9, 2 | BRF_ESS | BRF_PRG },	 //  8 Z80 Program Code
	{ "12.17b",  		0x04000, 0xd4531113, 2 | BRF_ESS | BRF_PRG },	 //  9

	{ "18.16g",    		0x02000, 0x591a3804, 3 | BRF_GRA },				 // 10 Chars

	{ "9.15f",    		0x08000, 0x984a597f, 4 | BRF_GRA },				 // 11 Tiles
	{ "10.17f",    		0x08000, 0x30e297ff, 4 | BRF_GRA },				 // 12

	{ "14.6e",     		0x04000, 0xbcf7740b, 5 | BRF_GRA },				 // 13 Sprites
	{ "16.6g",     		0x04000, 0x4a9ec3e6, 5 | BRF_GRA },				 // 14
	{ "15.7e",     		0x04000, 0xa70b565c, 5 | BRF_GRA },				 // 15
	{ "17.7g",     		0x04000, 0x450749fc, 5 | BRF_GRA },				 // 16

	{ "3.10f",  		0x00100, 0xce07c544, 6 | BRF_GRA },				 // 17 PROMs
	{ "2.11f",  		0x00100, 0x566d323a, 6 | BRF_GRA },				 // 18
	{ "1.12f",  		0x00100, 0x7ea63946, 6 | BRF_GRA },				 // 19
	{ "4.2g",   		0x00100, 0x08609bad, 6 | BRF_GRA },				 // 20

	{ "5.4e",   		0x00100, 0x2c43991f, 7 | BRF_GRA },				 // 21 Sprite Palette Bank

	{ "cp1408.11e",		0x00104, 0x00000000, 0 | BRF_OPT | BRF_NODUMP }, // 22 plds
	{ "tc1411.12a",		0x00104, 0x00000000, 0 | BRF_OPT | BRF_NODUMP }, // 23
	{ "tc1412.12f",		0x00104, 0x00000000, 0 | BRF_OPT | BRF_NODUMP }, // 24
};

STD_ROM_PICK(Drvn)
STD_ROM_FN(Drvn)

static INT32 TerracrenInit()
{
	has_ym2203 = 1;

	return TerracreInit();
}

struct BurnDriver BurnDrvTerracren = {
	"terracren", "terracre", NULL, NULL, "1985",
	"Terra Cresta (YM2203)\0", NULL, "Nichibutsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, DrvnRomInfo, DrvnRomName, NULL, NULL, NULL, NULL, TerracreInputInfo, TerracreDIPInfo,
	TerracrenInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&BurnRecalc, 0x1110, 224, 256, 3, 4
};


// Soldier Girl Amazon

static struct BurnRomInfo AmazonRomDesc[] = {
	{ "11.4d",         0x08000, 0x6c7f85c5, 1 | BRF_ESS | BRF_PRG },	//  0 68000 Program Code
	{ "9.4b",          0x08000, 0xe1b7a989, 1 | BRF_ESS | BRF_PRG },	//  1
	{ "12.6d",         0x08000, 0x4de8a3ee, 1 | BRF_ESS | BRF_PRG },	//  2
	{ "10.6b",         0x08000, 0xd86bad81, 1 | BRF_ESS | BRF_PRG },	//  3

	{ "1.15b",         0x04000, 0x55a8b5e7, 2 | BRF_ESS | BRF_PRG },	//  4 Z80 Program Code
	{ "2.17b",         0x04000, 0x427a7cca, 2 | BRF_ESS | BRF_PRG },	//  5
	{ "3.18b",         0x04000, 0xb8cceaf7, 2 | BRF_ESS | BRF_PRG },	//  6

	{ "8.16g",         0x02000, 0x0cec8644, 3 | BRF_GRA },				//  7 Chars

	{ "13.15f",        0x08000, 0x415ff4d9, 4 | BRF_GRA },				//  8 Tiles
	{ "14.17f",        0x08000, 0x492b5c48, 4 | BRF_GRA },				//  9
	{ "15.18f",        0x08000, 0xb1ac0b9d, 4 | BRF_GRA },				// 10

	{ "4.6e",          0x04000, 0xf77ced7a, 5 | BRF_GRA },				// 11 Sprites
	{ "6.6g",          0x04000, 0x936ec941, 5 | BRF_GRA },				// 12
	{ "5.7e",          0x04000, 0x16ef1465, 5 | BRF_GRA },				// 13
	{ "7.7g",          0x04000, 0x66dd718e, 5 | BRF_GRA },				// 14

	{ "clr.10f",       0x00100, 0x6440b341, 6 | BRF_GRA },				// 15 PROMs
	{ "clr.11f",       0x00100, 0x271e947f, 6 | BRF_GRA },				// 16
	{ "clr.12f",       0x00100, 0x7d38621b, 6 | BRF_GRA },				// 17
	{ "2g",            0x00100, 0x44ca16b9, 6 | BRF_GRA },				// 18

	{ "4e",            0x00100, 0x035f2c7b, 7 | BRF_GRA },				// 19 Sprite Palette Bank

	{ "16.18g",        0x02000, 0x1d8d592b, 8 | BRF_PRG | BRF_ESS },	// 20 Protection Data
};

STD_ROM_PICK(Amazon)
STD_ROM_FN(Amazon)

struct BurnDriver BurnDrvAmazon = {
	"amazon", NULL, NULL, NULL, "1986",
	"Soldier Girl Amazon\0", NULL, "Nichibutsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_RUNGUN, 0,
	NULL, AmazonRomInfo, AmazonRomName, NULL, NULL, NULL, NULL, AmazonInputInfo, AmazonDIPInfo,
	AmazonInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&BurnRecalc, 0x1110, 224, 256, 3, 4
};


// Soldier Girl Amazon (Tecfri license)

static struct BurnRomInfo AmazontRomDesc[] = {
	/* Found a on Tecfri TC-1A and TC-2A boardset */
	{ "5.4d",          0x08000, 0xae39432f, 1 | BRF_ESS | BRF_PRG },	//  0 68000 Program Code
	{ "7.4b",          0x08000, 0xa74cdcea, 1 | BRF_ESS | BRF_PRG },	//  1
	{ "4.6d",          0x08000, 0x0c6b0772, 1 | BRF_ESS | BRF_PRG },	//  2
	{ "6.6b",          0x08000, 0xedbaad3f, 1 | BRF_ESS | BRF_PRG },	//  3

	{ "1.15b",         0x04000, 0x55a8b5e7, 2 | BRF_ESS | BRF_PRG },	//  4 Z80 Program Code
	{ "2.17b",         0x04000, 0x427a7cca, 2 | BRF_ESS | BRF_PRG },	//  5
	{ "3.18b",         0x04000, 0xb8cceaf7, 2 | BRF_ESS | BRF_PRG },	//  6

	{ "8.16g",         0x02000, 0x0cec8644, 3 | BRF_GRA },				//  7 Chars

	{ "13.15f",        0x08000, 0x415ff4d9, 4 | BRF_GRA },				//  8 Tiles
	{ "14.17f",        0x08000, 0x492b5c48, 4 | BRF_GRA },				//  9
	{ "15.18f",        0x08000, 0xb1ac0b9d, 4 | BRF_GRA },				// 10

	{ "4.6e",          0x04000, 0xf77ced7a, 5 | BRF_GRA },				// 11 Sprites
	{ "6.6g",          0x04000, 0x936ec941, 5 | BRF_GRA },				// 12
	{ "5.7e",          0x04000, 0x16ef1465, 5 | BRF_GRA },				// 13
	{ "7.7g",          0x04000, 0x66dd718e, 5 | BRF_GRA },				// 14

	{ "clr.10f",       0x00100, 0x6440b341, 6 | BRF_GRA },				// 15 PROMs
	{ "clr.11f",       0x00100, 0x271e947f, 6 | BRF_GRA },				// 16
	{ "clr.12f",       0x00100, 0x7d38621b, 6 | BRF_GRA },				// 17
	{ "2g",            0x00100, 0x44ca16b9, 6 | BRF_GRA },				// 18

	{ "4e",            0x00100, 0x035f2c7b, 7 | BRF_GRA },				// 19 Sprite Palette Bank

	{ "16.18g",        0x02000, 0x1d8d592b, 8 | BRF_PRG | BRF_ESS },	// 20 Protection Data
};

STD_ROM_PICK(Amazont)
STD_ROM_FN(Amazont)

struct BurnDriver BurnDrvAmazont = {
	"amazont", "amazon", NULL, NULL, "1986",
	"Soldier Girl Amazon (Tecfri license)\0", NULL, "Nichibutsu (Tecfri license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_RUNGUN, 0,
	NULL, AmazontRomInfo, AmazontRomName, NULL, NULL, NULL, NULL, AmazonInputInfo, AmazonDIPInfo,
	AmazonInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&BurnRecalc, 0x1110, 224, 256, 3, 4
};


// Sei Senshi Amatelass

static struct BurnRomInfo AmatelasRomDesc[] = {
	{ "a11.4d",        0x08000, 0x3d226d0b, 1 | BRF_ESS | BRF_PRG },	//  0 68000 Program Code
	{ "a9.4b",         0x08000, 0xe2a0d21d, 1 | BRF_ESS | BRF_PRG },	//  1
	{ "a12.6d",        0x08000, 0xe6607c51, 1 | BRF_ESS | BRF_PRG },	//  2
	{ "a10.6b",        0x08000, 0xdbc1f1b4, 1 | BRF_ESS | BRF_PRG },	//  3

	{ "1.15b",         0x04000, 0x55a8b5e7, 2 | BRF_ESS | BRF_PRG },	//  4 Z80 Program Code
	{ "2.17b",         0x04000, 0x427a7cca, 2 | BRF_ESS | BRF_PRG },	//  5
	{ "3.18b",         0x04000, 0xb8cceaf7, 2 | BRF_ESS | BRF_PRG },	//  6

	{ "a8.16g",        0x02000, 0xaeba2102, 3 | BRF_GRA },				//  7 Chars

	{ "13.15f",        0x08000, 0x415ff4d9, 4 | BRF_GRA },				//  8 Tiles
	{ "14.17f",        0x08000, 0x492b5c48, 4 | BRF_GRA },				//  9
	{ "15.18f",        0x08000, 0xb1ac0b9d, 4 | BRF_GRA },				// 10

	{ "4.6e",          0x04000, 0xf77ced7a, 5 | BRF_GRA },				// 11 Sprites
	{ "6.6g",          0x04000, 0x936ec941, 5 | BRF_GRA },				// 12
	{ "a5.7e",         0x04000, 0x5fbf9a16, 5 | BRF_GRA },				// 13
	{ "7.7g",          0x04000, 0x66dd718e, 5 | BRF_GRA },				// 14

	{ "clr.10f",       0x00100, 0x6440b341, 6 | BRF_GRA },				// 15 PROMs
	{ "clr.11f",       0x00100, 0x271e947f, 6 | BRF_GRA },				// 16
	{ "clr.12f",       0x00100, 0x7d38621b, 6 | BRF_GRA },				// 17
	{ "2g",            0x00100, 0x44ca16b9, 6 | BRF_GRA },				// 18

	{ "4e",            0x00100, 0x035f2c7b, 7 | BRF_GRA },				// 19 Sprite Palette Bank

	{ "16.18g",        0x02000, 0x1d8d592b, 8 | BRF_PRG | BRF_ESS },	// 20 Protection Data
};

STD_ROM_PICK(Amatelas)
STD_ROM_FN(Amatelas)

struct BurnDriver BurnDrvAmatelas = {
	"amatelas", "amazon", NULL, NULL, "1986",
	"Sei Senshi Amatelass\0", NULL, "Nichibutsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_RUNGUN, 0,
	NULL, AmatelasRomInfo, AmatelasRomName, NULL, NULL, NULL, NULL, AmazonInputInfo, AmazonDIPInfo,
	AmazonInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&BurnRecalc, 0x1110, 224, 256, 3, 4
};


// Kid no Hore Hore Daisakusen

static struct BurnRomInfo HorekidRomDesc[] = {
	{ "horekid.03",    0x08000, 0x90ec840f, 1 | BRF_ESS | BRF_PRG },	//  0 68000 Program Code
	{ "horekid.01",    0x08000, 0xa282faf8, 1 | BRF_ESS | BRF_PRG },	//  1
	{ "horekid.04",    0x08000, 0x375c0c50, 1 | BRF_ESS | BRF_PRG },	//  2
	{ "horekid.02",    0x08000, 0xee7d52bb, 1 | BRF_ESS | BRF_PRG },	//  3

	{ "horekid.09",    0x04000, 0x49cd3b81, 2 | BRF_ESS | BRF_PRG },	//  4 Z80 Program Code
	{ "horekid.10",    0x04000, 0xc1eaa938, 2 | BRF_ESS | BRF_PRG },	//  5
	{ "horekid.11",    0x04000, 0x0a2bc702, 2 | BRF_ESS | BRF_PRG },	//  6

	{ "horekid.16",    0x02000, 0x104b77cc, 3 | BRF_GRA },				//  7 Chars

	{ "horekid.05",    0x08000, 0xda25ae10, 4 | BRF_GRA },				//  8 Tiles
	{ "horekid.06",    0x08000, 0x616e4321, 4 | BRF_GRA },				//  9
	{ "horekid.07",    0x08000, 0x8c7d2be2, 4 | BRF_GRA },				// 10
	{ "horekid.08",    0x08000, 0xa0066b02, 4 | BRF_GRA },				// 11

	{ "horekid.12",    0x08000, 0xa3caa07a, 5 | BRF_GRA },				// 12 Sprites
	{ "horekid.14",    0x08000, 0xe300747a, 5 | BRF_GRA },				// 13
	{ "horekid.13",    0x08000, 0x0e48ff8e, 5 | BRF_GRA },				// 14
	{ "horekid.15",    0x08000, 0x51105741, 5 | BRF_GRA },				// 15

	{ "kid_prom.10f",  0x00100, 0xca13ce23, 6 | BRF_GRA },				// 16 PROMs
	{ "kid_prom.11f",  0x00100, 0xfb44285a, 6 | BRF_GRA },				// 17
	{ "kid_prom.12f",  0x00100, 0x40d41237, 6 | BRF_GRA },				// 18
	{ "kid_prom.2g",   0x00100, 0x4b9be0ed, 6 | BRF_GRA },				// 19

	{ "kid_prom.4e",   0x00100, 0xe4fb54ee, 7 | BRF_GRA },				// 20 Sprite Palette Bank

	{ "horekid.17",    0x02000, 0x1d8d592b, 8 | BRF_PRG | BRF_ESS },	// 21 Protection Data
};

STD_ROM_PICK(Horekid)
STD_ROM_FN(Horekid)

static INT32 HorekidInit()
{
	is_horekid = 1;

	return AmazonInit();
}

struct BurnDriver BurnDrvHorekid = {
	"horekid", NULL, NULL, NULL, "1987",
	"Kid no Hore Hore Daisakusen\0", NULL, "Nichibutsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, HorekidRomInfo, HorekidRomName, NULL, NULL, NULL, NULL, TerracreInputInfo, HorekidDIPInfo,
	HorekidInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&BurnRecalc, 0x1110, 224, 256, 3, 4
};


// Kid no Hore Hore Daisakusen (bootleg set 1)

static struct BurnRomInfo HorekidbRomDesc[] = {
	{ "knhhd5",        0x08000, 0x786619c7, 1 | BRF_ESS | BRF_PRG },	//  0 68000 Program Code
	{ "knhhd7",        0x08000, 0x3bbb475b, 1 | BRF_ESS | BRF_PRG },	//  1
	{ "horekid.04",    0x08000, 0x375c0c50, 1 | BRF_ESS | BRF_PRG },	//  2
	{ "horekid.02",    0x08000, 0xee7d52bb, 1 | BRF_ESS | BRF_PRG },	//  3

	{ "horekid.09",    0x04000, 0x49cd3b81, 2 | BRF_ESS | BRF_PRG },	//  4 Z80 Program Code
	{ "horekid.10",    0x04000, 0xc1eaa938, 2 | BRF_ESS | BRF_PRG },	//  5
	{ "horekid.11",    0x04000, 0x0a2bc702, 2 | BRF_ESS | BRF_PRG },	//  6

	{ "horekid.16",    0x02000, 0x104b77cc, 3 | BRF_GRA },				//  7 Chars

	{ "horekid.05",    0x08000, 0xda25ae10, 4 | BRF_GRA },				//  8 Tiles
	{ "horekid.06",    0x08000, 0x616e4321, 4 | BRF_GRA },				//  9
	{ "horekid.07",    0x08000, 0x8c7d2be2, 4 | BRF_GRA },				// 10
	{ "horekid.08",    0x08000, 0xa0066b02, 4 | BRF_GRA },				// 11

	{ "horekid.12",    0x08000, 0xa3caa07a, 5 | BRF_GRA },				// 12 Sprites
	{ "horekid.14",    0x08000, 0xe300747a, 5 | BRF_GRA },				// 13
	{ "horekid.13",    0x08000, 0x0e48ff8e, 5 | BRF_GRA },				// 14
	{ "horekid.15",    0x08000, 0x51105741, 5 | BRF_GRA },				// 15

	{ "kid_prom.10f",  0x00100, 0xca13ce23, 6 | BRF_GRA },				// 16 PROMs
	{ "kid_prom.11f",  0x00100, 0xfb44285a, 6 | BRF_GRA },				// 17
	{ "kid_prom.12f",  0x00100, 0x40d41237, 6 | BRF_GRA },				// 18
	{ "kid_prom.2g",   0x00100, 0x4b9be0ed, 6 | BRF_GRA },				// 19

	{ "kid_prom.4e",   0x00100, 0xe4fb54ee, 7 | BRF_GRA },				// 20 Sprite Palette Bank

	{ "horekid.17",    0x02000, 0x1d8d592b, 8 | BRF_PRG | BRF_ESS },	// 21 Protection Data
};

STD_ROM_PICK(Horekidb)
STD_ROM_FN(Horekidb)

struct BurnDriver BurnDrvHorekidb = {
	"horekidb", "horekid", NULL, NULL, "1987",
	"Kid no Hore Hore Daisakusen (bootleg set 1)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, HorekidbRomInfo, HorekidbRomName, NULL, NULL, NULL, NULL, TerracreInputInfo, HorekidDIPInfo,
	HorekidInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&BurnRecalc, 0x1110, 224, 256, 3, 4
};


// Booby Kids (Italian manufactured graphic hack / bootleg of Kid no Hore Hore Daisakusen (bootleg))

static struct BurnRomInfo BoobhackRomDesc[] = {
	{ "1-c.bin",       0x08000, 0x786619c7, 1 | BRF_ESS | BRF_PRG },	//  0 68000 Program Code
	{ "1-b.bin",       0x08000, 0x3bbb475b, 1 | BRF_ESS | BRF_PRG },	//  1
	{ "1-d.bin",       0x08000, 0x375c0c50, 1 | BRF_ESS | BRF_PRG },	//  2
	{ "1-a.bin",       0x08000, 0xee7d52bb, 1 | BRF_ESS | BRF_PRG },	//  3

	{ "1-i.bin",       0x04000, 0x49cd3b81, 2 | BRF_ESS | BRF_PRG },	//  4 Z80 Program Code
	{ "1-j.bin",       0x04000, 0xc1eaa938, 2 | BRF_ESS | BRF_PRG },	//  5
	{ "1-k.bin",       0x04000, 0x0a2bc702, 2 | BRF_ESS | BRF_PRG },	//  6

	{ "1-p.bin",       0x02000, 0x104b77cc, 3 | BRF_GRA },				//  7 Chars

	{ "1-e.bin",       0x08000, 0xda25ae10, 4 | BRF_GRA },				//  8 Tiles
	{ "1-f.bin",       0x08000, 0x616e4321, 4 | BRF_GRA },				//  9
	{ "1-g.bin",       0x08000, 0x8c7d2be2, 4 | BRF_GRA },				// 10
	{ "1-h.bin",       0x08000, 0xa0066b02, 4 | BRF_GRA },				// 11

	{ "1-l.bin",       0x08000, 0xa3caa07a, 5 | BRF_GRA },				// 12 Sprites
	{ "1-n.bin",       0x08000, 0xe300747a, 5 | BRF_GRA },				// 13
	{ "1-m.bin",       0x08000, 0x15b6cbdf, 5 | BRF_GRA },				// 14
	{ "1-o.bin",       0x08000, 0xcddc6a6c, 5 | BRF_GRA },				// 15

	{ "kid_prom.10f",  0x00100, 0xca13ce23, 6 | BRF_GRA },				// 16 PROMs
	{ "kid_prom.11f",  0x00100, 0xfb44285a, 6 | BRF_GRA },				// 17
	{ "kid_prom.12f",  0x00100, 0x40d41237, 6 | BRF_GRA },				// 18
	{ "kid_prom.2g",   0x00100, 0x4b9be0ed, 6 | BRF_GRA },				// 19

	{ "kid_prom.4e",   0x00100, 0xe4fb54ee, 7 | BRF_GRA },				// 20 Sprite Palette Bank
};

STD_ROM_PICK(Boobhack)
STD_ROM_FN(Boobhack)

struct BurnDriver BurnDrvBoobhack = {
	"boobhack", "horekid", NULL, NULL, "1987",
	"Booby Kids (Italian manufactured graphic hack / bootleg of Kid no Hore Hore Daisakusen (bootleg))\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, BoobhackRomInfo, BoobhackRomName, NULL, NULL, NULL, NULL, TerracreInputInfo, HorekidDIPInfo,
	HorekidInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&BurnRecalc, 0x1110, 224, 256, 3, 4
};
