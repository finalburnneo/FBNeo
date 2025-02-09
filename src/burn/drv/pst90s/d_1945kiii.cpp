// FB Alpha 1945K-III driver module
// Based on MAME driver by David Haywood
// Port to Finalburn Alpha by OopsWare. 2007

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "msm6295.h"
#include "burn_pal.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvGfxROM[2];
static UINT8 *Drv68KRAM;
static UINT16 *DrvPalRAM;
static UINT16 *DrvSprRAM[2];
static UINT16 *DrvBgRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT16 *scrollx;
static UINT16 *scrolly;
static UINT8 *m6295bank;

static INT32 nGameSelect = 0;

static UINT8 DrvReset;
static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];

static struct BurnInputInfo _1945kiiiInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},

	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 4"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},

	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 4"	},
		
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(_1945kiii)

static struct BurnInputInfo FlagrallInputList[] = {
	{"P1 Coin 1",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Coin 2",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},

	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Flagrall)

static struct BurnDIPInfo _1945kiiiDIPList[] = {
	{0x14, 0xFF, 0xFF, 0xef, NULL					},
	{0x15, 0xFF, 0xFF, 0xff, NULL					},

	{0,    0xFE, 0,    8,	 "Coin 1"				},
	{0x14, 0x01, 0x07, 0x07, "1 coin  1 credit"		},
	{0x14, 0x01, 0x07, 0x06, "2 coins 1 credit"		},
	{0x14, 0x01, 0x07, 0x05, "3 coins 1 credit"		},
	{0x14, 0x01, 0x07, 0x04, "1 coin 2 credits"		},
	{0x14, 0x01, 0x07, 0x03, "Free Play"			},
	{0x14, 0x01, 0x07, 0x02, "5 coins 1 credit"		},
	{0x14, 0x01, 0x07, 0x01, "4 coins 1 credit"		},
	{0x14, 0x01, 0x07, 0x00, "1 coin 3 credits"		},

	{0,    0xFE, 0,    4,	 "Difficulty"			},
	{0x14, 0x01, 0x18, 0x18, "Hardest"				},
	{0x14, 0x01, 0x18, 0x10, "Hard"					},
	{0x14, 0x01, 0x18, 0x08, "Normal"				},
	{0x14, 0x01, 0x18, 0x00, "Easy"					},

	{0,    0xFE, 0,    4,	 "Lives"				},
	{0x14, 0x01, 0x60, 0x60, "3"					},
	{0x14, 0x01, 0x60, 0x40, "2"					},
	{0x14, 0x01, 0x60, 0x20, "4"					},
	{0x14, 0x01, 0x60, 0x00, "5"					},

	{0,    0xFE, 0,    2,	 "Service"				},
	{0x14, 0x01, 0x80, 0x80, "Off"					}, 
	{0x14, 0x01, 0x80, 0x00, "On"					},

	{0,    0xFE, 0,	   2,	 "Demo sound"			},
	{0x15, 0x01, 0x01, 0x01, "Off"					},
	{0x15, 0x01, 0x01, 0x00, "On"					},
	{0,    0xFE, 0,    2,	 "Allow Continue"		},
	{0x15, 0x01, 0x02, 0x02, "Yes"					},
	{0x15, 0x01, 0x02, 0x00, "No"					},
};

STDDIPINFO(_1945kiii)

static struct BurnDIPInfo _1945kiiioldDIPList[] = {
	{0x14, 0xFF, 0xFF, 0xef, NULL					},
	
	{0,    0xFE, 0,    8,	 "Coin 1"				},
	{0x14, 0x01, 0x07, 0x07, "1 coin  1 credit"		},
	{0x14, 0x01, 0x07, 0x06, "2 coins 1 credit"		},
	{0x14, 0x01, 0x07, 0x05, "3 coins 1 credit"		},
	{0x14, 0x01, 0x07, 0x04, "1 coin 2 credits"		},
	{0x14, 0x01, 0x07, 0x03, "Free Play"			},
	{0x14, 0x01, 0x07, 0x02, "1 coin 1 credit"		},
	{0x14, 0x01, 0x07, 0x01, "1 coin 1 credit"		},
	{0x14, 0x01, 0x07, 0x00, "1 coin 1 credit"		},

