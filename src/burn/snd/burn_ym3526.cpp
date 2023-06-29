#include "burnint.h"
#include "burn_ym3526.h"

void (*BurnYM3526Update)(INT16* pSoundBuf, INT32 nSegmentEnd);

static INT32 (*BurnYM3526StreamCallback)(INT32 nSoundRate);

static INT32 nBurnYM3526SoundRate;

static INT16* pBuffer;
static INT16* pYM3526Buffer;

static INT32 nYM3526Position;

static UINT32 nSampleSize;
static INT32 nFractionalPosition;

static INT32 bYM3526AddSignal;

static double YM3526Volumes[1];
static INT32 YM3526RouteDirs[1];

// ----------------------------------------------------------------------------
// Execute YM3526 for part of a frame

static void YM3526Render(INT32 nSegmentLength)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YM3526Initted) bprintf(PRINT_ERROR, _T("YM3526Render called without init\n"));
#endif

	if (nYM3526Position >= nSegmentLength || !pBurnSoundOut) {
		return;
	}

	nSegmentLength -= nYM3526Position;

	YM3526UpdateOne(0, pBuffer + 0 * 4096 + 4 + nYM3526Position, nSegmentLength);

	nYM3526Position += nSegmentLength;
}

// ----------------------------------------------------------------------------
// Update the sound buffer

