// Based on MAME driver by Nicola Salmoria

#include "burnint.h"
#include "sn76496.h"
#include <stddef.h>

#define MAX_SN76496_CHIPS 8

#define MAX_OUTPUT 0x7fff

#define STEP 0x10000

struct SN76496
{
	INT32 Register[8];	/* registers */
	INT32 LastRegister;	/* last register written */
	INT32 Volume[4];	/* volume of voice 0-2 and noise */
	INT32 RNG;		    /* noise generator      */
	INT32 NoiseMode;	/* active noise mode */
	INT32 Period[4];
	INT32 Count[4];
	INT32 Output[4];
	INT32 StereoMask;	/* the stereo output mask */

	// Init-time stuff
	INT32 VolTable[16];	/* volume table         */
	INT32 FeedbackMask;     /* mask for feedback */
	INT32 WhitenoiseTaps;   /* mask for white noise taps */
	INT32 WhitenoiseInvert; /* white noise invert flag */
	INT32 bSignalAdd;
	double nVolume;
	INT32 nOutputDir;
	UINT32 UpdateStep;
};

static INT32 NumChips = 0;
static struct SN76496 *Chips[MAX_SN76496_CHIPS] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };


// for stream-sync
static INT32 sn76496_buffered = 0;
static INT32 (*pCPUTotalCycles)() = NULL;
static UINT32 nDACCPUMHZ = 0;
static INT32 nPosition[MAX_SN76496_CHIPS];
static INT16 *soundbuf[MAX_SN76496_CHIPS];

// Streambuffer handling
static INT32 SyncInternal()
{
    if (!sn76496_buffered) return 0;
	return (INT32)(float)(nBurnSoundLen * (pCPUTotalCycles() / (nDACCPUMHZ / (nBurnFPS / 100.0000))));
}

static void UpdateStream(INT32 chip, INT32 samples_len)
{
    if (!sn76496_buffered || !pBurnSoundOut) return;
    if (samples_len > nBurnSoundLen) samples_len = nBurnSoundLen;

	INT32 nSamplesNeeded = samples_len;

    nSamplesNeeded -= nPosition[chip];
    if (nSamplesNeeded <= 0) return;

	INT16 *mix = soundbuf[chip] + 5 + (nPosition[chip] * 2); // * 2 (stereo stream)
	//memset(mix, 0, nSamplesNeeded * sizeof(INT16) * 2);
    //bprintf(0, _T("sn76496_sync: %d samples    frame %d\n"), nSamplesNeeded, nCurrentFrame);

    SN76496UpdateToBuffer(chip, mix, nSamplesNeeded);

    nPosition[chip] += nSamplesNeeded;
}

void SN76496SetBuffered(INT32 (*pCPUCyclesCB)(), INT32 nCpuMHZ)
{
    bprintf(0, _T("*** Using BUFFERED SN76496-mode.\n"));
    for (INT32 i = 0; i < NumChips; i++) {
        nPosition[i] = 0;
    }

    sn76496_buffered = 1;

    pCPUTotalCycles = pCPUCyclesCB;
    nDACCPUMHZ = nCpuMHZ;
}

void SN76496Update(INT32 Num, INT16* pSoundBuf, INT32 Length)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_SN76496Initted) bprintf(PRINT_ERROR, _T("SN76496Update called without init\n"));
	if (Num > NumChips) bprintf(PRINT_ERROR, _T("SN76496Update called with invalid chip %x\n"), Num);
