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

#define    ARM946ES_MAXHANDLER    (10)

static pArm946esReadByteHandler ReadByte[ARM946ES_MAXHANDLER];
static pArm946esWriteByteHandler WriteByte[ARM946ES_MAXHANDLER];
static pArm946esReadWordHandler ReadWord[ARM946ES_MAXHANDLER];
static pArm946esWriteWordHandler WriteWord[ARM946ES_MAXHANDLER];
static pArm946esReadLongHandler ReadLong[ARM946ES_MAXHANDLER];
static pArm946esWriteLongHandler WriteLong[ARM946ES_MAXHANDLER];

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

void Arm946esMapHandler(uintptr_t nHandler, UINT32 start, UINT32 finish, INT32 type)
{
    UINT32 len = (finish - start) >> PAGE_SHIFT;

    for (UINT32 i = 0; i < len + 1; i++)
    {
        UINT32 offset = i + (start >> PAGE_SHIFT);
        if (type & (1 << READ)) membase[READ][offset] = (UINT8*)nHandler;
        if (type & (1 << WRITE)) membase[WRITE][offset] = (UINT8*)nHandler;
        if (type & (1 << FETCH)) membase[FETCH][offset] = (UINT8*)nHandler;
    }
}

void Arm946esSetWriteByteHandler(INT32 i, pArm946esWriteByteHandler handle)
{
    WriteByte[i] = handle;
}

void Arm946esSetWriteWordHandler(INT32 i, pArm946esWriteWordHandler handle)
{
    WriteWord[i] = handle;
}

void Arm946esSetWriteLongHandler(INT32 i, pArm946esWriteLongHandler handle)
{
    WriteLong[i] = handle;
}

void Arm946esSetReadByteHandler(INT32 i, pArm946esReadByteHandler handle)
{
    ReadByte[i] = handle;
}

void Arm946esSetReadWordHandler(INT32 i, pArm946esReadWordHandler handle)
{
    ReadWord[i] = handle;
}

void Arm946esSetReadLongHandler(INT32 i, pArm946esReadLongHandler handle)
{
    ReadLong[i] = handle;
}

void Arm946esWriteByte(UINT32 addr, UINT8 data)
{
    addr &= MAX_MEMORY_AND;

#ifdef DEBUG_LOG
    bprintf(PRINT_NORMAL, _T("%5.5x, %2.2x wb\n"), addr, data);
#endif

    UINT8* ptr = membase[WRITE][addr >> PAGE_SHIFT];
    if ((uintptr_t)ptr >= ARM946ES_MAXHANDLER) {
        membase[WRITE][addr >> PAGE_SHIFT][addr & PAGE_BYTE_AND] = data;
        return;
    }

    if (WriteByte[(uintptr_t)ptr])
    {
        WriteByte[(uintptr_t)ptr](addr, data, 0xff);
    }
}

void Arm946esWriteWord(UINT32 addr, UINT16 data)
{
    addr &= MAX_MEMORY_AND;

#ifdef DEBUG_LOG
    bprintf(PRINT_NORMAL, _T("%5.5x, %8.8x wd\n"), addr, data);
#endif

    UINT8* ptr = membase[WRITE][addr >> PAGE_SHIFT];
    if ((uintptr_t)ptr >= ARM946ES_MAXHANDLER) {
        *((UINT16*)(membase[WRITE][addr >> PAGE_SHIFT] + (addr & PAGE_WORD_AND))) = data;
        return;
    }

    if (WriteWord[(uintptr_t)ptr])
    {
        WriteWord[(uintptr_t)ptr](addr, data, 0xffff);
    }
}

void Arm946esWriteLong(UINT32 addr, UINT32 data)
{
    addr &= MAX_MEMORY_AND;

#ifdef DEBUG_LOG
    bprintf(PRINT_NORMAL, _T("%5.5x, %8.8x wd\n"), addr, data);
#endif
    UINT8* ptr = membase[WRITE][addr >> PAGE_SHIFT];
    if ((uintptr_t)ptr >= ARM946ES_MAXHANDLER) {
        *((UINT32*)(membase[WRITE][addr >> PAGE_SHIFT] + (addr & PAGE_LONG_AND))) = data;
        return;
    }

    if (WriteLong[(uintptr_t)ptr])
    {
        WriteLong[(uintptr_t)ptr](addr, data, 0xffffffff);
    }
}


