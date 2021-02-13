#include "driver.h"
#include "ymf271.h"
#include "timer.h"

void BurnYMF271Write(INT32 nAddress, UINT8 nValue);
UINT8 BurnYMF271Read(INT32 nAddress);

INT32 BurnYMF271Init(INT32 nClockFrequency, UINT8 *rom, INT32 romsize, void (*IRQCallback)(INT32, INT32), INT32 nAdd);
INT32 BurnYMF271Init(INT32 nClockFrequency, UINT8 *rom, INT32 romsize, void (*IRQCallback)(INT32, INT32), INT32 (*StreamCallback)(INT32), INT32 nAdd);
void BurnYMF271SetRoute(INT32 nIndex, double nVolume, INT32 nRouteDir);
void BurnYMF271Reset();
void BurnYMF271Exit();
void BurnYMF271Update(INT32 nSegmentEnd);
void BurnYMF271Scan(INT32 nAction, INT32* pnMin);

#define BURN_SND_YMF271_YMF271_ROUTE_1		0
#define BURN_SND_YMF271_YMF271_ROUTE_2		1
#define BURN_SND_YMF271_YMF271_ROUTE_3      2
#define BURN_SND_YMF271_YMF271_ROUTE_4      3

#define BurnYMF271SetAllRoutes(v, d)								\
	BurnYMF271SetRoute(BURN_SND_YMF271_YMF271_ROUTE_1, v, d);		\
	BurnYMF271SetRoute(BURN_SND_YMF271_YMF271_ROUTE_2, v, d);   	\
	BurnYMF271SetRoute(BURN_SND_YMF271_YMF271_ROUTE_3, v, d);		\
	BurnYMF271SetRoute(BURN_SND_YMF271_YMF271_ROUTE_4, v, d);
