// FB Alpha CPU address map interface module
// written by iq_132 March of 2018.

#include "burnint.h"
#include "burnmap.h"
#include "burnmapcore.h"

/*
	to do:
	- hook up endianness
	- auto optimize map
		- loop through and merge functions that are the same address range
		- move rom/ram regions to beginning of list
*/

/*----------------------------------------------------------------------------------------*\
| Variables
|
\*----------------------------------------------------------------------------------------*/

#define MAX_CPUS		16

#define BURMAP_DEBUG	1

//	use for memptr and chip select
#define READ			0
#define WRITE			1
#define FETCHOP			2
#define FETCHARG		3

//						0x0000000f reserved for MAP_RAM flags
//						0x000000f0 reserved for expansion
#define MAP_WNOP		0x00000100
#define MAP_RNOP		0x00000200
#define MAP_SKIP		0x00000400
#define MAP_MBANK		0x00000800
#define MAP_ALLOC		0x00001000
#define MAP_NVALLOC		0x00002000
#define MAP_DEVICE		0x00004000

#define MAP_ENDMARKER	0xffffffff

static struct cpumap_entry cpuentries[MAX_CPUS];

/*----------------------------------------------------------------------------------------*\
| Driver functions (should these be wrapped in macros to look cleaner?)
|
\*----------------------------------------------------------------------------------------*/

static struct cpumap_entry *cpuentry_ptr;
static struct memory_entry *mementry_ptr;

void MAP_START(INT32 map_num, INT32 address_bits, INT32 flags)
{
#if defined (FBA_DEBUG)
	if (map_num >= MAX_CPUS || map_num < 0) {
		bprintf (0, _T("MAP_START using invalid map (%d), max is %d!\n"), map_num, MAX_CPUS);
		return;
	}

	if (address_bits == 0 || address_bits > 32) {
		bprintf (0, _T("MAP_START using invalid address bits (%d)!  Must be greater than 0 and less than 32!\n"), address_bits);
		return;
	}
#endif

	cpuentry_ptr = &cpuentries[map_num];
	cpuentry_ptr->initialized = 1;
	mementry_ptr = &cpuentry_ptr->mem_entries[BURNMAP_START_OFFSET-1];
	cpuentry_ptr->mapnumber = map_num;
	cpuentry_ptr->MEMMAP_MASK = (address_bits == 32) ? 0xffffffff : ((1 << address_bits) - 1); // ugly, but works on my compiler
	cpuentry_ptr->MEMMAP_ENDIANNESS = (flags & MAP_FLAG_BIGENDIAN) ? 1 : 0; // 0 - little (x86), 1 - big (68k)
	cpuentry_ptr->MEMMAP_ALIGNBYTES = (flags & MAP_FLAG_BYTEALIGN) ? 0 : 1; // 0 (v60), 1 (arm/arm7)
	cpuentry_ptr->MEMMAP_DATABITS = (flags >> 8) & 7; // 1, 2, 4
	if (cpuentry_ptr->MEMMAP_DATABITS <= 1) {
		cpuentry_ptr->MEMMAP_DATABITS = 1;
		cpuentry_ptr->MEMMAP_ALIGNBYTES = 0;
	}
	cpuentry_ptr->entry_start_offset = BURNMAP_START_OFFSET;
}

void MAP_MODIFY(INT32 map_num)
{
	cpuentry_ptr = &cpuentries[map_num];

#if defined (FBA_DEBUG)
	if (map_num >= MAX_CPUS || map_num < 0) {
		bprintf (0, _T("MAP_MODIFY using invalid map (%d), max is %d!\n"), map_num, MAX_CPUS);
		return;
	}

	if (cpuentry_ptr->initialized == 0) {
		bprintf (0, _T("MAP_MODIFY using unitialized map (%d)!\n"), map_num);
		return;
	}
#endif

	mementry_ptr = &cpuentry_ptr->mem_entries[0];

	for (INT32 i = 0; i < BURNMAP_ENTRIES; i++)
	{
		if (mementry_ptr->flags == MAP_ENDMARKER)
		{
			mementry_ptr--;
			return;
		}
		mementry_ptr++;
	}

#if defined (FBA_DEBUG)
	bprintf (0, _T("MAP_MODIFY(%d) called without valid end marker, use MAP_START instead?\n"), map_num);
#endif
}

void MAP_END()
{
	mementry_ptr++;
	mementry_ptr->flags = MAP_ENDMARKER;

	// check for errors
#if defined (FBA_DEBUG)
	{
		for (INT32 i = 0; i < BURNMAP_ENTRIES - BURNMAP_START_OFFSET; i++)
		{
			mementry_ptr = &cpuentry_ptr->mem_entries[BURNMAP_START_OFFSET + i];
			if (mementry_ptr->flags == MAP_ENDMARKER) break;

			if (mementry_ptr->memptr[READ] == NULL && mementry_ptr->memptr[WRITE] == NULL && mementry_ptr->memptr[FETCHOP] == NULL && mementry_ptr->memptr[FETCHARG] == NULL && 
				mementry_ptr->read == NULL && mementry_ptr->write == NULL && mementry_ptr->fetchop == NULL && 
				mementry_ptr->fetcharg == NULL && mementry_ptr->devread == NULL && mementry_ptr->devwrite == NULL)
			{
				if ((mementry_ptr->flags & MAP_WNOP) == 0 && (mementry_ptr->flags & MAP_RNOP) == 0 && (mementry_ptr->flags & MAP_MBANK) == 0 && (cpuentry_ptr->MEMMAP_MASK == mementry_ptr->mirror_mask_byte))
				{
					bprintf (0, _T("map %d entry (%d), address %8.8x - %8.8x has no function!\n"), cpuentry_ptr->mapnumber, i, mementry_ptr->address_start, mementry_ptr->address_end);
				}
			}

			if (mementry_ptr->address_start > mementry_ptr->address_end)
			{
				bprintf (0, _T("map %d entry (%d), address start is greater than end! %8.8x - %8.8x\n"), cpuentry_ptr->mapnumber, i, mementry_ptr->address_start, mementry_ptr->address_end);
			}

			if (i > 0) // ignore first entry
			{
				struct memory_entry *ptr;
				for (INT32 j = 0; j < i; j++)
				{
					ptr = &cpuentry_ptr->mem_entries[j];
					if ((mementry_ptr->address_start >= ptr->address_start) && (mementry_ptr->address_end <= ptr->address_end))
					{
						if ((mementry_ptr->flags & ptr->flags) & MAP_RAM)
						{
							bprintf (0, _T("map %d entry (%d), this range (%8.8x - %8.8x) may be hidden by entry (%d) (%8.8x-%8.8x)\n"), cpuentry_ptr->mapnumber, i, mementry_ptr->address_start, mementry_ptr->address_end, j, ptr->address_start, ptr->address_end);
						}
					}
				}
			}

			if (i >= 8 && (mementry_ptr->flags & MAP_RAM) == MAP_ROM)
			{
				bprintf (0, _T("map %d entry (%d) recommend moving ROM region (address %8.8x-%8.8x) to top of memory map for speed optimization.\n"), cpuentry_ptr->mapnumber, i, mementry_ptr->address_start, mementry_ptr->address_end);
			}
		}
	}
#endif

	mementry_ptr = NULL;
}

