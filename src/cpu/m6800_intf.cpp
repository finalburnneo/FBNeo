#include "burnint.h"
#include "m6800_intf.h"
#include <stddef.h>

#define MAX_CPU     8

INT32 nM6800Count = 0; // note: is 0 when 1 cpu is in use. also, 0 when no cpus are in use.
static INT32 nActiveCPU = 0;

static M6800Ext *M6800CPUContext = NULL;

static INT32 nM6800CyclesDone[MAX_CPU];
INT32 nM6800CyclesTotal;

static void m6800_core_set_irq(INT32 cpu, INT32 line, INT32 state)
{
	INT32 active = nActiveCPU;

	if (active != cpu)
	{
		if (active != -1) M6800Close();
		M6800Open(cpu);
	}

	M6800SetIRQLine(line, state);

	if (active != cpu)
	{
		M6800Close();
		if (active != -1) M6800Open(active);
	}
}

cpu_core_config M6800Config =  // M6802, M6808
{
	"M6800",
	M6800Open,
	M6800Close,
	M6800CheatRead,
	M6800WriteRom,
	M6800GetActive,
	M6800TotalCycles,
	M6800NewFrame,
	M6800Idle,
	m6800_core_set_irq,
	M6800Run,		// different
	M6800RunEnd,
	M6800Reset,
	0x10000,
	0
};

static void hd63701_core_set_irq(INT32 cpu, INT32 line, INT32 state)
{
	INT32 active = nActiveCPU;

	if (active != cpu)
	{
		if (active != -1) M6800Close();
		M6800Open(cpu);
	}

	HD63701SetIRQLine(line, state);

	if (active != cpu)
	{
		M6800Close();
		if (active != -1) M6800Open(active);
	}
}

cpu_core_config HD63701Config =
{
	"HD63701",
	M6800Open,
	M6800Close,
	M6800CheatRead,
	M6800WriteRom,
	M6800GetActive,
	M6800TotalCycles,
	M6800NewFrame,
	M6800Idle,
	hd63701_core_set_irq, // different
	HD63701Run,		// different
	M6800RunEnd,
	M6800Reset,
	0x10000,
	0
};

static void m6803_core_set_irq(INT32 cpu, INT32 line, INT32 state)
{
	INT32 active = nActiveCPU;

	if (active != cpu)
	{
		if (active != -1) M6800Close();
		M6800Open(cpu);
	}

	M6803SetIRQLine(line, state);

	if (active != cpu)
	{
		M6800Close();
		if (active != -1) M6800Open(active);
	}
}

cpu_core_config M6803Config =  // M6801, M6803
{
	"M6803",
	M6800Open,
	M6800Close,
	M6800CheatRead,
	M6800WriteRom,
	M6800GetActive,
	M6800TotalCycles,
	M6800NewFrame,
	M6800Idle,
	m6803_core_set_irq,
	M6803Run,		// different
	M6800RunEnd,
	M6800Reset,
	0x10000,
	0
};

static void m6801_core_set_irq(INT32 cpu, INT32 line, INT32 state)
{
	INT32 active = nActiveCPU;

	if (active != cpu)
	{
		if (active != -1) M6800Close();
		M6800Open(cpu);
	}

	M6801SetIRQLine(line, state);

	if (active != cpu)
	{
		M6800Close();
		if (active != -1) M6800Open(active);
	}
}

cpu_core_config M6801Config =
{
	"M6801",
	M6800Open,
	M6800Close,
	M6800CheatRead,
	M6800WriteRom,
	M6800GetActive,
	M6800TotalCycles,
	M6800NewFrame,
	M6800Idle,
	m6801_core_set_irq,
	M6803Run,		// different
	M6800RunEnd,
	M6800Reset,
	0x10000,
	0
};


static void msc8105_core_set_irq(INT32 cpu, INT32 line, INT32 state)
{
	INT32 active = nActiveCPU;

	if (active != cpu)
	{
		if (active != -1) M6800Close();
		M6800Open(cpu);
	}

	NSC8105SetIRQLine(line, state);

	if (active != cpu)
	{
		M6800Close();
		if (active != -1) M6800Open(active);
	}
}

cpu_core_config NSC8105Config =
{
	"NSC8015",
	M6800Open,
	M6800Close,
	M6800CheatRead,
	M6800WriteRom,
	M6800GetActive,
	M6800TotalCycles,
	M6800NewFrame,
	M6800Idle,
	msc8105_core_set_irq,
	NSC8105Run,		// different
	M6800RunEnd,
	M6800Reset,
	0x10000,
	0
};

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
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6800Reset called without init\n"));
#endif

	m6800_reset();
}

void M6800ResetSoft()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6800ResetSoft called without init\n"));
#endif

	m6800_reset_soft();
}

