// MIPS III (R4600, little-endian) cached interpreter
//
// Decoded instructions are kept in blocks (straight-line code ending on a branch + delay slot,
// a block-ending instruction or a page boundary). Each decoded instruction holds a pointer to its
// final "leaf" handler, so replaying a block costs one indirect call per instruction.
// Define MIPS3_CACHED_INTERPRETER to 0 to fetch/decode every instruction (same handlers).
//
// Based in part on the MAME MIPS III core by Aaron Giles (BSD-3-Clause).

#include "burnint.h"
#include "mips3.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <map>
#include <vector>

#ifndef MIPS3_CACHED_INTERPRETER
#define MIPS3_CACHED_INTERPRETER	1
#endif

// temporary boot diagnostics (exceptions, unmapped accesses, Status writes, periodic PC)
#ifndef MIPS3_DEBUG_LOG
#define MIPS3_DEBUG_LOG		0
#endif

// instruction trace around the first SYSCALL (forces the uncached path while enabled)
#ifndef MIPS3_DEBUG_TRACE
#define MIPS3_DEBUG_TRACE	0
#endif

#define PHYS_MASK			MIPS3_PHYS_MASK
#define VIRT_PAGES			(1 << 20)
#define MIPS3_TLB_ENTRIES	48
#define MIPS3_BLOCK_MAX		128
#define MIPS3_FAST_ENTRIES	4096

// skip the rest of the timeslice when a loop provably spins without progress (polling RAM/IO)
// periodic report of the hottest blocks and of the cycles skipped by idle detection
#ifndef MIPS3_PROFILE
#define MIPS3_PROFILE		0
#endif

#ifndef MIPS3_IDLE_SKIP
#define MIPS3_IDLE_SKIP		1
#endif
#define MIPS3_ARENA_SIZE	(8 * 1024 * 1024)

enum {
	CP0_Index = 0, CP0_Random, CP0_EntryLo0, CP0_EntryLo1, CP0_Context, CP0_PageMask, CP0_Wired, CP0_Reserved7,
	CP0_BadVAddr, CP0_Count, CP0_EntryHi, CP0_Compare, CP0_Status, CP0_Cause, CP0_EPC, CP0_PRId,
	CP0_Config, CP0_LLAddr, CP0_WatchLo, CP0_WatchHi, CP0_XContext, CP0_ErrorEPC = 30
};

#define SR_IE		0x00000001
#define SR_EXL		0x00000002
#define SR_ERL		0x00000004
#define SR_KSU		0x00000018
#define SR_BEV		0x00400000
#define SR_FR		0x04000000
#define SR_CU0		0x10000000
#define SR_CU1		0x20000000
#define SR_CU2		0x40000000

enum {
	EXC_INT = 0, EXC_MOD = 1, EXC_TLBL = 2, EXC_TLBS = 3, EXC_ADEL = 4, EXC_ADES = 5,
	EXC_SYS = 8, EXC_BP = 9, EXC_RI = 10, EXC_CPU = 11, EXC_OV = 12, EXC_TR = 13
};

// vtlb entry: physical page | flags
#define VT_READ		1
#define VT_WRITE	2
#define VT_MATCH	4		// a TLB entry matched (invalid/modified exception instead of refill)

// decoder flags
#define F_BRANCH	1		// has a delay slot
#define F_END		2		// ends the block (state that affects interrupts/translation changed)
#define F_DELAY		4		// instruction sits in a delay slot

struct Mips3Insn;
typedef void (*Mips3Handler)(const Mips3Insn *I);

struct Mips3Insn {
	Mips3Handler fn;
	UINT32 op;
	UINT16 pc;		// offset in the 4KB page
	UINT16 flags;
};

struct Mips3Block {
	UINT16 start;	// page offset of the first instruction
	UINT16 fall;	// page offset to continue at when nothing changed the flow (may be >= 0x1000)
	UINT16 count;
	UINT8 split;	// ends on a branch whose delay slot runs uncached (next page, or a branch itself)
	UINT8 idle;		// loops on itself with no stores: IDLE_STABLE or IDLE_TIMED (see IdleAnalyse)
	UINT32 wmask;	// registers written by the block, induction counters excluded
	UINT8 nind;		// induction counters: "addiu/daddiu r, r, imm" whose register nothing else reads
	UINT8 indreg[4];
	UINT8 ind64[4];
	INT16 indimm[4];
#if MIPS3_PROFILE
	UINT32 vpc;
	UINT64 prof;	// instructions executed in this block
#endif
	Mips3Insn insn[1];
};

// direct-mapped virtual pc -> block lookup, invalidated as a whole by bumping nFastGen
struct Mips3FastEntry {
	UINT32 pc;
	UINT32 gen;
	Mips3Block *b;
};

// one per distinct host memory page that code can be fetched from (mirrors share it)
struct Mips3BlockPage {
	Mips3Block **blk;	// 1024 entry slots, allocated on first use
	UINT16 codemask;	// 256-byte chunks holding cached code
};

struct Mips3TlbEntry {
	UINT64 mask;
	UINT64 hi;
	UINT64 lo[2];
};

struct Mips3State {
	UINT64 r[32];
	UINT64 hi, lo;
	UINT64 cp0[32];
	UINT64 fpr[32];
	UINT32 fcr31;
	UINT32 pc;
	UINT32 delay_pending;	// a branch was taken and its delay slot (at pc) is still to execute
	UINT32 delay_target;
	UINT32 llbit;
	UINT32 irq_pad;
	UINT64 total_cycles;
	UINT64 count_zero;		// total_cycles when Count was 0
	UINT64 random_zero;
	UINT64 compare_fire;	// total_cycles at which Count reaches Compare
	Mips3TlbEntry tlb[MIPS3_TLB_ENTRIES];
};

static Mips3State M;

// execution context of the instruction(s) currently running
static struct {
	UINT32 vpage;		// virtual page of the running code
	UINT32 next_pc;		// where to go after the block / after a taken branch's delay slot
	UINT32 brk;			// stop the block now and go to next_pc
	UINT32 blk_start;	// page offset of the first instruction of the running block
	UINT32 anext;		// branch-in-delay-slot: decision of the first branch
	UINT32 dpend;		// the block ends with a delay slot still to execute (at next_pc, then dtarget)
	UINT32 dtarget;
} Mx;

// total_cycles at which the run loop must look at interrupts, the timer or a pending delay slot;
// set to 0 whenever something that may need attention changes
static UINT64 nNextEvent = 0;

#if MIPS3_DEBUG_LOG
static INT32 nDbgExc = 0, nDbgIrq = 0, nDbgUnmapped = 0, nDbgStatus = 0, nDbgEret = 0, nDbgTlb = 0;
static UINT64 nDbgNextPc = 0;
#endif

#if MIPS3_DEBUG_TRACE
struct Mips3TraceEntry { UINT32 pc, op; UINT64 val; };
static Mips3TraceEntry DbgRing[512];
static INT32 nDbgRingPos = 0, nDbgTraceFwd = 0, nDbgTraceState = 0;
#endif

static INT32 nMips3ICount;
static INT32 bMips3Initted = 0;

static UINT32 *Vtlb = NULL;
static UINT32 TlbMappedVpn[MIPS3_TLB_ENTRIES][2];
static UINT32 TlbMappedCount[MIPS3_TLB_ENTRIES][2];

static std::map<UINT8*, Mips3BlockPage*> *BlockPageMap = NULL;
static std::vector<Mips3BlockPage*> *BlockPageList = NULL;
static UINT8 *Arena = NULL;
#if MIPS3_CACHED_INTERPRETER
static Mips3FastEntry FastCache[MIPS3_FAST_ENTRIES];
#endif
static UINT32 nFastGen = 1;

#if MIPS3_PROFILE
static UINT64 nProfIdle = 0, nProfStep = 0, nProfStart = 0, nProfNext = 0;
#endif
static UINT32 ArenaUsed = 0;

static Mips3Handler Mips3Decode(UINT32 op, UINT32 *flags);
static void Mips3FlushCache();

#define CP0(x)		M.cp0[x]
#define SR			M.cp0[CP0_Status]
#define CAUSE		M.cp0[CP0_Cause]

#if defined(_MSC_VER)
#define MIPS3_INLINE	__forceinline
#elif defined(__GNUC__)
#define MIPS3_INLINE	inline __attribute__((always_inline))
#else
#define MIPS3_INLINE	inline
#endif

#define EAT_CYCLES(n)	do { M.total_cycles += (n); nMips3ICount -= (n); } while (0)

// ---------------------------------------------------------------------------
// physical memory access

#define IS_PTR(p)	MIPS3_IS_PTR(p)

static void CodeWrite(Mips3BlockPage *bp, UINT32 off)
{
	UINT32 c = off >> 8;
	if (!(bp->codemask & (1 << c))) return;

	// blocks are at most (MIPS3_BLOCK_MAX + 4) entries, so only these can overlap the chunk
	INT32 first = (INT32)(c << 8) - (MIPS3_BLOCK_MAX + 4) * 4;
	if (first < 0) first = 0;
	UINT32 last = (c << 8) + 0xff;

	for (UINT32 o = first; o <= last; o += 4) {
		Mips3Block *b = bp->blk[o >> 2];
		if (b == NULL) continue;
		UINT32 c0 = b->start >> 8;
		UINT32 c1 = (b->start + b->count * 4 - 1) >> 8;
		if (c >= c0 && c <= c1) { bp->blk[o >> 2] = NULL; nFastGen++; }
	}

	bp->codemask &= ~(1 << c);
}

static inline void CheckCodeWrite(Mips3PhysPage *p, UINT32 pa)
{
	if (p->wcode && p->wcode->codemask) CodeWrite(p->wcode, pa & 0xfff);
}

#if MIPS3_DEBUG_LOG
static void DbgUnmapped(const char *what, UINT32 pa, UINT64 v)
{
	if (nDbgUnmapped < 100) {
		nDbgUnmapped++;
		bprintf(PRINT_NORMAL, _T("MIPS3 unmapped %hs %08x (data %08x) code page %08x\n"), what, pa, (UINT32)v, Mx.vpage);
	}
}
#define DBG_UNMAPPED_R(p, what, pa)		if ((uintptr_t)(p)->read == 0) DbgUnmapped(what, pa, 0)
#define DBG_UNMAPPED_W(p, what, pa, v)	if ((uintptr_t)(p)->write == 0) DbgUnmapped(what, pa, v)
#else
#define DBG_UNMAPPED_R(p, what, pa)
#define DBG_UNMAPPED_W(p, what, pa, v)
#endif

static inline UINT8 RdPhys8(UINT32 pa)
{
	Mips3PhysPage *p = &Mips3Phys[pa >> 12];
	if (IS_PTR(p->read)) return p->read[pa & 0xfff];
	DBG_UNMAPPED_R(p, "read8", pa);
	return Mips3ReadByteH[(uintptr_t)p->read](pa);
}

static inline UINT16 RdPhys16(UINT32 pa)
{
	Mips3PhysPage *p = &Mips3Phys[pa >> 12];
	if (IS_PTR(p->read)) return BURN_ENDIAN_SWAP_INT16(*(UINT16*)(p->read + (pa & 0xfff)));
	DBG_UNMAPPED_R(p, "read16", pa);
	return Mips3ReadHalfH[(uintptr_t)p->read](pa);
}

static inline UINT32 RdPhys32(UINT32 pa)
{
	Mips3PhysPage *p = &Mips3Phys[pa >> 12];
	if (IS_PTR(p->read)) return BURN_ENDIAN_SWAP_INT32(*(UINT32*)(p->read + (pa & 0xfff)));
	DBG_UNMAPPED_R(p, "read32", pa);
	return Mips3ReadWordH[(uintptr_t)p->read](pa);
}

static inline UINT64 RdPhys64(UINT32 pa)
{
	Mips3PhysPage *p = &Mips3Phys[pa >> 12];
	if (IS_PTR(p->read)) {
		UINT64 v;
		memcpy(&v, p->read + (pa & 0xfff), 8);
		return BURN_ENDIAN_SWAP_INT64(v);
	}
	DBG_UNMAPPED_R(p, "read64", pa);
	return Mips3ReadDoubleH[(uintptr_t)p->read](pa);
}

static inline void WrPhys8(UINT32 pa, UINT8 v)
{
	Mips3PhysPage *p = &Mips3Phys[pa >> 12];
	CheckCodeWrite(p, pa);
	if (IS_PTR(p->write)) { p->write[pa & 0xfff] = v; return; }
	DBG_UNMAPPED_W(p, "write8", pa, v);
	Mips3WriteByteH[(uintptr_t)p->write](pa, v);
}

static inline void WrPhys16(UINT32 pa, UINT16 v)
{
	Mips3PhysPage *p = &Mips3Phys[pa >> 12];
	CheckCodeWrite(p, pa);
	if (IS_PTR(p->write)) { *(UINT16*)(p->write + (pa & 0xfff)) = BURN_ENDIAN_SWAP_INT16(v); return; }
	DBG_UNMAPPED_W(p, "write16", pa, v);
	Mips3WriteHalfH[(uintptr_t)p->write](pa, v);
}

static inline void WrPhys32(UINT32 pa, UINT32 v)
{
	Mips3PhysPage *p = &Mips3Phys[pa >> 12];
	CheckCodeWrite(p, pa);
	if (IS_PTR(p->write)) { *(UINT32*)(p->write + (pa & 0xfff)) = BURN_ENDIAN_SWAP_INT32(v); return; }
	DBG_UNMAPPED_W(p, "write32", pa, v);
	Mips3WriteWordH[(uintptr_t)p->write](pa, v);
}

