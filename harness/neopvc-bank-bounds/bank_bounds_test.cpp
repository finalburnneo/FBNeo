// Bank-bounds harness for the Neo Geo 68K program-ROM bankswitch paths.
//
// The functions under test are NOT reimplemented here.  They are pulled
// verbatim out of src/burn/drv/neogeo/{d_neogeo,neo_run}.cpp by extract.py and
// #included below, so this binary exercises the code that actually ships.  The
// same file is built twice -- once against the pristine base commit, once
// against the working tree -- and run.sh compares the two.
//
// NO ROM OR BIOS DATA IS USED.  Every buffer is synthetic; every input is
// generated.
//
// Every output line carries a prefix so run.sh can select without depending on
// ordering:
//   IN   in-range results -- MUST be byte-identical between baseline and patched
//   OOB  out-of-range statistics -- expected to differ (that is the fix)
//   RPT  named acceptance-criterion checks
//   TIM  hot-path cost, excluded from the diff

#include "fbneo_bank_shim.h"

#include <time.h>

#include "gen/neo_run_funcs.inc"
#include "gen/d_neogeo_funcs.inc"

// ---------------------------------------------------------------------------
// Window reach at each site, from the SekMapMemory arguments (and, for mslugx,
// from its protection read handler, which reaches further than the mapping).

#define WIN_PVC     0x000FE000u   // 0x200000 .. 0x2FDFFF
#define WIN_MS5PLUS 0x000FE000u   // 0x200000 .. 0x2FDFFF
#define WIN_KF2K3BL 0x000FE000u   // 0x200000 .. 0x2FDFFF
#define WIN_MSLUGX  0x00100000u   // maps 0x200000..0x2FFBFF, but
                                  // mslugx_read_protection_word reads
                                  // bank + (addr & 0xFFFFE) -> reach is 1 MiB
#define WIN_GENERIC 0x00100000u   // 0x200000 .. 0x2FFFFF

// A value no bank computation can produce, so every case forces a remap.
#define BANK_SENTINEL 0xFFFFFFFFu

static const UINT32 kCodeSizes[] = {
	0x00500000u,   // mslugx, ms5plus, cthd2003, mslug5b2
	0x00700000u,   // kf2k3bla, kf2k3pl
	0x00800000u,   // mslug5, svc, svcboot, kf2k3bl, kf2k3upl, kof10th
	0x00900000u,   // kof2003, kof2003h, kf2k3pcb  <- the measured case
	0x00A00000u,   // kof98cp
	0x02000000u,   // svcpcb
};
#define N_CODESIZES ((int)(sizeof(kCodeSizes) / sizeof(kCodeSizes[0])))

// ---------------------------------------------------------------------------
// Accumulators

struct Acc {
	UINT64 hash;
	UINT64 cases;
	UINT64 violations;      // mappings whose reach exceeds nCodeSize
	UINT64 maxOver;         // largest such overshoot, in bytes
	UINT64 wrongInRange;    // in-range input whose result is not the identity
	UINT64 wrongFallback;   // out-of-range result that is neither unchanged
	                        // (baseline) nor the driver's own bank 0 (patched)
};

static void AccInit(Acc *a)
{
	a->hash = 1469598103934665603ULL;   // FNV-1a 64 offset basis
	a->cases = a->violations = a->maxOver = 0;
	a->wrongInRange = a->wrongFallback = 0;
}

static void HFold(Acc *a, UINT64 v)
{
	a->hash ^= v;
	a->hash *= 1099511628211ULL;
}

// Fold the complete observable result of one call: the resulting bank plus
// every SekMapMemory argument, in order.
static void AccRecord(Acc *a)
{
	a->cases++;
	HFold(a, nNeo68KROMBank);
	HFold(a, (UINT64)g_shimCallCount);
	int n = g_shimCallCount < SHIM_MAX_CALLS ? g_shimCallCount : SHIM_MAX_CALLS;
	for (int i = 0; i < n; i++) {
		HFold(a, ShimOffset(i));
		HFold(a, g_shimCalls[i].nStart);
		HFold(a, g_shimCalls[i].nEnd);
		HFold(a, (UINT64)g_shimCalls[i].nType);
	}
}

