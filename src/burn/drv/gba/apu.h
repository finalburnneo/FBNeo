#pragma once

#ifndef GBA_APU_H
#define GBA_APU_H

#include "gba.h"

static inline void gba_audio_fifo_push(gba_t* gba, INT32 fifo, INT8 data)
{
	INT32 size = (gba->audio.fifo[fifo].write_ptr - gba->audio.fifo[fifo].read_ptr) & 0x1f;
	if (size < 28) {
		gba->audio.fifo[fifo].write_ptr = (gba->audio.fifo[fifo].write_ptr + 1) & 0x1f;
		gba->audio.fifo[fifo].data[gba->audio.fifo[fifo].write_ptr] = data;
	}
}

static inline float gba_polyblep(float t, float dt)
{
	if (t <= dt) {
		t = t / dt;
		return t + t - t * t - 1.0;;
	} else if (t >= 1 - dt) {
		t = (t - 1.0) / dt;
		return t * t + t + t + 1.0;
	} else return 0;
}

static inline float gba_bandlimited_square(float t, float duty_cycle, float dt)
{
	float t2 = t - duty_cycle;
	if (t2 < 0.0)
		t2 += 1.0;
	float y = t < duty_cycle ? -1 : 1;
	y -= gba_polyblep(t,  dt);
	y += gba_polyblep(t2, dt);
	return y;
}

// BEGIN GB REUSE CODE SHIM
#define sb_compute_next_sweep_freq	gba_compute_next_sweep_freq
#define sb_tick_frame_sweep			gba_tick_frame_sweep
#define sb_tick_frame_seq			gba_tick_frame_seq
#define sb_process_audio_writes		gba_process_audio_writes
#define sb_process_audio			gba_tick_audio
#define sb_frame_sequencer_t		gba_frame_sequencer_t
#define sb_audio_t					gba_audio_t
#define sb_gb_t						gba_t
#define sb_read8_io					gba_audio_read8
#define sb_store8_io				gba_audio_store8
#define sb_bandlimited_square		gba_bandlimited_square
#define sb_gbc_enable(a)			(true)
#define sb_read_wave_ram			gba_read_wave_ram

#define GBA_AUDIO					1

#define SB_IO_AUD1_TONE_SWEEP		0xff10
#define SB_IO_AUD1_LENGTH_DUTY		0xff11
#define SB_IO_AUD1_VOL_ENV			0xff12
#define SB_IO_AUD1_FREQ				0xff13
#define SB_IO_AUD1_FREQ_HI			0xff14

#define SB_IO_AUD2_LENGTH_DUTY		0xff16
#define SB_IO_AUD2_VOL_ENV			0xff17
#define SB_IO_AUD2_FREQ				0xff18
#define SB_IO_AUD2_FREQ_HI			0xff19

#define SB_IO_AUD3_POWER			0xff1A
#define SB_IO_AUD3_LENGTH			0xff1B
#define SB_IO_AUD3_VOL				0xff1C
#define SB_IO_AUD3_FREQ				0xff1D
#define SB_IO_AUD3_FREQ_HI			0xff1E
#define SB_IO_AUD3_WAVE_BASE		0xff30

#define SB_IO_AUD4_LENGTH			0xff20
#define SB_IO_AUD4_VOL_ENV			0xff21
#define SB_IO_AUD4_POLY				0xff22
#define SB_IO_AUD4_COUNTER			0xff23
#define SB_IO_MASTER_VOLUME			0xff24
#define SB_IO_SOUND_OUTPUT_SEL		0xff25

#define SB_IO_SOUND_ON_OFF			0xff26

