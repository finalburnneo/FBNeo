// license:BSD-3-Clause
// copyright-holders:R. Belmont,byuu
/***************************************************************************

    upd7725.c

    Core implementation for the portable NEC uPD7725/uPD96050 emulator

    Original by byuu in the public domain.
    MAME conversion by R. Belmont

****************************************************************************/

#include "burnint.h"
#include "upd7725.h"

static INT32 program_address_mask;
static INT32 data_address_mask;
static UINT8 *upd96050Opcodes = NULL;
static UINT8 *upd96050Data = NULL;
static UINT16 *dataRAM;
static void (*out_p0_cb)(INT32) = NULL;
static void (*out_p1_cb)(INT32) = NULL;

static inline UINT32 read_op(UINT16 a)
{
	UINT32 *opcode = (UINT32*)upd96050Opcodes;

	return opcode[a & program_address_mask];
}

static inline UINT16 data_read_word(UINT16 a)
{
	UINT16 *data = (UINT16*)upd96050Data;

	return data[a & data_address_mask];
}

static inline void dataRAMWrite(INT32 offset, UINT16 data)
{
	dataRAM[offset & 0x7ff] = data;
}

static inline UINT16 dataRAMRead(INT32 offset)
{
	return dataRAM[offset & 0x7ff];
}

struct Flag
{
	bool s1, s0, c, z, ov1, ov0, ov0p, ov0pp;

	inline operator UINT8() const
	{
		return (s1 << 7) + (s0 << 6) + (c << 5) + (z << 4) + (ov1 << 3) + (ov0 << 2) + (ov0p << 1) + (ov0pp << 0);
	}

	inline unsigned operator=(UINT8 d)
	{
		s1 = d & 0x80; s0 = d & 0x40; c = d & 0x20; z = d & 0x10; ov1 = d & 0x08; ov0 = d & 0x04; ov0p = d & 0x02; ov0pp = d & 0x01;
		return d;
	}
};

struct Status
{
	bool rqm, usf1, usf0, drs, dma, drc, soc, sic, ei, p1, p0;

	inline operator UINT8() const
	{
		return (rqm << 15) + (usf1 << 14) + (usf0 << 13) + (drs << 12)
			+ (dma << 11) + (drc  << 10) + (soc  <<  9) + (sic <<  8)
			+ (ei  <<  7) + (p1   <<  1) + (p0   <<  0);
	}

	inline unsigned operator=(UINT8 d)
	{
		rqm = d & 0x8000; usf1 = d & 0x4000; usf0 = d & 0x2000; drs = d & 0x1000;
		dma = d & 0x0800; drc  = d & 0x0400; soc  = d & 0x0200; sic = d & 0x0100;
		ei  = d & 0x0080; p1   = d & 0x0002; p0   = d & 0x0001;
		return d;
	}
};

struct Regs
{
	UINT16 pc;          //program counter
	UINT16 stack[16];   //LIFO
	UINT16 rp;          //ROM pointer
	UINT16 dp;          //data pointer
	UINT8  sp;          //stack pointer
	INT16  k;
	INT16  l;
	INT16  m;
	INT16  n;
	INT16  a;         //accumulator
	INT16  b;         //accumulator
	Flag  flaga;
	Flag  flagb;
	UINT16 tr;        //temporary register
	UINT16 trb;       //temporary register
	Status sr;        //status register
	UINT16 dr;        //data register
	UINT16 si;
	UINT16 so;
	UINT16 idb;
} regs;

static INT32 m_icount = 0;

static void exec_ld(UINT32 opcode) {
	UINT16 id = opcode >> 6;  //immediate data
	UINT8 dst = (opcode >> 0) & 0xf;  //destination

	regs.idb = id;

	switch(dst) {
	case  0: break;
	case  1: regs.a = id; break;
	case  2: regs.b = id; break;
	case  3: regs.tr = id; break;
	case  4: regs.dp = id; break;
	case  5: regs.rp = id; break;
	case  6: regs.dr = id; regs.sr.rqm = 1; break;
	case  7: regs.sr = (regs.sr & 0x907c) | (id & ~0x907c);
				out_p0_cb(regs.sr&0x1);
				out_p1_cb((regs.sr&0x2)>>1);
				break;
	case  8: regs.so = id; break;  //LSB
	case  9: regs.so = id; break;  //MSB
	case 10: regs.k = id; break;
	case 11: regs.k = id; regs.l = data_read_word(regs.rp); break;
	case 12: regs.l = id; regs.k = dataRAMRead(regs.dp | 0x40); break;
	case 13: regs.l = id; break;
	case 14: regs.trb = id; break;
	case 15: dataRAMWrite(regs.dp, id); break;
	}
}

