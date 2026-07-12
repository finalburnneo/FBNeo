// Crazy Climber / Moon Shuttle sample player.

#include "gal.h"

static INT32 sample_num = 0;
static INT32 sample_freq = 0;
static INT32 sample_vol = 0;

static INT32 sample_len = 0;
static INT32 sample_pos = -1; // -1 not playing, 0 start

static INT16 *samplebuf = NULL;
static UINT8 *mshuttle_samples = NULL;

// 4bit decode from mame
#define SAMPLE_CONV4(a) (0x1111*((a&0x0f))-0x8000)

void cclimber_sample_init()
{
	samplebuf = (INT16*)BurnMalloc(0x10000 * sizeof(INT16));
	mshuttle_samples = BurnMalloc(0x2000);
}

UINT8 *cclimber_sample_rom()
{
	return mshuttle_samples;
}

void cclimber_sample_exit()
{
	BurnFree(mshuttle_samples);
	BurnFree(samplebuf);
	sample_num = sample_freq = sample_vol = sample_len = 0;
	sample_pos = -1;
}

void cclimber_sample_num(UINT32, UINT32 data)
{
	sample_num = data;
}

void cclimber_sample_w_freq(UINT8 data)
{
	sample_freq = 3072000 / 4 / (256 - data);
}

void cclimber_sample_w_vol(UINT8 data)
{
	sample_vol = data & 0x1f;
}

void cclimber_sample_scan()
{
	SCAN_VAR(sample_num);
	SCAN_VAR(sample_freq);
	SCAN_VAR(sample_vol);
	SCAN_VAR(sample_len);
	SCAN_VAR(sample_pos);
}

void cclimber_sample_start()
{
	const UINT8 *rom = mshuttle_samples;

	if (!rom) return;

	INT32 len = 0;
	INT32 start = 32 * sample_num;

	while (start + len < 0x2000 && rom[start+len] != 0x70)
	{
		INT32 sample;

		sample = (rom[start + len] & 0xf0) >> 4;
		samplebuf[2*len] = SAMPLE_CONV4(sample) * sample_vol / 31;

		sample = rom[start + len] & 0x0f;
		samplebuf[2*len + 1] = SAMPLE_CONV4(sample) * sample_vol / 31;

		len++;
	}
	sample_len = len * 2;
	sample_pos = 0;
}

void cclimber_sample_render(INT16 *buffer, INT32 nLen)
{
	if (sample_pos < 0) return; // stopped

	if ((sample_pos >> 16) >= 0x10000 ) {
		sample_pos = -1; // stop
		return;
	}

	INT32 step = (sample_freq << 16) / nBurnSoundRate;
	INT32 pos = 0;
	INT16 *rom = samplebuf;

	while (pos < nLen)
	{
		INT32 sample = (INT32)(rom[(sample_pos >> 16)] * 0.2);

		buffer[0] = BURN_SND_CLIP((INT32)(buffer[0] + sample));
		buffer[1] = BURN_SND_CLIP((INT32)(buffer[1] + sample));

		sample_pos += step;

		buffer+=2;
		pos++;

		if (sample_pos >= 0xfff0000 || (sample_pos >> 16) >= sample_len) {
			sample_pos = -1; // stop
			break;
		}
	}
}
