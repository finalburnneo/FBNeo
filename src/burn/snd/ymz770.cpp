// license:BSD-3-Clause
// copyright-holders:Olivier Galibert, R. Belmont, MetalliC
/***************************************************************************

    Yamaha YMZ770C "AMMS-A" and YMZ774 "AMMS2C"

    Emulation by R. Belmont and MetalliC
    AMM decode by Olivier Galibert

-----
TODO:
- What does channel ATBL mean?
- Simple Access mode. SACs is register / data lists same as SEQ. in 770C, when both /SEL and /CS pins goes low - will be run SAC with number set at data bus.
  can not be used in CV1K (/SEL pin is NC, internally pulled to VCC), probably not used in PGM2 too.
 770:
- sequencer timers implemented but seems unused, presumably because of design flaws or bugs, likely due to lack of automatic adding of sequencer # to register offset.
  in result sequences uses very long chains of 32-sample wait commands instead, wasting a lot of ROM space.
- sequencer triggers not implemented, not sure how they works (Deathsmiles ending tune starts sequence with TGST = 01h, likely a bug and don't affect tune playback)
 774:
- 4 channel output
- Equalizer
- pan delayed transition (not used in games)
- sequencer off trigger (not used in games)

 known SPUs in this series:
  YMZ770B  AMMSL    Capcom medal hardware (alien.cpp), sample format is not AMM, in other parts looks like 770C
  YMZ770C  AMMS-A   Cave CV1000
  YMZ771   SSGS3
  YMZ773   AMMS2    Cron corp. video slots
  YMZ775   AMMS2B
  YMZ774   AMMS2C   IGS PGM2
  YMZ776   AMMS3    uses AM3 sample format (modified Ogg?)
  YMZ778   AMMS3S
  YMZ779   AMMS3D
  YMZ870   AMMS3EX
// ported from mame 0.228 -dink  april, 2022
***************************************************************************/

#include "burnint.h"
#include "ymz770.h"
#include "stream.h"
#include "mpeg_audio.h"

#define logerror

static Stream stream;

static UINT8 *m_rom;
static INT32 m_rom_size;
static INT32 m_rom_mask;

static void (*sequencer)();
static void (*internal_reg_write)(UINT8, UINT8);
static UINT32 (*get_phrase_offs)(int);
static UINT32 (*get_seq_offs)(int);

static void ymz770_internal_reg_write(UINT8 reg, UINT8 data); // forward
static void ymz774_internal_reg_write(UINT8 reg, UINT8 data);
static void ymz770_sequencer();
static void ymz774_sequencer();
static void ymz770_stream_update(INT16 **streams, INT32 samples);

static UINT32 ymz770_get_phrase_offs(int phrase) { return m_rom[(4 * phrase) + 1] << 16 | m_rom[(4 * phrase) + 2] << 8 | m_rom[(4 * phrase) + 3]; }
static UINT32 ymz770_get_seq_offs(int sqn) { return m_rom[(4 * sqn) + 1 + 0x400] << 16 | m_rom[(4 * sqn) + 2 + 0x400] << 8 | m_rom[(4 * sqn) + 3 + 0x400]; }
static UINT8 get_rom_byte(UINT32 offset) { return m_rom[offset & m_rom_mask]; }

static UINT32 ymz774_get_phrase_offs(int phrase) { int ph = phrase * 4; return ((m_rom[ph] & 0x0f) << 24 | m_rom[ph + 1] << 16 | m_rom[ph + 2] << 8 | m_rom[ph + 3]) * 2; }
static UINT32 ymz774_get_seq_offs(int sqn) { int sq = sqn * 4 + 0x2000; return ((m_rom[sq] & 0x0f) << 24 | m_rom[sq + 1] << 16 | m_rom[sq + 2] << 8 | m_rom[sq + 3]) * 2; }
static UINT32 ymz774_get_sqc_offs(int sqc) { int sq = sqc * 4 + 0x6000; return ((m_rom[sq] & 0x0f) << 24 | m_rom[sq + 1] << 16 | m_rom[sq + 2] << 8 | m_rom[sq + 3]) * 2; }

