#include "burnint.h"
#include "burn_ymf278b.h"

static INT32 (*BurnYMF278BStreamCallback)(INT32 nSoundRate);

static INT16* pBuffer;
static INT16* pYMF278BBuffer[2];

static INT32 bYMF278BAddSignal;

static UINT32 nSampleSize;
static INT32 nYMF278BPosition;
static INT32 nFractionalPosition;

static INT32 nBurnYMF278SoundRate;

static double YMF278BVolumes[2];
static INT32 YMF278BRouteDirs[2];

static INT32 uses_timer; // when init passed w/NULL IRQHandler, timer is disabled.

// ----------------------------------------------------------------------------
// Dummy functions

static INT32 YMF278BStreamCallbackDummy(INT32 /* nSoundRate */)
{
	return 0;
}


// ----------------------------------------------------------------------------
// Execute YMF278B for part of a frame

static void YMF278BRender(INT32 nSegmentLength)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YMF278BInitted) bprintf(PRINT_ERROR, _T("YMF278BRender called without init\n"));
#endif

	if (nYMF278BPosition >= nSegmentLength || !pBurnSoundOut) {
		return;
	}

//	bprintf(PRINT_NORMAL, _T("    YMF278B render %6i -> %6i\n"), nYMF278BPosition, nSegmentLength);

	nSegmentLength -= nYMF278BPosition;

	pYMF278BBuffer[0] = pBuffer + 0 * 4096 + 4 + nYMF278BPosition;
	pYMF278BBuffer[1] = pBuffer + 1 * 4096 + 4 + nYMF278BPosition;

	ymf278b_pcm_update(0, pYMF278BBuffer, nSegmentLength);

	nYMF278BPosition += nSegmentLength;
}

// ----------------------------------------------------------------------------
// Update the sound buffer

#define INTERPOLATE_ADD_SOUND_LEFT(route, buffer)																	\
	if ((YMF278BRouteDirs[route] & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) {									\
		nLeftSample[0] += (INT32)(pYMF278BBuffer[buffer][(nFractionalPosition >> 16) - 3]/* * YMF278BVolumes[route]*/);	\
		nLeftSample[1] += (INT32)(pYMF278BBuffer[buffer][(nFractionalPosition >> 16) - 2]/* * YMF278BVolumes[route]*/);	\
		nLeftSample[2] += (INT32)(pYMF278BBuffer[buffer][(nFractionalPosition >> 16) - 1]/* * YMF278BVolumes[route]*/);	\
		nLeftSample[3] += (INT32)(pYMF278BBuffer[buffer][(nFractionalPosition >> 16) - 0]/* * YMF278BVolumes[route]*/);	\
	}

#define INTERPOLATE_ADD_SOUND_RIGHT(route, buffer)																	\
	if ((YMF278BRouteDirs[route] & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) {									\
		nRightSample[0] += (INT32)(pYMF278BBuffer[buffer][(nFractionalPosition >> 16) - 3]/* * YMF278BVolumes[route]*/);	\
		nRightSample[1] += (INT32)(pYMF278BBuffer[buffer][(nFractionalPosition >> 16) - 2]/* * YMF278BVolumes[route]*/);	\
		nRightSample[2] += (INT32)(pYMF278BBuffer[buffer][(nFractionalPosition >> 16) - 1]/* * YMF278BVolumes[route]*/);	\
		nRightSample[3] += (INT32)(pYMF278BBuffer[buffer][(nFractionalPosition >> 16) - 0]/* * YMF278BVolumes[route]*/);	\
	}

void BurnYMF278BUpdate(INT32 nSegmentEnd)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YMF278BInitted) bprintf(PRINT_ERROR, _T("BurnYMF278BUpdate called without init\n"));
#endif

	INT16* pSoundBuf = pBurnSoundOut;

	if (nBurnSoundRate == 0 || pBurnSoundOut == NULL) {
		return;
	}

//	bprintf(PRINT_NORMAL, _T("    YMF278B render %6i -> %6i\n"), nYMF278BPosition, nSegmentEnd);

	INT32 nSegmentLength = nSegmentEnd;
	INT32 nSamplesNeeded = nSegmentEnd * nBurnYMF278SoundRate / nBurnSoundRate + 1;

	if (nSamplesNeeded < nYMF278BPosition) {
		nSamplesNeeded = nYMF278BPosition;
	}

	if (nSegmentLength > nBurnSoundLen) {
		nSegmentLength = nBurnSoundLen;
	}
	nSegmentLength <<= 1;

	YMF278BRender(nSamplesNeeded);

	pYMF278BBuffer[0] = pBuffer + 0 * 4096 + 4;
	pYMF278BBuffer[1] = pBuffer + 1 * 4096 + 4;

	for (INT32 i = (nFractionalPosition & 0xFFFF0000) >> 15; i < nSegmentLength; i += 2, nFractionalPosition += nSampleSize) {
		INT32 nLeftSample[4] = {0, 0, 0, 0};
		INT32 nRightSample[4] = {0, 0, 0, 0};
		INT32 nTotalLeftSample, nTotalRightSample;

		INTERPOLATE_ADD_SOUND_LEFT  (BURN_SND_YMF278B_YMF278B_ROUTE_1, 0)
		INTERPOLATE_ADD_SOUND_RIGHT (BURN_SND_YMF278B_YMF278B_ROUTE_1, 0)
		INTERPOLATE_ADD_SOUND_LEFT  (BURN_SND_YMF278B_YMF278B_ROUTE_2, 1)
		INTERPOLATE_ADD_SOUND_RIGHT (BURN_SND_YMF278B_YMF278B_ROUTE_2, 1)

		nTotalLeftSample  = INTERPOLATE4PS_16BIT((nFractionalPosition >> 4) & 0x0fff, nLeftSample[0], nLeftSample[1], nLeftSample[2], nLeftSample[3]);
		nTotalRightSample = INTERPOLATE4PS_16BIT((nFractionalPosition >> 4) & 0x0fff, nRightSample[0], nRightSample[1], nRightSample[2], nRightSample[3]);

		nTotalLeftSample  = BURN_SND_CLIP(nTotalLeftSample * YMF278BVolumes[BURN_SND_YMF278B_YMF278B_ROUTE_1]);
		nTotalRightSample = BURN_SND_CLIP(nTotalRightSample * YMF278BVolumes[BURN_SND_YMF278B_YMF278B_ROUTE_2]);

		if (bYMF278BAddSignal) {
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
			pYMF278BBuffer[0][i] = pYMF278BBuffer[0][(nFractionalPosition >> 16) + i];
			pYMF278BBuffer[1][i] = pYMF278BBuffer[1][(nFractionalPosition >> 16) + i];
		}

		nFractionalPosition &= 0xFFFF;

		nYMF278BPosition = nExtraSamples;
	}
}

