#include "burnint.h"
// #include "burn_sound.h" // included in burnint.h, above!
#include "timer.h"

INT16 Precalc[4096 * 4];

// Routine used to precalculate the table used for interpolation
INT32 cmc_4p_Precalc()
{
	INT32 a, x, x2, x3;

	for (a = 0; a < 4096; a++) {
		x  = a  * 4;			// x = 0..16384
		x2 = x  * x / 16384;	// pow(x, 2);
		x3 = x2 * x / 16384;	// pow(x, 3);

		Precalc[a * 4 + 0] = (INT16)(-x / 3 + x2 / 2 - x3 / 6);
		Precalc[a * 4 + 1] = (INT16)(-x / 2 - x2     + x3 / 2 + 16384);
		Precalc[a * 4 + 2] = (INT16)( x     + x2 / 2 - x3 / 2);
		Precalc[a * 4 + 3] = (INT16)(-x / 6 + x3 / 6);
	}

	return 0;
}

void BurnSoundInit()
{
	cmc_4p_Precalc();
}

static INT16 dac_lastin_r  = 0;
static INT16 dac_lastout_r = 0;
static INT16 dac_lastin_l  = 0;
static INT16 dac_lastout_l = 0;

static INT32 limiting = 0;

// BurnSoundDCFilterReset() is called automatically @ game init, no need to call this in-driver.
void BurnSoundDCFilterReset()
{
	dac_lastin_r = dac_lastout_r = 0;
	dac_lastin_l = dac_lastout_l = 0;
	limiting = 0;
}

// Runs a dc-blocking filter on pBurnSoundOut - see drv/pre90s/d_mappy.cpp for usage.
void BurnSoundDCFilter()
{
	for (INT32 i = 0; i < nBurnSoundLen; i++) {
		INT16 r = pBurnSoundOut[i*2+0];
		INT16 l = pBurnSoundOut[i*2+1];

		INT16 outr = r - dac_lastin_r + 0.995 * dac_lastout_r;
		INT16 outl = l - dac_lastin_l + 0.995 * dac_lastout_l;

		dac_lastin_r = r;
		dac_lastout_r = outr;
		dac_lastin_l = l;
		dac_lastout_l = outl;
		pBurnSoundOut[i*2+0] = outr;
		pBurnSoundOut[i*2+1] = outl;
	}
}

void BurnSoundTweakVolume(INT16 *sndout, INT32 len, double volume)
{
	INT32 clip = 0;

	for (INT32 i = 0; i < (len * 2); i++) {
		INT32 sample = sndout[i] * volume;
		if (sample > 0x7fff) clip = 1;
		if (sample < -0x8000) clip = 1;
		sndout[i] = BURN_SND_CLIP(sample);
	}
	if (clip) bprintf(0, _T("BurnSoundTweakVolume(): CLIPPING @ frame %x\n"), nCurrentFrame);
}

void BurnSoundLimiter(INT16 *sndout, INT32 len, double percent, double make_up_gain)
{
	const INT32 limit_samples = nBurnSoundRate * 0.200; // 200ms (response time)

	const INT32 sample_pos_limit = 0x7fff * percent;
	const INT32 sample_neg_limit = -0x8000 * percent;

	static INT32 mode = -1; // -1 startup, 0 attack, 1 release
	static INT32 envelope = 0;
	static INT32 percent_int = percent * 100;

	for (INT32 i = 0; i < len; i++) {
		INT32 sample_l = (sndout[i * 2 + 0]);
		INT32 sample_r = (sndout[i * 2 + 1]);

		if (sample_l > sample_pos_limit || sample_l < sample_neg_limit ||
			sample_r > sample_pos_limit || sample_r < sample_neg_limit)
		{
			limiting = limit_samples;
		}

		if (limiting > 0) {
			switch (mode) {
				case -1: { // envelope start-up
					envelope = 100; // 1.0;
					mode++;
					//NO break; - go straight to attack!
				}
				case 0: { // attack
					if (envelope == percent_int) {
						// limit hit!
						//bprintf(0, _T("Attack ends! %d  %d\n"), envelope, percent_int);
						mode++;
					} else {
						// decay volume towards the limit
						envelope--;
					}
					break;
				}
				case 1: {  break; }
			}
			sample_l = sample_l * envelope / 100;
			sample_r = sample_r * envelope / 100;

			limiting--;
		} else {
			mode = -1;
		}

		sndout[i * 2 + 0] = BURN_SND_CLIP(sample_l * make_up_gain);
		sndout[i * 2 + 1] = BURN_SND_CLIP(sample_r * make_up_gain);
	}
}

void BurnSoundSwapLR(INT16 *sndout, INT32 len)
{
	for (INT32 i = 0; i < len; i++) {
		INT32 sample_l = (sndout[i * 2 + 0]);
		INT32 sample_r = (sndout[i * 2 + 1]);

		sndout[i * 2 + 0] = sample_r;
		sndout[i * 2 + 1] = sample_l;
	}
}

void BurnSoundClear()
{
	if (pBurnSoundOut) {
		memset(pBurnSoundOut, 0, nBurnSoundLen * 2 * sizeof(INT16));
	}
}
