#ifndef BURNMAP // let's not include this multiple times
#define BURNMAP

// flags to be passed in MAP_START
#define MAP_FLAG_BYTEALIGN	0x0001		// for 16+ bit cpus, words/longs aligned to byte (address) or word/long (address & ~3)
#define MAP_FLAG_BIGENDIAN	0x0002		// big endian or little
#define MAP_FLAG_BITS_8		0x0100		// this is an 8-bit cpu (optional)
#define MAP_FLAG_BITS_16	0x0200		// this is a 16-bit cpu (optional)
#define MAP_FLAG_BITS_32	0x0400		// this is a 32-bit cpu (optional)

// start the address map, address_bits is 16 for cpus like z80, 24 for 68k, 32 for arm/sh2
void MAP_START(INT32 map_num, INT32 address_bits, INT32 flags);

// open an address map and add to it
void MAP_MODIFY(INT32 map_num);

// we're finished with this map
void MAP_END();

// set an address range for reads/writes/etc
void MAP_RANGE(UINT32 address_start, UINT32 address_end);

// set address map mirroring
void MAP_MIRROR(UINT32 mirror);

// map already allocated memory (rom, for example. can be used to pass inputs - MAP_REGION(&DrvInputs[0], MAP_READ); )
void MAP_REGION(UINT8 *mem, UINT32 flags);

// allocate ram the size of address_start - address_end and map it
void MAP_RAMALLOC();

// allocate ram the size of address_start - address_end, map it, and set a pointer to it
void MAP_RAMALLOC(UINT8 **ptr);

// set pointer to point to RAM allocated by MAP_RAMALLOC() or MAP_NVRAMALLOC()
void MAP_RAMPTR(UINT8 **ptr);

// allocate ram the size of address_start - address_end and map it, treat it like Non-volatile RAM
void MAP_NVRAMALLOC();

// create a bankable area. Set banks with BurnMapSetBank... functions.
void MAP_BANK(UINT32 flags, INT32 banknumber);

// use NOP functions to keep from logging unmapped reads/writes
void MAP_WRITENOP();
void MAP_READNOP();
void MAP_NOP(); // both

// these allow you to change regions to only be accessable for reads or writes
// - some things will map RAM for sprites but can only write to it and not read
void MAP_WRITEONLY();
void MAP_READONLY();

// map read, write, or both functions
void MAP_R(UINT32 (*read)(UINT32 offset));
void MAP_W(void (*write)(UINT32 offset, UINT32 mask, UINT32 data));
void MAP_RW(UINT32 (*read)(UINT32 offset), void (*write)(UINT32 offset, UINT32 mask, UINT32 data));

// use device (pass a chip #), read, write, or both
void MAP_DEVR(INT32 chip, UINT32 (*read)(INT32 chip, UINT32 offset));
void MAP_DEVW(INT32 chip, void (*write)(INT32 chip, UINT32 offset, UINT32 mask, UINT32 data));
void MAP_DEVRW(INT32 chip, UINT32 (*read)(INT32 chip, UINT32 offset), void (*write)(INT32 chip, UINT32 offset, UINT32 mask, UINT32 data));

// map fetch functions, these are generally not used
void MAP_F(UINT32 (*fetch)(UINT32 offset));							// fetch (opcodes and args)
void MAP_FO(UINT32 (*fetchop)(UINT32 offset));						// fetch opcodes
void MAP_FA(UINT32 (*fetcharg)(UINT32 offset));						// fetch args

// unmap a memory range so that somethine else can be mapped there
// this is useful for MAP_MODIFY
void UNMAP_RANGE(INT32 map_num, UINT32 address_start, UINT32 address_end, UINT32 flags);

// set banks to read/write/regions
void BurnMapSetBankWrite(INT32 map_num, INT32 bank, void (*write)(UINT32,UINT32,UINT32));
void BurnMapSetBankRead(INT32 map_num, INT32 bank, UINT32 (*read)(UINT32));
void BurnMapSetBankRegion(INT32 map_num, INT32 bank, UINT8 *mem);

// add "patch" handlers, these are added to the top of the address map, and as such are handled first
void BurnMapAddWrite(INT32 map_num, INT32 address_start, INT32 address_end, void (*write)(UINT32,UINT32,UINT32));
void BurnMapAddRead(INT32 map_num, INT32 address_start, INT32 address_end, UINT32 (*read)(UINT32));
void BurnMapAddReadWrite(INT32 map_num, INT32 address_start, INT32 address_end, UINT32 (*read)(UINT32), void (*write)(UINT32,UINT32,UINT32));
void BurnMapAddDeviceWrite(INT32 map_num, INT32 address_start, INT32 address_end, INT32 chip, void (*write)(INT32,UINT32,UINT32,UINT32));
void BurnMapAddDeviceRead(INT32 map_num, INT32 address_start, INT32 address_end, INT32 chip, UINT32 (*read)(INT32,UINT32));
void BurnMapAddDeviceReadWrite(INT32 map_num, INT32 address_start, INT32 address_end, INT32 chip, UINT32 (*read)(INT32,UINT32), void (*write)(INT32,UINT32,UINT32,UINT32));
void BurnMapAddRegion(INT32 map_num, INT32 address_start, INT32 address_end, UINT8 *memptr, UINT32 flags);

// save states - should call this in burn.cpp?
void BurnMapScan(INT32 nAction, INT32 *pnMin);

// these are useful to see which bytes are written, some set-ups will ignore data in certain bytes (in words & longs)

#define USING_BITS_00to07	(mask & 0x000000ff)
#define USING_BITS_08to15	(mask & 0x0000ff00)
#define USING_BITS_16to23	(mask & 0x00ff0000)
#define USING_BITS_24to31	(mask & 0xff000000)

#define USING_BITS_00to15	(mask & 0x0000ffff)
#define USING_BITS_16to31	(mask & 0xffff0000)

// this function is to merge data that is masked into a word, byte or long
// COMBINE_DATA(&ram[offset]);

#define COMBINE_DATA(ptr) (ptr) = (((ptr) & mask) | (data & ~mask))

#endif
