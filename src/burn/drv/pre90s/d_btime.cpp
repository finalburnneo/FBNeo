// BurgerTime emu-layer for FB Alpha by dink & iq_132, based on the MAME driver by Zsolt Vasvari.

// Todo:
// Zoar - Sound and Input issues, after coining-up it starts automatically.
//        Also missing sounds in some places...

#include "tiles_generic.h"
#include "m6502_intf.h"
#include "bitswap.h"
#include "driver.h"
#include "flt_rc.h"

extern "C" {
    #include "ay8910.h"
}

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvMainROM;
static UINT8 *DrvMainROMdec;
static UINT8 *DrvSoundROM;
static UINT8 *DrvColPROM;

static UINT8 *DrvMainRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvBGRAM;
static UINT8 *DrvColRAM;
static UINT8 *DrvCharRAM; // disco
static UINT8 *DrvSpriteRAM; // disco
static UINT8 *DrvScrollRAM;
static UINT8 *DrvSoundRAM;
static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 *DrvBgMapROM;
static UINT16 *DrvBGBitmap;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static INT32 gfx0len;
static INT32 gfx1len;

static INT16 *pAY8910Buffer[6];
static INT16 *hpfiltbuffer;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvReset;
static UINT8 DrvInputs[3];

static UINT8 vblank;
static UINT8 soundlatch;
static UINT8 flipscreen;
static UINT8 audio_nmi_type;
static UINT8 audio_nmi_enable;
static UINT8 audio_nmi_state;
static UINT8 bnj_scroll1;
static UINT8 bnj_scroll2;
static UINT8 lnc_charbank;
static UINT8 btime_palette; // disco, zoar

// feature: speed up the sound processor init, since it takes several seconds
// after the game boots. with this, it inits instantly by overclocking the sound
// processor for 10 frames.
#define ZIPPYSOUNDFRAMES 10
static UINT8 zippysoundinit;

// mmonkey protection stuff
INT32 protection_command;
INT32 protection_status;
INT32 protection_value;
INT32 protection_ret;

static UINT8 btimemode = 0;
static UINT8 btime3mode = 0;
static UINT8 brubbermode = 0;
static UINT8 discomode = 0;
static UINT8 lncmode = 0;
static UINT8 zoarmode = 0;
static UINT8 mmonkeymode = 0;

static UINT8 bnjskew = 0;

enum
{
	AUDIO_ENABLE_NONE,
	AUDIO_ENABLE_DIRECT,        /* via direct address in memory map */
	AUDIO_ENABLE_AY8910         /* via ay-8910 port A */
};


static struct BurnInputInfo BtimeInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 6,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 7,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Tilt",		BIT_DIGITAL,	DrvJoy3 + 2,	"tilt"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
};

STDINPUTINFO(Btime)


static struct BurnDIPInfo BtimeDIPList[]=
{
	{0x10, 0xff, 0xff, 0x3f, NULL		},
	{0x11, 0xff, 0xff, 0x0b, NULL		},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x10, 0x01, 0x03, 0x00, "2 Coins 1 Credits"		},
	{0x10, 0x01, 0x03, 0x03, "1 Coin  1 Credits"		},
	{0x10, 0x01, 0x03, 0x02, "1 Coin  2 Credits"		},
	{0x10, 0x01, 0x03, 0x01, "1 Coin  3 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x10, 0x01, 0x0c, 0x00, "2 Coins 1 Credits"		},
	{0x10, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"		},
	{0x10, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"		},
	{0x10, 0x01, 0x0c, 0x04, "1 Coin  3 Credits"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x10, 0x01, 0x10, 0x10, "Off"		},
	{0x10, 0x01, 0x10, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Cross Hatch Pattern"		},
	{0x10, 0x01, 0x20, 0x20, "Off"		},
	{0x10, 0x01, 0x20, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Control Panel"		},
	{0x10, 0x01, 0x40, 0x00, "Upright"		},
	{0x10, 0x01, 0x40, 0x40, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x11, 0x01, 0x01, 0x01, "3"		},
	{0x11, 0x01, 0x01, 0x00, "5"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x11, 0x01, 0x06, 0x06, "10000"		},
	{0x11, 0x01, 0x06, 0x04, "15000"		},
	{0x11, 0x01, 0x06, 0x02, "20000"		},
	{0x11, 0x01, 0x06, 0x00, "30000"		},

	{0   , 0xfe, 0   ,    2, "Enemies"		},
	{0x11, 0x01, 0x08, 0x08, "4"		},
	{0x11, 0x01, 0x08, 0x00, "6"		},

	{0   , 0xfe, 0   ,    2, "End of Level Pepper"		},
	{0x11, 0x01, 0x10, 0x10, "No"		},
	{0x11, 0x01, 0x10, 0x00, "Yes"		},
};

STDDIPINFO(Btime)

static struct BurnInputInfo MmonkeyInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 6,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 7,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
};

STDINPUTINFO(Mmonkey)


static struct BurnDIPInfo MmonkeyDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x1f, NULL		},
	{0x10, 0xff, 0xff, 0x89, NULL		},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x0f, 0x01, 0x03, 0x00, "2 Coins 1 Credits"		},
	{0x0f, 0x01, 0x03, 0x03, "1 Coin  1 Credits"		},
	{0x0f, 0x01, 0x03, 0x02, "1 Coin  2 Credits"		},
	{0x0f, 0x01, 0x03, 0x01, "1 Coin  3 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x0f, 0x01, 0x0c, 0x00, "2 Coins 1 Credits"		},
	{0x0f, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"		},
	{0x0f, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"		},
	{0x0f, 0x01, 0x0c, 0x04, "1 Coin  3 Credits"		},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x0f, 0x01, 0x10, 0x10, "Off"		},
	{0x0f, 0x01, 0x10, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Control Panel"		},
	{0x0f, 0x01, 0x40, 0x00, "Upright"		},
	{0x0f, 0x01, 0x40, 0x40, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Service"		},
	{0x10, 0x01, 0x80, 0x80, "Off"		},
	{0x10, 0x01, 0x80, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x10, 0x01, 0x01, 0x01, "3"		},
	{0x10, 0x01, 0x01, 0x00, "5"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x10, 0x01, 0x06, 0x02, "Every 15000"		},
	{0x10, 0x01, 0x06, 0x04, "Every 30000"		},
	{0x10, 0x01, 0x06, 0x00, "20000"		},
	{0x10, 0x01, 0x06, 0x06, "None"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x10, 0x01, 0x18, 0x18, "Easy"		},
	{0x10, 0x01, 0x18, 0x08, "Medium"		},
	{0x10, 0x01, 0x18, 0x10, "Hard"		},
	{0x10, 0x01, 0x18, 0x00, "Level Skip Mode (Cheat)"		},
};

STDDIPINFO(Mmonkey)

static struct BurnInputInfo BnjInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 6,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 7,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Tilt",		BIT_DIGITAL,	DrvJoy3 + 0,	"tilt"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
};

STDINPUTINFO(Bnj)


static struct BurnDIPInfo BnjDIPList[]=
{
	{0x10, 0xff, 0xff, 0x1f, NULL		},
	{0x11, 0xff, 0xff, 0x17, NULL		},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x10, 0x01, 0x03, 0x00, "2 Coins 1 Credits"		},
	{0x10, 0x01, 0x03, 0x03, "1 Coin  1 Credits"		},
	{0x10, 0x01, 0x03, 0x02, "1 Coin  2 Credits"		},
	{0x10, 0x01, 0x03, 0x01, "1 Coin  3 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x10, 0x01, 0x0c, 0x00, "2 Coins 1 Credits"		},
	{0x10, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"		},
	{0x10, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"		},
	{0x10, 0x01, 0x0c, 0x04, "1 Coin  3 Credits"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x10, 0x01, 0x10, 0x10, "Off"		},
	{0x10, 0x01, 0x10, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x10, 0x01, 0x40, 0x00, "Upright"		},
	{0x10, 0x01, 0x40, 0x40, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x11, 0x01, 0x01, 0x01, "3"		},
	{0x11, 0x01, 0x01, 0x00, "5"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x11, 0x01, 0x06, 0x06, "Every 30000"		},
	{0x11, 0x01, 0x06, 0x04, "Every 70000"		},
	{0x11, 0x01, 0x06, 0x02, "20000 Only"		},
	{0x11, 0x01, 0x06, 0x00, "30000 Only"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x11, 0x01, 0x08, 0x08, "No"		},
	{0x11, 0x01, 0x08, 0x00, "Yes"		},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x11, 0x01, 0x10, 0x10, "Easy"		},
	{0x11, 0x01, 0x10, 0x00, "Hard"		},
};

STDDIPINFO(Bnj)

static struct BurnInputInfo DiscoInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 6,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 7,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
};

STDINPUTINFO(Disco)


static struct BurnDIPInfo DiscoDIPList[]=
{
	{0x0f, 0xff, 0xff, 0xc0, NULL		},
	{0x10, 0xff, 0xff, 0x00, NULL		},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x0f, 0x01, 0x03, 0x03, "2 Coins 1 Credits"		},
	{0x0f, 0x01, 0x03, 0x00, "1 Coin  1 Credits"		},
	{0x0f, 0x01, 0x03, 0x01, "1 Coin  2 Credits"		},
	{0x0f, 0x01, 0x03, 0x02, "1 Coin  3 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x0f, 0x01, 0x0c, 0x0c, "2 Coins 1 Credits"		},
	{0x0f, 0x01, 0x0c, 0x00, "1 Coin  1 Credits"		},
	{0x0f, 0x01, 0x0c, 0x04, "1 Coin  2 Credits"		},
	{0x0f, 0x01, 0x0c, 0x08, "1 Coin  3 Credits"		},

	{0   , 0xfe, 0   ,    2, "Control Panel"		},
	{0x0f, 0x01, 0x40, 0x40, "Upright"		},
	{0x0f, 0x01, 0x40, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0f, 0x01, 0x80, 0x80, "Upright"		},
	{0x0f, 0x01, 0x80, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x10, 0x01, 0x01, 0x00, "3"		},
	{0x10, 0x01, 0x01, 0x01, "5"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x10, 0x01, 0x06, 0x00, "10000"		},
	{0x10, 0x01, 0x06, 0x02, "20000"		},
	{0x10, 0x01, 0x06, 0x04, "30000"		},
	{0x10, 0x01, 0x06, 0x06, "None"		},

	{0   , 0xfe, 0   ,    2, "Music Weapons"		},
	{0x10, 0x01, 0x08, 0x00, "5"		},
	{0x10, 0x01, 0x08, 0x08, "8"		},

	{0   , 0xfe, 0   ,    2, "Game Speed"		},
	{0x10, 0x01, 0x10, 0x00, "Slow"		},
	{0x10, 0x01, 0x10, 0x10, "Fast"		},

	{0   , 0xfe, 0   ,    6, "Country Code"		},
	{0x10, 0x01, 0xe0, 0x00, "A"		},
	{0x10, 0x01, 0xe0, 0x20, "B"		},
	{0x10, 0x01, 0xe0, 0x40, "C"		},
	{0x10, 0x01, 0xe0, 0x60, "D"		},
	{0x10, 0x01, 0xe0, 0x80, "E"		},
	{0x10, 0x01, 0xe0, 0xa0, "F"		},
};

STDDIPINFO(Disco)

static struct BurnInputInfo LncInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 6,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 7,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
};

STDINPUTINFO(Lnc)


static struct BurnDIPInfo LncDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x3f, NULL		},
	{0x10, 0xff, 0xff, 0x0f, NULL		},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x0f, 0x01, 0x03, 0x00, "2 Coins 1 Credits"		},
	{0x0f, 0x01, 0x03, 0x03, "1 Coin  1 Credits"		},
	{0x0f, 0x01, 0x03, 0x02, "1 Coin  2 Credits"		},
	{0x0f, 0x01, 0x03, 0x01, "1 Coin  3 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x0f, 0x01, 0x0c, 0x00, "2 Coins 1 Credits"		},
	{0x0f, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"		},
	{0x0f, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"		},
	{0x0f, 0x01, 0x0c, 0x04, "1 Coin  3 Credits"		},

	{0   , 0xfe, 0   ,    4, "Test Mode"		},
	{0x0f, 0x01, 0x30, 0x30, "Off"		},
	{0x0f, 0x01, 0x30, 0x00, "RAM Test Only"		},
	{0x0f, 0x01, 0x30, 0x20, "Watchdog Test Only"		},
	{0x0f, 0x01, 0x30, 0x10, "All Tests"		},

	{0   , 0xfe, 0   ,    2, "Control Panel"		},
	{0x0f, 0x01, 0x40, 0x00, "Upright"		},
	{0x0f, 0x01, 0x40, 0x40, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x10, 0x01, 0x01, 0x01, "3"		},
	{0x10, 0x01, 0x01, 0x00, "5"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x10, 0x01, 0x06, 0x06, "15000"		},
	{0x10, 0x01, 0x06, 0x04, "20000"		},
	{0x10, 0x01, 0x06, 0x02, "30000"		},
	{0x10, 0x01, 0x06, 0x00, "None"		},

	{0   , 0xfe, 0   ,    2, "Game Speed"		},
	{0x10, 0x01, 0x08, 0x08, "Slow"		},
	{0x10, 0x01, 0x08, 0x00, "Hard"		},
};

STDDIPINFO(Lnc)

static struct BurnInputInfo ZoarInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 6,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 fire 3"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 7,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 fire 3"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
};

STDINPUTINFO(Zoar)


static struct BurnDIPInfo ZoarDIPList[]=
{
	{0x13, 0xff, 0xff, 0x2f, NULL		},
	{0x14, 0xff, 0xff, 0x1f, NULL		},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x13, 0x01, 0x03, 0x00, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x03, 0x03, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x03, 0x02, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x03, 0x01, "1 Coin  3 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x13, 0x01, 0x0c, 0x00, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x0c, 0x04, "1 Coin  3 Credits"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x13, 0x01, 0x20, 0x20, "Off"		},
	{0x13, 0x01, 0x20, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Control Panel"		},
	{0x13, 0x01, 0x40, 0x00, "Upright"		},
	{0x13, 0x01, 0x40, 0x40, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x14, 0x01, 0x01, 0x01, "3"		},
	{0x14, 0x01, 0x01, 0x00, "5"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x14, 0x01, 0x06, 0x06, "5000"		},
	{0x14, 0x01, 0x06, 0x04, "10000"		},
	{0x14, 0x01, 0x06, 0x02, "15000"		},
	{0x14, 0x01, 0x06, 0x00, "20000"		},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x14, 0x01, 0x08, 0x08, "Easy"		},
	{0x14, 0x01, 0x08, 0x00, "Hard"		},

	{0   , 0xfe, 0   ,    2, "Weapon Select"		},
	{0x14, 0x01, 0x10, 0x00, "Manual"		},
	{0x14, 0x01, 0x10, 0x10, "Auto"		},
};

STDDIPINFO(Zoar)


#define WB(a1,a2,dest) if (address >= a1 && address <= a2) { \
		dest[address - a1] = data; \
		return; \
	}
