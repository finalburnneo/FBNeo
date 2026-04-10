// ARM9 interface for FBNeo - memory mapping, handler dispatch
// Modeled on arm7_intf.cpp with Arm9 naming for ARM946E-S (IGS036)

#include "burnint.h"
#include "arm9_intf.h"

//#define DEBUG_LOG

#define MAX_MEMORY	0x80000000
#define MAX_MEMORY_AND	(MAX_MEMORY - 1)
#define PAGE_SIZE	0x00001000
#define PAGE_COUNT	(MAX_MEMORY/PAGE_SIZE)
#define PAGE_SHIFT	12
#define PAGE_BYTE_AND	0x00fff
#define PAGE_WORD_AND	0x00ffe
#define PAGE_LONG_AND	0x00ffc

#define READ	0
#define WRITE	1
#define FETCH	2

static UINT8 **membase[3];

static void (*pWriteLongHandler)(UINT32, UINT32) = NULL;
static void (*pWriteWordHandler)(UINT32, UINT16) = NULL;
static void (*pWriteByteHandler)(UINT32, UINT8 ) = NULL;

static UINT16 (*pReadWordHandler)(UINT32) = NULL;
static UINT32 (*pReadLongHandler)(UINT32) = NULL;
static UINT8  (*pReadByteHandler)(UINT32) = NULL;

static UINT32 Arm9IdleLoop = ~0;

extern void arm9_set_irq_line(INT32 irqline, INT32 state);

static void core_set_irq(INT32 /*cpu*/, INT32 irqline, INT32 state)
{
	arm9_set_irq_line(irqline, state);
}

cpu_core_config Arm9Config =
{
	"Arm9",
	Arm9Open,
	Arm9Close,
	Arm9ReadByte,
	Arm9_write_rom_byte,
	Arm9GetActive,
	Arm9TotalCycles,
	Arm9NewFrame,
	Arm9Idle,
	core_set_irq,
	Arm9Run,
	Arm9RunEnd,
	Arm9Reset,
	Arm9Scan,
	Arm9Exit,
	0x80000000,
	0
};

INT32 Arm9GetActive()
{
	return 0;
}

void Arm9Exit()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ARM9Initted) bprintf(PRINT_ERROR, _T("Arm9Exit called without init\n"));
#endif

	for (INT32 i = 0; i < 3; i++) {
		if (membase[i]) {
			free(membase[i]);
			membase[i] = NULL;
		}
	}

	Arm9IdleLoop = ~0;

	DebugCPU_ARM9Initted = 0;
}

void Arm9UnmapMemory(UINT32 start, UINT32 finish, INT32 type)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ARM9Initted) bprintf(PRINT_ERROR, _T("Arm9UnmapMemory called without init\n"));
	if (start >= MAX_MEMORY || finish >= MAX_MEMORY) bprintf(PRINT_ERROR, _T("Arm9UnmapMemory memory range unsupported 0x%8.8x-0x%8.8x\n"), start, finish);
#endif

	UINT32 len = (finish - start) >> PAGE_SHIFT;

	for (UINT32 i = 0; i < len + 1; i++)
	{
		UINT32 offset = i + (start >> PAGE_SHIFT);
		if (type & (1 <<  READ)) membase[ READ][offset] = NULL;
		if (type & (1 << WRITE)) membase[WRITE][offset] = NULL;
		if (type & (1 << FETCH)) membase[FETCH][offset] = NULL;
	}
}

void Arm9MapMemory(UINT8 *src, UINT32 start, UINT32 finish, INT32 type)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ARM9Initted) bprintf(PRINT_ERROR, _T("Arm9MapMemory called without init\n"));
	if (start >= MAX_MEMORY || finish >= MAX_MEMORY) bprintf(PRINT_ERROR, _T("Arm9MapMemory memory range unsupported 0x%8.8x-0x%8.8x\n"), start, finish);
#endif

	UINT32 len = (finish - start) >> PAGE_SHIFT;

	for (UINT32 i = 0; i < len + 1; i++)
	{
		UINT32 offset = i + (start >> PAGE_SHIFT);
		if (type & (1 <<  READ)) membase[ READ][offset] = src + (i << PAGE_SHIFT);
		if (type & (1 << WRITE)) membase[WRITE][offset] = src + (i << PAGE_SHIFT);
		if (type & (1 << FETCH)) membase[FETCH][offset] = src + (i << PAGE_SHIFT);
	}
}

