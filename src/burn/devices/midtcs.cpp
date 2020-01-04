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

static INT32 cpu_select;
static INT32 pia_select;
static INT32 dac_select;

static void tcs_porta_w(UINT16, UINT8 data)
{
	dacvalue = (dacvalue & 3) | (data << 2);
	DACWrite16Signed(dac_select, dacvalue << 6);
}

static void tcs_portb_w(UINT16, UINT8 data)
{
	dacvalue = (dacvalue & ~3) | (data >> 6);
	DACWrite16Signed(dac_select, dacvalue << 6);
	tcs_status = (data >> 4) & 3;
}

static void tcs_irq(int state)
{
	M6809SetIRQLine(0, state ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

void tcs_data_write(UINT16 data)
{
	if (tcs_is_initialized == 0) return;

	INT32 cpunum = M6809GetActive();
	if (cpunum == -1) {
		M6809Open(cpu_select);
	}
	else if (cpunum != cpu_select)
	{
		M6809Close();
		M6809Open(cpu_select);
	}

	pia_set_input_b(pia_select, (data >> 1) & 0xf);
	pia_set_input_ca1(pia_select, ~data & 0x01);

	if (cpunum == -1) {
		M6809Close();
	}
	else if (cpunum != cpu_select)
	{
		M6809Close();
		M6809Open(cpunum);
	}
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
		INT32 cpunum = M6809GetActive();
		if (cpunum == -1) {
			M6809Open(cpu_select);
		}
		else if (cpunum != cpu_select)
		{
			M6809Close();
			M6809Open(cpu_select);
		}
		M6809Reset();
		if (cpunum == -1) {
			M6809Close();
		}
		else if (cpunum != cpu_select)
		{
			M6809Close();
			M6809Open(cpunum);
		}
	}
}

static void tcs_write(UINT16 address, UINT8 data)
{
	if ((address & 0xc000) == 0x4000) {
		pia_write(pia_select, address & 3, data);
		return;
	}
}

static UINT8 tcs_read(UINT16 address)
{
	if ((address & 0xc000) == 0x4000) {
		return pia_read(pia_select, address & 3);
	}

	return 0;
}

void tcs_reset()
{
	if (tcs_is_initialized == 0) return;

	M6809Open(cpu_select);
	M6809Reset();
	M6809Close();

	if (pia_select == 0) pia_reset();
	if (dac_select == 0) DACReset();

	tcs_status = 0;
	tcs_in_reset = 0;
	dacvalue = 0;
}

static const pia6821_interface pia_intf = {
	0, 0, 0, 0, 0, 0,
	tcs_porta_w, tcs_portb_w, 0, 0,
	tcs_irq, tcs_irq
};

void tcs_init(INT32 cpunum, INT32 pianum, INT32 dacnum, UINT8 *rom, UINT8 *ram)
{
	cpu_select = cpunum;
	pia_select = pianum;
	dac_select = dacnum;

	M6809Init(cpu_select);
	M6809Open(cpu_select);
	for (INT32 i = 0; i < 0x4000; i+=0x800) {
		M6809MapMemory(ram,	0x0000 + i, 0x07ff + i, MAP_RAM);
	}
	M6809MapMemory(rom + 0x8000,	0x8000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(tcs_write);
	M6809SetReadHandler(tcs_read);
	M6809Close();

	if (pia_select == 0) pia_init();
	pia_config(pia_select, PIA_ALTERNATE_ORDERING, &pia_intf);
	
	DACInit(dacnum, 0, 0, M6809TotalCycles, 2000000);
    DACSetRoute(dacnum, 1.00, BURN_SND_ROUTE_BOTH);
    DACDCBlock(1);

	tcs_is_initialized = 1;
}

void tcs_exit()
{
	if (tcs_is_initialized == 0) return;

	if (cpu_select == 0) M6809Exit();
	if (pia_select == 0) pia_init();
	if (dac_select == 0) DACExit();
	tcs_is_initialized = 0;
}

void tcs_scan(INT32 nAction, INT32 *pnMin)
{
	if (tcs_is_initialized == 0) return;

	if (nAction & ACB_VOLATILE)
	{
		if (cpu_select == 0) M6809Scan(nAction);
		if (dac_select == 0) DACScan(nAction, pnMin);
		if (pia_select == 0) pia_scan(nAction, pnMin);

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

