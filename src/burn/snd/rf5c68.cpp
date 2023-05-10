// Based on MAME sources by Olivier Galibert,Aaron Giles
/*********************************************************/
/*    ricoh RF5C68(or clone) PCM controller              */
/*********************************************************/

#include "burnint.h"
#include "rf5c68.h"

#define NUM_CHANNELS	(8)

struct pcm_channel
{
	UINT8	enable;
	UINT8	env;
	UINT8	pan;
	UINT8	start;
	UINT32	addr;
	UINT16	step;
	UINT16	loopst;
};

struct rf5c68pcm
{
	struct pcm_channel	chan[NUM_CHANNELS];
	UINT8		cbank;
	UINT16		wbank;
	UINT8		enable;
	UINT8		data[0x10000];
	double		volume[2];
	INT32		output_dir[2];
};

static struct rf5c68pcm *chip = NULL;

// for resampling
static UINT64 nSampleSize;
static INT32 nFractionalPosition;
static INT32 nPosition;
static INT32 our_freq = 0;
static INT32 add_stream = 0;


// for stream-sync
static INT16 *soundbuf_l = NULL;
static INT16 *soundbuf_r = NULL;
static INT32 (*pCPUTotalCycles)() = NULL;
static UINT32  nDACCPUMHZ = 0;

// Streambuffer handling
static INT32 SyncInternal()
{
	return (INT32)(float)(nBurnSoundLen * (pCPUTotalCycles() / (nDACCPUMHZ / (nBurnFPS / 100.0000))));
}

void RF5C68PCMUpdate_internal(INT16* left, INT16 *right, INT32 length);

static void UpdateStream(INT32 samples_len)
{
	if (!pBurnSoundOut) return;
    if (samples_len > nBurnSoundLen) samples_len = nBurnSoundLen;

	INT64 nSamplesNeeded = ((((((our_freq * 1000) / nBurnFPS) * samples_len) / nBurnSoundLen)) / 10) + 1;
	if (nBurnSoundRate < 44100) nSamplesNeeded += 2; // so we don't end up with negative nPosition below

    nSamplesNeeded -= nPosition;
    if (nSamplesNeeded <= 0) return;

	INT16 *mixl = soundbuf_l + 5 + nPosition;
	INT16 *mixr = soundbuf_r + 5 + nPosition;
	//bprintf(0, _T("frame %d\tupdate @ %x\n"), nCurrentFrame, nPosition);
    RF5C68PCMUpdate_internal(mixl, mixr, nSamplesNeeded);
    nPosition += nSamplesNeeded;
}


void RF5C68PCMUpdate_internal(INT16* left, INT16 *right, INT32 length)
{
	if (!chip->enable) return;

	INT32 i, j;

	memset(left, 0, length * sizeof(INT16));
	memset(right, 0, length * sizeof(INT16));

	for (i = 0; i < NUM_CHANNELS; i++) {
		pcm_channel *chan = &chip->chan[i];

		if (chan->enable) {
			INT32 lv = (chan->pan & 0xf) * chan->env;
			INT32 rv = ((chan->pan >> 4) & 0xf) * chan->env;

			for (j = 0; j < length; j++) {
				UINT8 sample;

				sample = chip->data[(chan->addr >> 11) & 0xffff];
				if (sample == 0xff) {
					chan->addr = chan->loopst << 11;
					sample = chip->data[(chan->addr >> 11) & 0xffff];
					if (sample == 0xff) {
						break;
					}
				}

				chan->addr += chan->step;

				if (sample & 0x80) {
					sample &= 0x7f;
					left[j] = BURN_SND_CLIP((left[j] + ((sample * lv) >> 5)));
					right[j] = BURN_SND_CLIP((right[j] + ((sample * rv) >> 5)));
				} else {
					left[j] = BURN_SND_CLIP((left[j] - ((sample * lv) >> 5)));
					right[j] = BURN_SND_CLIP((right[j] - ((sample * rv) >> 5)));
				}
			}
		}
	}
}

