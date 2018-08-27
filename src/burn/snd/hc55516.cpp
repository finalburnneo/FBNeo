// license:BSD-3-Clause
// copyright-holders:Aaron Giles
/*****************************************************************************

    Harris HC-55516 (and related) emulator

   FBAlpha port by dink, July 2018

*****************************************************************************/

#include "burnint.h"
#include "hc55516.h"
#include <math.h>

#define SAMPLE_RATE             (48000)
#define FRAME_SIZE              800

#define INTEGRATOR_LEAK_TC      0.001
#define FILTER_DECAY_TC         0.004
#define FILTER_CHARGE_TC        0.004
#define FILTER_MIN              0.0416
#define FILTER_MAX              1.0954
#define SAMPLE_GAIN             10000.0

static INT32   m_active_clock_hi;
static UINT8   m_shiftreg_mask;

static UINT8   m_last_clock_state;
static UINT8   m_digit;
static UINT8   m_new_digit;
static UINT8   m_shiftreg;

static INT16   m_curr_sample;
static INT16   m_next_sample;

static UINT32  m_update_count;

static double  m_filter;
static double  m_integrator;

static double  m_charge;
static double  m_decay;
static double  m_leak;

static INT32   m_clock = 0; // always 0 for sw-driven clock

static INT16  *m_mixer_buffer; // re-sampler

static INT32 (*pCPUTotalCycles)() = NULL;
static UINT32  nDACCPUMHZ = 0;
static INT32   nCurrentPosition = 0;

static void    hc55516_update_int(INT16 *inputs, INT32 sample_len);


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

	hc55516_update_int(lbuf, length);

	nCurrentPosition += length;
}

static void start_common(UINT8 _shiftreg_mask, INT32 _active_clock_hi);

//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void hc55516_init(INT32 (*pCPUCyclesCB)(), INT32 nCpuMHZ)
{
	pCPUTotalCycles = pCPUCyclesCB;
	nDACCPUMHZ = nCpuMHZ;

	start_common(0x07, true);
}

#if 0
// maybe we'll need these in the future, who knows?
void mc3417_init(INT32 (*pCPUCyclesCB)(), INT32 nCpuMHZ)
{
	pCPUTotalCycles = pCPUCyclesCB;
	nDACCPUMHZ = nCpuMHZ;

	start_common(0x07, false);
}

void mc3418_init(INT32 (*pCPUCyclesCB)(), INT32 nCpuMHZ)
{
	pCPUTotalCycles = pCPUCyclesCB;
	nDACCPUMHZ = nCpuMHZ;

	start_common(0x0f, false);
}
#endif

void hc55516_exit()
{
	BurnFree(m_mixer_buffer);
}

//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void hc55516_reset()
{
	m_last_clock_state = 0;
	m_digit = 0;
	m_new_digit = 0;
	m_shiftreg = 0;

	m_curr_sample = 0;
	m_next_sample = 0;

	m_update_count = 0;

	m_filter = 0;
	m_integrator = 0;

	nCurrentPosition = 0;
}

void hc55516_scan(INT32 nAction, INT32 *)
{
	SCAN_VAR(m_last_clock_state);
	SCAN_VAR(m_digit);
	SCAN_VAR(m_new_digit);
	SCAN_VAR(m_shiftreg);

	SCAN_VAR(m_curr_sample);
	SCAN_VAR(m_next_sample);

	SCAN_VAR(m_update_count);

	SCAN_VAR(m_filter);
	SCAN_VAR(m_integrator);
}

//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

static void start_common(UINT8 _shiftreg_mask, INT32 _active_clock_hi)
{
	/* compute the fixed charge, decay, and leak time constants */
	m_charge = pow(exp(-1.0), 1.0 / (FILTER_CHARGE_TC * 16000.0));
	m_decay = pow(exp(-1.0), 1.0 / (FILTER_DECAY_TC * 16000.0));
	m_leak = pow(exp(-1.0), 1.0 / (INTEGRATOR_LEAK_TC * 16000.0));

	m_shiftreg_mask = _shiftreg_mask;
	m_active_clock_hi = _active_clock_hi;

	m_mixer_buffer = (INT16*)BurnMalloc(2 * sizeof(INT16) * SAMPLE_RATE);
}

static inline INT32 is_external_oscillator()
{
	return m_clock != 0;
}


