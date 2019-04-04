#include "burnint.h"
#include "m6502_intf.h"
#include "burn_ym2151.h"
#include "msm6295.h"
#include "pokey.h"
#include "tms5220.h"

static INT32 atarijsa_bank;
static INT32 speech_data;
static INT32 last_ctl;
static INT32 oki_banks[2];
static INT32 timed_int;
static INT32 ym2151_int;
static INT32 ym2151_ct1;

static double pokey_volume;
static double ym2151_volume;
//static double tms5220_volume;
static double oki6295_volume;

INT32 atarigen_cpu_to_sound;
INT32 atarigen_sound_to_cpu;
INT32 atarigen_cpu_to_sound_ready;
INT32 atarigen_sound_to_cpu_ready;

UINT8 atarijsa_input_port;
UINT8 atarijsa_test_port;
UINT8 atarijsa_test_mask;

static INT32 atarijsa_sound_timer;
INT32 atarijsa_int_state;

static INT32 has_pokey = 0;
static INT32 has_tms5220 = 0;
static UINT8 *samples[2] = { NULL, NULL };
static UINT8 *atarijsa_rom;
static UINT8 *atarijsa_ram;
static void (*update_int_callback)();

static void update_6502_irq()
{
	M6502SetIRQLine(0, (timed_int || ym2151_int) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static void bankswitch(INT32 data)
{
	atarijsa_bank = data & 3;
	M6502MapMemory(atarijsa_rom + (data & 3) * 0x1000, 0x3000, 0x3fff, MAP_ROM);
}

static void oki_bankswitch(INT32 chip, INT32 data)
{
	oki_banks[chip] = data;

	data = data & 3;
	if (data) data -= 1; // bank #1 is the same as bank #0

	MSM6295SetBank(chip, samples[chip] + data * 0x20000, 0x00000, 0x1ffff);
}

static void update_all_volumes()
{
	if (has_tms5220) {
//		m_tms5220->set_output_gain(ALL_OUTPUTS, tms5220_volume * ym2151_ct1);	// NOT SUPPORTED!!
	}
	if (has_pokey) {
//		PokeySetRoute(0, pokey_volume * ym2151_ct1, BURN_SND_ROUTE_BOTH);
	}

//	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, ym2151_volume, BURN_SND_ROUTE_LEFT);
//	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, ym2151_volume, BURN_SND_ROUTE_RIGHT);
}

static void atarijsa_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfc00) == 0x2c00) { // jsai only!
		if (has_pokey) pokey_write(0, address & 0xf, data);
		return;
	}

//	if (samples[0] == NULL) {
//		if ((address & 0xfc00) == 0x2800) address &= ~0x1f9;
//	} else {
//		if ((address & 0xf800) == 0x2000) address &= ~0x7fe;
//		if ((address & 0xfc00) == 0x2800) address &= ~0x5f8;
//	}

//	bprintf (0, _T("JSA W: %4.4x, %2.2x\n"), address, data);

	switch (address)
	{
		case 0x2000:
			BurnYM2151SelectRegister(data);
		return;

		case 0x2001:
			BurnYM2151WriteRegister(data);
		return;

		case 0x2800:
		case 0x2900:
			// :overall_volume_w (jsaiii) (volumes)
		return;

		case 0x2806:
		case 0x2807:
			timed_int = 0;
			update_6502_irq();
		return;

		case 0x2a00:
            speech_data = data;
            tms5220_write(data);
			if (samples[0]) MSM6295Write(0, data); // jsaii, jsaiii
		return;

		case 0x2a01:
			if (samples[1]) MSM6295Write(1, data); // jsaiii
		return;

		case 0x2a02:
		case 0x2a03:
		{
			atarigen_sound_to_cpu = data;
			atarigen_sound_to_cpu_ready = 1;
			atarijsa_int_state = 1;
			(*update_int_callback)();
		}
		return;

		case 0x2a04:
		case 0x2a05:
		{
            if (~data & 0x01) BurnYM2151Reset();

			if (has_tms5220)
			{
                tms5220_wsq_w((data >> 1) & 1);
                tms5220_rsq_w((data >> 2) & 1);

                INT32 count = 5 | ((data >> 2) & 2);
                tms5220_set_frequency((3579545*2) / (16 - count));
			}

			// coin counter = data & 0x30

			if (~data & 0x04) {
				INT32 pin = (data & 8) ? MSM6295_PIN7_HIGH : MSM6295_PIN7_LOW;
				if (samples[0]) MSM6295SetSamplerate(0, (3579545/3) / pin);
				if (samples[1]) MSM6295SetSamplerate(1, (3579545/3) / pin);
				if (samples[0]) MSM6295Reset(0);
				if (samples[1]) MSM6295Reset(1);
			}

			oki_banks[0] = (oki_banks[0] & 2) | ((data >> 1) & 1);
			if (samples[0]) oki_bankswitch(0, oki_banks[0]);

            bankswitch(data >> 6);
            last_ctl = data;
        }
		return;

		case 0x2a06:
		case 0x2a07:
		{
			oki_banks[1] = data >> 6;
			if (samples[1]) oki_bankswitch(1, oki_banks[1]);

			oki_banks[0] = (oki_banks[0] & 1) | ((data >> 3) & 2);
			if (samples[0]) oki_bankswitch(0, oki_banks[0]);

			ym2151_volume = ((data >> 1) & 7) / 7.0;
			oki6295_volume = (data & 1) ? 1.0 : 0.5;
			update_all_volumes();
		}
		return;
	}

	bprintf (0, _T("MISS JSA W: %4.4x, %2.2x\n"), address, data);
}

