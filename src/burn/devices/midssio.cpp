// Midway SSIO audio / input module
// Based on MAME sources by Aaron Giles

#include "burnint.h"
#include "z80_intf.h"
#include "ay8910.h"
#include "flt_rc.h"

// #define SSIODEBUG

static INT32 ssio_is_initialized = 0;
static INT32 ssio_14024_count;
static INT32 ssio_data[4];
static INT32 ssio_status;
static INT32 ssio_duty_cycle[2][3];
static INT32 ssio_mute;
static INT32 ssio_overall[2];

INT32 ssio_spyhunter = 0;

typedef void (*output_func)(UINT8 offset, UINT8 data);
typedef UINT8 (*input_func)(UINT8 offset);

static double ssio_ayvolume_lookup[16];
static output_func output_handlers[2] = { NULL, NULL };
static input_func input_handlers[5] = { NULL, NULL, NULL, NULL, NULL };
static INT32 output_mask[2] = { 0xff, 0xff };
static INT32 input_mask[5] = { 0, 0, 0, 0, 0 };

static double ssio_basevol = 0.0;

UINT8 *ssio_inputs; // 5
UINT8 ssio_dips; // 1

static void __fastcall ssio_cpu_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf000) == 0xc000) {
#ifdef SSIODEBUG
        bprintf (0, _T("SSIO Status Write: %2.2x\n"), data);
#endif
		ssio_status = data;
		return;
	}

	if ((address & 0xf000) == 0xd000) {
		return; // nop
	}

	switch (address & ~0xffc)
	{
		case 0xa000:
			AY8910Write(0, 0, data);
		return;

		case 0xa002:
			AY8910Write(0, 1, data);
		return;

		case 0xb000:
			AY8910Write(1, 0, data);
		return;

		case 0xb002:
			AY8910Write(1, 1, data);
		return;
	}

}

static UINT8 __fastcall ssio_cpu_read(UINT16 address)
{
	if ((address & 0xf000) == 0xc000) {
		return 0; // nop
	}

	if ((address & 0xf000) == 0xe000) {
		//ssio_14024_count = 0;
        //ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
		return 0xff;
	}

	if ((address & 0xf000) == 0xf000) {
		return ssio_dips;
	}

	switch (address & ~0xffc)
	{
		case 0x9000:
		case 0x9001:
		case 0x9002:
		case 0x9003:
			return ssio_data[address & 3];

		case 0xa001:
			return AY8910Read(0);

		case 0xb001:
			return AY8910Read(1);
	}


	return 0;
}

static void ssio_update_volumes()
{
    AY8910SetRoute(0, BURN_SND_AY8910_ROUTE_1, ssio_mute ? 0 : ssio_ayvolume_lookup[ssio_duty_cycle[0][0]]+ssio_basevol, BURN_SND_ROUTE_PANLEFT);
    AY8910SetRoute(0, BURN_SND_AY8910_ROUTE_2, ssio_mute ? 0 : ssio_ayvolume_lookup[ssio_duty_cycle[0][1]]+ssio_basevol, BURN_SND_ROUTE_PANLEFT);
    AY8910SetRoute(0, BURN_SND_AY8910_ROUTE_3, ssio_mute ? 0 : ssio_ayvolume_lookup[ssio_duty_cycle[0][2]]+ssio_basevol, BURN_SND_ROUTE_PANLEFT);

    AY8910SetRoute(1, BURN_SND_AY8910_ROUTE_1, ssio_mute ? 0 : ssio_ayvolume_lookup[ssio_duty_cycle[1][0]]+ssio_basevol, BURN_SND_ROUTE_PANRIGHT);
    AY8910SetRoute(1, BURN_SND_AY8910_ROUTE_2, ssio_mute ? 0 : ssio_ayvolume_lookup[ssio_duty_cycle[1][1]]+ssio_basevol, BURN_SND_ROUTE_PANRIGHT);
    AY8910SetRoute(1, BURN_SND_AY8910_ROUTE_3, ssio_mute ? 0 : ssio_ayvolume_lookup[ssio_duty_cycle[1][2]]+ssio_basevol, BURN_SND_ROUTE_PANRIGHT);

    if (ssio_spyhunter) {
        filter_rc_set_src_gain(0, ssio_mute ? 0 : ssio_ayvolume_lookup[ssio_duty_cycle[0][0]]+ssio_basevol);
        filter_rc_set_src_gain(1, ssio_mute ? 0 : ssio_ayvolume_lookup[ssio_duty_cycle[0][1]]+ssio_basevol);
        filter_rc_set_src_gain(2, ssio_mute ? 0 : ssio_ayvolume_lookup[ssio_duty_cycle[0][2]]+ssio_basevol);
        filter_rc_set_src_gain(3, ssio_mute ? 0 : ssio_ayvolume_lookup[ssio_duty_cycle[1][0]]+ssio_basevol);
        filter_rc_set_src_gain(4, ssio_mute ? 0 : ssio_ayvolume_lookup[ssio_duty_cycle[1][1]]+ssio_basevol);
        filter_rc_set_src_gain(5, ssio_mute ? 0 : ssio_ayvolume_lookup[ssio_duty_cycle[1][2]]+ssio_basevol);
    }
}

