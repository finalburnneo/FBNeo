/*
   FBAlpha port by dink, Sept 2018

*****************************************************************************/

#include "burnint.h"
#include "redbaron.h"
#include <math.h>

#define OUTPUT_RATE (6000*4)
#define FRAME_SIZE 400

static INT16  *m_mixer_buffer; // re-sampler

static INT32 (*pCPUTotalCycles)() = NULL;
static UINT32  nDACCPUMHZ = 0;
static INT32   nCurrentPosition = 0;

static void bzone_update_int(INT16 *buffer, INT32 samples);

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

	bzone_update_int(lbuf, length);

	nCurrentPosition += length;
}

/* Statics */
static INT16 *discharge = NULL;
#define EXP(charge,n) (charge ? 0x7fff - discharge[0x7fff-n] : discharge[n])

static INT32 latch;
static INT32 poly_counter;
static INT32 poly_shift;

static INT32 explosion_clock;
static INT32 explosion_out;
static INT32 explosion_amp;
static INT32 explosion_amp_counter;

static INT32 shell_clock;
static INT32 shell_out;
static INT32 shell_amp;
static INT32 shell_amp_counter;

static INT32 motor_counter;
static INT32 motor_counter_a;
static INT32 motor_counter_b;
static INT32 motor_rate;
static INT32 motor_rate_new;
static INT32 motor_rate_counter;
static INT32 motor_amp;
static INT32 motor_amp_new;
static INT32 motor_amp_step;
static INT32 motor_amp_counter;

INT32 bzone_sound_enable = 0;

void bzone_sound_scan(INT32 nAction, INT32 *)
{
	SCAN_VAR(latch);
	SCAN_VAR(poly_counter);
	SCAN_VAR(poly_shift);

	SCAN_VAR(explosion_clock);
	SCAN_VAR(explosion_out);
	SCAN_VAR(explosion_amp);
	SCAN_VAR(explosion_amp_counter);

	SCAN_VAR(shell_clock);
	SCAN_VAR(shell_out);
	SCAN_VAR(shell_amp);
	SCAN_VAR(shell_amp_counter);

	SCAN_VAR(motor_counter);
	SCAN_VAR(motor_counter_a);
	SCAN_VAR(motor_counter_b);
	SCAN_VAR(motor_rate);
	SCAN_VAR(motor_rate_new);
	SCAN_VAR(motor_rate_counter);
	SCAN_VAR(motor_amp);
	SCAN_VAR(motor_amp_new);
	SCAN_VAR(motor_amp_step);
	SCAN_VAR(motor_amp_counter);
}

void bzone_sound_reset()
{
	latch = 0;
	poly_counter = 0;
	poly_shift = 0;

	explosion_clock = 0;
	explosion_out = 0;
	explosion_amp = 0;
	explosion_amp_counter = 0;

	shell_clock = 0;
	shell_out = 0;
	shell_amp = 0;
	shell_amp_counter = 0;

	motor_counter = 0;
	motor_counter_a = 0;
	motor_counter_b = 0;
	motor_rate = 0;
	motor_rate_new = 0;
	motor_rate_counter = 0;
	motor_amp = 0;
	motor_amp_new = 0;
	motor_amp_step = 0;
	motor_amp_counter = 0;

	bzone_sound_enable = 0;
}

void bzone_sound_init(INT32 (*pCPUCyclesCB)(), INT32 nCpuMHZ)
{
	pCPUTotalCycles = pCPUCyclesCB;
	nDACCPUMHZ = nCpuMHZ;

	m_mixer_buffer = (INT16*)BurnMalloc(2 * sizeof(INT16) * OUTPUT_RATE);

	discharge = (INT16 *)BurnMalloc(32768 * sizeof(INT16));
    for(INT32 i = 0; i < 0x8000; i++ )
		discharge[0x7fff-i] = (INT16) (0x7fff/exp(1.0*i/4096));
}

void bzone_sound_exit()
{
	BurnFree(m_mixer_buffer);
	BurnFree(discharge);
}

