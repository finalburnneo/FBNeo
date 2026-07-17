// =============================================================================
//  FBNeo SNES  -  SuperFX (GSU) instruction set
// =============================================================================
//  Derived from ares/component/processor/gsu/instructions.cpp (ISC, see
//  license.txt).  ares implemented these as GSU member functions using nall
//  Register/BitField operator overloads; here they are file-scope statics
//  operating on the flat SnesGsuState.  Two structural notes:
//    * ares' packed SFR BitField assignment (`sfr.s = value & 0x8000`) coerces
//      the RHS to bool; the equivalent here writes an explicit 0/1 ((x)!=0) into
//      the expanded flag byte, since a bare store would truncate (e.g. a UINT8
//      taking 0x8000 would wrongly become 0).
//    * ares' `regs.dr() = v` / `regs.r[n] = v` go through Register::operator=,
//      which records the "modified" flag consulted for r14 (ROM buffering) and
//      r15 (branch suppression); snes_gsu_reg_write() preserves that exactly.
//
//  Included by gsu.cpp.  Not a standalone translation unit.
// =============================================================================

// decode POR from a source byte (ares POR::operator=)
static inline void snes_gsu_por_set(UINT8 d)
{
	gsu.por_obj = (d & 0x10) != 0;
	gsu.por_freezehigh = (d & 0x08) != 0;
	gsu.por_highnibble = (d & 0x04) != 0;
	gsu.por_dither = (d & 0x02) != 0;
	gsu.por_transparent = (d & 0x01) != 0;
}

//$00 stop
static void snes_gsu_i_stop()
{
	if (gsu.cfgr_irq == 0) {
		gsu.sfr_irq = 1;
		snes_gsu_stop();
	}
	gsu.sfr_g    = 0;
	gsu.pipeline = 0x01;  //nop
	snes_gsu_regs_reset();
}

//$01 nop
static void snes_gsu_i_nop()
{
	snes_gsu_regs_reset();
}

//$02 cache
static void snes_gsu_i_cache()
{
	if (gsu.cbr != (gsu.r[15] & 0xfff0)) {
		gsu.cbr = gsu.r[15] & 0xfff0;
		snes_gsu_cache_flush();
	}
	snes_gsu_regs_reset();
}

//$03 lsr
static void snes_gsu_i_lsr()
{
	gsu.sfr_cy = (SNES_GSU_SR() & 1);
	UINT16 d   = SNES_GSU_SR() >> 1;
	SNES_GSU_DR_WRITE(d);
	gsu.sfr_s  = (d & 0x8000) != 0;
	gsu.sfr_z  = (d == 0);
	snes_gsu_regs_reset();
}

//$04 rol
static void snes_gsu_i_rol()
{
	UINT8  carry = (SNES_GSU_SR() & 0x8000) != 0;
	UINT16 d   = (SNES_GSU_SR() << 1) | gsu.sfr_cy;
	SNES_GSU_DR_WRITE(d);
	gsu.sfr_s  = (d & 0x8000) != 0;
	gsu.sfr_cy = carry;
	gsu.sfr_z  = (d == 0);
	snes_gsu_regs_reset();
}

//$05-0f branches (bra/blt/bge/bne/beq/bpl/bmi/bcc/bcs/bvc/bvs)
static void snes_gsu_i_branch(int take)
{
	INT8 displacement = (INT8)snes_gsu_pipe();
	if (take) snes_gsu_reg_write(15, gsu.r[15] + displacement);
}

//$10-1f(b0) to rN  /  (b1) move rN
static void snes_gsu_i_to_move(UINT32 n)
{
	if (!gsu.sfr_b) {
		gsu.dreg = n;
	} else {
		snes_gsu_reg_write(n, SNES_GSU_SR());
		snes_gsu_regs_reset();
	}
}

//$20-2f with rN
static void snes_gsu_i_with(UINT32 n)
{
	gsu.sreg  = n;
	gsu.dreg  = n;
	gsu.sfr_b = 1;
}

