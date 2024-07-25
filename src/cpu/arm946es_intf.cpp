#include "burnint.h"
#include "arm946es_intf.h"

//#define DEBUG_LOG

#define MAX_MEMORY	0x100000000 // more than good enough for pgm2
#define MAX_MEMORY_AND	(MAX_MEMORY - 1)
#define PAGE_SIZE	0x00010000 // 65536 would be better...
#define PAGE_COUNT	(MAX_MEMORY/PAGE_SIZE)
#define PAGE_SHIFT	16	// 0x1000 -> 16 bits
#define PAGE_BYTE_AND	0x0ffff	// 0x10000 - 1 (byte align)
#define PAGE_WORD_AND	0x0fffe	// 0x10000 - 2 (word align)
#define PAGE_LONG_AND	0x0fffc // 0x10000 - 4 (ignore last 4 bytes, long align)

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

static UINT32 Arm946esIdleLoop = ~0;

void Arm7RunEnd();
void Arm7RunEndEatCycles();
void Arm7BurnCycles(INT32 cycles);
INT32 Arm7Idle(int cycles);
INT32 Arm7TotalCycles();
void Arm7NewFrame();
INT32 Arm7GetActive();

void Arm7Init(INT32 nCPU);
void Arm7Reset();
INT32 Arm7Run(INT32 cycles);
void Arm7Exit();
void Arm7Open(INT32 );
void Arm7Close();

INT32 Arm7Scan(INT32 nAction);

extern void arm7_set_irq_line(INT32 irqline, INT32 state);

extern void init_arm946es_calllback();
extern void exit_arm946es_calllback();

extern UINT8 m_archRev;

extern UINT32 cp15_control, cp15_itcm_base, cp15_dtcm_base, cp15_itcm_size, cp15_dtcm_size;
extern UINT32 cp15_itcm_end, cp15_dtcm_end, cp15_itcm_reg, cp15_dtcm_reg;
extern UINT8 ITCM[0x8000], DTCM[0x4000];

extern void RefreshITCM();
extern void RefreshDTCM();

extern void arm7_set_irq_line(INT32 irqline, INT32 state);

static void core_set_irq(INT32 /*cpu*/, INT32 irqline, INT32 state)
{
    arm7_set_irq_line(irqline, state);
}

void Arm946esMapMemory(UINT8 *src, UINT32 start, UINT32 finish, INT32 type)
{
#if defined FBNEO_DEBUG
    if (!DebugCPU_ARM7Initted) bprintf(PRINT_ERROR, _T("Arm946esMapMemory called without init\n"));
    if (start >= MAX_MEMORY || finish >= MAX_MEMORY) bprintf (PRINT_ERROR, _T("Arm946esMapMemory memory range unsupported 0x%8.8x-0x%8.8x\n"), start, finish);
#endif

    UINT32 len = (finish-start) >> PAGE_SHIFT;

    for (UINT32 i = 0; i < len+1; i++)
    {
        UINT32 offset = i + (start >> PAGE_SHIFT);
        if (type & (1 <<  READ)) membase[ READ][offset] = src + (i << PAGE_SHIFT);
        if (type & (1 << WRITE)) membase[WRITE][offset] = src + (i << PAGE_SHIFT);
        if (type & (1 << FETCH)) membase[FETCH][offset] = src + (i << PAGE_SHIFT);
    }
}

void Arm946esWriteByte(UINT32 addr, UINT8 data)
{
    addr &= MAX_MEMORY_AND;

    if (membase[WRITE][addr >> PAGE_SHIFT] != NULL) {
        membase[WRITE][addr >> PAGE_SHIFT][addr & PAGE_BYTE_AND] = data;
        return;
    }

    if (pWriteByteHandler) {
        pWriteByteHandler(addr, data);
    }
}

void Arm946esWriteWord(UINT32 addr, UINT16 data)
{
    addr &= MAX_MEMORY_AND;

    if (membase[WRITE][addr >> PAGE_SHIFT] != NULL) {
        *((UINT16*)(membase[WRITE][addr >> PAGE_SHIFT] + (addr & PAGE_WORD_AND))) = BURN_ENDIAN_SWAP_INT16(data);
        return;
    }

    if (pWriteWordHandler) {
        pWriteWordHandler(addr, data);
    }
}

void Arm946esWriteLong(UINT32 addr, UINT32 data)
{
    addr &= MAX_MEMORY_AND;

    if (membase[WRITE][addr >> PAGE_SHIFT] != NULL) {
        *((UINT32*)(membase[WRITE][addr >> PAGE_SHIFT] + (addr & PAGE_LONG_AND))) = BURN_ENDIAN_SWAP_INT32(data);
        return;
    }

    if (pWriteLongHandler) {
        pWriteLongHandler(addr, data);
    }
}


UINT8 Arm946esReadByte(UINT32 addr)
{
    addr &= MAX_MEMORY_AND;

    if (membase[ READ][addr >> PAGE_SHIFT] != NULL) {
        return membase[READ][addr >> PAGE_SHIFT][addr & PAGE_BYTE_AND];
    }

    if (pReadByteHandler) {
        return pReadByteHandler(addr);
    }

    return 0;
}

UINT16 Arm946esReadWord(UINT32 addr)
{

    addr &= MAX_MEMORY_AND;

    if (membase[ READ][addr >> PAGE_SHIFT] != NULL) {
        return *((UINT16*)(membase[READ][addr >> PAGE_SHIFT] + (addr & PAGE_WORD_AND)));
    }

    if (pReadWordHandler) {
        return pReadWordHandler(addr);
    }

    return 0;
}

