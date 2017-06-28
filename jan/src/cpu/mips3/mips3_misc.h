/*
 * Copyright (c) 2015, Marcos Medeiros
 * Licensed under BSD 3-clause.
 */
#ifndef MIPS3_MISC
#define MIPS3_MISC

#include "mips3.h"
#include "mipsdef.h"
#include "mips3_memory.h"

namespace mips
{

void mips3::SLT(uint32_t opcode)
{
    if (RDNUM)
        RD = RS_s64 < RT_s64;
}

void mips3::SLTU(uint32_t opcode)
{
    if (RDNUM)
        RD = RS < RT;
}

void mips3::SLTI(uint32_t opcode)
{
    if (RTNUM)
        RT = RS_s64 < IMM_s64 ? 1 : 0;
}

void mips3::SLTIU(uint32_t opcode)
{
    if (RTNUM)
        RT = RS < (uint64_t)SIMM ? 1 : 0;
}

void mips3::MFHI(uint32_t opcode)
{
    if (RDNUM)
        RD = HI;
}

void mips3::MTHI(uint32_t opcode)
{
    HI = RS;
}

void mips3::MFLO(uint32_t opcode)
{
    if (RDNUM)
        RD = LO;
}

void mips3::MTLO(uint32_t opcode)
{
    LO = RS;
}


}

#endif // MIPS3_MISC

