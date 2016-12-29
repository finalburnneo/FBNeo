// license:???
// copyright-holders:Alex Marshall,nimitz,austere
//ICS2115 by Raiden II team (c) 2010
//members: austere, nimitz, Alex Marshal
//
//Original driver by O. Galibert, ElSemi
//
//Use tab size = 4 for your viewing pleasure.

//Ported from MAME git, 13/07/2015

#include "burnint.h"
#include <cmath>

static UINT8 *m_rom = NULL; // ics2115 rom
static INT32 m_rom_len;
static void (*m_irq_cb)(INT32) = NULL; // cpu irq callback
static INT32 sound_cpu_clock = 0;
static INT16 *stream = NULL;

INT32 ICS2115_ddp2beestormmode = 0; // hack to fix volume fadeouts in ddp2 bee storm

struct ics2115_voice {
	struct {
		INT32 left;
		UINT32 acc, start, end;
		UINT16 fc;
		UINT8 ctl, saddr;
	} osc;

	struct {
		INT32 left;
		UINT32 add;
		UINT32 start, end;
		UINT32 acc;
		UINT16 regacc;
		UINT8 incr;
		UINT8 pan, mode;
	} vol;

	union {
		struct {
			UINT8 ulaw       : 1;
			UINT8 stop       : 1;   //stops wave + vol envelope
			UINT8 eightbit   : 1;
			UINT8 loop       : 1;
			UINT8 loop_bidir : 1;
			UINT8 irq        : 1;
			UINT8 invert     : 1;
			UINT8 irq_pending: 1;
			//IRQ on variable?
		} bitflags;
		UINT8 value;
	} osc_conf;

	union {
		struct {
			UINT8 done       : 1;   //indicates ramp has stopped
			UINT8 stop       : 1;   //stops the ramp
			UINT8 rollover   : 1;   //rollover (TODO)
			UINT8 loop       : 1;
			UINT8 loop_bidir : 1;
			UINT8 irq        : 1;   //enable IRQ generation
			UINT8 invert     : 1;   //invert direction
			UINT8 irq_pending: 1;   //(read only) IRQ pending
			//noenvelope == (done | disable)
		} bitflags;
		UINT8 value;
	} vol_ctrl;

	//Possibly redundant state. => improvements of wavetable logic
	//may lead to its elimination.
	union {
		struct {
			UINT8 on         : 1;
			UINT8 ramp       : 7;       // 100 0000 = 0x40 maximum
		} bitflags;
		UINT8 value;
	} state;

	bool playing();
	int update_volume_envelope();
	int update_oscillator();
	void update_ramp();
};

static const UINT16 revision = 0x1;

static INT16 m_ulaw[256];
static UINT16 m_volume[4096];
static const int volume_bits = 15;

static ics2115_voice m_voice[32];
struct timer_struct {
	UINT8 scale, preset;
	INT32 timer_enable;
	INT32 cycles_left;
	INT32 cycles_total;
	void (*timer_cb)(INT32);
	UINT64 period;  /* in nsec */
};
static timer_struct m_timer[2];

static UINT8 m_active_osc;
static UINT8 m_osc_select;
static UINT8 m_reg_select;
static UINT8 m_irq_enabled, m_irq_pending;
static bool m_irq_on;
static UINT8 m_vmode;

//internal register helper functions
void ics2115_recalc_timer(int timer);
void ics2115_keyon();
void ics2115_recalc_irq();
void ics2115_timer_cb_0(INT32 param);
void ics2115_timer_cb_1(INT32 param);


void ics2115_init(INT32 cpu_clock, void (*cpu_irq_cb)(INT32), UINT8 *sample_rom, INT32 sample_rom_size)
{
	DebugSnd_ICS2115Initted = 1;

	m_timer[0].timer_cb = ics2115_timer_cb_0;
	m_timer[1].timer_cb = ics2115_timer_cb_1;

	m_irq_cb = cpu_irq_cb;
	m_rom = sample_rom;
	m_rom_len = sample_rom_size;
	sound_cpu_clock = cpu_clock;

	for (int i = 0; i<4096; i++) {
		m_volume[i] = ((0x100 | (i & 0xff)) << (volume_bits-9)) >> (15 - (i>>8));
	}

	//u-Law table as per MIL-STD-188-113
	UINT16 lut[8];
	UINT16 lut_initial = 33 << 2;   //shift up 2-bits for 16-bit range.
	for(int i = 0; i < 8; i++)
		lut[i] = (lut_initial << i) - lut_initial;
	for(int i = 0; i < 256; i++) {
		UINT8 exponent = (~i >> 4) & 0x07;
		UINT8 mantissa = ~i & 0x0f;
		INT16 value = lut[exponent] + (mantissa << (exponent + 3));
		m_ulaw[i] = (i & 0x80) ? -value : value;
	}
}

