/*
 * Copyright (c) 2015, Marcos Medeiros
 * Licensed under BSD 3-clause.
 */
// All access to memory are 16bit
#ifndef TMS34010_MEMACC_H
#define TMS34010_MEMACC_H

#include "burnint.h"
#include "tms34010.h"
#include "tms34010_defs.h"
#include "../tms34010_intf.h"
// #include <QDebug>

namespace tms
{


#define mem_read(addr)  TMS34010ReadWord(addr)
#define mem_write(addr,v) TMS34010WriteWord(addr,v)


static inline dword mem_read_d(dword addr)
{
    dword data = mem_read(addr);
    return data | (mem_read(addr + 16) << 16);
}

static inline void mem_write_d(dword addr, dword data)
{
    mem_write(addr, data);
    mem_write(addr+16, data >> 16);
}


// Read fields


#define DEF_READ_FIELD(mask,boundary)                       \
    do {                                                    \
        const int shift = addr & 0xF;                     \
        const dword offset = addr & 0xFFFFFFF0;             \
        if (shift >= boundary) {                            \
            return (mem_read_d(offset) >> shift) & mask;    \
        }                                                   \
        return (mem_read(offset) >> shift) & mask;          \
    } while (0);

#define DEF_READ_FIELD_SX(mask,boundary,bits)               \
    do {                                                    \
        const int shift = addr & 0xF;                     \
        const dword offset = addr & 0xFFFFFFF0;             \
        dword data = 0;                                     \
        if (shift >= boundary) {                            \
            data = (mem_read_d(offset) >> shift) & mask;    \
        } else {                                            \
            data = (mem_read(offset) >> shift) & mask;      \
        }                                                   \
        return ((sdword)(data << bits)) >> bits;            \
    } while (0);


#define DEF_READ_FIELD_D(mask,boundary)                     \
    do {                                                    \
        const dword shift = addr & 0xF;                     \
        const dword offset = addr & 0xFFFFFFF0;             \
        dword data = (mem_read_d(offset) >> shift);         \
        if (shift >= boundary) {                            \
            data |= (mem_read(offset + 32) << (32 - shift));\
        }                                                   \
        return data & mask;                                 \
    } while (0);

#define DEF_READ_FIELD_D_SX(mask,boundary,bits)             \
    do {                                                    \
        const dword shift = addr & 0xF;                     \
        const dword offset = addr & 0xFFFFFFF0;             \
        dword data = (mem_read_d(offset) >> shift);         \
        if (shift >= boundary) {                            \
            data |= (mem_read(offset + 32) << (32 - shift));\
        }                                                   \
        return ((sdword)((data & mask) << bits)) >> bits;   \
    } while (0);


static dword rdfield_1(dword addr) { DEF_READ_FIELD(0x00000001, 16); }
static dword rdfield_2(dword addr) { DEF_READ_FIELD(0x00000003, 15); }
static dword rdfield_3(dword addr) { DEF_READ_FIELD(0x00000007, 14); }
static dword rdfield_4(dword addr) { DEF_READ_FIELD(0x0000000F, 13); }
static dword rdfield_5(dword addr) { DEF_READ_FIELD(0x0000001F, 12); }
static dword rdfield_6(dword addr) { DEF_READ_FIELD(0x0000003F, 11); }
static dword rdfield_7(dword addr) { DEF_READ_FIELD(0x0000007F, 10); }
static dword rdfield_8(dword addr) { DEF_READ_FIELD(0x000000FF, 9); }
static dword rdfield_9(dword addr) { DEF_READ_FIELD(0x000001FF, 8); }
static dword rdfield_10(dword addr) { DEF_READ_FIELD(0x000003FF, 7); }
static dword rdfield_11(dword addr) { DEF_READ_FIELD(0x000007FF, 6); }
static dword rdfield_12(dword addr) { DEF_READ_FIELD(0x00000FFF, 5); }
static dword rdfield_13(dword addr) { DEF_READ_FIELD(0x00001FFF, 4); }
static dword rdfield_14(dword addr) { DEF_READ_FIELD(0x00003FFF, 3); }
static dword rdfield_15(dword addr) { DEF_READ_FIELD(0x00007FFF, 2); }
static dword rdfield_16(dword addr) { DEF_READ_FIELD(0x0000FFFF, 1); }
static dword rdfield_17(dword addr) { DEF_READ_FIELD(0x0001FFFF, 0); }
static dword rdfield_18(dword addr) { DEF_READ_FIELD_D(0x0003FFFF, 15); }
static dword rdfield_19(dword addr) { DEF_READ_FIELD_D(0x0007FFFF, 13); }
static dword rdfield_20(dword addr) { DEF_READ_FIELD_D(0x000FFFFF, 12); }
static dword rdfield_21(dword addr) { DEF_READ_FIELD_D(0x001FFFFF, 11); }
static dword rdfield_22(dword addr) { DEF_READ_FIELD_D(0x003FFFFF, 10); }
static dword rdfield_23(dword addr) { DEF_READ_FIELD_D(0x007FFFFF, 9); }
static dword rdfield_24(dword addr) { DEF_READ_FIELD_D(0x00FFFFFF, 8); }
static dword rdfield_25(dword addr) { DEF_READ_FIELD_D(0x01FFFFFF, 7); }
static dword rdfield_26(dword addr) { DEF_READ_FIELD_D(0x03FFFFFF, 6); }
static dword rdfield_27(dword addr) { DEF_READ_FIELD_D(0x07FFFFFF, 5); }
static dword rdfield_28(dword addr) { DEF_READ_FIELD_D(0x0FFFFFFF, 4); }
static dword rdfield_29(dword addr) { DEF_READ_FIELD_D(0x1FFFFFFF, 3); }
static dword rdfield_30(dword addr) { DEF_READ_FIELD_D(0x3FFFFFFF, 2); }
static dword rdfield_31(dword addr) { DEF_READ_FIELD_D(0x7FFFFFFF, 1); }

static dword rdfield_32(dword addr) {
    const dword shift = addr & 0xF;
    const dword offset = addr & 0xFFFFFFF0;
    if (shift) {
        dword data = (mem_read_d(offset) >> shift);
        data |= (mem_read_d(offset + 32) << (32 - shift));
        return data;
    } else {
        return mem_read_d(offset);
    }
}

static dword rdfield_1_sx(dword addr) { DEF_READ_FIELD_SX(0x00000001, 16, 31); }
static dword rdfield_2_sx(dword addr) { DEF_READ_FIELD_SX(0x00000003, 15, 30); }
static dword rdfield_3_sx(dword addr) { DEF_READ_FIELD_SX(0x00000007, 14, 29); }
static dword rdfield_4_sx(dword addr) { DEF_READ_FIELD_SX(0x0000000F, 13, 28); }
static dword rdfield_5_sx(dword addr) { DEF_READ_FIELD_SX(0x0000001F, 12, 27); }
static dword rdfield_6_sx(dword addr) { DEF_READ_FIELD_SX(0x0000003F, 11, 26); }
static dword rdfield_7_sx(dword addr) { DEF_READ_FIELD_SX(0x0000007F, 10, 25); }
static dword rdfield_8_sx(dword addr) { DEF_READ_FIELD_SX(0x000000FF, 9, 24); }
static dword rdfield_9_sx(dword addr) { DEF_READ_FIELD_SX(0x000001FF, 8, 23); }
static dword rdfield_10_sx(dword addr) { DEF_READ_FIELD_SX(0x000003FF, 7, 22); }
static dword rdfield_11_sx(dword addr) { DEF_READ_FIELD_SX(0x000007FF, 6, 21); }
static dword rdfield_12_sx(dword addr) { DEF_READ_FIELD_SX(0x00000FFF, 5, 20); }
static dword rdfield_13_sx(dword addr) { DEF_READ_FIELD_SX(0x00001FFF, 4, 19); }
static dword rdfield_14_sx(dword addr) { DEF_READ_FIELD_SX(0x00003FFF, 3, 18); }
static dword rdfield_15_sx(dword addr) { DEF_READ_FIELD_SX(0x00007FFF, 2, 17); }
static dword rdfield_16_sx(dword addr) { DEF_READ_FIELD_SX(0x0000FFFF, 1, 16); }
static dword rdfield_17_sx(dword addr) { DEF_READ_FIELD_SX(0x0001FFFF, 0, 15); }
static dword rdfield_18_sx(dword addr) { DEF_READ_FIELD_D_SX(0x0003FFFF, 15, 14); }
static dword rdfield_19_sx(dword addr) { DEF_READ_FIELD_D_SX(0x0007FFFF, 13, 13); }
static dword rdfield_20_sx(dword addr) { DEF_READ_FIELD_D_SX(0x000FFFFF, 12, 12); }
static dword rdfield_21_sx(dword addr) { DEF_READ_FIELD_D_SX(0x001FFFFF, 11, 11); }
static dword rdfield_22_sx(dword addr) { DEF_READ_FIELD_D_SX(0x003FFFFF, 10, 10); }
static dword rdfield_23_sx(dword addr) { DEF_READ_FIELD_D_SX(0x007FFFFF, 9, 9); }
static dword rdfield_24_sx(dword addr) { DEF_READ_FIELD_D_SX(0x00FFFFFF, 8, 8); }
static dword rdfield_25_sx(dword addr) { DEF_READ_FIELD_D_SX(0x01FFFFFF, 7, 7); }
static dword rdfield_26_sx(dword addr) { DEF_READ_FIELD_D_SX(0x03FFFFFF, 6, 6); }
static dword rdfield_27_sx(dword addr) { DEF_READ_FIELD_D_SX(0x07FFFFFF, 5, 5); }
static dword rdfield_28_sx(dword addr) { DEF_READ_FIELD_D_SX(0x0FFFFFFF, 4, 4); }
static dword rdfield_29_sx(dword addr) { DEF_READ_FIELD_D_SX(0x1FFFFFFF, 3, 3); }
static dword rdfield_30_sx(dword addr) { DEF_READ_FIELD_D_SX(0x3FFFFFFF, 2, 2); }
static dword rdfield_31_sx(dword addr) { DEF_READ_FIELD_D_SX(0x7FFFFFFF, 1, 1); }


#define DEF_WRITE_FIELD(mask,boundary)                      \
    do {                                                    \
        const int shift = addr & 0xF;                     \
        const dword offset = addr & 0xFFFFFFF0;             \
        const dword wdata = data & mask;                    \
        dword mem = 0;                                      \
        if (shift >= boundary) {                            \
            mem = mem_read_d(offset) & ~(mask << shift);    \
            mem_write_d(offset, (wdata << shift) | mem);    \
        } else {                                            \
            mem = mem_read(offset) & ~((mask) << shift);    \
            mem_write(offset, (wdata << shift) | mem);      \
        }                                                   \
    } while (0);

// From MAME
#define WFIELDMAC_32()                                                              \
    if (addr & 0x0f)                                                              \
    {                                                                               \
        dword shift = addr&0x0f;                                                 \
        dword old;                                                                 \
        dword hiword;                                                              \
        dword offset = addr & 0xfffffff0;                                                       \
        old =    ((dword) mem_read_d(offset)&(0xffffffff>>(0x20-shift)));   \
        hiword = ((dword) mem_read_d(offset+0x20)&(0xffffffff<<shift));      \
        mem_write_d(offset     ,(data<<      shift) |old);          \
        mem_write_d(offset+0x20,(data>>(0x20-shift))|hiword);       \
    }                                                                               \
    else                                                                            \
        mem_write_d(addr,data);


#define DEF_WRITE_FIELD_D(mask,boundary)                        \
    do {                                                        \
        const dword shift = addr & 0xF;                         \
        const dword offset = addr & 0xFFFFFFF0;                 \
        const dword wdata = data & mask;                        \
        dword mem = 0;                                          \
        mem = mem_read_d(offset) & ~(mask << shift);            \
        mem_write_d(offset, (wdata << shift) | mem);            \
        if (shift >= boundary) {                                \
            mem = mem_read(offset + 32) & ~(mask >> (32-shift));\
            mem_write(offset + 32, (wdata >> (32-shift)) | mem);\
        }                                                       \
    } while (0);

static void wrfield_1(dword addr, dword data) { DEF_WRITE_FIELD(0x00000001, 16); }
static void wrfield_2(dword addr, dword data) { DEF_WRITE_FIELD(0x00000003, 15); }
static void wrfield_3(dword addr, dword data) { DEF_WRITE_FIELD(0x00000007, 14); }
static void wrfield_4(dword addr, dword data) { DEF_WRITE_FIELD(0x0000000F, 13); }
static void wrfield_5(dword addr, dword data) { DEF_WRITE_FIELD(0x0000001F, 12); }
static void wrfield_6(dword addr, dword data) { DEF_WRITE_FIELD(0x0000003F, 11); }
static void wrfield_7(dword addr, dword data) { DEF_WRITE_FIELD(0x0000007F, 10); }
static void wrfield_8(dword addr, dword data) { DEF_WRITE_FIELD(0x000000FF, 9); }
static void wrfield_9(dword addr, dword data) { DEF_WRITE_FIELD(0x000001FF, 8); }
static void wrfield_10(dword addr, dword data) { DEF_WRITE_FIELD(0x000003FF, 7); }
static void wrfield_11(dword addr, dword data) { DEF_WRITE_FIELD(0x000007FF, 6); }
static void wrfield_12(dword addr, dword data) { DEF_WRITE_FIELD(0x00000FFF, 5); }
static void wrfield_13(dword addr, dword data) { DEF_WRITE_FIELD(0x00001FFF, 4); }
static void wrfield_14(dword addr, dword data) { DEF_WRITE_FIELD(0x00003FFF, 3); }
static void wrfield_15(dword addr, dword data) { DEF_WRITE_FIELD(0x00007FFF, 2); }
static void wrfield_16(dword addr, dword data) { DEF_WRITE_FIELD(0x0000FFFF, 1); }
static void wrfield_17(dword addr, dword data) { DEF_WRITE_FIELD(0x0001FFFF, 0); }
static void wrfield_18(dword addr, dword data) { DEF_WRITE_FIELD_D(0x0003FFFF, 15); }
static void wrfield_19(dword addr, dword data) { DEF_WRITE_FIELD_D(0x0007FFFF, 13); }
static void wrfield_20(dword addr, dword data) { DEF_WRITE_FIELD_D(0x000FFFFF, 12); }
static void wrfield_21(dword addr, dword data) { DEF_WRITE_FIELD_D(0x001FFFFF, 11); }
static void wrfield_22(dword addr, dword data) { DEF_WRITE_FIELD_D(0x003FFFFF, 10); }
static void wrfield_23(dword addr, dword data) { DEF_WRITE_FIELD_D(0x007FFFFF, 9); }
static void wrfield_24(dword addr, dword data) { DEF_WRITE_FIELD_D(0x00FFFFFF, 8); }
static void wrfield_25(dword addr, dword data) { DEF_WRITE_FIELD_D(0x01FFFFFF, 7); }
static void wrfield_26(dword addr, dword data) { DEF_WRITE_FIELD_D(0x03FFFFFF, 6); }
static void wrfield_27(dword addr, dword data) { DEF_WRITE_FIELD_D(0x07FFFFFF, 5); }
static void wrfield_28(dword addr, dword data) { DEF_WRITE_FIELD_D(0x0FFFFFFF, 4); }
static void wrfield_29(dword addr, dword data) { DEF_WRITE_FIELD_D(0x1FFFFFFF, 3); }
static void wrfield_30(dword addr, dword data) { DEF_WRITE_FIELD_D(0x3FFFFFFF, 2); }
static void wrfield_31(dword addr, dword data) { DEF_WRITE_FIELD_D(0x7FFFFFFF, 1); }
//static void wrfield_32(dword addr, dword data) { DEF_WRITE_FIELD_D(0xFFFFFFFF, 0); }
static void wrfield_32(dword addr, dword data) { WFIELDMAC_32(); }

#define wrfield0(a,v) wrfield_table[FW0_](a,v)
#define wrfield1(a,v) wrfield_table[FW1_](a,v)
#define rdfield0(a) rdfield_table[RFW0](a)
#define rdfield1(a) rdfield_table[RFW1](a)

static inline void __push(cpu_state *cpu, dword value) {
    _sp -= 32;
    mem_write_d(_sp, value);
}

static inline dword __pop(cpu_state *cpu) {
    dword value = mem_read_d(_sp);
    _sp += 32;
    return value;
}

#define _push(v)    __push(cpu, v)
#define _pop(v)     __pop(cpu)

} // tms

#endif // TMS34010_MEMACC_H
