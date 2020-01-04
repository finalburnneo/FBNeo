// Megadrive ym2612 interface for eke-eke's genplus-gx ym2612, based on MAME's ym2612
// Note: if a mono route is needed (in the future), uncomment in the main render loop.

#include "burnint.h"
#include "burn_md2612.h"

void (*BurnMD2612Update)(INT16* pSoundBuf, INT32 nSegmentEnd);

static INT32 (*BurnMD2612StreamCallback)(INT32 nSoundRate);

static INT32 nBurnMD2612SoundRate;

static INT16* pBuffer;
static INT16* pMD2612Buffer[2];

static INT32 nMD2612Position;

static UINT32 nSampleSize;
static INT32 nFractionalPosition;

static INT32 nNumChips = 0;
static INT32 bMD2612AddSignal;

static double MD2612Volumes[2];
static INT32 MD2612RouteDirs[2];

// ----------------------------------------------------------------------------
// Dummy functions

static void MD2612UpdateDummy(INT16*, INT32)
{
	return;
}

static INT32 MD2612StreamCallbackDummy(INT32)
{
	return 0;
}

// ----------------------------------------------------------------------------
// Execute MD2612 for part of a frame

static void MD2612Render(INT32 nSegmentLength)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YM2612Initted) bprintf(PRINT_ERROR, _T("MD2612Render called without init\n"));
#endif
	
	if (nMD2612Position >= nSegmentLength) {
		return;
	}

	nSegmentLength -= nMD2612Position;
	
	pMD2612Buffer[0] = pBuffer + 0 * 4096 + 4 + nMD2612Position;
	pMD2612Buffer[1] = pBuffer + 1 * 4096 + 4 + nMD2612Position;

	MDYM2612Update(&pMD2612Buffer[0], nSegmentLength);

	nMD2612Position += nSegmentLength;
}

// ----------------------------------------------------------------------------

// Update the sound buffer

#define INTERPOLATE_ADD_SOUND_LEFT(route, buffer)																	\
	if ((MD2612RouteDirs[route] & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) {									\
		nLeftSample[0] += (INT32)(pMD2612Buffer[buffer][(nFractionalPosition >> 16) - 3]/* * MD2612Volumes[route]*/);	\
		nLeftSample[1] += (INT32)(pMD2612Buffer[buffer][(nFractionalPosition >> 16) - 2]/* * MD2612Volumes[route]*/);	\
		nLeftSample[2] += (INT32)(pMD2612Buffer[buffer][(nFractionalPosition >> 16) - 1]/* * MD2612Volumes[route]*/);	\
		nLeftSample[3] += (INT32)(pMD2612Buffer[buffer][(nFractionalPosition >> 16) - 0]/* * MD2612Volumes[route]*/);	\
	}

#define INTERPOLATE_ADD_SOUND_RIGHT(route, buffer)																	\
	if ((MD2612RouteDirs[route] & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) {									\
		nRightSample[0] += (INT32)(pMD2612Buffer[buffer][(nFractionalPosition >> 16) - 3]/* * MD2612Volumes[route]*/);	\
		nRightSample[1] += (INT32)(pMD2612Buffer[buffer][(nFractionalPosition >> 16) - 2]/* * MD2612Volumes[route]*/);	\
		nRightSample[2] += (INT32)(pMD2612Buffer[buffer][(nFractionalPosition >> 16) - 1]/* * MD2612Volumes[route]*/);	\
		nRightSample[3] += (INT32)(pMD2612Buffer[buffer][(nFractionalPosition >> 16) - 0]/* * MD2612Volumes[route]*/);	\
	}

static void MD2612UpdateResample(INT16* pSoundBuf, INT32 nSegmentEnd)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YM2612Initted) bprintf(PRINT_ERROR, _T("MD2612UpdateResample called without init\n"));
#endif

	INT32 nSegmentLength = nSegmentEnd;
	INT32 nSamplesNeeded = nSegmentEnd * nBurnMD2612SoundRate / nBurnSoundRate + 1;

	if (nSamplesNeeded < nMD2612Position) {
		nSamplesNeeded = nMD2612Position;
	}

	if (nSegmentLength > nBurnSoundLen) {
		nSegmentLength = nBurnSoundLen;
	}
	nSegmentLength <<= 1;

	MD2612Render(nSamplesNeeded);

	pMD2612Buffer[0] = pBuffer + 0 * 4096 + 4;
	pMD2612Buffer[1] = pBuffer + 1 * 4096 + 4;

	for (INT32 i = (nFractionalPosition & 0xFFFF0000) >> 15; i < nSegmentLength; i += 2, nFractionalPosition += nSampleSize) {
		INT32 nLeftSample[4] = {0, 0, 0, 0};
		INT32 nRightSample[4] = {0, 0, 0, 0};
		INT32 nTotalLeftSample, nTotalRightSample;

		INTERPOLATE_ADD_SOUND_LEFT  (BURN_SND_MD2612_MD2612_ROUTE_1, 0)
		//INTERPOLATE_ADD_SOUND_RIGHT (BURN_SND_MD2612_MD2612_ROUTE_1, 0) // uncomment these to fix MONO, if needed in the future. (unlikely)
		//INTERPOLATE_ADD_SOUND_LEFT  (BURN_SND_MD2612_MD2612_ROUTE_2, 1)
		INTERPOLATE_ADD_SOUND_RIGHT (BURN_SND_MD2612_MD2612_ROUTE_2, 1)

		nTotalLeftSample  = INTERPOLATE4PS_16BIT((nFractionalPosition >> 4) & 0x0fff, nLeftSample[0], nLeftSample[1], nLeftSample[2], nLeftSample[3]);
		nTotalRightSample = INTERPOLATE4PS_16BIT((nFractionalPosition >> 4) & 0x0fff, nRightSample[0], nRightSample[1], nRightSample[2], nRightSample[3]);

		nTotalLeftSample  = BURN_SND_CLIP(nTotalLeftSample * MD2612Volumes[BURN_SND_MD2612_MD2612_ROUTE_1]);
		nTotalRightSample = BURN_SND_CLIP(nTotalRightSample * MD2612Volumes[BURN_SND_MD2612_MD2612_ROUTE_2]);

		if (bMD2612AddSignal) {
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
			pMD2612Buffer[0][i] = pMD2612Buffer[0][(nFractionalPosition >> 16) + i];
			pMD2612Buffer[1][i] = pMD2612Buffer[1][(nFractionalPosition >> 16) + i];
		}

		nFractionalPosition &= 0xFFFF;

		nMD2612Position = nExtraSamples;
	}
}

