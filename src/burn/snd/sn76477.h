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
