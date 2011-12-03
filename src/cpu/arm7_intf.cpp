#include "burnint.h"
#include "arm7_intf.h"

//#define DEBUG_LOG

#define MAX_MEMORY	0x80000000 // more than good enough for pgm
#define PAGE_SIZE	0x00001000 // 400 would be better...
#define PAGE_COUNT	(MAX_MEMORY/PAGE_SIZE)
#define PAGE_SHIFT	12	// 0x1000 -> 12 bits
#define PAGE_BYTE_AND	0x00fff	// 0x1000 - 1 (byte align)
#define PAGE_WORD_AND	0x00ffe	// 0x1000 - 2 (word align)
#define PAGE_LONG_AND	0x00ffc // 0x1000 - 4 (ignore last 4 bytes, long align)

#define READ	0
#define WRITE	1
#define FETCH	2

static UINT8 **membase[3]; // 0 read, 1, write, 2 opcode

static void (*pWriteLongHandler)(UINT32, UINT32) = NULL;
static void (*pWriteWordHandler)(UINT32, UINT16) = NULL;
static void (*pWriteByteHandler)(UINT32, UINT8 ) = NULL;

static UINT16 (*pReadWordHandler)(UINT32) = NULL;
static UINT32 (*pReadLongHandler)(UINT32) = NULL;
static UINT8  (*pReadByteHandler)(UINT32) = NULL;

static UINT32 Arm7IdleLoop = ~0;

void Arm7Init( INT32 num ) // only one cpu supported
{
	for (INT32 i = 0; i < 3; i++) {
		membase[i] = (UINT8**)malloc(PAGE_COUNT * sizeof(UINT8*));
	}

	CpuCheatRegister(0x000a, num);
}

void Arm7Exit() // only one cpu supported
{
	for (INT32 i = 0; i < 3; i++) {
		if (membase[i]) {
			free (membase[i]);
			membase[i] = NULL;
		}
	}

	Arm7IdleLoop = ~0;
}

void Arm7MapMemory(UINT8 *src, INT32 start, INT32 finish, INT32 type)
{
	UINT32 len = (finish-start) >> PAGE_SHIFT;

	for (UINT32 i = 0; i < len+1; i++)
	{
		UINT32 offset = i + (start >> PAGE_SHIFT);
		if (type & (1 <<  READ)) membase[ READ][offset] = src + (i << PAGE_SHIFT);
		if (type & (1 << WRITE)) membase[WRITE][offset] = src + (i << PAGE_SHIFT);
		if (type & (1 << FETCH)) membase[FETCH][offset] = src + (i << PAGE_SHIFT);
	}
}

void Arm7SetWriteByteHandler(void (*write)(UINT32, UINT8))
{
	pWriteByteHandler = write;
}

void Arm7SetWriteWordHandler(void (*write)(UINT32, UINT16))
{
	pWriteWordHandler = write;
}

void Arm7SetWriteLongHandler(void (*write)(UINT32, UINT32))
{
	pWriteLongHandler = write;
}

void Arm7SetReadByteHandler(UINT8 (*read)(UINT32))
{
	pReadByteHandler = read;
}

void Arm7SetReadWordHandler(UINT16 (*read)(UINT32))
{
	pReadWordHandler = read;
}

void Arm7SetReadLongHandler(UINT32 (*read)(UINT32))
{
	pReadLongHandler = read;
}

void Arm7_program_write_byte_32le(UINT32 addr, UINT8 data)
{
#ifdef DEBUG_LOG
	bprintf (PRINT_NORMAL, _T("%5.5x, %2.2x wb\n"), addr, data);
#endif

	if (membase[WRITE][addr >> PAGE_SHIFT] != NULL) {
		membase[WRITE][addr >> PAGE_SHIFT][addr & PAGE_BYTE_AND] = data;
		return;
	}

	if (pWriteByteHandler) {
		pWriteByteHandler(addr, data);
	}
}

void Arm7_program_write_word_32le(UINT32 addr, UINT16 data)
{
#ifdef DEBUG_LOG
	bprintf (PRINT_NORMAL, _T("%5.5x, %8.8x wd\n"), addr, data);
#endif

	if (membase[WRITE][addr >> PAGE_SHIFT] != NULL) {
		*((UINT16*)(membase[WRITE][addr >> PAGE_SHIFT] + (addr & PAGE_WORD_AND))) = data;
		return;
	}

	if (pWriteWordHandler) {
		pWriteWordHandler(addr, data);
	}
}