static void YM3526UpdateResample(INT16* pSoundBuf, INT32 nSegmentEnd)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YM3526Initted) bprintf(PRINT_ERROR, _T("YM3526UpdateResample called without init\n"));
#endif

	if (!pBurnSoundOut) return;

	INT32 nSegmentLength = nSegmentEnd;
	INT32 nSamplesNeeded = nSegmentEnd * nBurnYM3526SoundRate / nBurnSoundRate + 1;

	if (nSamplesNeeded < nYM3526Position) {
		nSamplesNeeded = nYM3526Position;
	}

	if (nSegmentLength > nBurnSoundLen) {
		nSegmentLength = nBurnSoundLen;
	}
	nSegmentLength <<= 1;

	YM3526Render(nSamplesNeeded);

	pYM3526Buffer = pBuffer + 0 * 4096 + 4;

	for (INT32 i = (nFractionalPosition & 0xFFFF0000) >> 15; i < nSegmentLength; i += 2, nFractionalPosition += nSampleSize) {
		INT32 nLeftSample[4] = {0, 0, 0, 0};
		INT32 nRightSample[4] = {0, 0, 0, 0};
		INT32 nTotalLeftSample, nTotalRightSample;
		
		if ((YM3526RouteDirs[BURN_SND_YM3526_ROUTE] & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) {
			nLeftSample[0] += (INT32)(pYM3526Buffer[(nFractionalPosition >> 16) - 3] * YM3526Volumes[BURN_SND_YM3526_ROUTE]);
			nLeftSample[1] += (INT32)(pYM3526Buffer[(nFractionalPosition >> 16) - 2] * YM3526Volumes[BURN_SND_YM3526_ROUTE]);
			nLeftSample[2] += (INT32)(pYM3526Buffer[(nFractionalPosition >> 16) - 1] * YM3526Volumes[BURN_SND_YM3526_ROUTE]);
			nLeftSample[3] += (INT32)(pYM3526Buffer[(nFractionalPosition >> 16) - 0] * YM3526Volumes[BURN_SND_YM3526_ROUTE]);
		}
		if ((YM3526RouteDirs[BURN_SND_YM3526_ROUTE] & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) {
			nRightSample[0] += (INT32)(pYM3526Buffer[(nFractionalPosition >> 16) - 3] * YM3526Volumes[BURN_SND_YM3526_ROUTE]);
			nRightSample[1] += (INT32)(pYM3526Buffer[(nFractionalPosition >> 16) - 2] * YM3526Volumes[BURN_SND_YM3526_ROUTE]);
			nRightSample[2] += (INT32)(pYM3526Buffer[(nFractionalPosition >> 16) - 1] * YM3526Volumes[BURN_SND_YM3526_ROUTE]);
			nRightSample[3] += (INT32)(pYM3526Buffer[(nFractionalPosition >> 16) - 0] * YM3526Volumes[BURN_SND_YM3526_ROUTE]);
		}
		
		nTotalLeftSample = INTERPOLATE4PS_16BIT((nFractionalPosition >> 4) & 0x0fff, nLeftSample[0], nLeftSample[1], nLeftSample[2], nLeftSample[3]);
		nTotalRightSample = INTERPOLATE4PS_16BIT((nFractionalPosition >> 4) & 0x0fff, nRightSample[0], nRightSample[1], nRightSample[2], nRightSample[3]);
		
		nTotalLeftSample = BURN_SND_CLIP(nTotalLeftSample);
		nTotalRightSample = BURN_SND_CLIP(nTotalRightSample);
			
		if (bYM3526AddSignal) {
			pSoundBuf[i + 0] += nTotalLeftSample;
			pSoundBuf[i + 1] += nTotalRightSample;
		} else {
			pSoundBuf[i + 0] = nTotalLeftSample;
			pSoundBuf[i + 1] = nTotalRightSample;
		}
	}

	if (nSegmentEnd >= nBurnSoundLen) {
		INT32 nExtraSamples = nSamplesNeeded - (nFractionalPosition >> 16);

		for (INT32 i = -4; i < nExtraSamples; i++) {
			pYM3526Buffer[i] = pYM3526Buffer[(nFractionalPosition >> 16) + i];
		}

		nFractionalPosition &= 0xFFFF;

		nYM3526Position = nExtraSamples;
	}
}

static void YM3526UpdateNormal(INT16* pSoundBuf, INT32 nSegmentEnd)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YM3526Initted) bprintf(PRINT_ERROR, _T("YM3526UpdateNormal called without init\n"));
#endif

	if (!pBurnSoundOut) return;

	INT32 nSegmentLength = nSegmentEnd;

	if (nSegmentEnd < nYM3526Position) {
		nSegmentEnd = nYM3526Position;
	}

	if (nSegmentLength > nBurnSoundLen) {
		nSegmentLength = nBurnSoundLen;
	}

	YM3526Render(nSegmentEnd);

	pYM3526Buffer = pBuffer + 4 + 0 * 4096;

	for (INT32 n = nFractionalPosition; n < nSegmentLength; n++) {
		INT32 nLeftSample = 0, nRightSample = 0;
		
		if ((YM3526RouteDirs[BURN_SND_YM3526_ROUTE] & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) {
			nLeftSample += (INT32)(pYM3526Buffer[n] * YM3526Volumes[BURN_SND_YM3526_ROUTE]);
		}
		if ((YM3526RouteDirs[BURN_SND_YM3526_ROUTE] & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) {
			nRightSample += (INT32)(pYM3526Buffer[n] * YM3526Volumes[BURN_SND_YM3526_ROUTE]);
		}
		
		nLeftSample = BURN_SND_CLIP(nLeftSample);
		nRightSample = BURN_SND_CLIP(nRightSample);
			
		if (bYM3526AddSignal) {
			pSoundBuf[(n << 1) + 0] += nLeftSample;
			pSoundBuf[(n << 1) + 1] += nRightSample;
		} else {
			pSoundBuf[(n << 1) + 0] = nLeftSample;
			pSoundBuf[(n << 1) + 1] = nRightSample;
		}
	}

	nFractionalPosition = nSegmentLength;

	if (nSegmentEnd >= nBurnSoundLen) {
		INT32 nExtraSamples = nSegmentEnd - nBurnSoundLen;

		for (INT32 i = 0; i < nExtraSamples; i++) {
			pYM3526Buffer[i] = pYM3526Buffer[nBurnSoundLen + i];
		}

		nFractionalPosition = 0;

		nYM3526Position = nExtraSamples;

	}
}

// ----------------------------------------------------------------------------
// Callbacks for YM3526 core

void BurnYM3526UpdateRequest(int, int)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YM3526Initted) bprintf(PRINT_ERROR, _T("BurnYM3526UpdateRequest called without init\n"));
#endif

	YM3526Render(BurnYM3526StreamCallback(nBurnYM3526SoundRate));
}

