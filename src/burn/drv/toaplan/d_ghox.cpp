// Based on MAME driver by Quench, Yochizo, David Haywood

#include "toaplan.h"
#include "z180_intf.h"

#define REFRESHRATE 60
#define VBLANK_LINES (32)

static UINT8 DrvButton[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvJoy1[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvJoy2[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvInput[6] = {0, 0, 0, 0, 0, 0};

static UINT8 DrvReset = 0;
static UINT8 bDrawScreen;
static bool bVBlank;

static UINT8 *Mem = NULL, *MemEnd = NULL;
static UINT8 *RamStart, *RamEnd;
static UINT8 *Rom01;
static UINT8 *Ram01, *RamPal;
static UINT8 *ShareRAM;
static UINT8 *Rom02;
static UINT8 *Ram02;

static INT8 Paddle[2];
static INT8 PaddleOld[2];

static INT32 nColCount = 0x0800;

static struct BurnInputInfo GhoxInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvButton + 3,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvButton + 5,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"},

	{"P2 Coin",		BIT_DIGITAL,	DrvButton + 4,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvButton + 6,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvButton + 0,	"service"},
	{"Tilt",		BIT_DIGITAL,	DrvButton + 1,	"tilt"},
	{"Dip A",		BIT_DIPSWITCH,	DrvInput + 3,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvInput + 4,	"dip"},
	{"Dip C",		BIT_DIPSWITCH,	DrvInput + 5,	"dip"},
};

STDINPUTINFO(Ghox)

static struct BurnDIPInfo GhoxDIPList[]=
{
	{0x13, 0xff, 0xff, 0x00, NULL		},
	{0x14, 0xff, 0xff, 0x00, NULL		},
	{0x15, 0xff, 0xff, 0xf2, NULL		},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x13, 0x01, 0x01, 0x00, "Off"		},
	{0x13, 0x01, 0x01, 0x01, "On"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x02, 0x00, "Off"		},
	{0x13, 0x01, 0x02, 0x02, "On"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x13, 0x01, 0x04, 0x00, "Off"		},
	{0x13, 0x01, 0x04, 0x04, "On"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x08, 0x08, "Off"		},
	{0x13, 0x01, 0x08, 0x00, "On"		},

	{0   , 0xfe, 0   ,    7, "Coin A"		},
	{0x13, 0x01, 0x30, 0x30, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x30, 0x20, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x30, 0x10, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x30, 0x20, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x30, 0x00, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x30, 0x30, "2 Coins 3 Credits"		},
	{0x13, 0x01, 0x30, 0x10, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x13, 0x01, 0xc0, 0x80, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0xc0, 0x00, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0xc0, 0xc0, "2 Coins 3 Credits"		},
	{0x13, 0x01, 0xc0, 0x40, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0xc0, 0x00, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0xc0, 0x40, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0xc0, 0x80, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0xc0, 0xc0, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x03, 0x03, "Hardest"		},
	{0x14, 0x01, 0x03, 0x02, "Hard"		},
	{0x14, 0x01, 0x03, 0x00, "Normal"		},
	{0x14, 0x01, 0x03, 0x01, "Easy"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x14, 0x01, 0x0c, 0x0c, "None"		},
	{0x14, 0x01, 0x0c, 0x08, "100k only"		},
	{0x14, 0x01, 0x0c, 0x04, "100k and 300k"		},
	{0x14, 0x01, 0x0c, 0x00, "100k and every 200k"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0x30, 0x30, "1"		},
	{0x14, 0x01, 0x30, 0x20, "2"		},
	{0x14, 0x01, 0x30, 0x00, "3"		},
	{0x14, 0x01, 0x30, 0x10, "5"		},

	{0   , 0xfe, 0   ,    2, "Invulnerability"		},
	{0x14, 0x01, 0x40, 0x00, "Off"		},
	{0x14, 0x01, 0x40, 0x40, "On"		},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x14, 0x01, 0x80, 0x00, "Off"		},
	{0x14, 0x01, 0x80, 0x80, "On"		},

	{0   , 0xfe, 0   ,   16, "Region"		},
	{0x15, 0x01, 0x0f, 0x02, "Europe"		},
	{0x15, 0x01, 0x0f, 0x0a, "Europe (Nova Apparate GMBH & Co)"		},
	{0x15, 0x01, 0x0f, 0x0d, "Europe (Taito Corporation Japan)"		},
	{0x15, 0x01, 0x0f, 0x01, "USA"		},
	{0x15, 0x01, 0x0f, 0x09, "USA (Romstar)"		},
	{0x15, 0x01, 0x0f, 0x0b, "USA (Taito America Corporation)"		},
	{0x15, 0x01, 0x0f, 0x0c, "USA (Taito Corporation Japan)"		},
	{0x15, 0x01, 0x0f, 0x00, "Japan"		},
	{0x15, 0x01, 0x0f, 0x0e, "Japan (Licensed to [blank]"		},
	{0x15, 0x01, 0x0f, 0x0f, "Japan (Taito Corporation)"		},
	{0x15, 0x01, 0x0f, 0x04, "Korea"		},
	{0x15, 0x01, 0x0f, 0x03, "Hong Kong (Honest Trading Co.)"		},
	{0x15, 0x01, 0x0f, 0x05, "Taiwan"		},
	{0x15, 0x01, 0x0f, 0x06, "Spain & Portugal (APM Electronics SA)"		},
	{0x15, 0x01, 0x0f, 0x07, "Italy (Star Electronica SRL)"		},
	{0x15, 0x01, 0x0f, 0x08, "UK (JP Leisure Limited)"		},
};

