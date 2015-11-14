/*
 * Copyright (c) 2015, Marcos Medeiros
 * Licensed under BSD 3-clause.
 */
#ifndef TMS34010_H
#define TMS34010_H

#include <cstdint>
#include <string>
#include <list>
#include <fstream>
#include <array>
#include <cmath>


namespace tms
{

// Convert bit-address to byte-address
#define TOBYTE(a)   ((a) >> 3)

typedef uint8_t byte;
typedef uint16_t word;
typedef uint32_t dword;
typedef int8_t sbyte;
typedef int16_t sword;
typedef int32_t sdword;
typedef uint64_t u64;
typedef int64_t i64;

const int fw_lut[32] = {
    32,  1,  2,  3,  4,  5,  6,  7,  8,  9,
    10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
    20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
    30, 31,
};

enum STATUS_FLAGS
{
    ST_NONE      = 0,
    ST_FS0_MASK  = 0x1F,
    ST_FS0_SHIFT = 0,
    ST_FE0       = 0x20,
    ST_FS1_MASK  = 0x7C0,
    ST_FS1_SHIFT = 6,
    ST_FE1       = 0x800,
    ST_IE        = 0x200000,
    ST_PBX       = 0x1000000,
    ST_V         = 0x10000000,
    ST_Z         = 0x20000000,
    ST_C         = 0x40000000,
    ST_N         = 0x80000000,
};

enum IO_REGISTERS
{
    IO_REFCNT   = 0x0C00001F0,
    IO_DPYADR   = 0x0C00001E0,
    IO_VCOUNT   = 0x0C00001D0,
    IO_HCOUNT   = 0x0C00001C0,
    IO_DPYTAP   = 0x0C00001B0,
    IO_PMASK    = 0x0C0000160,
    IO_PSIZE    = 0x0C0000150,
    IO_CONVDP   = 0x0C0000140,
    IO_CONVSP   = 0x0C0000130,
    IO_INTPEND  = 0x0C0000120,
    IO_INTENB   = 0x0C0000110,
    IO_HSTCTLH  = 0x0C0000100,
    IO_HSTCTLL  = 0x0C00000F0,
    IO_HSTADRH  = 0x0C00000E0,
    IO_HSTADRL  = 0x0C00000D0,
    IO_HSTDATA  = 0x0C00000C0,
    IO_CONTROL  = 0x0C00000B0,
    IO_DPYINT   = 0x0C00000A0,
    IO_DPYSTRT  = 0x0C0000090,
    IO_DPYCTL   = 0x0C0000080,
    IO_VTOTAL   = 0x0C0000070,
    IO_VSBLNK   = 0x0C0000060,
    IO_VEBLNK   = 0x0C0000050,
    IO_VESYNC   = 0x0C0000040,
    IO_HTOTAL   = 0x0C0000030,
    IO_HSBLNK   = 0x0C0000020,
    IO_HEBLNK   = 0x0C0000010,
    IO_HESYNC   = 0x0C0000000,
};

enum {
    REFCNT   = 0x1F,
    DPYADR   = 0x1E,
    VCOUNT   = 0x1D,
    HCOUNT   = 0x1C,
    DPYTAP   = 0x1B,
    PMASK    = 0x16,
    PSIZE    = 0x15,
    CONVDP   = 0x14,
    CONVSP   = 0x13,
    INTPEND  = 0x12,
    INTENB   = 0x11,
    HSTCTLH  = 0x10,
    HSTCTLL  = 0x0F,
    HSTADRH  = 0x0E,
    HSTADRL  = 0x0D,
    HSTDATA  = 0x0C,
    CONTROL  = 0x0B,
    DPYINT   = 0x0A,
    DPYSTRT  = 0x09,
    DPYCTL   = 0x08,
    VTOTAL   = 0x07,
    VSBLNK   = 0x06,
    VEBLNK   = 0x05,
    VESYNC   = 0x04,
    HTOTAL   = 0x03,
    HSBLNK   = 0x02,
    HEBLNK   = 0x01,
    HESYNC   = 0x00,
};

enum {
    INTERRUPT_EXTERN_1 = (1 << 1),
    INTERRUPT_EXTERN_2 = (1 << 2),
    INTERRUPT_HOST     = (1 << 9),
    INTERRUPT_DISPLAY  = (1 << 10),
    INTERRUPT_WINDOW   = (1 << 11),
};


extern const char *io_regs_names[32];


enum TRAP_VECTORS
{
    VECT_RESET = 0xFFFFFFE0,
};

enum {
    SIGN_BIT32 = 0x80000000,
    SIGN_BIT16 = 0x00008000
};


enum B_FILE_REGISTERS
{
    _SADDR = 0,
    _SPTCH,
    _DADDR,
    _DPTCH,
    _OFFSET,
    _WSTART,
    _WEND,
    _DYDX,
    _COLOR0,
    _COLOR1,
    _COUNT,
    _INC1,
    _INC2,
    _PATTRN,
    _TEMP,
};

static const char *const gfx_reg_names[15] = {
    "saddr","sptch","daddr","dptch","offset","wstart", "wend",
    "dydx","color0","color1","count","inc1","inc2","pattrn","temp"
};

#ifdef TMS34010_DEBUGGER
typedef enum {
    ICOUNTER_EXPIRED = 0,
    BREAKPOINT_FOUND,
} stop_reason_t;
#endif

typedef union {
    struct {
        sword x;
        sword y;
    };
    dword value;
} cpu_register;

struct cpu_state {
    cpu_register *r[32];
    cpu_register a[15];
    cpu_register b[15];
    cpu_register sp;
    dword pc;
    dword last_pc;
    dword st;
    sdword icounter;
    dword convdp;
    dword convsp;
    byte pshift;
    word io_regs[32];
    word shiftreg[4096];

