 /**************************************************************************\
 *                      Microchip PIC16C5x Emulator                         *
 *                                                                          *
 *                    Copyright Tony La Porta                               *
 *                 Originally written for the MAME project.                 *
 *                                                                          *
 *                                                                          *
 *      Addressing architecture is based on the Harvard addressing scheme.  *
 *                                                                          *
 *                                                                          *
 *  **** Change Log ****                                                    *
 *  TLP (06-Apr-2003)                                                       *
 *   - First Public release.                                                *
 *  BO  (07-Apr-2003) Ver 1.01                                              *
 *   - Renamed 'sleep' function to 'sleepic' to avoid C conflicts.          *
 *  TLP (09-Apr-2003) Ver 1.10                                              *
 *   - Fixed modification of file register $03 (Status).                    *
 *   - Corrected support for 7FFh (12-bit) size ROMs.                       *
 *   - The 'call' and 'goto' instructions weren't correctly handling the    *
 *     STATUS page info correctly.                                          *
 *   - The FSR register was incorrectly oring the data with 0xe0 when read. *
 *   - Prescaler masking information was set to 3 instead of 7.             *
 *   - Prescaler assign bit was set to 4 instead of 8.                      *
 *   - Timer source and edge select flags/masks were wrong.                 *
 *   - Corrected the memory bank selection in GET/SET_REGFILE and also the  *
 *     indirect register addressing.                                        *
 *  BMP (18-May-2003) Ver 1.11                                              *
 *   - pic16c5x_get_reg functions were missing 'returns'.                   *
 *  TLP (27-May-2003) Ver 1.12                                              *
 *   - Fixed the WatchDog timer count.                                      *
 *   - The Prescaler rate was incorrectly being zeroed, instead of the      *
 *     actual Prescaler counter in the CLRWDT and SLEEP instructions.       *
 *   - Added masking to the FSR register. Upper unused bits are always 1.   *
 *  TLP (27-Aug-2009) Ver 1.13                                              *
 *   - Indirect addressing was not taking into account special purpose      *
 *     memory mapped locations.                                             *
 *   - 'iorlw' instruction was saving the result to memory instead of       *
 *     the W register.                                                      *
 *   - 'tris' instruction no longer modifies Port-C on PIC models that      *
 *     do not have Port-C implemented.                                      *
 *  TLP (07-Sep-2009) Ver 1.14                                              *
 *   - Edge sense control for the T0 count input was incorrectly reversed   *
 *                                                                          *
 *                                                                          *
 *  **** Notes: ****                                                        *
 *  PIC WatchDog Timer has a seperate internal clock. For the moment, we're *
 *     basing the count on a 4MHz input clock, since 4MHz is the typical    *
 *     input frequency (but by no means always).                            *
 *  A single scaler is available for the Counter/Timer or WatchDog Timer.   *
 *     When connected to the Counter/Timer, it functions as a Prescaler,    *
 *     hence prescale overflows, tick the Counter/Timer.                    *
 *     When connected to the WatchDog Timer, it functions as a Postscaler   *
 *     hence WatchDog Timer overflows, tick the Postscaler. This scenario   *
 *     means that the WatchDog timeout occurs when the Postscaler has       *
 *     reached the scaler rate value, not when the WatchDog reaches zero.   *
 *  CLRWDT should prevent the WatchDog Timer from timing out and generating *
 *     a device reset, but how is not known. The manual also mentions that  *
 *     the WatchDog Timer can only be disabled during ROM programming, and  *
 *     no other means seem to exist???                                      *
 *                                                                          *
 \**************************************************************************/



//#include "debugger.h"
#include "burnint.h"
#include "pic16c5x.h"

#define CLK 1	/* 1 cycle equals 4 Q-clock ticks */


#ifndef INLINE
#define INLINE static inline
#endif


#define M_RDRAM(A)		(((A) < 8) ? R.internalram[A] : PIC16C5x_RAM_RDMEM(A))
#define M_WRTRAM(A,V)		do { if ((A) < 8) R.internalram[A] = (V); else PIC16C5x_RAM_WRMEM(A,V); } while (0)
#define M_RDOP(A)		PIC16C5x_RDOP(A)
#define M_RDOP_ARG(A)		PIC16C5x_RDOP_ARG(A)
#define P_IN(A)			PIC16C5x_In(A)
#define P_OUT(A,V)		PIC16C5x_Out(A,V)
#define S_T0_IN			PIC16C5x_T0_In
#define ADDR_MASK		0x7ff

