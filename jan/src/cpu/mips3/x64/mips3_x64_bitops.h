#ifndef MIPS3_X64_BITOPS
#define MIPS3_X64_BITOPS

#include "../mips3.h"
#include "mips3_x64_defs.h"
#include "mips3_x64.h"
#include "xbyak/xbyak.h"

namespace mips
{


inline bool mips3_x64::AND(uint32_t opcode)
{
    if (RDNUM) {
        mov(rax, RS_x);
        and_(rax, RT_x);
        mov(RD_x, rax);
    }
    return false;
}

inline bool mips3_x64::XOR(uint32_t opcode)
{
    if (RDNUM) {
        mov(rax, RS_x);
        xor_(rax, RT_x);
        mov(RD_x, rax);
    }
    return false;
}

inline bool mips3_x64::OR(uint32_t opcode)
{
    if (RDNUM) {
        mov(rax, RS_x);
        or_(rax, RT_x);
        mov(RD_x, rax);
    }
    return false;
}

inline bool mips3_x64::NOR(uint32_t opcode)
{
    if (RDNUM) {
        mov(rax, RS_x);
        or_(rax, RT_x);
        not_(rax);
        mov(RD_x, rax);
    }
    return false;
}

inline bool mips3_x64::ANDI(uint32_t opcode)
{
    if (RTNUM) {
        mov(rax, RS_x);
        and_(rax, IMM);
        mov(RT_x, rax);
    }
    return false;
}

inline bool mips3_x64::XORI(uint32_t opcode)
{
    if (RTNUM) {
        mov(rax, RS_x);
        xor_(rax, IMM);
        mov(RT_x, rax);
    }
    return false;
}

inline bool mips3_x64::ORI(uint32_t opcode)
{
    if (RTNUM) {
        mov(rax, RS_x);
        or_(rax, IMM);
        mov(RT_x, rax);
    }
    return false;
}

}

#endif // MIPS3_X64_BITOPS

