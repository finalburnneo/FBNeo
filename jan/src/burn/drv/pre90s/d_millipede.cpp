// Millipede emu-layer for FB Alpha by dink, based on Ivan Mackintosh's Millipede/Centipede emulator and MAME driver.
// Todo:
//   Screen flip needs fixing (2p coctail mode) [move joystick <- -> or press OK to continue!]

#include "tiles_generic.h"
#include "m6502_intf.h"
#include "pokey.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv6502ROM;
static UINT8 *Drv6502RAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvSpriteRAM;

static UINT8 *DrvBGGFX;
static UINT8 *DrvSpriteGFX;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 m_dsw_select;
static UINT8 m_control_select;
static UINT32 m_flipscreen = 0;
static UINT32 vblank;
// transmask stuff
UINT8 m_penmask[64];
// trackball stuff
static int oldpos[4];
static UINT8 sign[4];

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvDip[5] = {0, 0, 0, 0, 0};
static UINT8 DrvInput[5];
static UINT8 DrvReset;

// hi-score stuff! (atari earom)
#define EAROM_SIZE	0x40
static UINT8 earom_offset;
static UINT8 earom_data;
static UINT8 earom[EAROM_SIZE];

static UINT32 centipedemode = 0;

static struct BurnInputInfo MillipedInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy4 + 3,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy4 + 2,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy4 + 0,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 7,	"service"},
	{"Tilt",		BIT_DIGITAL,	DrvJoy3 + 4,	"tilt"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDip + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDip + 1,	"dip"},
	{"Dip C",		BIT_DIPSWITCH,	DrvDip + 2,	"dip"},
	{"Dip D",		BIT_DIPSWITCH,	DrvDip + 3,	"dip"},
//  {"Dip E",		BIT_DIPSWITCH,	DrvDip + 4,	"dip"},
};

STDINPUTINFO(Milliped)

static struct BurnInputInfo CentipedInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy4 + 4,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy4 + 5,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy4 + 6,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy4 + 7,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 fire 1"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy4 + 0,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy4 + 3,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy4 + 2,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 1"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy2 + 7,	"service"},
	{"Tilt",		BIT_DIGITAL,	DrvJoy2 + 4,	"tilt"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDip + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDip + 1,	"dip"},
	{"Dip C",		BIT_DIPSWITCH,	DrvDip + 2,	"dip"},
};

STDINPUTINFO(Centiped)


static struct BurnDIPInfo CentipedDIPList[]=
{
	{0x11, 0xff, 0xff, 0x00, NULL		},
	{0x12, 0xff, 0xff, 0x54, NULL		},
	{0x13, 0xff, 0xff, 0x02, NULL		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x11, 0x01, 0x10, 0x00, "Upright"		},
	{0x11, 0x01, 0x10, 0x10, "Cocktail"		},

	{0   , 0xfe, 0   ,    4, "Language"		},
	{0x12, 0x01, 0x03, 0x00, "English"		},
	{0x12, 0x01, 0x03, 0x01, "German"		},
	{0x12, 0x01, 0x03, 0x02, "French"		},
	{0x12, 0x01, 0x03, 0x03, "Spanish"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x12, 0x01, 0x0c, 0x00, "2"		},
	{0x12, 0x01, 0x0c, 0x04, "3"		},
	{0x12, 0x01, 0x0c, 0x08, "4"		},
	{0x12, 0x01, 0x0c, 0x0c, "5"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x12, 0x01, 0x30, 0x00, "10000"		},
	{0x12, 0x01, 0x30, 0x10, "12000"		},
	{0x12, 0x01, 0x30, 0x20, "15000"		},
	{0x12, 0x01, 0x30, 0x30, "20000"		},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x12, 0x01, 0x40, 0x40, "Easy"		},
	{0x12, 0x01, 0x40, 0x00, "Hard"		},

	{0   , 0xfe, 0   ,    2, "Credit Minimum"		},
	{0x12, 0x01, 0x80, 0x00, "1"		},
	{0x12, 0x01, 0x80, 0x80, "2"		},

	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{0x13, 0x01, 0x03, 0x03, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x03, 0x02, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x03, 0x01, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x03, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    4, "Right Coin"		},
	{0x13, 0x01, 0x0c, 0x00, "*1"		},
	{0x13, 0x01, 0x0c, 0x04, "*4"		},
	{0x13, 0x01, 0x0c, 0x08, "*5"		},
	{0x13, 0x01, 0x0c, 0x0c, "*6"		},

	{0   , 0xfe, 0   ,    2, "Left Coin"		},
	{0x13, 0x01, 0x10, 0x00, "*1"		},
	{0x13, 0x01, 0x10, 0x10, "*2"		},

	{0   , 0xfe, 0   ,    6, "Bonus Coins"		},
	{0x13, 0x01, 0xe0, 0x00, "None"		},
	{0x13, 0x01, 0xe0, 0x20, "3 credits/2 coins"		},
	{0x13, 0x01, 0xe0, 0x40, "5 credits/4 coins"		},
	{0x13, 0x01, 0xe0, 0x60, "6 credits/4 coins"		},
	{0x13, 0x01, 0xe0, 0x80, "6 credits/5 coins"		},
	{0x13, 0x01, 0xe0, 0xa0, "4 credits/3 coins"		},
};