void Arm7_program_write_dword_32le(UINT32 addr, UINT32 data)
{
#ifdef DEBUG_LOG
	bprintf (PRINT_NORMAL, _T("%5.5x, %8.8x wd\n"), addr, data);
#endif

	if (membase[WRITE][addr >> PAGE_SHIFT] != NULL) {
		*((UINT32*)(membase[WRITE][addr >> PAGE_SHIFT] + (addr & PAGE_LONG_AND))) = data;
		return;
	}

	if (pWriteLongHandler) {
		pWriteLongHandler(addr, data);
	}
}


UINT8 Arm7_program_read_byte_32le(UINT32 addr)
{
#ifdef DEBUG_LOG
	bprintf (PRINT_NORMAL, _T("%5.5x, rb\n"), addr);
#endif

	if (membase[ READ][addr >> PAGE_SHIFT] != NULL) {
		return membase[READ][addr >> PAGE_SHIFT][addr & PAGE_BYTE_AND];
	}

	if (pReadByteHandler) {
		return pReadByteHandler(addr);
	}

	return 0;
}

UINT16 Arm7_program_read_word_32le(UINT32 addr)
{
#ifdef DEBUG_LOG
	bprintf (PRINT_NORMAL, _T("%5.5x, rl\n"), addr);
#endif

	if (membase[ READ][addr >> PAGE_SHIFT] != NULL) {
		return *((UINT16*)(membase[ READ][addr >> PAGE_SHIFT] + (addr & PAGE_WORD_AND)));
	}

	if (pReadWordHandler) {
		return pReadWordHandler(addr);
	}

	return 0;
}

UINT32 Arm7_program_read_dword_32le(UINT32 addr)
{
#ifdef DEBUG_LOG
	bprintf (PRINT_NORMAL, _T("%5.5x, rl\n"), addr);
#endif

	if (membase[ READ][addr >> PAGE_SHIFT] != NULL) {
		return *((UINT32*)(membase[ READ][addr >> PAGE_SHIFT] + (addr & PAGE_LONG_AND)));
	}

	if (pReadLongHandler) {
		return pReadLongHandler(addr);
	}

	return 0;
}

UINT16 Arm7_program_opcode_word_32le(UINT32 addr)
{
#ifdef DEBUG_LOG
	bprintf (PRINT_NORMAL, _T("%5.5x, rwo\n"), addr);
#endif

	// speed hack -- skip idle loop...
	if (addr == Arm7IdleLoop) {
		Arm7RunEnd();
	}

	if (membase[FETCH][addr >> PAGE_SHIFT] != NULL) {
		return *((UINT16*)(membase[FETCH][addr >> PAGE_SHIFT] + (addr & PAGE_WORD_AND)));
	}

	// good enough for now...
	if (pReadWordHandler) {
		return pReadWordHandler(addr);
	}

	return 0;
}

UINT32 Arm7_program_opcode_dword_32le(UINT32 addr)
{
#ifdef DEBUG_LOG
	bprintf (PRINT_NORMAL, _T("%5.5x, rlo\n"), addr);
#endif

	// speed hack - skip idle loop...
	if (addr == Arm7IdleLoop) {
		Arm7RunEnd();
	}

	if (membase[FETCH][addr >> PAGE_SHIFT] != NULL) {
		return *((UINT32*)(membase[FETCH][addr >> PAGE_SHIFT] + (addr & PAGE_LONG_AND)));
	}

	// good enough for now...
	if (pReadLongHandler) {
		return pReadLongHandler(addr);
	}

	return 0;
}

void Arm7SetIRQLine(INT32 line, INT32 state)
{
	if (state == ARM7_CLEAR_LINE || state == ARM7_ASSERT_LINE) {
		arm7_set_irq_line(line, state);
	}
	else if (ARM7_HOLD_LINE) {
		arm7_set_irq_line(line, ARM7_ASSERT_LINE);
		Arm7Run(0);
		arm7_set_irq_line(line, ARM7_CLEAR_LINE);
	}
}

// Set address of idle loop start - speed hack
void Arm7SetIdleLoopAddress(UINT32 address)
{
	Arm7IdleLoop = address;
}


// For cheats/etc

void Arm7_write_rom_byte(UINT32 addr, UINT8 data)
{
	// write to rom & ram
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
