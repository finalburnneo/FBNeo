 /**************************************************************************\
 *                 Texas Instruments TMS32010 DSP Emulator                  *
 *                                                                          *
 *                  Copyright (C) 1999-2004+ Tony La Porta                  *
 *      You are not allowed to distribute this software commercially.       *
 *                      Written for the MAME project.                       *
 *                                                                          *
 *                                                                          *
 *      Notes : The term 'DMA' within this document, is in reference        *
 *                  to Direct Memory Addressing, and NOT the usual term     *
 *                  of Direct Memory Access.                                *
 *              This is a word based microcontroller, with addressing       *
 *                  architecture based on the Harvard addressing scheme.    *
 *                                                                          *
 *                                                                          *
 *                                                                          *
 *  **** Change Log ****                                                    *
 *                                                                          *
 *  TLP (13-Jul-2002)                                                       *
 *   - Added Save-State support                                             *
 *   - Converted the pending_irq flag to INTF (a real flag in this device)  *
 *   - Fixed the ignore Interrupt Request for previous critical             *
 *     instructions requiring an extra instruction to be processed. For     *
 *     this reason, instant IRQ servicing cannot be supported here, so      *
 *     INTF needs to be polled within the instruction execution loop        *
 *   - Removed IRQ callback (IRQ ACK not supported on this device)          *
 *   - A pending IRQ will remain pending until it's serviced. De-asserting  *
 *     the IRQ Pin does not remove a pending IRQ state                      *
 *   - BIO is no longer treated as an IRQ line. It's polled when required.  *
 *     This is the true behaviour of the device                             *
 *   - Removed the Clear OV flag from overflow instructions. Overflow       *
 *     instructions can only set the flag. Flag test instructions clear it  *
 *   - Fixed the ABST, SUBC and SUBH instructions                           *
 *   - Fixed the signedness in many equation based instructions             *
 *   - Added the missing Previous PC to the get_register function           *
 *   - Changed Cycle timings to include clock ticks                         *
 *   - Converted some registers from ints to pairs for much cleaner code    *
 *  TLP (20-Jul-2002) Ver 1.10                                              *
 *   - Fixed the dissasembly from the debugger                              *
 *   - Changed all references from TMS320C10 to TMS32010                    *
 *  ASG (24-Sep-2002) Ver 1.20                                              *
 *   - Fixed overflow handling                                              *
 *   - Simplified logic in a few locations                                  *
 *  TLP (22-Feb-2004) Ver 1.21                                              *
 *   - Overflow for ADDH only affects upper 16bits (was modifying 32 bits)  *
 *   - Internal Data Memory map is assigned here now                        *
 *   - Cycle counts for invalid opcodes 7F1E and 7F1F are now 0             *
 *                                                                          *
 \**************************************************************************/



#include "burnint.h"
#include "driver.h"
#include "state.h"
#include "tms32010.h"


#ifdef INLINE
#undef INLINE
#endif
#define INLINE static inline

static INT32 addr_mask;

UINT16 *tms32010_ram = NULL;
UINT16 *tms32010_rom = NULL;

static void (*tms32010_write_port)(INT32,UINT16);
static UINT16 (*tms32010_read_port)(INT32);

static UINT16 program_read_word_16be(UINT16 address)
{
	UINT16 r = tms32010_rom[address & addr_mask];
	r = (r << 8) | (r >> 8);
	return r;
}

static void program_write_word_16be(UINT16 address, UINT16 data)
{
	data = (data << 8) | (data >> 8);

	tms32010_rom[address & addr_mask] = data;
}

static UINT16 data_read_word_16be(UINT16 address)
{
	UINT16 r = BURN_ENDIAN_SWAP_INT16(tms32010_ram[address & 0xff]);

	r = (r << 8) | (r >> 8);

	return r;
}

static void data_write_word_16be(UINT16 address, UINT16 data)
{
	data = (data << 8) | (data >> 8);

	tms32010_ram[address & 0xff] = BURN_ENDIAN_SWAP_INT16(data);
}

UINT16 io_read_word(INT32 offset)
{
	if (tms32010_read_port) {
		UINT16 r = tms32010_read_port(offset);
		return r;
	}

	return 0;
}

void io_write_word(INT32 offset, UINT16 data)
{
	if (tms32010_write_port) {
		tms32010_write_port(offset, data);
		return;
	}
}

