/*
 * Copyright (c) 2015, Marcos Medeiros
 * Licensed under BSD 3-clause.
 */
#include <string>
#include <cstring>
#include "mips3.h"
#include "mipsdef.h"

namespace mips
{

const int dasm_buf_size = 128;
const int dasm_pc_size = 10;

string mips3::dasm(uint32_t opcode, uint64_t pc)
{
    const int size = 128;
    char buf[size];
    char pcb[10];

    buf[0] = '\0';

    snprintf(pcb, dasm_pc_size, "%08X\t", pc);

    switch (opcode >> 26) {
    // SPECIAL
    case 0x00:
        if (opcode == 0) {
            snprintf(buf, dasm_buf_size, "nop");
            break;
        }

        switch (opcode & 0x3F) {
        case 0x00: snprintf(buf, dasm_buf_size, "sll\tr%d, r%d, %d", RDNUM, RTNUM, SHAMT); break;
        case 0x02: snprintf(buf, dasm_buf_size, "srl\tr%d, r%d, %d", RDNUM, RTNUM, SHAMT); break;
        case 0x03: snprintf(buf, dasm_buf_size, "sra\tr%d, r%d, %d", RDNUM, RTNUM, SHAMT); break;
        case 0x04: snprintf(buf, dasm_buf_size, "sllv\tr%d, r%d, r%d", RDNUM, RTNUM, RSNUM); break;
        case 0x06: snprintf(buf, dasm_buf_size, "srlv\tr%d, r%d, r%d", RDNUM, RTNUM, RSNUM); break;
        case 0x07: snprintf(buf, dasm_buf_size, "srav\tr%d, r%d, r%d", RDNUM, RTNUM, RSNUM); break;
        case 0x08: snprintf(buf, dasm_buf_size, "jr\tr%d", RSNUM); break;
        case 0x09: snprintf(buf, dasm_buf_size, "jalr\tr%d, r%d", RDNUM, RSNUM); break;
        case 0x0C: snprintf(buf, dasm_buf_size, "syscall"); break;
        case 0x0D: snprintf(buf, dasm_buf_size, "break"); break;
        case 0x0F: snprintf(buf, dasm_buf_size, "sync"); break;
        case 0x10: snprintf(buf, dasm_buf_size, "mfhi\tr%d", RDNUM); break;
        case 0x11: snprintf(buf, dasm_buf_size, "mthi\tr%d", RSNUM); break;
        case 0x12: snprintf(buf, dasm_buf_size, "mflo\tr%d", RDNUM); break;
        case 0x13: snprintf(buf, dasm_buf_size, "mtlo\tr%d", RSNUM); break;
        case 0x14: snprintf(buf, dasm_buf_size, "dsllv\tr%d, r%d, r%d", RDNUM, RTNUM, RSNUM); break;
        case 0x16: snprintf(buf, dasm_buf_size, "dsrlv\tr%d, r%d, r%d", RDNUM, RTNUM, RSNUM); break;
        case 0x17: snprintf(buf, dasm_buf_size, "dsrav\tr%d, r%d, r%d", RDNUM, RTNUM, RSNUM); break;
        case 0x18: snprintf(buf, dasm_buf_size, "mult\tr%d, r%d", RSNUM, RTNUM); break;
        case 0x19: snprintf(buf, dasm_buf_size, "multu\tr%d, r%d", RSNUM, RTNUM); break;
        case 0x1A: snprintf(buf, dasm_buf_size, "div\tr%d, r%d", RSNUM, RTNUM); break;
        case 0x1B: snprintf(buf, dasm_buf_size, "divu\tr%d, r%d", RSNUM, RTNUM); break;
        case 0x1C: snprintf(buf, dasm_buf_size, "dmult\tr%d, r%d", RSNUM, RTNUM); break;
        case 0x1D: snprintf(buf, dasm_buf_size, "dmultu\tr%d, r%d", RSNUM, RTNUM); break;
        case 0x1E: snprintf(buf, dasm_buf_size, "ddiv\tr%d, r%d", RSNUM, RTNUM); break;
        case 0x1F: snprintf(buf, dasm_buf_size, "ddivu\tr%d, r%d", RSNUM, RTNUM); break;
        case 0x20: snprintf(buf, dasm_buf_size, "add\tr%d, r%d, r%d", RDNUM, RSNUM, RTNUM); break;
        case 0x21: snprintf(buf, dasm_buf_size, "addu\tr%d, r%d, r%d", RDNUM, RSNUM, RTNUM); break;
        case 0x22: snprintf(buf, dasm_buf_size, "sub\tr%d, r%d, r%d", RDNUM, RSNUM, RTNUM); break;
        case 0x23: snprintf(buf, dasm_buf_size, "subu\tr%d, r%d, r%d", RDNUM, RSNUM, RTNUM); break;
        case 0x24: snprintf(buf, dasm_buf_size, "and\tr%d, r%d, r%d", RDNUM, RSNUM, RTNUM); break;
        case 0x25: snprintf(buf, dasm_buf_size, "or\tr%d, r%d, r%d", RDNUM, RSNUM, RTNUM); break;
        case 0x26: snprintf(buf, dasm_buf_size, "xor\tr%d, r%d, r%d", RDNUM, RSNUM, RTNUM); break;
        case 0x27: snprintf(buf, dasm_buf_size, "nor\tr%d, r%d, r%d", RDNUM, RSNUM, RTNUM); break;
        case 0x2A: snprintf(buf, dasm_buf_size, "slt\tr%d, r%d, r%d", RDNUM, RSNUM, RTNUM); break;
        case 0x2B: snprintf(buf, dasm_buf_size, "sltu\tr%d, r%d, r%d", RDNUM, RSNUM, RTNUM); break;
        case 0x2C: snprintf(buf, dasm_buf_size, "dadd\tr%d, r%d, r%d", RDNUM, RSNUM, RTNUM); break;
        case 0x2D: snprintf(buf, dasm_buf_size, "daddu\tr%d, r%d, r%d", RDNUM, RSNUM, RTNUM); break;
        case 0x2E: snprintf(buf, dasm_buf_size, "dsub\tr%d, r%d, r%d", RDNUM, RSNUM, RTNUM); break;
        case 0x2F: snprintf(buf, dasm_buf_size, "dsubu\tr%d, r%d, r%d", RDNUM, RSNUM, RTNUM); break;
        case 0x30: snprintf(buf, dasm_buf_size, "tge\tr%d, r%d", RSNUM, RTNUM); break;
        case 0x31: snprintf(buf, dasm_buf_size, "tgeu\tr%d, r%d", RSNUM, RTNUM); break;
        case 0x32: snprintf(buf, dasm_buf_size, "tlt\tr%d, r%d", RSNUM, RTNUM); break;
        case 0x33: snprintf(buf, dasm_buf_size, "tltu\tr%d, r%d", RSNUM, RTNUM); break;
        case 0x34: snprintf(buf, dasm_buf_size, "teq\tr%d, r%d", RSNUM, RTNUM); break;
        case 0x36: snprintf(buf, dasm_buf_size, "tne\tr%d, r%d", RSNUM, RTNUM); break;
        case 0x38: snprintf(buf, dasm_buf_size, "dsll\tr%d, r%d, %d", RDNUM, RTNUM, SHAMT); break;
        case 0x3A: snprintf(buf, dasm_buf_size, "dsrl\tr%d, r%d, %d", RDNUM, RTNUM, SHAMT); break;
        case 0x3B: snprintf(buf, dasm_buf_size, "dsra\tr%d, r%d, %d", RDNUM, RTNUM, SHAMT); break;
        case 0x3C: snprintf(buf, dasm_buf_size, "dsll32\tr%d, r%d, %d", RDNUM, RTNUM, SHAMT); break;
        case 0x3E: snprintf(buf, dasm_buf_size, "dsrl32\tr%d, r%d, %d", RDNUM, RTNUM, SHAMT); break;
        case 0x3F: snprintf(buf, dasm_buf_size, "dsra32\tr%d, r%d, %d", RDNUM, RTNUM, SHAMT); break;
        default:
            snprintf(buf, dasm_buf_size, "??? [SPECIAL] %08X", opcode);
            break;
        }

        break;

    // REGIMM
    case 0x01:
        switch ((opcode >> 16) & 0x1F) {
        case 0x00: snprintf(buf, dasm_buf_size, "bltz\tr%d, 0x%08X", RSNUM, pc + 4 + (int32_t)((int16_t) (IMM << 2))); break;
        case 0x01: snprintf(buf, dasm_buf_size, "bgez\tr%d, 0x%08X", RSNUM, pc + 4 + (int32_t)((int16_t) (IMM << 2))); break;
        case 0x10: snprintf(buf, dasm_buf_size, "bltzal\tr%d, 0x%08X", RSNUM, pc + 4 + (int32_t)((int16_t) (IMM << 2))); break;
        case 0x11: snprintf(buf, dasm_buf_size, "bgezal\tr%d, 0x%08X", RSNUM, pc + 4 + (int32_t)((int16_t) (IMM << 2))); break;

        default:
            snprintf(buf, dasm_buf_size, "??? [REGIMM] %08X", opcode);
            break;
        }

        break;

    case 0x02: snprintf(buf, dasm_buf_size, "j\t0x%08X", (pc & 0xF0000000) | (opcode & 0x03FFFFFF) << 2); break;
    case 0x03: snprintf(buf, dasm_buf_size, "jal\t0x%08X", (pc & 0xF0000000) | (opcode & 0x03FFFFFF) << 2); break;
    case 0x04: snprintf(buf, dasm_buf_size, "beq\tr%d, r%d, 0x%08X", RSNUM, RTNUM, pc + 4 + (int32_t)((int16_t) (IMM << 2))); break;
    case 0x05: snprintf(buf, dasm_buf_size, "bne\tr%d, r%d, 0x%08X", RSNUM, RTNUM, pc + 4 + (int32_t)((int16_t) (IMM << 2))); break;
    case 0x06: snprintf(buf, dasm_buf_size, "blez\tr%d, r%d, 0x%08X", RSNUM, RTNUM, pc + 4 + (int32_t)((int16_t) (IMM << 2))); break;
    case 0x07: snprintf(buf, dasm_buf_size, "bgtz\tr%d, 0x%08X", RSNUM, pc + 4 + (int32_t)((int16_t) (IMM << 2))); break;
    case 0x08: snprintf(buf, dasm_buf_size, "addi\tr%d, r%d, 0x%04X", RTNUM, RSNUM, SIMM); break;
    case 0x09: snprintf(buf, dasm_buf_size, "addiu\tr%d, r%d, 0x%04X", RTNUM, RSNUM, SIMM); break;
    case 0x0A: snprintf(buf, dasm_buf_size, "slti\tr%d, r%d, 0x%04X", RTNUM, RSNUM, SIMM); break;
    case 0x0B: snprintf(buf, dasm_buf_size, "sltiu\tr%d, r%d, 0x%04X", RTNUM, RSNUM, SIMM); break;
    case 0x0C: snprintf(buf, dasm_buf_size, "andi\tr%d, r%d, 0x%04X", RTNUM, RSNUM, IMM); break;
    case 0x0D: snprintf(buf, dasm_buf_size, "ori\tr%d, r%d, 0x%04X", RTNUM, RSNUM, IMM); break;
    case 0x0E: snprintf(buf, dasm_buf_size, "xori\tr%d, r%d, 0x%04X", RTNUM, RSNUM, IMM); break;
    case 0x0F: snprintf(buf, dasm_buf_size, "lui\tr%d, 0x%04X", RTNUM, IMM); break;
    case 0x10: return string(pcb) + dasm_cop0(opcode, pc);
    case 0x11: return string(pcb) + dasm_cop1(opcode, pc);

    case 0x18: snprintf(buf, dasm_buf_size, "daddi\tr%d, r%d, 0x%04X", RTNUM, RSNUM, SIMM); break;

    case 0x1A: snprintf(buf, dasm_buf_size, "ldl\tr%d, 0x%04X(r%d)", RTNUM, IMM, RSNUM); break;
    case 0x1B: snprintf(buf, dasm_buf_size, "ldr\tr%d, 0x%04X(r%d)", RTNUM, IMM, RSNUM); break;
    case 0x20: snprintf(buf, dasm_buf_size, "lb\tr%d, 0x%04X(r%d)", RTNUM, IMM, RSNUM); break;
    case 0x21: snprintf(buf, dasm_buf_size, "lh\tr%d, 0x%04X(r%d)", RTNUM, IMM, RSNUM); break;
    case 0x22: snprintf(buf, dasm_buf_size, "lwl\tr%d, 0x%04X(r%d)", RTNUM, IMM, RSNUM); break;
    case 0x23: snprintf(buf, dasm_buf_size, "lw\tr%d, 0x%04X(r%d)", RTNUM, IMM, RSNUM); break;
    case 0x24: snprintf(buf, dasm_buf_size, "lbu\tr%d, 0x%04X(r%d)", RTNUM, IMM, RSNUM); break;
    case 0x25: snprintf(buf, dasm_buf_size, "lhu\tr%d, 0x%04X(r%d)", RTNUM, IMM, RSNUM); break;
    case 0x26: snprintf(buf, dasm_buf_size, "lwr\tr%d, 0x%04X(r%d)", RTNUM, IMM, RSNUM); break;
    case 0x27: snprintf(buf, dasm_buf_size, "lwu\tr%d, 0x%04X(r%d)", RTNUM, IMM, RSNUM); break;
    case 0x28: snprintf(buf, dasm_buf_size, "sb\tr%d, 0x%04X(r%d)", RTNUM, IMM, RSNUM); break;
    case 0x29: snprintf(buf, dasm_buf_size, "sh\tr%d, 0x%04X(r%d)", RTNUM, IMM, RSNUM); break;
    case 0x2B: snprintf(buf, dasm_buf_size, "sw\tr%d, 0x%04X(r%d)", RTNUM, IMM, RSNUM); break;
    case 0x2F: snprintf(buf, dasm_buf_size, "cache\tr%d, 0x%04X(r%d)", RTNUM, IMM, RSNUM); break;
    case 0x31: snprintf(buf, dasm_buf_size, "lwc1\tf%d, 0x%04X(r%d)", FTNUM, IMM, RSNUM); break;
    case 0x37: snprintf(buf, dasm_buf_size, "ld\tr%d, 0x%04X(r%d)", RTNUM, IMM, RSNUM); break;
    case 0x39: snprintf(buf, dasm_buf_size, "swc1\tf%d, 0x%04X(r%d)", FTNUM, IMM, RSNUM); break;
    case 0x3F: snprintf(buf, dasm_buf_size, "sd\tr%d, 0x%04X(r%d)", RTNUM, IMM, RSNUM); break;

    default:
        snprintf(buf, dasm_buf_size, "??? %08X - %02X", opcode,  opcode >> 26);
        break;
    }
    return string(pcb) + buf;
}


string mips3::dasm_cop0(uint32_t opcode, uint64_t pc)
{
    char buf[128];
    switch (RSNUM) {
    case 0x00: snprintf(buf, dasm_buf_size, "mfc0\tr%d, %s", RTNUM, cop0_reg_names[RDNUM]); break;
    case 0x04: snprintf(buf, dasm_buf_size, "mtc0\tr%d, %s", RTNUM, cop0_reg_names[RDNUM]); break;
    case 0x10: snprintf(buf, dasm_buf_size, "tlbwi"); break;
    default:
        snprintf(buf, dasm_buf_size, "??? [COP0] %08X\n");
        break;
    }
    return buf;
}

static inline const char * const fmt_string(uint32_t opcode)
{
    if (RSNUM == 16)
        return "s";
    if (RSNUM == 17)
        return "d";
    if (RSNUM == 20)
        return "w";
    if (RSNUM == 21)
        return "l";
    return "#";
}

string mips3::dasm_cop1(uint32_t opcode, uint64_t pc)
{
    char buf[128];
    // MFC1 rt, rd
#if 0

#endif
    switch (RSNUM) {
    case 0x00: snprintf(buf, dasm_buf_size, "mfc1\tr%d, f%d", RTNUM, FSNUM); break;
    case 0x01: snprintf(buf, dasm_buf_size, "dmfc1\tr%d, f%d", RTNUM, FSNUM); break;
    case 0x02: snprintf(buf, dasm_buf_size, "cfc1\tr%d, fcr%d", RTNUM, FSNUM); break;
    case 0x04: snprintf(buf, dasm_buf_size, "mtc1\tr%d, f%d", RTNUM, FSNUM); break;
    case 0x05: snprintf(buf, dasm_buf_size, "dmtc1\tr%d, f%d", RTNUM, FSNUM); break;
    case 0x06: snprintf(buf, dasm_buf_size, "ctc1\tr%d, fcr%d", RTNUM, FSNUM); break;
    case 0x08:
    {
        switch ((opcode >> 16) & 3) {
        case 0x00: snprintf(buf, dasm_buf_size, "bc1f\t0x%08X", pc + 4 + (int32_t)((int16_t) (IMM << 2))); break;
        case 0x01: snprintf(buf, dasm_buf_size, "bc1t\t0x%08X", pc + 4 + (int32_t)((int16_t) (IMM << 2))); break;
        case 0x02: snprintf(buf, dasm_buf_size, "bc1fl\t0x%08X", pc + 4 + (int32_t)((int16_t) (IMM << 2))); break;
        case 0x03: snprintf(buf, dasm_buf_size, "bc1tl\t0x%08X", pc + 4 + (int32_t)((int16_t) (IMM << 2))); break;
        }
        break;
    }
    default:
        switch (opcode & 0x3F) {
        case 0x00: snprintf(buf, dasm_buf_size, "add.%s\tf%d, f%d, f%d", fmt_string(opcode), FDNUM, FSNUM, FTNUM); break;
        case 0x01: snprintf(buf, dasm_buf_size, "sub.%s\tf%d, f%d, f%d", fmt_string(opcode), FDNUM, FSNUM, FTNUM); break;
        case 0x02: snprintf(buf, dasm_buf_size, "mul.%s\tf%d, f%d, f%d", fmt_string(opcode), FDNUM, FSNUM, FTNUM); break;
        case 0x03: snprintf(buf, dasm_buf_size, "div.%s\tf%d, f%d, f%d", fmt_string(opcode), FDNUM, FSNUM, FTNUM); break;
        case 0x04: snprintf(buf, dasm_buf_size, "sqrt.%s\tf%d, f%d", fmt_string(opcode), FDNUM, FSNUM); break;
        case 0x05: snprintf(buf, dasm_buf_size, "abs.%s\tf%d, f%d", fmt_string(opcode), FDNUM, FSNUM); break;
        case 0x06: snprintf(buf, dasm_buf_size, "mov.%s\tf%d, f%d", fmt_string(opcode), FDNUM, FSNUM); break;
        case 0x07: snprintf(buf, dasm_buf_size, "neg.%s\tf%d, f%d", fmt_string(opcode), FDNUM, FSNUM); break;
        case 0x20: snprintf(buf, dasm_buf_size, "cvt.s.%s\tf%d, f%d", fmt_string(opcode), FDNUM, FSNUM); break;
        case 0x24: snprintf(buf, dasm_buf_size, "cvt.w.%s\tf%d, f%d", fmt_string(opcode), FDNUM, FSNUM); break;
        case 0x34: snprintf(buf, dasm_buf_size, "c.olt.%s\tf%d, f%d", fmt_string(opcode), FSNUM, FTNUM); break;
        default:
            snprintf(buf, dasm_buf_size, "??? [COP1] %08X");
            break;
        }

        break;
    }

    return buf;
}


}
