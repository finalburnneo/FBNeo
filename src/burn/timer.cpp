// Timers (for Yamaha FM chips and generic)
#include "burnint.h"
#include "timer.h"

#define MAX_TIMER_VALUE ((1 << 30) - 65536)

double dTime;									// Time elapsed since the emulated machine was started

#define TIMER_MAX 8
static INT32 nTimerCount[TIMER_MAX], nTimerStart[TIMER_MAX];

// Callbacks
static INT32 (*pTimerOverCallback)(INT32, INT32);
static double (*pTimerTimeCallback)();

INT32 BurnTimerCPUClockspeed = 0;
INT32 (*BurnTimerCPUTotalCycles)() = NULL;

static INT32 (*pCPURun)(INT32) = NULL;
static void (*pCPURunEnd)() = NULL;

// ---------------------------------------------------------------------------
// Running time

static double BurnTimerTimeCallbackDummy()
{
	return 0.0;
}

extern "C" double BurnTimerGetTime()
{
	return dTime + pTimerTimeCallback();
}

// ---------------------------------------------------------------------------
// Update timers

static INT32 nTicksTotal, nTicksDone, nTicksExtra;

INT32 BurnTimerUpdate(INT32 nCycles)
{
	INT32 nIRQStatus = 0;

	nTicksTotal = MAKE_TIMER_TICKS(nCycles, BurnTimerCPUClockspeed);

//	bprintf(PRINT_NORMAL, _T(" -- Ticks: %08X, cycles %i\n"), nTicksTotal, nCycles);

	while (nTicksDone < nTicksTotal) {
		INT32 nTimer, nFirstTimer, nCyclesSegment, nTicksSegment;

		// Determine which timer fires first
		nFirstTimer = 0;
		for (INT32 i = 0; i < TIMER_MAX; i++) {
			if (nTimerCount[i] < nTimerCount[nFirstTimer]) {
				nFirstTimer = i;
			}
		}

		nTicksSegment = nTimerCount[nFirstTimer];

		if (nTicksSegment > nTicksTotal) {
			nTicksSegment = nTicksTotal;
		}

		nCyclesSegment = MAKE_CPU_CYLES(nTicksSegment + nTicksExtra, BurnTimerCPUClockspeed);
//		bprintf(PRINT_NORMAL, _T("  - Timer: %08X, %08X, %08X, cycles %i, %i\n"), nTicksDone, nTicksSegment, nTicksTotal, nCyclesSegment, BurnTimerCPUTotalCycles());

		pCPURun(nCyclesSegment - BurnTimerCPUTotalCycles());

		nTicksDone = MAKE_TIMER_TICKS(BurnTimerCPUTotalCycles() + 1, BurnTimerCPUClockspeed) - 1;
//		bprintf(PRINT_NORMAL, _T("  - ticks done -> %08X cycles -> %i\n"), nTicksDone, BurnTimerCPUTotalCycles());

		nTimer = 0;

		for (INT32 i = 0; i < TIMER_MAX; i++) {
			if (nTicksDone >= nTimerCount[i]) {
				if (nTimerStart[i] == MAX_TIMER_VALUE) {
					nTimerCount[i] = MAX_TIMER_VALUE;
				} else {
					nTimerCount[i] += nTimerStart[i];
				}
				//bprintf(PRINT_NORMAL, _T("  - timer %d fired\n"), i);

				nIRQStatus |= pTimerOverCallback(i>>1, i&1);
			}
		}
	}

	return nIRQStatus;
}

void BurnTimerEndFrame(INT32 nCycles)
{
	INT32 nTicks = MAKE_TIMER_TICKS(nCycles, BurnTimerCPUClockspeed);

	BurnTimerUpdate(nCycles);

	for (INT32 i = 0; i < TIMER_MAX; i++) {
		if (nTimerCount[i] < MAX_TIMER_VALUE) {
			nTimerCount[i] -= nTicks;
		}
	}

	nTicksDone -= nTicks;
	if (nTicksDone < 0) {
//		bprintf(PRINT_ERROR, _T(" -- ticks done -> %08X\n"), nTicksDone);
		nTicksDone = 0;
	}
}

void BurnTimerUpdateEnd()
{
//	bprintf(PRINT_NORMAL, _T("  - end %i\n"), BurnTimerCPUTotalCycles());

	pCPURunEnd();

	nTicksTotal = 0;
}