// data
static UINT8 m_cur_reg;
static UINT8 m_mute;         // mute chip
static UINT8 m_doen;         // digital output enable
static UINT8 m_vlma;         // overall volume L0/R0
static UINT8 m_vlma1;        // overall volume L1/R1
static UINT8 m_bsl;          // boost level
static UINT8 m_cpl;          // clip limiter
static int   m_bank;         // ymz774
static UINT32 volinc[256];	 // ymz774 (no scan)

struct ymz_channel
{
	UINT16 phrase;
	UINT8 pan;
	UINT8 pan_delay;
	UINT8 pan1;
	UINT8 pan1_delay;
	INT32 volume;
	UINT8 volume_target;
	UINT8 volume_delay;
	UINT8 volume2;
	UINT8 loop;

	bool is_playing, last_block, is_paused;

	INT16 output_data[0x1000];
	int output_remaining;
	int output_ptr;
	int atbl;
	int pptr;

	int end_channel_data;
};

static mpeg_audio *mpeg_decoder[16];

struct ymz_sequence
{
	INT32 delay;
	UINT16 sequence;
	UINT16 timer;
	UINT16 stopchan;
	UINT8 loop;
	UINT32 offset;
	UINT8 bank;
	bool is_playing;
	bool is_paused;
};
struct ymz_sqc
{
	UINT8 sqc;
	UINT8 loop;
	UINT32 offset;
	bool is_playing;
	bool is_waiting;
};

static ymz_channel m_channels[16];
static ymz_sequence m_sequences[8];
static ymz_sqc m_sqcs[8];

static INT32 ymz_initted = 0;

//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

static void init_common()
{
	for (int i = 0; i < 16; i++)
	{
		m_channels[i].is_playing = false;
		mpeg_decoder[i] = new mpeg_audio(m_rom, mpeg_audio::AMM, false, 0);
	}
	for (int i = 0; i < 8; i++)
		m_sequences[i].is_playing = false;
	for (int i = 0; i < 8; i++)
		m_sqcs[i].is_playing = false;

	ymz_initted = 1;
}

void ymz770_exit()
{
	if (ymz_initted) {
		ymz_initted = 0;
		stream.exit();

		for (int i = 0; i < 16; i++)
		{
			delete mpeg_decoder[i];
		}
	}
}

void ymz770_set_buffered(INT32 (*pCPUCyclesCB)(), INT32 nCPUMhz)
{
	stream.set_buffered(pCPUCyclesCB, nCPUMhz);
}

void ymz770_init(UINT8 *rom, INT32 rom_length)
{
	// create the stream
	stream.init(16000, nBurnSoundRate, 2, 0, ymz770_stream_update);

	m_rom = rom;
	m_rom_size = rom_length;
	m_rom_mask = rom_length - 1;

	// setup function pointers
	sequencer = ymz770_sequencer;
	internal_reg_write = ymz770_internal_reg_write;
	get_phrase_offs = ymz770_get_phrase_offs;
	get_seq_offs = ymz770_get_seq_offs;

	init_common();
}

void ymz774_init(UINT8 *rom, INT32 rom_length)
{
	// create the stream
	stream.init(44100, nBurnSoundRate, 2, 0, ymz770_stream_update);

	m_rom = rom;
	m_rom_size = rom_length;
	m_rom_mask = rom_length - 1;

	// setup function pointers
	sequencer = ymz774_sequencer;
	internal_reg_write = ymz774_internal_reg_write;
	get_phrase_offs = ymz774_get_phrase_offs;
	get_seq_offs = ymz774_get_seq_offs;

	init_common();

	// calculate volume increments, fixed point values, fractions of 0x20000
	for (INT32 i = 0; i < 256; i++)
	{
		if (i < 0x20)
			volinc[i] = i;
		else
			volinc[i] = (0x20 | (i & 0x1f)) << ((i >> 5) - 1);
	}
}



