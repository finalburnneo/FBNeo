/*****************************************************************************
 *
 *	POKEY chip emulator 4.3
 *	Copyright (c) 2000 by The MAME Team
 *
 *	Based on original info found in Ron Fries' Pokey emulator,
 *	with additions by Brad Oliver, Eric Smith and Juergen Buchmueller,
 *	paddle (a/d conversion) details from the Atari 400/800 Hardware Manual.
 *	Polynome algorithms according to info supplied by Perry McFarlane.
 *
 *	This code is subject to the MAME license, which besides other
 *  things means it is distributed as is, no warranties whatsoever.
 *	For more details read the readme.txt that comes with MAME.
 *
 *	4.3:
 *	- for POT inputs returning zero, immediately assert the ALLPOT
 *	  bit after POTGO is written, otherwise start trigger timer
 *	  depending on SK_PADDLE mode, either 1-228 scanlines or 1-2
 *	  scanlines, depending on the SK_PADDLE bit of SKCTL.
 *	4.2:
 *	- half volume for channels which are inaudible (this should be
 *	  close to the real thing).
 *	4.1:
 *	- default gain increased to closely match the old code.
 *	- random numbers repeat rate depends on POLY9 flag too!
 *	- verified sound output with many, many Atari 800 games,
 *	  including the SUPPRESS_INAUDIBLE optimizations.
 *	4.0:
 *	- rewritten from scratch.
 *	- 16bit stream interface.
 *	- serout ready/complete delayed interrupts.
 *	- reworked pot analog/digital conversion timing.
 *	- optional non-indexing pokey update functions.
 *
 *****************************************************************************/

#include "burnint.h"
#include "pokey.h"

/*
 * Defining this produces much more (about twice as much)
 * but also more efficient code. Ideally this should be set
 * for processors with big code cache and for healthy compilers :)
 */
#define HEAVY_MACRO_USAGE   0

#define SUPPRESS_INAUDIBLE	1

/* Four channels with a range of 0..32767 and volume 0..15 */
//#define POKEY_DEFAULT_GAIN (32767/15/4)

/*
 * But we raise the gain and risk clipping, the old Pokey did
 * this too. It defined POKEY_DEFAULT_GAIN 6 and this was
 * 6 * 15 * 4 = 360, 360/256 = 1.40625
 * I use 15/11 = 1.3636, so this is a little lower.
 */
#define POKEY_DEFAULT_GAIN (32767/11/4)
static double pokey_mastervol = 1.0;
static INT32 nLeftSample = 0, nRightSample = 0;

#define VERBOSE 		0
#define VERBOSE_SOUND	0
#define VERBOSE_TIMER	0
#define VERBOSE_POLY	0
#define VERBOSE_RAND	0

#if VERBOSE
#define LOG(x) if( errorlog ) fprintf x
#else
#define LOG(x)
#endif

#if VERBOSE_SOUND
#define LOG_SOUND(x) if( errorlog ) fprintf x
#else
#define LOG_SOUND(x)
#endif

#if VERBOSE_TIMER
#define LOG_TIMER(x) if( errorlog ) fprintf x
#else
#define LOG_TIMER(x)
#endif

#if VERBOSE_POLY
#define LOG_POLY(x) if( errorlog ) fprintf x
#else
#define LOG_POLY(x)
#endif

#if VERBOSE_RAND
#define LOG_RAND(x) if( errorlog ) fprintf x
#else
#define LOG_RAND(x)
#endif

#define CHAN1	0
#define CHAN2	1
#define CHAN3	2
#define CHAN4	3

#define TIMER1	0
#define TIMER2	1
#define TIMER4	2

/* values to add to the divisors for the different modes */
#define DIVADD_LOCLK		1
#define DIVADD_HICLK		4
#define DIVADD_HICLK_JOINED 7

/* AUDCx */
#define NOTPOLY5	0x80	/* selects POLY5 or direct CLOCK */
#define POLY4		0x40	/* selects POLY4 or POLY17 */
#define PURE		0x20	/* selects POLY4/17 or PURE tone */
#define VOLUME_ONLY 0x10	/* selects VOLUME OUTPUT ONLY */
#define VOLUME_MASK 0x0f	/* volume mask */

/* AUDCTL */
#define POLY9		0x80	/* selects POLY9 or POLY17 */
#define CH1_HICLK	0x40	/* selects 1.78979 MHz for Ch 1 */
#define CH3_HICLK	0x20	/* selects 1.78979 MHz for Ch 3 */
#define CH12_JOINED 0x10	/* clocks channel 1 w/channel 2 */
#define CH34_JOINED 0x08	/* clocks channel 3 w/channel 4 */
#define CH1_FILTER	0x04	/* selects channel 1 high pass filter */
#define CH2_FILTER	0x02	/* selects channel 2 high pass filter */
#define CLK_15KHZ	0x01	/* selects 15.6999 kHz or 63.9211 kHz */

/* IRQEN (D20E) */
#define IRQ_BREAK	0x80	/* BREAK key pressed interrupt */
#define IRQ_KEYBD	0x40	/* keyboard data ready interrupt */
#define IRQ_SERIN	0x20	/* serial input data ready interrupt */
#define IRQ_SEROR	0x10	/* serial output register ready interrupt */
#define IRQ_SEROC	0x08	/* serial output complete interrupt */
#define IRQ_TIMR4	0x04	/* timer channel #4 interrupt */
#define IRQ_TIMR2	0x02	/* timer channel #2 interrupt */
#define IRQ_TIMR1	0x01	/* timer channel #1 interrupt */

/* SKSTAT (R/D20F) */
#define SK_FRAME	0x80	/* serial framing error */
#define SK_OVERRUN	0x40	/* serial overrun error */
#define SK_KBERR	0x20	/* keyboard overrun error */
#define SK_SERIN	0x10	/* serial input high */
#define SK_SHIFT	0x08	/* shift key pressed */
#define SK_KEYBD	0x04	/* keyboard key pressed */
#define SK_SEROUT	0x02	/* serial output active */

/* SKCTL (W/D20F) */
#define SK_BREAK	0x80	/* serial out break signal */
#define SK_BPS		0x70	/* bits per second */
#define SK_FM		0x08	/* FM mode */
#define SK_PADDLE	0x04	/* fast paddle a/d conversion */
#define SK_RESET	0x03	/* reset serial/keyboard interface */

#define DIV_64		28		 /* divisor for 1.78979 MHz clock to 63.9211 kHz */
#define DIV_15		114 	 /* divisor for 1.78979 MHz clock to 15.6999 kHz */

