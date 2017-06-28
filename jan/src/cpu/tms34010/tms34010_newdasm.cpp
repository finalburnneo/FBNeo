/*
 * Copyright (c) 2015, Marcos Medeiros
 * Licensed under BSD 3-clause.
 */
#include <string>
#include <vector>
#include "tms34010.h"
#include "memhandler.h"

namespace tms
{

#define READ_WORD(addr) tms::read_word(addr)
#define READ_DWORD(addr) (READ_WORD(addr) | (READ_WORD(addr + 16) << 16))

const int dasm_buf_size = 128;
const int dasm_pc_size = 32;

static const char *reg_names[32] =
{
    "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7",
    "a8", "a9", "a10", "a11", "a12", "a13", "a14", "sp",
    "b0", "b1", "b2", "b3", "b4", "b5", "b6", "b7",
    "b8", "b9", "b10", "b11", "b12", "b13", "b14", "sp",
};

#define BKW_DIR (st.opcode & (1 << 10))
#define OFFS    ((st.opcode >> 5) & 0x1F)
#define K       fw_lut[((st.opcode >> 5) & 0x1F)]
#define FS		fw_lut[st.opcode & 0x1F]
#define KN      ((~st.opcode >> 5) & 0x1F)
#define K2C     ((-K) & 0x1F)
#define M_BIT   (st.opcode & (1 << 9))
#define R_BIT   (st.opcode & 0x10)
#define RS_     (((st.opcode >> 5) & 0xF) | R_BIT)
#define RD_     ((st.opcode & 0xF) | R_BIT)
#define NF      (st.opcode & 0x1F)
#define RS_n    ((st.opcode >> 5) & 0xF)
#define RD_n    (st.opcode & 0xF)

#define RS	reg_names[RS_]
#define RD	reg_names[RD_]
#define IW	st.iw
#define IW2	st.iw2
#define IL	st.il
#define IL2	st.il2
#define W	st.words
#define OP  st.opcode

#define WDISP   (st.pc + ((short)(IW)) * 16) + 32
#define BDISP   (st.pc + ((char)(OP & 0xFF) * 16)) + 16

#define DASM(...)	snprintf(st.buf, dasm_buf_size, __VA_ARGS__)

struct dasm_state {
    dword pc;
    word opcode;
    char buf[dasm_buf_size];
    int words;
    word iw;
    word iw2;
    dword il;
    dword il2;
    dasm_state() {
        buf[0] = '\0';
    }
};


static std::string reg_list(word opcode, word data)
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
    for (size_t i = 0; i < regs.size(); i++) {
        s.append(regs[i]);
        if (i != regs.size() - 1)
            s.append(",");
    }

    return s;
}

static std::string reg_list_inv(word opcode, word data)
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

    return s;
}

