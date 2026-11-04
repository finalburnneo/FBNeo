#ifndef SEGACDLIST_H_
#define SEGACDLIST_H_

#include "cdlist.h"

#ifdef BUILD_MEGADRIVE
INT32  SegaCDInfo_Init();
TCHAR* SegaCDInfo_Text(INT32 nText);
void   SegaCDInfo_Exit();
#else
static inline INT32  SegaCDInfo_Init()      { return 0; }
static inline TCHAR* SegaCDInfo_Text(INT32) { return NULL; }
static inline void   SegaCDInfo_Exit()      { }
#endif

#endif
