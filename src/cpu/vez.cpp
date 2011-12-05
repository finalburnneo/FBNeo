// Nec V20/V30/V33/V25/V35 interface
// Written by OopsWare
// http://oopsware.googlepages.com
// Heavily modified by iq_132 (Nov, 2011)

#include "burnint.h"
#include "vez.h"

#define MAX_VEZ		4

//----------------------------------------------------------------------------------
// nec.cpp
void necInit(INT32 cpu, INT32 type);
void necCpuOpen(INT32 cpu);
void necCpuClose();
INT32 nec_reset();
INT32 nec_execute(INT32 cycles);
void nec_set_irq_line_and_vector(INT32 irqline, INT32 vector, INT32 state);
UINT32 nec_total_cycles();
void nec_new_frame();
INT32 necGetPC(INT32 n);
void necScan(INT32 cpu, INT32 nAction);
void necRunEnd();
void necIdle(INT32 cycles);

// v25.cpp
INT32 v25_reset();
void v25_open(INT32 cpu);
void v25_close();
void v25_set_irq_line_and_vector(INT32 irqline, INT32 vector, INT32 state);
INT32 v25_execute(INT32 cycles);
void v25Init(INT32 cpu, INT32 type, INT32 clock);
void v25_set_decode(UINT8 *table);
UINT32 v25_total_cycles();
void v25_new_frame();
INT32 v25GetPC(INT32 n);
void v25Scan(INT32 cpu, INT32 nAction);
void v25RunEnd();
void v25Idle(INT32 cycles);

//----------------------------------------------------------------------------------

struct VezContext {
	void (*cpu_open)(INT32);
	void (*cpu_close)();
	INT32 (*cpu_reset)();
	INT32 (*cpu_execute)(INT32);
	void (*cpu_set_irq_line)(INT32, INT32, INT32);
	void (*decode)(UINT8*);
	UINT32 (*total_cycles)();
	INT32 (*get_pc)(INT32);
	void (*scan)(INT32, INT32);
	void (*runend)();
	void (*idle)(INT32);

	UINT8 * ppMemRead[512];
	UINT8 * ppMemWrite[512];
	UINT8 * ppMemFetch[512];
	UINT8 * ppMemFetchData[512];

	// Handlers
 #ifdef FASTCALL
	UINT8 (__fastcall *ReadHandler)(UINT32 a);
	void (__fastcall *WriteHandler)(UINT32 a, UINT8 d);
	UINT8 (__fastcall *ReadPort)(UINT32 a);
	void (__fastcall *WritePort)(UINT32 a, UINT8 d);
 #else
	UINT8 (__cdecl *ReadHandler)(UINT32 a);
	void (__cdecl *WriteHandler)(UINT32 a, UINT8 d);
	UINT8 (__cdecl *ReadPort)(UINT32 a);
	void (__cdecl *WritePort)(UINT32 a, UINT8 d);
 #endif
};

static struct VezContext *VezCPUContext[MAX_VEZ] = { NULL, NULL, NULL, NULL };
struct VezContext *VezCurrentCPU = 0;

#define VEZ_MEM_SHIFT	11
#define VEZ_MEM_MASK	((1 << VEZ_MEM_SHIFT) - 1)

static INT32 nCPUCount = 0;
static INT32 nOpenedCPU = -1;
INT32 nVezCount;

UINT8 __fastcall VezDummyReadHandler(UINT32) { return 0; }
void __fastcall VezDummyWriteHandler(UINT32, UINT8) { }
UINT8 __fastcall VezDummyReadPort(UINT32) { return 0; }
void __fastcall VezDummyWritePort(UINT32, UINT8) { }

UINT8 cpu_readport(UINT32 p)
{
	p &= 0x100ff; // ?

	return VezCurrentCPU->ReadPort(p);
}

void cpu_writeport(UINT32 p,UINT32 d)
{
	VezCurrentCPU->WritePort(p, d);
}

UINT8 cpu_readmem20(UINT32 a)
{
	a &= 0xFFFFF;
	
	UINT8 * p = VezCurrentCPU->ppMemRead[ a >> VEZ_MEM_SHIFT ];
	if ( p )
		return *(p + a);
	else
		return VezCurrentCPU->ReadHandler(a);
}

UINT8 cpu_readmem20_op(UINT32 a)
{
	a &= 0xFFFFF;
	
	UINT8 * p = VezCurrentCPU->ppMemFetch[ a >> VEZ_MEM_SHIFT ];
	if ( p )
		return *(p + a);
	else
		return VezCurrentCPU->ReadHandler(a);
}