STDDIPINFO(Ghox)

static struct BurnDIPInfo GhoxjoDIPList[]=
{
	{0x13, 0xff, 0xff, 0x00, NULL		},
	{0x14, 0xff, 0xff, 0x00, NULL		},
	{0x15, 0xff, 0xff, 0xf2, NULL		},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x13, 0x01, 0x01, 0x00, "Off"		},
	{0x13, 0x01, 0x01, 0x01, "On"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x02, 0x00, "Off"		},
	{0x13, 0x01, 0x02, 0x02, "On"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x13, 0x01, 0x04, 0x00, "Off"		},
	{0x13, 0x01, 0x04, 0x04, "On"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x08, 0x08, "Off"		},
	{0x13, 0x01, 0x08, 0x00, "On"		},

	{0   , 0xfe, 0   ,    7, "Coin A"		},
	{0x13, 0x01, 0x30, 0x30, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x30, 0x20, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x30, 0x10, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x30, 0x20, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x30, 0x00, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x30, 0x30, "2 Coins 3 Credits"		},
	{0x13, 0x01, 0x30, 0x10, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x13, 0x01, 0xc0, 0x80, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0xc0, 0x00, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0xc0, 0xc0, "2 Coins 3 Credits"		},
	{0x13, 0x01, 0xc0, 0x40, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0xc0, 0x00, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0xc0, 0x40, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0xc0, 0x80, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0xc0, 0xc0, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x03, 0x03, "Hardest"		},
	{0x14, 0x01, 0x03, 0x02, "Hard"		},
	{0x14, 0x01, 0x03, 0x00, "Normal"		},
	{0x14, 0x01, 0x03, 0x01, "Easy"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x14, 0x01, 0x0c, 0x0c, "None"		},
	{0x14, 0x01, 0x0c, 0x08, "100k only"		},
	{0x14, 0x01, 0x0c, 0x04, "100k and 300k"		},
	{0x14, 0x01, 0x0c, 0x00, "100k and every 200k"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0x30, 0x30, "1"		},
	{0x14, 0x01, 0x30, 0x20, "2"		},
	{0x14, 0x01, 0x30, 0x00, "3"		},
	{0x14, 0x01, 0x30, 0x10, "5"		},

	{0   , 0xfe, 0   ,    2, "Invulnerability"		},
	{0x14, 0x01, 0x40, 0x00, "Off"		},
	{0x14, 0x01, 0x40, 0x40, "On"		},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x14, 0x01, 0x80, 0x00, "Off"		},
	{0x14, 0x01, 0x80, 0x80, "On"		},

	{0   , 0xfe, 0   ,   16, "Region"		},
	{0x15, 0x01, 0x0f, 0x02, "Europe"		},
	{0x15, 0x01, 0x0f, 0x0a, "Europe (Nova Apparate GMBH & Co)"		},
	{0x15, 0x01, 0x0f, 0x0d, "Japan (Unused) [d]"		},
	{0x15, 0x01, 0x0f, 0x01, "USA"		},
	{0x15, 0x01, 0x0f, 0x09, "USA (Romstar)"		},
	{0x15, 0x01, 0x0f, 0x0b, "Japan (Unused) [b]"		},
	{0x15, 0x01, 0x0f, 0x0c, "Japan (Unused) [c]"		},
	{0x15, 0x01, 0x0f, 0x00, "Japan"		},
	{0x15, 0x01, 0x0f, 0x0e, "Japan (Unused) [e]"		},
	{0x15, 0x01, 0x0f, 0x0f, "Japan (Unused) [f]"		},
	{0x15, 0x01, 0x0f, 0x04, "Korea"		},
	{0x15, 0x01, 0x0f, 0x03, "Hong Kong (Honest Trading Co.)"		},
	{0x15, 0x01, 0x0f, 0x05, "Taiwan"		},
	{0x15, 0x01, 0x0f, 0x06, "Spain & Portugal (APM Electronics SA)"		},
	{0x15, 0x01, 0x0f, 0x07, "Italy (Star Electronica SRL)"		},
	{0x15, 0x01, 0x0f, 0x08, "UK (JP Leisure Limited)"		},
};