static void exec_op(UINT32 opcode) {
	UINT8 pselect = (opcode >> 20)&0x3;  //P select
	UINT8 alu     = (opcode >> 16)&0xf;  //ALU operation mode
	UINT8 asl     = (opcode >> 15)&0x1;  //accumulator select
	UINT8 dpl     = (opcode >> 13)&0x3;  //DP low modify
	UINT8 dphm    = (opcode >>  9)&0xf;  //DP high XOR modify
	UINT8 rpdcr   = (opcode >>  8)&0x1;  //RP decrement
	UINT8 src     = (opcode >>  4)&0xf;  //move source
	UINT8 dst     = (opcode >>  0)&0xf;  //move destination

	switch(src) {
	case  0: regs.idb = regs.trb; break;
	case  1: regs.idb = regs.a; break;
	case  2: regs.idb = regs.b; break;
	case  3: regs.idb = regs.tr; break;
	case  4: regs.idb = regs.dp; break;
	case  5: regs.idb = regs.rp; break;
	case  6: regs.idb = data_read_word(regs.rp); break;
	case  7: regs.idb = 0x8000 - regs.flaga.s1; break;  //SGN
	case  8: regs.idb = regs.dr; regs.sr.rqm = 1; break;
	case  9: regs.idb = regs.dr; break;
	case 10: regs.idb = regs.sr; break;
	case 11: regs.idb = regs.si; break;  //MSB
	case 12: regs.idb = regs.si; break;  //LSB
	case 13: regs.idb = regs.k; break;
	case 14: regs.idb = regs.l; break;
	case 15: regs.idb = dataRAMRead(regs.dp); break;
	}

	if(alu) {
	UINT16 p=0, q=0, r=0;
	Flag flag;
	bool c=0;

	flag.c = 0;
	flag.s1 = 0;
	flag.ov0 = 0;
	flag.ov1 = 0;
	flag.ov0p = 0;
	flag.ov0pp = 0;

	switch(pselect) {
		case 0: p = dataRAMRead(regs.dp); break;
		case 1: p = regs.idb; break;
		case 2: p = regs.m; break;
		case 3: p = regs.n; break;
	}

	switch(asl) {
		case 0: q = regs.a; flag = regs.flaga; c = regs.flagb.c; break;
		case 1: q = regs.b; flag = regs.flagb; c = regs.flaga.c; break;
	}

	switch(alu) {
		case  1: r = q | p; break;                    //OR
		case  2: r = q & p; break;                    //AND
		case  3: r = q ^ p; break;                    //XOR
		case  4: r = q - p; break;                    //SUB
		case  5: r = q + p; break;                    //ADD
		case  6: r = q - p - c; break;                //SBB
		case  7: r = q + p + c; break;                //ADC
		case  8: r = q - 1; p = 1; break;             //DEC
		case  9: r = q + 1; p = 1; break;             //INC
		case 10: r = ~q; break;                       //CMP
		case 11: r = (q >> 1) | (q & 0x8000); break;  //SHR1 (ASR)
		case 12: r = (q << 1) | (c ? 1 : 0); break;             //SHL1 (ROL)
		case 13: r = (q << 2) | 3; break;             //SHL2
		case 14: r = (q << 4) | 15; break;            //SHL4
		case 15: r = (q << 8) | (q >> 8); break;      //XCHG
	}

	flag.s0 = (r & 0x8000);
	flag.z = (r == 0);
	flag.ov0pp = flag.ov0p;
	flag.ov0p = flag.ov0;

	switch(alu) {
		case  1: case  2: case  3: case 10: case 13: case 14: case 15: {
		flag.c = 0;
		flag.ov0 = flag.ov0p = flag.ov0pp = 0; // ASSUMPTION: previous ov0 values are nulled here to make ov1 zero
		break;
		}
		case  4: case  5: case  6: case  7: case  8: case  9: {
		if(alu & 1) {
			//addition
			flag.ov0 = (q ^ r) & ~(q ^ p) & 0x8000;
			flag.c = (r < q);
		} else {
			//subtraction
			flag.ov0 = (q ^ r) &  (q ^ p) & 0x8000;
			flag.c = (r > q);
		}
		break;
		}
		case 11: {
		flag.c = q & 1;
		flag.ov0 = flag.ov0p = flag.ov0pp = 0; // ASSUMPTION: previous ov0 values are nulled here to make ov1 zero
		break;
		}
		case 12: {
		flag.c = q >> 15;
		flag.ov0 = flag.ov0p = flag.ov0pp = 0; // ASSUMPTION: previous ov0 values are nulled here to make ov1 zero
		break;
		}
	}
	// flag.ov1 is only set if the number of overflows of the past 3 opcodes (of type 4,5,6,7,8,9) is odd
	flag.ov1 = (flag.ov0 + flag.ov0p + flag.ov0pp) & 1;
	// flag.s1 is based on ov1: s1 = ov1 ^ s0;
	flag.s1 = flag.ov1 ^ flag.s0;

	switch(asl) {
		case 0: regs.a = r; regs.flaga = flag; break;
		case 1: regs.b = r; regs.flagb = flag; break;
	}
	}

	exec_ld((regs.idb << 6) + dst);

	switch(dpl) {
	case 1: regs.dp = (regs.dp & 0xf0) + ((regs.dp + 1) & 0x0f); break;  //DPINC
	case 2: regs.dp = (regs.dp & 0xf0) + ((regs.dp - 1) & 0x0f); break;  //DPDEC
	case 3: regs.dp = (regs.dp & 0xf0); break;  //DPCLR
	}

	regs.dp ^= dphm << 4;

	if(rpdcr) regs.rp--;
}

