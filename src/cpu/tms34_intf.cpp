// TMS34 - cpu interface for MAME 0.127 TMS340x0 cpu core
#include "burnint.h"
#include "tms34/tms34010.h"
#include "tms34_intf.h"

#define MAX_CPUS    4

#define ADDR_BITS   32
#define PAGE_SIZE   0x1000
#define PAGE_SIZE_8 (0x1000 >> 3)
#define PAGE_SHIFT  12
#define PAGE_MASK   0xFFF
#define PAGE_COUNT  (1 << (ADDR_BITS - PAGE_SHIFT))
#define PAGE_WADD    (PAGE_COUNT)
#define MAXHANDLER  32
#define PFN(x)  (((x) >> PAGE_SHIFT) & 0xFFFFF)

template<typename T>
inline T tms_fast_read(UINT8 *ptr, UINT32 adr) {
    return *((T*)  ((UINT8*) ptr + TOBYTE(adr & PAGE_MASK)));
}

template<typename T>
inline void tms_fast_write(UINT8 *xptr, UINT32 adr, T value) {
    T *ptr = ((T*)  ((UINT8*) xptr + TOBYTE(adr & PAGE_MASK)));
    *ptr = value;
}

struct TMS34010MemoryMap
{
	INT32 CPU_TYPE;

    UINT8 *map[PAGE_COUNT * 2];
	UINT8 *context;

    pTMS34010ReadHandler read[MAXHANDLER];
    pTMS34010WriteHandler write[MAXHANDLER];

	pTMS34010ScanlineRender scanlineRenderCallback;
};

static TMS34010MemoryMap MapStore[MAX_CPUS];
static TMS34010MemoryMap *g_mmap = NULL;
static INT32 active_cpu = -1;
static INT32 total_cpus = 0;
static INT32 context_size = 0;

static UINT16 default_read(UINT32 address) { return 0; }
static void default_write(UINT32 address, UINT16 value) {}
static void default_shift_op(UINT32,UINT16*){}


static UINT16 IO_read(UINT32 address) { return tms34010_io_register_r(address); }
static void IO_write(UINT32 address, UINT16 value) { tms34010_io_register_w(address, value); }
static UINT16 IO_read020(UINT32 address) { return tms34020_io_register_r(address); }
static void IO_write020(UINT32 address, UINT16 value) { tms34020_io_register_w(address, value); }

static void TMS34010MapReset();

// cheat-engine hook-up
void TMS34010Open(INT32 nCpu)
{
	if (active_cpu != -1)	{
		bprintf(PRINT_ERROR, _T("TMS34010Open(%d); when cpu already open.\n"), nCpu);
	}

	g_mmap = &MapStore[nCpu];
	active_cpu = nCpu;

	if (g_mmap->context) {
		tms34010_set_context(g_mmap->context);
	}
}

void TMS34010Close()
{
	if (active_cpu == -1)	{
		bprintf(PRINT_ERROR, _T("TMS34010Close() called with no cpu open!\n"));
	}

	if (g_mmap->context) {
		tms34010_get_context(g_mmap->context);
	}

	g_mmap = NULL;
	active_cpu = -1;
}

INT32 TMS34010GetActive()
{
	return active_cpu;
}

void TMS34010WriteCheat(UINT32 address, UINT8 value);
UINT8 TMS34010ReadByte(UINT32 address);

INT32 TMS34010TotalCyclesi32()
{
	return TMS34010TotalCycles();
}

void TMS34010SetIRQLine(INT32 cpu, INT32 vector, INT32 status)
{
	tms34010_set_irq_line(vector, status);
}

cpu_core_config TMS34010Config =
{
	"TMS34010",
	TMS34010Open,
	TMS34010Close,
	TMS34010ReadByte,
	TMS34010WriteCheat,
	TMS34010GetActive,
	TMS34010TotalCyclesi32,
	TMS34010NewFrame,
	TMS34010Idle,
	TMS34010SetIRQLine,
	TMS34010Run,
	TMS34010RunEnd,
	TMS34010Reset,
	TMS34010Scan,
	TMS34010Exit,
	0x100000000ULL,
	MB_CHEAT_ENDI_SWAP | 16 // LE (16bit databus) but needs address swap when writing multibyte cheats
};
// end cheat-engine hook-up

