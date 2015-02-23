/*
 * Copyright (c) 2015, Marcos Medeiros
 * Licensed under BSD 3-clause.
 */
#ifndef MIPS3_X64_ARITHM
#define MIPS3_X64_ARITHM


#include "../mips3.h"
#include "mips3_x64_defs.h"
#include "mips3_x64.h"
#include "xbyak/xbyak.h"

namespace mips
{

inline bool mips3_x64::ADD(uint32_t opcode)
{
    if (RDNUM) {
        mov(eax, RS_x);
        add(eax, RT_x);
        cdqe();
        mov(RD_x, rax);
    }
    return false;
}

inline bool mips3_x64::ADDU(uint32_t opcode)
{
    if (RDNUM) {
        mov(eax, RS_x);
        add(eax, RT_x);
        cdqe();
        mov(RD_x, rax);
    }
    return false;
}

// TODO: Overflow exception
inline bool mips3_x64::ADDI(uint32_t opcode)
{
    if (RTNUM) {
        mov(eax, RS_x);
        add(eax, (int32_t)SIMM);
        cdqe();
        mov(RT_x, rax);
    }
    return false;
}

inline bool mips3_x64::ADDIU(uint32_t opcode)
{
    if (RTNUM) {
        mov(eax, RS_x);
        add(eax, (int32_t)SIMM);
        cdqe();
        mov(RT_x, rax);
    }
    return false;
}

inline bool mips3_x64::DADDI(uint32_t opcode)
{
    if (RTNUM) {
        mov(rax, RS_x);
        add(rax, (size_t)(int32_t)SIMM);
        mov(RT_x, rax);
    }
    return false;
}

inline bool mips3_x64::DADDIU(uint32_t opcode)
{
    if (RTNUM) {
        mov(rax, RS_x);
        add(rax, (size_t)(int32_t)SIMM);
        mov(RT_x, rax);
    }
    return false;
}

// TODO: Overflow exception
inline bool mips3_x64::DADD(uint32_t opcode)
{
    if (RDNUM) {
        mov(rax, RS_x);
        add(rax, RT_x);
        mov(RD_x, rax);
    }
    return false;
}

inline bool mips3_x64::DADDU(uint32_t opcode)
{
    if (RDNUM) {
        mov(rax, RS_x);
        add(rax, RT_x);
        mov(RD_x, rax);
    }
    return false;
}


// TODO: Overflow exception
inline bool mips3_x64::SUB(uint32_t opcode)
{
    if (RDNUM) {
        mov(eax, RS_x);
        sub(eax, RT_x);
        cdqe();
        mov(RD_x, rax);
    }
    return false;
}

inline bool mips3_x64::SUBU(uint32_t opcode)
{
    if (RDNUM) {
        mov(eax, RS_x);
        sub(eax, RT_x);
        cdqe();
        mov(RD_x, rax);
    }
    return false;
}


// TODO: Overflow exception
inline bool mips3_x64::DSUB(uint32_t opcode)
{
    if (RDNUM) {
        mov(rax, RS_x);
        sub(rax, RT_x);
        mov(RD_x, rax);
    }
    return false;
}


inline bool mips3_x64::DSUBU(uint32_t opcode)
{
    if (RDNUM) {
        mov(rax, RS_x);
        sub(rax, RT_x);
        mov(RD_x, rax);
    }
    return false;
}

inline bool mips3_x64::MULT(uint32_t opcode)
{
    mov(eax, RS_x);
    mov(ecx, RT_x);
    imul(ecx);
    movsxd(rdx, edx);
    movsxd(rax, eax);
    mov(HI_x, rdx);
    mov(LO_x, rax);

    m_block_icounter += 2;
    return false;
}

inline bool mips3_x64::MULTU(uint32_t opcode)
{
    mov(eax, RS_x);
    mov(ecx, RT_x);
    mul(ecx);
    movsxd(rdx, edx);
    movsxd(rax, eax);
    mov(HI_x, rdx);
    mov(LO_x, rax);

    m_block_icounter += 2;
    return false;
}


inline bool mips3_x64::DIV(uint32_t opcode)
{
    if (RTNUM) {
        inLocalLabel();
        mov(rcx, RT_x);
        cmp(rcx, 0);
        je(".end");

        mov(eax, RS_x);
        cdq();
        idiv(ecx);
        cdqe();

        mov(LO_x, rax);
        movsxd(rcx, edx);
        mov(HI_x, rdx);
        L(".end");
        outLocalLabel();

    }
    m_block_icounter += 34;
    return false;
}


inline bool mips3_x64::DIVU(uint32_t opcode)
{
    if (RTNUM) {
        inLocalLabel();
        mov(ecx, RT_x);
        cmp(ecx, 0);
        je(".end");

        mov(eax, RS_x);
        xor_(edx, edx);
        div(ecx);
        cdqe();

        mov(LO_x, rax);
        movsxd(rcx, edx);
        mov(HI_x, rdx);
        L(".end");
        outLocalLabel();

    }
    m_block_icounter += 34;
    return false;
}

inline bool mips3_x64::DMULT(uint32_t opcode)
{
    mov(rax, RS_x);
    mov(rcx, RT_x);
    imul(rcx);
    mov(HI_x, rdx);
    mov(LO_x, rax);
    m_block_icounter += 6;
    return false;
}

inline bool mips3_x64::DDIV(uint32_t opcode)
{
    if (RTNUM) {
        inLocalLabel();
        mov(rcx, RT_x);
        cmp(rcx, 0);
        je(".end");

        mov(rax, RS_x);
        cqo();
        idiv(rcx);

        mov(LO_x, rax);
        mov(HI_x, rdx);
        L(".end");
        outLocalLabel();

    }
    m_block_icounter += 66;
    return false;
}

inline bool mips3_x64::DMULTU(uint32_t opcode)
{
    mov(rax, RS_x);
    mov(rcx, RT_x);
    mul(rcx);
    mov(HI_x, rdx);
    mov(LO_x, rax);
    m_block_icounter += 6;
    return false;
}

inline bool mips3_x64::DDIVU(uint32_t opcode)
{
    if (RTNUM) {
        inLocalLabel();
        mov(rcx, RT_x);
        cmp(rcx, 0);
        je(".end");

        mov(rax, RS_x);
        cqo();
        div(rcx);

        mov(LO_x, rax);
        mov(HI_x, rdx);
        L(".end");
        outLocalLabel();

    }
    m_block_icounter += 66;
    return false;
}

}

#endif // MIPS3_X64_ARITHM