struct POKEYregisters {
	INT32 counter[4];		/* channel counter */
	INT32 divisor[4];		/* channel divisor (modulo value) */
	UINT32 volume[4];		/* channel volume - derived */
	UINT8 output[4];		/* channel output signal (1 active, 0 inactive) */
	UINT8 audible[4];		/* channel plays an audible tone/effect */
	UINT32 samplerate_24_8; /* sample rate in 24.8 format */
	UINT32 samplepos_fract; /* sample position fractional part */
	UINT32 samplepos_whole; /* sample position whole part */
	UINT32 polyadjust;		/* polynome adjustment */
    UINT32 p4;              /* poly4 index */
    UINT32 p5;              /* poly5 index */
    UINT32 p9;              /* poly9 index */
    UINT32 p17;             /* poly17 index */
	UINT32 r9;				/* rand9 index */
    UINT32 r17;             /* rand17 index */
	UINT32 clockmult;		/* clock multiplier */
    int channel;            /* streams channel */
	void *timer[3]; 		/* timers for channel 1,2 and 4 events */
    void *rtimer;           /* timer for calculating the random offset */
	void *ptimer[8];		/* pot timers */
	int (*pot_r[8])(int offs);
	int (*allpot_r)(int offs);
	int (*serin_r)(int offs);
	void (*serout_w)(int offs, int data);
	void (*interrupt_cb)(int mask);
    UINT8 AUDF[4];          /* AUDFx (D200, D202, D204, D206) */
	UINT8 AUDC[4];			/* AUDCx (D201, D203, D205, D207) */
	UINT8 POTx[8];			/* POTx   (R/D200-D207) */
	UINT8 AUDCTL;			/* AUDCTL (W/D208) */
	UINT8 ALLPOT;			/* ALLPOT (R/D208) */
	UINT8 KBCODE;			/* KBCODE (R/D209) */
	UINT8 RANDOM;			/* RANDOM (R/D20A) */
	UINT8 SERIN;			/* SERIN  (R/D20D) */
	UINT8 SEROUT;			/* SEROUT (W/D20D) */
	UINT8 IRQST;			/* IRQST  (R/D20E) */
	UINT8 IRQEN;			/* IRQEN  (W/D20E) */
	UINT8 SKSTAT;			/* SKSTAT (R/D20F) */
	UINT8 SKCTL;			/* SKCTL  (W/D20F) */
};

static struct POKEYinterface intf;
static struct POKEYregisters pokey[MAXPOKEYS];

void pokey_scan(INT32 nAction, INT32* pnMin)
{
	if (pnMin && *pnMin < 0x029521) {
		*pnMin = 0x029521;
	}

	if (nAction & ACB_DRIVER_DATA) {
		for (INT32 i = 0; i < MAXPOKEYS; i++)
		{
			// save pointers.
			void *timer[3] = { pokey[i].timer[0], pokey[i].timer[1], pokey[i].timer[2] };
			void *rtimer = pokey[i].rtimer;
			void *ptimer[8] = { pokey[i].ptimer[0],pokey[i].ptimer[1],pokey[i].ptimer[2],pokey[i].ptimer[3],pokey[i].ptimer[4],pokey[i].ptimer[5],pokey[i].ptimer[6],pokey[i].ptimer[7] };
			int (*pot_r[8])(int offs) = { pokey[i].pot_r[0], pokey[i].pot_r[1], pokey[i].pot_r[2], pokey[i].pot_r[3], pokey[i].pot_r[4], pokey[i].pot_r[5], pokey[i].pot_r[6], pokey[i].pot_r[7] };
			int (*allpot_r)(int offs) = pokey[i].allpot_r;
			int (*serin_r)(int offs) = pokey[i].serin_r;
			void (*serout_w)(int offs, int data) = pokey[i].serout_w;
			void (*interrupt_cb)(int mask) = pokey[i].interrupt_cb;

			SCAN_VAR(pokey[i]);

			// restore pointers.
			pokey[i].timer[0] = timer[0]; pokey[i].timer[1] = timer[1]; pokey[i].timer[2] = timer[2];
			pokey[i].rtimer = rtimer;
			pokey[i].ptimer[0] = ptimer[0]; pokey[i].ptimer[1] = ptimer[1]; pokey[i].ptimer[2] = ptimer[2]; pokey[i].ptimer[3] = ptimer[3]; pokey[i].ptimer[4] = ptimer[4]; pokey[i].ptimer[5] = ptimer[5]; pokey[i].ptimer[6] = ptimer[6]; pokey[i].ptimer[7] = ptimer[7];
			pokey[i].pot_r[0] = pot_r[0]; pokey[i].pot_r[1] = pot_r[1]; pokey[i].pot_r[2] = pot_r[2]; pokey[i].pot_r[3] = pot_r[3]; pokey[i].pot_r[4] = pot_r[4]; pokey[i].pot_r[5] = pot_r[5]; pokey[i].pot_r[6] = pot_r[6]; pokey[i].pot_r[7] = pot_r[7];
			pokey[i].allpot_r = allpot_r;
			pokey[i].serin_r = serin_r;
			pokey[i].serout_w = serout_w;
			pokey[i].interrupt_cb = interrupt_cb;
		}
	}
}

static UINT8 poly4[0x0f];
static UINT8 poly5[0x1f];
static UINT8 *poly9;
static UINT8 *poly17;

#define P4(chip)  poly4[pokey[chip].p4]
#define P5(chip)  poly5[pokey[chip].p5]
#define P9(chip)  poly9[pokey[chip].p9]
#define P17(chip) poly17[pokey[chip].p17]

/* 128K random values derived from the 17bit polynome */
static UINT8 *rand9;
static UINT8 *rand17;

#define SAMPLE	-1

#define ADJUST_EVENT(chip)												\
	pokey[chip].counter[CHAN1] -= event;								\
	pokey[chip].counter[CHAN2] -= event;								\
	pokey[chip].counter[CHAN3] -= event;								\
	pokey[chip].counter[CHAN4] -= event;								\
	pokey[chip].samplepos_whole -= event;								\
	pokey[chip].polyadjust += event

#if SUPPRESS_INAUDIBLE

#define PROCESS_CHANNEL(chip,ch)                                        \
	int toggle = 0; 													\
	ADJUST_EVENT(chip); 												\
	/* reset the channel counter */ 									\
	if( pokey[chip].audible[ch] )										\
		pokey[chip].counter[ch] = pokey[chip].divisor[ch];				\
	else																\
		pokey[chip].counter[ch] = 0x7fffffff;							\
	pokey[chip].p4 = (pokey[chip].p4+pokey[chip].polyadjust)%0x0000f;	\
	pokey[chip].p5 = (pokey[chip].p5+pokey[chip].polyadjust)%0x0001f;	\
	pokey[chip].p9 = (pokey[chip].p9+pokey[chip].polyadjust)%0x001ff;	\
	pokey[chip].p17 = (pokey[chip].p17+pokey[chip].polyadjust)%0x1ffff; \
	pokey[chip].polyadjust = 0; 										\
	if( (pokey[chip].AUDC[ch] & NOTPOLY5) || P5(chip) ) 				\
	{																	\
		if( pokey[chip].AUDC[ch] & PURE )								\
			toggle = 1; 												\
		else															\
		if( pokey[chip].AUDC[ch] & POLY4 )								\
			toggle = pokey[chip].output[ch] == !P4(chip);				\
		else															\
		if( pokey[chip].AUDCTL & POLY9 )								\
			toggle = pokey[chip].output[ch] == !P9(chip);				\
		else															\
			toggle = pokey[chip].output[ch] == !P17(chip);				\
	}																	\
	if( toggle )														\
	{																	\
		if( pokey[chip].audible[ch] )									\
		{																\
			if( pokey[chip].output[ch] )								\
				sum -= pokey[chip].volume[ch];							\
			else														\
				sum += pokey[chip].volume[ch];							\
		}																\
		pokey[chip].output[ch] ^= 1;									\
	}																	\
	/* is this a filtering channel (3/4) and is the filter active? */	\
	if( pokey[chip].AUDCTL & ((CH1_FILTER|CH2_FILTER) & (0x10 >> ch)) ) \
    {                                                                   \
		if( pokey[chip].output[ch-2] )									\
        {                                                               \
			pokey[chip].output[ch-2] = 0;								\
			if( pokey[chip].audible[ch] )								\
				sum -= pokey[chip].volume[ch-2];						\
        }                                                               \
    }                                                                   \

#else