STDDIPINFO(Centiped)


static struct BurnDIPInfo MillipedDIPList[]=
{
	{0x11, 0xff, 0xff, 0x04, NULL		},
//	{0x12, 0xff, 0xff, 0x08, NULL		},
	{0x12, 0xff, 0xff, 0xA0, NULL		},
	{0x13, 0xff, 0xff, 0x04, NULL		},
	{0x14, 0xff, 0xff, 0x02, NULL		},

	{0   , 0xfe, 0   ,    4, "Language"		},
	{0x11, 0x01, 0x03, 0x00, "English"		},
	{0x11, 0x01, 0x03, 0x01, "German"		},
	{0x11, 0x01, 0x03, 0x02, "French"		},
	{0x11, 0x01, 0x03, 0x03, "Spanish"		},

	{0   , 0xfe, 0   ,    4, "Bonus"		},
	{0x11, 0x01, 0x0c, 0x00, "0"		},
	{0x11, 0x01, 0x0c, 0x04, "0 1x"		},
	{0x11, 0x01, 0x0c, 0x08, "0 1x 2x"		},
	{0x11, 0x01, 0x0c, 0x0c, "0 1x 2x 3x"		},

/*	{0   , 0xfe, 0   ,    2, "Credit Minimum"		},
	{0x12, 0x01, 0x04, 0x00, "1"		},
	{0x12, 0x01, 0x04, 0x04, "2"		},

	{0   , 0xfe, 0   ,    2, "Coin Counters"		},
	{0x12, 0x01, 0x08, 0x00, "1"		},
	{0x12, 0x01, 0x08, 0x08, "2"		},  */

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x12, 0x01, 0x20, 0x20, "Upright"		},
	{0x12, 0x01, 0x20, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x12, 0x01, 0x80, 0x80, "Off"		},
	{0x12, 0x01, 0x80, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Millipede Head"		},
	{0x13, 0x01, 0x01, 0x00, "Easy"		},
	{0x13, 0x01, 0x01, 0x01, "Hard"		},

	{0   , 0xfe, 0   ,    2, "Beetle"		},
	{0x13, 0x01, 0x02, 0x00, "Easy"		},
	{0x13, 0x01, 0x02, 0x02, "Hard"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0x0c, 0x00, "2"		},
	{0x13, 0x01, 0x0c, 0x04, "3"		},
	{0x13, 0x01, 0x0c, 0x08, "4"		},
	{0x13, 0x01, 0x0c, 0x0c, "5"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x13, 0x01, 0x30, 0x00, "12000"		},
	{0x13, 0x01, 0x30, 0x10, "15000"		},
	{0x13, 0x01, 0x30, 0x20, "20000"		},
	{0x13, 0x01, 0x30, 0x30, "None"		},

	{0   , 0xfe, 0   ,    2, "Spider"		},
	{0x13, 0x01, 0x40, 0x00, "Easy"		},
	{0x13, 0x01, 0x40, 0x40, "Hard"		},

	{0   , 0xfe, 0   ,    2, "Starting Score Select"		},
	{0x13, 0x01, 0x80, 0x80, "Off"		},
	{0x13, 0x01, 0x80, 0x00, "On"		},

	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{0x14, 0x01, 0x03, 0x03, "2 Coins 1 Credits"		},
	{0x14, 0x01, 0x03, 0x02, "1 Coin  1 Credits"		},
	{0x14, 0x01, 0x03, 0x01, "1 Coin  2 Credits"		},
	{0x14, 0x01, 0x03, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    4, "Right Coin"		},
	{0x14, 0x01, 0x0c, 0x00, "*1"		},
	{0x14, 0x01, 0x0c, 0x04, "*4"		},
	{0x14, 0x01, 0x0c, 0x08, "*5"		},
	{0x14, 0x01, 0x0c, 0x0c, "*6"		},

	{0   , 0xfe, 0   ,    2, "Left Coin"		},
	{0x14, 0x01, 0x10, 0x00, "*1"		},
	{0x14, 0x01, 0x10, 0x10, "*2"		},

	{0   , 0xfe, 0   ,    7, "Bonus Coins"		},
	{0x14, 0x01, 0xe0, 0x00, "None"		},
	{0x14, 0x01, 0xe0, 0x20, "3 credits/2 coins"		},
	{0x14, 0x01, 0xe0, 0x40, "5 credits/4 coins"		},
	{0x14, 0x01, 0xe0, 0x60, "6 credits/4 coins"		},
	{0x14, 0x01, 0xe0, 0x80, "6 credits/5 coins"		},
	{0x14, 0x01, 0xe0, 0xa0, "4 credits/3 coins"		},
	{0x14, 0x01, 0xe0, 0xc0, "Demo Mode"		},
};

