/***************************************************************************

    qs1000.c

    QS1000 device emulator.
	by Phil Bennett, with parts by iq_132 & dink
****************************************************************************

    The QS1000 is a 32-voice wavetable synthesizer, believed to be based on
    the OPTi 82C941. It contains an 8051 core, 256b of RAM and an (undumped)
    internal program ROM. The internal ROM can be bypassed in favour of an
    external ROM. Commands are issued to the chip via the 8051 serial port.

    The QS1000 can access 24Mb of sample ROM. To reduce demand on the CPU,
    instrument parameters such as playback rate, envelope and filter values
    are encoded in ROM and directly accessed by the wavetable engine.
    There are table entries for every note of every instrument.

    Registers
    =========

    [200] = Key on/off
            0 = Key on
            1 = ?
            2 = key off
    [201] = Address byte 0 (LSB)
    [202] = Address byte 1
    [203] = Address byte 2
    [204] = Pitch
    [205] = Pitch high byte? (Usually 0)
    [206] = Left volume
    [207] = Right volume
    [208] = Volume
    [209] = ?
    [20a] = ?
    [20b] = ?
    [20c] = ?
    [20d] = Velocity
    [20e] = Channel select
    [20f] = Modulation
    [210] = Modulation
    [211] = 0 - Select global registers?
            3 - Select channel registers?

    Velocity register values for MIDI range 0-127:

    01 01 01 01 01 01 01 02 02 03 03 04 04 05 05 06
    06 07 07 08 08 09 09 0A 0A 0B 0B 0C 0C 0D 0D 0E
    0E 0F 10 11 11 12 13 14 14 15 16 17 17 18 19 1A
    1A 1B 1C 1D 1D 1E 1F 20 20 21 22 23 23 24 25 26
    26 27 28 29 29 2A 2B 2C 2C 2D 2E 2F 2F 30 31 32
    35 38 3B 3E 41 44 47 4A 4D 50 4F 51 52 53 54 56
    57 58 59 5B 5C 5D 5E 60 61 62 63 65 66 67 6A 6B
    6C 6E 6F 70 71 73 74 75 76 78 79 7A 7B 7D 7E 7F

    (TODO: Other register values)

    This is the sequence of register writes used to play the Iron Fortress credit sound:

    [211] 0     Select global registers?
    [200] 1     ?
    [203] d6    Address byte 2
    [202] a9    Address byte 1
    [201] 1     Address byte 0
    [204] 80    Pitch
    [205] 0     ?
    [206] 80    Left volume
    [207] 80    Right volume
    [208] b3    Volume
    [209] 0     ?
    [20a] ff    ?
    [20b] 0     ?
    [20c] 0     ?
    [20d] 78    Velocity
    [211] 3     Select channel registers
    [20e] 0     Select channel
    [200] 0     Key on


    Sound Headers
    =============

    The address registers point to a 6 byte entry in the sound ROM:

    [019be0]
    097b 397f 1510
    ^    ^    ^
    |    |    |
    |    |    +----- Sound descriptor pointer
    |    +---------- ?
    +--------------- Playback frequency (fixed point value representing 24MHz clock periods)

    This in turn points to a 24 byte descriptor:

    [1510]:
    0 4502D 4508E 45F91 D0 7F 0F 2A 1F 90 00 FF
    ^ ^     ^     ^     ^  ^  ^  ^  ^  ^  ^  ^
    | |     |     |     |  |  |  |  |  |  |  |
    | |     |     |     |  |  |  |  |  |  |  +-- ?
    | |     |     |     |  |  |  |  |  |  +----- ?
    | |     |     |     |  |  |  |  |  +-------- ?
    | |     |     |     |  |  |  |  +----------- ?
    | |     |     |     |  |  |  +-------------- ?
    | |     |     |     |  |  +----------------- Bit 7: Format (0:PCM 1:ADPCM)
    | |     |     |     |  +-------------------- ?
    | |     |     |     +----------------------- ?
    | |     |     +----------------------------- Loop end address
    | |     +----------------------------------- Loop start address
    | +----------------------------------------- Start address
    +------------------------------------------- Address most-significant nibble (shared with loop addresses)

    * The unknown parameters are most likely envelope and filter parameters.
    * Is there a loop flag or do sounds loop indefinitely until stopped?


    TODO:
    * Looping is currently disabled
    * Figure out unknown sound header parameters
    * Figure out and implement envelopes and filters
    * Pitch bending
    * Dump the internal ROM

***************************************************************************/
#include "burnint.h"
#include "mcs51.h"
#include <math.h>