// ----------------------------------------------------------------------------

void BurnYMF278BSelectRegister(INT32 nRegister, UINT8 nValue)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YMF278BInitted) bprintf(PRINT_ERROR, _T("BurnYMF278BSelectRegister called without init\n"));
#endif

	switch (nRegister) {
		case 0:
//			bprintf(PRINT_NORMAL, _T("    YMF278B register A -> %i\n"), nValue);
			YMF278B_control_port_0_A_w(nValue);
			break;
		case 1:
			YMF278B_control_port_0_B_w(nValue);
			break;
		case 2:
//			bprintf(PRINT_NORMAL, _T("    YMF278B register C -> %i\n"), nValue);
			YMF278B_control_port_0_C_w(nValue);
			break;
	}
}

void BurnYMF278BWriteRegister(INT32 nRegister, UINT8 nValue)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YMF278BInitted) bprintf(PRINT_ERROR, _T("BurnYMF278BWriteRegister called without init\n"));
#endif

	switch (nRegister) {
		case 0:
			YMF278BRender(BurnYMF278BStreamCallback(nBurnYMF278SoundRate));
			YMF278B_data_port_0_A_w(nValue);
			break;
		case 1:
			YMF278B_data_port_0_B_w(nValue);
			break;
		case 2:
			YMF278BRender(BurnYMF278BStreamCallback(nBurnYMF278SoundRate));
			YMF278B_data_port_0_C_w(nValue);
			break;
	}
}

void BurnYMF278BWrite(INT32 nRegister, UINT8 nValue)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YMF278BInitted) bprintf(PRINT_ERROR, _T("BurnYMF278BWrite called without init\n"));
#endif

	YMF278BRender(BurnYMF278BStreamCallback(nBurnYMF278SoundRate));
	ymf278b_write(0, nRegister, nValue);
}

UINT8 BurnYMF278BReadStatus()
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YMF278BInitted) bprintf(PRINT_ERROR, _T("BurnYMF278BReadStatus called without init\n"));
#endif

	YMF278BRender(BurnYMF278BStreamCallback(nBurnYMF278SoundRate));
	return YMF278B_status_port_0_r();
}

