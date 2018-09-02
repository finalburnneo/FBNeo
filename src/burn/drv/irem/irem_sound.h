#include "m6800_intf.h"
#include "msm5205.h"
#include "ay8910.h"

void IremSoundWrite(UINT8 d);
extern UINT8 IremSlaveMSM5205VClckReset;
void IremSoundClockSlave();

INT32 IremSoundReset();
void IremSoundInit(UINT8 *pZ80ROM, INT32 nType, INT32 nZ80Clock);
void IremSoundInit(UINT8 *pZ80ROM, INT32 nType, INT32 nZ80Clock, void (*ay8910cb)(UINT8));
INT32 IremSoundExit();
INT32 IremSoundScan(INT32 nAction, INT32 *pnMin);
