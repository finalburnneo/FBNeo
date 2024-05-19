// Z80 (Zed Eight-Ty) Interface

#ifndef FASTCALL
 #undef __fastcall
 #define __fastcall
#endif

#include "z80/z80.h"

extern INT32 nHasZet;
extern INT32 ZetInFetch;

void ZetWriteByte(UINT16 address, UINT8 data);
UINT8 ZetReadByte(UINT16 address);
void ZetWriteRom(UINT16 address, UINT8 data);
UINT8 __fastcall ZetReadIO(UINT32 a);
void __fastcall ZetWriteIO(UINT32 a, UINT8 d);
INT32 ZetInit(INT32 nCount);
void ZetDaisyInit(INT32 dev0, INT32 dev1);
void ZetExit();
void ZetNewFrame();
void ZetOpen(INT32 nCPU);
void ZetClose();
INT32 ZetGetActive();
void ZetSwapActive(INT32 nCPU);

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
void ZetReset(INT32 nCPU);
UINT32 ZetGetPC(INT32 n);
INT32 ZetGetPrevPC(INT32 n);
INT32 ZetBc(INT32 n);
INT32 ZetDe(INT32 n);
INT32 ZetHL(INT32 n);
INT32 ZetI(INT32 n);
INT32 ZetSP(INT32 n);
INT32 ZetScan(INT32 nAction);
INT32 ZetRun(INT32 nCycles);
INT32 ZetRun(INT32 nCPU, INT32 nCycles);
void ZetRunEnd();
void ZetRunEnd(INT32 nCPU);
void ZetSetIRQLine(const INT32 line, const INT32 status);
void ZetSetIRQLine(INT32 nCPU, const INT32 line, const INT32 status);
void ZetSetVector(INT32 vector);
void ZetSetVector(INT32 nCPU, INT32 vector);
UINT8 ZetGetVector();
UINT8 ZetGetVector(INT32 nCPU);
INT32 ZetNmi();
INT32 ZetNmi(INT32 nCPU);
INT32 ZetIdle(INT32 nCycles);
INT32 ZetIdle(INT32 nCPU, INT32 nCycles);
INT32 ZetSegmentCycles();
INT32 ZetTotalCycles();
INT32 ZetTotalCycles(INT32 nCPU);
void ZetSetAF(INT32 n, UINT16 value);
void ZetSetAF2(INT32 n, UINT16 value);
void ZetSetBC(INT32 n, UINT16 value);
void ZetSetBC2(INT32 n, UINT16 value);
void ZetSetDE(INT32 n, UINT16 value);
void ZetSetDE2(INT32 n, UINT16 value);
void ZetSetHL(INT32 n, UINT16 value);
void ZetSetHL2(INT32 n, UINT16 value);
void ZetSetI(INT32 n, UINT16 value);
void ZetSetIFF1(INT32 n, UINT16 value);
void ZetSetIFF2(INT32 n, UINT16 value);
void ZetSetIM(INT32 n, UINT16 value);
void ZetSetIX(INT32 n, UINT16 value);
void ZetSetIY(INT32 n, UINT16 value);
void ZetSetPC(INT32 n, UINT16 value);
void ZetSetR(INT32 n, UINT16 value);
void ZetSetSP(INT32 n, UINT16 value);

void ZetCPUPush(INT32 nCPU);
void ZetCPUPop();


void ZetSetReadHandler(UINT8 (__fastcall *pHandler)(UINT16));
void ZetSetWriteHandler(void (__fastcall *pHandler)(UINT16, UINT8));
void ZetSetInHandler(UINT8 (__fastcall *pHandler)(UINT16));
void ZetSetOutHandler(void (__fastcall *pHandler)(UINT16, UINT8));
void ZetSetEDFECallback(void (*pCallback)(Z80_Regs*));

void ZetSetHALT(INT32 nStatus);
void ZetSetHALT(INT32 nCPU, INT32 nStatus);
INT32 ZetGetHALT();
INT32 ZetGetHALT(INT32 nCPU);

#define ZetSetBUSREQLine ZetSetHALT

void ZetSetRESETLine(INT32 nCPU, INT32 nStatus);
void ZetSetRESETLine(INT32 nStatus);
INT32 ZetGetRESETLine();
INT32 ZetGetRESETLine(INT32 nCPU);

void ZetCheatWriteROM(UINT32 a, UINT8 d); // cheat core
UINT8 ZetCheatRead(UINT32 a);

extern struct cpu_core_config ZetConfig;

// depreciate this and use BurnTimerAttach directly!
#define BurnTimerAttachZet(clock)	\
	BurnTimerAttach(&ZetConfig, clock)
