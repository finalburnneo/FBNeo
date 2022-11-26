#include "m6800/m6800.h"

typedef UINT8 (*pReadByteHandler)(UINT16 a);
typedef void (*pWriteByteHandler)(UINT16 a, UINT8 d);
typedef UINT8 (*pReadOpHandler)(UINT16 a);
typedef UINT8 (*pReadOpArgHandler)(UINT16 a);
typedef UINT8 (*pReadPortHandler)(UINT16 a);
typedef void (*pWritePortHandler)(UINT16 a, UINT8 d);

struct M6800Ext {

	m6800_Regs reg;

	UINT8* pMemMap[0x100 * 3];

	pReadByteHandler ReadByte;
	pWriteByteHandler WriteByte;
	pReadOpHandler ReadOp;
	pReadOpArgHandler ReadOpArg;
	pReadPortHandler ReadPort;
	pWritePortHandler WritePort;

	INT32 nCpuType;
	INT32 nCyclesTotal;
	INT32 nCyclesSegment;
	INT32 nCyclesLeft;
};


#define HD63701_INPUT_LINE_NMI	M6800_INPUT_LINE_NMI
#define HD63701_IRQ_LINE		M6800_IRQ_LINE
#define HD63701_TIN_LINE		M6800_TIN_LINE

#define M6801_INPUT_LINE_NMI	M6800_INPUT_LINE_NMI
#define M6801_IRQ_LINE			M6800_IRQ_LINE
#define M6801_TIN_LINE			M6800_TIN_LINE

#define M6803_INPUT_LINE_NMI	M6800_INPUT_LINE_NMI
#define M6803_IRQ_LINE			M6800_IRQ_LINE
#define M6803_TIN_LINE			M6800_TIN_LINE

#define NSC8105_INPUT_LINE_NMI	M6800_INPUT_LINE_NMI
#define NSC8105_IRQ_LINE		M6800_IRQ_LINE
#define NSC8105_TIN_LINE		M6800_TIN_LINE

#define CPU_TYPE_M6800		1
#define CPU_TYPE_HD63701	2
#define CPU_TYPE_M6803		3
#define CPU_TYPE_M6801		4
#define CPU_TYPE_NSC8105    5

extern INT32 nM6800Count;

extern INT32 nM6800CyclesTotal;

void M6800Reset();
void M6800Reset(INT32 nCPU);
#define HD63701Reset		M6800Reset
#define M6803Reset			M6800Reset
#define M6801Reset			M6800Reset
#define NSC8105Reset		M6800Reset

void M6800ResetSoft();
#define HD63701ResetSoft	M6800ResetSoft
#define M6803ResetSoft		M6800ResetSoft
#define M6801ResetSoft		M6800ResetSoft
#define NSC8105ResetSoft    M6800ResetSoft

void M6800NewFrame();
#define HD63701NewFrame		M6800NewFrame
#define M6803NewFrame		M6800NewFrame
#define M6801NewFrame		M6800NewFrame
#define NSC8105NewFrame		M6800NewFrame

INT32 M6800CoreInit(INT32 num, INT32 type);
INT32 M6800Init(INT32 num);
INT32 HD63701Init(INT32 num);
INT32 M6803Init(INT32 num);
INT32 M6801Init(INT32 num);
INT32 NSC8105Init(INT32 num);

void M6800Exit();
#define HD63701Exit			M6800Exit
#define M6803Exit			M6800Exit
#define M6801Exit			M6800Exit
#define NSC8105Exit			M6800Exit

void M6800SetIRQLine(INT32 vector, INT32 status);
void M6800SetIRQLine(INT32 nCPU, const INT32 line, const INT32 status);
#define M6801SetIRQLine		M6800SetIRQLine
#define HD63701SetIRQLine	M6800SetIRQLine
#define M6803SetIRQLine		M6800SetIRQLine
#define NSC8105SetIRQLine	M6800SetIRQLine

INT32 M6800Run(INT32 cycles);
INT32 M6800Run(INT32 nCPU, INT32 nCycles);
#define M6801Run			M6800Run
#define HD63701Run			M6800Run
#define M6803Run			M6800Run
#define NSC8105Run			M6800Run

void M6800RunEnd();
#define HD63701RunEnd		M6800RunEnd
#define M6803RunEnd			M6800RunEnd
#define M6801RunEnd			M6800RunEnd
#define NSC8105RunEnd		M6800RunEnd

UINT32 M6800GetPC(INT32);
#define HD63701GetPC		M6800GetPC
#define M6803GetPC			M6800GetPC
#define M6801GetPC			M6800GetPC
#define NSC8105GetPC		M6800GetPC

INT32 M6800MapMemory(UINT8* pMemory, UINT16 nStart, UINT16 nEnd, INT32 nType);
#define HD63701MapMemory	M6800MapMemory
#define M6803MapMemory		M6800MapMemory
#define M6801MapMemory		M6800MapMemory
#define NSC8105MapMemory	M6800MapMemory

void M6800SetReadHandler(UINT8 (*pHandler)(UINT16));
#define HD63701SetReadHandler	M6800SetReadHandler
#define M6803SetReadHandler		M6800SetReadHandler
#define M6801SetReadHandler		M6800SetReadHandler
#define NSC8105SetReadHandler	M6800SetReadHandler