	{0,    0xFE, 0,    4,	 "Difficulty"			},
	{0x14, 0x01, 0x18, 0x18, "Hardest"				},
	{0x14, 0x01, 0x18, 0x10, "Hard"					},
	{0x14, 0x01, 0x18, 0x08, "Normal"				},
	{0x14, 0x01, 0x18, 0x00, "Easy"					},

	{0,    0xFE, 0,    4,	 "Lives"				},
	{0x14, 0x01, 0x60, 0x60, "3"					},
	{0x14, 0x01, 0x60, 0x40, "2"					},
	{0x14, 0x01, 0x60, 0x20, "4"					},
	{0x14, 0x01, 0x60, 0x00, "5"					},
};

STDDIPINFO(_1945kiiiold)

static struct BurnDIPInfo SoliteDIPList[] = {
	{0x14, 0xFF, 0xFF, 0xef, NULL					},
	
	{0,    0xFE, 0,    8,	 "Coin 1"				},
	{0x14, 0x01, 0x07, 0x07, "1 coin  1 credit"		},
	{0x14, 0x01, 0x07, 0x06, "2 coins 1 credit"		},
	{0x14, 0x01, 0x07, 0x05, "3 coins 1 credit"		},
	{0x14, 0x01, 0x07, 0x04, "1 coin 2 credits"		},
	{0x14, 0x01, 0x07, 0x03, "Free Play"			},
	{0x14, 0x01, 0x07, 0x02, "1 coin 1 credit"		},
	{0x14, 0x01, 0x07, 0x01, "1 coin 1 credit"		},
	{0x14, 0x01, 0x07, 0x00, "1 coin 1 credit"		},