static UINT8 atarijsa_read(UINT16 address)
{	
	if ((address & 0xfc00) == 0x2c00) { // jsai only!
		return has_pokey ? pokey_read(0, address & 0xf) : 0xff;
	}

//	bprintf (0, _T("JSA R: %4.4x\n"), address);

//	if (if (samples[0] == NULL) { // iq_132
//		if ((address & 0xfc00) == 0x2800) address &= ~0x1f9;
//	} else {
//		if ((address & 0xf800) == 0x2000) address &= ~0x7fe;
//		if ((address & 0xfc00) == 0x2800) address &= ~0x5f8;
//	}

	switch (address)
	{
		case 0x2000:
			return 0xff;

		case 0x2001:
			return BurnYM2151Read();

		case 0x2800: // jsaii && iii
		case 0x2808:
			return (samples[0]) ? MSM6295Read(0) : 0xff;

		case 0x2801: // jsa iii
			return (samples[1]) ? MSM6295Read(1) : 0xff;

		case 0x2802:
		case 0x280a:
		{
			if (atarigen_sound_to_cpu_ready)
				bprintf (0, _T("Missed result from 6502\n"));

			atarigen_cpu_to_sound_ready = 0;
			M6502SetIRQLine(0x20, CPU_IRQSTATUS_NONE);
			return atarigen_cpu_to_sound;
		}

		case 0x2804:
		case 0x280c:
		{
            UINT8 result = (atarijsa_input_port) | 0x10;
            if (!(atarijsa_test_port & atarijsa_test_mask)) result ^= 0x80;
			if (atarigen_cpu_to_sound_ready) result ^= 0x40;
			if (atarigen_sound_to_cpu_ready) result ^= 0x20;
            if (has_tms5220 && tms5220_ready() == 0) result ^= 0x10;
            return result;
		}

		case 0x2806:
		case 0x280e:
			timed_int = 0;
			update_6502_irq();
			return 0xff;
	}

	bprintf (0, _T("MISS JSA R: %4.4x\n"), address);

	return 0xff;
}

static void JsaYM2151IrqHandler(INT32 nStatus)
{
	ym2151_int = nStatus;
	update_6502_irq();
}

static void JsaYM2151Write(UINT32 , UINT32 data)
{
	ym2151_ct1 = (data >> 0) & 1;
	update_all_volumes();
}

void AtariJSAReset()
{
	M6502Open(0);
	bankswitch(0);
	M6502Reset();
	M6502Close();

	if (samples[0]) {
		oki_bankswitch(0, 0);
		MSM6295Reset(0);
	}

	if (samples[1]) {
		oki_bankswitch(1, 0);
		MSM6295Reset(1);
	}

	BurnYM2151Reset();

	if (has_pokey) PokeyReset();
	if (has_tms5220) tms5220_reset();

	speech_data = 0;
	last_ctl = 0;
	timed_int = 0;
	ym2151_int = 0;
	ym2151_ct1 = 0;

	atarijsa_sound_timer = 0;

	pokey_volume = 1.0;
	ym2151_volume = 1.0;
	//tms5220_volume = 1.0;

	atarigen_cpu_to_sound = 0;
	atarigen_cpu_to_sound_ready = 0;
	atarigen_sound_to_cpu = 0;
	atarigen_sound_to_cpu_ready = 0;

	atarijsa_int_state = 0;
}

