#include "fbneo_bank_shim.h"

UINT8 *Neo68KROMActive = NULL;
UINT8 *PVCRAM = NULL;
UINT32 nNeo68KROMBank = 0;
UINT32 nCodeSize[MAX_SLOT] = { 0 };
INT32  nNeoActiveSlot = 0;

struct ShimMapCall g_shimCalls[SHIM_MAX_CALLS];
int g_shimCallCount = 0;

INT32 SekMapMemory(UINT8 *pMemory, UINT32 nStart, UINT32 nEnd, INT32 nType)
{
	if (g_shimCallCount < SHIM_MAX_CALLS) {
		g_shimCalls[g_shimCallCount].pMemory = pMemory;
		g_shimCalls[g_shimCallCount].nStart  = nStart;
		g_shimCalls[g_shimCallCount].nEnd    = nEnd;
		g_shimCalls[g_shimCallCount].nType   = nType;
	}
	g_shimCallCount++;
	return 0;
}

void SekOpen(INT32) {}
void SekClose() {}

void ShimResetCalls()
{
	g_shimCallCount = 0;
	memset(g_shimCalls, 0, sizeof(g_shimCalls));
}

// The functions under test compute `Neo68KROMActive + nBank` for banks that,
// unpatched, run as high as 0xFFFF0000 (ms5plus).  Pointing the ROM base at an
// ordinary malloc block would make that pointer arithmetic run off the end of
// its object.  Instead the harness reserves 8 GiB of address space with
// PROT_NONE -- no pages are committed, nothing is ever dereferenced through it,
// and every pointer the code under test can construct still lands inside one
// real mapping.  The first 64 MiB is made writable because the PVC path reads
// Neo68KROMActive[0x108] and the harness has to set it.
#define SHIM_RESERVE ((size_t)8 << 30)
#define SHIM_USABLE  ((size_t)64 << 20)

void ShimInit()
{
	if (Neo68KROMActive != NULL) return;

	void *p = mmap(NULL, SHIM_RESERVE, PROT_NONE,
	               MAP_PRIVATE | MAP_ANON, -1, 0);
	if (p == MAP_FAILED) {
		fprintf(stderr, "shim: could not reserve %zu bytes of address space\n",
		        SHIM_RESERVE);
		exit(2);
	}
	if (mprotect(p, SHIM_USABLE, PROT_READ | PROT_WRITE) != 0) {
		fprintf(stderr, "shim: mprotect failed\n");
		exit(2);
	}
	memset(p, 0, SHIM_USABLE);

	Neo68KROMActive = (UINT8 *)p;

	PVCRAM = (UINT8 *)calloc(1, 0x2000);
	if (PVCRAM == NULL) {
		fprintf(stderr, "shim: PVCRAM allocation failed\n");
		exit(2);
	}
}

void ShimSetCodeSize(UINT32 nSize)
{
	nCodeSize[nNeoActiveSlot] = nSize;
}
