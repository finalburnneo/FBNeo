// Popeye & SkySkipper emu-layer for FB Alpha by dink, based on Marc Lafontaine's MAME driver.
// Note:
//   sprite clipping in popeye on the thru-way is normal (see pcb vid)

#include "tiles_generic.h"
#include "driver.h"
#include "z80_intf.h"
#include "bitswap.h"

extern "C" {
	#include "ay8910.h"
}
static INT16 *pAY8910Buffer[3];

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvZ80RAM2;
static UINT8 *DrvVidRAM;
static UINT8 *DrvColorRAM;
static UINT8 *DrvSpriteRAM;
static UINT8 *DrvBGRAM;
static UINT8 *DrvColorPROM;
static UINT8 *DrvCharGFX;
static UINT8 *DrvSpriteGFX;

static UINT8 *background_pos;
static UINT8 *palette_bank;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT16 *bgbitmap;

static UINT8 m_dswbit;
static UINT16 m_field;
static UINT8 m_prot0;
static UINT8 m_prot1;
static UINT8 m_prot_shift;
static UINT8 m_invertmask = 0xff;
static INT32 bgbitmapwh; // 512 for popeye, 1024 for skyskipr

static UINT8 skyskiprmode = 0;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDip[2] = {0, 0};
static UINT8 DrvInput[5];
static UINT8 DrvReset;

static struct BurnInputInfo SkyskiprInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 7,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 2"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 2"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 6,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDip + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDip + 1,	"dip"},
};

STDINPUTINFO(Skyskipr)


static struct BurnDIPInfo SkyskiprDIPList[]=
{
	{0x12, 0xff, 0xff, 0x7f, NULL		},
	{0x13, 0xff, 0xff, 0x7d, NULL		},

	{0   , 0xfe, 0   ,    16, "Coinage"		},
	{0x12, 0x01, 0x0f, 0x03, "A 3/1 B 1/2"		},
	{0x12, 0x01, 0x0f, 0x0e, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0x0f, 0x01, "A 2/1 B 2/5"		},
	{0x12, 0x01, 0x0f, 0x04, "A 2/1 B 1/3"		},
	{0x12, 0x01, 0x0f, 0x07, "A 1/1 B 2/1"		},
	{0x12, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0x0f, 0x0c, "A 1/1 B 1/2"		},
	{0x12, 0x01, 0x0f, 0x0d, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0x0f, 0x06, "A 1/2 B 1/4"		},
	{0x12, 0x01, 0x0f, 0x0b, "A 1/2 B 1/5"		},
	{0x12, 0x01, 0x0f, 0x02, "A 2/5 B 1/1"		},
	{0x12, 0x01, 0x0f, 0x0a, "A 1/3 B 1/1"		},
	{0x12, 0x01, 0x0f, 0x09, "A 1/4 B 1/1"		},
	{0x12, 0x01, 0x0f, 0x05, "A 1/5 B 1/1"		},
	{0x12, 0x01, 0x0f, 0x08, "A 1/6 B 1/1"		},
	{0x12, 0x01, 0x0f, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x12, 0x01, 0x10, 0x10, "Off"		},
	{0x12, 0x01, 0x10, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x12, 0x01, 0x20, 0x20, "Off"		},
	{0x12, 0x01, 0x20, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x12, 0x01, 0x40, 0x40, "Off"		},
	{0x12, 0x01, 0x40, 0x00, "On"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0x03, 0x03, "1"		},
	{0x13, 0x01, 0x03, 0x02, "2"		},
	{0x13, 0x01, 0x03, 0x01, "3"		},
	{0x13, 0x01, 0x03, 0x00, "4"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x13, 0x01, 0x04, 0x04, "Off"		},
	{0x13, 0x01, 0x04, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x13, 0x01, 0x08, 0x08, "Off"		},
	{0x13, 0x01, 0x08, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x13, 0x01, 0x10, 0x10, "Off"		},
	{0x13, 0x01, 0x10, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x13, 0x01, 0x20, 0x20, "15000"		},
	{0x13, 0x01, 0x20, 0x00, "30000"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x13, 0x01, 0x40, 0x40, "Off"		},
	{0x13, 0x01, 0x40, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x13, 0x01, 0x80, 0x00, "Upright"		},
	{0x13, 0x01, 0x80, 0x80, "Cocktail"		},
};

