// license:BSD-3-Clause
// copyright-holders:Eric Smith
/*
 * mathbox.c: math box simulation (Battlezone/Red Baron/Tempest)
 *
 * Copyright Eric Smith
 *
 */

#include "burnint.h"
#include "mathbox.h"


/* math box scratch registers */
static INT16 m_reg[16];

/* math box result */
static INT16 m_result;


#define REG0 m_reg [0x00]
#define REG1 m_reg [0x01]
#define REG2 m_reg [0x02]
#define REG3 m_reg [0x03]
#define REG4 m_reg [0x04]
#define REG5 m_reg [0x05]
#define REG6 m_reg [0x06]
#define REG7 m_reg [0x07]
#define REG8 m_reg [0x08]
#define REG9 m_reg [0x09]
#define REGa m_reg [0x0a]
#define REGb m_reg [0x0b]
#define REGc m_reg [0x0c]
#define REGd m_reg [0x0d]
#define REGe m_reg [0x0e]
#define REGf m_reg [0x0f]

void mathbox_reset()
{
	m_result = 0;
	memset(m_reg, 0, sizeof(m_reg));
}

void mathbox_scan(INT32 nAction, INT32*)
{
	SCAN_VAR(m_reg);
	SCAN_VAR(m_result);
}


void mathbox_go_write(UINT8 offset, UINT8 data)
{
	INT32 mb_temp = 0; /* temp 32-bit multiply results */
	INT16 mb_q = 0; /* temp used in division */
	INT32 msb = 0;

	switch (offset)
	{
	case 0x00: m_result = REG0 = (REG0 & 0xff00) | data;
		break;
	case 0x01: m_result = REG0 = (REG0 & 0x00ff) | (data << 8);
		break;
	case 0x02: m_result = REG1 = (REG1 & 0xff00) | data;
		break;
	case 0x03: m_result = REG1 = (REG1 & 0x00ff) | (data << 8);
		break;
	case 0x04: m_result = REG2 = (REG2 & 0xff00) | data;
		break;
	case 0x05: m_result = REG2 = (REG2 & 0x00ff) | (data << 8);
		break;
	case 0x06: m_result = REG3 = (REG3 & 0xff00) | data;
		break;
	case 0x07: m_result = REG3 = (REG3 & 0x00ff) | (data << 8);
		break;
	case 0x08: m_result = REG4 = (REG4 & 0xff00) | data;
		break;
	case 0x09: m_result = REG4 = (REG4 & 0x00ff) | (data << 8);
		break;

	case 0x0a: m_result = REG5 = (REG5 & 0xff00) | data;
		break;
	/* note: no function loads low part of REG5 without performing a computation */

	case 0x0c: m_result = REG6 = data;
		break;
	/* note: no function loads high part of REG6 */

	case 0x15: m_result = REG7 = (REG7 & 0xff00) | data;
		break;
	case 0x16: m_result = REG7 = (REG7 & 0x00ff) | (data << 8);
		break;

	case 0x1a: m_result = REG8 = (REG8 & 0xff00) | data;
		break;
	case 0x1b: m_result = REG8 = (REG8 & 0x00ff) | (data << 8);
		break;

	case 0x0d: m_result = REGa = (REGa & 0xff00) | data;
		break;
	case 0x0e: m_result = REGa = (REGa & 0x00ff) | (data << 8);
		break;
	case 0x0f: m_result = REGb = (REGb & 0xff00) | data;
		break;
	case 0x10: m_result = REGb = (REGb & 0x00ff) | (data << 8);
		break;

	case 0x17: m_result = REG7;
		break;
	case 0x19: m_result = REG8;
		break;
	case 0x18: m_result = REG9;
		break;

	case 0x0b:

		REG5 = (REG5 & 0x00ff) | (data << 8);

		REGf = static_cast<INT16>(0xffff);
		REG4 -= REG2;
		REG5 -= REG3;

	step_048:

		mb_temp = static_cast<INT32>(REG0) * static_cast<INT32>(REG4);
		REGc = mb_temp >> 16;
		REGe = mb_temp & 0xffff;

		mb_temp = -REG1 * static_cast<INT32>(REG5);
		REG7 = mb_temp >> 16;
		mb_q = mb_temp & 0xffff;

		REG7 += REGc;

	/* rounding */
		REGe = (REGe >> 1) & 0x7fff;
		REGc = (mb_q >> 1) & 0x7fff;
		mb_q = REGc + REGe;
		if (mb_q < 0)
			REG7++;

		m_result = REG7;

		if (REGf < 0)
			break;

		REG7 += REG2;

	/* fall into command 12 */

	case 0x12:
		mb_temp = static_cast<INT32>(REG1) * static_cast<INT32>(REG4);
		REGc = mb_temp >> 16;
		REG9 = mb_temp & 0xffff;

		mb_temp = static_cast<INT32>(REG0) * static_cast<INT32>(REG5);
		REG8 = mb_temp >> 16;
		mb_q = mb_temp & 0xffff;

		REG8 += REGc;

	/* rounding */
		REG9 = (REG9 >> 1) & 0x7fff;
		REGc = (mb_q >> 1) & 0x7fff;
		REG9 += REGc;
		if (REG9 < 0)
			REG8++;
		REG9 <<= 1; /* why? only to get the desired load address? */

		m_result = REG8;

		if (REGf < 0)
			break;

		REG8 += REG3;

		REG9 &= 0xff00;

	/* fall into command 13 */

	case 0x13:
		REGc = REG9;
		mb_q = REG8;
		goto step_0bf;

	case 0x14:
		REGc = REGa;
		mb_q = REGb;

	step_0bf:
		REGe = REG7 ^ mb_q; /* save sign of result */
		REGd = mb_q;
		if (mb_q >= 0)
			mb_q = REGc;
		else
		{
			REGd = -mb_q - 1;
			mb_q = -REGc - 1;
			if ((mb_q < 0) && ((mb_q + 1) < 0))
				REGd++;
			mb_q++;
		}

	/* step 0c9: */
	/* REGc = abs (REG7) */
		if (REG7 >= 0)
			REGc = REG7;
		else
			REGc = -REG7;

		REGf = REG6; /* step counter */

		do
		{
			REGd -= REGc;
			msb = ((mb_q & 0x8000) != 0);
			mb_q <<= 1;
			if (REGd >= 0)
				mb_q++;
			else
				REGd += REGc;
			REGd <<= 1;
			REGd += msb;
		}
		while (--REGf >= 0);

		if (REGe >= 0)
			m_result = mb_q;
		else
			m_result = -mb_q;
		break;

	case 0x11:
		REG5 = (REG5 & 0x00ff) | (data << 8);
		REGf = 0x0000; /* do everything in one step */
		goto step_048;
	//break; // never reached

	case 0x1c:
		/* window test? */
		REG5 = (REG5 & 0x00ff) | (data << 8);
		do
		{
			REGe = (REG4 + REG7) >> 1;
			REGf = (REG5 + REG8) >> 1;
			if ((REGb < REGe) && (REGf < REGe) && ((REGe + REGf) >= 0))
			{
				REG7 = REGe;
				REG8 = REGf;
			}
			else
			{
				REG4 = REGe;
				REG5 = REGf;
			}
		}
		while (--REG6 >= 0);

		m_result = REG8;
		break;

	case 0x1d:
		REG3 = (REG3 & 0x00ff) | (data << 8);

		REG2 -= REG0;
		if (REG2 < 0)
			REG2 = -REG2;

		REG3 -= REG1;
		if (REG3 < 0)
			REG3 = -REG3;

	/* fall into command 1e */

	case 0x1e:
		/* result = max (REG2, REG3) + 3/8 * min (REG2, REG3) */
		if (REG3 >= REG2)
		{
			REGc = REG2;
			REGd = REG3;
		}
		else
		{
			REGd = REG2;
			REGc = REG3;
		}
		REGc >>= 2;
		REGd += REGc;
		REGc >>= 1;
		m_result = REGd = (REGc + REGd);
		break;

	case 0x1f:
		/* $$$ do some computation here (selftest? signature analysis? */
		break;
	}
}

UINT8 mathbox_status_read()
{
	return 0x00; /* always done! */
}

UINT8 mathbox_lo_read()
{
	return m_result & 0xff;
}

UINT8 mathbox_hi_read()
{
	return (m_result >> 8) & 0xff;
}
