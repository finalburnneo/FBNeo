#pragma once

#ifndef GBA_TIMER_H
#define GBA_TIMER_H

#include "gba.h"

static inline void gba_compute_timers(gba_t* gba)
{
	// event fires count the firing cycle; register accesses stop one short
	bool   from_event       = !gba->timer_event.active;
	UINT32 pos              = gba->global_timer + (from_event ? 1u : 0u);
	UINT32 old_global_timer = gba->timer_settle_clock + 1;
	INT32  ticks            = (INT32)(pos - old_global_timer);
	gba->timer_settle_clock = pos - 1;

	INT32 last_timer_overflow      = 0;
	INT32 timer_ticks_before_event = 32768;
	const INT32 prescaler_lookup[] = { 0,6,8,10 };
	for (INT32 t = 0; t < 4; ++t) {
		UINT16 tm_cnt_h = gba_io_read16(gba, GBA_TM0CNT_H + t * 4);
		bool enable = SB_BFE(tm_cnt_h, 7, 1);
		if (enable) {
			UINT16 prescale = SB_BFE(tm_cnt_h, 0, 2);
			bool   count_up = SB_BFE(tm_cnt_h, 2, 1) && t != 0;
			bool   irq_en   = SB_BFE(tm_cnt_h, 6, 1);
			UINT16 value    = gba_io_read16(gba, GBA_TM0CNT_L + t * 4);
			if (enable != gba->timers[t].last_enable && enable) {
				gba->timers[t].startup_delay = 2;
				value = gba->timers[t].reload_value;
				gba_io_store16(gba, GBA_TM0CNT_L + t * 4, value);
			}
			if (gba->timers[t].startup_delay >= 0) {
				gba->timers[t].startup_delay -= ticks;
				gba->timers[t].last_enable    = enable;
				if (gba->timers[t].startup_delay >= 0) {
					if (gba->timers[t].startup_delay < timer_ticks_before_event)timer_ticks_before_event = gba->timers[t].startup_delay;
					continue;
				}
				gba->timers[t].startup_delay = -1;
			}
			if (count_up) {
				if (last_timer_overflow) {
					UINT32 v = value;
					v += last_timer_overflow;
					last_timer_overflow = 0;
					while (v > 0xffff) {
						v = (v + gba->timers[t].reload_value) - 0x10000;
						last_timer_overflow++;
					}
					value = v;
				}
			} else {
			last_timer_overflow = 0;
			INT32 prescale_duty = prescaler_lookup[prescale];

			INT32 increment = (INT32)((pos >> prescale_duty) - (old_global_timer >> prescale_duty));
			INT32 v         = value + increment;
			while (v > 0xffff) {
				v = (v + gba->timers[t].reload_value) - 0x10000;
				last_timer_overflow++;
			}
			value = v;
			INT32 ticks_before_overflow = (int)(0xffff - value) << (prescale_duty);
			if (ticks_before_overflow < timer_ticks_before_event)
				timer_ticks_before_event = ticks_before_overflow;
		}
		if (last_timer_overflow) {
			UINT16 soundcnt_h = gba_io_read16(gba, GBA_SOUNDCNT_H);
			if (t < 2) {
				for (INT32 i = 0; i < 2; ++i) {
					INT32 timer = SB_BFE(soundcnt_h, 10 + i * 4, 1);
					if (timer != t)
						continue;
					INT32 samples_to_pop = last_timer_overflow;
					INT32 size = (gba->audio.fifo[i].write_ptr - gba->audio.fifo[i].read_ptr) & 0x1f;
					while (samples_to_pop-- && size) {
						gba->audio.fifo[i].read_ptr = (gba->audio.fifo[i].read_ptr + 1) & 0x1f;
						--size;
					}
					if (size < GBA_AUDIO_DMA_ACTIVATE_THRESHOLD) {
						gba->dma[i + 1].activate_audio_dma = gba->activate_dmas = true;
					}
				}
			}
			if (irq_en) {
				UINT16 if_bit = 1 << (GBA_INT_TIMER0 + t);
				gba_send_interrupt(gba, 4, if_bit);
			}
		}
		gba->timers[t].reload_value = gba->timers[t].pending_reload_value;

		gba_io_store16(gba, GBA_TM0CNT_L + t * 4, value);
		} else
			last_timer_overflow = 0;
		gba->timers[t].last_enable = enable;
	}
	// horizon of 0 settles one cycle later, matching the per-cycle recompute cadence
	INT32 when_rel = (timer_ticks_before_event > 0 ? timer_ticks_before_event : 1) + (from_event ? 0 : -1);
	gba_timing_deschedule(gba, &gba->timer_event);
	gba_timing_schedule(gba, &gba->timer_event, when_rel);
}

static inline void gba_timer_event(gba_t* gba, sb_emu_state_t* /*emu*/, UINT32 /*cycles_late*/)
{
	gba_compute_timers(gba);
}

static inline void gba_send_interrupt(gba_t* gba, INT32 pipe_stage, INT32 if_bit)
{
	if (if_bit) {
		gba->active_if_pipe_stages |= 1 << pipe_stage;
		gba->pipelined_if[pipe_stage]   |= if_bit;
	}
}

static inline void gba_tick_interrupts(gba_t* gba)
{
	if (SB_UNLIKELY(gba->active_if_pipe_stages)) {
		UINT16 if_bit = gba->pipelined_if[0];
		if (if_bit) {
			UINT16 if_val = gba_io_read16(gba, GBA_IF);
			if_val |= if_bit;
			gba_io_store16(gba, GBA_IF, if_val);
			gba_update_interrupt_pending(gba);
		}
		gba->pipelined_if[0] = gba->pipelined_if[1];
		gba->pipelined_if[1] = gba->pipelined_if[2];
		gba->pipelined_if[2] = gba->pipelined_if[3];
		gba->pipelined_if[3] = gba->pipelined_if[4];
		gba->pipelined_if[4] = 0;
		gba->active_if_pipe_stages >>= 1;
	}
}

#endif
