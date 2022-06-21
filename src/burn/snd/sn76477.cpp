// Based on MAME sources by Zsolt Vasvari
/*****************************************************************************

    Texas Instruments SN76477 emulator

    authors: Derrick Renaud - info
             Zsolt Vasvari  - software

    (see sn76477.h for details)

    Notes:
        * All formulas were derived by taking measurements of a real device,
          then running the data sets through the numerical analysis
          application at http://zunzun.com to come up with the functions.

    Known issues/to-do's:
        * VCO
            * confirm value of VCO_MAX_EXT_VOLTAGE, VCO_TO_SLF_VOLTAGE_DIFF
              VCO_CAP_VOLTAGE_MIN and VCO_CAP_VOLTAGE_MAX
            * confirm value of VCO_MIN_DUTY_CYCLE
            * get real formulas for VCO cap charging and discharging
            * get real formula for VCO duty cycle
            * what happens if no vco_res
            * what happens if no vco_cap

        * Attack/Decay
            * get real formulas for a/d cap charging and discharging

 *****************************************************************************/

#include "burnint.h"
#include "sn76477.h"

#define VERBOSE 0

#if VERBOSE >= 0
#define LOG(n,x) //if( VERBOSE >= (n) ) logerror x
#else
#define LOG(n,x)
#endif

#define VMIN	0x0000
#define VMAX	0x7fff

static double mastervol;
static INT32 samplerate; 		/* from Machine->sample_rate */
static INT32 sn76477_num = 0;

struct SN76477 {
	INT32 vol;				/* current volume (attack/decay) */
	INT32 vol_count;			/* volume adjustment counter */
	INT32 vol_rate;			/* volume adjustment rate - dervied from attack/decay */
	INT32 vol_step;			/* volume adjustment step */

	double slf_count;		/* SLF emulation */
	double slf_freq;		/* frequency - derived */
	double slf_level;		/* triangular wave level */
    INT32 slf_dir;            /* triangular wave direction */
	INT32 slf_out;			/* rectangular output signal state */

	double vco_count;		/* VCO emulation */
	double vco_freq;		/* frequency - derived */
	double vco_step;		/* modulated frequency - derived */
	INT32 vco_out;			/* rectangular output signal state */

	INT32 noise_count;		/* NOISE emulation */
	INT32 noise_clock;		/* external clock signal */
	INT32 noise_freq; 		/* filter frequency - derived */
	INT32 noise_poly; 		/* polynome */
	INT32 noise_out;			/* rectangular output signal state */

	double envelope_timer;	/* ENVELOPE timer */
	INT32 envelope_state; 	/* attack / decay toggle */

	double attack_time; 	/* ATTACK time (time until vol reaches 100%) */
	double decay_time;		/* DECAY time (time until vol reaches 0%) */
	double oneshot_time;	/* ONE-SHOT time */
	double oneshot_timer;	/* ONE-SHOT timer */

	INT32 envelope;			/* pin	1, pin 28 */
	double noise_res;		/* pin	4 */
	double filter_res;		/* pin	5 */
	double filter_cap;		/* pin	6 */
	double decay_res;		/* pin	7 */
	double attack_decay_cap;/* pin	8 */
	INT32 enable; 			/* pin	9 */
	double attack_res;		/* pin 10 */
	double amplitude_res;	/* pin 11 */
	double feedback_res;	/* pin 12 */
	double vco_voltage; 	/* pin 16 */
	double vco_cap; 		/* pin 17 */
	double vco_res; 		/* pin 18 */
	double pitch_voltage;	/* pin 19 */
	double slf_res; 		/* pin 20 */
	double slf_cap; 		/* pin 21 */
	INT32 vco_select; 		/* pin 22 */
	double oneshot_cap; 	/* pin 23 */
	double oneshot_res; 	/* pin 24 */
	INT32 mixer;				/* pin 25,26,27 */

	INT16 vol_lookup[VMAX+1-VMIN];	/* volume lookup table */
};

//static struct SN76477interface *intf;
struct SN76477 *sn76477[MAX_SN76477] = {NULL, NULL, NULL, NULL};

static void attack_decay(INT32 param)
{
	struct SN76477 *sn = sn76477[param];
	sn->envelope_state ^= 1;
	if( sn->envelope_state )
	{
		/* start ATTACK */
		sn->vol_rate = ( sn->attack_time > 0 ) ? VMAX / sn->attack_time : VMAX;
		sn->vol_step = +1;
		LOG(2,("SN76477 #%d: ATTACK rate %d/%d = %d/sec\n", param, sn->vol_rate, samplerate, sn->vol_rate/samplerate));
    }
	else
	{
		/* start DECAY */
		sn->vol = VMAX; /* just in case... */
		sn->vol_rate = ( sn->decay_time > 0 ) ? VMAX / sn->decay_time : VMAX;
		sn->vol_step = -1;
		LOG(2,("SN76477 #%d: DECAY rate %d/%d = %d/sec\n", param, sn->vol_rate, samplerate, sn->vol_rate/samplerate));
    }
}

