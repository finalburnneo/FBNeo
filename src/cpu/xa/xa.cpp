// license:BSD-3-Clause
// copyright-holders:David Haywood
// FBNeo port from MAME 0.289, Aug 5, 2026

/*
   While advertised as a type of 80c51 (and even referred to as such by the test mode in some IGS titles)
   this is a distinct architecture.  The opcode set does extend on an 80c51, and tools were provided to
   help convert 80c51 sources to run on the XA architecture, but the encoding is entirely different and
   there is no binary compatibility.

   https://www.ceibo.com/eng/datasheets/Philips-XA-User-Guide.pdf
*/

#include "burnint.h"
#include "xa.h"
#include "xa_intf.h"

#define LOGMASKED(...)
#define fatalerror(...)

extern UINT8 xa_program_read_byte(UINT32 address);
extern UINT16 xa_program_read_word(UINT32 address);
extern UINT8 xa_data_read_byte(UINT32 address);
extern void xa_data_write_byte(UINT32 address, UINT8 data);
extern UINT16 xa_data_read_word(UINT32 address);
extern void xa_data_write_word(UINT32 address, UINT16 data);
extern UINT8 xa_port_read(UINT32 port);
extern void xa_port_write(UINT32 port, UINT8 data);

xa_cpu::xa_cpu()
	: m_pc(0)
	, m_icount(0)
	, m_total_cycles(0)
	, m_clock(0)
{
}

u8 xa_cpu::program_read_byte(u32 address)   { return xa_program_read_byte(address); }
u16 xa_cpu::program_read_word(u32 address)  { return xa_program_read_word(address); }
u8 xa_cpu::data_read_byte(u32 address)      { return xa_data_read_byte(address); }
void xa_cpu::data_write_byte(u32 address, u8 data) { xa_data_write_byte(address, data); }
u16 xa_cpu::data_read_word(u32 address)     { return xa_data_read_word(address); }
void xa_cpu::data_write_word(u32 address, u16 data) { xa_data_write_word(address, data); }

/*****************************************************************************/

u8 xa_cpu::sfr_port_r(u8 offset)
{
	return xa_port_read(offset);
}

void xa_cpu::sfr_port_w(u8 offset, u8 data)
{
	xa_port_write(offset, data);
}

u8 xa_cpu::sfr_PxCFGA_r(u8 offset)
{
	return m_PxCFGA[offset];
}

void xa_cpu::sfr_PxCFGA_w(u8 offset, u8 data)
{
	LOGMASKED(LOG_DEBUG, "P%d CFGA - Port Configuration A write %02x\n", offset, data);
	m_PxCFGA[offset] = data;
}

u8 xa_cpu::sfr_PxCFGB_r(u8 offset)
{
	return m_PxCFGB[offset];
}

void xa_cpu::sfr_PxCFGB_w(u8 offset, u8 data)
{
	LOGMASKED(LOG_DEBUG, "P%d CFGB - Port Configuration B write %02x\n", offset, data);
	m_PxCFGB[offset] = data;
}



void xa_cpu::set_pc_in_current_page(u16 addr)
{
	m_pc = addr;
}

u8 xa_cpu::sfr_WDCON_r()
{
	return m_WDCON;
}

// PSWL  C AC - - - V N Z
u8 xa_cpu::sfr_PSWL_r()
{
	u8 ret = m_PSWL & 0x38;
	if (get_z_flag()) ret |= 0x01;
	if (get_n_flag()) ret |= 0x02;
	if (get_v_flag()) ret |= 0x04;
	if (get_ac_flag()) ret |= 0x40;
	if (get_c_flag()) ret |= 0x80;
	LOGMASKED(LOG_DEBUG, "read %02x from PSWL\n", ret);
	return ret;
}

void xa_cpu::sfr_PSWL_w(u8 data)
{
	LOGMASKED(LOG_DEBUG, "write %02x to PSWL\n", data);
	m_PSWL = data;
	if (data & 0x01) set_z_flag(); else clear_z_flag();
	if (data & 0x02) set_n_flag(); else clear_n_flag();
	if (data & 0x04) set_v_flag(); else clear_v_flag();
	if (data & 0x40) set_ac_flag(); else clear_ac_flag();
	if (data & 0x80) set_c_flag(); else clear_c_flag();
}

u8 xa_cpu::sfr_PSWH_r()
{
	//u8 ret = m_PSWH;
	//LOGMASKED(LOG_DEBUG, "read %02x from PSWH\n", ret);
	return m_PSWH;
}

void xa_cpu::sfr_PSWH_w(u8 data)
{
	// PSWH  SM TM RS1 RS0 IM3 IM2 IM1 IM0
	LOGMASKED(LOG_DEBUG, "write %02x to PSWH\n", data);
	m_PSWH = data;

	if (m_PSWH & 0x80)
		m_usermode = false;
	else
		m_usermode = true;

	m_regbank = (m_PSWH & 0x30) >> 4;

}

void xa_cpu::sfr_PSW51_w(u8 data)
{
	// PSW51  C AC F0 RS1 RS0 OV F1 P

}

u8 xa_cpu::sfr_IEL_r()
{
	return m_IEL;
}

void xa_cpu::sfr_IEL_w(u8 data)
{
	m_IEL = data;
	LOGMASKED(LOG_DEBUG, "write to m_IEL %02x\n", data);
}

void xa_cpu::sfr_SCR_w(u8 data)
{
	/* System Configuration Register (SCR)

	---- PPCZ

	 PP = PT0/1: Peripheral timer multiplier
	 C = CM: 80c51 Compatibility Mode (registers appear in data memory, indirect addressing behavior changed)
	 Z = Page Zero / Small Memory mode (only 16-bits of address are pushed / pulled from stack, faster timing on those ops)

	*/

	LOGMASKED(LOG_DEBUG, "write %02x to SCR\n", data);
	m_SCR = data;
	m_pagezeromode = (data & 1);
}

void xa_cpu::sfr_WFEED1_w(u8 data)
{
	LOGMASKED(LOG_DEBUG, "write %02x to WFEED1\n", data);
}

void xa_cpu::sfr_WFEED2_w(u8 data)
{
	LOGMASKED(LOG_DEBUG, "write %02x to WFEED2\n", data);
}

/*****************************************************************************/
// SFR block dispatch - hand-ported from MAME's xa_cpu::sfr_map() address_map.
// offset is the SFR-relative address (0x000-0x7ff), i.e. (direct_addr - 0x400).

u8 xa_cpu::sfr_read_byte(u16 offset)
{
	switch (offset)
	{
		case 0x000: return sfr_PSWL_r();
		case 0x001: return sfr_PSWH_r();
		case 0x01f: return sfr_WDCON_r();
		case 0x026: return sfr_IEL_r();
		case 0x030: case 0x031: case 0x032: case 0x033:
			return sfr_port_r(offset & 3);
		case 0x070: case 0x071: case 0x072: case 0x073:
			return sfr_PxCFGA_r(offset & 3);
		case 0x0f0: case 0x0f1: case 0x0f2: case 0x0f3:
			return sfr_PxCFGB_r(offset & 3);
	}

	return 0;
}

void xa_cpu::sfr_write_byte(u16 offset, u8 data)
{
	switch (offset)
	{
		case 0x000: sfr_PSWL_w(data); return;
		case 0x001: sfr_PSWH_w(data); return;
		case 0x002: sfr_PSW51_w(data); return;
		case 0x026: sfr_IEL_w(data); return;
		case 0x030: case 0x031: case 0x032: case 0x033:
			sfr_port_w(offset & 3, data); return;
		case 0x040: sfr_SCR_w(data); return;
		case 0x05d: sfr_WFEED1_w(data); return;
		case 0x05e: sfr_WFEED2_w(data); return;
		case 0x070: case 0x071: case 0x072: case 0x073:
			sfr_PxCFGA_w(offset & 3, data); return;
		case 0x0f0: case 0x0f1: case 0x0f2: case 0x0f3:
			sfr_PxCFGB_w(offset & 3, data); return;
	}
}



/*****************************************************************************/

const xa_cpu::op_func xa_cpu::s_instruction[256] =
{
// group 0
&xa_cpu::e_nop,          &xa_cpu::e_add,      &xa_cpu::e_add,      &xa_cpu::e_add,     &xa_cpu::e_add,     &xa_cpu::e_add,     &xa_cpu::e_add,      &xa_cpu::e_push_rlist,
&xa_cpu::e_bitgroup,     &xa_cpu::e_add,      &xa_cpu::e_add,      &xa_cpu::e_add,     &xa_cpu::e_add,     &xa_cpu::e_add,     &xa_cpu::e_add,      &xa_cpu::e_push_rlist,
// group 1
&xa_cpu::e_illegal,      &xa_cpu::e_addc,     &xa_cpu::e_addc,     &xa_cpu::e_addc,    &xa_cpu::e_addc,    &xa_cpu::e_addc,    &xa_cpu::e_addc,     &xa_cpu::e_pushu_rlist,
&xa_cpu::e_illegal,      &xa_cpu::e_addc,     &xa_cpu::e_addc,     &xa_cpu::e_addc,    &xa_cpu::e_addc,    &xa_cpu::e_addc,    &xa_cpu::e_addc,     &xa_cpu::e_pushu_rlist,
// group 2
&xa_cpu::e_illegal,      &xa_cpu::e_sub,      &xa_cpu::e_sub,      &xa_cpu::e_sub,     &xa_cpu::e_sub,     &xa_cpu::e_sub,     &xa_cpu::e_sub,      &xa_cpu::e_pop_rlist,
&xa_cpu::e_illegal,      &xa_cpu::e_sub,      &xa_cpu::e_sub,      &xa_cpu::e_sub,     &xa_cpu::e_sub,     &xa_cpu::e_sub,     &xa_cpu::e_sub,      &xa_cpu::e_pop_rlist,
// group 3
&xa_cpu::e_illegal,      &xa_cpu::e_subb,     &xa_cpu::e_subb,     &xa_cpu::e_subb,    &xa_cpu::e_subb,    &xa_cpu::e_subb,    &xa_cpu::e_subb,     &xa_cpu::e_popu_rlist,
&xa_cpu::e_illegal,      &xa_cpu::e_subb,     &xa_cpu::e_subb,     &xa_cpu::e_subb,    &xa_cpu::e_subb,    &xa_cpu::e_subb,    &xa_cpu::e_subb,     &xa_cpu::e_popu_rlist,
// group 4
&xa_cpu::e_lea_offset8,  &xa_cpu::e_cmp,      &xa_cpu::e_cmp,      &xa_cpu::e_cmp,     &xa_cpu::e_cmp,     &xa_cpu::e_cmp,     &xa_cpu::e_cmp,      &xa_cpu::e_push_rlist,
&xa_cpu::e_lea_offset16, &xa_cpu::e_cmp,      &xa_cpu::e_cmp,      &xa_cpu::e_cmp,     &xa_cpu::e_cmp,     &xa_cpu::e_cmp,     &xa_cpu::e_cmp,      &xa_cpu::e_push_rlist,
// group 5
&xa_cpu::e_xch_type1,    &xa_cpu::e_and,      &xa_cpu::e_and,      &xa_cpu::e_and,     &xa_cpu::e_and,     &xa_cpu::e_and,     &xa_cpu::e_and,      &xa_cpu::e_pushu_rlist,
&xa_cpu::e_xch_type1,    &xa_cpu::e_and,      &xa_cpu::e_and,      &xa_cpu::e_and,     &xa_cpu::e_and,     &xa_cpu::e_and,     &xa_cpu::e_and,      &xa_cpu::e_pushu_rlist,
// group 6
&xa_cpu::e_xch_type2,    &xa_cpu::e_or,       &xa_cpu::e_or,       &xa_cpu::e_or,      &xa_cpu::e_or,      &xa_cpu::e_or,      &xa_cpu::e_or,       &xa_cpu::e_pop_rlist,
&xa_cpu::e_xch_type2,    &xa_cpu::e_or,       &xa_cpu::e_or,       &xa_cpu::e_or,      &xa_cpu::e_or,      &xa_cpu::e_or,      &xa_cpu::e_or,       &xa_cpu::e_pop_rlist,
// group 7
&xa_cpu::e_illegal,      &xa_cpu::e_xor,      &xa_cpu::e_xor,      &xa_cpu::e_xor,     &xa_cpu::e_xor,     &xa_cpu::e_xor,     &xa_cpu::e_xor,      &xa_cpu::e_popu_rlist,
&xa_cpu::e_illegal,      &xa_cpu::e_xor,      &xa_cpu::e_xor,      &xa_cpu::e_xor,     &xa_cpu::e_xor,     &xa_cpu::e_xor,     &xa_cpu::e_xor,      &xa_cpu::e_popu_rlist,
// group 8
&xa_cpu::e_movc_rd_rsinc,&xa_cpu::e_mov,      &xa_cpu::e_mov,      &xa_cpu::e_mov,     &xa_cpu::e_mov,     &xa_cpu::e_mov,     &xa_cpu::e_mov,      &xa_cpu::e_pushpop_djnz_subgroup,
&xa_cpu::e_movc_rd_rsinc,&xa_cpu::e_mov,      &xa_cpu::e_mov,      &xa_cpu::e_mov,     &xa_cpu::e_mov,     &xa_cpu::e_mov,     &xa_cpu::e_mov,      &xa_cpu::e_pushpop_djnz_subgroup,
// group 9
&xa_cpu::e_g9_subgroup,  &xa_cpu::e_alu,      &xa_cpu::e_alu,      &xa_cpu::e_alu,     &xa_cpu::e_alu,     &xa_cpu::e_alu,     &xa_cpu::e_alu,      &xa_cpu::e_jb_mov_subgroup,
&xa_cpu::e_g9_subgroup,  &xa_cpu::e_alu,      &xa_cpu::e_alu,      &xa_cpu::e_alu,     &xa_cpu::e_alu,     &xa_cpu::e_alu,     &xa_cpu::e_alu,      &xa_cpu::e_jb_mov_subgroup,
// group a
&xa_cpu::e_movdir,       &xa_cpu::e_adds,     &xa_cpu::e_adds,     &xa_cpu::e_adds,    &xa_cpu::e_adds,    &xa_cpu::e_adds,    &xa_cpu::e_adds,     &xa_cpu::e_movx_subgroup,
&xa_cpu::e_movdir,       &xa_cpu::e_adds,     &xa_cpu::e_adds,     &xa_cpu::e_adds,    &xa_cpu::e_adds,    &xa_cpu::e_adds,    &xa_cpu::e_adds,     &xa_cpu::e_movx_subgroup,
// group b
&xa_cpu::e_rr,           &xa_cpu::e_movs,     &xa_cpu::e_movs,     &xa_cpu::e_movs,    &xa_cpu::e_movs,    &xa_cpu::e_movs,    &xa_cpu::e_movs,     &xa_cpu::e_rrc,
&xa_cpu::e_rr,           &xa_cpu::e_movs,     &xa_cpu::e_movs,     &xa_cpu::e_movs,    &xa_cpu::e_movs,    &xa_cpu::e_movs,    &xa_cpu::e_movs,     &xa_cpu::e_rrc,
// group c
&xa_cpu::e_lsr_fc,       &xa_cpu::e_asl_c,    &xa_cpu::e_asr_c,    &xa_cpu::e_norm,    &xa_cpu::e_lsr_fc,  &xa_cpu::e_asl_c,   &xa_cpu::e_asr_c,    &xa_cpu::e_norm,
&xa_cpu::e_lsr_fc,       &xa_cpu::e_asl_c,    &xa_cpu::e_asr_c,    &xa_cpu::e_norm,    &xa_cpu::e_lsr_fc,  &xa_cpu::e_asl_c,   &xa_cpu::e_asr_c,    &xa_cpu::e_norm,
// group d
&xa_cpu::e_lsr_fj,       &xa_cpu::e_asl_j,   &xa_cpu::e_asr_j,     &xa_cpu::e_rl,       &xa_cpu::e_lsr_fj,  &xa_cpu::e_asl_j,  &xa_cpu::e_asr_j,    &xa_cpu::e_rlc,
&xa_cpu::e_lsr_fj,       &xa_cpu::e_asl_j,   &xa_cpu::e_asr_j,     &xa_cpu::e_rl,       &xa_cpu::e_lsr_fj,  &xa_cpu::e_asl_j,  &xa_cpu::e_asr_j,    &xa_cpu::e_rlc,
// group e
&xa_cpu::e_mulu_b,       &xa_cpu::e_divu_b,  &xa_cpu::e_djnz_cjne, &xa_cpu::e_cjne_d8,  &xa_cpu::e_mulu_w,  &xa_cpu::e_divu_w, &xa_cpu::e_mul_w,    &xa_cpu::e_div_w,
&xa_cpu::e_div_data8,    &xa_cpu::e_div_d16, &xa_cpu::e_djnz_cjne, &xa_cpu::e_cjne_d16, &xa_cpu::e_jz_rel8, &xa_cpu::e_divu_d, &xa_cpu::e_jnz_rel8, &xa_cpu::e_div_d,
// group f
&xa_cpu::e_branch,       &xa_cpu::e_branch,  &xa_cpu::e_branch,    &xa_cpu::e_branch,   &xa_cpu::e_branch,  &xa_cpu::e_branch, &xa_cpu::e_branch,   &xa_cpu::e_branch,
&xa_cpu::e_branch,       &xa_cpu::e_branch,  &xa_cpu::e_branch,    &xa_cpu::e_branch,   &xa_cpu::e_branch,  &xa_cpu::e_branch, &xa_cpu::e_branch,   &xa_cpu::e_bkpt,
};

