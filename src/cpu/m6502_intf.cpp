#include "burnint.h"
#include "m6502_intf.h"
#include <stddef.h>

#define MAX_CPU		8

INT32 nM6502Count = 0;
static INT32 nActiveCPU = 0;

static M6502Ext *m6502CPUContext[MAX_CPU] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
static M6502Ext *pCurrentCPU;

cpu_core_config M6502Config =
{
	"M6502",
	M6502CPUPush, //M6502Open,
	M6502CPUPop, //M6502Close,
	M6502CheatRead,
	M6502WriteRom,
	M6502GetActive,
	M6502TotalCycles,
	M6502NewFrame,
	M6502Idle,
	M6502SetIRQLine,
	M6502Run,
	M6502RunEnd,
	M6502Reset,
	M6502Scan,
	M6502Exit,
	0x10000,
	0
};

static UINT8 M6502ReadPortDummyHandler(UINT16)
{
	return 0;
}

static void M6502WritePortDummyHandler(UINT16, UINT8)
{
}

static UINT8 M6502ReadByteDummyHandler(UINT16)
{
	return 0;
}

static void M6502WriteByteDummyHandler(UINT16, UINT8)
{
}


// ## M6502CPUPush() / M6502CPUPop() ## internal helpers for sending signals to other m6809's
struct m6809pstack {
	INT32 nHostCPU;
	INT32 nPushedCPU;
};
#define MAX_PSTACK 10

static m6809pstack pstack[MAX_PSTACK];
static INT32 pstacknum = 0;

void M6502CPUPush(INT32 nCPU)
{
	m6809pstack *p = &pstack[pstacknum++];

	if (pstacknum + 1 >= MAX_PSTACK) {
		bprintf(0, _T("M6502CPUPush(): out of stack!  Possible infinite recursion?  Crash pending..\n"));
	}

	p->nPushedCPU = nCPU;

	p->nHostCPU = M6502GetActive();

	if (p->nHostCPU != p->nPushedCPU) {
		if (p->nHostCPU != -1) M6502Close();
		M6502Open(p->nPushedCPU);
	}
}

void M6502CPUPop()
{
	m6809pstack *p = &pstack[--pstacknum];

	if (p->nHostCPU != p->nPushedCPU) {
		M6502Close();
		if (p->nHostCPU != -1) M6502Open(p->nHostCPU);
	}
}

void M6502Reset()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6502Initted) bprintf(PRINT_ERROR, _T("M6502Reset called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6502Reset called with no CPU open\n"));
#endif

	pCurrentCPU->nCyclesStall = 0;

	pCurrentCPU->reset();
}

void M6502Reset(INT32 nCPU)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6502Initted) bprintf(PRINT_ERROR, _T("M6502Reset called without init\n"));
#endif

	M6502CPUPush(nCPU);

	M6502Reset();

	M6502CPUPop();
}

void M6502NewFrame()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6502Initted) bprintf(PRINT_ERROR, _T("M6502NewFrame called without init\n"));
#endif

	for (INT32 i = 0; i < nM6502Count; i++) {
		m6502CPUContext[i]->nCyclesTotal = 0;
	}
}

INT32 M6502Idle(INT32 nCycles)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6502Initted) bprintf(PRINT_ERROR, _T("M6502Idle called without init\n"));
#endif

	pCurrentCPU->nCyclesTotal += nCycles;

	return nCycles;
}

INT32 M6502Idle(INT32 nCPU, INT32 nCycles)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6502Initted) bprintf(PRINT_ERROR, _T("M6502Idle called without init\n"));
#endif

	M6502CPUPush(nCPU);

	INT32 nRet = M6502Idle(nCycles);

	M6502CPUPop();

	return nRet;
}

INT32 M6502Stall(INT32 nCycles)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6502Initted) bprintf(PRINT_ERROR, _T("M6502Stall called without init\n"));
#endif

	pCurrentCPU->nCyclesStall += nCycles;

	return nCycles;
}

INT32 M6502Stall(INT32 nCPU, INT32 nCycles)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6502Initted) bprintf(PRINT_ERROR, _T("M6502Stall called without init\n"));
#endif

	M6502CPUPush(nCPU);

	INT32 nRet = M6502Stall(nCycles);

	M6502CPUPop();

	return nRet;
}

UINT8 M6502CheatRead(UINT32 a)
{
	return M6502ReadByte(a);
}

