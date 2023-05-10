// license:BSD-3-Clause
// copyright-holders:Aaron Giles
/***************************************************************************

    Exidy 440 sound system

    Special thanks to Zonn Moore and Neil Bradley for letting me hack
    their Retrocade CVSD decoder into the sound system here.

***************************************************************************/

#include "burnint.h"
#include "exidy440_snd.h"
#include "stream.h"

#if defined (_MSC_VER)
#define _USE_MATH_DEFINES
#endif

#include <math.h>

#define SOUND_LOG       0
#define FADE_TO_ZERO    1


#define EXIDY440_AUDIO_CLOCK    (12979200 / 16)
#define EXIDY440_MC3418_CLOCK   (EXIDY440_AUDIO_CLOCK / 16)
#define EXIDY440_MC3417_CLOCK   (EXIDY440_AUDIO_CLOCK / 32)
#define clock()					(EXIDY440_AUDIO_CLOCK / 16)

/* internal caching */
#define MAX_CACHE_ENTRIES       1024                /* maximum separate samples we expect to ever see */
#define SAMPLE_BUFFER_LENGTH    1024                /* size of temporary decode buffer on the stack */

/* FIR digital filter parameters */
#define FIR_HISTORY_LENGTH      57                  /* number of FIR coefficients */

/* CVSD decoding parameters */
#define INTEGRATOR_LEAK_TC      (10e3 * 0.1e-6)
#define FILTER_DECAY_TC         ((18e3 + 3.3e3) * 0.33e-6)
#define FILTER_CHARGE_TC        (18e3 * 0.33e-6)
#define FILTER_MIN              0.0416
#define FILTER_MAX              1.0954
#define SAMPLE_GAIN             10000.0

/* constant channel parameters */
static const int channel_bits[4] =
{
	4, 4,                                   /* channels 0 and 1 are MC3418s, 4-bit CVSD */
	3, 3                                    /* channels 2 and 3 are MC3417s, 3-bit CVSD */
};

struct m6844_channel_data
{
	int active;
	int address;
	int counter;
	UINT8 control;
	int start_address;
	int start_counter;
};


/* channel_data structure holds info about each active sound channel */
struct sound_channel_data
{
	INT16 *base;
	int offset;
	int remaining;
};


/* sound_cache_entry structure contains info on each decoded sample */
struct sound_cache_entry
{
	struct sound_cache_entry *next;
	int address;
	int length;
	int bits;
	int frequency;
	INT16 data[1];
};

// internal state
static UINT8 m_sound_command;
static UINT8 m_sound_command_ack;

static UINT8 m_sound_banks[4];
static UINT8 m_sound_volume[0x10];
static INT32 *m_mixer_buffer_left;
static INT32 *m_mixer_buffer_right;
static INT32 m_sound_cache_length;
static sound_cache_entry *m_sound_cache;
static sound_cache_entry *m_sound_cache_end;
static sound_cache_entry *m_sound_cache_max;

/* 6844 description */
static m6844_channel_data m_m6844_channel[4];
static UINT8 m_m6844_priority;
static UINT8 m_m6844_interrupt;
static UINT8 m_m6844_chain;

/* sound interface parameters */
static Stream m_stream;
static sound_channel_data m_sound_channel[4];

/* debugging */
static FILE *m_debuglog;

/* channel frequency is configurable */
static int m_channel_frequency[4];

static UINT8 *exidy440_samples;
static INT32 exidy440_samples_len;


// forwards
static void play_cvsd(int ch);
static void cache_cvsd(int ch); // for state-ing
static void stop_cvsd(int ch);
static void decode_and_filter_cvsd(UINT8 *input, int bytes, int maskbits, int frequency, INT16 *output);
static void stream_update(INT16 **streams, INT32 samples);
static void reset_sound_cache();

