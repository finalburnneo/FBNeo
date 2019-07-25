#include "mips3_intf.h"
#include "mips3/mips3.h"
#include "burnint.h"
#include <stdint.h>

#ifdef MIPS3_X64_DRC
#include "mips3/x64/mips3_x64.h"
#endif

#define ADDR_BITS   32
#define PAGE_SIZE   0x1000
#define PAGE_SHIFT  12
#define PAGE_MASK   0xFFF
#define PAGE_COUNT  (1 << (ADDR_BITS - PAGE_SHIFT))
#define PAGE_WADD    (PAGE_COUNT)
#define MIPS_MAXHANDLER 10
#define PFN(x)  ((x >> PAGE_SHIFT) & 0xFFFFF)

struct Mips3MemoryMap
{
    UINT8 *MemMap[PAGE_COUNT * 2];

    pMips3ReadByteHandler ReadByte[MIPS_MAXHANDLER];
    pMips3WriteByteHandler WriteByte[MIPS_MAXHANDLER];
    pMips3ReadHalfHandler ReadHalf[MIPS_MAXHANDLER];
    pMips3WriteHalfHandler WriteHalf[MIPS_MAXHANDLER];
    pMips3ReadWordHandler ReadWord[MIPS_MAXHANDLER];
    pMips3WriteWordHandler WriteWord[MIPS_MAXHANDLER];
    pMips3ReadDoubleHandler ReadDouble[MIPS_MAXHANDLER];
    pMips3WriteDoubleHandler WriteDouble[MIPS_MAXHANDLER];
};


static mips::mips3 *g_mips = NULL;
static Mips3MemoryMap *g_mmap = NULL;
static bool g_useRecompiler = false;

#ifdef MIPS3_X64_DRC
static mips::mips3_x64 *g_mips_x64 = nullptr;
#endif

static unsigned char DefReadByte(unsigned int a) { return 0; }
static unsigned short DefReadHalf(unsigned int a) { return 0; }
static unsigned int DefReadWord(unsigned int a) { return 0; }
static unsigned long long DefReadDouble(unsigned int a) { return 0; }
static void DefWriteByte(unsigned int a, unsigned char value) { }
static void DefWriteHalf(unsigned int a, unsigned short value) { }
static void DefWriteWord(unsigned int a, unsigned int value) { }
static void DefWriteDouble(unsigned int a, unsigned long long value) { }

static void ResetMemoryMap()
{
    for (int page = 0; page < PAGE_COUNT; page++) {
        g_mmap->MemMap[page] = (unsigned char*) 0;
        g_mmap->MemMap[PAGE_WADD + page] = (unsigned char*) 0;

    }

    for (int i = 0; i < MIPS_MAXHANDLER; i++) {
        g_mmap->ReadByte[i] = DefReadByte;
        g_mmap->ReadHalf[i] = DefReadHalf;
        g_mmap->ReadWord[i] = DefReadWord;
        g_mmap->ReadDouble[i] = DefReadDouble;
        g_mmap->WriteByte[i] = DefWriteByte;
        g_mmap->WriteHalf[i] = DefWriteHalf;
        g_mmap->WriteWord[i] = DefWriteWord;
        g_mmap->WriteDouble[i] = DefWriteDouble;
    }
}

int Mips3Init()
{
    g_mips = new mips::mips3();
    g_mmap = new Mips3MemoryMap();

#ifdef MIPS3_X64_DRC
    g_mips_x64 = new mips::mips3_x64(g_mips);
#endif

    ResetMemoryMap();
	
	return 0;
}

int Mips3UseRecompiler(bool use)
{
    g_useRecompiler = use;
	
	return 0;
}

int Mips3Exit()
{
#ifdef MIPS3_X64_DRC
    delete g_mips_x64;
#endif
    delete g_mips;
    delete g_mmap;
    g_mips = NULL;
    g_mmap = NULL;
	
	return 0;
}


void Mips3Reset()
{
    if (g_mips)
        g_mips->reset();
}

