// license:BSD-3-Clause
// copyright-holders:Olivier Galibert
//
// Fujitsu MB86233 (TGP) - ported from MAME (src/devices/cpu/mb86233)
//
// Based on the reverse-engineering of Elsemi, extended by Olivier Galibert.
//
// The 86233 has two independent RAM banks, 256 dwords at 0x000-0x0ff and
// 512 dwords at 0x200-0x3ff. Everything else in the data space is external
// (Model 1 puts its fifos at 0x100 and 0x400). Only the floating point mode
// is implemented, all known Sega programs switch to it at startup.

#include "burnint.h"
#include "mb86233_intf.h"
#include <math.h>

static const UINT32 F_ZRC = 0x00000001;
static const UINT32 F_ZRD = 0x00000002;
static const UINT32 F_SGC = 0x00000004;
static const UINT32 F_SGD = 0x00000008;
static const UINT32 F_CPC = 0x00000010;
static const UINT32 F_CPD = 0x00000020;
static const UINT32 F_OVC = 0x00000040;
static const UINT32 F_OVD = 0x00000080;
static const UINT32 F_UNC = 0x00000100;
static const UINT32 F_UND = 0x00000200;
static const UINT32 F_DVZC = 0x00000400;
static const UINT32 F_DVZD = 0x00000800;
static const UINT32 F_ZX0 = 0x08000000;
static const UINT32 F_ZX1 = 0x10000000;
static const UINT32 F_ZX2 = 0x20000000;
static const UINT32 F_ZC0 = 0x40000000;
static const UINT32 F_ZC1 = 0x80000000;

#define ALU_STMASK	(F_ZRD|F_SGD|F_CPD|F_OVD|F_DVZD)

struct mb86233_regs {
	UINT32 st, a, b, d, p;
	UINT32 alu_stmask, alu_stset, alu_r1, alu_r2;
	UINT16 ppc, pc, sp, b0, b1, x0, x1, i0, i1, vsmr, pcs[4], mask, m;
	UINT8 r, rpc, c0, c1, sft, vsm;
	UINT8 gpio[4];
	UINT8 stall;
	UINT8 halt;
	UINT32 ram[0x400];		// 0x100-0x1ff is unused (external)
	INT32 total_cycles;
};

static mb86233_regs core;

static INT32 icount;
static INT32 cycles_to_run;
static INT32 end_run;

static UINT32 *program_rom = NULL;
static UINT32 program_mask = 0;

static UINT32 (*ext_data_read)(UINT32) = NULL;
static void (*ext_data_write)(UINT32, UINT32) = NULL;
static UINT32 (*ext_io_read)(UINT32) = NULL;
static void (*ext_io_write)(UINT32, UINT32) = NULL;
static UINT32 (*ext_rf_read)(UINT32) = NULL;
static void (*ext_rf_write)(UINT32, UINT32) = NULL;

// the tgp's floating point has no nan or infinity: an exponent of 255 is an ordinary
// (huge) number, taken as the largest finite value, an overflow saturates and a nan
// result (0/0) is 0 (desert tank's shells hit at the muzzle with ieee nans)
static inline float u2f(UINT32 v)
{
	union { UINT32 u; float f; } c;
	if (((v >> 23) & 0xff) == 0xff) v = (v & 0x80000000) | 0x7f7fffff;
	c.u = v;
	return c.f;
}

static inline UINT32 f2u(float v)
{
	union { UINT32 u; float f; } c;
	c.f = v;
	if (((c.u >> 23) & 0xff) == 0xff) c.u = (c.u & 0x007fffff) ? 0 : ((c.u & 0x80000000) | 0x7f7fffff);
	return c.u;
}

static inline UINT32 sext24(UINT32 v)
{
	return (v & 0x800000) ? (v | 0xff000000) : (v & 0x00ffffff);
}

static inline UINT16 sext5(UINT32 v)
{
	return (v & 0x10) ? (UINT16)(v | 0xffe0) : (UINT16)(v & 0x1f);
}

// float to int conversion with the x86 behaviour on overflow/nan, so the
// result doesn't depend on the host
static INT32 float_to_s32(double v)
{
	if (!(v >= -2147483648.0 && v < 2147483648.0))
		return (INT32)0x80000000;
	return (INT32)v;
}

