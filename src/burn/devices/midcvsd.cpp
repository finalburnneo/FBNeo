#include "burnint.h"
#include "m6809_intf.h"
#include "burn_ym2151.h"
#include "6821pia.h"
#include "dac.h"
#include "hc55516.h"

static UINT8 *mainrom;
static INT32 pia_select;
static INT32 dac_select;
static INT32 cpu_select;

static UINT8 audio_talkback;
static INT32 cpu_bank;
static INT32 cvsd_is_initialized = 0;
static INT32 cvsd_in_reset;
static INT32 ym_reset_state;

static void (*cb2_callback)(INT32 state) = NULL;
static INT32 s11_bg_map = 0; // williams system 11 bg boards: cvsd clock set / digit write are swapped
static INT32 dac_signed = 0;

static void bankswitch(INT32 data)
{
	INT32 bank = (0x8000 * ((data >> 2) & 3) + 0x20000 * (data & 3));

	cpu_bank = data;

	M6809MapMemory(mainrom + bank,		0x8000, 0xffff, MAP_ROM);
}

static void cvsd_write(UINT16 address, UINT8 data)
{
	if ((address & 0xe000) == 0x2000) {
		BurnYM2151Write(address & 1, data);
		return;
	}

	if ((address & 0xe000) == 0x4000) {
		pia_write(pia_select, address & 3, data);
		return;
	}

	if ((address & 0xf800) == 0x6000) {
		if (s11_bg_map) {
			hc55516_clock_w(1);
		} else {
			hc55516_digit_w(data);
			hc55516_clock_w(0);
		}
		return;
	}

	if ((address & 0xf800) == 0x6800) {
		if (s11_bg_map) {
			hc55516_clock_w(0);
			hc55516_digit_w(data & 1);
		} else {
			hc55516_clock_w(1);
		}
		return;
	}

	if ((address & 0xf800) == 0x7800) {
		bankswitch(data);
		return;
	}
}

static UINT8 cvsd_read(UINT16 address)
{
	if ((address & 0xe001) == 0x2001) {
		return BurnYM2151Read();
	}

	if ((address & 0xe000) == 0x4000) {
		return pia_read(pia_select, address & 3);
	}

	return 0;
}

static void pia_out_a(UINT16 , UINT8 data)
{
	if (dac_signed)
		DACSignedWrite(dac_select, data);
	else
		DACWrite(dac_select, data);
}

static void pia_out_b(UINT16 , UINT8 data)
{
	audio_talkback = data;
}

static void pia_out_ca2(UINT16 , UINT8 data)
{
	if (data != ym_reset_state) {
		ym_reset_state = data;
		if (data) BurnYM2151Reset();
	}
}

static void pia_out_cb2(UINT16 , UINT8 data)
{
	if (cb2_callback) cb2_callback(data);
}

