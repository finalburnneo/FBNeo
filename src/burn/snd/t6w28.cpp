/***************************************************************************

  t6w28.c (based on sn74696.c)

  The t6w28 sound core is used in the SNK NeoGeo Pocket. It is a stereo
  sound chip based on 2 partial sn76489a cores.

  The block diagram for this chip is as follows:

Offset 0:
        Tone 0          /---------->   Att0  ---\
                        |                       |
        Tone 1          |  /------->   Att1  ---+
                        |  |                    |    Right
        Tone 2          |  |  /---->   Att2  ---+-------->
         |              |  |  |                 |
        Noise   -----+------------->   Att3  ---/
                     |  |  |  |
                     |  |  |  |
 Offset 1:           |  |  |  |
        Tone 0  --------+---------->   Att0  ---\
                     |     |  |                 |
        Tone 1  -----------+------->   Att1  ---+
                     |        |                 |     Left
        Tone 2  --------------+---->   Att2  ---+-------->
                     |                          |
        Noise        \------------->   Att3  ---/


***************************************************************************/

#include "burnint.h"
#include "t6w28.h"

static int   m_sample_rate;
static int   m_vol_table[16];    /* volume table         */
static INT32 m_register[16];   /* registers */
static INT32 m_last_register[2];   /* last register written */
static INT32 m_volume[8];  /* volume of voice 0-2 and noise */
static UINT32 m_rng[2];        /* noise generator      */
static INT32 m_noise_mode[2];  /* active noise mode */
static INT32 m_feedback_mask;     /* mask for feedback */
static INT32 m_whitenoise_taps;   /* mask for white noise taps */
static INT32 m_whitenoise_invert; /* white noise invert flag */
static INT32 m_period[8];
static INT32 m_count[8];
static INT32 m_output[8];
static bool  m_enabled;

// for resampling
static UINT64 nSampleSize;
static INT32 nFractionalPosition;
static INT32 nPosition;
static INT32 our_freq = 0;

static double our_vol;
static INT32 add_stream;

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

static void t6w28_update(INT16 *outputsl, INT16 *outputsr, int samples);

static void UpdateStream(INT32 samples_len)
{
    if (samples_len > nBurnSoundLen) samples_len = nBurnSoundLen;

	INT64 nSamplesNeeded = ((((((our_freq * 1000) / nBurnFPS) * samples_len) / nBurnSoundLen)) / 10) + 1;
	if (nBurnSoundRate < 44100) nSamplesNeeded += 2; // so we don't end up with negative nPosition below

    nSamplesNeeded -= nPosition;
    if (nSamplesNeeded <= 0) return;

	INT16 *mixl = soundbuf_l + 5 + nPosition;
	INT16 *mixr = soundbuf_r + 5 + nPosition;
    t6w28_update(mixl, mixr, nSamplesNeeded);
    nPosition += nSamplesNeeded;
}


#define MAX_OUTPUT 0x7fff

#define STEP 0x10000

void t6w28Write(INT32 offset, UINT8 data)
{
	int n, r, c;


	/* update the output buffer before changing the registers */
	UpdateStream(SyncInternal());

	offset &= 1;

	if (data & 0x80)
	{
		r = (data & 0x70) >> 4;
		m_last_register[offset] = r;
		m_register[offset * 8 + r] = (m_register[offset * 8 + r] & 0x3f0) | (data & 0x0f);
	}
	else
	{
		r = m_last_register[offset];
	}
	c = r/2;
	switch (r)
	{
		case 0: /* tone 0 : frequency */
		case 2: /* tone 1 : frequency */
		case 4: /* tone 2 : frequency */
			if ((data & 0x80) == 0) m_register[offset * 8 + r] = (m_register[offset * 8 + r] & 0x0f) | ((data & 0x3f) << 4);
			m_period[offset * 4 + c] = STEP * m_register[offset * 8 + r];
			if (m_period[offset * 4 + c] == 0) m_period[offset * 4 + c] = STEP;
			if (r == 4)
			{
				/* update noise shift frequency */
				if ((m_register[offset * 8 + 6] & 0x03) == 0x03)
					m_period[offset * 4 + 3] = 2 * m_period[offset * 4 + 2];
			}
			break;
		case 1: /* tone 0 : volume */
		case 3: /* tone 1 : volume */
		case 5: /* tone 2 : volume */
		case 7: /* noise  : volume */
			m_volume[offset * 4 + c] = m_vol_table[data & 0x0f];
			if ((data & 0x80) == 0) m_register[offset * 8 + r] = (m_register[offset * 8 + r] & 0x3f0) | (data & 0x0f);
			break;
		case 6: /* noise  : frequency, mode */
			{
				if ((data & 0x80) == 0) m_register[offset * 8 + r] = (m_register[offset * 8 + r] & 0x3f0) | (data & 0x0f);
				n = m_register[offset * 8 + 6];
				m_noise_mode[offset] = (n & 4) ? 1 : 0;
				/* N/512,N/1024,N/2048,Tone #3 output */
				m_period[offset * 4 + 3] = ((n&3) == 3) ? 2 * m_period[offset * 4 + 2] : (STEP << (5+(n&3)));
					/* Reset noise shifter */
				m_rng[offset] = m_feedback_mask; /* this is correct according to the smspower document */
				//m_rng = 0xF35; /* this is not, but sounds better in do run run */
				m_output[offset * 4 + 3] = m_rng[offset] & 1;
			}
			break;
	}
}



