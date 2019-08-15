#include "burnint.h"
#include "z180_intf.h"

static INT32 DebugCPU_Z180Initted = 0;

#define NUM_CPUS		1

#define PROG_ADDRESS_BITS	20
#define PROG_PAGE_BITS		8
#define PORT_ADDRESS_BITS	16

#define READ			0
#define WRITE			1
#define FETCHOP			2
#define FETCHARG		3

#define PROG_RANGE		(1 << PROG_ADDRESS_BITS)
#define PROG_MASK		(PROG_RANGE - 1)
#define PROG_PAGE_RANGE		(1 << PROG_PAGE_BITS)
#define PROG_PAGE_MASK		(PROG_PAGE_RANGE - 1)
#define PROG_PAGES		(PROG_RANGE >> PROG_PAGE_BITS)
#define PROG_PAGES_MASK		(PROG_PAGES - 1)

#define PORT_RANGE		(1 << PORT_ADDRESS_BITS)
#define PORT_MASK		(PORT_RANGE - 1)

static void core_set_irq(INT32 cpu, INT32 line, INT32 state)
{
	INT32 active = Z180GetActive();

	if (active != cpu)
	{
		if (active != -1) Z180Close();
		Z180Open(cpu);
	}

	Z180SetIRQLine(line, state);

	if (active != cpu)
	{
		Z180Close();
		if (active != -1) Z180Open(active);
	}
}

cpu_core_config Z180Config =
{
	"Z180",
	Z180Open,
	Z180Close,
	z180_cheat_read,
	z180_cheat_write,
	Z180GetActive,
	Z180TotalCycles,
	Z180NewFrame,
	Z180Idle,
	core_set_irq,
	Z180Run,
	Z180RunEnd,
	Z180Reset,
	0x100000,
	0
};

static UINT8 *Mem[NUM_CPUS][4][PROG_PAGES];
static INT32 nActiveCPU = -1;

typedef UINT8 (__fastcall *read_cb)(UINT32 address);
typedef void (__fastcall *write_cb)(UINT32 address, UINT8 data);

static write_cb prog_write[NUM_CPUS];
static read_cb prog_read[NUM_CPUS];
static read_cb prog_fetchop[NUM_CPUS];
static read_cb prog_fetcharg[NUM_CPUS];
static write_cb port_write[NUM_CPUS];
static read_cb port_read[NUM_CPUS];

void Z180SetWriteHandler(void (__fastcall *write)(UINT32, UINT8))
{
	prog_write[nActiveCPU] = write;
}

void Z180SetReadHandler(UINT8 (__fastcall *read)(UINT32))
{
	prog_read[nActiveCPU] = read;
}

void Z180SetFetchOpHandler(UINT8 (__fastcall *fetch)(UINT32))
{
	prog_fetchop[nActiveCPU] = fetch;
}

void Z180SetFetchArgHandler(UINT8 (__fastcall *fetch)(UINT32))
{
	prog_fetcharg[nActiveCPU] = fetch;
}

void Z180SetWritePortHandler(void (__fastcall *write)(UINT32, UINT8))
{
	port_write[nActiveCPU] = write;
}

void Z180SetReadPortHandler(UINT8 (__fastcall *read)(UINT32))
{
	port_read[nActiveCPU] = read;
}