static inline INT32 gba_audio_reg_to_mmio(INT32 gb_reg)
{
	switch (gb_reg) {
		case SB_IO_AUD1_TONE_SWEEP : return GBA_SOUND1CNT_L;
		case SB_IO_AUD1_LENGTH_DUTY: return GBA_SOUND1CNT_H;
		case SB_IO_AUD1_VOL_ENV    : return GBA_SOUND1CNT_H + 1;
		case SB_IO_AUD1_FREQ       : return GBA_SOUND1CNT_X;
		case SB_IO_AUD1_FREQ_HI    : return GBA_SOUND1CNT_X + 1;
		case SB_IO_AUD2_LENGTH_DUTY: return GBA_SOUND2CNT_L;
		case SB_IO_AUD2_VOL_ENV    : return GBA_SOUND2CNT_L + 1;
		case SB_IO_AUD2_FREQ       : return GBA_SOUND2CNT_H;
		case SB_IO_AUD2_FREQ_HI    : return GBA_SOUND2CNT_H + 1;
		case SB_IO_AUD3_POWER      : return GBA_SOUND3CNT_L;
		case SB_IO_AUD3_LENGTH     : return GBA_SOUND3CNT_H;
		case SB_IO_AUD3_VOL        : return GBA_SOUND3CNT_H + 1;
		case SB_IO_AUD3_FREQ       : return GBA_SOUND3CNT_X;
		case SB_IO_AUD3_FREQ_HI    : return GBA_SOUND3CNT_X + 1;
		case SB_IO_AUD4_LENGTH     : return GBA_SOUND4CNT_L;
		case SB_IO_AUD4_VOL_ENV    : return GBA_SOUND4CNT_L + 1;
		case SB_IO_AUD4_POLY       : return GBA_SOUND4CNT_H;
		case SB_IO_AUD4_COUNTER    : return GBA_SOUND4CNT_H + 1;
		case SB_IO_MASTER_VOLUME   : return GBA_SOUNDCNT_L;
		case SB_IO_SOUND_OUTPUT_SEL: return GBA_SOUNDCNT_L  + 1;
		case SB_IO_SOUND_ON_OFF    : return GBA_SOUNDCNT_X;
	}
	printf("Unknown GB register:%04x\n", gb_reg);
	return 0;
}

static inline INT32 gba_mmio_to_audio_reg(INT32 gb_reg)
{
	switch (gb_reg) {
		case GBA_SOUND1CNT_L    : return SB_IO_AUD1_TONE_SWEEP;
		case GBA_SOUND1CNT_H    : return SB_IO_AUD1_LENGTH_DUTY;
		case GBA_SOUND1CNT_H + 1: return SB_IO_AUD1_VOL_ENV;
		case GBA_SOUND1CNT_X    : return SB_IO_AUD1_FREQ;
		case GBA_SOUND1CNT_X + 1: return SB_IO_AUD1_FREQ_HI;
		case GBA_SOUND2CNT_L    : return SB_IO_AUD2_LENGTH_DUTY;
		case GBA_SOUND2CNT_L + 1: return SB_IO_AUD2_VOL_ENV;
		case GBA_SOUND2CNT_H    : return SB_IO_AUD2_FREQ;
		case GBA_SOUND2CNT_H + 1: return SB_IO_AUD2_FREQ_HI;
		case GBA_SOUND3CNT_L    : return SB_IO_AUD3_POWER;
		case GBA_SOUND3CNT_H    : return SB_IO_AUD3_LENGTH;
		case GBA_SOUND3CNT_H + 1: return SB_IO_AUD3_VOL;
		case GBA_SOUND3CNT_X    : return SB_IO_AUD3_FREQ;
		case GBA_SOUND3CNT_X + 1: return SB_IO_AUD3_FREQ_HI;
		case GBA_SOUND4CNT_L    : return SB_IO_AUD4_LENGTH;
		case GBA_SOUND4CNT_L + 1: return SB_IO_AUD4_VOL_ENV;
		case GBA_SOUND4CNT_H    : return SB_IO_AUD4_POLY;
		case GBA_SOUND4CNT_H + 1: return SB_IO_AUD4_COUNTER;
		case GBA_SOUNDCNT_L     : return SB_IO_MASTER_VOLUME;
		case GBA_SOUNDCNT_L  + 1: return SB_IO_SOUND_OUTPUT_SEL;
		case GBA_SOUNDCNT_X     : return SB_IO_SOUND_ON_OFF;
	}
	return 0;
}
static inline UINT8 gba_audio_read8(gba_t* gba, INT32 addr){
  return gba_io_read8(gba,gba_audio_reg_to_mmio(addr));
}

