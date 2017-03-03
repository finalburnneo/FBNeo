// FB Alpha Traverse USA driver module
// Based on MAME driver by Lee Taylor (with thanks to John Clegg and Tomasz Slanina)

#include "tiles_generic.h"
#include "z80_intf.h"
#include "m6800_intf.h"
#include "msm5205.h"
#include "driver.h"
#include "bitswap.h"
extern "C" {
#include "ay8910.h"
}

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvSndROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvTransTable[2];
static UINT8 *DrvColPROM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvZ80RAM;
static UINT8 *M62M6803Ram;
static UINT8 *DrvSprRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT16 *pAY8910Buffer[6];

static UINT8 m6803_port1;
static UINT8 m6803_port2;
static UINT8 flipscreen;
static UINT8 soundlatch;
static UINT16 scrollx;

static UINT32 YFlipping = 0; // shtrider has a weird screen layout

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvInputs[5];
static UINT8 DrvReset;

static struct BurnInputInfo TravrusaInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 7,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvInputs + 3,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvInputs + 4,	"dip"		},
};

STDINPUTINFO(Travrusa)

static struct BurnInputInfo ShtriderInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvInputs + 3,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvInputs + 4,	"dip"		},
};

STDINPUTINFO(Shtrider)

static struct BurnDIPInfo TravrusaDIPList[]=
{
	{0x0d, 0xff, 0xff, 0xf7, NULL				},
	{0x0e, 0xff, 0xff, 0xfd, NULL				},

	{0   , 0xfe, 0   ,    4, "Fuel Reduced on Collision"	},
	{0x0d, 0x01, 0x03, 0x03, "Low"				},
	{0x0d, 0x01, 0x03, 0x02, "Med"				},
	{0x0d, 0x01, 0x03, 0x01, "Hi"				},
	{0x0d, 0x01, 0x03, 0x00, "Max"				},

	{0   , 0xfe, 0   ,    2, "Fuel Consumption"		},
	{0x0d, 0x01, 0x04, 0x04, "Low"				},
	{0x0d, 0x01, 0x04, 0x00, "Hi"				},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x0d, 0x01, 0x08, 0x08, "No"				},
	{0x0d, 0x01, 0x08, 0x00, "Yes"				},

	{0   , 0xfe, 0   ,    32, "Coinage"			},
	{0x0d, 0x01, 0xf0, 0x80, "Not Used"			},
	{0x0d, 0x01, 0xf0, 0x90, "Not Used"			},
	{0x0d, 0x01, 0xf0, 0xa0, "6 Coins 1 Credits"		},
	{0x0d, 0x01, 0xf0, 0xb0, "5 Coins 1 Credits"		},
	{0x0d, 0x01, 0xf0, 0xc0, "4 Coins 1 Credits"		},
	{0x0d, 0x01, 0xf0, 0xd0, "3 Coins 1 Credits"		},
	{0x0d, 0x01, 0xf0, 0xe0, "2 Coins 1 Credits"		},
	{0x0d, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x0d, 0x01, 0xf0, 0x70, "1 Coin  2 Credits"		},
	{0x0d, 0x01, 0xf0, 0x60, "1 Coin  3 Credits"		},
	{0x0d, 0x01, 0xf0, 0x50, "1 Coin  4 Credits"		},
	{0x0d, 0x01, 0xf0, 0x40, "1 Coin  5 Credits"		},
	{0x0d, 0x01, 0xf0, 0x30, "1 Coin  6 Credits"		},
	{0x0d, 0x01, 0xf0, 0x20, "1 Coin  7 Credits"		},
	{0x0d, 0x01, 0xf0, 0x10, "Not Used"			},
	{0x0d, 0x01, 0xf0, 0x00, "Free Play"			},
	{0x0d, 0x01, 0xf0, 0x80, "Free Play"			},
	{0x0d, 0x01, 0xf0, 0x90, "A 3C 1C / B 1C 3C"		},
	{0x0d, 0x01, 0xf0, 0xa0, "A 2C 1C / B 1C 3C"		},
	{0x0d, 0x01, 0xf0, 0xb0, "A 1C 1C / B 1C 3C"		},
	{0x0d, 0x01, 0xf0, 0xc0, "Free Play"			},
	{0x0d, 0x01, 0xf0, 0xd0, "A 3C 1C / B 1C 2C"		},
	{0x0d, 0x01, 0xf0, 0xe0, "A 2C 1C / B 1C 2C"		},
	{0x0d, 0x01, 0xf0, 0xf0, "A 1C 1C / B 1C 2C"		},
	{0x0d, 0x01, 0xf0, 0x70, "A 1C 1C / B 1C 5C"		},
	{0x0d, 0x01, 0xf0, 0x60, "A 2C 1C / B 1C 5C"		},
	{0x0d, 0x01, 0xf0, 0x50, "A 3C 1C / B 1C 5C"		},
	{0x0d, 0x01, 0xf0, 0x40, "Free Play"			},
	{0x0d, 0x01, 0xf0, 0x30, "A 1C 1C / B 1C 6C"		},
	{0x0d, 0x01, 0xf0, 0x20, "A 2C 1C / B 1C 6C"		},
	{0x0d, 0x01, 0xf0, 0x10, "A 3C 1C / B 1C 6C"		},
	{0x0d, 0x01, 0xf0, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x0e, 0x01, 0x01, 0x01, "Off"				},
	{0x0e, 0x01, 0x01, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x0e, 0x01, 0x02, 0x00, "Upright"			},
	{0x0e, 0x01, 0x02, 0x02, "Cocktail"			},

	{0   , 0xfe, 0   ,    2, "Coin Mode"			},
	{0x0e, 0x01, 0x04, 0x04, "Mode 1"			},
	{0x0e, 0x01, 0x04, 0x00, "Mode 2"			},

	{0   , 0xfe, 0   ,    2, "Speed Type"			},
	{0x0e, 0x01, 0x08, 0x08, "M/H"				},
	{0x0e, 0x01, 0x08, 0x00, "Km/H"				},

	{0   , 0xfe, 0   ,    2, "Stop Mode (Cheat)"		},
	{0x0e, 0x01, 0x10, 0x10, "Off"				},
	{0x0e, 0x01, 0x10, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Title"			},
	{0x0e, 0x01, 0x20, 0x20, "Traverse USA"			},
	{0x0e, 0x01, 0x20, 0x00, "Zippy Race"			},

	{0   , 0xfe, 0   ,    2, "Invulnerability (Cheat)"	},
	{0x0e, 0x01, 0x40, 0x40, "Off"				},
	{0x0e, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x0e, 0x01, 0x80, 0x80, "Off"				},
	{0x0e, 0x01, 0x80, 0x00, "On"				},
};