UINT8 cpu_readmem20_arg(UINT32 a)
{
	a &= 0xFFFFF;
	
	UINT8 * p = VezCurrentCPU->ppMemFetchData[ a >> VEZ_MEM_SHIFT ];
	if ( p )
		return *(p + a);
	else
		return VezCurrentCPU->ReadHandler(a);
}

void cpu_writemem20(UINT32 a, UINT8 d)
{
	a &= 0xFFFFF;
	
	UINT8 * p = VezCurrentCPU->ppMemWrite[ a >> VEZ_MEM_SHIFT ];
	if ( p )
		*(p + a) = d;
	else
		VezCurrentCPU->WriteHandler(a, d);
}

void VezSetReadHandler(UINT8 (__fastcall *pHandler)(UINT32))
{
	VezCurrentCPU->ReadHandler = pHandler;
}

void VezSetWriteHandler(void (__fastcall *pHandler)(UINT32, UINT8))
{
	VezCurrentCPU->WriteHandler = pHandler;
}

void VezSetReadPort(UINT8 (__fastcall *pHandler)(UINT32))
{
	VezCurrentCPU->ReadPort = pHandler;
}

void VezSetWritePort(void (__fastcall *pHandler)(UINT32, UINT8))
{
	VezCurrentCPU->WritePort = pHandler;
}

void VezSetDecode(UINT8 *table)
{
	if (VezCurrentCPU->decode) {
		VezCurrentCPU->decode(table);
	}
}

INT32 VezInit(INT32 cpu, INT32 type, INT32 clock)
{
	if (cpu >= MAX_VEZ) {
		bprintf (0, _T("Only %d Vez available! Increase MAX_VEZ in vez.cpp.\n"), MAX_VEZ);
	}

	nOpenedCPU = cpu;

	VezCPUContext[cpu] = (VezContext*)BurnMalloc(sizeof(VezContext));

	VezCurrentCPU = VezCPUContext[cpu];

	memset(VezCurrentCPU, 0, sizeof(struct VezContext));

	switch (type)
	{
		case V20_TYPE:
		case V30_TYPE:
		case V33_TYPE:
		{
			necInit(cpu, type);

			VezCurrentCPU->cpu_open = necCpuOpen;
			VezCurrentCPU->cpu_close = necCpuClose;
			VezCurrentCPU->cpu_reset = nec_reset;
			VezCurrentCPU->cpu_execute = nec_execute;
			VezCurrentCPU->cpu_set_irq_line = nec_set_irq_line_and_vector;
			VezCurrentCPU->decode = NULL; // ?
			VezCurrentCPU->total_cycles = nec_total_cycles;
			VezCurrentCPU->get_pc = necGetPC;
			VezCurrentCPU->scan = necScan;
			VezCurrentCPU->runend = necRunEnd;
			VezCurrentCPU->idle = necIdle;
		}
		break;

		case V25_TYPE:
		case V35_TYPE:
		{
			v25Init(cpu, type&0xff, clock);

			VezCurrentCPU->cpu_open = v25_open;
			VezCurrentCPU->cpu_close = v25_close;
			VezCurrentCPU->cpu_reset = v25_reset;
			VezCurrentCPU->cpu_execute = v25_execute;
			VezCurrentCPU->cpu_set_irq_line = v25_set_irq_line_and_vector;
			VezCurrentCPU->decode = v25_set_decode;
			VezCurrentCPU->total_cycles = v25_total_cycles;
			VezCurrentCPU->get_pc = v25GetPC;
			VezCurrentCPU->scan = v25Scan;
			VezCurrentCPU->runend = v25RunEnd;
			VezCurrentCPU->idle = v25Idle;

		}
		break;
	}
		
	VezCurrentCPU->ReadHandler = VezDummyReadHandler;
	VezCurrentCPU->WriteHandler = VezDummyWriteHandler;
	VezCurrentCPU->ReadPort = VezDummyReadPort;
	VezCurrentCPU->WritePort = VezDummyWritePort;

	INT32 nCount = nVezCount+1;

	nVezCount = nCPUCount = nCount;

	CpuCheatRegister(0x0001, cpu);

	return 0;
}

INT32 VezInit(INT32 cpu, INT32 type)
{
	return VezInit(cpu, type, 0);
}

void VezExit()
{
	for (INT32 i = 0; i < MAX_VEZ; i++) {
		if (VezCPUContext[i]) {
			BurnFree(VezCPUContext[i]);
		}
	}

	nCPUCount = 0;
	nOpenedCPU = -1;
	nVezCount = 0;

	nOpenedCPU = -1;
}

