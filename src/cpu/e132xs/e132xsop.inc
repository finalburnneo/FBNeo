#define LOCAL_DECODE_INIT \
	struct regs_decode decode_state; \
	struct regs_decode *decode = &decode_state; \
\
	/* clear 'current regs / flags' */ \
	decode->src = 0; \
	decode->dst = 0; \
	decode->src_value = 0; \
	decode->next_src_value = 0; \
	decode->dst_value = 0; \
	decode->next_dst_value = 0; \
	decode->sub_type = 0; \
	decode->extra.u = 0; \
	decode->src_is_local = 0; \
	decode->dst_is_local = 0; \
	decode->same_src_dst = 0; \
	decode->same_src_dstf = 0; \
	decode->same_srcf_dst = 0;

static void op00()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 0, 0);
	hyperstone_chk(decode);
}

static void op01()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 0, 1);
	hyperstone_chk(decode);
}

static void op02()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 1, 0);
	hyperstone_chk(decode);
}

static void op03()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 1, 1);
	hyperstone_chk(decode);
}

static void op04()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 0, 0);
	hyperstone_movd(decode);
}

static void op05()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 0, 1);
	hyperstone_movd(decode);
}

static void op06()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 1, 0);
	hyperstone_movd(decode);
}

static void op07()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 1, 1);
	hyperstone_movd(decode);
}

static void op08()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 0, 0);
	hyperstone_divu(decode);
}

static void op09()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 0, 1);
	hyperstone_divu(decode);
}

static void op0a()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 1, 0);
	hyperstone_divu(decode);
}

static void op0b()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 1, 1);
	hyperstone_divu(decode);
}

static void op0c()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 0, 0);
	hyperstone_divs(decode);
}

static void op0d()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 0, 1);
	hyperstone_divs(decode);
}

static void op0e()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 1, 0);
	hyperstone_divs(decode);
}

static void op0f()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 1, 1);
	hyperstone_divs(decode);
}



static void op10()
{
	LOCAL_DECODE_INIT;
	RRlimdecode(decode, 0, 0);
	hyperstone_xm(decode);
}

static void op11()
{
	LOCAL_DECODE_INIT;
	RRlimdecode(decode, 0, 1);
	hyperstone_xm(decode);
}

static void op12()
{
	LOCAL_DECODE_INIT;
	RRlimdecode(decode, 1, 0);
	hyperstone_xm(decode);
}

static void op13()
{
	LOCAL_DECODE_INIT;
	RRlimdecode(decode, 1, 1);
	hyperstone_xm(decode);
}

static void op14()
{
	LOCAL_DECODE_INIT;
	RRconstdecode(decode, 0, 0);
	hyperstone_mask(decode);
}

static void op15()
{
	LOCAL_DECODE_INIT;
	RRconstdecode(decode, 0, 1);
	hyperstone_mask(decode);
}

static void op16()
{
	LOCAL_DECODE_INIT;
	RRconstdecode(decode, 1, 0);
	hyperstone_mask(decode);
}

static void op17()
{
	LOCAL_DECODE_INIT;
	RRconstdecode(decode, 1, 1);
	hyperstone_mask(decode);
}

static void op18()
{
	LOCAL_DECODE_INIT;
	RRconstdecode(decode, 0, 0);
	hyperstone_sum(decode);
}

static void op19()
{
	LOCAL_DECODE_INIT;
	RRconstdecode(decode, 0, 1);
	hyperstone_sum(decode);
}

static void op1a()
{
	LOCAL_DECODE_INIT;
	RRconstdecode(decode, 1, 0);
	hyperstone_sum(decode);
}

static void op1b()
{
	LOCAL_DECODE_INIT;
	RRconstdecode(decode, 1, 1);
	hyperstone_sum(decode);
}

static void op1c()
{
	LOCAL_DECODE_INIT;
	RRconstdecode(decode, 0, 0);
	hyperstone_sums(decode);
}

static void op1d()
{
	LOCAL_DECODE_INIT;
	RRconstdecode(decode, 0, 1);
	hyperstone_sums(decode);
}

static void op1e()
{
	LOCAL_DECODE_INIT;
	RRconstdecode(decode, 1, 0);
	hyperstone_sums(decode);
}

static void op1f()
{
	LOCAL_DECODE_INIT;
	RRconstdecode(decode, 1, 1);
	hyperstone_sums(decode);
}



