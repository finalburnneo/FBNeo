/**********************************************************************************************

     TMS5110 simulator (modified from TMS5220 by Jarek Burczynski)

     Written for MAME by Frank Palazzolo
     With help from Neill Corlett
     Additional tweaking by Aaron Giles
     Various fixes by Lord Nightmare
     Additional enhancements by Couriersud
     Sub-interpolation-cycle parameter updating added by Lord Nightmare

     Todo:
        - implement CS
        - implement missing commands
        - TMS5110_CMD_TEST_TALK is only partially implemented

     TMS5100:

                 +-----------------+
        TST      |  1           28 |  CS
        PDC      |  2           27 |  CTL8
        ROM CK   |  3           26 |  ADD8
        CPU CK   |  4           25 |  CTL1
        VDD      |  5           24 |  ADD1
        CR OSC   |  6           23 |  CTL2
        RC OSC   |  7           22 |  ADD2
        T11      |  8           21 |  ADD4
        NC       |  9           20 |  CTL4
        I/O      | 10           19 |  M1
        SPK1     | 11           18 |  NC
        SPK2     | 12           17 |  NC
        PROM OUT | 13           16 |  NC
        VSS      | 14           15 |  M0
                 +-----------------+

        T11: Sync for serial data out


    M58817

    The following connections could be derived from radar scope schematics.
    The M58817 is not 100% pin compatible to the 5100, but really close.

                 +-----------------+
        (NC)     |  1           28 |  CS
        PDC      |  2           27 |  CTL8
        ROM CK   |  3           26 |  ADD8 (to 58819)
        (NC)     |  4           25 |  CTL1
        (VDD,-5) |  5           24 |  ADD1 (to 58819)
        (GND)    |  6           23 |  CTL2
        Xin      |  7           22 |  ADD2 (to 58819)
        Xout     |  8           21 |  ADD4 (to 58819)
        (NC)     |  9           20 |  CTL4
        (VDD,-5) | 10           19 |  Status back to CPU
        (NC)     | 11           18 |  C1 (to 58819)
        SPKR     | 12           17 |  (NC)
        SPKR     | 13           16 |  C0 (to 58819)
        (NC)     | 14           15 |  (5V)
                 +-----------------+

***********************************************************************************************/
// port from mame0138 dec.29 2022
#include <stddef.h>
#include <math.h>

#include "burnint.h"
#include "tms5110.h"

/* Pull in the ROM tables */
#include "tms5110_tables.h"

#define logerror
#define MAX_SAMPLE_CHUNK		512
#define FIFO_SIZE				64 // TODO: technically the tms51xx chips don't have a fifo at all

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

/* States for CTL */

#define CTL_STATE_INPUT 		(0)
#define CTL_STATE_OUTPUT		(1)
#define CTL_STATE_NEXT_OUTPUT	(2)

struct tms5110_state
{
	/* coefficient tables */
	int variant;				/* Variant of the 5110 - see tms5110.h */

	/* these contain data that describes the 64 bits FIFO */
	UINT8 fifo[FIFO_SIZE];
	UINT8 fifo_head;
	UINT8 fifo_tail;
	UINT8 fifo_count;

	/* these contain global status bits */
	UINT8 PDC;
	UINT8 CTL_pins;
	UINT8 speaking_now;
	UINT8 speak_delay_frames;
	UINT8 talk_status;
	UINT8 state;

	/* Rom interface */
	UINT32 address;
	UINT8  next_is_address;
	UINT8  schedule_dummy_read;
	UINT8  addr_bit;

	/* these contain data describing the current and previous voice frames */
	UINT16 old_energy;
	UINT16 old_pitch;
	INT32 old_k[10];

	UINT16 new_energy;
	UINT16 new_pitch;
	INT32 new_k[10];


	/* these are all used to contain the current state of the sound generation */
	UINT16 current_energy;
	UINT16 current_pitch;
	INT32 current_k[10];

	UINT16 target_energy;
	UINT16 target_pitch;
	INT32 target_k[10];

	UINT8 interp_count;       /* number of interp periods (0-7) */
	UINT8 sample_count;       /* sample number within interp (0-24) */
	INT32 pitch_count;

	INT32 x[11];

	INT32 speech_rom_bitnum;

	INT32 RNG;	/* the random noise generator configuration is: 1 + x + x^3 + x^4 + x^13 */
	INT32 our_freq; // samplerate of chip

	/* coefficient tables */
	const struct tms5100_coeffs *coeff;

	/* external callback */
	int (*M0_callback)();
	void (*set_load_address)(int);

	//const tms5110_interface *intf;
	const UINT8 *table; // the rom
	//sound_stream *stream;

	//emu_timer *romclk_timer;
	//UINT8 romclk_timer_started;
	//UINT8 romclk_state;
};



/* Static function prototypes */
static void tms5110_set_cvariant(tms5110_state *tms, int variant);
static void tms5110_reset_chip(tms5110_state *tms);

//static void tms5110_PDC_set(tms5110_state *tms, int data);
//static void tms5110_process(tms5110_state *tms, INT16 *buffer, unsigned int size);
static void parse_frame(tms5110_state *tms);
//static STREAM_UPDATE( tms5110_update );
//static TIMER_CALLBACK( romclk_timer_cb );