//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------
void exidy440_scan(INT32 nAction, INT32 *pnMin)
{
	SCAN_VAR(m_sound_command);
	SCAN_VAR(m_sound_command_ack);

	SCAN_VAR(m_sound_banks);
	SCAN_VAR(m_sound_volume);
	SCAN_VAR(m_m6844_channel);
	SCAN_VAR(m_m6844_priority);
	SCAN_VAR(m_m6844_interrupt);
	SCAN_VAR(m_m6844_chain);

	for (INT32 i = 0; i < 4; i++) {
		SCAN_VAR(m_sound_channel[i].offset);
		SCAN_VAR(m_sound_channel[i].remaining);
	}

	SCAN_VAR(m_channel_frequency);

	if (nAction & ACB_WRITE) {
		reset_sound_cache();
		cache_cvsd(0);
		cache_cvsd(1);
		cache_cvsd(2);
		cache_cvsd(3);
	}
}

void exidy440_reset()
{
	/* reset the system */
	m_sound_command = 0;
	m_sound_command_ack = 1;

	m_sound_banks[0] = m_sound_banks[1] = m_sound_banks[2] = m_sound_banks[3] = 0;

	for (INT32 i = 0; i < 4; i++)
	{
		m_sound_channel[i].base = NULL;
		m_sound_channel[i].offset = 0;
		m_sound_channel[i].remaining = 0;
	}

	/* reset the 6844 */
	for (INT32 i = 0; i < 4; i++)
	{
		m_m6844_channel[i].active = 0;
		m_m6844_channel[i].control = 0x00;
	}
	m_m6844_priority = 0x00;
	m_m6844_interrupt = 0x00;
	m_m6844_chain = 0x00;

	reset_sound_cache();
}

void exidy440_init(UINT8 *samples_rom, INT32 samples_len, INT32 (*pCPUCyclesCB)(), INT32 nCPUMhz)
{
	m_channel_frequency[0] = clock();   /* channels 0 and 1 are run by FCLK */
	m_channel_frequency[1] = clock();
	m_channel_frequency[2] = clock()/2; /* channels 2 and 3 are run by SCLK */
	m_channel_frequency[3] = clock()/2;

	exidy440_samples = samples_rom;
	exidy440_samples_len = samples_len;

	/* get stream channels */
	//m_stream = machine().sound().stream_alloc(*this, 0, 2, clock());
	m_stream.init(clock(), nBurnSoundRate, 2, 0, stream_update);
	m_stream.set_buffered(pCPUCyclesCB, nCPUMhz);
	m_stream.set_volume(0.75);

	/* allocate the sample cache */
	INT32 length = samples_len * 16 + MAX_CACHE_ENTRIES * sizeof(sound_cache_entry);
	m_sound_cache = (sound_cache_entry *)BurnMalloc(length);
	m_sound_cache_length = length;

	/* determine the hard end of the cache and reset */
	m_sound_cache_max = (sound_cache_entry *)((UINT8 *)m_sound_cache + length);
	reset_sound_cache();

	exidy440_reset();

	/* allocate the mixer buffer */
	m_mixer_buffer_left = (INT32*)BurnMalloc(clock() * 2 * sizeof(INT32));
	m_mixer_buffer_right = (INT32*)BurnMalloc(clock() * 2 * sizeof(INT32));

	if (SOUND_LOG)
		m_debuglog = fopen("sound.log", "w");
}

void exidy440_update(INT16 *output, INT32 samples_len)
{
	if (samples_len != nBurnSoundLen) {
		bprintf(0, _T("exidy440_update(): once per frame, please!\n"));
		return;
	}

	m_stream.render(output, samples_len);
}

//-------------------------------------------------
//  device_stop - device-specific stop
//-------------------------------------------------

void exidy440_exit()
{
	m_stream.exit();
	BurnFree(m_sound_cache);
	BurnFree(m_mixer_buffer_left);
	BurnFree(m_mixer_buffer_right);

	if (SOUND_LOG && m_debuglog)
		fclose(m_debuglog);
}

/*************************************
 *
 *  Add a bunch of samples to the mix
 *
 *************************************/

