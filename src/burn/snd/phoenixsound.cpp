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

static int sound_latch_a;
static int sound_latch_b;

static int tone1_vco1_cap;
static int tone1_level;
static int tone2_level;

static UINT32 *poly18 = NULL;

static INT32 phoenixsnd_initted = 0;

static int tone1_vco1(int samplerate)
{
    static int output, counter, level;
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
    static int rate[2][4] = {
		{
			(int)(double)(VMAX*2/3/(0.693*(R40+R41)*C18a)),
			(int)(double)(VMAX*2/3/(0.693*(R40+R41)*C18b)),
			(int)(double)(VMAX*2/3/(0.693*(R40+R41)*C18c)),
			(int)(double)(VMAX*2/3/(0.693*(R40+R41)*C18d))
		},
		{
			(int)(double)(VMAX*2/3/(0.693*R41*C18a)),
			(int)(double)(VMAX*2/3/(0.693*R41*C18b)),
			(int)(double)(VMAX*2/3/(0.693*R41*C18c)),
			(int)(double)(VMAX*2/3/(0.693*R41*C18d))
        }
	};
	if( output )
	{
		if (level > VMAX*1/3)
		{
			counter -= rate[1][tone1_vco1_cap];
			if( counter <= 0 )
			{
				int steps = -counter / samplerate + 1;
				counter += steps * samplerate;
				if( (level -= steps) <= VMAX*1/3 )
				{
					level = VMAX*1/3;
                    output = 0;
				}
			}
		}
	}
	else
	{
		if (level < VMAX*2/3)
		{
			counter -= rate[0][tone1_vco1_cap];
			if( counter <= 0 )
			{
                int steps = -counter / samplerate + 1;
				counter += steps * samplerate;
				if( (level += steps) >= VMAX*2/3 )
				{
					level = VMAX*2/3;
					output = 1;
				}
			}
		}
	}
	return output;
}

static int tone1_vco2(int samplerate)
{
	static int output, counter, level;

	/*
	 * L517 (NE555): Ra=570k, Rb=570k, C=10uF
	 * charge time = 0.639*(Ra+Rb)*C = 7.9002s
	 * discharge time = 0.639*Rb*C = 3.9501s
	 */
	#define C20 10.0e-6
	#define R43 570000
	#define R44 570000

	if( output )
	{
		if (level > VMIN)
		{
			counter -= (int)(VMAX*2/3 / (0.693 * R44 * C20));
			if( counter <= 0 )
			{
				int steps = -counter / samplerate + 1;
				counter += steps * samplerate;
				if( (level -= steps) <= VMAX*1/3 )
				{
					level = VMAX*1/3;
					output = 0;
				}
			}
		}
	}
	else
	{
		if (level < VMAX)
		{
			counter -= (int)(VMAX*2/3 / (0.693 * (R43 + R44) * C20));
			if( counter <= 0 )
			{
				int steps = -counter / samplerate + 1;
				counter += steps * samplerate;
				if( (level += steps) >= VMAX*2/3 )
				{
					level = VMAX*2/3;
					output = 1;
				}
			}
		}
	}

	return output;
}

static int tone1_vco(int samplerate, int vco1, int vco2)
{
	static int counter, level, rate, charge;
	int voltage;

	if (level != charge)
    {
        /* charge or discharge C22 */
        counter -= rate;
        while( counter <= 0 )
        {
            counter += samplerate;
            if( level < charge )
            {
				if( ++level == charge )
                    break;
            }
            else
            {
				if( --level == charge )
                    break;
            }
        }
    }

    if( vco2 )
	{
		#define C22 100.0e-6
		#define R42 10000
		#define R45 51000
		#define R46 51000
		#define RP	27777	/* R42+R46 parallel with R45 */
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
			charge = VMAX;
			rate = (int)((charge - level) / (RP * C22));
			voltage = level + (VMAX-level) * R46 / (R46 + R42);
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
            charge = VMAX * 27 / 50;
			if (charge >= level)
				rate = (int)((charge - level) / (R45 * C22));
			else
				rate = (int)((level - charge) / ((R46+R42) * C22));
			voltage = level * R42 / (R46 + R42);
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
            charge = VMAX * 23 / 50;
			if (charge >= level)
				rate = (int)((charge - level) / ((R42 + R46) * C22));
			else
				rate = (int)((level - charge) / (R45 * C22));
			voltage = level + (VMAX - level) * R46 / (R42 + R46);
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
			charge = VMIN;
			rate = (int)((level - charge) / (RP * C22));
			voltage = level * R42 / (R46 + R42);
		}
	}

	/* L507 (NE555): Ra=20k, Rb=20k, C=0.001uF
	 * frequency 1.44/((Ra+2*Rb)*C) = 24kHz
	 */
	return 24000*1/3 + 24000*2/3 * voltage / 32768;
}

static int tone1(int samplerate)
{
	static int counter, divisor, output;
	int vco1 = tone1_vco1(samplerate);
	int vco2 = tone1_vco2(samplerate);
	int frequency = tone1_vco(samplerate, vco1, vco2);

	if( (sound_latch_a & 15) != 15 )
	{
		counter -= frequency;
		while( counter <= 0 )
		{
			counter += samplerate;
			if( ++divisor == 16 )
			{
				divisor = sound_latch_a & 15;
				output ^= 1;
			}
		}
	}

	return output ? tone1_level : -tone1_level;
}

