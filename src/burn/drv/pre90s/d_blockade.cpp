// FB Alpha Blockade hardware driver module
// Based on MAME driver by Frank Palazzolo and Dirk Best

#include "tiles_generic.h"
#include "z80_intf.h"
#include "samples.h"
#include <math.h>

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvMainROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvMainRAM;
static UINT8 *DrvVidRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 coin_latch;
static UINT8 coin_inserted;

static INT32 vblank;

// tone generator from crazy baloon w/ dinkmods
static UINT32 crbaloon_tone_step;
static UINT32 crbaloon_tone_pos;
static double crbaloon_tone_freq;
static double envelope_ctr;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[1];
static UINT8 DrvInputs[3];
static UINT8 DrvDips[2];
static UINT8 DrvReset;

static struct BurnInputInfo BlockadeInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 4,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 6,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 right"	},

	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip b",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};	

STDINPUTINFO(Blockade)

static struct BurnInputInfo ComotionInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},

	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},

	{"P3 Up",			BIT_DIGITAL,	DrvJoy2 + 4,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	DrvJoy2 + 6,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy2 + 5,	"p3 right"	},

	{"P4 Up",			BIT_DIGITAL,	DrvJoy3 + 4,	"p4 up"		},
	{"P4 Down",			BIT_DIGITAL,	DrvJoy3 + 6,	"p4 down"	},
	{"P4 Left",			BIT_DIGITAL,	DrvJoy3 + 7,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy3 + 5,	"p4 right"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Comotion)

static struct BurnInputInfo BlastoInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy3 + 7,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy3 + 5,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy3 + 6,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 fire 1"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Blasto)

static struct BurnInputInfo HustleInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 4,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 6,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 right"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Hustle)

static struct BurnInputInfo MineswprInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},

	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 6,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 right"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Mineswpr)

static struct BurnInputInfo Mineswpr4InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},

	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 6,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 right"	},

	{"P3 Up",			BIT_DIGITAL,	DrvJoy3 + 0,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	DrvJoy3 + 2,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	DrvJoy3 + 3,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p3 right"	},

	{"P4 Up",			BIT_DIGITAL,	DrvJoy3 + 4,	"p4 up"		},
	{"P4 Down",			BIT_DIGITAL,	DrvJoy3 + 6,	"p4 down"	},
	{"P4 Left",			BIT_DIGITAL,	DrvJoy3 + 7,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy3 + 5,	"p4 right"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Mineswpr4)

static struct BurnDIPInfo BlockadeDIPList[]=
{
	{0x0a, 0xff, 0xff, 0xff, NULL					},
	{0x0b, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    2, "Boom Switch"			},
	{0x0a, 0x01, 0x04, 0x00, "Off"					},
	{0x0a, 0x01, 0x04, 0x04, "On"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x0a, 0x01, 0x70, 0x60, "3"					},
	{0x0a, 0x01, 0x70, 0x50, "4"					},
	{0x0a, 0x01, 0x70, 0x30, "5"					},
	{0x0a, 0x01, 0x70, 0x70, "6"					},
};

STDDIPINFO(Blockade)

static struct BurnDIPInfo ComotionDIPList[]=
{
	{0x13, 0xff, 0xff, 0xf7, NULL					},
	{0x14, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    2, "Boom Switch"			},
	{0x13, 0x01, 0x04, 0x00, "Off"					},
	{0x13, 0x01, 0x04, 0x04, "On"					},

	{0   , 0xfe, 0   ,    2, "Lives"				},
	{0x13, 0x01, 0x08, 0x00, "3"					},
	{0x13, 0x01, 0x08, 0x08, "4"					},
};

STDDIPINFO(Comotion)

static struct BurnDIPInfo BlastoDIPList[]=
{
	{0x0e, 0xff, 0xff, 0xff, NULL					},
	{0x0f, 0xff, 0xff, 0xff, NULL					},
	
	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x0e, 0x01, 0x03, 0x00, "4 Coins 1 Credits"	},
	{0x0e, 0x01, 0x03, 0x01, "3 Coins 1 Credits"	},
	{0x0e, 0x01, 0x03, 0x02, "2 Coins 1 Credits"	},
	{0x0e, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x0e, 0x01, 0x04, 0x00, "Off"					},
	{0x0e, 0x01, 0x04, 0x04, "On"					},

	{0   , 0xfe, 0   ,    2, "Game Time"			},
	{0x0e, 0x01, 0x08, 0x00, "70 Secs"				},
	{0x0e, 0x01, 0x08, 0x08, "90 Secs"				},
};