// ---------------------------------------------------------------------------
// Callbacks for the sound cores
/*
static INT32 BurnTimerExtraCallbackDummy()
{
	return 0;
}
*/
void BurnOPLTimerCallback(INT32 c, double period)
{
	pCPURunEnd();

	if (period == 0.0) {
		nTimerCount[c] = MAX_TIMER_VALUE;
//		bprintf(PRINT_NORMAL, _T("  - timer %i stopped\n"), c);
		return;
	}

	nTimerCount[c]  = (INT32)(period * (double)TIMER_TICKS_PER_SECOND);
	nTimerCount[c] += MAKE_TIMER_TICKS(BurnTimerCPUTotalCycles(), BurnTimerCPUClockspeed);

//	bprintf(PRINT_NORMAL, _T("  - timer %i started, %08X ticks (fires in %lf seconds)\n"), c, nTimerCount[c], period);
}

void BurnOPMTimerCallback(INT32 n, INT32 c, double period) // ym2151
{
	pCPURunEnd();
	
	if (period == 0.0) {
		nTimerCount[(n << 1) + c] = MAX_TIMER_VALUE;
		return;
	}

	nTimerCount[(n << 1) + c]  = (INT32)(period * (double)TIMER_TICKS_PER_SECOND);
	nTimerCount[(n << 1) + c] += MAKE_TIMER_TICKS(BurnTimerCPUTotalCycles(), BurnTimerCPUClockspeed);
}

void BurnOPNTimerCallback(INT32 n, INT32 c, INT32 cnt, double stepTime) // ym2203
{
	pCPURunEnd();
	
	if (cnt == 0) {
		nTimerCount[(n << 1) + c] = MAX_TIMER_VALUE;

//		bprintf(PRINT_NORMAL, _T("  - timer %i stopped\n"), c);

		return;
	}

	nTimerCount[(n << 1) + c]  = (INT32)(stepTime * cnt * (double)TIMER_TICKS_PER_SECOND);
	nTimerCount[(n << 1) + c] += MAKE_TIMER_TICKS(BurnTimerCPUTotalCycles(), BurnTimerCPUClockspeed);
	//bprintf(PRINT_NORMAL, _T("  - chip %i timer %i started, %08X ticks (fires in %lf seconds)\n"), n, c, nTimerCount[(n << 1) + c], stepTime * cnt);
}

void BurnYMFTimerCallback(INT32 /* n */, INT32 c, double period)
{
	pCPURunEnd();

	if (period == 0.0) {
		nTimerStart[c] = nTimerCount[c] = MAX_TIMER_VALUE;

//		bprintf(PRINT_NORMAL, _T("  - timer %i stopped\n"), c);

		return;
	}

	nTimerStart[c]  = nTimerCount[c] = (INT32)(period * (double)(TIMER_TICKS_PER_SECOND / 1000000));
	nTimerCount[c] += MAKE_TIMER_TICKS(BurnTimerCPUTotalCycles(), BurnTimerCPUClockspeed);

//	bprintf(PRINT_NORMAL, _T("  - timer %i started, %08X ticks (fires in %lf seconds)\n"), c, nTimerCount[c], period);
}

void BurnYMF262TimerCallback(INT32 /* n */, INT32 c, double period)
{
	pCPURunEnd();

	if (period == 0.0) {
		nTimerCount[c] = MAX_TIMER_VALUE;
		return;
	}

	nTimerCount[c]  = (INT32)(period * (double)TIMER_TICKS_PER_SECOND);
	nTimerCount[c] += MAKE_TIMER_TICKS(BurnTimerCPUTotalCycles(), BurnTimerCPUClockspeed);

//	bprintf(PRINT_NORMAL, _T("  - timer %i started, %08X ticks (fires in %lf seconds)\n"), c, nTimerCount[c], period);
}

void BurnTimerSetRetrig(INT32 c, double period)
{
	pCPURunEnd();

	if (period == 0.0) {
		nTimerStart[c] = nTimerCount[c] = MAX_TIMER_VALUE;

//		bprintf(PRINT_NORMAL, _T("  - timer %i stopped\n"), c);

		return;
	}

	nTimerStart[c]  = nTimerCount[c] = (INT32)(period * (double)(TIMER_TICKS_PER_SECOND));
	nTimerCount[c] += MAKE_TIMER_TICKS(BurnTimerCPUTotalCycles(), BurnTimerCPUClockspeed);

//	bprintf(PRINT_NORMAL, _T("  - timer %i started, %08X ticks (fires in %lf seconds)\n"), c, nTimerCount[c], period);
}

void BurnTimerSetOneshot(INT32 c, double period)
{
	pCPURunEnd();

	if (period == 0.0) {
		nTimerStart[c] = nTimerCount[c] = MAX_TIMER_VALUE;

//		bprintf(PRINT_NORMAL, _T("  - timer %i stopped\n"), c);

		return;
	}

	nTimerCount[c]  = (INT32)(period * (double)(TIMER_TICKS_PER_SECOND));
	nTimerCount[c] += MAKE_TIMER_TICKS(BurnTimerCPUTotalCycles(), BurnTimerCPUClockspeed);

//	bprintf(PRINT_NORMAL, _T("  - timer %i started, %08X ticks (fires in %lf seconds)\n"), c, nTimerCount[c], period / 1000000.0);
}

