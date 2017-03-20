// Based on Mermaid / Beast MCU code by Angelo Salese, Tomasz Slanina, David Haywood

#include "burnint.h"
#include "z80_intf.h"
#include "mcs51.h"

static UINT8 *mermaid_inputs;
static UINT8 mermaid_p[4] = { 0, 0, 0, 0 };
static UINT8 data_to_mermaid;
static UINT8 data_to_z80;
static INT32 z80_to_mermaid_full;
static INT32 mermaid_to_z80_full;
static INT32 mermaid_int0;
static INT32 mermaid_initted = 0;
INT32 mermaid_sub_z80_reset;

void mermaidWrite(UINT8 data)
{
	data_to_mermaid = data;
	z80_to_mermaid_full = 1;
	mermaid_int0 = 0;
	mcs51_set_irq_line(MCS51_INT0_LINE, CPU_IRQSTATUS_ACK);
}

UINT8 mermaidRead()
{
	mermaid_to_z80_full = 0;
	return data_to_z80;
}

UINT8 mermaidStatus()
{
	return (!mermaid_to_z80_full << 2) | (z80_to_mermaid_full << 3);
}

static void mermaid_write_port(INT32 port, UINT8 data)
{
	switch (port)
	{
		case MCS51_PORT_P0:
		{
			if ((mermaid_p[0] & 2) == 0 && (data & 2)) {
				mermaid_to_z80_full = 1;
				data_to_z80 = mermaid_p[1];
			}
			if (data & 1) z80_to_mermaid_full = 0;

			mermaid_p[0] = data;
		}
		return;

		case MCS51_PORT_P1:
		{
			if (data == 0xff) {
				mermaid_int0 = 1;
				mcs51_set_irq_line(MCS51_INT0_LINE, CPU_IRQSTATUS_NONE);
			}

			mermaid_p[1] = data;
		}			
		return;

		case MCS51_PORT_P2:
		return;

		case MCS51_PORT_P3:
		{
			if ((data & 2) == 0) {
				ZetOpen(1);
				ZetReset();
				ZetClose();
				mermaid_sub_z80_reset = 1;
			} else {
				mermaid_sub_z80_reset = 0;
			}
		}
		return;
	}
}

static UINT8 mermaid_read_port(INT32 port)
{
#define BIT(x,y)	(((x)>>(y))&1)

	switch (port)
	{
		case MCS51_PORT_P0:
			return 0;

		case MCS51_PORT_P1:
			if (mermaid_p[0] & 1) return 0;
			return data_to_mermaid;

		case MCS51_PORT_P2:
			return mermaid_inputs[(mermaid_p[0] >> 2) & 3];

		case MCS51_PORT_P3:
		{
			UINT8 dsw = 0;
			UINT8 dsw1 = mermaid_inputs[4];
			UINT8 dsw2 = mermaid_inputs[5];
		
			switch ((mermaid_p[0] >> 5) & 3)
			{
				case 0: dsw = (BIT(dsw2, 4) << 3) | (BIT(dsw2, 0) << 2) | (BIT(dsw1, 4) << 1) | BIT(dsw1, 0); break;
				case 1: dsw = (BIT(dsw2, 5) << 3) | (BIT(dsw2, 1) << 2) | (BIT(dsw1, 5) << 1) | BIT(dsw1, 1); break;
				case 2: dsw = (BIT(dsw2, 6) << 3) | (BIT(dsw2, 2) << 2) | (BIT(dsw1, 6) << 1) | BIT(dsw1, 2); break;
				case 3: dsw = (BIT(dsw2, 7) << 3) | (BIT(dsw2, 3) << 2) | (BIT(dsw1, 7) << 1) | BIT(dsw1, 3); break;
			}
		
			return (dsw << 4) | (mermaid_int0 << 2) | (mermaid_to_z80_full << 3);
		}
	}

	return 0;
#undef BIT
}

void mermaidReset()
{
	mcs51_reset();

	memset (mermaid_p, 0, 4);
	mermaid_sub_z80_reset = 0;
	data_to_mermaid = 0;
	data_to_z80 = 0;
	z80_to_mermaid_full = 0;
	mermaid_to_z80_full = 0;
	mermaid_int0 = 0;
}

void mermaidInit(UINT8 *rom, UINT8 *inputs)
{
	mcs51_program_data=rom;
	mermaid_inputs = inputs;
	mcs51_init ();
	mcs51_set_write_handler(mermaid_write_port);
	mcs51_set_read_handler(mermaid_read_port);
	mermaid_initted = 1;
}

void mermaidExit()
{
	if (!mermaid_initted) return;
	mermaid_initted = 0;
	mcs51_exit();
}

INT32 mermaidRun(INT32 cycles)
{
	mcs51Run(cycles / 12);

	return cycles;
}

INT32 mermaidScan(INT32 nAction)
{
	SCAN_VAR(mermaid_sub_z80_reset);
	SCAN_VAR(data_to_mermaid);
	SCAN_VAR(data_to_z80);
	SCAN_VAR(z80_to_mermaid_full);
	SCAN_VAR(mermaid_to_z80_full);
	SCAN_VAR(mermaid_int0);
	mcs51_scan(nAction);

	return 0;
}