static inline void gba_audio_store8(gba_t* gba, INT32 addr, UINT8 data)
{
	addr = gba_audio_reg_to_mmio(addr);
	if (addr)gba_io_store8(gba, addr, data);
}

static inline UINT8 gba_audio_process_byte_write(gba_t* gba, UINT32 addr, UINT8 value)
{
	gba_t* gb = gba;
	sb_frame_sequencer_t* seq = &gba->audio.sequencer;
	addr = gba_mmio_to_audio_reg(addr);
	INT32 i = (addr - SB_IO_AUD1_LENGTH_DUTY) / 5;
	if (!addr)
		return value;
	if (addr == SB_IO_SOUND_ON_OFF) {
		value &= 0xf0;
		value |= sb_read8_io(gb, SB_IO_SOUND_ON_OFF) & 0xf;
	}
	if (addr >= SB_IO_AUD3_WAVE_BASE && addr < SB_IO_AUD3_WAVE_BASE + 16) {
		bool wave_active = SB_BFE(sb_read8_io(gb, SB_IO_SOUND_ON_OFF), 2, 1);
		if (wave_active) {
			//Addr locked to the read pointer when the wave channel is active
			addr = SB_IO_AUD3_WAVE_BASE + ((gb->audio.wave_sample_offset) % 32) / 2;
		}
	}
	if (addr == SB_IO_AUD1_LENGTH_DUTY || addr == SB_IO_AUD2_LENGTH_DUTY || addr == SB_IO_AUD3_LENGTH || addr == SB_IO_AUD4_LENGTH) {
		UINT8 length_duty = value;
		if (i == 2)
			seq->length[i] = 256 - SB_BFE(length_duty, 0, 8);
		else
			seq->length[i] =  64 - SB_BFE(length_duty, 0, 6);
	} else if (addr == SB_IO_AUD1_VOL_ENV || addr == SB_IO_AUD2_VOL_ENV || addr == SB_IO_AUD4_VOL_ENV) {
		bool power = SB_BFE(value, 3, 5) != 0;
		seq->powered[i] = power;
		seq->active[i] &= power;
		seq->env_direction[i] = (SB_BFE(value, 3, 1) ? 1 : -1);
		seq->env_period[i]    =  SB_BFE(value, 0, 3);
		if (seq->env_period[i] == 0 && !seq->env_overflow[i]) {
			seq->volume[i] = (seq->volume[i] + 1) & 0xf;
		}
	} else if (addr == SB_IO_AUD1_FREQ || addr == SB_IO_AUD1_FREQ_HI ||
		addr == SB_IO_AUD2_FREQ || addr == SB_IO_AUD2_FREQ_HI ||
		addr == SB_IO_AUD3_FREQ || addr == SB_IO_AUD3_FREQ_HI
		) {
		sb_store8_io(gb, addr, value);
		UINT8 freq_lo = sb_read8_io(gb, SB_IO_AUD1_FREQ    + i * 5);
		UINT8 freq_hi = sb_read8_io(gb, SB_IO_AUD1_FREQ_HI + i * 5);
		seq->frequency[i] = freq_lo | ((int)(SB_BFE(freq_hi, 0, 3)) << 8u);
	}
	return value;
};

static inline UINT8 sb_read_wave_ram(sb_gb_t* gb, INT32 byte)
{
	return gba_io_read8(gb, GBA_WAVE_RAM + byte);
}

static inline INT32 sb_compute_next_sweep_freq(sb_frame_sequencer_t* seq)
{
	INT32 shift = seq->sweep_shift ? seq->sweep_shift : 8;
	INT32 increment     = (seq->frequency[0] >> shift) * seq->sweep_direction;
	INT32 new_frequency =  seq->frequency[0] + increment;
	seq->sweep_subtracted |= seq->sweep_direction == -1;
	return new_frequency;
}