static double round_half_away(double v)
{
	return (v >= 0.0) ? floor(v + 0.5) : ceil(v - 0.5);
}

// memory

static inline UINT32 data_read(UINT32 adr)
{
	adr &= 0xffff;
	if (adr < 0x100 || (adr >= 0x200 && adr < 0x400))
		return core.ram[adr];
	if (ext_data_read)
		return ext_data_read(adr);
	return 0;
}

static inline void data_write(UINT32 adr, UINT32 v)
{
	adr &= 0xffff;
	if (adr < 0x100 || (adr >= 0x200 && adr < 0x400)) {
		core.ram[adr] = v;
		return;
	}
	if (ext_data_write)
		ext_data_write(adr, v);
}

static inline UINT32 io_read(UINT32 adr)
{
	return ext_io_read ? ext_io_read(adr & 0xffff) : 0;
}

static inline void io_write(UINT32 adr, UINT32 v)
{
	if (ext_io_write)
		ext_io_write(adr & 0xffff, v);
}

static inline UINT32 program_read(UINT32 adr)
{
	return program_rom ? program_rom[adr & program_mask] : 0;
}

// cheat interface (bytes of the internal RAM)
static UINT8 mb86233CheatRead(UINT32 a)
{
	return (core.ram[(a >> 2) & 0x3ff] >> ((a & 3) * 8)) & 0xff;
}

static void mb86233CheatWrite(UINT32 a, UINT8 d)
{
	UINT32 &w = core.ram[(a >> 2) & 0x3ff];
	INT32 shift = (a & 3) * 8;
	w = (w & ~(0xff << shift)) | (d << shift);
}

static void core_set_irq(INT32, INT32, INT32)
{
}

static void core_open(INT32)
{
}

struct cpu_core_config mb86233Config =
{
	"MB86233",
	core_open,
	mb86233Close,
	mb86233CheatRead,
	mb86233CheatWrite,
	mb86233GetActive,
	mb86233TotalCycles,
	mb86233NewFrame,
	mb86233Idle,
	core_set_irq,
	mb86233Run,
	mb86233RunEnd,
	mb86233Reset,
	mb86233Scan,
	mb86233Exit,
	0x1000,
	0
};

// registers

static inline UINT32 set_exp(UINT32 val, UINT32 exp)
{
	return (val & 0x807fffff) | ((exp & 0xff) << 23);
}

static inline UINT32 set_mant(UINT32 val, UINT32 mant)
{
	return (val & 0x7f800000) | ((mant & 0x00800000) << 8) | (mant & 0x007fffff);
}

static inline UINT32 get_exp(UINT32 val)
{
	return (val >> 23) & 0xff;
}

static inline UINT32 get_mant(UINT32 val)
{
	return (val & 0x80000000) ? (val | 0x7f800000) : (val & 0x807fffff);
}

static void pcs_push()
{
	for (INT32 i = 3; i; i--)
		core.pcs[i] = core.pcs[i - 1];
	core.pcs[0] = core.pc;
}

static void pcs_pop()
{
	core.pc = core.pcs[0];
	for (INT32 i = 0; i != 3; i++)
		core.pcs[i] = core.pcs[i + 1];
}

static inline void stset_set_sz_int(UINT32 val)
{
	core.alu_stset = val ? ((val & 0x80000000) ? F_SGD : 0) : F_ZRD;
}

static inline void stset_set_sz_fp(UINT32 val)
{
	core.alu_stset = (val & 0x7fffffff) ? ((val & 0x80000000) ? F_SGD : 0) : F_ZRD;
}