void Z180MapMemory(UINT8 *ptr, UINT32 start, UINT32 end, UINT32 flags)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_Z180Initted) bprintf(PRINT_ERROR, _T("Z180MapMemory called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("Z180MapMemory called when no CPU open\n"));
	if (end <= start || start >= PROG_RANGE || end >= PROG_RANGE || flags == 0)
		bprintf(PRINT_ERROR, _T("Z180MapMemory called when invalid parameters ptr, start: %x, end: %x, flags: %x\n"), start, end, flags);
#endif

	UINT32 s = (start >> PROG_PAGE_BITS);

	for (UINT32 i = 0; i < ((end >> PROG_PAGE_BITS) - (start >> PROG_PAGE_BITS)) + 1; i++)
	{
		if (flags & MAP_READ    ) Mem[nActiveCPU][READ    ][s + i] = (ptr == NULL) ? NULL : (ptr + (i << PROG_PAGE_BITS));
		if (flags & MAP_WRITE   ) Mem[nActiveCPU][WRITE   ][s + i] = (ptr == NULL) ? NULL : (ptr + (i << PROG_PAGE_BITS));
		if (flags & MAP_FETCHOP ) Mem[nActiveCPU][FETCHOP ][s + i] = (ptr == NULL) ? NULL : (ptr + (i << PROG_PAGE_BITS));
		if (flags & MAP_FETCHARG) Mem[nActiveCPU][FETCHARG][s + i] = (ptr == NULL) ? NULL : (ptr + (i << PROG_PAGE_BITS));
	}
}

static INT32 dummy_irq_callback(INT32)
{
	return 0;
}

void Z180Reset()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_Z180Initted) bprintf(PRINT_ERROR, _T("Z180Reset called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("Z180Reset called when no CPU open\n"));
#endif

	z180_reset();
}

void Z180Exit()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_Z180Initted) bprintf(PRINT_ERROR, _T("Z180Exit called without init\n"));
#endif

	z180_exit();
	nActiveCPU = -1;
	DebugCPU_Z180Initted = 0;
}

INT32 Z180GetActive()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_Z180Initted) bprintf(PRINT_ERROR, _T("Z180GetActive called without init\n"));
#endif

	return nActiveCPU;
}

void Z180Open(INT32 nCPU)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_Z180Initted) bprintf(PRINT_ERROR, _T("Z180Open called without init\n"));
	if (nCPU >= NUM_CPUS) bprintf(PRINT_ERROR, _T("Z180Open called with invalid index %x\n"), nCPU);
	if (nActiveCPU != -1) bprintf(PRINT_ERROR, _T("Z180Open called when CPU already open with index %x\n"), nCPU);
#endif

	nActiveCPU = nCPU;
}

void Z180Close()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_Z180Initted) bprintf(PRINT_ERROR, _T("Z180Close called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("Z180Close called when no CPU open\n"));
#endif

	nActiveCPU = -1;
}

INT32 Z180Run(INT32 cycles)
{
	if (cycles <= 0) return 0;

#if defined FBNEO_DEBUG
	if (!DebugCPU_Z180Initted) bprintf(PRINT_ERROR, _T("Z180Run called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("Z180Run called when no CPU open\n"));
//	if (cycles <= 0) bprintf(PRINT_ERROR, _T("Z180Run called with invalid cycles (%d)\n"), cycles);
#endif

	return z180_execute(cycles);
}

INT32 Z180TotalCycles()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_Z180Initted) bprintf(PRINT_ERROR, _T("Z180TotalCycles called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("Z180TotalCycles called when no CPU open\n"));
#endif

	return z180_total_cycles();
}

void Z180RunEnd()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_Z180Initted) bprintf(PRINT_ERROR, _T("Z180RunEnd called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("Z180RunEnd called when no CPU open\n"));
#endif

	z180_run_end();
}

void Z180NewFrame()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_Z180Initted) bprintf(PRINT_ERROR, _T("Z180NewFrame called without init\n"));
#endif
	z180_new_frame();
}

void Z180BurnCycles(INT32 cycles)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_Z180Initted) bprintf(PRINT_ERROR, _T("Z180BurnCycles called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("Z180BurnCycles called when no CPU open\n"));
#endif

	z180_burn(cycles);
}

INT32 Z180Idle(INT32 cycles)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_Z180Initted) bprintf(PRINT_ERROR, _T("Z180Idle called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("Z180Idle called when no CPU open\n"));
#endif

	return z180_idle(cycles);
}

void Z180SetIRQLine(INT32 irqline, INT32 state)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_Z180Initted) bprintf(PRINT_ERROR, _T("Z180SetIRQLine called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("Z180SetIRQLine called when no CPU open\n"));
	if (irqline != 0 && irqline != Z180_INPUT_LINE_NMI) bprintf(PRINT_ERROR, _T("Z180SetIRQLine called with invalid line %d\n"), irqline);
	if (state != CPU_IRQSTATUS_NONE && state != CPU_IRQSTATUS_ACK && state != CPU_IRQSTATUS_AUTO)
		bprintf(PRINT_ERROR, _T("Z180SetIRQLine called with invalid state %d\n"), state);
#endif

	z180_set_irq_line(irqline, state); 
}

void Z180Scan(INT32 nAction)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_Z180Initted) bprintf(PRINT_ERROR, _T("Z180Scan called without init\n"));
#endif
	z180_scan(nAction);
}

