// TMS34 - cpu interface for MAME 0.127 TMS340x0 cpu core
#include "burnint.h"
#include "tms34/tms34010.h"
#include "tms34_intf.h"

#define ADDR_BITS   32
#define PAGE_SIZE   0x1000
#define PAGE_SIZE_8 (0x1000 >> 3)
#define PAGE_SHIFT  12
#define PAGE_MASK   0xFFF
#define PAGE_COUNT  (1 << (ADDR_BITS - PAGE_SHIFT))
#define PAGE_WADD    (PAGE_COUNT)
#define MAXHANDLER  32
#define PFN(x)  (((x) >> PAGE_SHIFT) & 0xFFFFF)

static INT32 CPU_TYPE = 0;

template<typename T>
inline T tms_fast_read(UINT8 *ptr, UINT32 adr) {
    return *((T*)  ((UINT8*) ptr + TOBYTE(adr & PAGE_MASK)));
}

template<typename T>
inline void tms_fast_write(UINT8 *xptr, UINT32 adr, T value) {
    T *ptr = ((T*)  ((UINT8*) xptr + TOBYTE(adr & PAGE_MASK)));
    *ptr = value;
}

static pTMS34010ScanlineRender scanlineRenderCallback = NULL;

struct TMS34010MemoryMap
{
    UINT8 *map[PAGE_COUNT * 2];

    pTMS34010ReadHandler read[MAXHANDLER];
    pTMS34010WriteHandler write[MAXHANDLER];
};

static TMS34010MemoryMap g_mmap;

static UINT16 default_read(UINT32 address) { return ~0; }
static void default_write(UINT32 address, UINT16 value) {}
static void default_shift_op(UINT32,UINT16*){}


static UINT16 IO_read(UINT32 address) { return tms34010_io_register_r(address); }
static void IO_write(UINT32 address, UINT16 value) { tms34010_io_register_w(address, value); }
static UINT16 IO_read020(UINT32 address) { return tms34020_io_register_r(address); }
static void IO_write020(UINT32 address, UINT16 value) { tms34020_io_register_w(address, value); }

// cheat-engine hook-up
void TMS34010Open(INT32 num)
{
	// not used, single core.
}

void TMS34010Close()
{
	// not used, single core.
}

INT32 TMS34010GetActive()
{
	return 0; // cpu is always active
}

void TMS34010WriteROM(UINT32 address, UINT8 value);
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
	TMS34010WriteROM,
	TMS34010GetActive,
	TMS34010TotalCyclesi32,
	TMS34010NewFrame,
	TMS34010Idle,
	TMS34010SetIRQLine,
	TMS34010Run,
	TMS34010RunEnd,
	TMS34010Reset,
	0x100000000ULL,
	0
};
// end cheat-engine hook-up

void TMS34010Init()
{
	CPU_TYPE = 10;
	tms34010_init();
	TMS34010SetToShift(default_shift_op);
	TMS34010SetFromShift(default_shift_op);

	// map IO registers
	TMS34010MapReset();
    TMS34010SetHandlers(MAXHANDLER-1, IO_read, IO_write);
	TMS34010MapHandler(MAXHANDLER-1, 0xc0000000, 0xc00001ff, MAP_READ | MAP_WRITE);

	CpuCheatRegister(0, &TMS34010Config);
}

void TMS34020Init()
{
	CPU_TYPE = 20;
	tms34010_init();
	TMS34010SetToShift(default_shift_op);
	TMS34010SetFromShift(default_shift_op);

    // map IO registers
    TMS34010SetHandlers(MAXHANDLER-1, IO_read020, IO_write020);
	TMS34010MapHandler(MAXHANDLER-1, 0xc0000000, 0xc00003ff, MAP_READ | MAP_WRITE);

	CpuCheatRegister(0, &TMS34010Config);
}

void TMS34010Exit()
{
	tms34010_exit();

	CPU_TYPE = 0;
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
	tms34010_new_frame();
}

void TMS34010RunEnd()
{
	tms34010_stop();
}