//-------------------------------------------------
//  sound_stream_update - handle a stream update
//-------------------------------------------------

static void t6w28_update(INT16 *outputsl, INT16 *outputsr, int samples)
{
	int i;

	/* If the volume is 0, increase the counter */
	for (i = 0;i < 8;i++)
	{
		if (m_volume[i] == 0)
		{
			/* note that I do count += samples, NOT count = samples + 1. You might think */
			/* it's the same since the volume is 0, but doing the latter could cause */
			/* interferencies when the program is rapidly modulating the volume. */
			if (m_count[i] <= samples*STEP) m_count[i] += samples*STEP;
		}
	}

	while (samples > 0)
	{
		int vol[8];
		unsigned int out0, out1;
		int left;


		/* vol[] keeps track of how long each square wave stays */
		/* in the 1 position during the sample period. */
		vol[0] = vol[1] = vol[2] = vol[3] = vol[4] = vol[5] = vol[6] = vol[7] = 0;

		for (i = 2;i < 3;i++)
		{
			if (m_output[i]) vol[i] += m_count[i];
			m_count[i] -= STEP;
			/* m_period[i] is the half period of the square wave. Here, in each */
			/* loop I add m_period[i] twice, so that at the end of the loop the */
			/* square wave is in the same status (0 or 1) it was at the start. */
			/* vol[i] is also incremented by m_period[i], since the wave has been 1 */
			/* exactly half of the time, regardless of the initial position. */
			/* If we exit the loop in the middle, m_output[i] has to be inverted */
			/* and vol[i] incremented only if the exit status of the square */
			/* wave is 1. */
			while (m_count[i] <= 0)
			{
				m_count[i] += m_period[i];
				if (m_count[i] > 0)
				{
					m_output[i] ^= 1;
					if (m_output[i]) vol[i] += m_period[i];
					break;
				}
				m_count[i] += m_period[i];
				vol[i] += m_period[i];
			}
			if (m_output[i]) vol[i] -= m_count[i];
		}

		for (i = 4;i < 7;i++)
		{
			if (m_output[i]) vol[i] += m_count[i];
			m_count[i] -= STEP;
			/* m_period[i] is the half period of the square wave. Here, in each */
			/* loop I add m_period[i] twice, so that at the end of the loop the */
			/* square wave is in the same status (0 or 1) it was at the start. */
			/* vol[i] is also incremented by m_period[i], since the wave has been 1 */
			/* exactly half of the time, regardless of the initial position. */
			/* If we exit the loop in the middle, m_output[i] has to be inverted */
			/* and vol[i] incremented only if the exit status of the square */
			/* wave is 1. */
			while (m_count[i] <= 0)
			{
				m_count[i] += m_period[i];
				if (m_count[i] > 0)
				{
					m_output[i] ^= 1;
					if (m_output[i]) vol[i] += m_period[i];
					break;
				}
				m_count[i] += m_period[i];
				vol[i] += m_period[i];
			}
			if (m_output[i]) vol[i] -= m_count[i];
		}

		left = STEP;
		do
		{
			int nextevent;


			if (m_count[3] < left) nextevent = m_count[3];
			else nextevent = left;

			if (m_output[3]) vol[3] += m_count[3];
			m_count[3] -= nextevent;
			if (m_count[3] <= 0)
			{
				if (m_noise_mode[0] == 1) /* White Noise Mode */
				{
					if (((m_rng[0] & m_whitenoise_taps) != m_whitenoise_taps) && ((m_rng[0] & m_whitenoise_taps) != 0)) /* crappy xor! */
					{
						m_rng[0] >>= 1;
						m_rng[0] |= m_feedback_mask;
					}
					else
					{
						m_rng[0] >>= 1;
					}
					m_output[3] = m_whitenoise_invert ? !(m_rng[0] & 1) : m_rng[0] & 1;
				}
				else /* Periodic noise mode */
				{
					if (m_rng[0] & 1)
					{
						m_rng[0] >>= 1;
						m_rng[0] |= m_feedback_mask;
					}
					else
					{
						m_rng[0] >>= 1;
					}
					m_output[3] = m_rng[0] & 1;
				}
				m_count[3] += m_period[3];
				if (m_output[3]) vol[3] += m_period[3];
			}
			if (m_output[3]) vol[3] -= m_count[3];

			left -= nextevent;
		} while (left > 0);

		if (m_enabled)
		{
			out0 = vol[4] * m_volume[4] + vol[5] * m_volume[5] +
					vol[6] * m_volume[6] + vol[3] * m_volume[7];

			out1 = vol[4] * m_volume[0] + vol[5] * m_volume[1] +
					vol[6] * m_volume[2] + vol[3] * m_volume[3];
		}
		else
		{
			out0 = 0;
			out1 = 0;
		}

		if (out0 > MAX_OUTPUT * STEP) out0 = MAX_OUTPUT * STEP;
		if (out1 > MAX_OUTPUT * STEP) out1 = MAX_OUTPUT * STEP;

		*(outputsl++) = out0 / STEP;
		*(outputsr++) = out1 / STEP;

		samples--;
	}
}

