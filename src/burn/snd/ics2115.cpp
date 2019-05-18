// license:???
// copyright-holders:Alex Marshall,nimitz,austere
//ICS2115 by Raiden II team (c) 2010
//members: austere, nimitz, Alex Marshal
//
//Original driver by O. Galibert, ElSemi
//
//Use tab size = 4 for your viewing pleasure.

//Ported from MAME git, 13/07/2015

// jan_klaasen was here: added interpolators for rendering at any samplerate (44.1/48kHz tested)
//						 added panning
//						 many changes -- fixed bugs and brought in line with ics2115 datasheet
//						 and AMD interwave patent details (where applicable)


#include "burnint.h"
//#include <cmath>

#include "timer.h"


// if defined, comform to hardware limits: wavetable increase at 33.075, use 1024-step linear interpolator
// otherwise, wavetable increase at final samplerate, allow the use of a more precise cubic interpolator
// #define INTERPOLATE_AS_HARDWARE

// if defined, ramp down the volume when a sample stops and exclude the voice from processing
// N.B. the hardware doesn't do this, which may result in a DC offset
#define RAMP_DOWN
#define RAMP_BITS 6	// ~2ms ay 33.075kHz

// if defined, honour each voice's pan register (N.B. PGM has a monophonic speaker connection only)
// #define DO_PANNING

// if defined, use both lo- and hi-bytes for volume envelope increase -- the Cave games get this wrong,
// but ddp2 at least seems to (sort-of) expect the volume envelopes to work anyway, despite the error
#define CAVE_HACK


static UINT8* m_rom = NULL;			// ics2115 rom
static INT32 m_rom_len;
static void (*m_irq_cb)(INT32) = NULL;// cpu irq callback

struct ics2115_voice {
	struct {
		INT32 left;
		UINT32 acc, start, end;
		UINT16 fc;
		UINT8 ctl, saddr;
		UINT8 vmode;					// reserved, write 0
	} osc;

	struct {
		INT32 left;
		UINT32 add;
		UINT32 start, end;
		UINT32 acc;
		UINT8 incr, inc_lo, inc_hi;	// separate hi- and lo-byte for cave hack
		UINT8 pan, mode;
	} vol;

	union {
		struct {
			UINT8 ulaw       : 1;
			UINT8 stop       : 1;   // stops wave + vol envelope
			UINT8 eightbit   : 1;
			UINT8 loop       : 1;
			UINT8 loop_bidir : 1;
			UINT8 irq        : 1;   // enable IRQ generation
			UINT8 invert     : 1;
			UINT8 irq_pending: 1;   // writable on ultrasound/interwave; writing 1 triggers IRQ, 0 clears
		} bitflags;
		UINT8 value;
	} osc_conf;

	union {
		struct {
			UINT8 done       : 1;   // indicates envelope status (hardware modifies this)
			UINT8 stop       : 1;   // stops the envelope
			UINT8 rollover   : 1;
			UINT8 loop       : 1;
			UINT8 loop_bidir : 1;
			UINT8 irq        : 1;   // enable IRQ generation
			UINT8 invert     : 1;
			UINT8 irq_pending: 1;   // writable on ultrasound/interwave; writing 1 triggers IRQ, 0 clears
		} bitflags;
		UINT8 value;
	} vol_ctrl;

	// variables used by the interpolator

	UINT32 prev_addr;
	INT32 int_fc;
	INT32 int_buf[4];

	UINT8 ramp;						// see definition of RAMP_DOWN above

	inline bool process_sample();

	bool update_volume_envelope();
	bool update_oscillator();
	void update_ramp();
};

static const UINT16 revision = 0x1;

static INT16 m_ulaw[256];
static UINT16 m_volume[4096];
#if defined DO_PANNING
static UINT16 m_panlaw[256];
#endif
static const INT32 volume_bits = 15;

static ics2115_voice m_voice[32];
struct timer_struct {
	UINT8 scale, preset;
	UINT64 period;  /* in fba timer ticks */
};
static timer_struct m_timer[2];

static UINT32 m_sample_rate;
static INT32 m_chip_volume;

static UINT8 m_active_osc;
static UINT8 m_osc_select;
static UINT8 m_reg_select;
static UINT8 m_timer_irq_enabled, m_timer_irq_pending;

static bool m_irq_on;
static UINT8 m_vmode;

// internal register helper functions
void ics2115_recalc_timer(INT32 timer);
void ics2115_recalc_irq();

INT32 ics2115_timer_cb(INT32 n, INT32 c);

static INT32 stream_pos;
static INT32 output_sample_rate;

static UINT32 sample_size;
static UINT32 sample_count;
static INT32* buffer;


static INT32 (*get_sample)(ics2115_voice& voice);


#if 0
static INT32 get_stream_pos()
{
	return (INT64)(BurnTimerCPUTotalCycles()) * nBurnSoundRate / BurnTimerCPUClockspeed;
}
#endif

static void set_internal_sample_rate()
{
	m_sample_rate = (m_active_osc > 24) ? 33075 : 44100;
	sample_size = (UINT32)(((UINT64)(m_sample_rate) << 32) / output_sample_rate);
}

