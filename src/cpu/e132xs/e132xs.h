#pragma once

#ifndef __E132XS_H__
#define __E132XS_H__


/*
    A note about clock multipliers and dividers:

    E1-16[T] and E1-32[T] accept a straight clock

    E1-16X[T|N] and E1-32X[T|N] accept a clock and multiply it
        internally by 4; in the emulator, you MUST specify 4 * XTAL
        to achieve the correct speed

    E1-16XS[R] and E1-32XS[R] accept a clock and multiply it
        internally by 8; in the emulator, you MUST specify 8 * XTAL
        to achieve the correct speed
*/

/* Functions */

/***************************************************************************
    COMPILE-TIME DEFINITIONS
***************************************************************************/

#define PC_REGISTER          0
#define SR_REGISTER          1
#define BCR_REGISTER        20
#define TPR_REGISTER        21
#define TCR_REGISTER        22
#define TR_REGISTER         23
#define ISR_REGISTER        25
#define FCR_REGISTER        26
#define MCR_REGISTER        27

#define X_CODE(val)      ((val & 0x7000) >> 12)
#define E_BIT(val)       ((val & 0x8000) >> 15)
#define S_BIT_CONST(val) ((val & 0x4000) >> 14)
#define DD(val)          ((val & 0x3000) >> 12)


/* Extended DSP instructions */
#define EMUL            0x102
#define EMULU           0x104
#define EMULS           0x106
#define EMAC            0x10a
#define EMACD           0x10e
#define EMSUB           0x11a
#define EMSUBD          0x11e
#define EHMAC           0x02a
#define EHMACD          0x02e
#define EHCMULD         0x046
#define EHCMACD         0x04e
#define EHCSUMD         0x086
#define EHCFFTD         0x096
#define EHCFFTSD        0x296

/* Delay values */
#define NO_DELAY        0
#define DELAY_EXECUTE   1

/* IRQ numbers */
#define IRQ_INT1        0
#define IRQ_INT2        1
#define IRQ_INT3        2
#define IRQ_INT4        3
#define IRQ_IO1         4
#define IRQ_IO2         5
#define IRQ_IO3         6

/* Trap numbers */
#define TRAPNO_IO2                  48
#define TRAPNO_IO1                  49
#define TRAPNO_INT4             50
#define TRAPNO_INT3             51
#define TRAPNO_INT2             52
#define TRAPNO_INT1             53
#define TRAPNO_IO3                  54
#define TRAPNO_TIMER                55
#define TRAPNO_RESERVED1            56
#define TRAPNO_TRACE_EXCEPTION      57
#define TRAPNO_PARITY_ERROR     58
#define TRAPNO_EXTENDED_OVERFLOW    59
#define TRAPNO_RANGE_ERROR          60
#define TRAPNO_PRIVILEGE_ERROR      TRAPNO_RANGE_ERROR
#define TRAPNO_FRAME_ERROR          TRAPNO_RANGE_ERROR
#define TRAPNO_RESERVED2            61
#define TRAPNO_RESET                62  // reserved if not mapped @ MEM3
#define TRAPNO_ERROR_ENTRY          63  // for instruction code of all ones

/* Trap codes */
#define TRAPLE      4
#define TRAPGT      5
#define TRAPLT      6
#define TRAPGE      7
#define TRAPSE      8
#define TRAPHT      9
#define TRAPST      10
#define TRAPHE      11
#define TRAPE       12
#define TRAPNE      13
#define TRAPV       14
#define TRAP        15

/* Entry point to get trap locations or emulated code associated */
#define E132XS_ENTRY_MEM0   0
#define E132XS_ENTRY_MEM1   1
#define E132XS_ENTRY_MEM2   2
#define E132XS_ENTRY_IRAM   3
#define E132XS_ENTRY_MEM3   7

/***************************************************************************
    REGISTER ENUMERATION
***************************************************************************/

enum
{
	E132XS_PC = 1,
	E132XS_SR,
	E132XS_FER,
	E132XS_G3,
	E132XS_G4,
	E132XS_G5,
	E132XS_G6,
	E132XS_G7,
	E132XS_G8,
	E132XS_G9,
	E132XS_G10,
	E132XS_G11,
	E132XS_G12,
	E132XS_G13,
	E132XS_G14,
	E132XS_G15,
	E132XS_G16,
	E132XS_G17,
	E132XS_SP,
	E132XS_UB,
	E132XS_BCR,
	E132XS_TPR,
	E132XS_TCR,
	E132XS_TR,
	E132XS_WCR,
	E132XS_ISR,
	E132XS_FCR,
	E132XS_MCR,
	E132XS_G28,
	E132XS_G29,
	E132XS_G30,
	E132XS_G31,
	E132XS_CL0, E132XS_CL1, E132XS_CL2, E132XS_CL3,
	E132XS_CL4, E132XS_CL5, E132XS_CL6, E132XS_CL7,
	E132XS_CL8, E132XS_CL9, E132XS_CL10,E132XS_CL11,
	E132XS_CL12,E132XS_CL13,E132XS_CL14,E132XS_CL15,
	E132XS_L0,  E132XS_L1,  E132XS_L2,  E132XS_L3,
	E132XS_L4,  E132XS_L5,  E132XS_L6,  E132XS_L7,
	E132XS_L8,  E132XS_L9,  E132XS_L10, E132XS_L11,
	E132XS_L12, E132XS_L13, E132XS_L14, E132XS_L15,
	E132XS_L16, E132XS_L17, E132XS_L18, E132XS_L19,
	E132XS_L20, E132XS_L21, E132XS_L22, E132XS_L23,
	E132XS_L24, E132XS_L25, E132XS_L26, E132XS_L27,
	E132XS_L28, E132XS_L29, E132XS_L30, E132XS_L31,
	E132XS_L32, E132XS_L33, E132XS_L34, E132XS_L35,
	E132XS_L36, E132XS_L37, E132XS_L38, E132XS_L39,
	E132XS_L40, E132XS_L41, E132XS_L42, E132XS_L43,
	E132XS_L44, E132XS_L45, E132XS_L46, E132XS_L47,
	E132XS_L48, E132XS_L49, E132XS_L50, E132XS_L51,
	E132XS_L52, E132XS_L53, E132XS_L54, E132XS_L55,
	E132XS_L56, E132XS_L57, E132XS_L58, E132XS_L59,
	E132XS_L60, E132XS_L61, E132XS_L62, E132XS_L63
};

extern unsigned dasm_hyperstone(char *buffer, unsigned pc, const UINT8 *oprom, unsigned h_flag, int private_fp);

/* Memory access */
/* read byte */
#define READ_B(addr)            program_read_byte_16be((addr))
/* read half-word */
#define READ_HW(addr)           program_read_word_16be((addr) & ~1)
/* read word */
#define READ_W(addr)            program_read_dword_32be((addr) & ~3)

/* write byte */
#define WRITE_B(addr, data)     program_write_byte_16be(addr, data)
/* write half-word */
#define WRITE_HW(addr, data)    program_write_word_16be((addr) & ~1, data)
/* write word */
#define WRITE_W(addr, data)     program_write_dword_32be((addr) & ~3, data)


/* I/O access */
/* read word */
#define IO_READ_W(addr)         io_read_dword_32be(((addr) >> 11) & 0x7ffc)
/* write word */
#define IO_WRITE_W(addr, data)  io_write_dword_32be(((addr) >> 11) & 0x7ffc, data)


#define READ_OP(addr)          cpu_readop16(addr)


#endif /* __E132XS_H__ */