#define offs_t	unsigned int


typedef struct
{
	/******************** CPU Internal Registers *******************/
	UINT16	PC;
	UINT16	PREVPC;		/* previous program counter */
	UINT8	W;
	UINT8	OPTION;
	UINT16	CONFIG;
	UINT8	ALU;
	UINT16	WDT;
	UINT8	TRISA;
	UINT8	TRISB;
	UINT8	TRISC;
	UINT16	STACK[2];
	UINT16	prescaler;	/* Note: this is really an 8-bit register */
	PAIR	opcode;
	UINT8	internalram[8];
} pic16C5x_Regs;

static pic16C5x_Regs R;
static UINT16 temp_config;
static UINT8  old_T0;
static INT8   old_data;
static UINT8  picRAMmask;
static int    inst_cycles;
static int    delay_timer;
static int    picmodel;
static int    pic16C5x_reset_vector;
static int    pic16C5x_icount;
typedef void (*opcode_fn) (void);

static const unsigned cycles_000_other[16]=
{
/*00*/	1*CLK, 0*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 0*CLK, 0*CLK, 0*CLK, 0*CLK, 0*CLK, 0*CLK, 0*CLK, 0*CLK
};

#define TMR0	internalram[1]
#define PCL		internalram[2]
#define STATUS	internalram[3]
#define FSR		internalram[4]
#define PORTA	internalram[5]
#define PORTB	internalram[6]
#define PORTC	internalram[7]
#define INDF	M_RDRAM(R.FSR)

#define ADDR	(R.opcode.b.l & 0x1f)

#define  RISING_EDGE_T0  (( (int)(T0_in-old_T0) > 0) ? 1 : 0)
#define FALLING_EDGE_T0  (( (int)(T0_in-old_T0) < 0) ? 1 : 0)


/********  The following is the Status Flag register definition.  *********/
			/* | 7 | 6 | 5 |  4 |  3 | 2 | 1  | 0 | */
			/* |    PA     | TO | PD | Z | DC | C | */
#define PA_REG		0xe0	/* PA   Program Page Preselect - bit 8 is unused here */
#define TO_FLAG		0x10	/* TO   Time Out flag (WatchDog) */
#define PD_FLAG		0x08	/* PD   Power Down flag */
#define Z_FLAG		0x04	/* Z    Zero Flag */
#define DC_FLAG		0x02	/* DC   Digit Carry/Borrow flag (Nibble) */
#define C_FLAG		0x01	/* C    Carry/Borrow Flag (Byte) */

#define PA		(R.STATUS & PA_REG)
#define TO		(R.STATUS & TO_FLAG)
#define PD		(R.STATUS & PD_FLAG)
#define ZERO	(R.STATUS & Z_FLAG)
#define DC		(R.STATUS & DC_FLAG)
#define CARRY	(R.STATUS & C_FLAG)


/********  The following is the Option Flag register definition.  *********/
			/* | 7 | 6 |   5  |   4  |  3  | 2 | 1 | 0 | */
			/* | 0 | 0 | TOCS | TOSE | PSA |    PS     | */
#define T0CS_FLAG	0x20	/* TOCS     Timer 0 clock source select */
#define T0SE_FLAG	0x10	/* TOSE     Timer 0 clock source edge select */
#define PSA_FLAG	0x08	/* PSA      Prescaler Assignment bit */
#define PS_REG		0x07	/* PS       Prescaler Rate select */

#define T0CS	(R.OPTION & T0CS_FLAG)
#define T0SE	(R.OPTION & T0SE_FLAG)
#define PSA		(R.OPTION & PSA_FLAG)
#define PS		(R.OPTION & PS_REG)


/********  The following is the Config Flag register definition.  *********/
	/* | 11 | 10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 |   2  | 1 | 0 | */
	/* |              CP                     | WDTE |  FOSC | */
							/* CP       Code Protect (ROM read protect) */
#define WDTE_FLAG	0x04	/* WDTE     WatchDog Timer enable */
#define FOSC_FLAG	0x03	/* FOSC     Oscillator source select */

#define WDTE	(R.CONFIG & WDTE_FLAG)
#define FOSC	(R.CONFIG & FOSC_FLAG)


/************************************************************************
 *  Shortcuts
 ************************************************************************/