	{0,    0xFE, 0,    4,	 "Difficulty"			},
	{0x14, 0x01, 0x18, 0x18, "Hardest"				},
	{0x14, 0x01, 0x18, 0x10, "Hard"					},
	{0x14, 0x01, 0x18, 0x08, "Normal"				},
	{0x14, 0x01, 0x18, 0x00, "Easy"					},
};

STDDIPINFO(Solite)

static struct BurnDIPInfo FlagrallDIPList[]=
{
	{0x0a, 0xff, 0xff, 0xa3, NULL					},
	{0x0b, 0xff, 0xff, 0xb7, NULL					},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x0a, 0x01, 0x03, 0x00, "3 Coins 1 Credits"	},
	{0x0a, 0x01, 0x03, 0x01, "2 Coins 1 Credits"	},
	{0x0a, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x0a, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x0a, 0x01, 0x10, 0x10, "Off"					},
	{0x0a, 0x01, 0x10, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Dip Control"			},
	{0x0a, 0x01, 0x20, 0x20, "Off"					},
	{0x0a, 0x01, 0x20, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Picture Test"			},
	{0x0a, 0x01, 0x80, 0x80, "Off"					},
	{0x0a, 0x01, 0x80, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x0b, 0x01, 0x03, 0x02, "1"					},
	{0x0b, 0x01, 0x03, 0x01, "2"					},
	{0x0b, 0x01, 0x03, 0x03, "3"					},
	{0x0b, 0x01, 0x03, 0x00, "5"					},

	{0   , 0xfe, 0   ,    2, "Bonus Type"			},
	{0x0b, 0x01, 0x04, 0x04, "0"					},
	{0x0b, 0x01, 0x04, 0x00, "1"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x0b, 0x01, 0x30, 0x00, "Very Hard"			},
	{0x0b, 0x01, 0x30, 0x10, "Hard"					},
	{0x0b, 0x01, 0x30, 0x20, "Easy"					},
	{0x0b, 0x01, 0x30, 0x30, "Normal"				},

	{0   , 0xfe, 0   ,    2, "Free Play"			},
	{0x0b, 0x01, 0x80, 0x80, "Off"					},
	{0x0b, 0x01, 0x80, 0x00, "On"					},
};

STDDIPINFO(Flagrall)

static inline UINT32 CalcCol(UINT16 nColour)
{
	return BurnHighCol(pal5bit(nColour >> 0), pal5bit(nColour >> 5), pal5bit(nColour >> 10), 0);
}

static void sndSetBank(UINT8 bank0, UINT8 bank1)
{
	if (bank0 != m6295bank[0]) {
		m6295bank[0] = bank0;
		MSM6295SetBank(0, MSM6295ROM + 0x00000 + 0x040000 * bank0, 0x00000, 0x3ffff);
	}

	if (bank1 != m6295bank[1] && nGameSelect < 2) { //1945kiii only
		m6295bank[1] = bank1;
		MSM6295SetBank(1, MSM6295ROM + 0x80000 + 0x040000 * bank1, 0x00000, 0x3ffff);
	}
}

static UINT16 __fastcall k1945iiiReadWord(UINT32 sekAddress)
{
	switch (sekAddress)
	{
		case 0x400000:
			return DrvInputs[0] | (DrvInputs[1] << 8);

		case 0x440000:
			return DrvInputs[2] | 0xff00;

		case 0x480000:
			return DrvDips[0] | (DrvDips[1] << 8);

		case 0x4C0000:
			return MSM6295Read(0);

		case 0x500000:
			return MSM6295Read(1);
	}

	return 0;
}

static void __fastcall k1945iiiWriteByte(UINT32 sekAddress, UINT8 byteValue)
{
	switch (sekAddress)
	{
		case 0x4C0000:
		case 0x4C0001: // flagrall
			MSM6295Write(0, byteValue);
		return;

		case 0x500000:
			MSM6295Write(1, byteValue);
		return;
	}
}

static void __fastcall k1945iiiWriteWord(UINT32 sekAddress, UINT16 wordValue)
{
	switch (sekAddress)
	{
		case 0x340000:
			scrollx[0] = wordValue;
		return;

		case 0x380000:
			scrolly[0] = wordValue;
		return;

		case 0x3C0000: {
			if (nGameSelect == 2) //flagrall
				sndSetBank((wordValue & 6) >> 1, 0);
			else
				sndSetBank((wordValue & 2) >> 1, (wordValue & 4) >> 2);
		}
		return;

		case 0x4C0000: // flagrall
			MSM6295Write(0, wordValue);
		return;
	}
}

static void __fastcall k1945iiiWriteWordPalette(UINT32 sekAddress, UINT16 wordValue)
{
	sekAddress = (sekAddress & 0xffe) / 2;
	DrvPalRAM[sekAddress] = BURN_ENDIAN_SWAP_INT16(wordValue);
	if (sekAddress < 0x200) DrvPalette[sekAddress] = CalcCol(wordValue);
}

static tilemap_callback( bg )
{
	INT32 code = BURN_ENDIAN_SWAP_INT16(DrvBgRAM[offs]);

	TILE_SET_INFO(0, code, 0, 0);
}

static INT32 MemIndex()
{
	UINT8 *Next;

	Next			= AllMem;

	Drv68KROM 		= Next; Next += 0x0100000;

	DrvGfxROM[0]	= Next; Next += 0x0200000;
	DrvGfxROM[1]	= Next; Next += 0x0400000;

	MSM6295ROM		= Next; Next += 0x0100000;
	
	DrvPalette		= (UINT32 *) Next; Next += 0x00200 * sizeof(UINT32);

	AllRam			= Next;
	
	Drv68KRAM		= Next; Next += 0x020000;
	DrvPalRAM		= (UINT16 *) Next; Next += 0x000800 * sizeof(UINT16);
	DrvSprRAM[0]	= (UINT16 *) Next; Next += 0x000800 * sizeof(UINT16);
	DrvSprRAM[1]	= (UINT16 *) Next; Next += 0x000800 * sizeof(UINT16);
	DrvBgRAM		= (UINT16 *) Next; Next += 0x000800 * sizeof(UINT16);

	m6295bank		= Next; Next += 0x000002;

	scrollx			= (UINT16 *) Next; Next += 0x000001 * sizeof(UINT16);
	scrolly			= (UINT16 *) Next; Next += 0x000001 * sizeof(UINT16);

	RamEnd			= Next;
	
	MemEnd			= Next;

	return 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();
	
	MSM6295Reset();

	m6295bank[0] = 1;
	m6295bank[1] = 1;

	sndSetBank(0, 0);

	HiscoreReset();

	return 0;
}

static INT32 DrvInit(INT32 game_select)
{
	nGameSelect = game_select;

	BurnAllocMemIndex();

	if (nGameSelect == 0)   // 1945kiii
	{
		if (BurnLoadRom(Drv68KROM 		+ 0x000000,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM		+ 0x000001,  1, 2)) return 1;

		if (BurnLoadRomExt(DrvGfxROM[1]	+ 0x000000,  2, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM[1]	+ 0x000002,  3, 4, LD_GROUP(2))) return 1;

		if (BurnLoadRom(DrvGfxROM[0]	+ 0x000000,  4, 1)) return 1;

		if (BurnLoadRom(MSM6295ROM		+ 0x000000,  5, 1)) return 1;

		if (BurnLoadRom(MSM6295ROM		+ 0x080000,  6, 1)) return 1;
	}
	else if (nGameSelect == 1) // 1945kiiio
	{
		if (BurnLoadRom(Drv68KROM		+ 0x000000,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM		+ 0x000001,  1, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM[1]	+ 0x000000,  2, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[1]	+ 0x000001,  3, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[1]	+ 0x000002,  4, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[1]	+ 0x000003,  5, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[1]	+ 0x200000,  6, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[1]	+ 0x200001,  7, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[1]	+ 0x200002,  8, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[1]	+ 0x200003,  9, 4)) return 1;

		if (BurnLoadRom(DrvGfxROM[0]	+ 0x000000, 10, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[0]	+ 0x000001, 11, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[0]	+ 0x000002, 12, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[0]	+ 0x000003, 13, 4)) return 1;

		if (BurnLoadRom(MSM6295ROM 		+ 0x000000, 14, 1)) return 1;

		if (BurnLoadRom(MSM6295ROM		+ 0x080000, 15, 1)) return 1;
	}
	else if (nGameSelect == 2)    // flagrall
	{
		if (BurnLoadRom(Drv68KROM		+ 0x000000,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM		+ 0x000001,  1, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM[1]	+ 0x000000,  2, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[1]	+ 0x000001,  3, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[1]	+ 0x000002,  4, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[1]	+ 0x000003,  5, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[1]	+ 0x200000,  6, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[1]	+ 0x200001,  7, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[1]	+ 0x200002,  8, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[1]	+ 0x200003,  9, 4)) return 1;

		if (BurnLoadRom(DrvGfxROM[0]	+ 0x000000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0]	+ 0x080000, 11, 1)) return 1;

		if (BurnLoadRom(MSM6295ROM		+ 0x000000, 12, 1)) return 1;

		if (BurnLoadRom(MSM6295ROM		+ 0x080000, 13, 1)) return 1;
	}

	{
		SekInit(0, 0x68000);
		SekOpen(0);
		SekMapMemory(Drv68KROM,				0x000000, 0x0fffff, MAP_ROM);
		SekMapMemory(Drv68KRAM,				0x100000, 0x10ffff, MAP_RAM);
		SekMapMemory((UINT8 *)DrvPalRAM,	0x200000, 0x200fff, MAP_ROM);
		SekMapHandler(1,					0x200000, 0x200fff, MAP_WRITE);	// palette write
		SekMapMemory((UINT8 *)DrvSprRAM[0],	0x240000, 0x240fff, MAP_RAM);
		SekMapMemory((UINT8 *)DrvSprRAM[1],	0x280000, 0x280fff, MAP_RAM);
		SekMapMemory((UINT8 *)DrvBgRAM,		0x2c0000, 0x2c0fff, MAP_RAM);
		SekMapMemory(Drv68KRAM + 0x10000,	0x8c0000, 0x8cffff, MAP_RAM);
		SekSetReadWordHandler(0, k1945iiiReadWord);
//		SekSetReadByteHandler(0, k1945iiiReadByte);
		SekSetWriteWordHandler(0, k1945iiiWriteWord);
		SekSetWriteByteHandler(0, k1945iiiWriteByte);
//		SekSetWriteByteHandler(1, k1945iiiWriteBytePalette);
		SekSetWriteWordHandler(1, k1945iiiWriteWordPalette);
		SekClose();
	}

    MSM6295Init(0, (nGameSelect == 2) ? 8000 : 7500, 1);
	MSM6295Init(1, (nGameSelect == 2) ? 8000 : 7500, 1);
	MSM6295SetRoute(0, (nGameSelect == 2) ? 1.00 : 2.50, BURN_SND_ROUTE_BOTH);
	MSM6295SetRoute(1, (nGameSelect == 2) ? 1.00 : 2.50, BURN_SND_ROUTE_BOTH);
	MSM6295SetBank(0, MSM6295ROM + 0x000000, 0, 0x3ffff);
	MSM6295SetBank(1, MSM6295ROM + 0x080000, 0, 0x3ffff);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 16, 16, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 8, 16, 16, 0x200000, 0x000, 0);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 8, 16, 16, 0x400000, 0x100, 1);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	SekExit();
	
	MSM6295Exit();

	nGameSelect = 0;

	BurnFreeMemIndex();

	return 0;
}

