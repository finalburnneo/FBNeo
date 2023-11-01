#include "burnint.h"
#include "burn_y8950.h"

#define MAX_Y8950	2

void (*BurnY8950Update)(INT16* pSoundBuf, INT32 nSegmentEnd);

static INT32 (*BurnY8950StreamCallback)(INT32 nSoundRate);

static INT32 nBurnY8950SoundRate;

static INT16* pBuffer;
static INT16* pY8950Buffer[MAX_Y8950];

static INT32 nY8950Position;

static UINT32 nSampleSize;
static INT32 nFractionalPosition;

static INT32 nNumChips = 0;

static INT32 bY8950AddSignal;

static double Y8950Volumes[1 * MAX_Y8950];
static INT32 Y8950RouteDirs[1 * MAX_Y8950];

// ----------------------------------------------------------------------------
// Execute Y8950 for part of a frame

static void Y8950Render(INT32 nSegmentLength)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_Y8950Initted) bprintf(PRINT_ERROR, _T("Y8950Render called without init\n"));
#endif

	if (nY8950Position >= nSegmentLength || !pBurnSoundOut) {
		return;
	}

	nSegmentLength -= nY8950Position;

	Y8950UpdateOne(0, pBuffer + 0 * 4096 + 4 + nY8950Position, nSegmentLength);
	
	if (nNumChips > 1) {
		Y8950UpdateOne(1, pBuffer + 1 * 4096 + 4 + nY8950Position, nSegmentLength);
	}

	nY8950Position += nSegmentLength;
}

// ----------------------------------------------------------------------------
// Update the sound buffer

