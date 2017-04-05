#include "burnint.h"
#include "s2650_intf.h"

#define ADDRESS_MAX	0x8000
#define ADDRESS_MASK	0x7fff
#define PAGE		0x0100
#define PAGE_MASK	0x00ff

#define READ		0
#define WRITE		1
#define FETCH		2

INT32 s2650Count;

struct s2650_handler
{
	UINT8 (*s2650Read)(UINT16 address);
	void (*s2650Write)(UINT16 address, UINT8 data);

	UINT8 (*s2650ReadPort)(UINT16 port);
	void (*s2650WritePort)(UINT16 port, UINT8 data);
	
	UINT8 *mem[3][ADDRESS_MAX / PAGE];
};

struct s2650_handler sHandler[MAX_S2650];
struct s2650_handler *sPointer;

s2650irqcallback s2650_irqcallback[MAX_S2650];

extern void s2650_open(INT32 num);
extern void s2650_close();
extern void s2650_init(INT32 num);
extern void s2650_exit();
extern void s2650_reset(void);
extern INT32 s2650_get_pc();

void s2650MapMemory(UINT8 *ptr, INT32 nStart, INT32 nEnd, INT32 nType)
{
#if defined FBA_DEBUG
	if (!DebugCPU_S2650Initted) bprintf(PRINT_ERROR, _T("s2650MapMemory called without init\n"));
	if (nActiveS2650 == -1) bprintf(PRINT_ERROR, _T("s2650MapMemory called when no CPU open\n"));
#endif

	nStart &= ADDRESS_MASK;
	nEnd   &= ADDRESS_MASK;

	for (INT32 i = nStart / PAGE; i < (nEnd / PAGE) + 1; i++)
	{
		if (nType & (1 <<  READ)) sPointer->mem[ READ][i] = ptr + ((i * PAGE) - nStart);
		if (nType & (1 << WRITE)) sPointer->mem[WRITE][i] = ptr + ((i * PAGE) - nStart);
		if (nType & (1 << FETCH)) sPointer->mem[FETCH][i] = ptr + ((i * PAGE) - nStart);
	}
}

void s2650SetWriteHandler(void (*write)(UINT16, UINT8))
{
#if defined FBA_DEBUG
	if (!DebugCPU_S2650Initted) bprintf(PRINT_ERROR, _T("s2650SetWriteHandler called without init\n"));
	if (nActiveS2650 == -1) bprintf(PRINT_ERROR, _T("s2650SetWriteHandler called when no CPU open\n"));
#endif

	sPointer->s2650Write = write;
}

void s2650SetReadHandler(UINT8 (*read)(UINT16))
{
#if defined FBA_DEBUG
	if (!DebugCPU_S2650Initted) bprintf(PRINT_ERROR, _T("s2650SetReadHandler called without init\n"));
	if (nActiveS2650 == -1) bprintf(PRINT_ERROR, _T("s2650SetReadHandler called when no CPU open\n"));
#endif

	sPointer->s2650Read = read;
}

void s2650SetOutHandler(void (*write)(UINT16, UINT8))
{
#if defined FBA_DEBUG
	if (!DebugCPU_S2650Initted) bprintf(PRINT_ERROR, _T("s2650SetOutHandler called without init\n"));
	if (nActiveS2650 == -1) bprintf(PRINT_ERROR, _T("s2650SetOutHandler called when no CPU open\n"));
#endif

	sPointer->s2650WritePort = write;
}

void s2650SetInHandler(UINT8 (*read)(UINT16))
{
#if defined FBA_DEBUG
	if (!DebugCPU_S2650Initted) bprintf(PRINT_ERROR, _T("s2650SetInHandler called without init\n"));
	if (nActiveS2650 == -1) bprintf(PRINT_ERROR, _T("s2650SetInHandler called when no CPU open\n"));
#endif

	sPointer->s2650ReadPort = read;
}

void s2650Write(UINT16 address, UINT8 data)
{
	address &= ADDRESS_MASK;

	if (sPointer->mem[WRITE][address / PAGE] != NULL) {
		sPointer->mem[WRITE][address / PAGE][address & PAGE_MASK] = data;
		return;
	}

	if (sPointer->s2650Write != NULL) {
		sPointer->s2650Write(address, data);
		return;
	}

	return;
}

UINT8 s2650Read(UINT16 address)
{
	address &= ADDRESS_MASK;

	if (sPointer->mem[READ][address / PAGE] != NULL) {
		return sPointer->mem[READ][address / PAGE][address & PAGE_MASK];
	}

	if (sPointer->s2650Read != NULL) {
		return sPointer->s2650Read(address);
	}

	return 0;
}

UINT8 s2650Fetch(UINT16 address)
{
	address &= ADDRESS_MASK;

	if (sPointer->mem[FETCH][address / PAGE] != NULL) {
		return sPointer->mem[FETCH][address / PAGE][address & PAGE_MASK];
	}

	return s2650Read(address);
}

void s2650WritePort(UINT16 port, UINT8 data)
{
	if (sPointer->s2650WritePort != NULL) {
		sPointer->s2650WritePort(port, data);
		return;
	}

	return;
}