#define RB(a1,a2,dest) if (address >= a1 && address <= a2) \
		return dest[address - a1];


#define mmonkeyBASE  0xb000

static UINT8 mmonkey_protection_r(UINT16 offset)
{
	UINT8 *RAM = DrvMainRAM;
	INT32 ret = 0;

	if (offset == 0x0000)
		ret = protection_status;
	else if (offset == 0x0e00)
		ret = protection_ret;
	else if (offset >= 0x0d00 && offset <= 0x0d02)
		ret = RAM[mmonkeyBASE + offset];  // addition result

	return ret;
}

static void mmonkey_protection_w(UINT16 offset, UINT8 data)
{
	UINT8 *RAM = DrvMainRAM;

	if (offset == 0)
	{
		/* protection trigger */
		if (data == 0)
		{
			int i, s1, s2, r;

			switch (protection_command)
			{
			case 0: /* score addition */

				s1 = (1 * (RAM[mmonkeyBASE + 0x0d00] & 0x0f)) + (10 * (RAM[mmonkeyBASE + 0x0d00] >> 4)) +
					(100 * (RAM[mmonkeyBASE + 0x0d01] & 0x0f)) + (1000 * (RAM[mmonkeyBASE + 0x0d01] >> 4)) +
					(10000 * (RAM[mmonkeyBASE + 0x0d02] & 0x0f)) + (100000 * (RAM[mmonkeyBASE + 0x0d02] >> 4));

				s2 = (1 * (RAM[mmonkeyBASE + 0x0d03] & 0x0f)) + (10 * (RAM[mmonkeyBASE + 0x0d03] >> 4)) +
					(100 * (RAM[mmonkeyBASE + 0x0d04] & 0x0f)) + (1000 * (RAM[mmonkeyBASE + 0x0d04] >> 4)) +
					(10000 * (RAM[mmonkeyBASE + 0x0d05] & 0x0f)) + (100000 * (RAM[mmonkeyBASE + 0x0d05] >> 4));

				r = s1 + s2;

				RAM[mmonkeyBASE + 0x0d00]  =  (r % 10);        r /= 10;
				RAM[mmonkeyBASE + 0x0d00] |= ((r % 10) << 4);  r /= 10;
				RAM[mmonkeyBASE + 0x0d01]  =  (r % 10);        r /= 10;
				RAM[mmonkeyBASE + 0x0d01] |= ((r % 10) << 4);  r /= 10;
				RAM[mmonkeyBASE + 0x0d02]  =  (r % 10);        r /= 10;
				RAM[mmonkeyBASE + 0x0d02] |= ((r % 10) << 4);

				break;

			case 1: /* decryption */

				/* Compute return value by searching the decryption table. */
				/* During the search the status should be 2, but we're done */
				/* instanteniously in emulation time */
				for (i = 0; i < 0x100; i++)
				{
					if (RAM[mmonkeyBASE + 0x0f00 + i] == protection_value)
					{
						protection_ret = i;
						break;
					}
				}
				break;

			default:
				// Uhoh.
				break;
			}

			protection_status = 0;
		}
	}
	else if (offset == 0x0c00)
		protection_command = data;
	else if (offset == 0x0e00)
		protection_value = data;
	else if (offset >= 0x0f00)
		RAM[mmonkeyBASE + offset] = data;   /* decrypt table */
	else if (offset >= 0x0d00 && offset <= 0x0d05)
		RAM[mmonkeyBASE + offset] = data;   /* source table */
}

static void lncpaletteupdate()
{
	const UINT8 *color_prom = DrvColPROM;

	for (INT32 i = 0; i < 0x20; i++)
	{
		INT32 bit0, bit1, bit2, r, g, b;

		/* red component */
		bit0 = (color_prom[i] >> 7) & 0x01;
		bit1 = (color_prom[i] >> 6) & 0x01;
		bit2 = (color_prom[i] >> 5) & 0x01;
		r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		/* green component */
		bit0 = (color_prom[i] >> 4) & 0x01;
		bit1 = (color_prom[i] >> 3) & 0x01;
		bit2 = (color_prom[i] >> 2) & 0x01;
		g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		/* blue component */
		bit0 = 0;
		bit1 = (color_prom[i] >> 1) & 0x01;
		bit2 = (color_prom[i] >> 0) & 0x01;
		b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}
}

static void discopaletteupdate()
{
	const UINT8 *color_prom = DrvColPROM;

	for (INT32 i = 0; i < 0x40; i++)
	{
		INT32 bit0, bit1, bit2, r, g, b;

		/* red component */
		bit0 = (color_prom[i] >> 0) & 0x01;
		bit1 = (color_prom[i] >> 1) & 0x01;
		bit2 = (color_prom[i] >> 2) & 0x01;
		r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		/* green component */
		bit0 = (color_prom[i] >> 3) & 0x01;
		bit1 = (color_prom[i] >> 4) & 0x01;
		bit2 = (color_prom[i] >> 5) & 0x01;
		g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		/* blue component */
		bit0 = 0;
		bit1 = (color_prom[i] >> 6) & 0x01;
		bit2 = (color_prom[i] >> 7) & 0x01;
		b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}
}

static void btimepalettewrite(UINT16 offset, UINT8 data)
{
	INT32 bit0, bit1, bit2;

	bit0 = (data >> 0) & 0x01;
	bit1 = (data >> 1) & 0x01;
	bit2 = (data >> 2) & 0x01;
	INT32 r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

	bit0 = (data >> 3) & 0x01;
	bit1 = (data >> 4) & 0x01;
	bit2 = (data >> 5) & 0x01;
	INT32 g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

	bit0 = 0;
	bit1 = (data >> 6) & 0x01;
	bit2 = (data >> 7) & 0x01;
	INT32 b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

	if (offset == 3 && bnjskew) { // hack to make some stuff readable in bnj.
		r = 0xff;
		g = 0xb8;
		b = 0x00;
	}

	DrvPalette[offset] = BurnHighCol(r, g, b, 0);
}

static UINT8 btime_main_read(UINT16 address)
{
	RB(0x0000, 0x07ff, DrvMainRAM);
	RB(0x0c00, 0x0c1f, DrvPalRAM);
	RB(0x1000, 0x13ff, DrvVidRAM);
	RB(0x1400, 0x17ff, DrvColRAM);

	if (address >= 0xb000 && address <= 0xffff) {
		return DrvMainROM[address];
	}

	if (address >= 0x1800 && address <= 0x1bff) {
		INT32 offset = address & 0x3ff;
		return DrvVidRAM[(((offset % 32) * 32) + (offset / 32))];
	}

	if (address >= 0x1c00 && address <= 0x1fff) {
		INT32 offset = address & 0x3ff;
		return DrvColRAM[(((offset % 32) * 32) + (offset / 32))];
	}

	switch (address)
	{
		case 0x4000:
			return DrvInputs[0];

		case 0x4001:
			return DrvInputs[1];

		case 0x4002:
			return DrvInputs[2];

		case 0x4003:
			return (DrvDips[0] & 0x7f) | vblank;

		case 0x4004:
			return DrvDips[1];
	}
	return 0;
}

static UINT8 bnj_main_read(UINT16 address)
{
	RB(0x0000, 0x07ff, DrvMainRAM);
	RB(0x5c00, 0x5c1f, DrvPalRAM);
	RB(0x4000, 0x43ff, DrvVidRAM);
	RB(0x4400, 0x47ff, DrvColRAM);

	if (address >= 0xa000 && address <= 0xffff) {
		return DrvMainROM[address];
	}

	if (address >= 0x4800 && address <= 0x4bff) {
		INT32 offset = address & 0x3ff;
		return DrvVidRAM[(((offset % 32) * 32) + (offset / 32))];
	}

	if (address >= 0x4c00 && address <= 0x4fff) {
		INT32 offset = address & 0x3ff;
		return DrvColRAM[(((offset % 32) * 32) + (offset / 32))];
	}

	switch (address)
	{
		case 0x1002:
			return DrvInputs[0];

		case 0x1003:
			return DrvInputs[1];

		case 0x1004:
			return DrvInputs[2];

		case 0x1000:
			return (DrvDips[0] & 0x7f) | vblank;

		case 0x1001:
			return DrvDips[1];
	}
	return 0;
}