    void (*shift_read_cycle)(dword address, void*);
    void (*shift_write_cycle)(dword address, void*);

#ifdef TMS34010_DEBUGGER
    // debugger...
    stop_reason_t reason;
    bool tracing;
    std::ofstream trace;
    std::array<dword,64> history;
    int loop_counter;
    int history_idx;
    std::list<dword> bpoints;
#endif
    cpu_state() {
        for (int i = 0; i < 15; ++i) {
            r[ 0 + i] = &a[i];
            r[16 + i] = &b[i];
        }
        r[15] = &sp;
        r[31] = &sp;
        pc = 0;
        sp.value = 0U;
#ifdef TMS34010_DEBUGGER
        loop_counter = 0;
        history_idx = 0;
#endif
    }
};

typedef void (*opcode_handler)(cpu_state*,word);
extern opcode_handler opcode_table[0x1000];
typedef void (*wrfield_handler)(dword, dword);
extern wrfield_handler wrfield_table[32];
typedef dword (*rdfield_handler)(dword);
extern rdfield_handler rdfield_table[64];

namespace ops {

void dint(cpu_state *cpu, word opcode);
void eint(cpu_state *cpu, word opcode);
void setf_0(cpu_state *cpu, word opcode);
void setf_1(cpu_state *cpu, word opcode);
void mmtm(cpu_state *cpu, word opcode);
void mmfm(cpu_state *cpu, word opcode);
void getpc(cpu_state *cpu, word opcode);
void pushst(cpu_state *cpu, word opcode);
void popst(cpu_state *cpu, word opcode);
void getst(cpu_state *cpu, word opcode);
void putst(cpu_state *cpu, word opcode);
void exgpc(cpu_state *cpu, word opcode);
void setc(cpu_state *cpu, word opcode);
void clrc(cpu_state *cpu, word opcode);
void reti(cpu_state *cpu, word opcode);
void exgf_rd_0(cpu_state *cpu, word opcode);
void exgf_rd_1(cpu_state *cpu, word opcode);
void trap(cpu_state *cpu, word opcode);
void emu(cpu_state *cpu, word opcode);
void rev_rd(cpu_state *cpu, word opcode);

void addxy_rs_rd(cpu_state *cpu, word opcode);
void subxy_rs_rd(cpu_state *cpu, word opcode);
void cmpxy_rs_rd(cpu_state *cpu, word opcode);

void add_rs_rd(cpu_state *cpu, word opcode);
void addc_rs_rd(cpu_state *cpu, word opcode);
void addi_iw_rd(cpu_state *cpu, word opcode);
void addi_il_rd(cpu_state *cpu, word opcode);
void subi_iw_rd(cpu_state *cpu, word opcode);
void subi_il_rd(cpu_state *cpu, word opcode);

void cmpi_iw_rd(cpu_state *cpu, word opcode);
void cmpi_il_rd(cpu_state *cpu, word opcode);

void abs_rd(cpu_state *cpu, word opcode);
void andi_il_rd(cpu_state *cpu, word opcode);
void and_rs_rd(cpu_state *cpu, word opcode);
void andn_rs_rd(cpu_state *cpu, word opcode);
void or_rs_rd(cpu_state *cpu, word opcode);
void ori_il_rd(cpu_state *cpu, word opcode);
void xor_rs_rd(cpu_state *cpu, word opcode);
void xori_il_rd(cpu_state *cpu, word opcode);
void cmp_rs_rd(cpu_state *cpu, word opcode);
void btst_rs_rd(cpu_state *cpu, word opcode);
void btst_k_rd(cpu_state *cpu, word opcode);
void not_rd(cpu_state *cpu, word opcode);
void neg_rd(cpu_state *cpu, word opcode);
void add_k_rd(cpu_state *cpu, word opcode);
void sub_k_rd(cpu_state *cpu, word opcode);

void sub_rs_rd(cpu_state *cpu, word opcode);
void divu_rs_rd(cpu_state *cpu, word opcode);
void divs_rs_rd(cpu_state *cpu, word opcode);
void modu_rs_rd(cpu_state *cpu, word opcode);
void mods_rs_rd(cpu_state *cpu, word opcode);

void rl_rs_rd(cpu_state *cpu, word opcode);
void rl_k_rd(cpu_state *cpu, word opcode);
void sll_rs_rd(cpu_state *cpu, word opcode);
void sll_k_rd(cpu_state *cpu, word opcode);
void sla_k_rd(cpu_state *cpu, word opcode);
void sla_rs_rd(cpu_state *cpu, word opcode);
void srl_rs_rd(cpu_state *cpu, word opcode);
void srl_k_rd(cpu_state *cpu, word opcode);
void sra_k_rd(cpu_state *cpu, word opcode);
void sra_rs_rd(cpu_state *cpu, word opcode);
void lmo_rs_rd(cpu_state *cpu, word opcode);

void mpyu_rs_rd(cpu_state *cpu, word opcode);
void mpys_rs_rd(cpu_state *cpu, word opcode);

void movx_rs_rd(cpu_state *cpu, word opcode);
void movy_rs_rd(cpu_state *cpu, word opcode);

void movb_rs_daddr(cpu_state *cpu, word opcode);
void movb_irs_rd(cpu_state *cpu, word opcode);
void movb_rs_ird(cpu_state *cpu, word opcode);
void movb_irs_ird(cpu_state *cpu, word opcode);
void movb_rs_irdo(cpu_state *cpu, word opcode);
void move_irs_rd_0(cpu_state *cpu, word opcode);
void move_irs_rd_1(cpu_state *cpu, word opcode);

void move_rs_ird_0(cpu_state *cpu, word opcode);
void move_rs_ird_1(cpu_state *cpu, word opcode);

void movi_iw_rd(cpu_state *cpu, word opcode);
void movi_il_rd(cpu_state *cpu, word opcode);

void move_rs_daddr_0(cpu_state *cpu, word opcode);
void move_rs_daddr_1(cpu_state *cpu, word opcode);

void move_saddr_rd_0(cpu_state *cpu, word opcode);
void move_saddr_rd_1(cpu_state *cpu, word opcode);

void movk_k_rd(cpu_state *cpu, word opcode);

void move_rs_rd(cpu_state *cpu, word opcode);
void move_rs_rd_a(cpu_state *cpu, word opcode);
void move_rs_rd_b(cpu_state *cpu, word opcode);


void move_irsp_irdp_0(cpu_state *cpu, word opcode);
void move_irsp_irdp_1(cpu_state *cpu, word opcode);

void move_rs_irdp_0(cpu_state *cpu, word opcode);
void move_rs_irdp_1(cpu_state *cpu, word opcode);

void move_irsp_rd_0(cpu_state *cpu, word opcode);
void move_irsp_rd_1(cpu_state *cpu, word opcode);

void move_rs_mird_0(cpu_state *cpu, word opcode);
void move_rs_mird_1(cpu_state *cpu, word opcode);

void move_irso_rd_0(cpu_state *cpu, word opcode);
void move_irso_rd_1(cpu_state *cpu, word opcode);

void movb_addr_rd(cpu_state *cpu, word opcode);

void move_mirs_mird_0(cpu_state *cpu, word opcode);
void move_mirs_mird_1(cpu_state *cpu, word opcode);

void move_rs_irdo_0(cpu_state *cpu, word opcode);
void move_rs_irdo_1(cpu_state *cpu, word opcode);

void movb_irso_rd(cpu_state *cpu, word opcode);

void move_irs_ird_0(cpu_state *cpu, word opcode);
void move_irs_ird_1(cpu_state *cpu, word opcode);

void move_saddr_daddr_0(cpu_state *cpu, word opcode);
void move_saddr_daddr_1(cpu_state *cpu, word opcode);

void move_irso_irdo_0(cpu_state *cpu, word opcode);
void move_irso_irdo_1(cpu_state *cpu, word opcode);

void move_mirs_rd_0(cpu_state *cpu, word opcode);
void move_mirs_rd_1(cpu_state *cpu, word opcode);

void move_addr_irsp_0(cpu_state *cpu, word opcode);
void move_addr_irsp_1(cpu_state *cpu, word opcode);

void move_irso_irdp_0(cpu_state *cpu, word opcode);
void move_irso_irdp_1(cpu_state *cpu, word opcode);

void movb_saddr_daddr(cpu_state *cpu, word opcode);

void movb_irso_irdo(cpu_state *cpu, word opcode);

void sext_rd_0(cpu_state *cpu, word opcode);
void sext_rd_1(cpu_state *cpu, word opcode);

void zext_rd_0(cpu_state *cpu, word opcode);
void zext_rd_1(cpu_state *cpu, word opcode);

void dsj(cpu_state *cpu, word opcode);
void dsjeq(cpu_state *cpu, word opcode);
void dsjne(cpu_state *cpu, word opcode);
void dsjs(cpu_state *cpu, word opcode);

void call_rs(cpu_state *cpu, word opcode);
void calla(cpu_state *cpu, word opcode);
void callr(cpu_state *cpu, word opcode);

void rets(cpu_state *cpu, word opcode);

void jump_rs(cpu_state *cpu, word opcode);

void jr_uc(cpu_state *cpu, word opcode);
void jr_uc_0(cpu_state *cpu, word opcode);
void jr_uc_8(cpu_state *cpu, word opcode);

void jr_ne(cpu_state *cpu, word opcode);
void jr_ne_0(cpu_state *cpu, word opcode);
void jr_ne_8(cpu_state *cpu, word opcode);

void jr_eq(cpu_state *cpu, word opcode);
void jr_eq_0(cpu_state *cpu, word opcode);
void jr_eq_8(cpu_state *cpu, word opcode);

void jr_nc(cpu_state *cpu, word opcode);
void jr_nc_0(cpu_state *cpu, word opcode);
void jr_nc_8(cpu_state *cpu, word opcode);

void jr_c(cpu_state *cpu, word opcode);
void jr_c_0(cpu_state *cpu, word opcode);
void jr_c_8(cpu_state *cpu, word opcode);

void jr_v(cpu_state *cpu, word opcode);
void jr_v_0(cpu_state *cpu, word opcode);
void jr_v_8(cpu_state *cpu, word opcode);

void jr_nv(cpu_state *cpu, word opcode);
void jr_nv_0(cpu_state *cpu, word opcode);
void jr_nv_8(cpu_state *cpu, word opcode);

void jr_n(cpu_state *cpu, word opcode);
void jr_n_0(cpu_state *cpu, word opcode);
void jr_n_8(cpu_state *cpu, word opcode);

void jr_nn(cpu_state *cpu, word opcode);
void jr_nn_0(cpu_state *cpu, word opcode);
void jr_nn_8(cpu_state *cpu, word opcode);

void jr_hi(cpu_state *cpu, word opcode);
void jr_hi_0(cpu_state *cpu, word opcode);
void jr_hi_8(cpu_state *cpu, word opcode);

void jr_ls(cpu_state *cpu, word opcode);
void jr_ls_0(cpu_state *cpu, word opcode);
void jr_ls_8(cpu_state *cpu, word opcode);

void jr_gt(cpu_state *cpu, word opcode);
void jr_gt_0(cpu_state *cpu, word opcode);
void jr_gt_8(cpu_state *cpu, word opcode);

void jr_ge(cpu_state *cpu, word opcode);
void jr_ge_0(cpu_state *cpu, word opcode);
void jr_ge_8(cpu_state *cpu, word opcode);

void jr_lt(cpu_state *cpu, word opcode);
void jr_lt_0(cpu_state *cpu, word opcode);
void jr_lt_8(cpu_state *cpu, word opcode);

void jr_le(cpu_state *cpu, word opcode);
void jr_le_0(cpu_state *cpu, word opcode);
void jr_le_8(cpu_state *cpu, word opcode);

void jr_p(cpu_state *cpu, word opcode);
void jr_p_0(cpu_state *cpu, word opcode);
void jr_p_8(cpu_state *cpu, word opcode);

void pixblt_b_xy(cpu_state *cpu, word opcode);
void pixt_irs_rd(cpu_state *cpu, word opcode);
void pixt_rd_irdxy(cpu_state *cpu, word opcode);
void fill_xy(cpu_state *cpu, word opcode);
void fill_l(cpu_state *cpu, word opcode);
void cvxyl_rs_rd(cpu_state *cpu, word opcode);
void drav_rs_rd(cpu_state *cpu, word opcode);
}

void generate_irq(cpu_state *cpu, int num);
void clear_irq(cpu_state *cpu, int num);

struct display_info {
    int rowaddr;
    int coladdr;
    int vsblnk;
    int veblnk;
    int heblnk;
    int hsblnk;
    int htotal;
};


typedef int (*scanline_render_t)(int line, display_info*);
int generate_scanline(cpu_state *cpu, int line, scanline_render_t render);


#ifdef TMS34010_DEBUGGER
void run(cpu_state *cpu, int cycles, bool stepping=false);
#else
void run(cpu_state *cpu, int cycles);

#endif
void reset(cpu_state *cpu);
void write_ioreg(cpu_state *cpu, dword addr, word value);
dword read_ioreg(cpu_state *cpu, dword addr);

std::string dasm(dword addr, size_t *size);
std::string new_dasm(dword pc, size_t *size);

}

#endif // TMS34010_H