//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void ymz770_reset()
{
	memset(&m_channels, 0, sizeof(m_channels));
	for (int i = 0; i < 16; i++)
	{
		m_channels[i].phrase = 0;
		m_channels[i].pan = 64;
		m_channels[i].pan_delay = 0;
		m_channels[i].pan1 = 64;
		m_channels[i].pan1_delay = 0;
		m_channels[i].volume = 0;
		m_channels[i].volume_target = 0;
		m_channels[i].volume_delay = 0;
		m_channels[i].volume2 = 0;
		m_channels[i].loop = 0;
		m_channels[i].is_playing = false;
		m_channels[i].is_paused = false;
		m_channels[i].output_remaining = 0;

		mpeg_decoder[i]->clear();
	}
	memset(&m_sequences, 0, sizeof(m_sequences));
	for (int i = 0; i < 8; i++)
	{
		m_sequences[i].delay = 0;
		m_sequences[i].sequence = 0;
		m_sequences[i].timer = 0;
		m_sequences[i].stopchan = 0;
		m_sequences[i].loop = 0;
		m_sequences[i].bank = 0;
		m_sequences[i].is_playing = false;
		m_sequences[i].is_paused = false;
	}
	memset(&m_sqcs, 0, sizeof(m_sqcs));
	for (int i = 0; i < 8; i++)
	{
		m_sqcs[i].is_playing = false;
		m_sqcs[i].loop = 0;
	}
}

void ymz770_update(INT16 *output, INT32 samples_len)
{
	if (samples_len != nBurnSoundLen) {
		bprintf(0, _T("ymz77x_update(): once per frame, please!\n"));
		return;
	}

	stream.render(output, samples_len);
}

void ymz770_scan(INT32 nAction, INT32 *pnMin)
{
	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(m_cur_reg);
		SCAN_VAR(m_mute);
		SCAN_VAR(m_doen);
		SCAN_VAR(m_vlma);
		SCAN_VAR(m_vlma1);
		SCAN_VAR(m_bsl);
		SCAN_VAR(m_cpl);
		SCAN_VAR(m_bank);
		SCAN_VAR(m_channels);
		SCAN_VAR(m_sequences);
		SCAN_VAR(m_sqcs);

		for (int i = 0; i < 16; i++) {
			mpeg_decoder[i]->scan(); // mpeg/amm2 decoder state
		}
	}
}


//-------------------------------------------------
//  sound_stream_update - handle update requests for
//  our sound stream
//-------------------------------------------------

static void ymz770_stream_update(INT16 **streams, INT32 samples)
{
	INT16 *outL = streams[0];
	INT16 *outR = streams[1];

	for (int i = 0; i < samples; i++)
	{
		sequencer();

		// process channels
		INT32 mixl = 0;
		INT32 mixr = 0;

		for (int ch = 0; ch < 16; ch++)
		{
			ymz_channel &channel = m_channels[ch];

			if (channel.output_remaining > 0)
			{
				// force finish current block
				INT32 smpl = ((INT32)channel.output_data[channel.output_ptr++] * (channel.volume >> 17)) >> 7;   // volume is linear, 0 - 128 (100%)
				smpl = (smpl * channel.volume2) >> 7;
				mixr += (smpl * channel.pan) >> 7;  // pan seems linear, 0 - 128, where 0 = 100% left, 128 = 100% right, 64 = 50% left 50% right
				mixl += (smpl * (128 - channel.pan)) >> 7;
				channel.output_remaining--;

				if (channel.output_remaining == 0 && !channel.is_playing)
					mpeg_decoder[ch]->clear();
			}

			if (channel.output_remaining == 0 && channel.is_playing && !channel.is_paused)
			{
retry:
				if (channel.last_block)
				{
					if (channel.loop)
					{
						if (channel.loop != 255)
							--channel.loop;
						// loop sample
						int phrase = channel.phrase;
						channel.atbl = m_rom[(4*phrase)+0] >> 4 & 7;
						channel.pptr = 8 * get_phrase_offs(phrase);
					}
					else
					{
						channel.is_playing = false;
						channel.output_remaining = 0;
						mpeg_decoder[ch]->clear();
					}
				}

				if (channel.is_playing)
				{
					// next block
					int sample_rate, channel_count;
					if (!mpeg_decoder[ch]->decode_buffer(channel.pptr, m_rom_size*8, channel.output_data, channel.output_remaining, sample_rate, channel_count) || channel.output_remaining == 0)
					{
						channel.is_playing = !channel.last_block; // detect infinite retry loop
						channel.last_block = true;
						channel.output_remaining = 0;
						goto retry;
					}

					channel.last_block = channel.output_remaining < 1152;
					channel.output_ptr = 0;
				}
			}
		}

		mixr *= m_vlma; // main volume is linear, 0 - 255, where 128 = 100%
		mixl *= m_vlma;
		mixr >>= 7 - m_bsl;
		mixl >>= 7 - m_bsl;
		// Clip limiter: 0 - off, 1 - 6.02 dB (100%), 2 - 4.86 dB (87.5%), 3 - 3.52 dB (75%)
		const INT32 ClipMax3 = 32768 * 75 / 100;
		const INT32 ClipMax2 = 32768 * 875 / 1000;
		switch (m_cpl)
		{
		case 3:
			mixl = (mixl > ClipMax3) ? ClipMax3 : (mixl < -ClipMax3) ? -ClipMax3 : mixl;
			mixr = (mixr > ClipMax3) ? ClipMax3 : (mixr < -ClipMax3) ? -ClipMax3 : mixr;
			break;
		case 2:
			mixl = (mixl > ClipMax2) ? ClipMax2 : (mixl < -ClipMax2) ? -ClipMax2 : mixl;
			mixr = (mixr > ClipMax2) ? ClipMax2 : (mixr < -ClipMax2) ? -ClipMax2 : mixr;
			break;
		case 1:
			mixl = (mixl > 32767) ? 32767 : (mixl < -32768) ? -32768 : mixl;
			mixr = (mixr > 32767) ? 32767 : (mixr < -32768) ? -32768 : mixr;
			break;
		}
		if (m_mute)
			mixr = mixl = 0;

		outL[i] = mixl;
		outR[i] = mixr;
	}
}

