#include "burnint.h"
#include "burn_ymf271.h"

static INT32 (*BurnYMF271StreamCallback)(INT32 nSoundRate);

static INT16* pBuffer;
static INT16* pYMF271Buffer[4];

static INT32 bYMF271AddSignal;

static UINT32 nSampleSize;
static INT32 nYMF271Position;
static INT32 nFractionalPosition;

static INT32 nBurnYMF271SoundRate;

static double YMF271Volumes[4];
static INT32 YMF271RouteDirs[4];

// ----------------------------------------------------------------------------
// Dummy functions

static INT32 YMF271StreamCallbackDummy(INT32 /* nSoundRate */)
{
	return 0;
}


// ----------------------------------------------------------------------------
// Execute YMF271 for part of a frame

static void YMF271Render(INT32 nSegmentLength)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YMF271Initted) bprintf(PRINT_ERROR, _T("YMF271Render called without init\n"));
#endif

	if (nYMF271Position >= nSegmentLength || !pBurnSoundOut) {
		return;
	}

//	bprintf(PRINT_NORMAL, _T("    YMF271 render %6i -> %6i\n"), nYMF271Position, nSegmentLength);

	nSegmentLength -= nYMF271Position;

	pYMF271Buffer[0] = pBuffer + 0 * 4096 + 4 + nYMF271Position;
	pYMF271Buffer[1] = pBuffer + 1 * 4096 + 4 + nYMF271Position;
	pYMF271Buffer[2] = pBuffer + 2 * 4096 + 4 + nYMF271Position;
	pYMF271Buffer[3] = pBuffer + 3 * 4096 + 4 + nYMF271Position;

	ymf271_update(pYMF271Buffer, nSegmentLength);

	nYMF271Position += nSegmentLength;
}

// ----------------------------------------------------------------------------
// Update the sound buffer

