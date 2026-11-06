// CPS - Run
#include "cps.h"

// CPS2
INT32 Cps2Volume = 39;
INT32 Cps2DisableDigitalVolume = 0;
UINT8 Cps2VolUp;
UINT8 Cps2VolDwn;

UINT16 Cps2VolumeStates[40] = {
	0xf010, 0xf008, 0xf004, 0xf002, 0xf001, 0xe810, 0xe808, 0xe804, 0xe802, 0xe801,
	0xe410, 0xe408, 0xe404, 0xe402, 0xe401, 0xe210, 0xe208, 0xe204, 0xe202, 0xe201,
	0xe110, 0xe108, 0xe104, 0xe102, 0xe101, 0xe090, 0xe088, 0xe084, 0xe082, 0xe081,
	0xe050, 0xe048, 0xe044, 0xe042, 0xe041, 0xe030, 0xe028, 0xe024, 0xe022, 0xe021
};

// Inputs:
UINT8 CpsReset = 0;
UINT8 Cpi01A = 0, Cpi01C = 0, Cpi01E = 0;

static INT32 nInterrupt;

static const INT32 nFirstLine = 0x10;							// The first scanline of the display

INT32 nCpsCyclesExtra;

// Emulated frames start at the beginning of vblank (line 240), like the hardware/MAME
// frame boundary: inputs latched by the frontend are seen by the vblank irq right away,
// and the picture is drawn when vblank begins.

// CPS-B-21 raster counters (CPS2 and ganbare), as in MAME: counters #1/#2 are reloaded at
// line 0 and decremented every line, irq4 is raised when one reaches 0, after a delay of
// 2 * counter #3 pixels. Bit 15 of a write to #1/#2 reloads the counter immediately.
INT32 bCpsRasterCounters = 0;
static void CopyCpsReg(INT32 i);
static void LatchCpsFrg();
static void UnLatchCpsFrg();
static UINT16 nRasterCounter[3], nRasterReload[3];
static INT32 nRasterLineCycles;							// cycle at which the current line started

static INT32 CpsLineCycles(INT32 nFrameLine)
{
	return (INT32)((INT64)nFrameLine * nCpsCycles / nCpsNumScanlines);
}

UINT16 CpsRasterRead(INT32 nReg)
{
	if (nReg == 2) {
		INT32 nLineLen = CpsLineCycles(1);
		INT32 nHPos = (nLineLen > 0) ? (INT32)((INT64)(SekTotalCycles() - nRasterLineCycles) * 512 / nLineLen) : 0;
		if (nHPos < 0) nHPos = 0;
		if (nHPos > 511) nHPos = 511;
		return (((nRasterCounter[2] - nHPos / 2) << 1) & 0x1fe) | (BURN_ENDIAN_SWAP_INT16(*((UINT16*)(CpsReg + 0x4E))) & 1);
	}

	return nRasterCounter[nReg] & 0x1ff;
}

void CpsRasterWrite(INT32 nReg, UINT16 d)
{
	if (nReg == 2) {
		nRasterReload[2] = (d >> 1) & 0xff;
		return;
	}

	nRasterReload[nReg] = d & 0x1ff;
	if (d & 0x8000) {
		nRasterCounter[nReg] = nRasterReload[nReg];
	}
}

static void CpsRasterReset()
{
	nRasterCounter[0] = nRasterCounter[1] = nRasterCounter[2] = 0;
	nRasterReload[0] = nRasterReload[1] = 0x1ff;
	nRasterReload[2] = 0;
}

void CpsRasterScan()
{
	SCAN_VAR(nRasterCounter);
	SCAN_VAR(nRasterReload);
}

// Start a scanline: clock the counters, returns true if a raster irq is due on this line
static bool CpsRasterClock(INT32 nLine)
{
	bool bIrq = false;

	for (INT32 i = 0; i < 2; i++) {
		if (nLine == 0) {
			nRasterCounter[i] = nRasterReload[i];
		} else {
			nRasterCounter[i] = (nRasterCounter[i] - 1) & 0x1ff;
		}
		if (nRasterCounter[i] == 0) bIrq = true;
	}
	nRasterCounter[2] = nRasterReload[2];

	return bIrq;
}

