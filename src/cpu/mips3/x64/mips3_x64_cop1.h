/*
 * Copyright (c) 2015, Marcos Medeiros
 * Licensed under BSD 3-clause.
 */
#ifndef MIPS3_X64_COP1
#define MIPS3_X64_COP1


#include "../mips3.h"
#include "mips3_x64_defs.h"
#include "mips3_x64.h"
#include "xbyak/xbyak.h"

namespace mips
{

#define DP  (RSNUM == 17 || RSNUM == 21)
#define SP  (RSNUM == 16 || RSNUM == 20)

#define INTEGER (RSNUM == 20 || RSNUM == 21)
// Word/Long precision
#define WT  (RSNUM == 20)
#define LT  (RSNUM == 21)

bool mips3_x64::BC1F(uint32_t opcode)
{
    if (m_is_delay_slot)
        return true;

    addr_t nextpc = m_drc_pc + ((int32_t)(SIMM) << 2);
    addr_t eaddr = 0;

    m_core->translate(m_drc_pc, &eaddr);
    uint32_t next_opcode = mem::read_word(eaddr);

    m_drc_pc += 4;

    update_icounter();

    inLocalLabel();
    mov(rcx, FCR_ref(31));
    mov(rcx, ptr[rcx]);
    not_(ecx);
    and_(ecx, 0x800000);
    test(ecx, ecx);
    je(".false");
    {
        m_is_delay_slot = true;
        compile_instruction(next_opcode);
        m_is_delay_slot = false;
        jmp_to_block(nextpc);
    }

    L(".false");
    compile_instruction(next_opcode);
    outLocalLabel();
    check_icounter();
    return false;

}

bool mips3_x64::BC1FL(uint32_t opcode)
{
    if (m_is_delay_slot)
        return true;

    addr_t nextpc = m_drc_pc + ((int32_t)(SIMM) << 2);
    addr_t eaddr = 0;

    m_core->translate(m_drc_pc, &eaddr);
    uint32_t next_opcode = mem::read_word(eaddr);

    m_drc_pc += 4;

    update_icounter();

    inLocalLabel();
    mov(rcx, FCR_ref(31));
    mov(rcx, ptr[rcx]);
    not_(ecx);
    and_(ecx, 0x800000);
    test(ecx, ecx);
    je(".false");
    {
        m_is_delay_slot = true;
        compile_instruction(next_opcode);
        m_is_delay_slot = false;
        jmp_to_block(nextpc);
    }

    L(".false");
    outLocalLabel();
    check_icounter();
    return false;

    return false;
}

bool mips3_x64::BC1T(uint32_t opcode)
{
    if (m_is_delay_slot)
        return true;

    addr_t nextpc = m_drc_pc + ((int32_t)(SIMM) << 2);
    addr_t eaddr = 0;

    m_core->translate(m_drc_pc, &eaddr);
    uint32_t next_opcode = mem::read_word(eaddr);

    m_drc_pc += 4;

    update_icounter();

    inLocalLabel();
    mov(rcx, FCR_ref(31));
    mov(rcx, ptr[rcx]);
    and_(ecx, 0x800000);
    test(ecx, ecx);
    je(".false");
    {
        m_is_delay_slot = true;
        compile_instruction(next_opcode);
        m_is_delay_slot = false;
        jmp_to_block(nextpc);
    }

    L(".false");
    compile_instruction(next_opcode);
    outLocalLabel();
    check_icounter();
    return false;

}

bool mips3_x64::BC1TL(uint32_t opcode)
{
    if (m_is_delay_slot)
        return true;

    addr_t nextpc = m_drc_pc + ((int32_t)(SIMM) << 2);
    addr_t eaddr = 0;

    m_core->translate(m_drc_pc, &eaddr);
    uint32_t next_opcode = mem::read_word(eaddr);

    m_drc_pc += 4;

    update_icounter();

    inLocalLabel();
    mov(rcx, FCR_ref(31));
    mov(rcx, ptr[rcx]);
    and_(ecx, 0x800000);
    test(ecx, ecx);
    je(".false");
    {
        m_is_delay_slot = true;
        compile_instruction(next_opcode);
        m_is_delay_slot = false;
        jmp_to_block(nextpc);
    }

    L(".false");
    outLocalLabel();
    check_icounter();
    return false;
}

bool mips3_x64::compile_cop1(uint32_t opcode)
{
    bool result = false;

    switch (RSNUM) {
    // MFC1 rt, rd
    case 0x00:
        if (RTNUM) {
            mov(rax, FPR_ref(RDNUM));
            mov(rcx, RT_ref);
            mov(eax, ptr[rax]);
            cdqe();
            mov(ptr[rcx], rax);
        }
        break;

    // DMFC1 rt, rd
    case 0x01:
        if (RTNUM) {
            mov(rax, FPR_ref(RDNUM));
            mov(rcx, RT_ref);
            mov(rax, ptr[rax]);
            mov(ptr[rcx], rax);
        }
        break;

    // CFC1 rt, fs
    case 0x02:
        if (RTNUM) {
            mov(rax, FCR_ref(FSNUM));
            mov(rcx, RT_ref);
            mov(eax, ptr[rax]);
            cdqe();
            mov(ptr[rcx], rax);
        }
        break;

    // MTC1 rt, fs
    case 0x04:
        mov(rax, FPR_ref(FSNUM));
        mov(rcx, RT_ref);
        mov(ecx, ptr[rcx]);
        mov(ptr[rax], rcx);
        break;

    // DMTC1 rt, fs
    case 0x05:
        mov(rax, FPR_ref(FSNUM));
        mov(rcx, RT_ref);
        mov(rcx, ptr[rcx]);
        mov(ptr[rax], rcx);
        break;

    // CTC1 rt, fs
    case 0x06:
        mov(rcx, FCR_ref(FSNUM));
        mov(rax, RT_ref);
        mov(eax, ptr[rax]);
        cdqe();
        mov(ptr[rcx], rax);
        break;

    // BC
    case 0x08:
    {
        switch ((opcode >> 16) & 3) {
        // BC1F offset
        case 0x00: result = BC1F(opcode); break;

        // BC1FL offset
        case 0x02: result = BC1FL(opcode); break;

        // BC1T offset
        case 0x01: result = BC1T(opcode); break;
            break;

        // BC1TL offset
        case 0x03: result = BC1TL(opcode); break;
        }
        break;
    }

    default:
        switch (opcode & 0x3F) {
        // ADD.fmt
        case 0x00:
            if (SP) {
                movss(xmm0, FS_x);
                movss(xmm1, FT_x);
                addss(xmm0, xmm1);
                movss(FD_x, xmm0);
            }
            else {
                movsd(xmm0, FS_x);
                movsd(xmm1, FT_x);
                addsd(xmm0, xmm1);
                movsd(FD_x, xmm0);
            }
            break;
            // SUB.fmt
        case 0x01:
            if (SP) {
                movss(xmm0, FS_x);
                movss(xmm1, FT_x);
                subss(xmm0, xmm1);
                movss(FD_x, xmm0);
            }
            else {
                movsd(xmm0, FS_x);
                movsd(xmm1, FT_x);
                subsd(xmm0, xmm1);
                movsd(FD_x, xmm0);
            }
            break;

            // MUL.fmt
        case 0x02:
            if (SP) {
                movss(xmm0, FS_x);
                movss(xmm1, FT_x);
                mulss(xmm0, xmm1);
                movss(FD_x, xmm0);
            }
            else {
                movsd(xmm0, FS_x);
                movsd(xmm1, FT_x);
                mulsd(xmm0, xmm1);
                movsd(FD_x, xmm0);
            }
            break;
            // DIV.fmt
        case 0x03:
            if (SP) {
                movss(xmm0, FS_x);
                movss(xmm1, FT_x);
                divss(xmm0, xmm1);
                movss(FD_x, xmm0);
            }
            else {
                movsd(xmm0, FS_x);
                movsd(xmm1, FT_x);
                divsd(xmm0, xmm1);
                movsd(FD_x, xmm0);
            }
            break;
            // SQRT.fmt
        case 0x04:
            if (SP) {
                movss(xmm0, FS_x);
                sqrtss(xmm1, xmm0);
                movss(FD_x, xmm1);
            }
            else {
                movsd(xmm0, FS_x);
                sqrtsd(xmm1, xmm0);
                movsd(FD_x, xmm1);
            }
            break;
            // ABS.fmt
        case 0x05: {
            static const size_t s_mask = 0x7FFFFFFF;
            static const size_t d_mask = 0x7FFFFFFFFFFFFFFFULL;
            if (SP) {
                movss(xmm0, FS_x);
                mov(rax, (size_t)&s_mask);
				movss(xmm2, dword[rax]); 
                andps(xmm0, xmm2);
                movss(FD_x, xmm0);
            }
            else {
                movsd(xmm0, FS_x);
                movsd(xmm2, ptr[&d_mask]);
                andpd(xmm0, xmm2);
                movsd(FD_x, xmm0);
            }
            break;
        }

            // MOV.fmt
        case 0x06:
            if (SP) {
                mov(eax, FS_x);
                mov(FD_x, eax);
            }
            else  {
                mov(rax, FS_x);
                mov(FD_x, rax);
            }
            break;
            // NEG.fmt
        case 0x07: {
            static const size_t s_mask = 0x80000000;
            static const size_t d_mask = 0x8000000000000000ULL;
            if (SP) {
                movss(xmm0, FS_x);
                mov(rax, (size_t)&s_mask);
				movss(xmm2, dword[rax]); 
                xorps(xmm0, xmm2);
                movss(FD_x, xmm0);
            }
            else {
                movsd(xmm0, FS_x);
                movsd(xmm2, ptr[&d_mask]);
                xorpd(xmm0, xmm2);
                movsd(FD_x, xmm0);
            }
            break;
		}

            // CVT.S.x
        case 0x20: {
            mov(rax, FS_x);
            pxor(xmm0, xmm0);
            if (INTEGER) {
                if (SP) {
                    cvtsi2ss(xmm0, eax);
                } else {
                    cvtsi2ss(xmm0, rax);
                }
            } else {
                movss(xmm0, FS_x);
            }
            movss(FD_x, xmm0);
            break;
        }

            // CVT.W.x
        case 0x24: {
            if (SP) {
                movss(xmm0, FS_x);
            }
            else {
                movsd(xmm0, FS_x);
            }
            cvtss2si(eax, xmm0);
            mov(FD_x, eax);
            break;
        }

           // C.UN.x fs, ft
        case 0x32:
           // C.F fs, ft
        case 0x3C:
            mov(rax, FCR31_x);
            and_(rax,~(0x800000ULL));
            mov(FCR31_x, rax);
            break;


            // C.OLT.x fs, ft
        case 0x34: {
            mov(rax, FCR31_x);
            mov(rcx, rax);
            and_(rcx,~(0x800000ULL));
            or_(rax, 0x800000ULL);
            if (SP) {
                movss(xmm0, FS_x);
                movss(xmm1, FT_x);
                ucomiss(xmm0, xmm1);
            }
            else  {
                movsd(xmm0, FS_x);
                movsd(xmm1, FT_x);
                ucomisd(xmm0, xmm1);
            }
            cmovnb(eax, ecx);
            mov(FCR31_x, rax);
            break;
        }

        default:
            drc_log("COP1 fallback for %x\n", opcode);
            fallback(opcode, &mips3::cop1_execute_32);
            break;
        }
        break;
    }
    return result;
}

}

#endif // MIPS3_X64_COP1