u16 xa_cpu::expand_rel16(u16 rel16)
{
	int address = m_pc + ((s16)rel16) * 2;
	address &= ~1; // must be word aligned
	return address;
}

u16 xa_cpu::expand_rel8(u8 rel8)
{
	int address = m_pc + ((s8)rel8) * 2;
	address &= ~1; // must be word aligned
	return address;
}


void xa_cpu::do_nz_flags_16(u16 data)
{
	if (data & 0x8000)
		set_n_flag();
	else
		clear_n_flag();

	if (data == 0x0000)
		set_z_flag();
	else
		clear_z_flag();
}


void xa_cpu::do_nz_flags_8(u8 data)
{
	if (data & 0x80)
		set_n_flag();
	else
		clear_n_flag();

	if (data == 0x00)
		set_z_flag();
	else
		clear_z_flag();
}

void xa_cpu::push_word_to_user_stack(u16 data)
{
	// should use the segment register as well!
	m_USP -= 2;
	data_write_word(m_USP, data);
}

void xa_cpu::push_word_to_system_stack(u16 data)
{
	m_SSP -= 2;
	data_write_word(m_SSP, data);
}

void xa_cpu::push_word_to_stack(u16 data)
{
	if (m_usermode)
	{
		push_word_to_user_stack(data);
	}
	else
	{
		push_word_to_system_stack(data);
	}
}

u16 xa_cpu::pull_word_from_user_stack()
{
	// should use the segment register as well!
	u16 data = data_read_word(m_USP);
	m_USP += 2;
	return data;
}

u16 xa_cpu::pull_word_from_system_stack()
{
	u16 data = data_read_word(m_SSP);
	m_SSP += 2;
	return data;
}


u16 xa_cpu::pull_word_from_stack()
{
	if (m_usermode)
	{
		return pull_word_from_user_stack();
	}
	else
	{
		return pull_word_from_system_stack();
	}
}

void xa_cpu::push_byte_to_user_stack(u8 data)
{
	// should use the segment register as well!
	m_USP -= 2;
	data_write_byte(m_USP, data);
}

void xa_cpu::push_byte_to_system_stack(u8 data)
{
	m_SSP -= 2;
	data_write_byte(m_SSP, data);
}

void xa_cpu::push_byte_to_stack(u16 data)
{
	if (m_usermode)
	{
		push_byte_to_user_stack(data);
	}
	else
	{
		push_byte_to_system_stack(data);
	}
}

u8 xa_cpu::pull_byte_from_user_stack()
{
	// should use the segment register as well!
	u8 data = data_read_byte(m_USP);
	m_USP += 2;
	return data;
}

u8 xa_cpu::pull_byte_from_system_stack()
{
	u8 data = data_read_byte(m_SSP);
	m_SSP += 2;
	return data;
}

u8 xa_cpu::pull_byte_from_stack()
{
	if (m_usermode)
	{
		return pull_byte_from_user_stack();
	}
	else
	{
		return pull_byte_from_system_stack();
	}
}


u8 xa_cpu::gr8(int reg)
{
	int high = reg & 1;

	reg >>= 1;

	if (reg < 4)
	{
		// banked regs
		int regbank = m_regbank;

		if (high)
			return (m_regs[(regbank * 4) + reg] & 0xff00) >> 8;
		else
			return (m_regs[(regbank * 4) + reg] & 0x00ff) >> 0;
	}
	else
	{
		if (reg == 7)
		{
			fatalerror("gr8 on register R7\n");
			return 0;
		}

		reg -= 4;
		if (high)
			return (m_regs[(4 * 4) + reg] & 0xff00) >> 8;
		else
			return (m_regs[(4 * 4) + reg] & 0x00ff) >> 0;
	}
}

void xa_cpu::sr8(int reg, u8 data)
{
	int high = reg & 1;

	reg >>= 1;

	if (reg < 4)
	{
		// banked regs
		int regbank = m_regbank;

		if (high)
			m_regs[(regbank * 4) + reg] = (m_regs[(regbank * 4) + reg] & 0x00ff) | (data << 8);
		else
			m_regs[(regbank * 4) + reg] = (m_regs[(regbank * 4) + reg] & 0xff00) | (data << 0);
	}
	else
	{
		if (reg == 7)
		{
			fatalerror("sr8 on register R7\n");
		}

		reg -= 4;
		if (high)
			m_regs[(4 * 4) + reg] = (m_regs[(4 * 4) + reg] & 0x00ff) | (data << 8);
		else
			m_regs[(4 * 4) + reg] = (m_regs[(4 * 4) + reg] & 0xff00) | (data << 0);
	}
}


void xa_cpu::sr16(int reg, u16 data)
{
	if (reg < 4)
	{
		// banked regs
		int regbank = m_regbank;
		m_regs[(regbank * 4) + reg] = data;

	}
	else if (reg < 8)
	{
		if (reg == 7)
		{
			// R7 is the stack pointer, which switches between USP and SSP depending on mode
			if (m_usermode)
				m_USP = data;
			else
				m_SSP = data;

			return;
		}

		// do we need to calculate parity bit (in PSW51) on all R4 writes (as R4 as backwards compatibility with accumulator)
		// or only when the backwards compatible instructions are used? (for now, simply ignore it)
		reg -= 4;
		m_regs[(4 * 4) + reg] = data;
	}
	else
	{
		fatalerror("sr16 with reg %d val = %04x\n", reg, data);
	}
}

u16 xa_cpu::gr16(int reg)
{
	if (reg < 4)
	{
		// banked regs
		int regbank = m_regbank;
		return m_regs[(regbank * 4) + reg];

	}
	else if (reg < 8)
	{
		if (reg == 7)
		{
			// R7 is the stack pointer, which switches between USP and SSP depending on mode
			if (m_usermode)
				return m_USP;
			else
				return m_SSP;
		}

		// do we need to calculate parity bit (in PSW51) on all R4 writes (as R4 as backwards compatibility with accumulator)
		// or only when the backwards compatible instructions are used? (for now, simply ignore it)
		reg -= 4;
		return m_regs[(4 * 4) + reg];
	}
	else
	{
		fatalerror("gr16 with reg %d\n", reg);
		return 0;
	}
}

void xa_cpu::wdat8(int address, u8 data)
{
	data_write_byte(address, data);
}

void xa_cpu::wdat16(int address, u16 data)
{
	if (address & 1)
		fatalerror("unaligned in wdat16\n");

	data_write_word(address, data);
}


u8 xa_cpu::rdat8(int address)
{
	return data_read_byte(address);
}

u16 xa_cpu::rdat16(int address)
{
	if (address & 1)
		fatalerror("unaligned in rdat16\n");

	return data_read_word(address);
}



void xa_cpu::write_direct16(u16 addr, u16 data)
{
	if (addr < 0x400)
	{
		if (addr & 1)
			fatalerror("write_direct16 (odd address) %04x %04x\n", addr, data);

		data_write_word(addr, data);
	}
	else
	{
		fatalerror("write_direct16 (SFR) %04x %04x\n", addr, data);
	}
}

void xa_cpu::write_direct8(u16 addr, u8 data)
{
	if (addr < 0x400)
	{
		data_write_byte(addr, data);
	}
	else
	{
		sfr_write_byte(addr - 0x400, data);
	}
}

u16 xa_cpu::read_direct16(u16 addr)
{
	if (addr < 0x400)
	{
		return data_read_word(addr);
	}
	else
	{
		fatalerror("read_direct16 on sfr %03x\n", addr);
		return 0;
	}
	return 0;
}

u8 xa_cpu::read_direct8(u16 addr)
{
	if (addr < 0x400)
	{
		return data_read_byte(addr);
	}
	else
	{
		return sfr_read_byte(addr - 0x400);
	}
	return 0;
}

void xa_cpu::e_illegal(u8 op)
{
	fatalerror( "illegal");
}

void xa_cpu::handle_shift(u8 op, int shift_type)
{
	int size = (op & 0x0c) >> 2;
	const u8 op2 = program_read_byte(m_pc++);
	u8 data, rd;
	if (size == 0x03)
	{
		data = op2 & 0x1f;
		rd = (op2 & 0xe0) >> 4;
	}
	else
	{
		data = op2 & 0x0f;
		rd = (op2 & 0xf0) >> 4;
	}

	// size == 1 has already been filtered out by the time we get here
	switch (shift_type)
	{
	case 0x0:
	{
		if (size == 0) asl_byte_rd_imm4(rd, data);
		else if (size == 2) asl_word_rd_imm4(rd, data);
		else if (size == 3) asl_dword_rd_imm5(rd, data);
		break;
	}
	case 0x1:
	{
		if (size == 0) asr_byte_rd_imm4(rd, data);
		else if (size == 2) asr_word_rd_imm4(rd, data);
		else if (size == 3) asr_dword_rd_imm5(rd, data);
		break;
	}
	case 0x2:
	{
		if (size == 0) lsr_byte_rd_imm4(rd, data);
		else if (size == 2) lsr_word_rd_imm4(rd, data);
		else if (size == 3) lsr_dword_rd_imm5(rd, data);
		break;
	}
	}
}


void xa_cpu::handle_alu_type0(u8 op, int alu_op)
{
	const int size = op & 0x08;
	const u8 op2 = program_read_byte(m_pc++);

	switch (op & 0x07)
	{
	case 0x01:
	{
		const u8 rs = (op2 & 0x0f);
		const u8 rd = (op2 & 0xf0) >> 4;
		if (size) { aluop_word_rd_rs(alu_op, rd, rs); } else { aluop_byte_rd_rs(alu_op, rd, rs); }
		break;
	}

	case 0x02:
	{
		const int optype = op2 & 0x08;
		if (!optype)
		{
			const u8 rs = (op2 & 0x07);
			const u8 rd = (op2 & 0xf0) >> 4;
			if (size) { aluop_word_rd_indrs(alu_op, rd, rs); } else { aluop_byte_rd_indrs(alu_op, rd, rs); }
		}
		else
		{
			const u8 rd = (op2 & 0x07);
			const u8 rs = (op2 & 0xf0) >> 4;
			if (size) { aluop_word_indrd_rs(alu_op, rd, rs); } else { aluop_byte_indrd_rs(alu_op, rd, rs); }
		}
		break;
	}

	case 0x03:
	{
		const int optype = op2 & 0x08;
		if (!optype)
		{
			const u8 rs = (op2 & 0x07);
			const u8 rd = (op2 & 0xf0) >> 4;
			if (size) { aluop_word_rd_indrsinc(alu_op, rd, rs); } else { aluop_byte_rd_indrsinc(alu_op, rd, rs); }
		}
		else
		{
			const u8 rd = (op2 & 0x07);
			const u8 rs = (op2 & 0xf0) >> 4;
			if (size) { aluop_word_indrdinc_rs(alu_op, rd, rs); } else { aluop_byte_indrdinc_rs(alu_op, rd, rs); }
		}
		break;
	}

	case 0x04:
	{
		const int optype = op2 & 0x08;
		const u8 offset8 = program_read_byte(m_pc++);
		if (!optype)
		{
			const u8 rs = (op2 & 0x07);
			const u8 rd = (op2 & 0xf0) >> 4;
			if (size) { aluop_word_rd_rsoff8(alu_op, rd, rs, offset8); } else { aluop_byte_rd_rsoff8(alu_op, rd, rs, offset8); }
		}
		else
		{
			const u8 rd = (op2 & 0x07);
			const u8 rs = (op2 & 0xf0) >> 4;
			if (size) { aluop_word_rdoff8_rs(alu_op, rd, offset8, rs); } else { aluop_byte_rdoff8_rs(alu_op, rd, offset8, rs); }
		}
		break;
	}

	case 0x05:
	{
		const int optype = op2 & 0x08;
		const u8 op3 = program_read_byte(m_pc++);
		const u8 op4 = program_read_byte(m_pc++);
		const int offset16 = (op3 << 8) | op4;
		if (!optype)
		{
			const u8 rs = (op2 & 0x07);
			const u8 rd = (op2 & 0xf0) >> 4;
			if (size) { aluop_word_rsoff16(alu_op, rd, rs, offset16); } else { aluop_byte_rsoff16(alu_op, rd, rs, offset16); }
		}
		else
		{
			const u8 rd = (op2 & 0x07);
			const u8 rs = (op2 & 0xf0) >> 4;
			if (size) { aluop_word_rdoff16_rs(alu_op, rd, offset16, rs); } else { aluop_byte_rdoff16_rs(alu_op, rd, offset16, rs); }
		}
		break;
	}

	case 0x06:
	{
		const int optype = op2 & 0x08;
		const u8 op3 = program_read_byte(m_pc++);
		const u16 direct = ((op2 & 0x07) << 8) | op3;
		if (!optype)
		{
			const u8 rd = (op2 & 0xf0) >> 4;
			if (size) { aluop_word_rd_direct(alu_op, rd, direct); } else { aluop_byte_rd_direct(alu_op, rd, direct); }
		}
		else
		{
			const u8 rs = (op2 & 0xf0) >> 4;
			if (size) { aluop_word_direct_rs(alu_op, direct, rs); } else { aluop_byte_direct_rs(alu_op, direct, rs); }
		}
		break;
	}

	}
}