#define DEBUG_5110	0

// for resampling
#include "stream.h"
static Stream stream;

static tms5110_state *our_chip = NULL;
static INT32 tms5110_initted = 0;

void tms5110_set_buffered(INT32 (*pCPUCyclesCB)(), INT32 nCpuMhz)
{
    bprintf(0, _T("*** Using BUFFERED tms5110-mode.\n"));

	stream.set_buffered(pCPUCyclesCB, nCpuMhz);
}

void tms5110_volume(double vol)
{
	stream.set_volume(vol);
}

void tms5110_set_cvariant(tms5110_state *tms, int variant)
{
	switch (variant)
	{
		case TMS5110_IS_5110A:
			tms->coeff = &tms5110a_coeff;
			break;
		case TMS5110_IS_5100:
			tms->coeff = &pat4209836_coeff;
			break;
		case TMS5110_IS_5110:
			tms->coeff = &pat4403965_coeff;
			break;
		default:
			//fatalerror("Unknown variant in tms5110_create\n");
			break;
	}

	tms->variant = variant;
}

void tms5110_scan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (nAction & ACB_DRIVER_DATA) {
		memset(&ba, 0, sizeof(ba));
		ba.Data		= our_chip;
		ba.nLen		= STRUCT_SIZE_HELPER(tms5110_state, our_freq);
		ba.szName	= "TMS5110 SpeechSynth Chip";
		BurnAcb(&ba);
	}

	if (nAction & ACB_WRITE) {
		stream.set_rate(our_chip->our_freq);
	}
}


/******************************************************************************************

     FIFO_data_write -- handle bit data write to the TMS5110 (as a result of toggling M0 pin)

******************************************************************************************/
static void FIFO_data_write(tms5110_state *tms, int data)
{
	/* add this bit to the FIFO */
	if (tms->fifo_count < FIFO_SIZE)
	{
		tms->fifo[tms->fifo_tail] = (data&1); /* set bit to 1 or 0 */

		tms->fifo_tail = (tms->fifo_tail + 1) % FIFO_SIZE;
		tms->fifo_count++;

		if (DEBUG_5110) logerror("Added bit to FIFO (size=%2d)\n", tms->fifo_count);
	}
	else
	{
		if (DEBUG_5110) logerror("Ran out of room in the FIFO!\n");
	}
}

/******************************************************************************************

     extract_bits -- extract a specific number of bits from the FIFO

******************************************************************************************/

static int extract_bits(tms5110_state *tms, int count)
{
	int val = 0;

	while (count--)
	{
		val = (val << 1) | (tms->fifo[tms->fifo_head] & 1);
		tms->fifo_count--;
		tms->fifo_head = (tms->fifo_head + 1) % FIFO_SIZE;
	}
	return val;
}

static void request_bits(tms5110_state *tms, int no)
{
int i;
	for (i=0; i<no; i++)
	{
		if (tms->M0_callback)
		{
			int data = (*tms->M0_callback)();
			FIFO_data_write(tms, data);
		}
		else
			if (DEBUG_5110) logerror("-->ERROR: TMS5110 missing M0 callback function\n");
	}
}

static void perform_dummy_read(tms5110_state *tms)
{
	if (tms->schedule_dummy_read)
	{
		if (tms->M0_callback)
		{
			int data = (*tms->M0_callback)();
				if (DEBUG_5110) logerror("TMS5110 performing dummy read; value read = %1i\n", data&1);
		}
			else
				if (DEBUG_5110) logerror("-->ERROR: TMS5110 missing M0 callback function\n");
		tms->schedule_dummy_read = FALSE;
	}
}




/**********************************************************************************************

     tms5110_process -- fill the buffer with a specific number of samples

***********************************************************************************************/