static void op20()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 0, 0);
	hyperstone_cmp(decode);
}

static void op21()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 0, 1);
	hyperstone_cmp(decode);
}

static void op22()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 1, 0);
	hyperstone_cmp(decode);
}

static void op23()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 1, 1);
	hyperstone_cmp(decode);
}

static void op24()
{
	LOCAL_DECODE_INIT;
	RRdecodewithHflag(decode, 0, 0);
	hyperstone_mov(decode);
}

static void op25()
{
	LOCAL_DECODE_INIT;
	RRdecodewithHflag(decode, 0, 1);
	hyperstone_mov(decode);
}

static void op26()
{
	LOCAL_DECODE_INIT;
	RRdecodewithHflag(decode, 1, 0);
	hyperstone_mov(decode);
}

static void op27()
{
	LOCAL_DECODE_INIT;
	RRdecodewithHflag(decode, 1, 1);
	hyperstone_mov(decode);
}

static void op28()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 0, 0);
	hyperstone_add(decode);
}

static void op29()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 0, 1);
	hyperstone_add(decode);
}

static void op2a()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 1, 0);
	hyperstone_add(decode);
}

static void op2b()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 1, 1);
	hyperstone_add(decode);
}

static void op2c()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 0, 0);
	hyperstone_adds(decode);
}

static void op2d()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 0, 1);
	hyperstone_adds(decode);
}

static void op2e()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 1, 0);
	hyperstone_adds(decode);
}

static void op2f()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 1, 1);
	hyperstone_adds(decode);
}



static void op30()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 0, 0);
	hyperstone_cmpb(decode);
}

static void op31()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 0, 1);
	hyperstone_cmpb(decode);
}

static void op32()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 1, 0);
	hyperstone_cmpb(decode);
}

static void op33()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 1, 1);
	hyperstone_cmpb(decode);
}

static void op34()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 0, 0);
	hyperstone_andn(decode);
}

static void op35()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 0, 1);
	hyperstone_andn(decode);
}

static void op36()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 1, 0);
	hyperstone_andn(decode);
}

static void op37()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 1, 1);
	hyperstone_andn(decode);
}

static void op38()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 0, 0);
	hyperstone_or(decode);
}

static void op39()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 0, 1);
	hyperstone_or(decode);
}

static void op3a()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 1, 0);
	hyperstone_or(decode);
}

static void op3b()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 1, 1);
	hyperstone_or(decode);
}

static void op3c()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 0, 0);
	hyperstone_xor(decode);
}

static void op3d()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 0, 1);
	hyperstone_xor(decode);
}

static void op3e()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 1, 0);
	hyperstone_xor(decode);
}

static void op3f()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 1, 1);
	hyperstone_xor(decode);
}



static void op40()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 0, 0);
	hyperstone_subc(decode);
}

static void op41()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 0, 1);
	hyperstone_subc(decode);
}

static void op42()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 1, 0);
	hyperstone_subc(decode);
}

static void op43()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 1, 1);
	hyperstone_subc(decode);
}

static void op44()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 0, 0);
	hyperstone_not(decode);
}

static void op45()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 0, 1);
	hyperstone_not(decode);
}

static void op46()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 1, 0);
	hyperstone_not(decode);
}

static void op47()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 1, 1);
	hyperstone_not(decode);
}

static void op48()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 0, 0);
	hyperstone_sub(decode);
}

static void op49()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 0, 1);
	hyperstone_sub(decode);
}

static void op4a()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 1, 0);
	hyperstone_sub(decode);
}

static void op4b()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 1, 1);
	hyperstone_sub(decode);
}

static void op4c()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 0, 0);
	hyperstone_subs(decode);
}

static void op4d()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 0, 1);
	hyperstone_subs(decode);
}

static void op4e()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 1, 0);
	hyperstone_subs(decode);
}

static void op4f()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 1, 1);
	hyperstone_subs(decode);
}



static void op50()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 0, 0);
	hyperstone_addc(decode);
}

static void op51()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 0, 1);
	hyperstone_addc(decode);
}

static void op52()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 1, 0);
	hyperstone_addc(decode);
}

static void op53()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 1, 1);
	hyperstone_addc(decode);
}

static void op54()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 0, 0);
	hyperstone_and(decode);
}

static void op55()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 0, 1);
	hyperstone_and(decode);
}

static void op56()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 1, 0);
	hyperstone_and(decode);
}