static inline void sb_tick_frame_sweep(sb_frame_sequencer_t* seq)
{
	INT32 new_frequency = sb_compute_next_sweep_freq(seq);
	if (new_frequency > 2047) {
		seq->active[0] = false;
		new_frequency  = 2047;
	} else if (new_frequency < 0)new_frequency = 0;
	if (seq->sweep_shift) {
		seq->frequency[0] = new_frequency;
		new_frequency = sb_compute_next_sweep_freq(seq);
		if (new_frequency > 2047) {
			seq->active[0] = false;
			new_frequency  = 2047;
		}
	}
}

static inline void sb_tick_frame_seq(sb_gb_t* gb, sb_frame_sequencer_t* seq)
{
	INT32 step = (seq->step_counter++) % 8;
	//Tick sweep
	if (step == 2 || step == 6) {
		if (seq->active[0] && seq->sweep_enable) {
			if (seq->sweep_timer > 0)
				seq->sweep_timer--;
			if (seq->sweep_timer == 0) {
				if (seq->sweep_period > 0) {
					seq->sweep_timer = seq->sweep_period;
					sb_tick_frame_sweep(seq);
				} else
					seq->sweep_timer = 8;
			}
		}
	}
	//Tick envelope
	if (step == 7) {
		for (INT32 i = 0; i < 4; ++i) {
			if (i == 2)
				continue;
			if (seq->env_period[i]) {
				if (seq->env_period_timer[i] > 0)
					seq->env_period_timer[i]--;
				if (seq->env_period_timer[i] == 0) {
					seq->env_period_timer[i] = seq->env_period[i];
					INT32 volume = seq->volume[i];
					volume += seq->env_direction[i];
					if (volume <= 0) {
						volume  = 0;
						seq->env_overflow[i] = true;
					}
					if (volume > 0xf) {
						volume = 0xf;
						seq->env_overflow[i] = true;
					};
					seq->volume[i] = volume;
				}
			}
		}
	}
	if ((step % 2) == 0) {
		//Tick length
		for (INT32 i = 0; i < 4; ++i) {
			if (!seq->use_length[i])
				continue;
			if (seq->length[i] > 0)
				seq->length[i]--;
			if (seq->length[i] == 0) {
				seq->active[i] = false;
				seq->length[i] = i == 2 ? 256 : 64;
				seq->use_length[i] = false;
			}
		}
	}
	INT32 nrf_52 = sb_read8_io(gb, SB_IO_SOUND_ON_OFF) & 0xf0;
	for (INT32 i = 0; i < 4; ++i) {
		seq->active[i] &= seq->powered[i];
		bool active = seq->active[i];
		nrf_52 |= active << i;
	}
	sb_store8_io(gb, SB_IO_SOUND_ON_OFF, nrf_52);
}