static void alu_pre(UINT32 alu)
{
	switch (alu) {
		case 0x00: break;

		case 0x01: // andd
			core.alu_stmask = ALU_STMASK;
			core.alu_r1 = core.d & core.a;
			stset_set_sz_int(core.alu_r1);
			break;

		case 0x02: // orad
			core.alu_stmask = ALU_STMASK;
			core.alu_r1 = core.d | core.a;
			stset_set_sz_int(core.alu_r1);
			break;

		case 0x03: // eord
			core.alu_stmask = ALU_STMASK;
			core.alu_r1 = core.d ^ core.a;
			stset_set_sz_int(core.alu_r1);
			break;

		case 0x04: // notd
			core.alu_stmask = ALU_STMASK;
			core.alu_r1 = ~core.d;
			stset_set_sz_int(core.alu_r1);
			break;

		case 0x05: { // fcpd
			core.alu_stmask = ALU_STMASK;
			UINT32 r = f2u(u2f(core.d) - u2f(core.a));
			stset_set_sz_fp(r);
			break;
		}

		case 0x06: // fadd
			core.alu_stmask = ALU_STMASK;
			core.alu_r1 = f2u(u2f(core.d) + u2f(core.a));
			stset_set_sz_fp(core.alu_r1);
			break;

		case 0x07: // fsbd
			core.alu_stmask = ALU_STMASK;
			core.alu_r1 = f2u(u2f(core.d) - u2f(core.a));
			stset_set_sz_fp(core.alu_r1);
			break;

		case 0x08: // fml
			core.alu_stmask = 0;
			core.alu_r1 = f2u(u2f(core.a) * u2f(core.b));
			core.alu_stset = 0;
			break;

		case 0x09: // fmsd
			core.alu_stmask = ALU_STMASK;
			core.alu_r1 = f2u(u2f(core.d) + u2f(core.p));
			core.alu_r2 = f2u(u2f(core.a) * u2f(core.b));
			stset_set_sz_fp(core.alu_r1);
			break;

		case 0x0a: // fmrd
			core.alu_stmask = ALU_STMASK;
			core.alu_r1 = f2u(u2f(core.d) - u2f(core.p));
			core.alu_r2 = f2u(u2f(core.a) * u2f(core.b));
			stset_set_sz_fp(core.alu_r1);
			break;

		case 0x0b: // fabd
			core.alu_stmask = ALU_STMASK;
			core.alu_r1 = core.d & 0x7fffffff;
			stset_set_sz_fp(core.alu_r1);
			break;

		case 0x0c: // fsmd
			core.alu_stmask = ALU_STMASK;
			core.alu_r1 = f2u(u2f(core.d) + u2f(core.p));
			stset_set_sz_fp(core.alu_r1);
			break;

		case 0x0d: // fspd
			core.alu_stmask = ALU_STMASK;
			core.alu_r1 = core.p;
			core.alu_r2 = f2u(u2f(core.a) * u2f(core.b));
			stset_set_sz_fp(core.alu_r1);
			break;

		case 0x0e: // cxfd
			core.alu_stmask = ALU_STMASK;
			core.alu_r1 = f2u((float)(INT32)core.d);
			stset_set_sz_int(core.alu_r1);
			break;

		case 0x0f: { // cfxd
			core.alu_stmask = ALU_STMASK;
			double v = u2f(core.d);
			switch ((core.m >> 1) & 3) {
				case 0: core.alu_r1 = float_to_s32(round_half_away(v)); break;
				case 1: core.alu_r1 = float_to_s32(ceil(v)); break;
				case 2: core.alu_r1 = float_to_s32(floor(v)); break;
				case 3: core.alu_r1 = float_to_s32(v); break;
			}
			stset_set_sz_int(core.alu_r1);
			break;
		}

		case 0x10: // fdvd
			core.alu_stmask = ALU_STMASK;
			core.alu_r1 = f2u(u2f(core.d) / u2f(core.a));
			stset_set_sz_fp(core.alu_r1);
			break;

		case 0x11: // fned
			core.alu_stmask = ALU_STMASK;
			core.alu_r1 = core.d ? (core.d ^ 0x80000000) : 0;
			stset_set_sz_fp(core.alu_r1);
			break;

		case 0x13: // d = b + a
			core.alu_stmask = ALU_STMASK;
			core.alu_r1 = f2u(u2f(core.b) + u2f(core.a));
			stset_set_sz_fp(core.alu_r1);
			break;

		case 0x14: // d = b - a
			core.alu_stmask = ALU_STMASK;
			core.alu_r1 = f2u(u2f(core.b) - u2f(core.a));
			stset_set_sz_fp(core.alu_r1);
			break;

		case 0x16: // lsrd
			core.alu_stmask = ALU_STMASK;
			core.alu_r1 = core.d >> (core.sft & 31);
			stset_set_sz_int(core.alu_r1);
			break;

		case 0x17: // lsld
			core.alu_stmask = ALU_STMASK;
			core.alu_r1 = core.d << (core.sft & 31);
			stset_set_sz_int(core.alu_r1);
			break;

		case 0x18: // asrd
			core.alu_stmask = ALU_STMASK;
			core.alu_r1 = (UINT32)((INT32)core.d >> (core.sft & 31));
			stset_set_sz_int(core.alu_r1);
			break;

		case 0x19: // asld
			core.alu_stmask = ALU_STMASK;
			core.alu_r1 = core.d << (core.sft & 31);
			stset_set_sz_int(core.alu_r1);
			break;

		case 0x1a: // addd
			core.alu_stmask = ALU_STMASK;
			core.alu_r1 = core.d + core.a;
			stset_set_sz_int(core.alu_r1);
			break;

		case 0x1b: // subd
			core.alu_stmask = ALU_STMASK;
			core.alu_r1 = core.d - core.a;
			stset_set_sz_int(core.alu_r1);
			break;

		default:
			break;
	}
}