/* Easy bit position selectors */
#define POS	 ((R.opcode.b.l >> 5) & 7)
static const unsigned int bit_clr[8] = { 0xfe, 0xfd, 0xfb, 0xf7, 0xef, 0xdf, 0xbf, 0x7f };
static const unsigned int bit_set[8] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };


INLINE void CLR(UINT16 flag) { R.STATUS &= (UINT8)(~flag); }
INLINE void SET(UINT16 flag) { R.STATUS |=  flag; }



INLINE void CALCULATE_Z_FLAG(void)
{
	if (R.ALU == 0) SET(Z_FLAG);
	else CLR(Z_FLAG);
}

INLINE void CALCULATE_ADD_CARRY(void)
{
	if ((UINT8)(old_data) > (UINT8)(R.ALU)) {
		SET(C_FLAG);
	}
	else {
		CLR(C_FLAG);
	}
}

INLINE void CALCULATE_ADD_DIGITCARRY(void)
{
	if (((UINT8)(old_data) & 0x0f) > ((UINT8)(R.ALU) & 0x0f)) {
		SET(DC_FLAG);
	}
	else {
		CLR(DC_FLAG);
	}
}

INLINE void CALCULATE_SUB_CARRY(void)
{
	if ((UINT8)(old_data) < (UINT8)(R.ALU)) {
		CLR(C_FLAG);
	}
	else {
		SET(C_FLAG);
	}
}

INLINE void CALCULATE_SUB_DIGITCARRY(void)
{
	if (((UINT8)(old_data) & 0x0f) < ((UINT8)(R.ALU) & 0x0f)) {
		CLR(DC_FLAG);
	}
	else {
		SET(DC_FLAG);
	}
}



INLINE UINT16 POP_STACK(void)
{
	UINT16 data = R.STACK[1];
	R.STACK[1] = R.STACK[0];
	return (data & ADDR_MASK);
}
INLINE void PUSH_STACK(UINT16 data)
{
	R.STACK[0] = R.STACK[1];
	R.STACK[1] = (data & ADDR_MASK);
}


//INLINE 
UINT8 GET_REGFILE(offs_t addr)	/* Read from internal memory */
{
	UINT8 data;
	
	if (addr == 0) { /* Indirect addressing */
		addr = (R.FSR & picRAMmask);
	}

	if ((picmodel == 0x16C57) || (picmodel == 0x16C58))
	{
		addr |= (R.FSR & 0x60);		/* FSR bits 6-5 are used for banking in direct mode */
	}
	if ((addr & 0x10) == 0) addr &= 0x0f;

	switch(addr)
	{
		case 00:	/* Not an actual register, so return 0 */
					data = 0;
					break;
		case 04:	data = (R.FSR | (UINT8)(~picRAMmask));
					break;
		case 05:	data = P_IN(0);
					data &= R.TRISA;
					data |= ((UINT8)(~R.TRISA) & R.PORTA);
					data &= 0xf;		/* 4-bit port (only lower 4 bits used) */
					break;
		case 06:	data = P_IN(1);
					data &= R.TRISB;
					data |= ((UINT8)(~R.TRISB) & R.PORTB);
					break;
		case 07:	if ((picmodel == 0x16C55) || (picmodel == 0x16C57)) {
						data = P_IN(2);
						data &= R.TRISC;
						data |= ((UINT8)(~R.TRISC) & R.PORTC);
					}
					else {		/* PIC16C54, PIC16C56, PIC16C58 */
						data = M_RDRAM(addr);
					}
					break;
		default:	data = M_RDRAM(addr);
					break;
	}

	return data;
}

//INLINE 
void STORE_REGFILE(offs_t addr, UINT8 data)	/* Write to internal memory */
{
	if (addr == 0) { /* Indirect addressing */
		addr = (R.FSR & picRAMmask);
	}
	
	if ((picmodel == 0x16C57) || (picmodel == 0x16C58))
	{
		addr |= (R.FSR & 0x60);			/* FSR bits 6-5 are used for banking in direct mode */
	}
	if ((addr & 0x10) == 0) addr &= 0x0f;

	switch(addr)
	{
		case 00:	/* Not an actual register, nothing to save */
					break;
		case 01:	delay_timer = 2;		/* Timer starts after next two instructions */
					if (PSA == 0) R.prescaler = 0;	/* Must clear the Prescaler */
					R.TMR0 = data;
					break;
		case 02:	R.PCL = data;
					R.PC = ((R.STATUS & PA_REG) << 4) | data;
					break;
		case 03:	R.STATUS &= (UINT8)(~PA_REG); R.STATUS |= (data & PA_REG);
					break;
		case 04:	R.FSR = (data | (UINT8)(~picRAMmask));
					break;
		case 05:	data &= 0xf;		/* 4-bit port (only lower 4 bits used) */
					P_OUT(0,data & (UINT8)(~R.TRISA)); R.PORTA = data;
					break;
		case 06:	P_OUT(1,data & (UINT8)(~R.TRISB)); R.PORTB = data;
					break;
		case 07:	if ((picmodel == 0x16C55) || (picmodel == 0x16C57)) {
						P_OUT(2,data & (UINT8)(~R.TRISC));
						R.PORTC = data;
					}
					else {		/* PIC16C54, PIC16C56, PIC16C58 */
						M_WRTRAM(addr, data);
					}
					break;
		default:	M_WRTRAM(addr, data);
					break;
	}
}


