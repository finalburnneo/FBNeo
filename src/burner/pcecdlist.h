#ifndef PCECDLIST_H_
#define PCECDLIST_H_

#include "cdlist.h"

#ifdef BUILD_PCE
INT32  PceCDInfo_Init();
TCHAR* PceCDInfo_Text(INT32 nText);
INT32  PceCDInfo_ID();
void   PceCDInfo_Exit();
#else
static inline INT32  PceCDInfo_Init()      { return 0; }
static inline TCHAR* PceCDInfo_Text(INT32) { return NULL; }
static inline INT32  PceCDInfo_ID()        { return 0; }
static inline void   PceCDInfo_Exit()      { }
#endif

#endif