STDDIPINFO(Skyskipr)

static struct BurnInputInfo PopeyeInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 7,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 6,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDip + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDip + 1,	"dip"},
};

STDINPUTINFO(Popeye)


static struct BurnDIPInfo PopeyeDIPList[]=
{
	{0x10, 0xff, 0xff, 0x5f, NULL		},
	{0x11, 0xff, 0xff, 0x3d, NULL		},

	{0   , 0xfe, 0   ,    9, "Coinage"		},
	{0x10, 0x01, 0x0f, 0x08, "6 Coins 1 Credits"		},
	{0x10, 0x01, 0x0f, 0x05, "5 Coins 1 Credits"		},
	{0x10, 0x01, 0x0f, 0x09, "4 Coins 1 Credits"		},
	{0x10, 0x01, 0x0f, 0x0a, "3 Coins 1 Credits"		},
	{0x10, 0x01, 0x0f, 0x0d, "2 Coins 1 Credits"		},
	{0x10, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"		},
	{0x10, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"		},
	{0x10, 0x01, 0x0f, 0x03, "1 Coin  3 Credits"		},
	{0x10, 0x01, 0x0f, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x10, 0x01, 0x10, 0x10, "Off"		},
	{0x10, 0x01, 0x10, 0x00, "On"		},

	{0   , 0xfe, 0   ,    3, "Copyright"		},
	{0x10, 0x01, 0x60, 0x40, "Nintendo"		},
	{0x10, 0x01, 0x60, 0x20, "Nintendo Co.,Ltd"		},
	{0x10, 0x01, 0x60, 0x60, "Nintendo of America"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x11, 0x01, 0x03, 0x03, "1"		},
	{0x11, 0x01, 0x03, 0x02, "2"		},
	{0x11, 0x01, 0x03, 0x01, "3"		},
	{0x11, 0x01, 0x03, 0x00, "4"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x11, 0x01, 0x0c, 0x0c, "Easy"		},
	{0x11, 0x01, 0x0c, 0x08, "Medium"		},
	{0x11, 0x01, 0x0c, 0x04, "Hard"		},
	{0x11, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x11, 0x01, 0x30, 0x30, "40000"		},
	{0x11, 0x01, 0x30, 0x20, "60000"		},
	{0x11, 0x01, 0x30, 0x10, "80000"		},
	{0x11, 0x01, 0x30, 0x00, "None"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x11, 0x01, 0x40, 0x40, "Off"		},
	{0x11, 0x01, 0x40, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x11, 0x01, 0x80, 0x00, "Upright"		},
	{0x11, 0x01, 0x80, 0x80, "Cocktail"		},
};

STDDIPINFO(Popeye)

static void popeye_do_palette()
{
	UINT8 *color_prom = DrvColorPROM + 32;

	for (INT32 i = 0; i < 16;i++)
	{
		int prom_offs = i | ((i & 8) << 1); /* address bits 3 and 4 are tied together */
		int bit0,bit1,bit2,r,g,b;

		/* red component */
		bit0 = ((color_prom[prom_offs] ^ m_invertmask) >> 0) & 0x01;
		bit1 = ((color_prom[prom_offs] ^ m_invertmask) >> 1) & 0x01;
		bit2 = ((color_prom[prom_offs] ^ m_invertmask) >> 2) & 0x01;
		r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		/* green component */
		bit0 = ((color_prom[prom_offs] ^ m_invertmask) >> 3) & 0x01;
		bit1 = ((color_prom[prom_offs] ^ m_invertmask) >> 4) & 0x01;
		bit2 = ((color_prom[prom_offs] ^ m_invertmask) >> 5) & 0x01;
		g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		/* blue component */
		bit0 = 0;
		bit1 = ((color_prom[prom_offs] ^ m_invertmask) >> 6) & 0x01;
		bit2 = ((color_prom[prom_offs] ^ m_invertmask) >> 7) & 0x01;
		b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		DrvPalette[0x100 + (2 * i) + 1] = BurnHighCol(r, g, b, 0);
	}

	color_prom += 32;

	for (INT32 i = 0; i < 256;i++)
	{
		int bit0,bit1,bit2,r,g,b;


		/* red component */
		bit0 = ((color_prom[0] ^ m_invertmask) >> 0) & 0x01;
		bit1 = ((color_prom[0] ^ m_invertmask) >> 1) & 0x01;
		bit2 = ((color_prom[0] ^ m_invertmask) >> 2) & 0x01;
		r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		/* green component */
		bit0 = ((color_prom[0] ^ m_invertmask) >> 3) & 0x01;
		bit1 = ((color_prom[256] ^ m_invertmask) >> 0) & 0x01;
		bit2 = ((color_prom[256] ^ m_invertmask) >> 1) & 0x01;
		g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		/* blue component */
		bit0 = 0;
		bit1 = ((color_prom[256] ^ m_invertmask) >> 2) & 0x01;
		bit2 = ((color_prom[256] ^ m_invertmask) >> 3) & 0x01;
		b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		DrvPalette[0x200 + i] = BurnHighCol(r, g, b, 0);

		color_prom++;
	}
}

static void popeye_do_background_palette(UINT32 bank)
{
	UINT8 *color_prom = DrvColorPROM + 16 * bank;

	for (INT32 i = 0; i < 16;i++)
	{
		int bit0,bit1,bit2;
		int r,g,b;

		/* red component */
		bit0 = ((color_prom[0] ^ m_invertmask) >> 0) & 0x01;
		bit1 = ((color_prom[0] ^ m_invertmask) >> 1) & 0x01;
		bit2 = ((color_prom[0] ^ m_invertmask) >> 2) & 0x01;
		r = 0x1c * bit0 + 0x31 * bit1 + 0x47 * bit2;
		/* green component */
		bit0 = ((color_prom[0] ^ m_invertmask) >> 3) & 0x01;
		bit1 = ((color_prom[0] ^ m_invertmask) >> 4) & 0x01;
		bit2 = ((color_prom[0] ^ m_invertmask) >> 5) & 0x01;
		g = 0x1c * bit0 + 0x31 * bit1 + 0x47 * bit2;
		/* blue component */
		bit0 = 0;
		bit1 = ((color_prom[0] ^ m_invertmask) >> 6) & 0x01;
		bit2 = ((color_prom[0] ^ m_invertmask) >> 7) & 0x01;
		if (skyskiprmode)
		{
			bit0 = bit1;
			bit1 = 0;
		}
		b = 0x1c * bit0 + 0x31 * bit1 + 0x47 * bit2;

		DrvPalette[i] = BurnHighCol(r, g, b, 0);

		color_prom++;
	}
}

static void popeye_recalcpalette()
{
	popeye_do_palette();
	popeye_do_background_palette((*palette_bank & 0x08) >> 3);
}

static void skyskipr_bitmap_w(UINT16 offset, UINT8 data)
{
	INT32 sx,sy,x,y,colour;

	if (skyskiprmode)
	{
		offset = ((offset & 0xfc0) << 1) | (offset & 0x03f);
		if (data & 0x80)
			offset |= 0x40;

		DrvBGRAM[offset] = data;

		sx = 8 * (offset % 128);
		sy = 8 * (offset / 128);

		//if (flip_screen())
		//	sy = 512-8 - sy;

		colour = data & 0x0f;
		for (y = 0; y < 8; y++)	{
			for (x = 0; x < 8; x++)	{
				bgbitmap[((sy+y) * 1024) + sx+x] = colour;
			}
		}
	}
	else
	{
		DrvBGRAM[offset] = data;

		sx = 8 * (offset % 64);
		sy = 4 * (offset / 64);

		//if (flip_screen())
		//	sy = 512-4 - sy;

		colour = data & 0x0f;
		for (y = 0; y < 4; y++) {
			for (x = 0; x < 8; x++)	{
				bgbitmap[((sy+y) * 512) + sx+x] = colour;
			}
		}
	}

}

static void __fastcall main_write(UINT16 address, UINT8 data)
{
	if (address >= 0x8c04 && address <= 0x8e7f) {
		DrvSpriteRAM[address - 0x8c04] = data;
		return;
	}

	if (address >= 0x8e80 && address <= 0x8fff) {
		DrvZ80RAM2[address - 0x8e80] = data;
		return;
	}

	if (address >= 0xc000 && address <= 0xdfff) {
		skyskipr_bitmap_w(address - 0xc000, data);
		return;
	}

	switch (address)
	{
		case 0xE000: m_prot_shift = data & 0x07; return;
		case 0xE001: m_prot0 = m_prot1; m_prot1 = data; return;
		case 0x8c00:
		case 0x8c01:
		case 0x8c02: background_pos[address & 3] = data; return;
		case 0x8c03: *palette_bank = data; return;
	}

	//bprintf(0, _T("mw %X,"), address);
}


static UINT8 __fastcall main_read(UINT16 address)
{
	if (address >= 0x8c04 && address <= 0x8e7f)
		return DrvSpriteRAM[address - 0x8c04];

	if (address >= 0x8e80 && address <= 0x8fff)
		return DrvZ80RAM2[address - 0x8e80];

	if (address >= 0xc000 && address <= 0xdfff) {
		return DrvBGRAM[address - 0xc000];
	}

	switch (address)
	{
		case 0xE000: return ((m_prot1 << m_prot_shift) | (m_prot0 >> (8-m_prot_shift))) & 0xff;
		case 0xE001: return 0;
		case 0x8c00:
		case 0x8c01:
		case 0x8c02: return background_pos[address & 3];
		case 0x8c03: return *palette_bank;
	}

	//bprintf(0, _T("mr %X,"), address);

	return 0;
}


static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	AY8910Reset(0);

	HiscoreReset();

	m_field = 0;
	m_dswbit = 0;
	m_field = 0;
	m_prot0 = 0;
	m_prot1 = 0;
	m_prot_shift = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x08000;

	DrvPalette		= (UINT32*)Next; Next += 0x0300 * sizeof(UINT32);
	DrvCharGFX      = Next; Next += 0x20000;
	DrvSpriteGFX    = Next; Next += 0x20000;
	DrvColorPROM    = Next; Next += 0x00400;

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x00c00;
	DrvZ80RAM2		= Next; Next += 0x00200;
	DrvVidRAM		= Next; Next += 0x00400;
	DrvColorRAM		= Next; Next += 0x00400;
	DrvSpriteRAM	= Next; Next += 0x00300;
	DrvBGRAM		= Next; Next += 0x02000;
	background_pos  = Next; Next += 0x00003;
	palette_bank    = Next; Next += 0x00002;
	bgbitmap        = (UINT16*)Next; Next += 1024 * 1024 * sizeof(UINT16); // background bitmap (RAM section)

	RamEnd			= Next;

	pAY8910Buffer[0]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[1]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[2]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);

