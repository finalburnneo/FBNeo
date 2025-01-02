#include "burnint.h"
#include "i2ceeprom.h"

// i2c eeprom / e2prom - dink 2025

struct i2c_device_struct {
	int id;
	int size;
	int pagemask;
};

static i2c_device_struct i2c_devices[] = {
	{ I2C_NONE,     0x00,   0x0 },
	{ I2C_24C01,	0x80,	0x4-1 },
	{ I2C_X24C01,	0x80,	0x4-1 },
	{ I2C_X24C02,	0x100,	0x4-1 },
	{ I2C_24C02,	0x100,	0x4-1 },
	{ I2C_24C04,	0x200,	0x10-1 },
	{ I2C_24C08,	0x400,	0x10-1 },
	{ I2C_24C16,	0x800,	0x10-1 },
	{ I2C_24C32,	0x1000,	0x20-1 },
	{ I2C_24C64,	0x2000,	0x20-1 },
	{ I2C_24C128,	0x4000,	0x40-1 },
	{ I2C_24C256,	0x8000,	0x40-1 },
};

enum {
	I2C_STOP = 0,
	I2C_DEVICEID,
	I2C_ADDR,
	I2C_ADDR_EXT,
	I2C_WRITE,
	I2C_READ
};

// config
static UINT16 i2c_device = 0;
static UINT16 i2c_mask;
static UINT16 i2c_page_mask;
static bool uses_bus = false;

// e2prom internals
static UINT8 *i2c_memory;
static int i2c_mode;
static UINT16 i2c_address;
static UINT8 i2c_shifter;
static INT8 i2c_bit_count;
static UINT8 i2c_selected_device;
static bool i2c_sda, i2c_scl;
static bool i2c_sda_read;

// bus interfacing
static UINT8 sda_bit, scl_bit, sda_read_bit;
static UINT8 bus_buffer[2];

#define bus_debug 0
#define device_debug 0

void i2c_write_bus16(UINT32 data) {
	const UINT8 scl = (data >> scl_bit) & 1;
	const UINT8 sda = (data >> sda_bit) & 1;
	if (bus_debug) bprintf(0, _T("wb16  %x, scl/sda:  %x  %x\n"), data, scl, sda);
	i2c_write_bit(sda, scl);
}

void i2c_write_bus8(UINT32 address, UINT8 data) {
	if (bus_debug) bprintf(0, _T("wb8  %x  %x\n"), address, data);
	bus_buffer[address & 1] = data;
	i2c_write_bus16(bus_buffer[0] << 8 | bus_buffer[1]);
}

UINT32 i2c_read_bus16(UINT32 address) {
	const UINT32 data = i2c_read() << sda_read_bit;
	if (bus_debug) bprintf(0, _T("rb16  %x, sda %x\n"), address, data);
	return data;
}

UINT32 i2c_read_bus8(UINT32 address) {
	const UINT32 data = i2c_read_bus16(address) >> ((~address & 1) << 3);
	if (bus_debug) bprintf(0, _T("rb8  %x  %x\n"), address, data);
	return data;
}

void i2c_init(UINT8 device, UINT8 sda, UINT8 scl, UINT8 sda_read) {
	i2c_init(device);

	uses_bus = true;

	sda_bit = sda;
	scl_bit = scl;
	sda_read_bit = sda_read;
}

void i2c_init(UINT8 device) {
	if (device == I2C_NONE) return;

	i2c_device = device;
	i2c_mask = i2c_devices[i2c_device].size - 1;
	i2c_page_mask = i2c_devices[i2c_device].pagemask;

	i2c_memory = (UINT8*)BurnMalloc(i2c_devices[i2c_device].size);

	memset(i2c_memory, 0xff, i2c_devices[i2c_device].size);
}

void i2c_exit() {
	BurnFree(i2c_memory);

	uses_bus = false;

	i2c_device = I2C_NONE;
}

void i2c_scan(INT32 nAction, INT32 *pnMin) {

	if (i2c_device == I2C_NONE) return;

	if (nAction & ACB_VOLATILE) {
		SCAN_VAR(i2c_mode);
		SCAN_VAR(i2c_address);
		SCAN_VAR(i2c_shifter);
		SCAN_VAR(i2c_bit_count);
		SCAN_VAR(i2c_selected_device);
		SCAN_VAR(i2c_sda);
		SCAN_VAR(i2c_scl);
		SCAN_VAR(i2c_sda_read);

		if (uses_bus) {
			SCAN_VAR(bus_buffer);
		}
	}

	if (nAction & ACB_NVRAM) {
		ScanVar(i2c_memory, i2c_devices[i2c_device].size, "NV RAM");
	}
}