INLINE void STORE_RESULT(offs_t addr, UINT8 data)
{
	if (R.opcode.b.l & 0x20)
	{
		STORE_REGFILE(addr, data);
	}
	else
	{
		R.W = data;
	}
}


/************************************************************************
 *  Emulate the Instructions
 ************************************************************************/

/* This following function is here to fill in the void for */
/* the opcode call function. This function is never called. */


static void illegal(void)
{

}


static void addwf(void)
{
	old_data = GET_REGFILE(ADDR);
	R.ALU = old_data + R.W;
	STORE_RESULT(ADDR, R.ALU);
	CALCULATE_Z_FLAG();
	CALCULATE_ADD_CARRY();
	CALCULATE_ADD_DIGITCARRY();
}

static void andwf(void)
{
	R.ALU = GET_REGFILE(ADDR) & R.W;
	STORE_RESULT(ADDR, R.ALU);
	CALCULATE_Z_FLAG();
}

static void andlw(void)
{
	R.ALU = R.opcode.b.l & R.W;
	R.W = R.ALU;
	CALCULATE_Z_FLAG();
}

static void bcf(void)
{
	R.ALU = GET_REGFILE(ADDR);
	R.ALU &= bit_clr[POS];
	STORE_REGFILE(ADDR, R.ALU);
}

static void bsf(void)
{
	R.ALU = GET_REGFILE(ADDR);
	R.ALU |= bit_set[POS];
	STORE_REGFILE(ADDR, R.ALU);
}

static void btfss(void)
{
	if ((GET_REGFILE(ADDR) & bit_set[POS]) == bit_set[POS])
	{
		R.PC++ ;
		R.PCL = R.PC & 0xff;
		inst_cycles += 1;		/* Add NOP cycles */
	}
}

static void btfsc(void)
{
	if ((GET_REGFILE(ADDR) & bit_set[POS]) == 0)
	{
		R.PC++ ;
		R.PCL = R.PC & 0xff;
		inst_cycles += 1;		/* Add NOP cycles */
	}
}

static void call(void)
{
	PUSH_STACK(R.PC);
	R.PC = ((R.STATUS & PA_REG) << 4) | R.opcode.b.l;
	R.PC &= 0x6ff;
	R.PCL = R.PC & 0xff;
}

static void clrw(void)
{
	R.W = 0;
	SET(Z_FLAG);
}

static void clrf(void)
{
	STORE_REGFILE(ADDR, 0);
	SET(Z_FLAG);
}

static void clrwdt(void)
{
	R.WDT = 0;
	if (PSA) R.prescaler = 0;
	SET(TO_FLAG);
	SET(PD_FLAG);
}

static void comf(void)
{
	R.ALU = (UINT8)(~(GET_REGFILE(ADDR)));
	STORE_RESULT(ADDR, R.ALU);
	CALCULATE_Z_FLAG();
}

static void decf(void)
{
	R.ALU = GET_REGFILE(ADDR) - 1;
	STORE_RESULT(ADDR, R.ALU);
	CALCULATE_Z_FLAG();
}

static void decfsz(void)
{
	R.ALU = GET_REGFILE(ADDR) - 1;
	STORE_RESULT(ADDR, R.ALU);
	if (R.ALU == 0)
	{
		R.PC++ ;
		R.PCL = R.PC & 0xff;
		inst_cycles += 1;		/* Add NOP cycles */
	}
}

static void goto_op(void)
{
	R.PC = ((R.STATUS & PA_REG) << 4) | (R.opcode.w.l & 0x1ff);
	R.PC &= ADDR_MASK;
	R.PCL = R.PC & 0xff;
}

