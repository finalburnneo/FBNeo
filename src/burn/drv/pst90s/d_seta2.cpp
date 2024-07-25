/********************************************************************************
 Newer Seta Hardware
 MAME driver by Luca Elia (l.elia@tin.it)
 ********************************************************************************
 port to Finalburn Neo by OopsWare. 2007
 ********************************************************************************/

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "burn_gun.h"
#include "eeprom.h"
#include "x1010.h"
#include "rectangle.h"

static UINT8 *Mem = NULL, *MemEnd = NULL;
static UINT8 *RamStart, *RamEnd;

static UINT8 *Rom68K;
static UINT8 *RomGfx;

static UINT8 *Ram68K;
static UINT8 *RamUnknown;
static UINT8 *RamNV;

static UINT16 *RamSpr;
static UINT16 *RamSprPriv;
static UINT16 *RamPal;
static UINT32 *CurPal;
static UINT16 *RamTMP68301;
static UINT16 *RamVReg;

static UINT8 DrvButton[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvJoy1[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvJoy2[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvJoy3[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvJoy4[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvJoy5[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvInput[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static INT16 DrvAxis[4];
static UINT8 DrvAnalogInput[4];

static UINT8 DrvReset = 0;
static UINT8 bRecalcPalette = 0;

//static UINT8 bMahjong = 0;
static UINT8 Mahjong_keyboard = 0;
static UINT8 HasNVRam = 0;

static INT32 nRomGfxLen;

static INT32 is_samshoot;
static INT32 grdiansbl = 0;

static INT32 nExtraCycles = 0;

static INT32 raster_extra;
static INT32 raster_latch;
static INT32 raster_pos;
static INT32 raster_en;
static INT32 current_scanline;

static INT32 M68K_CYCS = 50000000 / 3;

#define A(a, b, c, d) { a, b, (UINT8*)(c), d }

static struct BurnInputInfo grdiansInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvButton + 0,	"p1 coin"},

	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},
	{"P1 Button 2",	BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"},
	{"P1 Button 3",	BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"},
	{"P1 Start",	BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"},

	{"P2 Coin",		BIT_DIGITAL,	DrvButton + 1,	"p2 coin"},

	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"},
	{"P2 Right",	BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"},
	{"P2 Button 1",	BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"},
	{"P2 Button 2",	BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"},
	{"P2 Button 3",	BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"},
	{"P2 Start",	BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,		"reset"},
	{"Diagnostics",	BIT_DIGITAL, DrvButton + 3,	"diag"},
	{"Service",		BIT_DIGITAL, DrvButton + 2,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvInput + 3,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvInput + 4,	"dip"},
};

STDINPUTINFO(grdians)

static struct BurnDIPInfo grdiansDIPList[] = {

	// Defaults
	{0x15,	0xFF, 0xFF,	0x00, NULL},
	{0x16,	0xFF, 0xFF,	0x00, NULL},

	// DIP 1
	{0,		0xFE, 0,	4,	  "Difficulty"},
	{0x15,	0x01, 0x03, 0x01, "Easy"},
	{0x15,	0x01, 0x03, 0x00, "Normal"},
	{0x15,	0x01, 0x03, 0x02, "Hard"},
	{0x15,	0x01, 0x03, 0x03, "Hardest"},
	{0,		0xFE, 0,	2,	  "Bonus Life"},
	{0x15,	0x01, 0x04,	0x00, "500,000"},
	{0x15,	0x01, 0x04,	0x04, "300,000"},
	{0,		0xFE, 0,	2,	  "Title"},
	{0x15,	0x01, 0x08,	0x00, "Guardians"},
	{0x15,	0x01, 0x08,	0x08, "Denjin Makai II"},
	{0,		0xFE, 0,	4,	  "Lives"},
	{0x15,	0x01, 0x30, 0x10, "1"},
	{0x15,	0x01, 0x30, 0x00, "2"},
	{0x15,	0x01, 0x30, 0x20, "3"},
	{0x15,	0x01, 0x30, 0x30, "4"},
	{0,		0xFE, 0,	2,	  "Test mode"},
	{0x15,	0x01, 0x40, 0x00, "Off"}, 
	{0x15,	0x01, 0x40, 0x40, "On"},
	{0,		0xFE, 0,	2,	  "Demo sounds"},
	{0x15,	0x01, 0x80, 0x80, "Off"},
	{0x15,	0x01, 0x80, 0x00, "On"},

	// DIP 2
	{0,		0xFE, 0,	16,	  "Coin 1"},
	{0x16,	0x01, 0x0f, 0x00, "1 coin 1 credit"},
	{0x16,	0x01, 0x0f, 0x01, "1 coin 2 credits"},
	{0x16,	0x01, 0x0f, 0x02, "1 coin 3 credits"},
	{0x16,	0x01, 0x0f, 0x03, "1 coin 4 credits"},
	{0x16,	0x01, 0x0f, 0x04, "1 coin 5 credits"},
	{0x16,	0x01, 0x0f, 0x05, "1 coin 6 credits"},
	{0x16,	0x01, 0x0f, 0x06, "1 coin 7 credits"},
	{0x16,	0x01, 0x0f, 0x07, "2 coins 1 credit"},
	{0x16,	0x01, 0x0f, 0x08, "2 coins 3 credits"},
	{0x16,	0x01, 0x0f, 0x09, "2 coins 5 credits"},
	{0x16,	0x01, 0x0f, 0x0a, "3 coins 1 credit"},
	{0x16,	0x01, 0x0f, 0x0b, "3 coins 2 credits"},
	{0x16,	0x01, 0x0f, 0x0c, "3 coins 4 credits"},
	{0x16,	0x01, 0x0f, 0x0d, "4 coins 1 credit"},
	{0x16,	0x01, 0x0f, 0x0e, "4 coins 3 credits"},
	{0x16,	0x01, 0x0f, 0x0f, "Free play"},
	{0,		0xFE, 0,	16,	  "Coin 2"},
	{0x16,	0x01, 0xf0, 0x00, "1 coin 1 credit"},
	{0x16,	0x01, 0xf0, 0x10, "1 coin 2 credits"},
	{0x16,	0x01, 0xf0, 0x20, "1 coin 3 credits"},
	{0x16,	0x01, 0xf0, 0x30, "1 coin 4 credits"},
	{0x16,	0x01, 0xf0, 0x40, "1 coin 5 credits"},
	{0x16,	0x01, 0xf0, 0x50, "1 coin 6 credits"},
	{0x16,	0x01, 0xf0, 0x60, "1 coin 7 credits"},
	{0x16,	0x01, 0xf0, 0x70, "2 coins 1 credit"},
	{0x16,	0x01, 0xf0, 0x80, "2 coins 3 credits"},
	{0x16,	0x01, 0xf0, 0x90, "2 coins 5 credits"},
	{0x16,	0x01, 0xf0, 0xa0, "3 coins 1 credit"},
	{0x16,	0x01, 0xf0, 0xb0, "3 coins 2 credits"},
	{0x16,	0x01, 0xf0, 0xc0, "3 coins 4 credits"},
	{0x16,	0x01, 0xf0, 0xd0, "4 coins 1 credit"},
	{0x16,	0x01, 0xf0, 0xe0, "4 coins 3 credits"},
	{0x16,	0x01, 0xf0, 0xf0, "Free play"},
};

STDDIPINFO(grdians)

static struct BurnInputInfo mj4simaiInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvButton + 0,	"p1 coin"},
	{"P2 Coin",			BIT_DIGITAL,	DrvButton + 1,	"p2 coin"},

	{"P1 A",			BIT_DIGITAL,	DrvJoy1 + 0,	"mah a"},
	{"P1 E",			BIT_DIGITAL,	DrvJoy1 + 1,	"mah e"},
	{"P1 I",			BIT_DIGITAL,	DrvJoy1 + 2,	"mah i"},
	{"P1 M",			BIT_DIGITAL,	DrvJoy1 + 3,	"mah m"},
	{"P1 Kan",			BIT_DIGITAL,	DrvJoy1 + 4,	"mah kan"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 start"},

	{"P1 B",			BIT_DIGITAL,	DrvJoy2 + 0,	"mah b"},
	{"P1 F",			BIT_DIGITAL,	DrvJoy2 + 1,	"mah f"},
	{"P1 J",			BIT_DIGITAL,	DrvJoy2 + 2,	"mah j"},
	{"P1 N",			BIT_DIGITAL,	DrvJoy2 + 3,	"mah n"},
	{"P1 Reach",		BIT_DIGITAL,	DrvJoy2 + 4,	"mah reach"},
	{"P1 Bet",			BIT_DIGITAL,	DrvJoy2 + 5,	"mah bet"},

	{"P1 C",			BIT_DIGITAL,	DrvJoy3 + 0,	"mah c"},
	{"P1 G",			BIT_DIGITAL,	DrvJoy3 + 1,	"mah g"},
	{"P1 K",			BIT_DIGITAL,	DrvJoy3 + 2,	"mah k"},
	{"P1 Chi",			BIT_DIGITAL,	DrvJoy3 + 3,	"mah chi"},
	{"P1 Ron",			BIT_DIGITAL,	DrvJoy3 + 4,	"mah ron"},

	{"P1 D",			BIT_DIGITAL,	DrvJoy4 + 0,	"mah d"},
	{"P1 H",			BIT_DIGITAL,	DrvJoy4 + 1,	"mah h"},
	{"P1 L",			BIT_DIGITAL,	DrvJoy4 + 2,	"mah l"},
	{"P1 Pon",			BIT_DIGITAL,	DrvJoy4 + 3,	"mah pon"},

	{"P1 LastChange",	BIT_DIGITAL,	DrvJoy5 + 0,	"mah lc"},
	{"P1 Score",		BIT_DIGITAL,	DrvJoy5 + 1,	"mah score"},
	{"P1 DoubleUp",		BIT_DIGITAL,	DrvJoy5 + 2,	"mah du"},		// ????
	{"P1 FlipFlop",		BIT_DIGITAL,	DrvJoy5 + 3,	"mah ff"},
	{"P1 Big",			BIT_DIGITAL,	DrvJoy5 + 4,	"mah big"},		// ????
	{"P1 Smaill",		BIT_DIGITAL,	DrvJoy5 + 5,	"mah small"},	// ????

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"},
	{"Service",			BIT_DIGITAL, DrvButton + 2,	"service"},
	{"Dip A",			BIT_DIPSWITCH,	DrvInput + 3,	"dip"},
	{"Dip B",			BIT_DIPSWITCH,	DrvInput + 4,	"dip"},
};

STDINPUTINFO(mj4simai)

static struct BurnDIPInfo mj4simaiDIPList[] = {

	// Defaults
	{0x1F,	0xFF, 0xFF,	0x00, NULL},
	{0x20,	0xFF, 0xFF,	0x00, NULL},

	// DIP 1
	{0,		0xFE, 0,	8,	  "Coinage"},
	{0x1F,	0x01, 0x07, 0x00, "1 coin 1 credit"},
	{0x1F,	0x01, 0x07, 0x01, "1 coin 2 credits"},
	{0x1F,	0x01, 0x07, 0x02, "1 coin 3 credits"},
	{0x1F,	0x01, 0x07, 0x03, "1 coin 4 credits"},
	{0x1F,	0x01, 0x07, 0x04, "2 coins 1 credit"},
	{0x1F,	0x01, 0x07, 0x05, "3 coins 1 credit"},
	{0x1F,	0x01, 0x07, 0x06, "4 coins 1 credit"},
	{0x1F,	0x01, 0x07, 0x07, "5 coins 1 credit"},
	{0,		0xFE, 0,	2,	  "Demo sound"},
	{0x1F,	0x01, 0x08,	0x08, "Off"},
	{0x1F,	0x01, 0x08,	0x00, "On"},
	{0,		0xFE, 0,	2,	  "Tumo pin"},
	{0x1F,	0x01, 0x10,	0x10, "Off"},
	{0x1F,	0x01, 0x10,	0x00, "On"},
	{0,		0xFE, 0,	2,	  "Flip screen"},
	{0x1F,	0x01, 0x20,	0x00, "Off"},
	{0x1F,	0x01, 0x20,	0x20, "On"},
	{0,		0xFE, 0,	2,	  "Free play"},
	{0x1F,	0x01, 0x40,	0x00, "Off"},
	{0x1F,	0x01, 0x40,	0x40, "On"},
	{0,		0xFE, 0,	2,	  "Test mode"},
	{0x1F,	0x01, 0x80,	0x00, "Off"},
	{0x1F,	0x01, 0x80,	0x80, "On"},

	// DIP 2
	{0,		0xFE, 0,	8,	  "Difficulty"},
	{0x20,	0x01, 0x07, 0x03, "0"},
	{0x20,	0x01, 0x07, 0x04, "1"},
	{0x20,	0x01, 0x07, 0x05, "2"},
	{0x20,	0x01, 0x07, 0x06, "3"},
	{0x20,	0x01, 0x07, 0x07, "4"},
	{0x20,	0x01, 0x07, 0x00, "5"},
	{0x20,	0x01, 0x07, 0x01, "6"},
	{0x20,	0x01, 0x07, 0x02, "7"},
	{0,		0xFE, 0,	2,	  "Continue"},
	{0x20,	0x01, 0x08, 0x08, "Off"},
	{0x20,	0x01, 0x08, 0x00, "On"},
	{0,		0xFE, 0,	2,	  "Select girl"},
	{0x20,	0x01, 0x10, 0x00, "Off"},
	{0x20,	0x01, 0x10, 0x10, "On"},
	{0,		0xFE, 0,	2,	  "Com put"},
	{0x20,	0x01, 0x20, 0x00, "Off"},
	{0x20,	0x01, 0x20, 0x20, "On"},
};

STDDIPINFO(mj4simai)


static struct BurnInputInfo myangelInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvButton + 0,	"p1 coin"},

	{"P1 Start",	  BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 1"},
	{"P1 Button 2",	BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"},
	{"P1 Button 3",	BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 3"},
	{"P1 Button 4",	BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 4"},

	{"P2 Coin",		BIT_DIGITAL,	DrvButton + 1,	"p2 coin"},

	{"P2 Start",	  BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"},
	{"P2 Button 1",	BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 1"},
	{"P2 Button 2",	BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"},
	{"P2 Button 3",	BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 3"},
	{"P2 Button 4",	BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 4"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,		"reset"},
	{"Diagnostics",	BIT_DIGITAL, DrvButton + 3,	"diag"},
	{"Service",		BIT_DIGITAL, DrvButton + 2,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvInput + 3,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvInput + 4,	"dip"},
};

STDINPUTINFO(myangel)

static struct BurnDIPInfo myangel2DIPList[] = {

	// Defaults
	{0x0F,	0xFF, 0xFF,	0x00, NULL},
	{0x10,	0xFF, 0xFF,	0x00, NULL},

	// DIP 1
	{0,		0xFE, 0,	2,	  "Test mode"},
	{0x0F,	0x01, 0x01, 0x00, "Off"},
	{0x0F,	0x01, 0x01, 0x01, "On"},
	{0,		0xFE, 0,	2,	  "Increase lives while playing"},
	{0x0F,	0x01, 0x08,	0x08, "No"},
	{0x0F,	0x01, 0x08,	0x00, "Yes"},
	{0,		0xFE, 0,	4,	  "Lives"},
	{0x0F,	0x01, 0x30, 0x10, "2"},
	{0x0F,	0x01, 0x30, 0x00, "3"},
	{0x0F,	0x01, 0x30, 0x20, "4"},
	{0x0F,	0x01, 0x30, 0x30, "5"},
	{0,		0xFE, 0,	2,	  "Demo sounds"},
	{0x0F,	0x01, 0x40, 0x40, "Off"},
	{0x0F,	0x01, 0x40, 0x00, "On"},
	{0,		0xFE, 0,	2,	  "Flip screen"},
	{0x0F,	0x01, 0x80, 0x00, "Off"},
	{0x0F,	0x01, 0x80, 0x80, "On"},

	// DIP 2
	{0,		0xFE, 0,	16,	  "Coinage"},
	{0x10,	0x01, 0x0f, 0x00, "1 coin 1 credit"},
	{0x10,	0x01, 0x0f, 0x01, "1 coin 2 credits"},
	{0x10,	0x01, 0x0f, 0x02, "1 coin 3 credits"},
	{0x10,	0x01, 0x0f, 0x03, "1 coin 4 credits"},
	{0x10,	0x01, 0x0f, 0x04, "1 coin 5 credits"},
	{0x10,	0x01, 0x0f, 0x05, "1 coin 6 credits"},
	{0x10,	0x01, 0x0f, 0x06, "1 coin 7 credits"},
	{0x10,	0x01, 0x0f, 0x07, "2 coins 1 credit"},
	{0x10,	0x01, 0x0f, 0x08, "2 coins 3 credits"},
	{0x10,	0x01, 0x0f, 0x09, "2 coins 5 credits"},
	{0x10,	0x01, 0x0f, 0x0a, "3 coins 1 credit"},
	{0x10,	0x01, 0x0f, 0x0b, "3 coins 2 credits"},
	{0x10,	0x01, 0x0f, 0x0c, "3 coins 4 credits"},
	{0x10,	0x01, 0x0f, 0x0d, "4 coins 1 credit"},
	{0x10,	0x01, 0x0f, 0x0e, "4 coins 3 credits"},
	{0x10,	0x01, 0x0f, 0x0f, "Free play"},

};

static struct BurnDIPInfo myangelDIPList[] = {

	{0,		0xFE, 0,	2,	  "Push start to freeze (cheat)"},
	{0x10,	0x01, 0x80, 0x00, "No"},
	{0x10,	0x01, 0x80, 0x80, "Yes"},

};

STDDIPINFO(myangel2)
STDDIPINFOEXT(myangel, myangel2, myangel)

static struct BurnDIPInfo pzlbowlDIPList[] = {

	// Defaults
	{0x15,	0xFF, 0xFF,	0x00, NULL},
	{0x16,	0xFF, 0xFF,	0x80, NULL},

	// DIP 1
	{0,		0xFE, 0,	2,	  "Test mode"},
	{0x15,	0x01, 0x01, 0x00, "Off"}, 
	{0x15,	0x01, 0x01, 0x01, "On"},
	{0,		0xFE, 0,	2,	  "Demo sound"},
	{0x15,	0x01, 0x02, 0x02, "Off"},
	{0x15,	0x01, 0x02, 0x00, "On"}, 
	{0,		0xFE, 0,	2,	  "Flip screen"},
	{0x15,	0x01, 0x04, 0x00, "Off"}, 
	{0x15,	0x01, 0x04, 0x04, "On"},
	{0,		0xFE, 0,	8,	  "Difficulty"},
	{0x15,	0x01, 0x38, 0x08, "Easiest"},
	{0x15,	0x01, 0x38, 0x10, "Easier"},
	{0x15,	0x01, 0x38, 0x18, "Easy"},
	{0x15,	0x01, 0x38, 0x00, "Normal"},
	{0x15,	0x01, 0x38, 0x20, "Hard"},
	{0x15,	0x01, 0x38, 0x28, "Harder"},
	{0x15,	0x01, 0x38, 0x30, "Very hard"},
	{0x15,	0x01, 0x38, 0x38, "Hardest"},
	{0,		0xFE, 0,	4,	  "Winning rounds (player vs player)"},
	{0x15,	0x01, 0xc0, 0x80, "1"},
	{0x15,	0x01, 0xc0, 0x00, "2"}, 
	{0x15,	0x01, 0xc0, 0x40, "3"}, 
	{0x15,	0x01, 0xc0, 0xc0, "5"},
	
	// DIP 2
	{0,		0xFE, 0,	16,	  "Coinage"},
	{0x16,	0x01, 0x0f, 0x00, "1 coin 1 credit"},
	{0x16,	0x01, 0x0f, 0x01, "1 coin 2 credits"},
	{0x16,	0x01, 0x0f, 0x02, "1 coin 3 credits"},
	{0x16,	0x01, 0x0f, 0x03, "1 coin 4 credits"},
	{0x16,	0x01, 0x0f, 0x04, "1 coin 5 credits"},
	{0x16,	0x01, 0x0f, 0x05, "1 coin 6 credits"},
	{0x16,	0x01, 0x0f, 0x06, "1 coin 7 credits"},
	{0x16,	0x01, 0x0f, 0x07, "2 coins 1 credit"},
	{0x16,	0x01, 0x0f, 0x08, "2 coins 3 credits"},
	{0x16,	0x01, 0x0f, 0x09, "2 coins 5 credits"},
	{0x16,	0x01, 0x0f, 0x0a, "3 coins 1 credit"},
	{0x16,	0x01, 0x0f, 0x0b, "3 coins 2 credits"},
	{0x16,	0x01, 0x0f, 0x0c, "3 coins 4 credits"},
	{0x16,	0x01, 0x0f, 0x0d, "4 coins 1 credit"},
	{0x16,	0x01, 0x0f, 0x0e, "4 coins 3 credits"},
	{0x16,	0x01, 0x0f, 0x0f, "Free play"},
	{0,		0xFE, 0,	2,	  "Allow continue"},
	{0x16,	0x01, 0x10, 0x10, "No"},
	{0x16,	0x01, 0x10, 0x00, "Yes"},
	{0,		0xFE, 0,	2,	  "Join In"},
	{0x16,	0x01, 0x20, 0x20, "No"},
	{0x16,	0x01, 0x20, 0x00, "Yes"},
//	{0,		0xFE, 0,	2,	  "Unused"},
//	{0x16,	0x01, 0x40, 0x00, "Off"},
//	{0x16,	0x01, 0x40, 0x40, "On"},
	{0,		0xFE, 0,	2,	  "Language"},
	{0x16,	0x01, 0x80, 0x00, "Japanese"},
	{0x16,	0x01, 0x80, 0x80, "English"},

};

STDDIPINFO(pzlbowl)

static struct BurnInputInfo penbrosInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvButton + 0,	"p1 coin"},
	{"P1 Start",	BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"},

	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},
	{"P1 Button 2",	BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"},

	{"P2 Coin",		BIT_DIGITAL,	DrvButton + 1,	"p2 coin"},
	{"P2 Start",	BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"},

	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"},
	{"P2 Right",	BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"},
	{"P2 Button 1",	BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"},
	{"P2 Button 2",	BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,		"reset"},
	{"Diagnostics",	BIT_DIGITAL, DrvButton + 3,	"diag"},
	{"Service",		BIT_DIGITAL, DrvButton + 2,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvInput + 3,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvInput + 4,	"dip"},
};

STDINPUTINFO(penbros)

static struct BurnDIPInfo penbrosDIPList[] = {

	// Defaults
	{0x13,	0xFF, 0xFF,	0x00, NULL},
	{0x14,	0xFF, 0xFF,	0x00, NULL},

	// DIP 1
	{0,		0xFE, 0,	2,	  "Test mode"},
	{0x13,	0x01, 0x01, 0x00, "Off"}, 
	{0x13,	0x01, 0x01, 0x01, "On"},
	{0,		0xFE, 0,	2,	  "Screen"},
	{0x13,	0x01, 0x02, 0x00, "Normal"}, 
	{0x13,	0x01, 0x02, 0x02, "Reverse"},
	{0,		0xFE, 0,	2,	  "Demo sound"},
	{0x13,	0x01, 0x08, 0x08, "Off"},
	{0x13,	0x01, 0x08, 0x00, "On"}, 
	{0,		0xFE, 0,	4,	  "Coin A"},
	{0x13,	0x01, 0x30, 0x10, "3 coins 1 play"},
	{0x13,	0x01, 0x30, 0x20, "2 coins 1 play"},
	{0x13,	0x01, 0x30, 0x00, "1 coin 1 play"},
	{0x13,	0x01, 0x30, 0x30, "1 coin 2 plays"},
	{0,		0xFE, 0,	4,	  "Coin B"},
	{0x13,	0x01, 0xc0, 0x40, "3 coins 1 play"},
	{0x13,	0x01, 0xc0, 0x80, "2 coins 1 play"},
	{0x13,	0x01, 0xc0, 0x00, "1 coin 1 play"},
	{0x13,	0x01, 0xc0, 0xc0, "1 coin 2 plays"},

	// DIP 2
	{0,		0xFE, 0,	4,	  "Difficulty"},
	{0x14,	0x01, 0x03, 0x01, "Easy"},
	{0x14,	0x01, 0x03, 0x00, "Normal"},
	{0x14,	0x01, 0x03, 0x02, "Hard"},
	{0x14,	0x01, 0x03, 0x03, "Very hard"},
	{0,		0xFE, 0,	4,	  "Player stock"},
	{0x14,	0x01, 0x0c, 0x0c, "2"},
	{0x14,	0x01, 0x0c, 0x00, "3"},
	{0x14,	0x01, 0x0c, 0x08, "4"},
	{0x14,	0x01, 0x0c, 0x04, "5"},
	{0,		0xFE, 0,	4,	  "Extend"},
	{0x14,	0x01, 0x30, 0x20, "150000, 500000pts"},
	{0x14,	0x01, 0x30, 0x00, "200000, 700000pts"},
	{0x14,	0x01, 0x30, 0x30, "250000 every"},
	{0x14,	0x01, 0x30, 0x10, "None"},
	{0,		0xFE, 0,	4,	  "Match count"},
	{0x14,	0x01, 0xc0, 0x00, "2"},
	{0x14,	0x01, 0xc0, 0x80, "3"},
	{0x14,	0x01, 0xc0, 0x40, "4"},
	{0x14,	0x01, 0xc0, 0xc0, "5"},
};

STDDIPINFO(penbros)

static struct BurnInputInfo GundamexInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvButton + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 fire 4"},

	{"P2 Coin",		BIT_DIGITAL,	DrvButton + 1,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy4 + 0,	"p2 fire 4"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvButton + 2,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvInput + 3,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvInput + 4,	"dip"},
	{"Dip C",		BIT_DIPSWITCH,	DrvInput + 7,	"dip"},
};

STDINPUTINFO(Gundamex)

static struct BurnDIPInfo GundamexDIPList[]=
{
	{0x16, 0xff, 0xff, 0xff, NULL		},
	{0x17, 0xff, 0xff, 0xff, NULL		},
	{0x18, 0xff, 0xff, 0x20, NULL		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x16, 0x01, 0x01, 0x00, "Off"		},
	{0x16, 0x01, 0x01, 0x01, "On"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x16, 0x01, 0x06, 0x04, "Easy"		},
	{0x16, 0x01, 0x06, 0x06, "Normal"		},
	{0x16, 0x01, 0x06, 0x02, "Hard"		},
	{0x16, 0x01, 0x06, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x16, 0x01, 0x10, 0x10, "Off"		},
	{0x16, 0x01, 0x10, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x16, 0x01, 0x20, 0x20, "Off"		},
	{0x16, 0x01, 0x20, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Show Targets"		},
	{0x16, 0x01, 0x40, 0x40, "Off"		},
	{0x16, 0x01, 0x40, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x16, 0x01, 0x80, 0x80, "Off"		},
	{0x16, 0x01, 0x80, 0x00, "On"		},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x17, 0x01, 0x07, 0x00, "4 Coins 1 Credits "		},
	{0x17, 0x01, 0x07, 0x01, "3 Coins 1 Credits "		},
	{0x17, 0x01, 0x07, 0x02, "2 Coins 1 Credits "		},
	{0x17, 0x01, 0x07, 0x07, "1 Coin 1 Credits "		},
	{0x17, 0x01, 0x07, 0x06, "1 Coin 2 Credits "		},
	{0x17, 0x01, 0x07, 0x05, "1 Coin 3 Credits "		},
	{0x17, 0x01, 0x07, 0x03, "1 Coin 4 Credits "		},
	{0x17, 0x01, 0x07, 0x04, "1 Coin 5 Credits "		},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x17, 0x01, 0x38, 0x38, "1 Coin 1 Credits "		},
	{0x17, 0x01, 0x38, 0x10, "2 Coins 3 Credits "		},
	{0x17, 0x01, 0x38, 0x00, "3 Coins/5 Credits"		},
	{0x17, 0x01, 0x38, 0x30, "1 Coin 2 Credits "		},
	{0x17, 0x01, 0x38, 0x08, "2 Coins 5 Credits "		},
	{0x17, 0x01, 0x38, 0x28, "1 Coin 3 Credits "		},
	{0x17, 0x01, 0x38, 0x18, "1 Coin 4 Credits "		},
	{0x17, 0x01, 0x38, 0x20, "1 Coin 5 Credits "		},

	{0   , 0xfe, 0   ,    2, "Debug Mode"		},
	{0x17, 0x01, 0x40, 0x40, "Off"		},
	{0x17, 0x01, 0x40, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x17, 0x01, 0x80, 0x80, "Off"		},
	{0x17, 0x01, 0x80, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Language"		},
	{0x18, 0x01, 0x20, 0x20, "English"		},
	{0x18, 0x01, 0x20, 0x00, "Japanese"		},
};

STDDIPINFO(Gundamex)

static struct BurnInputInfo DeerhuntInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 start"},

	A("P1 Right / left",	BIT_ANALOG_REL, DrvAxis + 0,	"mouse x-axis"),
	A("P1 Up / Down",	BIT_ANALOG_REL, DrvAxis + 1,	"mouse y-axis"),

	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"mouse button 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"mouse button 2"},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy3 + 6,	"p1 fire 3"},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 4"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy4 + 2,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvInput + 6,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvInput + 7,	"dip"},
};

STDINPUTINFO(Deerhunt)

static struct BurnDIPInfo DeerhuntDIPList[]=
{
	{0x0a, 0xff, 0xff, 0xff, NULL		},
	{0x0b, 0xff, 0xff, 0x7f, NULL		},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x0a, 0x01, 0x07, 0x05, "4 Coins 1 Credits "		},
	{0x0a, 0x01, 0x07, 0x06, "2 Coins 1 Credits "		},
	{0x0a, 0x01, 0x07, 0x07, "1 Coin 1 Credits "		},
	{0x0a, 0x01, 0x07, 0x04, "1 Coin 2 Credits "		},
	{0x0a, 0x01, 0x07, 0x03, "1 Coin 3 Credits "		},
	{0x0a, 0x01, 0x07, 0x02, "1 Coin 4 Credits "		},
	{0x0a, 0x01, 0x07, 0x01, "1 Coin 5 Credits "		},
	{0x0a, 0x01, 0x07, 0x00, "1 Coin 6 Credits "		},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x0a, 0x01, 0x38, 0x28, "4 Coins 1 Credits "		},
	{0x0a, 0x01, 0x38, 0x30, "2 Coins 1 Credits "		},
	{0x0a, 0x01, 0x38, 0x38, "1 Coin 1 Credits "		},
	{0x0a, 0x01, 0x38, 0x20, "1 Coin 2 Credits "		},
	{0x0a, 0x01, 0x38, 0x18, "1 Coin 3 Credits "		},
	{0x0a, 0x01, 0x38, 0x10, "1 Coin 4 Credits "		},
	{0x0a, 0x01, 0x38, 0x08, "1 Coin 5 Credits "		},
	{0x0a, 0x01, 0x38, 0x00, "1 Coin 6 Credits "		},

	{0   , 0xfe, 0   ,    2, "Discount To Continue"		},
	{0x0a, 0x01, 0x40, 0x40, "Off"		},
	{0x0a, 0x01, 0x40, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x0a, 0x01, 0x80, 0x80, "Off"		},
	{0x0a, 0x01, 0x80, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Vert. Flip Screen"		},
	{0x0b, 0x01, 0x01, 0x01, "Off"		},
	{0x0b, 0x01, 0x01, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Horiz. Flip Screen"		},
	{0x0b, 0x01, 0x02, 0x02, "Off"		},
	{0x0b, 0x01, 0x02, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x0b, 0x01, 0x04, 0x00, "Off"		},
	{0x0b, 0x01, 0x04, 0x04, "On"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x0b, 0x01, 0x18, 0x10, "Easy"		},
	{0x0b, 0x01, 0x18, 0x18, "Normal"		},
	{0x0b, 0x01, 0x18, 0x08, "Hard"		},
	{0x0b, 0x01, 0x18, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Blood Color"		},
	{0x0b, 0x01, 0x20, 0x20, "Red"		},
	{0x0b, 0x01, 0x20, 0x00, "Yellow"		},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x0b, 0x01, 0x40, 0x40, "3"		},
	{0x0b, 0x01, 0x40, 0x00, "4"		},

	{0   , 0xfe, 0   ,    2, "Gun Type"		},
	{0x0b, 0x01, 0x80, 0x80, "Pump Action"		},
	{0x0b, 0x01, 0x80, 0x00, "Hand Gun"		},
};

STDDIPINFO(Deerhunt)


static struct BurnDIPInfo TurkhuntDIPList[]=
{
	{0x0a, 0xff, 0xff, 0xff, NULL		},
	{0x0b, 0xff, 0xff, 0x7f, NULL		},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x0a, 0x01, 0x07, 0x05, "4 Coins 1 Credits "		},
	{0x0a, 0x01, 0x07, 0x06, "2 Coins 1 Credits "		},
	{0x0a, 0x01, 0x07, 0x07, "1 Coin 1 Credits "		},
	{0x0a, 0x01, 0x07, 0x04, "1 Coin 2 Credits "		},
	{0x0a, 0x01, 0x07, 0x03, "1 Coin 3 Credits "		},
	{0x0a, 0x01, 0x07, 0x02, "1 Coin 4 Credits "		},
	{0x0a, 0x01, 0x07, 0x01, "1 Coin 5 Credits "		},
	{0x0a, 0x01, 0x07, 0x00, "1 Coin 6 Credits "		},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x0a, 0x01, 0x38, 0x28, "4 Coins 1 Credits "		},
	{0x0a, 0x01, 0x38, 0x30, "2 Coins 1 Credits "		},
	{0x0a, 0x01, 0x38, 0x38, "1 Coin 1 Credits "		},
	{0x0a, 0x01, 0x38, 0x20, "1 Coin 2 Credits "		},
	{0x0a, 0x01, 0x38, 0x18, "1 Coin 3 Credits "		},
	{0x0a, 0x01, 0x38, 0x10, "1 Coin 4 Credits "		},
	{0x0a, 0x01, 0x38, 0x08, "1 Coin 5 Credits "		},
	{0x0a, 0x01, 0x38, 0x00, "1 Coin 6 Credits "		},

	{0   , 0xfe, 0   ,    2, "Discount To Continue"		},
	{0x0a, 0x01, 0x40, 0x40, "Off"		},
	{0x0a, 0x01, 0x40, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x0a, 0x01, 0x80, 0x80, "Off"		},
	{0x0a, 0x01, 0x80, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Vert. Flip Screen"		},
	{0x0b, 0x01, 0x01, 0x01, "Off"		},
	{0x0b, 0x01, 0x01, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Horiz. Flip Screen"		},
	{0x0b, 0x01, 0x02, 0x02, "Off"		},
	{0x0b, 0x01, 0x02, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x0b, 0x01, 0x04, 0x00, "Off"		},
	{0x0b, 0x01, 0x04, 0x04, "On"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x0b, 0x01, 0x18, 0x10, "Easy"		},
	{0x0b, 0x01, 0x18, 0x18, "Normal"		},
	{0x0b, 0x01, 0x18, 0x08, "Hard"		},
	{0x0b, 0x01, 0x18, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Blood Color"		},
	{0x0b, 0x01, 0x20, 0x20, "Red"		},
	{0x0b, 0x01, 0x20, 0x00, "Yellow"		},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x0b, 0x01, 0x40, 0x40, "2"		},
	{0x0b, 0x01, 0x40, 0x00, "3"		},

	{0   , 0xfe, 0   ,    2, "Gun Type"		},
	{0x0b, 0x01, 0x80, 0x80, "Pump Action"		},
	{0x0b, 0x01, 0x80, 0x00, "Hand Gun"		},
};

STDDIPINFO(Turkhunt)

static struct BurnInputInfo WschampInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 start"},

	A("P1 Right / left",	BIT_ANALOG_REL, DrvAxis + 0,	"mouse x-axis"),
	A("P1 Up / Down",	BIT_ANALOG_REL, DrvAxis + 1,	"mouse y-axis"),

	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"mouse button 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"mouse button 2"},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy3 + 6,	"p1 fire 3"},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 4"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 7,	"p2 start"},

	A("P2 Right / left",	BIT_ANALOG_REL, DrvAxis + 2,	"p2 x-axis"),
	A("P2 Up / Down",	BIT_ANALOG_REL, DrvAxis + 3,	"p2 y-axis"),

	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 fire 2"},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 3"},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 4"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy4 + 2,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvInput + 6,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvInput + 7,	"dip"},
};

STDINPUTINFO(Wschamp)

static struct BurnDIPInfo WschampDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL		},
	{0x13, 0xff, 0xff, 0x7f, NULL		},

	{0   , 0xfe, 0   ,    16, "Coin A"		},
	{0x12, 0x01, 0x0f, 0x09, "4 Coins Start, 4 Coins Continue"		},
	{0x12, 0x01, 0x0f, 0x08, "4 Coins Start, 3 Coins Continue"		},
	{0x12, 0x01, 0x0f, 0x07, "4 Coins Start, 2 Coins Continue"		},
	{0x12, 0x01, 0x0f, 0x06, "4 Coins Start, 1 Coin Continue"		},
	{0x12, 0x01, 0x0f, 0x0c, "3 Coins Start, 3 Coins Continue"		},
	{0x12, 0x01, 0x0f, 0x0b, "3 Coins Start, 2 Coins Continue"		},
	{0x12, 0x01, 0x0f, 0x0a, "3 Coins Start, 1 Coin Continue"		},
	{0x12, 0x01, 0x0f, 0x0e, "2 Coins Start, 2 Coins Continue"		},
	{0x12, 0x01, 0x0f, 0x0d, "2 Coins Start, 1 Coin Continue"		},
	{0x12, 0x01, 0x0f, 0x0f, "1 Coin Start, 1 Coin Continue"		},
	{0x12, 0x01, 0x0f, 0x05, "1 Coin 2 Credits, 1 Credit Start & Continue"		},
	{0x12, 0x01, 0x0f, 0x04, "1 Coin 3 Credits, 1 Credit Start & Continue"		},
	{0x12, 0x01, 0x0f, 0x03, "1 Coin 4 Credits, 1 Credit Start & Continue"		},
	{0x12, 0x01, 0x0f, 0x02, "1 Coin 5 Credits, 1 Credit Start & Continue"		},
	{0x12, 0x01, 0x0f, 0x01, "1 Coin 6 Credits, 1 Credit Start & Continue"		},
	{0x12, 0x01, 0x0f, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x12, 0x01, 0x80, 0x80, "Off"		},
	{0x12, 0x01, 0x80, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Vert. Flip Screen"		},
	{0x13, 0x01, 0x01, 0x01, "Off"		},
	{0x13, 0x01, 0x01, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Horiz. Flip Screen"		},
	{0x13, 0x01, 0x02, 0x02, "Off"		},
	{0x13, 0x01, 0x02, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x04, 0x00, "Off"		},
	{0x13, 0x01, 0x04, 0x04, "On"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x18, 0x10, "Easy"		},
	{0x13, 0x01, 0x18, 0x18, "Normal"		},
	{0x13, 0x01, 0x18, 0x08, "Hard"		},
	{0x13, 0x01, 0x18, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x13, 0x01, 0x40, 0x40, "2"		},
	{0x13, 0x01, 0x40, 0x00, "3"		},

	{0   , 0xfe, 0   ,    2, "Gun Type"		},
	{0x13, 0x01, 0x80, 0x80, "Pump Action"		},
	{0x13, 0x01, 0x80, 0x00, "Hand Gun"		},
};

STDDIPINFO(Wschamp)

static struct BurnDIPInfo TrophyhDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL		},
	{0x13, 0xff, 0xff, 0x7f, NULL		},

	{0   , 0xfe, 0   ,    16, "Coin A"		},
	{0x12, 0x01, 0x0f, 0x09, "4 Coins Start, 4 Coins Continue"		},
	{0x12, 0x01, 0x0f, 0x08, "4 Coins Start, 3 Coins Continue"		},
	{0x12, 0x01, 0x0f, 0x07, "4 Coins Start, 2 Coins Continue"		},
	{0x12, 0x01, 0x0f, 0x06, "4 Coins Start, 1 Coin Continue"		},
	{0x12, 0x01, 0x0f, 0x0c, "3 Coins Start, 3 Coins Continue"		},
	{0x12, 0x01, 0x0f, 0x0b, "3 Coins Start, 2 Coins Continue"		},
	{0x12, 0x01, 0x0f, 0x0a, "3 Coins Start, 1 Coin Continue"		},
	{0x12, 0x01, 0x0f, 0x0e, "2 Coins Start, 2 Coins Continue"		},
	{0x12, 0x01, 0x0f, 0x0d, "2 Coins Start, 1 Coin Continue"		},
	{0x12, 0x01, 0x0f, 0x0f, "1 Coin Start, 1 Coin Continue"		},
	{0x12, 0x01, 0x0f, 0x05, "1 Coin 2 Credits, 1 Credit Start & Continue"		},
	{0x12, 0x01, 0x0f, 0x04, "1 Coin 3 Credits, 1 Credit Start & Continue"		},
	{0x12, 0x01, 0x0f, 0x03, "1 Coin 4 Credits, 1 Credit Start & Continue"		},
	{0x12, 0x01, 0x0f, 0x02, "1 Coin 5 Credits, 1 Credit Start & Continue"		},
	{0x12, 0x01, 0x0f, 0x01, "1 Coin 6 Credits, 1 Credit Start & Continue"		},
	{0x12, 0x01, 0x0f, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x12, 0x01, 0x80, 0x80, "Off"		},
	{0x12, 0x01, 0x80, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Vert. Flip Screen"		},
	{0x13, 0x01, 0x01, 0x01, "Off"		},
	{0x13, 0x01, 0x01, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Horiz. Flip Screen"		},
	{0x13, 0x01, 0x02, 0x02, "Off"		},
	{0x13, 0x01, 0x02, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x04, 0x00, "Off"		},
	{0x13, 0x01, 0x04, 0x04, "On"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x18, 0x10, "Easy"		},
	{0x13, 0x01, 0x18, 0x18, "Normal"		},
	{0x13, 0x01, 0x18, 0x08, "Hard"		},
	{0x13, 0x01, 0x18, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Blood Color"		},
	{0x13, 0x01, 0x20, 0x20, "Red"		},
	{0x13, 0x01, 0x20, 0x00, "Yellow"		},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x13, 0x01, 0x40, 0x40, "2"		},
	{0x13, 0x01, 0x40, 0x00, "3"		},

	{0   , 0xfe, 0   ,    2, "Gun Type"		},
	{0x13, 0x01, 0x80, 0x80, "Pump Action"		},
	{0x13, 0x01, 0x80, 0x00, "Hand Gun"		},
};

STDDIPINFO(Trophyh)

// Rom information

static struct BurnRomInfo grdiansRomDesc[] = {
	{ "u2.bin",		  0x080000, 0x36adc6f2, BRF_ESS | BRF_PRG },	// 68000 code
	{ "u3.bin",		  0x080000, 0x2704f416, BRF_ESS | BRF_PRG },
	{ "u4.bin",		  0x080000, 0xbb52447b, BRF_ESS | BRF_PRG },
	{ "u5.bin",		  0x080000, 0x9c164a3b, BRF_ESS | BRF_PRG },

	{ "u16.bin",	  0x400000, 0x6a65f265,	BRF_GRA },				// GFX
	{ "u20.bin",	  0x400000, 0xa7226ab7,	BRF_GRA },
	{ "u15.bin",	  0x400000, 0x01672dcd,	BRF_GRA },
	{ "u19.bin",	  0x400000, 0xc0c998a0,	BRF_GRA },
	{ "u18.bin",	  0x400000, 0x967babf4,	BRF_GRA },
	{ "u22.bin",	  0x400000, 0x6239997a,	BRF_GRA },
	{ "u17.bin",	  0x400000, 0x0fad0629,	BRF_GRA },
	{ "u21.bin",	  0x400000, 0x6f95e466,	BRF_GRA },

	{ "u32.bin",      0x100000, 0xcf0f3017, BRF_SND },				// PCM
};

STD_ROM_PICK(grdians)
STD_ROM_FN(grdians)

static struct BurnRomInfo grdiansaRomDesc[] = {
	{ "ka2_001_001.u2",		  0x080000, 0x36adc6f2, BRF_ESS | BRF_PRG },	// 68000 code
	{ "ka2_001_004.u3",		  0x080000, 0x2704f416, BRF_ESS | BRF_PRG },
	{ "ka2_001_002.u4",		  0x080000, 0xbb52447b, BRF_ESS | BRF_PRG },
	{ "ka2_001_003.u5",		  0x080000, 0x9c164a3b, BRF_ESS | BRF_PRG },

	{ "ka2-001-010.u18",	  0x200000, 0xb3e6e95f,	BRF_GRA },				// GFX
	{ "ka2-001-011.u20",	  0x200000, 0x676edca6,	BRF_GRA },
	{ "ka2-001-013.u17",	  0x200000, 0x9f7feb13,	BRF_GRA },
	{ "ka2-001-014.u19",	  0x200000, 0x5465ef1b,	BRF_GRA },
	{ "ka2-001-007.u22",	  0x200000, 0xd1035051,	BRF_GRA },
	{ "ka2-001-008.u23",	  0x200000, 0xb2c94f31,	BRF_GRA },
	{ "ka2-001-016.u1",		  0x200000, 0x99fc8efa,	BRF_GRA },
	{ "ka2-001-017.u2",		  0x200000, 0x60ad7a2b,	BRF_GRA },

	{ "ka2-001-015.u28",      0x200000, 0xfa97cc54, BRF_SND },				// PCM
};

STD_ROM_PICK(grdiansa)
STD_ROM_FN(grdiansa)

// bootleg PCB based on the P-FG01-1 PCB, still has the X1-010, DX-101 and DX-102 customs. Pressing start in-game changes character.
static struct BurnRomInfo grdiansblRomDesc[] = {
	{ "p1.u4",		0x200000, 0x4ba24d02, BRF_ESS | BRF_PRG },     // 68000 code

	{ "u16.u16",	0x400000, 0xd24e007f, BRF_GRA },               // GFX
	{ "u15.u15",	0x400000, 0x2a92b8de, BRF_GRA },
	{ "u18.u18",	0x400000, 0xa3d0ba96, BRF_GRA },
	{ "u17.u17",	0x400000, 0x020ee44f, BRF_GRA },

	{ "u32.u32",	0x200000, 0xfa97cc54, BRF_SND },               // PCM

	{ "ke-001.u38",	0x000117, 0x00000000, BRF_OPT | BRF_NODUMP },  // PLDs
};

STD_ROM_PICK(grdiansbl)
STD_ROM_FN(grdiansbl)

static struct BurnRomInfo mj4simaiRomDesc[] = {
	{ "ll.u2",		  0x080000, 0x7be9c781, BRF_ESS | BRF_PRG },	// 68000 code
	{ "lh1.u3",		  0x080000, 0x82aa3f72, BRF_ESS | BRF_PRG },
	{ "hl.u4",		  0x080000, 0x226063b7, BRF_ESS | BRF_PRG },
	{ "hh.u5",		  0x080000, 0x23aaf8df, BRF_ESS | BRF_PRG },

	{ "cha-03.u16",	  0x400000, 0xd367429a,	BRF_GRA },				// GFX
	{ "cha-04.u18",	  0x400000, 0x7f2008c3,	BRF_GRA },
	{ "cha-05.u15",	  0x400000, 0xe94ec40a,	BRF_GRA },
	{ "cha-06.u17",	  0x400000, 0x5cb0b3a9,	BRF_GRA },
	{ "cha-01.u21",	  0x400000, 0x35f47b37,	BRF_GRA },
	{ "cha-02.u22",	  0x400000, 0xf6346860,	BRF_GRA },

	{ "cha-07.u32",	  0x400000, 0x817519ee, BRF_SND },				// PCM
};

STD_ROM_PICK(mj4simai)
STD_ROM_FN(mj4simai)

static struct BurnRomInfo myangelRomDesc[] = {
	{ "kq1-prge.u2",  0x080000, 0x6137d4c0, BRF_ESS | BRF_PRG },	// 68000 code
	{ "kq1-prgo.u3",  0x080000, 0x4aad10d8, BRF_ESS | BRF_PRG },
	{ "kq1-tble.u4",  0x080000, 0xe332a514, BRF_ESS | BRF_PRG },
	{ "kq1-tblo.u5",  0x080000, 0x760cab15, BRF_ESS | BRF_PRG },

	{ "kq1-cg2.u20",  0x200000, 0x80b4e8de,	BRF_GRA },				// GFX
	{ "kq1-cg0.u16",  0x200000, 0xf8ae9a05,	BRF_GRA },
	{ "kq1-cg3.u19",  0x200000, 0x9bdc35c9,	BRF_GRA },
	{ "kq1-cg1.u15",  0x200000, 0x23bd7ea4,	BRF_GRA },
	{ "kq1-cg6.u22",  0x200000, 0xb25acf12,	BRF_GRA },
	{ "kq1-cg4.u18",  0x200000, 0xdca7f8f2,	BRF_GRA },
	{ "kq1-cg7.u21",  0x200000, 0x9f48382c,	BRF_GRA },
	{ "kq1-cg5.u17",  0x200000, 0xa4bc4516,	BRF_GRA },

	{ "kq1-snd.u32",  0x200000, 0x8ca1b449, BRF_SND },				// PCM
};

STD_ROM_PICK(myangel)
STD_ROM_FN(myangel)

static struct BurnRomInfo myangel2RomDesc[] = {
	{ "kqs1ezpr.u2",  0x080000, 0x2469aac2, BRF_ESS | BRF_PRG },	// 68000 code
	{ "kqs1ozpr.u3",  0x080000, 0x6336375c, BRF_ESS | BRF_PRG },
	{ "kqs1e-tb.u4",  0x080000, 0xe759b4cc, BRF_ESS | BRF_PRG },
	{ "kqs1o-tb.u5",  0x080000, 0xb6168737, BRF_ESS | BRF_PRG },

	{ "kqs1-cg4.u20", 0x200000, 0xd1802241,	BRF_GRA },				// GFX
	{ "kqs1-cg0.u16", 0x400000, 0xc21a33a7,	BRF_GRA },
	{ "kqs1-cg5.u19", 0x200000, 0xd86cf19c,	BRF_GRA },
	{ "kqs1-cg1.u15", 0x400000, 0xdca799ba,	BRF_GRA },
	{ "kqs1-cg6.u22", 0x200000, 0x3f08886b,	BRF_GRA },
	{ "kqs1-cg2.u18", 0x400000, 0xf7f92c7e,	BRF_GRA },
	{ "kqs1-cg7.u21", 0x200000, 0x2c977904,	BRF_GRA },
	{ "kqs1-cg3.u17", 0x400000, 0xde3b2191,	BRF_GRA },

	{ "kqs1-snd.u32", 0x400000, 0x792a6b49, BRF_SND },				// PCM
};

STD_ROM_PICK(myangel2)
STD_ROM_FN(myangel2)

static struct BurnRomInfo pzlbowlRomDesc[] = {
	{ "kup_u06_i03.u6",  0x080000, 0x314e03ac, BRF_ESS | BRF_PRG },	// 68000 code
	{ "kup_u07_i03.u7",  0x080000, 0xa0423a04, BRF_ESS | BRF_PRG },

	{ "kuc-u38-i00.u38", 0x400000, 0x3db24172,	BRF_GRA },				// GFX
	{ "kuc-u39-i00.u39", 0x400000, 0x9b26619b,	BRF_GRA },
	{ "kuc-u40-i00.u40", 0x400000, 0x7e49a2cf,	BRF_GRA },
	{ "kuc-u41-i00.u41", 0x400000, 0x2febf19b,	BRF_GRA },

	{ "kus-u18-i00.u18", 0x400000, 0xe2b1dfcf, BRF_SND },				// PCM
};

STD_ROM_PICK(pzlbowl)
STD_ROM_FN(pzlbowl)

static struct BurnRomInfo ablastRomDesc[] = {
	// Genuine P0-142A PCB
	{ "a-blast_twn_u06.u06",	  0x080000, 0xe62156d7, BRF_ESS | BRF_PRG },	// 68000 code
	{ "a-blast_twn_u07.u07",	  0x080000, 0xd4ddc16b, BRF_ESS | BRF_PRG },

	{ "a-blast_twn_u38.u38",	  0x400000, 0x090923da,	BRF_GRA },				// GFX
	{ "a-blast_twn_u39.u39",	  0x400000, 0x6bb17d83,	BRF_GRA },
	{ "a-blast_twn_u40.u40",	  0x400000, 0xdb94847d,	BRF_GRA },

	{ "a-blast_twn_u18.u18",	  0x200000, 0xde4e65e2, BRF_SND },				// PCM
};

STD_ROM_PICK(ablast)
STD_ROM_FN(ablast)

static struct BurnRomInfo penbrosRomDesc[] = {
	// Genuine P0-142A PCB - Original or hack?
	{ "a-blast_jpn_u06.u06",	  0x080000, 0x7bbdffac, BRF_ESS | BRF_PRG },	// 68000 code
	{ "a-blast_jpn_u07.u07",	  0x080000, 0xd50cda5f, BRF_ESS | BRF_PRG },

	{ "a-blast_jpn_u38.u38",	  0x400000, 0x4247b39e,	BRF_GRA },				// GFX
	{ "a-blast_jpn_u39.u39",	  0x400000, 0xf9f07faf,	BRF_GRA },
	{ "a-blast_jpn_u40.u40",	  0x400000, 0xdc9e0a96,	BRF_GRA },

	{ "a-blast_jpn_u18.u18",	  0x200000, 0xde4e65e2, BRF_SND },				// PCM
};

STD_ROM_PICK(penbros)
STD_ROM_FN(penbros)

static struct BurnRomInfo gundamexRomDesc[] = {
	{ "ka_002_002.u2",	0x080000, 0xe850f6d8, BRF_ESS | BRF_PRG }, //  0 68000 code
	{ "ka_002_004.u3",	0x080000, 0xc0fb1208, BRF_ESS | BRF_PRG }, //  1
	{ "ka_002_001.u4",	0x080000, 0x553ebe6b, BRF_ESS | BRF_PRG }, //  2
	{ "ka_002_003.u5",	0x080000, 0x946185aa, BRF_ESS | BRF_PRG }, //  3
	{ "ka-001-005.u77",	0x080000, 0xf01d3d00, BRF_ESS | BRF_PRG }, //  4

	{ "ka-001-009.u16",	0x200000, 0x997d8d93, BRF_GRA }, //  5 GFX
	{ "ka-001-010.u18",	0x200000, 0x811b67ca, BRF_GRA }, //  6
	{ "ka-001-011.u20",	0x200000, 0x08a72700, BRF_GRA }, //  7
	{ "ka-001-012.u15",	0x200000, 0xb789e4a8, BRF_GRA }, //  8
	{ "ka-001-013.u17",	0x200000, 0xd8a0201f, BRF_GRA }, //  9
	{ "ka-001-014.u19",	0x200000, 0x7635e026, BRF_GRA }, // 10
	{ "ka-001-006.u21",	0x200000, 0x6aac2f2f, BRF_GRA }, // 11
	{ "ka-001-007.u22",	0x200000, 0x588f9d63, BRF_GRA }, // 12
	{ "ka-001-008.u23",	0x200000, 0xdb55a60a, BRF_GRA }, // 13

	{ "ka-001-015.u28",	0x200000, 0xada2843b, BRF_SND }, // 14 PCM
	
	{ "eeprom.bin",         0x000080, 0x80f8e248, BRF_OPT },
};

STD_ROM_PICK(gundamex)
STD_ROM_FN(gundamex)

static struct BurnRomInfo deerhuntRomDesc[] = {
	{ "as0906_e05_u6_694e.u06",	0x100000, 0x20c81f17, 1 }, //  0 68000 code
	{ "as0907_e05_u7_5d89.u07",	0x100000, 0x1731aa2a, 1 }, //  1

	{ "as0901m01.u38",			0x800000, 0x1d6acf8f, 2 }, //  2 GFX
	{ "as0902m01.u39",			0x800000, 0xc7ca2128, 2 }, //  3
	{ "as0903m01.u40",			0x800000, 0xe8ef81b3, 2 }, //  4
	{ "as0904m01.u41",			0x800000, 0xd0f97fdc, 2 }, //  5

	{ "as0905m01.u18",			0x400000, 0x8d8165bb, 3 }, //  6 PCM
};

STD_ROM_PICK(deerhunt)
STD_ROM_FN(deerhunt)

static struct BurnRomInfo deerhunaRomDesc[] = {
	{ "as0906_e04_u6_6640.u06",	0x100000, 0xbb3af36f, 1 }, //  0 68000 code
	{ "as0907_e04_u7_595a.u07",	0x100000, 0x83f02117, 1 }, //  1

	{ "as0901m01.u38",			0x800000, 0x1d6acf8f, 2 }, //  2 GFX
	{ "as0902m01.u39",			0x800000, 0xc7ca2128, 2 }, //  3
	{ "as0903m01.u40",			0x800000, 0xe8ef81b3, 2 }, //  4
	{ "as0904m01.u41",			0x800000, 0xd0f97fdc, 2 }, //  5

	{ "as0905m01.u18",			0x400000, 0x8d8165bb, 3 }, //  6 PCM
};

STD_ROM_PICK(deerhuna)
STD_ROM_FN(deerhuna)

static struct BurnRomInfo deerhunbRomDesc[] = {
	{ "as_0906_e04.u06",	0x100000, 0x07d9b64a, 1 }, //  0 68000 code
	{ "as_0907_e04.u07",	0x100000, 0x19973d08, 1 }, //  1

	{ "as0901m01.u38",		0x800000, 0x1d6acf8f, 2 }, //  2 GFX
	{ "as0902m01.u39",		0x800000, 0xc7ca2128, 2 }, //  3
	{ "as0903m01.u40",		0x800000, 0xe8ef81b3, 2 }, //  4
	{ "as0904m01.u41",		0x800000, 0xd0f97fdc, 2 }, //  5

	{ "as0905m01.u18",		0x400000, 0x8d8165bb, 3 }, //  6 PCM
};

STD_ROM_PICK(deerhunb)
STD_ROM_FN(deerhunb)

static struct BurnRomInfo deerhuncRomDesc[] = {
	{ "as_0937_e01.u06",	0x100000, 0x8d74088e, 1 }, //  0 68000 code
	{ "as_0938_e01.u07",	0x100000, 0xc7657889, 1 }, //  1

	{ "as0901m01.u38",		0x800000, 0x1d6acf8f, 2 }, //  2 GFX
	{ "as0902m01.u39",		0x800000, 0xc7ca2128, 2 }, //  3
	{ "as0903m01.u40",		0x800000, 0xe8ef81b3, 2 }, //  4
	{ "as0904m01.u41",		0x800000, 0xd0f97fdc, 2 }, //  5

	{ "as0905m01.u18",		0x400000, 0x8d8165bb, 3 }, //  6 PCM
};

STD_ROM_PICK(deerhunc)
STD_ROM_FN(deerhunc)

static struct BurnRomInfo deerhundRomDesc[] = {
	{ "as_0906_e02.u06",	0x100000, 0x190cca42, 1 }, //  0 68000 code
	{ "as_0907_e02.u07",	0x100000, 0x9de2b901, 1 }, //  1

	{ "as0901m01.u38",		0x800000, 0x1d6acf8f, 2 }, //  2 GFX
	{ "as0902m01.u39",		0x800000, 0xc7ca2128, 2 }, //  3
	{ "as0903m01.u40",		0x800000, 0xe8ef81b3, 2 }, //  4
	{ "as0904m01.u41",		0x800000, 0xd0f97fdc, 2 }, //  5

	{ "as0905m01.u18",		0x400000, 0x8d8165bb, 3 }, //  6 PCM
};

STD_ROM_PICK(deerhund)
STD_ROM_FN(deerhund)

static struct BurnRomInfo deerhuneRomDesc[] = {
	{ "as_0906_e01.u06",	0x100000, 0x103e3ba3, 1 }, //  0 68000 code
	{ "as_0907_e01.u07",	0x100000, 0xddeb0f97, 1 }, //  1

	{ "as0901m01.u38",		0x800000, 0x1d6acf8f, 2 }, //  2 GFX
	{ "as0902m01.u39",		0x800000, 0xc7ca2128, 2 }, //  3
	{ "as0903m01.u40",		0x800000, 0xe8ef81b3, 2 }, //  4
	{ "as0904m01.u41",		0x800000, 0xd0f97fdc, 2 }, //  5

	{ "as0905m01.u18",		0x400000, 0x8d8165bb, 3 }, //  6 PCM
};

STD_ROM_PICK(deerhune)
STD_ROM_FN(deerhune)

static struct BurnRomInfo deerhunjRomDesc[] = {
	{ "as0_908e01_u6_jdh.u06",	0x100000, 0x52f037da, 1 }, //  0 68000 code
	{ "as0_909e01_u7_jdh.u07",	0x100000, 0xb391bc87, 1 }, //  1

	{ "as0901m01.u38",		0x800000, 0x1d6acf8f, 2 }, //  2 GFX
	{ "as0902m01.u39",		0x800000, 0xc7ca2128, 2 }, //  3
	{ "as0903m01.u40",		0x800000, 0xe8ef81b3, 2 }, //  4
	{ "as0904m01.u41",		0x800000, 0xd0f97fdc, 2 }, //  5

	{ "as0905m01.u18",		0x400000, 0x8d8165bb, 3 }, //  6 PCM
};

STD_ROM_PICK(deerhunj)
STD_ROM_FN(deerhunj)

static struct BurnRomInfo turkhuntRomDesc[] = {
	{ "asx_906e01_th.u06",	0x100000, 0xc96266e1, 1 }, //  0 68000 code
	{ "asx_907e01_th.u07",	0x100000, 0x7c67b502, 1 }, //  1

	{ "asx901m01.u38",		0x800000, 0xeabd3f44, 2 }, //  2 GFX
	{ "asx902m01.u39",		0x800000, 0xc32130c8, 2 }, //  3
	{ "asx903m01.u40",		0x800000, 0x5f86c322, 2 }, //  4
	{ "asx904m01.u41",		0x800000, 0xc77e0b66, 2 }, //  5

	{ "asx905m01.u18",		0x400000, 0x8d9dd9a9, 3 }, //  6 PCM
};

STD_ROM_PICK(turkhunt)
STD_ROM_FN(turkhunt)

static struct BurnRomInfo wschampRomDesc[] = {
	{ "as_1006_e03.u06",	0x100000, 0x0ad01677, 1 }, //  0 68000 code
	{ "as_1007_e03.u07",	0x100000, 0x572624f0, 1 }, //  1

	{ "as1001m01.u38",		0x800000, 0x92595579, 2 }, //  2 GFX
	{ "as1002m01.u39",		0x800000, 0x16c2bb08, 2 }, //  3
	{ "as1003m01.u40",		0x800000, 0x89618858, 2 }, //  4
	{ "as1004m01.u41",		0x800000, 0x500c0909, 2 }, //  5

	{ "as1005m01.u18",		0x400000, 0xe4b137b8, 3 }, //  6 PCM
};

STD_ROM_PICK(wschamp)
STD_ROM_FN(wschamp)

static struct BurnRomInfo wschampaRomDesc[] = {
	{ "as_1006_e02.u06",	0x100000, 0xd3d3b2b5, 1 }, //  0 68000 code
	{ "as_1007_e02.u07",	0x100000, 0x78ede6d9, 1 }, //  1

	{ "as1001m01.u38",		0x800000, 0x92595579, 2 }, //  2 GFX
	{ "as1002m01.u39",		0x800000, 0x16c2bb08, 2 }, //  3
	{ "as1003m01.u40",		0x800000, 0x89618858, 2 }, //  4
	{ "as1004m01.u41",		0x800000, 0x500c0909, 2 }, //  5

	{ "as1005m01.u18",		0x400000, 0xe4b137b8, 3 }, //  6 PCM
};

STD_ROM_PICK(wschampa)
STD_ROM_FN(wschampa)

static struct BurnRomInfo wschampbRomDesc[] = {
	{ "as10u6.u06",		0x100000, 0x70a18bef, 1 }, //  0 68000 code
	{ "as10u7.u07",		0x100000, 0xcf23be7d, 1 }, //  1

	{ "as1001m01.u38",	0x800000, 0x92595579, 2 }, //  2 GFX
	{ "as1002m01.u39",	0x800000, 0x16c2bb08, 2 }, //  3
	{ "as1003m01.u40",	0x800000, 0x89618858, 2 }, //  4
	{ "as1004m01.u41",	0x800000, 0x500c0909, 2 }, //  5

	{ "as1005m01.u18",	0x400000, 0xe4b137b8, 3 }, //  6 PCM
};

STD_ROM_PICK(wschampb)
STD_ROM_FN(wschampb)

static struct BurnRomInfo trophyhRomDesc[] = {
	{ "as_1106_e01.u06",	0x100000, 0xb4950882, 1 }, //  0 68000 code
	{ "as_1107_e01.u07",	0x100000, 0x19ee67cb, 1 }, //  1

	{ "as1101m01.u38",		0x800000, 0x855ed675, 2 }, //  2 GFX
	{ "as1102m01.u39",		0x800000, 0xd186d271, 2 }, //  3
	{ "as1103m01.u40",		0x800000, 0xadf8a54e, 2 }, //  4
	{ "as1104m01.u41",		0x800000, 0x387882e9, 2 }, //  5

	{ "as1105m01.u18",		0x400000, 0x633d0df8, 3 }, //  6 PCM
};

STD_ROM_PICK(trophyh)
STD_ROM_FN(trophyh)

static struct BurnRomInfo funcube2RomDesc[] = {
	{ "fc21_prg-0b.u3",	0x080000, 0xadd1c8a6, 1 }, //  0 68000 code

	{ "fc21_iopr-0.u49",	0x020000, 0x314555ef, 2 }, //  1 H8/3007 code

	{ "fc21a.u57",		0x000300, 0x00000000, 3 | BRF_NODUMP }, //  2 PIC12C508 Code

	{ "fc21_obj-0.u43",	0x400000, 0x08cfe6d9, 4 }, //  3 GFX
	{ "fc21_obj-1.u42",	0x400000, 0x4c1fbc20, 4 }, //  4

	{ "fc21_voi0.u47",	0x200000, 0x4a49370a, 5 }, //  5 PCM
};

STD_ROM_PICK(funcube2)
STD_ROM_FN(funcube2)

static struct BurnRomInfo funcube4RomDesc[] = {
	{ "fc41_prg-0.u3",	0x080000, 0xef870874, 1 }, //  0 68000 code

	{ "fc21_iopr-0.u49",	0x020000, 0x314555ef, 2 }, //  1 H8/3007 code

	{ "fc41a",		0x000300, 0x00000000, 3 | BRF_NODUMP }, //  2 PIC12C508 Code

	{ "fc41_obj-0.u43",	0x400000, 0x9ff029d5, 4 }, //  3 GFX
	{ "fc41_obj-1.u42",	0x400000, 0x5ab7b087, 4 }, //  4

	{ "fc41_snd0.u47",	0x200000, 0xe6f7d2bc, 5 }, //  5 PCM
};

STD_ROM_PICK(funcube4)
STD_ROM_FN(funcube4)


inline static UINT32 CalcCol(UINT16 nColour)
{
	INT32 r, g, b;

	r = (nColour & 0x7c00) >> 7;	// Red
	r |= r >> 5;
	g = (nColour & 0x03E0) >> 2;  // Green
	g |= g >> 5;
	b = (nColour & 0x001f) << 3;	// Blue
	b |= b >> 5;

	return BurnHighCol(r, g, b, 0);
}

// ---- Toshiba TMP68301  ---------------------------------------------

static INT32 tmp68301_timer[3] = {0, 0, 0};
static INT32 tmp68301_timer_counter[3] = {0, 0, 0};
static INT32 tmp68301_irq_vector[8] = {0, 0, 0, 0, 0, 0, 0, 0};

static void tmp68301_update_timer( INT32 i )
{
	UINT16 TCR	=	BURN_ENDIAN_SWAP_INT16(RamTMP68301[(0x200 + i * 0x20)/2]);
	UINT16 MAX1	=	BURN_ENDIAN_SWAP_INT16(RamTMP68301[(0x204 + i * 0x20)/2]);
	UINT16 MAX2	=	BURN_ENDIAN_SWAP_INT16(RamTMP68301[(0x206 + i * 0x20)/2]);

	INT32 max = 0;
	double duration = 0;

//	timer_adjust(tmp68301_timer[i],TIME_NEVER,i,0);
	tmp68301_timer[i] = 0;
	tmp68301_timer_counter[i] = 0;
	//bprintf(PRINT_NORMAL, _T("Tmp68301: update timer %d. TCR: %04x MAX: %04x %04x\n"), i, TCR, MAX1, MAX2);

	// timers 1&2 only
	switch( (TCR & 0x0030)>>4 )	{					// MR2..1
	case 1:	max = MAX1;	break;
	case 2:	max = MAX2;	break;
	}

	switch ( (TCR & 0xc000)>>14 ) {					// CK2..1
	case 0:	// System clock (CLK)
		if (max) {
			INT32 scale = (TCR & 0x3c00)>>10;			// P4..1
			if (scale > 8) scale = 8;
			duration = M68K_CYCS;					//Machine->drv->cpu[0].cpu_clock;
			duration /= 1 << scale;
			duration /= max;
		}
		break;
	}

//  logerror("CPU #0 PC %06X: TMP68301 Timer %d, duration %lf, max %04X\n",activecpu_get_pc(),i,duration,max);
//	bprintf(PRINT_NORMAL, _T("TMP68301 Timer %d, duration %lf, max %04X TCR %04X\n"),i,duration,max,TCR);

	if (!(TCR & 0x0002))				// CS
	{
		if (duration) {
			// timer_adjust(tmp68301_timer[i],TIME_IN_HZ(duration),i,0);
			// active tmp68301 timer i, and set duration
			tmp68301_timer[i] = (INT32) (M68K_CYCS / duration);
			//tmp68301_timer_counter[i] = 0;
			//bprintf(PRINT_NORMAL, _T("Tmp68301: update timer #%d duration to %d (%8.3f)\n"), i, tmp68301_timer[i], duration);
		} else {
			//logerror("CPU #0 PC %06X: TMP68301 error, timer %d duration is 0\n",activecpu_get_pc(),i);
			//bprintf(PRINT_ERROR, _T("Tmp68301: error timer %d duration is 0\n"), i, TCR, MAX1, MAX2);
		}
	}
}

static void tmp68301_timer_callback(INT32 i)
{
	UINT16 TCR	= BURN_ENDIAN_SWAP_INT16(RamTMP68301[(0x200 + i * 0x20)/2]);
	UINT16 IMR	= BURN_ENDIAN_SWAP_INT16(RamTMP68301[0x94/2]);		// Interrupt Mask Register (IMR)
	UINT16 ICR	= BURN_ENDIAN_SWAP_INT16(RamTMP68301[0x8e/2+i]);	// Interrupt Controller Register (ICR7..9)
	UINT16 IVNR	= BURN_ENDIAN_SWAP_INT16(RamTMP68301[0x9a/2]);		// Interrupt Vector Number Register (IVNR)

//  logerror("CPU #0 PC %06X: callback timer %04X, j = %d\n",activecpu_get_pc(),i,tcount);
//	bprintf(PRINT_NORMAL, _T("Tmp68301: timer[%d] TCR: %04x IMR: %04x\n"), i, TCR, IMR);

	if	( (TCR & 0x0004) &&	!(IMR & (0x100<<i))	) {
		INT32 level = ICR & 0x0007;
		// Interrupt Vector Number Register (IVNR)
		tmp68301_irq_vector[level]	=	IVNR & 0x00e0;
		tmp68301_irq_vector[level]	+=	4+i;

		//cpunum_set_input_line(0,level,HOLD_LINE);
		//bprintf(PRINT_NORMAL, _T("Tmp68301: CB IRQ[%x] %04x  timer[%d]\n"), level, tmp68301_irq_vector[level], i);
		//SekSetIRQLine(tmp68301_irq_vector[level], CPU_IRQSTATUS_AUTO);

		//SekSetIRQLine(level, CPU_IRQSTATUS_ACK);
		SekSetIRQLine(level, CPU_IRQSTATUS_AUTO);
	}

	if (TCR & 0x0080) {	// N/1
		// Repeat
		tmp68301_update_timer(i);
	} else {
		// One Shot
	}
}

static void tmp68301_update_irq_state(INT32 i)
{
	/* Take care of external interrupts */
	UINT16 IMR	= BURN_ENDIAN_SWAP_INT16(RamTMP68301[0x94/2]);		// Interrupt Mask Register (IMR)
	UINT16 IVNR	= BURN_ENDIAN_SWAP_INT16(RamTMP68301[0x9a/2]);		// Interrupt Vector Number Register (IVNR)

	if	( !(IMR & (1<<i)) )	{
		UINT16 ICR = BURN_ENDIAN_SWAP_INT16(RamTMP68301[0x80/2+i]);	// Interrupt Controller Register (ICR0..2)

		// Interrupt Controller Register (ICR0..2)
		INT32 level = ICR & 0x0007;

		// Interrupt Vector Number Register (IVNR)
		tmp68301_irq_vector[level]	=	IVNR & 0x00e0;
		tmp68301_irq_vector[level]	+=	i;

		//tmp68301_IE[i] = 0;		// Interrupts are edge triggerred
		//cpunum_set_input_line(0,level,HOLD_LINE);
		//bprintf(PRINT_NORMAL, _T("Tmp68301: UP IRQ[%x] %04x  timer[%d] IMR:%04x IVNR:%04x ICR:%04x\n"), level, tmp68301_irq_vector[level], i, IMR, IVNR, ICR);

		//SekSetIRQLine(level, CPU_IRQSTATUS_ACK);
		SekSetIRQLine(level, CPU_IRQSTATUS_AUTO);
	}
}

static void Tmp68301Reset()
{
	for (INT32 i = 0x80/2; i < 0x94/2; i++) {
		RamTMP68301[i] = 0x07;
	}
	RamTMP68301[0x94/2] = 0x07f7;
}

static void tmp68301_regs_w(UINT32 addr, UINT16 /*val*/ )
{
	//bprintf(PRINT_NORMAL, _T("Tmp68301: write val %04x to location %06x\n"), val, addr);
	//tmp68301_update_timer( (addr >> 5) & 3 );
	switch ( addr ) {
	case 0x200: tmp68301_update_timer(0); break;
	case 0x220: tmp68301_update_timer(1); break;
	case 0x240: tmp68301_update_timer(2); break;
	}
}

void __fastcall Tmp68301WriteByte(UINT32 sekAddress, UINT8 byteValue)
{
	sekAddress &= 0x0003ff;
	UINT8 *p = (UINT8 *)RamTMP68301;
	p[sekAddress ^ 1] = byteValue;
//	bprintf(PRINT_NORMAL, _T("TMP68301 Reg %04X <- %04X & %04X   %04x\n"),sekAddress&0xfffe, (sekAddress&1)?byteValue:byteValue<<8,(sekAddress&1)?0x00ff:0xff00, RamTMP68301[sekAddress>>1]);
}

void __fastcall Tmp68301WriteWord(UINT32 sekAddress, UINT16 wordValue)
{
	sekAddress &= 0x0003ff;
	RamTMP68301[ sekAddress >> 1 ] = BURN_ENDIAN_SWAP_INT16(wordValue);
	tmp68301_regs_w( sekAddress, wordValue );
//	bprintf(PRINT_NORMAL, _T("TMP68301 Reg %04X <- %04X & %04X   %04x\n"),sekAddress,wordValue,0xffff, RamTMP68301[sekAddress>>1]);
}

UINT8 __fastcall Tmp68301ReadByte(UINT32 sekAddress)
{
	sekAddress &= 0x0003ff;
	UINT8 *p = (UINT8 *)RamTMP68301;
	return p[sekAddress ^ 1];
}

UINT16 __fastcall Tmp68301ReadWord(UINT32 sekAddress)
{
	sekAddress &= 0x0003ff;
	UINT16 val = BURN_ENDIAN_SWAP_INT16(RamTMP68301[ sekAddress >> 1 ]);
	if (sekAddress == 0x010a)
		val = 0xff00 | DrvInput[7]; // second dip read through parallel port of tmp68301
	return val;
}

UINT8 __fastcall grdiansReadByte(UINT32 sekAddress)
{
	switch (sekAddress) {

		default:
			bprintf(PRINT_NORMAL, _T("Attempt to read byte value of location %x\n"), sekAddress);
	}
	return 0;
}

UINT16 __fastcall grdiansReadWord(UINT32 sekAddress)
{
	switch (sekAddress) {
		case 0x600000: return ~DrvInput[3]; // DIP 1
		case 0x600002: return ~DrvInput[4]; // DIP 2
		case 0x700000: return ~DrvInput[0]; // Joy 1
		case 0x700002: return ~DrvInput[1]; // Joy 2
		case 0x700004: return ~DrvInput[2]; // Coin

		case 0x70000C:
			//bprintf(PRINT_NORMAL, _T("watchdog_reset16_r\n"));
			return 0xFFFF;
//		default:
//			bprintf(PRINT_NORMAL, _T("Attempt to read word value of location %x\n"), sekAddress);
	}
	return 0;
}

void __fastcall grdiansWriteByte(UINT32 sekAddress, UINT8 byteValue)
{
	switch (sekAddress) {

		default:
			bprintf(PRINT_NORMAL, _T("Attempt to write byte value %x to location %x\n"), byteValue, sekAddress);
	}
}

void __fastcall grdiansWriteWord(UINT32 sekAddress, UINT16 wordValue)
{
	switch (sekAddress) {
		case 0x800000:
			//bprintf(PRINT_NORMAL, _T("lockout 0x%04x\n"), wordValue);
			break;

		case 0xE00010:
		case 0xE00012:
		case 0xE00014:
		case 0xE00016:
		case 0xE00018:
		case 0xE0001A:
		case 0xE0001C:
		case 0xE0001E:
			x1010_sound_bank_w( (sekAddress - 0xE00010) >> 1, wordValue );
			break;

		case 0xE00000:	// nop
			break;
//		default:
//			bprintf(PRINT_NORMAL, _T("Attempt to write word value %x to location %x\n"), wordValue, sekAddress);
	}
}

// d_seta.cpp
//void __fastcall setaSoundRegWriteByte(UINT32 sekAddress, UINT8 byteValue);
void __fastcall setaSoundRegWriteWord(UINT32 sekAddress, UINT16 wordValue);
//UINT8 __fastcall setaSoundRegReadByte(UINT32 sekAddress);
UINT16 __fastcall setaSoundRegReadWord(UINT32 sekAddress);

static UINT8 __fastcall setaSoundRegReadByte(UINT32 /*sekAddress*/)
{
	//bprintf(PRINT_NORMAL, _T("x1-010 to read byte value of location %x\n"), sekAddress);
	return 0;
}

static void __fastcall setaSoundRegWriteByte(UINT32 sekAddress, UINT8 byteValue)
{
	// bprintf(PRINT_NORMAL, _T("x1-010 to write byte value %x to location %x\n"), byteValue, sekAddress);
	UINT32 offset = (sekAddress & 0x00003fff) >> 1;
	INT32 channel, reg;

	if (sekAddress & 1) {

		x1_010_chip->HI_WORD_BUF[ offset ] = byteValue;

	} else {

		offset ^= x1_010_chip->address;

		channel	= offset / sizeof(X1_010_CHANNEL);
		reg		= offset % sizeof(X1_010_CHANNEL);

		if( channel < SETA_NUM_CHANNELS && reg == 0 && (x1_010_chip->reg[offset]&1) == 0 && (byteValue&1) != 0 ) {
			x1_010_chip->smp_offset[channel] = 0;
			x1_010_chip->env_offset[channel] = 0;
		}
		x1_010_chip->reg[offset] = byteValue;

	}

}

void __fastcall setaVideoRegWriteWord(UINT32 sekAddress, UINT16 wordValue)
{
	sekAddress &= 0x3f;

	RamVReg[sekAddress >> 1] = BURN_ENDIAN_SWAP_INT16(wordValue);

	switch (sekAddress)
	{
		case 0x1c:  // FLIP SCREEN (myangel)    <- this is actually zoom
			break;
		case 0x2a:  // FLIP X (pzlbowl)
			break;
		case 0x2c:  // FLIP Y (pzlbowl)
			break;
		case 0x30:  // BLANK SCREEN (pzlbowl, myangel)
			break;
		case 0x24: // funcube3 and staraudi write here instead, why? mirror or different meaning?
		case 0x26: // something display list related? buffering control?
			if (wordValue)
			{
				/* copy the base spritelist to a private (non-CPU visible buffer)
				   copy the indexed sprites to 0 in spriteram, adjusting pointers in base sprite list as appropriate
				   this at least gets the sprite data in the right place for the grdians raster effect to write the
				   changed scroll values to the correct sprites, but is still nothing more than a guess
				*/
				INT32 current_sprite_entry = 0;

				for (INT32 i = 0; i < 0x1000 / 2; i += 4)
				{
					UINT16 num = BURN_ENDIAN_SWAP_INT16(RamSpr[(0x3000 / 2) + i + 0]);
					RamSprPriv[i + 0] = RamSpr[(0x3000 / 2) + i + 0];
					RamSprPriv[i + 1] = RamSpr[(0x3000 / 2) + i + 1];
					RamSprPriv[i + 2] = RamSpr[(0x3000 / 2) + i + 2];

					INT32 sprite = BURN_ENDIAN_SWAP_INT16(RamSpr[(0x3000 / 2) + i + 3]);
					RamSprPriv[i + 3] = BURN_ENDIAN_SWAP_INT16(((current_sprite_entry / 4) & 0x7fff) | (sprite & 0x8000));

					INT32 list2addr = (sprite & 0x7fff) * 4;

					num &=0xff;

					for (INT32 j = 0; j <= num; j++)
					{
						if (current_sprite_entry < 0x3000 / 2)
						{
							RamSpr[current_sprite_entry + 0] = RamSpr[(list2addr + (j * 4) + 0) & 0x1ffff];
							RamSpr[current_sprite_entry + 1] = RamSpr[(list2addr + (j * 4) + 1) & 0x1ffff];
							RamSpr[current_sprite_entry + 2] = RamSpr[(list2addr + (j * 4) + 2) & 0x1ffff];
							RamSpr[current_sprite_entry + 3] = RamSpr[(list2addr + (j * 4) + 3) & 0x1ffff];
							current_sprite_entry += 4;
						}
					}

					if (BURN_ENDIAN_SWAP_INT16(RamSprPriv[i + 0]) & 0x8000) // end of list marker, mj4simai must draw the sprite this covers for the company logo, title screen etc.
					{
						// HACK: however penbros has a dummy sprite entry there which points to 0x0000 as the tile source, and causes garbage with the rearranged format,
						// so change it to something that's invalid where we can filter it later.  This strongly indicates that the current approach is incorrect however.
						if (sprite == 0x00)
						{
							RamSprPriv[i + 3] |= BURN_ENDIAN_SWAP_INT16(0x4000);
						}

						break;
					}
				}

			}
			break;
		case 0x3c: // Raster IRQ enable/arm
			raster_en = wordValue & 1;
			raster_extra = 0;
			raster_pos = raster_latch;
			if (raster_pos == current_scanline) {
				// the first scanline to start the raster chain is weird
				raster_pos = (current_scanline + 1);
				raster_extra = 1;
			}
			break;
		case 0x3e: // Raster IRQ latch
			raster_latch = wordValue;
			break;
	}
}

void __fastcall grdiansPaletteWriteByte(UINT32 sekAddress, UINT8 byteValue)
{
	bprintf(PRINT_NORMAL, _T("Pal to write byte value %x to location %x\n"), byteValue, sekAddress);
}

void __fastcall grdiansPaletteWriteWord(UINT32 sekAddress, UINT16 wordValue)
{
	//bprintf(PRINT_NORMAL, _T("Pal to write word value %x to location %x\n"), wordValue, sekAddress);
	sekAddress &= 0x00FFFF;
	sekAddress >>= 1;
	RamPal[sekAddress] = wordValue;
	CurPal[sekAddress] = CalcCol( wordValue );
}

void __fastcall grdiansClearWriteByte(UINT32, UINT8) {}
void __fastcall grdiansClearWriteWord(UINT32, UINT16) {}

INT32 __fastcall grdiansSekIrqCallback(INT32 irq)
{
	//bprintf(PRINT_NORMAL, _T("Sek Irq Call back %d vector %04x\n"), irq, tmp68301_irq_vector[irq]);
	return tmp68301_irq_vector[irq];
}

static INT32 DrvDoReset()
{
	SekOpen(0);
	SekReset();
	Tmp68301Reset();
	SekClose();

	x1010Reset();

	nExtraCycles = 0;

	if (!strcmp(BurnDrvGetTextA(DRV_NAME), "gundamex")) {
		EEPROMReset(); // gundam
		if (EEPROMAvailable() == 0) {
			UINT8 EEPROMDATA[2] = { 0x08, 0x70 };
			EEPROMFill(EEPROMDATA, 0, 2);
		}
	}

	HiscoreReset();

	return 0;
}

static INT32 MemIndex(INT32 CodeSize, INT32 GfxSize, INT32 PcmSize, INT32 ExtRamSize)
{
	nRomGfxLen = GfxSize;
	UINT8 *Next; Next = Mem;
	Rom68K 		= Next; Next += CodeSize;			// 68000 ROM
	RomGfx		= Next; Next += GfxSize;			// GFX Rom
	X1010SNDROM	= Next; Next += PcmSize;			// PCM

	RamStart	= Next;

	Ram68K		= Next; Next += 0x010000;
	RamUnknown	= Next; Next += ExtRamSize;
	if (HasNVRam) { RamNV = Next; Next += 0x10000; }
	RamSpr		= (UINT16 *) Next; Next += 0x020000 * sizeof(UINT16);
	RamSprPriv	= (UINT16 *) Next; Next += 0x1000/2 * sizeof(UINT16);
	RamPal		= (UINT16 *) Next; Next += 0x008000 * sizeof(UINT16);
	RamTMP68301	= (UINT16 *) Next; Next += 0x000200 * sizeof(UINT16);

	RamVReg		= (UINT16 *) Next; Next += 0x000020 * sizeof(UINT16);

	RamEnd		= Next;

	CurPal		= (UINT32 *) Next; Next += 0x008000 * sizeof(UINT32);

	MemEnd		= Next;
	return 0;
}

static void loadDecodeGfx(UINT8 *p, INT32 cnt, INT32 offset2x)
{
	UINT8 * d = RomGfx;
	UINT8 * q = p + 1;

	for (INT32 i=0; i<cnt; i++, p+=2, q+=2, d+=8) {
		*(d+0) |= (( (*p >> 7) & 1 ) << offset2x) | (( (*q >> 7) & 1 ) << (offset2x + 1));
		*(d+1) |= (( (*p >> 6) & 1 ) << offset2x) | (( (*q >> 6) & 1 ) << (offset2x + 1));
		*(d+2) |= (( (*p >> 5) & 1 ) << offset2x) | (( (*q >> 5) & 1 ) << (offset2x + 1));
		*(d+3) |= (( (*p >> 4) & 1 ) << offset2x) | (( (*q >> 4) & 1 ) << (offset2x + 1));
		*(d+4) |= (( (*p >> 3) & 1 ) << offset2x) | (( (*q >> 3) & 1 ) << (offset2x + 1));
		*(d+5) |= (( (*p >> 2) & 1 ) << offset2x) | (( (*q >> 2) & 1 ) << (offset2x + 1));
		*(d+6) |= (( (*p >> 1) & 1 ) << offset2x) | (( (*q >> 1) & 1 ) << (offset2x + 1));
		*(d+7) |= (( (*p >> 0) & 1 ) << offset2x) | (( (*q >> 0) & 1 ) << (offset2x + 1));
	}
}

static INT32 grdiansInit()
{
	INT32 nRet;

	Mem = NULL;
	MemIndex(0x0200000, 0x2000000, 0x0200000, 0x00C000);
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(Mem, 0, nLen);										// blank all memory
	MemIndex(0x0200000, 0x2000000, 0x0200000, 0x00C000);

	// Load and byte-swap 68000 Program roms
	nRet = BurnLoadRom(Rom68K + 0x000001, 0, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Rom68K + 0x000000, 1, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Rom68K + 0x100001, 2, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Rom68K + 0x100000, 3, 2); if (nRet != 0) return 1;

	// Load Gfx
	UINT8 * tmpGfx = (UINT8 *)BurnMalloc(0x0800000);
	for (INT32 i=0; i<8; i+=2) {
		BurnLoadRom(tmpGfx + 0x0200000, i+5, 1);
		memcpy(tmpGfx + 0x0600000, tmpGfx + 0x0200000, 0x0200000);
		BurnLoadRom(tmpGfx + 0x0000000, i+4, 1);
		loadDecodeGfx( tmpGfx, 0x0800000 / 2, i );
	}

	BurnFree(tmpGfx);

	// Leave 1MB empty (addressable by the chip)
	BurnLoadRom(X1010SNDROM + 0x100000, 12, 1);

	{
		SekInit(0, 0x68000);										// Allocate 68000
		SekOpen(0);

		// Map 68000 memory:
		SekMapMemory(Rom68K,		0x000000, 0x1FFFFF, MAP_ROM);	// CPU 0 ROM
		SekMapMemory(Ram68K,		0x200000, 0x20FFFF, MAP_RAM);	// CPU 0 RAM
		SekMapMemory(RamUnknown,	0x304000, 0x30FFFF, MAP_RAM);	// ? seems tile data

		SekMapMemory((UINT8 *)RamSpr,
									0xC00000, 0xC3FFFF, MAP_RAM);	// sprites
		SekMapMemory((UINT8 *)RamPal,
									0xC40000, 0xC4FFFF, MAP_ROM);	// Palette
		SekMapMemory((UINT8 *)RamVReg,
									0xC60000, 0xC6003F, MAP_READ | MAP_FETCH);	// Video Registers
		SekMapMemory((UINT8 *)RamTMP68301,
									0xFFFC00, 0xFFFFFF, MAP_ROM);	// TMP68301 Registers

		SekMapHandler(1,			0xB00000, 0xB03FFF, MAP_READ | MAP_WRITE);
		SekMapHandler(2,			0xC40000, 0xC4FFFF, MAP_WRITE);	// Palette
		SekMapHandler(3,			0xC50000, 0xC5FFFF, MAP_WRITE);
		SekMapHandler(4,			0xC60000, 0xC6003F, MAP_WRITE);
		SekMapHandler(5,			0xFFFC00, 0xFFFFFF, MAP_WRITE);

		SekSetReadWordHandler(0, grdiansReadWord);
		SekSetReadByteHandler(0, grdiansReadByte);
		SekSetWriteWordHandler(0, grdiansWriteWord);
		SekSetWriteByteHandler(0, grdiansWriteByte);

		SekSetReadWordHandler (1, setaSoundRegReadWord);
		SekSetReadByteHandler (1, setaSoundRegReadByte);
		SekSetWriteWordHandler(1, setaSoundRegWriteWord);
		SekSetWriteByteHandler(1, setaSoundRegWriteByte);

		SekSetWriteWordHandler(2, grdiansPaletteWriteWord);
		SekSetWriteByteHandler(2, grdiansPaletteWriteByte);

		SekSetWriteWordHandler(3, grdiansClearWriteWord);
		SekSetWriteByteHandler(3, grdiansClearWriteByte);

		SekSetWriteWordHandler(4, setaVideoRegWriteWord);

		SekSetWriteWordHandler(5, Tmp68301WriteWord);
		SekSetWriteByteHandler(5, Tmp68301WriteByte);

		SekSetIrqCallback( grdiansSekIrqCallback );

		SekClose();
	}
	
	GenericTilesInit();

	x1010_sound_init(M68K_CYCS, 0x0000);
	x1010_set_route(BURN_SND_X1010_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	x1010_set_route(BURN_SND_X1010_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);

	DrvDoReset();

	return 0;
}

static INT32 grdiansaInit()
{
	INT32 nRet;

	Mem = NULL;
	MemIndex(0x0200000, 0x2000000, 0x0200000, 0x00C000);
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(Mem, 0, nLen);										// blank all memory
	MemIndex(0x0200000, 0x2000000, 0x0200000, 0x00C000);

	UINT8* tmpGfx = (UINT8*)BurnMalloc(0x0600000);

	if (1 == grdiansbl) {
		// Load 68000 Program rom
		nRet = BurnLoadRom(Rom68K + 0x000000, 0, 1); if (nRet != 0) return 1;

		// Load Gfx
		for (INT32 i = 0; i < 4; i++) {
			BurnLoadRom(tmpGfx, i + 1, 1);
			memmove(tmpGfx + 0x400000, tmpGfx, 0x200000);
			memset(tmpGfx, 0, 0x200000);
			loadDecodeGfx(tmpGfx, 0x0600000 / 2, i * 2);
		}
	} else {
		// Load and byte-swap 68000 Program roms
		nRet = BurnLoadRom(Rom68K + 0x000001, 0, 2); if (nRet != 0) return 1;
		nRet = BurnLoadRom(Rom68K + 0x000000, 1, 2); if (nRet != 0) return 1;
		nRet = BurnLoadRom(Rom68K + 0x100001, 2, 2); if (nRet != 0) return 1;
		nRet = BurnLoadRom(Rom68K + 0x100000, 3, 2); if (nRet != 0) return 1;

		// Load Gfx
		for (INT32 i = 0; i < 8; i += 2) {
			BurnLoadRom(tmpGfx + 0x0200000, i + 4, 1);
			BurnLoadRom(tmpGfx + 0x0400000, i + 5, 1);
			loadDecodeGfx(tmpGfx, 0x0600000 / 2, i);
		}
	}

	BurnFree(tmpGfx);

	BurnLoadRom(X1010SNDROM + 0x000000, (1 == grdiansbl) ? 5 : 12, 1);

	{
		SekInit(0, 0x68000);										// Allocate 68000
		SekOpen(0);

		// Map 68000 memory:
		SekMapMemory(Rom68K,		0x000000, 0x1FFFFF, MAP_ROM);	// CPU 0 ROM
		SekMapMemory(Ram68K,		0x200000, 0x20FFFF, MAP_RAM);	// CPU 0 RAM
		SekMapMemory(RamUnknown,	0x304000, 0x30FFFF, MAP_RAM);	// ? seems tile data

		SekMapMemory((UINT8 *)RamSpr,
									0xC00000, 0xC3FFFF, MAP_RAM);	// sprites
		SekMapMemory((UINT8 *)RamPal,
									0xC40000, 0xC4FFFF, MAP_ROM);	// Palette
		SekMapMemory((UINT8 *)RamVReg,
									0xC60000, 0xC6003F, MAP_READ | MAP_FETCH);	// Video Registers
		SekMapMemory((UINT8 *)RamTMP68301,
									0xFFFC00, 0xFFFFFF, MAP_ROM);	// TMP68301 Registers

		SekMapHandler(1,			0xB00000, 0xB03FFF, MAP_READ | MAP_WRITE);
		SekMapHandler(2,			0xC40000, 0xC4FFFF, MAP_WRITE);	// Palette
		SekMapHandler(3,			0xC50000, 0xC5FFFF, MAP_WRITE);
		SekMapHandler(4,			0xC60000, 0xC6003F, MAP_WRITE);
		SekMapHandler(5,			0xFFFC00, 0xFFFFFF, MAP_WRITE);

		SekSetReadWordHandler(0, grdiansReadWord);
		SekSetReadByteHandler(0, grdiansReadByte);
		SekSetWriteWordHandler(0, grdiansWriteWord);
		SekSetWriteByteHandler(0, grdiansWriteByte);

		SekSetReadWordHandler (1, setaSoundRegReadWord);
		SekSetReadByteHandler (1, setaSoundRegReadByte);
		SekSetWriteWordHandler(1, setaSoundRegWriteWord);
		SekSetWriteByteHandler(1, setaSoundRegWriteByte);

		SekSetWriteWordHandler(2, grdiansPaletteWriteWord);
		SekSetWriteByteHandler(2, grdiansPaletteWriteByte);

		SekSetWriteWordHandler(3, grdiansClearWriteWord);
		SekSetWriteByteHandler(3, grdiansClearWriteByte);

		SekSetWriteWordHandler(4, setaVideoRegWriteWord);

		SekSetWriteWordHandler(5, Tmp68301WriteWord);
		SekSetWriteByteHandler(5, Tmp68301WriteByte);

		SekSetIrqCallback( grdiansSekIrqCallback );

		SekClose();
	}
	
	GenericTilesInit();

	M68K_CYCS = 32530470 / 2;
	x1010_sound_init(M68K_CYCS, 0x0000);
	x1010_set_route(BURN_SND_X1010_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	x1010_set_route(BURN_SND_X1010_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);

	DrvDoReset();

	return 0;
}

static INT32 grdiansblInit()
{
	grdiansbl = 1;

	return grdiansaInit();
}

// -- mj4simai -----------------------------------------------------------

UINT8 __fastcall mj4simaiReadByte(UINT32 sekAddress)
{
	switch (sekAddress) {

		default:
			bprintf(PRINT_NORMAL, _T("Attempt to read byte value of location %x\n"), sekAddress);
	}
	return 0;
}

UINT16 __fastcall mj4simaiReadWord(UINT32 sekAddress)
{
	switch (sekAddress) {

		case 0x600006: 
			//bprintf(PRINT_NORMAL, _T("watchdog_reset16_r\n"));
			return 0xffff;

		case 0x600000:
		case 0x600002:
			switch (Mahjong_keyboard)
			{
				case 0x01: return ~DrvInput[0];
				case 0x02: return ~DrvInput[1];
				case 0x04: return ~DrvInput[5];
				case 0x08: return ~DrvInput[6];
				case 0x10: return ~DrvInput[7];
				default:   return 0xffff;
			}	

		case 0x600100: return ~DrvInput[2];	// Coin

		case 0x600300: return ~DrvInput[3]; // DIP 1
		case 0x600302: return ~DrvInput[4]; // DIP 2

//		default:
//			bprintf(PRINT_NORMAL, _T("Attempt to read word value of location %x\n"), sekAddress);
	}
	return 0;
}

void __fastcall mj4simaiWriteByte(UINT32 sekAddress, UINT8 byteValue)
{
	switch (sekAddress) {

		default:
			bprintf(PRINT_NORMAL, _T("Attempt to write byte value %x to location %x\n"), byteValue, sekAddress);
	}
}

void __fastcall mj4simaiWriteWord(UINT32 sekAddress, UINT16 wordValue)
{
	switch (sekAddress) {
		case 0x600200: break; // NOP

		case 0x600300:
		case 0x600302:
		case 0x600304:
		case 0x600306:
		case 0x600308:
		case 0x60030A:
		case 0x60030C:
		case 0x60030E:
			x1010_sound_bank_w( (sekAddress & 0xf) >> 1, wordValue );
			break;
		case 0x600004:
			Mahjong_keyboard = wordValue & 0xff; 
			break;
//		default:
//			bprintf(PRINT_NORMAL, _T("Attempt to write word value %x to location %x\n"), wordValue, sekAddress);
	}
}

static INT32 mj4simaiInit()
{
	INT32 nRet;

	Mem = NULL;
	MemIndex(0x0200000, 0x2000000, 0x0500000, 0x000000);
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(Mem, 0, nLen);										// blank all memory
	MemIndex(0x0200000, 0x2000000, 0x0500000, 0x000000);

	// Load and byte-swap 68000 Program roms
	nRet = BurnLoadRom(Rom68K + 0x000001, 0, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Rom68K + 0x000000, 1, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Rom68K + 0x100001, 2, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Rom68K + 0x100000, 3, 2); if (nRet != 0) return 1;

	// Load Gfx
	UINT8 * tmpGfx = (UINT8 *)BurnMalloc(0x0800000);
	for (INT32 i=0; i<6; i+=2) {
		BurnLoadRom(tmpGfx + 0x0000000, i+4, 1);
		BurnLoadRom(tmpGfx + 0x0400000, i+5, 1);
		loadDecodeGfx( tmpGfx, 0x0800000 / 2, i );
	}
	BurnFree(tmpGfx);

	// Leave 1MB empty (addressable by the chip)
	BurnLoadRom(X1010SNDROM + 0x100000, 10, 1);

	{
		SekInit(0, 0x68000);										// Allocate 68000
		SekOpen(0);

		// Map 68000 memory:
		SekMapMemory(Rom68K,		0x000000, 0x1FFFFF, MAP_ROM);	// CPU 0 ROM
		SekMapMemory(Ram68K,		0x200000, 0x20FFFF, MAP_RAM);	// CPU 0 RAM

		SekMapMemory((UINT8 *)RamSpr,
									0xC00000, 0xC3FFFF, MAP_RAM);	// sprites
		SekMapMemory((UINT8 *)RamPal,
									0xC40000, 0xC4FFFF, MAP_ROM);	// Palette
		SekMapMemory((UINT8 *)RamVReg,
									0xC60000, 0xC6003F, MAP_RAM);	// Video Registers
		SekMapMemory((UINT8 *)RamTMP68301,
									0xFFFC00, 0xFFFFFF, MAP_ROM);	// TMP68301 Registers

		SekMapHandler(1,			0xB00000, 0xB03FFF, MAP_READ | MAP_WRITE);
		SekMapHandler(2,			0xC40000, 0xC4FFFF, MAP_WRITE);	// Palette
		SekMapHandler(3,			0xC60000, 0xC6003F, MAP_WRITE);
		SekMapHandler(4,			0xFFFC00, 0xFFFFFF, MAP_WRITE);

		SekSetReadWordHandler(0, mj4simaiReadWord);
		SekSetReadByteHandler(0, mj4simaiReadByte);
		SekSetWriteWordHandler(0, mj4simaiWriteWord);
		SekSetWriteByteHandler(0, mj4simaiWriteByte);

		SekSetReadWordHandler (1, setaSoundRegReadWord);
		SekSetReadByteHandler (1, setaSoundRegReadByte);
		SekSetWriteWordHandler(1, setaSoundRegWriteWord);
		SekSetWriteByteHandler(1, setaSoundRegWriteByte);

		SekSetWriteWordHandler(2, grdiansPaletteWriteWord);
		SekSetWriteByteHandler(2, grdiansPaletteWriteByte);

		SekSetWriteWordHandler(3, setaVideoRegWriteWord);

		SekSetWriteWordHandler(4, Tmp68301WriteWord);
		SekSetWriteByteHandler(4, Tmp68301WriteByte);

		SekSetIrqCallback( grdiansSekIrqCallback );

		SekClose();
	}

	GenericTilesInit();

	x1010_sound_init(50000000 / 3, 0x0000);
	x1010_set_route(BURN_SND_X1010_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	x1010_set_route(BURN_SND_X1010_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);

//	bMahjong = 1;

	DrvDoReset();	

	return 0;
}

// -- myangel -----------------------------------------------------------

UINT8 __fastcall myangelReadByte(UINT32 sekAddress)
{
	switch (sekAddress) {

		default:
			bprintf(PRINT_NORMAL, _T("Attempt to read byte value of location %x\n"), sekAddress);
	}
	return 0;
}

UINT16 __fastcall myangelReadWord(UINT32 sekAddress)
{
	switch (sekAddress) {
		case 0x700000: return ~DrvInput[0];
		case 0x700002: return ~DrvInput[1];
		case 0x700004: return ~DrvInput[2];

		case 0x700006:
			//bprintf(PRINT_NORMAL, _T("watchdog_reset16_r\n"));
			return 0xffff;

		case 0x700300: return ~DrvInput[3]; // DIP 1
		case 0x700302: return ~DrvInput[4]; // DIP 2

//		default:
//			bprintf(PRINT_NORMAL, _T("Attempt to read word value of location %x\n"), sekAddress);
	}
	return 0;
}

void __fastcall myangelWriteByte(UINT32 sekAddress, UINT8 byteValue)
{
	switch (sekAddress) {

		default:
			bprintf(PRINT_NORMAL, _T("Attempt to write byte value %x to location %x\n"), byteValue, sekAddress);
	}
}

void __fastcall myangelWriteWord(UINT32 sekAddress, UINT16 wordValue)
{
	switch (sekAddress) {
		case 0x700200: break; // NOP

		case 0x700310:
		case 0x700312:
		case 0x700314:
		case 0x700316:
		case 0x700318:
		case 0x70031A:
		case 0x70031C:
		case 0x70031E:
			x1010_sound_bank_w( (sekAddress & 0xf) >> 1, wordValue );
			break;

//		default:
//			bprintf(PRINT_NORMAL, _T("Attempt to write word value %x to location %x\n"), wordValue, sekAddress);
	}
}

static INT32 myangelInit()
{
	INT32 nRet;

	Mem = NULL;
	MemIndex(0x0200000, 0x1000000, 0x0300000, 0x000000);
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(Mem, 0, nLen);										// blank all memory
	MemIndex(0x0200000, 0x1000000, 0x0300000, 0x000000);

	// Load and byte-swap 68000 Program roms
	nRet = BurnLoadRom(Rom68K + 0x000001, 0, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Rom68K + 0x000000, 1, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Rom68K + 0x100001, 2, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Rom68K + 0x100000, 3, 2); if (nRet != 0) return 1;

	// Load Gfx
	UINT8 * tmpGfx = (UINT8 *)BurnMalloc(0x0400000);
	for (INT32 i=0; i<8; i+=2) {
		BurnLoadRom(tmpGfx + 0x0000000, i+4, 1);
		BurnLoadRom(tmpGfx + 0x0200000, i+5, 1);
		loadDecodeGfx( tmpGfx, 0x0400000 / 2, i );
	}
	BurnFree(tmpGfx);

	// Leave 1MB empty (addressable by the chip)
	BurnLoadRom(X1010SNDROM + 0x100000, 12, 1);

	{
		SekInit(0, 0x68000);										// Allocate 68000
		SekOpen(0);

		// Map 68000 memory:
		SekMapMemory(Rom68K,		0x000000, 0x1FFFFF, MAP_ROM);	// CPU 0 ROM
		SekMapMemory(Ram68K,		0x200000, 0x20FFFF, MAP_RAM);	// CPU 0 RAM

		SekMapMemory((UINT8 *)RamSpr,
									0xC00000, 0xC3FFFF, MAP_RAM);	// sprites
		SekMapMemory((UINT8 *)RamPal,
									0xC40000, 0xC4FFFF, MAP_ROM);	// Palette
		SekMapMemory((UINT8 *)RamVReg,
									0xC60000, 0xC6003F, MAP_RAM);	// Video Registers
		SekMapMemory((UINT8 *)RamTMP68301,
									0xFFFC00, 0xFFFFFF, MAP_ROM);	// TMP68301 Registers

		SekMapHandler(1,			0xB00000, 0xB03FFF, MAP_READ | MAP_WRITE);
		SekMapHandler(2,			0xC40000, 0xC4FFFF, MAP_WRITE);	// Palette
		SekMapHandler(3,			0xC60000, 0xC6003F, MAP_WRITE);	// Palette
		SekMapHandler(4,			0xFFFC00, 0xFFFFFF, MAP_WRITE);

		SekSetReadWordHandler(0, myangelReadWord);
		SekSetReadByteHandler(0, myangelReadByte);
		SekSetWriteWordHandler(0, myangelWriteWord);
		SekSetWriteByteHandler(0, myangelWriteByte);

		SekSetReadWordHandler (1, setaSoundRegReadWord);
		SekSetReadByteHandler (1, setaSoundRegReadByte);
		SekSetWriteWordHandler(1, setaSoundRegWriteWord);
		SekSetWriteByteHandler(1, setaSoundRegWriteByte);

		SekSetWriteWordHandler(2, grdiansPaletteWriteWord);
		SekSetWriteByteHandler(2, grdiansPaletteWriteByte);

		SekSetWriteWordHandler(3, setaVideoRegWriteWord);

		SekSetWriteWordHandler(4, Tmp68301WriteWord);
		SekSetWriteByteHandler(4, Tmp68301WriteByte);

		SekSetIrqCallback( grdiansSekIrqCallback );

		SekClose();
	}

	GenericTilesInit();

	x1010_sound_init(50000000 / 3, 0x0000);
	x1010_set_route(BURN_SND_X1010_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	x1010_set_route(BURN_SND_X1010_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);

	DrvDoReset();	

	return 0;
}

// -- myangel2 -----------------------------------------------------------

UINT8 __fastcall myangel2ReadByte(UINT32 sekAddress)
{
	switch (sekAddress) {

		default:
			bprintf(PRINT_NORMAL, _T("Attempt to read byte value of location %x\n"), sekAddress);
	}
	return 0;
}

UINT16 __fastcall myangel2ReadWord(UINT32 sekAddress)
{
	switch (sekAddress) {
		case 0x600000: return ~DrvInput[0];
		case 0x600002: return ~DrvInput[1];
		case 0x600004: return ~DrvInput[2];

		case 0x600006:
			//bprintf(PRINT_NORMAL, _T("watchdog_reset16_r\n"));
			return 0xffff;

		case 0x600300: return ~DrvInput[3]; // DIP 1
		case 0x600302: return ~DrvInput[4]; // DIP 2

//		default:
//			bprintf(PRINT_NORMAL, _T("Attempt to read word value of location %x\n"), sekAddress);
	}
	return 0;
}

void __fastcall myangel2WriteByte(UINT32 sekAddress, UINT8 byteValue)
{
	switch (sekAddress) {

		default:
			bprintf(PRINT_NORMAL, _T("Attempt to write byte value %x to location %x\n"), byteValue, sekAddress);
	}
}

void __fastcall myangel2WriteWord(UINT32 sekAddress, UINT16 wordValue)
{
	switch (sekAddress) {
		case 0x600200: break; // NOP

		case 0x600300:
		case 0x600302:
		case 0x600304:
		case 0x600306:
		case 0x600308:
		case 0x60030A:
		case 0x60030C:
		case 0x60030E:
			x1010_sound_bank_w( (sekAddress & 0xf) >> 1, wordValue );
			break;

//		default:
//			bprintf(PRINT_NORMAL, _T("Attempt to write word value %x to location %x\n"), wordValue, sekAddress);
	}
}

static INT32 myangel2Init()
{
	INT32 nRet;

	Mem = NULL;
	MemIndex(0x0200000, 0x1800000, 0x0500000, 0x000000);
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(Mem, 0, nLen);										// blank all memory
	MemIndex(0x0200000, 0x1800000, 0x0500000, 0x000000);

	// Load and byte-swap 68000 Program roms
	nRet = BurnLoadRom(Rom68K + 0x000001, 0, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Rom68K + 0x000000, 1, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Rom68K + 0x100001, 2, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Rom68K + 0x100000, 3, 2); if (nRet != 0) return 1;

	// Load Gfx
	UINT8 * tmpGfx = (UINT8 *)BurnMalloc(0x0600000);
	for (INT32 i = 0; i < 8; i+=2) {
		BurnLoadRom(tmpGfx + 0x0000000,  i + 4, 1);
		BurnLoadRom(tmpGfx + 0x0200000,  i + 5, 1);
		loadDecodeGfx(tmpGfx, 0x0600000 / 2, i);
	}
	BurnFree(tmpGfx);

	// Leave 1MB empty (addressable by the chip)
	BurnLoadRom(X1010SNDROM + 0x100000, 12, 1);

	{
		SekInit(0, 0x68000);										// Allocate 68000
		SekOpen(0);

		// Map 68000 memory:
		SekMapMemory(Rom68K,		0x000000, 0x1FFFFF, MAP_ROM);	// CPU 0 ROM
		SekMapMemory(Ram68K,		0x200000, 0x20FFFF, MAP_RAM);	// CPU 0 RAM

		SekMapMemory((UINT8 *)RamSpr,
									0xD00000, 0xD3FFFF, MAP_RAM);	// sprites
		SekMapMemory((UINT8 *)RamPal,
									0xD40000, 0xD4FFFF, MAP_ROM);	// Palette
		SekMapMemory((UINT8 *)RamVReg,
									0xD60000, 0xD6003F, MAP_RAM);	// Video Registers
		SekMapMemory((UINT8 *)RamTMP68301,
									0xFFFC00, 0xFFFFFF, MAP_ROM);	// TMP68301 Registers

		SekMapHandler(1,			0xB00000, 0xB03FFF, MAP_READ | MAP_WRITE);
		SekMapHandler(2,			0xD40000, 0xD4FFFF, MAP_WRITE);	// Palette
		SekMapHandler(3,			0xD60000, 0xD6003F, MAP_WRITE);
		SekMapHandler(4,			0xFFFC00, 0xFFFFFF, MAP_WRITE);

		SekSetReadWordHandler(0, myangel2ReadWord);
		SekSetReadByteHandler(0, myangel2ReadByte);
		SekSetWriteWordHandler(0, myangel2WriteWord);
		SekSetWriteByteHandler(0, myangel2WriteByte);

		SekSetReadWordHandler (1, setaSoundRegReadWord);
		SekSetReadByteHandler (1, setaSoundRegReadByte);
		SekSetWriteWordHandler(1, setaSoundRegWriteWord);
		SekSetWriteByteHandler(1, setaSoundRegWriteByte);

		SekSetWriteWordHandler(2, grdiansPaletteWriteWord);
		SekSetWriteByteHandler(2, grdiansPaletteWriteByte);

		SekSetWriteWordHandler(3, setaVideoRegWriteWord);

		SekSetWriteWordHandler(4, Tmp68301WriteWord);
		SekSetWriteByteHandler(4, Tmp68301WriteByte);

		SekSetIrqCallback( grdiansSekIrqCallback );

		SekClose();
	}

	GenericTilesInit();

	x1010_sound_init(50000000 / 3, 0x0000);
	x1010_set_route(BURN_SND_X1010_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	x1010_set_route(BURN_SND_X1010_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);

	DrvDoReset();

	return 0;
}

// -- pzlbowl -----------------------------------------------------------

UINT8 __fastcall pzlbowlReadByte(UINT32 sekAddress)
{
	switch (sekAddress) {

		default:
			bprintf(PRINT_NORMAL, _T("Attempt to read byte value of location %x\n"), sekAddress);
	}
	return 0;
}

UINT16 __fastcall pzlbowlReadWord(UINT32 sekAddress)
{
	switch (sekAddress) {
		case 0x400300: return ~DrvInput[3]; // DIP 1
		case 0x400302: return ~DrvInput[4]; // DIP 2
		case 0x500000: return ~DrvInput[0]; // Player 1
		case 0x500002: return ~DrvInput[1]; // Player 2

		case 0x500004: {
			//readinputport(4) | (rand() & 0x80 )
			static UINT16 prot = 0;
			prot ^= 0x80;
			return ~(prot | DrvInput[2]);
			}
		case 0x500006:
			//bprintf(PRINT_NORMAL, _T("watchdog_reset16_r\n"));
			return 0xffff;
		case 0x700000: {
			/*  The game checks for a specific value read from the ROM region.
				The offset to use is stored in RAM at address 0x20BA16 */
			UINT32 address = (BURN_ENDIAN_SWAP_INT16(*(UINT16 *)(Ram68K + 0x00ba16)) << 16) | BURN_ENDIAN_SWAP_INT16(*(UINT16 *)(Ram68K + 0x00ba18));
			bprintf(PRINT_NORMAL, _T("pzlbowl Protection read address %08x [%02x %02x %02x %02x]\n"), address,
					Rom68K[ address - 2 ], Rom68K[ address - 1 ], Rom68K[ address - 0 ], Rom68K[ address + 1 ]);
			return Rom68K[ address - 2 ]; }
//		default:
//			bprintf(PRINT_NORMAL, _T("Attempt to read word value of location %x\n"), sekAddress);
	}
	return 0;
}

void __fastcall pzlbowlWriteByte(UINT32 sekAddress, UINT8 byteValue)
{
	switch (sekAddress) {

		default:
			bprintf(PRINT_NORMAL, _T("Attempt to write byte value %x to location %x\n"), byteValue, sekAddress);
	}
}

void __fastcall pzlbowlWriteWord(UINT32 sekAddress, UINT16 wordValue)
{
	switch (sekAddress) {

		case 0x400300:
		case 0x400302:
		case 0x400304:
		case 0x400306:
		case 0x400308:
		case 0x40030A:
		case 0x40030C:
		case 0x40030E:
			x1010_sound_bank_w( (sekAddress & 0xf) >> 1, wordValue );
			break;
		case 0x500004:
			//bprintf(PRINT_NORMAL, _T("Coin Counter %x\n"), wordValue);
			break;
//		default:
//			bprintf(PRINT_NORMAL, _T("Attempt to write word value %x to location %x\n"), wordValue, sekAddress);
	}
}

static INT32 pzlbowlInit()
{
	INT32 nRet;

	Mem = NULL;
	MemIndex(0x0100000, 0x1000000, 0x0500000, 0x000000);
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(Mem, 0, nLen);										// blank all memory
	MemIndex(0x0100000, 0x1000000, 0x0500000, 0x000000);

	// Load and byte-swap 68000 Program roms
	nRet = BurnLoadRom(Rom68K + 0x000001, 0, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Rom68K + 0x000000, 1, 2); if (nRet != 0) return 1;

	// Load Gfx
	UINT8 * tmpGfx = (UINT8 *)BurnMalloc(0x0400000);
	for (INT32 i=0; i<4; i++) {
		BurnLoadRom(tmpGfx, i+2, 1);
		loadDecodeGfx( tmpGfx, 0x0400000 / 2, i*2 );
	}
	BurnFree(tmpGfx);

	// Leave 1MB empty (addressable by the chip)
	BurnLoadRom(X1010SNDROM + 0x100000, 6, 1);

	{
		SekInit(0, 0x68000);										// Allocate 68000
		SekOpen(0);

		// Map 68000 memory:
		SekMapMemory(Rom68K,		0x000000, 0x0FFFFF, MAP_ROM);	// CPU 0 ROM
		SekMapMemory(Ram68K,		0x200000, 0x20FFFF, MAP_RAM);	// CPU 0 RAM

		SekMapMemory((UINT8 *)RamSpr,
									0x800000, 0x83FFFF, MAP_RAM);	// sprites
		SekMapMemory((UINT8 *)RamPal,
									0x840000, 0x84FFFF, MAP_ROM);	// Palette
		SekMapMemory((UINT8 *)RamVReg,
									0x860000, 0x86003F, MAP_RAM);	// Video Registers
		SekMapMemory((UINT8 *)RamTMP68301,
									0xFFFC00, 0xFFFFFF, MAP_ROM);	// TMP68301 Registers

		SekMapHandler(1,			0x900000, 0x903FFF, MAP_READ | MAP_WRITE);
		SekMapHandler(2,			0x840000, 0x84FFFF, MAP_WRITE);	// Palette
		SekMapHandler(3,			0x860000, 0x86003F, MAP_WRITE);
		SekMapHandler(4,			0xFFFC00, 0xFFFFFF, MAP_WRITE);

		SekSetReadWordHandler(0, pzlbowlReadWord);
		SekSetReadByteHandler(0, pzlbowlReadByte);
		SekSetWriteWordHandler(0, pzlbowlWriteWord);
		SekSetWriteByteHandler(0, pzlbowlWriteByte);

		SekSetReadWordHandler (1, setaSoundRegReadWord);
		SekSetReadByteHandler (1, setaSoundRegReadByte);
		SekSetWriteWordHandler(1, setaSoundRegWriteWord);
		SekSetWriteByteHandler(1, setaSoundRegWriteByte);

		SekSetWriteWordHandler(2, grdiansPaletteWriteWord);
		SekSetWriteByteHandler(2, grdiansPaletteWriteByte);

		SekSetWriteWordHandler(3, setaVideoRegWriteWord);

		SekSetWriteWordHandler(4, Tmp68301WriteWord);
		SekSetWriteByteHandler(4, Tmp68301WriteByte);

		SekSetIrqCallback( grdiansSekIrqCallback );

		SekClose();
	}

	GenericTilesInit();

	x1010_sound_init(50000000 / 3, 0x0000);
	x1010_set_route(BURN_SND_X1010_ROUTE_1, 1.00, BURN_SND_ROUTE_BOTH);
	x1010_set_route(BURN_SND_X1010_ROUTE_2, 1.00, BURN_SND_ROUTE_BOTH);

	DrvDoReset();

	return 0;
}

// -- penbros -----------------------------------------------------------

UINT8 __fastcall penbrosReadByte(UINT32 sekAddress)
{
	switch (sekAddress) {

		default:
			bprintf(PRINT_NORMAL, _T("Attempt to read byte value of location %x\n"), sekAddress);
	}
	return 0;
}

UINT16 __fastcall penbrosReadWord(UINT32 sekAddress)
{
	switch (sekAddress) {
		case 0x500300: return ~DrvInput[3]; // DIP 1
		case 0x500302: return ~DrvInput[4]; // DIP 2
		case 0x600000: return ~DrvInput[0]; // Player 1
		case 0x600002: return ~DrvInput[1]; // Player 2
		case 0x600004: return ~DrvInput[2]; // Coin

		case 0x600006: 
			//bprintf(PRINT_NORMAL, _T("watchdog_reset16_r\n"));
			return 0xffff;
//		default:
//			bprintf(PRINT_NORMAL, _T("Attempt to read word value of location %x\n"), sekAddress);
	}
	return 0;
}

void __fastcall penbrosWriteByte(UINT32 sekAddress, UINT8 byteValue)
{
	switch (sekAddress) {

		default:
			bprintf(PRINT_NORMAL, _T("Attempt to write byte value %x to location %x\n"), byteValue, sekAddress);
	}
}

void __fastcall penbrosWriteWord(UINT32 sekAddress, UINT16 wordValue)
{
	switch (sekAddress) {

		case 0x500300:
		case 0x500302:
		case 0x500304:
		case 0x500306:
		case 0x500308:
		case 0x50030A:
		case 0x50030C:
		case 0x50030E:
			x1010_sound_bank_w( (sekAddress & 0xf) >> 1, wordValue );
			break;
		case 0x600004:
			//bprintf(PRINT_NORMAL, _T("Coin Counter %x\n"), wordValue);
			break;
//		default:
//			bprintf(PRINT_NORMAL, _T("Attempt to write word value %x to location %x\n"), wordValue, sekAddress);
	}
}

static INT32 penbrosInit()
{
	INT32 nRet;

	Mem = NULL;
	MemIndex(0x0100000, 0x1000000, 0x0300000, 0x040000);
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(Mem, 0, nLen);										// blank all memory
	MemIndex(0x0100000, 0x1000000, 0x0300000, 0x040000);

	// Load and byte-swap 68000 Program roms
	nRet = BurnLoadRom(Rom68K + 0x000001, 0, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Rom68K + 0x000000, 1, 2); if (nRet != 0) return 1;

	// Load Gfx
	UINT8 * tmpGfx = (UINT8 *)BurnMalloc(0x0400000);
	for (INT32 i=0; i<3; i++) {
		BurnLoadRom(tmpGfx, i+2, 1);
		loadDecodeGfx( tmpGfx, 0x0400000 / 2, i*2 );
	}
	BurnFree(tmpGfx);

	// Leave 1MB empty (addressable by the chip)
	BurnLoadRom(X1010SNDROM + 0x100000, 5, 1);

	{
		SekInit(0, 0x68000);										// Allocate 68000
		SekOpen(0);

		// Map 68000 memory:
		SekMapMemory(Rom68K,		0x000000, 0x0FFFFF, MAP_ROM);	// CPU 0 ROM
		SekMapMemory(Ram68K,		0x200000, 0x20FFFF, MAP_RAM);	// CPU 0 RAM

		SekMapMemory(RamUnknown + 0x00000,	
									0x210000, 0x23FFFF, MAP_RAM);
		SekMapMemory(RamUnknown + 0x30000,	
									0x300000, 0x30FFFF, MAP_RAM);

		SekMapMemory((UINT8 *)RamSpr,
									0xB00000, 0xB3FFFF, MAP_RAM);	// sprites
		SekMapMemory((UINT8 *)RamPal,
									0xB40000, 0xB4FFFF, MAP_ROM);	// Palette
		SekMapMemory((UINT8 *)RamVReg,
									0xB60000, 0xB6003F, MAP_RAM);	// Video Registers
		SekMapMemory((UINT8 *)RamTMP68301,
									0xFFFC00, 0xFFFFFF, MAP_ROM);	// TMP68301 Registers

		SekMapHandler(1,			0xA00000, 0xA03FFF, MAP_READ | MAP_WRITE);
		SekMapHandler(2,			0xB40000, 0xB4FFFF, MAP_WRITE);	// Palette
		SekMapHandler(3,			0xB60000, 0xB6003F, MAP_WRITE);
		SekMapHandler(4,			0xFFFC00, 0xFFFFFF, MAP_WRITE);

		SekSetReadWordHandler(0, penbrosReadWord);
		SekSetReadByteHandler(0, penbrosReadByte);
		SekSetWriteWordHandler(0, penbrosWriteWord);
		SekSetWriteByteHandler(0, penbrosWriteByte);

		SekSetReadWordHandler (1, setaSoundRegReadWord);
		SekSetReadByteHandler (1, setaSoundRegReadByte);
		SekSetWriteWordHandler(1, setaSoundRegWriteWord);
		SekSetWriteByteHandler(1, setaSoundRegWriteByte);

		SekSetWriteWordHandler(2, grdiansPaletteWriteWord);
		SekSetWriteByteHandler(2, grdiansPaletteWriteByte);

		SekSetWriteWordHandler(3, setaVideoRegWriteWord);

		SekSetWriteWordHandler(4, Tmp68301WriteWord);
		SekSetWriteByteHandler(4, Tmp68301WriteByte);

		SekSetIrqCallback( grdiansSekIrqCallback );

		SekClose();
	}

	GenericTilesInit();

	x1010_sound_init(50000000 / 3, 0x0000);
	x1010_set_route(BURN_SND_X1010_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	x1010_set_route(BURN_SND_X1010_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);

	DrvDoReset();

	return 0;
}

// -- gundamex -----------------------------------------------------------

void __fastcall gundamexWriteWord(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x70000c: // watchdog
		case 0x800000: // coin lockout
		return;

		case 0xe00010:
		case 0xe00012:
		case 0xe00014:
		case 0xe00016:
		case 0xe00018:
		case 0xe0001a:
		case 0xe0001c:
		case 0xe0001e:
			x1010_sound_bank_w( (address & 0x0f) >> 1, data );
		return;

		case 0xfffd0a:
			EEPROMWrite(data & 0x02, data & 0x04, data & 0x01);
		return;
	}

	if ((address & 0xfffc00) == 0xfffc00) {
		Tmp68301WriteWord(address, data);
		return;
	}
}

void __fastcall gundamexWriteByte(UINT32 address, UINT8 data)
{
	if ((address & 0xfffc00) == 0xfffc00) {
		Tmp68301WriteByte(address, data);
		return;
	}
}

UINT16 __fastcall gundamexReadWord(UINT32 address)
{
	switch (address)
	{
		case 0x600000:
			return 0xff00 | DrvInput[3];

		case 0x600002:
			return 0xff00 | DrvInput[4];

		case 0x700000:
			return 0xffff ^ DrvInput[0];

		case 0x700002:
			return 0xffff ^ DrvInput[1];

		case 0x700004:
			return ((0xffff ^ DrvInput[2]) & ~0x20) | (DrvInput[7] & 0x20);

		case 0x700008:
			return 0xffff ^ DrvInput[5];

		case 0x70000a:
			return 0xffff ^ DrvInput[6];

		case 0xfffd0a:
			return ((EEPROMRead() & 1)) << 3;
	}

	if ((address & 0xfffc00) == 0xfffc00) {
		return BURN_ENDIAN_SWAP_INT16(RamTMP68301[(address & 0x3ff) / 2]);
	}

	return 0;
}

static INT32 gundamexInit()
{
	INT32 nRet;

	Mem = NULL;
	MemIndex(0x0300000, 0x2000000, 0x0300000, 0x010000);
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(Mem, 0, nLen);										// blank all memory
	MemIndex(0x0300000, 0x2000000, 0x0300000, 0x010000);

	// Load and byte-swap 68000 Program roms
	nRet = BurnLoadRom(Rom68K + 0x000001, 0, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Rom68K + 0x000000, 1, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Rom68K + 0x100001, 2, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Rom68K + 0x100000, 3, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Rom68K + 0x200000, 4, 0); if (nRet != 0) return 1;

	UINT8 * tmpGfx = (UINT8 *)BurnMalloc(0x0600000);
	nRet = BurnLoadRom(tmpGfx + 0x0000000,  5, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(tmpGfx + 0x0200000,  6, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(tmpGfx + 0x0400000,  7, 1); if (nRet != 0) return 1;
	loadDecodeGfx(tmpGfx, 0x600000 / 2, 0);
	nRet = BurnLoadRom(tmpGfx + 0x0000000,  8, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(tmpGfx + 0x0200000,  9, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(tmpGfx + 0x0400000, 10, 1); if (nRet != 0) return 1;
	loadDecodeGfx(tmpGfx, 0x600000 / 2, 2);
	nRet = BurnLoadRom(tmpGfx + 0x0000000, 11, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(tmpGfx + 0x0200000, 12, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(tmpGfx + 0x0400000, 13, 1); if (nRet != 0) return 1;
	loadDecodeGfx(tmpGfx, 0x600000 / 2, 4);
	
	BurnFree(tmpGfx);

	BurnLoadRom(X1010SNDROM + 0x100000, 14, 1);

	{
		SekInit(0, 0x68000);										// Allocate 68000
		SekOpen(0);

		// Map 68000 memory:
		SekMapMemory(Rom68K,				0x000000, 0x1FFFFF, MAP_ROM);	// CPU 0 ROM
		SekMapMemory(Ram68K,				0x200000, 0x20FFFF, MAP_RAM);	// CPU 0 RAM
		SekMapMemory(Rom68K + 0x200000,			0x500000, 0x57FFFF, MAP_ROM);	// CPU 0 ROM

		SekMapMemory((UINT8 *)RamSpr,		0xc00000, 0xc3FFFF, MAP_RAM);	// sprites
		SekMapMemory((UINT8 *)RamPal,		0xc40000, 0xc4FFFF, MAP_ROM);	// Palette
		SekMapMemory(RamUnknown + 0x00000,		0xc50000, 0xc5FFFF, MAP_RAM);
		SekMapMemory((UINT8 *)RamVReg,		0xc60000, 0xc6003F, MAP_RAM);	// Video Registers

		SekMapHandler(1,			0xb00000, 0xb03FFF, MAP_READ | MAP_WRITE);
		SekMapHandler(2,			0xc40000, 0xc4FFFF, MAP_WRITE);
		SekMapHandler(3,			0xc60000, 0xc6003F, MAP_WRITE);

		SekSetReadWordHandler(0, gundamexReadWord);
		//SekSetReadByteHandler(0, gundamexReadByte);
		SekSetWriteWordHandler(0, gundamexWriteWord);
		SekSetWriteByteHandler(0, gundamexWriteByte);

		SekSetReadWordHandler (1, setaSoundRegReadWord);
		SekSetReadByteHandler (1, setaSoundRegReadByte);
		SekSetWriteWordHandler(1, setaSoundRegWriteWord);
		SekSetWriteByteHandler(1, setaSoundRegWriteByte);

		SekSetWriteWordHandler(2, grdiansPaletteWriteWord);
		SekSetWriteByteHandler(2, grdiansPaletteWriteByte);

		SekSetWriteWordHandler(3, setaVideoRegWriteWord);

		SekSetIrqCallback( grdiansSekIrqCallback );

		SekClose();
	}

	GenericTilesInit();

	M68K_CYCS = 32530470 / 2;
	x1010_sound_init(M68K_CYCS, 0x0000);
	x1010_set_route(BURN_SND_X1010_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	x1010_set_route(BURN_SND_X1010_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);

	EEPROMInit(&eeprom_interface_93C46);

	DrvDoReset();

	return 0;
}

// -- samshoot -----------------------------------------------------------

void __fastcall samshootWriteWord(UINT32 address, UINT16 data)
{
	if ((address & 0xfffff0) == 0x400300) {
		x1010_sound_bank_w( (address & 0x0f) >> 1, data );
		return;
	}
}	

UINT16 __fastcall samshootReadWord(UINT32 address)
{
	switch (address)
	{
		case 0x400000:
			return 0xff00 | DrvInput[6];

		case 0x400002:
			return 0xff00 | DrvInput[0];

		case 0x500000:
			return DrvAnalogInput[0] | (DrvAnalogInput[1] << 8);

		case 0x580000:
			return DrvAnalogInput[2] | (DrvAnalogInput[3] << 8);

		case 0x700000:
			return 0xff00 | DrvInput[1];

		case 0x700002:
			return 0xff00 | DrvInput[2];

		case 0x700004:
			return 0xff00 | DrvInput[3];

		case 0x700006:
			return 0; // watchdog

		case 0xfffd0a:
			return 0xff00 | DrvInput[7];
	}

	return 0;
}

static INT32 samshootInit(INT32 game_id)
{
	INT32 nRet;
	
	HasNVRam = 1;
	is_samshoot = game_id;

	Mem = NULL;
	MemIndex(0x0200000, 0x2000000, 0x0500000, 0x010000);
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(Mem, 0, nLen);
	MemIndex(0x0200000, 0x2000000, 0x0500000, 0x010000);

	// Load and byte-swap 68000 Program roms
	nRet = BurnLoadRom(Rom68K + 0x000001, 0, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Rom68K + 0x000000, 1, 2); if (nRet != 0) return 1;

	UINT8 * tmpGfx = (UINT8 *)BurnMalloc(0x0800000);

	nRet = BurnLoadRom(tmpGfx, 2, 1); if (nRet != 0) return 1;
	loadDecodeGfx(tmpGfx, 0x800000 / 2, 0);
	nRet = BurnLoadRom(tmpGfx, 3, 1); if (nRet != 0) return 1;
	loadDecodeGfx(tmpGfx, 0x800000 / 2, 2);
	nRet = BurnLoadRom(tmpGfx, 4, 1); if (nRet != 0) return 1;
	loadDecodeGfx(tmpGfx, 0x800000 / 2, 4);
	nRet = BurnLoadRom(tmpGfx, 5, 1); if (nRet != 0) return 1;
	loadDecodeGfx(tmpGfx, 0x800000 / 2, 6);
	
	BurnFree(tmpGfx);

	BurnLoadRom(X1010SNDROM + 0x100000, 6, 1);

	{
		SekInit(0, 0x68000);										// Allocate 68000
		SekOpen(0);

		// Map 68000 memory:
		SekMapMemory(Rom68K,				0x000000, 0x1FFFFF, MAP_ROM);	// CPU 0 ROM
		SekMapMemory(Ram68K,				0x200000, 0x20FFFF, MAP_RAM);	// CPU 0 RAM

		SekMapMemory(RamNV + 0x00000,		0x300000, 0x30FFFF, MAP_RAM);

		SekMapMemory((UINT8 *)RamSpr,		0x800000, 0x83FFFF, MAP_RAM);	// sprites
		SekMapMemory((UINT8 *)RamPal,		0x840000, 0x84FFFF, MAP_ROM);	// Palette
		SekMapMemory((UINT8 *)RamVReg,		0x860000, 0x86003F, MAP_READ | MAP_FETCH);	// Video Registers
		SekMapMemory((UINT8 *)RamTMP68301,	0xFFFC00, 0xFFFFFF, MAP_ROM);	// TMP68301 Registers

		SekMapHandler(1,			0x900000, 0x903FFF, MAP_READ | MAP_WRITE);
		SekMapHandler(2,			0x840000, 0x84FFFF, MAP_WRITE);	// Palette
		SekMapHandler(3,			0x860000, 0x86003F, MAP_WRITE);
		SekMapHandler(4,			0xFFFC00, 0xFFFFFF, MAP_RAM);

		SekSetReadWordHandler(0, samshootReadWord);
		SekSetWriteWordHandler(0, samshootWriteWord);

		SekSetReadWordHandler (1, setaSoundRegReadWord);
		SekSetReadByteHandler (1, setaSoundRegReadByte);
		SekSetWriteWordHandler(1, setaSoundRegWriteWord);
		SekSetWriteByteHandler(1, setaSoundRegWriteByte);

		SekSetWriteWordHandler(2, grdiansPaletteWriteWord);
		SekSetWriteByteHandler(2, grdiansPaletteWriteByte);

		SekSetWriteWordHandler(3, setaVideoRegWriteWord);

		SekSetWriteWordHandler(4, Tmp68301WriteWord);
		SekSetWriteByteHandler(4, Tmp68301WriteByte);
		SekSetReadWordHandler(4, Tmp68301ReadWord);
		SekSetReadByteHandler(4, Tmp68301ReadByte);

		SekSetIrqCallback( grdiansSekIrqCallback );

		SekClose();
	}

	GenericTilesInit();

	x1010_sound_init(50000000 / 3, 0x0000);
	x1010_set_route(BURN_SND_X1010_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	x1010_set_route(BURN_SND_X1010_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);

	BurnGunInit(2, true);

	DrvDoReset();

	return 0;
}

// is_samshoot = [0 deerhunt, 1 turkhunt, 2 trophyh, 3 wschamp]

static INT32 deerhuntInit()
{
	return samshootInit(0);
}

static INT32 turkhuntInit()
{
	return samshootInit(1);
}

static INT32 trophyhInit()
{
	return samshootInit(2);
}

static INT32 wschampInit()
{
	return samshootInit(3);
}


static INT32 grdiansExit()
{
	SekExit();
	x1010_exit();

	GenericTilesExit();

	BurnFree(Mem);

	if (!strcmp(BurnDrvGetTextA(DRV_NAME), "gundamex")) {
		EEPROMExit();
	}

	if (nBurnGunNumPlayers) BurnGunExit();
	
	HasNVRam = 0;
	is_samshoot = 0;
	grdiansbl = 0;

	M68K_CYCS = 50000000 / 3;

//	bMahjong = 0;

	return 0;
}

inline static void drawgfx_line(const rectangle &cliprect, INT32 which_gfx, INT32 code, const UINT32 realcolor, INT32 flipx, INT32 flipy, INT32 base_sx, UINT32 xzoom, INT32 use_shadow, INT32 screenline, INT32 line, INT32 opaque)
{
	const UINT8 *addr = RomGfx + ((code * 8 * 8) % nRomGfxLen);

	struct drawmodes
	{
		INT32 gfx_mask;
		INT32 gfx_shift;
		INT32 shadow;
	};

	// this is the same logic as ssv.cpp, although this has more known cases, but also some bugs with the handling
	static const drawmodes BPP_MASK_TABLE[8] = {
		{ 0xff, 0, 4 }, // 0: ultrax, twineag2 text - is there a local / global mixup somewhere, or is this an 'invalid' setting that just enables all planes?
		{ 0x30, 4, 2 }, // 1: unverified case, mimic old driver behavior of only using lowest bit  (myangel2 question bubble, myangel endgame)
		{ 0x07, 0, 3 }, // 2: unverified case, mimic old driver behavior of only using lowest bit  (myangel "Graduate Tests")
		{ 0xff, 0, 0 }, // 3: unverified case, mimic old driver behavior of only using lowest bit  (staraudi question bubble: pen %00011000 with shadow on!)
		{ 0x0f, 0, 3 }, // 4: eagle shot 4bpp birdie text
		{ 0xf0, 4, 4 }, // 5: eagle shot 4bpp japanese text
		{ 0x3f, 0, 5 }, // 6: common 6bpp case + keithlcy (logo), drifto94 (wheels) masking  ) (myangel sliding blocks test)
		{ 0xff, 0, 8 }, // 7: common 8bpp case
	};

	INT32 shadow = BPP_MASK_TABLE[(which_gfx & 0x0700)>>8].shadow;
	INT32 gfx_mask = BPP_MASK_TABLE[(which_gfx & 0x0700)>>8].gfx_mask;
	INT32 gfx_shift = BPP_MASK_TABLE[(which_gfx & 0x0700)>>8].gfx_shift;

	if (!use_shadow)
		shadow = 0;

	UINT16* dest = pTransDraw + (screenline * nScreenWidth);

	INT32 minx = cliprect.min_x << 16;
	INT32 maxx = cliprect.max_x << 16;

	if (xzoom < 0x10000) // shrink
	{
		INT32 x0 = flipx ? (base_sx + (8 * xzoom) - xzoom) : (base_sx);
		INT32 x1 = flipx ? (base_sx - xzoom) : (x0 + (8 * xzoom));
		const INT32 dx = flipx ? (-xzoom) : (xzoom);

		const UINT8 *const source = flipy ? addr + (7 - line) * 8 : addr + line * 8;
		INT32 column = 0;

		for (INT32 sx = x0; sx != x1; sx += dx)
		{
			UINT8 pen = (source[column++] & gfx_mask) >> gfx_shift;

			if (sx >= minx && sx <= maxx)
			{
				INT32 realsx = sx >> 16;

				if (pen || opaque)
				{
					if (!shadow)
					{
						dest[realsx] = (realcolor + pen) & 0x7fff;
					}
					else
					{
						INT32 pen_shift = 15 - shadow;
						INT32 pen_mask = (1 << pen_shift) - 1;
						dest[realsx] = ((dest[realsx] & pen_mask) | (pen << pen_shift)) & 0x7fff;
					}
				}
			}
		}
	}
	else // enlarge or no zoom
	{
		const UINT8* const source = flipy ? addr + (7 - line) * 8 : addr + line * 8;

		INT32 x0 = (base_sx);
		INT32 x1 = (x0 + (8 * xzoom));

		INT32 column;
		if (!flipx)
		{
			column = 0;
		}
		else
		{
			column = 7;
		}

		UINT32 countx = 0;
		for (INT32 sx = x0; sx < x1; sx += 0x10000)
		{
			UINT8 pen = (source[column] & gfx_mask) >> gfx_shift;

			if (sx >= minx && sx <= maxx)
			{
				INT32 realsx = sx >> 16;

				if (pen || opaque)
				{
					if (!shadow)
					{
						dest[realsx] = (realcolor + pen) & 0x7fff;
					}
					else
					{
						INT32 pen_shift = 15 - shadow;
						INT32 pen_mask = (1 << pen_shift) - 1;
						dest[realsx] = ((dest[realsx] & pen_mask) | (pen << pen_shift)) & 0x7fff;
					}
				}
			}

			countx += 0x10000;
			if (countx >= xzoom)
			{
				if (!flipx)
				{
					column++;
				}
				else
				{
					column--;
				}

				countx -= xzoom;
			}
		}
	}
}

inline static void get_tile(UINT16 *spriteram, INT32 is_16x16, INT32 x, INT32 y, INT32 page, INT32& code, INT32& attr, INT32& flipx, INT32& flipy, INT32& color)
{
	INT32 xtile = x >> (is_16x16 ? 4 : 3);
	INT32 ytile = y >> (is_16x16 ? 4 : 3);

	// yes the tilemap in RAM is flipped?!
	ytile ^= 0x1f;

	//UINT16 *s3 = &spriteram[2 * ((page * 0x2000 / 4) + ((ytile & 0x1f) << 6) + ((xtile) & 0x03f))];
	UINT16 *s3 = spriteram + ((page * 0x2000 / 4) + ((ytile & 0x1f) << 6) + ((xtile) & 0x03f)) * 2;
	attr = BURN_ENDIAN_SWAP_INT16(s3[0]);
	code = BURN_ENDIAN_SWAP_INT16(s3[1]) + ((attr & 0x0007) << 16);
	flipx = (attr & 0x0010);
	flipy = (attr & 0x0008);
	color = (attr & 0xffe0) >> 5;
	if (is_16x16)
	{
		code &= ~3;

		if (!flipx)
		{
			if (x & 8)
			{
				code += 1;
			}
		}
		else
		{
			if (!(x & 8))
			{
				code += 1;
			}
		}

		if (!flipy)
		{
			if (y & 8)
			{
				code += 2;
			}
		}
		else
		{
			if (!(y & 8))
			{
				code += 2;
			}
		}
	}
}

static INT32 calculate_global_xoffset(INT32 nozoom_fixedpalette_fixedposition)
{
	INT32 global_xoffset = 0;

	if (nozoom_fixedpalette_fixedposition)
		global_xoffset = 0x80;

	return global_xoffset;
}

static INT32 calculate_global_yoffset(INT32 nozoom_fixedpalette_fixedposition)
{
	INT32 global_yoffset = 0;

	if (nozoom_fixedpalette_fixedposition)
		global_yoffset = -0x90;

	return global_yoffset;
}

static void draw_sprites_line(const rectangle &cliprect, INT32 scanline, INT32 realscanline, INT32 xoffset, UINT32 xzoom, bool xzoominverted)
{
	UINT16 *spriteram = RamSpr;

	UINT16 *s1  = RamSprPriv;

	INT32 sprite_debug_count = 0;

	for ( ; s1 < RamSprPriv + 0x1000/2; s1+=4,sprite_debug_count++ ) {
		INT32 num = BURN_ENDIAN_SWAP_INT16(s1[0]);

		INT32 xoffs = BURN_ENDIAN_SWAP_INT16(s1[1]);
		INT32 yoffs = BURN_ENDIAN_SWAP_INT16(s1[2]);
		INT32 sprite = BURN_ENDIAN_SWAP_INT16(s1[3]);

		// Single-sprite address
		UINT16 *s2 = spriteram + (sprite & 0x7fff) * 4;
		UINT16 *end = spriteram + 0x040000 / 2;

		// Single-sprite tile size
		INT32 global_sizex = xoffs & 0xfc00;
		INT32 global_sizey = yoffs & 0xfc00;

		INT32 nozoom_fixedpalette_fixedposition = num & 0x4000; // ignore various things including global offsets, zoom.  different palette selection too?
		bool opaque = num & 0x2000;
		INT32 use_global_size = num & 0x1000;
		INT32 use_shadow = num & 0x0800;
		INT32 which_gfx = num & 0x0700;
		xoffs &= 0x3ff;
		yoffs &= 0x3ff;

		if (yoffs & 0x200)
			yoffs -= 0x400;

		INT32 global_xoffset = calculate_global_xoffset(nozoom_fixedpalette_fixedposition);
		INT32 global_yoffset = calculate_global_yoffset(nozoom_fixedpalette_fixedposition);
		INT32 usedscanline;
		INT32 usedxoffset;
		UINT32 usedxzoom;

		if (nozoom_fixedpalette_fixedposition)
		{
			use_shadow = 0;
		//  which_gfx = 4 << 8;
			usedscanline = realscanline; // no zooming?
			usedxzoom = 0x10000;
			usedxoffset = 0;
		}
		else
		{
			usedscanline = scanline;
			usedxzoom = xzoom;
			usedxoffset = xoffset;
		}

		// Number of single-sprites
		num = (num & 0x00ff) + 1;

		if ((sprite&0x7fff) < 0x3000 / 2 / 4)
		{

			for( ; num > 0; num--,s2+=4 ) {
				if (s2 >= end)	break;

				if (sprite & 0x8000) {
					INT32 sy       = BURN_ENDIAN_SWAP_INT16(s2[1]) & 0x3ff;
					sy += global_yoffset;
					sy &= 0x3ff;
					if (sy & 0x200)
						sy -= 0x400;
					INT32 local_sizey = BURN_ENDIAN_SWAP_INT16(s2[1]) & 0xfc00;
					INT32 height = use_global_size ? global_sizey : local_sizey;
					height = ((height & 0xfc00) >> 10) + 1;
					INT32 firstline = (sy + yoffs) & 0x3ff;
					if (firstline & 0x200)
						firstline -= 0x400;
					INT32 endline = firstline + height * 0x10 - 1;

					// if the sprite doesn't cover this scanline, bail now
					if (endline & 0x200)
						endline -= 0x400;

					if (endline >= firstline)
					{
						if (firstline > usedscanline)    continue;
						if (endline < usedscanline)    continue;
					}
					else
					{
						// cases where the sprite crosses 0
						if ((usedscanline > endline) && (usedscanline < firstline))
							continue;
					}

					// get everything we need to calculate if sprite is actually within the x co-ordinates of the screen
					INT32 sx = BURN_ENDIAN_SWAP_INT16(s2[0]);
					INT32 local_sizex = sx & 0xfc00;
					sx &= 0x3ff;
					sx -= global_xoffset;

					INT32 width    = use_global_size ? global_sizex : local_sizex;

					width  = ((width  & 0xfc00) >> 10)/* + 1*/;	// reelquak reels
					if (!width)
						continue;

					INT32 firstcolumn = (sx + xoffs);
					firstcolumn = (firstcolumn & 0x1ff) - (firstcolumn & 0x200);
					INT32 lastcolumn = firstcolumn + width * 0x10 - 1;

					// if the sprite isn't within the x-coordinates of the screen, bail
					if (firstcolumn > cliprect.max_x)    continue;
					if (lastcolumn < cliprect.min_x)    continue;

					// otherwise get the rest of the things we need to draw
					INT32 scrolly = BURN_ENDIAN_SWAP_INT16(s2[3]);
					scrolly &= 0x1ff;
					scrolly += global_yoffset;
					INT32 sourceline = (usedscanline - scrolly) & 0x1ff;

					INT32 scrollx = BURN_ENDIAN_SWAP_INT16(s2[2]);
					INT32 is_16x16 = (scrollx & 0x8000) >> 15;
					INT32 page = (scrollx & 0x7c00) >> 10;
					scrollx &= 0x3ff;

					// we treat 16x16 tiles as 4 8x8 tiles, so while the tilemap is 0x40 tiles wide in memory, that becomes 0x80 tiles in 16x16 mode, with the data wrapping in 8x8 mode
					for (INT32 x = 0; x < 0x80; x++)
					{
						INT32 code, attr, flipx, flipy, color;
						// tilemap data is NOT buffered?
						get_tile(spriteram, is_16x16, x * 8, sourceline, page, code, attr, flipx, flipy, color);

						INT32 tileline = sourceline & 0x07;
						INT32 dx = sx + (scrollx & 0x3ff) + xoffs + 0x10;
						INT32 px = (((dx + x * 8) + 0x10) & 0x3ff) - 0x10;
						INT32 dst_x = px & 0x3ff;
						dst_x = (dst_x & 0x1ff) - (dst_x & 0x200);

						if ((dst_x >= firstcolumn - 8) && (dst_x <= lastcolumn)) // reelnquak reels are heavily glitched without this check
						{
							UINT32 realsx = dst_x;
							realsx -= usedxoffset>>16; // need to refactor, this causes loss of lower 16 bits of offset which are important in zoomed cases for precision
							realsx = realsx * usedxzoom;
							drawgfx_line(cliprect, which_gfx, code, color << 4, flipx, flipy, realsx, usedxzoom, use_shadow, realscanline, tileline, opaque);
						}
					}

				} else {
					// "normal" sprite
					INT32 sy    = BURN_ENDIAN_SWAP_INT16(s2[1]) & 0x1ff;

					if (sy & 0x100)
						sy -= 0x200;

					sy += global_yoffset;
					sy &= 0x3ff;

					if (realscanline == 128)
					{
					//	printf("%04x %02x %d %d\n", sprite_debug_count, num, yoffs, sy);
					}

					INT32 sizey = use_global_size ? global_sizey : BURN_ENDIAN_SWAP_INT16(s2[1]) & 0xfc00;

					sizey = (1 << ((sizey & 0x0c00) >> 10)) - 1;

					INT32 firstline = (sy + yoffs) & 0x3ff;
					INT32 endline = (firstline + (sizey + 1) * 8) - 1;

					endline &= 0x3ff;

					if (firstline & 0x200)
						firstline -= 0x400;

					if (endline & 0x200)
						endline -= 0x400;

					// if the sprite doesn't cover this scanline, bail now
					if (endline >= firstline)
					{
						if (firstline > usedscanline)    continue;
						if (endline < usedscanline)    continue;
					}
					else
					{
						// cases where the sprite crosses 0
						if ((usedscanline > endline) && (usedscanline < firstline))
							continue;
					}

					// otherwise get the rest of the things we need to draw
					INT32 attr = BURN_ENDIAN_SWAP_INT16(s2[2]);
					INT32 code = BURN_ENDIAN_SWAP_INT16(s2[3]) + ((attr & 0x0007) << 16);
					INT32 flipx = (attr & 0x0010);
					INT32 flipy = (attr & 0x0008);
					INT32 color = (attr & 0xffe0) >> 5;

					INT32 sx = BURN_ENDIAN_SWAP_INT16(s2[0]);
					INT32 sizex = use_global_size ? global_sizex : sx;
					sizex = (1 << ((sizex & 0x0c00) >> 10)) - 1;

					sx += xoffs;
					sx = (sx & 0x1ff) - (sx & 0x200);
					sx -= global_xoffset;

					INT32 basecode = code &= ~((sizex + 1) * (sizey + 1) - 1);   // see myangel, myangel2 and grdians


					INT32 line = usedscanline - firstline;
					INT32 y = (line >> 3);
					line &= 0x7;

					if (nozoom_fixedpalette_fixedposition)
					{
						// grdians map...
						color = 0x7ff;
					}

					for (INT32 x = 0; x <= sizex; x++)
					{
						INT32 realcode = (basecode + (flipy ? sizey - y : y)*(sizex + 1)) + (flipx ? sizex - x : x);
						UINT32 realsx = (sx + x * 8);
						realsx -= usedxoffset>>16; // need to refactor, this causes loss of lower 16 bits of offset which are important in zoomed cases for precision
						realsx = realsx * usedxzoom;
						drawgfx_line(cliprect, which_gfx, realcode, color << 4, flipx, flipy, realsx, usedxzoom, use_shadow, realscanline, line, opaque);
					}
				}
			}
		}
		if (BURN_ENDIAN_SWAP_INT16(s1[0]) & 0x8000) break;	// end of list marker
	}
}

static void draw_sprites(const rectangle &cliprect)
{
	UINT32 yoffset = (BURN_ENDIAN_SWAP_INT16(RamVReg[0x1a / 2]) << 16) | BURN_ENDIAN_SWAP_INT16(RamVReg[0x18 / 2]);
	yoffset &= 0x07ffffff;
	yoffset = 0x07ffffff - yoffset;

	// TODO the global yscroll should be applied here too as it can be sub-pixel for precision in zoomed cases
	UINT32 yzoom = (BURN_ENDIAN_SWAP_INT16(RamVReg[0x1e / 2]) << 16) | BURN_ENDIAN_SWAP_INT16(RamVReg[0x1c / 2]);
	yzoom &= 0x07ffffff;
	bool yzoominverted = false;
	bool xzoominverted = false;

	if (yzoom & 0x04000000)
	{
		yzoom = 0x8000000 - yzoom;
		yzoominverted = true;
	}

	INT32 xoffset = (BURN_ENDIAN_SWAP_INT16(RamVReg[0x12 / 2]) << 16) | BURN_ENDIAN_SWAP_INT16(RamVReg[0x10 / 2]);
	xoffset &= 0x07ffffff;

	if (xoffset & 0x04000000)
		xoffset -= 0x08000000;

	UINT32 xzoom = (BURN_ENDIAN_SWAP_INT16(RamVReg[0x16 / 2]) << 16) | BURN_ENDIAN_SWAP_INT16(RamVReg[0x14 / 2]);

	if (xzoom & 0x04000000)
	{
		xzoom = 0x8000000 - xzoom;
		xzoominverted = true;
	}

	if (!xzoom)
		return;

	UINT64 inc = 0x100000000ULL;

	UINT32 inc2 = inc / xzoom;

	for (INT32 y = cliprect.min_y; y <= cliprect.max_y; y++)
	{
		rectangle tempcliprect = cliprect;

		tempcliprect.min_y = y;
		tempcliprect.max_y = y;

		INT32 yy;

		if (!yzoominverted)
		{
			yy = y; // not handled yet (this is using negative yzoom to do flipscreen...)
		}
		else
		{
			yy = y * yzoom;
			yy += yoffset;
			yy &= 0x07ffffff;
			yy >>= 16;

			if (yy & 0x400)
				yy -= 0x800;
		}

		draw_sprites_line(tempcliprect, yy, y, xoffset, inc2, xzoominverted);
	}
}

static rectangle cliprect, defcliprect;

static INT32 lastline = 0;

static INT32 DrvDrawBegin()
{
	if (bRecalcPalette) {
		for (INT32 i=0;i<0x08000; i++)
			CurPal[i] = CalcCol( RamPal[i] );
		bRecalcPalette = 0;
	}

	if (!pBurnDraw) return 0;
	BurnTransferClear();
	GenericTilesGetClip(&cliprect.min_x, &cliprect.max_x, &cliprect.min_y, &cliprect.max_y);
	cliprect.max_x -= 1;
	cliprect.max_y -= 1;
	defcliprect = cliprect;

	cliprect.max_y = 0;
	lastline = 0;

	return 0;
}

static INT32 DrvDrawScanline(INT32 drawto)
{
	if (drawto < 1) return 0;
	cliprect.max_y = drawto;
	cliprect.min_y = lastline;
	lastline = drawto;
	if (BURN_ENDIAN_SWAP_INT16(RamVReg[0x30/2]) == 0) { // 1 = Blank Screen
		if (pBurnDraw)
			draw_sprites(cliprect);
	}

	return 0;
}

static INT32 DrvDrawEnd()
{
	DrvDrawScanline(defcliprect.max_y);
	BurnTransferCopy(CurPal);

	return 0;
}

static INT32 DrvDraw()
{
	DrvDrawBegin();

	if (BURN_ENDIAN_SWAP_INT16(RamVReg[0x30/2]) == 0) { // 1 = Blank Screen
		draw_sprites(cliprect);
	}

	DrvDrawEnd();

	return 0;
}

static INT32 grdiansFrame()
{
	if (DrvReset)														// Reset machine
		DrvDoReset();

	DrvInput[0] = 0x00;													// Joy1
	DrvInput[1] = 0x00;													// Joy2
	DrvInput[2] = 0x00;													// Buttons
	DrvInput[5] = 0x00;													// Joy3
	DrvInput[6] = 0x00;													// Joy4
//	DrvInput[7] = 0x00;													// Joy5

	for (INT32 i = 0; i < 8; i++) {
		DrvInput[0] |= (DrvJoy1[i] & 1) << i;
		DrvInput[1] |= (DrvJoy2[i] & 1) << i;
		DrvInput[2] |= (DrvButton[i] & 1) << i;
		DrvInput[5] |= (DrvJoy3[i] & 1) << i;
		DrvInput[6] |= (DrvJoy4[i] & 1) << i;
		DrvInput[6] |= (DrvJoy5[i] & 1) << i;
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal = (M68K_CYCS / 60) / nInterleave;
	INT32 nCyclesDone = nExtraCycles;
	INT32 nCyclesNext = 0;
	INT32 nCyclesExec = 0;

	SekNewFrame();

	SekOpen(0);
	DrvDrawBegin();
	for (INT32 i = 0; i < nInterleave; i++) {
		nCyclesNext += nCyclesTotal;
		nCyclesExec = SekRun( nCyclesNext - nCyclesDone );
		nCyclesDone += nCyclesExec;

		current_scanline = i; // this should be _before_ the SekRun() above, but the
		// raster chain won't kick-off.  probably something I overlooked.. -dink

		if (raster_en && i == raster_pos) {
			if (raster_extra) { // first raster line is "special"
				INT32 c = SekRun( nCyclesTotal / 2 ); // run 1/2 line
				nCyclesExec += c;
				nCyclesDone += c;
				raster_extra = 0;
			}
			tmp68301_update_irq_state(1);
			DrvDrawScanline(i);
		}

		for (INT32 j=0;j<3;j++) {
			if (tmp68301_timer[j]) {
				tmp68301_timer_counter[j] += nCyclesExec;
				if (tmp68301_timer_counter[j] >= tmp68301_timer[j]) {
					// timer[j] timeout !
					tmp68301_timer[j] = 0;
					tmp68301_timer_counter[j] = 0;
					nCyclesDone += SekRun(1); // avoid masking raster irq
					tmp68301_timer_callback(j);
				}
			}
		}

		if (i == 232-1) {
			if (pBurnDraw)
				DrvDrawEnd();

			tmp68301_update_irq_state(0);
		}
	}
	nExtraCycles = SekTotalCycles() - (M68K_CYCS / 60);

	SekClose();

	if (pBurnSoundOut)
		x1010_sound_update();

	return 0;
}

static INT32 samshootDraw()
{
	DrvDraw();

	BurnGunDrawTargets();

	return 0;
}

static INT32 samshootFrame()
{
	if (DrvReset)														// Reset machine
		DrvDoReset();

	{
		memset (DrvInput, 0xff, 5);

		for (INT32 i = 0; i < 8; i++) {
			DrvInput[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInput[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInput[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInput[3] ^= (DrvJoy4[i] & 1) << i;
			DrvInput[4] ^= (DrvJoy5[i] & 1) << i;
		}

		BurnGunMakeInputs(0, DrvAxis[0], DrvAxis[1]);
		BurnGunMakeInputs(1, DrvAxis[2], DrvAxis[3]);
		
		float x0 = (float)((BurnGunX[0] >> 8) + 8) / 320 * 160;
		float y0 = (float)((BurnGunY[0] >> 8) + 8) / 240 * 240;
		float x1 = (float)((BurnGunX[1] >> 8) + 8) / 320 * 160;
		float y1 = (float)((BurnGunY[1] >> 8) + 8) / 240 * 240;

		INT32 gun_offs[4][2] = { {0, 0}, {4, 15}, {0, 0}, {4, 14} };
		// is_samshoot = [0 deerhunt, 1 turkhunt, 2 trophyh, 3 wschamp]

		DrvAnalogInput[0] = (INT8)x0 + 36 + gun_offs[is_samshoot][0];
		DrvAnalogInput[1] = (INT8)y0 + 22 + gun_offs[is_samshoot][1];
		DrvAnalogInput[2] = (INT8)x1 + 36 + gun_offs[is_samshoot][0];
		DrvAnalogInput[3] = (INT8)y1 + 22 + gun_offs[is_samshoot][1];
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal = (M68K_CYCS / 60) / nInterleave;
	INT32 nCyclesDone = nExtraCycles;
	INT32 nCyclesNext = 0;
	INT32 nCyclesExec = 0;

	SekNewFrame();

	SekOpen(0);
	DrvDrawBegin();
	for (INT32 i = 0; i < nInterleave; i++) {
		nCyclesNext += nCyclesTotal;
		nCyclesExec = SekRun( nCyclesNext - nCyclesDone );
		nCyclesDone += nCyclesExec;

		current_scanline = i; // this should be _before_ the SekRun() above, but the
		// raster chain won't kick-off.  probably something I overlooked.. -dink

		if (raster_en && i == raster_pos) {
			if (raster_extra) { // first raster line is "special"
				INT32 c = SekRun( nCyclesTotal / 2 ); // run 1/2 line
				nCyclesExec += c;
				nCyclesDone += c;
				raster_extra = 0;
			}
			tmp68301_update_irq_state(1);
			DrvDrawScanline(i);
		}

		for (INT32 j=0;j<3;j++)
			if (tmp68301_timer[j]) {
				tmp68301_timer_counter[j] += nCyclesExec;
				if (tmp68301_timer_counter[j] >= tmp68301_timer[j]) {
					// timer[j] timeout !
					tmp68301_timer[j] = 0;
					tmp68301_timer_counter[j] = 0;
					tmp68301_timer_callback(j);
				}
			}

		if (i == 241-1) tmp68301_update_irq_state(2);

		if (i == 240-1) {
			if (pBurnDraw)
				DrvDrawEnd();

			tmp68301_update_irq_state(0);
		}
	}

	nExtraCycles = SekTotalCycles() - (M68K_CYCS / 60);

	SekClose();

	if (pBurnDraw) {
		BurnGunDrawTargets();
	}

	if (pBurnSoundOut)
		x1010_sound_update();

	return 0;
}

static INT32 grdiansScan(INT32 nAction,INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) 											// Return minimum compatible version
		*pnMin =  0x029671;

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = RamStart;
		ba.nLen	  = RamEnd - RamStart;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_NVRAM && HasNVRam && RamNV) {
		memset(&ba, 0, sizeof(ba));
		ba.Data = RamNV;
		ba.nLen = 0x10000;
		ba.szName = "SetaNVRam";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {

		// Scan 68000 state
		SekScan(nAction);
		
		x1010_scan(nAction, pnMin);

		if (nBurnGunNumPlayers) BurnGunScan();

		// Scan TMP 68301 Chip state
		SCAN_VAR(tmp68301_timer);
		SCAN_VAR(tmp68301_timer_counter);
		SCAN_VAR(tmp68301_irq_vector);

		// Raster stuff (grdians: fire in intro, heli-crash @ shortly after game start)
		SCAN_VAR(raster_extra);
		SCAN_VAR(raster_latch);
		SCAN_VAR(raster_pos);
		SCAN_VAR(raster_en);

		// Keep track of cycles between frames
		SCAN_VAR(nExtraCycles);

		if (nAction & ACB_WRITE) {

			// palette changed
			bRecalcPalette = 1;

			// x1-010 bank changed
			for (INT32 i=0; i<SETA_NUM_BANKS; i++)
				memcpy(X1010SNDROM + i * 0x20000, X1010SNDROM + 0x100000 + x1_010_chip->sound_banks[i] * 0x20000, 0x20000);
		}
	}

	return 0;
}

struct BurnDriver BurnDrvGrdians = {
	"grdians", NULL, NULL, NULL, "1995",
	"Guardians / Denjin Makai II (P-FG01-1 PCB)\0", NULL, "Winkysoft (Banpresto license)", "Newer Seta",
	L"Guardians\0\u96fb\u795e\u9b54\u5080\u2161 (P-FG01-1 PCB)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA2, GBF_SCRFIGHT, 0,
	NULL, grdiansRomInfo, grdiansRomName, NULL, NULL, NULL, NULL, grdiansInputInfo, grdiansDIPInfo,
	grdiansInit, grdiansExit, grdiansFrame, DrvDraw, grdiansScan, &bRecalcPalette, 0x8000,
	304, 232, 4, 3
};

struct BurnDriver BurnDrvGrdiansa = {
	"grdiansa", "grdians", NULL, NULL, "1995",
	"Guardians / Denjin Makai II (P0-113A PCB)\0", NULL, "Winkysoft (Banpresto license)", "Newer Seta",
	L"Guardians\0\u96fb\u795e\u9b54\u5080\u2161 (P0-113A PCB)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA2, GBF_SCRFIGHT, 0,
	NULL, grdiansaRomInfo, grdiansaRomName, NULL, NULL, NULL, NULL, grdiansInputInfo, grdiansDIPInfo,
	grdiansaInit, grdiansExit, grdiansFrame, DrvDraw, grdiansScan, &bRecalcPalette, 0x8000,
	304, 232, 4, 3
};

struct BurnDriver BurnDrvGrdiansbl = {
	"grdiansbl", "grdians", NULL, NULL, "1998",
	"Guardians / Denjin Makai II - Cho Kyoka Ban (bootleg)\0", NULL, "bootleg (Intac Japan)", "Newer Seta",
	L"Guardians\0\u96fb\u795e\u9b54\u5080\u2161 - \u8d85\u7d1a\u52a0\u5f37\u7248 (bootleg)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA2, GBF_SCRFIGHT, 0,
	NULL, grdiansblRomInfo, grdiansblRomName, NULL, NULL, NULL, NULL, grdiansInputInfo, grdiansDIPInfo,
	grdiansblInit, grdiansExit, grdiansFrame, DrvDraw, grdiansScan, &bRecalcPalette, 0x8000,
	304, 232, 4, 3
};

struct BurnDriver BurnDrvMj4simai = {
	"mj4simai", NULL, NULL, NULL, "1996",
	"Wakakusamonogatari Mahjong Yonshimai (Japan)\0", NULL, "Maboroshi Ware", "Newer Seta",
	L"\u82E5\u8349\u7269\u8A9E \u9EBB\u96C0\u56DB\u59C9\u59B9 (Japan)\0Wakakusamonogatari Mahjong Yonshimai\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 1, HARDWARE_SETA2, GBF_MAHJONG, 0,
	NULL, mj4simaiRomInfo, mj4simaiRomName, NULL, NULL, NULL, NULL, mj4simaiInputInfo, mj4simaiDIPInfo,
	mj4simaiInit, grdiansExit, grdiansFrame, DrvDraw, grdiansScan, &bRecalcPalette, 0x8000,
	384, 240, 4, 3
};

struct BurnDriver BurnDrvMyangel = {
	"myangel", NULL, NULL, NULL, "1996",
	"Kosodate Quiz My Angel (Japan)\0", NULL, "MOSS / Namco", "Newer Seta",
	L"\u5B50\u80B2\u3066\u30AF\u30A4\u30BA \u30DE\u30A4 \u30A8\u30F3\u30B8\u30A7\u30EB (Japan)\0Kosodate Quiz My Angel\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA2, GBF_QUIZ, 0,
	NULL, myangelRomInfo, myangelRomName, NULL, NULL, NULL, NULL, myangelInputInfo, myangelDIPInfo,
	myangelInit, grdiansExit, grdiansFrame, DrvDraw, grdiansScan, &bRecalcPalette, 0x8000,
	376, 240, 4, 3
};

struct BurnDriver BurnDrvMyangel2 = {
	"myangel2", NULL, NULL, NULL, "1997",
	"Kosodate Quiz My Angel 2 (Japan)\0", NULL, "MOSS / Namco", "Newer Seta",
	L"\u5B50\u80B2\u3066\u30AF\u30A4\u30BA \u30DE\u30A4 \u30A8\u30F3\u30B8\u30A7\u30EB \uFF12 (Japan)\0Kosodate Quiz My Angel 2\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA2, GBF_QUIZ, 0,
	NULL, myangel2RomInfo, myangel2RomName, NULL, NULL, NULL, NULL, myangelInputInfo, myangel2DIPInfo,
	myangel2Init, grdiansExit, grdiansFrame, DrvDraw, grdiansScan, &bRecalcPalette, 0x8000,
	376, 240, 4, 3
};

struct BurnDriver BurnDrvPzlbowl = {
	"pzlbowl", NULL, NULL, NULL, "1999",
	"Puzzle De Bowling (Japan)\0", NULL, "Nihon System / MOSS", "Newer Seta",
	L"Puzzle De Bowling (Japan)\0\u30D1\u30BA\u30EB \uFF24\uFF25 \u30DC\u30FC\u30EA\u30F3\u30B0\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA2, GBF_PUZZLE, 0,
	NULL, pzlbowlRomInfo, pzlbowlRomName, NULL, NULL, NULL, NULL, grdiansInputInfo, pzlbowlDIPInfo,
	pzlbowlInit, grdiansExit, grdiansFrame, DrvDraw, grdiansScan, &bRecalcPalette, 0x8000,
	384, 240, 4, 3
};

struct BurnDriver BurnDrvPenbros = {
	"penbros", NULL, NULL, NULL, "2000",
	"Penguin Brothers (Japan)\0", NULL, "Subsino", "Newer Seta",
	L"Penguin Brothers (Japan)\0\u30DA\u30F3\u30AE\u30F3 \u30D6\u30E9\u30B6\u30FC\u30BA\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA2, GBF_PLATFORM, 0,
	NULL, penbrosRomInfo, penbrosRomName, NULL, NULL, NULL, NULL, penbrosInputInfo, penbrosDIPInfo,
	penbrosInit, grdiansExit, grdiansFrame, DrvDraw, grdiansScan, &bRecalcPalette, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvAblast = {
	"ablast", "penbros", NULL, NULL, "2000",
	"Hong Tian Lei (A-Blast) (Japan)\0", NULL, "Subsino", "Newer Seta",
	L"Hong Tian Lei (A-Blast) (Japan)\0\u8f5f\u5929\u96f7\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA2, GBF_PLATFORM, 0,
	NULL, ablastRomInfo, ablastRomName, NULL, NULL, NULL, NULL, penbrosInputInfo, penbrosDIPInfo,
	penbrosInit, grdiansExit, grdiansFrame, DrvDraw, grdiansScan, &bRecalcPalette, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvGundamex = {
	"gundamex", NULL, NULL, NULL, "1994",
	"Mobile Suit Gundam EX Revue\0", NULL, "Banpresto", "Newer Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA2, GBF_VSFIGHT, 0,
	NULL, gundamexRomInfo, gundamexRomName, NULL, NULL, NULL, NULL, GundamexInputInfo, GundamexDIPInfo,
	gundamexInit, grdiansExit, grdiansFrame, DrvDraw, grdiansScan, &bRecalcPalette, 0x8000,
	384, 224, 4, 3
};

struct BurnDriver BurnDrvDeerhunt = {
	"deerhunt", NULL, NULL, NULL, "2000",
	"Deer Hunting USA V4.3\0", NULL, "Sammy USA Corporation", "Newer Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 1, HARDWARE_SETA2, GBF_SHOOT, 0,
	NULL, deerhuntRomInfo, deerhuntRomName, NULL, NULL, NULL, NULL, DeerhuntInputInfo, DeerhuntDIPInfo,
	deerhuntInit, grdiansExit, samshootFrame, samshootDraw, grdiansScan, &bRecalcPalette, 0x8000,
	320, 240, 4, 3
};

struct BurnDriver BurnDrvDeerhuna = {
	"deerhunta", "deerhunt", NULL, NULL, "2000",
	"Deer Hunting USA V4.2\0", NULL, "Sammy USA Corporation", "Newer Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 1, HARDWARE_SETA2, GBF_SHOOT, 0,
	NULL, deerhunaRomInfo, deerhunaRomName, NULL, NULL, NULL, NULL, DeerhuntInputInfo, DeerhuntDIPInfo,
	deerhuntInit, grdiansExit, samshootFrame, samshootDraw, grdiansScan, &bRecalcPalette, 0x8000,
	320, 240, 4, 3
};

struct BurnDriver BurnDrvDeerhunb = {
	"deerhuntb", "deerhunt", NULL, NULL, "2000",
	"Deer Hunting USA V4.0\0", NULL, "Sammy USA Corporation", "Newer Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 1, HARDWARE_SETA2, GBF_SHOOT, 0,
	NULL, deerhunbRomInfo, deerhunbRomName, NULL, NULL, NULL, NULL, DeerhuntInputInfo, DeerhuntDIPInfo,
	deerhuntInit, grdiansExit, samshootFrame, samshootDraw, grdiansScan, &bRecalcPalette, 0x8000,
	320, 240, 4, 3
};

struct BurnDriver BurnDrvDeerhunc = {
	"deerhuntc", "deerhunt", NULL, NULL, "2000",
	"Deer Hunting USA V3\0", NULL, "Sammy USA Corporation", "Newer Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 1, HARDWARE_SETA2, GBF_SHOOT, 0,
	NULL, deerhuncRomInfo, deerhuncRomName, NULL, NULL, NULL, NULL, DeerhuntInputInfo, DeerhuntDIPInfo,
	deerhuntInit, grdiansExit, samshootFrame, samshootDraw, grdiansScan, &bRecalcPalette, 0x8000,
	320, 240, 4, 3
};

struct BurnDriver BurnDrvDeerhund = {
	"deerhuntd", "deerhunt", NULL, NULL, "2000",
	"Deer Hunting USA V2\0", NULL, "Sammy USA Corporation", "Newer Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 1, HARDWARE_SETA2, GBF_SHOOT, 0,
	NULL, deerhundRomInfo, deerhundRomName, NULL, NULL, NULL, NULL, DeerhuntInputInfo, DeerhuntDIPInfo,
	deerhuntInit, grdiansExit, samshootFrame, samshootDraw, grdiansScan, &bRecalcPalette, 0x8000,
	320, 240, 4, 3
};

struct BurnDriver BurnDrvDeerhune = {
	"deerhunte", "deerhunt", NULL, NULL, "2000",
	"Deer Hunting USA V1\0", NULL, "Sammy USA Corporation", "Newer Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 1, HARDWARE_SETA2, GBF_SHOOT, 0,
	NULL, deerhuneRomInfo, deerhuneRomName, NULL, NULL, NULL, NULL, DeerhuntInputInfo, DeerhuntDIPInfo,
	deerhuntInit, grdiansExit, samshootFrame, samshootDraw, grdiansScan, &bRecalcPalette, 0x8000,
	320, 240, 4, 3
};

struct BurnDriver BurnDrvDeerhunj = {
	"deerhuntj", "deerhunt", NULL, NULL, "2000",
	"Deer Hunting USA V4.4.1 (Japan)\0", NULL, "Sammy USA Corporation", "Newer Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 1, HARDWARE_SETA2, GBF_SHOOT, 0,
	NULL, deerhunjRomInfo, deerhunjRomName, NULL, NULL, NULL, NULL, DeerhuntInputInfo, DeerhuntDIPInfo,
	deerhuntInit, grdiansExit, samshootFrame, samshootDraw, grdiansScan, &bRecalcPalette, 0x8000,
	320, 240, 4, 3
};

struct BurnDriver BurnDrvTurkhunt = {
	"turkhunt", NULL, NULL, NULL, "2001",
	"Turkey Hunting USA V1.00\0", NULL, "Sammy USA Corporation", "Newer Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 1, HARDWARE_SETA2, GBF_SHOOT, 0,
	NULL, turkhuntRomInfo, turkhuntRomName, NULL, NULL, NULL, NULL, DeerhuntInputInfo, TurkhuntDIPInfo,
	turkhuntInit, grdiansExit, samshootFrame, samshootDraw, grdiansScan, &bRecalcPalette, 0x8000,
	320, 240, 4, 3
};

struct BurnDriver BurnDrvWschamp = {
	"wschamp", NULL, NULL, NULL, "2001",
	"Wing Shooting Championship V2.00\0", NULL, "Sammy USA Corporation", "Newer Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA2, GBF_SHOOT, 0,
	NULL, wschampRomInfo, wschampRomName, NULL, NULL, NULL, NULL, WschampInputInfo, WschampDIPInfo,
	wschampInit, grdiansExit, samshootFrame, samshootDraw, grdiansScan, &bRecalcPalette, 0x8000,
	320, 240, 4, 3
};

struct BurnDriver BurnDrvWschampa = {
	"wschampa", "wschamp", NULL, NULL, "2001",
	"Wing Shooting Championship V1.01\0", NULL, "Sammy USA Corporation", "Newer Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA2, GBF_SHOOT, 0,
	NULL, wschampaRomInfo, wschampaRomName, NULL, NULL, NULL, NULL, WschampInputInfo, WschampDIPInfo,
	wschampInit, grdiansExit, samshootFrame, samshootDraw, grdiansScan, &bRecalcPalette, 0x8000,
	320, 240, 4, 3
};

struct BurnDriver BurnDrvWschampb = {
	"wschampb", "wschamp", NULL, NULL, "2001",
	"Wing Shooting Championship V1.00\0", NULL, "Sammy USA Corporation", "Newer Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA2, GBF_SHOOT, 0,
	NULL, wschampbRomInfo, wschampbRomName, NULL, NULL, NULL, NULL, WschampInputInfo, WschampDIPInfo,
	wschampInit, grdiansExit, samshootFrame, samshootDraw, grdiansScan, &bRecalcPalette, 0x8000,
	320, 240, 4, 3
};

struct BurnDriver BurnDrvTrophyh = {
	"trophyh", NULL, NULL, NULL, "2002",
	"Trophy Hunting - Bear & Moose V1.00\0", "Hangs are normal, just wait it out.", "Sammy USA Corporation", "Newer Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA2, GBF_SHOOT, 0,
	NULL, trophyhRomInfo, trophyhRomName, NULL, NULL, NULL, NULL, WschampInputInfo, TrophyhDIPInfo,
	trophyhInit, grdiansExit, samshootFrame, samshootDraw, grdiansScan, &bRecalcPalette, 0x8000,
	320, 240, 4, 3
};

static INT32 funcubeInit()
{
	return 1;
}

struct BurnDriverD BurnDrvFuncube2 = {
	"funcube2", NULL, NULL, NULL, "2001",
	"Funcube 2 (v1.1)\0", "Unemulated Sub CPU", "Namco", "Newer Seta",
	NULL, NULL, NULL, NULL,
	0 | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA2, GBF_PUZZLE, 0,
	NULL, funcube2RomInfo, funcube2RomName, NULL, NULL, NULL, NULL, grdiansInputInfo, grdiansDIPInfo,
	funcubeInit, grdiansExit, grdiansFrame, DrvDraw, grdiansScan, &bRecalcPalette, 0x8000,
	320, 240, 4, 3
};

struct BurnDriverD BurnDrvFuncube4 = {
	"funcube4", NULL, NULL, NULL, "2001",
	"Funcube 4 (v1.0)\0", "Unemulated Sub CPU", "Namco", "Newer Seta",
	NULL, NULL, NULL, NULL,
	0 | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA2, GBF_PUZZLE, 0,
	NULL, funcube4RomInfo, funcube4RomName, NULL, NULL, NULL, NULL, grdiansInputInfo, grdiansDIPInfo,
	funcubeInit, grdiansExit, grdiansFrame, DrvDraw, grdiansScan, &bRecalcPalette, 0x8000,
	320, 240, 4, 3
};


// -----------------------------------------------------------------------------
// Guardians / Denjin Makai II Series
// -----------------------------------------------------------------------------

#define GRDIANS_COMPONENT										\
	{ "u4.bin",		0x080000, 0xbb52447b, BRF_ESS | BRF_PRG },	\
	{ "u5.bin",		0x080000, 0x9c164a3b, BRF_ESS | BRF_PRG },	\
	{ "u16.bin",	0x400000, 0x6a65f265, BRF_GRA },			\
	{ "u20.bin",	0x400000, 0xa7226ab7, BRF_GRA },			\
	{ "u15.bin",	0x400000, 0x01672dcd, BRF_GRA },			\
	{ "u19.bin",	0x400000, 0xc0c998a0, BRF_GRA },			\
	{ "u18.bin",	0x400000, 0x967babf4, BRF_GRA },			\
	{ "u22.bin",	0x400000, 0x6239997a, BRF_GRA },			\
	{ "u17.bin",	0x400000, 0x0fad0629, BRF_GRA },			\
	{ "u21.bin",	0x400000, 0x6f95e466, BRF_GRA },			\
	{ "u32.bin",	0x100000, 0xcf0f3017, BRF_SND },

// Guardians / Denjin Makai II (Pro, Hack)
// GOTVG 20240204

static struct BurnRomInfo grdiandsRomDesc[] = {
	{ "u2_ds.bin",	0x080000, 0x3e62b0f2, BRF_ESS | BRF_PRG },
	{ "u3_ds.bin",	0x080000, 0x46111562, BRF_ESS | BRF_PRG },
	GRDIANS_COMPONENT
};

STD_ROM_PICK(grdiands)
STD_ROM_FN(grdiands)

struct BurnDriver BurnDrvGrdiands = {
	"grdiands", "grdians", NULL, NULL, "2024",
	"Guardians / Denjin Makai II (Pro, Hack)\0", NULL, "hack", "Newer Seta",
	L"Guardians\0\u96fb\u795e\u9b54\u5080\u2161 (Pro, Hack)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA2, GBF_SCRFIGHT, 0,
	NULL, grdiandsRomInfo, grdiandsRomName, NULL, NULL, NULL, NULL, grdiansInputInfo, grdiansDIPInfo,
	grdiansInit, grdiansExit, grdiansFrame, DrvDraw, grdiansScan, &bRecalcPalette, 0x8000,
	304, 232, 4, 3
};


// Guardians / Denjin Makai II (Evil, Hack)
// GOTVG 20240204

static struct BurnRomInfo grdiansyRomDesc[] = {
	{ "u2_ds.bin",	0x080000, 0x3e62b0f2, BRF_ESS | BRF_PRG },
	{ "u3_sy.bin",	0x080000, 0x28ae495d, BRF_ESS | BRF_PRG },
	GRDIANS_COMPONENT
};

STD_ROM_PICK(grdiansy)
STD_ROM_FN(grdiansy)

struct BurnDriver BurnDrvGrdiansy = {
	"grdiansy", "grdians", NULL, NULL, "2024",
	"Guardians / Denjin Makai II (Evil, Hack)\0", NULL, "hack", "Newer Seta",
	L"Guardians\0\u96fb\u795e\u9b54\u5080\u2161 (Evil, Hack)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA2, GBF_SCRFIGHT, 0,
	NULL, grdiansyRomInfo, grdiansyRomName, NULL, NULL, NULL, NULL, grdiansInputInfo, grdiansDIPInfo,
	grdiansInit, grdiansExit, grdiansFrame, DrvDraw, grdiansScan, &bRecalcPalette, 0x8000,
	304, 232, 4, 3
};


// Guardians / Denjin Makai II Plus (Hack)
// GOTVG 20231008

static struct BurnRomInfo grdianspRomDesc[] = {
	{ "u2_pls.bin",	0x080000, 0xa920eea9, BRF_ESS | BRF_PRG },
	{ "u3_pls.bin",	0x080000, 0x4b98155f, BRF_ESS | BRF_PRG },
	GRDIANS_COMPONENT
};

STD_ROM_PICK(grdiansp)
STD_ROM_FN(grdiansp)

struct BurnDriver BurnDrvGrdiansp = {
	"grdiansp", "grdians", NULL, NULL, "2023",
	"Guardians / Denjin Makai II (Plus, Hack)\0", NULL, "hack", "Newer Seta",
	L"Guardians\0\u96fb\u795e\u9b54\u5080\u2161 (Plus, Hack)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA2, GBF_SCRFIGHT, 0,
	NULL, grdianspRomInfo, grdianspRomName, NULL, NULL, NULL, NULL, grdiansInputInfo, grdiansDIPInfo,
	grdiansInit, grdiansExit, grdiansFrame, DrvDraw, grdiansScan, &bRecalcPalette, 0x8000,
	304, 232, 4, 3
};


// Guardians / Denjin Makai II (LBS, Hack)
// GOTVG 20231030

static struct BurnRomInfo grdianslRomDesc[] = {
	{ "u2_lbs.bin",	0x080000, 0xaf77f6dd, BRF_ESS | BRF_PRG },
	{ "u3_lbs.bin",	0x080000, 0x9f6433df, BRF_ESS | BRF_PRG },
	GRDIANS_COMPONENT
};

STD_ROM_PICK(grdiansl)
STD_ROM_FN(grdiansl)

struct BurnDriver BurnDrvGrdiansl = {
	"grdiansl", "grdians", NULL, NULL, "2023",
	"Guardians / Denjin Makai II (LBS, Hack)\0", NULL, "hack", "Newer Seta",
	L"Guardians\0\u96fb\u795e\u9b54\u5080\u2161 (LBS, Hack)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA2, GBF_SCRFIGHT, 0,
	NULL, grdianslRomInfo, grdianslRomName, NULL, NULL, NULL, NULL, grdiansInputInfo, grdiansDIPInfo,
	grdiansInit, grdiansExit, grdiansFrame, DrvDraw, grdiansScan, &bRecalcPalette, 0x8000,
	304, 232, 4, 3
};


// Guardians / Denjin Makai II (LBS Super, Hack)
// GOTVG 20210419

static struct BurnRomInfo grdianlsRomDesc[] = {
	{ "u2_lbss.bin",	0x080000, 0xe8c9fb0f, BRF_ESS | BRF_PRG },
	{ "u3_lbss.bin",	0x080000, 0xd4dac047, BRF_ESS | BRF_PRG },
	GRDIANS_COMPONENT
};

STD_ROM_PICK(grdianls)
STD_ROM_FN(grdianls)

struct BurnDriver BurnDrvGrdianls = {
	"grdianls", "grdians", NULL, NULL, "2021",
	"Guardians / Denjin Makai II (LBS Super, Hack)\0", NULL, "hack", "Newer Seta",
	L"Guardians\0\u96fb\u795e\u9b54\u5080\u2161 (LBS Super, Hack)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA2, GBF_SCRFIGHT, 0,
	NULL, grdianlsRomInfo, grdianlsRomName, NULL, NULL, NULL, NULL, grdiansInputInfo, grdiansDIPInfo,
	grdiansInit, grdiansExit, grdiansFrame, DrvDraw, grdiansScan, &bRecalcPalette, 0x8000,
	304, 232, 4, 3
};


// Guardians / Denjin Makai II (Reverie, Hack)
// GOTVG 20220606

static struct BurnRomInfo grdiankeRomDesc[] = {
	{ "u2_ke.bin",	0x080000, 0xba4a9452, BRF_ESS | BRF_PRG },
	{ "u3_ke.bin",	0x080000, 0xd1713d73, BRF_ESS | BRF_PRG },
	GRDIANS_COMPONENT
};

STD_ROM_PICK(grdianke)
STD_ROM_FN(grdianke)

struct BurnDriver BurnDrvGrdianke = {
	"grdianke", "grdians", NULL, NULL, "2022",
	"Guardians / Denjin Makai II (Reverie, Hack)\0", NULL, "hack", "Newer Seta",
	L"Guardians\0\u96fb\u795e\u9b54\u5080\u2161 (Reverie, Hack)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA2, GBF_SCRFIGHT, 0,
	NULL, grdiankeRomInfo, grdiankeRomName, NULL, NULL, NULL, NULL, grdiansInputInfo, grdiansDIPInfo,
	grdiansInit, grdiansExit, grdiansFrame, DrvDraw, grdiansScan, &bRecalcPalette, 0x8000,
	304, 232, 4, 3
};

#undef GRDIANS_COMPONENT