//$30-3b(alt0) stw (rN)  /  (alt1) stb (rN)
static void snes_gsu_i_store(UINT32 n)
{
	gsu.ramaddr = gsu.r[n];
	snes_gsu_rambuffer_write(gsu.ramaddr, SNES_GSU_SR());
	if (!gsu.sfr_alt1) snes_gsu_rambuffer_write(gsu.ramaddr ^ 1, SNES_GSU_SR() >> 8);
	snes_gsu_regs_reset();
}

//$3c loop
static void snes_gsu_i_loop()
{
	UINT16 d  = gsu.r[12] - 1;
	snes_gsu_reg_write(12, d);
	gsu.sfr_s = (d & 0x8000) != 0;
	gsu.sfr_z = (d == 0);
	if (!gsu.sfr_z) snes_gsu_reg_write(15, gsu.r[13]);
	snes_gsu_regs_reset();
}

//$3d alt1
static void snes_gsu_i_alt1()
{
	gsu.sfr_b    = 0;
	gsu.sfr_alt1 = 1;
}

//$3e alt2
static void snes_gsu_i_alt2()
{
	gsu.sfr_b    = 0;
	gsu.sfr_alt2 = 1;
}

//$3f alt3
static void snes_gsu_i_alt3()
{
	gsu.sfr_b    = 0;
	gsu.sfr_alt1 = 1;
	gsu.sfr_alt2 = 1;
}

//$40-4b(alt0) ldw (rN)  /  (alt1) ldb (rN)
static void snes_gsu_i_load(UINT32 n)
{
	gsu.ramaddr = gsu.r[n];
	UINT16 d = snes_gsu_rambuffer_read(gsu.ramaddr);
	if (!gsu.sfr_alt1) d |= snes_gsu_rambuffer_read(gsu.ramaddr ^ 1) << 8;
	SNES_GSU_DR_WRITE(d);
	snes_gsu_regs_reset();
}

//$4c(alt0) plot  /  (alt1) rpix
static void snes_gsu_i_plot_rpix()
{
	if (!gsu.sfr_alt1) {
		snes_gsu_plot(gsu.r[1], gsu.r[2]);
		snes_gsu_reg_write(1, gsu.r[1] + 1);
	} else {
		UINT16 d  = snes_gsu_rpix(gsu.r[1], gsu.r[2]);
		SNES_GSU_DR_WRITE(d);
		gsu.sfr_s = (d & 0x8000) != 0;
		gsu.sfr_z = (d == 0);
	}
	snes_gsu_regs_reset();
}

//$4d swap
static void snes_gsu_i_swap()
{
	UINT16 d  = (SNES_GSU_SR() >> 8) | (SNES_GSU_SR() << 8);
	SNES_GSU_DR_WRITE(d);
	gsu.sfr_s = (d & 0x8000) != 0;
	gsu.sfr_z = (d == 0);
	snes_gsu_regs_reset();
}

//$4e(alt0) color  /  (alt1) cmode
static void snes_gsu_i_color_cmode()
{
	if (!gsu.sfr_alt1) {
		gsu.colr = snes_gsu_color(SNES_GSU_SR());
	} else {
		snes_gsu_por_set(SNES_GSU_SR());
	}
	snes_gsu_regs_reset();
}

//$4f not
static void snes_gsu_i_not()
{
	UINT16 d  = ~SNES_GSU_SR();
	SNES_GSU_DR_WRITE(d);
	gsu.sfr_s = (d & 0x8000) != 0;
	gsu.sfr_z = (d == 0);
	snes_gsu_regs_reset();
}

//$50-5f add/adc rN/#N
static void snes_gsu_i_add_adc(UINT32 n)
{
	UINT32 v   = n;
	if (!gsu.sfr_alt2) v = gsu.r[n];
	UINT16 s   = SNES_GSU_SR();
	INT32 r    = s + v + (gsu.sfr_alt1 ? gsu.sfr_cy : 0);
	gsu.sfr_ov = (~(s ^ v) & (v ^ r) & 0x8000) != 0;
	gsu.sfr_s  = (r & 0x8000) != 0;
	gsu.sfr_cy = (r >= 0x10000);
	gsu.sfr_z  = ((UINT16)r == 0);
	SNES_GSU_DR_WRITE(r);
	snes_gsu_regs_reset();
}

