// FB Neo memory management module

// The purpose of this module is to offer replacement functions for standard C/C++ ones 
// that allocate and free memory.  This should help deal with the problem of memory
// leaks and non-null pointers on game exit.

#include "burnint.h"

#define LOG_MEMORY_USAGE 0

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
			memptr[i] = (UINT8*)malloc(size);

			if (memptr[i] == NULL) {
				bprintf (0, _T("BurnMalloc failed to allocate %d bytes of memory!\n"), size);
				return NULL;
			}

			memset (memptr[i], 0, size); // set contents to 0

			mem_allocated += size;
			memsize[i] = size;

#if LOG_MEMORY_USAGE
			bprintf (0, _T("(%S:%d) BurnMalloc(%d): index %d.  %d total!\n"), file, line, size, i, mem_allocated);
#endif

			return memptr[i];
		}
	}

	bprintf (0, _T("BurnMalloc called too many times!\n"));

	return NULL; // Freak out!
}

UINT8 *BurnRealloc(void *ptr, INT32 size)
{
	UINT8 *mptr = (UINT8*)ptr;

	for (INT32 i = 0; i < MAX_MEM_PTR; i++)
	{
		if (memptr[i] == mptr) {
			memptr[i] = (UINT8*)realloc(ptr, size);
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
			free (memptr[i]);
			memptr[i] = NULL;

			mem_allocated -= memsize[i];
			memsize[i] = 0;
#if LOG_MEMORY_USAGE
			bprintf(0, _T("BurnFree(): index %d.  %d total!\n"), i, mem_allocated);
#endif
			break;
		}
	}
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