STDDIPINFO(Blasto)

static struct BurnDIPInfo HustleDIPList[]=
{
	{0x0c, 0xff, 0xff, 0xff, NULL					},
	{0x0d, 0xff, 0xff, 0xfe, NULL					},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x0c, 0x01, 0x03, 0x00, "4 Coins 1 Credits"	},
	{0x0c, 0x01, 0x03, 0x01, "3 Coins 1 Credits"	},
	{0x0c, 0x01, 0x03, 0x02, "2 Coins 1 Credits"	},
	{0x0c, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    2, "Game Time"			},
	{0x0c, 0x01, 0x04, 0x00, "1.5 mins"				},
	{0x0c, 0x01, 0x04, 0x04, "2 mins"				},

	{0   , 0xfe, 0   ,    5, "Free Game"			},
	{0x0c, 0x01, 0xf1, 0x71, "11000"				},
	{0x0c, 0x01, 0xf1, 0xb1, "13000"				},
	{0x0c, 0x01, 0xf1, 0xd1, "15000"				},
	{0x0c, 0x01, 0xf1, 0xe1, "17000"				},
	{0x0c, 0x01, 0xf1, 0xf0, "Disabled"				},
};

STDDIPINFO(Hustle)

static struct BurnDIPInfo MineswprDIPList[]=
{
	{0x0a, 0xff, 0xff, 0xff, NULL					},
	{0x0b, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    2, "Boom Switch"			},
	{0x0a, 0x01, 0x04, 0x00, "Off"					},
	{0x0a, 0x01, 0x04, 0x04, "On"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x0a, 0x01, 0x70, 0x60, "3"					},
	{0x0a, 0x01, 0x70, 0x50, "4"					},
	{0x0a, 0x01, 0x70, 0x30, "5"					},
	{0x0a, 0x01, 0x70, 0x70, "6"					},
};

STDDIPINFO(Mineswpr)

static struct BurnDIPInfo Mineswpr4DIPList[]=
{
	{0x12, 0xff, 0xff, 0xfd, NULL					},
	{0x13, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    2, "Boom Switch"			},
	{0x12, 0x01, 0x04, 0x00, "Off"					},
	{0x12, 0x01, 0x04, 0x04, "On"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x12, 0x01, 0x70, 0x60, "3"					},
	{0x12, 0x01, 0x70, 0x50, "4"					},
	{0x12, 0x01, 0x70, 0x30, "5"					},
	{0x12, 0x01, 0x70, 0x70, "6"					},
};

STDDIPINFO(Mineswpr4)

static void sound_tone_render(INT16 *buffer, INT32 len)
{
	INT16 square = 0;

	memset(buffer, 0, len * 2 * sizeof(INT16));

	if (crbaloon_tone_step)	{
		while (len-- > 0) {
			square = crbaloon_tone_pos & 0x80000000 ? 32767 : -32768;
			square = (INT16)((double)square * 0.05);        // volume
			square = (INT16)(square * exp(-envelope_ctr));  // envelope volume
			envelope_ctr += (crbaloon_tone_freq > 1100.0) ? 0.0008 : 0.0005; // step envelope, treat the higher pitched sounds w/ a faster envelope
			*buffer++ = square;
			*buffer++ = square;

			crbaloon_tone_pos += crbaloon_tone_step;
		}
	}
}

static void sound_tone_write(UINT8 data)
{
	crbaloon_tone_step = 0;
	envelope_ctr = 0.0;

	if (data && data != 0xff) {
		double freq = (13630.0 / (256 - data) + (data >= 0xea ? 13 : 0)) * 0.5;

		crbaloon_tone_freq = freq;
		crbaloon_tone_step = (UINT32)(freq * 65536.0 * 65536.0 / (double)nBurnSoundRate);
	}
}

static void __fastcall blockade_write(UINT16 address, UINT8 data)
{
	if ((address & 0x9000) == 0x8000) {
		DrvVidRAM[address & 0x3ff] = data;

		if (vblank == 0)
		{
			ZetRunEnd();
		}			
	}
}

static void __fastcall blockade_write_port(UINT16 port, UINT8 data)
{
   // bprintf (0, _T("%2.2x, %2.2x\n"), port&0xff,data);
	
	switch (port & 0xff)
	{
		case 0x01:
			if (data & 0x80) {
				coin_latch = coin_inserted;
				coin_inserted = 0;
			}
		return;

		case 0x02:
			sound_tone_write(data);
		return;

		case 0x04:
			BurnSamplePlay(0);
		return;

		case 0x08:
			// env_off (not emulated)
		return;
	}
}