STDDIPINFO(Ghoxjo)

// This routine is called first to determine how much memory is needed (MemEnd-(UINT8 *)0),
// and then afterwards to set up all the pointers
static INT32 MemIndex()
{
	UINT8 *Next; Next = Mem;
	Rom01		= Next; Next += 0x040000;		// 68000 ROM
	GP9001ROM[0]= Next; Next += nGP9001ROMSize[0];	// GP9001 tile data
	Rom02		= Next; Next += 0x008000;
	RamStart	= Next;
	Ram01		= Next; Next += 0x004000;		// CPU #0 work RAM
	ShareRAM	= Next; Next += 0x001000;
	Ram02		= Next;	Next += 0x000400;
	RamPal		= Next; Next += 0x001000;		// palette
	GP9001RAM[0]= Next; Next += 0x008000;		// Double size, as the game tests too much memory during POST
	GP9001Reg[0]= (UINT16*)Next; Next += 0x0100 * sizeof(UINT16);
	RamEnd		= Next;
	ToaPalette	= (UINT32 *)Next; Next += nColCount * sizeof(UINT32);
	MemEnd		= Next;

	return 0;
}

// Scan ram
static INT32 DrvScan(INT32 nAction,INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {						// Return minimum compatible version
		*pnMin = 0x020997;
	}

	if (nAction & ACB_VOLATILE) {		// Scan volatile ram
		memset(&ba, 0, sizeof(ba));
		ba.Data		= RamStart;
		ba.nLen		= RamEnd-RamStart;
		ba.szName	= "All Ram";
		BurnAcb(&ba);

		SekScan(nAction);				// scan 68000 states

		Z180Scan(nAction);

		ToaScanGP9001(nAction, pnMin);

		bDrawScreen = true; // get background back ?
	}

	return 0;
}

static INT32 LoadRoms()
{
	// Load 68000 ROM
	if (ToaLoadCode(Rom01, 0, 2)) return 1;

	// Load GP9001 tile data
	ToaLoadGP9001Tiles(GP9001ROM[0], 2, 2, nGP9001ROMSize[0]);
	
	if (BurnLoadRom(Rom02, 4, 1)) return 1;

	return 0;
}

UINT8 PaddleRead(UINT8 Num)
{
	INT8 Value;
	
	if (Paddle[Num] == PaddleOld[Num]) return 0;
	
	Value = Paddle[Num] - PaddleOld[Num];
	PaddleOld[Num] = Paddle[Num];
	return Value;	
}

UINT8 __fastcall ghoxReadByte(UINT32 sekAddress)
{
	switch (sekAddress) {
		case 0x18100d:								// Dipswitch 3 - Territory
			return DrvInput[5]&0x0f;

		case 0x14000D:								// VBlank
			return ToaVBlankRegister();

		case 0x040000:
		case 0x040001:
			return PaddleRead(1);

		case 0x100000:
		case 0x100001:
			return PaddleRead(0);
	}

	if (sekAddress >= 0x180000 && sekAddress <= 0x180fff) {
		return ShareRAM[(sekAddress - 0x180000) >> 1];
	}
	
//	bprintf(PRINT_NORMAL, _T("Read Byte %x\n"), sekAddress);

	return 0;
}

UINT16 __fastcall ghoxReadWord(UINT32 sekAddress)
{
	switch (sekAddress) {
		case 0x140004:
			return ToaGP9001ReadRAM_Hi(0);
		case 0x140006:
			return ToaGP9001ReadRAM_Lo(0);

		case 0x14000C:
			return ToaVBlankRegister();

		case 0x040000:
			return PaddleRead(1);

		case 0x100000:
			return PaddleRead(0);
	}

	if (sekAddress >= 0x180000 && sekAddress <= 0x180fff) {
		SEK_DEF_READ_WORD(0, sekAddress);
	}

//	bprintf(PRINT_NORMAL, _T("Read Word %x\n"), sekAddress);

	return 0;
}