#endif

	if (Num >= MAX_SN76496_CHIPS) return;

	struct SN76496 *R = Chips[Num];

	if (sn76496_buffered) {
		if (Length != nBurnSoundLen) {
			bprintf(0, _T("SN76496Update() in buffered mode must be called once per frame!\n"));
			return;
		}
	} else {
		nPosition[Num] = 0;
	}

	INT16 *mix = soundbuf[Num] + 5 + (nPosition[Num] * 2);

	SN76496UpdateToBuffer(Num, mix, Length - nPosition[Num]); // fill to end

	INT16 *pBuf = soundbuf[Num] + 5;

	while (Length > 0) {
		if (R->bSignalAdd) {
			pSoundBuf[0] = BURN_SND_CLIP(pSoundBuf[0] + pBuf[0]);
			pSoundBuf[1] = BURN_SND_CLIP(pSoundBuf[1] + pBuf[1]);
		} else {
			pSoundBuf[0] = BURN_SND_CLIP(pBuf[0]);
			pSoundBuf[1] = BURN_SND_CLIP(pBuf[1]);
		}

		pBuf += 2;
		pSoundBuf += 2;
		Length--;
	}

	nPosition[Num] = 0;
}

static INT16 dac_lastin_r  = 0;
static INT16 dac_lastout_r = 0;
static INT16 dac_lastin_l  = 0;
static INT16 dac_lastout_l = 0;

static INT16 dc_blockR(INT16 sam)
{
    INT16 outr = sam - dac_lastin_r + 0.998 * dac_lastout_r;
    dac_lastin_r = sam;
    dac_lastout_r = outr;

    return outr;
}

static INT16 dc_blockL(INT16 sam)
{
    INT16 outl = sam - dac_lastin_l + 0.998 * dac_lastout_l;
    dac_lastin_l = sam;
    dac_lastout_l = outl;

    return outl;
}

void SN76496Update(INT16* pSoundBuf, INT32 Length)
{
	for (INT32 i = 0; i < NumChips; i++) {
        SN76496Update(i, pSoundBuf, Length);
	}

	for (INT32 i = 0; i < Length*2; i += 2) {
		pSoundBuf[i + 0] = dc_blockR(pSoundBuf[i + 0]);
		pSoundBuf[i + 1] = dc_blockL(pSoundBuf[i + 1]);
	}
}

void SN76496UpdateToBuffer(INT32 Num, INT16* pSoundBuf, INT32 Length)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_SN76496Initted) bprintf(PRINT_ERROR, _T("SN76496Update called without init\n"));
	if (Num > NumChips) bprintf(PRINT_ERROR, _T("SN76496Update called with invalid chip %x\n"), Num);