static void add_and_scale_samples(int ch, INT32 *dest, int samples, int volume)
{
	sound_channel_data *channel = &m_sound_channel[ch];
	INT16 *srcdata;
	int i;

	/* channels 2 and 3 are half-rate samples */
	if (ch & 2)
	{
		srcdata = &channel->base[channel->offset >> 1];

		/* handle the edge case */
		if (channel->offset & 1)
		{
			*dest++ += *srcdata++ * volume / 256;
			samples--;
		}

		/* copy 1 for 2 to the destination */
		for (i = 0; i < samples; i += 2)
		{
			INT16 sample = *srcdata++ * volume / 256;
			*dest++ += sample;
			*dest++ += sample;
		}
	}

	/* channels 0 and 1 are full-rate samples */
	else
	{
		srcdata = &channel->base[channel->offset];
		for (i = 0; i < samples; i++)
			*dest++ += *srcdata++ * volume / 256;
	}
}



/*************************************
 *
 *  Mix the result to 16 bits
 *
 *************************************/

static void mix_to_16(int length, INT16 *dest_left, INT16 *dest_right)
{
	INT32 *mixer_left = m_mixer_buffer_left;
	INT32 *mixer_right = m_mixer_buffer_right;
	int i, clippers = 0;

	for (i = 0; i < length; i++)
	{
		INT32 sample_left = *mixer_left++;
		INT32 sample_right = *mixer_right++;

		if (sample_left < -32768) { sample_left = -32768; clippers++; }
		else if (sample_left > 32767) { sample_left = 32767; clippers++; }
		if (sample_right < -32768) { sample_right = -32768; clippers++; }
		else if (sample_right > 32767) { sample_right = 32767; clippers++; }

		*dest_left++ = sample_left;
		*dest_right++ = sample_right;
	}
}

/*************************************
 *
 *  Sound command register
 *
 *************************************/

UINT8 exidy440_sound_command_read()
{
	/* clear the FIRQ that got us here and acknowledge the read to the main CPU */
	//space.machine().device("audiocpu")->execute().set_input_line(1, CLEAR_LINE); -- driver side! -dink
	m_sound_command_ack = 1;

	return m_sound_command;
}

UINT8 exidy440_sound_command_ram()
{
	return m_sound_command;
}


void exidy440_sound_command(UINT8 param)
{
	m_sound_command = param;
	m_sound_command_ack = 0;
	//machine().device("audiocpu")->execute().set_input_line(INPUT_LINE_IRQ1, ASSERT_LINE); -- driver side! -dink
}


UINT8 exidy440_sound_command_ack()
{
	return m_sound_command_ack;
}



/*************************************
 *
 *  Sound volume registers
 *
 *************************************/

UINT8 exidy440_sound_volume_read(INT32 offset)
{
	return m_sound_volume[offset];
}

void exidy440_sound_volume_write(INT32 offset, UINT8 data)
{
	if (SOUND_LOG && m_debuglog)
		fprintf(m_debuglog, "Volume %02X=%02X\n", offset, data);

	/* update the stream */
	m_stream.update();

	/* set the new volume */
	m_sound_volume[offset] = ~data;
}



/*************************************
 *
 *  Sound interrupt handling
 *
 *************************************/

// interrupts on driver-side!
#if 0
WRITE8_MEMBER( exidy440_sound_device::sound_interrupt_clear_w )
{
	space.machine().device("audiocpu")->execute().set_input_line(0, CLEAR_LINE);
}
#endif

/*************************************
 *
 *  MC6844 DMA controller interface
 *
 *************************************/

static void m6844_update()
{
	/* update the stream */
	m_stream.update();
}


static void m6844_finished(m6844_channel_data *channel)
{
	/* mark us inactive */
	channel->active = 0;

	/* set the final address and counter */
	channel->counter = 0;
	channel->address = channel->start_address + channel->start_counter;

	/* clear the DMA busy bit and set the DMA end bit */
	channel->control &= ~0x40;
	channel->control |= 0x80;
}



/*************************************
 *
 *  MC6844 DMA controller I/O
 *
 *************************************/

