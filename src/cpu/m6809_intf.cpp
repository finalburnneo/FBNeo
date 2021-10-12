#include "burnint.h"
#include "m6809_intf.h"
#include <stddef.h>

#define MAX_CPU		8

INT32 nM6809Count = 0;
static INT32 nActiveCPU = 0;

static M6809Ext *m6809CPUContext = NULL;

static INT32 nM6809CyclesDone[MAX_CPU];
INT32 nM6809CyclesTotal;

static INT32 core_idle(INT32 cycles)
{
	return M6809Idle(cycles);
}

static void core_set_irq(INT32 cpu, INT32 line, INT32 state)
{
	INT32 active = nActiveCPU;

	if (active != cpu)
	{
		if (active != -1) M6809Close();
		M6809Open(cpu);
	}

	M6809SetIRQLine(line, state);

	if (active != cpu)
	{
		M6809Close();
		if (active != -1) M6809Open(active);
	}
}

cpu_core_config M6809Config =
{
	"M6809",
	M6809Open,
	M6809Close,
	M6809CheatRead,
	M6809WriteRom,
	M6809GetActive,
	M6809TotalCycles,
	M6809NewFrame,
	core_idle,	
	core_set_irq,
	M6809Run,
	M6809RunEnd,
	M6809Reset,
	0x10000,
	0
};

static UINT8 M6809ReadByteDummyHandler(UINT16)
{
	return 0;
}

static void M6809WriteByteDummyHandler(UINT16, UINT8)
{
}

static UINT8 M6809ReadOpDummyHandler(UINT16)
{
	return 0;
}

static UINT8 M6809ReadOpArgDummyHandler(UINT16)
{
	return 0;
}

void M6809Reset()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6809Initted) bprintf(PRINT_ERROR, _T("M6809Reset called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6809Reset called when no CPU open\n"));
#endif

	m6809_reset();
}

UINT16 M6809GetPC()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6809Initted) bprintf(PRINT_ERROR, _T("M6809GetPC called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6809GetPC called when no CPU open\n"));
#endif

	return m6809_get_pc();
}

UINT16 M6809GetPrevPC()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6809Initted) bprintf(PRINT_ERROR, _T("M6809GetPrevPC called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6809GetPrevPC called when no CPU open\n"));
#endif

	return m6809_get_prev_pc();
}

void M6809NewFrame()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6809Initted) bprintf(PRINT_ERROR, _T("M6809NewFrame called without init\n"));
#endif

	for (INT32 i = 0; i < nM6809Count+1; i++) {
		nM6809CyclesDone[i] = 0;
	}
	nM6809CyclesTotal = 0;
}

INT32 M6809TotalCycles()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6809Initted) bprintf(PRINT_ERROR, _T("M6809TotalCycles called without init\n"));
#endif

	return nM6809CyclesTotal + m6809_get_segmentcycles();
}

INT32 M6809Idle(INT32 cycles)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6809Initted) bprintf(PRINT_ERROR, _T("M6809Idle called without init\n"));
#endif

	nM6809CyclesTotal += cycles;

	return cycles;
}

UINT8 M6809CheatRead(UINT32 a)
{
	return M6809ReadByte(a);
}

INT32 M6809Init(INT32 cpu)
{
	DebugCPU_M6809Initted = 1;

	nActiveCPU = -1;
	nM6809Count = cpu;

#if defined FBNEO_DEBUG
	if (cpu >= MAX_CPU-1) bprintf (0, _T("M6809Init called with greater than maximum (%d) cpu number (%d)\n"), MAX_CPU-1, cpu);
#endif

	if (m6809CPUContext == NULL) {
		m6809CPUContext = (M6809Ext*)malloc(MAX_CPU * sizeof(M6809Ext));

		if (m6809CPUContext == NULL) {
#if defined FBNEO_DEBUG
			if (cpu >= MAX_CPU-1) bprintf (0, _T("M6809Init failed to initialize context!\n"));
#endif
			return 1;
		}
		
		memset(m6809CPUContext, 0, MAX_CPU * sizeof(M6809Ext));

		for (INT32 i = 0; i < MAX_CPU; i++) {
			m6809CPUContext[i].ReadByte = M6809ReadByteDummyHandler;
			m6809CPUContext[i].WriteByte = M6809WriteByteDummyHandler;
			m6809CPUContext[i].ReadOp = M6809ReadOpDummyHandler;
			m6809CPUContext[i].ReadOpArg = M6809ReadOpArgDummyHandler;
			nM6809CyclesDone[i] = 0;

			for (INT32 j = 0; j < (0x0100 * 3); j++) {
				m6809CPUContext[i].pMemMap[j] = NULL;
			}
		}

		m6809_init(NULL);
	}
	
	m6809CPUContext[cpu].ReadByte = M6809ReadByteDummyHandler;
	m6809CPUContext[cpu].WriteByte = M6809WriteByteDummyHandler;
	m6809CPUContext[cpu].ReadOp = M6809ReadOpDummyHandler;
	m6809CPUContext[cpu].ReadOpArg = M6809ReadOpArgDummyHandler;

	CpuCheatRegister(cpu, &M6809Config);

	return 0;
}

