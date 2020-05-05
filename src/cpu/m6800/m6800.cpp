/*** m6800: Portable 6800 class  emulator *************************************

    m6800.c

    References:

        6809 Simulator V09, By L.C. Benschop, Eidnhoven The Netherlands.

        m6809: Portable 6809 emulator, DS (6809 code in MAME, derived from
            the 6809 Simulator V09)

        6809 Microcomputer Programming & Interfacing with Experiments"
            by Andrew C. Staugaard, Jr.; Howard W. Sams & Co., Inc.

    System dependencies:    UINT16 must be 16 bit unsigned int
                            UINT8 must be 8 bit unsigned int
                            UINT32 must be more than 16 bits
                            arrays up to 65536 bytes must be supported
                            machine must be twos complement

History
991031  ZV
    Added NSC-8105 support

990319  HJB
    Fixed wrong LSB/MSB order for push/pull word.
    Subtract .extra_cycles at the beginning/end of the exectuion loops.

990316  HJB
    Renamed to 6800, since that's the basic CPU.
    Added different cycle count tables for M6800/2/8, M6801/3 and HD63701.

990314  HJB
    Also added the M6800 subtype.

990311  HJB
    Added _info functions. Now uses static m6808_Regs struct instead
    of single statics. Changed the 16 bit registers to use the generic
    PAIR union. Registers defined using macros. Split the core into
    four execution loops for M6802, M6803, M6808 and HD63701.
    TST, TSTA and TSTB opcodes reset carry flag.
TODO:
    Verify invalid opcodes for the different CPU types.
    Add proper credits to _info functions.
    Integrate m6808_Flags into the registers (multiple m6808 type CPUs?)

990301  HJB
    Modified the interrupt handling. No more pending interrupt checks.
    WAI opcode saves state, when an interrupt is taken (IRQ or OCI),
    the state is only saved if not already done by WAI.

*****************************************************************************/


//#include "debugger.h"
#include "burnint.h"
#include "m6800.h"
#include <stddef.h>

#define VERBOSE 0

#if VERBOSE
#define LOG(x)	logerror x
#else
#define LOG(x)
#endif

#define M6800_INLINE		static
//#define change_pc(newpc)	m6800.pc.w.l = (newpc)
#define M6800_CLEAR_LINE	0

#if 0
/* CPU subtypes, needed for extra insn after TAP/CLI/SEI */
enum {
	SUBTYPE_M6800,
	SUBTYPE_M6801,
	SUBTYPE_M6802,
	SUBTYPE_M6803,
	SUBTYPE_M6808,
	SUBTYPE_HD63701,
	SUBTYPE_NSC8105
};
#endif

/* 680x registers */
static m6800_Regs m6800;

#define m6801   m6800
#define m6802   m6800
#define m6803	m6800
#define m6808	m6800
#define hd63701 m6800
#define nsc8105 m6800

#define	pPPC	m6800.ppc
#define pPC 	m6800.pc
#define pS		m6800.s
#define pX		m6800.x
#define pD		m6800.d

#define PC		m6800.pc.w.l
#define PCD		m6800.pc.d
#define S		m6800.s.w.l
#define SD		m6800.s.d
#define X		m6800.x.w.l
#define D		m6800.d.w.l
#define A		m6800.d.b.h
#define B		m6800.d.b.l
#define CC		m6800.cc

#define CT		m6800.counter.w.l
#define CTH		m6800.counter.w.h
#define CTD		m6800.counter.d
#define OC		m6800.output_compare.w.l
#define OCH		m6800.output_compare.w.h
#define OCD		m6800.output_compare.d
#define TOH		m6800.timer_over.w.h
#define TOD		m6800.timer_over.d

//static PAIR ea; 		/* effective address */
#define ea		m6800.ea
#define EAD		ea.d
#define EA		ea.w.l

/* public globals */
#define m6800_ICount	m6800.m6800_ICount

/* point of next timer event */
#define timer_next		m6800.timer_next

/* DS -- THESE ARE RE-DEFINED IN m6800.h TO RAM, ROM or FUNCTIONS IN cpuintrf.c */
#define RM				M6800_RDMEM
#define WM				M6800_WRMEM
#define M_RDOP			M6800_RDOP
#define M_RDOP_ARG		M6800_RDOP_ARG

/* macros to access memory */
#define IMMBYTE(b)	b = M_RDOP_ARG(PCD); PC++
#define IMMWORD(w)	w.d = (M_RDOP_ARG(PCD)<<8) | M_RDOP_ARG((PCD+1)&0xffff); PC+=2

#define PUSHBYTE(b) WM(SD,b); --S
#define PUSHWORD(w) WM(SD,w.b.l); --S; WM(SD,w.b.h); --S
#define PULLBYTE(b) S++; b = RM(SD)
#define PULLWORD(w) S++; w.d = RM(SD)<<8; S++; w.d |= RM(SD)

#define MODIFIED_tcsr {	\
	m6800.irq2 = (m6800.tcsr&(m6800.tcsr<<3))&(TCSR_ICF|TCSR_OCF|TCSR_TOF); \
}

#define SET_TIMER_EVENT {					\
	timer_next = (OCD - CTD < TOD - CTD) ? OCD : TOD;	\
}

/* cleanup high-word of counters */
#define CLEANUP_conters {						\
	OCH -= CTH;									\
	TOH -= CTH;									\
	CTH = 0;									\
	SET_TIMER_EVENT;							\
}

/* when change freerunningcounter or outputcapture */
#define MODIFIED_counters {						\
	OCH = (OC >= CT) ? CTH : CTH+1;				\
	SET_TIMER_EVENT;							\
}

/* take interrupt */
#define TAKE_ICI ENTER_INTERRUPT("M6800#%d take ICI\n",0xfff6)
#define TAKE_OCI ENTER_INTERRUPT("M6800#%d take OCI\n",0xfff4)
#define TAKE_TOI ENTER_INTERRUPT("M6800#%d take TOI\n",0xfff2)
#define TAKE_SCI ENTER_INTERRUPT("M6800#%d take SCI\n",0xfff0)
#define TAKE_TRAP ENTER_INTERRUPT("M6800#%d take TRAP\n",0xffee)

/* check IRQ2 (internal irq) */
#define CHECK_IRQ2 {											\
	if(m6800.irq2&(TCSR_ICF|TCSR_OCF|TCSR_TOF))					\
	{															\
		if(m6800.irq2&TCSR_ICF)									\
		{														\
			TAKE_ICI;											\
		}														\
		else if(m6800.irq2&TCSR_OCF)							\
		{														\
			TAKE_OCI;											\
		}														\
		else if(m6800.irq2&TCSR_TOF)							\
		{														\
			TAKE_TOI;											\
		}														\
	}															\
}

/* operate one instruction for */
#define ONE_MORE_INSN() {		\
	UINT8 ireg; 							\
	pPPC = pPC; 							\
	ireg=M_RDOP(PCD);						\
	PC++;									\
	(*m6800.insn[ireg])();					\
	INCREMENT_COUNTER(m6800.cycles[ireg]);	\
}

/* check the IRQ lines for pending interrupts */
#define CHECK_IRQ_LINES() {										\
	{															\
		if( m6800.irq_state[M6800_IRQ_LINE] != M6800_CLEAR_LINE )		\
		{	/* standard IRQ */									\
			if(m6800.wai_state & M6800_SLP)                     \
				m6800.wai_state &= ~M6800_SLP;                  \
	        if( !(CC & 0x10) ) { 								\
			    ENTER_INTERRUPT("M6800#%d take IRQ1\n",0xfff8);	\
        	    if (m6800.irq_hold[M6800_IRQ_LINE])             \
	                m6800_set_irq_line(M6800_IRQ_LINE, 0);      \
    	    }													\
    	} else { 												\
	        if( !(CC & 0x10) ) { 								\
	            CHECK_IRQ2;										\
	            if (m6800.irq_hold[M6800_TIN_LINE])             \
                    m6800_set_irq_line(M6800_TIN_LINE, 0);      \
																\
    	    }													\
    	}														\
	}															\
}

