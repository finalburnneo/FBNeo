/****************************************************************************
 *
 * Phoenix sound hardware simulation - still very ALPHA!
 *
 * If you find errors or have suggestions, please mail me.
 * Juergen Buchmueller <pullmoll@t-online.de>
 *
 ****************************************************************************/


#include <math.h>
#include "burnint.h"
#include "tms36xx.h"
#include "rescap.h"

/****************************************************************************
 * 4006
 * Dual 4-bit and dual 5-bit serial-in serial-out shift registers.
 *
 *			+----------+
 *		1D5 |1	+--+ 14| VCC
 *	   /1Q4 |2		 13| 1Q1
 *		CLK |3		 12| 2Q0
 *		2D4 |4	4006 11| 2Q0
 *		3D4 |5		 10| 3Q0
 *		4D5 |6		  9| 4Q0
 *		GND |7		  8| 4Q1
 *			+----------+
 *
 * [This information is part of the GIICM]
 *
 * Pin 8 and 9 are connected to an EXOR gate and the inverted
 * output (EXNOR) is fed back to pin 1 (and the pseudo polynome output).
 *
 *		1D5 		 1Q1  2D4		2Q0  3D4	   3Q0	4D5 	 4Q1 4Q0
 *		+--+--+--+--+--+  +--+--+--+--+  +--+--+--+--+	+--+--+--+--+--+
 *	 +->| 0| 1| 2| 3| 4|->| 5| 6| 7| 8|->| 9|10|11|12|->|13|14|15|16|17|
 *	 |	+--+--+--+--+--+  +--+--+--+--+  +--+--+--+--+	+--+--+--+--+--+
 *	 |											 ____			  |  |
 *	 |											/	 |------------+  |
 *	 +-----------------------------------------|EXNOR|				 |
 *												\____|---------------+
 *
 ****************************************************************************/

#define VMIN    0
#define VMAX	32767

static INT32 sound_latch_a;
static INT32 sound_latch_b;

static INT32 tone1_vco1_cap;
static INT32 tone1_level;
static INT32 tone2_level;

static INT32 tone1_vco1_output, tone1_vco1_counter, tone1_vco1_level;
static INT32 tone1_vco2_output, tone1_vco2_counter, tone1_vco2_level;
static INT32 tone1_vco_counter, tone1_vco_level, tone1_vco_rate, tone1_vco_charge;
static INT32 tone1_counter, tone1_divisor, tone1_output;
static INT32 tone2_vco_counter, tone2_vco_level;
static INT32 tone2_counter, tone2_divisor, tone2_output;
static INT32 c24_counter, c24_level;
static INT32 c25_counter, c25_level;
static INT32 noise_counter, noise_polyoffs, noise_polybit, noise_lowpass_counter, noise_lowpass_polybit;

static UINT32 *poly18 = NULL;

static INT32 phoenixsnd_initted = 0;

void phoenix_sound_scan(INT32 nAction, INT32 *pnMin)
{
	tms36xx_scan(nAction, pnMin);

	SCAN_VAR(sound_latch_a);
	SCAN_VAR(sound_latch_b);
	SCAN_VAR(tone1_vco1_cap);
	SCAN_VAR(tone1_level);
	SCAN_VAR(tone2_level);

	SCAN_VAR(tone1_vco1_output); SCAN_VAR(tone1_vco1_counter); SCAN_VAR(tone1_vco1_level);
	SCAN_VAR(tone1_vco2_output); SCAN_VAR(tone1_vco2_counter); SCAN_VAR(tone1_vco2_level);
	SCAN_VAR(tone1_vco_counter); SCAN_VAR(tone1_vco_level); SCAN_VAR(tone1_vco_rate); SCAN_VAR(tone1_vco_charge);
	SCAN_VAR(tone1_counter); SCAN_VAR(tone1_divisor); SCAN_VAR(tone1_output);
	SCAN_VAR(tone2_vco_counter); SCAN_VAR(tone2_vco_level);
	SCAN_VAR(tone2_counter); SCAN_VAR(tone2_divisor); SCAN_VAR(tone2_output);
	SCAN_VAR(c24_counter); SCAN_VAR(c24_level);
	SCAN_VAR(c25_counter); SCAN_VAR(c25_level);
	SCAN_VAR(noise_counter); SCAN_VAR(noise_polyoffs); SCAN_VAR(noise_polybit); SCAN_VAR(noise_lowpass_counter); SCAN_VAR(noise_lowpass_polybit);
}