static void pia_irq0(INT32 state)
{
	M6809SetIRQLine(0x1/*firq*/, state ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static void pia_irq1(INT32 state)
{
	M6809SetIRQLine(0x20/*nmi*/, state ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static pia6821_interface pia_0 = {
	NULL, NULL,
	NULL, NULL, NULL, NULL,
	pia_out_a, pia_out_b, pia_out_ca2, pia_out_cb2,
	pia_irq0, pia_irq1
};

static void CVSDYM2151IrqHandler(INT32 state)
{
	pia_set_input_ca1(pia_select, !state);
}

// external

void cvsd_reset_write(INT32 state)
{
	if (cvsd_is_initialized == 0) return;

	cvsd_in_reset = state;

	if (state)
	{
		M6809CPUPush(cpu_select);
		M6809Reset();
		M6809SetIRQLine(0, CPU_IRQSTATUS_NONE);
		M6809SetIRQLine(1, CPU_IRQSTATUS_NONE);
		M6809SetIRQLine(0x20, CPU_IRQSTATUS_NONE);
		bankswitch(0);
		if (dac_select == 0) DACReset();
		BurnYM2151Reset();
		M6809CPUPop();
	}
}

void cvsd_cpu_reset()
{
	// board reset line only: cpu + rom bank (ym2151 / dac are not affected)
	if (cvsd_is_initialized == 0) return;

	M6809CPUPush(cpu_select);
	bankswitch(0);
	M6809Reset();
	M6809CPUPop();
}

void cvsd_data_write(UINT16 data)
{
	if (cvsd_is_initialized == 0) return;

	M6809CPUPush(cpu_select);
	pia_set_input_b(pia_select,   (data & 0x00ff));
	pia_set_input_cb1(pia_select, (data & 0x0100) >> 8);
	pia_set_input_cb2(pia_select, (data & 0x0200) >> 9);
	M6809CPUPop();
}

void cvsd_portb_write(UINT8 data)
{
	if (cvsd_is_initialized == 0) return;

	M6809CPUPush(cpu_select);
	pia_set_input_b(pia_select, data);
	M6809CPUPop();
}

void cvsd_cb1_write(INT32 state)
{
	if (cvsd_is_initialized == 0) return;

	M6809CPUPush(cpu_select);
	pia_set_input_cb1(pia_select, state ? 1 : 0);
	M6809CPUPop();
}

void cvsd_set_cb2_callback(void (*cb)(INT32 state))
{
	cb2_callback = cb;
}

void cvsd_set_s11_bg_mode(INT32 enable)
{
	s11_bg_map = enable;
}

void cvsd_set_dac_signed(INT32 enable)
{
	dac_signed = enable;
}

UINT8 cvsd_talkback_read()
{
	return audio_talkback;
}

INT32 cvsd_reset_status()
{
	if (cvsd_is_initialized == 0) return 0;
	return cvsd_in_reset;
}

void cvsd_reset()
{
	if (cvsd_is_initialized == 0) return;

	M6809Open(cpu_select);
	bankswitch(0);
	M6809Reset();
	BurnYM2151Reset();
	if (pia_select == 0) pia_reset();
	hc55516_reset();
	if (dac_select == 0) DACReset();
	pia_set_input_ca1(pia_select,1); // ??
	M6809Close();

	audio_talkback = 0;
	cvsd_in_reset = 0;
	ym_reset_state = 0;
}

void cvsd_init(INT32 m6809num, INT32 dacnum, INT32 pianum, UINT8 *rom, UINT8 *ram) // cpu is 2000000 hz
{
	mainrom = rom;
	pia_select = pianum;
	dac_select = dacnum;
	cpu_select = m6809num;
	cvsd_is_initialized = 1;
	cb2_callback = NULL;
	s11_bg_map = 0;
	dac_signed = 0;

	M6809Init(cpu_select);
	M6809Open(cpu_select);
	M6809MapMemory(ram,				0x0000, 0x07ff, MAP_RAM);
	M6809MapMemory(ram,				0x0800, 0x0fff, MAP_RAM);
	M6809MapMemory(ram,				0x1000, 0x17ff, MAP_RAM);
	M6809MapMemory(ram,				0x1800, 0x1fff, MAP_RAM);
	M6809MapMemory(mainrom,			0x8000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(cvsd_write);
	M6809SetReadHandler(cvsd_read);
	M6809Close();

	if (pia_select == 0) pia_init();
	pia_config(pia_select, 0, &pia_0);

	BurnYM2151InitBuffered(3579545, 1, NULL, 0);
	BurnYM2151SetIrqHandler(&CVSDYM2151IrqHandler);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.15, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.15, BURN_SND_ROUTE_RIGHT);
	BurnTimerAttachM6809(2000000);

	DACInit(dacnum, 0, 1, M6809TotalCycles, 2000000);
	DACSetRoute(dacnum, 0.50, BURN_SND_ROUTE_BOTH);

	hc55516_init(M6809TotalCycles, 2000000);
}

void cvsd_exit()
{
	if (cvsd_is_initialized == 0) return;

	if (cpu_select == 0) M6809Exit();
	if (pia_select == 0) pia_exit();
	BurnYM2151Exit();
	hc55516_exit();
	if (dac_select == 0) DACExit();

	cb2_callback = NULL;
	s11_bg_map = 0;
	dac_signed = 0;
	cvsd_is_initialized = 0;
}

void cvsd_update(INT16 *samples, INT32 length)
{
	if (cvsd_is_initialized == 0) return;

	INT32 cpunum = M6809GetActive();
	if (cpunum == -1) M6809Open(cpu_select);
	if (length) BurnYM2151Render(samples, length);
	if (((samples + length * 2) - (pBurnSoundOut + (nBurnSoundLen * 2))) == 0)
	{
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
		hc55516_update(pBurnSoundOut, nBurnSoundLen);
	}
	if (cpunum == -1) M6809Close();
}

INT32 cvsd_initialized()
{
	return cvsd_is_initialized;
}

void cvsd_scan(INT32 nAction, INT32 *pnMin)
{
	if (cvsd_is_initialized == 0) return;

	if (pnMin) {
		*pnMin = 0x029702;
	}

	if (nAction & ACB_VOLATILE) {
		if (cpu_select == 0) M6809Scan(nAction);

		BurnYM2151Scan(nAction, pnMin);
		if (dac_select == 0) DACScan(nAction,pnMin);
		if (pia_select == 0) pia_scan(nAction,pnMin);
		hc55516_scan(nAction, pnMin);

		SCAN_VAR(audio_talkback);
		SCAN_VAR(cpu_bank);
		SCAN_VAR(cvsd_in_reset);
		SCAN_VAR(ym_reset_state);
	}

	if (nAction & ACB_WRITE) {
		M6809Open(cpu_select);
		bankswitch(cpu_bank);
		M6809Close();
	}
}
