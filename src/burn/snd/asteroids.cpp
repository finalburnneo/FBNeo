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

#include <math.h>
#include "burnint.h"
#include "asteroids.h"

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

static INT32 explosion_latch;
static INT32 thump_latch;
static INT32 sound_latch[8];

static INT32 polynome;
static INT32 thump_frequency;

static INT16 *discharge;        // look up table
static INT16 vol_explosion[16]; // look up table
#define EXP(charge,n) (charge ? 0x7fff - discharge[0x7fff-n] : discharge[n])

#ifdef INLINE
#undef INLINE
#endif

#define INLINE static inline

struct asteroid_sound {
	INT32 explosion_counter;
	INT32 explosion_sample_counter;
	INT32 explosion_out;

	INT32 thrust_counter;
	INT32 thrust_out;
	INT32 thrust_amp;

	INT32 thump_counter;
	INT32 thump_out;

	INT32 saucer_vco, saucer_vco_charge, saucer_vco_counter;
    INT32 saucer_out, saucer_counter;

	INT32 saucerfire_vco, saucerfire_vco_counter;
	INT32 saucerfire_amp, saucerfire_amp_counter;
	INT32 saucerfire_out, saucerfire_counter;

	INT32 shipfire_vco, shipfire_vco_counter;
	INT32 shipfire_amp, shipfire_amp_counter;
    INT32 shipfire_out, shipfire_counter;

	INT32 life_counter, life_out;
};

static asteroid_sound asound;

INLINE INT32 explosion(INT32 samplerate)
{
	//static INT32 counter, sample_counter;
	//static INT32 out;

	asound.explosion_counter -= 12000;
	while( asound.explosion_counter <= 0 )
	{
		asound.explosion_counter += samplerate;
		if( ((polynome & 0x4000) == 0) == ((polynome & 0x0040) == 0) )
			polynome = (polynome << 1) | 1;
		else
			polynome <<= 1;
		if( ++asound.explosion_sample_counter == 16 )
		{
			asound.explosion_sample_counter = 0;
			if( explosion_latch & EXPITCH0 )
				asound.explosion_sample_counter |= 2 + 8;
			else
				asound.explosion_sample_counter |= 4;
			if( explosion_latch & EXPITCH1 )
				asound.explosion_sample_counter |= 1 + 8;
		}
		/* ripple count output is high? */
		if( asound.explosion_sample_counter == 15 )
			asound.explosion_out = polynome & 1;
	}
	if( asound.explosion_out )
		return vol_explosion[(explosion_latch & EXPAUDMASK) >> EXPAUDSHIFT];

    return 0;
}

