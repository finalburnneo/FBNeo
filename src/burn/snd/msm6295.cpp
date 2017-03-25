// OKI MSM6295 module
// Emulation by Jan Klaassen

#include <math.h>
#include "burnint.h"
#include "msm6295.h"
#include "burn_sound.h"

UINT8* MSM6295ROM;

UINT32 nMSM6295Status[MAX_MSM6295];

struct MSM6295ChannelInfo {
	INT32 nOutput;
	INT32 nVolume;
	INT32 nPosition;
	INT32 nSampleCount;
	INT32 nSample;
	INT32 nStep;
	INT32 nDelta;

	INT32 nBufPos;
	INT32 nPlaying;
};

static struct {
	INT32 nVolume;
	INT32 nSampleRate;
	INT32 nSampleSize;
	INT32 nFractionalPosition;

	// All current settings for each channel
	MSM6295ChannelInfo ChannelInfo[4];

	// Used for sending commands
	bool bIsCommand;
	INT32 nSampleInfo;
	
	INT32 nOutputDir;

} MSM6295[MAX_MSM6295];

static UINT8 *pBankPointer[MAX_MSM6295][0x40000/0x100];
INT32 nLastMSM6295Chip;

void MSM6295SetBank(INT32 nChip, UINT8 *pRomData, INT32 nStart, INT32 nEnd)
{
#if defined FBA_DEBUG
	if (!DebugSnd_MSM6295Initted) bprintf(PRINT_ERROR, _T("MSM6295SetBank called without init\n"));
	if (nChip > nLastMSM6295Chip) bprintf(PRINT_ERROR, _T("MSM6295SetBank called with invalid chip number %x\n"), nChip);
	if (nStart >= nEnd || nStart < 0 || nStart >= 0x40000) bprintf(PRINT_ERROR, _T("MSM6295SetBank (Chip %d) called with invalid nStart %x\n"), nChip, nStart);
	if (nStart >= nEnd || nEnd < 0 || nEnd >= 0x40000) bprintf(PRINT_ERROR, _T("MSM6295SetBank (Chip %d) called with invalid nEnd %x\n"), nChip, nEnd);
#endif

	if (pRomData == NULL) return;

//	if (nEnd >= nStart) return;
//	if (nEnd >= 0x40000) nEnd = 0x40000;

	for (INT32 i = 0; i < ((nEnd - nStart) >> 8) + 1; i++)
	{
		pBankPointer[nChip][(nStart >> 8) + i] = pRomData + (i << 8);
	}
}

#if defined FBA_DEBUG
static inline UINT8 MSM6295ReadData(INT32 nChip, UINT32 nAddress)
{
	nAddress &= 0x3ffff;

	if (pBankPointer[nChip][(nAddress >> 8)]) {
		return pBankPointer[nChip][(nAddress >> 8)][(nAddress & 0xff)];
	} else {
		return 0;
	}
}
#else
#define MSM6295ReadData(chip, addr)	\
	pBankPointer[chip][((addr) >> 8) & 0x3ff][((addr) & 0xff)]
#endif

static UINT32 MSM6295VolumeTable[16];
static INT32 MSM6295DeltaTable[49 * 16];
static INT32 MSM6295StepShift[8] = {-1, -1, -1, -1, 2, 4, 6, 8};

static INT32* MSM6295ChannelData[MAX_MSM6295][4];

static INT32* pLeftBuffer = NULL;
static INT32* pRightBuffer = NULL;

static bool bAdd;