static void Y8950UpdateResample(INT16* pSoundBuf, INT32 nSegmentEnd)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_Y8950Initted) bprintf(PRINT_ERROR, _T("Y8950UpdateResample called without init\n"));
#endif

	if (!pBurnSoundOut) return;

	INT32 nSegmentLength = nSegmentEnd;
	INT32 nSamplesNeeded = nSegmentEnd * nBurnY8950SoundRate / nBurnSoundRate + 1;


	if (nSamplesNeeded < nY8950Position) {
		nSamplesNeeded = nY8950Position;
	}

	if (nSegmentLength > nBurnSoundLen) {
		nSegmentLength = nBurnSoundLen;
	}
	nSegmentLength <<= 1;

	Y8950Render(nSamplesNeeded);
	
	pY8950Buffer[0] = pBuffer + 0 * 4096 + 4;
	
	if (nNumChips > 1) {
		pY8950Buffer[1] = pBuffer + 1 * 4096 + 4;
	}
	
	for (INT32 i = (nFractionalPosition & 0xFFFF0000) >> 15; i < nSegmentLength; i += 2, nFractionalPosition += nSampleSize) {
		INT32 nLeftSample[4] = {0, 0, 0, 0};
		INT32 nRightSample[4] = {0, 0, 0, 0};
		INT32 nTotalLeftSample, nTotalRightSample;
		
		if ((Y8950RouteDirs[BURN_SND_Y8950_ROUTE] & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) {
			nLeftSample[0] += (INT32)(pY8950Buffer[0][(nFractionalPosition >> 16) - 3] * Y8950Volumes[BURN_SND_Y8950_ROUTE]);
			nLeftSample[1] += (INT32)(pY8950Buffer[0][(nFractionalPosition >> 16) - 2] * Y8950Volumes[BURN_SND_Y8950_ROUTE]);
			nLeftSample[2] += (INT32)(pY8950Buffer[0][(nFractionalPosition >> 16) - 1] * Y8950Volumes[BURN_SND_Y8950_ROUTE]);
			nLeftSample[3] += (INT32)(pY8950Buffer[0][(nFractionalPosition >> 16) - 0] * Y8950Volumes[BURN_SND_Y8950_ROUTE]);
		}
		if ((Y8950RouteDirs[BURN_SND_Y8950_ROUTE] & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) {
			nRightSample[0] += (INT32)(pY8950Buffer[0][(nFractionalPosition >> 16) - 3] * Y8950Volumes[BURN_SND_Y8950_ROUTE]);
			nRightSample[1] += (INT32)(pY8950Buffer[0][(nFractionalPosition >> 16) - 2] * Y8950Volumes[BURN_SND_Y8950_ROUTE]);
			nRightSample[2] += (INT32)(pY8950Buffer[0][(nFractionalPosition >> 16) - 1] * Y8950Volumes[BURN_SND_Y8950_ROUTE]);
			nRightSample[3] += (INT32)(pY8950Buffer[0][(nFractionalPosition >> 16) - 0] * Y8950Volumes[BURN_SND_Y8950_ROUTE]);
		}
		
		if (nNumChips > 1) {
			if ((Y8950RouteDirs[1 + BURN_SND_Y8950_ROUTE] & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) {
				nLeftSample[0] += (INT32)(pY8950Buffer[1][(nFractionalPosition >> 16) - 3] * Y8950Volumes[1 + BURN_SND_Y8950_ROUTE]);
				nLeftSample[1] += (INT32)(pY8950Buffer[1][(nFractionalPosition >> 16) - 2] * Y8950Volumes[1 + BURN_SND_Y8950_ROUTE]);
				nLeftSample[2] += (INT32)(pY8950Buffer[1][(nFractionalPosition >> 16) - 1] * Y8950Volumes[1 + BURN_SND_Y8950_ROUTE]);
				nLeftSample[3] += (INT32)(pY8950Buffer[1][(nFractionalPosition >> 16) - 0] * Y8950Volumes[1 + BURN_SND_Y8950_ROUTE]);
			}
			if ((Y8950RouteDirs[1 + BURN_SND_Y8950_ROUTE] & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) {
				nRightSample[0] += (INT32)(pY8950Buffer[1][(nFractionalPosition >> 16) - 3] * Y8950Volumes[1 + BURN_SND_Y8950_ROUTE]);
				nRightSample[1] += (INT32)(pY8950Buffer[1][(nFractionalPosition >> 16) - 2] * Y8950Volumes[1 + BURN_SND_Y8950_ROUTE]);
				nRightSample[2] += (INT32)(pY8950Buffer[1][(nFractionalPosition >> 16) - 1] * Y8950Volumes[1 + BURN_SND_Y8950_ROUTE]);
				nRightSample[3] += (INT32)(pY8950Buffer[1][(nFractionalPosition >> 16) - 0] * Y8950Volumes[1 + BURN_SND_Y8950_ROUTE]);
			}
		}
		
		nTotalLeftSample = INTERPOLATE4PS_16BIT((nFractionalPosition >> 4) & 0x0fff, nLeftSample[0], nLeftSample[1], nLeftSample[2], nLeftSample[3]);
		nTotalRightSample = INTERPOLATE4PS_16BIT((nFractionalPosition >> 4) & 0x0fff, nRightSample[0], nRightSample[1], nRightSample[2], nRightSample[3]);
		
		nTotalLeftSample = BURN_SND_CLIP(nTotalLeftSample);
		nTotalRightSample = BURN_SND_CLIP(nTotalRightSample);
			
		if (bY8950AddSignal) {
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
			pY8950Buffer[0][i] = pY8950Buffer[0][(nFractionalPosition >> 16) + i];
			
			if (nNumChips > 1) {
				pY8950Buffer[1][i] = pY8950Buffer[1][(nFractionalPosition >> 16) + i];
			}
		}

		nFractionalPosition &= 0xFFFF;

		nY8950Position = nExtraSamples;
	}
}

static void Y8950UpdateNormal(INT16* pSoundBuf, INT32 nSegmentEnd)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_Y8950Initted) bprintf(PRINT_ERROR, _T("Y8950UpdateNormal called without init\n"));
#endif

	if (!pBurnSoundOut) return;

	INT32 nSegmentLength = nSegmentEnd;

	if (nSegmentEnd < nY8950Position) {
		nSegmentEnd = nY8950Position;
	}

	if (nSegmentLength > nBurnSoundLen) {
		nSegmentLength = nBurnSoundLen;
	}

	Y8950Render(nSegmentEnd);

	pY8950Buffer[0] = pBuffer + 4 + 0 * 4096;
	
	if (nNumChips > 1) {
		pY8950Buffer[1] = pBuffer + 4 + 1 * 4096;
	}

	for (INT32 n = nFractionalPosition; n < nSegmentLength; n++) {
		INT32 nLeftSample = 0, nRightSample = 0;
		
		if ((Y8950RouteDirs[BURN_SND_Y8950_ROUTE] & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) {
			nLeftSample += (INT32)(pY8950Buffer[0][n] * Y8950Volumes[BURN_SND_Y8950_ROUTE]);
		}
		if ((Y8950RouteDirs[BURN_SND_Y8950_ROUTE] & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) {
			nRightSample += (INT32)(pY8950Buffer[0][n] * Y8950Volumes[BURN_SND_Y8950_ROUTE]);
		}
		
		if (nNumChips > 1) {
			if ((Y8950RouteDirs[1 + BURN_SND_Y8950_ROUTE] & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) {
				nLeftSample += (INT32)(pY8950Buffer[1][n] * Y8950Volumes[1 + BURN_SND_Y8950_ROUTE]);
			}
			if ((Y8950RouteDirs[1 + BURN_SND_Y8950_ROUTE] & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) {
				nRightSample += (INT32)(pY8950Buffer[1][n] * Y8950Volumes[1 + BURN_SND_Y8950_ROUTE]);
			}
		}
		
		nLeftSample = BURN_SND_CLIP(nLeftSample);
		nRightSample = BURN_SND_CLIP(nRightSample);
			
		if (bY8950AddSignal) {
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
			pY8950Buffer[i] = pY8950Buffer[nBurnSoundLen + i];
		}

		nFractionalPosition = 0;

		nY8950Position = nExtraSamples;

	}
}

// ----------------------------------------------------------------------------
// Callbacks for Y8950 core

void BurnY8950UpdateRequest(INT32, INT32)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_Y8950Initted) bprintf(PRINT_ERROR, _T("BurnY8950UpdateRequest called without init\n"));
#endif

	Y8950Render(BurnY8950StreamCallback(nBurnY8950SoundRate));
}

// ----------------------------------------------------------------------------
// Initialisation, etc.

void BurnY8950Reset()
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_Y8950Initted) bprintf(PRINT_ERROR, _T("BurnY8950Reset called without init\n"));
#endif

	BurnTimerReset();

	for (INT32 i = 0; i < nNumChips; i++) {
		Y8950ResetChip(i);
	}
}

void BurnY8950Exit()
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_Y8950Initted) bprintf(PRINT_ERROR, _T("BurnY8950Exit called without init\n"));
#endif

	// Crash prevention.
	if (!DebugSnd_Y8950Initted) return;

	Y8950Shutdown();

	BurnTimerExit();

	BurnFree(pBuffer);
	
	nNumChips = 0;
	bY8950AddSignal = 0;
	
	DebugSnd_Y8950Initted = 0;
}