void tms5110_process(INT16 **streams, INT32 size)
{
	INT16 *buffer = streams[0];
	tms5110_state *tms = our_chip;

	int buf_count=0;
	int i, interp_period, bitout;
	INT16 Y11, cliptemp;

	/* if we're not speaking, fill with nothingness */
	if (!tms->speaking_now)
		goto empty;

	/* if we're to speak, but haven't started */
	if (!tms->talk_status)
	{

	/* a "dummy read" is mentioned in the tms5200 datasheet */
	/* The Bagman speech roms data are organized in such a way that
    ** the bit at address 0 is NOT a speech data. The bit at address 1
    ** is the speech data. It seems that the tms5110 performs a dummy read
    ** just before it executes a SPEAK command.
    ** This has been moved to command logic ...
    **  perform_dummy_read(tms);
    */

		/* clear out the new frame parameters (it will become old frame just before the first call to parse_frame() ) */
		tms->new_energy = 0;
		tms->new_pitch = 0;
		for (i = 0; i < tms->coeff->num_k; i++)
			tms->new_k[i] = 0;

		tms->talk_status = 1;
	}


	/* loop until the buffer is full or we've stopped speaking */
	while ((size > 0) && tms->speaking_now)
	{
		int current_val;

		/* if we're ready for a new frame */
		if ((tms->interp_count == 0) && (tms->sample_count == 0))
		{

			/* remember previous frame */
			tms->old_energy = tms->new_energy;
			tms->old_pitch = tms->new_pitch;
			for (i = 0; i < tms->coeff->num_k; i++)
				tms->old_k[i] = tms->new_k[i];


			/* if the old frame was a stop frame, exit and do not process any more frames */
			if (tms->old_energy == COEFF_ENERGY_SENTINEL)
			{
				if (DEBUG_5110) logerror("processing frame: stop frame\n");
				tms->target_energy = tms->current_energy = 0;
				tms->speaking_now = tms->talk_status = 0;
				tms->interp_count = tms->sample_count = tms->pitch_count = 0;
				goto empty;
			}


			/* Parse a new frame into the new_energy, new_pitch and new_k[] */
			parse_frame(tms);


			/* Set old target as new start of frame */
			tms->current_energy = tms->old_energy;
			tms->current_pitch = tms->old_pitch;

			for (i = 0; i < tms->coeff->num_k; i++)
				tms->current_k[i] = tms->old_k[i];


			/* is this the stop (ramp down) frame? */
			if (tms->new_energy == COEFF_ENERGY_SENTINEL)
			{
				/*logerror("processing frame: ramp down\n");*/
				tms->target_energy = 0;
				tms->target_pitch = tms->old_pitch;
				for (i = 0; i < tms->coeff->num_k; i++)
					tms->target_k[i] = tms->old_k[i];
			}
			else if ((tms->old_energy == 0) && (tms->new_energy != 0)) /* was the old frame a zero-energy frame? */
			{
				/* if so, and if the new frame is non-zero energy frame then the new parameters
                   should become our current and target parameters immediately,
                   i.e. we should NOT interpolate them slowly in.
                */

				/*logerror("processing non-zero energy frame after zero-energy frame\n");*/
				tms->target_energy = tms->new_energy;
				tms->target_pitch = tms->current_pitch = tms->new_pitch;
				for (i = 0; i < tms->coeff->num_k; i++)
					tms->target_k[i] = tms->current_k[i] = tms->new_k[i];
			}
			else if ((tms->old_pitch == 0) && (tms->new_pitch != 0))	/* is this a change from unvoiced to voiced frame ? */
			{
				/* if so, then the new parameters should become our current and target parameters immediately,
                   i.e. we should NOT interpolate them slowly in.
                */
				/*if (DEBUG_5110) logerror("processing frame: UNVOICED->VOICED frame change\n");*/
				tms->target_energy = tms->new_energy;
				tms->target_pitch = tms->current_pitch = tms->new_pitch;
				for (i = 0; i < tms->coeff->num_k; i++)
					tms->target_k[i] = tms->current_k[i] = tms->new_k[i];
			}
			else if ((tms->old_pitch != 0) && (tms->new_pitch == 0))	/* is this a change from voiced to unvoiced frame ? */
			{
				/* if so, then the new parameters should become our current and target parameters immediately,
                   i.e. we should NOT interpolate them slowly in.
                */
				/*if (DEBUG_5110) logerror("processing frame: VOICED->UNVOICED frame change\n");*/
				tms->target_energy = tms->new_energy;
				tms->target_pitch = tms->current_pitch = tms->new_pitch;
				for (i = 0; i < tms->coeff->num_k; i++)
					tms->target_k[i] = tms->current_k[i] = tms->new_k[i];
			}
			else
			{
				/*logerror("processing frame: Normal\n");*/
				/*logerror("*** Energy = %d\n",current_energy);*/
				/*logerror("proc: %d %d\n",last_fbuf_head,fbuf_head);*/

				tms->target_energy = tms->new_energy;
				tms->target_pitch = tms->new_pitch;
				for (i = 0; i < tms->coeff->num_k; i++)
					tms->target_k[i] = tms->new_k[i];
			}
		}
		else
		{
			interp_period = tms->sample_count / 25;
			switch(tms->interp_count)
			{
				/*         PC=X  X cycle, rendering change (change for next cycle which chip is actually doing) */
				case 0: /* PC=0, A cycle, nothing happens (calc energy) */
				break;
				case 1: /* PC=0, B cycle, nothing happens (update energy) */
				break;
				case 2: /* PC=1, A cycle, update energy (calc pitch) */
				tms->current_energy += ((tms->target_energy - tms->current_energy) >> tms->coeff->interp_coeff[interp_period]);
				break;
				case 3: /* PC=1, B cycle, nothing happens (update pitch) */
				break;
				case 4: /* PC=2, A cycle, update pitch (calc K1) */
				tms->current_pitch += ((tms->target_pitch - tms->current_pitch) >> tms->coeff->interp_coeff[interp_period]);
				break;
				case 5: /* PC=2, B cycle, nothing happens (update K1) */
				break;
				case 6: /* PC=3, A cycle, update K1 (calc K2) */
				tms->current_k[0] += ((tms->target_k[0] - tms->current_k[0]) >> tms->coeff->interp_coeff[interp_period]);
				break;
				case 7: /* PC=3, B cycle, nothing happens (update K2) */
				break;
				case 8: /* PC=4, A cycle, update K2 (calc K3) */
				tms->current_k[1] += ((tms->target_k[1] - tms->current_k[1]) >> tms->coeff->interp_coeff[interp_period]);
				break;
				case 9: /* PC=4, B cycle, nothing happens (update K3) */
				break;
				case 10: /* PC=5, A cycle, update K3 (calc K4) */
				tms->current_k[2] += ((tms->target_k[2] - tms->current_k[2]) >> tms->coeff->interp_coeff[interp_period]);
				break;
				case 11: /* PC=5, B cycle, nothing happens (update K4) */
				break;
				case 12: /* PC=6, A cycle, update K4 (calc K5) */
				tms->current_k[3] += ((tms->target_k[3] - tms->current_k[3]) >> tms->coeff->interp_coeff[interp_period]);
				break;
				case 13: /* PC=6, B cycle, nothing happens (update K5) */
				break;
				case 14: /* PC=7, A cycle, update K5 (calc K6) */
				tms->current_k[4] += ((tms->target_k[4] - tms->current_k[4]) >> tms->coeff->interp_coeff[interp_period]);
				break;
				case 15: /* PC=7, B cycle, nothing happens (update K6) */
				break;
				case 16: /* PC=8, A cycle, update K6 (calc K7) */
				tms->current_k[5] += ((tms->target_k[5] - tms->current_k[5]) >> tms->coeff->interp_coeff[interp_period]);
				break;
				case 17: /* PC=8, B cycle, nothing happens (update K7) */
				break;
				case 18: /* PC=9, A cycle, update K7 (calc K8) */
				tms->current_k[6] += ((tms->target_k[6] - tms->current_k[6]) >> tms->coeff->interp_coeff[interp_period]);
				break;
				case 19: /* PC=9, B cycle, nothing happens (update K8) */
				break;
				case 20: /* PC=10, A cycle, update K8 (calc K9) */
				tms->current_k[7] += ((tms->target_k[7] - tms->current_k[7]) >> tms->coeff->interp_coeff[interp_period]);
				break;
				case 21: /* PC=10, B cycle, nothing happens (update K9) */
				break;
				case 22: /* PC=11, A cycle, update K9 (calc K10) */
				tms->current_k[8] += ((tms->target_k[8] - tms->current_k[8]) >> tms->coeff->interp_coeff[interp_period]);
				break;
				case 23: /* PC=11, B cycle, nothing happens (update K10) */
				break;
				case 24: /* PC=12, A cycle, update K10 (do nothing) */
				tms->current_k[9] += ((tms->target_k[9] - tms->current_k[9]) >> tms->coeff->interp_coeff[interp_period]);
				break;
			}
		}


		/* calculate the output */

		if (tms->current_energy == 0)
		{
			/* generate silent samples here */
			current_val = 0x00;
		}
		else if (tms->old_pitch == 0)
		{
			/* generate unvoiced samples here */
			if (tms->RNG&1)
				current_val = -64; /* according to the patent it is (either + or -) half of the maximum value in the chirp table */
			else
				current_val = 64;

		}
		else
		{
	                 /* generate voiced samples here */
            /* US patent 4331836 Figure 14B shows, and logic would hold, that a pitch based chirp
             * function has a chirp/peak and then a long chain of zeroes.
             * The last entry of the chirp rom is at address 0b110011 (50d), the 51st sample,
             * and if the address reaches that point the ADDRESS incrementer is
             * disabled, forcing all samples beyond 50d to be == 50d
             * (address 50d holds zeroes)
             */

	  /*if (tms->coeff->subtype & (SUBTYPE_TMS5100 | SUBTYPE_M58817))*/

		if (tms->pitch_count > 50)
			current_val = tms->coeff->chirptable[50];
		else
			current_val = tms->coeff->chirptable[tms->pitch_count];
		}

		/* Update LFSR *20* times every sample, like patent shows */
		for (i=0; i<20; i++)
		{
			bitout = ((tms->RNG>>12)&1) ^
				 ((tms->RNG>>10)&1) ^
				 ((tms->RNG>> 9)&1) ^
				 ((tms->RNG>> 0)&1);
			tms->RNG >>= 1;
			tms->RNG |= (bitout<<12);
		}

		/* Lattice filter here */

		Y11 = (current_val * 64 * tms->current_energy) / 512;

		for (i = tms->coeff->num_k - 1; i >= 0; i--)
		{
			Y11 = Y11 - ((tms->current_k[i] * tms->x[i]) / 512);
			tms->x[i+1] = tms->x[i] + ((tms->current_k[i] * Y11) / 512);
		}

		tms->x[0] = Y11;


		/* clipping & wrapping, just like the patent */

		/* YL10 - YL4 ==> DA6 - DA0 */
		cliptemp = Y11 / 16;

		/* M58817 seems to be different */
		if (tms->coeff->subtype & (SUBTYPE_M58817))
			cliptemp = cliptemp / 2;

		if (cliptemp > 511) cliptemp = -512 + (cliptemp-511);
		else if (cliptemp < -512) cliptemp = 511 - (cliptemp+512);

		if (cliptemp > 127)
			buffer[buf_count] = 127*256;
		else if (cliptemp < -128)
			buffer[buf_count] = -128*256;
		else
			buffer[buf_count] = cliptemp *256;

		/* Update all counts */

		tms->sample_count = (tms->sample_count + 1) % 200;

		if (tms->current_pitch != 0)
		{
			tms->pitch_count++;
			if (tms->pitch_count >= tms->current_pitch)
				tms->pitch_count = 0;
		}
		else
			tms->pitch_count = 0;

		tms->interp_count = (tms->interp_count + 1) % 25;

		buf_count++;
		size--;
	}

empty:

	while (size > 0)
	{
		tms->sample_count = (tms->sample_count + 1) % 200;
		tms->interp_count = (tms->interp_count + 1) % 25;

		buffer[buf_count] = 0x00;
		buf_count++;
		size--;
	}
}