#define PROCESS_CHANNEL(chip,ch)                                        \
	int toggle = 0; 													\
	ADJUST_EVENT(chip); 												\
	/* reset the channel counter */ 									\
	pokey[chip].counter[ch] = p[chip].divisor[ch];						\
	pokey[chip].p4 = (pokey[chip].p4+pokey[chip].polyadjust)%0x0000f;	\
	pokey[chip].p5 = (pokey[chip].p5+pokey[chip].polyadjust)%0x0001f;	\
	pokey[chip].p9 = (pokey[chip].p9+pokey[chip].polyadjust)%0x001ff;	\
	pokey[chip].p17 = (pokey[chip].p17+pokey[chip].polyadjust)%0x1ffff; \
	pokey[chip].polyadjust = 0; 										\
	if( (pokey[chip].AUDC[ch] & NOTPOLY5) || P5(chip) ) 				\
	{																	\
		if( pokey[chip].AUDC[ch] & PURE )								\
			toggle = 1; 												\
		else															\
		if( pokey[chip].AUDC[ch] & POLY4 )								\
			toggle = pokey[chip].output[ch] == !P4(chip);				\
		else															\
		if( pokey[chip].AUDCTL & POLY9 )								\
			toggle = pokey[chip].output[ch] == !P9(chip);				\
		else															\
			toggle = pokey[chip].output[ch] == !P17(chip);				\
	}																	\
	if( toggle )														\
	{																	\
		if( pokey[chip].output[ch] )									\
			sum -= pokey[chip].volume[ch];								\
		else															\
			sum += pokey[chip].volume[ch];								\
		pokey[chip].output[ch] ^= 1;									\
	}																	\
	/* is this a filtering channel (3/4) and is the filter active? */	\
	if( pokey[chip].AUDCTL & ((CH1_FILTER|CH2_FILTER) & (0x10 >> ch)) ) \
    {                                                                   \
		if( pokey[chip].output[ch-2] )									\
        {                                                               \
			pokey[chip].output[ch-2] = 0;								\
			sum -= pokey[chip].volume[ch-2];							\
        }                                                               \
    }                                                                   \

#endif

#define PROCESS_SAMPLE(chip)                                            \
    ADJUST_EVENT(chip);                                                 \
    /* adjust the sample position */                                    \
	pokey[chip].samplepos_fract += pokey[chip].samplerate_24_8; 		\
	if( pokey[chip].samplepos_fract & 0xffffff00 )						\
	{																	\
		pokey[chip].samplepos_whole += pokey[chip].samplepos_fract>>8;	\
		pokey[chip].samplepos_fract &= 0x000000ff;						\
	}																	\
	/* store sum of output signals into the buffer */					\
	nLeftSample  = buffer[0] + (INT32)(sum * pokey_mastervol);          \
	nRightSample = buffer[1] + (INT32)(sum * pokey_mastervol);          \
	buffer[0] = BURN_SND_CLIP(nLeftSample);                             \
	buffer[1] = BURN_SND_CLIP(nRightSample);                            \
	buffer++; buffer++;                                                 \
	length--;

#if HEAVY_MACRO_USAGE

/*
 * This version of PROCESS_POKEY repeats the search for the minimum
 * event value without using an index to the channel. That way the
 * PROCESS_CHANNEL macros can be called with fixed values and expand
 * to much more efficient code
 */

#define PROCESS_POKEY(chip) 											\
	UINT32 sum = 0; 													\
	if( pokey[chip].output[CHAN1] ) 									\
		sum += pokey[chip].volume[CHAN1];								\
	if( pokey[chip].output[CHAN2] ) 									\
		sum += pokey[chip].volume[CHAN2];								\
	if( pokey[chip].output[CHAN3] ) 									\
		sum += pokey[chip].volume[CHAN3];								\
	if( pokey[chip].output[CHAN4] ) 									\
		sum += pokey[chip].volume[CHAN4];								\
    while( length > 0 )                                                 \
	{																	\
		if( pokey[chip].counter[CHAN1] < pokey[chip].samplepos_whole )	\
		{																\
			if( pokey[chip].counter[CHAN2] <  pokey[chip].counter[CHAN1] ) \
			{															\
				if( pokey[chip].counter[CHAN3] <  pokey[chip].counter[CHAN2] ) \
				{														\
					if( pokey[chip].counter[CHAN4] < pokey[chip].counter[CHAN3] ) \
					{													\
						UINT32 event = pokey[chip].counter[CHAN4];		\
                        PROCESS_CHANNEL(chip,CHAN4);                    \
					}													\
					else												\
					{													\
						UINT32 event = pokey[chip].counter[CHAN3];		\
                        PROCESS_CHANNEL(chip,CHAN3);                    \
					}													\
				}														\
				else													\
				if( pokey[chip].counter[CHAN4] < pokey[chip].counter[CHAN2] )  \
				{														\
					UINT32 event = pokey[chip].counter[CHAN4];			\
                    PROCESS_CHANNEL(chip,CHAN4);                        \
				}														\
                else                                                    \
				{														\
					UINT32 event = pokey[chip].counter[CHAN2];			\
                    PROCESS_CHANNEL(chip,CHAN2);                        \
				}														\
            }                                                           \
			else														\
			if( pokey[chip].counter[CHAN3] < pokey[chip].counter[CHAN1] ) \
			{															\
				if( pokey[chip].counter[CHAN4] < pokey[chip].counter[CHAN3] ) \
				{														\
					UINT32 event = pokey[chip].counter[CHAN4];			\
                    PROCESS_CHANNEL(chip,CHAN4);                        \
				}														\
                else                                                    \
				{														\
					UINT32 event = pokey[chip].counter[CHAN3];			\
                    PROCESS_CHANNEL(chip,CHAN3);                        \
				}														\
            }                                                           \
			else														\
			if( pokey[chip].counter[CHAN4] < pokey[chip].counter[CHAN1] ) \
			{															\
				UINT32 event = pokey[chip].counter[CHAN4];				\
                PROCESS_CHANNEL(chip,CHAN4);                            \
			}															\
            else                                                        \
			{															\
				UINT32 event = pokey[chip].counter[CHAN1];				\
                PROCESS_CHANNEL(chip,CHAN1);                            \
			}															\
		}																\
		else															\
		if( pokey[chip].counter[CHAN2] < pokey[chip].samplepos_whole )	\
		{																\
			if( pokey[chip].counter[CHAN3] < pokey[chip].counter[CHAN2] ) \
			{															\
				if( pokey[chip].counter[CHAN4] < pokey[chip].counter[CHAN3] ) \
				{														\
					UINT32 event = pokey[chip].counter[CHAN4];			\
                    PROCESS_CHANNEL(chip,CHAN4);                        \
				}														\
				else													\
				{														\
					UINT32 event = pokey[chip].counter[CHAN3];			\
                    PROCESS_CHANNEL(chip,CHAN3);                        \
				}														\
			}															\
			else														\
			if( pokey[chip].counter[CHAN4] < pokey[chip].counter[CHAN2] ) \
			{															\
				UINT32 event = pokey[chip].counter[CHAN4];				\
                PROCESS_CHANNEL(chip,CHAN4);                            \
			}															\
			else														\
			{															\
				UINT32 event = pokey[chip].counter[CHAN2];				\
                PROCESS_CHANNEL(chip,CHAN2);                            \
			}															\
		}																\
		else															\
		if( pokey[chip].counter[CHAN3] < pokey[chip].samplepos_whole )	\
        {                                                               \
			if( pokey[chip].counter[CHAN4] < pokey[chip].counter[CHAN3] ) \
			{															\
				UINT32 event = pokey[chip].counter[CHAN4];				\
                PROCESS_CHANNEL(chip,CHAN4);                            \
			}															\
			else														\
			{															\
				UINT32 event = pokey[chip].counter[CHAN3];				\
                PROCESS_CHANNEL(chip,CHAN3);                            \
			}															\
		}																\
		else															\
		if( pokey[chip].counter[CHAN4] < pokey[chip].samplepos_whole )	\
		{																\
			UINT32 event = pokey[chip].counter[CHAN4];					\
            PROCESS_CHANNEL(chip,CHAN4);                                \
        }                                                               \
		else															\
		{																\
			UINT32 event = pokey[chip].samplepos_whole; 				\
			PROCESS_SAMPLE(chip);										\
		}																\
	}
	//timer_reset(pokey[chip].rtimer, TIME_NEVER)

