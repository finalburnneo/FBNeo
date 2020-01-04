// FBAlpha BSMT2000 emulation, based on code written by Aaron Giles
#include "burnint.h"
#include "tms32010.h"
#include "dac.h"

static UINT32 bsmt2k_clock = 0;

static INT32 write_pending = 0;
static UINT16 write_data = 0;
static UINT16 register_select = 0;
static UINT16 rom_address = 0;
static UINT8 rom_bank = 0;
static UINT8 *datarom = NULL;
static INT32 datarom_len = 0;
static void (*ready_callback)() = NULL;
static UINT16 data_left = 0;
static UINT16 data_right = 0;

void bsmt2k_write_reg(UINT16 data)
{
	register_select = data;
}

void bsmt2k_write_data(UINT16 data)
{
	write_data = data;
	write_pending = 1;
}

INT32 bsmt2k_read_status()
{
	return write_pending ? 0 : 1;
}

static INT32 BSMTSyncDAC()
{
	return (INT32)(float)(nBurnSoundLen * (tms32010TotalCycles() / (bsmt2k_clock / (nBurnFPS / 100.0000))));
}

void bsmt2k_update()
{
	DACUpdate(pBurnSoundOut, nBurnSoundLen);
}

static void update_stream()
{
	DACWrite16Stereo(0, data_left, data_right);
}

static void bsmt2k_write_port(INT32 port, UINT16 data)
{
	switch (port)
	{
		case 0:
			rom_address = data;
		return;

		case 1:
			rom_bank = data;
		return;

		case 3:
			data_left = data;
			update_stream();
		return;

		case 7:
			data_right = data;
			update_stream();
		return;
	}
}

static UINT16 bsmt2k_read_port(INT32 port)
{
	switch (port)
	{
		case 0:
			return register_select;

		case 1:
			write_pending = 0;
			if (ready_callback)
				ready_callback();
			return write_data;

		case 2: {
			INT32 addr = (rom_bank << 16) + rom_address;
			if (addr >= datarom_len) return 0;
			return (INT16)(datarom[addr] << 8);
		}

		case TMS32010_BIO:
			return (write_pending) ? 1 : 0;
	}

	return 0;
}

void bsmt2kReset()
{
	tms32010_reset();

	DACReset();

	write_pending = 0;
	write_data = 0;
	register_select = 0;
	rom_address = 0;
	rom_bank = 0;
	data_left = 0;
	data_right = 0;
}

void bsmt2kResetCpu()
{
	tms32010_reset();
}

void bsmt2kExit()
{
	tms32010_exit();

	DACExit();
}

void bsmt2kScan(INT32 nAction, INT32 *pnMin)
{
	tms32010_scan(nAction);

	DACScan(nAction, pnMin);

	SCAN_VAR(write_pending);
	SCAN_VAR(write_data);
	SCAN_VAR(register_select);
	SCAN_VAR(rom_address);
	SCAN_VAR(rom_bank);
	SCAN_VAR(data_left);
	SCAN_VAR(data_right);
}

void bsmt2kInit(INT32 clock, UINT8 *tmsrom, UINT8 *tmsram, UINT8 *data, INT32 size, void (*cb)())
{
	bsmt2k_clock = clock;

	tms32010_rom = (UINT16*)tmsrom; // 0x1000 words (0x2000 bytes)
	tms32010_ram = (UINT16*)tmsram; // 0x100 bytes

	datarom = data;
	datarom_len = size;

	ready_callback = cb;

	tms32010_init();
	tms32010_set_write_port_handler(bsmt2k_write_port);
	tms32010_set_read_port_handler(bsmt2k_read_port);

	DACInit(0, 0, 0, BSMTSyncDAC);
	DACSetRoute(0, 0.60, BURN_SND_ROUTE_BOTH);
	DACStereoMode(0);
}

void bsmt2kNewFrame()
{
	tms32010NewFrame();
}
