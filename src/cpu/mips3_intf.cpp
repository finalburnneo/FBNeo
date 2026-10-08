// MIPS III (R4600) interface: memory map, handlers and FBNeo cpu core glue
// The interpreter itself is in mips3/mips3.cpp

#include "burnint.h"
#include "mips3_intf.h"
#include "mips3/mips3.h"
#include <stdlib.h>

Mips3PhysPage *Mips3Phys = NULL;

pMips3ReadByteHandler Mips3ReadByteH[MIPS3_MAXHANDLER];
pMips3WriteByteHandler Mips3WriteByteH[MIPS3_MAXHANDLER];
pMips3ReadHalfHandler Mips3ReadHalfH[MIPS3_MAXHANDLER];
pMips3WriteHalfHandler Mips3WriteHalfH[MIPS3_MAXHANDLER];
pMips3ReadWordHandler Mips3ReadWordH[MIPS3_MAXHANDLER];
pMips3WriteWordHandler Mips3WriteWordH[MIPS3_MAXHANDLER];
pMips3ReadDoubleHandler Mips3ReadDoubleH[MIPS3_MAXHANDLER];
pMips3WriteDoubleHandler Mips3WriteDoubleH[MIPS3_MAXHANDLER];

static INT32 bMips3IntfInitted = 0;

static UINT8 DefReadByte(UINT32) { return 0; }
static UINT16 DefReadHalf(UINT32) { return 0; }
static UINT32 DefReadWord(UINT32) { return 0; }
static UINT64 DefReadDouble(UINT32) { return 0; }
static void DefWriteByte(UINT32, UINT8) { }
static void DefWriteHalf(UINT32, UINT16) { }
static void DefWriteWord(UINT32, UINT32) { }
static void DefWriteDouble(UINT32, UINT64) { }

// ---------------------------------------------------------------------------
// memory map

int Mips3MapMemory(unsigned char* pMemory, unsigned int nStart, unsigned int nEnd, int nType)
{
	UINT32 first = (nStart & MIPS3_PHYS_MASK) >> 12;
	UINT32 last = (nEnd & MIPS3_PHYS_MASK) >> 12;

	for (UINT32 page = first, i = 0; page <= last; page++, i++) {
		if (nType & MAP_READ)  Mips3Phys[page].read = pMemory + 0x1000 * i;
		if (nType & MAP_WRITE) Mips3Phys[page].write = pMemory + 0x1000 * i;
		mips3_page_mapped(page);
	}
	return 0;
}

int Mips3MapHandler(uintptr_t nHandler, unsigned int nStart, unsigned int nEnd, int nType)
{
	UINT32 first = (nStart & MIPS3_PHYS_MASK) >> 12;
	UINT32 last = (nEnd & MIPS3_PHYS_MASK) >> 12;

	for (UINT32 page = first; page <= last; page++) {
		if (nType & MAP_READ)  Mips3Phys[page].read = (UINT8*)nHandler;
		if (nType & MAP_WRITE) Mips3Phys[page].write = (UINT8*)nHandler;
		mips3_page_mapped(page);
	}
	return 0;
}

void Mips3InvalidateRange(unsigned int nStart, unsigned int nEnd)
{
	mips3_invalidate(nStart, nEnd);
}

#define MIPS3_SET_HANDLER(name, type, table) \
int name(int i, type pHandler) { if (i < 0 || i >= MIPS3_MAXHANDLER) return 1; table[i] = pHandler; return 0; }

MIPS3_SET_HANDLER(Mips3SetReadByteHandler, pMips3ReadByteHandler, Mips3ReadByteH)
MIPS3_SET_HANDLER(Mips3SetWriteByteHandler, pMips3WriteByteHandler, Mips3WriteByteH)
MIPS3_SET_HANDLER(Mips3SetReadHalfHandler, pMips3ReadHalfHandler, Mips3ReadHalfH)
MIPS3_SET_HANDLER(Mips3SetWriteHalfHandler, pMips3WriteHalfHandler, Mips3WriteHalfH)
MIPS3_SET_HANDLER(Mips3SetReadWordHandler, pMips3ReadWordHandler, Mips3ReadWordH)
MIPS3_SET_HANDLER(Mips3SetWriteWordHandler, pMips3WriteWordHandler, Mips3WriteWordH)
MIPS3_SET_HANDLER(Mips3SetReadDoubleHandler, pMips3ReadDoubleHandler, Mips3ReadDoubleH)
MIPS3_SET_HANDLER(Mips3SetWriteDoubleHandler, pMips3WriteDoubleHandler, Mips3WriteDoubleH)

