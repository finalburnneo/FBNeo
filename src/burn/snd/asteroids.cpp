/*****************************************************************************
 *
 * A (partially wrong) try to emulate Asteroid's analog sound
 * It's getting better but is still imperfect :/
 * If you have ideas, corrections, suggestions contact Juergen
 * Buchmueller <pullmoll@t-online.de>
 *
 * Known issues (TODO):
 * - find out if/why the samples are 'damped', I don't see no
 *	 low pass filter in the sound output, but the samples sound
 *	 like there should be one. Maybe in the amplifier..
 * - better (accurate) way to emulate the low pass on the thrust sound?
 * - verify the thump_frequency calculation. It's only an approximation now
 * - the nastiest piece of the circuit is the saucer sound IMO
 *	 the circuits are almost equal, but there are some strange looking
 *	 things like the direct coupled op-amps and transistors and I can't
 *	 calculate the true resistance and thus voltage at the control
 *	 input (5) of the NE555s.
 * - saucer sound is not easy either and the calculations might be off, still.
 *
 *****************************************************************************/

#if defined (_MSC_VER)
#define _USE_MATH_DEFINES
#endif

#include <math.h>
#include "burnint.h"
#include "asteroids.h"
#include "biquad.h"
#include "rescap.h"

#define VMAX    32767
#define VMIN	0

#define SAUCEREN    0
#define SAUCRFIREEN 1
#define SAUCERSEL   2
#define THRUSTEN    3
#define SHIPFIREEN	4
#define LIFEEN		5

#define EXPITCH0	(1<<6)
#define EXPITCH1	(1<<7)
#define EXPAUDSHIFT 2
#define EXPAUDMASK	(0x0f<<EXPAUDSHIFT)

#define NE555_T1(Ra,Rb,C)	(VMAX*2/3/(0.639*((Ra)+(Rb))*(C)))
#define NE555_T2(Ra,Rb,C)	(VMAX*2/3/(0.639*(Rb)*(C)))
#define NE555_F(Ra,Rb,C)	(1.44/(((Ra)+2*(Rb))*(C)))

static INT16 *discharge;        // look up table
static INT16 vol_explosion[16]; // look up table
#define EXP(charge,n) (charge ? 0x7fff - discharge[0x7fff-((n)&0x7fff)] : discharge[(n)&0x7fff])

#ifdef INLINE
#undef INLINE
#endif

#define INLINE static inline

struct asteroid_sound {
	INT32 explosion_latch;
	INT32 explosion_counter;
	INT32 explosion_sample_counter;
	INT32 explosion_out;
	INT32 explosion_polynome;
	INT32 explosion_amp;
	double explosion_filt;
	double explosion_cap;

	INT32 thrust_counter;
	INT32 thrust_out;
	INT32 thrust_amp;

	INT32 thump_latch;
	INT32 thump_frequency;
	INT32 thump_counter;
	INT32 thump_out;
	INT32 thump_pulse;
	INT32 thump_amp;
	double thump_filt;
	double thump_cap;

	INT32 saucer_vco, saucer_vco_charge, saucer_vco_counter;
    INT32 saucer_out, saucer_counter;

	INT32 saucerfire_vco, saucerfire_vco_counter;
	INT32 saucerfire_amp, saucerfire_amp_counter;
	INT32 saucerfire_out, saucerfire_counter;

	INT32 shipfire_vco, shipfire_vco_counter;
	INT32 shipfire_amp, shipfire_amp_counter;
    INT32 shipfire_out, shipfire_counter;

	INT32 life_counter, life_out;

	INT32 sound_latch[8];
};

static asteroid_sound asound;
static BIQ biquad_thrust;
static BIQ biquad_thrust_bp;
static BIQ biquad_saucer;

INLINE INT32 explosion_INT(INT32 samplerate)
{
	asound.explosion_counter -= 12000;
	while( asound.explosion_counter <= 0 )
	{
		asound.explosion_counter += samplerate;
		if( ((asound.explosion_polynome & 0x4000) == 0) == ((asound.explosion_polynome & 0x0040) == 0) )
			asound.explosion_polynome = (asound.explosion_polynome << 1) | 1;
		else
			asound.explosion_polynome <<= 1;
		if( ++asound.explosion_sample_counter == 16 )
		{
			asound.explosion_sample_counter = 0;
			if( asound.explosion_latch & EXPITCH0 )
				asound.explosion_sample_counter |= 2 + 8;
			else
				asound.explosion_sample_counter |= 4;
			if( asound.explosion_latch & EXPITCH1 )
				asound.explosion_sample_counter |= 1 + 8;
		}
		/* ripple count output is high? */
		if( asound.explosion_sample_counter == 15 ) {
			asound.explosion_out = asound.explosion_polynome & 1;
		}
	}

	if (asound.explosion_amp) {
		INT16 rv = vol_explosion[asound.explosion_amp];
		return (asound.explosion_out) ? rv : -rv;
	}

	return 0;
}