void __fastcall ghoxWriteByte(UINT32 sekAddress, UINT8 byteValue)
{
	switch (sekAddress) {
		case 0x181001: {
			// coin counter
			return;
		}
		
		case 0x1c0001: {
			// ????
			return;
		}
	}
	
	if (sekAddress >= 0x180000 && sekAddress <= 0x180fff) {
		if ((sekAddress & 0x01) == 0x01) {
			ShareRAM[(sekAddress - 0x180000) >> 1] = byteValue;
		}
		return;
	}
	
//	bprintf(PRINT_NORMAL, _T("Write Byte %x, %x\n"), sekAddress, byteValue);
}

void __fastcall ghoxWriteWord(UINT32 sekAddress, UINT16 wordValue)
{
	switch (sekAddress) {
		case 0x140000:								// Set GP9001 VRAM address-pointer
			ToaGP9001SetRAMPointer(wordValue);
			return;

		case 0x140004:
			ToaGP9001WriteRAM(wordValue, 0);
			return;
		case 0x140006:
			ToaGP9001WriteRAM(wordValue, 0);
			return;

		case 0x140008:
			ToaGP9001SelectRegister(wordValue);
			return;

		case 0x14000C:
			ToaGP9001WriteRegister(wordValue);
			return;
	}

	if (sekAddress >= 0x180000 && sekAddress <= 0x180fff) {
		SEK_DEF_WRITE_WORD(0, sekAddress, wordValue)
		return;
	}
}

static UINT8 __fastcall GhoxMCURead(UINT32 a)
{
	switch (a) {
		case 0x80002: {
			return DrvInput[3];
		}
		
		case 0x80004: {
			return DrvInput[4];
		}
		
		case 0x80006: {
			return 0x00;
		}
		
		case 0x80008: {
			return DrvInput[0];
		}
		
		case 0x8000a: {
			return DrvInput[1];
		}
		
		case 0x8000c: {
			return DrvInput[2];
		}
		
		case 0x8000f: {
			return BurnYM2151Read();
		}
	}

//	bprintf(PRINT_NORMAL, _T("Read Prog %x\n"), a);
	
	return 0;
}

static void __fastcall GhoxMCUWrite(UINT32 a, UINT8 d)
{
	switch (a) {
		case 0x8000e: {
			BurnYM2151SelectRegister(d);
			return;
		}
		
		case 0x8000f: {
			BurnYM2151WriteRegister(d);
			return;
		}
	}
	
//	bprintf(PRINT_NORMAL, _T("Write Prog %x, %x\n"), a, d);
}

static INT32 DrvDoReset()
{
	SekOpen(0);
	SekReset();
	SekClose();

	Z180Open(0);
	Z180Reset();
	Z180Close();
	
	BurnYM2151Reset();

	Paddle[0] = 0;
	PaddleOld[0] = 0;
	Paddle[1] = 0;
	PaddleOld[1] = 0;

	HiscoreReset();

	return 0;
}

static INT32 DrvInit()
{
	INT32 nLen;

#ifdef DRIVER_ROTATION
	bToaRotateScreen = false;
#endif

	BurnSetRefreshRate(REFRESHRATE);

	nGP9001ROMSize[0] = 0x100000;

	// Find out how much memory is needed
	Mem = NULL;
	MemIndex();
	nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) {
		return 1;
	}
	memset(Mem, 0, nLen);										// blank all memory
	MemIndex();													// Index the allocated memory

	// Load the roms into memory
	if (LoadRoms()) {
		return 1;
	}

	{
		SekInit(0, 0x68000);										// Allocate 68000
		SekOpen(0);
		SekMapMemory(Rom01,		0x000000, 0x03FFFF, MAP_ROM);	// CPU 0 ROM
		SekMapMemory(Ram01,		0x080000, 0x083FFF, MAP_RAM);
		SekMapMemory(RamPal,		0x0c0000, 0x0c0FFF, MAP_RAM);	// Palette RAM
		SekSetReadWordHandler(0, 	ghoxReadWord);
		SekSetReadByteHandler(0, 	ghoxReadByte);
		SekSetWriteWordHandler(0, 	ghoxWriteWord);
		SekSetWriteByteHandler(0, 	ghoxWriteByte);
		SekClose();
	}

	nToa1Cycles68KSync = 0;

	nSpriteYOffset =  0x0001;

	nLayer0XOffset = -0x01D6;
	nLayer1XOffset = -0x01D8;
	nLayer2XOffset = -0x01DA;

	ToaInitGP9001();

	nToaPalLen = nColCount;
	ToaPalSrc = RamPal;
	ToaPalInit();
	
	Z180Init(0);
	Z180Open(0);
	Z180MapMemory(Rom02,		0x00000, 0x03fff, MAP_ROM);
	Z180MapMemory(Ram02,		0x0fe00, 0x0ffff, MAP_RAM);
	Z180MapMemory(Ram02 + 0x200,	0x3fe00, 0x3ffff, MAP_RAM);
	Z180MapMemory(ShareRAM,		0x40000, 0x407ff, MAP_RAM);
	Z180SetReadHandler(GhoxMCURead);
	Z180SetWriteHandler(GhoxMCUWrite);
	Z180Close();
	
	BurnYM2151Init(27000000 / 8);
	BurnYM2151SetAllRoutes(0.50, BURN_SND_ROUTE_BOTH);

	bDrawScreen = true;

	DrvDoReset();			// Reset machine
	return 0;
}