static inline void WrPhys64(UINT32 pa, UINT64 v)
{
	Mips3PhysPage *p = &Mips3Phys[pa >> 12];
	CheckCodeWrite(p, pa);
	if (IS_PTR(p->write)) {
		v = BURN_ENDIAN_SWAP_INT64(v);
		memcpy(p->write + (pa & 0xfff), &v, 8);
		return;
	}
	DBG_UNMAPPED_W(p, "write64", pa, v);
	Mips3WriteDoubleH[(uintptr_t)p->write](pa, v);
}

// ---------------------------------------------------------------------------
// exceptions

static void Mips3Exception(const Mips3Insn *I, INT32 code, INT32 refill, INT32 ce = 0)
{
	UINT32 sr = SR;
	UINT32 offset = 0x180;

	if (refill && !(sr & SR_EXL)) offset = 0;

#if MIPS3_DEBUG_TRACE
	if (code == EXC_SYS && nDbgTraceState == 0) nDbgTraceState = 1;
#endif
#if MIPS3_DEBUG_LOG
	if (code == EXC_INT) {
		if (nDbgIrq < 20) {
			nDbgIrq++;
			bprintf(PRINT_NORMAL, _T("MIPS3 irq: pc %08x cause %08x sr %08x\n"), Mx.vpage + I->pc, (UINT32)CAUSE, sr);
		}
	} else if (nDbgExc < 100) {
		nDbgExc++;
		bprintf(PRINT_NORMAL, _T("MIPS3 exception %d%s: pc %08x op %08x badvaddr %08x sr %08x%s\n"), code, refill ? " (refill)" : "",
			Mx.vpage + I->pc, I->op, (UINT32)CP0(CP0_BadVAddr), sr, (I->flags & F_DELAY) ? " delay slot" : "");
	}
#endif

	UINT32 prev_cause = (UINT32)CAUSE;
	CAUSE = (prev_cause & 0x0000ff00) | (code << 2) | ((UINT32)ce << 28);

	if (!(sr & SR_EXL)) {
		UINT32 pc = Mx.vpage + I->pc;
		if (I->flags & F_DELAY) {
			CP0(CP0_EPC) = (INT64)(INT32)(pc - 4);
			CAUSE |= 0x80000000;
		} else {
			CP0(CP0_EPC) = (INT64)(INT32)pc;
		}
		SR |= SR_EXL;
	} else {
		CAUSE |= prev_cause & 0x80000000;
	}

	Mx.next_pc = ((sr & SR_BEV) ? 0xbfc00200 : 0x80000000) + offset;
	Mx.brk = 1;
}

static void AddressError(const Mips3Insn *I, UINT32 va, INT32 store)
{
	CP0(CP0_BadVAddr) = (INT64)(INT32)va;
	Mips3Exception(I, store ? EXC_ADES : EXC_ADEL, 0);
}

static void TlbFault(const Mips3Insn *I, UINT32 va, INT32 store, UINT32 entry)
{
	CP0(CP0_BadVAddr) = (INT64)(INT32)va;
	CP0(CP0_Context) = (CP0(CP0_Context) & 0xff800000) | ((va >> 9) & 0x007ffff0);
	CP0(CP0_EntryHi) = (INT64)(INT32)((va & 0xffffe000) | ((UINT32)CP0(CP0_EntryHi) & 0xff));

	if (!(entry & VT_MATCH))
		Mips3Exception(I, store ? EXC_TLBS : EXC_TLBL, 1);
	else if (store && (entry & VT_READ))
		Mips3Exception(I, EXC_MOD, 0);
	else
		Mips3Exception(I, store ? EXC_TLBS : EXC_TLBL, 0);
}

// ---------------------------------------------------------------------------
// virtual memory access

static inline INT32 Xlat(const Mips3Insn *I, UINT32 va, UINT32 need, UINT32 *pa)
{
	UINT32 e = Vtlb[va >> 12];
	if (e & need) {
		*pa = (e & ~0xfff) | (va & 0xfff);
		return 1;
	}
	TlbFault(I, va, need == VT_WRITE, e);
	return 0;
}

static inline INT32 Load8(const Mips3Insn *I, UINT32 va, UINT32 *v)
{
	UINT32 pa;
	if (!Xlat(I, va, VT_READ, &pa)) return 0;
	*v = RdPhys8(pa);
	return 1;
}

static inline INT32 Load16(const Mips3Insn *I, UINT32 va, UINT32 *v)
{
	UINT32 pa;
	if (va & 1) { AddressError(I, va, 0); return 0; }
	if (!Xlat(I, va, VT_READ, &pa)) return 0;
	*v = RdPhys16(pa);
	return 1;
}

static inline INT32 Load32(const Mips3Insn *I, UINT32 va, UINT32 *v)
{
	UINT32 pa;
	if (va & 3) { AddressError(I, va, 0); return 0; }
	if (!Xlat(I, va, VT_READ, &pa)) return 0;
	*v = RdPhys32(pa);
	return 1;
}

static inline INT32 Load64(const Mips3Insn *I, UINT32 va, UINT64 *v)
{
	UINT32 pa;
	if (va & 7) { AddressError(I, va, 0); return 0; }
	if (!Xlat(I, va, VT_READ, &pa)) return 0;
	*v = RdPhys64(pa);
	return 1;
}

static inline INT32 Store8(const Mips3Insn *I, UINT32 va, UINT8 v)
{
	UINT32 pa;
	if (!Xlat(I, va, VT_WRITE, &pa)) return 0;
	WrPhys8(pa, v);
	return 1;
}

static inline INT32 Store16(const Mips3Insn *I, UINT32 va, UINT16 v)
{
	UINT32 pa;
	if (va & 1) { AddressError(I, va, 1); return 0; }
	if (!Xlat(I, va, VT_WRITE, &pa)) return 0;
	WrPhys16(pa, v);
	return 1;
}

static inline INT32 Store32(const Mips3Insn *I, UINT32 va, UINT32 v)
{
	UINT32 pa;
	if (va & 3) { AddressError(I, va, 1); return 0; }
	if (!Xlat(I, va, VT_WRITE, &pa)) return 0;
	WrPhys32(pa, v);
	return 1;
}

static inline INT32 Store64(const Mips3Insn *I, UINT32 va, UINT64 v)
{
	UINT32 pa;
	if (va & 7) { AddressError(I, va, 1); return 0; }
	if (!Xlat(I, va, VT_WRITE, &pa)) return 0;
	WrPhys64(pa, v);
	return 1;
}

// partial stores (SWL/SWR/SDL/SDR): only the bytes selected by mask are written
static INT32 StoreMasked(const Mips3Insn *I, UINT32 va, UINT64 data, UINT64 mask, INT32 size)
{
	UINT32 pa;
	if (!Xlat(I, va, VT_WRITE, &pa)) return 0;

	if (mask == (size == 4 ? 0xffffffffULL : ~0ULL)) {
		if (size == 4) WrPhys32(pa, (UINT32)data); else WrPhys64(pa, data);
		return 1;
	}

	for (INT32 i = 0; i < size; i++) {
		if ((mask >> (i * 8)) & 0xff) WrPhys8(pa + i, (UINT8)(data >> (i * 8)));
	}
	return 1;
}

// ---------------------------------------------------------------------------
// TLB

static void VtlbClear(UINT32 vpn, UINT32 count)
{
	for (UINT32 i = 0; i < count; i++) Vtlb[vpn + i] = 0;
}

static void TlbUnmap(INT32 i)
{
	nFastGen++;
	for (INT32 w = 0; w < 2; w++) {
		if (TlbMappedCount[i][w]) {
			VtlbClear(TlbMappedVpn[i][w], TlbMappedCount[i][w]);
			TlbMappedCount[i][w] = 0;
		}
	}
}

static void TlbMap(INT32 i)
{
	TlbUnmap(i);

	Mips3TlbEntry *e = &M.tlb[i];
	UINT32 asid = (UINT32)CP0(CP0_EntryHi) & 0xff;
	INT32 global = (e->lo[0] & e->lo[1] & 1);

	if (!global && ((UINT32)e->hi & 0xff) != asid) return;

	UINT32 vpn = ((UINT32)(e->hi >> 13) & 0x7ffff) << 1;
	UINT32 count = ((UINT32)(e->mask >> 13) & 0xfff) + 1;

	for (INT32 w = 0; w < 2; w++) {
		UINT32 effvpn = vpn + count * w;

		// never map over kseg0/kseg1 or past the end of the 32-bit space
		if (effvpn + count > VIRT_PAGES) continue;
		if (!((effvpn + count) <= 0x80000 || effvpn >= 0xc0000)) continue;

		UINT64 lo = e->lo[w];
		UINT32 pfn = (UINT32)(lo >> 6) & 0xffffff;
		UINT32 flags = VT_MATCH;
		if (lo & 2) {
			flags |= VT_READ;
			if (lo & 4) flags |= VT_WRITE;
		}

		for (UINT32 k = 0; k < count; k++) {
			Vtlb[effvpn + k] = (((pfn + k) << 12) & PHYS_MASK) | flags;
		}

		TlbMappedVpn[i][w] = effvpn;
		TlbMappedCount[i][w] = count;
	}
}

static void VtlbRebuild()
{
	nFastGen++;
	memset(Vtlb, 0, VIRT_PAGES * sizeof(UINT32));
	memset(TlbMappedCount, 0, sizeof(TlbMappedCount));

	// kseg0 and kseg1 both map the first 512MB
	for (UINT32 p = 0x80000; p < 0xc0000; p++) {
		Vtlb[p] = ((p << 12) & 0x1fffffff & PHYS_MASK) | VT_READ | VT_WRITE | VT_MATCH;
	}

	for (INT32 i = 0; i < MIPS3_TLB_ENTRIES; i++) TlbMap(i);
}

static UINT32 RandomIndex()
{
	UINT32 wired = (UINT32)CP0(CP0_Wired) & 0x3f;
	if (wired >= MIPS3_TLB_ENTRIES) return MIPS3_TLB_ENTRIES - 1;
	return ((MIPS3_TLB_ENTRIES - 1) - (UINT32)((M.total_cycles - M.random_zero) % (MIPS3_TLB_ENTRIES - wired))) & 0x3f;
}

static void TlbWrite(UINT32 idx)
{
	if (idx >= MIPS3_TLB_ENTRIES) return;

	Mips3TlbEntry *e = &M.tlb[idx];
	e->mask = CP0(CP0_PageMask);
	e->hi = CP0(CP0_EntryHi) & ~(e->mask & 0x7fffe000ULL);
	UINT64 global = CP0(CP0_EntryLo0) & CP0(CP0_EntryLo1) & 1;
	e->lo[0] = (CP0(CP0_EntryLo0) & ~1ULL) | global;
	e->lo[1] = (CP0(CP0_EntryLo1) & ~1ULL) | global;

	TlbMap(idx);
#if MIPS3_DEBUG_LOG
	if (nDbgTlb < 50) {
		nDbgTlb++;
		bprintf(PRINT_NORMAL, _T("MIPS3 tlb[%d] hi %08x lo0 %08x lo1 %08x mask %08x\n"), idx, (UINT32)e->hi, (UINT32)e->lo[0], (UINT32)e->lo[1], (UINT32)e->mask);
	}
#endif
}

// ---------------------------------------------------------------------------
// Count / Compare

static inline UINT64 CyclesNow(const Mips3Insn *I)
{
	return M.total_cycles + ((UINT32)(I->pc - Mx.blk_start) >> 2);
}

static void CompareRearm(UINT64 now)
{
	UINT32 count = (UINT32)((now - M.count_zero) / 2);
	UINT64 delta = (UINT32)((UINT32)CP0(CP0_Compare) - count);
	if (delta == 0) delta = 0x100000000ULL;
	M.compare_fire = now + delta * 2;
	nNextEvent = 0;
}

// ---------------------------------------------------------------------------
// instruction helpers

#define OP			(I->op)
#define RSn			((OP >> 21) & 31)
#define RTn			((OP >> 16) & 31)
#define RDn			((OP >> 11) & 31)
#define SA			((OP >> 6) & 31)
#define RS			M.r[RSn]
#define RT			M.r[RTn]
#define RD			M.r[RDn]
#define RS32		((UINT32)RS)
#define RT32		((UINT32)RT)
#define SIMM		((INT64)(INT16)OP)
#define UIMM		((UINT64)(UINT16)OP)
#define SX32(x)		((UINT64)(INT64)(INT32)(UINT32)(x))
#define CURPC		(Mx.vpage + I->pc)
#define EA			((UINT32)RS + (UINT32)(INT32)(INT16)OP)
#define BRTARGET	(CURPC + 4 + ((INT32)(INT16)OP << 2))

// unsigned 64x64 -> 128 multiply
static inline void Mul64(UINT64 a, UINT64 b, UINT64 *hi, UINT64 *lo)
{
#if defined(__SIZEOF_INT128__)
	unsigned __int128 p = (unsigned __int128)a * b;
	*lo = (UINT64)p;
	*hi = (UINT64)(p >> 64);
#else
	UINT64 a0 = (UINT32)a, a1 = a >> 32, b0 = (UINT32)b, b1 = b >> 32;
	UINT64 p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
	UINT64 mid = (p00 >> 32) + (UINT32)p01 + (UINT32)p10;
	*lo = (mid << 32) | (UINT32)p00;
	*hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
#endif
}

