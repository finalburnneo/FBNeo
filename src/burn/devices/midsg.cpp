// Midway Sounds Good audio module
// based on MAME sources by Aaron Giles

#include "burnint.h"
#include "m68000_intf.h"
#include "6821pia.h"
#include "dac.h"

static UINT16 dacvalue;
static UINT16 soundsgood_status;
static INT32 soundsgood_in_reset;
static INT32 soundsgood_is_initialized;
static INT32 which_cpu, which_dac;
static UINT16 *sg_ram = NULL;

INT32 soundsgood_rampage = 0;

// muting & pop-supression logic (rampage only)
struct anti_pop {
    UINT16 last80;
    INT32 booting;
};

static anti_pop ml;

static void soundsgood_porta_w(UINT16, UINT8 data)
{
	dacvalue = (dacvalue & 3) | (data << 2);

    if (ml.booting) {
        // for Rampage
        // The game holds the dac at full +dc for nearly 2 seconds while
        // booting up; this is a problem when using headphones or a very
        // powerfull amp/speaker system - or - if one doesn't want the added
        // noise.
        // 0x80 - 0x82 are 2 sound-code "fifo" buffers.  some sounds are
        // played in 0x80,0x81, and others in 0x82,0x83.  Simply put - we're
        // waiting for the soundcpu to fully bootup and play its first sound
        // before unmuting the dac.  This gets past all the nasty clicking
        // and popping noises that gets written to the dac @ bootup.
        // - dink April 2019
        if (ml.booting == 1 && sg_ram[0x80/2] == 1 && ml.last80 == 1) {
            ml.booting = 2;
            //bprintf(0, _T("booting pt.2.\n"));
        }
        if (ml.booting == 2 && sg_ram[0x80/2] == 0 && ml.last80 == 1) {
            //bprintf(0, _T("booting pt.3\n"));
            ml.booting = 3;
        }
        if (ml.booting == 3 && (sg_ram[0x80/2] != 0 || sg_ram[0x82/2] != 0) && ml.last80 == 0) {
            bprintf(0, _T("*** soundsgood[rampage]: un-muting\n"));
            ml.booting = 0;
        }
        ml.last80 = sg_ram[0x80/2];
    }

    if (!ml.booting) {
        DACWrite16Signed(which_dac, 0x4000 + (dacvalue << 6));
    }
}

static void soundsgood_portb_w(UINT16, UINT8 data)
{
    dacvalue = (dacvalue & ~3) | (data >> 6);

    if (!ml.booting) {
        DACWrite16Signed(which_dac, 0x4000 + (dacvalue << 6));
    }

	if (pia_get_ddr_b(0) & 0x30) soundsgood_status = (data >> 4) & 3;
}

static void soundsgood_irq(int state)
{
	SekSetIRQLine(4, state ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

void soundsgood_data_write(UINT16 data)
{
	pia_set_input_b(0, (data >> 1) & 0xf);
	pia_set_input_ca1(0, ~data & 0x01);
}

UINT8 soundsgood_status_read()
{
	if (soundsgood_is_initialized == 0) return 0;
	return soundsgood_status;
}

void soundsgood_reset_write(int state)
{
	if (soundsgood_is_initialized == 0) return;

	soundsgood_in_reset = state;

	if (state)
	{
		INT32 cpu_active = SekGetActive();
		if (cpu_active == -1) SekOpen(which_cpu);
		SekReset();
		if (cpu_active == -1) SekClose();
	}
}

static void __fastcall soundsgood_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xffff8) == 0x060000) {
		pia_write(0, (address / 2) & 3, data & 0xff);
		return;
	}
}

static void __fastcall soundsgood_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xffff8) == 0x060000) {
		pia_write(0, (address / 2) & 3, data);
		return;
	}
}

static UINT16 __fastcall soundsgood_read_word(UINT32 address)
{
	if ((address & 0xffff8) == 0x060000) {
		UINT8 ret = pia_read(0, (address / 2) & 3);
		return ret | (ret << 8);
	}

	return 0;
}

static UINT8 __fastcall soundsgood_read_byte(UINT32 address)
{
	if ((address & 0xffff8) == 0x060000) {
		return pia_read(0, (address / 2) & 3);
	}

	return 0;
}

void soundsgood_reset()
{
	if (soundsgood_is_initialized == 0) return;

	SekOpen(which_cpu);
	SekReset();
	DACReset();
	SekClose();

	pia_reset();

	soundsgood_status = 0;
	soundsgood_in_reset = 0;
	dacvalue = 0;

    // pop-suppression
    ml.booting = (soundsgood_rampage) ? 1 : 0;
    ml.last80 = -1;
}

static const pia6821_interface pia_intf = {
	0, 0, 0, 0, 0, 0,
	soundsgood_porta_w, soundsgood_portb_w, 0, 0,
	soundsgood_irq, soundsgood_irq
};

void soundsgood_init(INT32 n68knum, INT32 dacnum, UINT8 *rom, UINT8 *ram)
{
    sg_ram = (UINT16*)ram;
	SekInit(n68knum, 0x68000);
	SekOpen(n68knum);
	SekMapMemory(rom,			0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(ram,			0x070000, 0x070fff, MAP_RAM);
	SekSetWriteWordHandler(0,	soundsgood_write_word);
	SekSetWriteByteHandler(0,	soundsgood_write_byte);
	SekSetReadWordHandler(0,	soundsgood_read_word);
	SekSetReadByteHandler(0,	soundsgood_read_byte);
	SekClose();

	pia_init();
	pia_config(0, PIA_ALTERNATE_ORDERING, &pia_intf);
	
	DACInit(dacnum, 0, 0, SekTotalCycles, 8000000);
	DACSetRoute(dacnum, 1.00, BURN_SND_ROUTE_BOTH);
    DACDCBlock(1);

	soundsgood_is_initialized = 1;
}

void soundsgood_exit()
{
	if (soundsgood_is_initialized == 0) return;

	SekExit();
	pia_init();
	DACExit();

    soundsgood_is_initialized = 0;

    soundsgood_rampage = 0;
}

void soundsgood_scan(INT32 nAction, INT32 *pnMin)
{
	if (soundsgood_is_initialized == 0) return;

	if (nAction & ACB_VOLATILE)
	{
		if (which_cpu == 0) SekScan(nAction);
		if (which_dac == 0) DACScan(nAction, pnMin);
		pia_scan(nAction, pnMin);

		SCAN_VAR(soundsgood_status);
		SCAN_VAR(soundsgood_in_reset);
        SCAN_VAR(dacvalue);

        SCAN_VAR(ml);
	}
}

INT32 soundsgood_reset_status()
{
	if (soundsgood_is_initialized == 0) return 1;
	return soundsgood_in_reset;
}

INT32 soundsgood_initialized()
{
	return soundsgood_is_initialized;
}