static void AccCheckContainment(Acc *a, UINT32 reach)
{
	UINT64 size = (UINT64)nCodeSize[nNeoActiveSlot];
	int n = g_shimCallCount < SHIM_MAX_CALLS ? g_shimCallCount : SHIM_MAX_CALLS;
	for (int i = 0; i < n; i++) {
		UINT64 end = ShimOffset(i) + reach;
		if (end > size) {
			a->violations++;
			UINT64 over = end - size;
			if (over > a->maxOver) a->maxOver = over;
		}
	}
}

// For an out-of-range input: if the resulting mapping IS contained, the code
// under test must have clamped it, and the clamp must land on the driver's own
// bank 0.  A mapping that is NOT contained is the unpatched behaviour and is
// counted by AccCheckContainment instead.  Written this way so the same check
// is meaningful for both variants without the harness knowing which it is.
static void AccCheckFallback(Acc *a, UINT64 violationsBefore, UINT32 bank0)
{
	if (a->violations == violationsBefore && nNeo68KROMBank != bank0) {
		a->wrongFallback++;
	}
}

static void PrintAcc(const char *prefix, const char *kind, const char *tag,
                     UINT32 size, const Acc *a)
{
	printf("%-4s %-8s %-10s size=0x%08x cases=%llu hash=0x%016llx "
	       "violations=%llu maxover=%llu wrong_inrange=%llu wrong_fallback=%llu\n",
	       prefix, kind, tag, size,
	       (unsigned long long)a->cases,
	       (unsigned long long)a->hash,
	       (unsigned long long)a->violations,
	       (unsigned long long)a->maxOver,
	       (unsigned long long)a->wrongInRange,
	       (unsigned long long)a->wrongFallback);
}

static void PrintTrace(const char *tag, const char *inputs)
{
	printf("IN   trace    %-10s %-34s bank=0x%08x calls=%d",
	       tag, inputs, nNeo68KROMBank, g_shimCallCount);
	int n = g_shimCallCount < SHIM_MAX_CALLS ? g_shimCallCount : SHIM_MAX_CALLS;
	for (int i = 0; i < n; i++) {
		printf(" | off=0x%08llx start=0x%06x end=0x%06x type=%d",
		       (unsigned long long)ShimOffset(i),
		       g_shimCalls[i].nStart, g_shimCalls[i].nEnd, g_shimCalls[i].nType);
	}
	printf("\n");
}

// ---------------------------------------------------------------------------
// Drivers for each function under test

static void PvcSetup(UINT32 raw24, int kof2003Flag)
{
	ShimResetCalls();
	nNeo68KROMBank = BANK_SENTINEL;
	PVCRAM[0x1ff1] = (UINT8)(raw24 & 0xff);
	PVCRAM[0x1ff2] = (UINT8)((raw24 >> 8) & 0xff);
	PVCRAM[0x1ff3] = (UINT8)((raw24 >> 16) & 0xff);
	Neo68KROMActive[0x108] = kof2003Flag ? 0x10 : 0x00;
}

static UINT32 PvcBank0(int kof2003Flag) { return kof2003Flag ? 0x100000u : 0u; }

static int InRange(UINT64 bank, UINT32 reach, UINT32 size)
{
	return (bank + (UINT64)reach) <= (UINT64)size;
}

static void PvcCase(UINT32 raw24, int flag, Acc *in, Acc *oob, int trace)
{
	UINT32 expected = raw24 + PvcBank0(flag);
	UINT32 size = nCodeSize[nNeoActiveSlot];
	int isIn = InRange(expected, WIN_PVC, size);

	PvcSetup(raw24, flag);
	NeoPVCBankswitch();

	if (isIn) {
		AccRecord(in);
		if (nNeo68KROMBank != expected) in->wrongInRange++;
		AccCheckContainment(in, WIN_PVC);
		if (trace) {
			char buf[64];
			snprintf(buf, sizeof(buf), "raw=0x%06x f=%d sz=0x%08x", raw24, flag, size);
			PrintTrace("pvc", buf);
		}
	} else {
		AccRecord(oob);
		UINT64 before = oob->violations;
		AccCheckContainment(oob, WIN_PVC);
		AccCheckFallback(oob, before, PvcBank0(flag));
	}
}