static void AY8910_write_0A(UINT32 /*addr*/, UINT32 data)
{
	ssio_duty_cycle[0][0] = data & 15;
	ssio_duty_cycle[0][1] = data >> 4;
	ssio_update_volumes();
}

static void AY8910_write_0B(UINT32 /*addr*/, UINT32 data)
{
	ssio_duty_cycle[0][2] = data & 15;
	ssio_overall[0] = (data >> 4) & 7;
	ssio_update_volumes();
}

static void AY8910_write_1A(UINT32 /*addr*/, UINT32 data)
{
	ssio_duty_cycle[1][0] = data & 15;
	ssio_duty_cycle[1][1] = data >> 4;
	ssio_update_volumes();
}

static void AY8910_write_1B(UINT32 /*addr*/, UINT32 data)
{
	ssio_duty_cycle[1][2] = data & 15;
	ssio_overall[1] = (data >> 4) & 7;
	ssio_mute = data & 0x80;
	ssio_update_volumes();
}

void ssio_14024_clock(INT32 interleave) // interrupt generator
{
	if (ssio_is_initialized == 0) return;

    // ~26x per frame
    if (++ssio_14024_count >= (interleave/26)) {
        ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
        ssio_14024_count = 0;
    }
}

void ssio_reset_write(INT32 state)
{
	if (ssio_is_initialized == 0) return;

	if (state)
	{
		ZetSetRESETLine(1, 1);

		for (INT32 i = 0; i < 4; i++)
			ssio_data[i] = 0;
		ssio_status = 0;
		ssio_14024_count = 0;
    } else {
		ZetSetRESETLine(1, 0);
    }
}

static void ssio_compute_ay8910_modulation(UINT8 *prom)
{
	for (INT32 volval = 0; volval < 16; volval++)
	{
		INT32 clock;
		INT32 remaining_clocks = volval;
		INT32 cur = 0, prev = 1;

		for (clock = 0; clock < 160 && remaining_clocks; clock++)
		{
			cur = prom[clock / 8] & (0x80 >> (clock % 8));

			if (cur == 0 && prev != 0)
				remaining_clocks--;

			prev = cur;
		}

        ssio_ayvolume_lookup[15-volval] = ((double)(clock * 100 / 160) / 100) / 4;
        //bprintf(0, _T("vol %02d: %f\n"), 15-volval,ssio_ayvolume_lookup[15-volval]);
	}
}

static UINT8 ssio_input_port_read(UINT8 offset)
{
	offset &= 7;

	UINT8 result = ssio_inputs[offset & 7];

	if (input_handlers[offset])
		result = (result & ~input_mask[offset]) | ((input_handlers[offset])(offset) & input_mask[offset]);

	return result;
}

static void ssio_output_port_write(UINT8 offset, UINT8 data)
{
	offset &= 7;
    UINT8 which = offset >> 2;

	if (output_handlers[which])
		(*output_handlers[which])(offset, data & output_mask[which]);
}

void ssio_write_ports(UINT8 address, UINT8 data)
{
	switch (address)
	{
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
		case 0x04:
		case 0x05:
		case 0x06:
		case 0x07:
			ssio_output_port_write(address & 7, data);
		return;

		case 0x1c:
		case 0x1d:
		case 0x1e:
		case 0x1f:
#ifdef SSIODEBUG
			bprintf (0, _T("SSIO Write: %2.2x, %2.2x\n"), address & 3, data);
#endif
            ssio_data[address & 3] = data;
		return;
	}
}