void ics2115_exit()
{
#if defined FBA_DEBUG
	if (!DebugSnd_ICS2115Initted) bprintf(PRINT_ERROR, _T("ics2115_exit called without init\n"));
#endif

	if (!DebugSnd_ICS2115Initted) return;

	if (stream) {
		BurnFree(stream);
		stream = NULL;
	}

	m_rom = NULL;
	m_rom_len = 0;
	m_irq_cb= NULL;
	sound_cpu_clock = 0;
	ICS2115_ddp2beestormmode = 0;

	DebugSnd_ICS2115Initted = 0;
}

void ics2115_reset()
{
#if defined FBA_DEBUG
	if (!DebugSnd_ICS2115Initted) bprintf(PRINT_ERROR, _T("ics2115_reset called without init\n"));
#endif

	m_irq_enabled = 0;
	m_irq_pending = 0;
	//possible re-suss
	m_active_osc = 31;
	m_osc_select = 0;
	m_reg_select = 0;
	m_vmode = 0;
	m_irq_on = false;
	memset(m_voice, 0, sizeof(m_voice));
	for(int i = 0; i < 2; ++i)
	{
		m_timer[i].timer_enable = 0;
		m_timer[i].cycles_left = 0;
		m_timer[i].cycles_total = 0;
		m_timer[i].period = 0;
		m_timer[i].scale = 0;
		m_timer[i].preset = 0;
	}
	for(int i = 0; i < 32; i++) {
		m_voice[i].osc_conf.value = 2;
		m_voice[i].osc.fc = 0;
		m_voice[i].osc.acc = 0;
		m_voice[i].osc.start = 0;
		m_voice[i].osc.end = 0;
		m_voice[i].osc.ctl = 0;
		m_voice[i].osc.saddr = 0;
		m_voice[i].vol.acc = 0;
		m_voice[i].vol.incr = 0;
		m_voice[i].vol.start = 0;
		m_voice[i].vol.end = 0;
		m_voice[i].vol.pan = 0x7F;
		m_voice[i].vol_ctrl.value = 1;
		m_voice[i].vol.mode = 0;
		m_voice[i].state.value = 0;
	}
}

//TODO: improve using next-state logic from column 126 of patent 5809466
int ics2115_voice::update_volume_envelope()
{
	int ret = 0;

	if(vol_ctrl.bitflags.done || vol_ctrl.bitflags.stop)
		return ret;

	if(vol_ctrl.bitflags.invert) {
		vol.acc -= vol.add;
		vol.left = vol.acc - vol.start;
	} else {
		vol.acc += vol.add;
		vol.left = vol.end - vol.acc;
	}

	if(vol.left > 0)
		return ret;

	if(vol_ctrl.bitflags.irq) {
		vol_ctrl.bitflags.irq_pending = true;
		ret = 1;
	}

	if(osc_conf.bitflags.eightbit)
		return ret;

	if(vol_ctrl.bitflags.loop) {
		if(vol_ctrl.bitflags.loop_bidir)
			vol_ctrl.bitflags.invert = !vol_ctrl.bitflags.invert;

		if(vol_ctrl.bitflags.invert)
			vol.acc = vol.end + vol.left;
		else
			vol.acc = vol.start - vol.left;
	} else {
		state.bitflags.on = false;
		vol_ctrl.bitflags.done = true;
		if(vol_ctrl.bitflags.invert)
			vol.acc = vol.end;
		else
			vol.acc = vol.start;
	}

	return ret;
}