static int tone2_vco(int samplerate)
{
	static int counter, level;

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

    #define C7  6.8e-6
    #define R23 100000
	#define R22 47000
	#define R24 33000
	#define R22pR24 19388

	#define C7_MIN (VMAX * 254 / 500)
	#define C7_MAX (VMAX * 551 / 500)
	#define C7_DIFF (C7_MAX - C7_MIN)

	if( (sound_latch_b & 0x10) == 0 )
	{
		counter -= (C7_MAX - level) * 12 / (R23 * C7) / 5;
		if( counter <= 0 )
		{
			int n = (-counter / samplerate) + 1;
			counter += n * samplerate;
			if( (level += n) > C7_MAX)
				level = C7_MAX;
		}
	}
	else
	{
		counter -= (level - C7_MIN) * 12 / (R22pR24 * C7) / 5;
		if( counter <= 0 )
		{
			int n = (-counter / samplerate) + 1;
			counter += n * samplerate;
			if( (level -= n) < C7_MIN)
				level = C7_MIN;
		}
	}
	/*
	 * L487 (NE555):
	 * Ra = R25 (47k), Rb = R26 (47k), C = C8 (0.001uF)
	 * frequency 1.44/((Ra+2*Rb)*C) = 10212 Hz
	 */
	return 10212 * level / 32768;
}

static int tone2(int samplerate)
{
	static int counter, divisor, output;
	int frequency = tone2_vco(samplerate);

	if( (sound_latch_b & 15) != 15 )
	{
		counter -= frequency;
		while( counter <= 0 )
		{
			counter += samplerate;
			if( ++divisor == 16 )
			{
				divisor = sound_latch_b & 15;
				output ^= 1;
			}
		}
	}
	return output ? tone2_level : -tone2_level;
}

static int update_c24(int samplerate)
{
	static int counter, level;
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
		if (level > VMIN)
		{
			counter -= (int)((level - VMIN) / (R52 * C24));
			if( counter <= 0 )
			{
				int n = -counter / samplerate + 1;
				counter += n * samplerate;
				if( (level -= n) < VMIN)
					level = VMIN;
			}
		}
    }
	else
	{
		if (level < VMAX)
		{
			counter -= (int)((VMAX - level) / ((R51+R49) * C24));
			if( counter <= 0 )
			{
				int n = -counter / samplerate + 1;
				counter += n * samplerate;
				if( (level += n) > VMAX)
					level = VMAX;
			}
		}
    }
	return VMAX - level;
}

static int update_c25(int samplerate)
{
	static int counter, level;
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
		if (level < VMAX)
		{
			counter -= (int)((VMAX - level) / ((R50+R53) * C25));
			if( counter <= 0 )
			{
				int n = -counter / samplerate + 1;
				counter += n * samplerate;
				if( (level += n) > VMAX )
					level = VMAX;
			}
		}
	}
	else
	{
		if (level > VMIN)
		{
			counter -= (int)((level - VMIN) / (R54 * C25));
			if( counter <= 0 )
			{
				int n = -counter / samplerate + 1;
				counter += n * samplerate;
				if( (level -= n) < VMIN )
					level = VMIN;
			}
		}
	}
	return level;
}


static int noise(int samplerate)
{
	static int counter, polyoffs, polybit, lowpass_counter, lowpass_polybit;
	int vc24 = update_c24(samplerate);
	int vc25 = update_c25(samplerate);
	int sum = 0, level, frequency;

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
	counter -= frequency;
	if( counter <= 0 )
	{
		int n = (-counter / samplerate) + 1;
		counter += n * samplerate;
		polyoffs = (polyoffs + n) & 0x3ffff;
		polybit = (poly18[polyoffs>>5] >> (polyoffs & 31)) & 1;
	}
	if (!polybit)
		sum += vc24;

	/* 400Hz crude low pass filter: this is only a guess!! */
	lowpass_counter -= 400;
	if( lowpass_counter <= 0 )
	{
		lowpass_counter += samplerate;
		lowpass_polybit = polybit;
	}
	if (!lowpass_polybit)
		sum += vc25;

	return sum;
}

void phoenix_sound_update(INT16 *buffer, int length)
{
	int samplerate = nBurnSoundRate;

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

	if( sound_latch_b & 0x20 )
		tone2_level = VMAX * 10 / 11;
	else
		tone2_level = VMAX;

	/* eventually change the tune that the MM6221AA is playing */
	mm6221aa_tune_w(sound_latch_b >> 6);
}

void phoenix_sound_reset()
{
	sound_latch_a = sound_latch_b = 0;
	tone1_vco1_cap = 0;
	tone1_level = VMAX;
	tone2_level = VMAX;
	tms36xx_reset();
}

void phoenix_sound_init()
{
	int i, j;
	UINT32 shiftreg;

	poly18 = (UINT32 *)malloc((1ul << (18-5)) * sizeof(UINT32));

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

	if (poly18) {
		free(poly18);
		poly18 = NULL;
	}

	tms36xx_deinit();
}
