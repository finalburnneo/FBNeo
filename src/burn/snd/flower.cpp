/* Clarue Flower sound driver.
Initial version was based on the Wiping sound driver, which was based on the old namco.c sound driver.
*/

#include "burnint.h"
#include "flower.h"
#include <stddef.h>

static const INT32 samplerate = 48000;

struct flower_sound_channel
{
	UINT32 start;
	UINT32 pos;
	UINT16 freq;
	UINT8 volume;
	UINT8 voltab;
	UINT8 oneshot;
	UINT8 active;
	UINT8 effect;
	UINT32 ecount;
};

static flower_sound_channel m_channel_list[8];
static flower_sound_channel *m_last_channel;

/* global sound parameters */
static const UINT8 *m_sample_rom;
static const UINT8 *m_volume_rom;

/* mixer tables and internal buffers */
static INT16 *m_mixer_table;
static INT16 *m_mixer_lookup;
static INT16 *m_mixer_buffer;

static UINT8 m_soundregs1[0x40];
static UINT8 m_soundregs2[0x40];

/* build a table to divide by the number of voices; gain is specified as gain*16 */
static void make_mixer_table(INT32 voices, INT32 gain)
{
	INT32 count = voices * 128;

	/* allocate memory */
	m_mixer_table = (INT16 *)BurnMalloc(sizeof(INT16) * 256 * voices);

	/* find the middle of the table */
	m_mixer_lookup = m_mixer_table + (128 * voices);

	/* fill in the table - 16 bit case */
	for (INT32 i = 0; i < count; i++)
	{
		INT32 val = i * gain * 16 / voices;
		if (val > 32767) val = 32767;
		m_mixer_lookup[ i] = val;
		m_mixer_lookup[-i] =-val;
	}
}

//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void flower_sound_init(UINT8 *rom_sample, UINT8 *rom_volume)
{
	m_mixer_buffer = (INT16 *)BurnMalloc(sizeof(INT16) * 2 * samplerate);
	make_mixer_table(8, 48);

	m_sample_rom = rom_sample;
	m_volume_rom = rom_volume;

	m_last_channel = m_channel_list + 8;
}

void flower_sound_exit()
{
	BurnFree(m_mixer_buffer);
	BurnFree(m_mixer_table);
}

void flower_sound_scan()
{
	SCAN_VAR(m_channel_list);
	SCAN_VAR(m_soundregs1);
	SCAN_VAR(m_soundregs2);
}

//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void flower_sound_reset()
{
	/* reset all the voices */
	for (INT32 i = 0; i < 8; i++)
	{
		flower_sound_channel *voice = &m_channel_list[i];

		voice->freq = 0;
		voice->pos = 0;
		voice->volume = 0;
		voice->voltab = 0;
		voice->effect = 0;
		voice->ecount = 0;
		voice->oneshot = 1;
		voice->active = 0;
		voice->start = 0;
	}

	memset(m_soundregs1, 0, 0x40);
	memset(m_soundregs2, 0, 0x40);
}

/********************************************************************************/
/* register functions (preliminary):
offset: cccrrr      c=channel, r=register

set 1:
R  76543210
0  xxxxxxxx         frequency (which nibble?)
1  xxxxxxxx         *
2  xxxxxxxx         *
3  xxxxxxxx         *
4  ...x....         one-shot sample
5  ...x....         ??? same as R4?
6  ........         unused
7  xxxx....         volume

set 2:
R  76543210
0  ....xxxx         start address
1  ....xxxx         *
2  ....xxxx         *
3  ....xxxx         *
4  xxxx             assume it's channel pitch/volume effects
       xxxx         start address
5  x...             ???
       xxxx         start address
6  ........         unused
7  ......xx         volume table + start trigger

*/