UINT8 exidy440_m6844_read(INT32 offset)
{
	m6844_channel_data *m6844_channel = m_m6844_channel;
	int result = 0;

	/* first update the current state of the DMA transfers */
	m6844_update();

	/* switch off the offset we were given */
	switch (offset)
	{
		/* upper byte of address */
		case 0x00:
		case 0x04:
		case 0x08:
		case 0x0c:
			result = m6844_channel[offset / 4].address >> 8;
			break;

		/* lower byte of address */
		case 0x01:
		case 0x05:
		case 0x09:
		case 0x0d:
			result = m6844_channel[offset / 4].address & 0xff;
			break;

		/* upper byte of counter */
		case 0x02:
		case 0x06:
		case 0x0a:
		case 0x0e:
			result = m6844_channel[offset / 4].counter >> 8;
			break;

		/* lower byte of counter */
		case 0x03:
		case 0x07:
		case 0x0b:
		case 0x0f:
			result = m6844_channel[offset / 4].counter & 0xff;
			break;

		/* channel control */
		case 0x10:
		case 0x11:
		case 0x12:
		case 0x13:
			result = m6844_channel[offset - 0x10].control;

			/* a read here clears the DMA end flag */
			m6844_channel[offset - 0x10].control &= ~0x80;
			break;

		/* priority control */
		case 0x14:
			result = m_m6844_priority;
			break;

		/* interrupt control */
		case 0x15:

			/* update the global DMA end flag */
			m_m6844_interrupt &= ~0x80;
			m_m6844_interrupt |= (m6844_channel[0].control & 0x80) |
								(m6844_channel[1].control & 0x80) |
								(m6844_channel[2].control & 0x80) |
								(m6844_channel[3].control & 0x80);

			result = m_m6844_interrupt;
			break;

		/* chaining control */
		case 0x16:
			result = m_m6844_chain;
			break;

		/* 0x17-0x1f not used */
		default: break;
	}

	return result;
}


void exidy440_m6844_write(INT32 offset, UINT8 data)
{
	m6844_channel_data *m6844_channel = m_m6844_channel;
	int i;

	/* first update the current state of the DMA transfers */
	m6844_update();

	/* switch off the offset we were given */
	switch (offset)
	{
		/* upper byte of address */
		case 0x00:
		case 0x04:
		case 0x08:
		case 0x0c:
			m6844_channel[offset / 4].address = (m6844_channel[offset / 4].address & 0xff) | (data << 8);
			break;

		/* lower byte of address */
		case 0x01:
		case 0x05:
		case 0x09:
		case 0x0d:
			m6844_channel[offset / 4].address = (m6844_channel[offset / 4].address & 0xff00) | (data & 0xff);
			break;

		/* upper byte of counter */
		case 0x02:
		case 0x06:
		case 0x0a:
		case 0x0e:
			m6844_channel[offset / 4].counter = (m6844_channel[offset / 4].counter & 0xff) | (data << 8);
			break;

		/* lower byte of counter */
		case 0x03:
		case 0x07:
		case 0x0b:
		case 0x0f:
			m6844_channel[offset / 4].counter = (m6844_channel[offset / 4].counter & 0xff00) | (data & 0xff);
			break;

		/* channel control */
		case 0x10:
		case 0x11:
		case 0x12:
		case 0x13:
			m6844_channel[offset - 0x10].control = (m6844_channel[offset - 0x10].control & 0xc0) | (data & 0x3f);
			break;

		/* priority control */
		case 0x14:
			m_m6844_priority = data;

			/* update the sound playback on each channel */
			for (i = 0; i < 4; i++)
			{
				/* if we're going active... */
				if (!m6844_channel[i].active && (data & (1 << i)))
				{
					/* mark us active */
					m6844_channel[i].active = 1;

					/* set the DMA busy bit and clear the DMA end bit */
					m6844_channel[i].control |= 0x40;
					m6844_channel[i].control &= ~0x80;

					/* set the starting address, counter, and time */
					m6844_channel[i].start_address = m6844_channel[i].address;
					m6844_channel[i].start_counter = m6844_channel[i].counter;

					/* generate and play the sample */
					play_cvsd(i);
				}

				/* if we're going inactive... */
				else if (m6844_channel[i].active && !(data & (1 << i)))
				{
					/* mark us inactive */
					m6844_channel[i].active = 0;

					/* stop playing the sample */
					stop_cvsd(i);
				}
			}
			break;

		/* interrupt control */
		case 0x15:
			m_m6844_interrupt = (m_m6844_interrupt & 0x80) | (data & 0x7f);
			break;

		/* chaining control */
		case 0x16:
			m_m6844_chain = data;
			break;

		/* 0x17-0x1f not used */
		default: break;
	}
}