	MemEnd			= Next;

	return 0;
}

static UINT8 __fastcall port_read(UINT16 port)
{
	switch (port & 0xff) {
		case 0x00: return DrvInput[0];
		case 0x01: return DrvInput[1];
		case 0x02: return (skyskiprmode) ? DrvInput[2] : DrvInput[2] | (m_field ^ 1) << 4;
		case 0x03: return AY8910Read(0);
	}
	return 0xff;
}

static void __fastcall port_write(UINT16 port, UINT8 data)
{
	switch (port & 0xff) {
		case 0x00:
			AY8910Write(0, 0, data);
		break;

		case 0x01:
			AY8910Write(0, 1, data);
		break;
	}
}

static void popeye_ayportB_write(UINT32 /*addr*/, UINT32 data)
{
	/* bit 0 flips screen */
	//flip_screen_set(data & 1);

	/* bits 1-3 select DSW1 bit to read */
	m_dswbit = (data & 0x0e) >> 1;
}

static UINT8 popeye_ayportA_read(UINT32 /*addr*/)
{
	return (DrvDip[0] & 0x7f) | ((DrvDip[1] << (7-m_dswbit)) & 0x80);
}

static INT32 CharPlaneOffsets[2] = { 0, 0 };
static INT32 CharXOffsets[16] = { 7,7, 6,6, 5,5, 4,4, 3,3, 2,2, 1,1, 0,0 };
static INT32 CharYOffsets[16] = { 0*8,0*8, 1*8,1*8, 2*8,2*8, 3*8,3*8, 4*8,4*8, 5*8,5*8, 6*8,6*8, 7*8,7*8 };

