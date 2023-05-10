#include "burnint.h"
#include "burn_ym3812.h"

#define MAX_YM3812	2

// Timer Related

#define MAX_TIMER_VALUE ((1 << 30) - 65536)

static double dTimeYM3812;									// Time elapsed since the emulated machine was started

static INT32 nTimerCount[2], nTimerStart[2];

// Callbacks
static INT32 (*pTimerOverCallback)(INT32, INT32);
static double (*pTimerTimeCallback)();

static INT32 nCPUClockspeed = 0;
static INT32 (*pCPUTotalCycles)() = NULL;
static INT32 (*pCPURun)(INT32) = NULL;
static void (*pCPURunEnd)() = NULL;

// ---------------------------------------------------------------------------
// Running time

static double BurnTimerTimeCallbackDummy()
{
	return 0.0;
}

extern "C" double BurnTimerGetTimeYM3812()
{
	return dTimeYM3812 + pTimerTimeCallback();
}

// ---------------------------------------------------------------------------
// Update timers

static INT32 nTicksTotal, nTicksDone, nTicksExtra;

INT32 BurnTimerUpdateYM3812(INT32 nCycles)
{
	INT32 nIRQStatus = 0;

	nTicksTotal = MAKE_TIMER_TICKS(nCycles, nCPUClockspeed);

	while (nTicksDone < nTicksTotal) {
		INT32 nTimer, nCyclesSegment, nTicksSegment;

		// Determine which timer fires first
		if (nTimerCount[0] <= nTimerCount[1]) {
			nTicksSegment = nTimerCount[0];
		} else {
			nTicksSegment = nTimerCount[1];
		}
		if (nTicksSegment > nTicksTotal) {
			nTicksSegment = nTicksTotal;
		}

		nCyclesSegment = MAKE_CPU_CYLES(nTicksSegment + nTicksExtra, nCPUClockspeed);

		pCPURun(nCyclesSegment - pCPUTotalCycles());

		nTicksDone = MAKE_TIMER_TICKS(pCPUTotalCycles() + 1, nCPUClockspeed) - 1;

		nTimer = 0;
		if (nTicksDone >= nTimerCount[0]) {
			if (nTimerStart[0] == MAX_TIMER_VALUE) {
				nTimerCount[0] = MAX_TIMER_VALUE;
			} else {
				nTimerCount[0] += nTimerStart[0];
			}
			nTimer |= 1;
		}
		if (nTicksDone >= nTimerCount[1]) {
			if (nTimerStart[1] == MAX_TIMER_VALUE) {
				nTimerCount[1] = MAX_TIMER_VALUE;
			} else {
				nTimerCount[1] += nTimerStart[1];
			}
			nTimer |= 2;
		}
		if (nTimer & 1) {
			nIRQStatus |= pTimerOverCallback(0, 0);
		}
		if (nTimer & 2) {
			nIRQStatus |= pTimerOverCallback(0, 1);
		}
	}

	return nIRQStatus;
}

void BurnTimerEndFrameYM3812(INT32 nCycles)
{
	INT32 nTicks = MAKE_TIMER_TICKS(nCycles, nCPUClockspeed);

	BurnTimerUpdateYM3812(nCycles);

	if (nTimerCount[0] < MAX_TIMER_VALUE) {
		nTimerCount[0] -= nTicks;
	}
	if (nTimerCount[1] < MAX_TIMER_VALUE) {
		nTimerCount[1] -= nTicks;
	}

	nTicksDone -= nTicks;
	if (nTicksDone < 0) {
		nTicksDone = 0;
	}
}

void BurnTimerUpdateEndYM3812()
{
	pCPURunEnd();

	nTicksTotal = 0;
}

void BurnOPLTimerCallbackYM3812(INT32 c, double period)
{
	pCPURunEnd();

	if (period == 0.0) {
		nTimerCount[c] = MAX_TIMER_VALUE;
		return;
	}

	nTimerCount[c]  = (INT32)(period * (double)TIMER_TICKS_PER_SECOND);
	nTimerCount[c] += MAKE_TIMER_TICKS(pCPUTotalCycles(), nCPUClockspeed);
}

void BurnTimerScanYM3812(INT32 nAction, INT32* pnMin)
{
	if (pnMin && *pnMin < 0x029521) {
		*pnMin = 0x029521;
	}

	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(nTimerCount);
		SCAN_VAR(nTimerStart);
		SCAN_VAR(dTimeYM3812);

		SCAN_VAR(nTicksDone);
	}
}

