/*
 * Copyright (c) 2015, Marcos Medeiros
 * Licensed under BSD 3-clause.
 */
#ifndef MIPSDEFS
#define MIPSDEFS


#define IMM     ((uint16_t) opcode)
#define SIMM    ((int16_t) opcode)
#define TARGET  (opcode & 0x03FFFFFF)

#define IMM_s64 ((int64_t) SIMM)

#define RSNUM   ((opcode >> 21) & 0x1F)
#define RTNUM   ((opcode >> 16) & 0x1F)
#define RDNUM   ((opcode >> 11) & 0x1F)

#define SHAMT   ((opcode >> 6) & 0x1F)
#define FUNCT   ((opcode >> 0) & 0x3F)

#define RS      m_state.r[RSNUM]
#define RT      m_state.r[RTNUM]
#define RD      m_state.r[RDNUM]
#define LO      m_state.lo
#define HI      m_state.hi

#define RS32    ((uint32_t) RS)
#define RT32    ((uint32_t) RT)
#define RD32    ((uint32_t) RD)

#define RS_u32    ((uint32_t) RS)
#define RT_u32    ((uint32_t) RT)
#define RD_u32    ((uint32_t) RD)

#define RS_s32    ((int32_t) RS)
#define RT_s32    ((int32_t) RT)
#define RD_s32    ((int32_t) RD)


#define RS_s64    ((int64_t) RS)
#define RT_s64    ((int64_t) RT)
#define RD_s64    ((int64_t) RD)

#define FDNUM   ((opcode >>  6) & 0x1F)
#define FSNUM   ((opcode >> 11) & 0x1F)
#define FTNUM   ((opcode >> 16) & 0x1F)

#endif // MIPSDEFS