static void op57()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 1, 1);
	hyperstone_and(decode);
}

static void op58()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 0, 0);
	hyperstone_neg(decode);
}

static void op59()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 0, 1);
	hyperstone_neg(decode);
}

static void op5a()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 1, 0);
	hyperstone_neg(decode);
}

static void op5b()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 1, 1);
	hyperstone_neg(decode);
}

static void op5c()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 0, 0);
	hyperstone_negs(decode);
}

static void op5d()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 0, 1);
	hyperstone_negs(decode);
}

static void op5e()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 1, 0);
	hyperstone_negs(decode);
}

static void op5f()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 1, 1);
	hyperstone_negs(decode);
}



static void op60()
{
	LOCAL_DECODE_INIT;
	Rimmdecode(decode, 0, 0);
	hyperstone_cmpi(decode);
}

static void op61()
{
	LOCAL_DECODE_INIT;
	Rimmdecode(decode, 0, 1);
	hyperstone_cmpi(decode);
}

static void op62()
{
	LOCAL_DECODE_INIT;
	Rimmdecode(decode, 1, 0);
	hyperstone_cmpi(decode);
}

static void op63()
{
	LOCAL_DECODE_INIT;
	Rimmdecode(decode, 1, 1);
	hyperstone_cmpi(decode);
}

static void op64()
{
	LOCAL_DECODE_INIT;
	RimmdecodewithHflag(decode, 0, 0);
	hyperstone_movi(decode);
}

static void op65()
{
	LOCAL_DECODE_INIT;
	RimmdecodewithHflag(decode, 0, 1);
	hyperstone_movi(decode);
}

static void op66()
{
	LOCAL_DECODE_INIT;
	RimmdecodewithHflag(decode, 1, 0);
	hyperstone_movi(decode);
}

static void op67()
{
	LOCAL_DECODE_INIT;
	RimmdecodewithHflag(decode, 1, 1);
	hyperstone_movi(decode);
}

static void op68()
{
	LOCAL_DECODE_INIT;
	Rimmdecode(decode, 0, 0);
	hyperstone_addi(decode);
}

static void op69()
{
	LOCAL_DECODE_INIT;
	Rimmdecode(decode, 0, 1);
	hyperstone_addi(decode);
}

static void op6a()
{
	LOCAL_DECODE_INIT;
	Rimmdecode(decode, 1, 0);
	hyperstone_addi(decode);
}

static void op6b()
{
	LOCAL_DECODE_INIT;
	Rimmdecode(decode, 1, 1);
	hyperstone_addi(decode);
}

static void op6c()
{
	LOCAL_DECODE_INIT;
	Rimmdecode(decode, 0, 0);
	hyperstone_addsi(decode);
}

static void op6d()
{
	LOCAL_DECODE_INIT;
	Rimmdecode(decode, 0, 1);
	hyperstone_addsi(decode);
}

static void op6e()
{
	LOCAL_DECODE_INIT;
	Rimmdecode(decode, 1, 0);
	hyperstone_addsi(decode);
}

static void op6f()
{
	LOCAL_DECODE_INIT;
	Rimmdecode(decode, 1, 1);
	hyperstone_addsi(decode);
}



static void op70()
{
	LOCAL_DECODE_INIT;
	Rimmdecode(decode, 0, 0);
	hyperstone_cmpbi(decode);
}

static void op71()
{
	LOCAL_DECODE_INIT;
	Rimmdecode(decode, 0, 1);
	hyperstone_cmpbi(decode);
}

static void op72()
{
	LOCAL_DECODE_INIT;
	Rimmdecode(decode, 1, 0);
	hyperstone_cmpbi(decode);
}

static void op73()
{
	LOCAL_DECODE_INIT;
	Rimmdecode(decode, 1, 1);
	hyperstone_cmpbi(decode);
}

static void op74()
{
	LOCAL_DECODE_INIT;
	Rimmdecode(decode, 0, 0);
	hyperstone_andni(decode);
}

static void op75()
{
	LOCAL_DECODE_INIT;
	Rimmdecode(decode, 0, 1);
	hyperstone_andni(decode);
}

static void op76()
{
	LOCAL_DECODE_INIT;
	Rimmdecode(decode, 1, 0);
	hyperstone_andni(decode);
}

static void op77()
{
	LOCAL_DECODE_INIT;
	Rimmdecode(decode, 1, 1);
	hyperstone_andni(decode);
}