#define QS1000_CHANNELS       32
#define QS1000_ADDRESS_MASK   0x00ffffff

enum
{
	QS1000_KEYON   = 1,
	QS1000_PLAYING = 2,
	QS1000_ADPCM   = 4
};

// for resampling
static INT32 qs1000_rate;
static UINT32 nSampleSize;
static INT32 nFractionalPosition;
static INT32 nPosition;
static INT16 *mixer_buffer_left;
static INT16 *mixer_buffer_right;
static INT32 (*pCPUTotalCycles)() = NULL;
static UINT32  nDACCPUMHZ = 0;

static double qs1000_mastervol;

// oki adpcm pseudo-objecty
static const INT8 s_index_shift[8] = { -1, -1, -1, -1, 2, 4, 6, 8 };
static int s_diff_lookup[49*16];

struct okiadpcm_ch
{
	INT32 m_signal;
	INT32 m_step;

	void okiadpcmreset()
	{
		m_signal = -2;
		m_step = 0;
	}

	INT16 okiadpcmclock(UINT8 nibble)
	{
		// update the signal
		m_signal += s_diff_lookup[m_step * 16 + (nibble & 15)];

		// clamp to the maximum
		if (m_signal > 2047)
			m_signal = 2047;
		else if (m_signal < -2048)
			m_signal = -2048;

		// adjust the step size and clamp
		m_step += s_index_shift[nibble & 7];
		if (m_step > 48)
			m_step = 48;
		else if (m_step < 0)
			m_step = 0;

		// return the signal
		return m_signal;
	}
};

static void okiadpcm_compute_tables()
{
	// nibble to bit map
	static const INT8 nbl2bit[16][4] =
	{
		{ 1, 0, 0, 0}, { 1, 0, 0, 1}, { 1, 0, 1, 0}, { 1, 0, 1, 1},
		{ 1, 1, 0, 0}, { 1, 1, 0, 1}, { 1, 1, 1, 0}, { 1, 1, 1, 1},
		{-1, 0, 0, 0}, {-1, 0, 0, 1}, {-1, 0, 1, 0}, {-1, 0, 1, 1},
		{-1, 1, 0, 0}, {-1, 1, 0, 1}, {-1, 1, 1, 0}, {-1, 1, 1, 1}
	};

	// loop over all possible steps
	for (int step = 0; step <= 48; step++)
	{
		// compute the step value
		int stepval = floor(16.0 * pow(11.0 / 10.0, (double)step));

		// loop over all nibbles and compute the difference
		for (int nib = 0; nib < 16; nib++)
		{
			s_diff_lookup[step*16 + nib] = nbl2bit[nib][0] *
				(stepval   * nbl2bit[nib][1] +
					stepval/2 * nbl2bit[nib][2] +
					stepval/4 * nbl2bit[nib][3] +
					stepval/8);
		}
	}
}

struct qs1000_channel
{
	UINT32          m_acc;
	INT32           m_adpcm_signal;
	UINT32          m_start;
	UINT32          m_addr;
	UINT32          m_adpcm_addr;
	UINT32          m_loop_start;
	UINT32          m_loop_end;
	UINT16          m_freq;
	UINT16          m_flags;

	UINT8           m_regs[16];
	okiadpcm_ch     m_adpcm;
};

static qs1000_channel m_channels[QS1000_CHANNELS];

static void (*qs1000_p1_out)(UINT8 data);
static void (*qs1000_p2_out)(UINT8 data);
static void (*qs1000_p3_out)(UINT8 data);
static UINT8 (*qs1000_p1_in)();
static UINT8 (*qs1000_p2_in)();
static UINT8 (*qs1000_p3_in)();

static UINT8 *sample_rom;
static UINT32 sample_len;

static UINT8 *banked_rom = NULL;

static UINT8 ram[0x100];
static UINT8 serial_data_in;
static UINT8 wave_regs[18];

static UINT8 read_byte(UINT32 address)
{
	if (address > sample_len) return 0;

	return sample_rom[address];
}

