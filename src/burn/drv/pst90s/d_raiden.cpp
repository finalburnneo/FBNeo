// Raiden(c) 1990 Seibu Kaihatsu
// driver by Oliver Bergmann, Bryan McPhail, Randy Mongenel

// port to FB Alpha by OopsWare
// Update Aug. 15, 2014, IQ_132

#include "tiles_generic.h"
#include "nec_intf.h"
#include "z80_intf.h"
#include "seibusnd.h"

static UINT8 *Mem = NULL, *MemEnd = NULL;
static UINT8 *RamStart, *RamEnd;
static UINT8 *RomV30A;
static UINT8 *RomV30B;

static UINT8 *RomGfx1;
static UINT8 *RomGfx2;
static UINT8 *RomGfx3;
static UINT8 *RomGfx4;

static UINT8 *RamV30A;
static UINT8 *RamV30B;
static UINT8 *RamV30S;

static UINT8 *RamSpr;
static UINT16 *RamFg;
static UINT16 *RamBg;
static UINT16 *RamTxt;
static UINT8 *RamPal;
static UINT8 *RamScroll;

static UINT8 DrvLayerEnable;

static UINT32 *DrvPalette;
static UINT8 bRecalcPalette = 0;

static UINT8 DrvReset = 0;

static UINT8 DrvButton[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvJoy1[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvJoy2[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvInput[8] = {0, 0, 0, 0, 0, 0, 0, 0};

enum {
	GAME_RAIDEN=0,
	GAME_RAIDENB,
	GAME_RAIDENK,
	GAME_RAIDENU
};

static INT32 game_drv;

static struct BurnInputInfo raidenInputList[] = {
	{"P1 Coin", 	BIT_DIGITAL,    DrvButton + 0,  "p1 coin"   },
	{"P1 Start",    BIT_DIGITAL,    DrvJoy1 + 7,    "p1 start"  },

	{"P1 Up",   	BIT_DIGITAL,    DrvJoy1 + 0,    "p1 up"     },
	{"P1 Down", 	BIT_DIGITAL,    DrvJoy1 + 1,    "p1 down"   },
	{"P1 Left", 	BIT_DIGITAL,    DrvJoy1 + 2,    "p1 left"   },
	{"P1 Right",    BIT_DIGITAL,    DrvJoy1 + 3,    "p1 right"  },
	{"P1 Button 1", BIT_DIGITAL,    DrvJoy1 + 4,    "p1 fire 1" },
	{"P1 Button 2", BIT_DIGITAL,    DrvJoy1 + 5,    "p1 fire 2" },

	{"P2 Coin", 	BIT_DIGITAL,    DrvButton + 1,  "p2 coin"   },
	{"P2 Start",    BIT_DIGITAL,    DrvJoy2 + 7,    "p2 start"  },

	{"P2 Up",   	BIT_DIGITAL,    DrvJoy2 + 0,    "p2 up"     },
	{"P2 Down", 	BIT_DIGITAL,    DrvJoy2 + 1,    "p2 down"   },
	{"P2 Left", 	BIT_DIGITAL,    DrvJoy2 + 2,    "p2 left"   },
	{"P2 Right",    BIT_DIGITAL,    DrvJoy2 + 3,    "p2 right"  },
	{"P2 Button 1", BIT_DIGITAL,    DrvJoy2 + 4,    "p2 fire 1" },
	{"P2 Button 2", BIT_DIGITAL,    DrvJoy2 + 5,    "p2 fire 2" },

	{"Reset",   	BIT_DIGITAL,    &DrvReset,  	"reset"     },
	{"Dip A",   	BIT_DIPSWITCH,  DrvInput + 3,   "dip"       },
	{"Dip B",   	BIT_DIPSWITCH,  DrvInput + 4,   "dip"       },
};

STDINPUTINFO(raiden)

static struct BurnDIPInfo raidenDIPList[] =
{
	{0x11,  0xFF, 0xFF, 0x00, NULL          },
	{0x12,  0xFF, 0xFF, 0x00, NULL          },

	{0,     0xFE, 0,    2,    "Coin Mode"       },
	{0x11,  0x01, 0x01, 0x00, "A"          	 },
	{0x11,  0x01, 0x01, 0x01, "B"          	 },

	{0,     0xFE, 0,    16,   "Coinage"     },
	{0x11,  0x01, 0x1E, 0x00, "1 coin 1 credit" },
	{0x11,  0x01, 0x1E, 0x02, "2 coins 1 credit"    },
	{0x11,  0x01, 0x1E, 0x04, "3 coins 1 credit"    },
	{0x11,  0x01, 0x1E, 0x06, "4 coins 1 credits"   },
	{0x11,  0x01, 0x1E, 0x08, "5 coins 1 credit"    },
	{0x11,  0x01, 0x1E, 0x0A, "6 coins 1 credit"    },
	{0x11,  0x01, 0x1E, 0x0C, "1 coin 2 credits"    },
	{0x11,  0x01, 0x1E, 0x0E, "1 coin 3 credits"    },
	{0x11,  0x01, 0x1E, 0x10, "1 coin 4 credits"    },
	{0x11,  0x01, 0x1E, 0x12, "1 coin 5 credits"    },
	{0x11,  0x01, 0x1E, 0x14, "1 coin 6 credits"    },
	{0x11,  0x01, 0x1E, 0x16, "2 coins 3 credits"   },
	{0x11,  0x01, 0x1E, 0x18, "3 coins 2 credits"   },
	{0x11,  0x01, 0x1E, 0x1A, "5 coins 3 credits"   },
	{0x11,  0x01, 0x1E, 0x1C, "8 coins 3 credits"   },
	{0x11,  0x01, 0x1E, 0x1E, "Free Play"       },

	{0, 0xFE, 0,    2,    "Credits to Start"    },
	{0x11,  0x01, 0x20, 0x00, "1"           },
	{0x11,  0x01, 0x20, 0x20, "2"           },

	{0, 0xFE, 0,    2,    "Flip Screen"     },
	{0x11,  0x01, 0x80, 0x00, "Off"         },
	{0x11,  0x01, 0x80, 0x80, "On"          },

	{0, 0xFE, 0,    4,    "Lives"       },
	{0x12,  0x01, 0x03, 0x00, "3"           },
	{0x12,  0x01, 0x03, 0x01, "1"           },
	{0x12,  0x01, 0x03, 0x02, "2"           },
	{0x12,  0x01, 0x03, 0x03, "5"           },

	{0, 0xFE, 0,    4,    "Bonus Life"      },
	{0x12,  0x01, 0x0c, 0x00, "150000 400000"   },
	{0x12,  0x01, 0x0c, 0x04, "80000 300000"    },
	{0x12,  0x01, 0x0c, 0x08, "300000 1000000"  },
	{0x12,  0x01, 0x0c, 0x0c, "1000000 5000000" },

	{0, 0xFE, 0,    4,    "Difficulty"      },
	{0x12,  0x01, 0x30, 0x00, "Normal"      },
	{0x12,  0x01, 0x30, 0x10, "Easy"        },
	{0x12,  0x01, 0x30, 0x20, "Hard"        },
	{0x12,  0x01, 0x30, 0x30, "Very Hard"       },

	{0, 0xFE, 0,    2,    "Allow Continue"  },
	{0x12,  0x01, 0x40, 0x00, "Yes"         },
	{0x12,  0x01, 0x40, 0x40, "No"          },

	{0, 0xFE, 0,    2,    "Demo sound"      },
	{0x12,  0x01, 0x80, 0x00, "On"          },
	{0x12,  0x01, 0x80, 0x80, "Off"         },
};

STDDIPINFO(raiden)

inline static UINT32 CalcCol(INT32 offs)
{
	INT32 nColour = RamPal[offs + 0] | (RamPal[offs + 1] << 8);
	INT32 r, g, b;

	r = (nColour & 0x000F) << 4;
	r |= r >> 4;
	g = (nColour & 0x00F0) >> 0;
	g |= g >> 4;
	b = (nColour & 0x0F00) >> 4;
	b |= b >> 4;

	return BurnHighCol(r, g, b, 0);
}

static UINT8 __fastcall raidenReadByte(UINT32 vezAddress)
{
	switch (vezAddress)
	{
		case 0x0a000:
		case 0x0a001:
		case 0x0a002:
		case 0x0a003:
		case 0x0a004:
		case 0x0a005:
		case 0x0a006:
		case 0x0a007:
		case 0x0a008:
		case 0x0a009:
		case 0x0a00a:
		case 0x0a00b:
		case 0x0a00c:
		case 0x0a00d:
			return seibu_main_word_read(vezAddress);

		case 0x0e000: return ~DrvInput[1];
		case 0x0e001: return ~DrvInput[2];
		case 0x0e002: return ~DrvInput[3];
		case 0x0e003: return ~DrvInput[4];

	//  default:
	//      bprintf(PRINT_NORMAL, _T("CPU #0 Attempt to read byte value of location %x\n"), vezAddress);
	}

	return 0;
}

static void __fastcall raidenWriteByte(UINT32 vezAddress, UINT8 byteValue)
{
	switch (vezAddress)
	{
		case 0x0a000:
		case 0x0a001:
		case 0x0a002:
		case 0x0a003:
		case 0x0a004:
		case 0x0a005:
		case 0x0a006:
		case 0x0a007:
		case 0x0a008:
		case 0x0a009:
		case 0x0a00a:
		case 0x0a00b:
		case 0x0a00c:
		case 0x0a00d:
			seibu_main_word_write(vezAddress, byteValue);
		break;

		case 0x0e006:
		case 0x0e007:
			DrvLayerEnable = ~byteValue & 0x0f;
			// flipscreen not supported
		break;

		case 0x0f002:
		case 0x0f004:
		case 0x0f012:
		case 0x0f014:
		case 0x0f022:
		case 0x0f024:
		case 0x0f032:
		case 0x0f034:
			RamScroll[((vezAddress >> 2) & 1) | ((vezAddress >> 3) & 6)] = byteValue;
		break;

	//  default:
	//      bprintf(PRINT_NORMAL, _T("CPU #0 Attempt to write byte value %x to location %x\n"), byteValue, vezAddress);
	}
}

static UINT8 __fastcall raidenAltReadByte(UINT32 vezAddress)
{
	switch (vezAddress)
	{
		case 0x0b000: return ~DrvInput[1];
		case 0x0b001: return ~DrvInput[2];
		case 0x0b002: return ~DrvInput[3];
		case 0x0b003: return ~DrvInput[4];

		case 0x0d000:
		case 0x0d001:
		case 0x0d002:
		case 0x0d003:
		case 0x0d004:
		case 0x0d005:
		case 0x0d006:
		case 0x0d007:
		case 0x0d008:
		case 0x0d009:
		case 0x0d00a:
		case 0x0d00b:
		case 0x0d00c:
		case 0x0d00d:
			return seibu_main_word_read(vezAddress);

	//  default:
	//      bprintf(PRINT_NORMAL, _T("CPU #0 Attempt to read byte value of location %x\n"), vezAddress);
	}

	return 0;
}

static void __fastcall raidenAltWriteByte(UINT32 vezAddress, UINT8 byteValue)
{
	switch (vezAddress)
	{
		case 0x08002:   // Raidenu!!
		case 0x08004:
		case 0x08012:
		case 0x08014:
		case 0x08022:
		case 0x08024:
		case 0x08032:
		case 0x08034:
			RamScroll[((vezAddress >> 2) & 1) | ((vezAddress >> 3) & 6)] = byteValue;
		break;

		case 0x0b006:
		case 0x0b007:
			if (game_drv == GAME_RAIDENB) {
				DrvLayerEnable = (DrvLayerEnable & 0xfb) | ((~byteValue & 0x08) >> 1);
			} else {
				DrvLayerEnable = ~byteValue & 0x0f;
				// flipscreen not supported
			}
		break;

		case 0x0d000:
		case 0x0d001:
		case 0x0d002:
		case 0x0d003:
		case 0x0d004:
		case 0x0d005:
		case 0x0d006:
		case 0x0d007:
		case 0x0d008:
		case 0x0d009:
		case 0x0d00a:
		case 0x0d00b:
		case 0x0d00c:
		case 0x0d00d:
			seibu_main_word_write(vezAddress, byteValue);
		break;

		// ctc!
		case 0x0d05c: // raidenb
		case 0x0d05d:
			DrvLayerEnable = (DrvLayerEnable & 4) | (~byteValue & 0x03) | ((~byteValue & 0x10) >> 1);
		return;

		// ctc!!
		case 0x0d060: // raidenb
		case 0x0d061:
		case 0x0d062:
		case 0x0d064:
		case 0x0d065:
		case 0x0d066:
		case 0x0d067:
			RamScroll[ vezAddress - 0x0d060 ] = byteValue;
		break;

	//  default:
	//      bprintf(PRINT_NORMAL, _T("CPU #0 Attempt to write byte value %x to location %x\n"), byteValue, vezAddress);
	}
}

static void __fastcall raidenSubWriteByte(UINT32 vezAddress, UINT8 byteValue)
{
	if ((vezAddress & 0xFF000) == 0x03000 || (vezAddress & 0xFF000) == 0x07000 ) {
		vezAddress &= 0x00FFF;
		RamPal[ vezAddress ] = byteValue;
		if (vezAddress & 1)
			DrvPalette[ vezAddress >> 1 ] = CalcCol( vezAddress - 1 );
		return;
	}
}

static INT32 DrvDoReset()
{
	memset (RamStart, 0, RamEnd - RamStart);

	VezOpen(0);
	VezReset();
	VezClose();

	VezOpen(1);
	VezReset();
	VezClose();

	seibu_sound_reset();

	DrvLayerEnable = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = Mem;
	RomV30A     = Next; Next += 0x060000;
	RomV30B     = Next; Next += 0x040000;
	SeibuZ80ROM = Next; Next += 0x020000;
	SeibuZ80DecROM  = Next; Next += 0x020000;

	RomGfx1     = Next; Next += 0x020000;
	RomGfx2     = Next; Next += 0x100000;
	RomGfx3     = Next; Next += 0x100000;
	RomGfx4     = Next; Next += 0x100000;

	MSM6295ROM  = Next; Next += 0x010000;

	RamStart    = Next;

	RamV30A     = Next; Next += 0x007000;
	RamV30B     = Next; Next += 0x006000;
	RamV30S     = Next; Next += 0x001000;
	SeibuZ80RAM = Next; Next += 0x000800;

	RamSpr      = Next; Next += 0x001000;
	RamFg       = (UINT16 *)Next; Next += 0x000400 * sizeof(UINT16);
	RamBg       = (UINT16 *)Next; Next += 0x000400 * sizeof(UINT16);
	RamTxt      = (UINT16 *)Next; Next += 0x000400 * sizeof(UINT16);

	RamPal      = Next; Next += 0x001000;

	RamScroll   = Next; Next += 0x000008;

	RamEnd      = Next;

	DrvPalette  = (UINT32 *)Next; Next += 0x000800 * sizeof(UINT32);

	MemEnd      = Next;

	return 0;
}

void decode_gfx_1(UINT8 * dst, UINT8 * src)
{
	for(INT32 i=0;i<0x10000/32;i++) {
		for (INT32 j=0;j<8;j++) {
			dst[0] = (((src[0] >> 7) & 1) << 2) | (((src[0] >> 3) & 1) << 3) | (((src[0x8000] >> 7) & 1) << 0) | (((src[0x8000] >> 3) & 1) << 1);
			dst[1] = (((src[0] >> 6) & 1) << 2) | (((src[0] >> 2) & 1) << 3) | (((src[0x8000] >> 6) & 1) << 0) | (((src[0x8000] >> 2) & 1) << 1);
			dst[2] = (((src[0] >> 5) & 1) << 2) | (((src[0] >> 1) & 1) << 3) | (((src[0x8000] >> 5) & 1) << 0) | (((src[0x8000] >> 1) & 1) << 1);
			dst[3] = (((src[0] >> 4) & 1) << 2) | (((src[0] >> 0) & 1) << 3) | (((src[0x8000] >> 4) & 1) << 0) | (((src[0x8000] >> 0) & 1) << 1);
			dst[4] = (((src[1] >> 7) & 1) << 2) | (((src[1] >> 3) & 1) << 3) | (((src[0x8001] >> 7) & 1) << 0) | (((src[0x8001] >> 3) & 1) << 1);
			dst[5] = (((src[1] >> 6) & 1) << 2) | (((src[1] >> 2) & 1) << 3) | (((src[0x8001] >> 6) & 1) << 0) | (((src[0x8001] >> 2) & 1) << 1);
			dst[6] = (((src[1] >> 5) & 1) << 2) | (((src[1] >> 1) & 1) << 3) | (((src[0x8001] >> 5) & 1) << 0) | (((src[0x8001] >> 1) & 1) << 1);
			dst[7] = (((src[1] >> 4) & 1) << 2) | (((src[1] >> 0) & 1) << 3) | (((src[0x8001] >> 4) & 1) << 0) | (((src[0x8001] >> 0) & 1) << 1);

			src += 2;
			dst += 8;
		}
	}
}

void decode_gfx_2(UINT8 * dst, UINT8 * src)
{
	for(INT32 i=0;i<0x80000/128;i++) {
		for (INT32 j=0;j<16;j++) {
			dst[ 0] = (((src[ 1] >> 7) & 1) << 2) | (((src[ 1] >> 3) & 1) << 3) | (((src[ 0] >> 7) & 1) << 0) | (((src[ 0] >> 3) & 1) << 1);
			dst[ 1] = (((src[ 1] >> 6) & 1) << 2) | (((src[ 1] >> 2) & 1) << 3) | (((src[ 0] >> 6) & 1) << 0) | (((src[ 0] >> 2) & 1) << 1);
			dst[ 2] = (((src[ 1] >> 5) & 1) << 2) | (((src[ 1] >> 1) & 1) << 3) | (((src[ 0] >> 5) & 1) << 0) | (((src[ 0] >> 1) & 1) << 1);
			dst[ 3] = (((src[ 1] >> 4) & 1) << 2) | (((src[ 1] >> 0) & 1) << 3) | (((src[ 0] >> 4) & 1) << 0) | (((src[ 0] >> 0) & 1) << 1);
			dst[ 4] = (((src[ 3] >> 7) & 1) << 2) | (((src[ 3] >> 3) & 1) << 3) | (((src[ 2] >> 7) & 1) << 0) | (((src[ 2] >> 3) & 1) << 1);
			dst[ 5] = (((src[ 3] >> 6) & 1) << 2) | (((src[ 3] >> 2) & 1) << 3) | (((src[ 2] >> 6) & 1) << 0) | (((src[ 2] >> 2) & 1) << 1);
			dst[ 6] = (((src[ 3] >> 5) & 1) << 2) | (((src[ 3] >> 1) & 1) << 3) | (((src[ 2] >> 5) & 1) << 0) | (((src[ 2] >> 1) & 1) << 1);
			dst[ 7] = (((src[ 3] >> 4) & 1) << 2) | (((src[ 3] >> 0) & 1) << 3) | (((src[ 2] >> 4) & 1) << 0) | (((src[ 2] >> 0) & 1) << 1);

			dst[ 8] = (((src[65] >> 7) & 1) << 2) | (((src[65] >> 3) & 1) << 3) | (((src[64] >> 7) & 1) << 0) | (((src[64] >> 3) & 1) << 1);
			dst[ 9] = (((src[65] >> 6) & 1) << 2) | (((src[65] >> 2) & 1) << 3) | (((src[64] >> 6) & 1) << 0) | (((src[64] >> 2) & 1) << 1);
			dst[10] = (((src[65] >> 5) & 1) << 2) | (((src[65] >> 1) & 1) << 3) | (((src[64] >> 5) & 1) << 0) | (((src[64] >> 1) & 1) << 1);
			dst[11] = (((src[65] >> 4) & 1) << 2) | (((src[65] >> 0) & 1) << 3) | (((src[64] >> 4) & 1) << 0) | (((src[64] >> 0) & 1) << 1);
			dst[12] = (((src[67] >> 7) & 1) << 2) | (((src[67] >> 3) & 1) << 3) | (((src[66] >> 7) & 1) << 0) | (((src[66] >> 3) & 1) << 1);
			dst[13] = (((src[67] >> 6) & 1) << 2) | (((src[67] >> 2) & 1) << 3) | (((src[66] >> 6) & 1) << 0) | (((src[66] >> 2) & 1) << 1);
			dst[14] = (((src[67] >> 5) & 1) << 2) | (((src[67] >> 1) & 1) << 3) | (((src[66] >> 5) & 1) << 0) | (((src[66] >> 1) & 1) << 1);
			dst[15] = (((src[67] >> 4) & 1) << 2) | (((src[67] >> 0) & 1) << 3) | (((src[66] >> 4) & 1) << 0) | (((src[66] >> 0) & 1) << 1);


			src += 4;
			dst += 16;
		}
		src += 64;
	}
}

static void common_decrypt()
{
	UINT8 *RAM = RomV30A;
	INT32 i;
	UINT8 a;

	static const UINT8 xor_table[4][16]={
		{0xF1,0xF9,0xF5,0xFD,0xF1,0xF1,0x3D,0x3D,0x73,0xFB,0x77,0xFF,0x73,0xF3,0x3F,0x3F},   // rom 3
		{0xDF,0xFF,0xFF,0xFF,0xDB,0xFF,0xFB,0xFF,0xFF,0xFF,0xFF,0xFF,0xFB,0xFF,0xFB,0xFF},   // rom 4
		{0x7F,0x7F,0xBB,0x77,0x77,0x77,0xBE,0xF6,0x7F,0x7F,0xBB,0x77,0x77,0x77,0xBE,0xF6},   // rom 5
		{0xFF,0xFF,0xFD,0xFD,0xFD,0xFD,0xEF,0xEF,0xFF,0xFF,0xFD,0xFD,0xFD,0xFD,0xEF,0xEF}    // rom 6
	};

	// Rom 3 - main cpu even bytes
	for (i=0x20000; i<0x60000; i+=2) {
		a=RAM[i];
		a^=xor_table[0][(i/2) & 0x0f];
		a^=0xff;
		a=(a & 0x31) | ((a<<1) & 0x04) | ((a>>5) & 0x02) | ((a<<4) & 0x40) | ((a<<4) & 0x80) | ((a>>4) & 0x08);
		RAM[i]=a;
	}

	// Rom 4 - main cpu odd bytes
	for (i=0x20001; i<0x60000; i+=2) {
		a=RAM[i];
		a^=xor_table[1][(i/2) & 0x0f];
		a^=0xff;
		a=(a & 0xdb) | ((a>>3) & 0x04) | ((a<<3) & 0x20);
		RAM[i]=a;
	}

	RAM = RomV30B;

	// Rom 5 - sub cpu even bytes
	for (i=0x00000; i<0x40000; i+=2) {
		a=RAM[i];
		a^=xor_table[2][(i/2) & 0x0f];
		a^=0xff;
		a=(a & 0x32) | ((a>>1) & 0x04) | ((a>>4) & 0x08) | ((a<<5) & 0x80) | ((a>>6) & 0x01) | ((a<<6) & 0x40);
		RAM[i]=a;
	}

	// Rom 6 - sub cpu odd bytes
	for (i=0x00001; i<0x40000; i+=2) {
		a=RAM[i];
		a^=xor_table[3][(i/2) & 0x0f];
		a^=0xff;
		a=(a & 0xed) | ((a>>3) & 0x02) | ((a<<3) & 0x10);
		RAM[i]=a;
	}
}

static INT32 DrvInit(INT32 drv_select)
{
	game_drv = drv_select;

	Mem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(Mem, 0, nLen);
	MemIndex();

	if (BurnLoadRom(RomV30A + 0x000000, 0, 2)) return 1;
	if (BurnLoadRom(RomV30A + 0x000001, 1, 2)) return 1;
	if (BurnLoadRom(RomV30A + 0x020000, 2, 2)) return 1;
	if (BurnLoadRom(RomV30A + 0x020001, 3, 2)) return 1;

	if (BurnLoadRom(RomV30B + 0x000000, 4, 2)) return 1;
	if (BurnLoadRom(RomV30B + 0x000001, 5, 2)) return 1;

	if (game_drv != GAME_RAIDENB && game_drv != GAME_RAIDENU)
		common_decrypt();

	if (BurnLoadRom(SeibuZ80ROM  + 0x000000, 6, 1)) return 1;

	memcpy (SeibuZ80ROM + 0x10000, SeibuZ80ROM + 0x08000, 0x08000);
	memcpy (SeibuZ80ROM + 0x18000, SeibuZ80ROM + 0x00000, 0x08000);

	if (game_drv == GAME_RAIDEN || game_drv == GAME_RAIDENU) {
		// decrypt z80 in seibu sound init
	} else {
		SeibuZ80DecROM = NULL;
	}

	{
		UINT8 * tmp = (UINT8 *)BurnMalloc (0x80000);
		if (tmp == NULL) return 1;

		if (BurnLoadRom(tmp + 0x00000,  7, 1)) return 1;
		if (BurnLoadRom(tmp + 0x08000,  8, 1)) return 1;
		decode_gfx_1(RomGfx1, tmp);

		if (!strcmp(BurnDrvGetTextA(DRV_NAME), "raidenkb")) {
			if (BurnLoadRom(tmp + 0x00000,  9, 2)) return 1;
			if (BurnLoadRom(tmp + 0x00001, 10, 2)) return 1;
			if (BurnLoadRom(tmp + 0x40000, 11, 2)) return 1;
			if (BurnLoadRom(tmp + 0x40001, 12, 2)) return 1;
			decode_gfx_2(RomGfx2, tmp);
			if (BurnLoadRom(tmp + 0x00000, 13, 2)) return 1;
			if (BurnLoadRom(tmp + 0x00001, 14, 2)) return 1;
			if (BurnLoadRom(tmp + 0x40000, 15, 2)) return 1;
			if (BurnLoadRom(tmp + 0x40001, 16, 2)) return 1;
			decode_gfx_2(RomGfx3, tmp);
			if (BurnLoadRom(tmp + 0x00000, 17, 2)) return 1;
			if (BurnLoadRom(tmp + 0x00001, 18, 2)) return 1;
			if (BurnLoadRom(tmp + 0x40000, 19, 2)) return 1;
			if (BurnLoadRom(tmp + 0x40001, 20, 2)) return 1;
			decode_gfx_2(RomGfx4, tmp);
		} else {
			if (BurnLoadRom(tmp + 0x00000,  9, 1)) return 1;
			decode_gfx_2(RomGfx2, tmp);
			if (BurnLoadRom(tmp + 0x00000, 10, 1)) return 1;
			decode_gfx_2(RomGfx3, tmp);
			if (BurnLoadRom(tmp + 0x00000, 11, 1)) return 1;
			decode_gfx_2(RomGfx4, tmp);
		}
		
		BurnFree(tmp);
	}

	if (BurnLoadRom(MSM6295ROM, 12, 1)) return 1;

	{
		VezInit(0, V30_TYPE);
		VezOpen(0);
		VezMapArea(0x00000, 0x06fff, 0, RamV30A);               // RAM
		VezMapArea(0x00000, 0x06fff, 1, RamV30A);
		VezMapArea(0x07000, 0x07fff, 0, RamSpr);                // Sprites
		VezMapArea(0x07000, 0x07fff, 1, RamSpr);
		VezMapArea(0x0c000, 0x0c7ff, 1, (UINT8 *)RamTxt);
		VezMapArea(0xA0000, 0xFFFFF, 0, RomV30A);               // CPU 0 ROM
		VezMapArea(0xA0000, 0xFFFFF, 2, RomV30A);               // CPU 0 ROM

		if (game_drv == GAME_RAIDENB || game_drv == GAME_RAIDENU) {
			VezMapArea(0x0a000, 0x0afff, 0, RamV30S);           // Share RAM
			VezMapArea(0x0a000, 0x0afff, 1, RamV30S);
			VezSetReadHandler(raidenAltReadByte);
			VezSetWriteHandler(raidenAltWriteByte);
		} else {
			VezMapArea(0x08000, 0x08fff, 0, RamV30S);           // Share RAM
			VezMapArea(0x08000, 0x08fff, 1, RamV30S);
			VezSetReadHandler(raidenReadByte);
			VezSetWriteHandler(raidenWriteByte);
		}

		VezClose();

		VezInit(1, V30_TYPE);
		VezOpen(1);

		if (game_drv != GAME_RAIDENU) {
			VezMapArea(0x00000, 0x01fff, 0, RamV30B);           // RAM
			VezMapArea(0x00000, 0x01fff, 1, RamV30B);
			VezMapArea(0x02000, 0x027ff, 0, (UINT8 *)RamBg);        // Background
			VezMapArea(0x02000, 0x027ff, 1, (UINT8 *)RamBg);
			VezMapArea(0x02800, 0x02fff, 0, (UINT8 *)RamFg);        // Foreground
			VezMapArea(0x02800, 0x02fff, 1, (UINT8 *)RamFg);
			VezMapArea(0x03000, 0x03fff, 0, RamPal);            // Palette
		//  VezMapArea(0x03000, 0x03fff, 1, RamPal);
			VezMapArea(0x04000, 0x04fff, 0, RamV30S);           // Share RAM
			VezMapArea(0x04000, 0x04fff, 1, RamV30S);

			VezSetWriteHandler(raidenSubWriteByte);
		} else {
			VezMapArea(0x00000, 0x05fff, 0, RamV30B);           // RAM
			VezMapArea(0x00000, 0x05fff, 1, RamV30B);
			VezMapArea(0x06000, 0x067ff, 0, (UINT8 *)RamBg);        // Background
			VezMapArea(0x06000, 0x067ff, 1, (UINT8 *)RamBg);
			VezMapArea(0x06800, 0x06fff, 0, (UINT8 *)RamFg);        // Foreground
			VezMapArea(0x06800, 0x06fff, 1, (UINT8 *)RamFg);
			VezMapArea(0x07000, 0x07fff, 0, RamPal);            // Palette
		//  VezMapArea(0x07000, 0x07fff, 1, RamPal);
			VezMapArea(0x08000, 0x08fff, 0, RamV30S);           // Share RAM
			VezMapArea(0x08000, 0x08fff, 1, RamV30S);

			VezSetWriteHandler(raidenSubWriteByte);
		}

		VezMapArea(0xC0000, 0xFFFFF, 0, RomV30B);               // CPU 1 ROM
		VezMapArea(0xC0000, 0xFFFFF, 2, RomV30B);

		VezClose();
	}

	seibu_sound_init(0, 0x20000, 3579545, 3579545, 8000);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	VezExit();

	seibu_sound_exit();

	GenericTilesExit();

	BurnFree(Mem);

	return 0;
}

static void drawBackground()
{
	INT32 scrollx, scrolly;

	if (game_drv == GAME_RAIDENB) {
		scrollx = (RamScroll[0] | (RamScroll[1] << 8)) & 0x1ff;
		scrolly = (RamScroll[2] | (RamScroll[3] << 8)) & 0x1ff;
	} else {
		scrollx = ((RamScroll[2]&0x10)<<4) | ((RamScroll[3]&0x7f) << 1) | ((RamScroll[3]&0x80) >> 7);
		scrolly = ((RamScroll[0]&0x10)<<4) | ((RamScroll[1]&0x7f) << 1) | ((RamScroll[1]&0x80) >> 7);
	}

	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sy = (offs & 0x1f) * 16;
		INT32 sx = (offs / 0x20) * 16;

		sx -= scrollx;
		if (sx < -15) sx += 512;
		sy -= (scrolly + 16) & 0x1ff;
		if (sy < -15) sy += 512;

		if (sx >= 256 || sy >= 224) continue;

		INT32 code = BURN_ENDIAN_SWAP_INT16(RamBg[offs]) & 0x0FFF;
		INT32 color = (BURN_ENDIAN_SWAP_INT16(RamBg[offs]) & 0xF000) >> 12;

		if (sx < 0 || sx > 240 || sy < 0 || sy > 208) {
			Render16x16Tile_Clip(pTransDraw, code, sx, sy, color, 4, 0, RomGfx2);
		} else {
			Render16x16Tile(pTransDraw, code, sx, sy, color, 4, 0, RomGfx2);
		}
	}
}

static void drawForeground()
{
	INT32 scrollx, scrolly;

	if (game_drv == GAME_RAIDENB) {
		scrollx = (RamScroll[4] | (RamScroll[5] << 8)) & 0x1ff;
		scrolly = (RamScroll[6] | (RamScroll[7] << 8)) & 0x1ff;
	} else {
		scrollx = ((RamScroll[6]&0x10)<<4) | ((RamScroll[7]&0x7f) << 1) | ((RamScroll[7]&0x80) >> 7);
		scrolly = ((RamScroll[4]&0x10)<<4) | ((RamScroll[5]&0x7f) << 1) | ((RamScroll[5]&0x80) >> 7);
	}

	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sy = (offs & 0x1f) * 16;
		INT32 sx = (offs / 0x20) * 16;

		sx -= scrollx;
		if (sx < -15) sx += 512;
		sy -= (scrolly + 16) & 0x1ff;
		if (sy < -15) sy += 512;

		if (sx >= 256 || sy >= 224) continue;

		INT32 code = BURN_ENDIAN_SWAP_INT16(RamFg[offs]) & 0x0FFF;
		if (code == 0) continue;
		INT32 color = (BURN_ENDIAN_SWAP_INT16(RamFg[offs]) & 0xF000) >> 12;

		if (sx < 0 || sx > 240 || sy < 0 || sy > 208) {
			Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 4, 15, 0x100, RomGfx3);
		} else {
			Render16x16Tile_Mask(pTransDraw, code, sx, sy, color, 4, 15, 0x100, RomGfx3);
		}
	}
}