void tms32010_set_write_port_handler(void (*pointer)(INT32,UINT16))
{
	tms32010_write_port = pointer;
}

void tms32010_set_read_port_handler(UINT16 (*pointer)(INT32))
{
	tms32010_read_port = pointer;
}


#define TMS32010_BIO_In (io_read_word(TMS32010_BIO))
#define TMS32010_In(Port) (io_read_word(Port))
#define TMS32010_Out(Port,Value) (io_write_word(Port,Value))
#define TMS32010_ROM_RDMEM(A) (program_read_word_16be((A)))
#define TMS32010_ROM_WRMEM(A,V) (program_write_word_16be((A),V))
#define TMS32010_RAM_RDMEM(A) (data_read_word_16be((A)))
#define TMS32010_RAM_WRMEM(A,V) (data_write_word_16be((A),V))
#define TMS32010_RDOP(A) (program_read_word_16be((A)))
#define TMS32010_RDOP_ARG(A) (program_read_word_16be((A)))



#define M_RDROM(A)		TMS32010_ROM_RDMEM(A)
#define M_WRTROM(A,V)	TMS32010_ROM_WRMEM(A,V)
#define M_RDRAM(A)		TMS32010_RAM_RDMEM(A)
#define M_WRTRAM(A,V)	TMS32010_RAM_WRMEM(A,V)
#define M_RDOP(A)		TMS32010_RDOP(A)
#define M_RDOP_ARG(A)	TMS32010_RDOP_ARG(A)
#define P_IN(A)			TMS32010_In(A)
#define P_OUT(A,V)		TMS32010_Out(A,V)
#define BIO_IN			TMS32010_BIO_In



typedef struct			/* Page 3-6 shows all registers */
{
	/******************** CPU Internal Registers *******************/
	UINT16	PC;
	UINT16	PREVPC;		/* previous program counter */
	UINT16	STR;
	PAIR	ACC;
	PAIR	ALU;
	PAIR	Preg;
	UINT16	Treg;
	UINT16	AR[2];
	UINT16	STACK[4];

	/********************** Status data ****************************/
	PAIR	opcode;
	INT32	INTF;		/* Pending Interrupt flag */
	PAIR	oldacc;
	UINT16	memaccess;

	UINT32  total_cycles;
	UINT32	cycles_start;
	INT32	icount;
	INT32   end_run;
} tms32010_Regs;

static tms32010_Regs R;

static int add_branch_cycle();

/********  The following is the Status (Flag) register definition.  *********/
/* 15 | 14  |  13  | 12 | 11 | 10 | 9 |  8  | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0  */
/* OV | OVM | INTM |  1 |  1 |  1 | 1 | ARP | 1 | 1 | 1 | 1 | 1 | 1 | 1 | DP */
#define OV_FLAG		0x8000	/* OV   (Overflow flag) 1 indicates an overflow */
#define OVM_FLAG	0x4000	/* OVM  (Overflow Mode bit) 1 forces ACC overflow to greatest positive or negative saturation value */
#define INTM_FLAG	0x2000	/* INTM (Interrupt Mask flag) 0 enables maskable interrupts */
#define ARP_REG		0x0100	/* ARP  (Auxiliary Register Pointer) */
#define DP_REG		0x0001	/* DP   (Data memory Pointer (bank) bit) */

#define OV		( R.STR & OV_FLAG)			/* OV   (Overflow flag) */
#define OVM		( R.STR & OVM_FLAG)			/* OVM  (Overflow Mode bit) 1 indicates an overflow */
#define INTM	( R.STR & INTM_FLAG)		/* INTM (Interrupt enable flag) 0 enables maskable interrupts */
#define ARP		((R.STR & ARP_REG) >> 8)	/* ARP  (Auxiliary Register Pointer) */
#define DP		((R.STR & DP_REG) << 7)		/* DP   (Data memory Pointer bit) */

#define DMA_DP	(DP | (R.opcode.b.l & 0x7f))	/* address used in direct memory access operations */
#define DMA_DP1	(0x80 | R.opcode.b.l)			/* address used in direct memory access operations for sst instruction */
#define IND		(R.AR[ARP] & 0xff)				/* address used in indirect memory access operations */




/************************************************************************
 *  Shortcuts
 ************************************************************************/

