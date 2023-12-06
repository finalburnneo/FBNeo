#include "cps.h"
#include "burn_ym2151.h"

// CPS1 sound Mixing

INT32 bPsmOkay = 0;										// 1 if the module is okay

static INT32 nPos;

INT32 PsmInit()
{
	INT32 nRate, nRet;
	bPsmOkay = 0;										// not OK yet

	if (nBurnSoundRate > 0) {
		nRate = nBurnSoundRate;
	} else {
		nRate = 11025;
	}

	BurnYM2151InitBuffered(3579540, 1, NULL, 1);		// Init FM sound chip

	//BurnYM2151SetAllRoutes(0.35, BURN_SND_ROUTE_BOTH);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.35, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.35, BURN_SND_ROUTE_RIGHT);

	// Init ADPCM
	MSM6295ROM = CpsAd;
	if (Forgottn) {
		nRet = MSM6295Init(0, 6061, 0);
	} else {
		nRet = MSM6295Init(0, 7576, 0);
	}
	MSM6295SetRoute(0, 0.30, BURN_SND_ROUTE_BOTH);

	if (nRet!=0) {
		PsmExit(); return 1;
	}

	bPsmOkay = 1;										// OK

	return 0;
}

INT32 PsmExit()
{
	bPsmOkay = 0;

	MSM6295Exit(0);

	BurnYM2151Exit();									// Exit FM sound chip
	return 0;
}

void PsmNewFrame()
{
	nPos = 0;
}

INT32 PsmUpdate(INT32 nEnd)
{
	if (bPsmOkay == 0 || pBurnSoundOut == NULL) {
		return 1;
	}

	if (nEnd <= nPos) {
		return 0;
	}
	if (nEnd > nBurnSoundLen) {
		nEnd = nBurnSoundLen;
	}

	// Render FM
	//BurnYM2151Render(pBurnSoundOut + (nPos << 1), nEnd - nPos);

	// Render ADPCM
	//MSM6295Render(0, pBurnSoundOut + (nPos << 1), nEnd - nPos);

	nPos = nEnd;

	return 0;
}

INT32 PsmUpdateEnd()
{
	if (bPsmOkay == 0 || pBurnSoundOut == NULL) {
		return 1;
	}

	MSM6295Render(pBurnSoundOut, nBurnSoundLen);
	BurnYM2151Render(pBurnSoundOut, nBurnSoundLen);

	return 0;
}
