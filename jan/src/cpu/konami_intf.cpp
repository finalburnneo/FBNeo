#include "burnint.h"
#include "konami_intf.h"

#define MAX_CPU		1

#define MEMORY_SPACE	0x10000
#define PAGE_SIZE	0x100
#define PAGE_MASK	0xff
#define PAGE_SHIFT	8
#define PAGE_COUNT	MEMORY_SPACE / PAGE_SIZE

#define READ		0
#define WRITE		1
#define FETCH		2

INT32 nKonamiCpuCount = 0;
static INT32 nKonamiCpuActive = -1;

static UINT8 *mem[3][PAGE_COUNT];

static UINT8 (*pkonamiRead)(UINT16 address);
static void (*pkonamiWrite)(UINT16 address, UINT8 data);

static INT32 (*irqcallback)(INT32);

void konami_set_irq_line(INT32 irqline, INT32 state);
void konami_init(INT32 (*irqcallback)(INT32));
void konami_set_irq_hold(INT32 irq);

void konamiMapMemory(UINT8 *src, UINT16 start, UINT16 finish, INT32 type)
{
#if defined FBA_DEBUG
	if (!DebugCPU_KonamiInitted) bprintf(PRINT_ERROR, _T("konamiMapMemory called without init\n"));
#endif

	UINT16 len = (finish-start) >> PAGE_SHIFT;

	for (UINT16 i = 0; i < len+1; i++)
	{
		UINT32 offset = i + (start >> PAGE_SHIFT);
		if (type & (1 <<  READ)) mem[ READ][offset] = src + (i << PAGE_SHIFT);
		if (type & (1 << WRITE)) mem[WRITE][offset] = src + (i << PAGE_SHIFT);
		if (type & (1 << FETCH)) mem[FETCH][offset] = src + (i << PAGE_SHIFT);
	}
}

INT32 konamiDummyIrqCallback(INT32)
{
	return 0;
}

void konamiSetIrqCallbackHandler(INT32 (*callback)(INT32))
{
#if defined FBA_DEBUG
	if (!DebugCPU_KonamiInitted) bprintf(PRINT_ERROR, _T("konamiSetIrqCallbackHandler called without init\n"));
#endif

	irqcallback = callback;
}

void konamiSetWriteHandler(void (*write)(UINT16, UINT8))
{
#if defined FBA_DEBUG
	if (!DebugCPU_KonamiInitted) bprintf(PRINT_ERROR, _T("konamiSetWriteHandler called without init\n"));
#endif

	pkonamiWrite = write;
}

void konamiSetReadHandler(UINT8 (*read)(UINT16))
{
#if defined FBA_DEBUG
	if (!DebugCPU_KonamiInitted) bprintf(PRINT_ERROR, _T("konamiSetReadHandler called without init\n"));
#endif

	pkonamiRead = read;
}

static void konami_write_rom(UINT32 address, UINT8 data)
{
#if defined FBA_DEBUG
	if (!DebugCPU_KonamiInitted) bprintf(PRINT_ERROR, _T("konami_write_rom called without init\n"));
#endif

	address &= 0xffff;

	if (mem[READ][address >> PAGE_SHIFT] != NULL) {
		mem[READ][address >> PAGE_SHIFT][address & PAGE_MASK] = data;
	}

	if (mem[FETCH][address >> PAGE_SHIFT] != NULL) {
		mem[FETCH][address >> PAGE_SHIFT][address & PAGE_MASK] = data;
	}

	if (mem[WRITE][address >> PAGE_SHIFT] != NULL) {
		mem[WRITE][address >> PAGE_SHIFT][address & PAGE_MASK] = data;
	}

	if (pkonamiWrite != NULL) {
		pkonamiWrite(address, data);
	}
}

void konamiWrite(UINT16 address, UINT8 data)
{
	if (mem[WRITE][address >> PAGE_SHIFT] != NULL) {
		mem[WRITE][address >> PAGE_SHIFT][address & PAGE_MASK] = data;
		return;
	}

	if (pkonamiWrite != NULL) {
		pkonamiWrite(address, data);
		return;
	}

	return;
}