static void vco_envelope_cb(INT32 param)
{
	attack_decay(param);
}

static void oneshot_envelope_cb(INT32 param)
{
	attack_decay(param);
}

static void sntimer_tick(INT32 param)
{
	if (sn76477[param]->envelope_timer > 0.0) {
		vco_envelope_cb(param);
		sn76477[param]->envelope_timer -= 1.0;
	}

	if (sn76477[param]->oneshot_timer > 0.0) {
		oneshot_envelope_cb(param);
		sn76477[param]->oneshot_timer -= 1.0;
	}
}

#if VERBOSE
static const char *mixer_mode[8] = {
	"VCO",
	"SLF",
	"Noise",
	"VCO/Noise",
	"SLF/Noise",
	"SLF/VCO/Noise",
	"SLF/VCO",
	"Inhibit"
};
#endif

/*****************************************************************************
 * set MIXER select inputs
 *****************************************************************************/
void SN76477_mixer_w(INT32 chip, INT32 data)
{
	struct SN76477 *sn = sn76477[chip];

	if( data == sn->mixer )
		return;

	sn->mixer = data;
	LOG(1,("SN76477 #%d: MIXER mode %d [%s]\n", chip, sn->mixer, mixer_mode[sn->mixer]));
}

void SN76477_set_mixer_params(INT32 chip, INT32 a, INT32 b, INT32 c)
{
	SN76477_mixer_a_w(chip, a);
	SN76477_mixer_b_w(chip, b);
	SN76477_mixer_c_w(chip, c);
}

void SN76477_mixer_a_w(INT32 chip, INT32 data)
{
	struct SN76477 *sn = sn76477[chip];

	data = data ? 1 : 0;
    if( data == (sn->mixer & 1) )
		return;

	sn->mixer = (sn->mixer & ~1) | data;
	LOG(1,("SN76477 #%d: MIXER mode %d [%s]\n", chip, sn->mixer, mixer_mode[sn->mixer]));
}

void SN76477_mixer_b_w(INT32 chip, INT32 data)
{
	struct SN76477 *sn = sn76477[chip];

	data = data ? 2 : 0;
    if( data == (sn->mixer & 2) )
		return;

	sn->mixer = (sn->mixer & ~2) | data;
	LOG(1,("SN76477 #%d: MIXER mode %d [%s]\n", chip, sn->mixer, mixer_mode[sn->mixer]));
}

void SN76477_mixer_c_w(INT32 chip, INT32 data)
{
	struct SN76477 *sn = sn76477[chip];

	data = data ? 4 : 0;
    if( data == (sn->mixer & 4) )
		return;

	sn->mixer = (sn->mixer & ~4) | data;
	LOG(1,("SN76477 #%d: MIXER mode %d [%s]\n", chip, sn->mixer, mixer_mode[sn->mixer]));
}

#if VERBOSE
static const char *envelope_mode[4] = {
	"VCO",
	"One-Shot",
	"Mixer only",
	"VCO with alternating Polarity"
};
#endif

/*****************************************************************************
 * set ENVELOPE select inputs
 *****************************************************************************/
void SN76477_envelope_w(INT32 chip, INT32 data)
{
	struct SN76477 *sn = sn76477[chip];

	if( data == sn->envelope )
		return;

	sn->envelope = data;
	LOG(1,("SN76477 #%d: ENVELOPE mode %d [%s]\n", chip, sn->envelope, envelope_mode[sn->envelope]));
}

void SN76477_set_envelope_params(INT32 chip, INT32 one, INT32 two)
{
	SN76477_envelope_1_w(chip, one);
	SN76477_envelope_2_w(chip, two);

}

void SN76477_envelope_1_w(INT32 chip, INT32 data)
{
	struct SN76477 *sn = sn76477[chip];

	if( data == (sn->envelope & 1) )
		return;

	sn->envelope = (sn->envelope & ~1) | data;
	LOG(1,("SN76477 #%d: ENVELOPE mode %d [%s]\n", chip, sn->envelope, envelope_mode[sn->envelope]));
}

