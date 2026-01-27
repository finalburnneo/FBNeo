// FB Neo Alpha Denshi 8201 MCU device
// Based on MAME device by hap

#include "burnint.h"
#include "hmcs40.h"

static INT32 bus_dir;
static UINT16 mcu_address;
static UINT16 mcu_d;
static UINT8 mcu_r[4];

static UINT8 *ram;

static void write_ram(); // fwd

void Alpha8201SetBusDir(INT32 state)
{
	bus_dir = (state) ? 1 : 0;
	write_ram();
}

void Alpha8201Start(INT32 state)
{
	state = (state) ? 1 : 0;
	hmcs40SetIRQLine(0, state);
}

UINT8 Alpha8201ReadRam(INT32 address)
{
	return ram[address & 0x3ff];
}

void Alpha8201WriteRam(INT32 address, UINT8 data)
{
	ram[address & 0x3ff] = data;
}

static UINT8 mcu_data_read(INT32 address)
{
	UINT8 ret = 0;

	if (bus_dir && ~mcu_d & 4) {
		ret = ram[mcu_address];
	}

	if (address == 0) {
		ret >>= 4;
	}

	return ret & 0xf;
}

static void write_ram()
{
	if (bus_dir && (mcu_d & 0xc) == 0xc) {
		ram[mcu_address] = (mcu_r[0] << 4) | (mcu_r[1] & 0xf);
	}
}

static void update_address()
{
	mcu_address = ((mcu_d << 8) & 0x300) | (mcu_r[2] << 4) | (mcu_r[3] & 0x0f);
	write_ram();
}

static void mcu_data_write(INT32 address, UINT8 data)
{
	mcu_r[address & 3] = data & 0xf;
	update_address();
}

static void mcu_d_write(INT32 address, UINT16 data)
{
	mcu_d = data;
	update_address();
}


void Alpha8201Init(UINT8 *rom)
{
	hmcs40_hd44801Init(rom);
	hmcs40Open(0);

	hmcs40SetReadR(0, mcu_data_read);
	hmcs40SetReadR(1, mcu_data_read);
	hmcs40SetWriteR(0, mcu_data_write);
	hmcs40SetWriteR(1, mcu_data_write);
	hmcs40SetWriteR(2, mcu_data_write);
	hmcs40SetWriteR(3, mcu_data_write);
	hmcs40SetWriteD(mcu_d_write);

	hmcs40Close();

	ram = (UINT8*)BurnMalloc(0x400);
}

void Alpha8201Exit()
{
	BurnFree(ram);
	hmcs40Exit();
}

void Alpha8201Reset()
{
    memset(ram, 0, 0x400);
	bus_dir = 0;
	mcu_address = 0;
	mcu_d = 0;
	mcu_r[0] = mcu_r[1] = mcu_r[2] = mcu_r[3] = 0;
	hmcs40Reset();
	hmcs40SetIRQLine(0, 0);
}

INT32 Alpha8201Run(INT32 cycles)
{
	return hmcs40Run(cycles);
}

INT32 Alpha8201Idle(INT32 cycles)
{
	return hmcs40Idle(cycles);
}

void Alpha8201NewFrame()
{
	hmcs40NewFrame();
}

INT32 Alpha8201TotalCycles()
{
	return hmcs40TotalCycles();
}

INT32 Alpha8201Scan(INT32 nAction, INT32 *pnMin)
{
	hmcs40Scan(nAction);

	SCAN_VAR(bus_dir);
	SCAN_VAR(mcu_address);
	SCAN_VAR(mcu_d);
	SCAN_VAR(mcu_r);

	ScanVar(ram, 0x400, "Alpha8201RAM");

	return 0;
}