void VezOpen(INT32 nCPU)
{
	if (nCPU >= MAX_VEZ || nCPU < 0) nCPU = 0;

	nOpenedCPU = nCPU;
	VezCurrentCPU = VezCPUContext[nCPU];
	VezCurrentCPU->cpu_open(nCPU);
}

void VezClose()
{
	nOpenedCPU = -1;
	VezCurrentCPU->cpu_close();
	VezCurrentCPU = 0;
}

void VezNewFrame()
{
	// should be separated?
	v25_new_frame();
	nec_new_frame();
}

void VezRunEnd()
{
	VezCurrentCPU->runend();
}

void VezIdle(INT32 cycles)
{
	VezCurrentCPU->idle(cycles);
}

UINT32 VezTotalCycles()
{
	return VezCurrentCPU->total_cycles();
}

INT32 VezGetActive()
{
	return nOpenedCPU;
}

INT32 VezMemCallback(INT32 nStart,INT32 nEnd,INT32 nMode)
{
	nStart >>= VEZ_MEM_SHIFT;
	nEnd += VEZ_MEM_MASK;
	nEnd >>= VEZ_MEM_SHIFT;

	for (INT32 i = nStart; i < nEnd; i++) {
		switch (nMode) {
			case 0:
				VezCurrentCPU->ppMemRead[i] = NULL;
				break;
			case 1:
				VezCurrentCPU->ppMemWrite[i] = NULL;
				break;
			case 2:
				VezCurrentCPU->ppMemFetch[i] = NULL;
				VezCurrentCPU->ppMemFetchData[i] = NULL;
				break;
		}
	}
	return 0;
}

INT32 VezMapArea(INT32 nStart, INT32 nEnd, INT32 nMode, UINT8 *Mem)
{
	INT32 s = nStart >> VEZ_MEM_SHIFT;
	INT32 e = (nEnd + VEZ_MEM_MASK) >> VEZ_MEM_SHIFT;

	for (INT32 i = s; i < e; i++) {
		switch (nMode) {
			case 0:
				VezCurrentCPU->ppMemRead[i] = Mem - nStart;
				break;
			case 1:
				VezCurrentCPU->ppMemWrite[i] = Mem - nStart;
				break;
			case 2:
				VezCurrentCPU->ppMemFetch[i] = Mem - nStart;
				VezCurrentCPU->ppMemFetchData[i] = Mem - nStart;
				break;
		}
	}

	return 0;
}

INT32 VezMapArea(INT32 nStart, INT32 nEnd, INT32 nMode, UINT8 *Mem1, UINT8 *Mem2)
{
	INT32 s = nStart >> VEZ_MEM_SHIFT;
	INT32 e = (nEnd + VEZ_MEM_MASK) >> VEZ_MEM_SHIFT;
	
	if (nMode != 2) return 1;
	
	for (INT32 i = s; i < e; i++) {
		VezCurrentCPU->ppMemFetch[i] = Mem1 - nStart;
		VezCurrentCPU->ppMemFetchData[i] = Mem2 - nStart;
	}

	return 0;
}

INT32 VezReset()
{
	return VezCurrentCPU->cpu_reset();
}

INT32 VezRun(INT32 nCycles)
{
	if (nCycles <= 0) return 0;

	return VezCurrentCPU->cpu_execute(nCycles);
}

INT32 VezPc(INT32 n)
{
	if (n == -1) {
		return VezCurrentCPU->get_pc(-1);
	} else {
		if (n >= MAX_VEZ) return 0;
		struct VezContext *CPU = VezCPUContext[n];
		return CPU->get_pc(n);
	}

	return 0;
}

INT32 VezScan(INT32 nAction)
{
	if ((nAction & ACB_DRIVER_DATA) == 0)
		return 0;

	for (INT32 i = 0; i < nCPUCount; i++) {
		struct VezContext *CPU = VezCPUContext[i];
		if (CPU->scan) {
			CPU->scan(i, nAction);
		}
	}
	
	return 0;
}

void VezSetIRQLineAndVector(const INT32 line, const INT32 vector, const INT32 status)
{
	if (status == VEZ_IRQSTATUS_AUTO)
	{
		VezCurrentCPU->cpu_set_irq_line(line, vector, VEZ_IRQSTATUS_ACK);
		VezCurrentCPU->cpu_execute(100);
		VezCurrentCPU->cpu_set_irq_line(line, vector, VEZ_IRQSTATUS_NONE);
		VezCurrentCPU->cpu_execute(100);
	}
	else
	{
		VezCurrentCPU->cpu_set_irq_line(line, vector, status);
	}
}