static void Ms5plusCase(UINT32 word, Acc *in, Acc *oob)
{
	UINT32 expected = word << 16;
	UINT32 size = nCodeSize[nNeoActiveSlot];
	int isIn = InRange(expected, WIN_MS5PLUS, size);

	ShimResetCalls();
	nNeo68KROMBank = BANK_SENTINEL;
	ms5plusWriteWordBankSwitch(0x2ffff4, (UINT16)word);

	if (isIn) {
		AccRecord(in);
		if (nNeo68KROMBank != expected) in->wrongInRange++;
		AccCheckContainment(in, WIN_MS5PLUS);
	} else {
		AccRecord(oob);
		UINT64 before = oob->violations;
		AccCheckContainment(oob, WIN_MS5PLUS);
		AccCheckFallback(oob, before, 0x100000u);
	}
}

// kf2k3blaWriteWordBankswitch stores the word at PVCRAM[0x1ff2..0x1ff3]
// (little-endian host, identity swap) and then reads bytes 3, 2 and 0.
static UINT32 Kf2k3blaExpected(UINT32 word, UINT8 b0)
{
	UINT32 hi  = (word >> 8) & 0xff;
	UINT32 mid = word & 0xff;
	return ((hi << 16) | (mid << 8) | b0) + 0x100000u;
}

static void Kf2k3blaCase(UINT32 word, UINT8 b0, Acc *in, Acc *oob)
{
	UINT32 expected = Kf2k3blaExpected(word, b0);
	UINT32 size = nCodeSize[nNeoActiveSlot];
	int isIn = InRange(expected, WIN_KF2K3BL, size);

	ShimResetCalls();
	nNeo68KROMBank = BANK_SENTINEL;
	PVCRAM[0x1ff0] = b0;
	kf2k3blaWriteWordBankswitch(0x2ffff2, (UINT16)word);

	if (isIn) {
		AccRecord(in);
		if (nNeo68KROMBank != expected) in->wrongInRange++;
		AccCheckContainment(in, WIN_KF2K3BL);
	} else {
		AccRecord(oob);
		UINT64 before = oob->violations;
		AccCheckContainment(oob, WIN_KF2K3BL);
		AccCheckFallback(oob, before, 0x100000u);
	}
}

static void MslugxCase(UINT32 value, Acc *in, Acc *oob, int trace)
{
	UINT32 expected = ((value & 7) + 1) * 0x100000u;
	UINT32 size = nCodeSize[nNeoActiveSlot];
	int isIn = InRange(expected, WIN_MSLUGX, size);

	ShimResetCalls();
	nNeo68KROMBank = BANK_SENTINEL;
	mslugxBankswitch(value);

	if (isIn) {
		AccRecord(in);
		if (nNeo68KROMBank != expected) in->wrongInRange++;
		AccCheckContainment(in, WIN_MSLUGX);
		if (trace) {
			char buf[64];
			snprintf(buf, sizeof(buf), "v=0x%04x sz=0x%08x", value, size);
			PrintTrace("mslugx", buf);
		}
	} else {
		AccRecord(oob);
		UINT64 before = oob->violations;
		AccCheckContainment(oob, WIN_MSLUGX);
		AccCheckFallback(oob, before, 0x100000u);
	}
}

static void GenericCase(UINT32 value, Acc *in, int trace)
{
	ShimResetCalls();
	nNeo68KROMBank = BANK_SENTINEL;
	Bankswitch(value);
	AccRecord(in);
	AccCheckContainment(in, WIN_GENERIC);
	if (trace) {
		char buf[64];
		snprintf(buf, sizeof(buf), "v=0x%04x sz=0x%08x", value,
		         nCodeSize[nNeoActiveSlot]);
		PrintTrace("generic", buf);
	}
}

// ---------------------------------------------------------------------------
// Structured corpus -- printed call-by-call so a reviewer can read the diff.