STDDIPINFO(Travrusa)

static struct BurnDIPInfo ShtriderDIPList[]=
{
	{0x0d, 0xff, 0xff, 0xff, NULL				},
	{0x0e, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x0d, 0x01, 0x07, 0x00, "5 Coins 1 Credits"		},
	{0x0d, 0x01, 0x07, 0x04, "4 Coins 1 Credits"		},
	{0x0d, 0x01, 0x07, 0x02, "3 Coins 1 Credits"		},
	{0x0d, 0x01, 0x07, 0x06, "2 Coins 1 Credits"		},
	{0x0d, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x0d, 0x01, 0x07, 0x03, "1 Coin  2 Credits"		},
	{0x0d, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x0d, 0x01, 0x07, 0x01, "1 Coin  5 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"			},
	{0x0d, 0x01, 0x18, 0x00, "3 Coins 1 Credits"		},
	{0x0d, 0x01, 0x18, 0x10, "2 Coins 1 Credits"		},
	{0x0d, 0x01, 0x18, 0x18, "1 Coin  1 Credits"		},
	{0x0d, 0x01, 0x18, 0x08, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x0e, 0x01, 0x01, 0x01, "Off"				},
	{0x0e, 0x01, 0x01, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Speed Display"		},
	{0x0e, 0x01, 0x02, 0x02, "km/h"				},
	{0x0e, 0x01, 0x02, 0x00, "mph"				},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x0e, 0x01, 0x08, 0x08, "Upright"			},
	{0x0e, 0x01, 0x08, 0x00, "Cocktail"			},
};

STDDIPINFO(Shtrider)

static void __fastcall travrusa_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x9000:
			scrollx = (scrollx & 0x0100) | data;
		return;

		case 0xa000:
			scrollx = (scrollx & 0x00ff) | ((data << 8) & 0x100);
		return;

		case 0xd000:
		{
			if (data & 0x80) {
				M6803SetIRQLine(0, CPU_IRQSTATUS_ACK);
			} else {
				soundlatch = data & 0x7f;
			}
		}
		return;

		case 0xd001:
			flipscreen = (data & 1) ^ ((DrvInputs[4] & 1) ^ 1);
		return;
	}
}

static UINT8 __fastcall travrusa_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xd000:
		case 0xd001:
		case 0xd002:
		case 0xd003:
		case 0xd004:
			return DrvInputs[address & 7];
	}

	return 0;
}

static UINT8 __fastcall travrusa_main_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x11:
			return 0x88; // shtriderb
	}

	return 0;
}

static UINT8 m6803_read(UINT16 address)
{
	address &= 0x7fff;

	if (address < 0x20) {
		return m6803_internal_registers_r(address);
	}
	
	if (address >= 0x0080 && address <= 0x00ff) {
		return M62M6803Ram[address & 0x7f];
	}

	if (address >= 0x7000 && address <= 0x7fff) {
		return DrvSndROM[address - 0x7000];
	}

	return 0;
}

