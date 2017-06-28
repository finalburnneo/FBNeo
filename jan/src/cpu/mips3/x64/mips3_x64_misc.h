/*
 * Copyright (c) 2015, Marcos Medeiros
 * Licensed under BSD 3-clause.
 */
#ifndef MIPS3_X64_MISC
#define MIPS3_X64_MISC


#include "../mips3.h"
#include "mips3_x64_defs.h"
#include "mips3_x64.h"
#include "xbyak/xbyak.h"


namespace mips
{

inline bool mips3_x64::SLT(uint32_t opcode)
{
    if (RDNUM) {
        mov(rax, RS_x);
        cmp(rax, RT_x);
        setl(dl);
        movzx(edx, dl);
        mov(RD_x, rdx);
    }
    return false;
}

inline bool mips3_x64::SLTU(uint32_t opcode)
{
    if (RDNUM) {
        mov(rax, RS_x);
        cmp(rax, RT_x);
        setb(dl);
        movzx(edx, dl);
        mov(RD_x, rdx);
    }
    return false;
}

inline bool mips3_x64::SLTI(uint32_t opcode)
{
    if (RTNUM) {
        mov(rax, RS_x);
        cmp(rax, IMM_s64);
        setl(dl);
        movzx(edx, dl);
        mov(RT_x, rdx);
    }
    return false;
}

inline bool mips3_x64::SLTIU(uint32_t opcode)
{
    if (RTNUM) {
        mov(rax, RS_x);
        cmp(rax, IMM);
        setb(dl);
        movzx(edx, dl);
        mov(RT_x, rdx);
    }
    return false;
}

inline bool mips3_x64::MFHI(uint32_t opcode)
{
    if (RDNUM) {
        mov(rax, HI_x);
        mov(RD_x, rax);
    }
    return false;
}

inline bool mips3_x64::MTHI(uint32_t opcode)
{
    mov(rax, RS_x);
    mov(HI_x, rax);
    return false;
}

inline bool mips3_x64::MFLO(uint32_t opcode)
{
    if (RDNUM) {
        mov(rax, LO_x);
        mov(RD_x, rax);
    }
    return false;
}

inline bool mips3_x64::MTLO(uint32_t opcode)
{
    mov(rax, RS_x);
    mov(LO_x, rax);
    return false;
}


}

#endif // MIPS3_X64_MISC

