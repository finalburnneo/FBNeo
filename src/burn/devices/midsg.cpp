// Midway Sounds Good audio module
// based on MAME sources by Aaron Giles

#include "burnint.h"
#include "m68000_intf.h"
#include "6821pia.h"
#include "dac.h"

static UINT16 dacvalue;
static UINT16 soundsgood_status;
static INT32 soundsgood_in_reset;
static INT32 soundsgood_is_initialized;

static void soundsgood_porta_w(UINT16, UINT8 data)
{
	dacvalue = (dacvalue & 3) | (data << 2);
	DACWrite16Signed(0, dacvalue << 6);
}

static void soundsgood_portb_w(UINT16, UINT8 data)
{
    dacvalue = (dacvalue & ~3) | (data >> 6);
	DACWrite16Signed(0, dacvalue << 6);

	if (pia_get_ddr_b(0) & 0x30) soundsgood_status = (data >> 4) & 3;
}

static void soundsgood_irq(int state)
{
	SekSetIRQLine(4, state ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

void soundsgood_data_write(UINT16 data)
{
	pia_set_input_b(0, (data >> 1) & 0xf);
	pia_set_input_ca1(0, ~data & 0x01);
}

UINT8 soundsgood_status_read()
{
	if (soundsgood_is_initialized == 0) return 0;
	return soundsgood_status;
}

void soundsgood_reset_write(int state)
{
	if (soundsgood_is_initialized == 0) return;

	soundsgood_in_reset = state;

	if (state)
	{
		INT32 cpu_active = SekGetActive();
		if (cpu_active == -1) SekOpen(0);
		SekReset();
		if (cpu_active == -1) SekClose();
	}
}

static void __fastcall soundsgood_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xffff8) == 0x060000) {
	//	bprintf (0, _T("SGWW: %5.5x, %4.4x\n"), address, data);
		pia_write(0, (address / 2) & 3, data & 0xff);
		return;
	}
}

static void __fastcall soundsgood_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xffff8) == 0x060000) {
	//	bprintf (0, _T("SGWB: %5.5x, %2.2x\n"), address, data);
		pia_write(0, (address / 2) & 3, data);
		return;
	}
}

static UINT16 __fastcall soundsgood_read_word(UINT32 address)
{
	if ((address & 0xffff8) == 0x060000) {
	//	bprintf (0, _T("SGRW: %5.5x\n"), address);
		UINT8 ret = pia_read(0, (address / 2) & 3);
		return ret | (ret << 8);
	}

	return 0;
}

static UINT8 __fastcall soundsgood_read_byte(UINT32 address)
{
	if ((address & 0xffff8) == 0x060000) {
	//	bprintf (0, _T("SGRB: %5.5x\n"), address);
		return pia_read(0, (address / 2) & 3);
	}

	return 0;
}

void soundsgood_reset()
{
	if (soundsgood_is_initialized == 0) return;

	SekOpen(0);
	SekReset();
	DACReset();
	SekClose();

	pia_reset();

	soundsgood_status = 0;
	soundsgood_in_reset = 0;
	dacvalue = 0;
}

static const pia6821_interface pia_intf = {
	0, 0, 0, 0, 0, 0,
	soundsgood_porta_w, soundsgood_portb_w, 0, 0,
	soundsgood_irq, soundsgood_irq
};

void soundsgood_init(UINT8 *rom, UINT8 *ram)
{
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(rom,			0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(ram,			0x070000, 0x070fff, MAP_RAM);
	SekSetWriteWordHandler(0,	soundsgood_write_word);
	SekSetWriteByteHandler(0,	soundsgood_write_byte);
	SekSetReadWordHandler(0,	soundsgood_read_word);
	SekSetReadByteHandler(0,	soundsgood_read_byte);
	SekClose();

	pia_init();
	pia_config(0, PIA_ALTERNATE_ORDERING, &pia_intf);
	
	DACInit(0, 0, 0, SekTotalCycles, 8000000);
	DACSetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);
    DACDCBlock(1);

	soundsgood_is_initialized = 1;
}

void soundsgood_exit()
{
	if (soundsgood_is_initialized == 0) return;

	SekExit();
	pia_init();
	DACExit();

	soundsgood_is_initialized = 0;
}

void soundsgood_scan(INT32 nAction, INT32 *pnMin)
{
	if (soundsgood_is_initialized == 0) return;

	if (nAction & ACB_VOLATILE)
	{
		SekScan(nAction);
		DACScan(nAction, pnMin);
		pia_scan(nAction, pnMin);

		SCAN_VAR(soundsgood_status);
		SCAN_VAR(soundsgood_in_reset);
		SCAN_VAR(dacvalue);
	}
}

INT32 soundsgood_reset_status()
{
	if (soundsgood_is_initialized == 0) return 1;
	return soundsgood_in_reset;
}

INT32 soundsgood_initialized()
{
	return soundsgood_is_initialized;
}