static void m6803_write(UINT16 address, UINT8 data)
{
	address &= 0x7fff;

	if (address < 0x20) {
		m6803_internal_registers_w(address, data);
		return;
	}

	if (address >= 0x0080 && address <= 0x00ff) {
		M62M6803Ram[address & 0x7f] = data;
		return;
	}

	if (address < 0x80) return;

	if ((address & 0x7001) == 0x0001) {
		MSM5205DataWrite(0, data);
		return;
	}

	if ((address & 0x7000) == 0x1000) {
		M6803SetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;
	}
}

static void m6803_write_port(UINT16 port, UINT8 data)
{
	switch (port)
	{
		case M6803_PORT1:
			m6803_port1 = data;
		return;

		case M6803_PORT2:
		{
			if ((m6803_port2 & 0x01) && !(data & 0x01))
			{
				if (m6803_port2 & 0x04)
				{
					if (m6803_port2 & 0x08)
						AY8910Write(0, 0, m6803_port1);

					if (m6803_port2 & 0x10)
						AY8910Write(1, 0, m6803_port1);
				}
				else
				{
					if (m6803_port2 & 0x08)
						AY8910Write(0, 1, m6803_port1);

					if (m6803_port2 & 0x10)
						AY8910Write(1, 1, m6803_port1);
				}
			}

			m6803_port2 = data;
		}			
		return;
	}
}

static UINT8 m6803_read_port(UINT16 port)
{
	switch (port)
	{
		case M6803_PORT1:
			if (m6803_port2 & 0x08)
				return AY8910Read(0);

			if (m6803_port2 & 0x10)
				return AY8910Read(1);
			return 0xff;

		case M6803_PORT2:
			return 0;
	}

	return 0;
}

static UINT8 ay8910_0_read_A(UINT32)
{
	return soundlatch;
}

static void ay8910_0_write_B(UINT32, UINT32 data)
{
	MSM5205PlaymodeWrite(0, (data >> 2) & 7);
	MSM5205ResetWrite(0, data & 1);
}

static void ay8910_1_write_B(UINT32, UINT32 /*data*/)
{
//	if (m_audio_BD) m_audio_BD->write_line(data & 0x01 ? 1: 0);
//	if (m_audio_SD) m_audio_SD->write_line(data & 0x02 ? 1: 0);
//	if (m_audio_OH) m_audio_OH->write_line(data & 0x04 ? 1: 0);
//	if (m_audio_CH) m_audio_CH->write_line(data & 0x08 ? 1: 0);
}

static void adpcm_int()
{
	M6803SetIRQLine(M6800_INPUT_LINE_NMI, CPU_IRQSTATUS_AUTO); // adpcm interrupt
}

static INT32 DrvSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)(double)M6800TotalCycles() * nSoundRate / 3579545;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

//	M6803Open(0);
	M6803Reset();
//	M6803Close();

	AY8910Reset(0);
	AY8910Reset(1);
	MSM5205Reset();

	m6803_port1 = 0;
	m6803_port2 = 0;
	flipscreen = 0;
	soundlatch = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x008000;
	DrvSndROM		= Next; Next += 0x002000;

	DrvGfxROM0		= Next; Next += 0x010000;
	DrvGfxROM1		= Next; Next += 0x010000;

	DrvTransTable[0]	= Next; Next += 0x000100;
	DrvTransTable[1]	= Next; Next += 0x000080;

	DrvPalette		= (UINT32*)Next; Next += 0x0100 * sizeof(UINT32);

	DrvColPROM		= Next; Next += 0x000400;

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x001000;
	M62M6803Ram     = Next; Next += 0x000100;
	DrvVidRAM		= Next; Next += 0x001000;
	DrvSprRAM		= Next; Next += 0x000200;

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

static INT32 DrvGfxDecode(INT32 type)
{
	INT32 Plane[3]   = { (0x2000*2*8), (0x2000*1*8), (0x2000*0*8) };
	INT32 XOffs0[16] = { STEP8(0,1), STEP8(16*8,1) };
	INT32 YOffs0[16] = { STEP16(0,8) };
	INT32 XOffs1[16] = { STEP8(0,1), STEP8(8*8,1) };
	INT32 YOffs1[16] = { STEP8(0,8), STEP8(16*8,8) };

	INT32 *Xptr = (type) ? XOffs1 : XOffs0;
	INT32 *Yptr = (type) ? YOffs1 : YOffs0;

	UINT8 *tmp = (UINT8*)BurnMalloc(0x6000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x6000);

	GfxDecode(0x0400, 3, 8, 8, Plane, XOffs0, YOffs0, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x6000);

	GfxDecode(0x0100, 3, 16, 16, Plane, Xptr, Yptr, 0x100, tmp, DrvGfxROM1);

	BurnFree(tmp);

	return 0;
}

