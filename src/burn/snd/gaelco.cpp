// license:BSD-3-Clause
// copyright-holders:Manuel Abadia, David Haywood
/***************************************************************************
                    Gaelco Sound Hardware

                By Manuel Abadia <emumanu+mame@gmail.com>

CG-1V/GAE1 (Gaelco custom GFX & Sound chip):
    The CG-1V/GAE1 can handle up to 7 stereo channels.
    The chip output is connected to a TDA1543 (16 bit DAC).

Registers per channel:
======================
    Word | Bit(s)            | Description
    -----+-FEDCBA98-76543210-+--------------------------
      0  | xxxxxxxx xxxxxxxx | not used?
      1  | xxxx---- -------- | left channel volume (0x00..0x0f)
      1  | ----xxxx -------- | right channel volume (0x00..0x0f)
      1  | -------- xxxx---- | sample type (0x0c = PCM 8 bits mono, 0x08 = PCM 8 bits stereo)
      1  | -------- ----xxxx | ROM Bank
      2  | xxxxxxxx xxxxxxxx | sample end position
      3  | xxxxxxxx xxxxxxxx | remaining bytes to play

      the following are used only when looping (usually used for music)

      4  | xxxxxxxx xxxxxxxx | not used?
      5  | xxxx---- -------- | left channel volume (0x00..0x0f)
      5  | ----xxxx -------- | right channel volume (0x00..0x0f)
      5  | -------- xxxx---- | sample type (0x0c = PCM 8 bits mono, 0x08 = PCM 8 bits stereo)
      5  | -------- ----xxxx | ROM Bank
      6  | xxxxxxxx xxxxxxxx | sample end position
      7  | xxxxxxxx xxxxxxxx | remaining bytes to play

    The samples are played from (end position + length) to (end position)!

***************************************************************************/

#include "burnint.h"
#include "gaelco.h"

#define GAELCO_NUM_CHANNELS     0x07
#define GAELCO_VOLUME_LEVELS    0x10

struct gaelco_sound_channel
{
	INT32 active;         // is it playing?
	INT32 loop;           // = 0 no looping, = 1 looping
	INT32 chunkNum;       // current chunk if looping

	INT32 ch_data_r;      // for ramp-down -dink
	INT32 ch_data_l;
};

//sound_stream *m_stream;                                 /* our stream */
static UINT8 *m_snd_data;                                      /* PCM data */
static INT32 m_banks[4];                                         /* start of each ROM bank */
static gaelco_sound_channel m_channel[GAELCO_NUM_CHANNELS];    /* 7 stereo channels */
static UINT16 m_sndregs[0x38];

static INT32 gaelcosnd_initted = 0;

static INT32 gaelcosnd_mono = 0;
static INT32 gaelcosnd_swap_lr = 0;

// for resampling
static UINT32 nSampleSize;
static INT32 nFractionalPosition;
static INT32 nPosition;
static INT16 *sample_buffer;

// Table for converting from 8 to 16 bits with volume control
static INT16 m_volume_table[GAELCO_VOLUME_LEVELS][256];

static void gaelco_set_bank_offsets(INT32 offs1, INT32 offs2, INT32 offs3, INT32 offs4)
{
	m_banks[0] = offs1;
	m_banks[1] = offs2;
	m_banks[2] = offs3;
	m_banks[3] = offs4;
}

/*============================================================================
                        CG-1V/GAE1 Sound Update

            Writes length bytes to the sound buffer
  ============================================================================*/

