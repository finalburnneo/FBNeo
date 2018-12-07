
#pragma once

#include "burnint.h"

#define INLINE inline

#define logerror(...)
#define fatalerror(...)


/* register definitions */
enum
{
    MAX_REGS = 256,

    REG_GENPCBASE = MAX_REGS - 1,	/* generic "base" PC, should point to start of current opcode */
    REG_GENPC = MAX_REGS - 2,		/* generic PC, may point within an opcode */
    REG_GENSP = MAX_REGS - 3		/* generic SP, or closest equivalent */
};


enum {
    CLEAR_LINE = 0,				/* clear (a fired, held or pulsed) line */
    ASSERT_LINE,				/* assert an interrupt immediately */
    HOLD_LINE,					/* hold interrupt line until acknowledged */
    PULSE_LINE					/* pulse interrupt line for one instruction */
};


typedef int (*cpu_irq_callback)(int state);