void pokey0_update(int param, INT16 *buffer, int length) { PROCESS_POKEY(0); }
void pokey1_update(int param, INT16 *buffer, int length) { PROCESS_POKEY(1); }
void pokey2_update(int param, INT16 *buffer, int length) { PROCESS_POKEY(2); }
void pokey3_update(int param, INT16 *buffer, int length) { PROCESS_POKEY(3); }
void (*update[MAXPOKEYS])(int,INT16*,int) =
	{ pokey0_update,pokey1_update,pokey2_update,pokey3_update };

#else   /* no HEAVY_MACRO_USAGE */
/*
 * And this version of PROCESS_POKEY uses event and channel variables
 * so that the PROCESS_CHANNEL macro needs to index memory at runtime.
 */

#define PROCESS_POKEY(chip)                                             \
	UINT32 sum = 0; 													\
	if( pokey[chip].output[CHAN1] ) 									\
		sum += pokey[chip].volume[CHAN1];								\
	if( pokey[chip].output[CHAN2] ) 									\
		sum += pokey[chip].volume[CHAN2];								\
	if( pokey[chip].output[CHAN3] ) 									\
		sum += pokey[chip].volume[CHAN3];								\
	if( pokey[chip].output[CHAN4] ) 									\
        sum += pokey[chip].volume[CHAN4];                               \
    while( length > 0 )                                                 \
	{																	\
		UINT32 event = pokey[chip].samplepos_whole; 					\
		UINT32 channel = (UINT32)SAMPLE;								\
		if( pokey[chip].counter[CHAN1] < (INT32)event )					\
        {                                                               \
			event = pokey[chip].counter[CHAN1]; 						\
			channel = CHAN1;											\
		}																\
		if( pokey[chip].counter[CHAN2] < (INT32)event )					\
        {                                                               \
			event = pokey[chip].counter[CHAN2]; 						\
			channel = CHAN2;											\
        }                                                               \
		if( pokey[chip].counter[CHAN3] < (INT32)event )					\
        {                                                               \
			event = pokey[chip].counter[CHAN3]; 						\
			channel = CHAN3;											\
        }                                                               \
		if( pokey[chip].counter[CHAN4] < (INT32)event )					\
        {                                                               \
			event = pokey[chip].counter[CHAN4]; 						\
			channel = CHAN4;											\
        }                                                               \
        if( channel == (UINT32)SAMPLE )                                 \
		{																\
            PROCESS_SAMPLE(chip);                                       \
        }                                                               \
		else															\
		{																\
			PROCESS_CHANNEL(chip,channel);								\
		}																\
	}
	//timer_reset(pokey[chip].rtimer, TIME_NEVER)

void pokey_update(int num, INT16 *buffer, int length) {
	if (!intf.addtostream && num == 0)
		memset(buffer, 0, length * 4);
	PROCESS_POKEY(num);
}

void (*update[MAXPOKEYS])(int,INT16*,int) =
	{ pokey_update,pokey_update,pokey_update,pokey_update };

#endif

void pokey_sh_update(void)
{
//	int chip;

//	for( chip = 0; chip < intf.num; chip++ )
//		stream_update(pokey[chip].channel, 0);
}

static void poly_init(UINT8 *poly, int size, int left, int right, int add)
{
	int mask = (1 << size) - 1;
    int i, x = 0;

	LOG_POLY((errorlog,"poly %d\n", size));
	for( i = 0; i < mask; i++ )
	{
		*poly++ = x & 1;
		LOG_POLY((errorlog,"%05x: %d\n", x, x&1));
        /* calculate next bit */
		x = ((x << left) + (x >> right) + add) & mask;
	}
}

static void rand_init(UINT8 *rng, int size, int left, int right, int add)
{
    int mask = (1 << size) - 1;
    int i, x = 0;

	LOG_RAND((errorlog,"rand %d\n", size));
    for( i = 0; i < mask; i++ )
	{
		*rng = x >> (size - 8);   /* use the upper 8 bits */
		LOG_RAND((errorlog, "%05x: %02x\n", x, *rng));
        rng++;
        /* calculate next bit */
		x = ((x << left) + (x >> right) + add) & mask;
	}
}

int PokeyInit(int clock, int num, double vol, int addtostream)
{
	int chip, sample_rate;

	memset(&intf, 0, sizeof(intf));
	sample_rate = nBurnSoundRate;
	intf.num = num;
	//intf.mixing_level[0] = vol;
	pokey_mastervol = vol;
	intf.baseclock = (clock) ? clock : FREQ_17_EXACT;
	intf.addtostream = addtostream;

	poly9 = (UINT8 *)malloc(0x1ff+1);
	rand9 = (UINT8 *)malloc(0x1ff+1);
    poly17 = (UINT8 *)malloc(0x1ffff+1);
    rand17 = (UINT8 *)malloc(0x1ffff+1);
	if( !poly9 || !rand9 || !poly17 || !rand17 )
	{
		PokeyExit();	/* free any allocated memory again */
		return 1;
	}

	/* initialize the poly counters */
	poly_init(poly4,   4, 3, 1, 0x00004);
	poly_init(poly5,   5, 3, 2, 0x00008);
	poly_init(poly9,   9, 2, 7, 0x00080);
	poly_init(poly17, 17, 7,10, 0x18000);

	/* initialize the random arrays */
	rand_init(rand9,   9, 2, 7, 0x00080);
	rand_init(rand17, 17, 7,10, 0x18000);

	for( chip = 0; chip < intf.num; chip++ )
	{
		struct POKEYregisters *p = &pokey[chip];
        char name[40];

		memset(p, 0, sizeof(struct POKEYregisters));

		p->samplerate_24_8 = (sample_rate) ? (intf.baseclock << 8) / sample_rate : 1;
		p->divisor[CHAN1] = 4;
		p->divisor[CHAN2] = 4;
		p->divisor[CHAN3] = 4;
		p->divisor[CHAN4] = 4;
		p->clockmult = DIV_64;
		p->KBCODE = 0x09;		 /* Atari 800 'no key' */
		p->SKCTL = SK_RESET;	 /* let the RNG run after reset */
		//p->rtimer = timer_set(TIME_NEVER, chip, NULL);

		p->pot_r[0] = intf.pot0_r[chip];
		p->pot_r[1] = intf.pot1_r[chip];
		p->pot_r[2] = intf.pot2_r[chip];
		p->pot_r[3] = intf.pot3_r[chip];
		p->pot_r[4] = intf.pot4_r[chip];
		p->pot_r[5] = intf.pot5_r[chip];
		p->pot_r[6] = intf.pot6_r[chip];
		p->pot_r[7] = intf.pot7_r[chip];
		p->allpot_r = intf.allpot_r[chip];
		p->serin_r = intf.serin_r[chip];
		p->serout_w = intf.serout_w[chip];
		p->interrupt_cb = intf.interrupt_cb[chip];

		sprintf(name, "Pokey #%d", chip);
//		p->channel = stream_init(name, intf.mixing_level[chip], Machine->sample_rate, chip, update[chip]);

		if( p->channel == -1 )
		{
			//perror("failed to initialize sound channel");
            return 1;
		}
	}

    return 0;
}

void PokeyExit(void)
{
	if( rand17 ) free(rand17);
	rand17 = NULL;
	if( poly17 ) free(poly17);
	poly17 = NULL;
	if( rand9 )  free(rand9);
	rand9 = NULL;
	if( poly9 )  free(poly9);
	poly9 = NULL;
}

