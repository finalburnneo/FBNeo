// Sega Model 1 I/O boards (also on the original Model 2)
//  837-8950: z80, 315-5338A, msm6253, 93c45
//  837-10907 (model1io2): tmpz84c015, 315-5338A, msm6253, 93c45

#include "burnint.h"
#include "z80_intf.h"
#include "z80ctc.h"
#include "z80daisy.h"
#include "eeprom.h"
#include "model1_io.h"

static UINT8 *m1io_dpram;
static UINT8 *m1io_inputs;
static UINT8 *m1io_dips;
static UINT8 *m1io_analog;
static INT32 m1io_board2;
static INT32 (*m1io_port_in_hook)(INT32 port);
static void (*m1io_port_out_hook)(INT32 port, UINT8 data);

static UINT8 io_port_value[7];
static UINT8 io_port_config;
static UINT8 io_serial_output;
static UINT16 io_address;
static UINT8 io_cmd;
static UINT8 io_secondary;
static UINT8 adc_shift;
static UINT8 io_outputs;
static UINT8 io2_lcd_data;
static INT32 io2_fpga_counter;
static UINT8 (*m1io_fpga_read)(INT32 offset);


static UINT8 io2_port_in(INT32 port)
{
	switch (port) {
		case 0: return m1io_inputs[0];
		case 1: return m1io_inputs[1];
		case 2: return m1io_inputs[2];
		case 4: return 0xff;	// cn6
	}

	return 0xff;
}


static void io2_port_out(INT32 port, UINT8 data)
{
	switch (port) {
		case 3:
			io_outputs = data;	// lamps, coin counters
			return;

		case 4:
			io2_lcd_data = data;
			return;

		case 5:
			// eeprom pe, di, clk, cs, lcd control
			EEPROMWriteBit((data >> 6) & 1);
			EEPROMSetCSLine((data & 0x10) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
			EEPROMSetClockLine((data & 0x20) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
			return;

		case 6:
			// watchdog, control panel switch, comm error led
			io_secondary = (data >> 6) & 1;
			return;
	}
}

static UINT8 io_port_in(INT32 port)
{
	if (m1io_port_in_hook) {
		INT32 r = m1io_port_in_hook(port);
		if (r >= 0) return r;
	}

	if (m1io_board2) return io2_port_in(port);

	switch (port) {
		case 1: return io_secondary ? m1io_dips[0] : m1io_inputs[0];
		case 2: return io_secondary ? m1io_dips[1] : m1io_inputs[1];
		case 3: return io_secondary ? m1io_dips[2] : m1io_inputs[2];
		case 4: return 0xff;	// drive board
		case 6: return (EEPROMRead() ? 0x80 : 0) | 0x70 | 0x0f;
	}

	return 0xff;
}

static void io_port_out(INT32 port, UINT8 data)
{
	if (m1io_port_out_hook) m1io_port_out_hook(port, data);

	if (m1io_board2) {
		io2_port_out(port, data);
		return;
	}

	switch (port) {
		case 0:
			// eeprom clk, cs, di, control switch
			EEPROMWriteBit((data >> 5) & 1);
			EEPROMSetCSLine((data & 0x40) ? EEPROM_CLEAR_LINE : EEPROM_ASSERT_LINE);
			EEPROMSetClockLine((data & 0x80) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
			io_secondary = data & 1;
			return;

		case 5:
			io_outputs = data;	// lamps, coin counters
			return;
	}
}

static UINT8 io_5338a_read(INT32 offset)
{
	switch (offset) {
		case 0x00: case 0x01: case 0x02: case 0x03:
		case 0x04: case 0x05: case 0x06:
			if (io_port_config & (1 << offset))
				return io_port_in(offset);
			return io_port_value[offset];

		case 0x08: return io_port_config;
		case 0x0a: return io_serial_output;
		case 0x0b: return io_cmd;
		case 0x0c: return m1io_dpram[io_address & 0x7ff];
		case 0x0d: return 0x08;
	}

	return 0xff;
}

static void io_5338a_write(INT32 offset, UINT8 data)
{
	switch (offset) {
		case 0x00: case 0x01: case 0x02: case 0x03:
		case 0x04: case 0x05: case 0x06:
			io_port_value[offset] = data;
			io_port_out(offset, data);
			return;

		case 0x08: {
			UINT8 changed = data ^ io_port_config;
			for (INT32 i = 0; i < 7; i++) {
				if ((changed & (1 << i)) && (data & (1 << i)) == 0)
					io_port_out(i, io_port_value[i]);
			}
			io_port_config = data;
			return;
		}

		case 0x09:
			io_cmd = data;
			switch (data) {
				case 0x00: io_address = (io_address & 0xff00) | io_serial_output; break;
				case 0x01: io_address = (io_address & 0x00ff) | (io_serial_output << 8); break;
				case 0x07: m1io_dpram[io_address & 0x7ff] = io_serial_output; break;
				case 0x70: case 0x71: case 0x72: case 0x73:
				case 0x74: case 0x75: case 0x76: case 0x77:
					m1io_dpram[data & 7] = io_serial_output;
					break;
			}
			return;

		case 0x0a:
			io_serial_output = data;
			return;
	}
}

static void __fastcall io_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfff0) == 0x8000) {
		io_5338a_write(address & 0x0f, data);
		return;
	}

	if ((address & 0xfffc) == 0xc000) {
		adc_shift = m1io_analog[(address & 3) | (io_secondary ? 4 : 0)];
		return;
	}
}

