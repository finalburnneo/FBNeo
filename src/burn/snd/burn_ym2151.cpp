// FBAlpha YM-2151 sound core interface
#include "burnint.h"
#include "burn_ym2151.h"

// Irq Callback timing notes..
// Due to the way the internal timing of the ym2151 works, BurnYM2151Render()
// should not be called more than ~65 times per frame.  See DrvFrame() in
// drv/konami/d_surpratk.cpp for a simple and effective work-around.

UINT32 nBurnCurrentYM2151Register;

static INT32 nBurnYM2151SoundRate;

static INT16* pBuffer;
static INT16* pYM2151Buffer[2];

static INT32 nBurnPosition;
static UINT32 nSampleSize;
static UINT32 nFractionalPosition;
static UINT32 nSamplesRendered;

static double YM2151Volumes[2];
static INT32 YM2151RouteDirs[2];

static INT32 YM2151BurnTimer = 0;

void BurnYM2151Render(INT16* pSoundBuf, INT32 nSegmentLength)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YM2151Initted) bprintf(PRINT_ERROR, _T("YM2151Render called without init\n"));
#endif
	
	nBurnPosition += nSegmentLength;

	if (nBurnPosition >= nBurnSoundRate) {
		nBurnPosition = nSegmentLength;

		pYM2151Buffer[0][1] = pYM2151Buffer[0][(nFractionalPosition >> 16) - 3];
		pYM2151Buffer[0][2] = pYM2151Buffer[0][(nFractionalPosition >> 16) - 2];
		pYM2151Buffer[0][3] = pYM2151Buffer[0][(nFractionalPosition >> 16) - 1];

		pYM2151Buffer[1][1] = pYM2151Buffer[1][(nFractionalPosition >> 16) - 3];
		pYM2151Buffer[1][2] = pYM2151Buffer[1][(nFractionalPosition >> 16) - 2];
		pYM2151Buffer[1][3] = pYM2151Buffer[1][(nFractionalPosition >> 16) - 1];

		nSamplesRendered -= (nFractionalPosition >> 16) - 4;

		for (UINT32 i = 0; i <= nSamplesRendered; i++) {
			pYM2151Buffer[0][4 + i] = pYM2151Buffer[0][(nFractionalPosition >> 16) + i];
			pYM2151Buffer[1][4 + i] = pYM2151Buffer[1][(nFractionalPosition >> 16) + i];
		}

		nFractionalPosition &= 0x0000FFFF;
		nFractionalPosition |= 4 << 16;
	}

	pYM2151Buffer[0] = pBuffer + 4 + nSamplesRendered;
	pYM2151Buffer[1] = pBuffer + 4 + nSamplesRendered + 65536;

	YM2151UpdateOne(0, pYM2151Buffer, (UINT32)(nBurnPosition + 1) * nBurnYM2151SoundRate / nBurnSoundRate - nSamplesRendered);
	nSamplesRendered += (UINT32)(nBurnPosition + 1) * nBurnYM2151SoundRate / nBurnSoundRate - nSamplesRendered;

	pYM2151Buffer[0] = pBuffer;
	pYM2151Buffer[1] = pBuffer + 65536;

	nSegmentLength <<= 1;
	
	for (INT32 i = 0; i < nSegmentLength; i += 2, nFractionalPosition += nSampleSize) {
		INT32 nLeftSample[4] = {0, 0, 0, 0};
		INT32 nRightSample[4] = {0, 0, 0, 0};
		INT32 nTotalLeftSample, nTotalRightSample;
		
		if ((YM2151RouteDirs[BURN_SND_YM2151_YM2151_ROUTE_1] & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) {
			nLeftSample[0] += (INT32)(pYM2151Buffer[0][(nFractionalPosition >> 16) - 3] * YM2151Volumes[BURN_SND_YM2151_YM2151_ROUTE_1]);
			nLeftSample[1] += (INT32)(pYM2151Buffer[0][(nFractionalPosition >> 16) - 2] * YM2151Volumes[BURN_SND_YM2151_YM2151_ROUTE_1]);
			nLeftSample[2] += (INT32)(pYM2151Buffer[0][(nFractionalPosition >> 16) - 1] * YM2151Volumes[BURN_SND_YM2151_YM2151_ROUTE_1]);
			nLeftSample[3] += (INT32)(pYM2151Buffer[0][(nFractionalPosition >> 16) - 0] * YM2151Volumes[BURN_SND_YM2151_YM2151_ROUTE_1]);
		}
		if ((YM2151RouteDirs[BURN_SND_YM2151_YM2151_ROUTE_1] & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) {
			nRightSample[0] += (INT32)(pYM2151Buffer[0][(nFractionalPosition >> 16) - 3] * YM2151Volumes[BURN_SND_YM2151_YM2151_ROUTE_1]);
			nRightSample[1] += (INT32)(pYM2151Buffer[0][(nFractionalPosition >> 16) - 2] * YM2151Volumes[BURN_SND_YM2151_YM2151_ROUTE_1]);
			nRightSample[2] += (INT32)(pYM2151Buffer[0][(nFractionalPosition >> 16) - 1] * YM2151Volumes[BURN_SND_YM2151_YM2151_ROUTE_1]);
			nRightSample[3] += (INT32)(pYM2151Buffer[0][(nFractionalPosition >> 16) - 0] * YM2151Volumes[BURN_SND_YM2151_YM2151_ROUTE_1]);
		}
		
		if ((YM2151RouteDirs[BURN_SND_YM2151_YM2151_ROUTE_2] & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) {
			nLeftSample[0] += (INT32)(pYM2151Buffer[1][(nFractionalPosition >> 16) - 3] * YM2151Volumes[BURN_SND_YM2151_YM2151_ROUTE_2]);
			nLeftSample[1] += (INT32)(pYM2151Buffer[1][(nFractionalPosition >> 16) - 2] * YM2151Volumes[BURN_SND_YM2151_YM2151_ROUTE_2]);
			nLeftSample[2] += (INT32)(pYM2151Buffer[1][(nFractionalPosition >> 16) - 1] * YM2151Volumes[BURN_SND_YM2151_YM2151_ROUTE_2]);
			nLeftSample[3] += (INT32)(pYM2151Buffer[1][(nFractionalPosition >> 16) - 0] * YM2151Volumes[BURN_SND_YM2151_YM2151_ROUTE_2]);
		}
		if ((YM2151RouteDirs[BURN_SND_YM2151_YM2151_ROUTE_2] & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) {
			nRightSample[0] += (INT32)(pYM2151Buffer[1][(nFractionalPosition >> 16) - 3] * YM2151Volumes[BURN_SND_YM2151_YM2151_ROUTE_2]);
			nRightSample[1] += (INT32)(pYM2151Buffer[1][(nFractionalPosition >> 16) - 2] * YM2151Volumes[BURN_SND_YM2151_YM2151_ROUTE_2]);
			nRightSample[2] += (INT32)(pYM2151Buffer[1][(nFractionalPosition >> 16) - 1] * YM2151Volumes[BURN_SND_YM2151_YM2151_ROUTE_2]);
			nRightSample[3] += (INT32)(pYM2151Buffer[1][(nFractionalPosition >> 16) - 0] * YM2151Volumes[BURN_SND_YM2151_YM2151_ROUTE_2]);
		}
		
		nTotalLeftSample = INTERPOLATE4PS_CUSTOM((nFractionalPosition >> 4) & 0x0fff, nLeftSample[0], nLeftSample[1], nLeftSample[2], nLeftSample[3], 16384.0);
		nTotalRightSample = INTERPOLATE4PS_CUSTOM((nFractionalPosition >> 4) & 0x0fff, nRightSample[0], nRightSample[1], nRightSample[2], nRightSample[3], 16384.0);
		
		nTotalLeftSample = BURN_SND_CLIP(nTotalLeftSample);
		nTotalRightSample = BURN_SND_CLIP(nTotalRightSample);
			
		pSoundBuf[i + 0] = nTotalLeftSample;
		pSoundBuf[i + 1] = nTotalRightSample;
	}
}

void BurnYM2151Reset()
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YM2151Initted) bprintf(PRINT_ERROR, _T("BurnYM2151Reset called without init\n"));
#endif

	if (YM2151BurnTimer)
		BurnTimerReset();

	YM2151ResetChip(0);
}

void BurnYM2151Exit()
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YM2151Initted) bprintf(PRINT_ERROR, _T("BurnYM2151Exit called without init\n"));
#endif

	if (!DebugSnd_YM2151Initted) return;

	BurnYM2151SetIrqHandler(NULL);
	BurnYM2151SetPortHandler(NULL);

	YM2151Shutdown();

	if (YM2151BurnTimer) {
		BurnTimerExit();
		YM2151BurnTimer = 0;
	}

	BurnFree(pBuffer);
	
	DebugSnd_YM2151Initted = 0;
}

