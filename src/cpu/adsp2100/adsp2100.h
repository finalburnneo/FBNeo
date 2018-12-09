/***************************************************************************

    ADSP2100.h
    Interface file for the portable Analog ADSP-2100 emulator.
    Written by Aaron Giles

***************************************************************************/

#pragma once

#ifndef __ADSP2100_H__
#define __ADSP2100_H__

#include "adsp2100_defs.h"

/***************************************************************************
    REGISTER ENUMERATION
***************************************************************************/

enum
{
	ADSP2100_PC,
	ADSP2100_AX0, ADSP2100_AX1, ADSP2100_AY0, ADSP2100_AY1, ADSP2100_AR, ADSP2100_AF,
	ADSP2100_MX0, ADSP2100_MX1, ADSP2100_MY0, ADSP2100_MY1, ADSP2100_MR0, ADSP2100_MR1, ADSP2100_MR2, ADSP2100_MF,
	ADSP2100_SI, ADSP2100_SE, ADSP2100_SB, ADSP2100_SR0, ADSP2100_SR1,
	ADSP2100_I0, ADSP2100_I1, ADSP2100_I2, ADSP2100_I3, ADSP2100_I4, ADSP2100_I5, ADSP2100_I6, ADSP2100_I7,
	ADSP2100_L0, ADSP2100_L1, ADSP2100_L2, ADSP2100_L3, ADSP2100_L4, ADSP2100_L5, ADSP2100_L6, ADSP2100_L7,
	ADSP2100_M0, ADSP2100_M1, ADSP2100_M2, ADSP2100_M3, ADSP2100_M4, ADSP2100_M5, ADSP2100_M6, ADSP2100_M7,
	ADSP2100_PX, ADSP2100_CNTR, ADSP2100_ASTAT, ADSP2100_SSTAT, ADSP2100_MSTAT,
	ADSP2100_PCSP, ADSP2100_CNTRSP, ADSP2100_STATSP, ADSP2100_LOOPSP,
	ADSP2100_IMASK, ADSP2100_ICNTL, ADSP2100_IRQSTATE0, ADSP2100_IRQSTATE1, ADSP2100_IRQSTATE2, ADSP2100_IRQSTATE3,
	ADSP2100_FLAGIN, ADSP2100_FLAGOUT, ADSP2100_FL0, ADSP2100_FL1, ADSP2100_FL2,
	ADSP2100_AX0_SEC, ADSP2100_AX1_SEC, ADSP2100_AY0_SEC, ADSP2100_AY1_SEC, ADSP2100_AR_SEC, ADSP2100_AF_SEC,
	ADSP2100_MX0_SEC, ADSP2100_MX1_SEC, ADSP2100_MY0_SEC, ADSP2100_MY1_SEC, ADSP2100_MR0_SEC, ADSP2100_MR1_SEC, ADSP2100_MR2_SEC, ADSP2100_MF_SEC,
	ADSP2100_SI_SEC, ADSP2100_SE_SEC, ADSP2100_SB_SEC, ADSP2100_SR0_SEC, ADSP2100_SR1_SEC,

	ADSP2100_GENPC = REG_GENPC,
	ADSP2100_GENSP = REG_GENSP,
	ADSP2100_GENPCBASE = REG_GENPCBASE
};

void adsp21xx_reset(adsp2100_state *adsp);
void adsp21xx_exit(adsp2100_state *adsp);
int adsp21xx_execute(adsp2100_state *adsp, int cycles);
void adsp21xx_set_irq_line(adsp2100_state *adsp, int irqline, int state);

void adsp21xx_stop_execute(adsp2100_state *adsp);
int adsp21xx_total_cycles(adsp2100_state *adsp);
void adsp21xx_new_frame(adsp2100_state *adsp);
void adsp21xx_scan(adsp2100_state *adsp, INT32 nAction);

// direct memory r/w (functions in adsp2100_intf.cpp)
UINT16 adsp21xx_data_read_word_16le(UINT32 address);
UINT32 adsp21xx_read_dword_32le(UINT32 address);
void adsp21xx_data_write_word_16le(UINT32 address, UINT16 data);
void adsp21xx_write_dword_32le(UINT32 address, UINT32 data);

/***************************************************************************
    PUBLIC FUNCTIONS
***************************************************************************/

#define ADSP2100_IRQ0		0		/* IRQ0 */
#define ADSP2100_SPORT1_RX	0		/* SPORT1 receive IRQ */
#define ADSP2100_IRQ1		1		/* IRQ1 */
#define ADSP2100_SPORT1_TX	1		/* SPORT1 transmit IRQ */
#define ADSP2100_IRQ2		2		/* IRQ2 */
#define ADSP2100_IRQ3		3		/* IRQ3 */

//CPU_GET_INFO( adsp2100 );
//#define CPU_ADSP2100 CPU_GET_INFO_NAME( adsp2100 )
void adsp2100_init(adsp2100_state *adsp, cpu_irq_callback irqcallback);

/**************************************************************************
 * ADSP2101 section
 **************************************************************************/

