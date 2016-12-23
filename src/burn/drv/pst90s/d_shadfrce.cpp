// FB Alpha Shadow Force driver module
// Based on MAME driver by David Haywood
// port to Finalburn Alpha by OopsWare. 2007

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "burn_ym2151.h"
#include "msm6295.h"

static UINT8 *Mem = NULL, *MemEnd = NULL;
static UINT8 *RamStart, *RamEnd;

static UINT8 *Rom68K;
static UINT8 *RomZ80;
static UINT8 *RomGfx01;
static UINT8 *RomGfx02;
static UINT8 *RomGfx03;
static UINT8 *DrvOkiROM;
static UINT8 *Ram68K;
static UINT16 *RamBg00;
static UINT16 *RamBg01;
static UINT16 *RamFg;
static UINT16 *RamSpr;
static UINT16 *SprBuf;
static UINT16 *RamPal;
static UINT8 *RamZ80;

static UINT32 *RamCurPal;

static UINT8 DrvButton[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvJoy1[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvJoy2[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvDipBtn[8] = {0, 0, 0, 0, 0, 0, 0, 0};

static UINT8 DrvInput[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvReset = 0;
static UINT8 nBrightness = 0xFF;

static UINT8 okibank;
static UINT8 video_enable;
static UINT8 irqs_enable;
static UINT16 raster_scanline;
static UINT8 raster_irq_enable;
static UINT8 previous_irq_value;
static UINT8 bVBlank = 0;
static UINT16 bg0scrollx, bg0scrolly, bg1scrollx, bg1scrolly;
static UINT8 nSoundlatch = 0;
static UINT8 bRecalcPalette = 0;

static INT32 nZ80Cycles;

static struct BurnInputInfo shadfrceInputList[] = {
	{"P1 Coin",	BIT_DIGITAL,	DrvDipBtn + 0,	"p1 coin"	},
	{"P1 Start",	BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},

	{"P1 Right",	BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Left",	BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Up",	BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",	BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",	BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",	BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},
	{"P1 Button 4",	BIT_DIGITAL,	DrvButton + 0,	"p1 fire 4"	},
	{"P1 Button 5",	BIT_DIGITAL,	DrvButton + 1,	"p1 fire 5"	},
	{"P1 Button 6",	BIT_DIGITAL,	DrvButton + 2,	"p1 fire 6"	},

	{"P2 Coin",	BIT_DIGITAL,	DrvDipBtn + 1,	"p2 coin"	},
	{"P2 Start",	BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},

	{"P2 Right",	BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Left",	BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Up",	BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",	BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Button 1",	BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",	BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",	BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"	},
	{"P2 Button 4",	BIT_DIGITAL,	DrvButton + 3,	"p2 fire 4"	},
	{"P2 Button 5",	BIT_DIGITAL,	DrvButton + 4,	"p2 fire 5"	},
	{"P2 Button 6",	BIT_DIGITAL,	DrvButton + 5,	"p2 fire 6"	},

	{"Reset",	BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",	BIT_DIPSWITCH,	DrvInput + 1,	"dip"		},
	{"Dip B",	BIT_DIPSWITCH,	DrvInput + 3,	"dip"		},
	{"Dip C",	BIT_DIPSWITCH,	DrvInput + 5,	"dip"		},
	{"Dip D",	BIT_DIPSWITCH,	DrvInput + 7,	"dip"		},
};

STDINPUTINFO(shadfrce)

static struct BurnDIPInfo shadfrceDIPList[] =
{
	{0x19,	0xFF, 0xFF,	0x00, NULL		},
	{0x1A,	0xFF, 0xFF,	0x00, NULL		},
	{0x1B,	0xFF, 0xFF,	0x00, NULL		},
	{0x1C,	0xFF, 0xFF,	0x00, NULL		},

	{0,	0xFE, 0,       2, "Service1"		},	// only available when you are in "test mode"
	{0x19,	0x01, 0x04, 0x00, "Off"			},
	{0x19,	0x01, 0x04, 0x04, "On"			},

	{0,	0xFE, 0,    4,	  "Difficulty"		},
	{0x1A,	0x01, 0x03, 0x00, "Normal"		},
	{0x1A,	0x01, 0x03, 0x01, "Hard"		},
	{0x1A,	0x01, 0x03, 0x02, "Easy"		},
	{0x1A,	0x01, 0x03, 0x03, "Hardest"		},

	{0,	0xFE, 0,       4, "Stage Clear Energy Regain" },
	{0x1A,	0x01, 0x0C, 0x00, "25%"			}, 
	{0x1A,	0x01, 0x0C, 0x04, "10%"			},	
	{0x1A,	0x01, 0x0C, 0x08, "50%"			}, 
	{0x1A,	0x01, 0x0C, 0x0C, "0%"			},	

	{0,	0xFE, 0,    4,	  "Coinage"		},	
	{0x1B,	0x01, 0x06, 0x00, "1 Coin  1 Credit"	},
	{0x1B,	0x01, 0x06, 0x02, "1 Coin  2 Credits"	},
	{0x1B,	0x01, 0x06, 0x04, "2 Coins 1 Credit"	},
	{0x1B,	0x01, 0x06, 0x06, "3 Coins 1 Credit"	},

	{0,	0xFE, 0,       2, "Continue Price"	},
	{0x1B,	0x01, 0x08, 0x00, "Off"			},
	{0x1B,	0x01, 0x08, 0x80, "On"			},

	{0,	0xFE, 0,       2, "Free Play"		},
	{0x1B,	0x01, 0x10, 0x00, "Off"			}, 
	{0x1B,	0x01, 0x10, 0x10, "On"			},

	{0,	0xFE, 0,       2, "Flip Screen"		},
	{0x1B,	0x01, 0x20, 0x00, "Off"			}, 
	{0x1B,	0x01, 0x20, 0x20, "On"			},

	{0,	0xFE, 0,	2,"Demo Sounds"		},
	{0x1C,	0x01, 0x01, 0x00, "Off"			},
	{0x1C,	0x01, 0x01, 0x01, "On"			},

	{0,	0xFE, 0,       2, "Test Mode"		},
	{0x1C,	0x01, 0x02, 0x00, "Off"			},
	{0x1C,	0x01, 0x02, 0x02, "On"			},
};

STDDIPINFO(shadfrce)

inline static void CalcCol(INT32 idx)
{
	UINT16 nColour = RamPal[idx];
	INT32 r = (nColour & 0x001F) << 3; r |= r >> 5;	// Red
	INT32 g = (nColour & 0x03E0) >> 2; g |= g >> 5;	// Green
	INT32 b = (nColour & 0x7C00) >> 7; b |= b >> 5;	// Blue
	r = (r * nBrightness) >> 8;
	g = (g * nBrightness) >> 8;
	b = (b * nBrightness) >> 8;
	RamCurPal[idx] = BurnHighCol(r, g, b, 0);
}

static UINT8 __fastcall shadfrceReadByte(UINT32 sekAddress)
{
	switch (sekAddress)
	{
		case 0x1D0020:
			return (~DrvInput[1] & 0x3F);

		case 0x1D0021:
			return ~DrvInput[0];

		case 0x1D0022:
			return (~DrvInput[3] & 0x3F);

		case 0x1D0023:
			return ~DrvInput[2];

		case 0x1D0024:
			return (~DrvInput[5] & 0x3F);

		case 0x1D0025:
			return ~DrvInput[4];

		case 0x1D0026:
			return ~(DrvInput[7] | (bVBlank << 2));

		case 0x1D0027:
			return ~DrvInput[6];

		case 0x1C000B:
			return 0;

		case 0x1D000D:
			return nBrightness;

	//	default:
	//		bprintf(PRINT_NORMAL, _T("Attempt to read byte value of location %x\n"), sekAddress);
	}

	return 0;
}

static UINT16 __fastcall shadfrceReadWord(UINT32 sekAddress)
{
	switch (sekAddress)
	{
		case 0x1D0020:
			return ~(DrvInput[0] | (DrvInput[1] << 8)) & 0x3FFF;

		case 0x1D0022:
			return ~(DrvInput[2] | (DrvInput[3] << 8)) & 0x3FFF;

		case 0x1D0024:
			return ~(DrvInput[4] | (DrvInput[5] << 8)) & 0x3FFF;

		case 0x1D0026:
			return ~(DrvInput[6] | ( (DrvInput[7] | (bVBlank << 2))<< 8));

	//	default:
	//		bprintf(PRINT_NORMAL, _T("Attempt to read word value of location %x\n"), sekAddress);
	}

	return 0;
}

static void __fastcall shadfrceWriteByte(UINT32 sekAddress, UINT8 byteValue)
{
	switch (sekAddress)
	{
		case 0x1C000B:
			// shadfrce_flip_screen
		break;

		case 0x1D0000:
		case 0x1D0001:
		case 0x1D0002:
		case 0x1D0003:
		case 0x1D0004:
		case 0x1D0005:
			SekSetIRQLine(((sekAddress/2) & 3) ^ 3, CPU_IRQSTATUS_NONE);
		break;

		case 0x1D0007:
		{
			irqs_enable = byteValue & 1;
			video_enable = byteValue & 0x08;

			if ((previous_irq_value & 4) == 0 && (byteValue & 4) == 4) {
				raster_irq_enable = 1;
				GenericTilemapSetScrollRows(1, 16*32);
			}
			if ((previous_irq_value & 4) == 4 && (byteValue & 4) == 0) {
				raster_irq_enable = 0;
				GenericTilemapSetScrollRows(1, 1);
			}

			previous_irq_value = byteValue;
		}
		break;

		case 0x1D0009:
		case 0x1D0008:
			raster_scanline = 0;
		break;

		case 0x1D000C:
			nSoundlatch = byteValue;
			ZetNmi();
		break;

		case 0x1D000D:
			nBrightness = byteValue;
			for(INT32 i=0;i<0x4000;i++) CalcCol(i);
		break;

		case 0x1C000D:
		case 0x1D0011:
		case 0x1D0013:
		case 0x1D0015:
		case 0x1D0017:	// NOP 
		break;

	//	default:
	//		bprintf(PRINT_NORMAL, _T("Attempt to write byte value %x to location %x\n"), byteValue, sekAddress);
	}
}

static void __fastcall shadfrceWriteWord(UINT32 sekAddress, UINT16 wordValue)
{
	switch (sekAddress)
	{
		case 0x1C0000: bg0scrollx = wordValue & 0x1FF; break;
		case 0x1C0002: bg0scrolly = wordValue & 0x1FF; break;
		case 0x1C0004: bg1scrollx = wordValue & 0x1FF; break;
		case 0x1C0006: bg1scrolly = wordValue & 0x1FF; break;

		case 0x1D000D:
			//nBrightness = byteValue;
		break;

		case 0x1D0016:
			// wait v-blink dip change call back ???
		break;

		case 0x1D0006:
		{
			irqs_enable = wordValue & 1;
			video_enable = wordValue & 0x08;

			if ((previous_irq_value & 4) == 0 && (wordValue & 4) == 4) {
				raster_irq_enable = 1;
				GenericTilemapSetScrollRows(1, 16*32);
			}
			if ((previous_irq_value & 4) == 4 && (wordValue & 4) == 0) {
				raster_irq_enable = 0;
				GenericTilemapSetScrollRows(1, 1);
			}

			previous_irq_value = wordValue;
		}
		break;

		case 0x1D0000:
		case 0x1D0002:
		case 0x1D0004:
			SekSetIRQLine(((sekAddress/2) & 3) ^ 3, CPU_IRQSTATUS_NONE);
		break;

		case 0x1D0008:
			raster_scanline = 0;
		break;

	//	default:
	//		bprintf(PRINT_NORMAL, _T("Attempt to write word value %x to location %x\n"), wordValue, sekAddress);
	}
}

static void __fastcall shadfrceWriteWordPalette(UINT32 sekAddress, UINT16 wordValue)
{
	sekAddress &= 0x7FFF;
	sekAddress >>= 1;
	RamPal[sekAddress] = wordValue;
	CalcCol(sekAddress);
}

static UINT8 __fastcall shadfrceZRead(UINT16 a)
{
	switch (a)
	{
		case 0xC801:
			return BurnYM2151ReadStatus();

		case 0xD800:
			return MSM6295ReadStatus(0);

		case 0xE000:
			return nSoundlatch;

	//	default:
	//		bprintf(PRINT_NORMAL, _T("Z80 address %04X read.\n"), a);
	}

	return 0;
}

void __fastcall shadfrceZWrite(UINT16 a, UINT8 d)
{
	switch (a)
	{
		case 0xC800:
			BurnYM2151SelectRegister(d);
		break;

		case 0xC801:
			BurnYM2151WriteRegister(d);
		break;

		case 0xD800:
			MSM6295Command(0, d);
		break;

		case 0xE800:
			okibank = (d & 1);
			MSM6295SetBank(0, DrvOkiROM + okibank * 0x40000, 0, 0x3ffff);
		break;

	//	default:
	//		bprintf(PRINT_NORMAL, _T("Z80 address %04X -> %02X.\n"), a, d);
	}
}

static void shadfrceYM2151IRQHandler(INT32 nStatus)
{
	if (nStatus) {
		ZetSetIRQLine(0xFF, CPU_IRQSTATUS_ACK);
		ZetRun(1024);
	} else {
		ZetSetIRQLine(0,    CPU_IRQSTATUS_NONE);
	}
}

static INT32 DrvDoReset()
{
	SekOpen(0);
	SekSetIRQLine(0, CPU_IRQSTATUS_NONE);
	SekReset();
	SekClose();
	
	ZetOpen(0);
	ZetReset();
	ZetClose();

	MSM6295Reset(0);
	shadfrceZWrite(0xe800, 0); // set initial oki bank
	BurnYM2151Reset();

	video_enable = 0;
	irqs_enable = 0;
	raster_scanline = 0;
	raster_irq_enable = 0;
	previous_irq_value = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = Mem;

	Rom68K 		= Next; Next += 0x100000;		// 68000 ROM
	RomZ80		= Next; Next += 0x010000;		// Z80 ROM
	RomGfx01	= Next; Next += 0x020000 / 4 * 8;	// fg 8x8x4
	RomGfx02	= Next; Next += 0xA00000 / 5 * 8;	// spr 16x16x5 sprite
	RomGfx03	= Next; Next += 0x300000 / 6 * 8;	// bg 16x16x6 tile

	MSM6295ROM	= Next;
	DrvOkiROM 	= Next; Next += 0x080000;

	RamStart	= Next;

	RamBg00		= (UINT16 *) Next; Next += 0x001000 * sizeof(UINT16);
	RamBg01		= (UINT16 *) Next; Next += 0x001000 * sizeof(UINT16);
	RamFg		= (UINT16 *) Next; Next += 0x001000 * sizeof(UINT16);
	RamSpr		= (UINT16 *) Next; Next += 0x001000 * sizeof(UINT16);
	SprBuf		= (UINT16 *) Next; Next += 0x001000 * sizeof(UINT16);
	RamPal		= (UINT16 *) Next; Next += 0x004000 * sizeof(UINT16);
	Ram68K		= Next; Next += 0x010000;
	RamZ80		= Next; Next += 0x001800;
	RamEnd		= Next;
	
	RamCurPal	= (UINT32 *) Next; Next += 0x004000 * sizeof(UINT32);
	
	MemEnd		= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[4]  = { STEP4(0,2) };
	INT32 XOffs0[8]  = { 1, 0, 8*8+1, 8*8+0, 16*8+1, 16*8+0, 24*8+1, 24*8+0 };
	INT32 YOffs0[8]  = { STEP8(0,8) };

	INT32 Plane1[5]  = { (0x200000*8*4), (0x200000*8*3), (0x200000*8*2), (0x200000*8*1), (0x200000*8*0) };
	INT32 XOffs1[16] = { STEP8(0,1), STEP8(16*8,1) };
	INT32 YOffs1[16] = { STEP16(0,8) };

	INT32 Plane2[6]  = { (0x100000*8*0)+8, (0x100000*8*0), (0x100000*8*1)+8, (0x100000*8*1), (0x100000*8*2)+8, (0x100000*8*2) };
	INT32 XOffs2[16] = { STEP8(0,1), STEP8(16*16,1) };
	INT32 YOffs2[16] = { STEP16(0,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0xa00000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, RomGfx01, 0x20000);

	GfxDecode(0x01000, 4,  8,  8, Plane0, XOffs0, YOffs0, 0x100, tmp, RomGfx01);

	memcpy (tmp, RomGfx02, 0xa00000);

	GfxDecode(0x10000, 5, 16, 16, Plane1, XOffs1, YOffs1, 0x100, tmp, RomGfx02);

	memcpy (tmp, RomGfx03, 0x300000);

	GfxDecode(0x04000, 6, 16, 16, Plane2, XOffs2, YOffs2, 0x200, tmp, RomGfx03);

	BurnFree(tmp);

	return 0;
}

static tilemap_callback( background0 )
{
	INT32 attr = RamBg00[offs * 2 + 0];
	INT32 color = (attr & 0x1f);
	if (color & 0x10) color ^= 0x30;

	TILE_SET_INFO(1, RamBg00[offs * 2 + 1] & 0x3fff, color, TILE_FLIPYX((attr >> 6) & 3));
}

static tilemap_callback( background1 )
{
	INT32 attr = RamBg01[offs];

	TILE_SET_INFO(1, attr & 0xfff, (attr >> 12) + 0x40, 0);
}

static tilemap_callback( foreground )
{
	INT32 attr = RamFg[offs * 2 + 1] & 0xff;

	TILE_SET_INFO(0, (RamFg[offs * 2] & 0xff) + (attr << 8), attr >> 4, 0);
}

static INT32 shadfrceInit()
{
	INT32 nRet;

	Mem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(Mem, 0, nLen);
	MemIndex();	

	{
		nRet = BurnLoadRom(Rom68K + 0x000000, 0, 2); if (nRet != 0) return 1;
		nRet = BurnLoadRom(Rom68K + 0x000001, 1, 2); if (nRet != 0) return 1;
		nRet = BurnLoadRom(Rom68K + 0x080000, 2, 2); if (nRet != 0) return 1;
		nRet = BurnLoadRom(Rom68K + 0x080001, 3, 2); if (nRet != 0) return 1;

		nRet = BurnLoadRom(RomZ80 + 0x000000, 4, 1); if (nRet != 0) return 1;

		nRet = BurnLoadRom(RomGfx01 + 0x000000, 5, 1); if (nRet != 0) return 1;

		nRet = BurnLoadRom(RomGfx02 + 0x000000, 6, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(RomGfx02 + 0x200000, 7, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(RomGfx02 + 0x400000, 8, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(RomGfx02 + 0x600000, 9, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(RomGfx02 + 0x800000, 10, 1); if (nRet != 0) return 1;

		nRet = BurnLoadRom(RomGfx03 + 0x000000, 11, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(RomGfx03 + 0x100000, 12, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(RomGfx03 + 0x200000, 13, 1); if (nRet != 0) return 1;

		nRet = BurnLoadRom(DrvOkiROM, 14, 1); if (nRet != 0) return 1;

		DrvGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Rom68K,			0x000000, 0x0FFFFF, MAP_ROM);
	SekMapMemory((UINT8 *)RamBg00,		0x100000, 0x101FFF, MAP_RAM);
	SekMapMemory((UINT8 *)RamBg01,		0x102000, 0x103FFF, MAP_RAM);
	SekMapMemory((UINT8 *)RamFg,		0x140000, 0x141FFF, MAP_RAM);
	SekMapMemory((UINT8 *)RamSpr,		0x142000, 0x143FFF, MAP_RAM);
	SekMapMemory((UINT8 *)RamPal,		0x180000, 0x187FFF, MAP_ROM);
	SekMapMemory(Ram68K,			0x1F0000, 0x1FFFFF, MAP_RAM);
	SekMapHandler(1,			0x180000, 0x187FFF, MAP_WRITE);
	SekSetReadWordHandler(0, shadfrceReadWord);
	SekSetReadByteHandler(0, shadfrceReadByte);
	SekSetWriteWordHandler(0, shadfrceWriteWord);
	SekSetWriteByteHandler(0, shadfrceWriteByte);
	//SekSetWriteByteHandler(1, shadfrceWriteBytePalette);
	SekSetWriteWordHandler(1, shadfrceWriteWordPalette);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(RomZ80,		0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(RamZ80,		0xc000, 0xc7ff, MAP_RAM);
	ZetMapMemory(RamZ80 + 0x0800,	0xf000, 0xffff, MAP_RAM);	
	ZetSetReadHandler(shadfrceZRead);
	ZetSetWriteHandler(shadfrceZWrite);
	ZetClose();

	BurnYM2151Init(3579545);		// 3.5795 MHz
	YM2151SetIrqHandler(0, &shadfrceYM2151IRQHandler);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.50, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.50, BURN_SND_ROUTE_RIGHT);
	MSM6295Init(0, 12000, 1);		// 12.000 KHz
	MSM6295SetRoute(0, 0.50, BURN_SND_ROUTE_BOTH);
	nZ80Cycles = 3579545 * 100 / nBurnFPS;

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, foreground_map_callback,   8,  8, 64, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, background0_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, background1_map_callback, 16, 16, 32, 32);
	GenericTilemapSetGfx(0, RomGfx01, 6 /*actually 4*/,  8,  8, 0x040000, 0x0000, 0xff);
	GenericTilemapSetGfx(1, RomGfx03, 6, 16, 16, 0x400000, 0x2000, 0x7f);
//	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -8);
	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetTransparent(1, 0);

	DrvDoReset();

	return 0;
}

static INT32 shadfrceExit()
{
	GenericTilesExit();

	MSM6295Exit(0);
	BurnYM2151Exit();

	SekExit();
	ZetExit();

	BurnFree(Mem);

	return 0;
}

static void drawSprites()
{
	UINT16 * finish = SprBuf;
	UINT16 * source = finish + 0x2000/2 - 8;

	while (source>=finish)
	{
		INT32 ypos = 0x100 - (((source[0] & 0x0003) << 8) | (source[1] & 0x00ff));
		INT32 xpos = (((source[4] & 0x0001) << 8) | (source[5] & 0x00ff)) + 1;
		INT32 tile = ((source[2] & 0x00ff) << 8) | (source[3] & 0x00ff);
		INT32 height = ((source[0] & 0x00e0) >> 5) + 1;
		INT32 enable = ((source[0] & 0x0004));
		INT32 flipx = ((source[0] & 0x0010) >> 4);
		INT32 flipy = ((source[0] & 0x0008) >> 3);
		INT32 color = ((source[4] & 0x003e));
		INT32 pri_mask = (source[4] & 0x0040) ? 0x02 : 0x00;

		if (color & 0x20) color ^= 0x60;	// skip hole
		color <<= 5;

		if (enable)
		{
			for (INT32 hcount = 0; hcount < height; hcount++)
			{
				RenderPrioSprite(pTransDraw, RomGfx02, tile+hcount, 0x1000 + color, 0, xpos,       ypos-hcount*16-16,       flipx, flipy, 16, 16, pri_mask);
				RenderPrioSprite(pTransDraw, RomGfx02, tile+hcount, 0x1000 + color, 0, xpos-0x200, ypos-hcount*16-16,       flipx, flipy, 16, 16, pri_mask);
				RenderPrioSprite(pTransDraw, RomGfx02, tile+hcount, 0x1000 + color, 0, xpos,       ypos-hcount*16-16+0x200, flipx, flipy, 16, 16, pri_mask);
				RenderPrioSprite(pTransDraw, RomGfx02, tile+hcount, 0x1000 + color, 0, xpos-0x200, ypos-hcount*16-16+0x200, flipx, flipy, 16, 16, pri_mask);
			}
		}

		source-=8;
	}
}

static void draw_line(INT32 line)
{
	GenericTilesSetScanline(line);

	if (video_enable)
	{
	//	GenericTilemapSetScrollX(1, bg0scrollx);
		GenericTilemapSetScrollRow(1, line, bg0scrollx);
		GenericTilemapSetScrollY(1, bg0scrolly);

		if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 1);
	}
}

static void draw_bg_layer_raster()
{
	GenericTilesClearClip();

	if (video_enable)
	{
		GenericTilemapSetScrollX(2, bg1scrollx);
		GenericTilemapSetScrollY(2, bg1scrolly);

		if (nBurnLayer & 1) GenericTilemapDraw(2, pTransDraw, 0);
	}
	else
	{
		BurnTransferClear();
	}
}

static INT32 shadfrceDraw()
{
	if (bRecalcPalette) {
		for(INT32 i=0;i<0x4000;i++) CalcCol(i);
		bRecalcPalette = 0;
	}

	GenericTilesClearClip();

	if (video_enable)
	{
		if (raster_irq_enable == 0) {
			GenericTilemapSetScrollX(1, bg0scrollx);
			GenericTilemapSetScrollY(1, bg0scrolly);
			GenericTilemapSetScrollX(2, bg1scrollx);
			GenericTilemapSetScrollY(2, bg1scrolly);

			if (nBurnLayer & 1) GenericTilemapDraw(2, pTransDraw, 0);
			if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 1);
		}

		if (nBurnLayer & 4) drawSprites();
	
		if (nBurnLayer & 8) GenericTilemapDraw(0, pTransDraw, 0);
	}
	else
	{
		if (raster_irq_enable == 0) {
			BurnTransferClear();
		}
	}

	BurnTransferCopy(RamCurPal);

	return 0;
}

static INT32 shadfrceFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		DrvInput[0] = 0x00;
		DrvInput[2] = 0x00;
		DrvInput[4] = 0x00;
		for (INT32 i = 0; i < 8; i++) {
			DrvInput[0] |= (DrvJoy1[i] & 1) << i;
			DrvInput[2] |= (DrvJoy2[i] & 1) << i;
			DrvInput[4] |= (DrvButton[i] & 1) << i;
		}
		DrvInput[1] = (DrvInput[1] & 0xFC) | (DrvDipBtn[0] & 1) | ((DrvDipBtn[1] & 1) << 1); 
	}

	SekNewFrame();
	ZetNewFrame();

	INT32 nInterleave = 272;
	INT32 nCyclesTotal[2] = { 14000000 / 60, 3579545 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };
 	INT32 nSoundBufferPos = 0;

	SekOpen(0);
	ZetOpen(0);
 
	bVBlank = 1;

	for (INT32 scanline = 0; scanline < nInterleave; scanline++)
	{
		nCyclesDone[0] += SekRun(nCyclesTotal[0] / nInterleave);

		nCyclesDone[1] += ZetRun(nCyclesTotal[1] / nInterleave);

		if (raster_irq_enable && (scanline == raster_scanline)) {
			raster_scanline = (raster_scanline + 1) % 240;
			if (raster_scanline > 0) {
				SekSetIRQLine(1, CPU_IRQSTATUS_ACK);
			}
		}

		if (irqs_enable) {
			if ((scanline & 0xf) == 0 && (scanline > 0)) {
				SekSetIRQLine(2, CPU_IRQSTATUS_ACK);
			}

			if (scanline == 248) {
				SekSetIRQLine(3, CPU_IRQSTATUS_ACK);
			}
		}

		if (scanline == 247) {
			bVBlank = 0;
		}

		if (scanline == 0 && raster_irq_enable) {
			draw_bg_layer_raster();
		}

		if (scanline < 256 && raster_irq_enable) {
			draw_line(scanline);
		}

		if (pBurnSoundOut) {
			INT32 nSegmentLength = (nBurnSoundLen / nInterleave) - nSoundBufferPos;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos * 2);
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			MSM6295Render(0, pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		BurnYM2151Render(pSoundBuf, nSegmentLength);
		MSM6295Render(0, pSoundBuf, nSegmentLength);
		nSoundBufferPos += nSegmentLength;
	}

	ZetClose();
	SekClose();

	if (pBurnDraw) {
		shadfrceDraw();
	}

	memcpy (SprBuf, RamSpr, 0x2000); // buffer one frame

	return 0;
}

static INT32 shadfrceScan(INT32 nAction,INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin)
		*pnMin =  0x029671;

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = RamStart;
		ba.nLen	  = RamEnd-RamStart;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);
		ZetScan(nAction);

		MSM6295Scan(0, nAction);
		BurnYM2151Scan(nAction);

		SCAN_VAR(DrvInput);
		SCAN_VAR(nBrightness);
		SCAN_VAR(bg0scrollx);
		SCAN_VAR(bg0scrolly);
		SCAN_VAR(bg1scrollx);
		SCAN_VAR(bg1scrolly);
		SCAN_VAR(nSoundlatch);
		SCAN_VAR(video_enable);
		SCAN_VAR(okibank);
	}

	if (nAction & ACB_WRITE) {
		// recalc palette and brightness
		for(INT32 i=0;i<0x4000;i++) CalcCol(i);

		shadfrceZWrite(0xe800, okibank); // set oki bank
	}

	return 0;
}


// Shadow Force (World, Version 3)

static struct BurnRomInfo shadfrceRomDesc[] = {
	{ "32a12-011.34", 0x040000, 0x0c041e08, BRF_ESS | BRF_PRG },	// 68000 code 
	{ "32a13-010.26", 0x040000, 0x00985361, BRF_ESS | BRF_PRG }, 
	{ "32a14-010.33", 0x040000, 0xea03ca25, BRF_ESS | BRF_PRG }, 
	{ "32j15-01.14",  0x040000, 0x3dc3a84a, BRF_ESS | BRF_PRG }, 

	{ "32j10-0.42",   0x010000, 0x65daf475, BRF_ESS | BRF_PRG },	// Z80 code

	{ "32j11-0.55",   0x020000, 0x7252d993, BRF_GRA }, 		// gfx 1 chars

	{ "32j4-0.12", 	  0x200000, 0x1ebea5b6, BRF_GRA }, 		// gfx 2 sprite
	{ "32j5-0.13",	  0x200000, 0x600026b5, BRF_GRA }, 
	{ "32j6-0.24", 	  0x200000, 0x6cde8ebe, BRF_GRA }, 
	{ "32j7-0.25",	  0x200000, 0xbcb37922, BRF_GRA }, 
	{ "32j8-0.32",	  0x200000, 0x201bebf6, BRF_GRA }, 

	{ "32j1-0.4",     0x100000, 0xf1cca740, BRF_GRA },		// gfx 3 bg
	{ "32j2-0.5",     0x100000, 0x5fac3e01, BRF_GRA },			
	{ "32j3-0.6",     0x100000, 0xd297925e, BRF_GRA },

	{ "32j9-0.76",    0x080000, 0x16001e81, BRF_SND },		// PCM
};

STD_ROM_PICK(shadfrce)
STD_ROM_FN(shadfrce)

struct BurnDriver BurnDrvShadfrce = {
	"shadfrce", NULL, NULL, NULL, "1993",
	"Shadow Force (World, Version 3)\0", NULL, "Technos Japan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TECHNOS, GBF_SCRFIGHT, 0,
	NULL, shadfrceRomInfo, shadfrceRomName, NULL, NULL, shadfrceInputInfo, shadfrceDIPInfo,
	shadfrceInit, shadfrceExit, shadfrceFrame, shadfrceDraw, shadfrceScan, &bRecalcPalette, 0x4000,
	320, 256, 4, 3
};


// Shadow Force (US, Version 2)

static struct BurnRomInfo shadfrceuRomDesc[] = {
	{ "32a12-01.34",  0x040000, 0x04501198, BRF_ESS | BRF_PRG },	// 68000 code 
	{ "32a13-01.26",  0x040000, 0xb8f8a05c, BRF_ESS | BRF_PRG }, 
	{ "32a14-0.33",   0x040000, 0x08279be9, BRF_ESS | BRF_PRG }, 
	{ "32a15-0.14",	  0x040000, 0xbfcadfea, BRF_ESS | BRF_PRG }, 

	{ "32j10-0.42",   0x010000, 0x65daf475, BRF_ESS | BRF_PRG },	// Z80 code

	{ "32a11-0.55",	  0x020000, 0xcfaf5e77, BRF_GRA }, 		// gfx 1 chars

	{ "32j4-0.12", 	  0x200000, 0x1ebea5b6, BRF_GRA }, 		// gfx 2 sprite
	{ "32j5-0.13",	  0x200000, 0x600026b5, BRF_GRA }, 
	{ "32j6-0.24", 	  0x200000, 0x6cde8ebe, BRF_GRA }, 
	{ "32j7-0.25",	  0x200000, 0xbcb37922, BRF_GRA }, 
	{ "32j8-0.32",	  0x200000, 0x201bebf6, BRF_GRA }, 

	{ "32j1-0.4",     0x100000, 0xf1cca740, BRF_GRA },		// gfx 3 bg
	{ "32j2-0.5",     0x100000, 0x5fac3e01, BRF_GRA },			
	{ "32j3-0.6",     0x100000, 0xd297925e, BRF_GRA },

	{ "32j9-0.76",    0x080000, 0x16001e81, BRF_SND },		// PCM
};

STD_ROM_PICK(shadfrceu)
STD_ROM_FN(shadfrceu)

struct BurnDriver BurnDrvShadfrceu = {
	"shadfrceu", "shadfrce", NULL, NULL, "1993",
	"Shadow Force (US, Version 2)\0", NULL, "Technos Japan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TECHNOS, GBF_SCRFIGHT, 0,
	NULL, shadfrceuRomInfo, shadfrceuRomName, NULL, NULL, shadfrceInputInfo, shadfrceDIPInfo,
	shadfrceInit, shadfrceExit, shadfrceFrame, shadfrceDraw, shadfrceScan, &bRecalcPalette, 0x4000,
	320, 256, 4, 3
};


// Shadow Force (Japan, Version 2)

static struct BurnRomInfo shadfrcejRomDesc[] = {
	{ "32j12-01.34",  0x040000, 0x38fdbe1d, BRF_ESS | BRF_PRG },	// 68000 code 
	{ "32j13-01.26",  0x040000, 0x6e1df6f1, BRF_ESS | BRF_PRG }, 
	{ "32j14-01.33",  0x040000, 0x89e3fb60, BRF_ESS | BRF_PRG }, 
	{ "32j15-01.14",  0x040000, 0x3dc3a84a, BRF_ESS | BRF_PRG }, 

	{ "32j10-0.42",   0x010000, 0x65daf475, BRF_ESS | BRF_PRG },	// Z80 code

	{ "32j11-0.55",   0x020000, 0x7252d993, BRF_GRA }, 		// gfx 1 chars

	{ "32j4-0.12", 	  0x200000, 0x1ebea5b6, BRF_GRA }, 		// gfx 2 sprite
	{ "32j5-0.13",	  0x200000, 0x600026b5, BRF_GRA }, 
	{ "32j6-0.24", 	  0x200000, 0x6cde8ebe, BRF_GRA }, 
	{ "32j7-0.25",	  0x200000, 0xbcb37922, BRF_GRA }, 
	{ "32j8-0.32",	  0x200000, 0x201bebf6, BRF_GRA }, 

	{ "32j1-0.4",     0x100000, 0xf1cca740, BRF_GRA },		// gfx 3 bg
	{ "32j2-0.5",     0x100000, 0x5fac3e01, BRF_GRA },			
	{ "32j3-0.6",     0x100000, 0xd297925e, BRF_GRA },

	{ "32j9-0.76",    0x080000, 0x16001e81, BRF_SND },		// PCM
};

STD_ROM_PICK(shadfrcej)
STD_ROM_FN(shadfrcej)

struct BurnDriver BurnDrvShadfrcej = {
	"shadfrcej", "shadfrce", NULL, NULL, "1993",
	"Shadow Force (Japan, Version 2)\0", NULL, "Technos Japan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TECHNOS, GBF_SCRFIGHT, 0,
	NULL, shadfrcejRomInfo, shadfrcejRomName, NULL, NULL, shadfrceInputInfo, shadfrceDIPInfo,
	shadfrceInit, shadfrceExit, shadfrceFrame, shadfrceDraw, shadfrceScan, &bRecalcPalette, 0x4000,
	320, 256, 4, 3
};