void SN76477_envelope_2_w(INT32 chip, INT32 data)
{
	struct SN76477 *sn = sn76477[chip];

	data <<= 1;

	if( data == (sn->envelope & 2) )
		return;

	sn->envelope = (sn->envelope & ~2) | data;
	LOG(1,("SN76477 #%d: ENVELOPE mode %d [%s]\n", chip, sn->envelope, envelope_mode[sn->envelope]));
}

/*****************************************************************************
 * set VCO external/SLF input
 *****************************************************************************/
void SN76477_set_vco_mode(INT32 chip, INT32 data)
{
	SN76477_vco_w(chip, data);
}

void SN76477_vco_w(INT32 chip, INT32 data)
{
	struct SN76477 *sn = sn76477[chip];

	if( data == sn->vco_select )
		return;

	sn->vco_select = data;
	LOG(1,("SN76477 #%d: VCO select %d [%s]\n", chip, sn->vco_select, sn->vco_select ? "Internal (SLF)" : "External (Pin 16)"));
}

/*****************************************************************************
 * set VCO enable input
 *****************************************************************************/
void SN76477_set_enable(INT32 chip, INT32 enable)
{
	struct SN76477 *sn = sn76477[chip];

	sn->enable = (enable) ? 1 : 0;
}

void SN76477_enable_w(INT32 chip, INT32 data)
{
	struct SN76477 *sn = sn76477[chip];

	if( data == sn->enable )
		return;

	sn->enable = data;
	sn->envelope_state = data;

	sn->envelope_timer = 0;
	sn->oneshot_timer = 0;

	if( sn->enable == 0 )
	{
		switch( sn->envelope )
		{
		case 0: /* VCO */
			if( sn->vco_res > 0 && sn->vco_cap > 0 )
				sn->envelope_timer = (samplerate * TIME_IN_HZ(0.64/(sn->vco_res * sn->vco_cap))) / 1000;
			else
				oneshot_envelope_cb(chip);
			break;
			case 1: /* One-Shot */
			oneshot_envelope_cb(chip);
			if (sn->oneshot_time > 0)
				sn->oneshot_timer = sn->oneshot_time;
			break;
		case 2: /* MIXER only */
			sn->vol = VMAX;
			break;
		default:  /* VCO with alternating polariy */
			/* huh? */
			if( sn->vco_res > 0 && sn->vco_cap > 0 )
				sn->envelope_timer = (samplerate * TIME_IN_HZ(0.64/(sn->vco_res * sn->vco_cap)/2)) / 1000;
			else
				oneshot_envelope_cb(chip);
			break;
		}
	}
	else
	{
		switch( sn->envelope )
		{
		case 0: /* VCO */
			if( sn->vco_res > 0 && sn->vco_cap > 0 )
				sn->envelope_timer = (samplerate * TIME_IN_HZ(0.64/(sn->vco_res * sn->vco_cap))) / 1000;
			else
				oneshot_envelope_cb(chip);
			break;
		case 1: /* One-Shot */
			oneshot_envelope_cb(chip);
			break;
		case 2: /* MIXER only */
			sn->vol = VMIN;
			break;
		default:  /* VCO with alternating polariy */
			/* huh? */
			if( sn->vco_res > 0 && sn->vco_cap > 0 )
				sn->envelope_timer = (samplerate * TIME_IN_HZ(0.64/(sn->vco_res * sn->vco_cap)/2)) / 1000;
			else
				oneshot_envelope_cb(chip);
			break;
		}
	}
	LOG(1,("SN76477 #%d: ENABLE line %d [%s]\n", chip, sn->enable, sn->enable ? "Inhibited" : "Enabled" ));
}

/*****************************************************************************
 * set NOISE external signal (pin 3)
 *****************************************************************************/
void SN76477_noise_clock_w(INT32 chip, INT32 data)
{
	struct SN76477 *sn = sn76477[chip];

	if( data == sn->noise_clock )
		return;

	sn->noise_clock = data;
	/* on the rising edge shift the polynome */
	if( sn->noise_clock )
		sn->noise_poly = ((sn->noise_poly << 7) + (sn->noise_poly >> 10) + 0x18000) & 0x1ffff;
}

/*****************************************************************************
 * set NOISE resistor (pin 4)
 *****************************************************************************/
void SN76477_set_noise_params(INT32 chip, double res, double filtres, double cap)
{
	SN76477_set_noise_res(chip, res);
	SN76477_set_filter_res(chip, filtres);
	SN76477_set_filter_cap(chip, cap);
}

void SN76477_set_noise_res(INT32 chip, double res)
{
	struct SN76477 *sn = sn76477[chip];

	sn->noise_res = res;
}

/*****************************************************************************
 * set NOISE FILTER resistor (pin 5)
 *****************************************************************************/
