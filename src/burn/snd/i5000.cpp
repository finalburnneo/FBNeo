// license:BSD-3-Clause
// copyright-holders:hap
/***************************************************************************

    i5000.c - Imagetek I5000 sound emulator

    Imagetek I5000 is a multi-purpose chip, this covers the sound part.
    No official documentation is known to exist. It seems to be a simple
    16-channel ADPCM player.

    TODO:
    - verify that ADPCM is the same as standard OKI ADPCM
    - verify volume balance
    - sample command 0x0007
    - any more sound formats than 3-bit and 4-bit ADPCM?

***************************************************************************/

#include "burnint.h"
#include "i5000.h"
#include "math.h" // floor, pow

struct channel_t
{
	bool is_playing;
	INT32 signal;
	INT32 step;
	UINT32 address;
	INT32 freq_timer;
	INT32 freq_base;
	INT32 freq_min;
	UINT16 sample;
	UINT8 shift_pos;
	UINT8 shift_amount;
	UINT8 shift_mask;
	INT32 vol_r;
	INT32 vol_l;
	INT32 output_r;
	INT32 output_l;
};

static channel_t channels[16];
static UINT16 regs[0x80];

static UINT16 *rom_base;
static UINT32 rom_mask;
static INT32 lut_volume[0x100];
static const INT8 s_index_shift[8] = { -1, -1, -1, -1, 8, 4, 2, 1 };
static INT32 s_diff_lookup[49*16];

// cheap resampler-izer
static INT32 sample_rate;
static INT16 *mixer_buffer_left;
static INT16 *mixer_buffer_right;

static UINT32 nSampleSize;
static INT32 nFractionalPosition;
static INT32 nPosition;

static void compute_tables()
{
	static const INT8 nbl2bit[16][4] =
	{
		{ 1, 0, 0, 0}, { 1, 0, 0, 1}, { 1, 0, 1, 0}, { 1, 0, 1, 1},
		{ 1, 1, 0, 0}, { 1, 1, 0, 1}, { 1, 1, 1, 0}, { 1, 1, 1, 1},
		{-1, 0, 0, 0}, {-1, 0, 0, 1}, {-1, 0, 1, 0}, {-1, 0, 1, 1},
		{-1, 1, 0, 0}, {-1, 1, 0, 1}, {-1, 1, 1, 0}, {-1, 1, 1, 1}
	};

	// loop over all possible steps
	for (INT32 step = 0; step <= 48; step++)
	{
		// compute the step value
		INT32 stepval = floor(16.0 * pow(11.0 / 10.0, (double)step));

		// loop over all nibbles and compute the difference
		for (INT32 nib = 0; nib < 16; nib++)
		{
			s_diff_lookup[step*16 + nib] = nbl2bit[nib][0] * (stepval * nbl2bit[nib][1] + stepval/2 * nbl2bit[nib][2] + stepval/4 * nbl2bit[nib][3] + stepval/8);
		}
	}
}

void i5000sndInit(UINT8 *rom, INT32 clock, INT32 length)
{
	memset(channels, 0, sizeof(channels));
	memset(regs, 0, sizeof(regs));

	// fill volume table
	double div = 1.032;
	double vol = 2047.0;
	for (INT32 i = 0; i < 0x100; i++)
	{
		lut_volume[i] = vol + 0.5;
		vol /= div;
	}
	lut_volume[0xff] = 0;

	compute_tables();

	rom_base = (UINT16*)rom;
	rom_mask = (length / 2) - 1;

	sample_rate = clock / 0x400;

	// for resampling
	nSampleSize = (UINT32)sample_rate * (1 << 16) / nBurnSoundRate;
	nFractionalPosition = 0;
	nPosition = 0;


	mixer_buffer_left = (INT16*)BurnMalloc(2 * sizeof(INT16) * sample_rate);
	mixer_buffer_right = mixer_buffer_left + sample_rate;
}

void i5000sndExit()
{
	if (mixer_buffer_left) {
		BurnFree(mixer_buffer_left);
		mixer_buffer_left = mixer_buffer_right = NULL;
	}
}

void i5000sndReset()
{
	// stop playing
	i5000sndWrite(0x43, 0xffff);

	// reset channel regs
	for (INT32 i = 0; i < 0x40; i++) {
		i5000sndWrite(i, 0);
	}

	for (INT32 i = 0; i < 16; i++) {
		channels[i].signal = -2;
		channels[i].step = 0;
	}
}

static bool read_sample(int ch)
{
	channels[ch].shift_pos &= 0xf;
	channels[ch].sample = rom_base[channels[ch].address];
	channels[ch].address = (channels[ch].address + 1) & rom_mask;

	// handle command
	if (channels[ch].sample == 0x7f7f)
	{
		UINT16 cmd = rom_base[channels[ch].address];
		channels[ch].address = (channels[ch].address + 1) & rom_mask;

		// volume envelope? or loop sample?
		if ((cmd & 0x00ff) == 0x0007)
		{
			return false;
		}
		else return false;
	}

	return true;
}

static inline INT16 clock(INT32 ch, UINT8 nibble)
{
	// update the signal
	channels[ch].signal += s_diff_lookup[channels[ch].step * 16 + (nibble & 15)];

	// clamp to the maximum
	if (channels[ch].signal > 2047)
		channels[ch].signal = 2047;
	else if (channels[ch].signal < -2048)
		channels[ch].signal = -2048;

	// adjust the step size and clamp
	channels[ch].step += s_index_shift[nibble & 7];
	if (channels[ch].step > 48)
		channels[ch].step = 48;
	else if (channels[ch].step < 0)
		channels[ch].step = 0;

	// return the signal
	return channels[ch].signal;
}