static void op78()
{
	LOCAL_DECODE_INIT;
	Rimmdecode(decode, 0, 0);
	hyperstone_ori(decode);
}

static void op79()
{
	LOCAL_DECODE_INIT;
	Rimmdecode(decode, 0, 1);
	hyperstone_ori(decode);
}

static void op7a()
{
	LOCAL_DECODE_INIT;
	Rimmdecode(decode, 1, 0);
	hyperstone_ori(decode);
}

static void op7b()
{
	LOCAL_DECODE_INIT;
	Rimmdecode(decode, 1, 1);
	hyperstone_ori(decode);
}

static void op7c()
{
	LOCAL_DECODE_INIT;
	Rimmdecode(decode, 0, 0);
	hyperstone_xori(decode);
}

static void op7d()
{
	LOCAL_DECODE_INIT;
	Rimmdecode(decode, 0, 1);
	hyperstone_xori(decode);
}

static void op7e()
{
	LOCAL_DECODE_INIT;
	Rimmdecode(decode, 1, 0);
	hyperstone_xori(decode);
}

static void op7f()
{
	LOCAL_DECODE_INIT;
	Rimmdecode(decode, 1, 1);
	hyperstone_xori(decode);
}



static void op80()
{
	LOCAL_DECODE_INIT;
	Lndecode(decode);
	hyperstone_shrdi(decode);
}

static void op81()
{
	LOCAL_DECODE_INIT;
	Lndecode(decode);
	hyperstone_shrdi(decode);
}

static void op82()
{
	LOCAL_DECODE_INIT;
	LLdecode(decode);
	hyperstone_shrd(decode);
}

static void op83()
{
	LOCAL_DECODE_INIT;
	LLdecode(decode);
	hyperstone_shr(decode);
}

static void op84()
{
	LOCAL_DECODE_INIT;
	Lndecode(decode);
	hyperstone_sardi(decode);
}

static void op85()
{
	LOCAL_DECODE_INIT;
	Lndecode(decode);
	hyperstone_sardi(decode);
}

static void op86()
{
	LOCAL_DECODE_INIT;
	LLdecode(decode);
	hyperstone_sard(decode);
}

static void op87()
{
	LOCAL_DECODE_INIT;
	LLdecode(decode);
	hyperstone_sar(decode);
}

static void op88()
{
	LOCAL_DECODE_INIT;
	Lndecode(decode);
	hyperstone_shldi(decode);
}

static void op89()
{
	LOCAL_DECODE_INIT;
	Lndecode(decode);
	hyperstone_shldi(decode);
}

static void op8a()
{
	LOCAL_DECODE_INIT;
	LLdecode(decode);
	hyperstone_shld(decode);
}

static void op8b()
{
	LOCAL_DECODE_INIT;
	LLdecode(decode);
	hyperstone_shl(decode);
}

static void op8c()
{
	LOCAL_DECODE_INIT;
	no_decode(decode);
	reserved(decode);
}

static void op8d()
{
	LOCAL_DECODE_INIT;
	no_decode(decode);
	reserved(decode);
}

static void op8e()
{
	LOCAL_DECODE_INIT;
	LLdecode(decode);
	hyperstone_testlz(decode);
}

static void op8f()
{
	LOCAL_DECODE_INIT;
	LLdecode(decode);
	hyperstone_rol(decode);
}



static void op90()
{
	LOCAL_DECODE_INIT;
	RRdisdecode(decode, 0, 0);
	hyperstone_ldxx1(decode);
}

static void op91()
{
	LOCAL_DECODE_INIT;
	RRdisdecode(decode, 0, 1);
	hyperstone_ldxx1(decode);
}

static void op92()
{
	LOCAL_DECODE_INIT;
	RRdisdecode(decode, 1, 0);
	hyperstone_ldxx1(decode);
}

static void op93()
{
	LOCAL_DECODE_INIT;
	RRdisdecode(decode, 1, 1);
	hyperstone_ldxx1(decode);
}

static void op94()
{
	LOCAL_DECODE_INIT;
	RRdisdecode(decode, 0, 0);
	hyperstone_ldxx2(decode);
}

static void op95()
{
	LOCAL_DECODE_INIT;
	RRdisdecode(decode, 0, 1);
	hyperstone_ldxx2(decode);
}