INLINE void CLR(UINT16 flag) { R.STR &= ~flag; R.STR |= 0x1efe; }
INLINE void SET(UINT16 flag) { R.STR |=  flag; R.STR |= 0x1efe; }


INLINE void CALCULATE_ADD_OVERFLOW(INT32 addval)
{
	if ((INT32)(~(R.oldacc.d ^ addval) & (R.oldacc.d ^ R.ACC.d)) < 0) {
		SET(OV_FLAG);
		if (OVM)
			R.ACC.d = ((INT32)R.oldacc.d < 0) ? 0x80000000 : 0x7fffffff;
	}
}
INLINE void CALCULATE_SUB_OVERFLOW(INT32 subval)
{
	if ((INT32)((R.oldacc.d ^ subval) & (R.oldacc.d ^ R.ACC.d)) < 0) {
		SET(OV_FLAG);
		if (OVM)
			R.ACC.d = ((INT32)R.oldacc.d < 0) ? 0x80000000 : 0x7fffffff;
	}
}

INLINE UINT16 POP_STACK(void)
{
	UINT16 data = R.STACK[3];
	R.STACK[3] = R.STACK[2];
	R.STACK[2] = R.STACK[1];
	R.STACK[1] = R.STACK[0];
	return (data & addr_mask);
}
INLINE void PUSH_STACK(UINT16 data)
{
	R.STACK[0] = R.STACK[1];
	R.STACK[1] = R.STACK[2];
	R.STACK[2] = R.STACK[3];
	R.STACK[3] = (data & addr_mask);
}

INLINE void UPDATE_AR(void)
{
	if (R.opcode.b.l & 0x30) {
		UINT16 tmpAR = R.AR[ARP];
		if (R.opcode.b.l & 0x20) tmpAR++ ;
		if (R.opcode.b.l & 0x10) tmpAR-- ;
		R.AR[ARP] = (R.AR[ARP] & 0xfe00) | (tmpAR & 0x01ff);
	}
}
INLINE void UPDATE_ARP(void)
{
	if (~R.opcode.b.l & 0x08) {
		if (R.opcode.b.l & 0x01) SET(ARP_REG);
		else CLR(ARP_REG);
	}
}


INLINE void getdata(UINT8 shift,UINT8 signext)
{
	if (R.opcode.b.l & 0x80)
		R.memaccess = IND;
	else
		R.memaccess = DMA_DP;

	R.ALU.d = (UINT16)M_RDRAM(R.memaccess);
	if (signext) R.ALU.d = (INT16)R.ALU.d;
	R.ALU.d <<= shift;
	if (R.opcode.b.l & 0x80) {
		UPDATE_AR();
		UPDATE_ARP();
	}
}

INLINE void putdata(UINT16 data)
{
	if (R.opcode.b.l & 0x80)
		R.memaccess = IND;
	else
		R.memaccess = DMA_DP;

	if (R.opcode.b.l & 0x80) {
		UPDATE_AR();
		UPDATE_ARP();
	}
	M_WRTRAM(R.memaccess,data);
}
INLINE void putdata_sar(UINT8 data)
{
	if (R.opcode.b.l & 0x80)
		R.memaccess = IND;
	else
		R.memaccess = DMA_DP;

	if (R.opcode.b.l & 0x80) {
		UPDATE_AR();
		UPDATE_ARP();
	}
	M_WRTRAM(R.memaccess,R.AR[data]);
}
INLINE void putdata_sst(UINT16 data)
{
	if (R.opcode.b.l & 0x80)
		R.memaccess = IND;
	else
		R.memaccess = DMA_DP1;  /* Page 1 only */

	if (R.opcode.b.l & 0x80) {
		UPDATE_AR();
	}
	M_WRTRAM(R.memaccess,data);
}



/************************************************************************
 *  Emulate the Instructions
 ************************************************************************/

/* This following function is here to fill in the void for */
/* the opcode call function. This function is never called. */

static void opcodes_7F(void)  { }


static void illegal(void)
{
		logerror("TMS32010:  PC=%04x,  Illegal opcode = %04x\n", (R.PC-1), R.opcode.w.l);
}

static void abst(void)
{
		if ( (INT32)(R.ACC.d) < 0 ) {
			R.ACC.d = -R.ACC.d;
			if (OVM && (R.ACC.d == 0x80000000)) R.ACC.d-- ;
		}
}