STDDIPINFO(Milliped)

static void milliped_set_color(UINT16 offset, UINT8 data)
{
	UINT32 color;
	int bit0, bit1, bit2;
	int r, g, b;

	/* red component */
	bit0 = (~data >> 5) & 0x01;
	bit1 = (~data >> 6) & 0x01;
	bit2 = (~data >> 7) & 0x01;
	r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

	/* green component */
	bit0 = 0;
	bit1 = (~data >> 3) & 0x01;
	bit2 = (~data >> 4) & 0x01;
	g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

	/* blue component */
	bit0 = (~data >> 0) & 0x01;
	bit1 = (~data >> 1) & 0x01;
	bit2 = (~data >> 2) & 0x01;
	b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

	color = BurnHighCol(r, g, b, 0);

	/* character colors, set directly */
	if (offset < 0x10)
		DrvPalette[offset] = color;

	/* sprite colors - set all the applicable ones */
	else
	{
		int i;

		int base = offset & 0x0c;

		offset = offset & 0x03;

		for (i = (base << 6); i < (base << 6) + 0x100; i += 4)
		{
			if (offset == ((i >> 2) & 0x03))
				DrvPalette[i + 0x100 + 1] = color;

			if (offset == ((i >> 4) & 0x03))
				DrvPalette[i + 0x100 + 2] = color;

			if (offset == ((i >> 6) & 0x03))
				DrvPalette[i + 0x100 + 3] = color;
		}
	}
}

static void millipede_recalcpalette()
{
	for (INT32 i = 0;i <= 0x1f; i++) {
		milliped_set_color(i, DrvPalRAM[i]);
	}
}

static void centipede_set_color(UINT16 offset, UINT8 data)
{
	/* bit 2 of the output palette RAM is always pulled high, so we ignore */
	/* any palette changes unless the write is to a palette RAM address */
	/* that is actually used */
	if (offset & 4)
	{
		INT32 color;

		int r = 0xff * ((~data >> 0) & 1);
		int g = 0xff * ((~data >> 1) & 1);
		int b = 0xff * ((~data >> 2) & 1);

		if (~data & 0x08) /* alternate = 1 */
		{
			/* when blue component is not 0, decrease it. When blue component is 0, */
			/* decrease green component. */
			if (b) b = 0xc0;
			else if (g) g = 0xc0;
		}

		color = BurnHighCol(r, g, b, 0);

		/* character colors, set directly */
		if ((offset & 0x08) == 0)
			DrvPalette[offset & 0x03] = color;

		/* sprite colors - set all the applicable ones */
		else
		{
			int i;

			offset = offset & 0x03;

			for (i = 0; i < 0x100; i += 4)
			{
				if (offset == ((i >> 2) & 0x03))
					DrvPalette[i + 0x100 + 1] = color;

				if (offset == ((i >> 4) & 0x03))
					DrvPalette[i + 0x100 + 2] = color;

				if (offset == ((i >> 6) & 0x03))
					DrvPalette[i + 0x100 + 3] = color;
			}
		}
	}
}

static void centipede_recalcpalette()
{
	for (INT32 i = 0;i <= 0x0f; i++) {
		centipede_set_color(i, DrvPalRAM[i]);
	}
}

static UINT8 earom_read(UINT16 /*address*/)
{
	return (earom_data);
}

