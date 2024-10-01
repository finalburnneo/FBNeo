// license:BSD-3-Clause
// copyright-holders:Ed Bernard, Jonathan Gevaryahu, hap
// thanks-to:Kevin Horton
/*
    SSi TSI S14001A speech IC emulator
    aka CRC: Custom ROM Controller, designed in 1975, first usage in 1976 on TSI Speech+ calculator
    Originally written for MAME by Jonathan Gevaryahu(Lord Nightmare) 2006-2013,
    replaced with near-complete rewrite by Ed Bernard in 2016

    TODO:
    - nothing at the moment?

    Further reading:
    - http://www.vintagecalculators.com/html/speech-.html
    - http://www.vintagecalculators.com/html/development_of_the_tsi_speech-.html
    - http://www.vintagecalculators.com/html/speech-_state_machine.html
    - https://archive.org/stream/pdfy-QPCSwTWiFz1u9WU_/david_djvu.txt
*/

/* Chip Pinout:
The original datasheet (which is lost as far as I know) clearly called the
s14001a chip the 'CRC chip', or 'Custom Rom Controller', as it appears with
this name on the Stern and Canon schematics, as well as on some TSI speech
print advertisements.
Labels are not based on the labels used by the Atari wolf pack and Stern
schematics, as these are inconsistent. Atari calls the word select/speech address
input pins SAx while Stern calls them Cx. Also Atari and Canon both have the bit
ordering for the word select/speech address bus backwards, which may indicate it
was so on the original datasheet. Stern has it correct, and I've used their Cx
labeling.

                      ______    ______
                    _|o     \__/      |_
            +5V -- |_|1             40|_| -> /BUSY*
                    _|                |_
          ?TEST ?? |_|2             39|_| <- ROM D7
                    _|                |_
 XTAL CLOCK/CKC -> |_|3             38|_| -> ROM A11
                    _|                |_
  ROM CLOCK/CKR <- |_|4             37|_| <- ROM D6
                    _|                |_
  DIGITAL OUT 0 <- |_|5             36|_| -> ROM A10
                    _|                |_
  DIGITAL OUT 1 <- |_|6             35|_| -> ROM A9
                    _|                |_
  DIGITAL OUT 2 <- |_|7             34|_| <- ROM D5
                    _|                |_
  DIGITAL OUT 3 <- |_|8             33|_| -> ROM A8
                    _|                |_
        ROM /EN <- |_|9             32|_| <- ROM D4
                    _|       S        |_
          START -> |_|10 7   1   T  31|_| -> ROM A7
                    _|   7   4   S    |_
      AUDIO OUT <- |_|11 3   0   I  30|_| <- ROM D3
                    _|   7   0        |_
         ROM A0 <- |_|12     1      29|_| -> ROM A6
                    _|       A        |_
SPCH ADR BUS C0 -> |_|13            28|_| <- SPCH ADR BUS C5
                    _|                |_
         ROM A1 <- |_|14            27|_| <- ROM D2
                    _|                |_
SPCH ADR BUS C1 -> |_|15            26|_| <- SPCH ADR BUS C4
                    _|                |_
         ROM A2 <- |_|16            25|_| <- ROM D1
                    _|                |_
SPCH ADR BUS C2 -> |_|17            24|_| <- SPCH ADR BUS C3
                    _|                |_
         ROM A3 <- |_|18            23|_| <- ROM D0
                    _|                |_
         ROM A4 <- |_|19            22|_| -> ROM A5
                    _|                |_
            GND -- |_|20            21|_| -- -10V
                     |________________|

*Note from Kevin Horton when testing the hookup of the S14001A: the /BUSY line
is not a standard voltage line: when it is in its HIGH state (i.e. not busy) it
puts out a voltage of -10 volts, so it needs to be dropped back to a sane
voltage level before it can be passed to any sort of modern IC. The address
lines for the speech rom (A0-A11) do not have this problem, they output at a
TTL/CMOS compatible voltage. The AUDIO OUT pin also outputs a voltage below GND,
and the TEST pins may do so too.

START is pulled high when a word is to be said and the word number is on the
word select/speech address input lines. The Canon 'Canola' uses a separate 'rom
strobe' signal independent of the chip to either enable or clock the speech rom.
It's likely that they did this to be able to force the speech chip to stop talking,
which is normally impossible. The later 'version 3' TSI speech board as featured in
an advertisement in the John Cater book probably also has this feature, in addition
to external speech rom banking.

The Digital out pins supply a copy of the 4-bit waveform which also goes to the
internal DAC. They are only valid every other clock cycle. It is possible that
on 'invalid' cycles they act as a 4 bit input to drive the dac.

Because it requires -10V to operate, the chip manufacturing process must be PMOS.

* Operation:
Put the 6-bit address of the word to be said onto the C0-C5 word select/speech
address bus lines. Next, clock the START line low-high-low. As long as the START
line is held high, the first address byte of the first word will be read repeatedly
every clock, with the rom enable line enabled constantly (i.e. it doesn't toggle on
and off as it normally does during speech). Once START has gone low-high-low, the
/BUSY line will go low until 3 clocks after the chip is done speaking.
*/