/*static void pokey_timer_expire(int param)
{
	int chip = param >> 3;
	int timers = param & 7;
	struct POKEYregisters *p = &pokey[chip];

	LOG_TIMER((errorlog, "POKEY #%d timer %d with IRQEN $%02x\n", chip, param, p->IRQEN));

    // check if some of the requested timer interrupts are enabled
	timers &= p->IRQEN;

    if( timers )
    {
		// set the enabled timer irq status bits
		p->IRQST |= timers;
        // call back an application supplied function to handle the interrupt
		if( p->interrupt_cb )
			(*p->interrupt_cb)(timers);
    }
}*/

#if VERBOSE_SOUND
static char *audc2str(int val)
{
	static char buff[80];
	if( val & NOTPOLY5 )
	{
		if( val & PURE )
			strcpy(buff,"pure");
		else
		if( val & POLY4 )
			strcpy(buff,"poly4");
		else
			strcpy(buff,"poly9/17");
	}
	else
	{
		if( val & PURE )
			strcpy(buff,"poly5");
		else
		if( val & POLY4 )
			strcpy(buff,"poly4+poly5");
		else
			strcpy(buff,"poly9/17+poly5");
    }
	return buff;
}

static char *audctl2str(int val)
{
	static char buff[80];
	if( val & POLY9 )
		strcpy(buff,"poly9");
	else
		strcpy(buff,"poly17");
	if( val & CH1_HICLK )
		strcat(buff,"+ch1hi");
	if( val & CH3_HICLK )
		strcat(buff,"+ch3hi");
	if( val & CH12_JOINED )
		strcat(buff,"+ch1/2");
	if( val & CH34_JOINED )
		strcat(buff,"+ch3/4");
	if( val & CH1_FILTER )
		strcat(buff,"+ch1filter");
	if( val & CH2_FILTER )
		strcat(buff,"+ch2filter");
	if( val & CLK_15KHZ )
		strcat(buff,"+clk15");
    return buff;
}
#endif

/*static void pokey_serin_ready(int chip)
{
	struct POKEYregisters *p = &pokey[chip];
    if( p->IRQEN & IRQ_SERIN )
	{
		// set the enabled timer irq status bits
		p->IRQST |= IRQ_SERIN;
		// call back an application supplied function to handle the interrupt
		if( p->interrupt_cb )
			(*p->interrupt_cb)(IRQ_SERIN);
	}
}

static void pokey_serout_ready(int chip)
{
	struct POKEYregisters *p = &pokey[chip];
    if( p->IRQEN & IRQ_SEROR )
	{
		p->IRQST |= IRQ_SEROR;
		if( p->interrupt_cb )
			(*p->interrupt_cb)(IRQ_SEROR);
	}
}

static void pokey_serout_complete(int chip)
{
	struct POKEYregisters *p = &pokey[chip];
    if( p->IRQEN & IRQ_SEROC )
	{
		p->IRQST |= IRQ_SEROC;
		if( p->interrupt_cb )
			(*p->interrupt_cb)(IRQ_SEROC);
	}
}

static void pokey_pot_trigger(int param)
{
	int chip = param >> 3;
    int pot = param & 7;
	struct POKEYregisters *p = &pokey[chip];

	LOG((errorlog, "POKEY #%d POT%d triggers after %dus\n", chip, pot, (int)(1000000ul*timer_timeelapsed(p->ptimer[pot]))));
	p->ptimer[pot] = NULL;
	p->ALLPOT &= ~(1 << pot);	// set the enabled timer irq status bits
}*/

/* A/D conversion time:
 * In normal, slow mode (SKCTL bit SK_PADDLE is clear) the conversion
 * takes N scanlines, where N is the paddle value. A single scanline
 * takes approximately 64us to finish (1.78979MHz clock).
 * In quick mode (SK_PADDLE set) the conversion is done very fast
 * (takes two scalines) but the result is not as accurate.
 */
#define AD_TIME (double)(((p->SKCTL & SK_PADDLE) ? 64.0*2/228 : 64.0) * FREQ_17_EXACT / intf.baseclock)

static void pokey_potgo(int chip)
{
	struct POKEYregisters *p = &pokey[chip];
    int pot;

	LOG((errorlog, "POKEY #%d pokey_potgo\n", chip));

    p->ALLPOT = 0xff;

    for( pot = 0; pot < 8; pot++ )
	{
        if( p->ptimer[pot] )
		{
			//timer_remove(p->ptimer[pot]);
			p->ptimer[pot] = NULL;
			p->POTx[pot] = 0xff;
		}
		if( p->pot_r[pot] )
		{
			int r = (*p->pot_r[pot])(pot);
			LOG((errorlog, "POKEY #%d pot_r(%d) returned $%02x\n", chip, pot, r));
			if( r != -1 )
			{
				if (r > 228)
                    r = 228;
                /* final value */
                p->POTx[pot] = r;
				//p->ptimer[pot] = timer_set(TIME_IN_USEC(r * AD_TIME), (chip<<3)|pot, pokey_pot_trigger);
			}
		}
	}
}

int pokey_register_r(int chip, int offs)
{
	struct POKEYregisters *p = &pokey[chip];
    int data = 0, pot;

#ifdef MAME_DEBUG
	if( chip >= intf.num )
	{
		if( errorlog ) fprintf(errorlog, "POKEY #%d is >= number of Pokeys!\n", chip);
		return data;
	}
#endif

    switch (offs & 15)
	{
	case POT0_C: case POT1_C: case POT2_C: case POT3_C:
	case POT4_C: case POT5_C: case POT6_C: case POT7_C:
		pot = offs & 7;
		if( p->pot_r[pot] )
		{
			/*
			 * If the conversion is not yet finished (ptimer running),
			 * get the current value by the linear interpolation of
			 * the final value using the elapsed time.
			 */
			if( p->ALLPOT & (1 << pot) )
			{
				data = (UINT8)(/*timer_timeelapsed(p->ptimer[pot])*/1234 / AD_TIME);
				LOG((errorlog,"POKEY #%d read POT%d (interpolated) $%02x\n", chip, pot, data));
            }
			else
			{
				data = p->POTx[pot];
				LOG((errorlog,"POKEY #%d read POT%d (final value)  $%02x\n", chip, pot, data));
			}
		}
		else
		break;

    case ALLPOT_C:
		if( p->allpot_r )
		{
			data = (*p->allpot_r)(offs);
			LOG((errorlog,"POKEY #%d ALLPOT callback $%02x\n", chip, data));
		}
		else
		{
			data = p->ALLPOT;
			LOG((errorlog,"POKEY #%d ALLPOT internal $%02x\n", chip, data));
		}
		break;

	case KBCODE_C:
		data = p->KBCODE;
		break;

	case RANDOM_C:
		/****************************************************************
		 * If the 2 least significant bits of SKCTL are 0, the random
		 * number generator is disabled (SKRESET). Thanks to Eric Smith
		 * for pointing out this critical bit of info! If the random
		 * number generator is enabled, get a new random number. Take
		 * the time gone since the last read into account and read the
		 * new value from an appropriate offset in the rand17 table.
		 ****************************************************************/
		if( p->SKCTL & SK_RESET )
		{
			UINT32 adjust = (UINT32)(/*timer_timeelapsed(p->rtimer)*/rand() /*dink*/ * intf.baseclock);
			p->r9 = (p->r9 + adjust) % 0x001ff;
			p->r17 = (p->r17 + adjust) % 0x1ffff;
			if( p->AUDCTL & POLY9 )
			{
				p->RANDOM = rand9[p->r9];
				LOG_RAND((errorlog, "POKEY #%d adjust %u rand9[$%05x]: $%02x\n", chip, adjust, p->r9, p->RANDOM));
			}
            else
			{
				p->RANDOM = rand17[p->r17];
				LOG_RAND((errorlog, "POKEY #%d adjust %u rand17[$%05x]: $%02x\n", chip, adjust, p->r17, p->RANDOM));
			}
		}
		else
		{
			LOG_RAND((errorlog, "POKEY #%d rand17 freezed (SKCTL): $%02x\n", chip, p->RANDOM));
		}
		//timer_reset(p->rtimer, TIME_NEVER);
		data = p->RANDOM;
		break;

	case SERIN_C:
		if( p->serin_r )
			p->SERIN = (*p->serin_r)(offs);
		data = p->SERIN;
		LOG((errorlog, "POKEY #%d SERIN  $%02x\n", chip, data));
		break;

	case IRQST_C:
		/* IRQST is an active low input port; we keep it active high */
		/* internally to ease the (un-)masking of bits */
		data = p->IRQST ^ 0xff;
		LOG((errorlog, "POKEY #%d IRQST  $%02x\n", chip, data));
		break;

	case SKSTAT_C:
		/* SKSTAT is also an active low input port */
		data = p->SKSTAT ^ 0xff;
		LOG((errorlog, "POKEY #%d SKSTAT $%02x\n", chip, data));
		break;

	default:
		LOG((errorlog, "POKEY #%d register $%02x\n", chip, offs));
        break;
    }
    return data;
}