static INT32 DrvInit()
{
	INT32 SpritePlaneOffsets[2] = { 0, RGN_FRAC(((skyskiprmode) ? 0x4000 : 0x8000), 1,2) };
	INT32 SpriteXOffsets[16] = { RGN_FRAC(((skyskiprmode) ? 0x4000 : 0x8000), 1,4)+7,RGN_FRAC(((skyskiprmode) ? 0x4000 : 0x8000), 1,4)+6,RGN_FRAC(((skyskiprmode) ? 0x4000 : 0x8000), 1,4)+5,RGN_FRAC(((skyskiprmode) ? 0x4000 : 0x8000), 1,4)+4,RGN_FRAC(((skyskiprmode) ? 0x4000 : 0x8000), 1,4)+3,RGN_FRAC(((skyskiprmode) ? 0x4000 : 0x8000), 1,4)+2,RGN_FRAC(((skyskiprmode) ? 0x4000 : 0x8000), 1,4)+1,RGN_FRAC(((skyskiprmode) ? 0x4000 : 0x8000), 1,4)+0,7,6,5,4,3,2,1,0};
	INT32 SpriteYOffsets[16] = { 15*8, 14*8, 13*8, 12*8, 11*8, 10*8, 9*8, 8*8, 7*8, 6*8, 5*8, 4*8, 3*8, 2*8, 1*8, 0*8 };

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{   // Load ROMS parse GFX
		UINT8 *DrvTempRom = (UINT8 *)BurnMalloc(0x40000);
		memset(DrvTempRom, 0, 0x40000);

		if (skyskiprmode) { // SkySkipper
			bgbitmapwh = 1024;

			if (BurnLoadRom(DrvTempRom + 0x0000, 0, 1)) return 1;
			if (BurnLoadRom(DrvTempRom + 0x1000, 1, 1)) return 1;
			if (BurnLoadRom(DrvTempRom + 0x2000, 2, 1)) return 1;
			if (BurnLoadRom(DrvTempRom + 0x3000, 3, 1)) return 1;
			if (BurnLoadRom(DrvTempRom + 0x4000, 4, 1)) return 1;
			if (BurnLoadRom(DrvTempRom + 0x5000, 5, 1)) return 1;
			if (BurnLoadRom(DrvTempRom + 0x6000, 6, 1)) return 1;

			/* decrypt the program ROMs */
			for (INT32 i = 0; i < 0x8000; i++)
				DrvZ80ROM[i] = BITSWAP08(DrvTempRom[BITSWAP16(i,15,14,13,12,11,10,8,7,0,1,2,4,5,9,3,6) ^ 0xfc],3,4,2,5,1,6,0,7);

			memset(DrvTempRom, 0, 0x40000);
			if (BurnLoadRom(DrvTempRom         , 7, 1)) return 1;
			GfxDecode(0x100, 1, 16, 16, CharPlaneOffsets, CharXOffsets, CharYOffsets, 0x40, DrvTempRom, DrvCharGFX);

			memset(DrvTempRom, 0, 0x40000);
			if (BurnLoadRom(DrvTempRom + 0x0000, 8, 1)) return 1;
			if (BurnLoadRom(DrvTempRom + 0x1000, 9, 1)) return 1;
			if (BurnLoadRom(DrvTempRom + 0x2000,10, 1)) return 1;
			if (BurnLoadRom(DrvTempRom + 0x3000,11, 1)) return 1;
			GfxDecode(0x100 /*((0x4000*8)/2)/(16*16))*/, 2, 16, 16, SpritePlaneOffsets, SpriteXOffsets, SpriteYOffsets, 0x80, DrvTempRom, DrvSpriteGFX);

			if (BurnLoadRom(DrvColorPROM + 0x0000, 12, 1)) return 1;
			if (BurnLoadRom(DrvColorPROM + 0x0020, 13, 1)) return 1;
			if (BurnLoadRom(DrvColorPROM + 0x0040, 14, 1)) return 1;
			if (BurnLoadRom(DrvColorPROM + 0x0140, 15, 1)) return 1;
			if (BurnLoadRom(DrvColorPROM + 0x0240, 16, 1)) return 1;

		} else { // Popeye
			bgbitmapwh = 512;

			if (BurnLoadRom(DrvTempRom + 0x0000, 0, 1)) return 1;
			if (BurnLoadRom(DrvTempRom + 0x2000, 1, 1)) return 1;
			if (BurnLoadRom(DrvTempRom + 0x4000, 2, 1)) return 1;
			if (BurnLoadRom(DrvTempRom + 0x6000, 3, 1)) return 1;

			/* decrypt the program ROMs */
			for (INT32 i = 0; i < 0x8000; i++)
				DrvZ80ROM[i] = BITSWAP08(DrvTempRom[BITSWAP16(i,15,14,13,12,11,10,8,7,6,3,9,5,4,2,1,0) ^ 0x3f],3,4,2,5,1,6,0,7);

			memset(DrvTempRom, 0, 0x40000);
			if (BurnLoadRom(DrvTempRom         , 4, 1)) return 1;
			GfxDecode(0x100, 1, 16, 16, CharPlaneOffsets, CharXOffsets, CharYOffsets, 0x40, DrvTempRom+0x800, DrvCharGFX);

			memset(DrvTempRom, 0, 0x40000);
			if (BurnLoadRom(DrvTempRom + 0x0000, 5, 1)) return 1;
			if (BurnLoadRom(DrvTempRom + 0x2000, 6, 1)) return 1;
			if (BurnLoadRom(DrvTempRom + 0x4000, 7, 1)) return 1;
			if (BurnLoadRom(DrvTempRom + 0x6000, 8, 1)) return 1;
			GfxDecode(0x200, 2, 16, 16, SpritePlaneOffsets, SpriteXOffsets, SpriteYOffsets, 0x80, DrvTempRom, DrvSpriteGFX);

			if (BurnLoadRom(DrvColorPROM + 0x0000,  9, 1)) return 1;
			if (BurnLoadRom(DrvColorPROM + 0x0020, 10, 1)) return 1;
			if (BurnLoadRom(DrvColorPROM + 0x0040, 11, 1)) return 1;
			if (BurnLoadRom(DrvColorPROM + 0x0140, 12, 1)) return 1;
			if (BurnLoadRom(DrvColorPROM + 0x0240, 13, 1)) return 1;
		}
		BurnFree(DrvTempRom);
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,		0x8000, 0x8bff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,		0xa000, 0xa3ff, MAP_RAM);
	ZetMapMemory(DrvColorRAM,	0xa400, 0xa7ff, MAP_RAM);
	ZetSetWriteHandler(main_write);
	ZetSetReadHandler(main_read);
	ZetSetInHandler(port_read);
	ZetSetOutHandler(port_write);
	ZetClose();

	AY8910Init(0, 2000000, nBurnSoundRate, popeye_ayportA_read, NULL, NULL, popeye_ayportB_write);
	AY8910SetAllRoutes(0, 0.30, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}


static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	AY8910Exit(0);

	BurnFree(AllMem);

	skyskiprmode = 0;

	return 0;
}