static INT32 DrvExit()
{
	ToaPalExit();

	ToaExitGP9001();
	SekExit();				// Deallocate 68000s
	
	Z180Exit();
	
	BurnYM2151Exit();

	BurnFree(Mem);

	return 0;
}

static INT32 DrvDraw()
{
	ToaClearScreen(0);

	if (bDrawScreen) {
		ToaGetBitmap();
		ToaRenderGP9001();					// Render GP9001 graphics
	}

	ToaPalUpdate();							// Update the palette

	return 0;
}

static INT32 DrvFrame()
{
	INT32 nInterleave = 100;
	INT32 nSoundBufferPos = 0;

	if (DrvReset) {
		DrvDoReset();
	}

	memset (DrvInput, 0, 3);
	for (INT32 i = 0; i < 8; i++) {
		DrvInput[0] |= (DrvJoy1[i] & 1) << i;
		DrvInput[1] |= (DrvJoy2[i] & 1) << i;
		DrvInput[2] |= (DrvButton[i] & 1) << i;
	}
	ToaClearOpposites(&DrvInput[0]);
	ToaClearOpposites(&DrvInput[1]);

	if (DrvJoy1[2]) Paddle[0] -= 0x04;
	if (DrvJoy1[3]) Paddle[0] += 0x04;
	if (DrvJoy2[2]) Paddle[1] -= 0x04;
	if (DrvJoy2[3]) Paddle[1] += 0x04;
	
	SekNewFrame();

	SekOpen(0);
	Z180Open(0);

	SekIdle(nCyclesDone[0]);

	nCyclesTotal[0] = (INT32)((INT64)10000000 * nBurnCPUSpeedAdjust / (0x0100 * REFRESHRATE));
	nCyclesTotal[1] = nCyclesTotal[0];

	nCyclesDone[1] = 0;

	SekSetCyclesScanline(nCyclesTotal[0] / 262);
	nToaCyclesDisplayStart = nCyclesTotal[0] - ((nCyclesTotal[0] * (TOA_VBLANK_LINES + 240)) / 262);
	nToaCyclesVBlankStart = nCyclesTotal[0] - ((nCyclesTotal[0] * TOA_VBLANK_LINES) / 262);
	bVBlank = false;

	for (INT32 i = 0; i < nInterleave; i++) {
		INT32 nNext;

		// Run 68000
		nNext = (i + 1) * nCyclesTotal[0] / nInterleave;

		// Trigger VBlank interrupt
		if (nNext > nToaCyclesVBlankStart) {
			if (SekTotalCycles() < nToaCyclesVBlankStart) {
				nCyclesSegment = nToaCyclesVBlankStart - SekTotalCycles();
				SekRun(nCyclesSegment); // bring us to vbl
			}

			if (pBurnDraw) {
				DrvDraw();
			}

			ToaBufferGP9001Sprites();

			bVBlank = true;
			SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
		}

		nCyclesSegment = nNext - SekTotalCycles();

		SekRun(nCyclesSegment);
		
		// Run MCU
		nCyclesDone[1] += Z180Run(SekTotalCycles() - nCyclesDone[1]);
		
		// Render sound segment
		if (pBurnSoundOut) {
			INT32 nSegmentLength = (nBurnSoundLen * i / nInterleave) - nSoundBufferPos;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	nCyclesDone[0] = SekTotalCycles() - nCyclesTotal[0];
	
	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			BurnYM2151Render(pSoundBuf, nSegmentLength);
		}
	}

//	bprintf(PRINT_NORMAL, _T("    %i\n"), nCyclesDone[0]);

//	ToaBufferFCU2Sprites();

	Z180Close();
	SekClose();

	return 0;
}