void M6809Exit()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6809Initted) bprintf(PRINT_ERROR, _T("M6809Exit called without init\n"));
#endif

	nM6809Count = 0;

	if (m6809CPUContext) {
		free(m6809CPUContext);
		m6809CPUContext = NULL;
	}
	
	DebugCPU_M6809Initted = 0;
}

void M6809Open(INT32 num)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6809Initted) bprintf(PRINT_ERROR, _T("M6809Open called without init\n"));
	if (num > nM6809Count) bprintf(PRINT_ERROR, _T("M6809Open called with invalid index %x\n"), num);
	if (nActiveCPU != -1) bprintf(PRINT_ERROR, _T("M6809Open called when CPU already open with index %x\n"), num);
#endif

	nActiveCPU = num;
	
	m6809_set_context(&m6809CPUContext[nActiveCPU].reg);
	
	nM6809CyclesTotal = nM6809CyclesDone[nActiveCPU];
}

void M6809Close()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6809Initted) bprintf(PRINT_ERROR, _T("M6809Close called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6809Close called when no CPU open\n"));
#endif

	m6809_get_context(&m6809CPUContext[nActiveCPU].reg);
	
	nM6809CyclesDone[nActiveCPU] = nM6809CyclesTotal;
	
	nActiveCPU = -1;
}

INT32 M6809GetActive()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6809Initted) bprintf(PRINT_ERROR, _T("M6809GetActive called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6809GetActive called when no CPU open\n"));
#endif

	return nActiveCPU;
}

void M6809SetIRQLine(INT32 vector, INT32 status)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6809Initted) bprintf(PRINT_ERROR, _T("M6809SetIRQLine called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6809SetIRQLine called when no CPU open\n"));
#endif

	if (status == CPU_IRQSTATUS_NONE) {
		m6809_set_irq_line(vector, 0);
	}
	
	if (status == CPU_IRQSTATUS_ACK) {
		m6809_set_irq_line(vector, 1);
	}

	if (status == CPU_IRQSTATUS_HOLD) {
		m6809_set_irq_line(vector, 2);
	}
	
	if (status == CPU_IRQSTATUS_AUTO) {
		m6809_set_irq_line(vector, 1);
		m6809_execute(0);
		m6809_set_irq_line(vector, 0);
		m6809_execute(0);
	}
}

INT32 M6809Run(INT32 cycles)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6809Initted) bprintf(PRINT_ERROR, _T("M6809Run called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6809Run called when no CPU open\n"));
#endif

	cycles = m6809_execute(cycles);
	
	nM6809CyclesTotal += cycles;
	
	return cycles;
}

void M6809RunEnd()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6809Initted) bprintf(PRINT_ERROR, _T("M6809RunEnd called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6809RunEnd called when no CPU open\n"));
#endif

	m6809_end_timeslice();
}

INT32 M6809MapMemory(UINT8* pMemory, UINT16 nStart, UINT16 nEnd, INT32 nType)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6809Initted) bprintf(PRINT_ERROR, _T("M6809MapMemory called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6809MapMemory called when no CPU open\n"));
#endif

	UINT8 cStart = (nStart >> 8);
	UINT8 **pMemMap = m6809CPUContext[nActiveCPU].pMemMap;

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

INT32 M6809UnmapMemory(UINT16 nStart, UINT16 nEnd, INT32 nType)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6809Initted) bprintf(PRINT_ERROR, _T("M6809UnmapMemory called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6809UnmapMemory called when no CPU open\n"));
#endif

	UINT8 cStart = (nStart >> 8);
	UINT8 **pMemMap = m6809CPUContext[nActiveCPU].pMemMap;

	for (UINT16 i = cStart; i <= (nEnd >> 8); i++) {
		if (nType & MAP_READ)	{
			pMemMap[0     + i] = NULL;
		}
		if (nType & MAP_WRITE) {
			pMemMap[0x100 + i] = NULL;
		}
		if (nType & MAP_FETCH) {
			pMemMap[0x200 + i] = NULL;
		}
	}
	return 0;

}

void M6809SetReadHandler(UINT8 (*pHandler)(UINT16))
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6809Initted) bprintf(PRINT_ERROR, _T("M6809SetReadHandler called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6809SetReadHandler called when no CPU open\n"));
#endif

	m6809CPUContext[nActiveCPU].ReadByte = pHandler;
}