UINT8 ssio_read_ports(UINT8 address)
{
	switch (address & ~0x18)
	{
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
		case 0x04:
			return ssio_input_port_read(address & 7);

		case 0x07:
#ifdef SSIODEBUG
			bprintf (0, _T("SSIO Status Read: %2.2x\n"), ssio_status);
#endif
            return ssio_status;
	}

	return 0xff;
}

void ssio_set_custom_input(INT32 which, INT32 mask, UINT8 (*handler)(UINT8 offset))
{
	input_handlers[which] = handler;
	input_mask[which] = mask;
}

void ssio_set_custom_output(INT32 which, INT32 mask, void (*handler)(UINT8 offset, UINT8 data))
{
	output_handlers[which/4] = handler;
	output_mask[which/4] = mask;
}

void ssio_reset()
{
	if (ssio_is_initialized == 0) return;

	ssio_reset_write(1);
	ssio_reset_write(0);

	AY8910Reset(0);
	AY8910Reset(1);
}

void ssio_basevolume(double vol)
{
	if (ssio_is_initialized == 0) return;

    ssio_basevol = vol;

	AY8910SetAllRoutes(0, vol, BURN_SND_ROUTE_PANLEFT);
    AY8910SetAllRoutes(1, vol, BURN_SND_ROUTE_PANRIGHT);

    if (ssio_spyhunter) {
        filter_rc_set_src_gain(0, vol);
        filter_rc_set_src_gain(1, vol);
        filter_rc_set_src_gain(2, vol);
        filter_rc_set_src_gain(3, vol);
        filter_rc_set_src_gain(4, vol);
        filter_rc_set_src_gain(5, vol);
    }
}

void ssio_init(UINT8 *rom, UINT8 *ram, UINT8 *prom)
{
	ssio_compute_ay8910_modulation(prom);

	for (INT32 i = 0; i < 4; i++) {
		output_handlers[i>>1] = NULL;
		input_handlers[i] = NULL;
		output_mask[i>>1] = 0xff;
		input_mask[i] = 0;
	}

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(rom,		0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(ram,		0x8000, 0x83ff, MAP_RAM);
	ZetMapMemory(ram,		0x8400, 0x87ff, MAP_RAM);
	ZetMapMemory(ram,		0x8800, 0x8bff, MAP_RAM);
	ZetMapMemory(ram,		0x8c00, 0x8fff, MAP_RAM);
	ZetSetWriteHandler(ssio_cpu_write);
	ZetSetReadHandler(ssio_cpu_read);
	ZetClose();

	AY8910Init(0, 2000000, 0);
	AY8910Init(1, 2000000, 0);
	AY8910SetPorts(0, NULL, NULL, AY8910_write_0A, AY8910_write_0B);
	AY8910SetPorts(1, NULL, NULL, AY8910_write_1A, AY8910_write_1B);

	ssio_is_initialized = 1;

    ssio_basevolume(0.05);
}

void ssio_scan(INT32 nAction, INT32 *pnMin)
{
	if (ssio_is_initialized == 0) return;

    if (nAction & ACB_VOLATILE) {
        AY8910Scan(nAction, pnMin);

        SCAN_VAR(ssio_14024_count);
        SCAN_VAR(ssio_data);
        SCAN_VAR(ssio_status);
        SCAN_VAR(ssio_duty_cycle);
        SCAN_VAR(ssio_mute);
        SCAN_VAR(ssio_overall);
    }
}

void ssio_exit()
{
	ssio_set_custom_output(0, 0xff, NULL);
	ssio_set_custom_output(1, 0xff, NULL);

	for (INT32 i = 0; i < 5; i++ ){
		ssio_set_custom_input(i, 0, NULL);
	}

	if (ssio_is_initialized == 0) return;

	AY8910Exit(0);
	AY8910Exit(1);

    ssio_is_initialized = 0;
    ssio_spyhunter = 0;
}

INT32 ssio_initialized()
{
	return ssio_is_initialized;
}