static void DrawSprites()
{
	for (INT32 i = 0; i < 0x1000/2; i++)
	{
		INT32 sx	 =  BURN_ENDIAN_SWAP_INT16(DrvSprRAM[0][i]) >> 8;
		INT32 sy	 =  BURN_ENDIAN_SWAP_INT16(DrvSprRAM[0][i]) & 0xff;
		INT32 code	 = (BURN_ENDIAN_SWAP_INT16(DrvSprRAM[1][i]) & 0x7ffe) >> 1;
		INT32 color  =  BURN_ENDIAN_SWAP_INT16(DrvSprRAM[1][i]) >> 15;
		sx 		    |= (BURN_ENDIAN_SWAP_INT16(DrvSprRAM[1][i]) & 0x0001) << 8;

		if (sx >= 336) sx -= 512;
		if (sy >= 240) sy -= 256;

		DrawGfxMaskTile(0, 1, code, sx, sy, 0, 0, color, 0);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x200; i++) {
			DrvPalette[i] = CalcCol(DrvPalRAM[i]);
		}

		DrvRecalc = 0;
	}

	GenericTilemapSetScrollX(0, scrollx[0]);
	GenericTilemapSetScrollY(0, scrolly[0]);

	if ((nBurnLayer & 1) == 0) BurnTransferClear();
	if ((nBurnLayer & 1) != 0) GenericTilemapDraw(0, 0, 0);
	if (nSpriteEnable & 1) DrawSprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nTotalCycles = (INT32)((INT64)16000000 * nBurnCPUSpeedAdjust / (0x0100 * 60));

	SekOpen(0);
	SekRun(nTotalCycles);
	SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
	SekClose();

	if (pBurnSoundOut) {
		BurnSoundClear();
		MSM6295Render(pBurnSoundOut, nBurnSoundLen);
	}
	
	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	if (pnMin) *pnMin =  0x029671;

	struct BurnArea ba;
	
	if (nAction & ACB_MEMORY_RAM) {	
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}
	
	if (nAction & ACB_DRIVER_DATA)
	{
		SekScan(nAction);

		MSM6295Scan(nAction, pnMin);
		
		if (nAction & ACB_WRITE) {
			INT32 bank0 = m6295bank[0];
			INT32 bank1 = m6295bank[1];
			m6295bank[0] = ~0;
			m6295bank[1] = ~0;

			sndSetBank(bank0, bank1);
		}
	}
	
	return 0;
}