void tms5110_set_frequency(UINT32 freq)
{
	our_chip->our_freq = freq/80;

	stream.set_rate(our_chip->our_freq);
}

void tms5110_update(INT16 *output, INT32 samples_len)
{
	if (samples_len != nBurnSoundLen) {
		bprintf(0, _T("tms5110_update(): once per frame, please!\n"));
		return;
	}

	stream.render(output, samples_len);
}


/******************************************************************************************

     PDC_set -- set Processor Data Clock. Execute CTL_pins command on hi-lo transition.

******************************************************************************************/

void tms5110_PDC_set_internal(tms5110_state *tms, int data)
{
	if (tms->PDC != (data & 0x1) )
	{
		tms->PDC = data & 0x1;
		if (tms->PDC == 0) /* toggling 1->0 processes command on CTL_pins */
		{
			/* first pdc toggles output, next toggles input */
			switch (tms->state)
			{
			case CTL_STATE_INPUT:
				/* continue */
				break;
			case CTL_STATE_NEXT_OUTPUT:
				tms->state = CTL_STATE_OUTPUT;
				return;
			case CTL_STATE_OUTPUT:
				tms->state = CTL_STATE_INPUT;
				return;
			}
			/* the only real commands we handle now are SPEAK and RESET */
			if (tms->next_is_address)
			{
				tms->next_is_address = FALSE;
				tms->address = tms->address | ((tms->CTL_pins & 0x0F)<<tms->addr_bit);
				tms->addr_bit = (tms->addr_bit + 4) % 12;
				tms->schedule_dummy_read = TRUE;
				if (tms->set_load_address)
					tms->set_load_address(tms->address);
			}
			else
			{
				switch (tms->CTL_pins & 0xe) /*CTL1 - don't care*/
				{
				case TMS5110_CMD_SPEAK:
					perform_dummy_read(tms);
					tms->speaking_now = 1;

					//should FIFO be cleared now ?????
					break;

				case TMS5110_CMD_RESET:
					perform_dummy_read(tms);
					tms5110_reset_chip(tms);
					break;

				case TMS5110_CMD_READ_BIT:
					if (tms->schedule_dummy_read)
						perform_dummy_read(tms);
					else
					{
						request_bits(tms, 1);
						tms->CTL_pins = (tms->CTL_pins & 0x0E) | extract_bits(tms, 1);
					}
					break;

				case TMS5110_CMD_LOAD_ADDRESS:
					tms->next_is_address = TRUE;
					break;

				case TMS5110_CMD_TEST_TALK:
					tms->state = CTL_STATE_NEXT_OUTPUT;
					break;

				default:
					logerror("tms5110.c: unknown command: 0x%02x\n", tms->CTL_pins);
					break;
				}

			}
		}
	}
}



