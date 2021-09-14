// license:BSD-3-Clause
// copyright-holders:R. Belmont, superctr
/*
    c352.c - Namco C352 custom PCM chip emulation
    v2.0
    By R. Belmont
    Rewritten and improved by superctr
    Additional code by cync and the hoot development team

    Thanks to Cap of VivaNonno for info and The_Author for preliminary reverse-engineering

    Chip specs:
    32 voices
    Supports 8-bit linear and 8-bit muLaw samples
    Output: digital, 16 bit, 4 channels
    Output sample rate is the input clock / (288 * 2).
 */

#include "burnint.h"
#include "c352.h"
#include <stddef.h>

enum
{
	C352_FLG_BUSY       = 0x8000,   // channel is busy
	C352_FLG_KEYON      = 0x4000,   // Keyon
	C352_FLG_KEYOFF     = 0x2000,   // Keyoff
	C352_FLG_LOOPTRG    = 0x1000,   // Loop Trigger
	C352_FLG_LOOPHIST   = 0x0800,   // Loop History
	C352_FLG_FM         = 0x0400,   // Frequency Modulation
	C352_FLG_PHASERL    = 0x0200,   // Rear Left invert phase 180 degrees
	C352_FLG_PHASEFL    = 0x0100,   // Front Left invert phase 180 degrees
	C352_FLG_PHASEFR    = 0x0080,   // invert phase 180 degrees (e.g. flip sign of sample)
	C352_FLG_LDIR       = 0x0040,   // loop direction
	C352_FLG_LINK       = 0x0020,   // "long-format" sample (can't loop, not sure what else it means)
	C352_FLG_NOISE      = 0x0010,   // play noise instead of sample
	C352_FLG_MULAW      = 0x0008,   // sample is mulaw instead of linear 8-bit PCM
	C352_FLG_FILTER     = 0x0004,   // don't apply filter
	C352_FLG_REVLOOP    = 0x0003,   // loop backwards
	C352_FLG_LOOP       = 0x0002,   // loop forward
	C352_FLG_REVERSE    = 0x0001    // play sample backwards
};

struct c352_voice_t
{
	UINT32 pos;
	UINT32 counter;

	INT16 sample;
	INT16 last_sample;

	UINT16 vol_f;
	UINT16 vol_r;
	UINT8 curr_vol[4];

	UINT16 freq;
	UINT16 flags;

	UINT16 wave_bank;
	UINT16 wave_start;
	UINT16 wave_end;
	UINT16 wave_loop;
};

static INT32 m_sample_rate;

static c352_voice_t m_c352_v[32];

static INT16 m_mulawtab[256];

static UINT16 m_random;
static UINT16 m_control; // control flags, purpose unknown.

static INT8 *m_rom;
static INT32 m_romsize;

#include "stream.h"
static Stream stream;

void c352_set_sync(INT32 (*pCPUCyclesCB)(), INT32 nCPUMhz)
{
	stream.set_buffered(pCPUCyclesCB, nCPUMhz);
}

static INT8 read_byte(INT32 pos)
{
	if (pos >= m_romsize) {
		return 0;
	}

	return m_rom[pos];
}

static void fetch_sample(c352_voice_t& v)
{
	v.last_sample = v.sample;

	if (v.flags & C352_FLG_NOISE)
	{
		m_random = (m_random >> 1) ^ ((-(m_random & 1)) & 0xfff6);
		v.sample = m_random;
	}
	else
	{
		INT8 s = (INT8)read_byte(v.pos);

		if (v.flags & C352_FLG_MULAW)
			v.sample = m_mulawtab[s & 0xff];
		else
			v.sample = s << 8;

		UINT16 pos = v.pos & 0xffff;

		if ((v.flags & C352_FLG_LOOP) && v.flags & C352_FLG_REVERSE)
		{
			// backwards>forwards
			if ((v.flags & C352_FLG_LDIR) && pos == v.wave_loop)
				v.flags &= ~C352_FLG_LDIR;
			// forwards>backwards
			else if (!(v.flags & C352_FLG_LDIR) && pos == v.wave_end)
				v.flags |= C352_FLG_LDIR;

			v.pos += (v.flags & C352_FLG_LDIR) ? -1 : 1;
		}
		else if (pos == v.wave_end)
		{
			if ((v.flags & C352_FLG_LINK) && (v.flags & C352_FLG_LOOP))
			{
				v.pos = (v.wave_start << 16) | v.wave_loop;
				v.flags |= C352_FLG_LOOPHIST;
			}
			else if (v.flags & C352_FLG_LOOP)
			{
				v.pos = (v.pos & 0xff0000) | v.wave_loop;
				v.flags |= C352_FLG_LOOPHIST;
			}
			else
			{
				v.flags |= C352_FLG_KEYOFF;
				v.flags &= ~C352_FLG_BUSY;
				v.sample = 0;
			}
		}
		else
		{
			v.pos += (v.flags & C352_FLG_REVERSE) ? -1 : 1;
		}
	}
}