// ----------------------------------------------------------------------------
// Callbacks for YM2612 core
void BurnMD2612UpdateRequest()
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YM2612Initted) bprintf(PRINT_ERROR, _T("YM2612UpdateRequest called without init\n"));
#endif

	MD2612Render(BurnMD2612StreamCallback(nBurnMD2612SoundRate));
}

// ----------------------------------------------------------------------------
// Initialisation, etc.

void BurnMD2612Reset()
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YM2612Initted) bprintf(PRINT_ERROR, _T("BurnMD2612Reset called without init\n"));
#endif

	MDYM2612Reset();
}

void BurnMD2612Exit()
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YM2612Initted) bprintf(PRINT_ERROR, _T("BurnMD2612Exit called without init\n"));
#endif

	if (!DebugSnd_YM2612Initted) return;

	MDYM2612Exit();

	BurnFree(pBuffer);
	
	nNumChips = 0;
	bMD2612AddSignal = 0;
	
	DebugSnd_YM2612Initted = 0;
}

INT32 BurnMD2612Init(INT32 num, INT32 bIsPal, INT32 (*StreamCallback)(INT32), INT32 bAddSignal)
{
	if (num > 1) {
		bprintf(0, _T("BurnMD2612Init(): MD2612 only supports 1 chip!\n"));
		return 0;
	}

	DebugSnd_YM2612Initted = 1;

	if (nBurnSoundRate <= 0) {
		BurnMD2612StreamCallback = MD2612StreamCallbackDummy;

		BurnMD2612Update = MD2612UpdateDummy;

		MDYM2612Init();
		return 0;
	}

	BurnMD2612StreamCallback = StreamCallback;

	if (!StreamCallback) {
		bprintf(0, _T("BurnMD2612Init(): StreamCallback is NULL! Crashing in 3..2...1....\n"));
	}

	// Megadrive's 2612 runs at 53267hz NTSC, 52781hz PAL
	nBurnMD2612SoundRate = (bIsPal) ? 52781 : 53267;
	BurnMD2612Update = MD2612UpdateResample;
	nSampleSize = (UINT32)nBurnMD2612SoundRate * (1 << 16) / nBurnSoundRate;
	
	MDYM2612Init();

	pBuffer = (INT16*)BurnMalloc(4096 * 2 * num * sizeof(INT16));
	memset(pBuffer, 0, 4096 * 2 * num * sizeof(INT16));
	
	nMD2612Position = 0;
	nFractionalPosition = 0;
	
	nNumChips = num;
	bMD2612AddSignal = bAddSignal;
	
	// default routes
	MD2612Volumes[BURN_SND_MD2612_MD2612_ROUTE_1] = 1.00;
	MD2612Volumes[BURN_SND_MD2612_MD2612_ROUTE_2] = 1.00;
	MD2612RouteDirs[BURN_SND_MD2612_MD2612_ROUTE_1] = BURN_SND_ROUTE_LEFT;
	MD2612RouteDirs[BURN_SND_MD2612_MD2612_ROUTE_2] = BURN_SND_ROUTE_RIGHT;
	
	return 0;
}

void BurnMD2612SetRoute(INT32 nChip, INT32 nIndex, double nVolume, INT32 nRouteDir)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YM2612Initted) bprintf(PRINT_ERROR, _T("BurnMD2612SetRoute called without init\n"));
	if (nIndex < 0 || nIndex > 1) bprintf(PRINT_ERROR, _T("BurnMD2612SetRoute called with invalid index %i\n"), nIndex);
	if (nChip >= nNumChips) bprintf(PRINT_ERROR, _T("BurnMD2612SetRoute called with invalid chip %i\n"), nChip);
#endif

	if (nChip == 0) {
		MD2612Volumes[nIndex] = nVolume;
		MD2612RouteDirs[nIndex] = nRouteDir;
	}
}

void BurnMD2612Scan(INT32 nAction, INT32* pnMin)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YM2612Initted) bprintf(PRINT_ERROR, _T("BurnMD2612Scan called without init\n"));
#endif

	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(nMD2612Position);

		if (nAction & ACB_WRITE) {
			MDYM2612LoadContext();

			nMD2612Position = 0;
			nFractionalPosition = 0;
			memset(pBuffer, 0, 4096 * 2 * 1 * sizeof(INT16));
		} else {
			MDYM2612SaveContext();
		}
	}
}