static INT32 tone1_vco1(INT32 samplerate)
{
	/*
	 * L447 (NE555): Ra=47k, Rb=100k, C = 0.01uF, 0.48uF, 1.01uF or 1.48uF
     * charge times = 0.639*(Ra+Rb)*C = 0.0092s, 0.0451s, 0.0949s, 0.1390s
     * discharge times = 0.639*Rb*C = 0.0064s, 0.0307s, 0.0645s, 0.0946s
     */
    #define C18a    0.01e-6
	#define C18b	0.48e-6
	#define C18c	1.01e-6
	#define C18d	1.48e-6
	#define R40 	47000
	#define R41 	100000

	static const INT32 rate[2][4] = {
		{
			(int)(double)(VMAX*2/3/(1.639*(R40+R41)*C18a)),
			(int)(double)(VMAX*2/3/(1.639*(R40+R41)*C18b)),
			(int)(double)(VMAX*2/3/(1.639*(R40+R41)*C18c)),
			(int)(double)(VMAX*2/3/(1.639*(R40+R41)*C18d))
		},
		{
			(int)(double)(VMAX*2/3/(1.639*R41*C18a)),
			(int)(double)(VMAX*2/3/(1.639*R41*C18b)),
			(int)(double)(VMAX*2/3/(1.639*R41*C18c)),
			(int)(double)(VMAX*2/3/(1.639*R41*C18d))
        }
	};
	if( tone1_vco1_output )
	{
		if (tone1_vco1_level > VMAX*1/3)
		{
			tone1_vco1_counter -= rate[1][tone1_vco1_cap];
			if( tone1_vco1_counter <= 0 )
			{
				INT32 steps = -tone1_vco1_counter / samplerate + 1;
				tone1_vco1_counter += steps * samplerate;
				if( (tone1_vco1_level -= steps) <= VMAX*1/3 )
				{
					tone1_vco1_level = VMAX*1/3;
                    tone1_vco1_output = 0;
				}
			}
		}
	}
	else
	{
		if (tone1_vco1_level < VMAX*2/3)
		{
			tone1_vco1_counter -= rate[0][tone1_vco1_cap];
			if( tone1_vco1_counter <= 0 )
			{
                INT32 steps = -tone1_vco1_counter / samplerate + 1;
				tone1_vco1_counter += steps * samplerate;
				if( (tone1_vco1_level += steps) >= VMAX*2/3 )
				{
					tone1_vco1_level = VMAX*2/3;
					tone1_vco1_output = 1;
				}
			}
		}
	}
	return tone1_vco1_output;
}

static INT32 tone1_vco2(INT32 samplerate)
{
	/*
	 * L517 (NE555): Ra=570k, Rb=570k, C=10uF
	 * charge time = 0.639*(Ra+Rb)*C = 7.9002s
	 * discharge time = 0.639*Rb*C = 3.9501s
	 */
	#define C20 1.0e-6
	#define R43 510000
	#define R44 510000

	if( tone1_vco2_output )
	{
		if (tone1_vco2_level > VMIN)
		{
			tone1_vco2_counter -= (int)(VMAX*2/3 / (1.639 * R44 * C20));
			if( tone1_vco2_counter <= 0 )
			{
				INT32 steps = -tone1_vco2_counter / samplerate + 1;
				tone1_vco2_counter += steps * samplerate;
				if( (tone1_vco2_level -= steps) <= VMAX*1/3 )
				{
					tone1_vco2_level = VMAX*1/3;
					tone1_vco2_output = 0;
				}
			}
		}
	}
	else
	{
		if (tone1_vco2_level < VMAX)
		{
			tone1_vco2_counter -= (int)(VMAX*2/3 / (1.639 * (R43 + R44) * C20));
			if( tone1_vco2_counter <= 0 )
			{
				INT32 steps = -tone1_vco2_counter / samplerate + 1;
				tone1_vco2_counter += steps * samplerate;
				if( (tone1_vco2_level += steps) >= VMAX*2/3 )
				{
					tone1_vco2_level = VMAX*2/3;
					tone1_vco2_output = 1;
				}
			}
		}
	}

	return tone1_vco2_output;
}

