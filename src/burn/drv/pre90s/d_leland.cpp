// FinalBurn Neo Cinematronics Leland driver module
// Based on driver by Aaron Giles and Paul Leaman

#include "tiles_generic.h"
#include "z80_intf.h"
#include "nec_intf.h"
#include "ay8910.h"
#include "burn_ym2151.h"
#include "eeprom.h"
#include "burn_gun.h"
#include "bitswap.h"

enum { REG_MASTER = 0, REG_SLAVE, REG_SOUND, REG_EXT, REG_BGGFX, REG_BGPROM, REG_XROM, REG_EEPROM, REG_MAX };

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvRegion[REG_MAX];
static UINT8 *DrvTiles;
static UINT8 *DrvMainRAM;
static UINT8 *DrvSlaveRAM;
static UINT8 *DrvBatteryRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvQRAM;
static UINT8 *DrvTRAM;
static UINT8 *DrvSoundRAM;
static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 is_ataxx;
static INT32 tile_mask;			// tile_count - 1 when it's a power of 2, else -1
static INT32 tile_count;
static INT32 region_size[REG_MAX];

static INT32 master_cycles;		// per frame
static INT32 sound_cycles;		// per frame (also the mixer timebase)

static UINT8 DrvJoy[8][8];
static UINT8 DrvFake[4][4];		// digital up/down/left/right for analog controls
static UINT8 DrvInputs[8];
static UINT8 DrvReset;
static INT16 DrvAnalogX[4];
static INT16 DrvAnalogY[4];
static INT16 DrvAnalogP[4];
static UINT8 analog_value[6];
static UINT8 impulse_prev;
static INT32 trackball_count;

// master/slave state
static UINT8 master_bank;		// ataxx master bank
static UINT8 sound_port_bank;
static UINT8 top_board_bank;
static UINT8 alternate_bank;
static UINT8 battery_enable;
static UINT8 palette_enable;
static INT32 slave_bank_offset;
static UINT8 dac_control;
static UINT8 gfxbank;
static UINT16 xscroll;
static UINT16 yscroll;
static UINT8 analog_result;
static UINT8 dial_last_input[4];
static UINT8 dial_last_result[4];
static INT32 dangerz_x;
static INT32 dangerz_y;
static UINT8 keycard_shift;
static UINT8 keycard_bit;
static UINT8 keycard_state;
static UINT8 keycard_clock;
static UINT8 keycard_command[3];
static UINT16 xrom_addr[2];
static UINT8 irq_line;
// synchronization
enum { PENDING_NONE = 0, PENDING_VRAM, PENDING_SLAVE_LELAND, PENDING_SLAVE_ATAXX };
static UINT8 pending_kind;
static UINT8 pending_offset;
static UINT8 pending_data;

struct vram_state_data
{
	UINT8  buffer;
	UINT16 addr;
	UINT8  latch[2];
};

static vram_state_data vram_state[2];

struct LelandRomLoad
{
	INT32 rom;		// rom index, -1 ends the list
	INT32 region;
	INT32 mode;		// 0 = contiguous, 1 = every other byte
	INT32 src;		// offset in the rom file
	INT32 dst;		// offset in the region
	INT32 len;
};

struct LelandRegions
{
	INT32 size[REG_MAX];
	INT32 erase_ff;	// bitmask of regions filled with 0xff
};

enum { SND_NONE = 0, SND_REDLINE, SND_LELAND, SND_ATAXX, SND_WSF };
enum { BANK_CERBERUS = 0, BANK_MAYHEM, BANK_DANGERZ, BANK_BASEBAL2, BANK_REDLINE, BANK_VIPER, BANK_OFFROAD, BANK_ATAXX };
enum { SLAVE_SMALL = 0, SLAVE_LARGE, SLAVE_ATAXX, SLAVE_ASYLUM };
// analog port sources: (type << 4) | player
enum { AN_NONE = 0, AN_STICK_X, AN_STICK_Y, AN_STICK_X_ALLEY, AN_PEDAL, AN_DIAL, AN_TRACK_X, AN_TRACK_Y };
#define AN(type, player)	(((type) << 4) | (player))

// game specific master port reads
enum { OVR_END = 0, OVR_INPUT, OVR_CERBERUS_DIAL, OVR_DANGERZ_UPPER, OVR_DANGERZ_Y, OVR_DANGERZ_X, OVR_REDLINE_PEDAL, OVR_DIAL, OVR_INDYHEAT };

struct LelandPortOverride
{
	UINT8 port;
	UINT8 type;
	UINT8 a;
	UINT8 b;
};

struct LelandConfig
{
	INT32 sound;
	INT32 bank;
	INT32 slave;
	INT32 nports;
	UINT8 mvram_base[2];
	UINT8 io_base[2];
	INT32 rotate_master;
	INT32 rotate_slave;
	INT32 alleymas_kludge;
	const LelandPortOverride *overrides;
	UINT8 analog[6];
	UINT8 input_init[8];
	UINT8 impulse_mask;		// IN0 bits that are coin pulses
};

static const LelandConfig *cfg;
static const LelandRomLoad *rom_load;
static const LelandRegions *rom_regions;
static const LelandPortOverride *port_override[0x100];	// built from cfg->overrides
static INT32 has_indyheat;

// Intel 8254 PIT (sound board)
#define SOUND_CPU_CLOCK		8000000
#define PIT_NEVER			0xffffffff

struct pit_counter
{
	UINT16 value;
	UINT16 count;
	UINT16 latch;
	UINT8  lowcount;
	UINT8  control;
	UINT8  status;
	UINT8  phase;
	UINT8  gate;
	UINT8  output;
	UINT8  output_pin;
	UINT8  null_count;
	UINT8  rmsb;
	UINT8  wmsb;
	UINT8  latched_count;
	UINT8  latched_status;
	UINT64 last_tick;
	UINT32 to_output;	// pit clocks after last_tick until the next output change
};

static pit_counter pit[3][3];
static INT32 pit_num;
static INT32 pit_clock;
static UINT64 sound_frame_base;	// absolute sound cpu cycles at the start of the frame

static INT32 sound_mute;

static void pit_output_changed(INT32 chip, INT32 counter, INT32 state);

static INT32 sound_cycle_offset;	// keeps the frame position when the 80186 is reset (its counter restarts at 0)

static inline INT32 sound_pos()
{
	return VezTotalCycles() + sound_cycle_offset;
}

static inline UINT64 sound_abs_cycles()
{
	return sound_frame_base + sound_pos();
}

static inline UINT64 pit_now()
{
	return (sound_abs_cycles() * pit_clock) / SOUND_CPU_CLOCK;
}

#define CTRL_ACCESS(control)	(((control) >> 4) & 0x03)
#define CTRL_MODE(control)		(((control) >> 1) & (((control) & 0x04) ? 0x03 : 0x07))
#define CTRL_BCD(control)		(((control) >> 0) & 0x01)

static inline UINT32 pit_adjusted_count(pit_counter *c)
{
	UINT16 val = c->value;

	if (!CTRL_BCD(c->control))
		return (val == 0) ? 0x10000 : val;
	else if (val == 0)
		return 10000;

	return ((val>>12) & 0xF) * 1000 + ((val>> 8) & 0xF) * 100 + ((val>> 4) & 0xF) * 10 + ((val>> 0) & 0xF);
}

static void pit_decrease_counter_value(pit_counter *c, INT64 cycles)
{
	if (CTRL_BCD(c->control) == 0)
	{
		c->value -= (cycles & 0xffff);
		return;
	}

	UINT8 units     =  c->value        & 0xf;
	UINT8 tens      = (c->value >>  4) & 0xf;
	UINT8 hundreds  = (c->value >>  8) & 0xf;
	UINT8 thousands = (c->value >> 12) & 0xf;

	if (cycles <= units)
	{
		units -= cycles;
	}
	else
	{
		cycles -= units;
		units = (10 - cycles % 10) % 10;

		cycles = (cycles + 9) / 10;
		if (cycles <= tens)
		{
			tens -= cycles;
		}
		else
		{
			cycles -= tens;
			tens = (10 - cycles % 10) % 10;

			cycles = (cycles + 9) / 10;
			if (cycles <= hundreds)
			{
				hundreds -= cycles;
			}
			else
			{
				cycles -= hundreds;
				hundreds = (10 - cycles % 10) % 10;
				cycles = (cycles + 9) / 10;
				thousands = (10 + thousands - cycles % 10) % 10;
			}
		}
	}

	c->value = (thousands << 12) | (hundreds << 8) | (tens << 4) | units;
}

static void pit_set_output(INT32 chip, INT32 counter, INT32 output)
{
	pit_counter *c = &pit[chip][counter];

	if (output != c->output)
	{
		c->output = output;

		if (c->output_pin != output)
		{
			c->output_pin = output;
			pit_output_changed(chip, counter, output);
		}
	}
}

static inline void pit_load_counter_value(pit_counter *c)
{
	c->value = c->count;
	c->null_count = 0;
}

static void pit_simulate(INT32 chip, INT32 counter, INT64 elapsed_cycles)
{
	pit_counter *c = &pit[chip][counter];
	UINT32 adjusted_value;
	INT32 mode = CTRL_MODE(c->control);
	UINT32 cycles_to_output = 0;

	switch (mode)
	{
		case 0:
			if (c->phase == 0)
			{
				cycles_to_output = PIT_NEVER;
			}
			else
			{
				if (elapsed_cycles >= 0 && c->phase == 1)
				{
					if (elapsed_cycles > 0)
					{
						--elapsed_cycles;
						c->phase = 2;
					}
					pit_load_counter_value(c);
				}

				if (c->gate == 0)
				{
					cycles_to_output = PIT_NEVER;
				}
				else
				{
					if (c->phase == 2)
					{
						adjusted_value = pit_adjusted_count(c);
						if (elapsed_cycles >= adjusted_value)
						{
							elapsed_cycles -= adjusted_value;
							c->phase = 3;
							c->value = 0;
							pit_set_output(chip, counter, 1);
						}
					}

					pit_decrease_counter_value(c, elapsed_cycles);

					switch (c->phase)
					{
						case 1:  cycles_to_output = 1; break;
						case 2:  cycles_to_output = pit_adjusted_count(c); break;
						case 3:  cycles_to_output = pit_adjusted_count(c); break;
					}
				}
			}
			break;

		case 1:
			if (elapsed_cycles >= 0 && c->phase == 1)
			{
				if (elapsed_cycles > 0)
				{
					--elapsed_cycles;
					c->phase = 2;
				}
				pit_load_counter_value(c);
				pit_set_output(chip, counter, 0);
			}

			if (c->phase == 2)
			{
				adjusted_value = pit_adjusted_count(c);
				if (elapsed_cycles >= adjusted_value)
				{
					c->phase = 3;
					pit_set_output(chip, counter, 1);
				}
			}

			pit_decrease_counter_value(c, elapsed_cycles);

			switch (c->phase)
			{
				case 1:   cycles_to_output = 1; break;
				case 2:   cycles_to_output = pit_adjusted_count(c); break;
				default:  cycles_to_output = PIT_NEVER; break;
			}
			break;

		case 2:
			if (c->gate == 0 || c->phase == 0)
			{
				pit_set_output(chip, counter, 1);
				cycles_to_output = PIT_NEVER;
			}
			else
			{
				if (elapsed_cycles >= 0 && c->phase == 1)
				{
					if (elapsed_cycles > 0)
					{
						--elapsed_cycles;
						c->phase = 2;
					}
					pit_load_counter_value(c);
				}

				adjusted_value = pit_adjusted_count(c);

				do
				{
					if (c->phase == 2)
					{
						if (elapsed_cycles + 1 >= adjusted_value)
						{
							c->phase = 3;
							pit_set_output(chip, counter, 0);
						}
					}

					if (elapsed_cycles >= adjusted_value && c->phase == 3)
					{
						elapsed_cycles -= adjusted_value;
						c->phase = 2;
						pit_load_counter_value(c);
						adjusted_value = pit_adjusted_count(c);
						pit_set_output(chip, counter, 1);
					}
				}
				while (elapsed_cycles >= adjusted_value);

				pit_decrease_counter_value(c, elapsed_cycles);

				switch (c->phase)
				{
					case 1:   cycles_to_output = 1; break;
					default:  cycles_to_output = (c->value == 1) ? 1 : (pit_adjusted_count(c) - 1); break;
				}
			}
			break;

		case 3:
			if (c->gate == 0 || c->phase == 0)
			{
				pit_set_output(chip, counter, 1);
				cycles_to_output = PIT_NEVER;
			}
			else
			{
				if (elapsed_cycles >= 0 && c->phase == 1)
				{
					if (elapsed_cycles > 0)
					{
						--elapsed_cycles;
						c->phase = 2;
					}
					pit_load_counter_value(c);
				}

				if (elapsed_cycles > 0)
				{
					adjusted_value = pit_adjusted_count(c);

					do
					{
						if (c->phase == 2 && elapsed_cycles >= ((adjusted_value + 1) >> 1))
						{
							elapsed_cycles -= ((adjusted_value + 1) >> 1);
							c->phase = 3;
							pit_load_counter_value(c);
							adjusted_value = pit_adjusted_count(c);
							pit_set_output(chip, counter, 0);
						}

						if (c->phase == 3 && elapsed_cycles >= (adjusted_value >> 1))
						{
							elapsed_cycles -= (adjusted_value >> 1);
							c->phase = 2;
							pit_load_counter_value(c);
							adjusted_value = pit_adjusted_count(c);
							pit_set_output(chip, counter, 1);
						}
					}
					while ((c->phase == 2 && elapsed_cycles >= ((adjusted_value + 1) >> 1)) ||
							(c->phase == 3 && elapsed_cycles >= (adjusted_value >> 1)));

					pit_decrease_counter_value(c, elapsed_cycles * 2);
				}

				switch (c->phase)
				{
					case 1:  cycles_to_output = 1; break;
					case 2:  cycles_to_output = (pit_adjusted_count(c) + 1) >> 1; break;
					case 3:  cycles_to_output = pit_adjusted_count(c) >> 1; break;
				}
			}
			break;

		case 4:
		case 5:
			if (c->gate == 0 && mode == 4)
			{
				cycles_to_output = PIT_NEVER;
			}
			else
			{
				if (elapsed_cycles >= 0 && c->phase == 1)
				{
					if (elapsed_cycles > 0)
					{
						--elapsed_cycles;
						c->phase = 2;
					}
					pit_load_counter_value(c);
				}

				if (c->value == 0 && c->phase == 2)
					adjusted_value = 0;
				else
					adjusted_value = pit_adjusted_count(c);

				if (c->phase == 2 && elapsed_cycles >= adjusted_value)
				{
					elapsed_cycles -= adjusted_value;
					c->phase = 3;
					c->value = 0;
					pit_set_output(chip, counter, 0);
				}

				if (elapsed_cycles > 0 && c->phase == 3)
				{
					--elapsed_cycles;
					c->phase = 0;
					pit_decrease_counter_value(c, 1);
					pit_set_output(chip, counter, 1);
				}

				pit_decrease_counter_value(c, elapsed_cycles);

				switch (c->phase)
				{
					case 1:  cycles_to_output = 1; break;
					case 2:  cycles_to_output = pit_adjusted_count(c); break;
					case 3:  cycles_to_output = 1; break;
					default: cycles_to_output = PIT_NEVER; break;
				}
			}
			break;
	}

	c->to_output = cycles_to_output;
}

static void pit_update_at(INT32 chip, INT32 counter, UINT64 now)
{
	pit_counter *c = &pit[chip][counter];
	INT64 elapsed = 0;

	if (now > c->last_tick)
	{
		elapsed = now - c->last_tick;
		c->last_tick = now;
	}

	pit_simulate(chip, counter, elapsed);
}

static void pit_update(INT32 chip, INT32 counter)
{
	pit_update_at(chip, counter, pit_now());
}

// counters are simulated lazily (on access), so between cpu slices only the
// ones whose output is due to change need to be brought up to date.
// "due" is one tick early: mode 2 can drop its output with no elapsed time
// once the count sits at 1 (to_output == 1)
static void pit_update_due()
{
	UINT64 now = pit_now();

	for (INT32 i = 0; i < pit_num; i++)
	{
		for (INT32 j = 0; j < 3; j++)
		{
			pit_counter *c = &pit[i][j];
			if (c->to_output != PIT_NEVER && c->last_tick + c->to_output <= now + 1)
				pit_update_at(i, j, now);
		}
	}
}

// sound cpu cycles until the next pit output change
static INT32 pit_next_event()
{
	UINT64 now = sound_abs_cycles();
	INT64 next = 0x7fffffff;

	for (INT32 i = 0; i < pit_num; i++)
	{
		for (INT32 j = 0; j < 3; j++)
		{
			pit_counter *c = &pit[i][j];
			if (c->to_output == PIT_NEVER) continue;

			UINT64 tick = c->last_tick + c->to_output;
			UINT64 cyc = (tick * SOUND_CPU_CLOCK + pit_clock - 1) / pit_clock;
			INT64 rel = (INT64)(cyc - now);
			if (rel < 1) rel = 1;
			if (rel < next) next = rel;
		}
	}

	return (INT32)next;
}

static void pit_readback(pit_counter *c, INT32 command)
{
	if ((command & 1) == 0)
	{
		if (!c->latched_status)
		{
			c->status = (c->control & 0x3f) | ((c->output != 0) ? 0x80 : 0) | (c->null_count ? 0x40 : 0);
			c->latched_status = 1;
		}
	}

	if ((command & 2) == 0 && !c->rmsb)
	{
		if (c->latched_count == 0)
		{
			UINT16 value = (CTRL_MODE(c->control) == 3) ? (c->value & 0xfffe) : c->value;

			switch (CTRL_ACCESS(c->control))
			{
				case 1:
					c->latch = ((value << 8) & 0xff00) | (value & 0xff);
					c->latched_count = 1;
					break;

				case 2:
					c->latch = (value & 0xff00) | ((value >> 8) & 0xff);
					c->latched_count = 1;
					break;

				case 3:
					c->latch = value;
					c->latched_count = 2;
					break;
			}
		}
	}
}

static void pit_load_count(INT32 chip, INT32 counter, UINT16 newcount)
{
	pit_counter *c = &pit[chip][counter];
	INT32 mode = CTRL_MODE(c->control);

	if (newcount == 1)
	{
		// count of 1 is illegal in modes 2 and 3
		if (mode == 2)
			newcount = 2;
		if (mode == 3)
			newcount = 0;
	}

	c->count = newcount;

	if (mode == 2 || mode == 3)
	{
		if (c->phase == 0)
			c->phase = 1;
	}
	else
	{
		if (mode == 0 || mode == 4)
			c->phase = 1;
	}
}

static UINT8 pit_read(INT32 chip, INT32 offset)
{
	offset &= 3;
	if (offset == 3) return 0xff;

	pit_update(chip, offset);

	pit_counter *c = &pit[chip][offset];
	UINT8 data;

	if (c->latched_status)
	{
		data = c->status;
		c->latched_status = 0;
	}
	else if (c->latched_count != 0)
	{
		data = (c->latch >> (c->rmsb ? 8 : 0)) & 0xff;
		c->rmsb = !c->rmsb;
		--c->latched_count;
	}
	else
	{
		UINT16 value = (CTRL_MODE(c->control) == 3) ? (c->value & 0xfffe) : c->value;

		switch (CTRL_ACCESS(c->control))
		{
			case 1:
				data = value & 0xff;
				break;

			case 2:
				data = value >> 8;
				break;

			case 3:
				if (c->wmsb)
					data = ~c->lowcount;
				else
					data = value >> (c->rmsb ? 8 : 0);
				c->rmsb = !c->rmsb;
				break;

			default:
				data = 0;
				break;
		}
	}

	return data;
}

static void pit_write(INT32 chip, INT32 offset, UINT8 data)
{
	offset &= 3;

	if (offset == 3)
	{
		INT32 timer = (data >> 6) & 3;

		if (timer == 3)
		{
			// readback command
			for (INT32 t = 0; t < 3; t++)
			{
				if (BIT(data, t + 1))
				{
					pit_update(chip, t);
					pit_readback(&pit[chip][t], (data >> 4) & 3);
				}
			}
		}
		else
		{
			pit_update(chip, timer);
			pit_counter *c = &pit[chip][timer];

			if (CTRL_ACCESS(data) == 0)
			{
				pit_readback(c, 1);
			}
			else
			{
				c->control = data & 0x3f;
				c->null_count = 1;
				c->wmsb = c->rmsb = 0;
				c->phase = 0;
				pit_set_output(chip, timer, CTRL_MODE(c->control) ? 1 : 0);
				c->to_output = PIT_NEVER;
			}
		}
	}
	else
	{
		pit_update(chip, offset);
		pit_counter *c = &pit[chip][offset];

		switch (CTRL_ACCESS(c->control))
		{
			case 1:
				pit_load_count(chip, offset, data);
				pit_simulate(chip, offset, 0);
				if (CTRL_MODE(c->control) == 0)
					pit_set_output(chip, offset, 0);
				break;

			case 2:
				pit_load_count(chip, offset, data << 8);
				pit_simulate(chip, offset, 0);
				if (CTRL_MODE(c->control) == 0)
					pit_set_output(chip, offset, 0);
				break;

			case 3:
				if (c->wmsb)
				{
					pit_load_count(chip, offset, c->lowcount | (data << 8));
					pit_simulate(chip, offset, 0);
				}
				else
				{
					c->lowcount = data;
					if (CTRL_MODE(c->control) == 0)
					{
						c->phase = 0;
						pit_set_output(chip, offset, 0);
						c->to_output = PIT_NEVER;
					}
				}
				c->wmsb = !c->wmsb;
				break;
		}
	}

	// next output change may be earlier now
	VezRunEnd();
}

static void pit_reset()
{
	UINT64 now = pit_now();

	for (INT32 i = 0; i < 3; i++)
	{
		for (INT32 j = 0; j < 3; j++)
		{
			pit_counter *c = &pit[i][j];
			memset(c, 0, sizeof(pit_counter));
			c->control = c->status = 0x30;
			c->output = 2;
			c->output_pin = 2;
			c->null_count = 1;
			c->gate = 1;
			c->last_tick = now;
			c->to_output = PIT_NEVER;
		}
	}
}

// DAC stuff
static INT32 *mix_buffer;
static INT32 mix_pos;
static INT32 mix_sample;
static INT32 mix_start;			// sound cycle where mix_sample begins
static INT32 mix_boundary;		// sound cycle where mix_sample ends
static INT64 mix_acc;
static INT32 mix_level_vram;	// first generation dacs fed from video ram
static INT32 mix_level_board;	// 80186 sound board dacs
static INT32 dc_last_in;
static INT32 dc_last_out;

static inline INT32 mixer_sample_pos(INT32 sample)
{
	return (INT32)(((INT64)sample * sound_cycles) / nBurnSoundLen);
}

// the sound cycle count (sound_cycles per frame) is the mixer timebase
static void mixer_update(INT32 pos)
{
	if (pBurnSoundOut == NULL) return;

	if (pos > sound_cycles) pos = sound_cycles;

	INT32 level = mix_level_vram + mix_level_board;

	while (mix_pos < pos && mix_sample < nBurnSoundLen)
	{
		INT32 end = (pos < mix_boundary) ? pos : mix_boundary;

		mix_acc += (INT64)level * (end - mix_pos);
		mix_pos = end;

		if (mix_pos >= mix_boundary)
		{
			INT32 len = mix_boundary - mix_start;
			mix_buffer[mix_sample++] = (len > 0) ? (INT32)(mix_acc / len) : level;
			mix_acc = 0;
			mix_start = mix_boundary;
			mix_boundary = mixer_sample_pos(mix_sample + 1);
		}
	}
}

static void mixer_begin()
{
	mix_pos = 0;
	mix_sample = 0;
	mix_acc = 0;
	mix_start = 0;
	mix_boundary = (nBurnSoundLen > 0) ? mixer_sample_pos(1) : 0;
}

static void mixer_end()
{
	mixer_update(sound_cycles);

	// remove the dc offset of the dacs
	for (INT32 i = 0; i < nBurnSoundLen; i++)
	{
		INT32 in = mix_buffer[i];
		INT32 out = in - dc_last_in + (INT32)(0.995 * dc_last_out);
		dc_last_in = in;
		dc_last_out = out;
		mix_buffer[i] = out;
	}
}

static UINT8 vdac_value[2];		// first generation, fed from video ram
static UINT8 dac_value[8];
static UINT8 dac_vol[8];
static UINT16 dac9_value;
static INT32 vdac_contrib[2];
static INT32 dac_contrib[8];
static INT32 dac9_contrib;

static inline INT32 mixer_pos_line(INT32 line)
{
	return (INT32)(((INT64)line * sound_cycles) / 256);
}

// 8-bit binary weighted dac, gain 0.0625
static inline INT32 vdac_level(UINT8 data)
{
	return (INT32)((data / 64.0 - 1.0) * 0.0625 * 32767.0);
}

// ad7533, gain 1.0
static inline INT32 dac9_level(UINT16 data)
{
	return (INT32)((data / 512.0 - 1.0) * 32767.0);
}

static void vdac_write(INT32 which, UINT8 data, INT32 pos)
{
	if (vdac_value[which] == data) return;

	mixer_update(pos);
	vdac_value[which] = data;

	mix_level_vram -= vdac_contrib[which];
	vdac_contrib[which] = vdac_level(data);
	mix_level_vram += vdac_contrib[which];
}

static void dac_recalc(INT32 which)
{
	// ad7524 with a binary weighted volume dac as reference, gain 0.2
	double vol = dac_vol[which] / 128.0;
	mix_level_board -= dac_contrib[which];
	dac_contrib[which] = (INT32)(vol * (dac_value[which] / 128.0 - 1.0) * 0.2 * 32767.0);
	mix_level_board += dac_contrib[which];
}

static void dac_write(INT32 which, UINT8 data)
{
	if (dac_value[which] == data) return;

	mixer_update(sound_pos());
	dac_value[which] = data;
	dac_recalc(which);
}

static void dacvol_write(INT32 which, UINT8 data)
{
	if (dac_vol[which] == data) return;

	mixer_update(sound_pos());
	dac_vol[which] = data;
	dac_recalc(which);
}

static void dac9_write(UINT16 data)
{
	mixer_update(sound_pos());
	dac9_value = data & 0x3ff;

	mix_level_board -= dac9_contrib;
	dac9_contrib = dac9_level(dac9_value);
	mix_level_board += dac9_contrib;
}

static void dacs_reset()
{
	mix_level_vram = mix_level_board = 0;

	for (INT32 i = 0; i < 2; i++) {
		vdac_value[i] = 0;
		vdac_contrib[i] = vdac_level(0);
		mix_level_vram += vdac_contrib[i];
	}

	for (INT32 i = 0; i < 8; i++) {
		dac_value[i] = 0;
		dac_vol[i] = 0;
		dac_contrib[i] = 0;
	}

	dac9_value = 0;
	dac9_contrib = 0;
	if (cfg->sound == SND_LELAND || cfg->sound == SND_ATAXX || cfg->sound == SND_WSF) {
		dac9_contrib = dac9_level(0);
		mix_level_board = dac9_contrib;
	}
}

static void dacs_recalc_all()
{
	mix_level_vram = mix_level_board = 0;

	for (INT32 i = 0; i < 2; i++) {
		vdac_contrib[i] = vdac_level(vdac_value[i]);
		mix_level_vram += vdac_contrib[i];
	}

	if (cfg->sound == SND_NONE) return;

	for (INT32 i = 0; i < 8; i++) {
		dac_contrib[i] = 0;
		dac_recalc(i);
	}

	if (cfg->sound != SND_REDLINE) {
		dac9_contrib = dac9_level(dac9_value);
		mix_level_board += dac9_contrib;
	}
}

// 80186 sound board
static UINT8 last_control;
static UINT8 clock_active;
static UINT16 sound_command;
static UINT16 sound_response;
static UINT32 ext_start;
static UINT32 ext_stop;
static UINT8 ext_active;
static UINT16 periph_pacs;
static UINT16 periph_mpcs;
static UINT8 periph_installed;
static UINT8 test_line;			// /TEST input, polled by the WAIT instruction

static inline void set_clock_line(INT32 which, INT32 state)
{
	if (state)
		clock_active |= (1 << which);
	else
		clock_active &= ~(1 << which);
}

static void pit_output_changed(INT32 chip, INT32 counter, INT32 state)
{
	switch ((chip << 2) | counter)
	{
		case 0x00: VezSetIRQLineAndVector(I80186_INPUT_LINE_DRQ0, 0, state ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE); break;
		case 0x01: VezSetIRQLineAndVector(I80186_INPUT_LINE_DRQ1, 0, state ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE); break;
		case 0x02: set_clock_line(2, state); break;
		case 0x04: set_clock_line(3, state); break;
		case 0x05: set_clock_line(4, state); break;
		case 0x06: if (cfg->sound != SND_REDLINE) set_clock_line(5, state); break;
		case 0x08: set_clock_line(5, state); break;
	}
}

static void i186_timer_out(INT32 which, INT32 state)
{
	if (which == 0)
	{
		if (cfg->sound != SND_REDLINE)
			set_clock_line(6, state);
	}
	else if (which == 1 && cfg->sound == SND_WSF)
	{
		if (state && ext_active && ext_start < ext_stop)
		{
			if (region_size[REG_EXT])
				dac_write(3, DrvRegion[REG_EXT][ext_start % region_size[REG_EXT]]);
			ext_start++;
		}
		set_clock_line(3, state);
	}
}

static void dac_w(UINT32 offset, UINT16 data, UINT16 mask)
{
	INT32 dac = offset & 7;

	if (mask & 0x00ff)
	{
		if ((offset & 0x60) == 0x40)
			VezSetIRQLineAndVector(I80186_INPUT_LINE_DRQ0, 0, CPU_IRQSTATUS_NONE);
		else if ((offset & 0x60) == 0x60)
			VezSetIRQLineAndVector(I80186_INPUT_LINE_DRQ1, 0, CPU_IRQSTATUS_NONE);

		dac_write(dac, data & 0xff);
		set_clock_line(dac, 0);
	}

	if (mask & 0xff00)
		dacvol_write(dac, data >> 8);
}

static void redline_dac_w(UINT32 offset, UINT16 data)
{
	// volume comes from the address lines
	data = (data & 0xff) | (offset << 8);
	offset = ((offset >> 8) & 7) | ((offset & 0x2000) ? 0x40 : 0) | ((offset & 0x800) ? 0x20 : 0);
	dac_w(offset, data, 0xffff);
}

static void ataxx_dac_control_w(UINT32 offset, UINT16 data, UINT16 mask)
{
	if (mask & 0x00ff)
	{
		switch (offset & 0x1f)
		{
			case 0x00: dac_w(0x40, data, 0x00ff); return;
			case 0x01: dac_w(0x61, data, 0x00ff); return;
			case 0x02: dac_w(2, data, 0x00ff); return;
			case 0x03:
				dacvol_write(0, (data & 7) << 5);
				dacvol_write(1, ((data >> 3) & 7) << 5);
				dacvol_write(2, ((data >> 6) & 3) << 6);
				return;
		}
	}

	if (cfg->sound == SND_WSF)
	{
		switch (offset)
		{
			case 0x04: ext_active = 1; return;
			case 0x05: ext_active = 0; return;
			case 0x06: ext_start = (((ext_start >> 4) & ~mask) | (data & mask)) << 4; return;
			case 0x07: ext_stop  = (((ext_stop  >> 4) & ~mask) | (data & mask)) << 4; return;
		}
	}
}

static UINT16 peripheral_r(UINT32 offset, UINT16 mask)
{
	INT32 select = offset >> 6;
	offset &= 0x3f;

	switch (select)
	{
		case 0:
			if (cfg->sound != SND_REDLINE)
				return (clock_active >> 1) & 0x3e;
			else
				return (clock_active << 1) & 0x7e;

		case 1:
			return sound_command;

		case 2:
			if (mask & 0x00ff)
				return pit_read(0, offset & 3);
			break;

		case 3:
			if (cfg->sound == SND_LELAND || cfg->sound == SND_REDLINE)
			{
				if (mask & 0x00ff)
					return pit_read(1, offset & 3);
			}
			else if (cfg->sound == SND_WSF)
				return BurnYM2151Read();
			break;

		case 4:
			if (cfg->sound == SND_REDLINE && (mask & 0x00ff))
				return pit_read(2, offset & 3);
			break;
	}

	return 0xffff;
}

static void peripheral_w(UINT32 offset, UINT16 data, UINT16 mask)
{
	INT32 select = offset >> 6;
	offset &= 0x3f;

	switch (select)
	{
		case 1:
			sound_response = data;
			break;

		case 2:
			if (mask & 0x00ff)
				pit_write(0, offset & 3, data);
			break;

		case 3:
			if (cfg->sound == SND_LELAND || cfg->sound == SND_REDLINE)
			{
				if (mask & 0x00ff)
					pit_write(1, offset & 3, data);
			}
			else if (cfg->sound == SND_WSF)
				BurnYM2151Write(offset & 1, data);
			break;

		case 4:
			if (cfg->sound == SND_REDLINE)
			{
				if (mask & 0x00ff)
					pit_write(2, offset & 3, data);
			}
			else if (mask == 0xffff)
			{
				dac9_write(data);
				set_clock_line(6, 0);
			}
			break;

		case 5:
			if (cfg->sound == SND_ATAXX || cfg->sound == SND_WSF)
				ataxx_dac_control_w(offset, data, mask);
			break;
	}
}

// peripheral window decoding (PACS / MPCS)
static inline UINT32 periph_base()
{
	UINT32 base = (periph_pacs & 0xffc0) << 4;
	return (periph_mpcs & 0x40) ? base : (base & 0xffff);
}

static inline INT32 periph_in_mem(UINT32 address)
{
	return periph_installed && (periph_mpcs & 0x40) && address >= periph_base() && address < periph_base() + 0x300;
}

static inline INT32 periph_in_io(UINT32 port)
{
	return periph_installed && !(periph_mpcs & 0x40) && port >= periph_base() && port < periph_base() + 0x300;
}

static void sound_map_memory()
{
	for (INT32 i = 0; i < 0x20000; i += 0x4000)
		VezMapMemory(DrvSoundRAM, i, i + 0x3fff, MAP_RAM);

	VezMapMemory(DrvRegion[REG_SOUND] + 0x20000, 0x20000, 0xfffff, MAP_ROM);

	if (periph_installed && (periph_mpcs & 0x40))
	{
		UINT32 base = periph_base();
		VezMemCallback(base, base + 0x3ff, 0);
		VezMemCallback(base, base + 0x3ff, 1);
	}
}

static void i186_chip_select(INT32 which, UINT16 data)
{
	switch (which)
	{
		case 2:
			periph_pacs = data;
			break;

		case 4:
			periph_mpcs = data;
			periph_installed = 1;
			sound_map_memory();
			break;
	}
}

static UINT8 sound_fallback_read(UINT32 address)
{
	if (address < 0x20000) return DrvSoundRAM[address & 0x3fff];
	return DrvRegion[REG_SOUND][address & 0xfffff];
}

static UINT8 __fastcall sound_read_byte(UINT32 address)
{
	if (periph_in_mem(address))
	{
		UINT32 off = address - periph_base();
		return peripheral_r(off >> 1, (off & 1) ? 0xff00 : 0x00ff) >> ((off & 1) * 8);
	}

	return sound_fallback_read(address);
}

static void __fastcall sound_write_byte(UINT32 address, UINT8 data)
{
	if (periph_in_mem(address))
	{
		UINT32 off = address - periph_base();
		peripheral_w(off >> 1, data << ((off & 1) * 8), (off & 1) ? 0xff00 : 0x00ff);
		return;
	}

	if (address < 0x20000) DrvSoundRAM[address & 0x3fff] = data;
}

static UINT16 sound_read_word(UINT32 address)
{
	if (periph_in_mem(address))
		return peripheral_r((address - periph_base()) >> 1, 0xffff);

	return sound_fallback_read(address) | (sound_fallback_read(address + 1) << 8);
}

static void sound_write_word(UINT32 address, UINT16 data)
{
	if (periph_in_mem(address))
	{
		peripheral_w((address - periph_base()) >> 1, data, 0xffff);
		return;
	}

	sound_write_byte(address, data & 0xff);
	sound_write_byte(address + 1, data >> 8);
}

static UINT8 __fastcall sound_read_port(UINT32 port)
{
	port &= 0xffff;

	if (periph_in_io(port))
	{
		UINT32 off = port - periph_base();
		return peripheral_r(off >> 1, (off & 1) ? 0xff00 : 0x00ff) >> ((off & 1) * 8);
	}

	return 0xff;
}

static void sound_io_write(UINT32 port, UINT16 data, UINT16 mask)
{
	if (periph_in_io(port))
	{
		peripheral_w((port - periph_base()) >> 1, data, mask);
		return;
	}

	switch (cfg->sound)
	{
		case SND_REDLINE: redline_dac_w(port >> 1, data); break;
		case SND_LELAND:  dac_w(port >> 1, data, mask); break;
	}
}

static void __fastcall sound_write_port(UINT32 port, UINT8 data)
{
	port &= 0xffff;
	sound_io_write(port & ~1, data << ((port & 1) * 8), (port & 1) ? 0xff00 : 0x00ff);
}

static UINT16 sound_read_port_word(UINT32 port)
{
	port &= 0xffff;

	if (periph_in_io(port))
		return peripheral_r((port - periph_base()) >> 1, 0xffff);

	return 0xffff;
}

static void sound_write_port_word(UINT32 port, UINT16 data)
{
	sound_io_write(port & 0xffff, data, 0xffff);
}

static void sound_board_reset()
{
	INT32 pos = sound_pos();
	VezReset();
	VezSetRESETLine(0);
	sound_cycle_offset = pos - VezTotalCycles();

	// the cpu reset doesn't change the /TEST input
	VezSetIRQLineAndVector(NEC_INPUT_LINE_POLL, 0, test_line);

	pit_reset();

	if (cfg->sound == SND_WSF)
		BurnYM2151Reset();

	last_control = 0xf8;
	clock_active = 0;
	ext_start = 0;
	ext_stop = 0;
	ext_active = 0;

	if (cfg->sound == SND_WSF)
		dacvol_write(3, 0xff);
}

// run the sound cpu up to the given cycle count (in the current frame)
static void sound_run_to(INT32 target)
{
	while (sound_pos() < target)
	{
		INT32 todo = target - sound_pos();
		INT32 next = pit_next_event();
		if (next < todo) todo = next;

		VezRun(todo);
		pit_update_due();
	}
}

// catch up with the master cpu (must be called from a master cpu handler)
static void sound_sync()
{
	if (cfg->sound == SND_NONE) return;

	INT32 target = (INT32)(((INT64)ZetTotalCycles() * sound_cycles) / master_cycles);
	sound_run_to(target);
}

static void leland_80186_control_w(UINT8 data)
{
	UINT8 diff = (last_control ^ data) & 0xf8;
	if (diff == 0) return;

	sound_sync();
	last_control = data;

	INT32 pos = sound_pos();
	VezSetRESETLine((data & 0x80) ? 0 : 1);
	sound_cycle_offset = pos - VezTotalCycles();

	// WAIT stalls while /TEST is not asserted
	test_line = (data & 0x10) ? 0 : 1;
	VezSetIRQLineAndVector(NEC_INPUT_LINE_POLL, 0, test_line);

	// NMI (bit 6) is left disconnected

	VezSetIRQLineAndVector(I80186_INPUT_LINE_INT0, 0, (data & 0x20) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
	VezSetIRQLineAndVector(I80186_INPUT_LINE_INT1, 0, (data & 0x08) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);

	if ((diff & 0x80) && (data & 0x80))
		sound_board_reset();
}

static void ataxx_80186_control_w(UINT8 data)
{
	UINT8 modified = ((data & 0x01) << 7) | ((data & 0x02) << 5) | ((data & 0x04) << 3) | ((data & 0x08) << 1);
	leland_80186_control_w(modified);
}

static void sound_command_w(INT32 high, UINT8 data)
{
	sound_sync();

	if (high)
		sound_command = (sound_command & 0x00ff) | (data << 8);
	else
		sound_command = (sound_command & 0xff00) | data;
}

static UINT8 sound_response_r()
{
	sound_sync();
	return sound_response & 0xff;
}

static INT32 ym2151_stream_pos(INT32 rate)
{
	return (INT32)(((INT64)sound_pos() * (rate * 100 / nBurnFPS)) / sound_cycles);
}

static INT32 cpu_vpos()
{
	// both z80s run at the same clock
	INT32 vpos = (INT32)(((INT64)ZetTotalCycles() * 256) / master_cycles);
	if (vpos < 0) vpos = 0;
	if (vpos > 255) vpos = 255;
	return vpos;
}

static void video_addr_w(INT32 offset, UINT8 data, INT32 num)
{
	vram_state_data &state = vram_state[num];

	if (!offset)
		state.addr = (state.addr & 0xfe00) | ((data << 1) & 0x01fe);
	else
	{
		state.addr = ((data << 9) & 0xfe00) | (state.addr & 0x01fe);
		state.buffer = BIT(data, 7);
	}
}

static UINT8 vram_port_r(INT32 offset, INT32 num)
{
	vram_state_data &state = vram_state[num];
	UINT16 addr = state.addr;
	INT32 inc = (offset >> 2) & 2;
	UINT8 ret;

	switch (offset & 7)
	{
		case 3:
			ret = DrvVidRAM[addr];
			addr += inc & (addr << 1);
			addr ^= 1;
			break;

		case 5:
			ret = DrvVidRAM[addr | 1];
			addr += inc;
			break;

		case 6:
			ret = DrvVidRAM[addr & ~1];
			addr += inc;
			break;

		default:
			ret = 0;
			break;
	}

	state.addr = addr;
	return ret;
}

static void vram_port_w(INT32 offset, UINT8 data, INT32 num)
{
	vram_state_data &state = vram_state[num];
	UINT16 addr = state.addr;
	INT32 inc = (offset >> 2) & 2;
	INT32 trans = (offset >> 4) & num;

	switch (offset & 7)
	{
		case 1:
			DrvVidRAM[addr & ~1] = state.latch[0];
			DrvVidRAM[addr |  1] = data;
			addr += inc;
			break;

		case 2:
			DrvVidRAM[addr & ~1] = data;
			DrvVidRAM[addr |  1] = state.latch[1];
			addr += inc;
			break;

		case 3:
			if (trans)
			{
				if (!(data & 0xf0)) data |= DrvVidRAM[addr] & 0xf0;
				if (!(data & 0x0f)) data |= DrvVidRAM[addr] & 0x0f;
			}
			DrvVidRAM[addr] = data;
			addr += inc & (addr << 1);
			addr ^= 1;
			break;

		case 5:
			state.latch[1] = data;
			if (trans)
			{
				if (!(data & 0xf0)) data |= DrvVidRAM[addr | 1] & 0xf0;
				if (!(data & 0x0f)) data |= DrvVidRAM[addr | 1] & 0x0f;
			}
			DrvVidRAM[addr | 1] = data;
			addr += inc;
			break;

		case 6:
			state.latch[0] = data;
			if (trans)
			{
				if (!(data & 0xf0)) data |= DrvVidRAM[addr & ~1] & 0xf0;
				if (!(data & 0x0f)) data |= DrvVidRAM[addr & ~1] & 0x0f;
			}
			DrvVidRAM[addr & ~1] = data;
			addr += inc;
			break;
	}

	state.addr = addr;
}

static void apply_pending_write()
{
	UINT8 data = pending_data;

	switch (pending_kind)
	{
		case PENDING_VRAM:
			vram_port_w(pending_offset, data, 0);
			break;

		case PENDING_SLAVE_LELAND:
			ZetSetRESETLine(1, (data & 0x01) ? 0 : 1);
			ZetSetIRQLine(1, 0x20, (data & 0x04) ? CPU_IRQSTATUS_NONE : CPU_IRQSTATUS_ACK);
			ZetSetIRQLine(1, 0, (data & 0x08) ? CPU_IRQSTATUS_NONE : CPU_IRQSTATUS_ACK);
			break;

		case PENDING_SLAVE_ATAXX:
			ZetSetIRQLine(1, 0, (data & 0x01) ? CPU_IRQSTATUS_NONE : CPU_IRQSTATUS_ACK);
			ZetSetIRQLine(1, 0x20, (data & 0x04) ? CPU_IRQSTATUS_NONE : CPU_IRQSTATUS_ACK);
			ZetSetRESETLine(1, (data & 0x10) ? 0 : 1);
			break;
	}

	pending_kind = PENDING_NONE;
}

// the master stops, the write is done once the slave has caught up with it
// (the slave irq is only pulsed for a few cycles by some games)
static void master_defer_write(INT32 kind, INT32 offset, UINT8 data)
{
	if (pending_kind != PENDING_NONE)
		apply_pending_write();

	pending_kind = kind;
	pending_offset = offset;
	pending_data = data;
	ZetRunEnd();
}

static inline INT32 ataxx_vram_offset(INT32 offset)
{
	return ((offset >> 1) & 0x07) | ((offset << 3) & 0x08) | (offset & 0x10);
}

static void scroll_w(INT32 offset, UINT8 data)
{
	switch (offset)
	{
		case 0: xscroll = (xscroll & 0xff00) | data; break;
		case 1: xscroll = (xscroll & 0x00ff) | (data << 8); break;
		case 2: yscroll = (yscroll & 0xff00) | data; break;
		case 3: yscroll = (yscroll & 0x00ff) | (data << 8); break;
	}
}

static void palette_update(INT32 entry)
{
	if (is_ataxx)
	{
		UINT16 d = DrvPalRAM[entry * 2 + 0] | (DrvPalRAM[entry * 2 + 1] << 8);
		INT32 r = (d >> 8) & 0x0f;
		INT32 g = (d >> 4) & 0x0f;
		INT32 b = (d >> 0) & 0x0f;
		DrvPalette[entry] = BurnHighCol(r * 0x11, g * 0x11, b * 0x11, 0);
	}
	else
	{
		UINT8 d = DrvPalRAM[entry];
		INT32 r = (d >> 0) & 7;
		INT32 g = (d >> 3) & 7;
		INT32 b = (d >> 6) & 3;
		r = (r << 5) | (r << 2) | (r >> 1);
		g = (g << 5) | (g << 2) | (g >> 1);
		b = (b << 6) | (b << 4) | (b << 2) | b;
		DrvPalette[entry] = BurnHighCol(r, g, b, 0);
	}
}

static UINT8 dial_compute_value(INT32 new_val, INT32 indx)
{
	INT32 delta = new_val - (INT32)dial_last_input[indx];
	UINT8 result = dial_last_result[indx] & 0x80;

	dial_last_input[indx] = new_val;

	if (delta > 0x80)
		delta -= 0x100;
	else if (delta < -0x80)
		delta += 0x100;

	if (delta < 0)
	{
		result = 0x80;
		delta = -delta;
	}
	else if (delta > 0)
		result = 0x00;

	if (delta > 0x1f)
		delta = 0x1f;
	result |= (dial_last_result[indx] + delta) & 0x1f;

	dial_last_result[indx] = result;
	return result;
}

static void update_dangerz_xy()
{
	UINT8 newy = analog_value[0];
	UINT8 newx = analog_value[1];
	INT32 deltay = newy - dial_last_input[0];
	INT32 deltax = newx - dial_last_input[1];

	if (deltay <= -128) deltay += 256;
	else if (deltay >= 128) deltay -= 256;
	if (deltax <= -128) deltax += 256;
	else if (deltax >= 128) deltax -= 256;

	dangerz_y += deltay;
	dangerz_x += deltax;
	if (dangerz_y < 0) dangerz_y = 0;
	if (dangerz_y > 1023) dangerz_y = 1023;
	if (dangerz_x < 0) dangerz_x = 0;
	if (dangerz_x > 1023) dangerz_x = 1023;

	dial_last_input[0] = newy;
	dial_last_input[1] = newx;
}

static UINT8 read_analog(INT32 which)
{
	return (which < 6) ? analog_value[which] : 0;
}

static UINT8 keycard_r()
{
	UINT8 result = 0;

	if (BIT(keycard_state, 7))
	{
		if (keycard_bit == 1)
			keycard_shift = 0xff;

		result = BIT(~keycard_shift, 0) << ((keycard_state >> 4) & 3);
	}

	return result;
}

static void keycard_w(UINT8 data)
{
	UINT8 new_state = data & 0xb0;
	UINT8 new_clock = BIT(data, 6);

	if (!keycard_state && new_state)
	{
		keycard_command[0] = keycard_command[1] = keycard_command[2] = 0;
	}
	else if (keycard_state && !new_state)
	{
		keycard_command[0] = keycard_command[1] = keycard_command[2] = 0;
	}
	else if (keycard_state == new_state)
	{
		if (!new_clock && keycard_clock)
		{
			keycard_shift >>= 1;
			keycard_bit = (keycard_bit + 1) & 7;
		}
		else if (!new_clock && !keycard_clock && !(data & 0x80))
		{
			keycard_shift &= ~0x80;
			if (BIT(data, ((new_state >> 4) & 3)))
				keycard_shift |= 0x80;

			if (keycard_bit == 7)
			{
				keycard_command[0] = keycard_command[1];
				keycard_command[1] = keycard_command[2];
				keycard_command[2] = keycard_shift;
			}
		}
	}

	keycard_state = new_state;
	keycard_clock = new_clock;
}

static UINT8 *master_bank_ptr(UINT32 offset, UINT32 length)
{
	if (offset + length > (UINT32)region_size[REG_MASTER])
		offset = 0x2000;

	return DrvRegion[REG_MASTER] + offset;
}

static void master_set_banks(UINT32 bank, INT32 battery)
{
	if (cfg->bank <= BANK_BASEBAL2)
	{
		// 2000-dfff banked rom, a000-dfff can be replaced by battery ram
		UINT8 *ptr = master_bank_ptr(bank, 0xc000);
		ZetMapMemory(ptr, 0x2000, 0x9fff, MAP_ROM);

		ZetUnmapMemory(0xa000, 0xdfff, MAP_RAM);
		if (battery)
			ZetMapMemory(DrvBatteryRAM, 0xa000, 0xdfff, MAP_RAM);
		else
			ZetMapMemory(ptr + 0x8000, 0xa000, 0xdfff, MAP_ROM);
	}
	else
	{
		// 2000-9fff banked rom, a000-dfff fixed rom or battery ram
		ZetMapMemory(master_bank_ptr(bank, 0x8000), 0x2000, 0x9fff, MAP_ROM);

		ZetUnmapMemory(0xa000, 0xdfff, MAP_RAM);
		if (battery == 1)
			ZetMapMemory(DrvBatteryRAM, 0xa000, 0xdfff, MAP_RAM);
		else if (battery == 0)
			ZetMapMemory(DrvRegion[REG_MASTER] + 0xa000, 0xa000, 0xdfff, MAP_ROM);
		// battery == 2: ataxx qram, handled by the memory handlers
	}

	battery_enable = battery;
}

static void update_master_bank()
{
	static const UINT32 redline_list[4] = { 0x10000, 0x18000, 0x02000, 0x02000 };
	static const UINT32 viper_list[4]   = { 0x02000, 0x10000, 0x18000, 0x02000 };
	static const UINT32 offroad_list[8] = { 0x02000, 0x02000, 0x10000, 0x18000, 0x20000, 0x28000, 0x30000, 0x38000 };

	switch (cfg->bank)
	{
		case BANK_CERBERUS:
			master_set_banks(0x2000, 0);
			break;

		case BANK_MAYHEM:
			master_set_banks(!BIT(sound_port_bank, 2) ? 0x10000 : 0x1c000, (sound_port_bank & 0x24) == 0);
			break;

		case BANK_DANGERZ:
			master_set_banks(!BIT(alternate_bank, 0) ? 0x02000 : 0x12000, BIT(top_board_bank, 7));
			break;

		case BANK_BASEBAL2:
			if (!BIT(top_board_bank, 7))
				master_set_banks(!BIT(sound_port_bank, 2) ? 0x10000 : 0x1c000, 0);
			else
				master_set_banks(!BIT(top_board_bank, 6) ? 0x28000 : 0x30000, 1);
			break;

		case BANK_REDLINE:
			master_set_banks(redline_list[alternate_bank & 3], (alternate_bank & 3) == 1);
			break;

		case BANK_VIPER:
			master_set_banks(viper_list[alternate_bank & 3], BIT(alternate_bank, 2));
			break;

		case BANK_OFFROAD:
			master_set_banks(offroad_list[alternate_bank & 7], (alternate_bank & 7) == 1);
			break;

		case BANK_ATAXX:
		{
			static const UINT32 ataxx_list[16] = {
				0x02000, 0x18000, 0x20000, 0x28000, 0x30000, 0x38000, 0x40000, 0x48000,
				0x50000, 0x58000, 0x60000, 0x68000, 0x70000, 0x78000, 0x80000, 0x88000
			};

			INT32 view = 0;
			if ((master_bank & 0x30) == 0x10) view = 1;
			else if ((master_bank & 0x30) == 0x20) view = 2;

			master_set_banks(ataxx_list[master_bank & 0x0f], view);
			palette_enable = ((master_bank & 0x30) == 0x30);
		}
		break;
	}
}

static void sound_port_w(UINT32, UINT32 data)
{
	gfxbank = data;
	dac_control = data & 3;
	sound_port_bank = data & 0x24;
	update_master_bank();
}

static UINT8 sound_port_r(UINT32)
{
	return gfxbank;
}

static void alleymas_kludge_w(UINT8 data)
{
	// the game never sets the joystick enable flag at e0ca
	if (ZetGetPrevPC(-1) == 0x1827)
		DrvMainRAM[0x0ca] = 1;
	else
		DrvMainRAM[0x0ca] = data;

	if (DrvBatteryRAM[0] == 0)
	{
		DrvBatteryRAM[0] = 'C';
		DrvBatteryRAM[1] = 'I';
		DrvBatteryRAM[2] = 'N';
	}
}

static void __fastcall leland_master_write(UINT16 address, UINT8 data)
{
	if (address >= 0xe000 && address <= 0xe0ff)
	{
		// only reached on alleymas (page unmapped for writes)
		if (address == 0xe0ca)
			alleymas_kludge_w(data);
		else
			DrvMainRAM[address & 0xfff] = data;
		return;
	}

	if (address >= 0xf000 && address <= 0xf3ff)
	{
		if (palette_enable)
		{
			DrvPalRAM[address & 0x3ff] = data;
			palette_update(address & 0x3ff);
		}
		return;
	}

	if (address == 0xf800 || address == 0xf801)
		video_addr_w(address & 1, data, 0);
}

static UINT8 __fastcall leland_master_read(UINT16 address)
{
	if (address >= 0xf000 && address <= 0xf3ff)
		return palette_enable ? DrvPalRAM[address & 0x3ff] : 0;

	return 0;
}

static UINT8 leland_master_input_r(INT32 offset)
{
	UINT8 result = 0xff;

	switch (offset)
	{
		case 0x00:
			result = DrvInputs[0];
			break;

		case 0x01:
			result = DrvInputs[1];
			if (ZetIsHalted(1)) result ^= 0x01;
			break;

		case 0x02:
		case 0x12:
			ZetSetIRQLine(0x20, CPU_IRQSTATUS_NONE);
			break;

		case 0x03:
		case 0x13:
			result &= AY8910Read(0);
			break;

		case 0x10:
			result = DrvInputs[2];
			break;

		case 0x11:
			result = (DrvInputs[3] & ~0x03) | (EEPROMRead() ? 0x01 : 0) | ((cpu_vpos() >= 240) ? 0 : 0x02);
			break;
	}

	return result;
}

static void eeprom_w(UINT8 data)
{
	EEPROMWriteBit(BIT(data, 4));
	EEPROMSetClockLine(BIT(data, 5) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
	EEPROMSetCSLine(BIT(data, 6) ? EEPROM_CLEAR_LINE : EEPROM_ASSERT_LINE);
}

static void leland_master_output_w(INT32 offset, UINT8 data)
{
	switch (offset)
	{
		case 0x09:
			palette_enable = BIT(data, 1);
			master_defer_write(PENDING_SLAVE_LELAND, 0, data);
			eeprom_w(data);
			break;

		case 0x0a:
		case 0x0b:
			AY8910Write(0, offset & 1, data);
			break;

		case 0x0c:
		case 0x0d:
		case 0x0e:
		case 0x0f:
			scroll_w(offset - 0x0c, data);
			break;
	}
}

static void port_override_init()
{
	memset(port_override, 0, sizeof(port_override));
	has_indyheat = 0;

	for (const LelandPortOverride *o = cfg->overrides; o && o->type != OVR_END; o++)
	{
		if (port_override[o->port] == NULL) port_override[o->port] = o; // first match wins
		if (o->type == OVR_INDYHEAT) has_indyheat = 1;
	}
}

static UINT8 game_port_read(UINT8 port, INT32 *handled)
{
	*handled = 1;

	const LelandPortOverride *o = port_override[port];

	if (o)
	{
		switch (o->type)
		{
			case OVR_INPUT:
				return DrvInputs[o->a];

			case OVR_CERBERUS_DIAL:
			{
				UINT8 modified = dial_compute_value(analog_value[o->b], o->b);
				return (DrvInputs[o->a] & 0xc0) | ((modified & 0x80) >> 2) | (modified & 0x1f);
			}

			case OVR_DANGERZ_UPPER:
				update_dangerz_xy();
				return ((dangerz_y >> 2) & 0xc0) | ((dangerz_x >> 8) & 0x03);

			case OVR_DANGERZ_Y:
				update_dangerz_xy();
				return dangerz_y & 0xff;

			case OVR_DANGERZ_X:
				update_dangerz_xy();
				return dangerz_x & 0xff;

			case OVR_REDLINE_PEDAL:
			{
				static const UINT8 redline_pedal_value[8] = { 0xf0, 0xe0, 0xc0, 0xd0, 0x90, 0xb0, 0x30, 0x70 };
				return redline_pedal_value[DrvInputs[o->a] >> 5] | 0x0f;
			}

			case OVR_DIAL:
				return dial_compute_value(analog_value[o->a], o->b);

			case OVR_INDYHEAT:
				switch (port & 3)
				{
					case 0: return 0;
					case 1: return analog_result;
					case 2: return 0;
				}
				return 0xff;
		}
	}

	*handled = 0;
	return 0;
}

static UINT8 __fastcall leland_master_in(UINT16 port)
{
	port &= 0xff;

	INT32 handled;
	UINT8 ret = game_port_read(port, &handled);
	if (handled) return ret;

	for (INT32 i = cfg->nports - 1; i >= 0; i--)
	{
		if (port >= cfg->mvram_base[i] && port <= cfg->mvram_base[i] + 0x1f)
			return vram_port_r(port & 0x1f, 0);

		if (port >= cfg->io_base[i] && port <= cfg->io_base[i] + 0x1f)
			return leland_master_input_r(port & 0x1f);
	}

	switch (port)
	{
		case 0xf2:
			if (cfg->sound != SND_NONE) return sound_response_r();
			break;

		case 0xfd:
			return analog_result;

		case 0xfe:
			return 0x00;

		case 0xff:
			return keycard_r() & ~0x80;
	}

	return 0;
}

static void __fastcall leland_master_out(UINT16 port, UINT8 data)
{
	port &= 0xff;

	for (INT32 i = cfg->nports - 1; i >= 0; i--)
	{
		if (port >= cfg->mvram_base[i] && port <= cfg->mvram_base[i] + 0x1f)
		{
			master_defer_write(PENDING_VRAM, port & 0x1f, data);
			return;
		}

		if (port >= cfg->io_base[i] && port <= cfg->io_base[i] + 0x0f)
		{
			leland_master_output_w(port & 0x0f, data);
			return;
		}
	}

	switch (port)
	{
		case 0xf0:
			alternate_bank = data & 0x0f;
			update_master_bank();
			if (cfg->sound != SND_NONE)
				leland_80186_control_w(data);
			return;

		case 0xf2:
			if (cfg->sound != SND_NONE) sound_command_w(0, data);
			return;

		case 0xf4:
			if (cfg->sound != SND_NONE) sound_command_w(1, data);
			return;

		case 0xfd:
			return;

		case 0xfe:
			analog_result = read_analog(data & 0x0f);
			top_board_bank = data & 0xc0;
			update_master_bank();
			return;

		case 0xff:
			keycard_w(data);
			return;
	}
}

static void __fastcall ataxx_master_write(UINT16 address, UINT8 data)
{
	if (address >= 0xa000 && address <= 0xdfff)
	{
		if (battery_enable == 2)
			DrvQRAM[((master_bank & 0xc0) << 8) | (address - 0xa000)] = data;
		return;
	}

	if (address >= 0xf800)
	{
		if (palette_enable)
		{
			DrvPalRAM[address & 0x7ff] = data;
			palette_update((address & 0x7ff) >> 1);
			return;
		}

		if (address == 0xfff8 || address == 0xfff9)
		{
			video_addr_w(address & 1, data, 0);
			return;
		}

		if (address >= 0xfffc)
		{
			INT32 offset = address & 3;
			INT32 slot = BIT(offset, 1);
			INT32 shift = BIT(offset, 0) << 3;
			xrom_addr[slot] = (xrom_addr[slot] & ~(0xff << shift)) | (data << shift);
			return;
		}

		DrvTRAM[address & 0x7ff] = data;
	}
}

static UINT8 __fastcall ataxx_master_read(UINT16 address)
{
	if (address >= 0xa000 && address <= 0xdfff)
	{
		if (battery_enable == 2)
			return DrvQRAM[((master_bank & 0xc0) << 8) | (address - 0xa000)];
		return 0;
	}

	if (address >= 0xf800)
	{
		if (palette_enable)
			return DrvPalRAM[address & 0x7ff];

		if (address >= 0xfffc)
		{
			INT32 offset = address & 3;
			INT32 slot = BIT(offset, 1);
			UINT32 word = (slot << 16) | xrom_addr[slot];
			return DrvRegion[REG_XROM][((word * 2) + BIT(offset, 0)) & (region_size[REG_XROM] - 1)];
		}

		return DrvTRAM[address & 0x7ff];
	}

	return 0;
}

static UINT8 __fastcall ataxx_master_in(UINT16 port)
{
	port &= 0xff;

	INT32 handled;
	UINT8 ret = game_port_read(port, &handled);
	if (handled) return ret;

	if (port >= 0xd0 && port <= 0xef)
		return vram_port_r(ataxx_vram_offset(port - 0xd0), 0);

	switch (port)
	{
		case 0x04:
			return sound_response_r();

		case 0x20:
			return (DrvInputs[2] & ~0x01) | (EEPROMRead() ? 0x01 : 0);

		case 0xf6:
			return DrvInputs[0];

		case 0xf7:
		{
			UINT8 result = (DrvInputs[1] & ~0x02) | ((cpu_vpos() >= 240) ? 0 : 0x02);
			if (ZetIsHalted(1)) result ^= 0x01;
			return result;
		}
	}

	if (port >= 0xf0) return 0xff;

	return 0;
}

static void __fastcall ataxx_master_out(UINT16 port, UINT8 data)
{
	port &= 0xff;

	if (port >= 0xd0 && port <= 0xef)
	{
		master_defer_write(PENDING_VRAM, ataxx_vram_offset(port - 0xd0), data);
		return;
	}

	if (port >= 0x08 && port <= 0x0b && has_indyheat)
	{
		// indy heat analog select
		if ((port & 3) == 3)
			analog_result = read_analog(3 + data);
		return;
	}

	switch (port)
	{
		case 0x05:
			sound_command_w(1, data);
			return;

		case 0x06:
			sound_command_w(0, data);
			return;

		case 0x0c:
			ataxx_80186_control_w(data);
			return;

		case 0x20:
			eeprom_w(data);
			return;

		case 0xf0:
		case 0xf1:
		case 0xf2:
		case 0xf3:
			scroll_w(port & 3, data);
			return;

		case 0xf4:
			master_bank = data;
			update_master_bank();
			return;

		case 0xf5:
			master_defer_write(PENDING_SLAVE_ATAXX, 0, data);
			return;

		case 0xf8:
			irq_line = data + 1;
			return;
	}
}

static void slave_set_bank(UINT32 offset)
{
	switch (cfg->slave)
	{
		case SLAVE_SMALL:
			if (offset + 0xc000 > (UINT32)region_size[REG_SLAVE]) offset = (region_size[REG_SLAVE] > 0x10000) ? 0x10000 : 0x2000;
			ZetMapMemory(DrvRegion[REG_SLAVE] + offset, 0x2000, 0xdfff, MAP_ROM);
			break;

		case SLAVE_LARGE:
			if (offset + 0x8000 > (UINT32)region_size[REG_SLAVE]) offset = 0x10000;
			ZetMapMemory(DrvRegion[REG_SLAVE] + offset, 0x4000, 0xbfff, MAP_ROM);
			break;

		case SLAVE_ATAXX:
		case SLAVE_ASYLUM:
			if (offset + 0x8000 > (UINT32)region_size[REG_SLAVE]) offset = 0x2000;
			ZetMapMemory(DrvRegion[REG_SLAVE] + offset, 0x2000, 0x9fff, MAP_ROM);
			break;
	}

	slave_bank_offset = offset;
}

static void slave_bankswitch(UINT8 data)
{
	switch (cfg->slave)
	{
		case SLAVE_SMALL:
			slave_set_bank(0x10000 + 0xc000 * BIT(data, 0));
			break;

		case SLAVE_LARGE:
			slave_set_bank(0x10000 + 0x8000 * (data & 0x0f));
			break;

		default:
		{
			INT32 bank = data & 0x0f;
			UINT32 offset;

			if (bank == 0)
				offset = 0x2000;
			else
			{
				offset = 0x10000 * bank + 0x8000 * BIT(data, 4);
				if (region_size[REG_SLAVE] > 0x100000)
					offset += 0x100000 * BIT(data, 5);
			}

			slave_set_bank(offset);
		}
		break;
	}
}

static void __fastcall leland_slave_write(UINT16 address, UINT8 data)
{
	if (cfg->slave == SLAVE_SMALL)
	{
		if (address == 0xf800 || address == 0xf801) { video_addr_w(address & 1, data, 1); return; }
		if (address == 0xf803) { slave_bankswitch(data); return; }
		return;
	}

	if (cfg->slave == SLAVE_LARGE)
	{
		if (address == 0xc000) { slave_bankswitch(data); return; }
		if (address == 0xf800 || address == 0xf801) { video_addr_w(address & 1, data, 1); return; }
		return;
	}

	// ataxx
	if (address == 0xfffc || address == 0xfffd) { video_addr_w(address & 1, data, 1); return; }
	if (address == 0xffff) { slave_bankswitch(data); return; }
	if (cfg->slave == SLAVE_ASYLUM && address >= 0xf000 && address <= 0xfffb) DrvSlaveRAM[address - 0xe000] = data;
}

static UINT8 __fastcall leland_slave_read(UINT16 address)
{
	if (cfg->slave == SLAVE_SMALL || cfg->slave == SLAVE_LARGE)
	{
		if (address == 0xf802) return cpu_vpos();
		return 0;
	}

	if (address == 0xfffe) return cpu_vpos();
	if (cfg->slave == SLAVE_ASYLUM && address >= 0xf000 && address <= 0xfffb) return DrvSlaveRAM[address - 0xe000];
	return 0;
}

static UINT8 __fastcall leland_slave_in(UINT16 port)
{
	port &= 0xff;

	if (is_ataxx)
	{
		if (port >= 0x60 && port <= 0x7f)
			return vram_port_r(ataxx_vram_offset(port - 0x60), 1);
		return 0;
	}

	if ((port & 0xbf) <= 0x1f)
		return vram_port_r(port & 0x1f, 1);

	return 0;
}

static void __fastcall leland_slave_out(UINT16 port, UINT8 data)
{
	port &= 0xff;

	if (is_ataxx)
	{
		if (port >= 0x60 && port <= 0x7f)
			vram_port_w(ataxx_vram_offset(port - 0x60), data, 1);
		return;
	}

	if ((port & 0xbf) <= 0x1f)
		vram_port_w(port & 0x1f, data, 1);
}

static const eeprom_interface ataxx_eeprom_intf =
{
	8,				// address bits (93c56, top bit is a don't care)
	16,				// data bits
	"*110",			// read
	"*101",			// write
	"*111",			// erase
	"*10000xxxxxx",	// lock
	"*10011xxxxxx",	// unlock
	1,				// streaming
	0
};

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset(AllRam, 0, RamEnd - AllRam);
	}

	dacs_reset();

	// sound board first, the master z80 banking needs nothing from it
	if (cfg->sound != SND_NONE)
	{
		VezOpen(0);
		sound_frame_base = 0;
		sound_cycle_offset = 0;
		test_line = 1;
		periph_pacs = periph_mpcs = 0;
		periph_installed = 0;
		sound_map_memory();
		sound_board_reset();
		sound_command = sound_response = 0;
		VezClose();
	}

	sound_mute = 5; // mute sound for the first couple frames to avoid horrible popping noise

	ZetOpen(0);
	ZetReset();

	// the AY reset writes port A (gfx bank / master banking), master cpu must be open
	if (!is_ataxx) {
		AY8910Reset(0);
	}

	master_bank = 0;
	sound_port_bank = 0;
	top_board_bank = 0;
	alternate_bank = 0;
	palette_enable = 0;
	gfxbank = 0;
	dac_control = 0;
	update_master_bank();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	if (region_size[REG_SLAVE] > 0x10000)
		slave_set_bank(0x10000);
	else
		slave_set_bank(0x2000);
	ZetClose();

	EEPROMReset();

	dangerz_x = 512;
	dangerz_y = 512;
	analog_result = 0xff;
	memset(dial_last_input, 0, sizeof(dial_last_input));
	memset(dial_last_result, 0, sizeof(dial_last_result));

	keycard_shift = keycard_bit = keycard_state = keycard_clock = 0;
	memset(keycard_command, 0, sizeof(keycard_command));

	xrom_addr[0] = xrom_addr[1] = 0;
	memset(vram_state, 0, sizeof(vram_state));
	xscroll = yscroll = 0;
	irq_line = 8;
	pending_kind = PENDING_NONE;
	impulse_prev = 0;

	// start the dc filter at the dacs idle level (avoids a thump at reset)
	dc_last_in = mix_level_vram + mix_level_board;
	dc_last_out = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	for (INT32 i = 0; i < REG_MAX; i++) {
		DrvRegion[i] = Next; Next += (region_size[i] + 0xff) & ~0xff;
	}

	DrvTiles		= Next; Next += (is_ataxx ? (region_size[REG_BGGFX] / 6) : (region_size[REG_BGGFX] / 3)) * 8;

	DrvPalette		= (UINT32*)Next; Next += 0x400 * sizeof(UINT32);

	DrvBatteryRAM	= Next; Next += 0x004000;	// kept out of AllRam (nvram)

	mix_buffer		= (INT32*)Next; Next += 0x2000 * sizeof(INT32);

	AllRam			= Next;

	DrvMainRAM		= Next; Next += 0x001800;
	DrvSlaveRAM		= Next; Next += 0x002000;
	DrvPalRAM		= Next; Next += 0x000800;
	DrvVidRAM		= Next; Next += 0x010000;
	DrvQRAM			= Next; Next += 0x010000;
	DrvTRAM			= Next; Next += 0x000800;
	DrvSoundRAM		= Next; Next += 0x004000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvLoadRoms()
{
	for (INT32 i = 0; i < REG_MAX; i++) {
		if (region_size[i]) memset(DrvRegion[i], (rom_regions->erase_ff & (1 << i)) ? 0xff : 0x00, region_size[i]);
	}

	INT32 last_rom = -1;
	UINT8 *tmp = NULL;

	for (const LelandRomLoad *l = rom_load; l->rom >= 0; l++)
	{
		if (l->rom != last_rom)
		{
			struct BurnRomInfo ri;
			BurnDrvGetRomInfo(&ri, l->rom);

			BurnFree(tmp);
			tmp = (UINT8*)BurnMalloc(ri.nLen);
			if (BurnLoadRom(tmp, l->rom, 1)) {
				BurnFree(tmp);
				return 1;
			}
			last_rom = l->rom;
		}

		UINT8 *dst = DrvRegion[l->region];

		if (l->mode == 0) {
			memcpy(dst + l->dst, tmp + l->src, l->len);
		} else {
			for (INT32 j = 0; j < l->len; j++) {
				dst[l->dst + j * 2] = tmp[l->src + j];
			}
		}
	}

	BurnFree(tmp);

	return 0;
}

static void rotate_memory(INT32 region)
{
	INT32 startaddr = 0x10000;
	INT32 banks = (region_size[region] - startaddr) / 0x8000;
	UINT8 *ram = DrvRegion[region];
	UINT8 temp[0x2000];

	for (INT32 i = 0; i < banks; i++)
	{
		memmove(temp, &ram[startaddr + 0x0000], 0x2000);
		memmove(&ram[startaddr + 0x0000], &ram[startaddr + 0x2000], 0x6000);
		memmove(&ram[startaddr + 0x6000], temp, 0x2000);
		startaddr += 0x8000;
	}
}

static void DrvGfxDecode()
{
	UINT8 *src = DrvRegion[REG_BGGFX];
	INT32 planes = is_ataxx ? 6 : 3;
	INT32 plane_size = region_size[REG_BGGFX] / planes;
	INT32 tiles = plane_size / 8;

	for (INT32 t = 0; t < tiles; t++)
	{
		for (INT32 y = 0; y < 8; y++)
		{
			for (INT32 x = 0; x < 8; x++)
			{
				INT32 pix = 0;

				for (INT32 p = 0; p < planes; p++)
				{
					INT32 bit = (src[p * plane_size + t * 8 + y] >> (7 - x)) & 1;

					// leland: first plane is the msb, ataxx: last plane is the msb
					if (is_ataxx)
						pix |= bit << p;
					else
						pix |= bit << (planes - 1 - p);
				}

				DrvTiles[(t * 64) + (y * 8) + x] = pix;
			}
		}
	}

	tile_count = tiles;
	tile_mask = 1;
	while (tile_mask < tiles) tile_mask <<= 1;
	tile_mask = (tile_mask == tiles) ? (tiles - 1) : -1;
}

static void trackball_setup()
{
	trackball_count = 0;

	for (INT32 i = 0; i < 6; i++)
	{
		INT32 type = cfg->analog[i] >> 4;
		INT32 player = cfg->analog[i] & 0x0f;

		if (type == AN_DIAL || type == AN_TRACK_X || type == AN_TRACK_Y) {
			if (player + 1 > trackball_count) trackball_count = player + 1;
		}
	}

	if (trackball_count) {
		BurnTrackballInit(trackball_count);
	}
}

static INT32 DrvInit(const LelandConfig *config, const LelandRomLoad *load, const LelandRegions *regions)
{
	cfg = config;
	rom_load = load;
	rom_regions = regions;
	is_ataxx = (cfg->bank == BANK_ATAXX);

	for (INT32 i = 0; i < REG_MAX; i++) region_size[i] = regions->size[i];

	port_override_init();

	BurnSetRefreshRate(14318181.0 / 2 / 424 / 256);

	master_cycles = (INT32)(6000000.0 * 424 * 256 / (14318181.0 / 2));
	sound_cycles  = (INT32)(8000000.0 * 424 * 256 / (14318181.0 / 2));

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (DrvLoadRoms()) return 1;

	if (cfg->rotate_master) rotate_memory(REG_MASTER);
	for (INT32 i = 0; i < cfg->rotate_slave; i++) rotate_memory(REG_SLAVE);

	DrvGfxDecode();

	// master z80
	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvRegion[REG_MASTER], 0x0000, 0x1fff, MAP_ROM);
	if (is_ataxx)
	{
		ZetMapMemory(DrvMainRAM, 0xe000, 0xf7ff, MAP_RAM);
		ZetSetWriteHandler(ataxx_master_write);
		ZetSetReadHandler(ataxx_master_read);
		ZetSetOutHandler(ataxx_master_out);
		ZetSetInHandler(ataxx_master_in);
	}
	else
	{
		ZetMapMemory(DrvMainRAM, 0xe000, 0xefff, MAP_RAM);
		if (cfg->alleymas_kludge) ZetUnmapMemory(0xe000, 0xe0ff, MAP_WRITE);
		ZetSetWriteHandler(leland_master_write);
		ZetSetReadHandler(leland_master_read);
		ZetSetOutHandler(leland_master_out);
		ZetSetInHandler(leland_master_in);
	}
	ZetClose();

	// slave z80
	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvRegion[REG_SLAVE], 0x0000, 0x1fff, MAP_ROM);
	if (is_ataxx) ZetMapMemory(DrvRegion[REG_SLAVE] + 0xa000, 0xa000, 0xdfff, MAP_ROM);
	ZetMapMemory(DrvSlaveRAM, 0xe000, 0xefff, MAP_RAM);
	ZetSetWriteHandler(leland_slave_write);
	ZetSetReadHandler(leland_slave_read);
	ZetSetOutHandler(leland_slave_out);
	ZetSetInHandler(leland_slave_in);
	ZetClose();

	// 80186 sound board
	if (cfg->sound != SND_NONE)
	{
		VezInit(0, i80186_TYPE, 8000000);
		VezOpen(0);
		VezSetReadHandler(sound_read_byte);
		VezSetWriteHandler(sound_write_byte);
		VezSetReadWordHandler(sound_read_word);
		VezSetWriteWordHandler(sound_write_word);
		VezSetReadPort(sound_read_port);
		VezSetWritePort(sound_write_port);
		VezSetReadPortWord(sound_read_port_word);
		VezSetWritePortWord(sound_write_port_word);
		VezI186SetChipSelectCallback(i186_chip_select);
		VezI186SetTimerOutCallback(i186_timer_out);
		VezClose();

		switch (cfg->sound)
		{
			case SND_REDLINE: pit_num = 3; pit_clock = 7000000; break;
			case SND_LELAND:  pit_num = 2; pit_clock = 4000000; break;
			default:          pit_num = 1; pit_clock = 4000000; break;
		}

		if (cfg->sound == SND_WSF)
		{
			BurnYM2151InitBuffered(4000000, 0, ym2151_stream_pos, 0);
			BurnYM2151SetAllRoutes(0.40, BURN_SND_ROUTE_BOTH);
		}
	}
	else
	{
		pit_num = 0;
	}

	if (!is_ataxx)
	{
		AY8910Init(0, 10000000 / 6, 0);
		AY8910SetPorts(0, &sound_port_r, NULL, &sound_port_w, NULL);
		AY8910SetAllRoutes(0, 0.25, BURN_SND_ROUTE_BOTH);
		AY8910SetBuffered(ZetTotalCycles, 6000000);
	}

	if (is_ataxx)
	{
		EEPROMInit(&ataxx_eeprom_intf);
		if (!EEPROMAvailable()) {
			EEPROMFill(DrvRegion[REG_EEPROM], 0x000, 0x100);
			EEPROMFill(DrvRegion[REG_EEPROM], 0x100, 0x100);
		}
	}
	else
	{
		EEPROMInit(&eeprom_interface_93C46);
		if (!EEPROMAvailable()) {
			EEPROMFill(DrvRegion[REG_EEPROM], 0, 0x80);
		}
	}

	trackball_setup();

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	if (cfg->sound != SND_NONE) VezExit();
	if (cfg->sound == SND_WSF) BurnYM2151Exit();
	if (!is_ataxx) AY8910Exit(0);
	EEPROMExit();
	if (trackball_count) BurnTrackballExit();

	BurnFree(AllMem);

	cfg = NULL;

	return 0;
}

static void draw_line(INT32 y)
{
	if (y < 0 || y >= nScreenHeight) return;

	UINT16 *dst = pTransDraw + y * nScreenWidth;
	UINT8 *fg = DrvVidRAM + (y << 8);

	// the background is fetched one 8 pixel tile span at a time
	INT32 sy, rowbase, char_bank = 0, prom_bank = 0;

	if (is_ataxx)
	{
		sy = (y + yscroll) & 0x3ff;
		INT32 row = sy >> 3;
		rowbase = ((row & 0x3f) << 8) | ((row & 0x40) << 9);
	}
	else
	{
		char_bank = ((gfxbank >> 4) & 0x03) << 10;
		prom_bank = BIT(gfxbank, 3) << 13;
		sy = (y + yscroll) & 0x7ff;
		INT32 row = sy >> 3;
		rowbase = ((row & 0x1f) << 8) | ((row & 0xe0) << 9);
	}

	const UINT8 *tiles = DrvTiles + ((sy & 7) << 3);
	const INT32 prom_mask = region_size[REG_BGPROM] - 1;

	for (INT32 x = 0; x < nScreenWidth; )
	{
		INT32 sx = (x + xscroll) & 0x7ff;
		INT32 idx = rowbase | (sx >> 3);
		INT32 tile, color;

		if (is_ataxx)
		{
			tile = DrvQRAM[idx] | ((DrvQRAM[0x4000 | idx] & 0x7f) << 8);
			color = 0;
		}
		else
		{
			UINT8 code = DrvRegion[REG_BGPROM][(prom_bank | idx) & prom_mask];
			tile = code | ((idx >> 7) & 0x300) | char_bank;
			color = (code >> 5) << 3;
		}

		tile = (tile_mask >= 0) ? (tile & tile_mask) : (tile % tile_count);

		const UINT8 *src = tiles + (tile << 6);
		INT32 end = x + 8 - (sx & 7);
		if (end > nScreenWidth) end = nScreenWidth;

		for (INT32 px = sx & 7; x < end; x++, px++)
		{
			UINT8 fgpix = (x & 1) ? (fg[x >> 1] & 0x0f) : (fg[x >> 1] >> 4);
			dst[x] = color | src[px] | (fgpix << 6);
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x400; i++) palette_update(i);
		DrvRecalc = 0;
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void DrvMakeInputs()
{
	for (INT32 i = 0; i < 8; i++)
	{
		DrvInputs[i] = cfg->input_init[i];
		for (INT32 j = 0; j < 8; j++) {
			DrvInputs[i] ^= (DrvJoy[i][j] & 1) << j;
		}
	}

	// active high coin pulses (one frame long)
	if (cfg->impulse_mask)
	{
		UINT8 pressed = 0;
		for (INT32 j = 0; j < 8; j++) {
			if ((cfg->impulse_mask >> j) & 1) pressed |= (DrvJoy[0][j] & 1) << j;
		}

		UINT8 rising = pressed & ~impulse_prev;
		impulse_prev = pressed;

		DrvInputs[0] = (DrvInputs[0] & ~cfg->impulse_mask) | rising;
	}

	for (INT32 i = 0; i < trackball_count; i++)
	{
		BurnTrackballConfig(i, AXIS_NORMAL, AXIS_NORMAL);
		BurnTrackballFrame(i, DrvAnalogX[i], DrvAnalogY[i], 0x01, 0x08);
		BurnTrackballUDLR(i, DrvFake[i][0], DrvFake[i][1], DrvFake[i][2], DrvFake[i][3]);
		BurnTrackballUpdate(i);
	}

	for (INT32 i = 0; i < 6; i++)
	{
		INT32 type = cfg->analog[i] >> 4;
		INT32 p = cfg->analog[i] & 0x0f;
		UINT8 v = 0;

		switch (type)
		{
			case AN_STICK_X:
				v = ProcessAnalog(DrvAnalogX[p], 0, INPUT_DEADZONE, 0x00, 0xff);
				if (DrvFake[p][2]) v = 0x00;
				if (DrvFake[p][3]) v = 0xff;
				break;

			case AN_STICK_Y:
				v = ProcessAnalog(DrvAnalogY[p], 0, INPUT_DEADZONE, 0x00, 0xff);
				if (DrvFake[p][0]) v = 0x00;
				if (DrvFake[p][1]) v = 0xff;
				break;

			case AN_STICK_X_ALLEY:
				v = ProcessAnalog(DrvAnalogX[p], 0, INPUT_DEADZONE, 0x00, 0xe0, 0x80);
				if (DrvFake[p][2]) v = 0x00;
				if (DrvFake[p][3]) v = 0xe0;
				break;

			case AN_PEDAL:
				v = ProcessAnalog(DrvAnalogP[p], 0, INPUT_DEADZONE | INPUT_LINEAR | INPUT_MIGHTBEDIGITAL, 0x00, 0xff);
				if (DrvFake[p][0]) v = 0xff;
				break;

			case AN_DIAL:
			case AN_TRACK_X:
				v = BurnTrackballRead(p, 0);
				break;

			case AN_TRACK_Y:
				v = BurnTrackballRead(p, 1);
				break;
		}

		analog_value[i] = v;
	}

	// redline racer pedals live in the top bits of IN0 / IN2
	if (cfg->bank == BANK_REDLINE)
	{
		for (INT32 p = 0; p < 2; p++)
		{
			UINT8 v = ProcessAnalog(DrvAnalogP[p], 0, INPUT_DEADZONE | INPUT_LINEAR | INPUT_MIGHTBEDIGITAL, 0x00, 0xe0);
			if (DrvFake[p][0]) v = 0xe0;
			DrvInputs[p * 2] = (DrvInputs[p * 2] & 0x1f) | (v & 0xe0);
		}
	}
}

static INT32 DrvFrame()
{
	ZetNewFrame();
	if (cfg->sound != SND_NONE) VezNewFrame();
	sound_cycle_offset = 0;

	if (DrvReset) {
		DrvDoReset(1);
	}

	DrvMakeInputs();

	mixer_begin();

	INT32 has_sound = (cfg->sound != SND_NONE);
	if (has_sound) VezOpen(0);

	for (INT32 line = 0; line < 256; line++)
	{
		ZetOpen(0);

		if (!is_ataxx)
		{
			// dacs clocked by the video refresh (data at the end of the previous line)
			INT32 last = (line - 1) & 0xff;
			if (!BIT(dac_control, 0)) vdac_write(0, DrvVidRAM[(last << 8) | 0xa0], mixer_pos_line(line));
			if (!BIT(dac_control, 1)) vdac_write(1, DrvVidRAM[(last << 8) | 0xa1], mixer_pos_line(line));

			if ((line & 0x0f) == 8)
				ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);

			if (line == 240 && (DrvInputs[1] & 0x0e) != 0x0e)
				ZetSetIRQLine(0x20, CPU_IRQSTATUS_ACK);
		}
		else
		{
			if (line == irq_line)
				ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}

		ZetClose();

		INT32 target = (INT32)(((INT64)master_cycles * (line + 1)) / 256);

		while (1)
		{
			ZetOpen(0);
			if (ZetTotalCycles() < target) ZetRun(target - ZetTotalCycles());
			INT32 master_pos = ZetTotalCycles();
			ZetClose();

			ZetOpen(1);
			INT32 slave_target = (master_pos < target) ? master_pos : target;
			if (ZetTotalCycles() < slave_target) ZetRun(slave_target - ZetTotalCycles());
			ZetClose();

			if (pending_kind != PENDING_NONE)
				apply_pending_write();

			if (master_pos >= target) break;
		}

		if (has_sound)
			sound_run_to((INT32)(((INT64)sound_cycles * (line + 1)) / 256));

		if (line < 240 && pBurnDraw)
			draw_line(line);
	}

	if (has_sound)
	{
		sound_frame_base += sound_pos();
		VezClose();
	}

	if (pBurnSoundOut)
	{
		mixer_end();

		if (cfg->sound == SND_WSF)
			BurnYM2151Render(pBurnSoundOut, nBurnSoundLen);
		else if (!is_ataxx)
			AY8910Render(pBurnSoundOut, nBurnSoundLen);
		else
			memset(pBurnSoundOut, 0, nBurnSoundLen * 2 * sizeof(INT16));

		for (INT32 i = 0; i < nBurnSoundLen; i++)
		{
			INT32 l = pBurnSoundOut[i * 2 + 0] + mix_buffer[i];
			INT32 r = pBurnSoundOut[i * 2 + 1] + mix_buffer[i];
			pBurnSoundOut[i * 2 + 0] = BURN_SND_CLIP(l);
			pBurnSoundOut[i * 2 + 1] = BURN_SND_CLIP(r);
		}

		if (sound_mute) { // clicks and pops while booting up are no fun at all.
			BurnSoundClear();
			sound_mute--;
		}
	}

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	if (pnMin) {
		*pnMin = 0x029702;
	}

	if (nAction & ACB_VOLATILE)
	{
		ScanVar(AllRam, RamEnd - AllRam, "All Ram");

		ZetScan(nAction);

		if (cfg->sound != SND_NONE)
		{
			VezScan(nAction);
			if (cfg->sound == SND_WSF) BurnYM2151Scan(nAction, pnMin);

			SCAN_VAR(pit);
			SCAN_VAR(sound_frame_base);
			SCAN_VAR(sound_cycle_offset);
			SCAN_VAR(test_line);
			SCAN_VAR(last_control);
			SCAN_VAR(clock_active);
			SCAN_VAR(sound_command);
			SCAN_VAR(sound_response);
			SCAN_VAR(ext_start);
			SCAN_VAR(ext_stop);
			SCAN_VAR(ext_active);
			SCAN_VAR(periph_pacs);
			SCAN_VAR(periph_mpcs);
			SCAN_VAR(periph_installed);
			SCAN_VAR(dac_value);
			SCAN_VAR(dac_vol);
			SCAN_VAR(dac9_value);
		}

		SCAN_VAR(dc_last_in);
		SCAN_VAR(dc_last_out);

		if (!is_ataxx) AY8910Scan(nAction, pnMin);
		if (trackball_count) BurnTrackballScan();

		SCAN_VAR(sound_mute);
		SCAN_VAR(vdac_value);
		SCAN_VAR(master_bank);
		SCAN_VAR(sound_port_bank);
		SCAN_VAR(top_board_bank);
		SCAN_VAR(alternate_bank);
		SCAN_VAR(palette_enable);
		SCAN_VAR(slave_bank_offset);
		SCAN_VAR(dac_control);
		SCAN_VAR(gfxbank);
		SCAN_VAR(xscroll);
		SCAN_VAR(yscroll);
		SCAN_VAR(analog_result);
		SCAN_VAR(dial_last_input);
		SCAN_VAR(dial_last_result);
		SCAN_VAR(dangerz_x);
		SCAN_VAR(dangerz_y);
		SCAN_VAR(keycard_shift);
		SCAN_VAR(keycard_bit);
		SCAN_VAR(keycard_state);
		SCAN_VAR(keycard_clock);
		SCAN_VAR(keycard_command);
		SCAN_VAR(xrom_addr);
		SCAN_VAR(irq_line);
		SCAN_VAR(pending_kind);
		SCAN_VAR(pending_offset);
		SCAN_VAR(pending_data);
		SCAN_VAR(vram_state);
		SCAN_VAR(impulse_prev);
	}

	EEPROMScan(nAction, pnMin);

	if (nAction & ACB_NVRAM)
	{
		ScanVar(DrvBatteryRAM, 0x4000, "Battery RAM");
	}

	if (nAction & ACB_WRITE)
	{
		ZetOpen(0);
		update_master_bank();
		ZetClose();

		ZetOpen(1);
		slave_set_bank(slave_bank_offset);
		ZetClose();

		if (cfg->sound != SND_NONE)
		{
			VezOpen(0);
			sound_map_memory();
			VezClose();
		}

		dacs_recalc_all();

		DrvRecalc = 1; // DrvPalette is only rebuilt on palette writes
	}

	return 0;
}

#define A(a, b, c, d) { a, b, (UINT8*)(c), d }

#define LELAND_COMMON_P1 \
	{"P1 Coin",				BIT_DIGITAL,	DrvJoy[1] + 3,	"p1 coin"	}, \
	{"P1 Start",			BIT_DIGITAL,	DrvJoy[3] + 3,	"p1 start"	},

#define LELAND_COMMON_P2 \
	{"P2 Coin",				BIT_DIGITAL,	DrvJoy[1] + 2,	"p2 coin"	}, \
	{"P2 Start",			BIT_DIGITAL,	DrvJoy[3] + 2,	"p2 start"	},

#define LELAND_SYSTEM \
	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		}, \
	{"Service",				BIT_DIGITAL,	DrvJoy[1] + 1,	"service"	},

// analog x/y plus digital directions (fed through DrvFake)
#define ANALOG_XY(n, p, label) \
	A("P" #n " " label " X",	BIT_ANALOG_REL,	&DrvAnalogX[p],	"p" #n " x-axis"), \
	A("P" #n " " label " Y",	BIT_ANALOG_REL,	&DrvAnalogY[p],	"p" #n " y-axis"), \
	{"P" #n " Up",			BIT_DIGITAL,	&DrvFake[p][0],	"p" #n " up"	}, \
	{"P" #n " Down",		BIT_DIGITAL,	&DrvFake[p][1],	"p" #n " down"	}, \
	{"P" #n " Left",		BIT_DIGITAL,	&DrvFake[p][2],	"p" #n " left"	}, \
	{"P" #n " Right",		BIT_DIGITAL,	&DrvFake[p][3],	"p" #n " right"	},

#define STICK(n, p)		ANALOG_XY(n, p, "Stick")
#define TRACKBALL(n, p)	ANALOG_XY(n, p, "Trackball")

#define WHEEL(n, p) \
	A("P" #n " Wheel",		BIT_ANALOG_REL,	&DrvAnalogX[p],	"p" #n " x-axis"), \
	{"P" #n " Left",		BIT_DIGITAL,	&DrvFake[p][2],	"p" #n " left"	}, \
	{"P" #n " Right",		BIT_DIGITAL,	&DrvFake[p][3],	"p" #n " right"	},

#define PEDAL(n, p) \
	A("P" #n " Pedal",		BIT_ANALOG_REL,	&DrvAnalogP[p],	"p" #n " z-axis"), \
	{"P" #n " Accelerate",	BIT_DIGITAL,	&DrvFake[p][0],	"p" #n " up"	},

#define JOY(n, port, up, down, left, right) \
	{"P" #n " Up",			BIT_DIGITAL,	DrvJoy[port] + up,		"p" #n " up"	}, \
	{"P" #n " Down",		BIT_DIGITAL,	DrvJoy[port] + down,	"p" #n " down"	}, \
	{"P" #n " Left",		BIT_DIGITAL,	DrvJoy[port] + left,	"p" #n " left"	}, \
	{"P" #n " Right",		BIT_DIGITAL,	DrvJoy[port] + right,	"p" #n " right"	},

#define BUTTON(n, b, port, bit) \
	{"P" #n " Button " #b,	BIT_DIGITAL,	DrvJoy[port] + bit,	"p" #n " fire " #b	},

static struct BurnInputInfo CerberusInputList[] = {
	LELAND_COMMON_P1
	A("P1 Dial",			BIT_ANALOG_REL,	&DrvAnalogX[0],	"p1 x-axis"),
	{"P1 Left",				BIT_DIGITAL,	&DrvFake[0][2],	"p1 left"	},
	{"P1 Right",			BIT_DIGITAL,	&DrvFake[0][3],	"p1 right"	},
	BUTTON(1, 1, 0, 6)
	BUTTON(1, 2, 0, 7)

	LELAND_COMMON_P2
	A("P2 Dial",			BIT_ANALOG_REL,	&DrvAnalogX[1],	"p2 x-axis"),
	{"P2 Left",				BIT_DIGITAL,	&DrvFake[1][2],	"p2 left"	},
	{"P2 Right",			BIT_DIGITAL,	&DrvFake[1][3],	"p2 right"	},
	BUTTON(2, 1, 2, 6)
	BUTTON(2, 2, 2, 7)

	LELAND_SYSTEM
};

STDINPUTINFO(Cerberus)

static struct BurnInputInfo MayhemInputList[] = {
	LELAND_COMMON_P1
	JOY(1, 2, 7, 6, 5, 4)
	BUTTON(1, 1, 0, 5)
	BUTTON(1, 2, 0, 7)
	BUTTON(1, 3, 0, 6)

	LELAND_COMMON_P2
	JOY(2, 2, 3, 2, 1, 0)
	BUTTON(2, 1, 0, 1)
	BUTTON(2, 2, 0, 3)
	BUTTON(2, 3, 0, 2)

	LELAND_SYSTEM
};

STDINPUTINFO(Mayhem)

static struct BurnInputInfo WseriesInputList[] = {
	LELAND_COMMON_P1
	STICK(1, 0)
	BUTTON(1, 1, 0, 7)
	BUTTON(1, 2, 0, 6)
	BUTTON(1, 3, 2, 7)

	LELAND_COMMON_P2
	STICK(2, 1)

	LELAND_SYSTEM
};

STDINPUTINFO(Wseries)

static struct BurnInputInfo Basebal2InputList[] = {
	LELAND_COMMON_P1
	STICK(1, 0)
	BUTTON(1, 1, 0, 7)
	BUTTON(1, 2, 0, 6)
	BUTTON(1, 3, 2, 5)
	BUTTON(1, 4, 2, 4)
	BUTTON(1, 5, 2, 7)
	BUTTON(1, 6, 2, 6)

	LELAND_COMMON_P2
	STICK(2, 1)

	LELAND_SYSTEM
};

STDINPUTINFO(Basebal2)

static struct BurnInputInfo AlleymasInputList[] = {
	LELAND_COMMON_P1
	STICK(1, 0)
	BUTTON(1, 1, 0, 7)
	BUTTON(1, 2, 0, 6)

	LELAND_COMMON_P2

	LELAND_SYSTEM
};

STDINPUTINFO(Alleymas)

static struct BurnInputInfo UpyouralInputList[] = {
	LELAND_COMMON_P1
	STICK(1, 0)
	BUTTON(1, 1, 0, 7)
	BUTTON(1, 2, 2, 7)
	BUTTON(1, 3, 2, 6)
	BUTTON(1, 4, 0, 6)

	LELAND_COMMON_P2

	LELAND_SYSTEM
};

STDINPUTINFO(Upyoural)

static struct BurnInputInfo DangerzInputList[] = {
	LELAND_COMMON_P1
	TRACKBALL(1, 0)
	BUTTON(1, 1, 0, 5)
	BUTTON(1, 2, 0, 6)

	LELAND_COMMON_P2

	LELAND_SYSTEM
};

STDINPUTINFO(Dangerz)

static struct BurnInputInfo RedlineInputList[] = {
	LELAND_COMMON_P1
	WHEEL(1, 0)
	PEDAL(1, 0)

	LELAND_COMMON_P2
	WHEEL(2, 1)
	PEDAL(2, 1)

	LELAND_SYSTEM
};

STDINPUTINFO(Redline)

static struct BurnInputInfo QuarterbInputList[] = {
	LELAND_COMMON_P1
	JOY(1, 2, 7, 6, 5, 4)
	BUTTON(1, 1, 0, 4)
	A("P1 Stick X",			BIT_ANALOG_REL,	&DrvAnalogX[0],	"p1 x-axis"),
	A("P1 Stick Y",			BIT_ANALOG_REL,	&DrvAnalogY[0],	"p1 y-axis"),

	LELAND_COMMON_P2
	JOY(2, 2, 3, 2, 1, 0)
	BUTTON(2, 1, 0, 0)

	LELAND_SYSTEM
};

STDINPUTINFO(Quarterb)

static struct BurnInputInfo TeamqbInputList[] = {
	LELAND_COMMON_P1
	JOY(1, 2, 7, 6, 5, 4)
	BUTTON(1, 1, 0, 4)
	A("P1 Stick X",			BIT_ANALOG_REL,	&DrvAnalogX[0],	"p1 x-axis"),
	A("P1 Stick Y",			BIT_ANALOG_REL,	&DrvAnalogY[0],	"p1 y-axis"),

	LELAND_COMMON_P2
	JOY(2, 2, 3, 2, 1, 0)
	BUTTON(2, 1, 0, 0)

	{"P3 Start",			BIT_DIGITAL,	DrvJoy[4] + 6,	"p3 start"	},
	JOY(3, 5, 7, 6, 5, 4)
	BUTTON(3, 1, 4, 4)
	A("P3 Stick X",			BIT_ANALOG_REL,	&DrvAnalogX[2],	"p3 x-axis"),
	A("P3 Stick Y",			BIT_ANALOG_REL,	&DrvAnalogY[2],	"p3 y-axis"),

	{"P4 Start",			BIT_DIGITAL,	DrvJoy[4] + 7,	"p4 start"	},
	JOY(4, 5, 3, 2, 1, 0)
	BUTTON(4, 1, 4, 0)

	LELAND_SYSTEM
};

STDINPUTINFO(Teamqb)

static struct BurnInputInfo Aafb2pInputList[] = {
	LELAND_COMMON_P1
	JOY(1, 2, 7, 6, 5, 4)
	BUTTON(1, 1, 0, 4)
	A("P1 Stick X",			BIT_ANALOG_REL,	&DrvAnalogX[0],	"p1 x-axis"),
	A("P1 Stick Y",			BIT_ANALOG_REL,	&DrvAnalogY[0],	"p1 y-axis"),

	{"P2 Coin",				BIT_DIGITAL,	DrvJoy[1] + 2,	"p2 coin"	},
	{"P2 Start",			BIT_DIGITAL,	DrvJoy[4] + 6,	"p2 start"	},
	JOY(2, 5, 7, 6, 5, 4)
	BUTTON(2, 1, 4, 4)
	A("P2 Stick X",			BIT_ANALOG_REL,	&DrvAnalogX[1],	"p2 x-axis"),
	A("P2 Stick Y",			BIT_ANALOG_REL,	&DrvAnalogY[1],	"p2 y-axis"),

	LELAND_SYSTEM
};

STDINPUTINFO(Aafb2p)

static struct BurnInputInfo OffroadInputList[] = {
	{"P1 Coin",				BIT_DIGITAL,	DrvJoy[1] + 3,	"p1 coin"	},
	WHEEL(1, 0)
	PEDAL(1, 0)
	BUTTON(1, 1, 0, 4)

	{"P2 Coin",				BIT_DIGITAL,	DrvJoy[1] + 2,	"p2 coin"	},
	WHEEL(2, 1)
	PEDAL(2, 1)
	BUTTON(2, 1, 0, 5)

	{"P3 Coin",				BIT_DIGITAL,	DrvJoy[1] + 1,	"p3 coin"	},
	WHEEL(3, 2)
	PEDAL(3, 2)
	BUTTON(3, 1, 0, 6)

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",				BIT_DIGITAL,	DrvJoy[3] + 3,	"service"	},
};

STDINPUTINFO(Offroad)

static struct BurnInputInfo Offroadt2pInputList[] = {
	{"P1 Coin",				BIT_DIGITAL,	DrvJoy[1] + 3,	"p1 coin"	},
	{"P1 Start",			BIT_DIGITAL,	DrvJoy[0] + 0,	"p1 start"	},
	WHEEL(1, 0)
	PEDAL(1, 0)
	BUTTON(1, 1, 0, 4)

	{"P2 Coin",				BIT_DIGITAL,	DrvJoy[1] + 1,	"p2 coin"	},
	{"P2 Start",			BIT_DIGITAL,	DrvJoy[0] + 2,	"p2 start"	},
	WHEEL(2, 1)
	PEDAL(2, 1)
	BUTTON(2, 1, 0, 6)

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",				BIT_DIGITAL,	DrvJoy[3] + 3,	"service"	},
};

STDINPUTINFO(Offroadt2p)

static struct BurnInputInfo PigoutInputList[] = {
	{"P1 Coin",				BIT_DIGITAL,	DrvJoy[1] + 1,	"p1 coin"	},
	{"P1 Start",			BIT_DIGITAL,	DrvJoy[4] + 7,	"p1 start"	},
	JOY(1, 4, 1, 2, 3, 4)
	BUTTON(1, 1, 4, 5)
	BUTTON(1, 2, 4, 6)

	{"P2 Coin",				BIT_DIGITAL,	DrvJoy[1] + 3,	"p2 coin"	},
	{"P2 Start",			BIT_DIGITAL,	DrvJoy[2] + 4,	"p2 start"	},
	{"P2 Up",				BIT_DIGITAL,	DrvJoy[0] + 7,	"p2 up"		},
	{"P2 Down",				BIT_DIGITAL,	DrvJoy[2] + 7,	"p2 down"	},
	{"P2 Left",				BIT_DIGITAL,	DrvJoy[0] + 6,	"p2 left"	},
	{"P2 Right",			BIT_DIGITAL,	DrvJoy[2] + 6,	"p2 right"	},
	BUTTON(2, 1, 2, 5)
	BUTTON(2, 2, 0, 5)

	{"P3 Start",			BIT_DIGITAL,	DrvJoy[2] + 0,	"p3 start"	},
	{"P3 Up",				BIT_DIGITAL,	DrvJoy[2] + 3,	"p3 up"		},
	{"P3 Down",				BIT_DIGITAL,	DrvJoy[0] + 3,	"p3 down"	},
	{"P3 Left",				BIT_DIGITAL,	DrvJoy[2] + 2,	"p3 left"	},
	{"P3 Right",			BIT_DIGITAL,	DrvJoy[0] + 2,	"p3 right"	},
	BUTTON(3, 1, 2, 1)
	BUTTON(3, 2, 0, 1)

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",				BIT_DIGITAL,	DrvJoy[3] + 2,	"service"	},
};

STDINPUTINFO(Pigout)

static struct BurnInputInfo AtaxxInputList[] = {
	{"P1 Coin",				BIT_DIGITAL,	DrvJoy[0] + 0,	"p1 coin"	},
	{"P1 Start",			BIT_DIGITAL,	DrvJoy[0] + 4,	"p1 start"	},
	TRACKBALL(1, 0)
	BUTTON(1, 1, 0, 5)

	{"P2 Coin",				BIT_DIGITAL,	DrvJoy[0] + 1,	"p2 coin"	},
	{"P2 Start",			BIT_DIGITAL,	DrvJoy[0] + 6,	"p2 start"	},
	TRACKBALL(2, 1)
	BUTTON(2, 1, 0, 7)

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",				BIT_DIGITAL,	DrvJoy[0] + 3,	"service"	},
};

STDINPUTINFO(Ataxx)

static struct BurnInputInfo WsfInputList[] = {
	{"P1 Coin",				BIT_DIGITAL,	DrvJoy[0] + 0,	"p1 coin"	},
	{"P1 Start",			BIT_DIGITAL,	DrvJoy[6] + 3,	"p1 start"	},
	JOY(1, 4, 7, 6, 5, 4)
	BUTTON(1, 1, 6, 2)
	BUTTON(1, 2, 0, 4)

	{"P2 Coin",				BIT_DIGITAL,	DrvJoy[0] + 1,	"p2 coin"	},
	{"P2 Start",			BIT_DIGITAL,	DrvJoy[6] + 1,	"p2 start"	},
	JOY(2, 4, 3, 2, 1, 0)
	BUTTON(2, 1, 6, 0)
	BUTTON(2, 2, 0, 5)

	{"P3 Coin",				BIT_DIGITAL,	DrvJoy[0] + 2,	"p3 coin"	},
	{"P3 Start",			BIT_DIGITAL,	DrvJoy[6] + 5,	"p3 start"	},
	JOY(3, 5, 7, 6, 5, 4)
	BUTTON(3, 1, 6, 4)
	BUTTON(3, 2, 0, 6)

	{"P4 Coin",				BIT_DIGITAL,	DrvJoy[0] + 3,	"p4 coin"	},
	{"P4 Start",			BIT_DIGITAL,	DrvJoy[6] + 7,	"p4 start"	},
	JOY(4, 5, 3, 2, 1, 0)
	BUTTON(4, 1, 6, 6)
	BUTTON(4, 2, 0, 7)

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",				BIT_DIGITAL,	DrvJoy[2] + 2,	"service"	},
};

STDINPUTINFO(Wsf)

static struct BurnInputInfo IndyheatInputList[] = {
	{"P1 Coin",				BIT_DIGITAL,	DrvJoy[0] + 1,	"p1 coin"	},
	WHEEL(1, 0)
	PEDAL(1, 0)
	BUTTON(1, 1, 4, 0)
	BUTTON(1, 2, 0, 7)

	{"P2 Coin",				BIT_DIGITAL,	DrvJoy[0] + 2,	"p2 coin"	},
	WHEEL(2, 1)
	PEDAL(2, 1)
	BUTTON(2, 1, 5, 0)

	{"P3 Coin",				BIT_DIGITAL,	DrvJoy[0] + 3,	"p3 coin"	},
	WHEEL(3, 2)
	PEDAL(3, 2)
	BUTTON(3, 1, 6, 0)

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",				BIT_DIGITAL,	DrvJoy[6] + 7,	"service"	},
};

STDINPUTINFO(Indyheat)

static struct BurnInputInfo BrutforcInputList[] = {
	{"P1 Coin",				BIT_DIGITAL,	DrvJoy[0] + 2,	"p1 coin"	},
	{"P1 Start",			BIT_DIGITAL,	DrvJoy[5] + 6,	"p1 start"	},
	JOY(1, 5, 3, 2, 1, 0)
	BUTTON(1, 1, 5, 4)
	BUTTON(1, 2, 5, 5)

	{"P2 Coin",				BIT_DIGITAL,	DrvJoy[0] + 1,	"p2 coin"	},
	{"P2 Start",			BIT_DIGITAL,	DrvJoy[4] + 6,	"p2 start"	},
	JOY(2, 4, 3, 2, 1, 0)
	BUTTON(2, 1, 4, 4)
	BUTTON(2, 2, 4, 5)

	{"P3 Coin",				BIT_DIGITAL,	DrvJoy[0] + 3,	"p3 coin"	},
	{"P3 Start",			BIT_DIGITAL,	DrvJoy[6] + 6,	"p3 start"	},
	JOY(3, 6, 3, 2, 1, 0)
	BUTTON(3, 1, 6, 4)
	BUTTON(3, 2, 6, 5)

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",				BIT_DIGITAL,	DrvJoy[0] + 7,	"service"	},
};

STDINPUTINFO(Brutforc)

#undef A
#undef LELAND_COMMON_P1
#undef LELAND_COMMON_P2
#undef LELAND_SYSTEM
#undef ANALOG_XY
#undef STICK
#undef TRACKBALL
#undef WHEEL
#undef PEDAL
#undef JOY
#undef BUTTON

static const LelandPortOverride cerberus_ovr[] = {
	{ 0x80, OVR_CERBERUS_DIAL, 0, 0 }, { 0x90, OVR_CERBERUS_DIAL, 2, 1 }, { 0, OVR_END, 0, 0 }
};

static const LelandPortOverride dangerz_ovr[] = {
	{ 0xf4, OVR_DANGERZ_UPPER, 0, 0 }, { 0xf8, OVR_DANGERZ_Y, 0, 0 }, { 0xfc, OVR_DANGERZ_X, 0, 0 }, { 0, OVR_END, 0, 0 }
};

static const LelandPortOverride redline_ovr[] = {
	{ 0xc0, OVR_REDLINE_PEDAL, 0, 0 }, { 0xd0, OVR_REDLINE_PEDAL, 2, 0 },
	{ 0xf8, OVR_DIAL, 1, 1 }, { 0xfb, OVR_DIAL, 0, 0 }, { 0, OVR_END, 0, 0 }
};

static const LelandPortOverride viper_ovr[] = {
	{ 0xa4, OVR_DANGERZ_UPPER, 0, 0 }, { 0xb8, OVR_DANGERZ_Y, 0, 0 }, { 0xbc, OVR_DANGERZ_X, 0, 0 }, { 0, OVR_END, 0, 0 }
};

static const LelandPortOverride teamqb_ovr[] = {
	{ 0x7c, OVR_INPUT, 4, 0 }, { 0x7f, OVR_INPUT, 5, 0 }, { 0, OVR_END, 0, 0 }
};

static const LelandPortOverride offroad_ovr[] = {
	{ 0xf8, OVR_DIAL, 5, 2 }, { 0xf9, OVR_DIAL, 3, 0 }, { 0xfb, OVR_DIAL, 4, 1 }, { 0, OVR_END, 0, 0 }
};

static const LelandPortOverride pigout_ovr[] = {
	{ 0x7f, OVR_INPUT, 4, 0 }, { 0, OVR_END, 0, 0 }
};

static const LelandPortOverride ataxx_ovr[] = {
	{ 0x00, OVR_DIAL, 0, 0 }, { 0x01, OVR_DIAL, 1, 1 }, { 0x02, OVR_DIAL, 2, 2 }, { 0x03, OVR_DIAL, 3, 3 }, { 0, OVR_END, 0, 0 }
};

static const LelandPortOverride wsf_ovr[] = {
	{ 0x0d, OVR_INPUT, 4, 0 }, { 0x0e, OVR_INPUT, 5, 0 }, { 0x0f, OVR_INPUT, 6, 0 }, { 0, OVR_END, 0, 0 }
};

static const LelandPortOverride indyheat_ovr[] = {
	{ 0x00, OVR_DIAL, 0, 0 }, { 0x01, OVR_DIAL, 1, 1 }, { 0x02, OVR_DIAL, 2, 2 },
	{ 0x08, OVR_INDYHEAT, 0, 0 }, { 0x09, OVR_INDYHEAT, 0, 0 }, { 0x0a, OVR_INDYHEAT, 0, 0 }, { 0x0b, OVR_INDYHEAT, 0, 0 },
	{ 0x0d, OVR_INPUT, 4, 0 }, { 0x0e, OVR_INPUT, 5, 0 }, { 0x0f, OVR_INPUT, 6, 0 }, { 0, OVR_END, 0, 0 }
};

#define IN_LELAND	{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }
#define IN_ATAXX	{ 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }
#define IN_IMPULSE	{ 0xf1, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }

#define AN_NONE6	{ 0, 0, 0, 0, 0, 0 }
#define AN_WSERIES	{ AN(AN_STICK_Y, 0), AN(AN_STICK_X, 0), 0, 0, AN(AN_STICK_X, 1), AN(AN_STICK_Y, 1) }
#define AN_ALLEY	{ AN(AN_STICK_Y, 0), AN(AN_STICK_X_ALLEY, 0), 0, 0, 0, 0 }
#define AN_DANGERZ	{ AN(AN_TRACK_Y, 0), AN(AN_TRACK_X, 0), 0, 0, 0, 0 }
#define AN_2DIALS	{ AN(AN_DIAL, 0), AN(AN_DIAL, 1), 0, 0, 0, 0 }
#define AN_QB		{ AN(AN_STICK_X, 0), AN(AN_STICK_Y, 0), 0, 0, 0, 0 }
#define AN_TEAMQB	{ AN(AN_STICK_X, 0), AN(AN_STICK_Y, 0), 0, 0, AN(AN_STICK_Y, 2), AN(AN_STICK_X, 2) }
#define AN_AAFB2P	{ AN(AN_STICK_X, 0), AN(AN_STICK_Y, 0), 0, 0, AN(AN_STICK_Y, 1), AN(AN_STICK_X, 1) }
#define AN_OFFROAD	{ AN(AN_PEDAL, 0), AN(AN_PEDAL, 1), AN(AN_PEDAL, 2), AN(AN_DIAL, 0), AN(AN_DIAL, 1), AN(AN_DIAL, 2) }
#define AN_OFFRD2P	{ AN(AN_PEDAL, 0), 0, AN(AN_PEDAL, 1), AN(AN_DIAL, 0), 0, AN(AN_DIAL, 1) }
#define AN_ATAXX	{ AN(AN_TRACK_X, 0), AN(AN_TRACK_Y, 0), AN(AN_TRACK_X, 1), AN(AN_TRACK_Y, 1), 0, 0 }
#define AN_INDYHEAT	{ AN(AN_DIAL, 0), AN(AN_DIAL, 1), AN(AN_DIAL, 2), AN(AN_PEDAL, 0), AN(AN_PEDAL, 1), AN(AN_PEDAL, 2) }

//                                      sound        bank           slave         n  mvram          io          rm rs kl overrides
static const LelandConfig cerberus_cfg = { SND_NONE,    BANK_CERBERUS, SLAVE_SMALL,  1, { 0x40, 0 },    { 0x80, 0 },    0, 0, 0, cerberus_ovr, AN_2DIALS,   IN_LELAND, 0 };
static const LelandConfig mayhem_cfg   = { SND_NONE,    BANK_MAYHEM,   SLAVE_SMALL,  1, { 0x00, 0 },    { 0xc0, 0 },    0, 0, 0, NULL,         AN_NONE6,    IN_LELAND, 0 };
static const LelandConfig powrplay_cfg = { SND_NONE,    BANK_MAYHEM,   SLAVE_SMALL,  1, { 0x40, 0 },    { 0x80, 0 },    0, 0, 0, NULL,         AN_NONE6,    IN_LELAND, 0 };
static const LelandConfig wseries_cfg  = { SND_NONE,    BANK_MAYHEM,   SLAVE_SMALL,  1, { 0x40, 0 },    { 0x80, 0 },    0, 0, 0, NULL,         AN_WSERIES,  IN_LELAND, 0 };
static const LelandConfig alleymas_cfg = { SND_NONE,    BANK_MAYHEM,   SLAVE_SMALL,  1, { 0x00, 0 },    { 0xc0, 0 },    0, 0, 1, NULL,         AN_ALLEY,    IN_LELAND, 0 };
static const LelandConfig upyoural_cfg = { SND_NONE,    BANK_MAYHEM,   SLAVE_SMALL,  1, { 0x00, 0 },    { 0xc0, 0 },    0, 0, 0, NULL,         AN_ALLEY,    IN_LELAND, 0 };
static const LelandConfig dangerz_cfg  = { SND_NONE,    BANK_DANGERZ,  SLAVE_SMALL,  1, { 0x40, 0 },    { 0x80, 0 },    0, 0, 0, dangerz_ovr,  AN_DANGERZ,  IN_LELAND, 0 };
static const LelandConfig basebal2_cfg = { SND_NONE,    BANK_BASEBAL2, SLAVE_SMALL,  1, { 0x00, 0 },    { 0xc0, 0 },    0, 0, 0, NULL,         AN_WSERIES,  IN_LELAND, 0 };
static const LelandConfig dblplay_cfg  = { SND_NONE,    BANK_BASEBAL2, SLAVE_SMALL,  1, { 0x80, 0 },    { 0x40, 0 },    0, 0, 0, NULL,         AN_WSERIES,  IN_LELAND, 0 };
static const LelandConfig strkzone_cfg = { SND_NONE,    BANK_BASEBAL2, SLAVE_SMALL,  1, { 0x00, 0 },    { 0x40, 0 },    0, 0, 0, NULL,         AN_WSERIES,  IN_LELAND, 0 };
static const LelandConfig redlin2p_cfg = { SND_REDLINE, BANK_REDLINE,  SLAVE_SMALL,  1, { 0x00, 0 },    { 0xc0, 0 },    1, 0, 0, redline_ovr,  AN_2DIALS,   IN_LELAND, 0 };
static const LelandConfig quarterb_cfg = { SND_LELAND,  BANK_VIPER,    SLAVE_SMALL,  1, { 0x40, 0 },    { 0x80, 0 },    1, 0, 0, NULL,         AN_QB,       IN_LELAND, 0 };
static const LelandConfig viper_cfg    = { SND_LELAND,  BANK_VIPER,    SLAVE_LARGE,  1, { 0x00, 0 },    { 0xc0, 0 },    1, 2, 0, viper_ovr,    AN_DANGERZ,  IN_LELAND, 0 };
static const LelandConfig teamqb_cfg   = { SND_LELAND,  BANK_VIPER,    SLAVE_LARGE,  1, { 0x40, 0 },    { 0x80, 0 },    1, 2, 0, teamqb_ovr,   AN_TEAMQB,   IN_LELAND, 0 };
static const LelandConfig aafb_cfg     = { SND_LELAND,  BANK_VIPER,    SLAVE_LARGE,  1, { 0x00, 0 },    { 0xc0, 0 },    1, 2, 0, teamqb_ovr,   AN_TEAMQB,   IN_LELAND, 0 };
static const LelandConfig aafbb_cfg    = { SND_LELAND,  BANK_VIPER,    SLAVE_LARGE,  1, { 0x80, 0 },    { 0x40, 0 },    1, 2, 0, teamqb_ovr,   AN_TEAMQB,   IN_LELAND, 0 };
static const LelandConfig aafbd2p_cfg  = { SND_LELAND,  BANK_VIPER,    SLAVE_LARGE,  1, { 0x00, 0 },    { 0x40, 0 },    1, 2, 0, teamqb_ovr,   AN_AAFB2P,   IN_LELAND, 0 };
static const LelandConfig offroad_cfg  = { SND_LELAND,  BANK_OFFROAD,  SLAVE_LARGE,  2, { 0x00, 0x40 }, { 0xc0, 0x80 }, 1, 2, 0, offroad_ovr,  AN_OFFROAD,  IN_LELAND, 0 };
static const LelandConfig offroadt_cfg = { SND_LELAND,  BANK_OFFROAD,  SLAVE_LARGE,  1, { 0x80, 0 },    { 0x40, 0 },    1, 2, 0, offroad_ovr,  AN_OFFROAD,  IN_LELAND, 0 };
static const LelandConfig offrd2p_cfg  = { SND_LELAND,  BANK_OFFROAD,  SLAVE_LARGE,  1, { 0x80, 0 },    { 0x40, 0 },    1, 2, 0, offroad_ovr,  AN_OFFRD2P,  IN_LELAND, 0 };
static const LelandConfig pigout_cfg   = { SND_LELAND,  BANK_OFFROAD,  SLAVE_LARGE,  1, { 0x00, 0 },    { 0x40, 0 },    1, 2, 0, pigout_ovr,   AN_NONE6,    IN_LELAND, 0 };
static const LelandConfig ataxx_cfg    = { SND_ATAXX,   BANK_ATAXX,    SLAVE_ATAXX,  0, { 0, 0 },       { 0, 0 },       1, 1, 0, ataxx_ovr,    AN_ATAXX,    IN_ATAXX,  0 };
static const LelandConfig wsf_cfg      = { SND_WSF,     BANK_ATAXX,    SLAVE_ATAXX,  0, { 0, 0 },       { 0, 0 },       1, 1, 0, wsf_ovr,      AN_NONE6,    IN_ATAXX,  0 };
static const LelandConfig indyheat_cfg = { SND_WSF,     BANK_ATAXX,    SLAVE_ATAXX,  0, { 0, 0 },       { 0, 0 },       1, 1, 0, indyheat_ovr, AN_INDYHEAT, IN_IMPULSE, 0x0e };
static const LelandConfig brutforc_cfg = { SND_WSF,     BANK_ATAXX,    SLAVE_ATAXX,  0, { 0, 0 },       { 0, 0 },       1, 1, 0, wsf_ovr,      AN_NONE6,    IN_IMPULSE, 0x0e };
static const LelandConfig asylum_cfg   = { SND_WSF,     BANK_ATAXX,    SLAVE_ASYLUM, 0, { 0, 0 },       { 0, 0 },       1, 1, 0, wsf_ovr,      AN_NONE6,    IN_IMPULSE, 0x0e };


// Cerberus

static struct BurnRomInfo cerberusRomDesc[] = {
	{ "3-23u101",                  0x002000, 0xd78210df, 1 | BRF_PRG | BRF_ESS }, //  0 Master Z80 Code
	{ "3-23u102",                  0x002000, 0xeed121ef, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3-23u103",                  0x002000, 0x45b82bf7, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "3-23u104",                  0x002000, 0xe133d6bf, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "3-23u105",                  0x002000, 0xa12c2c79, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "3-23u106",                  0x002000, 0xd64110d2, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "3-23u107",                  0x002000, 0x24e41c34, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "3-23u3",                    0x002000, 0xb0579138, 2 | BRF_PRG | BRF_ESS }, //  7 Slave Z80 Code
	{ "3-23u4",                    0x002000, 0xba0dc990, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "3-23u5",                    0x002000, 0xf8d6cc5d, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "3-23u6",                    0x002000, 0x42cdd393, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "3-23u7",                    0x002000, 0xc020148a, 2 | BRF_PRG | BRF_ESS }, // 11
	{ "3-23u8",                    0x002000, 0xdbabdbde, 2 | BRF_PRG | BRF_ESS }, // 12
	{ "3-23u9",                    0x002000, 0xeb992385, 2 | BRF_PRG | BRF_ESS }, // 13
	{ "3-23u93",                   0x002000, 0x14a1a4b0, 5 | BRF_GRA },           // 14 Background Tiles
	{ "3-23u94",                   0x002000, 0x207a1709, 5 | BRF_GRA },           // 15
	{ "3-23u95",                   0x002000, 0xe9c86267, 5 | BRF_GRA },           // 16
	{ "3-23u70",                   0x002000, 0x96499983, 6 | BRF_GRA },           // 17 Background Tilemap
	{ "3-23_u92",                  0x002000, 0x497bb717, 6 | BRF_GRA },           // 18
	{ "3-23u69",                   0x002000, 0xebd14d9e, 6 | BRF_GRA },           // 19
	{ "3-23u91",                   0x002000, 0xb592d2e5, 6 | BRF_GRA },           // 20
	{ "3-23u68",                   0x002000, 0xcfa7b8bf, 6 | BRF_GRA },           // 21
	{ "3-23u90",                   0x002000, 0xb7566f8a, 6 | BRF_GRA },           // 22
	{ "3-23u67",                   0x002000, 0x02b079a8, 6 | BRF_GRA },           // 23
	{ "3-23u89",                   0x002000, 0x7e5e82bb, 6 | BRF_GRA },           // 24
	{ "eeprom-cerberus.bin",       0x000080, 0xa9594f08, 8 | BRF_PRG | BRF_ESS }, // 25 Default EEPROM
};

STD_ROM_PICK(cerberus)
STD_ROM_FN(cerberus)

static const LelandRomLoad cerberusRomLoad[] = {
	{  0, 0, 0, 0x000000, 0x000000, 0x002000 },
	{  1, 0, 0, 0x000000, 0x002000, 0x002000 },
	{  2, 0, 0, 0x000000, 0x004000, 0x002000 },
	{  3, 0, 0, 0x000000, 0x006000, 0x002000 },
	{  4, 0, 0, 0x000000, 0x008000, 0x002000 },
	{  5, 0, 0, 0x000000, 0x00a000, 0x002000 },
	{  6, 0, 0, 0x000000, 0x00c000, 0x002000 },
	{  7, 1, 0, 0x000000, 0x000000, 0x002000 },
	{  8, 1, 0, 0x000000, 0x002000, 0x002000 },
	{  9, 1, 0, 0x000000, 0x004000, 0x002000 },
	{ 10, 1, 0, 0x000000, 0x006000, 0x002000 },
	{ 11, 1, 0, 0x000000, 0x008000, 0x002000 },
	{ 12, 1, 0, 0x000000, 0x00a000, 0x002000 },
	{ 13, 1, 0, 0x000000, 0x00c000, 0x002000 },
	{ 14, 4, 0, 0x000000, 0x000000, 0x002000 },
	{ 15, 4, 0, 0x000000, 0x002000, 0x002000 },
	{ 16, 4, 0, 0x000000, 0x004000, 0x002000 },
	{ 17, 5, 0, 0x000000, 0x002000, 0x002000 },
	{ 18, 5, 0, 0x000000, 0x006000, 0x002000 },
	{ 19, 5, 0, 0x000000, 0x00a000, 0x002000 },
	{ 20, 5, 0, 0x000000, 0x00e000, 0x002000 },
	{ 21, 5, 0, 0x000000, 0x012000, 0x002000 },
	{ 22, 5, 0, 0x000000, 0x016000, 0x002000 },
	{ 23, 5, 0, 0x000000, 0x01a000, 0x002000 },
	{ 24, 5, 0, 0x000000, 0x01e000, 0x002000 },
	{ 25, 7, 0, 0x000000, 0x000000, 0x000080 },
	{ -1, 0, 0, 0, 0, 0 }
};

static const LelandRegions cerberusRegions = { { 0x010000, 0x010000, 0x000000, 0x000000, 0x006000, 0x020000, 0x000000, 0x000080 }, 0x00 };

static INT32 cerberusInit()
{
	return DrvInit(&cerberus_cfg, cerberusRomLoad, &cerberusRegions);
}

struct BurnDriver BurnDrvCerberus = {
	"cerberus", NULL, NULL, NULL, "1985",
	"Cerberus\0", NULL, "Cinematronics", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, cerberusRomInfo, cerberusRomName, NULL, NULL, NULL, NULL, CerberusInputInfo, NULL,
	cerberusInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};

// Mayhem 2002

static struct BurnRomInfo mayhemRomDesc[] = {
	{ "13208.101",                 0x004000, 0x04306973, 1 | BRF_PRG | BRF_ESS }, //  0 Master Z80 Code
	{ "13215.102",                 0x004000, 0x06e689ae, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "13216.103",                 0x004000, 0x6452a82c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "13217.104",                 0x004000, 0x62f6036e, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "13218.105",                 0x004000, 0x162f5eb1, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "13219.106",                 0x004000, 0xc0a74d6f, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "13207.3",                   0x004000, 0xbe1df6aa, 2 | BRF_PRG | BRF_ESS }, //  6 Slave Z80 Code
	{ "13209.4",                   0x004000, 0x39fcd7c6, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "13210.5",                   0x004000, 0x630ed136, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "13211.6",                   0x004000, 0x28b4aecd, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "13212.7",                   0x004000, 0x1d6b39ab, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "13213.8",                   0x004000, 0xf3b2ea05, 2 | BRF_PRG | BRF_ESS }, // 11
	{ "13214.9",                   0x004000, 0x96f3e8d9, 2 | BRF_PRG | BRF_ESS }, // 12
	{ "13204.93",                  0x004000, 0xde183518, 5 | BRF_GRA },           // 13 Background Tiles
	{ "13205.94",                  0x004000, 0xc61f63ac, 5 | BRF_GRA },           // 14
	{ "13206.95",                  0x004000, 0x8e7bd2fd, 5 | BRF_GRA },           // 15
	{ "13203.92",                  0x004000, 0x121ed5bf, 6 | BRF_GRA },           // 16 Background Tilemap
	{ "13201.69",                  0x004000, 0x90283e29, 6 | BRF_GRA },           // 17
	{ "13202.89",                  0x004000, 0xc5eaa4e3, 6 | BRF_GRA },           // 18
	{ "eeprom-mayhem.bin",         0x000080, 0x52bc0453, 8 | BRF_PRG | BRF_ESS }, // 19 Default EEPROM
};

STD_ROM_PICK(mayhem)
STD_ROM_FN(mayhem)

static const LelandRomLoad mayhemRomLoad[] = {
	{  0, 0, 0, 0x000000, 0x000000, 0x004000 },
	{  1, 0, 0, 0x000000, 0x010000, 0x002000 },
	{  1, 0, 0, 0x002000, 0x01c000, 0x002000 },
	{  2, 0, 0, 0x000000, 0x012000, 0x002000 },
	{  2, 0, 0, 0x002000, 0x01e000, 0x002000 },
	{  3, 0, 0, 0x000000, 0x014000, 0x002000 },
	{  3, 0, 0, 0x002000, 0x020000, 0x002000 },
	{  4, 0, 0, 0x000000, 0x016000, 0x002000 },
	{  4, 0, 0, 0x002000, 0x022000, 0x002000 },
	{  5, 0, 0, 0x000000, 0x018000, 0x002000 },
	{  5, 0, 0, 0x002000, 0x024000, 0x002000 },
	{  6, 1, 0, 0x000000, 0x000000, 0x004000 },
	{  7, 1, 0, 0x000000, 0x010000, 0x002000 },
	{  7, 1, 0, 0x002000, 0x01c000, 0x002000 },
	{  8, 1, 0, 0x000000, 0x012000, 0x002000 },
	{  8, 1, 0, 0x002000, 0x01e000, 0x002000 },
	{  9, 1, 0, 0x000000, 0x014000, 0x002000 },
	{  9, 1, 0, 0x002000, 0x020000, 0x002000 },
	{ 10, 1, 0, 0x000000, 0x016000, 0x002000 },
	{ 10, 1, 0, 0x002000, 0x022000, 0x002000 },
	{ 11, 1, 0, 0x000000, 0x018000, 0x002000 },
	{ 11, 1, 0, 0x002000, 0x024000, 0x002000 },
	{ 12, 1, 0, 0x000000, 0x01a000, 0x002000 },
	{ 12, 1, 0, 0x002000, 0x026000, 0x002000 },
	{ 13, 4, 0, 0x000000, 0x000000, 0x004000 },
	{ 14, 4, 0, 0x000000, 0x004000, 0x004000 },
	{ 15, 4, 0, 0x000000, 0x008000, 0x004000 },
	{ 16, 5, 0, 0x000000, 0x004000, 0x004000 },
	{ 17, 5, 0, 0x000000, 0x008000, 0x004000 },
	{ 18, 5, 0, 0x000000, 0x01c000, 0x004000 },
	{ 19, 7, 0, 0x000000, 0x000000, 0x000080 },
	{ -1, 0, 0, 0, 0, 0 }
};

static const LelandRegions mayhemRegions = { { 0x028000, 0x028000, 0x000000, 0x000000, 0x00c000, 0x020000, 0x000000, 0x000080 }, 0x00 };

static INT32 mayhemInit()
{
	return DrvInit(&mayhem_cfg, mayhemRomLoad, &mayhemRegions);
}

struct BurnDriver BurnDrvMayhem = {
	"mayhem", NULL, NULL, NULL, "1985",
	"Mayhem 2002\0", NULL, "Cinematronics", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, mayhemRomInfo, mayhemRomName, NULL, NULL, NULL, NULL, MayhemInputInfo, NULL,
	mayhemInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};

// Power Play

static struct BurnRomInfo powrplayRomDesc[] = {
	{ "13306.101",                 0x002000, 0x981fc215, 1 | BRF_PRG | BRF_ESS }, //  0 Master Z80 Code
	{ "13307.102",                 0x004000, 0x38a6ddfe, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "13308.103",                 0x004000, 0x7fa2ab9e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "13309.104",                 0x004000, 0xbd9e6fa8, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "13310.105",                 0x004000, 0xb6df3a5a, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "13311.106",                 0x004000, 0x5e17fe84, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "13305.003",                 0x002000, 0xdf8fbeed, 2 | BRF_PRG | BRF_ESS }, //  6 Slave Z80 Code
	{ "13313.004",                 0x004000, 0x081eb88f, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "13314.005",                 0x004000, 0xb8e61f8c, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "13315.006",                 0x004000, 0x776d3c40, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "13316.007",                 0x004000, 0x9b3ec2a1, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "13317.008",                 0x004000, 0xa081a031, 2 | BRF_PRG | BRF_ESS }, // 11
	{ "13302.093",                 0x002000, 0x9beaa403, 5 | BRF_GRA },           // 12 Background Tiles
	{ "13303.094",                 0x002000, 0x2bf711d0, 5 | BRF_GRA },           // 13
	{ "13304.095",                 0x002000, 0x06b8675b, 5 | BRF_GRA },           // 14
	{ "13301.070",                 0x002000, 0xaa6d3b9d, 6 | BRF_GRA },           // 15 Background Tilemap
	{ "eeprom-powrplay.bin",       0x000080, 0x0a999b1e, 8 | BRF_PRG | BRF_ESS }, // 16 Default EEPROM
};

STD_ROM_PICK(powrplay)
STD_ROM_FN(powrplay)

static const LelandRomLoad powrplayRomLoad[] = {
	{  0, 0, 0, 0x000000, 0x000000, 0x002000 },
	{  1, 0, 0, 0x000000, 0x010000, 0x002000 },
	{  1, 0, 0, 0x002000, 0x01c000, 0x002000 },
	{  2, 0, 0, 0x000000, 0x012000, 0x002000 },
	{  2, 0, 0, 0x002000, 0x01e000, 0x002000 },
	{  3, 0, 0, 0x000000, 0x014000, 0x002000 },
	{  3, 0, 0, 0x002000, 0x020000, 0x002000 },
	{  4, 0, 0, 0x000000, 0x016000, 0x002000 },
	{  4, 0, 0, 0x002000, 0x022000, 0x002000 },
	{  5, 0, 0, 0x000000, 0x018000, 0x002000 },
	{  5, 0, 0, 0x002000, 0x024000, 0x002000 },
	{  6, 1, 0, 0x000000, 0x000000, 0x002000 },
	{  7, 1, 0, 0x000000, 0x010000, 0x002000 },
	{  7, 1, 0, 0x002000, 0x01c000, 0x002000 },
	{  8, 1, 0, 0x000000, 0x012000, 0x002000 },
	{  8, 1, 0, 0x002000, 0x01e000, 0x002000 },
	{  9, 1, 0, 0x000000, 0x014000, 0x002000 },
	{  9, 1, 0, 0x002000, 0x020000, 0x002000 },
	{ 10, 1, 0, 0x000000, 0x016000, 0x002000 },
	{ 10, 1, 0, 0x002000, 0x022000, 0x002000 },
	{ 11, 1, 0, 0x000000, 0x018000, 0x002000 },
	{ 11, 1, 0, 0x002000, 0x024000, 0x002000 },
	{ 12, 4, 0, 0x000000, 0x000000, 0x002000 },
	{ 13, 4, 0, 0x000000, 0x002000, 0x002000 },
	{ 14, 4, 0, 0x000000, 0x004000, 0x002000 },
	{ 15, 5, 0, 0x000000, 0x000000, 0x002000 },
	{ 16, 7, 0, 0x000000, 0x000000, 0x000080 },
	{ -1, 0, 0, 0, 0, 0 }
};

static const LelandRegions powrplayRegions = { { 0x028000, 0x028000, 0x000000, 0x000000, 0x006000, 0x020000, 0x000000, 0x000080 }, 0x00 };

static INT32 powrplayInit()
{
	return DrvInit(&powrplay_cfg, powrplayRomLoad, &powrplayRegions);
}

struct BurnDriver BurnDrvPowrplay = {
	"powrplay", NULL, NULL, NULL, "1985",
	"Power Play\0", NULL, "Cinematronics", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, powrplayRomInfo, powrplayRomName, NULL, NULL, NULL, NULL, MayhemInputInfo, NULL,
	powrplayInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};

// World Series: The Season (rev 1)

static struct BurnRomInfo wseriesRomDesc[] = {
	{ "02-13409-01.101",           0x002000, 0xb5eccf5c, 1 | BRF_PRG | BRF_ESS }, //  0 Master Z80 Code
	{ "02-13410-01.102",           0x004000, 0xdd1ec091, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "02-13411-01.103",           0x004000, 0xec867a0e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "02-13412-01.104",           0x004000, 0x2977956d, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "02-13413-01.105",           0x004000, 0x569468a6, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "02-13414-01.106",           0x004000, 0xb178632d, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "02-13415-01.107",           0x004000, 0x20b92eff, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "02-13416-00.u3",            0x002000, 0x37c960cf, 2 | BRF_PRG | BRF_ESS }, //  7 Slave Z80 Code
	{ "02-13417-00.u4",            0x004000, 0x97f044b5, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "02-13418-00.u5",            0x004000, 0x0931cfc0, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "02-13419-00.u6",            0x004000, 0xa7962b5a, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "02-13420-00.u7",            0x004000, 0x3c275262, 2 | BRF_PRG | BRF_ESS }, // 11
	{ "02-13421-00.u8",            0x004000, 0x86f57c80, 2 | BRF_PRG | BRF_ESS }, // 12
	{ "02-13422-00.u9",            0x004000, 0x222e8405, 2 | BRF_PRG | BRF_ESS }, // 13
	{ "02-13401-00.u93",           0x004000, 0x4ea3e641, 5 | BRF_GRA },           // 14 Background Tiles
	{ "02-13402-00.u94",           0x004000, 0x71a8a56c, 5 | BRF_GRA },           // 15
	{ "02-13403-00.u95",           0x004000, 0x8077ae25, 5 | BRF_GRA },           // 16
	{ "02-13404-00.u92",           0x004000, 0x22da40aa, 6 | BRF_GRA },           // 17 Background Tilemap
	{ "02-13405-00.u69",           0x004000, 0x6f65b313, 6 | BRF_GRA },           // 18
	{ "02-13406-00.u68",           0x002000, 0xbb568693, 6 | BRF_GRA },           // 19
	{ "02-13407-00.u90",           0x004000, 0xe46ca57f, 6 | BRF_GRA },           // 20
	{ "02-13408-00.u67",           0x004000, 0xbe637305, 6 | BRF_GRA },           // 21
	{ "eeprom-wseries.bin",        0x000080, 0xc9c45105, 8 | BRF_PRG | BRF_ESS }, // 22 Default EEPROM
};

STD_ROM_PICK(wseries)
STD_ROM_FN(wseries)

static const LelandRomLoad wseriesRomLoad[] = {
	{  0, 0, 0, 0x000000, 0x000000, 0x002000 },
	{  1, 0, 0, 0x000000, 0x010000, 0x002000 },
	{  1, 0, 0, 0x002000, 0x01c000, 0x002000 },
	{  2, 0, 0, 0x000000, 0x012000, 0x002000 },
	{  2, 0, 0, 0x002000, 0x01e000, 0x002000 },
	{  3, 0, 0, 0x000000, 0x014000, 0x002000 },
	{  3, 0, 0, 0x002000, 0x020000, 0x002000 },
	{  4, 0, 0, 0x000000, 0x016000, 0x002000 },
	{  4, 0, 0, 0x002000, 0x022000, 0x002000 },
	{  5, 0, 0, 0x000000, 0x018000, 0x002000 },
	{  5, 0, 0, 0x002000, 0x024000, 0x002000 },
	{  6, 0, 0, 0x000000, 0x01a000, 0x002000 },
	{  6, 0, 0, 0x002000, 0x026000, 0x002000 },
	{  7, 1, 0, 0x000000, 0x000000, 0x002000 },
	{  8, 1, 0, 0x000000, 0x010000, 0x002000 },
	{  8, 1, 0, 0x002000, 0x01c000, 0x002000 },
	{  9, 1, 0, 0x000000, 0x012000, 0x002000 },
	{  9, 1, 0, 0x002000, 0x01e000, 0x002000 },
	{ 10, 1, 0, 0x000000, 0x014000, 0x002000 },
	{ 10, 1, 0, 0x002000, 0x020000, 0x002000 },
	{ 11, 1, 0, 0x000000, 0x016000, 0x002000 },
	{ 11, 1, 0, 0x002000, 0x022000, 0x002000 },
	{ 12, 1, 0, 0x000000, 0x018000, 0x002000 },
	{ 12, 1, 0, 0x002000, 0x024000, 0x002000 },
	{ 13, 1, 0, 0x000000, 0x01a000, 0x002000 },
	{ 13, 1, 0, 0x002000, 0x026000, 0x002000 },
	{ 14, 4, 0, 0x000000, 0x000000, 0x004000 },
	{ 15, 4, 0, 0x000000, 0x004000, 0x004000 },
	{ 16, 4, 0, 0x000000, 0x008000, 0x004000 },
	{ 17, 5, 0, 0x000000, 0x004000, 0x004000 },
	{ 18, 5, 0, 0x000000, 0x008000, 0x004000 },
	{ 19, 5, 0, 0x000000, 0x012000, 0x002000 },
	{ 20, 5, 0, 0x000000, 0x014000, 0x004000 },
	{ 21, 5, 0, 0x000000, 0x018000, 0x004000 },
	{ 22, 7, 0, 0x000000, 0x000000, 0x000080 },
	{ -1, 0, 0, 0, 0, 0 }
};

static const LelandRegions wseriesRegions = { { 0x028000, 0x028000, 0x000000, 0x000000, 0x00c000, 0x020000, 0x000000, 0x000080 }, 0x00 };

static INT32 wseriesInit()
{
	return DrvInit(&wseries_cfg, wseriesRomLoad, &wseriesRegions);
}

struct BurnDriver BurnDrvWseries = {
	"wseries", NULL, NULL, NULL, "1985",
	"World Series: The Season (rev 1)\0", NULL, "Cinematronics", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, wseriesRomInfo, wseriesRomName, NULL, NULL, NULL, NULL, WseriesInputInfo, NULL,
	wseriesInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};

// World Series: The Season (rev 0)

static struct BurnRomInfo wseries0RomDesc[] = {
	{ "02-13409-00.101",           0x002000, 0xa0820663, 1 | BRF_PRG | BRF_ESS }, //  0 Master Z80 Code
	{ "02-13410-00.102",           0x004000, 0xb2d71d2d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "02-13411-00.103",           0x004000, 0x50c29473, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "02-13412-00.104",           0x004000, 0x2e9294e9, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "02-13413-00.105",           0x004000, 0x2d785d3b, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "02-13414-00.106",           0x004000, 0xe6684f39, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "02-13415-00.107",           0x004000, 0x4c4db073, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "02-13416-00.u3",            0x002000, 0x37c960cf, 2 | BRF_PRG | BRF_ESS }, //  7 Slave Z80 Code
	{ "02-13417-00.u4",            0x004000, 0x97f044b5, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "02-13418-00.u5",            0x004000, 0x0931cfc0, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "02-13419-00.u6",            0x004000, 0xa7962b5a, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "02-13420-00.u7",            0x004000, 0x3c275262, 2 | BRF_PRG | BRF_ESS }, // 11
	{ "02-13421-00.u8",            0x004000, 0x86f57c80, 2 | BRF_PRG | BRF_ESS }, // 12
	{ "02-13422-00.u9",            0x004000, 0x222e8405, 2 | BRF_PRG | BRF_ESS }, // 13
	{ "02-13401-00.u93",           0x004000, 0x4ea3e641, 5 | BRF_GRA },           // 14 Background Tiles
	{ "02-13402-00.u94",           0x004000, 0x71a8a56c, 5 | BRF_GRA },           // 15
	{ "02-13403-00.u95",           0x004000, 0x8077ae25, 5 | BRF_GRA },           // 16
	{ "02-13404-00.u92",           0x004000, 0x22da40aa, 6 | BRF_GRA },           // 17 Background Tilemap
	{ "02-13405-00.u69",           0x004000, 0x6f65b313, 6 | BRF_GRA },           // 18
	{ "02-13406-00.u68",           0x002000, 0xbb568693, 6 | BRF_GRA },           // 19
	{ "02-13407-00.u90",           0x004000, 0xe46ca57f, 6 | BRF_GRA },           // 20
	{ "02-13408-00.u67",           0x004000, 0xbe637305, 6 | BRF_GRA },           // 21
	{ "eeprom-wseries.bin",        0x000080, 0xc9c45105, 8 | BRF_PRG | BRF_ESS }, // 22 Default EEPROM
};

STD_ROM_PICK(wseries0)
STD_ROM_FN(wseries0)

static INT32 wseries0Init()
{
	return DrvInit(&wseries_cfg, wseriesRomLoad, &wseriesRegions);
}

struct BurnDriver BurnDrvWseries0 = {
	"wseries0", "wseries", NULL, NULL, "1985",
	"World Series: The Season (rev 0)\0", NULL, "Cinematronics", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, wseries0RomInfo, wseries0RomName, NULL, NULL, NULL, NULL, WseriesInputInfo, NULL,
	wseries0Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};

// Alley Master

static struct BurnRomInfo alleymasRomDesc[] = {
	{ "02-13509-00.u101",          0x002000, 0x4273e260, 1 | BRF_PRG | BRF_ESS }, //  0 Master Z80 Code
	{ "02-13510-00.u102",          0x004000, 0xeb6575aa, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "02-13511-00.u103",          0x004000, 0xcc9d778c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "02-13512-00.u104",          0x004000, 0x8edb129b, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "02-13513-00.u105",          0x004000, 0xa342dc8e, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "02-13514-00.u106",          0x004000, 0xb396c254, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "02-13515-00.u107",          0x004000, 0x3ca13e8c, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "02-13516-00.u003",          0x002000, 0x3fee63ae, 2 | BRF_PRG | BRF_ESS }, //  7 Slave Z80 Code
	{ "02-13517-00.u004",          0x004000, 0xd302b5d1, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "02-13518-00.u005",          0x004000, 0x79bdb24d, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "02-13519-00.u006",          0x004000, 0xf0b15d68, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "02-13520-00.u007",          0x004000, 0x6974036c, 2 | BRF_PRG | BRF_ESS }, // 11
	{ "02-13521-00.u008",          0x004000, 0xa4357b5a, 2 | BRF_PRG | BRF_ESS }, // 12
	{ "02-13522-00.u009",          0x004000, 0x6d74274e, 2 | BRF_PRG | BRF_ESS }, // 13
	{ "02-13501-00.u093",          0x002000, 0x54456e6f, 5 | BRF_GRA },           // 14 Background Tiles
	{ "02-13502-00.u094",          0x002000, 0xedc240da, 5 | BRF_GRA },           // 15
	{ "02-13503-00.u095",          0x002000, 0x19793ed0, 5 | BRF_GRA },           // 16
	{ "02-13504-00.u092",          0x002000, 0xa020eab5, 6 | BRF_GRA },           // 17 Background Tilemap
	{ "02-13505-00.u069",          0x002000, 0x79abb979, 6 | BRF_GRA },           // 18
	{ "02-13506-00.u068",          0x002000, 0x0c583385, 6 | BRF_GRA },           // 19
	{ "02-13507-00.u090",          0x002000, 0x0e1769e3, 6 | BRF_GRA },           // 20
	{ "eeprom-alleymas.bin",       0x000080, 0x8c622ba1, 8 | BRF_PRG | BRF_ESS }, // 21 Default EEPROM
};

STD_ROM_PICK(alleymas)
STD_ROM_FN(alleymas)

static const LelandRomLoad alleymasRomLoad[] = {
	{  0, 0, 0, 0x000000, 0x000000, 0x002000 },
	{  1, 0, 0, 0x000000, 0x010000, 0x002000 },
	{  1, 0, 0, 0x002000, 0x01c000, 0x002000 },
	{  2, 0, 0, 0x000000, 0x012000, 0x002000 },
	{  2, 0, 0, 0x002000, 0x01e000, 0x002000 },
	{  3, 0, 0, 0x000000, 0x014000, 0x002000 },
	{  3, 0, 0, 0x002000, 0x020000, 0x002000 },
	{  4, 0, 0, 0x000000, 0x016000, 0x002000 },
	{  4, 0, 0, 0x002000, 0x022000, 0x002000 },
	{  5, 0, 0, 0x000000, 0x018000, 0x002000 },
	{  5, 0, 0, 0x002000, 0x024000, 0x002000 },
	{  6, 0, 0, 0x000000, 0x01a000, 0x002000 },
	{  6, 0, 0, 0x002000, 0x026000, 0x002000 },
	{  7, 1, 0, 0x000000, 0x000000, 0x002000 },
	{  8, 1, 0, 0x000000, 0x010000, 0x002000 },
	{  8, 1, 0, 0x002000, 0x01c000, 0x002000 },
	{  9, 1, 0, 0x000000, 0x012000, 0x002000 },
	{  9, 1, 0, 0x002000, 0x01e000, 0x002000 },
	{ 10, 1, 0, 0x000000, 0x014000, 0x002000 },
	{ 10, 1, 0, 0x002000, 0x020000, 0x002000 },
	{ 11, 1, 0, 0x000000, 0x016000, 0x002000 },
	{ 11, 1, 0, 0x002000, 0x022000, 0x002000 },
	{ 12, 1, 0, 0x000000, 0x018000, 0x002000 },
	{ 12, 1, 0, 0x002000, 0x024000, 0x002000 },
	{ 13, 1, 0, 0x000000, 0x01a000, 0x002000 },
	{ 13, 1, 0, 0x002000, 0x026000, 0x002000 },
	{ 14, 4, 0, 0x000000, 0x000000, 0x002000 },
	{ 15, 4, 0, 0x000000, 0x002000, 0x002000 },
	{ 16, 4, 0, 0x000000, 0x004000, 0x002000 },
	{ 17, 5, 0, 0x000000, 0x004000, 0x002000 },
	{ 18, 5, 0, 0x000000, 0x008000, 0x002000 },
	{ 19, 5, 0, 0x000000, 0x010000, 0x002000 },
	{ 20, 5, 0, 0x000000, 0x014000, 0x002000 },
	{ 21, 7, 0, 0x000000, 0x000000, 0x000080 },
	{ -1, 0, 0, 0, 0, 0 }
};

static const LelandRegions alleymasRegions = { { 0x028000, 0x028000, 0x000000, 0x000000, 0x006000, 0x020000, 0x000000, 0x000080 }, 0x00 };

static INT32 alleymasInit()
{
	return DrvInit(&alleymas_cfg, alleymasRomLoad, &alleymasRegions);
}

struct BurnDriver BurnDrvAlleymas = {
	"alleymas", "upyoural", NULL, NULL, "1985",
	"Alley Master\0", NULL, "Cinematronics", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, alleymasRomInfo, alleymasRomName, NULL, NULL, NULL, NULL, AlleymasInputInfo, NULL,
	alleymasInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	240, 320, 3, 4
};

// Up Your Alley

static struct BurnRomInfo upyouralRomDesc[] = {
	{ "03-14168-01_uya_chk-ec46.u101", 0x002000, 0x82bf3b7a, 1 | BRF_PRG | BRF_ESS }, //  0 Master Z80 Code
	{ "03-14169-01_uya_chk-06e9.u102", 0x004000, 0xe1681268, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "03-14170-01_uya_chk-abf4.u103", 0x004000, 0x0d36aa78, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "03-14171-01_uya_chk-9e1d.u104", 0x004000, 0xa4473886, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "03-14172-01_uya_chk-ec21.u105", 0x004000, 0x4cad86a4, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "03-14173-01_uya_chk-dc8a.u106", 0x004000, 0x26f4848e, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "03-14174-01_uya_chk-96f3.u107", 0x004000, 0xfd087cc7, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "03-14150-01_uya_chk-3c1d.u3", 0x002000, 0x3fee63ae, 2 | BRF_PRG | BRF_ESS }, //  7 Slave Z80 Code
	{ "03-14151-01_uya_chk-05a7.u4", 0x004000, 0xd302b5d1, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "03-14152-01_uya_chk-c686.u5", 0x004000, 0x79bdb24d, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "03-14153-01_uya_chk-9254.u6", 0x004000, 0xf0b15d68, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "03-14154-01_uya_chk-2d51.u7", 0x004000, 0x6974036c, 2 | BRF_PRG | BRF_ESS }, // 11
	{ "03-14155-01_uya_chk-42b6.u8", 0x004000, 0xa4357b5a, 2 | BRF_PRG | BRF_ESS }, // 12
	{ "03-14156-01_uya_chk-43e3.u9", 0x004000, 0x6d74274e, 2 | BRF_PRG | BRF_ESS }, // 13
	{ "03-14165-01_uya_chk-72ca.u93", 0x004000, 0xe8addd70, 5 | BRF_GRA },        // 14 Background Tiles
	{ "03-14166-01_uya_chk-9f27.u94", 0x004000, 0x3fd3be09, 5 | BRF_GRA },        // 15
	{ "03-14167-01_uya_chk-0151.u95", 0x004000, 0x37088dd1, 5 | BRF_GRA },        // 16
	{ "03-14164-01_uya_chk-e1d0.u92", 0x002000, 0xa020eab5, 6 | BRF_GRA },        // 17 Background Tilemap
	{ "03-14159-01_uya_chk-ca48.u69", 0x002000, 0x79abb979, 6 | BRF_GRA },        // 18
	{ "03-14158-01_uya_chk-18c8.u68", 0x002000, 0x0c583385, 6 | BRF_GRA },        // 19
	{ "03-14162-01_uya_chk-10e0.u90", 0x002000, 0x0e1769e3, 6 | BRF_GRA },        // 20
	{ "03-14157-01_uya_chk-1727.u67", 0x004000, 0xd30a385d, 6 | BRF_GRA },        // 21
	{ "03-14161-01_uya_chk-010f.u89", 0x004000, 0x5c17401e, 6 | BRF_GRA },        // 22
	{ "eeprom-upyoural.bin",       0x000080, 0x8c622ba1, 8 | BRF_PRG | BRF_ESS }, // 23 Default EEPROM
};

STD_ROM_PICK(upyoural)
STD_ROM_FN(upyoural)

static const LelandRomLoad upyouralRomLoad[] = {
	{  0, 0, 0, 0x000000, 0x000000, 0x002000 },
	{  1, 0, 0, 0x000000, 0x010000, 0x002000 },
	{  1, 0, 0, 0x002000, 0x01c000, 0x002000 },
	{  2, 0, 0, 0x000000, 0x012000, 0x002000 },
	{  2, 0, 0, 0x002000, 0x01e000, 0x002000 },
	{  3, 0, 0, 0x000000, 0x014000, 0x002000 },
	{  3, 0, 0, 0x002000, 0x020000, 0x002000 },
	{  4, 0, 0, 0x000000, 0x016000, 0x002000 },
	{  4, 0, 0, 0x002000, 0x022000, 0x002000 },
	{  5, 0, 0, 0x000000, 0x018000, 0x002000 },
	{  5, 0, 0, 0x002000, 0x024000, 0x002000 },
	{  6, 0, 0, 0x000000, 0x01a000, 0x002000 },
	{  6, 0, 0, 0x002000, 0x026000, 0x002000 },
	{  7, 1, 0, 0x000000, 0x000000, 0x002000 },
	{  8, 1, 0, 0x000000, 0x010000, 0x002000 },
	{  8, 1, 0, 0x002000, 0x01c000, 0x002000 },
	{  9, 1, 0, 0x000000, 0x012000, 0x002000 },
	{  9, 1, 0, 0x002000, 0x01e000, 0x002000 },
	{ 10, 1, 0, 0x000000, 0x014000, 0x002000 },
	{ 10, 1, 0, 0x002000, 0x020000, 0x002000 },
	{ 11, 1, 0, 0x000000, 0x016000, 0x002000 },
	{ 11, 1, 0, 0x002000, 0x022000, 0x002000 },
	{ 12, 1, 0, 0x000000, 0x018000, 0x002000 },
	{ 12, 1, 0, 0x002000, 0x024000, 0x002000 },
	{ 13, 1, 0, 0x000000, 0x01a000, 0x002000 },
	{ 13, 1, 0, 0x002000, 0x026000, 0x002000 },
	{ 14, 4, 0, 0x000000, 0x000000, 0x004000 },
	{ 15, 4, 0, 0x000000, 0x004000, 0x004000 },
	{ 16, 4, 0, 0x000000, 0x008000, 0x004000 },
	{ 17, 5, 0, 0x000000, 0x004000, 0x002000 },
	{ 18, 5, 0, 0x000000, 0x008000, 0x002000 },
	{ 19, 5, 0, 0x000000, 0x010000, 0x002000 },
	{ 20, 5, 0, 0x000000, 0x014000, 0x002000 },
	{ 21, 5, 0, 0x000000, 0x018000, 0x004000 },
	{ 22, 5, 0, 0x000000, 0x01c000, 0x004000 },
	{ 23, 7, 0, 0x000000, 0x000000, 0x000080 },
	{ -1, 0, 0, 0, 0, 0 }
};

static const LelandRegions upyouralRegions = { { 0x028000, 0x028000, 0x000000, 0x000000, 0x00c000, 0x020000, 0x000000, 0x000080 }, 0x00 };

static INT32 upyouralInit()
{
	return DrvInit(&upyoural_cfg, upyouralRomLoad, &upyouralRegions);
}

struct BurnDriver BurnDrvUpyoural = {
	"upyoural", NULL, NULL, NULL, "1987",
	"Up Your Alley\0", NULL, "Cinematronics", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, upyouralRomInfo, upyouralRomName, NULL, NULL, NULL, NULL, UpyouralInputInfo, NULL,
	upyouralInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	240, 320, 3, 4
};

// Danger Zone (rev 2)

static struct BurnRomInfo dangerzRomDesc[] = {
	{ "03-13823-02.u012t",         0x010000, 0x31604634, 1 | BRF_PRG | BRF_ESS }, //  0 Master Z80 Code
	{ "03-13824-00.u013",          0x010000, 0x381026c6, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "02-13816-00.u003",          0x004000, 0x71863c5b, 2 | BRF_PRG | BRF_ESS }, //  2 Slave Z80 Code
	{ "02-13817-00.u004",          0x004000, 0x924bead3, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "02-13818-00.u005",          0x004000, 0x403bdfea, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "02-13819-00.u006",          0x004000, 0x1fee5f10, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "02-13820-00.u007",          0x004000, 0x42657a1e, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "02-13821-00.u008",          0x004000, 0x92f3e006, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "02-13801-00.u093",          0x004000, 0xf9ff55ec, 5 | BRF_GRA },           //  8 Background Tiles
	{ "02-13802-00.u094",          0x004000, 0xd4adbcbb, 5 | BRF_GRA },           //  9
	{ "02-13803-00.u095",          0x004000, 0x9178ed76, 5 | BRF_GRA },           // 10
	{ "02-13809-00.u070",          0x004000, 0xe44eb9f5, 6 | BRF_GRA },           // 11 Background Tilemap
	{ "02-13804-00.u092",          0x004000, 0x6c23f1a5, 6 | BRF_GRA },           // 12
	{ "02-13805-00.u069",          0x004000, 0xe9c9f38b, 6 | BRF_GRA },           // 13
	{ "02-13808-00.u091",          0x004000, 0x035534ad, 6 | BRF_GRA },           // 14
	{ "02-13806-00.u068",          0x004000, 0x2dbd64d2, 6 | BRF_GRA },           // 15
	{ "02-13808-00.u090",          0x004000, 0xd5b4985d, 6 | BRF_GRA },           // 16
	{ "02-13822-00.u067",          0x004000, 0x00ff3033, 6 | BRF_GRA },           // 17
	{ "02-13810-00.u089",          0x004000, 0x4f645973, 6 | BRF_GRA },           // 18
	{ "eeprom-dangerz.bin",        0x000080, 0xdb2c632b, 8 | BRF_PRG | BRF_ESS }, // 19 Default EEPROM
};

STD_ROM_PICK(dangerz)
STD_ROM_FN(dangerz)

static const LelandRomLoad dangerzRomLoad[] = {
	{  0, 0, 0, 0x000000, 0x000000, 0x010000 },
	{  1, 0, 0, 0x000000, 0x010000, 0x010000 },
	{  2, 1, 0, 0x000000, 0x000000, 0x004000 },
	{  3, 1, 0, 0x000000, 0x010000, 0x002000 },
	{  3, 1, 0, 0x002000, 0x01c000, 0x002000 },
	{  4, 1, 0, 0x000000, 0x012000, 0x002000 },
	{  4, 1, 0, 0x002000, 0x01e000, 0x002000 },
	{  5, 1, 0, 0x000000, 0x014000, 0x002000 },
	{  5, 1, 0, 0x002000, 0x020000, 0x002000 },
	{  6, 1, 0, 0x000000, 0x016000, 0x002000 },
	{  6, 1, 0, 0x002000, 0x022000, 0x002000 },
	{  7, 1, 0, 0x000000, 0x018000, 0x002000 },
	{  7, 1, 0, 0x002000, 0x024000, 0x002000 },
	{  8, 4, 0, 0x000000, 0x000000, 0x004000 },
	{  9, 4, 0, 0x000000, 0x004000, 0x004000 },
	{ 10, 4, 0, 0x000000, 0x008000, 0x004000 },
	{ 11, 5, 0, 0x000000, 0x000000, 0x004000 },
	{ 12, 5, 0, 0x000000, 0x004000, 0x004000 },
	{ 13, 5, 0, 0x000000, 0x008000, 0x004000 },
	{ 14, 5, 0, 0x000000, 0x00c000, 0x004000 },
	{ 15, 5, 0, 0x000000, 0x010000, 0x004000 },
	{ 16, 5, 0, 0x000000, 0x014000, 0x004000 },
	{ 17, 5, 0, 0x000000, 0x018000, 0x004000 },
	{ 18, 5, 0, 0x000000, 0x01c000, 0x004000 },
	{ 19, 7, 0, 0x000000, 0x000000, 0x000080 },
	{ -1, 0, 0, 0, 0, 0 }
};

static const LelandRegions dangerzRegions = { { 0x020000, 0x028000, 0x000000, 0x000000, 0x00c000, 0x020000, 0x000000, 0x000080 }, 0x00 };

static INT32 dangerzInit()
{
	return DrvInit(&dangerz_cfg, dangerzRomLoad, &dangerzRegions);
}

struct BurnDriver BurnDrvDangerz = {
	"dangerz", NULL, NULL, NULL, "1986",
	"Danger Zone (rev 2)\0", NULL, "Cinematronics", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, dangerzRomInfo, dangerzRomName, NULL, NULL, NULL, NULL, DangerzInputInfo, NULL,
	dangerzInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};

// Danger Zone (rev 0)

static struct BurnRomInfo dangerz0RomDesc[] = {
	{ "03-13823-00.u012",          0x010000, 0xffc030b2, 1 | BRF_PRG | BRF_ESS }, //  0 Master Z80 Code
	{ "03-13824-00.u013",          0x010000, 0x381026c6, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "02-13816-00.u003",          0x004000, 0x71863c5b, 2 | BRF_PRG | BRF_ESS }, //  2 Slave Z80 Code
	{ "02-13817-00.u004",          0x004000, 0x924bead3, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "02-13818-00.u005",          0x004000, 0x403bdfea, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "02-13819-00.u006",          0x004000, 0x1fee5f10, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "02-13820-00.u007",          0x004000, 0x42657a1e, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "02-13821-00.u008",          0x004000, 0x92f3e006, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "02-13801-00.u093",          0x004000, 0xf9ff55ec, 5 | BRF_GRA },           //  8 Background Tiles
	{ "02-13802-00.u094",          0x004000, 0xd4adbcbb, 5 | BRF_GRA },           //  9
	{ "02-13803-00.u095",          0x004000, 0x9178ed76, 5 | BRF_GRA },           // 10
	{ "02-13809-00.u070",          0x004000, 0xe44eb9f5, 6 | BRF_GRA },           // 11 Background Tilemap
	{ "02-13804-00.u092",          0x004000, 0x6c23f1a5, 6 | BRF_GRA },           // 12
	{ "02-13805-00.u069",          0x004000, 0xe9c9f38b, 6 | BRF_GRA },           // 13
	{ "02-13808-00.u091",          0x004000, 0x035534ad, 6 | BRF_GRA },           // 14
	{ "02-13806-00.u068",          0x004000, 0x2dbd64d2, 6 | BRF_GRA },           // 15
	{ "02-13808-00.u090",          0x004000, 0xd5b4985d, 6 | BRF_GRA },           // 16
	{ "02-13822-00.u067",          0x004000, 0x00ff3033, 6 | BRF_GRA },           // 17
	{ "02-13810-00.u089",          0x004000, 0x4f645973, 6 | BRF_GRA },           // 18
	{ "eeprom-dangerz.bin",        0x000080, 0xdb2c632b, 8 | BRF_PRG | BRF_ESS }, // 19 Default EEPROM
};

STD_ROM_PICK(dangerz0)
STD_ROM_FN(dangerz0)

static INT32 dangerz0Init()
{
	return DrvInit(&dangerz_cfg, dangerzRomLoad, &dangerzRegions);
}

struct BurnDriver BurnDrvDangerz0 = {
	"dangerz0", "dangerz", NULL, NULL, "1986",
	"Danger Zone (rev 0)\0", NULL, "Cinematronics", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, dangerz0RomInfo, dangerz0RomName, NULL, NULL, NULL, NULL, DangerzInputInfo, NULL,
	dangerz0Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};

// Baseball: The Season II

static struct BurnRomInfo basebal2RomDesc[] = {
	{ "03-14115-00.u101",          0x002000, 0x05231fee, 1 | BRF_PRG | BRF_ESS }, //  0 Master Z80 Code
	{ "03-14116-00.u102",          0x004000, 0xe1482ea3, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "03-14117-01.u103",          0x004000, 0x677181dd, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "03-14118-01.u104",          0x004000, 0x5f570264, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "03-14119-01.u105",          0x004000, 0x90822145, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "03-14120-00.u106",          0x004000, 0x4d2b7217, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "03-14121-01.u107",          0x004000, 0xb987b97c, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "03-14122-01.u2t",           0x002000, 0xa89882d8, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "03-14123-01.u3t",           0x002000, 0xf9c51e5a, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "03-14100-01.u3",            0x002000, 0x1dffbdaf, 2 | BRF_PRG | BRF_ESS }, //  9 Slave Z80 Code
	{ "03-14101-01.u4",            0x004000, 0xc585529c, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "03-14102-01.u5",            0x004000, 0xace3f918, 2 | BRF_PRG | BRF_ESS }, // 11
	{ "03-14103-01.u6",            0x004000, 0xcd41cf7a, 2 | BRF_PRG | BRF_ESS }, // 12
	{ "03-14104-01.u7",            0x004000, 0x9b169e78, 2 | BRF_PRG | BRF_ESS }, // 13
	{ "03-14105-01.u8",            0x004000, 0xec596b43, 2 | BRF_PRG | BRF_ESS }, // 14
	{ "03-14106-01.u9",            0x004000, 0xb9656baa, 2 | BRF_PRG | BRF_ESS }, // 15
	{ "03-14112-00.u93",           0x004000, 0x8ccb1404, 5 | BRF_GRA },           // 16 Background Tiles
	{ "03-14113-00.u94",           0x004000, 0x9941a55b, 5 | BRF_GRA },           // 17
	{ "03-14114-00.u95",           0x004000, 0xb68baf47, 5 | BRF_GRA },           // 18
	{ "03-14111-01.u92",           0x004000, 0x2508a9ad, 6 | BRF_GRA },           // 19 Background Tilemap
	{ "03-14109-00.u69",           0x004000, 0xb123a28e, 6 | BRF_GRA },           // 20
	{ "03-14108-01.u68",           0x004000, 0xa1a51383, 6 | BRF_GRA },           // 21
	{ "03-14110-01.u90",           0x004000, 0xef01d997, 6 | BRF_GRA },           // 22
	{ "03-14107-00.u67",           0x004000, 0x976334e6, 6 | BRF_GRA },           // 23
	{ "eeprom-basebal2.bin",       0x000080, 0xc9c45105, 8 | BRF_PRG | BRF_ESS }, // 24 Default EEPROM
};

STD_ROM_PICK(basebal2)
STD_ROM_FN(basebal2)

static const LelandRomLoad basebal2RomLoad[] = {
	{  0, 0, 0, 0x000000, 0x000000, 0x002000 },
	{  1, 0, 0, 0x000000, 0x010000, 0x002000 },
	{  1, 0, 0, 0x002000, 0x01c000, 0x002000 },
	{  2, 0, 0, 0x000000, 0x012000, 0x002000 },
	{  2, 0, 0, 0x002000, 0x01e000, 0x002000 },
	{  3, 0, 0, 0x000000, 0x014000, 0x002000 },
	{  3, 0, 0, 0x002000, 0x020000, 0x002000 },
	{  4, 0, 0, 0x000000, 0x016000, 0x002000 },
	{  4, 0, 0, 0x002000, 0x022000, 0x002000 },
	{  5, 0, 0, 0x000000, 0x018000, 0x002000 },
	{  5, 0, 0, 0x002000, 0x024000, 0x002000 },
	{  6, 0, 0, 0x000000, 0x01a000, 0x002000 },
	{  6, 0, 0, 0x002000, 0x026000, 0x002000 },
	{  7, 0, 0, 0x000000, 0x028000, 0x002000 },
	{  7, 0, 0, 0x000000, 0x030000, 0x002000 },
	{  8, 0, 0, 0x000000, 0x02a000, 0x002000 },
	{  8, 0, 0, 0x000000, 0x032000, 0x002000 },
	{  9, 1, 0, 0x000000, 0x000000, 0x002000 },
	{ 10, 1, 0, 0x000000, 0x010000, 0x002000 },
	{ 10, 1, 0, 0x002000, 0x01c000, 0x002000 },
	{ 11, 1, 0, 0x000000, 0x012000, 0x002000 },
	{ 11, 1, 0, 0x002000, 0x01e000, 0x002000 },
	{ 12, 1, 0, 0x000000, 0x014000, 0x002000 },
	{ 12, 1, 0, 0x002000, 0x020000, 0x002000 },
	{ 13, 1, 0, 0x000000, 0x016000, 0x002000 },
	{ 13, 1, 0, 0x002000, 0x022000, 0x002000 },
	{ 14, 1, 0, 0x000000, 0x018000, 0x002000 },
	{ 14, 1, 0, 0x002000, 0x024000, 0x002000 },
	{ 15, 1, 0, 0x000000, 0x01a000, 0x002000 },
	{ 15, 1, 0, 0x002000, 0x026000, 0x002000 },
	{ 16, 4, 0, 0x000000, 0x000000, 0x004000 },
	{ 17, 4, 0, 0x000000, 0x004000, 0x004000 },
	{ 18, 4, 0, 0x000000, 0x008000, 0x004000 },
	{ 19, 5, 0, 0x000000, 0x004000, 0x004000 },
	{ 20, 5, 0, 0x000000, 0x008000, 0x004000 },
	{ 21, 5, 0, 0x000000, 0x010000, 0x004000 },
	{ 22, 5, 0, 0x000000, 0x014000, 0x004000 },
	{ 23, 5, 0, 0x000000, 0x018000, 0x004000 },
	{ 24, 7, 0, 0x000000, 0x000000, 0x000080 },
	{ -1, 0, 0, 0, 0, 0 }
};

static const LelandRegions basebal2Regions = { { 0x038000, 0x028000, 0x000000, 0x000000, 0x00c000, 0x020000, 0x000000, 0x000080 }, 0x00 };

static INT32 basebal2Init()
{
	return DrvInit(&basebal2_cfg, basebal2RomLoad, &basebal2Regions);
}

struct BurnDriver BurnDrvBasebal2 = {
	"basebal2", NULL, NULL, NULL, "1987",
	"Baseball: The Season II\0", NULL, "Cinematronics", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, basebal2RomInfo, basebal2RomName, NULL, NULL, NULL, NULL, Basebal2InputInfo, NULL,
	basebal2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};

// Super Baseball Double Play Home Run Derby

static struct BurnRomInfo dblplayRomDesc[] = {
	{ "15018-01.u101",             0x002000, 0x17b6af29, 1 | BRF_PRG | BRF_ESS }, //  0 Master Z80 Code
	{ "15019-01.u102",             0x004000, 0x9fc8205e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "15020-01.u103",             0x004000, 0x4edcc091, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "15021-01.u104",             0x004000, 0xa0eba1c7, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "15022-01.u105",             0x004000, 0x7bbfe0b7, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "15023-01.u106",             0x004000, 0xbbedae34, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "15024-01.u107",             0x004000, 0x02afcf52, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "15025-01.u2t",              0x002000, 0x1c959895, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "15026-01.u3t",              0x002000, 0xed5196d6, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "15027-01.u4t",              0x004000, 0x9b1e72e9, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "15000-01.u03",              0x002000, 0x208a920a, 2 | BRF_PRG | BRF_ESS }, // 10 Slave Z80 Code
	{ "15001-01.u04",              0x004000, 0x751c40d6, 2 | BRF_PRG | BRF_ESS }, // 11
	{ "14402-01.u05",              0x004000, 0x5ffaec36, 2 | BRF_PRG | BRF_ESS }, // 12
	{ "14403-01.u06",              0x004000, 0x48d6d9d3, 2 | BRF_PRG | BRF_ESS }, // 13
	{ "15004-01.u07",              0x004000, 0x6a7acebc, 2 | BRF_PRG | BRF_ESS }, // 14
	{ "15005-01.u08",              0x004000, 0x69d487c9, 2 | BRF_PRG | BRF_ESS }, // 15
	{ "15006-01.u09",              0x004000, 0xab3aac49, 2 | BRF_PRG | BRF_ESS }, // 16
	{ "15015-01.u93",              0x004000, 0x8ccb1404, 5 | BRF_GRA },           // 17 Background Tiles
	{ "15016-01.u94",              0x004000, 0x9941a55b, 5 | BRF_GRA },           // 18
	{ "15017-01.u95",              0x004000, 0xb68baf47, 5 | BRF_GRA },           // 19
	{ "15014-01.u92",              0x004000, 0x2508a9ad, 6 | BRF_GRA },           // 20 Background Tilemap
	{ "15009-01.u69",              0x004000, 0xb123a28e, 6 | BRF_GRA },           // 21
	{ "15008-01.u68",              0x004000, 0xa1a51383, 6 | BRF_GRA },           // 22
	{ "15012-01.u90",              0x004000, 0xef01d997, 6 | BRF_GRA },           // 23
	{ "15007-01.u67",              0x004000, 0x976334e6, 6 | BRF_GRA },           // 24
	{ "eeprom-dblplay.bin",        0x000080, 0x50fe6185, 8 | BRF_PRG | BRF_ESS }, // 25 Default EEPROM
};

STD_ROM_PICK(dblplay)
STD_ROM_FN(dblplay)

static const LelandRomLoad dblplayRomLoad[] = {
	{  0, 0, 0, 0x000000, 0x000000, 0x002000 },
	{  1, 0, 0, 0x000000, 0x010000, 0x002000 },
	{  1, 0, 0, 0x002000, 0x01c000, 0x002000 },
	{  2, 0, 0, 0x000000, 0x012000, 0x002000 },
	{  2, 0, 0, 0x002000, 0x01e000, 0x002000 },
	{  3, 0, 0, 0x000000, 0x014000, 0x002000 },
	{  3, 0, 0, 0x002000, 0x020000, 0x002000 },
	{  4, 0, 0, 0x000000, 0x016000, 0x002000 },
	{  4, 0, 0, 0x002000, 0x022000, 0x002000 },
	{  5, 0, 0, 0x000000, 0x018000, 0x002000 },
	{  5, 0, 0, 0x002000, 0x024000, 0x002000 },
	{  6, 0, 0, 0x000000, 0x01a000, 0x002000 },
	{  6, 0, 0, 0x002000, 0x026000, 0x002000 },
	{  7, 0, 0, 0x000000, 0x028000, 0x002000 },
	{  7, 0, 0, 0x000000, 0x030000, 0x002000 },
	{  8, 0, 0, 0x000000, 0x02a000, 0x002000 },
	{  8, 0, 0, 0x000000, 0x032000, 0x002000 },
	{  9, 0, 0, 0x000000, 0x02c000, 0x002000 },
	{  9, 0, 0, 0x002000, 0x034000, 0x002000 },
	{ 10, 1, 0, 0x000000, 0x000000, 0x002000 },
	{ 11, 1, 0, 0x000000, 0x010000, 0x002000 },
	{ 11, 1, 0, 0x002000, 0x01c000, 0x002000 },
	{ 12, 1, 0, 0x000000, 0x012000, 0x002000 },
	{ 12, 1, 0, 0x002000, 0x01e000, 0x002000 },
	{ 13, 1, 0, 0x000000, 0x014000, 0x002000 },
	{ 13, 1, 0, 0x002000, 0x020000, 0x002000 },
	{ 14, 1, 0, 0x000000, 0x016000, 0x002000 },
	{ 14, 1, 0, 0x002000, 0x022000, 0x002000 },
	{ 15, 1, 0, 0x000000, 0x018000, 0x002000 },
	{ 15, 1, 0, 0x002000, 0x024000, 0x002000 },
	{ 16, 1, 0, 0x000000, 0x01a000, 0x002000 },
	{ 16, 1, 0, 0x002000, 0x026000, 0x002000 },
	{ 17, 4, 0, 0x000000, 0x000000, 0x004000 },
	{ 18, 4, 0, 0x000000, 0x004000, 0x004000 },
	{ 19, 4, 0, 0x000000, 0x008000, 0x004000 },
	{ 20, 5, 0, 0x000000, 0x004000, 0x004000 },
	{ 21, 5, 0, 0x000000, 0x008000, 0x004000 },
	{ 22, 5, 0, 0x000000, 0x010000, 0x004000 },
	{ 23, 5, 0, 0x000000, 0x014000, 0x004000 },
	{ 24, 5, 0, 0x000000, 0x018000, 0x004000 },
	{ 25, 7, 0, 0x000000, 0x000000, 0x000080 },
	{ -1, 0, 0, 0, 0, 0 }
};

static const LelandRegions dblplayRegions = { { 0x038000, 0x028000, 0x000000, 0x000000, 0x00c000, 0x020000, 0x000000, 0x000080 }, 0x00 };

static INT32 dblplayInit()
{
	return DrvInit(&dblplay_cfg, dblplayRomLoad, &dblplayRegions);
}

struct BurnDriver BurnDrvDblplay = {
	"dblplay", NULL, NULL, NULL, "1987",
	"Super Baseball Double Play Home Run Derby\0", NULL, "Leland Corporation / Tradewest", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, dblplayRomInfo, dblplayRomName, NULL, NULL, NULL, NULL, Basebal2InputInfo, NULL,
	dblplayInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};

// Strike Zone Baseball

static struct BurnRomInfo strkzoneRomDesc[] = {
	{ "strkzone.u101",             0x004000, 0x8d83a611, 1 | BRF_PRG | BRF_ESS }, //  0 Master Z80 Code
	{ "strkzone.u102",             0x004000, 0x3859e67d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "strkzone.u103",             0x004000, 0xcdd83bfb, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "strkzone.u104",             0x004000, 0xbe280212, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "strkzone.u105",             0x004000, 0xafb63390, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "strkzone.u106",             0x004000, 0xe853b9f6, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "strkzone.u107",             0x004000, 0x1b4b6c2d, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "03-15313-01.u2t",           0x002000, 0x8e0af06f, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "03-15314-01.u3t",           0x002000, 0x909d35f3, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "03-15027-01.u4t",           0x004000, 0x9b1e72e9, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "strkzone.u3",               0x002000, 0x40258fbe, 2 | BRF_PRG | BRF_ESS }, // 10 Slave Z80 Code
	{ "strkzone.u4",               0x004000, 0xdf7f2604, 2 | BRF_PRG | BRF_ESS }, // 11
	{ "strkzone.u5",               0x004000, 0x37885206, 2 | BRF_PRG | BRF_ESS }, // 12
	{ "strkzone.u6",               0x004000, 0x6892dc4f, 2 | BRF_PRG | BRF_ESS }, // 13
	{ "strkzone.u7",               0x004000, 0x6ac8f87c, 2 | BRF_PRG | BRF_ESS }, // 14
	{ "strkzone.u8",               0x004000, 0x4b6d3725, 2 | BRF_PRG | BRF_ESS }, // 15
	{ "strkzone.u9",               0x004000, 0xab3aac49, 2 | BRF_PRG | BRF_ESS }, // 16
	{ "strkzone.u93",              0x004000, 0x8ccb1404, 5 | BRF_GRA },           // 17 Background Tiles
	{ "strkzone.u94",              0x004000, 0x9941a55b, 5 | BRF_GRA },           // 18
	{ "strkzone.u95",              0x004000, 0xb68baf47, 5 | BRF_GRA },           // 19
	{ "strkzone.u92",              0x004000, 0x2508a9ad, 6 | BRF_GRA },           // 20 Background Tilemap
	{ "03-15009-01.u69",           0x004000, 0xb123a28e, 6 | BRF_GRA },           // 21
	{ "03-15008-01.u68",           0x004000, 0xa1a51383, 6 | BRF_GRA },           // 22
	{ "strkzone.u90",              0x004000, 0xef01d997, 6 | BRF_GRA },           // 23
	{ "03-15007-01.u67",           0x004000, 0x976334e6, 6 | BRF_GRA },           // 24
	{ "eeprom-strkzone.bin",       0x000080, 0x0c2fe514, 8 | BRF_PRG | BRF_ESS }, // 25 Default EEPROM
};

STD_ROM_PICK(strkzone)
STD_ROM_FN(strkzone)

static const LelandRomLoad strkzoneRomLoad[] = {
	{  0, 0, 0, 0x000000, 0x000000, 0x004000 },
	{  1, 0, 0, 0x000000, 0x010000, 0x002000 },
	{  1, 0, 0, 0x002000, 0x01c000, 0x002000 },
	{  2, 0, 0, 0x000000, 0x012000, 0x002000 },
	{  2, 0, 0, 0x002000, 0x01e000, 0x002000 },
	{  3, 0, 0, 0x000000, 0x014000, 0x002000 },
	{  3, 0, 0, 0x002000, 0x020000, 0x002000 },
	{  4, 0, 0, 0x000000, 0x016000, 0x002000 },
	{  4, 0, 0, 0x002000, 0x022000, 0x002000 },
	{  5, 0, 0, 0x000000, 0x018000, 0x002000 },
	{  5, 0, 0, 0x002000, 0x024000, 0x002000 },
	{  6, 0, 0, 0x000000, 0x01a000, 0x002000 },
	{  6, 0, 0, 0x002000, 0x026000, 0x002000 },
	{  7, 0, 0, 0x000000, 0x028000, 0x002000 },
	{  7, 0, 0, 0x000000, 0x030000, 0x002000 },
	{  8, 0, 0, 0x000000, 0x02a000, 0x002000 },
	{  8, 0, 0, 0x000000, 0x032000, 0x002000 },
	{  9, 0, 0, 0x000000, 0x02c000, 0x002000 },
	{  9, 0, 0, 0x002000, 0x034000, 0x002000 },
	{ 10, 1, 0, 0x000000, 0x000000, 0x002000 },
	{ 11, 1, 0, 0x000000, 0x010000, 0x002000 },
	{ 11, 1, 0, 0x002000, 0x01c000, 0x002000 },
	{ 12, 1, 0, 0x000000, 0x012000, 0x002000 },
	{ 12, 1, 0, 0x002000, 0x01e000, 0x002000 },
	{ 13, 1, 0, 0x000000, 0x014000, 0x002000 },
	{ 13, 1, 0, 0x002000, 0x020000, 0x002000 },
	{ 14, 1, 0, 0x000000, 0x016000, 0x002000 },
	{ 14, 1, 0, 0x002000, 0x022000, 0x002000 },
	{ 15, 1, 0, 0x000000, 0x018000, 0x002000 },
	{ 15, 1, 0, 0x002000, 0x024000, 0x002000 },
	{ 16, 1, 0, 0x000000, 0x01a000, 0x002000 },
	{ 16, 1, 0, 0x002000, 0x026000, 0x002000 },
	{ 17, 4, 0, 0x000000, 0x000000, 0x004000 },
	{ 18, 4, 0, 0x000000, 0x004000, 0x004000 },
	{ 19, 4, 0, 0x000000, 0x008000, 0x004000 },
	{ 20, 5, 0, 0x000000, 0x004000, 0x004000 },
	{ 21, 5, 0, 0x000000, 0x008000, 0x004000 },
	{ 22, 5, 0, 0x000000, 0x010000, 0x004000 },
	{ 23, 5, 0, 0x000000, 0x014000, 0x004000 },
	{ 24, 5, 0, 0x000000, 0x018000, 0x004000 },
	{ 25, 7, 0, 0x000000, 0x000000, 0x000080 },
	{ -1, 0, 0, 0, 0, 0 }
};

static const LelandRegions strkzoneRegions = { { 0x038000, 0x028000, 0x000000, 0x000000, 0x00c000, 0x020000, 0x000000, 0x000080 }, 0x00 };

static INT32 strkzoneInit()
{
	return DrvInit(&strkzone_cfg, strkzoneRomLoad, &strkzoneRegions);
}

struct BurnDriver BurnDrvStrkzone = {
	"strkzone", NULL, NULL, NULL, "1988",
	"Strike Zone Baseball\0", NULL, "Leland Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, strkzoneRomInfo, strkzoneRomName, NULL, NULL, NULL, NULL, Basebal2InputInfo, NULL,
	strkzoneInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};

// Redline Racer (2 players)

static struct BurnRomInfo redlin2pRomDesc[] = {
	{ "02-13932-01.u23t",          0x010000, 0xecdf0fbe, 1 | BRF_PRG | BRF_ESS }, //  0 Master Z80 Code
	{ "02-13931-01.u22t",          0x010000, 0x16d01978, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "02-13907-01.u3",            0x004000, 0xb760d63e, 2 | BRF_PRG | BRF_ESS }, //  2 Slave Z80 Code
	{ "02-13908-01.u4",            0x004000, 0xa30739d3, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "02-13909-01.u5",            0x004000, 0xaaf16ad7, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "02-13910-01.u6",            0x004000, 0xd03469eb, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "02-13911-01.u7",            0x004000, 0x8ee1f547, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "02-13912-01.u8",            0x004000, 0xe5b57eac, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "02-13913-01.u9",            0x004000, 0x02886071, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "u17t",                      0x010000, 0x8d26f221, 3 | BRF_PRG | BRF_ESS }, //  9 80186 Code
	{ "u28t",                      0x010000, 0x7aa21b2c, 3 | BRF_PRG | BRF_ESS }, // 10
	{ "02-13930-01.u93",           0x004000, 0x0721f42e, 5 | BRF_GRA },           // 11 Background Tiles
	{ "02-13929-01.u94",           0x004000, 0x1522e7b2, 5 | BRF_GRA },           // 12
	{ "02-13928-01.u95",           0x004000, 0xc321b5d1, 5 | BRF_GRA },           // 13
	{ "02-13920-01.u70",           0x004000, 0xf343d34a, 6 | BRF_GRA },           // 14 Background Tilemap
	{ "02-13921-01.u92",           0x004000, 0xc9ba8d41, 6 | BRF_GRA },           // 15
	{ "02-13922-01.u69",           0x004000, 0x276cfba0, 6 | BRF_GRA },           // 16
	{ "02-13923-01.u91",           0x004000, 0x4a88ea34, 6 | BRF_GRA },           // 17
	{ "02-13924-01.u68",           0x004000, 0x3995cb7e, 6 | BRF_GRA },           // 18
	{ "02-13926-01.u67",           0x004000, 0xdaa30add, 6 | BRF_GRA },           // 19
	{ "02-13927-01.u89",           0x004000, 0x30e60fb5, 6 | BRF_GRA },           // 20
	{ "pal16l8-2601.25t",          0x000104, 0x00000000, 9 | BRF_NODUMP },        // 21 PLDs
	{ "pl20x10-0501.u21",          0x0000cc, 0x09ef7a46, 9 | BRF_OPT },           // 22
	{ "eeprom-redlin2p.bin",       0x000080, 0xe632970c, 8 | BRF_PRG | BRF_ESS }, // 23 Default EEPROM
};

STD_ROM_PICK(redlin2p)
STD_ROM_FN(redlin2p)

static const LelandRomLoad redlin2pRomLoad[] = {
	{  0, 0, 0, 0x000000, 0x000000, 0x010000 },
	{  1, 0, 0, 0x000000, 0x010000, 0x010000 },
	{  2, 1, 0, 0x000000, 0x000000, 0x004000 },
	{  3, 1, 0, 0x000000, 0x010000, 0x002000 },
	{  3, 1, 0, 0x002000, 0x01c000, 0x002000 },
	{  4, 1, 0, 0x000000, 0x012000, 0x002000 },
	{  4, 1, 0, 0x002000, 0x01e000, 0x002000 },
	{  5, 1, 0, 0x000000, 0x014000, 0x002000 },
	{  5, 1, 0, 0x002000, 0x020000, 0x002000 },
	{  6, 1, 0, 0x000000, 0x016000, 0x002000 },
	{  6, 1, 0, 0x002000, 0x022000, 0x002000 },
	{  7, 1, 0, 0x000000, 0x018000, 0x002000 },
	{  7, 1, 0, 0x002000, 0x024000, 0x002000 },
	{  8, 1, 0, 0x000000, 0x01a000, 0x002000 },
	{  8, 1, 0, 0x002000, 0x026000, 0x002000 },
	{  9, 2, 1, 0x000000, 0x0e0001, 0x010000 },
	{ 10, 2, 1, 0x000000, 0x0e0000, 0x010000 },
	{ 11, 4, 0, 0x000000, 0x000000, 0x004000 },
	{ 12, 4, 0, 0x000000, 0x004000, 0x004000 },
	{ 13, 4, 0, 0x000000, 0x008000, 0x004000 },
	{ 14, 5, 0, 0x000000, 0x000000, 0x004000 },
	{ 15, 5, 0, 0x000000, 0x004000, 0x004000 },
	{ 16, 5, 0, 0x000000, 0x008000, 0x004000 },
	{ 17, 5, 0, 0x000000, 0x00c000, 0x004000 },
	{ 18, 5, 0, 0x000000, 0x010000, 0x004000 },
	{ 19, 5, 0, 0x000000, 0x018000, 0x004000 },
	{ 20, 5, 0, 0x000000, 0x01c000, 0x004000 },
	{ 23, 7, 0, 0x000000, 0x000000, 0x000080 },
	{ -1, 0, 0, 0, 0, 0 }
};

static const LelandRegions redlin2pRegions = { { 0x020000, 0x080000, 0x100000, 0x000000, 0x00c000, 0x020000, 0x000000, 0x000080 }, 0x00 };

static INT32 redlin2pInit()
{
	return DrvInit(&redlin2p_cfg, redlin2pRomLoad, &redlin2pRegions);
}

struct BurnDriver BurnDrvRedlin2p = {
	"redlin2p", NULL, NULL, NULL, "1987",
	"Redline Racer (2 players)\0", NULL, "Cinematronics (Tradewest license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, redlin2pRomInfo, redlin2pRomName, NULL, NULL, NULL, NULL, RedlineInputInfo, NULL,
	redlin2pInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	240, 320, 3, 4
};

// Quarterback (rev 5)

static struct BurnRomInfo quarterbRomDesc[] = {
	{ "03-15219-05.u49t",          0x010000, 0xff653e4f, 1 | BRF_PRG | BRF_ESS }, //  0 Master Z80 Code
	{ "03-15218-05.u48t",          0x010000, 0x34b83d81, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "03-15200-01.u3",            0x004000, 0x83297861, 2 | BRF_PRG | BRF_ESS }, //  2 Slave Z80 Code
	{ "03-15201-01.u4",            0x004000, 0xaf8dbdab, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "03-15202-01.u5",            0x004000, 0x3eeecb3d, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "03-15203-01.u6",            0x004000, 0xb9c5b663, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "03-15204-01.u7",            0x004000, 0xc68821b7, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "03-15205-01.u8",            0x004000, 0x2be843a9, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "03-15206-01.u9",            0x004000, 0x6bf8d4ab, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "03-15222-02.u45t",          0x010000, 0x710bdc76, 3 | BRF_PRG | BRF_ESS }, //  9 80186 Code
	{ "03-15225-02.u62t",          0x010000, 0x041cecde, 3 | BRF_PRG | BRF_ESS }, // 10
	{ "03-15221-02.u44t",          0x010000, 0xe0459ddb, 3 | BRF_PRG | BRF_ESS }, // 11
	{ "03-15224-02.u61t",          0x010000, 0x9027c579, 3 | BRF_PRG | BRF_ESS }, // 12
	{ "03-15220-02.u43t",          0x010000, 0x48a8a018, 3 | BRF_PRG | BRF_ESS }, // 13
	{ "03-15223-02.u60t",          0x010000, 0x6a299766, 3 | BRF_PRG | BRF_ESS }, // 14
	{ "03-15215-01.u93",           0x004000, 0x4fb678d7, 5 | BRF_GRA },           // 15 Background Tiles
	{ "03-15216-01.u94",           0x004000, 0x7b57a44c, 5 | BRF_GRA },           // 16
	{ "03-15217-01.u95",           0x004000, 0x29bc33fd, 5 | BRF_GRA },           // 17
	{ "03-15210-01.u70",           0x004000, 0xa5aea20e, 6 | BRF_GRA },           // 18 Background Tilemap
	{ "03-15214-01.u92",           0x004000, 0x36f261ca, 6 | BRF_GRA },           // 19
	{ "03-15209-01.u69",           0x004000, 0x0f5d74a4, 6 | BRF_GRA },           // 20
	{ "03-15208-01.u68",           0x004000, 0x0319aec7, 6 | BRF_GRA },           // 21
	{ "03-15212-01.u90",           0x004000, 0x38b298d6, 6 | BRF_GRA },           // 22
	{ "03-15207-01.u67",           0x004000, 0x5ff86aad, 6 | BRF_GRA },           // 23
	{ "eeprom-quarterb.bin",       0x000080, 0x99d8c2ef, 8 | BRF_PRG | BRF_ESS }, // 24 Default EEPROM
};

STD_ROM_PICK(quarterb)
STD_ROM_FN(quarterb)

static const LelandRomLoad quarterbRomLoad[] = {
	{  0, 0, 0, 0x000000, 0x000000, 0x010000 },
	{  1, 0, 0, 0x000000, 0x010000, 0x010000 },
	{  2, 1, 0, 0x000000, 0x000000, 0x004000 },
	{  3, 1, 0, 0x000000, 0x010000, 0x002000 },
	{  3, 1, 0, 0x002000, 0x01c000, 0x002000 },
	{  4, 1, 0, 0x000000, 0x012000, 0x002000 },
	{  4, 1, 0, 0x002000, 0x01e000, 0x002000 },
	{  5, 1, 0, 0x000000, 0x014000, 0x002000 },
	{  5, 1, 0, 0x002000, 0x020000, 0x002000 },
	{  6, 1, 0, 0x000000, 0x016000, 0x002000 },
	{  6, 1, 0, 0x002000, 0x022000, 0x002000 },
	{  7, 1, 0, 0x000000, 0x018000, 0x002000 },
	{  7, 1, 0, 0x002000, 0x024000, 0x002000 },
	{  8, 1, 0, 0x000000, 0x01a000, 0x002000 },
	{  8, 1, 0, 0x002000, 0x026000, 0x002000 },
	{  9, 2, 1, 0x000000, 0x040001, 0x010000 },
	{ 10, 2, 1, 0x000000, 0x040000, 0x010000 },
	{ 11, 2, 1, 0x000000, 0x060001, 0x010000 },
	{ 12, 2, 1, 0x000000, 0x060000, 0x010000 },
	{ 13, 2, 1, 0x000000, 0x0e0001, 0x010000 },
	{ 14, 2, 1, 0x000000, 0x0e0000, 0x010000 },
	{ 15, 4, 0, 0x000000, 0x000000, 0x004000 },
	{ 16, 4, 0, 0x000000, 0x004000, 0x004000 },
	{ 17, 4, 0, 0x000000, 0x008000, 0x004000 },
	{ 18, 5, 0, 0x000000, 0x000000, 0x004000 },
	{ 19, 5, 0, 0x000000, 0x004000, 0x004000 },
	{ 20, 5, 0, 0x000000, 0x008000, 0x004000 },
	{ 21, 5, 0, 0x000000, 0x010000, 0x004000 },
	{ 22, 5, 0, 0x000000, 0x014000, 0x004000 },
	{ 23, 5, 0, 0x000000, 0x018000, 0x004000 },
	{ 24, 7, 0, 0x000000, 0x000000, 0x000080 },
	{ -1, 0, 0, 0, 0, 0 }
};

static const LelandRegions quarterbRegions = { { 0x020000, 0x080000, 0x100000, 0x000000, 0x00c000, 0x020000, 0x000000, 0x000080 }, 0x00 };

static INT32 quarterbInit()
{
	return DrvInit(&quarterb_cfg, quarterbRomLoad, &quarterbRegions);
}

struct BurnDriver BurnDrvQuarterb = {
	"quarterb", NULL, NULL, NULL, "1987",
	"Quarterback (rev 5)\0", NULL, "Leland Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, quarterbRomInfo, quarterbRomName, NULL, NULL, NULL, NULL, QuarterbInputInfo, NULL,
	quarterbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	240, 320, 3, 4
};

// Quarterback (rev 2)

static struct BurnRomInfo quarterbaRomDesc[] = {
	{ "03-15219-02.u49t",          0x010000, 0x7fbe1e5a, 1 | BRF_PRG | BRF_ESS }, //  0 Master Z80 Code
	{ "03-15218-02.u48t",          0x010000, 0x6fbd4b27, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "03-15200-01.u3",            0x004000, 0x83297861, 2 | BRF_PRG | BRF_ESS }, //  2 Slave Z80 Code
	{ "03-15201-01.u4",            0x004000, 0xaf8dbdab, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "03-15202-01.u5",            0x004000, 0x3eeecb3d, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "03-15203-01.u6",            0x004000, 0xb9c5b663, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "03-15204-01.u7",            0x004000, 0xc68821b7, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "03-15205-01.u8",            0x004000, 0x2be843a9, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "03-15206-01.u9",            0x004000, 0x6bf8d4ab, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "03-15222-01.u45t",          0x010000, 0x722d1a19, 3 | BRF_PRG | BRF_ESS }, //  9 80186 Code
	{ "03-15225-01.u62t",          0x010000, 0xf8c20496, 3 | BRF_PRG | BRF_ESS }, // 10
	{ "03-15221-01.u44t",          0x010000, 0xbc6abaaf, 3 | BRF_PRG | BRF_ESS }, // 11
	{ "03-15224-01.u61t",          0x010000, 0x7ce3c3b7, 3 | BRF_PRG | BRF_ESS }, // 12
	{ "03-15220-01.u43t",          0x010000, 0xccb6c8d7, 3 | BRF_PRG | BRF_ESS }, // 13
	{ "03-15223-01.u60t",          0x010000, 0xc0ee425d, 3 | BRF_PRG | BRF_ESS }, // 14
	{ "03-15215-01.u93",           0x004000, 0x4fb678d7, 5 | BRF_GRA },           // 15 Background Tiles
	{ "03-15216-01.u94",           0x004000, 0x7b57a44c, 5 | BRF_GRA },           // 16
	{ "03-15217-01.u95",           0x004000, 0x29bc33fd, 5 | BRF_GRA },           // 17
	{ "03-15210-01.u70",           0x004000, 0xa5aea20e, 6 | BRF_GRA },           // 18 Background Tilemap
	{ "03-15214-01.u92",           0x004000, 0x36f261ca, 6 | BRF_GRA },           // 19
	{ "03-15209-01.u69",           0x004000, 0x0f5d74a4, 6 | BRF_GRA },           // 20
	{ "03-15208-01.u68",           0x004000, 0x0319aec7, 6 | BRF_GRA },           // 21
	{ "03-15212-01.u90",           0x004000, 0x38b298d6, 6 | BRF_GRA },           // 22
	{ "03-15207-01.u67",           0x004000, 0x5ff86aad, 6 | BRF_GRA },           // 23
	{ "eeprom-quarterb.bin",       0x000080, 0x99d8c2ef, 8 | BRF_PRG | BRF_ESS }, // 24 Default EEPROM
};

STD_ROM_PICK(quarterba)
STD_ROM_FN(quarterba)

static INT32 quarterbaInit()
{
	return DrvInit(&quarterb_cfg, quarterbRomLoad, &quarterbRegions);
}

struct BurnDriver BurnDrvQuarterba = {
	"quarterba", "quarterb", NULL, NULL, "1987",
	"Quarterback (rev 2)\0", NULL, "Leland Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, quarterbaRomInfo, quarterbaRomName, NULL, NULL, NULL, NULL, QuarterbInputInfo, NULL,
	quarterbaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	240, 320, 3, 4
};

// Quarterback (rev 1, cocktail)

static struct BurnRomInfo quarterbcRomDesc[] = {
	{ "03-15506-01.u49t",          0x010000, 0xd750c5ed, 1 | BRF_PRG | BRF_ESS }, //  0 Master Z80 Code
	{ "03-15505-01.u48t",          0x010000, 0xe17b8a92, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "03-15200-01.u3",            0x004000, 0x83297861, 2 | BRF_PRG | BRF_ESS }, //  2 Slave Z80 Code
	{ "03-15201-01.u4",            0x004000, 0xaf8dbdab, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "03-15202-01.u5",            0x004000, 0x3eeecb3d, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "03-15203-01.u6",            0x004000, 0xb9c5b663, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "03-15204-01.u7",            0x004000, 0xc68821b7, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "03-15205-01.u8",            0x004000, 0x2be843a9, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "03-15206-01.u9",            0x004000, 0x6bf8d4ab, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "03-15222-02.u45t",          0x010000, 0x710bdc76, 3 | BRF_PRG | BRF_ESS }, //  9 80186 Code
	{ "03-15225-02.u62t",          0x010000, 0x041cecde, 3 | BRF_PRG | BRF_ESS }, // 10
	{ "03-15221-02.u44t",          0x010000, 0xe0459ddb, 3 | BRF_PRG | BRF_ESS }, // 11
	{ "03-15224-02.u61t",          0x010000, 0x9027c579, 3 | BRF_PRG | BRF_ESS }, // 12
	{ "03-15220-02.u43t",          0x010000, 0x48a8a018, 3 | BRF_PRG | BRF_ESS }, // 13
	{ "03-15223-02.u60t",          0x010000, 0x6a299766, 3 | BRF_PRG | BRF_ESS }, // 14
	{ "03-15502-01.u93",           0x004000, 0x5c2abd20, 5 | BRF_GRA },           // 15 Background Tiles
	{ "03-15503-01.u94",           0x004000, 0x544a192c, 5 | BRF_GRA },           // 16
	{ "03-15504-01.u95",           0x004000, 0xc8bb7ab4, 5 | BRF_GRA },           // 17
	{ "03-15500-01.u70",           0x004000, 0x1cf82a44, 6 | BRF_GRA },           // 18 Background Tilemap
	{ "03-15501-01.u92",           0x004000, 0x281be799, 6 | BRF_GRA },           // 19
	{ "03-15209-01.u69",           0x004000, 0x0f5d74a4, 6 | BRF_GRA },           // 20
	{ "03-15208-01.u68",           0x004000, 0x0319aec7, 6 | BRF_GRA },           // 21
	{ "03-15212-01.u90",           0x004000, 0x38b298d6, 6 | BRF_GRA },           // 22
	{ "03-15207-01.u67",           0x004000, 0x5ff86aad, 6 | BRF_GRA },           // 23
	{ "eeprom-quarterb.bin",       0x000080, 0x99d8c2ef, 8 | BRF_PRG | BRF_ESS }, // 24 Default EEPROM
};

STD_ROM_PICK(quarterbc)
STD_ROM_FN(quarterbc)

static INT32 quarterbcInit()
{
	return DrvInit(&quarterb_cfg, quarterbRomLoad, &quarterbRegions);
}

struct BurnDriver BurnDrvQuarterbc = {
	"quarterbc", "quarterb", NULL, NULL, "1987",
	"Quarterback (rev 1, cocktail)\0", NULL, "Leland Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, quarterbcRomInfo, quarterbcRomName, NULL, NULL, NULL, NULL, QuarterbInputInfo, NULL,
	quarterbcInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	240, 320, 3, 4
};

// Viper (rev 3)

static struct BurnRomInfo viperRomDesc[] = {
	{ "03-15617-03.u49t",          0x010000, 0x7e4688a6, 1 | BRF_PRG | BRF_ESS }, //  0 Master Z80 Code
	{ "03-15616-03.u48t",          0x010000, 0x3fe2f0bf, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "03-15600-02.u3",            0x002000, 0x0f57f68a, 2 | BRF_PRG | BRF_ESS }, //  2 Slave Z80 Code
	{ "03-15612-02.u2t",           0x010000, 0x4043d4ee, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "03-15613-02.u3t",           0x010000, 0x213bc02b, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "03-15614-02.u4t",           0x010000, 0xce0b95b4, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "03-15620-02.u45t",          0x010000, 0x7380ece1, 3 | BRF_PRG | BRF_ESS }, //  6 80186 Code
	{ "03-15623-02.u62t",          0x010000, 0x2921d8f9, 3 | BRF_PRG | BRF_ESS }, //  7
	{ "03-15619-02.u44t",          0x010000, 0xc8507cc2, 3 | BRF_PRG | BRF_ESS }, //  8
	{ "03-15622-02.u61t",          0x010000, 0x32dfda37, 3 | BRF_PRG | BRF_ESS }, //  9
	{ "03-15618-02.u43t",          0x010000, 0x5562e0c3, 3 | BRF_PRG | BRF_ESS }, // 10
	{ "03-15621-02.u60t",          0x010000, 0xcb468f2b, 3 | BRF_PRG | BRF_ESS }, // 11
	{ "03-15609-01.u93",           0x004000, 0x08ad92e9, 5 | BRF_GRA },           // 12 Background Tiles
	{ "03-15610-01.u94",           0x004000, 0xd4e56dfb, 5 | BRF_GRA },           // 13
	{ "03-15611-01.u95",           0x004000, 0x3a2c46fb, 5 | BRF_GRA },           // 14
	{ "03-15604-01.u70",           0x004000, 0x7e3b0cce, 6 | BRF_GRA },           // 15 Background Tilemap
	{ "03-15608-01.u92",           0x004000, 0xa9bde0ef, 6 | BRF_GRA },           // 16
	{ "03-15603-01.u69",           0x004000, 0xaecc9516, 6 | BRF_GRA },           // 17
	{ "03-15607-01.u91",           0x004000, 0x14f06f88, 6 | BRF_GRA },           // 18
	{ "03-15602-01.u68",           0x004000, 0x4ef613ad, 6 | BRF_GRA },           // 19
	{ "03-15606-01.u90",           0x004000, 0x3c2e8e76, 6 | BRF_GRA },           // 20
	{ "03-15601-01.u67",           0x004000, 0xdc7006cd, 6 | BRF_GRA },           // 21
	{ "03-15605-01.u89",           0x004000, 0x4aa9c788, 6 | BRF_GRA },           // 22
	{ "eeprom-viper.bin",          0x000080, 0x1c1e7a49, 8 | BRF_PRG | BRF_ESS }, // 23 Default EEPROM
};

STD_ROM_PICK(viper)
STD_ROM_FN(viper)

static const LelandRomLoad viperRomLoad[] = {
	{  0, 0, 0, 0x000000, 0x000000, 0x010000 },
	{  1, 0, 0, 0x000000, 0x010000, 0x010000 },
	{  2, 1, 0, 0x000000, 0x000000, 0x002000 },
	{  3, 1, 0, 0x000000, 0x010000, 0x010000 },
	{  4, 1, 0, 0x000000, 0x020000, 0x010000 },
	{  5, 1, 0, 0x000000, 0x030000, 0x010000 },
	{  6, 2, 1, 0x000000, 0x040001, 0x010000 },
	{  7, 2, 1, 0x000000, 0x040000, 0x010000 },
	{  8, 2, 1, 0x000000, 0x060001, 0x010000 },
	{  9, 2, 1, 0x000000, 0x060000, 0x010000 },
	{ 10, 2, 1, 0x000000, 0x0e0001, 0x010000 },
	{ 11, 2, 1, 0x000000, 0x0e0000, 0x010000 },
	{ 12, 4, 0, 0x000000, 0x000000, 0x004000 },
	{ 13, 4, 0, 0x000000, 0x004000, 0x004000 },
	{ 14, 4, 0, 0x000000, 0x008000, 0x004000 },
	{ 15, 5, 0, 0x000000, 0x000000, 0x004000 },
	{ 16, 5, 0, 0x000000, 0x004000, 0x004000 },
	{ 17, 5, 0, 0x000000, 0x008000, 0x004000 },
	{ 18, 5, 0, 0x000000, 0x00c000, 0x004000 },
	{ 19, 5, 0, 0x000000, 0x010000, 0x004000 },
	{ 20, 5, 0, 0x000000, 0x014000, 0x004000 },
	{ 21, 5, 0, 0x000000, 0x018000, 0x004000 },
	{ 22, 5, 0, 0x000000, 0x01c000, 0x004000 },
	{ 23, 7, 0, 0x000000, 0x000000, 0x000080 },
	{ -1, 0, 0, 0, 0, 0 }
};

static const LelandRegions viperRegions = { { 0x020000, 0x080000, 0x100000, 0x000000, 0x00c000, 0x020000, 0x000000, 0x000080 }, 0x00 };

static INT32 viperInit()
{
	return DrvInit(&viper_cfg, viperRomLoad, &viperRegions);
}

struct BurnDriver BurnDrvViper = {
	"viper", NULL, NULL, NULL, "1988",
	"Viper (rev 3)\0", NULL, "Leland Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, viperRomInfo, viperRomName, NULL, NULL, NULL, NULL, DangerzInputInfo, NULL,
	viperInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};

// John Elway's Team Quarterback (rev 3)

static struct BurnRomInfo teamqbRomDesc[] = {
	{ "03-15618-03.u58t",          0x010000, 0xb32568dc, 1 | BRF_PRG | BRF_ESS }, //  0 Master Z80 Code
	{ "03-15619-03.u59t",          0x010000, 0x40b3319f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "03-15600-01.u3",            0x002000, 0x46615844, 2 | BRF_PRG | BRF_ESS }, //  2 Slave Z80 Code
	{ "03-15601-01.u2t",           0x010000, 0x8e523c58, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "03-15602-01.u3t",           0x010000, 0x545b27a1, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "03-15603-01.u4t",           0x010000, 0xcdc9c09d, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "03-15604-01.u5t",           0x010000, 0x3c03e92e, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "03-15605-01.u6t",           0x010000, 0xcdf7d19c, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "03-15606-01.u7t",           0x010000, 0x8eeb007c, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "03-15607-01.u8t",           0x010000, 0x57cb6d2d, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "03-15623-01.u25t",          0x010000, 0x710bdc76, 3 | BRF_PRG | BRF_ESS }, // 10 80186 Code
	{ "03-15620-01.u13t",          0x010000, 0x7e5cb8ad, 3 | BRF_PRG | BRF_ESS }, // 11
	{ "03-15624-01.u26t",          0x010000, 0xdd090d33, 3 | BRF_PRG | BRF_ESS }, // 12
	{ "03-15621-01.u14t",          0x010000, 0xf68c68c9, 3 | BRF_PRG | BRF_ESS }, // 13
	{ "03-15625-01.u27t",          0x010000, 0xac442523, 3 | BRF_PRG | BRF_ESS }, // 14
	{ "03-15622-01.u15t",          0x010000, 0x9e84509a, 3 | BRF_PRG | BRF_ESS }, // 15
	{ "03-15615-01.u93",           0x004000, 0xa7ea6a87, 5 | BRF_GRA },           // 16 Background Tiles
	{ "03-15616-01.u94",           0x004000, 0x4a9b3900, 5 | BRF_GRA },           // 17
	{ "03-15617-01.u95",           0x004000, 0x2cd95edb, 5 | BRF_GRA },           // 18
	{ "03-15611-01.u70",           0x004000, 0xbf2695fb, 6 | BRF_GRA },           // 19 Background Tilemap
	{ "03-15614-01.u92",           0x004000, 0xc93fd870, 6 | BRF_GRA },           // 20
	{ "03-15610-01.u69",           0x004000, 0x3e5b786f, 6 | BRF_GRA },           // 21
	{ "03-15609-01.u68",           0x004000, 0x0319aec7, 6 | BRF_GRA },           // 22
	{ "03-15613-01.u90",           0x004000, 0x4805802e, 6 | BRF_GRA },           // 23
	{ "03-15608-01.u67",           0x004000, 0x78f0fd2b, 6 | BRF_GRA },           // 24
	{ "eeprom-teamqb.bin",         0x000080, 0x119fb1e7, 8 | BRF_PRG | BRF_ESS }, // 25 Default EEPROM
};

STD_ROM_PICK(teamqb)
STD_ROM_FN(teamqb)

static const LelandRomLoad teamqbRomLoad[] = {
	{  0, 0, 0, 0x000000, 0x000000, 0x010000 },
	{  1, 0, 0, 0x000000, 0x010000, 0x010000 },
	{  2, 1, 0, 0x000000, 0x000000, 0x002000 },
	{  3, 1, 0, 0x000000, 0x010000, 0x010000 },
	{  4, 1, 0, 0x000000, 0x020000, 0x010000 },
	{  5, 1, 0, 0x000000, 0x030000, 0x010000 },
	{  6, 1, 0, 0x000000, 0x040000, 0x010000 },
	{  7, 1, 0, 0x000000, 0x050000, 0x010000 },
	{  8, 1, 0, 0x000000, 0x060000, 0x010000 },
	{  9, 1, 0, 0x000000, 0x070000, 0x010000 },
	{ 10, 2, 1, 0x000000, 0x040001, 0x010000 },
	{ 11, 2, 1, 0x000000, 0x040000, 0x010000 },
	{ 12, 2, 1, 0x000000, 0x060001, 0x010000 },
	{ 13, 2, 1, 0x000000, 0x060000, 0x010000 },
	{ 14, 2, 1, 0x000000, 0x0e0001, 0x010000 },
	{ 15, 2, 1, 0x000000, 0x0e0000, 0x010000 },
	{ 16, 4, 0, 0x000000, 0x000000, 0x004000 },
	{ 17, 4, 0, 0x000000, 0x004000, 0x004000 },
	{ 18, 4, 0, 0x000000, 0x008000, 0x004000 },
	{ 19, 5, 0, 0x000000, 0x000000, 0x004000 },
	{ 20, 5, 0, 0x000000, 0x004000, 0x004000 },
	{ 21, 5, 0, 0x000000, 0x008000, 0x004000 },
	{ 22, 5, 0, 0x000000, 0x010000, 0x004000 },
	{ 23, 5, 0, 0x000000, 0x014000, 0x004000 },
	{ 24, 5, 0, 0x000000, 0x018000, 0x004000 },
	{ 25, 7, 0, 0x000000, 0x000000, 0x000080 },
	{ -1, 0, 0, 0, 0, 0 }
};

static const LelandRegions teamqbRegions = { { 0x020000, 0x080000, 0x100000, 0x000000, 0x00c000, 0x020000, 0x000000, 0x000080 }, 0x00 };

static INT32 teamqbInit()
{
	return DrvInit(&teamqb_cfg, teamqbRomLoad, &teamqbRegions);
}

struct BurnDriver BurnDrvTeamqb = {
	"teamqb", NULL, NULL, NULL, "1988",
	"John Elway's Team Quarterback (rev 3)\0", NULL, "Leland Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 4, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, teamqbRomInfo, teamqbRomName, NULL, NULL, NULL, NULL, TeamqbInputInfo, NULL,
	teamqbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	240, 320, 3, 4
};

// John Elway's Team Quarterback (rev 2)

static struct BurnRomInfo teamqb2RomDesc[] = {
	{ "03-15618-03.u58t",          0x010000, 0xb32568dc, 1 | BRF_PRG | BRF_ESS }, //  0 Master Z80 Code
	{ "03-15619-02.u59t",          0x010000, 0x6d533714, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "03-15600-01.u3",            0x002000, 0x46615844, 2 | BRF_PRG | BRF_ESS }, //  2 Slave Z80 Code
	{ "03-15601-01.u2t",           0x010000, 0x8e523c58, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "03-15602-01.u3t",           0x010000, 0x545b27a1, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "03-15603-01.u4t",           0x010000, 0xcdc9c09d, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "03-15604-01.u5t",           0x010000, 0x3c03e92e, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "03-15605-01.u6t",           0x010000, 0xcdf7d19c, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "03-15606-01.u7t",           0x010000, 0x8eeb007c, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "03-15607-01.u8t",           0x010000, 0x57cb6d2d, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "03-15623-01.u25t",          0x010000, 0x710bdc76, 3 | BRF_PRG | BRF_ESS }, // 10 80186 Code
	{ "03-15620-01.u13t",          0x010000, 0x7e5cb8ad, 3 | BRF_PRG | BRF_ESS }, // 11
	{ "03-15624-01.u26t",          0x010000, 0xdd090d33, 3 | BRF_PRG | BRF_ESS }, // 12
	{ "03-15621-01.u14t",          0x010000, 0xf68c68c9, 3 | BRF_PRG | BRF_ESS }, // 13
	{ "03-15625-01.u27t",          0x010000, 0xac442523, 3 | BRF_PRG | BRF_ESS }, // 14
	{ "03-15622-01.u15t",          0x010000, 0x9e84509a, 3 | BRF_PRG | BRF_ESS }, // 15
	{ "03-15615-01.u93",           0x004000, 0xa7ea6a87, 5 | BRF_GRA },           // 16 Background Tiles
	{ "03-15616-01.u94",           0x004000, 0x4a9b3900, 5 | BRF_GRA },           // 17
	{ "03-15617-01.u95",           0x004000, 0x2cd95edb, 5 | BRF_GRA },           // 18
	{ "03-15611-01.u70",           0x004000, 0xbf2695fb, 6 | BRF_GRA },           // 19 Background Tilemap
	{ "03-15614-01.u92",           0x004000, 0xc93fd870, 6 | BRF_GRA },           // 20
	{ "03-15610-01.u69",           0x004000, 0x3e5b786f, 6 | BRF_GRA },           // 21
	{ "03-15609-01.u68",           0x004000, 0x0319aec7, 6 | BRF_GRA },           // 22
	{ "03-15613-01.u90",           0x004000, 0x4805802e, 6 | BRF_GRA },           // 23
	{ "03-15608-01.u67",           0x004000, 0x78f0fd2b, 6 | BRF_GRA },           // 24
	{ "eeprom-teamqb.bin",         0x000080, 0x119fb1e7, 8 | BRF_PRG | BRF_ESS }, // 25 Default EEPROM
};

STD_ROM_PICK(teamqb2)
STD_ROM_FN(teamqb2)

static INT32 teamqb2Init()
{
	return DrvInit(&teamqb_cfg, teamqbRomLoad, &teamqbRegions);
}

struct BurnDriver BurnDrvTeamqb2 = {
	"teamqb2", "teamqb", NULL, NULL, "1988",
	"John Elway's Team Quarterback (rev 2)\0", NULL, "Leland Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 4, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, teamqb2RomInfo, teamqb2RomName, NULL, NULL, NULL, NULL, TeamqbInputInfo, NULL,
	teamqb2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	240, 320, 3, 4
};

// John Elway's Team Quarterback (rev 1)

static struct BurnRomInfo teamqb1RomDesc[] = {
	{ "03-15618-02.u58t",          0x010000, 0xd175a252, 1 | BRF_PRG | BRF_ESS }, //  0 Master Z80 Code
	{ "03-15619-01.u59t",          0x010000, 0x6e48c7c5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "03-15600-01.u3",            0x002000, 0x46615844, 2 | BRF_PRG | BRF_ESS }, //  2 Slave Z80 Code
	{ "03-15601-01.u2t",           0x010000, 0x8e523c58, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "03-15602-01.u3t",           0x010000, 0x545b27a1, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "03-15603-01.u4t",           0x010000, 0xcdc9c09d, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "03-15604-01.u5t",           0x010000, 0x3c03e92e, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "03-15605-01.u6t",           0x010000, 0xcdf7d19c, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "03-15606-01.u7t",           0x010000, 0x8eeb007c, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "03-15607-01.u8t",           0x010000, 0x57cb6d2d, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "03-15623-01.u25t",          0x010000, 0x710bdc76, 3 | BRF_PRG | BRF_ESS }, // 10 80186 Code
	{ "03-15620-01.u13t",          0x010000, 0x7e5cb8ad, 3 | BRF_PRG | BRF_ESS }, // 11
	{ "03-15624-01.u26t",          0x010000, 0xdd090d33, 3 | BRF_PRG | BRF_ESS }, // 12
	{ "03-15621-01.u14t",          0x010000, 0xf68c68c9, 3 | BRF_PRG | BRF_ESS }, // 13
	{ "03-15625-01.u27t",          0x010000, 0xac442523, 3 | BRF_PRG | BRF_ESS }, // 14
	{ "03-15622-01.u15t",          0x010000, 0x9e84509a, 3 | BRF_PRG | BRF_ESS }, // 15
	{ "03-15615-01.u93",           0x004000, 0xa7ea6a87, 5 | BRF_GRA },           // 16 Background Tiles
	{ "03-15616-01.u94",           0x004000, 0x4a9b3900, 5 | BRF_GRA },           // 17
	{ "03-15617-01.u95",           0x004000, 0x2cd95edb, 5 | BRF_GRA },           // 18
	{ "03-15611-01.u70",           0x004000, 0xbf2695fb, 6 | BRF_GRA },           // 19 Background Tilemap
	{ "03-15614-01.u92",           0x004000, 0xc93fd870, 6 | BRF_GRA },           // 20
	{ "03-15610-01.u69",           0x004000, 0x3e5b786f, 6 | BRF_GRA },           // 21
	{ "03-15609-01.u68",           0x004000, 0x0319aec7, 6 | BRF_GRA },           // 22
	{ "03-15613-01.u90",           0x004000, 0x4805802e, 6 | BRF_GRA },           // 23
	{ "03-15608-01.u67",           0x004000, 0x78f0fd2b, 6 | BRF_GRA },           // 24
	{ "eeprom-teamqb.bin",         0x000080, 0x119fb1e7, 8 | BRF_PRG | BRF_ESS }, // 25 Default EEPROM
};

STD_ROM_PICK(teamqb1)
STD_ROM_FN(teamqb1)

static INT32 teamqb1Init()
{
	return DrvInit(&teamqb_cfg, teamqbRomLoad, &teamqbRegions);
}

struct BurnDriver BurnDrvTeamqb1 = {
	"teamqb1", "teamqb", NULL, NULL, "1988",
	"John Elway's Team Quarterback (rev 1)\0", NULL, "Leland Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 4, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, teamqb1RomInfo, teamqb1RomName, NULL, NULL, NULL, NULL, TeamqbInputInfo, NULL,
	teamqb1Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	240, 320, 3, 4
};

// All American Football (Leland, rev E)

static struct BurnRomInfo aafbRomDesc[] = {
	{ "03-28011.u58t",             0x010000, 0xfa75a4a0, 1 | BRF_PRG | BRF_ESS }, //  0 Master Z80 Code
	{ "03-28012.u59t",             0x010000, 0xab6a606f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "03-28000.u3",               0x002000, 0xcb531986, 2 | BRF_PRG | BRF_ESS }, //  2 Slave Z80 Code
	{ "02-26001-01.u2t",           0x010000, 0xf118b9b4, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "02-24002-02.u3t",           0x010000, 0xbbb92184, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "02-15603-01.u4t",           0x010000, 0xcdc9c09d, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "02-15604-01.u5t",           0x010000, 0x3c03e92e, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "02-15605-01.u6t",           0x010000, 0xcdf7d19c, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "02-15606-01.u7t",           0x010000, 0x8eeb007c, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "03-28002.u8",               0x010000, 0xc3e09811, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "02-24019-01.u25",           0x010000, 0x9e344768, 3 | BRF_PRG | BRF_ESS }, // 10 80186 Code
	{ "02-24016-01.u13",           0x010000, 0x6997025f, 3 | BRF_PRG | BRF_ESS }, // 11
	{ "02-24020-01.u26",           0x010000, 0x0788f2a5, 3 | BRF_PRG | BRF_ESS }, // 12
	{ "02-24017-01.u14",           0x010000, 0xa48bd721, 3 | BRF_PRG | BRF_ESS }, // 13
	{ "02-24021-01.u27",           0x010000, 0x94081899, 3 | BRF_PRG | BRF_ESS }, // 14
	{ "02-24018-01.u15",           0x010000, 0x76eb6077, 3 | BRF_PRG | BRF_ESS }, // 15
	{ "03-28008.u93",              0x004000, 0x68f8addc, 5 | BRF_GRA },           // 16 Background Tiles
	{ "03-28009.u94",              0x004000, 0x669791ac, 5 | BRF_GRA },           // 17
	{ "03-28010.u95",              0x004000, 0xbd62aa8a, 5 | BRF_GRA },           // 18
	{ "03-28005.u70",              0x004000, 0x5ca6f4e2, 6 | BRF_GRA },           // 19 Background Tilemap
	{ "03-28007.u92",              0x004000, 0x1d9e33c2, 6 | BRF_GRA },           // 20
	{ "03-28004.u69",              0x004000, 0xd4b8a471, 6 | BRF_GRA },           // 21
	{ "03-28006.u90",              0x004000, 0xe68c8b6e, 6 | BRF_GRA },           // 22
	{ "03-28003.u67",              0x004000, 0xc92f6357, 6 | BRF_GRA },           // 23
	{ "eeprom-aafb.bin",           0x000080, 0x119fb1e7, 8 | BRF_PRG | BRF_ESS }, // 24 Default EEPROM
};

STD_ROM_PICK(aafb)
STD_ROM_FN(aafb)

static const LelandRomLoad aafbRomLoad[] = {
	{  0, 0, 0, 0x000000, 0x000000, 0x010000 },
	{  1, 0, 0, 0x000000, 0x010000, 0x010000 },
	{  2, 1, 0, 0x000000, 0x000000, 0x002000 },
	{  3, 1, 0, 0x000000, 0x010000, 0x010000 },
	{  4, 1, 0, 0x000000, 0x020000, 0x010000 },
	{  5, 1, 0, 0x000000, 0x030000, 0x010000 },
	{  6, 1, 0, 0x000000, 0x040000, 0x010000 },
	{  7, 1, 0, 0x000000, 0x050000, 0x010000 },
	{  8, 1, 0, 0x000000, 0x060000, 0x010000 },
	{  9, 1, 0, 0x000000, 0x070000, 0x010000 },
	{ 10, 2, 1, 0x000000, 0x040001, 0x010000 },
	{ 11, 2, 1, 0x000000, 0x040000, 0x010000 },
	{ 12, 2, 1, 0x000000, 0x060001, 0x010000 },
	{ 13, 2, 1, 0x000000, 0x060000, 0x010000 },
	{ 14, 2, 1, 0x000000, 0x0e0001, 0x010000 },
	{ 15, 2, 1, 0x000000, 0x0e0000, 0x010000 },
	{ 16, 4, 0, 0x000000, 0x000000, 0x004000 },
	{ 17, 4, 0, 0x000000, 0x004000, 0x004000 },
	{ 18, 4, 0, 0x000000, 0x008000, 0x004000 },
	{ 19, 5, 0, 0x000000, 0x000000, 0x004000 },
	{ 20, 5, 0, 0x000000, 0x004000, 0x004000 },
	{ 21, 5, 0, 0x000000, 0x008000, 0x004000 },
	{ 22, 5, 0, 0x000000, 0x014000, 0x004000 },
	{ 23, 5, 0, 0x000000, 0x018000, 0x004000 },
	{ 24, 7, 0, 0x000000, 0x000000, 0x000080 },
	{ -1, 0, 0, 0, 0, 0 }
};

static const LelandRegions aafbRegions = { { 0x020000, 0x080000, 0x100000, 0x000000, 0x00c000, 0x020000, 0x000000, 0x000080 }, 0x00 };

static INT32 aafbInit()
{
	return DrvInit(&aafb_cfg, aafbRomLoad, &aafbRegions);
}

struct BurnDriver BurnDrvAafb = {
	"aafb", NULL, NULL, NULL, "1989",
	"All American Football (Leland, rev E)\0", NULL, "Leland Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 4, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, aafbRomInfo, aafbRomName, NULL, NULL, NULL, NULL, TeamqbInputInfo, NULL,
	aafbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	240, 320, 3, 4
};

// All American Football (Leland, rev B)

static struct BurnRomInfo aafbbRomDesc[] = {
	{ "24014-01.u58t",             0x010000, 0x5db4a3d0, 1 | BRF_PRG | BRF_ESS }, //  0 Master Z80 Code
	{ "24015-01.u59t",             0x010000, 0xf384f716, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "02-24000-02.u3",            0x002000, 0x52df0354, 2 | BRF_PRG | BRF_ESS }, //  2 Slave Z80 Code
	{ "02-24001-02.u2t",           0x010000, 0x9b20697d, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "02-24002-02.u3t",           0x010000, 0xbbb92184, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "02-15603-01.u4t",           0x010000, 0xcdc9c09d, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "02-15604-01.u5t",           0x010000, 0x3c03e92e, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "02-15605-01.u6t",           0x010000, 0xcdf7d19c, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "02-15606-01.u7t",           0x010000, 0x8eeb007c, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "02-24002-02.u8t",           0x010000, 0x3d9747c9, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "02-24019-01.u25",           0x010000, 0x9e344768, 3 | BRF_PRG | BRF_ESS }, // 10 80186 Code
	{ "02-24016-01.u13",           0x010000, 0x6997025f, 3 | BRF_PRG | BRF_ESS }, // 11
	{ "02-24020-01.u26",           0x010000, 0x0788f2a5, 3 | BRF_PRG | BRF_ESS }, // 12
	{ "02-24017-01.u14",           0x010000, 0xa48bd721, 3 | BRF_PRG | BRF_ESS }, // 13
	{ "02-24021-01.u27",           0x010000, 0x94081899, 3 | BRF_PRG | BRF_ESS }, // 14
	{ "02-24018-01.u15",           0x010000, 0x76eb6077, 3 | BRF_PRG | BRF_ESS }, // 15
	{ "02-24011-02.u93",           0x008000, 0x71f4425b, 5 | BRF_GRA },           // 16 Background Tiles
	{ "02-24012-02.u94",           0x008000, 0xb2499547, 5 | BRF_GRA },           // 17
	{ "02-24013-02.u95",           0x008000, 0x0a604e0d, 5 | BRF_GRA },           // 18
	{ "02-24007-01.u70",           0x004000, 0x40e46aa4, 6 | BRF_GRA },           // 19 Background Tilemap
	{ "02-24010-01.u92",           0x004000, 0x78705f42, 6 | BRF_GRA },           // 20
	{ "02-24006-01.u69",           0x004000, 0x6a576aa9, 6 | BRF_GRA },           // 21
	{ "02-24009-02.u91",           0x004000, 0xb857a1ad, 6 | BRF_GRA },           // 22
	{ "02-24005-02.u68",           0x004000, 0x8ea75319, 6 | BRF_GRA },           // 23
	{ "02-24008-01.u90",           0x004000, 0x4538bc58, 6 | BRF_GRA },           // 24
	{ "02-24004-02.u67",           0x004000, 0xcd7a3338, 6 | BRF_GRA },           // 25
	{ "eeprom-aafb.bin",           0x000080, 0x119fb1e7, 8 | BRF_PRG | BRF_ESS }, // 26 Default EEPROM
};

STD_ROM_PICK(aafbb)
STD_ROM_FN(aafbb)

static const LelandRomLoad aafbbRomLoad[] = {
	{  0, 0, 0, 0x000000, 0x000000, 0x010000 },
	{  1, 0, 0, 0x000000, 0x010000, 0x010000 },
	{  2, 1, 0, 0x000000, 0x000000, 0x002000 },
	{  3, 1, 0, 0x000000, 0x010000, 0x010000 },
	{  4, 1, 0, 0x000000, 0x020000, 0x010000 },
	{  5, 1, 0, 0x000000, 0x030000, 0x010000 },
	{  6, 1, 0, 0x000000, 0x040000, 0x010000 },
	{  7, 1, 0, 0x000000, 0x050000, 0x010000 },
	{  8, 1, 0, 0x000000, 0x060000, 0x010000 },
	{  9, 1, 0, 0x000000, 0x070000, 0x010000 },
	{ 10, 2, 1, 0x000000, 0x040001, 0x010000 },
	{ 11, 2, 1, 0x000000, 0x040000, 0x010000 },
	{ 12, 2, 1, 0x000000, 0x060001, 0x010000 },
	{ 13, 2, 1, 0x000000, 0x060000, 0x010000 },
	{ 14, 2, 1, 0x000000, 0x0e0001, 0x010000 },
	{ 15, 2, 1, 0x000000, 0x0e0000, 0x010000 },
	{ 16, 4, 0, 0x000000, 0x000000, 0x008000 },
	{ 17, 4, 0, 0x000000, 0x008000, 0x008000 },
	{ 18, 4, 0, 0x000000, 0x010000, 0x008000 },
	{ 19, 5, 0, 0x000000, 0x000000, 0x004000 },
	{ 20, 5, 0, 0x000000, 0x004000, 0x004000 },
	{ 21, 5, 0, 0x000000, 0x008000, 0x004000 },
	{ 22, 5, 0, 0x000000, 0x00c000, 0x004000 },
	{ 23, 5, 0, 0x000000, 0x010000, 0x004000 },
	{ 24, 5, 0, 0x000000, 0x014000, 0x004000 },
	{ 25, 5, 0, 0x000000, 0x018000, 0x004000 },
	{ 26, 7, 0, 0x000000, 0x000000, 0x000080 },
	{ -1, 0, 0, 0, 0, 0 }
};

static const LelandRegions aafbbRegions = { { 0x020000, 0x080000, 0x100000, 0x000000, 0x018000, 0x020000, 0x000000, 0x000080 }, 0x00 };

static INT32 aafbbInit()
{
	return DrvInit(&aafbb_cfg, aafbbRomLoad, &aafbbRegions);
}

struct BurnDriver BurnDrvAafbb = {
	"aafbb", "aafb", NULL, NULL, "1989",
	"All American Football (Leland, rev B)\0", NULL, "Leland Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 4, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, aafbbRomInfo, aafbbRomName, NULL, NULL, NULL, NULL, TeamqbInputInfo, NULL,
	aafbbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	240, 320, 3, 4
};

// All American Football (Leland, rev C)

static struct BurnRomInfo aafbcRomDesc[] = {
	{ "02-24014-02.u58t",          0x010000, 0x25cc4ccc, 1 | BRF_PRG | BRF_ESS }, //  0 Master Z80 Code
	{ "02-24015-02.u59t",          0x010000, 0xbfa1b56f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "02-24000-02.u3",            0x002000, 0x52df0354, 2 | BRF_PRG | BRF_ESS }, //  2 Slave Z80 Code
	{ "02-24001-02.u2t",           0x010000, 0x9b20697d, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "02-24002-02.u3t",           0x010000, 0xbbb92184, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "02-15603-01.u4t",           0x010000, 0xcdc9c09d, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "02-15604-01.u5t",           0x010000, 0x3c03e92e, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "02-15605-01.u6t",           0x010000, 0xcdf7d19c, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "02-15606-01.u7t",           0x010000, 0x8eeb007c, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "02-24002-02.u8t",           0x010000, 0x3d9747c9, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "02-24019-01.u25",           0x010000, 0x9e344768, 3 | BRF_PRG | BRF_ESS }, // 10 80186 Code
	{ "02-24016-01.u13",           0x010000, 0x6997025f, 3 | BRF_PRG | BRF_ESS }, // 11
	{ "02-24020-01.u26",           0x010000, 0x0788f2a5, 3 | BRF_PRG | BRF_ESS }, // 12
	{ "02-24017-01.u14",           0x010000, 0xa48bd721, 3 | BRF_PRG | BRF_ESS }, // 13
	{ "02-24021-01.u27",           0x010000, 0x94081899, 3 | BRF_PRG | BRF_ESS }, // 14
	{ "02-24018-01.u15",           0x010000, 0x76eb6077, 3 | BRF_PRG | BRF_ESS }, // 15
	{ "02-24011-02.u93",           0x008000, 0x71f4425b, 5 | BRF_GRA },           // 16 Background Tiles
	{ "02-24012-02.u94",           0x008000, 0xb2499547, 5 | BRF_GRA },           // 17
	{ "02-24013-02.u95",           0x008000, 0x0a604e0d, 5 | BRF_GRA },           // 18
	{ "02-24007-01.u70",           0x004000, 0x40e46aa4, 6 | BRF_GRA },           // 19 Background Tilemap
	{ "02-24010-01.u92",           0x004000, 0x78705f42, 6 | BRF_GRA },           // 20
	{ "02-24006-01.u69",           0x004000, 0x6a576aa9, 6 | BRF_GRA },           // 21
	{ "02-24009-02.u91",           0x004000, 0xb857a1ad, 6 | BRF_GRA },           // 22
	{ "02-24005-02.u68",           0x004000, 0x8ea75319, 6 | BRF_GRA },           // 23
	{ "02-24008-01.u90",           0x004000, 0x4538bc58, 6 | BRF_GRA },           // 24
	{ "02-24004-02.u67",           0x004000, 0xcd7a3338, 6 | BRF_GRA },           // 25
	{ "eeprom-aafb.bin",           0x000080, 0x119fb1e7, 8 | BRF_PRG | BRF_ESS }, // 26 Default EEPROM
};

STD_ROM_PICK(aafbc)
STD_ROM_FN(aafbc)

static INT32 aafbcInit()
{
	return DrvInit(&aafbb_cfg, aafbbRomLoad, &aafbbRegions);
}

struct BurnDriver BurnDrvAafbc = {
	"aafbc", "aafb", NULL, NULL, "1989",
	"All American Football (Leland, rev C)\0", NULL, "Leland Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 4, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, aafbcRomInfo, aafbcRomName, NULL, NULL, NULL, NULL, TeamqbInputInfo, NULL,
	aafbcInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	240, 320, 3, 4
};

// All American Football (Leland, rev D, 2 players)

static struct BurnRomInfo aafbd2pRomDesc[] = {
	{ "02-26014-01.u58t",          0x010000, 0x79fd14cd, 1 | BRF_PRG | BRF_ESS }, //  0 Master Z80 Code
	{ "02-26015-01.u59t",          0x010000, 0x3b0382f0, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "02-26000-01.u3",            0x002000, 0x98c06c63, 2 | BRF_PRG | BRF_ESS }, //  2 Slave Z80 Code
	{ "02-26001-01.u2t",           0x010000, 0xf118b9b4, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "02-24002-02.u3t",           0x010000, 0xbbb92184, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "02-15603-01.u4t",           0x010000, 0xcdc9c09d, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "02-15604-01.u5t",           0x010000, 0x3c03e92e, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "02-15605-01.u6t",           0x010000, 0xcdf7d19c, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "02-15606-01.u7t",           0x010000, 0x8eeb007c, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "02-24002-02.u8t",           0x010000, 0x3d9747c9, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "02-24019-01.u25",           0x010000, 0x9e344768, 3 | BRF_PRG | BRF_ESS }, // 10 80186 Code
	{ "02-24016-01.u13",           0x010000, 0x6997025f, 3 | BRF_PRG | BRF_ESS }, // 11
	{ "02-24020-01.u26",           0x010000, 0x0788f2a5, 3 | BRF_PRG | BRF_ESS }, // 12
	{ "02-24017-01.u14",           0x010000, 0xa48bd721, 3 | BRF_PRG | BRF_ESS }, // 13
	{ "02-24021-01.u27",           0x010000, 0x94081899, 3 | BRF_PRG | BRF_ESS }, // 14
	{ "02-24018-01.u15",           0x010000, 0x76eb6077, 3 | BRF_PRG | BRF_ESS }, // 15
	{ "02-24011-02.u93",           0x008000, 0x71f4425b, 5 | BRF_GRA },           // 16 Background Tiles
	{ "02-24012-02.u94",           0x008000, 0xb2499547, 5 | BRF_GRA },           // 17
	{ "02-24013-02.u95",           0x008000, 0x0a604e0d, 5 | BRF_GRA },           // 18
	{ "02-24007-01.u70",           0x004000, 0x40e46aa4, 6 | BRF_GRA },           // 19 Background Tilemap
	{ "02-24010-01.u92",           0x004000, 0x78705f42, 6 | BRF_GRA },           // 20
	{ "02-24006-01.u69",           0x004000, 0x6a576aa9, 6 | BRF_GRA },           // 21
	{ "02-24009-02.u91",           0x004000, 0xb857a1ad, 6 | BRF_GRA },           // 22
	{ "02-24005-02.u68",           0x004000, 0x8ea75319, 6 | BRF_GRA },           // 23
	{ "02-24008-01.u90",           0x004000, 0x4538bc58, 6 | BRF_GRA },           // 24
	{ "02-24004-02.u67",           0x004000, 0xcd7a3338, 6 | BRF_GRA },           // 25
	{ "eeprom-aafb.bin",           0x000080, 0x119fb1e7, 8 | BRF_PRG | BRF_ESS }, // 26 Default EEPROM
};

STD_ROM_PICK(aafbd2p)
STD_ROM_FN(aafbd2p)

static INT32 aafbd2pInit()
{
	return DrvInit(&aafbd2p_cfg, aafbbRomLoad, &aafbbRegions);
}

struct BurnDriver BurnDrvAafbd2p = {
	"aafbd2p", "aafb", NULL, NULL, "1989",
	"All American Football (Leland, rev D, 2 players)\0", NULL, "Leland Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, aafbd2pRomInfo, aafbd2pRomName, NULL, NULL, NULL, NULL, Aafb2pInputInfo, NULL,
	aafbd2pInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	240, 320, 3, 4
};

// Ironman Ivan Stewart's Super Off-Road (rev 4)

static struct BurnRomInfo offroadRomDesc[] = {
	{ "03-22121-04.u58t",          0x010000, 0xc5790988, 1 | BRF_PRG | BRF_ESS }, //  0 Master Z80 Code
	{ "03-22122-03.u59t",          0x010000, 0xae862fdc, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "03-22120-01.u57t",          0x010000, 0xe9f0f175, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "03-22119-02.u56t",          0x010000, 0x38642f22, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "03-22100-02.u3",            0x002000, 0x08c96a4b, 2 | BRF_PRG | BRF_ESS }, //  4 Slave Z80 Code
	{ "03-22108-02.u4t",           0x010000, 0x0d72780a, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "03-22109-02.u5t",           0x010000, 0x5429ce2c, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "03-22110-02.u6t",           0x010000, 0xf97bad5c, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "03-22111-01.u7t",           0x010000, 0xf79157a1, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "03-22112-01.u8t",           0x010000, 0x3eef38d3, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "03-22116-03.u25t",          0x010000, 0x95bb31d3, 3 | BRF_PRG | BRF_ESS }, // 10 80186 Code
	{ "03-22113-03.u13t",          0x010000, 0x71b28df6, 3 | BRF_PRG | BRF_ESS }, // 11
	{ "03-22117-03.u26t",          0x010000, 0x703d81ce, 3 | BRF_PRG | BRF_ESS }, // 12
	{ "03-22114-03.u14t",          0x010000, 0xf8b31bf8, 3 | BRF_PRG | BRF_ESS }, // 13
	{ "03-22118-03.u27t",          0x010000, 0x806ccf8b, 3 | BRF_PRG | BRF_ESS }, // 14
	{ "03-22115-03.u15t",          0x010000, 0xc8439a7a, 3 | BRF_PRG | BRF_ESS }, // 15
	{ "03-22105-02.u93",           0x008000, 0x4426e367, 5 | BRF_GRA },           // 16 Background Tiles
	{ "03-22106-02.u94",           0x008000, 0x687dc1fc, 5 | BRF_GRA },           // 17
	{ "03-22107-02.u95",           0x008000, 0xcee6ee5f, 5 | BRF_GRA },           // 18
	{ "03-22104-01.u92",           0x004000, 0x03e0497d, 6 | BRF_GRA },           // 19 Background Tilemap
	{ "03-22102-01.u69",           0x004000, 0xc3f2e443, 6 | BRF_GRA },           // 20
	{ "03-22103-02.u90",           0x004000, 0x2266757a, 6 | BRF_GRA },           // 21
	{ "03-22101-02.u67",           0x004000, 0xecab0527, 6 | BRF_GRA },           // 22
	{ "eeprom-offroad.bin",        0x000080, 0x57955248, 8 | BRF_PRG | BRF_ESS }, // 23 Default EEPROM
};

STD_ROM_PICK(offroad)
STD_ROM_FN(offroad)

static const LelandRomLoad offroadRomLoad[] = {
	{  0, 0, 0, 0x000000, 0x000000, 0x010000 },
	{  1, 0, 0, 0x000000, 0x010000, 0x010000 },
	{  2, 0, 0, 0x000000, 0x020000, 0x010000 },
	{  3, 0, 0, 0x000000, 0x030000, 0x010000 },
	{  4, 1, 0, 0x000000, 0x000000, 0x002000 },
	{  5, 1, 0, 0x000000, 0x030000, 0x010000 },
	{  6, 1, 0, 0x000000, 0x040000, 0x010000 },
	{  7, 1, 0, 0x000000, 0x050000, 0x010000 },
	{  8, 1, 0, 0x000000, 0x060000, 0x010000 },
	{  9, 1, 0, 0x000000, 0x070000, 0x010000 },
	{ 10, 2, 1, 0x000000, 0x040001, 0x010000 },
	{ 11, 2, 1, 0x000000, 0x040000, 0x010000 },
	{ 12, 2, 1, 0x000000, 0x060001, 0x010000 },
	{ 13, 2, 1, 0x000000, 0x060000, 0x010000 },
	{ 14, 2, 1, 0x000000, 0x0e0001, 0x010000 },
	{ 15, 2, 1, 0x000000, 0x0e0000, 0x010000 },
	{ 16, 4, 0, 0x000000, 0x000000, 0x008000 },
	{ 17, 4, 0, 0x000000, 0x008000, 0x008000 },
	{ 18, 4, 0, 0x000000, 0x010000, 0x008000 },
	{ 19, 5, 0, 0x000000, 0x004000, 0x004000 },
	{ 20, 5, 0, 0x000000, 0x008000, 0x004000 },
	{ 21, 5, 0, 0x000000, 0x014000, 0x004000 },
	{ 22, 5, 0, 0x000000, 0x018000, 0x004000 },
	{ 23, 7, 0, 0x000000, 0x000000, 0x000080 },
	{ -1, 0, 0, 0, 0, 0 }
};

static const LelandRegions offroadRegions = { { 0x040000, 0x080000, 0x100000, 0x000000, 0x018000, 0x020000, 0x000000, 0x000080 }, 0x00 };

static INT32 offroadInit()
{
	return DrvInit(&offroad_cfg, offroadRomLoad, &offroadRegions);
}

struct BurnDriver BurnDrvOffroad = {
	"offroad", NULL, NULL, NULL, "1989",
	"Ironman Ivan Stewart's Super Off-Road (rev 4)\0", NULL, "Leland Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 3, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, offroadRomInfo, offroadRomName, NULL, NULL, NULL, NULL, OffroadInputInfo, NULL,
	offroadInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};

// Ironman Ivan Stewart's Super Off-Road (rev 3)

static struct BurnRomInfo offroad3RomDesc[] = {
	{ "03-22121-03.u58t",          0x010000, 0x8e3d0da1, 1 | BRF_PRG | BRF_ESS }, //  0 Master Z80 Code
	{ "03-22122-02.u59t",          0x010000, 0x37b8a0bd, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "03-22120-01.u57t",          0x010000, 0xe9f0f175, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "03-22119-01.u56t",          0x010000, 0xe7d144a8, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "03-22100-01.u3",            0x002000, 0x7c959be9, 2 | BRF_PRG | BRF_ESS }, //  4 Slave Z80 Code
	{ "03-22108-01.u4t",           0x010000, 0x7219f9ed, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "03-22109-01.u5t",           0x010000, 0x3d670117, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "03-22110-01.u6t",           0x010000, 0x22b4dc64, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "03-22111-01.u7t",           0x010000, 0xf79157a1, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "03-22112-01.u8t",           0x010000, 0x3eef38d3, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "03-22116-02.u25t",          0x010000, 0x1d906569, 3 | BRF_PRG | BRF_ESS }, // 10 80186 Code
	{ "03-22113-02.u13t",          0x010000, 0xde8a047b, 3 | BRF_PRG | BRF_ESS }, // 11
	{ "03-22117-02.u26t",          0x010000, 0x45f18abd, 3 | BRF_PRG | BRF_ESS }, // 12
	{ "03-22114-02.u14t",          0x010000, 0xa0257c5a, 3 | BRF_PRG | BRF_ESS }, // 13
	{ "03-22118-02.u27t",          0x010000, 0xfa62ad6f, 3 | BRF_PRG | BRF_ESS }, // 14
	{ "03-22115-02.u15t",          0x010000, 0x448648ae, 3 | BRF_PRG | BRF_ESS }, // 15
	{ "03-22105-01.u93",           0x008000, 0xe8af0080, 5 | BRF_GRA },           // 16 Background Tiles
	{ "03-22106-01.u94",           0x008000, 0x858f27b3, 5 | BRF_GRA },           // 17
	{ "03-22107-01.u95",           0x008000, 0x48059b20, 5 | BRF_GRA },           // 18
	{ "03-22104-01.u92",           0x004000, 0x03e0497d, 6 | BRF_GRA },           // 19 Background Tilemap
	{ "03-22102-01.u69",           0x004000, 0xc3f2e443, 6 | BRF_GRA },           // 20
	{ "03-22103-01.u90",           0x004000, 0x0ad8c061, 6 | BRF_GRA },           // 21
	{ "03-22101-01.u67",           0x004000, 0x19b46990, 6 | BRF_GRA },           // 22
	{ "eeprom-offroad.bin",        0x000080, 0x57955248, 8 | BRF_PRG | BRF_ESS }, // 23 Default EEPROM
};

STD_ROM_PICK(offroad3)
STD_ROM_FN(offroad3)

static INT32 offroad3Init()
{
	return DrvInit(&offroad_cfg, offroadRomLoad, &offroadRegions);
}

struct BurnDriver BurnDrvOffroad3 = {
	"offroad3", "offroad", NULL, NULL, "1989",
	"Ironman Ivan Stewart's Super Off-Road (rev 3)\0", NULL, "Leland Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 3, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, offroad3RomInfo, offroad3RomName, NULL, NULL, NULL, NULL, OffroadInputInfo, NULL,
	offroad3Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};

// Ironman Ivan Stewart's Super Off-Road Track-Pak (rev 4?)

static struct BurnRomInfo offroadtRomDesc[] = {
	{ "03-25016-0x.u58t",          0x010000, 0xadbc6211, 1 | BRF_PRG | BRF_ESS }, //  0 Master Z80 Code
	{ "03-25017-0x.u59t",          0x010000, 0x296dd3b6, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "03-22120-01.u57t",          0x010000, 0xe9f0f175, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "03-25015-01.u56t",          0x010000, 0x2c1a22b3, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "03-25000-01.u3",            0x002000, 0x95abb9f1, 2 | BRF_PRG | BRF_ESS }, //  4 Slave Z80 Code
	{ "03-25001-01.u2t",           0x010000, 0xc46c1627, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "03-25002-01.u3t",           0x010000, 0x2276546f, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "03-25003-01.u4t",           0x010000, 0xaa4b5975, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "03-25004-01.u5t",           0x010000, 0x69100b06, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "03-25005-01.u6t",           0x010000, 0xb75015b8, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "03-25006-01.u7t",           0x010000, 0xa5af5b4f, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "03-25007-01.u8t",           0x010000, 0x0f735078, 2 | BRF_PRG | BRF_ESS }, // 11
	{ "03-25021-01.u25t",          0x010000, 0xf952f800, 3 | BRF_PRG | BRF_ESS }, // 12 80186 Code
	{ "03-25018-01.u13t",          0x010000, 0x7beec9fc, 3 | BRF_PRG | BRF_ESS }, // 13
	{ "03-25022-01.u26t",          0x010000, 0x6227ea94, 3 | BRF_PRG | BRF_ESS }, // 14
	{ "03-25019-01.u14t",          0x010000, 0x0a44331d, 3 | BRF_PRG | BRF_ESS }, // 15
	{ "03-25023-01.u27t",          0x010000, 0xb80c5f99, 3 | BRF_PRG | BRF_ESS }, // 16
	{ "03-25020-01.u15t",          0x010000, 0x2a1a1c3c, 3 | BRF_PRG | BRF_ESS }, // 17
	{ "03-25012-01.u93",           0x008000, 0xf0c1d8b0, 5 | BRF_GRA },           // 18 Background Tiles
	{ "03-25013-01.u94",           0x008000, 0x7460d8c0, 5 | BRF_GRA },           // 19
	{ "03-25014-01.u95",           0x008000, 0x081ee7a8, 5 | BRF_GRA },           // 20
	{ "03-25011-01.u92",           0x004000, 0xf9988e28, 6 | BRF_GRA },           // 21 Background Tilemap
	{ "03-25009-01.u69",           0x004000, 0xfe5f8d8f, 6 | BRF_GRA },           // 22
	{ "03-25010-01.u90",           0x004000, 0xbda2ecb1, 6 | BRF_GRA },           // 23
	{ "03-25008-01.u67",           0x004000, 0x38c9bf29, 6 | BRF_GRA },           // 24
	{ "eeprom-offroad.bin",        0x000080, 0x57955248, 8 | BRF_PRG | BRF_ESS }, // 25 Default EEPROM
};

STD_ROM_PICK(offroadt)
STD_ROM_FN(offroadt)

static const LelandRomLoad offroadtRomLoad[] = {
	{  0, 0, 0, 0x000000, 0x000000, 0x010000 },
	{  1, 0, 0, 0x000000, 0x010000, 0x010000 },
	{  2, 0, 0, 0x000000, 0x020000, 0x010000 },
	{  3, 0, 0, 0x000000, 0x030000, 0x010000 },
	{  4, 1, 0, 0x000000, 0x000000, 0x002000 },
	{  5, 1, 0, 0x000000, 0x010000, 0x010000 },
	{  6, 1, 0, 0x000000, 0x020000, 0x010000 },
	{  7, 1, 0, 0x000000, 0x030000, 0x010000 },
	{  8, 1, 0, 0x000000, 0x040000, 0x010000 },
	{  9, 1, 0, 0x000000, 0x050000, 0x010000 },
	{ 10, 1, 0, 0x000000, 0x060000, 0x010000 },
	{ 11, 1, 0, 0x000000, 0x070000, 0x010000 },
	{ 12, 2, 1, 0x000000, 0x040001, 0x010000 },
	{ 13, 2, 1, 0x000000, 0x040000, 0x010000 },
	{ 14, 2, 1, 0x000000, 0x060001, 0x010000 },
	{ 15, 2, 1, 0x000000, 0x060000, 0x010000 },
	{ 16, 2, 1, 0x000000, 0x0e0001, 0x010000 },
	{ 17, 2, 1, 0x000000, 0x0e0000, 0x010000 },
	{ 18, 4, 0, 0x000000, 0x000000, 0x008000 },
	{ 19, 4, 0, 0x000000, 0x008000, 0x008000 },
	{ 20, 4, 0, 0x000000, 0x010000, 0x008000 },
	{ 21, 5, 0, 0x000000, 0x004000, 0x004000 },
	{ 22, 5, 0, 0x000000, 0x008000, 0x004000 },
	{ 23, 5, 0, 0x000000, 0x014000, 0x004000 },
	{ 24, 5, 0, 0x000000, 0x018000, 0x004000 },
	{ 25, 7, 0, 0x000000, 0x000000, 0x000080 },
	{ -1, 0, 0, 0, 0, 0 }
};

static const LelandRegions offroadtRegions = { { 0x040000, 0x090000, 0x100000, 0x000000, 0x018000, 0x020000, 0x000000, 0x000080 }, 0x00 };

static INT32 offroadtInit()
{
	return DrvInit(&offroadt_cfg, offroadtRomLoad, &offroadtRegions);
}

struct BurnDriver BurnDrvOffroadt = {
	"offroadt", NULL, NULL, NULL, "1989",
	"Ironman Ivan Stewart's Super Off-Road Track-Pak (rev 4?)\0", NULL, "Leland Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 3, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, offroadtRomInfo, offroadtRomName, NULL, NULL, NULL, NULL, OffroadInputInfo, NULL,
	offroadtInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};

// Ironman Ivan Stewart's Super Off-Road Track-Pak (rev 4, 2 players)

static struct BurnRomInfo offroadt2pRomDesc[] = {
	{ "03-27000-04.u58t",          0x010000, 0x8a785b66, 1 | BRF_PRG | BRF_ESS }, //  0 Master Z80 Code
	{ "03-27001-03.u59t",          0x010000, 0xff553895, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "03-22120-01.u57t",          0x010000, 0xe9f0f175, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "03-25015-01.u56t",          0x010000, 0x2c1a22b3, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "03-25000-01.u3",            0x002000, 0x95abb9f1, 2 | BRF_PRG | BRF_ESS }, //  4 Slave Z80 Code
	{ "03-25001-01.u2t",           0x010000, 0xc46c1627, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "03-25002-01.u3t",           0x010000, 0x2276546f, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "03-25003-01.u4t",           0x010000, 0xaa4b5975, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "03-25004-01.u5t",           0x010000, 0x69100b06, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "03-25005-01.u6t",           0x010000, 0xb75015b8, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "03-25006-01.u7t",           0x010000, 0xa5af5b4f, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "03-25007-01.u8t",           0x010000, 0x0f735078, 2 | BRF_PRG | BRF_ESS }, // 11
	{ "03-25021-01.u25t",          0x010000, 0xf952f800, 3 | BRF_PRG | BRF_ESS }, // 12 80186 Code
	{ "03-25018-01.u13t",          0x010000, 0x7beec9fc, 3 | BRF_PRG | BRF_ESS }, // 13
	{ "03-25022-01.u26t",          0x010000, 0x6227ea94, 3 | BRF_PRG | BRF_ESS }, // 14
	{ "03-25019-01.u14t",          0x010000, 0x0a44331d, 3 | BRF_PRG | BRF_ESS }, // 15
	{ "03-25023-01.u27t",          0x010000, 0xb80c5f99, 3 | BRF_PRG | BRF_ESS }, // 16
	{ "03-25020-01.u15t",          0x010000, 0x2a1a1c3c, 3 | BRF_PRG | BRF_ESS }, // 17
	{ "03-25012-01.u93",           0x008000, 0xf0c1d8b0, 5 | BRF_GRA },           // 18 Background Tiles
	{ "03-25013-01.u94",           0x008000, 0x7460d8c0, 5 | BRF_GRA },           // 19
	{ "03-25014-01.u95",           0x008000, 0x081ee7a8, 5 | BRF_GRA },           // 20
	{ "03-25011-01.u92",           0x004000, 0xf9988e28, 6 | BRF_GRA },           // 21 Background Tilemap
	{ "03-25009-01.u69",           0x004000, 0xfe5f8d8f, 6 | BRF_GRA },           // 22
	{ "03-25010-01.u90",           0x004000, 0xbda2ecb1, 6 | BRF_GRA },           // 23
	{ "03-25008-01.u67",           0x004000, 0x38c9bf29, 6 | BRF_GRA },           // 24
	{ "eeprom-offroad.bin",        0x000080, 0x57955248, 8 | BRF_PRG | BRF_ESS }, // 25 Default EEPROM
};

STD_ROM_PICK(offroadt2p)
STD_ROM_FN(offroadt2p)

static INT32 offroadt2pInit()
{
	return DrvInit(&offrd2p_cfg, offroadtRomLoad, &offroadtRegions);
}

struct BurnDriver BurnDrvOffroadt2p = {
	"offroadt2p", "offroadt", NULL, NULL, "1989",
	"Ironman Ivan Stewart's Super Off-Road Track-Pak (rev 4, 2 players)\0", NULL, "Leland Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, offroadt2pRomInfo, offroadt2pRomName, NULL, NULL, NULL, NULL, Offroadt2pInputInfo, NULL,
	offroadt2pInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};

// Pig Out: Dine Like a Swine! (rev 2?)

static struct BurnRomInfo pigoutRomDesc[] = {
	{ "03-29020-0x.u58t",          0x010000, 0x8fe4b683, 1 | BRF_PRG | BRF_ESS }, //  0 Master Z80 Code
	{ "03-29021-0x.u59t",          0x010000, 0xab907762, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "03-29019-01.u57t",          0x010000, 0xc22be0ff, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "03-29000-01.u3",            0x002000, 0xaf213cb7, 2 | BRF_PRG | BRF_ESS }, //  3 Slave Z80 Code
	{ "03-29001-01.u2t",           0x010000, 0xb23164c6, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "03-29002-01.u3t",           0x010000, 0xd93f105f, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "03-29003-01.u4t",           0x010000, 0xb7c47bfe, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "03-29004-01.u5t",           0x010000, 0xd9b9dfbf, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "03-29005-01.u6t",           0x010000, 0x728c7c1a, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "03-29006-01.u7t",           0x010000, 0x393bd990, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "03-29007-01.u8t",           0x010000, 0xcb9ffaad, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "03-29025-01.u25t",          0x010000, 0x92cd2617, 3 | BRF_PRG | BRF_ESS }, // 11 80186 Code
	{ "03-29022-01.u13t",          0x010000, 0x9448c389, 3 | BRF_PRG | BRF_ESS }, // 12
	{ "03-29026-01.u26t",          0x010000, 0xab57de8f, 3 | BRF_PRG | BRF_ESS }, // 13
	{ "03-29023-01.u14t",          0x010000, 0x30678e93, 3 | BRF_PRG | BRF_ESS }, // 14
	{ "03-29027-01.u27t",          0x010000, 0x37a8156e, 3 | BRF_PRG | BRF_ESS }, // 15
	{ "03-29024-01.u15t",          0x010000, 0x1c60d58b, 3 | BRF_PRG | BRF_ESS }, // 16
	{ "03-29016-01.u93",           0x008000, 0xf102a04d, 5 | BRF_GRA },           // 17 Background Tiles
	{ "03-29017-01.u94",           0x008000, 0xec63c015, 5 | BRF_GRA },           // 18
	{ "03-29018-01.u95",           0x008000, 0xba6e797e, 5 | BRF_GRA },           // 19
	{ "03-29011-01.u70",           0x004000, 0x7db4eaa1, 6 | BRF_GRA },           // 20 Background Tilemap
	{ "03-29015-01.u92",           0x004000, 0x20fa57bb, 6 | BRF_GRA },           // 21
	{ "03-29010-01.u69",           0x004000, 0xa16886f3, 6 | BRF_GRA },           // 22
	{ "03-29014-01.u91",           0x004000, 0x482a3581, 6 | BRF_GRA },           // 23
	{ "03-29009-01.u68",           0x004000, 0x7b62a3ed, 6 | BRF_GRA },           // 24
	{ "03-29013-01.u90",           0x004000, 0x9615d710, 6 | BRF_GRA },           // 25
	{ "03-29008-01.u67",           0x004000, 0xaf85ce79, 6 | BRF_GRA },           // 26
	{ "03-29012-01.u89",           0x004000, 0x6c874a05, 6 | BRF_GRA },           // 27
	{ "eeprom-pigout.bin",         0x000080, 0x9646fa72, 8 | BRF_PRG | BRF_ESS }, // 28 Default EEPROM
	{ "35-01.u44",                 0x000104, 0x00000000, 9 | BRF_NODUMP },        // 29 PLDs
	{ "36-01.u43",                 0x000104, 0x00000000, 9 | BRF_NODUMP },        // 30
	{ "37-01.u42",                 0x000104, 0x00000000, 9 | BRF_NODUMP },        // 31
	{ "38-01.u9",                  0x0000cc, 0x00000000, 9 | BRF_NODUMP },        // 32
	{ "39-01.u55",                 0x000149, 0x00000000, 9 | BRF_NODUMP },        // 33
	{ "40-01.u68",                 0x000149, 0x00000000, 9 | BRF_NODUMP },        // 34
	{ "41-02.u54",                 0x000104, 0x00000000, 9 | BRF_NODUMP },        // 35
	{ "01-01.u96",                 0x000104, 0x00000000, 9 | BRF_NODUMP },        // 36
	{ "02-29.u85",                 0x000104, 0x00000000, 9 | BRF_NODUMP },        // 37
	{ "03-01.u16",                 0x000104, 0x00000000, 9 | BRF_NODUMP },        // 38
	{ "04-01.u17",                 0x000104, 0x00000000, 9 | BRF_NODUMP },        // 39
	{ "05-01.u21",                 0x0000cc, 0x00000000, 9 | BRF_NODUMP },        // 40
	{ "06-01.u19",                 0x0000cc, 0x00000000, 9 | BRF_NODUMP },        // 41
	{ "07-01.u43",                 0x000104, 0x00000000, 9 | BRF_NODUMP },        // 42
	{ "08-01.u42",                 0x000104, 0x00000000, 9 | BRF_NODUMP },        // 43
	{ "09-01.u40",                 0x000104, 0x00000000, 9 | BRF_NODUMP },        // 44
	{ "10-01.u26",                 0x000104, 0x00000000, 9 | BRF_NODUMP },        // 45
	{ "11-01.u27",                 0x000104, 0x00000000, 9 | BRF_NODUMP },        // 46
};

STD_ROM_PICK(pigout)
STD_ROM_FN(pigout)

static const LelandRomLoad pigoutRomLoad[] = {
	{  0, 0, 0, 0x000000, 0x000000, 0x010000 },
	{  1, 0, 0, 0x000000, 0x010000, 0x010000 },
	{  2, 0, 0, 0x000000, 0x020000, 0x010000 },
	{  3, 1, 0, 0x000000, 0x000000, 0x002000 },
	{  4, 1, 0, 0x000000, 0x010000, 0x010000 },
	{  5, 1, 0, 0x000000, 0x020000, 0x010000 },
	{  6, 1, 0, 0x000000, 0x030000, 0x010000 },
	{  7, 1, 0, 0x000000, 0x040000, 0x010000 },
	{  8, 1, 0, 0x000000, 0x050000, 0x010000 },
	{  9, 1, 0, 0x000000, 0x060000, 0x010000 },
	{ 10, 1, 0, 0x000000, 0x070000, 0x010000 },
	{ 11, 2, 1, 0x000000, 0x040001, 0x010000 },
	{ 12, 2, 1, 0x000000, 0x040000, 0x010000 },
	{ 13, 2, 1, 0x000000, 0x060001, 0x010000 },
	{ 14, 2, 1, 0x000000, 0x060000, 0x010000 },
	{ 15, 2, 1, 0x000000, 0x0e0001, 0x010000 },
	{ 16, 2, 1, 0x000000, 0x0e0000, 0x010000 },
	{ 17, 4, 0, 0x000000, 0x000000, 0x008000 },
	{ 18, 4, 0, 0x000000, 0x008000, 0x008000 },
	{ 19, 4, 0, 0x000000, 0x010000, 0x008000 },
	{ 20, 5, 0, 0x000000, 0x000000, 0x004000 },
	{ 21, 5, 0, 0x000000, 0x004000, 0x004000 },
	{ 22, 5, 0, 0x000000, 0x008000, 0x004000 },
	{ 23, 5, 0, 0x000000, 0x00c000, 0x004000 },
	{ 24, 5, 0, 0x000000, 0x010000, 0x004000 },
	{ 25, 5, 0, 0x000000, 0x014000, 0x004000 },
	{ 26, 5, 0, 0x000000, 0x018000, 0x004000 },
	{ 27, 5, 0, 0x000000, 0x01c000, 0x004000 },
	{ 28, 7, 0, 0x000000, 0x000000, 0x000080 },
	{ -1, 0, 0, 0, 0, 0 }
};

static const LelandRegions pigoutRegions = { { 0x040000, 0x080000, 0x100000, 0x000000, 0x018000, 0x040000, 0x000000, 0x000080 }, 0x00 };

static INT32 pigoutInit()
{
	return DrvInit(&pigout_cfg, pigoutRomLoad, &pigoutRegions);
}

struct BurnDriver BurnDrvPigout = {
	"pigout", NULL, NULL, NULL, "1990",
	"Pig Out: Dine Like a Swine! (rev 2?)\0", NULL, "Leland Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 3, HARDWARE_MISC_POST90S, GBF_ACTION, 0,
	NULL, pigoutRomInfo, pigoutRomName, NULL, NULL, NULL, NULL, PigoutInputInfo, NULL,
	pigoutInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};

// Pig Out: Dine Like a Swine! (rev 1)

static struct BurnRomInfo pigoutaRomDesc[] = {
	{ "03-29020-01.u58t",          0x010000, 0x6c815982, 1 | BRF_PRG | BRF_ESS }, //  0 Master Z80 Code
	{ "03-29021-01.u59t",          0x010000, 0x9de7a763, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "03-29019-01.u57t",          0x010000, 0xc22be0ff, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "03-29000-01.u3",            0x002000, 0xaf213cb7, 2 | BRF_PRG | BRF_ESS }, //  3 Slave Z80 Code
	{ "03-29001-01.u2t",           0x010000, 0xb23164c6, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "03-29002-01.u3t",           0x010000, 0xd93f105f, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "03-29003-01.u4t",           0x010000, 0xb7c47bfe, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "03-29004-01.u5t",           0x010000, 0xd9b9dfbf, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "03-29005-01.u6t",           0x010000, 0x728c7c1a, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "03-29006-01.u7t",           0x010000, 0x393bd990, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "03-29007-01.u8t",           0x010000, 0xcb9ffaad, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "03-29025-01.u25t",          0x010000, 0x92cd2617, 3 | BRF_PRG | BRF_ESS }, // 11 80186 Code
	{ "03-29022-01.u13t",          0x010000, 0x9448c389, 3 | BRF_PRG | BRF_ESS }, // 12
	{ "03-29026-01.u26t",          0x010000, 0xab57de8f, 3 | BRF_PRG | BRF_ESS }, // 13
	{ "03-29023-01.u14t",          0x010000, 0x30678e93, 3 | BRF_PRG | BRF_ESS }, // 14
	{ "03-29027-01.u27t",          0x010000, 0x37a8156e, 3 | BRF_PRG | BRF_ESS }, // 15
	{ "03-29024-01.u15t",          0x010000, 0x1c60d58b, 3 | BRF_PRG | BRF_ESS }, // 16
	{ "03-29016-01.u93",           0x008000, 0xf102a04d, 5 | BRF_GRA },           // 17 Background Tiles
	{ "03-29017-01.u94",           0x008000, 0xec63c015, 5 | BRF_GRA },           // 18
	{ "03-29018-01.u95",           0x008000, 0xba6e797e, 5 | BRF_GRA },           // 19
	{ "03-29011-01.u70",           0x004000, 0x7db4eaa1, 6 | BRF_GRA },           // 20 Background Tilemap
	{ "03-29015-01.u92",           0x004000, 0x20fa57bb, 6 | BRF_GRA },           // 21
	{ "03-29010-01.u69",           0x004000, 0xa16886f3, 6 | BRF_GRA },           // 22
	{ "03-29014-01.u91",           0x004000, 0x482a3581, 6 | BRF_GRA },           // 23
	{ "03-29009-01.u68",           0x004000, 0x7b62a3ed, 6 | BRF_GRA },           // 24
	{ "03-29013-01.u90",           0x004000, 0x9615d710, 6 | BRF_GRA },           // 25
	{ "03-29008-01.u67",           0x004000, 0xaf85ce79, 6 | BRF_GRA },           // 26
	{ "03-29012-01.u89",           0x004000, 0x6c874a05, 6 | BRF_GRA },           // 27
	{ "eeprom-pigout.bin",         0x000080, 0x9646fa72, 8 | BRF_PRG | BRF_ESS }, // 28 Default EEPROM
	{ "35-01.u44",                 0x000104, 0x00000000, 9 | BRF_NODUMP },        // 29 PLDs
	{ "36-01.u43",                 0x000104, 0x00000000, 9 | BRF_NODUMP },        // 30
	{ "37-01.u42",                 0x000104, 0x00000000, 9 | BRF_NODUMP },        // 31
	{ "38-01.u9",                  0x0000cc, 0x00000000, 9 | BRF_NODUMP },        // 32
	{ "39-01.u55",                 0x000149, 0x00000000, 9 | BRF_NODUMP },        // 33
	{ "40-01.u68",                 0x000149, 0x00000000, 9 | BRF_NODUMP },        // 34
	{ "41-02.u54",                 0x000104, 0x00000000, 9 | BRF_NODUMP },        // 35
	{ "01-01.u96",                 0x000104, 0x00000000, 9 | BRF_NODUMP },        // 36
	{ "02-29.u85",                 0x000104, 0x00000000, 9 | BRF_NODUMP },        // 37
	{ "03-01.u16",                 0x000104, 0x00000000, 9 | BRF_NODUMP },        // 38
	{ "04-01.u17",                 0x000104, 0x00000000, 9 | BRF_NODUMP },        // 39
	{ "05-01.u21",                 0x0000cc, 0x00000000, 9 | BRF_NODUMP },        // 40
	{ "06-01.u19",                 0x0000cc, 0x00000000, 9 | BRF_NODUMP },        // 41
	{ "07-01.u43",                 0x000104, 0x00000000, 9 | BRF_NODUMP },        // 42
	{ "08-01.u42",                 0x000104, 0x00000000, 9 | BRF_NODUMP },        // 43
	{ "09-01.u40",                 0x000104, 0x00000000, 9 | BRF_NODUMP },        // 44
	{ "10-01.u26",                 0x000104, 0x00000000, 9 | BRF_NODUMP },        // 45
	{ "11-01.u27",                 0x000104, 0x00000000, 9 | BRF_NODUMP },        // 46
};

STD_ROM_PICK(pigouta)
STD_ROM_FN(pigouta)

static INT32 pigoutaInit()
{
	return DrvInit(&pigout_cfg, pigoutRomLoad, &pigoutRegions);
}

struct BurnDriver BurnDrvPigouta = {
	"pigouta", "pigout", NULL, NULL, "1990",
	"Pig Out: Dine Like a Swine! (rev 1)\0", NULL, "Leland Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 3, HARDWARE_MISC_POST90S, GBF_ACTION, 0,
	NULL, pigoutaRomInfo, pigoutaRomName, NULL, NULL, NULL, NULL, PigoutInputInfo, NULL,
	pigoutaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};

// Ataxx (rev 5)

static struct BurnRomInfo ataxxRomDesc[] = {
	{ "e-302-31005-05.u38",        0x020000, 0x3378937d, 1 | BRF_PRG | BRF_ESS }, //  0 Master Z80 Code
	{ "e-302-31012-01.u111",       0x020000, 0x9a3297cc, 2 | BRF_PRG | BRF_ESS }, //  1 Slave Z80 Code
	{ "e-302-31013-01.u112",       0x020000, 0x7e7c3e2f, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "e-302-31014-01.u113",       0x020000, 0x8cf3e101, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "e-302-31003-01.u15",        0x020000, 0x8bb3233b, 3 | BRF_PRG | BRF_ESS }, //  4 80186 Code
	{ "e-302-31001-01.u1",         0x020000, 0x728d75f2, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "e-302-31004-01.u16",        0x020000, 0xf2bdff48, 3 | BRF_PRG | BRF_ESS }, //  6
	{ "e-302-31002-01.u2",         0x020000, 0xca06a394, 3 | BRF_PRG | BRF_ESS }, //  7
	{ "e-302-31006-01.u98",        0x020000, 0x59d0f2ae, 5 | BRF_GRA },           //  8 Background Tiles
	{ "e-302-31007-01.u99",        0x020000, 0x6ab7db25, 5 | BRF_GRA },           //  9
	{ "e-302-31008-01.u100",       0x020000, 0x2352849e, 5 | BRF_GRA },           // 10
	{ "e-302-31009-01.u101",       0x020000, 0x4c31e02b, 5 | BRF_GRA },           // 11
	{ "e-302-31010-01.u102",       0x020000, 0xa951228c, 5 | BRF_GRA },           // 12
	{ "e-302-31011-01.u103",       0x020000, 0xed326164, 5 | BRF_GRA },           // 13
	{ "eeprom-ataxx.bin",          0x000100, 0x989cdb8c, 8 | BRF_PRG | BRF_ESS }, // 14 Default EEPROM
};

STD_ROM_PICK(ataxx)
STD_ROM_FN(ataxx)

static const LelandRomLoad ataxxRomLoad[] = {
	{  0, 0, 0, 0x000000, 0x000000, 0x020000 },
	{  0, 0, 0, 0x000000, 0x010000, 0x020000 },
	{  1, 1, 0, 0x000000, 0x000000, 0x020000 },
	{  2, 1, 0, 0x000000, 0x020000, 0x020000 },
	{  3, 1, 0, 0x000000, 0x040000, 0x020000 },
	{  4, 2, 1, 0x000000, 0x020001, 0x020000 },
	{  5, 2, 1, 0x000000, 0x020000, 0x020000 },
	{  6, 2, 1, 0x000000, 0x060001, 0x020000 },
	{  6, 2, 1, 0x000000, 0x0c0001, 0x020000 },
	{  7, 2, 1, 0x000000, 0x060000, 0x020000 },
	{  7, 2, 1, 0x000000, 0x0c0000, 0x020000 },
	{  8, 4, 0, 0x000000, 0x000000, 0x020000 },
	{  9, 4, 0, 0x000000, 0x020000, 0x020000 },
	{ 10, 4, 0, 0x000000, 0x040000, 0x020000 },
	{ 11, 4, 0, 0x000000, 0x060000, 0x020000 },
	{ 12, 4, 0, 0x000000, 0x080000, 0x020000 },
	{ 13, 4, 0, 0x000000, 0x0a0000, 0x020000 },
	{ 14, 7, 0, 0x000000, 0x000000, 0x000100 },
	{ -1, 0, 0, 0, 0, 0 }
};

static const LelandRegions ataxxRegions = { { 0x030000, 0x060000, 0x100000, 0x000000, 0x0c0000, 0x000000, 0x040000, 0x000100 }, 0x40 };

static INT32 ataxxInit()
{
	return DrvInit(&ataxx_cfg, ataxxRomLoad, &ataxxRegions);
}

struct BurnDriver BurnDrvAtaxx = {
	"ataxx", NULL, NULL, NULL, "1990",
	"Ataxx (rev 5)\0", NULL, "Leland Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_STRATEGY, 0,
	NULL, ataxxRomInfo, ataxxRomName, NULL, NULL, NULL, NULL, AtaxxInputInfo, NULL,
	ataxxInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};

// Ataxx (rev 4)

static struct BurnRomInfo ataxxaRomDesc[] = {
	{ "e-302-31005-04.u38",        0x020000, 0xe1cf6236, 1 | BRF_PRG | BRF_ESS }, //  0 Master Z80 Code
	{ "e-302-31012-01.u111",       0x020000, 0x9a3297cc, 2 | BRF_PRG | BRF_ESS }, //  1 Slave Z80 Code
	{ "e-302-31013-01.u112",       0x020000, 0x7e7c3e2f, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "e-302-31014-01.u113",       0x020000, 0x8cf3e101, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "e-302-31003-01.u15",        0x020000, 0x8bb3233b, 3 | BRF_PRG | BRF_ESS }, //  4 80186 Code
	{ "e-302-31001-01.u1",         0x020000, 0x728d75f2, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "e-302-31004-01.u16",        0x020000, 0xf2bdff48, 3 | BRF_PRG | BRF_ESS }, //  6
	{ "e-302-31002-01.u2",         0x020000, 0xca06a394, 3 | BRF_PRG | BRF_ESS }, //  7
	{ "e-302-31006-01.u98",        0x020000, 0x59d0f2ae, 5 | BRF_GRA },           //  8 Background Tiles
	{ "e-302-31007-01.u99",        0x020000, 0x6ab7db25, 5 | BRF_GRA },           //  9
	{ "e-302-31008-01.u100",       0x020000, 0x2352849e, 5 | BRF_GRA },           // 10
	{ "e-302-31009-01.u101",       0x020000, 0x4c31e02b, 5 | BRF_GRA },           // 11
	{ "e-302-31010-01.u102",       0x020000, 0xa951228c, 5 | BRF_GRA },           // 12
	{ "e-302-31011-01.u103",       0x020000, 0xed326164, 5 | BRF_GRA },           // 13
	{ "eeprom-ataxx.bin",          0x000100, 0x989cdb8c, 8 | BRF_PRG | BRF_ESS }, // 14 Default EEPROM
};

STD_ROM_PICK(ataxxa)
STD_ROM_FN(ataxxa)

static INT32 ataxxaInit()
{
	return DrvInit(&ataxx_cfg, ataxxRomLoad, &ataxxRegions);
}

struct BurnDriver BurnDrvAtaxxa = {
	"ataxxa", "ataxx", NULL, NULL, "1990",
	"Ataxx (rev 4)\0", NULL, "Leland Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_STRATEGY, 0,
	NULL, ataxxaRomInfo, ataxxaRomName, NULL, NULL, NULL, NULL, AtaxxInputInfo, NULL,
	ataxxaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};

// Ataxx (Europe)

static struct BurnRomInfo ataxxeRomDesc[] = {
	{ "euro_ataxx_u38_3079.u38",   0x020000, 0x16aef3b7, 1 | BRF_PRG | BRF_ESS }, //  0 Master Z80 Code
	{ "e-302-31012-01.u111",       0x020000, 0x9a3297cc, 2 | BRF_PRG | BRF_ESS }, //  1 Slave Z80 Code
	{ "e-302-31013-01.u112",       0x020000, 0x7e7c3e2f, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "e-302-31014-01.u113",       0x020000, 0x8cf3e101, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "302-38003-01.u15",          0x020000, 0xdb266d3f, 3 | BRF_PRG | BRF_ESS }, //  4 80186 Code
	{ "302-38001-01.u1",           0x020000, 0xd6db2724, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "302-38004-01.u16",          0x020000, 0x2b127f56, 3 | BRF_PRG | BRF_ESS }, //  6
	{ "302-38002-01.u2",           0x020000, 0x1b63b882, 3 | BRF_PRG | BRF_ESS }, //  7
	{ "e-302-31006-01.u98",        0x020000, 0x59d0f2ae, 5 | BRF_GRA },           //  8 Background Tiles
	{ "e-302-31007-01.u99",        0x020000, 0x6ab7db25, 5 | BRF_GRA },           //  9
	{ "e-302-31008-01.u100",       0x020000, 0x2352849e, 5 | BRF_GRA },           // 10
	{ "e-302-31009-01.u101",       0x020000, 0x4c31e02b, 5 | BRF_GRA },           // 11
	{ "e-302-31010-01.u102",       0x020000, 0xa951228c, 5 | BRF_GRA },           // 12
	{ "e-302-31011-01.u103",       0x020000, 0xed326164, 5 | BRF_GRA },           // 13
	{ "eeprom-ataxxe.bin",         0x000100, 0x8df1dee1, 8 | BRF_PRG | BRF_ESS }, // 14 Default EEPROM
};

STD_ROM_PICK(ataxxe)
STD_ROM_FN(ataxxe)

static INT32 ataxxeInit()
{
	return DrvInit(&ataxx_cfg, ataxxRomLoad, &ataxxRegions);
}

struct BurnDriver BurnDrvAtaxxe = {
	"ataxxe", "ataxx", NULL, NULL, "1990",
	"Ataxx (Europe)\0", NULL, "Leland Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_STRATEGY, 0,
	NULL, ataxxeRomInfo, ataxxeRomName, NULL, NULL, NULL, NULL, AtaxxInputInfo, NULL,
	ataxxeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};

// Ataxx (Japan)

static struct BurnRomInfo ataxxjRomDesc[] = {
	{ "ataxxj.u38",                0x020000, 0x513fa7d4, 1 | BRF_PRG | BRF_ESS }, //  0 Master Z80 Code
	{ "e-302-31012-01.u111",       0x020000, 0x9a3297cc, 2 | BRF_PRG | BRF_ESS }, //  1 Slave Z80 Code
	{ "e-302-31013-01.u112",       0x020000, 0x7e7c3e2f, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "e-302-31014-01.u113",       0x020000, 0x8cf3e101, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "302-38003-01.u15",          0x020000, 0xdb266d3f, 3 | BRF_PRG | BRF_ESS }, //  4 80186 Code
	{ "302-38001-01.u1",           0x020000, 0xd6db2724, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "302-38004-01.u16",          0x020000, 0x2b127f56, 3 | BRF_PRG | BRF_ESS }, //  6
	{ "302-38002-01.u2",           0x020000, 0x1b63b882, 3 | BRF_PRG | BRF_ESS }, //  7
	{ "e-302-31006-01.u98",        0x020000, 0x59d0f2ae, 5 | BRF_GRA },           //  8 Background Tiles
	{ "e-302-31007-01.u99",        0x020000, 0x6ab7db25, 5 | BRF_GRA },           //  9
	{ "e-302-31008-01.u100",       0x020000, 0x2352849e, 5 | BRF_GRA },           // 10
	{ "e-302-31009-01.u101",       0x020000, 0x4c31e02b, 5 | BRF_GRA },           // 11
	{ "e-302-31010-01.u102",       0x020000, 0xa951228c, 5 | BRF_GRA },           // 12
	{ "e-302-31011-01.u103",       0x020000, 0xed326164, 5 | BRF_GRA },           // 13
	{ "eeprom-ataxxj.bin",         0x000100, 0x8df1dee1, 8 | BRF_PRG | BRF_ESS }, // 14 Default EEPROM
};

STD_ROM_PICK(ataxxj)
STD_ROM_FN(ataxxj)

static INT32 ataxxjInit()
{
	return DrvInit(&ataxx_cfg, ataxxRomLoad, &ataxxRegions);
}

struct BurnDriver BurnDrvAtaxxj = {
	"ataxxj", "ataxx", NULL, NULL, "1990",
	"Ataxx (Japan)\0", NULL, "Leland Corporation (Capcom license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_STRATEGY, 0,
	NULL, ataxxjRomInfo, ataxxjRomName, NULL, NULL, NULL, NULL, AtaxxInputInfo, NULL,
	ataxxjInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};

// World Soccer Finals (rev 4)

static struct BurnRomInfo wsfRomDesc[] = {
	{ "e-302-30022-04.u64",        0x020000, 0x533cc90f, 1 | BRF_PRG | BRF_ESS }, //  0 Master Z80 Code
	{ "e-302-30023-04.u65",        0x020000, 0x763c6c1b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "e-302-30001-01.u151",       0x020000, 0x31c63af5, 2 | BRF_PRG | BRF_ESS }, //  2 Slave Z80 Code
	{ "e-302-30002-01.u152",       0x020000, 0xa53e88a6, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "e-302-30003-01.u153",       0x020000, 0x12afad1d, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "e-302-30004-01.u154",       0x020000, 0xb8b3d59c, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "e-302-30005-01.u155",       0x020000, 0x505724b9, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "e-302-30006-01.u156",       0x020000, 0xc86b5c4d, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "e-302-30007-01.u157",       0x020000, 0x451321ae, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "e-302-30008-01.u158",       0x020000, 0x4d23836f, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "e-302-30017-02.u3",         0x020000, 0xb1d578e1, 3 | BRF_PRG | BRF_ESS }, // 10 80186 Code
	{ "e-302-30020-02.u6",         0x020000, 0x919a62ee, 3 | BRF_PRG | BRF_ESS }, // 11
	{ "e-302-30018-02.u4",         0x020000, 0xd24947ee, 3 | BRF_PRG | BRF_ESS }, // 12
	{ "e-302-30019-02.u5",         0x020000, 0xd846f292, 3 | BRF_PRG | BRF_ESS }, // 13
	{ "e-302-30011-03.u145",       0x010000, 0x6d1c1f19, 5 | BRF_GRA },           // 14 Background Tiles
	{ "e-302-30012-03.u146",       0x010000, 0x6b69bc9e, 5 | BRF_GRA },           // 15
	{ "e-302-30013-03.u147",       0x010000, 0xe1b1e36f, 5 | BRF_GRA },           // 16
	{ "e-302-30014-02.u148",       0x010000, 0x4a5e7d2b, 5 | BRF_GRA },           // 17
	{ "e-302-30015-02.u149",       0x010000, 0xb30c4ff3, 5 | BRF_GRA },           // 18
	{ "e-302-30016-02.u150",       0x010000, 0x3bc8efac, 5 | BRF_GRA },           // 19
	{ "e-302-30009-01.u68",        0x010000, 0xf2fbfc15, 7 | BRF_PRG | BRF_ESS }, // 20 XROM Data
	{ "e-302-30010-01.u69",        0x010000, 0xb4ed2d3b, 7 | BRF_PRG | BRF_ESS }, // 21
	{ "e-302-30021-02.u8",         0x020000, 0xa8f97be4, 4 | BRF_SND },           // 22 External DAC Samples
	{ "eeprom-wsf.bin",            0x000100, 0x5bd0633d, 8 | BRF_PRG | BRF_ESS }, // 23 Default EEPROM
};

STD_ROM_PICK(wsf)
STD_ROM_FN(wsf)

static const LelandRomLoad wsfRomLoad[] = {
	{  0, 0, 0, 0x000000, 0x000000, 0x020000 },
	{  0, 0, 0, 0x000000, 0x010000, 0x020000 },
	{  1, 0, 0, 0x000000, 0x030000, 0x020000 },
	{  2, 1, 0, 0x000000, 0x000000, 0x020000 },
	{  3, 1, 0, 0x000000, 0x020000, 0x020000 },
	{  4, 1, 0, 0x000000, 0x040000, 0x020000 },
	{  5, 1, 0, 0x000000, 0x060000, 0x020000 },
	{  6, 1, 0, 0x000000, 0x080000, 0x020000 },
	{  7, 1, 0, 0x000000, 0x0a0000, 0x020000 },
	{  8, 1, 0, 0x000000, 0x0c0000, 0x020000 },
	{  9, 1, 0, 0x000000, 0x0e0000, 0x020000 },
	{ 10, 2, 1, 0x000000, 0x020001, 0x020000 },
	{ 11, 2, 1, 0x000000, 0x020000, 0x020000 },
	{ 12, 2, 1, 0x000000, 0x060001, 0x020000 },
	{ 12, 2, 1, 0x000000, 0x0c0001, 0x020000 },
	{ 13, 2, 1, 0x000000, 0x060000, 0x020000 },
	{ 13, 2, 1, 0x000000, 0x0c0000, 0x020000 },
	{ 14, 4, 0, 0x000000, 0x000000, 0x010000 },
	{ 15, 4, 0, 0x000000, 0x010000, 0x010000 },
	{ 16, 4, 0, 0x000000, 0x020000, 0x010000 },
	{ 17, 4, 0, 0x000000, 0x030000, 0x010000 },
	{ 18, 4, 0, 0x000000, 0x040000, 0x010000 },
	{ 19, 4, 0, 0x000000, 0x050000, 0x010000 },
	{ 20, 6, 1, 0x000000, 0x000000, 0x010000 },
	{ 21, 6, 1, 0x000000, 0x000001, 0x010000 },
	{ 22, 3, 0, 0x000000, 0x000000, 0x020000 },
	{ 23, 7, 0, 0x000000, 0x000000, 0x000100 },
	{ -1, 0, 0, 0, 0, 0 }
};

static const LelandRegions wsfRegions = { { 0x050000, 0x100000, 0x100000, 0x020000, 0x060000, 0x000000, 0x040000, 0x000100 }, 0x40 };

static INT32 wsfInit()
{
	return DrvInit(&wsf_cfg, wsfRomLoad, &wsfRegions);
}

struct BurnDriver BurnDrvWsf = {
	"wsf", NULL, NULL, NULL, "1990",
	"World Soccer Finals (rev 4)\0", NULL, "Leland Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_MISC_POST90S, GBF_SPORTSFOOTBALL, 0,
	NULL, wsfRomInfo, wsfRomName, NULL, NULL, NULL, NULL, WsfInputInfo, NULL,
	wsfInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};

// World Soccer Finals (rev 3)

static struct BurnRomInfo wsf3RomDesc[] = {
	{ "e-302-30022-03.u64",        0x020000, 0x2e7faa96, 1 | BRF_PRG | BRF_ESS }, //  0 Master Z80 Code
	{ "e-302-30023-03.u65",        0x020000, 0x7146328f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "e-302-30001-01.u151",       0x020000, 0x31c63af5, 2 | BRF_PRG | BRF_ESS }, //  2 Slave Z80 Code
	{ "e-302-30002-01.u152",       0x020000, 0xa53e88a6, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "e-302-30003-01.u153",       0x020000, 0x12afad1d, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "e-302-30004-01.u154",       0x020000, 0xb8b3d59c, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "e-302-30005-01.u155",       0x020000, 0x505724b9, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "e-302-30006-01.u156",       0x020000, 0xc86b5c4d, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "e-302-30007-01.u157",       0x020000, 0x451321ae, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "e-302-30008-01.u158",       0x020000, 0x4d23836f, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "e-302-30017-01.u3",         0x020000, 0x39ec13c1, 3 | BRF_PRG | BRF_ESS }, // 10 80186 Code
	{ "e-302-30020-01.u6",         0x020000, 0x532c02bf, 3 | BRF_PRG | BRF_ESS }, // 11
	{ "e-302-30018-01.u4",         0x020000, 0x1ec16735, 3 | BRF_PRG | BRF_ESS }, // 12
	{ "e-302-30019-01.u5",         0x020000, 0x2881f73b, 3 | BRF_PRG | BRF_ESS }, // 13
	{ "e-302-30011-02.u145",       0x010000, 0x6153569b, 5 | BRF_GRA },           // 14 Background Tiles
	{ "e-302-30012-02.u146",       0x010000, 0x52d65e21, 5 | BRF_GRA },           // 15
	{ "e-302-30013-02.u147",       0x010000, 0xb3afda12, 5 | BRF_GRA },           // 16
	{ "e-302-30014-01.u148",       0x010000, 0x624e6c64, 5 | BRF_GRA },           // 17
	{ "e-302-30015-01.u149",       0x010000, 0x5d9064f2, 5 | BRF_GRA },           // 18
	{ "e-302-30016-01.u150",       0x010000, 0xd76389cd, 5 | BRF_GRA },           // 19
	{ "e-302-30009-01.u68",        0x010000, 0xf2fbfc15, 7 | BRF_PRG | BRF_ESS }, // 20 XROM Data
	{ "e-302-30010-01.u69",        0x010000, 0xb4ed2d3b, 7 | BRF_PRG | BRF_ESS }, // 21
	{ "e-302-30021-01.u8",         0x020000, 0xbb91dc10, 4 | BRF_SND },           // 22 External DAC Samples
	{ "eeprom-wsf.bin",            0x000100, 0x5bd0633d, 8 | BRF_PRG | BRF_ESS }, // 23 Default EEPROM
};

STD_ROM_PICK(wsf3)
STD_ROM_FN(wsf3)

static INT32 wsf3Init()
{
	return DrvInit(&wsf_cfg, wsfRomLoad, &wsfRegions);
}

struct BurnDriver BurnDrvWsf3 = {
	"wsf3", "wsf", NULL, NULL, "1990",
	"World Soccer Finals (rev 3)\0", NULL, "Leland Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_MISC_POST90S, GBF_SPORTSFOOTBALL, 0,
	NULL, wsf3RomInfo, wsf3RomName, NULL, NULL, NULL, NULL, WsfInputInfo, NULL,
	wsf3Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};

// Danny Sullivan's Indy Heat (rev 1)

static struct BurnRomInfo indyheatRomDesc[] = {
	{ "e-302-33019-01.u64",        0x020000, 0x2b97a347, 1 | BRF_PRG | BRF_ESS }, //  0 Master Z80 Code
	{ "e-302-33020-01.u65",        0x020000, 0x71301d74, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "e-302-33017-01.u66",        0x020000, 0xc9612072, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "e-302-33018-01.u67",        0x020000, 0x4c4b25e0, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "e-302-33007-01.u151",       0x020000, 0x2622dfa4, 2 | BRF_PRG | BRF_ESS }, //  4 Slave Z80 Code
	{ "e-302-33008-01.u152",       0x040000, 0xad40e4e2, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "e-302-33009-01.u153",       0x040000, 0x1e3803f7, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "e-302-33010-01.u154",       0x020000, 0x76d3c235, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "e-302-33011-01.u155",       0x020000, 0xd5d866b3, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "e-302-33012-01.u156",       0x020000, 0x7fe71842, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "e-302-33013-01.u157",       0x020000, 0xa6462adc, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "e-302-33014-01.u158",       0x020000, 0xd6ef27a3, 2 | BRF_PRG | BRF_ESS }, // 11
	{ "e-302-33021-01.u3",         0x020000, 0x97413818, 3 | BRF_PRG | BRF_ESS }, // 12 80186 Code
	{ "e-302-33024-01.u6",         0x020000, 0x15a89962, 3 | BRF_PRG | BRF_ESS }, // 13
	{ "e-302-33022-01.u4",         0x020000, 0xfa7bfa04, 3 | BRF_PRG | BRF_ESS }, // 14
	{ "e-302-33023-01.u5",         0x020000, 0x198285d4, 3 | BRF_PRG | BRF_ESS }, // 15
	{ "e-302-33001-01.u145",       0x020000, 0x612d4bf8, 5 | BRF_GRA },           // 16 Background Tiles
	{ "e-302-33002-01.u146",       0x020000, 0x77a725f6, 5 | BRF_GRA },           // 17
	{ "e-302-33003-01.u147",       0x020000, 0xd6aac372, 5 | BRF_GRA },           // 18
	{ "e-302-33004-01.u148",       0x020000, 0x5d19723e, 5 | BRF_GRA },           // 19
	{ "e-302-33005-01.u149",       0x020000, 0x29056791, 5 | BRF_GRA },           // 20
	{ "e-302-33006-01.u150",       0x020000, 0xcb73dd6a, 5 | BRF_GRA },           // 21
	{ "e-302-33015-01.u68",        0x020000, 0x9e88efb3, 7 | BRF_PRG | BRF_ESS }, // 22 XROM Data
	{ "e-302-33016-01.u69",        0x020000, 0xaa39fcb3, 7 | BRF_PRG | BRF_ESS }, // 23
	{ "e-302-33025-01.u8",         0x020000, 0x9f16e5b6, 4 | BRF_SND },           // 24 External DAC Samples
	{ "e-302-33026-01.u9",         0x020000, 0x0dc8f488, 4 | BRF_SND },           // 25
	{ "eeprom-indyheat.bin",       0x000100, 0x896f7257, 8 | BRF_PRG | BRF_ESS }, // 26 Default EEPROM
};

STD_ROM_PICK(indyheat)
STD_ROM_FN(indyheat)

static const LelandRomLoad indyheatRomLoad[] = {
	{  0, 0, 0, 0x000000, 0x000000, 0x020000 },
	{  0, 0, 0, 0x000000, 0x010000, 0x020000 },
	{  1, 0, 0, 0x000000, 0x030000, 0x020000 },
	{  2, 0, 0, 0x000000, 0x050000, 0x020000 },
	{  3, 0, 0, 0x000000, 0x070000, 0x020000 },
	{  4, 1, 0, 0x000000, 0x000000, 0x020000 },
	{  5, 1, 0, 0x000000, 0x020000, 0x020000 },
	{  5, 1, 0, 0x020000, 0x120000, 0x020000 },
	{  6, 1, 0, 0x000000, 0x040000, 0x020000 },
	{  6, 1, 0, 0x020000, 0x140000, 0x020000 },
	{  7, 1, 0, 0x000000, 0x060000, 0x020000 },
	{  8, 1, 0, 0x000000, 0x080000, 0x020000 },
	{  9, 1, 0, 0x000000, 0x0a0000, 0x020000 },
	{ 10, 1, 0, 0x000000, 0x0c0000, 0x020000 },
	{ 11, 1, 0, 0x000000, 0x0e0000, 0x020000 },
	{ 12, 2, 1, 0x000000, 0x020001, 0x020000 },
	{ 13, 2, 1, 0x000000, 0x020000, 0x020000 },
	{ 14, 2, 1, 0x000000, 0x060001, 0x020000 },
	{ 14, 2, 1, 0x000000, 0x0c0001, 0x020000 },
	{ 15, 2, 1, 0x000000, 0x060000, 0x020000 },
	{ 15, 2, 1, 0x000000, 0x0c0000, 0x020000 },
	{ 16, 4, 0, 0x000000, 0x000000, 0x020000 },
	{ 17, 4, 0, 0x000000, 0x020000, 0x020000 },
	{ 18, 4, 0, 0x000000, 0x040000, 0x020000 },
	{ 19, 4, 0, 0x000000, 0x060000, 0x020000 },
	{ 20, 4, 0, 0x000000, 0x080000, 0x020000 },
	{ 21, 4, 0, 0x000000, 0x0a0000, 0x020000 },
	{ 22, 6, 1, 0x000000, 0x000000, 0x020000 },
	{ 23, 6, 1, 0x000000, 0x000001, 0x020000 },
	{ 24, 3, 0, 0x000000, 0x000000, 0x020000 },
	{ 25, 3, 0, 0x000000, 0x020000, 0x020000 },
	{ 26, 7, 0, 0x000000, 0x000000, 0x000100 },
	{ -1, 0, 0, 0, 0, 0 }
};

static const LelandRegions indyheatRegions = { { 0x090000, 0x160000, 0x100000, 0x040000, 0x0c0000, 0x000000, 0x040000, 0x000100 }, 0x00 };

static INT32 indyheatInit()
{
	return DrvInit(&indyheat_cfg, indyheatRomLoad, &indyheatRegions);
}

struct BurnDriver BurnDrvIndyheat = {
	"indyheat", NULL, NULL, NULL, "1991",
	"Danny Sullivan's Indy Heat (rev 1)\0", NULL, "Leland Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 3, HARDWARE_MISC_POST90S, GBF_RACING, 0,
	NULL, indyheatRomInfo, indyheatRomName, NULL, NULL, NULL, NULL, IndyheatInputInfo, NULL,
	indyheatInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};

// Brute Force

static struct BurnRomInfo brutforcRomDesc[] = {
	{ "u64",                       0x020000, 0x008ae3b8, 1 | BRF_PRG | BRF_ESS }, //  0 Master Z80 Code
	{ "u65",                       0x020000, 0x6036e3fa, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "u66",                       0x020000, 0x7ebf0795, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "u67",                       0x020000, 0xe3cbf8b4, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "u151",                      0x020000, 0xbd3b677b, 2 | BRF_PRG | BRF_ESS }, //  4 Slave Z80 Code
	{ "u152",                      0x020000, 0x5f4434e7, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "u153",                      0x020000, 0x20f7df53, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "u154",                      0x020000, 0x69ce2329, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "u155",                      0x020000, 0x33d92e25, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "u156",                      0x020000, 0xde7eca8b, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "u157",                      0x020000, 0xe42b3dba, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "u158",                      0x020000, 0xa0aa3220, 2 | BRF_PRG | BRF_ESS }, // 11
	{ "u3",                        0x020000, 0x9984906c, 3 | BRF_PRG | BRF_ESS }, // 12 80186 Code
	{ "u6",                        0x020000, 0xc9c5a413, 3 | BRF_PRG | BRF_ESS }, // 13
	{ "u4",                        0x020000, 0xca8ab3a6, 3 | BRF_PRG | BRF_ESS }, // 14
	{ "u5",                        0x020000, 0xcbdb914b, 3 | BRF_PRG | BRF_ESS }, // 15
	{ "u145",                      0x040000, 0xc3d20d24, 5 | BRF_GRA },           // 16 Background Tiles
	{ "u146",                      0x040000, 0x43e9dd87, 5 | BRF_GRA },           // 17
	{ "u147",                      0x040000, 0xfb855ce8, 5 | BRF_GRA },           // 18
	{ "u148",                      0x040000, 0xe4b54eae, 5 | BRF_GRA },           // 19
	{ "u149",                      0x040000, 0xcf48401c, 5 | BRF_GRA },           // 20
	{ "u150",                      0x040000, 0xca9e1e33, 5 | BRF_GRA },           // 21
	{ "u68",                       0x020000, 0x77c8de62, 7 | BRF_PRG | BRF_ESS }, // 22 XROM Data
	{ "u69",                       0x020000, 0x113aa6d5, 7 | BRF_PRG | BRF_ESS }, // 23
	{ "u8",                        0x020000, 0x1e0ead72, 4 | BRF_SND },           // 24 External DAC Samples
	{ "u9",                        0x020000, 0x3195b305, 4 | BRF_SND },           // 25
	{ "u10",                       0x020000, 0x1dc5f375, 4 | BRF_SND },           // 26
	{ "u11",                       0x020000, 0x5ed4877f, 4 | BRF_SND },           // 27
	{ "eeprom-brutforc.bin",       0x000100, 0x508809af, 8 | BRF_PRG | BRF_ESS }, // 28 Default EEPROM
};

STD_ROM_PICK(brutforc)
STD_ROM_FN(brutforc)

static const LelandRomLoad brutforcRomLoad[] = {
	{  0, 0, 0, 0x000000, 0x000000, 0x020000 },
	{  0, 0, 0, 0x000000, 0x010000, 0x020000 },
	{  1, 0, 0, 0x000000, 0x030000, 0x020000 },
	{  2, 0, 0, 0x000000, 0x050000, 0x020000 },
	{  3, 0, 0, 0x000000, 0x070000, 0x020000 },
	{  4, 1, 0, 0x000000, 0x000000, 0x020000 },
	{  5, 1, 0, 0x000000, 0x020000, 0x020000 },
	{  6, 1, 0, 0x000000, 0x040000, 0x020000 },
	{  7, 1, 0, 0x000000, 0x060000, 0x020000 },
	{  8, 1, 0, 0x000000, 0x080000, 0x020000 },
	{  9, 1, 0, 0x000000, 0x0a0000, 0x020000 },
	{ 10, 1, 0, 0x000000, 0x0c0000, 0x020000 },
	{ 11, 1, 0, 0x000000, 0x0e0000, 0x020000 },
	{ 12, 2, 1, 0x000000, 0x020001, 0x020000 },
	{ 13, 2, 1, 0x000000, 0x020000, 0x020000 },
	{ 14, 2, 1, 0x000000, 0x060001, 0x020000 },
	{ 14, 2, 1, 0x000000, 0x0c0001, 0x020000 },
	{ 15, 2, 1, 0x000000, 0x060000, 0x020000 },
	{ 15, 2, 1, 0x000000, 0x0c0000, 0x020000 },
	{ 16, 4, 0, 0x000000, 0x000000, 0x040000 },
	{ 17, 4, 0, 0x000000, 0x040000, 0x040000 },
	{ 18, 4, 0, 0x000000, 0x080000, 0x040000 },
	{ 19, 4, 0, 0x000000, 0x0c0000, 0x040000 },
	{ 20, 4, 0, 0x000000, 0x100000, 0x040000 },
	{ 21, 4, 0, 0x000000, 0x140000, 0x040000 },
	{ 22, 6, 1, 0x000000, 0x000000, 0x020000 },
	{ 23, 6, 1, 0x000000, 0x000001, 0x020000 },
	{ 24, 3, 0, 0x000000, 0x000000, 0x020000 },
	{ 25, 3, 0, 0x000000, 0x020000, 0x020000 },
	{ 26, 3, 0, 0x000000, 0x040000, 0x020000 },
	{ 27, 3, 0, 0x000000, 0x060000, 0x020000 },
	{ 28, 7, 0, 0x000000, 0x000000, 0x000100 },
	{ -1, 0, 0, 0, 0, 0 }
};

static const LelandRegions brutforcRegions = { { 0x090000, 0x100000, 0x100000, 0x080000, 0x180000, 0x000000, 0x040000, 0x000100 }, 0x00 };

static INT32 brutforcInit()
{
	return DrvInit(&brutforc_cfg, brutforcRomLoad, &brutforcRegions);
}

struct BurnDriver BurnDrvBrutforc = {
	"brutforc", NULL, NULL, NULL, "1991",
	"Brute Force\0", NULL, "Leland Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 3, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, brutforcRomInfo, brutforcRomName, NULL, NULL, NULL, NULL, BrutforcInputInfo, NULL,
	brutforcInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};

// Asylum (prototype)

static struct BurnRomInfo asylumRomDesc[] = {
	{ "asy-m0.64",                 0x020000, 0xf5ca36fd, 1 | BRF_PRG | BRF_ESS }, //  0 Master Z80 Code
	{ "asy-m1.65",                 0x020000, 0x14d91d09, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "asy-m2.66",                 0x020000, 0xa34a6ef9, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "asy-m3.67",                 0x020000, 0x9db4c2b1, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "asy-sp0.151",               0x020000, 0x5ad5e3b0, 2 | BRF_PRG | BRF_ESS }, //  4 Slave Z80 Code
	{ "asy-sp2.152",               0x040000, 0x6d2997ec, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "asy-sp4.153",               0x040000, 0x7c61973c, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "asy-sp6.154",               0x040000, 0xf0a4f9d3, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "asy-sp8.155",               0x040000, 0x2ad0640e, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "asy-spa.156",               0x040000, 0x9d584fb4, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "asy-spc.157",               0x040000, 0x8485e48c, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "asy-spe.158",               0x020000, 0x49d19520, 2 | BRF_PRG | BRF_ESS }, // 11
	{ "asy-65.3",                  0x020000, 0x709bdc78, 3 | BRF_PRG | BRF_ESS }, // 12 80186 Code
	{ "asy-65.6",                  0x020000, 0xd019fb2e, 3 | BRF_PRG | BRF_ESS }, // 13
	{ "asy-65.4",                  0x020000, 0x1882c3b2, 3 | BRF_PRG | BRF_ESS }, // 14
	{ "asy-65.5",                  0x020000, 0x5814b307, 3 | BRF_PRG | BRF_ESS }, // 15
	{ "asy-chr0.145",              0x040000, 0x4dbcae49, 5 | BRF_GRA },           // 16 Background Tiles
	{ "asy-chr1.146",              0x040000, 0x34e7762d, 5 | BRF_GRA },           // 17
	{ "asy-chr2.147",              0x040000, 0xf9b0d375, 5 | BRF_GRA },           // 18
	{ "asy-chr3.148",              0x040000, 0x5efcae94, 5 | BRF_GRA },           // 19
	{ "asy-chr4.149",              0x040000, 0xdbc2b155, 5 | BRF_GRA },           // 20
	{ "asy-chr5.150",              0x040000, 0x9675e44f, 5 | BRF_GRA },           // 21
	{ "asy-m4.68",                 0x020000, 0x77c8de62, 7 | BRF_PRG | BRF_ESS }, // 22 XROM Data
	{ "asy-m5.69",                 0x020000, 0xbfc50d6c, 7 | BRF_PRG | BRF_ESS }, // 23
	{ "asy-65.8",                  0x020000, 0x624ad02f, 4 | BRF_SND },           // 24 External DAC Samples
	{ "asy-65.9",                  0x020000, 0xc92ff376, 4 | BRF_SND },           // 25
	{ "asy-65.10",                 0x020000, 0x744dbf25, 4 | BRF_SND },           // 26
	{ "asy-65.11",                 0x020000, 0x4b185d22, 4 | BRF_SND },           // 27
	{ "eeprom-asylum.bin",         0x000100, 0x9a9a361b, 8 | BRF_PRG | BRF_ESS }, // 28 Default EEPROM
};

STD_ROM_PICK(asylum)
STD_ROM_FN(asylum)

static const LelandRomLoad asylumRomLoad[] = {
	{  0, 0, 0, 0x000000, 0x000000, 0x020000 },
	{  0, 0, 0, 0x000000, 0x010000, 0x020000 },
	{  1, 0, 0, 0x000000, 0x030000, 0x020000 },
	{  2, 0, 0, 0x000000, 0x050000, 0x020000 },
	{  3, 0, 0, 0x000000, 0x070000, 0x020000 },
	{  4, 1, 0, 0x000000, 0x000000, 0x020000 },
	{  5, 1, 0, 0x000000, 0x020000, 0x020000 },
	{  5, 1, 0, 0x020000, 0x120000, 0x020000 },
	{  6, 1, 0, 0x000000, 0x040000, 0x020000 },
	{  6, 1, 0, 0x020000, 0x140000, 0x020000 },
	{  7, 1, 0, 0x000000, 0x060000, 0x020000 },
	{  7, 1, 0, 0x020000, 0x160000, 0x020000 },
	{  8, 1, 0, 0x000000, 0x080000, 0x020000 },
	{  8, 1, 0, 0x020000, 0x180000, 0x020000 },
	{  9, 1, 0, 0x000000, 0x0a0000, 0x020000 },
	{  9, 1, 0, 0x020000, 0x1a0000, 0x020000 },
	{ 10, 1, 0, 0x000000, 0x0c0000, 0x020000 },
	{ 10, 1, 0, 0x020000, 0x1c0000, 0x020000 },
	{ 11, 1, 0, 0x000000, 0x0e0000, 0x020000 },
	{ 12, 2, 1, 0x000000, 0x020001, 0x020000 },
	{ 13, 2, 1, 0x000000, 0x020000, 0x020000 },
	{ 14, 2, 1, 0x000000, 0x060001, 0x020000 },
	{ 14, 2, 1, 0x000000, 0x0c0001, 0x020000 },
	{ 15, 2, 1, 0x000000, 0x060000, 0x020000 },
	{ 15, 2, 1, 0x000000, 0x0c0000, 0x020000 },
	{ 16, 4, 0, 0x000000, 0x000000, 0x040000 },
	{ 17, 4, 0, 0x000000, 0x040000, 0x040000 },
	{ 18, 4, 0, 0x000000, 0x080000, 0x040000 },
	{ 19, 4, 0, 0x000000, 0x0c0000, 0x040000 },
	{ 20, 4, 0, 0x000000, 0x100000, 0x040000 },
	{ 21, 4, 0, 0x000000, 0x140000, 0x040000 },
	{ 22, 6, 1, 0x000000, 0x000000, 0x020000 },
	{ 23, 6, 1, 0x000000, 0x000001, 0x020000 },
	{ 24, 3, 0, 0x000000, 0x000000, 0x020000 },
	{ 25, 3, 0, 0x000000, 0x020000, 0x020000 },
	{ 26, 3, 0, 0x000000, 0x040000, 0x020000 },
	{ 27, 3, 0, 0x000000, 0x060000, 0x020000 },
	{ 28, 7, 0, 0x000000, 0x000000, 0x000100 },
	{ -1, 0, 0, 0, 0, 0 }
};

static const LelandRegions asylumRegions = { { 0x090000, 0x1e0000, 0x100000, 0x080000, 0x180000, 0x000000, 0x040000, 0x000100 }, 0x00 };

static INT32 asylumInit()
{
	return DrvInit(&asylum_cfg, asylumRomLoad, &asylumRegions);
}

struct BurnDriver BurnDrvAsylum = {
	"asylum", NULL, NULL, NULL, "1991",
	"Asylum (prototype)\0", NULL, "Leland Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_PROTOTYPE, 3, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, asylumRomInfo, asylumRomName, NULL, NULL, NULL, NULL, BrutforcInputInfo, NULL,
	asylumInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	240, 320, 3, 4
};