void ics_2115_set_volume(double volume)
{
#if defined DO_PANNING

	// adjust by +3dB to account for center pan value when panning is enabled

#endif
	
	m_chip_volume = (INT32)((double)(1 << 14) / volume);
}

void ics2115_init(void (*cpu_irq_cb)(INT32), UINT8 *sample_rom, INT32 sample_rom_size)
{
	DebugSnd_ICS2115Initted = 1;

	m_irq_cb = cpu_irq_cb;
	m_rom = sample_rom;
	m_rom_len = sample_rom_size;

	// compute volume table
	for (INT32 i = 0; i < 4096; i++)
		m_volume[i] = ((0x100 | (i & 0xff)) << (volume_bits - 9)) >> (15 - (i >> 8));

#if defined DO_PANNING

	// compute pan law table
	for (INT32 i = 1; i < 256; i++)
		m_panlaw[i] = (UINT16)(log2(1.0L / ((double)(i) / 255.0) * 128));

	m_panlaw[0] = 4095;

#endif

	ics_2115_set_volume(1.0L);

	// u-Law table as per MIL-STD-188-113
	UINT16 lut[8];
	UINT16 lut_initial = 33 << 2;   // shift up 2-bits for 16-bit range.
	for (INT32 i = 0; i < 8; i++)
		lut[i] = (lut_initial << i) - lut_initial;
	for (INT32 i = 0; i < 256; i++)
	{
		UINT8 exponent = (~i >> 4) & 0x07;
		UINT8 mantissa = ~i & 0x0f;
		INT16 value = lut[exponent] + (mantissa << (exponent + 3));
		m_ulaw[i] = (i & 0x80) ? -value : value;
	}

	buffer = NULL;

	output_sample_rate = nBurnSoundRate;
	if (output_sample_rate == 0)
		output_sample_rate = 44100;
	else
		buffer = (INT32*)(BurnMalloc(output_sample_rate /*/ 50*/ * 2 * sizeof(INT32)));

	BurnTimerInit(&ics2115_timer_cb, NULL);

	sample_count = stream_pos = 0;

#if defined INTERPOLATE_AS_HARDWARE

	static INT32 get_sample_hardware(ics2115_voice& voice);

	get_sample = get_sample_hardware;

#endif

}

void ics2115_exit()
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_ICS2115Initted) bprintf(PRINT_ERROR, _T("ics2115_exit called without init\n"));
#endif

	if (!DebugSnd_ICS2115Initted) return;

	m_rom = NULL;
	m_rom_len = 0;
	m_irq_cb = NULL;

	BurnFree(buffer);

	DebugSnd_ICS2115Initted = 0;
}

void ics2115_reset()
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_ICS2115Initted) bprintf(PRINT_ERROR, _T("ics2115_reset called without init\n"));
#endif

	m_timer_irq_enabled = m_timer_irq_pending = 0;

	//possible re-suss
	m_active_osc = 31;
	m_osc_select = 0;
	m_reg_select = 0;
	m_vmode = 0;
	m_irq_on = false;

	memset(m_voice, 0, sizeof(m_voice));

	for(INT32 i = 0; i < 2; i++)
	{
		m_timer[i].period = 0;
		m_timer[i].scale = 0;
		m_timer[i].preset = 0;
	}

	for(INT32 i = 0; i < 32; i++)
	{
		m_voice[i].osc_conf.value = 2;
		m_voice[i].osc.fc = 0;
		m_voice[i].osc.acc = 0;
		m_voice[i].osc.start = 0;
		m_voice[i].osc.end = 0;
		m_voice[i].osc.ctl = 0;
		m_voice[i].osc.saddr = 0;
		m_voice[i].vol.acc = 0;
		m_voice[i].vol.incr = m_voice[i].vol.inc_lo = m_voice[i].vol.inc_hi = 0;
		m_voice[i].vol.start = 0;
		m_voice[i].vol.end = 0;
		m_voice[i].vol.pan = 0x7F;
		m_voice[i].vol_ctrl.value = 1;
		m_voice[i].vol.mode = 0;
		m_voice[i].ramp = 0;
	}

	set_internal_sample_rate();

	BurnTimerReset();
}

bool ics2115_voice::update_volume_envelope()
{
	if (vol_ctrl.bitflags.done || vol_ctrl.bitflags.stop)
		return false;

	if (vol.add == 0)
		return false;

	if (vol_ctrl.bitflags.invert)
	{
		vol.acc -= vol.add;

		vol.left = vol.acc - vol.start;
	}
	else
	{
		vol.acc += vol.add;

		vol.left = vol.end - vol.acc;
	}

	if (vol.left > 0)
		return false;

	if (vol_ctrl.bitflags.irq)
		vol_ctrl.bitflags.irq_pending = true;

	if (osc_conf.bitflags.eightbit)
		return vol_ctrl.bitflags.irq_pending;

	if (vol_ctrl.bitflags.loop)
	{
		if (osc_conf.bitflags.loop_bidir)
			vol_ctrl.bitflags.invert = !vol_ctrl.bitflags.invert;

		if (osc_conf.bitflags.invert)
			vol.acc = vol.end + vol.left;
		else
			vol.acc = vol.start - vol.left;
	}
	else
		vol_ctrl.bitflags.done = true;

	return vol_ctrl.bitflags.irq_pending;
}

