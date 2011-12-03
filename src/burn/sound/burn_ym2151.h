// burn_ym2151.h
#include "driver.h"
extern "C" {
 #include "ym2151.h"
}

INT32 BurnYM2151Init(INT32 nClockFrequency, float nVolume);
void BurnYM2151Reset();
void BurnYM2151Exit();
extern void (*BurnYM2151Render)(INT16* pSoundBuf, INT32 nSegmentLength);
void BurnYM2151Scan(INT32 nAction);

static inline void BurnYM2151SelectRegister(const UINT8 nRegister)
{
	extern UINT32 nBurnCurrentYM2151Register;

	nBurnCurrentYM2151Register = nRegister;
}

static inline void BurnYM2151WriteRegister(const UINT8 nValue)
{
	extern UINT32 nBurnCurrentYM2151Register;
	extern UINT8 BurnYM2151Registers[0x0100];

	BurnYM2151Registers[nBurnCurrentYM2151Register] = nValue;
	YM2151WriteReg(0, nBurnCurrentYM2151Register, nValue);
}

#define BurnYM2151ReadStatus() YM2151ReadStatus(0)
#define BurnYM2151SetIrqHandler(h) YM2151SetIrqHandler(0, h)
#define BurnYM2151SetPortHandler(h) YM2151SetPortWriteHandler(0, h)