// stop after this instruction so pending interrupts / new translations are seen
static inline void EndBlockAfter(const Mips3Insn *I)
{
	if (!(I->flags & F_DELAY)) {
		Mx.next_pc = CURPC + 4;
		Mx.brk = 1;
	}
}

// ---------------------------------------------------------------------------
// SPECIAL

static void op_NOP(const Mips3Insn *) { }

static void op_SLL(const Mips3Insn *I)    { RD = SX32(RT32 << SA); }
static void op_SRL(const Mips3Insn *I)    { RD = SX32(RT32 >> SA); }
static void op_SRA(const Mips3Insn *I)    { RD = SX32((INT32)RT32 >> SA); }
static void op_SLLV(const Mips3Insn *I)   { RD = SX32(RT32 << (RS & 31)); }
static void op_SRLV(const Mips3Insn *I)   { RD = SX32(RT32 >> (RS & 31)); }
static void op_SRAV(const Mips3Insn *I)   { RD = SX32((INT32)RT32 >> (RS & 31)); }
static void op_DSLLV(const Mips3Insn *I)  { RD = RT << (RS & 63); }
static void op_DSRLV(const Mips3Insn *I)  { RD = RT >> (RS & 63); }
static void op_DSRAV(const Mips3Insn *I)  { RD = (UINT64)((INT64)RT >> (RS & 63)); }
static void op_DSLL(const Mips3Insn *I)   { RD = RT << SA; }
static void op_DSRL(const Mips3Insn *I)   { RD = RT >> SA; }
static void op_DSRA(const Mips3Insn *I)   { RD = (UINT64)((INT64)RT >> SA); }
static void op_DSLL32(const Mips3Insn *I) { RD = RT << (SA + 32); }
static void op_DSRL32(const Mips3Insn *I) { RD = RT >> (SA + 32); }
static void op_DSRA32(const Mips3Insn *I) { RD = (UINT64)((INT64)RT >> (SA + 32)); }

static void op_MOVZ(const Mips3Insn *I)   { if (RT == 0) RD = RS; }
static void op_MOVN(const Mips3Insn *I)   { if (RT != 0) RD = RS; }
static void op_MOVCI(const Mips3Insn *I)  { if (((M.fcr31 >> 23) & 1) == ((OP >> 16) & 1)) RD = RS; }

static void op_JR(const Mips3Insn *I)     { Mx.next_pc = RS32; }

static void op_JALR(const Mips3Insn *I)
{
	UINT32 target = RS32;
	if (RDn) RD = SX32(CURPC + 8);
	Mx.next_pc = target;
}

static void op_SYSCALL(const Mips3Insn *I) { Mips3Exception(I, EXC_SYS, 0); }
static void op_BREAK(const Mips3Insn *I)   { Mips3Exception(I, EXC_BP, 0); }

static void op_MFHI(const Mips3Insn *I) { RD = M.hi; }
static void op_MTHI(const Mips3Insn *I) { M.hi = RS; }
static void op_MFLO(const Mips3Insn *I) { RD = M.lo; }
static void op_MTLO(const Mips3Insn *I) { M.lo = RS; }

static void op_MULT(const Mips3Insn *I)
{
	INT64 p = (INT64)(INT32)RS32 * (INT64)(INT32)RT32;
	M.lo = SX32(p);
	M.hi = SX32((UINT64)p >> 32);
	EAT_CYCLES(3);
}

static void op_MULTU(const Mips3Insn *I)
{
	UINT64 p = (UINT64)RS32 * (UINT64)RT32;
	M.lo = SX32(p);
	M.hi = SX32(p >> 32);
	EAT_CYCLES(3);
}

static void op_DIV(const Mips3Insn *I)
{
	INT32 a = (INT32)RS32, b = (INT32)RT32;
	if (b == 0) {
		M.lo = (a >= 0) ? ~0ULL : 1;
		M.hi = SX32(a);
	} else if (a == (INT32)0x80000000 && b == -1) {
		M.lo = SX32(0x80000000);
		M.hi = 0;
	} else {
		M.lo = SX32(a / b);
		M.hi = SX32(a % b);
	}
	EAT_CYCLES(35);
}

static void op_DIVU(const Mips3Insn *I)
{
	UINT32 a = RS32, b = RT32;
	if (b == 0) {
		M.lo = ~0ULL;
		M.hi = SX32(a);
	} else {
		M.lo = SX32(a / b);
		M.hi = SX32(a % b);
	}
	EAT_CYCLES(35);
}

static void op_DMULT(const Mips3Insn *I)
{
	UINT64 hi, lo;
	Mul64(RS, RT, &hi, &lo);
	if ((INT64)RS < 0) hi -= RT;
	if ((INT64)RT < 0) hi -= RS;
	M.lo = lo;
	M.hi = hi;
	EAT_CYCLES(7);
}

static void op_DMULTU(const Mips3Insn *I)
{
	Mul64(RS, RT, &M.hi, &M.lo);
	EAT_CYCLES(7);
}

static void op_DDIV(const Mips3Insn *I)
{
	INT64 a = (INT64)RS, b = (INT64)RT;
	if (b == 0) {
		M.lo = (a >= 0) ? ~0ULL : 1;
		M.hi = (UINT64)a;
	} else if ((UINT64)a == 0x8000000000000000ULL && b == -1) {
		M.lo = (UINT64)a;
		M.hi = 0;
	} else {
		M.lo = (UINT64)(a / b);
		M.hi = (UINT64)(a % b);
	}
	EAT_CYCLES(67);
}

static void op_DDIVU(const Mips3Insn *I)
{
	UINT64 a = RS, b = RT;
	if (b == 0) {
		M.lo = ~0ULL;
		M.hi = a;
	} else {
		M.lo = a / b;
		M.hi = a % b;
	}
	EAT_CYCLES(67);
}

static void op_ADD(const Mips3Insn *I)
{
	INT64 r = (INT64)(INT32)RS32 + (INT64)(INT32)RT32;
	if (r != (INT32)r) { Mips3Exception(I, EXC_OV, 0); return; }
	if (RDn) RD = (UINT64)(INT64)(INT32)r;
}

static void op_SUB(const Mips3Insn *I)
{
	INT64 r = (INT64)(INT32)RS32 - (INT64)(INT32)RT32;
	if (r != (INT32)r) { Mips3Exception(I, EXC_OV, 0); return; }
	if (RDn) RD = (UINT64)(INT64)(INT32)r;
}

static void op_DADD(const Mips3Insn *I)
{
	UINT64 a = RS, b = RT, r = a + b;
	if (~(a ^ b) & (a ^ r) & 0x8000000000000000ULL) { Mips3Exception(I, EXC_OV, 0); return; }
	if (RDn) RD = r;
}

static void op_DSUB(const Mips3Insn *I)
{
	UINT64 a = RS, b = RT, r = a - b;
	if ((a ^ b) & (a ^ r) & 0x8000000000000000ULL) { Mips3Exception(I, EXC_OV, 0); return; }
	if (RDn) RD = r;
}

static void op_ADDU(const Mips3Insn *I)  { RD = SX32(RS32 + RT32); }
static void op_SUBU(const Mips3Insn *I)  { RD = SX32(RS32 - RT32); }
static void op_AND(const Mips3Insn *I)   { RD = RS & RT; }
static void op_OR(const Mips3Insn *I)    { RD = RS | RT; }
static void op_XOR(const Mips3Insn *I)   { RD = RS ^ RT; }
static void op_NOR(const Mips3Insn *I)   { RD = ~(RS | RT); }
static void op_SLT(const Mips3Insn *I)   { RD = (INT64)RS < (INT64)RT; }
static void op_SLTU(const Mips3Insn *I)  { RD = RS < RT; }
static void op_DADDU(const Mips3Insn *I) { RD = RS + RT; }
static void op_DSUBU(const Mips3Insn *I) { RD = RS - RT; }

static void op_TGE(const Mips3Insn *I)  { if ((INT64)RS >= (INT64)RT) Mips3Exception(I, EXC_TR, 0); }
static void op_TGEU(const Mips3Insn *I) { if (RS >= RT) Mips3Exception(I, EXC_TR, 0); }
static void op_TLT(const Mips3Insn *I)  { if ((INT64)RS < (INT64)RT) Mips3Exception(I, EXC_TR, 0); }
static void op_TLTU(const Mips3Insn *I) { if (RS < RT) Mips3Exception(I, EXC_TR, 0); }
static void op_TEQ(const Mips3Insn *I)  { if (RS == RT) Mips3Exception(I, EXC_TR, 0); }
static void op_TNE(const Mips3Insn *I)  { if (RS != RT) Mips3Exception(I, EXC_TR, 0); }

static void op_RI(const Mips3Insn *I) { Mips3Exception(I, EXC_RI, 0); }

// ---------------------------------------------------------------------------
// REGIMM / branches

static void op_BLTZ(const Mips3Insn *I)   { if ((INT64)RS < 0) Mx.next_pc = BRTARGET; }
static void op_BGEZ(const Mips3Insn *I)   { if ((INT64)RS >= 0) Mx.next_pc = BRTARGET; }
static void op_BLTZL(const Mips3Insn *I)  { if ((INT64)RS < 0) Mx.next_pc = BRTARGET; else { Mx.next_pc = CURPC + 8; Mx.brk = 1; } }
static void op_BGEZL(const Mips3Insn *I)  { if ((INT64)RS >= 0) Mx.next_pc = BRTARGET; else { Mx.next_pc = CURPC + 8; Mx.brk = 1; } }

static void op_BLTZAL(const Mips3Insn *I)
{
	INT32 taken = (INT64)RS < 0;
	M.r[31] = SX32(CURPC + 8);
	if (taken) Mx.next_pc = BRTARGET;
}

static void op_BGEZAL(const Mips3Insn *I)
{
	INT32 taken = (INT64)RS >= 0;
	M.r[31] = SX32(CURPC + 8);
	if (taken) Mx.next_pc = BRTARGET;
}

static void op_BLTZALL(const Mips3Insn *I)
{
	INT32 taken = (INT64)RS < 0;
	M.r[31] = SX32(CURPC + 8);
	if (taken) Mx.next_pc = BRTARGET; else { Mx.next_pc = CURPC + 8; Mx.brk = 1; }
}

static void op_BGEZALL(const Mips3Insn *I)
{
	INT32 taken = (INT64)RS >= 0;
	M.r[31] = SX32(CURPC + 8);
	if (taken) Mx.next_pc = BRTARGET; else { Mx.next_pc = CURPC + 8; Mx.brk = 1; }
}

static void op_TGEI(const Mips3Insn *I)  { if ((INT64)RS >= (INT64)SIMM) Mips3Exception(I, EXC_TR, 0); }
static void op_TGEIU(const Mips3Insn *I) { if (RS >= (UINT64)SIMM) Mips3Exception(I, EXC_TR, 0); }
static void op_TLTI(const Mips3Insn *I)  { if ((INT64)RS < (INT64)SIMM) Mips3Exception(I, EXC_TR, 0); }
static void op_TLTIU(const Mips3Insn *I) { if (RS < (UINT64)SIMM) Mips3Exception(I, EXC_TR, 0); }
static void op_TEQI(const Mips3Insn *I)  { if ((INT64)RS == (INT64)SIMM) Mips3Exception(I, EXC_TR, 0); }
static void op_TNEI(const Mips3Insn *I)  { if ((INT64)RS != (INT64)SIMM) Mips3Exception(I, EXC_TR, 0); }

static void op_J(const Mips3Insn *I)   { Mx.next_pc = ((CURPC + 4) & 0xf0000000) | ((OP & 0x03ffffff) << 2); }
static void op_JAL(const Mips3Insn *I) { M.r[31] = SX32(CURPC + 8); Mx.next_pc = ((CURPC + 4) & 0xf0000000) | ((OP & 0x03ffffff) << 2); }

static void op_BEQ(const Mips3Insn *I)  { if (RS == RT) Mx.next_pc = BRTARGET; }
static void op_BNE(const Mips3Insn *I)  { if (RS != RT) Mx.next_pc = BRTARGET; }
static void op_BLEZ(const Mips3Insn *I) { if ((INT64)RS <= 0) Mx.next_pc = BRTARGET; }
static void op_BGTZ(const Mips3Insn *I) { if ((INT64)RS > 0) Mx.next_pc = BRTARGET; }

// "B" is BEQ r0,r0: the most common branch, no compare needed
static void op_B(const Mips3Insn *I)    { Mx.next_pc = BRTARGET; }

static void op_BEQL(const Mips3Insn *I)  { if (RS == RT) Mx.next_pc = BRTARGET; else { Mx.next_pc = CURPC + 8; Mx.brk = 1; } }
static void op_BNEL(const Mips3Insn *I)  { if (RS != RT) Mx.next_pc = BRTARGET; else { Mx.next_pc = CURPC + 8; Mx.brk = 1; } }
static void op_BLEZL(const Mips3Insn *I) { if ((INT64)RS <= 0) Mx.next_pc = BRTARGET; else { Mx.next_pc = CURPC + 8; Mx.brk = 1; } }
static void op_BGTZL(const Mips3Insn *I) { if ((INT64)RS > 0) Mx.next_pc = BRTARGET; else { Mx.next_pc = CURPC + 8; Mx.brk = 1; } }

// ---------------------------------------------------------------------------
// immediate ALU