void BurnMapExit()
{
	for (INT32 i = 0; i < MAX_CPUS; i++)
	{
		struct cpumap_entry *cptr = &cpuentries[i];
		if (cptr->initialized == 0) continue;

		struct memory_entry *ptr = &cptr->mem_entries[0];

		for (INT32 j = 0; j < BURNMAP_ENTRIES; j++)
		{
			if (ptr->flags == MAP_ENDMARKER) break;

			if (ptr->flags & MAP_ALLOC) {
				if (ptr->memptr[READ] != NULL) {
					BurnFree (ptr->memptr[0]);
					ptr->memptr[WRITE] = NULL;
					ptr->memptr[FETCHOP] = NULL;
					ptr->memptr[FETCHARG] = NULL;
				}
			}

			ptr++;
		}

		memset (cptr, 0, sizeof(cpumap_entry));
	}
}

void MAP_RANGE(UINT32 address_start, UINT32 address_end)
{
#if defined (FBA_DEBUG)
	if (address_start > address_end) {
		bprintf (0, _T("map %d MAP_RANGE [Range %x-%x] passed negative range!\n"), cpuentry_ptr->mapnumber, address_start, address_end);
		return;
	}
#endif

	mementry_ptr++;
	mementry_ptr->address_start = address_start;
	mementry_ptr->address_end = address_end;
	mementry_ptr->mirror_mask_byte = cpuentry_ptr->MEMMAP_MASK;
	mementry_ptr->mirror_mask_word = cpuentry_ptr->MEMMAP_MASK & (cpuentry_ptr->MEMMAP_ALIGNBYTES ? ~1 : ~0);
	mementry_ptr->mirror_mask_long = cpuentry_ptr->MEMMAP_MASK & (cpuentry_ptr->MEMMAP_ALIGNBYTES ? ~3 : ~0);
	mementry_ptr->memptr[READ] = NULL;
	mementry_ptr->memptr[WRITE] = NULL;
	mementry_ptr->memptr[FETCHOP] = NULL;
	mementry_ptr->memptr[FETCHARG] = NULL;
	mementry_ptr->flags = 0;
}

void MAP_MIRROR(UINT32 mirror)
{
	mementry_ptr->mirror_mask_byte = (mirror ^ 0xffffffff) & (cpuentry_ptr->MEMMAP_MASK);
	mementry_ptr->mirror_mask_word = (mirror ^ 0xffffffff) & (cpuentry_ptr->MEMMAP_MASK & (cpuentry_ptr->MEMMAP_ALIGNBYTES ? ~1 : ~0));
	mementry_ptr->mirror_mask_long = (mirror ^ 0xffffffff) & (cpuentry_ptr->MEMMAP_MASK & (cpuentry_ptr->MEMMAP_ALIGNBYTES ? ~3 : ~0));
}

void MAP_REGION(UINT8 *memptr, UINT32 flags)
{
#if defined (FBA_DEBUG)
	if (memptr == NULL) {
		bprintf (0, _T("map %d MAP_REGION [Range %x-%x] called with NULL \"memptr\".\n"), cpuentry_ptr->mapnumber, mementry_ptr->address_start, mementry_ptr->address_end);
		return;
	}

	if ((flags & MAP_RAM) == 0) {
		bprintf (0, _T("map %d MAP_REGION [Range %x-%x] called with 0 flags. This function will do nothing.\n"), cpuentry_ptr->mapnumber, mementry_ptr->address_start, mementry_ptr->address_end);
		return;
	}
#endif

	if (flags & MAP_READ)	 	mementry_ptr->memptr[READ] = memptr;
	if (flags & MAP_WRITE)	 	mementry_ptr->memptr[WRITE] = memptr;
	if (flags & MAP_FETCHOP) 	mementry_ptr->memptr[FETCHOP] = memptr;
	if (flags & MAP_FETCHARG)	mementry_ptr->memptr[FETCHARG] = memptr;
	mementry_ptr->flags |= flags;
}

void MAP_RAMALLOC()
{
	mementry_ptr->memptr[0] = (UINT8*)BurnMalloc((mementry_ptr->address_end - mementry_ptr->address_start)+1);

#if defined (FBA_DEBUG)
	if (mementry_ptr->memptr[0] == NULL) {
		bprintf (0, _T("map %d MAP_RAMALLOC() [Range %x-%x] could not auto alloc memory.\n"), cpuentry_ptr->mapnumber, mementry_ptr->address_start, mementry_ptr->address_end);
		return;
	}
#endif

	mementry_ptr->memptr[WRITE]		= mementry_ptr->memptr[READ];
	mementry_ptr->memptr[FETCHOP]	= mementry_ptr->memptr[READ];
	mementry_ptr->memptr[FETCHARG]	= mementry_ptr->memptr[READ];
	mementry_ptr->flags = MAP_RAM | MAP_ALLOC;
}

void MAP_RAMALLOC(UINT8 **ptr)
{
	mementry_ptr->memptr[0] = (UINT8*)BurnMalloc((mementry_ptr->address_end - mementry_ptr->address_start)+1);

#if defined (FBA_DEBUG)
	if (mementry_ptr->memptr[0] == NULL) {
		bprintf (0, _T("map %d MAP_RAMALLOC() [Range %x-%x] could not auto alloc memory.\n"), cpuentry_ptr->mapnumber, mementry_ptr->address_start, mementry_ptr->address_end);
		return;
	}
#endif

	mementry_ptr->memptr[WRITE]		= mementry_ptr->memptr[READ];
	mementry_ptr->memptr[FETCHOP]	= mementry_ptr->memptr[READ];
	mementry_ptr->memptr[FETCHARG]	= mementry_ptr->memptr[READ];
	mementry_ptr->flags = MAP_RAM | MAP_ALLOC;
	*ptr = mementry_ptr->memptr[0];
}

void MAP_NVRAMALLOC()
{
	mementry_ptr->memptr[0] = (UINT8*)BurnMalloc((mementry_ptr->address_end - mementry_ptr->address_start)+1);

#if defined (FBA_DEBUG)
	if (mementry_ptr->memptr[0] == NULL) {
		bprintf (0, _T("map %d MAP_NVRAMALLOC() [Range %x-%x] could not auto alloc memory.\n"), cpuentry_ptr->mapnumber, mementry_ptr->address_start, mementry_ptr->address_end);
		return;
	}
#endif

	mementry_ptr->memptr[WRITE]		= mementry_ptr->memptr[READ];
	mementry_ptr->memptr[FETCHOP]	= mementry_ptr->memptr[READ];
	mementry_ptr->memptr[FETCHARG]	= mementry_ptr->memptr[READ];
	mementry_ptr->flags = MAP_RAM | MAP_ALLOC | MAP_NVALLOC;
}

void MAP_RAMPTR(UINT8 **ptr)
{
#if defined (FBA_DEBUG)
	if (mementry_ptr->memptr[0] == NULL) {
		bprintf (0, _T("map %d MAP_RAMPTR pointer will be set to NULL!"), cpuentry_ptr->mapnumber);
		return;
	}
#endif

	*ptr = mementry_ptr->memptr[READ];
}

void MAP_BANK(UINT32 flags, INT32 banknumber)
{
	mementry_ptr->bank = banknumber;
	mementry_ptr->flags = flags | MAP_MBANK;
}