// 1945k III (newer, OPCX2 PCB)

static struct BurnRomInfo _1945kiiiRomDesc[] = {
//	{ "prg-1.u51",	0x040000, 0xc4bbae5d, BRF_ESS | BRF_PRG },	//  0 68000 code 
//	{ "prg-2.u52", 	0x040000, 0x092abf2e, BRF_ESS | BRF_PRG }, 	//  1
	{ "prg-1.u51",	0x080000, 0x6b345f27, BRF_ESS | BRF_PRG },	//  0 68000 code 
	{ "prg-2.u52", 	0x080000, 0xce09b98c, BRF_ESS | BRF_PRG }, 	//  1
	
	{ "m16m-1.u62",	0x200000, 0x0b9a6474, BRF_GRA }, 			//  2 Sprites
	{ "m16m-2.u63",	0x200000, 0x368a8c2e, BRF_GRA },			//  3 
	
	{ "m16m-3.u61",	0x200000, 0x32fc80dd, BRF_GRA }, 			//  4 Background Layer
	
	{ "snd-1.su7",	0x080000, 0xbbb7f0ff, BRF_SND }, 			//  5 MSM #0 Samples

	{ "snd-2.su4",	0x080000, 0x47e3952e, BRF_SND }, 			//  6 MSM #1 Samples
};