static void op_ADDI(const Mips3Insn *I)
{
	INT64 r = (INT64)(INT32)RS32 + SIMM;
	if (r != (INT32)r) { Mips3Exception(I, EXC_OV, 0); return; }
	if (RTn) RT = (UINT64)(INT64)(INT32)r;
}

static void op_DADDI(const Mips3Insn *I)
{
	UINT64 a = RS, b = (UINT64)SIMM, r = a + b;
	if (~(a ^ b) & (a ^ r) & 0x8000000000000000ULL) { Mips3Exception(I, EXC_OV, 0); return; }
	if (RTn) RT = r;
}

static void op_ADDIU(const Mips3Insn *I)  { RT = SX32(RS32 + (UINT32)SIMM); }
static void op_DADDIU(const Mips3Insn *I) { RT = RS + (UINT64)SIMM; }
static void op_SLTI(const Mips3Insn *I)   { RT = (INT64)RS < SIMM; }
static void op_SLTIU(const Mips3Insn *I)  { RT = RS < (UINT64)SIMM; }
static void op_ANDI(const Mips3Insn *I)   { RT = RS & UIMM; }
static void op_ORI(const Mips3Insn *I)    { RT = RS | UIMM; }
static void op_XORI(const Mips3Insn *I)   { RT = RS ^ UIMM; }
static void op_LUI(const Mips3Insn *I)    { RT = SX32(UIMM << 16); }

// ---------------------------------------------------------------------------
// loads / stores

static void op_LB(const Mips3Insn *I)  { UINT32 v; if (Load8(I, EA, &v) && RTn) RT = (UINT64)(INT64)(INT8)v; }
static void op_LBU(const Mips3Insn *I) { UINT32 v; if (Load8(I, EA, &v) && RTn) RT = (UINT8)v; }
static void op_LH(const Mips3Insn *I)  { UINT32 v; if (Load16(I, EA, &v) && RTn) RT = (UINT64)(INT64)(INT16)v; }
static void op_LHU(const Mips3Insn *I) { UINT32 v; if (Load16(I, EA, &v) && RTn) RT = (UINT16)v; }
static void op_LW(const Mips3Insn *I)  { UINT32 v; if (Load32(I, EA, &v) && RTn) RT = SX32(v); }
static void op_LWU(const Mips3Insn *I) { UINT32 v; if (Load32(I, EA, &v) && RTn) RT = v; }
static void op_LD(const Mips3Insn *I)  { UINT64 v; if (Load64(I, EA, &v) && RTn) RT = v; }

static void op_LWL(const Mips3Insn *I)
{
	UINT32 ea = EA, v;
	INT32 shift = 8 * (~ea & 3);
	UINT32 mask = 0xffffffffU << shift;
	if (Load32(I, ea & ~3, &v) && RTn) RT = SX32((RT32 & ~mask) | (v << shift));
}

static void op_LWR(const Mips3Insn *I)
{
	UINT32 ea = EA, v;
	INT32 shift = 8 * (ea & 3);
	UINT32 mask = 0xffffffffU >> shift;
	if (Load32(I, ea & ~3, &v) && RTn) RT = SX32((RT32 & ~mask) | (v >> shift));
}

static void op_LDL(const Mips3Insn *I)
{
	UINT32 ea = EA;
	UINT64 v;
	INT32 shift = 8 * (~ea & 7);
	UINT64 mask = ~0ULL << shift;
	if (Load64(I, ea & ~7, &v) && RTn) RT = (RT & ~mask) | (v << shift);
}

static void op_LDR(const Mips3Insn *I)
{
	UINT32 ea = EA;
	UINT64 v;
	INT32 shift = 8 * (ea & 7);
	UINT64 mask = ~0ULL >> shift;
	if (Load64(I, ea & ~7, &v) && RTn) RT = (RT & ~mask) | (v >> shift);
}

static void op_SB(const Mips3Insn *I) { Store8(I, EA, (UINT8)RT); }
static void op_SH(const Mips3Insn *I) { Store16(I, EA, (UINT16)RT); }
static void op_SW(const Mips3Insn *I) { Store32(I, EA, RT32); }
static void op_SD(const Mips3Insn *I) { Store64(I, EA, RT); }

static void op_SWL(const Mips3Insn *I)
{
	UINT32 ea = EA;
	INT32 shift = 8 * (~ea & 3);
	StoreMasked(I, ea & ~3, RT32 >> shift, 0xffffffffU >> shift, 4);
}

static void op_SWR(const Mips3Insn *I)
{
	UINT32 ea = EA;
	INT32 shift = 8 * (ea & 3);
	StoreMasked(I, ea & ~3, (UINT32)(RT32 << shift), (UINT32)(0xffffffffU << shift), 4);
}

static void op_SDL(const Mips3Insn *I)
{
	UINT32 ea = EA;
	INT32 shift = 8 * (~ea & 7);
	StoreMasked(I, ea & ~7, RT >> shift, ~0ULL >> shift, 8);
}

static void op_SDR(const Mips3Insn *I)
{
	UINT32 ea = EA;
	INT32 shift = 8 * (ea & 7);
	StoreMasked(I, ea & ~7, RT << shift, ~0ULL << shift, 8);
}

static void op_LL(const Mips3Insn *I)
{
	UINT32 ea = EA, v, pa;
	if (!Load32(I, ea, &v)) return;
	if (RTn) RT = SX32(v);
	pa = (Vtlb[ea >> 12] & ~0xfff) | (ea & 0xfff);
	CP0(CP0_LLAddr) = pa >> 4;
	M.llbit = 1;
}

static void op_LLD(const Mips3Insn *I)
{
	UINT32 ea = EA, pa;
	UINT64 v;
	if (!Load64(I, ea, &v)) return;
	if (RTn) RT = v;
	pa = (Vtlb[ea >> 12] & ~0xfff) | (ea & 0xfff);
	CP0(CP0_LLAddr) = pa >> 4;
	M.llbit = 1;
}

static void op_SC(const Mips3Insn *I)
{
	if (M.llbit) {
		if (!Store32(I, EA, RT32)) return;
		if (RTn) RT = 1;
	} else {
		if (RTn) RT = 0;
	}
}

static void op_SCD(const Mips3Insn *I)
{
	if (M.llbit) {
		if (!Store64(I, EA, RT)) return;
		if (RTn) RT = 1;
	} else {
		if (RTn) RT = 0;
	}
}

// ---------------------------------------------------------------------------
// COP0

static inline INT32 Cop0Usable(const Mips3Insn *I)
{
	UINT32 sr = SR;
	if ((sr & SR_KSU) && !(sr & (SR_CU0 | SR_EXL | SR_ERL))) {
		Mips3Exception(I, EXC_CPU, 0, 0);
		return 0;
	}
	return 1;
}

static UINT64 Cop0Read(const Mips3Insn *I, INT32 idx)
{
	switch (idx) {
		case CP0_Count:  return (UINT32)((CyclesNow(I) - M.count_zero) / 2);
		case CP0_Random: return RandomIndex();
	}
	return CP0(idx);
}

static void Cop0Write(const Mips3Insn *I, INT32 idx, UINT64 v)
{
	nNextEvent = 0;
	switch (idx) {
		case CP0_Cause:
			// only the two software interrupt bits are writable
			CAUSE = (CAUSE & ~0x300) | ((UINT32)v & 0x300);
			break;

		case CP0_Count: {
			UINT64 now = CyclesNow(I);
			CP0(idx) = (UINT32)v;
			M.count_zero = now - (UINT64)(UINT32)v * 2;
			CompareRearm(now);
			break;
		}

		case CP0_Compare:
			CP0(idx) = (UINT32)v;
			CAUSE &= ~0x8000;
			CompareRearm(CyclesNow(I));
			break;

		case CP0_PRId:
		case CP0_Random:
		case CP0_BadVAddr:
			break;

		case CP0_PageMask:
			CP0(idx) = v & 0x01ffe000;
			break;

		case CP0_Index:
			CP0(idx) = (CP0(idx) & 0x80000000) | (v & 0x3f);
			break;

		case CP0_Wired:
			CP0(idx) = v & 0x3f;
			M.random_zero = CyclesNow(I);
			break;

		case CP0_EntryLo0:
		case CP0_EntryLo1:
			CP0(idx) = v & 0x3fffffff;
			break;

		case CP0_Context:
		case CP0_XContext:
			CP0(idx) = v & ~0xfULL;
			break;

		case CP0_Config:
			// only K0 (kseg0 cache algorithm) is writable
			CP0(idx) = (CP0(idx) & ~7ULL) | (v & 7);
			break;

		case CP0_EntryHi: {
			UINT64 old = CP0(idx);
			CP0(idx) = v & 0xc00000ffffffe0ffULL;
			if ((old ^ CP0(idx)) & 0xff) {
				for (INT32 i = 0; i < MIPS3_TLB_ENTRIES; i++) TlbMap(i);
			}
			break;
		}

		default:
			CP0(idx) = v;
			break;
	}
}

static void op_MFC0(const Mips3Insn *I)  { if (!Cop0Usable(I)) return; UINT64 v = Cop0Read(I, RDn); if (RTn) RT = SX32(v); }
static void op_DMFC0(const Mips3Insn *I) { if (!Cop0Usable(I)) return; UINT64 v = Cop0Read(I, RDn); if (RTn) RT = v; }
static void op_MTC0(const Mips3Insn *I)
{
	if (!Cop0Usable(I)) return;
#if MIPS3_DEBUG_LOG
	if (RDn == CP0_Status && nDbgStatus < 50) {
		nDbgStatus++;
		bprintf(PRINT_NORMAL, _T("MIPS3 Status <- %08x at pc %08x\n"), RT32, CURPC);
	}
#endif
	Cop0Write(I, RDn, SX32(RT32));
	EndBlockAfter(I);
}
static void op_DMTC0(const Mips3Insn *I) { if (!Cop0Usable(I)) return; Cop0Write(I, RDn, RT); EndBlockAfter(I); }

static void op_TLBR(const Mips3Insn *I)
{
	if (!Cop0Usable(I)) return;
	UINT32 idx = (UINT32)CP0(CP0_Index) & 0x3f;
	if (idx < MIPS3_TLB_ENTRIES) {
		Mips3TlbEntry *e = &M.tlb[idx];
		CP0(CP0_PageMask) = e->mask;
		CP0(CP0_EntryHi) = e->hi;
		CP0(CP0_EntryLo0) = e->lo[0];
		CP0(CP0_EntryLo1) = e->lo[1];
	}
}

static void op_TLBWI(const Mips3Insn *I)
{
	if (!Cop0Usable(I)) return;
	TlbWrite((UINT32)CP0(CP0_Index) & 0x3f);
	EndBlockAfter(I);
}

static void op_TLBWR(const Mips3Insn *I)
{
	if (!Cop0Usable(I)) return;
	TlbWrite(RandomIndex());
	EndBlockAfter(I);
}

static void op_TLBP(const Mips3Insn *I)
{
	if (!Cop0Usable(I)) return;
	UINT64 hi = CP0(CP0_EntryHi);
	INT32 i;
	for (i = 0; i < MIPS3_TLB_ENTRIES; i++) {
		Mips3TlbEntry *e = &M.tlb[i];
		UINT64 mask = ~(((e->mask >> 13) & 0x3ffff) << 13) & 0xffffffffffffe000ULL;
		if ((e->hi & mask) == (hi & mask)) {
			if (((e->hi ^ hi) & 0xff) == 0 || (e->lo[0] & e->lo[1] & 1)) break;
		}
	}
	CP0(CP0_Index) = (i < MIPS3_TLB_ENTRIES) ? (UINT64)i : 0x80000000ULL;
}

static void op_ERET(const Mips3Insn *I)
{
	if (!Cop0Usable(I)) return;
	if (SR & SR_ERL) {
		Mx.next_pc = (UINT32)CP0(CP0_ErrorEPC);
		SR &= ~SR_ERL;
	} else {
		Mx.next_pc = (UINT32)CP0(CP0_EPC);
		SR &= ~SR_EXL;
	}
	M.llbit = 0;
	Mx.brk = 1;
	nNextEvent = 0;
#if MIPS3_DEBUG_LOG
	if (nDbgEret < 20) {
		nDbgEret++;
		bprintf(PRINT_NORMAL, _T("MIPS3 eret at pc %08x -> %08x sr %08x\n"), CURPC, Mx.next_pc, (UINT32)SR);
	}
#endif
}

static void op_CACHE(const Mips3Insn *I)
{
	if (!Cop0Usable(I)) return;
	// stores already invalidate stale code, nothing to do for the instruction cache
}

static void op_COP2(const Mips3Insn *I)
{
	if (!(SR & SR_CU2)) Mips3Exception(I, EXC_CPU, 0, 2);
}

static void op_COP3(const Mips3Insn *I)
{
	Mips3Exception(I, EXC_CPU, 0, 3);
}

// ---------------------------------------------------------------------------
// COP1 (FPU)

#define FS_N	((OP >> 11) & 31)
#define FT_N	((OP >> 16) & 31)
#define FD_N	((OP >> 6) & 31)

#define FCR31_C		0x00800000

#define COP1_CHECK	if (!(SR & SR_CU1)) { Mips3Exception(I, EXC_CPU, 0, 1); return; }

// FR=0: 16 even 64-bit registers, odd singles are the upper half of the even register
static inline UINT32 Fpr32(INT32 n)
{
	if (SR & SR_FR) return (UINT32)M.fpr[n];
	return (n & 1) ? (UINT32)(M.fpr[n & ~1] >> 32) : (UINT32)M.fpr[n];
}