void UNMAP_RANGE(INT32 map_num, UINT32 address_start, UINT32 address_end, UINT32 flags)
{
	struct cpumap_entry *cptr = &cpuentries[map_num];

#if defined (FBA_DEBUG)
	if (address_start > address_end) {
		bprintf (0, _T("map %d UNMAP_RANGE [Range %x-%x] called with invalid address range\n"), map_num, address_start, address_end);
		return;
	}

	if ((flags & MAP_RAM) == 0) {
		bprintf (0, _T("map %d UNMAP_RANGE[Range %x-%x] called with 0 flags (this will do nothing).\n"), map_num, address_start, address_end);
		return;
	}

	if (cptr->initialized == 0) {
		bprintf (0, _T("map %d UNMAP_RANGE [Range %x-%x] called with unitialized map.\n"), map_num, address_start, address_end);
		return;
	}
#endif

	struct memory_entry *ptr = &cptr->mem_entries[0];
	if (cptr->initialized == 0) return;

	for (INT32 i = cptr->entry_start_offset; i < BURNMAP_ENTRIES; i++)
	{
		if (ptr->flags == MAP_ENDMARKER) break;

		if (ptr->address_start == address_start && ptr->address_end == address_end)
		{
			if (flags == MAP_RAM) // RAM is all possible interactions
			{
				if (ptr->flags & (MAP_ALLOC | MAP_NVALLOC))
				{
					if (ptr->memptr[READ]!= NULL) {
						BurnFree (ptr->memptr[READ]);
						ptr->memptr[WRITE]		= NULL;
						ptr->memptr[FETCHOP]	= NULL;
						ptr->memptr[FETCHARG]	= NULL;
					}
				}

				memset (ptr, 0, sizeof(memory_entry));
			}
			else	// disable individual functions
			{
				ptr->flags &= ~flags;

				if (flags & MAP_WRITE)		ptr->write = NULL;
				if (flags & MAP_READ)		ptr->read = NULL;
				if (flags & MAP_FETCHOP)	ptr->fetchop = NULL;
				if (flags & MAP_FETCHARG)	ptr->fetcharg = NULL;
			}

			break;
		}

		ptr++;
	}
}

void MAP_WRITENOP()
{
	mementry_ptr->flags |= MAP_WNOP | MAP_WRITE;
}

void MAP_READNOP()
{
	mementry_ptr->flags |= MAP_RNOP | MAP_READ;
}

void MAP_NOP()
{
	mementry_ptr->flags |= MAP_RNOP | MAP_WNOP | MAP_RAM;
}

void MAP_WRITEONLY()
{
	mementry_ptr->flags &= ~(MAP_RAM);
	mementry_ptr->flags |= MAP_WRITE;
}

void MAP_READONLY()
{
	mementry_ptr->flags &= ~(MAP_RAM);
	mementry_ptr->flags |= MAP_ROM;
}

void MAP_RW(UINT32 (*read)(UINT32 offset), void (*write)(UINT32 offset, UINT32 mask, UINT32 data))
{
	mementry_ptr->write = write;
	mementry_ptr->read = read;

	if (write != NULL) mementry_ptr->flags |= MAP_WRITE;
	if (read != NULL) mementry_ptr->flags |= MAP_READ;

#if defined (FBA_DEBUG)
	if (write == NULL) bprintf (0, _T("map %d MAP_RW [Range %x-%x] passed NULL function.\n"), cpuentry_ptr->mapnumber, mementry_ptr->address_start, mementry_ptr->address_end);
	if (read == NULL) bprintf (0, _T("map %d MAP_RW [Range %x-%x] passed NULL function.\n"), cpuentry_ptr->mapnumber, mementry_ptr->address_start, mementry_ptr->address_end);
#endif
}

void MAP_DEVRW(INT32 chip, UINT32 (*read)(INT32 chip, UINT32 offset), void (*write)(INT32 chip, UINT32 offset, UINT32 mask, UINT32 data))
{
	mementry_ptr->devread = read;

	if (read != NULL) {
		mementry_ptr->flags |= MAP_READ | MAP_DEVICE;
		mementry_ptr->chip_select[WRITE] = chip;
	}

	mementry_ptr->devwrite = write;

	if (write != NULL) {
		mementry_ptr->flags |= MAP_WRITE | MAP_DEVICE;
		mementry_ptr->chip_select[READ] = chip;
	}

#if defined (FBA_DEBUG)
	if (write == NULL) bprintf (0, _T("map %d MAP_DEVRW [Range %x-%x] passed NULL function.\n"), cpuentry_ptr->mapnumber, mementry_ptr->address_start, mementry_ptr->address_end);
	if (read == NULL) bprintf (0, _T("map %d MAP_DEVRW [Range %x-%x] passed NULL function.\n"), cpuentry_ptr->mapnumber, mementry_ptr->address_start, mementry_ptr->address_end);
#endif
}

void MAP_W(void (*write)(UINT32 offset, UINT32 mask, UINT32 data))
{
	mementry_ptr->write = write;

	if (write != NULL) mementry_ptr->flags |= MAP_WRITE;

#if defined (FBA_DEBUG)
	if (write == NULL) bprintf (0, _T("map %d MAP_WU [Range %x-%x] passed NULL function.\n"), cpuentry_ptr->mapnumber, mementry_ptr->address_start, mementry_ptr->address_end);
#endif
}

void MAP_DEVW(INT32 chip, void (*write)(INT32 chip, UINT32 offset, UINT32 mask, UINT32 data))
{
	mementry_ptr->devwrite = write;

	if (write != NULL) {
		mementry_ptr->flags |= MAP_WRITE | MAP_DEVICE;
		mementry_ptr->chip_select[WRITE] = chip;
	}

#if defined (FBA_DEBUG)
	if (write == NULL) bprintf (0, _T("map %d MAP_W [Range %x-%x] passed NULL function.\n"), cpuentry_ptr->mapnumber, mementry_ptr->address_start, mementry_ptr->address_end);
#endif
}

void MAP_DEVR(INT32 chip, UINT32 (*read)(INT32 chip, UINT32 offset))
{
	mementry_ptr->devread = read;

	if (read != NULL) {
		mementry_ptr->flags |= MAP_READ | MAP_DEVICE;
		mementry_ptr->chip_select[READ] = chip;
	}

#if defined (FBA_DEBUG)
	if (read == NULL) bprintf (0, _T("map %d MAP_RU [Range %x-%x] passed NULL function.\n"), cpuentry_ptr->mapnumber, mementry_ptr->address_start, mementry_ptr->address_end);
#endif
}

void MAP_R(UINT32 (*read)(UINT32 offset))
{
	mementry_ptr->read = read;

	if (read != NULL) mementry_ptr->flags |= MAP_READ;

#if defined (FBA_DEBUG)
	if (read == NULL) bprintf (0, _T("map %d MAP_R [Range %x-%x] passed NULL function.\n"), cpuentry_ptr->mapnumber, mementry_ptr->address_start, mementry_ptr->address_end);
#endif
}

void MAP_F(UINT32 (*fetch)(UINT32 offset))
{
	mementry_ptr->fetchop = fetch;
	mementry_ptr->fetcharg = fetch;
	if (fetch != NULL) mementry_ptr->flags |= MAP_FETCH;

#if defined (FBA_DEBUG)
	if (fetch == NULL) bprintf (0, _T("map %d MAP_F [Range %x-%x] passed NULL function.\n"), cpuentry_ptr->mapnumber, mementry_ptr->address_start, mementry_ptr->address_end);
#endif
}