void xa_cpu::handle_alu_type1(u8 op, u8 op2)
{
	int alu_op = op2 & 0x0f;
	switch (op & 0x0f)
	{
	case 0x01: // ALUOP.b Rd, data8
	{
		const u8 data8 = program_read_byte(m_pc++);
		const u8 rd = (op2 & 0xf0) >> 4;
		aluop_byte_rd_data8(alu_op, rd, data8);
		break;
	}

	case 0x02: // ALUOP.b [Rd], data8
	{
		const u8 data8 = program_read_byte(m_pc++);
		const u8 rd = (op2 & 0xf0) >> 4;
		aluop_byte_indrd_data8(alu_op, rd, data8);
		break;
	}

	case 0x03: // ALUOP.b [Rd+], data8
	{
		const u8 data8 = program_read_byte(m_pc++);
		const u8 rd = (op2 & 0xf0) >> 4;
		aluop_byte_indrdinc_data8(alu_op, rd, data8);
		break;
	}

	case 0x04: // ALUOP.b [Rd+offs8], data8
	{
		const u8 offset8 = program_read_byte(m_pc++);
		const u8 data8 = program_read_byte(m_pc++);
		const u8 rd = (op2 & 0xf0) >> 4;
		aluop_byte_rdoff8_data8(alu_op, rd, offset8, data8);
		break;
	}

	case 0x05: // ALUOP.b [Rd+offs16], data8
	{
		const u8 op3 = program_read_byte(m_pc++);
		const u8 op4 = program_read_byte(m_pc++);
		const u8 data8 = program_read_byte(m_pc++);
		const u8 rd = (op2 & 0xf0) >> 4;
		const u16 offset16 = (op3 << 8) | op4;
		aluop_byte_rdoff16_data8(alu_op, rd, offset16, data8);
		break;
	}

	case 0x06: // ALUOP.b DIRECT, data8
	{
		const u8 op3 = program_read_byte(m_pc++);
		const u8 data8 = program_read_byte(m_pc++);
		const u16 direct = ((op2 & 0xf0) << 4) | op3;
		aluop_byte_direct_data8(alu_op, direct, data8);
		break;
	}

	case 0x09: // ALUOP.w Rd, data16
	{
		const u8 op3 = program_read_byte(m_pc++);
		const u8 op4 = program_read_byte(m_pc++);
		const u8 rd = (op2 & 0xf0) >> 4;
		const u16 data16 = (op3 << 8) | op4;
		aluop_byte_rd_data16(alu_op, rd, data16);
		break;
	}

	case 0x0a: // ALUOP.w [Rd], data16
	{
		const u8 op3 = program_read_byte(m_pc++);
		const u8 op4 = program_read_byte(m_pc++);
		const u8 rd = (op2 & 0xf0) >> 4;
		const u16 data16 = (op3 << 8) | op4;
		aluop_byte_indrd_data16(alu_op, rd, data16);
		break;
	}

	case 0x0b: // ALUOP.w [Rd+], data16
	{
		const u8 op3 = program_read_byte(m_pc++);
		const u8 op4 = program_read_byte(m_pc++);
		const u8 rd = (op2 & 0xf0) >> 4;
		const u16 data16 = (op3 << 8) | op4;
		aluop_byte_indrdinc_data16(alu_op, rd, data16);
		break;
	}

	case 0x0c: // ALPOP.w [Rd+offs8], data16
	{
		const u8 op3 = program_read_byte(m_pc++);
		const u8 op4 = program_read_byte(m_pc++);
		const u8 op5 = program_read_byte(m_pc++);
		const u8 rd = (op2 & 0xf0) >> 4;
		const int offset8 = op3;
		const u16 data16 = (op4 << 8) | op5;
		aluop_byte_rdoff8_data16(alu_op, rd, offset8, data16);
		break;
	}

	case 0x0d: // ALUOP.w [Rd+offs16], data16
	{
		const u8 op3 = program_read_byte(m_pc++);
		const u8 op4 = program_read_byte(m_pc++);
		const u8 op5 = program_read_byte(m_pc++);
		const u8 op6 = program_read_byte(m_pc++);
		const u8 rd = (op2 & 0xf0) >> 4;
		const int offset16= (op3 << 8) | op4;
		const u16 data16 = (op5 << 8) | op6;
		aluop_byte_rdoff16_data16(alu_op, rd, offset16, data16);
		break;
	}

	case 0x0e: // ALUOP.w DIRECT, data16
	{
		const u8 op3 = program_read_byte(m_pc++);
		const u8 op4 = program_read_byte(m_pc++);
		const u8 op5 = program_read_byte(m_pc++);
		const u16 direct =( (op2 & 0xf0) << 4) | op3;
		const u16 data16 = (op4 << 8) | op5;
		aluop_byte_direct_data16(alu_op, direct, data16);
		break;
	}
	}
}

void xa_cpu::handle_adds_movs(u8 op, int which)
{
	const u8 op2 = program_read_byte(m_pc++);
	int size = op & 0x08;
	const u16 data4 = op2 & 0x0f;

	switch (op & 0x07)
	{
	case 0x01:
	{
		int rd = (op2 & 0xf0) >> 4;
		if (which) { if (size) { movs_word_rd_data4(rd, data4); } else { movs_byte_rd_data4(rd, data4); } }
		else       { if (size) { adds_word_rd_data4(rd, data4); } else { adds_byte_rd_data4(rd, data4); } }
		break;
	}
	case 0x02:
	{
		int rd = (op2 & 0x70) >> 4;
		if (which == 1) { if (size) { movs_word_indrd_data4(rd, data4); } else { movs_byte_indrd_data4(rd, data4); } }
		else {            if (size) { adds_word_indrd_data4(rd, data4); } else { adds_byte_indrd_data4(rd, data4); } }
		break;
	}
	case 0x03:
	{
		int rd = (op2 & 0x70) >> 4;
		if (which) { if (size) { movs_word_indrdinc_data4(rd, data4); } else { movs_byte_indrdinc_data4(rd, data4); } }
		else       { if (size) { adds_word_indrdinc_data4(rd, data4); } else { adds_byte_indrdinc_data4(rd, data4); } }
		break;
	}
	case 0x04:
	{
		int rd = (op2 & 0x70) >> 4;
		const u8 off8 = program_read_byte(m_pc++);
		if (which == 1) { if (size) { movs_word_indrdoff8_data4(rd, off8, data4); } else { movs_byte_indrdoff8_data4(rd, off8, data4); } }
		else            { if (size) { adds_word_indrdoff8_data4(rd, off8, data4); } else { adds_byte_indrdoff8_data4(rd, off8, data4); } }
		break;
	}
	case 0x05:
	{
		int rd = (op2 & 0x70) >> 4;
		const u8 op3 = program_read_byte(m_pc++);
		const u8 op4 = program_read_byte(m_pc++);
		const int off16 = (op3 << 8) | op4;
		if (which == 1) { if (size) { movs_word_indrdoff16_data4(rd, off16, data4); } else { movs_byte_indrdoff16_data4(rd, off16, data4); } }
		else            { if (size) { adds_word_indrdoff16_data4(rd, off16, data4); } else { adds_byte_indrdoff16_data4(rd, off16, data4); } }
		break;
	}
	case 0x06:
	{
		const u8 op3 = program_read_byte(m_pc++);
		const u16 direct = ((op2 & 0xf0) << 4) | op3;
		if (which == 1) { if (size) { movs_word_direct_data4(direct, data4); } else { movs_byte_direct_data4(direct, data4); } }
		else            { if (size) { adds_word_direct_data4(direct, data4); } else { adds_byte_direct_data4(direct, data4); } }
		break;
	}
	}
}

void xa_cpu::push_byte_reglist(u8 op2, int h, bool force_user)
{
	for (int i = 7; i >= 0; i--)
	{
		int bit = (op2 & (1 << i));
		if (bit)
		{
			u8 regval = gr8(i + (h ? 8 : 0));
			if (force_user)
				push_byte_to_user_stack(regval);
			else
				push_byte_to_stack(regval);
			cy(2);
		}
	}
}

void xa_cpu::push_word_reglist(u8 op2, int h, bool force_user)
{
	// h is unused for word ops?
	for (int i = 7; i >= 0; i--)
	{
		int bit = (op2 & (1 << i));
		if (bit)
		{
			u16 regval = gr16(i);
			if (force_user)
				push_word_to_user_stack(regval);
			else
				push_word_to_stack(regval);
			cy(2);
		}
	}
}

void xa_cpu::pull_byte_reglist(u8 op2, int h, bool force_user)
{
	for (int i = 0; i < 8; i++)
	{
		int bit = (op2 & (1 << i));
		if (bit)
		{
			u8 stackval;
			if (force_user)
				stackval = pull_byte_from_user_stack();
			else
				stackval = pull_byte_from_stack();

			sr8(i + (h ? 8 : 0), stackval);
			cy(2);
		}
	}
}

void xa_cpu::pull_word_reglist(u8 op2, int h, bool force_user)
{
	// h is unused for word ops?
	for (int i = 0; i < 8; i++)
	{
		int bit = (op2 & (1 << i));
		if (bit)
		{
			u16 stackval;
			if (force_user)
				stackval = pull_word_from_user_stack();
			else
				stackval = pull_word_from_stack();

			sr16(i, stackval);
			cy(2);
		}
	}
}


void xa_cpu::handle_push_rlist(u8 op)
{
	const u8 h = op & 0x40;
	const u8 size = op & 0x08;
	const u8 op2 = program_read_byte(m_pc++);
	if (size) { push_word_rlist(op2, h); } else { push_byte_rlist(op2, h); }
}

void xa_cpu::handle_pushu_rlist(u8 op)
{
	const u8 h = op & 0x40;
	const u8 size = op & 0x08;
	const u8 op2 = program_read_byte(m_pc++);
	if (size) { pushu_word_rlist(op2, h); } else { pushu_byte_rlist(op2, h); }
}

void xa_cpu::handle_pop_rlist(u8 op)
{
	const u8 h = op & 0x40;
	const u8 size = op & 0x08;
	const u8 op2 = program_read_byte(m_pc++);
	if (size) { pop_word_rlist(op2, h); } else { pop_byte_rlist(op2, h); }
}

void xa_cpu::handle_popu_rlist(u8 op)
{
	const u8 h = op & 0x40;
	const u8 size = op & 0x08;
	const u8 op2 = program_read_byte(m_pc++);
	if (size) { popu_word_rlist(op2, h); } else { popu_byte_rlist(op2, h); }
}

// -------------------------------------- Group 0 --------------------------------------

/*
NOP                         No operation                                                            1 3         0000 0000
*/
void xa_cpu::e_nop(u8 op)
{
	do_nop();
}

/*
CLR bit                     Clear bit                                                               3 4         0000 1000  0000 00bb  bbbb bbbb
SETB bit                    Sets the bit specified                                                  3 4         0000 1000  0001 00bb  bbbb bbbb
MOV C, bit                  Move bit to the carry flag                                              3 4         0000 1000  0010 00bb  bbbb bbbb
MOV bit, C                  Move carry to bit                                                       3 4         0000 1000  0011 00bb  bbbb bbbb
ANL C, bit                  Logical AND bit to carry                                                3 4         0000 1000  0100 00bb  bbbb bbbb
ANL C, /bit                 Logical AND complement of a bit to carry                                3 4         0000 1000  0101 00bb  bbbb bbbb
ORL C, bit                  Logical OR a bit to carry                                               3 4         0000 1000  0110 00bb  bbbb bbbb
ORL C, /bit                 Logical OR complement of a bit to carry                                 3 4         0000 1000  0111 00bb  bbbb bbbb
*/

void xa_cpu::e_bitgroup(u8 op)
{
	const u8 op2 = program_read_byte(m_pc++);
	const u8 op3 = program_read_byte(m_pc++);

	u16 bit = ((op2 & 0x03) << 8) | op3;

	switch (op2 & 0xf0)
	{
	case 0x00: clr_bit(bit); break;
	case 0x10: setb_bit(bit); break;
	case 0x20: mov_c_bit(bit);  break;
	case 0x30: mov_bit_c(bit); break;
	case 0x40: anl_c_bit(bit); break;
	case 0x50: anl_c_notbit(bit); break;
	case 0x60: orl_c_bit(bit); break;
	case 0x70: orl_c_notbit(bit); break;
	default:   LOGMASKED(LOG_DEBUG,  "illegal bit op %s", get_bittext(bit) ); do_nop(); break;
	}
	return;
}

/*
ADD Rd, Rs                  Add regs direct                                                         2 3         0000 S001  dddd ssss
ADD Rd, [Rs]                Add reg-ind to reg                                                      2 4         0000 S010  dddd 0sss
ADD [Rd], Rs                Add reg to reg-ind                                                      2 4         0000 S010  ssss 1ddd
ADD Rd, [Rs+]               Add reg-ind w/ autoinc to reg                                           2 5         0000 S011  dddd 0sss
ADD [Rd+], Rs               Add reg-ind w/ autoinc to reg                                           2 5         0000 S011  ssss 1ddd
ADD Rd, [Rs+offset8]        Add reg-ind w/ 8-bit offs to reg                                        3 6         0000 S100  dddd 0sss  oooo oooo
ADD [Rd+offset8], Rs        Add reg to reg-ind w/ 8-bit offs                                        3 6         0000 S100  ssss 1ddd  oooo oooo
ADD Rd, [Rs+offset16]       Add reg-ind w/ 16-bit offs to reg                                       4 6         0000 S101  dddd 0sss  oooo oooo  oooo oooo
ADD [Rd+offset16], Rs       Add reg to reg-ind w/ 16-bit offs                                       4 6         0000 S101  ssss 1ddd  oooo oooo  oooo oooo
ADD direct, Rs              Add reg to mem                                                          3 4         0000 S110  ssss 1DDD  DDDD DDDD
ADD Rd, direct              Add mem to reg                                                          3 4         0000 S110  dddd 0DDD  DDDD DDDD
*/
void xa_cpu::e_add(u8 op)
{
	handle_alu_type0(op, 0);
}

/*
PUSH Rlist                  Push regs (b/w) onto the current stack                                  2 b*        0H00 S111  LLLL LLLL
*/
void xa_cpu::e_push_rlist(u8 op)
{
	// PUSH
	handle_push_rlist(op);
}

// -------------------------------------- Group 1 --------------------------------------

