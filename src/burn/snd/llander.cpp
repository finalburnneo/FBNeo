/***************************************************************************

  Lunar Lander Specific Sound Code

***************************************************************************/

#include <math.h>
#include "burnint.h"
#include "llander.h"

/* Variables for the lander custom sound support */

#define MIN_SLICE 10
#define LANDER_OVERSAMPLE_RATE  768000

#define AUDIO_CONV16(A) ((A)-0x8000)

static INT32 sinetable[64]=
{
	128,140,153,165,177,188,199,209,218,226,234,240,245,250,253,254,
	255,254,253,250,245,240,234,226,218,209,199,188,177,165,153,140,
	128,116,103, 91, 79, 68, 57, 47, 38, 30, 22, 16, 11,  6,  3,  2,
	1  ,2  ,3  ,  6, 11, 16, 22, 30, 38, 47, 57, 68, 79, 91,103,116
};

static INT32 llander_volume[8]={0x00,0x20,0x40,0x60,0x80,0xa0,0xc0,0xff};
static INT32 buffer_len;
static INT32 emulation_rate;
static INT32 multiplier;
static INT32 lfsr_index;
static UINT16 *lfsr_buffer = NULL;

static INT32 volume;
static INT32 tone_6khz;
static INT32 tone_3khz;
static INT32 llander_explosion;

void llander_sound_init()
{
	INT32 loop,lfsrtmp,nor1,nor2,bit14,bit6;
	INT32 fraction,remainder;

	/* Initialise the simple vars */

	volume=0;
	tone_3khz=0;
	tone_6khz=0;
	llander_explosion=0;

	buffer_len = nBurnSoundLen;
	emulation_rate = nBurnSoundRate;

	/* Calculate the multipler to convert output sample number to the oversample rate (768khz) number */
	/* multipler is held as a fixed point number 16:16                                                */

	multiplier=LANDER_OVERSAMPLE_RATE/(long)emulation_rate;
	remainder=multiplier*LANDER_OVERSAMPLE_RATE;
	fraction=remainder<<16;
	fraction/=emulation_rate;

	multiplier=(multiplier<<16)+fraction;

//	logerror("LANDER: Multiplier=%lx remainder=%lx fraction=%lx rate=%x\n",multiplier,remainder,fraction,emulation_rate);

	/* Generate the LFSR lookup table for the lander white noise generator */

	lfsr_index=0;
	lfsr_buffer = (UINT16*)BurnMalloc(65536*sizeof(UINT16));

	for(loop=0;loop<65536;loop++)
	{
		/* Calc next LFSR value from current value */

		lfsrtmp=(short)loop<<1;

		bit14=(loop&0x04000)?1:0;
		bit6=(loop&0x0040)?1:0;

		nor1=(!( bit14 &&  bit6 ) )?0:1;			/* Note the inversion for the NOR gate */
		nor2=(!(!bit14 && !bit6 ) )?0:1;
		lfsrtmp|=nor1|nor2;

		lfsr_buffer[loop]=lfsrtmp;

//		logerror("LFSR Buffer: %04x    Next=%04x\n",loop, lfsr_buffer[loop]);
	}

	/* Allocate channel and buffer */

//	sample_buffer = (INT16*)BurnMalloc(sizeof(INT16)*buffer_len);
//	memset(sample_buffer,0,sizeof(INT16)*buffer_len);
}

void llander_sound_exit()
{
	BurnFree(lfsr_buffer);
  //  BurnFree(sample_buffer);

}