void MAP_FO(UINT32 (*fetchop)(UINT32 offset))
{
	mementry_ptr->fetchop = fetchop;
	if (fetchop != NULL) mementry_ptr->flags |= MAP_FETCHOP;

#if defined (FBA_DEBUG)
	if (fetchop == NULL) bprintf (0, _T("map %d MAP_FO [Range %x-%x] passed NULL function.\n"), cpuentry_ptr->mapnumber, mementry_ptr->address_start, mementry_ptr->address_end);
#endif
}

void MAP_FA(UINT32 (*fetcharg)(UINT32 offset))
{
	mementry_ptr->fetcharg = fetcharg;
	if (fetcharg != NULL) mementry_ptr->flags |= MAP_FETCHARG;

#if defined (FBA_DEBUG)
	if (fetcharg == NULL) bprintf (0, _T("map %d MAP_FA [Range %x-%x] passed NULL function.\n"), cpuentry_ptr->mapnumber, mementry_ptr->address_start, mementry_ptr->address_end);
#endif
}

void BurnMapSetBankWrite(INT32 map_num, INT32 bank, void (*write)(UINT32,UINT32,UINT32))
{
#if defined (FBA_DEBUG)
	if (map_num < 0 || map_num >= MAX_CPUS) {
		bprintf (0, _T("BurnMapSetBankWrite called with invalid map number (%d)!\n"), map_num);
		return;
	}

	if (write == NULL) {
		bprintf (0, _T("map %d BurnMapSetBankWrite (bank %d) function is NULL!\n"), map_num, bank);
		return;
	}
#endif

	struct cpumap_entry *cptr = &cpuentries[map_num];

#if defined (FBA_DEBUG)
	if (cptr->initialized == 0) {
		bprintf (0, _T("map %d BurnMapSetBankWrite (bank %d) map is not initialized!\n"), map_num, bank);
		return;
	}
#endif

	for (INT32 i = cpuentry_ptr->entry_start_offset; i < BURNMAP_ENTRIES; i++)
	{
		struct memory_entry *ptr = &cptr->mem_entries[i];
		if (ptr->flags == MAP_ENDMARKER) break;

		if (ptr->flags & MAP_MBANK) {
			if (ptr->bank == bank) {
				ptr->write = write;
				return;
			}
		}
	}

#if defined (FBA_DEBUG)
	bprintf (0, _T("map %d BurnMapSetBankWrite (bank %d) could not find bank!\n"), map_num, bank);
#endif
}

void BurnMapSetBankRead(INT32 map_num, INT32 bank, UINT32 (*read)(UINT32))
{
#if defined (FBA_DEBUG)
	if (map_num < 0 || map_num >= MAX_CPUS) {
		bprintf (0, _T("BurnMapSetBankRead called with invalid map number (%d)!\n"), map_num);
		return;
	}

	if (read == NULL) {
		bprintf (0, _T("map %d BurnMapSetBankRead (bank %d) function is NULL!\n"), map_num, bank);
		return;
	}
#endif

	struct cpumap_entry *cptr = &cpuentries[map_num];

#if defined (FBA_DEBUG)
	if (cptr->initialized == 0) {
		bprintf (0, _T("map %d BurnMapSetBankRead(bank %d) map is not initialized!\n"), map_num, bank);
		return;
	}
#endif

	for (INT32 i = cpuentry_ptr->entry_start_offset; i < BURNMAP_ENTRIES; i++)
	{
		struct memory_entry *ptr = &cptr->mem_entries[i];
		if (ptr->flags == MAP_ENDMARKER) break;

		if (ptr->flags & MAP_MBANK) {
			if (ptr->bank == bank) {
				ptr->read = read;
				return;
			}
		}
	}

#if defined (FBA_DEBUG)
	bprintf (0, _T("map %d BurnMapSetBankRead (bank %d) could not find bank!\n"), map_num, bank);
#endif
}

void BurnMapSetBankRegion(INT32 map_num, INT32 bank, UINT8 *memptr)
{
#if defined (FBA_DEBUG)
	if (map_num < 0 || map_num >= MAX_CPUS) {
		bprintf (0, _T("BurnMapSetBankRegion (bank %d) called with invalid map number (%d)!\n"), bank, map_num);
		return;
	}

	if (memptr == NULL) {
		bprintf (0, _T("map %d BurnMapSetBankRegion (bank %d) called with NULL memptr!\n"), map_num, bank);
		return;
	}
#endif

	struct cpumap_entry *cptr = &cpuentries[map_num];

#if defined (FBA_DEBUG)
	if (cptr->initialized == 0) {
		bprintf (0, _T("map %d BurnMapSetBankRegion (bank %d) map is not initialized!\n"), map_num, bank);
		return;
	}
#endif

	for (INT32 i = cpuentry_ptr->entry_start_offset; i < BURNMAP_ENTRIES; i++)
	{
		struct memory_entry *ptr = &cptr->mem_entries[i];
		if (ptr->flags == MAP_ENDMARKER) break;

		if (ptr->flags & MAP_MBANK) {
			if (ptr->bank == bank) {
				if (ptr->flags & MAP_READ)		ptr->memptr[READ] = memptr;
				if (ptr->flags & MAP_WRITE)		ptr->memptr[WRITE] = memptr;
				if (ptr->flags & MAP_FETCHOP)	ptr->memptr[FETCHOP] = memptr;
				if (ptr->flags & MAP_FETCHARG)	ptr->memptr[FETCHARG] = memptr;
				return;
			}
		}
	}

#if defined (FBA_DEBUG)
	bprintf (0, _T("BurnMapSetBankRegion (map %d, bank %d) could not find bank!\n"), map_num, bank);
#endif
}

void BurnMapAddWrite(INT32 map_num, INT32 address_start, INT32 address_end, void (*write)(UINT32,UINT32,UINT32))
{
#if defined (FBA_DEBUG)
	if (map_num < 0 || map_num >= MAX_CPUS) {
		bprintf (0, _T("BurnMapAddWrite called with invalid map number (%d)!\n"), map_num);
		return;
	}

	if (address_start > address_end) {
		bprintf (0, _T("map %d BurnMapAddWrite [Range %x-%x] check address range.\n"), map_num, address_start, address_end);
		return;
	}

	if (write == NULL) {
		bprintf (0, _T("map %d BurnMapAddWrite [Range %x-%x] passed NULL function.\n"), map_num, address_start, address_end);
		return;
	}
#endif

	struct cpumap_entry *cptr= &cpuentries[map_num];

#if defined (FBA_DEBUG)
	if (cptr->initialized == 0) {
		bprintf (0, _T("map %d BurnMapAddWrite map is not initialized!\n"), map_num);
		return;
	}
#endif

	struct memory_entry *ptr = &cptr->mem_entries[cptr->entry_start_offset - 1];

	ptr->flags |= MAP_WRITE;
	ptr->address_start = address_start;
	ptr->address_end = address_end;
	ptr->write = write;

	ptr->mirror_mask_byte = cptr->MEMMAP_MASK;
	ptr->mirror_mask_word = cptr->MEMMAP_MASK & (cptr->MEMMAP_ALIGNBYTES ? ~1 : ~0);
	ptr->mirror_mask_long = cptr->MEMMAP_MASK & (cptr->MEMMAP_ALIGNBYTES ? ~3 : ~0);

	cptr->entry_start_offset--;
}