void gaelcosnd_update(INT16 *outputs, INT32 samples)
{
	if (samples != nBurnSoundLen) {
		bprintf(0, _T("gaelcosnd_update(): once per frame, please!\n"));
		return;
	}

	INT32 nSamplesNeeded = ((((((8000 * 1000) / nBurnFPS) * samples) / nBurnSoundLen)) / 10) + 1;
	if (nBurnSoundRate < 44100) nSamplesNeeded += 2; // so we don't end up with negative nPosition below

	INT16 *lmix = sample_buffer + (8000 * 0) + 5 + nPosition;
	INT16 *rmix = sample_buffer + (8000 * 1) + 5 + nPosition;

	/* zap the contents of the mixer buffer */
	memset(lmix, 0, nSamplesNeeded * sizeof(INT16));
	memset(rmix, 0, nSamplesNeeded * sizeof(INT16));

	/* fill all data needed */
	for(INT32 j = 0; j < (nSamplesNeeded - nPosition); j++){
		INT32 output_l = 0, output_r = 0;

		/* for each channel */
		for (INT32 ch = 0; ch < GAELCO_NUM_CHANNELS; ch ++){
			INT32 ch_data_l = 0, ch_data_r = 0;
			gaelco_sound_channel *channel = &m_channel[ch];

			/* if the channel is playing */
			if (channel->active == 1) {
				INT32 data, chunkNum = 0;
				INT32 base_offset, type, bank, vol_r, vol_l, end_pos;

				/* if the channel is looping, get current chunk to play */
				if (channel->loop == 1) {
					chunkNum = channel->chunkNum;
				}

				base_offset = ch*8 + chunkNum*4;

				/* get channel parameters */
				type = ((m_sndregs[base_offset + 1] >> 4) & 0x0f);
				bank = m_banks[((m_sndregs[base_offset + 1] >> 0) & 0x03)];
				vol_l = ((m_sndregs[base_offset + 1] >> 12) & 0x0f);
				vol_r = ((m_sndregs[base_offset + 1] >> 8) & 0x0f);
				end_pos = (m_sndregs[base_offset + 2] << 8) - 1; // get rid of clicks and pops -dink

				/* generates output data (range 0x00000..0xffff) */
				if (type == 0x08){
					/* PCM, 8 bits mono */
					data = m_snd_data[bank + end_pos + m_sndregs[base_offset + 3]];
					ch_data_l = m_volume_table[vol_l][data];
					ch_data_r = m_volume_table[vol_r][data];

					m_sndregs[base_offset + 3]--;
				} else if (type == 0x0c){
					/* PCM, 8 bits stereo */
					data = m_snd_data[bank + end_pos + m_sndregs[base_offset + 3]];
					ch_data_l = m_volume_table[vol_l][data];

					m_sndregs[base_offset + 3]--;

					if (m_sndregs[base_offset + 3] > 0){
						data = m_snd_data[bank + end_pos + m_sndregs[base_offset + 3]];
						ch_data_r = m_volume_table[vol_r][data];

						m_sndregs[base_offset + 3]--;
					}
				} else {
					//LOG_SOUND(("(GAE1) Playing unknown sample format in channel: %02d, type: %02x, bank: %02x, end: %08x, Length: %04x\n", ch, type, bank, end_pos, m_sndregs[base_offset + 3]));
					channel->active = 0;
				}

				/* check if the current sample has finished playing */
				if (m_sndregs[base_offset + 3] == 0){
					if (channel->loop == 0){    /* if no looping, we're done */
						channel->active = 0;
					} else {                    /* if we're looping, swap chunks */
						channel->chunkNum = (channel->chunkNum + 1) & 0x01;

						/* if the length of the next chunk is 0, we're done */
						if (m_sndregs[ch*8 + channel->chunkNum*4 + 3] == 0){
							channel->active = 0;
						}
					}
				}
				channel->ch_data_l = ch_data_l; // for ramp.
				channel->ch_data_r = ch_data_r;
			} else {
#if 0
				// ramp to zero.  games I tested don't seem to need it, keeping just in-case
				if (channel->ch_data_l > 0) {
					channel->ch_data_l -= ((channel->ch_data_l >  4) ? 4 : 1);
				} else if (channel->ch_data_l < 0) {
					channel->ch_data_l += ((channel->ch_data_l < -4) ? 4 : 1);
				}

				if (channel->ch_data_r > 0) {
					channel->ch_data_r -= ((channel->ch_data_r >  4) ? 4 : 1);
				} else if (channel->ch_data_r < 0) {
					channel->ch_data_r += ((channel->ch_data_r < -4) ? 4 : 1);
				}

				ch_data_l = channel->ch_data_l;
				ch_data_r = channel->ch_data_r;
#endif
			}

			/* add the contribution of this channel to the current data output */
			output_l = (output_l + ch_data_l);
			output_r = (output_r + ch_data_r);
		}

		output_l = (INT32)(double)(output_l * 0.50); // lower the volumes so mixing doesn't crackle
		output_r = (INT32)(double)(output_r * 0.50);

		if (gaelcosnd_mono) {
			output_l = output_r + output_l;
			output_r = output_l;
		}

		if (gaelcosnd_swap_lr) {
			*lmix++ = output_r;
			*rmix++ = output_l;
		} else {
			*lmix++ = output_l;
			*rmix++ = output_r;
		}
	}

	INT16 *pBufL = sample_buffer + (8000 * 0) + 5;
	INT16 *pBufR = sample_buffer + (8000 * 1) + 5;

	for (INT32 i = (nFractionalPosition & 0xFFFF0000) >> 15; i < (samples << 1); i += 2, nFractionalPosition += nSampleSize) {
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

		outputs[i + 0] = BURN_SND_CLIP(nTotalLeftSample);
		outputs[i + 1] = BURN_SND_CLIP(nTotalRightSample);
	}

	if (samples >= nBurnSoundLen) {
		INT32 nExtraSamples = nSamplesNeeded - (nFractionalPosition >> 16);
		//bprintf(0, _T("%d, "), nExtraSamples);

		for (INT32 i = -4; i < nExtraSamples; i++) {
			pBufL[i] = pBufL[(nFractionalPosition >> 16) + i];
			pBufR[i] = pBufR[(nFractionalPosition >> 16) + i];
		}

		nFractionalPosition &= 0xFFFF;

		nPosition = nExtraSamples;
	}
}

