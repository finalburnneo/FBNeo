#include "burnint.h"
#include "tlcs90_intf.h"

static UINT8 (*read)(UINT32) = NULL;
static void (*write)(UINT32, UINT8) = NULL;
static UINT8 (*readio)(UINT16) = NULL;
static void (*writeio)(UINT16, UINT8) = NULL;

static UINT8 *mem[2][0x1000]; // only read/fetch & write

extern void t90_internal_registers_w(UINT16 offset, UINT8 data);
extern UINT8 t90_internal_registers_r(UINT16 offset);
INT32 tlcs90_init(INT32 clock);

UINT8 tlcs90_program_read_byte(UINT32 address)
{
	address &= 0xfffff;

	// internal registers!!!!!!!!!!!!!!!!!
	if (address >= 0xffc0 && address <= 0xffef) {
		return t90_internal_registers_r(address & 0x3f);
	}

	if (mem[0][(address / 0x100)] != NULL) {
		return mem[0][(address / 0x100)][address & 0xff];
	}

	if (read) {
		return read(address);
	}

	return 0;
}

void tlcs90_program_write_byte(UINT32 address, UINT8 data)
{
	address &= 0xfffff;

	// internal registers!!!!!!!!!!!!!!!!!
	if (address >= 0xffc0 && address <= 0xffef) {
		t90_internal_registers_w(address & 0x3f, data);
		return;
	}

	if (mem[1][(address / 0x100)] != NULL) {
		mem[1][(address / 0x100)][address & 0xff] = data;
		return;
	}

	if (write) {
		write(address, data);
		return;
	}
}

UINT8 tlcs90_io_read_byte(UINT16 port)
{
	port &= 0xffff;

//	bprintf (0, _T("Read Port: %4.4x\n"), port);

	if (readio) {
		return readio(port);
	}

	return 0;
}

void tlcs90_io_write_byte(UINT16 port, UINT8 data)
{
	port &= 0xffff;

//	bprintf (0, _T("Write Port: %4.4x %2.2x\n"), port, data);

	if (writeio) {
		return writeio(port, data);
	}
}


void tlcs90SetReadHandler(UINT8 (*pread)(UINT32))
{
	read = pread;
}

void tlcs90SetWriteHandler(void (*pwrite)(UINT32, UINT8))
{
	write = pwrite;
}

void tlcs90SetReadPortHandler(UINT8 (*pread)(UINT16))
{
	readio = pread;
}

void tlcs90SetWritePortHandler(void (*pwrite)(UINT16, UINT8))
{
	writeio = pwrite;
}

void tlcs90MapMemory(UINT8 *ptr, UINT32 start, UINT32 end, INT32 flags)
{
	start &= 0xfffff;
	end &= 0xfffff;

	for (UINT32 i = start / 0x100; i < (end / 0x100) + 1; i++)
	{
		if (flags & (1 << 0)) mem[0][i] = ptr + ((i * 0x100) - start);
		if (flags & (1 << 1)) mem[1][i] = ptr + ((i * 0x100) - start);
	}
}


void tlcs90Open(INT32)
{
	// only one cpu for now
}

INT32 tlcs90Init(INT32, INT32 clock)
{
	memset (mem, 0, 2 * 0x1000 * sizeof(UINT8 *));

	read = NULL;
	write = NULL;
	readio = NULL;
	writeio = NULL;

	return tlcs90_init(clock);
}

void tlcs90Close()
{

}


INT32 tlcs90Exit()
{
	memset (mem, 0, 2 * 0x1000 * sizeof(UINT8 *));
	read = NULL;
	write = NULL;
	readio = NULL;
	writeio = NULL;

	return 0;
}