static inline void sb_process_audio_writes(sb_gb_t* gb)
{
	sb_audio_t* audio         = &gb->audio;
	sb_frame_sequencer_t* seq = &audio->sequencer;
	INT32 nrf_52 = sb_read8_io(gb, SB_IO_SOUND_ON_OFF) & 0xf0;
	bool master_enable = SB_BFE(nrf_52, 7, 1);
	if (!master_enable) {
		for (INT32 i = SB_IO_AUD1_TONE_SWEEP; i < SB_IO_SOUND_ON_OFF; ++i) {
			sb_store8_io(gb, i, 0);
		}
		for (INT32 i = 0; i < 4; ++i) {
			if (sb_gbc_enable(gb) || i != 3) {
				seq->active[i ] = false;
				seq->powered[i] = false;
				seq->length[i ] = 0;
			}
			seq->use_length[i]  = false;
		}
	} else {
		UINT8 freq_sweep1 = sb_read8_io(gb, SB_IO_AUD1_TONE_SWEEP);
		seq->sweep_period    = SB_BFE(freq_sweep1, 4, 3);
		seq->sweep_shift     = SB_BFE(freq_sweep1, 0, 3);
		seq->sweep_direction = SB_BFE(freq_sweep1, 3, 1) ? -1. : 1;
		for (INT32 i = 0; i < 4; ++i) {
			bool prev_length_en = seq->use_length[i];
			UINT8 freq_hi = sb_read8_io(gb, SB_IO_AUD1_FREQ_HI + i * 5);
			seq->use_length[i] = SB_BFE(freq_hi, 6, 1);
			UINT8 vol_env = sb_read8_io(gb, SB_IO_AUD1_VOL_ENV + i * 5);
			if (i == 2) {
				bool power = SB_BFE(sb_read8_io(gb, SB_IO_AUD3_POWER), 7, 1);
				seq->powered[i] = power;
			}
			if (i != 0) {
				UINT8 freq_lo = sb_read8_io(gb, SB_IO_AUD1_FREQ + i * 5);
				seq->frequency[i] = freq_lo | ((int)(SB_BFE(freq_hi, 0, 3)) << 8u);
			}
			if (i == 2) {
				seq->env_direction[i] = 0;
				seq->env_period[i   ] = 0;
			} else {
				seq->env_direction[i] = (SB_BFE(vol_env, 3, 1) ? 1 : -1);
				seq->env_period[i   ] =  SB_BFE(vol_env, 0, 3);
			}
			bool triggered = SB_BFE(freq_hi, 7, 1);
			if (triggered) {
				UINT8 freq_lo     = sb_read8_io(gb, SB_IO_AUD1_FREQ + i * 5);
				seq->frequency[i] = freq_lo | ((int)(SB_BFE(freq_hi, 0, 3)) << 8u);
				seq->volume[i   ] = SB_BFE(vol_env, 4, 4);

				if (seq->length[i] == 0)
					seq->length[i] = i == 2 ? 256 : 64;
				if (i == 3)
					seq->lfsr4 = 0x7fff;
				if (i == 2) {
					audio->wave_sample_offset = 31;
					audio->wave_freq_timer    = 4;
				}
				seq->env_period_timer[i] = 0;
				seq->env_overflow[i    ] = false;
				seq->chan_t[i          ] = 0;
				seq->active[i          ] = true;
				if (i == 0) {
					seq->sweep_subtracted = false;
					seq->sweep_enable     = seq->sweep_period || seq->sweep_shift;
					seq->sweep_timer      = seq->sweep_period;
					if (seq->sweep_timer == 0)
						seq->sweep_timer = 8;
					if (seq->sweep_shift && sb_compute_next_sweep_freq(seq) > 2047) {
						seq->active[0] = false;
					}
					seq->sweep_enable = seq->sweep_period > 0 || seq->sweep_shift > 0;
				}
			}
			if (i == 0 && seq->sweep_subtracted && seq->sweep_direction != -1) {
				seq->active[0]    = false;
				seq->sweep_enable = false;
			}
			if (seq->use_length[i] && !prev_length_en) {
				bool second_half_of_length_period = (seq->step_counter & 1);
				if (second_half_of_length_period) {
					if (seq->length[i])
						seq->length[i]--;
					if (seq->length[i] == 0) {
						if (triggered)
							seq->length[i] = i == 2 ? 255 : 63;
						else {
							seq->active[i    ] = false;
							seq->use_length[i] = triggered && seq->use_length[i];
						}
					}
				}
			}
			sb_store8_io(gb, SB_IO_AUD1_FREQ_HI + i * 5, freq_hi & 0x7f);
		}
	}
	nrf_52 = sb_read8_io(gb, SB_IO_SOUND_ON_OFF) & 0xf0;
	for (INT32 i = 0; i < 4; ++i) {
		seq->active[i] &= seq->powered[i];
		bool active = seq->active[i];
		nrf_52 |= active << i;
	}
	sb_store8_io(gb, SB_IO_SOUND_ON_OFF, nrf_52);
}