static inline void SetFpr32(INT32 n, UINT32 v)
{
	if (SR & SR_FR) { M.fpr[n] = v; return; }
	if (n & 1) M.fpr[n & ~1] = (M.fpr[n & ~1] & 0x00000000ffffffffULL) | ((UINT64)v << 32);
	else       M.fpr[n] = (M.fpr[n] & 0xffffffff00000000ULL) | v;
}

static inline UINT64 Fpr64(INT32 n)
{
	if (!(SR & SR_FR)) n &= ~1;
	return M.fpr[n];
}

static inline void SetFpr64(INT32 n, UINT64 v)
{
	if (!(SR & SR_FR)) n &= ~1;
	M.fpr[n] = v;
}

static inline float GetS(INT32 n)            { UINT32 v = Fpr32(n); float f; memcpy(&f, &v, 4); return f; }
static inline void SetS(INT32 n, float f)    { UINT32 v; memcpy(&v, &f, 4); SetFpr32(n, v); }
static inline double GetD(INT32 n)           { UINT64 v = Fpr64(n); double d; memcpy(&d, &v, 8); return d; }
static inline void SetD(INT32 n, double d)   { UINT64 v; memcpy(&v, &d, 8); SetFpr64(n, v); }

static inline double RoundMode(double v, INT32 rm)
{
	switch (rm & 3) {
		case 0: {
			// nearest, ties to even
			double f = floor(v);
			double d = v - f;
			if (d > 0.5 || (d == 0.5 && fmod(f, 2.0) != 0.0)) f += 1.0;
			return f;
		}
		case 1: return (v < 0.0) ? ceil(v) : floor(v);
		case 2: return ceil(v);
	}
	return floor(v);
}

static inline UINT32 ToW(double v, INT32 rm)
{
	v = RoundMode(v, rm);
	if (!(v >= -2147483648.0 && v <= 2147483647.0)) return 0x7fffffff;
	return (UINT32)(INT32)v;
}

static inline UINT64 ToL(double v, INT32 rm)
{
	v = RoundMode(v, rm);
	if (!(v >= -9223372036854775808.0 && v < 9223372036854775808.0)) return 0x7fffffffffffffffULL;
	return (UINT64)(INT64)v;
}

static void op_MFC1(const Mips3Insn *I)  { COP1_CHECK; if (RTn) RT = SX32(Fpr32(FS_N)); }
static void op_DMFC1(const Mips3Insn *I) { COP1_CHECK; if (RTn) RT = Fpr64(FS_N); }
static void op_MTC1(const Mips3Insn *I)  { COP1_CHECK; SetFpr32(FS_N, RT32); }
static void op_DMTC1(const Mips3Insn *I) { COP1_CHECK; SetFpr64(FS_N, RT); }

static void op_CFC1(const Mips3Insn *I)
{
	COP1_CHECK;
	UINT32 v = 0;
	if (FS_N == 0) v = 0x2020;
	else if (FS_N == 31) v = M.fcr31;
	if (RTn) RT = SX32(v);
}

static void op_CTC1(const Mips3Insn *I)
{
	COP1_CHECK;
	if (FS_N == 31) M.fcr31 = RT32;
}

static void op_BC1F(const Mips3Insn *I)  { COP1_CHECK; if (!(M.fcr31 & FCR31_C)) Mx.next_pc = BRTARGET; }
static void op_BC1T(const Mips3Insn *I)  { COP1_CHECK; if (M.fcr31 & FCR31_C) Mx.next_pc = BRTARGET; }
static void op_BC1FL(const Mips3Insn *I) { COP1_CHECK; if (!(M.fcr31 & FCR31_C)) Mx.next_pc = BRTARGET; else { Mx.next_pc = CURPC + 8; Mx.brk = 1; } }
static void op_BC1TL(const Mips3Insn *I) { COP1_CHECK; if (M.fcr31 & FCR31_C) Mx.next_pc = BRTARGET; else { Mx.next_pc = CURPC + 8; Mx.brk = 1; } }

static void op_LWC1(const Mips3Insn *I) { COP1_CHECK; UINT32 v; if (Load32(I, EA, &v)) SetFpr32(FT_N, v); }
static void op_LDC1(const Mips3Insn *I) { COP1_CHECK; UINT64 v; if (Load64(I, EA, &v)) SetFpr64(FT_N, v); }
static void op_SWC1(const Mips3Insn *I) { COP1_CHECK; Store32(I, EA, Fpr32(FT_N)); }
static void op_SDC1(const Mips3Insn *I) { COP1_CHECK; Store64(I, EA, Fpr64(FT_N)); }

// MIPS legacy NaN encoding: the mantissa MSB set means *signaling*, the default NaN is 0x7fbfffff / 0x7ff7ffffffffffff.
// A NaN result propagates a quiet NaN operand, anything else (signaling operand, invalid operation) gives the default NaN.
static inline INT32 IsNaN32(UINT32 v)  { return (v & 0x7fffffff) > 0x7f800000; }
static inline INT32 IsSNaN32(UINT32 v) { return IsNaN32(v) && (v & 0x00400000); }
static inline INT32 IsNaN64(UINT64 v)  { return (v & 0x7fffffffffffffffULL) > 0x7ff0000000000000ULL; }
static inline INT32 IsSNaN64(UINT64 v) { return IsNaN64(v) && (v & 0x0008000000000000ULL); }

static UINT32 NaN32(UINT32 a, UINT32 b, INT32 binary)
{
	if (IsSNaN32(a) || (binary && IsSNaN32(b))) return 0x7fbfffff;
	if (IsNaN32(a)) return a;
	if (binary && IsNaN32(b)) return b;
	return 0x7fbfffff;
}

static UINT64 NaN64(UINT64 a, UINT64 b, INT32 binary)
{
	if (IsSNaN64(a) || (binary && IsSNaN64(b))) return 0x7ff7ffffffffffffULL;
	if (IsNaN64(a)) return a;
	if (binary && IsNaN64(b)) return b;
	return 0x7ff7ffffffffffffULL;
}

static inline float U2F(UINT32 v)  { float f; memcpy(&f, &v, 4); return f; }
static inline double U2D(UINT64 v) { double d; memcpy(&d, &v, 8); return d; }

#define FP_OP_S(name, expr, binary) static void name(const Mips3Insn *I) { \
	COP1_CHECK; \
	UINT32 ra = Fpr32(FS_N), rb = binary ? Fpr32(FT_N) : 0; \
	float a = U2F(ra), b = U2F(rb); (void)b; \
	float r = (expr); \
	if (r != r) SetFpr32(FD_N, NaN32(ra, rb, binary)); else SetS(FD_N, r); }

#define FP_OP_D(name, expr, binary) static void name(const Mips3Insn *I) { \
	COP1_CHECK; \
	UINT64 ra = Fpr64(FS_N), rb = binary ? Fpr64(FT_N) : 0; \
	double a = U2D(ra), b = U2D(rb); (void)b; \
	double r = (expr); \
	if (r != r) SetFpr64(FD_N, NaN64(ra, rb, binary)); else SetD(FD_N, r); }

FP_OP_S(op_ADD_S, a + b, 1)
FP_OP_S(op_SUB_S, a - b, 1)
FP_OP_S(op_MUL_S, a * b, 1)
FP_OP_S(op_DIV_S, a / b, 1)
FP_OP_S(op_SQRT_S, sqrtf(a), 0)
FP_OP_D(op_ADD_D, a + b, 1)
FP_OP_D(op_SUB_D, a - b, 1)
FP_OP_D(op_MUL_D, a * b, 1)
FP_OP_D(op_DIV_D, a / b, 1)
FP_OP_D(op_SQRT_D, sqrt(a), 0)

// abs/neg/mov only touch the sign bit, so NaN payloads are kept
static void op_ABS_S(const Mips3Insn *I) { COP1_CHECK; SetFpr32(FD_N, Fpr32(FS_N) & 0x7fffffff); }
static void op_NEG_S(const Mips3Insn *I) { COP1_CHECK; SetFpr32(FD_N, Fpr32(FS_N) ^ 0x80000000); }
static void op_MOV_S(const Mips3Insn *I) { COP1_CHECK; SetFpr32(FD_N, Fpr32(FS_N)); }
static void op_ABS_D(const Mips3Insn *I) { COP1_CHECK; SetFpr64(FD_N, Fpr64(FS_N) & 0x7fffffffffffffffULL); }
static void op_NEG_D(const Mips3Insn *I) { COP1_CHECK; SetFpr64(FD_N, Fpr64(FS_N) ^ 0x8000000000000000ULL); }
static void op_MOV_D(const Mips3Insn *I) { COP1_CHECK; SetFpr64(FD_N, Fpr64(FS_N)); }

#define FP_TOINT(name, get, conv, set, rm) static void name(const Mips3Insn *I) { COP1_CHECK; set(FD_N, conv((double)get(FS_N), rm)); }

FP_TOINT(op_ROUND_L_S, GetS, ToL, SetFpr64, 0)
FP_TOINT(op_TRUNC_L_S, GetS, ToL, SetFpr64, 1)
FP_TOINT(op_CEIL_L_S,  GetS, ToL, SetFpr64, 2)
FP_TOINT(op_FLOOR_L_S, GetS, ToL, SetFpr64, 3)
FP_TOINT(op_ROUND_W_S, GetS, ToW, SetFpr32, 0)
FP_TOINT(op_TRUNC_W_S, GetS, ToW, SetFpr32, 1)
FP_TOINT(op_CEIL_W_S,  GetS, ToW, SetFpr32, 2)
FP_TOINT(op_FLOOR_W_S, GetS, ToW, SetFpr32, 3)
FP_TOINT(op_CVT_W_S,   GetS, ToW, SetFpr32, M.fcr31 & 3)
FP_TOINT(op_CVT_L_S,   GetS, ToL, SetFpr64, M.fcr31 & 3)
FP_TOINT(op_ROUND_L_D, GetD, ToL, SetFpr64, 0)
FP_TOINT(op_TRUNC_L_D, GetD, ToL, SetFpr64, 1)
FP_TOINT(op_CEIL_L_D,  GetD, ToL, SetFpr64, 2)
FP_TOINT(op_FLOOR_L_D, GetD, ToL, SetFpr64, 3)
FP_TOINT(op_ROUND_W_D, GetD, ToW, SetFpr32, 0)
FP_TOINT(op_TRUNC_W_D, GetD, ToW, SetFpr32, 1)
FP_TOINT(op_CEIL_W_D,  GetD, ToW, SetFpr32, 2)
FP_TOINT(op_FLOOR_W_D, GetD, ToW, SetFpr32, 3)
FP_TOINT(op_CVT_W_D,   GetD, ToW, SetFpr32, M.fcr31 & 3)
FP_TOINT(op_CVT_L_D,   GetD, ToL, SetFpr64, M.fcr31 & 3)

static void op_CVT_D_S(const Mips3Insn *I)
{
	COP1_CHECK;
	UINT32 v = Fpr32(FS_N);
	if (IsNaN32(v)) {
		if (IsSNaN32(v)) SetFpr64(FD_N, 0x7ff7ffffffffffffULL);
		else SetFpr64(FD_N, ((UINT64)(v & 0x80000000) << 32) | 0x7ff0000000000000ULL | ((UINT64)(v & 0x007fffff) << 29));
		return;
	}
	SetD(FD_N, (double)U2F(v));
}

static void op_CVT_S_D(const Mips3Insn *I)
{
	COP1_CHECK;
	UINT64 v = Fpr64(FS_N);
	if (IsNaN64(v)) {
		UINT32 m = (UINT32)((v >> 29) & 0x007fffff);
		if (IsSNaN64(v) || m == 0) SetFpr32(FD_N, 0x7fbfffff);
		else SetFpr32(FD_N, (UINT32)((v >> 32) & 0x80000000) | 0x7f800000 | m);
		return;
	}
	SetS(FD_N, (float)U2D(v));
}
static void op_CVT_S_W(const Mips3Insn *I) { COP1_CHECK; SetS(FD_N, (float)(INT32)Fpr32(FS_N)); }
static void op_CVT_D_W(const Mips3Insn *I) { COP1_CHECK; SetD(FD_N, (double)(INT32)Fpr32(FS_N)); }
static void op_CVT_S_L(const Mips3Insn *I) { COP1_CHECK; SetS(FD_N, (float)(INT64)Fpr64(FS_N)); }
static void op_CVT_D_L(const Mips3Insn *I) { COP1_CHECK; SetD(FD_N, (double)(INT64)Fpr64(FS_N)); }

static inline void FpCompare(UINT32 cond, double a, double b)
{
	INT32 r;
	if (a != a || b != b) {
		r = cond & 1;
	} else {
		r = ((cond & 2) && a == b) || ((cond & 4) && a < b);
	}
	if (r) M.fcr31 |= FCR31_C; else M.fcr31 &= ~FCR31_C;
}

static void op_C_S(const Mips3Insn *I) { COP1_CHECK; FpCompare(OP & 15, GetS(FS_N), GetS(FT_N)); }
static void op_C_D(const Mips3Insn *I) { COP1_CHECK; FpCompare(OP & 15, GetD(FS_N), GetD(FT_N)); }

// unimplemented FPU operation: real hardware raises an FPU exception, games are not expected to rely on it
static void op_COP1_NOP(const Mips3Insn *I) { COP1_CHECK; }

// ---------------------------------------------------------------------------
// branch inside a delay slot, kept inside the block as  A, PRE, B, POST, C
// (C is B's delay slot when A is not taken); the pseudo ops do not count as cycles