void i5000sndUpdate(INT16 *output, INT32 length)
{
	INT32 nSamplesNeeded = ((((((sample_rate * 1000) / nBurnFPS) * length) / nBurnSoundLen)) / 10) + 1;
	if (nBurnSoundRate < 44100) nSamplesNeeded += 2; // so we don't end up with negative nPosition below

	INT16 *lmix = mixer_buffer_left  + 5 + nPosition;
	INT16 *rmix = mixer_buffer_right + 5 + nPosition;

	memset(lmix, 0, nSamplesNeeded * sizeof(INT16));
	memset(rmix, 0, nSamplesNeeded * sizeof(INT16));

	for (INT32 i = 0; i < (nSamplesNeeded - nPosition); i++)
	{
		INT32 mix_l = 0;
		INT32 mix_r = 0;

		// loop over all channels
		for (INT32 ch = 0; ch < 16; ch++)
		{
			if (!channels[ch].is_playing)
				continue;

			channels[ch].freq_timer -= channels[ch].freq_min;
			if (channels[ch].freq_timer > 0)
			{
				mix_r += channels[ch].output_r;
				mix_l += channels[ch].output_l;
				continue;
			}
			channels[ch].freq_timer += channels[ch].freq_base;

			int adpcm_data = channels[ch].sample >> channels[ch].shift_pos;
			channels[ch].shift_pos += channels[ch].shift_amount;
			if (channels[ch].shift_pos & 0x10)
			{
				if (!read_sample(ch))
				{
					channels[ch].is_playing = false;
					continue;
				}

				adpcm_data |= (channels[ch].sample << (channels[ch].shift_amount - channels[ch].shift_pos));
			}

			adpcm_data = clock(ch,adpcm_data & channels[ch].shift_mask);

			channels[ch].output_r = adpcm_data * channels[ch].vol_r / 128;
			channels[ch].output_l = adpcm_data * channels[ch].vol_l / 128;
			mix_r += channels[ch].output_r;
			mix_l += channels[ch].output_l;
		}
		lmix[i] = mix_l / 16;
		rmix[i] = mix_r / 16;
	}

	INT16 *pBufL = mixer_buffer_left  + 5;
	INT16 *pBufR = mixer_buffer_right + 5;

	for (INT32 i = (nFractionalPosition & 0xFFFF0000) >> 15; i < (length << 1); i += 2, nFractionalPosition += nSampleSize) {
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

		nTotalLeftSample  = BURN_SND_CLIP(nTotalLeftSample);
		nTotalRightSample = BURN_SND_CLIP(nTotalRightSample);

		output[i + 0] = nTotalLeftSample;
		output[i + 1] = nTotalRightSample;
	}

	if (length >= nBurnSoundLen) {
		INT32 nExtraSamples = nSamplesNeeded - (nFractionalPosition >> 16);

		for (INT32 i = -4; i < nExtraSamples; i++) {
			pBufL[i] = pBufL[(nFractionalPosition >> 16) + i];
			pBufR[i] = pBufR[(nFractionalPosition >> 16) + i];
		}

		nFractionalPosition &= 0xFFFF;

		nPosition = nExtraSamples;
	}
}

void i5000sndWrite(INT32 offset, UINT16 data)
{
	UINT8 reg = offset;

	if (reg < 0x40)
	{
		INT32 ch = reg >> 2;
		switch (reg & 3)
		{
			case 2:
				channels[ch].freq_base = (0x1ff - (data & 0xff)) << (~data >> 8 & 3);
			break;

			case 3:
				channels[ch].vol_r = lut_volume[data & 0xff];
				channels[ch].vol_l = lut_volume[data >> 8 & 0xff];
			break;
		}
	}
	else
	{
		switch (reg)
		{
			case 0x42:
				for (INT32 ch = 0; ch < 16; ch++)
				{
					if (data & (1 << ch) && !channels[ch].is_playing)
					{
						UINT32 address = regs[ch << 2 | 1] << 16 | regs[ch << 2];
						UINT16 start = rom_base[(address + 0) & rom_mask];
						UINT16 param = rom_base[(address + 1) & rom_mask];

						if (start != 0x7f7f)
						{
							continue;
						}

						switch (param)
						{
							case 0x0104:
							case 0x0304: // same?
								channels[ch].freq_min = 0x140;
								channels[ch].shift_amount = 3;
								channels[ch].shift_mask = 0xe;
							break;

							default:
							case 0x0184:
								channels[ch].freq_min = 0x100;
								channels[ch].shift_amount = 4;
								channels[ch].shift_mask = 0xf;
							break;
						}

						channels[ch].address = (address + 4) & rom_mask;
						channels[ch].freq_timer = 0;
						channels[ch].shift_pos = 0;
						channels[ch].signal = -2;
						channels[ch].step = 0;
						channels[ch].is_playing = read_sample(ch);
					}
				}
			break;

			case 0x43:
				for (INT32 ch = 0; ch < 16; ch++)
				{
					if (data & (1 << ch))
						channels[ch].is_playing = false;
				}
			break;
		}
	}

	regs[reg] = data;
}

UINT16 i5000sndRead(INT32 offset)
{
	UINT16 ret = 0;

	switch (offset)
	{
		// channel active state
		case 0x42:
			for (INT32 ch = 0; ch < 16; ch++)
			{
				if (channels[ch].is_playing)
					ret |= (1 << ch);
			}
			break;
	}

	return ret;
}

void i5000sndScan(INT32 nAction, INT32 *pnMin)
{
	if (pnMin) {
		*pnMin = 0x029702;
	}

	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(channels);
		SCAN_VAR(regs);
	}
}
