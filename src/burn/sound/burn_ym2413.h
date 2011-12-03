// burn_ym2413.h
#include "driver.h"
extern "C" {
 #include "ym2413.h"
}

INT32 BurnYM2413Init(INT32 nClockFrequency, float nVolume);
void BurnYM2413Reset();
void BurnYM2413Exit();
extern void (*BurnYM2413Render)(INT16* pSoundBuf, INT32 nSegmentLength);
void BurnYM2413Scan(INT32 nAction);
void BurnYM2413IncreaseVolume(INT32 nFactor);
void BurnYM2413DecreaseVolume(INT32 nFactor);

#define BurnYM2413Read(a) YM2413Read(0, a)
#define BurnYM2413Write(a, d) YM2413Write(0, a, d)
//#define BurnYM2413SetUpdateHandler(h) YM2413SetUpdateHandler(0, h)