static UINT8 decocpu7Decode(UINT16 /*address*/,UINT8 op)
{
	return (op & 0x13) | ((op & 0x80) >> 5) | ((op & 0x64) << 1) | ((op & 0x08) << 2);
}

INT32 M6502Init(INT32 cpu, INT32 type)
{
	DebugCPU_M6502Initted = 1;
	
	nM6502Count++;
	nActiveCPU = -1;

	m6502CPUContext[cpu] = (M6502Ext*)BurnMalloc(sizeof(M6502Ext));

	pCurrentCPU = m6502CPUContext[cpu];

	memset(pCurrentCPU, 0, sizeof(M6502Ext));

	for (INT32 i = 0; i < 0x100; i++) {
		pCurrentCPU->opcode_reorder[i] = i;
	}

	switch (type)
	{
		case TYPE_M6502:
		case TYPE_M6504:
			pCurrentCPU->execute = m6502_execute;
			pCurrentCPU->reset = m6502_reset;
			pCurrentCPU->init = m6502_init;
			pCurrentCPU->set_irq_line = m6502_set_irq_line;
		break;

		case TYPE_DECOCPU7:
			pCurrentCPU->execute = decocpu7_execute;
			pCurrentCPU->reset = m6502_reset;
			pCurrentCPU->init = m6502_init;
			pCurrentCPU->set_irq_line = m6502_set_irq_line;
		break;

		case TYPE_DECO222:
		case TYPE_DECOC10707:
		{
			pCurrentCPU->execute = m6502_execute;
			pCurrentCPU->reset = m6502_reset;
			pCurrentCPU->init = m6502_init;
			pCurrentCPU->set_irq_line = m6502_set_irq_line;

			for (INT32 i = 0; i < 0x100; i++) {
				pCurrentCPU->opcode_reorder[i] = (i & 0x9f) | ((i >> 1) & 0x20) | ((i & 0x20) << 1);
			}
		}
		break;

		case TYPE_M65C02:
			pCurrentCPU->execute = m65c02_execute;
			pCurrentCPU->reset = m65c02_reset;
			pCurrentCPU->init = m65c02_init;
			pCurrentCPU->set_irq_line = m65c02_set_irq_line;
		break;

		case TYPE_DECO16:
			pCurrentCPU->execute = deco16_execute;
			pCurrentCPU->reset = deco16_reset;
			pCurrentCPU->init = deco16_init;
			pCurrentCPU->set_irq_line = deco16_set_irq_line;
		break;

		case TYPE_N2A03:
			pCurrentCPU->execute = m6502_execute;
			pCurrentCPU->reset = m6502_reset;
			pCurrentCPU->init = n2a03_init;		// different
			pCurrentCPU->set_irq_line = m6502_set_irq_line;
		break;

		case TYPE_M65SC02:
			pCurrentCPU->execute = m65c02_execute;
			pCurrentCPU->reset = m65c02_reset;
			pCurrentCPU->init = m65sc02_init;	// different
			pCurrentCPU->set_irq_line = m65c02_set_irq_line;
		break;

		case TYPE_M6510:
		case TYPE_M6510T:
		case TYPE_M7501:
		case TYPE_M8502:
			pCurrentCPU->execute = m6502_execute;
			pCurrentCPU->reset = m6510_reset;	// different
			pCurrentCPU->init = m6510_init;		// different
			pCurrentCPU->set_irq_line = m6502_set_irq_line;
		break;
	}

	pCurrentCPU->ReadPort = M6502ReadPortDummyHandler;
	pCurrentCPU->WritePort = M6502WritePortDummyHandler;
	pCurrentCPU->ReadByte = M6502ReadByteDummyHandler;
	pCurrentCPU->WriteByte = M6502WriteByteDummyHandler;
	
	pCurrentCPU->AddressMask = (1 << 16) - 1; // cpu range
	
	for (INT32 j = 0; j < (0x0100 * 3); j++) {
		pCurrentCPU->pMemMap[j] = NULL;
	}
	
	pCurrentCPU->nCyclesTotal = 0;
	pCurrentCPU->nCyclesStall = 0;

	M6502Open(cpu);
	pCurrentCPU->init();
	M6502Close();

	if (type == TYPE_DECOCPU7) {
		M6502Open(cpu);
		DecoCpu7SetDecode(decocpu7Decode);
		M6502Close();
	}

	CpuCheatRegister(cpu, &M6502Config);

	return 0;
}