static INT16 explosion_rc_filt(INT16 sam)
{
	if (asound.explosion_filt == 0.0) {
		asound.explosion_filt = 1.0 - exp(-1.0 / (3042.00 * 1e-6 * nBurnSoundRate));
		asound.explosion_cap = 0.0;
	}

	asound.explosion_cap += (sam - asound.explosion_cap) * asound.explosion_filt;
	return asound.explosion_cap - 0.5;
}

INLINE INT32 explosion(INT32 samplerate)
{
	return explosion_rc_filt(explosion_INT(samplerate));
}

INLINE INT32 thrust_INT(INT32 samplerate)
{
    if( asound.sound_latch[THRUSTEN] )
	{
		asound.thrust_out = 1;

		/* SHPSND filter */
		asound.thrust_counter -= (110);
		while( asound.thrust_counter <= 0 )
		{
			asound.thrust_counter += samplerate;
		}
		if( asound.thrust_out )
		{
			if( asound.thrust_amp < VMAX )
				asound.thrust_amp += (VMAX - asound.thrust_amp) * 32768 / 32 / samplerate + 1;
		}
		else
		{
			if( asound.thrust_amp > VMIN )
				asound.thrust_amp -= asound.thrust_amp * 32768 / 32 / samplerate + 1;
		}
		return asound.thrust_amp * (asound.explosion_polynome & 1);
	} else {
		asound.thrust_out = 0;

		// decay thrust amp for no clicks -dink
		asound.thrust_amp *= (double)0.997;
		return asound.thrust_amp * (asound.explosion_polynome & 1);
	}
	return 0;
}

INLINE INT32 thrust(INT32 samplerate)
{
	return biquad_thrust.filter(biquad_thrust_bp.filter(thrust_INT(samplerate)*3.00));
}

INLINE INT32 thump_INT(INT32 samplerate)
{
    if( asound.thump_latch & 0x10 )
	{
		if (asound.thump_out) { //pulse UP
			asound.thump_counter -= asound.thump_frequency * 9 / 16;
			while( asound.thump_counter <= 0 )
			{
				asound.thump_counter += samplerate * 9 / 16;
				asound.thump_out = 0;
			}
		} else {                  //pulse DOWN
			asound.thump_counter -= asound.thump_frequency * 7 / 16;
			while( asound.thump_counter <= 0 )
			{
				asound.thump_counter += samplerate * 7 / 16;
				asound.thump_out = 1;
			}
		}
		asound.thump_amp = (asound.thump_out) ? (VMAX/2) : -(VMAX/2);
		return asound.thump_amp;
	}

	return 0;
}

static INT16 thump_rc_filt(INT16 sam)
{
	if (asound.thump_filt == 0.0) {
		asound.thump_filt = 1.0 - exp(-1.0 / (RES_K(3.3) * CAP_U(0.1) * nBurnSoundRate));
		asound.thump_cap = 0.0;
	}

	asound.thump_cap += (sam - asound.thump_cap) * asound.thump_filt;
	return asound.thump_cap;
}

INLINE INT32 thump(INT32 samplerate)
{
	return thump_rc_filt(thump_INT(samplerate));
}