/*
ADDC Rd, Rs                 Add regs direct w/ carry                                                2 3         0001 S001  dddd ssss
ADDC Rd, [Rs]               Add reg-ind to reg w/ carry                                             2 4         0001 S010  dddd 0sss
ADDC [Rd], Rs               Add reg to reg-ind w/ carry                                             2 4         0001 S010  ssss 1ddd
ADDC Rd, [Rs+offset8]       Add reg-ind w/ 8-bit offs to reg w/ carry                               3 6         0001 S100  dddd 0sss  oooo oooo
ADDC [Rd+offset8], Rs       Add reg to reg-ind w/ 8-bit offs w/ carry                               3 6         0001 S100  ssss 1ddd  oooo oooo
ADDC Rd, [Rs+offset16]      Add reg-ind w/ 16-bit offs to reg w/ carry                              4 6         0001 S101  dddd 0sss  oooo oooo  oooo oooo
ADDC [Rd+offset16], Rs      Add reg to reg-ind w/ 16-bit offs w/ carry                              4 6         0001 S101  ssss 1ddd  oooo oooo  oooo oooo
ADDC Rd, [Rs+]              Add reg-ind w/ autoinc to reg w/ carry                                  2 5         0001 S011  dddd 0sss
ADDC [Rd+], Rs              Add reg-ind w/ autoinc to reg w/ carry                                  2 5         0001 S011  ssss 1ddd
ADDC direct, Rs             Add reg to mem w/ carry                                                 3 4         0001 S110  ssss 1DDD  DDDD DDDD
ADDC Rd, direct             Add mem to reg w/ carry                                                 3 4         0001 S110  dddd 0DDD  DDDD DDDD
*/

void xa_cpu::e_addc(u8 op)
{
	handle_alu_type0(op, 1);
}

/*
PUSHU Rlist                 Push regs (b/w) from the user stack                                     2 b*        0H01 S111  LLLL LLLL
*/
void xa_cpu::e_pushu_rlist(u8 op)
{
	// PUSHU
	handle_pushu_rlist(op);
}


// -------------------------------------- Group 2 --------------------------------------

/*
SUB Rd, Rs                  Subtract regs direct                                                    2 3         0010 S001  dddd ssss
SUB Rd, [Rs]                Subtract reg-ind to reg                                                 2 4         0010 S010  dddd 0sss
SUB [Rd], Rs                Subtract reg to reg-ind                                                 2 4         0010 S010  ssss 1ddd
SUB Rd, [Rs+offset8]        Subtract reg-ind w/ 8-bit offs to reg                                   3 6         0010 S100  dddd 0sss  oooo oooo
SUB [Rd+offset8], Rs        Subtract reg to reg-ind w/ 8-bit offs                                   3 6         0010 S100  ssss 1ddd  oooo oooo
SUB Rd, [Rs+offset16]       Subtract reg-ind w/ 16-bit offs to reg                                  4 6         0010 S101  dddd 0sss  oooo oooo  oooo oooo
SUB [Rd+offset16], Rs       Subtract reg to reg-ind w/ 16-bit offs                                  4 6         0010 S101  ssss 1ddd  oooo oooo  oooo oooo
SUB Rd, [Rs+]               Subtract reg-ind w/ autoinc to reg                                      2 5         0010 S011  dddd 0sss
SUB [Rd+], Rs               Subtract reg-ind w/ autoinc to reg                                      2 5         0010 S011  ssss 1ddd
SUB direct, Rs              Subtract reg to mem                                                     3 4         0010 S110  ssss 1DDD  DDDD DDDD
SUB Rd, direct              Subtract mem to reg                                                     3 4         0010 S110  dddd 0DDD  DDDD DDDD
*/
void xa_cpu::e_sub(u8 op)
{
	handle_alu_type0(op, 2);
}

/*
POP Rlist                   Pop regs (b/w) from the current stack                                   2 c*        0H10 S111  LLLL LLLL
*/
void xa_cpu::e_pop_rlist(u8 op)
{
	// POP
	handle_pop_rlist(op);
}


// -------------------------------------- Group 3 --------------------------------------

/*
SUBB Rd, Rs                 Subtract w/ borrow regs direct                                          2 3         0011 S001  dddd ssss
SUBB Rd, [Rs]               Subtract w/ borrow reg-ind to reg                                       2 4         0011 S010  dddd 0sss
SUBB [Rd], Rs               Subtract w/ borrow reg to reg-ind                                       2 4         0011 S010  ssss 1ddd
SUBB Rd, [Rs+]              Subtract w/ borrow reg-ind w/ autoinc to reg                            2 5         0011 S011  dddd 0sss
SUBB [Rd+], Rs              Subtract w/ borrow reg-ind w/ autoinc to reg                            2 5         0011 S011  ssss 1ddd
SUBB Rd, [Rs+offset8]       Subtract w/ borrow reg-ind w/ 8-bit offs to reg                         3 6         0011 S100  dddd 0sss  oooo oooo
SUBB [Rd+offset8], Rs       Subtract w/ borrow reg to reg-ind w/ 8-bit offs                         3 6         0011 S100  ssss 1ddd  oooo oooo
SUBB Rd, [Rs+offset16]      Subtract w/ borrow reg-ind w/ 16-bit offs to reg                        4 6         0011 S101  dddd 0sss  oooo oooo  oooo oooo
SUBB [Rd+offset16], Rs      Subtract w/ borrow reg to reg-ind w/ 16-bit offs                        4 6         0011 S101  ssss 1ddd  oooo oooo  oooo oooo
SUBB direct, Rs             Subtract w/ borrow reg to mem                                           3 4         0011 S110  ssss 1DDD  DDDD DDDD
SUBB Rd, direct             Subtract w/ borrow mem to reg                                           3 4         0011 S110  dddd 0DDD  DDDD DDDD
*/
void xa_cpu::e_subb(u8 op)
{
	handle_alu_type0(op, 3);
}

/*
POPU Rlist                  Pop regs (b/w) from the user stack                                      2 c*        0H11 S111  LLLL LLLL
*/
void xa_cpu::e_popu_rlist(u8 op)
{
	// POPU
	handle_popu_rlist(op);
}


// -------------------------------------- Group 4 --------------------------------------

/*
LEA Rd, Rs+offset8          Load 16-bit effective address w/ 8-bit offs to reg                      3 3         0100 0000  0ddd 0sss  oooo oooo
*/
void xa_cpu::e_lea_offset8(u8 op)
{
	const u8 op2 = program_read_byte(m_pc++);
	const u8 offs8 = program_read_byte(m_pc++);
	const u8 rd = (op2 & 0x70) >> 4;
	const u8 rs = (op2 & 0x07);
	lea_word_rd_rs_off8(rd, rs, offs8);
}

/*
LEA Rd, Rs+offset16         Load 16-bit effective address w/ 16-bit offs to reg                     4 3         0100 1000  0ddd 0sss  oooo oooo  oooo oooo
*/
void xa_cpu::e_lea_offset16(u8 op)
{
	const u8 op2 = program_read_byte(m_pc++);
	const u8 op3 = program_read_byte(m_pc++);
	const u8 op4 = program_read_byte(m_pc++);
	const u8 rd = (op2 & 0x70) >> 4;
	const u8 rs = (op2 & 0x07);
	const u16 offs16 = (op3 << 8) | op4;
	lea_word_rd_rs_off16(rd, rs, offs16);
}

/*
CMP Rd, Rs                  Compare dest and src regs                                               2 3         0100 S001  dddd ssss
CMP Rd, [Rs]                Compare reg-ind w/ reg                                                  2 4         0100 S010  dddd 0sss
CMP [Rd], Rs                Compare reg w/ reg-ind                                                  2 4         0100 S010  ssss 1ddd
CMP Rd, [Rs+offset8]        Compare reg-ind w/ 8-bit offs w/ reg                                    3 6         0100 S100  dddd 0sss  oooo oooo
CMP [Rd+offset8], Rs        Compare reg w/ reg-ind w/ 8-bit offs                                    3 6         0100 S100  ssss 1ddd  oooo oooo
CMP Rd, [Rs+offset16]       Compare reg-ind w/ 16-bit offs w/ reg                                   4 6         0100 S101  dddd 0sss  oooo oooo  oooo oooo
CMP [Rd+offset16], Rs       Compare reg w/ reg-ind w/ 16-bit offs                                   4 6         0100 S101  ssss 1ddd  oooo oooo  oooo oooo
CMP Rd, [Rs+]               Compare autoinc reg-ind w/ reg                                          2 5         0100 S011  dddd 0sss
CMP [Rd+], Rs               Compare reg w/ autoinc reg-ind                                          2 5         0100 S011  ssss 1ddd
CMP direct, Rs              Compare reg w/ mem                                                      3 4         0100 S110  ssss 1DDD  DDDD DDDD
CMP Rd, direct              Compare mem w/ reg                                                      3 4         0100 S110  dddd 0DDD  DDDD DDDD
*/
void xa_cpu::e_cmp(u8 op)
{
	handle_alu_type0(op, 4);
}

// -------------------------------------- Group 5 --------------------------------------

/*
XCH Rd, [Rs]                Exchange contents of a reg-ind address w/ a reg                         2 6         0101 S000  dddd 0sss
*/
void xa_cpu::e_xch_type1(u8 op)
{
	const u8 op2 = program_read_byte(m_pc++);
	int size = op & 0x08;
	const u8 rd = (op2 & 0xf0) >> 4;
	const u8 rs = (op2 & 0x07);

	if (size) { xch_word_rd_indrs(rd, rs); } else { xch_byte_rd_indrs(rd, rs); }
}

/*
AND Rd, Rs                  Logical AND regs direct                                                 2 3         0101 S001  dddd ssss
AND Rd, [Rs]                Logical AND reg-ind to reg                                              2 4         0101 S010  dddd 0sss
AND [Rd], Rs                Logical AND reg to reg-ind                                              2 4         0101 S010  ssss 1ddd
AND Rd, [Rs+offset8]        Logical AND reg-ind w/ 8-bit offs to reg                                3 6         0101 S100  dddd 0sss  oooo oooo
AND [Rd+offset8], Rs        Logical AND reg to reg-ind w/ 8-bit offs                                3 6         0101 S100  ssss 1ddd  oooo oooo
AND Rd, [Rs+offset16]       Logical AND reg-ind w/ 16-bit offs to reg                               4 6         0101 S101  dddd 0sss  oooo oooo  oooo oooo
AND [Rd+offset16], Rs       Logical AND reg to reg-ind w/ 16-bit offs                               4 6         0101 S101  ssss 1ddd  oooo oooo  oooo oooo
AND Rd, [Rs+]               Logical AND reg-ind w/ autoinc to reg                                   2 5         0101 S011  dddd 0sss
AND [Rd+], Rs               Logical AND reg-ind w/ autoinc to reg                                   2 5         0101 S011  ssss 1ddd
AND direct, Rs              Logical AND reg to mem                                                  3 4         0101 S110  ssss 1DDD  DDDD DDDD
AND Rd, direct              Logical AND mem to reg                                                  3 4         0101 S110  dddd 0DDD  DDDD DDDD
*/
void xa_cpu::e_and(u8 op)
{
	handle_alu_type0(op, 5);
}

// -------------------------------------- Group 6 --------------------------------------

/*
XCH Rd, Rs                  Exchange contents of two regs                                           2 5         0110 S000  dddd ssss
*/
void xa_cpu::e_xch_type2(u8 op)
{
	const u8 op2 = program_read_byte(m_pc++);
	int size = op & 0x08;
	const u8 rd = (op2 & 0xf0) >> 4;
	const u8 rs = (op2 & 0x0f);
	if (size) { xch_word_rd_rs(rd, rs); } else { xch_byte_rd_rs(rd, rs); }
}

/*
OR Rd, Rs                   Logical OR regs                                                         2 3         0110 S001  dddd ssss
OR Rd, [Rs]                 Logical OR reg-ind to reg                                               2 4         0110 S010  dddd 0sss
OR [Rd], Rs                 Logical OR reg to reg-ind                                               2 4         0110 S010  ssss 1ddd
OR Rd, [Rs+offset8]         Logical OR reg-ind w/ 8-bit offs to reg                                 3 6         0110 S100  dddd 0sss  oooo oooo
OR [Rd+offset8], Rs         Logical OR reg to reg-ind w/ 8-bit offs                                 3 6         0110 S100  ssss 1ddd  oooo oooo
OR Rd, [Rs+offset16]        Logical OR reg-ind w/ 16-bit offs to reg                                4 6         0110 S101  dddd 0sss  oooo oooo  oooo oooo
OR [Rd+offset16], Rs        Logical OR reg to reg-ind w/ 16-bit offs                                4 6         0110 S101  ssss 1ddd  oooo oooo  oooo oooo
OR Rd, [Rs+]                Logical OR reg-ind w/ autoinc to reg                                    2 5         0110 S011  dddd 0sss
OR [Rd+], Rs                Logical OR reg-ind w/ autoinc to reg                                    2 5         0110 S011  ssss 1ddd
OR direct, Rs               Logical OR reg to mem                                                   3 4         0110 S110  ssss 1DDD  DDDD DDDD
OR Rd, direct               Logical OR mem to reg                                                   3 4         0110 S110  dddd 0DDD  DDDD DDDD
*/
void xa_cpu::e_or(u8 op)
{
	handle_alu_type0(op, 6);
}

// -------------------------------------- Group 7 --------------------------------------

/*
XOR Rd, Rs                  Logical XOR regs                                                        2 3         0111 S001  dddd ssss
XOR Rd, [Rs]                Logical XOR reg-ind to reg                                              2 4         0111 S010  dddd 0sss
XOR [Rd], Rs                Logical XOR reg to reg-ind                                              2 4         0111 S010  ssss 1ddd
XOR Rd, [Rs+offset8]        Logical XOR reg-ind w/ 8-bit offs to reg                                3 6         0111 S100  dddd 0sss  oooo oooo
XOR [Rd+offset8], Rs        Logical XOR reg to reg-ind w/ 8-bit offs                                3 6         0111 S100  ssss 1ddd  oooo oooo
XOR Rd, [Rs+offset16]       Logical XOR reg-ind w/ 16-bit offs to reg                               4 6         0111 S101  dddd 0sss  oooo oooo  oooo oooo
XOR [Rd+offset16], Rs       Logical XOR reg to reg-ind w/ 16-bit offs                               4 6         0111 S101  ssss 1ddd  oooo oooo  oooo oooo
XOR Rd, [Rs+]               Logical XOR reg-ind w/ autoinc to reg                                   2 5         0111 S011  dddd 0sss
XOR [Rd+], Rs               Logical XOR reg-ind w/ autoinc to reg                                   2 5         0111 S011  ssss 1ddd
XOR direct, Rs              Logical XOR reg to mem                                                  3 4         0111 S110  ssss 1DDD  DDDD DDDD
XOR Rd, direct              Logical XOR mem to reg                                                  3 4         0111 S110  dddd 0DDD  DDDD DDDD
*/
void xa_cpu::e_xor(u8 op)
{
	handle_alu_type0(op, 7);
}

// -------------------------------------- Group 8 --------------------------------------