/******************************************************************************************

     parse_frame -- parse a new frame's worth of data; returns 0 if not enough bits in buffer

******************************************************************************************/

static void parse_frame(tms5110_state *tms)
{
	int bits, indx, i, rep_flag;
#if (DEBUG_5110)
	int ene;
#endif

	/* count the total number of bits available */
	bits = tms->fifo_count;


	/* attempt to extract the energy index */
	bits -= tms->coeff->energy_bits;
	if (bits < 0)
	{
		request_bits( tms,-bits ); /* toggle M0 to receive needed bits */
		bits = 0;
	}
	indx = extract_bits(tms,tms->coeff->energy_bits);
	tms->new_energy = tms->coeff->energytable[indx];
#if (DEBUG_5110)
	ene = indx;
#endif

	/* if the energy index is 0 or 15, we're done */

	if ((indx == 0) || (indx == 15))
	{
		if (DEBUG_5110) logerror("  (4-bit energy=%d frame)\n",tms->new_energy);

	/* clear the k's */
		if (indx == 0)
		{
			for (i = 0; i < tms->coeff->num_k; i++)
				tms->new_k[i] = 0;
		}

		/* clear fifo if stop frame encountered */
		if (indx == 15)
		{
			if (DEBUG_5110) logerror("  (4-bit energy=%d STOP frame)\n",tms->new_energy);
			tms->fifo_head = tms->fifo_tail = tms->fifo_count = 0;
		}
		return;
	}


	/* attempt to extract the repeat flag */
	bits -= 1;
	if (bits < 0)
	{
		request_bits( tms,-bits ); /* toggle M0 to receive needed bits */
		bits = 0;
	}
	rep_flag = extract_bits(tms,1);

	/* attempt to extract the pitch */
	bits -= tms->coeff->pitch_bits;
	if (bits < 0)
	{
		request_bits( tms,-bits ); /* toggle M0 to receive needed bits */
		bits = 0;
	}
	indx = extract_bits(tms,tms->coeff->pitch_bits);
	tms->new_pitch = tms->coeff->pitchtable[indx];

	/* if this is a repeat frame, just copy the k's */
	if (rep_flag)
	{
	//actually, we do nothing because the k's were already loaded (on parsing the previous frame)

		if (DEBUG_5110) logerror("  (10-bit energy=%d pitch=%d rep=%d frame)\n", tms->new_energy, tms->new_pitch, rep_flag);
		return;
	}


	/* if the pitch index was zero, we need 4 k's */
	if (indx == 0)
	{
		/* attempt to extract 4 K's */
		bits -= 18;
		if (bits < 0)
		{
		request_bits( tms,-bits ); /* toggle M0 to receive needed bits */
		bits = 0;
		}
		for (i = 0; i < 4; i++)
			tms->new_k[i] = tms->coeff->ktable[i][extract_bits(tms,tms->coeff->kbits[i])];

	/* and clear the rest of the new_k[] */
		for (i = 4; i < tms->coeff->num_k; i++)
			tms->new_k[i] = 0;

		if (DEBUG_5110) logerror("  (28-bit energy=%d pitch=%d rep=%d 4K frame)\n", tms->new_energy, tms->new_pitch, rep_flag);
		return;
	}

	/* else we need 10 K's */
	bits -= 39;
	if (bits < 0)
	{
			request_bits( tms,-bits ); /* toggle M0 to receive needed bits */
		bits = 0;
	}
#if (DEBUG_5110)
	printf("FrameDump %02d ", ene);
	for (i = 0; i < tms->coeff->num_k; i++)
	{
		int x;
		x = extract_bits(tms, tms->coeff->kbits[i]);
		tms->new_k[i] = tms->coeff->ktable[i][x];
		printf("%02d ", x);
	}
	printf("\n");
#else
	for (i = 0; i < tms->coeff->num_k; i++)
	{
		int x;
		x = extract_bits(tms, tms->coeff->kbits[i]);
		tms->new_k[i] = tms->coeff->ktable[i][x];
	}
#endif
	if (DEBUG_5110) logerror("  (49-bit energy=%d pitch=%d rep=%d 10K frame)\n", tms->new_energy, tms->new_pitch, rep_flag);

}