static void incf(void)
{
	R.ALU = GET_REGFILE(ADDR) + 1;
	STORE_RESULT(ADDR, R.ALU);
	CALCULATE_Z_FLAG();
}

static void incfsz(void)
{
	R.ALU = GET_REGFILE(ADDR) + 1;
	STORE_RESULT(ADDR, R.ALU);
	if (R.ALU == 0)
	{
		R.PC++ ;
		R.PCL = R.PC & 0xff;
		inst_cycles += 1;		/* Add NOP cycles */
	}
}

static void iorlw(void)
{
	R.ALU = R.opcode.b.l | R.W;
	R.W = R.ALU;
	CALCULATE_Z_FLAG();
}

static void iorwf(void)
{
	R.ALU = GET_REGFILE(ADDR) | R.W;
	STORE_RESULT(ADDR, R.ALU);
	CALCULATE_Z_FLAG();
}

static void movf(void)
{
	R.ALU = GET_REGFILE(ADDR);
	STORE_RESULT(ADDR, R.ALU);
	CALCULATE_Z_FLAG();
}

static void movlw(void)
{
	R.W = R.opcode.b.l;
}

static void movwf(void)
{
	STORE_REGFILE(ADDR, R.W);
}

static void nop(void)
{
	/* Do nothing */
}

static void option(void)
{
	R.OPTION = R.W & (T0CS_FLAG | T0SE_FLAG | PSA_FLAG | PS_REG);
}

static void retlw(void)
{
	R.W = R.opcode.b.l;
	R.PC = POP_STACK();
	R.PCL = R.PC & 0xff;
}

static void rlf(void)
{
	R.ALU = GET_REGFILE(ADDR);
	R.ALU <<= 1;
	if (R.STATUS & C_FLAG) R.ALU |= 1;
	if (GET_REGFILE(ADDR) & 0x80) SET(C_FLAG);
	else CLR(C_FLAG);
	STORE_RESULT(ADDR, R.ALU);
}

static void rrf(void)
{
	R.ALU = GET_REGFILE(ADDR);
	R.ALU >>= 1;
	if (R.STATUS & C_FLAG) R.ALU |= 0x80;
	if (GET_REGFILE(ADDR) & 1) SET(C_FLAG);
	else CLR(C_FLAG);
	STORE_RESULT(ADDR, R.ALU);
}

static void sleepic(void)
{
	if (WDTE) R.WDT = 0;
	if (PSA) R.prescaler = 0;
	SET(TO_FLAG);
	CLR(PD_FLAG);
}

static void subwf(void)
{
	old_data = GET_REGFILE(ADDR);
	R.ALU = old_data - R.W;
	STORE_RESULT(ADDR, R.ALU);
	CALCULATE_Z_FLAG();
	CALCULATE_SUB_CARRY();
	CALCULATE_SUB_DIGITCARRY();
}

static void swapf(void)
{
	R.ALU  = ((GET_REGFILE(ADDR) << 4) & 0xf0);
	R.ALU |= ((GET_REGFILE(ADDR) >> 4) & 0x0f);
	STORE_RESULT(ADDR, R.ALU);
}

static void tris(void)
{
	switch(R.opcode.b.l & 0x7)
	{
		case 05:	if (R.TRISA == R.W) break;
					else R.TRISA = R.W | 0xf0; P_OUT(0,R.PORTA & (UINT8)(~R.TRISA) & 0xf); break;
		case 06:	if (R.TRISB == R.W) break;
					else R.TRISB = R.W; P_OUT(1,R.PORTB & (UINT8)(~R.TRISB)); break;
		case 07:	if ((picmodel == 0x16C55) || (picmodel == 0x16C57)) {
						if (R.TRISC == R.W) break;
						else R.TRISC = R.W; P_OUT(2,R.PORTC & (UINT8)(~R.TRISC)); break;
					} else {
						illegal(); break;
					}
		default:	illegal(); break;
	}
}

static void xorlw(void)
{
	R.ALU = R.W ^ R.opcode.b.l;
	R.W = R.ALU;
	CALCULATE_Z_FLAG();
}

static void xorwf(void)
{
	R.ALU = GET_REGFILE(ADDR) ^ R.W;
	STORE_RESULT(ADDR, R.ALU);
	CALCULATE_Z_FLAG();
}



/***********************************************************************
 *  Cycle Timings
 ***********************************************************************/

