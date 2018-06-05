/**********************************************************************************************

     TMS5110 simulator (modified from TMS5220 by Jarek Burczynski)

     Written for MAME by Frank Palazzolo
     With help from Neill Corlett
     Additional tweaking by Aaron Giles

***********************************************************************************************/

#include <stddef.h>
#include <math.h>

#include "burnint.h"
#include "tms5110.h"

/* Pull in the ROM tables */
#include "tms5110_tables.h"

#define logerror

struct tms5110
{
	/* these contain data that describes the 64 bits FIFO */
	#define FIFO_SIZE 64
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

	INT32 u[11];
	INT32 x[10];

	INT32 RNG;	/* the random noise generator configuration is: 1 + x + x^3 + x^4 + x^13 */

	INT32 (*M0_callback)();
};


/* Static function prototypes */
static void parse_frame(struct tms5110 *tms);

#define DEBUG_5110	0

static struct tms5110 *our_chip = NULL;
static INT32 tms5110_initted = 0;
static INT16 *soundbuf = NULL;
static INT32 samples_from; // "re"sampler

static void *tms5110_create(void)
{
	struct tms5110 *tms;

	tms = (tms5110 *)malloc(sizeof(*tms));
	memset(tms, 0, sizeof(*tms));
	return tms;
}

static void tms5110_destroy(void *chip)
{
	free(chip);
}

void tms5110_init(UINT32 freq)
{
	our_chip = (tms5110 *)tms5110_create();
	soundbuf = (INT16*)malloc(0x800);

	tms5110_set_frequency(freq);
	tms5110_initted = 1;
}

void tms5110_exit()
{
	if (tms5110_initted == 0) {
		bprintf(0, _T("Warning: tms5110_exit() called without init!\n"));
		return;
	}

	tms5110_destroy(our_chip);
	free(soundbuf);

	tms5110_initted = 0;
}

void tms5110_scan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (nAction & ACB_DRIVER_DATA) {
		memset(&ba, 0, sizeof(ba));
		ba.Data		= our_chip;
		ba.nLen		= STRUCT_SIZE_HELPER(tms5110, M0_callback);
		ba.szName	= "TMS5110 SpeechSynth Chip";
		BurnAcb(&ba);
	}
}


/**********************************************************************************************

     tms5110_reset -- resets the TMS5110

***********************************************************************************************/

static void tms5110_reset_chip(void *chip)
{
	struct tms5110 *tms = (tms5110 *)chip;

    /* initialize the FIFO */
    memset(tms->fifo, 0, sizeof(tms->fifo));
    tms->fifo_head = tms->fifo_tail = tms->fifo_count = 0;

    /* initialize the chip state */
    tms->speaking_now = tms->speak_delay_frames = tms->talk_status = 0;
    tms->CTL_pins = 0;
	tms->RNG = 0xfff;

    /* initialize the energy/pitch/k states */
    tms->old_energy = tms->new_energy = tms->current_energy = tms->target_energy = 0;
    tms->old_pitch = tms->new_pitch = tms->current_pitch = tms->target_pitch = 0;
    memset(tms->old_k, 0, sizeof(tms->old_k));
    memset(tms->new_k, 0, sizeof(tms->new_k));
    memset(tms->current_k, 0, sizeof(tms->current_k));
    memset(tms->target_k, 0, sizeof(tms->target_k));

    /* initialize the sample generators */
    tms->interp_count = tms->sample_count = tms->pitch_count = 0;
    memset(tms->u, 0, sizeof(tms->u));
    memset(tms->x, 0, sizeof(tms->x));
}

void tms5110_reset()
{
	tms5110_reset_chip(our_chip);
}


/******************************************************************************************

     tms5110_set_M0_callback -- set M0 callback for the TMS5110

******************************************************************************************/

static void tms5110_set_M0_callback(void *chip, INT32 (*func)())
{
	struct tms5110 *tms = (tms5110 *)chip;
    tms->M0_callback = func;
}

void tms5110_M0_callback(INT32 (*func)())
{
	tms5110_set_M0_callback(our_chip, func);
}

/******************************************************************************************

     FIFO_data_write -- handle bit data write to the TMS5110 (as a result of toggling M0 pin)

******************************************************************************************/
static void FIFO_data_write(struct tms5110 *tms, INT32 data)
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

