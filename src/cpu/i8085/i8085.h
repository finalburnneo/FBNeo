// license:BSD-3-Clause
// copyright-holders:Juergen Buchmueller

/***************************************************************************
    CONSTANTS
***************************************************************************/

#define I8085_INTR_LINE     0
#define I8085_RST55_LINE    1
#define I8085_RST65_LINE    2
#define I8085_RST75_LINE    3
#define I8085_TRAP_LINE     CPU_IRQLINE_NMI

enum
{
	I8085_PC = 1, I8085_SP, I8085_AF, I8085_BC, I8085_DE, I8085_HL,
	I8085_A, I8085_B, I8085_C, I8085_D, I8085_E, I8085_F, I8085_H, I8085_L,
	I8085_STATUS, I8085_SOD, I8085_SID, I8085_INTE,
	I8085_HALT, I8085_IM
};

const UINT8 i8085_STATUS_INTA   = 0x01;
const UINT8 i8085_STATUS_WO     = 0x02;
const UINT8 i8085_STATUS_STACK  = 0x04;
const UINT8 i8085_STATUS_HLTA   = 0x08;
const UINT8 i8085_STATUS_OUT    = 0x10;
const UINT8 i8085_STATUS_M1     = 0x20;
const UINT8 i8085_STATUS_INP    = 0x40;
const UINT8 i8085_STATUS_MEMR   = 0x80;
