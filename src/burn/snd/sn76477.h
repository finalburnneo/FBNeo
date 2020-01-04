/*****************************************************************************
  SN76477 pins and assigned interface variables/functions

							SN76477_envelope_w()
						   /					\
					[ 1] ENV SEL 1			ENV SEL 2 [28]
					[ 2] GND				  MIXER C [27] \
  SN76477_noise_w() [ 3] NOISE EXT OSC		  MIXER A [26]	> SN76477_mixer_w()
		  noise_res [ 4] RES NOISE OSC		  MIXER B [25] /
		 filter_res [ 5] NOISE FILTER RES	  O/S RES [24] oneshot_res
		 filter_cap [ 6] NOISE FILTER CAP	  O/S CAP [23] oneshot_cap
		  decay_res [ 7] DECAY RES			  VCO SEL [22] SN76477_vco_w()
   attack_decay_cap [ 8] A/D CAP			  SLF CAP [21] slf_cap
 SN76477_enable_w() [ 9] ENABLE 			  SLF RES [20] slf_res
		 attack_res [10] ATTACK RES 			PITCH [19] pitch_voltage
	  amplitude_res [11] AMP				  VCO RES [18] vco_res
	   feedback_res [12] FEEDBACK			  VCO CAP [17] vco_cap
					[13] OUTPUT 		 VCO EXT CONT [16] vco_voltage
					[14] Vcc			  +5V REG OUT [15]

	All resistor values in Ohms.
	All capacitor values in Farads.
	Use RES_K, RES_M and CAP_U, CAP_N, CAP_P macros to convert
	magnitudes, eg. 220k = RES_K(220), 47nF = CAP_N(47)

 *****************************************************************************/

#define MAX_SN76477 4

#include "rescap.h"

/* Noise clock write, useful only if noise_res is zero */
void SN76477_noise_clock_w(INT32 chip, INT32 data);

/* Enable (one input line: 0 enabled, 1 inhibited) - resets one shot */
void SN76477_enable_w(INT32 chip, INT32 data);

/* Mixer select (three input lines, data 0 to 7) */
void SN76477_mixer_w(INT32 chip, INT32 data);
void SN76477_set_mixer_params(INT32 chip, INT32 a, INT32 b, INT32 c);

/* Alternatively write the single input lines */
void SN76477_mixer_a_w(INT32 chip, INT32 data);
void SN76477_mixer_b_w(INT32 chip, INT32 data);
void SN76477_mixer_c_w(INT32 chip, INT32 data);

/* Select envelope (two input lines, data 0 to 3) */
void SN76477_envelope_w(INT32 chip, INT32 data);
void SN76477_set_envelope_params(INT32 chip, INT32 one, INT32 two); // both!

/* Alternatively use the single input lines */
void SN76477_envelope_1_w(INT32 chip, INT32 data);
void SN76477_envelope_2_w(INT32 chip, INT32 data);

/* VCO select (one input line: 0 external control, 1: SLF control) */
void SN76477_vco_w(INT32 chip, INT32 data);
void SN76477_set_vco_mode(INT32 chip, INT32 data);

void SN76477_set_noise_params(INT32 chip, double res, double filtres, double cap);
void SN76477_set_noise_res(INT32 chip, double res);
void SN76477_set_filter_res(INT32 chip, double res);
void SN76477_set_filter_cap(INT32 chip, double cap);
void SN76477_set_decay_res(INT32 chip, double res);
void SN76477_set_attack_params(INT32 chip, double cap, double res);
void SN76477_set_attack_decay_cap(INT32 chip, double cap);
void SN76477_set_attack_res(INT32 chip, double res);
void SN76477_set_amplitude_res(INT32 chip, double res);
void SN76477_set_feedback_res(INT32 chip, double res);
void SN76477_set_slf_params(INT32 chip, double cap, double res);
void SN76477_set_slf_res(INT32 chip, double res);
void SN76477_set_slf_cap(INT32 chip, double cap);
void SN76477_set_oneshot_params(INT32 chip, double cap, double res);
void SN76477_set_oneshot_res(INT32 chip, double res);
void SN76477_set_oneshot_cap(INT32 chip, double cap);
void SN76477_set_vco_params(INT32 chip, double voltage, double cap, double res);
void SN76477_set_vco_res(INT32 chip, double res);
void SN76477_set_vco_cap(INT32 chip, double cap);
void SN76477_set_pitch_voltage(INT32 chip, double voltage);
void SN76477_set_vco_voltage(INT32 chip, double voltage);
void SN76477_set_mastervol(INT32 chip, double vol);
void SN76477_sound_update(INT32 param, INT16 *buffer, INT32 length);
void SN76477_set_enable(INT32 chip, INT32 enable);

void SN76477_init(INT32 num);
void SN76477_reset(INT32 num);
void SN76477_exit(INT32 num);
