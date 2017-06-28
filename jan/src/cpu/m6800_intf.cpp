#include "burnint.h"
#include "m6800_intf.h"

#define MAX_CPU		1

INT32 nM6800Count = 0; // note: is 0 when 1 cpu is in use. also, 0 when no cpus are in use.
static INT32 nCpuType = 0;

static M6800Ext *M6800CPUContext = NULL;

static INT32 nM6800CyclesDone[MAX_CPU];
INT32 nM6800CyclesTotal;

static UINT8 M6800ReadByteDummyHandler(UINT16)
{
	return 0;
}

static void M6800WriteByteDummyHandler(UINT16, UINT8)
{
}

static UINT8 M6800ReadOpDummyHandler(UINT16)
{
	return 0;
}

static UINT8 M6800ReadOpArgDummyHandler(UINT16)
{
	return 0;
}

static UINT8 M6800ReadPortDummyHandler(UINT16)
{
	return 0;
}

static void M6800WritePortDummyHandler(UINT16, UINT8)
{
}

void M6800Reset()
{
#if defined FBA_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6800Reset called without init\n"));
#endif

	m6800_reset();
}

void M6800NewFrame()
{
#if defined FBA_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6800NewFrame called without init\n"));
#endif

	for (INT32 i = 0; i <= nM6800Count; i++) {
		nM6800CyclesDone[i] = 0;
	}
	nM6800CyclesTotal = 0;
}

static UINT8 M6800CheatRead(UINT32 a)
{
	return M6800ReadByte(a);
}

void M6800Open(INT32 ) {} // does nothing
void M6800Close() {}	 // ""

INT32 M6800GetActive()
{
	return 0;
}

static cpu_core_config M6800CheatCpuConfig =
{
	M6800Open,
	M6800Close,
	M6800CheatRead,
	M6800WriteRom,
	M6800GetActive,
	M6800TotalCycles,
	M6800NewFrame,
	M6800Run,		// different
	M6800RunEnd,
	M6800Reset,
	1<<16,
	0
};

static cpu_core_config HD63701CheatCpuConfig =
{
	M6800Open,
	M6800Close,
	M6800CheatRead,
	M6800WriteRom,
	M6800GetActive,
	M6800TotalCycles,
	M6800NewFrame,
	HD63701Run,		// different
	M6800RunEnd,
	M6800Reset,
	1<<16,
	0
};

static cpu_core_config M6803CheatCpuConfig =
{
	M6800Open,
	M6800Close,
	M6800CheatRead,
	M6800WriteRom,
	M6800GetActive,
	M6800TotalCycles,
	M6800NewFrame,
	M6803Run,		// different
	M6800RunEnd,
	M6800Reset,
	1<<16,
	0
};

static cpu_core_config NSC8105CheatCpuConfig =
{
	M6800Open,
	M6800Close,
	M6800CheatRead,
	M6800WriteRom,
	M6800GetActive,
	M6800TotalCycles,
	M6800NewFrame,
	NSC8105Run,		// different
	M6800RunEnd,
	M6800Reset,
	1<<16,
	0
};

INT32 M6800CoreInit(INT32 num, INT32 type)
{
	DebugCPU_M6800Initted = 1;
	
	nM6800Count = num % MAX_CPU;
	
	M6800CPUContext = (M6800Ext*)malloc(num * sizeof(M6800Ext));
	if (M6800CPUContext == NULL) {
		return 1;
	}
	
	memset(M6800CPUContext, 0, num * sizeof(M6800Ext));
	
	for (INT32 i = 0; i < num; i++) {
		M6800CPUContext[i].ReadByte = M6800ReadByteDummyHandler;
		M6800CPUContext[i].WriteByte = M6800WriteByteDummyHandler;
		M6800CPUContext[i].ReadOp = M6800ReadOpDummyHandler;
		M6800CPUContext[i].ReadOpArg = M6800ReadOpArgDummyHandler;
		M6800CPUContext[i].ReadPort = M6800ReadPortDummyHandler;
		M6800CPUContext[i].WritePort = M6800WritePortDummyHandler;
		
		nM6800CyclesDone[i] = 0;
	
		for (INT32 j = 0; j < (0x0100 * 3); j++) {
			M6800CPUContext[i].pMemMap[j] = NULL;
		}
	}
	
	nM6800CyclesTotal = 0;
	nCpuType = type;
	
	if (type == CPU_TYPE_M6800) {
		m6800_init();

		for (INT32 i = 0; i < num; i++)
			CpuCheatRegister(i, &M6800CheatCpuConfig);
	}

	if (type == CPU_TYPE_HD63701) {
		hd63701_init();

		for (INT32 i = 0; i < num; i++)
			CpuCheatRegister(i, &HD63701CheatCpuConfig);
	}

	if (type == CPU_TYPE_M6803) {
		m6803_init();

		for (INT32 i = 0; i < num; i++)
			CpuCheatRegister(i, &M6803CheatCpuConfig);
	}

	if (type == CPU_TYPE_M6801) {
		m6801_init();

		for (INT32 i = 0; i < num; i++)
			CpuCheatRegister(i, &M6803CheatCpuConfig);
	}

	if (type == CPU_TYPE_NSC8105) {
		nsc8105_init();

		for (INT32 i = 0; i < num; i++)
			CpuCheatRegister(i, &NSC8105CheatCpuConfig);
	}

	return 0;
}