static void dasm_prefix_0000(dasm_state &st)
{
    switch ((OP >> 8) & 0xF) {
        case 0x0:
            if (((OP >> 5) & 7) == 1)
                DASM("rev %s", RD);
            else
                DASM("invalid opcode [%04Xh] 0000", OP);
            break;

        case 0x1:
            switch ((OP >> 5) & 7) {
                case 0: DASM("emu"); break;
                case 1: DASM("exgpc %s", RD); break;
                case 2: DASM("getpc %s", RD); break;
                case 3: DASM("jump %s", RD); break;
                case 4: DASM("getst %s", RD); break;
                case 5: DASM("putst %s", RD); break;
                case 6: DASM("popst"); break;
                case 7: DASM("pushst"); break;
            }
            break;

        case 0x2:
            DASM("invalid opcode [%04Xh] 0000 010X", OP);
            break;

        case 0x3:
            switch ((OP >> 5) & 7) {
                case 0: DASM("nop"); break;
                case 1: DASM("clrc"); break;
                case 2: DASM("movb @%08Xh, @%08x", IL, IL2); W+=4; break;
                case 3: DASM("dint"); break;
                case 4: DASM("abs %s", RD); break;
                case 5: DASM("neg %s", RD); break;
                case 6: DASM("negb %s", RD); break;
                case 7: DASM("not %s", RD); break;
            }
            break;

        case 0x4:
            DASM("invalid opcode [%04Xh] 0000 0100", OP);
            break;

        case 0x5:
            switch ((OP >> 5) & 7) {
                case 0: DASM("sext %s, 0", RD); break;
                case 1: DASM("zext %s, 0", RD); break;
                case 2: DASM("setf %d, 0, 0", FS); break;
                case 3: DASM("setf %d, 1, 0", FS); break;
                case 4: DASM("move %s, @%08Xh, 0", RD, IL); W+=2; break;
                case 5: DASM("move @%08Xh, %s, 0", IL, RD); W+=2; break;
                case 6: DASM("move @%08Xh, @%08Xh, 0", IL, IL2); W+=4;break;
                case 7: DASM("movb %s, @%08Xh", RD, IL); W+=2; break;
            }
            break;

        case 0x6:
            DASM("invalid opcode [%04Xh]", OP);
            break;

        case 0x7:
            switch ((OP >> 5) & 7) {
                case 0: DASM("sext %s, 1", RD); break;
                case 1: DASM("zext %s, 1", RD); break;
                case 2: DASM("setf %d, 0, 1", FS); break;
                case 3: DASM("setf %d, 1, 1", FS); break;
                case 4: DASM("move %s, @%08Xh, 1", RD, IL); W+=2; break;
                case 5: DASM("move @%08Xh, %s, 1", IL, RD); W+=2; break;
                case 6: DASM("move @%08Xh, @%08Xh, 1", IL, IL2); W+=4;break;
                case 7: DASM("movb @%08Xh, %s", IL, RD); W+=2; break;
            }
            break;

        case 0x8:
            DASM("invalid opcode [%04Xh]", OP);
            break;

        case 0x9:
            switch ((OP >> 5) & 7) {
                case 0: DASM("trap %Xh", OP & 0x1F); break;
                case 1: DASM("call %s", RD); break;
                case 2: DASM("reti"); break;
                case 3: DASM("rets 0x%d", OP & 0x1F); break;
                case 4: DASM("mmtm %s, %s", RD, reg_list(OP,IW).c_str()); W++; break;
                case 5: DASM("mmfm %s, %s", RD, reg_list_inv(OP,IW).c_str()); W++; break;
                case 6: DASM("movi %04Xh, %s", IW, RD); W++; break;
                case 7: DASM("movi %08Xh, %s", IL, RD); W+=2; break;
            }
            break;

        case 0xA:
            DASM("invalid opcode [%04Xh]", OP);
            break;

        case 0xB:
            switch ((OP >> 5) & 7) {
                case 0: DASM("addi %04Xh, %s", IW, RD); W++; break;
                case 1: DASM("addi %08Xh, %s", IL, RD); W+=2; break;
                case 2: DASM("cmpi %04Xh, %s", ~IW, RD); W++; break;
                case 3: DASM("cmpi %08Xh, %s", ~IL, RD); W+=2; break;
                case 4: DASM("andi %08Xh, %s", ~IL, RD); W+=2; break;
                case 5: DASM("ori %08Xh, %s", IL, RD); W+=2; break;
                case 6: DASM("xori %08Xh, %s", IL, RD); W+=2; break;
                case 7: DASM("subi %08Xh, %s", ~IL, RD); W+=2; break;
            }
            break;

        case 0xC:
            DASM("invalid opcode [%04Xh]", OP);
            break;

        case 0xD:
            switch ((OP >> 4) & 15) {
                case 0x3: DASM("callr %08Xh", WDISP); W++; break;
                case 0x5: DASM("calla %08Xh", IL); W+=2; break;
                case 0x6: DASM("eint"); break;
                case 0x8:
                case 0x9: DASM("dsj %s, %08x", RD, WDISP); W++; break;
                case 0xA:
                case 0xB: DASM("dsjeq %s, %08x", RD, WDISP); W++; break;
                case 0xC:
                case 0xD: DASM("dsjne %s, %08x", RD, WDISP); W++; break;
                case 0xE: DASM("setc"); break;
                default:
                    DASM("invalid opcode [%04Xh] 0000 1101", OP);
                    break;
            }
            break;
        case 0xE:
            DASM("invalid opcode [%04Xh]", OP);
            break;

        case 0xF:
            switch ((OP >> 4) & 0xF) {
                case 0x0: DASM("pixblt l, l"); break;
                case 0x2: DASM("pixblt l, xy"); break;
                case 0x4: DASM("pixblt xy, l"); break;
                case 0x6: DASM("pixblt xy, xy"); break;
                case 0x8: DASM("pixblt b, l"); break;
                case 0xA: DASM("pixblt b, xy"); break;
                case 0xC: DASM("fill l"); break;
                case 0xE: DASM("fill xy"); break;
                default:
                    DASM("invalid opcode [%04Xh] 0000 1111", OP);
                    break;
            }
            break;
    }
}