extern int counter;

static void draw_chars()
{
	for (INT32 offs = 0; offs <= 0x400; offs++)
	{
		int sx = offs % 32;
		int sy = offs / 32;

		int code = DrvVidRAM[offs];
		int color = DrvColorRAM[offs] & 0x0f;

		sx = 16 * sx;
		sy = (16 * sy) - 32;

		if (sx >= nScreenWidth || sy >= nScreenHeight || sx < 0 || sy < 0) continue;

 		Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 1, 0, 0x100, DrvCharGFX);
	}
}

static void draw_background()
{
	if (background_pos[1] == 0) {
		return; // background disabled
	}

	popeye_do_background_palette((*palette_bank & 0x08) >> 3);

	INT32 scrollx = 200 - background_pos[0] - 256*(background_pos[2]&1); /* ??? */
	INT32 scrolly = 2 * (256 - background_pos[1]);

	if (skyskiprmode)
		scrollx = 2 * scrollx - 512;

	// copy matching background section to pTransDraw
	INT32 destx = 1-scrollx;
	INT32 desty = 0-scrolly + 32;
	INT32 destendx = destx + nScreenWidth/* - 1*/;
	INT32 destendy = desty + nScreenHeight - 1;

	for (INT32 cury = desty; cury < destendy; cury++)
		for (INT32 curx = destx; curx < destendx; curx++) {
			UINT16 *pPixel = pTransDraw + ((cury-desty) * nScreenWidth) + (curx-destx);
			if (cury-desty < 0 || curx-destx < 0 || cury-desty >= nScreenHeight || curx-destx >= nScreenWidth || cury >= bgbitmapwh /*|| curx >= bgbitmapwh this is solved by modulus below!*/ || cury < 0 || curx < 0) continue;

			*pPixel = bgbitmap[(cury*bgbitmapwh) + (curx%bgbitmapwh)];
		}

}


