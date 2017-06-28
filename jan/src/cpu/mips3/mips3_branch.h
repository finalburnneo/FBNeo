/*
 * Copyright (c) 2015, Marcos Medeiros
 * Licensed under BSD 3-clause.
 */
#ifndef MIPS3_BRANCH
#define MIPS3_BRANCH

#include "mips3.h"
#include "mipsdef.h"
#include "mips3_memory.h"

namespace mips
{

inline void mips3::J(uint32_t opcode)
{
    m_next_pc = (m_state.pc & 0xF0000000) | (TARGET << 2);
    m_delay_slot = true;
}

inline void mips3::JR(uint32_t opcode)
{
    m_next_pc = (uint32_t)RS;
    m_delay_slot = true;
}

inline void mips3::JAL(uint32_t opcode)
{
    m_next_pc = (m_state.pc & 0xF0000000) | (TARGET << 2);
    m_delay_slot = true;
    m_state.r[LR] = (int32_t)(m_state.pc + 4);
}

inline void mips3::JALR(uint32_t opcode)
{
    m_next_pc = (uint32_t)RS;
    m_delay_slot = true;
    m_state.r[LR] = (int32_t)(m_state.pc + 4);
}

inline void mips3::BLTZ(uint32_t opcode)
{
    if ((int64_t)RS < 0) {
        m_next_pc = m_state.pc + ((int32_t)(SIMM) << 2);
        m_delay_slot = true;
    }
}

inline void mips3::BLTZAL(uint32_t opcode)
{
    if ((int64_t)RS < 0) {
        m_next_pc = m_state.pc + ((int32_t)(SIMM) << 2);
        m_delay_slot = true;
        m_state.r[LR] = (int32_t)(m_state.pc + 4);
    }
}

inline void mips3::BGEZ(uint32_t opcode)
{
    if ((int64_t)RS >= 0) {
        m_next_pc = m_state.pc + ((int32_t)(SIMM) << 2);
        m_delay_slot = true;
    }
}

inline void mips3::BGEZAL(uint32_t opcode)
{
    if ((int64_t)RS >= 0) {
        m_next_pc = m_state.pc + ((int32_t)(SIMM) << 2);
        m_delay_slot = true;
        m_state.r[LR] = (int32_t)(m_state.pc + 4);
    }
}

inline void mips3::BEQ(uint32_t opcode)
{
    if (RS == RT) {
        m_next_pc = m_state.pc + ((int32_t)(SIMM) << 2);
        m_delay_slot = true;
    }
}

inline void mips3::BNE(uint32_t opcode)
{
    if (RS != RT) {
        m_next_pc = m_state.pc + ((int32_t)(SIMM) << 2);
        m_delay_slot = true;
    }
}

inline void mips3::BLEZ(uint32_t opcode)
{
    if ((int64_t)RS <= 0) {
        m_next_pc = m_state.pc + ((int32_t)(SIMM) << 2);
        m_delay_slot = true;
    }
}

inline void mips3::BGTZ(uint32_t opcode)
{
    if ((int64_t)RS > 0) {
        m_next_pc = m_state.pc + ((int32_t)(SIMM) << 2);
        m_delay_slot = true;
    }
}

}

#endif // MIPS3_BRANCH