bool ics2115_voice::update_oscillator()
{
	if (osc_conf.bitflags.stop || osc.ctl != 0)
		return false;

#if defined INTERPOLATE_AS_HARDWARE

	// run wavetable addresssing at hardware samplerate

	if (osc_conf.bitflags.invert)
	{
		osc.acc -= osc.fc << 2;

		osc.left = osc.acc - osc.start;
	}
	else
	{
		osc.acc += osc.fc << 2;

		osc.left = osc.end - osc.acc;
	}

	if (osc.left > 0)
		return false;

#else

	// run wavetable addresssing at full samplerate

	if (osc_conf.bitflags.invert)
	{
		osc.acc -= int_fc;

		osc.left = osc.acc - osc.start;
	}
	else
	{
		osc.acc += int_fc;

		osc.left = osc.end - osc.acc;
	}

	if (osc.left > 0)
		return false;

#endif

	/* if (vol_ctrl.bitflags.rollover && (osc.left + (osc.fc << 2) >= 0))
		return false; */

	if (osc_conf.bitflags.irq)
		osc_conf.bitflags.irq_pending = true;

	if (osc_conf.bitflags.loop)
	{
		if (osc_conf.bitflags.loop_bidir)
			osc_conf.bitflags.invert = !osc_conf.bitflags.invert;

		if (osc_conf.bitflags.invert)
			osc.acc = osc.end + osc.left;
		else
			osc.acc = osc.start - osc.left;
	}
	else
		osc_conf.bitflags.stop = vol_ctrl.bitflags.done = true;

	return osc_conf.bitflags.irq_pending;
}

static inline INT32 read_wavetable(ics2115_voice& voice, const UINT32 curr_addr)
{
	if (voice.osc_conf.bitflags.ulaw || voice.osc_conf.bitflags.eightbit)
	{
		if (voice.osc_conf.bitflags.ulaw)
			return m_ulaw[m_rom[curr_addr]];

		return ((INT8)(m_rom[curr_addr]) << 8) | ((m_rom[curr_addr] & 0x7F) << 1);
	}

	return ((INT8)(m_rom[curr_addr + 1]) << 8) | m_rom[curr_addr + 0];
}

#if defined INTERPOLATE_AS_HARDWARE

// interpolate sample the same way the hardware does
static INT32 get_sample_hardware(ics2115_voice& voice)
{
	UINT32 curr_addr = ((voice.osc.saddr << 20) & 0x00FFFFFF) | (voice.osc.acc >> 12);

	INT32 sample_1 = read_wavetable(voice, curr_addr + 0);
	INT32 sample_2 = read_wavetable(voice, curr_addr + ((voice.osc_conf.bitflags.ulaw || voice.osc_conf.bitflags.eightbit) ? 1 : 2));

	INT32 diff = sample_2 - sample_1;
	UINT16 fract = ((voice.osc_conf.bitflags.invert ? ~voice.osc.acc : voice.osc.acc) & 0x0FFF) >> 2;

	return sample_1 + ((diff * fract) >> 10);
}

#else 

static inline void update_int_buf(ics2115_voice& voice)
{
	UINT32 curr_addr = ((voice.osc.saddr << 20) & 0x00FFFFFF) | (voice.osc.acc >> 12);

	if (curr_addr == voice.prev_addr)
		return;

	voice.int_buf[0] = voice.int_buf[1];
	voice.int_buf[1] = voice.int_buf[2];
	voice.int_buf[2] = voice.int_buf[3];
		
	voice.prev_addr = curr_addr;

	voice.int_buf[3] = read_wavetable(voice, curr_addr);
}

// interpolate sample at playback rate using linear interpolator
static INT32 get_sample_linear(ics2115_voice& voice)
{
	update_int_buf(voice);

	INT32 diff = voice.int_buf[3] - voice.int_buf[2];
	UINT16 fract = (voice.osc_conf.bitflags.invert ? ~voice.osc.acc : voice.osc.acc) & 0x0FFF;

	return voice.int_buf[2] + ((diff * fract) >> 12);
}

// interpolate sample at playback rate using a cubic interpolator
static INT32 get_sample_cubic(ics2115_voice& voice)
{
	update_int_buf(voice);

	const INT32 fract = (voice.osc_conf.bitflags.invert ? ~voice.osc.acc : voice.osc.acc) & 0x0FFF;

	return INTERPOLATE4PS_16BIT(fract, voice.int_buf[0], voice.int_buf[1], voice.int_buf[2], voice.int_buf[3]);
}

#endif

#if defined RAMP_DOWN

inline bool ics2115_voice::process_sample()
{
	return !osc.ctl && ramp;
}

inline void ics2115_voice::update_ramp()
{
	if (ramp && (osc_conf.bitflags.stop || osc.ctl))
	{
		ramp--;

		if (ramp == 0)
			memset(int_buf, 0, sizeof(int_buf));
	}
}

#else

inline bool ics2115_voice::process_sample()
{
#if 1

	// exclude the voice only if it is disabled

	return  !osc.ctl;

#else 
	
	// exclude the voice if the wavetable address isn't incrementing 

	return !(osc.ctl || osc_conf.bitflags.stop);

#endif
}