static INT32 tone1_vco(INT32 samplerate, INT32 vco1, INT32 vco2)
{
	INT32 voltage;

	if (tone1_vco_level != tone1_vco_charge)
    {
        /* charge or discharge C22 */
        tone1_vco_counter -= tone1_vco_rate;
        while( tone1_vco_counter <= 0 )
        {
            tone1_vco_counter += samplerate;
            if( tone1_vco_level < tone1_vco_charge )
            {
				if( ++tone1_vco_level == tone1_vco_charge )
                    break;
            }
            else
            {
				if( --tone1_vco_level == tone1_vco_charge )
                    break;
            }
        }
    }

    if( vco2 )
	{
		#define C22 100.0e-6
		#define R42 10000
		#define R45 5100
		#define R46 5100
		#define RP	5048//27777	/* R42+R46 parallel with R45 */
		if( vco1 )
		{
			/*		R42 10k
			 * +5V -/\/\/------------+
			 *			   5V		 |
			 * +5V -/\/\/--+--/\/\/--+--> V/C
			 *	   R45 51k | R46 51k
			 *			  ---
			 *			  --- 100u
			 *			   |
			 *			  0V
			 */
			tone1_vco_charge = VMAX;
			tone1_vco_rate = (int)((tone1_vco_charge - tone1_vco_level) / (RP * C22));
			voltage = tone1_vco_level + (VMAX-tone1_vco_level) * R46 / (R46 + R42);
		}
		else
		{
			/*		R42 10k
			 *	0V -/\/\/------------+
			 *			  2.7V		 |
			 * +5V -/\/\/--+--/\/\/--+--> V/C
			 *	   R45 51k | R46 51k
			 *			  ---
			 *			  --- 100u
			 *			   |
			 *			  0V
             */
			/* simplification: charge = (R42 + R46) / (R42 + R45 + R46); */
            tone1_vco_charge = VMAX * 27 / 50;
			if (tone1_vco_charge >= tone1_vco_level)
				tone1_vco_rate = (int)((tone1_vco_charge - tone1_vco_level) / (R45 * C22));
			else
				tone1_vco_rate = (int)((tone1_vco_level - tone1_vco_charge) / ((R46+R42) * C22));
			voltage = tone1_vco_level * R42 / (R46 + R42);
		}
	}
	else
	{
		if( vco1 )
		{
			/*		R42 10k
			 * +5V -/\/\/------------+
			 *			  2.3V		 |
			 *	0V -/\/\/--+--/\/\/--+--> V/C
			 *	   R45 51k | R46 51k
			 *			  ---
			 *			  --- 100u
			 *			   |
			 *			  0V
			 */
			/* simplification: charge = VMAX * R45 / (R42 + R45 + R46); */
            tone1_vco_charge = VMAX * 23 / 50;
			if (tone1_vco_charge >= tone1_vco_level)
				tone1_vco_rate = (int)((tone1_vco_charge - tone1_vco_level) / ((R42 + R46) * C22));
			else
				tone1_vco_rate = (int)((tone1_vco_level - tone1_vco_charge) / (R45 * C22));
			voltage = tone1_vco_level + (VMAX - tone1_vco_level) * R46 / (R42 + R46);
		}
		else
		{
			/*		R42 10k
			 *	0V -/\/\/------------+
			 *			   0V		 |
			 *	0V -/\/\/--+--/\/\/--+--> V/C
			 *	   R45 51k | R46 51k
			 *			  ---
			 *			  --- 100u
			 *			   |
			 *			  0V
			 */
			tone1_vco_charge = VMIN;
			tone1_vco_rate = (int)((tone1_vco_level - tone1_vco_charge) / (RP * C22));
			voltage = tone1_vco_level * R42 / (R46 + R42);
		}
	}

	/* L507 (NE555): Ra=20k, Rb=20k, C=0.001uF
	 * frequency 1.44/((Ra+2*Rb)*C) = 24kHz
	 */
	return (24000*2/3) + (24000*2/3) * voltage / 32768;
}

