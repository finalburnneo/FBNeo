#include "burnint.h"
#include "h6280_intf.h"

#define MEMORY_SPACE	0x200000
#define PAGE_SIZE	0x800
#define PAGE_MASK	0x7ff
#define PAGE_SHIFT	11
#define PAGE_COUNT	MEMORY_SPACE / PAGE_SIZE

#define READ		0
#define WRITE		1
#define FETCH		2

INT32 nh6280CpuCount = 0;
static INT32 nh6280CpuActive = -1;

static UINT8 *mem[3][PAGE_COUNT];

static UINT8 (*h6280Read)(UINT32 address);
static void (*h6280Write)(UINT32 address, UINT8 data);
static void (*h6280WriteIO)(UINT8 port, UINT8 data);
static INT32 (*irqcallback)(INT32);

void h6280MapMemory(UINT8 *src, UINT32 start, UINT32 finish, INT32 type)
{
	UINT32 len = (finish-start) >> PAGE_SHIFT;

	for (UINT32 i = 0; i < len+1; i++)
	{
		UINT32 offset = i + (start >> PAGE_SHIFT);
		if (type & (1 <<  READ)) mem[ READ][offset] = src + (i << PAGE_SHIFT);
		if (type & (1 << WRITE)) mem[WRITE][offset] = src + (i << PAGE_SHIFT);
		if (type & (1 << FETCH)) mem[FETCH][offset] = src + (i << PAGE_SHIFT);
	}
}

INT32 h6280DummyIrqCallback(INT32)
{
	return 0;
}

void h6280SetIrqCallbackHandler(INT32 (*callback)(INT32))
{
	irqcallback = callback;
}

void h6280SetWriteHandler(void (*write)(UINT32, UINT8))
{
	h6280Write = write;
}

void h6280SetWritePortHandler(void (*write)(UINT8, UINT8))
{
	h6280WriteIO = write;
}

void h6280SetReadHandler(UINT8 (*read)(UINT32))
{
	h6280Read = read;
}

void h6280_write_rom(UINT32 address, UINT8 data)
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

	if (h6280Write != NULL) {
		h6280Write(address, data);
	}
}

void h6280_write_port(UINT8 port, UINT8 data)
{
//	bprintf (0, _T("%5.5x write port\n"), port);

	if (h6280WriteIO != NULL) {
		h6280WriteIO(port, data);
		return;
	}

	return;
}

void h6280_write(UINT32 address, UINT8 data)
{
//	bprintf (0, _T("%5.5x write\n"), address);

	if (mem[WRITE][address >> PAGE_SHIFT] != NULL) {
		mem[WRITE][address >> PAGE_SHIFT][address & PAGE_MASK] = data;
		return;
	}

	if (h6280Write != NULL) {
		h6280Write(address, data);
		return;
	}

	return;
}

UINT8 h6280_read(UINT32 address)
{
//	bprintf (0, _T("%5.5x read\n"), address);

	if (mem[ READ][address >> PAGE_SHIFT] != NULL) {
		return mem[ READ][address >> PAGE_SHIFT][address & PAGE_MASK];
	}

	if (h6280Read != NULL) {
		return h6280Read(address);
	}

	return 0;
}

UINT8 h6280_fetch1(UINT32 address)
{
//	address &= 0xffff;

	if (mem[FETCH][address >> PAGE_SHIFT] != NULL) {
		return mem[FETCH][address >> PAGE_SHIFT][address & PAGE_MASK];
	}

	if (h6280Read != NULL) {
		return h6280Read(address);
	}

	return 0;
}

UINT8 h6280_fetch(UINT32 a)
{
//	bprintf (0, _T("%5.5x %5.5x, %2.2x fetch\n"), a, a&0xffff, h6280_fetch1(a));
	return h6280_fetch1(a);
}

void h6280SetIRQLine(INT32 line, INT32 state)
{
	if (state == H6280_IRQSTATUS_AUTO) {
		h6280_set_irq_line(line, 1);
		h6280Run(10);
		h6280_set_irq_line(line, 0);
	} else {
		h6280_set_irq_line(line, state);
	}
}

void h6280Init(INT32 num) // only 1 cpu (No examples exist of multi-cpu h6280 games)
{
	nh6280CpuCount = 1;
//	h6280_init(h6280DummyIrqCallback);

	for (INT32 i = 0; i < 3; i++) {
		for (INT32 j = 0; j < (MEMORY_SPACE / PAGE_SIZE); j++) {
			mem[i][j] = NULL;
		}
	}

	h6280Write = NULL;
	h6280Read = NULL;
	h6280WriteIO = NULL;

	CpuCheatRegister(0x0009, num);
}

void h6280Exit()
{
	nh6280CpuCount = 0;
	h6280Write = NULL;
	h6280Read = NULL;
	h6280WriteIO = NULL;
}

void h6280Open(INT32 num)
{
	nh6280CpuActive = num;
}

void h6280Close()
{
	nh6280CpuActive = -1;
}

INT32 h6280GetActive()
{
	return nh6280CpuActive;
}