inline void ics2115_voice::update_ramp()
{
}

#endif


static bool ics2115_fill_output(ics2115_voice& voice, INT32* outputs, INT32 samples)
{
	bool irq_invalid = false;

#if !defined INTERPOLATE_AS_HARDWARE

	UINT32 count = sample_count;

#endif

	if (outputs == 0)
	{
		// no need to compute the output, just update state

		for (INT32 i = 0; i < samples; i++)
		{
			if (!voice.process_sample())
				continue;

			irq_invalid |= voice.update_volume_envelope();
			irq_invalid |= voice.update_oscillator();
		}

		return irq_invalid;
	}

	for (INT32 i = 0; i < samples; i++)
	{
		if (voice.process_sample())
		{
			INT32 volacc = (voice.vol.acc >> 14) & 0x00000FFF;

#if defined DO_PANNING

#if defined RAMP_DOWN

			INT32 vleft  = volacc - m_panlaw[255 - voice.vol.pan]; vleft  = vleft  > 0 ? (m_volume[vleft]  * voice.ramp >> RAMP_BITS) : 0;
			INT32 vright = volacc - m_panlaw[      voice.vol.pan]; vright = vright > 0 ? (m_volume[vright] * voice.ramp >> RAMP_BITS) : 0;

#else

			INT32 vleft  = volacc - m_panlaw[255 - voice.vol.pan]; vleft  = vleft  > 0 ? m_volume[vleft]  : 0;
			INT32 vright = volacc - m_panlaw[      voice.vol.pan]; vright = vright > 0 ? m_volume[vright] : 0;

#endif

			if (vleft || vright)
			{
				INT32 sample = get_sample(voice);

				outputs[0] += ((sample * vleft)  >> (5 + volume_bits - 16));
				outputs[1] += ((sample * vright) >> (5 + volume_bits - 16));
			}

			outputs += 2;

#else

#if defined RAMP_DOWN

			UINT16 volume = ((UINT32)(m_volume[volacc]) * voice.ramp) >> RAMP_BITS;

#else

			UINT16 volume = m_volume[volacc];

#endif
			if (volume)
				*outputs += (get_sample(voice) * volume) >> (5 + volume_bits - 16);

			outputs++;
#endif

		}

#if !defined INTERPOLATE_AS_HARDWARE

		count += sample_size;
		if (count <= sample_size)

#endif

		{
			voice.update_ramp();

			if (voice.process_sample())
				irq_invalid |= voice.update_volume_envelope();
		}

		irq_invalid |= voice.update_oscillator();
	}

	return irq_invalid;
}

static void ics2115_render(INT16* outputs, INT32 samples)
{
#if defined DO_PANNING

	if (buffer)
		memset(buffer, 0, samples * sizeof(INT32) * 2);

#else

	if (buffer)
		memset(buffer, 0, samples * sizeof(INT32));

#endif

#if !defined INTERPOLATE_AS_HARDWARE

	get_sample = (nInterpolation < 3) ? get_sample_linear : get_sample_cubic;

#endif

	bool irq_invalid = false;

	for (INT32 osc = 0; osc <= m_active_osc; osc++)
		irq_invalid |= ics2115_fill_output(m_voice[osc], buffer, samples);

	if (nBurnSoundRate)
	{

#if defined DO_PANNING

		for (INT32 i = (samples - 1) << 1; i >= 0; i -= 2)
		{
			outputs[i    ] = BURN_SND_CLIP(buffer[i    ] / m_chip_volume);
			outputs[i + 1] = BURN_SND_CLIP(buffer[i + 1] / m_chip_volume);
		}

#else

		for (INT32 i = samples - 1; i >= 0; i--)
			outputs[i << 1] = outputs[(i << 1) + 1] = BURN_SND_CLIP(buffer[i] / m_chip_volume);

#endif

	}

	if (irq_invalid)
		ics2115_recalc_irq();

	sample_count += sample_count * samples;
}

#if defined INTERPOLATE_AS_HARDWARE

#include <exception>

void ics2115_update(INT32 segment_length)
{

	// not implemented!

	throw std::exception("resampling for rendering at hardware rate not implemented!");
}

#else

void ics2115_update(INT32 segment_length)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_ICS2115Initted) bprintf(PRINT_ERROR, _T("ics2115_update called without init\n"));
#endif

	if (pBurnSoundOut == NULL)
		return;

	if (segment_length >= nBurnSoundLen)
		segment_length = nBurnSoundLen;

	if (segment_length <= stream_pos)
		return;

	// bprintf(0, _T("    ICS2115 rendering %03i - %03i (%03i)\n"), stream_pos, segment_length, nBurnSoundLen);
	ics2115_render(pBurnSoundOut + stream_pos * 2, segment_length - stream_pos);

	stream_pos = segment_length;
	if (stream_pos >= nBurnSoundLen)
		stream_pos -= nBurnSoundLen;

}

#endif