UINT8 BurnYMF278BReadData()
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YMF278BInitted) bprintf(PRINT_ERROR, _T("BurnYMF278BReadData called without init\n"));
#endif

	return YMF278B_data_port_0_r();
}

// ----------------------------------------------------------------------------

void BurnYMF278BReset()
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YMF278BInitted) bprintf(PRINT_ERROR, _T("BurnYMF278BReset called without init\n"));
#endif

	if (uses_timer)
		BurnTimerReset();
	ymf278b_reset();
}

void BurnYMF278BExit()
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YMF278BInitted) bprintf(PRINT_ERROR, _T("BurnYMF278BExit called without init\n"));
#endif

	if (!DebugSnd_YMF278BInitted) return;

	YMF278B_sh_stop();

	if (uses_timer)
		BurnTimerExit();

	BurnFree(pBuffer);
	
	DebugSnd_YMF278BInitted = 0;
}

INT32 BurnYMF278BInit(INT32 nClockFrequency, UINT8* YMF278BROM, INT32 YMF278BROMSize, void (*IRQCallback)(INT32, INT32))
{
	return BurnYMF278BInit(nClockFrequency, YMF278BROM, YMF278BROMSize, IRQCallback, BurnSynchroniseStream);
}

INT32 BurnYMF278BInit(INT32 nClockFrequency, UINT8* YMF278BROM, INT32 YMF278BROMSize, void (*IRQCallback)(INT32, INT32), INT32 (*StreamCallback)(INT32))
{
	DebugSnd_YMF278BInitted = 1;

	BurnYMF278BStreamCallback = YMF278BStreamCallbackDummy;
	if (StreamCallback) {
		BurnYMF278BStreamCallback = StreamCallback;
	}

	if (!nClockFrequency) {
		nClockFrequency = YMF278B_STD_CLOCK;
	}

	if (nClockFrequency & 0x80000000) {
		nClockFrequency &= ~0x80000000;
		nBurnYMF278SoundRate = 44100; // hw rate.  *temporary* silly hack for fuukifg3..
	} else {
		nBurnYMF278SoundRate = nClockFrequency / 768; // hw rate based on input clock (44100 @ STD clock)
	}

	if (nBurnSoundRate) nSampleSize = (UINT32)nBurnYMF278SoundRate * (1 << 16) / nBurnSoundRate;
	bYMF278BAddSignal = 0; // not used by any driver. (yet)

	uses_timer = (IRQCallback != NULL);

	if (uses_timer)
		BurnTimerInit(&ymf278b_timer_over, NULL);

	ymf278b_start(0, YMF278BROM, YMF278BROMSize, IRQCallback, BurnYMFTimerCallback, nClockFrequency);

	pBuffer = (INT16*)BurnMalloc(4096 * 2 * sizeof(INT16));
	memset(pBuffer, 0, 4096 * 2 * sizeof(INT16));

	nYMF278BPosition = 0;
	nFractionalPosition = 0;
	
	// default routes
	YMF278BVolumes[BURN_SND_YMF278B_YMF278B_ROUTE_1] = 1.00;
	YMF278BVolumes[BURN_SND_YMF278B_YMF278B_ROUTE_2] = 1.00;
	YMF278BRouteDirs[BURN_SND_YMF278B_YMF278B_ROUTE_1] = BURN_SND_ROUTE_LEFT;
	YMF278BRouteDirs[BURN_SND_YMF278B_YMF278B_ROUTE_2] = BURN_SND_ROUTE_RIGHT;

	return 0;
}

void BurnYMF278BSetRoute(INT32 nIndex, double nVolume, INT32 nRouteDir)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YMF278BInitted) bprintf(PRINT_ERROR, _T("BurnYMF278BSetRoute called without init\n"));
	if (nIndex < 0 || nIndex > 1) bprintf(PRINT_ERROR, _T("BurnYMF278BSetRoute called with invalid index %i\n"), nIndex);
#endif

	YMF278BVolumes[nIndex] = nVolume;
	YMF278BRouteDirs[nIndex] = nRouteDir;
}

void BurnYMF278BScan(INT32 nAction, INT32* pnMin)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YMF278BInitted) bprintf(PRINT_ERROR, _T("BurnYMF278BScan called without init\n"));
#endif

	if (uses_timer) {
		BurnTimerScan(nAction, pnMin);
	}

	ymf278b_scan(nAction, pnMin);

	if (nAction & ACB_WRITE && ~nAction & ACB_RUNAHEAD) {
		nYMF278BPosition = 0;
		nFractionalPosition = 0;
		memset(pBuffer, 0, 4096 * 2 * sizeof(INT16));
	}
}