void M6502Exit()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6502Initted) bprintf(PRINT_ERROR, _T("M6502Exit called without init\n"));
#endif

	if (!DebugCPU_M6502Initted) return;

	for (INT32 i = 0; i < MAX_CPU; i++) {
		if (m6502CPUContext[i]) {
			BurnFree(m6502CPUContext[i]);
			m6502CPUContext[i] = NULL;
		}
	}

	m6502_core_exit();

	nM6502Count = 0;
	
	DebugCPU_M6502Initted = 0;
}

void M6502SetOpcodeDecode(UINT8 *table)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6502Initted) bprintf(PRINT_ERROR, _T("M6502SetOpcodeDecode called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6502SetOpcodeDecode called with no CPU open\n"));
#endif
	memcpy (pCurrentCPU->opcode_reorder, table, 0x100);
}

void M6502Open(INT32 num)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6502Initted) bprintf(PRINT_ERROR, _T("M6502Open called without init\n"));
	if (num >= nM6502Count) bprintf(PRINT_ERROR, _T("M6502Open called with invalid index %x\n"), num);
	if (nActiveCPU != -1) bprintf(PRINT_ERROR, _T("M6502Open called with CPU already open with index %x\n"), num);
#endif

	nActiveCPU = num;

	pCurrentCPU = m6502CPUContext[num];

	m6502_set_context(&pCurrentCPU->reg);
}

void M6502Close()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6502Initted) bprintf(PRINT_ERROR, _T("M6502Close called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6502Close called with no CPU open\n"));
#endif

	m6502_get_context(&pCurrentCPU->reg);

	pCurrentCPU = NULL; // cause problems! 
	
	nActiveCPU = -1;
}

INT32 M6502GetActive()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6502Initted) bprintf(PRINT_ERROR, _T("M6502GetActive called without init\n"));
	//if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6502GetActive called with no CPU open\n"));
#endif

	return nActiveCPU;
}

void M6502ReleaseSlice()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6502Initted) bprintf(PRINT_ERROR, _T("M6502ReleaseSlice called without init\n"));
#endif

	m6502_releaseslice();
}

void M6502SetIRQLine(INT32 vector, INT32 status)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6502Initted) bprintf(PRINT_ERROR, _T("M6502SetIRQLineLine called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6502SetIRQLineLine called with no CPU open\n"));
#endif

	if (status == CPU_IRQSTATUS_NONE) {
		pCurrentCPU->set_irq_line(vector, 0);
		return;
	}
	
	if (status == CPU_IRQSTATUS_ACK) {
		pCurrentCPU->set_irq_line(vector, 1);
		return;
	}

	if (status == CPU_IRQSTATUS_HOLD && vector == CPU_IRQLINE_NMI) {
		status = CPU_IRQSTATUS_AUTO;
	}

	if (status == CPU_IRQSTATUS_AUTO) {
		if (vector == CPU_IRQLINE_NMI /* 0x20 */) {
			m6502_set_nmi_hold(); // it will auto-clear on ack
			pCurrentCPU->set_irq_line(vector, 1);
			return;
		} else {
			pCurrentCPU->set_irq_line(vector, 1);
			pCurrentCPU->execute(0);
			pCurrentCPU->set_irq_line(vector, 0);
			pCurrentCPU->execute(0);
			return;
		}
	}

	if (status == CPU_IRQSTATUS_HOLD) {
		m6502_set_irq_hold();
		pCurrentCPU->set_irq_line(vector, 1);
		return;
	}
}

void M6502SetIRQLine(INT32 nCPU, const INT32 line, const INT32 status)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6502Initted) bprintf(PRINT_ERROR, _T("M6502SetIRQLine called without init\n"));
#endif

	M6502CPUPush(nCPU);

	M6502SetIRQLine(line, status);

	M6502CPUPop();
}

INT32 M6502Run(INT32 cycles)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6502Initted) bprintf(PRINT_ERROR, _T("M6502Run called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6502Run called with no CPU open\n"));
#endif

	INT32 nDelayed = 0;  // handle delayed cycle counts (from M6502Stall())

	while (pCurrentCPU->nCyclesStall && cycles) {
		pCurrentCPU->nCyclesStall--;
		cycles--;
		nDelayed++;
		pCurrentCPU->nCyclesTotal++;
	}

	if (cycles)
		cycles = pCurrentCPU->execute(cycles);

	pCurrentCPU->nCyclesTotal += cycles;

	return cycles + nDelayed;
}

