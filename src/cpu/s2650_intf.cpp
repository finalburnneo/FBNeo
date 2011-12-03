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

void s2650MapMemory(UINT8 *ptr, INT32 nStart, INT32 nEnd, INT32 nType)
{
	for (INT32 i = nStart / PAGE; i < (nEnd / PAGE) + 1; i++)
	{
		if (nType & (1 <<  READ)) sPointer->mem[ READ][i] = ptr + ((i * PAGE) - nStart);
		if (nType & (1 << WRITE)) sPointer->mem[WRITE][i] = ptr + ((i * PAGE) - nStart);
		if (nType & (1 << FETCH)) sPointer->mem[FETCH][i] = ptr + ((i * PAGE) - nStart);
	}
}

void s2650SetWriteHandler(void (*write)(UINT16, UINT8))
{
	sPointer->s2650Write = write;
}

void s2650SetReadHandler(UINT8 (*read)(UINT16))
{
	sPointer->s2650Read = read;
}

void s2650SetOutHandler(void (*write)(UINT16, UINT8))
{
	sPointer->s2650WritePort = write;
}

void s2650SetInHandler(UINT8 (*read)(UINT16))
{
	sPointer->s2650ReadPort = read;
}

void s2650_write(UINT16 address, UINT8 data)
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

UINT8 s2650_read(UINT16 address)
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

UINT8 s2650_fetch(UINT16 address)
{
	address &= ADDRESS_MASK;

	if (sPointer->mem[FETCH][address / PAGE] != NULL) {
		return sPointer->mem[FETCH][address / PAGE][address & PAGE_MASK];
	}

	return s2650_read(address);
}

void s2650_write_port(UINT16 port, UINT8 data)
{
	if (sPointer->s2650WritePort != NULL) {
		sPointer->s2650WritePort(port, data);
		return;
	}

	return;
}

UINT8 s2650_read_port(UINT16 port)
{
	if (sPointer->s2650ReadPort != NULL) {
		return sPointer->s2650ReadPort(port);
	}

	return 0;
}

void s2650_write_rom(UINT16 address, UINT8 data)
{
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
	s2650_irqcallback[nActiveS2650] = irqcallback;
}

void s2650Init(INT32 num)
{
	s2650Count = num;
	memset (&sHandler, 0, sizeof (s2650_handler) * (num % MAX_S2650));
	s2650_init(num);

	for (INT32 i = 0; i < num; i++)
		CpuCheatRegister(0x0008, i);
}

void s2650Exit()
{
	memset (&sHandler, 0, sizeof (sHandler));
	s2650Count = 0;
	s2650_exit();
}

void s2650Open(INT32 num)
{
	sPointer = &sHandler[num % MAX_S2650];
	s2650_open(num);
}

void s2650Close()
{
	s2650_close();
}

INT32 s2650GetPc()
{
	return s2650_get_pc();
}

INT32 s2650GetActive()
{
	return nActiveS2650;
}
