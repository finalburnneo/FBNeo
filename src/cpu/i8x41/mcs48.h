// license:BSD-3-Clause
// copyright-holders:Mirko Buffoni
/***************************************************************************

    Intel MCS-48/UPI-41 Portable Emulator

    Copyright Mirko Buffoni
    Based on the original work Copyright Dan Boris, an 8048 emulator

***************************************************************************/

#ifndef _MCS48_H
#define _MCS48_H

extern bool cflyball_hack;

/***************************************************************************
    CONSTANTS
***************************************************************************/

// I/O Port map
// 0 - ffff: "ext"

enum
{
	MCS48_T0 = 0x20000,
	MCS48_T1,
	MCS48_P0,   // 8021/8022 only
	MCS48_P1,
	MCS48_P2,
	MCS48_BUS,
	MCS48_PROG
};


/* I/O port access indexes */
enum
{
	MCS48_INPUT_IRQ = 0,
	UPI41_INPUT_IBF = 0,
	MCS48_INPUT_EA
};

void mcs48_set_read_port(UINT8 (*read)(UINT32));
void mcs48_set_write_port(void (*write)(UINT32, UINT8));

void mcs48Init(INT32 nCpu, INT32 subtype, UINT8 *prg);
void mcs48Reset();
void mcs48Exit();
void mcs48Open(INT32 nCpu);
void mcs48Close();
void mcs48Scan(INT32 nAction);
INT32 mcs48GetActive();

INT32 mcs48Run(INT32 cycles);
void mcs48RunEnd();
INT32 mcs48TotalCycles();
void mcs48NewFrame();
INT32 mcs48Idle(INT32 cycles);
void mcs48SetIRQLine(INT32 inputnum, INT32 state);

void mcs48_master_w(INT32 offset, UINT8 data);
UINT8 mcs48_master_r(INT32 offset);

#endif
