/*
 * Copyright (c) 2015, Marcos Medeiros
 * Licensed under BSD 3-clause.
 */
#ifndef MIPS3_X64_RW
#define MIPS3_X64_RW


#include "../mips3.h"
#include "mips3_x64_defs.h"
#include "mips3_x64.h"
#include "xbyak/xbyak.h"
#include "../mips3_memory.h"


namespace mips
{

#define GET_EADDR_IN_RDX(ignore) \
    do {    \
        auto eaddr = ptr[rbp-8];\
        mov(rdi, (size_t) m_core);\
        mov(rsi, (int64_t)(int32_t)SIMM);\
        add(rsi, RS_x);\
        and_(rsi, ~((uint64_t)ignore));\
        lea(rdx, eaddr);\
        size_t madr = M_ADR(mips3::translate);\
        mov(rax, madr);\
        call(rax);\
        mov(rdx, eaddr);\
    } while (0)

#define GET_EA_RDX_VA_RCX(ignore) \
    do {    \
        auto eaddr = ptr[rbp-8];\
        auto vaddr = ptr[rbp-16];\
        mov(rdi, (size_t) m_core);\
        mov(rsi, (int64_t)(int32_t)SIMM);\
        add(rsi, RS_x);\
        and_(rsi, ~((uint64_t)ignore));\
        lea(rdx, eaddr);\
        mov(vaddr, rsi);\
        mov(rax, M_ADR(mips3::translate));\
        call(rax);\
        mov(rdx, eaddr);\
        mov(rcx, vaddr);\
    } while (0)

bool mips3_x64::LUI(uint32_t opcode)
{
    if (RTNUM) {
        int64_t value = (int32_t)(IMM << 16);
        mov(RT_q, value);
    }
    return false;
}

bool mips3_x64::SB(uint32_t opcode)
{
    GET_EADDR_IN_RDX(0);
    mov(rdi, rdx);
    mov(rsi, RT_x);
    mov(rax, F_ADR(mem::write_byte));
    call(rax);
    return false;
}

bool mips3_x64::SH(uint32_t opcode)
{
    GET_EADDR_IN_RDX(1);
    mov(rdi, rdx);
    mov(rsi, RT_x);
    mov(rax, F_ADR(mem::write_half));
    call(rax);
    return false;
}

bool mips3_x64::SW(uint32_t opcode)
{
    GET_EADDR_IN_RDX(3);
    mov(rdi, rdx);
    mov(rsi, RT_x);
    mov(rax, F_ADR(mem::write_word));
    call(rax);
    return false;
}

bool mips3_x64::SD(uint32_t opcode)
{
    GET_EADDR_IN_RDX(7);
    mov(rdi, rdx);
    mov(rsi, RT_x);
    mov(rax, F_ADR(mem::write_dword));
    call(rax);
    return false;
}

// TODO: FIX IT
bool mips3_x64::SDL(uint32_t opcode)
{
    return false;
}

// TODO: FIX IT
bool mips3_x64::SDR(uint32_t opcode)
{
    return false;
}


bool mips3_x64::LWL(uint32_t opcode)
{
    if (RTNUM) {
        GET_EA_RDX_VA_RCX(3);
        auto shift = ptr[rbp-8];
        auto mask = ptr[rbp-16];

        mov(rax, rcx);
        and_(rax, 3);
        xor_(rax, 3);
        shl(rax, 3);
        mov(shift, rax);

        mov(rax, 0);
        not_(rax);
        mov(rcx, shift);
        shl(rax, cl);
        mov(mask, rax);

        mov(rdi, rdx);
        mov(rax, F_ADR(mem::read_dword));
        call(rax);

        mov(rcx, shift);
        shl(rax, cl);

        mov(rcx, RT_x);
        mov(rdi, mask);
        not_(rdi);
        and_(rcx, rdi);
        or_(rax, rcx);

        cdqe();
        mov(RT_x, rax);

    }
    return false;
}


bool mips3_x64::LWR(uint32_t opcode)
{
    if (RTNUM) {
        GET_EA_RDX_VA_RCX(3);

        auto shift = ptr[rbp-8];
        auto mask = ptr[rbp-16];

        mov(rax, rcx);
        and_(rax, 3);
        shl(rax, 3);
        mov(shift, rax);

        xor_(rax, rax);
        not_(rax);
        mov(rcx, shift);
        shr(rax, cl);
        mov(mask, rax);

        mov(rdi, rdx);
        mov(rax, F_ADR(mem::read_word));
        call(rax);

        mov(rcx, shift);
        shr(rax, cl);

        mov(rcx, RT_x);
        mov(rdi, mask);
        not_(rdi);
        and_(rcx, rdi);
        or_(rax, rcx);

        cdqe();
        mov(RT_x, rax);

    }
    return false;
}


// Válido apenas para little endian.
bool mips3_x64::LDL(uint32_t opcode)
{
    if (RTNUM) {
        GET_EA_RDX_VA_RCX(7);
        auto shift = ptr[rbp-8];
        auto mask = ptr[rbp-16];

        mov(rax, rcx);
        and_(rax, 7);
        xor_(rax, 7);
        shl(rax, 3);
        mov(shift, rax);

        mov(rax, 0);
        not_(rax);
        mov(rcx, shift);
        shl(rax, cl);
        mov(mask, rax);

        mov(rdi, rdx);
        mov(rax, F_ADR(mem::read_dword));
        call(rax);

        mov(rcx, shift);
        shl(rax, cl);

        mov(rcx, RT_x);
        mov(rdi, mask);
        not_(rdi);
        and_(rcx, rdi);
        or_(rax, rcx);

        mov(RT_x, rax);

    }
    return false;
}

// Válido apenas para little endian.
/*
    uint32_t vaddr = ((int32_t)SIMM) + RS;

    int shift = (vaddr & 7) * 8;
    uint64_t mask = (0xFFFFFFFFFFFFFFFFULL >> shift);

    addr_t eaddr;
    if (translate(vaddr & ~7, &eaddr)) {
    }
    //d18
    auto data = mem::read_dword(eaddr);

    if (RTNUM)
        RT = (RT & ~mask) | (data >> shift);*/
bool mips3_x64::LDR(uint32_t opcode)
{
    if (RTNUM) {
        GET_EA_RDX_VA_RCX(7);

        auto shift = ptr[rbp-8];
        auto mask = ptr[rbp-16];

        mov(rax, rcx);
        and_(rax, 7);
        shl(rax, 3);
        mov(shift, rax);

        xor_(rax, rax);
        not_(rax);
        mov(rcx, shift);
        shr(rax, cl);
        mov(mask, rax);

        mov(rdi, rdx);
        mov(rax, F_ADR(mem::read_dword));
        call(rax);

        mov(rcx, shift);
        shr(rax, cl);

        mov(rcx, RT_x);
        mov(rdi, mask);
        not_(rdi);
        and_(rcx, rdi);
        or_(rax, rcx);

        mov(RT_x, rax);
    }
    return false;
}


bool mips3_x64::LW(uint32_t opcode)
{
    if (RTNUM) {
        GET_EADDR_IN_RDX(3);
        mov(rdi, rdx);
        mov(rax, F_ADR(mem::read_word));
        call(rax);
        cdqe();
        mov(RT_x, rax);
    }
    return false;
}

bool mips3_x64::LWU(uint32_t opcode)
{
    if (RTNUM) {
        GET_EADDR_IN_RDX(3);
        mov(rdi, rdx);
        mov(rax, F_ADR(mem::read_word));
        call(rax);
        mov(RT_x, rax);
    }
    return false;
}

bool mips3_x64::LD(uint32_t opcode)
{
    if (RTNUM) {
        GET_EADDR_IN_RDX(7);
        mov(rdi, rdx);
        mov(rax, F_ADR(mem::read_dword));
        call(rax);
        mov(RT_x, rax);
    }
    return false;
}

bool mips3_x64::LL(uint32_t opcode)
{
    return false;
}

// TODO: FIX IT
bool mips3_x64::LWC1(uint32_t opcode)
{
    return false;
}

// TODO: FIX IT
bool mips3_x64::SWC1(uint32_t opcode)
{
    return false;
}

bool mips3_x64::LB(uint32_t opcode)
{
    if (RTNUM) {
        GET_EADDR_IN_RDX(0);
        mov(rdi, rdx);
        mov(rax, F_ADR(mem::read_byte));
        call(rax);
        movsx(eax, al);
        cdqe();
        mov(RT_x, rax);
    }
    return false;
}

bool mips3_x64::LBU(uint32_t opcode)
{
    if (RTNUM) {
        GET_EADDR_IN_RDX(0);
        mov(rdi, rdx);
        mov(rax, F_ADR(mem::read_byte));
        call(rax);
        movzx(eax, al);
        mov(RT_x, rax);
    }
    return false;
}

bool mips3_x64::LH(uint32_t opcode)
{
    if (RTNUM) {
        GET_EADDR_IN_RDX(1);
        mov(rdi, rdx);
        mov(rax, F_ADR(mem::read_half));
        call(rax);
        movsx(eax, ax);
        cdqe();
        mov(RT_x, rax);
    }
    return false;
}

bool mips3_x64::LHU(uint32_t opcode)
{
    if (RTNUM) {
        GET_EADDR_IN_RDX(1);
        mov(rdi, rdx);
        mov(rax, F_ADR(mem::read_half));
        call(rax);
        movzx(eax, ax);
        mov(RT_x, rax);
    }
    return false;
}

}

#endif // MIPS3_X64_RW