static void op96()
{
	LOCAL_DECODE_INIT;
	RRdisdecode(decode, 1, 0);
	hyperstone_ldxx2(decode);
}

static void op97()
{
	LOCAL_DECODE_INIT;
	RRdisdecode(decode, 1, 1);
	hyperstone_ldxx2(decode);
}

static void op98()
{
	LOCAL_DECODE_INIT;
	RRdisdecode(decode, 0, 0);
	hyperstone_stxx1(decode);
}

static void op99()
{
	LOCAL_DECODE_INIT;
	RRdisdecode(decode, 0, 1);
	hyperstone_stxx1(decode);
}

static void op9a()
{
	LOCAL_DECODE_INIT;
	RRdisdecode(decode, 1, 0);
	hyperstone_stxx1(decode);
}

static void op9b()
{
	LOCAL_DECODE_INIT;
	RRdisdecode(decode, 1, 1);
	hyperstone_stxx1(decode);
}

static void op9c()
{
	LOCAL_DECODE_INIT;
	RRdisdecode(decode, 0, 0);
	hyperstone_stxx2(decode);
}

static void op9d()
{
	LOCAL_DECODE_INIT;
	RRdisdecode(decode, 0, 1);
	hyperstone_stxx2(decode);
}

static void op9e()
{
	LOCAL_DECODE_INIT;
	RRdisdecode(decode, 1, 0);
	hyperstone_stxx2(decode);
}

static void op9f()
{
	LOCAL_DECODE_INIT;
	RRdisdecode(decode, 1, 1);
	hyperstone_stxx2(decode);
}



static void opa0()
{
	LOCAL_DECODE_INIT;
	Rndecode(decode, 0);
	hyperstone_shri(decode);
}

static void opa1()
{
	LOCAL_DECODE_INIT;
	Rndecode(decode, 0);
	hyperstone_shri(decode);
}

static void opa2()
{
	LOCAL_DECODE_INIT;
	Rndecode(decode, 1);
	hyperstone_shri(decode);
}

static void opa3()
{
	LOCAL_DECODE_INIT;
	Rndecode(decode, 1);
	hyperstone_shri(decode);
}

static void opa4()
{
	LOCAL_DECODE_INIT;
	Rndecode(decode, 0);
	hyperstone_sari(decode);
}

static void opa5()
{
	LOCAL_DECODE_INIT;
	Rndecode(decode, 0);
	hyperstone_sari(decode);
}

static void opa6()
{
	LOCAL_DECODE_INIT;
	Rndecode(decode, 1);
	hyperstone_sari(decode);
}

static void opa7()
{
	LOCAL_DECODE_INIT;
	Rndecode(decode, 1);
	hyperstone_sari(decode);
}

static void opa8()
{
	LOCAL_DECODE_INIT;
	Rndecode(decode, 0);
	hyperstone_shli(decode);
}

static void opa9()
{
	LOCAL_DECODE_INIT;
	Rndecode(decode, 0);
	hyperstone_shli(decode);
}

static void opaa()
{
	LOCAL_DECODE_INIT;
	Rndecode(decode, 1);
	hyperstone_shli(decode);
}

static void opab()
{
	LOCAL_DECODE_INIT;
	Rndecode(decode, 1);
	hyperstone_shli(decode);
}

static void opac()
{
	LOCAL_DECODE_INIT;
	no_decode(decode);
	reserved(decode);
}

static void opad()
{
	LOCAL_DECODE_INIT;
	no_decode(decode);
	reserved(decode);
}

static void opae()
{
	LOCAL_DECODE_INIT;
	no_decode(decode);
	reserved(decode);
}

static void opaf()
{
	LOCAL_DECODE_INIT;
	no_decode(decode);
	reserved(decode);
}



static void opb0()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 0, 0);
	hyperstone_mulu(decode);
}

static void opb1()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 0, 1);
	hyperstone_mulu(decode);
}

static void opb2()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 1, 0);
	hyperstone_mulu(decode);
}

static void opb3()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 1, 1);
	hyperstone_mulu(decode);
}

static void opb4()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 0, 0);
	hyperstone_muls(decode);
}

static void opb5()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 0, 1);
	hyperstone_muls(decode);
}

static void opb6()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 1, 0);
	hyperstone_muls(decode);
}

static void opb7()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 1, 1);
	hyperstone_muls(decode);
}