void t6w28Update(INT16 *buffer, INT32 samples_len)
{
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
		nTotalLeftSample  = BURN_SND_CLIP(nTotalLeftSample * our_vol);

		nTotalRightSample  = INTERPOLATE4PS_16BIT((nFractionalPosition >> 4) & 0x0fff, nRightSample[0], nRightSample[1], nRightSample[2], nRightSample[3]);
		nTotalRightSample  = BURN_SND_CLIP(nTotalRightSample * our_vol);

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


static void set_gain(int gain)
{
	int i;
	double out;

	gain &= 0xff;

	/* increase max output basing on gain (0.2 dB per step) */
	out = MAX_OUTPUT / 3;
	while (gain-- > 0)
		out *= 1.023292992; /* = (10 ^ (0.2/20)) */

	/* build volume table (2dB per step) */
	for (i = 0;i < 15;i++)
	{
		/* limit volume to avoid clipping */
		if (out > MAX_OUTPUT / 3) m_vol_table[i] = MAX_OUTPUT / 3;
		else m_vol_table[i] = out;

		out /= 1.258925412; /* = 10 ^ (2/20) = 2dB */
	}
	m_vol_table[15] = 0;
}


void t6w28Reset()
{
	for (int i = 0; i < 8; i++) m_volume[i] = 0;

	m_last_register[0] = 0;
	m_last_register[1] = 0;

	for (int i = 0; i < 8; i+=2)
	{
		m_register[i] = 0;
		m_register[i + 1] = 0x0f;   /* volume = 0 */
	}

	for (int i = 0; i < 8; i++)
	{
		m_output[i] = 0;
		m_period[i] = m_count[i] = STEP;
	}

	/* Default is SN76489 non-A */
	m_feedback_mask = 0x4000;     /* mask for feedback */
	m_whitenoise_taps = 0x03;   /* mask for white noise taps */
	m_whitenoise_invert = 1; /* white noise invert flag */

	m_rng[0] = m_feedback_mask;
	m_rng[1] = m_feedback_mask;
	m_output[3] = m_rng[0] & 1;

	set_gain(0);

	/* values from sn76489a */
	m_feedback_mask = 0x8000;
	m_whitenoise_taps = 0x06;
	m_whitenoise_invert = 0;
	m_enabled = 0;
}


//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void t6w28Init(INT32 clock, INT32 (*pCPUCyclesCB)(), INT32 nCpuMHZ, INT32 nAdd)
{
	m_sample_rate = clock / 16;

	t6w28Reset();

	our_freq = m_sample_rate;
	our_vol = 1.00;
	add_stream = nAdd;

	pCPUTotalCycles = pCPUCyclesCB;
	nDACCPUMHZ = nCpuMHZ;

	soundbuf_l = (INT16*)BurnMalloc(our_freq * 2);
	soundbuf_r = (INT16*)BurnMalloc(our_freq * 2);

	nSampleSize = (UINT64)((UINT64)our_freq * (1 << 16)) / nBurnSoundRate;
	nPosition = 0;
	nFractionalPosition = 0;
}

void t6w28Exit()
{
	BurnFree(soundbuf_l);
	BurnFree(soundbuf_r);
}

void t6w28SetVolume(double vol)
{
	our_vol = vol;
}

void t6w28Enable(bool enable)
{
	m_enabled = enable;
}

void t6w28Scan(INT32 nAction, INT32 *pnMin)
{
	SCAN_VAR(m_register);
	SCAN_VAR(m_last_register);
	SCAN_VAR(m_volume);
	SCAN_VAR(m_rng);
	SCAN_VAR(m_noise_mode);
	SCAN_VAR(m_period);
	SCAN_VAR(m_count);
	SCAN_VAR(m_output);
	SCAN_VAR(m_enabled);
}

