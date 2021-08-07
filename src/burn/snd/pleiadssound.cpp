/****************************************************************************
 *
 * Sound hardware for Pleiades, Naughty Boy and Pop Flamer.
 *
 * If you find errors or have suggestions, please mail me.
 * Juergen Buchmueller <pullmoll@t-online.de>
 *
 ****************************************************************************/
#include "burnint.h"
#include "tms36xx.h"

#define VMIN	0
#define VMAX	32767

//static INT32 channel;

static INT32 sound_latch_a;
static INT32 sound_latch_b;
static INT32 sound_latch_c;	/* part of the videoreg_w latch */

static UINT32 *poly18 = NULL;
static INT32 polybit;
static double custom_volume = 1.00;

static INT32 pleiadssound_initted = 0;

/* fixed 8kHz clock */
#define TONE1_CLOCK  8000

/* some resistor and capacitor dependent values which
   vary between the (otherwise identical) boards. */
static double pa5_charge_time;
static double pa5_discharge_time;

static double pa6_charge_time;
static double pa6_discharge_time;

static double pb4_charge_time;
static double pb4_discharge_time;

static double pc4_charge_time;
static double pc4_discharge_time;

static double pc5_charge_time;
static double pc5_discharge_time;

static INT32 pa5_resistor;
static INT32 pc5_resistor;

static INT32 tone2_max_freq;
static INT32 tone3_max_freq;
static INT32 tone4_max_freq;
static INT32 noise_freq;
static INT32 polybit_resistor;
static INT32 opamp_resistor;

static INT32 tone1_counter, tone1_divisor, tone1_output;
static INT32 pb4_counter, pb4_level;
static INT32 tone23_counter2, tone23_output2, tone23_counter3, tone23_output3;
#define PC4_MIN (int)(VMAX * 7 / 50)
static INT32 pc4_counter, pc4_level;
static INT32 pc5_counter, pc5_level;
static INT32 pa5_counter, pa5_level;
static INT32 tone4_counter, tone4_output;
static INT32 pa6_counter, pa6_level;
static INT32 noise_counter, noise_polyoffs;

void pleiads_sound_scan(INT32 nAction, INT32 *pnMin)
{
	tms36xx_scan(nAction, pnMin);

	SCAN_VAR(sound_latch_a);
	SCAN_VAR(sound_latch_b);
	SCAN_VAR(sound_latch_c);

	SCAN_VAR(tone1_counter);
	SCAN_VAR(tone1_divisor);
	SCAN_VAR(tone1_output);
	SCAN_VAR(pb4_counter);
	SCAN_VAR(pb4_level);
	SCAN_VAR(tone23_counter2);
	SCAN_VAR(tone23_output2);
	SCAN_VAR(tone23_counter3);
	SCAN_VAR(tone23_output3);
	SCAN_VAR(pc4_counter);
	SCAN_VAR(pc4_level);
	SCAN_VAR(pc5_counter);
	SCAN_VAR(pc5_level);
	SCAN_VAR(pa5_counter);
	SCAN_VAR(pa5_level);
	SCAN_VAR(tone4_counter);
	SCAN_VAR(tone4_output);
	SCAN_VAR(pa6_counter);
	SCAN_VAR(pa6_level);
	SCAN_VAR(noise_counter);
	SCAN_VAR(noise_polyoffs);
}

/*****************************************************************************
 * Tone #1 is a fixed 8 kHz signal divided by 1 to 15.
 *****************************************************************************/