UINT8 s2650ReadPort(UINT16 port)
{
	if (sPointer->s2650ReadPort != NULL) {
		return sPointer->s2650ReadPort(port);
	}

	return 0;
}

static void s2650WriteROM(UINT32 address, UINT8 data)
{
#if defined FBA_DEBUG
	if (!DebugCPU_S2650Initted) bprintf(PRINT_ERROR, _T("s2650WriteRom called without init\n"));
	if (nActiveS2650 == -1) bprintf(PRINT_ERROR, _T("s2650WriteRom called when no CPU open\n"));
#endif

	address &= ADDRESS_MASK;

	if (sPointer->mem[READ][address / PAGE] != NULL) {
		sPointer->mem[READ][address / PAGE][address & PAGE_MASK] = data;
	}

	if (sPointer->mem[WRITE][address / PAGE] != NULL) {
		sPointer->mem[WRITE][address / PAGE][address & PAGE_MASK] = data;
	}

	if (sPointer->mem[FETCH][address / PAGE] != NULL) {
		sPointer->mem[FETCH][address / PAGE][address & PAGE_MASK] = data;
	}

	if (sPointer->s2650Write != NULL) {
		sPointer->s2650Write(address, data);
		return;
	}

	return;
}

void s2650SetIrqCallback(INT32 (*irqcallback)(INT32))
{
#if defined FBA_DEBUG
	if (!DebugCPU_S2650Initted) bprintf(PRINT_ERROR, _T("s2650SetIrqCallback called without init\n"));
	if (nActiveS2650 == -1) bprintf(PRINT_ERROR, _T("s2650SetIrqCallback called when no CPU open\n"));
#endif

	s2650_irqcallback[nActiveS2650] = irqcallback;
}

static UINT8 s2650ReadCheat(UINT32 a)
{
	return s2650Read(a);
}

INT32 s2650TotalCycles()
{
	return 0;		// unimplemented
}

void s2650NewFrame()
{
	// unimplemented
}

void s2650RunEnd()
{
	// unimplemented
}

static cpu_core_config s2650CheatCpuConfig =
{
	s2650Open,
	s2650Close,
	s2650ReadCheat,
	s2650WriteROM,
	s2650GetActive,
	s2650TotalCycles,
	s2650NewFrame,
	s2650Run,
	s2650RunEnd,
	s2650Reset,
	ADDRESS_MAX,
	0
};

void s2650Init(INT32 num)
{
	DebugCPU_S2650Initted = 1;
	
	s2650Count = num;
	memset (&sHandler, 0, sizeof (s2650_handler) * (num % MAX_S2650));
	s2650_init(num);

	for (INT32 i = 0; i < num; i++)
		CpuCheatRegister(i, &s2650CheatCpuConfig);
}

void s2650Exit()
{
#if defined FBA_DEBUG
	if (!DebugCPU_S2650Initted) bprintf(PRINT_ERROR, _T("s2650Exit called without init\n"));
#endif

	if (!DebugCPU_S2650Initted) return;

	memset (&sHandler, 0, sizeof (sHandler));
	s2650Count = 0;
	s2650_exit();
	
	DebugCPU_S2650Initted = 0;
}

void s2650Open(INT32 num)
{
#if defined FBA_DEBUG
	if (!DebugCPU_S2650Initted) bprintf(PRINT_ERROR, _T("s2650Open called without init\n"));
	if (num > s2650Count) bprintf(PRINT_ERROR, _T("s2650Open called with invalid index %x\n"), num);
	if (nActiveS2650 != -1) bprintf(PRINT_ERROR, _T("s2650Open called when CPU already open with index %x\n"), num);
#endif

	sPointer = &sHandler[num % MAX_S2650];
	s2650_open(num);
}

void s2650Close()
{
#if defined FBA_DEBUG
	if (!DebugCPU_S2650Initted) bprintf(PRINT_ERROR, _T("s2650Close called without init\n"));
	if (nActiveS2650 == -1) bprintf(PRINT_ERROR, _T("s2650Close called when no CPU open\n"));
#endif

	s2650_close();
}

UINT32 s2650GetPC(INT32)
{
#if defined FBA_DEBUG
	if (!DebugCPU_S2650Initted) bprintf(PRINT_ERROR, _T("s2650GetPC called without init\n"));
	if (nActiveS2650 == -1) bprintf(PRINT_ERROR, _T("s2650GetPC called when no CPU open\n"));
#endif

	return s2650_get_pc();
}

INT32 s2650GetActive()
{
#if defined FBA_DEBUG
	if (!DebugCPU_S2650Initted) bprintf(PRINT_ERROR, _T("s2650GetActive called without init\n"));
	if (nActiveS2650 == -1) bprintf(PRINT_ERROR, _T("s2650GetActive called when no CPU open\n"));
#endif

	return nActiveS2650;
}

void s2650Reset()
{
#if defined FBA_DEBUG
	if (!DebugCPU_S2650Initted) bprintf(PRINT_ERROR, _T("s2650Reset called without init\n"));
	if (nActiveS2650 == -1) bprintf(PRINT_ERROR, _T("s2650Reset called when no CPU open\n"));
#endif

	 s2650_reset();
}