/*** The manual does not mention overflow with the ADD? instructions *****
 *** however I implemented overflow, coz it doesnt make sense otherwise **
 *** and newer generations of this type of chip supported it. I think ****
 *** the manual is wrong (apart from other errors the manual has). *******

static void add_sh(void)    { getdata(R.opcode.b.h,1); R.ACC.d += R.ALU.d; }
static void addh(void)      { getdata(0,0); R.ACC.d += (R.ALU.d << 16); }
 ***/

static void add_sh(void)
{
	R.oldacc.d = R.ACC.d;
	getdata((R.opcode.b.h & 0xf),1);
	R.ACC.d += R.ALU.d;
	CALCULATE_ADD_OVERFLOW(R.ALU.d);
}
static void addh(void)
{
	R.oldacc.d = R.ACC.d;
	getdata(0,0);
	R.ACC.w.h += R.ALU.w.l;
	if ((INT16)(~(R.oldacc.w.h ^ R.ALU.w.h) & (R.oldacc.w.h ^ R.ACC.w.h)) < 0) {
		SET(OV_FLAG);
		if (OVM)
			R.ACC.w.h = ((INT16)R.oldacc.w.h < 0) ? 0x8000 : 0x7fff;
	}
}
static void adds(void)
{
	R.oldacc.d = R.ACC.d;
	getdata(0,0);
	R.ACC.d += R.ALU.d;
	CALCULATE_ADD_OVERFLOW(R.ALU.d);
}
static void and_(void)
{
	getdata(0,0);
	R.ACC.d &= R.ALU.d;
}
static void apac(void)
{
	R.oldacc.d = R.ACC.d;
	R.ACC.d += R.Preg.d;
	CALCULATE_ADD_OVERFLOW(R.Preg.d);
}
static void br(void)
{
	R.PC = M_RDOP_ARG(R.PC);
}
static void banz(void)
{
	if (R.AR[ARP] & 0x01ff) {
		R.PC = M_RDOP_ARG(R.PC);
		R.icount -= add_branch_cycle();
	}
	else
		R.PC++ ;
	R.ALU.w.l = R.AR[ARP];
	R.ALU.w.l-- ;
	R.AR[ARP] = (R.AR[ARP] & 0xfe00) | (R.ALU.w.l & 0x01ff);
}
static void bgez(void)
{
	if ( (INT32)(R.ACC.d) >= 0 ) {
		R.PC = M_RDOP_ARG(R.PC);
		R.icount -= add_branch_cycle();
	}
	else
		R.PC++ ;
}
static void bgz(void)
{
	if ( (INT32)(R.ACC.d) > 0 ) {
		R.PC = M_RDOP_ARG(R.PC);
		R.icount -= add_branch_cycle();
	}
	else
		R.PC++ ;
}
static void bioz(void)
{
	if (BIO_IN != CPU_IRQSTATUS_NONE) {
		R.PC = M_RDOP_ARG(R.PC);
		R.icount -= add_branch_cycle();
	}
	else
		R.PC++ ;
}
static void blez(void)
{
	if ( (INT32)(R.ACC.d) <= 0 ) {
		R.PC = M_RDOP_ARG(R.PC);
		R.icount -= add_branch_cycle();
	}
	else
		R.PC++ ;
}
static void blz(void)
{
	if ( (INT32)(R.ACC.d) <  0 ) {
		R.PC = M_RDOP_ARG(R.PC);
		R.icount -= add_branch_cycle();
	}
	else
		R.PC++ ;
}
static void bnz(void)
{
	if (R.ACC.d != 0) {
		R.PC = M_RDOP_ARG(R.PC);
		R.icount -= add_branch_cycle();
	}
	else
		R.PC++ ;
}
static void bv(void)
{
	if (OV) {
		CLR(OV_FLAG);
		R.PC = M_RDOP_ARG(R.PC);
		R.icount -= add_branch_cycle();
	}
	else
		R.PC++ ;
}
static void bz(void)
{
	if (R.ACC.d == 0) {
		R.PC = M_RDOP_ARG(R.PC);
		R.icount -= add_branch_cycle();
	}
	else
		R.PC++ ;
}
static void cala(void)
{
	PUSH_STACK(R.PC);
	R.PC = R.ACC.w.l & addr_mask;
}
static void call(void)
{
	R.PC++ ;
	PUSH_STACK(R.PC);
	R.PC = M_RDOP_ARG((R.PC - 1));// & addr_mask;
}
static void dint(void)
{
	SET(INTM_FLAG);
}
static void dmov(void)
{
	getdata(0,0);
	M_WRTRAM((R.memaccess + 1),R.ALU.w.l);
}
static void eint(void)
{
	CLR(INTM_FLAG);
}
static void in_p(void)
{
	R.ALU.w.l = P_IN( (R.opcode.b.h & 7) );
	putdata(R.ALU.w.l);
}
static void lac_sh(void)
{
	getdata((R.opcode.b.h & 0x0f),1);
	R.ACC.d = R.ALU.d;
}
static void lack(void)
{
	R.ACC.d = R.opcode.b.l;
}
static void lar_ar0(void)
{
	getdata(0,0);
	R.AR[0] = R.ALU.w.l;
}
static void lar_ar1(void)
{
	getdata(0,0);
	R.AR[1] = R.ALU.w.l;
}
static void lark_ar0(void)
{
	R.AR[0] = R.opcode.b.l;
}
static void lark_ar1(void)
{
	R.AR[1] = R.opcode.b.l;
}
static void larp_mar(void)
{
	if (R.opcode.b.l & 0x80) {
		UPDATE_AR();
		UPDATE_ARP();
	}
}
static void ldp(void)
{
	getdata(0,0);
	if (R.ALU.d & 1)
		SET(DP_REG);
	else
		CLR(DP_REG);
}
static void ldpk(void)
{
	if (R.opcode.b.l & 1)
		SET(DP_REG);
	else
		CLR(DP_REG);
}
static void lst(void)
{
	if (R.opcode.b.l & 0x80) {
		R.opcode.b.l |= 0x08; /* In Indirect Addressing mode, next ARP is not supported here so mask it */
	}
	getdata(0,0);
	R.ALU.w.l &= (~INTM_FLAG);	/* Must not affect INTM */
	R.STR &= INTM_FLAG;
	R.STR |= R.ALU.w.l;
	R.STR |= 0x1efe;
}
static void lt(void)
{
	getdata(0,0);
	R.Treg = R.ALU.w.l;
}
static void lta(void)
{
	R.oldacc.d = R.ACC.d;
	getdata(0,0);
	R.Treg = R.ALU.w.l;
	R.ACC.d += R.Preg.d;
	CALCULATE_ADD_OVERFLOW(R.Preg.d);
}
static void ltd(void)
{
	R.oldacc.d = R.ACC.d;
	getdata(0,0);
	R.Treg = R.ALU.w.l;
	M_WRTRAM((R.memaccess + 1),R.ALU.w.l);
	R.ACC.d += R.Preg.d;
	CALCULATE_ADD_OVERFLOW(R.Preg.d);
}
static void mpy(void)
{
	getdata(0,0);
	R.Preg.d = (INT16)R.ALU.w.l * (INT16)R.Treg;
	if (R.Preg.d == 0x40000000) R.Preg.d = 0xc0000000;
}
static void mpyk(void)
{
	R.Preg.d = (INT16)R.Treg * ((INT16)(R.opcode.w.l << 3) >> 3);
}
static void nop(void)
{
	/* Nothing to do */
}
static void or_(void)
{
	getdata(0,0);
	R.ACC.w.l |= R.ALU.w.l;
}
static void out_p(void)
{
	getdata(0,0);
	P_OUT( (R.opcode.b.h & 7), R.ALU.w.l );
}
static void pac(void)
{
	R.ACC.d = R.Preg.d;
}
static void pop(void)
{
	R.ACC.w.l = POP_STACK();
	R.ACC.w.h = 0x0000;
}
static void push(void)
{
	PUSH_STACK(R.ACC.w.l);
}
static void ret(void)
{
	R.PC = POP_STACK();
}
static void rovm(void)
{
	CLR(OVM_FLAG);
}
static void sach_sh(void)
{
	R.ALU.d = (R.ACC.d << (R.opcode.b.h & 7));
	putdata(R.ALU.w.h);
}
static void sacl(void)
{
	putdata(R.ACC.w.l);
}
static void sar_ar0(void)
{
	putdata_sar(0);
}
static void sar_ar1(void)
{
	putdata_sar(1);
}
static void sovm(void)
{
	SET(OVM_FLAG);
}
static void spac(void)
{
	R.oldacc.d = R.ACC.d;
	R.ACC.d -= R.Preg.d;
	CALCULATE_SUB_OVERFLOW(R.Preg.d);
}
static void sst(void)
{
	putdata_sst(R.STR);
}
static void sub_sh(void)
{
	R.oldacc.d = R.ACC.d;
	getdata((R.opcode.b.h & 0x0f),1);
	R.ACC.d -= R.ALU.d;
	CALCULATE_SUB_OVERFLOW(R.ALU.d);
}
static void subc(void)
{
	R.oldacc.d = R.ACC.d;
	getdata(15,0);
	R.ALU.d = (INT32) R.ACC.d - R.ALU.d;
	if ((INT32)((R.oldacc.d ^ R.ALU.d) & (R.oldacc.d ^ R.ACC.d)) < 0)
		SET(OV_FLAG);
	if ( (INT32)(R.ALU.d) >= 0 )
		R.ACC.d = ((R.ALU.d << 1) + 1);
	else
		R.ACC.d = (R.ACC.d << 1);
}
static void subh(void)
{
	R.oldacc.d = R.ACC.d;
	getdata(16,0);
	R.ACC.d -= R.ALU.d;
	CALCULATE_SUB_OVERFLOW(R.ALU.d);
}
static void subs(void)
{
	R.oldacc.d = R.ACC.d;
	getdata(0,0);
	R.ACC.d -= R.ALU.d;
	CALCULATE_SUB_OVERFLOW(R.ALU.d);
}
static void tblr(void)
{
	R.ALU.d = M_RDROM((R.ACC.w.l & addr_mask));
	putdata(R.ALU.w.l);
	R.STACK[0] = R.STACK[1];
}
static void tblw(void)
{
	getdata(0,0);
	M_WRTROM(((R.ACC.w.l & addr_mask)),R.ALU.w.l);
	R.STACK[0] = R.STACK[1];
}
static void xor_(void)
{
	getdata(0,0);
	R.ACC.w.l ^= R.ALU.w.l;
}
static void zac(void)
{
	R.ACC.d = 0;
}
static void zalh(void)
{
	getdata(0,0);
	R.ACC.w.h = R.ALU.w.l;
	R.ACC.w.l = 0x0000;
}
static void zals(void)
{
	getdata(0,0);
	R.ACC.w.l = R.ALU.w.l;
	R.ACC.w.h = 0x0000;
}