static void exec_rt(UINT32 opcode) {
	exec_op(opcode);
	regs.pc = regs.stack[--regs.sp];
	regs.sp &= 0xf;
}

static void exec_jp(UINT32 opcode) {
	UINT16 brch = (opcode >> 13) & 0x1ff;  //branch
	UINT16 na  =  (opcode >>  2) & 0x7ff;  //next address
	UINT16 bank = (opcode >>  0) & 0x3;  //bank address

	UINT16 jps = (regs.pc & 0x2000) | (bank << 11) | (na << 0);
	UINT16 jpl = (bank << 11) | (na << 0);

	switch(brch) {
		case 0x000: regs.pc = regs.so; return;  //JMPSO

		case 0x080: if(regs.flaga.c == 0) regs.pc = jps; return;  //JNCA
		case 0x082: if(regs.flaga.c == 1) regs.pc = jps; return;  //JCA
		case 0x084: if(regs.flagb.c == 0) regs.pc = jps; return;  //JNCB
		case 0x086: if(regs.flagb.c == 1) regs.pc = jps; return;  //JCB

		case 0x088: if(regs.flaga.z == 0) regs.pc = jps; return;  //JNZA
		case 0x08a: if(regs.flaga.z == 1) regs.pc = jps; return;  //JZA
		case 0x08c: if(regs.flagb.z == 0) regs.pc = jps; return;  //JNZB
		case 0x08e: if(regs.flagb.z == 1) regs.pc = jps; return;  //JZB

		case 0x090: if(regs.flaga.ov0 == 0) regs.pc = jps; return;  //JNOVA0
		case 0x092: if(regs.flaga.ov0 == 1) regs.pc = jps; return;  //JOVA0
		case 0x094: if(regs.flagb.ov0 == 0) regs.pc = jps; return;  //JNOVB0
		case 0x096: if(regs.flagb.ov0 == 1) regs.pc = jps; return;  //JOVB0

		case 0x098: if(regs.flaga.ov1 == 0) regs.pc = jps; return;  //JNOVA1
		case 0x09a: if(regs.flaga.ov1 == 1) regs.pc = jps; return;  //JOVA1
		case 0x09c: if(regs.flagb.ov1 == 0) regs.pc = jps; return;  //JNOVB1
		case 0x09e: if(regs.flagb.ov1 == 1) regs.pc = jps; return;  //JOVB1

		case 0x0a0: if(regs.flaga.s0 == 0) regs.pc = jps; return;  //JNSA0
		case 0x0a2: if(regs.flaga.s0 == 1) regs.pc = jps; return;  //JSA0
		case 0x0a4: if(regs.flagb.s0 == 0) regs.pc = jps; return;  //JNSB0
		case 0x0a6: if(regs.flagb.s0 == 1) regs.pc = jps; return;  //JSB0

		case 0x0a8: if(regs.flaga.s1 == 0) regs.pc = jps; return;  //JNSA1
		case 0x0aa: if(regs.flaga.s1 == 1) regs.pc = jps; return;  //JSA1
		case 0x0ac: if(regs.flagb.s1 == 0) regs.pc = jps; return;  //JNSB1
		case 0x0ae: if(regs.flagb.s1 == 1) regs.pc = jps; return;  //JSB1

		case 0x0b0: if((regs.dp & 0x0f) == 0x00) regs.pc = jps; return;  //JDPL0
		case 0x0b1: if((regs.dp & 0x0f) != 0x00) regs.pc = jps; return;  //JDPLN0
		case 0x0b2: if((regs.dp & 0x0f) == 0x0f) regs.pc = jps; return;  //JDPLF
		case 0x0b3: if((regs.dp & 0x0f) != 0x0f) regs.pc = jps; return;  //JDPLNF

		case 0x0bc: if(regs.sr.rqm == 0) regs.pc = jps; return;  //JNRQM
		case 0x0be: if(regs.sr.rqm == 1) regs.pc = jps; return;  //JRQM

		case 0x100: regs.pc = 0x0000 | jpl; return;  //LJMP
		case 0x101: regs.pc = 0x2000 | jpl; return;  //HJMP

		case 0x140: regs.stack[regs.sp++] = regs.pc; regs.pc = 0x0000 | jpl; regs.sp &= 0xf; return;  //LCALL
		case 0x141: regs.stack[regs.sp++] = regs.pc; regs.pc = 0x2000 | jpl; regs.sp &= 0xf; return;  //HCALL
	}
}