static void earom_write(UINT16 offset, UINT8 data)
{
	earom_offset = offset;
	earom_data = data;
}

static void earom_ctrl_write(UINT16 /*offset*/, UINT8 data)
{
	/*
		0x01 = clock
		0x02 = set data latch? - writes only (not always)
		0x04 = write mode? - writes only
		0x08 = set addr latch?
	*/

	if (data & 0x01)
		earom_data = earom[earom_offset];
	if ((data & 0x0c) == 0x0c)
	{
		earom[earom_offset] = earom_data;
	}
}

static void millipede_write(UINT16 address, UINT8 data)
{
	address &= 0x7fff; // 15bit addressing
	if (address >= 0x1000 && address <= 0x13bf) { // Video Ram
		DrvVidRAM[address - 0x1000] = data;
		return;
	}
	if (address >= 0x13c0 && address <= 0x13ff) { // Sprite Ram
		DrvSpriteRAM[address - 0x13c0] = data;
		return;
	}
	if (address >= 0x2480 && address <= 0x249f) { // Palette Ram
		DrvPalRAM[address - 0x2480] = data;
		milliped_set_color(address - 0x2480, data);
		return;
	}

	if (address >= 0x400 && address <= 0x40f) { // Pokey 1
		pokey1_w(address - 0x400, data);
		return;
	}

	if (address >= 0x800 && address <= 0x80f) { // Pokey 2
		pokey2_w(address - 0x800, data);
		return;
	}

	if (address >= 0x2780 && address <= 0x27bf) { // EAROM Write
		earom_write(address - 0x2780, data);
		return;
	}

	switch (address)
	{
		case 0x2505:
			m_dsw_select = (~data >> 7) & 1;
		return;

		case 0x2506:
			m_flipscreen = data >> 7;
		return;
		case 0x2507:
			m_control_select = (data >> 7) & 1;
		return;
		case 0x2600:
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;
		case 0x2700:
			earom_ctrl_write(0x2700, data);
		return;
	}
//	bprintf(0, _T("mw %X,"), address);
}

static void centipede_write(UINT16 address, UINT8 data)
{
	address &= 0x3fff; // 14bit addressing
	if (address >= 0x400 && address <= 0x7bf) { // Video Ram
		DrvVidRAM[address - 0x400] = data;
		return;
	}
	if (address >= 0x7c0 && address <= 0x7ff) { // Sprite Ram
		DrvSpriteRAM[address - 0x7c0] = data;
		return;
	}
	if (address >= 0x1400 && address <= 0x140f) { // Palette Ram
		DrvPalRAM[address - 0x1400] = data;
		centipede_set_color(address - 0x1400, data);
		return;
	}

	if (address >= 0x1000 && address <= 0x100f) { // Pokey #1
		pokey1_w(address - 0x1000, data);
		return;
	}

	if (address >= 0x1600 && address <= 0x163f) { // EAROM Write
		earom_write(address - 0x1600, data);
		return;
	}

	switch (address)
	{
		case 0x2000: // watchdog
		return;
		case 0x1c07:
			m_flipscreen = data >> 7;
		return;
		case 0x1680:
			earom_ctrl_write(0x1680, data);
		return;
		case 0x2507:
			m_control_select = (data >> 7) & 1;
		return;
		case 0x1800:
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;
	}

//	bprintf(0, _T("mw %X,"), address);
}

static INT32 read_trackball(INT32 idx, INT32 switch_port)
{
	INT32 newpos;

	/* adjust idx if we're cocktail flipped */
	if (m_flipscreen)
		idx += 2;

	/* if we're to read the dipswitches behind the trackball data, do it now */
	if (m_dsw_select)
		return (DrvInput[switch_port] & 0x7f) | sign[idx];

	/* get the new position and adjust the result */
	//newpos = readinputport(6 + idx);
	newpos = 0; // no trackball!! -dink
	if (newpos != oldpos[idx])
	{
		sign[idx] = (newpos - oldpos[idx]) & 0x80;
		oldpos[idx] = newpos;
	}

	/* blend with the bits from the switch port */
	return (DrvInput[switch_port] & 0x70) | (oldpos[idx] & 0x0f) | sign[idx];
}