/***************************************************************************

  Sample Generation code.

  Lander has 4 sound sources: 3khz, 6khz, thrust, explosion

  As the filtering removes a lot of the signal amplitute on thrust and
  explosion paths the gain is partitioned unequally:

  3khz (tone)             Gain 1
  6khz (tone)             Gain 1
  thrust (12khz noise)    Gain 2
  explosion (12khz noise) Gain 4

  After combining the sources the output is scaled accordingly. (Div 8)

  Sound generation is done by oversampling (x64) of the signal to remove the
  need to interpolate between samples. It gives the closest point of the
  sample point to the real signal, there is a small error between the two, the
  higher the oversample rate the lower the error.

                                         oversample_rate
     oversample_number = sample_number * ---------------
                                         sample_rate

     e.g for sample rate=44100 hz and oversample_rate=768000 hz

     oversample_number = sample_number * 17.41487

  The calculations are all done in fixed point 16.16 format. The oversample is
  mapped to the sinewave in the following manner

     e.g for 3khz (12khz / 4)

     sine point = ( oversample_number / 4 ) & 0b00111111

     this coverts the oversample down to 3khz x 64 then wraps the buffer mod
     64 to give the sample point.

  The oversample rate chosen in lander is 12khz * 64 = 768khz as 12khz is a
  binary multiple of the all the frequencies involved.

  The noise generation is done by linear feedback shift register which I've
  modelled with an array, the array value at the current index points to the
  next index to be used, the table is precalulated at startup.

  The output of noise is taken evertime we cross a 12khz boundary, the code
  then sets a target value (noisetarg), we then scan the gap between the last
  oversample point and the current oversample point at the oversample rate
  using the following algorithm:

    noisecurrent = noisecurrent + (noisetarg-noisecurrent) * small_value

    (currently small value = 1/256)

  again this is done in fixed point 16.16 and results in the smoothing of the
  output waveform which reduces the high frequency noise. It also reduces the
  overall amplitude swing of the output, hence the gain partitioning.

  You could probably argue that the oversample rate could be dropped without
  any loss in quality and would recude the cpu load, but lander is hardly a cpu
  hog.

  The outputs from all of the above are then added and scaled up/down to a single
  sample

  K.Wilkins 13/5/98

***************************************************************************/

void llander_sound_update(INT16 *buffer, INT32 n)
{
	static INT32 sampnum=0;
	static INT32 noisetarg=0,noisecurrent=0;
	static INT32 lastoversampnum=0;
	INT32 loop,sample;
	INT32 oversampnum,loop2;

	for(loop=0;loop<n;loop++)
	{
		oversampnum=(long)(sampnum*multiplier)>>16;

//		logerror("LANDER: sampnum=%x oversampnum=%lx\n",sampnum, oversampnum);

		/* Pick up new noise target value whenever 12khz changes */

		if(lastoversampnum>>6!=oversampnum>>6)
		{
			lfsr_index=lfsr_buffer[lfsr_index];
			noisetarg=(lfsr_buffer[lfsr_index]&0x4000)?llander_volume[volume]:0x00;
			noisetarg<<=16;
		}

		/* Do tracking of noisetarg to noise current done in fixed point 16:16    */
		/* each step takes us 1/256 of the difference between desired and current */

		for(loop2=lastoversampnum;loop2<oversampnum;loop2++)
		{
			noisecurrent+=(noisetarg-noisecurrent)>>7;	/* Equiv of multiply by 1/256 */
		}

		sample=(int)(noisecurrent>>16);
		sample<<=1;	/* Gain = 2 */

		if(tone_3khz)
		{
			sample+=sinetable[(oversampnum>>2)&0x3f];
		}
		if(tone_6khz)
		{
			sample+=sinetable[(oversampnum>>1)&0x3f];
		}
		if(llander_explosion)
		{
			sample+=(int)(noisecurrent>>(16-2));	/* Gain of 4 */
		}

		/* Scale ouput down to buffer */
		INT16 psample = AUDIO_CONV16(sample<<5);
		buffer[loop*2+0] = psample;
		buffer[loop*2+1] = psample;

		sampnum++;
		lastoversampnum=oversampnum;
	}
}

void llander_sound_lfsr_reset()
{
	lfsr_index=0;
}

void llander_sound_reset()
{
	volume = 0;
	tone_3khz = 0;
	tone_6khz = 0;
	llander_explosion = 0;
}

void llander_sound_write(UINT8 data)
{
	/* Lunar Lander sound breakdown */  // get funky!

	volume    = data & 0x07;
	tone_3khz = data & 0x10;
	tone_6khz = data & 0x20;
	llander_explosion = data & 0x08;
}