/***********************************************************************
 *  Cycle Timings
 ***********************************************************************/

typedef void ( *opcode_func ) ();

struct tms32010_opcode
{
	UINT8       cycles;
	opcode_func function;
};

const tms32010_opcode s_opcode_main[256]=
{
/*00*/  {1, &add_sh  },{1, &add_sh    },{1, &add_sh    },{1, &add_sh    },{1, &add_sh    },{1, &add_sh    },{1, &add_sh    },{1, &add_sh    },
/*08*/  {1, &add_sh  },{1, &add_sh    },{1, &add_sh    },{1, &add_sh    },{1, &add_sh    },{1, &add_sh    },{1, &add_sh    },{1, &add_sh    },
/*10*/  {1, &sub_sh  },{1, &sub_sh    },{1, &sub_sh    },{1, &sub_sh    },{1, &sub_sh    },{1, &sub_sh    },{1, &sub_sh    },{1, &sub_sh    },
/*18*/  {1, &sub_sh  },{1, &sub_sh    },{1, &sub_sh    },{1, &sub_sh    },{1, &sub_sh    },{1, &sub_sh    },{1, &sub_sh    },{1, &sub_sh    },
/*20*/  {1, &lac_sh  },{1, &lac_sh    },{1, &lac_sh    },{1, &lac_sh    },{1, &lac_sh    },{1, &lac_sh    },{1, &lac_sh    },{1, &lac_sh    },
/*28*/  {1, &lac_sh  },{1, &lac_sh    },{1, &lac_sh    },{1, &lac_sh    },{1, &lac_sh    },{1, &lac_sh    },{1, &lac_sh    },{1, &lac_sh    },
/*30*/  {1, &sar_ar0 },{1, &sar_ar1   },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },
/*38*/  {1, &lar_ar0 },{1, &lar_ar1   },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },
/*40*/  {2, &in_p    },{2, &in_p      },{2, &in_p      },{2, &in_p      },{2, &in_p      },{2, &in_p      },{2, &in_p      },{2, &in_p      },
/*48*/  {2, &out_p   },{2, &out_p     },{2, &out_p     },{2, &out_p     },{2, &out_p     },{2, &out_p     },{2, &out_p     },{2, &out_p     },
/*50*/  {1, &sacl    },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },
/*58*/  {1, &sach_sh },{1, &sach_sh   },{1, &sach_sh   },{1, &sach_sh   },{1, &sach_sh   },{1, &sach_sh   },{1, &sach_sh   },{1, &sach_sh   },
/*60*/  {1, &addh    },{1, &adds      },{1, &subh      },{1, &subs      },{1, &subc      },{1, &zalh      },{1, &zals      },{3, &tblr      },
/*68*/  {1, &larp_mar},{1, &dmov      },{1, &lt        },{1, &ltd       },{1, &lta       },{1, &mpy       },{1, &ldpk      },{1, &ldp       },
/*70*/  {1, &lark_ar0},{1, &lark_ar1  },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },
/*78*/  {1, &xor_    },{1, &and_      },{1, &or_       },{1, &lst       },{1, &sst       },{3, &tblw      },{1, &lack      },{0, &opcodes_7F    },
/*80*/  {1, &mpyk    },{1, &mpyk      },{1, &mpyk      },{1, &mpyk      },{1, &mpyk      },{1, &mpyk      },{1, &mpyk      },{1, &mpyk      },
/*88*/  {1, &mpyk    },{1, &mpyk      },{1, &mpyk      },{1, &mpyk      },{1, &mpyk      },{1, &mpyk      },{1, &mpyk      },{1, &mpyk      },
/*90*/  {1, &mpyk    },{1, &mpyk      },{1, &mpyk      },{1, &mpyk      },{1, &mpyk      },{1, &mpyk      },{1, &mpyk      },{1, &mpyk      },
/*98*/  {1, &mpyk    },{1, &mpyk      },{1, &mpyk      },{1, &mpyk      },{1, &mpyk      },{1, &mpyk      },{1, &mpyk      },{1, &mpyk      },
/*A0*/  {0, &illegal },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },
/*A8*/  {0, &illegal },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },
/*B0*/  {0, &illegal },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },
/*B8*/  {0, &illegal },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },
/*C0*/  {0, &illegal },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },
/*C8*/  {0, &illegal },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },
/*D0*/  {0, &illegal },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },
/*D8*/  {0, &illegal },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },
/*E0*/  {0, &illegal },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },
/*E8*/  {0, &illegal },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },
/*F0*/  {0, &illegal },{0, &illegal   },{0, &illegal   },{0, &illegal   },{1, &banz      },{1, &bv        },{1, &bioz      },{0, &illegal   },
/*F8*/  {2, &call    },{2, &br        },{1, &blz       },{1, &blez      },{1, &bgz       },{1, &bgez      },{1, &bnz       },{1, &bz        }
};