static void dummy_cb(INT32)
{
}

void upd96050Init(INT32 type, UINT8 *opcode, UINT8 *data, UINT8 *ram, void (*p0_cb)(INT32), void (*p1_cb)(INT32))
{
	// get our address spaces
	upd96050Opcodes = opcode;
	upd96050Data = data;
	dataRAM = (UINT16*)ram;

	// resolve callbacks
	out_p0_cb = (p0_cb == NULL) ? dummy_cb :  p0_cb;
	out_p1_cb = (p1_cb == NULL) ? dummy_cb :  p1_cb;

	if (type == 96050)
	{
		program_address_mask = ((1 << 14) - 1) >> 2;
		data_address_mask = ((1 << 12) - 1) >> 1;
	}

	if (type == 7725)
	{
		program_address_mask = ((1 << 11) - 1) >> 2;
		data_address_mask = ((1 << 11) - 1) >> 1;
	}
}

void upd96050Reset()
{
	for (INT32 i = 0; i < 2048; i++)
	{
		dataRAM[i] = 0x0000;
	}

	regs.pc = 0x0000;
	regs.rp = 0x0000;
	regs.dp = 0x0000;
	regs.sp = 0x0;
	regs.k = 0x0000;
	regs.l = 0x0000;
	regs.m = 0x0000;
	regs.n = 0x0000;
	regs.a = 0x0000;
	regs.b = 0x0000;
	regs.flaga = 0x00;
	regs.flagb = 0x00;
	regs.tr = 0x0000;
	regs.trb = 0x0000;
	regs.sr = 0x0000;
	regs.dr = 0x0000;
	regs.si = 0x0000;
	regs.so = 0x0000;
	regs.idb = 0x0000;

	m_icount = 0;
}

INT32 upd96050Scan(INT32 /*nAction*/)
{
	SCAN_VAR(regs);
	
	return 0;
}

INT32 upd96050Run(INT32 cycles)
{
	UINT32 opcode;

	m_icount = cycles;

	do
	{
		opcode = read_op(regs.pc)>>8;
		regs.pc++;
		switch(opcode >> 22)
		{
			case 0: exec_op(opcode); break;
			case 1: exec_rt(opcode); break;
			case 2: exec_jp(opcode); break;
			case 3: exec_ld(opcode); break;
		}

		INT32 result = (INT32)regs.k * regs.l;  //sign + 30-bit result
		regs.m = result >> 15;  //store sign + top 15-bits
		regs.n = result <<  1;  //store low 15-bits + zero

		m_icount--;

	} while (m_icount > 0);

	return cycles - m_icount;
}

UINT8 snesdsp_read(bool mode)
{
	if (!mode)
	{
		return regs.sr >> 8;
	}

	if (regs.sr.drc == 0)
	{
		//16-bit
		if(regs.sr.drs == 0)
		{
			regs.sr.drs = 1;
			return regs.dr >> 0;
		}
		else
		{
			regs.sr.rqm = 0;
			regs.sr.drs = 0;
			return regs.dr >> 8;
		}
	}
	else
	{
		//8-bit
		regs.sr.rqm = 0;
		return regs.dr >> 0;
	}
}

void snesdsp_write(bool mode, UINT8 data)
{
	if (!mode) return;

	if (regs.sr.drc == 0)
	{
		//16-bit
		if (regs.sr.drs == 0)
		{
			regs.sr.drs = 1;
			regs.dr = (regs.dr & 0xff00) | (data << 0);
		}
		else
		{
			regs.sr.rqm = 0;
			regs.sr.drs = 0;
			regs.dr = (data << 8) | (regs.dr & 0x00ff);
		}
	}
	else
	{
		//8-bit
		regs.sr.rqm = 0;
		regs.dr = (regs.dr & 0xff00) | (data << 0);
	}
}