static void ymz770_sequencer()
{
	for (int i = 0; i < 8; i++)
	{
		ymz_sequence &sequence = m_sequences[i];

		if (sequence.is_playing)
		{
			if (sequence.delay > 0)
				sequence.delay--;
			else
			{
				int reg = get_rom_byte(sequence.offset++);
				UINT8 data = get_rom_byte(sequence.offset++);
				switch (reg)
				{
				case 0x0f:
					for (int ch = 0; ch < 8; ch++) // might be wrong, ie not needed in case of loop
						if (sequence.stopchan & (1 << ch))
							m_channels[ch].is_playing = false;
					if (sequence.loop)
						sequence.offset = get_seq_offs(sequence.sequence); // loop sequence
					else
						sequence.is_playing = false;
					break;
				case 0x0e:
					sequence.delay += sequence.timer * 32 + 32 - 1;
					break;
				default:
					// Let's assume starting a sequence takes an extra cycle - this fixes mmpork's title
					// without breaking ddpdfk music loop-points (character select & level 1 music)
					sequence.delay = -1;
					internal_reg_write(reg, data);
					break;
				}
			}
		}
	}
}

//-------------------------------------------------
//  write - write to the chip's registers
//-------------------------------------------------

void ymz770_write(INT32 offset, UINT8 data)
{
	if (offset & 1)
	{
		stream.update();
		internal_reg_write(m_cur_reg, data);
	}
	else
		m_cur_reg = data;
}