static UINT16 ics2115_reg_read()
{
	UINT16 ret;

	ics2115_voice& voice = m_voice[m_osc_select];

	switch(m_reg_select)
	{
		case 0x00: // [osc] Oscillator Configuration
			ret = voice.osc_conf.value << 8;
			break;

		case 0x01: // [osc] Wavesample frequency
			ret = voice.osc.fc;
			break;

		case 0x02: // [osc] Wavesample loop start high
			ret = (voice.osc.start >> 16) & 0xffff;
			break;

		case 0x03: // [osc] Wavesample loop start low
			ret = (voice.osc.start >>  0) & 0xff00;
			break;

		case 0x04: // [osc] Wavesample loop end high
			ret = (voice.osc.end >> 16) & 0xffff;
			break;

		case 0x05: // [osc] Wavesample loop end low
			ret = (voice.osc.end >>  0) & 0xff00;
			break;

		case 0x06: // [osc] Volume Increment
			ret = voice.vol.incr;
			break;

		case 0x07: // [osc] Volume Start
			ret = voice.vol.start >> (10 + 8);
			break;

		case 0x08: // [osc] Volume End
			ret = voice.vol.end >> (10 + 8);
			break;

		case 0x09: // [osc] Volume accumulator

			//ics2115_update(get_stream_pos());

			ret = voice.vol.acc >> 10;
			break;

		case 0x0A: // [osc] Wavesample address

			//ics2115_update(get_stream_pos());

			ret = (voice.osc.acc >> 16) & 0xffff;
			break;

		case 0x0B: // [osc] Wavesample address

			//ics2115_update(get_stream_pos());

			ret = (voice.osc.acc >> 0) & 0xfff8;
			break;


		case 0x0C: // [osc] Pan
			ret = voice.vol.pan << 8;
			break;

		case 0x0D: // [osc] Volume Envelope Control

			//ics2115_update(get_stream_pos());

			ret = voice.vol_ctrl.value << 8;
			break;

		case 0x0E: // Active Voices
			ret = m_active_osc;
			break;

		case 0x0F: // [osc] Interrupt source/oscillator

			//ics2115_update(get_stream_pos());

			ret = 0xFF;
			for (INT32 i = 0; i <= m_active_osc; i++)
			{
				ics2115_voice& v = m_voice[i];

				if (v.osc_conf.bitflags.irq_pending || v.vol_ctrl.bitflags.irq_pending)
				{
					ret = i | 0xE0;

					if (v.osc_conf.bitflags.irq_pending)
					{
						v.osc_conf.bitflags.irq_pending = 0;
						ret &= ~0x80;
					}
					if (v.vol_ctrl.bitflags.irq_pending)
					{
						v.vol_ctrl.bitflags.irq_pending = 0;
						ret &= ~0x40;
					}

					ics2115_recalc_irq();

					break;
				}
			}

			ret <<= 8;
			break;

		case 0x10: // [osc] Oscillator Control

			//ics2115_update(get_stream_pos());

			ret = voice.osc.ctl << 8;
			break;

		case 0x11: // [osc] Wavesample static address 27-20
			ret = voice.osc.saddr << 8;
			break;

		case 0x12: // [osc] vmode (reserved, write 0)
			ret = voice.osc.vmode << 8;
			break;


		// general purpose registers


		case 0x40: // Timer 0 clear irq
		case 0x41: // Timer 1 clear irq
			//TODO: examine this suspect code

			//ics2115_update(get_stream_pos());

			ret = m_timer[m_reg_select & 0x1].preset;
			m_timer_irq_pending &= ~(1 << (m_reg_select & 0x1));

			ics2115_recalc_irq();
			break;

		case 0x43: // Timer status

			//ics2115_update(get_stream_pos());

			ret = m_timer_irq_pending & 3;
			break;

		case 0x4A: // IRQ Pending

			//ics2115_update(get_stream_pos());

			ret = m_timer_irq_pending;
			break;

		case 0x4B: // Address of Interrupting Oscillator
			// bprintf(0, _T("IntOscAddr read\n"));

			ret = 0xFF;

			if (m_irq_on)
			{
				for (INT32 i = 0; i <= m_active_osc; i++)
				{
					ics2115_voice& v = m_voice[i];

					if (v.osc_conf.bitflags.irq_pending || v.vol_ctrl.bitflags.irq_pending)
					{
						ret = i | 0xE0;

						// should this be here?

						/* if (v.osc_conf.bitflags.irq_pending)
							ret &= ~0x80;

						if (v.vol_ctrl.bitflags.irq_pending)
							ret &= ~0x40; */

						break;
					}
				}
			}

			ret <<= 8;
			break;

		case 0x4C: // Chip Revision
			ret = revision;
			break;

		default:
			/* if (m_reg_select < 0x40)
				bprintf(PRINT_ERROR, _T("    ICS2115 voice %02X RESERVED register %02X read\n"), m_osc_select, m_reg_select);
			else
				bprintf(PRINT_ERROR, _T("    ICS2115 general purpose register %02X read\n"), m_reg_select); */

			ret = 0;
			break;
	}

	return ret;
}

