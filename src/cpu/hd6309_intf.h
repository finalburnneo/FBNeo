#include "hd6309/hd6309.h"

typedef UINT8 (*pReadByteHandler)(UINT16 a);
typedef void (*pWriteByteHandler)(UINT16 a, UINT8 d);
typedef UINT8 (*pReadOpHandler)(UINT16 a);
typedef UINT8 (*pReadOpArgHandler)(UINT16 a);

struct HD6309Ext {

	hd6309_Regs reg;
	
	UINT8* pMemMap[0x100 * 3];

	pReadByteHandler ReadByte;
	pWriteByteHandler WriteByte;
	pReadOpHandler ReadOp;
	pReadOpArgHandler ReadOpArg;
};

extern INT32 nHD6309Count;

extern INT32 nHD6309CyclesTotal;

void HD6309Reset();
void HD6309Reset(INT32 nCPU);
INT32 HD6309TotalCycles();
INT32 HD6309TotalCycles(INT32 nCPU);
void HD6309NewFrame();
INT32 HD6309Init(INT32 nCPU);
void HD6309Exit();
void HD6309Open(INT32 num);
void HD6309Close();
INT32 HD6309GetActive();
void HD6309SetIRQLine(INT32 vector, INT32 status);
void HD6309SetIRQLine(INT32 nCPU, const INT32 line, const INT32 status);
INT32 HD6309Run(INT32 cycles);
INT32 HD6309Run(INT32 nCPU, INT32 nCycles);
void HD6309RunEnd();
INT32 HD6309Idle(INT32 cycles);
INT32 HD6309Idle(INT32 nCPU, INT32 nCycles);
UINT32 HD6309GetPC(INT32);
INT32 HD6309MapMemory(UINT8* pMemory, UINT16 nStart, UINT16 nEnd, INT32 nType);
INT32 HD6309MemCallback(UINT16 nStart, UINT16 nEnd, INT32 nType);
void HD6309SetReadHandler(UINT8 (*pHandler)(UINT16));
void HD6309SetWriteHandler(void (*pHandler)(UINT16, UINT8));
void HD6309SetReadOpHandler(UINT8 (*pHandler)(UINT16));
void HD6309SetReadOpArgHandler(UINT8 (*pHandler)(UINT16));
INT32 HD6309Scan(INT32 nAction);

void HD6309WriteRom(UINT16 Address, UINT8 Data);

void HD6309CheatWriteRom(UINT32 a, UINT8 d); // cheat core
UINT8 HD6309CheatRead(UINT32 a); // cheat core

extern struct cpu_core_config HD6309Config;

// depreciate this and use BurnTimerAttach directly!
#define BurnTimerAttachHD6309(clock)	\
	BurnTimerAttach(&HD6309Config, clock)