#include <stddef.h>
#include "burnint.h"
#include "bitswap.h"
#include "s14001a.h"
#include "stream.h"

enum
{
	states_IDLE = 0,
	states_WORDWAIT,
	states_CWARMSB,    // read 8 CWAR MSBs
	states_CWARLSB,    // read 4 CWAR LSBs from rom d7-d4
	states_DARMSB,     // read 8 DAR  MSBs
	states_CTRLBITS,   // read Stop, Voiced, Silence, Length, XRepeat
	states_PLAY,
	states_DELAY
};

static Stream stream;

struct s14001a_vars {
	// internal state
	bool bPhase1; // 1 bit internal clock

	// registers
	UINT8 uStateP1;          // 3 bits
	UINT8 uStateP2;

	UINT16 uDAR13To05P1;      // 9 MSBs of delta address register
	UINT16 uDAR13To05P2;      // incrementing uDAR05To13 advances ROM address by 8 bytes

	UINT16 uDAR04To00P1;      // 5 LSBs of delta address register
	UINT16 uDAR04To00P2;      // 3 address ROM, 2 mux 8 bits of data into 2 bit delta
								// carry indicates end of quarter pitch period (32 cycles)

	UINT16 uCWARP1;           // 12 bits Control Word Address Register (syllable)
	UINT16 uCWARP2;

	bool bStopP1;
	bool bStopP2;
	bool bVoicedP1;
	bool bVoicedP2;
	bool bSilenceP1;
	bool bSilenceP2;
	UINT8 uLengthP1;          // 7 bits, upper three loaded from ROM length
	UINT8 uLengthP2;          // middle two loaded from ROM repeat and/or uXRepeat
								// bit 0 indicates mirror in voiced mode
								// bit 1 indicates internal silence in voiced mode
								// incremented each pitch period quarter

	UINT8 uXRepeatP1;         // 2 bits, loaded from ROM repeat
	UINT8 uXRepeatP2;
	UINT8 uDeltaOldP1;        // 2 bit old delta
	UINT8 uDeltaOldP2;
	UINT8 uOutputP1;          // 4 bits audio output, calculated during phase 1

	// derived signals
	bool bDAR04To00CarryP2;
	bool bPPQCarryP2;
	bool bRepeatCarryP2;
	bool bLengthCarryP2;
	UINT16 RomAddrP1;         // rom address

	// output pins
	UINT8 uOutputP2;          // output changes on phase2
	UINT16 uRomAddrP2;        // address pins change on phase 2
	bool bBusyP1;             // busy changes on phase 1

	// input pins
	bool bStart;
	UINT8 uWord;              // 6 bit word number to be spoken

	// emulator variables
	// statistics
	UINT32 uNPitchPeriods;
	UINT32 uNVoiced;
	UINT32 uNControlWords;

	// diagnostic output
	UINT32 uPrintLevel;

	// fbn added
	double volume;
	INT32 clock;

	UINT8 *SpeechRom;

	//devcb_write_line bsy_handler;
	//devcb_read8 ext_read_handler;
};

static s14001a_vars chip;

static UINT8 Mux8To2(bool bVoicedP2, UINT8 uPPQtrP2, UINT8 uDeltaAdrP2, UINT8 uRomDataP2)
{
	// pick two bits of rom data as delta

	if (bVoicedP2 && (uPPQtrP2 & 0x01)) // mirroring
		uDeltaAdrP2 ^= 0x03; // count backwards

	// emulate 8 to 2 mux to obtain delta from byte (bigendian)
	return uRomDataP2 >> (~uDeltaAdrP2 << 1 & 0x06) & 0x03;
}