#endif

	INT32 i;

	if (Num >= MAX_SN76496_CHIPS) return;

	struct SN76496 *R = Chips[Num];

	while (Length > 0)
	{
		INT32 Vol[4];
		UINT32 Out, Out2 = 0;
		INT32 Left;


		/* vol[] keeps track of how long each square wave stays */
		/* in the 1 position during the sample period. */
		Vol[0] = Vol[1] = Vol[2] = Vol[3] = 0;

		for (i = 0;i < 3;i++)
		{
			if (R->Output[i]) Vol[i] += R->Count[i];
			R->Count[i] -= STEP;
			/* Period[i] is the half period of the square wave. Here, in each */
			/* loop I add Period[i] twice, so that at the end of the loop the */
			/* square wave is in the same status (0 or 1) it was at the start. */
			/* vol[i] is also incremented by Period[i], since the wave has been 1 */
			/* exactly half of the time, regardless of the initial position. */
			/* If we exit the loop in the middle, Output[i] has to be inverted */
			/* and vol[i] incremented only if the exit status of the square */
			/* wave is 1. */
			while (R->Count[i] <= 0)
			{
				R->Count[i] += R->Period[i];
				if (R->Count[i] > 0)
				{
					R->Output[i] ^= 1;
					if (R->Output[i]) Vol[i] += R->Period[i];
					break;
				}
				R->Count[i] += R->Period[i];
				Vol[i] += R->Period[i];
			}
			if (R->Output[i]) Vol[i] -= R->Count[i];
		}

		Left = STEP;
		do
		{
			INT32 NextEvent;


			if (R->Count[3] < Left) NextEvent = R->Count[3];
			else NextEvent = Left;

			if (R->Output[3]) Vol[3] += R->Count[3];
			R->Count[3] -= NextEvent;
			if (R->Count[3] <= 0)
			{
		        if (R->NoiseMode == 1) /* White Noise Mode */
		        {
			        if (((R->RNG & R->WhitenoiseTaps) != R->WhitenoiseTaps) && ((R->RNG & R->WhitenoiseTaps) != 0)) /* crappy xor! */
					{
				        R->RNG >>= 1;
				        R->RNG |= R->FeedbackMask;
					}
					else
					{
				        R->RNG >>= 1;
					}
					R->Output[3] = R->WhitenoiseInvert ? !(R->RNG & 1) : R->RNG & 1;
				}
				else /* Periodic noise mode */
				{
			        if (R->RNG & 1)
					{
				        R->RNG >>= 1;
				        R->RNG |= R->FeedbackMask;
					}
					else
					{
				        R->RNG >>= 1;
					}
					R->Output[3] = R->RNG & 1;
				}
				R->Count[3] += R->Period[3];
				if (R->Output[3]) Vol[3] += R->Period[3];
			}
			if (R->Output[3]) Vol[3] -= R->Count[3];

			Left -= NextEvent;
		} while (Left > 0);

		if (R->StereoMask != 0xFF)
		{
			Out = ((R->StereoMask&0x10) ? Vol[0] * R->Volume[0]:0)
				+ ((R->StereoMask&0x20) ? Vol[1] * R->Volume[1]:0)
				+ ((R->StereoMask&0x40) ? Vol[2] * R->Volume[2]:0)
				+ ((R->StereoMask&0x80) ? Vol[3] * R->Volume[3]:0);

			Out2 = ((R->StereoMask&0x1) ? Vol[0] * R->Volume[0]:0)
				+ ((R->StereoMask&0x2) ? Vol[1] * R->Volume[1]:0)
				+ ((R->StereoMask&0x4) ? Vol[2] * R->Volume[2]:0)
				+ ((R->StereoMask&0x8) ? Vol[3] * R->Volume[3]:0);
			if (Out2 > MAX_OUTPUT * STEP) Out2 = MAX_OUTPUT * STEP;

			Out2 /= STEP;
		}
		else
		{
			Out = Vol[0] * R->Volume[0] + Vol[1] * R->Volume[1] +
				  Vol[2] * R->Volume[2] + Vol[3] * R->Volume[3];
		}

		if (Out > MAX_OUTPUT * STEP) Out = MAX_OUTPUT * STEP;

		Out /= STEP;

		INT32 nLeftSample = 0, nRightSample = 0;
		if ((R->nOutputDir & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) {
			nLeftSample += (INT32)(Out * R->nVolume);
		}
		if ((R->nOutputDir & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) {
			if (R->StereoMask != 0xFF)
				nRightSample += (INT32)(Out2 * R->nVolume);
			else
				nRightSample += (INT32)(Out * R->nVolume);
		}

		pSoundBuf[0] = BURN_SND_CLIP(nLeftSample);
		pSoundBuf[1] = BURN_SND_CLIP(nRightSample);

		pSoundBuf += 2;
		Length--;
	}
}

void SN76496StereoWrite(INT32 Num, INT32 Data)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_SN76496Initted) bprintf(PRINT_ERROR, _T("SN76496StereoWrite called without init\n"));
	if (Num > NumChips) bprintf(PRINT_ERROR, _T("SN76496StereoWrite called with invalid chip %x\n"), Num);
#endif

	if (Num >= MAX_SN76496_CHIPS) return;

	struct SN76496 *R = Chips[Num];

	R->StereoMask = Data;
}

void SN76496Write(INT32 Num, INT32 Data)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_SN76496Initted) bprintf(PRINT_ERROR, _T("SN76496Write called without init\n"));
	if (Num > NumChips) bprintf(PRINT_ERROR, _T("SN76496Write called with invalid chip %x\n"), Num);
