/*
 * Copyright (c) 2015, Marcos Medeiros
 * Licensed under BSD 3-clause.
 */
#ifndef TMS34010_ARITHM_H
#define TMS34010_ARITHM_H

#include "tms34010.h"
#include "tms34010_memacc.h"
#include "tms34010_defs.h"


namespace tms { namespace ops {

#define update_vc(sum, a, b)                 \
    do {                                        \
        _st &= ~(ST_C | ST_V);                   \
        if ((sum ^ a) & (sum ^ b) & SIGN_BIT32) \
            _st |= ST_V;                         \
        if ((dword)~(a) < (dword)(b))            \
            _st |= ST_C;                         \
    } while (0)

#define update_vc_sub(sum, a, b)                 \
    do {                                        \
        _st &= ~(ST_C | ST_V);                   \
        if ((a ^ b) & (a ^ sum) & SIGN_BIT32) \
            _st |= ST_V;                            \
        if ((dword)(b) > (dword)(a))            \
            _st |= ST_C;                         \
    } while (0)


void abs_rd(cpu_state *cpu, word opcode)
{
    _st &= ~(ST_Z | ST_N | ST_V);
    if (!_rd)
        _st |= ST_Z;
    dword res = 0 - _rd;
    if (!(res & SIGN_BIT32))
        _rd = res;
    else
        _st |= ST_N;

    if (res == (sdword)0x80000000)
        _st |= ST_V;

    CONSUME_CYCLES(1);
}

void add_rs_rd(cpu_state *cpu, word opcode)
{
    dword sum = _rd + _rs;
    update_vc(sum, _rd, _rs);
     _rd = sum;
    update_zn(_rd);

    CONSUME_CYCLES(1);
}

void addc_rs_rd(cpu_state *cpu, word opcode)
{
    dword sum = _rd + _rs + (CF ? 1 : 0);
    update_vc(sum, _rd, _rs);
    _rd = sum;
    update_zn(_rd);

    CONSUME_CYCLES(1);
}

void addi_iw_rd(cpu_state *cpu, word opcode)
{
    sdword iw = wsign_ext(mem_read(_pc));
    _pc += 16;
    dword sum = _rd + iw;
    update_vc(sum, _rd, iw);
     _rd = sum;
    update_zn(_rd);

    CONSUME_CYCLES(2);
}

void addi_il_rd(cpu_state *cpu, word opcode)
{
    sdword il = mem_read_d(_pc);
    _pc += 32;
    dword sum = _rd + il;
    update_vc(sum, _rd, il);
    _rd = sum;
    update_zn(_rd);

    CONSUME_CYCLES(3);
}

void add_k_rd(cpu_state *cpu, word opcode)
{
    dword k = fw_lut[K];
    uint64_t sum = _rd + k;
    update_vc(sum, _rd, k);
    _rd = sum;
    update_zn(_rd);

    CONSUME_CYCLES(1);
}

void addxy_rs_rd(cpu_state *cpu, word opcode)
{
    _st &= ~(ST_N | ST_V | ST_Z | ST_C);
    _rdx += _rsx;
    _rdy += _rsy;
    if (!_rdx)
        _st |= ST_N;
    if (!_rdy)
        _st |= ST_Z;
    if (_rdy & SIGN_BIT16)
        _st |= ST_C;
    if (_rdx & SIGN_BIT16)
        _st |= ST_V;

    CONSUME_CYCLES(1);
}

void and_rs_rd(cpu_state *cpu, word opcode)
{
    _rd &= _rs;
    update_z(_rd);

    CONSUME_CYCLES(1);
}

void andi_il_rd(cpu_state *cpu, word opcode)
{
    dword il = ~mem_read_d(_pc);
    _pc += 32;
    _rd &= il;
    update_z(_rd);
    CONSUME_CYCLES(3);
}

void andn_rs_rd(cpu_state *cpu, word opcode)
{
    _rd &= ~_rs;
    update_z(_rd);

    CONSUME_CYCLES(1);
}

void btst_k_rd(cpu_state *cpu, word opcode)
{
    if (_rd & (1 << KN))
        _st &= ~ST_Z;
    else
        _st |= ST_Z;

    CONSUME_CYCLES(1);
}

void btst_rs_rd(cpu_state *cpu, word opcode)
{
    if (_rd & (1 << (_rs & 0x1F)))
        _st &= ~ST_Z;
    else
        _st |= ST_Z;

    CONSUME_CYCLES(2);
}


void clrc(cpu_state *cpu, word opcode)
{
    _st &= ~ST_C;

    CONSUME_CYCLES(1);
}


void cmp_rs_rd(cpu_state *cpu, word opcode)
{
    uint64_t res = _rd - _rs;
    update_zn(res);
    update_vc_sub(res, _rd, _rs);

    CONSUME_CYCLES(1);
}


void cmpi_iw_rd(cpu_state *cpu, word opcode)
{
    sdword iw = (sword)(~mem_read(_pc));
    _pc += 16;
    uint64_t res = _rd - iw;
    update_zn(res);
    update_vc_sub(res, _rd, iw);

    CONSUME_CYCLES(2);
}

void cmpi_il_rd(cpu_state *cpu, word opcode)
{
    sdword il = ~mem_read_d(_pc);
    _pc += 32;
    uint64_t res = _rd - il;
    update_zn(res);
    update_vc_sub(res, _rd, il);

    CONSUME_CYCLES(3);
}


void cmpxy_rs_rd(cpu_state *cpu, word opcode)
{
    _st &= ~(ST_N | ST_V | ST_Z | ST_C);

    sword r = _rdx - _rsx;
    if (r == 0)
        _st |= ST_N;
    if (r & SIGN_BIT16)
        _st |= ST_V;

    r = _rdy - _rsy;
    if (r == 0)
        _st |= ST_Z;
    if (r & SIGN_BIT16)
        _st |= ST_C;

    CONSUME_CYCLES(3);
}


// TODO: FIX
void divs_rs_rd(cpu_state *cpu, word opcode)
{
    _st &= ~(ST_Z | ST_V | ST_N);
    if (!_rs)
        _st |= ST_V;

    sdword rd = _rd;
    const sdword rs = _rs;

    if (ODD_RD) {
        if (rs) {
            rd /= rs;
            update_zn(rd);
        }
        _rd = rd;
        CONSUME_CYCLES(39);
    } else {
        i64 x = (static_cast<u64>(R(RD_n)) << 32) | R(RD_n + 1);

        if (rs) {
            i64 res = x / rs;
            i64 mod = x % rs;
            if ((res >> 32) != 0)
                _st |= ST_V;
            else {
                R(RD_n+0) = res;
                R(RD_n+1) = mod;
                if (!res)
                    _st |= ST_Z;
            }
        }
        CONSUME_CYCLES(40);
    }
}

void divu_rs_rd(cpu_state *cpu, word opcode)
{
    _st &= ~(ST_Z | ST_V);
    if (!_rs)
        _st |= ST_V;

    if (RD_n & 1) {
        if (_rs)
            _rd = _rd / _rs;
        if (!_rd)
            _st |= ST_Z;
    } else {
        u64 x = (static_cast<u64>(R(RD_n)) << 32) | R(RD_n + 1);

        if (_rs) {
            u64 res = x / (dword)_rs;
            u64 mod = x % (dword)_rs;
            if ((res >> 32) != 0)
                _st |= ST_V;
            else {
                R(RD_n+0) = res;
                R(RD_n+1) = mod;
                if (!res)
                    _st |= ST_Z;
            }
        }
    }

    CONSUME_CYCLES(37);
}


void lmo_rs_rd(cpu_state *cpu, word opcode)
{
    dword val = _rs;
    update_z(_rs);
    int i = 0;
    if (_rs) {
        while (!(val & 0x80000000)) {
            val <<= 1;
            i++;
        }
    }
    _rd = i;

    CONSUME_CYCLES(1);
}


void mods_rs_rd(cpu_state *cpu, word opcode)
{
    _st &= ~(ST_Z | ST_V | ST_N);
    if (_rs) {
        _rd = static_cast<sdword>(_rd) % static_cast<sdword>(_rs);
        update_zn(_rd);
    } else {
        _st |= ST_V;
    }

    CONSUME_CYCLES(40);
}



void modu_rs_rd(cpu_state *cpu, word opcode)
{
    _st &= ~(ST_Z | ST_V);
    if (_rs) {
        _rd = _rd % _rs;
        if (!_rd)
            _st |= ST_Z;
    } else {
        _st |= ST_V;
    }

    CONSUME_CYCLES(35);
}


// FIX?
void mpys_rs_rd(cpu_state *cpu, word opcode)
{
    const int shift = 32 - FW1;
    sdword mm = ((sdword)(_rs << shift)) >> shift;
    i64 res = ((i64)(uint32_t)_rd) * ((i64)mm);

    _st &= ~(ST_Z | ST_N);
    if (ODD_RD) {
        _rd_0 = res;
    } else {
        _rd_0 = res >> 32;
        _rd_1 = (dword)res;
    }
    if ((res >> 32) & SIGN_BIT32)
        _st |= ST_N;
    if (res == 0)
        _st |= ST_Z;

    CONSUME_CYCLES(20);
}


void mpyu_rs_rd(cpu_state *cpu, word opcode)
{
    unsigned mm = _rs & (0xFFFFFFFF >> (32 - FW1));
    u64 res = static_cast<u64>(mm) * static_cast<u64>(_rd);
    _st &= ~ST_Z;
    if (RD_n & 1) {
        cpu->r[RD]->value = res;
    } else {
        cpu->r[RD+0]->value = res >> 32;
        cpu->r[RD+1]->value = res;
    }
    if (!res)
        _st |= ST_Z;

    CONSUME_CYCLES(21);
}


void neg_rd(cpu_state *cpu, word opcode)
{
    sdword sum = 0 - _rd;
    update_vc_sub(sum, 0, _rd);
    _rd = sum;
    update_zn(_rd);

    CONSUME_CYCLES(1);
}


void negb_rd(cpu_state *cpu, word opcode)
{
    // TODO

    CONSUME_CYCLES(1);
}


void not_rd(cpu_state *cpu, word opcode)
{
    _rd = ~_rd;
    update_z(_rd);

    CONSUME_CYCLES(1);
}


void or_rs_rd(cpu_state *cpu, word opcode)
{
    _rd |= _rs;
    update_z(_rd);

    CONSUME_CYCLES(1);
}


void ori_il_rd(cpu_state *cpu, word opcode)
{
    _rd |= mem_read_d(_pc);
    _pc += 32;
    update_z(_rd);

    CONSUME_CYCLES(3);
}


void setc(cpu_state *cpu, word opcode)
{
    _st |= ST_C;

    CONSUME_CYCLES(1);
}


void sext_rd_0(cpu_state *cpu, word opcode)
{
    const int shift = 32 - FW0;
    _rd = (((sdword)_rd) << shift) >> shift;
    update_zn(_rd);

    CONSUME_CYCLES(3);
}


void sext_rd_1(cpu_state *cpu, word opcode)
{
    const int shift = 32 - FW1;
    _rd = (((sdword)_rd) << shift) >> shift;
    update_zn(_rd);

    CONSUME_CYCLES(3);
}


void sub_rs_rd(cpu_state *cpu, word opcode)
{
    dword sum = _rd - _rs;
    update_vc_sub(sum, _rd, _rs);
    _rd = sum;
    update_zn(_rd);

    CONSUME_CYCLES(1);
}


void subb_rs_rd(cpu_state *cpu, word opcode)
{
    // TODO

    CONSUME_CYCLES(1);
}

void subi_iw_rd(cpu_state *cpu, word opcode)
{
    sdword iw = (sword)(~mem_read(_pc));
    _pc += 16;
    dword sum = _rd - iw;
    update_vc_sub(sum, _rd, iw);
    _rd = sum;
    update_zn(_rd);

    CONSUME_CYCLES(2);
}


void subi_il_rd(cpu_state *cpu, word opcode)
{
    sdword iw = ~mem_read_d(_pc);
    _pc += 32;
    dword sum = _rd - iw;
    update_vc_sub(sum, _rd, iw);
    _rd = sum;
    update_zn(_rd);

    CONSUME_CYCLES(3);
}


void sub_k_rd(cpu_state *cpu, word opcode)
{
    dword k = fw_lut[K];
    uint64_t sum = _rd - k;
    update_vc_sub(sum, _rd, k);
    _rd = sum;
    update_zn(_rd);

    CONSUME_CYCLES(1);
}


void subxy_rs_rd(cpu_state *cpu, word opcode)
{
    _st &= ~(ST_N | ST_V | ST_Z | ST_C);

    if (_rsx == _rdx)
        _st |= ST_N;
    if (_rsy > _rdy)
        _st |= ST_C;
    if (_rsy == _rdy)
        _st |= ST_Z;
    if (_rsx > _rdx)
        _st |= ST_V;

    _rdx -= _rsx;
    _rdy -= _rsy;

    CONSUME_CYCLES(1);
}


void xor_rs_rd(cpu_state *cpu, word opcode)
{
    _rd ^= _rs;
    update_z(_rd);

    CONSUME_CYCLES(1);
}

void xori_il_rd(cpu_state *cpu, word opcode)
{
    _rd ^= mem_read_d(_pc);
    _pc += 32;
    update_z(_rd);

    CONSUME_CYCLES(3);
}


void zext_rd_0(cpu_state *cpu, word opcode)
{
    const int shift = 32 - FW0;
    _rd &= 0xFFFFFFFF >> shift;
    update_z(_rd);

    CONSUME_CYCLES(1);
}


void zext_rd_1(cpu_state *cpu, word opcode)
{
    const int shift = 32 - FW1;
    _rd &= 0xFFFFFFFF >> shift;
    update_z(_rd);

    CONSUME_CYCLES(1);
}


} // ops
} // tms

#endif // TMS34010_ARITHM_H