INT32 M6502Run(INT32 nCPU, INT32 nCycles)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6502Initted) bprintf(PRINT_ERROR, _T("M6502Run called without init\n"));
#endif

	M6502CPUPush(nCPU);

	INT32 nRet = M6502Run(nCycles);

	M6502CPUPop();

	return nRet;
}

INT32 M6502TotalCycles()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6502Initted) bprintf(PRINT_ERROR, _T("M6502TotalCycles called without init\n"));
#endif

	if (pCurrentCPU == NULL) return 0; // let's not crash here..

	return pCurrentCPU->nCyclesTotal + m6502_get_segmentcycles();
}

INT32 M6502TotalCycles(INT32 nCPU)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6502Initted) bprintf(PRINT_ERROR, _T("M6502TotalCycles called without init\n"));
#endif

	M6502CPUPush(nCPU);

	INT32 nRet = M6502TotalCycles();

	M6502CPUPop();

	return nRet;
}

INT32 M6502MapMemory(UINT8* pMemory, UINT16 nStart, UINT16 nEnd, INT32 nType)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6502Initted) bprintf(PRINT_ERROR, _T("M6502MapMemory called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6502MapMemory called with no CPU open\n"));
#endif

	UINT8 cStart = (nStart >> 8);
	UINT8 **pMemMap = pCurrentCPU->pMemMap;

	for (UINT16 i = cStart; i <= (nEnd >> 8); i++) {
		if (nType & MAP_READ)	{
			pMemMap[0     + i] = (pMemory == NULL) ? NULL : (pMemory + ((i - cStart) << 8));
		}
		if (nType & MAP_WRITE) {
			pMemMap[0x100 + i] = (pMemory == NULL) ? NULL : (pMemory + ((i - cStart) << 8));
		}
		if (nType & MAP_FETCH) {
			pMemMap[0x200 + i] = (pMemory == NULL) ? NULL : (pMemory + ((i - cStart) << 8));
		}
	}
	return 0;
}

void M6502SetAddressMask(UINT16 RangeMask)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6502Initted) bprintf(PRINT_ERROR, _T("M6502SetAddressMask called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6502SetAddressMask called with no CPU open\n"));
	if ((RangeMask & 0xff) != 0xff) bprintf (PRINT_ERROR, _T("M6502SetAddressMask with likely bad mask value (%4.4x)!\n"), RangeMask);
#endif

	pCurrentCPU->AddressMask = RangeMask;

}

void M6502SetReadPortHandler(UINT8 (*pHandler)(UINT16))
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6502Initted) bprintf(PRINT_ERROR, _T("M6502SetReadPortHandler called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6502SetReadPortHandler called with no CPU open\n"));
#endif
	pCurrentCPU->ReadPort = pHandler;
}

void M6502SetWritePortHandler(void (*pHandler)(UINT16, UINT8))
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6502Initted) bprintf(PRINT_ERROR, _T("M6502SetWritePortHandler called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6502SetWritePortHandler called with no CPU open\n"));
#endif

	pCurrentCPU->WritePort = pHandler;
}

void M6502SetReadHandler(UINT8 (*pHandler)(UINT16))
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6502Initted) bprintf(PRINT_ERROR, _T("M6502SetReadHandler called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6502SetReadHandler called with no CPU open\n"));
#endif

	pCurrentCPU->ReadByte = pHandler;
}

void M6502SetWriteHandler(void (*pHandler)(UINT16, UINT8))
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6502Initted) bprintf(PRINT_ERROR, _T("M6502SetWriteHandler called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6502SetWriteHandler called with no CPU open\n"));
#endif

	pCurrentCPU->WriteByte = pHandler;
}

UINT8 M6502ReadPort(UINT16 Address)
{
	// check handler
	if (pCurrentCPU->ReadPort != NULL) {
		return pCurrentCPU->ReadPort(Address);
	}
	
	return 0;
}

void M6502WritePort(UINT16 Address, UINT8 Data)
{
	// check handler
	if (pCurrentCPU->WritePort != NULL) {
		pCurrentCPU->WritePort(Address, Data);
		return;
	}
}