INT32 M6800Init(INT32 num)
{
	return M6800CoreInit(num, CPU_TYPE_M6800);
}

INT32 HD63701Init(INT32 num)
{
	return M6800CoreInit(num, CPU_TYPE_HD63701);
}

INT32 M6803Init(INT32 num)
{
	return M6800CoreInit(num, CPU_TYPE_M6803);
}

INT32 M6801Init(INT32 num)
{
	return M6800CoreInit(num, CPU_TYPE_M6801);
}

INT32 NSC8105Init(INT32 num)
{
	return M6800CoreInit(num, CPU_TYPE_NSC8105);
}

void M6800Exit()
{
#if defined FBA_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6800Exit called without init\n"));
#endif

	nM6800Count = 0;
	nCpuType = 0;

	if (M6800CPUContext) {
		free(M6800CPUContext);
		M6800CPUContext = NULL;
	}
	
	DebugCPU_M6800Initted = 0;
}

void M6800SetIRQLine(INT32 vector, INT32 status)
{
#if defined FBA_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6800SetIRQLine called without init\n"));
	if (nCpuType != CPU_TYPE_M6800) bprintf(PRINT_ERROR, _T("M6800SetIRQLine called with invalid CPU Type\n"));
#endif

	if (status == CPU_IRQSTATUS_NONE) {
		m6800_set_irq_line(vector, 0);
	}
	
	if (status == CPU_IRQSTATUS_ACK) {
		m6800_set_irq_line(vector, 1);
	}
	
	if (status == CPU_IRQSTATUS_AUTO) {
		m6800_set_irq_line(vector, 1);
		m6800_execute(0);
		m6800_set_irq_line(vector, 0);
		m6800_execute(0);
	}
}

void HD63701SetIRQLine(INT32 vector, INT32 status)
{
#if defined FBA_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("HD63701SetIRQLine called without init\n"));
	if (nCpuType != CPU_TYPE_HD63701) bprintf(PRINT_ERROR, _T("HD63701SetIRQLine called with invalid CPU Type\n"));
#endif

	if (status == CPU_IRQSTATUS_NONE) {
		m6800_set_irq_line(vector, 0);
	}
	
	if (status == CPU_IRQSTATUS_ACK) {
		m6800_set_irq_line(vector, 1);
	}
	
	if (status == CPU_IRQSTATUS_AUTO) {
		m6800_set_irq_line(vector, 1);
		hd63701_execute(0);
		m6800_set_irq_line(vector, 0);
		hd63701_execute(0);
	}
}

void M6803SetIRQLine(INT32 vector, INT32 status)
{
#if defined FBA_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6803SetIRQLine called without init\n"));
	if (nCpuType != CPU_TYPE_M6803) bprintf(PRINT_ERROR, _T("M6803SetIRQLine called with invalid CPU Type\n"));
#endif

	if (status == CPU_IRQSTATUS_NONE) {
		m6800_set_irq_line(vector, 0);
	}
	
	if (status == CPU_IRQSTATUS_ACK) {
		m6800_set_irq_line(vector, 1);
	}
	
	if (status == CPU_IRQSTATUS_AUTO) {
		m6800_set_irq_line(vector, 1);
		m6803_execute(0);
		m6800_set_irq_line(vector, 0);
		m6803_execute(0);
	}
}

void M6801SetIRQLine(INT32 vector, INT32 status)
{
#if defined FBA_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6801SetIRQLine called without init\n"));
	if (nCpuType != CPU_TYPE_M6801) bprintf(PRINT_ERROR, _T("M6800SetIRQLine called with invalid CPU Type\n"));
#endif

	if (status == CPU_IRQSTATUS_NONE) {
		m6800_set_irq_line(vector, 0);
	}
	
	if (status == CPU_IRQSTATUS_ACK) {
		m6800_set_irq_line(vector, 1);
	}
	
	if (status == CPU_IRQSTATUS_AUTO) {
		m6800_set_irq_line(vector, 1);
		m6803_execute(0);
		m6800_set_irq_line(vector, 0);
		m6803_execute(0);
	}
}