void M6800SetWriteHandler(void (*pHandler)(UINT16, UINT8));
#define HD63701SetWriteHandler	M6800SetWriteHandler
#define M6803SetWriteHandler	M6800SetWriteHandler
#define M6801SetWriteHandler	M6800SetWriteHandler
#define NSC8105SetWriteHandler	M6800SetWriteHandler

void M6800SetReadOpHandler(UINT8 (*pHandler)(UINT16));
#define HD63701SetReadOpHandler		M6800SetReadOpHandler
#define M6803SetReadOpHandler		M6800SetReadOpHandler
#define M6801SetReadOpHandler		M6800SetReadOpHandler
#define NSC8105SetReadOpHandler		M6800SetReadOpHandler

void M6800SetReadOpArgHandler(UINT8 (*pHandler)(UINT16));
#define HD63701SetReadOpArgHandler	M6800SetReadOpArgHandler
#define M6803SetReadOpArgHandler	M6800SetReadOpArgHandler
#define M6801SetReadOpArgHandler	M6800SetReadOpArgHandler
#define NSC8105SetReadOpArgHandler	M6800SetReadOpArgHandler

void M6800SetReadPortHandler(UINT8 (*pHandler)(UINT16));
#define HD63701SetReadPortHandler	M6800SetReadPortHandler
#define M6803SetReadPortHandler		M6800SetReadPortHandler
#define M6801SetReadPortHandler		M6800SetReadPortHandler
#define NSC8105SetReadPortHandler	M6800SetReadPortHandler

void M6800SetWritePortHandler(void (*pHandler)(UINT16, UINT8));
#define HD63701SetWritePortHandler	M6800SetWritePortHandler
#define M6803SetWritePortHandler	M6800SetWritePortHandler
#define M6801SetWritePortHandler	M6800SetWritePortHandler
#define NSC8105SetWritePortHandler	M6800SetWritePortHandler

INT32 M6800Scan(INT32 nAction);
#define HD63701Scan		M6800Scan
#define M6803Scan		M6800Scan
#define M6801Scan		M6800Scan
#define NSC8105Scan		M6800Scan

inline static INT32 M6800TotalCycles()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6800TotalCycles called without init\n"));
#endif

	return nM6800CyclesTotal + m6800_get_segmentcycles();
}
INT32 M6800TotalCycles(INT32 nCPU);
#define HD63701TotalCycles		M6800TotalCycles
#define M6803TotalCycles		M6800TotalCycles
#define M6801TotalCycles		M6800TotalCycles
#define NSC8105TotalCycles		M6800TotalCycles

inline static INT32 M6800Idle(INT32 cycles)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6800Idle called without init\n"));
#endif

	nM6800CyclesTotal += cycles;

	return cycles;
}
INT32 M6800Idle(INT32 nCPU, INT32 nCycles);
#define HD63701Idle		M6800Idle
#define M6803Idle		M6800Idle
#define M6801Idle		M6800Idle
#define NSC8105Idle		M6800Idle

void M6800CPUPush(INT32 nCPU);
#define HD63701CPUPush		M6800CPUPush
#define M6803CPUPush		M6800CPUPush
#define M6801CPUPush		M6800CPUPush
#define NSC8105CPUPush		M6800CPUPush

void M6800CPUPop();
#define HD63701CPUPop		M6800CPUPop
#define M6803CPUPop			M6800CPUPop
#define M6801CPUPop			M6800CPUPop
#define NSC8105CPUPop		M6800CPUPop

void M6800WriteRom(UINT32 Address, UINT8 Data); // cheat core
UINT8 M6800CheatRead(UINT32 Address);

void M6800Open(INT32 num);
#define HD63701Open		M6800Open
#define M6803Open		M6800Open
#define M6801Open		M6800Open
#define NSC8105Open		M6800Open

void M6800Close();
#define HD63701Close	M6800Close
#define M6803Close		M6800Close
#define M6801Close		M6800Close
#define NSC8105Close	M6800Close

INT32 M6800GetActive();
#define HD63701GetActive	M6800GetActive
#define M6803GetActive		M6800GetActive
#define M6801GetActive		M6800GetActive
#define NSC8105GetActive	M6800GetActive

extern struct cpu_core_config M6800Config;
extern struct cpu_core_config HD63701Config;
extern struct cpu_core_config M6803Config;
extern struct cpu_core_config NSC8105Config;

// depreciate this and use BurnTimerAttach directly!
#define BurnTimerAttachM6800(clock)	\
	BurnTimerAttach(&M6800Config, clock)

// depreciate this and use BurnTimerAttach directly!
#define BurnTimerAttachHD63701(clock)	\
	BurnTimerAttach(&HD63701Config, clock)

// depreciate this and use BurnTimerAttach directly!
#define BurnTimerAttachM6803(clock)	\
	BurnTimerAttach(&M6803Config, clock)

// depreciate this and use BurnTimerAttach directly!
#define BurnTimerAttachNSC8105(clock)	\
	BurnTimerAttach(&NSC8105Config, clock)