STD_ROM_PICK(_1945kiii)
STD_ROM_FN(_1945kiii)

static INT32 _1945kiiiInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrv1945kiii = {
	"1945kiii", NULL, NULL, NULL, "2000",
	"1945k III (newer, OPCX2 PCB)\0", NULL, "Oriental Soft", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, _1945kiiiRomInfo, _1945kiiiRomName, NULL, NULL, NULL, NULL, _1945kiiiInputInfo, _1945kiiiDIPInfo,
	_1945kiiiInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 320, 3, 4
};


// 1945k III (older, OPCX1 PCB)

static struct BurnRomInfo _1945kiiioRomDesc[] = {
//	{ "3.u34",		0x40000, 0xbcfb8b84, 1 | BRF_PRG | BRF_ESS }, //  0 68000 code 
//	{ "4.u35",		0x40000, 0xec1234a5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.u34",		0x80000, 0x5515baa0, 1 | BRF_PRG | BRF_ESS }, //  0 68000 code 
	{ "4.u35",		0x80000, 0xfd177664, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "9.u5",		0x80000, 0xbe0f432e, 2 | BRF_GRA },           //  2 Sprites
	{ "10.u6",		0x80000, 0xcf9127b2, 2 | BRF_GRA },           //  3
	{ "11.u7",		0x80000, 0x644ee8cc, 2 | BRF_GRA },           //  4
	{ "12.u8",		0x80000, 0x0900c208, 2 | BRF_GRA },           //  5
	{ "13.u58",		0x80000, 0x8ea9c6be, 2 | BRF_GRA },           //  6
	{ "14.u59",		0x80000, 0x10c18fb4, 2 | BRF_GRA },           //  7
	{ "15.u60",		0x80000, 0x86ab6c7c, 2 | BRF_GRA },           //  8
	{ "16.u61",		0x80000, 0xff419080, 2 | BRF_GRA },           //  9

	{ "5.u102",		0x80000, 0x91b70a6b, 3 | BRF_GRA },           // 10 Background Layer
	{ "6.u103",		0x80000, 0x7b5bfb85, 3 | BRF_GRA },           // 11
	{ "7.u104",		0x80000, 0xcdafcedf, 3 | BRF_GRA },           // 12
	{ "8.u105",		0x80000, 0x2c3895d5, 3 | BRF_GRA },           // 13

	{ "s13.su4",	0x80000, 0xd45aec3b, 4 | BRF_SND },           // 14 MSM #0 Samples

	{ "s21.su5",	0x80000, 0x9d96fd55, 5 | BRF_SND },           // 15 MSM #1 Samples
};

STD_ROM_PICK(_1945kiiio)
STD_ROM_FN(_1945kiiio)

static INT32 _1945kiiioInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrv1945kiiio = {
	"1945kiiio", "1945kiii", NULL, NULL, "1999",
	"1945k III (older, OPCX1 PCB)\0", NULL, "Oriental Soft", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, _1945kiiioRomInfo, _1945kiiioRomName, NULL, NULL, NULL, NULL, _1945kiiiInputInfo, _1945kiiioldDIPInfo,
	_1945kiiioInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 320, 3, 4
};


// 1945k III (newer, OPCX1 PCB)

static struct BurnRomInfo _1945kiiinRomDesc[] = {
//	{ "u34",		0x40000, 0x234a55a5, 1 | BRF_PRG | BRF_ESS }, //  0 68000 code 
//	{ "u35",		0x40000, 0x1e6e23a5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "u34",		0x80000, 0xd0cf4f03, 1 | BRF_PRG | BRF_ESS }, //  0 68000 code 
	{ "u35",		0x80000, 0x056c64ed, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "u5",			0x80000, 0xf328f85e, 2 | BRF_GRA },           //  2 Sprites
	{ "u6",			0x80000, 0xcfdabf1b, 2 | BRF_GRA },           //  3
	{ "u7",			0x80000, 0x59a6a944, 2 | BRF_GRA },           //  4
	{ "u8",			0x80000, 0x59995aaf, 2 | BRF_GRA },           //  5
	{ "u58",		0x80000, 0x6acf2ce4, 2 | BRF_GRA },           //  6
	{ "u59",		0x80000, 0xca6ff210, 2 | BRF_GRA },           //  7
	{ "u60",		0x80000, 0x91eb038a, 2 | BRF_GRA },           //  8
	{ "u61",		0x80000, 0x1b358c6d, 2 | BRF_GRA },           //  9