int pokey1_r (int offset)
{
	return pokey_register_r(0, offset);
}

int pokey2_r (int offset)
{
	return pokey_register_r(1, offset);
}

int pokey3_r (int offset)
{
	return pokey_register_r(2, offset);
}

int pokey4_r (int offset)
{
	return pokey_register_r(3, offset);
}

int quad_pokey_r (int offset)
{
	int pokey_num = (offset >> 3) & ~0x04;
	int control = (offset & 0x20) >> 2;
	int pokey_reg = (offset % 8) | control;

	return pokey_register_r(pokey_num, pokey_reg);
}


void pokey_register_w(int chip, int offs, int data)
{
	struct POKEYregisters *p = &pokey[chip];
	int ch_mask = 0, new_val;

#ifdef MAME_DEBUG
	if( chip >= intf.num )
	{
		if( errorlog ) fprintf(errorlog, "POKEY #%d is >= number of Pokeys!\n", chip);
		return;
	}
#endif
	//stream_update(p->channel, 0);

    /* determine which address was changed */
	switch (offs & 15)
    {
    case AUDF1_C:
		if( data == p->AUDF[CHAN1] )
            return;
		LOG_SOUND((errorlog, "POKEY #%d AUDF1  $%02x\n", chip, data));
		p->AUDF[CHAN1] = data;
        ch_mask = 1 << CHAN1;
		if( p->AUDCTL & CH12_JOINED )		/* if ch 1&2 tied together */
            ch_mask |= 1 << CHAN2;    /* then also change on ch2 */
        break;

    case AUDC1_C:
		if( data == p->AUDC[CHAN1] )
            return;
		LOG_SOUND((errorlog, "POKEY #%d AUDC1  $%02x (%s)\n", chip, data, audc2str(data)));
		p->AUDC[CHAN1] = data;
        ch_mask = 1 << CHAN1;
        break;

    case AUDF2_C:
		if( data == p->AUDF[CHAN2] )
            return;
		LOG_SOUND((errorlog, "POKEY #%d AUDF2  $%02x\n", chip, data));
		p->AUDF[CHAN2] = data;
        ch_mask = 1 << CHAN2;
        break;

    case AUDC2_C:
		if( data == p->AUDC[CHAN2] )
            return;
		LOG_SOUND((errorlog, "POKEY #%d AUDC2  $%02x (%s)\n", chip, data, audc2str(data)));
		p->AUDC[CHAN2] = data;
        ch_mask = 1 << CHAN2;
        break;

    case AUDF3_C:
		if( data == p->AUDF[CHAN3] )
            return;
		LOG_SOUND((errorlog, "POKEY #%d AUDF3  $%02x\n", chip, data));
		p->AUDF[CHAN3] = data;
        ch_mask = 1 << CHAN3;

		if( p->AUDCTL & CH34_JOINED )	/* if ch 3&4 tied together */
            ch_mask |= 1 << CHAN4;  /* then also change on ch4 */
        break;

    case AUDC3_C:
		if( data == p->AUDC[CHAN3] )
            return;
		LOG_SOUND((errorlog, "POKEY #%d AUDC3  $%02x (%s)\n", chip, data, audc2str(data)));
		p->AUDC[CHAN3] = data;
        ch_mask = 1 << CHAN3;
        break;

    case AUDF4_C:
		if( data == p->AUDF[CHAN4] )
            return;
		LOG_SOUND((errorlog, "POKEY #%d AUDF4  $%02x\n", chip, data));
		p->AUDF[CHAN4] = data;
        ch_mask = 1 << CHAN4;
        break;

    case AUDC4_C:
		if( data == p->AUDC[CHAN4] )
            return;
		LOG_SOUND((errorlog, "POKEY #%d AUDC4  $%02x (%s)\n", chip, data, audc2str(data)));
		p->AUDC[CHAN4] = data;
        ch_mask = 1 << CHAN4;
        break;

    case AUDCTL_C:
		if( data == p->AUDCTL )
            return;
		LOG_SOUND((errorlog, "POKEY #%d AUDCTL $%02x (%s)\n", chip, data, audctl2str(data)));
		p->AUDCTL = data;
        ch_mask = 15;       /* all channels */
        /* determine the base multiplier for the 'div by n' calculations */
		p->clockmult = (p->AUDCTL & CLK_15KHZ) ? DIV_15 : DIV_64;
        break;

    case STIMER_C:
        /* first remove any existing timers */
		LOG_TIMER((errorlog, "POKEY #%d STIMER $%02x\n", chip, data));
		/*if( p->timer[TIMER1] )
			timer_remove(p->timer[TIMER1]);
		if( p->timer[TIMER2] )
			timer_remove(p->timer[TIMER2]);
		if( p->timer[TIMER4] )
			timer_remove(p->timer[TIMER4]);
		p->timer[TIMER1] = NULL;
		p->timer[TIMER2] = NULL;
		p->timer[TIMER4] = NULL;*/

        /* reset all counters to zero (side effect) */
		p->polyadjust = 0;
		p->counter[CHAN1] = 0;
		p->counter[CHAN2] = 0;
		p->counter[CHAN3] = 0;
		p->counter[CHAN4] = 0;

        /* joined chan#1 and chan#2 ? */
		if( p->AUDCTL & CH12_JOINED )
        {
			if( p->divisor[CHAN2] > 4 )
			{
				LOG_TIMER((errorlog, "POKEY #%d timer1+2 after %d clocks\n", chip, p->divisor[CHAN2]));
				/* set timer #1 _and_ #2 event after timer_div clocks of joined CHAN1+CHAN2 */
				/*p->timer[TIMER2] =
					timer_pulse(1.0 * p->divisor[CHAN2] / intf.baseclock,
						(chip<<3)|IRQ_TIMR2|IRQ_TIMR1, pokey_timer_expire);*/
			}
        }
        else
        {
			if( p->divisor[CHAN1] > 4 )
			{
				LOG_TIMER((errorlog, "POKEY #%d timer1 after %d clocks\n", chip, p->divisor[CHAN1]));
				/* set timer #1 event after timer_div clocks of CHAN1 */
				/*p->timer[TIMER1] =
					timer_pulse(1.0 * p->divisor[CHAN1] / intf.baseclock,
						(chip<<3)|IRQ_TIMR1, pokey_timer_expire);*/
			}

			if( p->divisor[CHAN2] > 4 )
			{
				LOG_TIMER((errorlog, "POKEY #%d timer2 after %d clocks\n", chip, p->divisor[CHAN2]));
				/* set timer #2 event after timer_div clocks of CHAN2 */
				/*p->timer[TIMER2] =
					timer_pulse(1.0 * p->divisor[CHAN2] / intf.baseclock,
						(chip<<3)|IRQ_TIMR2, pokey_timer_expire);*/
			}
        }

		/* Note: p[chip] does not have a timer #3 */

		if( p->AUDCTL & CH34_JOINED )
        {
            /* not sure about this: if audc4 == 0000xxxx don't start timer 4 ? */
			if( p->AUDC[CHAN4] & 0xf0 )
            {
				if( p->divisor[CHAN4] > 4 )
				{
					LOG_TIMER((errorlog, "POKEY #%d timer4 after %d clocks\n", chip, p->divisor[CHAN4]));
					/* set timer #4 event after timer_div clocks of CHAN4 */
					/*p->timer[TIMER4] =
						timer_pulse(1.0 * p->divisor[CHAN4] / intf.baseclock,
							(chip<<3)|IRQ_TIMR4, pokey_timer_expire);*/
				}
            }
        }
        else
        {
			if( p->divisor[CHAN4] > 4 )
			{
				LOG_TIMER((errorlog, "POKEY #%d timer4 after %d clocks\n", chip, p->divisor[CHAN4]));
				/* set timer #4 event after timer_div clocks of CHAN4 */
				/*p->timer[TIMER4] =
					timer_pulse(1.0 * p->divisor[CHAN4] / intf.baseclock,
						(chip<<3)|IRQ_TIMR4, pokey_timer_expire);*/
			}
        }
		/*if( p->timer[TIMER1] )
			timer_enable(p->timer[TIMER1], p->IRQEN & IRQ_TIMR1);
		if( p->timer[TIMER2] )
			timer_enable(p->timer[TIMER2], p->IRQEN & IRQ_TIMR2);
		if( p->timer[TIMER4] )
			timer_enable(p->timer[TIMER4], p->IRQEN & IRQ_TIMR4);*/
        break;

    case SKREST_C:
        /* reset SKSTAT */
		LOG((errorlog, "POKEY #%d SKREST $%02x\n", chip, data));
		p->SKSTAT &= ~(SK_FRAME|SK_OVERRUN|SK_KBERR);
        break;

    case POTGO_C:
		LOG((errorlog, "POKEY #%d POTGO  $%02x\n", chip, data));
		pokey_potgo(chip);
        break;

    case SEROUT_C:
		LOG((errorlog, "POKEY #%d SEROUT $%02x\n", chip, data));
		if (p->serout_w)
			(*p->serout_w)(offs, data);
		p->SKSTAT |= SK_SEROUT;
        /*
         * These are arbitrary values, tested with some custom boot
         * loaders from Ballblazer and Escape from Fractalus
         * The real times are unknown
         */
        //timer_set(TIME_IN_USEC(200), chip, pokey_serout_ready);
        /* 10 bits (assumption 1 start, 8 data and 1 stop bit) take how long? */
        //timer_set(TIME_IN_USEC(2000), chip, pokey_serout_complete);
        break;

    case IRQEN_C:
		LOG((errorlog, "POKEY #%d IRQEN  $%02x\n", chip, data));

        /* acknowledge one or more IRQST bits ? */
		if( p->IRQST & ~data )
        {
            /* reset IRQST bits that are masked now */
			p->IRQST &= data;
        }
        else
        {
			/* enable/disable timers now to avoid unneeded
               breaking of the CPU cores for masked timers */
			/*if( p->timer[TIMER1] && ((p->IRQEN^data) & IRQ_TIMR1) )
				timer_enable(p->timer[TIMER1], data & IRQ_TIMR1);
			if( p->timer[TIMER2] && ((p->IRQEN^data) & IRQ_TIMR2) )
				timer_enable(p->timer[TIMER2], data & IRQ_TIMR2);
			if( p->timer[TIMER4] && ((p->IRQEN^data) & IRQ_TIMR4) )
				timer_enable(p->timer[TIMER4], data & IRQ_TIMR4);*/
        }
		/* store irq enable */
		p->IRQEN = data;
        break;

    case SKCTL_C:
		if( data == p->SKCTL )
            return;
		LOG((errorlog, "POKEY #%d SKCTL  $%02x\n", chip, data));
		p->SKCTL = data;
        if( !(data & SK_RESET) )
        {
            pokey_register_w(chip, IRQEN_C,  0);
            pokey_register_w(chip, SKREST_C, 0);
        }
        break;
    }

	/************************************************************
	 * As defined in the manual, the exact counter values are
	 * different depending on the frequency and resolution:
	 *	  64 kHz or 15 kHz - AUDF + 1
	 *	  1.79 MHz, 8-bit  - AUDF + 4
	 *	  1.79 MHz, 16-bit - AUDF[CHAN1]+256*AUDF[CHAN2] + 7
	 ************************************************************/

    /* only reset the channels that have changed */

    if( ch_mask & (1 << CHAN1) )
    {
        /* process channel 1 frequency */
		if( p->AUDCTL & CH1_HICLK )
			new_val = p->AUDF[CHAN1] + DIVADD_HICLK;
        else
			new_val = (p->AUDF[CHAN1] + DIVADD_LOCLK) * p->clockmult;

		LOG_SOUND((errorlog, "POKEY #%d chan1 %d\n", chip, new_val));

		p->volume[CHAN1] = (p->AUDC[CHAN1] & VOLUME_MASK) * POKEY_DEFAULT_GAIN;
        p->divisor[CHAN1] = new_val;
		if( new_val < p->counter[CHAN1] )
			p->counter[CHAN1] = new_val;
		/*if( p->interrupt_cb && p->timer[TIMER1] )
			timer_reset(p->timer[TIMER1], 1.0 * new_val / intf.baseclock);*/
		p->audible[CHAN1] = !(
			(p->AUDC[CHAN1] & VOLUME_ONLY) ||
			(p->AUDC[CHAN1] & VOLUME_MASK) == 0 ||
			((p->AUDC[CHAN1] & PURE) && new_val < ((INT32)p->samplerate_24_8 >> 8)));
		if( !p->audible[CHAN1] )
		{
			p->output[CHAN1] = 1;
			p->counter[CHAN1] = 0x7fffffff;
			/* 50% duty cycle should result in half volume */
            p->volume[CHAN1] >>= 1;
        }
    }

    if( ch_mask & (1 << CHAN2) )
    {
        /* process channel 2 frequency */
		if( p->AUDCTL & CH12_JOINED )
        {
			if( p->AUDCTL & CH1_HICLK )
				new_val = p->AUDF[CHAN2] * 256 + p->AUDF[CHAN1] + DIVADD_HICLK_JOINED;
            else
				new_val = (p->AUDF[CHAN2] * 256 + p->AUDF[CHAN1] + DIVADD_LOCLK) * p->clockmult;
			LOG_SOUND((errorlog, "POKEY #%d chan1+2 %d\n", chip, new_val));
        }
        else
		{
			new_val = (p->AUDF[CHAN2] + DIVADD_LOCLK) * p->clockmult;
			LOG_SOUND((errorlog, "POKEY #%d chan2 %d\n", chip, new_val));
		}

		p->volume[CHAN2] = (p->AUDC[CHAN2] & VOLUME_MASK) * POKEY_DEFAULT_GAIN;
		p->divisor[CHAN2] = new_val;
		if( new_val < p->counter[CHAN2] )
			p->counter[CHAN2] = new_val;
		/*if( p->interrupt_cb && p->timer[TIMER2] )
			timer_reset(p->timer[TIMER2], 1.0 * new_val / intf.baseclock);*/
		p->audible[CHAN2] = !(
			(p->AUDC[CHAN2] & VOLUME_ONLY) ||
			(p->AUDC[CHAN2] & VOLUME_MASK) == 0 ||
			((p->AUDC[CHAN2] & PURE) && new_val < ((INT32)p->samplerate_24_8 >> 8)));
		if( !p->audible[CHAN2] )
		{
			p->output[CHAN2] = 1;
			p->counter[CHAN2] = 0x7fffffff;
			/* 50% duty cycle should result in half volume */
			p->volume[CHAN2] >>= 1;
        }
    }

    if( ch_mask & (1 << CHAN3) )
    {
        /* process channel 3 frequency */
		if( p->AUDCTL & CH3_HICLK )
			new_val = p->AUDF[CHAN3] + DIVADD_HICLK;
        else
			new_val = (p->AUDF[CHAN3] + DIVADD_LOCLK) * p->clockmult;

		LOG_SOUND((errorlog, "POKEY #%d chan3 %d\n", chip, new_val));

		p->volume[CHAN3] = (p->AUDC[CHAN3] & VOLUME_MASK) * POKEY_DEFAULT_GAIN;
		p->divisor[CHAN3] = new_val;
		if( new_val < p->counter[CHAN3] )
			p->counter[CHAN3] = new_val;
		/* channel 3 does not have a timer associated */
		p->audible[CHAN3] = !(
			(p->AUDC[CHAN3] & VOLUME_ONLY) ||
			(p->AUDC[CHAN3] & VOLUME_MASK) == 0 ||
			((p->AUDC[CHAN3] & PURE) && new_val < ((INT32)p->samplerate_24_8 >> 8))) ||
			(p->AUDCTL & CH1_FILTER);
		if( !p->audible[CHAN3] )
		{
			p->output[CHAN3] = 1;
			p->counter[CHAN3] = 0x7fffffff;
			/* 50% duty cycle should result in half volume */
			p->volume[CHAN3] >>= 1;
        }
    }

    if( ch_mask & (1 << CHAN4) )
    {
        /* process channel 4 frequency */
		if( p->AUDCTL & CH34_JOINED )
        {
			if( p->AUDCTL & CH3_HICLK )
				new_val = p->AUDF[CHAN4] * 256 + p->AUDF[CHAN3] + DIVADD_HICLK_JOINED;
            else
				new_val = (p->AUDF[CHAN4] * 256 + p->AUDF[CHAN3] + DIVADD_LOCLK) * p->clockmult;
			LOG_SOUND((errorlog, "POKEY #%d chan3+4 %d\n", chip, new_val));
        }
        else
		{
			new_val = (p->AUDF[CHAN4] + DIVADD_LOCLK) * p->clockmult;
			LOG_SOUND((errorlog, "POKEY #%d chan4 %d\n", chip, new_val));
		}

		p->volume[CHAN4] = (p->AUDC[CHAN4] & VOLUME_MASK) * POKEY_DEFAULT_GAIN;
		p->divisor[CHAN4] = new_val;
		if( new_val < p->counter[CHAN4] )
			p->counter[CHAN4] = new_val;
		/*if( p->interrupt_cb && p->timer[TIMER4] )
			timer_reset(p->timer[TIMER4], 1.0 * new_val / intf.baseclock);*/
		p->audible[CHAN4] = !(
			(p->AUDC[CHAN4] & VOLUME_ONLY) ||
			(p->AUDC[CHAN4] & VOLUME_MASK) == 0 ||
			((p->AUDC[CHAN4] & PURE) && new_val < ((INT32)p->samplerate_24_8 >> 8))) ||
			(p->AUDCTL & CH2_FILTER);
		if( !p->audible[CHAN4] )
		{
			p->output[CHAN4] = 1;
			p->counter[CHAN4] = 0x7fffffff;
			/* 50% duty cycle should result in half volume */
			p->volume[CHAN4] >>= 1;
        }
    }
}