static void drawSprites(INT32 pri)
{
	for (INT32 offs = 0x1000 - 8; offs >= 0; offs -= 8)
	{
		if (RamSpr[offs+7] != 0xf) continue;
		if ((RamSpr[offs+5] & pri) == 0) continue;

		INT32 flipx = RamSpr[offs+1] & 0x20;
		INT32 flipy = RamSpr[offs+1] & 0x40;
		INT32 sy = RamSpr[offs+0] - 16;
		INT32 sx = RamSpr[offs+4];

		if (RamSpr[offs+5] & 0x01)
			sx = 0 - (0x100 - sx);

		INT32 color = RamSpr[offs+1] & 0xf;
		INT32 code = (RamSpr[offs+2] | (RamSpr[offs+3] << 8)) & 0x0fff;

		if (sx <= -16 || sx >= 256 || sy <= -16 || sy >= 224) continue;

		if (sx < 0 || sx > 240 || sy < 0 || sy > 208)
		{
			if (flipy) {
				if (flipx) {
					Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 4, 15, 0x200, RomGfx4);
				} else {
					Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 4, 15, 0x200, RomGfx4);
				}
			} else {
				if (flipx) {
					Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 4, 15, 0x200, RomGfx4);
				} else {
					Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 4, 15, 0x200, RomGfx4);
				}
			}
		} else {
			if (flipy) {
				if (flipx) {
					Render16x16Tile_Mask_FlipXY(pTransDraw, code, sx, sy, color, 4, 15, 0x200, RomGfx4);
				} else {
					Render16x16Tile_Mask_FlipY(pTransDraw, code, sx, sy, color, 4, 15, 0x200, RomGfx4);
				}
			} else {
				if (flipx) {
					Render16x16Tile_Mask_FlipX(pTransDraw, code, sx, sy, color, 4, 15, 0x200, RomGfx4);
				} else {
					Render16x16Tile_Mask(pTransDraw, code, sx, sy, color, 4, 15, 0x200, RomGfx4);
				}
			}
		}
	}
}

