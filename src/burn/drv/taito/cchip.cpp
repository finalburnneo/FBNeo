// Based on C-Chip emulation by Jonathan Gevaryahu, David Haywood
// Ultra-super double thanks to Caps0ff for dumping the C-Chips

#include "burnint.h"
#include "taito_ic.h"
#include "taito.h"
#include "upd7810_intf.h"
#include "bitswap.h"
#include "m68000_intf.h"

//#define DEBUG_CCHIP

UINT8 cchip_active = 0;
UINT8 *cchip_rom;
UINT8 *cchip_eeprom;

static UINT8 *cchip_ram; // 0x2000, 8 0x400 chunks. separate banking for 68k and uPD
static UINT8 *cchip_updram; // 0x100, internal uPD ram

static INT32 bank;
static INT32 bank68k;
static UINT8 asic_ram[4];
static UINT8 porta, portb, portc, portadc;

void cchip_interrupt()
{
	upd7810SetIRQLine(UPD7810_INTF1, CPU_IRQSTATUS_ACK); // core auto un-acks
}

void cchip_loadports(UINT8 pa, UINT8 pb, UINT8 pc, UINT8 padc)
{
	porta = pa; portb = pb; portc = pc; portadc = padc;
}

INT32 cchip_run(INT32 cyc)
{
	return upd7810Run(cyc);
}

void cchip_reset()
{
	upd7810Reset();
	bank = 0;
	bank68k = 0;
	memset(&asic_ram, 0, 4);
	memset(cchip_ram, 0, 0x2000);
	memset(cchip_updram, 0, 0x100);
	porta = portb = portc = portadc = 0;
}

UINT8 cchip_asic_read(UINT32 offset)
{
	if (offset < 0x200)
		return asic_ram[offset & 3];

	return 0x00;
}

static void cchip_asic_write(UINT32 offset, UINT8 data)
{
	if (offset == 0x200)
	{
		bank = data & 7;
	}
	else
		asic_ram[offset & 3] = data;
}

void cchip_asic_write68k(UINT32 offset, UINT16 data)
{
	if (offset == 0x200)
	{
		bank68k = data & 7;
	}
	else
		asic_ram[offset & 3] = data & 0xff;
}

void cchip_68k_write(UINT16 address, UINT8 data)
{
	cchip_ram[(bank68k * 0x400) + (address & 0x3ff)] = data;
}

UINT8 cchip_68k_read(UINT16 address)
{
	return cchip_ram[(bank68k * 0x400) + (address & 0x3ff)];
}

static UINT8 upd7810_read_port(UINT8 port)
{
	switch (port)
	{
		case UPD7810_PORTA:
			return porta;

		case UPD7810_PORTB:
			return portb;

		case UPD7810_PORTC:
			return portc;
	}

	return 0;
}

static void upd7810_write_port(UINT8 port, UINT8 data)   // not impl yet. (usually just coin counters write back)
{
	switch (port)
	{
		case UPD7810_PORTA:
		return;

		case UPD7810_PORTB:
		return;

		case UPD7810_PORTC:
		return;
	}
}

static UINT8 upd7810_read(UINT16 address)
{
	if (address >= 0x1000 && address <= 0x13ff) {
		return cchip_ram[(bank * 0x400) + (address & 0x3ff)];
	}

	if (address >= 0x1400 && address <= 0x17ff) {
		return cchip_asic_read(address & 0x3ff);
	}

	return 0;
}

static void upd7810_write(UINT16 address, UINT8 data)
{
	if (address >= 0x1000 && address <= 0x13ff) {
		cchip_ram[(bank * 0x400) + (address & 0x3ff)] = data;
		return;
	}

	if (address >= 0x1400 && address <= 0x17ff) {
		cchip_asic_write(address & 0x3ff, data);
		return;
	}
}

#if 0
static void cchip_sync() // probably not needed, saving just incase
{
	INT32 cyc = ((SekTotalCycles() * 12) / 8) - upd7810TotalCycles();
	if (cyc > 0) {
		cchip_run(cyc);
	}
}
#endif