static void CalculateIncrement(bool bVoicedP2, UINT8 uPPQtrP2, bool bPPQStartP2, UINT8 uDelta, UINT8 uDeltaOldP2, UINT8 &uDeltaOldP1, UINT8 &uIncrementP2, bool &bAddP2)
{
	// uPPQtr, pitch period quarter counter; 2 lsb of uLength
	// bPPStart, start of a pitch period
	// implemented to mimic silicon (a bit)

	// beginning of a pitch period
	if ((uPPQtrP2 == 0x00) && bPPQStartP2) // note this is done for voiced and unvoiced
		uDeltaOldP2 = 0x02;

	static const UINT8 uIncrements[4][4] =
	{
	//    00  01  10  11
		{ 3,  3,  1,  1,}, // 00
		{ 1,  1,  0,  0,}, // 01
		{ 0,  0,  1,  1,}, // 10
		{ 1,  1,  3,  3 }, // 11
	};

	bool const MIRROR = BIT(uPPQtrP2, 0);

	// calculate increment from delta, always done even if silent to update uDeltaOld
	// in silicon a PLA determined 0,1,3 and add/subtract and passed uDelta to uDeltaOld
	if (!bVoicedP2 || !MIRROR)
	{
		uIncrementP2 = uIncrements[uDelta][uDeltaOldP2];
		bAddP2       = uDelta >= 0x02;
	}
	else
	{
		uIncrementP2 = uIncrements[uDeltaOldP2][uDelta];
		bAddP2       = uDeltaOldP2 < 0x02;
	}
	uDeltaOldP1 = uDelta;
	if (bVoicedP2 && bPPQStartP2 && MIRROR)
		uIncrementP2 = 0; // no change when first starting mirroring
}


static UINT8 CalculateOutput(bool bVoiced, bool bXSilence, UINT8 uPPQtr, bool bPPQStart, UINT8 uLOutput, UINT8 uIncrementP2, bool bAddP2)
{
	// implemented to mimic silicon (a bit)
	// limits output to 0x00 and 0x0f

	bool const SILENCE = BIT(uPPQtr, 1);

	// determine output
	if (bXSilence || (bVoiced && SILENCE))
		return 7;

	// beginning of a pitch period
	if ((uPPQtr == 0x00) && bPPQStart) // note this is done for voiced and nonvoiced
		uLOutput = 7;

	// adder
	UINT8 uTmp = uLOutput;
	if (!bAddP2)
		uTmp ^= 0x0F; // turns subtraction into addition

	// add 0, 1, 3; limit at 15
	uTmp += uIncrementP2;
	if (uTmp > 15)
		uTmp = 15;

	if (!bAddP2)
		uTmp ^= 0x0F; // turns addition back to subtraction

	return uTmp;
}


//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------
static void s14001a_sound_stream_update(INT16 **streams, INT32 length); // forward
static bool Clock(); // forward

void s14001a_init(UINT8 *rom, INT32 (*pCPUCyclesCB)(), INT32 nCpuMHZ)
{
	stream.init(nBurnSoundRate, nBurnSoundRate, 1, 1, s14001a_sound_stream_update);
	stream.set_buffered(pCPUCyclesCB, nCpuMHZ);
	stream.set_volume(1.00);

	//stream.set_debug(2);

	chip.SpeechRom = rom;

	// resolve callbacks
	//chip.ext_read_handler.resolve();
	//chip.bsy_handler.resolve();

	s14001a_reset();
}

void s14001a_render(INT16 *buffer, INT32 length)
{
	if (length != nBurnSoundLen) {
		bprintf(0, _T("s14001a_render(): once per frame, please!\n"));
		return;
	}

	stream.render(buffer, length);
}

void s14001a_exit()
{
	stream.exit();
}

void s14001a_scan(INT32 nAction, INT32 *pnMin)
{
	ScanVar(&chip, STRUCT_SIZE_HELPER(s14001a_vars, clock), "s14001a SpeechSynth Chip");

	if (nAction & ACB_WRITE) {
		s14001a_set_clock(chip.clock);
		s14001a_set_volume(chip.volume);
	}
}