/*************************************
 *
 *  Sound cache management
 *
 *************************************/

static void reset_sound_cache()
{
	memset(m_sound_cache, 0, m_sound_cache_length);

	m_sound_cache_end = m_sound_cache;
}


static INT16 *add_to_sound_cache(UINT8 *input, int address, int length, int bits, int frequency)
{
	sound_cache_entry *current = m_sound_cache_end;

	/* compute where the end will be once we add this entry */
	m_sound_cache_end = (sound_cache_entry *)((UINT8 *)current + sizeof(sound_cache_entry) + length * 16);

	/* if this will overflow the cache, reset and re-add */
	if (m_sound_cache_end > m_sound_cache_max)
	{
		reset_sound_cache();
		return add_to_sound_cache(input, address, length, bits, frequency);
	}

	/* fill in this entry */
	current->next = m_sound_cache_end;
	current->address = address;
	current->length = length;
	current->bits = bits;
	current->frequency = frequency;

	/* decode the data into the cache */
	decode_and_filter_cvsd(input, length, bits, frequency, current->data);
	return current->data;
}


static INT16 *find_or_add_to_sound_cache(int address, int length, int bits, int frequency)
{
	sound_cache_entry *current;

	for (current = m_sound_cache; current < m_sound_cache_end; current = current->next)
		if (current->address == address && current->length == length && current->bits == bits && current->frequency == frequency)
			return current->data;

	return add_to_sound_cache(&exidy440_samples[address], address, length, bits, frequency);
}



/*************************************
 *
 *  Internal CVSD decoder and player
 *
 *************************************/

static void play_cvsd(int ch)
{
	sound_channel_data *channel = &m_sound_channel[ch];
	int address = m_m6844_channel[ch].address;
	int length = m_m6844_channel[ch].counter;
	INT16 *base;

	/* add the bank number to the address */
	if (m_sound_banks[ch] & 1)
		address += 0x00000;
	else if (m_sound_banks[ch] & 2)
		address += 0x08000;
	else if (m_sound_banks[ch] & 4)
		address += 0x10000;
	else if (m_sound_banks[ch] & 8)
		address += 0x18000;

	/* compute the base address in the converted samples array */
	base = find_or_add_to_sound_cache(address, length, channel_bits[ch], m_channel_frequency[ch]);
	if (!base)
		return;

	/* if the length is 0 or 1, just do an immediate end */
	if (length <= 3)
	{
		channel->base = base;
		channel->offset = length;
		channel->remaining = 0;
		m6844_finished(&m_m6844_channel[ch]);
		return;
	}
/*	bprintf(0, _T("Sound channel %d play at %02X,%04X, length = %04X, volume = %02X/%02X\n"),
				ch, m_sound_banks[ch],
				m_m6844_channel[ch].address, m_m6844_channel[ch].counter,
				m_sound_volume[ch * 2], m_sound_volume[ch * 2 + 1]);
*/
	if (SOUND_LOG && m_debuglog)
		fprintf(m_debuglog, "Sound channel %d play at %02X,%04X, length = %04X, volume = %02X/%02X\n",
				ch, m_sound_banks[ch],
				m_m6844_channel[ch].address, m_m6844_channel[ch].counter,
				m_sound_volume[ch * 2], m_sound_volume[ch * 2 + 1]);

	/* set the pointer and count */
	channel->base = base;
	channel->offset = 0;
	channel->remaining = length * 8;

	/* channels 2 and 3 play twice as slow, so we need to count twice as many samples */
	if (ch & 2) channel->remaining *= 2;
}