static INT32 tone1(INT32 samplerate)
{
	INT32 vco1 = tone1_vco1(samplerate);
	INT32 vco2 = tone1_vco2(samplerate);
	INT32 frequency = tone1_vco(samplerate, vco1, vco2);

	if( (sound_latch_a & 15) != 15 )
	{
		tone1_counter -= frequency;
		while( tone1_counter <= 0 )
		{
			tone1_counter += samplerate;
			if( ++tone1_divisor == 16 )
			{
				tone1_divisor = sound_latch_a & 15;
				tone1_output ^= 1;
			}
		}
	}

	return tone1_output ? tone1_level : -tone1_level;
}

static INT32 tone2_vco(INT32 samplerate)
{
	/*
	 * This is how the tone2 part of the circuit looks like.
	 * I was having a hard time to guesstimate the results
	 * and they might still be wrong :(
	 *
	 *							+12V
	 *							 |
	 *							 / R23
	 *							 \ 100k
	 *							 /
	 * !bit4	| /|	R22 	 |
	 * 0V/5V >--|< |---/\/\/-----+-------+---> V C7
	 *			| \|	47k 	 |		 |
	 *			D4				 |		_|_
	 *					   6.8u --- 	\ / D5
	 *					   C7	--- 	---
	 *							 |		 |
	 *							 |		 |
	 *	  0V >-------------------+-/\/\/-+
	 *							 R24 33k
	 *
	 * V C7 min:
	 * 0.7V + (12V - 0.7V) * 19388 / (100000 + 19388) = 2.54V
	 * V C7 max:
	 * 0.7V + (12V - 0.7V) * 33000 / (100000 + 33000) +
	 *		  (12V - 5.7V) * 47000 / (100000 + 47000) = 5.51V
     */
	// 0.7 + (12 - 0.7) * 33000 / (100000 + 33000)  = 3.50 + 0.75 =
	// 0.7 + (12 - 0.7) * 470 / (100000 + 470) = 0.75
    #define C7  (6.8e-6)
    #define R23 100000
	#define R22 470
	#define R24 33000
	#define R22pR24 24812 // actually r23 parallel w/24 -dink

	#define C7_MIN (VMAX * 075 / 551)
	#define C7_MAX (VMAX * 425 / 551)
	#define C7_DIFF (C7_MAX - C7_MIN)

	if( (sound_latch_b & 0x10) == 0 )
	{
		tone2_vco_counter -= (C7_MAX - tone2_vco_level) * 12 / (R23 * C7) / 5;
		if( tone2_vco_counter <= 0 )
		{
			INT32 n = (-tone2_vco_counter / samplerate) + 1;
			tone2_vco_counter += n * samplerate;
			if( (tone2_vco_level += n) > C7_MAX)
				tone2_vco_level = C7_MAX;
		}
	}
	else
	{
		tone2_vco_counter -= (tone2_vco_level - C7_MIN) * 12 / (R22pR24 * C7) / 5;
		if( tone2_vco_counter <= 0 )
		{
			INT32 n = (-tone2_vco_counter / samplerate) + 1;
			tone2_vco_counter += n * samplerate;
			if( (tone2_vco_level -= n) < C7_MIN)
				tone2_vco_level = C7_MIN;
		}
	}
	/*
	 * L487 (NE555):
	 * Ra = R25 (47k), Rb = R26 (47k), C = C8 (0.001uF)
	 * frequency 1.44/((Ra+2*Rb)*C) = 10212 Hz
	 */
	return (10234 + (0x27*100)) * tone2_vco_level / 32768;
}

static INT32 tone2_pre(INT32 samplerate)
{
	INT32 frequency = tone2_vco(samplerate);

	if( (sound_latch_b & 15) != 15 )
	{
		tone2_counter -= frequency;
		while( tone2_counter <= 0 )
		{
			tone2_counter += samplerate;
			if( ++tone2_divisor == 16 )
			{
				tone2_divisor = sound_latch_b & 15;
				tone2_output ^= 1;
			}
		}
	}
	return tone2_output ? tone2_level : -tone2_level;
}

