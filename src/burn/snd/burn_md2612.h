// burn_md2612.h
#include "driver.h"
extern "C" {
 #include "ym2612.h"
}
extern "C" void BurnMD2612UpdateRequest();

INT32 BurnMD2612Init(INT32 num, INT32 bIsPal, INT32 (*StreamCallback)(INT32), INT32 bAddSignal);
void BurnMD2612SetRoute(INT32 nChip, INT32 nIndex, double nVolume, INT32 nRouteDir);
void BurnMD2612Reset();
void BurnMD2612Exit();
extern void (*BurnMD2612Update)(INT16* pSoundBuf, INT32 nSegmentEnd);
void BurnMD2612Scan(INT32 nAction, INT32* pnMin);

#define BURN_SND_MD2612_MD2612_ROUTE_1		0
#define BURN_SND_MD2612_MD2612_ROUTE_2		1

#define BurnMD2612SetAllRoutes(i, v, d)								\
	BurnMD2612SetRoute(i, BURN_SND_MD2612_MD2612_ROUTE_1, v, d);	\
	BurnMD2612SetRoute(i, BURN_SND_MD2612_MD2612_ROUTE_2, v, d);
	
#define BurnMD2612Read(i, a) MDYM2612Read()

#if defined FBA_DEBUG
	#define BurnMD2612Write(i, a, n) if (!DebugSnd_YM2612Initted) bprintf(PRINT_ERROR, _T("BurnMD2612Write called without init\n")); MDYM2612Write(a, n)
#else
	#define BurnMD2612Write(i, a, n) MDYM2612Write(a, n)
#endif
