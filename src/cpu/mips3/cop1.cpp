/*
 * Copyright (c) 2015, Marcos Medeiros
 * Licensed under BSD 3-clause.
 */
#include <iostream>
#include <cmath>
#include "mips3.h"
#include "mipsdef.h"
#include "mips3_memory.h"
//#include <QDebug>

#define FPR_s(n)    (*(float  *) ((void *)&m_state.cpr[1][n]))
#define FPR_d(n)    (*(double *) ((void *)&m_state.cpr[1][n]))

#define FD_d    FPR_d(FDNUM)
#define FS_d    FPR_d(FSNUM)
#define FT_d    FPR_d(FTNUM)

#define FD_s    FPR_s(FDNUM)
#define FS_s    FPR_s(FSNUM)
#define FT_s    FPR_s(FTNUM)

#define FD_w    (*(uint32_t *) &m_state.cpr[1][FDNUM])
#define FS_w    (*(uint32_t *) &m_state.cpr[1][FSNUM])
#define FT_w    (*(uint32_t *) &m_state.cpr[1][FTNUM])

#define FD_l    m_state.cpr[1][FDNUM]
#define FS_l    m_state.cpr[1][FSNUM]
#define FT_l    m_state.cpr[1][FTNUM]

#define FCR31   m_state.fcr[31]

#define FLOAT (RSNUM == 16 || RSNUM == 17)
// Double/Single precision
#define DP  (RSNUM == 17 || RSNUM == 21)
#define SP  (RSNUM == 16 || RSNUM == 20)

#define INTEGER (RSNUM == 20 || RSNUM == 21)
// Word/Long precision
#define WT  (RSNUM == 20)
#define LT  (RSNUM == 21)

namespace mips
{

void mips3::cop1_reset()
{
    for (int i = 0; i < 32; i++) {
        FPR_d(i) = -1.0;
        m_state.fcr[i] = 0;
    }
}

void mips3::cop1_execute_16(uint32_t opcode)
{
    //qDebug() << QString::number(m_state.pc, 16) << "Op:" << RSNUM << (opcode & 0x3F) << "[FPU16]";
}

void mips3::cop1_execute_32(uint32_t opcode)
{
    //qDebug() << QString(dasm(opcode, m_prev_pc).c_str());
    switch (RSNUM) {
    // MFC1 rt, rd
    case 0x00:
        if (RTNUM)
            RT = (int32_t) m_state.cpr[1][RDNUM];
        break;

    // DMFC1 rt, rd
    case 0x01:
        if (RTNUM)
            RT = (uint64_t) m_state.cpr[1][RDNUM];
        break;

    // CFC1 rt, fs
    case 0x02:
        if (RTNUM)
            RT = (int32_t) m_state.fcr[FSNUM];
        break;

    // MTC1 rt, fs
    case 0x04:
        m_state.cpr[1][FSNUM] = (uint32_t) RT;
        break;

    // DMTC1 rt, fs
    case 0x05:
        m_state.cpr[1][FSNUM] = RT;
        break;

    // CTC1 rt, fs
    case 0x06:
        m_state.fcr[FSNUM] = (int32_t) RT;
        break;

    // BC
    case 0x08:
    {
        switch ((opcode >> 16) & 3) {
        // BC1F offset
        case 0x00:
            if (~FCR31 & 0x800000) {
                m_next_pc = m_state.pc + ((int32_t)(SIMM) << 2);
                m_delay_slot = true;
            }
            break;

        // BC1FL offset
        case 0x02:
            if (~FCR31 & 0x800000) {
                m_next_pc = m_state.pc + ((int32_t)(SIMM) << 2);
                m_delay_slot = true;
            } else
                m_state.pc += 4;
            break;

        // BC1T offset
        case 0x01:
            if (FCR31 & 0x800000) {
                m_next_pc = m_state.pc + ((int32_t)(SIMM) << 2);
                m_delay_slot = true;
            }
            break;

        // BC1TL offset
        case 0x03:
            if (FCR31 & 0x800000) {
                m_next_pc = m_state.pc + ((int32_t)(SIMM) << 2);
                m_delay_slot = true;
            } else
                m_state.pc += 4;
            break;

        default:
            //qDebug() << QString::number(m_state.pc, 16) << "OpBC:" << ((opcode >> 16) & 0x1F) << "[FPU32][COP]";
            break;
        }
        break;
    }

    default:
    {
        switch (opcode & 0x3F) {

        // ADD.fmt
        case 0x00:
            if (SP) FD_s = FS_s + FT_s;
            else    FD_d = FS_d + FT_d;
            break;

        // SUB.fmt
        case 0x01:
            if (SP) FD_s = FS_s - FT_s;
            else    FD_d = FS_d - FT_d;
            break;

        // MUL.fmt
        case 0x02:
            if (SP) FD_s = FS_s * FT_s;
            else    FD_d = FS_d * FT_d;
            break;

        // DIV.fmt
        case 0x03:
            if (SP) FD_s = FS_s / FT_s;
            else    FD_d = FS_d / FT_d;
            break;

        // SQRT.fmt
        case 0x04:
            if (SP) FD_s = sqrt(FS_s);
            else    FD_d = sqrt(FS_d);
            break;

        // ABS.fmt
        case 0x05:
            if (SP) FD_s = abs(FS_s);
            else    FD_d = abs(FS_d);
            break;

        // MOV.fmt
        case 0x06:
            if (SP) FD_s = FS_s;
            else    FD_d = FS_d;
            break;

        // NEG.fmt
        case 0x07:
            if (SP) FD_s = -FS_s;
            else    FD_d = -FS_d;
            break;

        // CVT.S.x
        case 0x20:
            if (INTEGER) {
                if (SP) FD_s = (int32_t) FS_w;
                else    FD_s = (int64_t) FS_l;
            } else
                FD_s = FS_d;
            break;

        // CVT.W.x
        case 0x24:
            if (SP) FD_w = (int32_t) FS_s;
            else    FD_w = (int32_t) FS_d;
            break;

        // C.UN.x fs, ft
        case 0x32:
            if (SP) FCR31 = (isnan(FS_s) || isnan(FT_s)) ? FCR31 | 0x800000 : FCR31 & ~0x800000;
            else    FCR31 &= ~0x800000;
            break;

        // C.OLT.x fs, ft
        case 0x34:
            if (SP) FCR31 = (FS_s < FT_s) ? FCR31 | 0x800000 : FCR31 & ~0x800000;
            else    FCR31 = (FS_d < FT_d) ? FCR31 | 0x800000 : FCR31 & ~0x800000;
            break;

        // C.F fs, ft
        case 0x3C:
            FCR31 &= ~0x800000;
            break;

        default:
            //qDebug() << QString::number(m_state.pc, 16) << "Op:" << RSNUM << (opcode & 0x3F) << "[FPU32]";
            //exit(-6);
            break;
        }
    }
    }
}

}