static void shtrider_palette_decode()
{
	for (INT32 i = 0; i < 0x80; i++) {
		DrvColPROM[i] <<= 4;
		DrvColPROM[i] |= DrvColPROM[i+0x100];
	}
}

static void motoraceDecode()
{
	UINT8 *tmp = (UINT8*)BurnMalloc(0x2000);

	memcpy (tmp, DrvZ80ROM, 0x2000);

	for (INT32 A = 0; A < 0x2000; A++)
	{
		INT32 j = BITSWAP16(A,15,14,13,9,7,5,3,1,12,10,8,6,4,2,0,11);
		DrvZ80ROM[j] = BITSWAP08(tmp[A],2,7,4,1,6,3,0,5);
	}

	BurnFree(tmp);
}

static void shtrideraDecode()
{
	for (INT32 A = 0; A < 0x2000; A++)
	{
		DrvZ80ROM[A] = BITSWAP08(DrvZ80ROM[A],7,5,6,3,4,2,1,0);
	}
}

static void build_transtables()
{
	for (INT32 i = 0; i < 0x80; i++) {
		DrvTransTable[0][i] = 1; // opaque
		DrvTransTable[1][i] = (0xc0 >> (i & 7)) & 1;
		DrvTransTable[0][i+0x80] = DrvColPROM[0x80+(DrvColPROM[0x200+i]&0xf)] ? 1 : 0;
	}
}

static tilemap_callback( layer0 )
{
	INT32 attr = DrvVidRAM[offs * 2 + 1];
	INT32 code = DrvVidRAM[offs * 2 + 0] | ((attr << 2) & 0x300);

	INT32 color = attr & 0xf;

	TILE_SET_INFO(0, code, color, TILE_FLIPXY((attr & 0x30) >> 4));
}

static tilemap_callback( layer1 )
{
	INT32 attr = DrvVidRAM[offs * 2 + 1];
	INT32 code = DrvVidRAM[offs * 2 + 0] | ((attr << 2) & 0x300);

	INT32 color = attr & 0xf;

	INT32 flags = TILE_FLIPXY((attr & 0x30) >> 4);
	if (color == 0xf) flags |= TILE_GROUP(1);

	TILE_SET_INFO(0, code, color, flags);
}


static INT32 DrvInit(void (*pRomCallback)(), INT32 sndromsmall, INT32 gfxtype)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM  + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x2000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x4000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x6000,  3, 1)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x0000,  4, 1)) return 1;
		if (sndromsmall) memcpy (DrvSndROM + 0x1000, DrvSndROM + 0x0000, 0x1000);

		if (BurnLoadRom(DrvGfxROM0 + 0x0000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x2000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x4000,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x0000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x2000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x4000, 10, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x0000, 11, 1)) return 1;

		if (gfxtype) {
			if (BurnLoadRom(DrvColPROM + 0x0100, 12, 1)) return 1;
			if (BurnLoadRom(DrvColPROM + 0x0080, 13, 1)) return 1;
			if (BurnLoadRom(DrvColPROM + 0x0200, 14, 1)) return 1;

			shtrider_palette_decode();
		} else {
			if (BurnLoadRom(DrvColPROM + 0x0080, 12, 1)) return 1;
			if (BurnLoadRom(DrvColPROM + 0x0200, 13, 1)) return 1;
		}

		if (pRomCallback) {
			pRomCallback();
		}

		DrvGfxDecode(gfxtype);
		build_transtables();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvVidRAM,		0x8000, 0x8fff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xc800, 0xc9ff, MAP_WRITE);
	ZetMapMemory(DrvZ80RAM,		0xe000, 0xefff, MAP_RAM);
	ZetSetWriteHandler(travrusa_main_write);
	ZetSetReadHandler(travrusa_main_read);
	ZetSetInHandler(travrusa_main_read_port);
	ZetClose();

	M6803Init(1);
//	M6803Open(0);
	M6803MapMemory(DrvSndROM,	0x6000, 0x7fff, MAP_ROM);
	M6803MapMemory(DrvSndROM,	0xe000, 0xffff, MAP_ROM);
	M6803SetWriteHandler(m6803_write);
	M6803SetReadHandler(m6803_read);
	M6803SetWritePortHandler(m6803_write_port);
	M6803SetReadPortHandler(m6803_read_port);