static void draw_sprites()
{
	UINT8 *spriteram = DrvSpriteRAM;
	INT32 sprmask = ((skyskiprmode) ? 0xff : 0x1ff);

	for (INT32 offs = 0; offs < 0x27b; offs += 4)
	{
		INT32 code,color,flipx,flipy,sx,sy;

		color = (spriteram[offs + 3] & 0x07);
		if (color == 0) continue;
		if (!spriteram[offs]) continue;

		code = (spriteram[offs + 2] & 0x7f)
			+ ((spriteram[offs + 3] & 0x10) << 3)
			+ ((spriteram[offs + 3] & 0x04) << 6);

		color += (*palette_bank & 0x07) << 3;
		if (skyskiprmode) {
			color = (color & 0x0f) | ((color & 0x08) << 1);
		}

		flipx = spriteram[offs + 2] & 0x80;
		flipy = spriteram[offs + 3] & 0x08;

		sx = 2*(spriteram[offs])-8;
		sy = (2*(256-spriteram[offs + 1])) - 32;

		if (sx <= -6) sx += 512;

		code = (code ^ 0x1ff) & sprmask;

		if (flipy) { // we need a macro for this -dink
			if (flipx) {
				Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0x200, DrvSpriteGFX);
				Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx-512, sy, color, 2, 0, 0x200, DrvSpriteGFX);
			} else {
				Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0x200, DrvSpriteGFX);
				Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx-512, sy, color, 2, 0, 0x200, DrvSpriteGFX);
			}
		} else {
			if (flipx) {
				Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0x200, DrvSpriteGFX);
				Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx-512, sy, color, 2, 0, 0x200, DrvSpriteGFX);
			} else {
				Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0x200, DrvSpriteGFX);
				Render16x16Tile_Mask_Clip(pTransDraw, code, sx-512, sy, color, 2, 0, 0x200, DrvSpriteGFX);
			}
		}
	}
}


