/*
 * Copyright (c) 2015, Marcos Medeiros
 * Licensed under BSD 3-clause.
 */
#ifndef MIPS3_X64_SHIFT
#define MIPS3_X64_SHIFT


#include "../mips3.h"
#include "mips3_x64_defs.h"
#include "mips3_x64.h"
#include "xbyak/xbyak.h"


namespace mips
{

inline bool mips3_x64::SLL(uint32_t opcode)
{
    if (RDNUM && SHAMT) {
        mov(eax, RT_x);
        shl(eax, SHAMT);
        cdqe();
        mov(RD_x, rax);
    }
    return false;
}

inline bool mips3_x64::SRL(uint32_t opcode)
{
    if (RDNUM && SHAMT) {
        mov(eax, RT_x);
        shr(eax, SHAMT);
        cdqe();
        mov(RD_x, rax);
    }
    return false;
}

inline bool mips3_x64::SRA(uint32_t opcode)
{
    if (RDNUM) {
        mov(eax, RT_x);
        sar(eax, SHAMT);
        cdqe();
        mov(RD_x, rax);
    }
    return false;
}


inline bool mips3_x64::SLLV(uint32_t opcode)
{
    if (RDNUM) {
        mov(eax, RT_x);
        mov(ecx, RS_x);
        and_(ecx, 31);
        shl(eax, cl);
        cdqe();
        mov(RD_x, rax);
    }
    return false;
}

inline bool mips3_x64::SRLV(uint32_t opcode)
{
    if (RDNUM) {
        mov(eax, RT_x);
        mov(ecx, RS_x);
        and_(ecx, 31);
        shr(eax, cl);
        cdqe();
        mov(RD_x, rax);
    }
    return false;
}

inline bool mips3_x64::SRAV(uint32_t opcode)
{
    if (RDNUM) {
        mov(eax, RT_x);
        mov(ecx, RS_x);
        and_(ecx, 31);
        sar(eax, cl);
        cdqe();
        mov(RD_x, rax);
    }
    return false;
}

inline bool mips3_x64::DSLLV(uint32_t opcode)
{
    if (RDNUM) {
        mov(rax, RT_x);
        mov(rcx, RS_x);
        and_(rcx, 63);
        shl(rax, cl);
        mov(RD_x, rax);
    }
    return false;
}

inline bool mips3_x64::DSLRV(uint32_t opcode)
{
    if (RDNUM) {
        mov(rax, RT_x);
        mov(rcx, RS_x);
        and_(rcx, 63);
        shr(rax, cl);
        mov(RD_x, rax);
    }
    return false;
}

inline bool mips3_x64::DSRAV(uint32_t opcode)
{
    if (RDNUM) {
        mov(rax, RT_x);
        mov(rcx, RS_x);
        and_(rcx, 63);
        sar(rax, cl);
        mov(RD_x, rax);
    }
    return false;
}

inline bool mips3_x64::DSLL(uint32_t opcode)
{
    if (RDNUM) {
        mov(rax, RT_x);
        shl(rax, SHAMT);
        mov(RD_x, rax);
    }
    return false;
}

inline bool mips3_x64::DSRL(uint32_t opcode)
{
    if (RDNUM) {
        mov(rax, RT_x);
        shr(rax, SHAMT);
        mov(RD_x, rax);

    }
    return false;
}

inline bool mips3_x64::DSRA(uint32_t opcode)
{
    if (RDNUM) {
        mov(rax, RT_x);
        sar(rax, SHAMT & 63);
        mov(RD_x, rax);
    }
    return false;
}

inline bool mips3_x64::DSLL32(uint32_t opcode)
{
    if (RDNUM) {
        mov(rax, RT_x);
        shl(rax, SHAMT + 32);
        mov(RD_x, rax);
    }
    return false;
}

inline bool mips3_x64::DSRL32(uint32_t opcode)
{
    if (RDNUM) {
        mov(rax, RT_x);
        shr(rax, SHAMT + 32);
        mov(RD_x, rax);
    }
    return false;
}

inline bool mips3_x64::DSRA32(uint32_t opcode)
{
    if (RDNUM) {
        mov(rax, RT_x);
        sar(rax, SHAMT + 32);
        mov(RD_x, rax);
    }
    return false;
}



}

#endif // MIPS3_X64_SHIFT