static void opb8()
{
	LOCAL_DECODE_INIT;
	Rndecode(decode, 0);
	hyperstone_set(decode);
}

static void opb9()
{
	LOCAL_DECODE_INIT;
	Rndecode(decode, 0);
	hyperstone_set(decode);
}

static void opba()
{
	LOCAL_DECODE_INIT;
	Rndecode(decode, 1);
	hyperstone_set(decode);
}

static void opbb()
{
	LOCAL_DECODE_INIT;
	Rndecode(decode, 1);
	hyperstone_set(decode);
}

static void opbc()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 0, 0);
	hyperstone_mul(decode);
}

static void opbd()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 0, 1);
	hyperstone_mul(decode);
}

static void opbe()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 1, 0);
	hyperstone_mul(decode);
}

static void opbf()
{
	LOCAL_DECODE_INIT;
	RRdecode(decode, 1, 1);
	hyperstone_mul(decode);
}



static void opc0()
{
	LOCAL_DECODE_INIT;
	LLdecode(decode);
	hyperstone_fadd(decode);
}

static void opc1()
{
	LOCAL_DECODE_INIT;
	LLdecode(decode);
	hyperstone_faddd(decode);
}

static void opc2()
{
	LOCAL_DECODE_INIT;
	LLdecode(decode);
	hyperstone_fsub(decode);
}

static void opc3()
{
	LOCAL_DECODE_INIT;
	LLdecode(decode);
	hyperstone_fsubd(decode);
}

static void opc4()
{
	LOCAL_DECODE_INIT;
	LLdecode(decode);
	hyperstone_fmul(decode);
}

static void opc5()
{
	LOCAL_DECODE_INIT;
	LLdecode(decode);
	hyperstone_fmuld(decode);
}

static void opc6()
{
	LOCAL_DECODE_INIT;
	LLdecode(decode);
	hyperstone_fdiv(decode);
}

static void opc7()
{
	LOCAL_DECODE_INIT;
	LLdecode(decode);
	hyperstone_fdivd(decode);
}

static void opc8()
{
	LOCAL_DECODE_INIT;
	LLdecode(decode);
	hyperstone_fcmp(decode);
}

static void opc9()
{
	LOCAL_DECODE_INIT;
	LLdecode(decode);
	hyperstone_fcmpd(decode);
}

static void opca()
{
	LOCAL_DECODE_INIT;
	LLdecode(decode);
	hyperstone_fcmpu(decode);
}

static void opcb()
{
	LOCAL_DECODE_INIT;
	LLdecode(decode);
	hyperstone_fcmpud(decode);
}

static void opcc()
{
	LOCAL_DECODE_INIT;
	LLdecode(decode);
	hyperstone_fcvt(decode);
}

static void opcd()
{
	LOCAL_DECODE_INIT;
	LLdecode(decode);
	hyperstone_fcvtd(decode);
}

static void opce()
{
	LOCAL_DECODE_INIT;
	LLextdecode(decode);
	hyperstone_extend(decode);
}

static void opcf()
{
	LOCAL_DECODE_INIT;
	LLdecode(decode);
	hyperstone_do(decode);
}



static void opd0()
{
	LOCAL_DECODE_INIT;
	LRdecode(decode, 0);
	hyperstone_ldwr(decode);
}

static void opd1()
{
	LOCAL_DECODE_INIT;
	LRdecode(decode, 1);
	hyperstone_ldwr(decode);
}

static void opd2()
{
	LOCAL_DECODE_INIT;
	LRdecode(decode, 0);
	hyperstone_lddr(decode);
}

static void opd3()
{
	LOCAL_DECODE_INIT;
	LRdecode(decode, 1);
	hyperstone_lddr(decode);
}

static void opd4()
{
	LOCAL_DECODE_INIT;
	LRdecode(decode, 0);
	hyperstone_ldwp(decode);
}

static void opd5()
{
	LOCAL_DECODE_INIT;
	LRdecode(decode, 1);
	hyperstone_ldwp(decode);
}

static void opd6()
{
	LOCAL_DECODE_INIT;
	LRdecode(decode, 0);
	hyperstone_lddp(decode);
}

static void opd7()
{
	LOCAL_DECODE_INIT;
	LRdecode(decode, 1);
	hyperstone_lddp(decode);
}

static void opd8()
{
	LOCAL_DECODE_INIT;
	LRdecode(decode, 0);
	hyperstone_stwr(decode);
}

