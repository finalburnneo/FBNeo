// license:BSD-3-Clause
// copyright-holders:hap

// HMCS40 opcode handlers

//#include "emu.h"
//#include "hmcs40.h"


// internal helpers

static UINT8 ram_r()
{
	UINT8 address = ((m_x << 4) | m_y) & m_datamask;
	return m_data_read(address) & 0xf;
}

static void ram_w(UINT8 data)
{
	UINT8 address = ((m_x << 4) | m_y) & m_datamask;
	m_data_write(address, data & 0xf);
}

static void pop_stack()
{
	m_pc = m_stack[0] & m_pcmask;
	for (int i = 0; i < m_stack_levels-1; i++)
		m_stack[i] = m_stack[i+1];
}

static void push_stack()
{
	for (int i = m_stack_levels-1; i >= 1; i--)
		m_stack[i] = m_stack[i-1];
	m_stack[0] = m_pc;
}


// instruction set

static void op_illegal()
{
	bprintf(0, _T("unknown opcode $%03X @ $%04X\n"), m_op, m_prev_pc);
}


// register-to-register instructions

static void op_lab()
{
	// LAB: Load A from B
	m_a = m_b;
}

static void op_lba()
{
	// LBA: Load B from A
	m_b = m_a;
}

static void op_lay()
{
	// LAY: Load A from Y
	m_a = m_y;
}

static void op_laspx()
{
	// LASPX: Load A from SPX
	m_a = m_spx;
}

static void op_laspy()
{
	// LASPY: Load A from SPY
	m_a = m_spy;
}

static void op_xamr()
{
	// XAMR m: Exchange A and MR(m)

	// determine MR(Memory Register) location
	UINT8 address = m_op & 0xf;

	// HMCS42: MR0 on file 0, MR4-MR15 on file 4 (there is no file 1-3)
	// HMCS43: MR0-MR3 on file 0-3, MR4-MR15 on file 4
	if (m_family == HMCS42_FAMILY || m_family == HMCS43_FAMILY)
		address |= (address < 4) ? (address << 4) : 0x40;

	// HMCS44/45/46/47: all on last file
	else
		address |= 0xf0;

	address &= m_datamask;
	UINT8 old_a = m_a;
	m_a = m_data_read(address) & 0xf;
	m_data_write(address, old_a & 0xf);
}


// RAM address instructions

static void op_lxa()
{
	// LXA: Load X from A
	m_x = m_a;
}

static void op_lya()
{
	// LYA: Load Y from A
	m_y = m_a;
}

static void op_lxi()
{
	// LXI i: Load X from Immediate
	m_x = m_i;
}

static void op_lyi()
{
	// LYI i: Load Y from Immediate
	m_y = m_i;
}

static void op_iy()
{
	// IY: Increment Y
	m_y = (m_y + 1) & 0xf;
	m_s = (m_y != 0);
}

static void op_dy()
{
	// DY: Decrement Y
	m_y = (m_y - 1) & 0xf;
	m_s = (m_y != 0xf);
}

static void op_ayy()
{
	// AYY: Add A to Y
	m_y += m_a;
	m_s = BIT(m_y, 4);
	m_y &= 0xf;
}

static void op_syy()
{
	// SYY: Subtract A from Y
	m_y -= m_a;
	m_s = BIT(~m_y, 4);
	m_y &= 0xf;
}

static void op_xsp()
{
	// XSP(XY): Exchange X and SPX, Y and SPY, or NOP if 0
	if (m_op & 1)
	{
		UINT8 old_x = m_x;
		m_x = m_spx;
		m_spx = old_x;
	}
	if (m_op & 2)
	{
		UINT8 old_y = m_y;
		m_y = m_spy;
		m_spy = old_y;
	}
}


// RAM register instructions

static void op_lam()
{
	// LAM(XY): Load A from Memory
	m_a = ram_r();
	op_xsp();
}

static void op_lbm()
{
	// LBM(XY): Load B from Memory
	m_b = ram_r();
	op_xsp();
}

static void op_xma()
{
	// XMA(XY): Exchange Memory and A
	UINT8 old_a = m_a;
	m_a = ram_r();
	ram_w(old_a);
	op_xsp();
}

static void op_xmb()
{
	// XMB(XY): Exchange Memory and B
	UINT8 old_b = m_b;
	m_b = ram_r();
	ram_w(old_b);
	op_xsp();
}

static void op_lmaiy()
{
	// LMAIY(X): Load Memory from A, Increment Y
	ram_w(m_a);
	op_iy();
	op_xsp();
}

static void op_lmady()
{
	// LMADY(X): Load Memory from A, Decrement Y
	ram_w(m_a);
	op_dy();
	op_xsp();
}


