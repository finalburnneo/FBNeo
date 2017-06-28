/*
 * Copyright (c) 2015, Marcos Medeiros
 * Licensed under BSD 3-clause.
 */
#ifndef MIPS3_SHIFT
#define MIPS3_SHIFT

#include "mips3.h"
#include "mipsdef.h"
#include "mips3_memory.h"

namespace mips
{

void mips3::SLL(uint32_t opcode)
{
    if (RDNUM)
        RD = (int32_t)(RT_u32 << SHAMT);
}

void mips3::SRL(uint32_t opcode)
{
    if (RDNUM)
        RD = (int32_t)(RT_u32 >> SHAMT);
}

void mips3::SRA(uint32_t opcode)
{
    if (RDNUM)
        RD = (int32_t)RT_u32 >> SHAMT;
}

void mips3::SLLV(uint32_t opcode)
{
    if (RDNUM)
        RD = (int32_t)(RT_u32 << (RS & 0x1F));
}

void mips3::SRLV(uint32_t opcode)
{
    if (RDNUM)
        RD = (int32_t)(RT_u32 >> (RS & 0x1F));
}

void mips3::SRAV(uint32_t opcode)
{
    if (RDNUM)
        RD = (int32_t)RT_u32 >> (RS & 0x1F);
}

void mips3::DSLLV(uint32_t opcode)
{
    if (RDNUM)
        RD = RT << (RS & 0x3F);
}

void mips3::DSLRV(uint32_t opcode)
{
    if (RDNUM)
        RD = RT >> (RS & 0x3F);
}

void mips3::DSRAV(uint32_t opcode)
{
    if (RDNUM)
        RD = (int64_t)RT >> (RS & 0x3F);
}

void mips3::DSLL(uint32_t opcode)
{
    if (RDNUM)
        RD = RT << SHAMT;
}

void mips3::DSRL(uint32_t opcode)
{
    if (RDNUM)
        RD = RT >> SHAMT;
}

void mips3::DSRA(uint32_t opcode)
{
    if (RDNUM)
        RD = (int64_t)RT >> SHAMT;
}

void mips3::DSLL32(uint32_t opcode)
{
    if (RDNUM)
        RD = RT << (SHAMT + 32);
}

void mips3::DSRL32(uint32_t opcode)
{
    if (RDNUM)
        RD = RT >> (SHAMT + 32);
}

void mips3::DSRA32(uint32_t opcode)
{
    if (RDNUM)
        RD = (int64_t)RT >> (SHAMT + 32);
}



}

#endif // MIPS3_SHIFT