void SN76477_set_filter_res(INT32 chip, double res)
{
	struct SN76477 *sn = sn76477[chip];

	if( res == sn->filter_res )
		return;

	sn->filter_res = res;
	if( sn->filter_res > 0 && sn->filter_cap > 0 )
	{
		sn->noise_freq = (int)(1.28 / (sn->filter_res * sn->filter_cap));
		LOG(1,("SN76477 #%d: NOISE FILTER freqency %d\n", chip, sn->noise_freq));
	}
	else
		sn->noise_freq = samplerate;
}

/*****************************************************************************
 * set NOISE FILTER capacitor (pin 6)
 *****************************************************************************/
void SN76477_set_filter_cap(INT32 chip, double cap)
{
	struct SN76477 *sn = sn76477[chip];

	if( cap == sn->filter_cap )
		return;

	sn->filter_cap = cap;
	if( sn->filter_res > 0 && sn->filter_cap > 0 )
	{
		sn->noise_freq = (int)(1.28 / (sn->filter_res * sn->filter_cap));
		LOG(1,("SN76477 #%d: NOISE FILTER freqency %d\n", chip, sn->noise_freq));
	}
	else
		sn->noise_freq = samplerate;
}

/*****************************************************************************
 * set DECAY resistor (pin 7)
 *****************************************************************************/
void SN76477_set_decay_res(INT32 chip, double res)
{
	struct SN76477 *sn = sn76477[chip];

	if( res == sn->decay_res )
		return;

	sn->decay_res = res;
	sn->decay_time = sn->decay_res * sn->attack_decay_cap;
	LOG(1,("SN76477 #%d: DECAY time is %fs\n", chip, sn->decay_time));
}

/*****************************************************************************
 * set ATTACK/DECAY capacitor (pin 8)
 *****************************************************************************/
void SN76477_set_attack_params(INT32 chip, double cap, double res)
{
	SN76477_set_attack_decay_cap(chip, cap);
	SN76477_set_attack_res(chip, res);
}

void SN76477_set_attack_decay_cap(INT32 chip, double cap)
{
	struct SN76477 *sn = sn76477[chip];

	if( cap == sn->attack_decay_cap )
		return;

	sn->attack_decay_cap = cap;
	sn->decay_time = sn->decay_res * sn->attack_decay_cap;
	sn->attack_time = sn->attack_res * sn->attack_decay_cap;
	LOG(1,("SN76477 #%d: ATTACK time is %fs\n", chip, sn->attack_time));
	LOG(1,("SN76477 #%d: DECAY time is %fs\n", chip, sn->decay_time));
}

/*****************************************************************************
 * set ATTACK resistor (pin 10)
 *****************************************************************************/
void SN76477_set_attack_res(INT32 chip, double res)
{
	struct SN76477 *sn = sn76477[chip];

	if( res == sn->attack_res )
		return;

	sn->attack_res = res;
	sn->attack_time = sn->attack_res * sn->attack_decay_cap;
	LOG(1,("SN76477 #%d: ATTACK time is %fs\n", chip, sn->attack_time));
}

/*****************************************************************************
 * set AMP resistor (pin 11)
 *****************************************************************************/
void SN76477_set_amplitude_res(INT32 chip, double res)
{
	struct SN76477 *sn = sn76477[chip];
	INT32 i;

	if( res == sn->amplitude_res )
		return;

	sn->amplitude_res = res;
	if( sn->amplitude_res > 0 )
	{
#if VERBOSE
		INT32 clip = 0;
#endif
		for( i = 0; i < VMAX+1; i++ )
		{
			INT32 vol = (int)((3.4 * sn->feedback_res / sn->amplitude_res) * 32767 * i / (VMAX+1));
#if VERBOSE
			if( vol > 32767 && !clip )
				clip = i;
			LOG(3,("%d\n", vol));
#endif
			if( vol > 32767 ) vol = 32767;
			sn->vol_lookup[i] = vol * mastervol / 100;
		}
		LOG(1,("SN76477 #%d: volume range from -%d to +%d (clip at %d%%)\n", chip, sn->vol_lookup[VMAX-VMIN], sn->vol_lookup[VMAX-VMIN], clip * 100 / 256));
	}
	else
	{
		memset(sn->vol_lookup, 0, sizeof(sn->vol_lookup));
	}
}

/*****************************************************************************
 * set FEEDBACK resistor (pin 12)
 *****************************************************************************/