void M6809SetWriteHandler(void (*pHandler)(UINT16, UINT8))
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6809Initted) bprintf(PRINT_ERROR, _T("M6809SetWriteHandler called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6809SetWriteHandler called when no CPU open\n"));
#endif

	m6809CPUContext[nActiveCPU].WriteByte = pHandler;
}

void M6809SetReadOpHandler(UINT8 (*pHandler)(UINT16))
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6809Initted) bprintf(PRINT_ERROR, _T("M6809SetReadOpHandler called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6809SetReadOpHandler called when no CPU open\n"));
#endif

	m6809CPUContext[nActiveCPU].ReadOp = pHandler;
}

void M6809SetReadOpArgHandler(UINT8 (*pHandler)(UINT16))
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6809Initted) bprintf(PRINT_ERROR, _T("M6809SetReadOpArgHandler called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6809SetReadOpArgHandler called when no CPU open\n"));
#endif

	m6809CPUContext[nActiveCPU].ReadOpArg = pHandler;
}

UINT8 M6809ReadByte(UINT16 Address)
{
	// check mem map
	UINT8 * pr = m6809CPUContext[nActiveCPU].pMemMap[0x000 | (Address >> 8)];
	if (pr != NULL) {
		return pr[Address & 0xff];
	}
	
	// check handler
	if (m6809CPUContext[nActiveCPU].ReadByte != NULL) {
		return m6809CPUContext[nActiveCPU].ReadByte(Address);
	}
	
	return 0;
}

void M6809WriteByte(UINT16 Address, UINT8 Data)
{
	// check mem map
	UINT8 * pr = m6809CPUContext[nActiveCPU].pMemMap[0x100 | (Address >> 8)];
	if (pr != NULL) {
		pr[Address & 0xff] = Data;
		return;
	}
	
	// check handler
	if (m6809CPUContext[nActiveCPU].WriteByte != NULL) {
		m6809CPUContext[nActiveCPU].WriteByte(Address, Data);
		return;
	}
}

UINT8 M6809ReadOp(UINT16 Address)
{
	// check mem map
	UINT8 * pr = m6809CPUContext[nActiveCPU].pMemMap[0x200 | (Address >> 8)];
	if (pr != NULL) {
		return pr[Address & 0xff];
	}
	
	// check handler
	if (m6809CPUContext[nActiveCPU].ReadOp != NULL) {
		return m6809CPUContext[nActiveCPU].ReadOp(Address);
	}
	
	return 0;
}

UINT8 M6809ReadOpArg(UINT16 Address)
{
	// check mem map
	UINT8 * pr = m6809CPUContext[nActiveCPU].pMemMap[0x000 | (Address >> 8)];
	if (pr != NULL) {
		return pr[Address & 0xff];
	}
	
	// check handler
	if (m6809CPUContext[nActiveCPU].ReadOpArg != NULL) {
		return m6809CPUContext[nActiveCPU].ReadOpArg(Address);
	}
	
	return 0;
}

void M6809WriteRom(UINT32 Address, UINT8 Data)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6809Initted) bprintf(PRINT_ERROR, _T("M6809WriteRom called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6809WriteRom called when no CPU open\n"));
#endif
	Address &= 0xffff;

	UINT8 * pr = m6809CPUContext[nActiveCPU].pMemMap[0x000 | (Address >> 8)];
	UINT8 * pw = m6809CPUContext[nActiveCPU].pMemMap[0x100 | (Address >> 8)];
	UINT8 * pf = m6809CPUContext[nActiveCPU].pMemMap[0x200 | (Address >> 8)];

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
	if (m6809CPUContext[nActiveCPU].WriteByte != NULL) {
		m6809CPUContext[nActiveCPU].WriteByte(Address, Data);
		return;
	}
}

INT32 M6809Scan(INT32 nAction)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6809Initted) bprintf(PRINT_ERROR, _T("M6809Scan called without init\n"));
#endif

	struct BurnArea ba;
	
	if ((nAction & ACB_DRIVER_DATA) == 0) {
		return 1;
	}

	for (INT32 i = 0; i < nM6809Count+1; i++) {

		M6809Ext *ptr = &m6809CPUContext[i];

		char szName[] = "M6809 #n";
		szName[7] = '0' + i;

		memset(&ba, 0, sizeof(ba));
		ba.Data = &m6809CPUContext[i].reg;
		ba.nLen = STRUCT_SIZE_HELPER(m6809_Regs, nmi_state);
		ba.szName = szName;
		BurnAcb(&ba);

		// necessary?
		SCAN_VAR(ptr->nCyclesTotal);
		SCAN_VAR(ptr->nCyclesSegment);
		SCAN_VAR(ptr->nCyclesLeft);
	}
	
	return 0;
}
