#include "m6502/m6502.h"

typedef UINT8 (*pReadPortHandler)(UINT16 a);
typedef void (*pWritePortHandler)(UINT16 a, UINT8 d);
typedef UINT8 (*pReadByteHandler)(UINT16 a);
typedef void (*pWriteByteHandler)(UINT16 a, UINT8 d);

struct M6502Ext {

	m6502_Regs reg;
	
	INT32 (*execute)(INT32 cycles);
	void (*reset)();
	void (*init)();
	void (*set_irq_line)(INT32 irqline, INT32 state);

	UINT8* pMemMap[0x100 * 3];
	UINT32 AddressMask;
	UINT8 opcode_reorder[0x100];

	pReadPortHandler ReadPort;
	pWritePortHandler WritePort;
	pReadByteHandler ReadByte;
	pWriteByteHandler WriteByte;

	INT32 nCyclesTotal;
	INT32 nCyclesStall;
};

extern INT32 nM6502Count;

void M6502Reset();
void M6502Reset(INT32 nCPU);
void M6502NewFrame();

void n2a03_irq(void); // USED FOR PSG!!

// use these at addresses 0 and 1 for M6510, M6510T, M7501, M8502 map!
UINT8 m6510_read_0000(UINT16 offset);
void m6510_write_0000(UINT16 address, UINT8 data);

// The M6504 only has 13 address bits! use address mirroring!

enum { TYPE_M6502=0, TYPE_M6504, TYPE_M65C02, TYPE_M65SC02, TYPE_N2A03, TYPE_DECO16,
//	these are the same!
	TYPE_M6510, TYPE_M6510T, TYPE_M7501, TYPE_M8502,
//	these involve encryption
	TYPE_DECOCPU7, TYPE_DECO222, TYPE_DECOC10707
};

INT32 M6502Init(INT32 cpu, INT32 type); // if you're using more than one type
void M6502Exit();
void M6502Open(INT32 num);
void M6502Close();
INT32 M6502GetActive();
INT32 M6502Idle(INT32 nCycles);
INT32 M6502Idle(INT32 nCPU, INT32 nCycles);
INT32 M6502Stall(INT32 nCycles);
INT32 M6502Stall(INT32 nCPU, INT32 nCycles);
void M6502ReleaseSlice();
void M6502SetIRQLine(INT32 vector, INT32 status);
void M6502SetIRQLine(INT32 nCPU, const INT32 line, const INT32 status);
INT32 M6502Run(INT32 cycles);
INT32 M6502Run(INT32 nCPU, INT32 nCycles);
void M6502RunEnd();
INT32 M6502MapMemory(UINT8* pMemory, UINT16 nStart, UINT16 nEnd, INT32 nType);
void M6502SetReadPortHandler(UINT8 (*pHandler)(UINT16));
void M6502SetWritePortHandler(void (*pHandler)(UINT16, UINT8));
void M6502SetReadHandler(UINT8 (*pHandler)(UINT16));
void M6502SetWriteHandler(void (*pHandler)(UINT16, UINT8));
INT32 M6502Scan(INT32 nAction);

void M6502CPUPush(INT32 nCPU);
void M6502CPUPop();

void M6502SetOpcodeDecode(UINT8 *table);

void M6502SetAddressMask(UINT16 RangeMask);

void M6502SetCallback(int (*cb)(int));

void M6502SetPC(INT32 pc);
UINT32 M6502GetPC(INT32);
UINT32 M6502GetPrevPC(INT32);

INT32 M6502GetFetchStatus();

INT32 M6502TotalCycles();
INT32 M6502TotalCycles(INT32 nCPU);

// m6502.cpp used for Data East encrypted CPUs.
void DecoCpu7SetDecode(UINT8 (*write)(UINT16,UINT8));

void M6502WriteRom(UINT32 Address, UINT8 Data); // cheat core
UINT8 M6502CheatRead(UINT32 a);

extern struct cpu_core_config M6502Config;

// depreciate this and use BurnTimerAttach directly!
#define BurnTimerAttachM6502(clock)	\
	BurnTimerAttach(&M6502Config, clock)