INLINE INT32 saucer_INT(INT32 samplerate)
{
	double v5;

    /* saucer sound enabled ? */
	if( asound.sound_latch[SAUCEREN] )
	{
		/* NE555 setup as astable multivibrator:
		 * C = 10u, Ra = 5.6k, Rb = 10k
		 * or, with /SAUCERSEL being low:
		 * C = 10u, Ra = 5.6k, Rb = 6k (10k parallel with 15k)
		 */
		if( asound.saucer_vco_charge )
		{
			if( asound.sound_latch[SAUCERSEL] )
				asound.saucer_vco_counter -= NE555_T1(5600,10000,10e-6) / 2;
			else
				asound.saucer_vco_counter -= NE555_T1(5600,6000,10e-6) / 2;
			if( asound.saucer_vco_counter <= 0 )
			{
				INT32 steps = (-asound.saucer_vco_counter / samplerate) + 1;
				asound.saucer_vco_counter += steps * samplerate;
				if( (asound.saucer_vco += steps) >= VMAX*3/3 )
				{
					asound.saucer_vco = VMAX*3/3;
					asound.saucer_vco_charge = 0;
				}
			}
		}
		else
		{
			if( asound.sound_latch[SAUCERSEL] )
				asound.saucer_vco_counter -= NE555_T2(5600,10000,10e-6) / 2;
			else
				asound.saucer_vco_counter -= NE555_T2(5600,6000,10e-6) / 2;
			if( asound.saucer_vco_counter <= 0 )
			{
				INT32 steps = (-asound.saucer_vco_counter / samplerate) + 1;
				asound.saucer_vco_counter += steps * samplerate;
				if ( (asound.saucer_vco -= steps) <= VMAX*2/3)
				{
				    asound.saucer_vco = VMAX*2/3;
					asound.saucer_vco_charge = 1;
				}
			}
		}
		/*
		 * NE566 voltage controlled oscillator
		 * Co = 0.047u, Ro = 10k
		 * to = 2.4 * (Vcc - V5) / (Ro * Co * Vcc)
		 */

		INT32 happy_vco = (asound.saucer_vco_charge) ? (asound.saucer_vco + (32768 - VMAX*2/3)) : asound.saucer_vco;

		if( asound.sound_latch[SAUCERSEL] )
			v5 = 12.0 - 1.66 - 5.0 * EXP(asound.saucer_vco_charge,happy_vco) / 32768;
		else
			v5 = 11.3 - 1.66 - 5.0 * EXP(asound.saucer_vco_charge,happy_vco) / 32768;
		asound.saucer_counter -= floor(2.4 * (12.0 - v5) / (10000 * 0.047e-6 * 12.0));
		while( asound.saucer_counter <= 0 )
		{
			asound.saucer_counter += samplerate;
			asound.saucer_out ^= 1;
		}
		if( asound.saucer_out )
			return VMAX;
	}
	return 0;
}

INLINE INT32 saucer(INT32 samplerate)
{
	return biquad_saucer.filter(saucer_INT(samplerate));
}

INLINE INT32 saucerfire(INT32 samplerate)
{
    if( asound.sound_latch[SAUCRFIREEN] )
	{
		if( asound.saucerfire_vco < VMAX*12/5 )
		{
			/* charge C38 (10u) through R54 (10K) from 5V to 12V */
			#define C38_CHARGE_TIME (VMAX * 2)
			asound.saucerfire_vco_counter -= C38_CHARGE_TIME;
			while( asound.saucerfire_vco_counter <= 0 )
			{
				asound.saucerfire_vco_counter += samplerate;
				if( ++asound.saucerfire_vco == VMAX*12/5 )
					break;
			}
		}
		if( asound.saucerfire_amp > VMIN )
		{
			/* discharge C39 (10u) through R58 (10K) and diode CR6,
			 * but only during the time the output of the NE555 is low.
			 */
			if( asound.saucerfire_out )
			{
				#define C39_DISCHARGE_TIME (int)(VMAX)
				asound.saucerfire_amp_counter -= C39_DISCHARGE_TIME;
				while( asound.saucerfire_amp_counter <= 0 )
				{
					asound.saucerfire_amp_counter += samplerate;
					if( --asound.saucerfire_amp == VMIN )
						break;
				}
			}
		}
		if( asound.saucerfire_out )
		{
			/* C35 = 1u, Ra = 3.3k, Rb = 680
			 * discharge = 0.693 * 680 * 1e-6 = 4.7124e-4 -> 2122 Hz
			 */
			asound.saucerfire_counter -= 2122;
			if( asound.saucerfire_counter <= 0 )
			{
				INT32 n = -asound.saucerfire_counter / samplerate + 1;
				asound.saucerfire_counter += (EXP(1,n)/8) * samplerate;
				asound.saucerfire_out = 0;
			}
		}
		else
		{
			/* C35 = 1u, Ra = 3.3k, Rb = 680
			 * charge 0.693 * (3300+680) * 1e-6 = 2.75814e-3 -> 363Hz
			 */
			asound.saucerfire_counter -= 363 * 2.5 * (VMAX*12/5-asound.saucerfire_vco) / 32768;
			if( asound.saucerfire_counter <= 0 )
			{
				INT32 n = -asound.saucerfire_counter / samplerate + 1;
				asound.saucerfire_counter += n * samplerate;
				asound.saucerfire_out = 1;
			}
		}
        if( asound.saucerfire_out )
			return EXP(0, asound.saucerfire_amp);
	}
	else
	{
		/* charge C38 and C39 */
		asound.saucerfire_amp = VMAX;
		asound.saucerfire_vco = VMAX;
	}
	return 0;
}