static inline void sb_process_audio(sb_gb_t *gb, sb_emu_state_t*emu, double delta_time, INT32 cycles)
{
	sb_audio_t* audio = &gb->audio;
	sb_frame_sequencer_t* seq = &audio->sequencer;

	if (delta_time > 1.0 / 60.)
		delta_time = 1.0 / 60.;
	audio->current_sim_time += delta_time;
#ifdef GBA_AUDIO
	UINT32 prev_audio_clock = audio->audio_clock;
	audio->audio_clock += cycles;
	cycles = (audio->audio_clock - (prev_audio_clock & ~3)) / 4;
	UINT32 frame_cycles = (audio->audio_clock - (prev_audio_clock & ~32767)) / 32768;
	while (frame_cycles--)
		gba_tick_frame_seq(gb, seq);
#endif

	INT32 freq_tim = audio->wave_freq_timer;
	freq_tim -= cycles;
	if (freq_tim < 0) {
		INT32 wave_inc_count = (-freq_tim - 1) / ((2048 - seq->frequency[2]) * 2) + 1;
		audio->wave_sample_offset += wave_inc_count;
		freq_tim += (2048 - seq->frequency[2]) * 2 * wave_inc_count;
		UINT32 wav_samp = (audio->wave_sample_offset) % 32;
		INT32 dat = sb_read_wave_ram(gb, wav_samp / 2);
		audio->curr_wave_data = dat;
		INT32 offset = (wav_samp & 1) ? 0 : 4;
		audio->curr_wave_sample = ((dat >> offset) & 0xf);
	}
	audio->wave_freq_timer = freq_tim;

	audio->current_sample_generated_time -= (int)(audio->current_sim_time);
	audio->current_sim_time -= (int)(audio->current_sim_time);

	if (audio->current_sample_generated_time > audio->current_sim_time)
		return;

	INT32 nrf_52 = sb_read8_io(gb, SB_IO_SOUND_ON_OFF) & 0xf0;

	bool master_enable = SB_BFE(nrf_52, 7, 1);
	if (!master_enable)
		return;
	if (emu->audio_sample_rate <= 0.0)
		return;
	float sample_delta_t = (float)(1.0 / emu->audio_sample_rate);

	const static float duty_lookup[] = { 0.125, 0.25, 0.5, 0.75 };
	UINT8 length_duty1 = sb_read8_io(gb, SB_IO_AUD1_LENGTH_DUTY);
	float duty1 = duty_lookup[SB_BFE(length_duty1, 6, 2)];
	UINT8 length_duty2 = sb_read8_io(gb, SB_IO_AUD2_LENGTH_DUTY);
	float duty2 = duty_lookup[SB_BFE(length_duty2, 6, 2)];

	UINT8 power3   = sb_read8_io(gb, SB_IO_AUD3_POWER);
	UINT8 vol_env3 = sb_read8_io(gb, SB_IO_AUD3_VOL);
	INT32 channel3_shift = SB_BFE(vol_env3, 5, 2) - 1;
	if (SB_BFE(power3, 7, 1) == 0 || channel3_shift == -1)
		channel3_shift = 4;

	UINT8 poly4 = sb_read8_io(gb, SB_IO_AUD4_POLY);
	float r4       = SB_BFE(poly4, 0, 3);
	UINT8 s4       = SB_BFE(poly4, 4, 4);
	bool sevenBit4 = SB_BFE(poly4, 3, 1);
	if (r4 == 0)
		r4 = 0.5;

	UINT8 master_vol = sb_read8_io(gb, SB_IO_MASTER_VOLUME);
	float master_left  = SB_BFE(master_vol, 4, 3) / 7.;
	float master_right = SB_BFE(master_vol, 0, 3) / 7.;

	UINT8 chan_sel = sb_read8_io(gb, SB_IO_SOUND_OUTPUT_SEL);
	//These are type int to allow them to be multiplied to enable/disable
	float chan_l[6] = { 0 };
	float chan_r[6] = { 0 };
	for (INT32 i = 0;i < 4;++i) {
		chan_l[i] = SB_BFE(chan_sel, i,     1);
		chan_r[i] = SB_BFE(chan_sel, i + 4, 1);
	}

	#ifdef GBA_AUDIO
	{
		UINT16 soundcnt_h = gba_io_read16(gb, GBA_SOUNDCNT_H);
		//These are type int to allow them to be multiplied to enable/disable
		UINT16 snd_sel    = gba_io_read16(gb, GBA_SOUNDCNT_L);
		// SOUNDCNT_H: PSG volume, DMA FIFO volume/routing/timer select/reset
		float psg_volume_lookup[4] = { 0.25,0.5,1.0,0. };
		float psg_volume = psg_volume_lookup[SB_BFE(soundcnt_h, 0, 2)] * 0.25;

		float r_vol = SB_BFE(snd_sel, 0, 3) / 7. * psg_volume;
		float l_vol = SB_BFE(snd_sel, 4, 3) / 7. * psg_volume;
		for (INT32 i = 0;i < 4;++i) {
			chan_r[i] *= r_vol;
			chan_l[i] *= l_vol;
		}
		// Channel volume for each FIFO
		for (INT32 i = 0;i < 2;++i) {
			// Volume
			chan_r[i + 4] = chan_l[i + 4] = SB_BFE(soundcnt_h, 2 + i, 1) ? 1.0 : 0.5;
			chan_r[i + 4] *= SB_BFE(soundcnt_h, 8 + i * 4, 1);
			chan_l[i + 4] *= SB_BFE(soundcnt_h, 9 + i * 4, 1);
		}
		gba_io_store16(gb, GBA_SOUNDCNT_H, soundcnt_h & ~((1 << 11) | (1 << 15)));
		master_left = master_right = 1;
	}
	#endif

	float freq_hz[4];
	for (INT32 i = 0;i < 2;++i) {
		freq_hz[i] = 131072. / (2048 - seq->frequency[i]);
	}
	freq_hz[2] = (65536.) / (2048 - seq->frequency[2]);
	freq_hz[3] = 524288.0 / r4 / pow(2.0, s4 + 1);
	while (audio->current_sample_generated_time < audio->current_sim_time) {
		audio->current_sample_generated_time += sample_delta_t;

		bool buffer_full = sb_ring_buffer_size(&emu->audio_ring_buff) + 3 > SB_AUDIO_RING_BUFFER_SIZE;
		if (emu->capture_audio && buffer_full)
			continue;

		//Advance each channel
		for (INT32 i = 0; i < 4; ++i)
			seq->chan_t[i] += sample_delta_t * freq_hz[i];
		//Generate new noise value if needed
		if (seq->chan_t[3] >= 1.0) {
			INT32 bit = (seq->lfsr4 ^ (seq->lfsr4 >> 1)) & 1;
			seq->lfsr4 >>= 1;
			seq->lfsr4  |= bit << 14;
			if (sevenBit4) {
				seq->lfsr4 &= ~(1 << 7);
				seq->lfsr4 |= bit << 6;
			}
		}

		//Loopback
		for (INT32 i = 0; i < 4; ++i)
			seq->chan_t[i] -= (int)seq->chan_t[i];

		//Compute and clamp Volume Envelopes
		float v[4];
		for (INT32 i = 0; i < 4; ++i)
			v[i] = seq->active[i] ? seq->volume[i] / 15. : 0;
		v[2] = 1.0;

		INT32 dat = audio->curr_wave_sample >> channel3_shift;
		INT32 wav_offset = 8 >> channel3_shift;

		float channels[6];
		channels[0] = sb_bandlimited_square(seq->chan_t[0], duty1, sample_delta_t * freq_hz[0]) * v[0];
		channels[1] = sb_bandlimited_square(seq->chan_t[1], duty2, sample_delta_t * freq_hz[1]) * v[1];
		channels[2] = (dat - wav_offset) / 8.;
		channels[3] = ((seq->lfsr4 & 1) * 2. - 1.) * v[3];

	#ifdef GBA_AUDIO
		for (INT32 i = 0; i < 2; ++i)
			channels[4 + i] = audio->fifo[i].data[audio->fifo[i].read_ptr & 0x1f] / 128.;
	#else
		for (INT32 i = 0; i < 2; ++i)
			channels[4 + i] = 0;
	#endif 

		//Mix channels
		float sample_volume_l = 0;
		float sample_volume_r = 0;
		for (INT32 i = 0; i < 6; ++i) {
			float l = channels[i] * chan_l[i];
			float r = channels[i] * chan_r[i];
			if (l >= -2. && l <= 2)
				sample_volume_l += l;
			if (r >= -2. && r <= 2)
				sample_volume_r += r;
		}

		sample_volume_l *= 0.25;
		sample_volume_r *= 0.25;
		sample_volume_l *= master_left;
		sample_volume_r *= master_right;

		if (sample_volume_l >  1.0)
			sample_volume_l =  1;
		if (sample_volume_r >  1.0)
			sample_volume_r =  1;
		if (sample_volume_l < -1.0)
			sample_volume_l = -1;
		if (sample_volume_r < -1.0)
			sample_volume_r = -1;
		if (!(audio->capacitor_l<2 && audio->capacitor_l>-2))
			audio->capacitor_l = 0;
		if (!(audio->capacitor_r<2 && audio->capacitor_r>-2))
			audio->capacitor_r = 0;
		float out_l = sample_volume_l - audio->capacitor_l;
		float out_r = sample_volume_r - audio->capacitor_r;
		audio->capacitor_l = (sample_volume_l - out_l) * 0.996;
		audio->capacitor_r = (sample_volume_r - out_r) * 0.996;
		if (emu->capture_audio) {
			UINT32 write_entry0 = (emu->audio_ring_buff.write_ptr++) % SB_AUDIO_RING_BUFFER_SIZE;
			UINT32 write_entry1 = (emu->audio_ring_buff.write_ptr++) % SB_AUDIO_RING_BUFFER_SIZE;
			emu->audio_ring_buff.data[write_entry0] = (INT16)(out_l * 32760);
			emu->audio_ring_buff.data[write_entry1] = (INT16)(out_r * 32760);
		}
	}
}