INT32 BurnYM2151Init(INT32 nClockFrequency)
{
	return BurnYM2151Init(nClockFrequency, 0);
}

INT32 BurnYM2151Init(INT32 nClockFrequency, INT32 use_timer)
{
	DebugSnd_YM2151Initted = 1;
	
	if (nBurnSoundRate <= 0) {
		YM2151Init(1, nClockFrequency, 11025, NULL);
		return 0;
	}

	// Set YM2151 core samplerate to match the hardware
	nBurnYM2151SoundRate = nClockFrequency >> 6;
	// Bring YM2151 core samplerate within usable range
	while (nBurnYM2151SoundRate > nBurnSoundRate * 3) {
		nBurnYM2151SoundRate >>= 1;
	}

	if (use_timer)
	{
		bprintf(0, _T("YM2151: Using FM-Timer.\n"));
		YM2151BurnTimer = 1;
		BurnTimerInit(&ym2151_timer_over, NULL);
	}

	YM2151Init(1, nClockFrequency, nBurnYM2151SoundRate, (YM2151BurnTimer) ? BurnOPMTimerCallback : NULL);

	pBuffer = (INT16*)BurnMalloc(65536 * 2 * sizeof(INT16));
	memset(pBuffer, 0, 65536 * 2 * sizeof(INT16));

	nSampleSize = (UINT32)nBurnYM2151SoundRate * (1 << 16) / nBurnSoundRate;
	nFractionalPosition = 4 << 16;
	nSamplesRendered = 0;
	nBurnPosition = 0;

	// default routes
	YM2151Volumes[BURN_SND_YM2151_YM2151_ROUTE_1] = 1.00;
	YM2151Volumes[BURN_SND_YM2151_YM2151_ROUTE_2] = 1.00;
	YM2151RouteDirs[BURN_SND_YM2151_YM2151_ROUTE_1] = BURN_SND_ROUTE_BOTH;
	YM2151RouteDirs[BURN_SND_YM2151_YM2151_ROUTE_2] = BURN_SND_ROUTE_BOTH;

	return 0;
}