static void ics2115_reg_write(UINT8 data, bool msb) {
	ics2115_voice& voice = m_voice[m_osc_select];

	//if (m_reg_select <= 0x12)
	//	ics2115_update(get_stream_pos());

	switch(m_reg_select)
	{
		case 0x00: // [osc] Oscillator Configuration
			if (msb)
			{
				// bprintf(0, _T("    ICS2115 voice %02X osc conf -> %02X\n"), m_osc_select, data);

				if (~data & 0x20)
				{
					/* if (voice.osc_conf.bitflags.irq)
						bprintf(0, _T("    ICS2115 voice %02X wavetable irq disabled\n"), m_osc_select); */

					voice.osc_conf.value = data & 0x7F;

					if (m_irq_on)
						ics2115_recalc_irq();
				}
				else
				{
					/* if (!voice.osc_conf.bitflags.irq)
						bprintf(0, _T("    ICS2115 voice %02X wavetable irq enabled\n"), m_osc_select); */

					voice.osc_conf.value = data;

					if ((voice.vol_ctrl.value & 0x80) != (data & 0x80))
						ics2115_recalc_irq();
				}
			}
			break;

		case 0x01: // [osc] Wavesample frequency

			// freq = fc * 33075 / 1024 in 32 voices mode
			//        fc * 44100 / 1024 in 24 voices mode

			if (msb)
				voice.osc.fc = (voice.osc.fc & 0x00ff) | (data << 8);
			else
				voice.osc.fc = (voice.osc.fc & 0xff00) | (data & 0xFE);

#if !defined INTERPOLATE_AS_HARDWARE

			voice.int_fc = (UINT32)((UINT64)(voice.osc.fc) * 0x8000 * m_sample_rate / nBurnSoundRate >> 13);

			/* if (voice.int_fc > (1 << 14))
				bprintf(0, _T("    ICS2115 voice %02X skipping samples (fc: %08X)!\n"), m_osc_select, voice.osc.fc); */

#endif
			break;

		case 0x02: // [osc] Wavesample loop start high (16 bits integer)
			if (msb)
				voice.osc.start = (voice.osc.start & 0x00ffffff) | (data << 24);
			else
				voice.osc.start = (voice.osc.start & 0xff00ffff) | (data << 16);
			break;

		case 0x03: // [osc] Wavesample loop start low (4 bits integer, 4 fraction)
			if (msb)
				voice.osc.start = (voice.osc.start & 0xffff00ff) | (data << 8);
			/* else
				voice.osc.start = (voice.osc.start & 0xffffff00) |  data; */
			break;

		case 0x04: // [osc] Wavesample loop end high (16 bits integer)
			if (msb)
				voice.osc.end = (voice.osc.end & 0x00ffffff) | (data << 24);
			else
				voice.osc.end = (voice.osc.end & 0xff00ffff) | (data << 16);
			break;

		case 0x05: // [osc] Wavesample loop end low (4 bits integer, 4 fraction)
			if (msb)
				voice.osc.end = (voice.osc.end & 0xffff00ff) | (data << 8);
			/* else
				voice.osc.end = (voice.osc.end & 0xffffff00) |  data; */
			break;

		case 0x06: // [osc] Volume Increment

#if defined CAVE_HACK

			if (msb)
				voice.vol.inc_hi = data;
			else
				voice.vol.inc_lo = data;

			voice.vol.incr = voice.vol.inc_lo | voice.vol.inc_hi;

#else

			if (msb)
				voice.vol.incr = data;

#endif

			voice.vol.add = (voice.vol.incr & 0x3F) <<
							(10 - (1 << (3 * (voice.vol.incr >> 6))));

			/* if (msb)
				bprintf(0, _T("    ICS2115 voice %02X vol inc msb -> %02X\n"), m_osc_select, data);
			else
				bprintf(0, _T("    ICS2115 voice %02X vol inc lsb -> %02X\n"), m_osc_select, data); */

			break;

		case 0x07: // [osc] Volume Start (8 bits according to datasheet, though games use 12)
			if (msb)
				voice.vol.start = (voice.vol.start & 0x003FC00) | (data << (10 + 8));
			/* else
				voice.vol.start = (voice.vol.start & 0x3FC0000) | (data <<  10); */

			/* if (msb)
				bprintf(0, _T("    ICS2115 voice %02X vol start msb -> %02X\n"), m_osc_select, data);
			else
				bprintf(0, _T("    ICS2115 voice %02X vol start lsb -> %02X\n"), m_osc_select, data); */

			break;

		case 0x08: // [osc] Volume End (8 bits according to datasheet, though games use 12)
			if (msb)
				voice.vol.end = (voice.vol.end & 0x003FC00) | (data << (10 + 8));
			/* else
				voice.vol.end = (voice.vol.end & 0x3FC0000) | (data <<  10); */

			/* if (msb)
				bprintf(0, _T("    ICS2115 voice %02X vol end msb -> %02X\n"), m_osc_select, data);
			else
				bprintf(0, _T("    ICS2115 voice %02X vol end lsb -> %02X\n"), m_osc_select, data); */

			break;

		case 0x09: // [osc] Volume accumulator
			if (msb)
				voice.vol.acc = (voice.vol.acc & 0x003FC00) | (data << (10 + 8));
			else
				voice.vol.acc = (voice.vol.acc & 0x3FC0000) | (data <<  10);

			/* if (msb)
				bprintf(0, _T("    ICS2115 voice %02X vol acc msb -> %02X\n"), m_osc_select, data);
			else
				bprintf(0, _T("    ICS2115 voice %02X vol acc lsb -> %02X\n"), m_osc_select, data); */

			break;

		case 0x0A: // [osc] Wavesample address high (16 bits integer)
			if (msb)
				voice.osc.acc = (voice.osc.acc & 0x00ffffff) | (data << 24);
			else
				voice.osc.acc = (voice.osc.acc & 0xff00ffff) | (data << 16);
			break;

		case 0x0B: // [osc] Wavesample address low (4 bits integer, 9 fraction)
			if (msb)
				voice.osc.acc = (voice.osc.acc & 0xffff00ff) | (data << 8);
			else
				voice.osc.acc = (voice.osc.acc & 0xffffff00) | (data & 0xF8);
			break;

		case 0x0C: // [osc] Pan
			if (msb)
				voice.vol.pan = data;

			// bprintf(0, _T("    ICS2115 voice %02X pan %02X\n"), m_osc_select, data);

			break;

		case 0x0D: // [osc] Volume Envelope Control
			if (msb)
			{
				/* if ((voice.vol_ctrl.value & 3) && (data & 3) == 0)
					bprintf(0, _T("    ICS2115 voice %02X vol env enabled, %s - [data: %02X]\n"), m_osc_select, (data & 64) ? _T("<<--") : _T("-->>"), data);
				else if((voice.vol_ctrl.value & 3) == 0 && (data & 3))
					bprintf(0, _T("    ICS2115 voice %02X vol env disabled\n"), m_osc_select); */

				if (~data & 0x20)
				{
					/* if (voice.vol_ctrl.bitflags.irq)
						bprintf(0, _T("    ICS2115 voice %02X volume irq disabled\n"), m_osc_select); */

					voice.vol_ctrl.value = data & 0x7F;

					if (m_irq_on)
						ics2115_recalc_irq();
				}
				else
				{
					/* if (!voice.vol_ctrl.bitflags.irq)
						bprintf(0, _T("    ICS2115 voice %02X volume irq enabled\n"), m_osc_select); */

					voice.vol_ctrl.value = data;

					if ((voice.vol_ctrl.value & 0x80) != (data & 0x80))
						ics2115_recalc_irq();
				}
			}
			break;

		case 0x0E: // Active Voices
			if (msb)
			{
				m_active_osc = data & 0x1F;

				set_internal_sample_rate();
			}
			break;

		case 0x10: // [osc] Oscillator Control
			if (msb)
			{
				voice.osc.ctl = data;

				if (!data)
				{
					voice.ramp = 1 << RAMP_BITS;
					voice.prev_addr = ~0;

					/* if (voice.vol_ctrl.bitflags.rollover)
						bprintf(PRINT_ERROR, _T("*** ICS2115 ROLLOVER used - voice %02X enabled: %06X -> %06X\n"), m_osc_select, voice.osc.acc >> 8, voice.osc.end >> 8);
					else if (voice.vol_ctrl.bitflags.loop)
						bprintf(0, _T("    ICS2115 voice %02X enabled: %06X, %06X -> %06X\n"), m_osc_select, voice.osc.acc >> 6, voice.osc.start >> 8, voice.osc.end >> 8);
					else
						bprintf(0, _T("    ICS2115 voice %02X enabled: %06X -> %06X\n"), m_osc_select, voice.osc.acc >> 8, voice.osc.end >> 8); */
				}
				else if (data == 0x0F)	// guessing here
				{
					// bprintf(0, _T("    ICS2115 voice %02X disabled\n"), m_osc_select);

					voice.osc_conf.bitflags.stop = true;
					voice.vol_ctrl.bitflags.done = true;
				}
			}
			break;

		case 0x11: // [osc] Wavesample static address 27-20
			if (msb)
				voice.osc.saddr = data;
			break;

		case 0x12: // [osc] vmode (reserved, write 0)
			if (msb)
				voice.osc.vmode = data;

			/* if (data)
				bprintf(PRINT_ERROR, _T("    ICS2115 voice %02X VMode -> %02X\n"), m_osc_select, data); */

			break;


		// general purpose registers


		case 0x40: // Timer 1 Preset
		//case 0x41: // Timer 2 Preset
			if (!msb)
			{
				m_timer[m_reg_select & 0x1].preset = data;
				ics2115_recalc_timer(m_reg_select & 0x1);
			}
			break;

		case 0x42: // Timer 1 Prescale
		//case 0x43: // Timer 2 Prescale
			if (!msb)
			{
				m_timer[m_reg_select & 0x1].scale = data;
				ics2115_recalc_timer(m_reg_select & 0x1);
			}
			break;

		case 0x4A: // IRQ Enable
			if (!msb)
			{
				//ics2115_update(get_stream_pos());

				m_timer_irq_enabled = data;
				ics2115_recalc_irq();
			}
			break;

		case 0x4F: // Oscillator Address being Programmed
			if (!msb)
				m_osc_select = data & 0x1F;

			break;

		default:
			/* if (m_reg_select < 0x40)
				bprintf(PRINT_ERROR, _T("    ICS2115 voice %02X RESERVED register %02X -> %02X\n"), m_osc_select, m_reg_select, data);
			else
				bprintf(PRINT_ERROR, _T("    ICS2115 general purpose register %02X -> %02X\n"), m_reg_select, data); */

			break;
	}
}