int ics2115_voice::update_oscillator()
{
	int ret = 0;
	if(osc_conf.bitflags.stop)
		return ret;
	if(osc_conf.bitflags.invert) {
		osc.acc -= osc.fc << 2;
		osc.left = osc.acc - osc.start;
	} else {
		osc.acc += osc.fc << 2;
		osc.left = osc.end - osc.acc;
	}
	// > instead of >= to stop crackling?
	if(osc.left > 0)
		return ret;
	if(osc_conf.bitflags.irq) {
		osc_conf.bitflags.irq_pending = true;
		ret = 1;
	}
	if(osc_conf.bitflags.loop) {
		if(osc_conf.bitflags.loop_bidir)
			osc_conf.bitflags.invert = !osc_conf.bitflags.invert;

		if(osc_conf.bitflags.invert) {
			osc.acc = osc.end + osc.left;
			osc.left = osc.acc - osc.start;
		}
		else {
			osc.acc = osc.start - osc.left;
			osc.left = osc.end - osc.acc;
		}
	} else {
		state.bitflags.on = false;
		osc_conf.bitflags.stop = true;
		if(!osc_conf.bitflags.invert)
			osc.acc = osc.end;
		else
			osc.acc = osc.start;
	}
	return ret;
}

static INT32 get_sample(ics2115_voice& voice)
{
	UINT32 curaddr = ((voice.osc.saddr << 20) & 0xffffff) | (voice.osc.acc >> 12);
	UINT32 nextaddr;

	if (voice.state.bitflags.on && voice.osc_conf.bitflags.loop && !voice.osc_conf.bitflags.loop_bidir &&
			(voice.osc.left < (voice.osc.fc <<2))) {
		nextaddr = ((voice.osc.saddr << 20) & 0xffffff) | (voice.osc.start >> 12);
	}
	else
		nextaddr = curaddr + 2;


	INT16 sample1, sample2;
	if (voice.osc_conf.bitflags.eightbit) {
		sample1 = ((INT8)m_rom[curaddr]) << 8;
		sample2 = ((INT8)m_rom[curaddr + 1]) << 8;
	}
	else {
		sample1 = m_rom[curaddr + 0] | (((INT8)m_rom[curaddr + 1]) << 8);
		sample2 = m_rom[nextaddr+ 0] | (((INT8)m_rom[nextaddr+ 1]) << 8);
		//sample2 = m_rom[curaddr + 2] | (((INT8)m_rom[curaddr + 3]) << 8);
	}

	INT32 sample, diff;
	UINT16 fract;
	diff = sample2 - sample1;
	fract = (voice.osc.acc >> 3) & 0x1ff;

	sample = (((INT32)sample1 << 9) + diff * fract) >> 9;

	return sample;
}

bool ics2115_voice::playing()
{
	return state.bitflags.on && !((vol_ctrl.bitflags.done || vol_ctrl.bitflags.stop) && osc_conf.bitflags.stop);
}

void ics2115_voice::update_ramp() {
	//slow attack
	if (state.bitflags.on && !osc_conf.bitflags.stop) {
		if (state.bitflags.ramp < 0x40)
			state.bitflags.ramp += 0x1;
		else
			state.bitflags.ramp = 0x40;
	}
	//slow release
	else {
		if (state.bitflags.ramp)
			state.bitflags.ramp -= 0x1;
	}
}

static int ics2115_fill_output(ics2115_voice& voice, INT16 *outputs, int samples)
{
	bool irq_invalid = false;
	UINT16 fine = 1 << (3*(voice.vol.incr >> 6));
	voice.vol.add = (voice.vol.incr & 0x3F)<< (10 - fine);

	for (int i = 0; i < samples; i++) {
		UINT32 volacc = (voice.vol.acc >> 10) & 0xffff;
		UINT32 volume = (m_volume[volacc >> 4] * voice.state.bitflags.ramp) >> 6;
		UINT16 vleft = volume; //* (255 - voice.vol.pan) / 0x80];
		UINT16 vright = volume; //* (voice.vol.pan + 1) / 0x80];

		INT32 sample;
		if(voice.osc_conf.bitflags.ulaw) {
			UINT32 curaddr = ((voice.osc.saddr << 20) & 0xffffff) | (voice.osc.acc >> 12);
			sample = m_ulaw[m_rom[curaddr]];
		}
		else
			sample = get_sample(voice);

		if (!m_vmode || voice.playing()) {
			outputs[0] = BURN_SND_CLIP((INT32)(outputs[0] + (((sample * vleft) >> (5 + volume_bits - 16)) >> 16)));
			outputs[1] = BURN_SND_CLIP((INT32)(outputs[1] + (((sample * vright) >> (5 + volume_bits - 16)) >> 16)));
			outputs += 2;
		}

		voice.update_ramp();
		if (voice.playing()) {
			if (voice.update_oscillator())
				irq_invalid = true;
			if (ICS2115_ddp2beestormmode == 0) {
				if (voice.update_volume_envelope())
					irq_invalid = true;
			}
		}
	}
	return irq_invalid;
}

