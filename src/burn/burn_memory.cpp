// FB Neo memory management module

// The purpose of this module is to offer replacement functions for standard C/C++ ones 
// that allocate and free memory.  This should help deal with the problem of memory
// leaks and non-null pointers on game exit.

#include "burnint.h"

#define LOG_MEMORY_USAGE    0
#define OOB_CHECKER         1
#define OOB_CHECK			0x200 // default oob detection range (512bytes)

#define MAX_MEM_PTR	0x400 // more than 1024 malloc calls should be insane...

static UINT8 *memptr[MAX_MEM_PTR]; // pointer to allocated memory
static INT32 memsize[MAX_MEM_PTR];
static INT32 mem_allocated;

// this should be called early on... BurnDrvInit?

void BurnInitMemoryManager()
{
	memset (memptr, 0, sizeof(memptr));
	memset (memsize, 0, sizeof(memsize));
	mem_allocated = 0;
}

// call BurnMalloc() instead of 'malloc' (see macro in burnint.h)
UINT8 *_BurnMalloc(INT32 size, char *file, INT32 line)
{
	for (INT32 i = 0; i < MAX_MEM_PTR; i++)
	{
		if (memptr[i] == NULL) {
			INT32 spill = (OOB_CHECKER) ? OOB_CHECK : 0;
			memptr[i] = (UINT8*)malloc(size + spill);

			if (memptr[i] == NULL) {
				bprintf (0, _T("BurnMalloc failed to allocate %d bytes of memory!\n"), size);
				return NULL;
			}

			memset (memptr[i], 0, size + spill); // set contents to 0

			mem_allocated += size; // important: do not record "spill" here (see check_overwrite())
			memsize[i] = size;

#if LOG_MEMORY_USAGE
			bprintf (0, _T("(%S:%d) BurnMalloc(%x): index %d.  %d total!\n"), file, line, size, i, mem_allocated);
#endif

			return memptr[i];
		}
	}

	bprintf (0, _T("BurnMalloc called too many times!\n"));

	return NULL; // Freak out!
}

enum {
	MEM_FREE = 0,
	MEM_REALLOC
};

static void check_overwrite(INT32 i, INT32 type)
{
	if (OOB_CHECKER == 0) return;

	UINT8 *p = memptr[i];
	INT32 size = memsize[i];

	INT32 found_oob = 0;

	for (INT32 z = 0; z < OOB_CHECK; z++) {
		if (p[size + z] != 0) {
			bprintf(0, _T("burn_memory.cpp(%s): OOB detected in allocated index %d @ %x!!\n"), (type == MEM_FREE) ? _T("BurnFree()") : _T("BurnRealloc()"), i, z);
			found_oob = 1;
		}
	}

	if (found_oob) {
		bprintf(0, _T("->OOB memory issue detected in allocated index %d, please let FBNeo team know!\n"), i);
	}
}

UINT8 *BurnRealloc(void *ptr, INT32 size)
{
	UINT8 *mptr = (UINT8*)ptr;

	for (INT32 i = 0; i < MAX_MEM_PTR; i++)
	{
		if (memptr[i] == mptr) {
			check_overwrite(i, MEM_REALLOC);
			INT32 spill = (OOB_CHECKER) ? OOB_CHECK : 0;
			memptr[i] = (UINT8*)realloc(ptr, size + spill);
			if (spill) memset (memptr[i] + size, 0, spill);
			mem_allocated -= memsize[i];
			mem_allocated += size;
			memsize[i] = size;
			return memptr[i];
		}
	}

	return NULL;
}

// call BurnFree() instead of "free" (see macro in burnint.h)
void _BurnFree(void *ptr)
{
	UINT8 *mptr = (UINT8*)ptr;

	for (INT32 i = 0; i < MAX_MEM_PTR; i++)
	{
		if (mptr != NULL && memptr[i] == mptr) {
			check_overwrite(i, MEM_FREE);
			free (memptr[i]);
			memptr[i] = NULL;

			mem_allocated -= memsize[i];
#if LOG_MEMORY_USAGE
			bprintf(0, _T("BurnFree(): index %d, size %x.  %d total!\n"), i, memsize[i], mem_allocated);
#endif
			memsize[i] = 0;
			break;
		}
	}
}

// Swap contents of src with dst
void BurnSwapMemBlock(UINT8 *src, UINT8 *dst, INT32 size)
{
	UINT8 *temp = BurnMalloc(size);

	memcpy(temp,	src,	size);
	memcpy(src,		dst,	size);
	memcpy(dst,		temp,	size);

	BurnFree(temp);
}


// call in BurnDrvExit?

void BurnExitMemoryManager()
{
	for (INT32 i = 0; i < MAX_MEM_PTR; i++)
	{
		if (memptr[i] != NULL) {
#if defined FBNEO_DEBUG
			bprintf(PRINT_ERROR, _T("BurnExitMemoryManager had to free mem pointer %i (%d bytes)\n"), i, memsize[i]);
#endif
			free (memptr[i]);
			memptr[i] = NULL;

			mem_allocated -= memsize[i];
			memsize[i] = 0;
		}
	}

	mem_allocated = 0;
}
