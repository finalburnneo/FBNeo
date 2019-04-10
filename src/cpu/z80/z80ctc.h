/***************************************************************************

    Z80 CTC (Z8430) implementation

    Copyright Nicola Salmoria and the MAME Team.
    Visit http://mamedev.org for licensing and usage restrictions.

***************************************************************************/

#ifndef __Z80CTC_H__
#define __Z80CTC_H__


/***************************************************************************
    CONSTANTS
***************************************************************************/

#define NOTIMER_0 (1<<0)
#define NOTIMER_1 (1<<1)
#define NOTIMER_2 (1<<2)
#define NOTIMER_3 (1<<3)

// daisy chain stuff:
void  z80ctc_reset();
int   z80ctc_irq_state();
int   z80ctc_irq_ack();
void  z80ctc_irq_reti();
void  z80ctc_exit();
void  z80ctc_scan(INT32 nAction);

// driver stuff:
void  z80ctc_init(INT32 clock, INT32 notimer, void (*intr)(INT32), void (*zc0)(int, UINT8), void (*zc1)(int, UINT8), void (*zc2)(int, UINT8));
void  z80ctc_timer_update(INT32 cycles); // internal-use only (z80.cpp!)
void  z80ctc_trg_write(int ch, UINT8 data);
UINT8 z80ctc_read(int offset);
void  z80ctc_write(int offset, UINT8 data);
INT32 z80ctc_getperiod(int ch);

#endif