static void ramp_volume(c352_voice_t &v, int ch, UINT8 val)
{
	INT16 vol_delta = v.curr_vol[ch] - val;
	if (vol_delta != 0)
		v.curr_vol[ch] += (vol_delta > 0) ? -1 : 1;
}

static void c352_update_INT(INT16 **streams, INT32 samples)
{
	INT16 *buffer_fl = streams[0];
	INT16 *buffer_fr = streams[1];
	//INT16 *buffer_rl = outputs[2];
	//INT16 *buffer_rr = outputs[3];

	for (int i = 0; i < samples; i++)
	{
		long out[4] = { 0, 0, 0, 0 };

		for (int j = 0; j < 32; j++)
		{
			c352_voice_t &v = m_c352_v[j];
			INT16 s = 0;

			if (v.flags & C352_FLG_BUSY)
			{
				INT32 next_counter = v.counter + v.freq;

				if (next_counter & 0x10000)
				{
					fetch_sample(v);
				}

				if ((next_counter ^ v.counter) & 0x18000)
				{
					ramp_volume(v, 0, v.vol_f >> 8);
					ramp_volume(v, 1, v.vol_f & 0xff);
					ramp_volume(v, 2, v.vol_r >> 8);
					ramp_volume(v, 3, v.vol_r & 0xff);
				}

				v.counter = next_counter & 0xffff;

				s = v.sample;

				// Interpolate samples
				if ((v.flags & C352_FLG_FILTER) == 0)
					s = v.last_sample + (v.counter * (v.sample - v.last_sample) >> 16);
			}

			// Left
			out[0] += (((v.flags & C352_FLG_PHASEFL) ? -s : s) * v.curr_vol[0]) >> 8;
			out[2] += (((v.flags & C352_FLG_PHASERL) ? -s : s) * v.curr_vol[2]) >> 8;

			// Right
			out[1] += (((v.flags & C352_FLG_PHASEFR) ? -s : s) * v.curr_vol[1]) >> 8;
			out[3] += (((v.flags & C352_FLG_PHASEFR) ? -s : s) * v.curr_vol[3]) >> 8;
		}

		*buffer_fl++ = (INT16)(out[0] >> 3);
		*buffer_fr++ = (INT16)(out[1] >> 3);
		//*buffer_rl++ = (INT16)(out[2] >> 3);
		//*buffer_rr++ = (INT16)(out[3] >> 3);
	}
}

void c352_update(INT16 *output, INT32 samples_len)
{
	if (samples_len != nBurnSoundLen) {
		bprintf(0, _T("c352_update(): once per frame, please!\n"));
		return;
	}

	stream.render(output, samples_len);
}

UINT16 c352_read(unsigned long address)
{
	stream.update();

	const int reg_map[8] =
	{
		offsetof(c352_voice_t, vol_f) / sizeof(UINT16),
		offsetof(c352_voice_t, vol_r) / sizeof(UINT16),
		offsetof(c352_voice_t, freq) / sizeof(UINT16),
		offsetof(c352_voice_t, flags) / sizeof(UINT16),
		offsetof(c352_voice_t, wave_bank) / sizeof(UINT16),
		offsetof(c352_voice_t, wave_start) / sizeof(UINT16),
		offsetof(c352_voice_t, wave_end) / sizeof(UINT16),
		offsetof(c352_voice_t, wave_loop) / sizeof(UINT16),
	};

	if (address < 0x100)
		return *((UINT16*)&m_c352_v[address / 8] + reg_map[address % 8]);
	else if (address == 0x200)
		return m_control;
	else
		return 0;

	return 0;
}