static UINT8 millipede_read(UINT16 address)
{
	address &= 0x7fff; // 15bit addressing
	if (address >= 0x1000 && address <= 0x13bf) { // Video Ram
		return DrvVidRAM[address - 0x1000];
	}
	if (address >= 0x13c0 && address <= 0x13ff) { // Sprite Ram
		return DrvSpriteRAM[address - 0x13c0];
	}
	if (address >= 0x2480 && address <= 0x249f) { // Palette Ram
		return DrvPalRAM[address - 0x2480];
	}
	if (address >= 0x4000 && address <= 0x7fff) { // ROM
		return Drv6502ROM[address];
	}

	switch (address)
	{
		case 0x2000: return ((read_trackball(0, 0) | DrvDip[0]) & 0x3f) | ((vblank) ? 0x40 : 0x00);
		case 0x2001: return read_trackball(1, 1);// | DrvDip[1] ;
		case 0x2010: return DrvInput[2];
		case 0x2011: return DrvInput[3] | DrvDip[1];
		case 0x2030: return earom_read(address);
		case 0x0400:
		case 0x0401:
		case 0x0402:
		case 0x0403:
		case 0x0404:
		case 0x0405:
		case 0x0406:
		case 0x0407: return pokey1_r(address);
		case 0x0408: return DrvDip[2];
		case 0x0409:
		case 0x040a:
		case 0x040b:
		case 0x040c:
		case 0x040d:
		case 0x040e:
		case 0x040f: return pokey1_r(address);
		case 0x0800:
		case 0x0801:
		case 0x0802:
		case 0x0803:
		case 0x0804:
		case 0x0805:
		case 0x0806:
		case 0x0807: return pokey2_r(address);
		case 0x0808: return DrvDip[3];
		case 0x0809:
		case 0x080a:
		case 0x080b:
		case 0x080c:
		case 0x080d:
		case 0x080e:
		case 0x080f: return pokey2_r(address);
	}

	//bprintf(0, _T("mr %X,"), address);

	return 0;
}

static UINT8 centipede_read(UINT16 address)
{
	address &= 0x3fff; // 14bit addressing
	if (address >= 0x400 && address <= 0x7bf) { // Video Ram
		return DrvVidRAM[address - 0x400];
	}
	if (address >= 0x7c0 && address <= 0x7ff) { // Sprite Ram
		return DrvSpriteRAM[address - 0x7c0];
	}
	if (address >= 0x1400 && address <= 0x140f) { // Palette Ram
		return DrvPalRAM[address - 0x1400];
	}
	if (address >= 0x2000 && address <= 0x3fff) { // ROM
		return Drv6502ROM[address];
	}
	if (address >= 0x1700 && address <= 0x173f) { // EAROM
		return earom_read(address);
	}

	switch (address)
	{
		case 0x0c00: return ((read_trackball(0, 0) | DrvDip[0]) & 0x3f) | ((vblank) ? 0x40 : 0x00);
		case 0x0c01: return DrvInput[1];
		case 0x0c02: return read_trackball(1, 2);
		case 0x0c03: return DrvInput[3];
		case 0x0800: return DrvDip[1];
		case 0x0801: return DrvDip[2];
		case 0x1000:
		case 0x1001:
		case 0x1002:
		case 0x1003:
		case 0x1004:
		case 0x1005:
		case 0x1006:
		case 0x1007: return pokey1_r(address);
		case 0x1008: return DrvDip[2];
		case 0x1009:
		case 0x100a:
		case 0x100b:
		case 0x100c:
		case 0x100d:
		case 0x100e:
		case 0x100f: return pokey1_r(address);
	}

//	bprintf(0, _T("mr %X,"), address);

	return 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);
	m_dsw_select = 0;
	m_flipscreen = 0;

	M6502Open(0);
	M6502Reset();
	M6502Close();

	earom_offset = 0;
	earom_data = 0;
	//memset(&earom, 0, EAROM_SIZE); // only clear this @ init

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv6502ROM		= Next; Next += 0x012000;

	DrvPalette		= (UINT32*)Next; Next += 0x0600 * sizeof(UINT32);
	DrvBGGFX        = Next; Next += 0x10000;
	DrvSpriteGFX    = Next; Next += 0x10000;

	AllRam			= Next;

	Drv6502RAM		= Next; Next += 0x00400;
	DrvVidRAM		= Next; Next += 0x01000;
	DrvSpriteRAM	= Next; Next += 0x01000;
	DrvPalRAM		= Next; Next += 0x01000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void init_penmask()
{
	int i;

	for (i = 0; i < 64; i++)
	{
		UINT8 mask = 1;
		if (((i >> 0) & 3) == 0) mask |= 2;
		if (((i >> 2) & 3) == 0) mask |= 4;
		if (((i >> 4) & 3) == 0) mask |= 8;
		m_penmask[i] = mask;
	}
}