#define INTERPOLATE_ADD_SOUND_LEFT(route, buffer)																	\
	if ((YMF271RouteDirs[route] & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) {									\
		nLeftSample[0] += (INT32)(pYMF271Buffer[buffer][(nFractionalPosition >> 16) - 3] * YMF271Volumes[route]);	\
		nLeftSample[1] += (INT32)(pYMF271Buffer[buffer][(nFractionalPosition >> 16) - 2] * YMF271Volumes[route]);	\
		nLeftSample[2] += (INT32)(pYMF271Buffer[buffer][(nFractionalPosition >> 16) - 1] * YMF271Volumes[route]);	\
		nLeftSample[3] += (INT32)(pYMF271Buffer[buffer][(nFractionalPosition >> 16) - 0] * YMF271Volumes[route]);	\
	}

#define INTERPOLATE_ADD_SOUND_RIGHT(route, buffer)																	\
	if ((YMF271RouteDirs[route] & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) {									\
		nRightSample[0] += (INT32)(pYMF271Buffer[buffer][(nFractionalPosition >> 16) - 3] * YMF271Volumes[route]);	\
		nRightSample[1] += (INT32)(pYMF271Buffer[buffer][(nFractionalPosition >> 16) - 2] * YMF271Volumes[route]);	\
		nRightSample[2] += (INT32)(pYMF271Buffer[buffer][(nFractionalPosition >> 16) - 1] * YMF271Volumes[route]);	\
		nRightSample[3] += (INT32)(pYMF271Buffer[buffer][(nFractionalPosition >> 16) - 0] * YMF271Volumes[route]);	\
	}

void BurnYMF271Update(INT32 nSegmentEnd)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YMF271Initted) bprintf(PRINT_ERROR, _T("BurnYMF271Update called without init\n"));
#endif

	INT16* pSoundBuf = pBurnSoundOut;

	if (nBurnSoundRate == 0 || pBurnSoundOut == NULL) {
		return;
	}

//	bprintf(PRINT_NORMAL, _T("    YMF271 render %6i -> %6i\n"), nYMF271Position, nSegmentEnd);

	INT32 nSegmentLength = nSegmentEnd;
	INT32 nSamplesNeeded = nSegmentEnd * nBurnYMF271SoundRate / nBurnSoundRate + 1;

	if (nSamplesNeeded < nYMF271Position) {
		nSamplesNeeded = nYMF271Position;
	}

	if (nSegmentLength > nBurnSoundLen) {
		nSegmentLength = nBurnSoundLen;
	}
	nSegmentLength <<= 1;

	YMF271Render(nSamplesNeeded);

	pYMF271Buffer[0] = pBuffer + 0 * 4096 + 4;
	pYMF271Buffer[1] = pBuffer + 1 * 4096 + 4;
	pYMF271Buffer[2] = pBuffer + 2 * 4096 + 4;
	pYMF271Buffer[3] = pBuffer + 3 * 4096 + 4;

	for (INT32 i = (nFractionalPosition & 0xFFFF0000) >> 15; i < nSegmentLength; i += 2, nFractionalPosition += nSampleSize) {
		INT32 nLeftSample[4] = {0, 0, 0, 0};
		INT32 nRightSample[4] = {0, 0, 0, 0};
		INT32 nTotalLeftSample, nTotalRightSample;

		INTERPOLATE_ADD_SOUND_LEFT  (BURN_SND_YMF271_YMF271_ROUTE_1, 0)
		INTERPOLATE_ADD_SOUND_RIGHT (BURN_SND_YMF271_YMF271_ROUTE_1, 0)
		INTERPOLATE_ADD_SOUND_LEFT  (BURN_SND_YMF271_YMF271_ROUTE_2, 1)
		INTERPOLATE_ADD_SOUND_RIGHT (BURN_SND_YMF271_YMF271_ROUTE_2, 1)
		INTERPOLATE_ADD_SOUND_LEFT  (BURN_SND_YMF271_YMF271_ROUTE_3, 2)
		INTERPOLATE_ADD_SOUND_RIGHT (BURN_SND_YMF271_YMF271_ROUTE_3, 2)
		INTERPOLATE_ADD_SOUND_LEFT  (BURN_SND_YMF271_YMF271_ROUTE_4, 3)
		INTERPOLATE_ADD_SOUND_RIGHT (BURN_SND_YMF271_YMF271_ROUTE_4, 3)

		nTotalLeftSample  = INTERPOLATE4PS_16BIT((nFractionalPosition >> 4) & 0x0fff, nLeftSample[0], nLeftSample[1], nLeftSample[2], nLeftSample[3]);
		nTotalRightSample = INTERPOLATE4PS_16BIT((nFractionalPosition >> 4) & 0x0fff, nRightSample[0], nRightSample[1], nRightSample[2], nRightSample[3]);

		nTotalLeftSample  = BURN_SND_CLIP(nTotalLeftSample);
		nTotalRightSample = BURN_SND_CLIP(nTotalRightSample);

		if (bYMF271AddSignal) {
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
			pYMF271Buffer[0][i] = pYMF271Buffer[0][(nFractionalPosition >> 16) + i];
			pYMF271Buffer[1][i] = pYMF271Buffer[1][(nFractionalPosition >> 16) + i];
			pYMF271Buffer[2][i] = pYMF271Buffer[2][(nFractionalPosition >> 16) + i];
			pYMF271Buffer[3][i] = pYMF271Buffer[3][(nFractionalPosition >> 16) + i];
		}

		nFractionalPosition &= 0xFFFF;

		nYMF271Position = nExtraSamples;
	}
}

// ----------------------------------------------------------------------------

void BurnYMF271UpdateStream()
{
	YMF271Render(BurnYMF271StreamCallback(nBurnYMF271SoundRate));
}

void BurnYMF271Write(INT32 nAddress, UINT8 nValue)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YMF271Initted) bprintf(PRINT_ERROR, _T("BurnYMF271Write called without init\n"));
#endif

	YMF271Render(BurnYMF271StreamCallback(nBurnYMF271SoundRate));
	ymf271_write(nAddress, nValue);
}

UINT8 BurnYMF271Read(INT32 nAddress)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YMF271Initted) bprintf(PRINT_ERROR, _T("BurnYMF271Read called without init\n"));
#endif

	YMF271Render(BurnYMF271StreamCallback(nBurnYMF271SoundRate));
	return ymf271_read(nAddress);
}


// ----------------------------------------------------------------------------
static int ymf271_timerover(int /*num*/, int c)
{
	return ymf271_timer_over(c);
}