static void dasm_prefix_0001(dasm_state &st)
{
    switch ((OP >> 10) & 3) {
        case 0:	if (K==1) DASM("inc %s", RD); else DASM("addk %d, %s", K, RD); break;
        case 1:	if (K==1) DASM("dec %s", RD); else DASM("subk %d, %s", K, RD); break;
        case 2:	DASM("movk %d, %s", K, RD); break;
        case 3:	DASM("btst %d, %s", K, RD); break;
    }
}

static void dasm_prefix_0010(dasm_state &st)
{
    switch ((OP >> 10) & 3) {
        case 0:	DASM("sla %d, %s", K, RD); break;
        case 1:	DASM("sll %d, %s", K, RD); break;
        case 2:	DASM("sra %d, %s", K, RD); break;
        case 3:	DASM("srl %d, %s", K, RD); break;
    }
}

static void dasm_prefix_0011(dasm_state &st)
{
    switch ((OP >> 10) & 3) {
        case 0: DASM("rl %d, %s", K, RD); break;
        case 1: DASM("invalid opcode [%04Xh] 0011", OP); break;
        case 2:
        case 3: {
            int displace = OFFS * 16;
            if (BKW_DIR)
                displace = -displace;
            DASM("dsjs %s, %08Xh", RD, st.pc + 16 + displace);
            break;
        }
    }
}

static void dasm_prefix_0100(dasm_state &st)
{
    switch ((OP >> 9) & 7) {
        case 0:	DASM("add %s, %s", RS, RD); break;
        case 1:	DASM("addc %s, %s", RS, RD); break;
        case 2:	DASM("sub %s, %s", RS, RD); break;
        case 3:	DASM("subb %s, %s", RS, RD); break;
        case 4:	DASM("cmp %s, %s", RS, RD); break;
        case 5:	DASM("btst %s, %s", RS, RD); break;
        case 6:	DASM("move %s, %s", RS, RD); break;
        case 7:	{
            DASM("move %s, %s", RS, R_BIT ? reg_names[RD_n] : reg_names[RD_n | 0x10]);
            break;
        }
    }
}

static void dasm_prefix_0101(dasm_state &st)
{
    switch ((OP >> 9) & 7) {
        case 0:	DASM("and %s, %s", RS, RD); break;
        case 1:	DASM("andn %s, %s", RS, RD); break;
        case 2:	DASM("or %s, %s", RS, RD); break;
        case 3:	if (RS_==RD_) DASM("clr %s", RD); else DASM("xor %s, %s", RS, RD); break;
        case 4:	DASM("divs %s, %s", RS, RD); break;
        case 5:	DASM("divu %s, %s", RS, RD); break;
        case 6:	DASM("mpys %s, %s", RS, RD); break;
        case 7:	DASM("mpyu %s, %s", RS, RD); break;
    }
}

