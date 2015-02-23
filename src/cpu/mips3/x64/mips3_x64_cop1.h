/*
 * Copyright (c) 2015, Marcos Medeiros
 * Licensed under BSD 3-clause.
 */
#ifndef MIPS3_X64_COP1
#define MIPS3_X64_COP1


#include "../mips3.h"
#include "mips3_x64_defs.h"
#include "mips3_x64.h"
#include "xbyak/xbyak.h"

namespace mips
{

bool mips3_x64::BC1F(uint32_t opcode)
{
    if (m_is_delay_slot)
        return true;

    addr_t nextpc = m_drc_pc + ((int32_t)(SIMM) << 2);
    addr_t eaddr = 0;

    m_core->translate(m_drc_pc, &eaddr);
    uint32_t next_opcode = mem::read_word(eaddr);

    m_drc_pc += 4;

    update_icounter();

    inLocalLabel();
    mov(rcx, FCR_ref(31));
    mov(rcx, ptr[rcx]);
    not_(ecx);
    and_(ecx, 0x800000);
    test(ecx, ecx);
    je(".false");
    {
        m_is_delay_slot = true;
        compile_instruction(next_opcode);
        m_is_delay_slot = false;
        jmp_to_block(nextpc);
    }

    L(".false");
    compile_instruction(next_opcode);
    outLocalLabel();
    check_icounter();
    return false;

}

bool mips3_x64::BC1FL(uint32_t opcode)
{
    if (m_is_delay_slot)
        return true;

    addr_t nextpc = m_drc_pc + ((int32_t)(SIMM) << 2);
    addr_t eaddr = 0;

    m_core->translate(m_drc_pc, &eaddr);
    uint32_t next_opcode = mem::read_word(eaddr);

    m_drc_pc += 4;

    update_icounter();

    inLocalLabel();
    mov(rcx, FCR_ref(31));
    mov(rcx, ptr[rcx]);
    not_(ecx);
    and_(ecx, 0x800000);
    test(ecx, ecx);
    je(".false");
    {
        m_is_delay_slot = true;
        compile_instruction(next_opcode);
        m_is_delay_slot = false;
        jmp_to_block(nextpc);
    }

    L(".false");
    outLocalLabel();
    check_icounter();
    return false;

    return false;
}

bool mips3_x64::BC1T(uint32_t opcode)
{
    if (m_is_delay_slot)
        return true;

    addr_t nextpc = m_drc_pc + ((int32_t)(SIMM) << 2);
    addr_t eaddr = 0;

    m_core->translate(m_drc_pc, &eaddr);
    uint32_t next_opcode = mem::read_word(eaddr);

    m_drc_pc += 4;

    update_icounter();

    inLocalLabel();
    mov(rcx, FCR_ref(31));
    mov(rcx, ptr[rcx]);
    and_(ecx, 0x800000);
    test(ecx, ecx);
    je(".false");
    {
        m_is_delay_slot = true;
        compile_instruction(next_opcode);
        m_is_delay_slot = false;
        jmp_to_block(nextpc);
    }

    L(".false");
    compile_instruction(next_opcode);
    outLocalLabel();
    check_icounter();
    return false;

}

bool mips3_x64::BC1TL(uint32_t opcode)
{
    if (m_is_delay_slot)
        return true;

    addr_t nextpc = m_drc_pc + ((int32_t)(SIMM) << 2);
    addr_t eaddr = 0;

    m_core->translate(m_drc_pc, &eaddr);
    uint32_t next_opcode = mem::read_word(eaddr);

    m_drc_pc += 4;

    update_icounter();

    inLocalLabel();
    mov(rcx, FCR_ref(31));
    mov(rcx, ptr[rcx]);
    and_(ecx, 0x800000);
    test(ecx, ecx);
    je(".false");
    {
        m_is_delay_slot = true;
        compile_instruction(next_opcode);
        m_is_delay_slot = false;
        jmp_to_block(nextpc);
    }

    L(".false");
    outLocalLabel();
    check_icounter();
    return false;
}

bool mips3_x64::compile_cop1(uint32_t opcode)
{
    bool result = false;

    switch (RSNUM) {
    // MFC1 rt, rd
    case 0x00:
        if (RTNUM) {
            mov(rax, FPR_ref(RDNUM));
            mov(rcx, RT_ref);
            mov(eax, ptr[rax]);
            cdqe();
            mov(ptr[rcx], rax);
        }
        break;

    // DMFC1 rt, rd
    case 0x01:
        if (RTNUM) {
            mov(rax, FPR_ref(RDNUM));
            mov(rcx, RT_ref);
            mov(rax, ptr[rax]);
            mov(ptr[rcx], rax);
        }
        break;

    // CFC1 rt, fs
    case 0x02:
        if (RTNUM) {
            mov(rax, FCR_ref(FSNUM));
            mov(rcx, RT_ref);
            mov(eax, ptr[rax]);
            cdqe();
            mov(ptr[rcx], rax);
        }
        break;

    // MTC1 rt, fs
    case 0x04:
        mov(rax, FPR_ref(FSNUM));
        mov(rcx, RT_ref);
        mov(ecx, ptr[rcx]);
        mov(ptr[rax], rcx);
        break;

    // DMTC1 rt, fs
    case 0x05:
        mov(rax, FPR_ref(FSNUM));
        mov(rcx, RT_ref);
        mov(rcx, ptr[rcx]);
        mov(ptr[rax], rcx);
        break;

    // CTC1 rt, fs
    case 0x06:
        mov(rcx, FCR_ref(FSNUM));
        mov(rax, RT_ref);
        mov(eax, ptr[rax]);
        cdqe();
        mov(ptr[rcx], rax);
        break;

    // BC
    case 0x08:
    {
        switch ((opcode >> 16) & 3) {
        // BC1F offset
        case 0x00: result = BC1F(opcode); break;

        // BC1FL offset
        case 0x02: result = BC1FL(opcode); break;

        // BC1T offset
        case 0x01: result = BC1T(opcode); break;
            break;

        // BC1TL offset
        case 0x03: result = BC1TL(opcode); break;
        }
        break;
    }

    default:
        fallback(opcode, &mips3::cop1_execute_32);
        break;
    }
    return result;
}

}

#endif // MIPS3_X64_COP1

