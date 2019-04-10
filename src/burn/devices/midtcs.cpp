// Midway Turbo Cheap Squeak audio module
// based on MAME sources by Aaron Giles

#include "burnint.h"
#include "m6809_intf.h"
#include "dac.h"
#include "6821pia.h"

static UINT16 dacvalue;
static INT32 tcs_status;
static INT32 tcs_in_reset;
static INT32 tcs_is_initialized = 0;

static void tcs_porta_w(UINT16, UINT8 data)
{
	dacvalue = (dacvalue & 3) | (data << 2);
	DACWrite16Signed(0, dacvalue << 6);
}

static void tcs_portb_w(UINT16, UINT8 data)
{
	dacvalue = (dacvalue & ~3) | (data >> 6);
	DACWrite16Signed(0, dacvalue << 6);
	tcs_status = (data >> 4) & 3;
}

static void tcs_irq(int state)
{
	M6809SetIRQLine(0, state ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

void tcs_data_write(UINT16 data)
{
	if (tcs_is_initialized == 0) return;

	pia_set_input_b(0, (data >> 1) & 0xf);
	pia_set_input_ca1(0, ~data & 0x01);
}

UINT8 tcs_status_read()
{
	if (tcs_is_initialized == 0) return 0;
	return tcs_status;
}

void tcs_reset_write(int state)
{
	if (tcs_is_initialized == 0) return;

	tcs_in_reset = state;

	if (state) {
		M6809Reset();
	}
}

static void tcs_write(UINT16 address, UINT8 data)
{
	if ((address & 0xc000) == 0x4000) {
		pia_write(0, address & 3, data);
		return;
	}
}

static UINT8 tcs_read(UINT16 address)
{
	if ((address & 0xc000) == 0x4000) {
		return pia_read(0, address & 3);
	}

	return 0;
}

void tcs_reset()
{
	if (tcs_is_initialized == 0) return;

	M6809Open(0);
	M6809Reset();
	M6809Close();

	pia_reset();

	DACReset();

	tcs_status = 0;
	tcs_in_reset = 0;
	dacvalue = 0;
}

static const pia6821_interface pia_intf = {
	0, 0, 0, 0, 0, 0,
	tcs_porta_w, tcs_portb_w, 0, 0,
	tcs_irq, tcs_irq
};

void tcs_init(UINT8 *rom, UINT8 *ram)
{
	M6809Init(0);
	M6809Open(0);
	for (INT32 i = 0; i < 0x4000; i+=0x800) {
		M6809MapMemory(ram,	0x0000 + i, 0x07ff + i, MAP_RAM);
	}
	M6809MapMemory(rom + 0x8000,	0x8000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(tcs_write);
	M6809SetReadHandler(tcs_read);
	M6809Close();

	pia_init();
	pia_config(0, PIA_ALTERNATE_ORDERING, &pia_intf);
	
	DACInit(0, 0, 0, M6809TotalCycles, 2000000);
    DACSetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);
    DACDCBlock(1);

	tcs_is_initialized = 1;
}

void tcs_exit()
{
	if (tcs_is_initialized == 0) return;

	M6809Exit();
	pia_init();
	DACExit();
	tcs_is_initialized = 0;
}

void tcs_scan(INT32 nAction, INT32 *pnMin)
{
	if (tcs_is_initialized == 0) return;

	if (nAction & ACB_VOLATILE)
	{
		M6809Scan(nAction);
		DACScan(nAction, pnMin);
		pia_scan(nAction, pnMin);

		SCAN_VAR(tcs_status);
		SCAN_VAR(tcs_in_reset);
		SCAN_VAR(dacvalue);
	}
}

INT32 tcs_reset_status()
{
	if (tcs_is_initialized == 0) return 1;

	return tcs_in_reset;
}

INT32 tcs_initialized()
{
	return tcs_is_initialized;
}

