#include "burnint.h"
#include "m6809_intf.h"
#include <stddef.h>

#define MAX_CPU		8

INT32 nM6809Count = 0;
static INT32 nActiveCPU = 0;

static M6809Ext *m6809CPUContext = NULL;

cpu_core_config M6809Config =
{
	"M6809",
	M6809CPUPush, //M6809Open,
	M6809CPUPop, //M6809Close,
	M6809CheatRead,
	M6809WriteRom,
	M6809GetActive,
	M6809TotalCycles,
	M6809NewFrame,
	M6809Idle,
	M6809SetIRQLine,
	M6809Run,
	M6809RunEnd,
	M6809Reset,
	M6809Scan,
	M6809Exit,
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

// ## M6809CPUPush() / M6809CPUPop() ## internal helpers for sending signals to other m6809's
struct m6809pstack {
	INT32 nHostCPU;
	INT32 nPushedCPU;
};
#define MAX_PSTACK 10

static m6809pstack pstack[MAX_PSTACK];
static INT32 pstacknum = 0;

void M6809CPUPush(INT32 nCPU)
{
	m6809pstack *p = &pstack[pstacknum++];

	if (pstacknum + 1 >= MAX_PSTACK) {
		bprintf(0, _T("M6809CPUPush(): out of stack!  Possible infinite recursion?  Crash pending..\n"));
	}

	p->nPushedCPU = nCPU;

	p->nHostCPU = M6809GetActive();

	if (p->nHostCPU != p->nPushedCPU) {
		if (p->nHostCPU != -1) M6809Close();
		M6809Open(p->nPushedCPU);
	}
}

void M6809CPUPop()
{
	m6809pstack *p = &pstack[--pstacknum];

	if (p->nHostCPU != p->nPushedCPU) {
		M6809Close();
		if (p->nHostCPU != -1) M6809Open(p->nHostCPU);
	}
}

void M6809Reset()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6809Initted) bprintf(PRINT_ERROR, _T("M6809Reset called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6809Reset called when no CPU open\n"));
#endif

	m6809_reset();
}

void M6809Reset(INT32 nCPU)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6809Initted) bprintf(PRINT_ERROR, _T("M6809Reset called without init\n"));
#endif

	M6809CPUPush(nCPU);

	M6809Reset();

	M6809CPUPop();
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
		m6809CPUContext[i].nCyclesTotal = 0;
	}
}

INT32 M6809TotalCycles()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6809Initted) bprintf(PRINT_ERROR, _T("M6809TotalCycles called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6809TotalCycles called when no CPU open\n"));
#endif

	if (nActiveCPU == -1) return 0; // prevent crash

	return m6809CPUContext[nActiveCPU].nCyclesTotal + m6809_get_segmentcycles();
}

INT32 M6809TotalCycles(INT32 nCPU)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6809Initted) bprintf(PRINT_ERROR, _T("M6809TotalCycles called without init\n"));
#endif

	M6809CPUPush(nCPU);

	INT32 nRet = M6809TotalCycles();

	M6809CPUPop();

	return nRet;
}

INT32 M6809Idle(INT32 cycles)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6809Initted) bprintf(PRINT_ERROR, _T("M6809Idle called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6809Idle called when no CPU open\n"));
#endif

	m6809CPUContext[nActiveCPU].nCyclesTotal += cycles;

	return cycles;
}

INT32 M6809Idle(INT32 nCPU, INT32 nCycles)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6809Initted) bprintf(PRINT_ERROR, _T("M6809Idle called without init\n"));
#endif

	M6809CPUPush(nCPU);

	INT32 nRet = M6809Idle(nCycles);

	M6809CPUPop();

	return nRet;
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
			m6809CPUContext[i].nCyclesTotal = 0;

			for (INT32 j = 0; j < (0x0100 * 3); j++) {
				m6809CPUContext[i].pMemMap[j] = NULL;
			}
		}

		m6809_init(NULL);
	}
	
	m6809CPUContext[cpu].ReadByte = M6809ReadByteDummyHandler;
	m6809CPUContext[cpu].WriteByte = M6809WriteByteDummyHandler;

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
}

void M6809Close()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6809Initted) bprintf(PRINT_ERROR, _T("M6809Close called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6809Close called when no CPU open\n"));
#endif

	m6809_get_context(&m6809CPUContext[nActiveCPU].reg);
	
	nActiveCPU = -1;
}

INT32 M6809GetActive()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6809Initted) bprintf(PRINT_ERROR, _T("M6809GetActive called without init\n"));
	//if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6809GetActive called when no CPU open\n"));
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

void M6809SetIRQLine(INT32 nCPU, const INT32 line, const INT32 status)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6809Initted) bprintf(PRINT_ERROR, _T("M6809SetIRQLine called without init\n"));
#endif

	M6809CPUPush(nCPU);

	M6809SetIRQLine(line, status);

	M6809CPUPop();
}

INT32 M6809Run(INT32 cycles)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6809Initted) bprintf(PRINT_ERROR, _T("M6809Run called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6809Run called when no CPU open\n"));
#endif

	cycles = m6809_execute(cycles);
	
	m6809CPUContext[nActiveCPU].nCyclesTotal += cycles;
	
	return cycles;
}

INT32 M6809Run(INT32 nCPU, INT32 nCycles)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6809Initted) bprintf(PRINT_ERROR, _T("M6809Run called without init\n"));
#endif

	M6809CPUPush(nCPU);

	INT32 nRet = M6809Run(nCycles);

	M6809CPUPop();

	return nRet;
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

void M6809SetCallback(int (*cb)(int))
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6809Initted) bprintf(PRINT_ERROR, _T("M6809SetCallback called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6809SetCallback called when no CPU open\n"));
#endif

	m6809_set_callback(cb);
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
	if (m6809CPUContext[nActiveCPU].ReadByte != NULL) {
		return m6809CPUContext[nActiveCPU].ReadByte(Address);
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
	if (m6809CPUContext[nActiveCPU].ReadByte != NULL) {
		return m6809CPUContext[nActiveCPU].ReadByte(Address);
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

		SCAN_VAR(ptr->nCyclesTotal);
	}
	
	return 0;
}
