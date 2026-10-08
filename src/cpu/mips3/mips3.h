// MIPS III core: interface between the interpreter (mips3.cpp) and FBNeo's mips3_intf.cpp

#ifndef MIPS3_CORE_H
#define MIPS3_CORE_H

#include "mips3_intf.h"

// physical address space handled by the memory map (kseg0/kseg1 only reach 512MB anyway)
#ifndef MIPS3_PHYS_BITS
#define MIPS3_PHYS_BITS		29
#endif

#define MIPS3_PHYS_MASK		((UINT32)(((UINT64)1 << MIPS3_PHYS_BITS) - 1))
#define MIPS3_PHYS_PAGES	(1 << (MIPS3_PHYS_BITS - 12))
#define MIPS3_MAXHANDLER	10

// a page pointer below MIPS3_MAXHANDLER is a handler index
#define MIPS3_IS_PTR(p)		((uintptr_t)(p) >= MIPS3_MAXHANDLER)

struct Mips3BlockPage;

struct Mips3PhysPage {
	UINT8 *read;			// host pointer, or handler index
	UINT8 *write;
	Mips3BlockPage *code;	// code cache for fetches from this page (NULL: not cacheable)
	Mips3BlockPage *wcode;	// code cache that writes to this page can modify
};

// memory map, owned by mips3_intf.cpp and read directly by the interpreter
extern Mips3PhysPage *Mips3Phys;
extern pMips3ReadByteHandler Mips3ReadByteH[MIPS3_MAXHANDLER];
extern pMips3WriteByteHandler Mips3WriteByteH[MIPS3_MAXHANDLER];
extern pMips3ReadHalfHandler Mips3ReadHalfH[MIPS3_MAXHANDLER];
extern pMips3WriteHalfHandler Mips3WriteHalfH[MIPS3_MAXHANDLER];
extern pMips3ReadWordHandler Mips3ReadWordH[MIPS3_MAXHANDLER];
extern pMips3WriteWordHandler Mips3WriteWordH[MIPS3_MAXHANDLER];
extern pMips3ReadDoubleHandler Mips3ReadDoubleH[MIPS3_MAXHANDLER];
extern pMips3WriteDoubleHandler Mips3WriteDoubleH[MIPS3_MAXHANDLER];

INT32 mips3_init();
void mips3_exit();
void mips3_reset();
void mips3_scan(INT32 nAction);

INT32 mips3_run(INT32 cycles);
void mips3_run_end();
INT32 mips3_idle(INT32 cycles);
UINT64 mips3_total_cycles();

void mips3_set_irq_line(INT32 line, INT32 state);
UINT32 mips3_get_pc();

// must be called after a Mips3Phys entry changed
void mips3_page_mapped(UINT32 page);
// drops cached code overlapping the physical range
void mips3_invalidate(UINT32 nStart, UINT32 nEnd);

UINT8 mips3_read_phys8(UINT32 pa);

#endif
