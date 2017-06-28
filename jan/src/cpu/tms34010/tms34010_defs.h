/*
 * Copyright (c) 2015, Marcos Medeiros
 * Licensed under BSD 3-clause.
 */
#ifndef TMS34010_DEFS_H
#define TMS34010_DEFS_H

#define R_BIT   (opcode & 0x10)
#define RS      (((opcode >> 5) & 0xF) | R_BIT)
#define RD      ((opcode & 0xF) | R_BIT)
#define RS_n    ((opcode >> 5) & 0xF)
#define RD_n    (opcode & 0xF)
#define K       ((opcode >> 5) & 0x1F)
#define KN      ((~opcode >> 5) & 0x1F)

#define R(i)    cpu->r[(i)|R_BIT]->value

#define OFFS    ((opcode >> 5) & 0x1F)
#define BKW_DIR (opcode & (1 << 10))

#define _rs     cpu->r[RS]->value
#define _rd     cpu->r[RD]->value
#define _rd_0   cpu->r[RD+0]->value
#define _rd_1   cpu->r[RD+1]->value
#define _pc     cpu->pc
#define _sp     cpu->sp.value
#define _st     cpu->st

#define _rsx     cpu->r[RS]->x
#define _rdx     cpu->r[RD]->x
#define _rsy     cpu->r[RS]->y
#define _rdy     cpu->r[RD]->y

#define ODD_RD  (RD_n & 1)

#define ZF  (_st & ST_Z)
#define CF  (_st & ST_C)
#define VF  (_st & ST_V)
#define NF  (_st & ST_N)

#define FS0 (_st & ST_FS0_MASK)
#define FS1 ((_st & ST_FS1_MASK) >> ST_FS1_SHIFT)
#define FE0 ((_st & ST_FE0) ? 32 : 0)
#define FE1 ((_st & ST_FE1) ? 32 : 0)

#define CONSUME_CYCLES(n) cpu->icounter -= (n)

#define FW(i)         ((_st >> (i ? 6 : 0)) & 0x1f)
#define FWEX(i)       ((_st >> (i ? 6 : 0)) & 0x3f)

#define FW0_  FW(0)
#define FW1_  FW(1)
#define RFW0 FWEX(0)
#define RFW1 FWEX(1)

#define FW0  fw_lut[FW0_]
#define FW1  fw_lut[FW1_]

#define wsign_ext(n)    ((sdword)(sword)(n))

#define update_zn(val)              \
        _st &= ~(ST_N | ST_Z);      \
        if (!val)                   \
            _st |= ST_Z;            \
        _st |= val & SIGN_BIT32;

#define update_z(val)   \
        _st &= ~ST_Z;   \
        if (!val)        \
            _st |= ST_Z;

#define SADDR     cpu->b[_SADDR].value
#define SPTCH       cpu->b[_SPTCH].value
#define SADDR_R   cpu->b[_SADDR]

#define DADDR       cpu->b[_DADDR].value
#define DADDR_X   cpu->b[_DADDR].x
#define DADDR_Y   cpu->b[_DADDR].y
#define DADDR_R   cpu->b[_DADDR]

#define DPTCH   cpu->b[_DPTCH].value
#define OFFSET  cpu->b[_OFFSET].value
#define WSTART  cpu->b[_WSTART].value
#define WEND    cpu->b[_WEND].value

#define WSTART_X  cpu->b[_WSTART].x
#define WSTART_Y  cpu->b[_WSTART].y
#define WEND_X    cpu->b[_WEND].x
#define WEND_Y    cpu->b[_WEND].y


#define DYDX    cpu->b[_DYDX].value
#define DYDX_X    cpu->b[_DYDX].x
#define DYDX_Y    cpu->b[_DYDX].y

#define COLOR0  cpu->b[_COLOR0].value
#define COLOR1  cpu->b[_COLOR1].value
#define COUNT   cpu->b[COUNT].value
#define INC1    cpu->b[INC1].value
#define INC2    cpu->b[INC2].value
#define PATTRN  cpu->b[PATTRN].value
#define TEMP    cpu->b[TEMP].value

#define PPOP    ((cpu->io_regs[CONTROL] >> 10) & 0x1F)

#endif // TMS34010_DEFS_H