void BurnYMF271Reset()
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YMF271Initted) bprintf(PRINT_ERROR, _T("BurnYMF271Reset called without init\n"));
#endif

	BurnTimerReset();
	ymf271_reset();
}

void BurnYMF271Exit()
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YMF271Initted) bprintf(PRINT_ERROR, _T("BurnYMF271Exit called without init\n"));
#endif

	if (!DebugSnd_YMF271Initted) return;

	ymf271_exit();

	BurnTimerExit();

	BurnFree(pBuffer);
	
	DebugSnd_YMF271Initted = 0;
}

INT32 BurnYMF271Init(INT32 nClockFrequency, UINT8 *rom, INT32 romsize, void (*IRQCallback)(INT32, INT32), INT32 nAdd)
{
	return BurnYMF271Init(nClockFrequency, rom, romsize, IRQCallback, BurnSynchroniseStream, nAdd);
}

INT32 BurnYMF271Init(INT32 nClockFrequency, UINT8 *rom, INT32 romsize, void (*IRQCallback)(INT32, INT32), INT32 (*StreamCallback)(INT32), INT32 nAdd)
{
	DebugSnd_YMF271Initted = 1;

	BurnYMF271StreamCallback = YMF271StreamCallbackDummy;
	if (StreamCallback) {
		BurnYMF271StreamCallback = StreamCallback;
	}

	nBurnYMF271SoundRate = nClockFrequency / 384; // hw rate based on input clock

	if (nBurnSoundRate) nSampleSize = (UINT32)nBurnYMF271SoundRate * (1 << 16) / nBurnSoundRate;
	bYMF271AddSignal = nAdd;

	BurnTimerInit(&ymf271_timerover, NULL);
	ymf271_init(nClockFrequency, rom, romsize, IRQCallback, BurnYMF262TimerCallback);

	pBuffer = (INT16*)BurnMalloc((4096 + 4) * 4 * sizeof(INT16));
	memset(pBuffer, 0, (4096 + 4) * 4 * sizeof(INT16));

	nYMF271Position = 0;
	nFractionalPosition = 0;

	// default routes
	YMF271Volumes[BURN_SND_YMF271_YMF271_ROUTE_1] = 1.00;
	YMF271Volumes[BURN_SND_YMF271_YMF271_ROUTE_2] = 1.00;
	YMF271RouteDirs[BURN_SND_YMF271_YMF271_ROUTE_1] = BURN_SND_ROUTE_LEFT;
	YMF271RouteDirs[BURN_SND_YMF271_YMF271_ROUTE_2] = BURN_SND_ROUTE_RIGHT;

	YMF271Volumes[BURN_SND_YMF271_YMF271_ROUTE_3] = 1.00;
	YMF271Volumes[BURN_SND_YMF271_YMF271_ROUTE_4] = 1.00;
	YMF271RouteDirs[BURN_SND_YMF271_YMF271_ROUTE_3] = BURN_SND_ROUTE_LEFT;
	YMF271RouteDirs[BURN_SND_YMF271_YMF271_ROUTE_4] = BURN_SND_ROUTE_RIGHT;

	return 0;
}

void BurnYMF271SetRoute(INT32 nIndex, double nVolume, INT32 nRouteDir)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YMF271Initted) bprintf(PRINT_ERROR, _T("BurnYMF271SetRoute called without init\n"));
	if (nIndex < 0 || nIndex > 3) bprintf(PRINT_ERROR, _T("BurnYMF271SetRoute called with invalid index %i\n"), nIndex);
#endif

	YMF271Volumes[nIndex] = nVolume;
	YMF271RouteDirs[nIndex] = nRouteDir;
}

void BurnYMF271Scan(INT32 nAction, INT32* pnMin)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YMF271Initted) bprintf(PRINT_ERROR, _T("BurnYMF271Scan called without init\n"));
#endif

	BurnTimerScan(nAction, pnMin);
	ymf271_scan(nAction);

	if (nAction & ACB_WRITE && ~nAction & ACB_RUNAHEAD) {
		nYMF271Position = 0;
		nFractionalPosition = 0;
		memset(pBuffer, 0, 4096 * 2 * sizeof(INT16));
	}
}
