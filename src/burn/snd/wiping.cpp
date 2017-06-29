/***************************************************************************

    Wiping sound driver (quick hack of the Namco sound driver)

    used by wiping and clshroad

***************************************************************************/

#include "burnint.h"
#include "wiping.h"

static const INT32 samplerate = 48000;
static const INT32 defgain = 48;

/* 8 voices max */
#define MAX_VOICES 8

/* this structure defines the parameters for a channel */
struct sound_channel
{
	INT32 frequency;
	INT32 counter;
	INT32 volume;
	const UINT8 *wave;
	INT32 oneshot;
	INT32 oneshotplaying;
};

/* data about the sound system */
static sound_channel m_channel_list[MAX_VOICES];
static sound_channel *m_last_channel;

/* global sound parameters */
static UINT8 *m_sound_prom;
static UINT8 *m_sound_rom;
static INT32 m_num_voices;

/* mixer tables and internal buffers */
static INT16 *m_mixer_table;
static INT16 *m_mixer_lookup;
static INT16 *m_mixer_buffer;

static INT32 game_is_wiping = 0;

static UINT8 m_soundregs[0x4000];

static void make_mixer_table(INT32 voices, INT32 gain);

void wipingsnd_scan()
{
	SCAN_VAR(m_channel_list);
	SCAN_VAR(m_soundregs);
}

void wipingsnd_init(UINT8 *rom, UINT8 *prom)
{
	m_sound_prom = prom;
	m_sound_rom = rom;

	m_mixer_buffer = (INT16 *)BurnMalloc(sizeof(INT16) * 2 * samplerate);

	/* build the mixer table */
	make_mixer_table(8, defgain);
	wipingsnd_reset();
}

void wipingsnd_wipingmode() // lowers the volume of "that noise"
{
	game_is_wiping = 1;
}

void wipingsnd_exit()
{
	BurnFree(m_mixer_buffer);
	BurnFree(m_mixer_table);
	game_is_wiping = 0;
}

void wipingsnd_reset()
{
	memset(m_channel_list, 0, sizeof(sound_channel)*MAX_VOICES);
	memset(m_soundregs, 0, sizeof(UINT8)*0x4000);

	sound_channel *voice;

	m_num_voices = 8;
	m_last_channel = m_channel_list + m_num_voices;

	/* reset all the voices */
	for (voice = m_channel_list; voice < m_last_channel; voice++)
	{
		voice->frequency = 0;
		voice->volume = 0;
		voice->wave = &m_sound_prom[0];
		voice->counter = 0;
	}
}

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
		int val = i * gain * 16 / voices;
		if (val > 32767) val = 32767;
		m_mixer_lookup[ i] = val;
		m_mixer_lookup[-i] = -val;
	}
}

void wipingsnd_write(INT32 offset, UINT8 data)
{
	sound_channel *voice;
	INT32 base;

	offset &= 0x3fff;

	/* set the register */
	m_soundregs[offset] = data;

	/* recompute all the voice parameters */
	if (offset <= 0x3f)
	{
		for (base = 0, voice = m_channel_list; voice < m_last_channel; voice++, base += 8)
		{
			voice->frequency = m_soundregs[0x02 + base] & 0x0f;
			voice->frequency = voice->frequency * 16 + ((m_soundregs[0x01 + base]) & 0x0f);
			voice->frequency = voice->frequency * 16 + ((m_soundregs[0x00 + base]) & 0x0f);

			voice->volume = m_soundregs[0x07 + base] & 0x0f;

			if (m_soundregs[0x5 + base] & 0x0f)
			{
				// hack :)
				if (128 * (16 * (m_soundregs[0x5 + base] & 0x0f) + (m_soundregs[0x2005 + base] & 0x0f)) == 0x1800
					&& game_is_wiping) voice->volume = 0x06;
				// end of hack

				voice->wave = &m_sound_rom[128 * (16 * (m_soundregs[0x5 + base] & 0x0f)
						+ (m_soundregs[0x2005 + base] & 0x0f))];
				voice->oneshot = 1;
			}
			else
			{
				voice->wave = &m_sound_rom[16 * (m_soundregs[0x3 + base] & 0x0f)];
				voice->oneshot = 0;
				voice->oneshotplaying = 0;
			}
		}
	}
	else if (offset >= 0x2000)
	{
		voice = &m_channel_list[(offset & 0x3f)/8];
		if (voice->oneshot)
		{
			voice->counter = 0;
			voice->oneshotplaying = 1;
		}
	}
}

void wipingsnd_update(INT16 *outputs, INT32 samples_len)
{
	sound_channel *voice;

	// compute # of samples @ soundcore native (48khz) rate
	INT32 samples = (((((samplerate*1000) / nBurnFPS) * samples_len) / nBurnSoundLen)) / 10;

	if (samples > samplerate) samples = samplerate;

	/* zap the contents of the mixer buffer */
	memset(m_mixer_buffer, 0, sizeof(INT16) * 2 * samplerate);

	/* loop over each voice and add its contribution */
	for (voice = m_channel_list; voice < m_last_channel; voice++)
	{
		INT32 f = 16*voice->frequency;
		INT32 v = voice->volume;

		/* only update if we have non-zero volume and frequency */
		if (v && f)
		{
			const UINT8 *w = voice->wave;
			INT32 c = voice->counter;

			INT16 *mix = m_mixer_buffer;

			/* add our contribution */
			for (INT32 i = 0; i < samples; i++)
			{
				INT32 offs;

				c += f;

				if (voice->oneshot)
				{
					if (voice->oneshotplaying)
					{
						offs = (c >> 15);
						if (w[offs>>1] == 0xff)
						{
							voice->oneshotplaying = 0;
						}

						else
						{
							/* use full byte, first the high 4 bits, then the low 4 bits */
							if (offs & 1)
								*mix++ += ((w[offs>>1] & 0x0f) - 8) * v;
							else
								*mix++ += (((w[offs>>1]>>4) & 0x0f) - 8) * v;
						}
					}
				}
				else
				{
					offs = (c >> 15) & 0x1f;

					/* use full byte, first the high 4 bits, then the low 4 bits */
					if (offs & 1)
						*mix++ += ((w[offs>>1] & 0x0f) - 8) * v;
					else
						*mix++ += (((w[offs>>1]>>4) & 0x0f) - 8) * v;
				}
			}

			/* update the counter for this voice */
			voice->counter = c;
		}
	}

	// resample native (48khz) -> our rate
	for (INT32 j = 0; j < samples_len; j++)
	{
		INT32 k = (((((samplerate*1000) / nBurnFPS) * (j & ~2)) / nBurnSoundLen)) / 10;

		INT32 lr = (INT32)(m_mixer_lookup[m_mixer_buffer[k]] * 0.50);

		outputs[0] = BURN_SND_CLIP(lr);
		outputs[1] = BURN_SND_CLIP(lr);
		outputs += 2;
	}

}