const tms32010_opcode s_opcode_7F[32]=
{
/*80*/  {1, &nop     },{1, &dint      },{1, &eint      },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },
/*88*/  {1, &abst    },{1, &zac       },{1, &rovm      },{1, &sovm      },{2, &cala      },{2, &ret       },{1, &pac       },{1, &apac      },
/*90*/  {1, &spac    },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },{0, &illegal   },
/*98*/  {0, &illegal },{0, &illegal   },{0, &illegal   },{0, &illegal   },{2, &push      },{2, &pop       },{0, &illegal   },{0, &illegal   }
};

static int add_branch_cycle()
{
	return s_opcode_main[R.opcode.b.h].cycles;
}


/****************************************************************************
 *  Inits CPU emulation
 ****************************************************************************/
void tms32010_init (void)
{
#if 0
	state_save_register_item("tms32010", index, R.PC);
	state_save_register_item("tms32010", index, R.PREVPC);
	state_save_register_item("tms32010", index, R.STR);
	state_save_register_item("tms32010", index, R.ACC.d);
	state_save_register_item("tms32010", index, R.ALU.d);
	state_save_register_item("tms32010", index, R.Preg.d);
	state_save_register_item("tms32010", index, R.Treg);
	state_save_register_item("tms32010", index, R.AR[0]);
	state_save_register_item("tms32010", index, R.AR[1]);
	state_save_register_item("tms32010", index, R.STACK[0]);
	state_save_register_item("tms32010", index, R.STACK[1]);
	state_save_register_item("tms32010", index, R.STACK[2]);
	state_save_register_item("tms32010", index, R.STACK[3]);
	state_save_register_item("tms32010", index, R.INTF);
	state_save_register_item("tms32010", index, R.opcode.d);
#endif
}