void M6800NewFrame()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6800NewFrame called without init\n"));
#endif

	for (INT32 i = 0; i <= nM6800Count; i++) {
		nM6800CyclesDone[i] = 0;
	}
	nM6800CyclesTotal = 0;
}

UINT8 M6800CheatRead(UINT32 a)
{
	return M6800ReadByte(a);
}

void M6800Open(INT32 num)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6800Open called without init\n"));
	if (num > nM6800Count) bprintf(PRINT_ERROR, _T("M6800Open called with invalid index %x\n"), num);
	if (nActiveCPU != -1) bprintf(PRINT_ERROR, _T("M6800Open called when CPU already open with index %x\n"), num);
#endif
	//bprintf(0, _T("open m680x cpu: %d\n"), num);
	nActiveCPU = num;

	m6800_set_context(&M6800CPUContext[nActiveCPU].reg);
	
	nM6800CyclesTotal = nM6800CyclesDone[nActiveCPU];
}

void M6800Close()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6800Close called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6800Close called when no CPU open\n"));
#endif
	//bprintf(0, _T("close m680x cpu: %d\n"), nActiveCPU);

	m6800_get_context(&M6800CPUContext[nActiveCPU].reg);
	
	nM6800CyclesDone[nActiveCPU] = nM6800CyclesTotal;
	
	nActiveCPU = -1;
}

INT32 M6800GetActive()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6800GetActive called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6800GetActive called when no CPU open\n"));
#endif

	return nActiveCPU;
}