static double rc_discharge_exp = 0;
static INT32  rc_discharge_state = 0;
static double rc_discharge_time = 0;

static INT32 rc_ramp_state = 0;
static double rc_ramp_step = 0;
static double rc_ramp_val = 0;

static double rc_square_phase = 0;
static double rc_square_trig = 0;

void asteroid_sound_scan()
{
	SCAN_VAR(asound);

	SCAN_VAR(rc_discharge_exp);
	SCAN_VAR(rc_discharge_state);
	SCAN_VAR(rc_discharge_time);

	SCAN_VAR(rc_ramp_state);
	SCAN_VAR(rc_ramp_step);
	SCAN_VAR(rc_ramp_val);

	SCAN_VAR(rc_square_phase);
	SCAN_VAR(rc_square_trig);
}

static double rc_square(INT32 on, double freq, double amp, double duty)
{
	double rv = 0;

	if (on) {
		rc_square_trig = ((100 - duty) / 100) * (2.0 * M_PI);
		rv = (rc_square_phase > rc_square_trig) ? amp / 2.0 : -amp / 2.0;
		rc_square_phase = fmod(rc_square_phase + ((2.0 * M_PI * freq) / nBurnSoundRate), 2.0 * M_PI);
	}

	return rv;
}

static double rc_ramp_down(INT32 on, double start, double end, double speed)
{
	if (rc_ramp_step == 0.0) {
		rc_ramp_step = ((start - end) / speed) / nBurnSoundRate;
	}
	if (on) {
		if (rc_ramp_state == 0) {
			rc_ramp_state = 1;
			rc_ramp_val = start;
		}

		rc_ramp_val -= rc_ramp_step;

		if (rc_ramp_val < end) rc_ramp_val = end;
		if (rc_ramp_val > start) rc_ramp_val = start;
	} else {
		rc_ramp_state = 0;
		rc_ramp_val = 0;
	}
	return rc_ramp_val;
}

static double rc_discharge(INT32 on, double in, double r, double c)
{
	double rv = 0;

	if (rc_discharge_exp == 0.0) { // set-up
		rc_discharge_exp = -1.0 * r * c;
		rc_discharge_state = 0;
		rc_discharge_time = 0;
	}

	if (on) {
		if (rc_discharge_state == 0) {
			rc_discharge_state = 1; // reset state
			rc_discharge_time = 0;
		}

		rv = in * exp(rc_discharge_time / rc_discharge_exp);
		rc_discharge_time += 1.0 / nBurnSoundRate;
	} else {
		rc_discharge_state = 0;
	}

	return rv;
}

static INT32 shipfire()
{
    INT32 on = asound.sound_latch[SHIPFIREEN];

	INT32 freq = rc_ramp_down(on, 820, 110, 0.280147);
	INT32 amp = rc_discharge(on, 46.0, 8100, CAP_U(10)) + 7.0;
	INT32 duty = (4500.0 / freq) + 67;
	double square = rc_square(on, freq, amp, duty);

	return square * 1000;
}

INLINE INT32 life(INT32 samplerate)
{
    if( asound.sound_latch[LIFEEN] )
	{
		asound.life_counter -= 3000;
		while( asound.life_counter <= 0 )
		{
			asound.life_counter += samplerate;
			asound.life_out ^= 1;
		}
		if( asound.life_out )
			return VMAX;
	}
	return 0;
}


void asteroid_sound_update(INT16 *buffer, INT32 length)
{
	INT32 samplerate = nBurnSoundRate;

    while( length-- > 0 )
	{
		INT32 sum = 0;

		sum += explosion(samplerate) / 1.5;
		sum += thrust(samplerate) / 2;
		sum += thump(samplerate) / 7;
		sum += saucer(samplerate) / 24;
		sum += saucerfire(samplerate) / 10;
		sum += shipfire() / 9;
		sum += life(samplerate) / 8;
		sum = BURN_SND_CLIP(sum * 0.75);

		*buffer++ = sum;
        *buffer++ = sum;
	}
}

