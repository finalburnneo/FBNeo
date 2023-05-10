// Nec V20/V30/V33 interface

#ifndef FASTCALL
 #undef __fastcall
 #define __fastcall
#endif

#define V33_TYPE 0
#define V30_TYPE 8
#define V20_TYPE 16
#define i86_TYPE 32
#define V25_TYPE (V20_TYPE|(1<<16))
#define V35_TYPE (V30_TYPE|(1<<16))

UINT8 cpu_readmem20(UINT32 a);
void cpu_writemem20(UINT32 a, UINT8 d);

extern INT32 nVezCount;

void VezNewFrame();
void VezRunEnd();
void VezRunEnd(INT32 nCPU);

INT32 VezTotalCycles();
INT32 VezTotalCycles(INT32 nCPU);
INT32 VezIdle(INT32 cycles);
INT32 VezIdle(INT32 nCPU, INT32 nCycles);

void VezReset();
void VezReset(INT32 nCPU);

UINT32 VezGetPC(INT32 n);
INT32 VezScan(INT32 nAction);

INT32 VezRun(INT32 nCycles);
INT32 VezRun(INT32 nCPU, INT32 nCycles);

void VezSetRESETLine(INT32 nStatus);
void VezSetRESETLine(INT32 nCPU, INT32 nStatus);
INT32 VezGetRESETLine();
INT32 VezGetRESETLine(INT32 nCPU);

void VezSetHALT(INT32 nStatus);
void VezSetHALT(INT32 nCPU, INT32 nStatus);
INT32 VezGetHALT();
INT32 VezGetHALT(INT32 nCPU);

INT32 VezInit(INT32 nCPU, INT32 type, INT32 clock); // v20/v25/v30/v33/v35
INT32 VezInit(INT32 cpu, INT32 type); // v20/v30/v33 only
void VezSetDecode(UINT8 *decode); // set opcode decode
void VezExit();
void VezOpen(INT32 nCPU);
void VezClose();
INT32 VezGetActive();

void VezCPUPush(INT32 nCPU);
void VezCPUPop();

INT32 VezMemCallback(INT32 nStart,INT32 nEnd,INT32 nMode);
INT32 VezMapArea(INT32 nStart, INT32 nEnd, INT32 nMode, UINT8 *Mem);
INT32 VezMapArea(INT32 nStart, INT32 nEnd, INT32 nMode, UINT8 *Mem1, UINT8 *Mem2);
INT32 VezMapMemory(UINT8 *Mem, INT32 nStart, INT32 nEnd, INT32 nMode);

void VezSetReadHandler(UINT8 (__fastcall*)(UINT32));
void VezSetWriteHandler(void (__fastcall*)(UINT32, UINT8));

void VezWriteByte(UINT32 a, UINT8 d);
void VezWriteWord(UINT32 a, UINT16 d);
void VezWriteLong(UINT32 a, UINT32 d);
UINT8 VezReadByte(UINT32 a);
UINT16 VezReadWord(UINT32 a);
UINT32 VezReadLong(UINT32 a);

#define V25_PORT_P0 0x10000
#define V25_PORT_P1 0x10002
#define V25_PORT_P2 0x10004
#define V25_PORT_PT 0x10006

void VezSetReadPort(UINT8 (__fastcall*)(UINT32));
void VezSetWritePort(void (__fastcall*)(UINT32, UINT8));
void VezSetIrqCallBack(INT32 (*cb)(INT32));

#define NEC_INPUT_LINE_INTP0 10
#define NEC_INPUT_LINE_INTP1 11
#define NEC_INPUT_LINE_INTP2 12
#define NEC_INPUT_LINE_POLL  20

void VezSetIRQLineAndVector(const INT32 line, const INT32 vector, const INT32 status);
void VezSetIRQLineAndVector(INT32 nCPU, const INT32 line, const INT32 vector, const INT32 status);

void nec_set_vector_callback(UINT8 (*cb)());
void nec_set_irq_line(int state); // for use with vector callback


void VezCheatWrite(UINT32 a, UINT8 d); // cheat core

extern struct cpu_core_config VezConfig;

// depreciate this and use BurnTimerAttach directly!
#define BurnTimerAttachVez(clock)	\
	BurnTimerAttach(&VezConfig, clock)