UINT8 ics2115read(UINT8 offset)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_ICS2115Initted) bprintf(PRINT_ERROR, _T("ics2115read called without init\n"));
#endif

	UINT8 ret = 0;

	switch(offset & 3)
	{
		case 0:
			//ics2115_update(get_stream_pos());

			if (m_irq_on)
			{
				ret |= 0x80;

				if (m_timer_irq_enabled && (m_timer_irq_pending & 3))
					ret |= 1;

				for (INT32 i = 0; i <= m_active_osc; i++)
				{
					if (m_voice[i].osc_conf.bitflags.irq_pending || m_voice[i].vol_ctrl.bitflags.irq_pending)
					{
						ret |= 2;
						break;
					}
				}
			}

			break;
		case 1:
			ret = m_reg_select;
			break;
		case 2:
			ret = (UINT8)(ics2115_reg_read());
			break;
		case 3:
			ret = ics2115_reg_read() >> 8;
			break;
		default:
			break;
	}
	return ret;
}

void ics2115write(UINT8 offset, UINT8 data)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_ICS2115Initted) bprintf(PRINT_ERROR, _T("ics2115write called without init\n"));
#endif

	switch(offset & 3) {
		case 1:
			m_reg_select = data;
			break;
		case 2:
			ics2115_reg_write(data, 0);
			break;
		case 3:
			ics2115_reg_write(data, 1);
			break;
		default:
			break;
	}
}