static INT32 tone1(INT32 samplerate)
{
	if( (sound_latch_a & 15) != 15 )
	{
		tone1_counter -= TONE1_CLOCK;
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
	return tone1_output ? VMAX : -VMAX;
}

/*****************************************************************************
 * Tones #2 and #3 are coming from the upper 556 chip
 * It's labelled IC96 in Pop Flamer, 4D(??) in Naughty Boy.
 * C68 controls the frequencies of tones #2 and #3 (V/C inputs)
 *****************************************************************************/
static INT32 update_pb4(INT32 samplerate)
{
	/* bit 4 of latch B: charge 10uF (C28/C68) through 10k (R19/R25) */
	if( sound_latch_b & 0x10 )
	{
		if( pb4_level < VMAX )
		{
			pb4_counter -= (int)((VMAX - pb4_level) / pb4_charge_time);
			if( pb4_counter <= 0 )
			{
				INT32 n = (-pb4_counter / samplerate) + 1;
				pb4_counter += n * samplerate;
				if( (pb4_level += n) > VMAX )
					pb4_level = VMAX;
			}
		}
	}
	else
	{
		if( pb4_level > VMIN )
		{
			pb4_counter -= (int)((pb4_level - VMIN) / pb4_discharge_time);
			if( pb4_counter <= 0 )
			{
				INT32 n = (-pb4_counter / samplerate) + 1;
				pb4_counter += n * samplerate;
				if( (pb4_level -= n) < VMIN)
					pb4_level = VMIN;
			}
		}
	}
	return pb4_level;
}

static INT32 tone23(INT32 samplerate)
{
	INT32 level = VMAX - update_pb4(samplerate);
	INT32 sum = 0;

	/* bit 5 = low: tone23 disabled */
	if( (sound_latch_b & 0x20) == 0 )
		return sum;

    /* modulate timers from the upper 556 with the voltage on Cxx on PB4. */
	if( level < VMAX )
	{
		tone23_counter2 -= tone2_max_freq * level / 32768;
		if( tone23_counter2 <= 0 )
		{
			INT32 n = (-tone23_counter2 / samplerate) + 1;
			tone23_counter2 += n * samplerate;
			tone23_output2 = (tone23_output2 + n) & 1;
		}

		tone23_counter3 -= tone3_max_freq*1/3 + tone3_max_freq*2/3 * level / 33768;
		if( tone23_counter3 <= 0 )
		{
			INT32 n = (-tone23_counter2 / samplerate) + 1;
			tone23_counter3 += samplerate;
			tone23_output3 = (tone23_output3 + n) & 1;
		}
	}

	sum += (tone23_output2) ? VMAX : -VMAX;
	sum += (tone23_output3) ? VMAX : -VMAX;

	return sum / 2;
}

/*****************************************************************************
 * Tone #4 comes from upper half of the lower 556 (IC98 in Pop Flamer)
 * It's modulated by the voltage at C49, which is then divided between
 * 0V or 5V, depending on the polynome output bit.
 * The tone signal gates two signals (bits 5 of latches A and C), but
 * these are also swept between two levels (C52 and C53 in Pop Flamer).
 *****************************************************************************/
static INT32 update_c_pc4(INT32 samplerate)
{
	/* bit 4 of latch C: (part of videoreg_w) hi? */
	if (sound_latch_c & 0x10)
	{
		if (pc4_level < VMAX)
		{
			pc4_counter -= (int)((VMAX - pc4_level) / pc4_charge_time);
			if( pc4_counter <= 0 )
			{
				INT32 n = (-pc4_counter / samplerate) + 1;
				pc4_counter += n * samplerate;
				if( (pc4_level += n) > VMAX )
					pc4_level = VMAX;
			}
		}
	}
	else
	{
		if (pc4_level > PC4_MIN)
		{
			pc4_counter -= (int)((pc4_level - PC4_MIN) / pc4_discharge_time);
			if( pc4_counter <= 0 )
			{
				INT32 n = (-pc4_counter / samplerate) + 1;
				pc4_counter += n * samplerate;
				if( (pc4_level -= n) < PC4_MIN )
					pc4_level = PC4_MIN;
			}
		}
	}
	return pc4_level;
}

static INT32 update_c_pc5(INT32 samplerate)
{
	/* bit 5 of latch C: charge or discharge C52 */
	if (sound_latch_c & 0x20)
	{
		if (pc5_level < VMAX)
		{
			pc5_counter -= (int)((VMAX - pc5_level) / pc5_charge_time);
			if( pc5_counter <= 0 )
			{
				INT32 n = (-pc5_counter / samplerate) + 1;
				pc5_counter += n * samplerate;
				if( (pc5_level += n) > VMAX )
					pc5_level = VMAX;
			}
		}
	}
	else
	{
		if (pc5_level > VMIN)
		{
			pc5_counter -= (int)((pc5_level - VMIN) / pc5_discharge_time);
			if( pc5_counter <= 0 )
			{
				INT32 n = (-pc5_counter / samplerate) + 1;
				pc5_counter += samplerate;
				if( (pc5_level -= n) < VMIN )
					pc5_level = VMIN;
			}
		}
	}
	return pc5_level;
}

static INT32 update_c_pa5(INT32 samplerate)
{
	/* bit 5 of latch A: charge or discharge C63 */
	if (sound_latch_a & 0x20)
	{
		if (pa5_level < VMAX)
		{
			pa5_counter -= (int)((VMAX - pa5_level) / pa5_charge_time);
			if( pa5_counter <= 0 )
			{
				INT32 n = (-pa5_counter / samplerate) + 1;
				pa5_counter += n * samplerate;
				if( (pa5_level += n) > VMAX )
					pa5_level = VMAX;
			}
		}
	}
	else
	{
		if (pa5_level > VMIN)
		{
			pa5_counter -= (int)((pa5_level - VMIN) / pa5_discharge_time);
			if( pa5_counter <= 0 )
			{
				INT32 n = (-pa5_counter / samplerate) + 1;
				pa5_counter += samplerate;
				if( (pa5_level -= n) < VMIN )
					pa5_level = VMIN;
			}
		}
	}
	return pa5_level;
}

static INT32 tone4(INT32 samplerate)
{
	INT32 level = update_c_pc4(samplerate);
	INT32 vpc5 = update_c_pc5(samplerate);
	INT32 vpa5 = update_c_pa5(samplerate);
	INT32 sum;

	/* Two resistors divide the output voltage of the op-amp between
	 * polybit = 0: 0V and level: x * opamp_resistor / (opamp_resistor + polybit_resistor)
	 * polybit = 1: level and 5V: x * polybit_resistor / (opamp_resistor + polybit_resistor)
	 */
	if (polybit)
		level = level + (VMAX - level) * opamp_resistor / (opamp_resistor + polybit_resistor);
	else
		level = level * polybit_resistor / (opamp_resistor + polybit_resistor);

	tone4_counter -= tone4_max_freq * level / 32768;
	if( tone4_counter <= 0 )
	{
		INT32 n = (-tone4_counter / samplerate) + 1;
		tone4_counter += n * samplerate;
		tone4_output = (tone4_output + n) & 1;
	}

	/* mix the two signals */
	sum = vpc5 * pa5_resistor / (pa5_resistor + pc5_resistor) +
		  vpa5 * pc5_resistor / (pa5_resistor + pc5_resistor);

	return (tone4_output) ? sum : -sum;
}

/*****************************************************************************
 * Noise comes from a shift register (4006) hooked up just like in Phoenix.
 * Difference: the clock frequecy is toggled between two values only by
 * bit 4 of latch A. The output of the first shift register can be zapped(?)
 * by some control line (IC87 in Pop Flamer: not yet implemented)
 *****************************************************************************/
static INT32 update_c_pa6(INT32 samplerate)
{
	/* bit 6 of latch A: charge or discharge C63 */
	if (sound_latch_a & 0x40)
	{
		if (pa6_level < VMAX)
		{
			pa6_counter -= (int)((VMAX - pa6_level) / pa6_charge_time);
			if( pa6_counter <= 0 )
			{
				INT32 n = (-pa6_counter / samplerate) + 1;
				pa6_counter += n * samplerate;
				if( (pa6_level += n) > VMAX )
					pa6_level = VMAX;
			}
		}
	}
	else
	{
		/* only discharge of poly bit is active */
		if (polybit && pa6_level > VMIN)
		{
			/* discharge 10uF through 10k -> 0.1s */
			pa6_counter -= (int)((pa6_level - VMIN) / 0.1);
			if( pa6_counter <= 0 )
			{
				INT32 n = (-pa6_counter / samplerate) + 1;
				pa6_counter += n * samplerate;
				if( (pa6_level -= n) < VMIN )
					pa6_level = VMIN;
			}
		}
	}
	return pa6_level;
}

static INT32 noise(INT32 samplerate)
{
	INT32 c_pa6_level = update_c_pa6(samplerate);
	INT32 sum = 0;

	/*
	 * bit 4 of latch A: noise counter rate modulation?
	 * CV2 input of lower 556 is connected via 2k resistor
	 */
	if ( sound_latch_a & 0x10 )
		noise_counter -= noise_freq * 2 / 3; /* ????? */
	else
		noise_counter -= noise_freq * 1 / 3; /* ????? */

	if( noise_counter <= 0 )
	{
		INT32 n = (-noise_counter / samplerate) + 1;
		noise_counter += n * samplerate;
		noise_polyoffs = (noise_polyoffs + n) & 0x3ffff;
		polybit = (poly18[noise_polyoffs>>5] >> (noise_polyoffs & 31)) & 1;
	}

	/* The polynome output bit is used to gate bits 6 + 7 of
	 * sound latch A through the upper half of a 4066 chip.
	 * Bit 6 is sweeping a capacitor between 0V and 4.7V
	 * while bit 7 is connected directly to the 4066.
	 * Both outputs are then filtered, bit 7 even twice,
	 * but it's beyond me what the filters there are doing...
	 */
	if (polybit)
	{
		sum += c_pa6_level;
		/* bit 7 is connected directly */
		if (sound_latch_a & 0x80)
			sum += VMAX;
	}
	else
	{
		sum -= c_pa6_level;
		/* bit 7 is connected directly */
		if (sound_latch_a & 0x80)
			sum -= VMAX;
	}

	return sum / 2;
}

void pleiads_sound_update(INT16 *buffer, INT32 length)
{
	INT32 rate = nBurnSoundRate;

	INT16 *buffer2 = buffer;
	INT32 length2 = length;

	memset(buffer, 0, length * 2 * 2);

	while( length-- > 0 )
	{
		INT32 sum = tone1(rate)/2 + tone23(rate)/2 + tone4(rate) + noise(rate);
		INT16 sam = BURN_SND_CLIP(sum * custom_volume);
		*buffer++ = sam; //r
		*buffer++ = sam; //l
	}

	tms36xx_sound_update(buffer2, length2);
}

void pleiads_sound_control_a_w(INT32 address, UINT8 data)
{
	if (data == sound_latch_a)
		return;

	//logerror("pleiads_sound_control_b_w $%02x\n", data);

	sound_latch_a = data;
}

void pleiads_sound_control_b_w(INT32 address, UINT8 data)
{
	/*
	 * pitch selects one of 4 possible clock inputs
	 * (actually 3, because IC2 and IC3 are tied together)
	 * write note value to TMS3615; voice b1 & b2
	 */
	INT32 note = data & 15;
	INT32 pitch = (data >> 6) & 3;

	if (data == sound_latch_b)
		return;

	//bprintf(0, _T("b_w %x sl %x."), data, sound_latch_b);

	if (note==0) return; // this is a hack added by dink, sound wont work without this. what gives??

	if (pitch == 3)
		pitch = 2;	/* 2 and 3 are the same */

	tms36xx_note_w(pitch, note);

	sound_latch_b = data;
}

/* two bits (4 + 5) from the videoreg_w latch go here */
void pleiads_sound_control_c_w(INT32 address, UINT8 data)
{
	if (data == sound_latch_c)
		return;

	sound_latch_c = data;
}

void pleiads_sound_init(INT32 naughtybpopflamer)
{
	UINT32 shiftreg;

	poly18 = (UINT32 *)BurnMalloc((1ul << (18-5)) * sizeof(UINT32));

	if( !poly18 )
		return;

	shiftreg = 0;
	for(INT32 i = 0; i < (1ul << (18-5)); i++ )
	{
		UINT32 bits = 0;
		for(INT32 j = 0; j < 32; j++ )
		{
			bits = (bits >> 1) | (shiftreg << 31);
			if( ((shiftreg >> 16) & 1) == ((shiftreg >> 17) & 1) )
				shiftreg = (shiftreg << 1) | 1;
			else
				shiftreg <<= 1;
		}
		poly18[i] = bits;
	}

	if (naughtybpopflamer) {
		double decays[6] = {0.15, 0.20, 0, 0, 0, 0};
		tms36xx_init(350, TMS3615, &decays[0], 0.00);
	} else {
		double decays[6] = {0.33,0.33,0,0.33,0,0.33};
		tms36xx_init(247, TMS3615, &decays[0], 0.00);
	}
	pleiadssound_initted = 1;
}

void pleiads_sound_deinit()
{
	if (!pleiadssound_initted) return;

	BurnFree(poly18);

	tms36xx_deinit();
	pleiadssound_initted = 0;
}

static void internal_reset()
{
	sound_latch_a = sound_latch_b = sound_latch_c = 0;

	tone1_counter = tone1_divisor = tone1_output = 0;
	pb4_counter = pb4_level = 0;
	tone23_counter2 = tone23_output2 = tone23_counter3 = tone23_output3 = 0;

	pc4_level = PC4_MIN;

	pc4_counter = pc4_level = 0;
	pc5_counter = pc5_level = 0;
	pa5_counter = pa5_level = 0;
	tone4_counter = tone4_output = 0;
	pa6_counter = pa6_level = 0;
	noise_counter = noise_polyoffs = 0;

	tms36xx_reset();
}

void pleiads_sound_reset()
{
	/* The real values are _unknown_!
	 * I took the ones from Naughty Boy / Pop Flamer
	 */

	/* charge 10u?? (C??) through 330K?? (R??) -> 3.3s */
	pa5_charge_time = 3.3;

	/* discharge 10u?? (C??) through 220k?? (R??) -> 2.2s */
	pa5_discharge_time = 2.2;

	/* charge 2.2uF?? through 330?? -> 0.000726s */
	pa6_charge_time = 0.000726;

	/* discharge 2.2uF?? through 10k?? -> 0.22s */
	pa6_discharge_time = 0.022;

    /* 10k and 10uF */
	pb4_charge_time = 0.1;
	pb4_discharge_time = 0.1;

	/* charge C49 (22u?) via R47 (2k?) and R48 (1k)
	 * time constant (1000+2000) * 22e-6 = 0.066s */
	pc4_charge_time = 0.066;

	/* discharge C49 (22u?) via R48 (1k) and diode D1
	 * time constant 1000 * 22e-6 = 0.022s */
	pc4_discharge_time = 0.022;

	/* charge 10u?? through 330 -> 0.0033s */
	pc5_charge_time = 0.0033;

	/* discharge 10u?? through ??k (R??) -> 0.1s */
	pc5_discharge_time = 0.1;

	/* both in K */
	pa5_resistor = 33;
	pc5_resistor = 47;

	/* upper 556 upper half: Ra=10k??, Rb=200k??, C=0.01uF?? -> 351Hz */
	tone2_max_freq = 351;

	/* upper 556 lower half: Ra=47k??, Rb=100k??, C=0.01uF?? -> 582Hz */
	tone3_max_freq = 582;

	/* lower 556 upper half: Ra=33k??, Rb=100k??, C=0.0047uF??
	   freq = 1.44 / ((33000+2*100000) * 0.0047e-6) = approx. 1315 Hz */
	tone4_max_freq = 1315;

	/* how to divide the V/C voltage for tone #4 */
	polybit_resistor = 47;
	opamp_resistor = 20;

	/* lower 556 lower half: Ra=100k??, Rb=1k??, C=0.01uF??
	  freq = 1.44 / ((100000+2*1000) * 0.01e-6) = approx. 1412 Hz */
	noise_freq = 1412;	/* higher noise rate than popflame/naughtyb??? */

	custom_volume = 0.40;

	internal_reset();
	//return common_sh_start(msound, "Custom (Pleiads)");
}

void naughtyb_sound_reset()
{
	/* charge 10u??? through 330K (R??) -> 3.3s */
	pa5_charge_time = 3.3;

	/* discharge 10u through 220k (R??) -> 2.1s */
	pa5_discharge_time = 2.2;

	/* charge 2.2uF through 330 -> 0.000726s */
	pa6_charge_time = 0.000726;

	/* discharge 2.2uF through 10K -> 0.022s */
	pa6_discharge_time = 0.022;

    /* 10k and 10uF */
	pb4_charge_time = 0.1;
	pb4_discharge_time = 0.1;

	/* charge 10uF? (C??) via 3k?? (R??) and 2k?? (R28?)
	 * time constant (3000+2000) * 10e-6 = 0.05s */
	pc4_charge_time = 0.05 * 10;

	/* discharge 10uF? (C??) via 2k?? R28??  and diode D?
	 * time constant 2000 * 10e-6 = 0.02s */
	pc4_discharge_time = 0.02 * 10;

	/* charge 10u through 330 -> 0.0033s */
	pc5_charge_time = 0.0033;

	/* discharge 10u through ??k (R??) -> 0.1s */
	pc5_discharge_time = 0.1;

	/* both in K */
	pa5_resistor = 100;
	pc5_resistor = 78;

	/* upper 556 upper half: 10k, 200k, 0.01uF -> 351Hz */
	tone2_max_freq = 351;

	/* upper 556 lower half: 47k, 200k, 0.01uF -> 322Hz */
	tone3_max_freq = 322;

	/* lower 556 upper half: Ra=33k, Rb=100k, C=0.0047uF
	   freq = 1.44 / ((33000+2*100000) * 0.0047e-6) = approx. 1315 Hz */
	tone4_max_freq = 1315;

	/* how to divide the V/C voltage for tone #4 */
	polybit_resistor = 47;
	opamp_resistor = 20;

	/* lower 556 lower half: Ra=200k, Rb=1k, C=0.01uF
	  freq = 1.44 / ((200000+2*1000) * 0.01e-6) = approx. 713 Hz */
	noise_freq = 713;

	custom_volume = 0.40;

	internal_reset();
//	return common_sh_start(msound, "Custom (Naughty Boy)");
}

void popflame_sound_reset()
{
	/* charge 10u (C63 in Pop Flamer) through 330K -> 3.3s */
	pa5_charge_time = 3.3;

	/* discharge 10u (C63 in Pop Flamer) through 220k -> 2.2s */
	pa5_discharge_time = 2.2;

	/* charge 2.2uF through 330 -> 0.000726s */
	pa6_charge_time = 0.000726;

	/* discharge 2.2uF through 10K -> 0.022s */
	pa6_discharge_time = 0.022;

    /* 2k and 10uF */
	pb4_charge_time = 0.02;
	pb4_discharge_time = 0.02;

	/* charge 2.2uF (C49?) via R47 (100) and R48 (1k)
	 * time constant (100+1000) * 2.2e-6 = 0.00242 */
	pc4_charge_time = 0.000242;

	/* discharge 2.2uF (C49?) via R48 (1k) and diode D1
	 * time constant 1000 * 22e-6 = 0.0022s */
	pc4_discharge_time = 0.00022;

	/* charge 22u (C52 in Pop Flamer) through 10k -> 0.22s */
	pc5_charge_time = 0.22;

	/* discharge 22u (C52 in Pop Flamer) through ??k (R??) -> 0.1s */
	pc5_discharge_time = 0.1;

	/* both in K */
	pa5_resistor = 33;
	pc5_resistor = 47;

	/* upper 556 upper half: Ra=10k, Rb=100k, C=0.01uF -> 1309Hz */
	tone2_max_freq = 1309;

	/* upper 556 lower half: Ra=10k??, Rb=120k??, C=0.01uF -> 1108Hz */
	tone3_max_freq = 1108;

	/* lower 556 upper half: Ra=33k, Rb=100k, C=0.0047uF
	   freq = 1.44 / ((33000+2*100000) * 0.0047e-6) = approx. 1315 Hz */
	tone4_max_freq = 1315;

	/* how to divide the V/C voltage for tone #4 */
	polybit_resistor = 20;
	opamp_resistor = 20;

	/* lower 556 lower half: Ra=200k, Rb=1k, C=0.01uF
	  freq = 1.44 / ((200000+2*1000) * 0.01e-6) = approx. 713 Hz */
	noise_freq = 713;

	custom_volume = 0.40;

	internal_reset();
}