static INT32 DrvDraw()
{
	if (DrvRecalc) {
		popeye_recalcpalette();
		DrvRecalc = 0;
	}

	BurnTransferClear();

	draw_background();
	draw_sprites();
	draw_chars();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void DrvMakeInputs()
{
	UINT8 *DrvJoy[3] = { DrvJoy1, DrvJoy2, DrvJoy3 };
	UINT32 DrvJoyInit[3] = { 0x00, 0x00, 0x00 };

	CompileInput(DrvJoy, (void*)DrvInput, 3, 8, DrvJoyInit);

	if (!skyskiprmode) {
		// Convert to 4-way for Popeye
		ProcessJoystick(&DrvInput[0], 0, 3,2,1,0, INPUT_4WAY);
		ProcessJoystick(&DrvInput[1], 1, 3,2,1,0, INPUT_4WAY);
	}
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	DrvMakeInputs();

	INT32 nInterleave = 256;
	INT32 nCyclesTotal = 4000000 / 60;

	for (INT32 i = 0; i < nInterleave; i++) {
		ZetOpen(0);
		ZetRun(nCyclesTotal / nInterleave);

		if (i == nInterleave - 1 && (ZetI(-1) & 1))
			ZetNmi();

		ZetClose();
	}

	if (pBurnSoundOut) {
		AY8910Render(&pAY8910Buffer[0], pBurnSoundOut, nBurnSoundLen, 0);
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
		*pnMin = 0x029735;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);

		AY8910Scan(nAction, pnMin);

		SCAN_VAR(m_dswbit);
		SCAN_VAR(m_field);
		SCAN_VAR(m_prot0);
		SCAN_VAR(m_prot1);
		SCAN_VAR(m_prot_shift);
		SCAN_VAR(m_invertmask);
	}

	return 0;
}


// Sky Skipper