static inline INT32 is_active_clock_transition(INT32 clock_state)
{
	return (( m_active_clock_hi && !m_last_clock_state &&  clock_state) ||
			(!m_active_clock_hi &&  m_last_clock_state && !clock_state));
}


static inline INT32 current_clock_state()
{
	return ((UINT64)m_update_count * m_clock * 2 / SAMPLE_RATE) & 0x01;
}


static void process_digit()
{
	double integrator = m_integrator, temp;

	/* shift the bit into the shift register */
	m_shiftreg = (m_shiftreg << 1) | m_digit;

	/* move the estimator up or down a step based on the bit */
	if (m_digit)
		integrator += m_filter;
	else
		integrator -= m_filter;

	/* simulate leakage */
	integrator *= m_leak;

	/* if we got all 0's or all 1's in the last n bits, bump the step up */
	if (((m_shiftreg & m_shiftreg_mask) == 0) ||
		((m_shiftreg & m_shiftreg_mask) == m_shiftreg_mask))
	{
		m_filter = FILTER_MAX - ((FILTER_MAX - m_filter) * m_charge);

		if (m_filter > FILTER_MAX)
			m_filter = FILTER_MAX;
	}

	/* simulate decay */
	else
	{
		m_filter *= m_decay;

		if (m_filter < FILTER_MIN)
			m_filter = FILTER_MIN;
	}

	/* compute the sample as a 32-bit word */
	temp = integrator * SAMPLE_GAIN;
	m_integrator = integrator;

	/* compress the sample range to fit better in a 16-bit word */
	if (temp < 0)
		m_next_sample = (int)(temp / (-temp * (1.0 / 32768.0) + 1.0));
	else
		m_next_sample = (int)(temp / (temp * (1.0 / 32768.0) + 1.0));
}

void hc55516_clock_w(INT32 state)
{
	UINT8 clock_state = state ? true : false;

	/* only makes sense for setups with a software driven clock */
	//assert(!is_external_oscillator());

	/* speech clock changing? */
	if (is_active_clock_transition(clock_state))
	{
		/* update the output buffer before changing the registers */
		UpdateStream(SyncInternal());

		/* clear the update count */
		m_update_count = 0;

		process_digit();
	}

	/* update the clock */
	m_last_clock_state = clock_state;
}


void hc55516_digit_w(INT32 digit)
{
	if (is_external_oscillator())
	{
		UpdateStream(SyncInternal());
		m_new_digit = digit & 1;
	}
	else
		m_digit = digit & 1;
}


INT32 hc55516_clock_state_r()
{
	/* only makes sense for setups with an external oscillator */
	//assert(is_external_oscillator());

	UpdateStream(SyncInternal());

	return current_clock_state();
}


//-------------------------------------------------
//  sound_stream_update - handle a stream update
//-------------------------------------------------

static void hc55516_update_int(INT16 *inputs, INT32 samples)
{
	INT16 *buffer = inputs;
	INT32 i;
	INT32 sample, slope;

	/* zero-length? bail */
	if (samples == 0)
		return;

	if (!is_external_oscillator())
	{
		/* track how many samples we've updated without a clock */
		m_update_count += samples;
		if (m_update_count > SAMPLE_RATE / 32)
		{
			m_update_count = SAMPLE_RATE;
			m_next_sample = 0;
		}
	}

	/* compute the interpolation slope */
	sample = m_curr_sample;
	slope = ((INT32)m_next_sample - sample) / samples;
	m_curr_sample = m_next_sample;

	if (is_external_oscillator())
	{
		/* external oscillator */
		for (i = 0; i < samples; i++, sample += slope)
		{
			UINT8 clock_state;

			*buffer = sample;
			buffer++;

			m_update_count++;

			clock_state = current_clock_state();

			/* pull in next digit on the appropriate edge of the clock */
			if (is_active_clock_transition(clock_state))
			{
				m_digit = m_new_digit;

				process_digit();
			}

			m_last_clock_state = clock_state;
		}
	}

	/* software driven clock */
	else
		for (i = 0; i < samples; i++, sample += slope) {
			*buffer = sample;
			buffer++;
		}
}

void hc55516_update(INT16 *inputs, INT32 sample_len)
{
	if (sample_len != nBurnSoundLen) {
		bprintf(PRINT_ERROR, _T("*** hc55516_update(): call once per frame!\n"));
		return;
	}

	INT32 samples_from = (INT32)((double)((SAMPLE_RATE * 100) / nBurnFPS) + 0.5);

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