void AtariJSAInit(UINT8 *rom, void (*int_cb)(), UINT8 *samples0, UINT8 *samples1)
{
	atarijsa_rom = rom;
	atarijsa_ram = (UINT8*)BurnMalloc(0x2000);

	update_int_callback = int_cb;

	samples[0] = samples0;
	samples[1] = samples1;

	has_tms5220 = (samples0 == NULL) && (samples1 == NULL);
	has_pokey = has_tms5220;

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(atarijsa_ram,			0x0000, 0x1fff, MAP_RAM);
	M6502MapMemory(atarijsa_rom + 0x4000,	0x4000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(atarijsa_write);
	M6502SetReadHandler(atarijsa_read);
	M6502Close();

	BurnYM2151Init(3579545);
	BurnYM2151SetIrqHandler(&JsaYM2151IrqHandler);
	YM2151SetPortWriteHandler(0, &JsaYM2151Write);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.60, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.60, BURN_SND_ROUTE_RIGHT);

	MSM6295Init(0, (3579545/3) / MSM6295_PIN7_HIGH, 1);
	MSM6295Init(1, (3579545/3) / MSM6295_PIN7_HIGH, 1);
	MSM6295SetRoute(0, 0.75, BURN_SND_ROUTE_BOTH);
	MSM6295SetRoute(1, 0.75, BURN_SND_ROUTE_BOTH);
	if (samples[0]) {
		MSM6295SetBank(0, samples[0] + 0x00000, 0x00000, 0x1ffff);
		MSM6295SetBank(0, samples[0] + 0x60000, 0x20000, 0x3ffff);
	}

	if (samples[1]) {
		MSM6295SetBank(1, samples[1] + 0x00000, 0x00000, 0x1ffff);
		MSM6295SetBank(1, samples[1] + 0x60000, 0x20000, 0x3ffff);
	}

	PokeyInit(3579545/2, 1, 0.40, 1);
	PokeySetTotalCyclesCB(M6502TotalCycles);

	tms5220c_init(M6502TotalCycles, 1789773);
    tms5220_set_frequency((3579545*2)/11);
    tms5220_volume(1.50);
}

void AtariJSAExit()
{
	BurnYM2151Exit();
	MSM6295Exit(0);
	MSM6295Exit(1);
	tms5220_exit();
	PokeyExit();
	M6502Exit();

	BurnFree (atarijsa_ram);

	has_tms5220 = 0;
	MSM6295ROM = NULL;
}

void AtariJSAScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029722;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = atarijsa_ram;
		ba.nLen	  = 0x2000;
		ba.szName = "JSA Ram";
		BurnAcb(&ba);

		M6502Scan(nAction);

		BurnYM2151Scan(nAction, pnMin);
		MSM6295Scan(nAction, pnMin);
		pokey_scan(nAction, pnMin);
		tms5220_scan(nAction, pnMin);

		SCAN_VAR(atarijsa_bank);
		SCAN_VAR(speech_data);
		SCAN_VAR(last_ctl);
		SCAN_VAR(oki_banks);
		SCAN_VAR(atarigen_cpu_to_sound);
		SCAN_VAR(atarigen_cpu_to_sound_ready);
		SCAN_VAR(atarigen_sound_to_cpu);
		SCAN_VAR(atarigen_sound_to_cpu_ready);
		SCAN_VAR(atarijsa_int_state);
	}
	if (nAction & ACB_WRITE)
	{
		M6502Open(0);
		bankswitch(atarijsa_bank);
		M6502Close();
	}
}

// use AtariJSAInterruptUpdate unless we're not at ~60hz
void AtariJSAInterrupt() // TIME_IN_HZ((double)ATARI_CLOCK_3MHz/4/16/16/14) call 4x per frame (if 60hz)
{
	timed_int = 1;
	update_6502_irq();
}

void AtariJSAInterruptUpdate(INT32 interleave)
{
	INT32 modr = (((interleave * 1000) / 416) + 5) / 10; // should happen 4.16x per frame
	if (modr == 0) modr = 63;

	if ((atarijsa_sound_timer % modr) == (modr-1))
    {
		timed_int = 1;
		update_6502_irq();
	}

	atarijsa_sound_timer++;
}

void AtariJSAUpdate(INT16 *output, INT32 length)
{
	BurnYM2151Render(output, length);
	if (samples[0] || samples[1]) MSM6295Render(output, length);
	if (has_pokey) pokey_update(output, length);

	if ((output + (length*2)) == (pBurnSoundOut + (nBurnSoundLen*2))) {
        if (has_tms5220) tms5220_update(pBurnSoundOut, nBurnSoundLen);
	}
}

//#include "m68000_intf.h" // debug

UINT16 AtariJSARead()
{
//	bprintf (0, _T("JSA MAIN READ %4.4x\n"), atarigen_sound_to_cpu | 0xff00);

	atarigen_sound_to_cpu_ready = 0;
	atarijsa_int_state = 0;
	(*update_int_callback)();
	return atarigen_sound_to_cpu | 0xff00;
}

void AtariJSAWrite(UINT8 data)
{
	if (atarigen_cpu_to_sound_ready)
		bprintf (0, _T("Missed command from 68010\n"));

//	bprintf (0, _T("JSA MAIN WRITE %2.2x\n"), data);
	atarigen_cpu_to_sound = data;
	atarigen_cpu_to_sound_ready = 1;
	M6502SetIRQLine(0x20, CPU_IRQSTATUS_ACK);
}

void AtariJSAResetWrite(UINT8 data)
{
	if (data == 0) {
		M6502Reset();
	}

//	bprintf (0, _T("JSA MAIN RESET %2.2x\n"), data);
	atarigen_sound_to_cpu_ready = 0;
	atarijsa_int_state = 0;
	(*update_int_callback)();
}