static void StructuredCorpus()
{
	for (int ci = 0; ci < N_CODESIZES; ci++) {
		UINT32 size = kCodeSizes[ci];
		ShimSetCodeSize(size);

		for (int flag = 0; flag <= 1; flag++) {
			Acc in, oob;
			AccInit(&in);
			AccInit(&oob);
			UINT32 base = PvcBank0(flag);
			UINT64 limit = (UINT64)size - WIN_PVC;
			UINT32 lastRaw = (limit > (UINT64)base) ? (UINT32)(limit - base) : 0u;
			// The PVC bank register is only 3 bytes wide, so no raw value above
			// 0xFFFFFF is representable however large the ROM is (svcpcb).
			if (lastRaw > 0xFFFFFFu) lastRaw = 0xFFFFFFu;
			UINT32 probes[] = {
				0u, 1u, 2u, 0xFFFFu, 0x10000u,
				lastRaw > 2 ? lastRaw - 2 : 0u,
				lastRaw > 1 ? lastRaw - 1 : 0u,
				lastRaw,
			};
			for (unsigned i = 0; i < sizeof(probes) / sizeof(probes[0]); i++) {
				PvcCase(probes[i], flag, &in, &oob, 1);
			}
			for (UINT32 raw = 0; raw <= lastRaw; raw += 0x10000u) {
				PvcCase(raw, flag, &in, &oob, 1);
			}
			char tag[16];
			snprintf(tag, sizeof(tag), "pvc/f%d", flag);
			PrintAcc("IN", "struct", tag, size, &in);
		}

		{
			Acc in, oob;
			AccInit(&in);
			AccInit(&oob);
			for (UINT32 v = 0; v < 8; v++) MslugxCase(v, &in, &oob, 1);
			PrintAcc("IN", "struct", "mslugx", size, &in);
		}

		{
			Acc in;
			AccInit(&in);
			for (UINT32 v = 0; v < 8; v++) GenericCase(v, &in, 1);
			PrintAcc("IN", "struct", "generic", size, &in);
		}
	}
}

// ---------------------------------------------------------------------------
// Exhaustive sweeps -- every reachable input, summarised as a hash.

static void ExhaustiveSweeps()
{
	for (int ci = 0; ci < N_CODESIZES; ci++) {
		UINT32 size = kCodeSizes[ci];
		ShimSetCodeSize(size);

		for (int flag = 0; flag <= 1; flag++) {
			Acc in, oob;
			AccInit(&in);
			AccInit(&oob);
			for (UINT32 raw = 0; raw <= 0xFFFFFFu; raw++) {
				PvcCase(raw, flag, &in, &oob, 0);
			}
			char tag[16];
			snprintf(tag, sizeof(tag), "PVC/f%d", flag);
			PrintAcc("IN",  "exh", tag, size, &in);
			PrintAcc("OOB", "exh", tag, size, &oob);
		}

		{
			Acc in, oob;
			AccInit(&in);
			AccInit(&oob);
			for (UINT32 w = 0; w <= 0xFFFFu; w++) Ms5plusCase(w, &in, &oob);
			PrintAcc("IN",  "exh", "MS5PLUS", size, &in);
			PrintAcc("OOB", "exh", "MS5PLUS", size, &oob);
		}

		{
			Acc in, oob;
			AccInit(&in);
			AccInit(&oob);
			static const UINT8 b0s[] = { 0x00, 0x01, 0x55, 0xAA, 0xFF };
			for (unsigned k = 0; k < sizeof(b0s) / sizeof(b0s[0]); k++) {
				for (UINT32 w = 0; w <= 0xFFFFu; w++) Kf2k3blaCase(w, b0s[k], &in, &oob);
			}
			PrintAcc("IN",  "exh", "KF2K3BLA", size, &in);
			PrintAcc("OOB", "exh", "KF2K3BLA", size, &oob);
		}

		{
			Acc in, oob;
			AccInit(&in);
			AccInit(&oob);
			for (UINT32 v = 0; v <= 0xFFFFu; v++) MslugxCase(v, &in, &oob, 0);
			PrintAcc("IN",  "exh", "MSLUGX", size, &in);
			PrintAcc("OOB", "exh", "MSLUGX", size, &oob);
		}

		{
			Acc in;
			AccInit(&in);
			for (UINT32 v = 0; v <= 0xFFFFu; v++) GenericCase(v, &in, 0);
			PrintAcc("IN", "exh", "GENERIC", size, &in);
		}
	}
}

