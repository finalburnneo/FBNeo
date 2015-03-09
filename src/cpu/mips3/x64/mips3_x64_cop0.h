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
            // Hack: (total_cycles - reset_cycle) * 16
            case COP0_Count:
                m_block_icounter += 250;
                //sub(r15, 250);
                mov(rax, TOTAL_x);
                sub(rax, RSTCYC_x);
                shr(rax, 1);
                mov(RT_x, rax);
                return false;

            case COP0_Cause:
                m_block_icounter += 250;
                //sub(r15, 250);
                break;
            }
            mov(rax, COP0_x(RDNUM));
            cdqe();
            mov(RT_x, rax);
        }
        break;
    default:
        fallback(opcode, &mips3::cop0_execute);
        break;
    }
    return result;
}

}

#endif // MIPS3_X64_COP0

