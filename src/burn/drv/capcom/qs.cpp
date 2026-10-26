#include "cps.h"
// QSound

static INT32 nQsndCyclesExtra;

static INT32 qsndTimerOver(INT32, INT32)
{
//	bprintf(PRINT_NORMAL, _T("  - IRQ -> 1.\n"));
	ZetSetIRQLine(0xFF, CPU_IRQSTATUS_HOLD);

	return 0;
}

INT32 QsndInit()
{
	INT32 nRate;

	// Init QSound z80
	if (QsndZInit()) {
		return 1;
	}
	BurnTimerInit(qsndTimerOver, NULL);

	INT32 nClock = (Cps1Qs == 1) ? 8000000 : nQsndZ80Clock;
	nCpsZ80Cycles = nClock * 100 / nBurnFPS;
	BurnTimerAttachZet(nClock);

	if (nBurnSoundRate >= 0) {
		nRate = nBurnSoundRate;
	} else {
		nRate = 11025;
	}

	QscInit(nRate);		// Init QSound chip

	return 0;
}

void QsndSetRoute(INT32 nIndex, double nVolume, INT32 nRouteDir)
{
	QscSetRoute(nIndex, nVolume, nRouteDir);
}

void QsndReset()
{
	ZetOpen(0);
	BurnTimerReset();
	BurnTimerSetRetrig(0, 1.0 / 252.0);
	ZetClose();

	nQsndCyclesExtra = 0;
}

void QsndExit()
{
	QscExit();							// Exit QSound chip
	QsndZExit();
}

INT32 QsndScan(INT32 nAction, INT32 bScanHistory)
{
	if (nAction & ACB_DRIVER_DATA) {
		QsndZScan(nAction);				// Scan Z80
		QscScan(nAction, bScanHistory);				// Scan QSound Chip

		BurnTimerScan(nAction, NULL);
		SCAN_VAR(nQsndCyclesExtra);
	}

	return 0;
}

void QsndNewFrame()
{
	ZetNewFrame();

	ZetOpen(0);
	ZetIdle(nQsndCyclesExtra);

	QscNewFrame();
}

void QsndEndFrame()
{
	BurnTimerEndFrame(nCpsZ80Cycles);
	if (pBurnSoundOut) QscUpdate(nBurnSoundLen);

	nQsndCyclesExtra = ZetTotalCycles() - nCpsZ80Cycles;
	ZetClose();
}

void QsndSyncZ80()
{
	int nCycles = (INT64)SekTotalCycles() * nCpsZ80Cycles / nCpsCycles;

	if (nCycles <= ZetTotalCycles()) {
		return;
	}

	BurnTimerUpdate(nCycles);
}