static void explosion_init(void)
{
    for( INT32 i = 0; i < 16; i++ )
    {
        /* r0 = open, r1 = open */
        double r0 = 1.0/1e12, r1 = 1.0/1e12;

        /* R14 */
        if( i & 1 )
            r1 += 1.0/47000;
        else
            r0 += 1.0/47000;
        /* R15 */
        if( i & 2 )
            r1 += 1.0/22000;
        else
            r0 += 1.0/22000;
        /* R16 */
        if( i & 4 )
            r1 += 1.0/12000;
        else
            r0 += 1.0/12000;
        /* R17 */
        if( i & 8 )
            r1 += 1.0/5600;
        else
            r0 += 1.0/5600;
        r0 = 1.0/r0;
        r1 = 1.0/r1;
		vol_explosion[i] = (VMAX * r0 / (r0 + r1));
    }

}

void asteroid_sound_init()
{
	discharge = (INT16 *)BurnMalloc(32768 * sizeof(INT16));
	if( !discharge ) {
		bprintf(0, _T("Unable to allocate 64k ram for Asteroids sound custom.. crashing soon!\n"));
		return;
	}

    for( INT32 i = 0; i < 0x8000; i++ )
		discharge[0x7fff-i] = (INT16) (0x7fff/exp(1.0*i/4096));

	/* initialize explosion volume lookup table */
	explosion_init();

	biquad_thrust.init(FILT_LOWPASS, nBurnSoundRate, 160.00, 1.00, 0);
	biquad_thrust_bp.init(FILT_BANDPASS, nBurnSoundRate, 89.50, 7.600, 0);
	biquad_saucer.init(FILT_LOWPASS, nBurnSoundRate, 14400.00, 1.00, 0);

	return;
}

void asteroid_sound_exit()
{
	if( discharge )
		BurnFree(discharge);
	discharge = NULL;

	biquad_thrust.exit();
	biquad_thrust_bp.exit();
	biquad_saucer.exit();
}

void asteroid_sound_reset()
{
	memset(&asound, 0, sizeof(asound));

	biquad_thrust.reset();
	biquad_thrust_bp.reset();
	biquad_saucer.reset();
}

void asteroid_explode_w(UINT8 data)
{
	if( data == asound.explosion_latch )
		return;

	asound.explosion_latch = data;
	asound.explosion_amp = (asound.explosion_latch & EXPAUDMASK) >> EXPAUDSHIFT;
}

void asteroid_thump_w(UINT8 data)
{
	double r0 = 1.0/1e12;
	double r1 = 1.0/1e12;

	if ((asound.thump_latch & 0x10) == 0 && data & 0x10) {
		asound.thump_counter = 0; // sync start of waveform
		asound.thump_out = 0;
	}

    if( data == asound.thump_latch )
		return;

	asound.thump_latch = data;

	if( asound.thump_latch & 1 )
		r1 += 1.0/220000;
	else
		r0 += 1.0/220000;

	if( asound.thump_latch & 2 )
		r1 += 1.0/100000;
	else
		r0 += 1.0/100000;
	if( asound.thump_latch & 4 )
		r1 += 1.0/47000;
	else
		r0 += 1.0/47000;
	if( asound.thump_latch & 8 )
		r1 += 1.0/22000;
	else
		r0 += 1.0/22000;

	/* NE555 setup as voltage controlled astable multivibrator
	 * C = 0.22u, Ra = 22k...???, Rb = 18k
	 * frequency = 1.44 / ((22k + 2*18k) * 0.22n) = 56Hz .. huh?
	 */

	asound.thump_frequency = (56 + 110) + (56) * r1 / (r1 + r0);
}


void asteroid_sounds_w(UINT16 offset, UINT8 data)
{
	data &= 0x80;
    if( data == asound.sound_latch[offset] )
		return;

	asound.sound_latch[offset] = data;
}


void astdelux_sound_update(INT16 *buffer, INT32 length)
{
	INT32 samplerate = nBurnSoundRate;

    while( length-- > 0)
	{
		INT32 sum = 0;

		sum += explosion(samplerate) / 1.5;
		sum += thrust(samplerate) / 2;
		sum = BURN_SND_CLIP(sum * 0.55);

		*buffer++ = sum;
		*buffer++ = sum;
	}
}

void astdelux_sounds_w(UINT8 data)
{
	data = data & 0x80;
	if( data == asound.sound_latch[THRUSTEN] )
		return;
	asound.sound_latch[THRUSTEN] = data;
}
