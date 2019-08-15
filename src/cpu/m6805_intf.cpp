#include "burnint.h"
#include "m6805_intf.h"

static INT32 M6805_ADDRESS_MAX;
static INT32 M6805_ADDRESS_MASK;
static INT32 M6805_PAGE;
static INT32 M6805_PAGE_MASK;
static INT32 M6805_PAGE_SHIFT;

#define READ		0
#define WRITE		1
#define FETCH		2

static UINT8 (*m6805ReadFunction)(UINT16 address) = NULL;
static void (*m6805WriteFunction)(UINT16 address, UINT8 data) = NULL;

static UINT8 *mem[3][0x100];

void m6805_core_set_irq(INT32 cpu, INT32 line, INT32 state);

cpu_core_config M6805Config =
{
	"m6805",
	m6805Open,
	m6805Close,
	m6805CheatRead,
	m6805_write_rom,
	m6805GetActive,
	m6805TotalCycles,
	m6805NewFrame,
	m6805Idle,
	m6805_core_set_irq,
	m6805Run,
	m6805RunEnd,
	m6805Reset,
	0x10000,
	0
};

void m6805MapMemory(UINT8 *ptr, INT32 nStart, INT32 nEnd, INT32 nType)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6805Initted) bprintf(PRINT_ERROR, _T("m6805MapMemory called without init\n"));
#endif

	for (INT32 i = nStart / M6805_PAGE; i < (nEnd / M6805_PAGE) + 1; i++)
	{
		if (nType & (1 <<  READ)) mem[ READ][i] = ptr + ((i * M6805_PAGE) - nStart);
		if (nType & (1 << WRITE)) mem[WRITE][i] = ptr + ((i * M6805_PAGE) - nStart);
		if (nType & (1 << FETCH)) mem[FETCH][i] = ptr + ((i * M6805_PAGE) - nStart);
	}
}

void m6805SetWriteHandler(void (*write)(UINT16, UINT8))
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6805Initted) bprintf(PRINT_ERROR, _T("m6805SetWriteHandler called without init\n"));
#endif

	m6805WriteFunction = write;
}

void m6805SetReadHandler(UINT8 (*read)(UINT16))
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6805Initted) bprintf(PRINT_ERROR, _T("m6805SetReadHandler called without init\n"));
#endif

	m6805ReadFunction = read;
}

void m6805Write(UINT16 address, UINT8 data)
{
	address &= M6805_ADDRESS_MASK;

	if (mem[WRITE][address >> M6805_PAGE_SHIFT] != NULL) {
		mem[WRITE][address >> M6805_PAGE_SHIFT][address & M6805_PAGE_MASK] = data;
		return;
	}

	if (m6805WriteFunction != NULL) {
		m6805WriteFunction(address, data);
		return;
	}

	return;
}

UINT8 m6805Read(UINT16 address)
{
	address &= M6805_ADDRESS_MASK;

	if (mem[READ][address >> M6805_PAGE_SHIFT] != NULL) {
		return mem[READ][address >> M6805_PAGE_SHIFT][address & M6805_PAGE_MASK];
	}

	if (m6805ReadFunction != NULL) {
		return m6805ReadFunction(address);
	}

	return 0;
}

UINT8 m6805Fetch(UINT16 address)
{
	address &= M6805_ADDRESS_MASK;

	if (mem[FETCH][address >> M6805_PAGE_SHIFT] != NULL) {
		return mem[FETCH][address >> M6805_PAGE_SHIFT][address & M6805_PAGE_MASK];
	}

	return m6805Read(address);
}

void m6805_write_rom(UINT32 address, UINT8 data)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6805Initted) bprintf(PRINT_ERROR, _T("m6805_write_rom called without init\n"));
#endif

	address &= M6805_ADDRESS_MASK;

	if (mem[READ][address >> M6805_PAGE_SHIFT] != NULL) {
		mem[READ][address >> M6805_PAGE_SHIFT][address & M6805_PAGE_MASK] = data;
	}

	if (mem[WRITE][address >> M6805_PAGE_SHIFT] != NULL) {
		mem[WRITE][address >> M6805_PAGE_SHIFT][address & M6805_PAGE_MASK] = data;
	}

	if (mem[FETCH][address >> M6805_PAGE_SHIFT] != NULL) {
		mem[FETCH][address >> M6805_PAGE_SHIFT][address & M6805_PAGE_MASK] = data;
	}

	if (m6805WriteFunction != NULL) {
		m6805WriteFunction(address, data);
		return;
	}

	return;
}

INT32 m6805GetActive()
{
	return 0;
}

UINT8 m6805CheatRead(UINT32 a)
{
	return m6805Read(a);
}

void m6805Init(INT32 num, INT32 max)
{
	DebugCPU_M6805Initted = 1;
	
	M6805_ADDRESS_MAX  = max;
	M6805_ADDRESS_MASK = M6805_ADDRESS_MAX - 1;
	M6805_PAGE	     = M6805_ADDRESS_MAX / 0x100;
	M6805_PAGE_MASK    = M6805_PAGE - 1;
	M6805_PAGE_SHIFT   = 0;
	for (M6805_PAGE_SHIFT = 0; (1 << M6805_PAGE_SHIFT) < M6805_PAGE; M6805_PAGE_SHIFT++) {}

	memset (mem[0], 0, M6805_PAGE * sizeof(UINT8 *));
	memset (mem[1], 0, M6805_PAGE * sizeof(UINT8 *));
	memset (mem[2], 0, M6805_PAGE * sizeof(UINT8 *));

	for (INT32 i = 0; i < num; i++)
		CpuCheatRegister(i, &M6805Config);
}

void m6805Exit()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6805Initted) bprintf(PRINT_ERROR, _T("m6805Exit called without init\n"));
#endif

	M6805_ADDRESS_MAX	= 0;
	M6805_ADDRESS_MASK	= 0;
	M6805_PAGE		= 0;
	M6805_PAGE_MASK	= 0;
	M6805_PAGE_SHIFT	= 0;
	
	DebugCPU_M6805Initted = 0;
}

void m6805Open(INT32)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6805Initted) bprintf(PRINT_ERROR, _T("m6805Open called without init\n"));
#endif
}

void m6805Close()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6805Initted) bprintf(PRINT_ERROR, _T("m6805Close called without init\n"));
#endif
}
