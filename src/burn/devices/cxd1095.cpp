// license:BSD-3-Clause
// copyright-holders:AJR
// thanks-to:Aaron Giles
/**********************************************************************

    Sony CXD1095 CMOS I/O Port Expander

    Based on Sega X Board driver by Aaron Giles

    This device provides four 8-bit ports (PA-PD) and one 4-bit port
    (PE or PX). All these ports can be configured for input or for
    output, entirely or in parts. The upper and lower halves of ports
    A-D can be separately configured by the first of the two
    write-only control registers. The second control register
    determines the direction of individual PE/PX bits.

**********************************************************************/
#include "burnint.h"

#define	CXD1095_CHIPS	2

typedef UINT8 (*cxd1095_read_cb)(UINT8 offset);
typedef void (*cxd1095_write_cb)(UINT8 offset, UINT8 data);

static cxd1095_write_cb cxd1095_write_callbacks[CXD1095_CHIPS];
static cxd1095_read_cb cxd1095_read_callbacks[CXD1095_CHIPS];

static UINT8 cxd1095_data_latch[CXD1095_CHIPS][5];
static UINT8 cxd1095_data_dir[CXD1095_CHIPS][5];

UINT8 cxd1095_read(INT32 chip, UINT8 offset)
{
	if (offset < 5)
	{
		UINT8 input_data = 0;
		UINT8 input_mask = cxd1095_data_dir[chip][offset];
		if (offset == 4) input_mask &= 0x0f;

		if (input_mask != 0 && cxd1095_read_callbacks[chip] != NULL)
			input_data = cxd1095_read_callbacks[chip](offset) & input_mask;

		return input_data | (cxd1095_data_latch[chip][offset] & ~cxd1095_data_dir[chip][offset]);
	}

	return 0;
}

void cxd1095_write(INT32 chip, UINT8 offset, UINT8 data)
{
	if (offset < 5)
	{
		if (offset == 4) data &= 0x0f;
		cxd1095_data_latch[chip][offset] = data;

		data &= ~cxd1095_data_dir[chip][offset];
		if (cxd1095_write_callbacks[chip] != NULL)
			cxd1095_write_callbacks[chip](offset,data);
	}
	else if (offset == 6)
	{
		for (INT32 port = 0; port < 4; port++, data >>= 2)
		{
			cxd1095_data_dir[chip][port] = ((data & 1) ? 0x0f : 0) | ((data & 2) ? 0xf0 : 0);
		}
	}
	else if (offset == 7)
	{
		cxd1095_data_dir[chip][4] = (data & 0x0f) | 0xf0;
	}
}

void cxd1095Reset()
{
	memset (cxd1095_data_latch, 0, CXD1095_CHIPS * 5);
	memset (cxd1095_data_dir, 0xff, CXD1095_CHIPS * 5);
}

void cxd1095Init(INT32 chip, void (*write_cb)(UINT8,UINT8), UINT8 (*read_cb)(UINT8))
{
	cxd1095_write_callbacks[chip] = write_cb;
	cxd1095_read_callbacks[chip] = read_cb;
}

INT32 cxd1095Scan(INT32 nAction)
{
	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(cxd1095_data_latch);
		SCAN_VAR(cxd1095_data_dir);
	}

	return 0;
}