// ---------------------------------------------------------------------------
// execution

int Mips3Run(int cycles)
{
	return mips3_run(cycles);
}

void Mips3RunEnd()
{
	mips3_run_end();
}

int Mips3Idle(int cycles)
{
	return mips3_idle(cycles);
}

INT64 Mips3TotalCycles()
{
	return (INT64)mips3_total_cycles();
}

void Mips3NewFrame()
{
	// Count/Compare are derived from the total cycle count, it must keep running across frames
}

void Mips3SetIRQLine(const int line, const int state)
{
	mips3_set_irq_line(line, state);
}

unsigned int Mips3GetPC()
{
	return mips3_get_pc();
}

int Mips3UseRecompiler(bool)
{
	return 0;
}

// ---------------------------------------------------------------------------
// cpu core config (cheats, debugger)

static UINT8 Mips3CheatRead(UINT32 a)
{
	return mips3_read_phys8(a);
}

static void Mips3CheatWrite(UINT32 a, UINT8 d)
{
	a &= MIPS3_PHYS_MASK;
	Mips3PhysPage *p = &Mips3Phys[a >> 12];

	if (MIPS3_IS_PTR(p->write)) p->write[a & 0xfff] = d;
	else if (MIPS3_IS_PTR(p->read)) p->read[a & 0xfff] = d;
	else return;

	mips3_invalidate(a, a);
}

static void Mips3Open(INT32) { }
static void Mips3Close() { }
static INT32 Mips3GetActive() { return 0; }
static INT32 Mips3CoreTotalCycles() { return (INT32)mips3_total_cycles(); }
static INT32 Mips3CoreIdle(INT32 cycles) { return mips3_idle(cycles); }
static void Mips3CoreSetIRQ(INT32, INT32 line, INT32 state) { mips3_set_irq_line(line, state); }
static void Mips3CoreExit() { Mips3Exit(); }

cpu_core_config Mips3Config =
{
	"MIPS3",
	Mips3Open,
	Mips3Close,
	Mips3CheatRead,
	Mips3CheatWrite,
	Mips3GetActive,
	Mips3CoreTotalCycles,
	Mips3NewFrame,
	Mips3CoreIdle,
	Mips3CoreSetIRQ,
	Mips3Run,
	Mips3RunEnd,
	Mips3Reset,
	Mips3Scan,
	Mips3CoreExit,
	MIPS3_PHYS_MASK,
	0
};

// ---------------------------------------------------------------------------
// init / exit / reset / state

int Mips3Init()
{
	Mips3Phys = (Mips3PhysPage*)calloc(MIPS3_PHYS_PAGES, sizeof(Mips3PhysPage));

	for (INT32 i = 0; i < MIPS3_MAXHANDLER; i++) {
		Mips3ReadByteH[i] = DefReadByte;
		Mips3ReadHalfH[i] = DefReadHalf;
		Mips3ReadWordH[i] = DefReadWord;
		Mips3ReadDoubleH[i] = DefReadDouble;
		Mips3WriteByteH[i] = DefWriteByte;
		Mips3WriteHalfH[i] = DefWriteHalf;
		Mips3WriteWordH[i] = DefWriteWord;
		Mips3WriteDoubleH[i] = DefWriteDouble;
	}

	mips3_init();

	CpuCheatRegister(0, &Mips3Config);

	bMips3IntfInitted = 1;
	return 0;
}

int Mips3Exit()
{
	if (!bMips3IntfInitted) return 0;

	mips3_exit();

	free(Mips3Phys);
	Mips3Phys = NULL;

	bMips3IntfInitted = 0;
	return 0;
}

void Mips3Reset()
{
	mips3_reset();
}

int Mips3Scan(int nAction)
{
	mips3_scan(nAction);
	return 0;
}