static INT32 CharPlaneOffsets[2] = { 256*8*8, 0  };
static INT32 CharXOffsets[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
static INT32 CharYOffsets[8] = { 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 };

static INT32 SpritePlaneOffsets[2] = { 128*16*8, 0 };
static INT32 SpriteXOffsets[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 0, 0, 0, 0, 0, 0, 0, 0 };
static INT32 SpriteYOffsets[16] = { 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
                                    8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 };

static INT32 DrvInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv6502ROM + 0x4000, 0, 1)) return 1;
		if (BurnLoadRom(Drv6502ROM + 0x5000, 1, 1)) return 1;
		if (BurnLoadRom(Drv6502ROM + 0x6000, 2, 1)) return 1;
		if (BurnLoadRom(Drv6502ROM + 0x7000, 3, 1)) return 1;

		UINT8 *DrvTempRom = (UINT8 *)BurnMalloc(0x10000);
		memset(DrvTempRom, 0, 0x10000);
		if (BurnLoadRom(DrvTempRom         , 4, 1)) return 1;
		if (BurnLoadRom(DrvTempRom+0x800   , 5, 1)) return 1;
		GfxDecode(0x100, 2, 8, 8, CharPlaneOffsets, CharXOffsets, CharYOffsets, 0x40, DrvTempRom, DrvBGGFX);
		GfxDecode(0x80, 2, 8, 16, SpritePlaneOffsets, SpriteXOffsets, SpriteYOffsets, 0x80, DrvTempRom, DrvSpriteGFX);
		BurnFree(DrvTempRom);
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(Drv6502RAM,	0x0000, 0x03ff, MAP_RAM);
	M6502MapMemory(Drv6502ROM + 0x4000,	0x4000, 0x7fff, MAP_ROM);
	M6502SetWriteHandler(millipede_write);
	M6502SetReadHandler(millipede_read);
	M6502SetReadOpArgHandler(millipede_read);
	M6502SetReadOpHandler(millipede_read);
	M6502Close();

	PokeyInit(12096000/8, 2, 1.00, 0);

	init_penmask();

	GenericTilesInit();

	memset(&earom, 0, sizeof(earom)); // don't put this in DrvDoReset()

	DrvDoReset();

	return 0;
}

static INT32 DrvInitcentiped()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv6502ROM + 0x2000, 0, 1)) return 1;
		if (BurnLoadRom(Drv6502ROM + 0x2800, 1, 1)) return 1;
		if (BurnLoadRom(Drv6502ROM + 0x3000, 2, 1)) return 1;
		if (BurnLoadRom(Drv6502ROM + 0x3800, 3, 1)) return 1;

		UINT8 *DrvTempRom = (UINT8 *)BurnMalloc(0x10000);
		memset(DrvTempRom, 0, 0x10000);
		if (BurnLoadRom(DrvTempRom         , 4, 1)) return 1;
		if (BurnLoadRom(DrvTempRom+0x800   , 5, 1)) return 1;
		GfxDecode(0x100, 2, 8, 8, CharPlaneOffsets, CharXOffsets, CharYOffsets, 0x40, DrvTempRom, DrvBGGFX);
		GfxDecode(0x80, 2, 8, 16, SpritePlaneOffsets, SpriteXOffsets, SpriteYOffsets, 0x80, DrvTempRom, DrvSpriteGFX);
		BurnFree(DrvTempRom);
	}

	centipedemode = 1;

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(Drv6502RAM,	0x0000, 0x03ff, MAP_RAM);
	M6502MapMemory(Drv6502ROM + 0x2000,	0x2000, 0x3fff, MAP_ROM);
	M6502SetWriteHandler(centipede_write);
	M6502SetReadHandler(centipede_read);
	M6502SetReadOpArgHandler(centipede_read);
	M6502SetReadOpHandler(centipede_read);
	M6502Close();

	PokeyInit(12096000/8, 2, 2.40, 0);

	init_penmask();

	GenericTilesInit();

	memset(&earom, 0, sizeof(earom)); // don't put this in DrvDoReset()

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	PokeyExit();

	M6502Exit();

	BurnFree(AllMem);

	centipedemode = 0;
	m_dsw_select = 0;
	m_flipscreen = 0;

	return 0;
}


extern int counter;