void NSC8105SetIRQLine(INT32 vector, INT32 status)
{
#if defined FBA_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("NSC8105SetIRQLine called without init\n"));
	if (nCpuType != CPU_TYPE_M6801) bprintf(PRINT_ERROR, _T("NSC8105SetIRQLine called with invalid CPU Type\n"));
#endif

	if (status == CPU_IRQSTATUS_NONE) {
		m6800_set_irq_line(vector, 0);
	}
	
	if (status == CPU_IRQSTATUS_ACK) {
		m6800_set_irq_line(vector, 1);
	}
	
	if (status == CPU_IRQSTATUS_AUTO) {
		m6800_set_irq_line(vector, 1);
		nsc8105_execute(0);
		m6800_set_irq_line(vector, 0);
		nsc8105_execute(0);
	}
}

INT32 M6800Run(INT32 cycles)
{
#if defined FBA_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6800Run called without init\n"));
	if (nCpuType != CPU_TYPE_M6800) bprintf(PRINT_ERROR, _T("M6800Run called with invalid CPU Type\n"));
#endif

	cycles = m6800_execute(cycles);
	
	nM6800CyclesTotal += cycles;
	
	return cycles;
}

INT32 HD63701Run(INT32 cycles)
{
#if defined FBA_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("HD63701Run called without init\n"));
	if (nCpuType != CPU_TYPE_HD63701) bprintf(PRINT_ERROR, _T("HD63701Run called with invalid CPU Type\n"));
#endif

	cycles = hd63701_execute(cycles);
	
	nM6800CyclesTotal += cycles;
	
	return cycles;
}

INT32 M6803Run(INT32 cycles)
{
#if defined FBA_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6803Run called without init\n"));
	if (nCpuType != CPU_TYPE_M6803 && nCpuType != CPU_TYPE_M6801) bprintf(PRINT_ERROR, _T("M6803Run called with invalid CPU Type\n"));
#endif

	cycles = m6803_execute(cycles);
	
	nM6800CyclesTotal += cycles;
	
	return cycles;
}

INT32 NSC8105Run(INT32 cycles)
{
#if defined FBA_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("NSC8105Run called without init\n"));
	if (nCpuType != CPU_TYPE_NSC8105) bprintf(PRINT_ERROR, _T("NSC8105Run called with invalid CPU Type\n"));
#endif

	cycles = nsc8105_execute(cycles);
	
	nM6800CyclesTotal += cycles;
	
	return cycles;
}

void M6800RunEnd()
{
#if defined FBA_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6800RunEnd called without init\n"));
#endif
}

UINT32 M6800GetPC(INT32)
{
#if defined FBA_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6800GetPC called without init\n"));
#endif

	return m6800_get_pc();
}

INT32 M6800MapMemory(UINT8* pMemory, UINT16 nStart, UINT16 nEnd, INT32 nType)
{
#if defined FBA_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6800MapMemory called without init\n"));
#endif

	UINT8 cStart = (nStart >> 8);
	UINT8 **pMemMap = M6800CPUContext[0].pMemMap;

	for (UINT16 i = cStart; i <= (nEnd >> 8); i++) {
		if (nType & MAP_READ)	{
			pMemMap[0     + i] = pMemory + ((i - cStart) << 8);
		}
		if (nType & MAP_WRITE) {
			pMemMap[0x100 + i] = pMemory + ((i - cStart) << 8);
		}
		if (nType & MAP_FETCH) {
			pMemMap[0x200 + i] = pMemory + ((i - cStart) << 8);
		}
	}
	return 0;

}

void M6800SetReadHandler(UINT8 (*pHandler)(UINT16))
{
#if defined FBA_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6800SetReadHandler called without init\n"));
#endif

	M6800CPUContext[0].ReadByte = pHandler;
}

void M6800SetWriteHandler(void (*pHandler)(UINT16, UINT8))
{
#if defined FBA_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6800SetWriteHandler called without init\n"));
#endif

	M6800CPUContext[0].WriteByte = pHandler;
}

void M6800SetReadOpHandler(UINT8 (*pHandler)(UINT16))
{
#if defined FBA_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6800SetReadOpHandler called without init\n"));
#endif

	M6800CPUContext[0].ReadOp = pHandler;
}

void M6800SetReadOpArgHandler(UINT8 (*pHandler)(UINT16))
{
#if defined FBA_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6800SetReadOpArgHandler called without init\n"));
#endif

	M6800CPUContext[0].ReadOpArg = pHandler;
}

void M6800SetReadPortHandler(UINT8 (*pHandler)(UINT16))
{
#if defined FBA_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6800SetReadPortHandler called without init\n"));
#endif

	M6800CPUContext[0].ReadPort = pHandler;
}