void s14001a_reset()
{
	memset(&chip, 0, STRUCT_SIZE_HELPER(s14001a_vars, uPrintLevel));

	chip.uOutputP1 = chip.uOutputP2 = 7;
}

//-------------------------------------------------
//  sound_stream_update - handle a stream update
//-------------------------------------------------

void s14001a_sound_stream_update(INT16 **streams, INT32 length)
{
	for (INT32 i = 0; i < length; i++)
	{
		Clock();
		INT16 sample = chip.uOutputP2 - 7; // range -7..8
		streams[0][i] = sample * 0xf00;
	}
}


/**************************************************************************
    External interface
**************************************************************************/

void s14001a_force_update()
{
	stream.update();
}

INT32 s14001a_romen_read()
{
	stream.update();
	return (chip.bPhase1) ? 1 : 0;
}

INT32 s14001a_busy_read()
{
    stream.update();
	return (chip.bBusyP1) ? 1 : 0;
}

void s14001a_data_write(UINT8 data)
{
	stream.update();
	chip.uWord = data & 0x3f; // C0-C5
}

void s14001a_start_write(INT32 state)
{
	stream.update();
	chip.bStart = (state != 0);
	if (chip.bStart) chip.uStateP1 = states_WORDWAIT;
}

void s14001a_set_clock(INT32 clock)
{
	chip.clock = clock;
	stream.set_rate(clock);
}

void s14001a_set_volume(double volume)
{
	chip.volume = volume;
	stream.set_volume(volume);
}

/**************************************************************************
    Device emulation
**************************************************************************/

static UINT8 readmem(UINT16 offset, bool phase)
{
	offset &= 0xfff; // 11-bit internal
	return chip.SpeechRom[offset];
	//return ((chip.ext_read_handler.isnull()) ? chip.SpeechRom[offset & (chip.SpeechRom.bytes() - 1)] : chip.ext_read_handler(offset));
}