UINT32 Arm946esReadLong(UINT32 addr)
{
    addr &= MAX_MEMORY_AND;

    if (membase[ READ][addr >> PAGE_SHIFT] != NULL) {
        return *((UINT32*)(membase[READ][addr >> PAGE_SHIFT] + (addr & PAGE_LONG_AND)));
    }

    if (pReadLongHandler) {
        return pReadLongHandler(addr);
    }

    return 0;
}

UINT16 Arm946esFetchWord(UINT32 addr)
{
    addr &= MAX_MEMORY_AND;

    // speed hack -- skip idle loop...
    if (addr == Arm946esIdleLoop) {
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

UINT32 Arm946esFetchLong(UINT32 addr)
{

    addr &= MAX_MEMORY_AND;

    // speed hack - skip idle loop...
    if (addr == Arm946esIdleLoop) {
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

void Arm946esOpen(int nCPU)
{
	Arm7Open(nCPU);
}

void Arm946esClose()
{
	Arm7Close();
}

void Arm946esReset()
{
	Arm7Reset();
}

INT32 Arm946esGetActive()
{
	return 0;
}

void Arm946esExit() // only one cpu supported
{
	for (INT32 i = 0; i < 3; i++) {
		if (membase[i]) {
			free(membase[i]);
			membase[i] = NULL;
		}
	}
    
    Arm946esIdleLoop = ~0;
	m_archRev = 0;
    exit_arm946es_calllback();
}


int Arm946esTotalCycles()
{
	return Arm7TotalCycles();
}

void Arm946esRunEndEatCycles()
{
    Arm7RunEndEatCycles();
}

void Arm946esRunEnd()
{
	Arm7RunEnd();
}

void Arm946esBurnCycles(int cycles)
{
	Arm7BurnCycles(cycles);
}

INT32 Arm946esIdle(int cycles)
{
	return Arm7Idle(cycles);
}

void Arm946esNewFrame()
{
	Arm7NewFrame();
}

void Arm946esSetWriteByteHandler(void (*write)(UINT32, UINT8))
{
    pWriteByteHandler = write;
}

void Arm946esSetWriteWordHandler(void (*write)(UINT32, UINT16))
{
    pWriteWordHandler = write;
}

void Arm946esSetWriteLongHandler(void (*write)(UINT32, UINT32))
{
    pWriteLongHandler = write;
}

void Arm946esSetReadByteHandler(UINT8 (*read)(UINT32))
{
    pReadByteHandler = read;
}

void Arm946esSetReadWordHandler(UINT16 (*read)(UINT32))
{
    pReadWordHandler = read;
}

void Arm946esSetReadLongHandler(UINT32 (*read)(UINT32))
{
    pReadLongHandler = read;
}


INT32 Arm946esRun(INT32 cycles) {
	return Arm7Run(cycles);
}

void Arm946esSetIRQLine(INT32 line, INT32 state)
{
	if (state == CPU_IRQSTATUS_NONE || state == CPU_IRQSTATUS_ACK) {
		arm7_set_irq_line(line, state);
	}
	else if (CPU_IRQSTATUS_AUTO) {
		arm7_set_irq_line(line, CPU_IRQSTATUS_ACK);
		Arm946esRun(0);
		arm7_set_irq_line(line, CPU_IRQSTATUS_NONE);
	}
}

// Set address of idle loop start - speed hack
void Arm946esSetIdleLoopAddress(UINT32 address)
{
    Arm946esIdleLoop = address;
}

// For cheats/etc

static void Arm946es_write_rom_byte(UINT32 addr, UINT8 data)
{
#if defined FBNEO_DEBUG
    if (!DebugCPU_ARM7Initted) bprintf(PRINT_ERROR, _T("Arm7_write_rom_byte called without init\n"));
#endif
    addr &= MAX_MEMORY_AND;

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

static cpu_core_config Arm946esCheatCpuConfig =
{
    "Arm946es",
    Arm946esOpen,
    Arm946esClose,
    Arm946esReadByte,
    Arm946es_write_rom_byte,
    Arm946esGetActive,
    Arm946esTotalCycles,
    Arm946esNewFrame,
    Arm946esIdle,
    core_set_irq,
    Arm946esRun,
    Arm946esRunEnd,
    Arm946esReset,
	Arm946esScan,
	Arm946esExit,
    MAX_MEMORY,
    0
};

void Arm946esInit(INT32 nCPU) // only one cpu supported
{
    for (INT32 i = 0; i < 3; i++) {
        membase[i] = (UINT8**)malloc(PAGE_COUNT * sizeof(UINT8*));
        memset(membase[i], 0, PAGE_COUNT * sizeof(UINT8*));
    }
    
    m_archRev = 5;
    cp15_control = 0x78;
    cp15_itcm_base = 0xffffffff;
    cp15_itcm_size = 0;
    cp15_itcm_end = 0;
    cp15_dtcm_base = 0xffffffff;
    cp15_dtcm_size = 0;
    cp15_dtcm_end = 0;
    cp15_itcm_reg = cp15_dtcm_reg = 0;
    init_arm946es_calllback();
    memset(ITCM, 0, 0x8000);
    memset(DTCM, 0, 0x4000);

    CpuCheatRegister(nCPU, &Arm946esCheatCpuConfig);
}