int Mips3Run(int cycles)
{
#ifdef MIPS3_X64_DRC
    if (g_mips) {
        if (g_useRecompiler && g_mips_x64) {
            g_mips_x64->run(cycles);
        } else {
            g_mips->run(cycles);
        }
    }
#else
    if (g_mips)
        g_mips->run(cycles);
#endif
    return 0;
}

int Mips3MapMemory(unsigned char* pMemory, unsigned int nStart, unsigned int nEnd, int nType)
{
    const int maxPages = (PFN(nEnd) - PFN(nStart)) + 1;

    int page = PFN(nStart);
    for (int i = 0; i < maxPages; i++, page++) {

        if (nType & MAP_READ)
            g_mmap->MemMap[page] = pMemory + (PAGE_SIZE * i);

        if (nType & MAP_WRITE)
            g_mmap->MemMap[PAGE_WADD + page] = pMemory + (PAGE_SIZE * i);
    }
    return 0;
}

int Mips3MapHandler(uintptr_t nHandler, unsigned int nStart, unsigned int nEnd, int nType)
{
    const int maxPages = (PFN(nEnd) - PFN(nStart)) + 1;

    int page = PFN(nStart);
    for (int i = 0; i < maxPages; i++, page++) {

        if (nType & MAP_READ)
            g_mmap->MemMap[page] = (UINT8*) nHandler;

        if (nType & MAP_WRITE)
            g_mmap->MemMap[PAGE_WADD + page] = (UINT8*) nHandler;
    }
    return 0;
}

int Mips3SetReadByteHandler(int i, pMips3ReadByteHandler pHandler)
{
    if (i >= MIPS_MAXHANDLER)
        return 1;
    g_mmap->ReadByte[i] = pHandler;
    return 0;
}

int Mips3SetWriteByteHandler(int i, pMips3WriteByteHandler pHandler)
{
    if (i >= MIPS_MAXHANDLER)
        return 1;
    g_mmap->WriteByte[i] = pHandler;
    return 0;
}

int Mips3SetReadHalfHandler(int i, pMips3ReadHalfHandler pHandler)
{
    if (i >= MIPS_MAXHANDLER)
        return 1;
    g_mmap->ReadHalf[i] = pHandler;
    return 0;
}

int Mips3SetWriteHalfHandler(int i, pMips3WriteHalfHandler pHandler)
{
    if (i >= MIPS_MAXHANDLER)
        return 1;
    g_mmap->WriteHalf[i] = pHandler;
    return 0;
}

int Mips3SetReadWordHandler(int i, pMips3ReadWordHandler pHandler)
{
    if (i >= MIPS_MAXHANDLER)
        return 1;
    g_mmap->ReadWord[i] = pHandler;
    return 0;
}

int Mips3SetWriteWordHandler(int i, pMips3WriteWordHandler pHandler)
{
    if (i >= MIPS_MAXHANDLER)
        return 1;
    g_mmap->WriteWord[i] = pHandler;
    return 0;
}

int Mips3SetReadDoubleHandler(int i, pMips3ReadDoubleHandler pHandler)
{
    if (i >= MIPS_MAXHANDLER)
        return 1;
    g_mmap->ReadDouble[i] = pHandler;
    return 0;
}

int Mips3SetWriteDoubleHandler(int i, pMips3WriteDoubleHandler pHandler)
{
    if (i >= MIPS_MAXHANDLER)
        return 1;
    g_mmap->WriteDouble[i] = pHandler;
    return 0;
}

void Mips3SetIRQLine(const int line, const int state)
{
    if (g_mips) {
        if (state) {
            g_mips->m_state.cpr[0][13] |= (0x400 << line);
        } else {
            g_mips->m_state.cpr[0][13] &= ~(0x400 << line);
        }
    }
}