// The irq ends the current slice with the registers as they are now, the new one starts
// at nLine (MAME starts it at nLine + 1 to give the irq handler time, not needed here as
// the registers of a slice are taken when it ends)
static void CpsRasterSplit(INT32 nLine)
{
	INT32 nStart = nLine - nFirstLine;
	if (Cps == 2 && nStart > 0 && nStart < 224 && nInterrupt < MAX_RASTER - 1) {
		CopyCpsReg(nInterrupt);
		nInterrupt++;
		nRasterline[nInterrupt] = nStart;
	}
}

// Run one frame from line 240, scanline by scanline, with the raster counters
static void CpsRunRasterFrame(INT32 nVBlankIRQ)
{
	nInterrupt = 0;
	memset(nRasterline, 0, sizeof(nRasterline));

	for (INT32 nFrameLine = 0; nFrameLine < nCpsNumScanlines; nFrameLine++) {
		INT32 nLine = (nFrameLine + nFirstLine + 224) % nCpsNumScanlines;
		INT32 nLineEnd = CpsLineCycles(nFrameLine + 1);

		nRasterLineCycles = CpsLineCycles(nFrameLine);

		bool bIrq = CpsRasterClock(nLine);

		if (nLine == nFirstLine + 224) {
			// raster irq at the same time as vblank: irq6
			if (bIrq && nRasterCounter[2] == 0) {
				SekSetIRQLine(6, CPU_IRQSTATUS_AUTO);
				bIrq = false;
			} else {
				SekSetIRQLine(nVBlankIRQ, CPU_IRQSTATUS_AUTO);
			}
			if (Cps == 2) LatchCpsFrg();
		}

		if (nLine == nFirstLine && Cps == 2) {
			UnLatchCpsFrg();
		}

		if (bIrq) {
			INT32 nIrqCycles = nRasterLineCycles + (INT32)((INT64)nRasterCounter[2] * 2 * CpsLineCycles(1) / 512);
			if (nIrqCycles > SekTotalCycles()) {
				SekRun(nIrqCycles - SekTotalCycles());
			}
			SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
			CpsRasterSplit(nLine);
		}

		if (nLineEnd > SekTotalCycles()) {
			SekRun(nLineEnd - SekTotalCycles());
		}
	}

	CopyCpsReg(nInterrupt);
}

INT32 CpsDrawSpritesInReverse = 0;

INT32 nCpsNumScanlines = 262;
INT32 Cps1VBlankIRQLine = 2;

CpsRunInitCallback CpsRunInitCallbackFunction = NULL;
CpsRunInitCallback CpsRunExitCallbackFunction = NULL;
CpsRunResetCallback CpsRunResetCallbackFunction = NULL;
CpsRunFrameStartCallback CpsRunFrameStartCallbackFunction = NULL;
CpsRunFrameMiddleCallback CpsRunFrameMiddleCallbackFunction = NULL;
CpsRunFrameEndCallback CpsRunFrameEndCallbackFunction = NULL;

static void CpsQSoundCheatSearchCallback()
{
	// Q-Sound Shared RAM ranges - not useful for cheat searching, and runs the Z80
	// in the handler, exclude it from cheat searching
	if (Cps == 2) {
		CheatSearchExcludeAddressRange(0x618000, 0x619FFF);
	}
	
	if (Cps1Qs == 1) {	
		CheatSearchExcludeAddressRange(0xF18000, 0xF19FFF);
		CheatSearchExcludeAddressRange(0xF1E000, 0xF1FFFF);
	}
}

static INT32 DrvReset()
{
	// Reset machine
	if (Cps == 2 || PangEEP || Cps1Qs == 1 || CpsBootlegEEPROM) EEPROMReset();

	SekOpen(0);
	SekReset();
	SekClose();

	if (((Cps & 1) && !Cps1DisablePSnd) || ((Cps == 2) && !Cps2DisableQSnd)) {
		ZetOpen(0);
		ZetReset();
		ZetClose();
	}

	CpsRasterReset();

	if (Cps == 2) {
		// CPS-2 uses Object Banks
		SekOpen(0);
		CpsMapObjectBanks(0);
		SekClose();
	}

	nCpsCyclesExtra = 0;

	clear_opposite.reset();

	if (((Cps == 2) && !Cps2DisableQSnd) || Cps1Qs == 1) {			// Sound init (QSound)
		QsndReset();
	}
	
	if (CpsRunResetCallbackFunction) {
		CpsRunResetCallbackFunction();
	}

	HiscoreReset();

	return 0;
}