void ics2115_update(INT16 *outputs, int samples)
{
#if defined FBA_DEBUG
	if (!DebugSnd_ICS2115Initted) bprintf(PRINT_ERROR, _T("ics2115_update called without init\n"));
#endif

	INT32 sample_per_frame = (33075 * 100) / nBurnFPS;

	if (stream == NULL) {
		stream = (INT16*)malloc(sample_per_frame * 4); // * 4 -> (16 bits, 2 channels)
	}

//	memset(outputs, 0, samples * sizeof(INT16) * 2);
	memset(stream, 0, sample_per_frame * 4);

	bool irq_invalid = false;
	for(int osc = 0; osc <= m_active_osc; osc++) {
		ics2115_voice& voice = m_voice[osc];

		if(ics2115_fill_output(voice, stream, (33075 * 100) / nBurnFPS))
			irq_invalid = true;
	}

	//rescale
	for (int i = 0; i < samples; i++) {

		INT32 offset = ((i * sample_per_frame) / nBurnSoundLen) * 2;
		outputs[0] = (INT32)(stream[offset+0] * 5); // volume for pgm is 5.0
		outputs[1] = (INT32)(stream[offset+1] * 5);
		outputs += 2;
	}

	if(irq_invalid)
		ics2115_recalc_irq();

}

static UINT16 ics2115_reg_read() {
	UINT16 ret;
	ics2115_voice& voice = m_voice[m_osc_select];

	switch(m_reg_select) {
		case 0x00: // [osc] Oscillator Configuration
			ret = voice.osc_conf.value;
			ret <<= 8;
			break;

		case 0x01: // [osc] Wavesample frequency
			ret = voice.osc.fc;
			break;

		case 0x02: // [osc] Wavesample loop start high
			//TODO: are these returns valid? might be 0x00ff for this one...
			ret = (voice.osc.start >> 16) & 0xffff;
			break;

		case 0x03: // [osc] Wavesample loop start low
			ret = (voice.osc.start >> 0) & 0xff00;
			break;

		case 0x04: // [osc] Wavesample loop end high
			ret = (voice.osc.end >> 16) & 0xffff;
			break;

		case 0x05: // [osc] Wavesample loop end low
			ret = (voice.osc.end >> 0) & 0xff00;
			break;

		case 0x06: // [osc] Volume Increment
			ret = voice.vol.incr;
			break;

		case 0x07: // [osc] Volume Start
			ret = voice.vol.start >> (10+8);
			break;

		case 0x08: // [osc] Volume End
			ret = voice.vol.end >> (10+8);
			break;

		case 0x09: // [osc] Volume accumulator
			//ret = v->Vol.Acc;
			ret = voice.vol.acc  >> (10);
			break;

		case 0x0A: // [osc] Wavesample address
			ret = (voice.osc.acc >> 16) & 0xffff;
			break;

		case 0x0B: // [osc] Wavesample address
			ret = (voice.osc.acc >> 0) & 0xfff8;
			break;


		case 0x0C: // [osc] Pan
			ret = voice.vol.pan << 8;
			break;

		case 0x0D: // [osc] Volume Envelope Control
			if (!m_vmode)
				ret = voice.vol_ctrl.bitflags.irq ? 0x81 : 0x01;
			else
				ret = 0x01;
			ret <<= 8;
			break;

		case 0x0E: // Active Voices
			ret = m_active_osc;
			break;

		case 0x0F:{// [osc] Interrupt source/oscillator
			ret = 0xff;
			for (int i = 0; i <= m_active_osc; i++) {
				ics2115_voice& v = m_voice[i];
				if (v.osc_conf.bitflags.irq_pending || v.vol_ctrl.bitflags.irq_pending) {
					ret = i | 0xe0;
					ret &= v.vol_ctrl.bitflags.irq_pending ? (~0x40) : 0xff;
					ret &= v.osc_conf.bitflags.irq_pending ? (~0x80) : 0xff;
					ics2115_recalc_irq();
					if (v.osc_conf.bitflags.irq_pending) {
						v.osc_conf.bitflags.irq_pending = 0;
						ret &= ~0x80;
					}
					if (v.vol_ctrl.bitflags.irq_pending) {
						v.vol_ctrl.bitflags.irq_pending = 0;
						ret &= ~0x40;
					}
					break;
				}
			}
			ret <<= 8;
			break;}

		case 0x10: // [osc] Oscillator Control
			ret = voice.osc.ctl << 8;
			break;

		case 0x11: // [osc] Wavesample static address 27-20
			ret = voice.osc.saddr << 8;
			break;

		case 0x40: // Timer 0 clear irq
		case 0x41: // Timer 1 clear irq
			//TODO: examine this suspect code
			ret = m_timer[m_reg_select & 0x1].preset;
			m_irq_pending &= ~(1 << (m_reg_select & 0x1));
			ics2115_recalc_irq();
			break;

		case 0x43: // Timer status
			ret = m_irq_pending & 3;
			break;

		case 0x4A: // IRQ Pending
			ret = m_irq_pending;
			break;

		case 0x4B: // Address of Interrupting Oscillator
			ret = 0x80;
			break;

		case 0x4C: // Chip Revision
			ret = revision;
			break;

		default:
			ret = 0;
			break;
	}
	return ret;
}