static inline void alu_update_st()
{
	core.st = (core.st & ~core.alu_stmask) | core.alu_stset;
}

// integer alu ops, applied before the transfer
static void alu_post_1(UINT32 alu)
{
	switch (alu) {
		case 0x01: case 0x02: case 0x03: case 0x04:
		case 0x0e: case 0x0f: case 0x16: case 0x17:
		case 0x18: case 0x19: case 0x1a: case 0x1b:
			core.d = core.alu_r1;
			alu_update_st();
			break;

		default:
			break;
	}
}

// floating point alu ops take precedence over the transfer, assumed 2 cycles
static void alu_post_2(UINT32 alu)
{
	switch (alu) {
		case 0x05:
			alu_update_st();
			icount--;
			break;

		case 0x06: case 0x07: case 0x0b: case 0x0c:
		case 0x10: case 0x11: case 0x13: case 0x14:
			core.d = core.alu_r1;
			alu_update_st();
			icount--;
			break;

		case 0x08:
			core.p = core.alu_r1;
			icount--;
			break;

		case 0x09: case 0x0a: case 0x0d:
			core.d = core.alu_r1;
			core.p = core.alu_r2;
			alu_update_st();
			icount--;
			break;

		default:
			break;
	}
}

static UINT16 ea_pre_0(UINT32 r)
{
	switch (r & 0x180) {
		case 0x000: return r & 0x7f;
		case 0x080: case 0x100: return (r & 0x7f) + core.b0 + core.x0;
		case 0x180:
			switch (r & 0x60) {
				case 0x00: return core.b0 + core.x0;
				case 0x20: return core.x0;
				case 0x40: return core.b0 + (core.x0 & core.vsmr);
				case 0x60: return core.x0 & core.vsmr;
			}
	}
	return 0;
}

static void ea_post_0(UINT32 r)
{
	if (!(r & 0x100))
		return;
	if (!(r & 0x080))
		core.x0 += core.i0;
	else
		core.x0 += sext5(r);
}

static UINT16 ea_pre_1(UINT32 r)
{
	switch (r & 0x180) {
		case 0x000: return r & 0x7f;
		case 0x080: case 0x100: return (r & 0x7f) + core.b1 + core.x1;
		case 0x180:
			switch (r & 0x60) {
				case 0x00: return core.b1 + core.x1;
				case 0x20: return core.x1;
				case 0x40: return core.b1 + (core.x1 & core.vsmr);
				case 0x60: return core.x1 & core.vsmr;
			}
	}
	return 0;
}

static void ea_post_1(UINT32 r)
{
	if (!(r & 0x100))
		return;
	if (!(r & 0x080))
		core.x1 += core.i1;
	else
		core.x1 += sext5(r);
}

static UINT32 read_reg(UINT32 r)
{
	r &= 0x3f;
	if (r >= 0x20 && r < 0x30)
		return ext_rf_read ? ext_rf_read(r & 0x1f) : 0;

	switch (r) {
		case 0x00: return core.b0;
		case 0x01: return core.b1;
		case 0x02: return core.x0;
		case 0x03: return core.x1;

		case 0x0c: return core.c0;
		case 0x0d: return core.c1;

		case 0x10: return core.a;
		case 0x11: return get_exp(core.a);
		case 0x12: return get_mant(core.a);
		case 0x13: return core.b;
		case 0x14: return get_exp(core.b);
		case 0x15: return get_mant(core.b);
		case 0x19: return core.d;
		case 0x1a: return get_exp(core.d);
		case 0x1b: return get_mant(core.d);
		case 0x1c: return core.p;
		case 0x1d: return get_exp(core.p);
		case 0x1e: return get_mant(core.p);
		case 0x1f: return core.sft;

		case 0x34: return core.rpc;
	}

	return 0;
}