void BurnMapAddRead(INT32 map_num, INT32 address_start, INT32 address_end, UINT32 (*read)(UINT32))
{
#if defined (FBA_DEBUG)
	if (map_num < 0 || map_num >= MAX_CPUS) {
		bprintf (0, _T("BurnMapAddRead called with invalid map number (%d)!\n"), map_num);
		return;
	}

	if (address_start > address_end) {
		bprintf (0, _T("map %d BurnMapAddRead [Range %x-%x] check address range.\n"), map_num, address_start, address_end);
		return;
	}

	if (read == NULL) {
		bprintf (0, _T("map %d BurnMapAddRead [Range %x-%x] passed NULL function.\n"), map_num, address_start, address_end);
		return;
	}
#endif

	struct cpumap_entry *cptr= &cpuentries[map_num];

#if defined (FBA_DEBUG)
	if (cptr->initialized == 0) {
		bprintf (0, _T("map %d BurnMapAddRead map is not initialized!\n"), map_num);
		return;
	}
#endif

	struct memory_entry *ptr = &cptr->mem_entries[cptr->entry_start_offset - 1];

	ptr->flags |= MAP_READ;
	ptr->address_start = address_start;
	ptr->address_end = address_end;
	ptr->read = read;

	ptr->mirror_mask_byte = cptr->MEMMAP_MASK;
	ptr->mirror_mask_word = cptr->MEMMAP_MASK & (cptr->MEMMAP_ALIGNBYTES ? ~1 : ~0);
	ptr->mirror_mask_long = cptr->MEMMAP_MASK & (cptr->MEMMAP_ALIGNBYTES ? ~3 : ~0);

	cptr->entry_start_offset--;
}

void BurnMapAddReadWrite(INT32 map_num, INT32 address_start, INT32 address_end, UINT32 (*read)(UINT32), void (*write)(UINT32,UINT32,UINT32))
{
#if defined (FBA_DEBUG)
	if (map_num < 0 || map_num >= MAX_CPUS) {
		bprintf (0, _T("BurnMapAddReadWrite called with invalid map number (%d)!\n"), map_num);
		return;
	}

	if (address_start > address_end) {
		bprintf (0, _T("map %d BurnMapAddReadWrite [Range %x-%x] check address range.\n"), map_num, address_start, address_end);
		return;
	}

	if (read == NULL || write == NULL) {
		bprintf (0, _T("map %d BurnMapAddReadWrite [Range %x-%x] passed NULL function.\n"), map_num, address_start, address_end);
		return;
	}
#endif

	struct cpumap_entry *cptr= &cpuentries[map_num];

#if defined (FBA_DEBUG)
	if (cptr->initialized == 0) {
		bprintf (0, _T("map %d BurnMapAddReadWrite map is not initialized!\n"), map_num);
		return;
	}
#endif

	struct memory_entry *ptr = &cptr->mem_entries[cptr->entry_start_offset - 1];

	ptr->flags |= MAP_WRITE |  MAP_READ;
	ptr->address_start = address_start;
	ptr->address_end = address_end;
	ptr->read = read;
	ptr->write = write;

	ptr->mirror_mask_byte = cptr->MEMMAP_MASK;
	ptr->mirror_mask_word = cptr->MEMMAP_MASK & (cptr->MEMMAP_ALIGNBYTES ? ~1 : ~0);
	ptr->mirror_mask_long = cptr->MEMMAP_MASK & (cptr->MEMMAP_ALIGNBYTES ? ~3 : ~0);

	cptr->entry_start_offset--;
}

void BurnMapAddDeviceWrite(INT32 map_num, INT32 address_start, INT32 address_end, INT32 chip, void (*write)(INT32,UINT32,UINT32,UINT32))
{
#if defined (FBA_DEBUG)
	if (map_num < 0 || map_num >= MAX_CPUS) {
		bprintf (0, _T("BurnMapAddDeviceWrite called with invalid map number (%d)!\n"), map_num);
		return;
	}

	if (address_start > address_end) {
		bprintf (0, _T("map %d BurnMapAddDeviceWrite [Range %x-%x] check address range.\n"), map_num, address_start, address_end);
		return;
	}

	if (write == NULL) {
		bprintf (0, _T("map %d BurnMapAddDeviceWrite [Range %x-%x] passed NULL function.\n"), map_num, address_start, address_end);
		return;
	}
#endif

	struct cpumap_entry *cptr= &cpuentries[map_num];

#if defined (FBA_DEBUG)
	if (cptr->initialized == 0) {
		bprintf (0, _T("map %d BurnMapAddDeviceWrite map is not initialized!\n"), map_num);
		return;
	}
#endif

	struct memory_entry *ptr = &cptr->mem_entries[cptr->entry_start_offset - 1];

	ptr->flags |= MAP_WRITE;
	ptr->address_start = address_start;
	ptr->address_end = address_end;
	ptr->devwrite = write;
	ptr->chip_select[WRITE] = chip;

	ptr->mirror_mask_byte = cptr->MEMMAP_MASK;
	ptr->mirror_mask_word = cptr->MEMMAP_MASK & (cptr->MEMMAP_ALIGNBYTES ? ~1 : ~0);
	ptr->mirror_mask_long = cptr->MEMMAP_MASK & (cptr->MEMMAP_ALIGNBYTES ? ~3 : ~0);

	cptr->entry_start_offset--;
}

void BurnMapAddDeviceRead(INT32 map_num, INT32 address_start, INT32 address_end, INT32 chip, UINT32 (*read)(INT32,UINT32))
{
#if defined (FBA_DEBUG)
	if (map_num < 0 || map_num >= MAX_CPUS) {
		bprintf (0, _T("BurnMapAddDeviceRead called with invalid map number (%d)!\n"), map_num);
		return;
	}

	if (address_start > address_end) {
		bprintf (0, _T("map %d BurnMapAddDeviceRead [Range %x-%x] check address range.\n"), map_num, address_start, address_end);
		return;
	}

	if (read == NULL) {
		bprintf (0, _T("map %d BurnMapAddDeviceRead [Range %x-%x] passed NULL function.\n"), map_num, address_start, address_end);
		return;
	}
#endif

	struct cpumap_entry *cptr= &cpuentries[map_num];

#if defined (FBA_DEBUG)
	if (cptr->initialized == 0) {
		bprintf (0, _T("map %d BurnMapAddDeviceRead map is not initialized!\n"), map_num);
		return;
	}
#endif

	struct memory_entry *ptr = &cptr->mem_entries[cptr->entry_start_offset - 1];

	ptr->flags |= MAP_READ;
	ptr->address_start = address_start;
	ptr->address_end = address_end;
	ptr->devread = read;
	ptr->chip_select[READ] = chip;

	ptr->mirror_mask_byte = cptr->MEMMAP_MASK;
	ptr->mirror_mask_word = cptr->MEMMAP_MASK & (cptr->MEMMAP_ALIGNBYTES ? ~1 : ~0);
	ptr->mirror_mask_long = cptr->MEMMAP_MASK & (cptr->MEMMAP_ALIGNBYTES ? ~3 : ~0);

	cptr->entry_start_offset--;
}