UINT8 M6502ReadByte(UINT16 Address)
{
	Address &= pCurrentCPU->AddressMask;

	// check mem map
	UINT8 * pr = pCurrentCPU->pMemMap[0x000 | (Address >> 8)];
	if (pr != NULL) {
		return pr[Address & 0xff];
	}
	
	// check handler
	if (pCurrentCPU->ReadByte != NULL) {
		return pCurrentCPU->ReadByte(Address);
	}
	
	return 0;
}

void M6502WriteByte(UINT16 Address, UINT8 Data)
{
	Address &= pCurrentCPU->AddressMask;

	// check mem map
	UINT8 * pr = pCurrentCPU->pMemMap[0x100 | (Address >> 8)];
	if (pr != NULL) {
		pr[Address & 0xff] = Data;
		return;
	}
	
	// check handler
	if (pCurrentCPU->WriteByte != NULL) {
		pCurrentCPU->WriteByte(Address, Data);
		return;
	}
}

UINT8 M6502ReadOp(UINT16 Address)
{
	Address &= pCurrentCPU->AddressMask;

	// check mem map
	UINT8 * pr = pCurrentCPU->pMemMap[0x200 | (Address >> 8)];
	if (pr != NULL) {
		return pCurrentCPU->opcode_reorder[pr[Address & 0xff]];
	}
	
	// check handler
	if (pCurrentCPU->ReadByte != NULL) {
		return pCurrentCPU->opcode_reorder[pCurrentCPU->ReadByte(Address)];
	}
	
	return 0;
}

UINT8 M6502ReadOpArg(UINT16 Address)
{
	Address &= pCurrentCPU->AddressMask;

	// check mem map
	UINT8 * pr = pCurrentCPU->pMemMap[0x000 | (Address >> 8)];
	if (pr != NULL) {
		return pr[Address & 0xff];
	}
	
	// check handler
	if (pCurrentCPU->ReadByte != NULL) {
		return pCurrentCPU->ReadByte(Address);
	}
	
	return 0;
}

void M6502WriteRom(UINT32 Address, UINT8 Data)
{
	Address &= pCurrentCPU->AddressMask;

#if defined FBNEO_DEBUG
	if (!DebugCPU_M6502Initted) bprintf(PRINT_ERROR, _T("M6502WriteRom called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6502WriteRom called with no CPU open\n"));
#endif

	Address &= 0xffff;

	UINT8 * pr = pCurrentCPU->pMemMap[0x000 | (Address >> 8)];
	UINT8 * pw = pCurrentCPU->pMemMap[0x100 | (Address >> 8)];
	UINT8 * pf = pCurrentCPU->pMemMap[0x200 | (Address >> 8)];

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
	if (pCurrentCPU->WriteByte != NULL) {
		pCurrentCPU->WriteByte(Address, Data);
		return;
	}
}

void M6502SetPC(INT32 pc)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6502Initted) bprintf(PRINT_ERROR, _T("M6502SetPC called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6502SetPC called with no CPU open\n"));
#endif

	m6502_set_pc(pc);
}

UINT32 M6502GetPC(INT32)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6502Initted) bprintf(PRINT_ERROR, _T("M6502GetPC called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6502GetPC called with no CPU open\n"));
#endif

	return m6502_get_pc();
}

UINT32 M6502GetPrevPC(INT32)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6502Initted) bprintf(PRINT_ERROR, _T("M6502GetPrevPC called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6502GetPrevPC called with no CPU open\n"));
#endif

	return m6502_get_prev_pc();
}

INT32 M6502GetFetchStatus()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6502Initted) bprintf(PRINT_ERROR, _T("M6502GetFetchStatus called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6502GetFetchStatus called with no CPU open\n"));
#endif

	return m6502_get_fetch_status();
}

INT32 M6502Scan(INT32 nAction)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6502Initted) bprintf(PRINT_ERROR, _T("M6502Scan called without init\n"));
#endif

	struct BurnArea ba;
	
	if ((nAction & ACB_DRIVER_DATA) == 0) {
		return 1;
	}

	for (INT32 i = 0; i < nM6502Count; i++) {

		M6502Ext *ptr = m6502CPUContext[i];

		char szName[] = "M6502 #n";
		szName[7] = '0' + i;

		ba.Data = &ptr->reg;
		ba.nLen = STRUCT_SIZE_HELPER(m6502_Regs, port);
		ba.szName = szName;
		BurnAcb(&ba);

		SCAN_VAR(ptr->nCyclesTotal);
		SCAN_VAR(ptr->nCyclesStall);
	}
	
	return 0;
}