static void write_reg(UINT32 r, UINT32 v)
{
	r &= 0x3f;
	if (r >= 0x20 && r < 0x30) {
		if (ext_rf_write) ext_rf_write(r & 0x1f, v);
		return;
	}

	switch (r) {
		case 0x00: core.b0 = v; break;
		case 0x01: core.b1 = v; break;
		case 0x02: core.x0 = v; break;
		case 0x03: core.x1 = v; break;

		case 0x05: core.i0 = v; break;
		case 0x06: core.i1 = v; break;

		case 0x08: core.sp = v; break;

		case 0x0a: core.vsm = v & 7; core.vsmr = (8 << core.vsm) - 1; break;

		case 0x0c:
			core.c0 = v;
			if (core.c0 == 1)
				core.st |= F_ZC0;
			else
				core.st &= ~F_ZC0;
			break;

		case 0x0d:
			core.c1 = v;
			if (core.c1 == 1)
				core.st |= F_ZC1;
			else
				core.st &= ~F_ZC1;
			break;

		case 0x0f: break;

		case 0x10: core.a = v; break;
		case 0x11: core.a = set_exp(core.a, v); break;
		case 0x12: core.a = set_mant(core.a, v); break;
		case 0x13: core.b = v; break;
		case 0x14: core.b = set_exp(core.b, v); break;
		case 0x15: core.b = set_mant(core.b, v); break;
		case 0x19: core.d = v; break;
		case 0x1a: core.d = set_exp(core.d, v); break;
		case 0x1b: core.d = set_mant(core.d, v); break;
		case 0x1c: core.p = v; break;
		case 0x1d: core.p = set_exp(core.p, v); break;
		case 0x1e: core.p = set_mant(core.p, v); break;
		case 0x1f: core.sft = v; break;

		case 0x34: core.rpc = v; break;
		case 0x3c: core.mask = v; break;
	}
}

static void write_mem_internal_1(UINT32 r, UINT32 v, INT32 bank)
{
	UINT16 ea = ea_pre_1(r);
	if (bank)
		ea += 0x200;
	data_write(ea, v);
	ea_post_1(r);
}

static void write_mem_io_1(UINT32 r, UINT32 v)
{
	UINT16 ea = ea_pre_1(r);
	io_write(ea, v);
	ea_post_1(r);
}