void BurnMapAddDeviceReadWrite(INT32 map_num, INT32 address_start, INT32 address_end, INT32 chip, UINT32 (*read)(INT32,UINT32), void (*write)(INT32,UINT32,UINT32,UINT32))
{
#if defined (FBA_DEBUG)
	if (map_num < 0 || map_num >= MAX_CPUS) {
		bprintf (0, _T("BurnMapAddDeviceWrite called with invalid map number (%d)!\n"), map_num);
		return;
	}

	if (address_start > address_end) {
		bprintf (0, _T("map %d BurnMapAddDeviceReadWrite [Range %x-%x] check address range.\n"), map_num, address_start, address_end);
		return;
	}

	if (read == NULL || write == NULL) {
		bprintf (0, _T("map %d BurnMapAddDeviceReadWrite [Range %x-%x] passed NULL function.\n"), map_num, address_start, address_end);
		return;
	}
#endif

	struct cpumap_entry *cptr= &cpuentries[map_num];

#if defined (FBA_DEBUG)
	if (cptr->initialized == 0) {
		bprintf (0, _T("map %d BurnMapAddDeviceWrite map is not initialized!\n"), map_num);
		return;
	}
#endif

	struct memory_entry *ptr = &cptr->mem_entries[cptr->entry_start_offset - 1];

	ptr->flags |= MAP_WRITE |  MAP_READ;
	ptr->address_start = address_start;
	ptr->address_end = address_end;
	ptr->devread = read;
	ptr->devwrite = write;
	ptr->chip_select[READ] = chip;
	ptr->chip_select[WRITE] = chip;

	ptr->mirror_mask_byte = cptr->MEMMAP_MASK;
	ptr->mirror_mask_word = cptr->MEMMAP_MASK & (cptr->MEMMAP_ALIGNBYTES ? ~1 : ~0);
	ptr->mirror_mask_long = cptr->MEMMAP_MASK & (cptr->MEMMAP_ALIGNBYTES ? ~3 : ~0);

	cptr->entry_start_offset--;
}

void BurnMapAddRegion(INT32 map_num, INT32 address_start, INT32 address_end, UINT8 *memptr, UINT32 flags)
{
#if defined (FBA_DEBUG)
	if (address_start > address_end) {
		bprintf (0, _T("map %d BurnMapAddRegion [Range %x-%x] check address range.\n"), map_num, address_start, address_end);
		return;
	}

	if (memptr == NULL) {
		bprintf (0, _T("map %d BurnMapAddRegion [Range %x-%x] passed NULL mem pointer (memptr).\n"), map_num, address_start, address_end);
		return;
	}

	if (flags == 0) {
		bprintf (0, _T("map %d BurnMapAddRegion [Range %x-%x] flags are 0! This will have no function.\n"), map_num, address_start, address_end);
		return;
	}
#endif

	struct cpumap_entry *cptr= &cpuentries[map_num];

#if defined (FBA_DEBUG)
	if (cptr->initialized == 0) {
		bprintf (0, _T("map %d BurnMapAddDeviceWrite map is not initialized!\n"), map_num);
		return;
	}
#endif

	struct memory_entry *ptr = &cptr->mem_entries[cptr->entry_start_offset - 1];

	ptr->flags |= flags & MAP_RAM;
	ptr->address_start = address_start;
	ptr->address_end = address_end;
	if (ptr->flags & MAP_READ)		ptr->memptr[READ] = memptr;
	if (ptr->flags & MAP_WRITE)		ptr->memptr[WRITE] = memptr;
	if (ptr->flags & MAP_FETCHOP)	ptr->memptr[FETCHOP] = memptr;
	if (ptr->flags & MAP_FETCHARG)	ptr->memptr[FETCHARG] = memptr;

	ptr->mirror_mask_byte = cptr->MEMMAP_MASK;
	ptr->mirror_mask_word = cptr->MEMMAP_MASK & (cptr->MEMMAP_ALIGNBYTES ? ~1 : ~0);
	ptr->mirror_mask_long = cptr->MEMMAP_MASK & (cptr->MEMMAP_ALIGNBYTES ? ~3 : ~0);

	cptr->entry_start_offset--;
}

/*----------------------------------------------------------------------------------------*\
| CPU core functions
|	call from cpu
\*----------------------------------------------------------------------------------------*/

void BurnMapGetMapPtr(cpumap_entry **ptr, INT32 map_num)
{
#if defined (FBA_DEBUG)
	if (map_num < 0 || map_num >= MAX_CPUS) {
		bprintf (0, _T("BurnMapGetMapPtr called with invalid map number (%d)!\n"), map_num);
		return;
	}
#endif

	struct cpumap_entry *cptr = &cpuentries[map_num];

#if defined (FBA_DEBUG)
	if (cptr->initialized == 0) {
		bprintf (0, _T("map %d BurnMapGetMapPtr map is not initialized!\n"), map_num);
		return;
	}
#endif

	*ptr = cptr;
}

void BurnMapCheatWriteByte(cpumap_entry *cptr, UINT32 address, UINT8 data)
{
	struct memory_entry *ptr = &cptr->mem_entries[cpuentry_ptr->entry_start_offset];

	while (ptr->flags != MAP_ENDMARKER)
	{
		UINT32 addr = address & ptr->mirror_mask_byte;

		if (ptr->address_start <= addr && ptr->address_end >= addr)
		{
			if (ptr->write) {
				UINT32 offset = addr - ptr->address_start;
				INT32 shift = ((offset & (cptr->MEMMAP_DATABITS - 1)) * 8);
				ptr->write(offset, ~(0xff << shift), data << shift);
				return;
			}

			if (ptr->memptr[READ])	ptr->memptr[READ][addr - ptr->address_start] = data;
			if (ptr->memptr[WRITE])	ptr->memptr[WRITE][addr - ptr->address_start] = data;
			if (ptr->memptr[FETCHOP])	ptr->memptr[FETCHOP][addr - ptr->address_start] = data;
			if (ptr->memptr[FETCHARG])	ptr->memptr[FETCHARG][addr - ptr->address_start] = data;
		}

		ptr++;
	}

#if defined (FBA_DEBUG)
	bprintf (0, _T("Unmapped cheat byte write: 0x%8.8x, 0x%2.2x\n"), address, data);
#endif
}

void BurnMapWriteByte(cpumap_entry *cptr, UINT32 address, UINT8 data)
{
	struct memory_entry *ptr = &cptr->mem_entries[cpuentry_ptr->entry_start_offset];

	while (ptr->flags != MAP_ENDMARKER)
	{
		if (ptr->flags & MAP_WRITE)
		{
			UINT32 addr = address & ptr->mirror_mask_byte;

			if (ptr->address_start <= addr && ptr->address_end >= addr)
			{
				if (ptr->write) {
					UINT32 offset = addr - ptr->address_start;
					INT32 shift = ((offset & (cptr->MEMMAP_DATABITS - 1)) * 8);
					ptr->write(offset, ~(0xff << shift), data << shift);
					return;
				}

				if (ptr->devwrite) {
					UINT32 offset = addr - ptr->address_start;
					INT32 shift = ((offset & (cptr->MEMMAP_DATABITS - 1)) * 8);
					ptr->devwrite(ptr->chip_select[WRITE], offset, ~(0xff << shift), data << shift);
					return;
				}

				if (ptr->memptr[WRITE]) {
					ptr->memptr[WRITE][addr - ptr->address_start] = data;
					return;
				}

				if (ptr->flags & MAP_WNOP) return;
			}
		}

		ptr++;
	}

#if defined (FBA_DEBUG)
	bprintf (0, _T("Unmapped byte write: %d, 0x%8.8x, 0x%2.2x\n"), cptr->mapnumber, address & cptr->MEMMAP_MASK, data);
#endif
}