static void cache_cvsd(int ch) // for state-ing
{
	sound_channel_data *channel = &m_sound_channel[ch];
	int address = m_m6844_channel[ch].start_address;
	int length = m_m6844_channel[ch].start_counter;
	INT16 *base;

	if (channel->remaining <= 0) // time to leave
		return;

	/* add the bank number to the address */
	if (m_sound_banks[ch] & 1)
		address += 0x00000;
	else if (m_sound_banks[ch] & 2)
		address += 0x08000;
	else if (m_sound_banks[ch] & 4)
		address += 0x10000;
	else if (m_sound_banks[ch] & 8)
		address += 0x18000;

	/* compute the base address in the converted samples array */
	base = find_or_add_to_sound_cache(address, length, channel_bits[ch], m_channel_frequency[ch]);
	if (!base)
		return;

	channel->base = base;
}


static void stop_cvsd(int ch)
{
	/* the DMA channel is marked inactive; that will kill the audio */
	m_sound_channel[ch].remaining = 0;
	m_stream.update();

	if (SOUND_LOG && m_debuglog)
		fprintf(m_debuglog, "Channel %d stop\n", ch);
}



/*************************************
 *
 *  FIR digital filter
 *
 *************************************/

static void fir_filter(INT32 *input, INT16 *output, int count)
{
	while (count--)
	{
		INT32 result = (input[-1] - input[-8] - input[-48] + input[-55]) << 2;
		result += (input[0] + input[-18] + input[-38] + input[-56]) << 3;
		result += (-input[-2] - input[-4] + input[-5] + input[-51] - input[-52] - input[-54]) << 4;
		result += (-input[-3] - input[-11] - input[-45] - input[-53]) << 5;
		result += (input[-6] + input[-7] - input[-9] - input[-15] - input[-41] - input[-47] + input[-49] + input[-50]) << 6;
		result += (-input[-10] + input[-12] + input[-13] + input[-14] + input[-21] + input[-35] + input[-42] + input[-43] + input[-44] - input[-46]) << 7;
		result += (-input[-16] - input[-17] + input[-19] + input[-37] - input[-39] - input[-40]) << 8;
		result += (input[-20] - input[-22] - input[-24] + input[-25] + input[-31] - input[-32] - input[-34] + input[-36]) << 9;
		result += (-input[-23] - input[-33]) << 10;
		result += (input[-26] + input[-30]) << 11;
		result += (input[-27] + input[-28] + input[-29]) << 12;
		result >>= 14;

		if (result < -32768)
			result = -32768;
		else if (result > 32767)
			result = 32767;

		*output++ = result;
		input++;
	}
}



/*************************************
 *
 *  CVSD decoder
 *
 *************************************/