static void ics2115_reg_write(UINT8 data, bool msb) {
	ics2115_voice& voice = m_voice[m_osc_select];

	switch(m_reg_select) {
		case 0x00: // [osc] Oscillator Configuration
			if(msb) {
				voice.osc_conf.value &= 0x80;
				voice.osc_conf.value |= data & 0x7f;
			}
			break;

		case 0x01: // [osc] Wavesample frequency
			// freq = fc*33075/1024 in 32 voices mode, fc*44100/1024 in 24 voices mode
			if(msb)
				voice.osc.fc = (voice.osc.fc & 0x00ff) | (data << 8);
			else
				//last bit not used!
				voice.osc.fc = (voice.osc.fc & 0xff00) | (data & 0xfe);
			break;

		case 0x02: // [osc] Wavesample loop start high
			if(msb)
				voice.osc.start = (voice.osc.start & 0x00ffffff) | (data << 24);
			else
				voice.osc.start = (voice.osc.start & 0xff00ffff) | (data << 16);
			break;

		case 0x03: // [osc] Wavesample loop start low
			if(msb)
				voice.osc.start = (voice.osc.start & 0xffff00ff) | (data << 8);
			// This is unused?
			//else
				//voice.osc.start = (voice.osc.start & 0xffffff00) | (data & 0);
			break;

		case 0x04: // [osc] Wavesample loop end high
			if(msb)
				voice.osc.end = (voice.osc.end & 0x00ffffff) | (data << 24);
			else
				voice.osc.end = (voice.osc.end & 0xff00ffff) | (data << 16);
			break;

		case 0x05: // [osc] Wavesample loop end low
			if(msb)
				voice.osc.end = (voice.osc.end & 0xffff00ff) | (data << 8);
			// lsb is unused?
			break;

		case 0x06: // [osc] Volume Increment
			if(msb)
				voice.vol.incr = data;
			break;

		case 0x07: // [osc] Volume Start
			if (!msb)
				voice.vol.start = data << (10+8);
			break;

		case 0x08: // [osc] Volume End
			if (!msb)
				voice.vol.end = data << (10+8);
			break;

		case 0x09: // [osc] Volume accumulator
			if(msb)
				voice.vol.regacc = (voice.vol.regacc & 0x00ff) | (data << 8);
			else
				voice.vol.regacc = (voice.vol.regacc & 0xff00) | data;
			voice.vol.acc = voice.vol.regacc << 10;
			break;

		case 0x0A: // [osc] Wavesample address high
			if(msb)
				voice.osc.acc = (voice.osc.acc & 0x00ffffff) | (data << 24);
			else
				voice.osc.acc = (voice.osc.acc & 0xff00ffff) | (data << 16);
			break;

		case 0x0B: // [osc] Wavesample address low
			if(msb)
				voice.osc.acc = (voice.osc.acc & 0xffff00ff) | (data << 8);
			else
				voice.osc.acc = (voice.osc.acc & 0xffffff00) | (data & 0xF8);
			break;

		case 0x0C: // [osc] Pan
			if(msb)
				voice.vol.pan = data;
			break;

		case 0x0D: // [osc] Volume Envelope Control
			if(msb) {
				voice.vol_ctrl.value &= 0x80;
				voice.vol_ctrl.value |= data & 0x7F;
			}
			break;

		case 0x0E: // Active Voices
			//Does this value get added to 1? Not sure. Could trace for writes of 32.
			if(msb) {
				m_active_osc = data & 0x1F; // & 0x1F ? (Guessing)
			}
			break;
		//2X8 ?
		case 0x10: // [osc] Oscillator Control
			//Could this be 2X9?
			//[7 R | 6 M2 | 5 M1 | 4-2 Reserve | 1 - Timer 2 Strt | 0 - Timer 1 Strt]

			if (msb) {
				voice.osc.ctl = data;
				if (!data)
					ics2115_keyon();
				//guessing here
				else if(data == 0xf) {
					if (!m_vmode) {
						voice.osc_conf.bitflags.stop = true;
						voice.vol_ctrl.bitflags.stop = true;
						//try to key it off as well!
						voice.state.bitflags.on = false;
					}
				}
			}
			break;

		case 0x11: // [osc] Wavesample static address 27-20
			if(msb)
				//v->Osc.SAddr = data;
				voice.osc.saddr = data;
			break;
		case 0x12:
			//Could be per voice! -- investigate.
			if (msb)
				m_vmode = data;
			break;
		case 0x40: // Timer 1 Preset
		case 0x41: // Timer 2 Preset
			if(!msb) {
				m_timer[m_reg_select & 0x1].preset = data;
				ics2115_recalc_timer(m_reg_select & 0x1);
			}
			break;

		case 0x42: // Timer 1 Prescale
		case 0x43: // Timer 2 Prescale
			if(!msb) {
				m_timer[m_reg_select & 0x1].scale = data;
				ics2115_recalc_timer(m_reg_select & 0x1);
			}
			break;

		case 0x4A: // IRQ Enable
			if(!msb) {
				m_irq_enabled = data;
				ics2115_recalc_irq();
			}
			break;

		case 0x4F: // Oscillator Address being Programmed
			if(!msb) {
				m_osc_select = data % (1+m_active_osc);
			}
			break;
		default:
			break;
	}
}