namespace mips
{
namespace mem
{


template<typename T>
inline T mips_fast_read(uint8_t *ptr, unsigned adr) {
    return *((T*)  ((uint8_t*) ptr + (adr & PAGE_MASK)));
}

template<typename T>
inline void mips_fast_write(uint8_t *xptr, unsigned adr, T value) {
    T *ptr = ((T*)  ((uint8_t*) xptr + (adr & PAGE_MASK)));
    *ptr = value;
}


uint8_t read_byte(addr_t address)
{
    address &= 0xFFFFFFFF;

    UINT8 *pr = g_mmap->MemMap[PFN(address)];
    if ((uintptr_t)pr >= MIPS_MAXHANDLER) {
        return pr[address & PAGE_MASK];
    }
    return g_mmap->ReadByte[(uintptr_t)pr](address);
}

uint16_t read_half(addr_t address)
{
    address &= 0xFFFFFFFF;

    UINT8 *pr = g_mmap->MemMap[PFN(address)];
    if ((uintptr_t)pr >= MIPS_MAXHANDLER) {
        return BURN_ENDIAN_SWAP_INT16(mips_fast_read<uint16_t>(pr, address));
    }
    return g_mmap->ReadHalf[(uintptr_t)pr](address);
}

uint32_t read_word(addr_t address)
{
    address &= 0xFFFFFFFF;

    UINT8 *pr = g_mmap->MemMap[PFN(address)];
    if ((uintptr_t)pr >= MIPS_MAXHANDLER) {
        return BURN_ENDIAN_SWAP_INT32(mips_fast_read<uint32_t>(pr, address));
    }
    return g_mmap->ReadWord[(uintptr_t)pr](address);
}

uint64_t read_dword(addr_t address)
{
    address &= 0xFFFFFFFF;

    UINT8 *pr = g_mmap->MemMap[PFN(address)];
    if ((uintptr_t)pr >= MIPS_MAXHANDLER) {
        return BURN_ENDIAN_SWAP_INT64(mips_fast_read<uint64_t>(pr, address));
    }
    return g_mmap->ReadDouble[(uintptr_t)pr](address);
}


void write_byte(addr_t address, uint8_t value)
{
    address &= 0xFFFFFFFF;

    UINT8 *pr = g_mmap->MemMap[PAGE_WADD + PFN(address)];
    if ((uintptr_t)pr >= MIPS_MAXHANDLER) {
        pr[address & PAGE_MASK] = value;
        return;
    }
    g_mmap->WriteByte[(uintptr_t)pr](address, value);
}

void write_half(addr_t address, uint16_t value)
{
    address &= 0xFFFFFFFF;

    UINT8 *pr = g_mmap->MemMap[PAGE_WADD + PFN(address)];
    if ((uintptr_t)pr >= MIPS_MAXHANDLER) {
        mips_fast_write<uint16_t>(pr, address, BURN_ENDIAN_SWAP_INT16(value));
        return;
    }
    g_mmap->WriteHalf[(uintptr_t)pr](address, value);
}

void write_word(addr_t address, uint32_t value)
{
    address &= 0xFFFFFFFF;

    UINT8 *pr = g_mmap->MemMap[PAGE_WADD + PFN(address)];
    if ((uintptr_t)pr >= MIPS_MAXHANDLER) {
		mips_fast_write<uint32_t>(pr, address, BURN_ENDIAN_SWAP_INT32(value));
        return;
    }
    g_mmap->WriteWord[(uintptr_t)pr](address, value);
}

void write_dword(addr_t address, uint64_t value)
{
    address &= 0xFFFFFFFF;

    UINT8 *pr = g_mmap->MemMap[PAGE_WADD + PFN(address)];
    if ((uintptr_t)pr >= MIPS_MAXHANDLER) {
		mips_fast_write<uint64_t>(pr, address, BURN_ENDIAN_SWAP_INT64(value));
        return;
    }
    g_mmap->WriteDouble[(uintptr_t)pr](address, value);
}

}
}


unsigned int Mips3GetPC()
{
    return g_mips->m_prev_pc;
}