void SN76477_set_feedback_res(INT32 chip, double res)
{
	struct SN76477 *sn = sn76477[chip];
	INT32 i;

	if( res == sn->feedback_res )
		return;

	sn->feedback_res = res;
	if( sn->amplitude_res > 0 )
	{
#if VERBOSE
		INT32 clip = 0;
#endif
		for( i = 0; i < VMAX+1; i++ )
		{
			INT32 vol = (int)((3.4 * sn->feedback_res / sn->amplitude_res) * 32767 * i / (VMAX+1));
#if VERBOSE
			if( vol > 32767 && !clip ) clip = i;
#endif
			if( vol > 32767 ) vol = 32767;
			sn->vol_lookup[i] = vol * mastervol / 100;
		}
		LOG(1,("SN76477 #%d: volume range from -%d to +%d (clip at %d%%)\n", chip, sn->vol_lookup[VMAX-VMIN], sn->vol_lookup[VMAX-VMIN], clip * 100 / 256));
	}
	else
	{
		memset(sn->vol_lookup, 0, sizeof(sn->vol_lookup));
	}
}

/*****************************************************************************
 * set PITCH voltage (pin 19)
 * TODO: fill with live...
 *****************************************************************************/
void SN76477_set_pitch_voltage(INT32 chip, double voltage)
{
	struct SN76477 *sn = sn76477[chip];

	if( voltage == sn->pitch_voltage )
		return;

	sn->pitch_voltage = voltage;
	LOG(1,("SN76477 #%d: VCO pitch voltage %f (%d%% duty cycle)\n", chip, sn->pitch_voltage, 0));
}

/*****************************************************************************
 * set VCO resistor (pin 18)
 *****************************************************************************/
void SN76477_set_vco_params(INT32 chip, double voltage, double cap, double res)
{
	SN76477_set_vco_voltage(chip, voltage);
	SN76477_set_vco_cap(chip, cap);
	SN76477_set_vco_res(chip, res);
}

void SN76477_set_vco_res(INT32 chip, double res)
{
	struct SN76477 *sn = sn76477[chip];

	if( res == sn->vco_res )
		return;

	sn->vco_res = res;
	if( sn->vco_res > 0 && sn->vco_cap > 0 )
	{
		sn->vco_freq = 0.64 / (sn->vco_res * sn->vco_cap);
		LOG(1,("SN76477 #%d: VCO freqency %f\n", chip, sn->vco_freq));
	}
	else
		sn->vco_freq = 0;
}

/*****************************************************************************
 * set VCO capacitor (pin 17)
 *****************************************************************************/
void SN76477_set_vco_cap(INT32 chip, double cap)
{
	struct SN76477 *sn = sn76477[chip];

	if( cap == sn->vco_cap )
		return;

	sn->vco_cap = cap;
	if( sn->vco_res > 0 && sn->vco_cap > 0 )
	{
		sn->vco_freq = 0.64 / (sn->vco_res * sn->vco_cap);
		LOG(1,("SN76477 #%d: VCO freqency %f\n", chip, sn->vco_freq));
	}
	else
		sn->vco_freq = 0;
}

/*****************************************************************************
 * set VCO voltage (pin 16)
 *****************************************************************************/
void SN76477_set_vco_voltage(INT32 chip, double voltage)
{
	struct SN76477 *sn = sn76477[chip];

	if( voltage == sn->vco_voltage )
		return;

	sn->vco_voltage = voltage;
	LOG(1,("SN76477 #%d: VCO ext. voltage %f (%f * %f = %f Hz)\n", chip,
		sn->vco_voltage,
		sn->vco_freq,
		10.0 * (5.0 - sn->vco_voltage) / 5.0,
		sn->vco_freq * 10.0 * (5.0 - sn->vco_voltage) / 5.0));
}

/*****************************************************************************
 * set SLF resistor (pin 20)
 *****************************************************************************/
void SN76477_set_slf_params(INT32 chip, double cap, double res)
{
	SN76477_set_slf_cap(chip, cap);
	SN76477_set_slf_res(chip, res);
}

void SN76477_set_slf_res(INT32 chip, double res)
{
	struct SN76477 *sn = sn76477[chip];

	if( res == sn->slf_res )
		return;

	sn->slf_res = res;
	if( sn->slf_res > 0 && sn->slf_cap > 0 )
	{
		sn->slf_freq = 0.64 / (sn->slf_res * sn->slf_cap);
		LOG(1,("SN76477 #%d: SLF freqency %f\n", chip, sn->slf_freq));
	}
	else
		sn->slf_freq = 0;
}

/*****************************************************************************
 * set SLF capacitor (pin 21)
 *****************************************************************************/
