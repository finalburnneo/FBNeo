/*
 * Copyright (c) 2015, Marcos Medeiros
 * Licensed under BSD 3-clause.
 */
#ifndef MIPS3_ARITHM
#define MIPS3_ARITHM


#include "mips3.h"
#include "mipsdef.h"
#include "mips3_memory.h"

namespace mips
{

// TODO: Overflow exception
void mips3::ADD(uint32_t opcode)
{
    if (RDNUM)
        RD = (int32_t)(RS_u32 + RT_u32);
}

void mips3::ADDU(uint32_t opcode)
{
    if (RDNUM)
        RD = (int32_t)(RS_u32 + RT_u32);
}

// TODO: Overflow exception
void mips3::ADDI(uint32_t opcode)
{
    if (RTNUM)
        RT = (int32_t)(RS_u32 + SIMM);
}

void mips3::ADDIU(uint32_t opcode)
{
    if (RTNUM)
        RT = (int32_t)(RS_u32 + SIMM);
}

void mips3::DADDI(uint32_t opcode)
{
    if (RTNUM)
        RT = RS + IMM_s64;
}

void mips3::DADDIU(uint32_t opcode)
{
    if (RTNUM)
        RT = RS + (uint64_t) SIMM;
}

// TODO: Overflow exception
void mips3::DADD(uint32_t opcode)
{
    if (RDNUM)
        RD = RS + RT;
}

void mips3::DADDU(uint32_t opcode)
{
    if (RDNUM)
        RD = RS + RT;
}


// TODO: Overflow exception
void mips3::SUB(uint32_t opcode)
{
    if (RDNUM)
        RD = (int32_t)(RS_u32 - RT_u32);
}

void mips3::SUBU(uint32_t opcode)
{
    if (RDNUM)
        RD = (int32_t)(RS_u32 - RT_u32);
}


// TODO: Overflow exception
void mips3::DSUB(uint32_t opcode)
{
    if (RDNUM)
        RD = RS - RT;
}


void mips3::DSUBU(uint32_t opcode)
{
    if (RDNUM)
        RD = RS - RT;
}


void mips3::MULT(uint32_t opcode)
{
    int64_t value = (int64_t)RS_s32 * (int64_t)RT_s32;
    LO = (int32_t) value;
    HI = (int32_t) (value >> 32);
}

void mips3::MULTU(uint32_t opcode)
{
    uint64_t value = (uint64_t) RS_u32 * (uint64_t) RT_u32;
    LO = (int32_t) value;
    HI = (int32_t) (value >> 32);
}

void mips3::DIV(uint32_t opcode)
{
    if (RT) {
        LO = (int32_t)(RS_s32 / RT_s32);
        HI = (int32_t)(RS_s32 % RT_s32);
    }
}


void mips3::DIVU(uint32_t opcode)
{
    if (RT) {
        LO = (int32_t)(RS_u32 / RT_u32);
        HI = (int32_t)(RS_u32 % RT_u32);
    }
}


// TODO: 128bit multiplication
void mips3::DMULT(uint32_t opcode)
{
    uint64_t value = ((uint64_t) RS) * ((uint64_t) RT);
    LO = value;
    HI = (int64_t) (value >> 63);
}

void mips3::DDIV(uint32_t opcode)
{
    if (RT) {
        LO = RS / RT;
        HI = RS % RT;
    }
}

void mips3::DMULTU(uint32_t opcode)
{

}

void mips3::DDIVU(uint32_t opcode)
{
    if (RTNUM) {
        LO = RS / RT;
        HI = RS % RT;
    }
}

}

#endif // MIPS3_ARITHM