// immediate instructions

static void op_lmiiy()
{
	// LMIIY i: Load Memory from Immediate, Increment Y
	ram_w(m_i);
	op_iy();
}

static void op_lai()
{
	// LAI i: Load A from Immediate
	m_a = m_i;
}

static void op_lbi()
{
	// LBI i: Load B from Immediate
	m_b = m_i;
}


// arithmetic instructions

static void op_ai()
{
	// AI i: Add Immediate to A
	m_a += m_i;
	m_s = BIT(m_a, 4);
	m_a &= 0xf;
}

static void op_ib()
{
	// IB: Increment B
	m_b = (m_b + 1) & 0xf;
	m_s = (m_b != 0);
}

static void op_db()
{
	// DB: Decrement B
	m_b = (m_b - 1) & 0xf;
	m_s = (m_b != 0xf);
}

static void op_amc()
{
	// AMC: Add A to Memory with Carry
	m_a += ram_r() + m_c;
	m_c = BIT(m_a, 4);
	m_s = m_c;
	m_a &= 0xf;
}

static void op_smc()
{
	// SMC: Subtract A from Memory with Carry
	m_a = ram_r() - m_a - (m_c ^ 1);
	m_c = BIT(~m_a, 4);
	m_s = m_c;
	m_a &= 0xf;
}

static void op_am()
{
	// AM: Add A to Memory
	m_a += ram_r();
	m_s = BIT(m_a, 4);
	m_a &= 0xf;
}

static void op_daa()
{
	// DAA: Decimal Adjust for Addition
	if (m_c || m_a > 9)
	{
		m_a = (m_a + 6) & 0xf;
		m_c = 1;
	}
}

static void op_das()
{
	// DAS: Decimal Adjust for Subtraction
	if (!m_c || m_a > 9)
	{
		m_a = (m_a + 10) & 0xf;
		m_c = 0;
	}
}

static void op_nega()
{
	// NEGA: Negate A
	m_a = (0 - m_a) & 0xf;
}

static void op_comb()
{
	// COMB: Complement B
	m_b ^= 0xf;
}

static void op_sec()
{
	// SEC: Set Carry
	m_c = 1;
}

static void op_rec()
{
	// REC: Reset Carry
	m_c = 0;
}

static void op_tc()
{
	// TC: Test Carry
	m_s = m_c;
}

static void op_rotl()
{
	// ROTL: Rotate Left A with Carry
	m_a = (m_a << 1) | m_c;
	m_c = BIT(m_a, 4);
	m_a &= 0xf;
}

static void op_rotr()
{
	// ROTR: Rotate Right A with Carry
	UINT8 c = m_a & 1;
	m_a = (m_a >> 1) | (m_c << 3);
	m_c = c;
}

static void op_or()
{
	// OR: Or A with B
	m_a |= m_b;
}


// compare instructions

static void op_mnei()
{
	// MNEI i: Memory Not Equal to Immediate
	m_s = (m_i != ram_r());
}

static void op_ynei()
{
	// YNEI i: Y Not Equal to Immediate
	m_s = (m_y != m_i);
}

static void op_anem()
{
	// ANEM: A Not Equal to Memory
	m_s = (m_a != ram_r());
}

static void op_bnem()
{
	// BNEM: B Not Equal to Memory
	m_s = (m_b != ram_r());
}

static void op_alei()
{
	// ALEI i: A Less or Equal to Immediate
	m_s = (m_a <= m_i);
}

static void op_alem()
{
	// ALEM: A Less or Equal to Memory
	m_s = (m_a <= ram_r());
}

static void op_blem()
{
	// BLEM: B Less or Equal to Memory
	m_s = (m_b <= ram_r());
}


// RAM bit manipulation instructions

static void op_sem()
{
	// SEM n: Set Memory Bit
	ram_w(ram_r() | (1 << (m_op & 3)));
}

static void op_rem()
{
	// REM n: Reset Memory Bit
	ram_w(ram_r() & ~(1 << (m_op & 3)));
}

static void op_tm()
{
	// TM n: Test Memory Bit
	m_s = BIT(ram_r(), m_op & 3);
}


// ROM address instructions

static void op_br()
{
	// BR a: Branch on Status 1
	if (m_s)
		m_pc = (m_pc & ~0x3f) | (m_op & 0x3f);
	else
		m_s = 1;
}

static void op_cal()
{
	// CAL a: Subroutine Jump on Status 1
	if (m_s)
	{
		m_block_int = true;
		push_stack();
		m_pc = m_op & 0x3f; // short calls default to page 0
	}
	else
		m_s = 1;
}

