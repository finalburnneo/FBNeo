/***************************************************************************

    TMS34010: Portable Texas Instruments TMS34010 emulator

    Copyright Alex Pasadyn/Zsolt Vasvari
    Parts based on code by Aaron Giles

***************************************************************************/

#pragma once

#ifndef __TMS34010_H__
#define __TMS34010_H__

#define HAS_TMS34020 1
#define offs_t UINT32

#include "burnint.h"


/* register indexes for get_reg and set_reg */
enum
{
	TMS34010_PC = 1,
	TMS34010_SP,
	TMS34010_ST,
	TMS34010_A0,
	TMS34010_A1,
	TMS34010_A2,
	TMS34010_A3,
	TMS34010_A4,
	TMS34010_A5,
	TMS34010_A6,
	TMS34010_A7,
	TMS34010_A8,
	TMS34010_A9,
	TMS34010_A10,
	TMS34010_A11,
	TMS34010_A12,
	TMS34010_A13,
	TMS34010_A14,
	TMS34010_B0,
	TMS34010_B1,
	TMS34010_B2,
	TMS34010_B3,
	TMS34010_B4,
	TMS34010_B5,
	TMS34010_B6,
	TMS34010_B7,
	TMS34010_B8,
	TMS34010_B9,
	TMS34010_B10,
	TMS34010_B11,
	TMS34010_B12,
	TMS34010_B13,
	TMS34010_B14
};

/* Configuration structure */
typedef struct _tms34010_display_params tms34010_display_params;
struct _tms34010_display_params
{
	UINT16	vcount;								/* most recent VCOUNT */
	UINT16	vtotal, htotal;
	UINT16	veblnk, vsblnk;						/* start/end of VBLANK */
	UINT16	heblnk, hsblnk;						/* start/end of HBLANK */
	UINT16	rowaddr, coladdr;					/* row/column addresses */
	UINT8	yoffset;							/* y offset from addresses */
	UINT8	enabled;							/* video enabled */
};

typedef int (*scanline_render_t)(int line, tms34010_display_params*);
void tms34010_generate_scanline(INT32 line, scanline_render_t render);

typedef struct _tms34010_config tms34010_config;
struct _tms34010_config
{
	UINT8	halt_on_reset;						/* /HCS pin, which determines HALT state after reset */
	UINT32	pixclock;							/* the pixel clock (0 means don't adjust screen size) */
	UINT32  cpu_cyc_per_frame;
	INT32	pixperclock;						/* pixels per clock */
	void	(*output_int)(INT32 state);			/* output interrupt callback */
	void	(*to_shiftreg)(UINT32, UINT16 *);	/* shift register write */
	void	(*from_shiftreg)(UINT32, UINT16 *);	/* shift register read */
};


/* PUBLIC FUNCTIONS - 34010 */
void tms34010_get_display_params(tms34010_display_params *params);
void tms34010_io_register_w(INT32 address, UINT32 data);
void tms34020_io_register_w(INT32 address, UINT32 data);
UINT16 tms34010_io_register_r(INT32 address);
UINT16 tms34020_io_register_r(INT32 address);

void tms34010_init();
void tms34010_set_toshift(void (*to_shiftreg)(UINT32, UINT16 *));
void tms34010_set_fromshift(void (*from_shiftreg)(UINT32, UINT16 *));
void tms34010_set_pixclock(INT32 pxlclock, INT32 pxl_per_clock);
void tms34010_set_cycperframe(INT32 cpf);
void tms34010_set_output_int(void (*oi_func)(INT32));
void tms34010_set_halt_on_reset(INT32 onoff);

void tms34010_exit();
void tms34010_reset();
void tms34020_reset();
void tms34010_scan(INT32 nAction);

void tms34010_set_irq_line(int irqline, int linestate);
int tms34010_run(int cycles);
void tms34010_stop();
int tms34010_idle(int cycles);
INT64 tms34010_total_cycles();
void tms34010_new_frame();
UINT32 tms34010_get_pc();
void tms34010_modify_timeslice(int cycles);

int tms34010_context_size();
void tms34010_get_context(void *get);
void tms34010_set_context(void *set);

void tms34010_timer_set_cb(void (*t_cb)());
void tms34010_timer_arm(int cycle);

/* Host control interface */
#define TMS34010_HOST_ADDRESS_L		0
#define TMS34010_HOST_ADDRESS_H		1
#define TMS34010_HOST_DATA			2
#define TMS34010_HOST_CONTROL		3

void tms34010_host_w(INT32 reg, UINT16 data);
UINT16 tms34010_host_r(INT32 reg);


/* Use this macro in the memory definitions to specify bit-based addresses */
#define TOBYTE(bitaddr) ((offs_t)(bitaddr) >> 3)
#define TOWORD(bitaddr) ((offs_t)(bitaddr) >> 4)


#endif /* __TMS34010_H__ */
