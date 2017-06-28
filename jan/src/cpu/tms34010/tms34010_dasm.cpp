/*
 * Copyright (c) 2015, Marcos Medeiros
 * Licensed under BSD 3-clause.
 */
#include <string>
#include <vector>
#include "tms34010.h"
#include "memhandler.h"

#define READ_WORD(addr) tms::read_word(addr)
#define READ_DWORD(addr) (READ_WORD(addr) | (READ_WORD(addr + 16) << 16))

#define BKW_DIR (opcode & (1 << 10))
#define OFFS    ((opcode >> 5) & 0x1F)
#define K       fw_lut[((opcode >> 5) & 0x1F)]
#define KN      ((~opcode >> 5) & 0x1F)
#define K2C     ((-K) & 0x1F)
#define M_BIT   (opcode & (1 << 9))
#define R_BIT   (opcode & 0x10)
#define RS      (((opcode >> 5) & 0xF) | R_BIT)
#define RD      ((opcode & 0xF) | R_BIT)
#define NF      (opcode & 0x1F)
#define RS_n    ((opcode >> 5) & 0xF)
#define RD_n    (opcode & 0xF)

namespace tms
{

const int dasm_buf_size = 128;
const int dasm_pc_size = 32;

const char *reg_names[32] =
{
    "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7",
    "a8", "a9", "a10", "a11", "a12", "a13", "a14", "sp",
    "b0", "b1", "b2", "b3", "b4", "b5", "b6", "b7",
    "b8", "b9", "b10", "b11", "b12", "b13", "b14", "sp",
};

static inline dword dsjs_pc(dword pc, word opcode)
{
    int displace = OFFS * 16;
    if (BKW_DIR)
        displace = -displace;
    return pc + displace;
}

static inline dword jr_pc(dword pc, word opcode)
{
    int displace = (opcode & 0xFF) * 16;
    if (BKW_DIR)
        displace = -displace;
    return pc + displace;
}

static inline const char *jr_cond(word opcode)
{
    const char *cond_table[16] = {
        /* 0000 */ "",
        /* 0001 */ "p",
        /* 0010 */ "ls",
        /* 0011 */ "hi",
        /* 0100 */ "lt",
        /* 0101 */ "ge",
        /* 0110 */ "le",
        /* 0111 */ "gt",
        /* 1000 */ "c",
        /* 1001 */ "nc",
        /* 1010 */ "eq",
        /* 1011 */ "ne",
        /* 1100 */ "v",
        /* 1101 */ "nv",
        /* 1110 */ "n",
        /* 1111 */ "nn"
    };
    return cond_table[(opcode >> 8) & 0xF];
}


static const char *reg_list(word opcode, word data)
{
    int inc = (opcode & 0x10) ? 16 : 0;
    std::vector<std::string> regs;
    regs.reserve(16);


    for (int i = 0; i < 16; i++) {
        if (data & 0x8000)
            regs.push_back(reg_names[inc + i]);
        data <<= 1;
    }


    std::string s;
    s.reserve(100);
    for (int i = 0; i < regs.size(); i++) {
        s.append(regs[i]);
        if (i != regs.size() - 1)
            s.append(",");
    }

    return s.c_str();
}

static const char *reg_list_inv(word opcode, word data)
{
    int inc = (opcode & 0x10) ? 16 : 0;
    std::vector<std::string> regs;
    regs.reserve(16);


    for (int i = 0; i < 16; i++) {
        if (data & 1)
            regs.push_back(reg_names[inc + i]);
        data >>= 1;
    }


    std::string s;
    s.reserve(100);
    for (int i = 0; i < regs.size(); i++) {
        s.append(regs[i]);
        if (i != regs.size() - 1)
            s.append(",");
    }

    return s.c_str();
}

#define DASM(...)    snprintf(buf, dasm_buf_size, __VA_ARGS__)

std::string dasm(dword pc, size_t *szbits)
{
    const int size = 128;
    char buf[size];
    char pcb[dasm_pc_size];

    word opcode = READ_WORD(pc);

    buf[0] = '\0';
    pcb[0] = '\0';

    snprintf(pcb, dasm_pc_size, "%08X\t%04X\t", pc, opcode);
    int fs = opcode & 0x1F;
    int fe = (opcode & (1 << 5)) ? 1 : 0;
    int f = (opcode & (1 << 9)) ? 1 : 0;
    int rd = RD;
    int rs = RS;
    //int rfile = (opcode >> 5) & 1;

    int words = 1;

    if (!fs)
        fs = 32;

    switch (opcode >> 12) {
    case 0x00:
        switch ((opcode >> 5) & 0x7F) {

        case 0x09: DASM("exgpc %s", reg_names[rd]); break;
        case 0x0A: DASM("getpc %s", reg_names[rd]); break;
        case 0x0B: DASM("jump %s", reg_names[rd]); break;
        case 0x0F: DASM("pushst"); break;
        case 0x0E: DASM("popst"); break;
        case 0x1B: DASM("dint"); break;
        case 0x6B: DASM("eint"); break;

        case 0x1F: DASM("not %s", reg_names[rd]); break;

        case 0x2F: DASM("movb %s, @%x", reg_names[rd], READ_DWORD(pc + 16)); words += 2; break;
        case 0x3F: DASM("movb @%x, %s", READ_DWORD(pc + 16), reg_names[rd]); words += 2; break;

        case 0x2A:
        case 0x2B:
        case 0x3A:
        case 0x3B: DASM("setf 0x%x, %d, %d", fs, fe, f); break;

        case 0x4D: DASM("mmfm %s, %s", reg_names[rd], reg_list_inv(opcode, READ_WORD(pc + 16))); words++; break;

        case 0x4E: DASM("movi 0x%04x, %s", READ_WORD(pc + 16), reg_names[rd]); words++; break;
        case 0x4F: DASM("movi 0x%08x, %s", READ_DWORD(pc + 16), reg_names[rd]); words += 2; break;

        case 0x3C:
        case 0x2C: DASM("move %s, @%x, %d", reg_names[rd], READ_DWORD(pc + 16), f); words += 2; break;

        case 0x3D:
        case 0x2D: DASM("move @%x, %s, %d", READ_DWORD(pc + 16), reg_names[rd], f); words += 2; break;

        case 0x4B: DASM("rets 0x%x", NF); break;
        case 0x4C: DASM("mmtm %s, %s", reg_names[rd], reg_list(opcode, READ_WORD(pc + 16))); words++; break;

        case 0x58: DASM("addi 0x%08x, %s", READ_WORD(pc + 16), reg_names[rd]); words++; break;
        case 0x59: DASM("addi 0x%08x, %s", READ_DWORD(pc + 16), reg_names[rd]); words += 2; break;
        case 0x5C: DASM("andi 0x%08x, %s", ~READ_DWORD(pc + 16), reg_names[rd]); words += 2; break;

        case 0x5F: DASM("subi 0x%08x, %s", ~(short)READ_WORD(pc + 16), reg_names[rd]); words++; break;
        case 0x68: DASM("subi 0x%08x, %s", ~READ_DWORD(pc + 16), reg_names[rd]); words += 2; break;

        case 0x5A: DASM("cmpi 0x%08x, %s", ~(short)READ_WORD(pc + 16), reg_names[rd]); words++; break;
        case 0x5B: DASM("cmpi 0x%08x, %s", ~READ_DWORD(pc + 16), reg_names[rd]); words += 2; break;

        case 0x69: {
            switch (opcode & 0xFF) {
            case 0x3F:
                DASM("callr @%x", pc + 32 + (short)READ_WORD(pc + 16) * 16); words++; break;
            }
        }
        case 0x6A: {
            switch (opcode & 0xFF) {
            case 0x5F:
                DASM("calla @%x", READ_DWORD(pc + 16)); words += 2; break;
            }
            break;
        }

        case 0x6C: DASM("dsj %s, @%x", reg_names[rd], pc + 32 + (short)READ_WORD(pc + 16) * 16); words++; break;
        case 0x7D: DASM("pixblt b, xy"); break;


        default:
            DASM("[%04X] ??? [0000]", opcode);
            break;
        }
        break;

    case 0x01:
        switch ((opcode >> 10) & 3) {
        case 0x03: DASM("btst 0x%x, %s", KN, reg_names[rd]); break;
        case 0x02: DASM("movk 0x%x, %s", K, reg_names[rd]); break;
        case 0x01: DASM("subk 0x%x, %s", K, reg_names[rd]); break;
        case 0x00: DASM("addk 0x%x, %s", K, reg_names[rd]); break;
        default:
            DASM("[%04X] ??? [0001]", opcode);
            break;
        }
        break;

    case 0x02:
        switch ((opcode >> 10) & 3) {
        case 0x01: DASM("sll 0x%x, %s", K, reg_names[rd]); break;
        case 0x03: DASM("srl 0x%x, %s", K2C, reg_names[rd]); break;
        default:
            DASM("[%04X] ??? [0010]", opcode);
            break;
        }
        break;

    case 0x03:
        if (opcode & (1 << 11))
            DASM("dsjs %s, @%x", reg_names[rd], dsjs_pc(pc + 16, opcode));
        else {
            DASM("[%04X] ??? [0011]", opcode);
        }
        break;
    case 0x04:
        switch ((opcode >> 9) & 7) {
        case 0x0: DASM("add %s, %s", reg_names[rs], reg_names[rd]); break;
        case 0x2: DASM("sub %s, %s", reg_names[rs], reg_names[rd]); break;
        case 0x4: DASM("cmp %s, %s", reg_names[rs], reg_names[rd]); break;
        case 0x6: DASM("move %s, %s", reg_names[rs], reg_names[rd]); break;
        case 0x7: {
            int rs_, rd_;
            if (R_BIT) {
                rs_ = 16 + RS_n;
                rd_ = RD_n;
            } else {
                rs_ = RS_n;
                rd_ = 16 + RD_n;
            }
            DASM("move %s, %s", reg_names[rs_], reg_names[rd_]);
            break;
        }

        default:
            DASM("[%04X] ??? [0100]", opcode);
            break;
        }

        break;

    case 0x05:
        switch ((opcode >> 9) & 7) {
        case 0x00: DASM("and %s, %s", reg_names[rs], reg_names[rd]); break;
        case 0x01: DASM("andn %s, %s", reg_names[rs], reg_names[rd]); break;
        case 0x02: DASM("or %s, %s", reg_names[rs], reg_names[rd]); break;
        case 0x03:
            if (rs == rd)
                DASM("clr %s", reg_names[rs]);
            else
                DASM("xor %s, %s", reg_names[rs], reg_names[rd]);
            break;
        case 0x04: DASM("divs %s, %s", reg_names[rs], reg_names[rd]); break;
        case 0x05: DASM("divu %s, %s", reg_names[rs], reg_names[rd]); break;
        case 0x06: DASM("mpys %s, %s", reg_names[rs], reg_names[rd]); break;
        case 0x07: DASM("mpyu %s, %s", reg_names[rs], reg_names[rd]); break;
        default:
            DASM("[%04X] ??? [0101]", opcode);
        }

        break;

    case 0x06:
        switch ((opcode >> 9) & 7) {
        case 0x05: DASM("lmo %s, %s", reg_names[rs], reg_names[rd]); break;
        case 0x07: DASM("modu %s, %s", reg_names[rs], reg_names[rd]); break;
        default:
            DASM("[%04X] ??? [0110]", opcode);
            break;
        }
        break;


    case 0x08:
        switch ((opcode >> 9) & 7) {
        case 0x7: DASM("movb *%s, %s", reg_names[rs], reg_names[rd]); break;
        case 0x6: DASM("movb %s, *%s", reg_names[rs], reg_names[rd]); break;
        case 0x2: DASM("move *%s, %s, 0", reg_names[rs], reg_names[rd]); break;
        case 0x3: DASM("move *%s, %s, 1", reg_names[rs], reg_names[rd]); break;
        case 0x0: DASM("move %s, *%s, 0", reg_names[rs], reg_names[rd]); break;
        case 0x1: DASM("move %s, *%s, 1", reg_names[rs], reg_names[rd]); break;

        default:
            DASM("[%04X] ??? [1000]", opcode);
            break;
        }

        break;



    case 0x09:
        switch ((opcode >> 9) & 7) {
        case 0x00: DASM("move %s, *%s+, 0", reg_names[rs], reg_names[rd]); break;
        case 0x01: DASM("move %s, *%s+, 1", reg_names[rs], reg_names[rd]); break;
        case 0x02: DASM("move *%s+, %s, 0", reg_names[rs], reg_names[rd]); break;
        case 0x03: DASM("move *%s+, %s, 1", reg_names[rs], reg_names[rd]); break;
        case 0x04: DASM("move *%s+, *%s+, 0", reg_names[rs], reg_names[rd]); break;
        case 0x05: DASM("move *%s+, *%s+, 1", reg_names[rs], reg_names[rd]); break;
        default:
            DASM("[%04X] ??? [1001]", opcode);
            break;
        }
        break;

    case 0x0A:
        switch ((opcode >> 9) & 7) {
        case 0x00: DASM("move %s, -*%s, 0", reg_names[rs], reg_names[rd]); break;
        case 0x01: DASM("move %s, -*%s, 1", reg_names[rs], reg_names[rd]); break;
        case 0x02: DASM("move -%s, *%s, 0", reg_names[rs], reg_names[rd]); break;
        case 0x03: DASM("move -%s, *%s, 1", reg_names[rs], reg_names[rd]); break;
        case 0x04: DASM("move -%s, -*%s, 0", reg_names[rs], reg_names[rd]); break;
        case 0x05: DASM("move -%s, -*%s, 1", reg_names[rs], reg_names[rd]); break;
        case 0x06: DASM("movb %s, *%s(%x), 1", reg_names[rs], reg_names[rd],READ_WORD(pc+16)); words++; break;
        case 0x07: DASM("movb %s(%x), *%s, 1", reg_names[rs], READ_WORD(pc+16),reg_names[rd]); words++; break;
        default:
            DASM("[%04X] ??? [1010]", opcode);
            break;
        }
        break;

    case 0x0B:
        switch ((opcode >> 9) & 7) {
        case 0x00: DASM("move %s, *%s(%x), 0", reg_names[rs], reg_names[rd],READ_WORD(pc+16)); words++; break;
        case 0x01: DASM("move %s, *%s(%x), 1", reg_names[rs], reg_names[rd],READ_WORD(pc+16)); words++; break;
        case 0x02: DASM("move *%s(%x), %s, 0", reg_names[rs], READ_WORD(pc+16), reg_names[rd]); words++; break;
        case 0x03: DASM("move *%s(%x), %s, 1", reg_names[rs], READ_WORD(pc+16), reg_names[rd]); words++; break;
        case 0x04: DASM("move *%s(%x), *%s(%x), 0", reg_names[rs], READ_WORD(pc+16),reg_names[rd],READ_WORD(pc+32)); words+=2; break;
        case 0x05: DASM("move *%s(%x), *%s(%x), 1", reg_names[rs], READ_WORD(pc+16),reg_names[rd],READ_WORD(pc+32)); words+=2; break;
        default:
            DASM("[%04X] ??? [1011]", opcode);
            break;
        }
        break;


    case 0x0C:
        if (opcode & 0xFF) {
            if ((opcode & 0xFF) == 0x80) { // JA addr
                DASM("ja%s @%x", jr_cond(opcode), READ_DWORD(pc + 16));
                words += 2;
                break;
            }
            // JR shor addr
            DASM("jr%s @%x", jr_cond(opcode), pc + 16 + ((int8_t)(opcode & 0xFF)) * 16);
        } else {
            // JR rel cond
            DASM("jr%s @%x", jr_cond(opcode), pc + 32 + (short)READ_WORD(pc + 16) * 16);
            words++;
        }
        break;


    case 0x0E:
        switch ((opcode >> 9) & 7) {
        case 0x6: DASM("movx %s, %s", reg_names[rs], reg_names[rd]); break;
        case 0x7: DASM("movy %s, %s", reg_names[rs], reg_names[rd]); break;
        default:
            DASM("[%04X] ??? [1110]", opcode);
            break;
        }

        break;

    default:
        DASM("[%04X] ???", opcode);
        break;
    }

    if (szbits)
        *szbits = words * 16;

    return std::string(buf);
}

}