//	M6803Close();

	MSM5205Init(0, DrvSynchroniseStream, 384000, adpcm_int, MSM5205_S96_4B, 1);
	MSM5205SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	AY8910Init(0, 3579545/4, nBurnSoundRate, &ay8910_0_read_A, NULL, NULL, &ay8910_0_write_B);
	AY8910Init(1, 3579545/4, nBurnSoundRate, NULL, NULL, NULL, &ay8910_1_write_B);
	AY8910SetAllRoutes(0, 0.15, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.15, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, layer0_map_callback, 8, 8, 64, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, layer1_map_callback, 8, 8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 3, 8, 8, 0x10000, 0, 0xf);
	GenericTilemapSetGfx(1, DrvGfxROM0, 3, 8, 8, 0x10000, 0, 0xf); // this needed? -dink
	GenericTilemapSetScrollRows(0, 4);
	GenericTilemapSetScrollRows(1, 4);
	GenericTilemapSetTransMask(1, 0x3f);
	GenericTilemapSetScrollRow(0, 3, 0);
	GenericTilemapSetScrollRow(1, 3, 0);
	GenericTilemapSetOffsets(0, -8, 0);
	GenericTilemapSetOffsets(1, -8, 0);
	if (YFlipping) GenericTilemapSetFlip(TMAP_GLOBAL, TMAP_FLIPY);
	DrvDoReset();

	return 0;
}

static INT32 travrusaInit() { return DrvInit(NULL, 1, 0); }
static INT32 motoraceInit() { return DrvInit(motoraceDecode, 1, 0); }
static INT32 shtriderInit() { YFlipping = 1; return DrvInit(NULL, 0, 1); }
static INT32 shtrideraInit() { YFlipping = 1; return DrvInit(shtrideraDecode, 0, 1); }
static INT32 shtriderbInit() { YFlipping = 1; return DrvInit(NULL, 0, 0); }

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	M6803Exit();

	AY8910Exit(0);
	AY8910Exit(1);
	MSM5205Exit();

	BurnFree(AllMem);

	YFlipping = 0;

	return 0;
}

static void DrvPaletteInit()
{
	UINT32 tmp[0x10];
	UINT8 *color_prom = DrvColPROM;

	for (INT32 i = 0; i < 0x90; i++)
	{
		INT32 bit0 = 0;
		INT32 bit1 = (color_prom[i] >> 6) & 0x01;
		INT32 bit2 = (color_prom[i] >> 7) & 0x01;
		INT32 r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = (color_prom[i] >> 3) & 0x01;
		bit1 = (color_prom[i] >> 4) & 0x01;
		bit2 = (color_prom[i] >> 5) & 0x01;
		INT32 g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = (color_prom[i] >> 0) & 0x01;
		bit1 = (color_prom[i] >> 1) & 0x01;
		bit2 = (color_prom[i] >> 2) & 0x01;
		INT32 b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		if (i < 0x80)
			DrvPalette[i] = BurnHighCol(r,g,b,0);
		else
			tmp[i-0x80] = BurnHighCol(r,g,b,0);
	}

	for (INT32 i = 0x80; i < 0x100; i++)
	{
		DrvPalette[i] = tmp[DrvColPROM[i + 0x180] & 0x0f];
	}
}

static void draw_sprites()
{
	if (YFlipping) {
		GenericTilesSetClip(0,240,64,256); // shtrider
	} else {
		GenericTilesSetClip(0,240,0,192); // everything else
	}

	for (INT32 offs = 0x200 - 4; offs >= 0; offs -= 4)
	{
		int sx = ((DrvSprRAM[offs + 3] + 8) & 0xff) - 16;
		int sy = 240 - DrvSprRAM[offs];
		int code = DrvSprRAM[offs + 2];
		int attr = DrvSprRAM[offs + 1];
		int flipx = attr & 0x40;
		int flipy = attr & 0x80;
		int color = attr & 0x0f;

		if (YFlipping) {
			sy = 240 - sy;
			flipy = !flipy;
		}

		RenderTileTranstab(pTransDraw, DrvGfxROM1, code, (color<<3)+0x80, 0, sx, sy, flipx, flipy, 16, 16, DrvTransTable[0]);
	}

	GenericTilesClearClip();
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	BurnTransferClear();

	for (INT32 i = 0; i < 3; i++) {
		GenericTilemapSetScrollRow(0, i, scrollx);
		GenericTilemapSetScrollRow(1, i, scrollx);
	}

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, TMAP_FORCEOPAQUE);

	if (nSpriteEnable & 1) draw_sprites();

	if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, TMAP_SET_GROUP(1));

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	M6803NewFrame();
	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 3); // dips 3,4!

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = MSM5205CalcInterleave(0, 3579545);
	INT32 nCyclesTotal[2] = { 4000000 / 60, 3579545 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	ZetOpen(0);
//	M6803Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);
		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);

		nCyclesDone[1] += M6803Run(nCyclesTotal[1] / nInterleave);
		MSM5205Update(); // adpcm update samples
	}

	if (pBurnSoundOut) {
		AY8910Render(&pAY8910Buffer[0], pBurnSoundOut, nBurnSoundLen, 0);
		MSM5205Render(0, pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
//	M6803Close();

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
		M6800Scan(nAction);

		AY8910Scan(nAction, pnMin);
		MSM5205Scan(nAction, pnMin);

		SCAN_VAR(m6803_port1);
		SCAN_VAR(m6803_port2);
		SCAN_VAR(flipscreen);
		SCAN_VAR(soundlatch);
		SCAN_VAR(scrollx);
	}

	return 0;
}


