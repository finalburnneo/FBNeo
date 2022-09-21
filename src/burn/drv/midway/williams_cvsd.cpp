// Williams CVSD sound emulation
// Based on MAME sources by Aaron Giles

#include "burnint.h"
#include "m6809_intf.h"
#include "burn_ym2151.h"
#include "dac.h"
#include "hc55516.h"
#include "6821pia.h"

static UINT8 *rom;
static UINT8 *ram;

static UINT8 protram[0x80];

static INT32 talkback;
static INT32 bankdata;
static INT32 bankpos;
static INT32 sound_in_reset;
static INT32 protection_start;
static INT32 protection_end;
static INT32 cvsd_small; // protection only in bank0
static INT32 ym_inreset;

static void bankswitch(INT32 data)
{
	bankdata = data;

	INT32 bank = data & 3;
	if (bank == 3) bank = 0;
	bankpos = 0x10000 + 0x8000 * ((data >> 2) & 3) + (0x20000 * bank);
}

static void cvsd_write(UINT16 address, UINT8 data)
{
	if (address >= 0 && address <= 0x1fff) {
		ram[address & 0x7ff] = data;
		return;
	}

	if (address >= protection_start && address <= protection_end) {
		if (bankpos == 0x10000) {
			protram[address - protection_start] = data;
			//bprintf(0, _T("prot write %x (%x): %x\t@bank %x\n"), address, address - protection_start, data, bankpos);
		} else {
			bprintf(0, _T("attempt to write to prot ram in wrong bank (%x)\n"), bankpos);
		}
		return;
	}

	if ((address & 0xe000) == 0x2000) {
		BurnYM2151Write(address & 1, data);
		return;
	}

	if ((address & 0xe000) == 0x4000) {
		pia_write(0, address & 3, data);
		return;
	}

	if ((address & 0xf800) == 0x6000) {
		hc55516_clock_w(0);
		hc55516_digit_w(data & 1);
		return;
	}

	if ((address & 0xf800) == 0x6800) {
		hc55516_clock_w(1);
		return;
	}

	if ((address & 0xf800) == 0x7800) {
		bankswitch(data & 0xf);
		if (data & 0xf0) bprintf(0, _T("bank extra bits: %x\n"), data);
		return;
	}
	bprintf(0, _T("cvsd_wb %x  %x\n"), address, data);
}

static UINT8 cvsd_read(UINT16 address)
{
	if (address >= 0 && address <= 0x1fff) {
		return ram[address & 0x7ff];
	}

	if (address >= 0x8000 && address <= 0xffff) {
		if (address >= protection_start && address <= protection_end) {
			if (bankpos == 0x10000) {
				//bprintf(0, _T("read protram @ %x (%x): %x\n"), address, address-protection_start,protram[address - protection_start]);
				return protram[address - protection_start];
			}
		}
		return rom[bankpos + (address & 0x7fff)];
	}

	if ((address & 0xe000) == 0x2000) {
		return BurnYM2151Read();
	}

	if ((address & 0xe000) == 0x4000) {
		return pia_read(0, address & 3);
	}

	bprintf(0, _T("cvsd_rb %x \n"), address);

	return 0xff;
}

static void out_a_write(UINT16 offset, UINT8 data)
{
	DACSignedWrite(0, data);
}

static void out_b_write(UINT16 offset, UINT8 data)
{
	talkback = data;
}

static void irq_a_write(INT32 state)
{
	M6809SetIRQLine(1, state ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static void irq_b_write(INT32 state)
{
	M6809SetIRQLine(0x20, state ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static void out_ca2_write(UINT16 offset, UINT8 data)
{
	if (data == 0 && ym_inreset == 0) {
		BurnYM2151Reset();
		bprintf(0, _T("cvsd.out.ca2 reset ym2151() %x\n"), data);
	}
	ym_inreset = !data;
}

static pia6821_interface pia_intf = {
	NULL, NULL,
	NULL, NULL, NULL, NULL,
	out_a_write, out_b_write, out_ca2_write, NULL,
	irq_a_write, irq_b_write
};

static void YM2151IrqHandler(INT32 state)
{
	pia_set_input_ca1(0, !state);
}

void williams_cvsd_write(UINT16 data)
{
	pia_set_input_b(0, data & 0xff);
	pia_set_input_cb1(0, data & 0x100);
	pia_set_input_cb2(0, data & 0x200);
}

void williams_cvsd_reset_write(UINT16 state)
{
	// going high halts the CPU
	if (state)
	{
		bankswitch(0);
		M6809Reset();
		sound_in_reset = 1;
	}
	else
		sound_in_reset = 0;
}

void williams_cvsd_reset()
{
	memset (ram, 0, 0x800);

	// reset protram
	memcpy(&protram[0], &rom[0x10000 + (protection_start & 0x7fff)], protection_end - protection_start + 1);

	M6809Open(0);
	bankswitch(0);
	M6809Reset();
	M6809Close();

	pia_reset();

	BurnYM2151Reset();
	DACReset();
	hc55516_reset();

	pia_set_input_ca1(0, 1);

	talkback = 0;
	sound_in_reset = 0;
	ym_inreset = 0;
}

void williams_cvsd_init(UINT8 *prgrom, INT32 prot_start, INT32 prot_end, INT32 small)
{
	rom = prgrom;
	ram = (UINT8*)BurnMalloc(0x800);
	cvsd_small = small;

	M6809Init(0);
	M6809Open(0);
	bankswitch(0);
	M6809SetWriteHandler(cvsd_write);
	M6809SetReadHandler(cvsd_read);
	M6809Close();

	pia_init();
	pia_config(0, PIA_STANDARD_ORDERING, &pia_intf);

	BurnYM2151InitBuffered(3579545, 1, NULL, 0);
	BurnYM2151SetIrqHandler(&YM2151IrqHandler);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.10, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.10, BURN_SND_ROUTE_RIGHT);

	BurnTimerAttachM6809(2000000);

	hc55516_init(M6809TotalCycles, 2000000);
	hc55516_volume(0.35);

	DACInit(0, 0, 1, M6809TotalCycles, 2000000);
	DACSetRoute(0, 0.25, BURN_SND_ROUTE_BOTH);
	DACDCBlock(1);

	protection_start = prot_start;
	protection_end = prot_end;
}

void williams_cvsd_exit()
{
	M6809Exit();
	BurnYM2151Exit();
	DACExit();
	hc55516_exit();
	pia_exit();

	BurnFree (ram);
}

INT32 williams_cvsd_in_reset()
{
	return sound_in_reset;
}

void williams_cvsd_update(INT16 *stream, INT32 length)
{
	BurnYM2151Render(stream, length);
	hc55516_update(stream, length);
	DACUpdate(stream, length);
}

INT32 williams_cvsd_scan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = ram;
		ba.nLen	  = 0x800;
		ba.szName = "Sound Ram";
		BurnAcb(&ba);

		memset(&ba, 0, sizeof(ba));
		ba.Data	  = &protram[0];
		ba.nLen	  = 0x40;
		ba.szName = "Sound Ram protection";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		M6809Scan(nAction);
		pia_scan(nAction, pnMin);
		BurnYM2151Scan(nAction, pnMin);
		hc55516_scan(nAction, pnMin);
		DACScan(nAction, pnMin);

		SCAN_VAR(talkback);
		SCAN_VAR(bankdata);
		SCAN_VAR(bankpos);
		SCAN_VAR(sound_in_reset);
		SCAN_VAR(ym_inreset);
	}

	if (nAction & ACB_WRITE)
	{
		M6809Open(0);
		bankswitch(bankdata);
		M6809Close();
	}

	return 0;
}