void M6800SetWritePortHandler(void (*pHandler)(UINT16, UINT8))
{
#if defined FBA_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6800SetWritePortHandler called without init\n"));
#endif

	M6800CPUContext[0].WritePort = pHandler;
}

UINT8 M6800ReadByte(UINT16 Address)
{
	// check mem map
	UINT8 * pr = M6800CPUContext[0].pMemMap[0x000 | (Address >> 8)];
	if (pr != NULL) {
		return pr[Address & 0xff];
	}
	
	// check handler
	if (M6800CPUContext[0].ReadByte != NULL) {
		return M6800CPUContext[0].ReadByte(Address);
	}
	
	return 0;
}

void M6800WriteByte(UINT16 Address, UINT8 Data)
{
	// check mem map
	UINT8 * pr = M6800CPUContext[0].pMemMap[0x100 | (Address >> 8)];
	if (pr != NULL) {
		pr[Address & 0xff] = Data;
		return;
	}
	
	// check handler
	if (M6800CPUContext[0].WriteByte != NULL) {
		M6800CPUContext[0].WriteByte(Address, Data);
		return;
	}
}

UINT8 M6800ReadOp(UINT16 Address)
{
	// check mem map
	UINT8 * pr = M6800CPUContext[0].pMemMap[0x200 | (Address >> 8)];
	if (pr != NULL) {
		return pr[Address & 0xff];
	}
	
	// check handler
	if (M6800CPUContext[0].ReadOp != NULL) {
		return M6800CPUContext[0].ReadOp(Address);
	}
	
	return 0;
}

UINT8 M6800ReadOpArg(UINT16 Address)
{
	// check mem map
	UINT8 * pr = M6800CPUContext[0].pMemMap[0x200 | (Address >> 8)];
	if (pr != NULL) {
		return pr[Address & 0xff];
	}
	
	// check handler
	if (M6800CPUContext[0].ReadOpArg != NULL) {
		return M6800CPUContext[0].ReadOpArg(Address);
	}
	
	return 0;
}

UINT8 M6800ReadPort(UINT16 Address)
{
	// check handler
	if (M6800CPUContext[0].ReadPort != NULL) {
		return M6800CPUContext[0].ReadPort(Address);
	}
	
	return 0;
}

void M6800WritePort(UINT16 Address, UINT8 Data)
{
	// check handler
	if (M6800CPUContext[0].WritePort != NULL) {
		M6800CPUContext[0].WritePort(Address, Data);
		return;
	}
}

void M6800WriteRom(UINT32 Address, UINT8 Data)
{
#if defined FBA_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6800WriteRom called without init\n"));
#endif

	Address &= 0xffff;

	// check mem map
	UINT8 * pr = M6800CPUContext[0].pMemMap[0x000 | (Address >> 8)];
	UINT8 * pw = M6800CPUContext[0].pMemMap[0x100 | (Address >> 8)];
	UINT8 * pf = M6800CPUContext[0].pMemMap[0x200 | (Address >> 8)];

	if (pr != NULL) {
		pr[Address & 0xff] = Data;
	}
	
	if (pw != NULL) {
		pw[Address & 0xff] = Data;
	}

	if (pf != NULL) {
		pf[Address & 0xff] = Data;
	}

	// check handler
	if (M6800CPUContext[0].WriteByte != NULL) {
		M6800CPUContext[0].WriteByte(Address, Data);
		return;
	}
}

INT32 M6800Scan(INT32 nAction)
{
#if defined FBA_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6800Scan called without init\n"));
#endif

	if (nAction & ACB_DRIVER_DATA) {
		for (INT32 i = 0; i <= nM6800Count; i++) {
			m6800_Regs *R = &M6800CPUContext[i].reg;

			m6800_get_context((void *)&M6800CPUContext[i].reg);
			void (* const * insn)(void) = R->insn;	/* instruction table - save pointers */
			const UINT8 *cycles = R->cycles;	/* clock cycle of instruction table  */

			SCAN_VAR(M6800CPUContext[i].reg);

			R->insn = insn; // restore pointers -dink
			R->cycles = cycles;

			if (nAction & ACB_WRITE) {
				m6800_set_context((void *)&M6800CPUContext[i].reg);
			}

			SCAN_VAR(M6800CPUContext[i].nCyclesTotal);
			SCAN_VAR(M6800CPUContext[i].nCyclesSegment);
			SCAN_VAR(M6800CPUContext[i].nCyclesLeft);
			SCAN_VAR(nM6800CyclesDone[i]);
		}
		
		SCAN_VAR(nM6800CyclesTotal);
	}

	return 0;
}