// Traverse USA / Zippy Race

static struct BurnRomInfo travrusaRomDesc[] = {
	{ "zr1-0.m3",		0x2000, 0xbe066c0a, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "zr1-5.l3",		0x2000, 0x145d6b34, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "zr1-6a.k3",		0x2000, 0xe1b51383, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "zr1-7.j3",		0x2000, 0x85cd1a51, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "mr10.1a",		0x1000, 0xa02ad8a0, 2 | BRF_GRA },           //  4 M6803 Code

	{ "zippyrac.001",	0x2000, 0xaa8994dd, 3 | BRF_GRA },           //  5 Background tiles
	{ "mr8.3c",		0x2000, 0x3a046dd1, 3 | BRF_GRA },           //  6
	{ "mr9.3a",		0x2000, 0x1cc3d3f4, 3 | BRF_GRA },           //  7

	{ "zr1-8.n3",		0x2000, 0x3e2c7a6b, 4 | BRF_GRA },           //  8 Sprite tiles
	{ "zr1-9.l3",		0x2000, 0x13be6a14, 4 | BRF_GRA },           //  9
	{ "zr1-10.k3",		0x2000, 0x6fcc9fdb, 4 | BRF_GRA },           // 10

	{ "mmi6349.ij",		0x0200, 0xc9724350, 5 | BRF_GRA },           // 11 Color data
	{ "tbp18s.2",		0x0020, 0xa1130007, 5 | BRF_GRA },           // 12
	{ "tbp24s10.3",		0x0100, 0x76062638, 5 | BRF_GRA },           // 13
};

STD_ROM_PICK(travrusa)
STD_ROM_FN(travrusa)

struct BurnDriver BurnDrvTravrusa = {
	"travrusa", NULL, NULL, NULL, "1983",
	"Traverse USA / Zippy Race\0", NULL, "Irem", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, travrusaRomInfo, travrusaRomName, NULL, NULL, TravrusaInputInfo, TravrusaDIPInfo,
	travrusaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 240, 3, 4
};


// Traverse USA (bootleg)

static struct BurnRomInfo travrusabRomDesc[] = {
	{ "at4.m3",		0x2000, 0x704ce6e4, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "at5.l3",		0x2000, 0x686cb0e6, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "at6.k3",		0x2000, 0xbaf87d80, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "at7.h3",		0x2000, 0x48091ebe, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "11.a1",		0x1000, 0xd2c0bc33, 2 | BRF_GRA },           //  4 M6803 Code

	{ "zippyrac.001",	0x2000, 0xaa8994dd, 3 | BRF_GRA },           //  5 Background tiles
	{ "mr8.3c",		0x2000, 0x3a046dd1, 3 | BRF_GRA },           //  6
	{ "mr9.3a",		0x2000, 0x1cc3d3f4, 3 | BRF_GRA },           //  7

	{ "8.n3",		0x2000, 0x00c0f46b, 4 | BRF_GRA },           //  8 Sprite tiles
	{ "9.m3",		0x2000, 0x73ade73b, 4 | BRF_GRA },           //  9
	{ "10.k3",		0x2000, 0xfcfeaa69, 4 | BRF_GRA },           // 10

	{ "mmi6349.ij",		0x0200, 0xc9724350, 5 | BRF_GRA },           // 11 Color data
	{ "tbp18s.2",		0x0020, 0xa1130007, 5 | BRF_GRA },           // 12
	{ "tbp24s10.3",		0x0100, 0x76062638, 5 | BRF_GRA },           // 13
};

STD_ROM_PICK(travrusab)
STD_ROM_FN(travrusab)

struct BurnDriver BurnDrvTravrusab = {
	"travrusab", "travrusa", NULL, NULL, "1983",
	"Traverse USA (bootleg)\0", NULL, "bootleg (I.P.)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, travrusabRomInfo, travrusabRomName, NULL, NULL, TravrusaInputInfo, TravrusaDIPInfo,
	travrusaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 240, 3, 4
};


// MotoRace USA

static struct BurnRomInfo motoraceRomDesc[] = {
	{ "mr.cpu",		0x2000, 0x89030b0c, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "mr1.3l",		0x2000, 0x0904ed58, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mr2.3k",		0x2000, 0x8a2374ec, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "mr3.3j",		0x2000, 0x2f04c341, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "mr10.1a",		0x1000, 0xa02ad8a0, 2 | BRF_GRA },           //  4 M6803 Code

