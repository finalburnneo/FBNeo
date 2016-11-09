// Donpachi
#include "cave.h"
#include "msm6295.h"
#include "nmk112.h"

#define USE_SAMPLE_HACK // allow use of high quality music samples.

#define CAVE_VBLANK_LINES 12

#ifdef USE_SAMPLE_HACK
#include "samples.h"
#endif

static UINT8 DrvJoy1[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvJoy2[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static UINT16 DrvInput[2] = {0x0000, 0x0000};
static UINT8 DrvDips[1];

static UINT8 *Mem = NULL, *MemEnd = NULL;
static UINT8 *RamStart, *RamEnd;
static UINT8 *Rom01;
static UINT8 *Ram01;
static UINT8 *DefaultEEPROM = NULL;

static UINT8 DrvReset = 0;
static UINT8 bDrawScreen;
static bool bVBlank;

static INT8 nVideoIRQ;
static INT8 nSoundIRQ;
static INT8 nUnknownIRQ;

static INT8 nIRQPending;

static UINT8 bHasSamples = 0;
static UINT8 bLastSampleDIPMode = 0;

#ifdef USE_SAMPLE_HACK
static UINT8 previous_sound_write[3] = { 0, 0, 0 };
#endif

static struct BurnInputInfo donpachiInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 8,	"p1 coin"},
	{"P1 Start",	BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"},

	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0, 	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1, 	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2, 	"p1 left"},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy1 + 3, 	"p1 right"},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},
	{"P1 Button 2",	BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"},
	{"P1 Button 3",	BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 8,	"p2 coin"},
	{"P2 Start",	BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"},

	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0, 	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1, 	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2, 	"p2 left"},
	{"P2 Right",	BIT_DIGITAL,	DrvJoy2 + 3, 	"p2 right"},
	{"P2 Button 1",	BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"},
	{"P2 Button 2",	BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"},
	{"P2 Button 3",	BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,		"reset"},
	{"Diagnostics",	BIT_DIGITAL,	DrvJoy1 + 9,	"diag"},
	{"Service",		BIT_DIGITAL,	DrvJoy2 + 9,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
};

STDINPUTINFO(donpachi)

static struct BurnDIPInfo donpachiDIPList[]=
{
	{0x15, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    2, "Enable high-quality music w/samples"},
	{0x15, 0x01, 0x08, 0x08, "On"			},
	{0x15, 0x01, 0x08, 0x00, "Off"			},
};

STDDIPINFO(donpachi)

static void DrvSampleReset()
{
	BurnSampleReset();
	// don't autoplay everything @ reset w/ SAMPLE_AUTOLOOP
	for (INT32 i = 0;i < 20; i++){
		BurnSampleStop(i);
	}
}

static void UpdateIRQStatus()
{
	nIRQPending = (nVideoIRQ == 0 || nSoundIRQ == 0 || nUnknownIRQ == 0);
	SekSetIRQLine(1, nIRQPending ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

UINT8 __fastcall donpachiReadByte(UINT32 sekAddress)
{
	switch (sekAddress) {

		case 0x900000:
		case 0x900001:
		case 0x900002:
		case 0x900003: {
			UINT8 nRet = (nUnknownIRQ << 1) | nVideoIRQ;
			return nRet;
		}
		case 0x900004:
		case 0x900005: {
			UINT8 nRet = (nUnknownIRQ << 1) | nVideoIRQ;
			nVideoIRQ = 1;
			UpdateIRQStatus();
			return nRet;
		}
		case 0x900006:
		case 0x900007: {
			UINT8 nRet = (nUnknownIRQ << 1) | nVideoIRQ;
			nUnknownIRQ = 1;
			UpdateIRQStatus();
			return nRet;
		}

		case 0xB00001:
			return MSM6295ReadStatus(0);
		case 0xB00011:
			return MSM6295ReadStatus(1);

		case 0xC00000:
			return (DrvInput[0] >> 8) ^ 0xFF;
		case 0xC00001:
			return (DrvInput[0] & 0xFF) ^ 0xFF;
		case 0xC00002:
			return ((DrvInput[1] >> 8) ^ 0xF7) | (EEPROMRead() << 3);
		case 0xC00003:
			return (DrvInput[1] & 0xFF) ^ 0xFF;

		default: {
//			bprintf(PRINT_NORMAL, "Attempt to read byte value of location %x\n", sekAddress);
		}
	}
	return 0;
}

UINT16 __fastcall donpachiReadWord(UINT32 sekAddress)
{
	switch (sekAddress) {
		case 0x900000:
		case 0x900002: {
			UINT16 nRet = (nUnknownIRQ << 1) | nVideoIRQ;
			return nRet;
		}

		case 0x900004: {
			UINT16 nRet = (nUnknownIRQ << 1) | nVideoIRQ;
			nVideoIRQ = 1;
			UpdateIRQStatus();
			return nRet;
		}
		case 0x900006: {
			UINT16 nRet = (nUnknownIRQ << 1) | nVideoIRQ;
			nUnknownIRQ = 1;
			UpdateIRQStatus();
			return nRet;
		}

		case 0xB00000:
			return MSM6295ReadStatus(0);
		case 0xB00010:
			return MSM6295ReadStatus(1);

		case 0xC00000:
			return DrvInput[0] ^ 0xFFFF;
		case 0xC00002:
			return (DrvInput[1] ^ 0xF7FF) | (EEPROMRead() << 11);

		default: {
// 			bprintf(PRINT_NORMAL, "Attempt to read word value of location %x\n", sekAddress);
		}
	}
	return 0;
}

void __fastcall donpachiWriteByte(UINT32 sekAddress, UINT8 byteValue)
{
	switch (sekAddress) {

		case 0xB00000:
		case 0xB00001:
		case 0xB00002:
		case 0xB00003:
			MSM6295Command(0, byteValue);
			break;
		case 0xB00010:
		case 0xB00011:
		case 0xB00012:
		case 0xB00013:
			MSM6295Command(1, byteValue);
			break;

		case 0xB00020:
		case 0xB00021:
		case 0xB00022:
		case 0xB00023:
		case 0xB00024:
		case 0xB00025:
		case 0xB00026:
		case 0xB00027:
		case 0xB00028:
		case 0xB00029:
		case 0xB0002A:
		case 0xB0002B:
		case 0xB0002C:
		case 0xB0002D:
		case 0xB0002E:
		case 0xB0002F: {
			NMK112_okibank_write((sekAddress / 2) & 0x07, byteValue);
			break;
		}

		case 0xD00000:
			EEPROMWrite(byteValue & 0x04, byteValue & 0x02, byteValue & 0x08);
			break;

		default: {
//			bprintf(PRINT_NORMAL, "Attempt to write byte value %x to location %x\n", byteValue, sekAddress);
		}
	}
}

void __fastcall donpachiWriteWord(UINT32 sekAddress, UINT16 wordValue)
{
	switch (sekAddress) {
		case 0x600000:
			CaveTileReg[1][0] = wordValue;
			break;
		case 0x600002:
			CaveTileReg[1][1] = wordValue;
			break;
		case 0x600004:
			CaveTileReg[1][2] = wordValue;
			break;

		case 0x700000:
			CaveTileReg[0][0] = wordValue;
			break;
		case 0x700002:
			CaveTileReg[0][1] = wordValue;
			break;
		case 0x700004:
			CaveTileReg[0][2] = wordValue;
			break;

		case 0x800000:
			CaveTileReg[2][0] = wordValue;
			break;
		case 0x800002:
			CaveTileReg[2][1] = wordValue;
			break;
		case 0x800004:
			CaveTileReg[2][2] = wordValue;
			break;

		case 0x900000:
			nCaveXOffset = wordValue;
			return;
		case 0x900002:
			nCaveYOffset = wordValue;
			return;
		case 0x900008:
			CaveSpriteBuffer();
			nCaveSpriteBank = wordValue;
			return;

		case 0xB00000:
		case 0xB00002:
#ifdef USE_SAMPLE_HACK
			if (wordValue == 0x78) {
				memset (previous_sound_write, 0, 3);
				DrvSampleReset(); // STOP
			} else {
				previous_sound_write[0] = previous_sound_write[1];
				previous_sound_write[1] = previous_sound_write[2];
				previous_sound_write[2] = wordValue;

				if (previous_sound_write[0] == 0x08) {
					UINT16 select = (previous_sound_write[1]*256) + previous_sound_write[2];
					INT32 sample = 0xff;
					switch (select)
					{

						// I'm silencing all sounds from the music chip atm, it may be necessary to let some of
						// these actually play. Just set use BurnSampleReset, and restore the volume of the music chip.

						case 0xcc12: sample = 0x00; break;
						case 0x8112: sample = 0x01; break;
						case 0x9814: sample = 0x02; break;
						case 0xb113: sample = 0x03; break;
						case 0xa112: sample = 0x04; break;
						case 0xd812: sample = 0x05; break;
						case 0x9313: sample = 0x06; break;
						case 0xe813: sample = 0x07; break;
						case 0xf013: sample = 0x08; break;
						case 0xf111: sample = 0x09; break;
						case 0x9a13: sample = 0x0a; break;
						case 0xf813: sample = 0x0b; break;
						case 0xd013: sample = 0x0c; break;
						case 0xc813: sample = 0x0d; break;
					}

					if (sample != 0xff && BurnSampleGetStatus(sample) == 0) {
						DrvSampleReset();
						BurnSamplePlay(sample);
//						bprintf (0, _T("Sample playing: %d\n"), sample);
					}
				}
			}
#endif
			MSM6295Command(0, wordValue);
			break;
		case 0xB00010:
		case 0xB00012:
			MSM6295Command(1, wordValue);
			break;

		case 0xB00020:
		case 0xB00021:
		case 0xB00022:
		case 0xB00023:
		case 0xB00024:
		case 0xB00025:
		case 0xB00026:
		case 0xB00027:
		case 0xB00028:
		case 0xB00029:
		case 0xB0002A:
		case 0xB0002B:
		case 0xB0002C:
		case 0xB0002D:
		case 0xB0002E:
		case 0xB0002F: {
			NMK112_okibank_write((sekAddress / 2) & 0x07, wordValue & 0xff);
			break;
		}

		case 0xD00000:
			wordValue >>= 8;
			EEPROMWrite(wordValue & 0x04, wordValue & 0x02, wordValue & 0x08);
			break;

		default: {
//			bprintf(PRINT_NORMAL, "Attempt to write word value %x to location %x\n", wordValue, sekAddress);

		}
	}
}

static INT32 DrvExit()
{
	EEPROMExit();
	
	MSM6295Exit(0);
	MSM6295Exit(1);
#ifdef USE_SAMPLE_HACK
	BurnSampleExit();
#endif

	CaveTileExit();
	CaveSpriteExit();
	CavePalExit();

	SekExit();				// Deallocate 68000s

	BurnFree(Mem);

	return 0;
}

static INT32 DrvDoReset()
{
	SekOpen(0);
	SekReset();
	SekClose();

	EEPROMReset();

	nVideoIRQ = 1;
	nSoundIRQ = 1;
	nUnknownIRQ = 1;

	nIRQPending = 0;

	MSM6295Reset(0);
	MSM6295Reset(1);
	NMK112Reset();
#ifdef USE_SAMPLE_HACK
	DrvSampleReset();
	memset (previous_sound_write, 0, 3);
#endif
	HiscoreReset();
	return 0;
}

static INT32 DrvDraw()
{
	CavePalUpdate4Bit(0, 128);				// Update the palette
	CaveClearScreen(CavePalette[0x7F00]);

	if (bDrawScreen) {
//		CaveGetBitmap();

		CaveTileRender(1);					// Render tiles
	}

	return 0;
}

static void CheckDIP()
{
	if (bHasSamples && bLastSampleDIPMode != DrvDips[0]) {
		//bprintf(0, _T("DIP Changed! %X\n"), DrvDips[0]);
		bLastSampleDIPMode = DrvDips[0];

		MSM6295SetRoute(0, (bLastSampleDIPMode == 8) ? 0.00 : 1.60, BURN_SND_ROUTE_BOTH);
		BurnSampleSetAllRoutesAllSamples((bLastSampleDIPMode == 8) ? 0.40 : 0.00, BURN_SND_ROUTE_BOTH);
	}

}

inline static INT32 CheckSleep(INT32)
{
	return 0;
}

static INT32 DrvFrame()
{
	INT32 nCyclesVBlank;
	INT32 nInterleave = 8;

	INT32 nCyclesTotal[2];
	INT32 nCyclesDone[2];

	INT32 nCyclesSegment;

	if (DrvReset) {														// Reset machine
		DrvDoReset();
	}

	CheckDIP();

	// Compile digital inputs
	DrvInput[0] = 0x0000;  												// Player 1
	DrvInput[1] = 0x0000;  												// Player 2
	for (INT32 i = 0; i < 10; i++) {
		DrvInput[0] |= (DrvJoy1[i] & 1) << i;
		DrvInput[1] |= (DrvJoy2[i] & 1) << i;
	}
	CaveClearOpposites(&DrvInput[0]);
	CaveClearOpposites(&DrvInput[1]);

	SekNewFrame();

	nCyclesTotal[0] = (INT32)((INT64)16000000 * nBurnCPUSpeedAdjust / (0x0100 * CAVE_REFRESHRATE));
	nCyclesDone[0] = 0;

	nCyclesVBlank = nCyclesTotal[0] - (INT32)((nCyclesTotal[0] * CAVE_VBLANK_LINES) / 271.5);
	bVBlank = false;

	INT32 nSoundBufferPos = 0;

	SekOpen(0);

	for (INT32 i = 1; i <= nInterleave; i++) {
    	INT32 nCurrentCPU = 0;
		INT32 nNext = i * nCyclesTotal[nCurrentCPU] / nInterleave;

		// Run 68000

		// See if we need to trigger the VBlank interrupt
		if (!bVBlank && nNext > nCyclesVBlank) {
			if (nCyclesDone[nCurrentCPU] < nCyclesVBlank) {
				nCyclesSegment = nCyclesVBlank - nCyclesDone[nCurrentCPU];
				if (!CheckSleep(nCurrentCPU)) {							// See if this CPU is busywaiting
					nCyclesDone[nCurrentCPU] += SekRun(nCyclesSegment);
				} else {
					nCyclesDone[nCurrentCPU] += SekIdle(nCyclesSegment);
				}
			}

			if (pBurnDraw != NULL) {
				DrvDraw();												// Draw screen if needed
			}

			bVBlank = true;
			nVideoIRQ = 0;
			UpdateIRQStatus();
		}

		nCyclesSegment = nNext - nCyclesDone[nCurrentCPU];
		if (!CheckSleep(nCurrentCPU)) {									// See if this CPU is busywaiting
			nCyclesDone[nCurrentCPU] += SekRun(nCyclesSegment);
		} else {
			nCyclesDone[nCurrentCPU] += SekIdle(nCyclesSegment);
		}
	}

	// Make sure the buffer is entirely filled.
	{
		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			if (nSegmentLength) {
				MSM6295Render(0, pSoundBuf, nSegmentLength);
				MSM6295Render(1, pSoundBuf, nSegmentLength);
#ifdef USE_SAMPLE_HACK
				BurnSampleRender(pSoundBuf, nSegmentLength);
#endif
			}
		}
	}

	SekClose();

	return 0;
}

// This routine is called first to determine how much memory is needed (MemEnd-(UINT8 *)0),
// and then afterwards to set up all the pointers
static INT32 MemIndex()
{
	UINT8* Next; Next = Mem;
	Rom01			= Next; Next += 0x080000;		// 68K program
	CaveSpriteROM	= Next; Next += 0x800000;
	CaveTileROM[0]	= Next; Next += 0x200000;		// Tile layer 0
	CaveTileROM[1]	= Next; Next += 0x200000;		// Tile layer 1
	CaveTileROM[2]	= Next; Next += 0x080000;		// Tile layer 2
	MSM6295ROM		= Next; Next += 0x300000;
	DefaultEEPROM	= Next; Next += 0x000080;
	RamStart		= Next;
	Ram01			= Next; Next += 0x010000;		// CPU #0 work RAM
	CaveTileRAM[0]	= Next; Next += 0x008000;
	CaveTileRAM[1]	= Next; Next += 0x008000;
	CaveTileRAM[2]	= Next; Next += 0x008000;
	CaveSpriteRAM	= Next; Next += 0x010000;
	CavePalSrc		= Next; Next += 0x001000;		// palette
	RamEnd			= Next;
	MemEnd			= Next;

	return 0;
}

static void NibbleSwap2(UINT8* pData, INT32 nLen)
{
	UINT8* pOrg = pData + nLen - 1;
	UINT8* pDest = pData + ((nLen - 1) << 1);

	for (INT32 i = 0; i < nLen; i++, pOrg--, pDest -= 2) {
		pDest[1] = *pOrg & 15;
		pDest[0] = *pOrg >> 4;
	}

	return;
}

static INT32 LoadRoms()
{
	// Load 68000 ROM
	BurnLoadRom(Rom01, 0, 1);

	BurnLoadRom(CaveSpriteROM + 0x000000, 1, 1);
	BurnLoadRom(CaveSpriteROM + 0x200000, 2, 1);
	BurnByteswap(CaveSpriteROM, 0x400000);
	NibbleSwap2(CaveSpriteROM, 0x400000);

	BurnLoadRom(CaveTileROM[0], 3, 1);
	NibbleSwap2(CaveTileROM[0], 0x100000);
	BurnLoadRom(CaveTileROM[1], 4, 1);
	NibbleSwap2(CaveTileROM[1], 0x100000);
	BurnLoadRom(CaveTileROM[2], 5, 1);
	NibbleSwap2(CaveTileROM[2], 0x040000);

	// Load MSM6295 ADPCM data
	BurnLoadRom(MSM6295ROM + 0x000000, 6, 1); // OKI #1 ONLY
	BurnLoadRom(MSM6295ROM + 0x100000, 7, 1); // OKI #0 & #1
	
	BurnLoadRom(DefaultEEPROM, 8, 1);

	return 0;
}

// Scan ram
static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {						// Return minimum compatible version
		*pnMin = 0x020902;
	}

	EEPROMScan(nAction, pnMin);			// Scan EEPROM

	if (nAction & ACB_VOLATILE) {		// Scan volatile ram

		memset(&ba, 0, sizeof(ba));
		ba.Data		= RamStart;
		ba.nLen		= RamEnd - RamStart;
		ba.szName	= "RAM";
		BurnAcb(&ba);

		SekScan(nAction);				// scan 68000 states

		MSM6295Scan(0, nAction);
		MSM6295Scan(1, nAction);
		NMK112_Scan(nAction);

		SCAN_VAR(nVideoIRQ);
		SCAN_VAR(nSoundIRQ);
		SCAN_VAR(nUnknownIRQ);
		SCAN_VAR(bVBlank);

		CaveScanGraphics();

		SCAN_VAR(DrvInput);
#ifdef USE_SAMPLE_HACK
		BurnSampleScan(nAction, pnMin); // Must be at the end to maintain compatibility between sample and non-sample mode.
#endif
	}

	if (nAction & ACB_WRITE) {
		CaveRecalcPalette = 1;
		bLastSampleDIPMode = 0xf7;
	}

	return 0;
}

static INT32 DrvInit()
{
	INT32 nLen;

	BurnSetRefreshRate(CAVE_REFRESHRATE);

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
	
	EEPROMInit(&eeprom_interface_93C46);
	if (!EEPROMAvailable()) EEPROMFill(DefaultEEPROM,0, 0x80);	

	{
		SekInit(0, 0x68000);													// Allocate 68000
		SekOpen(0);

		// Map 68000 memory:
		SekMapMemory(Rom01,						0x000000, 0x07FFFF, MAP_ROM);	// CPU 0 ROM
		SekMapMemory(Ram01,						0x100000, 0x10FFFF, MAP_RAM);
		SekMapMemory(CaveTileRAM[1],			0x200000, 0x207FFF, MAP_RAM);
		SekMapMemory(CaveTileRAM[0],			0x300000, 0x307FFF, MAP_RAM);
		SekMapMemory(CaveTileRAM[2] + 0x4000,	0x400000, 0x403FFF, MAP_RAM);
		SekMapMemory(CaveTileRAM[2] + 0x4000,	0x404000, 0x407FFF, MAP_RAM);
		SekMapMemory(CaveSpriteRAM,				0x500000, 0x50FFFF, MAP_RAM);
		SekMapMemory(CavePalSrc,				0xA08000, 0xA08FFF, MAP_RAM);	// Palette RAM

		SekSetReadWordHandler(0, donpachiReadWord);
		SekSetReadByteHandler(0, donpachiReadByte);
		SekSetWriteWordHandler(0, donpachiWriteWord);
		SekSetWriteByteHandler(0, donpachiWriteByte);

		SekClose();
	}

	CavePalInit(0x8000);
	CaveTileInit();
	CaveSpriteInit(0, 0x0800000);
	CaveTileInitLayer(0, 0x200000, 8, 0x4000);
	CaveTileInitLayer(1, 0x200000, 8, 0x4000);
	CaveTileInitLayer(2, 0x080000, 8, 0x4000);

	MSM6295Init(0, 8000, 0);
	MSM6295Init(1, 16000, 0);
#ifdef USE_SAMPLE_HACK
	MSM6295SetRoute(0, 0.00, BURN_SND_ROUTE_BOTH);
#else
	MSM6295SetRoute(0, 1.60, BURN_SND_ROUTE_BOTH);
#endif
	MSM6295SetRoute(1, 1.00, BURN_SND_ROUTE_BOTH);

	NMK112_init(1 << 0, MSM6295ROM + 0x100000, MSM6295ROM, 0x200000, 0x300000);


#ifdef USE_SAMPLE_HACK
	BurnUpdateProgress(0.0, _T("Loading samples..."), 0);
	bBurnSampleTrimSampleEnd = 1;
	BurnSampleInit(1);
	BurnSampleSetAllRoutesAllSamples(0.40, BURN_SND_ROUTE_BOTH);
    bHasSamples = BurnSampleGetStatus(0) != -1;
	bLastSampleDIPMode = DrvDips[0];

	if (!(bLastSampleDIPMode == 8) || !bHasSamples) { // Samples not found, fallback to internal samples.
		MSM6295SetRoute(0, 1.60, BURN_SND_ROUTE_BOTH);
		BurnSampleSetAllRoutesAllSamples(0.00, BURN_SND_ROUTE_BOTH);
	}
#endif

	bDrawScreen = true;

	DrvDoReset(); // Reset machine

	return 0;
}

// Rom information
static struct BurnRomInfo donpachiRomDesc[] = {
	{ "prgu.u29",     0x080000, 0x89C36802, BRF_ESS | BRF_PRG }, //  0 CPU #0 code

	{ "atdp.u44",     0x200000, 0x7189E953, BRF_GRA },			 //  1 Sprite data
	{ "atdp.u45",     0x200000, 0x6984173F, BRF_GRA },			 //  2

	{ "atdp.u54",     0x100000, 0x6BDA6B66, BRF_GRA },			 //  3 Layer 0 Tile data
	{ "atdp.u57",     0x100000, 0x0A0E72B9, BRF_GRA },			 //  4 Layer 1 Tile data
	{ "text.u58",     0x040000, 0x5DBA06E7, BRF_GRA },			 //  5 Layer 2 Tile data

	{ "atdp.u32",     0x100000, 0x0D89FCCA, BRF_SND },			 //  6 MSM6295 #1 ADPCM data
	{ "atdp.u33",     0x200000, 0xD749DE00, BRF_SND },			 //  7 MSM6295 #0/1 ADPCM data
	
	{ "eeprom-donpachi.u10", 0x0080, 0x315fb546, BRF_ESS | BRF_PRG },
	
	{ "peel18cv8p-15.u18", 0x0155, 0x3f4787e9, BRF_OPT },
};


STD_ROM_PICK(donpachi)
STD_ROM_FN(donpachi)

static struct BurnRomInfo donpachijRomDesc[] = {
	{ "prg.u29",      0x080000, 0x6BE14AF6, BRF_ESS | BRF_PRG }, //  0 CPU #0 code

	{ "atdp.u44",     0x200000, 0x7189E953, BRF_GRA },			 //  1 Sprite data
	{ "atdp.u45",     0x200000, 0x6984173F, BRF_GRA },			 //  2

	{ "atdp.u54",     0x100000, 0x6BDA6B66, BRF_GRA },			 //  3 Layer 0 Tile data
	{ "atdp.u57",     0x100000, 0x0A0E72B9, BRF_GRA },			 //  4 Layer 1 Tile data
	{ "u58.bin",      0x040000, 0x285379FF, BRF_GRA },			 //  5 Layer 2 Tile data

	{ "atdp.u32",     0x100000, 0x0D89FCCA, BRF_SND },			 //  6 MSM6295 #1 ADPCM data
	{ "atdp.u33",     0x200000, 0xD749DE00, BRF_SND },			 //  7 MSM6295 #0/1 ADPCM data
	
	{ "eeprom-donpachi.bin", 0x0080, 0x315fb546, BRF_ESS | BRF_PRG },
	
	{ "peel18cv8p-15.u18", 0x0155, 0x3f4787e9, BRF_OPT },
};


STD_ROM_PICK(donpachij)
STD_ROM_FN(donpachij)

static struct BurnRomInfo donpachikrRomDesc[] = {
	{ "prgk.u26",     0x080000, 0xBBAF4C8B, BRF_ESS | BRF_PRG }, //  0 CPU #0 code

	{ "atdp.u44",     0x200000, 0x7189E953, BRF_GRA },			 //  1 Sprite data
	{ "atdp.u45",     0x200000, 0x6984173F, BRF_GRA },			 //  2

	{ "atdp.u54",     0x100000, 0x6BDA6B66, BRF_GRA },			 //  3 Layer 0 Tile data
	{ "atdp.u57",     0x100000, 0x0A0E72B9, BRF_GRA },			 //  4 Layer 1 Tile data
	{ "u58.bin",      0x040000, 0x285379FF, BRF_GRA },			 //  5 Layer 2 Tile data

	{ "atdp.u32",     0x100000, 0x0D89FCCA, BRF_SND },			 //  6 MSM6295 #1 ADPCM data
	{ "atdp.u33",     0x200000, 0xD749DE00, BRF_SND },			 //  7 MSM6295 #0/1 ADPCM data
	
	{ "eeprom-donpachi.bin", 0x0080, 0x315fb546, BRF_ESS | BRF_PRG },
	
	{ "peel18cv8p-15.u18", 0x0155, 0x3f4787e9, BRF_OPT },
};


STD_ROM_PICK(donpachikr)
STD_ROM_FN(donpachikr)

static struct BurnRomInfo donpachihkRomDesc[] = {
	{ "37.u29",       0x080000, 0x71f39f30, BRF_ESS | BRF_PRG }, //  0 CPU #0 code

	{ "atdp.u44",     0x200000, 0x7189E953, BRF_GRA },			 //  1 Sprite data
	{ "atdp.u45",     0x200000, 0x6984173F, BRF_GRA },			 //  2

	{ "atdp.u54",     0x100000, 0x6BDA6B66, BRF_GRA },			 //  3 Layer 0 Tile data
	{ "atdp.u57",     0x100000, 0x0A0E72B9, BRF_GRA },			 //  4 Layer 1 Tile data
	{ "u58.bin",      0x040000, 0x285379ff, BRF_GRA },			 //  5 Layer 2 Tile data

	{ "atdp.u32",     0x100000, 0x0D89FCCA, BRF_SND },			 //  6 MSM6295 #1 ADPCM data
	{ "atdp.u33",     0x200000, 0xD749DE00, BRF_SND },			 //  7 MSM6295 #0/1 ADPCM data
	
	{ "eeprom-donpachi.bin", 0x0080, 0x315fb546, BRF_ESS | BRF_PRG },
	
	{ "peel18cv8p-15.u18", 0x0155, 0x3f4787e9, BRF_OPT },
};

STD_ROM_PICK(donpachihk)
STD_ROM_FN(donpachihk)

static struct BurnSampleInfo DonpachiSampleDesc[] = {
#ifdef USE_SAMPLE_HACK
#if !defined ROM_VERIFY
	{ "02 - Sortie Instruction", SAMPLE_AUTOLOOP | SAMPLE_NOSTORE },
	{ "03 - Silent Outpost Base", SAMPLE_AUTOLOOP | SAMPLE_NOSTORE },
	{ "04 - Gale Force", SAMPLE_AUTOLOOP | SAMPLE_NOSTORE },
	{ "05 - God of Destruction", SAMPLE_AUTOLOOP | SAMPLE_NOSTORE },
	{ "06 - Advance Through the Sky", SAMPLE_AUTOLOOP | SAMPLE_NOSTORE },
	{ "07 - The Battle Intensifies", SAMPLE_AUTOLOOP | SAMPLE_NOSTORE },
	{ "08 - An Equal Match", SAMPLE_AUTOLOOP | SAMPLE_NOSTORE },
	{ "09 - It's All Up To Me!!", SAMPLE_NOLOOP | SAMPLE_NOSTORE },
	{ "10 - Chief's Congratulations", SAMPLE_NOLOOP | SAMPLE_NOSTORE },
	{ "11 - Breakthrough", SAMPLE_NOLOOP | SAMPLE_NOSTORE },
	{ "12 - Pressure", SAMPLE_AUTOLOOP | SAMPLE_NOSTORE },
	{ "13 - My Duty is Done", SAMPLE_AUTOLOOP | SAMPLE_NOSTORE },
	{ "14 - Eternal Soldier", SAMPLE_AUTOLOOP | SAMPLE_NOSTORE },
	{ "15 - Chase in the Dark", SAMPLE_AUTOLOOP | SAMPLE_NOSTORE },
#endif
#endif
	{ "", 0 }
};

STD_SAMPLE_PICK(Donpachi)
STD_SAMPLE_FN(Donpachi)

struct BurnDriver BurnDrvDonpachi = {
	"donpachi", NULL, NULL, "donpachi", "1995",
	"DonPachi (USA, ver. 1.12, 95/05/2x)\0", NULL, "Atlus / Cave", "Cave",
	L"\u9996\u9818\u8702 DonPachi (USA, ver. 1.12, 95/05/2x)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_16BIT_ONLY | BDF_HISCORE_SUPPORTED, 2, HARDWARE_CAVE_68K_ONLY | HARDWARE_CAVE_M6295, GBF_VERSHOOT, 0,
	NULL, donpachiRomInfo, donpachiRomName, DonpachiSampleInfo, DonpachiSampleName, donpachiInputInfo, donpachiDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&CaveRecalcPalette, 0x8000, 240, 320, 3, 4
};

struct BurnDriver BurnDrvDonpachij = {
	"donpachij", "donpachi", NULL, "donpachi", "1995",
	"DonPachi (Japan, ver. 1.01, 95/05/11)\0", NULL, "Atlus / Cave", "Cave",
	L"\u9996\u9818\u8702 DonPachi (Japan, ver. 1.01, 95/05/11)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_16BIT_ONLY | BDF_HISCORE_SUPPORTED, 2, HARDWARE_CAVE_68K_ONLY | HARDWARE_CAVE_M6295, GBF_VERSHOOT, 0,
	NULL, donpachijRomInfo, donpachijRomName, DonpachiSampleInfo, DonpachiSampleName, donpachiInputInfo, donpachiDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&CaveRecalcPalette, 0x8000, 240, 320, 3, 4
};

struct BurnDriver BurnDrvDonpachikr = {
	"donpachikr", "donpachi", NULL, "donpachi", "1995",
	"DonPachi (Korea, ver. 1.12, 95/05/2x)\0", NULL, "Atlus / Cave", "Cave",
	L"\u9996\u9818\u8702 DonPachi (Korea, ver. 1.12, 95/05/2x)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_16BIT_ONLY | BDF_HISCORE_SUPPORTED, 2, HARDWARE_CAVE_68K_ONLY | HARDWARE_CAVE_M6295, GBF_VERSHOOT, 0,
	NULL, donpachikrRomInfo, donpachikrRomName, DonpachiSampleInfo, DonpachiSampleName, donpachiInputInfo, donpachiDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&CaveRecalcPalette, 0x8000, 240, 320, 3, 4
};

struct BurnDriver BurnDrvDonpachihk = {
	"donpachihk", "donpachi", NULL, "donpachi", "1995",
	"DonPachi (Hong Kong, ver. 1.10, 95/05/17)\0", NULL, "Atlus / Cave", "Cave",
	L"\u9996\u9818\u8702 DonPachi (Hong Kong, ver. 1.10, 95/05/17)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_16BIT_ONLY | BDF_HISCORE_SUPPORTED, 2, HARDWARE_CAVE_68K_ONLY | HARDWARE_CAVE_M6295, GBF_VERSHOOT, 0,
	NULL, donpachihkRomInfo, donpachihkRomName, DonpachiSampleInfo, DonpachiSampleName, donpachiInputInfo, donpachiDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&CaveRecalcPalette, 0x8000, 240, 320, 3, 4
};
