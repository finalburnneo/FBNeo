/*
   FBAlpha port by iq_132, dink, Sept 2018

*****************************************************************************/

#include "burnint.h"
#include "redbaron.h"
#include <math.h>

#define OUTPUT_RATE 48000
#define FRAME_SIZE 800

static INT16  *m_mixer_buffer; // re-sampler

static INT32 (*pCPUTotalCycles)() = NULL;
static UINT32  nDACCPUMHZ = 0;
static INT32   nCurrentPosition = 0;

static void redbaron_update_int(INT16 *buffer, INT32 samples);

// Streambuffer handling
static INT32 SyncInternal()
{
	return (INT32)(float)(FRAME_SIZE * (pCPUTotalCycles() / (nDACCPUMHZ / (nBurnFPS / 100.0000))));
}

static void UpdateStream(INT32 length)
{
	if (length > FRAME_SIZE) length = FRAME_SIZE;

	length -= nCurrentPosition;
	if (length <= 0) return;

	INT16 *lbuf = m_mixer_buffer + nCurrentPosition;

	redbaron_update_int(lbuf, length);

	nCurrentPosition += length;
}

static INT32 m_latch;
static INT32 m_poly_counter;
static INT32 m_poly_shift;
static INT32 m_filter_counter;
static INT32 m_crash_amp;
static INT32 m_shot_amp;
static INT32 m_shot_amp_counter;
static INT32 m_squeal_amp;
static INT32 m_squeal_amp_counter;
static INT32 m_squeal_off_counter;
static INT32 m_squeal_on_counter;
static INT32 m_squeal_out;
static INT16 *m_vol_lookup;
static INT16 m_vol_crash[16];

void redbaron_sound_scan(INT32 nAction, INT32 *)
{
	SCAN_VAR(m_latch);
	SCAN_VAR(m_poly_counter);
	SCAN_VAR(m_poly_shift);
	SCAN_VAR(m_filter_counter);
	SCAN_VAR(m_crash_amp);
	SCAN_VAR(m_shot_amp);
	SCAN_VAR(m_shot_amp_counter);
	SCAN_VAR(m_squeal_amp);
	SCAN_VAR(m_squeal_amp_counter);
	SCAN_VAR(m_squeal_off_counter);
	SCAN_VAR(m_squeal_on_counter);
	SCAN_VAR(m_squeal_out);
}

void redbaron_sound_reset()
{
	m_latch = 0;
	m_poly_counter = 0;
	m_poly_shift = 0;
	m_filter_counter = 0;
	m_crash_amp = 0;
	m_shot_amp = 0;
	m_shot_amp_counter = 0;
	m_squeal_amp = 0;
	m_squeal_amp_counter = 0;
	m_squeal_off_counter = 0;
	m_squeal_on_counter = 0;
	m_squeal_out = 0;
}

void redbaron_sound_init(INT32 (*pCPUCyclesCB)(), INT32 nCpuMHZ)
{
	INT32 i;

	pCPUTotalCycles = pCPUCyclesCB;
	nDACCPUMHZ = nCpuMHZ;

	m_vol_lookup = (INT16*)BurnMalloc(0x8000 * 4);
	m_mixer_buffer = (INT16*)BurnMalloc(2 * sizeof(INT16) * OUTPUT_RATE);

	for( i = 0; i < 0x8000; i++ )
		m_vol_lookup[0x7fff-i] = (INT16) (0x7fff/exp(1.0*i/4096));

	for( i = 0; i < 16; i++ )
	{
		/* r0 = R18 and R24, r1 = open */
		double r0 = 1.0/(5600 + 680), r1 = 1/6e12;

		/* R14 */
		if( i & 1 )
			r1 += 1.0/8200;
		else
			r0 += 1.0/8200;
		/* R15 */
		if( i & 2 )
			r1 += 1.0/3900;
		else
			r0 += 1.0/3900;
		/* R16 */
		if( i & 4 )
			r1 += 1.0/2200;
		else
			r0 += 1.0/2200;
		/* R17 */
		if( i & 8 )
			r1 += 1.0/1000;
		else
			r0 += 1.0/1000;

		r0 = 1.0/r0;
		r1 = 1.0/r1;
		m_vol_crash[i] = (INT16)(32767 * r0 / (r0 + r1));
	}
}

void redbaron_sound_exit()
{
	BurnFree(m_mixer_buffer);
	BurnFree(m_vol_lookup);
}