void BurnMapWriteWord(cpumap_entry *cptr, UINT32 address, UINT16 data)
{
	struct memory_entry *ptr = &cptr->mem_entries[cpuentry_ptr->entry_start_offset];

	while (ptr->flags != MAP_ENDMARKER)
	{
		if (ptr->flags & MAP_WRITE)
		{
			UINT32 addr = address & ptr->mirror_mask_word;

			if (ptr->address_start <= addr && ptr->address_end >= addr)
			{
				if (ptr->write) {
					UINT32 offset = addr - ptr->address_start;
					INT32 shift = ((offset & (cptr->MEMMAP_DATABITS - 1) & 2) * 8);
					ptr->write(offset, ~(0xffff << shift), data << shift);
					return;
				}

				if (ptr->devwrite) {
					UINT32 offset = addr - ptr->address_start;
					INT32 shift = ((offset & (cptr->MEMMAP_DATABITS - 1) & 2) * 8);
					ptr->devwrite(ptr->chip_select[WRITE], offset, ~(0xffff << shift), data << shift);
					return;
				}

				if (ptr->memptr[WRITE]) {
					UINT8 *p = ptr->memptr[WRITE] + (addr - ptr->address_start);
					*((UINT16*)p) = data;
					return;
				}

				if (ptr->flags & MAP_WNOP) return;
			}
		}

		ptr++;
	}

#if defined (FBA_DEBUG)
	bprintf (0, _T("Unmapped word write: %d, 0x%8.8x, 0x%4.4x\n"), cptr->mapnumber, address & cptr->MEMMAP_MASK, data);
#endif
}

void BurnMapWriteLong(cpumap_entry *cptr, UINT32 address, UINT32 data)
{
	struct memory_entry *ptr = &cptr->mem_entries[cpuentry_ptr->entry_start_offset];

	while (ptr->flags != MAP_ENDMARKER)
	{
		if (ptr->flags & MAP_WRITE)
		{
			UINT32 addr = address & ptr->mirror_mask_long;

			if (ptr->address_start <= addr && ptr->address_end >= addr)
			{
				if (ptr->write) {
					ptr->write(addr - ptr->address_start, 0, data);
					return;
				}

				if (ptr->devwrite) {
					ptr->devwrite(ptr->chip_select[WRITE], addr - ptr->address_start, 0, data);
					return;
				}

				if (ptr->memptr[WRITE]) {
					UINT8 *p = ptr->memptr[WRITE] + (addr - ptr->address_start);
					*((UINT32*)p) = data;
					return;
				}

				if (ptr->flags & MAP_WNOP) return;
			}
		}

		ptr++;
	}

#if defined (FBA_DEBUG)
	bprintf (0, _T("Unmapped long write: %d, 0x%8.8x, 0x%8.8x\n"), cptr->mapnumber, address & cptr->MEMMAP_MASK, data);
#endif
}

UINT8 BurnMapReadByte(cpumap_entry *cptr, UINT32 address)
{
	struct memory_entry *ptr = &cptr->mem_entries[cpuentry_ptr->entry_start_offset];

	while (ptr->flags != MAP_ENDMARKER)
	{
		if (ptr->flags & MAP_READ)
		{
			UINT32 addr = address & ptr->mirror_mask_byte;

			if (ptr->address_start <= addr && ptr->address_end >= addr)
			{
				if (ptr->read) {
					UINT32 offset = addr - ptr->address_start;
					return ptr->read(offset) >> ((offset & (cptr->MEMMAP_DATABITS - 1)) * 8);
				}

				if (ptr->devread) {
					UINT32 offset = addr - ptr->address_start;
					return ptr->devread(ptr->chip_select[READ], offset) >> ((offset & (cptr->MEMMAP_DATABITS - 1)) * 8);
				}

				if (ptr->memptr[READ]) {
					return ptr->memptr[READ][addr - ptr->address_start];
				}

				if (ptr->flags & MAP_RNOP) return 0;
			}
		}

		ptr++;

	}

#if defined (FBA_DEBUG)
	bprintf (0, _T("Unmapped byte read: %d, 0x%5.5x\n"), cptr->mapnumber, address & cptr->MEMMAP_MASK);
#endif

	return 0;
}

UINT16 BurnMapReadWord(cpumap_entry *cptr, UINT32 address)
{
	struct memory_entry *ptr = &cptr->mem_entries[cpuentry_ptr->entry_start_offset];

	while (ptr->flags != MAP_ENDMARKER)
	{
		if (ptr->flags & MAP_READ)
		{
			UINT32 addr = address & ptr->mirror_mask_word;

			if (ptr->address_start <= addr && ptr->address_end >= addr)
			{
				if (ptr->read) {
					UINT32 offset = addr - ptr->address_start;
					return ptr->read(offset) >> ((offset & (cptr->MEMMAP_DATABITS - 1) & 2) * 8);
				}

				if (ptr->devread) {
					UINT32 offset = addr - ptr->address_start;
					return ptr->devread(ptr->chip_select[READ], offset) >> ((offset & (cptr->MEMMAP_DATABITS - 1) & 2) * 8);
				}

				if (ptr->memptr[READ]) {
					UINT8 *p = ptr->memptr[READ] + (addr - ptr->address_start);
					return *((UINT16*)p);
				}

				if (ptr->flags & MAP_RNOP) return 0;
			}
		}

		ptr++;
	}

#if defined (FBA_DEBUG)
	bprintf (0, _T("Unmapped word read: %d, 0x%5.5x\n"), cptr->mapnumber, address & cptr->MEMMAP_MASK);
#endif

	return 0;
}

UINT32 BurnMapReadLong(cpumap_entry *cptr, UINT32 address)
{
	struct memory_entry *ptr = &cptr->mem_entries[cpuentry_ptr->entry_start_offset];

	while (ptr->flags != MAP_ENDMARKER)
	{
		if (ptr->flags & MAP_READ)
		{
			UINT32 addr = address & ptr->mirror_mask_long;

			if (ptr->address_start <= addr && ptr->address_end >= addr)
			{
				if (ptr->read) {
					return ptr->read(addr - ptr->address_start);
				}

				if (ptr->devread) {
					return ptr->devread(ptr->chip_select[READ], addr - ptr->address_start);
				}

				if (ptr->memptr[READ]) {
					UINT8 *p = ptr->memptr[READ] + (addr - ptr->address_start);
					return *((UINT32*)p);
				}

				if (ptr->flags & MAP_RNOP) return 0;
			}
		}

		ptr++;
	}

#if defined (FBA_DEBUG)
	bprintf (0, _T("Unmapped long read: %d, 0x%5.5x\n"), cptr->mapnumber, address & cptr->MEMMAP_MASK);
#endif

	return 0;
}

UINT8 BurnMapFetchOpByte(cpumap_entry *cptr, UINT32 address)
{
	struct memory_entry *ptr = &cptr->mem_entries[cpuentry_ptr->entry_start_offset];

	while (ptr->flags != MAP_ENDMARKER)
	{
		if (ptr->flags & MAP_FETCHOP)
		{
			UINT32 addr = address & ptr->mirror_mask_byte;

			if (ptr->address_start <= addr && ptr->address_end >= addr)
			{
				if (ptr->memptr[FETCHOP]) {
					return ptr->memptr[FETCHOP][addr - ptr->address_start];
				}

				if (ptr->fetchop) {
					UINT32 offset = addr - ptr->address_start;
					return ptr->fetchop(offset) >> ((offset & (cptr->MEMMAP_DATABITS - 1)) * 8);
				}
			}
		}

		ptr++;
	}

#if defined (FBA_DEBUG)
	bprintf (0, _T("Unmapped byte fetch op: %d, 0x%5.5x\n"), cptr->mapnumber, address & cptr->MEMMAP_MASK);
#endif

	return 0;
}