static INT16 tone2_rc_filt(INT16 sam)
{
	double filt = 0.0;
	double cap = 0.0;

	if (filt == 0.0) {
		filt = 1.0 - exp(-1.0 / ((1.0/(1.0/RES_K(10) + 1.0/RES_K(100))) * CAP_U(.047) * nBurnSoundRate));
		cap = 0.0;
	}

	cap += (sam - cap) * filt;
	return cap * 7.00;
}

static INT32 tone2(INT32 samplerate)
{
	return (sound_latch_b & 0x20) ? tone2_rc_filt(tone2_pre(samplerate)) : tone2_pre(samplerate);
}

static INT32 update_c24(INT32 samplerate)
{
	/*
	 * Noise frequency control (Port B):
	 * Bit 6 lo charges C24 (6.8u) via R51 (330) and when
	 * bit 6 is hi, C24 is discharged through R52 (20k)
	 * in approx. 20000 * 6.8e-6 = 0.136 seconds
	 */
	#define C24 6.8e-6
	#define R49 1000
    #define R51 330
	#define R52 20000
	if( sound_latch_a & 0x40 )
	{
		if (c24_level > VMIN)
		{
			c24_counter -= (int)((c24_level - VMIN) / (R52 * C24));
			if( c24_counter <= 0 )
			{
				INT32 n = -c24_counter / samplerate + 1;
				c24_counter += n * samplerate;
				if( (c24_level -= n) < VMIN)
					c24_level = VMIN;
			}
		}
    }
	else
	{
		if (c24_level < VMAX)
		{
			c24_counter -= (int)((VMAX - c24_level) / ((R51+R49) * C24));
			if( c24_counter <= 0 )
			{
				INT32 n = -c24_counter / samplerate + 1;
				c24_counter += n * samplerate;
				if( (c24_level += n) > VMAX)
					c24_level = VMAX;
			}
		}
    }
	return VMAX - c24_level;
}

static INT32 update_c25(INT32 samplerate)
{
	/*
	 * Bit 7 hi charges C25 (6.8u) over a R50 (1k) and R53 (330) and when
	 * bit 7 is lo, C25 is discharged through R54 (47k)
	 * in about 47000 * 6.8e-6 = 0.3196 seconds
	 */
	#define C25 6.8e-6
	#define R50 1000
    #define R53 330
	#define R54 47000

	if( sound_latch_a & 0x80 )
	{
		if (c25_level < VMAX)
		{
			c25_counter -= (int)((VMAX - c25_level) / ((R50+R53) * C25));
			if( c25_counter <= 0 )
			{
				INT32 n = -c25_counter / samplerate + 1;
				c25_counter += n * samplerate;
				if( (c25_level += n) > VMAX )
					c25_level = VMAX;
			}
		}
	}
	else
	{
		if (c25_level > VMIN)
		{
			c25_counter -= (int)((c25_level - VMIN) / (R54 * C25));
			if( c25_counter <= 0 )
			{
				INT32 n = -c25_counter / samplerate + 1;
				c25_counter += n * samplerate;
				if( (c25_level -= n) < VMIN )
					c25_level = VMIN;
			}
		}
	}
	return c25_level;
}

static INT32 noise(INT32 samplerate)
{
	INT32 vc24 = update_c24(samplerate);
	INT32 vc25 = update_c25(samplerate);
	INT32 sum = 0, level, frequency;

	/*
	 * The voltage levels are added and control I(CE) of transistor TR1
	 * (NPN) which then controls the noise clock frequency (linearily?).
	 * level = voltage at the output of the op-amp controlling the noise rate.
	 */
	if( vc24 < vc25 )
		level = vc24 + (vc25 - vc24) / 2;
	else
		level = vc25 + (vc24 - vc25) / 2;

	frequency = 588 + 6325 * level / 32768;

    /*
	 * NE555: Ra=47k, Rb=1k, C=0.05uF
	 * minfreq = 1.44 / ((47000+2*1000) * 0.05e-6) = approx. 588 Hz
	 * R71 (2700 Ohms) parallel to R73 (47k Ohms) = approx. 2553 Ohms
	 * maxfreq = 1.44 / ((2553+2*1000) * 0.05e-6) = approx. 6325 Hz
	 */
	noise_counter -= frequency;
	if( noise_counter <= 0 )
	{
		INT32 n = (-noise_counter / samplerate) + 1;
		noise_counter += n * samplerate;
		noise_polyoffs = (noise_polyoffs + n) & 0x3ffff;
		noise_polybit = (poly18[noise_polyoffs>>5] >> (noise_polyoffs & 31)) & 1;
	}
	if (!noise_polybit)
		sum += vc24;

	/* 400Hz crude low pass filter: this is only a guess!! */
	noise_lowpass_counter -= 400;
	if( noise_lowpass_counter <= 0 )
	{
		noise_lowpass_counter += samplerate;
		noise_lowpass_polybit = noise_polybit;
	}
	if (!noise_lowpass_polybit)
		sum += vc25;

	return sum;
}