static const eeprom_interface qsound_eeprom_interface =
{
	7,		/* address bits */
	8,		/* data bits */
	"0110",	/*  read command */
	"0101",	/* write command */
	"0111",	/* erase command */
	0,
	0,
	0,
	0
};

static const eeprom_interface cps2_eeprom_interface =
{
	6,		/* address bits */
	16,		/* data bits */
	"0110",	/*  read command */
	"0101",	/* write command */
	"0111",	/* erase command */
	0,
	0,
	0,
	0
};

INT32 CpsRunInit()
{
	SekInit(0, 0x68000);					// Allocate 68000
	
	if (CpsMemInit()) {						// Memory init
		return 1;
	}
	
	if (Cps == 2 || PangEEP) {
		EEPROMInit(&cps2_eeprom_interface);
	} else {
		if (Cps1Qs == 1 || CpsBootlegEEPROM) {
			EEPROMInit(&qsound_eeprom_interface);
		}
	}

	CpsRwInit();							// Registers setup

	if (Cps == 2) {
		bCpsRasterCounters = 1;
	}

	if (CpsPalInit()) {						// Palette init
		return 1;
	}
	if (CpsObjInit()) {						// Sprite init
		return 1;
	}

	if ((Cps & 1) && Cps1Qs == 0 && Cps1DisablePSnd == 0) {			// Sound init (MSM6295 + YM2151)
		if (PsndInit()) {
			return 1;
		}
	}

	if (((Cps == 2) && !Cps2DisableQSnd) || Cps1Qs == 1) {			// Sound init (QSound)
		if (QsndInit()) {
			return 1;
		}
		QsndSetRoute(BURN_SND_QSND_OUTPUT_1, 2.00, BURN_SND_ROUTE_LEFT);
		QsndSetRoute(BURN_SND_QSND_OUTPUT_2, 2.00, BURN_SND_ROUTE_RIGHT);
	}

	if (Cps == 2 || PangEEP || Cps1Qs == 1 || CpsBootlegEEPROM) EEPROMReset();
	
	if (CpsRunInitCallbackFunction) {
		CpsRunInitCallbackFunction();
	}
	
	DrvReset();

	//Init Draw Function
	DrawFnInit();
	
	pBurnDrvPalette = CpsPal;
	
	if (Cps == 2 || Cps1Qs == 1) {
		CheatSearchInitCallbackFunction = CpsQSoundCheatSearchCallback;
	}

	return 0;
}

INT32 CpsRunExit()
{
	if (Cps == 2 || PangEEP || Cps1Qs == 1 || CpsBootlegEEPROM) EEPROMExit();

	// Sound exit
	if (((Cps == 2) && !Cps2DisableQSnd) || Cps1Qs == 1) QsndExit();
	if (Cps != 2 && Cps1Qs == 0 && !Cps1DisablePSnd) PsndExit();

	// Graphics exit
	CpsObjExit();
	CpsPalExit();

	// Sprite Masking exit
	ZBuf = NULL;

	// Memory exit
	CpsRwExit();
	CpsMemExit();

	SekExit();
	
	if (CpsRunExitCallbackFunction) {
		CpsRunExitCallbackFunction();
		CpsRunExitCallbackFunction = NULL;
	}
	CpsRunInitCallbackFunction = NULL;
	CpsRunResetCallbackFunction = NULL;
	CpsRunFrameStartCallbackFunction = NULL;
	CpsRunFrameMiddleCallbackFunction = NULL;
	CpsRunFrameEndCallbackFunction = NULL;
	
	Cps1VBlankIRQLine = 2;
	
	Cps2DisableQSnd = 0;
	CpsBootlegEEPROM = 0;
	bCpsRasterCounters = 0;

	return 0;
}

static void CopyCpsReg(INT32 i)
{
	memcpy(CpsSaveReg[i], CpsReg, 0x0100);
}