static void drawTextAlt()
{
	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sy = ((offs & 0x1f) * 8) - 16;
		INT32 sx = (offs / 0x20) * 8;

		INT32 code = (BURN_ENDIAN_SWAP_INT16(RamTxt[offs]) & 0x00FF) | ((BURN_ENDIAN_SWAP_INT16(RamTxt[offs]) & 0xC000) >> 6);
		INT32 color = (BURN_ENDIAN_SWAP_INT16(RamTxt[offs]) & 0x0F00) >> 8;

		if (sy < 0 || sy >= 224 || code == 0) continue;

		Render8x8Tile_Mask(pTransDraw, code, sx, sy, color, 4, 15, 0x300, RomGfx1);
	}
}

static void drawText()
{
	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sx = (offs & 0x1f) * 8;
		INT32 sy = ((offs / 0x20) * 8) - 16;

		INT32 code = (BURN_ENDIAN_SWAP_INT16(RamTxt[offs]) & 0x00FF) | ((BURN_ENDIAN_SWAP_INT16(RamTxt[offs]) & 0xC000) >> 6);
		INT32 color = (BURN_ENDIAN_SWAP_INT16(RamTxt[offs]) & 0x0F00) >> 8;

		if (sy < 0 || sy >= 224 || code == 0) continue;

		Render8x8Tile_Mask(pTransDraw, code, sx, sy, color, 4, 15, 0x300, RomGfx1);
	}
}