static void sound_stream_update(INT16 *outputL, INT16 *outputR, int samples)
{
	// Iterate over voices and accumulate sample data
	for (int ch = 0; ch < QS1000_CHANNELS; ch++)
	{
		qs1000_channel &chan = m_channels[ch];

		UINT8 lvol = chan.m_regs[6];
		UINT8 rvol = chan.m_regs[7];
		UINT8 vol  = chan.m_regs[8];

		if (chan.m_flags & QS1000_PLAYING)
		{
			if (chan.m_flags & QS1000_ADPCM)
			{
				for (int samp = 0; samp < samples; samp++)
				{
					if (chan.m_addr >= chan.m_loop_end)
					{
#if 0 // Looping disabled until envelopes work
						if (chan.m_flags & QS1000_KEYON)
						{
							chan.m_addr = chan.m_loop_start;
						}
						else
#endif
						{
							chan.m_flags &= ~QS1000_PLAYING;
							break;
						}
					}

					// Not too keen on this but it'll do for now
					while (chan.m_start + chan.m_adpcm_addr != chan.m_addr)
					{
						chan.m_adpcm_addr++;

						if (chan.m_start + chan.m_adpcm_addr >=  chan.m_loop_end) {
							chan.m_adpcm_addr = chan.m_loop_start - chan.m_start;
						}

						UINT8 data = read_byte(chan.m_start + (chan.m_adpcm_addr >> 1));
						UINT8 nibble = (chan.m_adpcm_addr & 1 ? data : data >> 4) & 0xf;
						chan.m_adpcm_signal = chan.m_adpcm.okiadpcmclock(nibble);
					}

					INT16 result = chan.m_adpcm_signal / 16;
					chan.m_acc += chan.m_freq;
					chan.m_addr = (chan.m_addr + (chan.m_acc >> 18)) & QS1000_ADDRESS_MASK;
					chan.m_acc &= ((1 << 18) - 1);

					outputL[samp] = BURN_SND_CLIP(outputL[samp] + ((result * 8 * lvol * vol) >> 12));
					outputR[samp] = BURN_SND_CLIP(outputR[samp] + ((result * 8 * rvol * vol) >> 12));
				}
			}
			else
			{
				for (int samp = 0; samp < samples; samp++)
				{
					if (chan.m_addr >= chan.m_loop_end)
					{
#if 0 // Looping disabled until envelopes work
						if (chan.m_flags & QS1000_KEYON)
						{
							chan.m_addr = chan.m_loop_start;
						}
						else
#endif
						{
							chan.m_flags &= ~QS1000_PLAYING;
							break;
						}
					}

					INT8 result = read_byte(chan.m_addr) - 128;

					chan.m_acc += chan.m_freq;
					chan.m_addr = (chan.m_addr + (chan.m_acc >> 18)) & QS1000_ADDRESS_MASK;
					chan.m_acc &= ((1 << 18) - 1);

					outputL[samp] = BURN_SND_CLIP(outputL[samp] + ((result * 3 * lvol * vol) >> 12));
					outputR[samp] = BURN_SND_CLIP(outputR[samp] + ((result * 3 * rvol * vol) >> 12));
				}
			}
		}
	}
}

// Streambuffer handling
static INT32 SyncInternal()
{
	return (INT32)(float)(nBurnSoundLen * (pCPUTotalCycles() / (nDACCPUMHZ / (nBurnFPS / 100.0000))));
}

static void UpdateStream(INT32 samples_len)
{
    if (samples_len > nBurnSoundLen) samples_len = nBurnSoundLen;

	INT32 nSamplesNeeded = ((((((qs1000_rate * 1000) / nBurnFPS) * samples_len) / nBurnSoundLen)) / 10) + 1;
	if (nBurnSoundRate < 44100) nSamplesNeeded += 2; // so we don't end up with negative nPosition below

    nSamplesNeeded -= nPosition;
    if (nSamplesNeeded <= 0) return;

	INT16   *lmix, *rmix;

	lmix = mixer_buffer_left  + 5 + nPosition;
	rmix = mixer_buffer_right + 5 + nPosition;

//	bprintf(0, _T("qs1000_sync: %d samples    frame %d\n"), nSamplesNeeded, nCurrentFrame);
	memset(lmix, 0, nSamplesNeeded * 2);
	memset(rmix, 0, nSamplesNeeded * 2);

	sound_stream_update(lmix, rmix, nSamplesNeeded);

    nPosition += nSamplesNeeded;
}


