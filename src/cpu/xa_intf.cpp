// Philips XA CPU interface
#include "burnint.h"
#include "xa_intf.h"
#include "xa.h"

static UINT8 (*prog_read)(UINT32) = NULL;
static void (*prog_write)(UINT32, UINT8) = NULL;
static UINT8 (*data_read)(UINT32) = NULL;
static void (*data_write)(UINT32, UINT8) = NULL;
static UINT8 (*port_read)(UINT32) = NULL;
static void (*port_write)(UINT32, UINT8) = NULL;

// 24-bit address space, page size 0x100
#define XA_PAGE_SHIFT 8
#define XA_PAGE_COUNT (0x1000000 >> XA_PAGE_SHIFT)

static UINT8 *prog_mem[2][XA_PAGE_COUNT]; // 0 = read/fetch, 1 = write
static UINT8 *data_mem[2][XA_PAGE_COUNT];

static xa_cpu *xacpu = NULL;

UINT8 xa_program_read_byte(UINT32 address)
{
	address &= 0xffffff;

	UINT8 *p = prog_mem[0][address >> XA_PAGE_SHIFT];
	if (p != NULL) return p[address & 0xff];

	if (prog_read) return prog_read(address);

	return 0;
}

void xa_program_write_byte(UINT32 address, UINT8 data)
{
	address &= 0xffffff;

	UINT8 *p = prog_mem[1][address >> XA_PAGE_SHIFT];
	if (p != NULL) { p[address & 0xff] = data; return; }

	if (prog_write) { prog_write(address, data); return; }
}

UINT16 xa_program_read_word(UINT32 address)
{
	return xa_program_read_byte(address) | (xa_program_read_byte(address + 1) << 8);
}

UINT8 xa_data_read_byte(UINT32 address)
{
	address &= 0xffffff;

	UINT8 *p = data_mem[0][address >> XA_PAGE_SHIFT];
	if (p != NULL) return p[address & 0xff];

	if (data_read) return data_read(address);

	return 0;
}

void xa_data_write_byte(UINT32 address, UINT8 data)
{
	address &= 0xffffff;

	UINT8 *p = data_mem[1][address >> XA_PAGE_SHIFT];
	if (p != NULL) { p[address & 0xff] = data; return; }

	if (data_write) { data_write(address, data); return; }
}

UINT16 xa_data_read_word(UINT32 address)
{
	return xa_data_read_byte(address) | (xa_data_read_byte(address + 1) << 8);
}

void xa_data_write_word(UINT32 address, UINT16 data)
{
	xa_data_write_byte(address, data & 0xff);
	xa_data_write_byte(address + 1, (data >> 8) & 0xff);
}

UINT8 xa_port_read(UINT32 port)
{
	if (port_read) return port_read(port);

	return 0xff;
}

void xa_port_write(UINT32 port, UINT8 data)
{
	if (port_write) port_write(port, data);
}

// ----------------------------------------------------------------------

void xaSetProgramReadHandler(UINT8 (*pread)(UINT32))    { prog_read = pread; }
void xaSetProgramWriteHandler(void (*pwrite)(UINT32, UINT8)) { prog_write = pwrite; }
void xaSetDataReadHandler(UINT8 (*pread)(UINT32))        { data_read = pread; }
void xaSetDataWriteHandler(void (*pwrite)(UINT32, UINT8))    { data_write = pwrite; }
void xaSetPortReadHandler(UINT8 (*pread)(UINT32))        { port_read = pread; }
void xaSetPortWriteHandler(void (*pwrite)(UINT32, UINT8))    { port_write = pwrite; }

static void map_pages(UINT8 *table[2][XA_PAGE_COUNT], UINT8 *ptr, UINT32 start, UINT32 end, INT32 flags)
{
	start &= 0xffffff;
	end &= 0xffffff;

	for (UINT32 i = start >> XA_PAGE_SHIFT; i <= (end >> XA_PAGE_SHIFT); i++)
	{
		if (flags & (1 << 0)) table[0][i] = ptr + ((i << XA_PAGE_SHIFT) - start);
		if (flags & (1 << 1)) table[1][i] = ptr + ((i << XA_PAGE_SHIFT) - start);
	}
}

void xaMapMemory(UINT8 *ptr, UINT32 start, UINT32 end, INT32 flags)
{
	map_pages(prog_mem, ptr, start, end, flags);
}

void xaMapDataMemory(UINT8 *ptr, UINT32 start, UINT32 end, INT32 flags)
{
	map_pages(data_mem, ptr, start, end, flags);
}

UINT8 xaCheatRead(UINT32 address)
{
	return xa_program_read_byte(address);
}

void xaWriteROM(UINT32 address, UINT8 data)
{
	address &= 0xffffff;

	UINT8 *p = prog_mem[0][address >> XA_PAGE_SHIFT];
	if (p != NULL) p[address & 0xff] = data;

	p = prog_mem[1][address >> XA_PAGE_SHIFT];
	if (p != NULL) p[address & 0xff] = data;
}

// ----------------------------------------------------------------------

static void core_set_irq(INT32 /*cpu*/, INT32 line, INT32 state)
{
	xaSetIRQLine(line, state);
}

INT32 xaGetActive()
{
	return 0; // only one for now
}

void xaOpen(INT32)
{
	// only one cpu for now
}

void xaClose()
{
}

INT32 xaGetPC(INT32)
{
	return xacpu ? xacpu->GetPC() : 0;
}

INT32 xaInit(INT32, INT32 clock)
{
	memset(prog_mem, 0, sizeof(prog_mem));
	memset(data_mem, 0, sizeof(data_mem));

	prog_read = NULL;
	prog_write = NULL;
	data_read = NULL;
	data_write = NULL;
	port_read = NULL;
	port_write = NULL;

	if (xacpu == NULL) xacpu = new xa_cpu();

	xacpu->Init(clock);

	CpuCheatRegister(0, &xaConfig);

	return 0;
}

void xaExit()
{
	if (xacpu) { delete xacpu; xacpu = NULL; }

	memset(prog_mem, 0, sizeof(prog_mem));
	memset(data_mem, 0, sizeof(data_mem));

	prog_read = NULL;
	prog_write = NULL;
	data_read = NULL;
	data_write = NULL;
	port_read = NULL;
	port_write = NULL;
}

void xaReset()
{
	xacpu->Reset();
}

INT32 xaRun(INT32 cycles)
{
	return xacpu->Run(cycles);
}

void xaRunEnd()
{
	xacpu->RunEnd();
}

void xaNewFrame()
{
	xacpu->NewFrame();
}

INT32 xaTotalCycles()
{
	return xacpu->GetTotalCycles();
}

INT32 xaIdle(INT32 cycles)
{
	return xacpu->Idle(cycles);
}

void xaSetIRQLine(INT32 line, INT32 state)
{
	xacpu->SetIRQLine(line, state);
}

INT32 xaScan(INT32 nAction)
{
	if (nAction & ACB_DRIVER_DATA) {
		xacpu->Scan(nAction);
	}

	return 0;
}

cpu_core_config xaConfig =
{
	"xa",
	xaOpen,
	xaClose,
	xaCheatRead,
	xaWriteROM,
	xaGetActive,
	xaTotalCycles,
	xaNewFrame,
	xaIdle,
	core_set_irq,
	xaRun,
	xaRunEnd,
	xaReset,
	xaScan,
	xaExit,
	0x1000000,
	0
};