#endif

	INT32 n, r, c;

	if (Num >= MAX_SN76496_CHIPS) return;

    if (sn76496_buffered) UpdateStream(Num, SyncInternal());

	struct SN76496 *R = Chips[Num];

	if (Data & 0x80) {
		r = (Data & 0x70) >> 4;
		R->LastRegister = r;
		R->Register[r] = (R->Register[r] & 0x3f0) | (Data & 0x0f);
	} else {
		r = R->LastRegister;
	}

	c = r / 2;

	switch (r)
	{
		case 0:	/* tone 0 : frequency */
		case 2:	/* tone 1 : frequency */
		case 4:	/* tone 2 : frequency */
		    if ((Data & 0x80) == 0) R->Register[r] = (R->Register[r] & 0x0f) | ((Data & 0x3f) << 4);
			R->Period[c] = R->UpdateStep * R->Register[r];
			if (R->Period[c] == 0) R->Period[c] = R->UpdateStep;
			if (r == 4)
			{
				/* update noise shift frequency */
				if ((R->Register[6] & 0x03) == 0x03)
					R->Period[3] = 2 * R->Period[2];
			}
			break;
		case 1:	/* tone 0 : volume */
		case 3:	/* tone 1 : volume */
		case 5:	/* tone 2 : volume */
		case 7:	/* noise  : volume */
			R->Volume[c] = R->VolTable[Data & 0x0f];
			if ((Data & 0x80) == 0) R->Register[r] = (R->Register[r] & 0x3f0) | (Data & 0x0f);
			break;
		case 6:	/* noise  : frequency, mode */
			{
			        if ((Data & 0x80) == 0) R->Register[r] = (R->Register[r] & 0x3f0) | (Data & 0x0f);
				n = R->Register[6];
				R->NoiseMode = (n & 4) ? 1 : 0;
				/* N/512,N/1024,N/2048,Tone #3 output */
				R->Period[3] = ((n&3) == 3) ? 2 * R->Period[2] : (R->UpdateStep << (5+(n&3)));
			        /* Reset noise shifter */
				R->RNG = R->FeedbackMask; /* this is correct according to the smspower document */
				//R->RNG = 0xF35; /* this is not, but sounds better in do run run */
				R->Output[3] = R->RNG & 1;
			}
			break;
	}
}

static void SN76496SetGain(struct SN76496 *R,INT32 Gain)
{
	INT32 i;
	double Out;

	Gain &= 0xff;

	/* increase max output basing on gain (0.2 dB per step) */
	Out = MAX_OUTPUT / 4;
	while (Gain-- > 0)
		Out *= 1.023292992;	/* = (10 ^ (0.2/20)) */

	/* build volume table (2dB per step) */
	for (i = 0;i < 15;i++)
	{
		/* limit volume to avoid clipping */
		if (Out > MAX_OUTPUT / 4) R->VolTable[i] = MAX_OUTPUT / 4;
		else R->VolTable[i] = (INT32)Out;

		Out /= 1.258925412;	/* = 10 ^ (2/20) = 2dB */
	}
	R->VolTable[15] = 0;
}

void SN76496Reset()
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_SN76496Initted) bprintf(PRINT_ERROR, _T("SN76496Reset called without init\n"));
#endif

	INT32 i;

	for (INT32 Num = 0; Num < NumChips; Num++) {
		struct SN76496 *R = Chips[Num];

		for (i = 0; i < 4; i++) R->Volume[i] = 0;

		R->LastRegister = 0;
		for (i = 0; i < 8; i += 2) {
			R->Register[i + 0] = 0x00;
			R->Register[i + 1] = 0x0f;
		}

		for (i = 0; i < 4; i++) {
			R->Output[i] = 0;
			R->Period[i] = R->Count[i] = R->UpdateStep;
		}

		R->FeedbackMask = 0x4000;
		R->WhitenoiseTaps = 0x03;
		R->WhitenoiseInvert = 1;
		R->StereoMask = 0xFF;

		R->RNG = R->FeedbackMask;
		R->Output[3] = R->RNG & 1;
	}

	// dc blocking stuff
	dac_lastin_r  = 0;
	dac_lastout_r = 0;
	dac_lastin_l  = 0;
	dac_lastout_l = 0;
}

static void SN76496Init(struct SN76496 *R, INT32 Clock)
{
	R->UpdateStep = (UINT32)(((double)STEP * nBurnSoundRate * 16) / Clock);

	SN76496Reset();
}