static bool Clock()
{
	// effectively toggles external clock twice, one cycle
	// internal clock toggles on external clock transition from 0 to 1 so internal clock will always transition here
	// return false if some emulator problem detected

	// On the actual chip, all register phase 1 values needed to be refreshed from phase 2 values
	// or else risk losing their state due to charge loss.
	// But on a computer the values are static.
	// So to reduce code clutter, phase 1 values are only modified if they are different
	// from the preceeding phase 2 values.

	if (chip.bPhase1)
	{
		// transition to phase2
		chip.bPhase1 = false;

		// transfer phase1 variables to phase2
		chip.uStateP2     = chip.uStateP1;
		chip.uDAR13To05P2 = chip.uDAR13To05P1;
		chip.uDAR04To00P2 = chip.uDAR04To00P1;
		chip.uCWARP2      = chip.uCWARP1;
		chip.bStopP2      = chip.bStopP1;
		chip.bVoicedP2    = chip.bVoicedP1;
		chip.bSilenceP2   = chip.bSilenceP1;
		chip.uLengthP2    = chip.uLengthP1;
		chip.uXRepeatP2   = chip.uXRepeatP1;
		chip.uDeltaOldP2  = chip.uDeltaOldP1;

		chip.uOutputP2    = chip.uOutputP1;
		chip.uRomAddrP2   = chip.RomAddrP1;

		// setup carries from phase 2 values
		chip.bDAR04To00CarryP2  = chip.uDAR04To00P2 == 0x1F;
		chip.bPPQCarryP2        = chip.bDAR04To00CarryP2 && ((chip.uLengthP2&0x03) == 0x03); // pitch period quarter
		chip.bRepeatCarryP2     = chip.bPPQCarryP2       && ((chip.uLengthP2&0x0C) == 0x0C);
		chip.bLengthCarryP2     = chip.bRepeatCarryP2    && ( chip.uLengthP2       == 0x7F);

		return true;
	}
	chip.bPhase1 = true;

	// logic done during phase 1
	switch (chip.uStateP1)
	{
	case states_IDLE:
		chip.uOutputP1 = 7;
		if (chip.bStart) chip.uStateP1 = states_WORDWAIT;

		//if (chip.bBusyP1 && !chip.bsy_handler.isnull())
		//	chip.bsy_handler(0);
		chip.bBusyP1 = false;
		break;

	case states_WORDWAIT:
		// the delta address register latches the word number into bits 03 to 08
		// all other bits forced to 0.  04 to 08 makes a multiply by two.
		chip.uDAR13To05P1 = (chip.uWord&0x3C)>>2;
		chip.uDAR04To00P1 = (chip.uWord&0x03)<<3;
		chip.RomAddrP1 = (chip.uDAR13To05P1<<3)|(chip.uDAR04To00P1>>2); // remove lower two bits
		chip.uOutputP1 = 7;
		if (chip.bStart) chip.uStateP1 = states_WORDWAIT;
		else          chip.uStateP1 = states_CWARMSB;

		//if (!chip.bBusyP1 && !chip.bsy_handler.isnull())
		//	chip.bsy_handler(1);
		chip.bBusyP1 = true;
		break;

	case states_CWARMSB:
		if (chip.uPrintLevel >= 1)
			bprintf(0, _T("\n speaking word %02x"),chip.uWord);

		// use uDAR to load uCWAR 8 msb
		chip.uCWARP1 = readmem(chip.uRomAddrP2,chip.bPhase1)<<4; // note use of rom address setup in previous state
		// increment DAR by 4, 2 lsb's count deltas within a byte
		chip.uDAR04To00P1 += 4;
		if (chip.uDAR04To00P1 >= 32) chip.uDAR04To00P1 = 0; // emulate 5 bit counter
		chip.RomAddrP1 = (chip.uDAR13To05P1<<3)|(chip.uDAR04To00P1>>2); // remove lower two bits

		chip.uOutputP1 = 7;
		if (chip.bStart) chip.uStateP1 = states_WORDWAIT;
		else          chip.uStateP1 = states_CWARLSB;
		break;

	case states_CWARLSB:
		chip.uCWARP1   = chip.uCWARP2|(readmem(chip.uRomAddrP2,chip.bPhase1)>>4); // setup in previous state
		chip.RomAddrP1 = chip.uCWARP1;

		chip.uOutputP1 = 7;
		if (chip.bStart) chip.uStateP1 = states_WORDWAIT;
		else          chip.uStateP1 = states_DARMSB;
		break;

	case states_DARMSB:
		chip.uDAR13To05P1 = readmem(chip.uRomAddrP2,chip.bPhase1)<<1; // 9 bit counter, 8 MSBs from ROM, lsb zeroed
		chip.uDAR04To00P1 = 0;
		chip.uCWARP1++;
		chip.RomAddrP1 = chip.uCWARP1;
		chip.uNControlWords++; // statistics

		chip.uOutputP1 = 7;
		if (chip.bStart) chip.uStateP1 = states_WORDWAIT;
		else          chip.uStateP1 = states_CTRLBITS;
		break;

	case states_CTRLBITS:
		chip.bStopP1    = readmem(chip.uRomAddrP2,chip.bPhase1)&0x80? true: false;
		chip.bVoicedP1  = readmem(chip.uRomAddrP2,chip.bPhase1)&0x40? true: false;
		chip.bSilenceP1 = readmem(chip.uRomAddrP2,chip.bPhase1)&0x20? true: false;
		chip.uXRepeatP1 = readmem(chip.uRomAddrP2,chip.bPhase1)&0x03;
		chip.uLengthP1  =(readmem(chip.uRomAddrP2,chip.bPhase1)&0x1F)<<2; // includes external length and repeat
		chip.uDAR04To00P1 = 0;
		chip.uCWARP1++; // gets ready for next DARMSB
		chip.RomAddrP1  = (chip.uDAR13To05P1<<3)|(chip.uDAR04To00P1>>2); // remove lower two bits

		chip.uOutputP1 = 7;
		if (chip.bStart) chip.uStateP1 = states_WORDWAIT;
		else          chip.uStateP1 = states_PLAY;

		if (chip.uPrintLevel >= 2)
			bprintf(0, _T("\n cw %d %d %d %d %d"),chip.bStopP1,chip.bVoicedP1,chip.bSilenceP1,chip.uLengthP1>>4,chip.uXRepeatP1);

		break;

	case states_PLAY:
	{
		// statistics
		if (chip.bPPQCarryP2)
		{
			// pitch period end
			if (chip.uPrintLevel >= 3)
				bprintf(0, _T("\n ppe: RomAddr %03x"),chip.uRomAddrP2);

			chip.uNPitchPeriods++;
			if (chip.bVoicedP2) chip.uNVoiced++;
		}
		// end statistics

		// modify output
		UINT8 uDeltaP2;     // signal line
		UINT8 uIncrementP2; // signal lines
		bool bAddP2;        // signal line
		uDeltaP2 = Mux8To2(chip.bVoicedP2,
					chip.uLengthP2 & 0x03,     // pitch period quater counter
					chip.uDAR04To00P2 & 0x03,  // two bit delta address within byte
					readmem(chip.uRomAddrP2,chip.bPhase1)
		);
		CalculateIncrement(chip.bVoicedP2,
					chip.uLengthP2 & 0x03,     // pitch period quater counter
					chip.uDAR04To00P2 == 0,    // pitch period quarter start
					uDeltaP2,
					chip.uDeltaOldP2,          // input
					chip.uDeltaOldP1,          // output
					uIncrementP2,           // output 0, 1, or 3
					bAddP2                  // output
		);
		chip.uOutputP1 = CalculateOutput(chip.bVoicedP2,
					chip.bSilenceP2,
					chip.uLengthP2 & 0x03,     // pitch period quater counter
					chip.uDAR04To00P2 == 0,    // pitch period quarter start
					chip.uOutputP2,            // last output
					uIncrementP2,
					bAddP2
		);

		// advance counters
		chip.uDAR04To00P1++;
		if (chip.bDAR04To00CarryP2) // pitch period quarter end
		{
			chip.uDAR04To00P1 = 0; // emulate 5 bit counter

			chip.uLengthP1++; // lower two bits of length count quarter pitch periods
			if (chip.uLengthP1 >= 0x80)
			{
				chip.uLengthP1 = 0; // emulate 7 bit counter
			}
		}

		if (chip.bVoicedP2 && chip.bRepeatCarryP2) // repeat complete
		{
			chip.uLengthP1 &= 0x70; // keep current "length"
			chip.uLengthP1 |= (chip.uXRepeatP1<<2); // load repeat from external repeat
			chip.uDAR13To05P1++; // advances ROM address 8 bytes
			if (chip.uDAR13To05P1 >= 0x200) chip.uDAR13To05P1 = 0; // emulate 9 bit counter
		}
		if (!chip.bVoicedP2 && chip.bDAR04To00CarryP2)
		{
			// unvoiced advances each quarter pitch period
			// note repeat counter not reloaded for non voiced speech
			chip.uDAR13To05P1++; // advances ROM address 8 bytes
			if (chip.uDAR13To05P1 >= 0x200) chip.uDAR13To05P1 = 0; // emulate 9 bit counter
		}

		// construct chip.RomAddrP1
		chip.RomAddrP1 = chip.uDAR04To00P1;
		if (chip.bVoicedP2 && chip.uLengthP1&0x1) // mirroring
		{
			chip.RomAddrP1 ^= 0x1f; // count backwards
		}
		chip.RomAddrP1 = (chip.uDAR13To05P1<<3) | chip.RomAddrP1>>2;

		// next state
		if (chip.bStart) chip.uStateP1 = states_WORDWAIT;
		else if (chip.bStopP2 && chip.bLengthCarryP2) chip.uStateP1 = states_DELAY;
		else if (chip.bLengthCarryP2)
		{
			chip.uStateP1  = states_DARMSB;
			chip.RomAddrP1 = chip.uCWARP1; // output correct address
		}
		else chip.uStateP1 = states_PLAY;
		break;
	}

	case states_DELAY:
		chip.uOutputP1 = 7;
		if (chip.bStart) chip.uStateP1 = states_WORDWAIT;
		else          chip.uStateP1 = states_IDLE;
		break;
	}

	return true;
}

/*static void ClearStatistics()
{
	chip.uNPitchPeriods = 0;
	chip.uNVoiced       = 0;
	chip.uPrintLevel    = 0;
	chip.uNControlWords = 0;
}

static void GetStatistics(UINT32 &uNPitchPeriods, UINT32 &uNVoiced, UINT32 &uNControlWords)
{
	uNPitchPeriods = chip.uNPitchPeriods;
	uNVoiced = chip.uNVoiced;
	uNControlWords = chip.uNControlWords;
}*/