INT32 BurnY8950Init(INT32 num, INT32 nClockFrequency, UINT8* Y8950ADPCM0ROM, INT32 nY8950ADPCM0Size, UINT8* Y8950ADPCM1ROM, INT32 nY8950ADPCM1Size, OPL_IRQHANDLER IRQCallback, INT32 bAddSignal)
{
	return BurnY8950Init(num, nClockFrequency, Y8950ADPCM0ROM, nY8950ADPCM0Size, Y8950ADPCM1ROM, nY8950ADPCM1Size, IRQCallback, BurnSynchroniseStream, bAddSignal);
}

INT32 BurnY8950Init(INT32 num, INT32 nClockFrequency, UINT8* Y8950ADPCM0ROM, INT32 nY8950ADPCM0Size, UINT8* Y8950ADPCM1ROM, INT32 nY8950ADPCM1Size, OPL_IRQHANDLER IRQCallback, INT32 (*StreamCallback)(INT32), INT32 bAddSignal)
{
	INT32 timer_chipbase = BurnTimerInit(&Y8950TimerOver, NULL, num);

	BurnY8950StreamCallback = StreamCallback;

	if (nFMInterpolation == 3) {
		// Set Y8950 core samplerate to match the hardware
		nBurnY8950SoundRate = nClockFrequency / 72;
		// Bring Y8950 core samplerate within usable range
		while (nBurnY8950SoundRate > nBurnSoundRate * 3) {
			nBurnY8950SoundRate >>= 1;
		}

		BurnY8950Update = Y8950UpdateResample;

		if (nBurnSoundRate) nSampleSize = (UINT32)nBurnY8950SoundRate * (1 << 16) / nBurnSoundRate;
		nFractionalPosition = 0;
	} else {
		nBurnY8950SoundRate = nBurnSoundRate;

		BurnY8950Update = Y8950UpdateNormal;
	}

	if (!nBurnY8950SoundRate) nBurnY8950SoundRate = 44100;

	Y8950Init(num, nClockFrequency, nBurnY8950SoundRate);
	Y8950SetIRQHandler(0, IRQCallback, 0);
	Y8950SetTimerHandler(0, &BurnOPLTimerCallback, timer_chipbase + 0);
	Y8950SetUpdateHandler(0, &BurnY8950UpdateRequest, 0);
	Y8950SetDeltaTMemory(0, Y8950ADPCM0ROM, nY8950ADPCM0Size);
	if (num > 1) {
//		Y8950SetIRQHandler(1, IRQCallback, 0); // ??
		Y8950SetTimerHandler(1, &BurnOPLTimerCallback, timer_chipbase + 1);
		Y8950SetUpdateHandler(1, &BurnY8950UpdateRequest, 0);
		Y8950SetDeltaTMemory(1, Y8950ADPCM1ROM, nY8950ADPCM1Size);
	}

	pBuffer = (INT16*)BurnMalloc(4096 * num * sizeof(INT16));
	memset(pBuffer, 0, 4096 * num * sizeof(INT16));

	nY8950Position = 0;

	nFractionalPosition = 0;
	
	nNumChips = num;
	bY8950AddSignal = bAddSignal;
	
	// default routes
	Y8950Volumes[BURN_SND_Y8950_ROUTE] = 1.00;
	Y8950RouteDirs[BURN_SND_Y8950_ROUTE] = BURN_SND_ROUTE_BOTH;
	if (nNumChips > 1) {
		Y8950Volumes[1 + BURN_SND_Y8950_ROUTE] = 1.00;
		Y8950RouteDirs[1 + BURN_SND_Y8950_ROUTE] = BURN_SND_ROUTE_BOTH;
	}
	
	DebugSnd_Y8950Initted = 1;

	return 0;
}