static void opd9()
{
	LOCAL_DECODE_INIT;
	LRdecode(decode, 1);
	hyperstone_stwr(decode);
}

static void opda()
{
	LOCAL_DECODE_INIT;
	LRdecode(decode, 0);
	hyperstone_stdr(decode);
}

static void opdb()
{
	LOCAL_DECODE_INIT;
	LRdecode(decode, 1);
	hyperstone_stdr(decode);
}

static void opdc()
{
	LOCAL_DECODE_INIT;
	LRdecode(decode, 0);
	hyperstone_stwp(decode);
}

static void opdd()
{
	LOCAL_DECODE_INIT;
	LRdecode(decode, 1);
	hyperstone_stwp(decode);
}

static void opde()
{
	LOCAL_DECODE_INIT;
	LRdecode(decode, 0);
	hyperstone_stdp(decode);
}

static void opdf()
{
	LOCAL_DECODE_INIT;
	LRdecode(decode, 1);
	hyperstone_stdp(decode);
}



static void ope0()
{
	LOCAL_DECODE_INIT;
	PCreldecode(decode);
	hyperstone_dbv(decode);
}

static void ope1()
{
	LOCAL_DECODE_INIT;
	PCreldecode(decode);
	hyperstone_dbnv(decode);
}

static void ope2()
{
	LOCAL_DECODE_INIT;
	PCreldecode(decode);
	hyperstone_dbe(decode);
}

static void ope3()
{
	LOCAL_DECODE_INIT;
	PCreldecode(decode);
	hyperstone_dbne(decode);
}

static void ope4()
{
	LOCAL_DECODE_INIT;
	PCreldecode(decode);
	hyperstone_dbc(decode);
}

static void ope5()
{
	LOCAL_DECODE_INIT;
	PCreldecode(decode);
	hyperstone_dbnc(decode);
}

static void ope6()
{
	LOCAL_DECODE_INIT;
	PCreldecode(decode);
	hyperstone_dbse(decode);
}

static void ope7()
{
	LOCAL_DECODE_INIT;
	PCreldecode(decode);
	hyperstone_dbht(decode);
}

static void ope8()
{
	LOCAL_DECODE_INIT;
	PCreldecode(decode);
	hyperstone_dbn(decode);
}

static void ope9()
{
	LOCAL_DECODE_INIT;
	PCreldecode(decode);
	hyperstone_dbnn(decode);
}

static void opea()
{
	LOCAL_DECODE_INIT;
	PCreldecode(decode);
	hyperstone_dble(decode);
}

static void opeb()
{
	LOCAL_DECODE_INIT;
	PCreldecode(decode);
	hyperstone_dbgt(decode);
}

static void opec()
{
	LOCAL_DECODE_INIT;
	PCreldecode(decode);
	hyperstone_dbr(decode);
}

static void oped()
{
	LOCAL_DECODE_INIT;
	LLdecode(decode);
	hyperstone_frame(decode);
}

static void opee()
{
	LOCAL_DECODE_INIT;
	LRconstdecode(decode, 0);
	hyperstone_call(decode);
}

static void opef()
{
	LOCAL_DECODE_INIT;
	LRconstdecode(decode, 1);
	hyperstone_call(decode);
}



static void opf0()
{
	LOCAL_DECODE_INIT;
	PCreldecode(decode);
	hyperstone_bv(decode);
}

static void opf1()
{
	LOCAL_DECODE_INIT;
	PCreldecode(decode);
	hyperstone_bnv(decode);
}

static void opf2()
{
	LOCAL_DECODE_INIT;
	PCreldecode(decode);
	hyperstone_be(decode);
}

static void opf3()
{
	LOCAL_DECODE_INIT;
	PCreldecode(decode);
	hyperstone_bne(decode);
}

static void opf4()
{
	LOCAL_DECODE_INIT;
	PCreldecode(decode);
	hyperstone_bc(decode);
}

static void opf5()
{
	LOCAL_DECODE_INIT;
	PCreldecode(decode);
	hyperstone_bnc(decode);
}

static void opf6()
{
	LOCAL_DECODE_INIT;
	PCreldecode(decode);
	hyperstone_bse(decode);
}

static void opf7()
{
	LOCAL_DECODE_INIT;
	PCreldecode(decode);
	hyperstone_bht(decode);
}