void Arm9SetWriteByteHandler(void (*write)(UINT32, UINT8))
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ARM9Initted) bprintf(PRINT_ERROR, _T("Arm9SetWriteByteHandler called without init\n"));
#endif
	pWriteByteHandler = write;
}

void Arm9SetWriteWordHandler(void (*write)(UINT32, UINT16))
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ARM9Initted) bprintf(PRINT_ERROR, _T("Arm9SetWriteWordHandler called without init\n"));
#endif
	pWriteWordHandler = write;
}

void Arm9SetWriteLongHandler(void (*write)(UINT32, UINT32))
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ARM9Initted) bprintf(PRINT_ERROR, _T("Arm9SetWriteLongHandler called without init\n"));
#endif
	pWriteLongHandler = write;
}

void Arm9SetReadByteHandler(UINT8 (*read)(UINT32))
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ARM9Initted) bprintf(PRINT_ERROR, _T("Arm9SetReadByteHandler called without init\n"));
#endif
	pReadByteHandler = read;
}

void Arm9SetReadWordHandler(UINT16 (*read)(UINT32))
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ARM9Initted) bprintf(PRINT_ERROR, _T("Arm9SetReadWordHandler called without init\n"));
#endif
	pReadWordHandler = read;
}

void Arm9SetReadLongHandler(UINT32 (*read)(UINT32))
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ARM9Initted) bprintf(PRINT_ERROR, _T("Arm9SetReadLongHandler called without init\n"));
#endif
	pReadLongHandler = read;
}

void Arm9WriteByte(UINT32 addr, UINT8 data)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ARM9Initted) bprintf(PRINT_ERROR, _T("Arm9WriteByte called without init\n"));
#endif

	addr &= MAX_MEMORY_AND;

	if (membase[WRITE][addr >> PAGE_SHIFT] != NULL) {
		membase[WRITE][addr >> PAGE_SHIFT][addr & PAGE_BYTE_AND] = data;
		return;
	}

	if (pWriteByteHandler) {
		pWriteByteHandler(addr, data);
	}
}

void Arm9WriteWord(UINT32 addr, UINT16 data)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ARM9Initted) bprintf(PRINT_ERROR, _T("Arm9WriteWord called without init\n"));
#endif

	addr &= MAX_MEMORY_AND;

	if (membase[WRITE][addr >> PAGE_SHIFT] != NULL) {
		*((UINT16*)(membase[WRITE][addr >> PAGE_SHIFT] + (addr & PAGE_WORD_AND))) = BURN_ENDIAN_SWAP_INT16(data);
		return;
	}

	if (pWriteWordHandler) {
		pWriteWordHandler(addr, data);
	}
}

void Arm9WriteLong(UINT32 addr, UINT32 data)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ARM9Initted) bprintf(PRINT_ERROR, _T("Arm9WriteLong called without init\n"));
#endif

	addr &= MAX_MEMORY_AND;

	if (membase[WRITE][addr >> PAGE_SHIFT] != NULL) {
		*((UINT32*)(membase[WRITE][addr >> PAGE_SHIFT] + (addr & PAGE_LONG_AND))) = BURN_ENDIAN_SWAP_INT32(data);
		return;
	}

	if (pWriteLongHandler) {
		pWriteLongHandler(addr, data);
	}
}

UINT8 Arm9ReadByte(UINT32 addr)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ARM9Initted) bprintf(PRINT_ERROR, _T("Arm9ReadByte called without init\n"));
#endif

	addr &= MAX_MEMORY_AND;

	if (membase[READ][addr >> PAGE_SHIFT] != NULL) {
		return membase[READ][addr >> PAGE_SHIFT][addr & PAGE_BYTE_AND];
	}

	if (pReadByteHandler) {
		return pReadByteHandler(addr);
	}

	return 0;
}

UINT16 Arm9ReadWord(UINT32 addr)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ARM9Initted) bprintf(PRINT_ERROR, _T("Arm9ReadWord called without init\n"));
#endif

	addr &= MAX_MEMORY_AND;

	if (membase[READ][addr >> PAGE_SHIFT] != NULL) {
		return BURN_ENDIAN_SWAP_INT16(*((UINT16*)(membase[READ][addr >> PAGE_SHIFT] + (addr & PAGE_WORD_AND))));
	}

	if (pReadWordHandler) {
		return pReadWordHandler(addr);
	}

	return 0;
}

