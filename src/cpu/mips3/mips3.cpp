/*
 *
 * Copyright (c) 2015, Marcos Medeiros
 * Licensed under BSD 3-clause.
 */
/*
 * TODO:
 *    Fix TLB emulation
 *    COP1 FR0 opcodes
 *    Exceptions!!!
 **/

//#include <QDebug>
#include <iostream>
#include "mips3.h"
#include "mipsdef.h"
#include "mips3_memory.h"
#include "mips3_rw.h"
#include "mips3_branch.h"
#include "mips3_bitops.h"
#include "mips3_arithm.h"
#include "mips3_shift.h"
#include "mips3_misc.h"

namespace mips
{

const char *mips3::reg_names[32] = {
    "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
    "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
    "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
    "r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31"
};


mips3::mips3() : m_tlb_entries(48)
{
    tlb_init();
    m_state.total_cycles = 0;
}

mips3::~mips3()
{
    tlb_exit();
}

void mips3::reset()
{
    m_state.pc = 0xBFC00000;
    m_state.hi = 0;
    m_state.lo = 0;
    for (int i = 0; i < 32; i++)
        m_state.r[i] = 0;

    m_next_pc = ~0;
    m_delay_slot = false;
    cop0_reset();
    cop1_reset();
    tlb_flush();
    m_counter = 0;
    m_state.reset_cycle = m_state.total_cycles;
}


inline addr_t mips3::tlb_translate(addr_t address)
{
    for (int i = 0; i < 48; i++) {
        tlb_entry *entry = &m_tlb[i];

        uint32_t vpn2 = (entry->b.hi & ~entry->b.pagemask) >> 13;
        uint32_t vpn = (address & ~entry->b.pagemask) >> 13;

        if (vpn2 == vpn) {
            if (address & 0x1000)
                return (address & 0xFFF) | ((entry->b.odd_lo >> 6) << 12);
            else
                return (address & 0xFFF) | ((entry->b.even_lo >> 6) << 12);
        }
    }
   // qDebug() << "Address error" << QString::number(address,16);
    return address;
}


uint32_t mips3::translate(addr_t addr, addr_t *out)
{
    switch (addr & 0xE0000000) {
    case 0x80000000: *out = addr - 0x80000000; break; /* cached */
    case 0xA0000000: *out = addr - 0xA0000000; break;
    default:
        /* traduzir pela TLB */
        *out = tlb_translate(addr);
        return 1;
        break;
    }
    return 0;
}


#if MIPS3_ENABLE_BREAKPOINTS

void mips3::bp_insert(addr_t address)
{
    m_breakpoints.insert(address);
}

void mips3::bp_remove(addr_t address)
{
    m_breakpoints.erase(m_breakpoints.find(address));
}

inline bool mips3::check_breakpoint()
{
    return (m_breakpoints.find(m_state.pc) != m_breakpoints.end());
}

#endif

bool mips3::run(int cycles, bool skip_bps)
{
    m_counter = 0;
    int last_icounter = 0;

    // endereço efetivo
    addr_t eaddr = 0;

#if MIPS3_ENABLE_BREAKPOINTS
    if (!skip_bps && check_breakpoint())
        return true;
#endif

    while (m_counter < cycles) {

        if (translate(m_state.pc, &eaddr)) {
            /* TODO: handle exceptions */
        }
        m_prev_pc = m_state.pc;
        uint32_t opcode = mem::read_word(eaddr);

        // We always execute delay slot
        if (m_delay_slot) {
            m_state.pc = m_next_pc;
            m_delay_slot = false;
            m_next_pc = ~0;
        } else {
            m_state.pc += 4;
        }

        last_icounter = m_counter;

        switch (opcode >> 26) {

        // SPECIAL
        case 0x00:
        {
            switch (opcode & 0x3F) {
            // SLL rd, rt, shamt
            case 0x00: SLL(opcode); break;

            // SRL rd, rt, shamt
            case 0x02: SRL(opcode); break;

            // SRA rd, rt, shamt
            case 0x03: SRA(opcode); break;

            // SLLV rd, rt, rs
            case 0x04: SLLV(opcode); break;

            // SRLV rd, rt, rs
            case 0x06: SRLV(opcode); break;

            // SRAV rd, rt, rs
            case 0x07: SRAV(opcode); break;

            // JR rs
            case 0x08: JR(opcode); break;

            // JALR rs, rd
            case 0x09: JALR(opcode); break;

            // SYSCALL > TODO
            case 0x0C:
                break;

            // MFHI rd
            case 0x10: MFHI(opcode); break;

            // MTHI rs
            case 0x11: MTHI(opcode); break;

            // MFLO rd
            case 0x12: MFLO(opcode); break;

            // MTLO rs
            case 0x13: MTLO(opcode); break;

            // DSLLV rd, rt, rs
            case 0x14: DSLLV(opcode); break;

            // DSLRV rd, rt, rs
            case 0x16: DSLRV(opcode); break;

            // DSRAV rd, rt, rs
            case 0x17: DSRAV(opcode); break;

            // MULT rs, rt
            case 0x18: MULT(opcode); break;

            // MULTU rs, rt
            case 0x19: MULTU(opcode); break;

            // DIV rs, rt
            case 0x1A: DIV(opcode); break;

            // DIVU rs, rt
            case 0x1B: DIVU(opcode); break;

            // DMULT rs, rt
            case 0x1C: DMULT(opcode); break;

            // DDIV rs, rt
            case 0x1E: DDIV(opcode); break;

            // DDIVU rs, rt
            case 0x1F: DDIVU(opcode); break;

            // ADD rd, rs, rt
            case 0x20: ADD(opcode); break;

            // ADDU rd, rs, rt
            case 0x21: ADDU(opcode); break;

            // SUB rd, rs, rt
            case 0x22: SUB(opcode); break;

            // SUBU rd, rs, rt
            case 0x23: SUBU(opcode); break;

            // AND rd, rs, rt
            case 0x24: AND(opcode); break;

            // OR rd, rs, rt
            case 0x25: OR(opcode); break;

            // XOR rd, rs, rt
            case 0x26: XOR(opcode); break;

            // NOR rd, rs, rt
            case 0x27: NOR(opcode); break;

            // SLT rd, rs, rt
            case 0x2A: SLT(opcode); break;

            // SLTU rd, rs, rt
            case 0x2B: SLTU(opcode); break;

            // DADDU rd, rs, rt
            case 0x2D: DADDU(opcode); break;

            // DSUBU rd, rs, rt
            case 0x2F: DSUBU(opcode); break;

            // DSLL rd, rt, shamt
            case 0x38: DSLL(opcode); break;

            // DSRL rd, rt, shamt
            case 0x3A: DSRL(opcode); break;

            // DSRA rd, rt, shamt
            case 0x3B: DSRA(opcode); break;

            // DSLL32 rd, rt, shamt
            case 0x3C: DSLL32(opcode); break;

            // DSRL32 rd, rt, shamt
            case 0x3E: DSRL32(opcode); break;

            // DSRA32 rd, rt, shamt
            case 0x3F: DSRA32(opcode); break;

            default:
                cout << "Op: " << (opcode & 0x3F) << " [SPECIAL]" << endl;
                break;
            }
            break;
        }

        // REGIMM
        case 0x01:
        {
            switch ((opcode >> 16) & 0x1F) {
            // BLTZ rs, offset
            case 0x00: BLTZ(opcode); break;

            // BGEZ rs, offset
            case 0x01: BGEZ(opcode); break;

            // BLTZAL rs, offset
            case 0x10: BLTZAL(opcode); break;

            // BGEZAL rs, offset
            case 0x11: BGEZAL(opcode); break;

            default:
                cout << "Op: " << ((opcode >> 16) & 0x1F) << " [REGIMM]" << endl;
                break;
            }

            break;
        }

        // J target
        case 0x02: J(opcode); break;

        // JAL target
        case 0x03: JAL(opcode); break;

        // BEQ rs, rt, offset
        case 0x04: BEQ(opcode); break;

        // BNE rs, rt, offset
        case 0x05: BNE(opcode); break;

        // BLEZ rs, rt, offset
        case 0x06: BLEZ(opcode); break;

        // BGTZ rs, offset
        case 0x07: BGTZ(opcode); break;

        // ADDI rt, rs, imm
        case 0x08: ADDI(opcode); break;

        // ADDIU rt, rs, imm
        case 0x09: ADDIU(opcode); break;

        // SLTI rt, rs, imm
        case 0x0A: SLTI(opcode); break;

        // SLTIU rt, rs, imm
        case 0x0B: SLTIU(opcode); break;

        // ANDI rt, rs, imm
        case 0x0C: ANDI(opcode); break;

        // ORI rt, rs, imm
        case 0x0D: ORI(opcode); break;

        // XORI rt, rs, imm
        case 0x0E: XORI(opcode); break;

        // LUI rt, imm
        case 0x0F: LUI(opcode); break;

        // COP0
        case 0x10: cop0_execute(opcode); break;

        // COP1 (FPU) > TODO
        case 0x11:
            if (m_state.cpr[0][COP0_SR] & (1 << 26))
                cop1_execute_32(opcode);
            else
                cop1_execute_16(opcode);
            break;

        // DADDI rt, rs, imm
        case 0x18: DADDI(opcode); break;

        // DADDIU rt, rs, imm
        case 0x19: DADDIU(opcode); break;

        // LDL rt, offset(base)
        case 0x1A: LDL(opcode); break;

        // LDR rt, offset(base)
        case 0x1B: LDR(opcode); break;

        // LB rt, offset(base)
        case 0x20: LB(opcode); break;

        // LH rt, offset(base)
        case 0x21: LH(opcode); break;

        // LWL rt, offset(base)
        case 0x22: LWL(opcode); break;

        // LW rt, offset(base)
        case 0x23: LW(opcode); break;

        // LBU rt, offset(base)
        case 0x24: LBU(opcode); break;

        // LHU rt, offset(base)
        case 0x25: LHU(opcode); break;

        // LWR rt, offset(base)
        case 0x26: LWR(opcode); break;

        // LWU rt, offset(base)
        case 0x27: LWU(opcode); break;

        // SB rt, offset(base)
        case 0x28: SB(opcode); break;

        // SB rt, offset(base)
        case 0x29: SH(opcode); break;

        // SW rt, offset(base)
        case 0x2B: SW(opcode); break;

        // SDL rt, offset(base)
        case 0x2C: SDL(opcode); break;

        // SDR rt, offset(base)
        case 0x2D: SDR(opcode); break;

        // CACHE
        case 0x2F: break;

        // LWC1 rt, offset(base)
        case 0x31: LWC1(opcode); break;

        // LD rt, offset(base)
        case 0x37: LD(opcode); break;

        // SWC1 rt, offset(base)
        case 0x39: SWC1(opcode); break;

        // SD rt, offset(base)
        case 0x3F: SD(opcode); break;

        default:
            cout << "Op: " << (opcode >> 26) << endl;
            break;
        }

        m_counter++;
        m_state.total_cycles += m_counter - last_icounter;
#if MIPS3_ENABLE_BREAKPOINTS
        if (!skip_bps && check_breakpoint())
            return true;
#endif
    }
    return false;
}

}