static const unsigned cycles_main[256]=
{
/*00*/	1*CLK, 0*CLK, 1*CLK, 1*CLK, 1*CLK, 0*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK,
/*10*/	1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK,
/*20*/	1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK,


/*30*/	1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK,
/*40*/	1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK,
/*50*/	1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK,
/*60*/	1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK,
/*70*/	1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK,
/*80*/	2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK,
/*90*/	2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK,
/*A0*/	2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK,
/*B0*/	2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK, 2*CLK,
/*C0*/	1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK,
/*D0*/	1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK,
/*E0*/	1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK,
/*F0*/	1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK
};

static const opcode_fn opcode_main[256]=
{
/*00*/  nop,	illegal,movwf,	movwf,	clrw,	illegal,clrf,	clrf,
/*08*/  subwf,	subwf,	subwf,	subwf,	decf,	decf,	decf,	decf,
/*10*/  iorwf,	iorwf,	iorwf,	iorwf,	andwf,	andwf,	andwf,	andwf,
/*18*/  xorwf,	xorwf,	xorwf,	xorwf,	addwf,	addwf,	addwf,	addwf,
/*20*/  movf,	movf,	movf,	movf,	comf,	comf,	comf,	comf,
/*28*/  incf,	incf,	incf,	incf,	decfsz,	decfsz,	decfsz,	decfsz,
/*30*/  rrf,	rrf,	rrf,	rrf,	rlf,	rlf,	rlf,	rlf,
/*38*/  swapf,	swapf,	swapf,	swapf,	incfsz,	incfsz,	incfsz,	incfsz,
/*40*/  bcf,	bcf,	bcf,	bcf,	bcf,	bcf,	bcf,	bcf,
/*48*/  bcf,	bcf,	bcf,	bcf,	bcf,	bcf,	bcf,	bcf,
/*50*/  bsf,	bsf,	bsf,	bsf,	bsf,	bsf,	bsf,	bsf,
/*58*/  bsf,	bsf,	bsf,	bsf,	bsf,	bsf,	bsf,	bsf,
/*60*/  btfsc,	btfsc,	btfsc,	btfsc,	btfsc,	btfsc,	btfsc,	btfsc,
/*68*/  btfsc,	btfsc,	btfsc,	btfsc,	btfsc,	btfsc,	btfsc,	btfsc,
/*70*/  btfss,	btfss,	btfss,	btfss,	btfss,	btfss,	btfss,	btfss,
/*78*/  btfss,	btfss,	btfss,	btfss,	btfss,	btfss,	btfss,	btfss,
/*80*/  retlw,	retlw,	retlw,	retlw,	retlw,	retlw,	retlw,	retlw,
/*88*/  retlw,	retlw,	retlw,	retlw,	retlw,	retlw,	retlw,	retlw,
/*90*/  call,	call,	call,	call,	call,	call,	call,	call,
/*98*/  call,	call,	call,	call,	call,	call,	call,	call,
/*A0*/  goto_op,goto_op,goto_op,goto_op,goto_op,goto_op,goto_op,goto_op,
/*A8*/  goto_op,goto_op,goto_op,goto_op,goto_op,goto_op,goto_op,goto_op,
/*B0*/  goto_op,goto_op,goto_op,goto_op,goto_op,goto_op,goto_op,goto_op,
/*B8*/  goto_op,goto_op,goto_op,goto_op,goto_op,goto_op,goto_op,goto_op,
/*C0*/  movlw,	movlw,	movlw,	movlw,	movlw,	movlw,	movlw,	movlw,
/*C8*/  movlw,	movlw,	movlw,	movlw,	movlw,	movlw,	movlw,	movlw,
/*D0*/  iorlw,	iorlw,	iorlw,	iorlw,	iorlw,	iorlw,	iorlw,	iorlw,
/*D8*/  iorlw,	iorlw,	iorlw,	iorlw,	iorlw,	iorlw,	iorlw,	iorlw,
/*E0*/  andlw,	andlw,	andlw,	andlw,	andlw,	andlw,	andlw,	andlw,
/*E8*/  andlw,	andlw,	andlw,	andlw,	andlw,	andlw,	andlw,	andlw,
/*F0*/  xorlw,	xorlw,	xorlw,	xorlw,	xorlw,	xorlw,	xorlw,	xorlw,
/*F8*/  xorlw,	xorlw,	xorlw,	xorlw,	xorlw,	xorlw,	xorlw,	xorlw
};