void redbaron_update_int(INT16 *buffer, INT32 samples)
{
	while( samples-- )
	{
		INT32 sum = 0;

		/* polynomial shifter E5 and F4 (LS164) clocked with 12kHz */
		m_poly_counter -= 12000;
		while( m_poly_counter <= 0 )
		{
			m_poly_counter += OUTPUT_RATE;
			if( ((m_poly_shift & 0x0001) == 0) == ((m_poly_shift & 0x4000) == 0) )
				m_poly_shift = (m_poly_shift << 1) | 1;
			else
				m_poly_shift <<= 1;
		}

		/* What is the exact low pass filter frequency? */
		m_filter_counter -= 330;
		while( m_filter_counter <= 0 )
		{
			m_filter_counter += OUTPUT_RATE;
			m_crash_amp = (m_poly_shift & 1) ? m_latch >> 4 : 0;
		}
		/* mix crash sound at 35% */
		sum += BURN_SND_CLIP(m_vol_crash[m_crash_amp] * 35 / 100);

		/* shot not active: charge C32 (0.1u) */
		if( (m_latch & 0x04) == 0 )
			m_shot_amp = 32767;
		else
		if( (m_poly_shift & 0x8000) == 0 )
		{
			if( m_shot_amp > 0 )
			{
				/* I think this is too short. Is C32 really 1u? */
				#define C32_DISCHARGE_TIME (int)(32767 / 0.03264);
				m_shot_amp_counter -= C32_DISCHARGE_TIME;
				while( m_shot_amp_counter <= 0 )
				{
					m_shot_amp_counter += OUTPUT_RATE;
					if( --m_shot_amp == 0 )
						break;
				}
				/* mix shot sound at 35% */
				sum += m_vol_lookup[m_shot_amp] * 35 / 100;
			}
		}


		if( (m_latch & 0x02) == 0 )
			m_squeal_amp = 32767;
		else
		{
			if( m_squeal_amp >= 0 )
			{
				/* charge C5 (22u) over R3 (68k) and CR1 (1N914)
				 * time = 0.68 * C5 * R3 = 1017280us
				 */
				#define C5_CHARGE_TIME (int)(32767 / 1.01728);
				m_squeal_amp_counter -= C5_CHARGE_TIME;
				while( m_squeal_amp_counter <= 0 )
				{
					m_squeal_amp_counter += OUTPUT_RATE;
					if( --m_squeal_amp == 0 )
						break;
				}
			}

			if( m_squeal_out )
			{
				/* NE555 setup as pulse position modulator
				 * C = 0.01u, Ra = 33k, Rb = 47k
				 * frequency = 1.44 / ((33k + 2*47k) * 0.01u) = 1134Hz
				 * modulated by squeal_amp
				 */
				m_squeal_off_counter -= (((1134 + 1134) * 3) * m_squeal_amp / 32767) / 3;
				while( m_squeal_off_counter <= 0 )
				{
					m_squeal_off_counter += OUTPUT_RATE;
					m_squeal_out = 0;
				}
			}
			else
			{
				m_squeal_on_counter -= 1134*10;
				while( m_squeal_on_counter <= 0 )
				{
					m_squeal_on_counter += OUTPUT_RATE;
					m_squeal_out = 1;
				}
			}
		}

		/* mix sequal sound at 40% */
		if( m_squeal_out )
			sum += 32767 * 25 / 100;

		*buffer = BURN_SND_CLIP(sum);
		buffer++;
	}
}

void redbaron_sound_write(UINT8 data)
{
	if( data == m_latch )
		return;

	UpdateStream(SyncInternal());

	m_latch = data;
}

void redbaron_sound_update(INT16 *inputs, INT32 sample_len)
{
	if (sample_len != nBurnSoundLen) {
		bprintf(PRINT_ERROR, _T("*** redbaron_sound_update(): call once per frame!\n"));
		return;
	}

	INT32 samples_from = (INT32)((double)((OUTPUT_RATE * 100) / nBurnFPS) + 0.5);

	UpdateStream(samples_from);

	for (INT32 j = 0; j < sample_len; j++)
	{
		INT32 k = (samples_from * j) / nBurnSoundLen;

		INT32 rlmono = m_mixer_buffer[k];

		inputs[0] = BURN_SND_CLIP(inputs[0] + BURN_SND_CLIP(rlmono));
		inputs[1] = BURN_SND_CLIP(inputs[1] + BURN_SND_CLIP(rlmono));
		inputs += 2;
	}

	memset(m_mixer_buffer, 0, samples_from * sizeof(INT16));
	nCurrentPosition = 0;
}