// Ghox (spinner)

static struct BurnRomInfo ghoxRomDesc[] = {
	{ "tp021-01.u10",	0x20000, 0x9e56ac67, BRF_PRG | BRF_ESS }, //  0 CPU #0 code
	{ "tp021-02.u11",	0x20000, 0x15cac60f, BRF_PRG | BRF_ESS }, //  1

	{ "tp021-03.u36",	0x80000, 0xa15d8e9d, BRF_GRA },           //  2 GP9001 Tile data
	{ "tp021-04.u37",	0x80000, 0x26ed1c9a, BRF_GRA },           //  3

	{ "hd647180.021",	0x08000, 0x6ab59e5b, BRF_PRG | BRF_ESS }, //  4 CPU #1 code
};

STD_ROM_PICK(ghox)
STD_ROM_FN(ghox)

struct BurnDriver BurnDrvGhox = {
	"ghox", NULL, NULL, NULL, "1991",
	"Ghox (spinner)\0", NULL, "Toaplan", "Toaplan GP9001 based",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TOAPLAN_68K_Zx80, GBF_BREAKOUT, 0,
	NULL, ghoxRomInfo, ghoxRomName, NULL, NULL, NULL, NULL, GhoxInputInfo, GhoxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &ToaRecalcPalette, 0x800,
	240, 320, 3, 4
};


// Ghox (joystick)

static struct BurnRomInfo ghoxjRomDesc[] = {
	{ "tp021-01a.u10",	0x20000, 0xc11b13c8, BRF_PRG | BRF_ESS }, //  0 CPU #0 code
	{ "tp021-02a.u11",	0x20000, 0x8d426767, BRF_PRG | BRF_ESS }, //  1

	{ "tp021-03.u36",	0x80000, 0xa15d8e9d, BRF_GRA },           //  2 GP9001 Tile data
	{ "tp021-04.u37",	0x80000, 0x26ed1c9a, BRF_GRA },           //  3

	{ "hd647180.021",	0x08000, 0x6ab59e5b, BRF_PRG | BRF_ESS }, //  4 CPU #1 code
};

STD_ROM_PICK(ghoxj)
STD_ROM_FN(ghoxj)

struct BurnDriver BurnDrvGhoxj = {
	"ghoxj", "ghox", NULL, NULL, "1991",
	"Ghox (joystick)\0", NULL, "Toaplan", "Toaplan GP9001 based",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TOAPLAN_68K_Zx80, GBF_BREAKOUT, 0,
	NULL, ghoxjRomInfo, ghoxjRomName, NULL, NULL, NULL, NULL, GhoxInputInfo, GhoxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &ToaRecalcPalette, 0x800,
	240, 320, 3, 4
};


// Ghox (joystick, older)

static struct BurnRomInfo ghoxjoRomDesc[] = {
	{ "tp021-01.ghoxsticker.u10",	0x20000, 0xad3a8817, BRF_PRG | BRF_ESS }, //  0 CPU #0 code
	{ "tp021-02.ghoxsticker.u11",	0x20000, 0x2340e981, BRF_PRG | BRF_ESS }, //  1

	{ "tp021-03.u36",				0x80000, 0xa15d8e9d, BRF_GRA },           //  2 GP9001 Tile data
	{ "tp021-04.u37",				0x80000, 0x26ed1c9a, BRF_GRA },           //  3

	{ "hd647180.021",				0x08000, 0x6ab59e5b, BRF_PRG | BRF_ESS }, //  4 CPU #1 code
};

STD_ROM_PICK(ghoxjo)
STD_ROM_FN(ghoxjo)

struct BurnDriver BurnDrvGhoxjo = {
	"ghoxjo", "ghox", NULL, NULL, "1991",
	"Ghox (joystick, older)\0", NULL, "Toaplan", "Toaplan GP9001 based",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TOAPLAN_68K_Zx80, GBF_BREAKOUT, 0,
	NULL, ghoxjoRomInfo, ghoxjoRomName, NULL, NULL, NULL, NULL, GhoxInputInfo, GhoxjoDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &ToaRecalcPalette, 0x800,
	240, 320, 3, 4
};