// =============================================================================
//  FBNeo SNES  -  SuperFX (GSU) opcode dispatch
// =============================================================================
//  Derived from ares/component/processor/gsu/instruction.cpp (ISC, see
//  license.txt).  ares dispatched with a preprocessor-expanded switch (op/op4/
//  op6/op12/op15/op16 macros) over templated member functions; the identical
//  grouping is reproduced here over the file-scope snes_gsu_i_* statics, passing
//  the low nibble of the opcode as the explicit register index.
//
//  Unlike mercury's 1024-entry ALT-prefixed table, ALT0..3 decoding stays inside
//  each instruction (it reads gsu.sfr_alt1/alt2), matching ares 1:1 - so this is
//  a flat switch over the raw 8-bit opcode.
//
//  Included by gsu.cpp.  Not a standalone translation unit.
// =============================================================================

static void snes_gsu_execute(UINT8 opcode) {
#define op(id, call)  case id: call; return;
#define op4(id, fn)								\
	case (id)+ 0: fn((opcode) & 15); return;	\
	case (id)+ 1: fn((opcode) & 15); return;	\
	case (id)+ 2: fn((opcode) & 15); return;	\
	case (id)+ 3: fn((opcode) & 15); return;
#define op6(id, fn)								\
	op4(id, fn)									\
	case (id)+ 4: fn((opcode) & 15); return;	\
	case (id)+ 5: fn((opcode) & 15); return;
#define op12(id, fn)							\
	op6(id, fn)									\
	case (id)+ 6: fn((opcode) & 15); return;	\
	case (id)+ 7: fn((opcode) & 15); return;	\
	case (id)+ 8: fn((opcode) & 15); return;	\
	case (id)+ 9: fn((opcode) & 15); return;	\
	case (id)+10: fn((opcode) & 15); return;	\
	case (id)+11: fn((opcode) & 15); return;
#define op15(id, fn)							\
	op12(id, fn)								\
	case (id)+12: fn((opcode) & 15); return;	\
	case (id)+13: fn((opcode) & 15); return;	\
	case (id)+14: fn((opcode) & 15); return;
#define op16(id, fn)							\
	op15(id, fn)								\
	case (id)+15: fn((opcode) & 15); return;

	switch (opcode) {
		op(  0x00, snes_gsu_i_stop())
		op(  0x01, snes_gsu_i_nop())
		op(  0x02, snes_gsu_i_cache())
		op(  0x03, snes_gsu_i_lsr())
		op(  0x04, snes_gsu_i_rol())
		op(  0x05, snes_gsu_i_branch(1))								//bra
		op(  0x06, snes_gsu_i_branch((gsu.sfr_s ^ gsu.sfr_ov) == 0))	//blt
		op(  0x07, snes_gsu_i_branch((gsu.sfr_s ^ gsu.sfr_ov) == 1))	//bge
		op(  0x08, snes_gsu_i_branch(gsu.sfr_z  == 0))					//bne
		op(  0x09, snes_gsu_i_branch(gsu.sfr_z  == 1))					//beq
		op(  0x0a, snes_gsu_i_branch(gsu.sfr_s  == 0))					//bpl
		op(  0x0b, snes_gsu_i_branch(gsu.sfr_s  == 1))					//bmi
		op(  0x0c, snes_gsu_i_branch(gsu.sfr_cy == 0))					//bcc
		op(  0x0d, snes_gsu_i_branch(gsu.sfr_cy == 1))					//bcs
		op(  0x0e, snes_gsu_i_branch(gsu.sfr_ov == 0))					//bvc
		op(  0x0f, snes_gsu_i_branch(gsu.sfr_ov == 1))					//bvs
		op16(0x10, snes_gsu_i_to_move)
		op16(0x20, snes_gsu_i_with)
		op12(0x30, snes_gsu_i_store)
		op(  0x3c, snes_gsu_i_loop())
		op(  0x3d, snes_gsu_i_alt1())
		op(  0x3e, snes_gsu_i_alt2())
		op(  0x3f, snes_gsu_i_alt3())
		op12(0x40, snes_gsu_i_load)
		op(  0x4c, snes_gsu_i_plot_rpix())
		op(  0x4d, snes_gsu_i_swap())
		op(  0x4e, snes_gsu_i_color_cmode())
		op(  0x4f, snes_gsu_i_not())
		op16(0x50, snes_gsu_i_add_adc)
		op16(0x60, snes_gsu_i_sub_sbc_cmp)
		op(  0x70, snes_gsu_i_merge())
		op15(0x71, snes_gsu_i_and_bic)
		op16(0x80, snes_gsu_i_mult_umult)
		op(  0x90, snes_gsu_i_sbk())
		op4( 0x91, snes_gsu_i_link)
		op(  0x95, snes_gsu_i_sex())
		op(  0x96, snes_gsu_i_asr_div2())
		op(  0x97, snes_gsu_i_ror())
		op6( 0x98, snes_gsu_i_jmp_ljmp)
		op(  0x9e, snes_gsu_i_lob())
		op(  0x9f, snes_gsu_i_fmult_lmult())
		op16(0xa0, snes_gsu_i_ibt_lms_sms)
		op16(0xb0, snes_gsu_i_from_moves)
		op(  0xc0, snes_gsu_i_hib())
		op15(0xc1, snes_gsu_i_or_xor)
		op15(0xd0, snes_gsu_i_inc)
		op(  0xdf, snes_gsu_i_getc_ramb_romb())
		op15(0xe0, snes_gsu_i_dec)
		op(  0xef, snes_gsu_i_getb())
		op16(0xf0, snes_gsu_i_iwt_lm_sm)
	}

#undef op
#undef op4
#undef op6
#undef op12
#undef op15
#undef op16
}
