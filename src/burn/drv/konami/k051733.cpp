// license:BSD-3-Clause
// copyright-holders:Fabio Priuli, Acho A. Tang, R. Belmont
/*
Konami 051733
------
Sort of a protection device, used for collision detection, and for
arithmetical operations.
It is passed a few parameters, and returns the result.

Memory map(preliminary):
------------------------
00-01 W operand 1
02-03 W operand 2
04-05 W operand 3

00-01 R operand 1 / operand 2
02-03 R operand 1 % operand 2
04-05 R sqrt(operand 3 << 16)
06    R random number (LFSR)

06-07 W distance for collision check
08-09 W Y pos of obj1
0a-0b W X pos of obj1
0c-0d W Y pos of obj2
0e-0f W X pos of obj2
07    R collision flags

08-1f R mirror of 00-07

10-11 W NMI timer (triggers NMI on overflow)
12    W NMI enable (bit 0)
13    W unknown

Other addresses are unknown or unused.
*/

#include "burnint.h"
#include "bitswap.h"
#include <math.h>

static UINT8 K051733Ram[0x20];
static UINT16 K051733lfsr;
static UINT16 K051733nmi_timer;

static UINT16 clock_lfsr()
{
	UINT16 prev_val = K051733lfsr;
	const INT32 feedback = BIT(K051733lfsr, 1) ^ BIT(K051733lfsr, 8) ^ BIT(K051733lfsr, 12);
	K051733lfsr = K051733lfsr << 1 | feedback;

	return prev_val;
}

static UINT32 u32_sqrt(UINT32 op)
{
	return (op) ? (UINT32(sqrt(op)) & ~1) : 0;
}

void K051733Reset()
{
	memset (K051733Ram, 0, 0x20);
	K051733lfsr = 0xff;
	K051733nmi_timer = 0;
}

void K051733Write(INT32 offset, INT32 data)
{
	K051733Ram[offset & 0x1f] = data;
	clock_lfsr();
}

UINT8 K051733Read(INT32 offset)
{
	offset &= 0x07;
	const UINT8 lfsr = clock_lfsr();

	INT16 op1	= (K051733Ram[0x00] << 8) | K051733Ram[0x01];
	INT16 op2	= (K051733Ram[0x02] << 8) | K051733Ram[0x03];
	UINT16 op3	= (K051733Ram[0x04] << 8) | K051733Ram[0x05];

	UINT16 rad	= (K051733Ram[0x06] << 8) | K051733Ram[0x07];
	UINT16 y1	= (K051733Ram[0x08] << 8) | K051733Ram[0x09];
	UINT16 x1	= (K051733Ram[0x0a] << 8) | K051733Ram[0x0b];
	UINT16 y2	= (K051733Ram[0x0c] << 8) | K051733Ram[0x0d];
	UINT16 x2	= (K051733Ram[0x0e] << 8) | K051733Ram[0x0f];

	switch (offset)
	{
		case 0x00:
			if (op2) return	(op1 / op2) >> 8;
			else return 0xff;
		case 0x01:
			if (op2) return	(op1 / op2) & 0xff;
			else return 0xff;

		case 0x02:
			if (op2) return	(op1 % op2) >> 8;
			else return 0xff;
		case 0x03:
			if (op2) return	(op1 % op2) & 0xff;
			else return 0xff;

		case 0x04:
			return u32_sqrt(op3 << 16) >> 8;
		case 0x05:
			return u32_sqrt(op3 << 16) & 0xff;

		case 0x06:
			return lfsr;

		case 0x07: {
			const int dx = x2 - x1;
			const int dy = y2 - y1;

			// note: Chequered Flag definitely wants all these bits to be set
			if (abs(dx) > rad || abs(dy) > rad)
				return 0xff;

			// octant angle
			if (y1 >= y2)
			{
				if (x1 >= x2)
				{
					if (dy >= dx)
						return 0x00;
					else
						return 0x06;
				}
				else
				{
					if (dy >= -dx)
						return 0x04;
					else
						return 0x07;
				}
			}
			else
			{
				if (x1 >= x2)
				{
					if (dy > -dx)
						return 0x02;
					else
						return 0x01;
				}
				else
				{
					if (dy > dx)
						return 0x03;
					else
						return 0x05;
				}
			}
		}
	}

	return 0;
}

UINT8 K051733nmi_ok()
{
	UINT8 ret = 0;
	if (!K051733nmi_timer--) {
		if (K051733Ram[0x12] & 1) {
			ret = 1;
		}
		K051733nmi_timer = (K051733Ram[0x10] << 8) | K051733Ram[0x11];
	}

	return ret;
}

void K051733Scan(INT32 nAction)
{
	if (nAction & ACB_MEMORY_RAM) {
		ScanVar(K051733Ram, 0x20, "K051733 Ram");
		SCAN_VAR(K051733lfsr);
		SCAN_VAR(K051733nmi_timer);
	}
}
