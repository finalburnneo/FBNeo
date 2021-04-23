// burn_ym2151.h
#include "driver.h"
extern "C" {
 #include "ym2151.h"
}

#include "timer.h"

INT32 BurnYM2151Init(INT32 nClockFrequency);
INT32 BurnYM2151Init(INT32 nClockFrequency, INT32 use_timer);
void BurnYM2151InitBuffered(INT32 nClockFrequency, INT32 use_timer, INT32 (*StreamCallback)(INT32), INT32 bAdd);
void BurnYM2151SetRoute(INT32 nIndex, double nVolume, INT32 nRouteDir);
void BurnYM2151Reset();
void BurnYM2151Exit();
void BurnYM2151Render(INT16* pSoundBuf, INT32 nSegmentLength);
void BurnYM2151Scan(INT32 nAction, INT32 *pnMin);
void BurnYM2151SetInterleave(INT32 nInterleave);

void BurnYM2151Write(INT32 offset, const UINT8 nData);
void BurnYM2151SelectRegister(const UINT8 nRegister);
void BurnYM2151WriteRegister(const UINT8 nValue);
UINT8 BurnYM2151Read();


#if defined FBNEO_DEBUG
	#define BurnYM2151SetIrqHandler(h) do { if (!DebugSnd_YM2151Initted) bprintf(PRINT_ERROR, _T("BurnYM2151SetIrqHandler called without init\n")); YM2151SetIrqHandler(0, h); } while (0);
	#define BurnYM2151SetPortHandler(h) do { if (!DebugSnd_YM2151Initted) bprintf(PRINT_ERROR, _T("BurnYM2151SetPortHandler called without init\n")); YM2151SetPortWriteHandler(0, h); } while (0);
#else
	#define BurnYM2151SetIrqHandler(h) YM2151SetIrqHandler(0, h)
	#define BurnYM2151SetPortHandler(h) YM2151SetPortWriteHandler(0, h)
#endif

#define BURN_SND_YM2151_YM2151_ROUTE_1		0
#define BURN_SND_YM2151_YM2151_ROUTE_2		1

#define BurnYM2151SetAllRoutes(v, d)							\
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, v, d);	\
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, v, d);