#if 0
/*This is an example word TEN taken from the TMS5110A datasheet*/
static const unsigned int example_word_TEN[619]={
/* 1*/1,0,0,0,	0,	0,0,0,0,0,	1,1,0,0,0,	0,0,0,1,0,	0,1,1,1,	0,1,0,1,
/* 2*/1,0,0,0,	0,	0,0,0,0,0,	1,0,0,1,0,	0,0,1,1,0,	0,0,1,1,	0,1,0,1,
/* 3*/1,1,0,0,	0,	1,0,0,0,0,	1,0,1,0,0,	0,1,0,1,0,	0,1,0,0,	1,0,1,0,	1,0,0,0,	1,0,0,1,	0,1,0,1,	0,0,1,	0,1,0,	0,1,1,
/* 4*/1,1,1,0,	0,	0,1,1,1,1,	1,0,1,0,1,	0,1,1,1,0,	0,1,0,1,	0,1,1,1,	0,1,1,1,	1,0,1,1,	1,0,1,0,	0,1,1,	0,1,0,	0,1,1,
/* 5*/1,1,1,0,	0,	1,0,0,0,0,	1,0,1,0,0,	0,1,1,1,0,	0,1,0,1,	1,0,1,0,	1,0,0,0,	1,1,0,0,	1,0,1,1,	1,0,0,	0,1,0,	0,1,1,
/* 6*/1,1,1,0,	0,	1,0,0,0,1,	1,0,1,0,1,	0,1,1,0,1,	0,1,1,0,	0,1,1,1,	0,1,1,1,	1,0,1,0,	1,0,1,0,	1,1,0,	0,0,1,	1,0,0,
/* 7*/1,1,1,0,	0,	1,0,0,1,0,	1,0,1,1,1,	0,1,1,1,0,	0,1,1,1,	0,1,1,1,	0,1,0,1,	0,1,1,0,	1,0,0,1,	1,1,0,	0,1,0,	0,1,1,
/* 8*/1,1,1,0,	1,	1,0,1,0,1,
/* 9*/1,1,1,0,	0,	1,1,0,0,1,	1,0,1,1,1,	0,1,0,1,1,	1,0,1,1,	0,1,1,1,	0,1,0,0,	1,0,0,0,	1,0,0,0,	1,1,0,	0,1,1,	0,1,1,
/*10*/1,1,0,1,	0,	1,1,0,1,0,	1,0,1,0,1,	0,1,1,0,1,	1,0,1,1,	0,1,0,1,	0,1,0,0,	1,0,0,0,	1,0,1,0,	1,1,0,	0,1,0,	1,0,0,
/*11*/1,0,1,1,	0,	1,1,0,1,1,	1,0,0,1,1,	1,0,0,1,0,	0,1,1,0,	0,0,1,1,	0,1,0,1,	1,0,0,1,	1,0,1,0,	1,0,0,	0,1,1,	0,1,1,
/*12*/1,0,0,0,	0,	1,1,1,0,0,	1,0,0,1,1,	0,0,1,1,0,	0,1,0,0,	0,1,1,0,	1,1,0,0,	0,1,0,1,	1,0,0,0,	1,0,0,	0,1,0,	1,0,1,
/*13*/0,1,1,1,	1,	1,1,1,0,1,
/*14*/0,1,1,1,	0,	1,1,1,1,0,	1,0,0,1,1,	0,0,1,1,1,	0,1,0,1,	0,1,0,1,	1,1,0,0,	0,1,1,1,	1,0,0,0,	1,0,0,	0,1,0,	1,0,1,
/*15*/0,1,1,0,	0,	1,1,1,1,0,	1,0,1,0,1,	0,0,1,1,0,	0,1,0,0,	0,0,1,1,	1,1,0,0,	1,0,0,1,	0,1,1,1,	1,0,1,	0,1,0,	1,0,1,
/*16*/1,1,1,1
};
#endif


