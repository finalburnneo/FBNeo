#include "driver.h"
extern "C" {
 #include "fmopl.h"
}
#include "timer.h"

INT32 BurnTimerUpdateYM3812(INT32 nCycles);
void BurnTimerEndFrameYM3812(INT32 nCycles);
void BurnTimerUpdateEndYM3812();
INT32 BurnTimerAttachYM3812(cpu_core_config *ptr, INT32 nClockspeed);

extern "C" void BurnYM3812UpdateRequest();

INT32 BurnYM3812Init(INT32 num, INT32 nClockFrequency, OPL_IRQHANDLER IRQCallback, INT32 bAddSignal);
INT32 BurnYM3812Init(INT32 num, INT32 nClockFrequency, OPL_IRQHANDLER IRQCallback, INT32 (*StreamCallback)(INT32), INT32 bAddSignal);
void BurnYM3812SetRoute(INT32 nChip, INT32 nIndex, double nVolume, INT32 nRouteDir);
void BurnYM3812Reset();
void BurnYM3812Exit();
extern void (*BurnYM3812Update)(INT16* pSoundBuf, INT32 nSegmentEnd);
void BurnYM3812Scan(INT32 nAction, INT32* pnMin);

#define BURN_SND_YM3812_ROUTE			0

#define BurnYM3812Read(i, a) YM3812Read(i, a)

#if defined FBA_DEBUG
	#define BurnYM3812Write(i, a, n) if (!DebugSnd_YM3812Initted) bprintf(PRINT_ERROR, _T("BurnYM3812Write called without init\n")); YM3812Write(i, a, n)
#else
	#define BurnYM3812Write(i, a, n) YM3812Write(i, a, n)
#endif