#if MIPS3_CACHED_INTERPRETER && !MIPS3_DEBUG_TRACE
static void op_DelayBranchPre(const Mips3Insn *)
{
	EAT_CYCLES(-1);
	Mx.anext = Mx.next_pc;
	Mx.next_pc = 1;
}

static void op_DelayBranchPost(const Mips3Insn *I)
{
	EAT_CYCLES(-1);
	UINT32 seq = CURPC + 8, a = Mx.anext, bnext = Mx.next_pc;

	if (a & 3) {
		// A not taken: B is an ordinary branch and C its delay slot
		Mx.next_pc = (bnext == 1) ? seq : bnext;
	} else if (bnext == 1) {
		Mx.next_pc = a;
		Mx.brk = 1;
	} else {
		// both taken: A's target runs as B's delay slot
		Mx.dpend = 1;
		Mx.dtarget = bnext;
		Mx.next_pc = a;
		Mx.brk = 1;
	}
}
#endif

// ---------------------------------------------------------------------------
// decoder

static Mips3Handler DecodeCop1(UINT32 op, UINT32 *flags)
{
	UINT32 fmt = (op >> 21) & 31;
	UINT32 fn = op & 0x3f;

	switch (fmt) {
		case 0x00: return op_MFC1;
		case 0x01: return op_DMFC1;
		case 0x02: return op_CFC1;
		case 0x04: return op_MTC1;
		case 0x05: return op_DMTC1;
		case 0x06: return op_CTC1;
		case 0x08:
			*flags |= F_BRANCH;
			switch ((op >> 16) & 3) {
				case 0: return op_BC1F;
				case 1: return op_BC1T;
				case 2: return op_BC1FL;
				default: return op_BC1TL;
			}

		case 0x10:
			if (fn >= 0x30) return op_C_S;
			switch (fn) {
				case 0x00: return op_ADD_S;
				case 0x01: return op_SUB_S;
				case 0x02: return op_MUL_S;
				case 0x03: return op_DIV_S;
				case 0x04: return op_SQRT_S;
				case 0x05: return op_ABS_S;
				case 0x06: return op_MOV_S;
				case 0x07: return op_NEG_S;
				case 0x08: return op_ROUND_L_S;
				case 0x09: return op_TRUNC_L_S;
				case 0x0a: return op_CEIL_L_S;
				case 0x0b: return op_FLOOR_L_S;
				case 0x0c: return op_ROUND_W_S;
				case 0x0d: return op_TRUNC_W_S;
				case 0x0e: return op_CEIL_W_S;
				case 0x0f: return op_FLOOR_W_S;
				case 0x21: return op_CVT_D_S;
				case 0x24: return op_CVT_W_S;
				case 0x25: return op_CVT_L_S;
			}
			return op_COP1_NOP;

		case 0x11:
			if (fn >= 0x30) return op_C_D;
			switch (fn) {
				case 0x00: return op_ADD_D;
				case 0x01: return op_SUB_D;
				case 0x02: return op_MUL_D;
				case 0x03: return op_DIV_D;
				case 0x04: return op_SQRT_D;
				case 0x05: return op_ABS_D;
				case 0x06: return op_MOV_D;
				case 0x07: return op_NEG_D;
				case 0x08: return op_ROUND_L_D;
				case 0x09: return op_TRUNC_L_D;
				case 0x0a: return op_CEIL_L_D;
				case 0x0b: return op_FLOOR_L_D;
				case 0x0c: return op_ROUND_W_D;
				case 0x0d: return op_TRUNC_W_D;
				case 0x0e: return op_CEIL_W_D;
				case 0x0f: return op_FLOOR_W_D;
				case 0x20: return op_CVT_S_D;
				case 0x24: return op_CVT_W_D;
				case 0x25: return op_CVT_L_D;
			}
			return op_COP1_NOP;

		case 0x14:
			if (fn == 0x20) return op_CVT_S_W;
			if (fn == 0x21) return op_CVT_D_W;
			return op_COP1_NOP;

		case 0x15:
			if (fn == 0x20) return op_CVT_S_L;
			if (fn == 0x21) return op_CVT_D_L;
			return op_COP1_NOP;
	}

	return op_COP1_NOP;
}

static Mips3Handler DecodeCop0(UINT32 op, UINT32 *flags)
{
	switch ((op >> 21) & 31) {
		case 0x00: return op_MFC0;
		case 0x01: return op_DMFC0;
		case 0x04: *flags |= F_END; return op_MTC0;
		case 0x05: *flags |= F_END; return op_DMTC0;
	}

	if (op & 0x02000000) {
		switch (op & 0x3f) {
			case 0x01: return op_TLBR;
			case 0x02: *flags |= F_END; return op_TLBWI;
			case 0x06: *flags |= F_END; return op_TLBWR;
			case 0x08: return op_TLBP;
			case 0x18: *flags |= F_END; return op_ERET;
		}
		return op_NOP;
	}

	return op_RI;
}

static Mips3Handler Mips3Decode(UINT32 op, UINT32 *flags)
{
	UINT32 rt = (op >> 16) & 31;
	UINT32 rd = (op >> 11) & 31;

	*flags = 0;

	switch (op >> 26) {
		case 0x00: {
			// writes to r0 without side effects are dropped here, so these handlers never check rd
			Mips3Handler h = NULL;
			switch (op & 0x3f) {
				case 0x00: h = op_SLL; break;
				case 0x01: h = op_MOVCI; break;
				case 0x02: h = op_SRL; break;
				case 0x03: h = op_SRA; break;
				case 0x04: h = op_SLLV; break;
				case 0x06: h = op_SRLV; break;
				case 0x07: h = op_SRAV; break;
				case 0x0a: h = op_MOVZ; break;
				case 0x0b: h = op_MOVN; break;
				case 0x10: h = op_MFHI; break;
				case 0x12: h = op_MFLO; break;
				case 0x14: h = op_DSLLV; break;
				case 0x16: h = op_DSRLV; break;
				case 0x17: h = op_DSRAV; break;
				case 0x21: h = op_ADDU; break;
				case 0x23: h = op_SUBU; break;
				case 0x24: h = op_AND; break;
				case 0x25: h = op_OR; break;
				case 0x26: h = op_XOR; break;
				case 0x27: h = op_NOR; break;
				case 0x2a: h = op_SLT; break;
				case 0x2b: h = op_SLTU; break;
				case 0x2d: h = op_DADDU; break;
				case 0x2f: h = op_DSUBU; break;
				case 0x38: h = op_DSLL; break;
				case 0x3a: h = op_DSRL; break;
				case 0x3b: h = op_DSRA; break;
				case 0x3c: h = op_DSLL32; break;
				case 0x3e: h = op_DSRL32; break;
				case 0x3f: h = op_DSRA32; break;
			}
			if (h) return rd ? h : op_NOP;

			switch (op & 0x3f) {
				case 0x08: *flags |= F_BRANCH; return op_JR;
				case 0x09: *flags |= F_BRANCH; return op_JALR;
				case 0x0c: *flags |= F_END; return op_SYSCALL;
				case 0x0d: *flags |= F_END; return op_BREAK;
				case 0x0f: return op_NOP;	// SYNC
				case 0x11: return op_MTHI;
				case 0x13: return op_MTLO;
				case 0x18: return op_MULT;
				case 0x19: return op_MULTU;
				case 0x1a: return op_DIV;
				case 0x1b: return op_DIVU;
				case 0x1c: return op_DMULT;
				case 0x1d: return op_DMULTU;
				case 0x1e: return op_DDIV;
				case 0x1f: return op_DDIVU;
				case 0x20: return op_ADD;
				case 0x22: return op_SUB;
				case 0x2c: return op_DADD;
				case 0x2e: return op_DSUB;
				case 0x30: return op_TGE;
				case 0x31: return op_TGEU;
				case 0x32: return op_TLT;
				case 0x33: return op_TLTU;
				case 0x34: return op_TEQ;
				case 0x36: return op_TNE;
			}
			return op_RI;
		}

		case 0x01:
			switch (rt) {
				case 0x00: *flags |= F_BRANCH; return op_BLTZ;
				case 0x01: *flags |= F_BRANCH; return op_BGEZ;
				case 0x02: *flags |= F_BRANCH; return op_BLTZL;
				case 0x03: *flags |= F_BRANCH; return op_BGEZL;
				case 0x08: return op_TGEI;
				case 0x09: return op_TGEIU;
				case 0x0a: return op_TLTI;
				case 0x0b: return op_TLTIU;
				case 0x0c: return op_TEQI;
				case 0x0e: return op_TNEI;
				case 0x10: *flags |= F_BRANCH; return op_BLTZAL;
				case 0x11: *flags |= F_BRANCH; return op_BGEZAL;
				case 0x12: *flags |= F_BRANCH; return op_BLTZALL;
				case 0x13: *flags |= F_BRANCH; return op_BGEZALL;
			}
			return op_RI;

		case 0x02: *flags |= F_BRANCH; return op_J;
		case 0x03: *flags |= F_BRANCH; return op_JAL;
		case 0x04: *flags |= F_BRANCH; return (((op >> 16) & 0x3ff) == 0) ? op_B : op_BEQ;
		case 0x05: *flags |= F_BRANCH; return op_BNE;
		case 0x06: *flags |= F_BRANCH; return op_BLEZ;
		case 0x07: *flags |= F_BRANCH; return op_BGTZ;

		case 0x08: return op_ADDI;
		case 0x09: return rt ? op_ADDIU : op_NOP;
		case 0x0a: return rt ? op_SLTI : op_NOP;
		case 0x0b: return rt ? op_SLTIU : op_NOP;
		case 0x0c: return rt ? op_ANDI : op_NOP;
		case 0x0d: return rt ? op_ORI : op_NOP;
		case 0x0e: return rt ? op_XORI : op_NOP;
		case 0x0f: return rt ? op_LUI : op_NOP;

		case 0x10: return DecodeCop0(op, flags);
		case 0x11: return DecodeCop1(op, flags);
		case 0x12: return op_COP2;
		case 0x13: return op_COP3;	// COP1X on MIPS IV

		case 0x14: *flags |= F_BRANCH; return op_BEQL;
		case 0x15: *flags |= F_BRANCH; return op_BNEL;
		case 0x16: *flags |= F_BRANCH; return op_BLEZL;
		case 0x17: *flags |= F_BRANCH; return op_BGTZL;

		case 0x18: return op_DADDI;
		case 0x19: return rt ? op_DADDIU : op_NOP;
		case 0x1a: return op_LDL;
		case 0x1b: return op_LDR;

		case 0x20: return op_LB;
		case 0x21: return op_LH;
		case 0x22: return op_LWL;
		case 0x23: return op_LW;
		case 0x24: return op_LBU;
		case 0x25: return op_LHU;
		case 0x26: return op_LWR;
		case 0x27: return op_LWU;
		case 0x28: return op_SB;
		case 0x29: return op_SH;
		case 0x2a: return op_SWL;
		case 0x2b: return op_SW;
		case 0x2c: return op_SDL;
		case 0x2d: return op_SDR;
		case 0x2e: return op_SWR;
		case 0x2f: return op_CACHE;
		case 0x30: return op_LL;
		case 0x31: return op_LWC1;
		case 0x32: return op_COP2;	// LWC2
		case 0x33: return op_NOP;	// PREF (MIPS IV)
		case 0x34: return op_LLD;
		case 0x35: return op_LDC1;
		case 0x36: return op_COP2;	// LDC2
		case 0x37: return op_LD;
		case 0x38: return op_SC;
		case 0x39: return op_SWC1;
		case 0x3a: return op_COP2;	// SWC2
		case 0x3c: return op_SCD;
		case 0x3d: return op_SDC1;
		case 0x3e: return op_COP2;	// SDC2
		case 0x3f: return op_SD;
	}

	return op_RI;
}

// ---------------------------------------------------------------------------
// block cache

static Mips3BlockPage *GetBlockPage(UINT8 *host)
{
	std::map<UINT8*, Mips3BlockPage*>::iterator it = BlockPageMap->find(host);
	if (it != BlockPageMap->end()) return it->second;

	Mips3BlockPage *bp = (Mips3BlockPage*)calloc(1, sizeof(Mips3BlockPage));
	(*BlockPageMap)[host] = bp;
	BlockPageList->push_back(bp);
	return bp;
}

static void Mips3FlushCache()
{
	if (BlockPageList == NULL) return;

	for (size_t i = 0; i < BlockPageList->size(); i++) {
		Mips3BlockPage *bp = (*BlockPageList)[i];
		if (bp->blk) memset(bp->blk, 0, 1024 * sizeof(Mips3Block*));
		bp->codemask = 0;
	}

	ArenaUsed = 0;
	nFastGen++;
}

#if MIPS3_CACHED_INTERPRETER && !MIPS3_DEBUG_TRACE
#define IDLE_STABLE		1	// spins with an unchanged state: skip the rest of the slice
#define IDLE_TIMED		2	// depends on Count only: binary search the first exiting iteration