static INT32 extract_bits(struct tms5110 *tms, INT32 count)
{
    INT32 val = 0;

    while (count--)
    {
        val = (val << 1) | (tms->fifo[tms->fifo_head] & 1);
        tms->fifo_count--;
        tms->fifo_head = (tms->fifo_head + 1) % FIFO_SIZE;
    }
    return val;
}

static void request_bits(struct tms5110 *tms, INT32 no)
{
	for (INT32 i=0; i<no; i++)
	{
		if (tms->M0_callback)
		{
			INT32 data = (*tms->M0_callback)();
			FIFO_data_write(tms, data);
		}
		else
			if (DEBUG_5110) logerror("-->ERROR: TMS5110 missing M0 callback function\n");
	}
}

static void perform_dummy_read(struct tms5110 *tms)
{
	if (tms->M0_callback)
	{
		INT32 data = (*tms->M0_callback)();
	        if (DEBUG_5110) logerror("TMS5110 performing dummy read; value read = %1i\n", data&1);
	}
    	else
	        if (DEBUG_5110) logerror("-->ERROR: TMS5110 missing M0 callback function\n");
}

/**********************************************************************************************

     tms5110_status_read -- read status from the TMS5110

        bit 0 = TS - Talk Status is active (high) when the VSP is processing speech data.
                Talk Status goes active at the initiation of a SPEAK command.
                It goes inactive (low) when the stop code (Energy=1111) is processed, or
                immediately(?????? not TMS5110) by a RESET command.
        TMS5110 datasheets mention this is only available as a result of executing
                TEST TALK command.


***********************************************************************************************/

static INT32 tms5110_status_read_internal(void *chip)
{
	struct tms5110 *tms = (tms5110 *)chip;
    if (DEBUG_5110) logerror("Status read: TS=%d\n", tms->talk_status);

    return (tms->talk_status << 0); /*CTL1 = still talking ? */
}

INT32 tms5110_status_read()
{
	 return tms5110_status_read_internal(our_chip);
}

/**********************************************************************************************

     tms5110_ready_read -- returns the ready state of the TMS5110

***********************************************************************************************/

static INT32 tms5110_ready_read_internal(void *chip)
{
	struct tms5110 *tms = (tms5110 *)chip;
    return (tms->fifo_count < FIFO_SIZE-1);
}

INT32 tms5110_ready_read()
{
	 return tms5110_ready_read_internal(our_chip);
}


/**********************************************************************************************

     tms5110_process -- fill the buffer with a specific number of samples

***********************************************************************************************/