void bzone_update_int(INT16 *buffer, INT32 length)
{
	while( length-- )
	{
		static INT32 last_val = 0;
		INT32 sum = 0;

		/* polynome shifter H5 and H4 (LS164) clocked with 6kHz */
		poly_counter -= 6000;
		while( poly_counter <= 0 )
		{
			INT32 clock;

			poly_counter += OUTPUT_RATE;
			if( ((poly_shift & 0x0008) == 0) == ((poly_shift & 0x4000) == 0) )
				poly_shift = (poly_shift << 1) | 1;
			else
				poly_shift <<= 1;

			/* NAND gate J4 */
			clock = ((poly_shift & 0x7000) == 0x7000) ? 0 : 1;

			/* raising edge on pin 3 of J5 (LS74)? */
			if( clock && !explosion_clock )
				explosion_out ^= 1;

			/* save explo clock level */
			explosion_clock = clock;

			/* input 11 of J5 (LS74) */
			clock = (poly_shift >> 15) & 1;

			/* raising edge on pin 11 of J5 (LS74)? */
			if( clock && !shell_clock )
				shell_out ^= 1;

			/* save shell clock level */
			shell_clock = clock;
		}

		/* explosion enable: charge C14 */
		if( latch & 0x01 )
			explosion_amp = 32767;

		/* explosion output? */
		if( explosion_out )
		{
			if( explosion_amp > 0 )
			{
				/*
                 * discharge C14 through R17 + R16
                 * time constant is 10e-6 * 23000 = 0.23 seconds
                 * (samples were decaying much slower: 1/4th rate? )
                 */
				explosion_amp_counter -= (int)(32767 / (0.23*1));
				if( explosion_amp_counter < 0 )
				{
					INT32 n = (-explosion_amp_counter / OUTPUT_RATE) + 1;
					explosion_amp_counter += n * OUTPUT_RATE;
					if( (explosion_amp -= n) < 0 )
						explosion_amp = 0;
				}
			}
			/*
             * I don't know the amplification of the op-amp
             * and feedback, so the loud/soft values are arbitrary
             */
			if( latch & 0x02 )	/* explosion loud ? */
				sum += BURN_SND_CLIP(EXP(0,explosion_amp)/3);
			else
				sum += BURN_SND_CLIP(EXP(0,explosion_amp)/4);
		}

		/* shell enable: charge C9 */
		if( latch & 0x04 )
			shell_amp = 32767;

		/* shell output? */
		if( shell_out )
		{
			if( shell_amp > 0 )
			{
				/*
                 * discharge C9 through R14 + R15
                 * time constant is 4.7e-6 * 23000 = 0.1081 seconds
                 * (samples were decaying much slower: 1/4th rate? )
                 */
				shell_amp_counter -= (int)(32767 / (0.1081*4));
				if( shell_amp_counter < 0 )
				{
					INT32 n = (-shell_amp_counter / OUTPUT_RATE) + 1;
					shell_amp_counter += n * OUTPUT_RATE;
					if( (shell_amp -= n) < 0 )
						shell_amp = 0;
				}
			}
			/*
             * I don't know the amplification of the op-amp
             * and feedback, so the loud/soft values are arbitrary
             */
			if( latch & 0x08 )	/* shell loud ? */
				sum += BURN_SND_CLIP(EXP(0,shell_amp)/6);
			else
				sum += BURN_SND_CLIP(EXP(0,shell_amp)/8);
		}

		if( latch & 0x80 )
		{
			/* NE5555 timer
             * C = 0.018u, Ra = 100k, Rb = 125k
             * charge time = 0.693 * (Ra + Rb) * C = 3870us
             * discharge time = 0.693 * Rb * C = 1559.25us
             * freq approx. 184 Hz
             * I have no idea what frequencies are coming from the NE555
             * with "MOTOR REV EN" being high or low. I took 240Hz as
             * higher rate and sweep up or down to the new rate in 0.25s
             */
			motor_rate_new = (latch & 0x10) ? (940) : (240);
			if( motor_rate != motor_rate_new )
			{
				/* sweep rate to new rate */
				motor_rate_counter -= (int)(((940) - (240)) / 0.25);
				while( motor_rate_counter <= 0 )
				{
					motor_rate_counter += OUTPUT_RATE;
					motor_rate += (motor_rate < motor_rate_new) ? +1 : -1;
				}
			}
			motor_counter -= motor_rate;
			while( motor_counter <= 0 )
			{
				double r0, r1;

				motor_counter += OUTPUT_RATE;

				r0 = 1.0/1e12;
				r1 = 1.0/1e12;

				if( ++motor_counter_a == 16 )
					motor_counter_a = 6;
				if( ++motor_counter_b == 16 )
					motor_counter_b = 4;

				if( motor_counter_a & 8 )	/* bit 3 */
					r1 += 1.0/33000;
				else
					r0 += 1.0/33000;
				if( motor_counter_a == 15 ) /* ripple carry */
					r1 += 1.0/33000;
				else
					r0 += 1.0/33000;

				if( motor_counter_b & 8 )	/* bit 3 */
					r1 += 1.0/33000;
				else
					r0 += 1.0/33000;
				if( motor_counter_b == 15 ) /* ripple carry */
					r1 += 1.0/33000;
				else
					r0 += 1.0/33000;

				/* new voltage at C29 */
				r0 = 1.0/r0;
				r1 = 1.0/r1;
				motor_amp_new = (int)(32767 * r0 / (r0 + r1));

				/* charge/discharge C29 (0.47uF) */
				if( motor_amp_new > motor_amp )
					motor_amp_step = (int)((motor_amp_new - motor_amp) / (r1*0.47e-6));
				else
					motor_amp_step = (int)((motor_amp - motor_amp_new) / (r0*0.47e-6));
			}
			if( motor_amp != motor_amp_new )
			{
				motor_amp_counter -= motor_amp_step;
				if( motor_amp_counter < 0 )
				{
					INT32 n = (-motor_amp_counter / OUTPUT_RATE) + 1;
					motor_amp_counter += n * OUTPUT_RATE;
					if( motor_amp > motor_amp_new )
					{
						motor_amp -= n;
						if( motor_amp < motor_amp_new )
							motor_amp = motor_amp_new;
					}
					else
					{
						motor_amp += n;
						if( motor_amp > motor_amp_new )
							motor_amp = motor_amp_new;
					}
				}
			}
			sum += EXP((motor_amp<motor_amp_new),motor_amp)/30;
		}

		*buffer++ = BURN_SND_CLIP((sum + last_val) / 2);

		/* crude 75% low pass filter */
		last_val = 0;//(sum + last_val * 3) / 4;
	}
}

void bzone_sound_write(UINT8 data)
{
	if( data == latch )
		return;

	bzone_sound_enable = data & (1 << 5);

	UpdateStream(SyncInternal());

	latch = data;
}

void bzone_sound_update(INT16 *inputs, INT32 sample_len)
{
	if (sample_len != nBurnSoundLen) {
		bprintf(PRINT_ERROR, _T("*** bzone_sound_update(): call once per frame!\n"));
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