static INT32 DrvDraw()
{
	if (bRecalcPalette) {
		for (INT32 i=0;i<(0x1000/2);i++)
			DrvPalette[i] = CalcCol(i << 1);
		bRecalcPalette = 0;
	}

	if (~DrvLayerEnable & 1) BurnTransferClear();

	if ( DrvLayerEnable & 1) drawBackground();
	if ( DrvLayerEnable & 8) drawSprites(0x40);
	if ( DrvLayerEnable & 2) drawForeground();
	if ( DrvLayerEnable & 8) drawSprites(0x80);
	if ( DrvLayerEnable & 4) drawText();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvDrawAlt()
{
	if (bRecalcPalette) {
		for (INT32 i=0;i<(0x1000/2);i++)
			DrvPalette[i] = CalcCol(i << 1);
		bRecalcPalette = 0;
	}

	if (~DrvLayerEnable & 1) BurnTransferClear();

	if ( DrvLayerEnable & 1) drawBackground();
	if ( DrvLayerEnable & 8) drawSprites(0x40);
	if ( DrvLayerEnable & 2) drawForeground();
	if ( DrvLayerEnable & 8) drawSprites(0x80);
	if ( DrvLayerEnable & 4) drawTextAlt();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInput, 0, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInput[0] |= (DrvButton[i] & 1) << i;
			DrvInput[1] |= (DrvJoy1[i] & 1) << i;
			DrvInput[2] |= (DrvJoy2[i] & 1) << i;
		}

		seibu_coin_input = DrvInput[0];
	}

	VezNewFrame();
	ZetNewFrame();

	INT32 nInterleave = 400;
	INT32 nCyclesTotal[3] = { 10000000 / 60, 10000000 / 60, 3579545 / 60 };
	INT32 nCyclesDone[3] = { 0, 0, 0 };

	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		VezOpen(0);
		nCyclesDone[0] += VezRun(nCyclesTotal[0] / nInterleave);
		if (i == (nInterleave - 1)) VezSetIRQLineAndVector(0, 0xc8/4, CPU_IRQSTATUS_ACK);
		VezClose();

		VezOpen(1);
		nCyclesDone[1] += VezRun(nCyclesTotal[1] / nInterleave);
		if (i == (nInterleave - 1)) VezSetIRQLineAndVector(0, 0xc8/4, CPU_IRQSTATUS_ACK);
		VezClose();

		BurnTimerUpdateYM3812(i * (nCyclesTotal[2] / nInterleave));
	}

	BurnTimerEndFrameYM3812(nCyclesTotal[2]);

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	if (pBurnSoundOut) {
		seibu_sound_update(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();

	return 0;
}

static INT32 DrvScan(INT32 nAction,INT32 *pnMin)
{
	if ( pnMin ) *pnMin =  0x029671;

	struct BurnArea ba;

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
			ba.Data   = RamStart;
		ba.nLen   = RamEnd-RamStart;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_WRITE) {
		bRecalcPalette = 1;
	}

	if (nAction & ACB_DRIVER_DATA) {
		VezScan(nAction);

		seibu_sound_scan(pnMin, nAction);

		SCAN_VAR(DrvLayerEnable);
	}

	return 0;
}