// At VBL, latch the F-regs.  Don't make them active yet
// (we don't want re-draws / etc using them yet)
static void LatchCpsFrg()
{
	memcpy(CpsSaveFrg[1], CpsFrg, 0x0010);
}

// Begining of frame, make them active
static void UnLatchCpsFrg()
{
	memcpy(CpsSaveFrg[0], CpsSaveFrg[1], 0x0010);
}

INT32 Cps1Frame()
{
	INT32 nNext, i;

	if (CpsReset) {
		DrvReset();
	}

	SekNewFrame();
	if (Cps1Qs == 1) {
		QsndNewFrame();
	} else {
		if (!Cps1DisablePSnd) {
			ZetOpen(0);
			PsndNewFrame();
		}
	}

	nCpsCycles = (INT32)((INT64)nCPS68KClockspeed * nBurnCPUSpeedAdjust >> 8);

	CpsRwGetInp();												// Update the input port values

	if (CpsRunFrameStartCallbackFunction) {
		CpsRunFrameStartCallbackFunction();
	}

	SekOpen(0);

	SekIdle(nCpsCyclesExtra);
	nCpsCyclesExtra = 0;

	if (bCpsRasterCounters) {
		CpsRunRasterFrame(Cps1VBlankIRQLine);
	} else {
		SekSetIRQLine(Cps1VBlankIRQLine, CPU_IRQSTATUS_AUTO);	// VBlank: the frame starts at line 240

		for (i = 0; i < 4; i++) {
			nNext = ((i + 1) * nCpsCycles) >> 2;

			if (i == 2 && CpsRunFrameMiddleCallbackFunction) {
				CpsRunFrameMiddleCallbackFunction();
			}

			SekRun(nNext - SekTotalCycles());
		}
	}

	// Next vblank: latch the registers and the sprite list (drawn next frame, as the hardware
	// does with its sprite buffer; bootleg sprites are drawn right away), then draw the frame
	memcpy(CpsSaveReg[0], CpsReg, 0x100);
	CpsObjGet();
	if (pBurnDraw) {
		CpsDraw();
	}

	if (Cps1Qs == 1) {
		QsndEndFrame();
	} else {
		if (!Cps1DisablePSnd) {
			PsndSyncZ80(nCpsZ80Cycles);
			PsndEndFrame();
			PsmUpdateEnd();
			ZetClose();
		}
	}

	if (CpsRunFrameEndCallbackFunction) {
		CpsRunFrameEndCallbackFunction();
	}

	nCpsCyclesExtra = SekTotalCycles() - nCpsCycles;

	SekClose();

	return 0;
}