//static const unsigned cycles_000_other[16]=
//{
///*00*/	1*CLK, 0*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 1*CLK, 0*CLK, 0*CLK, 0*CLK, 0*CLK, 0*CLK, 0*CLK, 0*CLK, 0*CLK
//};

static const opcode_fn opcode_000_other[16]=
{
/*00*/  nop,	illegal,option,	sleepic,clrwdt,	tris,	tris,	tris,
/*08*/  illegal,illegal,illegal,illegal,illegal,illegal,illegal,illegal
};

/****************************************************************************
 *  Reset registers to their initial values
 ****************************************************************************/

static void pic16C5x_reset_regs(void)
{
	R.PC     = pic16C5x_reset_vector;
	R.CONFIG = temp_config;
	R.TRISA  = 0xff;
	R.TRISB  = 0xff;
	R.TRISC  = 0xff;
	R.OPTION = (T0CS_FLAG | T0SE_FLAG | PSA_FLAG | PS_REG);
	R.PCL    = 0xff;
	R.FSR   |= (UINT8)(~picRAMmask);
	R.PORTA &= 0x0f;
	R.prescaler = 0;
	delay_timer = 0;
	old_T0      = 0;
	inst_cycles = 0;
}

static void pic16C5x_soft_reset(void)
{
//	R.STATUS &= 0x1f;
	SET((TO_FLAG | PD_FLAG | Z_FLAG | DC_FLAG | C_FLAG));
	pic16C5x_reset_regs();
}

void pic16c5x_config(int data)
{
//	logerror("Writing %04x to the PIC16C5x config register\n",data);
	temp_config = (data & 0xfff);
}


/****************************************************************************
 *  Shut down CPU emulation
 ****************************************************************************/

void pic16C5x_exit (void)
{
	/* nothing to do */
}


/****************************************************************************
 *  WatchDog
 ****************************************************************************/

static void pic16C5x_update_watchdog(int counts)
{
	/* WatchDog is set up to count 18,000 (0x464f hex) ticks to provide */
	/* the timeout period of 0.018ms based on a 4MHz input clock. */
	/* Note: the 4MHz clock should be divided by the PIC16C5x_CLOCK_DIVIDER */
	/* which effectively makes the PIC run at 1MHz internally. */

	/* If the current instruction is CLRWDT or SLEEP, don't update the WDT */

	if ((R.opcode.w.l != 3) && (R.opcode.w.l != 4))
	{
		UINT16 old_WDT = R.WDT;

		R.WDT -= counts;

		if (R.WDT > 0x464f) {
			R.WDT = 0x464f - (0xffff - R.WDT);
		}

		if (((old_WDT != 0) && (old_WDT < R.WDT)) || (R.WDT == 0))
		{
			if (PSA) {
				R.prescaler++;
				if (R.prescaler >= (1 << PS)) {	/* Prescale values from 1 to 128 */
					R.prescaler = 0;
					CLR(TO_FLAG);
					pic16C5x_soft_reset();
				}
			}
			else {
				CLR(TO_FLAG);
				pic16C5x_soft_reset();
			}
		}
	}
}


/****************************************************************************
 *  Update Timer
 ****************************************************************************/

static void pic16C5x_update_timer(int counts)
{
	if (PSA == 0) {
		R.prescaler += counts;
		if (R.prescaler >= (2 << PS)) {	/* Prescale values from 2 to 256 */
			R.TMR0 += (R.prescaler / (2 << PS));
			R.prescaler %= (2 << PS);	/* Overflow prescaler */
		}
	}
	else {
		R.TMR0 += counts;
	}
}


/****************************************************************************
 *  Execute IPeriod. Return 0 if emulation should be stopped
 ****************************************************************************/

