// Midway Squeak and Talk module
// based on MAME code by Aaron Giles

#include "burnint.h"
#include "m6800_intf.h"
#include "tms5220.h"
#include "6821pia.h"

static UINT8 *M6800RAM;
static INT32 tms_command;
static INT32 tms_strobes;
static INT32 midsat_in_reset;
static INT32 midsat_initialized = 0;

static void midsat_cpu_write(UINT16 address, UINT8 data)
{
	if (address < 0x0080) {
        M6800RAM[address] = data;
		return;
	}

	if ((address & 0xfffc) == 0x0080) {
		pia_write(0, address & 3, data);
		return;
	}

	if ((address & 0xfffc) == 0x0090) {
		pia_write(1, address & 3, data);
		return;
	}

	if ((address & 0x9000) == 0x1000) {
		// dac write (no chip on this hardware)
		return;
    }
}

static UINT8 midsat_cpu_read(UINT16 address)
{
	if (address < 0x0080) {
		return M6800RAM[address];
	}

    if ((address & 0xfffc) == 0x0080) {
		return pia_read(0, address & 3);
	}

	if ((address & 0xfffc) == 0x0090) {
		return pia_read(1, address & 3);
	}

	return 0xff;
}

static void pia0_out_a(UINT16 , UINT8 )
{
	// for ay8912 (no chip on this hardware)
}

static void pia1_out_a(UINT16 , UINT8 data)
{
    tms_command = data;
}

static void pia1_out_b(UINT16 , UINT8 data)
{
	if (((data ^ tms_strobes) & 2) && (~data & 2))
	{
        tms5220_write(tms_command);

		pia_set_input_ca2(1, 1);
		pia_set_input_ca2(1, 0);
	}
	else if (((data ^ tms_strobes) & 1) && (~data & 1))
	{
		pia_set_input_a(1, tms5220_status());

		pia_set_input_ca2(1, 1);
		pia_set_input_ca2(1, 0);
	}

	tms_strobes = data;
}

static void pia_irq(INT32 state)
{
	M6800SetIRQLine(M6800_IRQ_LINE, state ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static pia6821_interface pia_0 = {
	NULL, NULL,
	NULL, NULL, NULL, NULL,
	pia0_out_a, NULL, NULL, NULL,
	pia_irq, pia_irq
};

static pia6821_interface pia_1 = {
	NULL, NULL,
	NULL, NULL, NULL, NULL,
	pia1_out_a, pia1_out_b, NULL, NULL,
	pia_irq, pia_irq
};

void midsat_write(UINT8 data)
{
    M6800Open(0);
	pia_set_input_a(0, ~data & 0x0f);
    pia_set_input_cb1(0, (~data & 0x10) >> 4);
    M6800Close();
}

void midsat_reset_write(INT32 state)
{
	if (state == 0 && midsat_in_reset) {
		M6800Reset();
	}

    midsat_in_reset = state;
}

INT32 midsat_reset_status()
{
	return midsat_in_reset;
}

void midsat_reset()
{
	memset (M6800RAM, 0x00, 0x80);

	M6800Open(0);
	M6800Reset();
    tms5220_reset();
	pia_reset();
	M6800Close();

	tms_command = 0;
	tms_strobes = 0;
	midsat_in_reset = 0;
}

void midsat_init(UINT8 *rom)
{
	M6800RAM = (UINT8*)BurnMalloc(0x80);
	M6800Init(0);
	M6800Open(0);
    M6800MapMemory(rom + 0x0000,		0xd000, 0xffff, MAP_ROM);
	M6800SetWriteHandler(midsat_cpu_write);
	M6800SetReadHandler(midsat_cpu_read);
    M6800Close();

	pia_init();
	pia_config(0, 0, &pia_0);
	pia_config(1, 0, &pia_1);

	tms5200_init(M6800TotalCycles, 3579545/4);
    tms5220_set_frequency(640000);
    midsat_initialized = 1;
}

void midsat_exit()
{
	M6800Exit();
	pia_exit();
	tms5220_exit();

	BurnFree(M6800RAM);

	midsat_initialized = 0;
}

INT32 has_midsat()
{
	return midsat_initialized;
}

void midsatNewFrame()
{
    M6800NewFrame();
}

INT32 midsat_run(INT32 cycles)
{
    if (midsat_in_reset) {
		return cycles;
	}

    M6800Open(0);
    INT32 cyc = M6800Run(cycles);
    M6800Close();

    return cyc;
}

void midsat_update(INT16 *samples, INT32 length)
{
    M6800Open(0);
    tms5220_update(samples, length);
    M6800Close();
}

void midsat_scan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = M6800RAM;
		ba.nLen	  = 0x80;
		ba.szName = "M6800 Ram";
		BurnAcb(&ba);

		M6800Scan(nAction);
		pia_scan(nAction, pnMin);
		tms5220_scan(nAction, pnMin);

		SCAN_VAR(tms_strobes);
		SCAN_VAR(tms_command);
		SCAN_VAR(midsat_in_reset);
	}
}