INT32 M6800CoreInit(INT32 num, INT32 type)
{
	DebugCPU_M6800Initted = 1;

	nActiveCPU = -1;
	nM6800Count = num;

	if (M6800CPUContext == NULL) {
		M6800CPUContext = (M6800Ext*)malloc(MAX_CPU * sizeof(M6800Ext));
		if (M6800CPUContext == NULL) {
			return 1;
		}

		memset(M6800CPUContext, 0, MAX_CPU * sizeof(M6800Ext));

		for (INT32 i = 0; i < MAX_CPU; i++) {
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
	}

	nM6800CyclesTotal = 0;
	M6800CPUContext[num].nCpuType = type;

	M6800Open(num);
	if (type == CPU_TYPE_M6800) {
		m6800_init();

		CpuCheatRegister(num, &M6800Config);
	}

	if (type == CPU_TYPE_HD63701) {
		hd63701_init();

		CpuCheatRegister(num, &HD63701Config);
	}

	if (type == CPU_TYPE_M6803) {
		m6803_init();

		CpuCheatRegister(num, &M6803Config);
	}

	if (type == CPU_TYPE_M6801) {
		m6801_init();

		CpuCheatRegister(num, &M6801Config);
	}

	if (type == CPU_TYPE_NSC8105) {
		nsc8105_init();

		CpuCheatRegister(num, &NSC8105Config);
	}

	M6800Close();

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
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6800Exit called without init\n"));
#endif

	nM6800Count = 0;

	if (M6800CPUContext) {
		free(M6800CPUContext);
		M6800CPUContext = NULL;
	}
	
	DebugCPU_M6800Initted = 0;
}

void M6800SetIRQLine(INT32 vector, INT32 status)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6800SetIRQLine called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6800SetIRQLine called when no CPU open\n"));
	if (M6800CPUContext[nActiveCPU].nCpuType != CPU_TYPE_M6800) bprintf(PRINT_ERROR, _T("M6800SetIRQLine called with invalid CPU Type\n"));
#endif

	if (status == CPU_IRQSTATUS_NONE) {
		m6800_set_irq_line(vector, 0);
	}
	
	if (status == CPU_IRQSTATUS_ACK) {
		m6800_set_irq_line(vector, 1);
	}

	if (status == CPU_IRQSTATUS_HOLD) {
		m6800_set_irq_line(vector, 2);
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
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("HD63701SetIRQLine called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("HD63701SetIRQLine called when no CPU open\n"));
	if (M6800CPUContext[nActiveCPU].nCpuType != CPU_TYPE_HD63701) bprintf(PRINT_ERROR, _T("HD63701SetIRQLine called with invalid CPU Type\n"));
#endif

	if (status == CPU_IRQSTATUS_NONE) {
		m6800_set_irq_line(vector, 0);
	}
	
	if (status == CPU_IRQSTATUS_ACK) {
		m6800_set_irq_line(vector, 1);
	}

	if (status == CPU_IRQSTATUS_HOLD) {
		m6800_set_irq_line(vector, 2);
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
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6803SetIRQLine called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6803SetIRQLine called when no CPU open\n"));
	if (M6800CPUContext[nActiveCPU].nCpuType != CPU_TYPE_M6803) bprintf(PRINT_ERROR, _T("M6803SetIRQLine called with invalid CPU Type\n"));
#endif

	if (status == CPU_IRQSTATUS_NONE) {
		m6800_set_irq_line(vector, 0);
	}
	
	if (status == CPU_IRQSTATUS_ACK) {
		m6800_set_irq_line(vector, 1);
	}

	if (status == CPU_IRQSTATUS_HOLD) {
		m6800_set_irq_line(vector, 2);
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
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6801SetIRQLine called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6801SetIRQLine called when no CPU open\n"));
	if (M6800CPUContext[nActiveCPU].nCpuType != CPU_TYPE_M6801) bprintf(PRINT_ERROR, _T("M6800SetIRQLine called with invalid CPU Type\n"));
#endif

	if (status == CPU_IRQSTATUS_NONE) {
		m6800_set_irq_line(vector, 0);
	}
	
	if (status == CPU_IRQSTATUS_ACK) {
		m6800_set_irq_line(vector, 1);
	}

	if (status == CPU_IRQSTATUS_HOLD) {
		m6800_set_irq_line(vector, 2);
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
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("NSC8105SetIRQLine called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("NSC8105SetIRQLine called when no CPU open\n"));
	if (M6800CPUContext[nActiveCPU].nCpuType != CPU_TYPE_M6801) bprintf(PRINT_ERROR, _T("NSC8105SetIRQLine called with invalid CPU Type\n"));
#endif

	if (status == CPU_IRQSTATUS_NONE) {
		m6800_set_irq_line(vector, 0);
	}
	
	if (status == CPU_IRQSTATUS_ACK) {
		m6800_set_irq_line(vector, 1);
	}

	if (status == CPU_IRQSTATUS_HOLD) {
		m6800_set_irq_line(vector, 2);
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
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6800Run called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6800Run called when no CPU open\n"));
	if (M6800CPUContext[nActiveCPU].nCpuType != CPU_TYPE_M6800) bprintf(PRINT_ERROR, _T("M6800Run called with invalid CPU Type\n"));
#endif

	cycles = m6800_execute(cycles);
	
	nM6800CyclesTotal += cycles;
	
	return cycles;
}

INT32 HD63701Run(INT32 cycles)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("HD63701Run called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("HD63701Run called when no CPU open\n"));
	if (M6800CPUContext[nActiveCPU].nCpuType != CPU_TYPE_HD63701) bprintf(PRINT_ERROR, _T("HD63701Run called with invalid CPU Type\n"));
#endif

	cycles = hd63701_execute(cycles);
	
	nM6800CyclesTotal += cycles;
	
	return cycles;
}

INT32 M6803Run(INT32 cycles)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6803Run called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6803Run called when no CPU open\n"));
	if (M6800CPUContext[nActiveCPU].nCpuType != CPU_TYPE_M6803 && M6800CPUContext[nActiveCPU].nCpuType != CPU_TYPE_M6801) bprintf(PRINT_ERROR, _T("M6803Run called with invalid CPU Type\n"));
#endif

	cycles = m6803_execute(cycles);
	
	nM6800CyclesTotal += cycles;
	
	return cycles;
}

INT32 NSC8105Run(INT32 cycles)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("NSC8105Run called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("NSC8105Run called when no CPU open\n"));
	if (M6800CPUContext[nActiveCPU].nCpuType != CPU_TYPE_NSC8105) bprintf(PRINT_ERROR, _T("NSC8105Run called with invalid CPU Type\n"));
#endif

	cycles = nsc8105_execute(cycles);
	
	nM6800CyclesTotal += cycles;
	
	return cycles;
}

UINT32 M6800GetPC(INT32)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6800GetPC called without init\n"));
#endif

	return m6800_get_pc();
}

INT32 M6800MapMemory(UINT8* pMemory, UINT16 nStart, UINT16 nEnd, INT32 nType)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6800MapMemory called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("M6800MapMemory called when no CPU open\n"));
#endif

	UINT8 cStart = (nStart >> 8);
	UINT8 **pMemMap = M6800CPUContext[nActiveCPU].pMemMap;

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
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6800SetReadHandler called without init\n"));
#endif

	M6800CPUContext[nActiveCPU].ReadByte = pHandler;
}

void M6800SetWriteHandler(void (*pHandler)(UINT16, UINT8))
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6800SetWriteHandler called without init\n"));
#endif

	M6800CPUContext[nActiveCPU].WriteByte = pHandler;
}

void M6800SetReadOpHandler(UINT8 (*pHandler)(UINT16))
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6800SetReadOpHandler called without init\n"));
#endif

	M6800CPUContext[nActiveCPU].ReadOp = pHandler;
}

void M6800SetReadOpArgHandler(UINT8 (*pHandler)(UINT16))
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6800SetReadOpArgHandler called without init\n"));
#endif

	M6800CPUContext[nActiveCPU].ReadOpArg = pHandler;
}