// ---------------------------------------------------------------------------
// AC-2: reproduce the on-device measurement field for field.
//
//   BankMapOOB,site=d_neogeo:NeoPVCBankswitch,bank=0x010ffffe,start=0x010ffffe,
//              end=0x011fdffe,codesize=0x00900000,over=9428990

static void MeasuredCase()
{
	ShimSetCodeSize(0x00900000u);
	PvcSetup(0x00FFFFFEu, 1);
	NeoPVCBankswitch();

	UINT64 off = ShimOffset(0);
	UINT64 end = off + WIN_PVC;
	long long over = (long long)end - (long long)0x00900000;

	printf("RPT  measured kof2003 bank=0x%08x off=0x%08llx end=0x%08llx "
	       "codesize=0x00900000 over=%lld contained=%d\n",
	       nNeo68KROMBank,
	       (unsigned long long)off,
	       (unsigned long long)end,
	       over,
	       over <= 0 ? 1 : 0);
}

// Savestate restatement: an out-of-range nNeo68KROMBank restored from a state
// written by an older build, then re-mapped through pBankswitch.

static void RestatementCases()
{
	ShimSetCodeSize(0x00900000u);
	ShimResetCalls();
	nNeo68KROMBank = 0x010FFFFEu;
	Neo68KROMActive[0x108] = 0x10;
	NeoPVCMapBank();
	printf("RPT  restate NeoPVCMapBank in=0x010ffffe bank=0x%08x off=0x%08llx "
	       "end=0x%08llx size=0x00900000 contained=%d\n",
	       nNeo68KROMBank,
	       (unsigned long long)ShimOffset(0),
	       (unsigned long long)(ShimOffset(0) + WIN_PVC),
	       (ShimOffset(0) + WIN_PVC) <= 0x00900000u ? 1 : 0);

	ShimSetCodeSize(0x00500000u);
	ShimResetCalls();
	nNeo68KROMBank = 0x00800000u;
	mslugxMapBank();
	printf("RPT  restate mslugxMapBank in=0x00800000 bank=0x%08x off=0x%08llx "
	       "end=0x%08llx size=0x00500000 contained=%d\n",
	       nNeo68KROMBank,
	       (unsigned long long)ShimOffset(0),
	       (unsigned long long)(ShimOffset(0) + WIN_MSLUGX),
	       (ShimOffset(0) + WIN_MSLUGX) <= 0x00500000u ? 1 : 0);
}

// ---------------------------------------------------------------------------
// AC-7: hot-path cost.  NeoPVCBankswitch runs whenever the game writes the
// protection bank register, which happens during ordinary gameplay.

static void Timing()
{
	ShimSetCodeSize(0x00900000u);
	const UINT32 sweep = 0x40000u;
	double best = 1e30;
	UINT64 sink = 0;

	for (int r = 0; r < 20; r++) {
		struct timespec t0, t1;
		clock_gettime(CLOCK_MONOTONIC, &t0);
		for (UINT32 i = 0; i < sweep; i++) {
			PvcSetup((i * 0x400u) & 0x7FF000u, 1);
			NeoPVCBankswitch();
			sink += nNeo68KROMBank;
		}
		clock_gettime(CLOCK_MONOTONIC, &t1);
		double ns = (double)(t1.tv_sec - t0.tv_sec) * 1e9
		          + (double)(t1.tv_nsec - t0.tv_nsec);
		double per = ns / (double)sweep;
		if (per < best) best = per;
	}
	printf("TIM  NeoPVCBankswitch calls=%u best_ns_per_call=%.3f sink=%llu\n",
	       sweep, best, (unsigned long long)sink);
}

// ---------------------------------------------------------------------------

int main(void)
{
	ShimInit();
	StructuredCorpus();
	ExhaustiveSweeps();
	MeasuredCase();
	RestatementCases();
	Timing();
	return 0;
}