void RF5C68PCMUpdate(INT16 *buffer, INT32 samples_len)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_RF5C68Initted) bprintf(PRINT_ERROR, _T("RF5C68PCMUpdate called without init\n"));
#endif
	INT64 nSamplesNeeded = ((((((our_freq * 1000) / nBurnFPS) * samples_len) / nBurnSoundLen)) / 10) + 1;
	if (nBurnSoundRate < 44100) nSamplesNeeded += 2; // so we don't end up with negative nPosition below

	UpdateStream(nBurnSoundLen);

	INT16 *pBufL = soundbuf_l + 5;
	INT16 *pBufR = soundbuf_r + 5;

	//-4 -3 -2 -1  0
	//+1 +2 +3 +4 +5
	for (INT32 i = (nFractionalPosition & 0xFFFF0000) >> 15; i < (samples_len << 1); i += 2, nFractionalPosition += nSampleSize) {
		INT32 nLeftSample[4] = {0, 0, 0, 0};
		INT32 nRightSample[4] = {0, 0, 0, 0};
		INT32 nTotalLeftSample;
		INT32 nTotalRightSample;

		nLeftSample[0] += (INT32)(pBufL[(nFractionalPosition >> 16) - 3]);
		nLeftSample[1] += (INT32)(pBufL[(nFractionalPosition >> 16) - 2]);
		nLeftSample[2] += (INT32)(pBufL[(nFractionalPosition >> 16) - 1]);
		nLeftSample[3] += (INT32)(pBufL[(nFractionalPosition >> 16) - 0]);

		nRightSample[0] += (INT32)(pBufR[(nFractionalPosition >> 16) - 3]);
		nRightSample[1] += (INT32)(pBufR[(nFractionalPosition >> 16) - 2]);
		nRightSample[2] += (INT32)(pBufR[(nFractionalPosition >> 16) - 1]);
		nRightSample[3] += (INT32)(pBufR[(nFractionalPosition >> 16) - 0]);

		nTotalLeftSample  = INTERPOLATE4PS_16BIT((nFractionalPosition >> 4) & 0x0fff, nLeftSample[0], nLeftSample[1], nLeftSample[2], nLeftSample[3]);
		nTotalLeftSample  = BURN_SND_CLIP(nTotalLeftSample * chip->volume[BURN_SND_RF5C68PCM_ROUTE_1]);

		nTotalRightSample  = INTERPOLATE4PS_16BIT((nFractionalPosition >> 4) & 0x0fff, nRightSample[0], nRightSample[1], nRightSample[2], nRightSample[3]);
		nTotalRightSample  = BURN_SND_CLIP(nTotalRightSample * chip->volume[BURN_SND_RF5C68PCM_ROUTE_2]);

		if (!add_stream) buffer[i + 0] = buffer[i + 1] = 0;

		buffer[i + 0] = BURN_SND_CLIP(buffer[i + 0] + nTotalLeftSample);
		buffer[i + 1] = BURN_SND_CLIP(buffer[i + 1] + nTotalRightSample);
	}

	if (samples_len >= nBurnSoundLen) {
		INT32 nExtraSamples = nSamplesNeeded - (nFractionalPosition >> 16);

		for (INT32 i = -4; i < nExtraSamples; i++) {
			pBufL[i] = pBufL[(nFractionalPosition >> 16) + i];
			pBufR[i] = pBufR[(nFractionalPosition >> 16) + i];
		}

		nFractionalPosition &= 0xFFFF;

		nPosition = nExtraSamples;
	}
}

void RF5C68PCMReset()
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_RF5C68Initted) bprintf(PRINT_ERROR, _T("RF5C68PCMReset called without init\n"));
#endif

	memset(chip->data, 0xff, sizeof(chip->data));
}

void RF5C68PCMInit(INT32 clock, INT32 (*pCPUCyclesCB)(), INT32 nCpuMHZ, INT32 nAdd)
{
	chip = (struct rf5c68pcm*)BurnMalloc(sizeof(struct rf5c68pcm));

	our_freq = clock / 384;

	pCPUTotalCycles = pCPUCyclesCB;
	nDACCPUMHZ = nCpuMHZ;
	add_stream = nAdd;

	soundbuf_l = (INT16*)BurnMalloc(our_freq * sizeof(INT16));
	soundbuf_r = (INT16*)BurnMalloc(our_freq * sizeof(INT16));

	if (nBurnSoundRate) nSampleSize = (UINT64)((UINT64)our_freq * (1 << 16)) / nBurnSoundRate;
	nPosition = 0;
	nFractionalPosition = 0;

	chip->volume[BURN_SND_RF5C68PCM_ROUTE_1] = 1.00;
	chip->volume[BURN_SND_RF5C68PCM_ROUTE_2] = 1.00;
	chip->output_dir[BURN_SND_RF5C68PCM_ROUTE_1] = BURN_SND_ROUTE_LEFT;
	chip->output_dir[BURN_SND_RF5C68PCM_ROUTE_2] = BURN_SND_ROUTE_RIGHT;

	DebugSnd_RF5C68Initted = 1;
}