// Some games make heavy use of the B timer, to increase the accuracy of
// this timer, call this with the number of times per frame BurnYM2151Render()
// is called.  See drv/dataeast/d_rohga for example usage.
void BurnYM2151SetInterleave(INT32 nInterleave)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YM2151Initted) bprintf(PRINT_ERROR, _T("BurnYM2151SetInterleave called without init\n"));
#endif

	YM2151SetTimerInterleave(nInterleave * (nBurnFPS / 100));
}

void BurnYM2151SetRoute(INT32 nIndex, double nVolume, INT32 nRouteDir)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YM2151Initted) bprintf(PRINT_ERROR, _T("BurnYM2151SetRoute called without init\n"));
	if (nIndex < 0 || nIndex > 1) bprintf(PRINT_ERROR, _T("BurnYM2151SetRoute called with invalid index %i\n"), nIndex);
#endif
	
	YM2151Volumes[nIndex] = nVolume;
	YM2151RouteDirs[nIndex] = nRouteDir;
}

void BurnYM2151Scan(INT32 nAction, INT32 *pnMin)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YM2151Initted) bprintf(PRINT_ERROR, _T("BurnYM2151Scan called without init\n"));
#endif
	
	if ((nAction & ACB_DRIVER_DATA) == 0) {
		return;
	}

	SCAN_VAR(nBurnCurrentYM2151Register);

	BurnYM2151Scan_int(nAction); // Scan the YM2151's internal registers

	if (YM2151BurnTimer)
		BurnTimerScan(nAction, pnMin);
}
