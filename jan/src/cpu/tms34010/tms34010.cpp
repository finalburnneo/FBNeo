/*
 * Copyright (c) 2015, Marcos Medeiros
 * Licensed under BSD 3-clause.
 */
#include "tms34010.h"
#include "memory.h"
#include "tms34010_memacc.h"
#include "tms34010_ctrl.h"
#include "tms34010_mov.h"
#include "tms34010_arithm.h"
#include "tms34010_jump.h"
#include "tms34010_shift.h"
#include "tms34010_gfx.h"

#ifdef TMS34010_DEBUGGER
#include <algorithm>
#include <iterator>
#include <QDebug>
#endif

namespace tms {


#define IO(reg) cpu->io_regs[reg]

const char *io_regs_names[32] = {
    "HESYNC", "HEBLNK", "HSBLNK", "HTOTAL", "VESYNC", "VEBLNK", "VSBLNK",
    "VTOTAL", "DPYCTL", "DPYSTRT", "DPYINT", "CONTROL", "HSTDATA", "HSTADRL",
    "HSTADRH", "HSTCTLL", "HSTCTLH", "INTENB", "INTPEND", "CONVSP", "CONVDP",
    "PSIZE", "PMASK", "RS_70", "RS_80", "RS_90", "RS_A0",
    "DPYTAP", "HCOUNT", "VCOUNT", "DPYADR", "REFCNT"
};

void reset(cpu_state *cpu)
{
    cpu->pc = rdfield_32(VECT_RESET);
    cpu->icounter = 0;
    cpu->st = 0x00000010;
    for (int i = 0; i < 15; i++) {
        cpu->a[i].value = 0;
        cpu->b[i].value = 0;
    }
    for (int i = 0; i < 32; i++) {
        cpu->io_regs[i] = 0;
    }

    memset(cpu->shiftreg, 0, 4096*2);
}

static void check_irq(cpu_state *cpu)
{
    int irq = cpu->io_regs[INTPEND] & cpu->io_regs[INTENB];
    if (!(_st & ST_IE) || !irq)
        return;

    dword trap_addr = 0;
    if (irq & INTERRUPT_HOST) {
        trap_addr = 0xFFFFFEC0;
    }
    else if (irq & INTERRUPT_DISPLAY) {
        trap_addr = 0xFFFFFEA0;
    }
    else if (irq & INTERRUPT_EXTERN_1) {
        trap_addr = 0xFFFFFFC0;
    }
    else if (irq & INTERRUPT_EXTERN_2) {
        trap_addr = 0xFFFFFFA0;
    }
    if (trap_addr) {
        _push(_pc);
        _push(_st);
        _st = 0x10;
        _pc = mem_read_d(trap_addr) & 0xFFFFFFF0;
    }
}

#ifdef TMS34010_DEBUGGER
static void perform_trace(cpu_state *cpu)
{
    int pc_count = std::count(std::begin(cpu->history), std::end(cpu->history), cpu->pc);
    if (pc_count) {
        cpu->loop_counter++;
        return;
    }

    if (cpu->loop_counter) {
        cpu->trace << "\n\t(Loop for " << cpu->loop_counter << " instructions)\n\n";
    }
    cpu->loop_counter = 0;

    char pcbuf[16];
    snprintf(pcbuf, 16, "%08X\t", _pc);
    cpu->trace << pcbuf << new_dasm(_pc, nullptr) << std::endl;

    cpu->history[cpu->history_idx] = cpu->pc;
    cpu->history_idx = (cpu->history_idx + 1) % cpu->history.size();
}

void run(cpu_state *cpu, int cycles, bool stepping)
{
    cpu->icounter = cycles;
    while (cpu->icounter > 0) {

        check_irq(cpu);

        if (!stepping) {
            if (std::find(std::begin(cpu->bpoints), std::end(cpu->bpoints), _pc) !=
                std::end(cpu->bpoints)) {
                cpu->reason = BREAKPOINT_FOUND;
                return;
            }
        }

        if (cpu->tracing && cpu->trace.is_open()) {
            perform_trace(cpu);
        }

        cpu->pc &= 0xFFFFFFF0;

        word opcode = mem_read(cpu->pc);
        cpu->last_pc = cpu->pc;
        cpu->pc += 16;
        opcode_table[(opcode >> 4) & 0xFFF](cpu, opcode);
        if (cpu->icounter == 0)
            return;
    }
    cpu->reason = ICOUNTER_EXPIRED;
}

#else
void run(cpu_state *cpu, int cycles)
{
    cpu->icounter = cycles;
    while (cpu->icounter > 0) {

        check_irq(cpu);
        cpu->pc &= 0xFFFFFFF0;
        word opcode = mem_read(cpu->pc);
        cpu->last_pc = cpu->pc;
        cpu->pc += 16;
        opcode_table[(opcode >> 4) & 0xFFF](cpu, opcode);
    }
}
#endif

void generate_irq(cpu_state *cpu, int num)
{
    cpu->io_regs[INTPEND] |= num;
}

void clear_irq(cpu_state *cpu, int num)
{
    cpu->io_regs[INTPEND] &= ~num;
}

void write_ioreg(cpu_state *cpu, dword addr, word value)
{
    const int reg = (addr >> 4) & 0x1F;
    cpu->io_regs[reg] = value;
    switch (reg) {
    case PSIZE:  cpu->pshift = log2(value); break;
    case CONVDP: cpu->convdp = 1 << (~value & 0x1F); break;
    case CONVSP: cpu->convsp = 1 << (~value & 0x1F); break;
    case INTPEND:
        if (!(value & INTERRUPT_DISPLAY))
            cpu->io_regs[INTPEND] &= ~INTERRUPT_DISPLAY;
        break;
    case DPYSTRT:
        break;
    case VTOTAL:
    case HTOTAL:
    case HSBLNK:
    case HEBLNK:
    case VSBLNK:
    case VEBLNK:
#ifdef TMS34010_DEBUGGER
        qDebug() << QString().sprintf("vga crtc %dx%d - v:%d,h:%d",
            cpu->io_regs[HSBLNK]-cpu->io_regs[HEBLNK],
            cpu->io_regs[VSBLNK]-cpu->io_regs[VEBLNK],
            cpu->io_regs[VTOTAL],
            cpu->io_regs[HTOTAL]);
#endif
        break;
    }

}

dword read_ioreg(cpu_state *cpu, dword addr)
{
    int reg = (addr >> 4) & 0x1F;
    return cpu->io_regs[reg];
}

int generate_scanline(cpu_state *cpu, int line, scanline_render_t render)
{
    int enabled = cpu->io_regs[DPYCTL] & 0x8000;
    cpu->io_regs[VCOUNT] = line;

    if (enabled && line == cpu->io_regs[DPYINT]) {
        generate_irq(cpu, INTERRUPT_DISPLAY);
    }

    if (line == cpu->io_regs[VSBLNK]) {
        cpu->io_regs[DPYADR] = cpu->io_regs[DPYSTRT];
    }

    if (line >= cpu->io_regs[VEBLNK] && line <= cpu->io_regs[VSBLNK] - 1) {
        word dpyadr = cpu->io_regs[DPYADR];
        if (!(cpu->io_regs[DPYCTL] & 0x0400))
            dpyadr ^= 0xFFFC;

        int rowaddr = dpyadr >> 4;
        int coladdr = ((dpyadr & 0x007C) << 4) | (cpu->io_regs[DPYTAP] & 0x3FFF);

        display_info info;
        info.coladdr = coladdr;
        info.rowaddr = rowaddr;
        info.heblnk = cpu->io_regs[HEBLNK];
        info.hsblnk = cpu->io_regs[HSBLNK];
        info.htotal = cpu->io_regs[HTOTAL];
        if (render) {
            render(line, &info);
        }
    }

    if (line >= cpu->io_regs[VEBLNK] && line < cpu->io_regs[VSBLNK]) {
        word dpyadr = cpu->io_regs[DPYADR];
        if ((dpyadr & 3) == 0) {
            dpyadr = ((dpyadr & 0xFFFC) - (cpu->io_regs[DPYCTL] & 0x03FC));
            dpyadr |= (cpu->io_regs[DPYSTRT] & 0x0003);
        } else {
            dpyadr = (dpyadr & 0xfffc) | ((dpyadr - 1) & 3);
        }
        cpu->io_regs[DPYADR] = dpyadr;
    }
    if (++line >= cpu->io_regs[VTOTAL])
        line = 0;
    return line;
}

wrfield_handler wrfield_table[32] = {
    &wrfield_32, &wrfield_1,  &wrfield_2,  &wrfield_3,  &wrfield_4,  &wrfield_5,  &wrfield_6,  &wrfield_7,
    &wrfield_8,  &wrfield_9,  &wrfield_10, &wrfield_11, &wrfield_12, &wrfield_13, &wrfield_14, &wrfield_15,
    &wrfield_16, &wrfield_17, &wrfield_18, &wrfield_19, &wrfield_20, &wrfield_21, &wrfield_22, &wrfield_23,
    &wrfield_24, &wrfield_25, &wrfield_26, &wrfield_27, &wrfield_28, &wrfield_29, &wrfield_30, &wrfield_31,
};

rdfield_handler rdfield_table[64] = {
    &rdfield_32,    &rdfield_1,     &rdfield_2,     &rdfield_3,     &rdfield_4,     &rdfield_5,     &rdfield_6,     &rdfield_7,
    &rdfield_8,     &rdfield_9,     &rdfield_10,    &rdfield_11,    &rdfield_12,    &rdfield_13,    &rdfield_14,    &rdfield_15,
    &rdfield_16,    &rdfield_17,    &rdfield_18,    &rdfield_19,    &rdfield_20,    &rdfield_21,    &rdfield_22,    &rdfield_23,
    &rdfield_24,    &rdfield_25,    &rdfield_26,    &rdfield_27,    &rdfield_28,    &rdfield_29,    &rdfield_30,    &rdfield_31,
    &rdfield_32,    &rdfield_1_sx,  &rdfield_2_sx,  &rdfield_3_sx,  &rdfield_4_sx,  &rdfield_5_sx,  &rdfield_6_sx,  &rdfield_7_sx,
    &rdfield_8_sx,  &rdfield_9_sx,  &rdfield_10_sx, &rdfield_11_sx, &rdfield_12_sx, &rdfield_13_sx, &rdfield_14_sx, &rdfield_15_sx,
    &rdfield_16_sx, &rdfield_17_sx, &rdfield_18_sx, &rdfield_19_sx, &rdfield_20_sx, &rdfield_21_sx, &rdfield_22_sx, &rdfield_23_sx,
    &rdfield_24_sx, &rdfield_25_sx, &rdfield_26_sx, &rdfield_27_sx, &rdfield_28_sx, &rdfield_29_sx, &rdfield_30_sx, &rdfield_31_sx,
};




}