UINT8 ics2115read(UINT8 offset)
{
#if defined FBA_DEBUG
	if (!DebugSnd_ICS2115Initted) bprintf(PRINT_ERROR, _T("ics2115read called without init\n"));
#endif

	UINT8 ret = 0;

	switch(offset & 3) {
		case 0:
			//TODO: check this suspect code
			if (m_irq_on) {
				ret |= 0x80;
				if (m_irq_enabled && (m_irq_pending & 3))
					ret |= 1;
				for (int i = 0; i <= m_active_osc; i++) {
					if (m_voice[i].osc_conf.bitflags.irq_pending) {
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
#if defined FBA_DEBUG
	if (!DebugSnd_ICS2115Initted) bprintf(PRINT_ERROR, _T("ics2115write called without init\n"));
#endif

	switch(offset & 3) {
		case 1:
			m_reg_select = data;
			break;
		case 2:
			ics2115_reg_write(data,0);
			break;
		case 3:
			ics2115_reg_write(data,1);
			break;
		default:
			break;
	}
}

void ics2115_keyon()
{
	//set initial condition (may need to invert?) -- does NOT work since these are set to zero even
	m_voice[m_osc_select].state.bitflags.on = true;
	//no ramp up...
	m_voice[m_osc_select].state.bitflags.ramp = 0x40;
}

void ics2115_recalc_irq()
{
	//Suspect
	bool irq = (m_irq_pending & m_irq_enabled);
	for(int i = 0; (!irq) && (i < 32); i++)
		irq |=  m_voice[i].vol_ctrl.bitflags.irq_pending && m_voice[i].osc_conf.bitflags.irq_pending;
	m_irq_on = irq;
	if(m_irq_cb)
		m_irq_cb(irq ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

void ics2115_timer_cb_0(INT32 /*param*/)
{
	m_irq_pending |= 1 << 0;
	ics2115_recalc_irq();
}

void ics2115_timer_cb_1(INT32 /*param*/)
{
	m_irq_pending |= 1 << 1;
	ics2115_recalc_irq();
}

void ics2115_recalc_timer(int timer)
{
	UINT64 period  = ((m_timer[timer].scale & 0x1f) + 1) * (m_timer[timer].preset + 1);
	period = (period << (4 + (m_timer[timer].scale >> 5)))*78125/2646;

	if(m_timer[timer].period != period) {
		m_timer[timer].period = period;
	
		if(period) // Reset the length
		{
			m_timer[timer].timer_enable = 1;

			float cycles_until_irq = sound_cpu_clock * ((period * 1.0000) / 1000000000.0000);

			m_timer[timer].cycles_left = (INT32)cycles_until_irq;
			m_timer[timer].cycles_total = m_timer[timer].cycles_left;

	// 		m_timer[timer].timer->adjust(attotime::from_nsec(period), 0, attotime::from_nsec(period));
		}
		else // Kill the timer if length == 0
			m_timer[timer].timer_enable = 0; //timer->adjust(attotime::never);
	}
}

// add-on function to deal with timer.
void ics2115_adjust_timer(INT32 ticks)
{
	for (INT32 i = 0; i < 2; i++) {
		if (m_timer[i].timer_enable) {
			m_timer[i].cycles_left -= ticks;
			if (m_timer[i].cycles_left <= 0) {
				m_timer[i].timer_cb(0);
				m_timer[i].cycles_left = m_timer[i].cycles_total;
			}
		}
	}
}


void ics2115_scan(INT32 nAction,INT32 * /*pnMin*/)
{
#if defined FBA_DEBUG
	if (!DebugSnd_ICS2115Initted) bprintf(PRINT_ERROR, _T("ics2115_scan called without init\n"));
#endif

	if ( nAction & ACB_DRIVER_DATA ) {

		SCAN_VAR(m_irq_enabled);
		SCAN_VAR(m_irq_pending);
		SCAN_VAR(m_active_osc);
		SCAN_VAR(m_osc_select);
		SCAN_VAR(m_reg_select);
		SCAN_VAR(m_vmode);
		SCAN_VAR(m_irq_on);

		SCAN_VAR(m_voice);

		for(int i = 0; i < 2; ++i)
		{
			SCAN_VAR(m_timer[i].timer_enable);
			SCAN_VAR(m_timer[i].cycles_left);
			SCAN_VAR(m_timer[i].cycles_total);
			SCAN_VAR(m_timer[i].period);
			SCAN_VAR(m_timer[i].scale);
			SCAN_VAR(m_timer[i].preset);
		}

		for(int i = 0; i < 32; i++) {
			SCAN_VAR(m_voice[i].osc_conf.value);
			SCAN_VAR(m_voice[i].osc.fc);
			SCAN_VAR(m_voice[i].osc.acc);
			SCAN_VAR(m_voice[i].osc.start);
			SCAN_VAR(m_voice[i].osc.end);
			SCAN_VAR(m_voice[i].osc.ctl);
			SCAN_VAR(m_voice[i].osc.saddr);
			SCAN_VAR(m_voice[i].vol.acc);
			SCAN_VAR(m_voice[i].vol.incr);
			SCAN_VAR(m_voice[i].vol.start);
			SCAN_VAR(m_voice[i].vol.end);
			SCAN_VAR(m_voice[i].vol.pan);
			SCAN_VAR(m_voice[i].vol_ctrl.value);
			SCAN_VAR(m_voice[i].vol.mode);
			SCAN_VAR(m_voice[i].state.value);
		}
	}
}