static void TMS34010Init_Internal(INT32 nCpu, INT32 nType)
{
	if (nCpu >= MAX_CPUS) {
		bprintf(PRINT_ERROR, _T("TMS340%dInit(%d); cpu number too high, increase MAX_CPUS.\n"), nType, nCpu);
	}

	if (nType != 10 && nType != 20) {
		bprintf(PRINT_ERROR, _T("TMS34010Init_Internal: Bad cpu nType specified.\n"));
	}

	if (nCpu == 0) {
		memset(&MapStore, 0, sizeof(MapStore));
	}

	total_cpus = nCpu + 1;

	TMS34010Open(nCpu);

	g_mmap->CPU_TYPE = nType;

	context_size = tms34010_context_size();
	g_mmap->context = (UINT8*)BurnMalloc(context_size);

	tms34010_init();
	TMS34010SetToShift(default_shift_op);
	TMS34010SetFromShift(default_shift_op);

	// map IO registers
	TMS34010MapReset();
	switch (nType) {
		case 10:
			TMS34010SetHandlers(MAXHANDLER-1, IO_read, IO_write);
			TMS34010MapHandler(MAXHANDLER-1, 0xc0000000, 0xc00001ff, MAP_READ | MAP_WRITE);
			break;
		case 20:
			TMS34010SetHandlers(MAXHANDLER-1, IO_read020, IO_write020);
			TMS34010MapHandler(MAXHANDLER-1, 0xc0000000, 0xc00003ff, MAP_READ | MAP_WRITE);
			break;
	}

	TMS34010Close();

	CpuCheatRegister(nCpu, &TMS34010Config);
}

void TMS34010Init(INT32 nCpu)
{
	TMS34010Init_Internal(nCpu, 10);
}

void TMS34020Init(INT32 nCpu)
{
	TMS34010Init_Internal(nCpu, 20);
}

void TMS34010Exit()
{
	for (INT32 i = 0; i < total_cpus; i++) {
		TMS34010Open(i);
		tms34010_exit();
		BurnFree(g_mmap->context);
		TMS34010Close();
	}

	total_cpus = 0;
	active_cpu = -1;
}

void TMS34010SetCpuCyclesPerFrame(INT32 cpf)
{
	tms34010_set_cycperframe(cpf);
}

void TMS34010SetPixClock(INT32 pxlclock, INT32 pix_per_clock)
{
	tms34010_set_pixclock(pxlclock, pix_per_clock);
}

void TMS34010SetOutputINT(void (*output_int_func)(INT32))
{
	tms34010_set_output_int(output_int_func);
}

void TMS34010SetHaltOnReset(INT32 onoff)
{
	tms34010_set_halt_on_reset(onoff);
}

INT32 TMS34010Run(INT32 cycles)
{
    return tms34010_run(cycles);
}

INT32 TMS34010Idle(INT32 cycles)
{
    return tms34010_idle(cycles);
}

void TMS34010TimerSetCB(void (*timer_cb)())
{
	tms34010_timer_set_cb(timer_cb);
}

void TMS34010TimerSet(INT32 cycles)
{
	tms34010_timer_arm(cycles);
}

INT64 TMS34010TotalCycles()
{
	return tms34010_total_cycles();
}

void TMS34010NewFrame()
{
	for (INT32 i = 0; i < total_cpus; i++) {
		TMS34010Open(i);
		tms34010_new_frame();
		TMS34010Close();
	}
}

void TMS34010RunEnd()
{
	tms34010_stop();
}

INT32 TMS34010Scan(INT32 nAction)
{
	for (INT32 i = 0; i < total_cpus; i++) {
		TMS34010Open(i);
		tms34010_scan(nAction);
		TMS34010Close();
	}

	return 0;
}

UINT32 TMS34010GetPC()
{
	return tms34010_get_pc();
}

UINT32 TMS34010GetPPC()
{
	bprintf(0, _T("TMS34010GetPPC() not supported.\n"));
	//return tms34010_get_ppc();
	return 0;
}

void TMS34010Reset()
{
	switch (g_mmap->CPU_TYPE) {
		case 10: tms34010_reset(); break;
		case 20: tms34020_reset(); break;
	}
}

void TMS34010GenerateIRQ(UINT32 line)
{
	tms34010_set_irq_line(line, 1);
}

void TMS34010ClearIRQ(UINT32 line)
{
	tms34010_set_irq_line(line, 0);
}

void TMS34010SetScanlineRender(pTMS34010ScanlineRender sr)
{
    g_mmap->scanlineRenderCallback = sr;
}

void TMS34010SetToShift(void (*reader)(UINT32 addr, UINT16 *dst))
{
    tms34010_set_toshift(reader);
}

void TMS34010SetFromShift(void (*writer)(UINT32 addr, UINT16 *src))
{
    tms34010_set_fromshift(writer);
}

INT32 TMS34010GenerateScanline(INT32 line)
{
	tms34010_generate_scanline(line, g_mmap->scanlineRenderCallback);
	return 0;
}

void TMS34010HostWrite(INT32 reg, UINT16 data)
{
	tms34010_host_w(reg, data);
}

UINT16 TMS34010HostRead(INT32 reg)
{
	return tms34010_host_r(reg);
}

UINT8 TMS34010ReadByte(UINT32 address)
{
	address <<= 3;
    UINT8 *pr = g_mmap->map[PFN(address)];
    if ((uintptr_t)pr >= MAXHANDLER) {
        // address is bit-address
        return tms_fast_read<UINT8>(pr,address);
    } else {
        return g_mmap->read[(uintptr_t)pr](address);
    }
}