static void decode_and_filter_cvsd(UINT8 *input, int bytes, int maskbits, int frequency, INT16 *output)
{
	INT32 buffer[SAMPLE_BUFFER_LENGTH + FIR_HISTORY_LENGTH];
	int total_samples = bytes * 8;
	int mask = (1 << maskbits) - 1;
	double filter, integrator, leak;
	double charge, decay, gain;
	int steps;
	int chunk_start;

	/* compute the charge, decay, and leak constants */
	charge = pow(exp(-1.0), 1.0 / (FILTER_CHARGE_TC * (double)frequency));
	decay = pow(exp(-1.0), 1.0 / (FILTER_DECAY_TC * (double)frequency));
	leak = pow(exp(-1.0), 1.0 / (INTEGRATOR_LEAK_TC * (double)frequency));

	/* compute the gain */
	gain = SAMPLE_GAIN;

	/* clear the history words for a start */
	memset(&buffer[0], 0, FIR_HISTORY_LENGTH * sizeof(INT32));

	/* initialize the CVSD decoder */
	steps = 0xaa;
	filter = FILTER_MIN;
	integrator = 0.0;

	/* loop over chunks */
	for (chunk_start = 0; chunk_start < total_samples; chunk_start += SAMPLE_BUFFER_LENGTH)
	{
		INT32 *bufptr = &buffer[FIR_HISTORY_LENGTH];
		int chunk_bytes;
		int ind;

		/* how many samples do we generate in this chunk? */
		if (chunk_start + SAMPLE_BUFFER_LENGTH > total_samples)
			chunk_bytes = (total_samples - chunk_start) / 8;
		else
			chunk_bytes = SAMPLE_BUFFER_LENGTH / 8;

		/* loop over samples */
		for (ind = 0; ind < chunk_bytes; ind++)
		{
			double temp;
			int databyte = *input++;
			int bit;
			int sample;

			/* loop over bits in the byte, low to high */
			for (bit = 0; bit < 8; bit++)
			{
				/* move the estimator up or down a step based on the bit */
				if (databyte & (1 << bit))
				{
					integrator += filter;
					steps = (steps << 1) | 1;
				}
				else
				{
					integrator -= filter;
					steps <<= 1;
				}

				/* keep track of the last n bits */
				steps &= mask;

				/* simulate leakage */
				integrator *= leak;

				/* if we got all 0's or all 1's in the last n bits, bump the step up */
				if (steps == 0 || steps == mask)
				{
					filter = FILTER_MAX - ((FILTER_MAX - filter) * charge);
					if (filter > FILTER_MAX)
						filter = FILTER_MAX;
				}

				/* simulate decay */
				else
				{
					filter *= decay;
					if (filter < FILTER_MIN)
						filter = FILTER_MIN;
				}

				/* compute the sample as a 32-bit word */
				temp = integrator * gain;

				/* compress the sample range to fit better in a 16-bit word */
				if (temp < 0)
					sample = (int)(temp / (-temp * (1.0 / 32768.0) + 1.0));
				else
					sample = (int)(temp / (temp * (1.0 / 32768.0) + 1.0));

				/* store the result to our temporary buffer */
				*bufptr++ = sample;
			}
		}

		/* all done with this chunk, run the filter on it */
		fir_filter(&buffer[FIR_HISTORY_LENGTH], &output[chunk_start], chunk_bytes * 8);

		/* copy the last few input samples down to the start for a new history */
		memcpy(&buffer[0], &buffer[SAMPLE_BUFFER_LENGTH], FIR_HISTORY_LENGTH * sizeof(INT32));
	}

	/* make sure the volume goes smoothly to 0 over the last 512 samples */
	if (FADE_TO_ZERO)
	{
		INT16 *data;

		chunk_start = (total_samples > 512) ? total_samples - 512 : 0;
		data = output + chunk_start;
		for ( ; chunk_start < total_samples; chunk_start++)
		{
			*data = (*data * ((total_samples - chunk_start) >> 9));
			data++;
		}
	}
}


void exidy440_sound_banks_write(INT32 offset, UINT8 data)
{
	m_sound_banks[offset] = data;
}

//-------------------------------------------------
//  sound_stream_update - handle a stream update
//-------------------------------------------------

static void stream_update(INT16 **streams, INT32 samples)
{
	int ch;

	/* reset the mixer buffers */
	memset(m_mixer_buffer_left, 0, samples * sizeof(INT32));
	memset(m_mixer_buffer_right, 0, samples * sizeof(INT32));

	/* loop over channels */
	for (ch = 0; ch < 4; ch++)
	{
		sound_channel_data *channel = &m_sound_channel[ch];
		int length, volume, left = samples;
		int effective_offset;

		/* if we're not active, bail */
		if (channel->remaining <= 0)
			continue;

		/* see how many samples to copy */
		length = (left > channel->remaining) ? channel->remaining : left;

		/* get a pointer to the sample data and copy to the left */
		volume = m_sound_volume[2 * ch + 0];
		if (volume)
			add_and_scale_samples(ch, m_mixer_buffer_left, length, volume);

		/* get a pointer to the sample data and copy to the left */
		volume = m_sound_volume[2 * ch + 1];
		if (volume)
			add_and_scale_samples(ch, m_mixer_buffer_right, length, volume);

		/* update our counters */
		channel->offset += length;
		channel->remaining -= length;
		left -= length;

		/* update the MC6844 */
		effective_offset = (ch & 2) ? channel->offset / 2 : channel->offset;
		m_m6844_channel[ch].address = m_m6844_channel[ch].start_address + effective_offset / 8;
		m_m6844_channel[ch].counter = m_m6844_channel[ch].start_counter - effective_offset / 8;
		if (m_m6844_channel[ch].counter <= 0)
		{
			if (SOUND_LOG && m_debuglog)
				fprintf(m_debuglog, "Channel %d finished\n", ch);
			m6844_finished(&m_m6844_channel[ch]);
		}
	}

	/* all done, time to mix it */
	mix_to_16(samples, streams[0], streams[1]);
}

