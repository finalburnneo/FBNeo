// PSound (CPS1 sound)
#include "cps.h"
#include "driver.h"
extern "C" {
 #include "ym2151.h"
}

UINT8 PsndCode, PsndFade;						// Sound code/fade sent to the z80 program

static INT32 nPsndCyclesExtra;

static void drvYM2151IRQHandler(INT32 nStatus)
{
	ZetSetIRQLine(0, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

INT32 PsndInit()
{
	nCpsZ80Cycles = 4000000 * 100 / nBurnFPS;

	// Init PSound z80
	if (PsndZInit()!= 0) {
		return 1;
	}

	// Init PSound mixing (not critical if it fails)
	PsmInit();

	YM2151SetIrqHandler(0, &drvYM2151IRQHandler);

	BurnTimerAttachZet(4000000);

	PsndCode = 0; PsndFade = 0;

	nPsndCyclesExtra = 0;

	return 0;
}

INT32 PsndExit()
{
	PsmExit();
	PsndZExit();

	return 0;
}

INT32 PsndScan(INT32 nAction, INT32 *pnMin)
{
	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(nPsndCyclesExtra);
		PsndZScan(nAction, pnMin);							// Scan Z80
		SCAN_VAR(PsndCode); SCAN_VAR(PsndFade);		// Scan sound info
	}
	return 0;
}

void PsndNewFrame()
{
	ZetNewFrame();
	PsmNewFrame();

	ZetIdle(nPsndCyclesExtra % nCpsZ80Cycles);
	nPsndCyclesExtra = 0;
}

void PsndEndFrame()
{
	BurnTimerEndFrame(nCpsZ80Cycles);
	nPsndCyclesExtra = ZetTotalCycles() - nCpsZ80Cycles;
}

INT32 PsndSyncZ80(INT32 /*nCycles*/)
{
	INT32 nCycles = (INT64)SekTotalCycles() * nCpsZ80Cycles / nCpsCycles;

	if (nCycles <= ZetTotalCycles()) {
		return 0;
	}

	BurnTimerUpdate(nCycles);

	return 0;
}