static void dasm_prefix_0110(dasm_state &st)
{
    switch ((OP >> 9) & 7) {
        case 0:	DASM("sla %s, %s", RS, RD); break;
        case 1:	DASM("sll %s, %s", RS, RD); break;
        case 2:	DASM("sra %s, %s", RS, RD); break;
        case 3:	DASM("srl %s, %s", RS, RD); break;
        case 4:	DASM("rl %s, %s", RS, RD); break;
        case 5:	DASM("lmo %s, %s", RS, RD); break;
        case 6:	DASM("mods %s, %s", RS, RD); break;
        case 7:	DASM("modu %s, %s", RS, RD); break;
    }
}

static void dasm_prefix_0111(dasm_state &st)
{
    DASM("invalid opcode [%04Xh] 0111", OP);
}

static void dasm_prefix_1000(dasm_state &st)
{
    switch ((OP >> 9) & 7) {
        case 0:	DASM("move %s, *%s, 0", RS, RD); break;
        case 1:	DASM("move %s, *%s, 1", RS, RD); break;
        case 2:	DASM("move *%s, %s, 0", RS, RD); break;
        case 3:	DASM("move *%s, %s, 1", RS, RD); break;
        case 4:	DASM("move *%s, *%s, 0", RS, RD); break;
        case 5:	DASM("move *%s, *%s, 1", RS, RD); break;
        case 6:	DASM("move %s, *%s", RS, RD); break;
        case 7:	DASM("move *%s, %s", RS, RD); break;
    }
}

static void dasm_prefix_1001(dasm_state &st)
{
    switch ((OP >> 9) & 7) {
        case 0: DASM("move %s, *%s+, 0", RS, RD); break;
        case 1: DASM("move %s, *%s+, 1", RS, RD); break;
        case 2: DASM("move *%s+, %s, 0", RS, RD); break;
        case 3: DASM("move *%s+, %s, 1", RS, RD); break;
        case 4: DASM("move *%s+, *%s+, 0", RS, RD); break;
        case 5: DASM("move *%s+, *%s+, 1", RS, RD); break;
        case 6: DASM("movb *%s, *%s", RS, RD); break;
        case 7: DASM("invalid opcode [%04Xh] 1001", OP); break;
    }
}

static void dasm_prefix_1010(dasm_state &st)
{
    switch ((OP >> 9) & 7) {
        case 0: DASM("move %s, -*%s, 0", RS, RD); break;
        case 1: DASM("move %s, -*%s, 1", RS, RD);  break;
        case 2: DASM("move -*%s, %s, 0", RS, RD);  break;
        case 3: DASM("move -*%s, %s, 1", RS, RD); break;
        case 4: DASM("move -*%s, -*%s, 0", RS, RD); break;
        case 5: DASM("move -*%s, -*%s, 1", RS, RD); break;
        case 6: DASM("move %s, *%s(%Xh)", RS, RD, IW); W++; break;
        case 7: DASM("move *%s(%Xh), %s", RS, IW, RD); W++; break;
    }
}

static void dasm_prefix_1011(dasm_state &st)
{
    switch ((OP >> 9) & 7) {
        case 0: DASM("move %s, *%s(%Xh), 0", RS, RD, IW); W++; break;
        case 1: DASM("move %s, *%s(%Xh), 1", RS, RD, IW); W++; break;
        case 2: DASM("move *%s(%Xh), %s, 0", RS, IW, RD); W++; break;
        case 3: DASM("move *%s(%Xh), %s, 1", RS, IW, RD); W++; break;
        case 4: DASM("move *%s(%Xh), *%s(%Xh), 0", RS, IW, RD, IW2); W+=2; break;
        case 5: DASM("move *%s(%Xh), *%s(%Xh), 1", RS, IW, RD, IW2); W+=2; break;
        case 6: DASM("movb *%s(%Xh), *%s(%Xh)", RS, IW, RD, IW2); W+=2; break;
        case 7: DASM("invalid opcode [%08Xh] 1011", OP); break;
    }
}

static void dasm_prefix_1100(dasm_state &st)
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
    unsigned cc = (OP >> 8) & 0xF;
    if ((OP & 0xFF) == 0x0) {
        DASM("jr%s %08x (IW)", cond_table[cc], WDISP);
        W++;
    } else if ((OP & 0xFF) == 0x80) {
        DASM("ja%s %08x", cond_table[cc], IL);
        W+=2;
    } else {
        DASM("jr%s %08x", cond_table[cc], BDISP);
    }
}