#if 0
/*************************************
 *
 *  Memory handlers
 *
 *************************************/

static ADDRESS_MAP_START( exidy440_audio_map, AS_PROGRAM, 8, driver_device )
	AM_RANGE(0x0000, 0x7fff) AM_NOP
	AM_RANGE(0x8000, 0x801f) AM_MIRROR(0x03e0) AM_DEVREADWRITE("custom", exidy440_sound_device, m6844_r, m6844_w)
	AM_RANGE(0x8400, 0x840f) AM_MIRROR(0x03f0) AM_DEVREADWRITE("custom", exidy440_sound_device, sound_volume_r, sound_volume_w)
	AM_RANGE(0x8800, 0x8800) AM_MIRROR(0x03ff) AM_DEVREAD("custom", exidy440_sound_device, sound_command_r) AM_WRITENOP
	AM_RANGE(0x8c00, 0x93ff) AM_NOP
	AM_RANGE(0x9400, 0x9403) AM_MIRROR(0x03fc) AM_READNOP AM_DEVWRITE("custom", exidy440_sound_device, sound_banks_w)
	AM_RANGE(0x9800, 0x9800) AM_MIRROR(0x03ff) AM_READNOP AM_DEVWRITE("custom", exidy440_sound_device, sound_interrupt_clear_w)
	AM_RANGE(0x9c00, 0x9fff) AM_NOP
	AM_RANGE(0xa000, 0xbfff) AM_RAM
	AM_RANGE(0xc000, 0xdfff) AM_NOP
	AM_RANGE(0xe000, 0xffff) AM_ROM
ADDRESS_MAP_END



/*************************************
 *
 *  Machine driver
 *
 *************************************/

MACHINE_CONFIG_FRAGMENT( exidy440_audio )

	MCFG_CPU_ADD("audiocpu", M6809, EXIDY440_AUDIO_CLOCK)
	MCFG_CPU_PROGRAM_MAP(exidy440_audio_map)
	MCFG_CPU_VBLANK_INT_DRIVER("screen", driver_device, irq0_line_assert)

	MCFG_SPEAKER_STANDARD_STEREO("lspeaker", "rspeaker")

	MCFG_SOUND_ADD("custom", EXIDY440, EXIDY440_AUDIO_CLOCK/16)
	MCFG_SOUND_ROUTE(0, "lspeaker", 1.0)
	MCFG_SOUND_ROUTE(1, "rspeaker", 1.0)

//  MCFG_SOUND_ADD("cvsd1", MC3418, EXIDY440_MC3418_CLOCK)
//  MCFG_SOUND_ROUTE(ALL_OUTPUTS, "lspeaker", 1.0)

//  MCFG_SOUND_ADD("cvsd2", MC3418, EXIDY440_MC3418_CLOCK)
//  MCFG_SOUND_ROUTE(ALL_OUTPUTS, "rspeaker", 1.0)

//  MCFG_SOUND_ADD("cvsd3", MC3417, EXIDY440_MC3417_CLOCK)
//  MCFG_SOUND_ROUTE(ALL_OUTPUTS, "lspeaker", 1.0)

//  MCFG_SOUND_ADD("cvsd4", MC3417, EXIDY440_MC3417_CLOCK)
//  MCFG_SOUND_ROUTE(ALL_OUTPUTS, "rspeaker", 1.0)
MACHINE_CONFIG_END
#endif
