/*
 * Copyright (c) 2015, Marcos Medeiros
 * Licensed under BSD 3-clause.
 */
#ifndef MIPS3_X64_BRANCH
#define MIPS3_X64_BRANCH

#include "../mips3.h"
#include "../mips3_memory.h"
#include "mips3_x64_defs.h"
#include "mips3_x64.h"
#include "xbyak/xbyak.h"

namespace mips
{

bool mips3_x64::J(uint32_t opcode)
{
    if (m_is_delay_slot)
        return true;

    addr_t eaddr = 0;
    addr_t nextpc = (m_drc_pc & 0xF0000000) | (TARGET << 2);

    m_core->translate(m_drc_pc, &eaddr);
    uint32_t next_opcode = mem::read_word(eaddr);

    m_drc_pc += 4;
    m_is_delay_slot = true;
    if (compile_instruction(next_opcode)) {
        drc_log("Branch on delay slot!!! aborting...");
        exit(-1);
    }
    m_is_delay_slot = false;

    update_icounter();

    jmp_to_block(nextpc);
    return true;
}

inline bool mips3_x64::JR(uint32_t opcode)
{
    if (m_is_delay_slot)
        return true;

    addr_t eaddr;
    m_core->translate(m_drc_pc, &eaddr);
    uint32_t next_opcode = mem::read_word(eaddr);

    m_drc_pc += 4;
    m_is_delay_slot = true;
    if (compile_instruction(next_opcode)) {
        drc_log("Branch on delay slot!!! aborting...");
        exit(-1);
    }
    m_is_delay_slot = false;

    update_icounter();

    jmp_to_register(RSNUM);
    return true;
}

inline bool mips3_x64::JAL(uint32_t opcode)
{
    if (m_is_delay_slot)
        return true;

    addr_t eaddr = 0;
    addr_t nextpc = (m_drc_pc & 0xF0000000) | (TARGET << 2);

    m_core->translate(m_drc_pc, &eaddr);
    uint32_t next_opcode = mem::read_word(eaddr);

    m_drc_pc += 4;
    m_is_delay_slot = true;
    if (compile_instruction(next_opcode)) {
        drc_log("Branch on delay slot!!! aborting...");
        exit(-1);
    }
    m_is_delay_slot = false;

    update_icounter();

    mov(eax, (uint32_t)m_drc_pc);
    cdqe();
    mov(rcx, R_ref(31));
    mov(ptr[rcx], rax);

    jmp_to_block(nextpc);
    return true;
}

inline bool mips3_x64::JALR(uint32_t opcode)
{
    if (m_is_delay_slot)
        return true;

    addr_t eaddr;
    m_core->translate(m_drc_pc, &eaddr);
    uint32_t next_opcode = mem::read_word(eaddr);

    m_drc_pc += 4;
    m_is_delay_slot = true;
    if (compile_instruction(next_opcode)) {
        drc_log("Branch on delay slot!!! aborting...");
        exit(-1);
    }
    m_is_delay_slot = false;

    update_icounter();

    mov(eax, (uint32_t)m_drc_pc);
    cdqe();
    mov(rcx, R_ref(31));
    mov(ptr[rcx], rax);

    jmp_to_register(RSNUM);
    return true;
}

inline bool mips3_x64::BLTZ(uint32_t opcode)
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

    cmp(RS_q, 0);
    jge(".false");
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

inline bool mips3_x64::BLTZAL(uint32_t opcode)
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

    cmp(RS_q, 0);
    jge(".false");
    {
        mov(eax, (uint32_t)m_drc_pc);
        cdqe();
        mov(Rn_x(31), rax);

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


inline bool mips3_x64::BGEZ(uint32_t opcode)
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
    cmp(RS_q, 0);
    jl(".false");
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

inline bool mips3_x64::BGEZAL(uint32_t opcode)
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
    cmp(RS_q, 0);
    jl(".false");
    {
        mov(eax, (uint32_t)m_drc_pc);
        cdqe();
        mov(Rn_x(31), rax);

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

inline bool mips3_x64::BEQ(uint32_t opcode)
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
    mov(rax, RS_x);
    cmp(rax, RT_x);
    jne(".false");
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


inline bool mips3_x64::BNE(uint32_t opcode)
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
    mov(rax, RS_x);
    cmp(rax, RT_x);
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

inline bool mips3_x64::BLEZ(uint32_t opcode)
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
    cmp(RS_q, 0);
    jg(".false");
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

inline bool mips3_x64::BGTZ(uint32_t opcode)
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
    cmp(RS_q, 0);
    jle(".false");
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

}

#endif // MIPS3_X64_BRANCH

