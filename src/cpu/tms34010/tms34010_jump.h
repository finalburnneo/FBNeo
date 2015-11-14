/*
 * Copyright (c) 2015, Marcos Medeiros
 * Licensed under BSD 3-clause.
 */
#ifndef TMS34010_JUMP_H
#define TMS34010_JUMP_H

#include "tms34010.h"
#include "tms34010_memacc.h"
#include "tms34010_defs.h"


namespace tms { namespace ops {

void dsj(cpu_state *cpu, word opcode)
{
    if (--_rd) {
        _pc += wsign_ext(mem_read(_pc)) * 16 + 16;
        CONSUME_CYCLES(3);
    } else {
        _pc += 16;
        CONSUME_CYCLES(2);
    }
}

void dsjeq(cpu_state *cpu, word opcode)
{
    if (ZF) {
        if (--_rd) {
            _pc += wsign_ext(mem_read(_pc)) * 16 + 16;
            CONSUME_CYCLES(3);
        } else {
            _pc += 16;
            CONSUME_CYCLES(2);
        }
    } else {
        _pc += 16;
        CONSUME_CYCLES(2);
    }
}

void dsjne(cpu_state *cpu, word opcode)
{
    if (!ZF) {
        if (--_rd) {
            _pc += wsign_ext(mem_read(_pc)) * 16 + 16;
            CONSUME_CYCLES(3);
        } else {
            _pc += 16;
            CONSUME_CYCLES(2);
        }
    } else {
        _pc += 16;
        CONSUME_CYCLES(2);
    }
}

void dsjs(cpu_state *cpu, word opcode)
{
    if (--_rd) {
        _pc += (BKW_DIR ? -OFFS : OFFS) * 16;
         CONSUME_CYCLES(3);
    } else {
         CONSUME_CYCLES(2);
    }
}

void call_rs(cpu_state *cpu, word opcode)
{
    _push(_pc);
    _pc = _rd & 0xFFFFFFF0;

    CONSUME_CYCLES(3);
}

void calla(cpu_state *cpu, word opcode)
{
    _push(_pc + 32);
    _pc = mem_read_d(_pc);

     CONSUME_CYCLES(4);
}

void callr(cpu_state *cpu, word opcode)
{
    _push(_pc + 16);
    _pc += wsign_ext(mem_read(_pc)) * 16 + 16;

     CONSUME_CYCLES(3);
}

void rets(cpu_state *cpu, word opcode)
{
    _pc = _pop() & 0xFFFFFFF0;
    if (opcode & 0x1F) {
        _sp += (opcode & 0x1F) * 16;
    }
    CONSUME_CYCLES(7);
}

void jump_rs(cpu_state *cpu, word opcode)
{
    _pc = _rd;
    CONSUME_CYCLES(2);
}

#define JR(cc)                                  \
    if (cc) {                                   \
        _pc += ((sbyte)(opcode & 0xFF)) * 16;   \
        CONSUME_CYCLES(2);                      \
    } else {                                    \
        CONSUME_CYCLES(1);                      \
    }

#define JR_0(cc)                                       \
    if ((opcode & 0xFF) == 0) {                        \
        if (cc) {                                      \
            _pc += wsign_ext(mem_read(_pc)) * 16 + 16; \
            CONSUME_CYCLES(3);                         \
        } else {                                       \
            _pc += 16;                                 \
            CONSUME_CYCLES(4);                         \
        }                                              \
    } else JR(cc)

#define JR_8(cc)                                   \
    if ((opcode & 0xFF) == 0x80) {                 \
        if (cc) {                                  \
            _pc = mem_read_d(_pc);                 \
            CONSUME_CYCLES(3);                     \
        } else {                                   \
            _pc += 32;                             \
            CONSUME_CYCLES(4);                     \
        }                                          \
    } else JR(cc)


void jr_uc(cpu_state *cpu, word opcode) { JR(true); }
void jr_uc_0(cpu_state *cpu, word opcode) { JR_0(true); }
void jr_uc_8(cpu_state *cpu, word opcode) { JR_8(true); }

void jr_ne(cpu_state *cpu, word opcode) { JR(!ZF); }
void jr_ne_0(cpu_state *cpu, word opcode) { JR_0(!ZF); }
void jr_ne_8(cpu_state *cpu, word opcode) { JR_8(!ZF); }

void jr_eq(cpu_state *cpu, word opcode) { JR(ZF); }
void jr_eq_0(cpu_state *cpu, word opcode) { JR_0(ZF); }
void jr_eq_8(cpu_state *cpu, word opcode) { JR_8(ZF); }

void jr_nc(cpu_state *cpu, word opcode) { JR(!CF); }
void jr_nc_0(cpu_state *cpu, word opcode) { JR_0(!CF); }
void jr_nc_8(cpu_state *cpu, word opcode) { JR_8(!CF); }

void jr_c(cpu_state *cpu, word opcode) { JR(CF); }
void jr_c_0(cpu_state *cpu, word opcode) { JR_0(CF); }
void jr_c_8(cpu_state *cpu, word opcode) { JR_8(CF); }

void jr_v(cpu_state *cpu, word opcode) { JR(VF); }
void jr_v_0(cpu_state *cpu, word opcode) { JR_0(VF); }
void jr_v_8(cpu_state *cpu, word opcode) { JR_8(VF); }

void jr_nv(cpu_state *cpu, word opcode) { JR(!VF); }
void jr_nv_0(cpu_state *cpu, word opcode) { JR_0(!VF); }
void jr_nv_8(cpu_state *cpu, word opcode) { JR_8(!VF); }

void jr_n(cpu_state *cpu, word opcode) { JR(NF); }
void jr_n_0(cpu_state *cpu, word opcode) { JR_0(NF); }
void jr_n_8(cpu_state *cpu, word opcode) { JR_8(NF); }

void jr_nn(cpu_state *cpu, word opcode) { JR(!NF); }
void jr_nn_0(cpu_state *cpu, word opcode) { JR_0(!NF); }
void jr_nn_8(cpu_state *cpu, word opcode) { JR_8(!NF); }

void jr_hi(cpu_state *cpu, word opcode) { JR(!CF&&!ZF); }
void jr_hi_0(cpu_state *cpu, word opcode) { JR_0(!CF&&!ZF); }
void jr_hi_8(cpu_state *cpu, word opcode) { JR_8(!CF&&!ZF); }

void jr_ls(cpu_state *cpu, word opcode) { JR(CF||ZF); }
void jr_ls_0(cpu_state *cpu, word opcode) { JR_0(CF||ZF); }
void jr_ls_8(cpu_state *cpu, word opcode) { JR_8(CF||ZF); }


#define GT_COND (NF&&VF&&!ZF)||(!NF&&!VF&&!ZF)
void jr_gt(cpu_state *cpu, word opcode) { JR(GT_COND); }
void jr_gt_0(cpu_state *cpu, word opcode) { JR_0(GT_COND); }
void jr_gt_8(cpu_state *cpu, word opcode) { JR_8(GT_COND); }

#define GE_COND (NF&&VF)||(!NF&&!VF)
void jr_ge(cpu_state *cpu, word opcode) { JR(GE_COND); }
void jr_ge_0(cpu_state *cpu, word opcode) { JR_0(GE_COND); }
void jr_ge_8(cpu_state *cpu, word opcode) { JR_8(GE_COND); }

#define LT_COND (NF&&!VF)||(!NF&&VF)
void jr_lt(cpu_state *cpu, word opcode) { JR(LT_COND); }
void jr_lt_0(cpu_state *cpu, word opcode) { JR_0(LT_COND); }
void jr_lt_8(cpu_state *cpu, word opcode) { JR_8(LT_COND); }


#define LE_COND (NF && !VF) || (!NF && VF) || ZF
void jr_le(cpu_state *cpu, word opcode) { JR(LE_COND); }
void jr_le_0(cpu_state *cpu, word opcode) { JR_0(LE_COND); }
void jr_le_8(cpu_state *cpu, word opcode) { JR_8(LE_COND); }

#define P_COND (!NF && !ZF)
void jr_p(cpu_state *cpu, word opcode) { JR(P_COND); }
void jr_p_0(cpu_state *cpu, word opcode) { JR_0(P_COND); }
void jr_p_8(cpu_state *cpu, word opcode) { JR_8(P_COND); }

} // ops
} // tms

#endif // TMS34010_JUMP_H