/* CC masks                       HI NZVC
                                7654 3210   */
#define CLR_HNZVC	CC&=0xd0
#define CLR_NZV 	CC&=0xf1
#define CLR_HNZC	CC&=0xd2
#define CLR_NZVC	CC&=0xf0
#define CLR_Z		CC&=0xfb
#define CLR_ZC		CC&=0xfa
#define CLR_C		CC&=0xfe

/* macros for CC -- CC bits affected should be reset before calling */
#define SET_Z(a)		if(!(a))SEZ
#define SET_Z8(a)		SET_Z((UINT8)(a))
#define SET_Z16(a)		SET_Z((UINT16)(a))
#define SET_N8(a)		CC|=(((a)&0x80)>>4)
#define SET_N16(a)		CC|=(((a)&0x8000)>>12)
#define SET_H(a,b,r)	CC|=((((a)^(b)^(r))&0x10)<<1)
#define SET_C8(a)		CC|=(((a)&0x100)>>8)
#define SET_C16(a)		CC|=(((a)&0x10000)>>16)
#define SET_V8(a,b,r)	CC|=((((a)^(b)^(r)^((r)>>1))&0x80)>>6)
#define SET_V16(a,b,r)	CC|=((((a)^(b)^(r)^((r)>>1))&0x8000)>>14)

static const UINT8 flags8i[256]=	 /* increment */
{
0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x0a,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08
};
static const UINT8 flags8d[256]= /* decrement */
{
0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,
0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08
};
#define SET_FLAGS8I(a)		{CC|=flags8i[(a)&0xff];}
#define SET_FLAGS8D(a)		{CC|=flags8d[(a)&0xff];}

/* combos */
#define SET_NZ8(a)			{SET_N8(a);SET_Z8(a);}
#define SET_NZ16(a)			{SET_N16(a);SET_Z16(a);}
#define SET_FLAGS8(a,b,r)	{SET_N8(r);SET_Z8(r);SET_V8(a,b,r);SET_C8(r);}
#define SET_FLAGS16(a,b,r)	{SET_N16(r);SET_Z16(r);SET_V16(a,b,r);SET_C16(r);}

/* for treating an UINT8 as a signed INT16 */
#define SIGNED(b) ((INT16)(b&0x80?b|0xff00:b))

/* Macros for addressing modes */
#define DIRECT IMMBYTE(EAD)
#define IMM8 EA=PC++
#define IMM16 {EA=PC;PC+=2;}
#define EXTENDED IMMWORD(ea)
#define INDEXED {EA=X+(UINT8)M_RDOP_ARG(PCD);PC++;}

/* macros to set status flags */
#if defined(SEC)
#undef SEC
#endif
#define SEC CC|=0x01
#define CLC CC&=0xfe
#define SEZ CC|=0x04
#define CLZ CC&=0xfb
#define SEN CC|=0x08
#define CLN CC&=0xf7
#define SEV CC|=0x02
#define CLV CC&=0xfd
#define SEH CC|=0x20
#define CLH CC&=0xdf
#define SEI CC|=0x10
#define CLI CC&=~0x10

/* mnemonicos for the Timer Control and Status Register bits */
#define TCSR_OLVL 0x01
#define TCSR_IEDG 0x02
#define TCSR_ETOI 0x04
#define TCSR_EOCI 0x08
#define TCSR_EICI 0x10
#define TCSR_TOF  0x20
#define TCSR_OCF  0x40
#define TCSR_ICF  0x80

#define INCREMENT_COUNTER(amount)	\
{									\
	m6800_ICount -= amount;			\
	CTD += amount;					\
	if( CTD >= timer_next)			\
		check_timer_event();		\
}

#define EAT_CYCLES													\
{																	\
	int cycles_to_eat;												\
																	\
	cycles_to_eat = timer_next - CTD;								\
	if( cycles_to_eat > m6800_ICount) cycles_to_eat = m6800_ICount;	\
	if (cycles_to_eat > 0)											\
	{																\
		INCREMENT_COUNTER(cycles_to_eat);							\
	}																\
}

/* macros for convenience */
#define DIRBYTE(b) {DIRECT;b=RM(EAD);}
#define DIRWORD(w) {DIRECT;w.d=RM16(EAD);}
#define EXTBYTE(b) {EXTENDED;b=RM(EAD);}
#define EXTWORD(w) {EXTENDED;w.d=RM16(EAD);}

#define IDXBYTE(b) {INDEXED;b=RM(EAD);}
#define IDXWORD(w) {INDEXED;w.d=RM16(EAD);}

/* Macros for branch instructions */
#define BRANCH(f) {IMMBYTE(t);if(f){PC+=SIGNED(t);}}
#define NXORV  ((CC&0x08)^((CC&0x02)<<2))
#define NXORC  ((CC&0x08)^((CC&0x01)<<3))

/* Note: we use 99 cycles here for invalid opcodes so that we don't */
/* hang in an infinite loop if we hit one */
#define XX 5 // invalid opcode unknown cc
static const UINT8 cycles_6800[] =
{
		/* 0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F */
	/*0*/ XX, 2,XX,XX,XX,XX, 2, 2, 4, 4, 2, 2, 2, 2, 2, 2,
	/*1*/  2, 2,XX,XX,XX,XX, 2, 2,XX, 2,XX, 2,XX,XX,XX,XX,
	/*2*/  4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
	/*3*/  4, 4, 4, 4, 4, 4, 4, 4,XX, 5,XX,10,XX,XX, 9,12,
	/*4*/  2,XX,XX, 2, 2,XX, 2, 2, 2, 2, 2,XX, 2, 2,XX, 2,
	/*5*/  2,XX,XX, 2, 2,XX, 2, 2, 2, 2, 2,XX, 2, 2,XX, 2,
	/*6*/  7,XX,XX, 7, 7,XX, 7, 7, 7, 7, 7,XX, 7, 7, 4, 7,
	/*7*/  6,XX,XX, 6, 6,XX, 6, 6, 6, 6, 6,XX, 6, 6, 3, 6,
	/*8*/  2, 2, 2,XX, 2, 2, 2, 3, 2, 2, 2, 2, 3, 8, 3, 4,
	/*9*/  3, 3, 3,XX, 3, 3, 3, 4, 3, 3, 3, 3, 4, 6, 4, 5,
	/*A*/  5, 5, 5,XX, 5, 5, 5, 6, 5, 5, 5, 5, 6, 8, 6, 7,
	/*B*/  4, 4, 4,XX, 4, 4, 4, 5, 4, 4, 4, 4, 5, 9, 5, 6,
	/*C*/  2, 2, 2,XX, 2, 2, 2, 3, 2, 2, 2, 2,XX,XX, 3, 4,
	/*D*/  3, 3, 3,XX, 3, 3, 3, 4, 3, 3, 3, 3,XX,XX, 4, 5,
	/*E*/  5, 5, 5,XX, 5, 5, 5, 6, 5, 5, 5, 5,XX,XX, 6, 7,
	/*F*/  4, 4, 4,XX, 4, 4, 4, 5, 4, 4, 4, 4,XX,XX, 5, 6
};

