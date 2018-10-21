#include "burnint.h"
#include "burn_sound.h"
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

static INT16 dac_lastin_r  = 0;
static INT16 dac_lastout_r = 0;
static INT16 dac_lastin_l  = 0;
static INT16 dac_lastout_l = 0;

// BurnSoundDCFilterReset() is called automatically @ game init, no need to call this in-driver.
void BurnSoundDCFilterReset()
{
	dac_lastin_r = dac_lastout_r = 0;
	dac_lastin_l = dac_lastout_l = 0;
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

void BurnSoundClear()
{
	if (pBurnSoundOut)
		memset(pBurnSoundOut, 0, nBurnSoundLen * 2 * sizeof(INT16));
}