// Scheduler-driven audio settle on a 512-cycle grid; self-corrects via cycles_late
#define GBA_AUDIO_EVENT_INTERVAL 512
static inline void gba_audio_event(gba_t* gba, sb_emu_state_t* emu, UINT32 cycles_late)
{
	gba_tick_audio(gba, emu, (double)GBA_AUDIO_EVENT_INTERVAL / (16 * 1024 * 1024), GBA_AUDIO_EVENT_INTERVAL);
	gba_timing_schedule(gba, &gba->audio_event, GBA_AUDIO_EVENT_INTERVAL - (INT32)cycles_late);
}

#undef sb_compute_next_sweep_freq
#undef sb_tick_frame_sweep
#undef sb_tick_frame_seq
#undef sb_process_audio_writes
#undef sb_process_audio
#undef sb_frame_sequencer_t
#undef sb_audio_t
#undef sb_gb_t
#undef sb_read8_io 
#undef sb_store8_io
#undef sb_bandlimited_square
#undef sb_gbc_enable
#undef sb_read_wave_ram

#undef SB_IO_AUD1_TONE_SWEEP   
#undef SB_IO_AUD1_LENGTH_DUTY  
#undef SB_IO_AUD1_VOL_ENV      
#undef SB_IO_AUD1_FREQ         
#undef SB_IO_AUD1_FREQ_HI      
#undef SB_IO_AUD2_LENGTH_DUTY  
#undef SB_IO_AUD2_VOL_ENV      
#undef SB_IO_AUD2_FREQ         
#undef SB_IO_AUD2_FREQ_HI      
#undef SB_IO_AUD3_POWER        
#undef SB_IO_AUD3_LENGTH       
#undef SB_IO_AUD3_VOL          
#undef SB_IO_AUD3_FREQ         
#undef SB_IO_AUD3_FREQ_HI      
#undef SB_IO_AUD3_WAVE_BASE    
#undef SB_IO_AUD4_LENGTH       
#undef SB_IO_AUD4_VOL_ENV      
#undef SB_IO_AUD4_POLY         
#undef SB_IO_AUD4_COUNTER      
#undef SB_IO_MASTER_VOLUME     
#undef SB_IO_SOUND_OUTPUT_SEL  
#undef SB_IO_SOUND_ON_OFF     

#undef GBA_AUDIO 

// END GB REUSE CODE SHIM

#endif