void pokey1_w (int offset,int data)
{
	pokey_register_w(0,offset,data);
}

void pokey2_w (int offset,int data)
{
    pokey_register_w(1,offset,data);
}

void pokey3_w (int offset,int data)
{
    pokey_register_w(2,offset,data);
}

void pokey4_w (int offset,int data)
{
    pokey_register_w(3,offset,data);
}

void quad_pokey_w (int offset,int data)
{
    int pokey_num = (offset >> 3) & ~0x04;
    int control = (offset & 0x20) >> 2;
    int pokey_reg = (offset % 8) | control;

    pokey_register_w(pokey_num, pokey_reg, data);
}

void pokey1_serin_ready(int /*after*/)
{
	//timer_set(1.0 * after / intf.baseclock, 0, pokey_serin_ready);
}

void pokey2_serin_ready(int /*after*/)
{
	//timer_set(1.0 * after / intf.baseclock, 1, pokey_serin_ready);
}

void pokey3_serin_ready(int /*after*/)
{
	//timer_set(1.0 * after / intf.baseclock, 2, pokey_serin_ready);
}

void pokey4_serin_ready(int /*after*/)
{
	//timer_set(1.0 * after / intf.baseclock, 3, pokey_serin_ready);
}

void pokey_break_w(int chip, int shift)
{
	struct POKEYregisters *p = &pokey[chip];
    if( shift )                     /* shift code ? */
		p->SKSTAT |= SK_SHIFT;
	else
		p->SKSTAT &= ~SK_SHIFT;
	/* check if the break IRQ is enabled */
	if( p->IRQEN & IRQ_BREAK )
	{
		/* set break IRQ status and call back the interrupt handler */
		p->IRQST |= IRQ_BREAK;
		if( p->interrupt_cb )
			(*p->interrupt_cb)(IRQ_BREAK);
    }
}