static const UINT8 cycles_6803[] =
{
		/* 0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F */
	/*0*/ XX, 2,XX,XX, 3, 3, 2, 2, 3, 3, 2, 2, 2, 2, 2, 2,
	/*1*/  2, 2,XX,XX,XX,XX, 2, 2,XX, 2,XX, 2,XX,XX,XX,XX,
	/*2*/  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	/*3*/  3, 3, 4, 4, 3, 3, 3, 3, 5, 5, 3,10, 4,10, 9,12,
	/*4*/  2,XX,XX, 2, 2,XX, 2, 2, 2, 2, 2,XX, 2, 2,XX, 2,
	/*5*/  2,XX,XX, 2, 2,XX, 2, 2, 2, 2, 2,XX, 2, 2,XX, 2,
	/*6*/  6,XX,XX, 6, 6,XX, 6, 6, 6, 6, 6,XX, 6, 6, 3, 6,
	/*7*/  6,XX,XX, 6, 6,XX, 6, 6, 6, 6, 6,XX, 6, 6, 3, 6,
	/*8*/  2, 2, 2, 4, 2, 2, 2, 2, 2, 2, 2, 2, 4, 6, 3, 3,
	/*9*/  3, 3, 3, 5, 3, 3, 3, 3, 3, 3, 3, 3, 5, 5, 4, 4,
	/*A*/  4, 4, 4, 6, 4, 4, 4, 4, 4, 4, 4, 4, 6, 6, 5, 5,
	/*B*/  4, 4, 4, 6, 4, 4, 4, 4, 4, 4, 4, 4, 6, 6, 5, 5,
	/*C*/  2, 2, 2, 4, 2, 2, 2, 2, 2, 2, 2, 2, 3,XX, 3, 3,
	/*D*/  3, 3, 3, 5, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4,
	/*E*/  4, 4, 4, 6, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5,
	/*F*/  4, 4, 4, 6, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5
};

static const UINT8 cycles_63701[] =
{
		/* 0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F */
	/*0*/ XX, 1,XX,XX, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	/*1*/  1, 1,XX,XX,XX,XX, 1, 1, 2, 2, 4, 1,XX,XX,XX,XX,
	/*2*/  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	/*3*/  1, 1, 3, 3, 1, 1, 4, 4, 4, 5, 1,10, 5, 7, 9,12,
	/*4*/  1,XX,XX, 1, 1,XX, 1, 1, 1, 1, 1,XX, 1, 1,XX, 1,
	/*5*/  1,XX,XX, 1, 1,XX, 1, 1, 1, 1, 1,XX, 1, 1,XX, 1,
	/*6*/  6, 7, 7, 6, 6, 7, 6, 6, 6, 6, 6, 5, 6, 4, 3, 5,
	/*7*/  6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 4, 6, 4, 3, 5,
	/*8*/  2, 2, 2, 3, 2, 2, 2, 2, 2, 2, 2, 2, 3, 5, 3, 3,
	/*9*/  3, 3, 3, 4, 3, 3, 3, 3, 3, 3, 3, 3, 4, 5, 4, 4,
	/*A*/  4, 4, 4, 5, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5,
	/*B*/  4, 4, 4, 5, 4, 4, 4, 4, 4, 4, 4, 4, 5, 6, 5, 5,
	/*C*/  2, 2, 2, 3, 2, 2, 2, 2, 2, 2, 2, 2, 3,XX, 3, 3,
	/*D*/  3, 3, 3, 4, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4,
	/*E*/  4, 4, 4, 5, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5,
	/*F*/  4, 4, 4, 5, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5
};

static const UINT8 cycles_nsc8105[] =
{
		/* 0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F */
	/*0*/  5,XX, 2,XX,XX, 2,XX, 2, 4, 2, 4, 2, 2, 2, 2, 2,
	/*1*/  2,XX, 2,XX,XX, 2,XX, 2,XX,XX, 2, 2,XX,XX,XX,XX,
	/*2*/  4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
	/*3*/  4, 4, 4, 4, 4, 4, 4, 4,XX,XX, 5,10,XX, 9,XX,12,
	/*4*/  2, 2, 2,XX, 2, 2, 2, 3, 2, 2, 2, 2, 3, 3, 8, 4,
	/*5*/  3, 3, 3,XX, 3, 3, 3, 4, 3, 3, 3, 3, 4, 4, 6, 5,
	/*6*/  5, 5, 5,XX, 5, 5, 5, 6, 5, 5, 5, 5, 6, 6, 8, 7,
	/*7*/  4, 4, 4,XX, 4, 4, 4, 5, 4, 4, 4, 4, 5, 5, 9, 6,
	/*8*/  2,XX,XX, 2, 2, 2,XX, 2, 2, 2, 2,XX, 2,XX, 2, 2,
	/*9*/  2,XX,XX, 2, 2, 2,XX, 2, 2, 2, 2,XX, 2,XX, 2, 2,
	/*A*/  7,XX,XX, 7, 7, 7,XX, 7, 7, 7, 7,XX, 7, 4, 7, 7,
	/*B*/  6,XX,XX, 6, 6, 6,XX, 6, 6, 6, 6, 5, 6, 3, 6, 6,
	/*C*/  2, 2, 2,XX, 2, 2, 2, 3, 2, 2, 2, 2,XX, 3,XX, 4,
	/*D*/  3, 3, 3,XX, 3, 3, 3, 4, 3, 3, 3, 3,XX, 4,XX, 5,
	/*E*/  5, 5, 5,XX, 5, 5, 5, 6, 5, 5, 5, 5, 5, 6,XX, 7,
	/*F*/  4, 4, 4,XX, 4, 4, 4, 5, 4, 4, 4, 4, 4, 5,XX, 6
};
#undef XX // /invalid opcode unknown cc

M6800_INLINE UINT32 RM16( UINT32 Addr )
{
	UINT32 result = RM(Addr) << 8;
	return result | RM((Addr+1)&0xffff);
}

M6800_INLINE void WM16( UINT32 Addr, PAIR *p )
{
	WM( Addr, p->b.h );
	WM( (Addr+1)&0xffff, p->b.l );
}

/* IRQ enter */
static void ENTER_INTERRUPT(const char *,UINT16 irq_vector)
{
//	LOG((message, cpu_getactivecpu()));
	if( m6800.wai_state & (M6800_WAI|M6800_SLP) )
	{
		if( m6800.wai_state & M6800_WAI )
			m6800.extra_cycles += 4;
		m6800.wai_state &= ~(M6800_WAI|M6800_SLP);
	}
	else
	{
		PUSHWORD(pPC);
		PUSHWORD(pX);
		PUSHBYTE(A);
		PUSHBYTE(B);
		PUSHBYTE(CC);
		m6800.extra_cycles += 12;
	}
	SEI;
	PCD = RM16( irq_vector );
}

/* check OCI or TOI */
static void check_timer_event(void)
{
	/* OCI */
	if( CTD >= OCD)
	{
		OCH++;	// next IRQ point
		m6800.tcsr |= TCSR_OCF;
		m6800.pending_tcsr |= TCSR_OCF;
		MODIFIED_tcsr;
		if ( !(CC & 0x10) && (m6800.tcsr & TCSR_EOCI))
			TAKE_OCI;
	}
	/* TOI */
	if( CTD >= TOD)
	{
		TOH++;	// next IRQ point
#if 0
		CLEANUP_conters;
#endif
		m6800.tcsr |= TCSR_TOF;
		m6800.pending_tcsr |= TCSR_TOF;
		MODIFIED_tcsr;
		if ( !(CC & 0x10) && (m6800.tcsr & TCSR_ETOI))
			TAKE_TOI;
	}
	/* set next event */
	SET_TIMER_EVENT;
}

/* include the opcode prototypes and function pointer tables */
#include "6800tbl.c"

/* include the opcode functions */
#include "6800ops.c"

/****************************************************************************
 * Reset registers to their initial values
 ****************************************************************************/

void m6800_init()
{
//  m6800.subtype   = SUBTYPE_M6800;
	m6800.insn = m6800_insn;
	m6800.cycles = cycles_6800;
//	m6800.irq_callback = irqcallback;
//	state_register("m6800", index);
}