//$60-6f sub/sbc/cmp rN/#N
static void snes_gsu_i_sub_sbc_cmp(UINT32 n)
{
	UINT32 v   = n;
	if (!gsu.sfr_alt2 || gsu.sfr_alt1) v = gsu.r[n];
	UINT16 s   = SNES_GSU_SR();
	INT32 r    = s - v - ((!gsu.sfr_alt2 && gsu.sfr_alt1) ? !gsu.sfr_cy : 0);
	gsu.sfr_ov = ((s ^ v) & (s ^ r) & 0x8000) != 0;
	gsu.sfr_s  = (r & 0x8000) != 0;
	gsu.sfr_cy = (r >= 0);
	gsu.sfr_z  = ((UINT16)r == 0);
	if (!gsu.sfr_alt2 || !gsu.sfr_alt1) SNES_GSU_DR_WRITE(r);
	snes_gsu_regs_reset();
}

//$70 merge
static void snes_gsu_i_merge()
{
	UINT16 d   = (gsu.r[7] & 0xff00) | (gsu.r[8] >> 8);
	SNES_GSU_DR_WRITE(d);
	gsu.sfr_ov = (d & 0xc0c0) != 0;
	gsu.sfr_s  = (d & 0x8080) != 0;
	gsu.sfr_cy = (d & 0xe0e0) != 0;
	gsu.sfr_z  = (d & 0xf0f0) != 0;
	snes_gsu_regs_reset();
}

//$71-7f and/bic rN/#N
static void snes_gsu_i_and_bic(UINT32 n)
{
	UINT32 v  = n;
	if (!gsu.sfr_alt2) v = gsu.r[n];
	UINT16 d  = SNES_GSU_SR() & (gsu.sfr_alt1 ? ~v : v);
	SNES_GSU_DR_WRITE(d);
	gsu.sfr_s = (d & 0x8000) != 0;
	gsu.sfr_z = (d == 0);
	snes_gsu_regs_reset();
}

//$80-8f mult/umult rN/#N
static void snes_gsu_i_mult_umult(UINT32 n)
{
	UINT32 v  = n;
	if (!gsu.sfr_alt2) v = gsu.r[n];
	UINT16 s  = SNES_GSU_SR();
	UINT16 d  = (!gsu.sfr_alt1) ? (UINT16)((INT8)s * (INT8)v) : (UINT16)((UINT8)s * (UINT8)v);
	SNES_GSU_DR_WRITE(d);
	gsu.sfr_s = (d & 0x8000) != 0;
	gsu.sfr_z = (d == 0);
	snes_gsu_regs_reset();
	if (!gsu.cfgr_ms0) snes_gsu_step(gsu.clsr ? 1 : 2);
}

//$90 sbk
static void snes_gsu_i_sbk()
{
	snes_gsu_rambuffer_write(gsu.ramaddr ^ 0, SNES_GSU_SR() >> 0);
	snes_gsu_rambuffer_write(gsu.ramaddr ^ 1, SNES_GSU_SR() >> 8);
	snes_gsu_regs_reset();
}

//$91-94 link #N
static void snes_gsu_i_link(UINT32 n)
{
	snes_gsu_reg_write(11, gsu.r[15] + n);
	snes_gsu_regs_reset();
}

//$95 sex
static void snes_gsu_i_sex()
{
	UINT16 d  = (INT8)SNES_GSU_SR();
	SNES_GSU_DR_WRITE(d);
	gsu.sfr_s = (d & 0x8000) != 0;
	gsu.sfr_z = (d == 0);
	snes_gsu_regs_reset();
}

//$96(alt0) asr  /  (alt1) div2
static void snes_gsu_i_asr_div2()
{
	gsu.sfr_cy = (SNES_GSU_SR() & 1);
	UINT16 d   = ((INT16)SNES_GSU_SR() >> 1) + (gsu.sfr_alt1 ? ((SNES_GSU_SR() + 1) >> 16) : 0);
	SNES_GSU_DR_WRITE(d);
	gsu.sfr_s  = (d & 0x8000) != 0;
	gsu.sfr_z  = (d == 0);
	snes_gsu_regs_reset();
}