/*============================================================================
                        CG-1V/GAE1 Read Handler
  ============================================================================*/

UINT16 gaelcosnd_r(INT32 offset)
{
	//LOG_READ_WRITES(("%s: (GAE1): read from %04x\n", machine().describe_context(), offset));

	return m_sndregs[offset];
}

/*============================================================================
                        CG-1V/GAE1 Write Handler
  ============================================================================*/

void gaelcosnd_w(INT32 offset, UINT16 data)
{
	gaelco_sound_channel *channel = &m_channel[offset >> 3];

	m_sndregs[offset] = data;

	switch (offset & 0x07)
	{
	case 0x03:
		// if sample end position isn't 0, and length isn't 0
		if ((m_sndregs[offset - 1] != 0) && (data != 0))
		{
			//LOG_SOUND(("(GAE1) Playing or Queuing 1st chunk in channel: %02d, type: %02x, bank: %02x, end: %08x, Length: %04x\n", offset >> 3, (m_sndregs[offset - 2] >> 4) & 0x0f, m_sndregs[offset - 2] & 0x03, m_sndregs[offset - 1] << 8, data));

			channel->loop = 1;

			if (!channel->active)
			{
				channel->chunkNum = 0;
			}

			channel->active = 1;
		}
		else
		{
			//channel->loop = 0;
			channel->active = 0;
		}

		break;

	case 0x07:
		// if sample end position isn't 0, and length isn't 0
		if ((m_sndregs[offset - 1] != 0) && (data != 0))
		{
			//LOG_SOUND(("(GAE1) Playing or Queuing 2nd chunk in channel: %02d, type: %02x, bank: %02x, end: %08x, Length: %04x\n", offset >> 3, (m_sndregs[offset - 2] >> 4) & 0x0f, m_sndregs[offset - 2] & 0x03, m_sndregs[offset - 1] << 8, data));

			channel->loop = 1;

			if (!channel->active)
			{
				channel->chunkNum = 1;
			}

			channel->active = 1;
		}
		else
		{
			channel->loop = 0;
			// channel->active = 0;
		}

		break;
	}
}

/*============================================================================
                        CG-1V/GAE1 Init / Close
  ============================================================================*/

void gaelcosnd_start(UINT8 *soundrom, INT32 offs1, INT32 offs2, INT32 offs3, INT32 offs4)
{
	m_snd_data = soundrom;

	gaelco_set_bank_offsets(offs1, offs2, offs3, offs4);

	/* init volume table */
	for (INT32 vol = 0; vol < GAELCO_VOLUME_LEVELS; vol++){
		for (INT32 j = -128; j <= 127; j++){
			m_volume_table[vol][(j ^ 0x80) & 0xff] = (vol*j*256)/(GAELCO_VOLUME_LEVELS - 1);
		}
	}

	gaelcosnd_reset();

	// for resampling
	sample_buffer = (INT16 *)BurnMalloc(8000 * sizeof(INT16) * 2); // more than enough :)
	memset(sample_buffer, 0, 8000 * sizeof(INT16) * 2);

	if (nBurnSoundRate) nSampleSize = (UINT32)8000 * (1 << 16) / nBurnSoundRate;
	nFractionalPosition = 0;
	nPosition = 0;

	gaelcosnd_initted = 1;
}

void gaelcosnd_monoize()
{
	gaelcosnd_mono = 1;
}

void gaelcosnd_swaplr()
{
	gaelcosnd_swap_lr = 1;
}

void gaelcosnd_exit()
{
	if (!gaelcosnd_initted) return;

	BurnFree(sample_buffer);
	sample_buffer = NULL;
	m_snd_data = NULL;
	gaelcosnd_mono = 0;
	gaelcosnd_swap_lr = 0;
}

void gaelcosnd_scan(INT32 nAction, INT32 *)
{
	SCAN_VAR(m_channel);
	SCAN_VAR(m_sndregs);

	if (nAction & ACB_WRITE && ~nAction & ACB_RUNAHEAD) {
		nFractionalPosition = 0;
		nPosition = 0;
		memset(sample_buffer, 0, 8000 * sizeof(INT16) * 2);
	}
}

void gaelcosnd_reset()
{
	memset(&m_channel, 0, sizeof(m_channel));
	memset(&m_sndregs, 0, sizeof(m_sndregs));
}