void m6800_reset_soft(void)
{
	m6800.cc = 0xc0;
	SEI;				/* IRQ disabled */
	PCD = RM16( 0xfffe );

	m6800.wai_state = 0;
	m6800.nmi_state = 0;
	m6800.irq_state[M6800_IRQ_LINE] = 0;
	m6800.irq_state[M6800_TIN_LINE] = 0;
	m6800.ic_eddge = 0;

	m6800.port1_data = 0x00;
	m6800.port1_ddr = 0x00;
	m6800.port2_ddr = 0x00;
	m6800.port3_ddr = 0x00;
	m6800.portx_ddr[0] = 0x00;
	m6800.portx_ddr[1] = 0x00;

	/* TODO: on reset port 2 should be read to determine the operating mode (bits 0-2) */
	m6800.tcsr = 0x00;
	m6800.pending_tcsr = 0x00;
	m6800.irq2 = 0;
	CTD = 0x0000;
	OCD = 0xffff;
	TOD = 0xffff;
	m6800.ram_ctrl |= 0x40;
}

void m6800_reset(void)
{
	// clear context
	memset(&m6800, 0, STRUCT_SIZE_HELPER(m6800_Regs, timer_over));

	m6800_reset_soft();
}

int m6800_get_pc()
{
	return PC;
}

/****************************************************************************
 * Shut down CPU emulatio
 ****************************************************************************/
/*
static void m6800_exit(void)
{

}*/

/****************************************************************************
 * Get all registers in given buffer
 ****************************************************************************/
void m6800_get_context(void *dst)
{
	if( dst )
		*(m6800_Regs*)dst = m6800;
}


/****************************************************************************
 * Set all registers to given values
 ****************************************************************************/
void m6800_set_context(void *src)
{
	if( src )
		m6800 = *(m6800_Regs*)src;
	//CHANGE_PC();
	//CHECK_IRQ_LINES(); /* HJB 990417 */
}


void m6800_set_irq_line(int irqline, int state)
{
	int hold = 0;

	if (state == 2) { hold = 1; state = 1; }

	if (irqline == M6800_INPUT_LINE_NMI)
	{
		if (m6800.nmi_state == state) return;
		LOG(("M6800#%d set_nmi_line %d \n", cpu_getactivecpu(), state));
		m6800.nmi_state = state;
		if (state == M6800_CLEAR_LINE) return;

		/* NMI */
		ENTER_INTERRUPT("M6800#%d take NMI\n",0xfffc);
	}
	else
	{
		int eddge;

		if (m6800.irq_state[irqline] == state) return;
		LOG(("M6800#%d set_irq_line %d,%d\n", cpu_getactivecpu(), irqline, state));
		m6800.irq_state[irqline] = state;
		m6800.irq_hold[irqline] = hold;

		switch(irqline)
		{
		case M6800_IRQ_LINE:
			if (state == M6800_CLEAR_LINE) return;
			break;
		case M6800_TIN_LINE:
			eddge = (state == M6800_CLEAR_LINE ) ? 2 : 0;
			if( ((m6800.tcsr&TCSR_IEDG) ^ (state==M6800_CLEAR_LINE ? TCSR_IEDG : 0))==0 )
				return;
			/* active edge in */
			m6800.tcsr |= TCSR_ICF;
			m6800.pending_tcsr |= TCSR_ICF;
			m6800.input_capture = CT;
			MODIFIED_tcsr;
			if( !(CC & 0x10) )
				CHECK_IRQ2
			break;
		default:
			return;
		}
		CHECK_IRQ_LINES(); /* HJB 990417 */
	}
}

int m6800_get_segmentcycles()
{
	return m6800.segmentcycles - m6800_ICount;
}

static INT32 end_run;

/****************************************************************************
 * Execute cycles CPU cycles. Return number of cycles really executed
 ****************************************************************************/
int m6800_execute(int cycles)
{
	UINT8 ireg;

	m6800.segmentcycles = cycles;
	m6800_ICount = cycles;
	CHECK_IRQ_LINES();

	CLEANUP_conters;
	INCREMENT_COUNTER(m6800.extra_cycles);
	m6800.extra_cycles = 0;

	end_run = 0;

	do
	{
		if( m6800.wai_state & (M6800_WAI|M6800_SLP) )
		{
			EAT_CYCLES;
		}
		else
		{
			pPPC = pPC;
//			CALL_MAME_DEBUG;
			ireg=M_RDOP(PCD);
			PC++;
			(*m6800.insn[ireg])();
			INCREMENT_COUNTER(cycles_6800[ireg]);
		}

	} while( m6800_ICount>0 && !end_run );

	INCREMENT_COUNTER(m6800.extra_cycles);
	m6800.extra_cycles = 0;

	cycles = cycles - m6800_ICount;

	m6800.segmentcycles = m6800_ICount = 0;

	return cycles;
}

/****************************************************************************
 * M6801 almost (fully?) equal to the M6803
 ****************************************************************************/
#if (HAS_M6801)
void m6801_init()
{
//  m6800.subtype = SUBTYPE_M6801;
	m6800.insn = m6803_insn;
	m6800.cycles = cycles_6803;
//	m6800.irq_callback = irqcallback;
//	state_register("m6801", index);
}
#endif

/****************************************************************************
 * M6802 almost (fully?) equal to the M6800
 ****************************************************************************/
#if (HAS_M6802)
static void m6802_init(int index, int clock, const void *config, int (*irqcallback)(int))
{
//  m6800.subtype   = SUBTYPE_M6802;
	m6800.insn = m6800_insn;
	m6800.cycles = cycles_6800;
	m6800.irq_callback = irqcallback;
	state_register("m6802", index);
}
#endif

/****************************************************************************
 * M6803 almost (fully?) equal to the M6801
 ****************************************************************************/
#if (HAS_M6803)
void m6803_init()
{
//  m6800.subtype = SUBTYPE_M6803;
	m6800.insn = m6803_insn;
	m6800.cycles = cycles_6803;
//	m6800.irq_callback = irqcallback;
//	state_register("m6803", index);
}
#endif

/****************************************************************************
 * Execute cycles CPU cycles. Return number of cycles really executed
 ****************************************************************************/
#if (HAS_M6803||HAS_M6801)
int m6803_execute(int cycles)
{
	UINT8 ireg;

	m6800.segmentcycles = cycles;
	m6800_ICount = cycles;

	CLEANUP_conters;
	INCREMENT_COUNTER(m6803.extra_cycles);
	m6803.extra_cycles = 0;

	end_run = 0;

	do
	{
		if( m6803.wai_state & M6800_WAI )
		{
			EAT_CYCLES;
		}
		else
		{
			pPPC = pPC;
//			CALL_MAME_DEBUG;
			ireg=M_RDOP(PCD);
			PC++;
			(*m6803.insn[ireg])();
			INCREMENT_COUNTER(cycles_6803[ireg]);
		}

	} while( m6800_ICount>0 && !end_run );

	INCREMENT_COUNTER(m6803.extra_cycles);
	m6803.extra_cycles = 0;

	cycles = cycles - m6800_ICount;

	m6800.segmentcycles = m6800_ICount = 0;

	return cycles;
}
#endif

#if (HAS_M6803)

//static READ8_HANDLER( m6803_internal_registers_r );
//static WRITE8_HANDLER( m6803_internal_registers_w );

//static ADDRESS_MAP_START(m6803_mem, ADDRESS_SPACE_PROGRAM, 8)
//	AM_RANGE(0x0000, 0x001f) AM_READWRITE(m6803_internal_registers_r, m6803_internal_registers_w)
//	AM_RANGE(0x0020, 0x007f) AM_NOP        /* unused */
//	AM_RANGE(0x0080, 0x00ff) AM_RAM        /* 6803 internal RAM */
//ADDRESS_MAP_END

#endif

/****************************************************************************
 * M6808 almost (fully?) equal to the M6800
 ****************************************************************************/
#if (HAS_M6808)
static void m6808_init(int index, int clock, const void *config, int (*irqcallback)(int))
{
//  m6800.subtype = SUBTYPE_M6808;
	m6800.insn = m6800_insn;
	m6800.cycles = cycles_6800;
	m6800.irq_callback = irqcallback;
	state_register("m6808", index);
}
#endif

/****************************************************************************
 * HD63701 similiar to the M6800
 ****************************************************************************/