	{ "5.u102",		0x80000, 0x91b70a6b, 3 | BRF_GRA },           // 10 Background Layer
	{ "6.u103",		0x80000, 0x7b5bfb85, 3 | BRF_GRA },           // 11
	{ "7.u104",		0x80000, 0xcdafcedf, 3 | BRF_GRA },           // 12
	{ "8.u105",		0x80000, 0x2c3895d5, 3 | BRF_GRA },           // 13

	{ "snd-1.su7",	0x80000, 0xbbb7f0ff, 4 | BRF_SND },           // 14 MSM #0 Samples

	{ "snd-2.su4",	0x80000, 0x47e3952e, 5 | BRF_SND },           // 15 MSM #1 Samples
};

STD_ROM_PICK(_1945kiiin)
STD_ROM_FN(_1945kiiin)

struct BurnDriver BurnDrv1945kiiin = {
	"1945kiiin", "1945kiii", NULL, NULL, "2000",
	"1945k III (newer, OPCX1 PCB)\0", NULL, "Oriental Soft", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, _1945kiiinRomInfo, _1945kiiinRomName, NULL, NULL, NULL, NULL, _1945kiiiInputInfo, _1945kiiiDIPInfo,
	_1945kiiioInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 320, 3, 4
};


// 1945k III (Promat license, OPCX1 PCB)

static struct BurnRomInfo _1945kiiiptRomDesc[] = {
	{ "u34",		0x80000, 0xd4efcdf4, 1 | BRF_PRG | BRF_ESS }, //  0 68000 code 
	{ "u35",		0x80000, 0x4381341d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "u5",			0x80000, 0x4742aa38, 2 | BRF_GRA },           //  2 Sprites
	{ "u6",			0x80000, 0xc137fb33, 2 | BRF_GRA },           //  3
	{ "u7",			0x80000, 0xd0593a03, 2 | BRF_GRA },           //  4
	{ "u8",			0x80000, 0xbaa9eeb1, 2 | BRF_GRA },           //  5
	{ "u58",		0x80000, 0xeb586bd6, 2 | BRF_GRA },           //  6
	{ "u59",		0x80000, 0xabc8b869, 2 | BRF_GRA },           //  7
	{ "u60",		0x80000, 0x31f9b034, 2 | BRF_GRA },           //  8
	{ "u61",		0x80000, 0xf13e609b, 2 | BRF_GRA },           //  9

	{ "u102",		0x80000, 0xddca369b, 3 | BRF_GRA },           // 10 Background Layer
	{ "u103",		0x80000, 0x32f58186, 3 | BRF_GRA },           // 11
	{ "u104",		0x80000, 0x2031c42e, 3 | BRF_GRA },           // 12
	{ "u105",		0x80000, 0x8428b693, 3 | BRF_GRA },           // 13

	{ "su4",		0x80000, 0xd45aec3b, 4 | BRF_SND },           // 14 MSM #0 Samples

	{ "su5",		0x80000, 0x9d96fd55, 5 | BRF_SND },           // 15 MSM #1 Samples
};

STD_ROM_PICK(_1945kiiipt)
STD_ROM_FN(_1945kiiipt)

struct BurnDriver BurnDrv1945kiiipt = {
	"1945kiiipt", "1945kiii", NULL, NULL, "1999",
	"1945k III (Promat license, OPCX1 PCB)\0", NULL, "Promat", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, _1945kiiiptRomInfo, _1945kiiiptRomName, NULL, NULL, NULL, NULL, _1945kiiiInputInfo, SoliteDIPInfo,
	_1945kiiioInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 320, 3, 4
};


// Solite Spirits

