// burn_ym2151.h
#include "driver.h"
extern "C" {
 #include "ym2151.h"
}

#include "timer.h"

void BurnYM2151SetMultiChip(INT32 bYes); // set before init for 2 ym2151's!
INT32 BurnYM2151Init(INT32 nClockFrequency);
INT32 BurnYM2151Init(INT32 nClockFrequency, INT32 use_timer);
void BurnYM2151InitBuffered(INT32 nClockFrequency, INT32 use_timer, INT32 (*StreamCallback)(INT32), INT32 bAdd);
void BurnYM2151SetRoute(INT32 chip, INT32 nIndex, double nVolume, INT32 nRouteDir);
void BurnYM2151SetRoute(INT32 nIndex, double nVolume, INT32 nRouteDir);
void BurnYM2151SetAllRoutes(double vol, INT32 route);
void BurnYM2151SetAllRoutes(INT32 chip, double vol, INT32 route);
void BurnYM2151Reset();
void BurnYM2151Exit();
void BurnYM2151Render(INT16* pSoundBuf, INT32 nSegmentLength);
void BurnYM2151Scan(INT32 nAction, INT32 *pnMin);
void BurnYM2151SetInterleave(INT32 nInterleave);

void BurnYM2151Write(INT32 chip, INT32 offset, const UINT8 nData);
void BurnYM2151Write(INT32 offset, const UINT8 nData);
void BurnYM2151SelectRegister(const UINT8 nRegister);
void BurnYM2151WriteRegister(const UINT8 nValue);
UINT8 BurnYM2151Read(INT32 chip);
UINT8 BurnYM2151Read();

void BurnYM2151SetIrqHandler(void (*irq_cb)(INT32));
void BurnYM2151SetIrqHandler(INT32 chip, void (*irq_cb)(INT32));

void BurnYM2151SetPortHandler(write8_handler port_cb);
void BurnYM2151SetPortHandler(INT32 chip, write8_handler port_cb);

#define BURN_SND_YM2151_YM2151_ROUTE_1		0
#define BURN_SND_YM2151_YM2151_ROUTE_2		1