// ----------------------------------------------------------------------------
// Initialisation, etc.

void BurnYM3526Reset()
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YM3526Initted) bprintf(PRINT_ERROR, _T("BurnYM3526Reset called without init\n"));
#endif

	BurnTimerReset();

	YM3526ResetChip(0);
}

void BurnYM3526Exit()
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YM3526Initted) bprintf(PRINT_ERROR, _T("BurnYM3526Exit called without init\n"));
#endif

	if (!DebugSnd_YM3526Initted) return;

	YM3526Shutdown();

	BurnTimerExit();

	BurnFree(pBuffer);
	
	bYM3526AddSignal = 0;
	
	DebugSnd_YM3526Initted = 0;
}

INT32 BurnYM3526Init(INT32 nClockFrequency, OPL_IRQHANDLER IRQCallback, INT32 bAddSignal)
{
	return BurnYM3526Init(nClockFrequency, IRQCallback, BurnSynchroniseStream, bAddSignal);
}

INT32 BurnYM3526Init(INT32 nClockFrequency, OPL_IRQHANDLER IRQCallback, INT32 (*StreamCallback)(INT32), INT32 bAddSignal)
{
	DebugSnd_YM3526Initted = 1;
	
	INT32 timer_chipbase = BurnTimerInit(&YM3526TimerOver, NULL);

	BurnYM3526StreamCallback = StreamCallback;

	if (nFMInterpolation == 3) {
		// Set YM3526 core samplerate to match the hardware
		nBurnYM3526SoundRate = nClockFrequency / 72;
		// Bring YM3526 core samplerate within usable range
		while (nBurnYM3526SoundRate > nBurnSoundRate * 3) {
			nBurnYM3526SoundRate >>= 1;
		}

		BurnYM3526Update = YM3526UpdateResample;

		if (nBurnSoundRate) nSampleSize = (UINT32)nBurnYM3526SoundRate * (1 << 16) / nBurnSoundRate;
		nFractionalPosition = 0;
	} else {
		nBurnYM3526SoundRate = nBurnSoundRate;

		BurnYM3526Update = YM3526UpdateNormal;
	}

	if (!nBurnYM3526SoundRate) nBurnYM3526SoundRate = 44100;

	YM3526Init(1, nClockFrequency, nBurnYM3526SoundRate);
	YM3526SetIRQHandler(0, IRQCallback, 0);
	YM3526SetTimerHandler(0, &BurnOPLTimerCallback, timer_chipbase + 0);
	YM3526SetUpdateHandler(0, &BurnYM3526UpdateRequest, 0);

	pBuffer = (INT16*)BurnMalloc(4096 * sizeof(INT16));
	memset(pBuffer, 0, 4096 * sizeof(INT16));

	nYM3526Position = 0;

	nFractionalPosition = 0;
	
	bYM3526AddSignal = bAddSignal;
	
	// default routes
	YM3526Volumes[BURN_SND_YM3526_ROUTE] = 1.00;
	YM3526RouteDirs[BURN_SND_YM3526_ROUTE] = BURN_SND_ROUTE_BOTH;

	return 0;
}

void BurnYM3526SetRoute(INT32 nIndex, double nVolume, INT32 nRouteDir)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YM3526Initted) bprintf(PRINT_ERROR, _T("BurnYM3526SetRoute called without init\n"));
	if (nIndex < 0 || nIndex > 1) bprintf(PRINT_ERROR, _T("BurnYM3526SetRoute called with invalid index %i\n"), nIndex);
#endif
	
	YM3526Volumes[nIndex] = nVolume;
	YM3526RouteDirs[nIndex] = nRouteDir;
}

void BurnYM3526Scan(INT32 nAction, INT32* pnMin)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YM3526Initted) bprintf(PRINT_ERROR, _T("BurnYM3526Scan called without init\n"));
#endif

	BurnTimerScan(nAction, pnMin);
	FMOPLScan(FM_OPL_SAVESTATE_YM3526, 0, nAction, pnMin);

	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(nYM3526Position);
	}
}