/*
MOVC Rd, [Rs+]              Move data from WS:Rs address of code mem to reg w/ autoinc              2 4         1000 S000  dddd 0sss
*/
void xa_cpu::e_movc_rd_rsinc(u8 op)
{
	const u8 op2 = program_read_byte(m_pc++);
	int size = op & 0x08;
	int rd = (op2 & 0xf0) >> 4;
	int rs = (op2 & 0x07);
	if (size) { movc_word_rd_indrsinc(rd, rs); } else { movc_byte_rd_indrsinc(rd, rs); }
}

/*
MOV Rd, Rs                  Move reg to reg                                                         2 3         1000 S001  dddd ssss
MOV Rd, [Rs]                Move reg-ind to reg                                                     2 3         1000 S010  dddd 0sss
MOV [Rd], Rs                Move reg to reg-ind                                                     2 3         1000 S010  ssss 1ddd
MOV Rd, [Rs+offset8]        Move reg-ind w/ 8-bit offs to reg                                       3 5         1000 S100  dddd 0sss  oooo oooo
MOV [Rd+offset8], Rs        Move reg to reg-ind w/ 8-bit offs                                       3 5         1000 S100  ssss 1ddd  oooo oooo
MOV Rd, [Rs+offset16]       Move reg-ind w/ 16-bit offs to reg                                      4 5         1000 S101  dddd 0sss  oooo oooo  oooo oooo
MOV [Rd+offset16], Rs       Move reg to reg-ind w/ 16-bit offs                                      4 5         1000 S101  ssss 1ddd  oooo oooo  oooo oooo
MOV Rd, [Rs+]               Move reg-ind w/ autoinc to reg                                          2 4         1000 S011  dddd 0sss
MOV [Rd+], Rs               Move reg-ind w/ autoinc to reg                                          2 4         1000 S011  ssss 1ddd
MOV direct, Rs              Move reg to mem                                                         3 4         1000 S110  ssss 1DDD  DDDD DDDD
MOV Rd, direct              Move mem to reg                                                         3 4         1000 S110  dddd 0DDD  DDDD DDDD
*/
void xa_cpu::e_mov(u8 op)
{
	handle_alu_type0(op, 8);
}

/*
POPU direct                 Pop the mem content (b/w) from the user stack                           3 5         1000 S111  0000 0DDD  DDDD DDDD
POP direct                  Pop the mem content (b/w) from the current stack                        3 5         1000 S111  0001 0DDD  DDDD DDDD
PUSHU direct                Push the mem content (b/w) onto the user stack                          3 5         1000 S111  0010 0DDD  DDDD DDDD
PUSH direct                 Push the mem content (b/w) onto the current stack                       3 5         1000 S111  0011 0DDD  DDDD DDDD
DJNZ Rd,rel8                Decrement reg and jump if not zero                                      3 8t/5nt    1000 S111  dddd 1000  rrrr rrrr
*/
void xa_cpu::e_pushpop_djnz_subgroup(u8 op)
{
	const u8 op2 = program_read_byte(m_pc++);
	int size = op & 0x08;
	if (op2 & 0x08)
	{
		const u8 rel8 = program_read_byte(m_pc++);
		int rd = (op2 & 0xf0) >> 4;
		if (size) { djnz_word_rd_rel8(rd, rel8); } else { djnz_byte_rd_rel8(rd, rel8); }
	}
	else
	{
		const u8 op3 = program_read_byte(m_pc++);
		const u16 direct = ((op2 & 0x07) << 8) | op3;
		switch (op2 & 0xf0)
		{
		case 0x00: { if (size) { popu_word_direct(direct); } else { popu_byte_direct(direct); } break; }
		case 0x10: { if (size) { pop_word_direct(direct); } else { pop_byte_direct(direct); } break; }
		case 0x20: { if (size) { pushu_word_direct(direct); } else { pushu_byte_direct(direct); } break; }
		case 0x30: { if (size) { push_word_direct(direct); } else { push_byte_direct(direct); } break; }
		default: LOGMASKED(LOG_DEBUG, "illegal push/pop"); do_nop(); break;
		}
	}
}



// -------------------------------------- Group 9 --------------------------------------

/*
MOV [Rd+], [Rs+]            Move reg-ind to reg-ind, both pointers autoinc                          2 6         1001 S000  0ddd 0sss
DA Rd                       Decimal Adjust byte reg                                                 2 4         1001 0000  dddd 1000
SEXT Rd                     Sign extend last operation to reg                                       2 3         1001 S000  dddd 1001
CPL Rd                      Complement (ones complement) reg                                        2 3         1001 S000  dddd 1010
NEG Rd                      Negate (twos complement) reg                                            2 3         1001 S000  dddd 1011
MOVC A, [A+PC]              Move data from code mem to the accumulator ind w/ PC                    2 6         1001 0000  0100 1100
MOVC A, [A+DPTR]            Move data from code mem to the accumulator ind w/ DPTR                  2 6         1001 0000  0100 1110
MOV Rd, USP                 Move User Stack Pointer to reg (system mode only)                       2 3         1001 0000  dddd 1111
MOV USP, Rs                 Move reg to User Stack Pointer (system mode only)                       2 3         1001 1000  ssss 1111
*/
void xa_cpu::e_g9_subgroup(u8 op)
{
	const u8 op2 = program_read_byte(m_pc++);
	int size = op & 0x08;
	if ((op2 & 0x0f) < 0x08)
	{
		int rd = (op2 & 0x70) >> 4;
		int rs = (op2 & 0x07);
		if (size) { mov_word_indrdinc_indrsinc(rd, rs); } else { mov_byte_indrdinc_indrsinc(rd, rs); }
	}
	else
	{
		switch (op2 & 0x0f)
		{
		case 0x08: { int rd = (op2 & 0xf0) >> 4; da_rd(rd); break;  }
		case 0x09: { int rd = (op2 & 0xf0) >> 4; if (size) { sext_word_rd(rd); } else { sext_byte_rd(rd); } break; }
		case 0x0a: { int rd = (op2 & 0xf0) >> 4; if (size) { cpl_word_rd(rd); } else { cpl_byte_rd(rd); } break; }
		case 0x0b: { int rd = (op2 & 0xf0) >> 4; if (size) { neg_word_rd(rd); } else { neg_byte_rd(rd); } break; }
		case 0x0c: { movc_a_apc(); break; }
		case 0x0e: { movc_a_adptr(); break; }
		case 0x0f: { int reg = (op2 & 0xf0) >> 4; if (!size)    { mov_rd_usp(reg); } else { mov_usp_rs(reg); } break; }
		default: { LOGMASKED(LOG_DEBUG, "illegal %02x", op2); do_nop(); break; }
		}
	}
}

