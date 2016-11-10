// FB Alpha MSX arcade driver module, by dink. memory mapping code (megarom) from fMSX
//
// TODO:
// 1: Clean-up!
//
// Oddities:
//  VoidRunner and Milk Race freeze when selecting between kbd/joy. (VoidRunner has a kludge, but it doesn't work for Milk Race)
//  Krakout any key starts, can't get into settings

#include "tiles_generic.h"
#include "z80_intf.h"
#include "driver.h"
#include "tms9928a.h"
#include "8255ppi.h"
#include "bitswap.h"
#include "k051649.h"
#include "dac.h"

#ifdef BUILD_WIN32
extern void (*cBurnerKeyCallback)(UINT8 code, UINT8 KeyType, UINT8 down);
extern INT32 nReplayExternalDataCount;
extern UINT8 *ReplayExternalData;
#endif

#ifndef BUILD_WIN32
INT32 nReplayExternalDataCount = 0;
UINT8 *ReplayExternalData = NULL;
#endif

extern "C" {
	#include "ay8910.h"
}

static INT16 *pAY8910Buffer[6];

static UINT8 *AllMem	= NULL;
static UINT8 *MemEnd	= NULL;
static UINT8 *AllRam	= NULL;
static UINT8 *RamEnd	= NULL;
static UINT8 *maincpu	= NULL; // msx bios rom
static UINT8 *game      = NULL; // game cart rom, tape side A
static UINT8 *game2     = NULL; // tape side B
static UINT8 *main_mem	= NULL;
static UINT8 *kanji_rom = NULL;
static UINT8 *game_sram = NULL;

static UINT8 *curtape   = NULL; // pointer(only) to currently inserted tape.
static INT32 curtapelen = 0;

static UINT8 use_kanji     = 0;
static UINT8 msx_basicmode = 0;

static UINT8 DrvInputs[2];

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy4[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static UINT8 DrvDips[1];
static UINT8 DrvReset;
static UINT8 DrvNMI = 0;

static struct BurnRomInfo emptyRomDesc[] = {
	{ "",                    0,          0, 0 },
};

static struct BurnInputInfo MSXInputList[] = {
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},
	{"P1 Button 2",	BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"},

	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"},
	{"P2 Right",	BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"},
	{"P2 Button 1",	BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"},
	{"P2 Button 2",	BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"},

	{"Key F1",	    BIT_DIGITAL,	DrvJoy4 + 3,	"p1 F1" },
	{"Key F2",	    BIT_DIGITAL,	DrvJoy4 + 4,	"p1 F2" },
	{"Key F3",	    BIT_DIGITAL,	DrvJoy4 + 5,	"p1 F3" },
	{"Key F4",	    BIT_DIGITAL,	DrvJoy4 + 6,	"p1 F4" },
	{"Key F5",	    BIT_DIGITAL,	DrvJoy4 + 7,	"p1 F5" },
	{"Key F6",	    BIT_DIGITAL,	DrvJoy4 + 8,	"p1 F6" },

	{"Key UP",	    BIT_DIGITAL,	DrvJoy4 + 9,	"p1 KEYUP" },
	{"Key DOWN",	BIT_DIGITAL,	DrvJoy4 + 10,	"p1 KEYDOWN" },
	{"Key LEFT",	BIT_DIGITAL,	DrvJoy4 + 11,	"p1 KEYLEFT" },
	{"Key RIGHT",	BIT_DIGITAL,	DrvJoy4 + 12,	"p1 KEYRIGHT" },

	{"Reset",		BIT_DIGITAL,	&DrvReset,	    "reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
};

STDINPUTINFO(MSX)

static struct BurnDIPInfo MSXDIPList[]=
{
	//{0x17, 0xff, 0xff, 0x10, NULL		},

	{0   , 0xfe, 0   ,    2, "BIOS - NOTE: Changes require re-start!"	},
	{0x17, 0x01, 0x01, 0x00, "Normal (REQ: msx.rom)"	},
	{0x17, 0x01, 0x01, 0x01, "Japanese (REQ: msxj.rom, kanji.rom" },

	{0   , 0xfe, 0   ,    2, "Hertz"	},
	{0x17, 0x01, 0x10, 0x00, "50hz (Europe)"	},
	{0x17, 0x01, 0x10, 0x10, "60hz (Japan, US)"	},

	{0   , 0xfe, 0   ,    2, "Key-Clicker / 1-Bit DAC"	},
	{0x17, 0x01, 0x02, 0x00, "Off"	},
	{0x17, 0x01, 0x02, 0x02, "On"	},

	{0   , 0xfe, 0   ,    2, "Swap Joyports"	},
	{0x17, 0x01, 0x20, 0x00, "Normal"	},
	{0x17, 0x01, 0x20, 0x20, "Swapped"	},

	{0   , 0xfe, 0   ,    2, "Map Cursor Keys to Joy #1"	},
	{0x17, 0x01, 0x80, 0x00, "Off"	},
	{0x17, 0x01, 0x80, 0x80, "On"	},

	{0   , 0xfe, 0   ,    2, "Tape Side"	},
	{0x17, 0x01, 0x40, 0x00, "Side A"	},
	{0x17, 0x01, 0x40, 0x40, "Side B"	},
};

static struct BurnDIPInfo MSXDefaultDIPList[]=
{
	{0x17, 0xff, 0xff, 0x10, NULL		},
};

static struct BurnDIPInfo MSXJBIOSDIPList[]=
{
	{0x17, 0xff, 0xff, 0x11, NULL		},
};

static struct BurnDIPInfo MSX50hzJoySwapDIPList[]=
{
	{0x17, 0xff, 0xff, 0x20, NULL		},
};

static struct BurnDIPInfo MSX50hzDIPList[]=
{
	{0x17, 0xff, 0xff, 0x20, NULL		},
};

static struct BurnDIPInfo MSXJoySwapDIPList[]=
{
	{0x17, 0xff, 0xff, 0x30, NULL		},
};

static struct BurnDIPInfo MSXMapCursorToJoy1DIPList[]=
{
	{0x17, 0xff, 0xff, 0x80, NULL		},
};

static struct BurnDIPInfo MSXMapCursorToJoy1_60hzDIPList[]=
{
	{0x17, 0xff, 0xff, 0x80+0x10, NULL		},
};

static struct BurnDIPInfo MSXKeyClickerDACDIPList[]=
{
	{0x17, 0xff, 0xff, 0x02, NULL		},
};

STDDIPINFOEXT(MSX, MSXDefault, MSX)
STDDIPINFOEXT(MSXJapan, MSXJBIOS, MSX)
STDDIPINFOEXT(MSXJoyport2, MSXJoySwap, MSX)
STDDIPINFOEXT(MSXEuropeJoyport2, MSX50hzJoySwap, MSX)
STDDIPINFOEXT(MSXEurope, MSX50hz, MSX)
STDDIPINFOEXT(MSXJoyCursor, MSXMapCursorToJoy1, MSX)
STDDIPINFOEXT(MSXJoyCursor60hz, MSXMapCursorToJoy1_60hz, MSX)
STDDIPINFOEXT(MSXKeyClick, MSXKeyClickerDAC, MSX)

// ROM mapper types:
#define MAP_KONGEN8  0      // KonamiGeneric 8k
#define MAP_KONGEN16 1      // KonamiGeneric 16k
#define MAP_KONAMI5  2      // Konami SCC
#define MAP_KONAMI4  3      // Konami
#define MAP_ASCII8   4      // ASCII 8
#define MAP_ASCII16  5      // ASCII 16
#define MAP_DOOLY    6      // Dooly
#define MAP_CROSSBL  7      // Cross Blaim
#define MAP_RTYPE    8      // R-Type

#define MAXSLOTS    4
#define MAXMAPPERS  9

#define MAX_MSX_CARTSIZE 0x200000

// Machine Config
#define DEFAULT_BIOSSLOT 0
#define DEFAULT_CARTSLOTA 1
#define DEFAULT_CARTSLOTB 2
#define DEFAULT_RAMSLOT 3

static UINT8 SwapJoyports = 0; // Swap Joyport DIP
static UINT8 Joyselect = 0;    // read from Joystick 0 or 1? (internal)
static UINT8 Hertz60 = 0;      // DIP setting.
static UINT8 BiosmodeJapan = 0;// DIP setting.

// Game-based kludges
static INT32 MapCursorToJoy1 = 0; // Map Cursor Keys & space to Joy1

static INT32 VBlankKludge = 0; // For VoidRunner (joystick selection hangs)
static INT32 SwapRamslot = 0; // For Toshiba-EMI's Break Out!
static INT32 SwapButton2 = 0; // Swaps Joy button#2 with 'm', for Xenon and Astro Marine Corps
static INT32 SwapSlash = 0; // For Square Dancer, swaps the / key with the Japanese Underscore key

static INT32 CASMode = 0;      // Using .cas file?
static INT32 CASPos = 0;       // Internal tape position counter
static INT32 CASFrameCounter = 0; // for autoloading
static INT32 CASSide = 0; // Tape Side, 0 = A, 1 = B
static INT32 CASSideLast = 0; // used for detecting side changes

static INT32 BIOSSLOT = 0;      // Machine slot configuration
static INT32 CARTSLOTA = 0;
static INT32 CARTSLOTB = 0;
static INT32 RAMSLOT = 0;

static UINT8 *RAM[8];          // Mapped address space
static UINT8 *EmptyRAM = NULL; // Unmapped stuff points here
static UINT8 *MemMap[4][8];    // [prislot] [page]

static UINT8 *RAMData;         // main flat-chunk of ram
static UINT8 RAMMapper[4];
static UINT8 RAMMask;
static INT32 RAMPages = 4;

static UINT8 *SRAMData[MAXSLOTS]; // ascii8/16 sram

static UINT8 *ROMData[MAXSLOTS];  // flat chunk of cart-rom
static UINT8 ROMMapper[MAXSLOTS][4];
static UINT8 ROMMask[MAXSLOTS];
static UINT8 ROMType[MAXSLOTS];

// Game-specific mappers
static UINT8 dooly_prot;
static UINT8 crossblaim_selected_bank;
static UINT8 *crossblaim_bank_base[4];
static UINT8 rtype_selected_bank;
static UINT8 *rtype_bank_base[2];

static INT32 CurRomSizeA = 0; // cart A
static INT32 CurRomSizeB = 0; // cart B

static UINT8 WriteMode[MAXSLOTS];
static UINT8 PSL[MAXSLOTS]; // primary slot list
static UINT8 PSLReg; // primary slot register
static UINT8 SCCReg[MAXSLOTS]; // Konami-scc enable register

static UINT8 Kana, KanaByte; // Kanji-rom stuff

static UINT8 ppiC_row;
static UINT8 keyRows[12];
static INT32 charMatrix[][3] = {
	{'0', 0, 0}, {')', 0, 0}, {'1', 0, 1}, {'!', 0, 1}, {'2', 0, 2}, {'@', 0, 2},
	{'3', 0, 3}, {'#', 0, 3}, {'4', 0, 4}, {'$', 0, 4}, {'5', 0, 5}, {'%', 0, 5},
	{'6', 0, 6}, {'^', 0, 6}, {'7', 0, 7}, {'&', 0, 7},

	{'8', 1, 0}, {'*', 1, 0}, {'9', 1, 1}, {'(', 1, 1}, {'-', 1, 2}, {'_', 1, 2},
	{'=', 1, 3}, {'+', 1, 3}, {'\\', 1, 4}, {'|', 1, 4}, {'[', 1, 5}, {'{', 1, 5},
	{']', 1, 6}, {'}', 1, 6}, {';', 1, 7}, {':', 1, 7},

	{'A', 2, 6}, {'B', 2, 7}, {'a', 2, 6}, {'b', 2, 7},
	{'/', 2, 4}, {'?', 2, 4}, {'.', 2, 3}, {'>', 2, 3},
	{',', 2, 2}, {'<', 2, 2}, {'`', 2, 1}, {'~', 2, 1},
	{'\'', 2, 0}, {'"', 2, 0},

	{'C', 3, 0}, {'D', 3, 1}, {'E', 3, 2}, {'F', 3, 3}, {'G', 3, 4}, {'H', 3, 5},
	{'I', 3, 6}, {'J', 3, 7},
	{'c', 3, 0}, {'d', 3, 1}, {'e', 3, 2}, {'f', 3, 3}, {'g', 3, 4}, {'h', 3, 5},
	{'i', 3, 6}, {'j', 3, 7},

	{'K', 4, 0}, {'L', 4, 1}, {'M', 4, 2}, {'N', 4, 3}, {'O', 4, 4}, {'P', 4, 5},
	{'Q', 4, 6}, {'R', 4, 7},
	{'k', 4, 0}, {'l', 4, 1}, {'m', 4, 2}, {'n', 4, 3}, {'o', 4, 4}, {'p', 4, 5},
	{'q', 4, 6}, {'r', 4, 7},

	{'S', 5, 0}, {'T', 5, 1}, {'U', 5, 2}, {'V', 5, 3}, {'W', 5, 4}, {'X', 5, 5},
	{'Y', 5, 6}, {'Z', 5, 7},
	{'s', 5, 0}, {'t', 5, 1}, {'u', 5, 2}, {'v', 5, 3}, {'w', 5, 4}, {'x', 5, 5},
	{'y', 5, 6}, {'z', 5, 7}, 
	{/*VK_RETURN, 	  */0x0d,  7, 7},
	{/*VK_ESCAPE, 	  */0x1b,  7, 2},
	{/*VK_SPACE,	  */' ',   8, 0},
	{/*VK_F1, 		  */0xf1,  6, 5},
	{/*VK_F2, 		  */0xf2,  6, 6},
	{/*VK_F3, 		  */0xf3,  6, 7},
	{/*VK_F4, 		  */0xf4,  7, 0},
	{/*VK_F5,		  */0xf5,  7, 1},
	{/*VK_F6, 		  */0xf6,  7, 6},

	{/*J-SPADE        */0xe0,  2, 5},

	{/*VK_SHIFT       */0x10,  6, 0},
	{/*VK_CONTROL     */0x11,  6, 1},
	{/*VK_TAB         */0x09,  7, 3},
	//{VK_STOP, 	  7, 4},
	{/*VK_BACK        */0x08,  7, 5},
	//{VK_HOME,	  8, 1},
	//{VK_INSERT,   8, 2},
	{/*VK_DELETE      */0x2e,  8, 3},
	{/*VK_UP,		  */0xf8,  8, 5},
	{/*VK_DOWN, 	  */0xf9,  8, 6},
	{/*VK_LEFT, 	  */0xfa,  8, 4},
	{/*VK_RIGHT, 	  */0xfb,  8, 7},
	{'\0', 0, 0} // NULL/END.
};

static inline void intkeyOn(INT32 row, INT32 bit) {
	keyRows[row] = ((keyRows[row] & 0xff) | (1 << bit));
}

static inline void intkeyOff(INT32 row, INT32 bit) {
	keyRows[row] = ((keyRows[row] & 0xff) & ~(1 << bit));
}

static UINT8 keyRowGet(INT32 row) { // for ppi to read
	if (row > 11) return 0xff;
	return ~keyRows[row];
}

static void keyInput(UINT8 kchar, UINT8 onoff) { // input from emulator
	INT32 i = 0;
	INT32 gotkey = 0;

	while (charMatrix[i][0] != '\0') {
		if (kchar == charMatrix[i][0]) {
			if (onoff) {
				intkeyOn(charMatrix[i][1], charMatrix[i][2]);
			} else {
				intkeyOff(charMatrix[i][1], charMatrix[i][2]);
			}
			gotkey = 1;
			break;
		}
		i++;
	}
}

void msxKeyCallback(UINT8 code, UINT8 KeyType, UINT8 down)
{
	static INT32 lastshifted = 0;

	//bprintf(0, _T(" %c:%S,"), code, (down==1)?"DOWN":"UP");
	if (SwapSlash && code == '/') code = 0xe0;

	if (lastshifted) memset(&keyRows, 0, sizeof(keyRows));
	keyInput(/*VK_SHIFT*/'\x10', (KeyType & 0xf0));
	keyInput(code, down);
	lastshifted = (KeyType & 0xf0);
	// Note regarding 'lastshifted'.  If shift+key is pressed (f.ex. ") and shift
	// is let up before the key is let up, windows won't send the right keyup message.
	// this causes keys to get stuck.  To kludge around this, we clear the keyboard
	// matrix-buffer when shift is let up. -dink
}

static const char *ROMNames[MAXMAPPERS + 1] =
{ 
  "KonamiGeneric 8k\0", "KonamiGeneric 16k\0", "Konami-SCC\0",
  "Konami\0", "ASCII 8k\0", "ASCII 16k\0",
  "Dooly\0", "Cross Blaim\0", "R-Type\0", "???\0"
};

#define CAS_BLOAD 1
#define CAS_RUN 2
#define CAS_CLOAD 3
#define CAS_CLOADRR 4
#define CAS_WRONGSIDE 5

static const char *CASAutoLoadTypes[] =
{
	"bload \"cas:\", r\x0d", "run \"cas:\"\x0d", "cload\x0drun\x0d",
	"cload\x0drun\x0drun\x0d", "rem Set Tape Side-A in DIPs & reboot!\x0d"
};

static INT32 InsertCart(UINT8 *cartbuf, INT32 cartsize, INT32 nSlot);
static void PageMap(INT32 CartSlot, const char *cMap); //("0:0:0:0:0:0:0:0")
static void MapMegaROM(UINT8 nSlot, UINT8 nPg0, UINT8 nPg1, UINT8 nPg2, UINT8 nPg3);

static INT32 CASAutoLoadPos = 0;
static INT32 CASAutoLoadTicker = 0;

static void CASSideChange()
{
	curtape = (CASSide) ? game2 : game;
	curtapelen = (CASSide) ? CurRomSizeB : CurRomSizeA;
	CASPos = 0;
	CASSideLast = CASSide;
}

static void CASAutoLoad()
{
	CASAutoLoadPos = 0;
	CASAutoLoadTicker = 0;
	CASFrameCounter = 0;
	CASPos = 0;
}

static void CASAutoLoadTick()
{
	if (CASAutoLoadPos == 0xff) return;

	UINT8 CASModeTmp = (CASSide) ? CAS_WRONGSIDE : CASMode;
	UINT8 c = CASAutoLoadTypes[CASModeTmp - 1][CASAutoLoadPos];
	if (!c) {
		CASAutoLoadPos = 0xff;
		return;
	}

	keyInput(c, !(CASAutoLoadTicker & 1));
	if (c == '\"' || c == ':' || c == '&' || c == '!')
		keyInput(0x10/*SHIFT*/, !(CASAutoLoadTicker & 1));
	if (CASAutoLoadTicker & 1) CASAutoLoadPos++;
	CASAutoLoadTicker++;
}

static void CASPatchBIOS(UINT8 *bios)
{
	UINT8 PatchBytes[] = { 0xe1, 0xe4, 0xe7, 0xea, 0xed, 0xf0, 0xf3, 0x00 };
	UINT8 i = 0;

	while (PatchBytes[i] != 0x00) {
		UINT8 *p = bios + PatchBytes[i];
		p[0] = 0xed;
		p[1] = 0xfe;
		p[2] = 0xc9;
		i++;
	}
}

extern void (*z80edfe_callback)(Z80_Regs *Regs);

static void Z80EDFECallback(Z80_Regs *Regs)
{
	static const UINT8 TapeHeader[8] = { 0x1f, 0xa6, 0xde, 0xba, 0xcc, 0x13, 0x7d, 0x74 };
    #define Z80CF 0x01

	switch (Regs->pc.d - 2)
	{
		case 0x00e1: // TAPION (open & read header)
			{
				bprintf(0, _T("CAS: Searching header: "));

				Regs->af.b.l |= Z80CF;
				if (CASMode) {
					while (CASPos + 8 < curtapelen) {
						if (!memcmp(curtape + CASPos, TapeHeader, 8)) {
							CASPos+=8;
							bprintf(0, _T("Found.\n"));
							Regs->af.b.l &= ~Z80CF;
							return;
						}
						CASPos += 1;
					}
					bprintf(0, _T("Not found.\n"));
					CASPos = 0;
					return;
				}
				bprintf(0, _T("Tape offline.\n"));

				return;
			}

		  case 0x00e4: // TAPIN (read)
		  {
			  Regs->af.b.l |= Z80CF;

			  if (CASMode) {
				  UINT8 c = curtape[CASPos++];

				  if (CASPos > curtapelen) {
					  CASPos = 0;
				  }
				  else
				  {
					  Regs->af.b.h = c;
					  Regs->af.b.l &= ~Z80CF;
				  }
			  }

			  return;
		  }

		  case 0x00e7: // TAPIOF (stop reading from tape)
		  Regs->af.b.l &= ~Z80CF;
		  return;

		  case 0x00ea: // TAPOON (write header)
			  bprintf(0, _T("TAPOON"));
			  return;

		  case 0x00ed: // TAPOUT (write byte)
			  bprintf(0, _T("TAPOUT"));
			  return;

		  case 0x00f0: // TAPOOF (stop writing)
			  Regs->af.b.l &= ~Z80CF;
			  return;

		  case 0x00f3: // STMOTR (motor control)
			  Regs->af.b.l &= ~Z80CF;
			  return;
	}
}


void msxinit(INT32 cart_len)
{
	for(INT32 i = 0; i < MAXSLOTS; i++) {
		ROMMask[i] = 0;
		ROMData[i] = 0;
		ROMType[i] = 0;
		SRAMData[i] = 0;
		SCCReg[i] = 0;
	}

	BIOSSLOT = DEFAULT_BIOSSLOT;
	CARTSLOTA = DEFAULT_CARTSLOTA;
	CARTSLOTB = DEFAULT_CARTSLOTB;
	RAMSLOT = DEFAULT_RAMSLOT;

	if (SwapRamslot) { // Kludge for Toshiba-EMI's Break Out! - needs ram in slot 2
		RAMSLOT = 2;
		CARTSLOTB = 3;
	}

	bprintf(0, _T("Slots: BIOS %d RAM %d CART1 %d CART2 %d\n"), BIOSSLOT, RAMSLOT, CARTSLOTA, CARTSLOTB);

	memset(EmptyRAM, 0xff, 0x4000); // bus is pulled high for unmapped reads

	for(INT32 PSlot = 0; PSlot < 4; PSlot++) // Point all pages there by default
		for(INT32 Page = 0; Page < 8; Page++)
			MemMap[PSlot][Page] = EmptyRAM;

	RAMPages = 4; // 64k
	RAMMask = RAMPages - 1;
	RAMData = main_mem;

	if (CASMode) {
		bprintf(0, _T("Cassette mode.\n"));
		ZetSetEDFECallback(Z80EDFECallback);
		CASPatchBIOS(maincpu);
		CASAutoLoad();
		CASSide = 0; // Always start @ side A
		CASSideChange();
	}

	// "Insert" BIOS ROM
	ROMData[BIOSSLOT] = maincpu;
	PageMap(BIOSSLOT, "0:1:2:3:e:e:e:e");

	if (!msx_basicmode)
		InsertCart(game, cart_len, CARTSLOTA);

	PSLReg = 0;

	for (INT32 i = 0; i < 4; i++) {
		WriteMode[i] = 0;
		PSL[i] = 0;
		MemMap[RAMSLOT][i * 2] = RAMData + (3 - i) * 0x4000;
		MemMap[RAMSLOT][i * 2 + 1] = MemMap[RAMSLOT][i * 2] + 0x2000;
		RAMMapper[i] = 3 - i;
		RAM[i * 2] = MemMap[BIOSSLOT][i * 2];
		RAM[i * 2 + 1] = MemMap[BIOSSLOT][i * 2 + 1];
	}


	for (INT32 J = 0; J < MAXSLOTS; J++)
		if (((ROMMask[J] + 1) > 4) || (ROMType[J] == MAP_DOOLY))
		{
			INT32 I = ROMMask[J] + 1;
			
			if ((ROMData[J][0] == 'A') && (ROMData[J][1] == 'B')) {
				MapMegaROM(J, 0, 1, 2, 3);
			} else {
				if ((ROMData[J][(I - 2) << 13] == 'A') && (ROMData[J][((I - 2) << 13) + 1] == 'B'))
					MapMegaROM(J, I - 2, I - 1, I - 2, I - 1);
			}
		}
}

static void rtype_do_bank(UINT8 *romdata)
{
	rtype_bank_base[0] = romdata + 15 * 0x4000;
	if (rtype_selected_bank & 0x10)
	{
		rtype_selected_bank &= 0x17;
	}
	rtype_bank_base[1] = romdata + rtype_selected_bank * 0x4000;
}

static void crossblaim_do_bank(UINT8 *romdata)
{
	crossblaim_bank_base[0] = ( crossblaim_selected_bank & 2 ) ? NULL : romdata + ( crossblaim_selected_bank & 0x03 ) * 0x4000;
	crossblaim_bank_base[1] = romdata;
	crossblaim_bank_base[2] = romdata + ( crossblaim_selected_bank & 0x03 ) * 0x4000;
	crossblaim_bank_base[3] = ( crossblaim_selected_bank & 2 ) ? NULL : romdata + ( crossblaim_selected_bank & 0x03 ) * 0x4000;
}

static void Mapper_write(UINT16 address, UINT8 data)
{
	UINT8 Page = address >> 14; // pg. num
	UINT8 PSlot = PSL[Page];

	if (PSlot >= MAXSLOTS) return;

	if (!ROMData[PSlot] && (address == 0x9000))
		SCCReg[PSlot] = (data == 0x3f) ? 1 : 0;

	if (((address & 0xdf00) == 0x9800) && SCCReg[PSlot]) { // Handle Konami-SCC (+)
		UINT16 offset = address & 0x00ff;

		if (offset < 0x80) {
			K051649WaveformWrite(offset, data);
		}
		else if (offset < 0xa0)	{
			offset &= 0xf;

			if (offset < 0xa) {
				K051649FrequencyWrite(offset, data);
			}
			else if (offset < 0xf) {
				K051649VolumeWrite(offset - 0xa, data);
			}
			else {
				K051649KeyonoffWrite(data);
			}
		}

		return;
	}

	if (!ROMData[PSlot] || !ROMMask[PSlot]) return;

	switch (ROMType[PSlot])
	{
		case MAP_DOOLY:
			dooly_prot = data & 0x07;
			return;

		case MAP_CROSSBL:
			crossblaim_selected_bank = data & 3;
			if (crossblaim_selected_bank == 0) {
				crossblaim_selected_bank = 1;
			}
			crossblaim_do_bank(ROMData[PSlot]);

			return;

		case MAP_RTYPE:
			if (address >= 0x7000 && address < 0x8000)
			{
				rtype_selected_bank = data & 0x1f;
				if (rtype_selected_bank & 0x10)
				{
					rtype_selected_bank &= 0x17;
				}
				rtype_bank_base[1] = ROMData[PSlot] + rtype_selected_bank * 0x4000;
			}

		case MAP_KONGEN8:
			if ((address < 0x4000) || (address > 0xbfff)) break;
			Page = (address - 0x4000) >> 13;
			if (Page == 2) SCCReg[PSlot] = (data == 0x3f) ? 1 : 0;

			data &= ROMMask[PSlot];
			if (data != ROMMapper[PSlot][Page])
			{
				RAM[Page + 2] = MemMap[PSlot][Page + 2] = ROMData[PSlot] + (data << 13);
				ROMMapper[PSlot][Page] = data;
			}
			return;

		case MAP_KONGEN16:
			if ((address < 0x4000) || (address > 0xbfff)) break;
			Page = (address & 0x8000) >> 14;

			data = (data << 1) & ROMMask[PSlot];
			if (data != ROMMapper[PSlot][Page])
			{
				RAM[Page + 2] = MemMap[PSlot][Page + 2] = ROMData[PSlot] + (data << 13);
				RAM[Page + 3] = MemMap[PSlot][Page + 3] = RAM[Page + 2] + 0x2000;
				ROMMapper[PSlot][Page] = data;
				ROMMapper[PSlot][Page + 1] = data + 1;
			}
			return;

		case MAP_KONAMI5:
			if ((address < 0x5000) || (address > 0xb000) || ((address & 0x1fff) != 0x1000)) break;
			Page = (address - 0x5000) >> 13;

			if (Page == 2) SCCReg[PSlot] = (data == 0x3f) ? 1 : 0;

			data &= ROMMask[PSlot];
			if (data != ROMMapper[PSlot][Page])
			{
				RAM[Page + 2] = MemMap[PSlot][Page + 2] = ROMData[PSlot] + (data << 13);
				ROMMapper[PSlot][Page] = data;
			}
			return;

		case MAP_KONAMI4:
			if ((address < 0x6000) || (address > 0xa000) || (address & 0x1fff)) break;
			Page = (address - 0x4000) >> 13;

			data &= ROMMask[PSlot];
			if (data != ROMMapper[PSlot][Page])
			{
				RAM[Page + 2] = MemMap[PSlot][Page + 2] = ROMData[PSlot] + (data << 13);
				ROMMapper[PSlot][Page] = data;
			}
			return;

		case MAP_ASCII8:
			if ((address >= 0x6000) && (address < 0x8000))
			{
				UINT8 *pgPtr;

				Page = (address & 0x1800) >> 11;

				if (data & (ROMMask[PSlot] + 1)) {
					data = 0xff;
					pgPtr = SRAMData[PSlot];
				}
				else
				{
					data &= ROMMask[PSlot];
					pgPtr = ROMData[PSlot] + (data << 13);
				}
				
				if (data != ROMMapper[PSlot][Page])
				{
					MemMap[PSlot][Page + 2] = pgPtr;
					ROMMapper[PSlot][Page] = data;

					if (PSL[(Page >> 1) + 1] == PSlot)
						RAM[Page + 2] = pgPtr;
				}
				return;
			}
			if ((address >= 0x8000) && (address < 0xc000) && (ROMMapper[PSlot][((address >> 13) & 1) + 2] == 0xff))
			{
				RAM[address >> 13][address & 0x1fff] = data;
				return;
			}
			break;

		case MAP_ASCII16:
			if ((address >= 0x6000) && (address < 0x8000) && ((data <= ROMMask[PSlot] + 1) || !(address & 0x0fff)))
			{
				UINT8 *pgPtr;
				Page = (address & 0x1000) >> 11;

				if (data & (ROMMask[PSlot] + 1))
				{
					data = 0xff;
					pgPtr = SRAMData[PSlot];
				}
				else
				{

					data = (data << 1) & ROMMask[PSlot];
					pgPtr = ROMData[PSlot] + (data << 13);
				}

				if (data != ROMMapper[PSlot][Page])
				{
					MemMap[PSlot][Page + 2] = pgPtr;
					MemMap[PSlot][Page + 3] = pgPtr + 0x2000;
					ROMMapper[PSlot][Page] = data;
					ROMMapper[PSlot][Page + 1] = data + 1;

					if (PSL[(Page >> 1) + 1] == PSlot)
					{
						RAM[Page + 2] = pgPtr;
						RAM[Page + 3] = pgPtr + 0x2000;
					}
				}
				return;
			}

			if ((address >= 0x8000) && (address < 0xc000) && (ROMMapper[PSlot][2] == 0xff))
			{
				UINT8 *pgPtr = RAM[address >> 13];
				address &= 0x07ff;
				pgPtr[address + 0x0800] = pgPtr[address + 0x1000] = pgPtr[address + 0x1800] =
					pgPtr[address + 0x2000] = pgPtr[address + 0x2800] = pgPtr[address + 0x3000] =
					pgPtr[address + 0x3800] = pgPtr[address] = data;
				return;
			}
			break;
	}

	//bprintf(0, _T("Unhandled mapper write. 0x%04X: %02X, slot %d\n"), address, data, PSlot);
}

static INT32 Mapper_read(UINT16 address, UINT8 *data)
{
  UINT8 Page = address >> 14;
  UINT8 PSlot = PSL[Page];

  if (PSlot >= MAXSLOTS) return 0;

  if (!ROMData[PSlot] || !ROMMask[PSlot]) return 0;

  switch (ROMType[PSlot])
  {
	  case MAP_CROSSBL:
		  {
			  UINT8 *bank_base = crossblaim_bank_base[address >> 14];

			  if (bank_base != NULL)	{
				  *data = bank_base[address & 0x3fff];
				  return 1;
			  }
		  }
	  case MAP_DOOLY:
		  {
			  if ((address > 0x3fff) && (address < 0xc000)) {
				  UINT8 rb = ROMData[PSlot][address - 0x4000];

				  if (dooly_prot == 0x04) {
					  rb = BITSWAP08(rb, 7, 6, 5, 4, 3, 1, 0, 2);
				  }

				  *data = rb;
				  return 1;
			  }
		  }
	  case MAP_RTYPE:
		  {
			  if (address > 0x3fff && address < 0xc000)
			  {
				  *data = rtype_bank_base[address >> 15][address & 0x3fff];
				  return 1;
			  }
		  }
  }
  return 0;
}

static void SetSlot(UINT8 nSlot)
{
	UINT8 I, J;

	if (PSLReg != nSlot) {
		PSLReg = nSlot;
		for (J = 0; J < 4; J++)	{
			I = J << 1;
			PSL[J] = nSlot & 3;
			RAM[I] = MemMap[PSL[J]][I];
			RAM[I + 1] = MemMap[PSL[J]][I + 1];
			WriteMode[J] = (PSL[J] == RAMSLOT) && (MemMap[RAMSLOT][I] != EmptyRAM);
			nSlot >>= 2;
		}
	}
}

static void PageMap(INT32 CartSlot, const char *cMap)
{
	for (INT32 i = 0; i < 8; i++) {
		switch (cMap[i << 1]) {
			case 'n': // no change
				break;
			case 'e': // empty page
				{
					MemMap[CartSlot][i] = EmptyRAM;
				}
				break;
		    default: // map page num.
				{
					MemMap[CartSlot][i] = ROMData[CartSlot] + ((cMap[i << 1] - '0') * 0x2000);
					//bprintf(0, _T("pg %X @ %X\n"), i, ((cMap[i << 1] - '0') * 0x2000));
				}
		}
	}
}

static void MapMegaROM(UINT8 nSlot, UINT8 nPg0, UINT8 nPg1, UINT8 nPg2, UINT8 nPg3)
{
  if (nSlot >= MAXSLOTS) return;

  nPg0 &= ROMMask[nSlot];
  nPg1 &= ROMMask[nSlot];
  nPg2 &= ROMMask[nSlot];
  nPg3 &= ROMMask[nSlot];

  MemMap[nSlot][2] = ROMData[nSlot] + nPg0 * 0x2000;
  MemMap[nSlot][3] = ROMData[nSlot] + nPg1 * 0x2000;
  MemMap[nSlot][4] = ROMData[nSlot] + nPg2 * 0x2000;
  MemMap[nSlot][5] = ROMData[nSlot] + nPg3 * 0x2000;

  ROMMapper[nSlot][0] = nPg0;
  ROMMapper[nSlot][1] = nPg1;
  ROMMapper[nSlot][2] = nPg2;
  ROMMapper[nSlot][3] = nPg3;
}

static INT32 GuessROM(UINT8 *buf, INT32 Size)
{
	INT32 i, j;
	INT32 ROMCount[MAXMAPPERS];

	for (i = 0; i < MAXMAPPERS; i++)
		ROMCount[i] = 1;

	ROMCount[MAP_KONGEN8] += 1;
	ROMCount[MAP_ASCII16] -= 1;

	for (i = 0; i < Size-2; i++) {
		switch (buf[i] + (buf[i + 1] << 8) + (buf[i + 2] << 16))
		{
			case 0x500032:
			case 0x900032:
			case 0xB00032:
				{
					ROMCount[MAP_KONAMI5]++;
					break;
				}
			case 0x400032:
			case 0x800032:
			case 0xA00032:
				{
					ROMCount[MAP_KONAMI4]++;
					break;
				}
			case 0x680032:
			case 0x780032:
				{
					ROMCount[MAP_ASCII8]++;
					break;
				}
			case 0x600032:
				{
					ROMCount[MAP_KONAMI4]++;
					ROMCount[MAP_ASCII8]++;
					ROMCount[MAP_ASCII16]++;
					break;
				}
			case 0x700032:
				{
					ROMCount[MAP_KONAMI5]++;
					ROMCount[MAP_ASCII8]++;
					ROMCount[MAP_ASCII16]++;
					break;
				}
			case 0x77FF32:
				{
					ROMCount[MAP_ASCII16]++;
					break;
				}
		}
	}

	for (i = 0, j = 0; j < MAXMAPPERS; j++)
		if (ROMCount[j] > ROMCount[i]) i = j;

	return i;
}

static INT32 IsBasicROM(UINT8 *rom)
{
	return (rom[2] == 0 && rom[3] == 0 && rom[8] && rom[9]);
}

static UINT16 GetRomStart(UINT8* romData, INT32 size)
{
    INT32 pages[3] = { 0, 0, 0 };

    for (INT32 startPage = 0; startPage < 2; startPage++) {
        UINT8* romPtr = romData + 0x4000 * startPage;

        if (size < 0x4000 * startPage + 0x10) {
            continue;
        }
	    if (romPtr[0] == 'A' && romPtr[1] == 'B') {
		    for (INT32 i = 0; i < 4; i++) {
                UINT16 address = romPtr[2 * i + 2] + 256 * (UINT16)romPtr[2 * i + 3];

			    if (address > 0) {
				    UINT16 page = address / 0x4000 - startPage;

                    if (page < 3) {
                        pages[page]++;
				    }
			    }
		    }
        }
	}

    if (pages[1] && (pages[1] >= pages[0]) && (pages[1] >= pages[2])) {
		return 0x4000;
	}

	if (pages[0] && pages[0] >= pages[2]) {
		return 0x0000;
	}

	if (pages[2]) {
		return 0x8000;
	}

	return 0x0000;
}

static INT32 InsertCart(UINT8 *cartbuf, INT32 cartsize, INT32 nSlot)
{
	INT32 Len, Pages, Flat64, BasicROM;
	UINT8 ca, cb;

	if (nSlot >= MAXSLOTS) return 0;

	Len = cartsize >> 13; // Len, in 8k pages

	for (Pages = 1; Pages < Len; Pages <<= 1); // Calculate nearest power of 2 of len

	ROMData[nSlot] = cartbuf;
	SRAMData[nSlot] = game_sram;

	Flat64 = 0;
	BasicROM = 0;

	ca = cartbuf[0];
	cb = cartbuf[1];

	if ((ca == 'A') || (cb == 'B')) {
		BasicROM = IsBasicROM(cartbuf);
	} else {
		ca = cartbuf[0 + 0x4000];
		cb = cartbuf[1 + 0x4000];
		Flat64 = (ca == 'A') && (cb == 'B');
	}

	if ((Len >= 2) && ((ca != 'A') || (cb != 'B'))) { // check last page
		ca = cartbuf[0 + 0x2000 * (Len - 2)];
		cb = cartbuf[1 + 0x2000 * (Len - 2)];
	}

	if ((ca != 'A') || (cb != 'B')) {
		bprintf(0, _T("MSX Cartridge signature not found!\n"));
		return 0;
	}

	if (Len < Pages) { // rom isn't a valid page-length, so mirror
		memcpy(ROMData[nSlot] + Len * 0x2000,
			   ROMData[nSlot] + (Len - Pages / 2) * 0x2000,
			   (Pages - Len) * 0x2000);
	}

	bprintf(0, _T("Cartridge %c: %dk "), 'A' + nSlot - CARTSLOTA, Len * 8);


	ROMMask[nSlot]= !Flat64 && (Len > 4) ? (Pages - 1) : 0x00;

	bprintf(0, _T("%S\n"), (BasicROM) ? "Basic ROM Detected." : "");

	// Override mapper from hardware code
	switch (BurnDrvGetHardwareCode() & 0xff) {
		case HARDWARE_MSX_MAPPER_BASIC:
			BasicROM = 1;
			break;
		case HARDWARE_MSX_MAPPER_ASCII8:
			ROMType[nSlot] = MAP_ASCII8;
			break;
		case HARDWARE_MSX_MAPPER_ASCII16:
			ROMType[nSlot] = MAP_ASCII16;
			break;
		case HARDWARE_MSX_MAPPER_KONAMI:
			ROMType[nSlot] = MAP_KONAMI4;
			break;
		case HARDWARE_MSX_MAPPER_KONAMI_SCC:
			ROMType[nSlot] = MAP_KONAMI5;
			break;
		case HARDWARE_MSX_MAPPER_DOOLY:
			ROMType[nSlot] = MAP_DOOLY;
			ROMMask[nSlot]=3;
			break;
		case HARDWARE_MSX_MAPPER_CROSS_BLAIM:
			ROMType[nSlot] = MAP_CROSSBL;
			crossblaim_selected_bank = 1;
			crossblaim_do_bank(ROMData[nSlot]);
			break;
		case HARDWARE_MSX_MAPPER_RTYPE:
			ROMType[nSlot] = MAP_RTYPE;
			rtype_selected_bank = 15;
			rtype_do_bank(ROMData[nSlot]);
			break;
	default:
		if (ROMMask[nSlot] + 1 > 4) {
			ROMType[nSlot] = GuessROM(ROMData[nSlot], 0x2000 * (ROMMask[nSlot] + 1));
			bprintf(0, _T("Mapper heusitics detected: %S..\n"), ROMNames[ROMType[nSlot]]);
		}
	}

	if (ROMType[nSlot] != MAP_DOOLY) { // set-up non-megarom mirroring & mapping
		switch (Len)
		{
			case 1: // 8k rom-mirroring
				if (BasicROM)
				{ // BasicROM only on page 2
					PageMap(nSlot, "e:e:e:e:0:0:e:e");
				} else
				{ // normal 8k ROM
					PageMap(nSlot, "0:0:0:0:0:0:0:0");
				}
				break;

			case 2: // 16k rom-mirroring
				if (BasicROM)
				{ // BasicROM only on page 2
					PageMap(nSlot, "e:e:e:e:0:1:e:e");
				} else
				{ // normal 16k ROM
					PageMap(nSlot, "0:1:0:1:0:1:0:1");
				}
				break;

			case 3:
			case 4: // 24k & 32k rom-mirroring
				if (GetRomStart(cartbuf, cartsize) != 0x4000) {
					PageMap(nSlot, "0:1:0:1:2:3:2:3"); // normal
				} else {
					PageMap(nSlot, "2:3:0:1:2:3:0:1"); // swapped
					bprintf(0, _T("Swapped mirroring.\n"));
				}
				break;

		default:
			if (Flat64)
			{ // Flat-64k ROM
				PageMap(nSlot, "0:1:2:3:4:5:6:7");
			}
			break;
		}
		if (Flat64 || Len < 5)
			bprintf(0, _T("starting address 0x%04X.\n"),
					MemMap[nSlot][2][2] + 256 * MemMap[nSlot][2][3]);
	}

	// map gen/16k megaROM pages 0:1:last-1:last
	if ((ROMType[nSlot] == MAP_KONGEN16) && (ROMMask[nSlot] + 1 > 4))
		MapMegaROM(nSlot, 0, 1, ROMMask[nSlot] - 1, ROMMask[nSlot]);

	return 1;
}


static void __fastcall msx_write_port(UINT16 port, UINT8 data)
{
	port &= 0xff;
	switch (port)
	{
		case 0x98:
			TMS9928AWriteVRAM(data);
		return;

		case 0x99:
			TMS9928AWriteRegs(data);
		return;

		case 0xa0:
			AY8910Write(0, 0, data);
		break;

		case 0xa1:
			AY8910Write(0, 1, data);
		break;

		case 0xa8:
		case 0xa9:
		case 0xaa:
		case 0xab:
			ppi8255_w(0, port & 3, data);
			return;

		case 0xd8: // Kanji high bit address selector
			Kana = (Kana & 0x1f800) | (data & 0x3f) << 5;
			KanaByte = 0;
			return;

		case 0xd9: // Kanji low bit address selector
			Kana = (Kana & 0x007e0) | (data & 0x3f) << 11;
			KanaByte = 0;
			return;

#if 0
		// disable the ram-mapper for now (only really used in msx2)
		// causes some issues with games that erraneously write to 0xfc (stardust and utopia demo?)

		case 0xfc: // map ram-page 0x0000, 0x4000, 0x8000, 0xc000
		case 0xfd:
		case 0xfe:
		case 0xff: bprintf(0, _T("Port %X Data %X.\n"), port, data);
			INT32 PSlot = port - 0xfc;
			data &= RAMMask;
			if (RAMMapper[PSlot] != data) {
				bprintf(0, _T("Mapped RAM chunk %d @ 0x%X\n"), data, PSlot * 0x4000);
				INT32 Page = PSlot << 1;
				RAMMapper[PSlot] = data;
				MemMap[RAMSLOT][Page] = RAMData + (data << 14);
				MemMap[RAMSLOT][Page + 1] = MemMap[RAMSLOT][Page] + 0x2000;

				if ((PSL[PSlot] == RAMSLOT))	{
					WriteMode[PSlot] = 1;
					RAM[Page] = MemMap[RAMSLOT][Page];
					RAM[Page + 1] = MemMap[RAMSLOT][Page + 1];
				}
			}
			return;
#endif
	}

	//bprintf(0, _T("port[%X] data[%X],"), port, data);
}

static UINT8 __fastcall msx_read_port(UINT16 port)
{
	port &= 0xff;

	switch (port)
	{
		case 0x98:
			return TMS9928AReadVRAM();

		case 0x99:
			return TMS9928AReadRegs() | ((VBlankKludge) ? 0x80 : 0x00);

		case 0xa2:
			return AY8910Read(0);

		case 0xa8:
		case 0xa9:
		case 0xaa:
		case 0xab:
			return ppi8255_r(0, port & 3);

		case 0xd9: {
			UINT8 Kan = (use_kanji) ? kanji_rom[Kana + KanaByte] : 0xff;
			KanaByte = (KanaByte + 1) & 0x1f;
			return Kan;
		}

		case 0xfc: // map ram-page 0x0000, 0x4000, 0x8000, 0xc000
		case 0xfd:
		case 0xfe:
		case 0xff:
			return RAMMapper[port - 0xfc] | ~RAMMask;
	}

	//bprintf(0, _T("port[%X],"), port);

	return 0xff;
}

static UINT8 msx_ppi8255_portB_read()
{
	return keyRowGet(ppiC_row);
}

static void msx_ppi8255_portA_write(UINT8 data)
{
	SetSlot(data);
}

static void msx_ppi8255_portC_write(UINT8 data)
{
	ppiC_row = data & 0x0f;
	if (DrvDips[0] & 0x02)
		DACWrite(0, (data & 0x80) ? 0x80 : 0x00); // Key-Clicker / 1-bit DAC
}

static UINT8 ay8910portAread(UINT32 /*offset*/)
{
	if (SwapJoyports) {
		return (Joyselect) ? DrvInputs[0] : DrvInputs[1];
	} else {
		return (Joyselect) ? DrvInputs[1] : DrvInputs[0];
	}
}

static void ay8910portAwrite(UINT32 /*offset*/, UINT32 /*data*/)
{
	//bprintf(0, _T("8910 portAwrite %X:%X\n"), offset, data);
}

static void ay8910portBwrite(UINT32 /*offset*/, UINT32 data)
{
	//bprintf(0, _T("B %X:%X\n"), offset, data);
	Joyselect = (data & 0x40) ? 1 : 0;
}

static void vdp_interrupt(INT32 state)
{
	ZetSetIRQLine(0, state ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	memset(&keyRows, 0, sizeof(keyRows));
	ppiC_row = 0;

	Kana = 0;
	KanaByte = 0;

	msxinit(CurRomSizeA);

	ppi8255_init(1); // there is no reset, so use this.

	ZetOpen(0);
	ZetReset();
	TMS9928AReset();
	ZetClose();

	AY8910Reset(0);
	K051649Reset();
	DACReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	maincpu		    = Next; Next += 0x020000;
	game		    = Next; Next += MAX_MSX_CARTSIZE;
	game2		    = Next; Next += MAX_MSX_CARTSIZE;
	kanji_rom       = Next; Next += 0x040000;

	game_sram       = Next; Next += 0x004000;

	AllRam			= Next;

	main_mem		= Next; Next += 0x020000;
	EmptyRAM        = Next; Next += 0x010000;

	RamEnd			= Next;

	pAY8910Buffer[0]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[1]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[2]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);

	MemEnd			= Next;

	return 0;
}

static void __fastcall msx_write(UINT16 address, UINT8 data)
{
	if (WriteMode[address >> 14]) {
		RAM[address >> 13][address & 0x1fff] = data;
		return;
	}

	if ((address > 0x3fff) && (address < 0xc000))
		Mapper_write(address, data);

}

static UINT8 __fastcall msx_read(UINT16 address)
{
	UINT8 d = 0;

	if (Mapper_read(address, &d)) {
		return d;
	}

	return (RAM[address >> 13][address & 0x1fff]);
}

static INT32 DrvSyncDAC()
{
	return (INT32)(float)(nBurnSoundLen * (ZetTotalCycles() / (3579545.000 / ((Hertz60) ? 60.0 : 50.0))));
}

static INT32 DrvInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		struct BurnRomInfo ri;

		bprintf(0, _T("MSXINIT...\n"));
		Hertz60 = (DrvDips[0] & 0x10) ? 1 : 0;
		BiosmodeJapan = (DrvDips[0] & 0x01) ? 1 : 0;
		SwapJoyports = (DrvDips[0] & 0x20) ? 1 : 0;

		bprintf(0, _T("%Shz mode.\n"), (Hertz60) ? "60" : "50");
		bprintf(0, _T("BIOS mode: %S\n"), (BiosmodeJapan) ? "Japanese" : "Normal");
		bprintf(0, _T("%S"), (SwapJoyports) ? "Joystick Ports: Swapped.\n" : "");
		if (BurnLoadRom(maincpu, 0x80 + BiosmodeJapan, 1)) return 1; // BIOS

		use_kanji = (BurnLoadRom(kanji_rom, 0x82, 1) == 0);

		if (use_kanji)
			bprintf(0, _T("Kanji ROM loaded.\n"));

		BurnDrvGetRomInfo(&ri, 0);

		if (ri.nLen > MAX_MSX_CARTSIZE) {
			bprintf(0, _T("Bad MSX1 ROMSize! exiting.. (> %dk) \n"), MAX_MSX_CARTSIZE / 1024);
			return 1;
		}

		memset(game, 0xff, MAX_MSX_CARTSIZE);

		if (BurnLoadRom(game + 0x00000, 0, 1)) return 1;

		CurRomSizeA = ri.nLen;

		BurnDrvGetRomInfo(&ri, 1);

		if (ri.nLen > 0 && ri.nLen < MAX_MSX_CARTSIZE) {
			memset(game2, 0xff, MAX_MSX_CARTSIZE);

			if (BurnLoadRom(game2 + 0x00000, 1, 1)) return 1;

			CurRomSizeB = ri.nLen;
			bprintf(0, _T("Loaded secondary tape/rom, size: %d.\n"), CurRomSizeB);
		}

		// msxinit(ri.nLen); (in DrvDoReset()! -dink)
	}
#ifdef BUILD_WIN32
	cBurnerKeyCallback = msxKeyCallback;
	nReplayExternalDataCount = sizeof(keyRows);
	ReplayExternalData = &keyRows[0];
#endif
	BurnSetRefreshRate((Hertz60) ? 60.0 : 50.0);

	ZetInit(0);
	ZetOpen(0);

	ZetSetOutHandler(msx_write_port);
	ZetSetInHandler(msx_read_port);
	ZetSetWriteHandler(msx_write);
	ZetSetReadHandler(msx_read);
	ZetClose();

	AY8910Init(0, 3579545/2, nBurnSoundRate, ay8910portAread, NULL, ay8910portAwrite, ay8910portBwrite);
	AY8910SetAllRoutes(0, 0.15, BURN_SND_ROUTE_BOTH);

	K051649Init(3579545/2);
	K051649SetRoute(0.20, BURN_SND_ROUTE_BOTH);

	DACInit(0, 0, 1, DrvSyncDAC);
	DACSetRoute(0, 0.30, BURN_SND_ROUTE_BOTH);

	TMS9928AInit(TMS99x8A, 0x4000, 0, 0, vdp_interrupt);

	ppi8255_init(1);
	PPI0PortReadB	= msx_ppi8255_portB_read;
	PPI0PortWriteA	= msx_ppi8255_portA_write;
	PPI0PortWriteC	= msx_ppi8255_portC_write;

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	TMS9928AExit();
	ZetExit();

	AY8910Exit(0);
	K051649Exit();
	DACExit();

	ppi8255_exit();

	BurnFree (AllMem);
	AllMem = NULL;

	msx_basicmode = 0;
	BiosmodeJapan = 0;
	CASMode = 0;
	VBlankKludge = 0;
	SwapRamslot = 0;
	SwapButton2 = 0;
	SwapSlash = 0;
	MapCursorToJoy1 = 0;

#ifdef BUILD_WIN32
	cBurnerKeyCallback = NULL;
	nReplayExternalDataCount = 0;
	ReplayExternalData = NULL;
#endif
	return 0;
}

static INT32 DrvFrame()
{
	static UINT8 lastnmi = 0;

	if (DrvReset) {
		DrvDoReset();
	}

	{ // Compile Inputs
		memset (DrvInputs, 0xff, 2);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}

		if (SwapButton2)
		{ // Kludge for Xenon and Astro Marine Corps where button #2 is the 'm' key.
			static INT32 lastM = 0;
			if (DrvJoy1[5]) {
				keyInput('m', DrvJoy1[5]);
			} else {
				if (lastM) { // only turn 'm' off once after Button2 is unpressed.
					keyInput('m', DrvJoy1[5]);
				}
			}
			lastM = DrvJoy1[5];
		}

		SwapJoyports = (DrvDips[0] & 0x20) ? 1 : 0;
		MapCursorToJoy1 = (DrvDips[0] & 0x80) ? 1 : 0;

		// Keyboard fun!
		keyInput(0xf1, DrvJoy4[3]); // f1 - f6
		keyInput(0xf2, DrvJoy4[4]);
		keyInput(0xf3, DrvJoy4[5]);
		keyInput(0xf4, DrvJoy4[6]);
		keyInput(0xf5, DrvJoy4[7]);
		keyInput(0xf6, DrvJoy4[8]);

		if (MapCursorToJoy1)
		{ // Mapped to Joy #1
			keyInput(0xf8, DrvJoy1[0]);  // Key UP
			keyInput(0xf9, DrvJoy1[1]); // Key DOWN
			keyInput(0xfa, DrvJoy1[2]); // Key LEFT
			keyInput(0xfb, DrvJoy1[3]); // Key RIGHT
			keyInput(' ', DrvJoy1[4]);
		} else
		{ // Normal Cursor-key function
			keyInput(0xf8, DrvJoy4[9]);  // Key UP
			keyInput(0xf9, DrvJoy4[10]); // Key DOWN
			keyInput(0xfa, DrvJoy4[11]); // Key LEFT
			keyInput(0xfb, DrvJoy4[12]); // Key RIGHT
		}
	}

	{   // detect tape side changes
		CASSide = (DrvDips[0] & 0x40) ? 1 : 0;
		if (CASSideLast != CASSide) {
			bprintf(0, _T("Tape change: Side %c\n"), (CASSide) ? 'B' : 'A');
			CASSideChange();
		}

		// cassette auto-load keyboard stuffing
		if (CASMode && CASFrameCounter > 250 && CASFrameCounter & 2)
			CASAutoLoadTick();
		CASFrameCounter++;
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[1] = { 3579545 / ((Hertz60) ? 60 : 50) };
	INT32 nCyclesDone[1] = { 0 };
	INT32 nSoundBufferPos = 0;

	ZetNewFrame();
	ZetOpen(0);

	if (DrvNMI && !lastnmi) {
		ZetNmi();
		lastnmi = DrvNMI;
	} else lastnmi = DrvNMI;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);

		TMS9928AScanline(i);

		// Render Sound Segment
		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			AY8910Render(&pAY8910Buffer[0], pSoundBuf, nSegmentLength, 0);
			K051649Update(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	ZetClose();

	// Make sure the buffer is entirely filled.
	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			AY8910Render(&pAY8910Buffer[0], pSoundBuf, nSegmentLength, 0);
			K051649Update(pSoundBuf, nSegmentLength);
		}
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		TMS9928ADraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029708;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);
		AY8910Scan(nAction, pnMin);
		K051649Scan(nAction, pnMin);
		DACScan(nAction,pnMin);
		TMS9928AScan(nAction, pnMin);
		ppi8255_scan();

		SCAN_VAR(RAMMapper);
		SCAN_VAR(ROMMapper);
		SCAN_VAR(WriteMode);
		SCAN_VAR(PSL);
		SCAN_VAR(PSLReg);
		SCAN_VAR(SCCReg);

		SCAN_VAR(dooly_prot);
		SCAN_VAR(crossblaim_selected_bank);
		SCAN_VAR(rtype_selected_bank);

		SCAN_VAR(CASPos);
		SCAN_VAR(CASAutoLoadPos);
		SCAN_VAR(CASAutoLoadTicker);
		SCAN_VAR(CASFrameCounter);
	}

	if (nAction & ACB_WRITE) {
		if (RAMMask) { // re-map ram
			for (INT32 i = 0; i < 4; i++) {
				RAMMapper[i] &= RAMMask;
				MemMap[RAMSLOT][i * 2] = RAMData + RAMMapper[i] * 0x4000;
				MemMap[RAMSLOT][i * 2 + 1] = MemMap[RAMSLOT][i * 2] + 0x2000;
			}
		}

		for (INT32 i = 0; i < MAXSLOTS; i++)
			if (ROMData[i] && ROMMask[i]) {
				MapMegaROM(i, ROMMapper[i][0], ROMMapper[i][1], ROMMapper[i][2], ROMMapper[i][3]);
				crossblaim_do_bank(ROMData[i]);
				rtype_do_bank(ROMData[i]);
			}

		for (INT32 i = 0; i < 4; i++) {
			RAM[2 * i] = MemMap[PSL[i]][2 * i];
			RAM[2 * i + 1] = MemMap[PSL[i]][2 * i + 1];
		}
	}

	return 0;
}

INT32 MSXGetZipName(char** pszName, UINT32 i)
{
	static char szFilename[MAX_PATH];
	char* pszGameName = NULL;

	if (pszName == NULL) {
		return 1;
	}

	if (i == 0) {
		pszGameName = BurnDrvGetTextA(DRV_NAME);
	} else {
		if (i == 1 && BurnDrvGetTextA(DRV_BOARDROM)) {
			pszGameName = BurnDrvGetTextA(DRV_BOARDROM);
		} else {
			pszGameName = BurnDrvGetTextA(DRV_PARENT);
		}
	}

	if (pszGameName == NULL) {
		*pszName = NULL;
		return 1;
	}
   // remove msx_
	for (UINT32 j = 0; j < strlen(pszGameName); j++) {
		szFilename[j] = pszGameName[j + 4];
	}

	*pszName = szFilename;

	return 0;
}

// MSX1 BIOS
static struct BurnRomInfo msx_msxRomDesc[] = {
    { "msx.rom",     0x8000, 0xa317e6b4, BRF_BIOS }, // 0x80 - standard bios
    { "msxj.rom",    0x8000, 0x071135e0, BRF_BIOS | BRF_OPT }, // 0x81 - japanese bios
    { "kanji.rom",   0x40000, 0x1f6406fb, BRF_BIOS | BRF_OPT }, // 0x82 - kanji support
};

STD_ROM_PICK(msx_msx)
STD_ROM_FN(msx_msx)

struct BurnDriver BurnDrvmsx_msx = {
	"msx_msx", NULL, NULL, NULL, "1982",
	"MSX1 System BIOS\0", "BIOS only", "MSX", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_BOARDROM, 0, HARDWARE_MSX, GBF_BIOS, 0,
	MSXGetZipName, msx_msxRomInfo, msx_msxRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

static INT32 SwapSlashDrvInit()
{
	SwapSlash = 1;
	return DrvInit();
}

static INT32 SwapButton2DrvInit()
{
	SwapButton2 = 1;
	return DrvInit();
}

static INT32 SwapRamDrvInit()
{
	SwapRamslot = 1;
	return DrvInit();
}

static INT32 BasicDrvInit()
{
	msx_basicmode = 1;
	return DrvInit();
}

static INT32 CasBloadDrvInit()
{
	msx_basicmode = 1;
	CASMode = CAS_BLOAD;
	return DrvInit();
}

static INT32 CasRunSwapButton2DrvInit()
{
	msx_basicmode = 1;
	SwapButton2 = 1;
	CASMode = CAS_RUN;
	return DrvInit();
}

static INT32 CasRunDrvInit()
{
	msx_basicmode = 1;
	CASMode = CAS_RUN;
	return DrvInit();
}

static INT32 CasCloadDrvInit()
{
	msx_basicmode = 1;
	CASMode = CAS_CLOAD;
	return DrvInit();
}

static INT32 CasCloadRRDrvInit()
{ // special for perez the mouse :)
	msx_basicmode = 1;
	CASMode = CAS_CLOADRR;
	return DrvInit();
}

static INT32 VoidrunnerDrvInit()
{
	msx_basicmode = 1;
	VBlankKludge = 1;
	CASMode = CAS_RUN;
	return DrvInit();
}

// MSX1 Basic

static struct BurnRomInfo MSX_msxbasicRomDesc[] = {
	{ "msx.rom",	0x08000, 0xa317e6b4, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_msxbasic, MSX_msxbasic, msx_msx)
STD_ROM_FN(MSX_msxbasic)

struct BurnDriver BurnDrvMSX_msxbasic = {
	"msx_msxbasic", NULL, "msx_msx", NULL, "1983",
	"MSX Basic\0", NULL, "Microsoft / ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_msxbasicRomInfo, MSX_msxbasicRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	BasicDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// 1942 (Jpn)

static struct BurnRomInfo MSX_1942RomDesc[] = {
	{ "nc81820-g30 japan 8649",	0x20000, 0xa27787af, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_1942, MSX_1942, msx_msx)
STD_ROM_FN(MSX_1942)

struct BurnDriver BurnDrvMSX_1942 = {
	"msx_1942", NULL, "msx_msx", NULL, "1986",
	"1942 (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII8, GBF_MISC, 0,
	MSXGetZipName, MSX_1942RomInfo, MSX_1942RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// The Black Onyx II - Search For The Fire Crystal (Jpn)

static struct BurnRomInfo MSX_blckony2RomDesc[] = {
	{ "lh231013",	0x20000, 0x67bf8337, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_blckony2, MSX_blckony2, msx_msx)
STD_ROM_FN(MSX_blckony2)

struct BurnDriver BurnDrvMSX_blckony2 = {
	"msx_blckony2", NULL, "msx_msx", NULL, "1986",
	"The Black Onyx II - Search For The Fire Crystal (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII16, GBF_MISC, 0,
	MSXGetZipName, MSX_blckony2RomInfo, MSX_blckony2RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Borfes to 5-nin no Akuma - An Adventure Story (Jpn)

static struct BurnRomInfo MSX_borfesRomDesc[] = {
	{ "lh231066",	0x20000, 0xe040e8a1, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_borfes, MSX_borfes, msx_msx)
STD_ROM_FN(MSX_borfes)

struct BurnDriver BurnDrvMSX_borfes = {
	"msx_borfes", NULL, "msx_msx", NULL, "1987",
	"Borfes to 5-nin no Akuma - An Adventure Story (Jpn)\0", NULL, "Xtal Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII16, GBF_MISC, 0,
	MSXGetZipName, MSX_borfesRomInfo, MSX_borfesRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Daiva Story 4 - Asura's Bloodfeud (Jpn)

static struct BurnRomInfo MSX_daiva4RomDesc[] = {
	{ "lh532020",	0x40000, 0x99198ed9, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_daiva4, MSX_daiva4, msx_msx)
STD_ROM_FN(MSX_daiva4)

struct BurnDriver BurnDrvMSX_daiva4 = {
	"msx_daiva4", NULL, "msx_msx", NULL, "1987",
	"Daiva Story 4 - Asura's Bloodfeud (Jpn)\0", NULL, "T&E Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII8, GBF_MISC, 0,
	MSXGetZipName, MSX_daiva4RomInfo, MSX_daiva4RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Digital Devil Monogatari - Megami Tensei (Jpn)

static struct BurnRomInfo MSX_megamitRomDesc[] = {
	{ "lh231089",	0x20000, 0x25fc11fa, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_megamit, MSX_megamit, msx_msx)
STD_ROM_FN(MSX_megamit)

struct BurnDriver BurnDrvMSX_megamit = {
	"msx_megamit", NULL, "msx_msx", NULL, "1987",
	"Digital Devil Monogatari - Megami Tensei (Jpn)\0", NULL, "Nihon Telenet", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII8, GBF_MISC, 0,
	MSXGetZipName, MSX_megamitRomInfo, MSX_megamitRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Dragon Quest (Jpn)

static struct BurnRomInfo MSX_dquestRomDesc[] = {
	{ "cxk381000",	0x20000, 0x49f5478b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_dquest, MSX_dquest, msx_msx)
STD_ROM_FN(MSX_dquest)

struct BurnDriver BurnDrvMSX_dquest = {
	"msx_dquest", NULL, "msx_msx", NULL, "1986",
	"Dragon Quest (Jpn)\0", NULL, "Enix", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII8, GBF_MISC, 0,
	MSXGetZipName, MSX_dquestRomInfo, MSX_dquestRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Dragon Quest II - Akuryou no Kamigami (Jpn)

static struct BurnRomInfo MSX_dquest2RomDesc[] = {
	{ "lh532060",	0x40000, 0x8076fec6, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_dquest2, MSX_dquest2, msx_msx)
STD_ROM_FN(MSX_dquest2)

struct BurnDriver BurnDrvMSX_dquest2 = {
	"msx_dquest2", NULL, "msx_msx", NULL, "1987",
	"Dragon Quest II - Akuryou no Kamigami (Jpn)\0", NULL, "Enix", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII8, GBF_MISC, 0,
	MSXGetZipName, MSX_dquest2RomInfo, MSX_dquest2RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Dragon Slayer IV - Drasle Family (Jpn)

static struct BurnRomInfo MSX_dslayer4RomDesc[] = {
	{ "lh5320z5",	0x40000, 0x87dcd309, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_dslayer4, MSX_dslayer4, msx_msx)
STD_ROM_FN(MSX_dslayer4)

struct BurnDriver BurnDrvMSX_dslayer4 = {
	"msx_dslayer4", NULL, "msx_msx", NULL, "1987",
	"Dragon Slayer IV - Drasle Family (Jpn)\0", NULL, "Nihon Falcom", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII8, GBF_MISC, 0,
	MSXGetZipName, MSX_dslayer4RomInfo, MSX_dslayer4RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Dungeon Hunter (Jpn)

static struct BurnRomInfo MSX_dngnhntrRomDesc[] = {
	{ "rp231026d",	0x20000, 0x2526e568, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_dngnhntr, MSX_dngnhntr, msx_msx)
STD_ROM_FN(MSX_dngnhntr)

struct BurnDriver BurnDrvMSX_dngnhntr = {
	"msx_dngnhntr", NULL, "msx_msx", NULL, "1989",
	"Dungeon Hunter (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII16, GBF_MISC, 0,
	MSXGetZipName, MSX_dngnhntrRomInfo, MSX_dngnhntrRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Fantasy Zone (Jpn)

static struct BurnRomInfo MSX_fantzoneRomDesc[] = {
	{ "lh531057",	0x20000, 0x3e96d005, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_fantzone, MSX_fantzone, msx_msx)
STD_ROM_FN(MSX_fantzone)

struct BurnDriver BurnDrvMSX_fantzone = {
	"msx_fantzone", NULL, "msx_msx", NULL, "1986",
	"Fantasy Zone (Jpn)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII8, GBF_MISC, 0,
	MSXGetZipName, MSX_fantzoneRomInfo, MSX_fantzoneRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Final Zone (Jpn)

static struct BurnRomInfo MSX_fzoneRomDesc[] = {
	{ "fzr-2001",	0x20000, 0x14e2efcc, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_fzone, MSX_fzone, msx_msx)
STD_ROM_FN(MSX_fzone)

struct BurnDriver BurnDrvMSX_fzone = {
	"msx_fzone", NULL, "msx_msx", NULL, "1986",
	"Final Zone (Jpn)\0", NULL, "Nihon Telenet", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII8, GBF_MISC, 0,
	MSXGetZipName, MSX_fzoneRomInfo, MSX_fzoneRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Flight Simulator - Gyorai Kougeki (Jpn)

static struct BurnRomInfo MSX_fsimRomDesc[] = {
	{ "rp231024d",	0x20000, 0xa6165bd4, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_fsim, MSX_fsim, msx_msx)
STD_ROM_FN(MSX_fsim)

struct BurnDriver BurnDrvMSX_fsim = {
	"msx_fsim", NULL, "msx_msx", NULL, "1988",
	"Flight Simulator - Gyorai Kougeki (Jpn)\0", NULL, "subLOGIC", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII8, GBF_MISC, 0,
	MSXGetZipName, MSX_fsimRomInfo, MSX_fsimRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Gambler Jikochuushinha (Jpn)

static struct BurnRomInfo MSX_gamblerRomDesc[] = {
	{ "lh53210a game arts",	0x40000, 0x91955bcd, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_gambler, MSX_gambler, msx_msx)
STD_ROM_FN(MSX_gambler)

struct BurnDriver BurnDrvMSX_gambler = {
	"msx_gambler", NULL, "msx_msx", NULL, "1988",
	"Gambler Jikochuushinha (Jpn)\0", NULL, "Game Arts", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII8, GBF_MISC, 0,
	MSXGetZipName, MSX_gamblerRomInfo, MSX_gamblerRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Golvellius (Jpn)

static struct BurnRomInfo MSX_golvellRomDesc[] = {
	{ "compile ia-8701",	0x20000, 0x5eac55df, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_golvell, MSX_golvell, msx_msx)
STD_ROM_FN(MSX_golvell)

struct BurnDriver BurnDrvMSX_golvell = {
	"msx_golvell", NULL, "msx_msx", NULL, "1987",
	"Golvellius (Jpn)\0", NULL, "Compile", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII16, GBF_MISC, 0,
	MSXGetZipName, MSX_golvellRomInfo, MSX_golvellRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Hydlide II - Shine of Darkness (Jpn)

static struct BurnRomInfo MSX_hydlide2RomDesc[] = {
	{ "lh531043",	0x20000, 0xd8055f5f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_hydlide2, MSX_hydlide2, msx_msx)
STD_ROM_FN(MSX_hydlide2)

struct BurnDriver BurnDrvMSX_hydlide2 = {
	"msx_hydlide2", NULL, "msx_msx", NULL, "1986",
	"Hydlide II - Shine of Darkness (Jpn)\0", NULL, "T&E Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII16_SRAM, GBF_MISC, 0,
	MSXGetZipName, MSX_hydlide2RomInfo, MSX_hydlide2RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Hydlide 3 - The Space Memories (Jpn)

static struct BurnRomInfo MSX_hydlide3RomDesc[] = {
	{ "hydlide3 4m-rom",	0x80000, 0x00c5d5b5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_hydlide3, MSX_hydlide3, msx_msx)
STD_ROM_FN(MSX_hydlide3)

struct BurnDriver BurnDrvMSX_hydlide3 = {
	"msx_hydlide3", NULL, "msx_msx", NULL, "1987",
	"Hydlide 3 - The Space Memories (Jpn)\0", NULL, "T&E Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII8, GBF_MISC, 0,
	MSXGetZipName, MSX_hydlide3RomInfo, MSX_hydlide3RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Jagur (Jpn)

static struct BurnRomInfo MSX_jagurRomDesc[] = {
	{ "rp231024d",	0x20000, 0xd6465702, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_jagur, MSX_jagur, msx_msx)
STD_ROM_FN(MSX_jagur)

struct BurnDriver BurnDrvMSX_jagur = {
	"msx_jagur", NULL, "msx_msx", NULL, "1987",
	"Jagur (Jpn)\0", NULL, "Hudson Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII16, GBF_MISC, 0,
	MSXGetZipName, MSX_jagurRomInfo, MSX_jagurRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// King's Knight (Jpn)

static struct BurnRomInfo MSX_kingkngtRomDesc[] = {
	{ "643103",	0x20000, 0xa0481321, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_kingkngt, MSX_kingkngt, msx_msx)
STD_ROM_FN(MSX_kingkngt)

struct BurnDriver BurnDrvMSX_kingkngt = {
	"msx_kingkngt", NULL, "msx_msx", NULL, "1986",
	"King's Knight (Jpn)\0", NULL, "Square", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII16, GBF_MISC, 0,
	MSXGetZipName, MSX_kingkngtRomInfo, MSX_kingkngtRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Marchen Veil I (Jpn)

static struct BurnRomInfo MSX_marchenRomDesc[] = {
	{ "cxk381000",	0x20000, 0x44aa5422, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_marchen, MSX_marchen, msx_msx)
STD_ROM_FN(MSX_marchen)

struct BurnDriver BurnDrvMSX_marchen = {
	"msx_marchen", NULL, "msx_msx", NULL, "1987",
	"Marchen Veil I (Jpn)\0", NULL, "System Sacom", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII8, GBF_MISC, 0,
	MSXGetZipName, MSX_marchenRomInfo, MSX_marchenRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Mirai (Jpn)

static struct BurnRomInfo MSX_miraiRomDesc[] = {
	{ "rp231024d",	0x20000, 0x72eb9d0e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mirai, MSX_mirai, msx_msx)
STD_ROM_FN(MSX_mirai)

struct BurnDriver BurnDrvMSX_mirai = {
	"msx_mirai", NULL, "msx_msx", NULL, "1987",
	"Mirai (Jpn)\0", NULL, "Xain Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII8, GBF_MISC, 0,
	MSXGetZipName, MSX_miraiRomInfo, MSX_miraiRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Mugen Senshi Valis (Jpn)

static struct BurnRomInfo MSX_valisRomDesc[] = {
	{ "831000-440 20 bk z86",	0x20000, 0x309d996c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_valis, MSX_valis, msx_msx)
STD_ROM_FN(MSX_valis)

struct BurnDriver BurnDrvMSX_valis = {
	"msx_valis", NULL, "msx_msx", NULL, "1986",
	"Mugen Senshi Valis (Jpn)\0", NULL, "Nihon Telenet", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII8, GBF_MISC, 0,
	MSXGetZipName, MSX_valisRomInfo, MSX_valisRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Relics (Jpn)

static struct BurnRomInfo MSX_relicsRomDesc[] = {
	{ "cxk381000",	0x20000, 0xb612d79a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_relics, MSX_relics, msx_msx)
STD_ROM_FN(MSX_relics)

struct BurnDriver BurnDrvMSX_relics = {
	"msx_relics", NULL, "msx_msx", NULL, "1986",
	"Relics (Jpn)\0", NULL, "Bothtec", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII8, GBF_MISC, 0,
	MSXGetZipName, MSX_relicsRomInfo, MSX_relicsRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Romancia (Jpn)

static struct BurnRomInfo MSX_romanciaRomDesc[] = {
	{ "831000-20",	0x20000, 0x387c1de7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_romancia, MSX_romancia, msx_msx)
STD_ROM_FN(MSX_romancia)

struct BurnDriver BurnDrvMSX_romancia = {
	"msx_romancia", NULL, "msx_msx", NULL, "1987",
	"Romancia (Jpn)\0", NULL, "Nihon Falcom", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII16, GBF_MISC, 0,
	MSXGetZipName, MSX_romanciaRomInfo, MSX_romanciaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// R-Type (Jpn)

static struct BurnRomInfo MSX_rtypeRomDesc[] = {
	{ "R-Type (1988) (Irem) (J).rom",	0x60000, 0xa884911c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_rtype, MSX_rtype, msx_msx)
STD_ROM_FN(MSX_rtype)

struct BurnDriver BurnDrvMSX_rtype = {
	"msx_rtype", NULL, "msx_msx", NULL, "1988",
	"R-Type (Jpn)\0", NULL, "Irem", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_RTYPE, GBF_MISC, 0,
	MSXGetZipName, MSX_rtypeRomInfo, MSX_rtypeRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// R-Type (Jpn, Alt)

static struct BurnRomInfo MSX_rtypeaRomDesc[] = {
	{ "r-type (japan) (alt 2).rom",	0x80000, 0x827919e4, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_rtypea, MSX_rtypea, msx_msx)
STD_ROM_FN(MSX_rtypea)

struct BurnDriver BurnDrvMSX_rtypea = {
	"msx_rtypea", "msx_rtype", "msx_msx", NULL, "1988",
	"R-Type (Jpn, Alt)\0", NULL, "Irem", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_RTYPE, GBF_MISC, 0,
	MSXGetZipName, MSX_rtypeaRomInfo, MSX_rtypeaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Senjou no Ookami (Jpn)

static struct BurnRomInfo MSX_senjokamRomDesc[] = {
	{ "lh231073",	0x20000, 0x427b3f14, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_senjokam, MSX_senjokam, msx_msx)
STD_ROM_FN(MSX_senjokam)

struct BurnDriver BurnDrvMSX_senjokam = {
	"msx_senjokam", NULL, "msx_msx", NULL, "1987",
	"Senjou no Ookami (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII8, GBF_MISC, 0,
	MSXGetZipName, MSX_senjokamRomInfo, MSX_senjokamRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Shougun (Jpn)

static struct BurnRomInfo MSX_shogunRomDesc[] = {
	{ "lh23100y",	0x20000, 0x0e691c47, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_shogun, MSX_shogun, msx_msx)
STD_ROM_FN(MSX_shogun)

struct BurnDriver BurnDrvMSX_shogun = {
	"msx_shogun", NULL, "msx_msx", NULL, "1987",
	"Shougun (Jpn)\0", NULL, "Nihon Dexter", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII8_SRAM, GBF_MISC, 0,
	MSXGetZipName, MSX_shogunRomInfo, MSX_shogunRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Super Laydock - Mission Striker (Jpn)

static struct BurnRomInfo MSX_slaydockRomDesc[] = {
	{ "lh532072",	0x40000, 0x5dc45624, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_slaydock, MSX_slaydock, msx_msx)
STD_ROM_FN(MSX_slaydock)

struct BurnDriver BurnDrvMSX_slaydock = {
	"msx_slaydock", NULL, "msx_msx", NULL, "1987",
	"Super Laydock - Mission Striker (Jpn)\0", NULL, "T&E Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII8, GBF_MISC, 0,
	MSXGetZipName, MSX_slaydockRomInfo, MSX_slaydockRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Vaxol (Jpn)

static struct BurnRomInfo MSX_vaxolRomDesc[] = {
	{ "rp231024d",	0x20000, 0xfea70207, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_vaxol, MSX_vaxol, msx_msx)
STD_ROM_FN(MSX_vaxol)

struct BurnDriver BurnDrvMSX_vaxol = {
	"msx_vaxol", NULL, "msx_msx", NULL, "1987",
	"Vaxol (Jpn)\0", NULL, "Heart Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII16, GBF_MISC, 0,
	MSXGetZipName, MSX_vaxolRomInfo, MSX_vaxolRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Wing Man 2 - Kitakura no Fukkatsu (Jpn)

static struct BurnRomInfo MSX_wingman2RomDesc[] = {
	{ "enix wing2 rp231024d 0408",	0x20000, 0x5c9d8f62, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_wingman2, MSX_wingman2, msx_msx)
STD_ROM_FN(MSX_wingman2)

struct BurnDriver BurnDrvMSX_wingman2 = {
	"msx_wingman2", NULL, "msx_msx", NULL, "1987",
	"Wing Man 2 - Kitakura no Fukkatsu (Jpn)\0", NULL, "Enix", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII8, GBF_MISC, 0,
	MSXGetZipName, MSX_wingman2RomInfo, MSX_wingman2RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Xanadu - Dragon Slayer II (Jpn)

static struct BurnRomInfo MSX_xanaduRomDesc[] = {
	{ "lh532051",	0x40000, 0xd640deaf, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_xanadu, MSX_xanadu, msx_msx)
STD_ROM_FN(MSX_xanadu)

struct BurnDriver BurnDrvMSX_xanadu = {
	"msx_xanadu", NULL, "msx_msx", NULL, "1987",
	"Xanadu - Dragon Slayer II (Jpn)\0", NULL, "Nihon Falcom", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_xanaduRomInfo, MSX_xanaduRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// 10-Yard Fight (Jpn)

static struct BurnRomInfo MSX_10yardRomDesc[] = {
	{ "10-yard fight (japan).rom",	0x08000, 0x4e20d256, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_10yard, MSX_10yard, msx_msx)
STD_ROM_FN(MSX_10yard)

struct BurnDriver BurnDrvMSX_10yard = {
	"msx_10yard", NULL, "msx_msx", NULL, "1986",
	"10-Yard Fight (Jpn)\0", NULL, "Irem", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_10yardRomInfo, MSX_10yardRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// 10-Yard Fight (Jpn, Alt)

static struct BurnRomInfo MSX_10yardaRomDesc[] = {
	{ "10-yard fight (japan) (alt 1).rom",	0x08000, 0x879e6ddb, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_10yarda, MSX_10yarda, msx_msx)
STD_ROM_FN(MSX_10yarda)

struct BurnDriver BurnDrvMSX_10yarda = {
	"msx_10yarda", "msx_10yard", "msx_msx", NULL, "1986",
	"10-Yard Fight (Jpn, Alt)\0", NULL, "Irem", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_10yardaRomInfo, MSX_10yardaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// 1942 (Jpn, Alt)

static struct BurnRomInfo MSX_1942aRomDesc[] = {
	{ "1942 (japan) (alt 1).rom",	0x20000, 0xe54ee601, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_1942a, MSX_1942a, msx_msx)
STD_ROM_FN(MSX_1942a)

struct BurnDriver BurnDrvMSX_1942a = {
	"msx_1942a", "msx_1942", "msx_msx", NULL, "1986",
	"1942 (Jpn, Alt)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII8, GBF_MISC, 0,
	MSXGetZipName, MSX_1942aRomInfo, MSX_1942aRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// 1942 (Kor)

static struct BurnRomInfo MSX_1942kRomDesc[] = {
	{ "1942 (zemina).rom",	0x20000, 0x10d97b9e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_1942k, MSX_1942k, msx_msx)
STD_ROM_FN(MSX_1942k)

struct BurnDriver BurnDrvMSX_1942k = {
	"msx_1942k", "msx_1942", "msx_msx", NULL, "1987",
	"1942 (Kor)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_KONAMI, GBF_MISC, 0,
	MSXGetZipName, MSX_1942kRomInfo, MSX_1942kRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// A Life Planet - M36 - Mother Brain has been aliving (Jpn)

static struct BurnRomInfo MSX_m36RomDesc[] = {
	{ "m36 - a life planet (japan).rom",	0x20000, 0xfa8f9bbc, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_m36, MSX_m36, msx_msx)
STD_ROM_FN(MSX_m36)

struct BurnDriver BurnDrvMSX_m36 = {
	"msx_m36", NULL, "msx_msx", NULL, "1987",
	"A Life Planet - M36 - Mother Brain has been aliving (Jpn)\0", NULL, "Pixel", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII8, GBF_MISC, 0,
	MSXGetZipName, MSX_m36RomInfo, MSX_m36RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// A Na Za - Kaleidoscope Special (Jpn)

static struct BurnRomInfo MSX_anazaRomDesc[] = {
	{ "anaza - kaleidoscope special (japan).rom",	0x08000, 0x7dc880eb, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_anaza, MSX_anaza, msx_msx)
STD_ROM_FN(MSX_anaza)

struct BurnDriver BurnDrvMSX_anaza = {
	"msx_anaza", NULL, "msx_msx", NULL, "1987",
	"A Na Za - Kaleidoscope Special (Jpn)\0", NULL, "GA-Yume", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_anazaRomInfo, MSX_anazaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// A.E. (Jpn)

static struct BurnRomInfo MSX_aeRomDesc[] = {
	{ "a.e. (japan).rom",	0x04000, 0x2e1b3dd4, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_ae, MSX_ae, msx_msx)
STD_ROM_FN(MSX_ae)

struct BurnDriver BurnDrvMSX_ae = {
	"msx_ae", NULL, "msx_msx", NULL, "1982",
	"A.E. (Jpn)\0", NULL, "Toshiba EMI", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_aeRomInfo, MSX_aeRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// A1 Spirit - The Way to Formula-1 (Jpn)

static struct BurnRomInfo MSX_a1spiritRomDesc[] = {
	{ "a1 spirit - the way to formula-1 (japan).rom",	0x20000, 0x2608c959, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_a1spirit, MSX_a1spirit, msx_msx)
STD_ROM_FN(MSX_a1spirit)

struct BurnDriver BurnDrvMSX_a1spirit = {
	"msx_a1spirit", "msx_f1spirit", "msx_msx", NULL, "1987",
	"A1 Spirit - The Way to Formula-1 (Jpn)\0", "Set BIOS to Japanese mode.", "Panasoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_KONAMI_SCC, GBF_MISC, 0,
	MSXGetZipName, MSX_a1spiritRomInfo, MSX_a1spiritRomName, NULL, NULL, MSXInputInfo, MSXJapanDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Actman (Jpn)

static struct BurnRomInfo MSX_actmanRomDesc[] = {
	{ "actman (japan).rom",	0x04000, 0xe4dbcdbd, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_actman, MSX_actman, msx_msx)
STD_ROM_FN(MSX_actman)

struct BurnDriver BurnDrvMSX_actman = {
	"msx_actman", NULL, "msx_msx", NULL, "1985",
	"Actman (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_actmanRomInfo, MSX_actmanRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Actman (Jpn, Alt)

static struct BurnRomInfo MSX_actmanaRomDesc[] = {
	{ "actman (japan) (alt 1).rom",	0x04000, 0x7179a4bd, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_actmana, MSX_actmana, msx_msx)
STD_ROM_FN(MSX_actmana)

struct BurnDriver BurnDrvMSX_actmana = {
	"msx_actmana", "msx_actman", "msx_msx", NULL, "1985",
	"Actman (Jpn, Alt)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_actmanaRomInfo, MSX_actmanaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Albatross (Jpn)

static struct BurnRomInfo MSX_albatrosRomDesc[] = {
	{ "albatros (japan).rom",	0x08000, 0x847fc6e2, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_albatros, MSX_albatros, msx_msx)
STD_ROM_FN(MSX_albatros)

struct BurnDriver BurnDrvMSX_albatros = {
	"msx_albatros", NULL, "msx_msx", NULL, "1986",
	"Albatross (Jpn)\0", NULL, "Nihon Telenet", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_albatrosRomInfo, MSX_albatrosRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Albatross (Jpn, Alt)

static struct BurnRomInfo MSX_albatrosaRomDesc[] = {
	{ "albatros (japan) (alt 1).rom",	0x08000, 0xe27f41df, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_albatrosa, MSX_albatrosa, msx_msx)
STD_ROM_FN(MSX_albatrosa)

struct BurnDriver BurnDrvMSX_albatrosa = {
	"msx_albatrosa", "msx_albatros", "msx_msx", NULL, "1986",
	"Albatross (Jpn, Alt)\0", NULL, "Nihon Telenet", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_albatrosaRomInfo, MSX_albatrosaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Alibaba and 40 Thieves (Jpn)

static struct BurnRomInfo MSX_alibabaRomDesc[] = {
	{ "alibaba and 40 thieves (japan).rom",	0x04000, 0x2f72b1e3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_alibaba, MSX_alibaba, msx_msx)
STD_ROM_FN(MSX_alibaba)

struct BurnDriver BurnDrvMSX_alibaba = {
	"msx_alibaba", NULL, "msx_msx", NULL, "1984",
	"Alibaba and 40 Thieves (Jpn)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_alibabaRomInfo, MSX_alibabaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Alibaba and 40 Thieves (Jpn, Alt)

static struct BurnRomInfo MSX_alibabaaRomDesc[] = {
	{ "alibaba and 40 thieves (japan) (alt 1).rom",	0x04000, 0xf1b90309, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_alibabaa, MSX_alibabaa, msx_msx)
STD_ROM_FN(MSX_alibabaa)

struct BurnDriver BurnDrvMSX_alibabaa = {
	"msx_alibabaa", "msx_alibaba", "msx_msx", NULL, "1984",
	"Alibaba and 40 Thieves (Jpn, Alt)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_alibabaaRomInfo, MSX_alibabaaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Alibaba and 40 Thieves (Jpn, Alt 2)

static struct BurnRomInfo MSX_alibababRomDesc[] = {
	{ "alibaba and 40 thieves (japan) (alt 2).rom",	0x04000, 0xf1d176ff, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_alibabab, MSX_alibabab, msx_msx)
STD_ROM_FN(MSX_alibabab)

struct BurnDriver BurnDrvMSX_alibabab = {
	"msx_alibabab", "msx_alibaba", "msx_msx", NULL, "1984",
	"Alibaba and 40 Thieves (Jpn, Alt 2)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_alibababRomInfo, MSX_alibababRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Alien 8 (Jpn)

static struct BurnRomInfo MSX_alien8RomDesc[] = {
	{ "alien 8 (japan).rom",	0x08000, 0x93a25be1, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_alien8, MSX_alien8, msx_msx)
STD_ROM_FN(MSX_alien8)

struct BurnDriver BurnDrvMSX_alien8 = {
	"msx_alien8", NULL, "msx_msx", NULL, "1987",
	"Alien 8 (Jpn)\0", NULL, "Jaleco", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_alien8RomInfo, MSX_alien8RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Alien 8 (Jpn, Hacked?)

static struct BurnRomInfo MSX_alien8hRomDesc[] = {
	{ "alien 8 (japan) (alt 1).rom",	0x08000, 0x3b4ed316, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_alien8h, MSX_alien8h, msx_msx)
STD_ROM_FN(MSX_alien8h)

struct BurnDriver BurnDrvMSX_alien8h = {
	"msx_alien8h", "msx_alien8", "msx_msx", NULL, "1985",
	"Alien 8 (Jpn, Hacked?)\0", NULL, "A.C.G.", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_alien8hRomInfo, MSX_alien8hRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Aliens - Alien 2 (Jpn)

static struct BurnRomInfo MSX_aliensRomDesc[] = {
	{ "aliens - alien 2 (japan).rom",	0x20000, 0xc6fc7bd7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_aliens, MSX_aliens, msx_msx)
STD_ROM_FN(MSX_aliens)

struct BurnDriver BurnDrvMSX_aliens = {
	"msx_aliens", NULL, "msx_msx", NULL, "1987",
	"Aliens - Alien 2 (Jpn)\0", NULL, "Square", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII16, GBF_MISC, 0,
	MSXGetZipName, MSX_aliensRomInfo, MSX_aliensRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Aliens - Alien 2 (Jpn, Alt)

static struct BurnRomInfo MSX_aliensaRomDesc[] = {
	{ "aliens - alien 2 (japan) (alt 1).rom",	0x20000, 0xa6e924ab, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_aliensa, MSX_aliensa, msx_msx)
STD_ROM_FN(MSX_aliensa)

struct BurnDriver BurnDrvMSX_aliensa = {
	"msx_aliensa", "msx_aliens", "msx_msx", NULL, "1987",
	"Aliens - Alien 2 (Jpn, Alt)\0", NULL, "Square", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII16, GBF_MISC, 0,
	MSXGetZipName, MSX_aliensaRomInfo, MSX_aliensaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Aliens - Alien 2 (Jpn, Alt 2)

static struct BurnRomInfo MSX_aliensbRomDesc[] = {
	{ "aliens - alien 2 (japan) (alt 2).rom",	0x20000, 0x3ddcb524, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_aliensb, MSX_aliensb, msx_msx)
STD_ROM_FN(MSX_aliensb)

struct BurnDriver BurnDrvMSX_aliensb = {
	"msx_aliensb", "msx_aliens", "msx_msx", NULL, "1987",
	"Aliens - Alien 2 (Jpn, Alt 2)\0", NULL, "Square", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII16, GBF_MISC, 0,
	MSXGetZipName, MSX_aliensbRomInfo, MSX_aliensbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Alfa Roid (Jpn)

static struct BurnRomInfo MSX_aroidRomDesc[] = {
	{ "alpha roid (japan).rom",	0x08000, 0x4ef7c4e7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_aroid, MSX_aroid, msx_msx)
STD_ROM_FN(MSX_aroid)

struct BurnDriver BurnDrvMSX_aroid = {
	"msx_aroid", NULL, "msx_msx", NULL, "1986",
	"Alfa Roid (Jpn)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_aroidRomInfo, MSX_aroidRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Alfa Roid (Jpn, Alt)

static struct BurnRomInfo MSX_aroidaRomDesc[] = {
	{ "alpha roid (japan) (alt 1).rom",	0x08000, 0x716dc9af, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_aroida, MSX_aroida, msx_msx)
STD_ROM_FN(MSX_aroida)

struct BurnDriver BurnDrvMSX_aroida = {
	"msx_aroida", "msx_aroid", "msx_msx", NULL, "1986",
	"Alfa Roid (Jpn, Alt)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_aroidaRomInfo, MSX_aroidaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// American Truck (Jpn)

static struct BurnRomInfo MSX_amtruckaRomDesc[] = {
	{ "american truck (japan).rom",	0x08000, 0x1dd9b4d9, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_amtrucka, MSX_amtrucka, msx_msx)
STD_ROM_FN(MSX_amtrucka)

struct BurnDriver BurnDrvMSX_amtrucka = {
	"msx_amtrucka", NULL, "msx_msx", NULL, "1985",
	"American Truck (Jpn)\0", NULL, "Nihon Telenet", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_amtruckaRomInfo, MSX_amtruckaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Angelo (Jpn)

static struct BurnRomInfo MSX_angeloRomDesc[] = {
	{ "angelo (japan).rom",	0x04000, 0x20143ec7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_angelo, MSX_angelo, msx_msx)
STD_ROM_FN(MSX_angelo)

struct BurnDriver BurnDrvMSX_angelo = {
	"msx_angelo", NULL, "msx_msx", NULL, "1984",
	"Angelo (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_angeloRomInfo, MSX_angeloRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Angelo (Jpn, Alt)

static struct BurnRomInfo MSX_angeloaRomDesc[] = {
	{ "angelo (japan) (alt 1).rom",	0x04000, 0x3d8d0f4b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_angeloa, MSX_angeloa, msx_msx)
STD_ROM_FN(MSX_angeloa)

struct BurnDriver BurnDrvMSX_angeloa = {
	"msx_angeloa", "msx_angelo", "msx_msx", NULL, "1984",
	"Angelo (Jpn, Alt)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_angeloaRomInfo, MSX_angeloaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Antarctic Adventure (Euro)

static struct BurnRomInfo MSX_antarctRomDesc[] = {
	{ "antarctic adventure (europe).rom",	0x04000, 0x6b739c93, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_antarct, MSX_antarct, msx_msx)
STD_ROM_FN(MSX_antarct)

struct BurnDriver BurnDrvMSX_antarct = {
	"msx_antarct", NULL, "msx_msx", NULL, "1984",
	"Antarctic Adventure (Euro)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_antarctRomInfo, MSX_antarctRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Antarctic Adventure (Euro, Alt)

static struct BurnRomInfo MSX_antarctaRomDesc[] = {
	{ "antarctic adventure (europe) (alt 1).rom",	0x04000, 0xe8bddedd, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_antarcta, MSX_antarcta, msx_msx)
STD_ROM_FN(MSX_antarcta)

struct BurnDriver BurnDrvMSX_antarcta = {
	"msx_antarcta", "msx_antarct", "msx_msx", NULL, "1984",
	"Antarctic Adventure (Euro, Alt)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_antarctaRomInfo, MSX_antarctaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Antarctic Adventure (Jpn)

static struct BurnRomInfo MSX_antarctjRomDesc[] = {
	{ "antarctic adventure (japan).rom",	0x04000, 0xaae7028b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_antarctj, MSX_antarctj, msx_msx)
STD_ROM_FN(MSX_antarctj)

struct BurnDriver BurnDrvMSX_antarctj = {
	"msx_antarctj", "msx_antarct", "msx_msx", NULL, "1984",
	"Antarctic Adventure (Jpn)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_antarctjRomInfo, MSX_antarctjRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Antarctic Adventure (Kor)

static struct BurnRomInfo MSX_antarctkRomDesc[] = {
	{ "antarctic adventure - japanese version (1984)(konami)[cr prosoft][rc-701].rom",	0x04001, 0x8ced0683, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_antarctk, MSX_antarctk, msx_msx)
STD_ROM_FN(MSX_antarctk)

struct BurnDriver BurnDrvMSX_antarctk = {
	"msx_antarctk", "msx_antarct", "msx_msx", NULL, "19??",
	"Antarctic Adventure (Kor)\0", NULL, "Prosoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_antarctkRomInfo, MSX_antarctkRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Anty (Jpn)

static struct BurnRomInfo MSX_antyRomDesc[] = {
	{ "anty (japan).rom",	0x04000, 0xb1ace0a0, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_anty, MSX_anty, msx_msx)
STD_ROM_FN(MSX_anty)

struct BurnDriver BurnDrvMSX_anty = {
	"msx_anty", NULL, "msx_msx", NULL, "1984",
	"Anty (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_antyRomInfo, MSX_antyRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Anty (Jpn, Alt)

static struct BurnRomInfo MSX_antyaRomDesc[] = {
	{ "anty (japan) (alt 1).rom",	0x04000, 0x5baacb31, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_antya, MSX_antya, msx_msx)
STD_ROM_FN(MSX_antya)

struct BurnDriver BurnDrvMSX_antya = {
	"msx_antya", "msx_anty", "msx_msx", NULL, "1984",
	"Anty (Jpn, Alt)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_antyaRomInfo, MSX_antyaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Aqua Polis SOS (Jpn)

static struct BurnRomInfo MSX_aquasosRomDesc[] = {
	{ "aqua polis sos (japan).rom",	0x04000, 0xf115257a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_aquasos, MSX_aquasos, msx_msx)
STD_ROM_FN(MSX_aquasos)

struct BurnDriver BurnDrvMSX_aquasos = {
	"msx_aquasos", NULL, "msx_msx", NULL, "1983",
	"Aqua Polis SOS (Jpn)\0", NULL, "Paxon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_aquasosRomInfo, MSX_aquasosRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Aquattack (Jpn)

static struct BurnRomInfo MSX_aquatackRomDesc[] = {
	{ "aquattack (japan).rom",	0x04000, 0xf3297895, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_aquatack, MSX_aquatack, msx_msx)
STD_ROM_FN(MSX_aquatack)

struct BurnDriver BurnDrvMSX_aquatack = {
	"msx_aquatack", NULL, "msx_msx", NULL, "1984",
	"Aquattack (Jpn)\0", NULL, "Toshiba EMI", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_aquatackRomInfo, MSX_aquatackRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Aramo (Jpn)

static struct BurnRomInfo MSX_aramoRomDesc[] = {
	{ "aramo (japan).rom",	0x08000, 0xab6b56ba, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_aramo, MSX_aramo, msx_msx)
STD_ROM_FN(MSX_aramo)

struct BurnDriver BurnDrvMSX_aramo = {
	"msx_aramo", NULL, "msx_msx", NULL, "1986",
	"Aramo (Jpn)\0", NULL, "Sein Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_aramoRomInfo, MSX_aramoRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Arkanoid (Euro?)

static struct BurnRomInfo MSX_arkanoidRomDesc[] = {
	{ "arkanoid (japan) (alt 1).rom",	0x08000, 0x4fd4f778, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_arkanoid, MSX_arkanoid, msx_msx)
STD_ROM_FN(MSX_arkanoid)

struct BurnDriver BurnDrvMSX_arkanoid = {
	"msx_arkanoid", NULL, "msx_msx", NULL, "1986",
	"Arkanoid (Euro?)\0", NULL, "Taito ~ Imagine", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_arkanoidRomInfo, MSX_arkanoidRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Arkanoid (Euro?, Alt)

static struct BurnRomInfo MSX_arkanoidaRomDesc[] = {
	{ "arkanoid (japan) (alt 3).rom",	0x08000, 0x140a5965, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_arkanoida, MSX_arkanoida, msx_msx)
STD_ROM_FN(MSX_arkanoida)

struct BurnDriver BurnDrvMSX_arkanoida = {
	"msx_arkanoida", "msx_arkanoid", "msx_msx", NULL, "1986",
	"Arkanoid (Euro?, Alt)\0", NULL, "Taito ~ Imagine", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_arkanoidaRomInfo, MSX_arkanoidaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Arkanoid (Jpn)

static struct BurnRomInfo MSX_arkanoidjRomDesc[] = {
	{ "arkanoid (japan).rom",	0x08000, 0xc9a22dfc, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_arkanoidj, MSX_arkanoidj, msx_msx)
STD_ROM_FN(MSX_arkanoidj)

struct BurnDriver BurnDrvMSX_arkanoidj = {
	"msx_arkanoidj", "msx_arkanoid", "msx_msx", NULL, "1986",
	"Arkanoid (Jpn)\0", NULL, "Taito", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_arkanoidjRomInfo, MSX_arkanoidjRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Arkanoid (Jpn, Alt)

static struct BurnRomInfo MSX_arkanoidjaRomDesc[] = {
	{ "arkanoid (japan) (alt 2).rom",	0x08000, 0x38a00d1b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_arkanoidja, MSX_arkanoidja, msx_msx)
STD_ROM_FN(MSX_arkanoidja)

struct BurnDriver BurnDrvMSX_arkanoidja = {
	"msx_arkanoidja", "msx_arkanoid", "msx_msx", NULL, "1986",
	"Arkanoid (Jpn, Alt)\0", NULL, "Taito", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_arkanoidjaRomInfo, MSX_arkanoidjaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Arkanoid (Kor)

static struct BurnRomInfo MSX_arkanoidkRomDesc[] = {
	{ "arkanoid.rom",	0x08000, 0xb62a0973, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_arkanoidk, MSX_arkanoidk, msx_msx)
STD_ROM_FN(MSX_arkanoidk)

struct BurnDriver BurnDrvMSX_arkanoidk = {
	"msx_arkanoidk", "msx_arkanoid", "msx_msx", NULL, "198?",
	"Arkanoid (Kor)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_arkanoidkRomInfo, MSX_arkanoidkRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Astro Marine Corps (Spa)

static struct BurnRomInfo MSX_amcRomDesc[] = {
	{ "astro marine corps (1989)(dinamic software)(es).rom",	0x20000, 0x60f99d30, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_amc, MSX_amc, msx_msx)
STD_ROM_FN(MSX_amc)

struct BurnDriver BurnDrvMSX_amc = {
	"msx_amc", NULL, "msx_msx", NULL, "1989",
	"Astro Marine Corps (Spa)\0", "Uses joyport #2. Press 'M' for Grenade.", "Dinamic Software?", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII16, GBF_MISC, 0,
	MSXGetZipName, MSX_amcRomInfo, MSX_amcRomName, NULL, NULL, MSXInputInfo, MSXJoyport2DIPInfo,
	SwapButton2DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Athletic Ball (Jpn)

static struct BurnRomInfo MSX_athlballRomDesc[] = {
	{ "athletic ball (japan).rom",	0x04000, 0x11bac0e6, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_athlball, MSX_athlball, msx_msx)
STD_ROM_FN(MSX_athlball)

struct BurnDriver BurnDrvMSX_athlball = {
	"msx_athlball", NULL, "msx_msx", NULL, "1984",
	"Athletic Ball (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_athlballRomInfo, MSX_athlballRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Athletic Land (Jpn)

static struct BurnRomInfo MSX_athlandRomDesc[] = {
	{ "athletic land (japan).rom",	0x04000, 0x7b1acdea, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_athland, MSX_athland, msx_msx)
STD_ROM_FN(MSX_athland)

struct BurnDriver BurnDrvMSX_athland = {
	"msx_athland", NULL, "msx_msx", NULL, "1984",
	"Athletic Land (Jpn)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_athlandRomInfo, MSX_athlandRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Athletic Land (Jpn, Alt)

static struct BurnRomInfo MSX_athlandaRomDesc[] = {
	{ "athletic land (japan) (alt 1).rom",	0x04000, 0xd14361b2, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_athlanda, MSX_athlanda, msx_msx)
STD_ROM_FN(MSX_athlanda)

struct BurnDriver BurnDrvMSX_athlanda = {
	"msx_athlanda", "msx_athland", "msx_msx", NULL, "1984",
	"Athletic Land (Jpn, Alt)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_athlandaRomInfo, MSX_athlandaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Athletic Land (Jpn, Prototype)

static struct BurnRomInfo MSX_athlandpRomDesc[] = {
	{ "athletic land (japan) (beta).rom",	0x04000, 0xe4a428b5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_athlandp, MSX_athlandp, msx_msx)
STD_ROM_FN(MSX_athlandp)

struct BurnDriver BurnDrvMSX_athlandp = {
	"msx_athlandp", "msx_athland", "msx_msx", NULL, "1984",
	"Athletic Land (Jpn, Prototype)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_athlandpRomInfo, MSX_athlandpRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// B.C.'s Quest (Jpn)

static struct BurnRomInfo MSX_bcquestRomDesc[] = {
	{ "b.c.'s quest (japan).rom",	0x08000, 0x9c9420d0, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_bcquest, MSX_bcquest, msx_msx)
STD_ROM_FN(MSX_bcquest)

struct BurnDriver BurnDrvMSX_bcquest = {
	"msx_bcquest", NULL, "msx_msx", NULL, "1985",
	"B.C.'s Quest for Tires (Jpn)\0", NULL, "Toshiba EMI", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_bcquestRomInfo, MSX_bcquestRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// B.C.'s Quest (Jpn, Alt)

static struct BurnRomInfo MSX_bcquestaRomDesc[] = {
	{ "b.c.'s quest (japan) (alt 1).rom",	0x08000, 0xe2400f55, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_bcquesta, MSX_bcquesta, msx_msx)
STD_ROM_FN(MSX_bcquesta)

struct BurnDriver BurnDrvMSX_bcquesta = {
	"msx_bcquesta", "msx_bcquest", "msx_msx", NULL, "1985",
	"B.C.'s Quest for Tires (Jpn, Alt)\0", NULL, "Toshiba EMI", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_bcquestaRomInfo, MSX_bcquestaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Back to the Future (Jpn)

static struct BurnRomInfo MSX_backtofRomDesc[] = {
	{ "back to the future (japan).rom",	0x08000, 0xdbdb64ac, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_backtof, MSX_backtof, msx_msx)
STD_ROM_FN(MSX_backtof)

struct BurnDriver BurnDrvMSX_backtof = {
	"msx_backtof", NULL, "msx_msx", NULL, "1986",
	"Back to the Future (Jpn)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_backtofRomInfo, MSX_backtofRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Back to the Future (Jpn, Alt)

static struct BurnRomInfo MSX_backtofaRomDesc[] = {
	{ "back to the future (japan) (alt 1).rom",	0x08000, 0x795c7aa1, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_backtofa, MSX_backtofa, msx_msx)
STD_ROM_FN(MSX_backtofa)

struct BurnDriver BurnDrvMSX_backtofa = {
	"msx_backtofa", "msx_backtof", "msx_msx", NULL, "1986",
	"Back to the Future (Jpn, Alt)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_backtofaRomInfo, MSX_backtofaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Back Gammon (Jpn)

static struct BurnRomInfo MSX_backgamjRomDesc[] = {
	{ "back gammon (japan).rom",	0x04000, 0xb632b6bb, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_backgamj, MSX_backgamj, msx_msx)
STD_ROM_FN(MSX_backgamj)

struct BurnDriver BurnDrvMSX_backgamj = {
	"msx_backgamj", NULL, "msx_msx", NULL, "1984",
	"Back Gammon (Jpn)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_backgamjRomInfo, MSX_backgamjRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Back Gammon (Jpn, Alt)

static struct BurnRomInfo MSX_backgamjaRomDesc[] = {
	{ "back gammon (japan) (alt 1).rom",	0x04000, 0x70c79a87, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_backgamja, MSX_backgamja, msx_msx)
STD_ROM_FN(MSX_backgamja)

struct BurnDriver BurnDrvMSX_backgamja = {
	"msx_backgamja", "msx_backgamj", "msx_msx", NULL, "1984",
	"Back Gammon (Jpn, Alt)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_backgamjaRomInfo, MSX_backgamjaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Balance (Jpn)

static struct BurnRomInfo MSX_balanceRomDesc[] = {
	{ "balance (japan).rom",	0x02000, 0xcdabd75b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_balance, MSX_balance, msx_msx)
STD_ROM_FN(MSX_balance)

struct BurnDriver BurnDrvMSX_balance = {
	"msx_balance", NULL, "msx_msx", NULL, "1985",
	"Balance (Jpn)\0", NULL, "HAL Kenkyuujo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_balanceRomInfo, MSX_balanceRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Balance (Jpn, Alt)

static struct BurnRomInfo MSX_balance1RomDesc[] = {
	{ "balance (japan) (alt 1).rom",	0x02000, 0x336fdcd9, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_balance1, MSX_balance1, msx_msx)
STD_ROM_FN(MSX_balance1)

struct BurnDriver BurnDrvMSX_balance1 = {
	"msx_balance1", "msx_balance", "msx_msx", NULL, "1985",
	"Balance (Jpn, Alt)\0", NULL, "HAL Kenkyuujo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_balance1RomInfo, MSX_balance1RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Balance (Jpn, Alt 2)

static struct BurnRomInfo MSX_balance2RomDesc[] = {
	{ "balance (japan) (alt 2).rom",	0x04000, 0x86ea609d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_balance2, MSX_balance2, msx_msx)
STD_ROM_FN(MSX_balance2)

struct BurnDriver BurnDrvMSX_balance2 = {
	"msx_balance2", "msx_balance", "msx_msx", NULL, "1985",
	"Balance (Jpn, Alt 2)\0", NULL, "HAL Kenkyuujo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_balance2RomInfo, MSX_balance2RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Banana (Jpn)

static struct BurnRomInfo MSX_bananaRomDesc[] = {
	{ "banana (japan).rom",	0x04000, 0x6aa7cbe0, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_banana, MSX_banana, msx_msx)
STD_ROM_FN(MSX_banana)

struct BurnDriver BurnDrvMSX_banana = {
	"msx_banana", NULL, "msx_msx", NULL, "19??",
	"Banana (Jpn)\0", NULL, "Studio GEN", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_bananaRomInfo, MSX_bananaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Bank Panic (Jpn)

static struct BurnRomInfo MSX_bankpRomDesc[] = {
	{ "bank panic (japan).rom",	0x08000, 0xd5e18df0, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_bankp, MSX_bankp, msx_msx)
STD_ROM_FN(MSX_bankp)

struct BurnDriver BurnDrvMSX_bankp = {
	"msx_bankp", NULL, "msx_msx", NULL, "1986",
	"Bank Panic (Jpn)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_bankpRomInfo, MSX_bankpRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Bank Panic (Jpn, Alt)

static struct BurnRomInfo MSX_bankpaRomDesc[] = {
	{ "bank panic (japan) (alt 1).rom",	0x08000, 0xa0a844ca, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_bankpa, MSX_bankpa, msx_msx)
STD_ROM_FN(MSX_bankpa)

struct BurnDriver BurnDrvMSX_bankpa = {
	"msx_bankpa", "msx_bankp", "msx_msx", NULL, "1986",
	"Bank Panic (Jpn, Alt)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_bankpaRomInfo, MSX_bankpaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Bank Panic (Kor)

static struct BurnRomInfo MSX_bankpkRomDesc[] = {
	{ "bank panic (1985)(pony canyon)[cr prosoft].rom",	0x08000, 0x70160fe9, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_bankpk, MSX_bankpk, msx_msx)
STD_ROM_FN(MSX_bankpk)

struct BurnDriver BurnDrvMSX_bankpk = {
	"msx_bankpk", "msx_bankp", "msx_msx", NULL, "19??",
	"Bank Panic (Kor)\0", NULL, "Prosoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_bankpkRomInfo, MSX_bankpkRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Batman (Jpn)

static struct BurnRomInfo MSX_batmanRomDesc[] = {
	{ "batman (japan).rom",	0x10000, 0x637f0494, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_batman, MSX_batman, msx_msx)
STD_ROM_FN(MSX_batman)

struct BurnDriver BurnDrvMSX_batman = {
	"msx_batman", NULL, "msx_msx", NULL, "1987",
	"Batman (Jpn)\0", NULL, "Pack-In-Video", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII8, GBF_MISC, 0,
	MSXGetZipName, MSX_batmanRomInfo, MSX_batmanRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Batten Tanuki no Daibouken (Jpn, v1.03)

static struct BurnRomInfo MSX_btanukiRomDesc[] = {
	{ "batten tanuki no daibouken (japan) (v1.03).rom",	0x08000, 0xeba0738d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_btanuki, MSX_btanuki, msx_msx)
STD_ROM_FN(MSX_btanuki)

struct BurnDriver BurnDrvMSX_btanuki = {
	"msx_btanuki", NULL, "msx_msx", NULL, "1986",
	"Batten Tanuki no Daibouken (Jpn, v1.03)\0", NULL, "Tecno Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_btanukiRomInfo, MSX_btanukiRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Batten Tanuki no Daibouken (Jpn)

static struct BurnRomInfo MSX_btanukiaRomDesc[] = {
	{ "batten tanuki no daibouken (japan) (alt 1).rom",	0x08000, 0xe403ebea, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_btanukia, MSX_btanukia, msx_msx)
STD_ROM_FN(MSX_btanukia)

struct BurnDriver BurnDrvMSX_btanukia = {
	"msx_btanukia", "msx_btanuki", "msx_msx", NULL, "1986",
	"Batten Tanuki no Daibouken (Jpn)\0", NULL, "Tecno Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_btanukiaRomInfo, MSX_btanukiaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Batten Tanuki no Daibouken (Jpn, Alt)

static struct BurnRomInfo MSX_btanukibRomDesc[] = {
	{ "batten tanuki no daibouken (japan) (alt 2).rom",	0x08000, 0x42262bd4, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_btanukib, MSX_btanukib, msx_msx)
STD_ROM_FN(MSX_btanukib)

struct BurnDriver BurnDrvMSX_btanukib = {
	"msx_btanukib", "msx_btanuki", "msx_msx", NULL, "1986",
	"Batten Tanuki no Daibouken (Jpn, Alt)\0", NULL, "Tecno Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_btanukibRomInfo, MSX_btanukibRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Battle Cross (Jpn)

static struct BurnRomInfo MSX_battlexRomDesc[] = {
	{ "battle cross (japan).rom",	0x04000, 0x25e675ea, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_battlex, MSX_battlex, msx_msx)
STD_ROM_FN(MSX_battlex)

struct BurnDriver BurnDrvMSX_battlex = {
	"msx_battlex", NULL, "msx_msx", NULL, "1984",
	"Battle Cross (Jpn)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_battlexRomInfo, MSX_battlexRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Battleship Clapton II (Jpn)

static struct BurnRomInfo MSX_clapton2RomDesc[] = {
	{ "battleship clapton ii (japan).rom",	0x02000, 0x85f4767b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_clapton2, MSX_clapton2, msx_msx)
STD_ROM_FN(MSX_clapton2)

struct BurnDriver BurnDrvMSX_clapton2 = {
	"msx_clapton2", NULL, "msx_msx", NULL, "1984",
	"Battleship Clapton II (Jpn)\0", NULL, "T&E Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_clapton2RomInfo, MSX_clapton2RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Battleship Clapton II (Jpn, Alt)

static struct BurnRomInfo MSX_clapton2aRomDesc[] = {
	{ "battleship clapton ii (japan) (alt 1).rom",	0x04000, 0x2de725a3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_clapton2a, MSX_clapton2a, msx_msx)
STD_ROM_FN(MSX_clapton2a)

struct BurnDriver BurnDrvMSX_clapton2a = {
	"msx_clapton2a", "msx_clapton2", "msx_msx", NULL, "1984",
	"Battleship Clapton II (Jpn, Alt)\0", NULL, "T&E Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_clapton2aRomInfo, MSX_clapton2aRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Beach-Head (Euro)

static struct BurnRomInfo MSX_beacheadRomDesc[] = {
	{ "beach-head (europe).rom",	0x08000, 0x785fa4cc, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_beachead, MSX_beachead, msx_msx)
STD_ROM_FN(MSX_beachead)

struct BurnDriver BurnDrvMSX_beachead = {
	"msx_beachead", NULL, "msx_msx", NULL, "19??",
	"Beach-Head (Euro)\0", NULL, "Access Software?", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_beacheadRomInfo, MSX_beacheadRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Beam Rider (Jpn)

static struct BurnRomInfo MSX_beamridrRomDesc[] = {
	{ "beam rider (japan) (alt 1).rom",	0x04000, 0xd6a6bee6, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_beamridr, MSX_beamridr, msx_msx)
STD_ROM_FN(MSX_beamridr)

struct BurnDriver BurnDrvMSX_beamridr = {
	"msx_beamridr", NULL, "msx_msx", NULL, "1984",
	"Beam Rider (Jpn)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_beamridrRomInfo, MSX_beamridrRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Becky (Jpn)

static struct BurnRomInfo MSX_beckyRomDesc[] = {
	{ "becky (japan).rom",	0x04000, 0xa580b72a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_becky, MSX_becky, msx_msx)
STD_ROM_FN(MSX_becky)

struct BurnDriver BurnDrvMSX_becky = {
	"msx_becky", NULL, "msx_msx", NULL, "1983",
	"Becky (Jpn)\0", NULL, "MIA", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_beckyRomInfo, MSX_beckyRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Becky (Jpn, Alt)

static struct BurnRomInfo MSX_beckyaRomDesc[] = {
	{ "becky (japan) (alt 1).rom",	0x04000, 0xd062ad02, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_beckya, MSX_beckya, msx_msx)
STD_ROM_FN(MSX_beckya)

struct BurnDriver BurnDrvMSX_beckya = {
	"msx_beckya", "msx_becky", "msx_msx", NULL, "1983",
	"Becky (Jpn, Alt)\0", NULL, "MIA", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_beckyaRomInfo, MSX_beckyaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Becky (Jpn, Alt 2)

static struct BurnRomInfo MSX_beckybRomDesc[] = {
	{ "becky (japan) (alt 2).rom",	0x04000, 0x14156258, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_beckyb, MSX_beckyb, msx_msx)
STD_ROM_FN(MSX_beckyb)

struct BurnDriver BurnDrvMSX_beckyb = {
	"msx_beckyb", "msx_becky", "msx_msx", NULL, "1983",
	"Becky (Jpn, Alt 2)\0", NULL, "MIA", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_beckybRomInfo, MSX_beckybRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Bee & Flower (Jpn)

static struct BurnRomInfo MSX_beeflowrRomDesc[] = {
	{ "bee & flower (japan).rom",	0x04000, 0x0ad3707d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_beeflowr, MSX_beeflowr, msx_msx)
STD_ROM_FN(MSX_beeflowr)

struct BurnDriver BurnDrvMSX_beeflowr = {
	"msx_beeflowr", NULL, "msx_msx", NULL, "1983",
	"Bee & Flower (Jpn)\0", NULL, "Reizon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_beeflowrRomInfo, MSX_beeflowrRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Bee & Flower (Jpn, Alt)

static struct BurnRomInfo MSX_beeflowraRomDesc[] = {
	{ "bee & flower (japan) (alt 1).rom",	0x04000, 0x36fc4792, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_beeflowra, MSX_beeflowra, msx_msx)
STD_ROM_FN(MSX_beeflowra)

struct BurnDriver BurnDrvMSX_beeflowra = {
	"msx_beeflowra", "msx_beeflowr", "msx_msx", NULL, "1983",
	"Bee & Flower (Jpn, Alt)\0", NULL, "Reizon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_beeflowraRomInfo, MSX_beeflowraRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Bee & Flower (Jpn, Alt 2)

static struct BurnRomInfo MSX_beeflowrbRomDesc[] = {
	{ "bee & flower (japan) (alt 2).rom",	0x04000, 0xcf2335c7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_beeflowrb, MSX_beeflowrb, msx_msx)
STD_ROM_FN(MSX_beeflowrb)

struct BurnDriver BurnDrvMSX_beeflowrb = {
	"msx_beeflowrb", "msx_beeflowr", "msx_msx", NULL, "1983",
	"Bee & Flower (Jpn, Alt 2)\0", NULL, "Reizon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_beeflowrbRomInfo, MSX_beeflowrbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// The Black Onyx (Jpn)

static struct BurnRomInfo MSX_blckonyxRomDesc[] = {
	{ "black onyx, the (japan).rom",	0x08000, 0xe258c032, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_blckonyx, MSX_blckonyx, msx_msx)
STD_ROM_FN(MSX_blckonyx)

struct BurnDriver BurnDrvMSX_blckonyx = {
	"msx_blckonyx", NULL, "msx_msx", NULL, "1985",
	"The Black Onyx (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_blckonyxRomInfo, MSX_blckonyxRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Blagger MSX (Jpn)

static struct BurnRomInfo MSX_blaggerRomDesc[] = {
	{ "blagger.rom",	0x08000, 0x50475a7c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_blagger, MSX_blagger, msx_msx)
STD_ROM_FN(MSX_blagger)

struct BurnDriver BurnDrvMSX_blagger = {
	"msx_blagger", NULL, "msx_msx", NULL, "1984",
	"Blagger MSX (Jpn)\0", NULL, "MicroCabin", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_blaggerRomInfo, MSX_blaggerRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Blagger Enhanced

static struct BurnRomInfo MSX_blaggerenhRomDesc[] = {
	{ "blaggere.rom",	0x04000, 0xbdbdf44a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_blaggerenh, MSX_blaggerenh, msx_msx)
STD_ROM_FN(MSX_blaggerenh)

struct BurnDriver BurnDrvMSX_blaggerenh = {
	"msx_blaggerenh", NULL, "msx_msx", NULL, "2013",
	"Blagger Enhanced\0", NULL, "MicroCabin / JLTURSAN", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_blaggerenhRomInfo, MSX_blaggerenhRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Block Hole (Kor)

static struct BurnRomInfo MSX_blockholRomDesc[] = {
	{ "block hole (korea) (unl).rom",	0x08000, 0x824ae486, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_blockhol, MSX_blockhol, msx_msx)
STD_ROM_FN(MSX_blockhol)

struct BurnDriver BurnDrvMSX_blockhol = {
	"msx_blockhol", NULL, "msx_msx", NULL, "1990",
	"Block Hole (Kor)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_blockholRomInfo, MSX_blockholRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Blockade Runner (Jpn)

static struct BurnRomInfo MSX_blockrunRomDesc[] = {
	{ "blockade runner (japan).rom",	0x04000, 0x8334b431, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_blockrun, MSX_blockrun, msx_msx)
STD_ROM_FN(MSX_blockrun)

struct BurnDriver BurnDrvMSX_blockrun = {
	"msx_blockrun", NULL, "msx_msx", NULL, "1985",
	"Blockade Runner (Jpn)\0", NULL, "Toshiba EMI", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_blockrunRomInfo, MSX_blockrunRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Boggy '84 (Jpn)

static struct BurnRomInfo MSX_boggy84RomDesc[] = {
	{ "boggy '84 (japan).rom",	0x02000, 0xec089246, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_boggy84, MSX_boggy84, msx_msx)
STD_ROM_FN(MSX_boggy84)

struct BurnDriver BurnDrvMSX_boggy84 = {
	"msx_boggy84", NULL, "msx_msx", NULL, "1984",
	"Boggy '84 (Jpn)\0", NULL, "Nihon Columbia", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_boggy84RomInfo, MSX_boggy84RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Boggy '84 (Jpn, Alt)

static struct BurnRomInfo MSX_boggy84aRomDesc[] = {
	{ "boggy '84 (japan) (alt 1).rom",	0x04000, 0x22e8b312, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_boggy84a, MSX_boggy84a, msx_msx)
STD_ROM_FN(MSX_boggy84a)

struct BurnDriver BurnDrvMSX_boggy84a = {
	"msx_boggy84a", "msx_boggy84", "msx_msx", NULL, "1984",
	"Boggy '84 (Jpn, Alt)\0", NULL, "Nihon Columbia", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_boggy84aRomInfo, MSX_boggy84aRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Boggy '84 (Jpn, Alt 2)

static struct BurnRomInfo MSX_boggy84bRomDesc[] = {
	{ "boggy '84 (japan) (alt 2).rom",	0x02000, 0xa4bf1253, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_boggy84b, MSX_boggy84b, msx_msx)
STD_ROM_FN(MSX_boggy84b)

struct BurnDriver BurnDrvMSX_boggy84b = {
	"msx_boggy84b", "msx_boggy84", "msx_msx", NULL, "1984",
	"Boggy '84 (Jpn, Alt 2)\0", NULL, "Nihon Columbia", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_boggy84bRomInfo, MSX_boggy84bRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Boing Boing (Spa)

static struct BurnRomInfo MSX_boing2RomDesc[] = {
	{ "boing boing (spain).rom",	0x02000, 0xf44e7fcd, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_boing2, MSX_boing2, msx_msx)
STD_ROM_FN(MSX_boing2)

struct BurnDriver BurnDrvMSX_boing2 = {
	"msx_boing2", NULL, "msx_msx", NULL, "1985",
	"Boing Boing (Spa)\0", NULL, "Sony Spain", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_boing2RomInfo, MSX_boing2RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Bokosuka Wars (Jpn)

static struct BurnRomInfo MSX_bokosukaRomDesc[] = {
	{ "bokosuka wars (japan).rom",	0x04000, 0xdd6d9aad, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_bokosuka, MSX_bokosuka, msx_msx)
STD_ROM_FN(MSX_bokosuka)

struct BurnDriver BurnDrvMSX_bokosuka = {
	"msx_bokosuka", NULL, "msx_msx", NULL, "1984",
	"Bokosuka Wars (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_bokosukaRomInfo, MSX_bokosukaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Bokosuka Wars (Jpn, Alt)

static struct BurnRomInfo MSX_bokosukaaRomDesc[] = {
	{ "bokosuka wars (japan) (alt 1).rom",	0x04000, 0x93b8917a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_bokosukaa, MSX_bokosukaa, msx_msx)
STD_ROM_FN(MSX_bokosukaa)

struct BurnDriver BurnDrvMSX_bokosukaa = {
	"msx_bokosukaa", "msx_bokosuka", "msx_msx", NULL, "1984",
	"Bokosuka Wars (Jpn, Alt)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_bokosukaaRomInfo, MSX_bokosukaaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Bomber King (Jpn)

static struct BurnRomInfo MSX_bombkingRomDesc[] = {
	{ "bomber king (japan).rom",	0x20000, 0x8cf0e6c0, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_bombking, MSX_bombking, msx_msx)
STD_ROM_FN(MSX_bombking)

struct BurnDriver BurnDrvMSX_bombking = {
	"msx_bombking", NULL, "msx_msx", NULL, "1988",
	"Bomber King (Jpn)\0", NULL, "Hudson Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII8, GBF_MISC, 0,
	MSXGetZipName, MSX_bombkingRomInfo, MSX_bombkingRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Bomber Man (Jpn)

static struct BurnRomInfo MSX_bombmanRomDesc[] = {
	{ "bomber man (japan).rom",	0x02000, 0x9a5aef05, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_bombman, MSX_bombman, msx_msx)
STD_ROM_FN(MSX_bombman)

struct BurnDriver BurnDrvMSX_bombman = {
	"msx_bombman", NULL, "msx_msx", NULL, "1983",
	"Bomber Man (Jpn)\0", NULL, "Hudson Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_bombmanRomInfo, MSX_bombmanRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Bomber Man (Kor)

static struct BurnRomInfo MSX_bombmank1RomDesc[] = {
	{ "bomber man (1986)(korea soft bank).rom",	0x04000, 0x3df1f56f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_bombmank1, MSX_bombmank1, msx_msx)
STD_ROM_FN(MSX_bombmank1)

struct BurnDriver BurnDrvMSX_bombmank1 = {
	"msx_bombmank1", "msx_bombman", "msx_msx", NULL, "1986",
	"Bomber Man (Kor)\0", NULL, "Korea Soft Bank", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_bombmank1RomInfo, MSX_bombmank1RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Bomber Man (Kor, Alt)

static struct BurnRomInfo MSX_bombmank2RomDesc[] = {
	{ "bomber man (1986)(korea soft bank)[a].rom",	0x02000, 0xc9a2c37e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_bombmank2, MSX_bombmank2, msx_msx)
STD_ROM_FN(MSX_bombmank2)

struct BurnDriver BurnDrvMSX_bombmank2 = {
	"msx_bombmank2", "msx_bombman", "msx_msx", NULL, "1986",
	"Bomber Man (Kor, Alt)\0", NULL, "Korea Soft Bank", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_bombmank2RomInfo, MSX_bombmank2RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Bomber Man Special (Jpn)

static struct BurnRomInfo MSX_bombmnspRomDesc[] = {
	{ "bomber man special (japan).rom",	0x08000, 0x042ad44d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_bombmnsp, MSX_bombmnsp, msx_msx)
STD_ROM_FN(MSX_bombmnsp)

struct BurnDriver BurnDrvMSX_bombmnsp = {
	"msx_bombmnsp", NULL, "msx_msx", NULL, "1986",
	"Bomber Man Special (Jpn)\0", NULL, "Hudson Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_bombmnspRomInfo, MSX_bombmnspRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Boogie Woogi Jungle (Jpn)

static struct BurnRomInfo MSX_boogieRomDesc[] = {
	{ "boogie woogi jungle (japan).rom",	0x04000, 0x90ccea11, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_boogie, MSX_boogie, msx_msx)
STD_ROM_FN(MSX_boogie)

struct BurnDriver BurnDrvMSX_boogie = {
	"msx_boogie", NULL, "msx_msx", NULL, "1983",
	"Boogie Woogi Jungle (Jpn)\0", NULL, "Ample Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_boogieRomInfo, MSX_boogieRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Boogie Woogi Jungle (Jpn, Alt)

static struct BurnRomInfo MSX_boogie1RomDesc[] = {
	{ "boogie woogi jungle (japan) (alt 1).rom",	0x04000, 0x374b63c9, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_boogie1, MSX_boogie1, msx_msx)
STD_ROM_FN(MSX_boogie1)

struct BurnDriver BurnDrvMSX_boogie1 = {
	"msx_boogie1", "msx_boogie", "msx_msx", NULL, "1983",
	"Boogie Woogi Jungle (Jpn, Alt)\0", NULL, "Ample Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_boogie1RomInfo, MSX_boogie1RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Boogie Woogi Jungle (Jpn, Alt 2)

static struct BurnRomInfo MSX_boogie2RomDesc[] = {
	{ "boogie woogi jungle (japan) (alt 2).rom",	0x04001, 0x464e7594, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_boogie2, MSX_boogie2, msx_msx)
STD_ROM_FN(MSX_boogie2)

struct BurnDriver BurnDrvMSX_boogie2 = {
	"msx_boogie2", "msx_boogie", "msx_msx", NULL, "1983",
	"Boogie Woogi Jungle (Jpn, Alt 2)\0", NULL, "Ample Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_boogie2RomInfo, MSX_boogie2RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Boomerang (Jpn)

static struct BurnRomInfo MSX_boomrangRomDesc[] = {
	{ "boomerang (japan).rom",	0x04000, 0x0a140b27, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_boomrang, MSX_boomrang, msx_msx)
STD_ROM_FN(MSX_boomrang)

struct BurnDriver BurnDrvMSX_boomrang = {
	"msx_boomrang", NULL, "msx_msx", NULL, "1984",
	"Boomerang (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_boomrangRomInfo, MSX_boomrangRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Boomerang (Jpn, Alt)

static struct BurnRomInfo MSX_boomrangaRomDesc[] = {
	{ "boomerang (japan) (alt 1).rom",	0x04000, 0x5f1f9288, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_boomranga, MSX_boomranga, msx_msx)
STD_ROM_FN(MSX_boomranga)

struct BurnDriver BurnDrvMSX_boomranga = {
	"msx_boomranga", "msx_boomrang", "msx_msx", NULL, "1984",
	"Boomerang (Jpn, Alt)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_boomrangaRomInfo, MSX_boomrangaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Star Destroyer Bosconian (Jpn)

static struct BurnRomInfo MSX_boscoRomDesc[] = {
	{ "bosconian (japan).rom",	0x08000, 0x02f1997a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_bosco, MSX_bosco, msx_msx)
STD_ROM_FN(MSX_bosco)

struct BurnDriver BurnDrvMSX_bosco = {
	"msx_bosco", NULL, "msx_msx", NULL, "1984",
	"Star Destroyer Bosconian (Jpn)\0", NULL, "Namcot", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_boscoRomInfo, MSX_boscoRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Star Destroyer Bosconian (Jpn, Alt)

static struct BurnRomInfo MSX_boscoaRomDesc[] = {
	{ "bosconian (japan) (alt 1).rom",	0x08000, 0xaf3c5d48, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_boscoa, MSX_boscoa, msx_msx)
STD_ROM_FN(MSX_boscoa)

struct BurnDriver BurnDrvMSX_boscoa = {
	"msx_boscoa", "msx_bosco", "msx_msx", NULL, "1984",
	"Star Destroyer Bosconian (Jpn, Alt)\0", NULL, "Namcot", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_boscoaRomInfo, MSX_boscoaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Star Destroyer Bosconian (Jpn, Alt 2)

static struct BurnRomInfo MSX_boscobRomDesc[] = {
	{ "bosconian (japan) (alt 2).rom",	0x08000, 0x626132ae, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_boscob, MSX_boscob, msx_msx)
STD_ROM_FN(MSX_boscob)

struct BurnDriver BurnDrvMSX_boscob = {
	"msx_boscob", "msx_bosco", "msx_msx", NULL, "1984",
	"Star Destroyer Bosconian (Jpn, Alt 2)\0", NULL, "Namcot", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_boscobRomInfo, MSX_boscobRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Bouken Roman - Dota (Jpn)

static struct BurnRomInfo MSX_dotaRomDesc[] = {
	{ "bouken roman - dota (japan).rom",	0x08000, 0x57efca5a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_dota, MSX_dota, msx_msx)
STD_ROM_FN(MSX_dota)

struct BurnDriver BurnDrvMSX_dota = {
	"msx_dota", NULL, "msx_msx", NULL, "1986",
	"Bouken Roman - Dota (Jpn)\0", NULL, "System Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_dotaRomInfo, MSX_dotaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Bouken Roman - Dota (Jpn, Alt)

static struct BurnRomInfo MSX_dotaaRomDesc[] = {
	{ "bouken roman - dota (japan) (alt 1).rom",	0x08000, 0xc5ca7dff, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_dotaa, MSX_dotaa, msx_msx)
STD_ROM_FN(MSX_dotaa)

struct BurnDriver BurnDrvMSX_dotaa = {
	"msx_dotaa", "msx_dota", "msx_msx", NULL, "1986",
	"Bouken Roman - Dota (Jpn, Alt)\0", NULL, "System Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_dotaaRomInfo, MSX_dotaaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Boulder Dash (Jpn)

static struct BurnRomInfo MSX_bdashRomDesc[] = {
	{ "boulder dash (japan).rom",	0x08000, 0xa3a56524, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_bdash, MSX_bdash, msx_msx)
STD_ROM_FN(MSX_bdash)

struct BurnDriver BurnDrvMSX_bdash = {
	"msx_bdash", NULL, "msx_msx", NULL, "1985",
	"Boulder Dash (Jpn)\0", NULL, "Comptiq", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_bdashRomInfo, MSX_bdashRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Break In (Jpn)

static struct BurnRomInfo MSX_breakinjRomDesc[] = {
	{ "break in (japan).rom",	0x10000, 0x8801b31e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_breakinj, MSX_breakinj, msx_msx)
STD_ROM_FN(MSX_breakinj)

struct BurnDriver BurnDrvMSX_breakinj = {
	"msx_breakinj", NULL, "msx_msx", NULL, "1987",
	"Break In (Jpn)\0", NULL, "Jaleco", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_breakinjRomInfo, MSX_breakinjRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Break Out (Jpn)

static struct BurnRomInfo MSX_breakoutRomDesc[] = {
	{ "break out (japan).rom",	0x04000, 0x70113f76, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_breakout, MSX_breakout, msx_msx)
STD_ROM_FN(MSX_breakout)

struct BurnDriver BurnDrvMSX_breakout = {
	"msx_breakout", NULL, "msx_msx", NULL, "1983",
	"Break Out (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_BASIC, GBF_MISC, 0,
	MSXGetZipName, MSX_breakoutRomInfo, MSX_breakoutRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Brother Adventure - Hyeongje Moheom (Kor)

static struct BurnRomInfo MSX_brosadvRomDesc[] = {
	{ "brother adventure (korea) (unl).rom",	0x08000, 0x3ca757d0, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_brosadv, MSX_brosadv, msx_msx)
STD_ROM_FN(MSX_brosadv)

struct BurnDriver BurnDrvMSX_brosadv = {
	"msx_brosadv", NULL, "msx_msx", NULL, "1987",
	"Brother Adventure - Hyeongje Moheom (Kor)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_brosadvRomInfo, MSX_brosadvRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Brother Adventure - Hyeongje Moheom (Kor, Alt)

static struct BurnRomInfo MSX_brosadvaRomDesc[] = {
	{ "brother adventure (korea) (alt 1) (unl).rom",	0x08000, 0xfe0a902c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_brosadva, MSX_brosadva, msx_msx)
STD_ROM_FN(MSX_brosadva)

struct BurnDriver BurnDrvMSX_brosadva = {
	"msx_brosadva", "msx_brosadv", "msx_msx", NULL, "1987",
	"Brother Adventure - Hyeongje Moheom (Kor, Alt)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_brosadvaRomInfo, MSX_brosadvaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Bruce Lee (Jpn)

static struct BurnRomInfo MSX_bruceleeRomDesc[] = {
	{ "bruce lee (japan).rom",	0x08000, 0xa20b196d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_brucelee, MSX_brucelee, msx_msx)
STD_ROM_FN(MSX_brucelee)

struct BurnDriver BurnDrvMSX_brucelee = {
	"msx_brucelee", NULL, "msx_msx", NULL, "1985",
	"Bruce Lee (Jpn)\0", NULL, "Comptiq", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_bruceleeRomInfo, MSX_bruceleeRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Burgertime (Jpn)

static struct BurnRomInfo MSX_btimeRomDesc[] = {
	{ "burgertime (japan).rom",	0x08000, 0xe4a7c230, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_btime, MSX_btime, msx_msx)
STD_ROM_FN(MSX_btime)

struct BurnDriver BurnDrvMSX_btime = {
	"msx_btime", NULL, "msx_msx", NULL, "1986",
	"Burgertime (Jpn)\0", NULL, "Dempa", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_btimeRomInfo, MSX_btimeRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Bull to Mighty Kiki Ippatsu (Jpn)

static struct BurnRomInfo MSX_bullmigtRomDesc[] = {
	{ "buru to marty kikiippatsu - inspecteur z (japan).rom",	0x08000, 0x9cf39bd6, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_bullmigt, MSX_bullmigt, msx_msx)
STD_ROM_FN(MSX_bullmigt)

struct BurnDriver BurnDrvMSX_bullmigt = {
	"msx_bullmigt", NULL, "msx_msx", NULL, "1986",
	"Bull to Mighty Kiki Ippatsu (Jpn)\0", NULL, "HAL Kenkyuujo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_bullmigtRomInfo, MSX_bullmigtRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Butamaru Pants. Pig Mock (Jpn)

static struct BurnRomInfo MSX_butampanRomDesc[] = {
	{ "butam pants (japan).rom",	0x02000, 0x4b2aa972, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_butampan, MSX_butampan, msx_msx)
STD_ROM_FN(MSX_butampan)

struct BurnDriver BurnDrvMSX_butampan = {
	"msx_butampan", NULL, "msx_msx", NULL, "1983",
	"Butamaru Pants. Pig Mock (Jpn)\0", NULL, "HAL Kenkyuujo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_butampanRomInfo, MSX_butampanRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Butam Pants (Jpn, Alt)

static struct BurnRomInfo MSX_butampanaRomDesc[] = {
	{ "butam pants (japan) (alt 1).rom",	0x04000, 0x4474ca21, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_butampana, MSX_butampana, msx_msx)
STD_ROM_FN(MSX_butampana)

struct BurnDriver BurnDrvMSX_butampana = {
	"msx_butampana", "msx_butampan", "msx_msx", NULL, "1983",
	"Butam Pants (Jpn, Alt)\0", NULL, "HAL Kenkyuujo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_butampanaRomInfo, MSX_butampanaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Butam Pants (Jpn, Alt 2)

static struct BurnRomInfo MSX_butampanbRomDesc[] = {
	{ "butam pants (japan) (alt 2).rom",	0x04000, 0xa102f82d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_butampanb, MSX_butampanb, msx_msx)
STD_ROM_FN(MSX_butampanb)

struct BurnDriver BurnDrvMSX_butampanb = {
	"msx_butampanb", "msx_butampan", "msx_msx", NULL, "1983",
	"Butam Pants (Jpn, Alt 2)\0", NULL, "HAL Kenkyuujo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_butampanbRomInfo, MSX_butampanbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// C_So! (Jpn)

static struct BurnRomInfo MSX_csoRomDesc[] = {
	{ "c-so! (japan).rom",	0x04000, 0xdf8bfc89, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_cso, MSX_cso, msx_msx)
STD_ROM_FN(MSX_cso)

struct BurnDriver BurnDrvMSX_cso = {
	"msx_cso", NULL, "msx_msx", NULL, "1985",
	"C_So! (Jpn)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_csoRomInfo, MSX_csoRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// C_So! (Kor)

static struct BurnRomInfo MSX_csokRomDesc[] = {
	{ "c-so.rom",	0x08000, 0x4f195441, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_csok, MSX_csok, msx_msx)
STD_ROM_FN(MSX_csok)

struct BurnDriver BurnDrvMSX_csok = {
	"msx_csok", "msx_cso", "msx_msx", NULL, "198?",
	"C_So! (Kor)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_csokRomInfo, MSX_csokRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Cannon Turbo (Jpn)

static struct BurnRomInfo MSX_cannontRomDesc[] = {
	{ "cannon_turbo_(1987)_(brother).rom",	0x08000, 0x1ff280e3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_cannont, MSX_cannont, msx_msx)
STD_ROM_FN(MSX_cannont)

struct BurnDriver BurnDrvMSX_cannont = {
	"msx_cannont", NULL, "msx_msx", NULL, "198?",
	"Cannon Turbo (Jpn)\0", NULL, "Brother Kougyou", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_cannontRomInfo, MSX_cannontRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Cabbage Patch Kids (Jpn)

static struct BurnRomInfo MSX_cabbagepRomDesc[] = {
	{ "cabbage patch kids (japan) (alt 1).rom",	0x04000, 0x97a1b4b9, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_cabbagep, MSX_cabbagep, msx_msx)
STD_ROM_FN(MSX_cabbagep)

struct BurnDriver BurnDrvMSX_cabbagep = {
	"msx_cabbagep", NULL, "msx_msx", NULL, "1984",
	"Cabbage Patch Kids (Jpn)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_cabbagepRomInfo, MSX_cabbagepRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Cabbage Patch Kids (Jpn, Older?)

static struct BurnRomInfo MSX_cabbagepaRomDesc[] = {
	{ "cabbage patch kids (japan).rom",	0x04000, 0x057d2790, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_cabbagepa, MSX_cabbagepa, msx_msx)
STD_ROM_FN(MSX_cabbagepa)

struct BurnDriver BurnDrvMSX_cabbagepa = {
	"msx_cabbagepa", "msx_cabbagep", "msx_msx", NULL, "1983",
	"Cabbage Patch Kids (Jpn, Older?)\0", NULL, "Konami?", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_cabbagepaRomInfo, MSX_cabbagepaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Cabbage Patch Kids (Kor)

static struct BurnRomInfo MSX_cabbagepkRomDesc[] = {
	{ "cpkids.rom",	0x08000, 0x2ab67576, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_cabbagepk, MSX_cabbagepk, msx_msx)
STD_ROM_FN(MSX_cabbagepk)

struct BurnDriver BurnDrvMSX_cabbagepk = {
	"msx_cabbagepk", "msx_cabbagep", "msx_msx", NULL, "198?",
	"Cabbage Patch Kids (Kor)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_cabbagepkRomInfo, MSX_cabbagepkRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Candoo Ninja (Jpn)

static struct BurnRomInfo MSX_candoonRomDesc[] = {
	{ "candoo ninja (japan).rom",	0x04000, 0xb6ab6786, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_candoon, MSX_candoon, msx_msx)
STD_ROM_FN(MSX_candoon)

struct BurnDriver BurnDrvMSX_candoon = {
	"msx_candoon", NULL, "msx_msx", NULL, "1984",
	"Candoo Ninja (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_candoonRomInfo, MSX_candoonRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Candoo Ninja (Jpn, Alt)

static struct BurnRomInfo MSX_candoonaRomDesc[] = {
	{ "candoo ninja (japan) (alt 1).rom",	0x04000, 0x037ba8b7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_candoona, MSX_candoona, msx_msx)
STD_ROM_FN(MSX_candoona)

struct BurnDriver BurnDrvMSX_candoona = {
	"msx_candoona", "msx_candoon", "msx_msx", NULL, "1984",
	"Candoo Ninja (Jpn, Alt)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_candoonaRomInfo, MSX_candoonaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Cannon Ball (Jpn)

static struct BurnRomInfo MSX_cannonblRomDesc[] = {
	{ "cannon ball (japan).rom",	0x02000, 0xa667ad8a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_cannonbl, MSX_cannonbl, msx_msx)
STD_ROM_FN(MSX_cannonbl)

struct BurnDriver BurnDrvMSX_cannonbl = {
	"msx_cannonbl", NULL, "msx_msx", NULL, "1983",
	"Cannon Ball (Jpn)\0", NULL, "Hudson Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_cannonblRomInfo, MSX_cannonblRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Cannon Ball (Jpn, Alt)

static struct BurnRomInfo MSX_cannonblaRomDesc[] = {
	{ "cannon ball (japan) (alt 1).rom",	0x04000, 0xb64390e7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_cannonbla, MSX_cannonbla, msx_msx)
STD_ROM_FN(MSX_cannonbla)

struct BurnDriver BurnDrvMSX_cannonbla = {
	"msx_cannonbla", "msx_cannonbl", "msx_msx", NULL, "1983",
	"Cannon Ball (Jpn, Alt)\0", NULL, "Hudson Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_cannonblaRomInfo, MSX_cannonblaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Captain Chef (Jpn)

static struct BurnRomInfo MSX_captchefRomDesc[] = {
	{ "captain chef (japan).rom",	0x02000, 0x8e985857, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_captchef, MSX_captchef, msx_msx)
STD_ROM_FN(MSX_captchef)

struct BurnDriver BurnDrvMSX_captchef = {
	"msx_captchef", NULL, "msx_msx", NULL, "1984",
	"Captain Chef (Jpn)\0", NULL, "Nihon Columbia", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_captchefRomInfo, MSX_captchefRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Car Fighter (Jpn)

static struct BurnRomInfo MSX_carfightRomDesc[] = {
	{ "car fighter (japan).rom",	0x04000, 0x303187f5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_carfight, MSX_carfight, msx_msx)
STD_ROM_FN(MSX_carfight)

struct BurnDriver BurnDrvMSX_carfight = {
	"msx_carfight", NULL, "msx_msx", NULL, "1985",
	"Car Fighter (Jpn)\0", NULL, "Casio", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_carfightRomInfo, MSX_carfightRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Car Jamboree (Jpn)

static struct BurnRomInfo MSX_carjambRomDesc[] = {
	{ "car jamboree (japan).rom",	0x04000, 0x94fc9169, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_carjamb, MSX_carjamb, msx_msx)
STD_ROM_FN(MSX_carjamb)

struct BurnDriver BurnDrvMSX_carjamb = {
	"msx_carjamb", NULL, "msx_msx", NULL, "1984",
	"Car Jamboree (Jpn)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_carjambRomInfo, MSX_carjambRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Car Jamboree (Jpn, Alt)

static struct BurnRomInfo MSX_carjambaRomDesc[] = {
	{ "car jamboree (japan) (alt 1).rom",	0x08000, 0xa24195fa, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_carjamba, MSX_carjamba, msx_msx)
STD_ROM_FN(MSX_carjamba)

struct BurnDriver BurnDrvMSX_carjamba = {
	"msx_carjamba", "msx_carjamb", "msx_msx", NULL, "1984",
	"Car Jamboree (Jpn, Alt)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_carjambaRomInfo, MSX_carjambaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Car-Race (Jpn)

static struct BurnRomInfo MSX_carraceRomDesc[] = {
	{ "car-race (japan).rom",	0x02000, 0x3f0db564, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_carrace, MSX_carrace, msx_msx)
STD_ROM_FN(MSX_carrace)

struct BurnDriver BurnDrvMSX_carrace = {
	"msx_carrace", NULL, "msx_msx", NULL, "1983",
	"Car-Race (Jpn)\0", NULL, "Ample Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_carraceRomInfo, MSX_carraceRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Car-Race (Jpn, Alt)

static struct BurnRomInfo MSX_carraceaRomDesc[] = {
	{ "car-race (japan) (alt 1).rom",	0x04000, 0x9538d829, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_carracea, MSX_carracea, msx_msx)
STD_ROM_FN(MSX_carracea)

struct BurnDriver BurnDrvMSX_carracea = {
	"msx_carracea", "msx_carrace", "msx_msx", NULL, "1983",
	"Car-Race (Jpn, Alt)\0", NULL, "Ample Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_carraceaRomInfo, MSX_carraceaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Car-Race (Jpn, Alt 2)

static struct BurnRomInfo MSX_carracebRomDesc[] = {
	{ "car-race (japan) (alt 2).rom",	0x04001, 0x51445292, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_carraceb, MSX_carraceb, msx_msx)
STD_ROM_FN(MSX_carraceb)

struct BurnDriver BurnDrvMSX_carraceb = {
	"msx_carraceb", "msx_carrace", "msx_msx", NULL, "1983",
	"Car-Race (Jpn, Alt 2)\0", NULL, "Ample Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_carracebRomInfo, MSX_carracebRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Casio Worldopen (Jpn)

static struct BurnRomInfo MSX_wrldopenRomDesc[] = {
	{ "casio worldopen (japan).rom",	0x08000, 0x338491f5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_wrldopen, MSX_wrldopen, msx_msx)
STD_ROM_FN(MSX_wrldopen)

struct BurnDriver BurnDrvMSX_wrldopen = {
	"msx_wrldopen", NULL, "msx_msx", NULL, "1985",
	"Casio Worldopen (Jpn)\0", NULL, "Casio", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_wrldopenRomInfo, MSX_wrldopenRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Casio Worldopen (Kor)

static struct BurnRomInfo MSX_wrldopenkRomDesc[] = {
	{ "casio world open (1985)(casio)[cr boram soft].rom",	0x08000, 0x3508626f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_wrldopenk, MSX_wrldopenk, msx_msx)
STD_ROM_FN(MSX_wrldopenk)

struct BurnDriver BurnDrvMSX_wrldopenk = {
	"msx_wrldopenk", "msx_wrldopen", "msx_msx", NULL, "1986",
	"Casio Worldopen (Kor)\0", NULL, "Boram Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_wrldopenkRomInfo, MSX_wrldopenkRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// The Castle (Jpn)

static struct BurnRomInfo MSX_castleRomDesc[] = {
	{ "castle, the (japan).rom",	0x08000, 0x2149c32d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_castle, MSX_castle, msx_msx)
STD_ROM_FN(MSX_castle)

struct BurnDriver BurnDrvMSX_castle = {
	"msx_castle", NULL, "msx_msx", NULL, "1985",
	"The Castle (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_castleRomInfo, MSX_castleRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// The Castle (Kor)

static struct BurnRomInfo MSX_castlekRomDesc[] = {
	{ "castle, the (1986)(ascii)[cr static soft].rom",	0x08000, 0x48eccfaf, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_castlek, MSX_castlek, msx_msx)
STD_ROM_FN(MSX_castlek)

struct BurnDriver BurnDrvMSX_castlek = {
	"msx_castlek", "msx_castle", "msx_msx", NULL, "1987",
	"The Castle (Kor)\0", NULL, "Static Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_castlekRomInfo, MSX_castlekRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Castle Excellent (Jpn)

static struct BurnRomInfo MSX_castlexRomDesc[] = {
	{ "castle excellent (japan).rom",	0x08000, 0x90f38029, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_castlex, MSX_castlex, msx_msx)
STD_ROM_FN(MSX_castlex)

struct BurnDriver BurnDrvMSX_castlex = {
	"msx_castlex", NULL, "msx_msx", NULL, "1986",
	"Castle Excellent (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_castlexRomInfo, MSX_castlexRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Castle Excellent (Jpn, Alt)

static struct BurnRomInfo MSX_castlexaRomDesc[] = {
	{ "castle excellent (japan) (alt 1).rom",	0x08000, 0x3794a648, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_castlexa, MSX_castlexa, msx_msx)
STD_ROM_FN(MSX_castlexa)

struct BurnDriver BurnDrvMSX_castlexa = {
	"msx_castlexa", "msx_castlex", "msx_msx", NULL, "1986",
	"Castle Excellent (Jpn, Alt)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_castlexaRomInfo, MSX_castlexaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Chack'n Pop (Jpn)

static struct BurnRomInfo MSX_chacknRomDesc[] = {
	{ "chack'n pop (japan).rom",	0x04000, 0x04f7e0b1, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_chackn, MSX_chackn, msx_msx)
STD_ROM_FN(MSX_chackn)

struct BurnDriver BurnDrvMSX_chackn = {
	"msx_chackn", NULL, "msx_msx", NULL, "1984",
	"Chack'n Pop (Jpn)\0", NULL, "Taito", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_chacknRomInfo, MSX_chacknRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Chack'n Pop (Jpn, Alt)

static struct BurnRomInfo MSX_chacknaRomDesc[] = {
	{ "chack'n pop (japan) (alt 1).rom",	0x04000, 0x0fff4d26, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_chackna, MSX_chackna, msx_msx)
STD_ROM_FN(MSX_chackna)

struct BurnDriver BurnDrvMSX_chackna = {
	"msx_chackna", "msx_chackn", "msx_msx", NULL, "1984",
	"Chack'n Pop (Jpn, Alt)\0", NULL, "Taito", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_chacknaRomInfo, MSX_chacknaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Champion Boxing (Jpn)

static struct BurnRomInfo MSX_champboxRomDesc[] = {
	{ "champion boxing (japan).rom",	0x08000, 0xc4b7a5b9, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_champbox, MSX_champbox, msx_msx)
STD_ROM_FN(MSX_champbox)

struct BurnDriver BurnDrvMSX_champbox = {
	"msx_champbox", NULL, "msx_msx", NULL, "1985",
	"Champion Boxing (Jpn)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_champboxRomInfo, MSX_champboxRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Champion Ice Hockey (Jpn)

static struct BurnRomInfo MSX_champiceRomDesc[] = {
	{ "champion ice hockey (japan).rom",	0x08000, 0xbf4b018f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_champice, MSX_champice, msx_msx)
STD_ROM_FN(MSX_champice)

struct BurnDriver BurnDrvMSX_champice = {
	"msx_champice", NULL, "msx_msx", NULL, "1986",
	"Champion Ice Hockey (Jpn)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_champiceRomInfo, MSX_champiceRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Champion Kendou (Jpn)

static struct BurnRomInfo MSX_champkenRomDesc[] = {
	{ "champion kendou (japan).rom",	0x08000, 0x0103c767, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_champken, MSX_champken, msx_msx)
STD_ROM_FN(MSX_champken)

struct BurnDriver BurnDrvMSX_champken = {
	"msx_champken", NULL, "msx_msx", NULL, "1986",
	"Champion Kendou (Jpn)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_champkenRomInfo, MSX_champkenRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Champion Kendou (Jpn, Alt)

static struct BurnRomInfo MSX_champkenaRomDesc[] = {
	{ "champion kendou (japan) (alt 1).rom",	0x08000, 0x6f05e6bf, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_champkena, MSX_champkena, msx_msx)
STD_ROM_FN(MSX_champkena)

struct BurnDriver BurnDrvMSX_champkena = {
	"msx_champkena", "msx_champken", "msx_msx", NULL, "1986",
	"Champion Kendou (Jpn, Alt)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_champkenaRomInfo, MSX_champkenaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Champion Pro Wrestling (Jpn)

static struct BurnRomInfo MSX_champpwrRomDesc[] = {
	{ "champion pro wrestling (japan).rom",	0x08000, 0x6686c84e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_champpwr, MSX_champpwr, msx_msx)
STD_ROM_FN(MSX_champpwr)

struct BurnDriver BurnDrvMSX_champpwr = {
	"msx_champpwr", NULL, "msx_msx", NULL, "1985",
	"Champion Pro Wrestling (Jpn)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_champpwrRomInfo, MSX_champpwrRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Champion Pro Wrestling (Jpn, Alt)

static struct BurnRomInfo MSX_champpwraRomDesc[] = {
	{ "champion pro wrestling (japan) (alt 1).rom",	0x08000, 0x8d658f41, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_champpwra, MSX_champpwra, msx_msx)
STD_ROM_FN(MSX_champpwra)

struct BurnDriver BurnDrvMSX_champpwra = {
	"msx_champpwra", "msx_champpwr", "msx_msx", NULL, "1985",
	"Champion Pro Wrestling (Jpn, Alt)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_champpwraRomInfo, MSX_champpwraRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Champion Pro Wrestling (Jpn, Alt 2)

static struct BurnRomInfo MSX_champpwrbRomDesc[] = {
	{ "champion pro wrestling (japan) (alt 2).rom",	0x08000, 0xd4b0accd, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_champpwrb, MSX_champpwrb, msx_msx)
STD_ROM_FN(MSX_champpwrb)

struct BurnDriver BurnDrvMSX_champpwrb = {
	"msx_champpwrb", "msx_champpwr", "msx_msx", NULL, "1985",
	"Champion Pro Wrestling (Jpn, Alt 2)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_champpwrbRomInfo, MSX_champpwrbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Champion Soccer (Jpn)

static struct BurnRomInfo MSX_champscrRomDesc[] = {
	{ "champion soccer (japan).rom",	0x04000, 0x67ab6f8f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_champscr, MSX_champscr, msx_msx)
STD_ROM_FN(MSX_champscr)

struct BurnDriver BurnDrvMSX_champscr = {
	"msx_champscr", NULL, "msx_msx", NULL, "1985",
	"Champion Soccer (Jpn)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_champscrRomInfo, MSX_champscrRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Championship Lode Runner (Jpn)

static struct BurnRomInfo MSX_cloderunRomDesc[] = {
	{ "championship lode runner (japan).rom",	0x08000, 0x2f75e79c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_cloderun, MSX_cloderun, msx_msx)
STD_ROM_FN(MSX_cloderun)

struct BurnDriver BurnDrvMSX_cloderun = {
	"msx_cloderun", NULL, "msx_msx", NULL, "1985",
	"Championship Lode Runner (Jpn)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_cloderunRomInfo, MSX_cloderunRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Checkers in Tan Tan Tanuki (Jpn)

static struct BurnRomInfo MSX_tantanRomDesc[] = {
	{ "checkers in tantan tanuki (japan).rom",	0x04000, 0xd2b8c058, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_tantan, MSX_tantan, msx_msx)
STD_ROM_FN(MSX_tantan)

struct BurnDriver BurnDrvMSX_tantan = {
	"msx_tantan", NULL, "msx_msx", NULL, "1985",
	"Checkers in Tan Tan Tanuki (Jpn)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_tantanRomInfo, MSX_tantanRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Chess (Jpn)

static struct BurnRomInfo MSX_chessRomDesc[] = {
	{ "chess (japan).rom",	0x04000, 0x11eed1c7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_chess, MSX_chess, msx_msx)
STD_ROM_FN(MSX_chess)

struct BurnDriver BurnDrvMSX_chess = {
	"msx_chess", NULL, "msx_msx", NULL, "1984",
	"Chess (Jpn)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_chessRomInfo, MSX_chessRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Choplifter (Jpn)

static struct BurnRomInfo MSX_chopliftRomDesc[] = {
	{ "choplifter (japan).rom",	0x08000, 0x1f5eafc8, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_choplift, MSX_choplift, msx_msx)
STD_ROM_FN(MSX_choplift)

struct BurnDriver BurnDrvMSX_choplift = {
	"msx_choplift", NULL, "msx_msx", NULL, "1985",
	"Choplifter (Jpn)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_chopliftRomInfo, MSX_chopliftRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Choplifter (Jpn, Alt)

static struct BurnRomInfo MSX_chopliftaRomDesc[] = {
	{ "choplifter (japan) (alt 1).rom",	0x08000, 0xc6f30d45, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_choplifta, MSX_choplifta, msx_msx)
STD_ROM_FN(MSX_choplifta)

struct BurnDriver BurnDrvMSX_choplifta = {
	"msx_choplifta", "msx_choplift", "msx_msx", NULL, "1985",
	"Choplifter (Jpn, Alt)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_chopliftaRomInfo, MSX_chopliftaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Choplifter (Jpn, Alt 2)

static struct BurnRomInfo MSX_chopliftbRomDesc[] = {
	{ "choplifter (japan) (alt 2).rom",	0x08000, 0x155768b0, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_chopliftb, MSX_chopliftb, msx_msx)
STD_ROM_FN(MSX_chopliftb)

struct BurnDriver BurnDrvMSX_chopliftb = {
	"msx_chopliftb", "msx_choplift", "msx_msx", NULL, "1985",
	"Choplifter (Jpn, Alt 2)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_chopliftbRomInfo, MSX_chopliftbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Choro Q (Jpn)

static struct BurnRomInfo MSX_choroqRomDesc[] = {
	{ "choro q (japan).rom",	0x04000, 0x5506bf9b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_choroq, MSX_choroq, msx_msx)
STD_ROM_FN(MSX_choroq)

struct BurnDriver BurnDrvMSX_choroq = {
	"msx_choroq", NULL, "msx_msx", NULL, "1984",
	"Choro Q (Jpn)\0", NULL, "Taito", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_choroqRomInfo, MSX_choroqRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Choro Q (Jpn, Alt)

static struct BurnRomInfo MSX_choroqaRomDesc[] = {
	{ "choro q (japan) (alt 1).rom",	0x04000, 0x65b34072, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_choroqa, MSX_choroqa, msx_msx)
STD_ROM_FN(MSX_choroqa)

struct BurnDriver BurnDrvMSX_choroqa = {
	"msx_choroqa", "msx_choroq", "msx_msx", NULL, "1984",
	"Choro Q (Jpn, Alt)\0", NULL, "Taito", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_choroqaRomInfo, MSX_choroqaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Choro Q (Kor)

static struct BurnRomInfo MSX_choroqkRomDesc[] = {
	{ "choro q (1984)(taito)[cr prosoft].rom",	0x08000, 0xba580eb7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_choroqk, MSX_choroqk, msx_msx)
STD_ROM_FN(MSX_choroqk)

struct BurnDriver BurnDrvMSX_choroqk = {
	"msx_choroqk", "msx_choroq", "msx_msx", NULL, "19??",
	"Choro Q (Kor)\0", NULL, "Prosoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_choroqkRomInfo, MSX_choroqkRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Chuugaku Hisshuu Eibunpou 1 (Jpn)

static struct BurnRomInfo MSX_chuheib1RomDesc[] = {
	{ "chugaku hisshu eibunpo 1 (japan).rom",	0x08000, 0x097e4a7e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_chuheib1, MSX_chuheib1, msx_msx)
STD_ROM_FN(MSX_chuheib1)

struct BurnDriver BurnDrvMSX_chuheib1 = {
	"msx_chuheib1", NULL, "msx_msx", NULL, "1984",
	"Chuugaku Hisshuu Eibunpou 1 (Jpn)\0", NULL, "Stratford Computer Center", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_chuheib1RomInfo, MSX_chuheib1RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Circus Charlie (Jpn)

static struct BurnRomInfo MSX_circuscRomDesc[] = {
	{ "circus charlie (japan).rom",	0x04000, 0x83b8d8f3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_circusc, MSX_circusc, msx_msx)
STD_ROM_FN(MSX_circusc)

struct BurnDriver BurnDrvMSX_circusc = {
	"msx_circusc", NULL, "msx_msx", NULL, "1984",
	"Circus Charlie (Jpn)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_circuscRomInfo, MSX_circuscRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// City Connection (Jpn)

static struct BurnRomInfo MSX_cityconRomDesc[] = {
	{ "city connection (japan).rom",	0x08000, 0x6031a583, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_citycon, MSX_citycon, msx_msx)
STD_ROM_FN(MSX_citycon)

struct BurnDriver BurnDrvMSX_citycon = {
	"msx_citycon", NULL, "msx_msx", NULL, "1986",
	"City Connection (Jpn)\0", NULL, "Nihon Dexter", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_cityconRomInfo, MSX_cityconRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// City Connection (Jpn, Alt)

static struct BurnRomInfo MSX_cityconaRomDesc[] = {
	{ "city connection (japan) (alt 1).rom",	0x08000, 0x625c3689, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_citycona, MSX_citycona, msx_msx)
STD_ROM_FN(MSX_citycona)

struct BurnDriver BurnDrvMSX_citycona = {
	"msx_citycona", "msx_citycon", "msx_msx", NULL, "1986",
	"City Connection (Jpn, Alt)\0", NULL, "Nihon Dexter", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_cityconaRomInfo, MSX_cityconaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Coaster Race (Jpn)

static struct BurnRomInfo MSX_coastracRomDesc[] = {
	{ "coaster race (japan).rom",	0x08000, 0x399fd445, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_coastrac, MSX_coastrac, msx_msx)
STD_ROM_FN(MSX_coastrac)

struct BurnDriver BurnDrvMSX_coastrac = {
	"msx_coastrac", NULL, "msx_msx", NULL, "1986",
	"Coaster Race (Jpn)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_coastracRomInfo, MSX_coastracRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Coaster Race (Jpn, Alt)

static struct BurnRomInfo MSX_coastracaRomDesc[] = {
	{ "coaster race (japan) (alt 1).rom",	0x08000, 0xa48acff7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_coastraca, MSX_coastraca, msx_msx)
STD_ROM_FN(MSX_coastraca)

struct BurnDriver BurnDrvMSX_coastraca = {
	"msx_coastraca", "msx_coastrac", "msx_msx", NULL, "1986",
	"Coaster Race (Jpn, Alt)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_coastracaRomInfo, MSX_coastracaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Coaster Race (Jpn, Alt 2)

static struct BurnRomInfo MSX_coastracbRomDesc[] = {
	{ "coaster race (japan) (alt 2).rom",	0x08000, 0x00d996ff, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_coastracb, MSX_coastracb, msx_msx)
STD_ROM_FN(MSX_coastracb)

struct BurnDriver BurnDrvMSX_coastracb = {
	"msx_coastracb", "msx_coastrac", "msx_msx", NULL, "1986",
	"Coaster Race (Jpn, Alt 2)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_coastracbRomInfo, MSX_coastracbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Color Ball (Jpn)

static struct BurnRomInfo MSX_colballRomDesc[] = {
	{ "color ball (japan).rom",	0x02000, 0x12552a1f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_colball, MSX_colball, msx_msx)
STD_ROM_FN(MSX_colball)

struct BurnDriver BurnDrvMSX_colball = {
	"msx_colball", NULL, "msx_msx", NULL, "1984",
	"Color Ball (Jpn)\0", NULL, "Hudson Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_colballRomInfo, MSX_colballRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Color Ball (Jpn, Alt)

static struct BurnRomInfo MSX_colballaRomDesc[] = {
	{ "color ball (japan) (alt 1).rom",	0x04000, 0x73f1e939, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_colballa, MSX_colballa, msx_msx)
STD_ROM_FN(MSX_colballa)

struct BurnDriver BurnDrvMSX_colballa = {
	"msx_colballa", "msx_colball", "msx_msx", NULL, "1984",
	"Color Ball (Jpn, Alt)\0", NULL, "Hudson Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_colballaRomInfo, MSX_colballaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Color Ball (Jpn, Alt 2)

static struct BurnRomInfo MSX_colballbRomDesc[] = {
	{ "color ball (japan) (alt 2).rom",	0x04000, 0xf4f6e561, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_colballb, MSX_colballb, msx_msx)
STD_ROM_FN(MSX_colballb)

struct BurnDriver BurnDrvMSX_colballb = {
	"msx_colballb", "msx_colball", "msx_msx", NULL, "1984",
	"Color Ball (Jpn, Alt 2)\0", NULL, "Hudson Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_colballbRomInfo, MSX_colballbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Come On! Picot (Jpn)

static struct BurnRomInfo MSX_picotRomDesc[] = {
	{ "come on! picot (japan).rom",	0x08000, 0x98b1cc99, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_picot, MSX_picot, msx_msx)
STD_ROM_FN(MSX_picot)

struct BurnDriver BurnDrvMSX_picot = {
	"msx_picot", NULL, "msx_msx", NULL, "1986",
	"Come On! Picot (Jpn)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_picotRomInfo, MSX_picotRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Comet Tail (Jpn)

static struct BurnRomInfo MSX_cometailRomDesc[] = {
	{ "comet tail (japan).rom",	0x04000, 0x74af0726, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_cometail, MSX_cometail, msx_msx)
STD_ROM_FN(MSX_cometail)

struct BurnDriver BurnDrvMSX_cometail = {
	"msx_cometail", NULL, "msx_msx", NULL, "1983",
	"Comet Tail (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_cometailRomInfo, MSX_cometailRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Comic Bakery (Jpn)

static struct BurnRomInfo MSX_comicbakRomDesc[] = {
	{ "comic bakery (japan).rom",	0x04000, 0xa4097e41, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_comicbak, MSX_comicbak, msx_msx)
STD_ROM_FN(MSX_comicbak)

struct BurnDriver BurnDrvMSX_comicbak = {
	"msx_comicbak", NULL, "msx_msx", NULL, "1984",
	"Comic Bakery (Jpn)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_comicbakRomInfo, MSX_comicbakRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Comic Bakery (Jpn, Alt)

static struct BurnRomInfo MSX_comicbakaRomDesc[] = {
	{ "comic bakery (japan) (alt 1).rom",	0x04000, 0x9a70d9c9, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_comicbaka, MSX_comicbaka, msx_msx)
STD_ROM_FN(MSX_comicbaka)

struct BurnDriver BurnDrvMSX_comicbaka = {
	"msx_comicbaka", "msx_comicbak", "msx_msx", NULL, "1984",
	"Comic Bakery (Jpn, Alt)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_comicbakaRomInfo, MSX_comicbakaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Comic Bakery (Jpn, Alt 2)

static struct BurnRomInfo MSX_comicbakbRomDesc[] = {
	{ "comic bakery (japan) (alt 2).rom",	0x04000, 0x03dae53c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_comicbakb, MSX_comicbakb, msx_msx)
STD_ROM_FN(MSX_comicbakb)

struct BurnDriver BurnDrvMSX_comicbakb = {
	"msx_comicbakb", "msx_comicbak", "msx_msx", NULL, "1984",
	"Comic Bakery (Jpn, Alt 2)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_comicbakbRomInfo, MSX_comicbakbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Computer Billiards (Jpn)

static struct BurnRomInfo MSX_cbilliarRomDesc[] = {
	{ "computer billiards (japan).rom",	0x02000, 0x9098682d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_cbilliar, MSX_cbilliar, msx_msx)
STD_ROM_FN(MSX_cbilliar)

struct BurnDriver BurnDrvMSX_cbilliar = {
	"msx_cbilliar", NULL, "msx_msx", NULL, "1983",
	"Computer Billiards (Jpn)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_cbilliarRomInfo, MSX_cbilliarRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Computer Nyuumon (Jpn)

static struct BurnRomInfo MSX_cnyumonRomDesc[] = {
	{ "computer nyuumon - computer lessons (japan).rom",	0x08000, 0x50bb1930, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_cnyumon, MSX_cnyumon, msx_msx)
STD_ROM_FN(MSX_cnyumon)

struct BurnDriver BurnDrvMSX_cnyumon = {
	"msx_cnyumon", NULL, "msx_msx", NULL, "1985",
	"Computer Nyuumon (Jpn)\0", NULL, "Casio", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_cnyumonRomInfo, MSX_cnyumonRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Computer Pachinko (Jpn)

static struct BurnRomInfo MSX_cpachiRomDesc[] = {
	{ "computer pachinko (japan).rom",	0x04000, 0x1bd2c6b3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_cpachi, MSX_cpachi, msx_msx)
STD_ROM_FN(MSX_cpachi)

struct BurnDriver BurnDrvMSX_cpachi = {
	"msx_cpachi", NULL, "msx_msx", NULL, "1984",
	"Computer Pachinko (Jpn)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_cpachiRomInfo, MSX_cpachiRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Con-Dori (Jpn)

static struct BurnRomInfo MSX_condoriRomDesc[] = {
	{ "con-dori (japan).rom",	0x02000, 0xd9006ebe, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_condori, MSX_condori, msx_msx)
STD_ROM_FN(MSX_condori)

struct BurnDriver BurnDrvMSX_condori = {
	"msx_condori", NULL, "msx_msx", NULL, "1983",
	"Con-Dori (Jpn)\0", NULL, "Cross Talk", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_condoriRomInfo, MSX_condoriRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Con-Dori (Jpn, Alt)

static struct BurnRomInfo MSX_condoriaRomDesc[] = {
	{ "con-dori (japan) (alt 1).rom",	0x04000, 0xce71bb57, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_condoria, MSX_condoria, msx_msx)
STD_ROM_FN(MSX_condoria)

struct BurnDriver BurnDrvMSX_condoria = {
	"msx_condoria", "msx_condori", "msx_msx", NULL, "1983",
	"Con-Dori (Jpn, Alt)\0", NULL, "Cross Talk", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_condoriaRomInfo, MSX_condoriaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Con-Dori (Jpn, Alt 2)

static struct BurnRomInfo MSX_condoribRomDesc[] = {
	{ "con-dori (japan) (alt 2).rom",	0x04000, 0x69405f96, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_condorib, MSX_condorib, msx_msx)
STD_ROM_FN(MSX_condorib)

struct BurnDriver BurnDrvMSX_condorib = {
	"msx_condorib", "msx_condori", "msx_msx", NULL, "1983",
	"Con-Dori (Jpn, Alt 2)\0", NULL, "Cross Talk", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_condoribRomInfo, MSX_condoribRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Confused? (Euro)

static struct BurnRomInfo MSX_confusedRomDesc[] = {
	{ "confused (europe).rom",	0x0c000, 0x63084e71, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_confused, MSX_confused, msx_msx)
STD_ROM_FN(MSX_confused)

struct BurnDriver BurnDrvMSX_confused = {
	"msx_confused", NULL, "msx_msx", NULL, "198?",
	"Confused? (Euro)\0", NULL, "Eaglesoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_confusedRomInfo, MSX_confusedRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Cosmo (Jpn)

static struct BurnRomInfo MSX_cosmoRomDesc[] = {
	{ "cosmo (japan).rom",	0x04000, 0x5780acfd, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_cosmo, MSX_cosmo, msx_msx)
STD_ROM_FN(MSX_cosmo)

struct BurnDriver BurnDrvMSX_cosmo = {
	"msx_cosmo", NULL, "msx_msx", NULL, "1984",
	"Cosmo (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_cosmoRomInfo, MSX_cosmoRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Cosmo (Jpn, Alt)

static struct BurnRomInfo MSX_cosmoaRomDesc[] = {
	{ "cosmo (japan) (alt 1).rom",	0x04000, 0xff3c8091, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_cosmoa, MSX_cosmoa, msx_msx)
STD_ROM_FN(MSX_cosmoa)

struct BurnDriver BurnDrvMSX_cosmoa = {
	"msx_cosmoa", "msx_cosmo", "msx_msx", NULL, "1984",
	"Cosmo (Jpn, Alt)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_cosmoaRomInfo, MSX_cosmoaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Cosmo-Explorer (Jpn)

static struct BurnRomInfo MSX_cosmoexpRomDesc[] = {
	{ "cosmo-explorer (japan).rom",	0x08000, 0x3f534824, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_cosmoexp, MSX_cosmoexp, msx_msx)
STD_ROM_FN(MSX_cosmoexp)

struct BurnDriver BurnDrvMSX_cosmoexp = {
	"msx_cosmoexp", NULL, "msx_msx", NULL, "1985",
	"Cosmo-Explorer (Jpn)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_cosmoexpRomInfo, MSX_cosmoexpRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Cosmo-Explorer (Jpn, Alt)

static struct BurnRomInfo MSX_cosmoexpaRomDesc[] = {
	{ "cosmo-explorer (japan) (alt 1).rom",	0x08000, 0xd21f6b76, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_cosmoexpa, MSX_cosmoexpa, msx_msx)
STD_ROM_FN(MSX_cosmoexpa)

struct BurnDriver BurnDrvMSX_cosmoexpa = {
	"msx_cosmoexpa", "msx_cosmoexp", "msx_msx", NULL, "1985",
	"Cosmo-Explorer (Jpn, Alt)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_cosmoexpaRomInfo, MSX_cosmoexpaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Craze (Jpn)

static struct BurnRomInfo MSX_crazeRomDesc[] = {
	{ "craze (japan).rom",	0x20000, 0xd83966d0, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_craze, MSX_craze, msx_msx)
STD_ROM_FN(MSX_craze)

struct BurnDriver BurnDrvMSX_craze = {
	"msx_craze", NULL, "msx_msx", NULL, "1988",
	"Craze (Jpn)\0", NULL, "Heart Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII16, GBF_MISC, 0,
	MSXGetZipName, MSX_crazeRomInfo, MSX_crazeRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Crazy Bullet (Jpn)

static struct BurnRomInfo MSX_crazybulRomDesc[] = {
	{ "crazy bullet (japan).rom",	0x04000, 0xd0e8f418, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_crazybul, MSX_crazybul, msx_msx)
STD_ROM_FN(MSX_crazybul)

struct BurnDriver BurnDrvMSX_crazybul = {
	"msx_crazybul", NULL, "msx_msx", NULL, "1983",
	"Crazy Bullet (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_BASIC, GBF_MISC, 0,
	MSXGetZipName, MSX_crazybulRomInfo, MSX_crazybulRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Crazy Cars (Euro)

static struct BurnRomInfo MSX_crazycarRomDesc[] = {
	{ "crazy cars (europe).rom",	0x08000, 0xde18372d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_crazycar, MSX_crazycar, msx_msx)
STD_ROM_FN(MSX_crazycar)

struct BurnDriver BurnDrvMSX_crazycar = {
	"msx_crazycar", NULL, "msx_msx", NULL, "1988",
	"Crazy Cars (Euro)\0", NULL, "Titus", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_crazycarRomInfo, MSX_crazycarRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Crazy Train (Jpn)

static struct BurnRomInfo MSX_crazytrnRomDesc[] = {
	{ "crazy train (japan).rom",	0x02000, 0x39e6ff25, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_crazytrn, MSX_crazytrn, msx_msx)
STD_ROM_FN(MSX_crazytrn)

struct BurnDriver BurnDrvMSX_crazytrn = {
	"msx_crazytrn", NULL, "msx_msx", NULL, "1983",
	"Crazy Train (Jpn)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_crazytrnRomInfo, MSX_crazytrnRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Crazy Train (Jpn, Alt)

static struct BurnRomInfo MSX_crazytrnaRomDesc[] = {
	{ "crazy train (japan) (alt 1).rom",	0x02000, 0xb8764cd3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_crazytrna, MSX_crazytrna, msx_msx)
STD_ROM_FN(MSX_crazytrna)

struct BurnDriver BurnDrvMSX_crazytrna = {
	"msx_crazytrna", "msx_crazytrn", "msx_msx", NULL, "1983",
	"Crazy Train (Jpn, Alt)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_crazytrnaRomInfo, MSX_crazytrnaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Cross Blaim (Jpn)

static struct BurnRomInfo MSX_crossblmRomDesc[] = {
	{ "cross blaim (japan).rom",	0x10000, 0x47273220, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_crossblm, MSX_crossblm, msx_msx)
STD_ROM_FN(MSX_crossblm)

struct BurnDriver BurnDrvMSX_crossblm = {
	"msx_crossblm", NULL, "msx_msx", NULL, "1986",
	"Cross Blaim (Jpn)\0", NULL, "dB-Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_CROSS_BLAIM, GBF_MISC, 0,
	MSXGetZipName, MSX_crossblmRomInfo, MSX_crossblmRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Crusader (Jpn)

static struct BurnRomInfo MSX_crusaderRomDesc[] = {
	{ "crusader (japan).rom",	0x08000, 0x0b69dd50, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_crusader, MSX_crusader, msx_msx)
STD_ROM_FN(MSX_crusader)

struct BurnDriver BurnDrvMSX_crusader = {
	"msx_crusader", NULL, "msx_msx", NULL, "1985",
	"Crusader (Jpn)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_crusaderRomInfo, MSX_crusaderRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Crusader (Jpn, Alt)

static struct BurnRomInfo MSX_crusaderaRomDesc[] = {
	{ "crusader (japan) (alt 1).rom",	0x08000, 0xe167fede, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_crusadera, MSX_crusadera, msx_msx)
STD_ROM_FN(MSX_crusadera)

struct BurnDriver BurnDrvMSX_crusadera = {
	"msx_crusadera", "msx_crusader", "msx_msx", NULL, "1985",
	"Crusader (Jpn, Alt)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_crusaderaRomInfo, MSX_crusaderaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Crusader (Jpn, Alt 2)

static struct BurnRomInfo MSX_crusaderbRomDesc[] = {
	{ "crusader (japan) (alt 2).rom",	0x08000, 0x40afef74, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_crusaderb, MSX_crusaderb, msx_msx)
STD_ROM_FN(MSX_crusaderb)

struct BurnDriver BurnDrvMSX_crusaderb = {
	"msx_crusaderb", "msx_crusader", "msx_msx", NULL, "1985",
	"Crusader (Jpn, Alt 2)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_crusaderbRomInfo, MSX_crusaderbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Crusader (Kor)

static struct BurnRomInfo MSX_crusaderkRomDesc[] = {
	{ "crusader.rom",	0x08000, 0x73f3c7e2, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_crusaderk, MSX_crusaderk, msx_msx)
STD_ROM_FN(MSX_crusaderk)

struct BurnDriver BurnDrvMSX_crusaderk = {
	"msx_crusaderk", "msx_crusader", "msx_msx", NULL, "198?",
	"Crusader (Kor)\0", NULL, "Clover", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_crusaderkRomInfo, MSX_crusaderkRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Cyborg Z (Kor)

static struct BurnRomInfo MSX_cyborgzRomDesc[] = {
	{ "cyborg z (zemina, 1991).rom",	0x20000, 0x77efe84a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_cyborgz, MSX_cyborgz, msx_msx)
STD_ROM_FN(MSX_cyborgz)

struct BurnDriver BurnDrvMSX_cyborgz = {
	"msx_cyborgz", NULL, "msx_msx", NULL, "1991",
	"Cyborg Z (Kor)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_KONAMI, GBF_MISC, 0,
	MSXGetZipName, MSX_cyborgzRomInfo, MSX_cyborgzRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// D-Day (Jpn)

static struct BurnRomInfo MSX_ddayRomDesc[] = {
	{ "d-day (japan).rom",	0x04000, 0x5d284ea6, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_dday, MSX_dday, msx_msx)
STD_ROM_FN(MSX_dday)

struct BurnDriver BurnDrvMSX_dday = {
	"msx_dday", NULL, "msx_msx", NULL, "1984",
	"D-Day (Jpn)\0", NULL, "Toshiba", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_ddayRomInfo, MSX_ddayRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// D-Day (Jpn, Alt)

static struct BurnRomInfo MSX_ddayaRomDesc[] = {
	{ "d-day (japan) (alt 1).rom",	0x04000, 0x6641c97a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_ddaya, MSX_ddaya, msx_msx)
STD_ROM_FN(MSX_ddaya)

struct BurnDriver BurnDrvMSX_ddaya = {
	"msx_ddaya", "msx_dday", "msx_msx", NULL, "1984",
	"D-Day (Jpn, Alt)\0", NULL, "Toshiba", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_ddayaRomInfo, MSX_ddayaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// D-Day (Jpn, Alt 2)

static struct BurnRomInfo MSX_ddaybRomDesc[] = {
	{ "d-day (japan) (alt 2).rom",	0x04000, 0xa7f0dd41, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_ddayb, MSX_ddayb, msx_msx)
STD_ROM_FN(MSX_ddayb)

struct BurnDriver BurnDrvMSX_ddayb = {
	"msx_ddayb", "msx_dday", "msx_msx", NULL, "1984",
	"D-Day (Jpn, Alt 2)\0", NULL, "Toshiba", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_ddaybRomInfo, MSX_ddaybRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// D-Day (Jpn, Alt 3)

static struct BurnRomInfo MSX_ddaycRomDesc[] = {
	{ "d-day (japan) (alt 3).rom",	0x04000, 0xa89b0f57, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_ddayc, MSX_ddayc, msx_msx)
STD_ROM_FN(MSX_ddayc)

struct BurnDriver BurnDrvMSX_ddayc = {
	"msx_ddayc", "msx_dday", "msx_msx", NULL, "1984",
	"D-Day (Jpn, Alt 3)\0", NULL, "Toshiba", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_ddaycRomInfo, MSX_ddaycRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// D-Day (Kor)

static struct BurnRomInfo MSX_ddaykRomDesc[] = {
	{ "d-day.rom",	0x04000, 0x8a79ee57, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_ddayk, MSX_ddayk, msx_msx)
STD_ROM_FN(MSX_ddayk)

struct BurnDriver BurnDrvMSX_ddayk = {
	"msx_ddayk", "msx_dday", "msx_msx", NULL, "198?",
	"D-Day (Kor)\0", NULL, "San Ho", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_ddaykRomInfo, MSX_ddaykRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Dai Shougai Keiba (Jpn)

static struct BurnRomInfo MSX_dskeibaRomDesc[] = {
	{ "casio daishogai keiba (japan).rom",	0x04000, 0x5312db08, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_dskeiba, MSX_dskeiba, msx_msx)
STD_ROM_FN(MSX_dskeiba)

struct BurnDriver BurnDrvMSX_dskeiba = {
	"msx_dskeiba", NULL, "msx_msx", NULL, "1984",
	"Dai Shougai Keiba (Jpn)\0", NULL, "Casio", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_dskeibaRomInfo, MSX_dskeibaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Dai Dassou (Jpn)

static struct BurnRomInfo MSX_daidassoRomDesc[] = {
	{ "daidasso (japan).rom",	0x08000, 0xfc17c9bc, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_daidasso, MSX_daidasso, msx_msx)
STD_ROM_FN(MSX_daidasso)

struct BurnDriver BurnDrvMSX_daidasso = {
	"msx_daidasso", NULL, "msx_msx", NULL, "1985",
	"Dai Dassou (Jpn)\0", NULL, "Carry Lab", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_daidassoRomInfo, MSX_daidassoRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// The Dam Busters (Euro?)

static struct BurnRomInfo MSX_dambustrRomDesc[] = {
	{ "dam busters, the (japan) (alt 1).rom",	0x08000, 0x711a7a6e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_dambustr, MSX_dambustr, msx_msx)
STD_ROM_FN(MSX_dambustr)

struct BurnDriver BurnDrvMSX_dambustr = {
	"msx_dambustr", NULL, "msx_msx", NULL, "1985",
	"The Dam Busters (Euro?)\0", NULL, "Comptiq", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_dambustrRomInfo, MSX_dambustrRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// The Dam Busters (Jpn)

static struct BurnRomInfo MSX_dambustraRomDesc[] = {
	{ "dam busters, the (japan).rom",	0x08000, 0x6fde5bca, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_dambustra, MSX_dambustra, msx_msx)
STD_ROM_FN(MSX_dambustra)

struct BurnDriver BurnDrvMSX_dambustra = {
	"msx_dambustra", "msx_dambustr", "msx_msx", NULL, "1985",
	"The Dam Busters (Jpn)\0", NULL, "Comptiq", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_dambustraRomInfo, MSX_dambustraRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Danger X4 (Jpn)

static struct BurnRomInfo MSX_dangerx4RomDesc[] = {
	{ "danger x4 (japan).rom",	0x04000, 0x571f12fb, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_dangerx4, MSX_dangerx4, msx_msx)
STD_ROM_FN(MSX_dangerx4)

struct BurnDriver BurnDrvMSX_dangerx4 = {
	"msx_dangerx4", NULL, "msx_msx", NULL, "1984",
	"Danger X4 (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_dangerx4RomInfo, MSX_dangerx4RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Danger X4 (Jpn, Alt)

static struct BurnRomInfo MSX_dangerx4aRomDesc[] = {
	{ "danger x4 (japan) (alt 1).rom",	0x04000, 0x3cf16a16, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_dangerx4a, MSX_dangerx4a, msx_msx)
STD_ROM_FN(MSX_dangerx4a)

struct BurnDriver BurnDrvMSX_dangerx4a = {
	"msx_dangerx4a", "msx_dangerx4", "msx_msx", NULL, "1984",
	"Danger X4 (Jpn, Alt)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_dangerx4aRomInfo, MSX_dangerx4aRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Danger X4 (Jpn, Alt 2)

static struct BurnRomInfo MSX_dangerx4bRomDesc[] = {
	{ "danger x4 (japan) (alt 2).rom",	0x04000, 0x22f5e82a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_dangerx4b, MSX_dangerx4b, msx_msx)
STD_ROM_FN(MSX_dangerx4b)

struct BurnDriver BurnDrvMSX_dangerx4b = {
	"msx_dangerx4b", "msx_dangerx4", "msx_msx", NULL, "1984",
	"Danger X4 (Jpn, Alt 2)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_BASIC, GBF_MISC, 0,
	MSXGetZipName, MSX_dangerx4bRomInfo, MSX_dangerx4bRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Dawn Patrol (Jpn)

static struct BurnRomInfo MSX_dawnpatrRomDesc[] = {
	{ "dawn patrol (japan).rom",	0x10000, 0x1fc65e80, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_dawnpatr, MSX_dawnpatr, msx_msx)
STD_ROM_FN(MSX_dawnpatr)

struct BurnDriver BurnDrvMSX_dawnpatr = {
	"msx_dawnpatr", NULL, "msx_msx", NULL, "1986",
	"Dawn Patrol (Jpn)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_dawnpatrRomInfo, MSX_dawnpatrRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Decathlon (Jpn)

static struct BurnRomInfo MSX_decathlnRomDesc[] = {
	{ "decathlon (japan).rom",	0x04000, 0xf99b1c22, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_decathln, MSX_decathln, msx_msx)
STD_ROM_FN(MSX_decathln)

struct BurnDriver BurnDrvMSX_decathln = {
	"msx_decathln", NULL, "msx_msx", NULL, "1984",
	"Decathlon (Jpn)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_decathlnRomInfo, MSX_decathlnRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Deep Dungeon (Jpn)

static struct BurnRomInfo MSX_deepdngRomDesc[] = {
	{ "deep dungeon (japan).rom",	0x20000, 0x27fd8f9a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_deepdng, MSX_deepdng, msx_msx)
STD_ROM_FN(MSX_deepdng)

struct BurnDriver BurnDrvMSX_deepdng = {
	"msx_deepdng", NULL, "msx_msx", NULL, "1988",
	"Deep Dungeon (Jpn)\0", NULL, "ScapTrust", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_deepdngRomInfo, MSX_deepdngRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Deep Dungeon II - Yuushi no Monshou (Jpn)

static struct BurnRomInfo MSX_deepdng2RomDesc[] = {
	{ "deep dungeon ii (japan).rom",	0x20000, 0x101db19c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_deepdng2, MSX_deepdng2, msx_msx)
STD_ROM_FN(MSX_deepdng2)

struct BurnDriver BurnDrvMSX_deepdng2 = {
	"msx_deepdng2", NULL, "msx_msx", NULL, "1988",
	"Deep Dungeon II - Yuushi no Monshou (Jpn)\0", NULL, "ScapTrust", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_deepdng2RomInfo, MSX_deepdng2RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// The Demon Crystal (Jpn)

static struct BurnRomInfo MSX_dcrystalRomDesc[] = {
	{ "demon crystal, the (japan).rom",	0x08000, 0x11de4dfd, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_dcrystal, MSX_dcrystal, msx_msx)
STD_ROM_FN(MSX_dcrystal)

struct BurnDriver BurnDrvMSX_dcrystal = {
	"msx_dcrystal", NULL, "msx_msx", NULL, "1986",
	"The Demon Crystal (Jpn)\0", NULL, "Dempa", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_dcrystalRomInfo, MSX_dcrystalRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// D.H. - Devil's Heaven (Jpn)

static struct BurnRomInfo MSX_devilhvnRomDesc[] = {
	{ "devil's heaven (japan).rom",	0x04000, 0xce08f27d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_devilhvn, MSX_devilhvn, msx_msx)
STD_ROM_FN(MSX_devilhvn)

struct BurnDriver BurnDrvMSX_devilhvn = {
	"msx_devilhvn", NULL, "msx_msx", NULL, "1984",
	"D.H. - Devil's Heaven (Jpn)\0", NULL, "General", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_devilhvnRomInfo, MSX_devilhvnRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Dig Dug (Jpn)

static struct BurnRomInfo MSX_digdugRomDesc[] = {
	{ "dig dug (japan).rom",	0x08000, 0x3c749758, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_digdug, MSX_digdug, msx_msx)
STD_ROM_FN(MSX_digdug)

struct BurnDriver BurnDrvMSX_digdug = {
	"msx_digdug", NULL, "msx_msx", NULL, "1984",
	"Dig Dug (Jpn)\0", NULL, "Namcot", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_digdugRomInfo, MSX_digdugRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Dig Dug (Jpn, Alt)

static struct BurnRomInfo MSX_digdugaRomDesc[] = {
	{ "dig dug (japan) (alt 1).rom",	0x08000, 0x7c3d7dea, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_digduga, MSX_digduga, msx_msx)
STD_ROM_FN(MSX_digduga)

struct BurnDriver BurnDrvMSX_digduga = {
	"msx_digduga", "msx_digdug", "msx_msx", NULL, "1984",
	"Dig Dug (Jpn, Alt)\0", NULL, "Namcot", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_digdugaRomInfo, MSX_digdugaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Digital Devil Monogatari - Megami Tensei (Jpn, Alt)

static struct BurnRomInfo MSX_megamitaRomDesc[] = {
	{ "digital devil monogatari megami tensei (japan) (alt 1).rom",	0x20000, 0x367d385e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_megamita, MSX_megamita, msx_msx)
STD_ROM_FN(MSX_megamita)

struct BurnDriver BurnDrvMSX_megamita = {
	"msx_megamita", "msx_megamit", "msx_msx", NULL, "1987",
	"Digital Devil Monogatari - Megami Tensei (Jpn, Alt)\0", NULL, "Nihon Telenet", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_KONAMI, GBF_MISC, 0,
	MSXGetZipName, MSX_megamitaRomInfo, MSX_megamitaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Doki Doki Penguin Land (Jpn)

static struct BurnRomInfo MSX_dokidokiRomDesc[] = {
	{ "doki doki penguin land (japan).rom",	0x08000, 0x652d0e39, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_dokidoki, MSX_dokidoki, msx_msx)
STD_ROM_FN(MSX_dokidoki)

struct BurnDriver BurnDrvMSX_dokidoki = {
	"msx_dokidoki", NULL, "msx_msx", NULL, "1985",
	"Doki Doki Penguin Land (Jpn)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_dokidokiRomInfo, MSX_dokidokiRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Doki Doki Penguin Land (Jpn, Alt)

static struct BurnRomInfo MSX_dokidokiaRomDesc[] = {
	{ "doki doki penguin land (japan) (alt 1).rom",	0x08000, 0xde4af7f6, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_dokidokia, MSX_dokidokia, msx_msx)
STD_ROM_FN(MSX_dokidokia)

struct BurnDriver BurnDrvMSX_dokidokia = {
	"msx_dokidokia", "msx_dokidoki", "msx_msx", NULL, "1985",
	"Doki Doki Penguin Land (Jpn, Alt)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_dokidokiaRomInfo, MSX_dokidokiaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Doordoor (Jpn)

static struct BurnRomInfo MSX_doordoorRomDesc[] = {
	{ "doordoor (japan).rom",	0x04000, 0xf8ad9717, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_doordoor, MSX_doordoor, msx_msx)
STD_ROM_FN(MSX_doordoor)

struct BurnDriver BurnDrvMSX_doordoor = {
	"msx_doordoor", NULL, "msx_msx", NULL, "1985",
	"Doordoor (Jpn)\0", NULL, "Enix", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_doordoorRomInfo, MSX_doordoorRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Dorodon (Jpn)

static struct BurnRomInfo MSX_dorodonRomDesc[] = {
	{ "dorodon (japan).rom",	0x04000, 0x5aa63a76, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_dorodon, MSX_dorodon, msx_msx)
STD_ROM_FN(MSX_dorodon)

struct BurnDriver BurnDrvMSX_dorodon = {
	"msx_dorodon", NULL, "msx_msx", NULL, "1984",
	"Dorodon (Jpn)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_dorodonRomInfo, MSX_dorodonRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Double Dragon (Kor)

static struct BurnRomInfo MSX_ddragonRomDesc[] = {
	{ "double dragon (korea) (unl).rom",	0x08000, 0xc70e3a34, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_ddragon, MSX_ddragon, msx_msx)
STD_ROM_FN(MSX_ddragon)

struct BurnDriver BurnDrvMSX_ddragon = {
	"msx_ddragon", NULL, "msx_msx", NULL, "1989",
	"Double Dragon (Kor)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_ddragonRomInfo, MSX_ddragonRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Dragon Attack (Jpn)

static struct BurnRomInfo MSX_drgnatckRomDesc[] = {
	{ "dragon attack (japan).rom",	0x02000, 0xa0ff8771, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_drgnatck, MSX_drgnatck, msx_msx)
STD_ROM_FN(MSX_drgnatck)

struct BurnDriver BurnDrvMSX_drgnatck = {
	"msx_drgnatck", NULL, "msx_msx", NULL, "1983",
	"Dragon Attack (Jpn)\0", NULL, "HAL Kenkyuujo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_drgnatckRomInfo, MSX_drgnatckRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Dragon Attack (Jpn, Alt)

static struct BurnRomInfo MSX_drgnatckaRomDesc[] = {
	{ "dragon attack (japan) (alt 1).rom",	0x04000, 0x981facd3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_drgnatcka, MSX_drgnatcka, msx_msx)
STD_ROM_FN(MSX_drgnatcka)

struct BurnDriver BurnDrvMSX_drgnatcka = {
	"msx_drgnatcka", "msx_drgnatck", "msx_msx", NULL, "1983",
	"Dragon Attack (Jpn, Alt)\0", NULL, "HAL Kenkyuujo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_drgnatckaRomInfo, MSX_drgnatckaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Dragon Quest II (Jpn, Alt)

static struct BurnRomInfo MSX_dquest2aRomDesc[] = {
	{ "dragon quest ii (japan) (alt 1).rom",	0x40000, 0xd44165a3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_dquest2a, MSX_dquest2a, msx_msx)
STD_ROM_FN(MSX_dquest2a)

struct BurnDriver BurnDrvMSX_dquest2a = {
	"msx_dquest2a", "msx_dquest", "msx_msx", NULL, "1987",
	"Dragon Quest II (Jpn, Alt)\0", NULL, "Enix", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII8, GBF_MISC, 0,
	MSXGetZipName, MSX_dquest2aRomInfo, MSX_dquest2aRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Dragon Slayer (Jpn)

static struct BurnRomInfo MSX_dslayerRomDesc[] = {
	{ "dragon slayer (japan).rom",	0x08000, 0x6a515349, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_dslayer, MSX_dslayer, msx_msx)
STD_ROM_FN(MSX_dslayer)

struct BurnDriver BurnDrvMSX_dslayer = {
	"msx_dslayer", NULL, "msx_msx", NULL, "1985",
	"Dragon Slayer (Jpn)\0", NULL, "Square", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_dslayerRomInfo, MSX_dslayerRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Drainer (Jpn)

static struct BurnRomInfo MSX_drainerRomDesc[] = {
	{ "drainer (japan).rom",	0x08000, 0xdb803e8a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_drainer, MSX_drainer, msx_msx)
STD_ROM_FN(MSX_drainer)

struct BurnDriver BurnDrvMSX_drainer = {
	"msx_drainer", NULL, "msx_msx", NULL, "1987",
	"Drainer (Jpn)\0", NULL, "Victor", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_drainerRomInfo, MSX_drainerRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Dunk Shot (Jpn)

static struct BurnRomInfo MSX_dunkshotRomDesc[] = {
	{ "dunk shot (japan).rom",	0x08000, 0x6c366b32, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_dunkshot, MSX_dunkshot, msx_msx)
STD_ROM_FN(MSX_dunkshot)

struct BurnDriver BurnDrvMSX_dunkshot = {
	"msx_dunkshot", NULL, "msx_msx", NULL, "1986",
	"Dunk Shot (Jpn)\0", NULL, "HAL Kenkyuujo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_dunkshotRomInfo, MSX_dunkshotRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Dunk Shot (Jpn, Alt)

static struct BurnRomInfo MSX_dunkshotaRomDesc[] = {
	{ "dunk shot (japan) (alt 1).rom",	0x08000, 0x99b7eab1, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_dunkshota, MSX_dunkshota, msx_msx)
STD_ROM_FN(MSX_dunkshota)

struct BurnDriver BurnDrvMSX_dunkshota = {
	"msx_dunkshota", "msx_dunkshot", "msx_msx", NULL, "1986",
	"Dunk Shot (Jpn, Alt)\0", NULL, "HAL Kenkyuujo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_dunkshotaRomInfo, MSX_dunkshotaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Dynamite Bowl (Jpn)

static struct BurnRomInfo MSX_dynabowlRomDesc[] = {
	{ "dynamite bowl (japan).rom",	0x20000, 0x47ec57da, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_dynabowl, MSX_dynabowl, msx_msx)
STD_ROM_FN(MSX_dynabowl)

struct BurnDriver BurnDrvMSX_dynabowl = {
	"msx_dynabowl", NULL, "msx_msx", NULL, "1988",
	"Dynamite Bowl (Jpn)\0", NULL, "Toshiba EMI", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII16, GBF_MISC, 0,
	MSXGetZipName, MSX_dynabowlRomInfo, MSX_dynabowlRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// E.I. - Exa Innova (Jpn)

static struct BurnRomInfo MSX_exainnovRomDesc[] = {
	{ "exa innova (japan).rom",	0x04000, 0x4ff88059, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_exainnov, MSX_exainnov, msx_msx)
STD_ROM_FN(MSX_exainnov)

struct BurnDriver BurnDrvMSX_exainnov = {
	"msx_exainnov", NULL, "msx_msx", NULL, "1983",
	"E.I. - Exa Innova (Jpn)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_exainnovRomInfo, MSX_exainnovRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// E.I. - Exa Innova (Jpn, Alt)

static struct BurnRomInfo MSX_exainnov1RomDesc[] = {
	{ "exa innova (japan) (alt 1).rom",	0x04000, 0xd5b51149, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_exainnov1, MSX_exainnov1, msx_msx)
STD_ROM_FN(MSX_exainnov1)

struct BurnDriver BurnDrvMSX_exainnov1 = {
	"msx_exainnov1", "msx_exainnov", "msx_msx", NULL, "1983",
	"E.I. - Exa Innova (Jpn, Alt)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_exainnov1RomInfo, MSX_exainnov1RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Eagles 5 (Kor)

static struct BurnRomInfo MSX_eagle5RomDesc[] = {
	{ "eagles 5 (zemina, 1990).rom",	0x08000, 0xb9df4c42, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_eagle5, MSX_eagle5, msx_msx)
STD_ROM_FN(MSX_eagle5)

struct BurnDriver BurnDrvMSX_eagle5 = {
	"msx_eagle5", NULL, "msx_msx", NULL, "1990",
	"Eagles 5 (Kor)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_eagle5RomInfo, MSX_eagle5RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Eagle Fighter (Jpn)

static struct BurnRomInfo MSX_eaglefgtRomDesc[] = {
	{ "eagle fighter (japan).rom",	0x08000, 0x5c808e73, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_eaglefgt, MSX_eaglefgt, msx_msx)
STD_ROM_FN(MSX_eaglefgt)

struct BurnDriver BurnDrvMSX_eaglefgt = {
	"msx_eaglefgt", NULL, "msx_msx", NULL, "1985",
	"Eagle Fighter (Jpn)\0", NULL, "Casio", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_eaglefgtRomInfo, MSX_eaglefgtRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Eagle Fighter (Jpn, Alt)

static struct BurnRomInfo MSX_eaglefgtaRomDesc[] = {
	{ "eagle fighter (japan) (alt 1).rom",	0x08000, 0xcf7edaeb, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_eaglefgta, MSX_eaglefgta, msx_msx)
STD_ROM_FN(MSX_eaglefgta)

struct BurnDriver BurnDrvMSX_eaglefgta = {
	"msx_eaglefgta", "msx_eaglefgt", "msx_msx", NULL, "1985",
	"Eagle Fighter (Jpn, Alt)\0", NULL, "Casio", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_eaglefgtaRomInfo, MSX_eaglefgtaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Eggerland Mystery (Jpn)

static struct BurnRomInfo MSX_eggerlndRomDesc[] = {
	{ "eggerland mystery (japan).rom",	0x08000, 0x232b1050, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_eggerlnd, MSX_eggerlnd, msx_msx)
STD_ROM_FN(MSX_eggerlnd)

struct BurnDriver BurnDrvMSX_eggerlnd = {
	"msx_eggerlnd", NULL, "msx_msx", NULL, "1985",
	"Eggerland Mystery (Jpn)\0", NULL, "HAL Kenkyuujo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_eggerlndRomInfo, MSX_eggerlndRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Elevator Action (Jpn)

static struct BurnRomInfo MSX_elevatorRomDesc[] = {
	{ "elevator action (japan).rom",	0x08000, 0x39886593, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_elevator, MSX_elevator, msx_msx)
STD_ROM_FN(MSX_elevator)

struct BurnDriver BurnDrvMSX_elevator = {
	"msx_elevator", NULL, "msx_msx", NULL, "1985",
	"Elevator Action (Jpn)\0", NULL, "Nidecom", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_elevatorRomInfo, MSX_elevatorRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Exchanger (Jpn)

static struct BurnRomInfo MSX_exchangrRomDesc[] = {
	{ "exchanger (japan).rom",	0x04000, 0x2c2b8a0e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_exchangr, MSX_exchangr, msx_msx)
STD_ROM_FN(MSX_exchangr)

struct BurnDriver BurnDrvMSX_exchangr = {
	"msx_exchangr", NULL, "msx_msx", NULL, "1984",
	"Exchanger (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_BASIC, GBF_MISC, 0,
	MSXGetZipName, MSX_exchangrRomInfo, MSX_exchangrRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Exchanger (Jpn, Alt)

static struct BurnRomInfo MSX_exchangraRomDesc[] = {
	{ "exchanger (japan) (alt 1).rom",	0x04000, 0x691c0503, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_exchangra, MSX_exchangra, msx_msx)
STD_ROM_FN(MSX_exchangra)

struct BurnDriver BurnDrvMSX_exchangra = {
	"msx_exchangra", "msx_exchangr", "msx_msx", NULL, "1984",
	"Exchanger (Jpn, Alt)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_BASIC, GBF_MISC, 0,
	MSXGetZipName, MSX_exchangraRomInfo, MSX_exchangraRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Exerion (Jpn)

static struct BurnRomInfo MSX_exerionRomDesc[] = {
	{ "exerion (japan).rom",	0x04000, 0x7abefd3d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_exerion, MSX_exerion, msx_msx)
STD_ROM_FN(MSX_exerion)

struct BurnDriver BurnDrvMSX_exerion = {
	"msx_exerion", NULL, "msx_msx", NULL, "1984",
	"Exerion (Jpn)\0", NULL, "Jaleco", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_exerionRomInfo, MSX_exerionRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Exerion (Jpn, Alt)

static struct BurnRomInfo MSX_exerionaRomDesc[] = {
	{ "exerion (japan) (alt 1).rom",	0x04000, 0x24b3b811, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_exeriona, MSX_exeriona, msx_msx)
STD_ROM_FN(MSX_exeriona)

struct BurnDriver BurnDrvMSX_exeriona = {
	"msx_exeriona", "msx_exerion", "msx_msx", NULL, "1984",
	"Exerion (Jpn, Alt)\0", NULL, "Jaleco", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_exerionaRomInfo, MSX_exerionaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Exerion (Jpn, Alt 2)

static struct BurnRomInfo MSX_exerionbRomDesc[] = {
	{ "exerion (japan) (alt 2).rom",	0x04021, 0x369cb84e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_exerionb, MSX_exerionb, msx_msx)
STD_ROM_FN(MSX_exerionb)

struct BurnDriver BurnDrvMSX_exerionb = {
	"msx_exerionb", "msx_exerion", "msx_msx", NULL, "1984",
	"Exerion (Jpn, Alt 2)\0", NULL, "Jaleco", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_exerionbRomInfo, MSX_exerionbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Exerion (Kor)

static struct BurnRomInfo MSX_exerionkRomDesc[] = {
	{ "exerion.rom",	0x08000, 0x55b25506, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_exerionk, MSX_exerionk, msx_msx)
STD_ROM_FN(MSX_exerionk)

struct BurnDriver BurnDrvMSX_exerionk = {
	"msx_exerionk", "msx_exerion", "msx_msx", NULL, "198?",
	"Exerion (Kor)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_exerionkRomInfo, MSX_exerionkRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Exerion II - Zorni (Jpn)

static struct BurnRomInfo MSX_exerion2RomDesc[] = {
	{ "exerion ii - zorni (japan).rom",	0x04000, 0x0b6c146f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_exerion2, MSX_exerion2, msx_msx)
STD_ROM_FN(MSX_exerion2)

struct BurnDriver BurnDrvMSX_exerion2 = {
	"msx_exerion2", NULL, "msx_msx", NULL, "1985",
	"Exerion II - Zorni (Jpn)\0", NULL, "Toshiba EMI", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_exerion2RomInfo, MSX_exerion2RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Exerion II - Zorni (Jpn, Alt)

static struct BurnRomInfo MSX_exerion2aRomDesc[] = {
	{ "exerion ii - zorni (japan) (alt 1).rom",	0x04000, 0xf3144243, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_exerion2a, MSX_exerion2a, msx_msx)
STD_ROM_FN(MSX_exerion2a)

struct BurnDriver BurnDrvMSX_exerion2a = {
	"msx_exerion2a", "msx_exerion2", "msx_msx", NULL, "1985",
	"Exerion II - Zorni (Jpn, Alt)\0", NULL, "Toshiba EMI", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_exerion2aRomInfo, MSX_exerion2aRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Exoide-Z (Jpn)

static struct BurnRomInfo MSX_exoidezRomDesc[] = {
	{ "exoide-z (japan).rom",	0x04000, 0x6e19c254, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_exoidez, MSX_exoidez, msx_msx)
STD_ROM_FN(MSX_exoidez)

struct BurnDriver BurnDrvMSX_exoidez = {
	"msx_exoidez", NULL, "msx_msx", NULL, "1986",
	"Exoide-Z (Jpn)\0", NULL, "Casio", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_exoidezRomInfo, MSX_exoidezRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Exoide-Z (Jpn, Alt)

static struct BurnRomInfo MSX_exoidezaRomDesc[] = {
	{ "exoide-z (japan) (alt 1).rom",	0x04000, 0x0c7fb621, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_exoideza, MSX_exoideza, msx_msx)
STD_ROM_FN(MSX_exoideza)

struct BurnDriver BurnDrvMSX_exoideza = {
	"msx_exoideza", "msx_exoidez", "msx_msx", NULL, "1986",
	"Exoide-Z (Jpn, Alt)\0", NULL, "Casio", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_exoidezaRomInfo, MSX_exoidezaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Exoide-Z (Jpn, Alt 2)

static struct BurnRomInfo MSX_exoidezbRomDesc[] = {
	{ "exoide-z (japan) (alt 2).rom",	0x04000, 0x2d97d2bd, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_exoidezb, MSX_exoidezb, msx_msx)
STD_ROM_FN(MSX_exoidezb)

struct BurnDriver BurnDrvMSX_exoidezb = {
	"msx_exoidezb", "msx_exoidez", "msx_msx", NULL, "1986",
	"Exoide-Z (Jpn, Alt 2)\0", NULL, "Casio", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_exoidezbRomInfo, MSX_exoidezbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Exoide-Z Area 5 (Jpn)

static struct BurnRomInfo MSX_exoidez5RomDesc[] = {
	{ "exoide-z area 5 (japan).rom",	0x08000, 0xad529df0, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_exoidez5, MSX_exoidez5, msx_msx)
STD_ROM_FN(MSX_exoidez5)

struct BurnDriver BurnDrvMSX_exoidez5 = {
	"msx_exoidez5", NULL, "msx_msx", NULL, "1986",
	"Exoide-Z Area 5 (Jpn)\0", NULL, "Casio", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_exoidez5RomInfo, MSX_exoidez5RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Exoide-Z Area 5 (Kor)

static struct BurnRomInfo MSX_exoidez5kRomDesc[] = {
	{ "exoidez.rom",	0x08000, 0xb0d63c50, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_exoidez5k, MSX_exoidez5k, msx_msx)
STD_ROM_FN(MSX_exoidez5k)

struct BurnDriver BurnDrvMSX_exoidez5k = {
	"msx_exoidez5k", "msx_exoidez5", "msx_msx", NULL, "198?",
	"Exoide-Z Area 5 (Kor)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_exoidez5kRomInfo, MSX_exoidez5kRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// F-1 Spirit - The Way to Formula-1 (Jpn)

static struct BurnRomInfo MSX_f1spiritRomDesc[] = {
	{ "f-1 spirit - the way to formula-1 (japan).rom",	0x20000, 0x12bfd3a9, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_f1spirit, MSX_f1spirit, msx_msx)
STD_ROM_FN(MSX_f1spirit)

struct BurnDriver BurnDrvMSX_f1spirit = {
	"msx_f1spirit", NULL, "msx_msx", NULL, "1987",
	"F-1 Spirit - The Way to Formula-1 (Jpn)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_KONAMI_SCC, GBF_MISC, 0,
	MSXGetZipName, MSX_f1spiritRomInfo, MSX_f1spiritRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// F-1 Spirit - The Way to Formula-1 (Jpn, Alt)

static struct BurnRomInfo MSX_f1spiritaRomDesc[] = {
	{ "f-1 spirit - the way to formula-1 (japan) (alt 1).rom",	0x20000, 0x64d2df7c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_f1spirita, MSX_f1spirita, msx_msx)
STD_ROM_FN(MSX_f1spirita)

struct BurnDriver BurnDrvMSX_f1spirita = {
	"msx_f1spirita", "msx_f1spirit", "msx_msx", NULL, "1987",
	"F-1 Spirit - The Way to Formula-1 (Jpn, Alt)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_KONAMI_SCC, GBF_MISC, 0,
	MSXGetZipName, MSX_f1spiritaRomInfo, MSX_f1spiritaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// F-1 Spirit - The Way to Formula-1 (Kor)

static struct BurnRomInfo MSX_f1spiritkRomDesc[] = {
	{ "f-1 spirit - the way to formula 1 (1987)(zemina)[rc-752].rom",	0x20000, 0xf5a3b765, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_f1spiritk, MSX_f1spiritk, msx_msx)
STD_ROM_FN(MSX_f1spiritk)

struct BurnDriver BurnDrvMSX_f1spiritk = {
	"msx_f1spiritk", "msx_f1spirit", "msx_msx", NULL, "19??",
	"F-1 Spirit - The Way to Formula-1 (Kor)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_KONAMI_SCC, GBF_MISC, 0,
	MSXGetZipName, MSX_f1spiritkRomInfo, MSX_f1spiritkRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// FA Tetris (Kor)

static struct BurnRomInfo MSX_fatetrisRomDesc[] = {
	{ "fa tetris (korea) (unl).rom",	0x08000, 0x1ec87e3a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_fatetris, MSX_fatetris, msx_msx)
STD_ROM_FN(MSX_fatetris)

struct BurnDriver BurnDrvMSX_fatetris = {
	"msx_fatetris", NULL, "msx_msx", NULL, "1989",
	"FA Tetris (Kor)\0", NULL, "FA Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_fatetrisRomInfo, MSX_fatetrisRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// FA Tetris (Kor, Alt)

static struct BurnRomInfo MSX_fatetrisaRomDesc[] = {
	{ "tetrisb.rom",	0x08000, 0x4c6df534, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_fatetrisa, MSX_fatetrisa, msx_msx)
STD_ROM_FN(MSX_fatetrisa)

struct BurnDriver BurnDrvMSX_fatetrisa = {
	"msx_fatetrisa", "msx_fatetris", "msx_msx", NULL, "1989",
	"FA Tetris (Kor, Alt)\0", NULL, "FA Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_fatetrisaRomInfo, MSX_fatetrisaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// FA Tetris (Kor, Alt 2)

static struct BurnRomInfo MSX_fatetrisbRomDesc[] = {
	{ "tetris_n.rom",	0x08000, 0x30151594, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_fatetrisb, MSX_fatetrisb, msx_msx)
STD_ROM_FN(MSX_fatetrisb)

struct BurnDriver BurnDrvMSX_fatetrisb = {
	"msx_fatetrisb", "msx_fatetris", "msx_msx", NULL, "1989",
	"FA Tetris (Kor, Alt 2)\0", NULL, "FA Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_fatetrisbRomInfo, MSX_fatetrisbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Fairy (Jpn)

static struct BurnRomInfo MSX_fairyRomDesc[] = {
	{ "fairy (japan).rom",	0x04000, 0x314728c3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_fairy, MSX_fairy, msx_msx)
STD_ROM_FN(MSX_fairy)

struct BurnDriver BurnDrvMSX_fairy = {
	"msx_fairy", NULL, "msx_msx", NULL, "1985",
	"Fairy (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_fairyRomInfo, MSX_fairyRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// The Fairyland Story (Jpn)

static struct BurnRomInfo MSX_flstoryRomDesc[] = {
	{ "fairy land story, the (japan).rom",	0x20000, 0x26e34f05, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_flstory, MSX_flstory, msx_msx)
STD_ROM_FN(MSX_flstory)

struct BurnDriver BurnDrvMSX_flstory = {
	"msx_flstory", NULL, "msx_msx", NULL, "1987",
	"The Fairyland Story (Jpn)\0", NULL, "Hot-B", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII8, GBF_MISC, 0,
	MSXGetZipName, MSX_flstoryRomInfo, MSX_flstoryRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// The Fairyland Story (Jpn, Alt)

static struct BurnRomInfo MSX_flstoryaRomDesc[] = {
	{ "fairy land story, the (japan) (alt 1).rom",	0x20000, 0xc032376b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_flstorya, MSX_flstorya, msx_msx)
STD_ROM_FN(MSX_flstorya)

struct BurnDriver BurnDrvMSX_flstorya = {
	"msx_flstorya", "msx_flstory", "msx_msx", NULL, "1987",
	"The Fairyland Story (Jpn, Alt)\0", NULL, "Hot-B", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII8, GBF_MISC, 0,
	MSXGetZipName, MSX_flstoryaRomInfo, MSX_flstoryaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Fantasy Zone (Kor)

static struct BurnRomInfo MSX_fantzonekRomDesc[] = {
	{ "fantasy zone (1987)(zemina).rom",	0x20000, 0xc73d4d25, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_fantzonek, MSX_fantzonek, msx_msx)
STD_ROM_FN(MSX_fantzonek)

struct BurnDriver BurnDrvMSX_fantzonek = {
	"msx_fantzonek", "msx_fantzone", "msx_msx", NULL, "19??",
	"Fantasy Zone (Kor)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_KONAMI, GBF_MISC, 0,
	MSXGetZipName, MSX_fantzonekRomInfo, MSX_fantzonekRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Fathom (Jpn)

static struct BurnRomInfo MSX_fathomRomDesc[] = {
	{ "fathom (japan).rom",	0x08000, 0xf06a58da, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_fathom, MSX_fathom, msx_msx)
STD_ROM_FN(MSX_fathom)

struct BurnDriver BurnDrvMSX_fathom = {
	"msx_fathom", NULL, "msx_msx", NULL, "1985",
	"Fathom (Jpn)\0", NULL, "Toshiba EMI", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_fathomRomInfo, MSX_fathomRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Final Justice (Jpn)

static struct BurnRomInfo MSX_fjusticeRomDesc[] = {
	{ "final justice (japan).rom",	0x04000, 0x851ba4bb, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_fjustice, MSX_fjustice, msx_msx)
STD_ROM_FN(MSX_fjustice)

struct BurnDriver BurnDrvMSX_fjustice = {
	"msx_fjustice", NULL, "msx_msx", NULL, "1985",
	"Final Justice (Jpn)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_fjusticeRomInfo, MSX_fjusticeRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Final Justice (Jpn, Alt)

static struct BurnRomInfo MSX_fjusticeaRomDesc[] = {
	{ "final justice (japan) (alt 1).rom",	0x04000, 0x41a86301, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_fjusticea, MSX_fjusticea, msx_msx)
STD_ROM_FN(MSX_fjusticea)

struct BurnDriver BurnDrvMSX_fjusticea = {
	"msx_fjusticea", "msx_fjustice", "msx_msx", NULL, "1985",
	"Final Justice (Jpn, Alt)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_fjusticeaRomInfo, MSX_fjusticeaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Final Justice (Jpn, Alt 2)

static struct BurnRomInfo MSX_fjusticebRomDesc[] = {
	{ "final justice (japan) (alt 2).rom",	0x04000, 0xb1663de8, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_fjusticeb, MSX_fjusticeb, msx_msx)
STD_ROM_FN(MSX_fjusticeb)

struct BurnDriver BurnDrvMSX_fjusticeb = {
	"msx_fjusticeb", "msx_fjustice", "msx_msx", NULL, "1985",
	"Final Justice (Jpn, Alt 2)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_fjusticebRomInfo, MSX_fjusticebRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Final Zone (Kor)

static struct BurnRomInfo MSX_fzonekRomDesc[] = {
	{ "final zone wolf (1986)(zemina).rom",	0x20000, 0x06310fb1, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_fzonek, MSX_fzonek, msx_msx)
STD_ROM_FN(MSX_fzonek)

struct BurnDriver BurnDrvMSX_fzonek = {
	"msx_fzonek", "msx_fzone", "msx_msx", NULL, "19??",
	"Final Zone (Kor)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_KONAMI, GBF_MISC, 0,
	MSXGetZipName, MSX_fzonekRomInfo, MSX_fzonekRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Fire Rescue (Jpn)

static struct BurnRomInfo MSX_firerescRomDesc[] = {
	{ "fire rescue (japan).rom",	0x04000, 0x8005a9ba, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_fireresc, MSX_fireresc, msx_msx)
STD_ROM_FN(MSX_fireresc)

struct BurnDriver BurnDrvMSX_fireresc = {
	"msx_fireresc", NULL, "msx_msx", NULL, "1984",
	"Fire Rescue (Jpn)\0", NULL, "Hudson Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_firerescRomInfo, MSX_firerescRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Fire Rescue (Jpn, Alt)

static struct BurnRomInfo MSX_firerescaRomDesc[] = {
	{ "fire rescue (japan) (alt 1).rom",	0x04000, 0x7b2ef621, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_fireresca, MSX_fireresca, msx_msx)
STD_ROM_FN(MSX_fireresca)

struct BurnDriver BurnDrvMSX_fireresca = {
	"msx_fireresca", "msx_fireresc", "msx_msx", NULL, "1984",
	"Fire Rescue (Jpn, Alt)\0", NULL, "Hudson Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_firerescaRomInfo, MSX_firerescaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Flappy (Jpn)

static struct BurnRomInfo MSX_flappyRomDesc[] = {
	{ "flappy (japan).rom",	0x04000, 0xb6285a0b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_flappy, MSX_flappy, msx_msx)
STD_ROM_FN(MSX_flappy)

struct BurnDriver BurnDrvMSX_flappy = {
	"msx_flappy", NULL, "msx_msx", NULL, "1985",
	"Flappy (Jpn)\0", NULL, "dB-Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_flappyRomInfo, MSX_flappyRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Flappy (Jpn, Alt)

static struct BurnRomInfo MSX_flappyaRomDesc[] = {
	{ "flappy (japan) (alt 1).rom",	0x04000, 0x56ec7bbf, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_flappya, MSX_flappya, msx_msx)
STD_ROM_FN(MSX_flappya)

struct BurnDriver BurnDrvMSX_flappya = {
	"msx_flappya", "msx_flappy", "msx_msx", NULL, "1985",
	"Flappy (Jpn, Alt)\0", NULL, "dB-Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_flappyaRomInfo, MSX_flappyaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Flappy - Limited 85 (Jpn)

static struct BurnRomInfo MSX_flappy85RomDesc[] = {
	{ "flappy - limited 85 (japan).rom",	0x04000, 0x4a4f3084, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_flappy85, MSX_flappy85, msx_msx)
STD_ROM_FN(MSX_flappy85)

struct BurnDriver BurnDrvMSX_flappy85 = {
	"msx_flappy85", NULL, "msx_msx", NULL, "1985",
	"Flappy - Limited 85 (Jpn)\0", NULL, "dB-Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_flappy85RomInfo, MSX_flappy85RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Flashpoint (Kor)

static struct BurnRomInfo MSX_fpointRomDesc[] = {
	{ "flash point (korea) (unl).rom",	0x08000, 0xc4a33da7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_fpoint, MSX_fpoint, msx_msx)
STD_ROM_FN(MSX_fpoint)

struct BurnDriver BurnDrvMSX_fpoint = {
	"msx_fpoint", NULL, "msx_msx", NULL, "1991",
	"Flashpoint (Kor)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_fpointRomInfo, MSX_fpointRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Flashpoint (Kor, Alt)

static struct BurnRomInfo MSX_fpointaRomDesc[] = {
	{ "flash point (korea) (alt 1) (unl).rom",	0x08000, 0x25708cfa, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_fpointa, MSX_fpointa, msx_msx)
STD_ROM_FN(MSX_fpointa)

struct BurnDriver BurnDrvMSX_fpointa = {
	"msx_fpointa", "msx_fpoint", "msx_msx", NULL, "1991",
	"Flashpoint (Kor, Alt)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_fpointaRomInfo, MSX_fpointaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Flash Splash (Jpn)

static struct BurnRomInfo MSX_fsplashRomDesc[] = {
	{ "flash splash (japan).rom",	0x02000, 0x5c187cf7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_fsplash, MSX_fsplash, msx_msx)
STD_ROM_FN(MSX_fsplash)

struct BurnDriver BurnDrvMSX_fsplash = {
	"msx_fsplash", NULL, "msx_msx", NULL, "1984",
	"Flash Splash (Jpn)\0", NULL, "Toshiba EMI", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_fsplashRomInfo, MSX_fsplashRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Flicky (Jpn)

static struct BurnRomInfo MSX_flickyRomDesc[] = {
	{ "flicky (japan).rom",	0x08000, 0x88250e5d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_flicky, MSX_flicky, msx_msx)
STD_ROM_FN(MSX_flicky)

struct BurnDriver BurnDrvMSX_flicky = {
	"msx_flicky", NULL, "msx_msx", NULL, "1986",
	"Flicky (Jpn)\0", NULL, "Micronet", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_flickyRomInfo, MSX_flickyRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Flight Deck (Jpn)

static struct BurnRomInfo MSX_flideckRomDesc[] = {
	{ "flight deck (japan).rom",	0x10000, 0xa00526d0, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_flideck, MSX_flideck, msx_msx)
STD_ROM_FN(MSX_flideck)

struct BurnDriver BurnDrvMSX_flideck = {
	"msx_flideck", NULL, "msx_msx", NULL, "1986",
	"Flight Deck (Jpn)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_flideckRomInfo, MSX_flideckRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Flight Deck (Jpn, Alt)

static struct BurnRomInfo MSX_flideckaRomDesc[] = {
	{ "flight deck (japan) (alt 1).rom",	0x10000, 0x4b162065, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_flidecka, MSX_flidecka, msx_msx)
STD_ROM_FN(MSX_flidecka)

struct BurnDriver BurnDrvMSX_flidecka = {
	"msx_flidecka", "msx_flideck", "msx_msx", NULL, "1986",
	"Flight Deck (Jpn, Alt)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_flideckaRomInfo, MSX_flideckaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Flipper Slipper (Jpn)

static struct BurnRomInfo MSX_flipperRomDesc[] = {
	{ "flipper slipper (japan).rom",	0x04000, 0xfd7de91e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_flipper, MSX_flipper, msx_msx)
STD_ROM_FN(MSX_flipper)

struct BurnDriver BurnDrvMSX_flipper = {
	"msx_flipper", NULL, "msx_msx", NULL, "1983",
	"Flipper Slipper (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_flipperRomInfo, MSX_flipperRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Flipper Slipper (Jpn, Alt)

static struct BurnRomInfo MSX_flipperaRomDesc[] = {
	{ "flipper slipper (japan) (alt 1).rom",	0x04000, 0xf0b5fe8d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_flippera, MSX_flippera, msx_msx)
STD_ROM_FN(MSX_flippera)

struct BurnDriver BurnDrvMSX_flippera = {
	"msx_flippera", "msx_flipper", "msx_msx", NULL, "1983",
	"Flipper Slipper (Jpn, Alt)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_flipperaRomInfo, MSX_flipperaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Formation Z (Jpn)

static struct BurnRomInfo MSX_formatzRomDesc[] = {
	{ "formation z (japan).rom",	0x08000, 0x37b55d09, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_formatz, MSX_formatz, msx_msx)
STD_ROM_FN(MSX_formatz)

struct BurnDriver BurnDrvMSX_formatz = {
	"msx_formatz", NULL, "msx_msx", NULL, "1985",
	"Formation Z (Jpn)\0", NULL, "Nihon Dexter", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_formatzRomInfo, MSX_formatzRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Formation Z (Jpn, Alt)

static struct BurnRomInfo MSX_formatzaRomDesc[] = {
	{ "formation z (japan) (alt 1).rom",	0x08000, 0x74560bbd, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_formatza, MSX_formatza, msx_msx)
STD_ROM_FN(MSX_formatza)

struct BurnDriver BurnDrvMSX_formatza = {
	"msx_formatza", "msx_formatz", "msx_msx", NULL, "1985",
	"Formation Z (Jpn, Alt)\0", NULL, "Nihon Dexter", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_formatzaRomInfo, MSX_formatzaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Frogger (Jpn)

static struct BurnRomInfo MSX_froggerRomDesc[] = {
	{ "frogger (japan).rom",	0x02000, 0x97e2fcb4, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_frogger, MSX_frogger, msx_msx)
STD_ROM_FN(MSX_frogger)

struct BurnDriver BurnDrvMSX_frogger = {
	"msx_frogger", NULL, "msx_msx", NULL, "1983",
	"Frogger (Jpn)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_froggerRomInfo, MSX_froggerRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Frogger (Jpn, Alt)

static struct BurnRomInfo MSX_froggeraRomDesc[] = {
	{ "frogger (japan) (alt 1).rom",	0x04000, 0x71edc580, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_froggera, MSX_froggera, msx_msx)
STD_ROM_FN(MSX_froggera)

struct BurnDriver BurnDrvMSX_froggera = {
	"msx_froggera", "msx_frogger", "msx_msx", NULL, "1983",
	"Frogger (Jpn, Alt)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_froggeraRomInfo, MSX_froggeraRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Front Line (Jpn)

static struct BurnRomInfo MSX_frontlinRomDesc[] = {
	{ "front line (japan).rom",	0x04000, 0xb9d03f7b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_frontlin, MSX_frontlin, msx_msx)
STD_ROM_FN(MSX_frontlin)

struct BurnDriver BurnDrvMSX_frontlin = {
	"msx_frontlin", NULL, "msx_msx", NULL, "1984",
	"Front Line (Jpn)\0", NULL, "Taito", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_frontlinRomInfo, MSX_frontlinRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Front Line (Jpn, Alt)

static struct BurnRomInfo MSX_frontlinaRomDesc[] = {
	{ "front line (japan) (alt 1).rom",	0x04000, 0x8d632577, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_frontlina, MSX_frontlina, msx_msx)
STD_ROM_FN(MSX_frontlina)

struct BurnDriver BurnDrvMSX_frontlina = {
	"msx_frontlina", "msx_frontlin", "msx_msx", NULL, "1984",
	"Front Line (Jpn, Alt)\0", NULL, "Taito", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_frontlinaRomInfo, MSX_frontlinaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Front Line (Jpn, Alt 2)

static struct BurnRomInfo MSX_frontlinbRomDesc[] = {
	{ "front line (japan) (alt 2).rom",	0x04000, 0x5958f98c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_frontlinb, MSX_frontlinb, msx_msx)
STD_ROM_FN(MSX_frontlinb)

struct BurnDriver BurnDrvMSX_frontlinb = {
	"msx_frontlinb", "msx_frontlin", "msx_msx", NULL, "1984",
	"Front Line (Jpn, Alt 2)\0", NULL, "Taito", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_frontlinbRomInfo, MSX_frontlinbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Fruit Search (Jpn)

static struct BurnRomInfo MSX_fruitsrcRomDesc[] = {
	{ "fruit search (japan).rom",	0x02000, 0xeba95a38, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_fruitsrc, MSX_fruitsrc, msx_msx)
STD_ROM_FN(MSX_fruitsrc)

struct BurnDriver BurnDrvMSX_fruitsrc = {
	"msx_fruitsrc", NULL, "msx_msx", NULL, "1983",
	"Fruit Search (Jpn)\0", NULL, "HAL Kenkyuujo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_fruitsrcRomInfo, MSX_fruitsrcRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Fruit Search (Jpn, Alt)

static struct BurnRomInfo MSX_fruitsrcaRomDesc[] = {
	{ "fruit search (japan) (alt 1).rom",	0x04000, 0xb1160421, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_fruitsrca, MSX_fruitsrca, msx_msx)
STD_ROM_FN(MSX_fruitsrca)

struct BurnDriver BurnDrvMSX_fruitsrca = {
	"msx_fruitsrca", "msx_fruitsrc", "msx_msx", NULL, "1983",
	"Fruit Search (Jpn, Alt)\0", NULL, "HAL Kenkyuujo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_fruitsrcaRomInfo, MSX_fruitsrcaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Funky Mouse (Jpn)

static struct BurnRomInfo MSX_funmouseRomDesc[] = {
	{ "funky mouse (japan).rom",	0x04000, 0xb5c0dace, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_funmouse, MSX_funmouse, msx_msx)
STD_ROM_FN(MSX_funmouse)

struct BurnDriver BurnDrvMSX_funmouse = {
	"msx_funmouse", NULL, "msx_msx", NULL, "1984",
	"Funky Mouse (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_funmouseRomInfo, MSX_funmouseRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Galaga (Jpn)

static struct BurnRomInfo MSX_galagaRomDesc[] = {
	{ "galaga (japan).rom",	0x08000, 0x8856961d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_galaga, MSX_galaga, msx_msx)
STD_ROM_FN(MSX_galaga)

struct BurnDriver BurnDrvMSX_galaga = {
	"msx_galaga", NULL, "msx_msx", NULL, "1984",
	"Galaga (Jpn)\0", NULL, "Namcot", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_galagaRomInfo, MSX_galagaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Galaxian (Jpn)

static struct BurnRomInfo MSX_galaxianRomDesc[] = {
	{ "galaxian (japan).rom",	0x02000, 0xe223ffd1, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_galaxian, MSX_galaxian, msx_msx)
STD_ROM_FN(MSX_galaxian)

struct BurnDriver BurnDrvMSX_galaxian = {
	"msx_galaxian", NULL, "msx_msx", NULL, "1984",
	"Galaxian (Jpn)\0", NULL, "Namcot", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_galaxianRomInfo, MSX_galaxianRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Galaxian (Jpn, Alt)

static struct BurnRomInfo MSX_galaxianaRomDesc[] = {
	{ "galaxian (japan) (alt 1).rom",	0x02000, 0x4980ffac, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_galaxiana, MSX_galaxiana, msx_msx)
STD_ROM_FN(MSX_galaxiana)

struct BurnDriver BurnDrvMSX_galaxiana = {
	"msx_galaxiana", "msx_galaxian", "msx_msx", NULL, "1984",
	"Galaxian (Jpn, Alt)\0", NULL, "Namcot", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_galaxianaRomInfo, MSX_galaxianaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Galaxian (Jpn, Alt 2)

static struct BurnRomInfo MSX_galaxianbRomDesc[] = {
	{ "galaxian (japan) (alt 2).rom",	0x02000, 0xe6f9d8a7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_galaxianb, MSX_galaxianb, msx_msx)
STD_ROM_FN(MSX_galaxianb)

struct BurnDriver BurnDrvMSX_galaxianb = {
	"msx_galaxianb", "msx_galaxian", "msx_msx", NULL, "1984",
	"Galaxian (Jpn, Alt 2)\0", NULL, "Namcot", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_galaxianbRomInfo, MSX_galaxianbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Gall Force - Defense of Chaos (Jpn)

static struct BurnRomInfo MSX_galforceRomDesc[] = {
	{ "gall force - defense of chaos (japan).rom",	0x20000, 0xf65b5271, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_galforce, MSX_galforce, msx_msx)
STD_ROM_FN(MSX_galforce)

struct BurnDriver BurnDrvMSX_galforce = {
	"msx_galforce", NULL, "msx_msx", NULL, "1986",
	"Gall Force - Defense of Chaos (Jpn)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII16, GBF_MISC, 0,
	MSXGetZipName, MSX_galforceRomInfo, MSX_galforceRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Gall Force - Defense of Chaos (Jpn, Alt)

static struct BurnRomInfo MSX_galforcebRomDesc[] = {
	{ "gall force - defense of chaos (japan) (alt).rom",	0x20000, 0xec036e37, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_galforceb, MSX_galforceb, msx_msx)
STD_ROM_FN(MSX_galforceb)

struct BurnDriver BurnDrvMSX_galforceb = {
	"msx_galforcea", "msx_galforce", "msx_msx", NULL, "1986",
	"Gall Force - Defense of Chaos (Jpn, Alt)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII16, GBF_MISC, 0,
	MSXGetZipName, MSX_galforcebRomInfo, MSX_galforcebRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Ghostbusters (Jpn)

static struct BurnRomInfo MSX_ghostbstRomDesc[] = {
	{ "ghostbusters (europe).rom",	0x08000, 0xc9bcbe5a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_ghostbst, MSX_ghostbst, msx_msx)
STD_ROM_FN(MSX_ghostbst)

struct BurnDriver BurnDrvMSX_ghostbst = {
	"msx_ghostbst", NULL, "msx_msx", NULL, "1984",
	"Ghostbusters (Jpn)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_ghostbstRomInfo, MSX_ghostbstRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Glider (Jpn)

static struct BurnRomInfo MSX_gliderRomDesc[] = {
	{ "glider (japan).rom",	0x04000, 0xae35e4ad, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_glider, MSX_glider, msx_msx)
STD_ROM_FN(MSX_glider)

struct BurnDriver BurnDrvMSX_glider = {
	"msx_glider", NULL, "msx_msx", NULL, "1985",
	"Glider (Jpn)\0", NULL, "ZAP", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_gliderRomInfo, MSX_gliderRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Glider (Jpn, Alt)

static struct BurnRomInfo MSX_glideraRomDesc[] = {
	{ "glider (japan) (alt 1).rom",	0x04000, 0x5a23f1ee, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_glidera, MSX_glidera, msx_msx)
STD_ROM_FN(MSX_glidera)

struct BurnDriver BurnDrvMSX_glidera = {
	"msx_glidera", "msx_glider", "msx_msx", NULL, "1985",
	"Glider (Jpn, Alt)\0", NULL, "ZAP", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_glideraRomInfo, MSX_glideraRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Godzilla (Jpn)

static struct BurnRomInfo MSX_godzillaRomDesc[] = {
	{ "godzilla (japan).rom",	0x04000, 0xbe071826, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_godzilla, MSX_godzilla, msx_msx)
STD_ROM_FN(MSX_godzilla)

struct BurnDriver BurnDrvMSX_godzilla = {
	"msx_godzilla", NULL, "msx_msx", NULL, "1984",
	"Godzilla (Jpn)\0", NULL, "Bandai", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_godzillaRomInfo, MSX_godzillaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Godzilla (Jpn, Hacked?)

static struct BurnRomInfo MSX_godzillaaRomDesc[] = {
	{ "godzilla (japan) (alt 1).rom",	0x04000, 0x83d44b03, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_godzillaa, MSX_godzillaa, msx_msx)
STD_ROM_FN(MSX_godzillaa)

struct BurnDriver BurnDrvMSX_godzillaa = {
	"msx_godzillaa", "msx_godzilla", "msx_msx", NULL, "1984",
	"Godzilla (Jpn, Hacked?)\0", NULL, "Bandai", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_godzillaaRomInfo, MSX_godzillaaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Godzilla-kun (Jpn)

static struct BurnRomInfo MSX_godzikunRomDesc[] = {
	{ "godzilla-kun (japan).rom",	0x08000, 0xed4a211d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_godzikun, MSX_godzikun, msx_msx)
STD_ROM_FN(MSX_godzikun)

struct BurnDriver BurnDrvMSX_godzikun = {
	"msx_godzikun", NULL, "msx_msx", NULL, "1985",
	"Godzilla-kun (Jpn)\0", NULL, "Toho", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_godzikunRomInfo, MSX_godzikunRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Godzilla-kun (Jpn, Alt)

static struct BurnRomInfo MSX_godzikunaRomDesc[] = {
	{ "godzilla-kun (japan) (alt 1).rom",	0x08000, 0x6498865f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_godzikuna, MSX_godzikuna, msx_msx)
STD_ROM_FN(MSX_godzikuna)

struct BurnDriver BurnDrvMSX_godzikuna = {
	"msx_godzikuna", "msx_godzikun", "msx_msx", NULL, "1985",
	"Godzilla-kun (Jpn, Alt)\0", NULL, "Toho", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_godzikunaRomInfo, MSX_godzikunaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Gokiburi Daisakusen (Jpn)

static struct BurnRomInfo MSX_gokiburiRomDesc[] = {
	{ "gokiburi daisakusen - bug bomb (japan).rom",	0x04000, 0x69ecb2ed, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_gokiburi, MSX_gokiburi, msx_msx)
STD_ROM_FN(MSX_gokiburi)

struct BurnDriver BurnDrvMSX_gokiburi = {
	"msx_gokiburi", NULL, "msx_msx", NULL, "1983",
	"Gokiburi Daisakusen. Bug Bomb (Jpn)\0", NULL, "Magicsoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_gokiburiRomInfo, MSX_gokiburiRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Golf Game (Jpn)

static struct BurnRomInfo MSX_golfgameRomDesc[] = {
	{ "golf game (japan).rom",	0x04000, 0x5d88275f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_golfgame, MSX_golfgame, msx_msx)
STD_ROM_FN(MSX_golfgame)

struct BurnDriver BurnDrvMSX_golfgame = {
	"msx_golfgame", NULL, "msx_msx", NULL, "1983",
	"Golf Game (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_BASIC, GBF_MISC, 0,
	MSXGetZipName, MSX_golfgameRomInfo, MSX_golfgameRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Gomoku Narabe (Jpn)

static struct BurnRomInfo MSX_gomokunaRomDesc[] = {
	{ "gomok narabe - omo go (japan).rom",	0x04000, 0x269f079f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_gomokuna, MSX_gomokuna, msx_msx)
STD_ROM_FN(MSX_gomokuna)

struct BurnDriver BurnDrvMSX_gomokuna = {
	"msx_gomokuna", NULL, "msx_msx", NULL, "1984",
	"Gomoku Narabe (Jpn)\0", NULL, "Toshiba EMI", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_gomokunaRomInfo, MSX_gomokunaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Gomoku Narabe (Jpn, Alt)

static struct BurnRomInfo MSX_gomokunaaRomDesc[] = {
	{ "gomok narabe - omo go (japan) (alt 1).rom",	0x04000, 0xc3816d36, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_gomokunaa, MSX_gomokunaa, msx_msx)
STD_ROM_FN(MSX_gomokunaa)

struct BurnDriver BurnDrvMSX_gomokunaa = {
	"msx_gomokunaa", "msx_gomokuna", "msx_msx", NULL, "1984",
	"Gomoku Narabe (Jpn, Alt)\0", NULL, "Toshiba EMI", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_gomokunaaRomInfo, MSX_gomokunaaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// The Goonies (Jpn)

static struct BurnRomInfo MSX_gooniesRomDesc[] = {
	{ "goonies, the (japan).rom",	0x08000, 0xdb327847, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_goonies, MSX_goonies, msx_msx)
STD_ROM_FN(MSX_goonies)

struct BurnDriver BurnDrvMSX_goonies = {
	"msx_goonies", NULL, "msx_msx", NULL, "1986",
	"The Goonies (Jpn)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_gooniesRomInfo, MSX_gooniesRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// The Goonies (Jpn, Alt)

static struct BurnRomInfo MSX_gooniesaRomDesc[] = {
	{ "goonies, the (japan) (alt 1).rom",	0x08000, 0xc6445f82, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_gooniesa, MSX_gooniesa, msx_msx)
STD_ROM_FN(MSX_gooniesa)

struct BurnDriver BurnDrvMSX_gooniesa = {
	"msx_gooniesa", "msx_goonies", "msx_msx", NULL, "1986",
	"The Goonies (Jpn, Alt)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_gooniesaRomInfo, MSX_gooniesaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// The Goonies (Jpn, Alt 2)

static struct BurnRomInfo MSX_gooniesbRomDesc[] = {
	{ "goonies, the (japan) (alt 2).rom",	0x08000, 0x38f04741, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_gooniesb, MSX_gooniesb, msx_msx)
STD_ROM_FN(MSX_gooniesb)

struct BurnDriver BurnDrvMSX_gooniesb = {
	"msx_gooniesb", "msx_goonies", "msx_msx", NULL, "1986",
	"The Goonies (Jpn, Alt 2)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_gooniesbRomInfo, MSX_gooniesbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// GP World (Jpn)

static struct BurnRomInfo MSX_gpworldRomDesc[] = {
	{ "gp world (japan).rom",	0x08000, 0x9dbdd4bc, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_gpworld, MSX_gpworld, msx_msx)
STD_ROM_FN(MSX_gpworld)

struct BurnDriver BurnDrvMSX_gpworld = {
	"msx_gpworld", NULL, "msx_msx", NULL, "1985",
	"GP World (Jpn)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_gpworldRomInfo, MSX_gpworldRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Green Beret (Euro)

static struct BurnRomInfo MSX_gberetRomDesc[] = {
	{ "green beret (europe).rom",	0x08000, 0x61f41bcd, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_gberet, MSX_gberet, msx_msx)
STD_ROM_FN(MSX_gberet)

struct BurnDriver BurnDrvMSX_gberet = {
	"msx_gberet", NULL, "msx_msx", NULL, "1986",
	"Green Beret (Euro)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_gberetRomInfo, MSX_gberetRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Green Beret (Euro, Alt)

static struct BurnRomInfo MSX_gberetaRomDesc[] = {
	{ "green beret (europe) (alt 1).rom",	0x08000, 0x46f5e571, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_gbereta, MSX_gbereta, msx_msx)
STD_ROM_FN(MSX_gbereta)

struct BurnDriver BurnDrvMSX_gbereta = {
	"msx_gbereta", "msx_gberet", "msx_msx", NULL, "1986",
	"Green Beret (Euro, Alt)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_gberetaRomInfo, MSX_gberetaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Green Beret (Kor)

static struct BurnRomInfo MSX_gberetkRomDesc[] = {
	{ "green beret (1987)(zemina).rom",	0x08000, 0x3caec828, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_gberetk, MSX_gberetk, msx_msx)
STD_ROM_FN(MSX_gberetk)

struct BurnDriver BurnDrvMSX_gberetk = {
	"msx_gberetk", "msx_gberet", "msx_msx", NULL, "1987",
	"Green Beret (Kor)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_gberetkRomInfo, MSX_gberetkRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Grog's Revenge (Jpn)

static struct BurnRomInfo MSX_grogrevRomDesc[] = {
	{ "grog's revenge (japan).rom",	0x08000, 0x5f74ae0e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_grogrev, MSX_grogrev, msx_msx)
STD_ROM_FN(MSX_grogrev)

struct BurnDriver BurnDrvMSX_grogrev = {
	"msx_grogrev", NULL, "msx_msx", NULL, "1985",
	"B.C.'s Quest for Tires II - Grog's Revenge (Jpn)\0", NULL, "Comptiq", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_grogrevRomInfo, MSX_grogrevRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Grog's Revenge (Jpn, Alt)

static struct BurnRomInfo MSX_grogrevaRomDesc[] = {
	{ "grog's revenge (japan) (alt 1).rom",	0x08000, 0xeba19b7e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_grogreva, MSX_grogreva, msx_msx)
STD_ROM_FN(MSX_grogreva)

struct BurnDriver BurnDrvMSX_grogreva = {
	"msx_grogreva", "msx_grogrev", "msx_msx", NULL, "1985",
	"B.C.'s Quest for Tires II - Grog's Revenge (Jpn, Alt)\0", NULL, "Comptiq", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_grogrevaRomInfo, MSX_grogrevaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Guardic (Jpn)

static struct BurnRomInfo MSX_guardicRomDesc[] = {
	{ "guardic (japan).rom",	0x08000, 0x6aebb9d3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_guardic, MSX_guardic, msx_msx)
STD_ROM_FN(MSX_guardic)

struct BurnDriver BurnDrvMSX_guardic = {
	"msx_guardic", NULL, "msx_msx", NULL, "1986",
	"Guardic (Jpn)\0", NULL, "Compile", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_guardicRomInfo, MSX_guardicRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Guardic (Jpn, Alt)

static struct BurnRomInfo MSX_guardicaRomDesc[] = {
	{ "guardic (japan) (alt 1).rom",	0x08000, 0x106230b8, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_guardica, MSX_guardica, msx_msx)
STD_ROM_FN(MSX_guardica)

struct BurnDriver BurnDrvMSX_guardica = {
	"msx_guardica", "msx_guardic", "msx_msx", NULL, "1986",
	"Guardic (Jpn, Alt)\0", NULL, "Compile", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_guardicaRomInfo, MSX_guardicaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Gulkave (Jpn)

static struct BurnRomInfo MSX_gulkaveRomDesc[] = {
	{ "gulkave (japan).rom",	0x08000, 0xa02029d0, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_gulkave, MSX_gulkave, msx_msx)
STD_ROM_FN(MSX_gulkave)

struct BurnDriver BurnDrvMSX_gulkave = {
	"msx_gulkave", NULL, "msx_msx", NULL, "1986",
	"Gulkave (Jpn)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_gulkaveRomInfo, MSX_gulkaveRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Gulkave (Jpn, Alt)

static struct BurnRomInfo MSX_gulkaveaRomDesc[] = {
	{ "gulkave (japan) (alt 1).rom",	0x08000, 0xfdb3bc27, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_gulkavea, MSX_gulkavea, msx_msx)
STD_ROM_FN(MSX_gulkavea)

struct BurnDriver BurnDrvMSX_gulkavea = {
	"msx_gulkavea", "msx_gulkave", "msx_msx", NULL, "1986",
	"Gulkave (Jpn, Alt)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_gulkaveaRomInfo, MSX_gulkaveaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Gulkave (Kor)

static struct BurnRomInfo MSX_gulkavekRomDesc[] = {
	{ "gulkave.rom",	0x08000, 0x0bee5a87, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_gulkavek, MSX_gulkavek, msx_msx)
STD_ROM_FN(MSX_gulkavek)

struct BurnDriver BurnDrvMSX_gulkavek = {
	"msx_gulkavek", "msx_gulkave", "msx_msx", NULL, "198?",
	"Gulkave (Kor)\0", NULL, "ProSoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_gulkavekRomInfo, MSX_gulkavekRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Gun.Smoke

static struct BurnRomInfo MSX_gunsmokeRomDesc[] = {
	{ "prosoft_gunsmoke_(1990)(prosoft).rom",	0x08000, 0x1318dba7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_gunsmoke, MSX_gunsmoke, msx_msx)
STD_ROM_FN(MSX_gunsmoke)

struct BurnDriver BurnDrvMSX_gunsmoke = {
	"msx_gunsmoke", NULL, "msx_msx", NULL, "1990",
	"Gun.Smoke\0", NULL, "Prosoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_gunsmokeRomInfo, MSX_gunsmokeRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Gun Fright (Jpn)

static struct BurnRomInfo MSX_gunfrghtRomDesc[] = {
	{ "gun fright (japan).rom",	0x08000, 0xf877b3d6, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_gunfrght, MSX_gunfrght, msx_msx)
STD_ROM_FN(MSX_gunfrght)

struct BurnDriver BurnDrvMSX_gunfrght = {
	"msx_gunfrght", NULL, "msx_msx", NULL, "1986",
	"Gun Fright (Jpn)\0", NULL, "Jaleco", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_gunfrghtRomInfo, MSX_gunfrghtRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Gun Fright (Jpn, Hacked?)

static struct BurnRomInfo MSX_gunfrghtaRomDesc[] = {
	{ "gunfright (1986)(ultimate play the game).rom",	0x08000, 0x31fe5c5b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_gunfrghta, MSX_gunfrghta, msx_msx)
STD_ROM_FN(MSX_gunfrghta)

struct BurnDriver BurnDrvMSX_gunfrghta = {
	"msx_gunfrghta", "msx_gunfrght", "msx_msx", NULL, "1985",
	"Gun Fright (Jpn, Hacked?)\0", NULL, "A.C.G.", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_gunfrghtaRomInfo, MSX_gunfrghtaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Gyrodine (Jpn)

static struct BurnRomInfo MSX_gyrodineRomDesc[] = {
	{ "gyrodine (japan).rom",	0x08000, 0x0e89433b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_gyrodine, MSX_gyrodine, msx_msx)
STD_ROM_FN(MSX_gyrodine)

struct BurnDriver BurnDrvMSX_gyrodine = {
	"msx_gyrodine", NULL, "msx_msx", NULL, "1986",
	"Gyrodine (Jpn)\0", NULL, "Nidecom", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_gyrodineRomInfo, MSX_gyrodineRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Gyrodine (Kor)

static struct BurnRomInfo MSX_gyrodinekRomDesc[] = {
	{ "gyrodine.rom",	0x08000, 0x1f10db79, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_gyrodinek, MSX_gyrodinek, msx_msx)
STD_ROM_FN(MSX_gyrodinek)

struct BurnDriver BurnDrvMSX_gyrodinek = {
	"msx_gyrodinek", "msx_gyrodine", "msx_msx", NULL, "198?",
	"Gyrodine (Kor)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_gyrodinekRomInfo, MSX_gyrodinekRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// H.E.R.O. (Jpn)

static struct BurnRomInfo MSX_heroRomDesc[] = {
	{ "h.e.r.o. (japan).rom",	0x04000, 0x97ab0d70, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_hero, MSX_hero, msx_msx)
STD_ROM_FN(MSX_hero)

struct BurnDriver BurnDrvMSX_hero = {
	"msx_hero", NULL, "msx_msx", NULL, "1984",
	"H.E.R.O. (Jpn)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_heroRomInfo, MSX_heroRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Hang-On (Jpn)

static struct BurnRomInfo MSX_hangonRomDesc[] = {
	{ "hang-on (japan).rom",	0x08000, 0x48e7212c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_hangon, MSX_hangon, msx_msx)
STD_ROM_FN(MSX_hangon)

struct BurnDriver BurnDrvMSX_hangon = {
	"msx_hangon", NULL, "msx_msx", NULL, "1986",
	"Hang-On (Jpn)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_hangonRomInfo, MSX_hangonRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Hang-On (Jpn, Alt)

static struct BurnRomInfo MSX_hangonaRomDesc[] = {
	{ "hang-on (japan) (alt 1).rom",	0x08000, 0x1f9bbd9a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_hangona, MSX_hangona, msx_msx)
STD_ROM_FN(MSX_hangona)

struct BurnDriver BurnDrvMSX_hangona = {
	"msx_hangona", "msx_hangon", "msx_msx", NULL, "1986",
	"Hang-On (Jpn, Alt)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_hangonaRomInfo, MSX_hangonaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Hang-On (Jpn, Alt 2)

static struct BurnRomInfo MSX_hangonbRomDesc[] = {
	{ "hang-on (japan) (alt 2).rom",	0x08000, 0x69dc9f85, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_hangonb, MSX_hangonb, msx_msx)
STD_ROM_FN(MSX_hangonb)

struct BurnDriver BurnDrvMSX_hangonb = {
	"msx_hangonb", "msx_hangon", "msx_msx", NULL, "1986",
	"Hang-On (Jpn, Alt 2)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_hangonbRomInfo, MSX_hangonbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Harapeko Pakkun (Jpn)

static struct BurnRomInfo MSX_harapekoRomDesc[] = {
	{ "harapeko pakkun (japan).rom",	0x02000, 0x145bb27b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_harapeko, MSX_harapeko, msx_msx)
STD_ROM_FN(MSX_harapeko)

struct BurnDriver BurnDrvMSX_harapeko = {
	"msx_harapeko", NULL, "msx_msx", NULL, "1984",
	"Harapeko Pakkun (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_harapekoRomInfo, MSX_harapekoRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// The Heist (Jpn)

static struct BurnRomInfo MSX_heistRomDesc[] = {
	{ "heist, the (japan).rom",	0x08000, 0x04e454c5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_heist, MSX_heist, msx_msx)
STD_ROM_FN(MSX_heist)

struct BurnDriver BurnDrvMSX_heist = {
	"msx_heist", NULL, "msx_msx", NULL, "1985",
	"The Heist (Jpn)\0", NULL, "Comptiq", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_heistRomInfo, MSX_heistRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Helitank (Jpn)

static struct BurnRomInfo MSX_helitankRomDesc[] = {
	{ "helitank (japan).rom",	0x04000, 0xcd63cd50, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_helitank, MSX_helitank, msx_msx)
STD_ROM_FN(MSX_helitank)

struct BurnDriver BurnDrvMSX_helitank = {
	"msx_helitank", NULL, "msx_msx", NULL, "1984",
	"Helitank (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_helitankRomInfo, MSX_helitankRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// High Way Star (Jpn)

static struct BurnRomInfo MSX_highwayRomDesc[] = {
	{ "high way star (japan).rom",	0x04000, 0x24851440, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_highway, MSX_highway, msx_msx)
STD_ROM_FN(MSX_highway)

struct BurnDriver BurnDrvMSX_highway = {
	"msx_highway", NULL, "msx_msx", NULL, "1983",
	"High Way Star (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_highwayRomInfo, MSX_highwayRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// High Way Star (Kor)

static struct BurnRomInfo MSX_highwaykRomDesc[] = {
	{ "high way star (korea).rom",	0x04000, 0xed1625d8, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_highwayk, MSX_highwayk, msx_msx)
STD_ROM_FN(MSX_highwayk)

struct BurnDriver BurnDrvMSX_highwayk = {
	"msx_highwayk", "msx_highway", "msx_msx", NULL, "1983",
	"High Way Star (Kor)\0", NULL, "Qnix", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_highwaykRomInfo, MSX_highwaykRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Hitsuji-Ya (Jpn)

static struct BurnRomInfo MSX_hitsujiRomDesc[] = {
	{ "hitsuji yai - preety sheep (japan).rom",	0x02000, 0x11502a96, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_hitsuji, MSX_hitsuji, msx_msx)
STD_ROM_FN(MSX_hitsuji)

struct BurnDriver BurnDrvMSX_hitsuji = {
	"msx_hitsuji", NULL, "msx_msx", NULL, "1984",
	"Hitsuji-Ya (Jpn)\0", NULL, "Hudson Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_hitsujiRomInfo, MSX_hitsujiRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Hole in One (Jpn)

static struct BurnRomInfo MSX_holein1RomDesc[] = {
	{ "hole in one (japan).rom",	0x04000, 0x98e7b01b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_holein1, MSX_holein1, msx_msx)
STD_ROM_FN(MSX_holein1)

struct BurnDriver BurnDrvMSX_holein1 = {
	"msx_holein1", NULL, "msx_msx", NULL, "1984",
	"Hole in One (Jpn)\0", NULL, "HAL Kenkyuujo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_holein1RomInfo, MSX_holein1RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Hole in One (Jpn, Alt)

static struct BurnRomInfo MSX_holein1aRomDesc[] = {
	{ "hole in one (japan) (alt 1).rom",	0x04000, 0x86731751, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_holein1a, MSX_holein1a, msx_msx)
STD_ROM_FN(MSX_holein1a)

struct BurnDriver BurnDrvMSX_holein1a = {
	"msx_holein1a", "msx_holein1", "msx_msx", NULL, "1984",
	"Hole in One (Jpn, Alt)\0", NULL, "HAL Kenkyuujo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_holein1aRomInfo, MSX_holein1aRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Hole in One (Jpn, Alt 2)

static struct BurnRomInfo MSX_holein1bRomDesc[] = {
	{ "hole in one (japan) (alt 2).rom",	0x04000, 0xff063cdb, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_holein1b, MSX_holein1b, msx_msx)
STD_ROM_FN(MSX_holein1b)

struct BurnDriver BurnDrvMSX_holein1b = {
	"msx_holein1b", "msx_holein1", "msx_msx", NULL, "1984",
	"Hole in One (Jpn, Alt 2)\0", NULL, "HAL Kenkyuujo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_holein1bRomInfo, MSX_holein1bRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Hole in One (Jpn, Alt 3)

static struct BurnRomInfo MSX_holein1cRomDesc[] = {
	{ "hole in one (japan) (alt 3).rom",	0x04000, 0x5bd60572, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_holein1c, MSX_holein1c, msx_msx)
STD_ROM_FN(MSX_holein1c)

struct BurnDriver BurnDrvMSX_holein1c = {
	"msx_holein1c", "msx_holein1", "msx_msx", NULL, "1984",
	"Hole in One (Jpn, Alt 3)\0", NULL, "HAL Kenkyuujo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_holein1cRomInfo, MSX_holein1cRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Hole in One Professional (Jpn)

static struct BurnRomInfo MSX_holein1pRomDesc[] = {
	{ "hole in one professional (japan).rom",	0x08000, 0x350ae107, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_holein1p, MSX_holein1p, msx_msx)
STD_ROM_FN(MSX_holein1p)

struct BurnDriver BurnDrvMSX_holein1p = {
	"msx_holein1p", NULL, "msx_msx", NULL, "1985",
	"Hole in One Professional (Jpn)\0", NULL, "HAL Kenkyuujo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_holein1pRomInfo, MSX_holein1pRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Hole in One Professional (Jpn, Alt)

static struct BurnRomInfo MSX_holein1paRomDesc[] = {
	{ "hole in one professional (japan) (alt 1).rom",	0x08000, 0xacfcbba6, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_holein1pa, MSX_holein1pa, msx_msx)
STD_ROM_FN(MSX_holein1pa)

struct BurnDriver BurnDrvMSX_holein1pa = {
	"msx_holein1pa", "msx_holein1p", "msx_msx", NULL, "1985",
	"Hole in One Professional (Jpn, Alt)\0", NULL, "HAL Kenkyuujo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_holein1paRomInfo, MSX_holein1paRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Hopper (Euro)

static struct BurnRomInfo MSX_hopperRomDesc[] = {
	{ "hopper (europe).rom",	0x04000, 0xd3032ad7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_hopper, MSX_hopper, msx_msx)
STD_ROM_FN(MSX_hopper)

struct BurnDriver BurnDrvMSX_hopper = {
	"msx_hopper", NULL, "msx_msx", NULL, "1986",
	"Hopper (Euro)\0", NULL, "Eaglesoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_hopperRomInfo, MSX_hopperRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Hurry Fox MSX Special (Jpn)

static struct BurnRomInfo MSX_hfoxRomDesc[] = {
	{ "harryfox msx special (japan).rom",	0x20000, 0x96b7faca, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_hfox, MSX_hfox, msx_msx)
STD_ROM_FN(MSX_hfox)

struct BurnDriver BurnDrvMSX_hfox = {
	"msx_hfox", NULL, "msx_msx", NULL, "1986",
	"Hurry Fox MSX Special (Jpn)\0", NULL, "MicroCabin", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII16_SRAM, GBF_MISC, 0,
	MSXGetZipName, MSX_hfoxRomInfo, MSX_hfoxRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Hustle! Chumy (Jpn)

static struct BurnRomInfo MSX_hustleRomDesc[] = {
	{ "hustle! chumy (japan).rom",	0x04000, 0x99518a12, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_hustle, MSX_hustle, msx_msx)
STD_ROM_FN(MSX_hustle)

struct BurnDriver BurnDrvMSX_hustle = {
	"msx_hustle", NULL, "msx_msx", NULL, "1984",
	"Hustle! Chumy (Jpn)\0", NULL, "General", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_hustleRomInfo, MSX_hustleRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Hustle! Chumy (Jpn, Alt)

static struct BurnRomInfo MSX_hustleaRomDesc[] = {
	{ "hustle! chumy (japan) (alt 1).rom",	0x04000, 0x22fd4780, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_hustlea, MSX_hustlea, msx_msx)
STD_ROM_FN(MSX_hustlea)

struct BurnDriver BurnDrvMSX_hustlea = {
	"msx_hustlea", "msx_hustle", "msx_msx", NULL, "1984",
	"Hustle! Chumy (Jpn, Alt)\0", NULL, "General", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_hustleaRomInfo, MSX_hustleaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Hydlide (Jpn)

static struct BurnRomInfo MSX_hydlideRomDesc[] = {
	{ "hydlide (japan).rom",	0x08000, 0x0f9d6f56, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_hydlide, MSX_hydlide, msx_msx)
STD_ROM_FN(MSX_hydlide)

struct BurnDriver BurnDrvMSX_hydlide = {
	"msx_hydlide", NULL, "msx_msx", NULL, "1985",
	"Hydlide (Jpn)\0", NULL, "T&E Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_hydlideRomInfo, MSX_hydlideRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Hydlide II - Shine of Darkness (Jpn, Alt)

static struct BurnRomInfo MSX_hydlide2aRomDesc[] = {
	{ "hydlide ii - shine of darkness (japan) (alt 1).rom",	0x20000, 0xb886d7f5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_hydlide2a, MSX_hydlide2a, msx_msx)
STD_ROM_FN(MSX_hydlide2a)

struct BurnDriver BurnDrvMSX_hydlide2a = {
	"msx_hydlide2a", "msx_hydlide2", "msx_msx", NULL, "1986",
	"Hydlide II - Shine of Darkness (Jpn, Alt)\0", NULL, "T&E Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII16_SRAM, GBF_MISC, 0,
	MSXGetZipName, MSX_hydlide2aRomInfo, MSX_hydlide2aRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Hyper Olympic 1 (Jpn)

static struct BurnRomInfo MSX_hyperol1RomDesc[] = {
	{ "hyper olympic 1 (japan).rom",	0x04000, 0xfef1d8fa, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_hyperol1, MSX_hyperol1, msx_msx)
STD_ROM_FN(MSX_hyperol1)

struct BurnDriver BurnDrvMSX_hyperol1 = {
	"msx_hyperol1", "msx_trackfld", "msx_msx", NULL, "1984",
	"Hyper Olympic 1 (Jpn)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_hyperol1RomInfo, MSX_hyperol1RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Hyper Olympic 1 (Kor)

static struct BurnRomInfo MSX_hyperol1kRomDesc[] = {
	{ "holympic.rom",	0x08000, 0xf36447c5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_hyperol1k, MSX_hyperol1k, msx_msx)
STD_ROM_FN(MSX_hyperol1k)

struct BurnDriver BurnDrvMSX_hyperol1k = {
	"msx_hyperol1k", "msx_trackfld", "msx_msx", NULL, "198?",
	"Hyper Olympic 1 (Kor)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_hyperol1kRomInfo, MSX_hyperol1kRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Hyper Olympic 2 (Jpn)

static struct BurnRomInfo MSX_hyperol2RomDesc[] = {
	{ "hyper olympic 2 (japan).rom",	0x04000, 0x38cb690b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_hyperol2, MSX_hyperol2, msx_msx)
STD_ROM_FN(MSX_hyperol2)

struct BurnDriver BurnDrvMSX_hyperol2 = {
	"msx_hyperol2", "msx_trackfl2", "msx_msx", NULL, "1984",
	"Hyper Olympic 2 (Jpn)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_hyperol2RomInfo, MSX_hyperol2RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Hyper Rally (Jpn)

static struct BurnRomInfo MSX_hyprallyRomDesc[] = {
	{ "hyper rally (japan).rom",	0x04000, 0xf94d452e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_hyprally, MSX_hyprally, msx_msx)
STD_ROM_FN(MSX_hyprally)

struct BurnDriver BurnDrvMSX_hyprally = {
	"msx_hyprally", NULL, "msx_msx", NULL, "1985",
	"Hyper Rally (Jpn)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_hyprallyRomInfo, MSX_hyprallyRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Hyper Rally (Jpn, Alt)

static struct BurnRomInfo MSX_hyprallyaRomDesc[] = {
	{ "hyper rally (japan) (alt 1).rom",	0x04000, 0xc575cec6, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_hyprallya, MSX_hyprallya, msx_msx)
STD_ROM_FN(MSX_hyprallya)

struct BurnDriver BurnDrvMSX_hyprallya = {
	"msx_hyprallya", "msx_hyprally", "msx_msx", NULL, "1985",
	"Hyper Rally (Jpn, Alt)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_hyprallyaRomInfo, MSX_hyprallyaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Hyper Rally (Jpn, Alt 2)

static struct BurnRomInfo MSX_hyprallybRomDesc[] = {
	{ "hyper rally (japan) (alt 2).rom",	0x04000, 0x75cbb75d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_hyprallyb, MSX_hyprallyb, msx_msx)
STD_ROM_FN(MSX_hyprallyb)

struct BurnDriver BurnDrvMSX_hyprallyb = {
	"msx_hyprallyb", "msx_hyprally", "msx_msx", NULL, "1985",
	"Hyper Rally (Jpn, Alt 2)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_hyprallybRomInfo, MSX_hyprallybRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Hyper Rally (Jpn, Alt 3)

static struct BurnRomInfo MSX_hyprallycRomDesc[] = {
	{ "hyper rally (japan) (alt 3).rom",	0x04000, 0x0c5957aa, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_hyprallyc, MSX_hyprallyc, msx_msx)
STD_ROM_FN(MSX_hyprallyc)

struct BurnDriver BurnDrvMSX_hyprallyc = {
	"msx_hyprallyc", "msx_hyprally", "msx_msx", NULL, "1985",
	"Hyper Rally (Jpn, Alt 3)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_hyprallycRomInfo, MSX_hyprallycRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Hyper Sports 1 (Jpn)

static struct BurnRomInfo MSX_hypersptRomDesc[] = {
	{ "hyper sports 1 (japan).rom",	0x04000, 0x18db4ff2, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_hyperspt, MSX_hyperspt, msx_msx)
STD_ROM_FN(MSX_hyperspt)

struct BurnDriver BurnDrvMSX_hyperspt = {
	"msx_hyperspt", NULL, "msx_msx", NULL, "1984",
	"Hyper Sports 1 (Jpn)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_hypersptRomInfo, MSX_hypersptRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Hyper Sports 1 (Jpn, Alt)

static struct BurnRomInfo MSX_hypersptaRomDesc[] = {
	{ "hyper sports 1 (japan) (alt 1).rom",	0x04000, 0x0b5296f7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_hyperspta, MSX_hyperspta, msx_msx)
STD_ROM_FN(MSX_hyperspta)

struct BurnDriver BurnDrvMSX_hyperspta = {
	"msx_hyperspta", "msx_hyperspt", "msx_msx", NULL, "1984",
	"Hyper Sports 1 (Jpn, Alt)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_hypersptaRomInfo, MSX_hypersptaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Hyper Sports 2 (Jpn)

static struct BurnRomInfo MSX_hypersp2RomDesc[] = {
	{ "hyper sports 2 (japan).rom",	0x04000, 0x968fa8d6, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_hypersp2, MSX_hypersp2, msx_msx)
STD_ROM_FN(MSX_hypersp2)

struct BurnDriver BurnDrvMSX_hypersp2 = {
	"msx_hypersp2", NULL, "msx_msx", NULL, "1984",
	"Hyper Sports 2 (Jpn)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_hypersp2RomInfo, MSX_hypersp2RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Hyper Sports 2 (Kor)

static struct BurnRomInfo MSX_hypersp2kRomDesc[] = {
	{ "hsports2.rom",	0x08000, 0xfc932c9f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_hypersp2k, MSX_hypersp2k, msx_msx)
STD_ROM_FN(MSX_hypersp2k)

struct BurnDriver BurnDrvMSX_hypersp2k = {
	"msx_hypersp2k", "msx_hypersp2", "msx_msx", NULL, "198?",
	"Hyper Sports 2 (Kor)\0", NULL, "Topia", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_hypersp2kRomInfo, MSX_hypersp2kRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Hyper Sports 3 (Jpn)

static struct BurnRomInfo MSX_hypersp3RomDesc[] = {
	{ "hyper sports 3 (japan).rom",	0x08000, 0x80a831e1, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_hypersp3, MSX_hypersp3, msx_msx)
STD_ROM_FN(MSX_hypersp3)

struct BurnDriver BurnDrvMSX_hypersp3 = {
	"msx_hypersp3", NULL, "msx_msx", NULL, "1985",
	"Hyper Sports 3 (Jpn)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_hypersp3RomInfo, MSX_hypersp3RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Hyper Sports 3 (Jpn, Alt)

static struct BurnRomInfo MSX_hypersp3aRomDesc[] = {
	{ "hyper sports 3 (japan) (alt 1).rom",	0x08000, 0x9f47e445, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_hypersp3a, MSX_hypersp3a, msx_msx)
STD_ROM_FN(MSX_hypersp3a)

struct BurnDriver BurnDrvMSX_hypersp3a = {
	"msx_hypersp3a", "msx_hypersp3", "msx_msx", NULL, "1985",
	"Hyper Sports 3 (Jpn, Alt)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_hypersp3aRomInfo, MSX_hypersp3aRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Hyper Sports 3 (Jpn, Alt 2)

static struct BurnRomInfo MSX_hypersp3bRomDesc[] = {
	{ "hyper sports 3 (japan) (alt 2).rom",	0x08000, 0xb615c709, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_hypersp3b, MSX_hypersp3b, msx_msx)
STD_ROM_FN(MSX_hypersp3b)

struct BurnDriver BurnDrvMSX_hypersp3b = {
	"msx_hypersp3b", "msx_hypersp3", "msx_msx", NULL, "1985",
	"Hyper Sports 3 (Jpn, Alt 2)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_hypersp3bRomInfo, MSX_hypersp3bRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Ice World (Jpn)

static struct BurnRomInfo MSX_iceworldRomDesc[] = {
	{ "ice world (japan).rom",	0x04000, 0x16e7b4be, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_iceworld, MSX_iceworld, msx_msx)
STD_ROM_FN(MSX_iceworld)

struct BurnDriver BurnDrvMSX_iceworld = {
	"msx_iceworld", NULL, "msx_msx", NULL, "1985",
	"Ice World (Jpn)\0", NULL, "Casio", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_iceworldRomInfo, MSX_iceworldRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Iga Ninpouchou (Jpn)

static struct BurnRomInfo MSX_iganinpoRomDesc[] = {
	{ "iga ninpouten - small ninja (japan).rom",	0x04000, 0x51727e48, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_iganinpo, MSX_iganinpo, msx_msx)
STD_ROM_FN(MSX_iganinpo)

struct BurnDriver BurnDrvMSX_iganinpo = {
	"msx_iganinpo", NULL, "msx_msx", NULL, "1985",
	"Iga Ninpouchou (Jpn)\0", NULL, "Casio", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_iganinpoRomInfo, MSX_iganinpoRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Iga Ninpouchou - Mangetsujou no Tatakai (Jpn)

static struct BurnRomInfo MSX_iganinp2RomDesc[] = {
	{ "iga ninpouten 2 - small ninja 2 (japan).rom",	0x08000, 0x4aa97644, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_iganinp2, MSX_iganinp2, msx_msx)
STD_ROM_FN(MSX_iganinp2)

struct BurnDriver BurnDrvMSX_iganinp2 = {
	"msx_iganinp2", NULL, "msx_msx", NULL, "1986",
	"Iga Ninpouchou - Mangetsujou no Tatakai (Jpn)\0", NULL, "Casio", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_iganinp2RomInfo, MSX_iganinp2RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Indian no Bouken (Jpn)

static struct BurnRomInfo MSX_indianbRomDesc[] = {
	{ "indian no bouken (japan).rom",	0x04000, 0x3a550788, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_indianb, MSX_indianb, msx_msx)
STD_ROM_FN(MSX_indianb)

struct BurnDriver BurnDrvMSX_indianb = {
	"msx_indianb", NULL, "msx_msx", NULL, "1983",
	"Indian no Bouken (Jpn)\0", NULL, "Hudson Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_indianbRomInfo, MSX_indianbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Theseus - Iligks I (Jpn)

static struct BurnRomInfo MSX_theseusRomDesc[] = {
	{ "iriegas - theseus (japan).rom",	0x04000, 0x80495007, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_theseus, MSX_theseus, msx_msx)
STD_ROM_FN(MSX_theseus)

struct BurnDriver BurnDrvMSX_theseus = {
	"msx_theseus", NULL, "msx_msx", NULL, "1984",
	"Theseus - Iligks I (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_theseusRomInfo, MSX_theseusRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Theseus - Iligks I (Jpn, Alt)

static struct BurnRomInfo MSX_theseusaRomDesc[] = {
	{ "iriegas - theseus (japan) (alt 1).rom",	0x04000, 0x53236741, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_theseusa, MSX_theseusa, msx_msx)
STD_ROM_FN(MSX_theseusa)

struct BurnDriver BurnDrvMSX_theseusa = {
	"msx_theseusa", "msx_theseus", "msx_msx", NULL, "1984",
	"Theseus - Iligks I (Jpn, Alt)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_theseusaRomInfo, MSX_theseusaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Theseus - Iligks I (Kor)

static struct BurnRomInfo MSX_theseuskRomDesc[] = {
	{ "theseus.rom",	0x04000, 0x6235de29, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_theseusk, MSX_theseusk, msx_msx)
STD_ROM_FN(MSX_theseusk)

struct BurnDriver BurnDrvMSX_theseusk = {
	"msx_theseusk", "msx_theseus", "msx_msx", NULL, "198?",
	"Theseus - Iligks I (Kor)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_theseuskRomInfo, MSX_theseuskRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Issunboushi no Donnamondai (Jpn)

static struct BurnRomInfo MSX_issunRomDesc[] = {
	{ "issunhoushi no donnamondai (japan).rom",	0x08000, 0xa7c43855, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_issun, MSX_issun, msx_msx)
STD_ROM_FN(MSX_issun)

struct BurnDriver BurnDrvMSX_issun = {
	"msx_issun", NULL, "msx_msx", NULL, "1987",
	"Issunboushi no Donnamondai (Jpn)\0", NULL, "Casio", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_issunRomInfo, MSX_issunRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Issunboushi no Donnamondai (Jpn, Alt)

static struct BurnRomInfo MSX_issunaRomDesc[] = {
	{ "issunhoushi no donnamondai (japan) (alt 1).rom",	0x08000, 0x2a285706, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_issuna, MSX_issuna, msx_msx)
STD_ROM_FN(MSX_issuna)

struct BurnDriver BurnDrvMSX_issuna = {
	"msx_issuna", "msx_issun", "msx_msx", NULL, "1987",
	"Issunboushi no Donnamondai (Jpn, Alt)\0", NULL, "Casio", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_issunaRomInfo, MSX_issunaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Issunboushi no Donnamondai (Jpn, Hacked?)

static struct BurnRomInfo MSX_issunhRomDesc[] = {
	{ "issunhoushi no donnamondai. little samurai (1987)(casio)[cr angel].rom",	0x08000, 0x5b66062b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_issunh, MSX_issunh, msx_msx)
STD_ROM_FN(MSX_issunh)

struct BurnDriver BurnDrvMSX_issunh = {
	"msx_issunh", "msx_issun", "msx_msx", NULL, "1987",
	"Issunboushi no Donnamondai (Jpn, Hacked?)\0", NULL, "Angel?", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_issunhRomInfo, MSX_issunhRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// J.P. Winkle (Jpn)

static struct BurnRomInfo MSX_jpwinkleRomDesc[] = {
	{ "j.p. winkle (japan).rom",	0x08000, 0x57e1221c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_jpwinkle, MSX_jpwinkle, msx_msx)
STD_ROM_FN(MSX_jpwinkle)

struct BurnDriver BurnDrvMSX_jpwinkle = {
	"msx_jpwinkle", NULL, "msx_msx", NULL, "1986",
	"J.P. Winkle (Jpn)\0", NULL, "ASCII ~ MSX Magazine", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_jpwinkleRomInfo, MSX_jpwinkleRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// J.P. Winkle (Kor)

static struct BurnRomInfo MSX_jpwinklekRomDesc[] = {
	{ "jpwinkle.rom",	0x08000, 0x1bc4132b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_jpwinklek, MSX_jpwinklek, msx_msx)
STD_ROM_FN(MSX_jpwinklek)

struct BurnDriver BurnDrvMSX_jpwinklek = {
	"msx_jpwinklek", "msx_jpwinkle", "msx_msx", NULL, "198?",
	"J.P. Winkle (Kor)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_jpwinklekRomInfo, MSX_jpwinklekRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Jagur (Jpn, Alt)

static struct BurnRomInfo MSX_jaguraRomDesc[] = {
	{ "jagur (japan) (alt 1).rom",	0x20000, 0xdbf2f244, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_jagura, MSX_jagura, msx_msx)
STD_ROM_FN(MSX_jagura)

struct BurnDriver BurnDrvMSX_jagura = {
	"msx_jagura", "msx_jagur", "msx_msx", NULL, "1987",
	"Jagur (Jpn, Alt)\0", NULL, "Hudson Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII16, GBF_MISC, 0,
	MSXGetZipName, MSX_jaguraRomInfo, MSX_jaguraRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Jet Set Willy (Jpn)

static struct BurnRomInfo MSX_jetsetwRomDesc[] = {
	{ "jet set willy (japan).rom",	0x04000, 0x9191c890, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_jetsetw, MSX_jetsetw, msx_msx)
STD_ROM_FN(MSX_jetsetw)

struct BurnDriver BurnDrvMSX_jetsetw = {
	"msx_jetsetw", NULL, "msx_msx", NULL, "1985",
	"Jet Set Willy (Jpn)\0", NULL, "Hudson Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_jetsetwRomInfo, MSX_jetsetwRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Jet Set Willy (Jpn, Alt)

static struct BurnRomInfo MSX_jetsetwaRomDesc[] = {
	{ "jet set willy (japan) (alt 1).rom",	0x04000, 0x6e57f290, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_jetsetwa, MSX_jetsetwa, msx_msx)
STD_ROM_FN(MSX_jetsetwa)

struct BurnDriver BurnDrvMSX_jetsetwa = {
	"msx_jetsetwa", "msx_jetsetw", "msx_msx", NULL, "1985",
	"Jet Set Willy (Jpn, Alt)\0", NULL, "Hudson Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_jetsetwaRomInfo, MSX_jetsetwaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Jump (Jpn)

static struct BurnRomInfo MSX_jumpRomDesc[] = {
	{ "jump (japan).rom",	0x04000, 0x0599617b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_jump, MSX_jump, msx_msx)
STD_ROM_FN(MSX_jump)

struct BurnDriver BurnDrvMSX_jump = {
	"msx_jump", NULL, "msx_msx", NULL, "1985",
	"Jump (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_jumpRomInfo, MSX_jumpRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Jump Coaster (Jpn)

static struct BurnRomInfo MSX_jumpcstrRomDesc[] = {
	{ "jump coaster (japan).rom",	0x02000, 0xc15a25da, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_jumpcstr, MSX_jumpcstr, msx_msx)
STD_ROM_FN(MSX_jumpcstr)

struct BurnDriver BurnDrvMSX_jumpcstr = {
	"msx_jumpcstr", NULL, "msx_msx", NULL, "1984",
	"Jump Coaster (Jpn)\0", NULL, "Nihon Columbia", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_jumpcstrRomInfo, MSX_jumpcstrRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Juno First (Jpn)

static struct BurnRomInfo MSX_junofrstRomDesc[] = {
	{ "juno first (japan).rom",	0x02000, 0xea67df20, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_junofrst, MSX_junofrst, msx_msx)
STD_ROM_FN(MSX_junofrst)

struct BurnDriver BurnDrvMSX_junofrst = {
	"msx_junofrst", NULL, "msx_msx", NULL, "1983",
	"Juno First (Jpn)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_junofrstRomInfo, MSX_junofrstRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Kage no Densetsu - The Legend of Kage (Jpn)

static struct BurnRomInfo MSX_legkageRomDesc[] = {
	{ "kage no densetsu - legend of kage, the (japan).rom",	0x08000, 0x69367e10, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_legkage, MSX_legkage, msx_msx)
STD_ROM_FN(MSX_legkage)

struct BurnDriver BurnDrvMSX_legkage = {
	"msx_legkage", NULL, "msx_msx", NULL, "1986",
	"Kage no Densetsu - The Legend of Kage (Jpn)\0", NULL, "Taito", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_legkageRomInfo, MSX_legkageRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Karamaru - Chindou Chuu (Jpn)

static struct BurnRomInfo MSX_karamaruRomDesc[] = {
	{ "karamaru (japan).rom",	0x04000, 0x12be29fc, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_karamaru, MSX_karamaru, msx_msx)
STD_ROM_FN(MSX_karamaru)

struct BurnDriver BurnDrvMSX_karamaru = {
	"msx_karamaru", NULL, "msx_msx", NULL, "1985",
	"Karamaru - Chindou Chuu (Jpn)\0", NULL, "HAL Kenkyuujo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_BASIC, GBF_MISC, 0,
	MSXGetZipName, MSX_karamaruRomInfo, MSX_karamaruRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Keystone Kapers (Jpn)

static struct BurnRomInfo MSX_keykaperRomDesc[] = {
	{ "keystone kapers (japan).rom",	0x04000, 0xb1cf2097, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_keykaper, MSX_keykaper, msx_msx)
STD_ROM_FN(MSX_keykaper)

struct BurnDriver BurnDrvMSX_keykaper = {
	"msx_keykaper", NULL, "msx_msx", NULL, "1984",
	"Keystone Kapers (Jpn)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_keykaperRomInfo, MSX_keykaperRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Kick It (Jpn)

static struct BurnRomInfo MSX_kickitRomDesc[] = {
	{ "kick it (japan).rom",	0x08000, 0xe4f63cb7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_kickit, MSX_kickit, msx_msx)
STD_ROM_FN(MSX_kickit)

struct BurnDriver BurnDrvMSX_kickit = {
	"msx_kickit", NULL, "msx_msx", NULL, "1986",
	"Kick It (Jpn)\0", NULL, "HAL Kenkyuujo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_kickitRomInfo, MSX_kickitRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Kick It (Jpn, Alt)

static struct BurnRomInfo MSX_kickitaRomDesc[] = {
	{ "kick it (japan) (alt 1).rom",	0x08000, 0x3424a220, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_kickita, MSX_kickita, msx_msx)
STD_ROM_FN(MSX_kickita)

struct BurnDriver BurnDrvMSX_kickita = {
	"msx_kickita", "msx_kickit", "msx_msx", NULL, "1986",
	"Kick It (Jpn, Alt)\0", NULL, "HAL Kenkyuujo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_kickitaRomInfo, MSX_kickitaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// King & Balloon (Jpn)

static struct BurnRomInfo MSX_kingballRomDesc[] = {
	{ "king & balloon (japan).rom",	0x08000, 0x2aba2253, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_kingball, MSX_kingball, msx_msx)
STD_ROM_FN(MSX_kingball)

struct BurnDriver BurnDrvMSX_kingball = {
	"msx_kingball", NULL, "msx_msx", NULL, "1984",
	"King & Balloon (Jpn)\0", NULL, "Namcot", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_kingballRomInfo, MSX_kingballRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// King & Balloon (Jpn, Alt)

static struct BurnRomInfo MSX_kingballaRomDesc[] = {
	{ "king & balloon (japan) (alt 1).rom",	0x04000, 0x1eeba987, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_kingballa, MSX_kingballa, msx_msx)
STD_ROM_FN(MSX_kingballa)

struct BurnDriver BurnDrvMSX_kingballa = {
	"msx_kingballa", "msx_kingball", "msx_msx", NULL, "1984",
	"King & Balloon (Jpn, Alt)\0", NULL, "Namcot", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_kingballaRomInfo, MSX_kingballaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// King's Knight (Jpn, Alt)

static struct BurnRomInfo MSX_kingkngtaRomDesc[] = {
	{ "king knight (japan) (alt 1).rom",	0x20000, 0x4d884352, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_kingkngta, MSX_kingkngta, msx_msx)
STD_ROM_FN(MSX_kingkngta)

struct BurnDriver BurnDrvMSX_kingkngta = {
	"msx_kingkngta", "msx_kingkngt", "msx_msx", NULL, "1986",
	"King's Knight (Jpn, Alt)\0", NULL, "Square", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII16, GBF_MISC, 0,
	MSXGetZipName, MSX_kingkngtaRomInfo, MSX_kingkngtaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// King's Knight (Jpn, Alt 2)

static struct BurnRomInfo MSX_kingkngtbRomDesc[] = {
	{ "king knight (japan) (alt 2).rom",	0x20000, 0xab6cd62c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_kingkngtb, MSX_kingkngtb, msx_msx)
STD_ROM_FN(MSX_kingkngtb)

struct BurnDriver BurnDrvMSX_kingkngtb = {
	"msx_kingkngtb", "msx_kingkngt", "msx_msx", NULL, "1986",
	"King's Knight (Jpn, Alt 2)\0", NULL, "Square", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII16, GBF_MISC, 0,
	MSXGetZipName, MSX_kingkngtbRomInfo, MSX_kingkngtbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// King's Valley (Euro, Jpn)

static struct BurnRomInfo MSX_kingvalRomDesc[] = {
	{ "king's valley (japan, europe).rom",	0x04000, 0xdfac2125, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_kingval, MSX_kingval, msx_msx)
STD_ROM_FN(MSX_kingval)

struct BurnDriver BurnDrvMSX_kingval = {
	"msx_kingval", NULL, "msx_msx", NULL, "1985",
	"King's Valley (Euro, Jpn)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_kingvalRomInfo, MSX_kingvalRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// King's Valley (Euro, Jpn, Alt)

static struct BurnRomInfo MSX_kingvalaRomDesc[] = {
	{ "king's valley (japan, europe) (alt 1).rom",	0x04000, 0x5a141c44, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_kingvala, MSX_kingvala, msx_msx)
STD_ROM_FN(MSX_kingvala)

struct BurnDriver BurnDrvMSX_kingvala = {
	"msx_kingvala", "msx_kingval", "msx_msx", NULL, "1985",
	"King's Valley (Euro, Jpn, Alt)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_kingvalaRomInfo, MSX_kingvalaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// King's Valley (Euro, Jpn, Alt 2)

static struct BurnRomInfo MSX_kingvalbRomDesc[] = {
	{ "king's valley (japan, europe) (alt 2).rom",	0x04000, 0xe7c3e1b7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_kingvalb, MSX_kingvalb, msx_msx)
STD_ROM_FN(MSX_kingvalb)

struct BurnDriver BurnDrvMSX_kingvalb = {
	"msx_kingvalb", "msx_kingval", "msx_msx", NULL, "1985",
	"King's Valley (Euro, Jpn, Alt 2)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_kingvalbRomInfo, MSX_kingvalbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// King's Valley (Euro, Jpn, Alt 3)

static struct BurnRomInfo MSX_kingvalcRomDesc[] = {
	{ "king's valley (japan, europe) (alt 3).rom",	0x04000, 0x201d7691, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_kingvalc, MSX_kingvalc, msx_msx)
STD_ROM_FN(MSX_kingvalc)

struct BurnDriver BurnDrvMSX_kingvalc = {
	"msx_kingvalc", "msx_kingval", "msx_msx", NULL, "1985",
	"King's Valley (Euro, Jpn, Alt 3)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_kingvalcRomInfo, MSX_kingvalcRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// King's Valley (Euro, Jpn, Alt 4)

static struct BurnRomInfo MSX_kingvaldRomDesc[] = {
	{ "king's valley (japan, europe) (alt 4).rom",	0x04000, 0x627bdcd6, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_kingvald, MSX_kingvald, msx_msx)
STD_ROM_FN(MSX_kingvald)

struct BurnDriver BurnDrvMSX_kingvald = {
	"msx_kingvald", "msx_kingval", "msx_msx", NULL, "1985",
	"King's Valley (Euro, Jpn, Alt 4)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_kingvaldRomInfo, MSX_kingvaldRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// King's Valley II (Euro) ~ Ouke no Tani - El Giza no Fuuin (Jpn)

static struct BurnRomInfo MSX_kingval2RomDesc[] = {
	{ "king's valley ii (japan, europe).rom",	0x20000, 0xe82a8e8e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_kingval2, MSX_kingval2, msx_msx)
STD_ROM_FN(MSX_kingval2)

struct BurnDriver BurnDrvMSX_kingval2 = {
	"msx_kingval2", NULL, "msx_msx", NULL, "1988",
	"King's Valley II (Euro) ~ Ouke no Tani - El Giza no Fuuin (Jpn)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_KONAMI_SCC, GBF_MISC, 0,
	MSXGetZipName, MSX_kingval2RomInfo, MSX_kingval2RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Kinnikuman - Colosseum Deathmatch (Jpn)

static struct BurnRomInfo MSX_kinnikumRomDesc[] = {
	{ "kinnikuman - muscle man (japan).rom",	0x08000, 0x166781b7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_kinnikum, MSX_kinnikum, msx_msx)
STD_ROM_FN(MSX_kinnikum)

struct BurnDriver BurnDrvMSX_kinnikum = {
	"msx_kinnikum", NULL, "msx_msx", NULL, "1985",
	"Kinnikuman - Colosseum Deathmatch (Jpn)\0", NULL, "Bandai", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_kinnikumRomInfo, MSX_kinnikumRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Kinnikuman - Colosseum Deathmatch (Jpn, Alt)

static struct BurnRomInfo MSX_kinnikumaRomDesc[] = {
	{ "kinnikuman - muscle man (japan) (alt 1).rom",	0x08000, 0x500d112d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_kinnikuma, MSX_kinnikuma, msx_msx)
STD_ROM_FN(MSX_kinnikuma)

struct BurnDriver BurnDrvMSX_kinnikuma = {
	"msx_kinnikuma", "msx_kinnikum", "msx_msx", NULL, "1985",
	"Kinnikuman - Colosseum Deathmatch (Jpn, Alt)\0", NULL, "Bandai", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_kinnikumaRomInfo, MSX_kinnikumaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Kinnikuman - Colosseum Deathmatch (Kor)

static struct BurnRomInfo MSX_kinnikumkRomDesc[] = {
	{ "wrestling.rom",	0x08000, 0x36ed61af, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_kinnikumk, MSX_kinnikumk, msx_msx)
STD_ROM_FN(MSX_kinnikumk)

struct BurnDriver BurnDrvMSX_kinnikumk = {
	"msx_kinnikumk", "msx_kinnikum", "msx_msx", NULL, "198?",
	"Kinnikuman - Colosseum Deathmatch (Kor)\0", NULL, "San Ho", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_kinnikumkRomInfo, MSX_kinnikumkRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Knight Lore (Jpn)

static struct BurnRomInfo MSX_knightlrRomDesc[] = {
	{ "knight lore (japan).rom",	0x08000, 0xb575c44a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_knightlr, MSX_knightlr, msx_msx)
STD_ROM_FN(MSX_knightlr)

struct BurnDriver BurnDrvMSX_knightlr = {
	"msx_knightlr", NULL, "msx_msx", NULL, "1986",
	"Knight Lore (Jpn)\0", NULL, "Nihon Dexter", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_knightlrRomInfo, MSX_knightlrRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Knightmare - Majou Densetsu (Jpn)

static struct BurnRomInfo MSX_knightmrRomDesc[] = {
	{ "knightmare - majou densetsu (japan).rom",	0x08000, 0x0db84205, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_knightmr, MSX_knightmr, msx_msx)
STD_ROM_FN(MSX_knightmr)

struct BurnDriver BurnDrvMSX_knightmr = {
	"msx_knightmr", NULL, "msx_msx", NULL, "1986",
	"Knightmare - Majou Densetsu (Jpn)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_knightmrRomInfo, MSX_knightmrRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Knightmare - Majou Densetsu (Jpn, Alt)

static struct BurnRomInfo MSX_knightmraRomDesc[] = {
	{ "knightmare - majou densetsu (japan) (alt 1).rom",	0x08000, 0xca9f791b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_knightmra, MSX_knightmra, msx_msx)
STD_ROM_FN(MSX_knightmra)

struct BurnDriver BurnDrvMSX_knightmra = {
	"msx_knightmra", "msx_knightmr", "msx_msx", NULL, "1986",
	"Knightmare - Majou Densetsu (Jpn, Alt)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_knightmraRomInfo, MSX_knightmraRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Knightmare - Majou Densetsu (Jpn, Alt 2)

static struct BurnRomInfo MSX_knightmrbRomDesc[] = {
	{ "knightmare - majou densetsu (japan) (alt 2).rom",	0x08000, 0x5876a372, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_knightmrb, MSX_knightmrb, msx_msx)
STD_ROM_FN(MSX_knightmrb)

struct BurnDriver BurnDrvMSX_knightmrb = {
	"msx_knightmrb", "msx_knightmr", "msx_msx", NULL, "1986",
	"Knightmare - Majou Densetsu (Jpn, Alt 2)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_knightmrbRomInfo, MSX_knightmrbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Knightmare - Majou Densetsu (Kor)

static struct BurnRomInfo MSX_knightmrkRomDesc[] = {
	{ "knightma.rom",	0x08000, 0xb9819ea6, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_knightmrk, MSX_knightmrk, msx_msx)
STD_ROM_FN(MSX_knightmrk)

struct BurnDriver BurnDrvMSX_knightmrk = {
	"msx_knightmrk", "msx_knightmr", "msx_msx", NULL, "198?",
	"Knightmare - Majou Densetsu (Kor)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_knightmrkRomInfo, MSX_knightmrkRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Knightmare II - The Maze of Galious (Jpn)

static struct BurnRomInfo MSX_knightm2RomDesc[] = {
	{ "knightmare ii - the maze of galious (japan).rom",	0x20000, 0xfe23d253, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_knightm2, MSX_knightm2, msx_msx)
STD_ROM_FN(MSX_knightm2)

struct BurnDriver BurnDrvMSX_knightm2 = {
	"msx_knightm2", NULL, "msx_msx", NULL, "1987",
	"Knightmare II - The Maze of Galious (Jpn)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_KONAMI, GBF_MISC, 0,
	MSXGetZipName, MSX_knightm2RomInfo, MSX_knightm2RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Knightmare II - The Maze of Galious (Kor)

static struct BurnRomInfo MSX_knightm2kRomDesc[] = {
	{ "knightmare ii - the maze of galious (1987)(zemina)[rc-749].rom",	0x20000, 0xd7f35938, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_knightm2k, MSX_knightm2k, msx_msx)
STD_ROM_FN(MSX_knightm2k)

struct BurnDriver BurnDrvMSX_knightm2k = {
	"msx_knightm2k", "msx_knightm2", "msx_msx", NULL, "1987",
	"Knightmare II - The Maze of Galious (Kor)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_KONAMI, GBF_MISC, 0,
	MSXGetZipName, MSX_knightm2kRomInfo, MSX_knightm2kRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Knightmare III - Shalom (Jpn)

static struct BurnRomInfo MSX_knightm3RomDesc[] = {
	{ "knightmare iii - shalom (japan).rom",	0x40000, 0xcf60fa7d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_knightm3, MSX_knightm3, msx_msx)
STD_ROM_FN(MSX_knightm3)

struct BurnDriver BurnDrvMSX_knightm3 = {
	"msx_knightm3", NULL, "msx_msx", NULL, "1987",
	"Knightmare III - Shalom (Jpn)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_KONAMI, GBF_MISC, 0,
	MSXGetZipName, MSX_knightm3RomInfo, MSX_knightm3RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Knither Special (Jpn)

static struct BurnRomInfo MSX_knithersRomDesc[] = {
	{ "knither special (japan).rom",	0x20000, 0xa3b2fe71, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_knithers, MSX_knithers, msx_msx)
STD_ROM_FN(MSX_knithers)

struct BurnDriver BurnDrvMSX_knithers = {
	"msx_knithers", NULL, "msx_msx", NULL, "1987",
	"Knither Special (Jpn)\0", NULL, "Dempa", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII16, GBF_MISC, 0,
	MSXGetZipName, MSX_knithersRomInfo, MSX_knithersRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Knuckle Joe (Kor)

static struct BurnRomInfo MSX_knucklejRomDesc[] = {
	{ "knuckle joe.rom",	0x08000, 0x33be6192, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_knucklej, MSX_knucklej, msx_msx)
STD_ROM_FN(MSX_knucklej)

struct BurnDriver BurnDrvMSX_knucklej = {
	"msx_knucklej", NULL, "msx_msx", NULL, "1989",
	"Knuckle Joe (Kor)\0", NULL, "Prosoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_knucklejRomInfo, MSX_knucklejRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Koedoli (Kor)

static struct BurnRomInfo MSX_koedoliRomDesc[] = {
	{ "koedoli (mickey soft, 1988).rom",	0x08000, 0xcb345bcc, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_koedoli, MSX_koedoli, msx_msx)
STD_ROM_FN(MSX_koedoli)

struct BurnDriver BurnDrvMSX_koedoli = {
	"msx_koedoli", NULL, "msx_msx", NULL, "1988",
	"Koedoli (Kor)\0", NULL, "Aproman", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_koedoliRomInfo, MSX_koedoliRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Konami's Baseball (Jpn)

static struct BurnRomInfo MSX_konbballRomDesc[] = {
	{ "konami's baseball (japan).rom",	0x04000, 0xb660494b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_konbball, MSX_konbball, msx_msx)
STD_ROM_FN(MSX_konbball)

struct BurnDriver BurnDrvMSX_konbball = {
	"msx_konbball", NULL, "msx_msx", NULL, "1985",
	"Konami's Baseball (Jpn)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_konbballRomInfo, MSX_konbballRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Konami's Baseball (Jpn, Alt)

static struct BurnRomInfo MSX_konbballaRomDesc[] = {
	{ "konami's baseball (japan) (alt 1).rom",	0x04000, 0x6a8e56e1, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_konbballa, MSX_konbballa, msx_msx)
STD_ROM_FN(MSX_konbballa)

struct BurnDriver BurnDrvMSX_konbballa = {
	"msx_konbballa", "msx_konbball", "msx_msx", NULL, "1985",
	"Konami's Baseball (Jpn, Alt)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_konbballaRomInfo, MSX_konbballaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Konami's Billiards (Euro)

static struct BurnRomInfo MSX_konbillRomDesc[] = {
	{ "konami's billiards (europe).rom",	0x02000, 0x0b65fe4d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_konbill, MSX_konbill, msx_msx)
STD_ROM_FN(MSX_konbill)

struct BurnDriver BurnDrvMSX_konbill = {
	"msx_konbill", NULL, "msx_msx", NULL, "1984",
	"Konami's Billiards (Euro)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_konbillRomInfo, MSX_konbillRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Konami's Boxing (Jpn)

static struct BurnRomInfo MSX_konboxinRomDesc[] = {
	{ "konami's boxing (japan).rom",	0x08000, 0x3a442408, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_konboxin, MSX_konboxin, msx_msx)
STD_ROM_FN(MSX_konboxin)

struct BurnDriver BurnDrvMSX_konboxin = {
	"msx_konboxin", NULL, "msx_msx", NULL, "1985",
	"Konami's Boxing (Jpn)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_konboxinRomInfo, MSX_konboxinRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Konami's Boxing (Jpn, Alt)

static struct BurnRomInfo MSX_konboxinaRomDesc[] = {
	{ "konami's boxing (japan) (alt 1).rom",	0x08000, 0x19ccbce1, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_konboxina, MSX_konboxina, msx_msx)
STD_ROM_FN(MSX_konboxina)

struct BurnDriver BurnDrvMSX_konboxina = {
	"msx_konboxina", "msx_konboxin", "msx_msx", NULL, "1985",
	"Konami's Boxing (Jpn, Alt)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_konboxinaRomInfo, MSX_konboxinaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Konami's Boxing (Jpn, Alt 2)

static struct BurnRomInfo MSX_konboxinbRomDesc[] = {
	{ "konami's boxing (japan) (alt 2).rom",	0x08000, 0x0d94e7b2, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_konboxinb, MSX_konboxinb, msx_msx)
STD_ROM_FN(MSX_konboxinb)

struct BurnDriver BurnDrvMSX_konboxinb = {
	"msx_konboxinb", "msx_konboxin", "msx_msx", NULL, "1985",
	"Konami's Boxing (Jpn, Alt 2)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_konboxinbRomInfo, MSX_konboxinbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Konami's Football (Euro)

static struct BurnRomInfo MSX_konfootbRomDesc[] = {
	{ "konami's football (europe).rom",	0x08000, 0xba1b16fc, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_konfootb, MSX_konfootb, msx_msx)
STD_ROM_FN(MSX_konfootb)

struct BurnDriver BurnDrvMSX_konfootb = {
	"msx_konfootb", NULL, "msx_msx", NULL, "1985",
	"Konami's Football (Euro)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_konfootbRomInfo, MSX_konfootbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Konami's Football (Euro, Alt)

static struct BurnRomInfo MSX_konfootbaRomDesc[] = {
	{ "konami's football (europe) (alt 1).rom",	0x08000, 0x93604c07, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_konfootba, MSX_konfootba, msx_msx)
STD_ROM_FN(MSX_konfootba)

struct BurnDriver BurnDrvMSX_konfootba = {
	"msx_konfootba", "msx_konfootb", "msx_msx", NULL, "1985",
	"Konami's Football (Euro, Alt)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_konfootbaRomInfo, MSX_konfootbaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Konami's Football (Euro, Alt 2)

static struct BurnRomInfo MSX_konfootbbRomDesc[] = {
	{ "konami's football (europe) (alt 2).rom",	0x08000, 0xc700fc49, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_konfootbb, MSX_konfootbb, msx_msx)
STD_ROM_FN(MSX_konfootbb)

struct BurnDriver BurnDrvMSX_konfootbb = {
	"msx_konfootbb", "msx_konfootb", "msx_msx", NULL, "1985",
	"Konami's Football (Euro, Alt 2)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_konfootbbRomInfo, MSX_konfootbbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Konami's Golf (Jpn)

static struct BurnRomInfo MSX_kongolfRomDesc[] = {
	{ "konami's golf (japan).rom",	0x04000, 0x08c7e406, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_kongolf, MSX_kongolf, msx_msx)
STD_ROM_FN(MSX_kongolf)

struct BurnDriver BurnDrvMSX_kongolf = {
	"msx_kongolf", NULL, "msx_msx", NULL, "1985",
	"Konami's Golf (Jpn)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_kongolfRomInfo, MSX_kongolfRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Konami's Golf (Jpn, Alt)

static struct BurnRomInfo MSX_kongolfaRomDesc[] = {
	{ "konami's golf (japan) (alt 1).rom",	0x04000, 0x0e3380fe, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_kongolfa, MSX_kongolfa, msx_msx)
STD_ROM_FN(MSX_kongolfa)

struct BurnDriver BurnDrvMSX_kongolfa = {
	"msx_kongolfa", "msx_kongolf", "msx_msx", NULL, "1985",
	"Konami's Golf (Jpn, Alt)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_kongolfaRomInfo, MSX_kongolfaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Konami's Golf (Jpn, Alt 2)

static struct BurnRomInfo MSX_kongolfbRomDesc[] = {
	{ "konami's golf (japan) (alt 2).rom",	0x04000, 0xf06befd3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_kongolfb, MSX_kongolfb, msx_msx)
STD_ROM_FN(MSX_kongolfb)

struct BurnDriver BurnDrvMSX_kongolfb = {
	"msx_kongolfb", "msx_kongolf", "msx_msx", NULL, "1985",
	"Konami's Golf (Jpn, Alt 2)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_kongolfbRomInfo, MSX_kongolfbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Mahjong Dojo (Jpn)

static struct BurnRomInfo MSX_mjdojoRomDesc[] = {
	{ "konami's mahjong (japan).rom",	0x08000, 0x114198e5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mjdojo, MSX_mjdojo, msx_msx)
STD_ROM_FN(MSX_mjdojo)

struct BurnDriver BurnDrvMSX_mjdojo = {
	"msx_mjdojo", NULL, "msx_msx", NULL, "1984",
	"Mahjong Dojo (Jpn)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_mjdojoRomInfo, MSX_mjdojoRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Konami's Ping-Pong (Jpn)

static struct BurnRomInfo MSX_pingpongRomDesc[] = {
	{ "konami's ping-pong (japan).rom",	0x04000, 0x3f200491, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_pingpong, MSX_pingpong, msx_msx)
STD_ROM_FN(MSX_pingpong)

struct BurnDriver BurnDrvMSX_pingpong = {
	"msx_pingpong", NULL, "msx_msx", NULL, "1985",
	"Konami's Ping-Pong (Jpn)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_pingpongRomInfo, MSX_pingpongRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Konami's Ping-Pong (Jpn, Alt)

static struct BurnRomInfo MSX_pingpongaRomDesc[] = {
	{ "konami's ping-pong (japan) (alt 1).rom",	0x04000, 0xbbcdd731, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_pingponga, MSX_pingponga, msx_msx)
STD_ROM_FN(MSX_pingponga)

struct BurnDriver BurnDrvMSX_pingponga = {
	"msx_pingponga", "msx_pingpong", "msx_msx", NULL, "1985",
	"Konami's Ping-Pong (Jpn, Alt)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_pingpongaRomInfo, MSX_pingpongaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Konami's Ping-Pong (Jpn, Alt 2)

static struct BurnRomInfo MSX_pingpongbRomDesc[] = {
	{ "konami's ping-pong (japan) (alt 2).rom",	0x04000, 0xa4d465a4, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_pingpongb, MSX_pingpongb, msx_msx)
STD_ROM_FN(MSX_pingpongb)

struct BurnDriver BurnDrvMSX_pingpongb = {
	"msx_pingpongb", "msx_pingpong", "msx_msx", NULL, "1985",
	"Konami's Ping-Pong (Jpn, Alt 2)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_pingpongbRomInfo, MSX_pingpongbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Konami's Ping-Pong (Jpn, Alt 3)

static struct BurnRomInfo MSX_pingpongcRomDesc[] = {
	{ "konami's ping-pong (japan) (alt 3).rom",	0x04000, 0x2829a061, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_pingpongc, MSX_pingpongc, msx_msx)
STD_ROM_FN(MSX_pingpongc)

struct BurnDriver BurnDrvMSX_pingpongc = {
	"msx_pingpongc", "msx_pingpong", "msx_msx", NULL, "1985",
	"Konami's Ping-Pong (Jpn, Alt 3)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_pingpongcRomInfo, MSX_pingpongcRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Konami's Ping-Pong (Kor)

static struct BurnRomInfo MSX_pingpongkRomDesc[] = {
	{ "pingpong.rom",	0x08000, 0x67a1ba79, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_pingpongk, MSX_pingpongk, msx_msx)
STD_ROM_FN(MSX_pingpongk)

struct BurnDriver BurnDrvMSX_pingpongk = {
	"msx_pingpongk", "msx_pingpong", "msx_msx", NULL, "198?",
	"Konami's Ping-Pong (Kor)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_pingpongkRomInfo, MSX_pingpongkRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Konami's Soccer (Jpn)

static struct BurnRomInfo MSX_konsoccrRomDesc[] = {
	{ "konami's soccer (japan).rom",	0x08000, 0x74bc8295, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_konsoccr, MSX_konsoccr, msx_msx)
STD_ROM_FN(MSX_konsoccr)

struct BurnDriver BurnDrvMSX_konsoccr = {
	"msx_konsoccr", "msx_konfootb", "msx_msx", NULL, "1985",
	"Konami's Soccer (Jpn)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_konsoccrRomInfo, MSX_konsoccrRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Konami's Soccer (Jpn, Alt)

static struct BurnRomInfo MSX_konsoccraRomDesc[] = {
	{ "konami's soccer (japan) (alt 1).rom",	0x08000, 0x58ea53b9, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_konsoccra, MSX_konsoccra, msx_msx)
STD_ROM_FN(MSX_konsoccra)

struct BurnDriver BurnDrvMSX_konsoccra = {
	"msx_konsoccra", "msx_konfootb", "msx_msx", NULL, "1985",
	"Konami's Soccer (Jpn, Alt)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_konsoccraRomInfo, MSX_konsoccraRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Konami's Soccer (Jpn, Alt 2)

static struct BurnRomInfo MSX_konsoccrbRomDesc[] = {
	{ "konami's soccer (japan) (alt 2).rom",	0x08000, 0xe861d2bd, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_konsoccrb, MSX_konsoccrb, msx_msx)
STD_ROM_FN(MSX_konsoccrb)

struct BurnDriver BurnDrvMSX_konsoccrb = {
	"msx_konsoccrb", "msx_konfootb", "msx_msx", NULL, "1985",
	"Konami's Soccer (Jpn, Alt 2)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_konsoccrbRomInfo, MSX_konsoccrbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Konami's Soccer (Jpn, Alt 3)

static struct BurnRomInfo MSX_konsoccrcRomDesc[] = {
	{ "konami's soccer (japan) (alt 3).rom",	0x08000, 0xccfb0ca2, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_konsoccrc, MSX_konsoccrc, msx_msx)
STD_ROM_FN(MSX_konsoccrc)

struct BurnDriver BurnDrvMSX_konsoccrc = {
	"msx_konsoccrc", "msx_konfootb", "msx_msx", NULL, "1985",
	"Konami's Soccer (Jpn, Alt 3)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_konsoccrcRomInfo, MSX_konsoccrcRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Konami's Tennis (Jpn)

static struct BurnRomInfo MSX_kontennRomDesc[] = {
	{ "konami's tennis (japan).rom",	0x04000, 0x61726be4, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_kontenn, MSX_kontenn, msx_msx)
STD_ROM_FN(MSX_kontenn)

struct BurnDriver BurnDrvMSX_kontenn = {
	"msx_kontenn", NULL, "msx_msx", NULL, "1985",
	"Konami's Tennis (Jpn)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_kontennRomInfo, MSX_kontennRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Konami's Tennis (Jpn, Alt)

static struct BurnRomInfo MSX_kontennaRomDesc[] = {
	{ "konami's tennis (japan) (alt 1).rom",	0x04000, 0xcfce0a28, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_kontenna, MSX_kontenna, msx_msx)
STD_ROM_FN(MSX_kontenna)

struct BurnDriver BurnDrvMSX_kontenna = {
	"msx_kontenna", "msx_kontenn", "msx_msx", NULL, "1985",
	"Konami's Tennis (Jpn, Alt)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_kontennaRomInfo, MSX_kontennaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Konami's Tennis (Kor)

static struct BurnRomInfo MSX_kontennkRomDesc[] = {
	{ "konami's tennis (1984)(konami)[cr prosoft][rc-720].rom",	0x04001, 0xa5427ecd, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_kontennk, MSX_kontennk, msx_msx)
STD_ROM_FN(MSX_kontennk)

struct BurnDriver BurnDrvMSX_kontennk = {
	"msx_kontennk", "msx_kontenn", "msx_msx", NULL, "19??",
	"Konami's Tennis (Kor)\0", NULL, "Prosoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_kontennkRomInfo, MSX_kontennkRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Koneko no Daibouken - Chibi-chan ga Iku (Jpn)

static struct BurnRomInfo MSX_konekoRomDesc[] = {
	{ "koneko no daibouken - catboy (japan).rom",	0x08000, 0x5a0b8739, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_koneko, MSX_koneko, msx_msx)
STD_ROM_FN(MSX_koneko)

struct BurnDriver BurnDrvMSX_koneko = {
	"msx_koneko", NULL, "msx_msx", NULL, "1986",
	"Koneko no Daibouken - Chibi-chan ga Iku (Jpn)\0", NULL, "Casio", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_konekoRomInfo, MSX_konekoRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Kung Fu Master (Jpn)

static struct BurnRomInfo MSX_kungfumRomDesc[] = {
	{ "kung fu master (japan).rom",	0x04000, 0x08f23b3e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_kungfum, MSX_kungfum, msx_msx)
STD_ROM_FN(MSX_kungfum)

struct BurnDriver BurnDrvMSX_kungfum = {
	"msx_kungfum", NULL, "msx_msx", NULL, "1983",
	"Kung Fu Master (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_kungfumRomInfo, MSX_kungfumRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Kung Fu Master (Jpn, Alt)

static struct BurnRomInfo MSX_kungfumaRomDesc[] = {
	{ "kung fu master (japan) (alt 1).rom",	0x04000, 0x10fc1118, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_kungfuma, MSX_kungfuma, msx_msx)
STD_ROM_FN(MSX_kungfuma)

struct BurnDriver BurnDrvMSX_kungfuma = {
	"msx_kungfuma", "msx_kungfum", "msx_msx", NULL, "1983",
	"Kung Fu Master (Jpn, Alt)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_kungfumaRomInfo, MSX_kungfumaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Kung Fu Master (Jpn, Alt 2)

static struct BurnRomInfo MSX_kungfumbRomDesc[] = {
	{ "kung fu master (japan) (alt 2).rom",	0x04000, 0x103724ad, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_kungfumb, MSX_kungfumb, msx_msx)
STD_ROM_FN(MSX_kungfumb)

struct BurnDriver BurnDrvMSX_kungfumb = {
	"msx_kungfumb", "msx_kungfum", "msx_msx", NULL, "1983",
	"Kung Fu Master (Jpn, Alt 2)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_kungfumbRomInfo, MSX_kungfumbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Kung-Fu Taikun (Jpn)

static struct BurnRomInfo MSX_kungfutRomDesc[] = {
	{ "kung fu taigun (japan).rom",	0x04000, 0xc1aaf8cb, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_kungfut, MSX_kungfut, msx_msx)
STD_ROM_FN(MSX_kungfut)

struct BurnDriver BurnDrvMSX_kungfut = {
	"msx_kungfut", NULL, "msx_msx", NULL, "1985",
	"Kung-Fu Taikun (Jpn)\0", NULL, "Toshiba EMI", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_kungfutRomInfo, MSX_kungfutRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Kung-Fu Taikun (Jpn, Alt)

static struct BurnRomInfo MSX_kungfutaRomDesc[] = {
	{ "kung fu taigun (japan) (alt 1).rom",	0x04001, 0x8c30be94, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_kungfuta, MSX_kungfuta, msx_msx)
STD_ROM_FN(MSX_kungfuta)

struct BurnDriver BurnDrvMSX_kungfuta = {
	"msx_kungfuta", "msx_kungfut", "msx_msx", NULL, "1985",
	"Kung-Fu Taikun (Jpn, Alt)\0", NULL, "Toshiba EMI", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_kungfutaRomInfo, MSX_kungfutaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Kung-Fu Taikun (Kor)

static struct BurnRomInfo MSX_kungfutkRomDesc[] = {
	{ "kimpo.rom",	0x04000, 0xcb43bfba, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_kungfutk, MSX_kungfutk, msx_msx)
STD_ROM_FN(MSX_kungfutk)

struct BurnDriver BurnDrvMSX_kungfutk = {
	"msx_kungfutk", "msx_kungfut", "msx_msx", NULL, "198?",
	"Kung-Fu Taikun (Kor)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_kungfutkRomInfo, MSX_kungfutkRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Laptick 2 (Jpn)

static struct BurnRomInfo MSX_laptick2RomDesc[] = {
	{ "laptick 2 (japan).rom",	0x08000, 0x091fe438, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_laptick2, MSX_laptick2, msx_msx)
STD_ROM_FN(MSX_laptick2)

struct BurnDriver BurnDrvMSX_laptick2 = {
	"msx_laptick2", NULL, "msx_msx", NULL, "1986",
	"Laptick 2 (Jpn)\0", NULL, "dB-Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_laptick2RomInfo, MSX_laptick2RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Legendly Knight - Daemaseong (Kor)

static struct BurnRomInfo MSX_legendkRomDesc[] = {
	{ "legendly knight (korea).rom",	0x20000, 0x8e0f0638, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_legendk, MSX_legendk, msx_msx)
STD_ROM_FN(MSX_legendk)

struct BurnDriver BurnDrvMSX_legendk = {
	"msx_legendk", NULL, "msx_msx", NULL, "1988",
	"Legendly Knight - Daemaseong (Kor)\0", NULL, "Topia", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_KONAMI, GBF_MISC, 0,
	MSXGetZipName, MSX_legendkRomInfo, MSX_legendkRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Legendly Knight - Daemaseong (Kor, Alt)

static struct BurnRomInfo MSX_legendkaRomDesc[] = {
	{ "legendly knight (korea) (alt 1).rom",	0x20000, 0xd6635d68, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_legendka, MSX_legendka, msx_msx)
STD_ROM_FN(MSX_legendka)

struct BurnDriver BurnDrvMSX_legendka = {
	"msx_legendka", "msx_legendk", "msx_msx", NULL, "1988",
	"Legendly Knight - Daemaseong (Kor, Alt)\0", NULL, "Topia", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII8, GBF_MISC, 0,
	MSXGetZipName, MSX_legendkaRomInfo, MSX_legendkaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Lode Runner (Jpn)

static struct BurnRomInfo MSX_ldrunRomDesc[] = {
	{ "lode runner (japan).rom",	0x08000, 0xadc58732, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_ldrun, MSX_ldrun, msx_msx)
STD_ROM_FN(MSX_ldrun)

struct BurnDriver BurnDrvMSX_ldrun = {
	"msx_ldrun", NULL, "msx_msx", NULL, "1983",
	"Lode Runner (Jpn)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_ldrunRomInfo, MSX_ldrunRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Lode Runner (Jpn, Alt)

static struct BurnRomInfo MSX_ldrunaRomDesc[] = {
	{ "lode runner (japan) (alt 1).rom",	0x08000, 0x25b70773, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_ldruna, MSX_ldruna, msx_msx)
STD_ROM_FN(MSX_ldruna)

struct BurnDriver BurnDrvMSX_ldruna = {
	"msx_ldruna", "msx_ldrun", "msx_msx", NULL, "1983",
	"Lode Runner (Jpn, Alt)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_ldrunaRomInfo, MSX_ldrunaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Lode Runner II (Jpn)

static struct BurnRomInfo MSX_ldrun2RomDesc[] = {
	{ "lode runner ii (japan).rom",	0x08000, 0x32ab8b20, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_ldrun2, MSX_ldrun2, msx_msx)
STD_ROM_FN(MSX_ldrun2)

struct BurnDriver BurnDrvMSX_ldrun2 = {
	"msx_ldrun2", NULL, "msx_msx", NULL, "1985",
	"Lode Runner II (Jpn)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_ldrun2RomInfo, MSX_ldrun2RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Lord Over (Jpn)

static struct BurnRomInfo MSX_lordoverRomDesc[] = {
	{ "lord over (japan).rom",	0x04000, 0xab5e42fe, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_lordover, MSX_lordover, msx_msx)
STD_ROM_FN(MSX_lordover)

struct BurnDriver BurnDrvMSX_lordover = {
	"msx_lordover", NULL, "msx_msx", NULL, "1984",
	"Lord Over (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_lordoverRomInfo, MSX_lordoverRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Lot Lot (Jpn)

static struct BurnRomInfo MSX_lotlotRomDesc[] = {
	{ "lot lot (japan).rom",	0x08000, 0xc1679aeb, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_lotlot, MSX_lotlot, msx_msx)
STD_ROM_FN(MSX_lotlot)

struct BurnDriver BurnDrvMSX_lotlot = {
	"msx_lotlot", NULL, "msx_msx", NULL, "1985",
	"Lot Lot (Jpn)\0", NULL, "Tokuma Shoten", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_lotlotRomInfo, MSX_lotlotRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Lunar Ball (Jpn)

static struct BurnRomInfo MSX_lunarbalRomDesc[] = {
	{ "lunar ball (japan).rom",	0x08000, 0xd8a116d8, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_lunarbal, MSX_lunarbal, msx_msx)
STD_ROM_FN(MSX_lunarbal)

struct BurnDriver BurnDrvMSX_lunarbal = {
	"msx_lunarbal", NULL, "msx_msx", NULL, "1985",
	"Lunar Ball (Jpn)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_lunarbalRomInfo, MSX_lunarbalRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Machinegun Joe vs The Mafia (Jpn)

static struct BurnRomInfo MSX_mgunjoeRomDesc[] = {
	{ "machinegun joe vs the mafia (japan).rom",	0x04000, 0x628f033d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mgunjoe, MSX_mgunjoe, msx_msx)
STD_ROM_FN(MSX_mgunjoe)

struct BurnDriver BurnDrvMSX_mgunjoe = {
	"msx_mgunjoe", NULL, "msx_msx", NULL, "1984",
	"Machinegun Joe vs The Mafia (Jpn)\0", NULL, "Hudson Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_mgunjoeRomInfo, MSX_mgunjoeRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Choujikuu Yousai Macross (Jpn)

static struct BurnRomInfo MSX_macrossRomDesc[] = {
	{ "macross (japan).rom",	0x08000, 0x6e742024, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_macross, MSX_macross, msx_msx)
STD_ROM_FN(MSX_macross)

struct BurnDriver BurnDrvMSX_macross = {
	"msx_macross", NULL, "msx_msx", NULL, "1985",
	"Choujikuu Yousai Macross (Jpn)\0", NULL, "Bothtec", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_macrossRomInfo, MSX_macrossRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Choujikuu Yousai Macross (Jpn, Alt)

static struct BurnRomInfo MSX_macrossaRomDesc[] = {
	{ "macross (japan) (alt 1).rom",	0x08000, 0x86ccf417, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_macrossa, MSX_macrossa, msx_msx)
STD_ROM_FN(MSX_macrossa)

struct BurnDriver BurnDrvMSX_macrossa = {
	"msx_macrossa", "msx_macross", "msx_msx", NULL, "1985",
	"Choujikuu Yousai Macross (Jpn, Alt)\0", NULL, "Bothtec", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_macrossaRomInfo, MSX_macrossaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Magical Kid Wiz (Jpn)

static struct BurnRomInfo MSX_mkidwizRomDesc[] = {
	{ "magical kid wiz (japan).rom",	0x08000, 0x8c2f2c59, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mkidwiz, MSX_mkidwiz, msx_msx)
STD_ROM_FN(MSX_mkidwiz)

struct BurnDriver BurnDrvMSX_mkidwiz = {
	"msx_mkidwiz", NULL, "msx_msx", NULL, "1986",
	"Magical Kid Wiz (Jpn)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_mkidwizRomInfo, MSX_mkidwizRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Magical Kid Wiz (Jpn, Alt)

static struct BurnRomInfo MSX_mkidwizaRomDesc[] = {
	{ "magical kid wiz (japan) (alt 1).rom",	0x08000, 0x2c2f5b6c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mkidwiza, MSX_mkidwiza, msx_msx)
STD_ROM_FN(MSX_mkidwiza)

struct BurnDriver BurnDrvMSX_mkidwiza = {
	"msx_mkidwiza", "msx_mkidwiz", "msx_msx", NULL, "1986",
	"Magical Kid Wiz (Jpn, Alt)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_mkidwizaRomInfo, MSX_mkidwizaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Magical Kid Wiz (Jpn, Alt 2)

static struct BurnRomInfo MSX_mkidwizbRomDesc[] = {
	{ "magical kid wiz (japan) (alt 2).rom",	0x08000, 0x74d6a1b1, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mkidwizb, MSX_mkidwizb, msx_msx)
STD_ROM_FN(MSX_mkidwizb)

struct BurnDriver BurnDrvMSX_mkidwizb = {
	"msx_mkidwizb", "msx_mkidwiz", "msx_msx", NULL, "1986",
	"Magical Kid Wiz (Jpn, Alt 2)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_mkidwizbRomInfo, MSX_mkidwizbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Magical Tree (Jpn)

static struct BurnRomInfo MSX_magtreeRomDesc[] = {
	{ "magical tree (japan).rom",	0x04000, 0x827038a2, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_magtree, MSX_magtree, msx_msx)
STD_ROM_FN(MSX_magtree)

struct BurnDriver BurnDrvMSX_magtree = {
	"msx_magtree", NULL, "msx_msx", NULL, "1985",
	"Magical Tree (Jpn)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_magtreeRomInfo, MSX_magtreeRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Magical Tree (Jpn, Alt)

static struct BurnRomInfo MSX_magtreeaRomDesc[] = {
	{ "magical tree (japan) (alt 1).rom",	0x04000, 0xf3606c66, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_magtreea, MSX_magtreea, msx_msx)
STD_ROM_FN(MSX_magtreea)

struct BurnDriver BurnDrvMSX_magtreea = {
	"msx_magtreea", "msx_magtree", "msx_msx", NULL, "1985",
	"Magical Tree (Jpn, Alt)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_magtreeaRomInfo, MSX_magtreeaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Magnum Kiki Ippatsu - Empire City 1931 (Jpn)

static struct BurnRomInfo MSX_empcityRomDesc[] = {
	{ "magnum prohibition 1931 (japan).rom",	0x20000, 0xd446ba1e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_empcity, MSX_empcity, msx_msx)
STD_ROM_FN(MSX_empcity)

struct BurnDriver BurnDrvMSX_empcity = {
	"msx_empcity", NULL, "msx_msx", NULL, "1988",
	"Magnum Kiki Ippatsu - Empire City 1931 (Jpn)\0", NULL, "Toshiba EMI", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII16, GBF_MISC, 0,
	MSXGetZipName, MSX_empcityRomInfo, MSX_empcityRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Magnum Kiki Ippatsu - Empire City 1931 (Jpn, Alt)

static struct BurnRomInfo MSX_empcityaRomDesc[] = {
	{ "magnum prohibition 1931 (japan) (alt 1).rom",	0x20000, 0x2a8bbb4d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_empcitya, MSX_empcitya, msx_msx)
STD_ROM_FN(MSX_empcitya)

struct BurnDriver BurnDrvMSX_empcitya = {
	"msx_empcitya", "msx_empcity", "msx_msx", NULL, "1988",
	"Magnum Kiki Ippatsu - Empire City 1931 (Jpn, Alt)\0", NULL, "Toshiba EMI", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII16, GBF_MISC, 0,
	MSXGetZipName, MSX_empcityaRomInfo, MSX_empcityaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Manes (Jpn)

static struct BurnRomInfo MSX_manesRomDesc[] = {
	{ "manes (japan).rom",	0x04000, 0xb7002f08, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_manes, MSX_manes, msx_msx)
STD_ROM_FN(MSX_manes)

struct BurnDriver BurnDrvMSX_manes = {
	"msx_manes", NULL, "msx_msx", NULL, "1984",
	"Manes (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_manesRomInfo, MSX_manesRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Manes (Jpn, Alt)

static struct BurnRomInfo MSX_manesaRomDesc[] = {
	{ "manes (japan) (alt 1).rom",	0x04000, 0xa97a19ec, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_manesa, MSX_manesa, msx_msx)
STD_ROM_FN(MSX_manesa)

struct BurnDriver BurnDrvMSX_manesa = {
	"msx_manesa", "msx_manes", "msx_msx", NULL, "1984",
	"Manes (Jpn, Alt)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_manesaRomInfo, MSX_manesaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Manes (Jpn, Alt 2)

static struct BurnRomInfo MSX_manesbRomDesc[] = {
	{ "manes (japan) (alt 2).rom",	0x04000, 0xaf275ca8, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_manesb, MSX_manesb, msx_msx)
STD_ROM_FN(MSX_manesb)

struct BurnDriver BurnDrvMSX_manesb = {
	"msx_manesb", "msx_manes", "msx_msx", NULL, "1984",
	"Manes (Jpn, Alt 2)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_manesbRomInfo, MSX_manesbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Mappy (Jpn)

static struct BurnRomInfo MSX_mappyRomDesc[] = {
	{ "mappy (japan).rom",	0x04000, 0x3b702e9a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mappy, MSX_mappy, msx_msx)
STD_ROM_FN(MSX_mappy)

struct BurnDriver BurnDrvMSX_mappy = {
	"msx_mappy", NULL, "msx_msx", NULL, "1984",
	"Mappy (Jpn)\0", NULL, "Namcot", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_mappyRomInfo, MSX_mappyRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Mappy (Jpn, Alt)

static struct BurnRomInfo MSX_mappyaRomDesc[] = {
	{ "mappy (japan) (alt 1).rom",	0x08000, 0xa0358870, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mappya, MSX_mappya, msx_msx)
STD_ROM_FN(MSX_mappya)

struct BurnDriver BurnDrvMSX_mappya = {
	"msx_mappya", "msx_mappy", "msx_msx", NULL, "1984",
	"Mappy (Jpn, Alt)\0", NULL, "Namcot", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_mappyaRomInfo, MSX_mappyaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Mappy (Jpn, Alt 2)

static struct BurnRomInfo MSX_mappybRomDesc[] = {
	{ "mappy (japan) (alt 2).rom",	0x08000, 0xa006ad6b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mappyb, MSX_mappyb, msx_msx)
STD_ROM_FN(MSX_mappyb)

struct BurnDriver BurnDrvMSX_mappyb = {
	"msx_mappyb", "msx_mappy", "msx_msx", NULL, "1984",
	"Mappy (Jpn, Alt 2)\0", NULL, "Namcot", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_mappybRomInfo, MSX_mappybRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Marine Battle (Jpn)

static struct BurnRomInfo MSX_marinbatRomDesc[] = {
	{ "marine battle (japan).rom",	0x04000, 0x24923bdb, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_marinbat, MSX_marinbat, msx_msx)
STD_ROM_FN(MSX_marinbat)

struct BurnDriver BurnDrvMSX_marinbat = {
	"msx_marinbat", NULL, "msx_msx", NULL, "1983",
	"Marine Battle (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_BASIC, GBF_MISC, 0,
	MSXGetZipName, MSX_marinbatRomInfo, MSX_marinbatRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Mashou no Yakata - Gabalin (Jpn)

static struct BurnRomInfo MSX_gabalinRomDesc[] = {
	{ "mashou no tachi goblin (japan).rom",	0x10000, 0xd34d74f7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_gabalin, MSX_gabalin, msx_msx)
STD_ROM_FN(MSX_gabalin)

struct BurnDriver BurnDrvMSX_gabalin = {
	"msx_gabalin", NULL, "msx_msx", NULL, "1987",
	"Mashou no Yakata - Goblin (Jpn)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_gabalinRomInfo, MSX_gabalinRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Mashou no Yakata - Gabalin (Jpn, Alt)

static struct BurnRomInfo MSX_gabalinaRomDesc[] = {
	{ "mashou no tachi goblin (japan) (alt 1).rom",	0x10000, 0x6ef086eb, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_gabalina, MSX_gabalina, msx_msx)
STD_ROM_FN(MSX_gabalina)

struct BurnDriver BurnDrvMSX_gabalina = {
	"msx_gabalina", "msx_gabalin", "msx_msx", NULL, "1987",
	"Mashou no Yakata - Gabalin (Jpn, Alt)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_gabalinaRomInfo, MSX_gabalinaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Megalopolis SOS (Jpn)

static struct BurnRomInfo MSX_megalsosRomDesc[] = {
	{ "megalopolis sos (japan).rom",	0x04000, 0xcf744d2e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_megalsos, MSX_megalsos, msx_msx)
STD_ROM_FN(MSX_megalsos)

struct BurnDriver BurnDrvMSX_megalsos = {
	"msx_megalsos", NULL, "msx_msx", NULL, "1983",
	"Megalopolis SOS (Jpn)\0", NULL, "Paxon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_megalsosRomInfo, MSX_megalsosRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Megalopolis SOS (Jpn, Alt)

static struct BurnRomInfo MSX_megalsosaRomDesc[] = {
	{ "megalopolis sos (japan) (alt 1).rom",	0x04000, 0x02b17e2a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_megalsosa, MSX_megalsosa, msx_msx)
STD_ROM_FN(MSX_megalsosa)

struct BurnDriver BurnDrvMSX_megalsosa = {
	"msx_megalsosa", "msx_megalsos", "msx_msx", NULL, "1983",
	"Megalopolis SOS (Jpn, Alt)\0", NULL, "Paxon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_megalsosaRomInfo, MSX_megalsosaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Megalopolis SOS (Jpn, Alt 2)

static struct BurnRomInfo MSX_megalsosbRomDesc[] = {
	{ "megalopolis sos (japan) (alt 2).rom",	0x04000, 0x07e85697, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_megalsosb, MSX_megalsosb, msx_msx)
STD_ROM_FN(MSX_megalsosb)

struct BurnDriver BurnDrvMSX_megalsosb = {
	"msx_megalsosb", "msx_megalsos", "msx_msx", NULL, "1983",
	"Megalopolis SOS (Jpn, Alt 2)\0", NULL, "Paxon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_megalsosbRomInfo, MSX_megalsosbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Meikyuu Shinwa (Jpn, Alt)

static struct BurnRomInfo MSX_meikyushaRomDesc[] = {
	{ "meikyuu shinwa (japan) (alt 1).rom",	0x20000, 0x1d1ec602, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_meikyusha, MSX_meikyusha, msx_msx)
STD_ROM_FN(MSX_meikyusha)

struct BurnDriver BurnDrvMSX_meikyusha = {
	"msx_meikyusha", NULL, "msx_msx", NULL, "1986",
	"Meikyuu Shinwa (Jpn, Alt)\0", NULL, "HAL Kenkyuujo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII16, GBF_MISC, 0,
	MSXGetZipName, MSX_meikyushaRomInfo, MSX_meikyushaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Midnight Brothers (Jpn)

static struct BurnRomInfo MSX_midbrosRomDesc[] = {
	{ "midnight brothers (japan).rom",	0x08000, 0xce2882f6, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_midbros, MSX_midbros, msx_msx)
STD_ROM_FN(MSX_midbros)

struct BurnDriver BurnDrvMSX_midbros = {
	"msx_midbros", NULL, "msx_msx", NULL, "1986",
	"Midnight Brothers (Jpn)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_midbrosRomInfo, MSX_midbrosRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Midnight Brothers (Jpn, Alt)

static struct BurnRomInfo MSX_midbrosaRomDesc[] = {
	{ "midnight brothers (japan) (alt 1).rom",	0x08000, 0x7bc61bd5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_midbrosa, MSX_midbrosa, msx_msx)
STD_ROM_FN(MSX_midbrosa)

struct BurnDriver BurnDrvMSX_midbrosa = {
	"msx_midbrosa", "msx_midbros", "msx_msx", NULL, "1986",
	"Midnight Brothers (Jpn, Alt)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_midbrosaRomInfo, MSX_midbrosaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Midnight Building (Jpn)

static struct BurnRomInfo MSX_midbuildRomDesc[] = {
	{ "midnight building (japan).rom",	0x04000, 0x7b5282e4, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_midbuild, MSX_midbuild, msx_msx)
STD_ROM_FN(MSX_midbuild)

struct BurnDriver BurnDrvMSX_midbuild = {
	"msx_midbuild", NULL, "msx_msx", NULL, "1983",
	"Midnight Building (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_midbuildRomInfo, MSX_midbuildRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Midway (Jpn)

static struct BurnRomInfo MSX_midwayRomDesc[] = {
	{ "midway (japan).rom",	0x04000, 0x40e2f32a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_midway, MSX_midway, msx_msx)
STD_ROM_FN(MSX_midway)

struct BurnDriver BurnDrvMSX_midway = {
	"msx_midway", NULL, "msx_msx", NULL, "1983",
	"Midway (Jpn)\0", NULL, "Magicsoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_midwayRomInfo, MSX_midwayRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Mitsume Ga Tooru - The Three-Eyed One Comes Here (Jpn)

static struct BurnRomInfo MSX_mitsumgtRomDesc[] = {
	{ "mitsumega toohru - three-eyed one comes here, the (japan).rom",	0x20000, 0x4faccae0, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mitsumgt, MSX_mitsumgt, msx_msx)
STD_ROM_FN(MSX_mitsumgt)

struct BurnDriver BurnDrvMSX_mitsumgt = {
	"msx_mitsumgt", NULL, "msx_msx", NULL, "1989",
	"Mitsume Ga Tooru - The Three-Eyed One Comes Here (Jpn)\0", NULL, "Natsume", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII8, GBF_MISC, 0,
	MSXGetZipName, MSX_mitsumgtRomInfo, MSX_mitsumgtRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Moai no Hihou (Jpn)

static struct BurnRomInfo MSX_moaihihoRomDesc[] = {
	{ "moai no hibou (japan).rom",	0x08000, 0x0427df83, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_moaihiho, MSX_moaihiho, msx_msx)
STD_ROM_FN(MSX_moaihiho)

struct BurnDriver BurnDrvMSX_moaihiho = {
	"msx_moaihiho", NULL, "msx_msx", NULL, "1986",
	"Moai no Hihou (Jpn)\0", NULL, "Casio", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_moaihihoRomInfo, MSX_moaihihoRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Moai no Hihou (Kor)

static struct BurnRomInfo MSX_moaihihokRomDesc[] = {
	{ "moai no hibou (japan) (alt 1).rom",	0x08000, 0x0b3061fc, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_moaihihok, MSX_moaihihok, msx_msx)
STD_ROM_FN(MSX_moaihihok)

struct BurnDriver BurnDrvMSX_moaihihok = {
	"msx_moaihihok", "msx_moaihiho", "msx_msx", NULL, "1986?",
	"Moai no Hihou (Kor)\0", NULL, "Static Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_moaihihokRomInfo, MSX_moaihihokRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Mobile Planet Suthirus - Approach from the Westgate (Jpn)

static struct BurnRomInfo MSX_suthirRomDesc[] = {
	{ "mobile planet suthirus - approach from the westgate (japan).rom",	0x08000, 0xeba19259, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_suthir, MSX_suthir, msx_msx)
STD_ROM_FN(MSX_suthir)

struct BurnDriver BurnDrvMSX_suthir = {
	"msx_suthir", NULL, "msx_msx", NULL, "1986",
	"Mobile Planet Suthirus - Approach from the Westgate (Jpn)\0", NULL, "HAL Kenkyuujo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_suthirRomInfo, MSX_suthirRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Mobile-Suit Gundam - Last Shooting (Jpn)

static struct BurnRomInfo MSX_gundamRomDesc[] = {
	{ "mobile-suit gundam - last shooting (japan).rom",	0x04000, 0x1d27d31f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_gundam, MSX_gundam, msx_msx)
STD_ROM_FN(MSX_gundam)

struct BurnDriver BurnDrvMSX_gundam = {
	"msx_gundam", NULL, "msx_msx", NULL, "1984",
	"Mobile-Suit Gundam - Last Shooting (Jpn)\0", NULL, "Bandai", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_gundamRomInfo, MSX_gundamRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Mobile-Suit Gundam - Last Shooting (Jpn, Alt)

static struct BurnRomInfo MSX_gundamaRomDesc[] = {
	{ "mobile-suit gundam - last shooting (japan) (alt 1).rom",	0x08000, 0x4725206f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_gundama, MSX_gundama, msx_msx)
STD_ROM_FN(MSX_gundama)

struct BurnDriver BurnDrvMSX_gundama = {
	"msx_gundama", "msx_gundam", "msx_msx", NULL, "1984",
	"Mobile-Suit Gundam - Last Shooting (Jpn, Alt)\0", NULL, "Bandai", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_gundamaRomInfo, MSX_gundamaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Mobile-Suit Gundam - Last Shooting (Kor)

static struct BurnRomInfo MSX_gundamkRomDesc[] = {
	{ "gundam.rom",	0x08000, 0x5cef14d1, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_gundamk, MSX_gundamk, msx_msx)
STD_ROM_FN(MSX_gundamk)

struct BurnDriver BurnDrvMSX_gundamk = {
	"msx_gundamk", "msx_gundam", "msx_msx", NULL, "198?",
	"Mobile-Suit Gundam - Last Shooting (Kor)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_gundamkRomInfo, MSX_gundamkRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Mokari Makka? Bochibochi Denna! (Jpn)

static struct BurnRomInfo MSX_mokarimaRomDesc[] = {
	{ "mokarimakka (japan).rom",	0x08000, 0xd40f481d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mokarima, MSX_mokarima, msx_msx)
STD_ROM_FN(MSX_mokarima)

struct BurnDriver BurnDrvMSX_mokarima = {
	"msx_mokarima", NULL, "msx_msx", NULL, "1986",
	"Mokari Makka? Bochibochi Denna! (Jpn)\0", NULL, "Leben Pro", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_mokarimaRomInfo, MSX_mokarimaRomName, NULL, NULL, MSXInputInfo, MSXJoyCursor60hzDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Mole Mole (Bra)

static struct BurnRomInfo MSX_molemolRomDesc[] = {
	{ "mole mole (1985)(victor)[tr pt].rom",	0x08000, 0x7092ca0c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_molemol, MSX_molemol, msx_msx)
STD_ROM_FN(MSX_molemol)

struct BurnDriver BurnDrvMSX_molemol = {
	"msx_molemol", NULL, "msx_msx", NULL, "198?",
	"Mole Mole (Bra)\0", NULL, "Unknown", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_molemolRomInfo, MSX_molemolRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Mole Mole 2 (Jpn)

static struct BurnRomInfo MSX_molemol2RomDesc[] = {
	{ "mole mole 2 (japan).rom",	0x08000, 0x309f886b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_molemol2, MSX_molemol2, msx_msx)
STD_ROM_FN(MSX_molemol2)

struct BurnDriver BurnDrvMSX_molemol2 = {
	"msx_molemol2", NULL, "msx_msx", NULL, "1987",
	"Mole Mole 2 (Jpn)\0", NULL, "Victor", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_molemol2RomInfo, MSX_molemol2RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Monkey Academy (Jpn)

static struct BurnRomInfo MSX_monkeyacRomDesc[] = {
	{ "monkey academy (japan).rom",	0x04000, 0x98cd6912, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_monkeyac, MSX_monkeyac, msx_msx)
STD_ROM_FN(MSX_monkeyac)

struct BurnDriver BurnDrvMSX_monkeyac = {
	"msx_monkeyac", NULL, "msx_msx", NULL, "1984",
	"Monkey Academy (Jpn)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_monkeyacRomInfo, MSX_monkeyacRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Monkey Academy (Jpn, Alt)

static struct BurnRomInfo MSX_monkeyacaRomDesc[] = {
	{ "monkey academy (japan) (alt 1).rom",	0x04000, 0xe73c12e6, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_monkeyaca, MSX_monkeyaca, msx_msx)
STD_ROM_FN(MSX_monkeyaca)

struct BurnDriver BurnDrvMSX_monkeyaca = {
	"msx_monkeyaca", "msx_monkeyac", "msx_msx", NULL, "1984",
	"Monkey Academy (Jpn, Alt)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_monkeyacaRomInfo, MSX_monkeyacaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Monkey Academy (Jpn, Alt 2)

static struct BurnRomInfo MSX_monkeyacbRomDesc[] = {
	{ "monkey academy (japan) (alt 2).rom",	0x04000, 0x3ddcac18, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_monkeyacb, MSX_monkeyacb, msx_msx)
STD_ROM_FN(MSX_monkeyacb)

struct BurnDriver BurnDrvMSX_monkeyacb = {
	"msx_monkeyacb", "msx_monkeyac", "msx_msx", NULL, "1984",
	"Monkey Academy (Jpn, Alt 2)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_monkeyacbRomInfo, MSX_monkeyacbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Monkey Academy (Jpn, Alt 3)

static struct BurnRomInfo MSX_monkeyaccRomDesc[] = {
	{ "monkey academy (japan) (alt 3).rom",	0x04000, 0xd1896f6e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_monkeyacc, MSX_monkeyacc, msx_msx)
STD_ROM_FN(MSX_monkeyacc)

struct BurnDriver BurnDrvMSX_monkeyacc = {
	"msx_monkeyacc", "msx_monkeyac", "msx_msx", NULL, "1984",
	"Monkey Academy (Jpn, Alt 3)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_monkeyaccRomInfo, MSX_monkeyaccRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Monster's Fair (Jpn)

static struct BurnRomInfo MSX_mnstfairRomDesc[] = {
	{ "monster's fair (japan).rom",	0x08000, 0x1ba6e461, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mnstfair, MSX_mnstfair, msx_msx)
STD_ROM_FN(MSX_mnstfair)

struct BurnDriver BurnDrvMSX_mnstfair = {
	"msx_mnstfair", NULL, "msx_msx", NULL, "1986",
	"Monster's Fair (Jpn)\0", NULL, "Toho", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_mnstfairRomInfo, MSX_mnstfairRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Moon Patrol (Jpn)

static struct BurnRomInfo MSX_mpatrolRomDesc[] = {
	{ "moon patrol (japan).rom",	0x04000, 0xfebb1155, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mpatrol, MSX_mpatrol, msx_msx)
STD_ROM_FN(MSX_mpatrol)

struct BurnDriver BurnDrvMSX_mpatrol = {
	"msx_mpatrol", NULL, "msx_msx", NULL, "1984",
	"Moon Patrol (Jpn)\0", NULL, "Dempa", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_mpatrolRomInfo, MSX_mpatrolRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Moon Patrol (Kor)

static struct BurnRomInfo MSX_mpatrolkRomDesc[] = {
	{ "moon patrol (1984)(irem)[cr prosoft].rom",	0x04000, 0xd4151ce7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mpatrolk, MSX_mpatrolk, msx_msx)
STD_ROM_FN(MSX_mpatrolk)

struct BurnDriver BurnDrvMSX_mpatrolk = {
	"msx_mpatrolk", "msx_mpatrol", "msx_msx", NULL, "19??",
	"Moon Patrol (Kor)\0", NULL, "Prosoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_mpatrolkRomInfo, MSX_mpatrolkRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Moonsweeper (Jpn)

static struct BurnRomInfo MSX_moonswepRomDesc[] = {
	{ "moonsweeper (japan).rom",	0x08000, 0xf1637c31, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_moonswep, MSX_moonswep, msx_msx)
STD_ROM_FN(MSX_moonswep)

struct BurnDriver BurnDrvMSX_moonswep = {
	"msx_moonswep", NULL, "msx_msx", NULL, "1985",
	"Moonsweeper (Jpn)\0", NULL, "Toshiba EMI", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_moonswepRomInfo, MSX_moonswepRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Mopiranger (Jpn)

static struct BurnRomInfo MSX_mopirangRomDesc[] = {
	{ "mopiranger (japan).rom",	0x04000, 0x9d73e4c5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mopirang, MSX_mopirang, msx_msx)
STD_ROM_FN(MSX_mopirang)

struct BurnDriver BurnDrvMSX_mopirang = {
	"msx_mopirang", NULL, "msx_msx", NULL, "1985",
	"Mopiranger (Jpn)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_mopirangRomInfo, MSX_mopirangRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Mopiranger (Jpn, Alt)

static struct BurnRomInfo MSX_mopirangaRomDesc[] = {
	{ "mopiranger (japan) (alt 1).rom",	0x04000, 0x097f4e4a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mopiranga, MSX_mopiranga, msx_msx)
STD_ROM_FN(MSX_mopiranga)

struct BurnDriver BurnDrvMSX_mopiranga = {
	"msx_mopiranga", "msx_mopirang", "msx_msx", NULL, "1985",
	"Mopiranger (Jpn, Alt)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_mopirangaRomInfo, MSX_mopirangaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Mopiranger (Jpn, Alt 2)

static struct BurnRomInfo MSX_mopirangbRomDesc[] = {
	{ "mopiranger (japan) (alt 2).rom",	0x04000, 0x213e623b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mopirangb, MSX_mopirangb, msx_msx)
STD_ROM_FN(MSX_mopirangb)

struct BurnDriver BurnDrvMSX_mopirangb = {
	"msx_mopirangb", "msx_mopirang", "msx_msx", NULL, "1985",
	"Mopiranger (Jpn, Alt 2)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_mopirangbRomInfo, MSX_mopirangbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Mopiranger (Jpn, Alt 3)

static struct BurnRomInfo MSX_mopirangcRomDesc[] = {
	{ "mopiranger (japan) (alt 3).rom",	0x04000, 0x73d0ce5a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mopirangc, MSX_mopirangc, msx_msx)
STD_ROM_FN(MSX_mopirangc)

struct BurnDriver BurnDrvMSX_mopirangc = {
	"msx_mopirangc", "msx_mopirang", "msx_msx", NULL, "1985",
	"Mopiranger (Jpn, Alt 3)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_mopirangcRomInfo, MSX_mopirangcRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Mopiranger (Jpn, Alt 4)

static struct BurnRomInfo MSX_mopirangdRomDesc[] = {
	{ "mopiranger (japan) (alt 4).rom",	0x04000, 0x6135bd9a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mopirangd, MSX_mopirangd, msx_msx)
STD_ROM_FN(MSX_mopirangd)

struct BurnDriver BurnDrvMSX_mopirangd = {
	"msx_mopirangd", "msx_mopirang", "msx_msx", NULL, "1985",
	"Mopiranger (Jpn, Alt 4)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_mopirangdRomInfo, MSX_mopirangdRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Mopiranger (Kor)

static struct BurnRomInfo MSX_mopirangkRomDesc[] = {
	{ "mopirang.rom",	0x08000, 0x46bfe597, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mopirangk, MSX_mopirangk, msx_msx)
STD_ROM_FN(MSX_mopirangk)

struct BurnDriver BurnDrvMSX_mopirangk = {
	"msx_mopirangk", "msx_mopirang", "msx_msx", NULL, "198?",
	"Mopiranger (Kor)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_mopirangkRomInfo, MSX_mopirangkRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Morita Kazuo no Othello (Jpn)

static struct BurnRomInfo MSX_moritaotRomDesc[] = {
	{ "morita kazuo no othello (japan).rom",	0x08000, 0xa94d7d08, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_moritaot, MSX_moritaot, msx_msx)
STD_ROM_FN(MSX_moritaot)

struct BurnDriver BurnDrvMSX_moritaot = {
	"msx_moritaot", NULL, "msx_msx", NULL, "1986",
	"Morita Kazuo no Othello (Jpn)\0", NULL, "Toshiba EMI", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_moritaotRomInfo, MSX_moritaotRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Mouser (Jpn)

static struct BurnRomInfo MSX_mouserRomDesc[] = {
	{ "mouser (japan).rom",	0x04000, 0x06a64527, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mouser, MSX_mouser, msx_msx)
STD_ROM_FN(MSX_mouser)

struct BurnDriver BurnDrvMSX_mouser = {
	"msx_mouser", NULL, "msx_msx", NULL, "1983",
	"Mouser (Jpn)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_mouserRomInfo, MSX_mouserRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Mr. Chin (Jpn)

static struct BurnRomInfo MSX_mrchinRomDesc[] = {
	{ "mr. chin (japan).rom",	0x02000, 0x414d29bc, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mrchin, MSX_mrchin, msx_msx)
STD_ROM_FN(MSX_mrchin)

struct BurnDriver BurnDrvMSX_mrchin = {
	"msx_mrchin", NULL, "msx_msx", NULL, "1984",
	"Mr. Chin (Jpn)\0", NULL, "HAL Kenkyuujo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_mrchinRomInfo, MSX_mrchinRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Mr. Chin (Kor)

static struct BurnRomInfo MSX_mrchinkRomDesc[] = {
	{ "mrchin.rom",	0x08000, 0x68c2426a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mrchink, MSX_mrchink, msx_msx)
STD_ROM_FN(MSX_mrchink)

struct BurnDriver BurnDrvMSX_mrchink = {
	"msx_mrchink", "msx_mrchin", "msx_msx", NULL, "198?",
	"Mr. Chin (Kor)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_mrchinkRomInfo, MSX_mrchinkRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Mr. Do (Jpn)

static struct BurnRomInfo MSX_mrdoRomDesc[] = {
	{ "mr. do (japan).rom",	0x02000, 0x4098d71d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mrdo, MSX_mrdo, msx_msx)
STD_ROM_FN(MSX_mrdo)

struct BurnDriver BurnDrvMSX_mrdo = {
	"msx_mrdo", NULL, "msx_msx", NULL, "1984",
	"Mr. Do (Jpn)\0", NULL, "Nihon Columbia", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_mrdoRomInfo, MSX_mrdoRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Mr. Do (Kor)

static struct BurnRomInfo MSX_mrdokRomDesc[] = {
	{ "extra.rom",	0x08000, 0x121c7d23, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mrdok, MSX_mrdok, msx_msx)
STD_ROM_FN(MSX_mrdok)

struct BurnDriver BurnDrvMSX_mrdok = {
	"msx_mrdok", "msx_mrdo", "msx_msx", NULL, "198?",
	"Mr. Do (Kor)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_mrdokRomInfo, MSX_mrdokRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Mr. Do! vs Unicorns (Jpn)

static struct BurnRomInfo MSX_mrdovsunRomDesc[] = {
	{ "mr. do vs unicorns (japan).rom",	0x04000, 0x09090135, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mrdovsun, MSX_mrdovsun, msx_msx)
STD_ROM_FN(MSX_mrdovsun)

struct BurnDriver BurnDrvMSX_mrdovsun = {
	"msx_mrdovsun", NULL, "msx_msx", NULL, "1984",
	"Mr. Do! vs Unicorns (Jpn)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_mrdovsunRomInfo, MSX_mrdovsunRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Mr. Do's Wild Ride (Jpn)

static struct BurnRomInfo MSX_mrdowildRomDesc[] = {
	{ "mr. do's wild ride (japan).rom",	0x04000, 0x6786a7ee, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mrdowild, MSX_mrdowild, msx_msx)
STD_ROM_FN(MSX_mrdowild)

struct BurnDriver BurnDrvMSX_mrdowild = {
	"msx_mrdowild", NULL, "msx_msx", NULL, "1985",
	"Mr. Do's Wild Ride (Jpn)\0", NULL, "Nihon Columbia", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_mrdowildRomInfo, MSX_mrdowildRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Mr. Do's Wild Ride (Jpn, Hacked?)

static struct BurnRomInfo MSX_mrdowildhRomDesc[] = {
	{ "mr. do's wildride (1985)(nippon columbia - colpax - universal)[cr angel].rom",	0x04000, 0x01ea3a27, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mrdowildh, MSX_mrdowildh, msx_msx)
STD_ROM_FN(MSX_mrdowildh)

struct BurnDriver BurnDrvMSX_mrdowildh = {
	"msx_mrdowildh", "msx_mrdowild", "msx_msx", NULL, "1985",
	"Mr. Do's Wild Ride (Jpn, Hacked?)\0", NULL, "Angel?", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_mrdowildhRomInfo, MSX_mrdowildhRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// MSX Baseball (Jpn)

static struct BurnRomInfo MSX_msxbballRomDesc[] = {
	{ "msx baseball (japan).rom",	0x04000, 0xf79d3088, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_msxbball, MSX_msxbball, msx_msx)
STD_ROM_FN(MSX_msxbball)

struct BurnDriver BurnDrvMSX_msxbball = {
	"msx_msxbball", NULL, "msx_msx", NULL, "1984",
	"MSX Baseball (Jpn)\0", NULL, "Panasoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_msxbballRomInfo, MSX_msxbballRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// MSX Baseball (Jpn, Alt)

static struct BurnRomInfo MSX_msxbballaRomDesc[] = {
	{ "msx baseball (japan) (alt 1).rom",	0x04000, 0xf928f075, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_msxbballa, MSX_msxbballa, msx_msx)
STD_ROM_FN(MSX_msxbballa)

struct BurnDriver BurnDrvMSX_msxbballa = {
	"msx_msxbballa", "msx_msxbball", "msx_msx", NULL, "1984",
	"MSX Baseball (Jpn, Alt)\0", NULL, "Panasoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_msxbballaRomInfo, MSX_msxbballaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// MSX Baseball II National (Jpn)

static struct BurnRomInfo MSX_msxbbal2RomDesc[] = {
	{ "msx baseball ii national (japan).rom",	0x04000, 0x0cb78f0e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_msxbbal2, MSX_msxbbal2, msx_msx)
STD_ROM_FN(MSX_msxbbal2)

struct BurnDriver BurnDrvMSX_msxbbal2 = {
	"msx_msxbbal2", NULL, "msx_msx", NULL, "1986",
	"MSX Baseball II National (Jpn)\0", NULL, "Panasoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_msxbbal2RomInfo, MSX_msxbbal2RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// MSX Rugby (Jpn)

static struct BurnRomInfo MSX_msxrugbyRomDesc[] = {
	{ "msx rugby (japan).rom",	0x04000, 0x61b33748, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_msxrugby, MSX_msxrugby, msx_msx)
STD_ROM_FN(MSX_msxrugby)

struct BurnDriver BurnDrvMSX_msxrugby = {
	"msx_msxrugby", NULL, "msx_msx", NULL, "1985",
	"MSX Rugby (Jpn)\0", NULL, "Panasoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_msxrugbyRomInfo, MSX_msxrugbyRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// MSX Soccer (Jpn)

static struct BurnRomInfo MSX_msxsoccrRomDesc[] = {
	{ "msx soccer (japan).rom",	0x04000, 0x6824e45d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_msxsoccr, MSX_msxsoccr, msx_msx)
STD_ROM_FN(MSX_msxsoccr)

struct BurnDriver BurnDrvMSX_msxsoccr = {
	"msx_msxsoccr", NULL, "msx_msx", NULL, "1985",
	"MSX Soccer (Jpn)\0", NULL, "Panasoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_msxsoccrRomInfo, MSX_msxsoccrRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// MSX Soccer (Jpn, Alt)

static struct BurnRomInfo MSX_msxsoccraRomDesc[] = {
	{ "msx soccer (japan) (alt 1).rom",	0x04000, 0xf31d13b9, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_msxsoccra, MSX_msxsoccra, msx_msx)
STD_ROM_FN(MSX_msxsoccra)

struct BurnDriver BurnDrvMSX_msxsoccra = {
	"msx_msxsoccra", "msx_msxsoccr", "msx_msx", NULL, "1985",
	"MSX Soccer (Jpn, Alt)\0", NULL, "Panasoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_msxsoccraRomInfo, MSX_msxsoccraRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// MSX Soccer (Kor)

static struct BurnRomInfo MSX_msxsoccrkRomDesc[] = {
	{ "msxsocce.rom",	0x08000, 0x80d26722, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_msxsoccrk, MSX_msxsoccrk, msx_msx)
STD_ROM_FN(MSX_msxsoccrk)

struct BurnDriver BurnDrvMSX_msxsoccrk = {
	"msx_msxsoccrk", "msx_msxsoccr", "msx_msx", NULL, "198?",
	"MSX Soccer (Kor)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_msxsoccrkRomInfo, MSX_msxsoccrkRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// MSX Soukoban (Jpn)

static struct BurnRomInfo MSX_sokobanRomDesc[] = {
	{ "soukoban (japan).rom",	0x04000, 0x030ddf11, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_sokoban, MSX_sokoban, msx_msx)
STD_ROM_FN(MSX_sokoban)

struct BurnDriver BurnDrvMSX_sokoban = {
	"msx_sokoban", NULL, "msx_msx", NULL, "1984",
	"MSX Soukoban (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_sokobanRomInfo, MSX_sokobanRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// MSX Soukoban (Jpn, Alt)

static struct BurnRomInfo MSX_sokobanaRomDesc[] = {
	{ "soukoban (japan) (alt 1).rom",	0x08000, 0xbc815cfa, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_sokobana, MSX_sokobana, msx_msx)
STD_ROM_FN(MSX_sokobana)

struct BurnDriver BurnDrvMSX_sokobana = {
	"msx_sokobana", "msx_sokoban", "msx_msx", NULL, "1984",
	"MSX Soukoban (Jpn, Alt)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_sokobanaRomInfo, MSX_sokobanaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// MSX Soukoban (Jpn, Alt 2)

static struct BurnRomInfo MSX_sokobanbRomDesc[] = {
	{ "soukoban (japan) (alt 2).rom",	0x04000, 0xe683af8a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_sokobanb, MSX_sokobanb, msx_msx)
STD_ROM_FN(MSX_sokobanb)

struct BurnDriver BurnDrvMSX_sokobanb = {
	"msx_sokobanb", "msx_sokoban", "msx_msx", NULL, "1984",
	"MSX Soukoban (Jpn, Alt 2)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_sokobanbRomInfo, MSX_sokobanbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Soukoban (Kor)

static struct BurnRomInfo MSX_sokobankRomDesc[] = {
	{ "soukoban (1984)(qnix).rom",	0x04000, 0xebb55d7f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_sokobank, MSX_sokobank, msx_msx)
STD_ROM_FN(MSX_sokobank)

struct BurnDriver BurnDrvMSX_sokobank = {
	"msx_sokobank", "msx_sokoban", "msx_msx", NULL, "1984?",
	"Soukoban (Kor)\0", NULL, "Qnix", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_sokobankRomInfo, MSX_sokobankRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Mugen Senshi Valis (Kor)

static struct BurnRomInfo MSX_valiskRomDesc[] = {
	{ "valis - the fantasm soldier (1987)(zemina).rom",	0x20000, 0x87361b76, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_valisk, MSX_valisk, msx_msx)
STD_ROM_FN(MSX_valisk)

struct BurnDriver BurnDrvMSX_valisk = {
	"msx_valisk", "msx_valis", "msx_msx", NULL, "1987",
	"Mugen Senshi Valis (Kor)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_KONAMI, GBF_MISC, 0,
	MSXGetZipName, MSX_valiskRomInfo, MSX_valiskRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Nemesis (Euro) ~ Gradius (Jpn)

static struct BurnRomInfo MSX_gradiusRomDesc[] = {
	{ "nemesis (japan, europe).rom",	0x20000, 0x4d44255f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_gradius, MSX_gradius, msx_msx)
STD_ROM_FN(MSX_gradius)

struct BurnDriver BurnDrvMSX_gradius = {
	"msx_gradius", NULL, "msx_msx", NULL, "1986",
	"Nemesis (Euro) ~ Gradius (Jpn)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_KONAMI, GBF_MISC, 0,
	MSXGetZipName, MSX_gradiusRomInfo, MSX_gradiusRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Nemesis (Euro) ~ Gradius (Jpn) (Alt)

static struct BurnRomInfo MSX_gradiusaRomDesc[] = {
	{ "nemesis (japan, europe) (alt 1).rom",	0x20000, 0x4dfcc009, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_gradiusa, MSX_gradiusa, msx_msx)
STD_ROM_FN(MSX_gradiusa)

struct BurnDriver BurnDrvMSX_gradiusa = {
	"msx_gradiusa", "msx_gradius", "msx_msx", NULL, "1986",
	"Nemesis (Euro) ~ Gradius (Jpn) (Alt)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_KONAMI, GBF_MISC, 0,
	MSXGetZipName, MSX_gradiusaRomInfo, MSX_gradiusaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Nemesis 2 (Euro) ~ Gradius 2 (Jpn)

static struct BurnRomInfo MSX_gradius2RomDesc[] = {
	{ "nemesis 2 (japan, europe).rom",	0x20000, 0xdb847b2b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_gradius2, MSX_gradius2, msx_msx)
STD_ROM_FN(MSX_gradius2)

struct BurnDriver BurnDrvMSX_gradius2 = {
	"msx_gradius2", NULL, "msx_msx", NULL, "1987",
	"Nemesis 2 (Euro) ~ Gradius 2 (Jpn)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_KONAMI_SCC, GBF_MISC, 0,
	MSXGetZipName, MSX_gradius2RomInfo, MSX_gradius2RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Nemesis 2 (Euro) ~ Gradius 2 (Jpn) (Alt)

static struct BurnRomInfo MSX_gradius2aRomDesc[] = {
	{ "nemesis 2 (japan, europe) (alt 1).rom",	0x20000, 0x32aba4e3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_gradius2a, MSX_gradius2a, msx_msx)
STD_ROM_FN(MSX_gradius2a)

struct BurnDriver BurnDrvMSX_gradius2a = {
	"msx_gradius2a", "msx_gradius2", "msx_msx", NULL, "1987",
	"Nemesis 2 (Euro) ~ Gradius 2 (Jpn) (Alt)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_KONAMI_SCC, GBF_MISC, 0,
	MSXGetZipName, MSX_gradius2aRomInfo, MSX_gradius2aRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Nemesis 2 (Euro) ~ Gradius 2 (Jpn) (Prototype)

static struct BurnRomInfo MSX_gradius2pRomDesc[] = {
	{ "nemesis 2 (japan, europe) (beta).rom",	0x20000, 0xdfa8c827, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_gradius2p, MSX_gradius2p, msx_msx)
STD_ROM_FN(MSX_gradius2p)

struct BurnDriver BurnDrvMSX_gradius2p = {
	"msx_gradius2p", "msx_gradius2", "msx_msx", NULL, "1987",
	"Nemesis 2 (Euro) ~ Gradius 2 (Jpn) (Prototype)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_KONAMI_SCC, GBF_MISC, 0,
	MSXGetZipName, MSX_gradius2pRomInfo, MSX_gradius2pRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Nemesis 2 (Euro) ~ Gradius 2 (Jpn) (Demo)

static struct BurnRomInfo MSX_gradius2dRomDesc[] = {
	{ "nemesis 2 (japan) (demo).rom",	0x20000, 0xc8fac21a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_gradius2d, MSX_gradius2d, msx_msx)
STD_ROM_FN(MSX_gradius2d)

struct BurnDriver BurnDrvMSX_gradius2d = {
	"msx_gradius2d", "msx_gradius2", "msx_msx", NULL, "1987",
	"Nemesis 2 (Euro) ~ Gradius 2 (Jpn) (Demo)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_KONAMI_SCC, GBF_MISC, 0,
	MSXGetZipName, MSX_gradius2dRomInfo, MSX_gradius2dRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Nemesis 3 - The Eve of Destruction (Euro) ~ Gopher no Yabou - Episode II (Jpn)

static struct BurnRomInfo MSX_nemesis3RomDesc[] = {
	{ "nemesis 3 - the eve of destruction (japan, europe).rom",	0x40000, 0x4b61ae91, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_nemesis3, MSX_nemesis3, msx_msx)
STD_ROM_FN(MSX_nemesis3)

struct BurnDriver BurnDrvMSX_nemesis3 = {
	"msx_nemesis3", NULL, "msx_msx", NULL, "1988",
	"Nemesis 3 - The Eve of Destruction (Euro) ~ Gopher no Yabou - Episode II (Jpn)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_KONAMI_SCC, GBF_MISC, 0,
	MSXGetZipName, MSX_nemesis3RomInfo, MSX_nemesis3RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Nemesis 3 - The Eve of Destruction (Euro) ~ Gopher no Yabou - Episode II (Jpn) (Alt)

static struct BurnRomInfo MSX_nemesis3aRomDesc[] = {
	{ "nemesis 3 - the eve of destruction (japan, europe) (alt 1).rom",	0x40000, 0x0db7132f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_nemesis3a, MSX_nemesis3a, msx_msx)
STD_ROM_FN(MSX_nemesis3a)

struct BurnDriver BurnDrvMSX_nemesis3a = {
	"msx_nemesis3a", "msx_nemesis3", "msx_msx", NULL, "1988",
	"Nemesis 3 - The Eve of Destruction (Euro) ~ Gopher no Yabou - Episode II (Jpn) (Alt)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_KONAMI_SCC, GBF_MISC, 0,
	MSXGetZipName, MSX_nemesis3aRomInfo, MSX_nemesis3aRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Nemesis 3 - The Eve of Destruction (Euro) ~ Gopher no Yabou - Episode II (Jpn) (Alt 2)

static struct BurnRomInfo MSX_nemesis3bRomDesc[] = {
	{ "nemesis 3 - the eve of destruction (japan, europe) (alt 2).rom",	0x40000, 0x329987bc, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_nemesis3b, MSX_nemesis3b, msx_msx)
STD_ROM_FN(MSX_nemesis3b)

struct BurnDriver BurnDrvMSX_nemesis3b = {
	"msx_nemesis3b", "msx_nemesis3", "msx_msx", NULL, "1988",
	"Nemesis 3 - The Eve of Destruction (Euro) ~ Gopher no Yabou - Episode II (Jpn) (Alt 2)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_KONAMI_SCC, GBF_MISC, 0,
	MSXGetZipName, MSX_nemesis3bRomInfo, MSX_nemesis3bRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Nessen Koushien (Jpn)

static struct BurnRomInfo MSX_nkoshienRomDesc[] = {
	{ "nessen koushiyen (japan).rom",	0x04000, 0x2afbf7d1, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_nkoshien, MSX_nkoshien, msx_msx)
STD_ROM_FN(MSX_nkoshien)

struct BurnDriver BurnDrvMSX_nkoshien = {
	"msx_nkoshien", NULL, "msx_msx", NULL, "1984",
	"Nessen Koushien (Jpn)\0", NULL, "Casio", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_nkoshienRomInfo, MSX_nkoshienRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Night Shade (Jpn)

static struct BurnRomInfo MSX_nightshdRomDesc[] = {
	{ "night shade (japan).rom",	0x08000, 0x72ddb449, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_nightshd, MSX_nightshd, msx_msx)
STD_ROM_FN(MSX_nightshd)

struct BurnDriver BurnDrvMSX_nightshd = {
	"msx_nightshd", NULL, "msx_msx", NULL, "1986",
	"Night Shade (Jpn)\0", NULL, "Nihon Dexter", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_nightshdRomInfo, MSX_nightshdRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Ninja II (Kor)

static struct BurnRomInfo MSX_ninja2RomDesc[] = {
	{ "ninja.rom",	0x08000, 0x18510ca7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_ninja2, MSX_ninja2, msx_msx)
STD_ROM_FN(MSX_ninja2)

struct BurnDriver BurnDrvMSX_ninja2 = {
	"msx_ninja2", "msx_iganinp2", "msx_msx", NULL, "198?",
	"Ninja II (Kor)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_ninja2RomInfo, MSX_ninja2RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Ninja Jajamaru-kun (Jpn)

static struct BurnRomInfo MSX_ninjajajRomDesc[] = {
	{ "ninja jajamaru-kun (japan).rom",	0x08000, 0x2a28ff97, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_ninjajaj, MSX_ninjajaj, msx_msx)
STD_ROM_FN(MSX_ninjajaj)

struct BurnDriver BurnDrvMSX_ninjajaj = {
	"msx_ninjajaj", NULL, "msx_msx", NULL, "1986",
	"Ninja Jajamaru-kun (Jpn)\0", NULL, "Jaleco", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_ninjajajRomInfo, MSX_ninjajajRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Ninja Jajamaru-kun (Kor)

static struct BurnRomInfo MSX_ninjajajkRomDesc[] = {
	{ "ninja jaja maru kun (1986)(nippon dexter)[cr prosoft].rom",	0x08000, 0x9cc8c883, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_ninjajajk, MSX_ninjajajk, msx_msx)
STD_ROM_FN(MSX_ninjajajk)

struct BurnDriver BurnDrvMSX_ninjajajk = {
	"msx_ninjajajk", "msx_ninjajaj", "msx_msx", NULL, "19??",
	"Ninja Jajamaru-kun (Kor)\0", NULL, "Prosoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_ninjajajkRomInfo, MSX_ninjajajkRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Ninja Princess (Jpn)

static struct BurnRomInfo MSX_ninjapriRomDesc[] = {
	{ "ninja princess (japan).rom",	0x08000, 0x203ef741, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_ninjapri, MSX_ninjapri, msx_msx)
STD_ROM_FN(MSX_ninjapri)

struct BurnDriver BurnDrvMSX_ninjapri = {
	"msx_ninjapri", NULL, "msx_msx", NULL, "1986",
	"Ninja Princess (Jpn)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_ninjapriRomInfo, MSX_ninjapriRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Ninja Princess (Jpn, Alt)

static struct BurnRomInfo MSX_ninjapriaRomDesc[] = {
	{ "ninja princess (japan) (alt 1).rom",	0x08000, 0x99260557, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_ninjapria, MSX_ninjapria, msx_msx)
STD_ROM_FN(MSX_ninjapria)

struct BurnDriver BurnDrvMSX_ninjapria = {
	"msx_ninjapria", "msx_ninjapri", "msx_msx", NULL, "1986",
	"Ninja Princess (Jpn, Alt)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_ninjapriaRomInfo, MSX_ninjapriaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Ninja-kun (Jpn)

static struct BurnRomInfo MSX_ninjakunRomDesc[] = {
	{ "ninjakun (japan).rom",	0x04000, 0xd388cfd1, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_ninjakun, MSX_ninjakun, msx_msx)
STD_ROM_FN(MSX_ninjakun)

struct BurnDriver BurnDrvMSX_ninjakun = {
	"msx_ninjakun", NULL, "msx_msx", NULL, "1983",
	"Ninja-kun (Jpn)\0", NULL, "Toshiba", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_ninjakunRomInfo, MSX_ninjakunRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Ninja-kun - Majou no Bouken (Jpn)

static struct BurnRomInfo MSX_ninjakmbRomDesc[] = {
	{ "ninjakun majou (japan).rom",	0x08000, 0xef339b82, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_ninjakmb, MSX_ninjakmb, msx_msx)
STD_ROM_FN(MSX_ninjakmb)

struct BurnDriver BurnDrvMSX_ninjakmb = {
	"msx_ninjakmb", NULL, "msx_msx", NULL, "1985",
	"Ninja-kun - Majou no Bouken (Jpn)\0", NULL, "Nihon Dexter", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_ninjakmbRomInfo, MSX_ninjakmbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Ninjya Kage (Jpn)

static struct BurnRomInfo MSX_ninjakagRomDesc[] = {
	{ "ninjya kage (japan).rom",	0x04000, 0xb202f481, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_ninjakag, MSX_ninjakag, msx_msx)
STD_ROM_FN(MSX_ninjakag)

struct BurnDriver BurnDrvMSX_ninjakag = {
	"msx_ninjakag", NULL, "msx_msx", NULL, "1984",
	"Ninjya Kage (Jpn)\0", NULL, "Hudson Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_ninjakagRomInfo, MSX_ninjakagRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Ninjya Kage (Jpn, Alt)

static struct BurnRomInfo MSX_ninjakagaRomDesc[] = {
	{ "ninjya kage (japan) (alt 1).rom",	0x04000, 0x56bd8018, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_ninjakaga, MSX_ninjakaga, msx_msx)
STD_ROM_FN(MSX_ninjakaga)

struct BurnDriver BurnDrvMSX_ninjakaga = {
	"msx_ninjakaga", "msx_ninjakag", "msx_msx", NULL, "1984",
	"Ninjya Kage (Jpn, Alt)\0", NULL, "Hudson Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_ninjakagaRomInfo, MSX_ninjakagaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Nuts & Milk (Jpn)

static struct BurnRomInfo MSX_nutsmilkRomDesc[] = {
	{ "nuts_milk_(1984)_(hudson).rom",	0x08000, 0x8bff4901, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_nutsmilk, MSX_nutsmilk, msx_msx)
STD_ROM_FN(MSX_nutsmilk)

struct BurnDriver BurnDrvMSX_nutsmilk = {
	"msx_nutsmilk", NULL, "msx_msx", NULL, "1983",
	"Nuts & Milk (Jpn)\0", NULL, "Hudson Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_nutsmilkRomInfo, MSX_nutsmilkRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// O'Mac Farmer (Jpn)

static struct BurnRomInfo MSX_omacfarmRomDesc[] = {
	{ "o'mac farmer (japan).rom",	0x04000, 0xb05ffed2, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_omacfarm, MSX_omacfarm, msx_msx)
STD_ROM_FN(MSX_omacfarm)

struct BurnDriver BurnDrvMSX_omacfarm = {
	"msx_omacfarm", NULL, "msx_msx", NULL, "1983",
	"O'Mac Farmer (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_omacfarmRomInfo, MSX_omacfarmRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Oil's Well (Jpn)

static struct BurnRomInfo MSX_oilswellRomDesc[] = {
	{ "oil's well (japan).rom",	0x08000, 0x3c7f3767, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_oilswell, MSX_oilswell, msx_msx)
STD_ROM_FN(MSX_oilswell)

struct BurnDriver BurnDrvMSX_oilswell = {
	"msx_oilswell", NULL, "msx_msx", NULL, "1985",
	"Oil's Well (Jpn)\0", NULL, "Comptiq", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_oilswellRomInfo, MSX_oilswellRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Oil's Well (Jpn, Alt)

static struct BurnRomInfo MSX_oilswellaRomDesc[] = {
	{ "oil's well (japan) (alt 1).rom",	0x08000, 0xaa2154fb, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_oilswella, MSX_oilswella, msx_msx)
STD_ROM_FN(MSX_oilswella)

struct BurnDriver BurnDrvMSX_oilswella = {
	"msx_oilswella", "msx_oilswell", "msx_msx", NULL, "1985",
	"Oil's Well (Jpn, Alt)\0", NULL, "Comptiq", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_oilswellaRomInfo, MSX_oilswellaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Othello (Jpn, Pony Canyon)

static struct BurnRomInfo MSX_othelloRomDesc[] = {
	{ "othello (japan).rom",	0x08000, 0xfd3921cd, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_othello, MSX_othello, msx_msx)
STD_ROM_FN(MSX_othello)

struct BurnDriver BurnDrvMSX_othello = {
	"msx_othello", NULL, "msx_msx", NULL, "1985",
	"Othello (Jpn, Pony Canyon)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_othelloRomInfo, MSX_othelloRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Oyoide Tango (Jpn)

static struct BurnRomInfo MSX_oyotangoRomDesc[] = {
	{ "oyoide tango (japan).rom",	0x02000, 0x01a24ca7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_oyotango, MSX_oyotango, msx_msx)
STD_ROM_FN(MSX_oyotango)

struct BurnDriver BurnDrvMSX_oyotango = {
	"msx_oyotango", NULL, "msx_msx", NULL, "1985",
	"Oyoide Tango (Jpn)\0", NULL, "HAL Kenkyuujo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_oyotangoRomInfo, MSX_oyotangoRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Pac-Man (Jpn)

static struct BurnRomInfo MSX_pacmanRomDesc[] = {
	{ "pac-man (japan).rom",	0x04000, 0xd74dffa2, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_pacman, MSX_pacman, msx_msx)
STD_ROM_FN(MSX_pacman)

struct BurnDriver BurnDrvMSX_pacman = {
	"msx_pacman", NULL, "msx_msx", NULL, "1984",
	"Pac-Man (Jpn)\0", NULL, "Namcot", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_pacmanRomInfo, MSX_pacmanRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Pac-Man (Jpn, Alt)

static struct BurnRomInfo MSX_pacmanaRomDesc[] = {
	{ "pac-man (japan) (alt 1).rom",	0x08000, 0x505acae7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_pacmana, MSX_pacmana, msx_msx)
STD_ROM_FN(MSX_pacmana)

struct BurnDriver BurnDrvMSX_pacmana = {
	"msx_pacmana", "msx_pacman", "msx_msx", NULL, "1984",
	"Pac-Man (Jpn, Alt)\0", NULL, "Namcot", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_pacmanaRomInfo, MSX_pacmanaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Pac-Man (Jpn, Alt 2)

static struct BurnRomInfo MSX_pacmanbRomDesc[] = {
	{ "pac-man (japan) (alt 2).rom",	0x04000, 0x926bc903, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_pacmanb, MSX_pacmanb, msx_msx)
STD_ROM_FN(MSX_pacmanb)

struct BurnDriver BurnDrvMSX_pacmanb = {
	"msx_pacmanb", "msx_pacman", "msx_msx", NULL, "1984",
	"Pac-Man (Jpn, Alt 2)\0", NULL, "Namcot", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_pacmanbRomInfo, MSX_pacmanbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Pachi Com (Jpn)

static struct BurnRomInfo MSX_pachicomRomDesc[] = {
	{ "pachi com (japan).rom",	0x08000, 0xce7f6c91, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_pachicom, MSX_pachicom, msx_msx)
STD_ROM_FN(MSX_pachicom)

struct BurnDriver BurnDrvMSX_pachicom = {
	"msx_pachicom", NULL, "msx_msx", NULL, "1985",
	"Pachi Com (Jpn)\0", NULL, "Toshiba EMI", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_pachicomRomInfo, MSX_pachicomRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Pachi Com (Kor)

static struct BurnRomInfo MSX_pachicomkRomDesc[] = {
	{ "pachicom (1985)(toshiba-emi)[cr boram soft].rom",	0x08000, 0xbc1b4322, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_pachicomk, MSX_pachicomk, msx_msx)
STD_ROM_FN(MSX_pachicomk)

struct BurnDriver BurnDrvMSX_pachicomk = {
	"msx_pachicomk", "msx_pachicom", "msx_msx", NULL, "1986",
	"Pachi Com (Kor)\0", NULL, "Boram Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_pachicomkRomInfo, MSX_pachicomkRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Pachinko-U.F.O. (Jpn)

static struct BurnRomInfo MSX_pachiufoRomDesc[] = {
	{ "casio pachinko-u.f.o. (japan).rom",	0x04000, 0x4088efaa, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_pachiufo, MSX_pachiufo, msx_msx)
STD_ROM_FN(MSX_pachiufo)

struct BurnDriver BurnDrvMSX_pachiufo = {
	"msx_pachiufo", NULL, "msx_msx", NULL, "1984",
	"Pachinko-U.F.O. (Jpn)\0", NULL, "Casio", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_pachiufoRomInfo, MSX_pachiufoRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Pai Panic (Jpn)

static struct BurnRomInfo MSX_paipanicRomDesc[] = {
	{ "pai panic (japan).rom",	0x04000, 0x6fd70773, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_paipanic, MSX_paipanic, msx_msx)
STD_ROM_FN(MSX_paipanic)

struct BurnDriver BurnDrvMSX_paipanic = {
	"msx_paipanic", NULL, "msx_msx", NULL, "1983",
	"Pai Panic (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_paipanicRomInfo, MSX_paipanicRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Pairs (Jpn)

static struct BurnRomInfo MSX_pairsRomDesc[] = {
	{ "pairs (japan).rom",	0x04000, 0x9c1c10ca, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_pairs, MSX_pairs, msx_msx)
STD_ROM_FN(MSX_pairs)

struct BurnDriver BurnDrvMSX_pairs = {
	"msx_pairs", NULL, "msx_msx", NULL, "1983",
	"Pairs (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_BASIC, GBF_MISC, 0,
	MSXGetZipName, MSX_pairsRomInfo, MSX_pairsRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Pairs (Jpn, Alt)

static struct BurnRomInfo MSX_pairsaRomDesc[] = {
	{ "pairs (japan) (alt 1).rom",	0x04000, 0x3f042540, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_pairsa, MSX_pairsa, msx_msx)
STD_ROM_FN(MSX_pairsa)

struct BurnDriver BurnDrvMSX_pairsa = {
	"msx_pairsa", "msx_pairs", "msx_msx", NULL, "1983",
	"Pairs (Jpn, Alt)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_BASIC, GBF_MISC, 0,
	MSXGetZipName, MSX_pairsaRomInfo, MSX_pairsaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Panther (Jpn)

static struct BurnRomInfo MSX_pantherRomDesc[] = {
	{ "panther (japan).rom",	0x08000, 0x33b9968c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_panther, MSX_panther, msx_msx)
STD_ROM_FN(MSX_panther)

struct BurnDriver BurnDrvMSX_panther = {
	"msx_panther", NULL, "msx_msx", NULL, "1986",
	"Panther (Jpn)\0", NULL, "Irem", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_pantherRomInfo, MSX_pantherRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Parodius - Tako wa Chikyuu wo Sukuu (Jpn)

static struct BurnRomInfo MSX_parodiusRomDesc[] = {
	{ "parodius (japan).rom",	0x20000, 0x9bb308f5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_parodius, MSX_parodius, msx_msx)
STD_ROM_FN(MSX_parodius)

struct BurnDriver BurnDrvMSX_parodius = {
	"msx_parodius", NULL, "msx_msx", NULL, "1988",
	"Parodius - Tako wa Chikyuu wo Sukuu (Jpn)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_KONAMI_SCC, GBF_MISC, 0,
	MSXGetZipName, MSX_parodiusRomInfo, MSX_parodiusRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Parodius - Tako wa Chikyuu wo Sukuu (Jpn, Alt)

static struct BurnRomInfo MSX_parodiusaRomDesc[] = {
	{ "parodius (japan) (alt 1).rom",	0x20000, 0xca21cde4, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_parodiusa, MSX_parodiusa, msx_msx)
STD_ROM_FN(MSX_parodiusa)

struct BurnDriver BurnDrvMSX_parodiusa = {
	"msx_parodiusa", "msx_parodius", "msx_msx", NULL, "1988",
	"Parodius - Tako wa Chikyuu wo Sukuu (Jpn, Alt)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_KONAMI_SCC, GBF_MISC, 0,
	MSXGetZipName, MSX_parodiusaRomInfo, MSX_parodiusaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Parodius - Tako Saves Earth (1988)(Konami)[SCC][RC-759].rom

static struct BurnRomInfo MSX_parodiuseRomDesc[] = {
	{ "parodius (eng).rom",	0x20000, 0x68dea3f0, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_parodiuse, MSX_parodiuse, msx_msx)
STD_ROM_FN(MSX_parodiuse)

struct BurnDriver BurnDrvMSX_parodiuse = {
	"msx_parodiuse", "msx_parodius", "msx_msx", NULL, "1988",
	"Parodius - Tako Saves Earth (English)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_KONAMI_SCC, GBF_MISC, 0,
	MSXGetZipName, MSX_parodiuseRomInfo, MSX_parodiuseRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Pastfinder (Jpn)

static struct BurnRomInfo MSX_pastfindRomDesc[] = {
	{ "pastfinder (japan).rom",	0x04000, 0xd6d8d1d7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_pastfind, MSX_pastfind, msx_msx)
STD_ROM_FN(MSX_pastfind)

struct BurnDriver BurnDrvMSX_pastfind = {
	"msx_pastfind", NULL, "msx_msx", NULL, "1984",
	"Pastfinder (Jpn)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_pastfindRomInfo, MSX_pastfindRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Peetan (Jpn)

static struct BurnRomInfo MSX_peetanRomDesc[] = {
	{ "peetan (japan).rom",	0x02000, 0x941069ae, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_peetan, MSX_peetan, msx_msx)
STD_ROM_FN(MSX_peetan)

struct BurnDriver BurnDrvMSX_peetan = {
	"msx_peetan", NULL, "msx_msx", NULL, "1984",
	"Peetan (Jpn)\0", NULL, "Nihon Columbia", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_peetanRomInfo, MSX_peetanRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Pegasus (Jpn)

static struct BurnRomInfo MSX_pegasusRomDesc[] = {
	{ "pegasus (japan).rom",	0x08000, 0xc23fa0d5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_pegasus, MSX_pegasus, msx_msx)
STD_ROM_FN(MSX_pegasus)

struct BurnDriver BurnDrvMSX_pegasus = {
	"msx_pegasus", NULL, "msx_msx", NULL, "1986",
	"Pegasus (Jpn)\0", NULL, "Victor", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_pegasusRomInfo, MSX_pegasusRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Penguin Adventure (Euro) ~ Yume Tairiku Adventure (Jpn)

static struct BurnRomInfo MSX_pengadvRomDesc[] = {
	{ "penguin adventure (japan, europe).rom",	0x20000, 0x0f6418d3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_pengadv, MSX_pengadv, msx_msx)
STD_ROM_FN(MSX_pengadv)

struct BurnDriver BurnDrvMSX_pengadv = {
	"msx_pengadv", NULL, "msx_msx", NULL, "1986",
	"Penguin Adventure (Euro) ~ Yume Tairiku Adventure (Jpn)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_KONAMI, GBF_MISC, 0,
	MSXGetZipName, MSX_pengadvRomInfo, MSX_pengadvRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Yume Tairiku Adventure (Kor, Zemina)

static struct BurnRomInfo MSX_pengadvk1RomDesc[] = {
	{ "yume tairiku adventure. penguin adventure (1987)(zemina)[rc-743].rom",	0x20000, 0x80814e55, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_pengadvk1, MSX_pengadvk1, msx_msx)
STD_ROM_FN(MSX_pengadvk1)

struct BurnDriver BurnDrvMSX_pengadvk1 = {
	"msx_pengadvk1", "msx_pengadv", "msx_msx", NULL, "1987",
	"Yume Tairiku Adventure (Kor, Zemina)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_KONAMI, GBF_MISC, 0,
	MSXGetZipName, MSX_pengadvk1RomInfo, MSX_pengadvk1RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Yume Tairiku Adventure (Kor, Screen Software)

static struct BurnRomInfo MSX_pengadvk2RomDesc[] = {
	{ "yume tairiku adventure. penguin adventure (1986)(konami)[cr screen][rc-743].rom",	0x20000, 0x38c35d99, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_pengadvk2, MSX_pengadvk2, msx_msx)
STD_ROM_FN(MSX_pengadvk2)

struct BurnDriver BurnDrvMSX_pengadvk2 = {
	"msx_pengadvk2", "msx_pengadv", "msx_msx", NULL, "1988",
	"Yume Tairiku Adventure (Kor, Screen Software)\0", NULL, "Screen Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_KONAMI, GBF_MISC, 0,
	MSXGetZipName, MSX_pengadvk2RomInfo, MSX_pengadvk2RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Penguin-kun Wars (Jpn)

static struct BurnRomInfo MSX_penguinwRomDesc[] = {
	{ "penguin-kun wars (japan).rom",	0x08000, 0x6d2b3e0c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_penguinw, MSX_penguinw, msx_msx)
STD_ROM_FN(MSX_penguinw)

struct BurnDriver BurnDrvMSX_penguinw = {
	"msx_penguinw", NULL, "msx_msx", NULL, "1985",
	"Penguin-kun Wars (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_penguinwRomInfo, MSX_penguinwRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Penguin-kun Wars (Kor)

static struct BurnRomInfo MSX_penguinwkRomDesc[] = {
	{ "penguin wars (1985)(ascii)[cr prosoft].rom",	0x08000, 0x9827fad6, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_penguinwk, MSX_penguinwk, msx_msx)
STD_ROM_FN(MSX_penguinwk)

struct BurnDriver BurnDrvMSX_penguinwk = {
	"msx_penguinwk", "msx_penguinw", "msx_msx", NULL, "19??",
	"Penguin-kun Wars (Kor)\0", NULL, "Prosoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_penguinwkRomInfo, MSX_penguinwkRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Pico Pico (Jpn)

static struct BurnRomInfo MSX_picopicoRomDesc[] = {
	{ "pico pico (japan).rom",	0x02000, 0xba3e62d3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_picopico, MSX_picopico, msx_msx)
STD_ROM_FN(MSX_picopico)

struct BurnDriver BurnDrvMSX_picopico = {
	"msx_picopico", NULL, "msx_msx", NULL, "1983",
	"Pico Pico (Jpn)\0", NULL, "Microcabin", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_picopicoRomInfo, MSX_picopicoRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Pillbox (Jpn)

static struct BurnRomInfo MSX_pillboxRomDesc[] = {
	{ "pillbox (japan).rom",	0x04000, 0x436c3f29, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_pillbox, MSX_pillbox, msx_msx)
STD_ROM_FN(MSX_pillbox)

struct BurnDriver BurnDrvMSX_pillbox = {
	"msx_pillbox", NULL, "msx_msx", NULL, "1983",
	"Pillbox (Jpn)\0", NULL, "Magicsoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_pillboxRomInfo, MSX_pillboxRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Pillbox (Jpn, Alt)

static struct BurnRomInfo MSX_pillboxaRomDesc[] = {
	{ "pillbox (japan) (alt 1).rom",	0x04000, 0x3345caef, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_pillboxa, MSX_pillboxa, msx_msx)
STD_ROM_FN(MSX_pillboxa)

struct BurnDriver BurnDrvMSX_pillboxa = {
	"msx_pillboxa", "msx_pillbox", "msx_msx", NULL, "1983",
	"Pillbox (Jpn, Alt)\0", NULL, "Magicsoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_pillboxaRomInfo, MSX_pillboxaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Pinball Blaster (Euro)

static struct BurnRomInfo MSX_pinblastRomDesc[] = {
	{ "pinball blaster (europe).rom",	0x10000, 0x68fe9580, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_pinblast, MSX_pinblast, msx_msx)
STD_ROM_FN(MSX_pinblast)

struct BurnDriver BurnDrvMSX_pinblast = {
	"msx_pinblast", NULL, "msx_msx", NULL, "1988",
	"Pinball Blaster (Euro)\0", NULL, "Eurosoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII16, GBF_MISC, 0,
	MSXGetZipName, MSX_pinblastRomInfo, MSX_pinblastRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Pine Applin (Jpn)

static struct BurnRomInfo MSX_pineapplRomDesc[] = {
	{ "pine applin (japan).rom",	0x04000, 0x7411c6f2, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_pineappl, MSX_pineappl, msx_msx)
STD_ROM_FN(MSX_pineappl)

struct BurnDriver BurnDrvMSX_pineappl = {
	"msx_pineappl", NULL, "msx_msx", NULL, "1984",
	"Pine Applin (Jpn)\0", NULL, "ZAP", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_pineapplRomInfo, MSX_pineapplRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Pine Applin (Jpn, Alt)

static struct BurnRomInfo MSX_pineapplaRomDesc[] = {
	{ "pine applin (japan) (alt 1).rom",	0x04000, 0xbdcb6199, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_pineappla, MSX_pineappla, msx_msx)
STD_ROM_FN(MSX_pineappla)

struct BurnDriver BurnDrvMSX_pineappla = {
	"msx_pineappla", "msx_pineappl", "msx_msx", NULL, "1984",
	"Pine Applin (Jpn, Alt)\0", NULL, "ZAP", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_pineapplaRomInfo, MSX_pineapplaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Pinky Chase (Jpn)

static struct BurnRomInfo MSX_pinkychsRomDesc[] = {
	{ "pinky chase (japan).rom",	0x04000, 0xb5b40df0, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_pinkychs, MSX_pinkychs, msx_msx)
STD_ROM_FN(MSX_pinkychs)

struct BurnDriver BurnDrvMSX_pinkychs = {
	"msx_pinkychs", NULL, "msx_msx", NULL, "1984",
	"Pinky Chase (Jpn)\0", NULL, "Nihon Columbia", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_pinkychsRomInfo, MSX_pinkychsRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Pipi (Jpn)

static struct BurnRomInfo MSX_pipiRomDesc[] = {
	{ "pipi (japan).rom",	0x08000, 0x6f172cd8, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_pipi, MSX_pipi, msx_msx)
STD_ROM_FN(MSX_pipi)

struct BurnDriver BurnDrvMSX_pipi = {
	"msx_pipi", NULL, "msx_msx", NULL, "1985",
	"Pipi (Jpn)\0", NULL, "Nihon Dexter", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_pipiRomInfo, MSX_pipiRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Pippols (Jpn)

static struct BurnRomInfo MSX_pippolsRomDesc[] = {
	{ "pippols (japan).rom",	0x04000, 0xbe6a5e19, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_pippols, MSX_pippols, msx_msx)
STD_ROM_FN(MSX_pippols)

struct BurnDriver BurnDrvMSX_pippols = {
	"msx_pippols", NULL, "msx_msx", NULL, "1985",
	"Pippols (Jpn)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_pippolsRomInfo, MSX_pippolsRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Pippols (Jpn, Alt)

static struct BurnRomInfo MSX_pippolsaRomDesc[] = {
	{ "pippols (japan) (alt 1).rom",	0x04000, 0xf4e97ad5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_pippolsa, MSX_pippolsa, msx_msx)
STD_ROM_FN(MSX_pippolsa)

struct BurnDriver BurnDrvMSX_pippolsa = {
	"msx_pippolsa", "msx_pippols", "msx_msx", NULL, "1985",
	"Pippols (Jpn, Alt)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_pippolsaRomInfo, MSX_pippolsaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Pippols (Jpn, Alt 2)

static struct BurnRomInfo MSX_pippolsbRomDesc[] = {
	{ "pippols (japan) (alt 2).rom",	0x04000, 0xd5fe3564, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_pippolsb, MSX_pippolsb, msx_msx)
STD_ROM_FN(MSX_pippolsb)

struct BurnDriver BurnDrvMSX_pippolsb = {
	"msx_pippolsb", "msx_pippols", "msx_msx", NULL, "1985",
	"Pippols (Jpn, Alt 2)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_pippolsbRomInfo, MSX_pippolsbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Pippols (Jpn, Alt 3)

static struct BurnRomInfo MSX_pippolscRomDesc[] = {
	{ "pippols (japan) (alt 3).rom",	0x04000, 0xeab63f24, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_pippolsc, MSX_pippolsc, msx_msx)
STD_ROM_FN(MSX_pippolsc)

struct BurnDriver BurnDrvMSX_pippolsc = {
	"msx_pippolsc", "msx_pippols", "msx_msx", NULL, "1985",
	"Pippols (Jpn, Alt 3)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_pippolscRomInfo, MSX_pippolscRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Pitfall II - Lost Caverns (Jpn)

static struct BurnRomInfo MSX_pitfall2RomDesc[] = {
	{ "pitfall ii - lost caverns (japan).rom",	0x04000, 0xd307a7b8, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_pitfall2, MSX_pitfall2, msx_msx)
STD_ROM_FN(MSX_pitfall2)

struct BurnDriver BurnDrvMSX_pitfall2 = {
	"msx_pitfall2", NULL, "msx_msx", NULL, "1984",
	"Pitfall II - Lost Caverns (Jpn)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_pitfall2RomInfo, MSX_pitfall2RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Pitfall II - Lost Caverns (Jpn, Alt)

static struct BurnRomInfo MSX_pitfall2aRomDesc[] = {
	{ "pitfall ii - lost caverns (japan) (alt 1).rom",	0x04000, 0x71c59868, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_pitfall2a, MSX_pitfall2a, msx_msx)
STD_ROM_FN(MSX_pitfall2a)

struct BurnDriver BurnDrvMSX_pitfall2a = {
	"msx_pitfall2a", "msx_pitfall2", "msx_msx", NULL, "1984",
	"Pitfall II - Lost Caverns (Jpn, Alt)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_pitfall2aRomInfo, MSX_pitfall2aRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Pitfall! (Jpn)

static struct BurnRomInfo MSX_pitfallRomDesc[] = {
	{ "pitfall! (japan).rom",	0x04000, 0x5a009c55, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_pitfall, MSX_pitfall, msx_msx)
STD_ROM_FN(MSX_pitfall)

struct BurnDriver BurnDrvMSX_pitfall = {
	"msx_pitfall", NULL, "msx_msx", NULL, "1984",
	"Pitfall! (Jpn)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_pitfallRomInfo, MSX_pitfallRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Pitfall! (Jpn, Alt)

static struct BurnRomInfo MSX_pitfallaRomDesc[] = {
	{ "pitfall! (japan) (alt 1).rom",	0x04000, 0x930aeb2c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_pitfalla, MSX_pitfalla, msx_msx)
STD_ROM_FN(MSX_pitfalla)

struct BurnDriver BurnDrvMSX_pitfalla = {
	"msx_pitfalla", "msx_pitfall", "msx_msx", NULL, "1984",
	"Pitfall! (Jpn, Alt)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_pitfallaRomInfo, MSX_pitfallaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Pitfall! (Jpn, Alt 2)

static struct BurnRomInfo MSX_pitfallbRomDesc[] = {
	{ "pitfall! (japan) (alt 2).rom",	0x04000, 0x2cb24473, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_pitfallb, MSX_pitfallb, msx_msx)
STD_ROM_FN(MSX_pitfallb)

struct BurnDriver BurnDrvMSX_pitfallb = {
	"msx_pitfallb", "msx_pitfall", "msx_msx", NULL, "1984",
	"Pitfall! (Jpn, Alt 2)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_pitfallbRomInfo, MSX_pitfallbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Play Ball (Jpn)

static struct BurnRomInfo MSX_playballRomDesc[] = {
	{ "play ball (japan).rom",	0x08000, 0xd178833b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_playball, MSX_playball, msx_msx)
STD_ROM_FN(MSX_playball)

struct BurnDriver BurnDrvMSX_playball = {
	"msx_playball", NULL, "msx_msx", NULL, "1986",
	"Play Ball (Jpn)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_playballRomInfo, MSX_playballRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// The Police Story (Jpn)

static struct BurnRomInfo MSX_policestRomDesc[] = {
	{ "police story, the (japan).rom",	0x08000, 0x7867d044, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_policest, MSX_policest, msx_msx)
STD_ROM_FN(MSX_policest)

struct BurnDriver BurnDrvMSX_policest = {
	"msx_policest", NULL, "msx_msx", NULL, "1986",
	"The Police Story (Jpn)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_policestRomInfo, MSX_policestRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// The Police Story (Jpn, Alt)

static struct BurnRomInfo MSX_policestaRomDesc[] = {
	{ "police story, the (japan) (alt 1).rom",	0x08000, 0xe4554b51, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_policesta, MSX_policesta, msx_msx)
STD_ROM_FN(MSX_policesta)

struct BurnDriver BurnDrvMSX_policesta = {
	"msx_policesta", "msx_policest", "msx_msx", NULL, "1986",
	"The Police Story (Jpn, Alt)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_policestaRomInfo, MSX_policestaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Pooyan (Jpn)

static struct BurnRomInfo MSX_pooyanRomDesc[] = {
	{ "pooyan (japan).rom",	0x04000, 0x558a09f6, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_pooyan, MSX_pooyan, msx_msx)
STD_ROM_FN(MSX_pooyan)

struct BurnDriver BurnDrvMSX_pooyan = {
	"msx_pooyan", NULL, "msx_msx", NULL, "1985",
	"Pooyan (Jpn)\0", NULL, "Hudson Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_pooyanRomInfo, MSX_pooyanRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Poppaq the Fish (Jpn)

static struct BurnRomInfo MSX_poppaqRomDesc[] = {
	{ "poppaq the fish (japan).rom",	0x04000, 0xbd387377, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_poppaq, MSX_poppaq, msx_msx)
STD_ROM_FN(MSX_poppaq)

struct BurnDriver BurnDrvMSX_poppaq = {
	"msx_poppaq", NULL, "msx_msx", NULL, "1984",
	"Poppaq the Fish (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_poppaqRomInfo, MSX_poppaqRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Poppaq the Fish (Kor)

static struct BurnRomInfo MSX_poppaqkRomDesc[] = {
	{ "poppaq the fish (1986)(ascii)[cr boram soft].rom",	0x04000, 0xcc36e779, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_poppaqk, MSX_poppaqk, msx_msx)
STD_ROM_FN(MSX_poppaqk)

struct BurnDriver BurnDrvMSX_poppaqk = {
	"msx_poppaqk", "msx_poppaq", "msx_msx", NULL, "1986",
	"Poppaq the Fish (Kor)\0", NULL, "Boram Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_poppaqkRomInfo, MSX_poppaqkRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// The Price of Magik (Euro)

static struct BurnRomInfo MSX_pricemagRomDesc[] = {
	{ "price of magik, the (europe).rom",	0x10000, 0xb43e7881, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_pricemag, MSX_pricemag, msx_msx)
STD_ROM_FN(MSX_pricemag)

struct BurnDriver BurnDrvMSX_pricemag = {
	"msx_pricemag", NULL, "msx_msx", NULL, "1986",
	"The Price of Magik (Euro)\0", NULL, "Level 9 Computing", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII16, GBF_MISC, 0,
	MSXGetZipName, MSX_pricemagRomInfo, MSX_pricemagRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Professional Baseball (Jpn)

static struct BurnRomInfo MSX_profbbRomDesc[] = {
	{ "professional baseball (japan).rom",	0x08000, 0x8ca4cd58, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_profbb, MSX_profbb, msx_msx)
STD_ROM_FN(MSX_profbb)

struct BurnDriver BurnDrvMSX_profbb = {
	"msx_profbb", NULL, "msx_msx", NULL, "1986",
	"Professional Baseball (Jpn)\0", NULL, "Technopolis Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_profbbRomInfo, MSX_profbbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// The Protector (Jpn)

static struct BurnRomInfo MSX_protectrRomDesc[] = {
	{ "protector, the (japan).rom",	0x04000, 0x5747e69d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_protectr, MSX_protectr, msx_msx)
STD_ROM_FN(MSX_protectr)

struct BurnDriver BurnDrvMSX_protectr = {
	"msx_protectr", NULL, "msx_msx", NULL, "1985",
	"The Protector (Jpn)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_protectrRomInfo, MSX_protectrRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Puznic

static struct BurnRomInfo MSX_puznicRomDesc[] = {
	{ "puznic.rom",	0x08000, 0xac28fa71, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_puznic, MSX_puznic, msx_msx)
STD_ROM_FN(MSX_puznic)

struct BurnDriver BurnDrvMSX_puznic = {
	"msx_puznic", NULL, "msx_msx", NULL, "1990",
	"Puznic\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_puznicRomInfo, MSX_puznicRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Puzzle Panic (Jpn)

static struct BurnRomInfo MSX_puzpanicRomDesc[] = {
	{ "puzzle panic (japan).rom",	0x08000, 0x1313c0c7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_puzpanic, MSX_puzpanic, msx_msx)
STD_ROM_FN(MSX_puzpanic)

struct BurnDriver BurnDrvMSX_puzpanic = {
	"msx_puzpanic", NULL, "msx_msx", NULL, "1986",
	"Puzzle Panic (Jpn)\0", NULL, "System Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_puzpanicRomInfo, MSX_puzpanicRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Pyramid Warp (Jpn)

static struct BurnRomInfo MSX_pyramidwRomDesc[] = {
	{ "pyramid warp (japan).rom",	0x02000, 0x0aec8ddb, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_pyramidw, MSX_pyramidw, msx_msx)
STD_ROM_FN(MSX_pyramidw)

struct BurnDriver BurnDrvMSX_pyramidw = {
	"msx_pyramidw", NULL, "msx_msx", NULL, "1983",
	"Pyramid Warp (Jpn)\0", NULL, "T&E Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_pyramidwRomInfo, MSX_pyramidwRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Q*bert (Jpn)

static struct BurnRomInfo MSX_qbertRomDesc[] = {
	{ "q-bert (japan).rom",	0x08000, 0x0e988f0e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_qbert, MSX_qbert, msx_msx)
STD_ROM_FN(MSX_qbert)

struct BurnDriver BurnDrvMSX_qbert = {
	"msx_qbert", NULL, "msx_msx", NULL, "1986",
	"Q*bert (Jpn)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_qbertRomInfo, MSX_qbertRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Q*bert (Jpn, Alt)

static struct BurnRomInfo MSX_qbertaRomDesc[] = {
	{ "q-bert (japan) (alt 1).rom",	0x08000, 0xa112532b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_qberta, MSX_qberta, msx_msx)
STD_ROM_FN(MSX_qberta)

struct BurnDriver BurnDrvMSX_qberta = {
	"msx_qberta", "msx_qbert", "msx_msx", NULL, "1986",
	"Q*bert (Jpn, Alt)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_qbertaRomInfo, MSX_qbertaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Queen's Golf (Jpn)

static struct BurnRomInfo MSX_queenglfRomDesc[] = {
	{ "queen's golf (japan).rom",	0x04000, 0x40eef5cd, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_queenglf, MSX_queenglf, msx_msx)
STD_ROM_FN(MSX_queenglf)

struct BurnDriver BurnDrvMSX_queenglf = {
	"msx_queenglf", NULL, "msx_msx", NULL, "1984",
	"Queen's Golf (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_queenglfRomInfo, MSX_queenglfRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Queen's Golf (Jpn, Alt)

static struct BurnRomInfo MSX_queenglfaRomDesc[] = {
	{ "queen's golf (japan) (alt 2).rom",	0x04000, 0x9c4787d7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_queenglfa, MSX_queenglfa, msx_msx)
STD_ROM_FN(MSX_queenglfa)

struct BurnDriver BurnDrvMSX_queenglfa = {
	"msx_queenglfa", "msx_queenglf", "msx_msx", NULL, "1984",
	"Queen's Golf (Jpn, Alt)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_queenglfaRomInfo, MSX_queenglfaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Queen's Golf (Jpn, Hacked?)

static struct BurnRomInfo MSX_queenglfbRomDesc[] = {
	{ "queen's golf (japan) (alt 1).rom",	0x04000, 0x27b78cb3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_queenglfb, MSX_queenglfb, msx_msx)
STD_ROM_FN(MSX_queenglfb)

struct BurnDriver BurnDrvMSX_queenglfb = {
	"msx_queenglfb", "msx_queenglf", "msx_msx", NULL, "1984",
	"Queen's Golf (Jpn, Hacked?)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_queenglfbRomInfo, MSX_queenglfbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Raid on Bungeling Bay (Jpn)

static struct BurnRomInfo MSX_raidbungRomDesc[] = {
	{ "raid on bungeling bay (japan).rom",	0x08000, 0xa87a666d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_raidbung, MSX_raidbung, msx_msx)
STD_ROM_FN(MSX_raidbung)

struct BurnDriver BurnDrvMSX_raidbung = {
	"msx_raidbung", NULL, "msx_msx", NULL, "1984",
	"Raid on Bungeling Bay (Jpn)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_raidbungRomInfo, MSX_raidbungRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Rally-X (Jpn)

static struct BurnRomInfo MSX_rallyxRomDesc[] = {
	{ "rally-x (japan).rom",	0x04000, 0x63413493, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_rallyx, MSX_rallyx, msx_msx)
STD_ROM_FN(MSX_rallyx)

struct BurnDriver BurnDrvMSX_rallyx = {
	"msx_rallyx", NULL, "msx_msx", NULL, "1984",
	"Rally-X (Jpn)\0", NULL, "Namcot", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_rallyxRomInfo, MSX_rallyxRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Rally-X (Jpn, Alt)

static struct BurnRomInfo MSX_rallyxaRomDesc[] = {
	{ "rally-x (japan) (alt 1).rom",	0x04000, 0x9fd2f1dc, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_rallyxa, MSX_rallyxa, msx_msx)
STD_ROM_FN(MSX_rallyxa)

struct BurnDriver BurnDrvMSX_rallyxa = {
	"msx_rallyxa", "msx_rallyx", "msx_msx", NULL, "1984",
	"Rally-X (Jpn, Alt)\0", NULL, "Namcot", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_rallyxaRomInfo, MSX_rallyxaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Rally-X (Jpn, Alt 2)

static struct BurnRomInfo MSX_rallyxbRomDesc[] = {
	{ "rally-x (japan) (alt 2).rom",	0x08000, 0x2b5cb04e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_rallyxb, MSX_rallyxb, msx_msx)
STD_ROM_FN(MSX_rallyxb)

struct BurnDriver BurnDrvMSX_rallyxb = {
	"msx_rallyxb", "msx_rallyx", "msx_msx", NULL, "1984",
	"Rally-X (Jpn, Alt 2)\0", NULL, "Namcot", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_rallyxbRomInfo, MSX_rallyxbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Rally-X (Kor)

static struct BurnRomInfo MSX_rallyxkRomDesc[] = {
	{ "rally-x.rom",	0x08000, 0xcf49b8f5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_rallyxk, MSX_rallyxk, msx_msx)
STD_ROM_FN(MSX_rallyxk)

struct BurnDriver BurnDrvMSX_rallyxk = {
	"msx_rallyxk", "msx_rallyx", "msx_msx", NULL, "198?",
	"Rally-X (Kor)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_rallyxkRomInfo, MSX_rallyxkRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Rambo (Jpn)

static struct BurnRomInfo MSX_ramboRomDesc[] = {
	{ "rambo (japan).rom",	0x08000, 0x6a2e3726, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_rambo, MSX_rambo, msx_msx)
STD_ROM_FN(MSX_rambo)

struct BurnDriver BurnDrvMSX_rambo = {
	"msx_rambo", NULL, "msx_msx", NULL, "1985",
	"Rambo (Jpn)\0", NULL, "Pack-In-Video", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_ramboRomInfo, MSX_ramboRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Rambo (Jpn, Alt)

static struct BurnRomInfo MSX_ramboaRomDesc[] = {
	{ "rambo (japan) (alt 1).rom",	0x08000, 0xdba4377b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_ramboa, MSX_ramboa, msx_msx)
STD_ROM_FN(MSX_ramboa)

struct BurnDriver BurnDrvMSX_ramboa = {
	"msx_ramboa", "msx_rambo", "msx_msx", NULL, "1985",
	"Rambo (Jpn, Alt)\0", NULL, "Pack-In-Video", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_ramboaRomInfo, MSX_ramboaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Rambo (Jpn, Alt 2)

static struct BurnRomInfo MSX_rambobRomDesc[] = {
	{ "rambo (japan) (alt 2).rom",	0x08000, 0x0859f662, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_rambob, MSX_rambob, msx_msx)
STD_ROM_FN(MSX_rambob)

struct BurnDriver BurnDrvMSX_rambob = {
	"msx_rambob", "msx_rambo", "msx_msx", NULL, "1985",
	"Rambo (Jpn, Alt 2)\0", NULL, "Pack-In-Video", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_rambobRomInfo, MSX_rambobRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Rambo (Jpn, Alt 3)

static struct BurnRomInfo MSX_rambocRomDesc[] = {
	{ "rambo (japan) (alt 3).rom",	0x08000, 0x2236ddf6, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_ramboc, MSX_ramboc, msx_msx)
STD_ROM_FN(MSX_ramboc)

struct BurnDriver BurnDrvMSX_ramboc = {
	"msx_ramboc", "msx_rambo", "msx_msx", NULL, "1985",
	"Rambo (Jpn, Alt 3)\0", NULL, "Pack-In-Video", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_rambocRomInfo, MSX_rambocRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Real Tennis (Jpn)

static struct BurnRomInfo MSX_realtennRomDesc[] = {
	{ "real tennis (japan).rom",	0x02000, 0x25fe441c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_realtenn, MSX_realtenn, msx_msx)
STD_ROM_FN(MSX_realtenn)

struct BurnDriver BurnDrvMSX_realtenn = {
	"msx_realtenn", NULL, "msx_msx", NULL, "1983",
	"Real Tennis (Jpn)\0", NULL, "Takara", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_realtennRomInfo, MSX_realtennRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Red Zone (Jpn)

static struct BurnRomInfo MSX_redzoneRomDesc[] = {
	{ "red zone (japan).rom",	0x04000, 0xe9b5b6ff, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_redzone, MSX_redzone, msx_msx)
STD_ROM_FN(MSX_redzone)

struct BurnDriver BurnDrvMSX_redzone = {
	"msx_redzone", NULL, "msx_msx", NULL, "1985",
	"Red Zone (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_redzoneRomInfo, MSX_redzoneRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Red Zone (Jpn, Alt)

static struct BurnRomInfo MSX_redzoneaRomDesc[] = {
	{ "red zone (japan) (alt 1).rom",	0x04000, 0x0c2da50f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_redzonea, MSX_redzonea, msx_msx)
STD_ROM_FN(MSX_redzonea)

struct BurnDriver BurnDrvMSX_redzonea = {
	"msx_redzonea", "msx_redzone", "msx_msx", NULL, "1985",
	"Red Zone (Jpn, Alt)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_redzoneaRomInfo, MSX_redzoneaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Rise Out from Dungeons (Jpn)

static struct BurnRomInfo MSX_risedungRomDesc[] = {
	{ "rise out from dungeons (japan).rom",	0x04000, 0x6a6d37cf, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_risedung, MSX_risedung, msx_msx)
STD_ROM_FN(MSX_risedung)

struct BurnDriver BurnDrvMSX_risedung = {
	"msx_risedung", NULL, "msx_msx", NULL, "1983",
	"Rise Out from Dungeons (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_risedungRomInfo, MSX_risedungRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Rise Out from Dungeons (Jpn, Alt)

static struct BurnRomInfo MSX_risedungaRomDesc[] = {
	{ "rise out from dungeons (japan) (alt 1).rom",	0x04000, 0x01043328, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_risedunga, MSX_risedunga, msx_msx)
STD_ROM_FN(MSX_risedunga)

struct BurnDriver BurnDrvMSX_risedunga = {
	"msx_risedunga", "msx_risedung", "msx_msx", NULL, "1983",
	"Rise Out from Dungeons (Jpn, Alt)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_risedungaRomInfo, MSX_risedungaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// River Raid (Jpn)

static struct BurnRomInfo MSX_riveraidRomDesc[] = {
	{ "river raid (japan).rom",	0x04000, 0x2fc1d75b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_riveraid, MSX_riveraid, msx_msx)
STD_ROM_FN(MSX_riveraid)

struct BurnDriver BurnDrvMSX_riveraid = {
	"msx_riveraid", NULL, "msx_msx", NULL, "1985",
	"River Raid (Jpn)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_riveraidRomInfo, MSX_riveraidRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Road Fighter (Jpn)

static struct BurnRomInfo MSX_roadfghtRomDesc[] = {
	{ "road fighter (japan).rom",	0x04000, 0x01ddb68f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_roadfght, MSX_roadfght, msx_msx)
STD_ROM_FN(MSX_roadfght)

struct BurnDriver BurnDrvMSX_roadfght = {
	"msx_roadfght", NULL, "msx_msx", NULL, "1985",
	"Road Fighter (Jpn)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_roadfghtRomInfo, MSX_roadfghtRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Road Fighter (Jpn, Alt)

static struct BurnRomInfo MSX_roadfghtaRomDesc[] = {
	{ "road fighter (japan) (alt 1).rom",	0x04000, 0xcb82d8c9, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_roadfghta, MSX_roadfghta, msx_msx)
STD_ROM_FN(MSX_roadfghta)

struct BurnDriver BurnDrvMSX_roadfghta = {
	"msx_roadfghta", "msx_roadfght", "msx_msx", NULL, "1985",
	"Road Fighter (Jpn, Alt)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_roadfghtaRomInfo, MSX_roadfghtaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// RoboCop

static struct BurnRomInfo MSX_seicorobocopRomDesc[] = {
	{ "sieco_robocop_(1992)(sieco).rom",	0x20000, 0x4628ef05, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_seicorobocop, MSX_seicorobocop, msx_msx)
STD_ROM_FN(MSX_seicorobocop)

struct BurnDriver BurnDrvMSX_seicorobocop = {
	"msx_seicorobocop", NULL, "msx_msx", NULL, "1992",
	"RoboCop\0", NULL, "Sieco", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_KONAMI, GBF_MISC, 0,
	MSXGetZipName, MSX_seicorobocopRomInfo, MSX_seicorobocopRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Robofrog (Jpn)

static struct BurnRomInfo MSX_robofrogRomDesc[] = {
	{ "robofrog (japan).rom",	0x04000, 0x99ddb974, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_robofrog, MSX_robofrog, msx_msx)
STD_ROM_FN(MSX_robofrog)

struct BurnDriver BurnDrvMSX_robofrog = {
	"msx_robofrog", NULL, "msx_msx", NULL, "1985",
	"Robofrog (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_robofrogRomInfo, MSX_robofrogRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Robofrog (Jpn, Alt)

static struct BurnRomInfo MSX_robofrogaRomDesc[] = {
	{ "robofrog (japan) (alt 1).rom",	0x04000, 0x82e47a43, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_robofroga, MSX_robofroga, msx_msx)
STD_ROM_FN(MSX_robofroga)

struct BurnDriver BurnDrvMSX_robofroga = {
	"msx_robofroga", "msx_robofrog", "msx_msx", NULL, "1985",
	"Robofrog (Jpn, Alt)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_robofrogaRomInfo, MSX_robofrogaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Rock'n Bolt (Jpn)

static struct BurnRomInfo MSX_rockboltRomDesc[] = {
	{ "rock'n bolt (japan).rom",	0x04000, 0x430e5789, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_rockbolt, MSX_rockbolt, msx_msx)
STD_ROM_FN(MSX_rockbolt)

struct BurnDriver BurnDrvMSX_rockbolt = {
	"msx_rockbolt", NULL, "msx_msx", NULL, "1985",
	"Rock'n Bolt (Jpn)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_rockboltRomInfo, MSX_rockboltRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Roger Rubbish (Euro)

static struct BurnRomInfo MSX_rogerrubRomDesc[] = {
	{ "roger rubbish (europe).rom",	0x04000, 0x452556ce, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_rogerrub, MSX_rogerrub, msx_msx)
STD_ROM_FN(MSX_rogerrub)

struct BurnDriver BurnDrvMSX_rogerrub = {
	"msx_rogerrub", NULL, "msx_msx", NULL, "1985",
	"Roger Rubbish (Euro)\0", NULL, "Spectravideo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_rogerrubRomInfo, MSX_rogerrubRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Roger Rubbish (Euro, Alt)

static struct BurnRomInfo MSX_rogerrubaRomDesc[] = {
	{ "roger rubbish (europe) (alt 1).rom",	0x02000, 0x33056633, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_rogerruba, MSX_rogerruba, msx_msx)
STD_ROM_FN(MSX_rogerruba)

struct BurnDriver BurnDrvMSX_rogerruba = {
	"msx_rogerruba", "msx_rogerrub", "msx_msx", NULL, "1985",
	"Roger Rubbish (Euro, Alt)\0", NULL, "Spectravideo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_rogerrubaRomInfo, MSX_rogerrubaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Roller Ball (Jpn)

static struct BurnRomInfo MSX_rollerblRomDesc[] = {
	{ "roller ball (japan).rom",	0x04000, 0x56200fef, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_rollerbl, MSX_rollerbl, msx_msx)
STD_ROM_FN(MSX_rollerbl)

struct BurnDriver BurnDrvMSX_rollerbl = {
	"msx_rollerbl", NULL, "msx_msx", NULL, "1984",
	"Roller Ball (Jpn)\0", NULL, "HAL Kenkyuujo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_rollerblRomInfo, MSX_rollerblRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Roller Ball (Jpn, Alt)

static struct BurnRomInfo MSX_rollerblaRomDesc[] = {
	{ "roller ball (japan) (alt 1).rom",	0x04000, 0x798fa044, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_rollerbla, MSX_rollerbla, msx_msx)
STD_ROM_FN(MSX_rollerbla)

struct BurnDriver BurnDrvMSX_rollerbla = {
	"msx_rollerbla", "msx_rollerbl", "msx_msx", NULL, "1984",
	"Roller Ball (Jpn, Alt)\0", NULL, "HAL Kenkyuujo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_rollerblaRomInfo, MSX_rollerblaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Rotors (Jpn)

static struct BurnRomInfo MSX_rotorsRomDesc[] = {
	{ "rotors (japan).rom",	0x04000, 0x1cdb462e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_rotors, MSX_rotors, msx_msx)
STD_ROM_FN(MSX_rotors)

struct BurnDriver BurnDrvMSX_rotors = {
	"msx_rotors", NULL, "msx_msx", NULL, "1984",
	"Rotors (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_BASIC, GBF_MISC, 0,
	MSXGetZipName, MSX_rotorsRomInfo, MSX_rotorsRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Salamander (Jpn)

static struct BurnRomInfo MSX_salamandRomDesc[] = {
	{ "salamander (japan).rom",	0x20000, 0xc36f559d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_salamand, MSX_salamand, msx_msx)
STD_ROM_FN(MSX_salamand)

struct BurnDriver BurnDrvMSX_salamand = {
	"msx_salamand", NULL, "msx_msx", NULL, "1987",
	"Salamander (Jpn)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_KONAMI_SCC, GBF_MISC, 0,
	MSXGetZipName, MSX_salamandRomInfo, MSX_salamandRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Salamander - Operation X (Kor)

static struct BurnRomInfo MSX_salamandkRomDesc[] = {
	{ "salamander - operation x (1988)(zemina)[rc-758].rom",	0x20000, 0x40d19bf6, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_salamandk, MSX_salamandk, msx_msx)
STD_ROM_FN(MSX_salamandk)

struct BurnDriver BurnDrvMSX_salamandk = {
	"msx_salamandk", "msx_salamand", "msx_msx", NULL, "1988",
	"Salamander - Operation X (Kor)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_KONAMI_SCC, GBF_MISC, 0,
	MSXGetZipName, MSX_salamandkRomInfo, MSX_salamandkRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Sasa (Jpn)

static struct BurnRomInfo MSX_sasaRomDesc[] = {
	{ "sasa (japan).rom",	0x04000, 0x7faf00c0, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_sasa, MSX_sasa, msx_msx)
STD_ROM_FN(MSX_sasa)

struct BurnDriver BurnDrvMSX_sasa = {
	"msx_sasa", NULL, "msx_msx", NULL, "1984",
	"Sasa (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_sasaRomInfo, MSX_sasaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Sasa (Jpn, Alt)

static struct BurnRomInfo MSX_sasaaRomDesc[] = {
	{ "sasa (japan) (alt 1).rom",	0x04000, 0xae6f517d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_sasaa, MSX_sasaa, msx_msx)
STD_ROM_FN(MSX_sasaa)

struct BurnDriver BurnDrvMSX_sasaa = {
	"msx_sasaa", "msx_sasa", "msx_msx", NULL, "1984",
	"Sasa (Jpn, Alt)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_sasaaRomInfo, MSX_sasaaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Saurusland (Jpn)

static struct BurnRomInfo MSX_saurusRomDesc[] = {
	{ "saurus land (japan).rom",	0x04000, 0x5f2fe556, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_saurus, MSX_saurus, msx_msx)
STD_ROM_FN(MSX_saurus)

struct BurnDriver BurnDrvMSX_saurus = {
	"msx_saurus", NULL, "msx_msx", NULL, "1983",
	"Saurusland (Jpn)\0", NULL, "Nihon Columbia", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_saurusRomInfo, MSX_saurusRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Scarlet 7 - The Mightiest Women (Jpn)

static struct BurnRomInfo MSX_scarlet7RomDesc[] = {
	{ "scarlet 7 - the mightiest women (japan).rom",	0x08000, 0xc2ed4c08, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_scarlet7, MSX_scarlet7, msx_msx)
STD_ROM_FN(MSX_scarlet7)

struct BurnDriver BurnDrvMSX_scarlet7 = {
	"msx_scarlet7", NULL, "msx_msx", NULL, "1986",
	"Scarlet 7 - The Mightiest Women (Jpn)\0", NULL, "Toshiba EMI", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_scarlet7RomInfo, MSX_scarlet7RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Scion (Jpn)

static struct BurnRomInfo MSX_scionRomDesc[] = {
	{ "scion (japan).rom",	0x04000, 0xba3a8ea1, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_scion, MSX_scion, msx_msx)
STD_ROM_FN(MSX_scion)

struct BurnDriver BurnDrvMSX_scion = {
	"msx_scion", NULL, "msx_msx", NULL, "1985",
	"Scion (Jpn)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_scionRomInfo, MSX_scionRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Scramble Eggs (Jpn)

static struct BurnRomInfo MSX_scrameggRomDesc[] = {
	{ "scramble eggs (japan).rom",	0x02000, 0x02dc77e8, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_scramegg, MSX_scramegg, msx_msx)
STD_ROM_FN(MSX_scramegg)

struct BurnDriver BurnDrvMSX_scramegg = {
	"msx_scramegg", NULL, "msx_msx", NULL, "1983",
	"Scramble Eggs (Jpn)\0", NULL, "Ample Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_scrameggRomInfo, MSX_scrameggRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Sea Hunter (Euro)

static struct BurnRomInfo MSX_seahuntrRomDesc[] = {
	{ "sea hunter (europe).rom",	0x04000, 0x1fd18174, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_seahuntr, MSX_seahuntr, msx_msx)
STD_ROM_FN(MSX_seahuntr)

struct BurnDriver BurnDrvMSX_seahuntr = {
	"msx_seahuntr", NULL, "msx_msx", NULL, "1984",
	"Sea Hunter (Euro)\0", NULL, "DynaData?", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_seahuntrRomInfo, MSX_seahuntrRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Seiken Achou (Jpn)

static struct BurnRomInfo MSX_seikachoRomDesc[] = {
	{ "kung fu acho (japan).rom",	0x08000, 0x999dd794, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_seikacho, MSX_seikacho, msx_msx)
STD_ROM_FN(MSX_seikacho)

struct BurnDriver BurnDrvMSX_seikacho = {
	"msx_seikacho", NULL, "msx_msx", NULL, "1985",
	"Kung Fu Master. Seiken Achou (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_seikachoRomInfo, MSX_seikachoRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Seiken Achou (Jpn, Alt)

static struct BurnRomInfo MSX_seikachoaRomDesc[] = {
	{ "kung fu acho (japan) (alt 1).rom",	0x08000, 0x0da11df8, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_seikachoa, MSX_seikachoa, msx_msx)
STD_ROM_FN(MSX_seikachoa)

struct BurnDriver BurnDrvMSX_seikachoa = {
	"msx_seikachoa", "msx_seikacho", "msx_msx", NULL, "1985",
	"Kung Fu Master. Seiken Achou (Jpn, Alt)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_seikachoaRomInfo, MSX_seikachoaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Seiken Achou (Kor)

static struct BurnRomInfo MSX_seikachokRomDesc[] = {
	{ "karateka.rom",	0x08000, 0x6adeadf5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_seikachok, MSX_seikachok, msx_msx)
STD_ROM_FN(MSX_seikachok)

struct BurnDriver BurnDrvMSX_seikachok = {
	"msx_seikachok", "msx_seikacho", "msx_msx", NULL, "198?",
	"Kung Fu Master. Seiken Achou (Kor)\0", NULL, "Clover", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_seikachokRomInfo, MSX_seikachokRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Senjyo (Jpn)

static struct BurnRomInfo MSX_senjyoRomDesc[] = {
	{ "senjyo (japan).rom",	0x04000, 0x126bc4cd, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_senjyo, MSX_senjyo, msx_msx)
STD_ROM_FN(MSX_senjyo)

struct BurnDriver BurnDrvMSX_senjyo = {
	"msx_senjyo", NULL, "msx_msx", NULL, "1984",
	"Senjyo (Jpn)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_senjyoRomInfo, MSX_senjyoRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Senjyo (Jpn, Alt)

static struct BurnRomInfo MSX_senjyoaRomDesc[] = {
	{ "senjyo (japan) (alt 1).rom",	0x04000, 0x7d558b04, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_senjyoa, MSX_senjyoa, msx_msx)
STD_ROM_FN(MSX_senjyoa)

struct BurnDriver BurnDrvMSX_senjyoa = {
	"msx_senjyoa", "msx_senjyo", "msx_msx", NULL, "1984",
	"Senjyo (Jpn, Alt)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_senjyoaRomInfo, MSX_senjyoaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Sewer Sam (Jpn)

static struct BurnRomInfo MSX_sewersamRomDesc[] = {
	{ "sewer sam (japan).rom",	0x08000, 0x925c0aee, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_sewersam, MSX_sewersam, msx_msx)
STD_ROM_FN(MSX_sewersam)

struct BurnDriver BurnDrvMSX_sewersam = {
	"msx_sewersam", NULL, "msx_msx", NULL, "1984",
	"Sewer Sam (Jpn)\0", NULL, "Toshiba EMI", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_sewersamRomInfo, MSX_sewersamRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Shout Match (Jpn)

static struct BurnRomInfo MSX_shoutmatRomDesc[] = {
	{ "shout match (japan).rom",	0x08000, 0x729d2540, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_shoutmat, MSX_shoutmat, msx_msx)
STD_ROM_FN(MSX_shoutmat)

struct BurnDriver BurnDrvMSX_shoutmat = {
	"msx_shoutmat", NULL, "msx_msx", NULL, "1987",
	"Shout Match (Jpn)\0", NULL, "Victor", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_shoutmatRomInfo, MSX_shoutmatRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Sinbad - Nanatsu no Bouken (Jpn)

static struct BurnRomInfo MSX_sinbadRomDesc[] = {
	{ "sinbad (japan).rom",	0x04000, 0x8273fd0e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_sinbad, MSX_sinbad, msx_msx)
STD_ROM_FN(MSX_sinbad)

struct BurnDriver BurnDrvMSX_sinbad = {
	"msx_sinbad", NULL, "msx_msx", NULL, "1986",
	"Sinbad - Nanatsu no Bouken (Jpn)\0", NULL, "Casio", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_sinbadRomInfo, MSX_sinbadRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Ski Command (Jpn)

static struct BurnRomInfo MSX_skicommRomDesc[] = {
	{ "casio ski command (japan).rom",	0x04000, 0xd8750242, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_skicomm, MSX_skicomm, msx_msx)
STD_ROM_FN(MSX_skicomm)

struct BurnDriver BurnDrvMSX_skicomm = {
	"msx_skicomm", NULL, "msx_msx", NULL, "1985",
	"Ski Command (Jpn)\0", NULL, "Casio", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_skicommRomInfo, MSX_skicommRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Ski Command (Kor, Aproman)

static struct BurnRomInfo MSX_skicommk1RomDesc[] = {
	{ "skicomma.rom",	0x08000, 0x06980ea1, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_skicommk1, MSX_skicommk1, msx_msx)
STD_ROM_FN(MSX_skicommk1)

struct BurnDriver BurnDrvMSX_skicommk1 = {
	"msx_skicommk1", "msx_skicomm", "msx_msx", NULL, "198?",
	"Ski Command (Kor, Aproman)\0", NULL, "Aproman", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_skicommk1RomInfo, MSX_skicommk1RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Ski Command (Kor, ProSoft)

static struct BurnRomInfo MSX_skicommk2RomDesc[] = {
	{ "skicommb.rom",	0x08000, 0xa0276799, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_skicommk2, MSX_skicommk2, msx_msx)
STD_ROM_FN(MSX_skicommk2)

struct BurnDriver BurnDrvMSX_skicommk2 = {
	"msx_skicommk2", "msx_skicomm", "msx_msx", NULL, "198?",
	"Ski Command (Kor, ProSoft)\0", NULL, "ProSoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_skicommk2RomInfo, MSX_skicommk2RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Skooter (Jpn)

static struct BurnRomInfo MSX_skooterRomDesc[] = {
	{ "skooter (japan) (alt 1).rom",	0x08000, 0xf9e0fb4c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_skooter, MSX_skooter, msx_msx)
STD_ROM_FN(MSX_skooter)

struct BurnDriver BurnDrvMSX_skooter = {
	"msx_skooter", NULL, "msx_msx", NULL, "1988",
	"Skooter (Jpn)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_skooterRomInfo, MSX_skooterRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Skooter (Jpn, Alt)

static struct BurnRomInfo MSX_skooteraRomDesc[] = {
	{ "skooter (japan).rom",	0x08000, 0x53b87deb, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_skootera, MSX_skootera, msx_msx)
STD_ROM_FN(MSX_skootera)

struct BurnDriver BurnDrvMSX_skootera = {
	"msx_skootera", "msx_skooter", "msx_msx", NULL, "1988",
	"Skooter (Jpn, Alt)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_skooteraRomInfo, MSX_skooteraRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Skygaldo (Jpn)

static struct BurnRomInfo MSX_skygaldoRomDesc[] = {
	{ "sky galdo (japan).rom",	0x08000, 0x54f84047, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_skygaldo, MSX_skygaldo, msx_msx)
STD_ROM_FN(MSX_skygaldo)

struct BurnDriver BurnDrvMSX_skygaldo = {
	"msx_skygaldo", NULL, "msx_msx", NULL, "1986",
	"Skygaldo (Jpn)\0", NULL, "Magical Zoo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_skygaldoRomInfo, MSX_skygaldoRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Sky Jaguar (Jpn)

static struct BurnRomInfo MSX_skyjagRomDesc[] = {
	{ "sky jaguar (japan).rom",	0x04000, 0xe4f725fd, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_skyjag, MSX_skyjag, msx_msx)
STD_ROM_FN(MSX_skyjag)

struct BurnDriver BurnDrvMSX_skyjag = {
	"msx_skyjag", NULL, "msx_msx", NULL, "1984",
	"Sky Jaguar (Jpn)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_skyjagRomInfo, MSX_skyjagRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Sky Jaguar (Kor)

static struct BurnRomInfo MSX_skyjagkRomDesc[] = {
	{ "skyjagua.rom",	0x08000, 0x7e7fa3a0, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_skyjagk, MSX_skyjagk, msx_msx)
STD_ROM_FN(MSX_skyjagk)

struct BurnDriver BurnDrvMSX_skyjagk = {
	"msx_skyjagk", "msx_skyjag", "msx_msx", NULL, "198?",
	"Sky Jaguar (Kor)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_skyjagkRomInfo, MSX_skyjagkRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Snake It (Euro)

static struct BurnRomInfo MSX_snakeitRomDesc[] = {
	{ "snake it (europe).rom",	0x08000, 0x4a44bf23, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_snakeit, MSX_snakeit, msx_msx)
STD_ROM_FN(MSX_snakeit)

struct BurnDriver BurnDrvMSX_snakeit = {
	"msx_snakeit", NULL, "msx_msx", NULL, "1986?",
	"Snake It (Euro)\0", NULL, "Eaglesoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_snakeitRomInfo, MSX_snakeitRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Sofia (Jpn)

static struct BurnRomInfo MSX_sofiaRomDesc[] = {
	{ "sofia (japan).rom",	0x10000, 0x14811951, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_sofia, MSX_sofia, msx_msx)
STD_ROM_FN(MSX_sofia)

struct BurnDriver BurnDrvMSX_sofia = {
	"msx_sofia", NULL, "msx_msx", NULL, "1988",
	"Sofia (Jpn)\0", NULL, "Dempa", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII8, GBF_MISC, 0,
	MSXGetZipName, MSX_sofiaRomInfo, MSX_sofiaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Space Camp (Jpn)

static struct BurnRomInfo MSX_spacecmpRomDesc[] = {
	{ "space camp (japan).rom",	0x08000, 0xeb197b9d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_spacecmp, MSX_spacecmp, msx_msx)
STD_ROM_FN(MSX_spacecmp)

struct BurnDriver BurnDrvMSX_spacecmp = {
	"msx_spacecmp", NULL, "msx_msx", NULL, "1986",
	"Space Camp (Jpn)\0", NULL, "Pack-In-Video", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_spacecmpRomInfo, MSX_spacecmpRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Space Camp (Jpn, Alt)

static struct BurnRomInfo MSX_spacecmpaRomDesc[] = {
	{ "space camp (japan) (alt 1).rom",	0x08000, 0xcdd43807, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_spacecmpa, MSX_spacecmpa, msx_msx)
STD_ROM_FN(MSX_spacecmpa)

struct BurnDriver BurnDrvMSX_spacecmpa = {
	"msx_spacecmpa", "msx_spacecmp", "msx_msx", NULL, "1986",
	"Space Camp (Jpn, Alt)\0", NULL, "Pack-In-Video", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_spacecmpaRomInfo, MSX_spacecmpaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Space Invaders (Jpn)

static struct BurnRomInfo MSX_spaceinvRomDesc[] = {
	{ "space invaders (japan).rom",	0x04000, 0xde02932d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_spaceinv, MSX_spaceinv, msx_msx)
STD_ROM_FN(MSX_spaceinv)

struct BurnDriver BurnDrvMSX_spaceinv = {
	"msx_spaceinv", NULL, "msx_msx", NULL, "1985",
	"Space Invaders (Jpn)\0", NULL, "Nidecom", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_spaceinvRomInfo, MSX_spaceinvRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Space Maze Attack (Jpn)

static struct BurnRomInfo MSX_spacmazeRomDesc[] = {
	{ "space maze attack (japan).rom",	0x02000, 0x4a45cbc0, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_spacmaze, MSX_spacmaze, msx_msx)
STD_ROM_FN(MSX_spacmaze)

struct BurnDriver BurnDrvMSX_spacmaze = {
	"msx_spacmaze", NULL, "msx_msx", NULL, "1983",
	"Space Maze Attack (Jpn)\0", NULL, "HAL Kenkyuujo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_spacmazeRomInfo, MSX_spacmazeRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Space Maze Attack (Jpn, Alt)

static struct BurnRomInfo MSX_spacmazeaRomDesc[] = {
	{ "space maze attack (japan) (alt 1).rom",	0x02000, 0xd6eadaa2, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_spacmazea, MSX_spacmazea, msx_msx)
STD_ROM_FN(MSX_spacmazea)

struct BurnDriver BurnDrvMSX_spacmazea = {
	"msx_spacmazea", "msx_spacmaze", "msx_msx", NULL, "1983",
	"Space Maze Attack (Jpn, Alt)\0", NULL, "HAL Kenkyuujo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_spacmazeaRomInfo, MSX_spacmazeaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Space Maze Attack (Jpn, Alt 2)

static struct BurnRomInfo MSX_spacmazebRomDesc[] = {
	{ "space maze attack (japan) (alt 2).rom",	0x04000, 0x1932baf6, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_spacmazeb, MSX_spacmazeb, msx_msx)
STD_ROM_FN(MSX_spacmazeb)

struct BurnDriver BurnDrvMSX_spacmazeb = {
	"msx_spacmazeb", "msx_spacmaze", "msx_msx", NULL, "1983",
	"Space Maze Attack (Jpn, Alt 2)\0", NULL, "HAL Kenkyuujo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_spacmazebRomInfo, MSX_spacmazebRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Space Trouble (Jpn)

static struct BurnRomInfo MSX_spacetrbRomDesc[] = {
	{ "space trouble (japan).rom",	0x02000, 0x26119f0a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_spacetrb, MSX_spacetrb, msx_msx)
STD_ROM_FN(MSX_spacetrb)

struct BurnDriver BurnDrvMSX_spacetrb = {
	"msx_spacetrb", NULL, "msx_msx", NULL, "1984",
	"Space Trouble (Jpn)\0", NULL, "HAL Kenkyuujo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_spacetrbRomInfo, MSX_spacetrbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Sparkie (Jpn)

static struct BurnRomInfo MSX_sparkieRomDesc[] = {
	{ "sparkie (japan).rom",	0x02000, 0xf03ed7d5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_sparkie, MSX_sparkie, msx_msx)
STD_ROM_FN(MSX_sparkie)

struct BurnDriver BurnDrvMSX_sparkie = {
	"msx_sparkie", NULL, "msx_msx", NULL, "1983",
	"Sparkie (Jpn)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_sparkieRomInfo, MSX_sparkieRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Spelunker (Jpn)

static struct BurnRomInfo MSX_spelunkrRomDesc[] = {
	{ "spelunker (japan).rom",	0x08000, 0xdc948a3a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_spelunkr, MSX_spelunkr, msx_msx)
STD_ROM_FN(MSX_spelunkr)

struct BurnDriver BurnDrvMSX_spelunkr = {
	"msx_spelunkr", NULL, "msx_msx", NULL, "1986",
	"Spelunker (Jpn)\0", NULL, "Irem", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_spelunkrRomInfo, MSX_spelunkrRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Spelunker (Jpn, Alt)

static struct BurnRomInfo MSX_spelunkraRomDesc[] = {
	{ "spelunker (japan) (alt 1).rom",	0x08000, 0x4c738b64, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_spelunkra, MSX_spelunkra, msx_msx)
STD_ROM_FN(MSX_spelunkra)

struct BurnDriver BurnDrvMSX_spelunkra = {
	"msx_spelunkra", "msx_spelunkr", "msx_msx", NULL, "1986",
	"Spelunker (Jpn, Alt)\0", NULL, "Irem", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_spelunkraRomInfo, MSX_spelunkraRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// The Spider (Jpn)

static struct BurnRomInfo MSX_spiderRomDesc[] = {
	{ "spider, the (japan).rom",	0x04000, 0xa156ac02, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_spider, MSX_spider, msx_msx)
STD_ROM_FN(MSX_spider)

struct BurnDriver BurnDrvMSX_spider = {
	"msx_spider", NULL, "msx_msx", NULL, "1984",
	"The Spider (Jpn)\0", NULL, "Hudson Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_spiderRomInfo, MSX_spiderRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Square Dancer (Jpn)

static struct BurnRomInfo MSX_squardanRomDesc[] = {
	{ "square dancer (japan).rom",	0x04000, 0xdd5cf5c8, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_squardan, MSX_squardan, msx_msx)
STD_ROM_FN(MSX_squardan)

struct BurnDriver BurnDrvMSX_squardan = {
	"msx_squardan", NULL, "msx_msx", NULL, "1984",
	"Square Dancer (Jpn)\0", "Use keys 'Z', '/' and Enter", "Toshiba EMI", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_squardanRomInfo, MSX_squardanRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	SwapSlashDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Squish'em (Jpn)

static struct BurnRomInfo MSX_squishemRomDesc[] = {
	{ "squish'em (japan).rom",	0x04000, 0xf3b22c91, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_squishem, MSX_squishem, msx_msx)
STD_ROM_FN(MSX_squishem)

struct BurnDriver BurnDrvMSX_squishem = {
	"msx_squishem", NULL, "msx_msx", NULL, "1984",
	"Squish'em (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_squishemRomInfo, MSX_squishemRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Squish'em (Jpn, Alt)

static struct BurnRomInfo MSX_squishemaRomDesc[] = {
	{ "squish'em (japan) (alt 1).rom",	0x04000, 0xda50254f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_squishema, MSX_squishema, msx_msx)
STD_ROM_FN(MSX_squishema)

struct BurnDriver BurnDrvMSX_squishema = {
	"msx_squishema", "msx_squishem", "msx_msx", NULL, "1984",
	"Squish'em (Jpn, Alt)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_squishemaRomInfo, MSX_squishemaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Star Blazer (Jpn)

static struct BurnRomInfo MSX_starblazRomDesc[] = {
	{ "star blazer (japan).rom",	0x04000, 0xfd902e5d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_starblaz, MSX_starblaz, msx_msx)
STD_ROM_FN(MSX_starblaz)

struct BurnDriver BurnDrvMSX_starblaz = {
	"msx_starblaz", NULL, "msx_msx", NULL, "1985",
	"Star Blazer (Jpn)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_starblazRomInfo, MSX_starblazRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Star Blazer (Jpn, Alt)

static struct BurnRomInfo MSX_starblazaRomDesc[] = {
	{ "star blazer (japan) (alt 1).rom",	0x04000, 0x1c952691, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_starblaza, MSX_starblaza, msx_msx)
STD_ROM_FN(MSX_starblaza)

struct BurnDriver BurnDrvMSX_starblaza = {
	"msx_starblaza", "msx_starblaz", "msx_msx", NULL, "1985",
	"Star Blazer (Jpn, Alt)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_starblazaRomInfo, MSX_starblazaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Star Blazer (Jpn, Alt 2)

static struct BurnRomInfo MSX_starblazbRomDesc[] = {
	{ "star blazer (japan) (alt 2).rom",	0x04000, 0xa242fe0d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_starblazb, MSX_starblazb, msx_msx)
STD_ROM_FN(MSX_starblazb)

struct BurnDriver BurnDrvMSX_starblazb = {
	"msx_starblazb", "msx_starblaz", "msx_msx", NULL, "1985",
	"Star Blazer (Jpn, Alt 2)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_starblazbRomInfo, MSX_starblazbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Star Force (Jpn)

static struct BurnRomInfo MSX_starfrceRomDesc[] = {
	{ "star force (japan).rom",	0x08000, 0xc14e53a1, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_starfrce, MSX_starfrce, msx_msx)
STD_ROM_FN(MSX_starfrce)

struct BurnDriver BurnDrvMSX_starfrce = {
	"msx_starfrce", NULL, "msx_msx", NULL, "1985",
	"Star Force (Jpn)\0", NULL, "Hudson Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_starfrceRomInfo, MSX_starfrceRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Star Force (Jpn, Alt)

static struct BurnRomInfo MSX_starfrceaRomDesc[] = {
	{ "star force (japan) (alt 1).rom",	0x08000, 0xe9de7e32, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_starfrcea, MSX_starfrcea, msx_msx)
STD_ROM_FN(MSX_starfrcea)

struct BurnDriver BurnDrvMSX_starfrcea = {
	"msx_starfrcea", "msx_starfrce", "msx_msx", NULL, "1985",
	"Star Force (Jpn, Alt)\0", NULL, "Hudson Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_starfrceaRomInfo, MSX_starfrceaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Star Soldier (Jpn)

static struct BurnRomInfo MSX_starsoldRomDesc[] = {
	{ "star soldier (japan).rom",	0x08000, 0x0b3d975d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_starsold, MSX_starsold, msx_msx)
STD_ROM_FN(MSX_starsold)

struct BurnDriver BurnDrvMSX_starsold = {
	"msx_starsold", NULL, "msx_msx", NULL, "1986",
	"Star Soldier (Jpn)\0", NULL, "Hudson Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_starsoldRomInfo, MSX_starsoldRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Star Soldier (Jpn, Alt)

static struct BurnRomInfo MSX_starsoldaRomDesc[] = {
	{ "star soldier (japan) (alt 1).rom",	0x08000, 0xf6d4e101, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_starsolda, MSX_starsolda, msx_msx)
STD_ROM_FN(MSX_starsolda)

struct BurnDriver BurnDrvMSX_starsolda = {
	"msx_starsolda", "msx_starsold", "msx_msx", NULL, "1986",
	"Star Soldier (Jpn, Alt)\0", NULL, "Hudson Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_starsoldaRomInfo, MSX_starsoldaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Starship Simulator (Jpn)

static struct BurnRomInfo MSX_starshipRomDesc[] = {
	{ "starship simulator (japan).rom",	0x04000, 0x236d2ab1, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_starship, MSX_starship, msx_msx)
STD_ROM_FN(MSX_starship)

struct BurnDriver BurnDrvMSX_starship = {
	"msx_starship", NULL, "msx_msx", NULL, "1984",
	"Starship Simulator (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_starshipRomInfo, MSX_starshipRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Starship Simulator (Jpn, Alt)

static struct BurnRomInfo MSX_starshipaRomDesc[] = {
	{ "starship simulator (japan) (alt 1).rom",	0x04000, 0x87824af6, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_starshipa, MSX_starshipa, msx_msx)
STD_ROM_FN(MSX_starshipa)

struct BurnDriver BurnDrvMSX_starshipa = {
	"msx_starshipa", "msx_starship", "msx_msx", NULL, "1984",
	"Starship Simulator (Jpn, Alt)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_starshipaRomInfo, MSX_starshipaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Step Up (Jpn)

static struct BurnRomInfo MSX_stepupRomDesc[] = {
	{ "step up (japan).rom",	0x02000, 0xcabcddf1, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_stepup, MSX_stepup, msx_msx)
STD_ROM_FN(MSX_stepup)

struct BurnDriver BurnDrvMSX_stepup = {
	"msx_stepup", NULL, "msx_msx", NULL, "1983",
	"Step Up (Jpn)\0", NULL, "HAL Kenkyuujo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_stepupRomInfo, MSX_stepupRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Step Up (Jpn, Alt)

static struct BurnRomInfo MSX_stepupaRomDesc[] = {
	{ "step up (japan) (alt 1).rom",	0x02000, 0x60a8927b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_stepupa, MSX_stepupa, msx_msx)
STD_ROM_FN(MSX_stepupa)

struct BurnDriver BurnDrvMSX_stepupa = {
	"msx_stepupa", "msx_stepup", "msx_msx", NULL, "1983",
	"Step Up (Jpn, Alt)\0", NULL, "HAL Kenkyuujo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_stepupaRomInfo, MSX_stepupaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Step Up (Kor)

static struct BurnRomInfo MSX_stepupkRomDesc[] = {
	{ "stepup.rom",	0x08000, 0x5bce3fec, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_stepupk, MSX_stepupk, msx_msx)
STD_ROM_FN(MSX_stepupk)

struct BurnDriver BurnDrvMSX_stepupk = {
	"msx_stepupk", "msx_stepup", "msx_msx", NULL, "198?",
	"Step Up (Kor)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_stepupkRomInfo, MSX_stepupkRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Stepper (Jpn)

static struct BurnRomInfo MSX_stepperRomDesc[] = {
	{ "stepper (japan).rom",	0x04000, 0x0fb7df8f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_stepper, MSX_stepper, msx_msx)
STD_ROM_FN(MSX_stepper)

struct BurnDriver BurnDrvMSX_stepper = {
	"msx_stepper", NULL, "msx_msx", NULL, "1985",
	"Stepper (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_stepperRomInfo, MSX_stepperRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Kenja no Ishi - The Stone of Wisdom (Jpn)

static struct BurnRomInfo MSX_stonewisRomDesc[] = {
	{ "stone of wisdom, the (japan).rom",	0x08000, 0x8c7a7435, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_stonewis, MSX_stonewis, msx_msx)
STD_ROM_FN(MSX_stonewis)

struct BurnDriver BurnDrvMSX_stonewis = {
	"msx_stonewis", NULL, "msx_msx", NULL, "1986",
	"Kenja no Ishi - The Stone of Wisdom (Jpn)\0", NULL, "Casio", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_stonewisRomInfo, MSX_stonewisRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Strange Loop (Jpn)

static struct BurnRomInfo MSX_stranglpRomDesc[] = {
	{ "strange loop (japan).rom",	0x08000, 0x6554c0c5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_stranglp, MSX_stranglp, msx_msx)
STD_ROM_FN(MSX_stranglp)

struct BurnDriver BurnDrvMSX_stranglp = {
	"msx_stranglp", NULL, "msx_msx", NULL, "1987",
	"Strange Loop (Jpn)\0", NULL, "Nihon Dexter", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_stranglpRomInfo, MSX_stranglpRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Street Master (Kor)

static struct BurnRomInfo MSX_strtmastRomDesc[] = {
	{ "street master.rom",	0x20000, 0x7269c520, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_strtmast, MSX_strtmast, msx_msx)
STD_ROM_FN(MSX_strtmast)

struct BurnDriver BurnDrvMSX_strtmast = {
	"msx_strtmast", NULL, "msx_msx", NULL, "1992",
	"Street Master (Kor)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII8, GBF_MISC, 0,
	MSXGetZipName, MSX_strtmastRomInfo, MSX_strtmastRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Suparobo (Jpn)

static struct BurnRomInfo MSX_suparoboRomDesc[] = {
	{ "suparobo (japan).rom",	0x04000, 0x483eddcc, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_suparobo, MSX_suparobo, msx_msx)
STD_ROM_FN(MSX_suparobo)

struct BurnDriver BurnDrvMSX_suparobo = {
	"msx_suparobo", NULL, "msx_msx", NULL, "1984",
	"Suparobo (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_suparoboRomInfo, MSX_suparoboRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Super Billiards (Jpn)

static struct BurnRomInfo MSX_sbillardRomDesc[] = {
	{ "super billiards (japan).rom",	0x02000, 0x79bc12b2, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_sbillard, MSX_sbillard, msx_msx)
STD_ROM_FN(MSX_sbillard)

struct BurnDriver BurnDrvMSX_sbillard = {
	"msx_sbillard", NULL, "msx_msx", NULL, "1983",
	"Super Billiards (Jpn)\0", NULL, "HAL Kenkyuujo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_BASIC, GBF_MISC, 0,
	MSXGetZipName, MSX_sbillardRomInfo, MSX_sbillardRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Super Bioman 4 (Kor)

static struct BurnRomInfo MSX_sbioman4RomDesc[] = {
	{ "super bioman 4 (korea) (unl).rom",	0x10000, 0x112abf34, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_sbioman4, MSX_sbioman4, msx_msx)
STD_ROM_FN(MSX_sbioman4)

struct BurnDriver BurnDrvMSX_sbioman4 = {
	"msx_sbioman4", NULL, "msx_msx", NULL, "199?",
	"Super Bioman 4 (Kor)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_KONAMI, GBF_MISC, 0,
	MSXGetZipName, MSX_sbioman4RomInfo, MSX_sbioman4RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Super Boy 3 (Kor)

static struct BurnRomInfo MSX_sboy3RomDesc[] = {
	{ "super boy 3 (korea) (unl).rom",	0x20000, 0x9195c34c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_sboy3, MSX_sboy3, msx_msx)
STD_ROM_FN(MSX_sboy3)

struct BurnDriver BurnDrvMSX_sboy3 = {
	"msx_sboy3", NULL, "msx_msx", NULL, "1991",
	"Super Boy 3 (Kor)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_KONAMI, GBF_MISC, 0,
	MSXGetZipName, MSX_sboy3RomInfo, MSX_sboy3RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Super Boy 3 (Kor, Alt)

static struct BurnRomInfo MSX_sboy3aRomDesc[] = {
	{ "super boy iii (1991)(zemina).rom",	0x20000, 0xf422ad79, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_sboy3a, MSX_sboy3a, msx_msx)
STD_ROM_FN(MSX_sboy3a)

struct BurnDriver BurnDrvMSX_sboy3a = {
	"msx_sboy3a", "msx_sboy3", "msx_msx", NULL, "1991",
	"Super Boy 3 (Kor, Alt)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII8, GBF_MISC, 0,
	MSXGetZipName, MSX_sboy3aRomInfo, MSX_sboy3aRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Super Boy I (Kor)

static struct BurnRomInfo MSX_sboy1RomDesc[] = {
	{ "super boy i (korea) (unl).rom",	0x08000, 0x13b34548, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_sboy1, MSX_sboy1, msx_msx)
STD_ROM_FN(MSX_sboy1)

struct BurnDriver BurnDrvMSX_sboy1 = {
	"msx_sboy1", NULL, "msx_msx", NULL, "1989",
	"Super Boy I (Kor)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_sboy1RomInfo, MSX_sboy1RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Super Boy II. (Kor)

static struct BurnRomInfo MSX_sboy2RomDesc[] = {
	{ "super boy ii (korea) (unl).rom",	0x08000, 0x09206bfd, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_sboy2, MSX_sboy2, msx_msx)
STD_ROM_FN(MSX_sboy2)

struct BurnDriver BurnDrvMSX_sboy2 = {
	"msx_sboy2", NULL, "msx_msx", NULL, "1989",
	"Super Boy II. (Kor)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_sboy2RomInfo, MSX_sboy2RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Super Boy II. (Kor, Alt)

static struct BurnRomInfo MSX_sboy2aRomDesc[] = {
	{ "super boy ii (1989)(zemina).rom",	0x08000, 0x7de493ab, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_sboy2a, MSX_sboy2a, msx_msx)
STD_ROM_FN(MSX_sboy2a)

struct BurnDriver BurnDrvMSX_sboy2a = {
	"msx_sboy2a", "msx_sboy2", "msx_msx", NULL, "1989",
	"Super Boy II. (Kor, Alt)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_sboy2aRomInfo, MSX_sboy2aRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Super Cobra (Jpn)

static struct BurnRomInfo MSX_scobraRomDesc[] = {
	{ "super cobra (japan).rom",	0x02000, 0x97425d70, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_scobra, MSX_scobra, msx_msx)
STD_ROM_FN(MSX_scobra)

struct BurnDriver BurnDrvMSX_scobra = {
	"msx_scobra", NULL, "msx_msx", NULL, "1983",
	"Super Cobra (Jpn)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_scobraRomInfo, MSX_scobraRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Super Cobra (Jpn, Alt)

static struct BurnRomInfo MSX_scobraaRomDesc[] = {
	{ "super cobra (japan) (alt 1).rom",	0x02000, 0xbfcc181d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_scobraa, MSX_scobraa, msx_msx)
STD_ROM_FN(MSX_scobraa)

struct BurnDriver BurnDrvMSX_scobraa = {
	"msx_scobraa", "msx_scobra", "msx_msx", NULL, "1983",
	"Super Cobra (Jpn, Alt)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_scobraaRomInfo, MSX_scobraaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Super Columns (Kor)

static struct BurnRomInfo MSX_supercolRomDesc[] = {
	{ "super columns (japan).rom",	0x08000, 0xf0a030ec, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_supercol, MSX_supercol, msx_msx)
STD_ROM_FN(MSX_supercol)

struct BurnDriver BurnDrvMSX_supercol = {
	"msx_supercol", NULL, "msx_msx", NULL, "1990",
	"Super Columns (Kor)\0", NULL, "Hi-Com", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_supercolRomInfo, MSX_supercolRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Super Cross Force (Euro)

static struct BurnRomInfo MSX_superxfRomDesc[] = {
	{ "super cross force (europe).rom",	0x04000, 0xf05a7f4e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_superxf, MSX_superxf, msx_msx)
STD_ROM_FN(MSX_superxf)

struct BurnDriver BurnDrvMSX_superxf = {
	"msx_superxf", NULL, "msx_msx", NULL, "1983",
	"Super Cross Force (Euro)\0", NULL, "Spectravideo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_superxfRomInfo, MSX_superxfRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Super Drinker (Jpn)

static struct BurnRomInfo MSX_supdrinkRomDesc[] = {
	{ "super drinker (japan).rom",	0x04000, 0x3254017e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_supdrink, MSX_supdrink, msx_msx)
STD_ROM_FN(MSX_supdrink)

struct BurnDriver BurnDrvMSX_supdrink = {
	"msx_supdrink", NULL, "msx_msx", NULL, "1983",
	"Super Drinker (Jpn)\0", NULL, "Ample Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_supdrinkRomInfo, MSX_supdrinkRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Super Golf (Jpn)

static struct BurnRomInfo MSX_superglfRomDesc[] = {
	{ "super golf (japan).rom",	0x08000, 0x66aadfa8, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_superglf, MSX_superglf, msx_msx)
STD_ROM_FN(MSX_superglf)

struct BurnDriver BurnDrvMSX_superglf = {
	"msx_superglf", NULL, "msx_msx", NULL, "1984",
	"Super Golf (Jpn)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_superglfRomInfo, MSX_superglfRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Super Laydock - Mission Striker (Kor)

static struct BurnRomInfo MSX_slaydockkRomDesc[] = {
	{ "super laydock - mission striker (1988)(zemina).rom",	0x40000, 0xb885a464, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_slaydockk, MSX_slaydockk, msx_msx)
STD_ROM_FN(MSX_slaydockk)

struct BurnDriver BurnDrvMSX_slaydockk = {
	"msx_slaydockk", "msx_slaydock", "msx_msx", NULL, "1988",
	"Super Laydock - Mission Striker (Kor)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_KONAMI, GBF_MISC, 0,
	MSXGetZipName, MSX_slaydockkRomInfo, MSX_slaydockkRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Super Pachinko (Jpn)

static struct BurnRomInfo MSX_suppachiRomDesc[] = {
	{ "super pachinko (japan).rom",	0x04000, 0x8aebddd2, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_suppachi, MSX_suppachi, msx_msx)
STD_ROM_FN(MSX_suppachi)

struct BurnDriver BurnDrvMSX_suppachi = {
	"msx_suppachi", NULL, "msx_msx", NULL, "1985",
	"Super Pachinko (Jpn)\0", NULL, "Nihon Columbia", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_suppachiRomInfo, MSX_suppachiRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Super Penguin

static struct BurnRomInfo MSX_spenguinRomDesc[] = {
	{ "sieco_superpenguin_(sieco).rom",	0x08000, 0x0555a6e9, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_spenguin, MSX_spenguin, msx_msx)
STD_ROM_FN(MSX_spenguin)

struct BurnDriver BurnDrvMSX_spenguin = {
	"msx_spenguin", NULL, "msx_msx", NULL, "198?",
	"Super Penguin\0", NULL, "Sieco", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_spenguinRomInfo, MSX_spenguinRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Super Snake (Jpn)

static struct BurnRomInfo MSX_supsnakeRomDesc[] = {
	{ "super snake (japan).rom",	0x02000, 0x491c6af0, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_supsnake, MSX_supsnake, msx_msx)
STD_ROM_FN(MSX_supsnake)

struct BurnDriver BurnDrvMSX_supsnake = {
	"msx_supsnake", NULL, "msx_msx", NULL, "1983",
	"Super Snake (Jpn)\0", NULL, "HAL Kenkyuujo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_supsnakeRomInfo, MSX_supsnakeRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Super Snake (Jpn, Alt)

static struct BurnRomInfo MSX_supsnakeaRomDesc[] = {
	{ "super snake (japan) (alt 1).rom",	0x04000, 0xf192f67e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_supsnakea, MSX_supsnakea, msx_msx)
STD_ROM_FN(MSX_supsnakea)

struct BurnDriver BurnDrvMSX_supsnakea = {
	"msx_supsnakea", "msx_supsnake", "msx_msx", NULL, "1983",
	"Super Snake (Jpn, Alt)\0", NULL, "HAL Kenkyuujo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_supsnakeaRomInfo, MSX_supsnakeaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Super Snake (Jpn, Alt 2)

static struct BurnRomInfo MSX_supsnakebRomDesc[] = {
	{ "super snake (japan) (alt 2).rom",	0x04000, 0xe31a0336, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_supsnakeb, MSX_supsnakeb, msx_msx)
STD_ROM_FN(MSX_supsnakeb)

struct BurnDriver BurnDrvMSX_supsnakeb = {
	"msx_supsnakeb", "msx_supsnake", "msx_msx", NULL, "1983",
	"Super Snake (Jpn, Alt 2)\0", NULL, "HAL Kenkyuujo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_supsnakebRomInfo, MSX_supsnakebRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Super Soccer (Jpn)

static struct BurnRomInfo MSX_supsoccrRomDesc[] = {
	{ "super soccer (japan).rom",	0x08000, 0x6408ed24, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_supsoccr, MSX_supsoccr, msx_msx)
STD_ROM_FN(MSX_supsoccr)

struct BurnDriver BurnDrvMSX_supsoccr = {
	"msx_supsoccr", NULL, "msx_msx", NULL, "1985",
	"Super Soccer (Jpn)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_supsoccrRomInfo, MSX_supsoccrRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// MSX Super Tennis (Jpn)

static struct BurnRomInfo MSX_suptennRomDesc[] = {
	{ "super tennis (japan).rom",	0x04000, 0xd560d9c0, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_suptenn, MSX_suptenn, msx_msx)
STD_ROM_FN(MSX_suptenn)

struct BurnDriver BurnDrvMSX_suptenn = {
	"msx_suptenn", NULL, "msx_msx", NULL, "1984",
	"MSX Super Tennis (Jpn)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_suptennRomInfo, MSX_suptennRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Supertripper (Spa)

static struct BurnRomInfo MSX_suptripRomDesc[] = {
	{ "super tripper (spain).rom",	0x08000, 0x0c36f5bd, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_suptrip, MSX_suptrip, msx_msx)
STD_ROM_FN(MSX_suptrip)

struct BurnDriver BurnDrvMSX_suptrip = {
	"msx_suptrip", NULL, "msx_msx", NULL, "1985",
	"Supertripper (Spa)\0", NULL, "Sony Spain", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_suptripRomInfo, MSX_suptripRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Sweet Acorn (Jpn)

static struct BurnRomInfo MSX_sweetacrRomDesc[] = {
	{ "sweet acorn (japan).rom",	0x04000, 0x58670a22, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_sweetacr, MSX_sweetacr, msx_msx)
STD_ROM_FN(MSX_sweetacr)

struct BurnDriver BurnDrvMSX_sweetacr = {
	"msx_sweetacr", NULL, "msx_msx", NULL, "1984",
	"Sweet Acorn (Jpn)\0", NULL, "Taito", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_sweetacrRomInfo, MSX_sweetacrRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Sweet Acorn (Jpn, Alt)

static struct BurnRomInfo MSX_sweetacraRomDesc[] = {
	{ "sweet acorn (japan) (alt 1).rom",	0x04000, 0x54a8bf3c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_sweetacra, MSX_sweetacra, msx_msx)
STD_ROM_FN(MSX_sweetacra)

struct BurnDriver BurnDrvMSX_sweetacra = {
	"msx_sweetacra", "msx_sweetacr", "msx_msx", NULL, "1984",
	"Sweet Acorn (Jpn, Alt)\0", NULL, "Taito", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_sweetacraRomInfo, MSX_sweetacraRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Swing (Jpn)

static struct BurnRomInfo MSX_swingRomDesc[] = {
	{ "swing (japan).rom",	0x04000, 0xc93fadf4, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_swing, MSX_swing, msx_msx)
STD_ROM_FN(MSX_swing)

struct BurnDriver BurnDrvMSX_swing = {
	"msx_swing", NULL, "msx_msx", NULL, "1985",
	"Swing (Jpn)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_swingRomInfo, MSX_swingRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Swing (Jpn, Alt)

static struct BurnRomInfo MSX_swingaRomDesc[] = {
	{ "swing (japan) (alt 1).rom",	0x04000, 0x60355c9e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_swinga, MSX_swinga, msx_msx)
STD_ROM_FN(MSX_swinga)

struct BurnDriver BurnDrvMSX_swinga = {
	"msx_swinga", "msx_swing", "msx_msx", NULL, "1985",
	"Swing (Jpn, Alt)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_swingaRomInfo, MSX_swingaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Takahashi Meijin no Boukenjima (Jpn)

static struct BurnRomInfo MSX_takameijRomDesc[] = {
	{ "wonder boy (japan).rom",	0x08000, 0x892266ca, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_takameij, MSX_takameij, msx_msx)
STD_ROM_FN(MSX_takameij)

struct BurnDriver BurnDrvMSX_takameij = {
	"msx_takameij", NULL, "msx_msx", NULL, "1986",
	"Wonder Boy. Takahashi Meijin no Boukenjima (Jpn)\0", NULL, "Hudson Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_takameijRomInfo, MSX_takameijRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Takahashi Meijin no Boukenjima (Jpn, Alt)

static struct BurnRomInfo MSX_takameijaRomDesc[] = {
	{ "wonder boy (japan) (alt 1).rom",	0x08000, 0x5130c27b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_takameija, MSX_takameija, msx_msx)
STD_ROM_FN(MSX_takameija)

struct BurnDriver BurnDrvMSX_takameija = {
	"msx_takameija", "msx_takameij", "msx_msx", NULL, "1986",
	"Takahashi Meijin no Boukenjima (Jpn, Alt)\0", NULL, "Hudson Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_takameijaRomInfo, MSX_takameijaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Takahashi Meijin no Boukenjima (Kor, Star Frontiers)

static struct BurnRomInfo MSX_takameijk1RomDesc[] = {
	{ "takahasi meijin no boukenjima. wonder boy (1986)(hudson soft)[cr star frontiers].rom",	0x08000, 0x78ab8fd7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_takameijk1, MSX_takameijk1, msx_msx)
STD_ROM_FN(MSX_takameijk1)

struct BurnDriver BurnDrvMSX_takameijk1 = {
	"msx_takameijk1", "msx_takameij", "msx_msx", NULL, "1988",
	"Takahashi Meijin no Boukenjima (Kor, Star Frontiers)\0", NULL, "Star Frontiers", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_takameijk1RomInfo, MSX_takameijk1RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Takahashi Meijin no Boukenjima (Kor, Zemina)

static struct BurnRomInfo MSX_takameijk2RomDesc[] = {
	{ "wonderboy.rom",	0x08000, 0x9b4fb6c9, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_takameijk2, MSX_takameijk2, msx_msx)
STD_ROM_FN(MSX_takameijk2)

struct BurnDriver BurnDrvMSX_takameijk2 = {
	"msx_takameijk2", "msx_takameij", "msx_msx", NULL, "198?",
	"Takahashi Meijin no Boukenjima (Kor, Zemina)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_takameijk2RomInfo, MSX_takameijk2RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Takeru Densetsu (Jpn)

static struct BurnRomInfo MSX_takeruRomDesc[] = {
	{ "fuun takeshijyou (japan).rom",	0x08000, 0xc57272c3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_takeru, MSX_takeru, msx_msx)
STD_ROM_FN(MSX_takeru)

struct BurnDriver BurnDrvMSX_takeru = {
	"msx_takeru", NULL, "msx_msx", NULL, "1987",
	"Takeru Densetsu (Jpn)\0", NULL, "Brother Kougyou", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_takeruRomInfo, MSX_takeruRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Tank Battalion (Jpn)

static struct BurnRomInfo MSX_tankbattRomDesc[] = {
	{ "tank battalion (japan).rom",	0x02000, 0xf48a0c3b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_tankbatt, MSX_tankbatt, msx_msx)
STD_ROM_FN(MSX_tankbatt)

struct BurnDriver BurnDrvMSX_tankbatt = {
	"msx_tankbatt", NULL, "msx_msx", NULL, "1984",
	"Tank Battalion (Jpn)\0", NULL, "Namcot", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_tankbattRomInfo, MSX_tankbattRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Tatica (Jpn)

static struct BurnRomInfo MSX_taticaRomDesc[] = {
	{ "tatica (japan).rom",	0x04000, 0x7914f7a6, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_tatica, MSX_tatica, msx_msx)
STD_ROM_FN(MSX_tatica)

struct BurnDriver BurnDrvMSX_tatica = {
	"msx_tatica", NULL, "msx_msx", NULL, "1985",
	"Tatica (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_taticaRomInfo, MSX_taticaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Tawara-kun (Jpn)

static struct BurnRomInfo MSX_tawaraknRomDesc[] = {
	{ "tawara-kun (japan).rom",	0x04000, 0x589ce03e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_tawarakn, MSX_tawarakn, msx_msx)
STD_ROM_FN(MSX_tawarakn)

struct BurnDriver BurnDrvMSX_tawarakn = {
	"msx_tawarakn", NULL, "msx_msx", NULL, "1984",
	"Tawara-kun (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_tawaraknRomInfo, MSX_tawaraknRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Tawara-kun (Jpn, Alt)

static struct BurnRomInfo MSX_tawaraknaRomDesc[] = {
	{ "tawara-kun (japan) (alt 1).rom",	0x04000, 0xd69e9ea6, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_tawarakna, MSX_tawarakna, msx_msx)
STD_ROM_FN(MSX_tawarakna)

struct BurnDriver BurnDrvMSX_tawarakna = {
	"msx_tawarakna", "msx_tawarakn", "msx_msx", NULL, "1984",
	"Tawara-kun (Jpn, Alt)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_tawaraknaRomInfo, MSX_tawaraknaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Tawara-kun (Jpn, Alt 2)

static struct BurnRomInfo MSX_tawaraknbRomDesc[] = {
	{ "tawara-kun (japan) (alt 2).rom",	0x04000, 0x895cb183, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_tawaraknb, MSX_tawaraknb, msx_msx)
STD_ROM_FN(MSX_tawaraknb)

struct BurnDriver BurnDrvMSX_tawaraknb = {
	"msx_tawaraknb", "msx_tawarakn", "msx_msx", NULL, "1984",
	"Tawara-kun (Jpn, Alt 2)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_tawaraknbRomInfo, MSX_tawaraknbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Tear of Nile (Jpn)

static struct BurnRomInfo MSX_tearnileRomDesc[] = {
	{ "tear of nile (japan).rom",	0x08000, 0x867ba302, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_tearnile, MSX_tearnile, msx_msx)
STD_ROM_FN(MSX_tearnile)

struct BurnDriver BurnDrvMSX_tearnile = {
	"msx_tearnile", NULL, "msx_msx", NULL, "1986",
	"Tear of Nile (Jpn)\0", NULL, "Victor", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_tearnileRomInfo, MSX_tearnileRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Telebunnie (Jpn)

static struct BurnRomInfo MSX_telebunnRomDesc[] = {
	{ "telebunnie (japan).rom",	0x04000, 0x377fefef, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_telebunn, MSX_telebunn, msx_msx)
STD_ROM_FN(MSX_telebunn)

struct BurnDriver BurnDrvMSX_telebunn = {
	"msx_telebunn", NULL, "msx_msx", NULL, "1984",
	"Telebunnie (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_telebunnRomInfo, MSX_telebunnRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Telebunnie (Jpn, Alt)

static struct BurnRomInfo MSX_telebunnaRomDesc[] = {
	{ "telebunnie (japan) (alt 1).rom",	0x04000, 0x29314400, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_telebunna, MSX_telebunna, msx_msx)
STD_ROM_FN(MSX_telebunna)

struct BurnDriver BurnDrvMSX_telebunna = {
	"msx_telebunna", "msx_telebunn", "msx_msx", NULL, "1984",
	"Telebunnie (Jpn, Alt)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_telebunnaRomInfo, MSX_telebunnaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Tensai Rabbian Daifunsen (Jpn)

static struct BurnRomInfo MSX_tensairdRomDesc[] = {
	{ "tensai rabbian daifunsen (japan).rom",	0x08000, 0xb7322600, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_tensaird, MSX_tensaird, msx_msx)
STD_ROM_FN(MSX_tensaird)

struct BurnDriver BurnDrvMSX_tensaird = {
	"msx_tensaird", NULL, "msx_msx", NULL, "1986",
	"Tensai Rabbian Daifunsen (Jpn)\0", NULL, "Toshiba EMI", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_tensairdRomInfo, MSX_tensairdRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Tetra Horror (Jpn)

static struct BurnRomInfo MSX_tetrahorRomDesc[] = {
	{ "tetra horror (japan).rom",	0x04000, 0xe2d3377c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_tetrahor, MSX_tetrahor, msx_msx)
STD_ROM_FN(MSX_tetrahor)

struct BurnDriver BurnDrvMSX_tetrahor = {
	"msx_tetrahor", NULL, "msx_msx", NULL, "1983",
	"Tetra Horror (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_tetrahorRomInfo, MSX_tetrahorRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Tetra Horror (Jpn, Alt)

static struct BurnRomInfo MSX_tetrahoraRomDesc[] = {
	{ "tetra horror (japan) (alt 1).rom",	0x04000, 0x2e1be8b6, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_tetrahora, MSX_tetrahora, msx_msx)
STD_ROM_FN(MSX_tetrahora)

struct BurnDriver BurnDrvMSX_tetrahora = {
	"msx_tetrahora", "msx_tetrahor", "msx_msx", NULL, "1983",
	"Tetra Horror (Jpn, Alt)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_tetrahoraRomInfo, MSX_tetrahoraRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Tetris (Kor)

static struct BurnRomInfo MSX_tetrisRomDesc[] = {
	{ "tetris (korea) (unl).rom",	0x08000, 0xadcb026d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_tetris, MSX_tetris, msx_msx)
STD_ROM_FN(MSX_tetris)

struct BurnDriver BurnDrvMSX_tetris = {
	"msx_tetris", NULL, "msx_msx", NULL, "19??",
	"Tetris (Kor)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_tetrisRomInfo, MSX_tetrisRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Tetris (Kor, Alt)

static struct BurnRomInfo MSX_tetrisaRomDesc[] = {
	{ "tetrisa.rom",	0x08000, 0xb358d5a7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_tetrisa, MSX_tetrisa, msx_msx)
STD_ROM_FN(MSX_tetrisa)

struct BurnDriver BurnDrvMSX_tetrisa = {
	"msx_tetrisa", "msx_tetris", "msx_msx", NULL, "198?",
	"Tetris (Kor, Alt)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_tetrisaRomInfo, MSX_tetrisaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Tetris II (Kor)

static struct BurnRomInfo MSX_tetris2RomDesc[] = {
	{ "tetris2.rom",	0x08000, 0xd1e2f414, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_tetris2, MSX_tetris2, msx_msx)
STD_ROM_FN(MSX_tetris2)

struct BurnDriver BurnDrvMSX_tetris2 = {
	"msx_tetris2", NULL, "msx_msx", NULL, "1989",
	"Tetris II (Kor)\0", NULL, "Prosoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_tetris2RomInfo, MSX_tetris2RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Tetsuman (Jpn)

static struct BurnRomInfo MSX_tetsumanRomDesc[] = {
	{ "tetsuman (japan).rom",	0x04000, 0x929d6786, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_tetsuman, MSX_tetsuman, msx_msx)
STD_ROM_FN(MSX_tetsuman)

struct BurnDriver BurnDrvMSX_tetsuman = {
	"msx_tetsuman", NULL, "msx_msx", NULL, "1985",
	"Tetsuman (Jpn)\0", NULL, "HAL Kenkyuujo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_tetsumanRomInfo, MSX_tetsumanRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Tetsuman (Jpn, Alt)

static struct BurnRomInfo MSX_tetsumanaRomDesc[] = {
	{ "tetsuman (japan) (alt 1).rom",	0x04000, 0xb6c02ae7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_tetsumana, MSX_tetsumana, msx_msx)
STD_ROM_FN(MSX_tetsumana)

struct BurnDriver BurnDrvMSX_tetsumana = {
	"msx_tetsumana", "msx_tetsuman", "msx_msx", NULL, "1985",
	"Tetsuman (Jpn, Alt)\0", NULL, "HAL Kenkyuujo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_tetsumanaRomInfo, MSX_tetsumanaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Thexder (Jpn)

static struct BurnRomInfo MSX_thexderRomDesc[] = {
	{ "thexder (japan).rom",	0x08000, 0x599aa9ac, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_thexder, MSX_thexder, msx_msx)
STD_ROM_FN(MSX_thexder)

struct BurnDriver BurnDrvMSX_thexder = {
	"msx_thexder", NULL, "msx_msx", NULL, "1986",
	"Thexder (Jpn)\0", NULL, "Game Arts?", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_thexderRomInfo, MSX_thexderRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Thexder (Jpn, Alt)

static struct BurnRomInfo MSX_thexderaRomDesc[] = {
	{ "thexder (japan) (alt 1).rom",	0x08000, 0xda428d4b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_thexdera, MSX_thexdera, msx_msx)
STD_ROM_FN(MSX_thexdera)

struct BurnDriver BurnDrvMSX_thexdera = {
	"msx_thexdera", "msx_thexder", "msx_msx", NULL, "1986",
	"Thexder (Jpn, Alt)\0", NULL, "Game Arts?", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_thexderaRomInfo, MSX_thexderaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Thexder (Jpn, Alt 2)

static struct BurnRomInfo MSX_thexderbRomDesc[] = {
	{ "thexder (japan) (alt 2).rom",	0x08000, 0x61704513, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_thexderb, MSX_thexderb, msx_msx)
STD_ROM_FN(MSX_thexderb)

struct BurnDriver BurnDrvMSX_thexderb = {
	"msx_thexderb", "msx_thexder", "msx_msx", NULL, "1986",
	"Thexder (Jpn, Alt 2)\0", NULL, "Game Arts?", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_thexderbRomInfo, MSX_thexderbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// The Three Dragon Story (Kor)

static struct BurnRomInfo MSX_3dragonRomDesc[] = {
	{ "thrdrsto.rom",	0x08000, 0xf052e5fb, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_3dragon, MSX_3dragon, msx_msx)
STD_ROM_FN(MSX_3dragon)

struct BurnDriver BurnDrvMSX_3dragon = {
	"msx_3dragon", NULL, "msx_msx", NULL, "1989",
	"The Three Dragon Story (Kor)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_3dragonRomInfo, MSX_3dragonRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Thunder Ball (Jpn)

static struct BurnRomInfo MSX_thndrbalRomDesc[] = {
	{ "thunder ball (japan).rom",	0x08000, 0x86f902a9, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_thndrbal, MSX_thndrbal, msx_msx)
STD_ROM_FN(MSX_thndrbal)

struct BurnDriver BurnDrvMSX_thndrbal = {
	"msx_thndrbal", NULL, "msx_msx", NULL, "1985",
	"Thunder Ball (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_thndrbalRomInfo, MSX_thndrbalRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Thunder Ball (Jpn, Alt)

static struct BurnRomInfo MSX_thndrbalaRomDesc[] = {
	{ "thunder ball (japan) (alt 1).rom",	0x08000, 0xb06b8920, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_thndrbala, MSX_thndrbala, msx_msx)
STD_ROM_FN(MSX_thndrbala)

struct BurnDriver BurnDrvMSX_thndrbala = {
	"msx_thndrbala", "msx_thndrbal", "msx_msx", NULL, "1985",
	"Thunder Ball (Jpn, Alt)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_thndrbalaRomInfo, MSX_thndrbalaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Thunderbolt (Jpn)

static struct BurnRomInfo MSX_thndboltRomDesc[] = {
	{ "thunderbolt (japan).rom",	0x08000, 0xd8739501, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_thndbolt, MSX_thndbolt, msx_msx)
STD_ROM_FN(MSX_thndbolt)

struct BurnDriver BurnDrvMSX_thndbolt = {
	"msx_thndbolt", NULL, "msx_msx", NULL, "1986",
	"Thunderbolt (Jpn)\0", NULL, "Pixel", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_thndboltRomInfo, MSX_thndboltRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Thunderbolt (Jpn, Alt)

static struct BurnRomInfo MSX_thndboltaRomDesc[] = {
	{ "thunderbolt (japan) (alt 1).rom",	0x08000, 0x6b63047c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_thndbolta, MSX_thndbolta, msx_msx)
STD_ROM_FN(MSX_thndbolta)

struct BurnDriver BurnDrvMSX_thndbolta = {
	"msx_thndbolta", "msx_thndbolt", "msx_msx", NULL, "1986",
	"Thunderbolt (Jpn, Alt)\0", NULL, "Pixel", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_thndboltaRomInfo, MSX_thndboltaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Thunderbolt (Jpn, Alt 2)

static struct BurnRomInfo MSX_thndboltbRomDesc[] = {
	{ "thunderbolt (japan) (alt 2).rom",	0x08000, 0xde3a82f5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_thndboltb, MSX_thndboltb, msx_msx)
STD_ROM_FN(MSX_thndboltb)

struct BurnDriver BurnDrvMSX_thndboltb = {
	"msx_thndboltb", "msx_thndbolt", "msx_msx", NULL, "1986",
	"Thunderbolt (Jpn, Alt 2)\0", NULL, "Pixel", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_thndboltbRomInfo, MSX_thndboltbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Time Pilot (Jpn)

static struct BurnRomInfo MSX_timepltRomDesc[] = {
	{ "time pilot (japan).rom",	0x04000, 0xe4abe008, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_timeplt, MSX_timeplt, msx_msx)
STD_ROM_FN(MSX_timeplt)

struct BurnDriver BurnDrvMSX_timeplt = {
	"msx_timeplt", NULL, "msx_msx", NULL, "1983",
	"Time Pilot (Jpn)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_timepltRomInfo, MSX_timepltRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Time Pilot (Jpn, Alt)

static struct BurnRomInfo MSX_timepltaRomDesc[] = {
	{ "time pilot (japan) (alt 1).rom",	0x04000, 0x23152fc3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_timeplta, MSX_timeplta, msx_msx)
STD_ROM_FN(MSX_timeplta)

struct BurnDriver BurnDrvMSX_timeplta = {
	"msx_timeplta", "msx_timeplt", "msx_msx", NULL, "1983",
	"Time Pilot (Jpn, Alt)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_timepltaRomInfo, MSX_timepltaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Ti Ti! Pang Pang! (Kor)

static struct BurnRomInfo MSX_titipangRomDesc[] = {
	{ "ti ti! pang pang!.rom",	0x08000, 0x2808a3bc, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_titipang, MSX_titipang, msx_msx)
STD_ROM_FN(MSX_titipang)

struct BurnDriver BurnDrvMSX_titipang = {
	"msx_titipang", NULL, "msx_msx", NULL, "1989",
	"Ti Ti! Pang Pang! (Kor)\0", NULL, "Aproman", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_titipangRomInfo, MSX_titipangRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Top Roller! (Jpn)

static struct BurnRomInfo MSX_toprollrRomDesc[] = {
	{ "top roller! (japan).rom",	0x04000, 0xfc609730, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_toprollr, MSX_toprollr, msx_msx)
STD_ROM_FN(MSX_toprollr)

struct BurnDriver BurnDrvMSX_toprollr = {
	"msx_toprollr", NULL, "msx_msx", NULL, "1984",
	"Top Roller! (Jpn)\0", NULL, "Jaleco", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_toprollrRomInfo, MSX_toprollrRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Topple Zip (Jpn)

static struct BurnRomInfo MSX_topzipRomDesc[] = {
	{ "topple zip (japan).rom",	0x08000, 0x190f4ce5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_topzip, MSX_topzip, msx_msx)
STD_ROM_FN(MSX_topzip)

struct BurnDriver BurnDrvMSX_topzip = {
	"msx_topzip", NULL, "msx_msx", NULL, "1986",
	"Topple Zip (Jpn)\0", NULL, "Bothtec", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_topzipRomInfo, MSX_topzipRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// The Tower of Druaga (Jpn)

static struct BurnRomInfo MSX_druagaRomDesc[] = {
	{ "tower of druaga, the (japan).rom",	0x08000, 0x47bef309, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_druaga, MSX_druaga, msx_msx)
STD_ROM_FN(MSX_druaga)

struct BurnDriver BurnDrvMSX_druaga = {
	"msx_druaga", NULL, "msx_msx", NULL, "1986",
	"The Tower of Druaga (Jpn)\0", NULL, "Namcot", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_druagaRomInfo, MSX_druagaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Track & Field 1 (Euro)

static struct BurnRomInfo MSX_trackfldRomDesc[] = {
	{ "track & field 1 (europe).rom",	0x04000, 0xe96b861d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_trackfld, MSX_trackfld, msx_msx)
STD_ROM_FN(MSX_trackfld)

struct BurnDriver BurnDrvMSX_trackfld = {
	"msx_trackfld", NULL, "msx_msx", NULL, "1984",
	"Track & Field 1 (Euro)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_trackfldRomInfo, MSX_trackfldRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Track & Field 2 (Euro)

static struct BurnRomInfo MSX_trackfl2RomDesc[] = {
	{ "track & field 2 (europe).rom",	0x04000, 0xb1ff5899, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_trackfl2, MSX_trackfl2, msx_msx)
STD_ROM_FN(MSX_trackfl2)

struct BurnDriver BurnDrvMSX_trackfl2 = {
	"msx_trackfl2", NULL, "msx_msx", NULL, "1984",
	"Track & Field 2 (Euro)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_trackfl2RomInfo, MSX_trackfl2RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Traffic (Jpn)

static struct BurnRomInfo MSX_trafficRomDesc[] = {
	{ "traffic (japan).rom",	0x08000, 0xb80ebeb4, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_traffic, MSX_traffic, msx_msx)
STD_ROM_FN(MSX_traffic)

struct BurnDriver BurnDrvMSX_traffic = {
	"msx_traffic", NULL, "msx_msx", NULL, "1986",
	"Traffic (Jpn)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_trafficRomInfo, MSX_trafficRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Traffic (Jpn, Alt)

static struct BurnRomInfo MSX_trafficaRomDesc[] = {
	{ "traffic (japan) (alt 1).rom",	0x08000, 0xf6b87d85, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_traffica, MSX_traffica, msx_msx)
STD_ROM_FN(MSX_traffica)

struct BurnDriver BurnDrvMSX_traffica = {
	"msx_traffica", "msx_traffic", "msx_msx", NULL, "1986",
	"Traffic (Jpn, Alt)\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_trafficaRomInfo, MSX_trafficaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Trial Ski (Jpn)

static struct BurnRomInfo MSX_trialskiRomDesc[] = {
	{ "trial ski (japan).rom",	0x04000, 0x152bebc2, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_trialski, MSX_trialski, msx_msx)
STD_ROM_FN(MSX_trialski)

struct BurnDriver BurnDrvMSX_trialski = {
	"msx_trialski", NULL, "msx_msx", NULL, "1984",
	"Trial Ski (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_BASIC, GBF_MISC, 0,
	MSXGetZipName, MSX_trialskiRomInfo, MSX_trialskiRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Triton (Jpn)

static struct BurnRomInfo MSX_tritonRomDesc[] = {
	{ "tritorn (japan).rom",	0x08000, 0xc0a81a48, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_triton, MSX_triton, msx_msx)
STD_ROM_FN(MSX_triton)

struct BurnDriver BurnDrvMSX_triton = {
	"msx_triton", NULL, "msx_msx", NULL, "1986",
	"Triton (Jpn)\0", NULL, "Sein Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_tritonRomInfo, MSX_tritonRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Turboat (Jpn)

static struct BurnRomInfo MSX_turboatRomDesc[] = {
	{ "turboat (japan).rom",	0x04000, 0xc7abc409, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_turboat, MSX_turboat, msx_msx)
STD_ROM_FN(MSX_turboat)

struct BurnDriver BurnDrvMSX_turboat = {
	"msx_turboat", NULL, "msx_msx", NULL, "1984",
	"Turboat (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_turboatRomInfo, MSX_turboatRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Turmoil (Jpn)

static struct BurnRomInfo MSX_turmoilRomDesc[] = {
	{ "turmoil (japan).rom",	0x04000, 0x17683f42, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_turmoil, MSX_turmoil, msx_msx)
STD_ROM_FN(MSX_turmoil)

struct BurnDriver BurnDrvMSX_turmoil = {
	"msx_turmoil", NULL, "msx_msx", NULL, "1984",
	"Turmoil (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_turmoilRomInfo, MSX_turmoilRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Turmoil (Kor)

static struct BurnRomInfo MSX_turmoilkRomDesc[] = {
	{ "turmoil (1984)(qnix).rom",	0x04000, 0x2cf9d4ab, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_turmoilk, MSX_turmoilk, msx_msx)
STD_ROM_FN(MSX_turmoilk)

struct BurnDriver BurnDrvMSX_turmoilk = {
	"msx_turmoilk", "msx_turmoil", "msx_msx", NULL, "1984?",
	"Turmoil (Kor)\0", NULL, "Qnix", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_turmoilkRomInfo, MSX_turmoilkRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Twin Bee (Jpn)

static struct BurnRomInfo MSX_twinbeeRomDesc[] = {
	{ "twin bee (japan).rom",	0x08000, 0xc730d4a4, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_twinbee, MSX_twinbee, msx_msx)
STD_ROM_FN(MSX_twinbee)

struct BurnDriver BurnDrvMSX_twinbee = {
	"msx_twinbee", NULL, "msx_msx", NULL, "1986",
	"Twin Bee (Jpn)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_twinbeeRomInfo, MSX_twinbeeRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Twin Bee (Jpn, Alt)

static struct BurnRomInfo MSX_twinbeeaRomDesc[] = {
	{ "twin bee (japan) (alt 1).rom",	0x08000, 0x71309c8f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_twinbeea, MSX_twinbeea, msx_msx)
STD_ROM_FN(MSX_twinbeea)

struct BurnDriver BurnDrvMSX_twinbeea = {
	"msx_twinbeea", "msx_twinbee", "msx_msx", NULL, "1986",
	"Twin Bee (Jpn, Alt)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_twinbeeaRomInfo, MSX_twinbeeaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Twin Bee (Jpn, Alt 2)

static struct BurnRomInfo MSX_twinbeebRomDesc[] = {
	{ "twin bee (japan) (alt 2).rom",	0x08000, 0x3827a473, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_twinbeeb, MSX_twinbeeb, msx_msx)
STD_ROM_FN(MSX_twinbeeb)

struct BurnDriver BurnDrvMSX_twinbeeb = {
	"msx_twinbeeb", "msx_twinbee", "msx_msx", NULL, "1986",
	"Twin Bee (Jpn, Alt 2)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_twinbeebRomInfo, MSX_twinbeebRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Twin Bee (Jpn, Alt 3)

static struct BurnRomInfo MSX_twinbeecRomDesc[] = {
	{ "twin bee (japan) (alt 3).rom",	0x08000, 0x23260108, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_twinbeec, MSX_twinbeec, msx_msx)
STD_ROM_FN(MSX_twinbeec)

struct BurnDriver BurnDrvMSX_twinbeec = {
	"msx_twinbeec", "msx_twinbee", "msx_msx", NULL, "1986",
	"Twin Bee (Jpn, Alt 3)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_twinbeecRomInfo, MSX_twinbeecRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Twin Hammer (Euro)

static struct BurnRomInfo MSX_twinhammRomDesc[] = {
	{ "twin hammer (europe).rom",	0x08000, 0x28a7cebc, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_twinhamm, MSX_twinhamm, msx_msx)
STD_ROM_FN(MSX_twinhamm)

struct BurnDriver BurnDrvMSX_twinhamm = {
	"msx_twinhamm", NULL, "msx_msx", NULL, "1989",
	"Twin Hammer (Euro)\0", NULL, "Best", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_twinhammRomInfo, MSX_twinhammRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// TZR - GrandPrix Rider (Jpn)

static struct BurnRomInfo MSX_tzrRomDesc[] = {
	{ "tzr - grand prix rider (japan).rom",	0x08000, 0xd792c827, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_tzr, MSX_tzr, msx_msx)
STD_ROM_FN(MSX_tzr)

struct BurnDriver BurnDrvMSX_tzr = {
	"msx_tzr", NULL, "msx_msx", NULL, "1986",
	"TZR - GrandPrix Rider (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_tzrRomInfo, MSX_tzrRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Ultraman (Jpn)

static struct BurnRomInfo MSX_ultramanRomDesc[] = {
	{ "ultraman (japan).rom",	0x04000, 0x973d816d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_ultraman, MSX_ultraman, msx_msx)
STD_ROM_FN(MSX_ultraman)

struct BurnDriver BurnDrvMSX_ultraman = {
	"msx_ultraman", NULL, "msx_msx", NULL, "1984",
	"Ultraman (Jpn)\0", NULL, "Bandai", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_ultramanRomInfo, MSX_ultramanRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Video Hustler (Jpn)

static struct BurnRomInfo MSX_hustlerRomDesc[] = {
	{ "video hustler (japan).rom",	0x02000, 0x64853262, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_hustler, MSX_hustler, msx_msx)
STD_ROM_FN(MSX_hustler)

struct BurnDriver BurnDrvMSX_hustler = {
	"msx_hustler", "msx_konbill", "msx_msx", NULL, "1984",
	"Video Hustler (Jpn)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_hustlerRomInfo, MSX_hustlerRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Video Hustler (Jpn, Alt)

static struct BurnRomInfo MSX_hustleraRomDesc[] = {
	{ "video hustler (japan) (alt 1).rom",	0x02000, 0x8d99c3b0, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_hustlera, MSX_hustlera, msx_msx)
STD_ROM_FN(MSX_hustlera)

struct BurnDriver BurnDrvMSX_hustlera = {
	"msx_hustlera", "msx_konbill", "msx_msx", NULL, "1984",
	"Video Hustler (Jpn, Alt)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_hustleraRomInfo, MSX_hustleraRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Vigilante (Kor)

static struct BurnRomInfo MSX_vigilantRomDesc[] = {
	{ "vigilante_(clover).rom",	0x08000, 0x2619f221, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_vigilant, MSX_vigilant, msx_msx)
STD_ROM_FN(MSX_vigilant)

struct BurnDriver BurnDrvMSX_vigilant = {
	"msx_vigilant", NULL, "msx_msx", NULL, "1990",
	"Vigilante (Kor)\0", NULL, "Clover", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_vigilantRomInfo, MSX_vigilantRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Volguard (Jpn)

static struct BurnRomInfo MSX_volguardRomDesc[] = {
	{ "volguard (japan).rom",	0x08000, 0x874e59fb, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_volguard, MSX_volguard, msx_msx)
STD_ROM_FN(MSX_volguard)

struct BurnDriver BurnDrvMSX_volguard = {
	"msx_volguard", NULL, "msx_msx", NULL, "1985",
	"Volguard (Jpn)\0", NULL, "dB-Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_volguardRomInfo, MSX_volguardRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Warp & Warp (Jpn)

static struct BurnRomInfo MSX_warpwarpRomDesc[] = {
	{ "warp & warp (japan).rom",	0x02000, 0x90f5f414, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_warpwarp, MSX_warpwarp, msx_msx)
STD_ROM_FN(MSX_warpwarp)

struct BurnDriver BurnDrvMSX_warpwarp = {
	"msx_warpwarp", NULL, "msx_msx", NULL, "1984",
	"Warp & Warp (Jpn)\0", NULL, "Namcot", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_warpwarpRomInfo, MSX_warpwarpRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Warroid (Jpn)

static struct BurnRomInfo MSX_warroidRomDesc[] = {
	{ "warroid (japan).rom",	0x08000, 0xa0a19fd5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_warroid, MSX_warroid, msx_msx)
STD_ROM_FN(MSX_warroid)

struct BurnDriver BurnDrvMSX_warroid = {
	"msx_warroid", NULL, "msx_msx", NULL, "1985",
	"Warroid (Jpn)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_warroidRomInfo, MSX_warroidRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Warroid (Jpn, Alt)

static struct BurnRomInfo MSX_warroidaRomDesc[] = {
	{ "warroid (japan) (alt 1).rom",	0x08000, 0xe621ebc9, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_warroida, MSX_warroida, msx_msx)
STD_ROM_FN(MSX_warroida)

struct BurnDriver BurnDrvMSX_warroida = {
	"msx_warroida", "msx_warroid", "msx_msx", NULL, "1985",
	"Warroid (Jpn, Alt)\0", NULL, "ASCII", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_warroidaRomInfo, MSX_warroidaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Water Driver (Jpn)

static struct BurnRomInfo MSX_waterdrvRomDesc[] = {
	{ "water driver (japan).rom",	0x04000, 0xd5af9e82, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_waterdrv, MSX_waterdrv, msx_msx)
STD_ROM_FN(MSX_waterdrv)

struct BurnDriver BurnDrvMSX_waterdrv = {
	"msx_waterdrv", NULL, "msx_msx", NULL, "1984",
	"Water Driver (Jpn)\0", NULL, "Colpax?", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_waterdrvRomInfo, MSX_waterdrvRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Wedding Bells (Jpn)

static struct BurnRomInfo MSX_wbellsRomDesc[] = {
	{ "wedding bells (1984)(nippon columbia - colpax - universal).rom",	0x04000, 0x00585f00, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_wbells, MSX_wbells, msx_msx)
STD_ROM_FN(MSX_wbells)

struct BurnDriver BurnDrvMSX_wbells = {
	"msx_wbells", NULL, "msx_msx", NULL, "1984",
	"Wedding Bells (Jpn)\0", NULL, "Nihon Columbia", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_wbellsRomInfo, MSX_wbellsRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Wedding Bells (Jpn, Alt)

static struct BurnRomInfo MSX_wbellsaRomDesc[] = {
	{ "wedding bells (1984)(nippon columbia - colpax - universal)[a].rom",	0x04000, 0xc41c55cb, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_wbellsa, MSX_wbellsa, msx_msx)
STD_ROM_FN(MSX_wbellsa)

struct BurnDriver BurnDrvMSX_wbellsa = {
	"msx_wbellsa", "msx_wbells", "msx_msx", NULL, "1984",
	"Wedding Bells (Jpn, Alt)\0", NULL, "Nihon Columbia", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_wbellsaRomInfo, MSX_wbellsaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Wonsiin (Kor)

static struct BurnRomInfo MSX_wonsiinRomDesc[] = {
	{ "wonsiin (kr).rom",	0x20000, 0xa05258f5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_wonsiin, MSX_wonsiin, msx_msx)
STD_ROM_FN(MSX_wonsiin)

struct BurnDriver BurnDrvMSX_wonsiin = {
	"msx_wonsiin", NULL, "msx_msx", NULL, "1991",
	"Wonsiin (Kor)\0", NULL, "Zemina", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_KONAMI, GBF_MISC, 0,
	MSXGetZipName, MSX_wonsiinRomInfo, MSX_wonsiinRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Wrangler (Spa)

static struct BurnRomInfo MSX_wranglerRomDesc[] = {
	{ "wrangler (spain).rom",	0x04000, 0x7185f5e8, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_wrangler, MSX_wrangler, msx_msx)
STD_ROM_FN(MSX_wrangler)

struct BurnDriver BurnDrvMSX_wrangler = {
	"msx_wrangler", NULL, "msx_msx", NULL, "1985",
	"Wrangler (Spa)\0", NULL, "Sony Spain", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_wranglerRomInfo, MSX_wranglerRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// The Wreck (Euro)

static struct BurnRomInfo MSX_wreckRomDesc[] = {
	{ "wreck, the (europe).rom",	0x08000, 0x7efde800, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_wreck, MSX_wreck, msx_msx)
STD_ROM_FN(MSX_wreck)

struct BurnDriver BurnDrvMSX_wreck = {
	"msx_wreck", NULL, "msx_msx", NULL, "1984",
	"The Wreck (Euro)\0", NULL, "Electric Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_wreckRomInfo, MSX_wreckRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Xyzolog (Jpn)

static struct BurnRomInfo MSX_xyzologRomDesc[] = {
	{ "xyxolog (japan).rom",	0x04000, 0xc7303910, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_xyzolog, MSX_xyzolog, msx_msx)
STD_ROM_FN(MSX_xyzolog)

struct BurnDriver BurnDrvMSX_xyzolog = {
	"msx_xyzolog", NULL, "msx_msx", NULL, "1984",
	"Xyzolog (Jpn)\0", NULL, "Nidecom", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_xyzologRomInfo, MSX_xyzologRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Xyzolog (Jpn, Alt)

static struct BurnRomInfo MSX_xyzologaRomDesc[] = {
	{ "xyxolog (japan) (alt 1).rom",	0x04000, 0xc9074b91, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_xyzologa, MSX_xyzologa, msx_msx)
STD_ROM_FN(MSX_xyzologa)

struct BurnDriver BurnDrvMSX_xyzologa = {
	"msx_xyzologa", "msx_xyzolog", "msx_msx", NULL, "1984",
	"Xyzolog (Jpn, Alt)\0", NULL, "Nidecom", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_xyzologaRomInfo, MSX_xyzologaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Yakyu Kyo (Jpn)

static struct BurnRomInfo MSX_yakyukyoRomDesc[] = {
	{ "honkball (japan).rom",	0x08000, 0x24b94274, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_yakyukyo, MSX_yakyukyo, msx_msx)
STD_ROM_FN(MSX_yakyukyo)

struct BurnDriver BurnDrvMSX_yakyukyo = {
	"msx_yakyukyo", NULL, "msx_msx", NULL, "1985",
	"Yakyu Kyo (Jpn)\0", NULL, "Hudson Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_yakyukyoRomInfo, MSX_yakyukyoRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Yami no Ryuuou - Hades no Monshou (Jpn)

static struct BurnRomInfo MSX_hadesRomDesc[] = {
	{ "hades no monsho (japan).rom",	0x08000, 0x4d105f57, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_hades, MSX_hades, msx_msx)
STD_ROM_FN(MSX_hades)

struct BurnDriver BurnDrvMSX_hades = {
	"msx_hades", NULL, "msx_msx", NULL, "1986",
	"Yami no Ryuuou - Hades no Monshou (Jpn)\0", NULL, "Casio", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_hadesRomInfo, MSX_hadesRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Yami no Ryuuou - Hades no Monshou (Jpn, Alt)

static struct BurnRomInfo MSX_hadesaRomDesc[] = {
	{ "hades no monsho (japan) (alt 1).rom",	0x08000, 0xf42f4a15, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_hadesa, MSX_hadesa, msx_msx)
STD_ROM_FN(MSX_hadesa)

struct BurnDriver BurnDrvMSX_hadesa = {
	"msx_hadesa", "msx_hades", "msx_msx", NULL, "1986",
	"Yami no Ryuuou - Hades no Monshou (Jpn, Alt)\0", NULL, "Casio", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_hadesaRomInfo, MSX_hadesaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Yellow Submarine (Jpn)

static struct BurnRomInfo MSX_yellowsbRomDesc[] = {
	{ "yellow submarine (japan).rom",	0x04000, 0x71e6605f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_yellowsb, MSX_yellowsb, msx_msx)
STD_ROM_FN(MSX_yellowsb)

struct BurnDriver BurnDrvMSX_yellowsb = {
	"msx_yellowsb", NULL, "msx_msx", NULL, "1987",
	"Yellow Submarine (Jpn)\0", NULL, "Brother Kougyou", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_yellowsbRomInfo, MSX_yellowsbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Yie Ar Kung-Fu (Jpn)

static struct BurnRomInfo MSX_yiearRomDesc[] = {
	{ "yie ar kung-fu (japan).rom",	0x04000, 0x8a12ec4f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_yiear, MSX_yiear, msx_msx)
STD_ROM_FN(MSX_yiear)

struct BurnDriver BurnDrvMSX_yiear = {
	"msx_yiear", NULL, "msx_msx", NULL, "1985",
	"Yie Ar Kung-Fu (Jpn)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_yiearRomInfo, MSX_yiearRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Yie Ar Kung-Fu (Jpn, Alt)

static struct BurnRomInfo MSX_yiearaRomDesc[] = {
	{ "yie ar kung-fu (japan) (alt 1).rom",	0x04000, 0x0e197b49, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_yieara, MSX_yieara, msx_msx)
STD_ROM_FN(MSX_yieara)

struct BurnDriver BurnDrvMSX_yieara = {
	"msx_yieara", "msx_yiear", "msx_msx", NULL, "1985",
	"Yie Ar Kung-Fu (Jpn, Alt)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_yiearaRomInfo, MSX_yiearaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Yie Ar Kung-Fu (Kor)

static struct BurnRomInfo MSX_yiearkRomDesc[] = {
	{ "yakungfu.rom",	0x08000, 0x372086b5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_yieark, MSX_yieark, msx_msx)
STD_ROM_FN(MSX_yieark)

struct BurnDriver BurnDrvMSX_yieark = {
	"msx_yieark", "msx_yiear", "msx_msx", NULL, "198?",
	"Yie Ar Kung-Fu (Kor)\0", NULL, "Unknown", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_yiearkRomInfo, MSX_yiearkRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Yie Ar Kung-Fu II - The Emperor Yie-Gah (Jpn)

static struct BurnRomInfo MSX_yiear2RomDesc[] = {
	{ "yie ar kung-fu ii - the emperor yie-gah (japan).rom",	0x08000, 0xd66034a9, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_yiear2, MSX_yiear2, msx_msx)
STD_ROM_FN(MSX_yiear2)

struct BurnDriver BurnDrvMSX_yiear2 = {
	"msx_yiear2", NULL, "msx_msx", NULL, "1985",
	"Yie Ar Kung-Fu II - The Emperor Yie-Gah (Jpn)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_yiear2RomInfo, MSX_yiear2RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Yie Ar Kung-Fu II - The Emperor Yie-Gah (Jpn, Alt)

static struct BurnRomInfo MSX_yiear2aRomDesc[] = {
	{ "yie ar kung-fu ii - the emperor yie-gah (japan) (alt 1).rom",	0x08000, 0x8099f5c7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_yiear2a, MSX_yiear2a, msx_msx)
STD_ROM_FN(MSX_yiear2a)

struct BurnDriver BurnDrvMSX_yiear2a = {
	"msx_yiear2a", "msx_yiear2", "msx_msx", NULL, "1985",
	"Yie Ar Kung-Fu II - The Emperor Yie-Gah (Jpn, Alt)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_yiear2aRomInfo, MSX_yiear2aRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Yie Ar Kung-Fu II - The Emperor Yie-Gah (Jpn, Alt 2)

static struct BurnRomInfo MSX_yiear2bRomDesc[] = {
	{ "yie ar kung-fu ii - the emperor yie-gah (japan) (alt 2).rom",	0x08000, 0x70e679c3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_yiear2b, MSX_yiear2b, msx_msx)
STD_ROM_FN(MSX_yiear2b)

struct BurnDriver BurnDrvMSX_yiear2b = {
	"msx_yiear2b", "msx_yiear2", "msx_msx", NULL, "1985",
	"Yie Ar Kung-Fu II - The Emperor Yie-Gah (Jpn, Alt 2)\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_yiear2bRomInfo, MSX_yiear2bRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Youkai Tantei Chima Chima (Jpn)

static struct BurnRomInfo MSX_chimaRomDesc[] = {
	{ "yokai tanken chimachima (japan).rom",	0x08000, 0x8d636963, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_chima, MSX_chima, msx_msx)
STD_ROM_FN(MSX_chima)

struct BurnDriver BurnDrvMSX_chima = {
	"msx_chima", NULL, "msx_msx", NULL, "1985",
	"Youkai Tantei Chima Chima (Jpn)\0", NULL, "Bothtec", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_chimaRomInfo, MSX_chimaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Youkai Yashiki (Jpn)

static struct BurnRomInfo MSX_youkaiyaRomDesc[] = {
	{ "haunted boynight (japan).rom",	0x08000, 0x2faf6e26, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_youkaiya, MSX_youkaiya, msx_msx)
STD_ROM_FN(MSX_youkaiya)

struct BurnDriver BurnDrvMSX_youkaiya = {
	"msx_youkaiya", NULL, "msx_msx", NULL, "1986",
	"Haunted Boy. Youkai Yashiki (Jpn)\0", NULL, "Casio", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_youkaiyaRomInfo, MSX_youkaiyaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Chou Senshi Zaider - Battle of Peguss (Jpn)

static struct BurnRomInfo MSX_zaiderRomDesc[] = {
	{ "zaider - battle of peguss (japan).rom",	0x08000, 0xbeccf133, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_zaider, MSX_zaider, msx_msx)
STD_ROM_FN(MSX_zaider)

struct BurnDriver BurnDrvMSX_zaider = {
	"msx_zaider", NULL, "msx_msx", NULL, "1986",
	"Chou Senshi Zaider - Battle of Peguss (Jpn)\0", NULL, "Cosmos Computer", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_zaiderRomInfo, MSX_zaiderRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Chou Senshi Zaider - Battle of Peguss (Jpn, Alt)

static struct BurnRomInfo MSX_zaideraRomDesc[] = {
	{ "zaider - battle of peguss (japan) (alt 1).rom",	0x08000, 0x453bcd36, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_zaidera, MSX_zaidera, msx_msx)
STD_ROM_FN(MSX_zaidera)

struct BurnDriver BurnDrvMSX_zaidera = {
	"msx_zaidera", "msx_zaider", "msx_msx", NULL, "1986",
	"Chou Senshi Zaider - Battle of Peguss (Jpn, Alt)\0", NULL, "Cosmos Computer", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_zaideraRomInfo, MSX_zaideraRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Zanac 2nd Version (Jpn)

static struct BurnRomInfo MSX_zanac2RomDesc[] = {
	{ "zanac (japan) (v2).rom",	0x08000, 0x8f1917d4, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_zanac2, MSX_zanac2, msx_msx)
STD_ROM_FN(MSX_zanac2)

struct BurnDriver BurnDrvMSX_zanac2 = {
	"msx_zanac2", NULL, "msx_msx", NULL, "1987",
	"Zanac 2nd Version (Jpn)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_zanac2RomInfo, MSX_zanac2RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Zanac 2nd Version (Jpn, Alt)

static struct BurnRomInfo MSX_zanac2aRomDesc[] = {
	{ "zanac (japan) (v2) (alt 1).rom",	0x08000, 0x8414ea0e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_zanac2a, MSX_zanac2a, msx_msx)
STD_ROM_FN(MSX_zanac2a)

struct BurnDriver BurnDrvMSX_zanac2a = {
	"msx_zanac2a", "msx_zanac2", "msx_msx", NULL, "1987",
	"Zanac 2nd Version (Jpn, Alt)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_zanac2aRomInfo, MSX_zanac2aRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Zanac (Jpn)

static struct BurnRomInfo MSX_zanacRomDesc[] = {
	{ "zanac (japan).rom",	0x08000, 0x425e0d34, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_zanac, MSX_zanac, msx_msx)
STD_ROM_FN(MSX_zanac)

struct BurnDriver BurnDrvMSX_zanac = {
	"msx_zanac", NULL, "msx_msx", NULL, "1986",
	"Zanac (Jpn)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_zanacRomInfo, MSX_zanacRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Zanac (Jpn, Alt)

static struct BurnRomInfo MSX_zanacaRomDesc[] = {
	{ "zanac (japan) (alt 1).rom",	0x08000, 0x7f95533c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_zanaca, MSX_zanaca, msx_msx)
STD_ROM_FN(MSX_zanaca)

struct BurnDriver BurnDrvMSX_zanaca = {
	"msx_zanaca", "msx_zanac", "msx_msx", NULL, "1986",
	"Zanac (Jpn, Alt)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_zanacaRomInfo, MSX_zanacaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Zaxxon (Jpn)

static struct BurnRomInfo MSX_zaxxonRomDesc[] = {
	{ "zaxxon (japan).rom",	0x08000, 0x0b3ba595, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_zaxxon, MSX_zaxxon, msx_msx)
STD_ROM_FN(MSX_zaxxon)

struct BurnDriver BurnDrvMSX_zaxxon = {
	"msx_zaxxon", NULL, "msx_msx", NULL, "1985",
	"Zaxxon (Jpn)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_zaxxonRomInfo, MSX_zaxxonRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Zaxxon (Jpn, Alt)

static struct BurnRomInfo MSX_zaxxonaRomDesc[] = {
	{ "zaxxon (japan) (alt 1).rom",	0x08000, 0x353dd532, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_zaxxona, MSX_zaxxona, msx_msx)
STD_ROM_FN(MSX_zaxxona)

struct BurnDriver BurnDrvMSX_zaxxona = {
	"msx_zaxxona", "msx_zaxxon", "msx_msx", NULL, "1985",
	"Zaxxon (Jpn, Alt)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_zaxxonaRomInfo, MSX_zaxxonaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Zemmix Baduk Gyosil (Kor)

static struct BurnRomInfo MSX_badukRomDesc[] = {
	{ "baduk.rom",	0x04000, 0x5ad4442d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_baduk, MSX_baduk, msx_msx)
STD_ROM_FN(MSX_baduk)

struct BurnDriver BurnDrvMSX_baduk = {
	"msx_baduk", NULL, "msx_msx", NULL, "19??",
	"Zemmix Baduk Gyosil (Kor)\0", NULL, "Unknown", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_badukRomInfo, MSX_badukRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Zenji (Jpn)

static struct BurnRomInfo MSX_zenjiRomDesc[] = {
	{ "zenji (japan).rom",	0x04000, 0x77b3b0b9, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_zenji, MSX_zenji, msx_msx)
STD_ROM_FN(MSX_zenji)

struct BurnDriver BurnDrvMSX_zenji = {
	"msx_zenji", NULL, "msx_msx", NULL, "1984",
	"Zenji (Jpn)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_zenjiRomInfo, MSX_zenjiRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Zexas Limited (Jpn)

static struct BurnRomInfo MSX_zexasltdRomDesc[] = {
	{ "zexas limited (japan).rom",	0x08000, 0xefefa02a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_zexasltd, MSX_zexasltd, msx_msx)
STD_ROM_FN(MSX_zexasltd)

struct BurnDriver BurnDrvMSX_zexasltd = {
	"msx_zexasltd", NULL, "msx_msx", NULL, "1985",
	"Zexas Limited (Jpn)\0", NULL, "dB-Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_zexasltdRomInfo, MSX_zexasltdRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Zexas Limited (Jpn, Alt)

static struct BurnRomInfo MSX_zexasltdaRomDesc[] = {
	{ "zexas limited (japan) (alt 1).rom",	0x08000, 0x01a73292, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_zexasltda, MSX_zexasltda, msx_msx)
STD_ROM_FN(MSX_zexasltda)

struct BurnDriver BurnDrvMSX_zexasltda = {
	"msx_zexasltda", "msx_zexasltd", "msx_msx", NULL, "1985",
	"Zexas Limited (Jpn, Alt)\0", NULL, "dB-Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_zexasltdaRomInfo, MSX_zexasltdaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Zexas Limited (Jpn, Alt 2)

static struct BurnRomInfo MSX_zexasltdbRomDesc[] = {
	{ "zexas limited (japan) (alt 2).rom",	0x08000, 0x1e48edca, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_zexasltdb, MSX_zexasltdb, msx_msx)
STD_ROM_FN(MSX_zexasltdb)

struct BurnDriver BurnDrvMSX_zexasltdb = {
	"msx_zexasltdb", "msx_zexasltd", "msx_msx", NULL, "1985",
	"Zexas Limited (Jpn, Alt 2)\0", NULL, "dB-Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_zexasltdbRomInfo, MSX_zexasltdbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Zoom 909 (Jpn)

static struct BurnRomInfo MSX_zoom909RomDesc[] = {
	{ "zoom 909 (japan).rom",	0x08000, 0x64283863, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_zoom909, MSX_zoom909, msx_msx)
STD_ROM_FN(MSX_zoom909)

struct BurnDriver BurnDrvMSX_zoom909 = {
	"msx_zoom909", NULL, "msx_msx", NULL, "1986",
	"Zoom 909 (Jpn)\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_zoom909RomInfo, MSX_zoom909RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Zoom 909 (Kor)

static struct BurnRomInfo MSX_zoom909kRomDesc[] = {
	{ "zoom 909 (1985)(sega)[cr prosoft].rom",	0x08000, 0x34b186ad, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_zoom909k, MSX_zoom909k, msx_msx)
STD_ROM_FN(MSX_zoom909k)

struct BurnDriver BurnDrvMSX_zoom909k = {
	"msx_zoom909k", "msx_zoom909", "msx_msx", NULL, "19??",
	"Zoom 909 (Kor)\0", NULL, "Prosoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_zoom909kRomInfo, MSX_zoom909kRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Mars II (Jpn)

static struct BurnRomInfo MSX_mars2RomDesc[] = {
	{ "mars ii (japan).rom",	0x02000, 0x48e9d13b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mars2, MSX_mars2, msx_msx)
STD_ROM_FN(MSX_mars2)

struct BurnDriver BurnDrvMSX_mars2 = {
	"msx_mars2", NULL, "msx_msx", NULL, "1990",
	"Mars II (Jpn)\0", NULL, "Nagi-P Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_mars2RomInfo, MSX_mars2RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Bousou Tokkyuu SOS. Stop the Express (1985)(Hudson Soft)(JP)

static struct BurnRomInfo MSX_bousousosRomDesc[] = {
	{ "bousoutokkyuusos.rom",	0x08000, 0x334efc07, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_bousousos, MSX_bousousos, msx_msx)
STD_ROM_FN(MSX_bousousos)

struct BurnDriver BurnDrvMSX_bousousos = {
	"msx_bousousos", NULL, "msx_msx", NULL, "1985",
	"Stop the Express. Bousou Tokkyuu SOS (Jpn)\0", NULL, "Hudson Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_bousousosRomInfo, MSX_bousousosRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Bousou Tokkyuu SOS. Stop the Express (1985)(Hudson Soft)(JP) ALT

static struct BurnRomInfo MSX_bousousosaRomDesc[] = {
	{ "bousoutokkyuusosa.rom",	0x04000, 0xb6063493, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_bousousosa, MSX_bousousosa, msx_msx)
STD_ROM_FN(MSX_bousousosa)

struct BurnDriver BurnDrvMSX_bousousosa = {
	"msx_bousousosa", "msx_bousousos", "msx_msx", NULL, "1985",
	"Stop the Express. Bousou Tokkyuu SOS (Jpn, Alt)\0", NULL, "Hudson Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_bousousosaRomInfo, MSX_bousousosaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Invasion of the Zombie Monsters

static struct BurnRomInfo MSX_invazmRomDesc[] = {
	{ "RLV904_Invasion_of_the_Zombie_Monsters.rom",	0x08000, 0xa232351f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_invazm, MSX_invazm, msx_msx)
STD_ROM_FN(MSX_invazm)

struct BurnDriver BurnDrvMSX_invazm = {
	"msx_invazm", NULL, "msx_msx", NULL, "2010",
	"Invasion of the Zombie Monsters [RLV904]\0", NULL, "RELEVO", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_invazmRomInfo, MSX_invazmRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Gyruss

static struct BurnRomInfo MSX_gyrussRomDesc[] = {
	{ "Gyruss.rom",	0x10000, 0x55335dd3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_gyruss, MSX_gyruss, msx_msx)
STD_ROM_FN(MSX_gyruss)

struct BurnDriver BurnDrvMSX_gyruss = {
	"msx_gyruss", NULL, "msx_msx", NULL, "1984",
	"Gyruss\0", NULL, "Parker Bros.", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_gyrussRomInfo, MSX_gyrussRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Hype

static struct BurnRomInfo MSX_hypeRomDesc[] = {
	{ "hype.rom",	0x08000, 0xf9c5977a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_hype, MSX_hype, msx_msx)
STD_ROM_FN(MSX_hype)

struct BurnDriver BurnDrvMSX_hype = {
	"msx_hype", NULL, "msx_msx", NULL, "1987",
	"Hype\0", NULL, "The Bytebusters", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_hypeRomInfo, MSX_hypeRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// J.E.T.P.A.C.

static struct BurnRomInfo MSX_jetpacRomDesc[] = {
	{ "jetpac.rom",	0x08000, 0x38ad7112, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_jetpac, MSX_jetpac, msx_msx)
STD_ROM_FN(MSX_jetpac)

struct BurnDriver BurnDrvMSX_jetpac = {
	"msx_jetpac", NULL, "msx_msx", NULL, "2009",
	"J.E.T.P.A.C.\0", NULL, "Imanok", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_jetpacRomInfo, MSX_jetpacRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Mikie

static struct BurnRomInfo MSX_mikieRomDesc[] = {
	{ "mikie.rom",	0x08000, 0xf4567a08, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mikie, MSX_mikie, msx_msx)
STD_ROM_FN(MSX_mikie)

struct BurnDriver BurnDrvMSX_mikie = {
	"msx_mikie", NULL, "msx_msx", NULL, "1984",
	"Mikie. Shinnyuushain Tooru-Kun.\0", NULL, "Konami", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_mikieRomInfo, MSX_mikieRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Skate Air

static struct BurnRomInfo MSX_skateairRomDesc[] = {
	{ "SKATE_AIR_en.rom",	0x20000, 0x6398a569, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_skateair, MSX_skateair, msx_msx)
STD_ROM_FN(MSX_skateair)

struct BurnDriver BurnDrvMSX_skateair = {
	"msx_skateair", NULL, "msx_msx", NULL, "2006",
	"Skate Air\0", NULL, "Yerami Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_skateairRomInfo, MSX_skateairRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Universe Unknown

static struct BurnRomInfo MSX_univunkRomDesc[] = {
	{ "UniverseUnknown.rom",	0x0c000, 0xa017f389, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_univunk, MSX_univunk, msx_msx)
STD_ROM_FN(MSX_univunk)

struct BurnDriver BurnDrvMSX_univunk = {
	"msx_univunk", NULL, "msx_msx", NULL, "2005",
	"Universe Unknown\0", NULL, "Infinite", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_univunkRomInfo, MSX_univunkRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Wonder Kid. Adventure Kid.

static struct BurnRomInfo MSX_wondkidRomDesc[] = {
	{ "Adventure_Kid (ASCII 16).rom",	0x20000, 0xcd8b5dd6, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_wondkid, MSX_wondkid, msx_msx)
STD_ROM_FN(MSX_wondkid)

struct BurnDriver BurnDrvMSX_wondkid = {
	"msx_wondkid", NULL, "msx_msx", NULL, "1993",
	"Wonder Kid. Adventure Kid.\0", NULL, "Open", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII16, GBF_MISC, 0,
	MSXGetZipName, MSX_wondkidRomInfo, MSX_wondkidRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Zaxxon - Electric Software

static struct BurnRomInfo MSX_zaxxonesRomDesc[] = {
	{ "Zaxxon (Electric Software).rom",	0x08000, 0x2ef2142d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_zaxxones, MSX_zaxxones, msx_msx)
STD_ROM_FN(MSX_zaxxones)

struct BurnDriver BurnDrvMSX_zaxxones = {
	"msx_zaxxones", NULL, "msx_msx", NULL, "1985",
	"Zaxxon (Electric Software)\0", NULL, "Electric Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_zaxxonesRomInfo, MSX_zaxxonesRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Mecha-8

static struct BurnRomInfo MSX_mecha8RomDesc[] = {
	{ "mecha8.rom",	0x08000, 0xc60e346e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mecha8, MSX_mecha8, msx_msx)
STD_ROM_FN(MSX_mecha8)

struct BurnDriver BurnDrvMSX_mecha8 = {
	"msx_mecha8", NULL, "msx_msx", NULL, "2011",
	"Mecha-8\0", NULL, "Oscar Toledo G.", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_mecha8RomInfo, MSX_mecha8RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Zombie Incident

static struct BurnRomInfo MSX_zombieincRomDesc[] = {
	{ "ZombieIncident.rom",	0x0c000, 0x0d5c497a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_zombieinc, MSX_zombieinc, msx_msx)
STD_ROM_FN(MSX_zombieinc)

struct BurnDriver BurnDrvMSX_zombieinc = {
	"msx_zombieinc12", NULL, "msx_msx", NULL, "2013",
	"Zombie Incident (v1.2)\0", NULL, "NENEFRANZ", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_zombieincRomInfo, MSX_zombieincRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Alter Ego

static struct BurnRomInfo MSX_alteregoRomDesc[] = {
	{ "alterego.rom",	0x0c000, 0x41b51564, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_alterego, MSX_alterego, msx_msx)
STD_ROM_FN(MSX_alterego)

struct BurnDriver BurnDrvMSX_alterego = {
	"msx_alterego", NULL, "msx_msx", NULL, "2011",
	"Alter Ego\0", NULL, "The New Image", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_alteregoRomInfo, MSX_alteregoRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// M-Tanks

static struct BurnRomInfo MSX_mtanksRomDesc[] = {
	{ "mtanks.rom",	0x20000, 0xc0efa4ff, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mtanks, MSX_mtanks, msx_msx)
STD_ROM_FN(MSX_mtanks)

struct BurnDriver BurnDrvMSX_mtanks = {
	"msx_mtanks", NULL, "msx_msx", NULL, "2011",
	"M-Tanks\0", NULL, "Assembler Games", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_mtanksRomInfo, MSX_mtanksRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// JumpinG

static struct BurnRomInfo MSX_jumpingRomDesc[] = {
	{ "DMZ001-JpinG_English.rom",	0x08000, 0x7f492f09, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_jumping, MSX_jumping, msx_msx)
STD_ROM_FN(MSX_jumping)

struct BurnDriver BurnDrvMSX_jumping = {
	"msx_jumping", NULL, "msx_msx", NULL, "2011",
	"JumpinG (eng)\0", NULL, "DimensionZ", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_jumpingRomInfo, MSX_jumpingRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Kill Mice

static struct BurnRomInfo MSX_killmiceRomDesc[] = {
	{ "killmice.rom",	0x10000, 0x783afacb, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_killmice, MSX_killmice, msx_msx)
STD_ROM_FN(MSX_killmice)

struct BurnDriver BurnDrvMSX_killmice = {
	"msx_killmice", NULL, "msx_msx", NULL, "2011",
	"Kill Mice\0", NULL, "Gamecast Entertainment", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_killmiceRomInfo, MSX_killmiceRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Shouganai

static struct BurnRomInfo MSX_shouganai11RomDesc[] = {
	{ "shoganai.rom",	0x04000, 0xd27ea890, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_shouganai11, MSX_shouganai11, msx_msx)
STD_ROM_FN(MSX_shouganai11)

struct BurnDriver BurnDrvMSX_shouganai11 = {
	"msx_shouganai11", NULL, "msx_msx", NULL, "2013",
	"Shouganai\0", NULL, "Paxanga Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_shouganai11RomInfo, MSX_shouganai11RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Malaika

static struct BurnRomInfo MSX_malaikaRomDesc[] = {
	{ "RLV914_MALAIKA.rom",	0x04000, 0x37c13320, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_malaika, MSX_malaika, msx_msx)
STD_ROM_FN(MSX_malaika)

struct BurnDriver BurnDrvMSX_malaika = {
	"msx_malaika12", NULL, "msx_msx", NULL, "2013",
	"Malaika (v1.2)\0", NULL, "RELEVO", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_malaikaRomInfo, MSX_malaikaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Shmup!

static struct BurnRomInfo MSX_shmupRomDesc[] = {
	{ "shmup!.rom",	0x04000, 0x99cca8a4, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_shmup, MSX_shmup, msx_msx)
STD_ROM_FN(MSX_shmup)

struct BurnDriver BurnDrvMSX_shmup = {
	"msx_shmup11", NULL, "msx_msx", NULL, "2013",
	"Shmup! (v1.1)\0", NULL, "IMANOK", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_shmupRomInfo, MSX_shmupRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Gommy

static struct BurnRomInfo MSX_gommyRomDesc[] = {
	{ "DMZ-004_Gommy MD_msx.rom",	0x04000, 0x66b27261, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_gommy, MSX_gommy, msx_msx)
STD_ROM_FN(MSX_gommy)

struct BurnDriver BurnDrvMSX_gommy = {
	"msx_gommy", NULL, "msx_msx", NULL, "2013",
	"Gommy\0", NULL, "Retroworks/Dimension Z/NENEFRANZ", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_gommyRomInfo, MSX_gommyRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Teodoro No Sabe Volar

static struct BurnRomInfo MSX_teodoroRomDesc[] = {
	{ "DMZ002 - Teodoro no sabe volar (English).rom",	0x08000, 0xd2d53275, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_teodoro, MSX_teodoro, msx_msx)
STD_ROM_FN(MSX_teodoro)

struct BurnDriver BurnDrvMSX_teodoro = {
	"msx_teodoro", NULL, "msx_msx", NULL, "2012",
	"Teodoro No Sabe Volar\0", NULL, "Retroworks/Dimension Z", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_teodoroRomInfo, MSX_teodoroRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Subacuatic

static struct BurnRomInfo MSX_subacuaticRomDesc[] = {
	{ "Subacuatic_(2012)_(rlv911).rom",	0x08000, 0x91884cc0, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_subacuatic, MSX_subacuatic, msx_msx)
STD_ROM_FN(MSX_subacuatic)

struct BurnDriver BurnDrvMSX_subacuatic = {
	"msx_subacuatic", NULL, "msx_msx", NULL, "2012",
	"Subacuatic\0", NULL, "RELEVO / The Mojon Twins", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_subacuaticRomInfo, MSX_subacuaticRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Heroes Arena

static struct BurnRomInfo MSX_heroesarenaRomDesc[] = {
	{ "h-arena.rom",	0x08000, 0x3c6c9c72, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_heroesarena, MSX_heroesarena, msx_msx)
STD_ROM_FN(MSX_heroesarena)

struct BurnDriver BurnDrvMSX_heroesarena = {
	"msx_heroesarena", NULL, "msx_msx", NULL, "2010",
	"Heroes Arena\0", NULL, "IMANOK", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_heroesarenaRomInfo, MSX_heroesarenaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Zombie Near

static struct BurnRomInfo MSX_zombienearRomDesc[] = {
	{ "Zombie Near.rom",	0x20000, 0xedea8c56, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_zombienear, MSX_zombienear, msx_msx)
STD_ROM_FN(MSX_zombienear)

struct BurnDriver BurnDrvMSX_zombienear = {
	"msx_zombienear11", NULL, "msx_msx", NULL, "2011",
	"Zombie Near\0", NULL, "Oscar Toledo G.", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_zombienearRomInfo, MSX_zombienearRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Menace

static struct BurnRomInfo MSX_menaceRomDesc[] = {
	{ "menace.rom",	0x08000, 0x500d37ec, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_menace, MSX_menace, msx_msx)
STD_ROM_FN(MSX_menace)

struct BurnDriver BurnDrvMSX_menace = {
	"msx_menace", NULL, "msx_msx", NULL, "2009",
	"Menace\0", NULL, "The New Image", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_menaceRomInfo, MSX_menaceRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Stray Cat

static struct BurnRomInfo MSX_straycatRomDesc[] = {
	{ "straycat.rom",	0x08000, 0x22a99a44, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_straycat, MSX_straycat, msx_msx)
STD_ROM_FN(MSX_straycat)

struct BurnDriver BurnDrvMSX_straycat = {
	"msx_straycat", NULL, "msx_msx", NULL, "2009",
	"Stray Cat\0", NULL, "IMANOK", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_straycatRomInfo, MSX_straycatRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// British Bob

static struct BurnRomInfo MSX_britishbobRomDesc[] = {
	{ "rlv903_british_bob.rom",	0x04000, 0x8ab004f9, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_britishbob, MSX_britishbob, msx_msx)
STD_ROM_FN(MSX_britishbob)

struct BurnDriver BurnDrvMSX_britishbob = {
	"msx_britishbob", NULL, "msx_msx", NULL, "2009",
	"British Bob\0", NULL, "RELEVO", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_britishbobRomInfo, MSX_britishbobRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Cow Abductors

static struct BurnRomInfo MSX_cowabductRomDesc[] = {
	{ "cow.rom",	0x04000, 0x790623de, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_cowabduct, MSX_cowabduct, msx_msx)
STD_ROM_FN(MSX_cowabduct)

struct BurnDriver BurnDrvMSX_cowabduct = {
	"msx_cow", NULL, "msx_msx", NULL, "2009",
	"Cow Abductors\0", NULL, "Paxanga Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_cowabductRomInfo, MSX_cowabductRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Deep Dungeon

static struct BurnRomInfo MSX_deepdunRomDesc[] = {
	{ "DDUNGEON.ROM",	0x08000, 0x012e13d3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_deepdun, MSX_deepdun, msx_msx)
STD_ROM_FN(MSX_deepdun)

struct BurnDriver BurnDrvMSX_deepdun = {
	"msx_deepdun", NULL, "msx_msx", NULL, "2008",
	"Deep Dungeon\0", NULL, "Trilobyte", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_deepdunRomInfo, MSX_deepdunRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Tomb of Genghis Khan

static struct BurnRomInfo MSX_togkRomDesc[] = {
	{ "togk.rom",	0x08000, 0x903d4e51, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_togk, MSX_togk, msx_msx)
STD_ROM_FN(MSX_togk)

struct BurnDriver BurnDrvMSX_togk = {
	"msx_togk", NULL, "msx_msx", NULL, "2008",
	"Tomb of Genghis Khan\0", NULL, "impulse9", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_togkRomInfo, MSX_togkRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Caos Begins

static struct BurnRomInfo MSX_caosRomDesc[] = {
	{ "caos.rom",	0x08000, 0x44421f97, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_caos, MSX_caos, msx_msx)
STD_ROM_FN(MSX_caos)

struct BurnDriver BurnDrvMSX_caos = {
	"msx_caos", NULL, "msx_msx", NULL, "2007",
	"Caos Begins\0", NULL, "Hikaru Games", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_caosRomInfo, MSX_caosRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Txupinazo!

static struct BurnRomInfo MSX_txupiRomDesc[] = {
	{ "txupi.rom",	0x08000, 0xe1b66d80, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_txupi, MSX_txupi, msx_msx)
STD_ROM_FN(MSX_txupi)

struct BurnDriver BurnDrvMSX_txupi = {
	"msx_txupi", NULL, "msx_msx", NULL, "2007",
	"Txupinazo!\0", NULL, "IMANOK", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_txupiRomInfo, MSX_txupiRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Penguin Mind

static struct BurnRomInfo MSX_pengmindRomDesc[] = {
	{ "PENGMIND.ROM",	0x10000, 0xd2de6ec4, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_pengmind, MSX_pengmind, msx_msx)
STD_ROM_FN(MSX_pengmind)

struct BurnDriver BurnDrvMSX_pengmind = {
	"msx_pengmind", NULL, "msx_msx", NULL, "2007",
	"Penguin Mind\0", NULL, "MSX Cafe", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_pengmindRomInfo, MSX_pengmindRomName, NULL, NULL, MSXInputInfo, MSXJoyCursorDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Monster Hunter

static struct BurnRomInfo MSX_monsterhunterRomDesc[] = {
	{ "NLKMSX001ES_MonsterHunter.rom",	0x20000, 0xb876a92f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_monsterhunter, MSX_monsterhunter, msx_msx)
STD_ROM_FN(MSX_monsterhunter)

struct BurnDriver BurnDrvMSX_monsterhunter = {
	"msx_monsterhunter", NULL, "msx_msx", NULL, "2006",
	"Monster Hunter\0", NULL, "Nerlaska Studio", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_monsterhunterRomInfo, MSX_monsterhunterRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Malaika Prehistoric Quest

static struct BurnRomInfo MSX_malaikapq13RomDesc[] = {
	{ "rk711r3.rom",	0x08000, 0x3ddf42ac, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_malaikapq13, MSX_malaikapq13, msx_msx)
STD_ROM_FN(MSX_malaikapq13)

struct BurnDriver BurnDrvMSX_malaikapq13 = {
	"msx_malaikapq13", NULL, "msx_msx", NULL, "2006",
	"Malaika Prehistoric Quest (v1.3)\0", NULL, "Karoshi Corporation", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_malaikapq13RomInfo, MSX_malaikapq13RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Penguin Cafe

static struct BurnRomInfo MSX_penguincafeRomDesc[] = {
	{ "penguincafe.rom",	0x08000, 0xba41efb1, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_penguincafe, MSX_penguincafe, msx_msx)
STD_ROM_FN(MSX_penguincafe)

struct BurnDriver BurnDrvMSX_penguincafe = {
	"msx_penguincafe", NULL, "msx_msx", NULL, "2006",
	"Penguin Cafe\0", NULL, "MSX Cafe", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_penguincafeRomInfo, MSX_penguincafeRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Grid Wars

static struct BurnRomInfo MSX_gridwarsRomDesc[] = {
	{ "GRIDWARS.ROM",	0x08000, 0x03b56d90, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_gridwars, MSX_gridwars, msx_msx)
STD_ROM_FN(MSX_gridwars)

struct BurnDriver BurnDrvMSX_gridwars = {
	"msx_gridwars", NULL, "msx_msx", NULL, "2006",
	"Grid Wars\0", NULL, "Emma Six", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_gridwarsRomInfo, MSX_gridwarsRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Night City

static struct BurnRomInfo MSX_nightcityRomDesc[] = {
	{ "EN-NIGHT.ROM",	0x10000, 0x7990a7f8, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_nightcity, MSX_nightcity, msx_msx)
STD_ROM_FN(MSX_nightcity)

struct BurnDriver BurnDrvMSX_nightcity = {
	"msx_nightcity", NULL, "msx_msx", NULL, "2006",
	"Night City\0", NULL, "German Gomez Herrera", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_nightcityRomInfo, MSX_nightcityRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// The Cure

static struct BurnRomInfo MSX_thecureRomDesc[] = {
	{ "thecure.rom",	0x0c000, 0x7f855276, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_thecure, MSX_thecure, msx_msx)
STD_ROM_FN(MSX_thecure)

struct BurnDriver BurnDrvMSX_thecure = {
	"msx_thecure", NULL, "msx_msx", NULL, "2005",
	"The Cure\0", NULL, "XL2S Entertainment", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_thecureRomInfo, MSX_thecureRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Caverns of Titan

static struct BurnRomInfo MSX_cavernsRomDesc[] = {
	{ "caverns.rom",	0x04000, 0x9e02db6c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_caverns, MSX_caverns, msx_msx)
STD_ROM_FN(MSX_caverns)

struct BurnDriver BurnDrvMSX_caverns = {
	"msx_caverns", NULL, "msx_msx", NULL, "2005",
	"Caverns of Titan\0", NULL, "JLTURSAN", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_cavernsRomInfo, MSX_cavernsRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Parachuteless Joe

static struct BurnRomInfo MSX_pjoeRomDesc[] = {
	{ "pjoe.rom",	0x08000, 0xbff072fe, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_pjoe, MSX_pjoe, msx_msx)
STD_ROM_FN(MSX_pjoe)

struct BurnDriver BurnDrvMSX_pjoe = {
	"msx_pjoe", NULL, "msx_msx", NULL, "2005",
	"Parachuteless Joe\0", NULL, "Paxanga Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_pjoeRomInfo, MSX_pjoeRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Crazy Buggy

static struct BurnRomInfo MSX_crazyRomDesc[] = {
	{ "crazy.rom",	0x04000, 0xfea00d3b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_crazy, MSX_crazy, msx_msx)
STD_ROM_FN(MSX_crazy)

struct BurnDriver BurnDrvMSX_crazy = {
	"msx_crazy", NULL, "msx_msx", NULL, "2005",
	"Crazy Buggy\0", NULL, "Crappysoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_crazyRomInfo, MSX_crazyRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Sink King

static struct BurnRomInfo MSX_sinkkingRomDesc[] = {
	{ "sinkking.rom",	0x02000, 0xdcd241fa, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_sinkking, MSX_sinkking, msx_msx)
STD_ROM_FN(MSX_sinkking)

struct BurnDriver BurnDrvMSX_sinkking = {
	"msx_sinkking", NULL, "msx_msx", NULL, "2005",
	"Sink King\0", NULL, "Guzuta Raster Leisure", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_sinkkingRomInfo, MSX_sinkkingRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Seleniak

static struct BurnRomInfo MSX_seleniakRomDesc[] = {
	{ "seleniak.rom",	0x02000, 0x6e8bb5fa, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_seleniak, MSX_seleniak, msx_msx)
STD_ROM_FN(MSX_seleniak)

struct BurnDriver BurnDrvMSX_seleniak = {
	"msx_seleniak", NULL, "msx_msx", NULL, "2005",
	"Seleniak\0", NULL, "Guzuta Raster Leisure", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_seleniakRomInfo, MSX_seleniakRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// T-Virus

static struct BurnRomInfo MSX_tvirusRomDesc[] = {
	{ "tvirus.rom",	0x02000, 0x72b26927, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_tvirus, MSX_tvirus, msx_msx)
STD_ROM_FN(MSX_tvirus)

struct BurnDriver BurnDrvMSX_tvirus = {
	"msx_tvirus", NULL, "msx_msx", NULL, "2005",
	"T-Virus\0", NULL, "Dioniso (Alfonso D.C.)", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_tvirusRomInfo, MSX_tvirusRomName, NULL, NULL, MSXInputInfo, MSXJoyCursorDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Factory Infection

static struct BurnRomInfo MSX_factoryRomDesc[] = {
	{ "factory.rom",	0x02000, 0x43f31061, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_factory, MSX_factory, msx_msx)
STD_ROM_FN(MSX_factory)

struct BurnDriver BurnDrvMSX_factory = {
	"msx_factory", NULL, "msx_msx", NULL, "2005",
	"Factory Infection\0", NULL, "Karoshi Corporation", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_factoryRomInfo, MSX_factoryRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Ninja Savior

static struct BurnRomInfo MSX_ninjasavRomDesc[] = {
	{ "RLV921_Ninja_Savior.rom",	0x04000, 0xc059ffba, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_ninjasav, MSX_ninjasav, msx_msx)
STD_ROM_FN(MSX_ninjasav)

struct BurnDriver BurnDrvMSX_ninjasav = {
	"msx_ninjasav", NULL, "msx_msx", NULL, "2015",
	"Ninja Savior\0", NULL, "RELEVO", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_ninjasavRomInfo, MSX_ninjasavRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Mr. Cracksman

static struct BurnRomInfo MSX_mrcrackRomDesc[] = {
	{ "RLV917_MR_CRACKSMAN.rom",	0x02000, 0xe8ded848, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mrcrack, MSX_mrcrack, msx_msx)
STD_ROM_FN(MSX_mrcrack)

struct BurnDriver BurnDrvMSX_mrcrack = {
	"msx_mrcrack", NULL, "msx_msx", NULL, "2013",
	"Mr. Cracksman\0", NULL, "RELEVO", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_mrcrackRomInfo, MSX_mrcrackRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// La Corona Encantada

static struct BurnRomInfo MSX_lacoronaRomDesc[] = {
	{ "RLV901_La_Corona_Encantada.rom",	0x0c000, 0x71e4252e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_lacorona, MSX_lacorona, msx_msx)
STD_ROM_FN(MSX_lacorona)

struct BurnDriver BurnDrvMSX_lacorona = {
	"msx_lacorona", NULL, "msx_msx", NULL, "2009",
	"La Corona Encantada\0", NULL, "Karoshi", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_lacoronaRomInfo, MSX_lacoronaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Abu Simbel Profanation

static struct BurnRomInfo MSX_abusimbelRomDesc[] = {
	{ "abusimbel.rom",	0x20000, 0x603a0c0d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_abusimbel, MSX_abusimbel, msx_msx)
STD_ROM_FN(MSX_abusimbel)

struct BurnDriver BurnDrvMSX_abusimbel = {
	"msx_abusimbel", NULL, "msx_msx", NULL, "1986",
	"Abu Simbel Profanation\0", NULL, "Dinamic", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_abusimbelRomInfo, MSX_abusimbelRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// 180 Degrees

static struct BurnRomInfo MSX_180RomDesc[] = {
	{ "180.rom",	0x20000, 0x1369ca55, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_180, MSX_180, msx_msx)
STD_ROM_FN(MSX_180)

struct BurnDriver BurnDrvMSX_180 = {
	"msx_180", NULL, "msx_msx", NULL, "1986",
	"180 Degrees\0", NULL, "MAD", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_180RomInfo, MSX_180RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Auf Wiedersehen Monty v1.7 special rom

static struct BurnRomInfo MSX_awmontyRomDesc[] = {
	{ "MONTY.ROM",	0x10000, 0x1306ccca, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_awmonty, MSX_awmonty, msx_msx)
STD_ROM_FN(MSX_awmonty)

struct BurnDriver BurnDrvMSX_awmonty = {
	"msx_awmonty", NULL, "msx_msx", NULL, "1987",
	"Auf Wiedersehen Monty (v1.7 special s/l rom)\0", NULL, "Gremlin Graphics", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_awmontyRomInfo, MSX_awmontyRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Batman v1.0 special rom

static struct BurnRomInfo MSX_batman10RomDesc[] = {
	{ "BATMAN.ROM",	0x10000, 0x21e671f0, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_batman10, MSX_batman10, msx_msx)
STD_ROM_FN(MSX_batman10)

struct BurnDriver BurnDrvMSX_batman10 = {
	"msx_batman10", "msx_batman", "msx_msx", NULL, "1987",
	"Batman (v1.0 special s/l rom)\0", NULL, "Ocean", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_batman10RomInfo, MSX_batman10RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Mutant Monty

static struct BurnRomInfo MSX_mumontyRomDesc[] = {
	{ "mumonty.rom",	0x20000, 0x308b71ae, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mumonty, MSX_mumonty, msx_msx)
STD_ROM_FN(MSX_mumonty)

struct BurnDriver BurnDrvMSX_mumonty = {
	"msx_mumonty", NULL, "msx_msx", NULL, "1984",
	"Mutant Monty\0", NULL, "artic", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII16, GBF_MISC, 0,
	MSXGetZipName, MSX_mumontyRomInfo, MSX_mumontyRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Dino Sourcers

static struct BurnRomInfo MSX_dinosourRomDesc[] = {
	{ "Dino Sourcers.rom",	0x04000, 0x83d44b03, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_dinosour, MSX_dinosour, msx_msx)
STD_ROM_FN(MSX_dinosour)

struct BurnDriver BurnDrvMSX_dinosour = {
	"msx_dinosour", NULL, "msx_msx", NULL, "1988",
	"Dino Sourcers\0", NULL, "JALECO", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_dinosourRomInfo, MSX_dinosourRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Temptations

static struct BurnRomInfo MSX_temptationsRomDesc[] = {
	{ "temptati.rom",	0x20000, 0x2d46f211, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_temptations, MSX_temptations, msx_msx)
STD_ROM_FN(MSX_temptations)

struct BurnDriver BurnDrvMSX_temptations = {
	"msx_temptations", NULL, "msx_msx", NULL, "1988",
	"Temptations\0", NULL, "Topo Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_temptationsRomInfo, MSX_temptationsRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Griel's Quest for the Sangraal

static struct BurnRomInfo MSX_grielRomDesc[] = {
	{ "griel.rom",	0x08000, 0x6cebb29b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_griel, MSX_griel, msx_msx)
STD_ROM_FN(MSX_griel)

struct BurnDriver BurnDrvMSX_griel = {
	"msx_griel", NULL, "msx_msx", NULL, "2005",
	"Griel's Quest for the Sangraal\0", NULL, "Karoshi Corporation", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_grielRomInfo, MSX_grielRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Griel's Quest for the Sangraal

static struct BurnRomInfo MSX_grielexRomDesc[] = {
	{ "grielex.rom",	0x08000, 0xc9427fe2, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_grielex, MSX_grielex, msx_msx)
STD_ROM_FN(MSX_grielex)

struct BurnDriver BurnDrvMSX_grielex = {
	"msx_grielex", "msx_griel", "msx_msx", NULL, "2005",
	"Griel's Quest for the Sangraal (Extended Version)\0", NULL, "Karoshi Corporation", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_grielexRomInfo, MSX_grielexRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Saimazoom

static struct BurnRomInfo MSX_smzoomRomDesc[] = {
	{ "rk708en.rom",	0x08000, 0xe76bc18e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_smzoom, MSX_smzoom, msx_msx)
STD_ROM_FN(MSX_smzoom)

struct BurnDriver BurnDrvMSX_smzoom = {
	"msx_smzoom", NULL, "msx_msx", NULL, "2005",
	"Saimazoom\0", NULL, "Karoshi Corporation", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_smzoomRomInfo, MSX_smzoomRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Magical Stones

static struct BurnRomInfo MSX_magicalRomDesc[] = {
	{ "ms16k.rom",	0x04000, 0x0ec3f7a3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_magical, MSX_magical, msx_msx)
STD_ROM_FN(MSX_magical)

struct BurnDriver BurnDrvMSX_magical = {
	"msx_magical", NULL, "msx_msx", NULL, "2005",
	"Magical Stones\0", NULL, "Dioniso (Alfonso D.C.)", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_magicalRomInfo, MSX_magicalRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Wing Warriors

static struct BurnRomInfo MSX_wingwarrRomDesc[] = {
	{ "WingWarriors.rom",	0x0c000, 0x207d0e62, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_wingwarr, MSX_wingwarr, msx_msx)
STD_ROM_FN(MSX_wingwarr)

struct BurnDriver BurnDrvMSX_wingwarr = {
	"msx_wingwarr", NULL, "msx_msx", NULL, "2015",
	"Wing Warriors\0", NULL, "Kitmaker", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_wingwarrRomInfo, MSX_wingwarrRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// BitLogic

static struct BurnRomInfo MSX_bitlogicRomDesc[] = {
	{ "BITLOGIC_v_100.ROM",	0x0c000, 0xfbcd1942, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_bitlogic, MSX_bitlogic, msx_msx)
STD_ROM_FN(MSX_bitlogic)

struct BurnDriver BurnDrvMSX_bitlogic = {
	"msx_bitlogic", NULL, "msx_msx", NULL, "2015",
	"BitLogic\0", NULL, "OxiAB Studio", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_bitlogicRomInfo, MSX_bitlogicRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Uridium

static struct BurnRomInfo MSX_uridiumRomDesc[] = {
	{ "URDIUM48.rom",	0x0c000, 0xc62d92da, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_uridium, MSX_uridium, msx_msx)
STD_ROM_FN(MSX_uridium)

struct BurnDriver BurnDrvMSX_uridium = {
	"msx_uridium", NULL, "msx_msx", NULL, "2014",
	"Uridium\0", NULL, "Trilobyte", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_uridiumRomInfo, MSX_uridiumRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Pretty Kingdom

static struct BurnRomInfo MSX_prettykngdmRomDesc[] = {
	{ "NLKMSX006EN_PRETTYKINGDOM.rom",	0x08000, 0xcb2d148c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_prettykngdm, MSX_prettykngdm, msx_msx)
STD_ROM_FN(MSX_prettykngdm)

struct BurnDriver BurnDrvMSX_prettykngdm = {
	"msx_prettykngdm", NULL, "msx_msx", NULL, "2014",
	"Pretty Kingdom\0", NULL, "Nerlaska Studio", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_prettykngdmRomInfo, MSX_prettykngdmRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Gorgeous Gemma in Escape from the Space Disposal Planet

static struct BurnRomInfo MSX_ggeftsdpRomDesc[] = {
	{ "ggeftsdp.rom",	0x08000, 0x818ccade, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_ggeftsdp, MSX_ggeftsdp, msx_msx)
STD_ROM_FN(MSX_ggeftsdp)

struct BurnDriver BurnDrvMSX_ggeftsdp = {
	"msx_ggeftsdp", NULL, "msx_msx", NULL, "2013",
	"Gorgeous Gemma in Escape from the Space Disposal Planet\0", NULL, "Impulse9", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_ggeftsdpRomInfo, MSX_ggeftsdpRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Zambeze

static struct BurnRomInfo MSX_zambezeRomDesc[] = {
	{ "zambezefacil.rom",	0x08000, 0x91d07554, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_zambeze, MSX_zambeze, msx_msx)
STD_ROM_FN(MSX_zambeze)

struct BurnDriver BurnDrvMSX_zambeze = {
	"msx_zambeze", NULL, "msx_msx", NULL, "2006",
	"Zambeze\0", NULL, "Degora", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_zambezeRomInfo, MSX_zambezeRomName, NULL, NULL, MSXInputInfo, MSXJoyCursorDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Nayade Resistance

static struct BurnRomInfo MSX_nayadeRomDesc[] = {
	{ "nayade.rom",	0x08000, 0x3587844a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_nayade, MSX_nayade, msx_msx)
STD_ROM_FN(MSX_nayade)

struct BurnDriver BurnDrvMSX_nayade = {
	"msx_nayade", NULL, "msx_msx", NULL, "2013",
	"Nayade Resistance\0", "Control w/ Arrow-keys, M to select weapon", "Pentacour", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_nayadeRomInfo, MSX_nayadeRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Princess Quest

static struct BurnRomInfo MSX_princessquestRomDesc[] = {
	{ "princess_quest_msx.rom",	0x10000, 0xaa31ac36, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_princessquest, MSX_princessquest, msx_msx)
STD_ROM_FN(MSX_princessquest)

struct BurnDriver BurnDrvMSX_princessquest = {
	"msx_princessquest", NULL, "msx_msx", NULL, "2012",
	"Princess Quest\0", NULL, "Oscar Toledo G.", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_princessquestRomInfo, MSX_princessquestRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Baby Dinosaur Dooly

static struct BurnRomInfo MSX_doolyRomDesc[] = {
	{ "dooly.rom",	0x08000, 0x71a1b1ec, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_dooly, MSX_dooly, msx_msx)
STD_ROM_FN(MSX_dooly)

struct BurnDriver BurnDrvMSX_dooly = {
	"msx_dooly", NULL, "msx_msx", NULL, "1991",
	"Baby Dinosaur Dooly\0", NULL, "Daou", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_DOOLY, GBF_MISC, 0,
	MSXGetZipName, MSX_doolyRomInfo, MSX_doolyRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Xenon (Spa)

static struct BurnRomInfo MSX_xenonRomDesc[] = {
	{ "xenon (1988)(dro soft)(es)[run'cas-'].cas",	0x12332, 0x4ad26b8f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_xenon, MSX_xenon, msx_msx)
STD_ROM_FN(MSX_xenon)

struct BurnDriver BurnDrvMSX_xenon = {
	"msx_xenon", NULL, "msx_msx", NULL, "1988",
	"Xenon (Spa)\0", NULL, "Dro Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_xenonRomInfo, MSX_xenonRomName, NULL, NULL, MSXInputInfo, MSXEuropeJoyport2DIPInfo,
	CasRunSwapButton2DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Genesis: dawn of a new day

static struct BurnRomInfo MSX_genesisRomDesc[] = {
	{ "genesis.rom",	0x20000, 0x391e6f9a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_genesis, MSX_genesis, msx_msx)
STD_ROM_FN(MSX_genesis)

struct BurnDriver BurnDrvMSX_genesis = {
	"msx_genesis", NULL, "msx_msx", NULL, "2012",
	"Genesis: dawn of a new day\0", NULL, "Retroworks", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_genesisRomInfo, MSX_genesisRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Jinj 2 - Belmonte's Revenge

static struct BurnRomInfo MSX_jinj2RomDesc[] = {
	{ "jinj2-v2.rom",	0x08000, 0xd43cf107, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_jinj2, MSX_jinj2, msx_msx)
STD_ROM_FN(MSX_jinj2)

struct BurnDriver BurnDrvMSX_jinj2 = {
	"msx_jinj2", NULL, "msx_msx", NULL, "2010",
	"Jinj 2 - Belmonte's Revenge\0", NULL, "Retroworks", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_jinj2RomInfo, MSX_jinj2RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Trailblazer (Euro)

static struct BurnRomInfo MSX_trailblzRomDesc[] = {
	{ "trailblazer (1986)(gremlin graphics software)(gb)[run'cas-'].cas",	0x095ff, 0x4ace4920, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_trailblz, MSX_trailblz, msx_msx)
STD_ROM_FN(MSX_trailblz)

struct BurnDriver BurnDrvMSX_trailblz = {
	"msx_trailblz", NULL, "msx_msx", NULL, "1986",
	"Trailblazer (Euro)\0", NULL, "Gremlin Graphics", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_trailblzRomInfo, MSX_trailblzRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Gauntlet (Euro)

static struct BurnRomInfo MSX_gauntletRomDesc[] = {
	{ "gauntlet (1986)(u.s. gold)(gb)(side a)[run'cas-'].cas",	0x0e2f5, 0x7e4c19ec, BRF_PRG | BRF_ESS },
	{ "gauntlet (1986)(u.s. gold)(gb)(side b)[run'cas-'].cas",	0x173b5, 0xce0a53a4, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_gauntlet, MSX_gauntlet, msx_msx)
STD_ROM_FN(MSX_gauntlet)

struct BurnDriver BurnDrvMSX_gauntlet = {
	"msx_gauntlet", NULL, "msx_msx", NULL, "1986",
	"Gauntlet (Euro)\0", NULL, "U.S. Gold", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_gauntletRomInfo, MSX_gauntletRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Navy Moves (Euro)

static struct BurnRomInfo MSX_navymoveRomDesc[] = {
	{ "Navy Moves (1988)(Dinamic Software)(ES)(Side A)[RUN'CAS-'].cas",	0x0c68f, 0xd420952c, BRF_PRG | BRF_ESS },
	{ "Navy Moves (1988)(Dinamic Software)(ES)(Side B)[RUN'CAS-'].cas",	0x0b4f5, 0x045a7642, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_navymove, MSX_navymove, msx_msx)
STD_ROM_FN(MSX_navymove)

struct BurnDriver BurnDrvMSX_navymove = {
	"msx_navymove", NULL, "msx_msx", NULL, "1988",
	"Navy Moves (Euro)\0", "Select Keyboard mode, and use Joystick!", "Dinamic Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_navymoveRomInfo, MSX_navymoveRomName, NULL, NULL, MSXInputInfo, MSXJoyCursorDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Army Moves (Euro)

static struct BurnRomInfo MSX_armymoveRomDesc[] = {
	{ "army moves (1987)(dinamic software)(gb)(side a)[run'cas-'].cas",	0x0c222, 0xd33316dd, BRF_PRG | BRF_ESS },
	{ "army moves (1987)(dinamic software)(gb)(side b)[run'cas-'].cas",	0x0c222, 0x8e5fd272, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_armymove, MSX_armymove, msx_msx)
STD_ROM_FN(MSX_armymove)

struct BurnDriver BurnDrvMSX_armymove = {
	"msx_armymove", NULL, "msx_msx", NULL, "1987",
	"Army Moves (Euro)\0", "Select Keyboard mode, and use Joystick!", "Dinamic Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_armymoveRomInfo, MSX_armymoveRomName, NULL, NULL, MSXInputInfo, MSXJoyCursorDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Toobin'

static struct BurnRomInfo MSX_toobinRomDesc[] = {
	{ "Toobin' (1989)(Domark)(GB).rom",	0x10000, 0x704ec575, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_toobin, MSX_toobin, msx_msx)
STD_ROM_FN(MSX_toobin)

struct BurnDriver BurnDrvMSX_toobin = {
	"msx_toobin", NULL, "msx_msx", NULL, "1989",
	"Toobin'\0", NULL, "Domark", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII16, GBF_MISC, 0,
	MSXGetZipName, MSX_toobinRomInfo, MSX_toobinRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Tension

static struct BurnRomInfo MSX_tensionRomDesc[] = {
	{ "Tension (1988)(System 4)(ES).rom",	0x08000, 0x3d78462c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_tension, MSX_tension, msx_msx)
STD_ROM_FN(MSX_tension)

struct BurnDriver BurnDrvMSX_tension = {
	"msx_tension", NULL, "msx_msx", NULL, "1988",
	"Tension\0", NULL, "System 4", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_tensionRomInfo, MSX_tensionRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Payload

static struct BurnRomInfo MSX_payloadRomDesc[] = {
	{ "Payload (1985)(Sony)(JP).rom",	0x08000, 0x165eae6d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_payload, MSX_payload, msx_msx)
STD_ROM_FN(MSX_payload)

struct BurnDriver BurnDrvMSX_payload = {
	"msx_payload", NULL, "msx_msx", NULL, "1985",
	"Payload\0", NULL, "Sony", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_payloadRomInfo, MSX_payloadRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Namake's Bridgedrome

static struct BurnRomInfo MSX_namakeRomDesc[] = {
	{ "Namake's Bridgedrome (2005)(Buresto Faiya).rom",	0x04000, 0x92aee975, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_namake, MSX_namake, msx_msx)
STD_ROM_FN(MSX_namake)

struct BurnDriver BurnDrvMSX_namake = {
	"msx_namake", NULL, "msx_msx", NULL, "2005",
	"Namake's Bridgedrome\0", "Uses joyport #2", "Buresto Faiya", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_namakeRomInfo, MSX_namakeRomName, NULL, NULL, MSXInputInfo, MSXJoyport2DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Mayhem

static struct BurnRomInfo MSX_mayhemRomDesc[] = {
	{ "Mayhem (1985)(Mr. Micro)(GB)[cr Magicracks].rom",	0x04000, 0xb0b90a4d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mayhem, MSX_mayhem, msx_msx)
STD_ROM_FN(MSX_mayhem)

struct BurnDriver BurnDrvMSX_mayhem = {
	"msx_mayhem", NULL, "msx_msx", NULL, "1985",
	"Mayhem\0", "Uses joyport #2", "Mr. Micro", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_mayhemRomInfo, MSX_mayhemRomName, NULL, NULL, MSXInputInfo, MSXJoyport2DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Kobashi

static struct BurnRomInfo MSX_kobashiRomDesc[] = {
	{ "Kobashi (2004)(Desire in Envy)(NL).rom",	0x02000, 0xd137c518, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_kobashi, MSX_kobashi, msx_msx)
STD_ROM_FN(MSX_kobashi)

struct BurnDriver BurnDrvMSX_kobashi = {
	"msx_kobashi", NULL, "msx_msx", NULL, "2004",
	"Kobashi\0", "Uses joyport #2", "Desire in Envy", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_kobashiRomInfo, MSX_kobashiRomName, NULL, NULL, MSXInputInfo, MSXJoyport2DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Jumping Rabbit

static struct BurnRomInfo MSX_jrabbitRomDesc[] = {
	{ "Jumping Rabbit (1984)(MIA)(JP).rom",	0x02000, 0x15471ab9, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_jrabbit, MSX_jrabbit, msx_msx)
STD_ROM_FN(MSX_jrabbit)

struct BurnDriver BurnDrvMSX_jrabbit = {
	"msx_jrabbit", NULL, "msx_msx", NULL, "1984",
	"Jumping Rabbit\0", NULL, "MIA", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_jrabbitRomInfo, MSX_jrabbitRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Jump Land

static struct BurnRomInfo MSX_jumplandRomDesc[] = {
	{ "Jump Land (1985)(Nippon Columbia - Colpax - Universal).rom",	0x04000, 0x0f8699ba, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_jumpland, MSX_jumpland, msx_msx)
STD_ROM_FN(MSX_jumpland)

struct BurnDriver BurnDrvMSX_jumpland = {
	"msx_jumpland", NULL, "msx_msx", NULL, "1985",
	"Jump Land\0", NULL, "Nippon Columbia - Colpax - Universal", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_jumplandRomInfo, MSX_jumplandRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Happy Fret

static struct BurnRomInfo MSX_happyfretRomDesc[] = {
	{ "Happy Fret (1985)(Micro Cabin)(JP).rom",	0x08000, 0xbe2f3236, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_happyfret, MSX_happyfret, msx_msx)
STD_ROM_FN(MSX_happyfret)

struct BurnDriver BurnDrvMSX_happyfret = {
	"msx_happyfret", NULL, "msx_msx", NULL, "1985",
	"Happy Fret\0", "Uses joyport #2", "Micro Cabin", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_happyfretRomInfo, MSX_happyfretRomName, NULL, NULL, MSXInputInfo, MSXJoyport2DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Gyro Adventure

static struct BurnRomInfo MSX_gyroadvRomDesc[] = {
	{ "Gyro Adventure (1984)(Nippon Columbia - Colpax - Universal)(JP)[Martos].rom",	0x04000, 0x05939074, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_gyroadv, MSX_gyroadv, msx_msx)
STD_ROM_FN(MSX_gyroadv)

struct BurnDriver BurnDrvMSX_gyroadv = {
	"msx_gyroadv", NULL, "msx_msx", NULL, "1984",
	"Gyro Adventure\0", NULL, "Nippon Columbia - Colpax - Universal", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_gyroadvRomInfo, MSX_gyroadvRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Eat Blue!

static struct BurnRomInfo MSX_eatblueRomDesc[] = {
	{ "Eat Blue! 2004 v2 (2004)(Paxanga Soft).rom",	0x02000, 0x1ac005a0, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_eatblue, MSX_eatblue, msx_msx)
STD_ROM_FN(MSX_eatblue)

struct BurnDriver BurnDrvMSX_eatblue = {
	"msx_eatblue", NULL, "msx_msx", NULL, "2004",
	"Eat Blue!\0", NULL, "Paxanga Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_BASIC, GBF_MISC, 0,
	MSXGetZipName, MSX_eatblueRomInfo, MSX_eatblueRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Driller Tanks

static struct BurnRomInfo MSX_drillertnkRomDesc[] = {
	{ "Driller Tanks (1984)(Hudson Soft)(JP).rom",	0x04000, 0xd78da5fc, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_drillertnk, MSX_drillertnk, msx_msx)
STD_ROM_FN(MSX_drillertnk)

struct BurnDriver BurnDrvMSX_drillertnk = {
	"msx_drillertnk", NULL, "msx_msx", NULL, "1984",
	"Driller Tanks\0", NULL, "Hudson Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_drillertnkRomInfo, MSX_drillertnkRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Dip Dip

static struct BurnRomInfo MSX_dipdipRomDesc[] = {
	{ "Dip Dip (1985)(Indescomp)(ES).rom",	0x04000, 0x9a2cc849, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_dipdip, MSX_dipdip, msx_msx)
STD_ROM_FN(MSX_dipdip)

struct BurnDriver BurnDrvMSX_dipdip = {
	"msx_dipdip", NULL, "msx_msx", NULL, "1985",
	"Dip Dip\0", NULL, "Indescomp", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_dipdipRomInfo, MSX_dipdipRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Cannon Fighter

static struct BurnRomInfo MSX_cannonfgtRomDesc[] = {
	{ "Cannon Fighter (1984)(Policy)(JP).rom",	0x04000, 0x9c7fb01e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_cannonfgt, MSX_cannonfgt, msx_msx)
STD_ROM_FN(MSX_cannonfgt)

struct BurnDriver BurnDrvMSX_cannonfgt = {
	"msx_cannonfgt", NULL, "msx_msx", NULL, "1984",
	"Cannon Fighter\0", NULL, "Policy", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_cannonfgtRomInfo, MSX_cannonfgtRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Death Wish 3

static struct BurnRomInfo MSX_deathwish3RomDesc[] = {
	{ "deathwish3.rom",	0x20000, 0x90ae7ee8, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_deathwish3, MSX_deathwish3, msx_msx)
STD_ROM_FN(MSX_deathwish3)

struct BurnDriver BurnDrvMSX_deathwish3 = {
	"msx_deathwish3", NULL, "msx_msx", NULL, "1987",
	"Death Wish 3\0", NULL, "Gremlin", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_deathwish3RomInfo, MSX_deathwish3RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Martianoids

static struct BurnRomInfo MSX_martianoidsRomDesc[] = {
	{ "martianoids.rom",	0x20000, 0xd796235c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_martianoids, MSX_martianoids, msx_msx)
STD_ROM_FN(MSX_martianoids)

struct BurnDriver BurnDrvMSX_martianoids = {
	"msx_martianoids", NULL, "msx_msx", NULL, "1987",
	"Martianoids\0", NULL, "Ultimate Play The Game", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_martianoidsRomInfo, MSX_martianoidsRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Gonzzalezz

static struct BurnRomInfo MSX_gonzzalezzRomDesc[] = {
	{ "gonzzalezz.rom",	0x40000, 0x8765d1fd, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_gonzzalezz, MSX_gonzzalezz, msx_msx)
STD_ROM_FN(MSX_gonzzalezz)

struct BurnDriver BurnDrvMSX_gonzzalezz = {
	"msx_gonzzalezz", NULL, "msx_msx", NULL, "1989",
	"Gonzzalezz \0", NULL, "Opera Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_gonzzalezzRomInfo, MSX_gonzzalezzRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Feud

static struct BurnRomInfo MSX_feudRomDesc[] = {
	{ "feud.rom",	0x20000, 0xda665159, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_feud, MSX_feud, msx_msx)
STD_ROM_FN(MSX_feud)

struct BurnDriver BurnDrvMSX_feud = {
	"msx_feud", NULL, "msx_msx", NULL, "1987",
	"Feud\0", NULL, "Bulldog", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_feudRomInfo, MSX_feudRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Head over Heels

static struct BurnRomInfo MSX_headheelsRomDesc[] = {
	{ "Head over Heels (1987)(Ocean Software)(GB).rom",	0x20000, 0x48b88fbc, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_headheels, MSX_headheels, msx_msx)
STD_ROM_FN(MSX_headheels)

struct BurnDriver BurnDrvMSX_headheels = {
	"msx_headheels", NULL, "msx_msx", NULL, "1987",
	"Head over Heels\0", NULL, "Ocean Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_headheelsRomInfo, MSX_headheelsRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Perspective v1.1 T.Yoshida

static struct BurnRomInfo MSX_perspectiveRomDesc[] = {
	{ "perspective.rom",	0x40000, 0x2c4eead6, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_perspective, MSX_perspective, msx_msx)
STD_ROM_FN(MSX_perspective)

struct BurnDriver BurnDrvMSX_perspective = {
	"msx_perspective", NULL, "msx_msx", NULL, "1987",
	"Perspective (v1.1)\0", NULL, "T.Yoshida", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_perspectiveRomInfo, MSX_perspectiveRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Fruit Panic

static struct BurnRomInfo MSX_fruitpanicRomDesc[] = {
	{ "Fruit Panic (1984)(Pony Canyon)(JP).rom",	0x10000, 0x7429eb0e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_fruitpanic, MSX_fruitpanic, msx_msx)
STD_ROM_FN(MSX_fruitpanic)

struct BurnDriver BurnDrvMSX_fruitpanic = {
	"msx_fruitpanic", NULL, "msx_msx", NULL, "1984",
	"Fruit Panic\0", NULL, "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_fruitpanicRomInfo, MSX_fruitpanicRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Ale Hop!

static struct BurnRomInfo MSX_alehopRomDesc[] = {
	{ "Ale Hop! (1988)(Topo Soft)(ES).rom",	0x20000, 0xc2694922, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_alehop, MSX_alehop, msx_msx)
STD_ROM_FN(MSX_alehop)

struct BurnDriver BurnDrvMSX_alehop = {
	"msx_alehop", NULL, "msx_msx", NULL, "1988",
	"Ale Hop!\0", NULL, "Topo Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_alehopRomInfo, MSX_alehopRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Blackbeard

static struct BurnRomInfo MSX_blackbeardRomDesc[] = {
	{ "Blackbeard (1988)(Topo Soft)(ES).rom",	0x20000, 0x6e566a11, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_blackbeard, MSX_blackbeard, msx_msx)
STD_ROM_FN(MSX_blackbeard)

struct BurnDriver BurnDrvMSX_blackbeard = {
	"msx_blackbeard", NULL, "msx_msx", NULL, "1988",
	"Blackbeard\0", "Uses joyport #2", "Topo Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_blackbeardRomInfo, MSX_blackbeardRomName, NULL, NULL, MSXInputInfo, MSXJoyport2DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Capitan Trueno

static struct BurnRomInfo MSX_ctruenoRomDesc[] = {
	{ "Capitan Trueno (1989)(Dinamic Software)(ES).rom",	0x20000, 0xb1b434be, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_ctrueno, MSX_ctrueno, msx_msx)
STD_ROM_FN(MSX_ctrueno)

struct BurnDriver BurnDrvMSX_ctrueno = {
	"msx_ctrueno", NULL, "msx_msx", NULL, "1989",
	"Capitan Trueno\0", "Uses joyport #2", "Dinamic Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_ctruenoRomInfo, MSX_ctruenoRomName, NULL, NULL, MSXInputInfo, MSXJoyport2DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Colt 36

static struct BurnRomInfo MSX_colt36RomDesc[] = {
	{ "Colt 36 (1987)(Topo Soft)(ES).rom",	0x10000, 0x7ce25b7c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_colt36, MSX_colt36, msx_msx)
STD_ROM_FN(MSX_colt36)

struct BurnDriver BurnDrvMSX_colt36 = {
	"msx_colt36", NULL, "msx_msx", NULL, "1987",
	"Colt 36\0", NULL, "Topo Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_colt36RomInfo, MSX_colt36RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Corsarios

static struct BurnRomInfo MSX_corsariosRomDesc[] = {
	{ "Corsarios (1989)(Opera Soft)(ES).rom",	0x20000, 0x64005d08, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_corsarios, MSX_corsarios, msx_msx)
STD_ROM_FN(MSX_corsarios)

struct BurnDriver BurnDrvMSX_corsarios = {
	"msx_corsarios", NULL, "msx_msx", NULL, "1989",
	"Corsarios\0", "Uses joyport #2", "Opera Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_corsariosRomInfo, MSX_corsariosRomName, NULL, NULL, MSXInputInfo, MSXJoyport2DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Game Over

static struct BurnRomInfo MSX_gameoverRomDesc[] = {
	{ "Game Over (1988)(Dinamic Software)(ES).rom",	0x20000, 0xdfbbdf10, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_gameover, MSX_gameover, msx_msx)
STD_ROM_FN(MSX_gameover)

struct BurnDriver BurnDrvMSX_gameover = {
	"msx_gameover", NULL, "msx_msx", NULL, "1988",
	"Game Over\0", NULL, "Dinamic Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_gameoverRomInfo, MSX_gameoverRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Poder Oscuro, El

static struct BurnRomInfo MSX_poderoRomDesc[] = {
	{ "Poder Oscuro, El (1988)(Zigurat Software)(ES).rom",	0x20000, 0xdc6eef71, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_podero, MSX_podero, msx_msx)
STD_ROM_FN(MSX_podero)

struct BurnDriver BurnDrvMSX_podero = {
	"msx_podero", NULL, "msx_msx", NULL, "1988",
	"Poder Oscuro, El\0", NULL, "Zigurat Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_poderoRomInfo, MSX_poderoRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Sorcery

static struct BurnRomInfo MSX_sorceryRomDesc[] = {
	{ "Sorcery (1985)(Virgin Games)(GB).rom",	0x20000, 0x4300eaa9, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_sorcery, MSX_sorcery, msx_msx)
STD_ROM_FN(MSX_sorcery)

struct BurnDriver BurnDrvMSX_sorcery = {
	"msx_sorcery", NULL, "msx_msx", NULL, "1985",
	"Sorcery\0", NULL, "Virgin Games", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_sorceryRomInfo, MSX_sorceryRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Who Dares Wins II

static struct BurnRomInfo MSX_whodaresRomDesc[] = {
	{ "Who Dares Wins II (1986)(Alligata Software)(GB).rom",	0x20000, 0x1d10443f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_whodares, MSX_whodares, msx_msx)
STD_ROM_FN(MSX_whodares)

struct BurnDriver BurnDrvMSX_whodares = {
	"msx_whodares", NULL, "msx_msx", NULL, "1986",
	"Who Dares Wins II\0", NULL, "Alligata Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_whodaresRomInfo, MSX_whodaresRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Manic Miner

static struct BurnRomInfo MSX_manicminerRomDesc[] = {
	{ "Manic Miner (1984)(Software Projects)(GB).rom",	0x10000, 0x4b5cf16a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_manicminer, MSX_manicminer, msx_msx)
STD_ROM_FN(MSX_manicminer)

struct BurnDriver BurnDrvMSX_manicminer = {
	"msx_manicminer", NULL, "msx_msx", NULL, "1984",
	"Manic Miner\0", NULL, "Software Projects", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_manicminerRomInfo, MSX_manicminerRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Last Mission

static struct BurnRomInfo MSX_lastmissionRomDesc[] = {
	{ "Last Mission, The (1987)(Opera Soft)(ES).rom",	0x20000, 0x494b9bc0, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_lastmission, MSX_lastmission, msx_msx)
STD_ROM_FN(MSX_lastmission)

struct BurnDriver BurnDrvMSX_lastmission = {
	"msx_lastmission", NULL, "msx_msx", NULL, "1987",
	"Last Mission\0", NULL, "Opera Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_lastmissionRomInfo, MSX_lastmissionRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Ballblazer

static struct BurnRomInfo MSX_ballblzrRomDesc[] = {
	{ "Ballblazer (1988)(Pony Canyon)(JP).rom",	0x20000, 0xbd307546, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_ballblzr, MSX_ballblzr, msx_msx)
STD_ROM_FN(MSX_ballblzr)

struct BurnDriver BurnDrvMSX_ballblzr = {
	"msx_ballblzr", NULL, "msx_msx", NULL, "1988",
	"Ballblazer\0", "Keyboard control only.", "Pony Canyon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_ballblzrRomInfo, MSX_ballblzrRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Blow Up

static struct BurnRomInfo MSX_blowupRomDesc[] = {
	{ "Blow Up (1988)(Eurosoft)(NL).rom",	0x20000, 0x72ec1918, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_blowup, MSX_blowup, msx_msx)
STD_ROM_FN(MSX_blowup)

struct BurnDriver BurnDrvMSX_blowup = {
	"msx_blowup", NULL, "msx_msx", NULL, "1988",
	"Blow Up\0", NULL, "Eurosoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_blowupRomInfo, MSX_blowupRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Buck Rogers - Planet of Zoom

static struct BurnRomInfo MSX_buckrogRomDesc[] = {
	{ "Buck Rogers - Planet of Zoom (1983)(Sega)(JP).rom",	0x10000, 0xdaf045ca, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_buckrog, MSX_buckrog, msx_msx)
STD_ROM_FN(MSX_buckrog)

struct BurnDriver BurnDrvMSX_buckrog = {
	"msx_buckrog", NULL, "msx_msx", NULL, "1983",
	"Buck Rogers - Planet of Zoom\0", NULL, "Sega", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_buckrogRomInfo, MSX_buckrogRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Camelot Warriors

static struct BurnRomInfo MSX_camelotwRomDesc[] = {
	{ "Camelot Warriors (1986)(Dinamic Software)(ES).rom",	0x20000, 0x89a53f23, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_camelotw, MSX_camelotw, msx_msx)
STD_ROM_FN(MSX_camelotw)

struct BurnDriver BurnDrvMSX_camelotw = {
	"msx_camelotw", NULL, "msx_msx", NULL, "1986",
	"Camelot Warriors\0", NULL, "Dinamic Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_camelotwRomInfo, MSX_camelotwRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Congo Bongo

static struct BurnRomInfo MSX_congobongoRomDesc[] = {
	{ "Congo Bongo (1983)(Sega)(JP).rom",	0x10000, 0xcb054111, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_congobongo, MSX_congobongo, msx_msx)
STD_ROM_FN(MSX_congobongo)

struct BurnDriver BurnDrvMSX_congobongo = {
	"msx_congobongo", NULL, "msx_msx", NULL, "1983",
	"Congo Bongo\0", NULL, "Sega", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_congobongoRomInfo, MSX_congobongoRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Demon Crystal, The

static struct BurnRomInfo MSX_demoncrystlRomDesc[] = {
	{ "Demon Crystal, The (1986)(Radio Wave Newspaper Publisher)(JP).rom",	0x10000, 0x3ebf588d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_demoncrystl, MSX_demoncrystl, msx_msx)
STD_ROM_FN(MSX_demoncrystl)

struct BurnDriver BurnDrvMSX_demoncrystl = {
	"msx_demoncrystl", NULL, "msx_msx", NULL, "1986",
	"Demon Crystal, The\0", NULL, "Radio Wave Newspaper Publisher", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_demoncrystlRomInfo, MSX_demoncrystlRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Demonia

static struct BurnRomInfo MSX_demoniaRomDesc[] = {
	{ "Demonia (1986)(Microids)(GB)(fr).rom",	0x20000, 0x8f1d68a0, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_demonia, MSX_demonia, msx_msx)
STD_ROM_FN(MSX_demonia)

struct BurnDriver BurnDrvMSX_demonia = {
	"msx_demonia", NULL, "msx_msx", NULL, "1986",
	"Demonia\0", NULL, "Microids", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_demoniaRomInfo, MSX_demoniaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Jack the Nipper

static struct BurnRomInfo MSX_nipperRomDesc[] = {
	{ "Jack the Nipper (1986)(Gremlin Graphics Software)(GB).rom",	0x20000, 0x666725ec, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_nipper, MSX_nipper, msx_msx)
STD_ROM_FN(MSX_nipper)

struct BurnDriver BurnDrvMSX_nipper = {
	"msx_nipper", NULL, "msx_msx", NULL, "1986",
	"Jack the Nipper\0", NULL, "Gremlin Graphics Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_nipperRomInfo, MSX_nipperRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Jack the Nipper II - In Coconut Capers

static struct BurnRomInfo MSX_nipper2RomDesc[] = {
	{ "Jack the Nipper II - In Coconut Capers (1987)(Gremlin Graphics Software)(GB).rom",	0x20000, 0x6b0fd271, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_nipper2, MSX_nipper2, msx_msx)
STD_ROM_FN(MSX_nipper2)

struct BurnDriver BurnDrvMSX_nipper2 = {
	"msx_nipper2", NULL, "msx_msx", NULL, "1987",
	"Jack the Nipper II - In Coconut Capers\0", NULL, "Gremlin Graphics Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_nipper2RomInfo, MSX_nipper2RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Life in the Fast Lane

static struct BurnRomInfo MSX_lifefastlnRomDesc[] = {
	{ "Life in the Fast Lane (1987)(Methodic Solutions)(NL).rom",	0x20000, 0x37a8713d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_lifefastln, MSX_lifefastln, msx_msx)
STD_ROM_FN(MSX_lifefastln)

struct BurnDriver BurnDrvMSX_lifefastln = {
	"msx_lifefastln", NULL, "msx_msx", NULL, "1987",
	"Life in the Fast Lane\0", NULL, "Methodic Solutions", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_lifefastlnRomInfo, MSX_lifefastlnRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Livingstone Supongo

static struct BurnRomInfo MSX_livingstoneRomDesc[] = {
	{ "Livingstone Supongo (1986)(Opera Soft)(ES).rom",	0x20000, 0xcea7e9bc, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_livingstone, MSX_livingstone, msx_msx)
STD_ROM_FN(MSX_livingstone)

struct BurnDriver BurnDrvMSX_livingstone = {
	"msx_livingstone", NULL, "msx_msx", NULL, "1986",
	"Livingstone Supongo\0", NULL, "Opera Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_livingstoneRomInfo, MSX_livingstoneRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Livingstone Supongo II

static struct BurnRomInfo MSX_livingstone2RomDesc[] = {
	{ "Livingstone Supongo II (1989)(Opera Soft)(ES).rom",	0x20000, 0x5410c622, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_livingstone2, MSX_livingstone2, msx_msx)
STD_ROM_FN(MSX_livingstone2)

struct BurnDriver BurnDrvMSX_livingstone2 = {
	"msx_livingstone2", NULL, "msx_msx", NULL, "1989",
	"Livingstone Supongo II\0", NULL, "Opera Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_livingstone2RomInfo, MSX_livingstone2RomName, NULL, NULL, MSXInputInfo, MSXJoyport2DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Spooks & Ladders

static struct BurnRomInfo MSX_spookslRomDesc[] = {
	{ "Spooks & Ladders (1985)(Kuma Computers)(GB).rom",	0x10000, 0x71df64a8, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_spooksl, MSX_spooksl, msx_msx)
STD_ROM_FN(MSX_spooksl)

struct BurnDriver BurnDrvMSX_spooksl = {
	"msx_spooksl", NULL, "msx_msx", NULL, "1985",
	"Spooks & Ladders\0", NULL, "Kuma Computers", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_spookslRomInfo, MSX_spookslRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Star Avenger

static struct BurnRomInfo MSX_staravngRomDesc[] = {
	{ "Star Avenger (1984)(Kuma Computers)(GB).rom",	0x10000, 0xd7c545cc, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_staravng, MSX_staravng, msx_msx)
STD_ROM_FN(MSX_staravng)

struct BurnDriver BurnDrvMSX_staravng = {
	"msx_staravng", NULL, "msx_msx", NULL, "1984",
	"Star Avenger\0", NULL, "Kuma Computers", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_staravngRomInfo, MSX_staravngRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Lazy Jones

static struct BurnRomInfo MSX_lazyjonesRomDesc[] = {
	{ "Lazy Jones (1985)(Terminal Software)(GB).rom",	0x10000, 0xa94670f3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_lazyjones, MSX_lazyjones, msx_msx)
STD_ROM_FN(MSX_lazyjones)

struct BurnDriver BurnDrvMSX_lazyjones = {
	"msx_lazyjones", NULL, "msx_msx", NULL, "1985",
	"Lazy Jones\0", "Crashes... check later", "Terminal Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_lazyjonesRomInfo, MSX_lazyjonesRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Afteroids

static struct BurnRomInfo MSX_afteroidsRomDesc[] = {
	{ "Afteroids (1988)(Zigurat Software)(ES).rom",	0x20000, 0x8a599dad, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_afteroids, MSX_afteroids, msx_msx)
STD_ROM_FN(MSX_afteroids)

struct BurnDriver BurnDrvMSX_afteroids = {
	"msx_afteroids", NULL, "msx_msx", NULL, "1988",
	"Afteroids\0", NULL, "Zigurat Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_afteroidsRomInfo, MSX_afteroidsRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Bubbler

static struct BurnRomInfo MSX_bubblerRomDesc[] = {
	{ "Bubbler (1987)(Ultimate Play The Game)(GB).rom",	0x20000, 0x0150e22e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_bubbler, MSX_bubbler, msx_msx)
STD_ROM_FN(MSX_bubbler)

struct BurnDriver BurnDrvMSX_bubbler = {
	"msx_bubbler", NULL, "msx_msx", NULL, "1987",
	"Bubbler\0", NULL, "Ultimate Play The Game", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_bubblerRomInfo, MSX_bubblerRomName, NULL, NULL, MSXInputInfo, MSXJoyport2DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Cyberun

static struct BurnRomInfo MSX_cyberunRomDesc[] = {
	{ "Cyberun (1985)(Ultimate Play The Game)(GB).rom",	0x20000, 0x12c23b87, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_cyberun, MSX_cyberun, msx_msx)
STD_ROM_FN(MSX_cyberun)

struct BurnDriver BurnDrvMSX_cyberun = {
	"msx_cyberun", NULL, "msx_msx", NULL, "1985",
	"Cyberun\0", NULL, "Ultimate Play The Game", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_cyberunRomInfo, MSX_cyberunRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Mortadelo y Filemon

static struct BurnRomInfo MSX_mortadeloRomDesc[] = {
	{ "Mortadelo y Filemon (1988)(Dro Soft)(ES).rom",	0x20000, 0xff4852f4, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mortadelo, MSX_mortadelo, msx_msx)
STD_ROM_FN(MSX_mortadelo)

struct BurnDriver BurnDrvMSX_mortadelo = {
	"msx_mortadelo", NULL, "msx_msx", NULL, "1988",
	"Mortadelo y Filemon\0", NULL, "Dro Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_mortadeloRomInfo, MSX_mortadeloRomName, NULL, NULL, MSXInputInfo, MSXJoyport2DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Mortadelo y Filemon II - Safari Callejero

static struct BurnRomInfo MSX_mortadelo2RomDesc[] = {
	{ "Mortadelo y Filemon II - Safari Callejero (1990)(Dro Soft)(ES).rom",	0x20000, 0x0c6a21b2, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mortadelo2, MSX_mortadelo2, msx_msx)
STD_ROM_FN(MSX_mortadelo2)

struct BurnDriver BurnDrvMSX_mortadelo2 = {
	"msx_mortadelo2", NULL, "msx_msx", NULL, "1990",
	"Mortadelo y Filemon II - Safari Callejero\0", NULL, "Dro Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_mortadelo2RomInfo, MSX_mortadelo2RomName, NULL, NULL, MSXInputInfo, MSXJoyport2DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Pentagram

static struct BurnRomInfo MSX_pentagramRomDesc[] = {
	{ "Pentagram (1986)(Ultimate Play The Game)(GB).rom",	0x20000, 0x04b90e53, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_pentagram, MSX_pentagram, msx_msx)
STD_ROM_FN(MSX_pentagram)

struct BurnDriver BurnDrvMSX_pentagram = {
	"msx_pentagram", NULL, "msx_msx", NULL, "1986",
	"Pentagram\0", NULL, "Ultimate Play The Game", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_pentagramRomInfo, MSX_pentagramRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Turbo Girl

static struct BurnRomInfo MSX_turbogirlRomDesc[] = {
	{ "Turbo Girl (1988)(Dinamic Software)(ES).rom",	0x20000, 0x6fded825, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_turbogirl, MSX_turbogirl, msx_msx)
STD_ROM_FN(MSX_turbogirl)

struct BurnDriver BurnDrvMSX_turbogirl = {
	"msx_turbogirl", NULL, "msx_msx", NULL, "1988",
	"Turbo Girl\0", NULL, "Dinamic Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_turbogirlRomInfo, MSX_turbogirlRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Super Suwanggi - Super Altered Beast

static struct BurnRomInfo MSX_suwanggiRomDesc[] = {
	{ "Super Suwanggi - Super Altered Beast (1990)(Clover)[ASCII16k].rom",	0x10000, 0xe953f2ab, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_suwanggi, MSX_suwanggi, msx_msx)
STD_ROM_FN(MSX_suwanggi)

struct BurnDriver BurnDrvMSX_suwanggi = {
	"msx_suwanggi", NULL, "msx_msx", NULL, "1990",
	"Super Suwanggi - Super Altered Beast\0", NULL, "Clover", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII16, GBF_MISC, 0,
	MSXGetZipName, MSX_suwanggiRomInfo, MSX_suwanggiRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Amurisus

static struct BurnRomInfo MSX_amurisusRomDesc[] = {
	{ "amurisus.rom",	0x10000, 0xacf0cab8, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_amurisus, MSX_amurisus, msx_msx)
STD_ROM_FN(MSX_amurisus)

struct BurnDriver BurnDrvMSX_amurisus = {
	"msx_amurisus", NULL, "msx_msx", NULL, "2013",
	"Amurisus (demo)\0", NULL, "Lieves!Tuore", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_amurisusRomInfo, MSX_amurisusRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Rock 'n Roller

static struct BurnRomInfo MSX_rocknrollerRomDesc[] = {
	{ "Rock'nRoller.rom",	0x10000, 0xb0c0e46d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_rocknroller, MSX_rocknroller, msx_msx)
STD_ROM_FN(MSX_rocknroller)

struct BurnDriver BurnDrvMSX_rocknroller = {
	"msx_rocknroller", NULL, "msx_msx", NULL, "1988",
	"Rock 'n Roller\0", NULL, "RCC", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_rocknrollerRomInfo, MSX_rocknrollerRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Super Ilevan

static struct BurnRomInfo MSX_ilevanRomDesc[] = {
	{ "Super Ilevan (Koichi Nishida, 1987).cas",	0x59a6, 0x87157a8d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_ilevan, MSX_ilevan, msx_msx)
STD_ROM_FN(MSX_ilevan)

struct BurnDriver BurnDrvMSX_ilevan = {
	"msx_ilevan", NULL, "msx_msx", NULL, "1987",
	"Super Ilevan\0", NULL, "Koichi Nishida", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_ilevanRomInfo, MSX_ilevanRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasBloadDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Scentipede

static struct BurnRomInfo MSX_scentipedeRomDesc[] = {
	{ "Scentipede (1986)(Eaglesoft)(NL)[RUN'CAS-'].cas",	0x93b8, 0x63f02ed5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_scentipede, MSX_scentipede, msx_msx)
STD_ROM_FN(MSX_scentipede)

struct BurnDriver BurnDrvMSX_scentipede = {
	"msx_scentipede", NULL, "msx_msx", NULL, "1986",
	"Scentipede\0", NULL, "Aackosoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_scentipedeRomInfo, MSX_scentipedeRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

static struct BurnRomInfo MSX_zona0RomDesc[] = {
	{ "zona 0 (1991)(topo soft)(es)[run'cas-'].cas",	0x10f6e, 0x0b919291, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_zona0, MSX_zona0, msx_msx)
STD_ROM_FN(MSX_zona0)

struct BurnDriver BurnDrvMSX_zona0 = {
	"msx_zona0", NULL, "msx_msx", NULL, "1991",
	"Zona 0 (Spa)\0", NULL, "Topo Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_zona0RomInfo, MSX_zona0RomName, NULL, NULL, MSXInputInfo, MSXJoyport2DIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Wizard's Lair (Euro)

static struct BurnRomInfo MSX_wizlairRomDesc[] = {
	{ "wizard's lair (1986)(bubble bus software)(gb)(m3)[cload + run].cas",	0x0c718, 0x8dc78f7b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_wizlair, MSX_wizlair, msx_msx)
STD_ROM_FN(MSX_wizlair)

struct BurnDriver BurnDrvMSX_wizlair = {
	"msx_wizlair", NULL, "msx_msx", NULL, "1986",
	"Wizard's Lair (Euro)\0", NULL, "Bubble Bus Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_wizlairRomInfo, MSX_wizlairRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasCloadDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Winterhawk (Euro)

static struct BurnRomInfo MSX_winthawkRomDesc[] = {
	{ "winterhawk (1988)(eurosoft)(nl)[run'cas-'].cas",	0x0be12, 0x143c3a53, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_winthawk, MSX_winthawk, msx_msx)
STD_ROM_FN(MSX_winthawk)

struct BurnDriver BurnDrvMSX_winthawk = {
	"msx_winthawk", NULL, "msx_msx", NULL, "1988",
	"Winterhawk (Euro)\0", NULL, "Eurosoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_winthawkRomInfo, MSX_winthawkRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Void Runner (Euro)

static struct BurnRomInfo MSX_voidrunRomDesc[] = {
	{ "voidrunner (1987)(mastertronic added dimension)(gb)[run'cas-'].cas",	0x05398, 0x7e9b01f8, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_voidrun, MSX_voidrun, msx_msx)
STD_ROM_FN(MSX_voidrun)

struct BurnDriver BurnDrvMSX_voidrun = {
	"msx_voidrun", NULL, "msx_msx", NULL, "1987",
	"Void Runner (Euro)\0", NULL, "Mastertronic", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_voidrunRomInfo, MSX_voidrunRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	VoidrunnerDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Wamp Cola (Spa)

static struct BurnRomInfo MSX_wampcolaRomDesc[] = {
	{ "wamp cola (1988)(iber soft)(es)[aka dracula][run'cas-'].cas",	0x04d4b, 0xa6438fe8, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_wampcola, MSX_wampcola, msx_msx)
STD_ROM_FN(MSX_wampcola)

struct BurnDriver BurnDrvMSX_wampcola = {
	"msx_wampcola", NULL, "msx_msx", NULL, "1988",
	"Wamp Cola (Spa)\0", NULL, "Iber Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_wampcolaRomInfo, MSX_wampcolaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Vortex Raider (Euro)

static struct BurnRomInfo MSX_vortraidRomDesc[] = {
	{ "vortex raider (1988)(eurosoft)(nl)[run'cas-'].cas",	0x11580, 0x5feacdfe, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_vortraid, MSX_vortraid, msx_msx)
STD_ROM_FN(MSX_vortraid)

struct BurnDriver BurnDrvMSX_vortraid = {
	"msx_vortraid", NULL, "msx_msx", NULL, "1988",
	"Vortex Raider (Euro)\0", NULL, "Eurosoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_vortraidRomInfo, MSX_vortraidRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Vestron (Euro)

static struct BurnRomInfo MSX_vestronRomDesc[] = {
	{ "vestron (1986)(players software)(gb)[bload'cas-',r].cas",	0x04e1f, 0xf5e9984d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_vestron, MSX_vestron, msx_msx)
STD_ROM_FN(MSX_vestron)

struct BurnDriver BurnDrvMSX_vestron = {
	"msx_vestron", NULL, "msx_msx", NULL, "1986",
	"Vestron (Euro)\0", NULL, "Players Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_vestronRomInfo, MSX_vestronRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasBloadDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Valkyr (Euro)

static struct BurnRomInfo MSX_valkyrRomDesc[] = {
	{ "valkyr (1985)(gremlin graphics software)(gb)[bload'cas-',r].cas",	0x0b342, 0x1466df59, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_valkyr, MSX_valkyr, msx_msx)
STD_ROM_FN(MSX_valkyr)

struct BurnDriver BurnDrvMSX_valkyr = {
	"msx_valkyr", NULL, "msx_msx", NULL, "1985",
	"Valkyr (Euro)\0", NULL, "Gremlin Graphics", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_valkyrRomInfo, MSX_valkyrRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasBloadDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Underground (Spa)

static struct BurnRomInfo MSX_undergndRomDesc[] = {
	{ "underground (1988)(system 4)(es)[run'cas-'].cas",	0x0b080, 0x0019e507, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_undergnd, MSX_undergnd, msx_msx)
STD_ROM_FN(MSX_undergnd)

struct BurnDriver BurnDrvMSX_undergnd = {
	"msx_undergnd", NULL, "msx_msx", NULL, "1988",
	"Underground (Spa)\0", NULL, "System 4", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_undergndRomInfo, MSX_undergndRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Trantor - The Last Stormtrooper (Euro)

static struct BurnRomInfo MSX_trantorRomDesc[] = {
	{ "trantor - the last stormtrooper (1987)(go!)[run'cas-'].cas",	0x09b3e, 0x2cea057a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_trantor, MSX_trantor, msx_msx)
STD_ROM_FN(MSX_trantor)

struct BurnDriver BurnDrvMSX_trantor = {
	"msx_trantor", NULL, "msx_msx", NULL, "1987",
	"Trantor - The Last Stormtrooper (Euro)\0", NULL, "Go!", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_trantorRomInfo, MSX_trantorRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Time Trax (Euro)

static struct BurnRomInfo MSX_timetraxRomDesc[] = {
	{ "time trax (1986)(bug-byte software)(gb)[run'cas-'].cas",	0x0e2aa, 0x70617478, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_timetrax, MSX_timetrax, msx_msx)
STD_ROM_FN(MSX_timetrax)

struct BurnDriver BurnDrvMSX_timetrax = {
	"msx_timetrax", NULL, "msx_msx", NULL, "1986",
	"Time Trax (Euro)\0", NULL, "Bug-Byte Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_timetraxRomInfo, MSX_timetraxRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Titanic (Spa)

static struct BurnRomInfo MSX_titanicRomDesc[] = {
	{ "titanic (1988)(topo soft)(es)(side a)[run'cas-'].cas",	0x0c300, 0x7e7a8653, BRF_PRG | BRF_ESS },
	{ "titanic (1988)(topo soft)(es)(side b)[run'cas-'].cas",	0x0c91c, 0x45b1c035, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_titanic, MSX_titanic, msx_msx)
STD_ROM_FN(MSX_titanic)

struct BurnDriver BurnDrvMSX_titanic = {
	"msx_titanic", NULL, "msx_msx", NULL, "1988",
	"Titanic (Spa)\0", NULL, "Topo Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_titanicRomInfo, MSX_titanicRomName, NULL, NULL, MSXInputInfo, MSXJoyport2DIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Time Bomb (Euro)

static struct BurnRomInfo MSX_timebombRomDesc[] = {
	{ "time bomb (1987)(methodic solutions)(nl)[run'cas-'].cas",	0x07f88, 0x7d364b31, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_timebomb, MSX_timebomb, msx_msx)
STD_ROM_FN(MSX_timebomb)

struct BurnDriver BurnDrvMSX_timebomb = {
	"msx_timebomb", NULL, "msx_msx", NULL, "1987",
	"Time Bomb (Euro)\0", NULL, "Methodic Solutions", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_timebombRomInfo, MSX_timebombRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Terramex (Euro)

static struct BurnRomInfo MSX_terramexRomDesc[] = {
	{ "terramex (1988)(grandslam entertainments)(gb)[bload'cas-',r].cas",	0x0ff61, 0x5dc6b09a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_terramex, MSX_terramex, msx_msx)
STD_ROM_FN(MSX_terramex)

struct BurnDriver BurnDrvMSX_terramex = {
	"msx_terramex", NULL, "msx_msx", NULL, "1988",
	"Terramex (Euro)\0", NULL, "Grandslam Entertainments", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_terramexRomInfo, MSX_terramexRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasBloadDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Terminus - Prison Planet (Euro)

static struct BurnRomInfo MSX_terminusRomDesc[] = {
	{ "terminus - prison planet (1987)(mastertronic added dimension)(gb)[run'cas-'][martos].cas",	0x09947, 0xfe2b00e5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_terminus, MSX_terminus, msx_msx)
STD_ROM_FN(MSX_terminus)

struct BurnDriver BurnDrvMSX_terminus = {
	"msx_terminus", NULL, "msx_msx", NULL, "1987",
	"Terminus - Prison Planet (Euro)\0", NULL, "Mastertronic Added Dimension", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_terminusRomInfo, MSX_terminusRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Starbuggy (Euro)

static struct BurnRomInfo MSX_starbuggRomDesc[] = {
	{ "starbuggy (1988)(eurosoft)(nl)[run'cas-'].cas",	0x0a028, 0x94d525f5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_starbugg, MSX_starbugg, msx_msx)
STD_ROM_FN(MSX_starbugg)

struct BurnDriver BurnDrvMSX_starbugg = {
	"msx_starbugg", NULL, "msx_msx", NULL, "1988",
	"Starbuggy (Euro)\0", NULL, "Eurosoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_starbuggRomInfo, MSX_starbuggRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Moon Rider (Euro)

static struct BurnRomInfo MSX_moonridrRomDesc[] = {
	{ "moon rider (1986)(eaglesoft)(nl)[run'cas-'].cas",	0x12468, 0x8f94818c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_moonridr, MSX_moonridr, msx_msx)
STD_ROM_FN(MSX_moonridr)

struct BurnDriver BurnDrvMSX_moonridr = {
	"msx_moonridr", "msx_starbugg", "msx_msx", NULL, "1986",
	"Moon Rider (Euro)\0", NULL, "Eaglesoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_moonridrRomInfo, MSX_moonridrRomName, NULL, NULL, MSXInputInfo, MSXJoyport2DIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Spy vs Spy II - The Island Caper (Euro)

static struct BurnRomInfo MSX_spyvssp2RomDesc[] = {
	{ "spy vs spy ii - the island caper (1987)(databyte)[run'cas-'].cas",	0x09676, 0x8324d38d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_spyvssp2, MSX_spyvssp2, msx_msx)
STD_ROM_FN(MSX_spyvssp2)

struct BurnDriver BurnDrvMSX_spyvssp2 = {
	"msx_spyvssp2", NULL, "msx_msx", NULL, "1987",
	"Spy vs Spy II - The Island Caper (Euro)\0", NULL, "Databyte", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_spyvssp2RomInfo, MSX_spyvssp2RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Splash (Spa)

static struct BurnRomInfo MSX_splashRomDesc[] = {
	{ "splash (1986)(mind games espana)(es)[run'cas-'].cas",	0x08293, 0x68943e75, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_splash, MSX_splash, msx_msx)
STD_ROM_FN(MSX_splash)

struct BurnDriver BurnDrvMSX_splash = {
	"msx_splash", NULL, "msx_msx", NULL, "1986",
	"Splash (Spa)\0", NULL, "Mind Games España", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_splashRomInfo, MSX_splashRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Space Walk (Euro)

static struct BurnRomInfo MSX_spacewlkRomDesc[] = {
	{ "space walk (1985)(mastertronic)(gb)[bload'cas-',r].cas",	0x01977, 0x0b88b07a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_spacewlk, MSX_spacewlk, msx_msx)
STD_ROM_FN(MSX_spacewlk)

struct BurnDriver BurnDrvMSX_spacewlk = {
	"msx_spacewlk", NULL, "msx_msx", NULL, "1985",
	"Space Walk (Euro)\0", NULL, "Mastertronic", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_spacewlkRomInfo, MSX_spacewlkRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasBloadDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Space Rescue (Euro)

static struct BurnRomInfo MSX_srescueRomDesc[] = {
	{ "space rescue (1988)(eurosoft)(nl)[run'cas-'].cas",	0x059a8, 0x12318fd4, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_srescue, MSX_srescue, msx_msx)
STD_ROM_FN(MSX_srescue)

struct BurnDriver BurnDrvMSX_srescue = {
	"msx_srescue", NULL, "msx_msx", NULL, "1988",
	"Space Rescue (Euro)\0", NULL, "Eurosoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_srescueRomInfo, MSX_srescueRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Smack Wacker (Euro)

static struct BurnRomInfo MSX_smackwacRomDesc[] = {
	{ "smack wacker (1986)(eaglesoft)(nl)[run'cas-'].cas",	0x10638, 0xb1ff3d49, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_smackwac, MSX_smackwac, msx_msx)
STD_ROM_FN(MSX_smackwac)

struct BurnDriver BurnDrvMSX_smackwac = {
	"msx_smackwac", NULL, "msx_msx", NULL, "1986",
	"Smack Wacker (Euro)\0", NULL, "Eaglesoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_smackwacRomInfo, MSX_smackwacRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Sky Diver (Jpn)

static struct BurnRomInfo MSX_skydiverRomDesc[] = {
	{ "sky diver (1984)(hudson soft - japanese softbank)(jp)(en)[bload'cas-',r][martos].cas",	0x02dd6, 0x93a40a2c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_skydiver, MSX_skydiver, msx_msx)
STD_ROM_FN(MSX_skydiver)

struct BurnDriver BurnDrvMSX_skydiver = {
	"msx_skydiver", NULL, "msx_msx", NULL, "1984",
	"Sky Diver (Jpn)\0", NULL, "Hudson Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_skydiverRomInfo, MSX_skydiverRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasBloadDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Skull Exilon (Spa)

static struct BurnRomInfo MSX_skulexilRomDesc[] = {
	{ "skull exilon (1988)(iber soft)(es)[aka safari x][run'cas-'].cas",	0x077e7, 0xec4e2be2, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_skulexil, MSX_skulexil, msx_msx)
STD_ROM_FN(MSX_skulexil)

struct BurnDriver BurnDrvMSX_skulexil = {
	"msx_skulexil", NULL, "msx_msx", NULL, "1988",
	"Skull Exilon (Spa)\0", NULL, "Iber Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_skulexilRomInfo, MSX_skulexilRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Silent Shadow (Euro?)

static struct BurnRomInfo MSX_sshadowRomDesc[] = {
	{ "silent shadow (1988)(topo soft)[run'cas-'].cas",	0x260b2, 0xa995ca1b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_sshadow, MSX_sshadow, msx_msx)
STD_ROM_FN(MSX_sshadow)

struct BurnDriver BurnDrvMSX_sshadow = {
	"msx_sshadow", NULL, "msx_msx", NULL, "1988",
	"Silent Shadow (Euro?)\0", NULL, "Topo Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_sshadowRomInfo, MSX_sshadowRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Shnax (Euro)

static struct BurnRomInfo MSX_shnaxRomDesc[] = {
	{ "shnax (1985)(kuma computers)(gb)[run'cas-',r][martos].cas",	0x063f2, 0xe14a712e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_shnax, MSX_shnax, msx_msx)
STD_ROM_FN(MSX_shnax)

struct BurnDriver BurnDrvMSX_shnax = {
	"msx_shnax", NULL, "msx_msx", NULL, "1985",
	"Shnax (Euro)\0", NULL, "Kuma Computers", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_shnaxRomInfo, MSX_shnaxRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Shark Hunter (Euro)

static struct BurnRomInfo MSX_sharkhRomDesc[] = {
	{ "shark hunter (1984)(electric software)(gb)[bload'cas-',r].cas",	0x0a875, 0x2066f45b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_sharkh, MSX_sharkh, msx_msx)
STD_ROM_FN(MSX_sharkh)

struct BurnDriver BurnDrvMSX_sharkh = {
	"msx_sharkh", NULL, "msx_msx", NULL, "1984",
	"Shark Hunter (Euro)\0", NULL, "Electric Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_sharkhRomInfo, MSX_sharkhRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasBloadDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Science Fiction (Euro)

static struct BurnRomInfo MSX_scifiRomDesc[] = {
	{ "science fiction (1986)(eaglesoft)(nl)[run'cas-'].cas",	0x06820, 0x9bdc6759, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_scifi, MSX_scifi, msx_msx)
STD_ROM_FN(MSX_scifi)

struct BurnDriver BurnDrvMSX_scifi = {
	"msx_scifi", NULL, "msx_msx", NULL, "1986",
	"Science Fiction (Euro)\0", NULL, "Eaglesoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_scifiRomInfo, MSX_scifiRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Runner (Fra)

static struct BurnRomInfo MSX_runnerRomDesc[] = {
	{ "runner (1986)(loriciels)(fr)[run'cas-'].cas",	0x096e8, 0x80a0f4d2, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_runner, MSX_runner, msx_msx)
STD_ROM_FN(MSX_runner)

struct BurnDriver BurnDrvMSX_runner = {
	"msx_runner", NULL, "msx_msx", NULL, "1986",
	"Runner (Fra)\0", NULL, "Loriciels", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_runnerRomInfo, MSX_runnerRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Break Out!

static struct BurnRomInfo MSX_breakouttRomDesc[] = {
	{ "Break Out! (1985)(Toshiba-EMI)(JP)[needs 64k in slot 2].rom",	0x20000, 0x784d5f7f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_breakoutt, MSX_breakoutt, msx_msx)
STD_ROM_FN(MSX_breakoutt)

struct BurnDriver BurnDrvMSX_breakoutt = {
	"msx_breakoutt", NULL, "msx_msx", NULL, "1985",
	"Break Out!\0", NULL, "Toshiba-EMI", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_breakouttRomInfo, MSX_breakouttRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	SwapRamDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Roboy (Euro)

static struct BurnRomInfo MSX_roboyRomDesc[] = {
	{ "roboy (1987)(methodic solutions)(nl)[run'cas-'].cas",	0x06d08, 0xa61c1805, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_roboy, MSX_roboy, msx_msx)
STD_ROM_FN(MSX_roboy)

struct BurnDriver BurnDrvMSX_roboy = {
	"msx_roboy", NULL, "msx_msx", NULL, "1987",
	"Roboy (Euro)\0", NULL, "Methodic Solutions", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_roboyRomInfo, MSX_roboyRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Rock 'n Roller (Spa)

static struct BurnRomInfo MSX_rockrollRomDesc[] = {
	{ "rock 'n roller (1988)(topo soft)(es)[run'cas-'].cas",	0x0cbf1, 0xcc0bc2e0, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_rockroll, MSX_rockroll, msx_msx)
STD_ROM_FN(MSX_rockroll)

struct BurnDriver BurnDrvMSX_rockroll = {
	"msx_rockroll", NULL, "msx_msx", NULL, "1988",
	"Rock 'n Roller (Spa)\0", NULL, "Topo Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_rockrollRomInfo, MSX_rockrollRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Robot Wars (Euro)

static struct BurnRomInfo MSX_robotwarRomDesc[] = {
	{ "robot wars (1986)(eaglesoft)(nl)[run'cas-'].cas",	0x0c482, 0x08f5270d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_robotwar, MSX_robotwar, msx_msx)
STD_ROM_FN(MSX_robotwar)

struct BurnDriver BurnDrvMSX_robotwar = {
	"msx_robotwar", NULL, "msx_msx", NULL, "1986",
	"Robot Wars (Euro)\0", NULL, "Eaglesoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_robotwarRomInfo, MSX_robotwarRomName, NULL, NULL, MSXInputInfo, MSXJoyport2DIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Rescate Atlantida (Spa)

static struct BurnRomInfo MSX_rescatlRomDesc[] = {
	{ "rescate atlantida (1989)(dinamic software)(es)[run'cas-'].cas",	0x1d48a, 0x39411ccb, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_rescatl, MSX_rescatl, msx_msx)
STD_ROM_FN(MSX_rescatl)

struct BurnDriver BurnDrvMSX_rescatl = {
	"msx_rescatl", NULL, "msx_msx", NULL, "1989",
	"Rescate Atlantida (Spa)\0", NULL, "Dinamic Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_rescatlRomInfo, MSX_rescatlRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Rath-Tha (Spa)

static struct BurnRomInfo MSX_raththaRomDesc[] = {
	{ "rath-tha (1989)(positive)(es)[run'cas-'].cas",	0x0e2ef, 0xd804222d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_raththa, MSX_raththa, msx_msx)
STD_ROM_FN(MSX_raththa)

struct BurnDriver BurnDrvMSX_raththa = {
	"msx_raththa", NULL, "msx_msx", NULL, "1989",
	"Rath-Tha (Spa)\0", NULL, "Positive", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_raththaRomInfo, MSX_raththaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// R.A.M. (Spa)

static struct BurnRomInfo MSX_ramRomDesc[] = {
	{ "r.a.m. (1990)(topo soft)(es)[run'cas-'].cas",	0x10158, 0x497462cc, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_ram, MSX_ram, msx_msx)
STD_ROM_FN(MSX_ram)

struct BurnDriver BurnDrvMSX_ram = {
	"msx_ram", NULL, "msx_msx", NULL, "1990",
	"R.A.M. (Spa)\0", NULL, "Topo Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_ramRomInfo, MSX_ramRomName, NULL, NULL, MSXInputInfo, MSXJoyport2DIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Psychedelia (Euro)

static struct BurnRomInfo MSX_psychedRomDesc[] = {
	{ "psychedelia (1984)(aackosoft)(nl)[run'cas-'].cas",	0x01bbe, 0xdb0b5487, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_psyched, MSX_psyched, msx_msx)
STD_ROM_FN(MSX_psyched)

struct BurnDriver BurnDrvMSX_psyched = {
	"msx_psyched", NULL, "msx_msx", NULL, "1984",
	"Psychedelia (Euro)\0", NULL, "Aackosoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_psychedRomInfo, MSX_psychedRomName, NULL, NULL, MSXInputInfo, MSXJoyport2DIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Polar Star (Jpn)

static struct BurnRomInfo MSX_polstarRomDesc[] = {
	{ "polar star (1984)(micro cabin)(jp)[cload + run].cas",	0x05cdb, 0xba314b2c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_polstar, MSX_polstar, msx_msx)
STD_ROM_FN(MSX_polstar)

struct BurnDriver BurnDrvMSX_polstar = {
	"msx_polstar", NULL, "msx_msx", NULL, "1984",
	"Polar Star (Jpn)\0", NULL, "Micro Cabin", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_polstarRomInfo, MSX_polstarRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasCloadDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Pharaoh's Revenge (Euro)

static struct BurnRomInfo MSX_pharaohRomDesc[] = {
	{ "pharaoh's revenge (1988)(eurosoft)(nl)[run'cas-'].cas",	0x0d7c0, 0x38a49e2b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_pharaoh, MSX_pharaoh, msx_msx)
STD_ROM_FN(MSX_pharaoh)

struct BurnDriver BurnDrvMSX_pharaoh = {
	"msx_pharaoh", NULL, "msx_msx", NULL, "1988",
	"Pharaoh's Revenge (Euro)\0", NULL, "Eurosoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_pharaohRomInfo, MSX_pharaohRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Pac-Mania (Euro)

static struct BurnRomInfo MSX_pacmaniaRomDesc[] = {
	{ "pac-mania (1988)(grandslam entertainments)(gb)[bload'cas-',r].cas",	0x11b71, 0xfaeb79cd, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_pacmania, MSX_pacmania, msx_msx)
STD_ROM_FN(MSX_pacmania)

struct BurnDriver BurnDrvMSX_pacmania = {
	"msx_pacmania", NULL, "msx_msx", NULL, "1988",
	"Pac-Mania (Euro)\0", NULL, "Grandslam Entertainments", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_pacmaniaRomInfo, MSX_pacmaniaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasBloadDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Outroyd (Jpn)

static struct BurnRomInfo MSX_outroydRomDesc[] = {
	{ "outroyd (1985)(magical zoo)(jp)[cload + run].cas",	0x08d76, 0x24cb4684, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_outroyd, MSX_outroyd, msx_msx)
STD_ROM_FN(MSX_outroyd)

struct BurnDriver BurnDrvMSX_outroyd = {
	"msx_outroyd", NULL, "msx_msx", NULL, "1985",
	"Outroyd (Jpn)\0", NULL, "Magical Zoo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_outroydRomInfo, MSX_outroydRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasCloadDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Obliterator (Spa)

static struct BurnRomInfo MSX_obliterRomDesc[] = {
	{ "obliterator (1989)(dro soft)(es)[run'cas-'].cas",	0x0e297, 0xe9653411, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_obliter, MSX_obliter, msx_msx)
STD_ROM_FN(MSX_obliter)

struct BurnDriver BurnDrvMSX_obliter = {
	"msx_obliter", NULL, "msx_msx", NULL, "1989",
	"Obliterator (Spa)\0", NULL, "Dro Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_obliterRomInfo, MSX_obliterRomName, NULL, NULL, MSXInputInfo, MSXJoyport2DIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Mythos (Euro)

static struct BurnRomInfo MSX_mythosRomDesc[] = {
	{ "mythos (1990)(opera soft)[bload'cas-',r].cas",	0x0e340, 0x36f71cfb, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mythos, MSX_mythos, msx_msx)
STD_ROM_FN(MSX_mythos)

struct BurnDriver BurnDrvMSX_mythos = {
	"msx_mythos", NULL, "msx_msx", NULL, "1990",
	"Mythos (Euro)\0", NULL, "Opera Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_mythosRomInfo, MSX_mythosRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasBloadDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// El Mundo Perdido (Spa)

static struct BurnRomInfo MSX_mundoperRomDesc[] = {
	{ "mundo perdido, el (1988)(topo soft)(es)[run'cas-'].cas",	0x0fb47, 0x488a17b0, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mundoper, MSX_mundoper, msx_msx)
STD_ROM_FN(MSX_mundoper)

struct BurnDriver BurnDrvMSX_mundoper = {
	"msx_mundoper", NULL, "msx_msx", NULL, "1988",
	"El Mundo Perdido (Spa)\0", NULL, "Topo Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_mundoperRomInfo, MSX_mundoperRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Mr. Wong's Loopy Laundry (Euro)

static struct BurnRomInfo MSX_mrwongRomDesc[] = {
	{ "mr. wong's loopy laundry (1984)(artic computing)(gb)[bload'cas-',r].cas",	0x034d6, 0xe116b37a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mrwong, MSX_mrwong, msx_msx)
STD_ROM_FN(MSX_mrwong)

struct BurnDriver BurnDrvMSX_mrwong = {
	"msx_mrwong", NULL, "msx_msx", NULL, "1984",
	"Mr. Wong's Loopy Laundry (Euro)\0", NULL, "Artic Computing", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_mrwongRomInfo, MSX_mrwongRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasBloadDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// El Motorista Sideral (Spa)

static struct BurnRomInfo MSX_sideralRomDesc[] = {
	{ "motorista sideral, el (1986)(anaya multimedia)(es)[run'cas-'].cas",	0x0642f, 0x8b07f4f6, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_sideral, MSX_sideral, msx_msx)
STD_ROM_FN(MSX_sideral)

struct BurnDriver BurnDrvMSX_sideral = {
	"msx_sideral", NULL, "msx_msx", NULL, "1986",
	"El Motorista Sideral (Spa)\0", NULL, "Anaya Multimedia", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_sideralRomInfo, MSX_sideralRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Milk Race (Euro)

static struct BurnRomInfo MSX_milkraceRomDesc[] = {
	{ "milk race (1987)(mastertronic)(gb)[run'cas-'].cas",	0x0b610, 0x74ce546e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_milkrace, MSX_milkrace, msx_msx)
STD_ROM_FN(MSX_milkrace)

struct BurnDriver BurnDrvMSX_milkrace = {
	"msx_milkrace", NULL, "msx_msx", NULL, "1987",
	"Milk Race (Euro)\0", "Press '0' to start", "Mastertronic", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_milkraceRomInfo, MSX_milkraceRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Master of the Lamps (Euro)

static struct BurnRomInfo MSX_mastlampRomDesc[] = {
	{ "master of the lamps (1985)(activision)(us)[run'cas-'].cas",	0x0a1c7, 0xfc70048f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mastlamp, MSX_mastlamp, msx_msx)
STD_ROM_FN(MSX_mastlamp)

struct BurnDriver BurnDrvMSX_mastlamp = {
	"msx_mastlamp", NULL, "msx_msx", NULL, "1985",
	"Master of the Lamps (Euro)\0", NULL, "Activision", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_mastlampRomInfo, MSX_mastlampRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Mask II (Euro)

static struct BurnRomInfo MSX_mask2RomDesc[] = {
	{ "mask ii (1987)(gremlin graphics software)(gb)[bload'cas-',r].cas",	0x13d30, 0x7639c66b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mask2, MSX_mask2, msx_msx)
STD_ROM_FN(MSX_mask2)

struct BurnDriver BurnDrvMSX_mask2 = {
	"msx_mask2", NULL, "msx_msx", NULL, "1987",
	"Mask II (Euro)\0", NULL, "Gremlin Graphics", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_mask2RomInfo, MSX_mask2RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasBloadDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Mask III - Venom Strikes Back (Euro)

static struct BurnRomInfo MSX_mask3RomDesc[] = {
	{ "mask iii - venom strikes back (1988)(gremlin graphics software)(gb)[bload'cas-',r].cas",	0x10ef2, 0xc6774b56, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mask3, MSX_mask3, msx_msx)
STD_ROM_FN(MSX_mask3)

struct BurnDriver BurnDrvMSX_mask3 = {
	"msx_mask3", NULL, "msx_msx", NULL, "1988",
	"Mask III - Venom Strikes Back (Euro)\0", NULL, "Gremlin Graphics", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_mask3RomInfo, MSX_mask3RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasBloadDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// M-Droid (Euro)

static struct BurnRomInfo MSX_mdroidRomDesc[] = {
	{ "m-droid (19xx)(blue ribbon software)[run'cas-'].cas",	0x09a96, 0x03869d9e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mdroid, MSX_mdroid, msx_msx)
STD_ROM_FN(MSX_mdroid)

struct BurnDriver BurnDrvMSX_mdroid = {
	"msx_mdroid", NULL, "msx_msx", NULL, "198?",
	"M-Droid (Euro)\0", NULL, "Blue Ribbon Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_mdroidRomInfo, MSX_mdroidRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Krakout (Euro)

static struct BurnRomInfo MSX_krakoutRomDesc[] = {
	{ "krakout (1987)(gremlin graphics software)(gb)[bload'cas-',r].cas",	0x107f3, 0x5a0776f5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_krakout, MSX_krakout, msx_msx)
STD_ROM_FN(MSX_krakout)

struct BurnDriver BurnDrvMSX_krakout = {
	"msx_krakout", NULL, "msx_msx", NULL, "1987",
	"Krakout (Euro)\0", NULL, "Gremlin Graphics", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_krakoutRomInfo, MSX_krakoutRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasBloadDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Robocop (Euro)

static struct BurnRomInfo MSX_robocopRomDesc[] = {
	{ "robocop (1988)(ocean software)(gb)(side a)[bload'cas-',r].cas",	0x13ee2, 0xfb8aa367, BRF_PRG | BRF_ESS },
	{ "robocop (1988)(ocean software)(gb)(side b)[bload'cas-',r].cas",	0x0e0f2, 0x8a43b63f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_robocop, MSX_robocop, msx_msx)
STD_ROM_FN(MSX_robocop)

struct BurnDriver BurnDrvMSX_robocop = {
	"msx_robocop", NULL, "msx_msx", NULL, "1988",
	"Robocop (Euro)\0", NULL, "Ocean Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_robocopRomInfo, MSX_robocopRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasBloadDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Meganova (Spa)

static struct BurnRomInfo MSX_meganovaRomDesc[] = {
	{ "meganova (1988)(dinamic software)(es)[run'cas-'].cas",	0x1264f, 0xa055f596, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_meganova, MSX_meganova, msx_msx)
STD_ROM_FN(MSX_meganova)

struct BurnDriver BurnDrvMSX_meganova = {
	"msx_meganova", NULL, "msx_msx", NULL, "1988",
	"Meganova (Spa)\0", NULL, "Dinamic Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_meganovaRomInfo, MSX_meganovaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Licence to Kill (Euro)

static struct BurnRomInfo MSX_lic2killRomDesc[] = {
	{ "007 - licence to kill (1989)(domark)(gb)[run'cas-'].cas",	0x0cfe2, 0xbb994bbc, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_lic2kill, MSX_lic2kill, msx_msx)
STD_ROM_FN(MSX_lic2kill)

struct BurnDriver BurnDrvMSX_lic2kill = {
	"msx_lic2kill", NULL, "msx_msx", NULL, "1989",
	"Licence to Kill (Euro)\0", NULL, "Domark", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_lic2killRomInfo, MSX_lic2killRomName, NULL, NULL, MSXInputInfo, MSXJoyport2DIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Whopper Chase (Spa)

static struct BurnRomInfo MSX_whopperRomDesc[] = {
	{ "whopper chase (1987)(erbe software)(es)[run'cas-'][martos].cas",	0x08f43, 0x7c28051b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_whopper, MSX_whopper, msx_msx)
STD_ROM_FN(MSX_whopper)

struct BurnDriver BurnDrvMSX_whopper = {
	"msx_whopper", NULL, "msx_msx", NULL, "1987",
	"Whopper Chase (Spa)\0", NULL, "Erbe Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_whopperRomInfo, MSX_whopperRomName, NULL, NULL, MSXInputInfo, MSXJoyport2DIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Klax (Euro)

static struct BurnRomInfo MSX_klaxRomDesc[] = {
	{ "klax (1990)(domark)(gb)[bload'cas-',r].cas",	0x11b71, 0x0f06d7a4, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_klax, MSX_klax, msx_msx)
STD_ROM_FN(MSX_klax)

struct BurnDriver BurnDrvMSX_klax = {
	"msx_klax", NULL, "msx_msx", NULL, "1990",
	"Klax (Euro)\0", NULL, "Domark", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_klaxRomInfo, MSX_klaxRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasBloadDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// King Leonard (Spa)

static struct BurnRomInfo MSX_kingleonRomDesc[] = {
	{ "king leonard (1986)(mind games espana)(es)[bload'cas-',r].cas",	0x09a87, 0x0f752c47, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_kingleon, MSX_kingleon, msx_msx)
STD_ROM_FN(MSX_kingleon)

struct BurnDriver BurnDrvMSX_kingleon = {
	"msx_kingleon", NULL, "msx_msx", NULL, "1986",
	"King Leonard (Spa)\0", NULL, "Mind Games España", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_kingleonRomInfo, MSX_kingleonRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasBloadDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Khazzad-Dum (Spa)

static struct BurnRomInfo MSX_khazzadRomDesc[] = {
	{ "khazzad-dum (1989)(system 4)(es)[run'cas-'].cas",	0x0a2de, 0x27c10ddf, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_khazzad, MSX_khazzad, msx_msx)
STD_ROM_FN(MSX_khazzad)

struct BurnDriver BurnDrvMSX_khazzad = {
	"msx_khazzad", NULL, "msx_msx", NULL, "1989",
	"Khazzad-Dum (Spa)\0", NULL, "System 4", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_khazzadRomInfo, MSX_khazzadRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Jungle Warrior (Spa)

static struct BurnRomInfo MSX_jungwarrRomDesc[] = {
	{ "jungle warrior (1990)(zigurat software)(es)[run'cas-'].cas",	0x0e8d9, 0xdf10a5b8, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_jungwarr, MSX_jungwarr, msx_msx)
STD_ROM_FN(MSX_jungwarr)

struct BurnDriver BurnDrvMSX_jungwarr = {
	"msx_jungwarr", NULL, "msx_msx", NULL, "1990",
	"Jungle Warrior (Spa)\0", NULL, "Zigurat Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_jungwarrRomInfo, MSX_jungwarrRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Jumping Jack (Euro)

static struct BurnRomInfo MSX_jumpjackRomDesc[] = {
	{ "jumping jack (1986)(livewire software)(gb)[run'cas-'][martos].cas",	0x056a1, 0xa3e9284f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_jumpjack, MSX_jumpjack, msx_msx)
STD_ROM_FN(MSX_jumpjack)

struct BurnDriver BurnDrvMSX_jumpjack = {
	"msx_jumpjack", NULL, "msx_msx", NULL, "1986",
	"Jumping Jack (Euro)\0", NULL, "Livewire Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_jumpjackRomInfo, MSX_jumpjackRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Joe Blade (Euro)

static struct BurnRomInfo MSX_joebladeRomDesc[] = {
	{ "joe blade (1989)(players software)(gb)[bload'cas-',r].cas",	0x068a7, 0xc958dd10, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_joeblade, MSX_joeblade, msx_msx)
STD_ROM_FN(MSX_joeblade)

struct BurnDriver BurnDrvMSX_joeblade = {
	"msx_joeblade", NULL, "msx_msx", NULL, "1989",
	"Joe Blade (Euro)\0", NULL, "Players Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_joebladeRomInfo, MSX_joebladeRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasBloadDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Jet Fighter (Euro, Eurosoft)

static struct BurnRomInfo MSX_jetfightRomDesc[] = {
	{ "jet fighter (1988)(eurosoft)(nl)[run'cas-'].cas",	0x08c60, 0x0a788a57, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_jetfight, MSX_jetfight, msx_msx)
STD_ROM_FN(MSX_jetfight)

struct BurnDriver BurnDrvMSX_jetfight = {
	"msx_jetfight", NULL, "msx_msx", NULL, "1988",
	"Jet Fighter (Euro, Eurosoft)\0", NULL, "Eurosoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_jetfightRomInfo, MSX_jetfightRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Jaws (Spa?)

static struct BurnRomInfo MSX_jawsRomDesc[] = {
	{ "jaws (1989)(screen 7)[run'cas-'].cas",	0x0bfda, 0x908478e6, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_jaws, MSX_jaws, msx_msx)
STD_ROM_FN(MSX_jaws)

struct BurnDriver BurnDrvMSX_jaws = {
	"msx_jaws", NULL, "msx_msx", NULL, "1989",
	"Jaws (Spa?)\0", NULL, "A.M.H.", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_jawsRomInfo, MSX_jawsRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Jackson City (Spa)

static struct BurnRomInfo MSX_jackcityRomDesc[] = {
	{ "jackson city (1990)(g.ll. software)(es)[run'cas-'][ctrl][martos].cas",	0x105ae, 0x7cdfa5b3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_jackcity, MSX_jackcity, msx_msx)
STD_ROM_FN(MSX_jackcity)

struct BurnDriver BurnDrvMSX_jackcity = {
	"msx_jackcity", NULL, "msx_msx", NULL, "1990",
	"Jackson City (Spa)\0", NULL, "G.LL. Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_jackcityRomInfo, MSX_jackcityRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Indiana Jones and the Temple of Doom (Euro)

static struct BurnRomInfo MSX_indyRomDesc[] = {
	{ "indiana jones and the temple of doom (1987)(u.s. gold)(gb)[run'cas-'].cas",	0x0c014, 0x5c70746c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_indy, MSX_indy, msx_msx)
STD_ROM_FN(MSX_indy)

struct BurnDriver BurnDrvMSX_indy = {
	"msx_indy", NULL, "msx_msx", NULL, "1987",
	"Indiana Jones and the Temple of Doom (Euro)\0", NULL, "U.S. Gold", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_indyRomInfo, MSX_indyRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Inca (Euro)

static struct BurnRomInfo MSX_incaRomDesc[] = {
	{ "inca (1987)(eaglesoft)(nl)[run'cas-'].cas",	0x06588, 0xbf667649, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_inca, MSX_inca, msx_msx)
STD_ROM_FN(MSX_inca)

struct BurnDriver BurnDrvMSX_inca = {
	"msx_inca", NULL, "msx_msx", NULL, "1987",
	"Inca (Euro)\0", NULL, "Eaglesoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_incaRomInfo, MSX_incaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Ice-Breaker (Spa)

static struct BurnRomInfo MSX_icebreakRomDesc[] = {
	{ "ice-breaker (1990)(topo soft)(es)[run'cas-'].cas",	0x12fa4, 0x5079a19c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_icebreak, MSX_icebreak, msx_msx)
STD_ROM_FN(MSX_icebreak)

struct BurnDriver BurnDrvMSX_icebreak = {
	"msx_icebreak", NULL, "msx_msx", NULL, "1990",
	"Ice-Breaker (Spa)\0", NULL, "Topo Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_icebreakRomInfo, MSX_icebreakRomName, NULL, NULL, MSXInputInfo, MSXJoyport2DIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// The Ice King (Euro)

static struct BurnRomInfo MSX_icekingRomDesc[] = {
	{ "ice king, the (1986)(cds micro systems)(gb)[run'cas-'][martos].cas",	0x09657, 0x160cad54, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_iceking, MSX_iceking, msx_msx)
STD_ROM_FN(MSX_iceking)

struct BurnDriver BurnDrvMSX_iceking = {
	"msx_iceking", NULL, "msx_msx", NULL, "1986",
	"The Ice King (Euro)\0", NULL, "CDS Micro Systems", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_icekingRomInfo, MSX_icekingRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Rambo III (Euro)

static struct BurnRomInfo MSX_rambo3RomDesc[] = {
	{ "rambo iii (1988)(ocean software)(gb)[bload'cas-',r].cas",	0x2b5c0, 0x448cce74, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_rambo3, MSX_rambo3, msx_msx)
STD_ROM_FN(MSX_rambo3)

struct BurnDriver BurnDrvMSX_rambo3 = {
	"msx_rambo3", NULL, "msx_msx", NULL, "1988",
	"Rambo III (Euro)\0", NULL, "Ocean Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_rambo3RomInfo, MSX_rambo3RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasBloadDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Psycho Pig U.X.B. (Euro)

static struct BurnRomInfo MSX_psychpigRomDesc[] = {
	{ "psycho pig u.x.b. (1988)(u.s. gold)(gb)[run'cas-'].cas",	0x0aa67, 0xefe975a0, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_psychpig, MSX_psychpig, msx_msx)
STD_ROM_FN(MSX_psychpig)

struct BurnDriver BurnDrvMSX_psychpig = {
	"msx_psychpig", NULL, "msx_msx", NULL, "1988",
	"Psycho Pig U.X.B. (Euro)\0", NULL, "U.S. Gold", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_psychpigRomInfo, MSX_psychpigRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Punchy (Euro)

static struct BurnRomInfo MSX_punchyRomDesc[] = {
	{ "punchy (1984)(mr. micro)[bload'cas-',r].cas",	0x03d7f, 0x1f389e5d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_punchy, MSX_punchy, msx_msx)
STD_ROM_FN(MSX_punchy)

struct BurnDriver BurnDrvMSX_punchy = {
	"msx_punchy", NULL, "msx_msx", NULL, "1984",
	"Punchy (Euro)\0", NULL, "Mr. Micro", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_punchyRomInfo, MSX_punchyRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasBloadDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Hundra (Spa)

static struct BurnRomInfo MSX_hundraRomDesc[] = {
	{ "hundra (1988)(dinamic software)(es)[run'cas-'].cas",	0x0befd, 0xe0ad7798, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_hundra, MSX_hundra, msx_msx)
STD_ROM_FN(MSX_hundra)

struct BurnDriver BurnDrvMSX_hundra = {
	"msx_hundra", NULL, "msx_msx", NULL, "1988",
	"Hundra (Spa)\0", NULL, "Dinamic Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_hundraRomInfo, MSX_hundraRomName, NULL, NULL, MSXInputInfo, MSXJoyport2DIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Hunchback (Euro)

static struct BurnRomInfo MSX_hnchbackRomDesc[] = {
	{ "hunchback (1984)(ocean software)(gb)[bload'cas-',r].cas",	0x0402f, 0x42b6c9c5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_hnchback, MSX_hnchback, msx_msx)
STD_ROM_FN(MSX_hnchback)

struct BurnDriver BurnDrvMSX_hnchback = {
	"msx_hnchback", NULL, "msx_msx", NULL, "1984",
	"Hunchback (Euro)\0", NULL, "Ocean Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_hnchbackRomInfo, MSX_hnchbackRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasBloadDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Hot Shoe (Euro)

static struct BurnRomInfo MSX_hotshoeRomDesc[] = {
	{ "hot shoe (1984)(longman software)(gb)[bload'cas-',r][martos].cas",	0x03627, 0x582c9afc, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_hotshoe, MSX_hotshoe, msx_msx)
STD_ROM_FN(MSX_hotshoe)

struct BurnDriver BurnDrvMSX_hotshoe = {
	"msx_hotshoe", NULL, "msx_msx", NULL, "1984",
	"Hot Shoe (Euro)\0", NULL, "Longman Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_hotshoeRomInfo, MSX_hotshoeRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasBloadDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Hostages (Spa, Hacked by Martos)

static struct BurnRomInfo MSX_hostagesRomDesc[] = {
	{ "hostages (1990)(erbe)(es)(en)(m martos)[run'cas-'].cas",	0x2234a, 0x1a6e23df, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_hostages, MSX_hostages, msx_msx)
STD_ROM_FN(MSX_hostages)

struct BurnDriver BurnDrvMSX_hostages = {
	"msx_hostages", NULL, "msx_msx", NULL, "1990",
	"Hostages (Spa, Hacked by Martos)\0", NULL, "Erbe Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_hostagesRomInfo, MSX_hostagesRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Highway Encounter (Spa)

static struct BurnRomInfo MSX_highwayenRomDesc[] = {
	{ "highway encounter (1985)(dinamic software)(es)[run'cas-'].cas",	0x0a415, 0x1c036ed9, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_highwayen, MSX_highwayen, msx_msx)
STD_ROM_FN(MSX_highwayen)

struct BurnDriver BurnDrvMSX_highwayen = {
	"msx_highwayen", NULL, "msx_msx", NULL, "1985",
	"Highway Encounter (Spa)\0", NULL, "Dinamic Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_highwayenRomInfo, MSX_highwayenRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Hercules - Slayer of the Damned (Euro)

static struct BurnRomInfo MSX_herculesRomDesc[] = {
	{ "hercules - slayer of the damned (1988)(gremlin graphics software)(gb)[bload'cas-',r].cas",	0x21daa, 0x2b3fc6f5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_hercules, MSX_hercules, msx_msx)
STD_ROM_FN(MSX_hercules)

struct BurnDriver BurnDrvMSX_hercules = {
	"msx_hercules", NULL, "msx_msx", NULL, "1988",
	"Hercules - Slayer of the Damned (Euro)\0", NULL, "Gremlin Graphics", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_herculesRomInfo, MSX_herculesRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasBloadDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Heat Seeker - Missil (Spa)

static struct BurnRomInfo MSX_heatseekRomDesc[] = {
	{ "heat seeker - missil (1986)(mind games espana)(es)[run'cas-'][martos].cas",	0x08859, 0x7515587f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_heatseek, MSX_heatseek, msx_msx)
STD_ROM_FN(MSX_heatseek)

struct BurnDriver BurnDrvMSX_heatseek = {
	"msx_heatseek", NULL, "msx_msx", NULL, "1986",
	"Heat Seeker - Missil (Spa)\0", NULL, "Mind Games España", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_heatseekRomInfo, MSX_heatseekRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Haunted House (Euro)

static struct BurnRomInfo MSX_hhouseRomDesc[] = {
	{ "haunted house (1988)(eurosoft)(nl)[run'cas-'].cas",	0x11078, 0x3c4ee5c3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_hhouse, MSX_hhouse, msx_msx)
STD_ROM_FN(MSX_hhouse)

struct BurnDriver BurnDrvMSX_hhouse = {
	"msx_hhouse", NULL, "msx_msx", NULL, "1988",
	"Haunted House (Euro)\0", NULL, "Eurosoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_hhouseRomInfo, MSX_hhouseRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Harvey Smith's Showjumper (Euro, Hacked Protection)

static struct BurnRomInfo MSX_showjumphRomDesc[] = {
	{ "harvey smith's showjumper (1985)(software projects)[run'cas-'].cas",	0x073a8, 0x3dc22773, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_showjumph, MSX_showjumph, msx_msx)
STD_ROM_FN(MSX_showjumph)

struct BurnDriver BurnDrvMSX_showjumph = {
	"msx_showjumph", NULL, "msx_msx", NULL, "1985",
	"Harvey Smith's Showjumper (Euro, Hacked Protection)\0", NULL, "Software Projects", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_showjumphRomInfo, MSX_showjumphRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Hard Boiled (Euro)

static struct BurnRomInfo MSX_hardboilRomDesc[] = {
	{ "hard boiled (1987)(methodic solutions)(nl)[run'cas-'].cas",	0x0ef38, 0xd7fad1be, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_hardboil, MSX_hardboil, msx_msx)
STD_ROM_FN(MSX_hardboil)

struct BurnDriver BurnDrvMSX_hardboil = {
	"msx_hardboil", NULL, "msx_msx", NULL, "1987",
	"Hard Boiled (Euro)\0", NULL, "Methodic Solutions", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_hardboilRomInfo, MSX_hardboilRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Hammer Boy (Spa)

static struct BurnRomInfo MSX_hammboyRomDesc[] = {
	{ "hammer boy (1991)(dinamic software)(es)(side a)[run'cas-'].cas",	0x0da92, 0x907a6931, BRF_PRG | BRF_ESS },
	{ "hammer boy (1991)(dinamic software)(es)(side b)[run'cas-'].cas",	0x0da92, 0x2e7eba8e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_hammboy, MSX_hammboy, msx_msx)
STD_ROM_FN(MSX_hammboy)

struct BurnDriver BurnDrvMSX_hammboy = {
	"msx_hammboy", NULL, "msx_msx", NULL, "1991",
	"Hammer Boy (Spa)\0", "Keyboard control only.", "Dinamic Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_hammboyRomInfo, MSX_hammboyRomName, NULL, NULL, MSXInputInfo, MSXJoyCursorDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Habilit (Spa)

static struct BurnRomInfo MSX_habilitRomDesc[] = {
	{ "habilit (1988)(iber soft)(es)[run'cas-'].cas",	0x09d77, 0xb48a02de, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_habilit, MSX_habilit, msx_msx)
STD_ROM_FN(MSX_habilit)

struct BurnDriver BurnDrvMSX_habilit = {
	"msx_habilit", NULL, "msx_msx", NULL, "1988",
	"Habilit (Spa)\0", NULL, "Iber Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_habilitRomInfo, MSX_habilitRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Guttblaster (Euro)

static struct BurnRomInfo MSX_guttblstRomDesc[] = {
	{ "guttblaster (1988)(eurosoft)(nl)[run'cas-'].cas",	0x0f598, 0x221735ed, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_guttblst, MSX_guttblst, msx_msx)
STD_ROM_FN(MSX_guttblst)

struct BurnDriver BurnDrvMSX_guttblst = {
	"msx_guttblst", NULL, "msx_msx", NULL, "1988",
	"Guttblaster (Euro)\0", NULL, "Eurosoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_guttblstRomInfo, MSX_guttblstRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Grid Trap (Euro)

static struct BurnRomInfo MSX_gridtrapRomDesc[] = {
	{ "grid trap (1985)(livewire software)(gb)[run'cas-'].cas",	0x026e8, 0x6cacf65f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_gridtrap, MSX_gridtrap, msx_msx)
STD_ROM_FN(MSX_gridtrap)

struct BurnDriver BurnDrvMSX_gridtrap = {
	"msx_gridtrap", NULL, "msx_msx", NULL, "1985",
	"Grid Trap (Euro)\0", NULL, "Livewire Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_gridtrapRomInfo, MSX_gridtrapRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Goody (Spa)

static struct BurnRomInfo MSX_goodyRomDesc[] = {
	{ "goody (1987)(opera soft)(es)[bload'cas-',r].cas",	0x0dd40, 0x8ce47b52, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_goody, MSX_goody, msx_msx)
STD_ROM_FN(MSX_goody)

struct BurnDriver BurnDrvMSX_goody = {
	"msx_goody", NULL, "msx_msx", NULL, "1987",
	"Goody (Spa)\0", NULL, "Opera Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_goodyRomInfo, MSX_goodyRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasBloadDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Glass (Spa)

static struct BurnRomInfo MSX_glassRomDesc[] = {
	{ "glass (1985)(quicksilva)(es-gb)[run'cas-'].cas",	0x082d7, 0x9902f154, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_glass, MSX_glass, msx_msx)
STD_ROM_FN(MSX_glass)

struct BurnDriver BurnDrvMSX_glass = {
	"msx_glass", NULL, "msx_msx", NULL, "1985",
	"Glass (Spa)\0", NULL, "Quicksilva", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_glassRomInfo, MSX_glassRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Ghostbusters II (Euro)

static struct BurnRomInfo MSX_ghostbs2RomDesc[] = {
	{ "ghostbusters ii (1989)(activision)(us)[run'cas-'].cas",	0x29e91, 0x6e8a88f6, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_ghostbs2, MSX_ghostbs2, msx_msx)
STD_ROM_FN(MSX_ghostbs2)

struct BurnDriver BurnDrvMSX_ghostbs2 = {
	"msx_ghostbs2", NULL, "msx_msx", NULL, "1989",
	"Ghostbusters II (Euro)\0", NULL, "Activision", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_ghostbs2RomInfo, MSX_ghostbs2RomName, NULL, NULL, MSXInputInfo, MSXJoyport2DIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Ghost (Spa)

static struct BurnRomInfo MSX_ghostRomDesc[] = {
	{ "ghost (1989)(mind games espana)(es)[run'cas-'].cas",	0x0bed7, 0xc3d65cb5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_ghost, MSX_ghost, msx_msx)
STD_ROM_FN(MSX_ghost)

struct BurnDriver BurnDrvMSX_ghost = {
	"msx_ghost", NULL, "msx_msx", NULL, "1989",
	"Ghost (Spa)\0", NULL, "Mind Games Espana", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_ghostRomInfo, MSX_ghostRomName, NULL, NULL, MSXInputInfo, MSXJoyCursorDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Gang Man (Euro)

static struct BurnRomInfo MSX_gangmanRomDesc[] = {
	{ "gang man (1988)(armati software)[run'cas-'].cas",	0x02167, 0x39aa53da, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_gangman, MSX_gangman, msx_msx)
STD_ROM_FN(MSX_gangman)

struct BurnDriver BurnDrvMSX_gangman = {
	"msx_gangman", NULL, "msx_msx", NULL, "1988",
	"Gang Man (Euro)\0", NULL, "Armati Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_gangmanRomInfo, MSX_gangmanRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Gang Man (Jpn)

static struct BurnRomInfo MSX_gangmanjRomDesc[] = {
	{ "gangman (1984)(hudson soft)(jp)[run'cas-'].cas",	0x0228f, 0xe45bc3e0, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_gangmanj, MSX_gangmanj, msx_msx)
STD_ROM_FN(MSX_gangmanj)

struct BurnDriver BurnDrvMSX_gangmanj = {
	"msx_gangmanj", "msx_gangman", "msx_msx", NULL, "1983",
	"Gang Man (Jpn)\0", NULL, "Hudson Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_gangmanjRomInfo, MSX_gangmanjRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Forbidden Fruit (Spa)

static struct BurnRomInfo MSX_ffruitRomDesc[] = {
	{ "forbidden fruit (1985)(mind games espana)(es)[cload + run].cas",	0x03faf, 0x945a9dd0, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_ffruit, MSX_ffruit, msx_msx)
STD_ROM_FN(MSX_ffruit)

struct BurnDriver BurnDrvMSX_ffruit = {
	"msx_ffruit", NULL, "msx_msx", NULL, "1985",
	"Forbidden Fruit (Spa)\0", NULL, "Mind Games España", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_ffruitRomInfo, MSX_ffruitRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasCloadDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Flash Gordon (Euro)

static struct BurnRomInfo MSX_flashgorRomDesc[] = {
	{ "flash gordon (1987)(mastertronic added dimension)(gb)[run'cas-'].cas",	0x12dd8, 0x4acea35f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_flashgor, MSX_flashgor, msx_msx)
STD_ROM_FN(MSX_flashgor)

struct BurnDriver BurnDrvMSX_flashgor = {
	"msx_flashgor", NULL, "msx_msx", NULL, "1987",
	"Flash Gordon (Euro)\0", NULL, "Mastertronic Added Dimension", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_flashgorRomInfo, MSX_flashgorRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Firehawk (Euro)

static struct BurnRomInfo MSX_firehawkRomDesc[] = {
	{ "firehawk (1987)(players software)(gb)[run'cas-'].cas",	0x0973f, 0xc88425df, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_firehawk, MSX_firehawk, msx_msx)
STD_ROM_FN(MSX_firehawk)

struct BurnDriver BurnDrvMSX_firehawk = {
	"msx_firehawk", NULL, "msx_msx", NULL, "1987",
	"Firehawk (Euro)\0", NULL, "Players Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_firehawkRomInfo, MSX_firehawkRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Finders Keepers (Euro)

static struct BurnRomInfo MSX_findkeepRomDesc[] = {
	{ "finders keepers (1986)(mastertronic)(gb)[run'cas-'].cas",	0x0a49f, 0x685e2505, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_findkeep, MSX_findkeep, msx_msx)
STD_ROM_FN(MSX_findkeep)

struct BurnDriver BurnDrvMSX_findkeep = {
	"msx_findkeep", NULL, "msx_msx", NULL, "1986",
	"Finders Keepers (Euro)\0", NULL, "Mastertronic", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_findkeepRomInfo, MSX_findkeepRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Exterminator (Euro)

static struct BurnRomInfo MSX_exterminRomDesc[] = {
	{ "exterminator (1987)(eaglesoft)(nl)(en)[run'cas-'][martos].cas",	0x069b0, 0xccec8d53, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_extermin, MSX_extermin, msx_msx)
STD_ROM_FN(MSX_extermin)

struct BurnDriver BurnDrvMSX_extermin = {
	"msx_extermin", NULL, "msx_msx", NULL, "1987",
	"Exterminator (Euro)\0", NULL, "Eaglesoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_exterminRomInfo, MSX_exterminRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Dynamite Dan (Euro)

static struct BurnRomInfo MSX_dynadanRomDesc[] = {
	{ "dynamite dan (1986)(mirrorsoft)(gb)[bload'cas-',r].cas",	0x0c411, 0x3f349148, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_dynadan, MSX_dynadan, msx_msx)
STD_ROM_FN(MSX_dynadan)

struct BurnDriver BurnDrvMSX_dynadan = {
	"msx_dynadan", NULL, "msx_msx", NULL, "1986",
	"Dynamite Dan (Euro)\0", NULL, "Mirrorsoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_dynadanRomInfo, MSX_dynadanRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasBloadDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Drome (Euro)

static struct BurnRomInfo MSX_dromeRomDesc[] = {
	{ "drome (1987)(eaglesoft)(nl)[run'cas-'].cas",	0x10040, 0x5d3d2237, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_drome, MSX_drome, msx_msx)
STD_ROM_FN(MSX_drome)

struct BurnDriver BurnDrvMSX_drome = {
	"msx_drome", NULL, "msx_msx", NULL, "1987",
	"Drome (Euro)\0", NULL, "Eaglesoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_dromeRomInfo, MSX_dromeRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Dragon Ninja (Euro)

static struct BurnRomInfo MSX_dninjaRomDesc[] = {
	{ "dragon ninja (1988)(imagine software)(gb)[run'cas-'].cas",	0x1a6e4, 0x3c495adc, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_dninja, MSX_dninja, msx_msx)
STD_ROM_FN(MSX_dninja)

struct BurnDriver BurnDrvMSX_dninja = {
	"msx_dninja", NULL, "msx_msx", NULL, "1988",
	"Dragon Ninja (Euro)\0", NULL, "Imagine Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_dninjaRomInfo, MSX_dninjaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Dog Fighter (Euro)

static struct BurnRomInfo MSX_dogfightRomDesc[] = {
	{ "dog fighter (1986)(kuma computers)(gb)[bload'cas-',r].cas",	0x02196, 0xb2233398, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_dogfight, MSX_dogfight, msx_msx)
STD_ROM_FN(MSX_dogfight)

struct BurnDriver BurnDrvMSX_dogfight = {
	"msx_dogfight", NULL, "msx_msx", NULL, "1986",
	"Dog Fighter (Euro)\0", NULL, "Kuma Computers", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_dogfightRomInfo, MSX_dogfightRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasBloadDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Disc Warrior (Euro)

static struct BurnRomInfo MSX_discwarrRomDesc[] = {
	{ "disc warrior (1984)(alligata software)(gb)[run'cas-'].cas",	0x0d06f, 0x3999c18f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_discwarr, MSX_discwarr, msx_msx)
STD_ROM_FN(MSX_discwarr)

struct BurnDriver BurnDrvMSX_discwarr = {
	"msx_discwarr", NULL, "msx_msx", NULL, "1984",
	"Disc Warrior (Euro)\0", NULL, "Alligata Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_discwarrRomInfo, MSX_discwarrRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Destroyer (Spa)

static struct BurnRomInfo MSX_destroyrRomDesc[] = {
	{ "destroyer (1986)(mind games espana)(es)[run'cas-'].cas",	0x0c67f, 0xbad9355d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_destroyr, MSX_destroyr, msx_msx)
STD_ROM_FN(MSX_destroyr)

struct BurnDriver BurnDrvMSX_destroyr = {
	"msx_destroyr", NULL, "msx_msx", NULL, "1986",
	"Destroyer (Spa)\0", NULL, "Mind Games España", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_destroyrRomInfo, MSX_destroyrRomName, NULL, NULL, MSXInputInfo, MSXJoyport2DIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Desolator (Euro)

static struct BurnRomInfo MSX_desolatrRomDesc[] = {
	{ "desolator (1986)(gremlin graphics software)(gb)[bload'cas-',r].cas",	0x06241, 0xb228b187, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_desolatr, MSX_desolatr, msx_msx)
STD_ROM_FN(MSX_desolatr)

struct BurnDriver BurnDrvMSX_desolatr = {
	"msx_desolatr", NULL, "msx_msx", NULL, "1986",
	"Desolator (Euro)\0", NULL, "Gremlin Graphics", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_desolatrRomInfo, MSX_desolatrRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasBloadDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Cyberbig (Spa)

static struct BurnRomInfo MSX_cyberbigRomDesc[] = {
	{ "cyberbig (1989)(animagic)(es)[run'cas-'].cas",	0x0e72b, 0x99023cf9, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_cyberbig, MSX_cyberbig, msx_msx)
STD_ROM_FN(MSX_cyberbig)

struct BurnDriver BurnDrvMSX_cyberbig = {
	"msx_cyberbig", NULL, "msx_msx", NULL, "1989",
	"Cyberbig (Spa)\0", NULL, "Animagic", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_cyberbigRomInfo, MSX_cyberbigRomName, NULL, NULL, MSXInputInfo, MSXJoyport2DIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Cosmic Sheriff (Spa)

static struct BurnRomInfo MSX_csheriffRomDesc[] = {
	{ "cosmic sheriff (1989)(dinamic software)(es)[run'cas-'].cas",	0x0b7bb, 0xe1984c54, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_csheriff, MSX_csheriff, msx_msx)
STD_ROM_FN(MSX_csheriff)

struct BurnDriver BurnDrvMSX_csheriff = {
	"msx_csheriff", NULL, "msx_msx", NULL, "1989",
	"Cosmic Sheriff (Spa)\0", NULL, "Dinamic Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_csheriffRomInfo, MSX_csheriffRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Cosa Nostra (Spa)

static struct BurnRomInfo MSX_cosanostRomDesc[] = {
	{ "cosa nostra (1986)(opera soft)(es)[bload'cas-',r].cas",	0x0a329, 0xda23ba44, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_cosanost, MSX_cosanost, msx_msx)
STD_ROM_FN(MSX_cosanost)

struct BurnDriver BurnDrvMSX_cosanost = {
	"msx_cosanost", NULL, "msx_msx", NULL, "1986",
	"Cosa Nostra (Spa)\0", NULL, "Opera Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_cosanostRomInfo, MSX_cosanostRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasBloadDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Colony (Euro)

static struct BurnRomInfo MSX_colonyRomDesc[] = {
	{ "colony (1987)(bulldog)(gb)[run'cas-'].cas",	0x07c22, 0xf6d82b88, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_colony, MSX_colony, msx_msx)
STD_ROM_FN(MSX_colony)

struct BurnDriver BurnDrvMSX_colony = {
	"msx_colony", NULL, "msx_msx", NULL, "1987",
	"Colony (Euro)\0", NULL, "Bulldog", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_colonyRomInfo, MSX_colonyRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Chuckie Egg (Euro, AandF Software)

static struct BurnRomInfo MSX_chuckieRomDesc[] = {
	{ "chuckie egg (1984)(aandf software)[bload'cas-',r].cas",	0x03a54, 0x3c44f316, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_chuckie, MSX_chuckie, msx_msx)
STD_ROM_FN(MSX_chuckie)

struct BurnDriver BurnDrvMSX_chuckie = {
	"msx_chuckie", NULL, "msx_msx", NULL, "1984",
	"Chuckie Egg (Euro, AandF Software)\0", NULL, "AandF Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_chuckieRomInfo, MSX_chuckieRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasBloadDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Chopper One (Euro)

static struct BurnRomInfo MSX_chopper1RomDesc[] = {
	{ "chopper one (1988)(eurosoft)(nl)[run'cas-'].cas",	0x0d6d0, 0xefb363a3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_chopper1, MSX_chopper1, msx_msx)
STD_ROM_FN(MSX_chopper1)

struct BurnDriver BurnDrvMSX_chopper1 = {
	"msx_chopper1", NULL, "msx_msx", NULL, "1988",
	"Chopper One (Euro)\0", NULL, "Eurosoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_chopper1RomInfo, MSX_chopper1RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Chiller (Euro)

static struct BurnRomInfo MSX_chillerRomDesc[] = {
	{ "chiller (1985)(mastertronic)(gb)[run'cas-'].cas",	0x0683b, 0xb33086a0, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_chiller, MSX_chiller, msx_msx)
STD_ROM_FN(MSX_chiller)

struct BurnDriver BurnDrvMSX_chiller = {
	"msx_chiller", NULL, "msx_msx", NULL, "1985",
	"Chiller (Euro)\0", NULL, "Mastertronic", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_chillerRomInfo, MSX_chillerRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Chicken Chase (Euro)

static struct BurnRomInfo MSX_chickenRomDesc[] = {
	{ "chicken chase (1986)(bug-byte software)(gb)[run'cas-'].cas",	0x09227, 0x027c7e11, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_chicken, MSX_chicken, msx_msx)
STD_ROM_FN(MSX_chicken)

struct BurnDriver BurnDrvMSX_chicken = {
	"msx_chicken", NULL, "msx_msx", NULL, "1986",
	"Chicken Chase (Euro)\0", NULL, "Bug-Byte Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_chickenRomInfo, MSX_chickenRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Carlos Sainz - Campeonato del Mundo (Spa)

static struct BurnRomInfo MSX_csainzRomDesc[] = {
	{ "carlos sainz (1990)(zigurat software)(es)[run'cas-'].cas",	0x1025b, 0xe8eb4c98, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_csainz, MSX_csainz, msx_msx)
STD_ROM_FN(MSX_csainz)

struct BurnDriver BurnDrvMSX_csainz = {
	"msx_csainz", NULL, "msx_msx", NULL, "1990",
	"Carlos Sainz - Campeonato del Mundo (Spa)\0", NULL, "Zigurat Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_csainzRomInfo, MSX_csainzRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Buzz Off! (Euro)

static struct BurnRomInfo MSX_buzzoffRomDesc[] = {
	{ "buzz off! (1984)(electric software)(gb)[run'cas-'].cas",	0x06cfd, 0x2c76939c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_buzzoff, MSX_buzzoff, msx_msx)
STD_ROM_FN(MSX_buzzoff)

struct BurnDriver BurnDrvMSX_buzzoff = {
	"msx_buzzoff", NULL, "msx_msx", NULL, "1984",
	"Buzz Off! (Euro)\0", NULL, "Electric Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_buzzoffRomInfo, MSX_buzzoffRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Bumpy (Fra)

static struct BurnRomInfo MSX_bumpyRomDesc[] = {
	{ "bumpy (1989)(loriciels)(fr)[run'cas-'].cas",	0x0becb, 0xdc1d5578, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_bumpy, MSX_bumpy, msx_msx)
STD_ROM_FN(MSX_bumpy)

struct BurnDriver BurnDrvMSX_bumpy = {
	"msx_bumpy", NULL, "msx_msx", NULL, "1989",
	"Bumpy (Fra)\0", NULL, "Loriciels", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_bumpyRomInfo, MSX_bumpyRomName, NULL, NULL, MSXInputInfo, MSXJoyport2DIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Bronx (Spa)

static struct BurnRomInfo MSX_bronxRomDesc[] = {
	{ "bronx (1989)(animagic)(es)[run'cas-'].cas",	0x18fc2, 0x7b6597bf, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_bronx, MSX_bronx, msx_msx)
STD_ROM_FN(MSX_bronx)

struct BurnDriver BurnDrvMSX_bronx = {
	"msx_bronx", NULL, "msx_msx", NULL, "1989",
	"Bronx (Spa)\0", NULL, "Animagic", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_bronxRomInfo, MSX_bronxRomName, NULL, NULL, MSXInputInfo, MSXJoyport2DIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Breaker Breaker (Euro)

static struct BurnRomInfo MSX_breakerRomDesc[] = {
	{ "breaker breaker (1988)(eurosoft)(nl)[run'cas-'].cas",	0x0e1e8, 0xdeb36e3f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_breaker, MSX_breaker, msx_msx)
STD_ROM_FN(MSX_breaker)

struct BurnDriver BurnDrvMSX_breaker = {
	"msx_breaker", NULL, "msx_msx", NULL, "1988",
	"Breaker Breaker (Euro)\0", NULL, "Eurosoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_breakerRomInfo, MSX_breakerRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Bounder (Euro)

static struct BurnRomInfo MSX_bounderRomDesc[] = {
	{ "bounder (1987)(gremlin graphics software)(gb)[bload'cas-',r].cas",	0x106d2, 0xab5bdc07, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_bounder, MSX_bounder, msx_msx)
STD_ROM_FN(MSX_bounder)

struct BurnDriver BurnDrvMSX_bounder = {
	"msx_bounder", NULL, "msx_msx", NULL, "1987",
	"Bounder (Euro)\0", NULL, "Gremlin Graphics", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_bounderRomInfo, MSX_bounderRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasBloadDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Bounce (Euro)

static struct BurnRomInfo MSX_bounceRomDesc[] = {
	{ "bounce (1987)(methodic solutions)(nl)[run'cas-'].cas",	0x04c96, 0xddbff717, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_bounce, MSX_bounce, msx_msx)
STD_ROM_FN(MSX_bounce)

struct BurnDriver BurnDrvMSX_bounce = {
	"msx_bounce", NULL, "msx_msx", NULL, "1987",
	"Bounce (Euro)\0", NULL, "Methodic Solutions", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_bounceRomInfo, MSX_bounceRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Boulder Dash II - Rockford's Riot (Euro)

static struct BurnRomInfo MSX_bdash2RomDesc[] = {
	{ "boulder dash ii - rockford's riot (1986)(databyte)[run'cas-'].cas",	0x04656, 0xb5bf54a3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_bdash2, MSX_bdash2, msx_msx)
STD_ROM_FN(MSX_bdash2)

struct BurnDriver BurnDrvMSX_bdash2 = {
	"msx_bdash2", NULL, "msx_msx", NULL, "1986",
	"Boulder Dash II - Rockford's Riot (Euro)\0", NULL, "Databyte", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_bdash2RomInfo, MSX_bdash2RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Bop! (Euro)

static struct BurnRomInfo MSX_bopRomDesc[] = {
	{ "bop! (1986)(bug-byte software)(gb)[run'cas-'].cas",	0x0282f, 0xc5463d0c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_bop, MSX_bop, msx_msx)
STD_ROM_FN(MSX_bop)

struct BurnDriver BurnDrvMSX_bop = {
	"msx_bop", NULL, "msx_msx", NULL, "1986",
	"Bop! (Euro)\0", NULL, "Bug-Byte Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_bopRomInfo, MSX_bopRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Booty (Euro)

static struct BurnRomInfo MSX_bootyRomDesc[] = {
	{ "booty (1988)(eurosoft)(nl)[run'cas-'].cas",	0x057e8, 0xc20898f4, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_booty, MSX_booty, msx_msx)
STD_ROM_FN(MSX_booty)

struct BurnDriver BurnDrvMSX_booty = {
	"msx_booty", NULL, "msx_msx", NULL, "1988",
	"Booty (Euro)\0", NULL, "Eurosoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_bootyRomInfo, MSX_bootyRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// BMX Simulator (Euro)

static struct BurnRomInfo MSX_bmxsimRomDesc[] = {
	{ "bmx simulator (1987)(codemasters)(gb)[run'cas-'].cas",	0x0e69f, 0x234a0f79, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_bmxsim, MSX_bmxsim, msx_msx)
STD_ROM_FN(MSX_bmxsim)

struct BurnDriver BurnDrvMSX_bmxsim = {
	"msx_bmxsim", NULL, "msx_msx", NULL, "1987",
	"BMX Simulator (Euro)\0", NULL, "Codemasters", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_bmxsimRomInfo, MSX_bmxsimRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Bloody (Spa)

static struct BurnRomInfo MSX_bloodyRomDesc[] = {
	{ "bloody (1987)(p.j. software)(es)[run'cas-'].cas",	0x05b41, 0xeaa520de, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_bloody, MSX_bloody, msx_msx)
STD_ROM_FN(MSX_bloody)

struct BurnDriver BurnDrvMSX_bloody = {
	"msx_bloody", NULL, "msx_msx", NULL, "1987",
	"Bloody (Spa)\0", NULL, "P.J. Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_bloodyRomInfo, MSX_bloodyRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Bestial Warrior (Spa)

static struct BurnRomInfo MSX_bestwarrRomDesc[] = {
	{ "bestial warrior (1989)(dinamic software)(es)[run'cas-'].cas",	0x100fe, 0x06073712, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_bestwarr, MSX_bestwarr, msx_msx)
STD_ROM_FN(MSX_bestwarr)

struct BurnDriver BurnDrvMSX_bestwarr = {
	"msx_bestwarr", NULL, "msx_msx", NULL, "1989",
	"Bestial Warrior (Spa)\0", NULL, "Dinamic Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_bestwarrRomInfo, MSX_bestwarrRomName, NULL, NULL, MSXInputInfo, MSXJoyport2DIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Barn Stormer (Euro)

static struct BurnRomInfo MSX_barnstrmRomDesc[] = {
	{ "barn stormer (1985)(electric software)(gb)[bload'cas-',r].cas",	0x08198, 0x54cf5202, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_barnstrm, MSX_barnstrm, msx_msx)
STD_ROM_FN(MSX_barnstrm)

struct BurnDriver BurnDrvMSX_barnstrm = {
	"msx_barnstrm", NULL, "msx_msx", NULL, "1985",
	"Barn Stormer (Euro)\0", NULL, "Electric Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_barnstrmRomInfo, MSX_barnstrmRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasBloadDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Barbarian (Euro)

static struct BurnRomInfo MSX_barbRomDesc[] = {
	{ "barbarian (1988)(mastertronic)(es-gb)[run'cas-'].cas",	0x0dfe2, 0xde27937b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_barb, MSX_barb, msx_msx)
STD_ROM_FN(MSX_barb)

struct BurnDriver BurnDrvMSX_barb = {
	"msx_barb", NULL, "msx_msx", NULL, "1988",
	"Barbarian (Euro)\0", NULL, "Mastertronic", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_barbRomInfo, MSX_barbRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};


// Barbarian II - The Dungeon of Drax (Spa)

static struct BurnRomInfo MSX_barb2RomDesc[] = {
	{ "barbarian ii - the dungeon of drax (1988)(erbe software)(es)[run'cas-'].cas",	0x1e162, 0xd557cf7a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_barb2, MSX_barb2, msx_msx)
STD_ROM_FN(MSX_barb2)

struct BurnDriver BurnDrvMSX_barb2 = {
	"msx_barb2", NULL, "msx_msx", NULL, "1988",
	"Barbarian II - The Dungeon of Drax (Spa)\0", NULL, "Erbe Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_barb2RomInfo, MSX_barb2RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Astro Plumber (Euro)

static struct BurnRomInfo MSX_astroplmRomDesc[] = {
	{ "astro plumber (1986)(blue ribbon software)(gb)[run'cas-'][martos].cas",	0x06e97, 0x2e9f6101, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_astroplm, MSX_astroplm, msx_msx)
STD_ROM_FN(MSX_astroplm)

struct BurnDriver BurnDrvMSX_astroplm = {
	"msx_astroplm", NULL, "msx_msx", NULL, "1986",
	"Astro Plumber (Euro)\0", NULL, "Blue Ribbon Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_astroplmRomInfo, MSX_astroplmRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Astro Blaster (Euro)

static struct BurnRomInfo MSX_astroblsRomDesc[] = {
	{ "astro blaster (1988)(eurosoft)(nl)[run'cas-'].cas",	0x055d8, 0x550da760, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_astrobls, MSX_astrobls, msx_msx)
STD_ROM_FN(MSX_astrobls)

struct BurnDriver BurnDrvMSX_astrobls = {
	"msx_astrobls", NULL, "msx_msx", NULL, "1988",
	"Astro Blaster (Euro)\0", NULL, "Eurosoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_astroblsRomInfo, MSX_astroblsRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Amoto's Puf (Spa)

static struct BurnRomInfo MSX_amotopufRomDesc[] = {
	{ "amoto's puf (1988)(spe)(es)[run'cas-'].cas",	0x08248, 0xaa21b91d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_amotopuf, MSX_amotopuf, msx_msx)
STD_ROM_FN(MSX_amotopuf)

struct BurnDriver BurnDrvMSX_amotopuf = {
	"msx_amotopuf", NULL, "msx_msx", NULL, "1988",
	"Amoto's Puf (Spa)\0", NULL, "SPE", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_amotopufRomInfo, MSX_amotopufRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Amo del Mundo (Spa)

static struct BurnRomInfo MSX_amomundoRomDesc[] = {
	{ "amo del mundo (1990)(positive)(es)[run'cas-'][martos].cas",	0x0bd51, 0x8419ee76, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_amomundo, MSX_amomundo, msx_msx)
STD_ROM_FN(MSX_amomundo)

struct BurnDriver BurnDrvMSX_amomundo = {
	"msx_amomundo", NULL, "msx_msx", NULL, "1990",
	"Amo del Mundo (Spa)\0", NULL, "Positive", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_amomundoRomInfo, MSX_amomundoRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Adel (Spa)

static struct BurnRomInfo MSX_adelRomDesc[] = {
	{ "adel (1986)(mind games espana)(es)[run'cas-'].cas",	0x0b0c1, 0x62db7bde, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_adel, MSX_adel, msx_msx)
STD_ROM_FN(MSX_adel)

struct BurnDriver BurnDrvMSX_adel = {
	"msx_adel", NULL, "msx_msx", NULL, "1986",
	"Adel (Spa)\0", NULL, "Mind Games España", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_adelRomInfo, MSX_adelRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Aaargh! (Spa)

static struct BurnRomInfo MSX_aaarghRomDesc[] = {
	{ "aaargh! (1989)(dro soft)(es)[run'cas-'].cas",	0x28364, 0x1cdc6750, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_aaargh, MSX_aaargh, msx_msx)
STD_ROM_FN(MSX_aaargh)

struct BurnDriver BurnDrvMSX_aaargh = {
	"msx_aaargh", NULL, "msx_msx", NULL, "1989",
	"Aaargh! (Spa)\0", NULL, "Dro Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_aaarghRomInfo, MSX_aaarghRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// 007 - The Living Daylights (Euro)

static struct BurnRomInfo MSX_007tldRomDesc[] = {
	{ "007 - the living daylights (1987)(domark)(gb)[run'cas-'].cas",	0x0b7c6, 0x6eeb8d76, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_007tld, MSX_007tld, msx_msx)
STD_ROM_FN(MSX_007tld)

struct BurnDriver BurnDrvMSX_007tld = {
	"msx_007tld", NULL, "msx_msx", NULL, "1987",
	"007 - The Living Daylights (Euro)\0", NULL, "Domark", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_007tldRomInfo, MSX_007tldRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Inspecteur Z

static struct BurnRomInfo MSX_inspecteurzRomDesc[] = {
	{ "Buru to Marty Kikiippatsu. Inspecteur Z (1986)(HAL Laboratory)(JP).rom",	0x08000, 0x57a1bfb3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_inspecteurz, MSX_inspecteurz, msx_msx)
STD_ROM_FN(MSX_inspecteurz)

struct BurnDriver BurnDrvMSX_inspecteurz = {
	"msx_inspecteurz", NULL, "msx_msx", NULL, "1986",
	"Buru to Marty Kikiippatsu. Inspecteur Z\0", NULL, "HAL Laboratory", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_inspecteurzRomInfo, MSX_inspecteurzRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Betiled!

static struct BurnRomInfo MSX_betiledRomDesc[] = {
	{ "BeTiled!.rom",	0x0c000, 0x4f89a784, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_betiled, MSX_betiled, msx_msx)
STD_ROM_FN(MSX_betiled)

struct BurnDriver BurnDrvMSX_betiled = {
	"msx_betiled", NULL, "msx_msx", NULL, "2007",
	"Betiled!\0", NULL, "CEZ GS", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_betiledRomInfo, MSX_betiledRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Lotus F3

static struct BurnRomInfo MSX_lotusf3RomDesc[] = {
	{ "lotusf3.rom",	0x20000, 0x565022a4, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_lotusf3, MSX_lotusf3, msx_msx)
STD_ROM_FN(MSX_lotusf3)

struct BurnDriver BurnDrvMSX_lotusf3 = {
	"msx_lotusf3", NULL, "msx_msx", NULL, "2007",
	"Lotus F3\0", NULL, "dvik & joyrex", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_lotusf3RomInfo, MSX_lotusf3RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Mr. Mole

static struct BurnRomInfo MSX_mrmoleRomDesc[] = {
	{ "NLKMSX003EN_MrMole_KONAMI5.rom",	0x20000, 0xaa6f2968, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mrmole, MSX_mrmole, msx_msx)
STD_ROM_FN(MSX_mrmole)

struct BurnDriver BurnDrvMSX_mrmole = {
	"msx_mrmole", NULL, "msx_msx", NULL, "2007",
	"Mr. Mole\0", NULL, "Nerlaska Studio", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_KONAMI_SCC, GBF_MISC, 0,
	MSXGetZipName, MSX_mrmoleRomInfo, MSX_mrmoleRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Altered Beast

static struct BurnRomInfo MSX_alteredbeastRomDesc[] = {
	{ "alteredbeast.rom",	0x40000, 0xbb5cd0f1, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_alteredbeast, MSX_alteredbeast, msx_msx)
STD_ROM_FN(MSX_alteredbeast)

struct BurnDriver BurnDrvMSX_alteredbeast = {
	"msx_alteredbeast", NULL, "msx_msx", NULL, "1988",
	"Altered Beast\0", "Choose 'B' Cursor from menu.", "Activision / Amusement Factory", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_alteredbeastRomInfo, MSX_alteredbeastRomName, NULL, NULL, MSXInputInfo, MSXJoyCursorDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Bold

static struct BurnRomInfo MSX_boldRomDesc[] = {
	{ "bold.rom",	0x20000, 0x00d02aa4, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_bold, MSX_bold, msx_msx)
STD_ROM_FN(MSX_bold)

struct BurnDriver BurnDrvMSX_bold = {
	"msx_bold", NULL, "msx_msx", NULL, "2009",
	"Bold (demo)\0", NULL, "dvik & joyrex", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_boldRomInfo, MSX_boldRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// MSX Unleashed

static struct BurnRomInfo MSX_unleashedRomDesc[] = {
	{ "unleashed.rom",	0x40000, 0x10f86aa9, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_unleashed, MSX_unleashed, msx_msx)
STD_ROM_FN(MSX_unleashed)

struct BurnDriver BurnDrvMSX_unleashed = {
	"msx_unleashed", NULL, "msx_msx", NULL, "2006",
	"MSX Unleashed (demo)\0", NULL, "dvik & joyrex", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX | HARDWARE_MSX_MAPPER_ASCII8, GBF_MISC, 0,
	MSXGetZipName, MSX_unleashedRomInfo, MSX_unleashedRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Invasion of the big pixels

static struct BurnRomInfo MSX_invasionRomDesc[] = {
	{ "invasion.rom",	0x20000, 0x261c87aa, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_invasion, MSX_invasion, msx_msx)
STD_ROM_FN(MSX_invasion)

struct BurnDriver BurnDrvMSX_invasion = {
	"msx_invasion", NULL, "msx_msx", NULL, "2008",
	"Invasion of the big pixels (demo)\0", NULL, "noice", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_invasionRomInfo, MSX_invasionRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Vesikauhu

static struct BurnRomInfo MSX_vesikauhuRomDesc[] = {
	{ "Vesikauhu.rom",	0x40000, 0x7392b7c7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_vesikauhu, MSX_vesikauhu, msx_msx)
STD_ROM_FN(MSX_vesikauhu)

struct BurnDriver BurnDrvMSX_vesikauhu = {
	"msx_vesikauhu", NULL, "msx_msx", NULL, "2000",
	"Vesikauhu (demo)\0", NULL, "Bandwagon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_vesikauhuRomInfo, MSX_vesikauhuRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Riyadh

static struct BurnRomInfo MSX_riyadhRomDesc[] = {
	{ "riyadh.rom",	0x40000, 0x3e207332, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_riyadh, MSX_riyadh, msx_msx)
STD_ROM_FN(MSX_riyadh)

struct BurnDriver BurnDrvMSX_riyadh = {
	"msx_riyadh", NULL, "msx_msx", NULL, "2001",
	"Riyadh (demo)\0", NULL, "Bandwagon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_riyadhRomInfo, MSX_riyadhRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Montana John

static struct BurnRomInfo MSX_montanajohnRomDesc[] = {
	{ "MJTT.ROM",	0x20000, 0x7b5e126f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_montanajohn, MSX_montanajohn, msx_msx)
STD_ROM_FN(MSX_montanajohn)

struct BurnDriver BurnDrvMSX_montanajohn = {
	"msx_montanajohn", NULL, "msx_msx", NULL, "2008",
	"Montana John and the Templar's Treasure\0", NULL, "Infinite", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_montanajohnRomInfo, MSX_montanajohnRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Atsarias

static struct BurnRomInfo MSX_atsariasRomDesc[] = {
	{ "atsarias.rom",	0x10000, 0xe7e245de, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_atsarias, MSX_atsarias, msx_msx)
STD_ROM_FN(MSX_atsarias)

struct BurnDriver BurnDrvMSX_atsarias = {
	"msx_atsarias", NULL, "msx_msx", NULL, "2013",
	"Atsarias (demo)\0", NULL, "Lieves!Tuore", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_atsariasRomInfo, MSX_atsariasRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Operation Wolf

static struct BurnRomInfo MSX_opwolfRomDesc[] = {
	{ "OWMSX1.ROM",	0x08000, 0xe9492c87, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_opwolf, MSX_opwolf, msx_msx)
STD_ROM_FN(MSX_opwolf)

struct BurnDriver BurnDrvMSX_opwolf = {
	"msx_opwolf", NULL, "msx_msx", NULL, "2009",
	"Operation Wolf\0", NULL, "TOYBOX", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_opwolfRomInfo, MSX_opwolfRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Lorna (Spa)

static struct BurnRomInfo MSX_lornaRomDesc[] = {
	{ "lorna (1990)(topo soft)(es)[run'cas-'].cas",	0x1e3c2, 0x2ae741cc, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_lorna, MSX_lorna, msx_msx)
STD_ROM_FN(MSX_lorna)

struct BurnDriver BurnDrvMSX_lorna = {
	"msx_lorna", NULL, "msx_msx", NULL, "1990",
	"Lorna (Spa)\0", NULL, "Topo Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_lornaRomInfo, MSX_lornaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Binary Land

static struct BurnRomInfo MSX_binarylndRomDesc[] = {
	{ "Binary Land (1984)(Kuma Computers)(GB).rom",	0x04000, 0xfbacbc7a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_binarylnd, MSX_binarylnd, msx_msx)
STD_ROM_FN(MSX_binarylnd)

struct BurnDriver BurnDrvMSX_binarylnd = {
	"msx_binarylnd", NULL, "msx_msx", NULL, "1984",
	"Binary Land\0", NULL, "Kuma Computers", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_binarylndRomInfo, MSX_binarylndRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// WEC Le Mans (Euro)

static struct BurnRomInfo MSX_weclemanRomDesc[] = {
	{ "wec le mans (1988)(imagine software)(gb)[run'cas-'].cas",	0x0cc97, 0xb10ac049, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_wecleman, MSX_wecleman, msx_msx)
STD_ROM_FN(MSX_wecleman)

struct BurnDriver BurnDrvMSX_wecleman = {
	"msx_wecleman", NULL, "msx_msx", NULL, "1988",
	"WEC Le Mans (Euro)\0", NULL, "Imagine Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_weclemanRomInfo, MSX_weclemanRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Nausicaa

static struct BurnRomInfo MSX_nausicaaRomDesc[] = {
	{ "nausicaa (japan).rom",	0x04000, 0xe84994f7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_nausicaa, MSX_nausicaa, msx_msx)
STD_ROM_FN(MSX_nausicaa)

struct BurnDriver BurnDrvMSX_nausicaa = {
	"msx_nausicaa", NULL, "msx_msx", NULL, "1984",
	"Nausicaa\0", NULL, "Technopolis Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_nausicaaRomInfo, MSX_nausicaaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Invaders Revenge

static struct BurnRomInfo MSX_invrevengeRomDesc[] = {
	{ "Invaders Revenge (MMG Software, 19xx).rom",	0x20000, 0x8a899751, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_invrevenge, MSX_invrevenge, msx_msx)
STD_ROM_FN(MSX_invrevenge)

struct BurnDriver BurnDrvMSX_invrevenge = {
	"msx_invrevenge", NULL, "msx_msx", NULL, "19xx",
	"Invaders Revenge\0", NULL, "MMG Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_invrevengeRomInfo, MSX_invrevengeRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Hyper Viper

static struct BurnRomInfo MSX_hyperviperRomDesc[] = {
	{ "Hyper Viper (1985)(Kuma Computers)[cursorkeys].rom",	0x10000, 0xa88ecd5d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_hyperviper, MSX_hyperviper, msx_msx)
STD_ROM_FN(MSX_hyperviper)

struct BurnDriver BurnDrvMSX_hyperviper = {
	"msx_hyperviper", NULL, "msx_msx", NULL, "1985",
	"Hyper Viper\0", NULL, "Kuma Computers", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_hyperviperRomInfo, MSX_hyperviperRomName, NULL, NULL, MSXInputInfo, MSXJoyCursorDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// StratoS (v2)

static struct BurnRomInfo MSX_stratos2RomDesc[] = {
	{ "stratos2.rom",	0x04000, 0xe3093554, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_stratos2, MSX_stratos2, msx_msx)
STD_ROM_FN(MSX_stratos2)

struct BurnDriver BurnDrvMSX_stratos2 = {
	"msx_stratos2", NULL, "msx_msx", NULL, "2005",
	"StratoS (v2)\0", NULL, "CEZ GS", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_stratos2RomInfo, MSX_stratos2RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Addictaball (Euro)

static struct BurnRomInfo MSX_addictblRomDesc[] = {
	{ "addictaball (1988)(alligata software)(gb)[run'cas-'].cas",	0x0a54f, 0xa74a1df6, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_addictbl, MSX_addictbl, msx_msx)
STD_ROM_FN(MSX_addictbl)

struct BurnDriver BurnDrvMSX_addictbl = {
	"msx_addictbl", NULL, "msx_msx", NULL, "1988",
	"Addictaball (Euro)\0", NULL, "Alligata Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_addictblRomInfo, MSX_addictblRomName, NULL, NULL, MSXInputInfo, MSXJoyport2DIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// The Apeman Strikes Again

static struct BurnRomInfo MSX_apemanRomDesc[] = {
	{ "The Apeman Strikes Again (Bytebusters, 1985).rom",	0x10000, 0x395bec07, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_apeman, MSX_apeman, msx_msx)
STD_ROM_FN(MSX_apeman)

struct BurnDriver BurnDrvMSX_apeman = {
	"msx_apeman", NULL, "msx_msx", NULL, "1985",
	"The Apeman Strikes Again\0", NULL, "Bytebusters", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_apemanRomInfo, MSX_apemanRomName, NULL, NULL, MSXInputInfo, MSXEuropeDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Balloon Punch

static struct BurnRomInfo MSX_balloonpunchRomDesc[] = {
	{ "Balloon Punch (1989)(Satico)(JP).rom",	0x10000, 0xe018a918, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_balloonpunch, MSX_balloonpunch, msx_msx)
STD_ROM_FN(MSX_balloonpunch)

struct BurnDriver BurnDrvMSX_balloonpunch = {
	"msx_balloonpunch", NULL, "msx_msx", NULL, "1989",
	"Balloon Punch\0", NULL, "Satico", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_balloonpunchRomInfo, MSX_balloonpunchRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Chubby Gristle

static struct BurnRomInfo MSX_chubbygristleRomDesc[] = {
	{ "Chubby Gristle (1988)(Grandslam Entertainments)(GB).rom",	0x20000, 0x28f873f8, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_chubbygristle, MSX_chubbygristle, msx_msx)
STD_ROM_FN(MSX_chubbygristle)

struct BurnDriver BurnDrvMSX_chubbygristle = {
	"msx_chubbygristle", NULL, "msx_msx", NULL, "1988",
	"Chubby Gristle\0", NULL, "Grandslam Entertainments", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_chubbygristleRomInfo, MSX_chubbygristleRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Attack Mosquiton

static struct BurnRomInfo MSX_mosquitonRomDesc[] = {
	{ "Attack_Mosquiton.cas",	0x33b6, 0xe759ac71, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_mosquiton, MSX_mosquiton, msx_msx)
STD_ROM_FN(MSX_mosquiton)

struct BurnDriver BurnDrvMSX_mosquiton = {
	"msx_mosquiton", NULL, "msx_msx", NULL, "1986",
	"Attack Mosquiton\0", NULL, "Shouji Ogawa", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_mosquitonRomInfo, MSX_mosquitonRomName, NULL, NULL, MSXInputInfo, MSXJoyCursor60hzDIPInfo,
	CasCloadDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// The Ghost

static struct BurnRomInfo MSX_theghostRomDesc[] = {
	{ "THE_GHOST.CAS",	0x02726, 0x2db14915, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_theghost, MSX_theghost, msx_msx)
STD_ROM_FN(MSX_theghost)

struct BurnDriver BurnDrvMSX_theghost = {
	"msx_theghost", NULL, "msx_msx", NULL, "1987",
	"The Ghost\0", NULL, "Teruhiko Matsubara", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_theghostRomInfo, MSX_theghostRomName, NULL, NULL, MSXInputInfo, MSXJoyCursor60hzDIPInfo,
	CasCloadDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Alpha Blaster

static struct BurnRomInfo MSX_alphablasterRomDesc[] = {
	{ "Alpha Blaster (1984)(Aackosoft)(NL).rom",	0x10000, 0xf59ce3f6, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_alphablaster, MSX_alphablaster, msx_msx)
STD_ROM_FN(MSX_alphablaster)

struct BurnDriver BurnDrvMSX_alphablaster = {
	"msx_alphablaster", NULL, "msx_msx", NULL, "1984",
	"Alpha Blaster\0", NULL, "Aackosoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_alphablasterRomInfo, MSX_alphablasterRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Alpine Ski

static struct BurnRomInfo MSX_alpineskiRomDesc[] = {
	{ "Alpine Ski (1987)(Methodic Solutions)(NL).rom",	0x10000, 0x32662af5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_alpineski, MSX_alpineski, msx_msx)
STD_ROM_FN(MSX_alpineski)

struct BurnDriver BurnDrvMSX_alpineski = {
	"msx_alpineski", NULL, "msx_msx", NULL, "1987",
	"Alpine Ski\0", NULL, "Methodic Solutions", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_alpineskiRomInfo, MSX_alpineskiRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Chick Fighter. Hiyoko Fighter

static struct BurnRomInfo MSX_chickfighterRomDesc[] = {
	{ "Chick Fighter. Hiyoko Fighter (1983)(Honeybee Soft)(JP).rom",	0x10000, 0xd8e1aadb, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_chickfighter, MSX_chickfighter, msx_msx)
STD_ROM_FN(MSX_chickfighter)

struct BurnDriver BurnDrvMSX_chickfighter = {
	"msx_chickfighter", NULL, "msx_msx", NULL, "1983",
	"Chick Fighter. Hiyoko Fighter\0", NULL, "Honeybee Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_chickfighterRomInfo, MSX_chickfighterRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Eat It

static struct BurnRomInfo MSX_eatitRomDesc[] = {
	{ "Eat It (1986)(Aackosoft)(NL).rom",	0x10000, 0x54851e18, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_eatit, MSX_eatit, msx_msx)
STD_ROM_FN(MSX_eatit)

struct BurnDriver BurnDrvMSX_eatit = {
	"msx_eatit", NULL, "msx_msx", NULL, "1986",
	"Eat It\0", NULL, "Aackosoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_eatitRomInfo, MSX_eatitRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Galaxia

static struct BurnRomInfo MSX_galaxiaRomDesc[] = {
	{ "Galaxia (1985)(Kuma Computers)(GB).rom",	0x10000, 0x180a2b4e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_galaxia, MSX_galaxia, msx_msx)
STD_ROM_FN(MSX_galaxia)

struct BurnDriver BurnDrvMSX_galaxia = {
	"msx_galaxia", NULL, "msx_msx", NULL, "1985",
	"Galaxia\0", NULL, "Kuma Computers", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_galaxiaRomInfo, MSX_galaxiaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Infernal Miner

static struct BurnRomInfo MSX_infernalminerRomDesc[] = {
	{ "Infernal Miner (1985)(Sprites).rom",	0x10000, 0xa4f88c9e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_infernalminer, MSX_infernalminer, msx_msx)
STD_ROM_FN(MSX_infernalminer)

struct BurnDriver BurnDrvMSX_infernalminer = {
	"msx_infernalminer", NULL, "msx_msx", NULL, "1985",
	"Infernal Miner\0", NULL, "Sprites", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_infernalminerRomInfo, MSX_infernalminerRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Space Mouse

static struct BurnRomInfo MSX_spacemouseRomDesc[] = {
	{ "Space Mouse (H. Kobo, 201x).rom",	0x20000, 0xb55702f7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_spacemouse, MSX_spacemouse, msx_msx)
STD_ROM_FN(MSX_spacemouse)

struct BurnDriver BurnDrvMSX_spacemouse = {
	"msx_spacemouse", NULL, "msx_msx", NULL, "201x",
	"Space Mouse\0", NULL, "H. Kobo", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_spacemouseRomInfo, MSX_spacemouseRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Search for Mum

static struct BurnRomInfo MSX_searchformumRomDesc[] = {
	{ "Search for Mum (1991)(Triple Soft)[NL].rom",	0x20000, 0xa5ff786f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_searchformum, MSX_searchformum, msx_msx)
STD_ROM_FN(MSX_searchformum)

struct BurnDriver BurnDrvMSX_searchformum = {
	"msx_searchformum", NULL, "msx_msx", NULL, "1991",
	"Search for Mum\0", NULL, "Triple Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_searchformumRomInfo, MSX_searchformumRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Tuma 7

static struct BurnRomInfo MSX_tuma7RomDesc[] = {
	{ "Tuma 7 (1990)(Delta Software)(ES).rom",	0x20000, 0x61cb184d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_tuma7, MSX_tuma7, msx_msx)
STD_ROM_FN(MSX_tuma7)

struct BurnDriver BurnDrvMSX_tuma7 = {
	"msx_tuma7", NULL, "msx_msx", NULL, "1990",
	"Tuma 7\0", NULL, "Delta Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_tuma7RomInfo, MSX_tuma7RomName, NULL, NULL, MSXInputInfo, MSXJoyport2DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Perez the Mouse

static struct BurnRomInfo MSX_perezthemouseRomDesc[] = {
	{ "Perez the Mouse 1.1 (2011) (theNestruo Wonder).cas",	0x05983, 0xcc8d9d62, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_perezthemouse, MSX_perezthemouse, msx_msx)
STD_ROM_FN(MSX_perezthemouse)

struct BurnDriver BurnDrvMSX_perezthemouse = {
	"msx_perezthemouse", NULL, "msx_msx", NULL, "2011",
	"Perez the Mouse\0", NULL, "theNestruo Wonder", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_perezthemouseRomInfo, MSX_perezthemouseRomName, NULL, NULL, MSXInputInfo, MSXJoyCursor60hzDIPInfo,
	CasCloadRRDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// La Abadia del Crimen (Spa)

static struct BurnRomInfo MSX_abadcrimRomDesc[] = {
	{ "abadia del crimen, la (1988)(opera soft)(es)[bload'cas-',r].cas",	0x195a0, 0x812afd46, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_abadcrim, MSX_abadcrim, msx_msx)
STD_ROM_FN(MSX_abadcrim)

struct BurnDriver BurnDrvMSX_abadcrim = {
	"msx_abadcrim", NULL, "msx_msx", NULL, "1988",
	"La Abadia del Crimen (Spa)\0", NULL, "Opera Soft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_abadcrimRomInfo, MSX_abadcrimRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasBloadDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Starquake (Euro)

static struct BurnRomInfo MSX_starquakRomDesc[] = {
	{ "starquake (1986)(bubble bus software)(gb)[run'cas-'].cas",	0x0c5c6, 0x2ae2f857, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_starquak, MSX_starquak, msx_msx)
STD_ROM_FN(MSX_starquak)

struct BurnDriver BurnDrvMSX_starquak = {
	"msx_starquak", NULL, "msx_msx", NULL, "1986",
	"Starquake (Euro)\0", NULL, "Bubble Bus Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_starquakRomInfo, MSX_starquakRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Batman - The Movie (Euro)

static struct BurnRomInfo MSX_batmanmvRomDesc[] = {
	{ "batman - the movie (1989)(ocean software)(gb)(side a)[run'cas-'].cas",	0x1efb5, 0xb51ff961, BRF_PRG | BRF_ESS },
	{ "batman - the movie (1989)(ocean software)(gb)(side b)[run'cas-'].cas",	0x195dd, 0x9bf4001d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_batmanmv, MSX_batmanmv, msx_msx)
STD_ROM_FN(MSX_batmanmv)

struct BurnDriver BurnDrvMSX_batmanmv = {
	"msx_batmanmv", NULL, "msx_msx", NULL, "1989",
	"Batman - The Movie (Euro)\0", NULL, "Ocean Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_batmanmvRomInfo, MSX_batmanmvRomName, NULL, NULL, MSXInputInfo, MSXKeyClickDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// International Karate (Euro)

static struct BurnRomInfo MSX_ikRomDesc[] = {
	{ "international karate (1986)(endurance games)(gb)[run'cas-'].cas",	0x0ca03, 0xd7eb1d87, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_ik, MSX_ik, msx_msx)
STD_ROM_FN(MSX_ik)

struct BurnDriver BurnDrvMSX_ik = {
	"msx_ik", NULL, "msx_msx", NULL, "1986",
	"International Karate (Euro)\0", NULL, "Endurance Games", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_ikRomInfo, MSX_ikRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Quebert (Euro)

static struct BurnRomInfo MSX_quebertRomDesc[] = {
	{ "quebert (1988)(eurosoft)(nl)[run'cas-'].cas",	0x08998, 0xf7930f38, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_quebert, MSX_quebert, msx_msx)
STD_ROM_FN(MSX_quebert)

struct BurnDriver BurnDrvMSX_quebert = {
	"msx_quebert", NULL, "msx_msx", NULL, "1988",
	"Quebert (Euro)\0", NULL, "Eurosoft", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_quebertRomInfo, MSX_quebertRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Renegade III - The Final Chapter (Euro)

static struct BurnRomInfo MSX_renegad3RomDesc[] = {
	{ "renegade iii - the final chapter (1989)(imagine software)(gb)[run'cas-'].cas",	0x184e2, 0x9fc765b8, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_renegad3, MSX_renegad3, msx_msx)
STD_ROM_FN(MSX_renegad3)

struct BurnDriver BurnDrvMSX_renegad3 = {
	"msx_renegad3", NULL, "msx_msx", NULL, "1989",
	"Renegade III - The Final Chapter (Euro)\0", NULL, "Imagine Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_renegad3RomInfo, MSX_renegad3RomName, NULL, NULL, MSXInputInfo, MSXKeyClickDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Satan (Euro)

static struct BurnRomInfo MSX_satanRomDesc[] = {
	{ "satan (1989)(dinamic software)(es)(en)(side a)[english edition][run'cas-'].cas",	0x0da31, 0x6c16e392, BRF_PRG | BRF_ESS },
	{ "satan (1989)(dinamic software)(es)(en)(side b)[english edition][run'cas-'].cas",	0x0da31, 0x10914f8a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_satan, MSX_satan, msx_msx)
STD_ROM_FN(MSX_satan)

struct BurnDriver BurnDrvMSX_satan = {
	"msx_satan", NULL, "msx_msx", NULL, "1989",
	"Satan (Euro)\0", NULL, "Dinamic Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_satanRomInfo, MSX_satanRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Speedboat Racer (Euro)

static struct BurnRomInfo MSX_sbracerRomDesc[] = {
	{ "speedboat racer (1987)(methodic solutions)(nl)[run'cas-'].cas",	0x0c8d8, 0xa90435f9, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_sbracer, MSX_sbracer, msx_msx)
STD_ROM_FN(MSX_sbracer)

struct BurnDriver BurnDrvMSX_sbracer = {
	"msx_sbracer", NULL, "msx_msx", NULL, "1987",
	"Speedboat Racer (Euro)\0", NULL, "Methodic Solutions", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_sbracerRomInfo, MSX_sbracerRomName, NULL, NULL, MSXInputInfo, MSXJoyCursor60hzDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Thunderbirds (Euro)

static struct BurnRomInfo MSX_tbirds1aRomDesc[] = {
	{ "thunderbirds (1989)(grandslam entertainments)(gb)(tape 1 of 2 side a)[bload'cas-',r][martos].cas",	0x11b71, 0x04456b16, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_tbirds1a, MSX_tbirds1a, msx_msx)
STD_ROM_FN(MSX_tbirds1a)

struct BurnDriver BurnDrvMSX_tbirds1a = {
	"msx_tbirds1a", NULL, "msx_msx", NULL, "1989",
	"Thunderbirds Tape 1 Side A (Euro)\0", NULL, "Grandslam Entertainments", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_tbirds1aRomInfo, MSX_tbirds1aRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasBloadDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Thunderbirds (Euro)

static struct BurnRomInfo MSX_tbirds1bRomDesc[] = {
	{ "thunderbirds (1989)(grandslam entertainments)(gb)(tape 1 of 2 side b)[bload'cas-',r][martos].cas",	0x11b71, 0xc4b0130c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_tbirds1b, MSX_tbirds1b, msx_msx)
STD_ROM_FN(MSX_tbirds1b)

struct BurnDriver BurnDrvMSX_tbirds1b = {
	"msx_tbirdst1b", "msx_tbirds1a", "msx_msx", NULL, "1989",
	"Thunderbirds Tape 1 Side B (Euro)\0", NULL, "Grandslam Entertainments", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_tbirds1bRomInfo, MSX_tbirds1bRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasBloadDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Thunderbirds (Euro)

static struct BurnRomInfo MSX_tbirds2aRomDesc[] = {
	{ "thunderbirds (1989)(grandslam entertainments)(gb)(tape 2 of 2 side a)[bload'cas-',r][martos].cas",	0x11b71, 0xc986703f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_tbirds2a, MSX_tbirds2a, msx_msx)
STD_ROM_FN(MSX_tbirds2a)

struct BurnDriver BurnDrvMSX_tbirds2a = {
	"msx_tbirdst2a", "msx_tbirds1a", "msx_msx", NULL, "1989",
	"Thunderbirds Tape 2 Side A (Euro)\0", NULL, "Grandslam Entertainments", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_tbirds2aRomInfo, MSX_tbirds2aRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasBloadDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Thunderbirds (Euro)

static struct BurnRomInfo MSX_tbirds2bRomDesc[] = {
	{ "thunderbirds (1989)(grandslam entertainments)(gb)(tape 2 of 2 side b)[bload'cas-',r][martos].cas",	0x11b71, 0x1bcb9d6f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_tbirds2b, MSX_tbirds2b, msx_msx)
STD_ROM_FN(MSX_tbirds2b)

struct BurnDriver BurnDrvMSX_tbirds2b = {
	"msx_tbirdst2b", "msx_tbirds1a", "msx_msx", NULL, "1989",
	"Thunderbirds Tape 2 Side B (Euro)\0", NULL, "Grandslam Entertainments", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_tbirds2bRomInfo, MSX_tbirds2bRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasBloadDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Game Over II (Spa)

static struct BurnRomInfo MSX_gameovr2RomDesc[] = {
	{ "game over ii (1988)(dinamic software)(es)(side a)[re-release of phantis][run'cas-'].cas",	0x0c3c3, 0x12de1170, BRF_PRG | BRF_ESS },
	{ "game over ii (1988)(dinamic software)(es)(side b)[re-release of phantis][run'cas-'].cas",	0x0bffd, 0x994cd2a2, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_gameovr2, MSX_gameovr2, msx_msx)
STD_ROM_FN(MSX_gameovr2)

struct BurnDriver BurnDrvMSX_gameovr2 = {
	"msx_gameovr2", NULL, "msx_msx", NULL, "1988",
	"Game Over II (Spa)\0", NULL, "Dinamic Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_gameovr2RomInfo, MSX_gameovr2RomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Sea King (Euro)

static struct BurnRomInfo MSX_seakingRomDesc[] = {
	{ "sea king (1986)(players software)(gb)[run'cas-'].cas",	0x082ff, 0xb29571ed, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_seaking, MSX_seaking, msx_msx)
STD_ROM_FN(MSX_seaking)

struct BurnDriver BurnDrvMSX_seaking = {
	"msx_seaking", NULL, "msx_msx", NULL, "1986",
	"Sea King (Euro)\0", NULL, "Players Software", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_seakingRomInfo, MSX_seakingRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	CasRunDrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Maxima (Euro)

static struct BurnRomInfo MSX_maximaRomDesc[] = {
	{ "Maxima (1984)(PSS)(GB).rom",	0x10000, 0xc9c5b360, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_maxima, MSX_maxima, msx_msx)
STD_ROM_FN(MSX_maxima)

struct BurnDriver BurnDrvMSX_maxima = {
	"msx_maxima", NULL, "msx_msx", NULL, "1984",
	"Maxima (Euro)\0", NULL, "PSS", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_maximaRomInfo, MSX_maximaRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

// Transball (v1.3.2)

static struct BurnRomInfo MSX_transballRomDesc[] = {
	{ "transball-en.rom",	0x08000, 0xcbcf03c5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(MSX_transball, MSX_transball, msx_msx)
STD_ROM_FN(MSX_transball)

struct BurnDriver BurnDrvMSX_transball = {
	"msx_transball", NULL, "msx_msx", NULL, "2005",
	"Transball (v1.3.2)\0", NULL, "Santiago Ontanon", "MSX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MSX, GBF_MISC, 0,
	MSXGetZipName, MSX_transballRomInfo, MSX_transballRomName, NULL, NULL, MSXInputInfo, MSXDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, 0x10,
	272, 228, 4, 3
};