void BurnTimerExitYM3812()
{
	nCPUClockspeed = 0;
	pCPUTotalCycles = NULL;
	pCPURun = NULL;
	pCPURunEnd = NULL;

	return;
}

void BurnTimerResetYM3812()
{
	nTimerCount[0] = nTimerCount[1] = MAX_TIMER_VALUE;
	nTimerStart[0] = nTimerStart[1] = MAX_TIMER_VALUE;

	dTimeYM3812 = 0.0;

	nTicksDone = 0;
}

INT32 BurnTimerInitYM3812(INT32 (*pOverCallback)(INT32, INT32), double (*pTimeCallback)())
{
	BurnTimerExitYM3812();

	pTimerOverCallback = pOverCallback;
	pTimerTimeCallback = pTimeCallback ? pTimeCallback : BurnTimerTimeCallbackDummy;

	BurnTimerResetYM3812();

	return 0;
}

INT32 BurnTimerAttachYM3812(cpu_core_config *ptr, INT32 nClockspeed)
{
	nCPUClockspeed = nClockspeed;
	pCPUTotalCycles = ptr->totalcycles;
	pCPURun = ptr->run;
	pCPURunEnd = ptr->runend;

	nTicksExtra = MAKE_TIMER_TICKS(1, nCPUClockspeed) - 1;

	return 0;
}

static INT32 YM3812SynchroniseStream(INT32 nSoundRate)
{
	return (INT64)(pCPUTotalCycles() * nSoundRate / nCPUClockspeed);
}

// Sound Related

void (*BurnYM3812Update)(INT16* pSoundBuf, INT32 nSegmentEnd);

static INT32 (*BurnYM3812StreamCallback)(INT32 nSoundRate);

static INT32 nBurnYM3812SoundRate;

static INT16* pBuffer;
static INT16* pYM3812Buffer[2 * MAX_YM3812];

static INT32 nYM3812Position;

static UINT32 nSampleSize;
static INT32 nFractionalPosition;

static INT32 nNumChips = 0;
static INT32 bYM3812AddSignal;

static double YM3812Volumes[1 * MAX_YM3812];
static INT32 YM3812RouteDirs[1 * MAX_YM3812];

// ----------------------------------------------------------------------------
// Execute YM3812 for part of a frame

static void YM3812Render(INT32 nSegmentLength)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YM3812Initted) bprintf(PRINT_ERROR, _T("YM3812Render called without init\n"));
#endif

	if (nYM3812Position >= nSegmentLength || !pBurnSoundOut) {
		return;
	}

	nSegmentLength -= nYM3812Position;

	YM3812UpdateOne(0, pBuffer + 0 * 4096 + 4 + nYM3812Position, nSegmentLength);
	
	if (nNumChips > 1) {
		YM3812UpdateOne(1, pBuffer + 1 * 4096 + 4 + nYM3812Position, nSegmentLength);
	}

	nYM3812Position += nSegmentLength;
}

// ----------------------------------------------------------------------------
// Update the sound buffer