void SN76477_set_slf_cap(INT32 chip, double cap)
{
	struct SN76477 *sn = sn76477[chip];

	if( cap == sn->slf_cap )
		return;

	sn->slf_cap = cap;
	if( sn->slf_res > 0 && sn->slf_cap > 0 )
	{
		sn->slf_freq = 0.64 / (sn->slf_res * sn->slf_cap);
		LOG(1,("SN76477 #%d: SLF freqency %f\n", chip, sn->slf_freq));
	}
	else
		sn->slf_freq = 0;
}

/*****************************************************************************
 * set ONESHOT resistor (pin 24)
 *****************************************************************************/
void SN76477_set_oneshot_params(INT32 chip, double cap, double res)
{
	SN76477_set_oneshot_cap(chip, cap);
	SN76477_set_oneshot_res(chip, res);
}

void SN76477_set_oneshot_res(INT32 chip, double res)
{
	struct SN76477 *sn = sn76477[chip];

	if( res == sn->oneshot_res )
		return;
	sn->oneshot_res = res;
	sn->oneshot_time = 0.8 * sn->oneshot_res * sn->oneshot_cap;
	//bprintf(0,_T("SN76477 #%d: ONE-SHOT(res) time %fs\n"), chip, sn->oneshot_time);
}

/*****************************************************************************
 * set ONESHOT capacitor (pin 23)
 *****************************************************************************/
void SN76477_set_oneshot_cap(INT32 chip, double cap)
{
	struct SN76477 *sn = sn76477[chip];

	if( cap == sn->oneshot_cap )
        return;
    sn->oneshot_cap = cap;
	sn->oneshot_time = 0.8 * sn->oneshot_res * sn->oneshot_cap;
	//bprintf(0,_T("SN76477 #%d: ONE-SHOT(cap) time %fs\n"), chip, sn->oneshot_time);
}

void SN76477_set_mastervol(INT32 chip, double vol)
{
    mastervol = vol;
}

#define UPDATE_SLF															\
	/*************************************									\
	 * SLF super low frequency oscillator									\
	 * frequency = 0.64 / (r_slf * c_slf)									\
	 *************************************/ 								\
	sn->slf_count -= sn->slf_freq;											\
	while( sn->slf_count <= 0 ) 											\
	{																		\
		sn->slf_count += samplerate;									\
		sn->slf_out ^= 1;													\
	}

#define UPDATE_VCO															\
	/************************************									\
	 * VCO voltage controlled oscilator 									\
	 * min. freq = 0.64 / (r_vco * c_vco)									\
	 * freq. range is approx. 10:1											\
	 ************************************/									\
	if( sn->vco_select )													\
	{																		\
		/* VCO is controlled by SLF */										\
		if( sn->slf_dir == 0 )												\
		{																	\
			sn->slf_level -= sn->slf_freq * 2 * 5.0 / samplerate;		\
			if( sn->slf_level <= 0.0 )										\
			{																\
                sn->slf_level = 0.0;                                        \
				sn->slf_dir = 1;											\
			}																\
		}																	\
		else																\
		if( sn->slf_dir == 1 )												\
		{																	\
			sn->slf_level += sn->slf_freq * 2 * 5.0 / samplerate;		\
			if( sn->slf_level >= 5.0 )										\
			{																\
				sn->slf_level = 5.0;										\
				sn->slf_dir = 0;											\
            }                                                               \
        }                                                                   \
		sn->vco_step = sn->vco_freq * sn->slf_level;						\
	}																		\
	else																	\
	{																		\
		/* VCO is controlled by external voltage */ 						\
		sn->vco_step = sn->vco_freq * sn->vco_voltage;						\
	}																		\
	sn->vco_count -= sn->vco_step;											\
	while( sn->vco_count <= 0 ) 											\
	{																		\
		sn->vco_count += samplerate;									\
		sn->vco_out ^= 1;													\
	}

#define UPDATE_NOISE														\
	/*************************************									\
	 * NOISE pseudo rand number generator									\
	 *************************************/ 								\
	if( sn->noise_res > 0 ) 												\
		sn->noise_poly = ( (sn->noise_poly << 7) +							\
						   (sn->noise_poly >> 10) + 						\
						   0x18000 ) & 0x1ffff; 							\
																			\
	/* low pass filter: sample every noise_freq pseudo random value */		\
	sn->noise_count -= sn->noise_freq;										\
	while( sn->noise_count <= 0 )											\
	{																		\
		sn->noise_count = samplerate;									\
		sn->noise_out = sn->noise_poly & 1; 								\
	}