static void op_lpu()
{
	// LPU u: Load Program Counter Upper on Status 1
	if (m_s)
	{
		m_block_int = true;
		m_pc_upper = m_op & 0x1f;

		// on HMCS46/47, also latches bank from R70
		if (m_family == HMCS46_FAMILY || m_family == HMCS47_FAMILY)
			m_pc_upper |= (~m_r[7] << 5) & 0x20;
	}
	else
		m_op |= 0x400; // indicate unhandled LPU
}

static void op_tbr()
{
	// TBR p: Table Branch
	UINT16 address = m_a | (m_b << 4) | (m_c << 8) | ((m_op & 7) << 9) | (m_pc & ~0x3f);
	m_pc = address & m_pcmask;
}

static void op_rtn()
{
	// RTN: Return from Subroutine
	pop_stack();
}


// interrupt instructions

static void op_seie()
{
	// SEIE: Set I/E
	m_ie = 1;
}

static void op_seif0()
{
	// SEIF0: Set IF0
	m_if[0] = 1;
}

static void op_seif1()
{
	// SEIF1: Set IF1
	m_if[1] = 1;
}

static void op_setf()
{
	// SETF: Set TF
	m_tf = 1;
}

static void op_secf()
{
	// SECF: Set CF
	m_cf = 1;
}

static void op_reie()
{
	// REIE: Reset I/E
	m_ie = 0;
}

static void op_reif0()
{
	// REIF0: Reset IF0
	m_if[0] = 0;
}

static void op_reif1()
{
	// REIF1: Reset IF1
	m_if[1] = 0;
}

static void op_retf()
{
	// RETF: Reset TF
	m_tf = 0;
}

static void op_recf()
{
	// RECF: Reset CF
	m_cf = 0;
}

static void op_ti0()
{
	// TI0: Test INT0
	m_s = m_int[0];
}

static void op_ti1()
{
	// TI1: Test INT1
	m_s = m_int[1];
}

static void op_tif0()
{
	// TIF0: Test IF0
	m_s = m_if[0];
}

static void op_tif1()
{
	// TIF1: Test IF1
	m_s = m_if[1];
}

static void op_ttf()
{
	// TTF: Test TF
	m_s = m_tf;
}

static void op_lti()
{
	// LTI i: Load Timer/Counter from Immediate
	m_tc = m_i;
	m_prescaler = 0;
}

static void op_lta()
{
	// LTA: Load Timer/Counter from A
	m_tc = m_a;
	m_prescaler = 0;
}

static void op_lat()
{
	// LAT: Load A from Timer/Counter
	m_a = m_tc;
}

static void op_rtni()
{
	// RTNI: Return from Interrupt
	op_seie();
	op_rtn();
}


// input/output instructions

static void op_sed()
{
	// SED: Set Discrete I/O Latch
	write_d(m_y, 1);
}

static void op_red()
{
	// RED: Reset Discrete I/O Latch
	write_d(m_y, 0);
}

static void op_td()
{
	// TD: Test Discrete I/O Latch
	m_s = read_d(m_y);
}

static void op_sedd()
{
	// SEDD n: Set Discrete I/O Latch Direct
	write_d(m_op & 3, 1);
}

static void op_redd()
{
	// REDD n: Reset Discrete I/O Latch Direct
	write_d(m_op & 3, 0);
}

static void op_lar()
{
	// LAR p: Load A from R-Port Register
	m_a = read_r(m_op & 7);
}

static void op_lbr()
{
	// LBR p: Load B from R-Port Register
	m_b = read_r(m_op & 7);
}

static void op_lra()
{
	// LRA p: Load R-Port Register from A
	write_r(m_op & 7, m_a);
}

static void op_lrb()
{
	// LRB p: Load R-Port Register from B
	write_r(m_op & 7, m_b);
}

static void op_p()
{
	// P p: Pattern Generation
	cycle();

	UINT16 address = m_a | (m_b << 4) | (m_c << 8) | ((m_op & 7) << 9) | (m_pc & ~0x3f);
	UINT16 data = program_read_word(address & m_prgmask);

	// destination is determined by the 2 highest bits
	if (data & 0x100)
	{
		// B3 B2 B1 B0 A0 A1 A2 A3
		m_a = BITSWAP04(data,0,1,2,3);
		m_b = (data >> 4) & 0xf;
	}
	if (data & 0x200)
	{
		// R20 R21 R22 R23 R30 R31 R32 R33
		data = BITSWAP08(data,0,1,2,3,4,5,6,7);
		write_r(2, data & 0xf);
		write_r(3, (data >> 4) & 0xf);
	}
}
