// FBAlpha "deco pinball bsmt" emulation
#include "burnint.h"
//#include "m6809_intf.h" // in decobsmt.h
#include "bsmt2000.h"
#include "decobsmt.h"

static UINT8 bsmt_latch = 0;
static UINT8 bsmt_reset = 0;
static UINT8 bsmt_comms = 0;

INT32 bsmt_in_reset = 0;

void tattass_sound_cb(UINT16 data)
{
	bsmt_comms = (data & 0xf6) | ((data & 1) << 3) | ((data & 8) >> 3);
}

static void decobsmt_write(UINT16 address, UINT8 data)
{
	if ((address & 0xff00) == 0xa000) {
		bsmt2k_write_reg((address & 0xff) ^ 0xff);
		bsmt2k_write_data((bsmt_latch << 8) | data);
		M6809SetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;
	}

	switch (address)
	{
		case 0x2000:
		case 0x2001: {
			UINT8 r = data ^ bsmt_reset;
			bsmt_reset = data;
			if ((r & 0x80) && (data & 0x80) == 0) {
				bsmt2kResetCpu();
			}
		}
		return;

		case 0x6000:
			bsmt_latch = data;
		return;
	}
}

static UINT8 decobsmt_read(UINT16 address)
{
	switch (address)
	{
		case 0x2002:
		case 0x2003:
			return bsmt_comms;

		case 0x2006:
		case 0x2007:
			return (bsmt2k_read_status()) ? 0x80 : 0;
	}

	return 0;
}

void decobsmt_reset_line(INT32 state)
{
	if (state) M6809Reset();
	bsmt_in_reset = state;
}

void decobsmt_firq_interrupt() // 489 times per second (58hz) - 8.5 times
{
	M6809SetIRQLine(1, CPU_IRQSTATUS_HOLD);
}

static void decobsmt_ready_callback()
{
	M6809SetIRQLine(0, CPU_IRQSTATUS_ACK);
}

void decobsmt_reset()
{
	M6809Open(0);
	M6809Reset();
	M6809Close();

	bsmt2kReset();

	bsmt_latch = 0;
	bsmt_reset = 0;
	bsmt_comms = 0;
	bsmt_in_reset = 0;
}

void decobsmt_scan(INT32 nAction, INT32 *pnMin)
{
	SCAN_VAR(bsmt_latch);
	SCAN_VAR(bsmt_reset);
	SCAN_VAR(bsmt_comms);
	SCAN_VAR(bsmt_in_reset);

	M6809Scan(nAction);

	bsmt2kScan(nAction, pnMin);
}

void decobsmt_new_frame()
{
	M6809NewFrame();

	bsmt2kNewFrame();
}

void decobsmt_update()
{
	bsmt2k_update();
}

void decobsmt_init(UINT8 *m6809_rom, UINT8 *m6809_ram, UINT8 *tmsrom, UINT8 *tmsram, UINT8 *datarom, INT32 datarom_size)
{
	M6809Init(1);
	M6809Open(0);
	M6809MapMemory(m6809_ram,		0x0000, 0x1fff, MAP_RAM);
	M6809MapMemory(m6809_rom + 0x2100,	0x2100, 0xffff, MAP_ROM);
	M6809SetWriteHandler(decobsmt_write);
	M6809SetReadHandler(decobsmt_read);
	M6809Close();

	bsmt2kInit(24000000/4, tmsrom, tmsram, datarom, datarom_size, decobsmt_ready_callback);
}

void decobsmt_exit()
{
	bsmt2kExit();
	M6809Exit();
}
