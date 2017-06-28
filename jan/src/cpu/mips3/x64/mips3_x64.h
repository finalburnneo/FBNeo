/*
 * Copyright (c) 2015, Marcos Medeiros
 * Licensed under BSD 3-clause.
 */
#ifndef MIPS3_X64
#define MIPS3_X64

#include <unordered_map>
#include "xbyak/xbyak.h"
#include "../mips3.h"

#ifdef HAS_UDIS86
#include "udis86/udis86.h"
#endif

namespace mips
{


class mips3_x64 : public Xbyak::CodeGenerator
{
public:
    mips3_x64(mips3 *interpreter);
    void run(int cycles);

private:
    int64_t m_icounter;
    addr_t m_drc_pc;
    bool m_is_delay_slot;
    void run_this(void *ptr);
    void *compile_block(addr_t pc);
    void *get_block(addr_t pc);
    bool compile_special(uint32_t opcode);
    bool compile_regimm(uint32_t opcode);
    bool compile_instruction(uint32_t opcode);
    bool compile_cop0(uint32_t opcode);
    bool compile_cop1(uint32_t opcode);
    void check_icounter();
    bool cop1_fallback(uint32_t opcode);
    void set_next_pc(addr_t addr);
    void fallback(uint32_t opcode, void (mips3::*f)(uint32_t));
    void update_icounter();
    void jmp_to_block(uint64_t addr);
    void jmp_to_register(int reg);
    void translate_failed();
    void update_cp0_count();
    void prolog();
    void epilog(bool do_ret=true);

    uint8_t *m_cache;
    mips3 *m_core;
    void *m_current_block;
    uint64_t m_block_icounter;
    bool m_translate_failed;
    bool m_stop_translation;
    unordered_map<addr_t, void(*)> m_blocks;
#ifdef HAS_UDIS86
    ud_t m_udobj;
#endif

    // COP1 branch
    bool BC1F(uint32_t opcode);
    bool BC1FL(uint32_t opcode);
    bool BC1T(uint32_t opcode);
    bool BC1TL(uint32_t opcode);

    // Arithmetic
    bool ADD(uint32_t opcode);
    bool SUB(uint32_t opcode);
    bool MULT(uint32_t opcode);
    bool DIV(uint32_t opcode);
    bool ADDU(uint32_t opcode);
    bool SUBU(uint32_t opcode);
    bool MULTU(uint32_t opcode);
    bool DIVU(uint32_t opcode);

    bool ADDI(uint32_t opcode);
    bool ADDIU(uint32_t opcode);
    bool DADDI(uint32_t opcode);
    bool DADDIU(uint32_t opcode);

    bool DADD(uint32_t opcode);
    bool DSUB(uint32_t opcode);
    bool DMULT(uint32_t opcode);
    bool DDIV(uint32_t opcode);
    bool DADDU(uint32_t opcode);
    bool DSUBU(uint32_t opcode);
    bool DMULTU(uint32_t opcode);
    bool DDIVU(uint32_t opcode);

    // Bitwise
    bool AND(uint32_t opcode);
    bool XOR(uint32_t opcode);
    bool OR(uint32_t opcode);
    bool NOR(uint32_t opcode);
    bool ANDI(uint32_t opcode);
    bool XORI(uint32_t opcode);
    bool ORI(uint32_t opcode);

    // Shifts
    bool SLL(uint32_t opcode);
    bool SRL(uint32_t opcode);
    bool SRA(uint32_t opcode);
    bool SLLV(uint32_t opcode);
    bool SRLV(uint32_t opcode);
    bool SRAV(uint32_t opcode);
    bool DSLLV(uint32_t opcode);
    bool DSLRV(uint32_t opcode);
    bool SLT(uint32_t opcode);
    bool SLTU(uint32_t opcode);
    bool DSLL(uint32_t opcode);
    bool DSRL(uint32_t opcode);
    bool DSRA(uint32_t opcode);
    bool DSRAV(uint32_t opcode);
    bool DSLL32(uint32_t opcode);
    bool DSRL32(uint32_t opcode);
    bool DSRA32(uint32_t opcode);

    // Jump & Branchs
    bool J(uint32_t opcode);
    bool JR(uint32_t opcode);
    bool JAL(uint32_t opcode);
    bool JALR(uint32_t opcode);
    bool BLTZ(uint32_t opcode);
    bool BLTZAL(uint32_t opcode);
    bool BGEZ(uint32_t opcode);
    bool BGEZAL(uint32_t opcode);
    bool BEQ(uint32_t opcode);
    bool BNE(uint32_t opcode);
    bool BLEZ(uint32_t opcode);
    bool BGTZ(uint32_t opcode);

    // Load & Store
    bool LUI(uint32_t opcode);
    bool SB(uint32_t opcode);
    bool SH(uint32_t opcode);
    bool SW(uint32_t opcode);
    bool SD(uint32_t opcode);
    bool SDL(uint32_t opcode);
    bool SDR(uint32_t opcode);
    bool LWL(uint32_t opcode);
    bool LWR(uint32_t opcode);
    bool LDL(uint32_t opcode);
    bool LDR(uint32_t opcode);
    bool LB(uint32_t opcode);
    bool LBU(uint32_t opcode);
    bool LH(uint32_t opcode);
    bool LHU(uint32_t opcode);
    bool LW(uint32_t opcode);
    bool LWU(uint32_t opcode);
    bool LD(uint32_t opcode);
    bool LL(uint32_t opcode);
    bool LWC1(uint32_t opcode);
    bool SWC1(uint32_t opcode);

    // Misc
    bool SLTI(uint32_t opcode);
    bool SLTIU(uint32_t opcode);
    bool MFHI(uint32_t opcode);
    bool MTHI(uint32_t opcode);
    bool MFLO(uint32_t opcode);
    bool MTLO(uint32_t opcode);

};

}

#endif // MIPS3_X64

