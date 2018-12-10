#ifndef ADSP2100_DEFS
#define ADSP2100_DEFS

#include <stdint.h>
#include "burnint.h"

/***************************************************************************
    CONSTANTS
***************************************************************************/

#define TRACK_HOTSPOTS		0

/* stack depths */
#define	PC_STACK_DEPTH		16
#define CNTR_STACK_DEPTH	4
#define STAT_STACK_DEPTH	4
#define LOOP_STACK_DEPTH	4

/* chip types */
#define CHIP_TYPE_ADSP2100	0
#define CHIP_TYPE_ADSP2101	1
#define CHIP_TYPE_ADSP2104	2
#define CHIP_TYPE_ADSP2105	3
#define CHIP_TYPE_ADSP2115	4
#define CHIP_TYPE_ADSP2181	5

#define CLEAR_LINE 0
#ifdef INLINE
#undef INLINE
#endif
#define INLINE inline

#define logerror(...)
#define fatalerror(...)

typedef int (*cpu_irq_callback)(int state);

/* register definitions */
enum
{
    MAX_REGS = 256,

    REG_GENPCBASE = MAX_REGS - 1,	/* generic "base" PC, should point to start of current opcode */
    REG_GENPC = MAX_REGS - 2,		/* generic PC, may point within an opcode */
    REG_GENSP = MAX_REGS - 3		/* generic SP, or closest equivalent */
};


/***************************************************************************
    TYPE DEFINITIONS
***************************************************************************/

struct ADSP2100STATE;

/* transmit and receive data callbacks types */
typedef INT32 (*adsp21xx_rx_func)(ADSP2100STATE *adsp, int port);
typedef void  (*adsp21xx_tx_func)(ADSP2100STATE *adsp, int port, INT32 data);
typedef void  (*adsp21xx_timer_func)(ADSP2100STATE *adsp, int enable);

typedef struct _adsp21xx_config adsp21xx_config;
struct _adsp21xx_config
{
    adsp21xx_rx_func		rx;				/* callback for serial receive */
    adsp21xx_tx_func		tx;				/* callback for serial transmit */
    adsp21xx_timer_func		timer;			/* callback for timer fired */
};


/***************************************************************************
    STRUCTURES & TYPEDEFS
***************************************************************************/

/* 16-bit registers that can be loaded signed or unsigned */
typedef union
{
    UINT16	u;
    INT16	s;
} ADSPREG16;


/* the SHIFT result register is 32 bits */
typedef union
{
#ifdef LSB_FIRST
    struct { ADSPREG16 sr0, sr1; } srx;
#else
    struct { ADSPREG16 sr1, sr0; } srx;
#endif
    UINT32 sr;
} SHIFTRESULT;


/* the MAC result register is 40 bits */
typedef union
{
#ifdef LSB_FIRST
    struct { ADSPREG16 mr0, mr1, mr2, mrzero; } mrx;
    struct { UINT32 mr0, mr1; } mry;
#else
    struct { ADSPREG16 mrzero, mr2, mr1, mr0; } mrx;
    struct { UINT32 mr1, mr0; } mry;
#endif
    UINT64 mr;
} MACRESULT;

/* there are two banks of "core" registers */
typedef struct ADSPCORE
{
    /* ALU registers */
    ADSPREG16	ax0, ax1;
    ADSPREG16	ay0, ay1;
    ADSPREG16	ar;
    ADSPREG16	af;

    /* MAC registers */
    ADSPREG16	mx0, mx1;
    ADSPREG16	my0, my1;
    MACRESULT	mr;
    ADSPREG16	mf;

    /* SHIFT registers */
    ADSPREG16	si;
    ADSPREG16	se;
    ADSPREG16	sb;
    SHIFTRESULT	sr;

    /* dummy registers */
    ADSPREG16	zero;
} ADSPCORE;


/* ADSP-2100 Registers */
typedef struct ADSP2100STATE
{
    /* Core registers, 2 banks */
    ADSPCORE	core;
    ADSPCORE	alt;

    /* Memory addressing registers */
    UINT32		i[8];
    INT32		m[8];
    UINT32		l[8];
    UINT32		lmask[8];
    UINT32		base[8];
    UINT8		px;

    /* other CPU registers */
    UINT32		pc;
    UINT32		ppc;
    UINT32		loop;
    UINT32		loop_condition;
    UINT32		cntr;

    /* status registers */
    UINT32		astat;
    UINT32		sstat;
    UINT32		mstat;
    UINT32		mstat_prev;
    UINT32		astat_clear;
    UINT32		idle;

    /* stacks */
    UINT32		loop_stack[LOOP_STACK_DEPTH];
    UINT32		cntr_stack[CNTR_STACK_DEPTH];
    UINT32		pc_stack[PC_STACK_DEPTH];
    UINT16		stat_stack[STAT_STACK_DEPTH][3];
    INT32		pc_sp;
    INT32		cntr_sp;
    INT32		stat_sp;
    INT32		loop_sp;

    /* external I/O */
    UINT8		flagout;
    UINT8		flagin;
    UINT8		fl0;
    UINT8		fl1;
    UINT8		fl2;
    UINT16		idma_addr;
    UINT16		idma_cache;
    UINT8		idma_offs;

    /* interrupt handling */
    UINT16		imask;
    UINT8		icntl;
    UINT16		ifc;
    UINT8    	irq_state[9];
    UINT8    	irq_latch[9];

	cpu_irq_callback irq_callback;
    //const device_config *device;

    /* other internal states */
	int			icount;

	int         total_cycles;
	int         cycles_start;
	int         end_run;


	int			chip_type;
    int			mstat_mask;
    int			imask_mask;

    /* register maps */
    void *		alu_xregs[8];
    void *		alu_yregs[4];
    void *		mac_xregs[8];
    void *		mac_yregs[4];
    void *		shift_xregs[8];

    /* other callbacks */
    adsp21xx_rx_func sport_rx_callback;
    adsp21xx_tx_func sport_tx_callback;
    adsp21xx_timer_func timer_fired;

    /* memory spaces */
//    const address_space *program;
//    const address_space *data;
//    const address_space *io;
//    cpu_state_table state;

} adsp2100_state;



#endif // ADSP2100_DEFS

