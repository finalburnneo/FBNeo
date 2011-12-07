#include "burnint.h"
#include "m6800_intf.h"

#define MAX_CPU		1

INT32 nM6800Count = 0;
static INT32 nCpuType = 0;

static M6800Ext *M6800CPUContext;

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

	for (INT32 i = 0; i < nM6800Count; i++) {
		nM6800CyclesDone[i] = 0;
	}
	nM6800CyclesTotal = 0;
}

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
	
	if (type == CPU_TYPE_M6800) m6800_init();
	if (type == CPU_TYPE_HD63701) hd63701_init();
	if (type == CPU_TYPE_M6803) m6803_init();
	if (type == CPU_TYPE_M6801) m6801_init();
	
	nCpuType = type;

	for (INT32 i = 0; i < num; i++)
		CpuCheatRegister(0x0007, i);

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

void M6800SetIRQ(INT32 vector, INT32 status)
{
#if defined FBA_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6800SetIRQ called without init\n"));
	if (nCpuType != CPU_TYPE_M6800) bprintf(PRINT_ERROR, _T("M6800SetIRQ called with invalid CPU Type\n"));
#endif

	if (status == M6800_IRQSTATUS_NONE) {
		m6800_set_irq_line(vector, 0);
	}
	
	if (status == M6800_IRQSTATUS_ACK) {
		m6800_set_irq_line(vector, 1);
	}
	
	if (status == M6800_IRQSTATUS_AUTO) {
		m6800_set_irq_line(vector, 1);
		m6800_execute(0);
		m6800_set_irq_line(vector, 0);
		m6800_execute(0);
	}
}

void HD63701SetIRQ(INT32 vector, INT32 status)
{
#if defined FBA_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("HD63701SetIRQ called without init\n"));
	if (nCpuType != CPU_TYPE_HD63701) bprintf(PRINT_ERROR, _T("HD63701SetIRQ called with invalid CPU Type\n"));
#endif

	if (status == HD63701_IRQSTATUS_NONE) {
		m6800_set_irq_line(vector, 0);
	}
	
	if (status == HD63701_IRQSTATUS_ACK) {
		m6800_set_irq_line(vector, 1);
	}
	
	if (status == HD63701_IRQSTATUS_AUTO) {
		m6800_set_irq_line(vector, 1);
		hd63701_execute(0);
		m6800_set_irq_line(vector, 0);
		hd63701_execute(0);
	}
}

void M6803SetIRQ(INT32 vector, INT32 status)
{
#if defined FBA_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6803SetIRQ called without init\n"));
	if (nCpuType != CPU_TYPE_M6803) bprintf(PRINT_ERROR, _T("M6803SetIRQ called with invalid CPU Type\n"));
#endif

	if (status == M6803_IRQSTATUS_NONE) {
		m6800_set_irq_line(vector, 0);
	}
	
	if (status == M6803_IRQSTATUS_ACK) {
		m6800_set_irq_line(vector, 1);
	}
	
	if (status == M6803_IRQSTATUS_AUTO) {
		m6800_set_irq_line(vector, 1);
		m6803_execute(0);
		m6800_set_irq_line(vector, 0);
		m6803_execute(0);
	}
}

void M6801SetIRQ(INT32 vector, INT32 status)
{
#if defined FBA_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6801SetIRQ called without init\n"));
	if (nCpuType != CPU_TYPE_M6801) bprintf(PRINT_ERROR, _T("M6800SetIRQ called with invalid CPU Type\n"));
#endif

	if (status == M6801_IRQSTATUS_NONE) {
		m6800_set_irq_line(vector, 0);
	}
	
	if (status == M6801_IRQSTATUS_ACK) {
		m6800_set_irq_line(vector, 1);
	}
	
	if (status == M6801_IRQSTATUS_AUTO) {
		m6800_set_irq_line(vector, 1);
		m6803_execute(0);
		m6800_set_irq_line(vector, 0);
		m6803_execute(0);
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

void M6800RunEnd()
{
#if defined FBA_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6800RunEnd called without init\n"));
#endif
}

INT32 M6800GetPC()
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
		if (nType & M6800_READ)	{
			pMemMap[0     + i] = pMemory + ((i - cStart) << 8);
		}
		if (nType & M6800_WRITE) {
			pMemMap[0x100 + i] = pMemory + ((i - cStart) << 8);
		}
		if (nType & M6800_FETCH) {
			pMemMap[0x200 + i] = pMemory + ((i - cStart) << 8);
		}
	}
	return 0;

}

void M6800SetReadByteHandler(UINT8 (*pHandler)(UINT16))
{
#if defined FBA_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6800SetReadByteHandler called without init\n"));
#endif

	M6800CPUContext[0].ReadByte = pHandler;
}

void M6800SetWriteByteHandler(void (*pHandler)(UINT16, UINT8))
{
#if defined FBA_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6800SetWriteByteHandler called without init\n"));
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

void M6800WriteRom(UINT16 Address, UINT8 Data)
{
#if defined FBA_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6800WriteRom called without init\n"));
#endif

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

	struct BurnArea ba;

	if (nAction & ACB_DRIVER_DATA) {
		m6800_Regs *tmp;
		void (* const * insn)(void);
		const UINT8 *cycles;

		for (INT32 i = 0; i < nM6800Count; i++) {
			tmp = &M6800CPUContext[i].reg;
	
			char szName[] = "M6800 #n";
			szName[7] = '0' + i;
	
			cycles = tmp->cycles;
			insn = tmp->insn;
	
			ba.Data = &M6800CPUContext[i].reg;
			ba.nLen = sizeof(M6800CPUContext[i].reg);
			ba.szName = szName;
			BurnAcb(&ba);
	
			tmp->cycles = cycles;
			tmp->insn = insn;
		}
	}

	return 0;
}