/****************************************************************************
 *  Reset registers to their initial values
 ****************************************************************************/
void tms32010_reset (void)
{
	memset(&R, 0, sizeof(R));
	R.PC    = 0;
	R.STR   = 0;
	R.ACC.d = 0;
	R.INTF  = TMS32010_INT_NONE;
	addr_mask = 0x0fff;	/* TMS32010 can only address 0x0fff */
						/* however other TMS3201x devices   */
						/* can address up to 0xffff (incase */
						/* their support is ever added).    */

	/* Setup Status Register : 7efe */
	CLR((OV_FLAG | ARP_REG | DP_REG));
	SET((OVM_FLAG | INTM_FLAG));

	R.total_cycles = 0;
}


/****************************************************************************
 *  Shut down CPU emulation
 ****************************************************************************/
void tms32010_exit (void)
{
	/* nothing to do ? */
}


/****************************************************************************
 *  Issue an interrupt if necessary
 ****************************************************************************/
int tms32010_Ext_IRQ(void)
{
	if (INTM == 0)
	{
		logerror("TMS32010:  EXT INTERRUPT\n");
		R.INTF = TMS32010_INT_NONE;
		SET(INTM_FLAG);
		PUSH_STACK(R.PC);
		R.PC = 0x0002;
		return (s_opcode_7F[0x1c].cycles + s_opcode_7F[0x01].cycles);   /* 3 cycles used due to PUSH and DINT operation ? */
	}
	return (0);
}