static UINT8 __fastcall io_read(UINT16 address)
{
	if ((address & 0xfff0) == 0x8000) {
		return io_5338a_read(address & 0x0f);
	}

	if ((address & 0xfffc) == 0xc000) {
		UINT8 msb = adc_shift >> 7;
		adc_shift <<= 1;
		return msb;
	}

	return 0;
}

static void __fastcall io2_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfff0) == 0x8000) {
		io_5338a_write(address & 0x0f, data);
		return;
	}

	if ((address & 0xfff8) == 0x8200) {
		adc_shift = m1io_analog[(address & 3) | (io_secondary ? 4 : 0)];
		return;
	}

	if ((address & 0xfff0) == 0x8100) {
		io2_fpga_counter++;	// virtua cop uploads its lightgun fpga here
		return;
	}
}

static UINT8 __fastcall io2_read(UINT16 address)
{
	if ((address & 0xfff0) == 0x8000) {
		return io_5338a_read(address & 0x0f);
	}

	if ((address & 0xfff8) == 0x8200) {
		UINT8 msb = adc_shift >> 7;
		adc_shift <<= 1;
		return msb;
	}

	switch (address) {
		case 0x8040:
			// board buttons (released), rom_emu and mode jumpers off, eeprom do
			return 0xbf | (EEPROMRead() ? 0x40 : 0);

		case 0x8080:
			return m1io_dips[0];
	}

	if ((address & 0xfff0) == 0x8100) {
		if (io2_fpga_counter < 0x1400) return 0x80;	// upload not finished
		return m1io_fpga_read ? m1io_fpga_read(address & 0x0f) : 0xff;
	}

	return 0xff;
}

// tmpz84c015 on-chip peripherals
static void __fastcall io2_write_port(UINT16 port, UINT8 data)
{
	port &= 0xff;

	if ((port & 0xfc) == 0x10) {
		z80ctc_write(port & 3, data);
		return;
	}

	// sio, pio, watchdog and daisy chain setup are not needed
}

static UINT8 __fastcall io2_read_port(UINT16 port)
{
	port &= 0xff;

	if ((port & 0xfc) == 0x10) {
		return z80ctc_read(port & 3);
	}

	switch (port) {
		case 0x19:
		case 0x1b:
			return 0x2c;	// sio rr0: tx buffer empty, dcd, cts

		case 0x1c:
			return m1io_dips[1];	// pio a, dsw2

		case 0x1e:
			return m1io_dips[2];	// pio b, dsw3
	}

	return 0xff;
}

static void io2_ctc_irq(INT32 state)
{
	ZetSetIRQLine(0, state ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}


void M1IOInit(INT32 board2, UINT8 *rom, UINT8 *ram, UINT8 *dpram, UINT8 *inputs, UINT8 *dips, UINT8 *analog)
{
	m1io_board2 = board2;
	m1io_dpram = dpram;
	m1io_inputs = inputs;
	m1io_dips = dips;
	m1io_analog = analog;
	m1io_port_in_hook = NULL;
	m1io_port_out_hook = NULL;
	m1io_fpga_read = NULL;

	ZetInit(0);
	ZetOpen(0);
	if (board2) {
		ZetDaisyInit(Z80_CTC, 0);
		ZetMapMemory(rom,			0x0000, 0x7fff, MAP_ROM);
		ZetMapMemory(ram,			0xe000, 0xffff, MAP_RAM);
		ZetSetWriteHandler(io2_write);
		ZetSetReadHandler(io2_read);
		ZetSetOutHandler(io2_write_port);
		ZetSetInHandler(io2_read_port);
		z80ctc_init(M1IO2_CLOCK, M1IO2_CLOCK, 0, io2_ctc_irq, NULL, NULL, NULL);
	} else {
		ZetMapMemory(rom,			0x0000, 0x3fff, MAP_ROM);
		ZetMapMemory(ram,			0x4000, 0x5fff, MAP_RAM);
		ZetSetWriteHandler(io_write);
		ZetSetReadHandler(io_read);
	}
	ZetClose();
}

void M1IOSetHooks(INT32 (*port_in)(INT32), void (*port_out)(INT32, UINT8))
{
	m1io_port_in_hook = port_in;
	m1io_port_out_hook = port_out;
}

void M1IOSetFpgaRead(UINT8 (*read)(INT32 offset))
{
	m1io_fpga_read = read;
}

void M1IOReset()
{
	memset(io_port_value, 0xff, sizeof(io_port_value));
	io_port_config = 0;
	io_serial_output = 0;
	io_address = 0;
	io_cmd = 0;
	io_secondary = 0;
	adc_shift = 0;
	io_outputs = 0;
	io2_lcd_data = 0;
	io2_fpga_counter = 0;
}

UINT8 M1IOGetOutputs()
{
	return io_outputs;
}

INT32 M1IOGetSecondary()
{
	return io_secondary;
}

void M1IOScan(INT32 nAction)
{
	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(io_port_value);
		SCAN_VAR(io_port_config);
		SCAN_VAR(io_serial_output);
		SCAN_VAR(io_address);
		SCAN_VAR(io_cmd);
		SCAN_VAR(io_secondary);
		SCAN_VAR(adc_shift);
		SCAN_VAR(io_outputs);
		SCAN_VAR(io2_lcd_data);
		SCAN_VAR(io2_fpga_counter);
	}
}