void ics2115_recalc_irq()
{
	bool irq = (m_timer_irq_pending & m_timer_irq_enabled);

	for (INT32 i = 0; (!irq) && (i < 32); i++)
		irq |= m_voice[i].vol_ctrl.bitflags.irq_pending || m_voice[i].osc_conf.bitflags.irq_pending;

	m_irq_on = irq;

	if (m_irq_cb)
		m_irq_cb(irq ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

INT32 ics2115_timer_cb(INT32 /* n */, INT32 c)
{
	m_timer_irq_pending |= 1 << c;

	ics2115_recalc_irq();

	return m_irq_on ? 1 : 0;
}

void ics2115_recalc_timer(INT32 timer)
{
	// adjusted from nsecs -> fba timer ticks/sec
	UINT64 period = ((m_timer[timer].scale & 0x1f) + 1) * (m_timer[timer].preset + 1);
	period = (period << (4 + (m_timer[timer].scale >> 5))) * 160000 / 2646;

	if (m_timer[timer].period == period)
		return;

	m_timer[timer].period = period;
	BurnTimerSetRetrig(timer, period);

	/* UINT64 period = ((m_timer[timer].scale & 0x1f) + 1) * (m_timer[timer].preset + 1);
	period = (period << (4 + (m_timer[timer].scale >> 5))) * 78125 / 2646; */
}


void ics2115_scan(INT32 nAction, INT32* pnMin)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_ICS2115Initted) bprintf(PRINT_ERROR, _T("ics2115_scan called without init\n"));
#endif

	if (pnMin)
		*pnMin = 0x029743;

	BurnTimerScan(nAction, pnMin);

	if (nAction & ACB_DRIVER_DATA)
	{
		SCAN_VAR(m_timer_irq_enabled);
		SCAN_VAR(m_timer_irq_pending);
		SCAN_VAR(m_active_osc);
		SCAN_VAR(m_osc_select);
		SCAN_VAR(m_reg_select);
		SCAN_VAR(m_vmode);
		SCAN_VAR(m_irq_on);

		SCAN_VAR(m_voice);

		for (INT32 i = 0; i < 2; i++)
		{
			SCAN_VAR(m_timer[i].period);
			SCAN_VAR(m_timer[i].scale);
			SCAN_VAR(m_timer[i].preset);
		}

		for (INT32 i = 0; i < 32; i++)
		{
			SCAN_VAR(m_voice[i].osc_conf.value);
			SCAN_VAR(m_voice[i].osc.fc);
			SCAN_VAR(m_voice[i].osc.acc);
			SCAN_VAR(m_voice[i].osc.start);
			SCAN_VAR(m_voice[i].osc.end);
			SCAN_VAR(m_voice[i].osc.ctl);
			SCAN_VAR(m_voice[i].osc.saddr);
			SCAN_VAR(m_voice[i].vol.acc);
			SCAN_VAR(m_voice[i].vol.incr); SCAN_VAR(m_voice[i].vol.inc_lo); SCAN_VAR(m_voice[i].vol.inc_hi);
			SCAN_VAR(m_voice[i].vol.start);
			SCAN_VAR(m_voice[i].vol.end);
			SCAN_VAR(m_voice[i].vol.pan);
			SCAN_VAR(m_voice[i].vol_ctrl.value);
			SCAN_VAR(m_voice[i].vol.mode);
			SCAN_VAR(m_voice[i].ramp);

			SCAN_VAR(m_voice[i].prev_addr);
			SCAN_VAR(m_voice[i].int_buf);
		}

		if (nAction & ACB_WRITE)
			set_internal_sample_rate();
	}
}