void i2c_reset() {

	i2c_mode = I2C_STOP;
	i2c_address = 0;
	i2c_shifter = 0;
	i2c_bit_count = 0;
	i2c_selected_device = 0;

	i2c_sda = i2c_scl = 1;
	i2c_sda_read = 1;

	bus_buffer[0] = bus_buffer[1] = 0xff;
}

bool i2c_read() {
	return (i2c_mode == I2C_STOP) ? i2c_sda : i2c_sda_read;
}

void i2c_write_bit(bool sda, bool scl) {

	if (sda != i2c_sda && (scl && i2c_scl)) {
		// start / stop condition: when sda toggles while scl remains high
		// sda, high to low: start
		// sda, low to high: stop
		i2c_mode = (sda) ? I2C_STOP : I2C_DEVICEID;
		if (device_debug)
			bprintf(0, _T("%S condition.\n"), (i2c_mode == I2C_DEVICEID) ? "START":"STOP");
		i2c_bit_count = 0;
		i2c_shifter = 0;
		i2c_sda = sda;
		i2c_scl = scl;
		return;
	}

	if (!i2c_scl && scl && i2c_mode != I2C_STOP) {

		if (i2c_mode != I2C_READ) {
			// when not in READ mode, we're shifting bits into the shifter
			if (i2c_bit_count < 8) {
				i2c_shifter = (i2c_shifter << 1) | sda;
			}
			i2c_bit_count++;
		}

		if (i2c_bit_count == 9 || i2c_mode == I2C_READ) {

			if (i2c_bit_count == 9) {
				// when a full byte is received, respond with ACK
				i2c_sda_read = 0;   // ACK
				i2c_bit_count = 0;
			}

			switch (i2c_mode) {
				case I2C_DEVICEID: {
					if (device_debug)
						bprintf(0, _T("deviceid: %x\n"), i2c_shifter);

					if (i2c_device == I2C_X24C01) { // special handling for X24c01!
						i2c_address = (i2c_shifter & 0xfe) >> 1;
						if (device_debug)
							bprintf(0, _T("x24c01 address: %x\n"), i2c_address);
						i2c_mode = (i2c_shifter & 1) ? I2C_READ : I2C_WRITE;
					} else
					if ((i2c_shifter & 0xf0) == 0xa0) {
						i2c_selected_device = i2c_shifter;
						i2c_mode = (i2c_shifter & 1) ? I2C_READ : I2C_ADDR;
					} else {
						// not our device on the i2c bus, try again
						i2c_mode = I2C_DEVICEID;
					}
				}
				break;

				case I2C_ADDR: {
					i2c_address = i2c_shifter;
					if (i2c_devices[i2c_device].size >= 0x1000) {
						i2c_mode = I2C_ADDR_EXT;
					} else {
						i2c_address |= (i2c_selected_device & 0xe) << 7;
						i2c_mode = I2C_WRITE;
					}
				}
				break;

				case I2C_ADDR_EXT: {
					i2c_address = (i2c_address << 8) | i2c_shifter;
					i2c_mode = I2C_WRITE;
				}
				break;

				case I2C_WRITE: {
					i2c_memory[i2c_address & i2c_mask] = i2c_shifter;
					i2c_address = ((i2c_address + 1) & i2c_page_mask) | (i2c_address & ~i2c_page_mask);
				}
				break;

				case I2C_READ: {
					if (i2c_bit_count == 0) {
						i2c_shifter = i2c_memory[i2c_address & i2c_mask];
						i2c_address++;
					}

					i2c_sda_read = (i2c_shifter >> 7) & 1;
					i2c_shifter = i2c_shifter << 1;

					i2c_bit_count++;

					if (i2c_bit_count == 9) {
						// after the last bit is read, respond with ACK
						i2c_sda_read = 0;
						i2c_bit_count = 0;
					}
				}
				break;
			}
		}
	}

	i2c_sda = sda;
	i2c_scl = scl;
}