void pokey1_break_w(int shift)
{
	pokey_break_w(0, shift);
}

void pokey2_break_w(int shift)
{
	pokey_break_w(1, shift);
}

void pokey3_break_w(int shift)
{
	pokey_break_w(2, shift);
}

void pokey4_break_w(int shift)
{
	pokey_break_w(3, shift);
}

void pokey_kbcode_w(int chip, int kbcode, int make)
{
	struct POKEYregisters *p = &pokey[chip];
    /* make code ? */
	if( make )
	{
		p->KBCODE = kbcode;
		p->SKSTAT |= SK_KEYBD;
		if( kbcode & 0x40 ) 		/* shift code ? */
			p->SKSTAT |= SK_SHIFT;
		else
			p->SKSTAT &= ~SK_SHIFT;

		if( p->IRQEN & IRQ_KEYBD )
		{
			/* last interrupt not acknowledged ? */
			if( p->IRQST & IRQ_KEYBD )
				p->SKSTAT |= SK_KBERR;
			p->IRQST |= IRQ_KEYBD;
			if( p->interrupt_cb )
				(*p->interrupt_cb)(IRQ_KEYBD);
		}
	}
	else
	{
		p->KBCODE = kbcode;
		p->SKSTAT &= ~SK_KEYBD;
    }
}

void pokey1_kbcode_w(int kbcode, int make)
{
	pokey_kbcode_w(0, kbcode, make);
}

void pokey2_kbcode_w(int kbcode, int make)
{
	pokey_kbcode_w(1, kbcode, make);
}

void pokey3_kbcode_w(int kbcode, int make)
{
	pokey_kbcode_w(2, kbcode, make);
}

void pokey4_kbcode_w(int kbcode, int make)
{
	pokey_kbcode_w(3, kbcode, make);
}

