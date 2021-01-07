#ifndef TMS34010_INTF_H
#define TMS34010_INTF_H

#include <stdint.h>
#include "tms34/tms34010.h"

#define TMS34010_INT_EX1        0
#define TMS34010_INT_EX2		1

typedef UINT16 (*pTMS34010ReadHandler)(UINT32);
typedef void (*pTMS34010WriteHandler)(UINT32, UINT16);

typedef tms34010_display_params TMS34010Display;
typedef scanline_render_t pTMS34010ScanlineRender;

void TMS34010Init();
void TMS34010Exit();
int TMS34010Run(int cycles);
int TMS34010Idle(int cycles);
void TMS34010TimerSetCB(void (*timer_cb)());
void TMS34010TimerSet(int cycles);
INT64 TMS34010TotalCycles();
void TMS34010Scan(INT32 nAction);
void TMS34010RunEnd();
void TMS34010NewFrame();
void TMS34010Reset();
void TMS34020Reset(); // reset w/ this to use tms34020!
void TMS34010GenerateIRQ(UINT32 line);
void TMS34010ClearIRQ(UINT32 line);
void TMS34010SetScanlineRender(pTMS34010ScanlineRender sr);
void TMS34010SetToShift(void (*SL)(UINT32 addr, UINT16 *dst));
void TMS34010SetFromShift(void (*FS)(UINT32 addr, UINT16 *src));
void TMS34010SetPixClock(INT32 pxlclock, INT32 pix_per_clock);
int TMS34010GenerateScanline(int line);
UINT32 TMS34010GetPC();
UINT32 TMS34010GetPPC();

UINT8 TMS34010ReadByte(UINT32 address);
UINT16 TMS34010ReadWord(UINT32 address);
void TMS34010WriteByte(UINT32 address, UINT8 value);
void TMS34010WriteWord(UINT32 address, UINT16 value);
void TMS34010MapReset();
void TMS34010MapMemory(UINT8 *mem, UINT32 start, UINT32 end, UINT8 type);
void TMS34010MapHandler(uintptr_t num, UINT32 start, UINT32 end, UINT8 type);
int TMS34010SetReadHandler(UINT32 num, pTMS34010ReadHandler handler);
int TMS34010SetWriteHandler(UINT32 num, pTMS34010WriteHandler handler);
int TMS34010SetHandlers(UINT32 num, pTMS34010ReadHandler rhandler, pTMS34010WriteHandler whandler);


#endif // TMS34010_INTF_H