static void execute()
{
	while (icount > 0 && !end_run && !core.halt) {
		core.ppc = core.pc;
		UINT32 opcode = program_read(core.pc++);

		switch ((opcode >> 26) & 0x3f) {
			case 0x00: { // lab
				UINT32 r1 = opcode & 0x1ff;
				UINT32 r2 = (opcode >> 9) & 0x1ff;
				UINT32 alu = (opcode >> 21) & 0x1f;
				UINT32 op = (opcode >> 18) & 0x7;

				alu_pre(alu);

				switch (op) {
					case 0: case 1: { // lab mem, mem (e)
						UINT32 v1 = data_read(ea_pre_0(r1));
						if (core.stall) goto do_stall;
						UINT32 v2 = io_read(ea_pre_1(r2));
						if (core.stall) goto do_stall;
						ea_post_0(r1);
						ea_post_1(r2);
						core.a = v1;
						core.b = v2;
						break;
					}

					case 3: { // lab mem, mem + 0x200
						UINT32 v1 = data_read(ea_pre_0(r1));
						if (core.stall) goto do_stall;
						UINT32 v2 = data_read(ea_pre_1(r2) + 0x200);
						if (core.stall) goto do_stall;
						ea_post_0(r1);
						ea_post_1(r2);
						core.a = v1;
						core.b = v2;
						break;
					}

					case 4: { // lab mem + 0x200, mem
						UINT32 v1 = data_read(ea_pre_0(r1) + 0x200);
						if (core.stall) goto do_stall;
						UINT32 v2 = data_read(ea_pre_1(r2));
						if (core.stall) goto do_stall;
						ea_post_0(r1);
						ea_post_1(r2);
						core.a = v1;
						core.b = v2;
						break;
					}

					default:
						break;
				}

				alu_post_1(alu);
				alu_post_2(alu);
				break;
			}

			case 0x07: { // ld / mov
				UINT32 r1 = opcode & 0x1ff;
				UINT32 r2 = (opcode >> 9) & 0x1ff;
				UINT32 alu = (opcode >> 21) & 0x1f;
				UINT32 op = (opcode >> 18) & 0x7;

				alu_pre(alu);

				switch (op) {
					case 0:
					case 1: { // mov mem, mem (e)
						UINT32 v = data_read(ea_pre_0(r1));
						if (core.stall) goto do_stall;
						ea_post_0(r1);
						alu_post_1(alu);
						write_mem_io_1(r2, v);
						break;
					}

					case 2: { // mov mem (e), mem
						UINT32 v = io_read(ea_pre_0(r1));
						if (core.stall) goto do_stall;
						ea_post_0(r1);
						alu_post_1(alu);
						write_mem_internal_1(r2, v, 0);
						break;
					}

					case 3: { // mov mem, mem + 0x200
						UINT32 v = data_read(ea_pre_0(r1));
						if (core.stall) goto do_stall;
						ea_post_0(r1);
						alu_post_1(alu);
						write_mem_internal_1(r2, v, 1);
						break;
					}

					case 4: { // mov mem + 0x200, mem
						UINT32 v = data_read(ea_pre_0(r1) + 0x200);
						if (core.stall) goto do_stall;
						ea_post_0(r1);
						alu_post_1(alu);
						write_mem_internal_1(r2, v, 0);
						break;
					}

					case 5: { // mov mem (o), mem
						UINT32 v = program_read(ea_pre_0(r1));
						ea_post_0(r1);
						alu_post_1(alu);
						write_mem_internal_1(r2, v, 0);
						break;
					}

					case 7: {
						switch (r2 >> 6) {
							case 0: { // mov reg, mem
								UINT32 v = read_reg(r2);
								if (core.stall) goto do_stall;
								alu_post_1(alu);
								write_mem_internal_1(r1, v, 0);
								break;
							}

							case 1: { // mov reg, mem (e)
								UINT32 v = read_reg(r2);
								if (core.stall) goto do_stall;
								alu_post_1(alu);
								write_mem_io_1(r1, v);
								break;
							}

							case 2: { // mov mem + 0x200, reg
								UINT32 v = data_read(ea_pre_1(r1) + 0x200);
								if (core.stall) goto do_stall;
								ea_post_1(r1);
								alu_post_1(alu);
								write_reg(r2, v);
								break;
							}

							case 3: { // mov mem, reg
								UINT32 v = data_read(ea_pre_1(r1));
								if (core.stall) goto do_stall;
								ea_post_1(r1);
								alu_post_1(alu);
								write_reg(r2, v);
								break;
							}

							case 4: { // mov mem (e), reg
								UINT32 v = io_read(ea_pre_1(r1));
								if (core.stall) goto do_stall;
								ea_post_1(r1);
								alu_post_1(alu);
								write_reg(r2, v);
								break;
							}

							case 5: { // mov mem (o), reg
								UINT32 v = program_read(ea_pre_0(r1));
								ea_post_0(r1);
								alu_post_1(alu);
								write_reg(r2, v);
								break;
							}

							case 6: { // mov reg, reg
								UINT32 v = read_reg(r1);
								if (core.stall) goto do_stall;
								alu_post_1(alu);
								write_reg(r2, v);
								break;
							}

							default:
								alu_post_1(alu);
								break;
						}
						break;
					}

					default:
						alu_post_1(alu);
						break;
				}

				alu_post_2(alu);
				break;
			}

			case 0x0d: { // stm/clm
				UINT32 sub2 = (opcode >> 17) & 7;
				if (sub2 == 5) {
					// stmh: bit 0 = floating point, bits 1-2 = rounding mode
					core.m = opcode;
				}
				break;
			}

			case 0x0e: { // lipl / lia / lib / lid
				switch ((opcode >> 24) & 0x3) {
					case 0: core.p = (core.p & 0xff000000) | (opcode & 0xffffff); break;
					case 1: core.a = sext24(opcode); break;
					case 2: core.b = sext24(opcode); break;
					case 3: core.d = sext24(opcode); break;
				}
				break;
			}

			case 0x0f: { // rep/clr0/clr1/set
				UINT32 alu = (opcode >> 20) & 0x1f;
				UINT32 sub2 = (opcode >> 17) & 7;

				alu_pre(alu);

				switch (sub2) {
					case 0: // clr0
						if (opcode & 0x0004) core.a = 0;
						if (opcode & 0x0008) core.b = 0;
						if (opcode & 0x0010) core.d = 0;
						break;

					case 1: // clr1, flags mapping unknown
						break;

					case 2: { // rep
						UINT8 r = (opcode & 0x8000) ? read_reg(opcode) : opcode;
						if (core.stall) goto do_stall;
						core.r = r;
						goto rep_start;
					}

					case 3: // set, flags mapping unknown
						break;

					default:
						break;
				}

				alu_post_1(alu);
				break;
			}

			case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17:
			case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: case 0x1e: case 0x1f: // ldi
				write_reg(opcode >> 24, sext24(opcode));
				break;

			case 0x2f: case 0x3f: { // conditional branches
				UINT32 cond = (opcode >> 20) & 0x1f;
				UINT32 subtype = (opcode >> 17) & 7;
				UINT32 data = opcode & 0xffff;
				INT32 invert = opcode & 0x40000000;
				INT32 cond_passed = 0;

				switch (cond) {
					case 0x00: cond_passed = (core.st & F_ZRD) ? 1 : 0; break;            // zrd
					case 0x01: cond_passed = (core.st & F_SGD) ? 0 : 1; break;            // ged
					case 0x02: cond_passed = (core.st & (F_ZRD | F_SGD)) ? 1 : 0; break;  // led
					case 0x0a: cond_passed = core.gpio[0]; break;
					case 0x0b: cond_passed = core.gpio[1]; break;
					case 0x0c: cond_passed = core.gpio[2]; break;
					case 0x10: cond_passed = (core.st & F_ZC0) ? 0 : 1; break;            // c0 != 1
					case 0x11: cond_passed = (core.st & F_ZC1) ? 0 : 1; break;            // c1 != 1
					case 0x12: cond_passed = core.gpio[3]; break;
					case 0x16: cond_passed = 1; break;                                    // always
					default: break;
				}
				if (invert)
					cond_passed = !cond_passed;

				if (cond_passed) {
					switch (subtype) {
						case 0: // brif #adr
							core.pc = data;
							break;

						case 1: // brul
							if (opcode & 0x4000) {
								UINT32 v = read_reg(opcode);
								if (core.stall) goto do_stall;
								core.pc = v;
							} else {
								UINT32 v = data_read(ea_pre_0(opcode));
								if (core.stall) goto do_stall;
								ea_post_0(opcode);
								core.pc = v;
							}
							break;

						case 2: // bsif #adr
							pcs_push();
							core.pc = data;
							break;

						case 3: // bsul
							if (opcode & 0x4000) {
								UINT32 v = read_reg(opcode);
								if (core.stall) goto do_stall;
								pcs_push();
								core.pc = v;
							} else {
								UINT32 v = data_read(ea_pre_0(opcode));
								if (core.stall) goto do_stall;
								ea_post_0(opcode);
								pcs_push();
								core.pc = v;
							}
							break;

						case 5: // rtif
							pcs_pop();
							break;

						case 6: { // ldif adr, rn
							UINT32 v = data_read(ea_pre_0(opcode));
							if (core.stall) goto do_stall;
							ea_post_0(opcode);
							write_reg(opcode >> 9, v);
							break;
						}

						default:
							break;
					}
				}

				// the loop counters only count down when the branch is taken
				if (subtype < 2 && cond_passed) {
					if (cond == 0x10) {
						if (core.c0 != 1) {
							core.c0--;
							if (core.c0 == 1)
								core.st |= F_ZC0;
						}
					} else if (cond == 0x11) {
						if (core.c1 != 1) {
							core.c1--;
							if (core.c1 == 1)
								core.st |= F_ZC1;
						}
					}
				}
				break;
			}

			default:
				break;
		}

		if (core.r != 1) {
			core.pc = core.ppc;
			core.r--;
		}

	rep_start:
		if (0) {
		do_stall:
			core.pc = core.ppc;
			core.stall = 0;
		}
		icount--;
	}
}