void BurnY8950SetRoute(INT32 nChip, INT32 nIndex, double nVolume, INT32 nRouteDir)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_Y8950Initted) bprintf(PRINT_ERROR, _T("BurnY8950SetRoute called without init\n"));
	if (nIndex < 0 || nIndex > 1) bprintf(PRINT_ERROR, _T("BurnY8950SetRoute called with invalid index %i\n"), nIndex);
	if (nChip >= nNumChips) bprintf(PRINT_ERROR, _T("BurnY8950SetRoute called with invalid chip %i\n"), nChip);
#endif
	
	if (nChip == 0) {
		Y8950Volumes[nIndex] = nVolume;
		Y8950RouteDirs[nIndex] = nRouteDir;
	}
	
	if (nChip == 1) {
		Y8950Volumes[1 + nIndex] = nVolume;
		Y8950RouteDirs[1 + nIndex] = nRouteDir;
	}
}

void BurnY8950Scan(INT32 nAction, INT32* pnMin)
{
	#if defined FBNEO_DEBUG
	if (!DebugSnd_Y8950Initted) bprintf(PRINT_ERROR, _T("BurnY8950Scan called without init\n"));
#endif

	BurnTimerScan(nAction, pnMin);
	FMOPLScan(FM_OPL_SAVESTATE_Y8950, 0, nAction, pnMin);

	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(nY8950Position);
	}
}

#undef MAX_Y8950