static void dasm_prefix_1101(dasm_state &st)
{
    switch ((OP >> 8) & 0xF) {
        case 0x0:
        case 0x1: DASM("move *%s(%0x04x), %s, 0", RS, IW, RD); W++; break;
        case 0x2:
        case 0x3: DASM("move *%s(%0x04x), %s, 1", RS, IW, RD); W++; break;
        case 0x4: DASM("move @%08Xh, %s, 0", IL, RD); W+=2; break;
        case 0x5: DASM("exgf %s, 0", RD); break;
        case 0x6: DASM("move @%08Xh, %s, 1", IL, RD); W+=2; break;
        case 0x7: DASM("exgf %s, 1", RD); break;
        case 0xF:
            if (((OP >> 4) & 7) == 1) {
                DASM("line %d", (OP >> 7) & 1);
                break;
            }
        default:
            DASM("invalid opcode [%04Xh] 1101", OP);
            break;
    }
}

static void dasm_prefix_1110(dasm_state &st)
{
    switch ((OP >> 9) & 7) {
        case 0: DASM("addxy %s, %s", RS, RD); break;
        case 1: DASM("subxy %s, %s", RS, RD); break;
        case 2: DASM("cmpxy %s, %s", RS, RD); break;
        case 3: DASM("cpw %s, %s", RS, RD); break;
        case 4: DASM("cvxyl %s, %s", RS, RD); break;
        case 5: DASM("invalid opcode [%04Xh] 1110", OP); break;
        case 6: DASM("movx %s, %s", RS, RD); break;
        case 7: DASM("movy %s, %s", RS, RD); break;
    }
}


static void dasm_prefix_1111(dasm_state &st)
{
    switch ((st.opcode >> 9) & 7) {
        case 0: DASM("pixt %s, *%s.xy", RS, RD); break;
        case 1: DASM("pixt *%s.xy, %s", RS, RD); break;
        case 2:	DASM("pixt *%s.xy, *%s.xy", RS, RD); break;
        case 3: DASM("drav %s, %s", RS, RD); break;
        case 4: DASM("pixt %s, *%s", RS, RD); break;
        case 5: DASM("pixt *%s, %s", RS, RD); break;
        case 6: DASM("pixt *%s, *%s", RS, RD); break;
        case 7: DASM("invalid opcode [%04Xh] 1111", OP); break;
    }
}

std::string new_dasm(dword pc, size_t *size)
{
    dasm_state st;

    st.pc = pc & ~0xF;
    st.opcode = READ_WORD(pc);
    st.iw = READ_WORD(pc + 16);
    st.iw2 = READ_WORD(pc + 32);
    st.il = READ_DWORD(pc + 16);
    st.il2 = READ_DWORD(pc + 16 + 32);
    st.words = 1;

    switch ((OP >> 12) & 0xF) {
        case 0x0: dasm_prefix_0000(st); break;
        case 0x1: dasm_prefix_0001(st); break;
        case 0x2: dasm_prefix_0010(st); break;
        case 0x3: dasm_prefix_0011(st); break;
        case 0x4: dasm_prefix_0100(st); break;
        case 0x5: dasm_prefix_0101(st); break;
        case 0x6: dasm_prefix_0110(st); break;
        case 0x7: dasm_prefix_0111(st); break;
        case 0x8: dasm_prefix_1000(st); break;
        case 0x9: dasm_prefix_1001(st); break;
        case 0xA: dasm_prefix_1010(st); break;
        case 0xB: dasm_prefix_1011(st); break;
        case 0xC: dasm_prefix_1100(st); break;
        case 0xD: dasm_prefix_1101(st); break;
        case 0xE: dasm_prefix_1110(st); break;
        case 0xF: dasm_prefix_1111(st); break;
    }

    if (size)
        *size = st.words * 16;

    return std::string(st.buf);
}

}
