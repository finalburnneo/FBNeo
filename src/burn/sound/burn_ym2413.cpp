#include "burnint.h"
#include "burn_sound.h"
#include "burn_ym2413.h"

void (*BurnYM2413Render)(INT16* pSoundBuf, INT32 nSegmentLength);

//unsigned char BurnYM2151Registers[0x0100];
//unsigned int nBurnCurrentYM2151Register;

static INT32 nBurnYM2413SoundRate;

static INT16* pBuffer;
static INT16* pYM2413Buffer[2];

static INT32 nBurnPosition;
static UINT32 nSampleSize;
static INT32 nFractionalPosition;
static UINT32 nSamplesRendered;

static INT32 nYM2413Volume;

static void YM2413RenderResample(INT16* pSoundBuf, INT32 nSegmentLength)
{
	nBurnPosition += nSegmentLength;

	if (nBurnPosition >= nBurnSoundRate) {
		nBurnPosition = nSegmentLength;

		pYM2413Buffer[0][1] = pYM2413Buffer[0][(nFractionalPosition >> 16) - 3];
		pYM2413Buffer[0][2] = pYM2413Buffer[0][(nFractionalPosition >> 16) - 2];
		pYM2413Buffer[0][3] = pYM2413Buffer[0][(nFractionalPosition >> 16) - 1];

		pYM2413Buffer[1][1] = pYM2413Buffer[1][(nFractionalPosition >> 16) - 3];
		pYM2413Buffer[1][2] = pYM2413Buffer[1][(nFractionalPosition >> 16) - 2];
		pYM2413Buffer[1][3] = pYM2413Buffer[1][(nFractionalPosition >> 16) - 1];

		nSamplesRendered -= (nFractionalPosition >> 16) - 4;

		for (UINT32 i = 0; i <= nSamplesRendered; i++) {
			pYM2413Buffer[0][4 + i] = pYM2413Buffer[0][(nFractionalPosition >> 16) + i];
			pYM2413Buffer[1][4 + i] = pYM2413Buffer[1][(nFractionalPosition >> 16) + i];
		}

		nFractionalPosition &= 0x0000FFFF;
		nFractionalPosition |= 4 << 16;
	}

	pYM2413Buffer[0] = pBuffer + 4 + nSamplesRendered;
	pYM2413Buffer[1] = pBuffer + 4 + nSamplesRendered + 65536;

	YM2413UpdateOne(0, pYM2413Buffer, (UINT32)(nBurnPosition + 1) * nBurnYM2413SoundRate / nBurnSoundRate - nSamplesRendered);
	nSamplesRendered += (UINT32)(nBurnPosition + 1) * nBurnYM2413SoundRate / nBurnSoundRate - nSamplesRendered;

	pYM2413Buffer[0] = pBuffer;
	pYM2413Buffer[1] = pBuffer + 65536;

	nSegmentLength <<= 1;

	for (INT32 i = 0; i < nSegmentLength; i += 2, nFractionalPosition += nSampleSize) {

		INT32 nSample = 	INTERPOLATE4PS_CUSTOM((nFractionalPosition >> 4) & 0x0FFF,
												 pYM2413Buffer[0][(nFractionalPosition >> 16) - 3],
												 pYM2413Buffer[0][(nFractionalPosition >> 16) - 2],
												 pYM2413Buffer[0][(nFractionalPosition >> 16) - 1],
												 pYM2413Buffer[0][(nFractionalPosition >> 16) - 0],
												 nYM2413Volume / 100)
				+ INTERPOLATE4PS_CUSTOM((nFractionalPosition >> 4) & 0x0FFF,
												 pYM2413Buffer[1][(nFractionalPosition >> 16) - 3],
												 pYM2413Buffer[1][(nFractionalPosition >> 16) - 2],
												 pYM2413Buffer[1][(nFractionalPosition >> 16) - 1],
												 pYM2413Buffer[1][(nFractionalPosition >> 16) - 0],
												 nYM2413Volume / 100);
												 
		pSoundBuf[i + 0] = nSample;
		pSoundBuf[i + 1] = nSample;
	}
}

static void YM2413RenderNormal(INT16* pSoundBuf, INT32 nSegmentLength)
{
	nBurnPosition += nSegmentLength;

	pYM2413Buffer[0] = pBuffer;
	pYM2413Buffer[1] = pBuffer + nSegmentLength;

	YM2413UpdateOne(0, pYM2413Buffer, nSegmentLength);
	
	for (INT32 n = 0; n < nSegmentLength; n++) {
		INT32 nSample1 = pYM2413Buffer[0][n] * (nYM2413Volume >> 10);
		nSample1 >>= 8;
		INT32 nSample2 = pYM2413Buffer[1][n] * (nYM2413Volume >> 10);
		nSample2 >>= 8;
	
		INT32 nSample = nSample1 + nSample2;
		if (nSample < -32768) nSample = -32768;
		if (nSample > 32767) nSample = 32767;
	
		pSoundBuf[(n << 1) + 0] = nSample;
		pSoundBuf[(n << 1) + 1] = nSample;
	}
}

void BurnYM2413Reset()
{
	YM2413ResetChip(0);
}

void BurnYM2413Exit()
{
	YM2413Shutdown();

	if (pBuffer) {
		free(pBuffer);
		pBuffer = NULL;
	}
}

INT32 BurnYM2413Init(INT32 nClockFrequency, float nVolume)
{
	if (nBurnSoundRate <= 0) {
		YM2413Init(1, nClockFrequency, 11025);
		return 0;
	}

	if (nFMInterpolation == 3) {
		// Set YM2413 core samplerate to match the hardware
		nBurnYM2413SoundRate = nClockFrequency >> 6;
		// Bring YM2413 core samplerate within usable range
		while (nBurnYM2413SoundRate > nBurnSoundRate * 3) {
			nBurnYM2413SoundRate >>= 1;
		}

		BurnYM2413Render = YM2413RenderResample;

		nYM2413Volume = (INT32)((double)16384.0 * 100.0 / nVolume);
	} else {
		nBurnYM2413SoundRate = nBurnSoundRate;
		BurnYM2413Render = YM2413RenderNormal;

		nYM2413Volume = (INT32)((double)65536.0 * 100.0 / nVolume);
	}

	YM2413Init(1, nClockFrequency, nBurnYM2413SoundRate);

	pBuffer = (INT16*)malloc(65536 * 2 * sizeof(INT16));
	memset(pBuffer, 0, 65536 * 2 * sizeof(INT16));

	nSampleSize = (UINT32)nBurnYM2413SoundRate * (1 << 16) / nBurnSoundRate;
	nFractionalPosition = 4 << 16;
	nSamplesRendered = 0;
	nBurnPosition = 0;

	return 0;
}

void BurnYM2413IncreaseVolume(INT32 nFactor)
{
	nYM2413Volume *= nFactor;
}

void BurnYM2413DecreaseVolume(INT32 nFactor)
{
	nYM2413Volume /= nFactor;
}

void BurnYM2413Scan(INT32 nAction)
{
	if ((nAction & ACB_DRIVER_DATA) == 0) {
		return;
	}
//	SCAN_VAR(nBurnCurrentYM2151Register);
//	SCAN_VAR(BurnYM2151Registers);

	if (nAction & ACB_WRITE) {
//		for (int i = 0; i < 0x0100; i++) {
//			YM2151WriteReg(0, i, BurnYM2151Registers[i]);
//		}
	}
}