void BurnTimerSetRetrig(INT32 c, UINT64 timer_ticks)
{
	pCPURunEnd();

	if (timer_ticks == 0) {
		nTimerStart[c] = nTimerCount[c] = MAX_TIMER_VALUE;

		// bprintf(PRINT_NORMAL, L"  - timer %i stopped\n", c);

		return;
	}

	nTimerStart[c] = nTimerCount[c] = (UINT32)(timer_ticks);
	nTimerCount[c] += MAKE_TIMER_TICKS(BurnTimerCPUTotalCycles(), BurnTimerCPUClockspeed);

	// bprintf(PRINT_NORMAL, L"  - timer %i started, %08X ticks (fires in %lf seconds)\n", c, nTimerCount[c], (double)(TIMER_TICKS_PER_SECOND) / timer_ticks);
}

void BurnTimerSetOneshot(INT32 c, UINT64 timer_ticks)
{
	pCPURunEnd();

	if (timer_ticks == 0) {
		nTimerStart[c] = nTimerCount[c] = MAX_TIMER_VALUE;

		// bprintf(PRINT_NORMAL, L"  - timer %i stopped\n", c);

		return;
	}

	nTimerCount[c] = (UINT32)(timer_ticks);
	nTimerCount[c] += MAKE_TIMER_TICKS(BurnTimerCPUTotalCycles(), BurnTimerCPUClockspeed);

	// bprintf(PRINT_NORMAL, L"  - timer %i started, %08X ticks (fires in %lf seconds)\n", c, nTimerCount[c], (double)(TIMER_TICKS_PER_SECOND) / timer_ticks);
}

// ------------------------------------ ---------------------------------------
// Initialisation etc.

void BurnTimerScan(INT32 nAction, INT32* pnMin)
{
	if (pnMin && *pnMin < 0x029521) {
		*pnMin = 0x029521;
	}

	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(nTimerCount);
		SCAN_VAR(nTimerStart);
		SCAN_VAR(dTime);

		SCAN_VAR(nTicksDone);
	}
}

void BurnTimerExit()
{
	BurnTimerCPUClockspeed = 0;
	BurnTimerCPUTotalCycles = NULL;
	pCPURun = NULL;
	pCPURunEnd = NULL;

	return;
}

void BurnTimerReset()
{
	for (INT32 i = 0; i < TIMER_MAX; i++) {
		nTimerCount[i] = nTimerStart[i] = MAX_TIMER_VALUE;
	}

	dTime = 0.0;

	nTicksDone = 0;
}

INT32 BurnTimerInit(INT32 (*pOverCallback)(INT32, INT32), double (*pTimeCallback)())
{
	BurnTimerExit();

	pTimerOverCallback = pOverCallback;
	pTimerTimeCallback = pTimeCallback ? pTimeCallback : BurnTimerTimeCallbackDummy;

	BurnTimerReset();

	return 0;
}

// Null CPU, for a FM timer that isn't attached to anything.
static INT32 NullCyclesTotal;

void NullNewFrame()
{
	NullCyclesTotal = 0;
}

INT32 NullTotalCycles()
{
	return NullCyclesTotal;
}

INT32 NullRun(const INT32 nCycles)
{
	NullCyclesTotal += nCycles;

	return nCycles;
}

void NullRunEnd()
{
}

INT32 BurnTimerAttach(cpu_core_config *ptr, INT32 nClockspeed)
{
	BurnTimerCPUClockspeed = nClockspeed;
	BurnTimerCPUTotalCycles = ptr->totalcycles;
	pCPURun = ptr->run;
	pCPURunEnd = ptr->runend;

	nTicksExtra = MAKE_TIMER_TICKS(1, BurnTimerCPUClockspeed) - 1;

//	bprintf(PRINT_NORMAL, _T("--- timer cpu speed %iHz, one cycle = %i ticks.\n"), nClockspeed, MAKE_TIMER_TICKS(1, BurnTimerCPUClockspeed));

	return 0;
}

INT32 BurnTimerAttachNull(INT32 nClockspeed)
{
	BurnTimerCPUClockspeed = nClockspeed;
	BurnTimerCPUTotalCycles = NullTotalCycles;
	pCPURun = NullRun;
	pCPURunEnd = NullRunEnd;

	nTicksExtra = MAKE_TIMER_TICKS(1, BurnTimerCPUClockspeed) - 1;

//	bprintf(PRINT_NORMAL, _T("--- timer cpu speed %iHz, one cycle = %i ticks.\n"), nClockspeed, MAKE_TIMER_TICKS(1, BurnTimerCPUClockspeed));

	return 0;
}
