/*
 * Copyright (c) 2015, Marcos Medeiros
 * Licensed under BSD 3-clause.
 */
#ifndef MIPS3_H
#define MIPS3_H

#define MIPS3_ENABLE_BREAKPOINTS    0

#include "mips3_common.h"

#include <string>

#if MIPS3_ENABLE_BREAKPOINTS
#include <unordered_set>
#endif

namespace mips
{

enum COP0_Registers {
    COP0_Index = 0,
    COP0_Random,
    COP0_EntryLo0,
    COP0_EntryLo1,
    COP0_Context,
    COP0_PageMask,
    COP0_Wired,
    COP0_Unused0,
    COP0_BadVAddr,
    COP0_Count,
    COP0_EntryHi,
    COP0_Compare,
    COP0_SR,
    COP0_Cause,
    COP0_EPC,
    COP0_PRId,
    COP0_Config,
    COP0_LLAddr,
    COP0_WatchLo,
    COP0_WatchHi,
    COP0_XContext,
    COP0_Unused1,
    COP0_Unused2,
    COP0_Unused3,
    COP0_Unused4,
    COP0_Unused5,
    COP0_ECC,
    COP0_CacheErr,
    COP0_TagLo,
    COP0_TagHi,
    COP0_ErrorEPC
};

#ifdef MIPS3_X64_DRC
class mips3_x64;
#endif

class mips3
{
#ifdef MIPS3_X64_DRC
    friend class mips3_x64;
#endif
public:
    mips3();
    ~mips3();

    void reset();
    bool run(int cycles, bool skip_bps=true);

    string dasm(uint32_t opcode, uint64_t pc);

    enum {
        LR = 31
    };

    addr_t m_next_pc;
    bool m_delay_slot;


    struct tlb_entry {
        union {
            struct {
                uint32_t even_lo;
                uint32_t odd_lo;
                uint32_t hi;
                uint32_t pagemask;
            } b;
            uint64_t v[2];
        };
    };

    tlb_entry *m_tlb;

    struct cpu_state {
        uint64_t r[32];
        uint64_t pc;
        uint64_t lo, hi;

        // coprocessadores
        uint64_t cpr[3][32];
        // fpu control registers (FCR)
        uint64_t fcr[32];
        uint64_t reset_cycle;
        uint64_t total_cycles;
    };
    ALIGN_DECL(16) cpu_state m_state;
    addr_t m_prev_pc;

    static const char *reg_names[];
    static const char *cop0_reg_names[];
    uint32_t translate(addr_t addr, addr_t *out);
    const int m_tlb_entries;

#if MIPS3_ENABLE_BREAKPOINTS
    void bp_insert(addr_t address);
    void bp_remove(addr_t address);
#endif
private:
    int m_counter;
    addr_t tlb_translate(addr_t address);

#if MIPS3_ENABLE_BREAKPOINTS
    unordered_set<addr_t> m_breakpoints;
    bool check_breakpoint();
#endif
    void tlb_init();
    void tlb_flush();
    void cop0_reset();
    void cop0_execute(uint32_t opcode);
    void cop1_reset();
    void cop1_execute_16(uint32_t opcode);
    void cop1_execute_32(uint32_t opcode);

    // Arithmetic
    void ADD(uint32_t opcode);
    void SUB(uint32_t opcode);
    void MULT(uint32_t opcode);
    void DIV(uint32_t opcode);
    void ADDU(uint32_t opcode);
    void SUBU(uint32_t opcode);
    void MULTU(uint32_t opcode);
    void DIVU(uint32_t opcode);

    void ADDI(uint32_t opcode);
    void ADDIU(uint32_t opcode);
    void DADDI(uint32_t opcode);
    void DADDIU(uint32_t opcode);

    void DADD(uint32_t opcode);
    void DSUB(uint32_t opcode);
    void DMULT(uint32_t opcode);
    void DDIV(uint32_t opcode);
    void DADDU(uint32_t opcode);
    void DSUBU(uint32_t opcode);
    void DMULTU(uint32_t opcode);
    void DDIVU(uint32_t opcode);

    // Bitwise
    void AND(uint32_t opcode);
    void XOR(uint32_t opcode);
    void OR(uint32_t opcode);
    void NOR(uint32_t opcode);
    void ANDI(uint32_t opcode);
    void XORI(uint32_t opcode);
    void ORI(uint32_t opcode);

    // Shifts
    void SLL(uint32_t opcode);
    void SRL(uint32_t opcode);
    void SRA(uint32_t opcode);
    void SLLV(uint32_t opcode);
    void SRLV(uint32_t opcode);
    void SRAV(uint32_t opcode);
    void DSLLV(uint32_t opcode);
    void DSLRV(uint32_t opcode);
    void DSRAV(uint32_t opcode);
    void SLT(uint32_t opcode);
    void SLTU(uint32_t opcode);
    void DSLL(uint32_t opcode);
    void DSRL(uint32_t opcode);
    void DSRA(uint32_t opcode);
    void DSLL32(uint32_t opcode);
    void DSRL32(uint32_t opcode);
    void DSRA32(uint32_t opcode);

    // Jump & Branchs
    void J(uint32_t opcode);
    void JR(uint32_t opcode);
    void JAL(uint32_t opcode);
    void JALR(uint32_t opcode);
    void BLTZ(uint32_t opcode);
    void BLTZAL(uint32_t opcode);
    void BGEZ(uint32_t opcode);
    void BGEZAL(uint32_t opcode);
    void BEQ(uint32_t opcode);
    void BNE(uint32_t opcode);
    void BLEZ(uint32_t opcode);
    void BGTZ(uint32_t opcode);

    // Load & Store
    void LUI(uint32_t opcode);
    void SB(uint32_t opcode);
    void SH(uint32_t opcode);
    void SW(uint32_t opcode);
    void SD(uint32_t opcode);
    void SDL(uint32_t opcode);
    void SDR(uint32_t opcode);
    void LWL(uint32_t opcode);
    void LWR(uint32_t opcode);
    void LDL(uint32_t opcode);
    void LDR(uint32_t opcode);
    void LB(uint32_t opcode);
    void LBU(uint32_t opcode);
    void LH(uint32_t opcode);
    void LHU(uint32_t opcode);
    void LW(uint32_t opcode);
    void LWU(uint32_t opcode);
    void LD(uint32_t opcode);
    void LL(uint32_t opcode);
    void LWC1(uint32_t opcode);
    void SWC1(uint32_t opcode);

    // Misc
    void SLTI(uint32_t opcode);
    void SLTIU(uint32_t opcode);
    void MFHI(uint32_t opcode);
    void MTHI(uint32_t opcode);
    void MFLO(uint32_t opcode);
    void MTLO(uint32_t opcode);

    string dasm_cop0(uint32_t opcode, uint64_t pc);
    string dasm_cop1(uint32_t opcode, uint64_t pc);
};

extern mips3 *g_mips;

}

#endif // MIPS3_H
