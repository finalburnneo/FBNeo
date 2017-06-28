// Z80 (Zed Eight-Ty) Interface

#ifndef FASTCALL
 #undef __fastcall
 #define __fastcall
#endif

#include "z80/z80.h"

extern INT32 nHasZet;
void ZetWriteByte(UINT16 address, UINT8 data);
UINT8 ZetReadByte(UINT16 address);
void ZetWriteRom(UINT16 address, UINT8 data);
INT32 ZetInit(INT32 nCount);
void ZetExit();
void ZetNewFrame();
void ZetOpen(INT32 nCPU);
void ZetClose();
INT32 ZetGetActive();

//#define ZET_FETCHOP	4
//#define ZET_FETCHARG	8
//#define ZET_READ	1
//#define ZET_WRITE	2
//#define ZET_FETCH	(ZET_FETCHOP|ZET_FETCHARG)
//#define ZET_ROM		(ZET_READ|ZET_FETCH)
//#define ZET_RAM		(ZET_ROM|ZET_WRITE)

INT32 ZetUnmapMemory(INT32 nStart,INT32 nEnd,INT32 nFlags);
void ZetMapMemory(UINT8 *Mem, INT32 nStart, INT32 nEnd, INT32 nFlags);

INT32 ZetMemCallback(INT32 nStart,INT32 nEnd,INT32 nMode);
INT32 ZetMapArea(INT32 nStart, INT32 nEnd, INT32 nMode, UINT8 *Mem);
INT32 ZetMapArea(INT32 nStart, INT32 nEnd, INT32 nMode, UINT8 *Mem01, UINT8 *Mem02);

void ZetReset();
UINT32 ZetGetPC(INT32 n);
INT32 ZetGetPrevPC(INT32 n);
INT32 ZetBc(INT32 n);
INT32 ZetDe(INT32 n);
INT32 ZetHL(INT32 n);
INT32 ZetI(INT32 n);
INT32 ZetScan(INT32 nAction);
INT32 ZetRun(INT32 nCycles);
void ZetRunEnd();
void ZetSetIRQLine(const INT32 line, const INT32 status);
void ZetSetVector(INT32 vector);
UINT8 ZetGetVector();
INT32 ZetNmi();
INT32 ZetIdle(INT32 nCycles);
INT32 ZetSegmentCycles();
INT32 ZetTotalCycles();
void ZetSetHL(INT32 n, UINT16 value);
void ZetSetSP(INT32 n, UINT16 value);

//#define ZetRaiseIrq(n) ZetSetIRQLine(n, ZET_IRQSTATUS_AUTO)
//#define ZetLowerIrq() ZetSetIRQLine(0, Z80_CLEAR_LINE)

void ZetSetReadHandler(UINT8 (__fastcall *pHandler)(UINT16));
void ZetSetWriteHandler(void (__fastcall *pHandler)(UINT16, UINT8));
void ZetSetInHandler(UINT8 (__fastcall *pHandler)(UINT16));
void ZetSetOutHandler(void (__fastcall *pHandler)(UINT16, UINT8));
void ZetSetEDFECallback(void (*pCallback)(Z80_Regs*));

void ZetSetBUSREQLine(INT32 nStatus);