UINT8 konamiRead(UINT16 address)
{
	if (mem[ READ][address >> PAGE_SHIFT] != NULL) {
		return mem[ READ][address >> PAGE_SHIFT][address & PAGE_MASK];
	}

	if (pkonamiRead != NULL) {
		return pkonamiRead(address);
	}

	return 0;
}

UINT8 konamiFetch(UINT16 address)
{
	if (mem[FETCH][address >> PAGE_SHIFT] != NULL) {
		return mem[FETCH][address >> PAGE_SHIFT][address & PAGE_MASK];
	}

	if (pkonamiRead != NULL) {
		return pkonamiRead(address);
	}

	return 0;
}

void konamiSetIrqLine(INT32 line, INT32 state)
{
#if defined FBA_DEBUG
	if (!DebugCPU_KonamiInitted) bprintf(PRINT_ERROR, _T("konamiSetIrqLine called without init\n"));
#endif

	if (state == CPU_IRQSTATUS_HOLD) {
		konami_set_irq_line(line, CPU_IRQSTATUS_ACK);
		konami_set_irq_hold(line);
	} else
	if (state == CPU_IRQSTATUS_AUTO) {
		konami_set_irq_line(line, CPU_IRQSTATUS_ACK);
		konamiRun(0);
		konami_set_irq_line(line, CPU_IRQSTATUS_NONE);
	} else {
		konami_set_irq_line(line, state);
	}
}

void konamiRunEnd()
{
	// nothing atm
}

static UINT8 konami_cheat_read(UINT32 a)
{
	return konamiRead(a);
}

static cpu_core_config konamiCheatCpuConfig =
{
	konamiOpen,
	konamiClose,
	konami_cheat_read,
	konami_write_rom,
	konamiGetActive,
	konamiTotalCycles,
	konamiNewFrame,
	konamiRun,
	konamiRunEnd,
	konamiReset,
	1<<16,
	0
};

#if defined FBA_DEBUG
void konamiInit(INT32 nCpu) // only 1 cpu (No examples exist of multi-cpu konami games)
#else
void konamiInit(INT32 /*nCpu*/) // only 1 cpu (No examples exist of multi-cpu konami games)
#endif
{
	DebugCPU_KonamiInitted = 1;

#if defined FBA_DEBUG
	if (nCpu >= MAX_CPU) bprintf(PRINT_ERROR, _T("konamiInit nCpu is more than MAX_CPU (%d), MAX IS %d\n"), nCpu, MAX_CPU);
#endif

	nKonamiCpuCount = 1;
	konami_init(konamiDummyIrqCallback);

	for (INT32 i = 0; i < 3; i++) {
		for (INT32 j = 0; j < (MEMORY_SPACE / PAGE_SIZE); j++) {
			mem[i][j] = NULL;
		}
	}

	CpuCheatRegister(0, &konamiCheatCpuConfig);
}

void konamiExit()
{
#if defined FBA_DEBUG
	if (!DebugCPU_KonamiInitted) bprintf(PRINT_ERROR, _T("konamiExit called without init\n"));
#endif

	nKonamiCpuCount = 0;
	pkonamiWrite = NULL;
	pkonamiRead = NULL;
	
	DebugCPU_KonamiInitted = 0;
}

void konamiOpen(INT32 num)
{
#if defined FBA_DEBUG
	if (!DebugCPU_KonamiInitted) bprintf(PRINT_ERROR, _T("konamiOpen called without init\n"));
#endif

	nKonamiCpuActive = num;
}

void konamiClose()
{
#if defined FBA_DEBUG
	if (!DebugCPU_KonamiInitted) bprintf(PRINT_ERROR, _T("konamiClose called without init\n"));
#endif

	nKonamiCpuActive = -1;
}

INT32 konamiGetActive()
{
#if defined FBA_DEBUG
	if (!DebugCPU_KonamiInitted) bprintf(PRINT_ERROR, _T("konamiGetActive called without init\n"));
#endif

	return nKonamiCpuActive;
}
