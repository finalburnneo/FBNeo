#include "m6809/m6809.h"

typedef UINT8 (*pReadByteHandler)(UINT16 a);
typedef void (*pWriteByteHandler)(UINT16 a, UINT8 d);

struct M6809Ext {

	m6809_Regs reg;

	UINT8* pMemMap[0x100 * 3];

	pReadByteHandler ReadByte;
	pWriteByteHandler WriteByte;

	INT32 nCyclesTotal;
};

extern INT32 nM6809Count;

void M6809Reset();
void M6809Reset(INT32 nCPU);
void M6809NewFrame();
INT32 M6809Init(INT32 cpu);
void M6809Exit();
void M6809Open(INT32 num);
void M6809Close();
INT32 M6809GetActive();
void M6809SetIRQLine(INT32 vector, INT32 status);
void M6809SetIRQLine(INT32 nCPU, const INT32 line, const INT32 status);
INT32 M6809Run(INT32 cycles);
INT32 M6809Run(INT32 nCPU, INT32 nCycles);
void M6809RunEnd();
INT32 M6809TotalCycles();
INT32 M6809TotalCycles(INT32 nCPU);
INT32 M6809Idle(INT32 cycles);
INT32 M6809Idle(INT32 nCPU, INT32 nCycles);
INT32 M6809MapMemory(UINT8* pMemory, UINT16 nStart, UINT16 nEnd, INT32 nType);
INT32 M6809UnmapMemory(UINT16 nStart, UINT16 nEnd, INT32 nType);
void M6809SetReadHandler(UINT8 (*pHandler)(UINT16));
void M6809SetWriteHandler(void (*pHandler)(UINT16, UINT8));
void M6809SetCallback(int (*cb)(int));
INT32 M6809Scan(INT32 nAction);
UINT16 M6809GetPC();
UINT16 M6809GetPrevPC();

void M6809CPUPush(INT32 nCPU);
void M6809CPUPop();

UINT8 M6809ReadByte(UINT16 Address);
void M6809WriteByte(UINT16 Address, UINT8 Data);

void M6809WriteRom(UINT32 Address, UINT8 Data); // cheat core
UINT8 M6809CheatRead(UINT32 Address);

extern struct cpu_core_config M6809Config;

// depreciate this and use BurnTimerAttach directly!
#define BurnTimerAttachM6809(clock)	\
	BurnTimerAttach(&M6809Config, clock)