void c352_write(unsigned long address, unsigned short val)
{
	stream.update();

	const int reg_map[8] =
	{
		offsetof(c352_voice_t, vol_f) / sizeof(UINT16),
		offsetof(c352_voice_t, vol_r) / sizeof(UINT16),
		offsetof(c352_voice_t, freq) / sizeof(UINT16),
		offsetof(c352_voice_t, flags) / sizeof(UINT16),
		offsetof(c352_voice_t, wave_bank) / sizeof(UINT16),
		offsetof(c352_voice_t, wave_start) / sizeof(UINT16),
		offsetof(c352_voice_t, wave_end) / sizeof(UINT16),
		offsetof(c352_voice_t, wave_loop) / sizeof(UINT16),
	};

	if (address < 0x100)
	{
		*((UINT16*)&m_c352_v[address / 8] + reg_map[address % 8]) = val;
	}
	else if (address == 0x200)
	{
		m_control = val;
		//logerror("C352 control register write: %04x\n",val);
	}
	else if (address == 0x202) // execute keyons/keyoffs
	{
		for (int i = 0; i < 32; i++)
		{
			if (m_c352_v[i].flags & C352_FLG_KEYON)
			{
				m_c352_v[i].pos = (m_c352_v[i].wave_bank << 16) | m_c352_v[i].wave_start;

				m_c352_v[i].sample = 0;
				m_c352_v[i].last_sample = 0;
				m_c352_v[i].counter = 0xffff;

				m_c352_v[i].flags |= C352_FLG_BUSY;
				m_c352_v[i].flags &= ~(C352_FLG_KEYON | C352_FLG_LOOPHIST);

				m_c352_v[i].curr_vol[0] = m_c352_v[i].curr_vol[1] = 0;
				m_c352_v[i].curr_vol[2] = m_c352_v[i].curr_vol[3] = 0;

			}
			if (m_c352_v[i].flags & C352_FLG_KEYOFF)
			{
				m_c352_v[i].flags &= ~(C352_FLG_BUSY | C352_FLG_KEYOFF);
				m_c352_v[i].counter = 0xffff;
			}
		}
	}
}

void c352_init(INT32 clock, INT32 divider, UINT8 *c352_rom, INT32 c352_romsize, INT32 AddToStream)
{
	m_sample_rate = clock / divider; //48384000/2/288 = 84000 (nb1) -dink
	m_rom = (INT8*)c352_rom;
	m_romsize = c352_romsize;

	stream.init(m_sample_rate, nBurnSoundRate, 2, AddToStream, c352_update_INT);

	// generate mulaw table (Output similar to namco's VC emulator)
	int j = 0;
	for (int i = 0; i < 128; i++)
	{
		m_mulawtab[i] = j << 5;
		if (i < 16)
			j += 1;
		else if (i < 24)
			j += 2;
		else if (i < 48)
			j += 4;
		else if (i < 100)
			j += 8;
		else
			j += 16;
	}
	for (int i = 0; i < 128; i++)
		m_mulawtab[i + 128] = (~m_mulawtab[i]) & 0xffe0;
}

void c352_exit()
{
	stream.exit();
}

void c352_scan(INT32 nAction, INT32 *)
{
	SCAN_VAR(m_random);
	SCAN_VAR(m_control);
	SCAN_VAR(m_c352_v);

	if (nAction & ACB_WRITE && ~nAction & ACB_RUNAHEAD) {
		// nothing
	}
}

void c352_reset()
{
	// clear all channels states
	memset(m_c352_v, 0, sizeof(c352_voice_t) * 32);

	// init noise generator
	m_random = 0x1234;
	m_control = 0;
}