/*
ADD Rd, #data8              Add 8-bit imm data to reg                                               3 3         1001 0001  dddd 0000  iiii iiii
ADD [Rd], #data8            Add 8-bit imm data to reg-ind                                           3 4         1001 0010  0ddd 0000  iiii iiii
ADD [Rd+], #data8           Add 8-bit imm data to reg-ind w/ autoinc                                3 5         1001 0011  0ddd 0000  iiii iiii
ADD [Rd+offset8], #data8    Add 8-bit imm data to reg-ind w/ 8-bit offs                             4 6         1001 0100  0ddd 0000  oooo oooo  iiii iiii
ADD [Rd+offset16], #data8   Add 8-bit imm data to reg-ind w/ 16-bit offs                            5 6         1001 0101  0ddd 0000  oooo oooo  oooo oooo  iiii iiii
ADD direct, #data8          Add 8-bit imm data to mem                                               4 4         1001 0110  0DDD 0000  DDDD DDDD  iiii iiii
ADD Rd, #data16             Add 16-bit imm data to reg                                              4 3         1001 1001  dddd 0000  iiii iiii  iiii iiii
ADD [Rd], #data16           Add 16-bit imm data to reg-ind                                          4 4         1001 1010  0ddd 0000  iiii iiii  iiii iiii
ADD [Rd+], #data16          Add 16-bit imm data to reg-ind w/ autoinc                               4 5         1001 1011  0ddd 0000  iiii iiii  iiii iiii
ADD [Rd+offset8], #data16   Add 16-bit imm data to reg-ind w/ 8-bit offs                            5 6         1001 1100  0ddd 0000  oooo oooo  iiii iiii  iiii iiii
ADD [Rd+offset16], #data16  Add 16-bit imm data to reg-ind w/ 16-bit offs                           6 6         1001 1101  0ddd 0000  oooo oooo  oooo oooo  iiii iiii  iiii iiii
ADD direct, #data16         Add 16-bit imm data to mem                                              5 4         1001 1110  0DDD 0000  DDDD DDDD  iiii iiii  iiii iiii

ADDC Rd, #data8             Add 8-bit imm data to reg w/ carry                                      3 3         1001 0001  dddd 0001  iiii iiii
ADDC Rd, #data16            Add 16-bit imm data to reg w/ carry                                     4 3         1001 1001  dddd 0001  iiii iiii  iiii iiii
ADDC [Rd], #data8           Add 16-bit imm data to reg-ind w/ carry                                 3 4         1001 0010  0ddd 0001  iiii iiii
ADDC [Rd], #data16          Add 16-bit imm data to reg-ind w/ carry                                 4 4         1001 1010  0ddd 0001  iiii iiii  iiii iiii
ADDC [Rd+], #data8          Add 8-bit imm data to reg-ind and autoinc w/ carry                      3 5         1001 0011  0ddd 0001  iiii iiii
ADDC [Rd+], #data16         Add 16-bit imm data to reg-ind and autoinc w/ carry                     4 5         1001 1011  0ddd 0001  iiii iiii  iiii iiii
ADDC [Rd+offset8], #data8   Add 8-bit imm data to reg-ind w/ 8-bit offs and carry                   4 6         1001 0100  0ddd 0001  oooo oooo  iiii iiii
ADDC [Rd+offset8], #data16  Add 16-bit imm data to reg-ind w/ 8-bit offs and carry                  5 6         1001 1100  0ddd 0001  oooo oooo  iiii iiii  iiii iiii
ADDC [Rd+offset16], #data8  Add 8-bit imm data to reg-ind w/ 16-bit offs and carry                  5 6         1001 0101  0ddd 0001  oooo oooo  oooo oooo  iiii iiii
ADDC [Rd+offset16], #data16 Add 16-bit imm data to reg-ind w/ 16-bit offs and carry                 6 6         1001 1101  0ddd 0001  oooo oooo  oooo oooo  iiii iiii  iiii iiii
ADDC direct, #data8         Add 8-bit imm data to mem w/ carry                                      4 4         1001 0110  0DDD 0001  DDDD DDDD  iiii iiii
ADDC direct, #data16        Add 16-bit imm data to mem w/ carry                                     5 4         1001 1110  0DDD 0001  DDDD DDDD  iiii iiii  iiii iiii

SUB Rd, #data8              Subtract 8-bit imm data to reg                                          3 3         1001 0001  dddd 0010  iiii iiii
SUB Rd, #data16             Subtract 16-bit imm data to reg                                         4 3         1001 1001  dddd 0010  iiii iiii  iiii iiii
SUB [Rd], #data8            Subtract 8-bit imm data to reg-ind                                      3 4         1001 0010  0ddd 0010  iiii iiii
SUB [Rd], #data16           Subtract 16-bit imm data to reg-ind                                     4 4         1001 1010  0ddd 0010  iiii iiii  iiii iiii
SUB [Rd+], #data8           Subtract 8-bit imm data to reg-ind w/ autoinc                           3 5         1001 0011  0ddd 0010  iiii iiii
SUB [Rd+], #data16          Subtract 16-bit imm data to reg-ind w/ autoinc                          4 5         1001 1011  0ddd 0010  iiii iiii  iiii iiii
SUB [Rd+offset8], #data8    Subtract 8-bit imm data to reg-ind w/ 8-bit offs                        4 6         1001 0100  0ddd 0010  oooo oooo  iiii iiii
SUB [Rd+offset8], #data16   Subtract 16-bit imm data to reg-ind w/ 8-bit offs                       5 6         1001 1100  0ddd 0010  oooo oooo  iiii iiii  iiii iiii
SUB [Rd+offset16], #data8   Subtract 8-bit imm data to reg-ind w/ 16-bit offs                       5 6         1001 0101  0ddd 0010  oooo oooo  oooo oooo  iiii iiii
SUB [Rd+offset16], #data16  Subtract 16-bit imm data to reg-ind w/ 16-bit offs                      6 6         1001 1101  0ddd 0010  oooo oooo  oooo oooo  iiii iiii  iiii iiii
SUB direct, #data8          Subtract 8-bit imm data to mem                                          4 4         1001 0110  0DDD 0010  DDDD DDDD  iiii iiii
SUB direct, #data16         Subtract 16-bit imm data to mem                                         5 4         1001 1110  0DDD 0010  DDDD DDDD  iiii iiii  iiii iiii

SUBB Rd, #data8             Subtract w/ borrow 8-bit imm data to reg                                3 3         1001 0001  dddd 0011  iiii iiii
SUBB Rd, #data16            Subtract w/ borrow 16-bit imm data to reg                               4 3         1001 1001  dddd 0011  iiii iiii  iiii iiii
SUBB [Rd], #data8           Subtract w/ borrow 8-bit imm data to reg-ind                            3 4         1001 0010  0ddd 0011  iiii iiii
SUBB [Rd], #data16          Subtract w/ borrow 16-bit imm data to reg-ind                           4 4         1001 1010  0ddd 0011  iiii iiii  iiii iiii
SUBB [Rd+], #data8          Subtract w/ borrow 8-bit imm data to reg-ind w/ autoinc                 3 5         1001 0011  0ddd 0011  iiii iiii
SUBB [Rd+], #data16         Subtract w/ borrow 16-bit imm data to reg-ind w/ autoinc                4 5         1001 1011  0ddd 0011  iiii iiii  iiii iiii
SUBB [Rd+offset8], #data8   Subtract w/ borrow 8-bit imm data to reg-ind w/ 8-bit offs              4 6         1001 0100  0ddd 0011  oooo oooo  iiii iiii
SUBB [Rd+offset8], #data16  Subtract w/ borrow 16-bit imm data to reg-ind w/ 8-bit offs             5 6         1001 1100  0ddd 0011  oooo oooo  iiii iiii  iiii iiii
SUBB [Rd+offset16], #data8  Subtract w/ borrow 8-bit imm data to reg-ind w/ 16-bit offs             5 6         1001 0101  0ddd 0011  oooo oooo  oooo oooo  iiii iiii
SUBB [Rd+offset16], #data16 Subtract w/ borrow 16-bit imm data to reg-ind w/ 16-bit offs            6 6         1001 1101  0ddd 0011  oooo oooo  oooo oooo  iiii iiii  iiii iiii
SUBB direct, #data8         Subtract w/ borrow 8-bit imm data to mem                                4 4         1001 0110  0DDD 0011  DDDD DDDD  iiii iiii
SUBB direct, #data16        Subtract w/ borrow 16-bit imm data to mem                               5 4         1001 1110  0DDD 0011  DDDD DDDD  iiii iiii  iiii iiii

CMP Rd, #data8              Compare 8-bit imm data to reg                                           3 3         1001 0001  dddd 0100  iiii iiii
CMP Rd, #data16             Compare 16-bit imm data to reg                                          4 3         1001 1001  dddd 0100  iiii iiii  iiii iiii
CMP [Rd], #data8            Compare 8-bit imm data to reg-ind                                       3 4         1001 0010  0ddd 0100  iiii iiii
CMP [Rd], #data16           Compare 16-bit imm data to reg-ind                                      4 4         1001 1010  0ddd 0100  iiii iiii  iiii iiii
CMP [Rd+], #data8           Compare 8-bit imm data to reg-ind w/ autoinc                            3 5         1001 0011  0ddd 0100  iiii iiii
CMP [Rd+], #data16          Compare 16-bit imm data to reg-ind w/ autoinc                           4 5         1001 1011  0ddd 0100  iiii iiii  iiii iiii
CMP [Rd+offset8], #data8    Compare 8-bit imm data to reg-ind w/ 8-bit offs                         4 6         1001 0100  0ddd 0100  oooo oooo  iiii iiii
CMP [Rd+offset8], #data16   Compare 16-bit imm data to reg-ind w/ 8-bit offs                        5 6         1001 1100  0ddd 0100  oooo oooo  iiii iiii  iiii iiii
CMP [Rd+offset16], #data8   Compare 8-bit imm data to reg-ind w/ 16-bit offs                        5 6         1001 0101  0ddd 0100  oooo oooo  oooo oooo  iiii iiii
CMP [Rd+offset16], #data16  Compare 16-bit imm data to reg-ind w/ 16-bit offs                       6 6         1001 1101  0ddd 0100  oooo oooo  oooo oooo  iiii iiii  iiii iiii
CMP direct, #data8          Compare 8-bit imm data to mem                                           4 4         1001 0110  0DDD 0100  DDDD DDDD  iiii iiii
CMP direct, #data16         Compare 16-bit imm data to mem                                          5 4         1001 1110  0DDD 0100  DDDD DDDD  iiii iiii  iiii iiii

AND Rd, #data8              Logical AND 8-bit imm data to reg                                       3 3         1001 0001  dddd 0101  iiii iiii
AND Rd, #data16             Logical AND 16-bit imm data to reg                                      4 3         1001 1001  dddd 0101  iiii iiii  iiii iiii
AND [Rd], #data8            Logical AND 8-bit imm data to reg-ind                                   3 4         1001 0010  0ddd 0101  iiii iiii
AND [Rd], #data16           Logical AND 16-bit imm data to reg-ind                                  4 4         1001 1010  0ddd 0101  iiii iiii  iiii iiii
AND [Rd+], #data8           Logical AND 8-bit imm data to reg-ind and autoinc                       3 5         1001 0011  0ddd 0101  iiii iiii
AND [Rd+], #data16          Logical AND 16-bit imm data to reg-ind and autoinc                      4 5         1001 1011  0ddd 0101  iiii iiii  iiii iiii
AND [Rd+offset8], #data8    Logical AND 8-bit imm data to reg-ind w/ 8-bit offs                     4 6         1001 0100  0ddd 0101  oooo oooo  iiii iiii
AND [Rd+offset8], #data16   Logical AND 16-bit imm data to reg-ind w/ 8-bit offs                    5 6         1001 1100  0ddd 0101  oooo oooo  iiii iiii  iiii iiii
AND [Rd+offset16], #data8   Logical AND 8-bit imm data to reg-ind w/ 16-bit offs                    5 6         1001 0101  0ddd 0101  oooo oooo  oooo oooo  iiii iiii
AND [Rd+offset16], #data16  Logical AND 16-bit imm data to reg-ind w/ 16-bit offs                   6 6         1001 1101  0ddd 0101  oooo oooo  oooo oooo  iiii iiii  iiii iiii
AND direct, #data8          Logical AND 8-bit imm data to mem                                       4 4         1001 0110  0DDD 0101  DDDD DDDD  iiii iiii
AND direct, #data16         Logical AND 16-bit imm data to mem                                      5 4         1001 1110  0DDD 0101  DDDD DDDD  iiii iiii  iiii iiii

OR Rd, #data8               Logical OR 8-bit imm data to reg                                        3 3         1001 0001  dddd 0110  iiii iiii
OR Rd, #data16              Logical OR 16-bit imm data to reg                                       4 3         1001 1001  dddd 0110  iiii iiii  iiii iiii
OR [Rd], #data8             Logical OR 8-bit imm data to reg-ind                                    3 4         1001 0010  0ddd 0110  iiii iiii
OR [Rd], #data16            Logical OR 16-bit imm data to reg-ind                                   4 4         1001 1010  0ddd 0110  iiii iiii  iiii iiii
OR [Rd+], #data8            Logical OR 8-bit imm data to reg-ind w/ autoinc                         3 5         1001 0011  0ddd 0110  iiii iiii
OR [Rd+], #data16           Logical OR 16-bit imm data to reg-ind w/ autoinc                        4 5         1001 1011  0ddd 0110  iiii iiii  iiii iiii
OR [Rd+offset8], #data8     Logical OR 8-bit imm data to reg-ind w/ 8-bit offs                      4 6         1001 0100  0ddd 0110  oooo oooo  iiii iiii
OR [Rd+offset8], #data16    Logical OR 16-bit imm data to reg-ind w/ 8-bit offs                     5 6         1001 1100  0ddd 0110  oooo oooo  iiii iiii  iiii iiii
OR [Rd+offset16], #data8    Logical OR 8-bit imm data to reg-ind w/ 16-bit offs                     5 6         1001 0101  0ddd 0110  oooo oooo  oooo oooo  iiii iiii
OR [Rd+offset16], #data16   Logical OR 16-bit imm data to reg-ind w/ 16-bit offs                    6 6         1001 1101  0ddd 0110  oooo oooo  oooo oooo  iiii iiii  iiii iiii
OR direct, #data8           Logical OR 8-bit imm data to mem                                        4 4         1001 0110  0DDD 0110  DDDD DDDD  iiii iiii
OR direct, #data16          Logical OR 16-bit imm data to mem                                       5 4         1001 1110  0DDD 0110  DDDD DDDD  iiii iiii  iiii iiii

XOR Rd, #data8              Logical XOR 8-bit imm data to reg                                       3 3         1001 0001  dddd 0111  iiii iiii
XOR Rd, #data16             Logical XOR 16-bit imm data to reg                                      4 3         1001 1001  dddd 0111  iiii iiii  iiii iiii
XOR [Rd], #data8            Logical XOR 8-bit imm data to reg-ind                                   3 4         1001 0010  0ddd 0111  iiii iiii
XOR [Rd], #data16           Logical XOR 16-bit imm data to reg-ind                                  4 4         1001 1010  0ddd 0111  iiii iiii  iiii iiii
XOR [Rd+], #data8           Logical XOR 8-bit imm data to reg-ind w/ autoinc                        3 5         1001 0011  0ddd 0111  iiii iiii
XOR [Rd+], #data16          Logical XOR 16-bit imm data to reg-ind w/ autoinc                       4 5         1001 1011  0ddd 0111  iiii iiii  iiii iiii
XOR [Rd+offset8], #data8    Logical XOR 8-bit imm data to reg-ind w/ 8-bit offs                     4 6         1001 0100  0ddd 0111  oooo oooo  iiii iiii
XOR [Rd+offset8], #data16   Logical XOR 16-bit imm data to reg-ind w/ 8-bit offs                    5 6         1001 1100  0ddd 0111  oooo oooo  iiii iiii  iiii iiii
XOR [Rd+offset16], #data8   Logical XOR 8-bit imm data to reg-ind w/ 16-bit offs                    5 6         1001 0101  0ddd 0111  oooo oooo  oooo oooo  iiii iiii
XOR [Rd+offset16], #data16  Logical XOR 16-bit imm data to reg-ind w/ 16-bit offs                   6 6         1001 1101  0ddd 0111  oooo oooo  oooo oooo  iiii iiii  iiii iiii
XOR direct, #data8          Logical XOR 8-bit imm data to mem                                       4 4         1001 0110  0DDD 0111  DDDD DDDD  iiii iiii
XOR direct, #data16         Logical XOR 16-bit imm data to mem                                      5 4         1001 1110  0DDD 0111  DDDD DDDD  iiii iiii  iiii iiii

MOV Rd, #data8              Move 8-bit imm data to reg                                              3 3         1001 0001  dddd 1000  iiii iiii
MOV Rd, #data16             Move 16-bit imm data to reg                                             4 3         1001 1001  dddd 1000  iiii iiii  iiii iiii
MOV [Rd], #data8            Move 16-bit imm data to reg-ind                                         3 3         1001 0010  0ddd 1000  iiii iiii
MOV [Rd], #data16           Move 16-bit imm data to reg-ind                                         4 3         1001 1010  0ddd 1000  iiii iiii  iiii iiii
MOV [Rd+], #data8           Move 8-bit imm data to reg-ind w/ autoinc                               3 4         1001 0011  0ddd 1000  iiii iiii
MOV [Rd+], #data16          Move 16-bit imm data to reg-ind w/ autoinc                              4 4         1001 1011  0ddd 1000  iiii iiii  iiii iiii
MOV [Rd+offset8], #data8    Move 8-bit imm data to reg-ind w/ 8-bit offs                            4 5         1001 0100  0ddd 1000  oooo oooo  iiii iiii
MOV [Rd+offset8], #data16   Move 16-bit imm data to reg-ind w/ 8-bit offs                           5 5         1001 1100  0ddd 1000  oooo oooo  iiii iiii  iiii iiii
MOV [Rd+offset16], #data8   Move 8-bit imm data to reg-ind w/ 16-bit offs                           5 5         1001 0101  0ddd 1000  oooo oooo  oooo oooo  iiii iiii
MOV [Rd+offset16], #data16  Move 16-bit imm data to reg-ind w/ 16-bit offs                          6 5         1001 1101  0ddd 1000  oooo oooo  oooo oooo  iiii iiii  iiii iiii
MOV direct, #data8          Move 8-bit imm data to mem                                              4 3         1001 0110  0DDD 1000  DDDD DDDD  iiii iiii
MOV direct, #data16         Move 16-bit imm data to mem                                             5 3         1001 1110  0DDD 1000  DDDD DDDD  iiii iiii  iiii iiii
*/
void xa_cpu::e_alu(u8 op)
{
	const u8 op2 = program_read_byte(m_pc++);
	handle_alu_type1(op, op2);
}

/*
MOV direct, direct          Move mem to mem                                                         4 4         1001 S111  0DDD 0ddd  DDDD DDDD  dddd dddd
JB bit,rel8                 Jump if bit set                                                         4 10t/6nt   1001 0111  1000 00bb  bbbb bbbb  rrrr rrrr
JNB bit,rel8                Jump if bit not set                                                     4 10t/6nt   1001 0111  1010 00bb  bbbb bbbb  rrrr rrrr
JBC bit,rel8                Jump if bit set and then clear the bit                                  4 11t/7nt   1001 0111  1100 00bb  bbbb bbbb  rrrr rrrr
*/
void xa_cpu::e_jb_mov_subgroup(u8 op)
{
	const u8 op2 = program_read_byte(m_pc++);
	const u8 op3 = program_read_byte(m_pc++);
	const u8 op4 = program_read_byte(m_pc++);

	if (op2 & 0x80)
	{
		int bit = ((op2 & 0x03) << 8) | op3;
		switch (op2 & 0x70)
		{
		case 0x00: jb_bit_rel8(bit, op4); break;
		case 0x20: jnb_bit_rel8(bit, op4); break;
		case 0x40: jbc_bit_rel8(bit, op4); break;
		default:   LOGMASKED(LOG_DEBUG,  "illegal conditional jump %s $%02x", get_bittext(bit), expand_rel8(op4) ); break;
		}
	}
	else
	{
		int direct_dst = ((op2 & 0x70) << 4) | op3;
		int direct_src = ((op2 & 0x07) << 8) | op4;
		int size = op & 0x08;
		if (size) { mov_word_direct_direct(direct_dst, direct_src); } else { mov_byte_direct_direct(direct_dst, direct_src); }
	}
}

// -------------------------------------- Group a --------------------------------------

/*
XCH Rd, direct              Exchange contents of mem w/ a reg                                       3 6         1010 S000  dddd 1DDD  DDDD DDDD

MOV direct, [Rs]            Move reg-ind to mem                                                     3 4         1010 S000  1sss 0DDD  DDDD DDDD
MOV [Rd], direct            Move mem to reg-ind                                                     3 4         1010 S000  0ddd 0DDD  DDDD DDDD
*/
void xa_cpu::e_movdir(u8 op)
{
	const u8 op2 = program_read_byte(m_pc++);
	const u8 op3 = program_read_byte(m_pc++);
	int size = op & 0x08;
	const u16 direct = ((op2 & 0x07) << 8) | op3;

	if (op2 & 0x08)
	{
		const u8 rd = op2 & (0xf0) >> 4;
		if (size) { xch_word_rd_direct(rd, direct); } else { xch_byte_rd_direct(rd, direct); }
	}
	else
	{
		if (op2 & 0x80)
		{
			const u8 rs = op2 & (0x70) >> 4;
			if (size) { mov_word_direct_indrs(direct, rs); } else { mov_byte_direct_indrs(direct, rs); }
		}
		else
		{
			const u8 rd = op2 & (0x70) >> 4;
			if (size) { mov_word_indrd_direct(rd, direct); } else { mov_byte_indrd_direct(rd, direct); }
		}
	}
}

/*
ADDS Rd, #data4             Add 4-bit signed imm data to reg                                        2 3         1010 S001  dddd iiii
ADDS [Rd], #data4           Add 4-bit signed imm data to reg-ind                                    2 4         1010 S010  0ddd iiii
ADDS [Rd+], #data4          Add 4-bit signed imm data to reg-ind w/ autoinc                         2 5         1010 S011  0ddd iiii
ADDS [Rd+offset8], #data4   Add reg-ind w/ 8-bit offs to 4-bit signed imm data                      3 6         1010 S100  0ddd iiii  oooo oooo
ADDS [Rd+offset16], #data4  Add reg-ind w/ 16-bit offs to 4-bit signed imm data                     4 6         1010 S101  0ddd iiii  oooo oooo  oooo oooo
ADDS direct, #data4         Add 4-bit signed imm data to mem                                        3 4         1010 S110  0DDD iiii  DDDD DDDD
*/
void xa_cpu::e_adds(u8 op)
{
	handle_adds_movs(op, 0);
}

/*
MOVX [Rd], Rs               Move external data from reg to mem                                      2 6         1010 S111  ssss 1ddd
MOVX Rd, [Rs]               Move external data from mem to reg                                      2 6         1010 S111  dddd 0sss
*/
void xa_cpu::e_movx_subgroup(u8 op)
{
	const u8 op2 = program_read_byte(m_pc++);
	int size = op & 0x08;
	if (op2 & 0x08)
	{
		const u8 rs = (op2 & 0xf0) >> 4;
		const u8 rd = (op2 & 0x07);
		if (size) { movx_word_indrd_rs(rd, rs); } else { movx_byte_indrd_rs(rd, rs); }
	}
	else
	{
		const u8 rd = (op2 & 0xf0) >> 4;
		const u8 rs = (op2 & 0x07);
		if (size) { movx_word_rd_indrs(rd, rs); } else { movx_byte_rd_indrs(rd, rs); }
	}
}