UINT32 Arm9ReadLong(UINT32 addr)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ARM9Initted) bprintf(PRINT_ERROR, _T("Arm9ReadLong called without init\n"));
#endif

	addr &= MAX_MEMORY_AND;

	if (membase[READ][addr >> PAGE_SHIFT] != NULL) {
		return BURN_ENDIAN_SWAP_INT32(*((UINT32*)(membase[READ][addr >> PAGE_SHIFT] + (addr & PAGE_LONG_AND))));
	}

	if (pReadLongHandler) {
		return pReadLongHandler(addr);
	}

	return 0;
}

UINT16 Arm9FetchWord(UINT32 addr)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ARM9Initted) bprintf(PRINT_ERROR, _T("Arm9FetchWord called without init\n"));
#endif

	addr &= MAX_MEMORY_AND;

	if (addr == Arm9IdleLoop) {
		Arm9RunEndEatCycles();
	}

	if (membase[FETCH][addr >> PAGE_SHIFT] != NULL) {
		return BURN_ENDIAN_SWAP_INT16(*((UINT16*)(membase[FETCH][addr >> PAGE_SHIFT] + (addr & PAGE_WORD_AND))));
	}

	if (pReadWordHandler) {
		return pReadWordHandler(addr);
	}

	return 0;
}

UINT32 Arm9FetchLong(UINT32 addr)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ARM9Initted) bprintf(PRINT_ERROR, _T("Arm9FetchLong called without init\n"));
#endif

	addr &= MAX_MEMORY_AND;

	if (addr == Arm9IdleLoop) {
		Arm9RunEndEatCycles();
	}

	if (membase[FETCH][addr >> PAGE_SHIFT] != NULL) {
		return BURN_ENDIAN_SWAP_INT32(*((UINT32*)(membase[FETCH][addr >> PAGE_SHIFT] + (addr & PAGE_LONG_AND))));
	}

	if (pReadLongHandler) {
		return pReadLongHandler(addr);
	}

	return 0;
}

void Arm9SetIRQLine(INT32 line, INT32 state)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ARM9Initted) bprintf(PRINT_ERROR, _T("Arm9SetIRQLine called without init\n"));
#endif

	if (state == CPU_IRQSTATUS_NONE || state == CPU_IRQSTATUS_ACK) {
		arm9_set_irq_line(line, state);
	}
	else if (CPU_IRQSTATUS_AUTO) {
		arm9_set_irq_line(line, CPU_IRQSTATUS_ACK);
		Arm9Run(0);
		arm9_set_irq_line(line, CPU_IRQSTATUS_NONE);
	}
}

void Arm9SetIdleLoopAddress(UINT32 address)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ARM9Initted) bprintf(PRINT_ERROR, _T("Arm9SetIdleLoopAddress called without init\n"));
#endif
	Arm9IdleLoop = address;
}

void Arm9_write_rom_byte(UINT32 addr, UINT8 data)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ARM9Initted) bprintf(PRINT_ERROR, _T("Arm9_write_rom_byte called without init\n"));
#endif
	addr &= MAX_MEMORY_AND;

	if (membase[WRITE][addr >> PAGE_SHIFT] != NULL) {
		membase[WRITE][addr >> PAGE_SHIFT][addr & PAGE_BYTE_AND] = data;
	}

	if (membase[READ][addr >> PAGE_SHIFT] != NULL) {
		membase[READ][addr >> PAGE_SHIFT][addr & PAGE_BYTE_AND] = data;
	}

	if (pWriteByteHandler) {
		pWriteByteHandler(addr, data);
	}
}

void Arm9Init(INT32 nCPU)
{
	DebugCPU_ARM9Initted = 1;

	for (INT32 i = 0; i < 3; i++) {
		membase[i] = (UINT8**)malloc(PAGE_COUNT * sizeof(UINT8*));
		memset(membase[i], 0, PAGE_COUNT * sizeof(UINT8*));
	}

	CpuCheatRegister(nCPU, &Arm9Config);
}