static void ymz770_internal_reg_write(UINT8 reg, UINT8 data)
{
	// global registers
	if (reg < 0x40)
	{
		switch (reg)
		{
			case 0x00:
				m_mute = data & 1;
				m_doen = data >> 1 & 1;
				break;

			case 0x01:
				m_vlma = data;
				break;

			case 0x02:
				m_bsl = data & 7;
				m_cpl = data >> 4 & 7;
				break;

			// unused
			default:
				logerror("unimplemented write %02X %02X\n", reg, data);
				break;
		}
	}

	// playback registers
	else if (reg < 0x60)
	{
		int ch = reg >> 2 & 0x07;

		switch (reg & 0x03)
		{
			case 0:
				m_channels[ch].phrase = data;
				break;

			case 1:
				m_channels[ch].volume2 = data;
				m_channels[ch].volume = 128 << 17;
				break;

			case 2:
				m_channels[ch].pan = data << 3;
				break;

			case 3:
				if ((data & 6) == 2 || ((data & 6) == 6 && !m_channels[ch].is_playing)) // both KON bits is 1 = "Keep Playing", do not restart channel in this case
				{
					UINT8 phrase = m_channels[ch].phrase;
					m_channels[ch].atbl = m_rom[(4*phrase)+0] >> 4 & 7;
					m_channels[ch].pptr = 8 * get_phrase_offs(phrase);
					m_channels[ch].last_block = false;

					m_channels[ch].is_playing = true;
				}
				else if ((data & 6) == 0) {
					m_channels[ch].is_playing = false;
				}

				m_channels[ch].loop = (data & 1) ? 255 : 0;
				break;
		}
	}

	// sequencer registers
	else if (reg >= 0x80)
	{
		int ch = reg >> 4 & 0x07;

		switch (reg & 0x0f)
		{
			case 0: // SQSN
				m_sequences[ch].sequence = data;
				break;
			case 1: // SQON SQLP
				if ((data & 6) == 2 || ((data & 6) == 6 && !m_sequences[ch].is_playing)) // both KON bits 1 is "Keep Playing"
				{
					m_sequences[ch].offset = get_seq_offs(m_sequences[ch].sequence);
					m_sequences[ch].delay = 0;
					m_sequences[ch].is_playing = true;
				}
				else if ((data & 6) == 0 && m_sequences[ch].is_playing)
				{
					m_sequences[ch].is_playing = false;
					for (int i = 0; i < 8; i++)
						if (m_sequences[ch].stopchan & (1 << i))
							m_channels[i].is_playing = false;
				}

				m_sequences[ch].loop = data & 1;
				break;
			case 2: // TMRH
				m_sequences[ch].timer = (m_sequences[ch].timer & 0x00ff) | (data << 8);
				break;
			case 3: // TMRL
				m_sequences[ch].timer = (m_sequences[ch].timer & 0xff00) | data;
				break;
			case 6: // SQOF
				m_sequences[ch].stopchan = data;
				break;
			//case 4: // TGST "ON trigger playback channel selection bitmask"
			//case 5: // TGEN "OFF trigger playback channel selection bitmask"
			//case 7: // YMZ770: unused, but CV1K games write 1 here, must be game bugs or YMZ770C datasheet is wrong and have messed 7 and 8 registers ? ; YMZ771: SQOF_SSG
			//case 8: // YMZ770: docs said "set to 01h" but CV1K games never write it, see above. ; YMZ771: TEMPO (wait timer speed control)
			default:
				if (data)
					logerror("unimplemented write %02X %02X\n", reg, data);
				break;
		}
	}
	else
		logerror("unimplemented write %02X %02X\n", reg, data);
}


//-------------------------------------------------
//  ymz774_device
//-------------------------------------------------

UINT8 ymz774_read(INT32 offset)
{
	if (offset & 1)
	{
		if (m_cur_reg == 0xe3 || m_cur_reg == 0xe4)
		{
			stream.update();
			UINT8 res = 0;
			int bank = (m_cur_reg == 0xe3) ? 8 : 0;
			for (int i = 0; i < 8; i++)
				if (m_channels[i + bank].is_playing)
					res |= 1 << i;
			return res;
		}
	}
	logerror("unimplemented read %02X\n", m_cur_reg);
	return 0;
}