void qs1000_update(INT16 *outputs, INT32 samples_len)
{
	if (samples_len != nBurnSoundLen) {
		bprintf(0, _T("qs1000_update(): once per frame, please!\n"));
		return;
	}

	BurnSoundClear();

	INT32 nSamplesNeeded = ((((((qs1000_rate * 1000) / nBurnFPS) * samples_len) / nBurnSoundLen)) / 10) + 1;
	if (nBurnSoundRate < 44100) nSamplesNeeded += 2; // so we don't end up with negative nPosition below

	UpdateStream(nBurnSoundLen);

	INT16 *pBufL = mixer_buffer_left  + 5;
	INT16 *pBufR = mixer_buffer_right + 5;

	for (INT32 i = (nFractionalPosition & 0xFFFF0000) >> 15; i < (samples_len << 1); i += 2, nFractionalPosition += nSampleSize) {
		INT32 nLeftSample[4] = {0, 0, 0, 0};
		INT32 nRightSample[4] = {0, 0, 0, 0};
		INT32 nTotalLeftSample, nTotalRightSample;

		nLeftSample[0] += (INT32)(pBufL[(nFractionalPosition >> 16) - 3]);
		nLeftSample[1] += (INT32)(pBufL[(nFractionalPosition >> 16) - 2]);
		nLeftSample[2] += (INT32)(pBufL[(nFractionalPosition >> 16) - 1]);
		nLeftSample[3] += (INT32)(pBufL[(nFractionalPosition >> 16) - 0]);

		nRightSample[0] += (INT32)(pBufR[(nFractionalPosition >> 16) - 3]);
		nRightSample[1] += (INT32)(pBufR[(nFractionalPosition >> 16) - 2]);
		nRightSample[2] += (INT32)(pBufR[(nFractionalPosition >> 16) - 1]);
		nRightSample[3] += (INT32)(pBufR[(nFractionalPosition >> 16) - 0]);

		nTotalLeftSample  = INTERPOLATE4PS_16BIT((nFractionalPosition >> 4) & 0x0fff, nLeftSample[0], nLeftSample[1], nLeftSample[2], nLeftSample[3]);
		nTotalRightSample = INTERPOLATE4PS_16BIT((nFractionalPosition >> 4) & 0x0fff, nRightSample[0], nRightSample[1], nRightSample[2], nRightSample[3]);

		nTotalLeftSample  = BURN_SND_CLIP(nTotalLeftSample  * qs1000_mastervol);
		nTotalRightSample = BURN_SND_CLIP(nTotalRightSample * qs1000_mastervol);

		outputs[i + 0] = BURN_SND_CLIP(outputs[i + 0] + nTotalLeftSample);
		outputs[i + 1] = BURN_SND_CLIP(outputs[i + 1] + nTotalRightSample);
	}

	if (samples_len >= nBurnSoundLen) {
		INT32 nExtraSamples = nSamplesNeeded - (nFractionalPosition >> 16);

		for (INT32 i = -4; i < nExtraSamples; i++) {
			pBufL[i] = pBufL[(nFractionalPosition >> 16) + i];
			pBufR[i] = pBufR[(nFractionalPosition >> 16) + i];
		}

		nFractionalPosition &= 0xFFFF;

		nPosition = nExtraSamples;
	}
}