static struct BurnRomInfo skyskiprRomDesc[] = {
	{ "tnx1-c.2a",	0x1000, 0xbdc7f218, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "tnx1-c.2b",	0x1000, 0xcbe601a8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tnx1-c.2c",	0x1000, 0x5ca79abf, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "tnx1-c.2d",	0x1000, 0x6b7a7071, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "tnx1-c.2e",	0x1000, 0x6b0c0525, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "tnx1-c.2f",	0x1000, 0xd1712424, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "tnx1-c.2g",	0x1000, 0x8b33c4cf, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "tnx1-v.3h",	0x0800, 0xecb6a046, 2 | BRF_GRA }, //  7 gfx1

	{ "tnx1-t.1e",	0x1000, 0x01c1120e, 3 | BRF_GRA }, //  8 gfx2
	{ "tnx1-t.2e",	0x1000, 0x70292a71, 3 | BRF_GRA }, //  9
	{ "tnx1-t.3e",	0x1000, 0x92b6a0e8, 3 | BRF_GRA }, // 10
	{ "tnx1-t.5e",	0x1000, 0xcc5f0ac3, 3 | BRF_GRA }, // 11

	{ "tnx1-t.4a",	0x0020, 0x98846924, 4 | BRF_GRA }, // 12 proms
	{ "tnx1-t.1a",	0x0020, 0xc2bca435, 4 | BRF_GRA }, // 13
	{ "tnx1-t.3a",	0x0100, 0x8abf9de4, 4 | BRF_GRA }, // 14
	{ "tnx1-t.2a",	0x0100, 0xaa7ff322, 4 | BRF_GRA }, // 15
	{ "tnx1-t.3j",	0x0100, 0x1c5c8dea, 4 | BRF_GRA }, // 16
};

STD_ROM_PICK(skyskipr)
STD_ROM_FN(skyskipr)

static INT32 DrvInitskyskipr()
{
	skyskiprmode = 1;

	return DrvInit();
}

struct BurnDriver BurnDrvSkyskipr = {
	"skyskipr", NULL, NULL, NULL, "1981",
	"Sky Skipper\0", NULL, "Nintendo", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, skyskiprRomInfo, skyskiprRomName, NULL, NULL, SkyskiprInputInfo, SkyskiprDIPInfo,
	DrvInitskyskipr, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	512, 448, 4, 3
};

// Popeye (revision D)

static struct BurnRomInfo popeyeRomDesc[] = {
	{ "tpp2-c.7a",	0x2000, 0x9af7c821, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "tpp2-c.7b",	0x2000, 0xc3704958, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tpp2-c.7c",	0x2000, 0x5882ebf9, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "tpp2-c.7e",	0x2000, 0xef8649ca, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "tpp2-v.5n",	0x1000, 0xcca61ddd, 2 | BRF_GRA }, //  4 gfx1

	{ "tpp2-v.1e",	0x2000, 0x0f2cd853, 3 | BRF_GRA }, //  5 gfx2
	{ "tpp2-v.1f",	0x2000, 0x888f3474, 3 | BRF_GRA }, //  6
	{ "tpp2-v.1j",	0x2000, 0x7e864668, 3 | BRF_GRA }, //  7
	{ "tpp2-v.1k",	0x2000, 0x49e1d170, 3 | BRF_GRA }, //  8

	{ "tpp2-c.4a",	0x0020, 0x375e1602, 4 | BRF_GRA }, //  9 proms
	{ "tpp2-c.3a",	0x0020, 0xe950bea1, 4 | BRF_GRA }, // 10
	{ "tpp2-c.5b",	0x0100, 0xc5826883, 4 | BRF_GRA }, // 11
	{ "tpp2-c.5a",	0x0100, 0xc576afba, 4 | BRF_GRA }, // 12
	{ "tpp2-v.7j",	0x0100, 0xa4655e2e, 4 | BRF_GRA }, // 13
};

STD_ROM_PICK(popeye)
STD_ROM_FN(popeye)

struct BurnDriver BurnDrvPopeye = {
	"popeye", NULL, NULL, NULL, "1982",
	"Popeye (revision D)\0", NULL, "Nintendo", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, popeyeRomInfo, popeyeRomName, NULL, NULL, PopeyeInputInfo, PopeyeDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	512, 448, 4, 3
};