#if (HAS_HD63701)
void hd63701_init()
{
//  m6800.subtype = SUBTYPE_HD63701;
	m6800.insn = hd63701_insn;
	m6800.cycles = cycles_63701;
//	m6800.irq_callback = irqcallback;
//	state_register("hd63701", index);
}
/****************************************************************************
 * Execute cycles CPU cycles. Return number of cycles really executed
 ****************************************************************************/
int hd63701_execute(int cycles)
{
	UINT8 ireg;

	m6800.segmentcycles = cycles;
	m6800_ICount = cycles;

	CLEANUP_conters;
	INCREMENT_COUNTER(hd63701.extra_cycles);
	hd63701.extra_cycles = 0;

	end_run = 0;

	do
	{
		if( hd63701.wai_state & (HD63701_WAI|HD63701_SLP) )
		{
			EAT_CYCLES;
		}
		else
		{
			pPPC = pPC;
//			CALL_MAME_DEBUG;
			ireg=M_RDOP(PCD);
			PC++;
			(*hd63701.insn[ireg])();
			INCREMENT_COUNTER(cycles_63701[ireg]);
		}
	} while( m6800_ICount>0 && !end_run );

	INCREMENT_COUNTER(hd63701.extra_cycles);
	hd63701.extra_cycles = 0;

	cycles = cycles - m6800_ICount;

	m6800.segmentcycles = m6800_ICount = 0;

	return cycles;
}

/*
    if change_pc() direccted these areas ,Call hd63701_trap_pc().
    'mode' is selected by the sense of p2.0,p2.1,and p2.3 at reset timming.
    mode 0,1,2,4,6 : $0000-$001f
    mode 5         : $0000-$001f,$0200-$efff
    mode 7         : $0000-$001f,$0100-$efff
*/
void hd63701_trap_pc(void)
{
	TAKE_TRAP;
}

//static READ8_HANDLER( m6803_internal_registers_r );
//static WRITE8_HANDLER( m6803_internal_registers_w );

//READ8_HANDLER( hd63701_internal_registers_r )
//{
//	return m6803_internal_registers_r(offset);
//}

//WRITE8_HANDLER( hd63701_internal_registers_w )
//{
//	m6803_internal_registers_w(offset,data);
//}
#endif

/****************************************************************************
 * NSC-8105 similiar to the M6800, but the opcodes are scrambled and there
 * is at least one new opcode ($fc)
 ****************************************************************************/
#if (HAS_NSC8105)
void nsc8105_init()
{
//  m6800.subtype = SUBTYPE_NSC8105;
	m6800.insn = nsc8105_insn;
	m6800.cycles = cycles_nsc8105;
	//state_register("nsc8105", index);
}


/****************************************************************************
 * Execute cycles CPU cycles. Return number of cycles really executed
 ****************************************************************************/
int nsc8105_execute(int cycles)
{
	UINT8 ireg;

	m6800.segmentcycles = cycles;
	m6800_ICount = cycles;

	CLEANUP_conters;
	INCREMENT_COUNTER(nsc8105.extra_cycles);
	nsc8105.extra_cycles = 0;

	end_run = 0;

	do
	{
		if( nsc8105.wai_state & NSC8105_WAI )
		{
			EAT_CYCLES;
		}
		else
		{
			pPPC = pPC;
			//CALL_MAME_DEBUG;
			ireg=M_RDOP(PCD);
			PC++;
			(*nsc8105.insn[ireg])();
			INCREMENT_COUNTER(cycles_nsc8105[ireg]);
		}
	} while( m6800_ICount>0 && !end_run );

	INCREMENT_COUNTER(nsc8105.extra_cycles);
	nsc8105.extra_cycles = 0;

	cycles = cycles - m6800_ICount;

	m6800.segmentcycles = m6800_ICount = 0;

	return cycles;
}
#endif

void M6800RunEnd()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_M6800Initted) bprintf(PRINT_ERROR, _T("M6800RunEnd called without init\n"));
#endif

	end_run = 1;
}

#if (HAS_M6803||HAS_HD63701)
// xdinkx
unsigned char hd63xy_internal_registers_r(unsigned short offset)
{
	switch (offset)
	{
		case 0x15: // port 5 data read
			return (M6800_io_read_byte_8(HD63701_PORT5) & (m6800.portx_ddr[0] ^ 0xff))
				| (m6800.portx_data[0] & m6800.portx_ddr[0]);
		case 0x17: // port 6 data read
			return (M6800_io_read_byte_8(HD63701_PORT6) & (m6800.portx_ddr[1] ^ 0xff))
				| (m6800.portx_data[1] & m6800.portx_ddr[1]);
		case 0x18: // port 7 data read
			return 0xe0 | m6800.portx_data[2];
	}

	return m6803_internal_registers_r(offset);
}

unsigned char m6803_internal_registers_r(unsigned short offset)
{
	switch (offset)
	{
		case 0x00:
			return m6800.port1_ddr;
		case 0x01:
			return m6800.port2_ddr;
		case 0x02:
			return (M6800_io_read_byte_8(M6803_PORT1) & (m6800.port1_ddr ^ 0xff))
					| (m6800.port1_data & m6800.port1_ddr);
		case 0x03:
			return (M6800_io_read_byte_8(M6803_PORT2) & (m6800.port2_ddr ^ 0xff))
					| (m6800.port2_data & m6800.port2_ddr);
		case 0x04:
			return m6800.port3_ddr;
		case 0x05:
			return m6800.port4_ddr;
		case 0x06:
			return (M6800_io_read_byte_8(M6803_PORT3) & (m6800.port3_ddr ^ 0xff))
					| (m6800.port3_data & m6800.port3_ddr);
		case 0x07:
			return (M6800_io_read_byte_8(M6803_PORT4) & (m6800.port4_ddr ^ 0xff))
					| (m6800.port4_data & m6800.port4_ddr);
		case 0x08:
			m6800.pending_tcsr = 0;
//logerror("CPU #%d PC %04x: warning - read TCSR register\n",cpu_getactivecpu(),activecpu_get_pc());
			return m6800.tcsr;
		case 0x09:
			if(!(m6800.pending_tcsr&TCSR_TOF))
			{
				m6800.tcsr &= ~TCSR_TOF;
				MODIFIED_tcsr;
			}
			return m6800.counter.b.h;
		case 0x0a:
			return m6800.counter.b.l;
		case 0x0b:
			return m6800.output_compare.b.h;
		case 0x0c:
			return m6800.output_compare.b.l;
		case 0x0d:
			if(!(m6800.pending_tcsr&TCSR_ICF))
			{
				m6800.tcsr &= ~TCSR_ICF;
				MODIFIED_tcsr;
			}
			return (m6800.input_capture >> 0) & 0xff;
		case 0x0e:
			return (m6800.input_capture >> 8) & 0xff;
		case 0x0f:
		case 0x10:
		case 0x11:
		case 0x12:
		case 0x13:
//			logerror("CPU #%d PC %04x: warning - read from unsupported internal register %02x\n",cpu_getactivecpu(),activecpu_get_pc(),offset);
			return 0;
		case 0x14:
//			logerror("CPU #%d PC %04x: read RAM control register\n",cpu_getactivecpu(),activecpu_get_pc());
			return m6800.ram_ctrl;
		case 0x15:
		case 0x16:
		case 0x17:
		case 0x18:
		case 0x19:
		case 0x1a:
		case 0x1b:
		case 0x1c:
		case 0x1d:
		case 0x1e:
		case 0x1f:
		default:
//			logerror("CPU #%d PC %04x: warning - read from reserved internal register %02x\n",cpu_getactivecpu(),activecpu_get_pc(),offset);
			return 0;
	}
}