static void ymz774_internal_reg_write(UINT8 reg, UINT8 data)
{
	// playback registers
	if (reg < 0x10)  // phrase num H and L
	{
		int ch = ((reg >> 1) & 7) + m_bank * 8;
		if (reg & 1)
			m_channels[ch].phrase = (m_channels[ch].phrase & 0xff00) | data;
		else
			m_channels[ch].phrase = (m_channels[ch].phrase & 0x00ff) | ((data & 7) << 8);
	}
	else if (reg < 0x60)
	{
		int ch = (reg & 7) + m_bank * 8;
		switch (reg & 0xf8)
		{
		case 0x10: // Volume 1
			m_channels[ch].volume_target = data;
			break;
		case 0x18: // Volume 1 delayed transition
			m_channels[ch].volume_delay = data;
			break;
		case 0x20: // Volume 2
			m_channels[ch].volume2 = data;
			break;
		case 0x28: // Pan L/R
			m_channels[ch].pan = data;
			break;
		case 0x30: // Pan L/R delayed transition
			if (data) logerror("unimplemented write %02X %02X\n", reg, data);
			m_channels[ch].pan_delay = data;
			break;
		case 0x38: // Pan T/B
			m_channels[ch].pan1 = data;
			break;
		case 0x40: // Pan T/B delayed transition
			if (data) logerror("unimplemented write %02X %02X\n", reg, data);
			m_channels[ch].pan1_delay = data;
			break;
		case 0x48: // Loop
			m_channels[ch].loop = data;
			break;
		case 0x50: // Start / Stop
			if (data)
			{
				int phrase = m_channels[ch].phrase;
				m_channels[ch].atbl = m_rom[(4 * phrase) + 0] >> 4 & 7;
				m_channels[ch].pptr = 8 * get_phrase_offs(phrase);
				m_channels[ch].last_block = false;

				m_channels[ch].is_playing = true;
				m_channels[ch].is_paused = false; // checkme, might be not needed
			}
			else
				m_channels[ch].is_playing = false;
			break;
		case 0x58: // Pause / Resume
			m_channels[ch].is_paused = data ? true : false;
			if (data) logerror("CHECKME: CHAN pause/resume %02X %02X\n", reg, data);
			break;
		}
	}
	else if (reg < 0xd0)
	{
		if (m_bank == 0)
		{
			int sq = reg & 7;
			switch (reg & 0xf8)
			{
			case 0x60: // sequence num H and L
			case 0x68:
				sq = (reg >> 1) & 7;
				if (reg & 1)
					m_sequences[sq].sequence = (m_sequences[sq].sequence & 0xff00) | data;
				else
					m_sequences[sq].sequence = (m_sequences[sq].sequence & 0x00ff) | ((data & 0x07) << 8);
				break;
			case 0x70: // Start / Stop
				if (data)
				{
					//logerror("SEQ %d start (%s)\n", sq, m_sequences[sq].is_playing ? "playing":"stopped");
					m_sequences[sq].offset = get_seq_offs(m_sequences[sq].sequence);
					m_sequences[sq].delay = 0;
					m_sequences[sq].is_playing = true;
					m_sequences[sq].is_paused = false; // checkme, might be not needed
				}
				else
				{
					//logerror("SEQ %d stop (%s)\n", sq, m_sequences[sq].is_playing ? "playing" : "stopped");
					if (m_sequences[sq].is_playing)
						for (int ch = 0; ch < 16; ch++)
							if (m_sequences[sq].stopchan & (1 << ch))
								m_channels[ch].is_playing = false;
					m_sequences[sq].is_playing = false;
				}
				break;
			case 0x78: // Pause / Resume
				m_sequences[sq].is_paused = data ? true : false;
				if (data) logerror("CHECKME: SEQ pause/resume %02X %02X\n", reg, data);
				break;
			case 0x80: // Loop count, 0 = off, 255 - infinite
				m_sequences[sq].loop = data;
				break;
			case 0x88: // timer H and L
			case 0x90:
				sq = (reg - 0x88) >> 1;
				if (reg & 1)
					m_sequences[sq].timer = (m_sequences[sq].timer & 0xff00) | data;
				else
					m_sequences[sq].timer = (m_sequences[sq].timer & 0x00ff) | (data << 8);
				break;
			case 0x98: // Off trigger, bit4 = on/off, bits0-3 channel (end sequence when channel playback ends)
				if (data) logerror("SEQ Off trigger unimplemented %02X %02X\n", reg, data);
				break;
			case 0xa0: // stop channel mask H and L (when sequence stopped)
			case 0xa8:
				sq = (reg >> 1) & 7;
				if (reg & 1)
					m_sequences[sq].stopchan = (m_sequences[sq].stopchan & 0xff00) | data;
				else
					m_sequences[sq].stopchan = (m_sequences[sq].stopchan & 0x00ff) | (data << 8);
				break;
			case 0xb0: // SQC number
				m_sqcs[sq].sqc = data;
				break;
			case 0xb8: // SQC start / stop
				if (data)
				{
					//logerror("SQC %d start (%s)\n", sq, m_sqcs[sq].is_playing ? "playing" : "stopped");
					m_sqcs[sq].offset = ymz774_get_sqc_offs(m_sqcs[sq].sqc);
					m_sqcs[sq].is_playing = true;
					m_sqcs[sq].is_waiting = false;
				}
				else
				{
					//logerror("SQC %d stop (%s)\n", sq, m_sqcs[sq].is_playing ? "playing" : "stopped");
					m_sqcs[sq].is_playing = false;
					// stop SEQ too, and stop channels
					if (m_sequences[sq].is_playing)
						for (int ch = 0; ch < 16; ch++)
							if (m_sequences[sq].stopchan & (1 << ch))
								m_channels[ch].is_playing = false;
					m_sequences[sq].is_playing = false;
				}
				break;
			case 0xc0: // SQC loop (255 = infinite)
				m_sqcs[sq].loop = data;
				break;
			default:
				logerror("unimplemented write %02X %02X\n", reg, data);
				break;
			}
		}
		// else bank1 - Equalizer control
	}
	// global registers
	else
	{
		switch (reg) {
		case 0xd0: // Total Volume L0/R0
			m_vlma = data;
			break;
		case 0xd1: // Total Volume L1/R1
			m_vlma1 = data;
			break;
		case 0xd2: // Clip limit
			m_cpl = data;
			break;
		//case 0xd3: // Digital/PWM output
		//case 0xd4: // Digital audio IF/IS L0/R0
		//case 0xd5: // Digital audio IF/IS L1/R1
		//case 0xd8: // GPIO A
		//case 0xdd: // GPIO B
		//case 0xde: // GPIO C
		case 0xf0:
			m_bank = data & 1;
			if (data > 1) logerror("Set bank %02X!\n", data);
			break;
		default:
			logerror("unimplemented write %02X %02X\n", reg, data);
			break;
		}
	}
}

