#include "burnint.h"
#include "burn_ymf262.h"

static INT32 (*BurnYMF262StreamCallback)(INT32 nSoundRate);

static INT16* pBuffer;
static INT16* pYMF262Buffer[2];

static INT32 bYMF262AddSignal;

static UINT32 nSampleSize;
static INT32 nYMF262Position;
static INT32 nFractionalPosition;

static INT32 nBurnYMF262SoundRate;

static double YMF262Volumes[2];
static INT32 YMF262RouteDirs[2];

static void *ymfchip = NULL;

// ----------------------------------------------------------------------------
// Dummy functions

static INT32 YMF262StreamCallbackDummy(INT32 /* nSoundRate */)
{
	return 0;
}


// ----------------------------------------------------------------------------
// Execute YMF262 for part of a frame

static void YMF262Render(INT32 nSegmentLength)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YMF262Initted) bprintf(PRINT_ERROR, _T("YMF262Render called without init\n"));
#endif

	if (nYMF262Position >= nSegmentLength) {
		return;
	}

//	bprintf(PRINT_NORMAL, _T("    YMF262 render %6i -> %6i\n"), nYMF262Position, nSegmentLength);

	nSegmentLength -= nYMF262Position;

	pYMF262Buffer[0] = pBuffer + 0 * 4096 + 4 + nYMF262Position;
	pYMF262Buffer[1] = pBuffer + 1 * 4096 + 4 + nYMF262Position;

	ymf262_update_one(ymfchip, pYMF262Buffer, nSegmentLength);

	nYMF262Position += nSegmentLength;
}

// ----------------------------------------------------------------------------
// Update the sound buffer

#define INTERPOLATE_ADD_SOUND_LEFT(route, buffer)																	\
	if ((YMF262RouteDirs[route] & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) {									\
		nLeftSample[0] += (INT32)(pYMF262Buffer[buffer][(nFractionalPosition >> 16) - 3]/* * YMF262Volumes[route]*/);	\
		nLeftSample[1] += (INT32)(pYMF262Buffer[buffer][(nFractionalPosition >> 16) - 2]/* * YMF262Volumes[route]*/);	\
		nLeftSample[2] += (INT32)(pYMF262Buffer[buffer][(nFractionalPosition >> 16) - 1]/* * YMF262Volumes[route]*/);	\
		nLeftSample[3] += (INT32)(pYMF262Buffer[buffer][(nFractionalPosition >> 16) - 0]/* * YMF262Volumes[route]*/);	\
	}

#define INTERPOLATE_ADD_SOUND_RIGHT(route, buffer)																	\
	if ((YMF262RouteDirs[route] & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) {									\
		nRightSample[0] += (INT32)(pYMF262Buffer[buffer][(nFractionalPosition >> 16) - 3]/* * YMF262Volumes[route]*/);	\
		nRightSample[1] += (INT32)(pYMF262Buffer[buffer][(nFractionalPosition >> 16) - 2]/* * YMF262Volumes[route]*/);	\
		nRightSample[2] += (INT32)(pYMF262Buffer[buffer][(nFractionalPosition >> 16) - 1]/* * YMF262Volumes[route]*/);	\
		nRightSample[3] += (INT32)(pYMF262Buffer[buffer][(nFractionalPosition >> 16) - 0]/* * YMF262Volumes[route]*/);	\
	}

void BurnYMF262Update(INT32 nSegmentEnd)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YMF262Initted) bprintf(PRINT_ERROR, _T("BurnYMF262Update called without init\n"));
#endif

	INT16* pSoundBuf = pBurnSoundOut;

	if (nBurnSoundRate == 0 || pBurnSoundOut == NULL) {
		return;
	}

//	bprintf(PRINT_NORMAL, _T("    YMF262 render %6i -> %6i\n"), nYMF262Position, nSegmentEnd);

	INT32 nSegmentLength = nSegmentEnd;
	INT32 nSamplesNeeded = nSegmentEnd * nBurnYMF262SoundRate / nBurnSoundRate + 1;

	if (nSamplesNeeded < nYMF262Position) {
		nSamplesNeeded = nYMF262Position;
	}

	if (nSegmentLength > nBurnSoundLen) {
		nSegmentLength = nBurnSoundLen;
	}
	nSegmentLength <<= 1;

	YMF262Render(nSamplesNeeded);

	pYMF262Buffer[0] = pBuffer + 0 * 4096 + 4;
	pYMF262Buffer[1] = pBuffer + 1 * 4096 + 4;

	for (INT32 i = (nFractionalPosition & 0xFFFF0000) >> 15; i < nSegmentLength; i += 2, nFractionalPosition += nSampleSize) {
		INT32 nLeftSample[4] = {0, 0, 0, 0};
		INT32 nRightSample[4] = {0, 0, 0, 0};
		INT32 nTotalLeftSample, nTotalRightSample;

		INTERPOLATE_ADD_SOUND_LEFT  (BURN_SND_YMF262_YMF262_ROUTE_1, 0)
		INTERPOLATE_ADD_SOUND_RIGHT (BURN_SND_YMF262_YMF262_ROUTE_1, 0)
		INTERPOLATE_ADD_SOUND_LEFT  (BURN_SND_YMF262_YMF262_ROUTE_2, 1)
		INTERPOLATE_ADD_SOUND_RIGHT (BURN_SND_YMF262_YMF262_ROUTE_2, 1)

		nTotalLeftSample  = INTERPOLATE4PS_16BIT((nFractionalPosition >> 4) & 0x0fff, nLeftSample[0], nLeftSample[1], nLeftSample[2], nLeftSample[3]);
		nTotalRightSample = INTERPOLATE4PS_16BIT((nFractionalPosition >> 4) & 0x0fff, nRightSample[0], nRightSample[1], nRightSample[2], nRightSample[3]);

		nTotalLeftSample  = BURN_SND_CLIP(nTotalLeftSample * YMF262Volumes[BURN_SND_YMF262_YMF262_ROUTE_1]);
		nTotalRightSample = BURN_SND_CLIP(nTotalRightSample * YMF262Volumes[BURN_SND_YMF262_YMF262_ROUTE_2]);

		if (bYMF262AddSignal) {
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
			pYMF262Buffer[0][i] = pYMF262Buffer[0][(nFractionalPosition >> 16) + i];
			pYMF262Buffer[1][i] = pYMF262Buffer[1][(nFractionalPosition >> 16) + i];
		}

		nFractionalPosition &= 0xFFFF;

		nYMF262Position = nExtraSamples;
	}
}