static void start_voice(int ch)
{
	UINT32 table_addr = (m_channels[ch].m_regs[0x01] << 16) | (m_channels[ch].m_regs[0x02] << 8) | m_channels[ch].m_regs[0x03];

	// Fetch the sound information
	UINT16 freq = (read_byte(table_addr + 0) << 8) | read_byte(table_addr + 1);
//	UINT16 word1 = (read_byte(table_addr + 2) << 8) | read_byte(table_addr + 3);
	UINT16 base = (read_byte(table_addr + 4) << 8) | read_byte(table_addr + 5);

	// See Raccoon World and Wyvern Wings nullptr sound
	if (freq == 0)
		return;

	// Fetch the sample pointers and flags
	UINT8 byte0 = read_byte(base);

	UINT32 start_addr;

	start_addr  = byte0 << 16;
	start_addr |= read_byte(base + 1) << 8;
	start_addr |= read_byte(base + 2) << 0;
	start_addr &= QS1000_ADDRESS_MASK;

	UINT32 loop_start;

	loop_start = (byte0 & 0xf0) << 16;
	loop_start |= read_byte(base + 3) << 12;
	loop_start |= read_byte(base + 4) << 4;
	loop_start |= read_byte(base + 5) >> 4;
	loop_start &= QS1000_ADDRESS_MASK;

	UINT32 loop_end;

	loop_end = (byte0 & 0xf0) << 16;
	loop_end |= (read_byte(base + 5) & 0xf) << 16;
	loop_end |= read_byte(base + 6) << 8;
	loop_end |= read_byte(base + 7);
	loop_end &= QS1000_ADDRESS_MASK;

	UINT8 byte8 = read_byte(base + 8);

#if 0
	{
		UINT8 byte9 = read_byte(base + 9);
		UINT8 byte10 = read_byte(base + 10);
		UINT8 byte11 = read_byte(base + 11);
		UINT8 byte12 = read_byte(base + 12);
		UINT8 byte13 = read_byte(base + 13);
		UINT8 byte14 = read_byte(base + 14);
		UINT8 byte15 = read_byte(base + 15);

		bprintf(0, _T("[%.6x] Sample Start:%.6x  Loop Start:%.6x  Loop End:%.6x  Params: %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x\n"), base, start_addr, loop_start, loop_end, byte8, byte9, byte10, byte11, byte12, byte13, byte14, byte15);
	}
#endif

	m_channels[ch].m_acc = 0;
	m_channels[ch].m_start = start_addr;
	m_channels[ch].m_addr = start_addr;
	m_channels[ch].m_loop_start = loop_start;
	m_channels[ch].m_loop_end = loop_end;
	m_channels[ch].m_freq = freq;
	m_channels[ch].m_flags = QS1000_PLAYING | QS1000_KEYON;

	if (byte8 & 0x08)
	{
		m_channels[ch].m_adpcm.okiadpcmreset();
		m_channels[ch].m_adpcm_addr = -1;
		m_channels[ch].m_flags |= QS1000_ADPCM;
	}
}

void qs1000_wave_w(INT32 offset, UINT8 data)
{
	UpdateStream(SyncInternal());
	//bprintf(0, _T("wave_w: %x   %x\n"), offset, data);
	switch (offset)
	{
		case 0x00:
		{
			int ch = wave_regs[0xe];

			if (data == 0)
			{
				// TODO
				for (int i = 0; i < 16; ++i)
					m_channels[ch].m_regs[i] = wave_regs[i];

				// Key on
				start_voice(ch);
			}
			if (data == 1)
			{
				// ?
			}
			else if (data == 2)
			{
				// Key off
				m_channels[ch].m_flags &= ~QS1000_KEYON;
			}
			break;
		}

		case 0x01:
		case 0x02:
		case 0x03:
		case 0x04:
		case 0x05:
		case 0x06:
		case 0x07:
		case 0x08:
		case 0x09:
		case 0x0a:
		case 0x0b:
		case 0x0c:
		case 0x0d:
		{
			if (wave_regs[0x11] == 3)
			{
				// Channel-specific write?
				m_channels[wave_regs[0xe]].m_regs[offset] = data;
			}
			else
			{
				// Global write?
				wave_regs[offset] = data;
			}
			break;
		}

		default:
			wave_regs[offset] = data;
	}
}

static UINT8 qs1000_read_port(INT32 port)
{
	if (port < 0x100) { // qs1000 internal ram
		return ram[port];
	}

	if (port >= 0x100 && port <= 0xffff) {
		if (banked_rom) return banked_rom[port];
	}

	switch (port)
	{
		case MCS51_PORT_P0:
			return 0xff;

		case MCS51_PORT_P1:
			return (qs1000_p1_in) ? qs1000_p1_in() : 0;

		case MCS51_PORT_P2:
			return (qs1000_p2_in) ? qs1000_p2_in() : 0;

		case MCS51_PORT_P3:
			return (qs1000_p3_in) ? qs1000_p3_in() : 0;
	}

	return 0;
}

static void qs1000_write_port(INT32 port, UINT8 data)
{
	if (port < 0x100) {
		ram[port] = data;
		return;
	}

	if (port >= 0x200 && port <= 0x211) {
		qs1000_wave_w(port & 0x1f, data);
		return;
	}

	switch (port)
	{
		case MCS51_PORT_P0: // not used
		return;

		case MCS51_PORT_P1:
			if (qs1000_p1_out) qs1000_p1_out(data);
		return;

		case MCS51_PORT_P2:
			if (qs1000_p2_out) qs1000_p2_out(data);
		return;

		case MCS51_PORT_P3:
			if (qs1000_p3_out) qs1000_p3_out(data);
		return;
	}
}