//$97 ror
static void snes_gsu_i_ror()
{
	UINT8  carry = (SNES_GSU_SR() & 1);
	UINT16 d   = (gsu.sfr_cy << 15) | (SNES_GSU_SR() >> 1);
	SNES_GSU_DR_WRITE(d);
	gsu.sfr_s  = (d & 0x8000) != 0;
	gsu.sfr_cy = carry;
	gsu.sfr_z  = (d == 0);
	snes_gsu_regs_reset();
}

//$98-9d(alt0) jmp rN  /  (alt1) ljmp rN
static void snes_gsu_i_jmp_ljmp(UINT32 n)
{
	if (!gsu.sfr_alt1) {
		snes_gsu_reg_write(15, gsu.r[n]);
	} else {
		gsu.pbr = gsu.r[n] & 0x7f;
		snes_gsu_reg_write(15, SNES_GSU_SR());
		gsu.cbr = gsu.r[15] & 0xfff0;
		snes_gsu_cache_flush();
	}
	snes_gsu_regs_reset();
}

//$9e lob
static void snes_gsu_i_lob()
{
	UINT16 d  = SNES_GSU_SR() & 0xff;
	SNES_GSU_DR_WRITE(d);
	gsu.sfr_s = (d & 0x80) != 0;
	gsu.sfr_z = (d == 0);
	snes_gsu_regs_reset();
}

//$9f(alt0) fmult  /  (alt1) lmult
static void snes_gsu_i_fmult_lmult()
{
	UINT32 result = (UINT32)((INT16)SNES_GSU_SR() * (INT16)gsu.r[6]);
	if (gsu.sfr_alt1) snes_gsu_reg_write(4, result);
	UINT16 d   = result >> 16;
	SNES_GSU_DR_WRITE(d);
	gsu.sfr_s  = (d & 0x8000) != 0;
	gsu.sfr_cy = (result & 0x8000) != 0;
	gsu.sfr_z  = (d == 0);
	snes_gsu_regs_reset();
	snes_gsu_step((gsu.cfgr_ms0 ? 3 : 7) * (gsu.clsr ? 1 : 2));
}

//$a0-af ibt rN,#pp / lms rN,(yy) / sms (yy),rN
static void snes_gsu_i_ibt_lms_sms(UINT32 n)
{
	if (gsu.sfr_alt1) {
		gsu.ramaddr = snes_gsu_pipe() << 1;
		UINT8 lo    = snes_gsu_rambuffer_read(gsu.ramaddr ^ 0);
		snes_gsu_reg_write(n, snes_gsu_rambuffer_read(gsu.ramaddr ^ 1) << 8 | lo);
	} else if (gsu.sfr_alt2) {
		gsu.ramaddr = snes_gsu_pipe() << 1;
		snes_gsu_rambuffer_write(gsu.ramaddr ^ 0, gsu.r[n] >> 0);
		snes_gsu_rambuffer_write(gsu.ramaddr ^ 1, gsu.r[n] >> 8);
	} else {
		snes_gsu_reg_write(n, (INT8)snes_gsu_pipe());
	}
	snes_gsu_regs_reset();
}

//$b0-bf(b0) from rN  /  (b1) moves rN
static void snes_gsu_i_from_moves(UINT32 n)
{
	if (!gsu.sfr_b) {
		gsu.sreg   = n;
	} else {
		UINT16 d   = gsu.r[n];
		SNES_GSU_DR_WRITE(d);
		gsu.sfr_ov = (d & 0x80) != 0;
		gsu.sfr_s  = (d & 0x8000) != 0;
		gsu.sfr_z  = (d == 0);
		snes_gsu_regs_reset();
	}
}