void hd63xy_internal_registers_w(unsigned short offset, unsigned char data)
{
	switch (offset)
	{
		case 0x15: // port 5 data write
			m6800.portx_data[0] = data;
			if(m6800.portx_ddr[0] == 0xff)
				M6800_io_write_byte_8(HD63701_PORT5,m6800.portx_data[0]);
			else
				M6800_io_write_byte_8(HD63701_PORT5,(m6800.portx_data[0] & m6800.portx_ddr[0])
					| (M6800_io_read_byte_8(HD63701_PORT5) & (m6800.portx_ddr[0] ^ 0xff)));
			break;

		case 0x16: // port 6 ddr write
			if (m6800.portx_ddr[1] != data)
			{
			   m6800.portx_ddr[1] = data;
				if(m6800.portx_ddr[1] == 0xff)
					M6800_io_write_byte_8(HD63701_PORT6,m6800.portx_data[1]);
				else
					M6800_io_write_byte_8(HD63701_PORT6,(m6800.portx_data[1] & m6800.portx_ddr[1])
						| (M6800_io_read_byte_8(HD63701_PORT6) & (m6800.portx_ddr[1] ^ 0xff)));
			}
			break;

		case 0x17: // port 6 data write
			m6800.portx_data[1] = data;
			if(m6800.portx_ddr[1] == 0xff)
				M6800_io_write_byte_8(HD63701_PORT6,m6800.portx_data[1]);
			else
				M6800_io_write_byte_8(HD63701_PORT6,(m6800.portx_data[1] & m6800.portx_ddr[1])
					| (M6800_io_read_byte_8(HD63701_PORT6) & (m6800.portx_ddr[1] ^ 0xff)));
			break;

		case 0x18: // port 7 data write
			m6800.portx_data[2] = data & 0x1f;
			M6800_io_write_byte_8(HD63701_PORT7,m6800.portx_data[2]);
			break;

		case 0x20: // port 5 ddr write
			if (m6800.portx_ddr[0] != data)
			{
				m6800.portx_ddr[0] = data;
				if(m6800.portx_ddr[0] == 0xff)
					M6800_io_write_byte_8(HD63701_PORT5,m6800.portx_data[0]);
				else
					M6800_io_write_byte_8(HD63701_PORT5,(m6800.portx_data[0] & m6800.portx_ddr[0])
						| (M6800_io_read_byte_8(HD63701_PORT5) & (m6800.portx_ddr[0] ^ 0xff)));
			}
			break;


		default:
			m6803_internal_registers_w(offset, data);
			break;
	}
}

void m6803_internal_registers_w(unsigned short offset, unsigned char data)
{
	switch (offset)
	{
		case 0x00:
			if (m6800.port1_ddr != data)
			{
				m6800.port1_ddr = data;
				if(m6800.port1_ddr == 0xff)
					M6800_io_write_byte_8(M6803_PORT1,m6800.port1_data);
				else
					M6800_io_write_byte_8(M6803_PORT1,(m6800.port1_data & m6800.port1_ddr)
						| (M6800_io_read_byte_8(M6803_PORT1) & (m6800.port1_ddr ^ 0xff)));
			}
			break;
		case 0x01:
			if (m6800.port2_ddr != data)
			{
				m6800.port2_ddr = data;
				if(m6800.port2_ddr == 0xff)
					M6800_io_write_byte_8(M6803_PORT2,m6800.port2_data);
				else
					M6800_io_write_byte_8(M6803_PORT2,(m6800.port2_data & m6800.port2_ddr)
						| (M6800_io_read_byte_8(M6803_PORT2) & (m6800.port2_ddr ^ 0xff)));

//				if (m6800.port2_ddr & 2)
//					logerror("CPU #%d PC %04x: warning - port 2 bit 1 set as output (OLVL) - not supported\n",cpu_getactivecpu(),activecpu_get_pc());
			}
			break;
		case 0x02:
			m6800.port1_data = data;
			if(m6800.port1_ddr == 0xff)
				M6800_io_write_byte_8(M6803_PORT1,m6800.port1_data);
			else
				M6800_io_write_byte_8(M6803_PORT1,(m6800.port1_data & m6800.port1_ddr)
					| (M6800_io_read_byte_8(M6803_PORT1) & (m6800.port1_ddr ^ 0xff)));
			break;
		case 0x03:
			m6800.port2_data = data;
			m6800.port2_ddr = data;
			if(m6800.port2_ddr == 0xff)
				M6800_io_write_byte_8(M6803_PORT2,m6800.port2_data);
			else
				M6800_io_write_byte_8(M6803_PORT2,(m6800.port2_data & m6800.port2_ddr)
					| (M6800_io_read_byte_8(M6803_PORT2) & (m6800.port2_ddr ^ 0xff)));
			break;
		case 0x04:
			if (m6800.port3_ddr != data)
			{
				m6800.port3_ddr = data;
				if(m6800.port3_ddr == 0xff)
					M6800_io_write_byte_8(M6803_PORT3,m6800.port3_data);
				else
					M6800_io_write_byte_8(M6803_PORT3,(m6800.port3_data & m6800.port3_ddr)
						| (M6800_io_read_byte_8(M6803_PORT3) & (m6800.port3_ddr ^ 0xff)));
			}
			break;
		case 0x05:
			if (m6800.port4_ddr != data)
			{
				m6800.port4_ddr = data;
				if(m6800.port4_ddr == 0xff)
					M6800_io_write_byte_8(M6803_PORT4,m6800.port4_data);
				else
					M6800_io_write_byte_8(M6803_PORT4,(m6800.port4_data & m6800.port4_ddr)
						| (M6800_io_read_byte_8(M6803_PORT4) & (m6800.port4_ddr ^ 0xff)));
			}
			break;
		case 0x06:
			m6800.port3_data = data;
			if(m6800.port3_ddr == 0xff)
				M6800_io_write_byte_8(M6803_PORT3,m6800.port3_data);
			else
				M6800_io_write_byte_8(M6803_PORT3,(m6800.port3_data & m6800.port3_ddr)
					| (M6800_io_read_byte_8(M6803_PORT3) & (m6800.port3_ddr ^ 0xff)));
			break;
		case 0x07:
			m6800.port4_data = data;
			if(m6800.port4_ddr == 0xff)
				M6800_io_write_byte_8(M6803_PORT4,m6800.port4_data);
			else
				M6800_io_write_byte_8(M6803_PORT4,(m6800.port4_data & m6800.port4_ddr)
					| (M6800_io_read_byte_8(M6803_PORT4) & (m6800.port4_ddr ^ 0xff)));
			break;
		case 0x08:
			m6800.tcsr = data;
			m6800.pending_tcsr &= m6800.tcsr;
			MODIFIED_tcsr;
			if( !(CC & 0x10) )
				CHECK_IRQ2;
//logerror("CPU #%d PC %04x: TCSR = %02x\n",cpu_getactivecpu(),activecpu_get_pc(),data);
			break;
		case 0x09:
			m6800.latch09 = data & 0xff;	/* 6301 only */
			CT  = 0xfff8;
			TOH = CTH;
			MODIFIED_counters;
			break;
		case 0x0a:	/* 6301 only */
			CT = (m6800.latch09 << 8) | (data & 0xff);
			TOH = CTH;
			MODIFIED_counters;
			break;
		case 0x0b:
			// M6801U4.pdf pg.25 bottom-left "OCFI is cleared by reading the TCSR or the TSR (with OCFI set) and then writing to output compare register 1 ($OB or $OC); or during reset..." -dink [April 30, 2020]
			if(!(m6800.pending_tcsr&TCSR_OCF))
			{
				m6800.tcsr &= ~TCSR_OCF;
				MODIFIED_tcsr;
			}
			if( m6800.output_compare.b.h != data)
			{
				m6800.output_compare.b.h = data;
				MODIFIED_counters;
			}
			break;
		case 0x0c:
			// M6801U4.pdf pg.25 bottom-left "OCFI is cleared by reading the TCSR or the TSR (with OCFI set) and then writing to output compare register 1 ($OB or $OC); or during reset..." -dink [April 30, 2020]
			if(!(m6800.pending_tcsr&TCSR_OCF))
			{
				m6800.tcsr &= ~TCSR_OCF;
				MODIFIED_tcsr;
			}
			if( m6800.output_compare.b.l != data)
			{
				m6800.output_compare.b.l = data;
				MODIFIED_counters;
			}
			break;
		case 0x0d:
		case 0x0e:
//			logerror("CPU #%d PC %04x: warning - write %02x to read only internal register %02x\n",cpu_getactivecpu(),activecpu_get_pc(),data,offset);
			break;
		case 0x0f:
		case 0x10:
		case 0x11:
		case 0x12:
		case 0x13:
//			logerror("CPU #%d PC %04x: warning - write %02x to unsupported internal register %02x\n",cpu_getactivecpu(),activecpu_get_pc(),data,offset);
			break;
		case 0x14:
//			logerror("CPU #%d PC %04x: write %02x to RAM control register\n",cpu_getactivecpu(),activecpu_get_pc(),data);
			m6800.ram_ctrl = data;
			break;
		case 0x15:
		case 0x16:
		case 0x17:
		case 0x18:
		case 0x19:
		case 0x1a:
		case 0x1b:
		case 0x1c:
		case 0x1d:
		case 0x1e:
		case 0x1f:
		default:
//			logerror("CPU #%d PC %04x: warning - write %02x to reserved internal register %02x\n",cpu_getactivecpu(),activecpu_get_pc(),data,offset);
			break;
	}
}
#endif