#define UPDATE_VOLUME														\
	/*************************************									\
	 * VOLUME adjust for attack/decay										\
	 *************************************/ 								\
	sn->vol_count -= sn->vol_rate;											\
	if( sn->vol_count <= 0 )												\
	{																		\
		INT32 n = - sn->vol_count / samplerate + 1; /* number of steps */ \
		sn->vol_count += n * samplerate;								\
		sn->vol += n * sn->vol_step;										\
		if( sn->vol < VMIN ) sn->vol = VMIN;								\
		if( sn->vol > VMAX ) sn->vol = VMAX;								\
		LOG(3,("SN76477 #%d: vol = $%04X\n", chip, sn->vol));      \
	}


/*****************************************************************************
 * mixer select 0 0 0 : VCO
 *****************************************************************************/
static void SN76477_update_0(INT32 chip, INT16 *buffer, INT32 length)
{
	struct SN76477 *sn = sn76477[chip];
	while( length-- )
	{
		UPDATE_VCO;
		UPDATE_VOLUME;
		INT16 sam = BURN_SND_CLIP((sn->vco_out ? sn->vol_lookup[sn->vol-VMIN] : -sn->vol_lookup[sn->vol-VMIN]) * mastervol);
		*buffer = BURN_SND_CLIP(*buffer + sam);
		buffer++;
		*buffer = BURN_SND_CLIP(*buffer + sam);
		buffer++;
		sntimer_tick(chip);
	}
}

/*****************************************************************************
 * mixer select 0 0 1 : SLF
 *****************************************************************************/
static void SN76477_update_1(INT32 chip, INT16 *buffer, INT32 length)
{
	struct SN76477 *sn = sn76477[chip];
	while( length-- )
	{
		UPDATE_SLF;
		UPDATE_VOLUME;
		INT16 sam = BURN_SND_CLIP((sn->slf_out ? sn->vol_lookup[sn->vol-VMIN] : -sn->vol_lookup[sn->vol-VMIN]) * mastervol);
		*buffer = BURN_SND_CLIP(*buffer + sam);
		buffer++;
		*buffer = BURN_SND_CLIP(*buffer + sam);
		buffer++;
		sntimer_tick(chip);
	}
}

/*****************************************************************************
 * mixer select 0 1 0 : NOISE
 *****************************************************************************/
static void SN76477_update_2(INT32 chip, INT16 *buffer, INT32 length)
{
	struct SN76477 *sn = sn76477[chip];
	while( length-- )
	{
		UPDATE_NOISE;
		UPDATE_VOLUME;
		INT16 sam = BURN_SND_CLIP((sn->noise_out ? sn->vol_lookup[sn->vol-VMIN] : -sn->vol_lookup[sn->vol-VMIN]) * mastervol);
		*buffer = BURN_SND_CLIP(*buffer + sam);
		buffer++;
		*buffer = BURN_SND_CLIP(*buffer + sam);
		buffer++;
		sntimer_tick(chip);
	}
}

/*****************************************************************************
 * mixer select 0 1 1 : VCO and NOISE
 *****************************************************************************/
static void SN76477_update_3(INT32 chip, INT16 *buffer, INT32 length)
{
	struct SN76477 *sn = sn76477[chip];
	while( length-- )
	{
		UPDATE_VCO;
		UPDATE_NOISE;
		UPDATE_VOLUME;
		INT16 sam = BURN_SND_CLIP(((sn->vco_out & sn->noise_out) ? sn->vol_lookup[sn->vol-VMIN] : -sn->vol_lookup[sn->vol-VMIN]) * mastervol);
		*buffer = BURN_SND_CLIP(*buffer + sam);
		buffer++;
		*buffer = BURN_SND_CLIP(*buffer + sam);
		buffer++;
		sntimer_tick(chip);
	}
}

/*****************************************************************************
 * mixer select 1 0 0 : SLF and NOISE
 *****************************************************************************/
static void SN76477_update_4(INT32 chip, INT16 *buffer, INT32 length)
{
	struct SN76477 *sn = sn76477[chip];
	while( length-- )
	{
		UPDATE_SLF;
		UPDATE_NOISE;
		UPDATE_VOLUME;
		INT16 sam = BURN_SND_CLIP(((sn->slf_out & sn->noise_out) ? sn->vol_lookup[sn->vol-VMIN] : -sn->vol_lookup[sn->vol-VMIN]) * mastervol);
		*buffer = BURN_SND_CLIP(*buffer + sam);
		buffer++;
		*buffer = BURN_SND_CLIP(*buffer + sam);
		buffer++;
		sntimer_tick(chip);
	}
}

/*****************************************************************************
 * mixer select 1 0 1 : VCO, SLF and NOISE
 *****************************************************************************/