static int speech_rom_read_bit()
{
	tms5110_state *tms = our_chip;

	int r;

	if (tms->speech_rom_bitnum<0)
		r = 0;
	else
		r = (tms->table[tms->speech_rom_bitnum >> 3] >> (0x07 - (tms->speech_rom_bitnum & 0x07))) & 1;

	tms->speech_rom_bitnum++;

	return r;
}

static void speech_rom_set_addr(int addr)
{
	tms5110_state *tms = our_chip;

	tms->speech_rom_bitnum = addr * 8 - 1;
}

/******************************************************************************

     DEVICE_START( tms5110 ) -- allocate buffers and reset the 5110

******************************************************************************/

void tms5110_set_M0_callback(INT32 (*func)())
{
	our_chip->M0_callback = func;
}

void tms5110_set_load_address(void (*func)(int))
{
	our_chip->set_load_address = func;
}

void tms5110_init(INT32 freq, UINT8 *rom)
{
	struct tms5110_state *tms;

	our_chip = tms = (tms5110_state *)malloc(sizeof(*tms));
	memset(tms, 0, sizeof(*tms));

	our_chip->our_freq = freq/80;

	// init stream/resampler
	stream.init(our_chip->our_freq, nBurnSoundRate, 1, 1, tms5110_process);
    stream.set_volume(1.00);

	tms5110_initted = 1;

	tms->table = rom;

	tms5110_set_cvariant(tms, TMS5110_IS_5110A);

	if (tms->table == NULL)
	{
		bprintf(0, _T("tms5100: loading data via M0\n"));
	}
	else
	{
		bprintf(0, _T("tms5100: loading data via rom\n"));
	    tms->M0_callback = speech_rom_read_bit;
	    tms->set_load_address = speech_rom_set_addr;
	}

	tms->state = CTL_STATE_INPUT; /* most probably not defined */
	//tms->romclk_timer = timer_alloc(device->machine, romclk_timer_cb, (void *) device);
}

void tms5110_set_variant(INT32 vari)
{
	tms5110_set_cvariant(our_chip, vari);
}

void tms5110_exit()
{
	if (tms5110_initted == 0) {
		bprintf(0, _T("Warning: tms5110_exit() called without init!\n"));
		return;
	}

	free(our_chip);

	tms5110_initted = 0;

	stream.exit();
}

/*
static DEVICE_START( tms5100 )
{
	tms5110_state *tms = get_safe_token(device);
	DEVICE_START_CALL( tms5110 );
	tms5110_set_variant(tms, TMS5110_IS_5100);
}

static DEVICE_START( tms5110a )
{
	tms5110_state *tms = get_safe_token(device);
	DEVICE_START_CALL( tms5110 );
	tms5110_set_variant(tms, TMS5110_IS_5110A);
}

static DEVICE_START( cd2801 )
{
	tms5110_state *tms = get_safe_token(device);
	DEVICE_START_CALL( tms5110 );
	tms5110_set_variant(tms, TMS5110_IS_CD2801);
}

static DEVICE_START( tmc0281 )
{
	tms5110_state *tms = get_safe_token(device);
	DEVICE_START_CALL( tms5110 );
	tms5110_set_variant(tms, TMS5110_IS_TMC0281);
}

static DEVICE_START( cd2802 )
{
	tms5110_state *tms = get_safe_token(device);
	DEVICE_START_CALL( tms5110 );
	tms5110_set_variant(tms, TMS5110_IS_CD2802);
}

static DEVICE_START( m58817 )
{
	tms5110_state *tms = get_safe_token(device);
	DEVICE_START_CALL( tms5110 );
	tms5110_set_variant(tms, TMS5110_IS_M58817);
}
*/