#define ADSP2101_IRQ0		0		/* IRQ0 */
#define ADSP2101_SPORT1_RX	0		/* SPORT1 receive IRQ */
#define ADSP2101_IRQ1		1		/* IRQ1 */
#define ADSP2101_SPORT1_TX	1		/* SPORT1 transmit IRQ */
#define ADSP2101_IRQ2		2		/* IRQ2 */
#define ADSP2101_SPORT0_RX	3		/* SPORT0 receive IRQ */
#define ADSP2101_SPORT0_TX	4		/* SPORT0 transmit IRQ */
#define ADSP2101_TIMER		5		/* internal timer IRQ */

//CPU_GET_INFO( adsp2101 );
//#define CPU_ADSP2101 CPU_GET_INFO_NAME( adsp2101 )
void adsp2104_init(adsp2100_state *adsp, cpu_irq_callback irqcallback);

/**************************************************************************
 * ADSP2104 section
 **************************************************************************/

#define ADSP2104_IRQ0		0		/* IRQ0 */
#define ADSP2104_SPORT1_RX	0		/* SPORT1 receive IRQ */
#define ADSP2104_IRQ1		1		/* IRQ1 */
#define ADSP2104_SPORT1_TX	1		/* SPORT1 transmit IRQ */
#define ADSP2104_IRQ2		2		/* IRQ2 */
#define ADSP2104_SPORT0_RX	3		/* SPORT0 receive IRQ */
#define ADSP2104_SPORT0_TX	4		/* SPORT0 transmit IRQ */
#define ADSP2104_TIMER		5		/* internal timer IRQ */

//CPU_GET_INFO( adsp2104 );
//#define CPU_ADSP2104 CPU_GET_INFO_NAME( adsp2104 )

void adsp2104_init(adsp2100_state *adsp, cpu_irq_callback irqcallback);
void adsp2104_load_boot_data(UINT8 *srcdata, UINT32 *dstdata);


/**************************************************************************
 * ADSP2105 section
 **************************************************************************/

#define ADSP2105_IRQ0		0		/* IRQ0 */
#define ADSP2105_SPORT1_RX	0		/* SPORT1 receive IRQ */
#define ADSP2105_IRQ1		1		/* IRQ1 */
#define ADSP2105_SPORT1_TX	1		/* SPORT1 transmit IRQ */
#define ADSP2105_IRQ2		2		/* IRQ2 */
#define ADSP2105_TIMER		5		/* internal timer IRQ */

//CPU_GET_INFO( adsp2105 );
//#define CPU_ADSP2105 CPU_GET_INFO_NAME( adsp2105 )

void adsp2105_init(adsp2100_state *adsp, cpu_irq_callback irqcallback);
void adsp2105_load_boot_data(UINT8 *srcdata, UINT32 *dstdata);


/**************************************************************************
 * ADSP2115 section
 **************************************************************************/

#define ADSP2115_IRQ0		0		/* IRQ0 */
#define ADSP2115_SPORT1_RX	0		/* SPORT1 receive IRQ */
#define ADSP2115_IRQ1		1		/* IRQ1 */
#define ADSP2115_SPORT1_TX	1		/* SPORT1 transmit IRQ */
#define ADSP2115_IRQ2		2		/* IRQ2 */
#define ADSP2115_SPORT0_RX	3		/* SPORT0 receive IRQ */
#define ADSP2115_SPORT0_TX	4		/* SPORT0 transmit IRQ */
#define ADSP2115_TIMER		5		/* internal timer IRQ */

//CPU_GET_INFO( adsp2115 );
//#define CPU_ADSP2115 CPU_GET_INFO_NAME( adsp2115 )

void adsp2115_init(adsp2100_state *adsp, cpu_irq_callback irqcallback);
void adsp2115_load_boot_data(UINT8 *srcdata, UINT32 *dstdata);


/**************************************************************************
 * ADSP2181 section
 **************************************************************************/

#define ADSP2181_IRQ0		0		/* IRQ0 */
#define ADSP2181_SPORT1_RX	0		/* SPORT1 receive IRQ */
#define ADSP2181_IRQ1		1		/* IRQ1 */
#define ADSP2181_SPORT1_TX	1		/* SPORT1 transmit IRQ */
#define ADSP2181_IRQ2		2		/* IRQ2 */
#define ADSP2181_SPORT0_RX	3		/* SPORT0 receive IRQ */
#define ADSP2181_SPORT0_TX	4		/* SPORT0 transmit IRQ */
#define ADSP2181_TIMER		5		/* internal timer IRQ */
#define ADSP2181_IRQE		6		/* IRQE */
#define ADSP2181_IRQL1		7		/* IRQL1 */
#define ADSP2181_IRQL2		8		/* IRQL2 */

//CPU_GET_INFO( adsp2181 );
//#define CPU_ADSP2181 CPU_GET_INFO_NAME( adsp2181 )
void adsp2181_init(adsp2100_state *adsp, cpu_irq_callback irqcallback);
void adsp2181_load_boot_data(UINT8 *srcdata, UINT32 *dstdata);
void adsp2181_idma_addr_w(adsp2100_state *adsp, UINT16 data);
UINT16 adsp2181_idma_addr_r(adsp2100_state *adsp);
void adsp2181_idma_data_w(adsp2100_state *adsp, UINT16 data);
UINT16 adsp2181_idma_data_r(adsp2100_state *adsp);


#endif /* __ADSP2100_H__ */