INLINE INT32 thrust(INT32 samplerate)
{
	//static INT32 counter, out, amp;

    if( sound_latch[THRUSTEN] )
	{
		/* SHPSND filter */
		asound.thrust_counter -= 110;
		while( asound.thrust_counter <= 0 )
		{
			asound.thrust_counter += samplerate;
			asound.thrust_out = polynome & 1;
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
		return asound.thrust_amp;
	} else {
		// decay thrust amp for no clicks -dink
		asound.thrust_amp *= (double)0.997;
		return asound.thrust_amp;
	}
	return 0;
}

INLINE INT32 thump(INT32 samplerate)
{
	//static INT32 counter, out;

    if( thump_latch & 0x10 )
	{
		asound.thump_counter -= thump_frequency;
		while( asound.thump_counter <= 0 )
		{
			asound.thump_counter += samplerate;
			asound.thump_out ^= 1;
		}
		if( asound.thump_out )
			return VMAX;
	}
	return 0;
}


INLINE INT32 saucer(INT32 samplerate)
{
	//static INT32 vco, vco_charge, vco_counter;  asound.saucer_
    //static INT32 out, counter;
	double v5;

    /* saucer sound enabled ? */
	if( sound_latch[SAUCEREN] )
	{
		/* NE555 setup as astable multivibrator:
		 * C = 10u, Ra = 5.6k, Rb = 10k
		 * or, with /SAUCERSEL being low:
		 * C = 10u, Ra = 5.6k, Rb = 6k (10k parallel with 15k)
		 */
		if( asound.saucer_vco_charge )
		{
			if( sound_latch[SAUCERSEL] )
				asound.saucer_vco_counter -= NE555_T1(5600,10000,10e-6);
			else
				asound.saucer_vco_counter -= NE555_T1(5600,6000,10e-6);
			if( asound.saucer_vco_counter <= 0 )
			{
				INT32 steps = (-asound.saucer_vco_counter / samplerate) + 1;
				asound.saucer_vco_counter += steps * samplerate;
				if( (asound.saucer_vco += steps) >= VMAX*2/3 )
				{
					asound.saucer_vco = VMAX*2/3;
					asound.saucer_vco_charge = 0;
				}
			}
		}
		else
		{
			if( sound_latch[SAUCERSEL] )
				asound.saucer_vco_counter -= NE555_T2(5600,10000,10e-6);
			else
				asound.saucer_vco_counter -= NE555_T2(5600,6000,10e-6);
			if( asound.saucer_vco_counter <= 0 )
			{
				INT32 steps = (-asound.saucer_vco_counter / samplerate) + 1;
				asound.saucer_vco_counter += steps * samplerate;
				if( (asound.saucer_vco -= steps) <= VMAX*1/3 )
				{
					asound.saucer_vco = VMIN*1/3;
					asound.saucer_vco_charge = 1;
				}
			}
		}
		/*
		 * NE566 voltage controlled oscillator
		 * Co = 0.047u, Ro = 10k
		 * to = 2.4 * (Vcc - V5) / (Ro * Co * Vcc)
		 */
		if( sound_latch[SAUCERSEL] )
			v5 = 12.0 - 1.66 - 5.0 * EXP(asound.saucer_vco_charge,asound.saucer_vco) / 32768;
		else
			v5 = 11.3 - 1.66 - 5.0 * EXP(asound.saucer_vco_charge,asound.saucer_vco) / 32768;
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

INLINE INT32 saucerfire(INT32 samplerate)
{
	//static INT32 vco, vco_counter;
	//static INT32 amp, amp_counter;
	//static INT32 out, counter;

    if( sound_latch[SAUCRFIREEN] )
	{
		if( asound.saucerfire_vco < VMAX*12/5 )
		{
			/* charge C38 (10u) through R54 (10K) from 5V to 12V */
			#define C38_CHARGE_TIME (VMAX)
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
				asound.saucerfire_counter += n * samplerate;
				asound.saucerfire_out = 0;
			}
		}
		else
		{
			/* C35 = 1u, Ra = 3.3k, Rb = 680
			 * charge 0.693 * (3300+680) * 1e-6 = 2.75814e-3 -> 363Hz
			 */
			asound.saucerfire_counter -= 363 * 2 * (VMAX*12/5-asound.saucerfire_vco) / 32768;
			if( asound.saucerfire_counter <= 0 )
			{
				INT32 n = -asound.saucerfire_counter / samplerate + 1;
				asound.saucerfire_counter += n * samplerate;
				asound.saucerfire_out = 1;
			}
		}
        if( asound.saucerfire_out )
			return asound.saucerfire_amp;
	}
	else
	{
		/* charge C38 and C39 */
		asound.saucerfire_amp = VMAX;
		asound.saucerfire_vco = VMAX;
	}
	return 0;
}

INLINE INT32 shipfire(INT32 samplerate)
{
	//static INT32 vco, vco_counter;
	//static INT32 amp, amp_counter;
    //static INT32 out, counter;

    if( sound_latch[SHIPFIREEN] )
	{
		if( asound.shipfire_vco < VMAX*12/5 )
		{
			/* charge C47 (1u) through R52 (33K) and Q3 from 5V to 12V */
			#define C47_CHARGE_TIME (VMAX * 3)
			asound.shipfire_vco_counter -= C47_CHARGE_TIME;
			while( asound.shipfire_vco_counter <= 0 )
			{
				asound.shipfire_vco_counter += samplerate;
				if( ++asound.shipfire_vco == VMAX*12/5 )
					break;
			}
        }
		if( asound.shipfire_amp > VMIN )
		{
			/* discharge C48 (10u) through R66 (2.7K) and CR8,
			 * but only while the output of theNE555 is low.
			 */
			if( asound.shipfire_out )
			{
				#define C48_DISCHARGE_TIME (VMAX * 3)
				asound.shipfire_amp_counter -= C48_DISCHARGE_TIME;
				while( asound.shipfire_amp_counter <= 0 )
				{
					asound.shipfire_amp_counter += samplerate;
					if( --asound.shipfire_amp == VMIN )
						break;
				}
			}
		}

		if( asound.shipfire_out )
		{
			/* C50 = 1u, Ra = 3.3k, Rb = 680
			 * discharge = 0.693 * 680 * 1e-6 = 4.7124e-4 -> 2122 Hz
			 */
			asound.shipfire_counter -= 2122;
			if( asound.shipfire_counter <= 0 )
			{
				INT32 n = -asound.shipfire_counter / samplerate + 1;
				asound.shipfire_counter += n * samplerate;
				asound.shipfire_out = 0;
			}
		}
		else
		{
			/* C50 = 1u, Ra = R65 (3.3k), Rb = R61 (680)
			 * charge = 0.693 * (3300+680) * 1e-6) = 2.75814e-3 -> 363Hz
			 */
			asound.shipfire_counter -= 363 * 2 * (VMAX*12/5-asound.shipfire_vco) / 32768;
			if( asound.shipfire_counter <= 0 )
			{
				INT32 n = -asound.shipfire_counter / samplerate + 1;
				asound.shipfire_counter += n * samplerate;
				asound.shipfire_out = 1;
			}
		}
		if( asound.shipfire_out )
			return asound.shipfire_amp;
	}
	else
	{
		/* charge C47 and C48 */
		asound.shipfire_amp = VMAX;
		asound.shipfire_vco = VMAX;
	}
	return 0;
}

INLINE INT32 life(INT32 samplerate)
{
	//static INT32 asound.life_counter, out;
    if( sound_latch[LIFEEN] )
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

		sum += explosion(samplerate) / 7;
		sum += thrust(samplerate) / 7;
		sum += thump(samplerate) / 7;
		sum += saucer(samplerate) / 7;
		sum += saucerfire(samplerate) / 7;
		sum += shipfire(samplerate) / 7;
		sum += life(samplerate) / 7;

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
        vol_explosion[i] = VMAX * r0 / (r0 + r1);
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

    return;
}

void asteroid_sound_exit()
{
	if( discharge )
		BurnFree(discharge);
	discharge = NULL;
}

void asteroid_sound_reset()
{
	explosion_latch = 0;
	thump_latch = 0;
	memset(sound_latch, 0, sizeof(sound_latch));
	polynome = 0;
	thump_frequency = 0;

	memset(&asound, 0, sizeof(asound));
}

void asteroid_explode_w(UINT8 data)
{
	if( data == explosion_latch )
		return;

	explosion_latch = data;
}

void asteroid_thump_w(UINT8 data)
{
	double r0 = 1/47000, r1 = 1/1e12;

    if( data == thump_latch )
		return;

	thump_latch = data;

	if( thump_latch & 1 )
		r1 += 1.0/220000;
	else
		r0 += 1.0/220000;
	if( thump_latch & 2 )
		r1 += 1.0/100000;
	else
		r0 += 1.0/100000;
	if( thump_latch & 4 )
		r1 += 1.0/47000;
	else
		r0 += 1.0/47000;
	if( thump_latch & 8 )
		r1 += 1.0/22000;
	else
		r0 += 1.0/22000;

	/* NE555 setup as voltage controlled astable multivibrator
	 * C = 0.22u, Ra = 22k...???, Rb = 18k
	 * frequency = 1.44 / ((22k + 2*18k) * 0.22n) = 56Hz .. huh?
	 */
	thump_frequency = 56 + 56 * r0 / (r0 + r1);
}


void asteroid_sounds_w(UINT16 offset, UINT8 data)
{
	data &= 0x80;
    if( data == sound_latch[offset] )
		return;

	sound_latch[offset] = data;
}


void astdelux_sound_update(INT16 *buffer, INT32 length)
{
	INT32 samplerate = nBurnSoundRate;

    while( length-- > 0)
	{
		INT32 sum = 0;

		sum += explosion(samplerate) / 2;
		sum += thrust(samplerate) / 2;

		*buffer++ = sum;
		*buffer++ = sum;
	}
}

void astdelux_sounds_w(UINT8 data)
{
	data = data & 0x80;
	if( data == sound_latch[THRUSTEN] )
		return;
	sound_latch[THRUSTEN] = data;
}