void phoenix_sound_update(INT16 *buffer, INT32 length)
{
	INT32 samplerate = nBurnSoundRate;

	INT16 *buffer2 = buffer;
	INT32 length2 = length;

	memset(buffer, 0, length * 2 * 2);

	while( length-- > 0 )
	{
		INT32 sum = (tone1(samplerate) + tone2(samplerate) + noise(samplerate)) / 4;
		INT16 sam = BURN_SND_CLIP(sum * 0.60);
		*buffer++ = sam; //r
		*buffer++ = sam; //l
	}

	tms36xx_sound_update(buffer2, length2);
}

void phoenix_sound_control_a_w(INT32 address, UINT8 data)
{
	if( data == sound_latch_a )
		return;

	sound_latch_a = data;

	tone1_vco1_cap = (sound_latch_a >> 4) & 3;

	if( sound_latch_a & 0x20 )
		tone1_level = VMAX * 10000 / (10000+10000);
	else
		tone1_level = VMAX;
}

void phoenix_sound_control_b_w(INT32 address, UINT8 data)
{
	if( data == sound_latch_b )
		return;

	sound_latch_b = data;

//	if( sound_latch_b & 0x20 )
//		tone2_level = VMAX;// * 10 / 11;
//	else
	tone2_level = VMAX; // attenuated by rc.filter -dink

	/* eventually change the tune that the MM6221AA is playing */
	mm6221aa_tune_w(sound_latch_b >> 6);
}

void phoenix_sound_reset()
{
	sound_latch_a = sound_latch_b = 0;
	tone1_vco1_cap = 0;
	tone1_level = VMAX;
	tone2_level = VMAX;

	tone1_vco1_output = tone1_vco1_counter = tone1_vco1_level = 0;
	tone1_vco2_output = tone1_vco2_counter = tone1_vco2_level = 0;
	tone1_vco_counter = tone1_vco_level = tone1_vco_rate = tone1_vco_charge = 0;
	tone1_counter = tone1_divisor = tone1_output = 0;
	tone2_vco_counter = tone2_vco_level = 0;
	tone2_counter = tone2_divisor = tone2_output = 0;
	c24_counter = c24_level = 0;
	c25_counter = c25_level = 0;
	noise_counter = noise_polyoffs = noise_polybit = noise_lowpass_counter = noise_lowpass_polybit = 0;

	tms36xx_reset();
}

void phoenix_sound_init()
{
	INT32 i, j;
	UINT32 shiftreg;

	poly18 = (UINT32 *)BurnMalloc((1ul << (18-5)) * sizeof(UINT32));

	if( !poly18 )
		return;

	shiftreg = 0;
	for( i = 0; i < (1ul << (18-5)); i++ )
	{
		UINT32 bits = 0;
		for( j = 0; j < 32; j++ )
		{
			bits = (bits >> 1) | (shiftreg << 31);
			if( ((shiftreg >> 16) & 1) == ((shiftreg >> 17) & 1) )
				shiftreg = (shiftreg << 1) | 1;
			else
				shiftreg <<= 1;
		}
		poly18[i] = bits;
	}

	double decays[6] = {0.50,0,0,1.05,0,0};
	tms36xx_init(372, MM6221AA, &decays[0], 0.21);

	phoenix_sound_reset();

	phoenixsnd_initted = 1;
}

void phoenix_sound_deinit()
{
	if (!phoenixsnd_initted) return;

	phoenixsnd_initted = 0;

	BurnFree(poly18);

	tms36xx_deinit();
}