void RF5C68PCMSetRoute(INT32 nIndex, double nVolume, INT32 nRouteDir)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_RF5C68Initted) bprintf(PRINT_ERROR, _T("RF5C68PCMSetRoute called without init\n"));
	if (nIndex < 0 || nIndex > 1) bprintf(PRINT_ERROR, _T("RF5C68PCMSetRoute called with invalid index %i\n"), nIndex);
#endif

	chip->volume[nIndex] = nVolume;
	chip->output_dir[nIndex] = nRouteDir;
}

void RF5C68PCMExit()
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_RF5C68Initted) bprintf(PRINT_ERROR, _T("RF5C68PCMExit called without init\n"));
#endif

	BurnFree(soundbuf_l);
	BurnFree(soundbuf_r);
	BurnFree(chip);

	DebugSnd_RF5C68Initted = 0;
}

void RF5C68PCMScan(INT32 nAction, INT32 *)
{
	struct BurnArea ba;

	if (nAction & ACB_DRIVER_DATA) {
		memset(&ba, 0, sizeof(ba));
		ba.Data = chip->data;
		ba.nLen = 0x10000;
		ba.szName = "RF5C68PCMData";
		BurnAcb(&ba);

		SCAN_VAR(chip->cbank);
		SCAN_VAR(chip->wbank);
		SCAN_VAR(chip->enable);
		SCAN_VAR(chip->chan);
	}
}

UINT8 RF5C68PCMRegRead(UINT8 offset)
{
	UINT8 shift;

	UpdateStream(SyncInternal());
	shift = (offset & 1) ? 11 + 8 : 11;

	return (chip->chan[(offset & 0x0e) >> 1].addr) >> (shift);
}

void RF5C68PCMRegWrite(UINT8 offset, UINT8 data)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_RF5C68Initted) bprintf(PRINT_ERROR, _T("RF5C68PCMReqWrite called without init\n"));
#endif

	struct pcm_channel *chan = &chip->chan[chip->cbank];
	INT32 i;

	/* force the stream to update first */
	UpdateStream(SyncInternal());

	switch (offset) {
		case 0x00: {
			chan->env = data;
			break;
		}

		case 0x01: {
			chan->pan = data;
			break;
		}

		case 0x02: {
			chan->step = (chan->step & 0xff00) | (data & 0xff);
			break;
		}

		case 0x03: {
			chan->step = (chan->step & 0xff) | ((data << 8) & 0xff00);
			break;
		}

		case 0x04: {
			chan->loopst = (chan->loopst & 0xff00) | (data & 0xff);
			break;
		}

		case 0x05: {
			chan->loopst = (chan->loopst & 0xff) | ((data << 8) & 0xff00);
			break;
		}

		case 0x06: {
			chan->start = data;
			if (!chan->enable)
				chan->addr = chan->start << (8 + 11);
			break;
		}

		case 0x07: {
			chip->enable = (data >> 7) & 1;
			if (data & 0x40) {
				chip->cbank = data & 7;
			} else {
				chip->wbank = (data & 15) << 12;
			}
			break;
		}

		case 0x08: {
			for (i = 0; i < 8; i++) {
				chip->chan[i].enable = (~data >> i) & 1;
				if (!chip->chan[i].enable)
					chip->chan[i].addr = chip->chan[i].start << (8 + 11);
			}
			break;
		}
	}
}

UINT8 RF5C68PCMRead(UINT16 offset)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_RF5C68Initted) bprintf(PRINT_ERROR, _T("RF5C68PCMRead called without init\n"));
#endif

	return chip->data[chip->wbank | offset];
}

void RF5C68PCMWrite(UINT16 offset, UINT8 data)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_RF5C68Initted) bprintf(PRINT_ERROR, _T("RF5C68PCMWrite called without init\n"));
#endif

	chip->data[chip->wbank | offset] = data;
}

#undef NUM_CHANNELS