static UINT8 __fastcall blockade_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x01:
			return (DrvInputs[0] & 0xff) ^ (coin_latch ? 0x80 : 0);

		case 0x02:
			return DrvInputs[1];

		case 0x04:
			return DrvInputs[2];
	}

	return 0;
}

static tilemap_callback( layer )
{
	TILE_SET_INFO(0, DrvVidRAM[offs], 0, 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	BurnSampleReset();

	coin_latch   = 0;
	coin_inserted = 0;

	crbaloon_tone_step = 0;
	crbaloon_tone_pos = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvMainROM		= Next; Next += 0x001000;

	DrvGfxROM		= Next; Next += 0x001000;

	DrvPalette		= (UINT32*)Next; Next += 0x0002 * sizeof(UINT32);

	AllRam			= Next;

	DrvMainRAM		= Next; Next += 0x000100;
	DrvVidRAM		= Next; Next += 0x000400;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[1] = { 0 };
	INT32 XOffs[8] = { STEP8(0,1) };
	INT32 YOffs[8] = { STEP8(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x0200);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM, 0x0200);

	GfxDecode(0x40, 1, 8, 8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit(INT32 game_select)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		switch (game_select)
		{
			case 0: // blockade
			{
				if (BurnLoadRom(DrvMainROM + 0x000,  0, 1)) return 1;
				if (BurnLoadRom(DrvMainROM + 0x800,  1, 1)) return 1;

				if (BurnLoadRom(DrvGfxROM  + 0x000,  2, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM  + 0x100,  2, 1)) return 1; // mirror
				if (BurnLoadRom(DrvGfxROM  + 0x200,  3, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM  + 0x300,  3, 1)) return 1; // mirror
			}
			break;

			case 1: // comotion
			{
				if (BurnLoadRom(DrvMainROM + 0x000,  0, 1)) return 1;
				if (BurnLoadRom(DrvMainROM + 0x800,  1, 1)) return 1;
				if (BurnLoadRom(DrvMainROM + 0x400,  2, 1)) return 1;
				if (BurnLoadRom(DrvMainROM + 0xc00,  3, 1)) return 1;

				if (BurnLoadRom(DrvGfxROM  + 0x000,  4, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM  + 0x100,  4, 1)) return 1; // mirror
				if (BurnLoadRom(DrvGfxROM  + 0x200,  5, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM  + 0x300,  5, 1)) return 1; // mirror
			}
			break;

			case 2: // blasto
			{
				if (BurnLoadRom(DrvMainROM + 0x000,  0, 1)) return 1;
				if (BurnLoadRom(DrvMainROM + 0x800,  1, 1)) return 1;
				if (BurnLoadRom(DrvMainROM + 0x400,  2, 1)) return 1;
				if (BurnLoadRom(DrvMainROM + 0xc00,  3, 1)) return 1;

				if (BurnLoadRom(DrvGfxROM  + 0x000,  4, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM  + 0x200,  5, 1)) return 1;
			}
			break;

			case 3: // mineswpr
			{
				if (BurnLoadRom(DrvMainROM + 0x000,  0, 1)) return 1;
				if (BurnLoadRom(DrvMainROM + 0x800,  1, 1)) return 1;

				if (BurnLoadRom(DrvGfxROM  + 0x000,  2, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM  + 0x200,  3, 1)) return 1;
			}
			break;
		}

		for (INT32 i = 0; i < 0x800; i++) {
			DrvMainROM[i] = (DrvMainROM[i + 0x800] & 0xf) | (DrvMainROM[i] << 4);
		}

		for (INT32 i = 0; i < 0x200; i++) {
			DrvGfxROM[i] = (DrvGfxROM[i + 0x200] & 0xf) | (DrvGfxROM[i] << 4);
		}

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	for (INT32 i = 0; i < 0x8000; i+=0x2000)
	{
		ZetMapMemory(DrvMainROM,		0x0000 + i, 0x07ff + i, MAP_ROM);
		ZetMapMemory(DrvMainROM,		0x0800 + i, 0x0fff + i, MAP_ROM);

		for (INT32 j = 0; j < 0x1000; j+=0x400) {
			ZetMapMemory(DrvVidRAM,		0x8000 + i + j, 0x83ff + i + j, MAP_ROM);
		}

		for (INT32 j = 0; j < 0x1000; j+=0x100) {
			ZetMapMemory(DrvMainRAM,	0x9000 + i + j, 0x90ff + i + j, MAP_RAM);
		}
	}
	ZetSetWriteHandler(blockade_write);
	ZetSetOutHandler(blockade_write_port);
	ZetSetInHandler(blockade_read_port);
	ZetClose();

	BurnSampleInit(1);
	BurnSampleSetAllRoutesAllSamples(0.25, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, layer_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM, 1, 8, 8, 0x1000, 0, 0);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	BurnSampleExit();

	BurnFree(AllMem);

	return 0;
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPalette[0] = 0;
		DrvPalette[1] = BurnHighCol(0xff,0xff,0xff, 0);
		DrvRecalc = 0;
	}

	GenericTilemapDraw(0, pTransDraw, 0);

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
		DrvInputs[0] = DrvDips[0];
		DrvInputs[1] = 0xff;
		DrvInputs[2] = DrvDips[1];

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		if (DrvJoy4[0] && coin_inserted == 0) {
			ZetOpen(0);
			ZetReset();
			ZetClose();
			coin_inserted = 1;
		}
	}

	ZetOpen(0);
	vblank = 0;
	ZetRun(((2007900 / 60) * 224) / 262);
	vblank = 1;
	ZetRun(((2007900 / 60) *  38) / 262);
	ZetClose();

	if (pBurnSoundOut) {
		sound_tone_render(pBurnSoundOut, nBurnSoundLen);
		BurnSampleRender(pBurnSoundOut, nBurnSoundLen);
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
		BurnSampleScan(nAction, pnMin);

		SCAN_VAR(coin_latch);
		SCAN_VAR(coin_inserted);
		SCAN_VAR(crbaloon_tone_step);
		SCAN_VAR(crbaloon_tone_pos);
		SCAN_VAR(crbaloon_tone_freq);
		SCAN_VAR(envelope_ctr);
	}

	return 0;
}