void MSM6295Reset(INT32 nChip)
{
#if defined FBA_DEBUG
	if (!DebugSnd_MSM6295Initted) bprintf(PRINT_ERROR, _T("MSM6295Reset called without init\n"));
	if (nChip > nLastMSM6295Chip) bprintf(PRINT_ERROR, _T("MSM6295Reset called with invalid chip number %x\n"), nChip);
#endif

	nMSM6295Status[nChip] = 0;
	MSM6295[nChip].bIsCommand = false;

	MSM6295[nChip].nFractionalPosition = 0;

	for (INT32 nChannel = 0; nChannel < 4; nChannel++) {
		MSM6295[nChip].ChannelInfo[nChannel].nPlaying = 0;
		memset(MSM6295ChannelData[nChip][nChannel], 0, 0x1000 * sizeof(INT32));
		MSM6295[nChip].ChannelInfo[nChannel].nBufPos = 4;
	}

	// set bank data only if DataPointer has not already been set
	if (pBankPointer[nChip][0] == NULL) {
		MSM6295SetBank(nChip, MSM6295ROM + (nChip * 0x0100000), 0, 0x3ffff); // set initial bank (compatibility)
	}
}

INT32 MSM6295Scan(INT32 nChip, INT32 )
{
#if defined FBA_DEBUG
	if (!DebugSnd_MSM6295Initted) bprintf(PRINT_ERROR, _T("MSM6295Scan called without init\n"));
	if (nChip > nLastMSM6295Chip) bprintf(PRINT_ERROR, _T("MSM6295Scan called with invalid chip number %x\n"), nChip);
#endif

	INT32 nSampleSize = MSM6295[nChip].nSampleSize;
	SCAN_VAR(MSM6295[nChip]);
	MSM6295[nChip].nSampleSize = nSampleSize;

	SCAN_VAR(nMSM6295Status[nChip]);

	for (INT32 i = 0; i < 4; i++) {
		SCAN_VAR(MSM6295[nChip].ChannelInfo[i].nPlaying);
	}

	return 0;
}