// Raiden (set 1)

static struct BurnRomInfo raidenRomDesc[] = {
	{ "1.u0253",        0x010000, 0xa4b12785, BRF_ESS | BRF_PRG },  // CPU 0, V30
	{ "2.u0252",        0x010000, 0x17640bd5, BRF_ESS | BRF_PRG },
	{ "3.u022",     	0x020000, 0xf6af09d0, BRF_ESS | BRF_PRG },
	{ "4j.u023",        0x020000, 0x505c4c5d, BRF_ESS | BRF_PRG },

	{ "5.u042",     	0x020000, 0xed03562e, BRF_ESS | BRF_PRG },  // CPU 1, V30
	{ "6.u043",     	0x020000, 0xa19d5b5d, BRF_ESS | BRF_PRG },

	{ "8.u212",     	0x010000, 0xcbe055c7, BRF_ESS | BRF_PRG },  // CPU 2, Z80

	{ "9",          	0x008000, 0x1922b25e, BRF_GRA },        	// Tiles
	{ "10",         	0x008000, 0x5f90786a, BRF_GRA },
	{ "sei420",     	0x080000, 0xda151f0b, BRF_GRA },
	{ "sei430",     	0x080000, 0xac1f57ac, BRF_GRA },
	{ "sei440",         0x080000, 0x946d7bde, BRF_GRA },

