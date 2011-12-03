#include "burnint.h"
#include "konami_intf.h"

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

static UINT8 (*konamiRead)(UINT16 address);
static void (*konamiWrite)(UINT16 address, UINT8 data);
static INT32 (*irqcallback)(INT32);

void konamiMapMemory(UINT8 *src, UINT16 start, UINT16 finish, INT32 type)
{
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
	irqcallback = callback;
}

void konamiSetWriteHandler(void (*write)(UINT16, UINT8))
{
	konamiWrite = write;
}

void konamiSetReadHandler(UINT8 (*read)(UINT16))
{
	konamiRead = read;
}

void konami_write_rom(UINT16 address, UINT8 data)
{
	if (mem[READ][address >> PAGE_SHIFT] != NULL) {
		mem[READ][address >> PAGE_SHIFT][address & PAGE_MASK] = data;
	}

	if (mem[FETCH][address >> PAGE_SHIFT] != NULL) {
		mem[FETCH][address >> PAGE_SHIFT][address & PAGE_MASK] = data;
	}

	if (mem[WRITE][address >> PAGE_SHIFT] != NULL) {
		mem[WRITE][address >> PAGE_SHIFT][address & PAGE_MASK] = data;
	}

	if (konamiWrite != NULL) {
		konamiWrite(address, data);
	}
}

void konami_write(UINT16 address, UINT8 data)
{
	if (mem[WRITE][address >> PAGE_SHIFT] != NULL) {
		mem[WRITE][address >> PAGE_SHIFT][address & PAGE_MASK] = data;
		return;
	}

	if (konamiWrite != NULL) {
		konamiWrite(address, data);
		return;
	}

	return;
}

UINT8 konami_read(UINT16 address)
{
	if (mem[ READ][address >> PAGE_SHIFT] != NULL) {
		return mem[ READ][address >> PAGE_SHIFT][address & PAGE_MASK];
	}

	if (konamiRead != NULL) {
		return konamiRead(address);
	}

	return 0;
}

UINT8 konami_fetch(UINT16 address)
{
	if (mem[FETCH][address >> PAGE_SHIFT] != NULL) {
		return mem[FETCH][address >> PAGE_SHIFT][address & PAGE_MASK];
	}

	if (konamiRead != NULL) {
		return konamiRead(address);
	}

	return 0;
}

void konamiSetIrqLine(INT32 line, INT32 state)
{
	if (state == KONAMI_HOLD_LINE) {
		konami_set_irq_line(line, KONAMI_HOLD_LINE);
		konamiRun(0);
		konami_set_irq_line(line, KONAMI_CLEAR_LINE);
		konamiRun(0);
	} else {
		konami_set_irq_line(line, state);
	}
}

void konamiInit(INT32 num) // only 1 cpu (No examples exist of multi-cpu konami games)
{
	nKonamiCpuCount = 1;
	konami_init(konamiDummyIrqCallback);

	for (INT32 i = 0; i < 3; i++) {
		for (INT32 j = 0; j < (MEMORY_SPACE / PAGE_SIZE); j++) {
			mem[i][j] = NULL;
		}
	}

	CpuCheatRegister(0x0009, num);
}

void konamiExit()
{
	nKonamiCpuCount = 0;
	konamiWrite = NULL;
	konamiRead = NULL;
}

void konamiOpen(INT32 num)
{
	nKonamiCpuActive = num;
}

void konamiClose()
{
	nKonamiCpuActive = -1;
}

INT32 konamiGetActive()
{
	return nKonamiCpuActive;
}