UINT16 TMS34010ReadWord(UINT32 address)
{
	address <<= 3;
    UINT8 *pr = g_mmap->map[PFN(address)];
    if ((uintptr_t)pr >= MAXHANDLER) {
        // address is bit-address
        return BURN_ENDIAN_SWAP_INT16(tms_fast_read<UINT16>(pr,address));
    } else {
        return g_mmap->read[(uintptr_t)pr](address);
	}
}

void TMS34010WriteByte(UINT32 address, UINT8 value)
{
	address <<= 3;
    UINT8 *pr = g_mmap->map[PAGE_WADD + PFN(address)];
    if ((uintptr_t)pr >= MAXHANDLER) {
        // address is bit-address
        return tms_fast_write<UINT8>(pr,address, value);
    } else {
        return g_mmap->write[(uintptr_t)pr](address, value);
    }
}

void TMS34010WriteWord(UINT32 address, UINT16 value)
{
	address <<= 3;
    UINT8 *pr = g_mmap->map[PAGE_WADD + PFN(address)];
    if ((uintptr_t)pr >= MAXHANDLER) {
        // address is bit-address
        return tms_fast_write<UINT16>(pr,address, BURN_ENDIAN_SWAP_INT16(value));
    } else {
        return g_mmap->write[(uintptr_t)pr](address, value);
    }
}

void TMS34010WriteCheat(UINT32 address, UINT8 value) // for cheat-engine
{
	address <<= 3; // cheat.dat format is not in bit-address. *kludge*

    UINT8 *pr = g_mmap->map[PAGE_WADD + PFN(address)];
    if ((uintptr_t)pr >= MAXHANDLER) {
        // address is bit-address
		tms_fast_write<UINT8>(pr,address, value);
    } else {
		g_mmap->write[(uintptr_t)pr](address, value);
	}

	pr = g_mmap->map[PFN(address)];
    if ((uintptr_t)pr >= MAXHANDLER) {
        // address is bit-address
		tms_fast_write<UINT8>(pr,address, value);
	}
}

static void TMS34010MapReset()
{
    for (int page = 0; page < PAGE_COUNT; page++) {
        g_mmap->map[page] = NULL;
        g_mmap->map[page + PAGE_WADD] = NULL;
    }
    for (int handler = 0; handler < MAXHANDLER; handler++) {
        g_mmap->read[handler] = default_read;
        g_mmap->write[handler] = default_write;
    }
}

void TMS34010MapMemory(UINT8 *mem, UINT32 start, UINT32 end, UINT8 type)
{
    const int max_pages = (PFN(end) - PFN(start)) + 1;

    int page = PFN(start);
    for (int i = 0; i < max_pages; i++, page++) {

        if (type & MAP_READ)
            g_mmap->map[page] = mem + (PAGE_SIZE_8 * i);

        if (type & MAP_WRITE)
            g_mmap->map[page + PAGE_WADD] = mem + (PAGE_SIZE_8 * i);
    }
}

void TMS34010UnmapMemory(UINT32 start, UINT32 end, UINT8 type)
{
    const int max_pages = (PFN(end) - PFN(start)) + 1;

    int page = PFN(start);
    for (int i = 0; i < max_pages; i++, page++) {

        if (type & MAP_READ)
            g_mmap->map[page] = NULL;

        if (type & MAP_WRITE)
            g_mmap->map[page + PAGE_WADD] = NULL;
    }
}

void TMS34010MapHandler(uintptr_t num, UINT32 start, UINT32 end, UINT8 type)
{
    const int max_pages = (PFN(end) - PFN(start)) + 1;

    int page = PFN(start);
    for (int i = 0; i < max_pages; i++, page++) {

        if (type & MAP_READ)
            g_mmap->map[page] = (UINT8*) num;

        if (type & MAP_WRITE)
            g_mmap->map[page + PAGE_WADD] = (UINT8*) num;
    }
}

void TMS34010UnmapHandler(INT32 start, UINT32 end, UINT8 type)
{
    const int max_pages = (PFN(end) - PFN(start)) + 1;

    int page = PFN(start);
    for (int i = 0; i < max_pages; i++, page++) {

        if (type & MAP_READ)
            g_mmap->map[page] = (UINT8*) NULL;

        if (type & MAP_WRITE)
            g_mmap->map[page + PAGE_WADD] = (UINT8*) NULL;
    }
}

int TMS34010SetReadHandler(UINT32 num, pTMS34010ReadHandler handler)
{
    if (num >= MAXHANDLER)
        return 1;
    g_mmap->read[num] = handler;
    return 0;
}

int TMS34010SetWriteHandler(UINT32 num, pTMS34010WriteHandler handler)
{
    if (num >= MAXHANDLER)
        return 1;
    g_mmap->write[num] = handler;
    return 0;
}

int TMS34010SetHandlers(UINT32 num, pTMS34010ReadHandler rhandler, pTMS34010WriteHandler whandler)
{
    if (num >= MAXHANDLER)
        return 1;
    g_mmap->read[num] = rhandler;
    g_mmap->write[num] = whandler;
    return 0;
}