static void draw_bg()
{
	UINT8 *videoram = DrvVidRAM;
	for (INT32 offs = 0; offs <= 0x3bf; offs++)
	{
		int flip_tiles;
		int sx = offs % 32;
		int sy = offs / 32;

		int data = videoram[offs];
		int bank = ((data >> 6) & 1);
		int color = (data >> 6) & 3;
		// Flip both x and y if flipscreen is non-zero
		flip_tiles = (m_flipscreen) ? 0x03 : 0;
		if (centipedemode) {
			bank = 0;
			color = 0;
			flip_tiles = data >> 6;
		}
		int code = (data & 0x3f) + 0x40 + (bank * 0x80);


		sx = 8 * sx;
		sy = 8 * sy;
		if (sx >= nScreenWidth) continue;
		if (sy >= nScreenHeight) continue;

		if (flip_tiles) {
			Render8x8Tile_FlipXY_Clip(pTransDraw, code, 248 - sx, 184 - sy, color, 2, 0, DrvBGGFX);
		} else {
			Render8x8Tile_Clip(pTransDraw, code, sx, sy, color, 2, 0, DrvBGGFX);
		}
	}
}

static void RenderTileCPMP(INT32 code, INT32 color, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy, INT32 width, INT32 height)
{
	UINT16 *dest = pTransDraw;
	UINT8 *gfx = DrvSpriteGFX;

	INT32 flip = 0;
	if (flipy) flip |= (height - 1) * width;
	if (flipx) flip |= width - 1;

	gfx += code * width * height;

	for (INT32 y = 0; y < height; y++, sy++) {
		if (sy < 0 || sy >= nScreenHeight) continue;

		for (INT32 x = 0; x < width; x++, sx++) {
			if (sx < 0 || sx >= nScreenWidth) continue;

			INT32 pxl = gfx[((y * width) + x) ^ flip];

			if (m_penmask[color & 0x3f] & (1 << pxl) || !pxl) continue; // is this right?
			dest[sy * nScreenWidth + sx] = pxl | (color << 2) | 0x100;
		}
		sx -= width;
	}
}


static void draw_sprites()
{
	UINT8 *spriteram = DrvSpriteRAM;
	for (INT32 offs = 0; offs < 0x10; offs++)
	{
		int code = ((spriteram[offs] & 0x3e) >> 1) | ((spriteram[offs] & 0x01) << 6);
		int color = spriteram[offs + 0x30];
		int flipx = (centipedemode) ? (spriteram[offs] >> 6) & 1 : m_flipscreen;
		int flipy = (centipedemode) ? (spriteram[offs] >> 7) & 1 : (spriteram[offs] & 0x80);
		int x = spriteram[offs + 0x20];
		int y = 240 - spriteram[offs + 0x10];
		if (flipx && !centipedemode) {
			flipy = !flipy;
		}

		if (x + 8 >= nScreenWidth) continue; // clip top 8px of sprites (top of screen)

		RenderTileCPMP(code, color, x, y, flipx, flipy, 8, 16);
	}
}

static INT32 DrvDraw()
{
	BurnTransferClear();

    draw_bg();
	draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void DrvMakeInputs()
{
	// Reset Inputs - bring active-LOW stuff HIGH
	DrvInput[0] = (centipedemode) ? 0x20 : 0x10 + 0x20;
	DrvInput[1] = (centipedemode) ? 0xff : 0x01 + 0x02 + 0x10 + 0x20 + 0x40;
	DrvInput[2] = 0xff;
	DrvInput[3] = (centipedemode) ? 0xff : 0x01 + 0x02 + 0x04 + 0x08 + 0x10 + 0x40;

	// Compile Digital Inputs
	for (INT32 i = 0; i < 8; i++) {
		DrvInput[0] -= (DrvJoy1[i] & 1) << i;
		DrvInput[1] -= (DrvJoy2[i] & 1) << i;
		DrvInput[2] -= (DrvJoy3[i] & 1) << i;
		DrvInput[3] -= (DrvJoy4[i] & 1) << i;
	}

}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	if(DrvRecalc) {
		if (centipedemode)
			centipede_recalcpalette();
		else
			millipede_recalcpalette();
		DrvRecalc = 0;
	}

	DrvMakeInputs();

	M6502NewFrame();

	INT32 nTotalCycles = 12096000/8 / 60;
	INT32 nInterleave = 4;

	vblank = 0;

	M6502Open(0);
	INT32 nNext, nCyclesDone = 0, nCyclesSegment;

	for (INT32 i = 0; i < nInterleave; i++) {
		nNext = (i + 1) * nTotalCycles / nInterleave;
		nCyclesSegment = nNext - nCyclesDone;
		nCyclesDone += M6502Run(nCyclesSegment);
		M6502SetIRQLine(0, CPU_IRQSTATUS_AUTO);

		if (i == 2)
		    vblank = 1;
	}

	M6502Close();
	if (pBurnSoundOut) {
		pokey_update(0, pBurnSoundOut, nBurnSoundLen);
		pokey_update(1, pBurnSoundOut, nBurnSoundLen);
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
		*pnMin = 0x029722;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		M6502Scan(nAction);

		pokey_scan(nAction, pnMin);

		SCAN_VAR(earom_offset);
		SCAN_VAR(earom_data);
		SCAN_VAR(m_dsw_select);
		SCAN_VAR(m_control_select);
		SCAN_VAR(m_flipscreen);
	}

	if (nAction & ACB_NVRAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data		= earom;
		ba.nLen		= sizeof(earom);
		ba.szName	= "NV RAM";
		BurnAcb(&ba);
	}

	return 0;
}