static UINT8 zoar_main_read(UINT16 address)
{
	RB(0x0000, 0x07ff, DrvMainRAM);
	RB(0x8000, 0x83ff, DrvVidRAM);
	RB(0x8400, 0x87ff, DrvColRAM);

	if (address >= 0xd000 && address <= 0xffff) {
		return DrvMainROM[address];
	}

	if (address >= 0x8800 && address <= 0x8bff) {
		INT32 offset = address & 0x3ff;
		return DrvVidRAM[(((offset % 32) * 32) + (offset / 32))];
	}

	if (address >= 0x8c00 && address <= 0x8fff) {
		INT32 offset = address & 0x3ff;
		return DrvColRAM[(((offset % 32) * 32) + (offset / 32))];
	}

	switch (address)
	{
		case 0x9800:
			return ((vblank) ? 0x00 : 0x80) | (DrvDips[0] & 0x7f);

		case 0x9801:
			return DrvDips[1];

		case 0x9802:
			return DrvInputs[0];

		case 0x9803:
			return DrvInputs[1];
	}
	return 0;
}

static UINT8 disco_main_read(UINT16 address)
{
	RB(0x0000, 0x07ff, DrvMainRAM);
	RB(0x2000, 0x7fff, DrvCharRAM);
	RB(0x8000, 0x83ff, DrvVidRAM);
	RB(0x8400, 0x87ff, DrvColRAM);
	RB(0x8800, 0x881f, DrvSpriteRAM);

	if (address >= 0xa000 && address <= 0xffff) {
		return DrvMainROM[address];
	}

	switch (address)
	{
		case 0x9200:
			return DrvInputs[0];

		case 0x9400:
			return DrvInputs[1];

		case 0x9000:
			return 0x1f + (DrvInputs[2] & 0xc0);

		case 0x9800:
			return DrvDips[0];

		case 0x9a00:
			return DrvDips[1];

		case 0x9c00:
			return 0x7f | vblank;
	}
	return 0;
}

static UINT8 mmonkeyop_main_read(UINT16 address)
{
	return DrvMainROMdec[address];
}

static UINT8 mmonkey_main_read(UINT16 address)
{
	RB(0x0000, 0x3bff, DrvMainRAM);
	RB(0x3c00, 0x3fff, DrvVidRAM);

	if (address >= 0xb000 && address <= 0xbfff) {
		if (lncmode) {
			return DrvCharRAM[address - 0xb000];
		} else {
			return mmonkey_protection_r(address - 0xb000);
		}
	}

	if (address >= 0xc000) {
		return DrvMainROM[address];
	}

	if (address >= 0x7c00 && address <= 0x7fff) {
		INT32 offset = address & 0x3ff;
		return DrvVidRAM[(((offset % 32) * 32) + (offset / 32))];
	}

	switch (address)
	{
		case 0x9000:
			return DrvInputs[0];

		case 0x9001:
			return DrvInputs[1];

		case 0x9002:
			return DrvInputs[2];

		case 0x8000:
			return (DrvDips[0] & 0x7f) | vblank;

		case 0x8001:
			return DrvDips[1];
	}
	return 0;
}

static void mmonkey_main_write(UINT16 address, UINT8 data)
{
	DrvMainRAM[address] = data;
	DrvMainROMdec[address] = data;

	if (address >= 0x3c00 && address <= 0x3fff) {
		DrvVidRAM[address - 0x3c00] = data;
		DrvColRAM[address - 0x3c00] = lnc_charbank;
		return;
	}

	if (address >= 0x7c00 && address <= 0x7fff) {
		INT32 offset = address & 0x3ff;
		DrvVidRAM[(((offset % 32) * 32) + (offset / 32))] = data;
		DrvColRAM[(((offset % 32) * 32) + (offset / 32))] = lnc_charbank;
		return;
	}

	if (address >= 0xb000 && address <= 0xbfff) {
		if (lncmode) {
			DrvCharRAM[address - 0xb000] = data;
		} else {
			mmonkey_protection_w(address - 0xb000, data);
		}
		return;
	}

	switch (address)
	{
		case 0x8003:
			lnc_charbank = data;
		return;

		case 0x9002:
			soundlatch = data;
			M6502Close();
			M6502Open(1);
			M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
			M6502Close();
			M6502Open(0);
		return;
	}
}

static void btime_main_write(UINT16 address, UINT8 data)
{
	WB(0x0000, 0x07ff, DrvMainRAM);
	WB(0x1000, 0x13ff, DrvVidRAM);
	WB(0x1400, 0x17ff, DrvColRAM);

	if (address >= 0xc00 && address <= 0xc1f) {
		DrvPalRAM[address - 0xc00] = data;
		if (address <= 0xc0f) btimepalettewrite(address - 0xc00, ~data);
		return;
	}

	if (address >= 0x1800 && address <= 0x1bff) {
		INT32 offset = address & 0x3ff;
		DrvVidRAM[(((offset % 32) * 32) + (offset / 32))] = data;
		return;
	}

	if (address >= 0x1c00 && address <= 0x1fff) {
		INT32 offset = address & 0x3ff;
		DrvColRAM[(((offset % 32) * 32) + (offset / 32))] = data;
		return;
	}

	switch (address)
	{
		case 0x4000:
		return; // nop

		case 0x4002:
			// video control (flipscreen)
		return;

		case 0x4003:
			soundlatch = data;
			M6502Close();
			M6502Open(1);
			M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
			M6502Close();
			M6502Open(0);
		return;

		case 0x4004:
			bnj_scroll1 = data;
		return;
	}
}

static void zoar_main_write(UINT16 address, UINT8 data)
{
	WB(0x0000, 0x07ff, DrvMainRAM);
	WB(0x8000, 0x83ff, DrvVidRAM);
	WB(0x8400, 0x87ff, DrvColRAM);
	WB(0x9800, 0x9803, DrvScrollRAM);

	if (address >= 0x8800 && address <= 0x8bff) {
		INT32 offset = address & 0x3ff;
		DrvVidRAM[(((offset % 32) * 32) + (offset / 32))] = data;
		return;
	}

	if (address >= 0x8c00 && address <= 0x8fff) {
		INT32 offset = address & 0x3ff;
		DrvColRAM[(((offset % 32) * 32) + (offset / 32))] = data;
		return;
	}

	switch (address)
	{
		case 0x9804:
			bnj_scroll2 = data;
		return;

		case 0x9805:
			bnj_scroll1 = data;
		return;

		case 0x9000:
			btime_palette = (data & 0x30) >> 3;
		return;

		case 0x9806:
			soundlatch = data;
			M6502Close();
			M6502Open(1);
			M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
			M6502Close();
			M6502Open(0);
		return;
	}
}

static void disco_main_write(UINT16 address, UINT8 data)
{
	WB(0x0000, 0x07ff, DrvMainRAM);
	WB(0x2000, 0x7fff, DrvCharRAM);
	WB(0x8000, 0x83ff, DrvVidRAM);
	WB(0x8400, 0x87ff, DrvColRAM);
	WB(0x8800, 0x881f, DrvSpriteRAM);

	switch (address)
	{
		case 0x9c00:
			btime_palette = (data >> 2) & 0x03;
		return;

		case 0x9a00:
			soundlatch = data;
			M6502Close();
			M6502Open(1);
			M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
			M6502Close();
			M6502Open(0);
		return;
	}
}

static void bnj_main_write(UINT16 address, UINT8 data)
{

	WB(0x0000, 0x07ff, DrvMainRAM);
	WB(0x4000, 0x43ff, DrvVidRAM);
	WB(0x4400, 0x47ff, DrvColRAM);
	WB(0x5000, 0x51ff, DrvBGRAM);

	if (address >= 0x5c00 && address <= 0x5c1f) {
		DrvPalRAM[address - 0x5c00] = data;
		if (address <= 0x5c0f) btimepalettewrite(address - 0x5c00, ~data);
		return;
	}

	if (address >= 0x4800 && address <= 0x4bff) {
		INT32 offset = address & 0x3ff;
		DrvVidRAM[(((offset % 32) * 32) + (offset / 32))] = data;
		return;
	}

	if (address >= 0x4c00 && address <= 0x4fff) {
		INT32 offset = address & 0x3ff;
		DrvColRAM[(((offset % 32) * 32) + (offset / 32))] = data;
		return;
	}

	switch (address)
	{
		case 0x5400:
			bnj_scroll1 = data;
		return;

		case 0x5800:
			bnj_scroll2 = data;
		return;

		case 0x1002:
			soundlatch = data;
			M6502Close();
			M6502Open(1);
			M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
			M6502Close();
			M6502Open(0);
		return;
	}
}

static UINT8 btime_sound_read(UINT16 address)
{
	if (address <= 0x1fff) {
		return DrvSoundRAM[address & 0x3ff];
	}

	RB(0xe000, 0xefff, DrvSoundROM);
	RB(0xf000, 0xffff, DrvSoundROM);

	switch (address >> 13)
	{
		case 0x05:
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
			return soundlatch;
	}

	return 0;
}

/*
 dink's notes on the horrible migraine-inducing high-pitched hissing noise
 for a note-off, burgertime's audiocpu sends a command with fine/coarse tuning
 set to 0 to each channel.  Right after this, it sends a volume command for
 that channel.  The ay8910 soundcore (old and recent) treats a tuning command
 of 0 as a very high pitched noise.  theres also a funny bug in the 8910
 emulator: if you remove the check for a 0 fine/coarse tuning - to actually
 allow for a tune of 0 - it will actually lock up while rendering the samples.
 see: if (PSG->PeriodA == 0) PSG->PeriodA = PSG->UpdateStep;

 what I do to get around making any changes to the ay8910 core is look for the
 fine/coarse tuning value of 0 being sent to each of the first ay's 3 channels
 and set the volume down to 0. The next command is always a volume, so we just
 change the next data command to 0.

0x02: 0. 0x01: 0. AF: 0 AC: 0.
0x02: 1. 0x01: 0. AF: 0 AC: 0.

0x02: 2. 0x01: 0. BF: 0 BC: 0.
0x02: 3. 0x01: 0. BF: 0 BC: 0.

0x02: 4. 0x01: 0. CF: 0 CC: 0.
0x02: 5. 0x01: 0. CF: 0 CC: 0.
*/

static UINT8 last01[3] = {0xff,0xff};
static UINT8 last02[3] = {0xff,0xff};
static UINT8 ignext = 0;

static void checkhiss_add02(UINT8 data)
{
	last02[1] = last02[0];
	last02[0] = data;
}

static void checkhiss_and_add01(UINT8 data)
{
	last01[1] = last01[0];
	last01[0] = data;

	if (last01[0] == 0 && last02[0] == 1 && last01[1] == 0 && last02[1] == 0)
	{
		ignext = 1; // next command will be VolA
	}
	if (last01[0] == 0 && last02[0] == 3 && last01[1] == 0 && last02[1] == 2)
	{
		ignext = 1; // next command will be VolB
	}
	if (last01[0] == 0 && last02[0] == 5 && last01[1] == 0 && last02[1] == 4)
	{
		ignext = 1; // next command will be VolC
	}
}

