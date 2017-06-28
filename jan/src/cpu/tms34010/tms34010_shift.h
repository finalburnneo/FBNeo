/*
 * Copyright (c) 2015, Marcos Medeiros
 * Licensed under BSD 3-clause.
 */
#ifndef TMS34010_SHIFT_H
#define TMS34010_SHIFT_H

#include "tms34010.h"
#include "tms34010_memacc.h"
#include "tms34010_defs.h"

namespace tms { namespace ops {

void rl_k_rd(cpu_state *cpu, word opcode)
{
    _st &= ~(ST_C | ST_Z);
    int k = K;
    if (k) {
        const int shift = 32 - k;
        dword rot = (_rd >> shift) & (0xFFFFFFFF >> shift);

        _rd <<= k - 1;
        if (_rd & 0x80000000)
            _st |= ST_C;
        _rd <<= 1;
        _rd |= rot;
    }
    if (!_rd)
        _st |= ST_Z;

    CONSUME_CYCLES(1);
}


void rl_rs_rd(cpu_state *cpu, word opcode)
{
    _st &= ~(ST_C | ST_Z);
    int k = _rs & 0x1F;
    if (k) {
        const int shift = 32 - k;
        dword rot = (_rd >> shift) & (0xFFFFFFFF >> shift);

        _rd <<= k - 1;
        if (_rd & 0x80000000)
            _st |= ST_C;
        _rd <<= 1;
        _rd |= rot;
    }
    if (!_rd)
        _st |= ST_Z;

    CONSUME_CYCLES(1);
}


void sll_rs_rd(cpu_state *cpu, word opcode)
{
    _st &= ~(ST_C | ST_Z);
    int k = _rs & 0x1F;
    if (k) {
        _rd <<= k - 1;
        if (_rd & 0x80000000)
            _st |= ST_C;
        _rd <<= 1;
    }
    if (!_rd)
        _st |= ST_Z;

    CONSUME_CYCLES(1);
}

void sll_k_rd(cpu_state *cpu, word opcode)
{
    _st &= ~(ST_C | ST_Z);
    int k = K;
    if (k) {
        _rd <<= k - 1;
        if (_rd & 0x80000000)
            _st |= ST_C;
        _rd <<= 1;
    }
    if (!_rd)
        _st |= ST_Z;

    CONSUME_CYCLES(1);
}

void sla_k_rd(cpu_state *cpu, word opcode)
{
    _st &= ~(ST_C | ST_V);
    int k = K;
    sdword rr = _rd;
    if (k) {
        const dword mask = (0xFFFFFFFF << (31 - k)) & 0x7FFFFFFF;
        dword rr_ = (_rd & 0x80000000) ? _rd ^ mask : _rd;
        if ((rr_ & mask) != 0)
            _st |= ST_V;

        rr <<= (k - 1);
        if (rr & 0x80000000)
            _st |= ST_C;
        rr <<= 1;
    }
    _rd = rr;
    update_zn(_rd);

    CONSUME_CYCLES(3);
}

void sla_rs_rd(cpu_state *cpu, word opcode)
{
    _st &= ~(ST_C | ST_V);
    int k = _rs & 0x1F;
    sdword rr = _rd;
    if (k) {
        const dword mask = (0xFFFFFFFF << (31 - k)) & 0x7FFFFFFF;
        dword rr_ = (_rd & 0x80000000) ? _rd ^ mask : _rd;
        if ((rr_ & mask) != 0)
            _st |= ST_V;

        rr <<= (k - 1);
        if (rr & 0x80000000)
            _st |= ST_C;
        rr <<= 1;
    }
    _rd = rr;
    update_zn(_rd);

    CONSUME_CYCLES(3);
}

void srl_k_rd(cpu_state *cpu, word opcode)
{
    _st &= ~(ST_C | ST_Z);
    int k = (-K) & 0x1F;
    if (k) {
        _rd >>= k - 1;
        if (_rd & 1)
            _st |= ST_C;
        _rd >>= 1;
    }
    if (!_rd)
        _st |= ST_Z;

    CONSUME_CYCLES(1);
}

void srl_rs_rd(cpu_state *cpu, word opcode)
{
    _st &= ~(ST_C | ST_Z);
    int k = _rs & 0x1F;
    if (k) {
        _rd >>= k - 1;
        if (_rd & 1)
            _st |= ST_C;
        _rd >>= 1;
    }
    if (!_rd)
        _st |= ST_Z;

    CONSUME_CYCLES(1);
}

void sra_k_rd(cpu_state *cpu, word opcode)
{
    _st &= ~(ST_C | ST_Z);
    int k = (-K) & 0x1F;
    sdword rr = _rd;
    if (k) {
        rr >>= k - 1;
        if (rr & 1)
            _st |= ST_C;
        rr >>= 1;
    }
    _rd = rr;
    if (!_rd)
        _st |= ST_Z;

    CONSUME_CYCLES(1);
}

void sra_rs_rd(cpu_state *cpu, word opcode)
{
    _st &= ~(ST_C | ST_Z);
    int k = _rs & 0x1F;
    sdword rr = _rd;
    if (k) {
        rr >>= k - 1;
        if (rr & 1)
            _st |= ST_C;
        rr >>= 1;
    }
    _rd = rr;
    if (!_rd)
        _st |= ST_Z;

    CONSUME_CYCLES(1);
}


} // ops
} // tms

#endif // TMS34010_SHIFT_H
