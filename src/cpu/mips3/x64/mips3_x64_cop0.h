/*
 * Copyright (c) 2015, Marcos Medeiros
 * Licensed under BSD 3-clause.
 */
#ifndef MIPS3_X64_COP0
#define MIPS3_X64_COP0


#include "../mips3.h"
#include "mips3_x64_defs.h"
#include "mips3_x64.h"
#include "xbyak/xbyak.h"

namespace mips
{


bool mips3_x64::compile_cop0(uint32_t opcode)
{
    bool result = false;

    switch (RSNUM) {
    // MFC0 rt, rd
    case 0x00:
        if (RTNUM) {
            switch (RDNUM) {

            case COP0_Count:
                m_block_icounter += 250;
                mov(rax, TOTAL_x);
                sub(rax, RSTCYC_x);
                shr(rax, 1);
                and_(eax, ~0);
                mov(RT_x, rax);
                return false;

            case COP0_Cause:
                m_block_icounter += 250;
                break;
            }
            mov(rax, COP0_x(RDNUM));
            cdqe();
            mov(RT_x, rax);
        }
        break;
    // MTC0 rd, rt
    case 0x04:
        if (RDNUM == COP0_Count) {
            mov(rax, RT_x);
            mov(COP0_x(COP0_Count), rax);
            mov(rcx, TOTAL_x);
            shl(eax, 1);
            sub(rcx, rax);
            mov(RSTCYC_q, rax);
            return false;
        }
    default:
        fallback(opcode, &mips3::cop0_execute);
        break;
    }
    return result;
}

}

#endif // MIPS3_X64_COP0