// -------------------------------------- Group b --------------------------------------

/*
RR Rd, #data4               Rotate right reg by the 4-bit imm value                                 2 a*        1011 S000  dddd iiii
*/
void xa_cpu::e_rr(u8 op)
{
	const u8 op2 = program_read_byte(m_pc++);
	int size = op & 0x08;
	const u8 rd = (op2 & 0xf0) >> 4;
	const u8 data = rd & 0x0f;
	if (size) { rr_word_rd_data4(rd, data); } else { rr_byte_rd_data4(rd, data); }
}

/*
MOVS Rd, #data4             Move 4-bit sign-extended imm data to reg                                2 3         1011 S001  dddd iiii
MOVS [Rd], #data4           Move 4-bit sign-extended imm data to reg-ind                            2 3         1011 S010  0ddd iiii
MOVS [Rd+], #data4          Move 4-bit sign-extended imm data to reg-ind w/ autoinc                 2 4         1011 S011  0ddd iiii
MOVS [Rd+offset8], #data4   Move reg-ind w/ 8-bit offs to 4-bit sign-extended imm data              3 5         1011 S100  0ddd iiii  oooo oooo
MOVS [Rd+offset16], #data4  Move reg-ind w/ 16-bit offs to 4-bit sign-extended imm data             4 5         1011 S101  0ddd iiii  oooo oooo  oooo oooo
MOVS direct, #data4         Move 4-bit sign-extended imm data to mem                                3 3         1011 S110  0DDD iiii  DDDD DDDD
*/
void xa_cpu::e_movs(u8 op)
{
	handle_adds_movs(op, 1);
}

/*
RRC Rd, #data4              Rotate right reg though carry by the 4-bit imm value                    2 a*        1011 S111  dddd iiii
*/
void xa_cpu::e_rrc(u8 op)
{
	const u8 op2 = program_read_byte(m_pc++);
	int size = op & 0x08;
	const u8 rd = (op2 & 0xf0) >> 4;
	const u8 data = rd & 0x0f;
	if (size) { rrc_word_rd_data4(rd, data); } else { rrc_byte_rd_data4(rd, data); }
}


// -------------------------------------- Group c --------------------------------------

/*
LSR Rd, Rs                  Logical right shift dest reg by the value in the src reg                2 a*        1100 SS00  dddd ssss
FCALL addr24                Far call (full 24-bit address space)                                    4 12/8(PZ)  1100 0100  aaaa aaaa  AAAA AAAA  AAAA AAAA
*/
void xa_cpu::e_lsr_fc(u8 op)
{
	int size = (op & 0x0c) >> 2;
	if (size == 0x01)
	{
		const u8 op2 = program_read_byte(m_pc++);
		const u8 op3 = program_read_byte(m_pc++);
		const u8 op4 = program_read_byte(m_pc++);
		const u32 addr = (op2 << 8) | op3 | (op4 << 16);
		fcall_addr24(addr);
	}
	else
	{
		const u8 op2 = program_read_byte(m_pc++);
		const u8 rd = (op2 & 0xf0) >> 4;
		const u8 rs = (op2 & 0x0f);
		if (size == 0) lsr_byte_rd_rs(rd, rs);
		else if (size == 2) lsr_word_rd_rs(rd, rs);
		else if (size == 3) lsr_dword_rd_rs(rd, rs);
	}
}

/*
ASL Rd, Rs                  Logical left shift dest reg by the value in the src reg                 2 a*        1100 SS01  dddd ssss
CALL rel16                  Relative call (range +/- 64K)                                           3 7/4(PZ)   1100 0101  rrrr rrrr  rrrr rrrr
*/
void xa_cpu::e_asl_c(u8 op)
{
	int size = (op & 0x0c) >> 2;
	if (size == 0x01)
	{
		const u8 op2 = program_read_byte(m_pc++);
		const u8 op3 = program_read_byte(m_pc++);
		u16 rel16 = (op2 << 8) | op3;
		call_rel16(rel16);
	}
	else
	{
		const u8 op2 = program_read_byte(m_pc++);
		const u8 rd = (op2 & 0xf0) >> 4;
		const u8 rs = (op2 & 0x0f);
		if (size == 0) asl_byte_rd_rs(rd, rs);
		else if (size == 2) asl_word_rd_rs(rd, rs);
		else if (size == 3) asl_dword_rd_rs(rd, rs);
	}
}

/*
ASR Rd, Rs                  Arithmetic shift right dest reg by the count in the src                 2 a*        1100 SS10  dddd ssss
CALL [Rs]                   Subroutine call ind w/ a reg                                            2 8/5(PZ)   1100 0110  0000 0sss
*/
void xa_cpu::e_asr_c(u8 op)
{
	int size = (op & 0x0c) >> 2;
	if (size == 0x01)
	{
		const u8 op2 = program_read_byte(m_pc++);
		const u8 rs = op2 & 0x07;
		call_indrs(rs);
	}
	else
	{
		const u8 op2 = program_read_byte(m_pc++);
		const u8 rd = (op2 & 0xf0) >> 4;
		const u8 rs = (op2 & 0x0f);
		if (size == 0) asr_byte_rd_rs(rd, rs);
		else if (size == 2) asr_word_rd_rs(rd, rs);
		else if (size == 3) asr_dword_rd_rs(rd, rs);
	}
}

/*
NORM Rd, Rs                 Logical shift left dest reg by the value in the src reg until MSB set   2 a*        1100 SS11  dddd ssss
*/
void xa_cpu::e_norm(u8 op)
{
	int size = (op & 0x0c) >> 2;
	if (size == 0x01)
	{
		program_read_byte(m_pc++);
		//LOGMASKED(LOG_DEBUG, "illegal %02x", op2);
		do_nop();
	}
	else
	{
		const u8 op2 = program_read_byte(m_pc++);
		int rd = (op2 & 0xf0) >> 4;
		int rs = (op2 & 0x0f);
		if (size == 0) norm_byte_rd_rs(rd, rs);
		else if (size == 2) norm_word_rd_rs(rd, rs);
		else if (size == 3) norm_dword_rd_rs(rd, rs);
	}
}

// -------------------------------------- Group d --------------------------------------

/*
LSR Rd, #data4              Logical right shift reg by the 4-bit imm value                          2 a*        1101 SS00  dddd iiii
LSR Rd, #data5              Logical right shift reg by the 4-bit imm value                          2 a*        1101 1100  dddi iiii
FJMP addr24                 Far jump (full 24-bit address space)                                    4 6         1101 0100  aaaa aaaa  AAAA AAAA  AAAA AAAA
*/
void xa_cpu::e_lsr_fj(u8 op)
{
	int size = (op & 0x0c) >> 2;
	if (size == 0x01)
	{
		const u8 op2 = program_read_byte(m_pc++);
		const u8 op3 = program_read_byte(m_pc++);
		const u8 op4 = program_read_byte(m_pc++);
		const u32 addr = (op2 << 8) | op3 | (op4 << 16);
		fjmp_addr24(addr);
	}
	else
	{
		handle_shift(op, 2);
	}
}

/*
ASL Rd, #data4              Logical left shift reg by the 4-bit imm value                           2 a*        1101 SS01  dddd iiii
ASL Rd, #data5              Logical left shift reg by the 5-bit imm value                           2 a*        1101 1101  dddi iiii
JMP rel16                   Long unconditional branch                                               3 6         1101 0101  rrrr rrrr  rrrr rrrr
*/
void xa_cpu::e_asl_j(u8 op)
{
	int size = (op & 0x0c) >> 2;
	if (size == 0x01)
	{
		const u8 op2 = program_read_byte(m_pc++);
		const u8 op3 = program_read_byte(m_pc++);
		u16 offset = (op2 << 8) | op3;
		jmp_rel16(offset);
	}
	else
	{
		handle_shift(op, 0);
	}
}

/*
ASR Rd, #data4              Arithmetic shift right reg by the 4-bit imm count                       2 a*        1101 SS10  dddd iiii
ASR Rd, #data5              Arithmetic shift right reg by the 5-bit imm count                       2 a*        1101 1110  dddi iiii
RESET                       Causes a hardware Reset (same as external Reset)                        2 18        1101 0110  0001 0000
TRAP #data4                 Causes 1 of 16 hardware traps to be executed                            2 23/19(PZ) 1101 0110  0011 tttt
JMP [A+DPTR]                Jump ind relative to the DPTR                                           2 5         1101 0110  0100 0110
JMP [[Rs+]]                 Jump double-ind to the address (pointer to a pointer)                   2 8         1101 0110  0110 0sss
JMP [Rs]                    Jump ind to the address in the reg (64K)                                2 7         1101 0110  0111 0sss
RET                         Return from subroutine                                                  2 8/6(PZ)   1101 0110  1000 0000
RETI                        Return from interrupt                                                   2 10/8(PZ)  1101 0110  1001 0000
*/
void xa_cpu::e_asr_j(u8 op)
{
	int size = (op & 0x0c) >> 2;
	const u8 op2 = program_read_byte(m_pc++);
	if (size == 0x01)
	{
		switch (op2 & 0xf0)
		{
		case 0x10: reset(); break;
		case 0x30: trap_data4(op2 & 0x0f); break;
		case 0x40: jmp_ind_adptr(); break;
		case 0x60: jmp_dblindrs(op2 & 0x07); break;
		case 0x70: jmp_indrs(op2 & 0x07); break;
		case 0x80: ret(); break;
		case 0x90: reti(); break;
		default:   LOGMASKED(LOG_DEBUG, "illegal"); do_nop(); break;
		}
	}
	else
	{
		handle_shift(op, 1);
	}
}

/*
RL Rd, #data4               Rotate left reg by the 4-bit imm value                                  2 a*        1101 S011  dddd iiii
*/
void xa_cpu::e_rl(u8 op)
{
	int size = op & 0x08;
	const u8 op2 = program_read_byte(m_pc++);
	const u8 rd = (op2 & 0xf0) >> 4;
	const u8 data4 = (op2 & 0x0f);
	if (size) { rl_word_rd_data4(rd, data4); } else { rl_byte_rd_data4(rd, data4); }
}

/*
RLC Rd, #data4              Rotate left reg though carry by the 4-bit imm value                     2 a*        1101 S111  dddd iiii
*/
void xa_cpu::e_rlc(u8 op)
{
	int size = op & 0x08;
	const u8 op2 = program_read_byte(m_pc++);
	const u8 rd = (op2 & 0xf0) >> 4;
	const u8 data4 = (op2 & 0x0f);
	if (size) { rlc_word_rd_data4(rd, data4); } else { rlc_byte_rd_data4(rd, data4); }
}

// -------------------------------------- Group e --------------------------------------

/*
DJNZ direct,rel8            Decrement mem and jump if not zero                                      4 9t/5nt    1110 S010  0000 1DDD  DDDD DDDD  rrrr rrrr
CJNE Rd,direct,rel8         Compare dir byte to reg and jump if not equal                           4 10t/7nt   1110 S010  dddd 0DDD  DDDD DDDD  rrrr rrrr
*/
void xa_cpu::e_djnz_cjne(u8 op)
{
	const u8 op2 = program_read_byte(m_pc++);
	const u8 op3 = program_read_byte(m_pc++);
	const u8 op4 = program_read_byte(m_pc++);
	int size = op & 0x08;

	const u16 direct = ((op2 & 0x07) << 8) | op3;
	if (op2 & 0x08)
	{
		if (size) { djnz_word_direct_rel8(direct, op4); } else { djnz_byte_direct_rel8(direct, op4); }
	}
	else
	{
		int rd = (op2 & 0xf0) >> 4;
		if (size) { cjne_word_rd_direct_rel8(rd, direct, op4); } else { cjne_byte_rd_direct_rel8(rd, direct, op4); }
	}
}

/*
MULU.b Rd, Rs               8X8 unsigned multiply of reg contents                                   2 12        1110 0000  dddd ssss
*/
void xa_cpu::e_mulu_b(u8 op)
{
	const u8 op2 = program_read_byte(m_pc++);
	const u8 rd = (op2 & 0xf0) >> 4;
	const u8 rs = (op2 & 0x0f);
	mulu_byte_rd_rs(rd, rs);
}

/*
DIVU.b Rd, Rs               8x8 unsigned reg divide                                                 2 12        1110 0001  dddd ssss
*/
void xa_cpu::e_divu_b(u8 op)
{
	const u8 op2 = program_read_byte(m_pc++);
	const u8 rd = (op2 & 0xf0) >> 4;
	const u8 rs = (op2 & 0x0f);
	divu_byte_rd_rs(rd, rs);
}

/*
MULU.w Rd, Rs               16X16 unsigned reg multiply                                             2 12        1110 0100  dddd ssss
*/
void xa_cpu::e_mulu_w(u8 op)
{
	const u8 op2 = program_read_byte(m_pc++);
	const u8 rd = (op2 & 0xf0) >> 4;
	const u8 rs = (op2 & 0x0f);
	mulu_word_rd_rs(rd, rs);
}

/*
DIVU.w Rd, Rs               16X8 unsigned reg divide                                                2 12        1110 0101  dddd ssss
*/
void xa_cpu::e_divu_w(u8 op)
{
	const u8 op2 = program_read_byte(m_pc++);
	const u8 rd = (op2 & 0xf0) >> 4;
	const u8 rs = (op2 & 0x0f);
	divu_word_rd_rs(rd, rs);
}

/*
MUL.w Rd, Rs                16X16 signed multiply of reg contents                                   2 12        1110 0110  dddd ssss
*/
void xa_cpu::e_mul_w(u8 op)
{
	const u8 op2 = program_read_byte(m_pc++);
	const u8 rd = (op2 & 0xf0) >> 4;
	const u8 rs = (op2 & 0x0f);
	mul_word_rd_rs(rd, rs);
}

/*
DIV.w Rd, Rs                16x8 signed reg divide                                                  2 14        1110 0111  dddd ssss
*/
void xa_cpu::e_div_w(u8 op)
{
	const u8 op2 = program_read_byte(m_pc++);
	const u8 rd = (op2 & 0xf0) >> 4;
	const u8 rs = (op2 & 0x0f);
	div_word_rd_rs(rd, rs);
}

/*
MULU.b Rd, #data8           8X8 unsigned multiply of 8-bit imm data w/ reg                          3 12        1110 1000  dddd 0000  iiii iiii
DIVU.b Rd, #data8           8X8 unsigned reg divide w/ imm byte                                     3 12        1110 1000  dddd 0001  iiii iiii
DIVU.w Rd, #data8           16X8 unsigned reg divide w/ imm byte                                    3 12        1110 1000  dddd 0011  iiii iiii
DIV.w Rd, #data8            16x8 signed divide reg w/ imm word                                      3 14        1110 1000  dddd 1011  iiii iiii

*/
void xa_cpu::e_div_data8(u8 op)
{
	const u8 op2 = program_read_byte(m_pc++);
	const u8 data8 = program_read_byte(m_pc++);
	const u8 rd = (op2 & 0xf0) >> 4;

	switch (op2 & 0x0f)
	{
	case 0x00: { mulu_byte_rd_data8(rd, data8); break; }
	case 0x01: { divu_byte_rd_data8(rd, data8); break; }
	case 0x03: { divu_word_rd_data8(rd, data8); break; }
	case 0x0b: { div_word_rd_data8(rd, data8);break; }
	default:   { LOGMASKED(LOG_DEBUG, "illegal mul/div data8 %s #$%02x", m_regnames8[rd], data8); do_nop(); break; }
	}
}

