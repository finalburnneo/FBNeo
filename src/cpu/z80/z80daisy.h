/***************************************************************************

    z80daisy.h

    Z80/180 daisy chaining support functions.

***************************************************************************/


#ifndef Z80DAISY_H
#define Z80DAISY_H

#include "z80ctc.h"
#include "z80pio.h"

#define Z80_CTC 0x01
#define Z80_PIO 0x02

void z80daisy_init(int dev0, int dev1); // for driver-dev, use this. (see drv/pre90s/d_senjyo.cpp)

/* daisy-chain link */
struct z80_irq_daisy_chain
{
	void (*reset)(); 			/* reset callback */
	int (*irq_state)();			/* get interrupt state */
	int (*irq_ack)();			/* interrupt acknowledge callback */
	void (*irq_reti)();			/* reti callback */
	void (*dev_exit)();			/* exit callback */
	void (*dev_scan)(INT32);	/* scan callback */
	int param;					/* callback parameter (-1 ends list) */
};


/* these constants are returned from the irq_state function */
#define Z80_DAISY_INT 	0x01		/* interrupt request mask */
#define Z80_DAISY_IEO 	0x02		/* interrupt disable mask (IEO) */

extern INT32 z80daisy_has_ctc; // status for internal-use (z80.cpp)

/* prototypes */
// this are called automatically by the z80 core, after the daisy has been
// initted for said cpu.  do not use these functions in-driver!
void z80daisy_reset(const struct z80_irq_daisy_chain *daisy);
int z80daisy_update_irq_state(const struct z80_irq_daisy_chain *chain);
int z80daisy_call_ack_device(const struct z80_irq_daisy_chain *chain);
void z80daisy_call_reti_device(const struct z80_irq_daisy_chain *chain);

void z80daisy_exit();
void z80daisy_scan(int nAction);
void Z80SetDaisy(void *dptr); // from z80.cpp

#endif