static struct BurnSampleInfo BlockadeSampleDesc[] = {
	{ "boom",  SAMPLE_NOLOOP },
	{ "", 0 }
};

STD_SAMPLE_PICK(Blockade)
STD_SAMPLE_FN(Blockade)


// Blockade

static struct BurnRomInfo blockadeRomDesc[] = {
	{ "316-0004.u2",	0x0400, 0xa93833e9, 1 | BRF_PRG | BRF_ESS }, //  0 I8080 Code
	{ "316-0003.u3",	0x0400, 0x85960d3b, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "316-0002.u29",	0x0100, 0x409f610f, 2 | BRF_GRA },           //  2 Tiles
	{ "316-0001.u43",	0x0100, 0x41a00b28, 2 | BRF_GRA },           //  3
};

STD_ROM_PICK(blockade)
STD_ROM_FN(blockade)

static INT32 BlockadeInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvBlockade = {
	"blockade", NULL, NULL, "blockade", "1976",
	"Blockade\0", NULL, "Gremlin", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, blockadeRomInfo, blockadeRomName, NULL, NULL, BlockadeSampleInfo, BlockadeSampleName, BlockadeInputInfo, BlockadeDIPInfo,
	BlockadeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 2,
	256, 224, 4, 3
};


// CoMOTION

static struct BurnRomInfo comotionRomDesc[] = {
	{ "316-0007.u2",	0x0400, 0x5b9bd054, 1 | BRF_PRG | BRF_ESS }, //  0 I8080 Code
	{ "316-0008.u3",	0x0400, 0x1a856042, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "316-0009.u4",	0x0400, 0x2590f87c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "316-0010.u5",	0x0400, 0xfb49a69b, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "316-0006.u43",	0x0100, 0x8f071297, 2 | BRF_GRA },           //  4 Tiles
	{ "316-0005.u29",	0x0100, 0x53fb8821, 2 | BRF_GRA },           //  5
};

STD_ROM_PICK(comotion)
STD_ROM_FN(comotion)

static INT32 ComotionInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvComotion = {
	"comotion", NULL, NULL, "blockade", "1976",
	"CoMOTION\0", NULL, "Gremlin", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, comotionRomInfo, comotionRomName, NULL, NULL,  BlockadeSampleInfo, BlockadeSampleName, ComotionInputInfo, ComotionDIPInfo,
	ComotionInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 2,
	256, 224, 4, 3
};


// Blasto