void __fastcall z180_cpu_write_handler(UINT32 address, UINT8 data)
{
	address &= PROG_MASK;

	if (Mem[nActiveCPU][WRITE][(address >> PROG_PAGE_BITS)]) {
		Mem[nActiveCPU][WRITE][(address >> PROG_PAGE_BITS)][address & PROG_PAGE_MASK] = data;
		return;
	}

	if (prog_write[nActiveCPU]) {
		prog_write[nActiveCPU](address, data);
		return;
	}
}

UINT8 __fastcall z180_cpu_fetchop_handler(UINT32 address)
{
	address &= PROG_MASK;

	if (Mem[nActiveCPU][FETCHOP][(address >> PROG_PAGE_BITS)]) {
		return Mem[nActiveCPU][FETCHOP][(address >> PROG_PAGE_BITS)][address & PROG_PAGE_MASK];
	}

	if (prog_fetchop[nActiveCPU]) {
		return prog_fetchop[nActiveCPU](address);
	}

	// fall back to arg
	if (Mem[nActiveCPU][FETCHARG][(address >> PROG_PAGE_BITS)]) {
		return Mem[nActiveCPU][FETCHARG][(address >> PROG_PAGE_BITS)][address & PROG_PAGE_MASK];
	}

	if (prog_fetcharg[nActiveCPU]) {
		return prog_fetcharg[nActiveCPU](address);
	}

	// fall back to read
	if (Mem[nActiveCPU][READ ][(address >> PROG_PAGE_BITS)]) {
		return Mem[nActiveCPU][READ ][(address >> PROG_PAGE_BITS)][address & PROG_PAGE_MASK];
	}

	if (prog_read[nActiveCPU]) {
		return prog_read[nActiveCPU](address);
	}

	return 0;
}

UINT8 __fastcall z180_cpu_fetcharg_handler(UINT32 address)
{
	address &= PROG_MASK;

	if (Mem[nActiveCPU][FETCHARG][(address >> PROG_PAGE_BITS)]) {
		return Mem[nActiveCPU][FETCHARG][(address >> PROG_PAGE_BITS)][address & PROG_PAGE_MASK];
	}

	if (prog_fetcharg[nActiveCPU]) {
		return prog_fetcharg[nActiveCPU](address);
	}

	// fall back to op
	if (Mem[nActiveCPU][FETCHOP][(address >> PROG_PAGE_BITS)]) {
		return Mem[nActiveCPU][FETCHOP][(address >> PROG_PAGE_BITS)][address & PROG_PAGE_MASK];
	}

	if (prog_fetchop[nActiveCPU]) {
		return prog_fetchop[nActiveCPU](address);
	}

	// fall back to read
	if (Mem[nActiveCPU][READ ][(address >> PROG_PAGE_BITS)]) {
		return Mem[nActiveCPU][READ ][(address >> PROG_PAGE_BITS)][address & PROG_PAGE_MASK];
	}

	if (prog_read[nActiveCPU]) {
		return prog_read[nActiveCPU](address);
	}

	return 0;
}

