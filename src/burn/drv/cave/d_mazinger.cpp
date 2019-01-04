// mazinger
#include "cave.h"
#include "watchdog.h"
#include "msm6295.h"
#include "burn_ym2203.h"
#include "bitswap.h"

#define CAVE_VBLANK_LINES 12

static UINT8 DrvJoy1[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvJoy2[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static UINT16 DrvInput[2] = {0x0000, 0x0000};

static UINT8 *Mem = NULL, *MemEnd = NULL;
static UINT8 *RamStart, *RamEnd;
static UINT8 *Rom01, *RomZ80;
static UINT8 *Ram01, *RamZ80;
static UINT8 *DefEEPROM = NULL;

static UINT8 *DrvSndROM;

static UINT8 DrvReset = 0;
static UINT8 bDrawScreen;
static bool bVBlank;

static INT8 nVideoIRQ;
static INT8 nSoundIRQ;
static INT8 nUnknownIRQ;

static INT8 nIRQPending;

static INT32 nCyclesTotal[2];
static INT32 nCyclesDone[2];

static INT32 SoundLatch;
static INT32 SoundLatchReply;
static INT32 SoundLatchStatus;

static UINT8 DrvZ80Bank;
static UINT8 DrvOkiBank1;
static UINT8 DrvOkiBank2;

static struct BurnInputInfo mazingerInputList[] = {
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
};

STDINPUTINFO(mazinger)

static void UpdateIRQStatus()
{
	nIRQPending = (nVideoIRQ == 0 || nSoundIRQ == 0 || nUnknownIRQ == 0);
	SekSetIRQLine(1, nIRQPending ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static UINT8 __fastcall mazingerReadByte(UINT32 sekAddress)
{
	switch (sekAddress) {
		case 0x800002:
			return (DrvInput[1] ^ 0xF7) | (EEPROMRead() << 3);
		case 0x800003:
			return (DrvInput[1] & 0xFF) ^ 0xFF;

		default: {
 			bprintf(PRINT_NORMAL, _T("Attempt to read byte value of location %x\n"), sekAddress);
		}
	}
	return 0;
}

static void __fastcall mazingerWriteByte(UINT32 sekAddress, UINT8 byteValue)
{
	switch (sekAddress) {

		case 0x900000:
			EEPROMWrite(byteValue & 0x04, byteValue & 0x02, byteValue & 0x08);
			break;

		default: {
			bprintf(PRINT_NORMAL, _T("Attempt to write byte value %x to location %x\n"), byteValue, sekAddress);

		}
	}
}

static UINT16 __fastcall mazingerReadWord(UINT32 sekAddress)
{
	switch (sekAddress) {
		case 0x300000:
		case 0x300002: {
			UINT8 nRet = (nUnknownIRQ << 1) | nVideoIRQ;
			return nRet;
		}
		case 0x300004: {
			UINT8 nRet = (nUnknownIRQ << 1) | nVideoIRQ;
			nVideoIRQ = 1;
			UpdateIRQStatus();
			return nRet;
		}
		case 0x300006: {
			UINT8 nRet = (nUnknownIRQ << 1) | nVideoIRQ;
			nUnknownIRQ = 1;
			UpdateIRQStatus();
			return nRet;
		}

		case 0x30006E:
			return SoundLatchReply;

		case 0x800000:
			return DrvInput[0] ^ 0xFFFF;
		case 0x800002:
			return (DrvInput[1] ^ 0xF7FF) | (EEPROMRead() << 11);

		default: {
 			bprintf(PRINT_NORMAL, _T("Attempt to read word value of location %x\n"), sekAddress);
		}
	}
	return 0;
}

static void __fastcall mazingerWriteWord(UINT32 sekAddress, UINT16 wordValue)
{
	if (sekAddress >= 0x30000a && sekAddress <= 0x300066) return;
	if (sekAddress >= 0x30006a && sekAddress <= 0x30006c) return;
	if (sekAddress >= 0x300004 && sekAddress <= 0x300006) return;

	switch (sekAddress) {
		case 0x300000:
			nCaveXOffset = wordValue;
			return;
		case 0x300002:
			nCaveYOffset = wordValue;
			return;

		case 0x300008:
//			CaveSpriteBuffer();
			nCaveSpriteBank = wordValue;
			return;
		case 0x300068:
			BurnWatchdogWrite();
			return;
		case 0x30006e:
			SoundLatch = wordValue;
			SoundLatchStatus |= 0x0C;

			ZetNmi();
			nCyclesDone[1] += ZetRun(0x0400);
			return;

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

		case 0x900000:
			wordValue >>= 8;
			EEPROMWrite(wordValue & 0x04, wordValue & 0x02, wordValue & 0x08);
			break;

		default: {
			bprintf(PRINT_NORMAL, _T("Attempt to write word value %x to location %x\n"), wordValue, sekAddress);

		}
	}
}

static void __fastcall mazingerWriteBytePalette(UINT32 sekAddress, UINT8 byteValue)
{
	CavePalWriteByte(sekAddress & 0xffff, byteValue);
}

static void __fastcall mazingerWriteWordPalette(UINT32 sekAddress, UINT16 wordValue)
{
	CavePalWriteWord(sekAddress & 0xffff, wordValue);
}

static UINT8 __fastcall mazingerZIn(UINT16 nAddress)
{
	nAddress &= 0xFF;

	switch (nAddress) {
		case 0x30: {
			SoundLatchStatus |= 0x04;
			return SoundLatch & 0xFF;
		}

		case 0x52: {
			return BurnYM2203Read(0, 0);
		}

		default: {
			bprintf(PRINT_NORMAL, _T("Z80 Port Read %x\n"), nAddress);
		}
	}

	return 0;
}

static void msm6295_bank()
{
	MSM6295SetBank(0, DrvSndROM + 0x20000 * DrvOkiBank1, 0x00000, 0x1ffff);
	MSM6295SetBank(0, DrvSndROM + 0x20000 * DrvOkiBank2, 0x20000, 0x3ffff);
}

static void __fastcall mazingerZOut(UINT16 nAddress, UINT8 nValue)
{
	nAddress &= 0xFF;

	switch (nAddress) {
		case 0x00: {
			DrvZ80Bank = nValue & 0x07;

			ZetMapMemory(RomZ80 + (DrvZ80Bank * 0x4000), 0x4000, 0x7FFF, MAP_ROM);
			return;
		}

		case 0x10:
			SoundLatchReply = nValue;
			break;

		case 0x50: {
			BurnYM2203Write(0, 0, nValue);
			return;
		}

		case 0x51: {
			BurnYM2203Write(0, 1, nValue);
			return;
		}

		case 0x70: {
			MSM6295Write(0, nValue);
			return;
		}

		case 0x74: {
			DrvOkiBank1 = (nValue >> 0) & 0x03;
			DrvOkiBank2 = (nValue >> 4) & 0x03;

			msm6295_bank();
			return;
		}

		default: {
			bprintf(PRINT_NORMAL, _T("Z80 Port Write %x, %x\n"), nAddress, nValue);
		}
	}
}

static UINT8 __fastcall mazingerZRead(UINT16 a)
{
	switch (a) {
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 Read => %04X\n"), a);
		}
	}

	return 0;
}

static void __fastcall mazingerZWrite(UINT16 a, UINT8 d)
{
	switch (a) {
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 Write => %04X, %02X\n"), a, d);
		}
	}
}

static INT32 DrvExit()
{
	EEPROMExit();

	MSM6295Exit(0);

	CaveTileExit();
	CaveSpriteExit();
    CavePalExit();

	SekExit();				// Deallocate 68000s
	ZetExit();

	BurnYM2203Exit();

	SoundLatch = 0;
	DrvZ80Bank = 0;
	DrvOkiBank1 = 0;
	DrvOkiBank2 = 0;

	BurnFree(Mem);

	return 0;
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (RamStart, 0, RamEnd - RamStart);
	}

	SekOpen(0);
	SekReset();
	SekClose();

	ZetOpen(0);
	ZetReset();
	BurnYM2203Reset();
	ZetClose();

	MSM6295Reset(0);

	EEPROMReset();

	BurnWatchdogResetEnable();

	HiscoreReset();

	nVideoIRQ = 1;
	nSoundIRQ = 1;
	nUnknownIRQ = 1;

	nIRQPending = 0;

	SoundLatch = 0;
	DrvZ80Bank = 0;
	DrvOkiBank1 = 0;
	DrvOkiBank2 = 0;
	msm6295_bank();

	SoundLatch = 0;
	SoundLatchStatus = 0x0C;

	return 0;
}

static INT32 DrvDraw()
{
	if (CaveRecalcPalette) {
		CavePalUpdate8Bit(0x4400, 12);
		CaveRecalcPalette = 1;
	}
	CavePalUpdate4Bit(0, 64);

	CaveClearScreen(CavePalette[0x3F00]);

	if (bDrawScreen) {
//		CaveGetBitmap();

		CaveTileRender(1);					// Render tiles
	}

	return 0;
}

static INT32 DrvFrame()
{
	INT32 nCyclesVBlank;
	INT32 nInterleave = 80;
	INT32 nCyclesSegment;

	BurnWatchdogUpdate();

	if (DrvReset) {														// Reset machine
		DrvDoReset(1);
	}

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
	ZetNewFrame();

	SekOpen(0);
	ZetOpen(0);

	nCyclesTotal[0] = (INT32)((INT64)16000000 * nBurnCPUSpeedAdjust / (0x0100 * CAVE_REFRESHRATE));
	nCyclesTotal[1] = (INT32)(4000000 / CAVE_REFRESHRATE);
	nCyclesDone[0] = nCyclesDone[1] = 0;

	nCyclesVBlank = nCyclesTotal[0] - (INT32)((nCyclesTotal[0] * CAVE_VBLANK_LINES) / 271.5);
	bVBlank = false;

	for (INT32 i = 1; i <= nInterleave; i++) {
    	INT32 nCurrentCPU = 0;
		INT32 nNext = i * nCyclesTotal[nCurrentCPU] / nInterleave;

		// Run 68000

		// See if we need to trigger the VBlank interrupt
		if (!bVBlank && nNext > nCyclesVBlank) {
			if (nCyclesDone[nCurrentCPU] < nCyclesVBlank) {
				nCyclesSegment = nCyclesVBlank - nCyclesDone[nCurrentCPU];
				nCyclesDone[nCurrentCPU] += SekRun(nCyclesSegment);
			}

			if (pBurnDraw != NULL) {
				DrvDraw();												// Draw screen if needed
			}

			CaveSpriteBuffer();
			UINT8 Temp = nCaveSpriteBank;
			nCaveSpriteBank = nCaveSpriteBankDelay;
			nCaveSpriteBankDelay = Temp;

			bVBlank = true;
			nVideoIRQ = 0;
			nUnknownIRQ = 0;
			UpdateIRQStatus();
		}

		nCyclesSegment = nNext - nCyclesDone[nCurrentCPU];
		nCyclesDone[nCurrentCPU] += SekRun(nCyclesSegment);

		BurnTimerUpdate(i * (nCyclesTotal[1] / nInterleave));
	}

	SekClose();

	BurnTimerEndFrame(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();

	return 0;
}

// This routine is called first to determine how much memory is needed (MemEnd-(UINT8 *)0),
// and then afterwards to set up all the pointers
static INT32 MemIndex()
{
	UINT8* Next; Next = Mem;
	Rom01			= Next; Next += 0x100000;		// 68K program
	RomZ80			= Next; Next += 0x020000;
	CaveSpriteROM	= Next; Next += 0x800000;
	CaveTileROM[0]	= Next; Next += 0x400000;		// Tile layer 0
	CaveTileROM[1]	= Next; Next += 0x400000;		// Tile layer 1

	MSM6295ROM		= Next;
	DrvSndROM		= Next; Next += 0x080000;

	DefEEPROM               = Next; Next += 0x000080;
	RamStart		= Next;
	Ram01			= Next; Next += 0x010000;		// CPU #0 work RAM
	RamZ80			= Next; Next += 0x001000;
	CaveTileRAM[0]	= Next; Next += 0x008000;
	CaveTileRAM[1]	= Next; Next += 0x008000;
	CaveSpriteRAM	= Next; Next += 0x010000;
	CavePalSrc		= Next; Next += 0x010000;		// palette
	RamEnd			= Next;
	MemEnd			= Next;

	return 0;
}

static void NibbleSwap1(UINT8* pData, INT32 nLen)
{
	UINT8* pOrg = pData + nLen - 1;
	UINT8* pDest = pData + ((nLen - 1) << 1);

	for (INT32 i = 0; i < nLen; i++, pOrg--, pDest -= 2) {
		pDest[0] = *pOrg & 15;
		pDest[1] = *pOrg >> 4;
	}

	return;
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
	BurnLoadRom(Rom01 + 0x00000, 0, 1);
	BurnLoadRom(Rom01 + 0x80000, 1, 1);

	BurnLoadRom(RomZ80, 2, 1);

	UINT8 *pTemp = (UINT8*)BurnMalloc(0x400000);
	BurnLoadRom(pTemp + 0x000000, 3, 1);
	BurnLoadRom(pTemp + 0x200000, 4, 1);
	for (INT32 i = 0; i < 0x400000; i++) {
		CaveSpriteROM[i ^ 0xdf88] = pTemp[BITSWAP24(i,23,22,21,20,19,9,7,3,15,4,17,14,18,2,16,5,11,8,6,13,1,10,12,0)];
	}
	BurnFree(pTemp);
	NibbleSwap1(CaveSpriteROM, 0x400000);

	BurnLoadRom(CaveTileROM[0], 5, 1);
	NibbleSwap2(CaveTileROM[0], 0x200000);

	pTemp = (UINT8*)BurnMalloc(0x200000);
	BurnLoadRom(pTemp, 6, 1);
	for (INT32 i = 0; i < 0x0100000; i++) {
		CaveTileROM[1][(i << 1) + 1] = (pTemp[(i << 1) + 0] & 15) | ((pTemp[(i << 1) + 1] & 15) << 4);
		CaveTileROM[1][(i << 1) + 0] = (pTemp[(i << 1) + 0] >> 4) | (pTemp[(i << 1) + 1] & 240);
	}
	BurnFree(pTemp);

	// Load MSM6295 ADPCM data
	BurnLoadRom(DrvSndROM, 7, 1);

	BurnLoadRom(DefEEPROM, 8, 1);

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

		SekScan(nAction);
		ZetScan(nAction);

		ZetOpen(0);
		BurnYM2203Scan(nAction, pnMin);
		ZetClose();

		MSM6295Scan(nAction, pnMin);

		SCAN_VAR(nVideoIRQ);
		SCAN_VAR(nSoundIRQ);
		SCAN_VAR(nUnknownIRQ);
		SCAN_VAR(bVBlank);

		CaveScanGraphics();

		SCAN_VAR(DrvInput);
		SCAN_VAR(SoundLatch);
		SCAN_VAR(DrvZ80Bank);
		SCAN_VAR(DrvOkiBank1);
		SCAN_VAR(DrvOkiBank2);

		BurnWatchdogScan(nAction);

		if (nAction & ACB_WRITE) {
			ZetOpen(0);
			ZetMapMemory(RomZ80 + (DrvZ80Bank * 0x4000), 0x4000, 0x7FFF, MAP_ROM);
			ZetClose();

			msm6295_bank();
		}
	}

	return 0;
}

static void DrvFMIRQHandler(INT32, INT32 nStatus)
{
	ZetSetIRQLine(0, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 drvZInit()
{
	ZetInit(0);
	ZetOpen(0);
	ZetSetInHandler(mazingerZIn);
	ZetSetOutHandler(mazingerZOut);
	ZetSetReadHandler(mazingerZRead);
	ZetSetWriteHandler(mazingerZWrite);

	// ROM bank 1
	ZetMapMemory(RomZ80 + 0x0000, 0x0000, 0x3FFF, MAP_ROM);
	// ROM bank 2
	ZetMapMemory(RomZ80 + 0x4000, 0x4000, 0x7FFF, MAP_ROM);
	// RAM
	ZetMapMemory(RamZ80 + 0x0000, 0xc000, 0xc7FF, MAP_RAM);
	ZetMapMemory(RamZ80 + 0x0800, 0xf800, 0xffFF, MAP_RAM);
	ZetClose();

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

	{
		SekInit(0, 0x68000);													// Allocate 68000
	    SekOpen(0);

		// Map 68000 memory:
		SekMapMemory(Rom01,				0x000000, 0x07FFFF, MAP_ROM);	// CPU 0 ROM
		SekMapMemory(Ram01,				0x100000, 0x10FFFF, MAP_RAM);
		SekMapMemory(CaveSpriteRAM,			0x200000, 0x20FFFF, MAP_RAM);
		SekMapMemory(CaveTileRAM[1] + 0x4000,		0x400000, 0x403FFF, MAP_RAM);
		SekMapMemory(CaveTileRAM[1] + 0x4000,		0x404000, 0x407FFF, MAP_RAM);
		SekMapMemory(CaveTileRAM[0] + 0x4000,		0x500000, 0x503FFF, MAP_RAM);
		SekMapMemory(CaveTileRAM[0] + 0x4000,		0x504000, 0x507FFF, MAP_RAM);
		SekMapMemory(CavePalSrc,		   	0xC08000, 0xc087FF, MAP_RAM);	// Palette RAM
		SekMapMemory(CavePalSrc + 0x8800,		0xC08800, 0xC0FFFF, MAP_ROM);	// Palette RAM (write goes through handler)
		SekMapHandler(1,				0xC08800, 0xC0FFFF, MAP_WRITE);	//

		SekMapMemory(Rom01 + 0x80000,			0xD00000, 0xD7FFFF, MAP_ROM);	// CPU 0 ROM

		SekSetReadByteHandler(0, mazingerReadByte);
		SekSetWriteByteHandler(0, mazingerWriteByte);
		SekSetReadWordHandler(0, mazingerReadWord);
		SekSetWriteWordHandler(0, mazingerWriteWord);

		SekSetWriteWordHandler(1, mazingerWriteWordPalette);
		SekSetWriteByteHandler(1, mazingerWriteBytePalette);
		SekClose();
	}

	drvZInit();

	CavePalInit(0x8000);
	CaveTileInit();
	CaveSpriteInit(2, 0x0800000);
	CaveTileInitLayer(0, 0x400000, 8, 0x0000);
	CaveTileInitLayer(1, 0x400000, 6, 0x4400);

	BurnWatchdogInit(DrvDoReset, 180 /*NOT REALLY*/);

	BurnYM2203Init(1, 4000000, &DrvFMIRQHandler, 0);
	BurnTimerAttachZet(4000000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE, 0.60, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.20, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.20, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.20, BURN_SND_ROUTE_BOTH);

	MSM6295Init(0, 1056000 / 132, 1);
	MSM6295SetRoute(0, 2.00, BURN_SND_ROUTE_BOTH);

	EEPROMInit(&eeprom_interface_93C46);
	if (!EEPROMAvailable()) EEPROMFill(DefEEPROM, 0, 0x80);

	bDrawScreen = true;

	DrvDoReset(1); // Reset machine

	return 0;
}

// Rom information
static struct BurnRomInfo mazingerRomDesc[] = {
	{ "mzp-0.u24",    0x080000, 0x43a4279f, BRF_ESS | BRF_PRG }, //  0 CPU #0 code
	{ "mzp-1.924",    0x080000, 0xdb40acba, BRF_ESS | BRF_PRG }, //  1

	{ "mzs.u21",      0x020000, 0xc5b4f7ed, BRF_ESS | BRF_PRG }, //  2 Z80 Code

	{ "bp943a-2.u56", 0x200000, 0x97e13959, BRF_GRA },	     //  3 Sprite data
	{ "bp943a-3.u55", 0x080000, 0x9c4957dd, BRF_GRA },	     //  4

	{ "bp943a-1.u60", 0x200000, 0x46327415, BRF_GRA },			 //  5 Layer 0 Tile data
	{ "bp943a-0.u63", 0x200000, 0xc1fed98a, BRF_GRA },			 //  6 Layer 1 Tile data

	{ "bp943a-4.u64", 0x080000, 0x3fc7f29a, BRF_SND },			 //  7 MSM6295 #1 ADPCM data

	{ "mazinger_world.nv", 0x0080, 0x4f6225c6, BRF_ESS | BRF_PRG },
};


STD_ROM_PICK(mazinger)
STD_ROM_FN(mazinger)

static struct BurnRomInfo mazingerjRomDesc[] = {
	{ "mzp-0.u24",    0x080000, 0x43a4279f, BRF_ESS | BRF_PRG }, //  0 CPU #0 code
	{ "mzp-1.924",    0x080000, 0xdb40acba, BRF_ESS | BRF_PRG }, //  1

	{ "mzs.u21",      0x020000, 0xc5b4f7ed, BRF_ESS | BRF_PRG }, //  2 Z80 Code

	{ "bp943a-2.u56", 0x200000, 0x97e13959, BRF_GRA },	     //  3 Sprite data
	{ "bp943a-3.u55", 0x080000, 0x9c4957dd, BRF_GRA },	     //  4

	{ "bp943a-1.u60", 0x200000, 0x46327415, BRF_GRA },			 //  5 Layer 0 Tile data
	{ "bp943a-0.u63", 0x200000, 0xc1fed98a, BRF_GRA },			 //  6 Layer 1 Tile data

	{ "bp943a-4.u64", 0x080000, 0x3fc7f29a, BRF_SND },			 //  7 MSM6295 #1 ADPCM data

	{ "mazinger_japan.nv", 0x0080, 0xf84a2a45, BRF_ESS | BRF_PRG },
};


STD_ROM_PICK(mazingerj)
STD_ROM_FN(mazingerj)

struct BurnDriver BurnDrvmazinger = {
	"mazinger", NULL, NULL, NULL, "1994",
	"Mazinger Z (World, ver. 94/06/27)\0", NULL, "Banpresto / Dynamic Pl. Toei Animation", "Cave",
	L"Mazinger Z\0\u30DE\u30B8\u30F3\u30AC\u30FC \uFF3A (World, ver. 94/06/27)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_16BIT_ONLY | BDF_HISCORE_SUPPORTED, 2, HARDWARE_CAVE_68K_Z80, GBF_VERSHOOT, 0,
	NULL, mazingerRomInfo, mazingerRomName, NULL, NULL, NULL, NULL, mazingerInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&CaveRecalcPalette, 0x8000, 240, 384, 3, 4
};

struct BurnDriver BurnDrvmazingerj = {
	"mazingerj", "mazinger", NULL, NULL, "1994",
	"Mazinger Z (Japan, ver. 94/06/27)\0", NULL, "Banpresto / Dynamic Pl. Toei Animation", "Cave",
	L"Mazinger Z\0\u30DE\u30B8\u30F3\u30AC\u30FC \uFF3A (Japan, ver. 94/06/27)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_16BIT_ONLY | BDF_HISCORE_SUPPORTED, 2, HARDWARE_CAVE_68K_Z80, GBF_VERSHOOT, 0,
	NULL, mazingerjRomInfo, mazingerjRomName, NULL, NULL, NULL, NULL, mazingerInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&CaveRecalcPalette, 0x8000, 240, 384, 3, 4
};