static struct BurnRomInfo slspiritRomDesc[] = {
	{ "3.u34",		0x40000, 0xb5ac3272, 1 | BRF_PRG | BRF_ESS }, //  0 68000 code
	{ "4.u35",		0x40000, 0x86397bcb, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "9.u5",		0x80000, 0x4742aa38, 2 | BRF_GRA },           //  2 Sprites
	{ "10.u6",		0x80000, 0xc137fb33, 2 | BRF_GRA },           //  3
	{ "11.u7",		0x80000, 0xd0593a03, 2 | BRF_GRA },           //  4
	{ "12.u8",		0x80000, 0xbaa9eeb1, 2 | BRF_GRA },           //  5
	{ "13.u58",		0x80000, 0xeb586bd6, 2 | BRF_GRA },           //  6
	{ "14.u59",		0x80000, 0xabc8b869, 2 | BRF_GRA },           //  7
	{ "15.u60",		0x80000, 0x31f9b034, 2 | BRF_GRA },           //  8
	{ "16.u61",		0x80000, 0xc1fc95e5, 2 | BRF_GRA },           //  9

	{ "5.u102",		0x80000, 0x9f0855a6, 3 | BRF_GRA },           // 10 Background Layer
	{ "6.u103",		0x80000, 0x0dda4489, 3 | BRF_GRA },           // 11
	{ "7.u104",		0x80000, 0x6208cdc7, 3 | BRF_GRA },           // 12
	{ "8.u105",		0x80000, 0xeb5f67a2, 3 | BRF_GRA },           // 13

	{ "s13.su4",	0x80000, 0xd9c63d55, 4 | BRF_SND },           // 14 MSM #0 Samples

	{ "s21.su5",	0x80000, 0xd4cbc27c, 5 | BRF_SND },           // 15 MSM #1 Samples
};

STD_ROM_PICK(slspirit)
STD_ROM_FN(slspirit)

struct BurnDriver BurnDrvSlspirit = {
	"slspirit", NULL, NULL, NULL, "1999",
	"Solite Spirits\0", NULL, "Promat", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, slspiritRomInfo, slspiritRomName, NULL, NULL, NULL, NULL, _1945kiiiInputInfo, SoliteDIPInfo,
	_1945kiiioInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 320, 3, 4
};


// '96 Flag Rally

static struct BurnRomInfo flagrallRomDesc[] = {
	{ "11_u34.bin",	0x040000, 0x24dd439d, BRF_ESS | BRF_PRG },	//  0 68000 code
	{ "12_u35.bin",	0x040000, 0x373b71a5, BRF_ESS | BRF_PRG },	//  1

	{ "1_u5.bin",	0x080000, 0x9377704b, BRF_GRA }, 			//  2 Sprites
	{ "5_u6.bin",	0x080000, 0x1ac0bd0c, BRF_GRA }, 			//  3 
	{ "2_u7.bin",	0x080000, 0x5f6db2b3, BRF_GRA }, 			//  4
	{ "6_u8.bin",	0x080000, 0x79e4643c, BRF_GRA }, 			//  5
	{ "3_u58.bin",	0x040000, 0xc913df7d, BRF_GRA }, 			//  6
	{ "4_u59.bin",	0x040000, 0xcb192384, BRF_GRA }, 			//  7
	{ "7_u60.bin",	0x040000, 0xf187a7bf, BRF_GRA }, 			//  8
	{ "8_u61.bin",	0x040000, 0xb73fa441, BRF_GRA }, 			//  9

	{ "10_u102.bin",0x080000, 0xb1fd3279, BRF_GRA }, 			// 10 Background Layer
	{ "9_u103.bin",	0x080000, 0x01e6d654, BRF_GRA }, 			// 11

	{ "13_su4.bin",	0x080000, 0x7b0630b3, BRF_SND }, 			// 12 MSM #0 Samples
	{ "14_su6.bin",	0x040000, 0x593b038f, BRF_SND }, 			// 13
};

STD_ROM_PICK(flagrall)
STD_ROM_FN(flagrall)

static INT32 flagrallInit()
{
	return DrvInit(2);
}

struct BurnDriver BurnDrvFlagrall = {
	"flagrall", NULL, NULL, NULL, "1996",
	"'96 Flag Rally\0", NULL, "Promat?", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 1, HARDWARE_MISC_POST90S, GBF_MAZE, 0,
	NULL, flagrallRomInfo, flagrallRomName, NULL, NULL, NULL, NULL, FlagrallInputInfo, FlagrallDIPInfo,
	flagrallInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	320, 240, 4, 3
};