static void YM3812UpdateResample(INT16* pSoundBuf, INT32 nSegmentEnd)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YM3812Initted) bprintf(PRINT_ERROR, _T("YM3812UpdateResample called without init\n"));
#endif

	INT32 nSegmentLength = nSegmentEnd;
	INT32 nSamplesNeeded = nSegmentEnd * nBurnYM3812SoundRate / nBurnSoundRate + 1;

	if (nSamplesNeeded < nYM3812Position) {
		nSamplesNeeded = nYM3812Position;
	}

	if (nSegmentLength > nBurnSoundLen) {
		nSegmentLength = nBurnSoundLen;
	}
	nSegmentLength <<= 1;

	YM3812Render(nSamplesNeeded);

	pYM3812Buffer[0] = pBuffer + 0 * 4096 + 4;
	if (nNumChips > 1) {
		pYM3812Buffer[1] = pBuffer + 1 * 4096 + 4;
	}

	for (INT32 i = (nFractionalPosition & 0xFFFF0000) >> 15; i < nSegmentLength; i += 2, nFractionalPosition += nSampleSize) {
		INT32 nLeftSample[4] = {0, 0, 0, 0};
		INT32 nRightSample[4] = {0, 0, 0, 0};
		INT32 nTotalLeftSample, nTotalRightSample;
		
		if ((YM3812RouteDirs[0 + BURN_SND_YM3812_ROUTE] & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) {
			nLeftSample[0] += (INT32)(pYM3812Buffer[0][(nFractionalPosition >> 16) - 3] * YM3812Volumes[0 + BURN_SND_YM3812_ROUTE]);
			nLeftSample[1] += (INT32)(pYM3812Buffer[0][(nFractionalPosition >> 16) - 2] * YM3812Volumes[0 + BURN_SND_YM3812_ROUTE]);
			nLeftSample[2] += (INT32)(pYM3812Buffer[0][(nFractionalPosition >> 16) - 1] * YM3812Volumes[0 + BURN_SND_YM3812_ROUTE]);
			nLeftSample[3] += (INT32)(pYM3812Buffer[0][(nFractionalPosition >> 16) - 0] * YM3812Volumes[0 + BURN_SND_YM3812_ROUTE]);
		}
		if ((YM3812RouteDirs[0 + BURN_SND_YM3812_ROUTE] & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) {
			nRightSample[0] += (INT32)(pYM3812Buffer[0][(nFractionalPosition >> 16) - 3] * YM3812Volumes[0 + BURN_SND_YM3812_ROUTE]);
			nRightSample[1] += (INT32)(pYM3812Buffer[0][(nFractionalPosition >> 16) - 2] * YM3812Volumes[0 + BURN_SND_YM3812_ROUTE]);
			nRightSample[2] += (INT32)(pYM3812Buffer[0][(nFractionalPosition >> 16) - 1] * YM3812Volumes[0 + BURN_SND_YM3812_ROUTE]);
			nRightSample[3] += (INT32)(pYM3812Buffer[0][(nFractionalPosition >> 16) - 0] * YM3812Volumes[0 + BURN_SND_YM3812_ROUTE]);
		}
		
		if (nNumChips > 1) {
			if ((YM3812RouteDirs[1 + BURN_SND_YM3812_ROUTE] & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) {
				nLeftSample[0] += (INT32)(pYM3812Buffer[1][(nFractionalPosition >> 16) - 3] * YM3812Volumes[1 + BURN_SND_YM3812_ROUTE]);
				nLeftSample[1] += (INT32)(pYM3812Buffer[1][(nFractionalPosition >> 16) - 2] * YM3812Volumes[1 + BURN_SND_YM3812_ROUTE]);
				nLeftSample[2] += (INT32)(pYM3812Buffer[1][(nFractionalPosition >> 16) - 1] * YM3812Volumes[1 + BURN_SND_YM3812_ROUTE]);
				nLeftSample[3] += (INT32)(pYM3812Buffer[1][(nFractionalPosition >> 16) - 0] * YM3812Volumes[1 + BURN_SND_YM3812_ROUTE]);
			}
			if ((YM3812RouteDirs[1 + BURN_SND_YM3812_ROUTE] & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) {
				nRightSample[0] += (INT32)(pYM3812Buffer[1][(nFractionalPosition >> 16) - 3] * YM3812Volumes[1 + BURN_SND_YM3812_ROUTE]);
				nRightSample[1] += (INT32)(pYM3812Buffer[1][(nFractionalPosition >> 16) - 2] * YM3812Volumes[1 + BURN_SND_YM3812_ROUTE]);
				nRightSample[2] += (INT32)(pYM3812Buffer[1][(nFractionalPosition >> 16) - 1] * YM3812Volumes[1 + BURN_SND_YM3812_ROUTE]);
				nRightSample[3] += (INT32)(pYM3812Buffer[1][(nFractionalPosition >> 16) - 0] * YM3812Volumes[1 + BURN_SND_YM3812_ROUTE]);
			}
		}
		
		nTotalLeftSample = INTERPOLATE4PS_16BIT((nFractionalPosition >> 4) & 0x0fff, nLeftSample[0], nLeftSample[1], nLeftSample[2], nLeftSample[3]);
		nTotalRightSample = INTERPOLATE4PS_16BIT((nFractionalPosition >> 4) & 0x0fff, nRightSample[0], nRightSample[1], nRightSample[2], nRightSample[3]);
		
		nTotalLeftSample = BURN_SND_CLIP(nTotalLeftSample);
		nTotalRightSample = BURN_SND_CLIP(nTotalRightSample);
			
		if (bYM3812AddSignal) {
			pSoundBuf[i + 0] = BURN_SND_CLIP(pSoundBuf[i + 0] + nTotalLeftSample);
			pSoundBuf[i + 1] = BURN_SND_CLIP(pSoundBuf[i + 1] + nTotalRightSample);
		} else {
			pSoundBuf[i + 0] = nTotalLeftSample;
			pSoundBuf[i + 1] = nTotalRightSample;
		}
	}

	if (nSegmentEnd >= nBurnSoundLen) {
		INT32 nExtraSamples = nSamplesNeeded - (nFractionalPosition >> 16);

		for (INT32 i = -4; i < nExtraSamples; i++) {
			pYM3812Buffer[0][i] = pYM3812Buffer[0][(nFractionalPosition >> 16) + i];
			if (nNumChips > 1) {
				pYM3812Buffer[1][i] = pYM3812Buffer[1][(nFractionalPosition >> 16) + i];
			}
		}

		nFractionalPosition &= 0xFFFF;

		nYM3812Position = nExtraSamples;
	}
}

static void YM3812UpdateNormal(INT16* pSoundBuf, INT32 nSegmentEnd)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YM3812Initted) bprintf(PRINT_ERROR, _T("YM3812UpdateNormal called without init\n"));
#endif

	INT32 nSegmentLength = nSegmentEnd;

	if (nSegmentEnd < nYM3812Position) {
		nSegmentEnd = nYM3812Position;
	}

	if (nSegmentLength > nBurnSoundLen) {
		nSegmentLength = nBurnSoundLen;
	}

	YM3812Render(nSegmentEnd);

	pYM3812Buffer[0] = pBuffer + 4 + 0 * 4096;
	pYM3812Buffer[1] = pBuffer + 4 + 1 * 4096;

	for (INT32 n = nFractionalPosition; n < nSegmentLength; n++) {
		INT32 nLeftSample = 0, nRightSample = 0;
		
		if ((YM3812RouteDirs[0 + BURN_SND_YM3812_ROUTE] & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) {
			nLeftSample += (INT32)(pYM3812Buffer[0][n] * YM3812Volumes[0 + BURN_SND_YM3812_ROUTE]);
		}
		if ((YM3812RouteDirs[0 + BURN_SND_YM3812_ROUTE] & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) {
			nRightSample += (INT32)(pYM3812Buffer[0][n] * YM3812Volumes[0 + BURN_SND_YM3812_ROUTE]);
		}
		
		if (nNumChips > 1) {
			if ((YM3812RouteDirs[1 + BURN_SND_YM3812_ROUTE] & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) {
				nLeftSample += (INT32)(pYM3812Buffer[1][n] * YM3812Volumes[1 + BURN_SND_YM3812_ROUTE]);
			}
			if ((YM3812RouteDirs[1 + BURN_SND_YM3812_ROUTE] & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) {
				nRightSample += (INT32)(pYM3812Buffer[1][n] * YM3812Volumes[1 + BURN_SND_YM3812_ROUTE]);
			}
		}
		
		nLeftSample = BURN_SND_CLIP(nLeftSample);
		nRightSample = BURN_SND_CLIP(nRightSample);
			
		if (bYM3812AddSignal) {
			pSoundBuf[(n << 1) + 0] = BURN_SND_CLIP(pSoundBuf[(n << 1) + 0] + nLeftSample);
			pSoundBuf[(n << 1) + 1] = BURN_SND_CLIP(pSoundBuf[(n << 1) + 1] + nRightSample);
		} else {
			pSoundBuf[(n << 1) + 0] = nLeftSample;
			pSoundBuf[(n << 1) + 1] = nRightSample;
		}
	}

	nFractionalPosition = nSegmentLength;

	if (nSegmentEnd >= nBurnSoundLen) {
		INT32 nExtraSamples = nSegmentEnd - nBurnSoundLen;

		for (INT32 i = 0; i < nExtraSamples; i++) {
			pYM3812Buffer[0][i] = pYM3812Buffer[0][nBurnSoundLen + i];
			if (nNumChips > 1) {
				pYM3812Buffer[1][i] = pYM3812Buffer[1][nBurnSoundLen + i];
			}
		}

		nFractionalPosition = 0;

		nYM3812Position = nExtraSamples;

	}
}

// ----------------------------------------------------------------------------
// Callbacks for YM3812 core

void BurnYM3812UpdateRequest(INT32, INT32)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YM3812Initted) bprintf(PRINT_ERROR, _T("BurnYM3812UpdateRequest called without init\n"));
#endif

	YM3812Render(BurnYM3812StreamCallback(nBurnYM3812SoundRate));
}

// ----------------------------------------------------------------------------
// Initialisation, etc.

void BurnYM3812Reset()
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YM3812Initted) bprintf(PRINT_ERROR, _T("BurnYM3812Reset called without init\n"));
#endif

	BurnTimerResetYM3812();

	for (INT32 i = 0; i < nNumChips; i++) {
		YM3812ResetChip(i);
	}
}

void BurnYM3812Exit()
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YM3812Initted) bprintf(PRINT_ERROR, _T("BurnYM3812Exit called without init\n"));
#endif

	if (!DebugSnd_YM3812Initted) return;

	YM3812Shutdown();

	BurnTimerExitYM3812();

	BurnFree(pBuffer);
	
	nNumChips = 0;
	bYM3812AddSignal = 0;
	
	DebugSnd_YM3812Initted = 0;
}