	{ "7.u203",     	0x010000, 0x8f927822, BRF_SND },        	// Sound
};

STD_ROM_PICK(raiden)
STD_ROM_FN(raiden)

static INT32 RaidenInit() {
	return DrvInit(GAME_RAIDEN);
}

struct BurnDriver BurnDrvRaiden = {
	"raiden", NULL, NULL, NULL, "1990",
	"Raiden (set 1)\0", NULL, "Seibu Kaihatsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, raidenRomInfo, raidenRomName, NULL, NULL, raidenInputInfo, raidenDIPInfo,
	RaidenInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &bRecalcPalette, 0x800,
	224, 256, 3, 4
};


// Raiden (set 2)

static struct BurnRomInfo raidenaRomDesc[] = {
	{ "1.u0253",        0x010000, 0xa4b12785, BRF_ESS | BRF_PRG },  // CPU 0, V30
	{ "2.u0252",        0x010000, 0x17640bd5, BRF_ESS | BRF_PRG },
	{ "3.u022",     	0x020000, 0xf6af09d0, BRF_ESS | BRF_PRG },
	{ "4.u023",     	0x020000, 0x6bdfd416, BRF_ESS | BRF_PRG },

	{ "5.u042",     	0x020000, 0xed03562e, BRF_ESS | BRF_PRG },  // CPU 1, V30
	{ "6.u043",     	0x020000, 0xa19d5b5d, BRF_ESS | BRF_PRG },

