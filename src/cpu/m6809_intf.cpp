#include "burnint.h"
#include "m6809_intf.h"

#define MAX_CPU		8

INT32 nM6809Count = 0;
static INT32 nActiveCPU = 0;

static M6809Ext *m6809CPUContext;

static INT32 nM6809CyclesDone[MAX_CPU];
INT32 nM6809CyclesTotal;

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
	m6809_reset();
}

void M6809NewFrame()
{
	for (INT32 i = 0; i < nM6809Count; i++) {
		nM6809CyclesDone[i] = 0;
	}
	nM6809CyclesTotal = 0;
}

INT32 M6809Init(INT32 num)
{
	nActiveCPU = -1;
	nM6809Count = num % MAX_CPU;
	
	m6809CPUContext = (M6809Ext*)malloc(num * sizeof(M6809Ext));
	if (m6809CPUContext == NULL) {
		return 1;
	}

	memset(m6809CPUContext, 0, num * sizeof(M6809Ext));
	
	for (INT32 i = 0; i < num; i++) {
		m6809CPUContext[i].ReadByte = M6809ReadByteDummyHandler;
		m6809CPUContext[i].WriteByte = M6809WriteByteDummyHandler;
		m6809CPUContext[i].ReadOp = M6809ReadOpDummyHandler;
		m6809CPUContext[i].ReadOpArg = M6809ReadOpArgDummyHandler;
		
		nM6809CyclesDone[i] = 0;
	
		for (INT32 j = 0; j < (0x0100 * 3); j++) {
			m6809CPUContext[i].pMemMap[j] = NULL;
		}
	}
	
	nM6809CyclesTotal = 0;
	
	m6809_init(NULL);

	for (INT32 i = 0; i < num; i++)
		CpuCheatRegister(0x0005, i);

	return 0;
}

void M6809Exit()
{
	nM6809Count = 0;

	if (m6809CPUContext) {
		free(m6809CPUContext);
		m6809CPUContext = NULL;
	}
}

void M6809Open(INT32 num)
{
	nActiveCPU = num;
	
	m6809_set_context(&m6809CPUContext[nActiveCPU].reg);
	
	nM6809CyclesTotal = nM6809CyclesDone[nActiveCPU];
}

void M6809Close()
{
	m6809_get_context(&m6809CPUContext[nActiveCPU].reg);
	
	nM6809CyclesDone[nActiveCPU] = nM6809CyclesTotal;
	
	nActiveCPU = -1;
}

INT32 M6809GetActive()
{
	return nActiveCPU;
}

void M6809SetIRQ(INT32 vector, INT32 status)
{
	if (status == M6809_IRQSTATUS_NONE) {
		m6809_set_irq_line(vector, 0);
	}
	
	if (status == M6809_IRQSTATUS_ACK) {
		m6809_set_irq_line(vector, 1);
	}
	
	if (status == M6809_IRQSTATUS_AUTO) {
		m6809_set_irq_line(vector, 1);
		m6809_execute(0);
		m6809_set_irq_line(vector, 0);
		m6809_execute(0);
	}
}

INT32 M6809Run(INT32 cycles)
{
	cycles = m6809_execute(cycles);
	
	nM6809CyclesTotal += cycles;
	
	return cycles;
}

void M6809RunEnd()
{

}

INT32 M6809MapMemory(UINT8* pMemory, UINT16 nStart, UINT16 nEnd, INT32 nType)
{
	UINT8 cStart = (nStart >> 8);
	UINT8 **pMemMap = m6809CPUContext[nActiveCPU].pMemMap;

	for (UINT16 i = cStart; i <= (nEnd >> 8); i++) {
		if (nType & M6809_READ)	{
			pMemMap[0     + i] = pMemory + ((i - cStart) << 8);
		}
		if (nType & M6809_WRITE) {
			pMemMap[0x100 + i] = pMemory + ((i - cStart) << 8);
		}
		if (nType & M6809_FETCH) {
			pMemMap[0x200 + i] = pMemory + ((i - cStart) << 8);
		}
	}
	return 0;

}

void M6809SetReadByteHandler(UINT8 (*pHandler)(UINT16))
{
	m6809CPUContext[nActiveCPU].ReadByte = pHandler;
}

void M6809SetWriteByteHandler(void (*pHandler)(UINT16, UINT8))
{
	m6809CPUContext[nActiveCPU].WriteByte = pHandler;
}

void M6809SetReadOpHandler(UINT8 (*pHandler)(UINT16))
{
	m6809CPUContext[nActiveCPU].ReadOp = pHandler;
}

void M6809SetReadOpArgHandler(UINT8 (*pHandler)(UINT16))
{
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

void M6809WriteRom(UINT16 Address, UINT8 Data)
{
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
	struct BurnArea ba;
	
	if ((nAction & ACB_DRIVER_DATA) == 0) {
		return 1;
	}

	for (INT32 i = 0; i < nM6809Count; i++) {

		M6809Ext *ptr = &m6809CPUContext[i];

		INT32 (*Callback)(INT32 irqline);

		Callback = ptr->reg.irq_callback;

		char szName[] = "M6809 #n";
		szName[7] = '0' + i;

		ba.Data = &m6809CPUContext[i].reg;
		ba.nLen = sizeof(m6809CPUContext[i].reg);
		ba.szName = szName;
		BurnAcb(&ba);

		// necessary?
		SCAN_VAR(ptr->nCyclesTotal);
		SCAN_VAR(ptr->nCyclesSegment);
		SCAN_VAR(ptr->nCyclesLeft);

		ptr->reg.irq_callback = Callback;
	}
	
	return 0;
}