static void btime_sound_write(UINT16 address, UINT8 data)
{
	if (address <= 0x1fff) {
		DrvSoundRAM[address & 0x3ff] = data;
	}
	switch (address >> 13)
	{
		case 0x01:
			//bprintf(0, _T("0x01: %X. "), data);
			if (ignext) {
				data = 0; // set volume to 0
				ignext = 0;
			}
			AY8910Write(0, ~((address>>13)-1) & 1, data);
			checkhiss_and_add01(data);
			return;
		case 0x02:
			//bprintf(0, _T("0x02: %X. "), data);
			AY8910Write(0, ~((address>>13)-1) & 1, data);
			checkhiss_add02(data);
			return;
		case 0x03:
			AY8910Write(1, ~((address>>13)-1) & 1, data);
			return;
		case 0x04:
			AY8910Write(1, ~((address>>13)-1) & 1, data);
			return;

		case 0x06: {
			if (audio_nmi_type == AUDIO_ENABLE_DIRECT) {
				audio_nmi_enable = data & 1;
				M6502SetIRQLine(0x20, (audio_nmi_enable && audio_nmi_state) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
			}
		}
		return;
	}
}

static UINT8 disco_sound_read(UINT16 address)
{
	RB(0x0000, 0x03ff, DrvSoundRAM);

	RB(0xf000, 0xffff, DrvSoundROM);

	switch (address & 0xf000)
	{
		case 0x8000:
			return soundlatch;
	}

	return 0;
}

static void disco_sound_write(UINT16 address, UINT8 data)
{
	WB(0x0000, 0x03ff, DrvSoundRAM);

	switch (address & 0xf000)
	{
		case 0x4000:
			AY8910Write(0, 1, data);
			return;
		case 0x5000:
			AY8910Write(0, 0, data);
			return;
		case 0x6000:
			AY8910Write(1, 1, data);
			return;
		case 0x7000:
			AY8910Write(1, 0, data);
			return;
	}
}

static void ay8910_0_portA_write(UINT32, UINT32 data)
{
	if (M6502GetActive() == -1) return; // prevent crash.

	if (audio_nmi_type == AUDIO_ENABLE_AY8910)
	{
		audio_nmi_enable = ~data & 1;
		M6502SetIRQLine(0x20, (audio_nmi_enable && audio_nmi_state) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
	}

}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvMainROM	= Next; Next += 0x010000;
	DrvMainROMdec	= Next; Next += 0x010000; // for mmonkey's readop
	DrvSoundROM	= Next; Next += 0x010000;

	DrvGfxROM0	= Next; Next += 0x020000;
	DrvGfxROM1	= Next; Next += 0x020000;
	DrvGfxROM2	= Next; Next += 0x020000;
	DrvGfxROM3	= Next; Next += 0x020000;
	DrvBgMapROM = Next; Next += 0x020000;
	DrvBGBitmap     = (UINT16*)Next; Next += (512 * 512) * sizeof(UINT16);
	DrvColPROM  = Next; Next += 0x000200;

	DrvPalette	= (UINT32*)Next; Next += 0x0200 * sizeof(INT32);

	pAY8910Buffer[0] = (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[1] = (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[2] = (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[3] = (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[4] = (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[5] = (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);

	AllRam		= Next;

	DrvMainRAM	= Next; Next += 0x010000;
	DrvPalRAM	= Next; Next += 0x001000;
	DrvVidRAM	= Next; Next += 0x001000;
	DrvBGRAM    = Next; Next += 0x001000;
	DrvColRAM	= Next; Next += 0x001000;
	DrvCharRAM  = Next; Next += 0x008000;
	DrvSpriteRAM= Next; Next += 0x001000;
	DrvScrollRAM= Next; Next += 0x000100;
	DrvSoundRAM	= Next; Next += 0x001000;

	RamEnd		= Next;
	MemEnd		= Next;

	return 0;
}


static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	M6502Open(0);
	M6502Reset();
	M6502Close();

	M6502Open(1);
	M6502Reset();
	AY8910Reset(0);
	AY8910Reset(1);
	M6502Close();

	HiscoreReset();

	flipscreen = 0;
	soundlatch = 0;
	bnj_scroll1 = 0;
	bnj_scroll2 = 0;
	btime_palette = 0;

	audio_nmi_enable = 0;
	audio_nmi_state = 0;

	protection_command = 0;
	protection_status = 0;
	protection_value = 0;
	protection_ret = 0;

	zippysoundinit = ZIPPYSOUNDFRAMES;

	return 0;
}

static INT32 DrvBnjGfxDecode()
{
	INT32 Plane_t8[3] = { RGN_FRAC(gfx0len, 2,3), RGN_FRAC(gfx0len, 1,3), RGN_FRAC(gfx0len, 0,3) };
	INT32 Plane_t16spr[3] = { RGN_FRAC(gfx0len, 2,3), RGN_FRAC(gfx0len, 1,3), RGN_FRAC(gfx0len, 0,3) };
	INT32 Plane_t16bnj[3] = { RGN_FRAC(gfx1len, 1,2)+4, RGN_FRAC(gfx1len, 0,2)+0, RGN_FRAC(gfx1len, 0,2)+4 };

	INT32 t8XOffs[8] = { STEP8(0,1) };
	INT32 t8YOffs[8] = { STEP8(0,8) };

	INT32 t16bnjXOffs[16] = { STEP4(3*16*8,1), STEP4(2*16*8,1), STEP4(1*16*8,1), STEP4(0*16*8,1) };
	INT32 t16bnjYOffs[16] = { STEP16(0,8) };

	INT32 t16XOffs[16] = { STEP8(16*8,1), STEP8(0,1) };
	INT32 t16YOffs[16] = { STEP16(0,8) };

	UINT8 *tmp = (UINT8*)malloc(gfx0len + gfx1len); // more = more happy-fun. yay!
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, gfx0len);

	GfxDecode(0x0400, 3, 8, 8, Plane_t8, t8XOffs, t8YOffs, 0x40, tmp, DrvGfxROM0);
	GfxDecode(0x00ff, 3, 16, 16, Plane_t16spr, t16XOffs, t16YOffs, 0x100, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, gfx1len);

	GfxDecode(0x0040, 3, 16, 16, Plane_t16bnj, t16bnjXOffs, t16bnjYOffs, 0x200, tmp, DrvGfxROM2);

	free (tmp);

	return 0;
}

static INT32 DrvDiscoGfxDecode() // Decoded from Disco's RAM
{
	INT32 Plane_t8[3] = { 0x4000*8, 0x2000*8, 0x0000*8 };
	INT32 Plane_t16[3] = { 0x4000*8, 0x2000*8, 0x0000*8 };

	INT32 t8XOffs[8] = { STEP8(0,1) };
	INT32 t8YOffs[8] = { STEP8(0,8) };

	INT32 t16XOffs[16] = { STEP8(16*8,1), STEP8(0,1) };
	INT32 t16YOffs[16] = { STEP16(0,8) };

	GfxDecode(0x0400, 3, 8, 8, Plane_t8, t8XOffs, t8YOffs, 0x40, DrvCharRAM, DrvGfxROM0);
	GfxDecode(0x00ff, 3, 16, 16, Plane_t16, t16XOffs, t16YOffs, 0x100, DrvCharRAM, DrvGfxROM1);

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane_t8[3] = { RGN_FRAC(gfx0len, 2,3), RGN_FRAC(gfx0len, 1,3), RGN_FRAC(gfx0len, 0,3) };
	INT32 Plane_t16spr[3] = { RGN_FRAC(gfx0len, 2,3), RGN_FRAC(gfx0len, 1,3), RGN_FRAC(gfx0len, 0,3) };
	INT32 Plane_t16[3] = { RGN_FRAC(gfx1len, 2,3), RGN_FRAC(gfx1len, 1,3), RGN_FRAC(gfx1len, 0,3) };

	INT32 t8XOffs[8] = { STEP8(0,1) };
	INT32 t8YOffs[8] = { STEP8(0,8) };

	INT32 t16XOffs[16] = { STEP8(16*8,1), STEP8(0,1) };
	INT32 t16YOffs[16] = { STEP16(0,8) };

	UINT8 *tmp = (UINT8*)malloc(gfx0len + gfx1len); // more = more happy-fun. yay!
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, gfx0len);

	GfxDecode(0x0400, 3, 8, 8, Plane_t8, t8XOffs, t8YOffs, 0x40, tmp, DrvGfxROM0);
	GfxDecode(0x00ff, 3, 16, 16, Plane_t16spr, t16XOffs, t16YOffs, 0x100, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, gfx1len);

	GfxDecode(0x0040, 3, 16, 16, Plane_t16, t16XOffs, t16YOffs, 0x100, tmp, DrvGfxROM2);

	free (tmp);

	return 0;
}

static INT32 ZoarGfxDecode()
{
	INT32 Plane_t8[3] = { RGN_FRAC(gfx0len, 2,3), RGN_FRAC(gfx0len, 1,3), RGN_FRAC(gfx0len, 0,3) };
	INT32 Plane_t16spr[3] = { RGN_FRAC(0x3000, 2,3), RGN_FRAC(0x3000, 1,3), RGN_FRAC(0x3000, 0,3) };
	INT32 Plane_t16[3] = { RGN_FRAC(gfx1len, 2,3), RGN_FRAC(gfx1len, 1,3), RGN_FRAC(gfx1len, 0,3) };

	INT32 t8XOffs[8] = { STEP8(0,1) };
	INT32 t8YOffs[8] = { STEP8(0,8) };

	INT32 t16XOffs[16] = { STEP8(16*8,1), STEP8(0,1) };
	INT32 t16YOffs[16] = { STEP16(0,8) };

	UINT8 *tmp = (UINT8*)malloc(gfx0len + gfx1len + 0x3000); // more = more happy-fun. yay!
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, gfx0len);

	GfxDecode(0x0400, 3, 8, 8, Plane_t8, t8XOffs, t8YOffs, 0x40, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x3000);

	GfxDecode(0x0080, 3, 16, 16, Plane_t16spr, t16XOffs, t16YOffs, 0x100, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, gfx1len);

	GfxDecode(0x0040, 3, 16, 16, Plane_t16, t16XOffs, t16YOffs, 0x100, tmp, DrvGfxROM2);

	free (tmp);

	return 0;
}

static INT32 MmonkeyInit() // and lnc
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvMainROM + 0x0c000,  0, 1)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x0d000,  1, 1)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x0e000,  2, 1)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x0f000,  3, 1)) return 1;

		if (BurnLoadRom(DrvSoundROM + 0x0000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x001000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x002000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x003000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x004000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x005000,  10, 1)) return 1;
		gfx0len = 0x6000;

		if (BurnLoadRom(DrvColPROM + 0x000000,  11, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x000020,  12, 1)) return 1;
		gfx1len = 0x00;

		DrvGfxDecode();

	}

	memcpy(DrvMainROMdec, DrvMainROM, 0x10000);

	M6502Init(0, TYPE_DECOC10707);
	M6502Open(0);
	M6502SetWriteHandler(mmonkey_main_write);
	M6502SetReadHandler(mmonkey_main_read);
	M6502SetReadOpArgHandler(mmonkey_main_read);
	M6502SetReadOpHandler(mmonkeyop_main_read);
	M6502Close();

	M6502Init(1, TYPE_M6502);
	M6502Open(1);
	M6502SetWriteHandler(btime_sound_write);
	M6502SetReadHandler(btime_sound_read);
	M6502SetReadOpArgHandler(btime_sound_read);
	M6502SetReadOpHandler(btime_sound_read);
	M6502Close();

	M6502Open(1);
	AY8910Init(0, 1500000, nBurnSoundRate, NULL, NULL, &ay8910_0_portA_write, NULL);
	AY8910Init(1, 1500000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.20, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.20, BURN_SND_ROUTE_BOTH);
	M6502Close();

	audio_nmi_type = AUDIO_ENABLE_AY8910;

	GenericTilesInit();

	// first ay, pass-thru mixer only (CAP_P(0) = passthru)
	filter_rc_init(0, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_P(0), 0);
	filter_rc_init(1, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_P(0), 1);
	filter_rc_init(2, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_P(0), 1);

	// second ay, apply a slight bit of lpf to match
	filter_rc_init(3, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_N(10*0x15), 1);
	filter_rc_init(4, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_N(10*0x10), 1);
	filter_rc_init(5, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_N(10*0x10), 1);

	filter_rc_set_route(0, 0.20, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(1, 0.20, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(2, 0.20, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(3, (lncmode) ? 0.20 : 0.10, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(4, (lncmode) ? 0.20 : 0.10, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(5, (lncmode) ? 0.20 : 0.10, BURN_SND_ROUTE_BOTH);

	mmonkeymode = 1;

	DrvDoReset();

	return 0;
}

static INT32 DiscoInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvMainROM + 0x0a000,  0, 1)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x0b000,  1, 1)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x0c000,  2, 1)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x0d000,  3, 1)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x0e000,  4, 1)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x0f000,  5, 1)) return 1;

		if (BurnLoadRom(DrvSoundROM + 0x0000,  6, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x000000,  7, 1)) return 1;

		// Graphics are decoded from RAM.
		gfx0len = 0x6000; // DrvCharRAM mapped-size
		gfx1len = 0x00;
	}

	M6502Init(0, TYPE_DECOCPU7);
	M6502Open(0);
	M6502SetWriteHandler(disco_main_write);
	M6502SetReadHandler(disco_main_read);
	M6502SetReadOpArgHandler(disco_main_read);
	M6502SetReadOpHandler(disco_main_read);
	M6502Close();

	M6502Init(1, TYPE_M6502);
	M6502Open(1);
	M6502SetWriteHandler(disco_sound_write);
	M6502SetReadHandler(disco_sound_read);
	M6502SetReadOpArgHandler(disco_sound_read);
	M6502SetReadOpHandler(disco_sound_read);
	M6502Close();

	M6502Open(1);
	AY8910Init(0, 1500000, nBurnSoundRate, NULL, NULL, &ay8910_0_portA_write, NULL);
	AY8910Init(1, 1500000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.20, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.20, BURN_SND_ROUTE_BOTH);
	M6502Close();

	audio_nmi_type = AUDIO_ENABLE_AY8910;

	discomode = 1;

	GenericTilesInit();

	// first ay, pass-thru mixer only (CAP_P(0) = passthru)
	filter_rc_init(0, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_P(0), 0);
	filter_rc_init(1, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_P(0), 1);
	filter_rc_init(2, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_P(0), 1);

	// second ay, apply a slight bit of lpf to match
	filter_rc_init(3, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_N(10*0x15), 1);
	filter_rc_init(4, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_N(10*0x10), 1);
	filter_rc_init(5, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_N(10*0x10), 1);

	filter_rc_set_route(0, 0.20, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(1, 0.20, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(2, 0.20, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(3, 0.20, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(4, 0.20, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(5, 0.20, BURN_SND_ROUTE_BOTH);

	DrvDoReset();

	return 0;
}

static INT32 BnjInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (brubbermode)
	{
		if (BurnLoadRom(DrvMainROM + 0xc000,  0, 1)) return 1;
		if (BurnLoadRom(DrvMainROM + 0xe000,  1, 1)) return 1;

		if (BurnLoadRom(DrvSoundROM + 0x0000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  3, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x002000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x004000,  5, 1)) return 1;
		gfx0len = 0x6000;

		if (BurnLoadRom(DrvGfxROM2 + 0x000000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x001000,  7, 1)) return 1;
		gfx1len = 0x2000;

		DrvBnjGfxDecode();
	}
	else
	{ // bnj
		if (BurnLoadRom(DrvMainROM + 0xa000,  0, 1)) return 1;
		if (BurnLoadRom(DrvMainROM + 0xc000,  1, 1)) return 1;
		if (BurnLoadRom(DrvMainROM + 0xe000,  2, 1)) return 1;

		if (BurnLoadRom(DrvSoundROM + 0x0000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x002000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x004000,  6, 1)) return 1;
		gfx0len = 0x6000;

		if (BurnLoadRom(DrvGfxROM2 + 0x000000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x001000,  8, 1)) return 1;
		gfx1len = 0x2000;

		DrvBnjGfxDecode();
	}

	M6502Init(0, TYPE_DECOC10707);
	M6502Open(0);
	M6502SetWriteHandler(bnj_main_write);
	M6502SetReadHandler(bnj_main_read);
	M6502SetReadOpArgHandler(bnj_main_read);
	M6502SetReadOpHandler(bnj_main_read);
    M6502Close();

	M6502Init(1, TYPE_M6502);
	M6502Open(1);
	M6502SetWriteHandler(btime_sound_write);
	M6502SetReadHandler(btime_sound_read);
	M6502SetReadOpArgHandler(btime_sound_read);
	M6502SetReadOpHandler(btime_sound_read);
	M6502Close();

	M6502Open(1);
	AY8910Init(0, 1500000, nBurnSoundRate, NULL, NULL, &ay8910_0_portA_write, NULL);
	AY8910Init(1, 1500000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.20, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.20, BURN_SND_ROUTE_BOTH);
	M6502Close();

	audio_nmi_type = AUDIO_ENABLE_DIRECT;

	bnjskew = 1;

	GenericTilesInit();

	// first ay, pass-thru mixer only (CAP_P(0) = passthru)
	filter_rc_init(0, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_P(0), 0);
	filter_rc_init(1, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_P(0), 1);
	filter_rc_init(2, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_P(0), 1);

	// second ay, apply a slight bit of lpf to match
	filter_rc_init(3, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_N(10*0x15), 1);
	filter_rc_init(4, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_N(10*0x10), 1);
	filter_rc_init(5, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_N(10*0x10), 1);

	filter_rc_set_route(0, 0.20, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(1, 0.20, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(2, 0.20, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(3, 0.25, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(4, 0.25, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(5, 0.25, BURN_SND_ROUTE_BOTH);

	DrvDoReset();

	return 0;
}

static INT32 BtimeInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		INT32 base = (btime3mode) ? 0xb000 : 0xc000;

		if (BurnLoadRom(DrvMainROM + base + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvMainROM + base + 0x01000,  1, 1)) return 1;
		if (BurnLoadRom(DrvMainROM + base + 0x02000,  2, 1)) return 1;
		if (BurnLoadRom(DrvMainROM + base + 0x03000,  3, 1)) return 1;

		if (btime3mode) {
			if (BurnLoadRom(DrvMainROM + base + 0x04000,  4, 1)) return 1; // btime3mode
		}

		if (BurnLoadRom(DrvSoundROM + 0x0000,  4+btime3mode, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  5+btime3mode, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x001000,  6+btime3mode, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x002000,  7+btime3mode, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x003000,  8+btime3mode, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x004000,  9+btime3mode, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x005000,  10+btime3mode, 1)) return 1;
		gfx0len = 0x6000;

		if (BurnLoadRom(DrvGfxROM2 + 0x000000,  11+btime3mode, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x000800,  12+btime3mode, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x001000,  13+btime3mode, 1)) return 1;
		gfx1len = 0x1800;

		if (BurnLoadRom(DrvBgMapROM + 0x000000,  14+btime3mode, 1)) return 1;

		DrvGfxDecode();

	}

	btimemode = 1;

	M6502Init(0, TYPE_DECOCPU7);
	M6502Open(0);
	M6502SetWriteHandler(btime_main_write);
	M6502SetReadHandler(btime_main_read);
	M6502SetReadOpArgHandler(btime_main_read);
	M6502SetReadOpHandler(btime_main_read);
	M6502Close();

	M6502Init(1, TYPE_M6502);
	M6502Open(1);
	M6502SetWriteHandler(btime_sound_write);
	M6502SetReadHandler(btime_sound_read);
	M6502SetReadOpArgHandler(btime_sound_read);
	M6502SetReadOpHandler(btime_sound_read);
	M6502Close();

	AY8910Init(0, 1500000, nBurnSoundRate, NULL, NULL, &ay8910_0_portA_write, NULL);
	AY8910Init(1, 1500000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.20, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.20, BURN_SND_ROUTE_BOTH);

	audio_nmi_type = AUDIO_ENABLE_DIRECT;

	GenericTilesInit();

	// first ay, lots of lowpass hiss-killer.
	filter_rc_init(0, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_N(0x330), 0);
	filter_rc_init(1, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_N(0x330), 1);
	filter_rc_init(2, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_N(0x330), 1);
	filter_rc_set_src_gain(0, 0.55);
	filter_rc_set_src_gain(1, 0.55);
	filter_rc_set_src_gain(2, 0.55);

	// second ay, apply a slight bit of lpf to match
	filter_rc_init(3, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_N(10*0x25), 1);
	filter_rc_init(4, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_N(10*0x10), 1);
	filter_rc_init(5, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_N(10*0x10), 1);

	// #6 takes mixed #0,1,2 and highpasses it a little to get rid of the dc offset
	filter_rc_init(6, FLT_RC_HIGHPASS, 3846, 0, 0, CAP_N(0x310), 0);
	filter_rc_set_src_stereo(6);

	hpfiltbuffer = (INT16*)BurnMalloc(nBurnSoundLen*8); // for #6

	filter_rc_set_route(0, 1.00, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(1, 1.00, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(2, 1.00, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(3, 0.35, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(4, 0.15, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(5, 0.15, BURN_SND_ROUTE_BOTH);

	DrvDoReset();

	return 0;
}

static INT32 ZoarInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvMainROM + 0x0d000,  0, 1)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x0e000,  1, 1)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x0f000,  2, 1)) return 1;

		memset(&DrvMainROM[0xd50a], 0xea, 8); // patch bad rom with nops (othewise freezes @ boot)

		if (BurnLoadRom(DrvSoundROM + 0x0000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x001000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x002000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x003000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x004000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x005000,  9, 1)) return 1;
		gfx0len = 0x6000;

		if (BurnLoadRom(DrvGfxROM2 + 0x000000,  10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x000800,  11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x001000,  12, 1)) return 1;
		gfx1len = 0x1800;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  13, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x001000,  14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x002000,  15, 1)) return 1;

		if (BurnLoadRom(DrvBgMapROM + 0x00000,  16, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x000000,  17, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x000020,  18, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x000040,  19, 1)) return 1;

		ZoarGfxDecode();

	}

	M6502Init(0, TYPE_DECOCPU7);
	M6502Open(0);
	M6502SetWriteHandler(zoar_main_write);
	M6502SetReadHandler(zoar_main_read);
	M6502SetReadOpArgHandler(zoar_main_read);
	M6502SetReadOpHandler(zoar_main_read);
	M6502Close();

	M6502Init(1, TYPE_M6502);
	M6502Open(1);
	M6502SetWriteHandler(btime_sound_write);
	M6502SetReadHandler(btime_sound_read);
	M6502SetReadOpArgHandler(btime_sound_read);
	M6502SetReadOpHandler(btime_sound_read);
	M6502Close();

	AY8910Init(0, 3000000, nBurnSoundRate, NULL, NULL, &ay8910_0_portA_write, NULL);
	AY8910Init(1, 3000000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.20, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.20, BURN_SND_ROUTE_BOTH);

	audio_nmi_type = AUDIO_ENABLE_AY8910;
	zoarmode = 1;

	GenericTilesInit();

	// first ay, pass-thru mixer only (CAP_P(0) = passthru)
	filter_rc_init(0, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_P(0), 0);
	filter_rc_init(1, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_P(0), 1);
	filter_rc_init(2, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_P(0), 1);

	// second ay, apply a slight bit of lpf to match
	filter_rc_init(3, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_N(10*0x15), 1);
	filter_rc_init(4, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_N(10*0x10), 1);
	filter_rc_init(5, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_N(10*0x10), 1);

	filter_rc_set_route(0, 0.25, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(1, 0.25, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(2, 0.25, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(3, 0.25, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(4, 0.25, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(5, 0.25, BURN_SND_ROUTE_BOTH);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6502Exit();

	AY8910Exit(0);
	AY8910Exit(1);

	filter_rc_exit();

	BurnFree(AllMem);

	if (btimemode) {
		BurnFree(hpfiltbuffer);
		hpfiltbuffer = NULL;
	}

	btimemode = 0;
	btime3mode = 0;
	brubbermode = 0;
	discomode = 0;
	lncmode = 0;
	zoarmode = 0;
	mmonkeymode = 0;
	bnjskew = 0;
	zippysoundinit = 0;

	audio_nmi_type = AUDIO_ENABLE_NONE;

	return 0;
}

static void BtimePaletteRecalc()
{
	for (INT32 i = 0; i < 0x10; i++) {
		btimepalettewrite(i, ~DrvPalRAM[i]);
	}
}

static void draw_background(UINT8 *tmap, INT32 color)
{
	const UINT8 *gfx = DrvBgMapROM;
	int scroll = -(bnj_scroll2 | ((bnj_scroll1 & 0x03) << 8));

	for (INT32 i = 0; i < 5; i++, scroll += 256)
	{
		INT32 tileoffset = tmap[i & 3] * 0x100;

		if (scroll > 256) break;
		if (scroll < -256) continue;

		for (INT32 offs = 0; offs < 0x100; offs++)
		{
			int x = 240 - (16 * (offs / 16) + scroll) - 1;
			int y = 16 * (offs % 16);

			if (flipscreen)
			{
				x = 240 - x;
				y = 240 - y;
			}

			y -= 8;
			x -= 8;

			if (x < -15 || x >= 256 || y < -15 || y >= 256) continue;

			INT32 code = gfx[tileoffset + offs] & 0x7f;

			Render16x16Tile_Clip(pTransDraw, code, x, y, color, 3, (zoarmode) ? 0 : 8, DrvGfxROM2);
		}
	}
}

static void draw_chars(UINT8 transparency, UINT8 color, int priority)
{
	for (INT32 offs = 0; offs < 0x400; offs++)
	{
		INT32 x = 31 - (offs / 32);
		INT32 y = offs % 32;

		UINT16 code = DrvVidRAM[offs] + 256 * (DrvColRAM[offs] & 3);

		if ((priority != -1) && (priority != ((code >> 7) & 0x01)))
			continue;

		if (flipscreen)
		{
			x = 31 - x;
			y = 31 - y;
		}

		y -= 1;
		x -= (bnjskew) ? 0 : 1;

		if (transparency) {
			Render8x8Tile_Mask_Clip(pTransDraw, code, x * 8, y * 8, color, 3, 0, 0, DrvGfxROM0);
		} else {
			Render8x8Tile_Clip(pTransDraw, code, x * 8, y * 8, color, 3, 0, DrvGfxROM0);
		}
	}
}

static void draw_sprites(UINT8 color, UINT8 sprite_y_adjust, UINT8 sprite_y_adjust_flip_screen, UINT8 *sprite_ram, INT32 interleave)
{
	for (INT32 i = 0, offs = 0; i < 8; i++, offs += 4 * interleave)
	{
		INT32 x, y;
		UINT8 flipx, flipy;

		if (!(sprite_ram[offs + 0] & 0x01)) continue;

		x = 240 - sprite_ram[offs + 3 * interleave];
		y = 240 - sprite_ram[offs + 2 * interleave];

		flipx = sprite_ram[offs + 0] & 0x04;
		flipy = sprite_ram[offs + 0] & 0x02;

		if (flipscreen)
		{
			x = 240 - x;
			y = 240 - y + sprite_y_adjust_flip_screen;

			flipx = !flipx;
			flipy = !flipy;
		}


		INT32 code = sprite_ram[offs + interleave];

		y -= 8;
		x -= (bnjskew) ? 0 : 8;

		y = y - sprite_y_adjust;

		if (flipy) {
			if (flipx) {
				Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, x, y, color, 3, 0, 0, DrvGfxROM1);
			} else {
				Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, x, y, color, 3, 0, 0, DrvGfxROM1);
			}
		} else {
			if (flipx) {
				Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, x, y, color, 3, 0, 0, DrvGfxROM1);
			} else {
				Render16x16Tile_Mask_Clip(pTransDraw, code, x, y, color, 3, 0, 0, DrvGfxROM1);
			}
		}

		y = y + (flipscreen ? -256 : 256);

		if (flipy) {
			if (flipx) {
				Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, x, y, color, 3, 0, 0, DrvGfxROM1);
			} else {
				Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, x, y, color, 3, 0, 0, DrvGfxROM1);
			}
		} else {
			if (flipx) {
				Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, x, y, color, 3, 0, 0, DrvGfxROM1);
			} else {
				Render16x16Tile_Mask_Clip(pTransDraw, code, x, y, color, 3, 0, 0, DrvGfxROM1);
			}
		}
	}
}

static INT32 BtimeDraw()
{
	if (DrvRecalc) {
		BtimePaletteRecalc();
		DrvRecalc = 0;
	}

	BurnTransferClear();

	if (bnj_scroll1 & 0x10) {
		INT32 start = 0;
		UINT8 tmap[5];

		if (flipscreen)
			start = 0;
		else
			start = 1;

		for (INT32 i = 0; i < 4; i++)
		{
			tmap[i] = start | (bnj_scroll1 & 0x04);
			start = (start + 1) & 0x03;
		}

		if (nBurnLayer & 1) draw_background(&tmap[0], 0);
		if (nBurnLayer & 2) draw_chars(1, 0, -1);
	} else {
		if (nBurnLayer & 2) draw_chars(0, 0, -1);
	}

	if (nBurnLayer & 4) draw_sprites(0, 1, 0, DrvVidRAM, 0x20);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 ZoarDraw()
{
	if (DrvRecalc) {
		discopaletteupdate();
		DrvRecalc = 0;
	}

	BurnTransferClear();

	if (bnj_scroll1 & 0x04) {
		if (nBurnLayer & 1) draw_background(DrvScrollRAM, btime_palette);
		if (nBurnLayer & 2) draw_chars(1, btime_palette + 1, -1);
	} else {
		if (nBurnLayer & 2) draw_chars(0, btime_palette + 1, -1);
	}

	if (nBurnLayer & 4) draw_sprites(btime_palette + 1, 1, 2, DrvVidRAM + 0x1f, 0x20);
	if (nBurnLayer & 8) draw_sprites(btime_palette + 1, 1, 2, DrvVidRAM, 0x20);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 BnjDraw()
{
	if (DrvRecalc) {
		BtimePaletteRecalc();
		DrvRecalc = 0;
	}

	BurnTransferClear();

	if (bnj_scroll1) {
		GenericTilesSetClipRaw(0,512,0,256);

		for (INT32 offs = 0x200 - 1; offs >= 0; offs--)
		{
			INT32 sx = 16 * ((offs < 0x100) ? ((offs % 0x80) / 8) : ((offs % 0x80) / 8) + 16);
			INT32 sy = 16 * (((offs % 0x100) < 0x80) ? offs % 8 : (offs % 8) + 8);
			sx = 496 - sx;

			if (flipscreen)
			{
				sx = 496 - sx;
				sy = 240 - sy;
			}
			INT32 code = (DrvBGRAM[offs] >> 4) + ((offs & 0x80) >> 3) + 32;

			sy -= 8;

			Render16x16Tile_Clip(DrvBGBitmap, code, sx, sy, 0, 3, 8, DrvGfxROM2);
		}

		GenericTilesClearClipRaw();

		/* copy the background bitmap to the screen */
		INT32 scroll = (bnj_scroll1 & 0x02) * 128 + 511 - bnj_scroll2;
		if (!flipscreen)
			scroll = 767 - scroll;

		//copyscrollbitmap(bitmap, *m_background_bitmap, 1, &scroll, 0, 0, cliprect);
		// copy the BGBitmap to pTransDraw
		for (INT32 sy = 0; sy < nScreenHeight; sy++) {

			UINT16 *src = DrvBGBitmap + sy * 512;
			UINT16 *dst = pTransDraw + sy * nScreenWidth;

			for (INT32 sx = 0; sx < nScreenWidth; sx++) {
				dst[sx] = src[(sx-scroll)&0x1ff];
			}
		}

		/* copy the low priority characters followed by the sprites
		 then the high priority characters */

		if (nBurnLayer & 2) draw_chars(1, 0, 1);
		if (nBurnLayer & 4) draw_sprites(0, 1, 0, DrvVidRAM, 0x20);
		if (nBurnLayer & 8) draw_chars(1, 0, 0);
	}
	else
	{
		if (nBurnLayer & 2) draw_chars(0, 0, -1);
		if (nBurnLayer & 4) draw_sprites(0, 0, 0, DrvVidRAM, 0x20);
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 eggsDraw()
{
	if (DrvRecalc) {
		lncpaletteupdate();
		DrvRecalc = 0;
	}

	BurnTransferClear();

	if (nBurnLayer & 2) draw_chars(0, 0, -1);
	if (nBurnLayer & 4) draw_sprites(0, 0, 0, DrvVidRAM, 0x20); // for eggs

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 LncDraw()
{
	if (DrvRecalc) {
		lncpaletteupdate();
		DrvRecalc = 0;
	}

	BurnTransferClear();

	if (nBurnLayer & 2) draw_chars(0, 0, -1);
	if (nBurnLayer & 4) draw_sprites(0, 1, 2, DrvVidRAM, 0x20); // for eggs

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DiscoDraw()
{
	if (DrvRecalc) {
		discopaletteupdate();
		DrvRecalc = 0;
	}

	DrvDiscoGfxDecode(); // decode from RAM

	BurnTransferClear();

	if (nBurnLayer & 2) draw_chars(0, btime_palette, -1);
	if (nBurnLayer & 4) draw_sprites(btime_palette, 0, 0, DrvSpriteRAM, 1);

	BurnTransferCopy(DrvPalette);

	return 0;
}

#define FILTERDEBUG 0

#if FILTERDEBUG
extern int counter; // save for later debugging/tweaking. -dink
int lastctr = 0;
#endif

static INT32 BtimeFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	M6502NewFrame();

	{
		static UINT8 prevcoin = 0;

		memset(DrvInputs, ((discomode) ? 0x00 : 0xff), 3);

		if (btimemode) {
			DrvInputs[0] = DrvInputs[1] = 0;
			DrvInputs[2] = 0x3f;
		}

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		if (btimemode) {
			ProcessJoystick(&DrvInputs[0], 0, 2,3,1,0, INPUT_4WAY | INPUT_CLEAROPPOSITES | INPUT_MAKEACTIVELOW );
			ProcessJoystick(&DrvInputs[1], 1, 2,3,1,0, INPUT_4WAY | INPUT_CLEAROPPOSITES | INPUT_MAKEACTIVELOW );
		}

		UINT8 thiscoin = (DrvJoy3[6] << 6) | (DrvJoy3[7] << 7);

		if (thiscoin && (prevcoin != thiscoin)) {
			M6502Open(0);
			if (discomode || zoarmode || btime3mode)
				M6502SetIRQLine(0, CPU_IRQSTATUS_HOLD);
			else
				M6502SetIRQLine(0x20, CPU_IRQSTATUS_AUTO);
			M6502Close();
		}
		prevcoin = thiscoin;
	}

	INT32 nInterleave = 272;

	INT32 nCyclesTotal[2] = { ((discomode) ? 750000 : 1500000) / 60, ((zippysoundinit) ? 6500000 : 500000) / 60 };
	INT32 nCyclesDone[2]  = { 0, 0 };
	INT32 nSoundBufferPos = 0;

	if (zippysoundinit) zippysoundinit--;

	vblank = 0x80;

#if FILTERDEBUG
	if (counter != lastctr) { // save this block for tweaking filters etc. -dink
		lastctr = counter;

		//btimepalettewrite(3, 0x3f);
		//filter_rc_set_RC(0, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_N(0x4a0 + (counter * 0x10)));
		//filter_rc_set_RC(7, FLT_RC_LOWPASS, 1000+counter*100, 5100, 0, CAP_N(0x310/* + (counter * 0x10)*/ ));
		//filter_rc_set_RC(6, FLT_RC_HIGHPASS, 3846, 0, 0, CAP_N(0x310+counter*0x10));
		//filter_rc_set_RC(7, FLT_RC_LOWPASS, 3846, 0, 0, CAP_N(0x310 + (counter * 0x10)));
	}
#endif

	for (INT32 i = 0; i < nInterleave; i++)
	{
		M6502Open(0);
		INT32 nSegment = (nCyclesTotal[0] - nCyclesDone[0]) / (nInterleave - i);
		nCyclesDone[0] += M6502Run(nSegment);
		M6502Close();

		if (i == 248) vblank = 0x80;
		if (i == 8) {
			vblank = 0x00;

			if (pBurnDraw) {
				BurnDrvRedraw();
			}

		}

		M6502Open(1);
		nSegment = (nCyclesTotal[1] - nCyclesDone[1]) / (nInterleave - i);
		nCyclesDone[1] += M6502Run(nSegment);

		audio_nmi_state = (i + 1) & 8;
		M6502SetIRQLine(0x20, (audio_nmi_enable && audio_nmi_state) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);

		M6502Close();

		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			AY8910Render(&pAY8910Buffer[0], pSoundBuf, nSegmentLength, 0);

			filter_rc_update(0, pAY8910Buffer[0], pSoundBuf, nSegmentLength);
			filter_rc_update(1, pAY8910Buffer[1], pSoundBuf, nSegmentLength);
			filter_rc_update(2, pAY8910Buffer[2], pSoundBuf, nSegmentLength);

			if (btimemode) {
				filter_rc_update(6, pSoundBuf, hpfiltbuffer, nSegmentLength);
				memmove(pSoundBuf, hpfiltbuffer, nSegmentLength*4);
			}

			filter_rc_update(3, pAY8910Buffer[3], pSoundBuf, nSegmentLength);
			filter_rc_update(4, pAY8910Buffer[4], pSoundBuf, nSegmentLength);
			filter_rc_update(5, pAY8910Buffer[5], pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			AY8910Render(&pAY8910Buffer[0], pSoundBuf, nSegmentLength, 0);

			filter_rc_update(0, pAY8910Buffer[0], pSoundBuf, nSegmentLength);
			filter_rc_update(1, pAY8910Buffer[1], pSoundBuf, nSegmentLength);
			filter_rc_update(2, pAY8910Buffer[2], pSoundBuf, nSegmentLength);

			if (btimemode) {
				filter_rc_update(6, pSoundBuf, hpfiltbuffer, nSegmentLength);
				memmove(pSoundBuf, hpfiltbuffer, nSegmentLength*4);
			}

			filter_rc_update(3, pAY8910Buffer[3], pSoundBuf, nSegmentLength);
			filter_rc_update(4, pAY8910Buffer[4], pSoundBuf, nSegmentLength);
			filter_rc_update(5, pAY8910Buffer[5], pSoundBuf, nSegmentLength);
		}
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {			// Return minimum compatible version
		*pnMin = 0x029719;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		if (mmonkeymode) {
			memset(&ba, 0, sizeof(ba));
			ba.Data	  = DrvMainROMdec;
			ba.nLen	  = 0x10000;
			ba.szName = "decROMops";
			BurnAcb(&ba);
		}
	}

	if (nAction & ACB_DRIVER_DATA) {
		M6502Scan(nAction);

		AY8910Scan(nAction, pnMin);

		SCAN_VAR(vblank);
		SCAN_VAR(soundlatch);
		SCAN_VAR(flipscreen);
		SCAN_VAR(audio_nmi_type);
		SCAN_VAR(audio_nmi_enable);
		SCAN_VAR(audio_nmi_state);
		SCAN_VAR(bnj_scroll1);
		SCAN_VAR(bnj_scroll2);
	}

	return 0;
}

// Burger Time (Data East set 1)

static struct BurnRomInfo btimeRomDesc[] = {
	{ "aa04.9b",	0x1000, 0x368a25b5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "aa06.13b",	0x1000, 0xb4ba400d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "aa05.10b",	0x1000, 0x8005bffa, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "aa07.15b",	0x1000, 0x086440ad, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "ab14.12h",	0x1000, 0xf55e5211, 2 | BRF_PRG | BRF_ESS }, //  4 audiocpu

	{ "aa12.7k",	0x1000, 0xc4617243, 3 | BRF_GRA },           //  5 gfx1
	{ "ab13.9k",	0x1000, 0xac01042f, 3 | BRF_GRA },           //  6
	{ "ab10.10k",	0x1000, 0x854a872a, 3 | BRF_GRA },           //  7
	{ "ab11.12k",	0x1000, 0xd4848014, 3 | BRF_GRA },           //  8
	{ "aa8.13k",	0x1000, 0x8650c788, 3 | BRF_GRA },           //  9
	{ "ab9.15k",	0x1000, 0x8dec15e6, 3 | BRF_GRA },           // 10

	{ "ab00.1b",	0x0800, 0xc7a14485, 4 | BRF_GRA },           // 11 gfx2
	{ "ab01.3b",	0x0800, 0x25b49078, 4 | BRF_GRA },           // 12
	{ "ab02.4b",	0x0800, 0xb8ef56c3, 4 | BRF_GRA },           // 13

	{ "ab03.6b",	0x0800, 0xd26bc1f3, 5 | BRF_GRA },           // 14 bg_map
};

STD_ROM_PICK(btime)
STD_ROM_FN(btime)

struct BurnDriver BurnDrvBtime = {
	"btime", NULL, NULL, NULL, "1982",
	"Burger Time (Data East set 1)\0", NULL, "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, btimeRomInfo, btimeRomName, NULL, NULL, BtimeInputInfo, BtimeDIPInfo,
	BtimeInit, DrvExit, BtimeFrame, BtimeDraw, DrvScan, &DrvRecalc, 16,
	240, 242, 3, 4
};

// Burger Time (Data East set 2)

static struct BurnRomInfo btime2RomDesc[] = {
	{ "aa04.9b2",	0x1000, 0xa041e25b, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "aa06.13b",	0x1000, 0xb4ba400d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "aa05.10b",	0x1000, 0x8005bffa, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "aa07.15b",	0x1000, 0x086440ad, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "ab14.12h",	0x1000, 0xf55e5211, 2 | BRF_PRG | BRF_ESS }, //  4 audiocpu

	{ "aa12.7k",	0x1000, 0xc4617243, 3 | BRF_GRA },           //  5 gfx1
	{ "ab13.9k",	0x1000, 0xac01042f, 3 | BRF_GRA },           //  6
	{ "ab10.10k",	0x1000, 0x854a872a, 3 | BRF_GRA },           //  7
	{ "ab11.12k",	0x1000, 0xd4848014, 3 | BRF_GRA },           //  8
	{ "aa8.13k",	0x1000, 0x8650c788, 3 | BRF_GRA },           //  9
	{ "ab9.15k",	0x1000, 0x8dec15e6, 3 | BRF_GRA },           // 10

	{ "ab00.1b",	0x0800, 0xc7a14485, 4 | BRF_GRA },           // 11 gfx2
	{ "ab01.3b",	0x0800, 0x25b49078, 4 | BRF_GRA },           // 12
	{ "ab02.4b",	0x0800, 0xb8ef56c3, 4 | BRF_GRA },           // 13

	{ "ab03.6b",	0x0800, 0xd26bc1f3, 5 | BRF_GRA },           // 14 bg_map
};

STD_ROM_PICK(btime2)
STD_ROM_FN(btime2)

struct BurnDriver BurnDrvBtime2 = {
	"btime2", "btime", NULL, NULL, "1982",
	"Burger Time (Data East set 2)\0", NULL, "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, btime2RomInfo, btime2RomName, NULL, NULL, BtimeInputInfo, BtimeDIPInfo,
	BtimeInit, DrvExit, BtimeFrame, BtimeDraw, DrvScan, &DrvRecalc, 16,
	240, 242, 3, 4
};

INT32 Btime3Init()
{
	btime3mode = 1;
	return BtimeInit();
}

// Burger Time (Data East USA)

static struct BurnRomInfo btime3RomDesc[] = {
	{ "ab05a-3.12b",	0x1000, 0x12e9f58c, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "ab04-3.9b",		0x1000, 0x5d90c696, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ab06-3.13b",		0x1000, 0xe0b993ad, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ab05-3.10b",		0x1000, 0xc2b44b7f, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ab07-3.15b",		0x1000, 0x91986594, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "ab14-1.12h",		0x1000, 0xf55e5211, 2 | BRF_PRG | BRF_ESS }, //  5 audiocpu

	{ "ab12-1.7k",		0x1000, 0x6c79f79f, 3 | BRF_GRA },           //  6 gfx1
	{ "ab13-1.9k",		0x1000, 0xac01042f, 3 | BRF_GRA },           //  7
	{ "ab10-1.10k",		0x1000, 0x854a872a, 3 | BRF_GRA },           //  8
	{ "ab11-1.12k",		0x1000, 0xd4848014, 3 | BRF_GRA },           //  9
	{ "ab8-1.13k",		0x1000, 0x70b35bbe, 3 | BRF_GRA },           // 10
	{ "ab9-1.15k",		0x1000, 0x8dec15e6, 3 | BRF_GRA },           // 11

	{ "ab00-1.1b",		0x0800, 0xc7a14485, 4 | BRF_GRA },           // 12 gfx2
	{ "ab01-1.3b",		0x0800, 0x25b49078, 4 | BRF_GRA },           // 13
	{ "ab02-1.4b",		0x0800, 0xb8ef56c3, 4 | BRF_GRA },           // 14

	{ "ab03-3.6b",		0x0800, 0xf699d797, 5 | BRF_GRA },           // 15 bg_map
};

STD_ROM_PICK(btime3)
STD_ROM_FN(btime3)

struct BurnDriver BurnDrvBtime3 = {
	"btime3", "btime", NULL, NULL, "1982",
	"Burger Time (Data East USA)\0", "graphics issues, use parent romset!", "Data East USA Inc.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, btime3RomInfo, btime3RomName, NULL, NULL, BtimeInputInfo, BtimeDIPInfo,
	Btime3Init, DrvExit, BtimeFrame, BtimeDraw, DrvScan, &DrvRecalc, 16,
	240, 242, 3, 4
};

// Burger Time (Midway)

static struct BurnRomInfo btimemRomDesc[] = {
	{ "ab05a1.12b",	0x1000, 0x0a98b230, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "ab04.9b",	0x1000, 0x797e5f75, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ab06.13b",	0x1000, 0xc77f3f64, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ab05.10b",	0x1000, 0xb0d3640f, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ab07.15b",	0x1000, 0xa142f862, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "ab14.12h",	0x1000, 0xf55e5211, 2 | BRF_PRG | BRF_ESS }, //  5 audiocpu

	{ "ab12.7k",	0x1000, 0x6c79f79f, 3 | BRF_GRA },           //  6 gfx1
	{ "ab13.9k",	0x1000, 0xac01042f, 3 | BRF_GRA },           //  7
	{ "ab10.10k",	0x1000, 0x854a872a, 3 | BRF_GRA },           //  8
	{ "ab11.12k",	0x1000, 0xd4848014, 3 | BRF_GRA },           //  9
	{ "ab8.13k",	0x1000, 0x70b35bbe, 3 | BRF_GRA },           // 10
	{ "ab9.15k",	0x1000, 0x8dec15e6, 3 | BRF_GRA },           // 11

	{ "ab00.1b",	0x0800, 0xc7a14485, 4 | BRF_GRA },           // 12 gfx2
	{ "ab01.3b",	0x0800, 0x25b49078, 4 | BRF_GRA },           // 13
	{ "ab02.4b",	0x0800, 0xb8ef56c3, 4 | BRF_GRA },           // 14

	{ "ab03.6b",	0x0800, 0xd26bc1f3, 5 | BRF_GRA },           // 15 bg_map
};

STD_ROM_PICK(btimem)
STD_ROM_FN(btimem)

struct BurnDriver BurnDrvBtimem = {
	"btimem", "btime", NULL, NULL, "1982",
	"Burger Time (Midway)\0", NULL, "Data East (Bally Midway license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, btimemRomInfo, btimemRomName, NULL, NULL, BtimeInputInfo, BtimeDIPInfo,
	Btime3Init, DrvExit, BtimeFrame, BtimeDraw, DrvScan, &DrvRecalc, 16,
	240, 242, 3, 4
};


// Minky Monkey

static struct BurnRomInfo mmonkeyRomDesc[] = {
	{ "mmonkey.e4",		0x1000, 0x8d31bf6a, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "mmonkey.d4",		0x1000, 0xe54f584a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mmonkey.b4",		0x1000, 0x399a161e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "mmonkey.a4",		0x1000, 0xf7d3d1e3, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "mmonkey.h1",		0x1000, 0x5bcb2e81, 2 | BRF_PRG | BRF_ESS }, //  4 audiocpu

	{ "mmonkey.l11",	0x1000, 0xb6aa8566, 3 | BRF_GRA },           //  5 gfx1
	{ "mmonkey.m11",	0x1000, 0x6cc4d0c4, 3 | BRF_GRA },           //  6
	{ "mmonkey.l13",	0x1000, 0x2a343b7e, 3 | BRF_GRA },           //  7
	{ "mmonkey.m13",	0x1000, 0x0230b50d, 3 | BRF_GRA },           //  8
	{ "mmonkey.l14",	0x1000, 0x922bb3e1, 3 | BRF_GRA },           //  9
	{ "mmonkey.m14",	0x1000, 0xf943e28c, 3 | BRF_GRA },           // 10

	{ "mmi6331.m5",		0x0020, 0x55e28b32, 4 | BRF_GRA },           // 11 proms
	{ "sb-4c",		0x0020, 0xa29b4204, 4 | BRF_GRA },           // 12
};

STD_ROM_PICK(mmonkey)
STD_ROM_FN(mmonkey)

struct BurnDriver BurnDrvMmonkey = {
	"mmonkey", NULL, NULL, NULL, "1982",
	"Minky Monkey\0", NULL, "Technos Japan / Roller Tron", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, mmonkeyRomInfo, mmonkeyRomName, NULL, NULL, MmonkeyInputInfo, MmonkeyDIPInfo,
	MmonkeyInit, DrvExit, BtimeFrame, eggsDraw, DrvScan, &DrvRecalc, 16,
	240, 242, 3, 4
};

INT32 brubberInit()
{
	brubbermode = 1;
	return BnjInit();
}

// Burnin' Rubber

static struct BurnRomInfo brubberRomDesc[] = {
	{ "brubber.12c",	0x2000, 0xb5279c70, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "brubber.12d",	0x2000, 0xb2ce51f5, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "bnj6c.bin",		0x1000, 0x8c02f662, 2 | BRF_PRG | BRF_ESS }, //  2 audiocpu

	{ "bnj4e.bin",		0x2000, 0xb864d082, 3 | BRF_GRA },           //  3 gfx1
	{ "bnj4f.bin",		0x2000, 0x6c31d77a, 3 | BRF_GRA },           //  4
	{ "bnj4h.bin",		0x2000, 0x5824e6fb, 3 | BRF_GRA },           //  5

	{ "bnj10e.bin",		0x1000, 0xf4e9eb49, 4 | BRF_GRA },           //  6 gfx2
	{ "bnj10f.bin",		0x1000, 0xa9ffacb4, 4 | BRF_GRA },           //  7
};

STD_ROM_PICK(brubber)
STD_ROM_FN(brubber)

struct BurnDriver BurnDrvBrubber = {
	"brubber", NULL, NULL, NULL, "1982",
	"Burnin' Rubber\0", NULL, "Data East", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, brubberRomInfo, brubberRomName, NULL, NULL, BnjInputInfo, BnjDIPInfo,
	brubberInit, DrvExit, BtimeFrame, BnjDraw, DrvScan, &DrvRecalc, 16,
	240, 256, 3, 4
};


// Bump 'n' Jump

static struct BurnRomInfo bnjRomDesc[] = {
	{ "ad08.12b",		0x2000, 0x8d649bd5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "ad07.12c",		0x2000, 0x7a27f5f4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ad06.12d",		0x2000, 0xf855a2d2, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "ad05.6c",		0x1000, 0x8c02f662, 2 | BRF_PRG | BRF_ESS }, //  3 audiocpu

	{ "ad00.4e",		0x2000, 0xb864d082, 3 | BRF_GRA },           //  4 gfx1
	{ "ad01.4f",		0x2000, 0x6c31d77a, 3 | BRF_GRA },           //  5
	{ "ad02.4h",		0x2000, 0x5824e6fb, 3 | BRF_GRA },           //  6

	{ "ad03.10e",		0x1000, 0xf4e9eb49, 4 | BRF_GRA },           //  7 gfx2
	{ "ad04.10f",		0x1000, 0xa9ffacb4, 4 | BRF_GRA },           //  8

	{ "pb-5.10k.bin",	0x002c, 0xdc72a65f, 5 | BRF_OPT },           //  9 plds
	{ "pb-4.2d.bin",	0x0001, 0x00000000, 5 | BRF_NODUMP | BRF_OPT },           // 10
};

STD_ROM_PICK(bnj)
STD_ROM_FN(bnj)

struct BurnDriver BurnDrvBnj = {
	"bnj", "brubber", NULL, NULL, "1982",
	"Bump 'n' Jump\0", NULL, "Data East USA", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, bnjRomInfo, bnjRomName, NULL, NULL, BnjInputInfo, BnjDIPInfo,
	BnjInit, DrvExit, BtimeFrame, BnjDraw, DrvScan, &DrvRecalc, 16,
	240, 256, 3, 4
};

// Disco No.1

static struct BurnRomInfo discoRomDesc[] = {
	{ "disco.w5",	0x1000, 0xb2c87b78, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "disco.w4",	0x1000, 0xad7040ee, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "disco.w3",	0x1000, 0x12fb4f08, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "disco.w2",	0x1000, 0x73f6fb2f, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "disco.w1",	0x1000, 0xee7b536b, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "disco.w0",	0x1000, 0x7c26e76b, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "disco.w6",	0x1000, 0xd81e781e, 2 | BRF_PRG | BRF_ESS }, //  6 audiocpu

	{ "disco.clr",	0x0020, 0xa393f913, 3 | BRF_GRA },           //  7 proms
};

STD_ROM_PICK(disco)
STD_ROM_FN(disco)

struct BurnDriver BurnDrvDisco = {
	"disco", NULL, NULL, NULL, "1982",
	"Disco No.1\0", NULL, "Data East", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, discoRomInfo, discoRomName, NULL, NULL, DiscoInputInfo, DiscoDIPInfo,
	DiscoInit, DrvExit, BtimeFrame, DiscoDraw, DrvScan, &DrvRecalc, 32,
	240, 242, 3, 4
};


// Disco No.1 (Rev.F)

static struct BurnRomInfo discofRomDesc[] = {
	{ "w5-f.1a",	0x1000, 0x9d53c71c, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "w4-f.2a",	0x1000, 0xc1f8d747, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "w3-f.4a",	0x1000, 0x9aadd252, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "w2-f.6a",	0x1000, 0xf131a5bb, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "w1-f.9a",	0x1000, 0xa6ce9a19, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "w0-f.9a",	0x1000, 0xb3787a92, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "w6-.1b",	0x1000, 0xd81e781e, 2 | BRF_PRG | BRF_ESS }, //  6 audiocpu

	{ "disco.clr",	0x0020, 0xa393f913, 3 | BRF_GRA },           //  7 proms
};

STD_ROM_PICK(discof)
STD_ROM_FN(discof)

struct BurnDriver BurnDrvDiscof = {
	"discof", "disco", NULL, NULL, "1982",
	"Disco No.1 (Rev.F)\0", NULL, "Data East", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, discofRomInfo, discofRomName, NULL, NULL, DiscoInputInfo, DiscoDIPInfo,
	DiscoInit, DrvExit, BtimeFrame, DiscoDraw, DrvScan, &DrvRecalc, 32,
	240, 242, 3, 4
};

INT32 LncInit()
{
	lncmode = 1;
	return MmonkeyInit();
}

// Lock'n'Chase

static struct BurnRomInfo lncRomDesc[] = {
	{ "s3-3d",	0x1000, 0x1ab4f2c2, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "s2-3c",	0x1000, 0x5e46b789, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "s1-3b",	0x1000, 0x1308a32e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "s0-3a",	0x1000, 0xbeb4b1fc, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "sa-1h",	0x1000, 0x379387ec, 2 | BRF_PRG | BRF_ESS }, //  4 audiocpu

	{ "s4-11l",	0x1000, 0xa2162a9e, 3 | BRF_GRA },           //  5 gfx1
	{ "s5-11m",	0x1000, 0x12f1c2db, 3 | BRF_GRA },           //  6
	{ "s6-13l",	0x1000, 0xd21e2a57, 3 | BRF_GRA },           //  7
	{ "s7-13m",	0x1000, 0xc4f247cd, 3 | BRF_GRA },           //  8
	{ "s8-15l",	0x1000, 0x672a92d0, 3 | BRF_GRA },           //  9
	{ "s9-15m",	0x1000, 0x87c8ee9a, 3 | BRF_GRA },           // 10

	{ "sc-5m",	0x0020, 0x2a976ebe, 4 | BRF_GRA },           // 11 proms
	{ "sb-4c",	0x0020, 0xa29b4204, 4 | BRF_GRA },           // 12
};

STD_ROM_PICK(lnc)
STD_ROM_FN(lnc)

struct BurnDriver BurnDrvLnc = {
	"lnc", NULL, NULL, NULL, "1981",
	"Lock'n'Chase\0", NULL, "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, lncRomInfo, lncRomName, NULL, NULL, LncInputInfo, LncDIPInfo,
	LncInit, DrvExit, BtimeFrame, LncDraw, DrvScan, &DrvRecalc, 16,
	240, 242, 3, 4
};

// Zoar

static struct BurnRomInfo zoarRomDesc[] = {
	{ "z15.12b",	0x1000, 0x1f0cfdb7, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "z16.13b",	0x1000, 0x7685999c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "z17.15b",	0x1000, 0x619ea867, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "z09.13c",	0x1000, 0x18d96ff1, 2 | BRF_PRG | BRF_ESS }, //  3 audiocpu

	{ "z00.3l",	0x1000, 0xfd2dcb64, 3 | BRF_GRA },           //  4 gfx1
	{ "z01.5l",	0x1000, 0x74d3ca48, 3 | BRF_GRA },           //  5
	{ "z03.8l",	0x1000, 0x77b7df14, 3 | BRF_GRA },           //  6
	{ "z04.9l",	0x1000, 0x9be786de, 3 | BRF_GRA },           //  7
	{ "z06.12l",	0x1000, 0x07638c71, 3 | BRF_GRA },           //  8
	{ "z07.14l",	0x1000, 0xf4710f25, 3 | BRF_GRA },           //  9

	{ "z10.1b",	0x0800, 0xaa8bcab8, 4 | BRF_GRA },           // 10 gfx2
	{ "z11.3b",	0x0800, 0xdcdad357, 4 | BRF_GRA },           // 11
	{ "z12.4b",	0x0800, 0xed317e40, 4 | BRF_GRA },           // 12

	{ "z02.6l",	0x1000, 0xd8c3c122, 5 | BRF_GRA },           // 13 gfx3
	{ "z05.14l",	0x1000, 0x05dc6b09, 5 | BRF_GRA },           // 14
	{ "z08.15l",	0x1000, 0x9a148551, 5 | BRF_GRA },           // 15

	{ "z13.6b",	0x1000, 0x8fefa960, 6 | BRF_GRA },           // 16 bg_map

	{ "z20.1l",	0x0020, 0xa63f0a07, 7 | BRF_GRA },           // 17 proms
	{ "z21.2l",	0x0020, 0x5e1e5788, 7 | BRF_GRA },           // 18
	{ "z19.7b",	0x0020, 0x03ee3a96, 7 | BRF_GRA },           // 19
};

STD_ROM_PICK(zoar)
STD_ROM_FN(zoar)

struct BurnDriver BurnDrvZoar = {
	"zoar", NULL, NULL, NULL, "1982",
	"Zoar\0", "Sound issues..", "Data East USA", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, zoarRomInfo, zoarRomName, NULL, NULL, ZoarInputInfo, ZoarDIPInfo,
	ZoarInit, DrvExit, BtimeFrame, ZoarDraw, DrvScan, &DrvRecalc, 64,
	240, 256, 3, 4
};