// interface

void mb86233Init()
{
	memset(&core, 0, sizeof(core));
	program_rom = NULL;
	program_mask = 0;
	ext_data_read = NULL;
	ext_data_write = NULL;
	ext_io_read = NULL;
	ext_io_write = NULL;
	ext_rf_read = NULL;
	ext_rf_write = NULL;

	CpuCheatRegister(0, &mb86233Config);
}

void mb86233Exit()
{
	program_rom = NULL;
}

void mb86233Reset()
{
	INT32 total = core.total_cycles;
	UINT32 ram[0x400];
	memcpy(ram, core.ram, sizeof(ram));

	memset(&core, 0, sizeof(core));
	memcpy(core.ram, ram, sizeof(ram));
	core.total_cycles = total;

	core.st = F_ZRC | F_ZRD | F_ZX0 | F_ZX1 | F_ZX2 | F_ZC0 | F_ZC1;
	core.r = 1;
	core.rpc = 1;
	core.c0 = 1;
	core.c1 = 1;
	core.vsmr = 7;
	core.m = 1;
}

void mb86233Open(INT32)
{
}

void mb86233Close()
{
}

INT32 mb86233GetActive()
{
	return 0;
}

INT32 mb86233Run(INT32 nCycles)
{
	if (nCycles <= 0)
		return 0;

	if (core.halt) {
		core.total_cycles += nCycles;
		return nCycles;
	}

	cycles_to_run = nCycles;
	icount = nCycles;
	end_run = 0;

	execute();

	INT32 done;
	if (core.halt && !end_run) {
		// halted by a fifo during the slice, the rest of it is spent waiting
		done = nCycles;
	} else {
		done = nCycles - icount;
	}

	core.total_cycles += done;
	cycles_to_run = icount = 0;

	return done;
}