#ifdef DEBUG_CCHIP
void cchip_dumptakedisinhibitorendofframe()
{
	bprintf(0, _T("0x0000: "));
	for (INT32 rc = 0; rc < 16; rc++)
		bprintf(0, _T("%02X, "), cchip_ram[rc]);

	bprintf(0, _T("\n0x0010: "));
	for (INT32 rc = 0; rc < 16; rc++)
		bprintf(0, _T("%02X, "), cchip_ram[rc+0x10]);

	bprintf(0, _T("\n0x0020: "));
	for (INT32 rc = 0; rc < 16; rc++)
		bprintf(0, _T("%02X, "), cchip_ram[rc+0x20]);
	bprintf(0, _T("\n\n"));
}
#endif

static UINT8 cchip_an0_read() {	return BIT(portadc, 0); }
static UINT8 cchip_an1_read() {	return BIT(portadc, 1); }
static UINT8 cchip_an2_read() {	return BIT(portadc, 2); }
static UINT8 cchip_an3_read() {	return BIT(portadc, 3); }
static UINT8 cchip_an4_read() {	return BIT(portadc, 4); }
static UINT8 cchip_an5_read() {	return BIT(portadc, 5); }
static UINT8 cchip_an6_read() {	return BIT(portadc, 6); }
static UINT8 cchip_an7_read() {	return BIT(portadc, 7); }

void cchip_init()
{
	cchip_ram = (UINT8 *)BurnMalloc(0x2000);
	cchip_updram = (UINT8 *)BurnMalloc(0x100);

	upd7810Init(NULL);
	upd7810MapMemory(cchip_rom,     0x0000, 0x0fff, MAP_ROM); // c-chip bios
	upd7810MapMemory(cchip_eeprom,  0x2000, 0x3fff, MAP_ROM); // pre-game eeprom
	upd7810MapMemory(cchip_updram,  0xff00, 0xffff, MAP_RAM); // internal uPD ram

	upd7810SetReadPortHandler(upd7810_read_port);
	upd7810SetWritePortHandler(upd7810_write_port);
	upd7810SetReadHandler(upd7810_read);
	upd7810SetWriteHandler(upd7810_write);

	upd7810SetAnfunc(0, cchip_an0_read);
	upd7810SetAnfunc(1, cchip_an1_read);
	upd7810SetAnfunc(2, cchip_an2_read);
	upd7810SetAnfunc(3, cchip_an3_read);
	upd7810SetAnfunc(4, cchip_an4_read);
	upd7810SetAnfunc(5, cchip_an5_read);
	upd7810SetAnfunc(6, cchip_an6_read);
	upd7810SetAnfunc(7, cchip_an7_read);

	cchip_active = 1;

	cchip_reset();

#ifdef DEBUG_CCHIP
	bprintf(0, _T("\n-- c-chippy debug --\ncc eeprom: \n"));
	for (INT32 rc = 0; rc < 16; rc++)
		bprintf(0, _T("%X, "), cchip_eeprom[rc]);

	bprintf(0, _T("\ncc biosrom: \n"));
	for (INT32 rc = 0; rc < 16; rc++)
		bprintf(0, _T("%X, "), cchip_rom[rc]);
	bprintf(0, _T("\n"));
#endif
}

void cchip_exit()
{
	upd7810Exit();
	BurnFree(cchip_ram);
	BurnFree(cchip_updram);
	cchip_active = 0;
}

INT32 cchip_scan(INT32 nAction)
{
	if (nAction & ACB_VOLATILE)	{
		upd7810Scan(nAction);

		ScanVar(cchip_updram,   0x100,  "cchip_updram");
		ScanVar(cchip_ram,      0x2000, "cchip_bankram");

		SCAN_VAR(bank);
		SCAN_VAR(bank68k);
		SCAN_VAR(asic_ram);

		SCAN_VAR(porta);
		SCAN_VAR(portb);
		SCAN_VAR(portc);
		SCAN_VAR(portadc);
	}

	return 0;
}

