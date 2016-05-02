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

/* Little helpers for magnitude conversions */
#define RES_K(res) ((double)res*1e3)
#define RES_M(res) ((double)res*1e6)
#define CAP_U(cap) ((double)cap*1e-6)
#define CAP_N(cap) ((double)cap*1e-9)
#define CAP_P(cap) ((double)cap*1e-12)
#define TIME_IN_HZ(hz) (1.0 / (double)(hz))

/* Noise clock write, useful only if noise_res is zero */
extern void SN76477_noise_clock_w(int chip, int data);

/* Enable (one input line: 0 enabled, 1 inhibited) - resets one shot */
extern void SN76477_enable_w(int chip, int data);

/* Mixer select (three input lines, data 0 to 7) */
extern void SN76477_mixer_w(int chip, int data);

/* Alternatively write the single input lines */
extern void SN76477_mixer_a_w(int chip, int data);
extern void SN76477_mixer_b_w(int chip, int data);
extern void SN76477_mixer_c_w(int chip, int data);

/* Select envelope (two input lines, data 0 to 3) */
extern void SN76477_envelope_w(int chip, int data);

/* Alternatively use the single input lines */
extern void SN76477_envelope_1_w(int chip, int data);
extern void SN76477_envelope_2_w(int chip, int data);

/* VCO select (one input line: 0 external control, 1: SLF control) */
extern void SN76477_vco_w(int chip, int data);

void SN76477_set_noise_res(int chip, double res);
void SN76477_set_filter_res(int chip, double res);
void SN76477_set_filter_cap(int chip, double cap);
void SN76477_set_decay_res(int chip, double res);
void SN76477_set_attack_decay_cap(int chip, double cap);
void SN76477_set_attack_res(int chip, double res);
void SN76477_set_amplitude_res(int chip, double res);
void SN76477_set_feedback_res(int chip, double res);
void SN76477_set_slf_res(int chip, double res);
void SN76477_set_slf_cap(int chip, double cap);
void SN76477_set_oneshot_res(int chip, double res);
void SN76477_set_oneshot_cap(int chip, double cap);
void SN76477_set_vco_res(int chip, double res);
void SN76477_set_vco_cap(int chip, double cap);
void SN76477_set_pitch_voltage(int chip, double voltage);
void SN76477_set_vco_voltage(int chip, double voltage);
void SN76477_set_mastervol(int chip, double vol);
void SN76477_sound_update(int param, INT16 *buffer, int length);

void SN76477_init(int num);
void SN76477_exit(int num);