	{ "mr7.3e",		0x2000, 0x492a60be, 3 | BRF_GRA },           //  5 Background tiles
	{ "mr8.3c",		0x2000, 0x3a046dd1, 3 | BRF_GRA },           //  6
	{ "mr9.3a",		0x2000, 0x1cc3d3f4, 3 | BRF_GRA },           //  7

	{ "mr4.3n",		0x2000, 0x5cf1a0d6, 4 | BRF_GRA },           //  8 Sprite tiles
	{ "mr5.3m",		0x2000, 0xf75f2aad, 4 | BRF_GRA },           //  9
	{ "mr6.3k",		0x2000, 0x518889a0, 4 | BRF_GRA },           // 10

	{ "mmi6349.ij",		0x0200, 0xc9724350, 5 | BRF_GRA },           // 11 Color data
	{ "tbp18s.2",		0x0020, 0xa1130007, 5 | BRF_GRA },           // 12
	{ "tbp24s10.3",		0x0100, 0x76062638, 5 | BRF_GRA },           // 13
};

STD_ROM_PICK(motorace)
STD_ROM_FN(motorace)

struct BurnDriver BurnDrvMotorace = {
	"motorace", "travrusa", NULL, NULL, "1983",
	"MotoRace USA\0", NULL, "Irem (Williams license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, motoraceRomInfo, motoraceRomName, NULL, NULL, TravrusaInputInfo, TravrusaDIPInfo,
	motoraceInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 240, 3, 4
};


// MotoTour / Zippy Race (Tecfri license)

static struct BurnRomInfo mototourRomDesc[] = {
	{ "mt1-4.m3",		0x2000, 0xfe643567, 1 | BRF_PRG | BRF_ESS }, //  0  Z80 Code
	{ "mt1-5.l3",		0x2000, 0x38d9d0f5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mt1-6.k3",		0x2000, 0xefd325f2, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "mt1-7.j3",		0x2000, 0xab8a3a33, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "snd.a1",		0x1000, 0xa02ad8a0, 2 | BRF_GRA },           //  4 M6803 Code

	{ "mt1-1.e3",		0x2000, 0xaa8994dd, 3 | BRF_GRA },           //  5 Background tiles
	{ "mt1-2.c3",		0x2000, 0x3a046dd1, 3 | BRF_GRA },           //  6
	{ "mt1-3.a3",		0x2000, 0x1cc3d3f4, 3 | BRF_GRA },           //  7

	{ "mt1-8..n3",		0x2000, 0x600a57f5, 4 | BRF_GRA },           //  8 Sprite tiles
	{ "mt1-9..m3",		0x2000, 0x6f9f2a4e, 4 | BRF_GRA },           //  9
	{ "mt1-10..k3",		0x2000, 0xd958def5, 4 | BRF_GRA },           // 10

	{ "mmi6349.k2",		0x0200, 0xc9724350, 5 | BRF_GRA },           // 11 Color data
	{ "prom1.f1",		0x0020, 0xa1130007, 5 | BRF_GRA },           // 12
	{ "prom2.h2",		0x0100, 0x76062638, 5 | BRF_GRA },           // 13
};

STD_ROM_PICK(mototour)
STD_ROM_FN(mototour)

struct BurnDriver BurnDrvMototour = {
	"mototour", "travrusa", NULL, NULL, "1983",
	"MotoTour / Zippy Race (Tecfri license)\0", NULL, "Irem (Tecfri license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, mototourRomInfo, mototourRomName, NULL, NULL, TravrusaInputInfo, TravrusaDIPInfo,
	travrusaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 240, 3, 4
};


// Shot Rider

static struct BurnRomInfo shtriderRomDesc[] = {
	{ "sr01a.bin",		0x2000, 0xde76d537, 1 | BRF_PRG | BRF_ESS }, //  0  Z80 Code
	{ "sr02a.bin",		0x2000, 0xcd1e1bfc, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sr03a.bin",		0x2000, 0x3ade11b9, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sr04a.bin",		0x2000, 0x02b96eaa, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "sr11a.bin",		0x2000, 0xa8396b76, 2 | BRF_GRA },           //  4 M6803 Code

	{ "sr05a.bin",		0x2000, 0x34449f79, 3 | BRF_GRA },           //  5 Background tiles
	{ "sr06a.bin",		0x2000, 0xde43653d, 3 | BRF_GRA },           //  6
	{ "sr07a.bin",		0x2000, 0x3445b81c, 3 | BRF_GRA },           //  7

	{ "sr08a.bin",		0x2000, 0x4072b096, 4 | BRF_GRA },           //  8 Sprite tiles
	{ "sr09a.bin",		0x2000, 0xfd4cc7e6, 4 | BRF_GRA },           //  9
	{ "sr10b.bin",		0x2000, 0x0a117925, 4 | BRF_GRA },           // 10

