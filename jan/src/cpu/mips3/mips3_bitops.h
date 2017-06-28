/*
 * Copyright (c) 2015, Marcos Medeiros
 * Licensed under BSD 3-clause.
 */
#ifndef MIPS3_BITOPS
#define MIPS3_BITOPS

#include "mips3.h"
#include "mipsdef.h"
#include "mips3_memory.h"

namespace mips
{

void mips3::AND(uint32_t opcode)
{
    if (RDNUM)
        RD = RS & RT;
}

void mips3::XOR(uint32_t opcode)
{
    if (RDNUM)
        RD = RS ^ RT;
}

void mips3::OR(uint32_t opcode)
{
    if (RDNUM)
        RD = RS | RT;
}

void mips3::NOR(uint32_t opcode)
{
    if (RDNUM)
        RD = ~(RS | RT);
}

void mips3::ANDI(uint32_t opcode)
{
    if (RTNUM)
        RT = RS & IMM;
}

void mips3::XORI(uint32_t opcode)
{
    if (RTNUM)
        RT = RS ^ IMM;
}

void mips3::ORI(uint32_t opcode)
{
    if (RTNUM)
        RT = RS | IMM;
}

}

#endif // MIPS3_BITOPS

