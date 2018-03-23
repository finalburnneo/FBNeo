#ifndef BURNMAP_CORE
#define BURNMAP_CORE

#define BURNMAP_START_OFFSET	32
#define BURNMAP_ENTRIES			(128+BURNMAP_START_OFFSET)

struct memory_entry {
	UINT32 address_start;
	UINT32 address_end;
	UINT32 flags;
	UINT8  *memptr[4];
	INT32 chip_select[2];

	void  (*devwrite)(INT32 chip, UINT32 offset, UINT32 mask, UINT32 data);
	UINT32 (*devread)(INT32 chip, UINT32 offset);

	void   (*write)(UINT32 offset, UINT32 mask, UINT32 data);
	UINT32 (*read)(UINT32 offset);
	UINT32 (*fetchop)(UINT32 offset);
	UINT32 (*fetcharg)(UINT32 offset);

	UINT32 mirror_mask_byte;
	UINT32 mirror_mask_word;
	UINT32 mirror_mask_long;
	INT32 bank;
};

struct cpumap_entry {
	INT32 mapnumber; // debug
	INT32 initialized;
	INT32 entry_start_offset;
	UINT32 MEMMAP_MASK;
	INT32 MEMMAP_ENDIANNESS;
	UINT32 MEMMAP_ALIGNBYTES;
	INT32 MEMMAP_DATABITS;
	memory_entry mem_entries[BURNMAP_ENTRIES];
};

void BurnMapGetMapPtr(cpumap_entry **ptr, INT32 map_num);

void BurnMapWriteByte(cpumap_entry *ptr, UINT32 address, UINT8  data);
void BurnMapWriteWord(cpumap_entry *ptr, UINT32 address, UINT16 data);
void BurnMapWriteLong(cpumap_entry *ptr, UINT32 address, UINT32 data);

UINT8  BurnMapReadByte(cpumap_entry *ptr, UINT32 address);
UINT16 BurnMapReadWord(cpumap_entry *ptr, UINT32 address);
UINT32 BurnMapReadLong(cpumap_entry *ptr, UINT32 address);

UINT8  BurnMapFetchOpByte(cpumap_entry *ptr, UINT32 address);
UINT16 BurnMapFetchOpWord(cpumap_entry *ptr, UINT32 address);
UINT32 BurnMapFetchOpLong(cpumap_entry *ptr, UINT32 address);

UINT8  BurnMapFetchArgByte(cpumap_entry *ptr, UINT32 address);
UINT16 BurnMapFetchArgWord(cpumap_entry *ptr, UINT32 address);
UINT32 BurnMapFetchArgLong(cpumap_entry *ptr, UINT32 address);

void BurnMapCheatWriteByte(cpumap_entry *ptr, UINT32 address, UINT8 data);

void BurnMapExit();

#endif