void tms5110_process(void *chip, INT16 *buffer, INT32 size)
{
	struct tms5110 *tms = (tms5110 *)chip;
    INT32 buf_count=0;
    INT32 i, interp_period;

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
    */
		perform_dummy_read(tms);

        /* clear out the new frame parameters (it will become old frame just before the first call to parse_frame() ) */
        tms->new_energy = 0;
        tms->new_pitch = 0;
        for (i = 0; i < 10; i++)
            tms->new_k[i] = 0;

        tms->talk_status = 1;
    }


    /* loop until the buffer is full or we've stopped speaking */
    while ((size > 0) && tms->speaking_now)
    {
        INT32 current_val;

        /* if we're ready for a new frame */
        if ((tms->interp_count == 0) && (tms->sample_count == 0))
        {

            /* remember previous frame */
            tms->old_energy = tms->new_energy;
            tms->old_pitch = tms->new_pitch;
            for (i = 0; i < 10; i++)
                tms->old_k[i] = tms->new_k[i];


            /* if the old frame was a stop frame, exit and do not process any more frames */
            if (tms->old_energy == energytable[15])
            {
                /*if (DEBUG_5110) logerror("processing frame: stop frame\n");*/
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
            for (i = 0; i < 10; i++)
                tms->current_k[i] = tms->old_k[i];


            /* is this the stop (ramp down) frame? */
            if (tms->new_energy == energytable[15])
            {
                /*logerror("processing frame: ramp down\n");*/
                tms->target_energy = 0;
                tms->target_pitch = tms->old_pitch;
                for (i = 0; i < 10; i++)
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
                for (i = 0; i < 10; i++)
                    tms->target_k[i] = tms->current_k[i] = tms->new_k[i];
            }
            else if ((tms->old_pitch == 0) && (tms->new_pitch != 0))    /* is this a change from unvoiced to voiced frame ? */
            {
                /* if so, then the new parameters should become our current and target parameters immediately,
                   i.e. we should NOT interpolate them slowly in.
                */
                /*if (DEBUG_5110) logerror("processing frame: UNVOICED->VOICED frame change\n");*/
                tms->target_energy = tms->new_energy;
                tms->target_pitch = tms->current_pitch = tms->new_pitch;
                for (i = 0; i < 10; i++)
                    tms->target_k[i] = tms->current_k[i] = tms->new_k[i];
            }
            else if ((tms->old_pitch != 0) && (tms->new_pitch == 0))    /* is this a change from voiced to unvoiced frame ? */
            {
                /* if so, then the new parameters should become our current and target parameters immediately,
                   i.e. we should NOT interpolate them slowly in.
                */
                /*if (DEBUG_5110) logerror("processing frame: VOICED->UNVOICED frame change\n");*/
                tms->target_energy = tms->new_energy;
                tms->target_pitch = tms->current_pitch = tms->new_pitch;
                for (i = 0; i < 10; i++)
                    tms->target_k[i] = tms->current_k[i] = tms->new_k[i];
            }
            else
            {
                /*logerror("processing frame: Normal\n");*/
                /*logerror("*** Energy = %d\n",current_energy);*/
                /*logerror("proc: %d %d\n",last_fbuf_head,fbuf_head);*/

                tms->target_energy = tms->new_energy;
                tms->target_pitch = tms->new_pitch;
                for (i = 0; i < 10; i++)
                    tms->target_k[i] = tms->new_k[i];
            }
        }
        else if (tms->interp_count == 0)
        {
            /* interpolate (update) values based on step values */
            /*logerror("\n");*/

            interp_period = tms->sample_count / 25;
            tms->current_energy += (tms->target_energy - tms->current_energy) / interp_coeff[interp_period];
            tms->current_pitch += (tms->target_pitch - tms->current_pitch) / interp_coeff[interp_period];

            /*logerror("*** Energy = %d\n",current_energy);*/

            for (i = 0; i < 10; i++)
            {
                tms->current_k[i] += (tms->target_k[i] - tms->current_k[i]) / interp_coeff[interp_period];
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
            INT32 bitout, randbit;

            /* generate unvoiced samples here */
            if (tms->RNG&1)
                randbit = -64; /* according to the patent it is (either + or -) half of the maximum value in the chirp table */
            else
                randbit = 64;

            bitout = ((tms->RNG>>12)&1) ^
                     ((tms->RNG>>10)&1) ^
                     ((tms->RNG>> 9)&1) ^
                     ((tms->RNG>> 0)&1);
            tms->RNG >>= 1;
            tms->RNG |= (bitout<<12);

            current_val = (randbit * tms->current_energy) / 256;
        }
        else
        {
            /* generate voiced samples here */
            if (tms->pitch_count < sizeof(chirptable))
                current_val = (chirptable[tms->pitch_count] * tms->current_energy) / 256;
            else
                current_val = 0x00;
        }


        /* Lattice filter here */

        tms->u[10] = current_val;

        for (i = 9; i >= 0; i--)
        {
            tms->u[i] = tms->u[i+1] - ((tms->current_k[i] * tms->x[i]) / 32768);
        }
        for (i = 9; i >= 1; i--)
        {
            tms->x[i] = tms->x[i-1] + ((tms->current_k[i-1] * tms->u[i-1]) / 32768);
        }

        tms->x[0] = tms->u[0];


        /* clipping, just like the chip */

        if (tms->u[0] > 511)
            buffer[buf_count] = 127<<8;
        else if (tms->u[0] < -512)
            buffer[buf_count] = -128<<8;
        else
            buffer[buf_count] = tms->u[0] << 6;


        /* Update all counts */

        tms->sample_count = (tms->sample_count + 1) % 200;

        if (tms->current_pitch != 0)
            tms->pitch_count = (tms->pitch_count + 1) % tms->current_pitch;
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

static INT32 our_freq = 0;

void tms5110_set_frequency(UINT32 freq)
{
	our_freq = freq/80;
	samples_from = (INT32)((double)((our_freq * 100) / nBurnFPS) + 0.5);
}

void tms5110_update(INT16 *buffer, INT32 samples_len)
{
	INT32 samples = (samples_from * samples_len) / nBurnSoundLen;

	tms5110_process(our_chip, soundbuf, samples);

	INT16 *mix = soundbuf;

	for (INT32 j = 0; j < samples_len; j++)
	{
		INT32 k = (samples_from * j) / nBurnSoundLen;

		INT32 s = mix[k];
		buffer[0] = BURN_SND_CLIP(buffer[0] + s);
		buffer[1] = BURN_SND_CLIP(buffer[1] + s);
		buffer += 2;
	}

}


/******************************************************************************************

     CTL_set -- set CTL pins named CTL1, CTL2, CTL4 and CTL8

******************************************************************************************/

void tms5110_CTL_set_internal(void *chip, INT32 data)
{
	struct tms5110 *tms = (tms5110 *)chip;
	tms->CTL_pins = data & 0xf;
}

void tms5110_CTL_set(UINT8 data)
{
	tms5110_CTL_set_internal(our_chip, data);
}

/******************************************************************************************

     PDC_set -- set Processor Data Clock. Execute CTL_pins command on hi-lo transition.

******************************************************************************************/

void tms5110_PDC_set_internal(void *chip, INT32 data)
{
	struct tms5110 *tms = (tms5110 *)chip;
	if (tms->PDC != (data & 0x1) )
	{
		tms->PDC = data & 0x1;
		if (tms->PDC == 0) /* toggling 1->0 processes command on CTL_pins */
		{
			/* the only real commands we handle now are SPEAK and RESET */

			switch (tms->CTL_pins & 0xe) /*CTL1 - don't care*/
			{
			case TMS5110_CMD_SPEAK:
				tms->speaking_now = 1;

				//should FIFO be cleared now ?????

				break;

        		case TMS5110_CMD_RESET:
				tms->speaking_now = 0;
				tms->talk_status = 0;
				break;

			default:
				break;
			}
		}
	}
}

void tms5110_PDC_set(UINT8 data)
{
	tms5110_PDC_set_internal(our_chip, data);
}


/******************************************************************************************

     parse_frame -- parse a new frame's worth of data; returns 0 if not enough bits in buffer

******************************************************************************************/

static void parse_frame(struct tms5110 *tms)
{
    INT32 bits, indx, i, rep_flag;


    /* count the total number of bits available */
    bits = tms->fifo_count;


    /* attempt to extract the energy index */
    bits -= 4;
    if (bits < 0)
    {
        request_bits( tms,-bits ); /* toggle M0 to receive needed bits */
	bits = 0;
    }
    indx = extract_bits(tms,4);
    tms->new_energy = energytable[indx];


    /* if the energy index is 0 or 15, we're done */

    if ((indx == 0) || (indx == 15))
    {
        if (DEBUG_5110) logerror("  (4-bit energy=%d frame)\n",tms->new_energy);

	/* clear the k's */
        if (indx == 0)
        {
            for (i = 0; i < 10; i++)
                tms->new_k[i] = 0;
	}

        /* clear fifo if stop frame encountered */
        if (indx == 15)
        {
            if (DEBUG_5110) logerror("  (4-bit energy=%d STOP frame)\n",tms->new_energy);
            tms->fifo_head = tms->fifo_tail = tms->fifo_count = 0;
            //speaking_now = talk_status = 0;
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
    bits -= 5;
    if (bits < 0)
    {
        request_bits( tms,-bits ); /* toggle M0 to receive needed bits */
        bits = 0;
    }
    indx = extract_bits(tms,5);
    tms->new_pitch = pitchtable[indx];


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
        tms->new_k[0] = k1table[extract_bits(tms,5)];
        tms->new_k[1] = k2table[extract_bits(tms,5)];
        tms->new_k[2] = k3table[extract_bits(tms,4)];
        tms->new_k[3] = k4table[extract_bits(tms,4)];

	/* and clear the rest of the new_k[] */
        for (i = 4; i < 10; i++)
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
    tms->new_k[0] = k1table[extract_bits(tms,5)];
    tms->new_k[1] = k2table[extract_bits(tms,5)];
    tms->new_k[2] = k3table[extract_bits(tms,4)];
    tms->new_k[3] = k4table[extract_bits(tms,4)];
    tms->new_k[4] = k5table[extract_bits(tms,4)];
    tms->new_k[5] = k6table[extract_bits(tms,4)];
    tms->new_k[6] = k7table[extract_bits(tms,4)];
    tms->new_k[7] = k8table[extract_bits(tms,3)];
    tms->new_k[8] = k9table[extract_bits(tms,3)];
    tms->new_k[9] = k10table[extract_bits(tms,3)];

    if (DEBUG_5110) logerror("  (49-bit energy=%d pitch=%d rep=%d 10K frame)\n", tms->new_energy, tms->new_pitch, rep_flag);

}


#if 0
/*This is an example word TEN taken from the TMS5110A datasheet*/
static unsigned INT32 example_word_TEN[619]={
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