UINT8 Arm946esReadByte(UINT32 addr)
{
    addr &= MAX_MEMORY_AND;

#ifdef DEBUG_LOG
    bprintf(PRINT_NORMAL, _T("%5.5x, rl\n"), addr);
#endif
    UINT8* ptr = membase[READ][addr >> PAGE_SHIFT];
    if ((uintptr_t)ptr >= ARM946ES_MAXHANDLER) {
        return *((UINT16*)(membase[READ][addr >> PAGE_SHIFT] + (addr & PAGE_WORD_AND)));
    }

    if (ReadWord[(uintptr_t)ptr])
    {
        return ReadWord[(uintptr_t)ptr](addr);
    }
    return 0;
}

UINT16 Arm946esReadWord(UINT32 addr)
{
    addr &= MAX_MEMORY_AND;

#ifdef DEBUG_LOG
    bprintf(PRINT_NORMAL, _T("%5.5x, rl\n"), addr);
#endif

    UINT8* ptr = membase[READ][addr >> PAGE_SHIFT];
    if ((uintptr_t)ptr >= ARM946ES_MAXHANDLER) {
        return *((UINT32*)(membase[READ][addr >> PAGE_SHIFT] + (addr & PAGE_LONG_AND)));
    }

    if (ReadLong[(uintptr_t)ptr])
    {
        return ReadLong[(uintptr_t)ptr](addr);
    }

    return 0;
}

UINT32 Arm946esReadLong(UINT32 addr)
{
    addr &= MAX_MEMORY_AND;

#ifdef DEBUG_LOG
    bprintf(PRINT_NORMAL, _T("%5.5x, rl\n"), addr);
#endif

    UINT8* ptr = membase[READ][addr >> PAGE_SHIFT];
    if ((uintptr_t)ptr >= ARM946ES_MAXHANDLER) {
        return *((UINT32*)(membase[READ][addr >> PAGE_SHIFT] + (addr & PAGE_LONG_AND)));
    }

    if (ReadLong[(uintptr_t)ptr])
    {
        return ReadLong[(uintptr_t)ptr](addr);
    }

    return 0;
}

UINT16 Arm946esFetchWord(UINT32 addr)
{
    addr &= MAX_MEMORY_AND;

#ifdef DEBUG_LOG
    bprintf(PRINT_NORMAL, _T("%5.5x, rwo\n"), addr);
#endif

    // speed hack -- skip idle loop...
    if (addr == Arm946esIdleLoop) {
        Arm7RunEndEatCycles();
    }

    UINT8* ptr = membase[FETCH][addr >> PAGE_SHIFT];
    if ((uintptr_t)ptr >= ARM946ES_MAXHANDLER) {
        return *((UINT16*)(membase[FETCH][addr >> PAGE_SHIFT] + (addr & PAGE_WORD_AND)));
    }

    if (ReadWord[(uintptr_t)ptr])
    {
        return ReadWord[(uintptr_t)ptr](addr);
    }
    return 0;
}

UINT32 Arm946esFetchLong(UINT32 addr)
{
    addr &= MAX_MEMORY_AND;

#ifdef DEBUG_LOG
    bprintf(PRINT_NORMAL, _T("%5.5x, rlo\n"), addr);
#endif

    // speed hack - skip idle loop...
    if (addr == Arm946esIdleLoop) {
        Arm7RunEndEatCycles();
    }

    UINT8* ptr = membase[FETCH][addr >> PAGE_SHIFT];
    if ((uintptr_t)ptr >= ARM946ES_MAXHANDLER) {
        return *((UINT32*)(membase[FETCH][addr >> PAGE_SHIFT] + (addr & PAGE_LONG_AND)));
    }

    if (ReadLong[(uintptr_t)ptr])
    {
        return ReadLong[(uintptr_t)ptr](addr);
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

INT32 Arm946esRun(INT32 cycles) {
	return Arm7Run(cycles);
}

// For cheats/etc

static void Arm946es_write_rom_byte(UINT32 addr, UINT8 data)
{
#if defined FBNEO_DEBUG
    if (!DebugCPU_ARM7Initted) bprintf(PRINT_ERROR, _T("Arm7_write_rom_byte called without init\n"));
#endif
    addr &= MAX_MEMORY_AND;

    // write to rom & ram
    UINT8* WritePtr = membase[WRITE][addr >> PAGE_SHIFT];
    if ((uintptr_t)WritePtr >= ARM946ES_MAXHANDLER) {
        membase[WRITE][addr >> PAGE_SHIFT][addr & PAGE_BYTE_AND] = data;
    }

    UINT8* ReadPtr = membase[READ][addr >> PAGE_SHIFT];
    if ((uintptr_t)ReadPtr >= ARM946ES_MAXHANDLER) {
        membase[READ][addr >> PAGE_SHIFT][addr & PAGE_BYTE_AND] = data;
    }

    if (WriteByte[(uintptr_t)WritePtr])
    {
        WriteByte[(uintptr_t)WritePtr](addr, data, 0xff);
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
