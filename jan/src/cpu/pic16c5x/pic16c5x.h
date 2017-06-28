 /**************************************************************************\
 *                      Microchip PIC16C5x Emulator                         *
 *                                                                          *
 *                    Copyright Tony La Porta                               *
 *                 Originally written for the MAME project.                 *
 *                                                                          *
 *                                                                          *
 *      Addressing architecture is based on the Harvard addressing scheme.  *
 *                                                                          *
 \**************************************************************************/


#ifndef __PIC16C5X_H__
#define __PIC16C5X_H__

#include "pic16c5x_intf.h"


/**************************************************************************
 *  Internal Clock divisor
 *
 *  External Clock is divided internally by 4 for the instruction cycle
 *  times. (Each instruction cycle passes through 4 machine states). This
 *  is handled by the cpu execution engine.
 */

enum
{
	PIC16C5x_PC=1, PIC16C5x_STK0, PIC16C5x_STK1, PIC16C5x_FSR,
	PIC16C5x_W,    PIC16C5x_ALU,  PIC16C5x_STR,  PIC16C5x_OPT,
	PIC16C5x_TMR0, PIC16C5x_PRTA, PIC16C5x_PRTB, PIC16C5x_PRTC,
	PIC16C5x_WDT,  PIC16C5x_TRSA, PIC16C5x_TRSB, PIC16C5x_TRSC,
	PIC16C5x_PSCL
};




/****************************************************************************
 *  Function to configure the CONFIG register. This is actually hard-wired
 *  during ROM programming, so should be called in the driver INIT, with
 *  the value if known (available in HEX dumps of the ROM).
 */

void pic16c5x_config(int data);


/****************************************************************************
 *  Read the state of the T0 Clock input signal
 */

#define PIC16C5x_T0		0x10
#define PIC16C5x_T0_In (pic16c5xReadPort((PIC16C5x_T0)))



/****************************************************************************
 *  Input a word from given I/O port
 */

#define PIC16C5x_In(Port) (pic16c5xReadPort((Port)))



/****************************************************************************
 *  Output a word to given I/O port
 */

#define PIC16C5x_Out(Port,Value) (pic16c5xWritePort(Port, Value))



/****************************************************************************
 *  Read a word from given RAM memory location
 */

#define PIC16C5x_RAM_RDMEM(A) (pic16c5xRead(A))



/****************************************************************************
 *  Write a word to given RAM memory location
 */

#define PIC16C5x_RAM_WRMEM(A,V) (pic16c5xWrite(A, V))



/****************************************************************************
 *  PIC16C5X_RDOP() is identical to PIC16C5X_RDMEM() except it is used for
 *  reading opcodes. In case of system with memory mapped I/O, this function
 *  can be used to greatly speed up emulation
 */

#define PIC16C5x_RDOP(A) pic16c5xFetch(A)


/****************************************************************************
 *  PIC16C5X_RDOP_ARG() is identical to PIC16C5X_RDOP() except it is used
 *  for reading opcode arguments. This difference can be used to support systems
 *  that use different encoding mechanisms for opcodes and opcode arguments
 */

#define PIC16C5x_RDOP_ARG(A) (pic16c5xFetch(A))



#endif	/* __PIC16C5X_H__ */