int pic16c5xRun(int cycles)
{
	UINT8 T0_in;
	pic16C5x_icount = cycles;
	
	do
	{
		if (PD == 0)						/* Sleep Mode */
		{
			inst_cycles = (1*CLK);

			if (WDTE) {
				pic16C5x_update_watchdog(1*CLK);
			}
		}
		else
		{
			R.PREVPC = R.PC;
			R.opcode.d = M_RDOP(R.PC);

			R.PC++;
			R.PCL++;

			if ((R.opcode.w.l & 0xff0) != 0x000)	{	/* Do all opcodes except the 00? ones */
				inst_cycles = cycles_main[((R.opcode.w.l >> 4) & 0xff)];
				(*(opcode_main[((R.opcode.w.l >> 4) & 0xff)]))();
			}
			else {	/* Opcode 0x00? has many opcodes in its minor nibble */
				inst_cycles = cycles_000_other[(R.opcode.b.l & 0x1f)];
				(*(opcode_000_other[(R.opcode.b.l & 0x1f)]))();
			}

			if (T0CS) {						/* Count mode */
				T0_in = S_T0_IN;
				if (T0_in) T0_in = 1;
				if (T0SE) {					/* Count falling edge T0 input */
					if (FALLING_EDGE_T0) {
						pic16C5x_update_timer(1);
					}
				}
				else {						/* Count rising edge T0 input */
					if (RISING_EDGE_T0) {
						pic16C5x_update_timer(1);
					}
				}
				old_T0 = T0_in;
			}
			else {							/* Timer mode */
				if (delay_timer) {
					delay_timer--;
				}
				else {
					pic16C5x_update_timer((inst_cycles/CLK));
				}
			}
			if (WDTE) {
				pic16C5x_update_watchdog((inst_cycles/CLK));
			}
		}

		pic16C5x_icount -= inst_cycles;

	} while (pic16C5x_icount>0);

	return (cycles - pic16C5x_icount);
}


static void pic16C54_reset(void)
{
	picmodel = 0x16C54;
	picRAMmask = 0x1f;
	pic16C5x_reset_vector = 0x1ff;
	pic16C5x_reset_regs();
	CLR(PA_REG);
	SET((TO_FLAG | PD_FLAG));
}

static void pic16C55_reset(void)
{
	picmodel = 0x16C55;
	picRAMmask = 0x1f;
	pic16C5x_reset_vector = 0x1ff;
	pic16C5x_reset_regs();
	CLR(PA_REG);
	SET((TO_FLAG | PD_FLAG));
}

static void pic16C56_reset(void)
{
	picmodel = 0x16C56;
	picRAMmask = 0x1f;
	pic16C5x_reset_vector = 0x3ff;
	pic16C5x_reset_regs();
	CLR(PA_REG);
	SET((TO_FLAG | PD_FLAG));
}

static void pic16C57_reset(void)
{
	picmodel = 0x16C57;
	picRAMmask = 0x7f;
	pic16C5x_reset_vector = 0x7ff;
	pic16C5x_reset_regs();
	CLR(PA_REG);
	SET((TO_FLAG | PD_FLAG));
}

static void pic16C58_reset(void)
{
	picmodel = 0x16C58;
	picRAMmask = 0x7f;
	pic16C5x_reset_vector = 0x7ff;
	pic16C5x_reset_regs();
	CLR(PA_REG);
	SET((TO_FLAG | PD_FLAG));
}


void pic16c5xDoReset(int type, int *romlen, int *ramlen)
{
	switch (type)
	{
		case 0x16C54:
			pic16C54_reset();
			*romlen = 0x1ff;
			*ramlen = 0x01f;
		return;

		case 0x16C55:
			pic16C55_reset();
			*romlen = 0x3ff; // correct?
			*ramlen = 0x01f;
		return;

		case 0x16C56:
			pic16C56_reset();
			*romlen = 0x3ff;
			*ramlen = 0x01f;
		return;

		case 0x16C57:
			pic16C57_reset();
			*romlen = 0x7ff;
			*ramlen = 0x07f;
		return;

		case 0x16C58:
			pic16C58_reset();
			*romlen = 0x7ff;
			*ramlen = 0x07f;
		return;
	}
}

void pic16c5xRunEnd()
{
	pic16C5x_icount = 0;
}

int pic16c5xScanCpu(int nAction,int */*pnMin*/)
{
	struct BurnArea ba;

	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(R.PC);
		SCAN_VAR(R.PREVPC);
		SCAN_VAR(R.W);
		SCAN_VAR(R.OPTION);
		SCAN_VAR(R.CONFIG);
		SCAN_VAR(R.ALU);
		SCAN_VAR(R.WDT);
		SCAN_VAR(R.TRISA);
		SCAN_VAR(R.TRISC);
		SCAN_VAR(R.STACK[0]);
		SCAN_VAR(R.STACK[1]);
		SCAN_VAR(R.prescaler);
		SCAN_VAR(R.opcode);
	}

	if (nAction & ACB_MEMORY_RAM) {
		ba.Data		= R.internalram;
		ba.nLen		= 8;
		ba.nAddress = 0;
		ba.szName	= "Internal RAM";
		BurnAcb(&ba);
	}

	return 0;
}
