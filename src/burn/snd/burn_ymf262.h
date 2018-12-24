#include "driver.h"
//extern "C" {
 #include "ymf262.h"
//}

#include "timer.h"

void BurnYMF262Write(INT32 nAddress, UINT8 nValue);
UINT8 BurnYMF262Read(INT32 nAddress);

INT32 BurnYMF262Init(INT32 nClockFrequency, void (*IRQCallback)(INT32, INT32), INT32 nAdd);
INT32 BurnYMF262Init(INT32 nClockFrequency, void (*IRQCallback)(INT32, INT32), INT32 (*StreamCallback)(INT32), INT32 nAdd);
void BurnYMF262SetRoute(INT32 nIndex, double nVolume, INT32 nRouteDir);
void BurnYMF262Reset();
void BurnYMF262Exit();
void BurnYMF262Update(INT32 nSegmentEnd);
void BurnYMF262Scan(INT32 nAction, INT32* pnMin);

#define BURN_SND_YMF262_YMF262_ROUTE_1		0
#define BURN_SND_YMF262_YMF262_ROUTE_2		1

#define BurnYMF262SetAllRoutes(v, d)								\
	BurnYMF262SetRoute(BURN_SND_YMF262_YMF262_ROUTE_1, v, d);	\
	BurnYMF262SetRoute(BURN_SND_YMF262_YMF262_ROUTE_2, v, d);