UINT8 __fastcall z180_cpu_read_handler(UINT32 address)
{
	address &= PROG_MASK;

	if (Mem[nActiveCPU][READ ][(address >> PROG_PAGE_BITS)]) {
		return Mem[nActiveCPU][READ ][(address >> PROG_PAGE_BITS)][address & PROG_PAGE_MASK];
	}

	if (prog_read[nActiveCPU]) {
		return prog_read[nActiveCPU](address);
	}

	if (Mem[nActiveCPU][FETCHOP][(address >> PROG_PAGE_BITS)]) {
		return Mem[nActiveCPU][FETCHOP][(address >> PROG_PAGE_BITS)][address & PROG_PAGE_MASK];
	}

	if (prog_fetchop[nActiveCPU]) {
		return prog_fetchop[nActiveCPU](address);
	}

	if (Mem[nActiveCPU][FETCHARG][(address >> PROG_PAGE_BITS)]) {
		return Mem[nActiveCPU][FETCHARG][(address >> PROG_PAGE_BITS)][address & PROG_PAGE_MASK];
	}

	if (prog_fetcharg[nActiveCPU]) {
		return prog_fetcharg[nActiveCPU](address);
	}

	return 0;
}

void __fastcall z180_cpu_write_port_handler(UINT32 address, UINT8 data)
{
	address &= PORT_MASK;

	if (port_write[nActiveCPU]) {
		port_write[nActiveCPU](address,data);
		return;
	}
}

UINT8 __fastcall z180_cpu_read_port_handler(UINT32 address)
{
	address &= PORT_MASK;

	if (port_read[nActiveCPU]) {
		return port_read[nActiveCPU](address);
	}

	return 0;
}


UINT8 z180_cheat_read(UINT32 address)
{
	return z180_cpu_read_handler(address);
}

void z180_cheat_write(UINT32 address, UINT8 data)
{
	if (Mem[nActiveCPU][FETCHOP][(address >> PROG_PAGE_BITS)]) {
		Mem[nActiveCPU][FETCHOP][(address >> PROG_PAGE_BITS)][address & PROG_PAGE_MASK] = data;
	}

	if (Mem[nActiveCPU][FETCHARG][(address >> PROG_PAGE_BITS)]) {
		Mem[nActiveCPU][FETCHARG][(address >> PROG_PAGE_BITS)][address & PROG_PAGE_MASK] = data;
	}

	if (Mem[nActiveCPU][READ ][(address >> PROG_PAGE_BITS)]) {
		Mem[nActiveCPU][READ ][(address >> PROG_PAGE_BITS)][address & PROG_PAGE_MASK] = data;
	}

	if (Mem[nActiveCPU][WRITE][(address >> PROG_PAGE_BITS)]) {
		Mem[nActiveCPU][WRITE][(address >> PROG_PAGE_BITS)][address & PROG_PAGE_MASK] = data;
		return;
	}

	if (prog_write[nActiveCPU]) {
		prog_write[nActiveCPU](address, data);
		return;
	}
}

void Z180Init(UINT32 nCPU)
{
	DebugCPU_Z180Initted = 1;

#if defined FBNEO_DEBUG
	if (nCPU >= NUM_CPUS) bprintf(PRINT_ERROR, _T("Z180Init called with invalid nCPU (%d), max is %d\n"), nCPU, NUM_CPUS-1);
	if (nCPU >= NUM_CPUS) nCPU = 0;
#endif

	nActiveCPU = nCPU; // default

	z180_init(nActiveCPU, 0, dummy_irq_callback);

	memset (Mem[nActiveCPU], 0, 4 * PROG_PAGES * sizeof(UINT8*));

	prog_write[nActiveCPU] = NULL;
	prog_read[nActiveCPU] = NULL;
	prog_fetchop[nActiveCPU] = NULL;
	prog_fetcharg[nActiveCPU] = NULL;
	port_write[nActiveCPU] = NULL;
	port_read[nActiveCPU] = NULL;

	CpuCheatRegister(nActiveCPU, &Z180Config);

	nActiveCPU = -1; // default
}