static void opf8()
{
	LOCAL_DECODE_INIT;
	PCreldecode(decode);
	hyperstone_bn(decode);
}

static void opf9()
{
	LOCAL_DECODE_INIT;
	PCreldecode(decode);
	hyperstone_bnn(decode);
}

static void opfa()
{
	LOCAL_DECODE_INIT;
	PCreldecode(decode);
	hyperstone_ble(decode);
}

static void opfb()
{
	LOCAL_DECODE_INIT;
	PCreldecode(decode);
	hyperstone_bgt(decode);
}

static void opfc()
{
	LOCAL_DECODE_INIT;
	PCreldecode(decode);
	hyperstone_br(decode);
}

static void opfd()
{
	LOCAL_DECODE_INIT;
	PCadrdecode(decode);
	hyperstone_trap(decode);
}

static void opfe()
{
	LOCAL_DECODE_INIT;
	PCadrdecode(decode);
	hyperstone_trap(decode);
}

static void opff()
{
	LOCAL_DECODE_INIT;
	PCadrdecode(decode);
	hyperstone_trap(decode);
}

#if 0
const ophandler s_opcodetable[256] =
{
	&op00, &op01, &op02, &op03,
	&op04, &op05, &op06, &op07,
	&op08, &op09, &op0a, &op0b,
	&op0c, &op0d, &op0e, &op0f,

	&op10, &op11, &op12, &op13,
	&op14, &op15, &op16, &op17,
	&op18, &op19, &op1a, &op1b,
	&op1c, &op1d, &op1e, &op1f,

	&op20, &op21, &op22, &op23,
	&op24, &op25, &op26, &op27,
	&op28, &op29, &op2a, &op2b,
	&op2c, &op2d, &op2e, &op2f,

	&op30, &op31, &op32, &op33,
	&op34, &op35, &op36, &op37,
	&op38, &op39, &op3a, &op3b,
	&op3c, &op3d, &op3e, &op3f,

	&op40, &op41, &op42, &op43,
	&op44, &op45, &op46, &op47,
	&op48, &op49, &op4a, &op4b,
	&op4c, &op4d, &op4e, &op4f,

	&op50, &op51, &op52, &op53,
	&op54, &op55, &op56, &op57,
	&op58, &op59, &op5a, &op5b,
	&op5c, &op5d, &op5e, &op5f,

	&op60, &op61, &op62, &op63,
	&op64, &op65, &op66, &op67,
	&op68, &op69, &op6a, &op6b,
	&op6c, &op6d, &op6e, &op6f,

	&op70, &op71, &op72, &op73,
	&op74, &op75, &op76, &op77,
	&op78, &op79, &op7a, &op7b,
	&op7c, &op7d, &op7e, &op7f,

	&op80, &op81, &op82, &op83,
	&op84, &op85, &op86, &op87,
	&op88, &op89, &op8a, &op8b,
	&op8c, &op8d, &op8e, &op8f,

	&op90, &op91, &op92, &op93,
	&op94, &op95, &op96, &op97,
	&op98, &op99, &op9a, &op9b,
	&op9c, &op9d, &op9e, &op9f,

	&opa0, &opa1, &opa2, &opa3,
	&opa4, &opa5, &opa6, &opa7,
	&opa8, &opa9, &opaa, &opab,
	&opac, &opad, &opae, &opaf,

	&opb0, &opb1, &opb2, &opb3,
	&opb4, &opb5, &opb6, &opb7,
	&opb8, &opb9, &opba, &opbb,
	&opbc, &opbd, &opbe, &opbf,

	&opc0, &opc1, &opc2, &opc3,
	&opc4, &opc5, &opc6, &opc7,
	&opc8, &opc9, &opca, &opcb,
	&opcc, &opcd, &opce, &opcf,

	&opd0, &opd1, &opd2, &opd3,
	&opd4, &opd5, &opd6, &opd7,
	&opd8, &opd9, &opda, &opdb,
	&opdc, &opdd, &opde, &opdf,

	&ope0, &ope1, &ope2, &ope3,
	&ope4, &ope5, &ope6, &ope7,
	&ope8, &ope9, &opea, &opeb,
	&opec, &oped, &opee, &opef,

	&opf0, &opf1, &opf2, &opf3,
	&opf4, &opf5, &opf6, &opf7,
	&opf8, &opf9, &opfa, &opfb,
	&opfc, &opfd, &opfe, &opff
};
#endif