//$c0 hib
static void snes_gsu_i_hib()
{
	UINT16 d  = SNES_GSU_SR() >> 8;
	SNES_GSU_DR_WRITE(d);
	gsu.sfr_s = (d & 0x80) != 0;
	gsu.sfr_z = (d == 0);
	snes_gsu_regs_reset();
}

//$c1-cf or/xor rN/#N
static void snes_gsu_i_or_xor(UINT32 n)
{
	UINT32 v  = n;
	if (!gsu.sfr_alt2) v = gsu.r[n];
	UINT16 d  = (!gsu.sfr_alt1) ? (SNES_GSU_SR() | v) : (SNES_GSU_SR() ^ v);
	SNES_GSU_DR_WRITE(d);
	gsu.sfr_s = (d & 0x8000) != 0;
	gsu.sfr_z = (d == 0);
	snes_gsu_regs_reset();
}

//$d0-de inc rN
static void snes_gsu_i_inc(UINT32 n)
{
	UINT16 d  = gsu.r[n] + 1;
	snes_gsu_reg_write(n, d);
	gsu.sfr_s = (d & 0x8000) != 0;
	gsu.sfr_z = (d == 0);
	snes_gsu_regs_reset();
}

//$df(alt0) getc  /  (alt2) ramb  /  (alt3) romb
static void snes_gsu_i_getc_ramb_romb()
{
	if (!gsu.sfr_alt2) {
		gsu.colr  = snes_gsu_color(snes_gsu_rombuffer_read());
	} else if (!gsu.sfr_alt1) {
		snes_gsu_rambuffer_sync();
		gsu.rambr = SNES_GSU_SR() & 0x01;
	} else {
		snes_gsu_rombuffer_sync();
		gsu.rombr = SNES_GSU_SR() & 0x7f;
	}
	snes_gsu_regs_reset();
}

//$e0-ee dec rN
static void snes_gsu_i_dec(UINT32 n)
{
	UINT16 d  = gsu.r[n] - 1;
	snes_gsu_reg_write(n, d);
	gsu.sfr_s = (d & 0x8000) != 0;
	gsu.sfr_z = (d == 0);
	snes_gsu_regs_reset();
}

//$ef(alt0) getb / (alt1) getbh / (alt2) getbl / (alt3) getbs
static void snes_gsu_i_getb()
{
	UINT16 d;
	switch ((gsu.sfr_alt2 << 1) | (gsu.sfr_alt1 << 0)) {
		case 0:  d = snes_gsu_rombuffer_read();                             break;
		case 1:  d = snes_gsu_rombuffer_read() << 8 | (UINT8)SNES_GSU_SR(); break;
		case 2:  d = (SNES_GSU_SR() & 0xff00) | snes_gsu_rombuffer_read();  break;
		default: d = (INT8)snes_gsu_rombuffer_read();                       break;	// case 3
	}
	SNES_GSU_DR_WRITE(d);
	snes_gsu_regs_reset();
}

//$f0-ff iwt rN,#xx / lm rN,(xx) / sm (xx),rN
static void snes_gsu_i_iwt_lm_sm(UINT32 n)
{
	if (gsu.sfr_alt1) {
		gsu.ramaddr  = snes_gsu_pipe() << 0;
		gsu.ramaddr |= snes_gsu_pipe() << 8;
		UINT8 lo = snes_gsu_rambuffer_read(gsu.ramaddr ^ 0);
		snes_gsu_reg_write(n, snes_gsu_rambuffer_read(gsu.ramaddr ^ 1) << 8 | lo);
	} else if (gsu.sfr_alt2) {
		gsu.ramaddr  = snes_gsu_pipe() << 0;
		gsu.ramaddr |= snes_gsu_pipe() << 8;
		snes_gsu_rambuffer_write(gsu.ramaddr ^ 0, gsu.r[n] >> 0);
		snes_gsu_rambuffer_write(gsu.ramaddr ^ 1, gsu.r[n] >> 8);
	} else {
		UINT8 lo     = snes_gsu_pipe();
		snes_gsu_reg_write(n, snes_gsu_pipe() << 8 | lo);
	}
	snes_gsu_regs_reset();
}