static void SN76477_update_5(INT32 chip, INT16 *buffer, INT32 length)
{
	struct SN76477 *sn = sn76477[chip];
	while( length-- )
	{
		UPDATE_SLF;
		UPDATE_VCO;
		UPDATE_NOISE;
		UPDATE_VOLUME;
		INT16 sam = BURN_SND_CLIP(((sn->vco_out & sn->slf_out & sn->noise_out) ? sn->vol_lookup[sn->vol-VMIN] : -sn->vol_lookup[sn->vol-VMIN]) * mastervol);
		*buffer = BURN_SND_CLIP(*buffer + sam);
		buffer++;
		*buffer = BURN_SND_CLIP(*buffer + sam);
		buffer++;
		sntimer_tick(chip);
	}
}

/*****************************************************************************
 * mixer select 1 1 0 : VCO and SLF
 *****************************************************************************/
static void SN76477_update_6(INT32 chip, INT16 *buffer, INT32 length)
{
	struct SN76477 *sn = sn76477[chip];
	while( length-- )
	{
		UPDATE_SLF;
		UPDATE_VCO;
		UPDATE_VOLUME;
		INT16 sam = BURN_SND_CLIP(((sn->vco_out & sn->slf_out) ? sn->vol_lookup[sn->vol-VMIN] : -sn->vol_lookup[sn->vol-VMIN]) * mastervol);
		*buffer = BURN_SND_CLIP(*buffer + sam);
		buffer++;
		*buffer = BURN_SND_CLIP(*buffer + sam);
		buffer++;
		sntimer_tick(chip);
	}
}

/*****************************************************************************
 * mixer select 1 1 1 : Inhibit
 *****************************************************************************/
static void SN76477_update_7(INT32 chip, INT16 *buffer, INT32 length)
{
	while( length-- ) {
		//*buffer++ = 0;
		//*buffer++ = 0;
		sntimer_tick(chip);
	}
}

void SN76477_sound_update_INT(INT32 param, INT16 *buffer, INT32 length)
{
	struct SN76477 *sn = sn76477[param];
	if( sn->enable )
	{
		SN76477_update_7(param,buffer,length);
	}
	else
	{
		switch( sn->mixer )
		{
		case 0:
			SN76477_update_0(param,buffer,length);
			break;
		case 1:
			SN76477_update_1(param,buffer,length);
			break;
		case 2:
			SN76477_update_2(param,buffer,length);
			break;
		case 3:
			SN76477_update_3(param,buffer,length);
			break;
		case 4:
			SN76477_update_4(param,buffer,length);
			break;
		case 5:
			SN76477_update_5(param,buffer,length);
			break;
		case 6:
			SN76477_update_6(param,buffer,length);
			break;
		default:
			SN76477_update_7(param,buffer,length);
			break;
		}
	}
}

void SN76477_sound_update(INT16 *buffer, INT32 length)
{
	for (INT32 i = 0; i < sn76477_num; i++) {
		SN76477_sound_update_INT(i, buffer, length);
	}
}

void SN76477_exit()
{
	for (INT32 i = 0; i < sn76477_num; i++) {
		BurnFree(sn76477[i]);
	}
	sn76477_num = 0;
}

void SN76477_reset_INT(INT32 num)
{
	SN76477_mixer_w(num, 0x07);		/* turn off mixing */
	SN76477_envelope_w(num, 0x03);	/* envelope inputs open */
	SN76477_enable_w(num, 0x01);		/* enable input open */
}

void SN76477_reset()
{
	for (INT32 i = 0; i < sn76477_num; i++) {
		SN76477_reset_INT(i);
	}
}

void SN76477_init(INT32 num)
{
	if (num > MAX_SN76477-1)
	{
		bprintf(0, _T("SN76477_init(%d): initted past max.  crashing shortly...\n"));
		return;
	}

	sn76477[num] = (SN76477 *)BurnMalloc(sizeof(struct SN76477));
	if( !sn76477[num] )
	{
		LOG(0,("%s failed to malloc struct SN76477\n", name));
		return;
	}
	memset(sn76477[num], 0, sizeof(struct SN76477));

	sn76477_num = num + 1;

	samplerate = nBurnSoundRate;
	mastervol = 1.00;

	sn76477[num]->envelope_timer = 0;
	sn76477[num]->oneshot_timer = 0;

	SN76477_reset_INT(num);
}

void SN76477_scan(INT32 nAction, INT32* pnMin)
{
	for (INT32 i = 0; i < sn76477_num; i++) {
		ScanVar(sn76477[i], sizeof(SN76477), "sn76477 chippy");
	}
}