static void MSM6295Render_Linear(INT32 nChip, INT32* pLeftBuf, INT32 *pRightBuf, INT32 nSegmentLength)
{
	static INT32 nPreviousSample[MAX_MSM6295], nCurrentSample[MAX_MSM6295];
	INT32 nVolume = MSM6295[nChip].nVolume;
	INT32 nFractionalPosition = MSM6295[nChip].nFractionalPosition;

	INT32 nChannel, nDelta, nSample;
	MSM6295ChannelInfo* pChannelInfo;

	while (nSegmentLength--) {
		if (nFractionalPosition >= 0x1000) {

			nPreviousSample[nChip] = nCurrentSample[nChip];

			do {
				nCurrentSample[nChip] = 0;

				for (nChannel = 0; nChannel < 4; nChannel++) {
					if (nMSM6295Status[nChip] & (1 << nChannel)) {
						pChannelInfo = &MSM6295[nChip].ChannelInfo[nChannel];

						// Check for end of sample
						if (pChannelInfo->nSampleCount-- == 0) {
							nMSM6295Status[nChip] &= ~(1 << nChannel);
							MSM6295[nChip].ChannelInfo[nChannel].nPlaying = 0;
							continue;
						}

						// Get new delta from ROM
						if (pChannelInfo->nPosition & 1) {
							nDelta = pChannelInfo->nDelta & 0x0F;
						} else {
							pChannelInfo->nDelta = MSM6295ReadData(nChip, (pChannelInfo->nPosition >> 1) & 0x3ffff);
							nDelta = pChannelInfo->nDelta >> 4;
						}

						// Compute new sample
						nSample = pChannelInfo->nSample + MSM6295DeltaTable[(pChannelInfo->nStep << 4) + nDelta];
						if (nSample > 2047) {
							nSample = 2047;
						} else {
							if (nSample < -2048) {
								nSample = -2048;
							}
						}
						pChannelInfo->nSample = nSample;
						pChannelInfo->nOutput = (nSample * pChannelInfo->nVolume);

						// Update step value
						pChannelInfo->nStep = pChannelInfo->nStep + MSM6295StepShift[nDelta & 7];
						if (pChannelInfo->nStep > 48) {
							pChannelInfo->nStep = 48;
						} else {
							if (pChannelInfo->nStep < 0) {
								pChannelInfo->nStep = 0;
							}
						}

						nCurrentSample[nChip] += pChannelInfo->nOutput / 16;

						// Advance sample position
						pChannelInfo->nPosition++;
					}
				}

				nFractionalPosition -= 0x1000;

			} while (nFractionalPosition >= 0x1000);
		}

		// Compute linearly interpolated sample
		nSample = nPreviousSample[nChip] + (((nCurrentSample[nChip] - nPreviousSample[nChip]) * nFractionalPosition) >> 12);

		// Scale all 4 channels
		nSample *= nVolume;

		if ((MSM6295[nChip].nOutputDir & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) {
			*pLeftBuf++ += nSample;
		}
		if ((MSM6295[nChip].nOutputDir & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) {
			*pRightBuf++ += nSample;
		}

		nFractionalPosition += MSM6295[nChip].nSampleSize;
	}

	MSM6295[nChip].nFractionalPosition = nFractionalPosition;
}

static void MSM6295Render_Cubic(INT32 nChip, INT32* pLeftBuf, INT32 *pRightBuf, INT32 nSegmentLength)
{
	INT32 nVolume = MSM6295[nChip].nVolume;
	INT32 nFractionalPosition;

	INT32 nChannel, nDelta, nSample, nOutput;
	MSM6295ChannelInfo* pChannelInfo;

	while (nSegmentLength--) {

		nOutput = 0;

		for (nChannel = 0; nChannel < 4; nChannel++) {
			pChannelInfo = &MSM6295[nChip].ChannelInfo[nChannel];
			nFractionalPosition = MSM6295[nChip].nFractionalPosition;

			if (nMSM6295Status[nChip] & (1 << nChannel)) {

				while (nFractionalPosition >= 0x1000) {

					// Check for end of sample
					if (pChannelInfo->nSampleCount-- <= 0) {
						if (pChannelInfo->nSampleCount <= -2) {
							nMSM6295Status[nChip] &= ~(1 << nChannel);
							MSM6295[nChip].ChannelInfo[nChannel].nPlaying = 0;
						}

						MSM6295ChannelData[nChip][nChannel][pChannelInfo->nBufPos++] = pChannelInfo->nOutput / 16;

						break;

					} else {
						// Get new delta from ROM
						if (pChannelInfo->nPosition & 1) {
							nDelta = pChannelInfo->nDelta & 0x0F;
						} else {
							pChannelInfo->nDelta = MSM6295ReadData(nChip, (pChannelInfo->nPosition >> 1) & 0x3ffff);
							nDelta = pChannelInfo->nDelta >> 4;
						}

						// Compute new sample
						nSample = pChannelInfo->nSample + MSM6295DeltaTable[(pChannelInfo->nStep << 4) + nDelta];
						if (nSample > 2047) {
							nSample = 2047;
						} else {
							if (nSample < -2048) {
								nSample = -2048;
							}
						}
						pChannelInfo->nSample = nSample;
						pChannelInfo->nOutput = nSample * pChannelInfo->nVolume;

						// Update step value
						pChannelInfo->nStep = pChannelInfo->nStep + MSM6295StepShift[nDelta & 7];
						if (pChannelInfo->nStep > 48) {
							pChannelInfo->nStep = 48;
						} else {
							if (pChannelInfo->nStep < 0) {
								pChannelInfo->nStep = 0;
							}
						}

						// The interpolator needs a 16-bit sample, pChannelInfo->nOutput is now a 20-bit number
						MSM6295ChannelData[nChip][nChannel][pChannelInfo->nBufPos++] = pChannelInfo->nOutput / 16;

						// Advance sample position
						pChannelInfo->nPosition++;
						nFractionalPosition -= 0x1000;
					}
				}

				if (pChannelInfo->nBufPos > 0x0FF0) {
					MSM6295ChannelData[nChip][nChannel][0] = MSM6295ChannelData[nChip][nChannel][pChannelInfo->nBufPos - 4];
					MSM6295ChannelData[nChip][nChannel][1] = MSM6295ChannelData[nChip][nChannel][pChannelInfo->nBufPos - 3];
					MSM6295ChannelData[nChip][nChannel][2] = MSM6295ChannelData[nChip][nChannel][pChannelInfo->nBufPos - 2];
					MSM6295ChannelData[nChip][nChannel][3] = MSM6295ChannelData[nChip][nChannel][pChannelInfo->nBufPos - 1];
					pChannelInfo->nBufPos = 4;
				}

				nOutput += INTERPOLATE4PS_16BIT(nFractionalPosition,
												MSM6295ChannelData[nChip][nChannel][pChannelInfo->nBufPos - 4],
												MSM6295ChannelData[nChip][nChannel][pChannelInfo->nBufPos - 3],
												MSM6295ChannelData[nChip][nChannel][pChannelInfo->nBufPos - 2],
												MSM6295ChannelData[nChip][nChannel][pChannelInfo->nBufPos - 1]);
			} else {
				// Ramp channel output to 0
				if (pChannelInfo->nOutput != 0) {
					INT32 nRamp = 2048 * 256 * 256 / nBurnSoundRate;
					if (pChannelInfo->nOutput > 0) {
						if (pChannelInfo->nOutput > nRamp) {
							pChannelInfo->nOutput -= nRamp;
						} else {
							pChannelInfo->nOutput = 0;
						}
					} else {
						if (pChannelInfo->nOutput < -nRamp) {
							pChannelInfo->nOutput += nRamp;
						} else {
							pChannelInfo->nOutput = 0;
						}
					}
					nOutput += pChannelInfo->nOutput / 16;
				}
			}
		}

		nOutput *= nVolume;

		//*pLeftBuf++ += nOutput;
		if ((MSM6295[nChip].nOutputDir & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) {
			*pLeftBuf++ += nOutput;
		}
		if ((MSM6295[nChip].nOutputDir & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) {
			*pRightBuf++ += nOutput;
		}

		MSM6295[nChip].nFractionalPosition = (MSM6295[nChip].nFractionalPosition & 0x0FFF) + MSM6295[nChip].nSampleSize;
	}
}

INT32 MSM6295Render(INT32 nChip, INT16* pSoundBuf, INT32 nSegmentLength)
{
#if defined FBA_DEBUG
	if (!DebugSnd_MSM6295Initted) bprintf(PRINT_ERROR, _T("MSM6295Render called without init\n"));
	if (nChip > nLastMSM6295Chip) bprintf(PRINT_ERROR, _T("MSM6295Render called with invalid chip number %x\n"), nChip);
#endif

	if (nChip == 0) {
		memset(pLeftBuffer, 0, nSegmentLength * sizeof(INT32));
		memset(pRightBuffer, 0, nSegmentLength * sizeof(INT32));
	}

	if (nInterpolation >= 3) {
		MSM6295Render_Cubic(nChip, pLeftBuffer, pRightBuffer, nSegmentLength);
	} else {
		MSM6295Render_Linear(nChip, pLeftBuffer, pRightBuffer, nSegmentLength);
	}

	if (nChip == nLastMSM6295Chip)	{
		for (INT32 i = 0; i < nSegmentLength; i++) {
			if (bAdd) {
				pSoundBuf[0] = BURN_SND_CLIP(pSoundBuf[0] + (pLeftBuffer[i] >> 8));
				pSoundBuf[1] = BURN_SND_CLIP(pSoundBuf[1] + (pRightBuffer[i] >> 8));
			} else {
				pSoundBuf[0] = BURN_SND_CLIP(pLeftBuffer[i] >> 8);
				pSoundBuf[1] = BURN_SND_CLIP(pRightBuffer[i] >> 8);
			}
			pSoundBuf += 2;
		}
	}

	return 0;
}

void MSM6295Command(INT32 nChip, UINT8 nCommand)
{
#if defined FBA_DEBUG
	if (!DebugSnd_MSM6295Initted) bprintf(PRINT_ERROR, _T("MSM6295Command called without init\n"));
	if (nChip > nLastMSM6295Chip) bprintf(PRINT_ERROR, _T("MSM6295Command called with invalid chip number %x\n"), nChip);
#endif

	if (MSM6295[nChip].bIsCommand) {
		// Process second half of command
		INT32 nChannel, nSampleStart, nSampleCount;
		INT32 nVolume = nCommand & 0x0F;
		nCommand >>= 4;

		MSM6295[nChip].bIsCommand = false;

		for (nChannel = 0; nChannel < 4; nChannel++) {
			if (nCommand & (0x01 << nChannel)) {
				if (MSM6295[nChip].ChannelInfo[nChannel].nPlaying == 0) {

					nSampleStart  = MSM6295ReadData(nChip, (MSM6295[nChip].nSampleInfo & 0x03ff) + 0) << 17;
					nSampleStart |= MSM6295ReadData(nChip, (MSM6295[nChip].nSampleInfo & 0x03ff) + 1) <<  9;
					nSampleStart |= MSM6295ReadData(nChip, (MSM6295[nChip].nSampleInfo & 0x03ff) + 2) <<  1;

					nSampleCount  = MSM6295ReadData(nChip, (MSM6295[nChip].nSampleInfo & 0x03ff) + 3) << 17;
					nSampleCount |= MSM6295ReadData(nChip, (MSM6295[nChip].nSampleInfo & 0x03ff) + 4) <<  9;
					nSampleCount |= MSM6295ReadData(nChip, (MSM6295[nChip].nSampleInfo & 0x03ff) + 5) <<  1;

					MSM6295[nChip].nSampleInfo &= 0xFF;

					nSampleCount -= nSampleStart;
					
					if (nSampleCount < 0x80000) {
						// Start playing channel
						MSM6295[nChip].ChannelInfo[nChannel].nVolume = MSM6295VolumeTable[nVolume];
						MSM6295[nChip].ChannelInfo[nChannel].nPosition = nSampleStart;
						MSM6295[nChip].ChannelInfo[nChannel].nSampleCount = nSampleCount;
						MSM6295[nChip].ChannelInfo[nChannel].nStep = 0;
						MSM6295[nChip].ChannelInfo[nChannel].nSample = -1;
						MSM6295[nChip].ChannelInfo[nChannel].nPlaying = 1;
						MSM6295[nChip].ChannelInfo[nChannel].nOutput = 0;

						nMSM6295Status[nChip] |= nCommand;

						if (nInterpolation >= 3) {
							MSM6295ChannelData[nChip][nChannel][0] = 0;
							MSM6295ChannelData[nChip][nChannel][1] = 0;
							MSM6295ChannelData[nChip][nChannel][2] = 0;
							MSM6295ChannelData[nChip][nChannel][3] = 0;
							MSM6295[nChip].ChannelInfo[nChannel].nBufPos = 4;
						}
					}
				}
			}
		}

	} else {
		// Process command
		if (nCommand & 0x80) {
			MSM6295[nChip].nSampleInfo = (nCommand & 0x7F) << 3;
			MSM6295[nChip].bIsCommand = true;
		} else {
			// Stop playing samples
			INT32 nChannel;
			nCommand >>= 3;
			nMSM6295Status[nChip] &= ~nCommand;

			for (nChannel = 0; nChannel < 4; nChannel++, nCommand>>=1) {
				if (nCommand & 1) {
					MSM6295[nChip].ChannelInfo[nChannel].nPlaying = 0;
				}
			}
		}
	}
}

void MSM6295Exit(INT32 nChip)
{
#if defined FBA_DEBUG
	if (!DebugSnd_MSM6295Initted) bprintf(PRINT_ERROR, _T("MSM6295Exit called without init\n"));
	if (nChip > nLastMSM6295Chip) bprintf(PRINT_ERROR, _T("MSM6295Exit called with invalid chip number %x\n"), nChip);
#endif

	if (!DebugSnd_MSM6295Initted) return;

	if (pLeftBuffer) BurnFree(pLeftBuffer);
	if (pRightBuffer) BurnFree(pRightBuffer);
	pLeftBuffer = NULL;
	pRightBuffer = NULL;

	for (INT32 nChannel = 0; nChannel < 4; nChannel++) {
		BurnFree(MSM6295ChannelData[nChip][nChannel]);
	}
	
	if (nChip == nLastMSM6295Chip) DebugSnd_MSM6295Initted = 0;
}

void MSM6295SetSamplerate(INT32 nChip, INT32 nSamplerate)
{
	MSM6295[nChip].nSampleRate = nSamplerate;
	if (nBurnSoundRate > 0) {
		MSM6295[nChip].nSampleSize = (nSamplerate << 12) / nBurnSoundRate;
	} else {
		MSM6295[nChip].nSampleSize = (nSamplerate << 12) / 11025;
	}
}

INT32 MSM6295Init(INT32 nChip, INT32 nSamplerate, bool bAddSignal)
{
	DebugSnd_MSM6295Initted = 1;
	
	if (nBurnSoundRate > 0) {
		if (pLeftBuffer == NULL) {
			pLeftBuffer = (INT32*)BurnMalloc(nBurnSoundRate * sizeof(INT32));
		}
		if (pRightBuffer == NULL) {
			pRightBuffer = (INT32*)BurnMalloc(nBurnSoundRate * sizeof(INT32));
		}
	}

	bAdd = bAddSignal;

	// Convert volume from percentage
	MSM6295[nChip].nVolume = INT32(100.0 * 256.0 / 100.0 + 0.5);

	MSM6295[nChip].nSampleRate = nSamplerate;
	if (nBurnSoundRate > 0) {
		MSM6295[nChip].nSampleSize = (nSamplerate << 12) / nBurnSoundRate;
	} else {
		MSM6295[nChip].nSampleSize = (nSamplerate << 12) / 11025;
	}

	MSM6295[nChip].nFractionalPosition = 0;

	nMSM6295Status[nChip] = 0;
	MSM6295[nChip].bIsCommand = false;

	if (nChip == 0) {
		nLastMSM6295Chip = 0;
	} else {
		if (nLastMSM6295Chip < nChip) {
			nLastMSM6295Chip = nChip;
		}
	}

	// Compute sample deltas
	for (INT32 i = 0; i < 49; i++) {
		INT32 nStep = (INT32)(pow(1.1, (double)i) * 16.0);
		for (INT32 n = 0; n < 16; n++) {
			INT32 nDelta = nStep >> 3;
			if (n & 1) {
				nDelta += nStep >> 2;
			}
			if (n & 2) {
				nDelta += nStep >> 1;
			}
			if (n & 4) {
				nDelta += nStep;
			}
			if (n & 8) {
				nDelta = -nDelta;
			}
			MSM6295DeltaTable[(i << 4) + n] = nDelta;
		}
	}

	// Compute volume levels
	for (INT32 i = 0; i < 16; i++) {
		double nVolume = 256.0;
		for (INT32 n = i; n > 0; n--) {
			nVolume /= 1.412537545;
		}
		MSM6295VolumeTable[i] = (UINT32)(nVolume + 0.5);
	}

	for (INT32 nChannel = 0; nChannel < 4; nChannel++) {
		MSM6295ChannelData[nChip][nChannel] = (INT32*)BurnMalloc(0x1000 * sizeof(INT32));
	}
	
	MSM6295[nChip].nOutputDir = BURN_SND_ROUTE_BOTH;

	memset (pBankPointer[nChip], 0, (0x40000/0x100) * sizeof(UINT8*));

	MSM6295Reset(nChip);

	return 0;
}

void MSM6295SetRoute(INT32 nChip, double nVolume, INT32 nRouteDir)
{
#if defined FBA_DEBUG
	if (!DebugSnd_MSM6295Initted) bprintf(PRINT_ERROR, _T("MSM6295SetRoute called without init\n"));
	if (nChip > nLastMSM6295Chip) bprintf(PRINT_ERROR, _T("MSM6295SetRoute called with invalid chip %i\n"), nChip);
#endif

	MSM6295[nChip].nVolume = INT32(nVolume * 256.0 + 0.5);
	MSM6295[nChip].nOutputDir = nRouteDir;
}