/*
MULU.w Rd, #data16          16X16 unsigned multiply 16-bit imm data w/ reg                          4 12        1110 1001  dddd 0000  iiii iiii  iiii iiii
DIVU.d Rd, #data16          32X16 unsigned double reg divide w/ imm word                            4 22        1110 1001  ddd0 0001  iiii iiii  iiii iiii
MUL.w Rd, #data16           16X16 signed multiply 16-bit imm data w/ reg                            4 12        1110 1001  dddd 1000  iiii iiii  iiii iiii
DIV.d Rd, #data16           32x16 signed double reg divide w/ imm word                              4 24        1110 1001  ddd0 1001  iiii iiii  iiii iiii
*/
void xa_cpu::e_div_d16(u8 op)
{
	const u8 op2 = program_read_byte(m_pc++);
	const u8 op3 = program_read_byte(m_pc++);
	const u8 op4 = program_read_byte(m_pc++);
	const u16 data16 = (op3 << 8) | op4;
	switch (op2 & 0x0f)
	{
	case 0x00: { const u8 rd = (op2 & 0xf0) >> 4; mulu_word_rd_data16(rd, data16); break; }
	case 0x01: { const u8 rd = (op2 & 0xe0) >> 4; divu_dword_rd_data16(rd, data16); break; }
	case 0x08: { const u8 rd = (op2 & 0xf0) >> 4; mul_word_rd_data16(rd, data16); break; }
	case 0x09: { const u8 rd = (op2 & 0xe0) >> 4; div_dword_rd_data16(rd, data16); break; }
	default:   { /*const u8 rd = (op2 & 0xf0) >> 4; LOGMASKED(LOG_DEBUG, "illegal mul/div data16 %s, #$%04x", m_regnames16[rd], data16);*/ do_nop();  break; }
	}
}

/*
DIVU.d Rd, Rs               32X16 unsigned double reg divide                                        2 22        1110 1101  ddd0 ssss
*/
void xa_cpu::e_divu_d(u8 op)
{
	const u8 op2 = program_read_byte(m_pc++);
	const u8 rd = (op2 & 0xe0) >> 4;
	const u8 rs = (op2 & 0x0f);
	divu_dword_rd_rs(rd, rs);
}

/*
DIV.d Rd, Rs                32x16 signed double reg divide                                          2 24        1110 1111  ddd0 ssss
*/
void xa_cpu::e_div_d(u8 op)
{
	const u8 op2 = program_read_byte(m_pc++);
	const u8 rd = (op2 & 0xe0) >> 4;
	const u8 rs = (op2 & 0x0f);
	div_dword_rd_rs(rd, rs);
}

/*
CJNE [Rd],#data8,rel8       Compare imm word to reg-ind and jump if not equal                       4 10t/7nt   1110 0011  0ddd 1000  rrrr rrrr  iiii iiii
CJNE Rd,#data8,rel8         Compare imm byte to reg and jump if not equal                           4 9t/6nt    1110 0011  dddd 0000  rrrr rrrr  iiii iiii
*/
void xa_cpu::e_cjne_d8(u8 op)
{
	const u8 op2 = program_read_byte(m_pc++);
	const u8 op3 = program_read_byte(m_pc++);
	const u8 op4 = program_read_byte(m_pc++);
	if (op2 & 0x08)
	{
		const u8 rd = (op2 & 0x70) >> 4;
		cjne_indrd_data8_rel8(rd, op4, op3);
	}
	else
	{
		const u8 rd = (op2 & 0xf0) >> 4;
		cjne_rd_data8_rel8(rd, op4, op3);
	}
}

/*
CJNE [Rd],#data16,rel8      Compare imm word to reg-ind and jump if not equal                       5 10t/7nt   1110 1011  0ddd 1000  rrrr rrrr  iiii iiii  iiii iiii
CJNE Rd,#data16,rel8        Compare imm word to reg and jump if not equal                           5 9t/6nt    1110 1011  dddd 0000  rrrr rrrr  iiii iiii  iiii iiii
*/
void xa_cpu::e_cjne_d16(u8 op)
{
	const u8 op2 = program_read_byte(m_pc++);
	const u8 op3 = program_read_byte(m_pc++);
	const u8 op4 = program_read_byte(m_pc++);
	const u8 op5 = program_read_byte(m_pc++);
	const u16 data = (op4 << 8) | op5;
	if (op2 & 0x08)
	{
		const u8 rd = (op2 & 0x70) >> 4;
		cjne_indrd_data16_rel8(rd, data, op3);
	}
	else
	{
		const u8 rd = (op2 & 0xf0) >> 4;
		cjne_rd_data16_rel8(rd, data, op3);
	}
}

/*
JZ rel8                     Jump if accumulator equals zero                                         2 6t/3nt    1110 1100  rrrr rrrr
*/
void xa_cpu::e_jz_rel8(u8 op)
{
	const u8 op2 = program_read_byte(m_pc++);
	jz_rel8(op2);
}

/*
JNZ rel8                    Jump if accumulator not equal zero                                      2 6t/3nt    1110 1110  rrrr rrrr
*/
void xa_cpu::e_jnz_rel8(u8 op)
{
	const u8 op2 = program_read_byte(m_pc++);
	jnz_rel8(op2);
}


// -------------------------------------- Group f --------------------------------------

/*
BCC rel8                    Branch if the carry flag is clear                                       2 6t/3nt    1111 0000  rrrr rrrr
BCS rel8                    Branch if the carry flag is set                                         2 6t/3nt    1111 0001  rrrr rrrr
BNE rel8                    Branch if the zero flag is not set                                      2 6t/3nt    1111 0010  rrrr rrrr
BEQ rel8                    Branch if the zero flag is set                                          2 6t/3nt    1111 0011  rrrr rrrr
BNV rel8                    Branch if overflow flag is clear                                        2 6t/3nt    1111 0100  rrrr rrrr
BOV rel8                    Branch if overflow flag is set                                          2 6t/3nt    1111 0101  rrrr rrrr
BPL rel8                    Branch if the negative flag is clear                                    2 6t/3nt    1111 0110  rrrr rrrr
BMI rel8                    Branch if the negative flag is set                                      2 6t/3nt    1111 0111  rrrr rrrr
BG rel8                     Branch if greater than (unsigned)                                       2 6t/3nt    1111 1000  rrrr rrrr
BL rel8                     Branch if less than or equal to (unsigned)                              2 6t/3nt    1111 1001  rrrr rrrr
BGE rel8                    Branch if greater than or equal to (signed)                             2 6t/3nt    1111 1010  rrrr rrrr
BLT rel8                    Branch if less than (signed)                                            2 6t/3nt    1111 1011  rrrr rrrr
BGT rel8                    Branch if greater than (signed)                                         2 6t/3nt    1111 1100  rrrr rrrr
BLE rel8                    Branch if less than or equal to (signed)                                2 6t/3nt    1111 1101  rrrr rrrr
BR rel8                     Short unconditional branch                                              2 6         1111 1110  rrrr rrrr
*/

void xa_cpu::e_branch(u8 op)
{
	const u8 rel8 = program_read_byte(m_pc++);
	switch (op & 0x0f)
	{
	case 0x00: bcc_rel8(rel8); break;
	case 0x01: bcs_rel8(rel8); break;
	case 0x02: bne_rel8(rel8); break;
	case 0x03: beq_rel8(rel8); break;
	case 0x04: bnv_rel8(rel8); break;
	case 0x05: bov_rel8(rel8); break;
	case 0x06: bpl_rel8(rel8); break;
	case 0x07: bmi_rel8(rel8); break;
	case 0x08: bg_rel8(rel8); break;
	case 0x09: bl_rel8(rel8); break;
	case 0x0a: bge_rel8(rel8); break;
	case 0x0b: blt_rel8(rel8); break;
	case 0x0c: bgt_rel8(rel8); break;
	case 0x0d: ble_rel8(rel8); break;
	case 0x0e: br_rel8(rel8); break;
	case 0x0f: LOGMASKED(LOG_DEBUG, "Illegal branch type"); do_nop(); break; // probably acts as 'branch never'
	}
}

/*
BKPT                        Cause the breakpoint trap to be executed.                               1 23/19(PZ) 1111 1111
*/
void xa_cpu::e_bkpt(u8 op)
{
	fatalerror( "BKPT");
}


/*****************************************************************************/

void xa_cpu::Init(INT32 clock)
{
	m_clock = clock;
	m_total_cycles = 0;
	m_start_cycles = 0;
	m_run_end = 0;
	m_icount = 0;
}

void xa_cpu::Reset()
{
	m_im = 0;
	m_rs = 0;
	m_zflag = 0;
	m_nflag = 0;
	m_vflag = 0;
	m_cflag = 0;
	m_acflag = 0;
	m_sm_flag = 0;
	m_tm_flag = 0;
	m_p_flag = 0;
	m_f0_flag = 0;
	m_f1_flag = 0;

	m_pc = 0;

	m_usermode = false;
	m_pagezeromode = 0;

	m_USP = 0;
	m_SSP = 0;

	m_WDCON = 0;
	m_SCR = 0;
	m_IEL = 0;
	m_PSWL = 0;
	m_PSWH = 0;

	m_regbank = 0;

	memset(m_PxCFGA, 0, sizeof(m_PxCFGA));
	memset(m_PxCFGB, 0, sizeof(m_PxCFGB));

	m_in_interrupt = 0;
	m_irq_pending = 0;
	memset(m_regs, 0, sizeof(m_regs));

	m_icount = 0;
	m_total_cycles = 0;
	m_start_cycles = 0;
	m_run_end = 0;

	u16 temppsw = program_read_word(0);
	sfr_PSWL_w(temppsw & 0xff);
	sfr_PSWH_w((temppsw >> 8) & 0xff);
	m_pc = program_read_word(2);

	m_WDCON = 0x00;
	m_SCR = 0x00;
	m_IEL = 0x00;

	m_USP = 0x0100;
	m_SSP = 0x0100;

	for (int i = 0; i < 4 * 4 + 4; i++)
	{
		m_regs[i] = 0x0000;
	}

	m_nflag = 0;
	m_zflag = 0;
	m_cflag = 0;
	m_vflag = 0;
	m_acflag = 0;

	m_pagezeromode = 1;

	m_in_interrupt = 0;
	m_irq_pending = 0;

	for (int i = 0; i < 4; i++)
	{
		m_PxCFGA[i] = 0;
		m_PxCFGB[i] = 0;
	}
}

/*****************************************************************************/

void xa_cpu::check_external_irq_level(int level)
{
	if (m_IEL & 0x80)
	{
		if (m_IEL & (1 << level))
		{
			if (m_irq_pending & (1 << level))
			{
				LOGMASKED(LOG_DEBUG, "testing irq %d\n", level);
				int vector = 0x20 + level;
				push_word_to_system_stack(sfr_PSWH_r());
				push_word_to_system_stack(sfr_PSWL_r());
				push_word_to_system_stack(m_pc);

				u16 temppsw = program_read_word(vector*4);
				sfr_PSWL_w(temppsw & 0xff);
				sfr_PSWH_w((temppsw >> 8) & 0xff);
				m_pc = program_read_word((vector*4)+2);
				m_in_interrupt = 1;
				m_irq_pending &= ~(1 << level);
				//standard_irq_callback(level, m_pc);
			}
		}
	}
}

void xa_cpu::check_interrupts()
{
	if (m_irq_pending)
	{
		if (!m_in_interrupt)
		{
			for (int i = 0; i < 4; i++)
				check_external_irq_level(i);
		}
	}

}

void xa_cpu::SetIRQLine(INT32 inputnum, INT32 state)
{
	// This is not accurate, just test code for fearless/superkds
	if (state)
		m_irq_pending |= 1 << inputnum;
	check_interrupts();
}

INT32 xa_cpu::Run(INT32 cycles)
{
	m_icount = cycles;
	m_start_cycles = cycles;
	m_run_end = 0;

	{
		check_interrupts();

		//debugger_instruction_hook(m_pc);
		u32 oldpc = m_pc;
		u8 op = program_read_byte(m_pc++);
		INT32 old_icount = m_icount;
		(this->*s_instruction[op])(op);
		if (m_icount == old_icount) {
			m_icount = 0;
			bprintf(0, _T("xa, cpu probably deadlocked. ppc / pc: %x / %x?\n"), oldpc, m_pc);
			////	fatalerror("op at %06x took no cycles\n", oldpc);
		}
	} while (m_icount > 0 && !m_run_end)

	cycles = m_start_cycles - m_icount;
	m_start_cycles = m_icount = 0;

	m_total_cycles += cycles;

	return cycles;
}

INT32 xa_cpu::Idle(INT32 cycles)
{
	m_total_cycles += cycles;

	return cycles;
}

INT32 xa_cpu::GetTotalCycles()
{
	return m_total_cycles + (m_start_cycles + m_icount);
}

void xa_cpu::RunEnd()
{
	m_run_end = 1;
}

void xa_cpu::NewFrame()
{
	m_total_cycles = 0;
}

INT32 xa_cpu::GetPC()
{
	return m_pc;
}

INT32 xa_cpu::Scan(INT32 nAction)
{
	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(m_im);
		SCAN_VAR(m_rs);
		SCAN_VAR(m_zflag);
		SCAN_VAR(m_nflag);
		SCAN_VAR(m_vflag);
		SCAN_VAR(m_cflag);
		SCAN_VAR(m_acflag);
		SCAN_VAR(m_sm_flag);
		SCAN_VAR(m_tm_flag);
		SCAN_VAR(m_p_flag);
		SCAN_VAR(m_f0_flag);
		SCAN_VAR(m_f1_flag);

		SCAN_VAR(m_pc);

		SCAN_VAR(m_usermode);
		SCAN_VAR(m_pagezeromode);

		SCAN_VAR(m_USP);
		SCAN_VAR(m_SSP);

		SCAN_VAR(m_WDCON);
		SCAN_VAR(m_SCR);
		SCAN_VAR(m_IEL);
		SCAN_VAR(m_PSWL);
		SCAN_VAR(m_PSWH);

		SCAN_VAR(m_regbank);

		SCAN_VAR(m_PxCFGA);
		SCAN_VAR(m_PxCFGB);

		SCAN_VAR(m_in_interrupt);
		SCAN_VAR(m_irq_pending);
		SCAN_VAR(m_regs);

		SCAN_VAR(m_icount);
		SCAN_VAR(m_total_cycles);
		SCAN_VAR(m_start_cycles);
		SCAN_VAR(m_clock);
	}

	return 0;
}