UINT16 BurnMapFetchOpWord(cpumap_entry *cptr, UINT32 address)
{
	struct memory_entry *ptr = &cptr->mem_entries[cpuentry_ptr->entry_start_offset];

	while (ptr->flags != MAP_ENDMARKER)
	{
		if (ptr->flags & MAP_FETCHOP)
		{
			UINT32 addr = address & ptr->mirror_mask_word;

			if (ptr->address_start <= addr && ptr->address_end >= addr)
			{
				if (ptr->memptr[FETCHOP]) {
					UINT8 *p = ptr->memptr[FETCHOP] + (addr - ptr->address_start);
					return *((UINT16*)p);
				}

				if (ptr->fetchop) {
					UINT32 offset = addr - ptr->address_start;
					return ptr->fetchop(offset) >> ((offset & (cptr->MEMMAP_DATABITS - 1) & 2) * 8);
				}
			}
		}

		ptr++;
	}

#if defined (FBA_DEBUG)
	bprintf (0, _T("Unmapped word fetch op: %d, 0x%5.5x\n"), cptr->mapnumber, address & cptr->MEMMAP_MASK);
#endif

	return 0;
}

UINT32 BurnMapFetchOpLong(cpumap_entry *cptr, UINT32 address)
{
	struct memory_entry *ptr = &cptr->mem_entries[cpuentry_ptr->entry_start_offset];

	while (ptr->flags != MAP_ENDMARKER)
	{
		if (ptr->flags & MAP_FETCHOP)
		{
			UINT32 addr = address & ptr->mirror_mask_long;

			if (ptr->address_start <= addr && ptr->address_end >= addr)
			{
				if (ptr->memptr[FETCHOP]) {
					UINT8 *p = ptr->memptr[FETCHOP] + (addr - ptr->address_start);
					return *((UINT32*)p);
				}

				if (ptr->fetchop) {
					return ptr->fetchop(addr - ptr->address_start);
				}
			}
		}

		ptr++;
	}

#if defined (FBA_DEBUG)
	bprintf (0, _T("Unmapped long fetch op: %d, 0x%5.5x\n"), cptr->mapnumber, address & cptr->MEMMAP_MASK);
#endif

	return 0;
}

UINT8 BurnMapFetchArgByte(cpumap_entry *cptr, UINT32 address)
{
	struct memory_entry *ptr = &cptr->mem_entries[cpuentry_ptr->entry_start_offset];

	while (ptr->flags != MAP_ENDMARKER)
	{
		if (ptr->flags & MAP_FETCHARG)
		{
			UINT32 addr = address & ptr->mirror_mask_byte;

			if (ptr->address_start <= addr && ptr->address_end >= addr)
			{
				if (ptr->memptr[FETCHARG]) {
					return ptr->memptr[FETCHARG][addr - ptr->address_start];
				}

				if (ptr->fetcharg) {
					UINT32 offset = addr - ptr->address_start;
					return ptr->fetcharg(offset) >> ((offset & (cptr->MEMMAP_DATABITS - 1)) * 8);
				}
			}
		}

		ptr++;
	}

#if defined (FBA_DEBUG)
	bprintf (0, _T("Unmapped byte fetch arg: %d, 0x%5.5x\n"), cptr->mapnumber, address & cptr->MEMMAP_MASK);
#endif

	return 0;
}

UINT16 BurnMapFetchArgWord(cpumap_entry *cptr, UINT32 address)
{
	struct memory_entry *ptr = &cptr->mem_entries[cpuentry_ptr->entry_start_offset];

	while (ptr->flags != MAP_ENDMARKER)
	{
		if (ptr->flags & MAP_FETCHARG)
		{
			UINT32 addr = address & ptr->mirror_mask_word;

			if (ptr->address_start <= addr && ptr->address_end >= addr)
			{
				if (ptr->memptr[FETCHARG]) {
					UINT8 *p = ptr->memptr[FETCHARG] + (addr - ptr->address_start);
					return *((UINT16*)p);
				}

				if (ptr->fetcharg) {
					UINT32 offset = addr - ptr->address_start;
					return ptr->fetcharg(offset) >> ((offset & (cptr->MEMMAP_DATABITS - 1) & 2) * 8);
				}
			}
		}

		ptr++;
	}

#if defined (FBA_DEBUG)
	bprintf (0, _T("Unmapped word fetch arg: %d, 0x%5.5x\n"), cptr->mapnumber, address & cptr->MEMMAP_MASK);
#endif

	return 0;
}

UINT32 BurnMapFetchArgLong(cpumap_entry *cptr, UINT32 address)
{
	struct memory_entry *ptr = &cptr->mem_entries[cpuentry_ptr->entry_start_offset];

	while (ptr->flags != MAP_ENDMARKER)
	{
		if (ptr->flags & MAP_FETCHARG)
		{
			UINT32 addr = address & ptr->mirror_mask_long;

			if (ptr->address_start <= addr && ptr->address_end >= addr)
			{
				if (ptr->memptr[FETCHARG]) {
					UINT8 *p = ptr->memptr[FETCHARG] + (addr - ptr->address_start);
					return *((UINT32*)p);
				}

				if (ptr->fetcharg) {
					return ptr->fetcharg(addr - ptr->address_start);
				}
			}
		}

		ptr++;
	}

#if defined (FBA_DEBUG)
	bprintf (0, _T("Unmapped long fetch arg: %d, 0x%5.5x\n"), cptr->mapnumber, address & cptr->MEMMAP_MASK);
#endif

	return 0;
}

void BurnMapScan(INT32 nAction, INT32 *)
{
	struct BurnArea ba;

	for (INT32 i = 0; i < MAX_CPUS; i++)
	{
		struct cpumap_entry *cptr = &cpuentries[i];

		for (INT32 j = cptr->entry_start_offset; j < BURNMAP_ENTRIES; j++)
		{
			struct memory_entry *ptr = &cptr->mem_entries[j];
			if (ptr->flags == MAP_ENDMARKER) break;

			if ((nAction & ACB_MEMORY_RAM) && (ptr->flags & MAP_ALLOC) && (ptr->flags & MAP_NVALLOC) == 0)
			{
				ba.Data		= ptr->memptr[READ];
				ba.nLen		= ptr->address_end - ptr->address_start;
				ba.nAddress	= ptr->address_start;
				ba.szName	= "Auto Allocated RAM";
				BurnAcb(&ba);
			}

			if ((nAction & ACB_NVRAM) && (ptr->flags & MAP_ALLOC) && (ptr->flags & MAP_NVALLOC))
			{
				ba.Data		= ptr->memptr[READ];
				ba.nLen		= ptr->address_end - ptr->address_start;
				ba.nAddress	= ptr->address_start;
				ba.szName	= "Auto Allocated RAM";
				BurnAcb(&ba);
			}

			if ((nAction & ACB_DRIVER_DATA) && (ptr->memptr[1] != NULL) && (ptr->flags & MAP_RAM) && (ptr->flags & MAP_ALLOC) == 0)
			{
				ba.Data		= ptr->memptr[WRITE];
				ba.nLen		= ptr->address_end - ptr->address_start;
				ba.nAddress	= ptr->address_start;
				ba.szName	= "Auto Allocated Object";
				BurnAcb(&ba);
			}
		}
	}
}
