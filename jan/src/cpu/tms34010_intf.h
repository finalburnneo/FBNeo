#ifndef TMS34010_INTF_H
#define TMS34010_INTF_H

#include "tms34010/tms34010.h"

#define TMS34010_INT_EX1		tms::INTERRUPT_EXTERN_1
#define TMS34010_INT_EX2		tms::INTERRUPT_EXTERN_2
#define TMS34010_INT_HOST		tms::INTERRUPT_HOST
#define TMS34010_INT_DISPLAY	tms::INTERRUPT_DISPLAY
#define TMS34010_INT_WIN		tms::INTERRUPT_WINDOW

typedef UINT16 (*pTMS34010ReadHandler)(UINT32);
typedef void (*pTMS34010WriteHandler)(UINT32, UINT16);

typedef tms::cpu_state TMS34010State;
typedef tms::display_info TMS34010Display;
typedef tms::scanline_render_t pTMS34010ScanlineRender;
// typedef void (*pTMS34010ScanlineRender)(int line, TMS34010Display *info);

void TMS34010Init();
int TMS34010Run(int cycles);
void TMS34010Reset();
void TMS34010GenerateIRQ(UINT32 line);
void TMS34010ClearIRQ(UINT32 line);
void TMS34010SetScanlineRender(pTMS34010ScanlineRender sr);
void TMS34010SetToShift(void (*SL)(UINT32 addr, void *dst));
void TMS34010SetFromShift(void (*FS)(UINT32 addr, void *src));
int TMS34010GenerateScanline(int line);
TMS34010State *TMS34010GetState();

UINT16 TMS34010ReadWord(UINT32 address);
void TMS34010WriteWord(UINT32 address, UINT16 value);
void TMS34010MapReset();
void TMS34010MapMemory(UINT8 *mem, UINT32 start, UINT32 end, UINT8 type);
void TMS34010MapHandler(UINT32 num, UINT32 start, UINT32 end, UINT8 type);
int TMS34010SetReadHandler(UINT32 num, pTMS34010ReadHandler handler);
int TMS34010SetWriteHandler(UINT32 num, pTMS34010WriteHandler handler);
int TMS34010SetHandlers(UINT32 num, pTMS34010ReadHandler rhandler, pTMS34010WriteHandler whandler);


#endif // TMS34010_INTF_H