// Millipede

static struct BurnRomInfo millipedRomDesc[] = {
	{ "136013-104.mn1",	0x1000, 0x40711675, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "136013-103.l1",	0x1000, 0xfb01baf2, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136013-102.jk1",	0x1000, 0x62e137e0, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136013-101.h1",	0x1000, 0x46752c7d, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "136013-107.r5",	0x0800, 0x68c3437a, 2 | BRF_PRG | BRF_ESS }, //  4 gfx1
	{ "136013-106.p5",	0x0800, 0xf4468045, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "136001-213.e7",	0x0100, 0x6fa3093a, 3 | BRF_PRG | BRF_ESS }, //  6 proms
};

STD_ROM_PICK(milliped)
STD_ROM_FN(milliped)

struct BurnDriver BurnDrvMilliped = {
	"milliped", NULL, NULL, NULL, "1982",
	"Millipede\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_16BIT_ONLY, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, millipedRomInfo, millipedRomName, NULL, NULL, MillipedInputInfo, MillipedDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x600,
	240, 256, 3, 4
};

// Centipede (revision 4)
static struct BurnRomInfo centipedRomDesc[] = {
	{ "136001-407.d1",	0x0800, 0xc4d995eb, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "136001-408.e1",	0x0800, 0xbcdebe1b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136001-409.fh1",	0x0800, 0x66d7b04a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136001-410.j1",	0x0800, 0x33ce4640, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "136001-211.f7",	0x0800, 0x880acfb9, 2 | BRF_PRG | BRF_ESS }, //  4 gfx1
	{ "136001-212.hj7",	0x0800, 0xb1397029, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "136001-213.p4",	0x0100, 0x6fa3093a, 3 | BRF_PRG | BRF_ESS }, //  6 proms
};


STD_ROM_PICK(centiped)
STD_ROM_FN(centiped)

struct BurnDriver BurnDrvCentiped = {
	"centiped", NULL, NULL, NULL, "1980",
	"Centipede (revision 4)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_16BIT_ONLY, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, centipedRomInfo, centipedRomName, NULL, NULL, CentipedInputInfo, CentipedDIPInfo,
	DrvInitcentiped, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x600,
	240, 256, 3, 4
};

// Centipede (revision 3)

static struct BurnRomInfo centiped3RomDesc[] = {
	{ "136001-307.d1",	0x0800, 0x5ab0d9de, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "136001-308.e1",	0x0800, 0x4c07fd3e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136001-309.fh1",	0x0800, 0xff69b424, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136001-310.j1",	0x0800, 0x44e40fa4, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "136001-211.f7",	0x0800, 0x880acfb9, 2 | BRF_PRG | BRF_ESS }, //  4 gfx1
	{ "136001-212.hj7",	0x0800, 0xb1397029, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "136001-213.p4",	0x0100, 0x6fa3093a, 3 | BRF_PRG | BRF_ESS }, //  6 proms
};

STD_ROM_PICK(centiped3)
STD_ROM_FN(centiped3)

struct BurnDriver BurnDrvCentiped3 = {
	"centiped3", "centiped", NULL, NULL, "1980",
	"Centipede (revision 3)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_16BIT_ONLY, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, centiped3RomInfo, centiped3RomName, NULL, NULL, CentipedInputInfo, CentipedDIPInfo,
	DrvInitcentiped, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x600,
	240, 256, 3, 4
};