#if 0
/**************************************************************************
 * Generic set_info
 **************************************************************************/

static void m6800_set_info(UINT32 state, cpuinfo *info)
{
	switch (state)
	{
		/* --- the following bits of info are set as 64-bit signed integers --- */
		case CPUINFO_INT_INPUT_STATE + M6800_IRQ_LINE:	set_irq_line(M6800_IRQ_LINE, info->i);	break;
		case CPUINFO_INT_INPUT_STATE + M6800_TIN_LINE:	set_irq_line(M6800_TIN_LINE, info->i);	break;
		case CPUINFO_INT_INPUT_STATE + M6800_INPUT_LINE_NMI:	set_irq_line(M6800_INPUT_LINE_NMI, info->i);	break;

		case CPUINFO_INT_PC:							PC = info->i; CHANGE_PC();				break;
		case CPUINFO_INT_REGISTER + M6800_PC:			m6800.pc.w.l = info->i;					break;
		case CPUINFO_INT_SP:							S = info->i;							break;
		case CPUINFO_INT_REGISTER + M6800_S:			m6800.s.w.l = info->i;					break;
		case CPUINFO_INT_REGISTER + M6800_CC:			m6800.cc = info->i;						break;
		case CPUINFO_INT_REGISTER + M6800_A:			m6800.d.b.h = info->i;					break;
		case CPUINFO_INT_REGISTER + M6800_B:			m6800.d.b.l = info->i;					break;
		case CPUINFO_INT_REGISTER + M6800_X:			m6800.x.w.l = info->i;					break;
	}
}



/**************************************************************************
 * Generic get_info
 **************************************************************************/

void m6800_get_info(UINT32 state, cpuinfo *info)
{
	switch (state)
	{
		/* --- the following bits of info are returned as 64-bit signed integers --- */
		case CPUINFO_INT_CONTEXT_SIZE:					info->i = sizeof(m6800);				break;
		case CPUINFO_INT_INPUT_LINES:					info->i = 2;							break;
		case CPUINFO_INT_DEFAULT_IRQ_VECTOR:			info->i = 0;							break;
		case CPUINFO_INT_ENDIANNESS:					info->i = CPU_IS_BE;					break;
		case CPUINFO_INT_CLOCK_DIVIDER:					info->i = 1;							break;
		case CPUINFO_INT_MIN_INSTRUCTION_BYTES:			info->i = 1;							break;
		case CPUINFO_INT_MAX_INSTRUCTION_BYTES:			info->i = 4;							break;
		case CPUINFO_INT_MIN_CYCLES:					info->i = 1;							break;
		case CPUINFO_INT_MAX_CYCLES:					info->i = 12;							break;

		case CPUINFO_INT_DATABUS_WIDTH + ADDRESS_SPACE_PROGRAM:	info->i = 8;					break;
		case CPUINFO_INT_ADDRBUS_WIDTH + ADDRESS_SPACE_PROGRAM: info->i = 16;					break;
		case CPUINFO_INT_ADDRBUS_SHIFT + ADDRESS_SPACE_PROGRAM: info->i = 0;					break;
		case CPUINFO_INT_DATABUS_WIDTH + ADDRESS_SPACE_DATA:	info->i = 0;					break;
		case CPUINFO_INT_ADDRBUS_WIDTH + ADDRESS_SPACE_DATA: 	info->i = 0;					break;
		case CPUINFO_INT_ADDRBUS_SHIFT + ADDRESS_SPACE_DATA: 	info->i = 0;					break;
		case CPUINFO_INT_DATABUS_WIDTH + ADDRESS_SPACE_IO:		info->i = 0;					break;
		case CPUINFO_INT_ADDRBUS_WIDTH + ADDRESS_SPACE_IO: 		info->i = 0;					break;
		case CPUINFO_INT_ADDRBUS_SHIFT + ADDRESS_SPACE_IO: 		info->i = 0;					break;

		case CPUINFO_INT_INPUT_STATE + M6800_IRQ_LINE:	info->i = m6800.irq_state[M6800_IRQ_LINE]; break;
		case CPUINFO_INT_INPUT_STATE + M6800_TIN_LINE:	info->i = m6800.irq_state[M6800_TIN_LINE]; break;
		case CPUINFO_INT_INPUT_STATE + M6800_INPUT_LINE_NMI:	info->i = m6800.nmi_state;				break;

		case CPUINFO_INT_PREVIOUSPC:					info->i = m6800.ppc.w.l;				break;

		case CPUINFO_INT_PC:							info->i = PC;							break;
		case CPUINFO_INT_REGISTER + M6800_PC:			info->i = m6800.pc.w.l;					break;
		case CPUINFO_INT_SP:							info->i = S;							break;
		case CPUINFO_INT_REGISTER + M6800_S:			info->i = m6800.s.w.l;					break;
		case CPUINFO_INT_REGISTER + M6800_CC:			info->i = m6800.cc;						break;
		case CPUINFO_INT_REGISTER + M6800_A:			info->i = m6800.d.b.h;					break;
		case CPUINFO_INT_REGISTER + M6800_B:			info->i = m6800.d.b.l;					break;
		case CPUINFO_INT_REGISTER + M6800_X:			info->i = m6800.x.w.l;					break;
		case CPUINFO_INT_REGISTER + M6800_WAI_STATE:	info->i = m6800.wai_state;				break;

		/* --- the following bits of info are returned as pointers to data or functions --- */
		case CPUINFO_PTR_SET_INFO:						info->setinfo = m6800_set_info;			break;
		case CPUINFO_PTR_GET_CONTEXT:					info->getcontext = m6800_get_context;	break;
		case CPUINFO_PTR_SET_CONTEXT:					info->setcontext = m6800_set_context;	break;
		case CPUINFO_PTR_INIT:							info->init = m6800_init;				break;
		case CPUINFO_PTR_RESET:							info->reset = m6800_reset;				break;
		case CPUINFO_PTR_EXIT:							info->exit = m6800_exit;				break;
		case CPUINFO_PTR_EXECUTE:						info->execute = m6800_execute;			break;
		case CPUINFO_PTR_BURN:							info->burn = NULL;						break;
#ifdef MAME_DEBUG
		case CPUINFO_PTR_DISASSEMBLE:					info->disassemble = m6800_dasm;			break;
#endif /* MAME_DEBUG */
		case CPUINFO_PTR_INSTRUCTION_COUNTER:			info->icount = &m6800_ICount;			break;

		/* --- the following bits of info are returned as NULL-terminated strings --- */
		case CPUINFO_STR_NAME:							strcpy(info->s, "M6800");				break;
		case CPUINFO_STR_CORE_FAMILY:					strcpy(info->s, "Motorola 6800");		break;
		case CPUINFO_STR_CORE_VERSION:					strcpy(info->s, "1.1");					break;
		case CPUINFO_STR_CORE_FILE:						strcpy(info->s, __FILE__);				break;
		case CPUINFO_STR_CORE_CREDITS:					strcpy(info->s, "The MAME team.");		break;

		case CPUINFO_STR_FLAGS:
			sprintf(info->s, "%c%c%c%c%c%c%c%c",
				m6800.cc & 0x80 ? '?':'.',
				m6800.cc & 0x40 ? '?':'.',
				m6800.cc & 0x20 ? 'H':'.',
				m6800.cc & 0x10 ? 'I':'.',
				m6800.cc & 0x08 ? 'N':'.',
				m6800.cc & 0x04 ? 'Z':'.',
				m6800.cc & 0x02 ? 'V':'.',
				m6800.cc & 0x01 ? 'C':'.');
			break;

		case CPUINFO_STR_REGISTER + M6800_A:			sprintf(info->s, "A:%02X", m6800.d.b.h); break;
		case CPUINFO_STR_REGISTER + M6800_B:			sprintf(info->s, "B:%02X", m6800.d.b.l); break;
		case CPUINFO_STR_REGISTER + M6800_PC:			sprintf(info->s, "PC:%04X", m6800.pc.w.l); break;
		case CPUINFO_STR_REGISTER + M6800_S:			sprintf(info->s, "S:%04X", m6800.s.w.l); break;
		case CPUINFO_STR_REGISTER + M6800_X:			sprintf(info->s, "X:%04X", m6800.x.w.l); break;
		case CPUINFO_STR_REGISTER + M6800_CC:			sprintf(info->s, "CC:%02X", m6800.cc); break;
		case CPUINFO_STR_REGISTER + M6800_WAI_STATE:	sprintf(info->s, "WAI:%X", m6800.wai_state); break;
	}
}


