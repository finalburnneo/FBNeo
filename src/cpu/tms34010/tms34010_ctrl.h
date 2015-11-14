/*
 * Copyright (c) 2015, Marcos Medeiros
 * Licensed under BSD 3-clause.
 */
#ifndef TMS34010_CTRL_H
#define TMS34010_CTRL_H

#include "tms34010.h"
#include "tms34010_defs.h"


extern unsigned g_inputs[];
namespace tms { namespace ops {

void dint(cpu_state *cpu, word opcode)
{
    _st &= ~ST_IE;

    CONSUME_CYCLES(3);
}

void eint(cpu_state *cpu, word opcode)
{
    _st |= ST_IE;

    CONSUME_CYCLES(3);
}

void emu(cpu_state *cpu, word opcode)
{
    //  ???
    CONSUME_CYCLES(6);
}

void rev_rd(cpu_state *cpu, word opcode)
{
    _rd = 0x8;

    CONSUME_CYCLES(1);
}

void setf_0(cpu_state *cpu, word opcode)
{
    unsigned fs = opcode & 0x1F;
    _st &= ~(ST_FS0_MASK | ST_FE0);
    _st |= fs | (opcode & 0x20);

    CONSUME_CYCLES(1);
}

void setf_1(cpu_state *cpu, word opcode)
{
    unsigned fs = opcode & 0x1F;
    _st &= ~(ST_FS1_MASK | ST_FE1);
    _st |= (fs | (opcode & 0x20)) << ST_FS1_SHIFT;

    CONSUME_CYCLES(1);
}

void exgf_rd_0(cpu_state *cpu, word opcode)
{
    word k = _rd & 0x3F;
    _rd = FWEX(0);
    _st &= ~(ST_FS0_MASK | ST_FE0);
    _st |= k;

    CONSUME_CYCLES(1);
}


void exgf_rd_1(cpu_state *cpu, word opcode)
{
    word k = _rd & 0x3F;
    _rd = FWEX(1);
    _st &= ~(ST_FS1_MASK | ST_FE1);
    _st |= (k << ST_FS1_SHIFT);

    CONSUME_CYCLES(1);
}

void mmtm(cpu_state *cpu, word opcode)
{
    word list = mem_read(_pc);
    _pc += 16;

#if 0// TMS34020 ???
    _st &= ~ST_N;
    if (!_rd) _st |= ST_N;
    else {
        if (((0 - _rd) & SIGN_BIT32) && !(_rd == 0x80000000))
            _st |= ST_N;
    }
#endif
    CONSUME_CYCLES(2);

    for (int i = 0; i < 16; i++) {
        if (list & 0x8000) {
            _rd -= 32;
            mem_write_d(_rd, cpu->r[i|R_BIT]->value);
            CONSUME_CYCLES(4);
        }
        list <<= 1;
    }
}

void mmfm(cpu_state *cpu, word opcode)
{
    word list = mem_read(_pc);
    _pc += 16;

    CONSUME_CYCLES(3);

    for (int i = 15; i >= 0; i--) {
        if (list & 0x8000) {
            cpu->r[i|R_BIT]->value = mem_read_d(_rd);
            _rd += 32;
            CONSUME_CYCLES(4);
        }
        list <<= 1;
    }
}

void reti(cpu_state *cpu, word opcode)
{
    _st = _pop();
    _pc = _pop() & 0xFFFFFFF0;

    CONSUME_CYCLES(11);
}

void trap(cpu_state *cpu, word opcode)
{
    _push(_pc);
    _push(_st);
    _st &= ~ST_IE;
    _pc = mem_read_d(0xFFFFFFE0 - (opcode & 0x1F) * 32) & 0xFFFFFFF0;

    CONSUME_CYCLES(16);
}

void exgpc(cpu_state *cpu, word opcode)
{
    dword tmp = _rd;
    _rd = _pc;
    _pc = tmp;

    CONSUME_CYCLES(2);
}

void getpc(cpu_state *cpu, word opcode)
{
    _rd = _pc;

    CONSUME_CYCLES(1);
}

void putst(cpu_state *cpu, word opcode)
{
    _st = _rd;

    CONSUME_CYCLES(3);
}

void pushst(cpu_state *cpu, word opcode)
{
    _push(_st);

    CONSUME_CYCLES(2);
}

void popst(cpu_state *cpu, word opcode)
{
    _st = _pop();

    CONSUME_CYCLES(8);
}

void getst(cpu_state *cpu, word opcode)
{
    _rd = _st;

    CONSUME_CYCLES(1);
}

} // ops
} // tms

#endif // TMS34010_CTRL_H