void mb86233RunEnd()
{
	end_run = 1;
}

INT32 mb86233Idle(INT32 nCycles)
{
	core.total_cycles += nCycles;
	return nCycles;
}

INT32 mb86233TotalCycles()
{
	return core.total_cycles + (cycles_to_run - icount);
}

void mb86233NewFrame()
{
	core.total_cycles = 0;
}

INT32 mb86233Scan(INT32 nAction)
{
	if (nAction & ACB_DRIVER_DATA) {
		struct BurnArea ba;
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = &core;
		ba.nLen	  = sizeof(core);
		ba.szName = "MB86233 Regs";
		BurnAcb(&ba);
	}

	return 0;
}

void mb86233SetProgram(UINT32 *pRom, UINT32 nMask)
{
	program_rom = pRom;
	program_mask = nMask;
}

void mb86233SetDataHandlers(UINT32 (*read)(UINT32), void (*write)(UINT32, UINT32))
{
	ext_data_read = read;
	ext_data_write = write;
}

void mb86233SetIOHandlers(UINT32 (*read)(UINT32), void (*write)(UINT32, UINT32))
{
	ext_io_read = read;
	ext_io_write = write;
}

void mb86233SetRFHandlers(UINT32 (*read)(UINT32), void (*write)(UINT32, UINT32))
{
	ext_rf_read = read;
	ext_rf_write = write;
}

UINT32 mb86233ReadRAM(UINT32 nAddress)
{
	return core.ram[nAddress & 0x3ff];
}

void mb86233WriteRAM(UINT32 nAddress, UINT32 nData)
{
	core.ram[nAddress & 0x3ff] = nData;
}

void mb86233Stall()
{
	core.stall = 1;
}

void mb86233SetHalt(INT32 nState)
{
	core.halt = nState ? 1 : 0;
}

INT32 mb86233GetHalt()
{
	return core.halt;
}

void mb86233SetGPIO(INT32 nLine, INT32 nState)
{
	core.gpio[nLine & 3] = nState ? 1 : 0;
}

UINT32 mb86233GetPC()
{
	return core.pc;
}
