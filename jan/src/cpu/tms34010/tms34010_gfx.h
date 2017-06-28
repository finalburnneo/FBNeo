/*
 * Copyright (c) 2015, Marcos Medeiros
 * Licensed under BSD 3-clause.
 */
#ifndef TMS34010_GFX_H
#define TMS34010_GFX_H

#include <iostream>
#include "tms34010.h"
#include "tms34010_defs.h"
#include "tms34010_memacc.h"

#ifdef TMS34010_DEBUGGER
#include <QDebug>
#define GFX_LOG(...)    qDebug()<<QString().sprintf(__VA_ARGS__)
#else
#define GFX_LOG(...)
#endif

#define DEBUG_GSP   0

#define DXYTOL(r)  (((r.y * cpu->convdp) | (r.x << cpu->pshift)) + OFFSET)

namespace tms { namespace ops {


void gfx_params_dump(cpu_state *cpu)
{
    for (int i = 0; i < 15; i++) {
        GFX_LOG("%-10s: %08X [%d,%d]", gfx_reg_names[i],
            cpu->b[i].value, cpu->b[i].x, cpu->b[i].y);
    }
}

void pixblt_b_xy(cpu_state *cpu, word opcode)
{
#if DEBUG_GSP
    GFX_LOG("=======> PIXBLT B, XY at %08x", cpu->last_pc);
    gfx_params_dump(cpu);
#endif
    int width = DYDX_X;
    int height = DYDX_Y;
    dword daddr = DXYTOL(DADDR_R);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {

            int bit = rdfield_1(SADDR + x);
            word col = bit ? COLOR1 : COLOR0;
            wrfield_8(daddr + x * 8, col);
        }
        daddr += DPTCH;
        SADDR += SPTCH;
    }
    CONSUME_CYCLES(width*height*3);
}

void pixt_irs_rd(cpu_state *cpu, word opcode)
{
#if DEBUG_GSP
    GFX_LOG("=======> PIXT *RS, RD at %08X", cpu->last_pc);
    gfx_params_dump(cpu);
#endif
    if (cpu->io_regs[DPYCTL] & 0x0800)
    {
        cpu->shift_read_cycle(_rs, cpu->shiftreg);

        _rd = cpu->shiftreg[0] & 0xff;
        if (_rd)
            _st |= ST_V;
        else
            _st &= ~ST_V;
    }
    else
    {

        _rd = rdfield_8(_rs) & 0xff;
        if (_rd)
            _st |= ST_V;
        else
            _st &= ~ST_V;
    }
    CONSUME_CYCLES(3);
}

void fill_xy(cpu_state *cpu, word opcode)
{
#if DEBUG_GSP
    GFX_LOG("FILL XY [%d,%d,%d,%d] <- %04x from %08x -- PPOP %x %x",
        DADDR_X, DADDR_Y, DYDX_X, DYDX_Y, COLOR1, OFFSET, PPOP, cpu->io_regs[PSIZE]);
    gfx_params_dump(cpu);
#endif
    int width = DYDX_X;
    int height = DYDX_Y;

    dword daddr = DXYTOL(DADDR_R);
    if (cpu->io_regs[DPYCTL] & 0x0800)
    {
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
//                wrfield_8(daddr + x * 8, COLOR1);
                cpu->shift_write_cycle(daddr + x * 8, cpu->shiftreg);
            }
            daddr += DPTCH;
        }
    }
    else
    {
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                wrfield_8(daddr + x * 8, COLOR1);
            }
            daddr += DPTCH;
        }
    }
    DADDR_Y += DYDX_Y;
    CONSUME_CYCLES(width*height*3);
}

void fill_l(cpu_state *cpu, word opcode)
{
#if DEBUG_GSP
    GFX_LOG("FILL XY [%d,%d,%d,%d] <- %04x from %08x -- PPOP %x",
        DADDR_X, DADDR_Y, DYDX_X, DYDX_Y, COLOR1, OFFSET, PPOP);
    gfx_params_dump(cpu);
#endif
    int width = DYDX_X;
    int height = DYDX_Y;

    dword daddr = DADDR;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            wrfield_8(daddr + x * 8, COLOR1);
        }
        daddr += DPTCH;
    }
    DADDR = daddr;
    CONSUME_CYCLES(width*height*3);
}


void pixt_rd_irdxy(cpu_state *cpu, word opcode)
{
#if DEBUG_GSP
    GFX_LOG("=======> PIXT RS, *RD.XY at %08x", cpu->last_pc);
    gfx_params_dump(cpu);
#endif
    dword daddr = DXYTOL((*cpu->r[RD]));
    if (cpu->io_regs[DPYCTL] & 0x0800)
    {
        cpu->shift_write_cycle(daddr, cpu->shiftreg);
    }
    else
    {
        wrfield_8(daddr, _rs);
    }
    CONSUME_CYCLES(3);
}


void cvxyl_rs_rd(cpu_state *cpu, word opcode)
{
    _rd = DXYTOL((*cpu->r[RS]));
    CONSUME_CYCLES(1);
}

void drav_rs_rd(cpu_state *cpu, word opcode)
{
#if DEBUG_GSP
    GFX_LOG("=======> DRAV RS,RD at %08x", cpu->last_pc);
    gfx_params_dump(cpu);
#endif
    dword daddr = DXYTOL((*cpu->r[RD]));
    if (cpu->io_regs[DPYCTL] & 0x0800)
    {
        cpu->shift_write_cycle(daddr, cpu->shiftreg);
    }
    else
    {
        wrfield_8(daddr, COLOR1);
    }
    _rdx += _rsx;
    _rdy += _rsy;
    CONSUME_CYCLES(3);
}

} // ops
} // tms

#endif // TMS34010_GFX_H