// ----------------------------------------------------------------------------

void BurnYMF262Write(INT32 nAddress, UINT8 nValue)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YMF262Initted) bprintf(PRINT_ERROR, _T("BurnYMF262Write called without init\n"));
#endif

	YMF262Render(BurnYMF262StreamCallback(nBurnYMF262SoundRate));
	ymf262_write(ymfchip, nAddress&3, nValue);
}

UINT8 BurnYMF262Read(INT32 nAddress)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YMF262Initted) bprintf(PRINT_ERROR, _T("BurnYMF262Read called without init\n"));
#endif

	YMF262Render(BurnYMF262StreamCallback(nBurnYMF262SoundRate));
	return ymf262_read(ymfchip, nAddress&3);
}


// ----------------------------------------------------------------------------
static int ymf262_timerover(int /*num*/, int c)
{
	return ymf262_timer_over(ymfchip, c);
}


void BurnYMF262Reset()
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YMF262Initted) bprintf(PRINT_ERROR, _T("BurnYMF262Reset called without init\n"));
#endif

	BurnTimerReset();
	ymf262_reset_chip(ymfchip);
}

void BurnYMF262Exit()
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YMF262Initted) bprintf(PRINT_ERROR, _T("BurnYMF262Exit called without init\n"));
#endif

	if (!DebugSnd_YMF262Initted) return;

	ymf262_shutdown(ymfchip);

	BurnTimerExit();

	BurnFree(pBuffer);
	
	DebugSnd_YMF262Initted = 0;
}

INT32 BurnYMF262Init(INT32 nClockFrequency, void (*IRQCallback)(INT32, INT32), INT32 nAdd)
{
	return BurnYMF262Init(nClockFrequency, IRQCallback, BurnSynchroniseStream, nAdd);
}

INT32 BurnYMF262Init(INT32 nClockFrequency, void (*IRQCallback)(INT32, INT32), INT32 (*StreamCallback)(INT32), INT32 nAdd)
{
	DebugSnd_YMF262Initted = 1;

	BurnYMF262StreamCallback = YMF262StreamCallbackDummy;
	if (StreamCallback) {
		BurnYMF262StreamCallback = StreamCallback;
	}

	nBurnYMF262SoundRate = nClockFrequency / 288; // hw rate based on input clock

	nSampleSize = (UINT32)nBurnYMF262SoundRate * (1 << 16) / nBurnSoundRate;
	bYMF262AddSignal = nAdd;

	BurnTimerInit(&ymf262_timerover, NULL);
	ymfchip = ymf262_init(nClockFrequency, nBurnYMF262SoundRate, IRQCallback, BurnYMF262TimerCallback);

	pBuffer = (INT16*)BurnMalloc(4096 * 2 * sizeof(INT16));
	memset(pBuffer, 0, 4096 * 2 * sizeof(INT16));

	nYMF262Position = 0;
	nFractionalPosition = 0;
	
	// default routes
	YMF262Volumes[BURN_SND_YMF262_YMF262_ROUTE_1] = 1.00;
	YMF262Volumes[BURN_SND_YMF262_YMF262_ROUTE_2] = 1.00;
	YMF262RouteDirs[BURN_SND_YMF262_YMF262_ROUTE_1] = BURN_SND_ROUTE_LEFT;
	YMF262RouteDirs[BURN_SND_YMF262_YMF262_ROUTE_2] = BURN_SND_ROUTE_RIGHT;

	return 0;
}

void BurnYMF262SetRoute(INT32 nIndex, double nVolume, INT32 nRouteDir)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YMF262Initted) bprintf(PRINT_ERROR, _T("BurnYMF262SetRoute called without init\n"));
	if (nIndex < 0 || nIndex > 1) bprintf(PRINT_ERROR, _T("BurnYMF262SetRoute called with invalid index %i\n"), nIndex);
#endif

	YMF262Volumes[nIndex] = nVolume;
	YMF262RouteDirs[nIndex] = nRouteDir;
}

void BurnYMF262Scan(INT32 nAction, INT32* pnMin)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YMF262Initted) bprintf(PRINT_ERROR, _T("BurnYMF262Scan called without init\n"));
#endif

	BurnTimerScan(nAction, pnMin);
	ymf262_save_state(ymfchip, nAction);

	if (nAction & ACB_WRITE) {
		nYMF262Position = 0;
		nFractionalPosition = 0;
		memset(pBuffer, 0, 4096 * 2 * sizeof(INT16));
	}
}