INT32 Cps2Frame()
{

	if (CpsReset) {
		DrvReset();
	}

//	extern INT32 prevline;
//	prevline = -1;

	SekNewFrame();
	if (!Cps2DisableQSnd) QsndNewFrame();

	nCpsCycles = (INT32)(((INT64)nCPS68KClockspeed * nBurnCPUSpeedAdjust) / 0x0100);
	SekOpen(0);
	SekSetCyclesScanline(nCpsCycles / nCpsNumScanlines);

	CpsRwGetInp();											// Update the input port values

	if (CpsRunFrameStartCallbackFunction) {
		CpsRunFrameStartCallbackFunction();
	}
	
	// Check the volumes every 5 frames or so
	if (GetCurrentFrame() % 5 == 0) {
		if (Cps2VolUp) Cps2Volume++;
		if (Cps2VolDwn) Cps2Volume--;
		
		if (Cps2Volume > 39) Cps2Volume = 39;
		if (Cps2Volume < 0) Cps2Volume = 0;

		double Cps2Amp = 1.00;

		if (strstr(BurnDrvGetTextA(DRV_NAME), "ddsom")) {
			Cps2Amp = 2.0;
		}

		QscSetRoute(BURN_SND_QSND_OUTPUT_1, (Cps2Volume / 39.0) * Cps2Amp, BURN_SND_ROUTE_LEFT);
		QscSetRoute(BURN_SND_QSND_OUTPUT_2, (Cps2Volume / 39.0) * Cps2Amp, BURN_SND_ROUTE_RIGHT);
	}
	
	SekIdle(nCpsCyclesExtra);
	nCpsCyclesExtra = 0;

	CpsRunRasterFrame(2);

	// Next vblank: latch the sprites (drawn next frame) and draw the frame just displayed
	CpsObjGet();
	if (pBurnDraw) {
		CpsDraw();
	}

	nCpsCyclesExtra = SekTotalCycles() - nCpsCycles;

	if (!Cps2DisableQSnd) QsndEndFrame();

	SekClose();

//	bprintf(PRINT_NORMAL, _T("    -\n"));

#if 0 && defined FBNEO_DEBUG
	if (nInterrupt) {
		bprintf(PRINT_IMPORTANT, _T("Beam synchronized interrupt at line %2X.\r"), nRasterline[nInterrupt]);
	} else {
		bprintf(PRINT_NORMAL, _T("Beam synchronized interrupt disabled.   \r"));
	}

	extern INT32 counter;
	if (counter) {
		bprintf(PRINT_NORMAL, _T("\n\nSlices start at: "));
		for (i = 0; i < MAX_RASTER + 2; i++) {
			bprintf(PRINT_NORMAL, _T("%2X "), nRasterline[i]);
		}
		bprintf(PRINT_NORMAL, _T("\n"));
		for (i = 0; i < 0x80; i++) {
			if (*((UINT16*)(CpsSaveReg[0] + i * 2)) != *((UINT16*)(CpsSaveReg[nInterrupt] + i * 2))) {
				bprintf(PRINT_NORMAL, _T("Register %2X: %4X -> %4X\n"), i * 2, *((UINT16*)(CpsSaveReg[0] + i * 2)), *((UINT16*)(CpsSaveReg[nInterrupt] + i * 2)));
			}
		}
		bprintf(PRINT_NORMAL, _T("\n"));
		for (i = 0; i < 0x010; i++) {
			if (CpsSaveFrg[0][i] != CpsSaveFrg[nInterrupt][i]) {
				bprintf(PRINT_NORMAL, _T("FRG %X: %02X -> %02X\n"), i, CpsSaveFrg[0][i], CpsSaveFrg[nInterrupt][i]);
			}
		}
		bprintf(PRINT_NORMAL, _T("\n"));
		if (((CpsSaveFrg[0][4] << 8) | CpsSaveFrg[0][5]) != ((CpsSaveFrg[nInterrupt][4] << 8) | CpsSaveFrg[nInterrupt][5])) {
			bprintf(PRINT_NORMAL, _T("Layer-sprite priority: %04X -> %04X\n"), ((CpsSaveFrg[0][4] << 8) | CpsSaveFrg[0][5]), ((CpsSaveFrg[nInterrupt][4] << 8) | CpsSaveFrg[nInterrupt][5]));
		}

		bprintf(PRINT_NORMAL, _T("\n"));
		for (INT32 j = 0; j <= nInterrupt; j++) {
			if (j) {
				bprintf(PRINT_NORMAL, _T("IRQ : %i (triggered at line %3i)\n\n"), j, nRasterline[j]);
			} else {
				bprintf(PRINT_NORMAL, _T("Initial register status\n\n"));
			}

			for (i = 0; i < 0x080; i+= 8) {
				bprintf(PRINT_NORMAL, _T("%2X: %4X %4X %4X %4X %4X %4X %4X %4X\n"), i * 2, *((UINT16*)(CpsSaveReg[j] + 0 + i * 2)), *((UINT16*)(CpsSaveReg[j] + 2 + i * 2)), *((UINT16*)(CpsSaveReg[j] + 4 + i * 2)), *((UINT16*)(CpsSaveReg[j] + 6 + i * 2)), *((UINT16*)(CpsSaveReg[j] + 8 + i * 2)), *((UINT16*)(CpsSaveReg[j] + 10 + i * 2)), *((UINT16*)(CpsSaveReg[j] + 12 + i * 2)), *((UINT16*)(CpsSaveReg[j] + 14 + i * 2)));
			}

			bprintf(PRINT_NORMAL, _T("\nFRG: "));
			for (i = 0; i < 0x010; i++) {
				bprintf(PRINT_NORMAL, _T("%02X "), CpsSaveFrg[j][i]);
			}
			bprintf(PRINT_NORMAL, _T("\n\n"));

		}

		extern INT32 bRunPause;
		bRunPause = 1;
		counter = 0;
	}
#endif

	return 0;
}