void M6800SetReadPortHandler(UINT8 (*pHandler)(UINT16))
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6800SetReadPortHandler called without init\n"));
#endif

	M6800CPUContext[nActiveCPU].ReadPort = pHandler;
}

void M6800SetWritePortHandler(void (*pHandler)(UINT16, UINT8))
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6800SetWritePortHandler called without init\n"));
#endif

	M6800CPUContext[nActiveCPU].WritePort = pHandler;
}

UINT8 M6800ReadByte(UINT16 Address)
{
	// check mem map
	UINT8 * pr = M6800CPUContext[nActiveCPU].pMemMap[0x000 | (Address >> 8)];
	if (pr != NULL) {
		return pr[Address & 0xff];
	}
	
	// check handler
	if (M6800CPUContext[nActiveCPU].ReadByte != NULL) {
		return M6800CPUContext[nActiveCPU].ReadByte(Address);
	}
	
	return 0;
}

void M6800WriteByte(UINT16 Address, UINT8 Data)
{
	// check mem map
	UINT8 * pr = M6800CPUContext[nActiveCPU].pMemMap[0x100 | (Address >> 8)];
	if (pr != NULL) {
		pr[Address & 0xff] = Data;
		return;
	}
	
	// check handler
	if (M6800CPUContext[nActiveCPU].WriteByte != NULL) {
		M6800CPUContext[nActiveCPU].WriteByte(Address, Data);
		return;
	}
}

UINT8 M6800ReadOp(UINT16 Address)
{
	// check mem map
	UINT8 * pr = M6800CPUContext[nActiveCPU].pMemMap[0x200 | (Address >> 8)];
	if (pr != NULL) {
		return pr[Address & 0xff];
	}
	
	// check handler
	if (M6800CPUContext[nActiveCPU].ReadOp != NULL) {
		return M6800CPUContext[nActiveCPU].ReadOp(Address);
	}
	
	return 0;
}

UINT8 M6800ReadOpArg(UINT16 Address)
{
	// check mem map
	UINT8 * pr = M6800CPUContext[nActiveCPU].pMemMap[0x200 | (Address >> 8)];
	if (pr != NULL) {
		return pr[Address & 0xff];
	}
	
	// check handler
	if (M6800CPUContext[nActiveCPU].ReadOpArg != NULL) {
		return M6800CPUContext[nActiveCPU].ReadOpArg(Address);
	}
	
	return 0;
}

UINT8 M6800ReadPort(UINT16 Address)
{
	// check handler
	if (M6800CPUContext[nActiveCPU].ReadPort != NULL) {
		return M6800CPUContext[nActiveCPU].ReadPort(Address);
	}
	
	return 0;
}

void M6800WritePort(UINT16 Address, UINT8 Data)
{
	// check handler
	if (M6800CPUContext[nActiveCPU].WritePort != NULL) {
		M6800CPUContext[nActiveCPU].WritePort(Address, Data);
		return;
	}
}

void M6800WriteRom(UINT32 Address, UINT8 Data)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6800WriteRom called without init\n"));
#endif

	Address &= 0xffff;

	// check mem map
	UINT8 * pr = M6800CPUContext[nActiveCPU].pMemMap[0x000 | (Address >> 8)];
	UINT8 * pw = M6800CPUContext[nActiveCPU].pMemMap[0x100 | (Address >> 8)];
	UINT8 * pf = M6800CPUContext[nActiveCPU].pMemMap[0x200 | (Address >> 8)];

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
	if (M6800CPUContext[nActiveCPU].WriteByte != NULL) {
		M6800CPUContext[nActiveCPU].WriteByte(Address, Data);
		return;
	}
}

INT32 M6800Scan(INT32 nAction)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6800Scan called without init\n"));
#endif

	if (nAction & ACB_DRIVER_DATA) {
		for (INT32 i = 0; i < nM6800Count+1; i++) { // this will only work for 1 cpu (that's all this interface currently supports)
			struct BurnArea ba;

			memset(&ba, 0, sizeof(ba));
			ba.Data	  = &M6800CPUContext[i].reg;
			ba.nLen	  = STRUCT_SIZE_HELPER(m6800_Regs, timer_over);
			ba.szName = "M6800 Registers";
			BurnAcb(&ba);

			SCAN_VAR(M6800CPUContext[i].nCyclesTotal);
			SCAN_VAR(M6800CPUContext[i].nCyclesSegment);
			SCAN_VAR(M6800CPUContext[i].nCyclesLeft);
			SCAN_VAR(nM6800CyclesDone[i]);
		}
		
		SCAN_VAR(nM6800CyclesTotal);
	}

	return 0;
}