static void tms5110_reset_chip(tms5110_state *tms)
{
	/* initialize the FIFO */
	memset(tms->fifo, 0, sizeof(tms->fifo));
	tms->fifo_head = tms->fifo_tail = tms->fifo_count = 0;

	/* initialize the chip state */
	tms->speaking_now = tms->speak_delay_frames = tms->talk_status = 0;
	tms->CTL_pins = 0;
		tms->RNG = 0x1fff;

	/* initialize the energy/pitch/k states */
	tms->old_energy = tms->new_energy = tms->current_energy = tms->target_energy = 0;
	tms->old_pitch = tms->new_pitch = tms->current_pitch = tms->target_pitch = 0;
	memset(tms->old_k, 0, sizeof(tms->old_k));
	memset(tms->new_k, 0, sizeof(tms->new_k));
	memset(tms->current_k, 0, sizeof(tms->current_k));
	memset(tms->target_k, 0, sizeof(tms->target_k));

	/* initialize the sample generators */
	tms->interp_count = tms->sample_count = tms->pitch_count = 0;
	memset(tms->x, 0, sizeof(tms->x));
	tms->next_is_address = FALSE;
	tms->address = 0;
	tms->schedule_dummy_read = TRUE;
	tms->addr_bit = 0;
}

void tms5110_reset()
{
	tms5110_reset_chip(our_chip);
}


/******************************************************************************

     tms5110_ctl_w -- write Control Command to the sound chip
commands like Speech, Reset, etc., are loaded into the chip via the CTL pins

******************************************************************************/

void tms5110_CTL_set(UINT8 data)
{
	stream.update();
	our_chip->CTL_pins = data & 0xf;
}


/******************************************************************************

     tms5110_pdc_w -- write to PDC pin on the sound chip

******************************************************************************/

void tms5110_PDC_set(UINT8 data)
{
	stream.update();
	tms5110_PDC_set_internal(our_chip, data);
}


/******************************************************************************

     tms5110_ctl_r -- read status from the sound chip

        bit 0 = TS - Talk Status is active (high) when the VSP is processing speech data.
                Talk Status goes active at the initiation of a SPEAK command.
                It goes inactive (low) when the stop code (Energy=1111) is processed, or
                immediately(?????? not TMS5110) by a RESET command.
        TMS5110 datasheets mention this is only available as a result of executing
                TEST TALK command.

                FIXME: data read not implemented, CTL1 only available after TALK command

******************************************************************************/

INT32 tms5110_ctl_read()
{
    /* bring up to date first */
    stream.update();
    if (our_chip->state == CTL_STATE_OUTPUT)
    {
		//if (DEBUG_5110) logerror("Status read (status=%2d)\n", tms->talk_status);
    	return (our_chip->talk_status << 0); /*CTL1 = still talking ? */
    }
    else
    {
		//if (DEBUG_5110) logerror("Status read (not in output mode)\n");
    	return (0);
    }
}

INT32 m58817_status_read()
{
    /* bring up to date first */
    stream.update();
    return (our_chip->talk_status << 0); /*CTL1 = still talking ? */
}

/******************************************************************************

     tms5110_romclk_r -- read status of romclk

******************************************************************************/
#if 0
static TIMER_CALLBACK( romclk_timer_cb )
{
	tms5110_state *tms = get_safe_token((running_device *) ptr);
	tms->romclk_state = !tms->romclk_state;
}

READ8_DEVICE_HANDLER( tms5110_romclk_r )
{
	tms5110_state *tms = get_safe_token(device);

    /* bring up to date first */
	stream.update();

    /* create and start timer if necessary */
    if (!tms->romclk_timer_started)
    {
    	tms->romclk_timer_started = TRUE;
		timer_adjust_periodic(tms->romclk_timer, ATTOTIME_IN_HZ(device->clock / 40), 0, ATTOTIME_IN_HZ(device->clock / 40));
	}
    return tms->romclk_state;
}
#endif


/******************************************************************************

     tms5110_ready_r -- return the not ready status from the sound chip

******************************************************************************/

INT32 tms5110_ready_read()
{
    /* bring up to date first */
    stream.update();
    return (our_chip->fifo_count < FIFO_SIZE-1);
}



/******************************************************************************

     tms5110_update -- update the sound chip so that it is in sync with CPU execution

******************************************************************************/
#if 0
static STREAM_UPDATE( tms5110_update )
{
	tms5110_state *tms = (tms5110_state *)param;
	INT16 sample_data[MAX_SAMPLE_CHUNK];
	stream_sample_t *buffer = outputs[0];

	/* loop while we still have samples to generate */
	while (samples)
	{
		int length = (samples > MAX_SAMPLE_CHUNK) ? MAX_SAMPLE_CHUNK : samples;
		int index;

		/* generate the samples and copy to the target buffer */
		tms5110_process(tms, sample_data, length);
		for (index = 0; index < length; index++)
			*buffer++ = sample_data[index];

		/* account for the samples */
		samples -= length;
	}
}
#endif