void flower_sound1_w(UINT16 offset, UINT8 data)
{
	flower_sound_channel *voice = &m_channel_list[offset >> 3 & 7];
	INT32 c = offset & 0xf8;
	UINT8 *base1 = m_soundregs1;

	base1[offset] = data;

	// recompute voice parameters
	voice->freq = (base1[c+2] & 0xf) << 12 | (base1[c+3] & 0xf) << 8 | (base1[c+0] & 0xf) << 4 | (base1[c+1] & 0xf);
	voice->volume = base1[c+7] >> 4;
}

void flower_sound2_w(UINT16 offset, UINT8 data)
{
	flower_sound_channel *voice = &m_channel_list[offset >> 3 & 7];
	INT32 i, c = offset & 0xf8;
	UINT8 *base1 = m_soundregs1;
	UINT8 *base2 = m_soundregs2;

	base2[offset] = data;

	// reg 7 is start trigger!
	if ((offset & 7) != 7)
		return;

	voice->voltab = (base2[c+7] & 3) << 4;
	voice->oneshot = (~base1[c+4] & 0x10) >> 4;
	voice->effect = base2[c+4] >> 4;
	voice->ecount = 0;
	voice->pos = 0;
	voice->active = 1;

	// full start address is 6 nibbles
	voice->start = 0;
	for (i = 5; i >= 0; i--)
		voice->start = (voice->start << 4) | (base2[c+i] & 0xf);
}

static void update_effects()
{
	flower_sound_channel *voice;

	for (voice = m_channel_list; voice < m_last_channel; voice++)
		voice->ecount += (voice->ecount < (1<<22));
}

//-------------------------------------------------
//  sound_stream_update - handle a stream update
//-------------------------------------------------

void flower_sound_update(INT16 *outputs, INT32 samples_len)
{
	INT16 *mix;

	// compute # of samples @ soundcore native (48khz) rate
	INT32 samples = (((((samplerate*1000) / nBurnFPS) * samples_len) / nBurnSoundLen)) / 10;

	if (samples > samplerate) samples = samplerate;

	/* zap the contents of the mixer buffer */
	memset(m_mixer_buffer, 0, samples * sizeof(INT16));

	update_effects(); // once per frame

	/* loop over each voice and add its contribution */
	for (flower_sound_channel *voice = m_channel_list; voice < m_last_channel; voice++)
	{
		INT32 f = voice->freq;
		INT32 v = voice->volume;

		if (!voice->active)
			continue;

		// effects
		// bit 0: volume slide down?
		if (voice->effect & 1 && !voice->oneshot)
		{
			// note: one-shot samples are fixed volume
			v -= (voice->ecount >> 4);
			if (v < 0) v = 0;
		}
		// bit 1: used often, but hard to figure out what for
		// bit 2: probably pitch slide
		if (voice->effect & 4)
		{
			f -= (voice->ecount << 7);
			if (f < 0) f = 0;
		}
		// bit 3: not used much, maybe pitch slide the other way?

		v |= voice->voltab;

		mix = m_mixer_buffer;

		for (INT32 i = 0; i < samples; i++)
		{
			// add sample
			if (voice->oneshot)
			{
				UINT8 sample = m_sample_rom[(voice->start + voice->pos) >> 7 & 0x7fff];
				if (sample == 0xff)
				{
					voice->active = 0;
					break;
				}
				else
					*mix++ += m_volume_rom[v << 8 | sample] - 0x80;
			}
			else
			{
				UINT8 sample = m_sample_rom[(voice->start >> 7 & 0x7e00) | (voice->pos >> 7 & 0x1ff)];
				*mix++ += m_volume_rom[v << 8 | sample] - 0x80;
			}

			// update counter
			voice->pos += f;
		}
	}

	// resample native (48khz) -> our rate
	for (INT32 j = 0; j < samples_len; j++)
	{
		INT32 k = (((((samplerate*1000) / nBurnFPS) * j) / nBurnSoundLen)) / 10;

		INT32 lr = (INT32)(m_mixer_lookup[m_mixer_buffer[k]] * 0.50);

		outputs[0] = BURN_SND_CLIP(lr);
		outputs[1] = BURN_SND_CLIP(lr);
		outputs += 2;
	}

}
