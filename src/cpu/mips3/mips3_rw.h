/*
 * Copyright (c) 2015, Marcos Medeiros
 * Licensed under BSD 3-clause.
 */
#ifndef MIPS3_RW
#define MIPS3_RW

#include "mips3.h"
#include "mipsdef.h"
#include "memory.h"

namespace mips
{


void mips3::LUI(uint32_t opcode)
{
    if (RTNUM)
        RT = (int32_t)(IMM << 16);
}

void mips3::SB(uint32_t opcode)
{
    addr_t vaddr = ((int32_t)SIMM) + RS;
    addr_t eaddr;
    if (translate(vaddr, &eaddr)) {
    }

    mem::write_byte(eaddr, RT);
}

void mips3::SH(uint32_t opcode)
{
    addr_t vaddr = ((int32_t)SIMM) + RS;
    addr_t eaddr;
    if (translate(vaddr, &eaddr)) {
    }

    mem::write_half(eaddr & ~1, RT);
}

void mips3::SW(uint32_t opcode)
{
    addr_t vaddr = ((int32_t)SIMM) + RS;
    addr_t eaddr;
    if (translate(vaddr & ~3, &eaddr)) {
    }

    mem::write_word(eaddr, RT);
}

void mips3::SD(uint32_t opcode)
{
    addr_t vaddr = ((int32_t)SIMM) + RS;
    addr_t eaddr;
    if (translate(vaddr & ~7, &eaddr)) {
    }

    mem::write_dword(eaddr, RT);
}

// TODO: FIX IT
void mips3::SDL(uint32_t opcode)
{
    addr_t vaddr = ((int32_t)SIMM) + RS;
    addr_t eaddr;
    if (translate(vaddr & ~7, &eaddr)) {
    }

    auto data = mem::read_dword(eaddr);
    uint64_t wrdata = 0;
    uint64_t mask = ~0ULL;

    int t = vaddr & 7;
    wrdata = (RT >> ((7 - t) * 8)) | (data & (mask << ((t + 1) * 8)));
    mem::write_dword(eaddr, wrdata);
}

// TODO: FIX IT
void mips3::SDR(uint32_t opcode)
{
    addr_t vaddr = ((int32_t)SIMM) + RS;
    addr_t eaddr;
    if (translate(vaddr & ~7, &eaddr)) {
    }

    auto data = mem::read_dword(eaddr);
    uint64_t wrdata = 0;
    uint64_t mask = ~0ULL;

    int t = vaddr & 7;
    wrdata = (RT << (t * 8)) | (data & (mask >> ((7 - t + 1) * 8)));
    mem::write_dword(eaddr, wrdata);
}

void mips3::LWL(uint32_t opcode)
{
    uint32_t vaddr = ((int32_t)SIMM) + RS;

    int shift = (3 - (vaddr & 3)) * 8;
    uint64_t mask = (0xFFFFFFFFFFFFFFFFULL << shift);

    addr_t eaddr;
    if (translate(vaddr & ~3, &eaddr)) {
    }

    auto data = mem::read_dword(eaddr);

    if (RTNUM)
        RT = (int32_t)((RT & ~mask) | (data << shift));
}

void mips3::LWR(uint32_t opcode)
{
    uint32_t vaddr = ((int32_t)SIMM) + RS;

    int shift = (vaddr & 3) * 8;
    uint64_t mask = (0xFFFFFFFFFFFFFFFFULL >> shift);

    addr_t eaddr;
    if (translate(vaddr & ~3, &eaddr)) {
    }
    //d18
    auto data = mem::read_dword(eaddr);

    if (RTNUM)
        RT = (int32_t)((RT & ~mask) | (data >> shift));
}


// Válido apenas para little endian.
void mips3::LDL(uint32_t opcode)
{
    uint32_t vaddr = ((int32_t)SIMM) + RS;

    int shift = (7 - (vaddr & 7)) * 8;
    uint64_t mask = (0xFFFFFFFFFFFFFFFFULL << shift);

    addr_t eaddr;
    if (translate(vaddr & ~7, &eaddr)) {
    }

    auto data = mem::read_dword(eaddr);

    if (RTNUM)
        RT = (RT & ~mask) | (data << shift);
}

// Válido apenas para little endian.
void mips3::LDR(uint32_t opcode)
{
    uint32_t vaddr = ((int32_t)SIMM) + RS;

    int shift = (vaddr & 7) * 8;
    uint64_t mask = (0xFFFFFFFFFFFFFFFFULL >> shift);

    addr_t eaddr;
    if (translate(vaddr & ~7, &eaddr)) {
    }
    //d18
    auto data = mem::read_dword(eaddr);

    if (RTNUM)
        RT = (RT & ~mask) | (data >> shift);
}


void mips3::LW(uint32_t opcode)
{
    addr_t vaddr = ((int32_t)SIMM) + RS;
    addr_t eaddr;
    if (translate(vaddr, &eaddr)) {
    }

    if (RTNUM)
        RT = (int32_t) mem::read_word(eaddr);
}

void mips3::LWU(uint32_t opcode)
{
    addr_t vaddr = ((int32_t)SIMM) + RS;
    addr_t eaddr;
    if (translate(vaddr & ~3, &eaddr)) {
    }

    if (RTNUM)
        RT = (uint32_t) mem::read_word(eaddr);
}

void mips3::LD(uint32_t opcode)
{
    addr_t vaddr = ((int32_t)SIMM) + RS;
    addr_t eaddr;
    if (translate(vaddr & ~7, &eaddr)) {
    }

    if (RTNUM)
        RT = mem::read_dword(eaddr);
}

void mips3::LL(uint32_t opcode)
{
    addr_t vaddr = ((int32_t)SIMM) + RS;
    addr_t eaddr;
    if (translate(vaddr & ~3, &eaddr)) {
    }

    if (RTNUM)
        RT = (int32_t) mem::read_word(eaddr);
}

// TODO: FIX IT
void mips3::LWC1(uint32_t opcode)
{
    addr_t vaddr = ((int32_t)SIMM) + RS;
    addr_t eaddr;
    if (translate(vaddr & ~3, &eaddr)) {
    }
    m_state.cpr[1][RTNUM] = (uint32_t) mem::read_word(eaddr);
}

// TODO: FIX IT
void mips3::SWC1(uint32_t opcode)
{
    addr_t vaddr = ((int32_t)SIMM) + RS;
    addr_t eaddr;
    if (translate(vaddr & ~3, &eaddr)) {
    }
    mem::write_word(eaddr, m_state.cpr[1][RTNUM]);
}

void mips3::LB(uint32_t opcode)
{
    addr_t vaddr = ((int32_t)SIMM) + RS;
    addr_t eaddr;
    if (translate(vaddr, &eaddr)) {
    }

    if (RTNUM)
        RT = (int8_t) mem::read_byte(eaddr);
}

void mips3::LBU(uint32_t opcode)
{
    addr_t vaddr = ((int32_t)SIMM) + RS;
    addr_t eaddr;
    if (translate(vaddr, &eaddr)) {
    }

    if (RTNUM)
        RT = (uint8_t) mem::read_byte(eaddr);
}

void mips3::LH(uint32_t opcode)
{
    addr_t vaddr = ((int32_t)SIMM) + RS;
    addr_t eaddr;
    if (translate(vaddr & ~1, &eaddr)) {
    }

    if (RTNUM)
        RT = (int16_t) mem::read_word(eaddr);
}

void mips3::LHU(uint32_t opcode)
{
    addr_t vaddr = ((int32_t)SIMM) + RS;
    addr_t eaddr;
    if (translate(vaddr & ~1, &eaddr)) {
    }

    if (RTNUM)
        RT = (uint16_t) mem::read_word(eaddr);
}

}

#endif // MIPS3_RW