// instructions allowed in an idle loop (no stores, no hi/lo or coprocessor side effects),
// with the registers they read and write
static INT32 IdleSafe(UINT32 op, UINT32 *rmask, UINT32 *wmask, INT32 *timed)
{
	UINT32 rs = (op >> 21) & 31, rt = (op >> 16) & 31, rd = (op >> 11) & 31;
	*rmask = *wmask = 0;

	switch (op >> 26) {
		case 0x00:
			switch (op & 0x3f) {
				case 0x00: case 0x02: case 0x03: case 0x38: case 0x3a: case 0x3b: case 0x3c: case 0x3e: case 0x3f:
					*rmask = 1 << rt; *wmask = 1 << rd; return 1;
				case 0x04: case 0x06: case 0x07: case 0x0a: case 0x0b: case 0x14: case 0x16: case 0x17:
				case 0x21: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27: case 0x2a: case 0x2b: case 0x2d: case 0x2f:
					*rmask = (1 << rs) | (1 << rt); *wmask = 1 << rd;
					if ((op & 0x3f) == 0x0a || (op & 0x3f) == 0x0b) *rmask |= 1 << rd;
					return 1;
				case 0x10: case 0x12:
					*wmask = 1 << rd; return 1;
				case 0x0f:
					return 1;
			}
			return 0;

		case 0x01:
			*rmask = 1 << rs;
			if (rt <= 3) return 1;
			if (rt >= 0x10 && rt <= 0x13) { *wmask = 1U << 31; return 1; }
			return 0;

		case 0x04: case 0x05: case 0x14: case 0x15:
			*rmask = (1 << rs) | (1 << rt); return 1;
		case 0x06: case 0x07: case 0x16: case 0x17:
			*rmask = 1 << rs; return 1;
		case 0x33:
			return 1;

		case 0x0f:
			*wmask = 1 << rt; return 1;
		case 0x08: case 0x09: case 0x0a: case 0x0b: case 0x0c: case 0x0d: case 0x0e: case 0x18: case 0x19:
		case 0x20: case 0x21: case 0x23: case 0x24: case 0x25: case 0x27: case 0x37:
			*rmask = 1 << rs; *wmask = 1 << rt; return 1;
		case 0x1a: case 0x1b: case 0x22: case 0x26:
			*rmask = (1 << rs) | (1 << rt); *wmask = 1 << rt; return 1;

		case 0x10:
			// MFC0/DMFC0: Cause/Status only change between slices, Count/Random follow time
			if (rs == 0 || rs == 1) {
				if (rd == CP0_Count || rd == CP0_Random) *timed = 1;
				*wmask = 1 << rt;
				return 1;
			}
			return 0;
	}
	return 0;
}

static void IdleAnalyse(Mips3Block *b, const Mips3Insn *tmp, INT32 n)
{
	UINT32 rmask[MIPS3_BLOCK_MAX + 4], wmask[MIPS3_BLOCK_MAX + 4];
	UINT32 written = 0, indmask = 0;
	INT32 timed = 0;

	for (INT32 i = 0; i < n; i++) {
		if (!IdleSafe(tmp[i].op, &rmask[i], &wmask[i], &timed)) return;
		rmask[i] &= ~1U;
		wmask[i] &= ~1U;
		written |= wmask[i];
	}

	// induction counters
	INT32 nind = 0;
	for (INT32 i = 0; i < n; i++) {
		UINT32 op = tmp[i].op, opc = op >> 26, rs = (op >> 21) & 31, rt = (op >> 16) & 31;
		if ((opc != 0x09 && opc != 0x19) || rs != rt || rt == 0) continue;
		INT32 ok = 1;
		for (INT32 j = 0; j < n && ok; j++) {
			if (j == i) continue;
			if ((rmask[j] | wmask[j]) & (1 << rt)) ok = 0;
		}
		if (!ok || nind >= 4) continue;
		b->indreg[nind] = rt;
		b->ind64[nind] = (opc == 0x19);
		b->indimm[nind] = (INT16)op;
		nind++;
		indmask |= 1 << rt;
	}

	// a register read before being written in the iteration carries a value from the previous one
	INT32 carried = 0;
	UINT32 seen = 0;
	for (INT32 i = 0; i < n; i++) {
		if (rmask[i] & written & ~seen & ~indmask) carried = 1;
		seen |= wmask[i];
	}

	if (timed && carried) return;

	b->idle = timed ? IDLE_TIMED : IDLE_STABLE;
	b->wmask = written & ~indmask;
	b->nind = nind;
}

static Mips3Block *BuildBlock(Mips3BlockPage *bp, UINT8 *host, UINT32 off)
{
	Mips3Insn tmp[MIPS3_BLOCK_MAX + 4];
	INT32 n = 0, split = 0;
	UINT32 o = off, fall, fl;

	for (;;) {
		UINT32 op = BURN_ENDIAN_SWAP_INT32(*(UINT32*)(host + o));
		tmp[n].fn = Mips3Decode(op, &fl);
		tmp[n].op = op;
		tmp[n].pc = o;
		tmp[n].flags = 0;
		n++;

		if (fl & F_BRANCH) {
			UINT32 fl2 = F_BRANCH;
			Mips3Handler h2 = NULL;
			UINT32 op2 = 0;
			if (o + 4 < 0x1000) {
				op2 = BURN_ENDIAN_SWAP_INT32(*(UINT32*)(host + o + 4));
				h2 = Mips3Decode(op2, &fl2);
			}
			UINT32 fl3 = F_BRANCH;
			Mips3Handler h3 = NULL;
			UINT32 op3 = 0;
			if ((fl2 & F_BRANCH) && h2 && o + 8 < 0x1000) {
				op3 = BURN_ENDIAN_SWAP_INT32(*(UINT32*)(host + o + 8));
				h3 = Mips3Decode(op3, &fl3);
			}

			if ((fl2 & F_BRANCH) && !(fl3 & F_BRANCH)) {
				tmp[n].fn = op_DelayBranchPre; tmp[n].op = 0; tmp[n].pc = o + 4; tmp[n].flags = F_DELAY; n++;
				tmp[n].fn = h2; tmp[n].op = op2; tmp[n].pc = o + 4; tmp[n].flags = F_DELAY; n++;
				tmp[n].fn = op_DelayBranchPost; tmp[n].op = 0; tmp[n].pc = o + 4; tmp[n].flags = F_DELAY; n++;
				tmp[n].fn = h3; tmp[n].op = op3; tmp[n].pc = o + 8; tmp[n].flags = F_DELAY; n++;
				fall = 0xffff;	// odd: tells POST that A was not taken
				break;
			}

			// delay slot in the next page, or a branch inside the delay slot: finish it uncached
			if (fl2 & F_BRANCH) {
				split = 1;
			} else {
				tmp[n].fn = h2;
				tmp[n].op = op2;
				tmp[n].pc = o + 4;
				tmp[n].flags = F_DELAY;
				n++;
			}
			fall = o + 8;
			break;
		}

		if (fl & F_END) { fall = o + 4; break; }

		o += 4;
		if (o >= 0x1000 || n >= MIPS3_BLOCK_MAX) { fall = o; break; }
	}

	UINT32 size = (sizeof(Mips3Block) - sizeof(Mips3Insn) + n * sizeof(Mips3Insn) + 15) & ~15;
	if (ArenaUsed + size > MIPS3_ARENA_SIZE) Mips3FlushCache();

	Mips3Block *b = (Mips3Block*)(Arena + ArenaUsed);
	ArenaUsed += size;

	b->start = off;
	b->fall = fall;
	b->count = n;
	b->split = split;
	b->idle = 0;
	b->wmask = 0;
	b->nind = 0;
#if MIPS3_PROFILE
	b->vpc = 0;
	b->prof = 0;
#endif

#if MIPS3_IDLE_SKIP
	// a block whose closing PC-relative branch jumps back to its own start
	if (!split && n >= 2 && (tmp[n - 1].flags & F_DELAY)) {
		UINT32 bop = tmp[n - 2].op, opc = bop >> 26;
		INT32 relative = (opc == 0x01 || (opc >= 0x04 && opc <= 0x07) || (opc >= 0x14 && opc <= 0x17));
		if (relative && (INT32)(tmp[n - 2].pc + 4) + ((INT32)(INT16)bop << 2) == (INT32)off) {
			IdleAnalyse(b, tmp, n);
		}
	}
#endif
	memcpy(b->insn, tmp, n * sizeof(Mips3Insn));

	if (bp->blk == NULL) bp->blk = (Mips3Block**)calloc(1024, sizeof(Mips3Block*));
	bp->blk[off >> 2] = b;

	UINT32 c0 = off >> 8, c1 = (off + n * 4 - 1) >> 8;
	for (UINT32 c = c0; c <= c1; c++) bp->codemask |= 1 << c;

	return b;
}

static void ExecBlockOut(Mips3Block *b);
#if MIPS3_IDLE_SKIP
static inline void ExecBlockPlain(Mips3Block *b)
{
	UINT8 idle = b->idle;
	b->idle = 0;
	ExecBlockOut(b);
	b->idle = idle;
}

// runs one iteration with time set to t, then puts everything back: tells whether it loops again
static INT32 IdleProbe(Mips3Block *b, UINT64 t, UINT64 cause)
{
	UINT64 saved_total = M.total_cycles;
	INT32 saved_icount = nMips3ICount;
	UINT64 ind[4];
	for (INT32 i = 0; i < b->nind; i++) ind[i] = M.r[b->indreg[i]];

	M.total_cycles = t;
	ExecBlockPlain(b);
	INT32 loops = !Mx.brk && M.pc == Mx.vpage + b->start && CAUSE == cause;

	M.total_cycles = saved_total;
	nMips3ICount = saved_icount;
	M.pc = Mx.vpage + b->start;
	for (INT32 i = 0; i < b->nind; i++) M.r[b->indreg[i]] = ind[i];
	return loops;
}

static void ExecIdleCandidate(Mips3Block *b)
{
	UINT64 saved[32];
	UINT32 mask = b->wmask;
	UINT64 cause = CAUSE;

	for (INT32 i = 1; i < 32; i++) if (mask & (1U << i)) saved[i] = M.r[i];

	ExecBlockPlain(b);

	if (Mx.brk || M.pc != Mx.vpage + b->start || CAUSE != cause || nMips3ICount <= 0) return;

	if (b->idle == IDLE_STABLE) {
		for (INT32 i = 1; i < 32; i++) if ((mask & (1U << i)) && saved[i] != M.r[i]) return;
	}

	// whole iterations that fit in the slice, without passing the Count/Compare match
	if (M.compare_fire <= M.total_cycles) return;	// the timer interrupt is due now
	INT64 budget = nMips3ICount;
	if ((UINT64)budget > M.compare_fire - M.total_cycles) budget = (INT64)(M.compare_fire - M.total_cycles);
	INT64 len = b->count;
	INT64 k = budget / len;
	if (k <= 0) return;

	if (b->idle == IDLE_TIMED) {
		// each iteration only depends on Count: find the first one that leaves the loop
		UINT64 t0 = M.total_cycles;
		if (!IdleProbe(b, t0 + k * len, cause)) {
			INT64 lo = 0, hi = k;
			while (hi - lo > 1) {
				INT64 mid = (lo + hi) / 2;
				if (IdleProbe(b, t0 + mid * len, cause)) lo = mid; else hi = mid;
			}
			k = hi;
		}
		if (Mx.brk) return;
	}

	M.total_cycles += k * len;
	nMips3ICount -= (INT32)(k * len);
	for (INT32 i = 0; i < b->nind; i++) {
		UINT32 r = b->indreg[i];
		if (b->ind64[i]) M.r[r] = M.r[r] + (UINT64)(k * b->indimm[i]);
		else M.r[r] = SX32((UINT32)M.r[r] + (UINT32)(k * b->indimm[i]));
	}
#if MIPS3_PROFILE
	nProfIdle += k * len;
#endif
}
#endif

static MIPS3_INLINE void ExecBlock(Mips3Block *b)
{
#if MIPS3_IDLE_SKIP
	if (b->idle) { ExecIdleCandidate(b); return; }
#endif
	Mx.blk_start = b->start;
	Mx.next_pc = Mx.vpage + b->fall;
	Mx.brk = 0;

	const Mips3Insn *I = b->insn;
	const Mips3Insn *end = I + b->count;

	do {
		I->fn(I);
		I++;
	} while (!Mx.brk && I < end);

	INT32 n = (INT32)(I - b->insn);
	M.total_cycles += n;
	nMips3ICount -= n;
#if MIPS3_PROFILE
	b->vpc = Mx.vpage + b->start;
	b->prof += n;
#endif

	if (Mx.brk) {
		M.pc = Mx.next_pc;
		if (Mx.dpend) {
			Mx.dpend = 0;
			M.delay_pending = 1;
			M.delay_target = Mx.dtarget;
			nNextEvent = 0;
		}
	} else if (b->split) {
		M.delay_pending = 1;
		M.delay_target = Mx.next_pc;
		M.pc = Mx.vpage + b->fall - 4;
		nNextEvent = 0;
	} else {
		M.pc = Mx.next_pc;
	}
}

static void ExecBlockOut(Mips3Block *b)
{
	ExecBlock(b);
}

#endif

// ---------------------------------------------------------------------------
// uncached execution: used for delay slots split across pages, code in handler space,
// fetch exceptions and when the cache is disabled