INT32 BurnYM3812Init(INT32 num, INT32 nClockFrequency, OPL_IRQHANDLER IRQCallback, INT32 bAddSignal)
{
	return BurnYM3812Init(num, nClockFrequency, IRQCallback, YM3812SynchroniseStream, bAddSignal);
}

INT32 BurnYM3812Init(INT32 num, INT32 nClockFrequency, OPL_IRQHANDLER IRQCallback, INT32 (*StreamCallback)(INT32), INT32 bAddSignal)
{
	DebugSnd_YM3812Initted = 1;
	
	if (num > MAX_YM3812) num = MAX_YM3812;
	
	BurnTimerInitYM3812(&YM3812TimerOver, NULL);

	BurnYM3812StreamCallback = StreamCallback;

	if (nFMInterpolation == 3) {
		// Set YM3812 core samplerate to match the hardware
		nBurnYM3812SoundRate = nClockFrequency / 72;
		// Bring YM3812 core samplerate within usable range
		while (nBurnYM3812SoundRate > nBurnSoundRate * 3) {
			nBurnYM3812SoundRate >>= 1;
		}

		BurnYM3812Update = YM3812UpdateResample;

		if (nBurnSoundRate) nSampleSize = (UINT32)nBurnYM3812SoundRate * (1 << 16) / nBurnSoundRate;
		nFractionalPosition = 0;
	} else {
		nBurnYM3812SoundRate = nBurnSoundRate;

		BurnYM3812Update = YM3812UpdateNormal;
	}

	if (!nBurnYM3812SoundRate) nBurnYM3812SoundRate = 44100;

	YM3812Init(num, nClockFrequency, nBurnYM3812SoundRate);
	YM3812SetIRQHandler(0, IRQCallback, 0);
	YM3812SetTimerHandler(0, &BurnOPLTimerCallbackYM3812, 0);
	YM3812SetUpdateHandler(0, &BurnYM3812UpdateRequest, 0);

	pBuffer = (INT16*)BurnMalloc(4096 * num * sizeof(INT16));
	memset(pBuffer, 0, 4096 * num * sizeof(INT16));

	nYM3812Position = 0;

	nFractionalPosition = 0;
	
	nNumChips = num;
	bYM3812AddSignal = bAddSignal;
	
	// default routes
	YM3812Volumes[BURN_SND_YM3812_ROUTE] = 1.00;
	YM3812RouteDirs[BURN_SND_YM3812_ROUTE] = BURN_SND_ROUTE_BOTH;
	
	if (nNumChips > 0) {
		YM3812Volumes[1 + BURN_SND_YM3812_ROUTE] = 1.00;
		YM3812RouteDirs[1 + BURN_SND_YM3812_ROUTE] = BURN_SND_ROUTE_BOTH;
	}

	return 0;
}