#if (HAS_M6801)
/**************************************************************************
 * CPU-specific set_info
 **************************************************************************/

void m6801_get_info(UINT32 state, cpuinfo *info)
{
	switch (state)
	{
		/* --- the following bits of info are returned as 64-bit signed integers --- */
		case CPUINFO_INT_DATABUS_WIDTH + ADDRESS_SPACE_IO:		info->i = 8;					break;
		case CPUINFO_INT_ADDRBUS_WIDTH + ADDRESS_SPACE_IO: 		info->i = 9;					break;

		/* --- the following bits of info are returned as pointers to data or functions --- */
		case CPUINFO_PTR_INIT:							info->init = m6801_init;				break;
		case CPUINFO_PTR_EXECUTE:						info->execute = m6803_execute;			break;
#ifdef MAME_DEBUG
		case CPUINFO_PTR_DISASSEMBLE:					info->disassemble = m6801_dasm;			break;
#endif /* MAME_DEBUG */

		/* --- the following bits of info are returned as NULL-terminated strings --- */
		case CPUINFO_STR_NAME:							strcpy(info->s, "M6801");				break;

		default:										m6800_get_info(state, info);			break;
	}
}
#endif


#if (HAS_M6802)
/**************************************************************************
 * CPU-specific set_info
 **************************************************************************/

void m6802_get_info(UINT32 state, cpuinfo *info)
{
	switch (state)
	{
		/* --- the following bits of info are returned as pointers to data or functions --- */
		case CPUINFO_PTR_INIT:							info->init = m6802_init;				break;
#ifdef MAME_DEBUG
		case CPUINFO_PTR_DISASSEMBLE:					info->disassemble = m6802_dasm;			break;
#endif /* MAME_DEBUG */

		/* --- the following bits of info are returned as NULL-terminated strings --- */
		case CPUINFO_STR_NAME:							strcpy(info->s, "M6802");				break;

		default:										m6800_get_info(state, info);			break;
	}
}
#endif


#if (HAS_M6803)
/**************************************************************************
 * CPU-specific set_info
 **************************************************************************/

void m6803_get_info(UINT32 state, cpuinfo *info)
{
	switch (state)
	{
		/* --- the following bits of info are returned as 64-bit signed integers --- */
		case CPUINFO_INT_DATABUS_WIDTH + ADDRESS_SPACE_IO:		info->i = 8;					break;
		case CPUINFO_INT_ADDRBUS_WIDTH + ADDRESS_SPACE_IO: 		info->i = 9;					break;

		/* --- the following bits of info are returned as pointers to data or functions --- */
		case CPUINFO_PTR_INIT:							info->init = m6803_init;				break;
		case CPUINFO_PTR_EXECUTE:						info->execute = m6803_execute;			break;
#ifdef MAME_DEBUG
		case CPUINFO_PTR_DISASSEMBLE:					info->disassemble = m6803_dasm;			break;
#endif /* MAME_DEBUG */

		case CPUINFO_PTR_INTERNAL_MEMORY_MAP + ADDRESS_SPACE_PROGRAM: info->internal_map = construct_map_m6803_mem; break;

		/* --- the following bits of info are returned as NULL-terminated strings --- */
		case CPUINFO_STR_NAME:							strcpy(info->s, "M6803");				break;

		default:										m6800_get_info(state, info);			break;
	}
}
#endif


#if (HAS_M6808)
/**************************************************************************
 * CPU-specific set_info
 **************************************************************************/

void m6808_get_info(UINT32 state, cpuinfo *info)
{
	switch (state)
	{
		/* --- the following bits of info are returned as pointers to data or functions --- */
		case CPUINFO_PTR_INIT:							info->init = m6808_init;				break;
#ifdef MAME_DEBUG
		case CPUINFO_PTR_DISASSEMBLE:					info->disassemble = m6808_dasm;			break;
#endif /* MAME_DEBUG */

		/* --- the following bits of info are returned as NULL-terminated strings --- */
		case CPUINFO_STR_NAME:							strcpy(info->s, "M6808");				break;

		default:										m6800_get_info(state, info);			break;
	}
}
#endif


#if (HAS_HD63701)
/**************************************************************************
 * CPU-specific set_info
 **************************************************************************/

void hd63701_get_info(UINT32 state, cpuinfo *info)
{
	switch (state)
	{
		/* --- the following bits of info are returned as 64-bit signed integers --- */
		case CPUINFO_INT_DATABUS_WIDTH + ADDRESS_SPACE_IO:		info->i = 8;					break;
		case CPUINFO_INT_ADDRBUS_WIDTH + ADDRESS_SPACE_IO: 		info->i = 9;					break;

		/* --- the following bits of info are returned as pointers to data or functions --- */
		case CPUINFO_PTR_INIT:							info->init = hd63701_init;				break;
		case CPUINFO_PTR_EXECUTE:						info->execute = hd63701_execute;		break;
#ifdef MAME_DEBUG
		case CPUINFO_PTR_DISASSEMBLE:					info->disassemble = hd63701_dasm;		break;
#endif /* MAME_DEBUG */

		/* --- the following bits of info are returned as NULL-terminated strings --- */
		case CPUINFO_STR_NAME:							strcpy(info->s, "HD63701");				break;

		default:										m6800_get_info(state, info);			break;
	}
}
#endif


#if (HAS_NSC8105)
/**************************************************************************
 * CPU-specific set_info
 **************************************************************************/

void nsc8105_get_info(UINT32 state, cpuinfo *info)
{
	switch (state)
	{
		/* --- the following bits of info are returned as pointers to data or functions --- */
		case CPUINFO_PTR_INIT:							info->init = nsc8105_init;				break;
		case CPUINFO_PTR_EXECUTE:						info->execute = nsc8105_execute;		break;
#ifdef MAME_DEBUG
		case CPUINFO_PTR_DISASSEMBLE:					info->disassemble = nsc8105_dasm;		break;
#endif /* MAME_DEBUG */

		/* --- the following bits of info are returned as NULL-terminated strings --- */
		case CPUINFO_STR_NAME:							strcpy(info->s, "NSC8105");				break;

		default:										m6800_get_info(state, info);			break;
	}
}
#endif

#endif