	{ "1.bpr",		0x0100, 0xe9e258e5, 5 | BRF_GRA },           // 11 Color data
	{ "2.bpr",		0x0100, 0x6cf4591c, 5 | BRF_GRA },           // 12
	{ "4.bpr",		0x0020, 0xee97c581, 5 | BRF_GRA },           // 13
	{ "3.bpr",		0x0100, 0x5db47092, 5 | BRF_GRA },           // 14
};

STD_ROM_PICK(shtrider)
STD_ROM_FN(shtrider)

struct BurnDriver BurnDrvShtrider = {
	"shtrider", NULL, NULL, NULL, "1985",
	"Shot Rider\0", NULL, "Seibu Kaihatsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, shtriderRomInfo, shtriderRomName, NULL, NULL, ShtriderInputInfo, ShtriderDIPInfo,
	shtriderInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 240, 3, 4
};


// Shot Rider (Sigma license)

static struct BurnRomInfo shtrideraRomDesc[] = {
	{ "1.bin",		0x2000, 0xeb51315c, 1 | BRF_PRG | BRF_ESS }, //  0  Z80 Code
	{ "2.bin",		0x2000, 0x97675d19, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.bin",		0x2000, 0x78d051cd, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.bin",		0x2000, 0x02b96eaa, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "11.bin",		0x2000, 0xa8396b76, 2 | BRF_GRA },           //  4 M6803 Code

	{ "5.bin",		0x2000, 0x34449f79, 3 | BRF_GRA },           //  5 Background tiles
	{ "6.bin",		0x2000, 0xde43653d, 3 | BRF_GRA },           //  6
	{ "7.bin",		0x2000, 0x3445b81c, 3 | BRF_GRA },           //  7

	{ "8.bin",		0x2000, 0x4072b096, 4 | BRF_GRA },           //  8 Sprite tiles
	{ "9.bin",		0x2000, 0xfd4cc7e6, 4 | BRF_GRA },           //  9
	{ "10.bin",		0x2000, 0x0a117925, 4 | BRF_GRA },           // 10

	{ "1.bpr",		0x0100, 0xe9e258e5, 5 | BRF_GRA },           // 11 Color data
	{ "2.bpr",		0x0100, 0x6cf4591c, 5 | BRF_GRA },           // 12
	{ "4.bpr",		0x0020, 0xee97c581, 5 | BRF_GRA },           // 13
	{ "3.bpr",		0x0100, 0x5db47092, 5 | BRF_GRA },           // 14
};

STD_ROM_PICK(shtridera)
STD_ROM_FN(shtridera)

struct BurnDriver BurnDrvShtridera = {
	"shtridera", "shtrider", NULL, NULL, "1984",
	"Shot Rider (Sigma license)\0", NULL, "Seibu Kaihatsu (Sigma license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, shtrideraRomInfo, shtrideraRomName, NULL, NULL, ShtriderInputInfo, ShtriderDIPInfo,
	shtrideraInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 240, 3, 4
};


// Shot Rider (bootleg)

static struct BurnRomInfo shtriderbRomDesc[] = {
	{ "sr1.20.m3",		0x2000, 0x8bca38d7, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "sr2.21.l3",		0x2000, 0x56d4a66a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sr3.22.k3",		0x2000, 0x44cab4cc, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sr4.23.h3",		0x2000, 0x02b96eaa, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "sr11.7.a1",		0x2000, 0xa8396b76, 2 | BRF_GRA },           //  4 M6803 Code

	{ "sr5.f3",		0x2000, 0x34449f79, 3 | BRF_GRA },           //  5 Background tiles
	{ "sr6.c3",		0x2000, 0xde43653d, 3 | BRF_GRA },           //  6
	{ "sr7.a3",		0x2000, 0x3445b81c, 3 | BRF_GRA },           //  7

	{ "sr8.17.n3",		0x2000, 0x4072b096, 4 | BRF_GRA },           //  8 Sprite tiles
	{ "sr9.18.m3",		0x2000, 0xfd4cc7e6, 4 | BRF_GRA },           //  9
	{ "sr10.19.k3",		0x2000, 0x0a117925, 4 | BRF_GRA },           // 10

	{ "6349-2.k2",		0x0200, 0x854487a7, 5 | BRF_GRA },           // 11 Color data
	{ "prom1.6.f1",		0x0020, 0xee97c581, 5 | BRF_GRA },           // 12
	{ "prom2.12.h2",	0x0100, 0x5db47092, 5 | BRF_GRA },           // 13
};

STD_ROM_PICK(shtriderb)
STD_ROM_FN(shtriderb)

struct BurnDriver BurnDrvShtriderb = {
	"shtriderb", "shtrider", NULL, NULL, "1985",
	"Shot Rider (bootleg)\0", "Graphics issues", "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, shtriderbRomInfo, shtriderbRomName, NULL, NULL, ShtriderInputInfo, ShtriderDIPInfo,
	shtriderbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 240, 3, 4
};