static void StepOne()
{
	UINT32 pc = M.pc;
	Mips3Insn I;
	UINT32 pa, fl = 0;

	I.pc = pc & 0xfff;
	I.flags = M.delay_pending ? F_DELAY : 0;
	I.op = 0;
	I.fn = op_NOP;

	Mx.vpage = pc & ~0xfff;
	Mx.blk_start = I.pc;
	Mx.next_pc = 1;		// never a valid target: tells whether a branch was taken
	Mx.brk = 0;

	M.total_cycles++;
	nMips3ICount--;
#if MIPS3_PROFILE
	nProfStep++;
#endif

	if (pc & 3) {
		AddressError(&I, pc, 0);
	} else {
		UINT32 e = Vtlb[pc >> 12];
		if (!(e & VT_READ)) {
			TlbFault(&I, pc, 0, e);
		} else {
			pa = (e & ~0xfff) | (pc & 0xfff);
			I.op = RdPhys32(pa);
			I.fn = Mips3Decode(I.op, &fl);
			I.fn(&I);
		}
	}

#if MIPS3_DEBUG_TRACE
	{
		UINT32 dest = ((I.op >> 26) == 0) ? ((I.op >> 11) & 31) : ((I.op >> 16) & 31);
		Mips3TraceEntry *t = &DbgRing[nDbgRingPos++ & 511];
		t->pc = pc;
		t->op = I.op;
		t->val = M.r[dest];

		if (nDbgTraceState == 2 && nDbgTraceFwd > 0) {
			nDbgTraceFwd--;
			bprintf(PRINT_NORMAL, _T("MIPS3 T %08x %08x %08x%08x\n"), pc, I.op, (UINT32)(t->val >> 32), (UINT32)t->val);
		}

		if (nDbgTraceState == 1) {
			nDbgTraceState = 2;
			nDbgTraceFwd = 1500;
			bprintf(PRINT_NORMAL, _T("MIPS3 trace: last 512 instructions before the SYSCALL\n"));
			for (INT32 i = 0; i < 512; i++) {
				Mips3TraceEntry *e = &DbgRing[(nDbgRingPos + i) & 511];
				bprintf(PRINT_NORMAL, _T("MIPS3 B %08x %08x %08x%08x\n"), e->pc, e->op, (UINT32)(e->val >> 32), (UINT32)e->val);
			}
			for (INT32 i = 0; i < 32; i++) {
				bprintf(PRINT_NORMAL, _T("MIPS3 R r%d %08x%08x\n"), i, (UINT32)(M.r[i] >> 32), (UINT32)M.r[i]);
			}
			bprintf(PRINT_NORMAL, _T("MIPS3 R sr %08x cause %08x epc %08x config %08x\n"), (UINT32)SR, (UINT32)CAUSE, (UINT32)CP0(CP0_EPC), (UINT32)CP0(CP0_Config));
			bprintf(PRINT_NORMAL, _T("MIPS3 trace: next 1500 instructions\n"));
		}
	}
#endif

	if (Mx.brk) {
		M.delay_pending = 0;
		M.pc = Mx.next_pc;
	} else if (M.delay_pending) {
		if ((fl & F_BRANCH) && Mx.next_pc != 1) {
			// taken branch inside a delay slot (used by real code, e.g. KI boot ROM):
			// the first branch's target becomes the delay slot of the second one
			M.pc = M.delay_target;
			M.delay_target = Mx.next_pc;
		} else {
			M.delay_pending = 0;
			M.pc = M.delay_target;
		}
	} else if (fl & F_BRANCH) {
		M.delay_pending = 1;
		nNextEvent = 0;
		M.delay_target = (Mx.next_pc != 1) ? Mx.next_pc : pc + 8;
		M.pc = pc + 4;
	} else {
		M.pc = pc + 4;
	}
}

static inline void CheckInterrupts()
{

	if (M.total_cycles >= M.compare_fire) {
		CAUSE |= 0x8000;
		M.compare_fire = ~0ULL;
	}

	UINT32 sr = SR;
	if ((sr & (SR_IE | SR_EXL | SR_ERL)) == SR_IE && (CAUSE & sr & 0xfc00)) {
		Mips3Insn I;
		I.pc = M.pc & 0xfff;
		I.flags = 0;
		Mx.vpage = M.pc & ~0xfff;
		Mips3Exception(&I, EXC_INT, 0);
		M.pc = Mx.next_pc;
	}
}

#if MIPS3_PROFILE && MIPS3_CACHED_INTERPRETER && !MIPS3_DEBUG_TRACE
static void ProfileReport()
{
	const INT32 TOP = 12;
	Mips3Block *top[TOP];
	memset(top, 0, sizeof(top));

	// walk the arena: blocks are stored back to back
	UINT32 off = 0;
	while (off < ArenaUsed) {
		Mips3Block *b = (Mips3Block*)(Arena + off);
		for (INT32 i = 0; i < TOP; i++) {
			if (top[i] == NULL || b->prof > top[i]->prof) {
				for (INT32 j = TOP - 1; j > i; j--) top[j] = top[j - 1];
				top[i] = b;
				break;
			}
		}
		off += (sizeof(Mips3Block) - sizeof(Mips3Insn) + b->count * sizeof(Mips3Insn) + 15) & ~15;
	}

	UINT64 total = M.total_cycles - nProfStart;
	if (total == 0) total = 1;
	bprintf(PRINT_NORMAL, _T("MIPS3 profile: %d Mcycles, idle skipped %d pct, uncached steps %d pct\n"), (INT32)(total / 1000000),
		(INT32)(nProfIdle * 100 / total), (INT32)(nProfStep * 100 / total));

	for (INT32 i = 0; i < TOP && top[i] && top[i]->prof; i++) {
		Mips3Block *b = top[i];
		// one bprintf per line: frontends prefix every call
		char line[400];
		INT32 len = 0;
		for (INT32 j = 0; j < b->count && j < 24; j++) len += sprintf(line + len, " %08x", b->insn[j].op);
		line[len] = 0;
		bprintf(PRINT_NORMAL, _T("MIPS3 hot %2d: %08x %3d.%d pct len %d idle %d :%hs\n"), i, b->vpc, (INT32)(b->prof * 100 / total), (INT32)(b->prof * 1000 / total % 10), b->count, b->idle, line);
	}

	off = 0;
	while (off < ArenaUsed) {
		Mips3Block *b = (Mips3Block*)(Arena + off);
		b->prof = 0;
		off += (sizeof(Mips3Block) - sizeof(Mips3Insn) + b->count * sizeof(Mips3Insn) + 15) & ~15;
	}
	nProfIdle = nProfStep = 0;
	nProfStart = M.total_cycles;
}
#endif

INT32 mips3_run(INT32 cycles)
{
	nMips3ICount += cycles;	// a negative leftover from the previous slice is paid back here
	nNextEvent = 0;
	INT32 start = nMips3ICount;

	while (nMips3ICount > 0) {
		if (M.total_cycles >= nNextEvent) {
			if (M.delay_pending) {
				StepOne();
				continue;
			}
			CheckInterrupts();
			nNextEvent = M.compare_fire;
		}

#if MIPS3_CACHED_INTERPRETER && !MIPS3_DEBUG_TRACE
		UINT32 pc = M.pc;
		Mips3FastEntry *fe = &FastCache[(pc >> 2) & (MIPS3_FAST_ENTRIES - 1)];
		if (fe->pc == pc && fe->gen == nFastGen) {
			Mx.vpage = pc & ~0xfff;
			ExecBlock(fe->b);
			continue;
		}

		UINT32 e = Vtlb[pc >> 12];
		if ((e & VT_READ) && !(pc & 3)) {
			UINT32 pa = (e & ~0xfff) | (pc & 0xfff);
			Mips3PhysPage *p = &Mips3Phys[pa >> 12];
			if (p->code) {
				Mips3BlockPage *bp = p->code;
				Mips3Block *b = bp->blk ? bp->blk[(pa & 0xfff) >> 2] : NULL;
				if (b == NULL) b = BuildBlock(bp, p->read, pa & 0xfff);
				fe->pc = pc;
				fe->gen = nFastGen;
				fe->b = b;
				Mx.vpage = pc & ~0xfff;
				ExecBlock(b);
				continue;
			}
		}
#endif
		StepOne();
	}

#if MIPS3_DEBUG_LOG
	if (M.total_cycles >= nDbgNextPc) {
		nDbgNextPc = M.total_cycles + 50000000;
		bprintf(PRINT_NORMAL, _T("MIPS3 [%d Mcycles] pc %08x sr %08x cause %08x ra %08x sp %08x\n"), (INT32)(M.total_cycles / 1000000),
			M.pc, (UINT32)SR, (UINT32)CAUSE, (UINT32)M.r[31], (UINT32)M.r[29]);
	}
#endif

#if MIPS3_PROFILE && MIPS3_CACHED_INTERPRETER && !MIPS3_DEBUG_TRACE
	if (M.total_cycles >= nProfNext) {
		if (nProfNext) ProfileReport();
		nProfStart = M.total_cycles;
		nProfNext = M.total_cycles + 1000000000ULL;
	}
#endif

	INT32 ran = start - nMips3ICount;
	return ran;
}

void mips3_run_end()
{
	nMips3ICount = 0;
}

INT32 mips3_idle(INT32 cycles)
{
	M.total_cycles += cycles;
	return cycles;
}

UINT64 mips3_total_cycles()
{
	return M.total_cycles;
}

// ---------------------------------------------------------------------------
// memory map changes / interface

void mips3_page_mapped(UINT32 page)
{
	nFastGen++;
	Mips3PhysPage *p = &Mips3Phys[page];
	p->code = IS_PTR(p->read) ? GetBlockPage(p->read) : NULL;
	p->wcode = IS_PTR(p->write) ? GetBlockPage(p->write) : p->code;
}

void mips3_invalidate(UINT32 nStart, UINT32 nEnd)
{
	nStart &= PHYS_MASK;
	nEnd &= PHYS_MASK;

	for (UINT32 a = nStart & ~0xff; a <= nEnd; a += 0x100) {
		Mips3PhysPage *p = &Mips3Phys[a >> 12];
		if (p->code && p->code->codemask) CodeWrite(p->code, a & 0xfff);
		if (p->wcode && p->wcode != p->code && p->wcode->codemask) CodeWrite(p->wcode, a & 0xfff);
		if (a + 0x100 < a) break;
	}
}

void mips3_set_irq_line(INT32 line, INT32 state)
{
	if (line < 0 || line > 5) return;
	nNextEvent = 0;
	if (state) CAUSE |= 0x400 << line;
	else       CAUSE &= ~(0x400 << line);
}

UINT32 mips3_get_pc()
{
	return M.pc;
}

UINT8 mips3_read_phys8(UINT32 pa)
{
	return RdPhys8(pa & PHYS_MASK);
}

INT32 mips3_init()
{
	Vtlb = (UINT32*)calloc(VIRT_PAGES, sizeof(UINT32));
	Arena = (UINT8*)malloc(MIPS3_ARENA_SIZE);
	ArenaUsed = 0;
	BlockPageMap = new std::map<UINT8*, Mips3BlockPage*>();
	BlockPageList = new std::vector<Mips3BlockPage*>();

	memset(&M, 0, sizeof(M));
	M.compare_fire = ~0ULL;
	nMips3ICount = 0;

	VtlbRebuild();

	bMips3Initted = 1;
	return 0;
}

void mips3_exit()
{
	if (!bMips3Initted) return;

	if (BlockPageList) {
		for (size_t i = 0; i < BlockPageList->size(); i++) {
			free((*BlockPageList)[i]->blk);
			free((*BlockPageList)[i]);
		}
	}
	delete BlockPageList;
	delete BlockPageMap;
	BlockPageList = NULL;
	BlockPageMap = NULL;

	free(Vtlb);
	free(Arena);
	Vtlb = NULL;
	Arena = NULL;

	bMips3Initted = 0;
}

void mips3_reset()
{
#if MIPS3_DEBUG_LOG
	nDbgExc = nDbgIrq = nDbgUnmapped = nDbgStatus = nDbgEret = nDbgTlb = 0;
	nDbgNextPc = 0;
	bprintf(PRINT_NORMAL, _T("MIPS3 reset (cached interpreter %d)\n"), MIPS3_CACHED_INTERPRETER);
#endif

	M.pc = 0xbfc00000;
	M.delay_pending = 0;
	M.llbit = 0;

	CP0(CP0_Status) = SR_BEV | SR_ERL;
	CP0(CP0_Cause) &= 0xfc00;	// keep the state of the external interrupt lines
	CP0(CP0_Wired) = 0;
	CP0(CP0_Compare) = 0xffffffff;
	CP0(CP0_Count) = 0;
	CP0(CP0_PRId) = 0x2020;				// R4600
	CP0(CP0_Config) = 0x00026030 | (2 << 6) | (2 << 9);	// 16KB I/D caches, 32 byte lines
	CP0(CP0_LLAddr) = 0;
	M.fcr31 = 0;

	M.count_zero = M.total_cycles;
	M.random_zero = M.total_cycles;
	M.compare_fire = ~0ULL;
	CompareRearm(M.total_cycles);

	for (INT32 i = 0; i < MIPS3_TLB_ENTRIES; i++) {
		M.tlb[i].mask = 0;
		M.tlb[i].hi = 0xffffffff;
		M.tlb[i].lo[0] = 0xfffffff8;
		M.tlb[i].lo[1] = 0xfffffff8;
	}

	VtlbRebuild();
	Mips3FlushCache();
	nMips3ICount = 0;
	nNextEvent = 0;
	Mx.dpend = 0;
}

void mips3_scan(INT32 nAction)
{
	if (!bMips3Initted) return;

	if (nAction & ACB_DRIVER_DATA) {
		ScanVar(&M, sizeof(M), (char*)"MIPS3 Regs");
		SCAN_VAR(nMips3ICount);
	}

	if (nAction & ACB_WRITE) {
		VtlbRebuild();
		Mips3FlushCache();
		nNextEvent = 0;
	}
}