void TMS34010Scan(INT32 nAction)
{
	tms34010_scan(nAction);
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
	switch (CPU_TYPE) {
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
    scanlineRenderCallback = sr;
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
	tms34010_generate_scanline(line, scanlineRenderCallback);
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
    UINT8 *pr = g_mmap.map[PFN(address)];
    if ((uintptr_t)pr >= MAXHANDLER) {
        // address is bit-address
        return tms_fast_read<UINT8>(pr,address);
    } else {
        return g_mmap.read[(uintptr_t)pr](address);
    }
}

UINT16 TMS34010ReadWord(UINT32 address)
{
	address <<= 3;
    UINT8 *pr = g_mmap.map[PFN(address)];
    if ((uintptr_t)pr >= MAXHANDLER) {
        // address is bit-address
        return BURN_ENDIAN_SWAP_INT16(tms_fast_read<UINT16>(pr,address));
    } else {
        return g_mmap.read[(uintptr_t)pr](address);
	}
}

void TMS34010WriteByte(UINT32 address, UINT8 value)
{
	address <<= 3;
    UINT8 *pr = g_mmap.map[PAGE_WADD + PFN(address)];
    if ((uintptr_t)pr >= MAXHANDLER) {
        // address is bit-address
        return tms_fast_write<UINT8>(pr,address, value);
    } else {
        return g_mmap.write[(uintptr_t)pr](address, value);
    }
}

void TMS34010WriteWord(UINT32 address, UINT16 value)
{
	address <<= 3;
    UINT8 *pr = g_mmap.map[PAGE_WADD + PFN(address)];
    if ((uintptr_t)pr >= MAXHANDLER) {
        // address is bit-address
        return tms_fast_write<UINT16>(pr,address, BURN_ENDIAN_SWAP_INT16(value));
    } else {
        return g_mmap.write[(uintptr_t)pr](address, value);
    }
}

void TMS34010WriteROM(UINT32 address, UINT8 value) // for cheat-engine
{
    UINT8 *pr = g_mmap.map[PAGE_WADD + PFN(address)];
    if ((uintptr_t)pr >= MAXHANDLER) {
        // address is bit-address
		tms_fast_write<UINT8>(pr,address, value);
    } else {
		g_mmap.write[(uintptr_t)pr](address, value);
	}

	pr = g_mmap.map[PFN(address)];
    if ((uintptr_t)pr >= MAXHANDLER) {
        // address is bit-address
		tms_fast_write<UINT8>(pr,address, value);
	}
}

void TMS34010MapReset()
{
    for (int page = 0; page < PAGE_COUNT; page++) {
        g_mmap.map[page] = NULL;
        g_mmap.map[page + PAGE_WADD] = NULL;
    }
    for (int handler = 0; handler < MAXHANDLER; handler++) {
        g_mmap.read[handler] = default_read;
        g_mmap.write[handler] = default_write;
    }
}

void TMS34010MapMemory(UINT8 *mem, UINT32 start, UINT32 end, UINT8 type)
{
    const int max_pages = (PFN(end) - PFN(start)) + 1;

    int page = PFN(start);
    for (int i = 0; i < max_pages; i++, page++) {

        if (type & MAP_READ)
            g_mmap.map[page] = mem + (PAGE_SIZE_8 * i);

        if (type & MAP_WRITE)
            g_mmap.map[page + PAGE_WADD] = mem + (PAGE_SIZE_8 * i);
    }
}

void TMS34010MapHandler(uintptr_t num, UINT32 start, UINT32 end, UINT8 type)
{
    const int max_pages = (PFN(end) - PFN(start)) + 1;

    int page = PFN(start);
    for (int i = 0; i < max_pages; i++, page++) {

        if (type & MAP_READ)
            g_mmap.map[page] = (UINT8*) num;

        if (type & MAP_WRITE)
            g_mmap.map[page + PAGE_WADD] = (UINT8*) num;
    }
}

int TMS34010SetReadHandler(UINT32 num, pTMS34010ReadHandler handler)
{
    if (num >= MAXHANDLER)
        return 1;
    g_mmap.read[num] = handler;
    return 0;
}

int TMS34010SetWriteHandler(UINT32 num, pTMS34010WriteHandler handler)
{
    if (num >= MAXHANDLER)
        return 1;
    g_mmap.write[num] = handler;
    return 0;
}

int TMS34010SetHandlers(UINT32 num, pTMS34010ReadHandler rhandler, pTMS34010WriteHandler whandler)
{
    if (num >= MAXHANDLER)
        return 1;
    g_mmap.read[num] = rhandler;
    g_mmap.write[num] = whandler;
    return 0;
}