void BurnYM3812SetRoute(INT32 nChip, INT32 nIndex, double nVolume, INT32 nRouteDir)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YM3812Initted) bprintf(PRINT_ERROR, _T("BurnYM3812SetRoute called without init\n"));
	if (nIndex < 0 || nIndex > 1) bprintf(PRINT_ERROR, _T("BurnYM3812SetRoute called with invalid index %i\n"), nIndex);
	if (nChip >= nNumChips) bprintf(PRINT_ERROR, _T("BurnYM3812SetRoute called with invalid chip %i\n"), nChip);
#endif
	
	if (nChip == 0) {
		YM3812Volumes[nIndex] = nVolume;
		YM3812RouteDirs[nIndex] = nRouteDir;
	}
	
	if (nChip == 1) {
		YM3812Volumes[1 + nIndex] = nVolume;
		YM3812RouteDirs[1 + nIndex] = nRouteDir;
	}
}

void BurnYM3812Scan(INT32 nAction, INT32* pnMin)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YM3812Initted) bprintf(PRINT_ERROR, _T("BurnYM3812Scan called without init\n"));
#endif

	BurnTimerScanYM3812(nAction, pnMin);
	FMOPLScan(FM_OPL_SAVESTATE_YM3812, 0, nAction, pnMin);
	
	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(nYM3812Position);
	}
}