	{ "8.u212",     	0x010000, 0xcbe055c7, BRF_ESS | BRF_PRG },  // CPU 2, Z80

	{ "9",          	0x008000, 0x1922b25e, BRF_GRA },        	// Tiles
	{ "10",         	0x008000, 0x5f90786a, BRF_GRA },
	{ "sei420",     	0x080000, 0xda151f0b, BRF_GRA },
	{ "sei430",     	0x080000, 0xac1f57ac, BRF_GRA },
	{ "sei440",     	0x080000, 0x946d7bde, BRF_GRA },

	{ "7.u203",     	0x010000, 0x8f927822, BRF_SND },        	// Sound
};

STD_ROM_PICK(raidena)
STD_ROM_FN(raidena)

struct BurnDriver BurnDrvRaidena = {
	"raidena", "raiden", NULL, NULL, "1990",
	"Raiden (set 2)\0", NULL, "Seibu Kaihatsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, raidenaRomInfo, raidenaRomName, NULL, NULL, raidenInputInfo, raidenDIPInfo,
	RaidenInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &bRecalcPalette, 0x800,
	224, 256, 3, 4
};


// Raiden (Taiwan)

static struct BurnRomInfo raidentRomDesc[] = {
	{ "1.u0253",        0x010000, 0xa4b12785, BRF_ESS | BRF_PRG },  // CPU 0, V30
	{ "2.u0252",        0x010000, 0x17640bd5, BRF_ESS | BRF_PRG },
	{ "3.u022",     	0x020000, 0xf6af09d0, BRF_ESS | BRF_PRG },
	{ "4t.u023",        0x020000, 0x61eefab1, BRF_ESS | BRF_PRG },

	{ "5.u042",     	0x020000, 0xed03562e, BRF_ESS | BRF_PRG },  // CPU 1, V30
	{ "6.u043",     	0x020000, 0xa19d5b5d, BRF_ESS | BRF_PRG },

	{ "8.u212",     	0x010000, 0xcbe055c7, BRF_ESS | BRF_PRG },  // CPU 2, Z80

	{ "9",          	0x008000, 0x1922b25e, BRF_GRA },        	// Tiles
	{ "10",         	0x008000, 0x5f90786a, BRF_GRA },
	{ "sei420",     	0x080000, 0xda151f0b, BRF_GRA },
	{ "sei430",     	0x080000, 0xac1f57ac, BRF_GRA },
	{ "sei440",     	0x080000, 0x946d7bde, BRF_GRA },

	{ "7.u203",     	0x010000, 0x8f927822, BRF_SND },        	// Sound
};

STD_ROM_PICK(raident)
STD_ROM_FN(raident)

struct BurnDriver BurnDrvRaident = {
	"raident", "raiden", NULL, NULL, "1990",
	"Raiden (Taiwan)\0", NULL, "Seibu Kaihatsu (Liang HWA Electronics license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, raidentRomInfo, raidentRomName, NULL, NULL, raidenInputInfo, raidenDIPInfo,
	RaidenInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &bRecalcPalette, 0x800,
	224, 256, 3, 4
};


// Raiden (US, set 1)

static struct BurnRomInfo raidenuRomDesc[] = {
	{ "1.u0253",        0x010000, 0xa4b12785, BRF_ESS | BRF_PRG },  // CPU 0, V30
	{ "2.u0252",        0x010000, 0x17640bd5, BRF_ESS | BRF_PRG },
	{ "3a.u022",        0x020000, 0xa8fadbdd, BRF_ESS | BRF_PRG },
	{ "4a.u023",        0x020000, 0xbafb268d, BRF_ESS | BRF_PRG },

	{ "5.u042",     	0x020000, 0xed03562e, BRF_ESS | BRF_PRG },  // CPU 1, V30
	{ "6.u043",     	0x020000, 0xa19d5b5d, BRF_ESS | BRF_PRG },

	{ "8.u212",     	0x010000, 0xcbe055c7, BRF_ESS | BRF_PRG },  // CPU 2, Z80

	{ "9",          	0x008000, 0x1922b25e, BRF_GRA },        	// Tiles
	{ "10",         	0x008000, 0x5f90786a, BRF_GRA },
	{ "sei420",     	0x080000, 0xda151f0b, BRF_GRA },
	{ "sei430",     	0x080000, 0xac1f57ac, BRF_GRA },
	{ "sei440",     	0x080000, 0x946d7bde, BRF_GRA },

	{ "7.u203",     	0x010000, 0x8f927822, BRF_SND },        	// Sound
};

STD_ROM_PICK(raidenu)
STD_ROM_FN(raidenu)

struct BurnDriver BurnDrvRaidenu = {
	"raidenu", "raiden", NULL, NULL, "1990",
	"Raiden (US, set 1)\0", NULL, "Seibu Kaihatsu (Fabtek license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, raidenuRomInfo, raidenuRomName, NULL, NULL, raidenInputInfo, raidenDIPInfo,
	RaidenInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &bRecalcPalette, 0x800,
	224, 256, 3, 4
};


// Raiden (Korea)

static struct BurnRomInfo raidenkRomDesc[] = {
	{ "1.u0253",        0x010000, 0xa4b12785, BRF_ESS | BRF_PRG },  // CPU 0, V30
	{ "2.u0252",        0x010000, 0x17640bd5, BRF_ESS | BRF_PRG },
	{ "3.u022",     	0x020000, 0xf6af09d0, BRF_ESS | BRF_PRG },
	{ "4k.u023",        0x020000, 0xfddf24da, BRF_ESS | BRF_PRG },

	{ "5.u042",     	0x020000, 0xed03562e, BRF_ESS | BRF_PRG },  // CPU 1, V30
	{ "6.u043",     	0x020000, 0xa19d5b5d, BRF_ESS | BRF_PRG },

	{ "8b.u212",        0x010000, 0x99ee7505, BRF_ESS | BRF_PRG },  // CPU 2, Z80

	{ "9",          	0x008000, 0x1922b25e, BRF_GRA },        	// Tiles
	{ "10",         	0x008000, 0x5f90786a, BRF_GRA },
	{ "sei420",     	0x080000, 0xda151f0b, BRF_GRA },
	{ "sei430",     	0x080000, 0xac1f57ac, BRF_GRA },
	{ "sei440",     	0x080000, 0x946d7bde, BRF_GRA },

	{ "7.u203",     	0x010000, 0x8f927822, BRF_SND },        	// Sound
};

STD_ROM_PICK(raidenk)
STD_ROM_FN(raidenk)

static INT32 RaidenkInit() {
	return DrvInit(GAME_RAIDENK);
}

struct BurnDriver BurnDrvRaidenk = {
	"raidenk", "raiden", NULL, NULL, "1990",
	"Raiden (Korea)\0", NULL, "Seibu Kaihatsu (IBL Corporation license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, raidenkRomInfo, raidenkRomName, NULL, NULL, raidenInputInfo, raidenDIPInfo,
	RaidenkInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &bRecalcPalette, 0x800,
	224, 256, 3, 4
};


// Raiden (Korea, bootleg)