static void GenericStart(INT32 Num, INT32 Clock, INT32 FeedbackMask, INT32 NoiseTaps, INT32 NoiseInvert, INT32 SignalAdd)
{
	DebugSnd_SN76496Initted = 1;

	if (Num >= MAX_SN76496_CHIPS) return;

    if (sn76496_buffered) {
        bprintf(0, _T("*** ERROR: SN76496SetBuffered() must be called AFTER all chips have been initted!\n"));
    }

	NumChips = Num + 1;

	Chips[Num] = (struct SN76496*)BurnMalloc(sizeof(struct SN76496));
	memset(Chips[Num], 0, sizeof(struct SN76496));

	SN76496Init(Chips[Num], Clock);
	SN76496SetGain(Chips[Num], 0);

	soundbuf[Num] = (INT16*)BurnMalloc(0x1000);

	Chips[Num]->FeedbackMask = FeedbackMask;
	Chips[Num]->WhitenoiseTaps = NoiseTaps;
	Chips[Num]->WhitenoiseInvert = NoiseInvert;
	Chips[Num]->bSignalAdd = SignalAdd;
	Chips[Num]->nVolume = 1.00;
	Chips[Num]->nOutputDir = BURN_SND_ROUTE_BOTH;

	// dc blocking stuff
	dac_lastin_r  = 0;
	dac_lastout_r = 0;
	dac_lastin_l  = 0;
	dac_lastout_l = 0;
}

void SN76489Init(INT32 Num, INT32 Clock, INT32 SignalAdd)
{
	return GenericStart(Num, Clock, 0x4000, 0x03, 1, SignalAdd);
}

void SN76489AInit(INT32 Num, INT32 Clock, INT32 SignalAdd)
{
	return GenericStart(Num, Clock, 0x8000, 0x06, 0, SignalAdd);
}

void SN76494Init(INT32 Num, INT32 Clock, INT32 SignalAdd)
{
	return GenericStart(Num, Clock, 0x8000, 0x06, 0, SignalAdd);
}

void SN76496Init(INT32 Num, INT32 Clock, INT32 SignalAdd)
{
	return GenericStart(Num, Clock, 0x8000, 0x06, 0, SignalAdd);
}

void SN76496SetRoute(INT32 Num, double nVolume, INT32 nRouteDir)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_SN76496Initted) bprintf(PRINT_ERROR, _T("SN76496SetRoute called without init\n"));
	if (Num > NumChips) bprintf(PRINT_ERROR, _T("SN76496SetRoute called with invalid chip %i\n"), Num);
#endif

	if (Num >= MAX_SN76496_CHIPS) return;

	struct SN76496 *R = Chips[Num];

	R->nVolume = nVolume;
	R->nOutputDir = nRouteDir;
}

void SN76496Exit()
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_SN76496Initted) bprintf(PRINT_ERROR, _T("SN76496Exit called without init\n"));
#endif

	for (INT32 i = 0; i < NumChips; i++) {
		BurnFree(Chips[i]);
		BurnFree(soundbuf[i]);
		Chips[i] = NULL;

        if (sn76496_buffered) {
            nPosition[i] = 0;
        }
    }

	NumChips = 0;

    if (sn76496_buffered) {
        sn76496_buffered = 0;
        pCPUTotalCycles = NULL;
        nDACCPUMHZ = 0;
    }

	DebugSnd_SN76496Initted = 0;
}

void SN76496Scan(INT32 nAction, INT32 *pnMin)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_SN76496Initted) bprintf(PRINT_ERROR, _T("SN76496Scan called without init\n"));
#endif

	if (pnMin != NULL) {
		*pnMin = 0x029719;
	}

	if (nAction & ACB_DRIVER_DATA) {
		for (INT32 i = 0; i < NumChips; i++) {
			ScanVar(Chips[i], STRUCT_SIZE_HELPER(struct SN76496, StereoMask), "SN76496/SN76489 Chip");
		}
	}
}

#undef MAX_SN76496_CHIPS
#undef MAX_OUTPUT
#undef STEP