/****************************************************************************
 *  Execute IPeriod. Return 0 if emulation should be stopped
 ****************************************************************************/
int tms32010Run(int cycles)
{
	R.icount = cycles;
	R.cycles_start = cycles;

	R.end_run = 0;

	do
	{
		if (R.INTF) {
			/* Dont service INT if prev instruction was MPY, MPYK or EINT */
			if ((R.opcode.b.h != 0x6d) && ((R.opcode.b.h & 0xe0) != 0x80) && (R.opcode.w.l != 0x7f82))
				R.icount -= tms32010_Ext_IRQ();
		}

		R.PREVPC = R.PC;

		//CALL_MAME_DEBUG;

		R.opcode.d = M_RDOP(R.PC);
		R.PC++;

		if (R.opcode.b.h != 0x7f)	{ /* Do all opcodes except the 7Fxx ones */
			R.icount -= s_opcode_main[R.opcode.b.h].cycles;
			(*s_opcode_main[R.opcode.b.h].function)();
		}
		else { /* Opcode major byte 7Fxx has many opcodes in its minor byte */
			R.icount -= s_opcode_7F[(R.opcode.b.l & 0x1f)].cycles;
			(*s_opcode_7F[(R.opcode.b.l & 0x1f)].function)();
		}
	} while (R.icount > 0 && !R.end_run);

	cycles = cycles - R.icount;
	R.total_cycles += cycles;

	R.cycles_start = 0;
	R.icount = 0;

	return cycles;
}

int tms32010Idle(int cycles)
{
	R.total_cycles += cycles;

	return cycles;
}

UINT32 tms32010TotalCycles()
{
	return R.total_cycles + (R.cycles_start - R.icount);
}

void tms32010NewFrame()
{
	R.total_cycles = 0;
}

void tms32010RunEnd()
{
	R.end_run = 1;
}

void tms32010_scan(INT32 nAction)
{
	if (nAction & ACB_DRIVER_DATA) {
		struct BurnArea ba;
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = &R;
		ba.nLen	  = sizeof(tms32010_Regs);
		ba.szName = "tms32010 Regs";
		BurnAcb(&ba);
	}
}

UINT16 tms32010_get_pc()
{
	return R.PC;
}

/****************************************************************************
 *  Set IRQ line state
 ****************************************************************************/
void tms32010_set_irq_line(int irqline, int state)
{
	/* Pending Interrupts cannot be cleared! */
	if (state == CPU_IRQSTATUS_ACK) R.INTF |=  TMS32010_INT_PENDING;
}