static struct BurnRomInfo raidenkbRomDesc[] = {
	{ "1.u0253",        0x010000, 0xa4b12785, BRF_ESS | BRF_PRG },  // CPU 0, V30
	{ "2.u0252",        0x010000, 0x17640bd5, BRF_ESS | BRF_PRG },
	{ "3.u022",     	0x020000, 0xf6af09d0, BRF_ESS | BRF_PRG },
	{ "4k.u023",        0x020000, 0xfddf24da, BRF_ESS | BRF_PRG },

	{ "5.u042",     	0x020000, 0xed03562e, BRF_ESS | BRF_PRG },  // CPU 1, V30
	{ "6.u043",     	0x020000, 0xa19d5b5d, BRF_ESS | BRF_PRG },

	{ "8b.u212",        0x010000, 0x99ee7505, BRF_ESS | BRF_PRG },  // CPU 2, Z80

	{ "9",          	0x008000, 0x1922b25e, BRF_GRA },        	// Tiles
	{ "10",         	0x008000, 0x5f90786a, BRF_GRA },
	{ "rkb15bg.bin",    0x020000, 0x13a69064, BRF_GRA },
	{ "rkb17bg.bin",    0x020000, 0xd7a6c649, BRF_GRA },
	{ "rkb16bg.bin",    0x020000, 0x66ea8484, BRF_GRA },
	{ "rkb18bg.bin",    0x020000, 0x42362d56, BRF_GRA },
	{ "rkb7bg.bin",     0x020000, 0x25239711, BRF_GRA },
	{ "rkb9bg.bin",     0x020000, 0x6ca0d7b3, BRF_GRA },
	{ "rkb8bg.bin",     0x020000, 0x3cad38fc, BRF_GRA },
	{ "rkb10bg.bin",    0x020000, 0x6fce95a3, BRF_GRA },
	{ "rkb19obj.bin",   0x020000, 0x34fa4485, BRF_GRA },
	{ "rkb21obj.bin",   0x020000, 0xd806395b, BRF_GRA },
	{ "rkb20obj.bin",   0x020000, 0x8b7ca3c6, BRF_GRA },
	{ "rkb22obj.bin",   0x020000, 0x82ee78a0, BRF_GRA },

	{ "7.u203",     	0x010000, 0x8f927822, BRF_SND },        	// Sound
};

STD_ROM_PICK(raidenkb)
STD_ROM_FN(raidenkb)

struct BurnDriver BurnDrvRaidenkb = {
	"raidenkb", "raiden", NULL, NULL, "1990",
	"Raiden (Korea, bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, raidenkbRomInfo, raidenkbRomName, NULL, NULL, raidenInputInfo, raidenDIPInfo,
	RaidenkInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &bRecalcPalette, 0x800,
	224, 256, 3, 4
};

// Raiden (set 3, Alternate hardware)

static struct BurnRomInfo raidenbRomDesc[] = {
	{ "1.u0253",        	0x010000, 0xa4b12785, BRF_ESS | BRF_PRG },  // CPU 0, V30
	{ "2.u0252",        	0x010000, 0x17640bd5, BRF_ESS | BRF_PRG },
	{ "3__(raidenb).u022",  0x020000, 0x9d735bf5, BRF_ESS | BRF_PRG },
	{ "4__(raidenb).u023",  0x020000, 0x8d184b99, BRF_ESS | BRF_PRG },

	{ "5__(raidenb).u042",  0x020000, 0x7aca6d61, BRF_ESS | BRF_PRG },  // CPU 1, V30
	{ "6__(raidenb).u043",  0x020000, 0xe3d35cc2, BRF_ESS | BRF_PRG },

	{ "rai6.u212",      	0x010000, 0x723a483b, BRF_ESS | BRF_PRG },  // CPU 2, Z80

	{ "9",          		0x008000, 0x1922b25e, BRF_GRA },        	// Tiles
	{ "10",         		0x008000, 0x5f90786a, BRF_GRA },
	{ "sei420",     		0x080000, 0xda151f0b, BRF_GRA },
	{ "sei430",     		0x080000, 0xac1f57ac, BRF_GRA },
	{ "sei440",     		0x080000, 0x946d7bde, BRF_GRA },

	{ "7.u203",     		0x010000, 0x8f927822, BRF_SND },        	// Sound

	{ "ep910pc-1.bin",  	0x000884, 0x00000000, BRF_NODUMP },
	{ "ep910pc-2.bin",  	0x000884, 0x00000000, BRF_NODUMP },
};

STD_ROM_PICK(raidenb)
STD_ROM_FN(raidenb)

static INT32 RaidenbInit() {
	return DrvInit(GAME_RAIDENB);
}

struct BurnDriver BurnDrvRaidenb = {
	"raidenb", "raiden", NULL, NULL, "1990",
	"Raiden (set 3, Alternate hardware)\0", NULL, "Seibu Kaihatsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, raidenbRomInfo, raidenbRomName, NULL, NULL, raidenInputInfo, raidenDIPInfo,
	RaidenbInit, DrvExit, DrvFrame, DrvDrawAlt, DrvScan, &bRecalcPalette, 0x800,
	224, 256, 3, 4
};


// Raiden (US, set 2, SEI8904 + SEI9008 PCBs)

static struct BurnRomInfo raidenuaRomDesc[] = {
	{ "1.c8",       	0x010000, 0xa4b12785, BRF_ESS | BRF_PRG },  // CPU 0, V30
	{ "2.c7",       	0x010000, 0x17640bd5, BRF_ESS | BRF_PRG },
	{ "3dd.e8",     	0x020000, 0xb6f3bad2, BRF_ESS | BRF_PRG },
	{ "4dd.e7",     	0x020000, 0xd294dfc1, BRF_ESS | BRF_PRG },

	{ "5.p8",       	0x020000, 0x15c1cf45, BRF_ESS | BRF_PRG },  // CPU 1, V30
	{ "6.p7",       	0x020000, 0x261c381b, BRF_ESS | BRF_PRG },

	{ "8.w8",       	0x010000, 0x105b9c11, BRF_ESS | BRF_PRG },  // CPU 2, Z80

	{ "9",          	0x008000, 0x1922b25e, BRF_GRA },        	// Tiles
	{ "10",         	0x008000, 0x5f90786a, BRF_GRA },
	{ "sei420",     	0x080000, 0xda151f0b, BRF_GRA },
	{ "sei430",     	0x080000, 0xac1f57ac, BRF_GRA },
	{ "sei440",     	0x080000, 0x946d7bde, BRF_GRA },

	{ "7.x10",      	0x010000, 0x2051263e, BRF_SND },        	// Sound
};

STD_ROM_PICK(raidenua)
STD_ROM_FN(raidenua)

static INT32 RaidenuInit()
{
	return DrvInit(GAME_RAIDENU);
}

struct BurnDriver BurnDrvRaidenua = {
	"raidenua", "raiden", NULL, NULL, "1990",
	"Raiden (US, set 2, SEI8904 + SEI9008 PCBs)\0", NULL, "Seibu Kaihatsu (Fabtek license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, raidenuaRomInfo, raidenuaRomName, NULL, NULL, raidenInputInfo, raidenDIPInfo,
	RaidenuInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &bRecalcPalette, 0x800,
	224, 256, 3, 4
};