static void ymz774_sequencer()
{
	for (int i = 0; i < 16; i++)
	{
		ymz_channel &chan = m_channels[i];

		if (chan.is_playing && !chan.is_paused && (chan.volume >> 17) != chan.volume_target)
		{
			if (chan.volume_delay)
			{
				if ((chan.volume >> 17) < chan.volume_target)
					chan.volume += volinc[chan.volume_delay];
				else
					chan.volume -= volinc[chan.volume_delay];
			}
			else
				chan.volume = chan.volume_target << 17;
		}
	}

	for (int i = 0; i < 8; i++)
	{
		ymz_sqc &sqc = m_sqcs[i];
		ymz_sequence &sequence = m_sequences[i];

		if (sqc.is_playing && !sqc.is_waiting)
		{
			// SQC consists of 4 byte records: SEQ num H, SEQ num L, SEQ Loop count, End flag (0xff)
			sequence.sequence = ((get_rom_byte(sqc.offset) << 8) | get_rom_byte(sqc.offset + 1)) & 0x7ff;
			sqc.offset += 2;
			sequence.loop = get_rom_byte(sqc.offset++);
			sequence.offset = get_seq_offs(sequence.sequence);
			sequence.delay = 0;
			sequence.is_playing = true;
			sequence.is_paused = false; // checkme, might be not needed
			sqc.is_waiting = true;
			if (get_rom_byte(sqc.offset++) == 0xff)
			{
				if (sqc.loop)
				{
					if (sqc.loop != 255)
						--sqc.loop;
					sqc.offset = ymz774_get_sqc_offs(sqc.sqc);
				}
				else
					sqc.is_playing = false;
			}
		}

		if (sequence.is_playing && !sequence.is_paused)
		{
			if (sequence.delay > 0)
				--sequence.delay;
			else
			{
				int reg = get_rom_byte(sequence.offset++);
				UINT8 data = get_rom_byte(sequence.offset++);
				switch (reg)
				{
				case 0xff: // end
					for (int ch = 0; ch < 16; ch++) // might be wrong, ie not needed in case of loop
						if (sequence.stopchan & (1 << ch))
							m_channels[ch].is_playing = false;
					if (sequence.loop)
					{
						if (sequence.loop != 255)
							--sequence.loop;
						sequence.offset = get_seq_offs(sequence.sequence);
					}
					else
					{
						sequence.is_playing = false;
						sqc.is_waiting = false;
					}
					break;
				case 0xfe: // timer delay
					sequence.delay = sequence.timer * 32 + 32 - 1;
					break;
				case 0xf0:
					sequence.bank = data & 1;
					break;
				default:
				{
					UINT8 temp = m_bank;
					m_bank = sequence.bank;
					if (m_bank == 0 && reg >= 0x60 && reg < 0xb0) // if we hit SEQ registers need to add this sequence offset
					{
						int sqn = i;
						if (reg < 0x70 || (reg >= 0x88 && reg < 0x98) || reg >= 0xa0)
							sqn = i * 2;
						internal_reg_write(reg + sqn, data);
					}
					else
						internal_reg_write(reg, data);
					m_bank = temp;
				}
					break;
				}
			}
		}
	}
}
