// Cheap Squeak Deluxe audio module
// based on MAME sources by Aaron Giles

#include "burnint.h"
#include "m68000_intf.h"
#include "6821pia.h"
#include "dac.h"

static UINT16 dacvalue;
static UINT16 csd_status;
static INT32 csd_in_reset;
static INT32 csd_is_intialized = 0;
static UINT16 *csd_ram = NULL;
extern INT32 ssio_spyhunter;

static INT32 pia_select;
static INT32 cpu_select;

// muting & pop-supression logic
struct anti_pop {
    UINT16 lastdacvalue;
    UINT16 cm30ctr;
    INT32 ending;
    INT32 mute;
    INT32 booting;
};

static anti_pop ml;

static void csd_porta_w(UINT16, UINT8 data)
{
    if (!ml.mute) {
        ml.lastdacvalue = dacvalue;
        dacvalue = (data << 2) | (dacvalue & 3);
    }

	if (ssio_spyhunter) {
        // csd_ram[0x30/2] continuously counts down as music is playing
        // if its stopped, or 0 - nothing is playing.  Let's zero-out the dac
        // so that we don't get loud clicks or pops before or after the music.
        // 0x4000 + (0x100 << 6) ends up being 0 in the dac.
        // extra notes:
        // porta is called every sample, playing or not.
        // portb is only called when music is playing (if needed)
        if (ml.cm30ctr == csd_ram[0x30/2] || csd_ram[0x30/2] == 0) {
            if (ml.mute == 0) {
                // counter stopped, begin the ending process.
                //bprintf(0, _T("csd ending.\n"));
                dacvalue = ml.lastdacvalue;
                ml.ending = 1;
            }
            // ramp dac from +-dc to 0 (0x100)
            if (dacvalue > 0x100) dacvalue--;
            else if (dacvalue < 0x100) dacvalue++;
            else if (dacvalue == 0x100 && ml.ending) {
                // we've gracefully ended a tune, or got stable after boot-up
                //bprintf(0, _T("csd %S.\n"), (ml.booting) ? "booted" : "sound ending");
                ml.ending = 0;
                ml.booting = 0;
                ml.lastdacvalue = 0x100;
            }
            ml.mute = 1;
        } else ml.mute = 0;

        ml.cm30ctr = csd_ram[0x30/2];
    }

    if (!ml.booting)
        DACWrite16Signed(0, 0x4000 + (dacvalue << 6));
}

static void csd_portb_w(UINT16, UINT8 data)
{
    if (!ml.mute) {
        ml.lastdacvalue = dacvalue;
        dacvalue = (dacvalue & ~0x3) | (data >> 6);
    }

    if (!ml.booting)
        DACWrite16Signed(0, 0x4000 + (dacvalue << 6));

    if (~pia_get_ddr_b(pia_select) & 0x30) csd_status = (data >> 4) & 3;
}

static void csd_irq(int state)
{
	SekSetIRQLine(cpu_select, 4, state ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

void csd_reset_write(int state)
{
	if (csd_is_intialized == 0) return;

	csd_in_reset = state;

	if (state) {
		SekReset(cpu_select);
	}
}

static void __fastcall csd_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0x1fff8) == 0x18000) {
		pia_write(pia_select, (address / 2) & 3, (data >> 8) & 0xff);
		return;
	}
}

static void __fastcall csd_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0x1fff8) == 0x18000) {
		pia_write(pia_select, (address / 2) & 3, data);
		return;
	}
}

static UINT16 __fastcall csd_read_word(UINT32 address)
{
	if ((address & 0x1fff8) == 0x18000) {
		UINT8 ret = pia_read(pia_select, (address / 2) & 3);
		return ret | (ret << 8);
	}

	return 0;
}

static UINT8 __fastcall csd_read_byte(UINT32 address)
{
	if ((address & 0x1fff8) == 0x18000) {
		return pia_read(pia_select, (address / 2) & 3);
	}

	return 0;
}

void csd_data_write(UINT16 data)
{
	if (csd_is_intialized == 0) return;

	pia_set_input_b(pia_select, data & 0x0f);
	pia_set_input_ca1(pia_select, ~data & 0x10);
}

UINT8 csd_status_read()
{
	if (csd_is_intialized == 0) return 0;

	return csd_status;
}

INT32 csd_reset_status()
{
	if (csd_is_intialized == 0) return 1;

	return csd_in_reset;
}

INT32 csd_initialized()
{
	return csd_is_intialized;
}

void csd_reset()
{
	if (csd_is_intialized == 0) return;

	SekOpen(cpu_select);
	SekReset();
	DACReset();
	SekClose();

	if (pia_select == 0) pia_reset();

	csd_status = 0;
	csd_in_reset = 0;
    dacvalue = 0;

	// pop-suppression
	ml.lastdacvalue = 0;
	ml.ending = 0;
	ml.mute = 0;
	ml.booting = (ssio_spyhunter) ? 1 : 0;
	ml.cm30ctr = 0;
}

static const pia6821_interface pia_intf = {
	0, 0, 0, 0, 0, 0,
	csd_porta_w, csd_portb_w, 0, 0,
	csd_irq, csd_irq
};

void csd_init(INT32 m68knum, INT32 pianum, UINT8 *rom, UINT8 *ram)
{
	pia_select = pianum;
	cpu_select = m68knum;

	csd_ram = (UINT16*)ram;
	SekInit(cpu_select, 0x68000);
	SekOpen(cpu_select);
	SekMapMemory(rom,			0x000000, 0x007fff, MAP_ROM);
	SekMapMemory(ram,			0x01c000, 0x01cfff, MAP_RAM);
	SekSetWriteWordHandler(0,	csd_write_word);
	SekSetWriteByteHandler(0,	csd_write_byte);
	SekSetReadWordHandler(0,	csd_read_word);
	SekSetReadByteHandler(0,	csd_read_byte);
	SekClose();

	if (pia_select == 0) pia_init();
	pia_config(pia_select, PIA_ALTERNATE_ORDERING, &pia_intf);
	
	DACInit(0, 0, 1, SekTotalCycles, 8000000);
	DACSetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);
	DACDCBlock(1);

	csd_is_intialized = 1;
}

void csd_exit()
{
	if (csd_is_intialized == 0) return;

	if (cpu_select == 0) SekExit();
	if (pia_select == 0) pia_init();
	DACExit();
    csd_is_intialized = 0;
    csd_ram = NULL;
}

void csd_scan(INT32 nAction, INT32 *pnMin)
{
	if (csd_is_intialized == 0) return;

	if (nAction & ACB_VOLATILE)
	{
		if (cpu_select == 0) SekScan(nAction);
		DACScan(nAction, pnMin);
		if (pia_select == 0) pia_scan(nAction, pnMin);

		SCAN_VAR(csd_status);
		SCAN_VAR(csd_in_reset);
        SCAN_VAR(dacvalue);
        SCAN_VAR(ml);
	}
}