static UINT8 qs1000_rx_serial()
{
	return serial_data_in;
}

void qs1000_reset()
{
	mcs51Open(0);
	mcs51_reset();
	mcs51Close();

	memset (m_channels, 0, sizeof(m_channels));
	memset (ram, 0, sizeof(ram));
	memset (wave_regs, 0, sizeof(wave_regs));

	serial_data_in = 0;
}

void qs1000_init(UINT8 *program_rom, UINT8 *samples, INT32 samplesize)
{
	sample_rom = samples;
	sample_len = samplesize;

	i8052Init(0);
	mcs51Open(0);
	mcs51_set_program_data(program_rom);
	mcs51_set_write_handler(qs1000_write_port);
	mcs51_set_read_handler(qs1000_read_port);
	mcs51_set_serial_rx_callback(qs1000_rx_serial);
	mcs51Close();

	qs1000_p1_out = NULL;
	qs1000_p2_out = NULL;
	qs1000_p3_out = NULL;
	qs1000_p1_in = NULL;
	qs1000_p2_in = NULL;
	qs1000_p3_in = NULL;

	okiadpcm_compute_tables();

	qs1000_mastervol = 3.00;

	// resampling l/r buffers
	qs1000_rate = 24000000 / 32;
	mixer_buffer_left = (INT16*)BurnMalloc(2 * sizeof(INT16) * qs1000_rate);
	mixer_buffer_right = mixer_buffer_left + qs1000_rate;
	memset(mixer_buffer_left, 0, 2 * sizeof(INT16) * qs1000_rate);

	pCPUTotalCycles = mcs51TotalCycles;
	nDACCPUMHZ = 24000000 / 12;

	// for resampling
	nSampleSize = (UINT64)qs1000_rate * (1 << 16) / nBurnSoundRate;
	nFractionalPosition = 0;
	nPosition = 0;
}

void qs1000_exit()
{
	mcs51_exit();

	if (mixer_buffer_left) {
		BurnFree(mixer_buffer_left);
		mixer_buffer_left = mixer_buffer_right = NULL;
	}
}

void qs1000_scan(INT32 nAction, INT32 *)
{
	SCAN_VAR(m_channels);
	SCAN_VAR(ram);
	SCAN_VAR(serial_data_in);
	SCAN_VAR(wave_regs);

	if (nAction & ACB_WRITE) {
		nFractionalPosition = 0;
		nPosition = 0;
		memset(mixer_buffer_left, 0, 2 * sizeof(INT16) * qs1000_rate);
	}
}

void qs1000_set_volume(double volume)
{
	qs1000_mastervol = volume;
}

void qs1000_set_write_handler(INT32 port, void (*handler)(UINT8))
{
	switch (port)
	{
		case 1: qs1000_p1_out = handler; return;
		case 2: qs1000_p2_out = handler; return;
		case 3: qs1000_p3_out = handler; return;
	}
}

void qs1000_set_read_handler(INT32 port, UINT8 (*handler)())
{
	switch (port)
	{
		case 1: qs1000_p1_in = handler; return;
		case 2: qs1000_p2_in = handler; return;
		case 3: qs1000_p3_in = handler; return;
	}
}

void qs1000_set_bankedrom(UINT8 *rom)
{
	UpdateStream(SyncInternal());
	banked_rom = rom;
}

void qs1000_serial_in(UINT8 data)
{
	serial_data_in = data;
	
	INT32 active = mcs51GetActive();
	
	if (active != 0)
	{
		mcs51Close();
		mcs51Open(0);
	}

	mcs51_set_irq_line(MCS51_RX_LINE, CPU_IRQSTATUS_ACK);
	mcs51_set_irq_line(MCS51_RX_LINE, CPU_IRQSTATUS_NONE);
	
	if (active != 0)
	{
		mcs51Close();
		mcs51Open(active);
	}
}

void qs1000_set_irq(int state)
{
	INT32 active = mcs51GetActive();

	if (active != 0)
	{
		mcs51Close();
		mcs51Open(0);
	}
	
	mcs51_set_irq_line(MCS51_INT1_LINE, state ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
	
	if (active != 0)
	{
		mcs51Close();
		mcs51Open(active);
	}
}