static struct BurnRomInfo blastoRomDesc[] = {
	{ "316-0089.u2",	0x0400, 0xec99d043, 1 | BRF_PRG | BRF_ESS }, //  0 I8080 Code
	{ "316-0090.u3",	0x0400, 0xbe333415, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "316-0091.u4",	0x0400, 0x1c889993, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "316-0092.u5",	0x0400, 0xefb640cb, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "316-0093.u29",	0x0200, 0x4dd69499, 2 | BRF_GRA },           //  4 Tiles
	{ "316-0094.u43",	0x0200, 0x104051a4, 2 | BRF_GRA },           //  5
};

STD_ROM_PICK(blasto)
STD_ROM_FN(blasto)

static INT32 BlastoInit()
{
	return DrvInit(2);
}

struct BurnDriver BurnDrvBlasto = {
	"blasto", NULL, NULL, "blockade", "1978",
	"Blasto\0", NULL, "Gremlin", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, blastoRomInfo, blastoRomName, NULL, NULL,  BlockadeSampleInfo, BlockadeSampleName, BlastoInputInfo, BlastoDIPInfo,
	BlastoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 2,
	256, 224, 4, 3
};


// Hustle

static struct BurnRomInfo hustleRomDesc[] = {
	{ "316-0016.u2",	0x0400, 0xd983de7c, 1 | BRF_PRG | BRF_ESS }, //  0 I8080 Code
	{ "316-0017.u3",	0x0400, 0xedec9cb9, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "316-0018.u4",	0x0400, 0xf599b9c0, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "316-0019.u5",	0x0400, 0x7794bc7e, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "316-0020.u29",	0x0200, 0x541d2c67, 2 | BRF_GRA },           //  4 Tiles
	{ "316-0021.u43",	0x0200, 0xb5083128, 2 | BRF_GRA },           //  5
};

STD_ROM_PICK(hustle)
STD_ROM_FN(hustle)

static INT32 HustleInit()
{
	return DrvInit(2);
}

struct BurnDriver BurnDrvHustle = {
	"hustle", NULL, NULL, "blockade", "1977",
	"Hustle\0", NULL, "Gremlin", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, hustleRomInfo, hustleRomName, NULL, NULL,  BlockadeSampleInfo, BlockadeSampleName, HustleInputInfo, HustleDIPInfo,
	HustleInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 2,
	256, 224, 4, 3
};


// Minesweeper

static struct BurnRomInfo mineswprRomDesc[] = {
	{ "mineswee.h0p",	0x0400, 0x5850a4ba, 1 | BRF_PRG | BRF_ESS }, //  0 I8080 Code
	{ "mineswee.l0p",	0x0400, 0x05961379, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "mineswee.ums",	0x0200, 0x0e1c5c37, 2 | BRF_GRA },           //  2 Tiles
	{ "mineswee.uls",	0x0200, 0x3a4f66e1, 2 | BRF_GRA },           //  3
};

STD_ROM_PICK(mineswpr)
STD_ROM_FN(mineswpr)

static INT32 MineswprInit()
{
	return DrvInit(3);
}

struct BurnDriver BurnDrvMineswpr = {
	"mineswpr", NULL, NULL, "blockade", "1977",
	"Minesweeper\0", NULL, "Amutech", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, mineswprRomInfo, mineswprRomName, NULL, NULL,  BlockadeSampleInfo, BlockadeSampleName, MineswprInputInfo, MineswprDIPInfo,
	MineswprInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 2,
	256, 224, 4, 3
};


// Minesweeper (4-Player)

static struct BurnRomInfo mineswpr4RomDesc[] = {
	{ "mineswee.h0p",	0x0400, 0x5850a4ba, 1 | BRF_PRG | BRF_ESS }, //  0 I8080 Code
	{ "mineswee.l0p",	0x0400, 0x05961379, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "mineswee.cms",	0x0200, 0xaad3ce0c, 2 | BRF_GRA },           //  2 Tiles
	{ "mineswee.cls",	0x0200, 0x70959755, 2 | BRF_GRA },           //  3
};

STD_ROM_PICK(mineswpr4)
STD_ROM_FN(mineswpr4)

struct BurnDriver BurnDrvMineswpr4 = {
	"mineswpr4", "mineswpr", NULL, "blockade", "1977",
	"Minesweeper (4-Player)\0", NULL, "Amutech", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, mineswpr4RomInfo, mineswpr4RomName, NULL, NULL,  BlockadeSampleInfo, BlockadeSampleName, Mineswpr4InputInfo, Mineswpr4DIPInfo,
	MineswprInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 2,
	256, 224, 4, 3
};
