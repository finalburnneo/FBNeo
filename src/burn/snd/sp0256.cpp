/*
   GI SP0256 Narrator Speech Processor
   GI SPB640 Speech Buffer

   By Joe Zbiciak. Ported to MESS by tim lindner.

   Unimplemented:
   - Microsequencer repeat count of zero
   - Support for non bit-flipped ROMs
   - SPB-640 perpherial/RAM bus

 Copyright Joseph Zbiciak, all rights reserved.
 Copyright tim lindner, all rights reserved.

 - This source code is released as freeware for non-commercial purposes.
 - You are free to use and redistribute this code in modified or
   unmodified form, provided you list us in the credits.
 - If you modify this source code, you must add a notice to each
   modified source file that it has been changed.  If you're a nice
   person, you will clearly mark each change too.  :)
 - If you wish to use this for commercial purposes, please contact us at
   intvnut@gmail.com (Joseph Zbiciak), tlindner@macmess.org (tim lindner)
 - This entire notice must remain in the source code.
*/

#include "burnint.h"
#include "sp0256.h"

#define READY_TIMER     (0x0010)
#define CLOCK_DIVIDER   (7*6*8)
#define HIGH_QUALITY

#define SCBUF_SIZE      (4096)             /* Must be power of 2               */
#define SCBUF_MASK      (SCBUF_SIZE - 1)
#define PER_PAUSE       (64)               /* Equiv timing period for pauses.  */
#define PER_NOISE       (64)               /* Equiv timing period for noise.   */

#define FIFO_ADDR       (0x1800 << 3)      /* address of SPB260 speech FIFO.   */

#define VERBOSE 0
#define DEBUG_FIFO 0

#define LOG(x)  do { if (VERBOSE) bprintf x; } while (0)
#define LOG_FIFO(x) do { if (DEBUG_FIFO) bprintf x; } while (0)

#define SET_SBY(line_state) {       \
	if (m_sby_line != line_state)   \
	{                               \
		m_sby_line = line_state;    \
		m_sby_cb(m_sby_line);       \
	}                               \
}

struct lpc12_t
{
	INT32   rpt, cnt;       /* Repeat counter, Period down-counter.         */
	UINT32  per, rng;       /* Period, Amplitude, Random Number Generator   */
	INT32   amp;
	INT16   f_coef[6];      /* F0 through F5.                               */
	INT16   b_coef[6];      /* B0 through B5.                               */
	INT16   z_data[6][2];   /* Time-delay data for the filter stages.       */
	UINT8   r[16];          /* The encoded register set.                    */
	INT32   interp;
};

static void (*m_drq_cb)(UINT8) = NULL;
static void (*m_sby_cb)(UINT8) = NULL;

static void null_callback_drq(UINT8 data)
{
	//bprintf(0, _T("null callback drq: %X\n"), data);
}

static void null_callback_sby(UINT8 data)
{
	//bprintf(0, _T("null callback sby: %X\n"), data);
}

void sp0256_set_drq_cb(void (*cb)(UINT8))
{
	m_drq_cb = cb;
}

void sp0256_set_sby_cb(void (*cb)(UINT8))
{
	m_sby_cb = cb;
}

// "re"sampler stuff
static INT32            samples_from;
static INT16            *mixer_buffer;

static UINT8            *m_rom;            /* 64K ROM.                                     */

static INT32            m_sby_line;        /* Standby line state                           */
static INT32            m_silent;          /* Flag: SP0256 is silent.                      */

static INT16            *m_scratch;        /* Scratch buffer for audio.                    */
static UINT32           m_sc_head;         /* Head pointer into scratch circular buf       */
static UINT32           m_sc_tail;         /* Tail pointer into scratch circular buf       */

static struct lpc12_t   m_filt;            /* 12-pole filter                               */
static INT32            m_lrq;             /* Load ReQuest.  == 0 if we can accept a load  */
static INT32            m_ald;             /* Address LoaD.  < 0 if no command pending.    */
static INT32            m_pc;              /* Microcontroller's PC value.                  */
static INT32            m_stack;           /* Microcontroller's PC stack.                  */
static INT32            m_fifo_sel;        /* True when executing from FIFO.               */
static INT32            m_halted;          /* True when CPU is halted.                     */
static UINT32           m_mode;            /* Mode register.                               */
static UINT32           m_page;            /* Page set by SETPAGE                          */

static UINT32           m_fifo_head;       /* FIFO head pointer (where new data goes).     */
static UINT32           m_fifo_tail;       /* FIFO tail pointer (where data comes from).   */
static UINT32           m_fifo_bitp;       /* FIFO bit-pointer (for partial decles).       */
static UINT16           m_fifo[64];        /* The 64-decle FIFO.                           */


//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------
void sp0256_set_clock(INT32 clock)
{
	samples_from = (INT32)((double)(((clock / CLOCK_DIVIDER) * 100) / nBurnFPS) + 0.5);
}

void sp0256_scan(INT32 nAction, INT32* pnMin)
{
	if (pnMin && *pnMin < 0x029521) {
		*pnMin = 0x029521;
	}

	if (nAction & ACB_DRIVER_DATA) {
		ScanVar(&m_scratch, SCBUF_SIZE * sizeof(INT16), "sp0256 ScratchRam");

		SCAN_VAR(m_sby_line);
		SCAN_VAR(m_silent);

		SCAN_VAR(m_sc_head);
		SCAN_VAR(m_sc_tail);

		SCAN_VAR(m_filt);
		SCAN_VAR(m_lrq);
		SCAN_VAR(m_ald);
		SCAN_VAR(m_pc);
		SCAN_VAR(m_stack);
		SCAN_VAR(m_fifo_sel);
		SCAN_VAR(m_halted);
		SCAN_VAR(m_mode);
		SCAN_VAR(m_page);

		SCAN_VAR(m_fifo_head);
		SCAN_VAR(m_fifo_tail);
		SCAN_VAR(m_fifo_bitp);
		SCAN_VAR(m_fifo);
	}
}

void sp0256_init(UINT8 *rom, INT32 clock)
{
	m_rom = rom;

	sp0256_set_drq_cb(null_callback_drq);
	sp0256_set_sby_cb(null_callback_sby);

	/* -------------------------------------------------------------------- */
	/*  Allocate a scratch buffer for generating ~10kHz samples.             */
	/* -------------------------------------------------------------------- */
	m_scratch = (INT16 *)BurnMalloc(SCBUF_SIZE * sizeof(INT16));

	// Init Re-sampler buffer
	mixer_buffer = (INT16 *)BurnMalloc(nBurnSoundRate * sizeof(INT16));
	sp0256_set_clock(clock);
}

void sp0256_exit()
{
	BurnFree(m_scratch);
	BurnFree(mixer_buffer);
}

//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void sp0256_reset()
{
	// reset FIFO and SP0256
	m_fifo_head = m_fifo_tail = m_fifo_bitp = 0;
	m_sc_head = m_sc_tail = 0;

	memset(m_scratch, 0, SCBUF_SIZE * sizeof(INT16));
	memset(&m_filt, 0, sizeof(m_filt));
	memset(&m_fifo, 0, sizeof(m_fifo));
	m_halted   = 1;
	m_filt.rpt = -1;
	m_filt.rng = 1;
	m_lrq      = 0x0000;
	m_ald      = 0x0000;
	m_pc       = 0x0000;
	m_stack    = 0x0000;
	m_fifo_sel = 0;
	m_mode     = 0;
	m_page     = 0x1000 << 3;
	m_silent   = 1;
	m_sby_line = 1;
	//m_drq_cb(1);
	//SET_SBY(1)
}


/* ======================================================================== */
/*  LIMIT            -- Limiter function for digital sample output.         */
/* ======================================================================== */
static INT16 limit(INT16 s)
{
#ifdef HIGH_QUALITY /* Higher quality than the original, but who cares? */
	if (s >  8191) return  8191;
	if (s < -8192) return -8192;
#else
	if (s >  127) return  127;
	if (s < -128) return -128;
#endif
	return s;
}

/* ======================================================================== */
/*  LPC12_UPDATE     -- Update the 12-pole filter, outputting samples.      */
/* ======================================================================== */
static INT32 lpc12_update(struct lpc12_t *f, INT32 num_samp, INT16 *out, UINT32 *optr)
{
	INT32 i, j;
	INT16 samp;
	INT32 do_int;
	INT32 oidx = *optr;

	/* -------------------------------------------------------------------- */
	/*  Iterate up to the desired number of samples.  We actually may       */
	/*  break out early if our repeat count expires.                        */
	/* -------------------------------------------------------------------- */
	for (i = 0; i < num_samp; i++)
	{
		/* ---------------------------------------------------------------- */
		/*  Generate a series of periodic impulses, or random noise.        */
		/* ---------------------------------------------------------------- */
		do_int = 0;
		samp   = 0;
		if (f->per)
		{
			if (f->cnt <= 0)
			{
				f->cnt += f->per;
				samp    = f->amp;
				f->rpt--;
				do_int  = f->interp;

				for (j = 0; j < 6; j++)
					f->z_data[j][1] = f->z_data[j][0] = 0;

			} else
			{
				samp = 0;
				f->cnt--;
			}

		} else
		{
			INT32 bit;

			if (--f->cnt <= 0)
			{
				do_int = f->interp;
				f->cnt = PER_NOISE;
				f->rpt--;
				for (j = 0; j < 6; j++)
					f->z_data[j][0] = f->z_data[j][1] = 0;
			}

			bit = f->rng & 1;
			f->rng = (f->rng >> 1) ^ (bit ? 0x4001 : 0);

			if (bit) { samp =  f->amp; }
			else     { samp = -f->amp; }
		}

		/* ---------------------------------------------------------------- */
		/*  If we need to, process the interpolation registers.             */
		/* ---------------------------------------------------------------- */
		if (do_int)
		{
			f->r[0] += f->r[14];
			f->r[1] += f->r[15];

			f->amp   = (f->r[0] & 0x1F) << (((f->r[0] & 0xE0) >> 5) + 0);
			f->per   = f->r[1];
		}

		/* ---------------------------------------------------------------- */
		/*  Stop if we expire our repeat counter and return the actual      */
		/*  number of samples we did.                                       */
		/* ---------------------------------------------------------------- */
		if (f->rpt <= 0) break;

		/* ---------------------------------------------------------------- */
		/*  Each 2nd order stage looks like one of these.  The App. Manual  */
		/*  gives the first form, the patent gives the second form.         */
		/*  They're equivalent except for time delay.  I implement the      */
		/*  first form.   (Note: 1/Z == 1 unit of time delay.)              */
		/*                                                                  */
		/*          ---->(+)-------->(+)----------+------->                 */
		/*                ^           ^           |                         */
		/*                |           |           |                         */
		/*                |           |           |                         */
		/*               [B]        [2*F]         |                         */
		/*                ^           ^           |                         */
		/*                |           |           |                         */
		/*                |           |           |                         */
		/*                +---[1/Z]<--+---[1/Z]<--+                         */
		/*                                                                  */
		/*                                                                  */
		/*                +---[2*F]<---+                                    */
		/*                |            |                                    */
		/*                |            |                                    */
		/*                v            |                                    */
		/*          ---->(+)-->[1/Z]-->+-->[1/Z]---+------>                 */
		/*                ^                        |                        */
		/*                |                        |                        */
		/*                |                        |                        */
		/*                +-----------[B]<---------+                        */
		/*                                                                  */
		/* ---------------------------------------------------------------- */
		for (j = 0; j < 6; j++)
		{
			samp += (((int)f->b_coef[j] * (int)f->z_data[j][1]) >> 9);
			samp += (((int)f->f_coef[j] * (int)f->z_data[j][0]) >> 8);

			f->z_data[j][1] = f->z_data[j][0];
			f->z_data[j][0] = samp;
		}

#ifdef HIGH_QUALITY /* Higher quality than the original, but who cares? */
		out[oidx++ & SCBUF_MASK] = limit(samp) << 2;
#else
		out[oidx++ & SCBUF_MASK] = (limit(samp >> 4) << 8);
#endif
	}

	*optr = oidx;

	return i;
}

/* ======================================================================== */
/*  LPC12_REGDEC -- Decode the register set in the filter bank.             */
/* ======================================================================== */
static void lpc12_regdec(struct lpc12_t *f)
{
	/* ======================================================================== */
	/*  qtbl  -- Coefficient Quantization Table.  This comes from a             */
	/*              SP0250 data sheet, and should be correct for SP0256.        */
	/* ======================================================================== */
	static const INT16 qtbl[128] =
	{
		0,      9,      17,     25,     33,     41,     49,     57,
		65,     73,     81,     89,     97,     105,    113,    121,
		129,    137,    145,    153,    161,    169,    177,    185,
		193,    201,    209,    217,    225,    233,    241,    249,
		257,    265,    273,    281,    289,    297,    301,    305,
		309,    313,    317,    321,    325,    329,    333,    337,
		341,    345,    349,    353,    357,    361,    365,    369,
		373,    377,    381,    385,    389,    393,    397,    401,
		405,    409,    413,    417,    421,    425,    427,    429,
		431,    433,    435,    437,    439,    441,    443,    445,
		447,    449,    451,    453,    455,    457,    459,    461,
		463,    465,    467,    469,    471,    473,    475,    477,
		479,    481,    482,    483,    484,    485,    486,    487,
		488,    489,    490,    491,    492,    493,    494,    495,
		496,    497,    498,    499,    500,    501,    502,    503,
		504,    505,    506,    507,    508,    509,    510,    511
	};

	/* -------------------------------------------------------------------- */
	/*  Decode the Amplitude and Period registers.  Force the 'cnt' to 0    */
	/*  to get an initial impulse.  We compensate elsewhere by setting      */
	/*  the repeat count to "repeat + 1".                                   */
	/* -------------------------------------------------------------------- */
	f->amp = (f->r[0] & 0x1F) << (((f->r[0] & 0xE0) >> 5) + 0);
	f->cnt = 0;
	f->per = f->r[1];

	/* -------------------------------------------------------------------- */
	/*  Decode the filter coefficients from the quant table.                */
	/* -------------------------------------------------------------------- */
	for (INT32 i = 0; i < 6; i++)
	{
		#define IQ(x) (((x) & 0x80) ? qtbl[0x7F & -(x)] : -qtbl[(x)])

		f->b_coef[i] = IQ(f->r[2 + 2*i]);
		f->f_coef[i] = IQ(f->r[3 + 2*i]);
	}

	/* -------------------------------------------------------------------- */
	/*  Set the Interp flag based on whether we have interpolation parms    */
	/* -------------------------------------------------------------------- */
	f->interp = f->r[14] || f->r[15];

	return;
}

/* ======================================================================== */
/*  SP0256_DATAFMT   -- Data format table for the SP0256's microsequencer   */
/*                                                                          */
/*  len     4 bits      Length of field to extract                          */
/*  lshift  4 bits      Left-shift amount on field                          */
/*  param   4 bits      Parameter number being updated                      */
/*  delta   1 bit       This is a delta-update.  (Implies sign-extend)      */
/*  field   1 bit       This is a field replace.                            */
/*  clr5    1 bit       Clear F5, B5.                                       */
/*  clrall  1 bit       Clear all before doing this update                  */
/* ======================================================================== */

#define CR(l,s,p,d,f,c5,ca)         \
		(                           \
			(((l)  & 15) <<  0) |   \
			(((s)  & 15) <<  4) |   \
			(((p)  & 15) <<  8) |   \
			(((d)  &  1) << 12) |   \
			(((f)  &  1) << 13) |   \
			(((c5) &  1) << 14) |   \
			(((ca) &  1) << 15)     \
		)

#define CR_DELTA  CR(0,0,0,1,0,0,0)
#define CR_FIELD  CR(0,0,0,0,1,0,0)
#define CR_CLR5   CR(0,0,0,0,0,1,0)
#define CR_CLRA   CR(0,0,0,0,0,0,1)
#define CR_LEN(x) ((x) & 15)
#define CR_SHF(x) (((x) >> 4) & 15)
#define CR_PRM(x) (((x) >> 8) & 15)

enum { AM = 0, PR, B0, F0, B1, F1, B2, F2, B3, F3, B4, F4, B5, F5, IA, IP };

static const UINT16 sp0256_datafmt[] =
{
	/* -------------------------------------------------------------------- */
	/*  OPCODE 1111: PAUSE                                                  */
	/* -------------------------------------------------------------------- */
	/*    0 */  CR( 0,  0,  0,  0,  0,  0,  1),     /*  Clear all   */

	/* -------------------------------------------------------------------- */
	/*  Opcode 0001: LOADALL                                                */
	/* -------------------------------------------------------------------- */
				/* All modes                */
	/*    1 */  CR( 8,  0,  AM, 0,  0,  0,  1),     /*  Amplitude   */
	/*    2 */  CR( 8,  0,  PR, 0,  0,  0,  0),     /*  Period      */
	/*    3 */  CR( 8,  0,  B0, 0,  0,  0,  0),     /*  B0          */
	/*    4 */  CR( 8,  0,  F0, 0,  0,  0,  0),     /*  F0          */
	/*    5 */  CR( 8,  0,  B1, 0,  0,  0,  0),     /*  B1          */
	/*    6 */  CR( 8,  0,  F1, 0,  0,  0,  0),     /*  F1          */
	/*    7 */  CR( 8,  0,  B2, 0,  0,  0,  0),     /*  B2          */
	/*    8 */  CR( 8,  0,  F2, 0,  0,  0,  0),     /*  F2          */
	/*    9 */  CR( 8,  0,  B3, 0,  0,  0,  0),     /*  B3          */
	/*   10 */  CR( 8,  0,  F3, 0,  0,  0,  0),     /*  F3          */
	/*   11 */  CR( 8,  0,  B4, 0,  0,  0,  0),     /*  B4          */
	/*   12 */  CR( 8,  0,  F4, 0,  0,  0,  0),     /*  F4          */
	/*   13 */  CR( 8,  0,  B5, 0,  0,  0,  0),     /*  B5          */
	/*   14 */  CR( 8,  0,  F5, 0,  0,  0,  0),     /*  F5          */
				/* Mode 01 and 11 only      */
	/*   15 */  CR( 8,  0,  IA, 0,  0,  0,  0),     /*  Amp Interp  */
	/*   16 */  CR( 8,  0,  IP, 0,  0,  0,  0),     /*  Pit Interp  */

	/* -------------------------------------------------------------------- */
	/*  Opcode 0100: LOAD_4                                                 */
	/* -------------------------------------------------------------------- */
				/* Mode 00 and 01           */
	/*   17 */  CR( 6,  2,  AM, 0,  0,  0,  1),     /*  Amplitude   */
	/*   18 */  CR( 8,  0,  PR, 0,  0,  0,  0),     /*  Period      */
	/*   19 */  CR( 4,  3,  B3, 0,  0,  0,  0),     /*  B3 (S=0)    */
	/*   20 */  CR( 6,  2,  F3, 0,  0,  0,  0),     /*  F3          */
	/*   21 */  CR( 7,  1,  B4, 0,  0,  0,  0),     /*  B4          */
	/*   22 */  CR( 6,  2,  F4, 0,  0,  0,  0),     /*  F4          */
				/* Mode 01 only             */
	/*   23 */  CR( 8,  0,  B5, 0,  0,  0,  0),     /*  B5          */
	/*   24 */  CR( 8,  0,  F5, 0,  0,  0,  0),     /*  F5          */

				/* Mode 10 and 11           */
	/*   25 */  CR( 6,  2,  AM, 0,  0,  0,  1),     /*  Amplitude   */
	/*   26 */  CR( 8,  0,  PR, 0,  0,  0,  0),     /*  Period      */
	/*   27 */  CR( 6,  1,  B3, 0,  0,  0,  0),     /*  B3 (S=0)    */
	/*   28 */  CR( 7,  1,  F3, 0,  0,  0,  0),     /*  F3          */
	/*   29 */  CR( 8,  0,  B4, 0,  0,  0,  0),     /*  B4          */
	/*   30 */  CR( 8,  0,  F4, 0,  0,  0,  0),     /*  F4          */
				/* Mode 11 only             */
	/*   31 */  CR( 8,  0,  B5, 0,  0,  0,  0),     /*  B5          */
	/*   32 */  CR( 8,  0,  F5, 0,  0,  0,  0),     /*  F5          */

	/* -------------------------------------------------------------------- */
	/*  Opcode 0110: SETMSB_6                                               */
	/* -------------------------------------------------------------------- */
				/* Mode 00 only             */
	/*   33 */  CR( 0,  0,  0,  0,  0,  1,  0),     /*  Clear 5     */
				/* Mode 00 and 01           */
	/*   34 */  CR( 6,  2,  AM, 0,  0,  0,  0),     /*  Amplitude   */
	/*   35 */  CR( 6,  2,  F3, 0,  1,  0,  0),     /*  F3 (5 MSBs) */
	/*   36 */  CR( 6,  2,  F4, 0,  1,  0,  0),     /*  F4 (5 MSBs) */
				/* Mode 01 only             */
	/*   37 */  CR( 8,  0,  F5, 0,  1,  0,  0),     /*  F5 (5 MSBs) */

				/* Mode 10 only             */
	/*   38 */  CR( 0,  0,  0,  0,  0,  1,  0),     /*  Clear 5     */
				/* Mode 10 and 11           */
	/*   39 */  CR( 6,  2,  AM, 0,  0,  0,  0),     /*  Amplitude   */
	/*   40 */  CR( 7,  1,  F3, 0,  1,  0,  0),     /*  F3 (6 MSBs) */
	/*   41 */  CR( 8,  0,  F4, 0,  1,  0,  0),     /*  F4 (6 MSBs) */
				/* Mode 11 only             */
	/*   42 */  CR( 8,  0,  F5, 0,  1,  0,  0),     /*  F5 (6 MSBs) */

	/*   43 */  0,  /* unused */
	/*   44 */  0,  /* unused */

	/* -------------------------------------------------------------------- */
	/*  Opcode 1001: DELTA_9                                                */
	/* -------------------------------------------------------------------- */
				/* Mode 00 and 01           */
	/*   45 */  CR( 4,  2,  AM, 1,  0,  0,  0),     /*  Amplitude   */
	/*   46 */  CR( 5,  0,  PR, 1,  0,  0,  0),     /*  Period      */
	/*   47 */  CR( 3,  4,  B0, 1,  0,  0,  0),     /*  B0 4 MSBs   */
	/*   48 */  CR( 3,  3,  F0, 1,  0,  0,  0),     /*  F0 5 MSBs   */
	/*   49 */  CR( 3,  4,  B1, 1,  0,  0,  0),     /*  B1 4 MSBs   */
	/*   50 */  CR( 3,  3,  F1, 1,  0,  0,  0),     /*  F1 5 MSBs   */
	/*   51 */  CR( 3,  4,  B2, 1,  0,  0,  0),     /*  B2 4 MSBs   */
	/*   52 */  CR( 3,  3,  F2, 1,  0,  0,  0),     /*  F2 5 MSBs   */
	/*   53 */  CR( 3,  3,  B3, 1,  0,  0,  0),     /*  B3 5 MSBs   */
	/*   54 */  CR( 4,  2,  F3, 1,  0,  0,  0),     /*  F3 6 MSBs   */
	/*   55 */  CR( 4,  1,  B4, 1,  0,  0,  0),     /*  B4 7 MSBs   */
	/*   56 */  CR( 4,  2,  F4, 1,  0,  0,  0),     /*  F4 6 MSBs   */
				/* Mode 01 only             */
	/*   57 */  CR( 5,  0,  B5, 1,  0,  0,  0),     /*  B5 8 MSBs   */
	/*   58 */  CR( 5,  0,  F5, 1,  0,  0,  0),     /*  F5 8 MSBs   */

				/* Mode 10 and 11           */
	/*   59 */  CR( 4,  2,  AM, 1,  0,  0,  0),     /*  Amplitude   */
	/*   60 */  CR( 5,  0,  PR, 1,  0,  0,  0),     /*  Period      */
	/*   61 */  CR( 4,  1,  B0, 1,  0,  0,  0),     /*  B0 7 MSBs   */
	/*   62 */  CR( 4,  2,  F0, 1,  0,  0,  0),     /*  F0 6 MSBs   */
	/*   63 */  CR( 4,  1,  B1, 1,  0,  0,  0),     /*  B1 7 MSBs   */
	/*   64 */  CR( 4,  2,  F1, 1,  0,  0,  0),     /*  F1 6 MSBs   */
	/*   65 */  CR( 4,  1,  B2, 1,  0,  0,  0),     /*  B2 7 MSBs   */
	/*   66 */  CR( 4,  2,  F2, 1,  0,  0,  0),     /*  F2 6 MSBs   */
	/*   67 */  CR( 4,  1,  B3, 1,  0,  0,  0),     /*  B3 7 MSBs   */
	/*   68 */  CR( 5,  1,  F3, 1,  0,  0,  0),     /*  F3 7 MSBs   */
	/*   69 */  CR( 5,  0,  B4, 1,  0,  0,  0),     /*  B4 8 MSBs   */
	/*   70 */  CR( 5,  0,  F4, 1,  0,  0,  0),     /*  F4 8 MSBs   */
				/* Mode 11 only             */
	/*   71 */  CR( 5,  0,  B5, 1,  0,  0,  0),     /*  B5 8 MSBs   */
	/*   72 */  CR( 5,  0,  F5, 1,  0,  0,  0),     /*  F5 8 MSBs   */

	/* -------------------------------------------------------------------- */
	/*  Opcode 1010: SETMSB_A                                               */
	/* -------------------------------------------------------------------- */
				/* Mode 00 only             */
	/*   73 */  CR( 0,  0,  0,  0,  0,  1,  0),     /*  Clear 5     */
				/* Mode 00 and 01           */
	/*   74 */  CR( 6,  2,  AM, 0,  0,  0,  0),     /*  Amplitude   */
	/*   75 */  CR( 5,  3,  F0, 0,  1,  0,  0),     /*  F0 (5 MSBs) */
	/*   76 */  CR( 5,  3,  F1, 0,  1,  0,  0),     /*  F1 (5 MSBs) */
	/*   77 */  CR( 5,  3,  F2, 0,  1,  0,  0),     /*  F2 (5 MSBs) */

				/* Mode 10 only             */
	/*   78 */  CR( 0,  0,  0,  0,  0,  1,  0),     /*  Clear 5     */
				/* Mode 10 and 11           */
	/*   79 */  CR( 6,  2,  AM, 0,  0,  0,  0),     /*  Amplitude   */
	/*   80 */  CR( 6,  2,  F0, 0,  1,  0,  0),     /*  F0 (6 MSBs) */
	/*   81 */  CR( 6,  2,  F1, 0,  1,  0,  0),     /*  F1 (6 MSBs) */
	/*   82 */  CR( 6,  2,  F2, 0,  1,  0,  0),     /*  F2 (6 MSBs) */

	/* -------------------------------------------------------------------- */
	/*  Opcode 0010: LOAD_2  Mode 00 and 10                                 */
	/*  Opcode 1100: LOAD_C  Mode 00 and 10                                 */
	/* -------------------------------------------------------------------- */
				/* LOAD_2, LOAD_C  Mode 00  */
	/*   83 */  CR( 6,  2,  AM, 0,  0,  0,  1),     /*  Amplitude   */
	/*   84 */  CR( 8,  0,  PR, 0,  0,  0,  0),     /*  Period      */
	/*   85 */  CR( 3,  4,  B0, 0,  0,  0,  0),     /*  B0 (S=0)    */
	/*   86 */  CR( 5,  3,  F0, 0,  0,  0,  0),     /*  F0          */
	/*   87 */  CR( 3,  4,  B1, 0,  0,  0,  0),     /*  B1 (S=0)    */
	/*   88 */  CR( 5,  3,  F1, 0,  0,  0,  0),     /*  F1          */
	/*   89 */  CR( 3,  4,  B2, 0,  0,  0,  0),     /*  B2 (S=0)    */
	/*   90 */  CR( 5,  3,  F2, 0,  0,  0,  0),     /*  F2          */
	/*   91 */  CR( 4,  3,  B3, 0,  0,  0,  0),     /*  B3 (S=0)    */
	/*   92 */  CR( 6,  2,  F3, 0,  0,  0,  0),     /*  F3          */
	/*   93 */  CR( 7,  1,  B4, 0,  0,  0,  0),     /*  B4          */
	/*   94 */  CR( 6,  2,  F4, 0,  0,  0,  0),     /*  F4          */
				/* LOAD_2 only              */
	/*   95 */  CR( 5,  0,  IA, 0,  0,  0,  0),     /*  Ampl. Intr. */
	/*   96 */  CR( 5,  0,  IP, 0,  0,  0,  0),     /*  Per. Intr.  */

				/* LOAD_2, LOAD_C  Mode 10  */
	/*   97 */  CR( 6,  2,  AM, 0,  0,  0,  1),     /*  Amplitude   */
	/*   98 */  CR( 8,  0,  PR, 0,  0,  0,  0),     /*  Period      */
	/*   99 */  CR( 6,  1,  B0, 0,  0,  0,  0),     /*  B0 (S=0)    */
	/*  100 */  CR( 6,  2,  F0, 0,  0,  0,  0),     /*  F0          */
	/*  101 */  CR( 6,  1,  B1, 0,  0,  0,  0),     /*  B1 (S=0)    */
	/*  102 */  CR( 6,  2,  F1, 0,  0,  0,  0),     /*  F1          */
	/*  103 */  CR( 6,  1,  B2, 0,  0,  0,  0),     /*  B2 (S=0)    */
	/*  104 */  CR( 6,  2,  F2, 0,  0,  0,  0),     /*  F2          */
	/*  105 */  CR( 6,  1,  B3, 0,  0,  0,  0),     /*  B3 (S=0)    */
	/*  106 */  CR( 7,  1,  F3, 0,  0,  0,  0),     /*  F3          */
	/*  107 */  CR( 8,  0,  B4, 0,  0,  0,  0),     /*  B4          */
	/*  108 */  CR( 8,  0,  F4, 0,  0,  0,  0),     /*  F4          */
				/* LOAD_2 only              */
	/*  109 */  CR( 5,  0,  IA, 0,  0,  0,  0),     /*  Ampl. Intr. */
	/*  110 */  CR( 5,  0,  IP, 0,  0,  0,  0),     /*  Per. Intr.  */

	/* -------------------------------------------------------------------- */
	/*  OPCODE 1101: DELTA_D                                                */
	/* -------------------------------------------------------------------- */
				/* Mode 00 and 01           */
	/*  111 */  CR( 4,  2,  AM, 1,  0,  0,  0),     /*  Amplitude   */
	/*  112 */  CR( 5,  0,  PR, 1,  0,  0,  0),     /*  Period      */
	/*  113 */  CR( 3,  3,  B3, 1,  0,  0,  0),     /*  B3 5 MSBs   */
	/*  114 */  CR( 4,  2,  F3, 1,  0,  0,  0),     /*  F3 6 MSBs   */
	/*  115 */  CR( 4,  1,  B4, 1,  0,  0,  0),     /*  B4 7 MSBs   */
	/*  116 */  CR( 4,  2,  F4, 1,  0,  0,  0),     /*  F4 6 MSBs   */
				/* Mode 01 only             */
	/*  117 */  CR( 5,  0,  B5, 1,  0,  0,  0),     /*  B5 8 MSBs   */
	/*  118 */  CR( 5,  0,  F5, 1,  0,  0,  0),     /*  F5 8 MSBs   */

				/* Mode 10 and 11           */
	/*  119 */  CR( 4,  2,  AM, 1,  0,  0,  0),     /*  Amplitude   */
	/*  120 */  CR( 5,  0,  PR, 1,  0,  0,  0),     /*  Period      */
	/*  121 */  CR( 4,  1,  B3, 1,  0,  0,  0),     /*  B3 7 MSBs   */
	/*  122 */  CR( 5,  1,  F3, 1,  0,  0,  0),     /*  F3 7 MSBs   */
	/*  123 */  CR( 5,  0,  B4, 1,  0,  0,  0),     /*  B4 8 MSBs   */
	/*  124 */  CR( 5,  0,  F4, 1,  0,  0,  0),     /*  F4 8 MSBs   */
				/* Mode 11 only             */
	/*  125 */  CR( 5,  0,  B5, 1,  0,  0,  0),     /*  B5 8 MSBs   */
	/*  126 */  CR( 5,  0,  F5, 1,  0,  0,  0),     /*  F5 8 MSBs   */

	/* -------------------------------------------------------------------- */
	/*  OPCODE 1110: LOAD_E                                                 */
	/* -------------------------------------------------------------------- */
	/*  127 */  CR( 6,  2,  AM, 0,  0,  0,  0),     /*  Amplitude   */
	/*  128 */  CR( 8,  0,  PR, 0,  0,  0,  0),     /*  Period      */

	/* -------------------------------------------------------------------- */
	/*  Opcode 0010: LOAD_2  Mode 01 and 11                                 */
	/*  Opcode 1100: LOAD_C  Mode 01 and 11                                 */
	/* -------------------------------------------------------------------- */
				/* LOAD_2, LOAD_C  Mode 01  */
	/*  129 */  CR( 6,  2,  AM, 0,  0,  0,  1),     /*  Amplitude   */
	/*  130 */  CR( 8,  0,  PR, 0,  0,  0,  0),     /*  Period      */
	/*  131 */  CR( 3,  4,  B0, 0,  0,  0,  0),     /*  B0 (S=0)    */
	/*  132 */  CR( 5,  3,  F0, 0,  0,  0,  0),     /*  F0          */
	/*  133 */  CR( 3,  4,  B1, 0,  0,  0,  0),     /*  B1 (S=0)    */
	/*  134 */  CR( 5,  3,  F1, 0,  0,  0,  0),     /*  F1          */
	/*  135 */  CR( 3,  4,  B2, 0,  0,  0,  0),     /*  B2 (S=0)    */
	/*  136 */  CR( 5,  3,  F2, 0,  0,  0,  0),     /*  F2          */
	/*  137 */  CR( 4,  3,  B3, 0,  0,  0,  0),     /*  B3 (S=0)    */
	/*  138 */  CR( 6,  2,  F3, 0,  0,  0,  0),     /*  F3          */
	/*  139 */  CR( 7,  1,  B4, 0,  0,  0,  0),     /*  B4          */
	/*  140 */  CR( 6,  2,  F4, 0,  0,  0,  0),     /*  F4          */
	/*  141 */  CR( 8,  0,  B5, 0,  0,  0,  0),     /*  B5          */
	/*  142 */  CR( 8,  0,  F5, 0,  0,  0,  0),     /*  F5          */
				/* LOAD_2 only              */
	/*  143 */  CR( 5,  0,  IA, 0,  0,  0,  0),     /*  Ampl. Intr. */
	/*  144 */  CR( 5,  0,  IP, 0,  0,  0,  0),     /*  Per. Intr.  */

				/* LOAD_2, LOAD_C  Mode 11  */
	/*  145 */  CR( 6,  2,  AM, 0,  0,  0,  1),     /*  Amplitude   */
	/*  146 */  CR( 8,  0,  PR, 0,  0,  0,  0),     /*  Period      */
	/*  147 */  CR( 6,  1,  B0, 0,  0,  0,  0),     /*  B0 (S=0)    */
	/*  148 */  CR( 6,  2,  F0, 0,  0,  0,  0),     /*  F0          */
	/*  149 */  CR( 6,  1,  B1, 0,  0,  0,  0),     /*  B1 (S=0)    */
	/*  150 */  CR( 6,  2,  F1, 0,  0,  0,  0),     /*  F1          */
	/*  151 */  CR( 6,  1,  B2, 0,  0,  0,  0),     /*  B2 (S=0)    */
	/*  152 */  CR( 6,  2,  F2, 0,  0,  0,  0),     /*  F2          */
	/*  153 */  CR( 6,  1,  B3, 0,  0,  0,  0),     /*  B3 (S=0)    */
	/*  154 */  CR( 7,  1,  F3, 0,  0,  0,  0),     /*  F3          */
	/*  155 */  CR( 8,  0,  B4, 0,  0,  0,  0),     /*  B4          */
	/*  156 */  CR( 8,  0,  F4, 0,  0,  0,  0),     /*  F4          */
	/*  157 */  CR( 8,  0,  B5, 0,  0,  0,  0),     /*  B5          */
	/*  158 */  CR( 8,  0,  F5, 0,  0,  0,  0),     /*  F5          */
				/* LOAD_2 only              */
	/*  159 */  CR( 5,  0,  IA, 0,  0,  0,  0),     /*  Ampl. Intr. */
	/*  160 */  CR( 5,  0,  IP, 0,  0,  0,  0),     /*  Per. Intr.  */

	/* -------------------------------------------------------------------- */
	/*  Opcode 0011: SETMSB_3                                               */
	/*  Opcode 0101: SETMSB_5                                               */
	/* -------------------------------------------------------------------- */
				/* Mode 00 only             */
	/*  161 */  CR( 0,  0,  0,  0,  0,  1,  0),     /*  Clear 5     */
				/* Mode 00 and 01           */
	/*  162 */  CR( 6,  2,  AM, 0,  0,  0,  0),     /*  Amplitude   */
	/*  163 */  CR( 8,  0,  PR, 0,  0,  0,  0),     /*  Period      */
	/*  164 */  CR( 5,  3,  F0, 0,  1,  0,  0),     /*  F0 (5 MSBs) */
	/*  165 */  CR( 5,  3,  F1, 0,  1,  0,  0),     /*  F1 (5 MSBs) */
	/*  166 */  CR( 5,  3,  F2, 0,  1,  0,  0),     /*  F2 (5 MSBs) */
				/* SETMSB_3 only            */
	/*  167 */  CR( 5,  0,  IA, 0,  0,  0,  0),     /*  Ampl. Intr. */
	/*  168 */  CR( 5,  0,  IP, 0,  0,  0,  0),     /*  Per. Intr.  */

				/* Mode 10 only             */
	/*  169 */  CR( 0,  0,  0,  0,  0,  1,  0),     /*  Clear 5     */
				/* Mode 10 and 11           */
	/*  170 */  CR( 6,  2,  AM, 0,  0,  0,  0),     /*  Amplitude   */
	/*  171 */  CR( 8,  0,  PR, 0,  0,  0,  0),     /*  Period      */
	/*  172 */  CR( 6,  2,  F0, 0,  1,  0,  0),     /*  F0 (6 MSBs) */
	/*  173 */  CR( 6,  2,  F1, 0,  1,  0,  0),     /*  F1 (6 MSBs) */
	/*  174 */  CR( 6,  2,  F2, 0,  1,  0,  0),     /*  F2 (6 MSBs) */
				/* SETMSB_3 only            */
	/*  175 */  CR( 5,  0,  IA, 0,  0,  0,  0),     /*  Ampl. Intr. */
	/*  176 */  CR( 5,  0,  IP, 0,  0,  0,  0),     /*  Per. Intr.  */
};

static const INT16 sp0256_df_idx[16 * 8] =
{
	/*  OPCODE 0000 */      -1, -1,     -1, -1,     -1, -1,     -1, -1,
	/*  OPCODE 1000 */      -1, -1,     -1, -1,     -1, -1,     -1, -1,
	/*  OPCODE 0100 */      17, 22,     17, 24,     25, 30,     25, 32,
	/*  OPCODE 1100 */      83, 94,     129,142,    97, 108,    145,158,
	/*  OPCODE 0010 */      83, 96,     129,144,    97, 110,    145,160,
	/*  OPCODE 1010 */      73, 77,     74, 77,     78, 82,     79, 82,
	/*  OPCODE 0110 */      33, 36,     34, 37,     38, 41,     39, 42,
	/*  OPCODE 1110 */      127,128,    127,128,    127,128,    127,128,
	/*  OPCODE 0001 */      1,  14,     1,  16,     1,  14,     1,  16,
	/*  OPCODE 1001 */      45, 56,     45, 58,     59, 70,     59, 72,
	/*  OPCODE 0101 */      161,166,    162,166,    169,174,    170,174,
	/*  OPCODE 1101 */      111,116,    111,118,    119,124,    119,126,
	/*  OPCODE 0011 */      161,168,    162,168,    169,176,    170,176,
	/*  OPCODE 1011 */      -1, -1,     -1, -1,     -1, -1,     -1, -1,
	/*  OPCODE 0111 */      -1, -1,     -1, -1,     -1, -1,     -1, -1,
	/*  OPCODE 1111 */      0,  0,      0,  0,      0,  0,      0,  0
};

/* ======================================================================== */
/*  BITREV32       -- Bit-reverse a 32-bit number.                            */
/* ======================================================================== */
static UINT32 bitrev32(UINT32 val)
{
	val = ((val & 0xFFFF0000) >> 16) | ((val & 0x0000FFFF) << 16);
	val = ((val & 0xFF00FF00) >>  8) | ((val & 0x00FF00FF) <<  8);
	val = ((val & 0xF0F0F0F0) >>  4) | ((val & 0x0F0F0F0F) <<  4);
	val = ((val & 0xCCCCCCCC) >>  2) | ((val & 0x33333333) <<  2);
	val = ((val & 0xAAAAAAAA) >>  1) | ((val & 0x55555555) <<  1);

	return val;
}

#if 0
/* ======================================================================== */
/*  BITREV8       -- Bit-reverse a 8-bit number.                            */
/* ======================================================================== */
static UINT8 bitrev8(UINT8 val)
{
	val = ((val & 0xF0) >>  4) | ((val & 0x0F) <<  4);
	val = ((val & 0xCC) >>  2) | ((val & 0x33) <<  2);
	val = ((val & 0xAA) >>  1) | ((val & 0x55) <<  1);

	return val;
}

/* ======================================================================== */
/*  BITREVBUFF       -- Bit-reverse a buffer.                               */
/* ======================================================================== */
void bitrevbuff(UINT8 *buffer, UINT32 start, UINT32 length)
{
	for (UINT32 i = start; i < length; i++ )
		buffer[i] = bitrev8(buffer[i]);
}
#endif

/* ======================================================================== */
/*  SP0256_GETB  -- Get up to 8 bits at the current PC.                     */
/* ======================================================================== */
static UINT32 getb( INT32 len )
{
	UINT32 data = 0;

	/* -------------------------------------------------------------------- */
	/*  Fetch data from the FIFO or from the MASK                           */
	/* -------------------------------------------------------------------- */
	if (m_fifo_sel)
	{
		UINT32 d0, d1;
		d0 = m_fifo[(m_fifo_tail    ) & 63];
		d1 = m_fifo[(m_fifo_tail + 1) & 63];

		data = ((d1 << 10) | d0) >> m_fifo_bitp;

		LOG_FIFO((0, _T("sp0256: RD_FIFO %.3X %d.%d %d\n"), data & ((1 << len) - 1),
				m_fifo_tail, m_fifo_bitp, m_fifo_head));

		/* ---------------------------------------------------------------- */
		/*  Note the PC doesn't advance when we execute from FIFO.          */
		/*  Just the FIFO's bit-pointer advances.   (That's not REALLY      */
		/*  what happens, but that's roughly how it behaves.)               */
		/* ---------------------------------------------------------------- */
		m_fifo_bitp += len;
		if (m_fifo_bitp >= 10)
		{
			m_fifo_tail++;
			m_fifo_bitp -= 10;
		}
	} else
	{
		/* ---------------------------------------------------------------- */
		/*  Figure out which ROMs are being fetched into, and grab two      */
		/*  adjacent bytes.  The byte we're interested in is extracted      */
		/*  from the appropriate bit-boundary between them.                 */
		/* ---------------------------------------------------------------- */
		INT32 idx0 = (m_pc    ) >> 3, d0;
		INT32 idx1 = (m_pc + 8) >> 3, d1;

		d0 = m_rom[idx0 & 0xffff];
		d1 = m_rom[idx1 & 0xffff];

		data = ((d1 << 8) | d0) >> (m_pc & 7);

		m_pc += len;
	}

	/* -------------------------------------------------------------------- */
	/*  Mask data to the requested length.                                  */
	/* -------------------------------------------------------------------- */
	data &= ((1 << len) - 1);

	return data;
}

/* ======================================================================== */
/*  SP0256_MICRO -- Emulate the microsequencer in the SP0256.  Executes     */
/*                  instructions either until the repeat count != 0 or      */
/*                  the sequencer gets halted by a RTS to 0.                */
/* ======================================================================== */
static void micro()
{
	UINT8  immed4;
	UINT8  opcode;
	UINT16 cr;
	INT32 ctrl_xfer = 0;
	INT32 repeat    = 0;
	INT32 i, idx0, idx1;

	/* -------------------------------------------------------------------- */
	/*  Only execute instructions while the filter is not busy.             */
	/* -------------------------------------------------------------------- */
	while (m_filt.rpt <= 0)
	{
		/* ---------------------------------------------------------------- */
		/*  If the CPU is halted, see if we have a new command pending      */
		/*  in the Address LoaD buffer.                                     */
		/* ---------------------------------------------------------------- */
		if (m_halted && !m_lrq)
		{
			m_pc       = m_ald | (0x1000 << 3);
			m_fifo_sel = 0;
			m_halted   = 0;
			m_lrq      = 0x8000;
			m_ald      = 0;
			for (i = 0; i < 16; i++)
				m_filt.r[i] = 0;
			m_drq_cb(1);
		}

		/* ---------------------------------------------------------------- */
		/*  If we're still halted, do nothing.                              */
		/* ---------------------------------------------------------------- */
		if (m_halted)
		{
			m_filt.rpt = 1;
			m_lrq      = 0x8000;
			m_ald      = 0;
			for (i = 0; i < 16; i++)
				m_filt.r[i] = 0;

			SET_SBY(1)

			return;
		}

		/* ---------------------------------------------------------------- */
		/*  Fetch the first 8 bits of the opcode, which are always in the   */
		/*  same approximate format -- immed4 followed by opcode.           */
		/* ---------------------------------------------------------------- */
		immed4 = getb(4);
		opcode = getb(4);
		repeat = 0;
		ctrl_xfer = 0;

		LOG((0, _T("$%.4X.%.1X: OPCODE %d%d%d%d.%d%d\n"),
				(m_pc >> 3) - 1, m_pc & 7,
				!!(opcode & 1), !!(opcode & 2),
				!!(opcode & 4), !!(opcode & 8),
				!!(m_mode&4), !!(m_mode&2)));

		/* ---------------------------------------------------------------- */
		/*  Handle the special cases for specific opcodes.                  */
		/* ---------------------------------------------------------------- */
		switch (opcode)
		{
			/* ------------------------------------------------------------ */
			/*  OPCODE 0000:  RTS / SETPAGE                                 */
			/* ------------------------------------------------------------ */
			case 0x0:
			{
				/* -------------------------------------------------------- */
				/*  If immed4 != 0, then this is a SETPAGE instruction.     */
				/* -------------------------------------------------------- */
				if (immed4)     /* SETPAGE */
				{
					m_page = bitrev32(immed4) >> 13;
				} else
				/* -------------------------------------------------------- */
				/*  Otherwise, this is an RTS / HLT.                        */
				/* -------------------------------------------------------- */
				{
					UINT32 btrg;

					/* ---------------------------------------------------- */
					/*  Figure out our branch target.                       */
					/* ---------------------------------------------------- */
					btrg = m_stack;

					m_stack = 0;

					/* ---------------------------------------------------- */
					/*  If the branch target is zero, this is a HLT.        */
					/*  Otherwise, it's an RTS, so set the PC.              */
					/* ---------------------------------------------------- */
					if (!btrg)
					{
						m_halted   = 1;
						m_pc       = 0;
						ctrl_xfer  = 1;
					} else
					{
						m_pc      = btrg;
						ctrl_xfer = 1;
					}
				}

				break;
			}

			/* ------------------------------------------------------------ */
			/*  OPCODE 0111:  JMP          Jump to 12-bit/16-bit Abs Addr   */
			/*  OPCODE 1011:  JSR          Jump to Subroutine               */
			/* ------------------------------------------------------------ */
			case 0xE:
			case 0xD:
			{
				INT32 btrg;

				/* -------------------------------------------------------- */
				/*  Figure out our branch target.                           */
				/* -------------------------------------------------------- */
				btrg = m_page                     |
						(bitrev32(immed4)  >> 17) |
						(bitrev32(getb(8)) >> 21);
				ctrl_xfer = 1;

				/* -------------------------------------------------------- */
				/*  If this is a JSR, push our return address on the        */
				/*  stack.  Make sure it's byte aligned.                    */
				/* -------------------------------------------------------- */
				if (opcode == 0xD)
					m_stack = (m_pc + 7) & ~7;

				/* -------------------------------------------------------- */
				/*  Jump to the new location!                               */
				/* -------------------------------------------------------- */
				m_pc = btrg;
				break;
			}

			/* ------------------------------------------------------------ */
			/*  OPCODE 1000:  SETMODE      Set the Mode and Repeat MSBs     */
			/* ------------------------------------------------------------ */
			case 0x1:
			{
				m_mode = ((immed4 & 8) >> 2) | (immed4 & 4) | ((immed4 & 3) << 4);
				break;
			}

			/* ------------------------------------------------------------ */
			/*  OPCODE 0001:  LOADALL      Load All Parameters              */
			/*  OPCODE 0010:  LOAD_2       Load Per, Ampl, Coefs, Interp.   */
			/*  OPCODE 0011:  SETMSB_3     Load Pitch, Ampl, MSBs, & Intrp  */
			/*  OPCODE 0100:  LOAD_4       Load Pitch, Ampl, Coeffs         */
			/*  OPCODE 0101:  SETMSB_5     Load Pitch, Ampl, and Coeff MSBs */
			/*  OPCODE 0110:  SETMSB_6     Load Ampl, and Coeff MSBs.       */
			/*  OPCODE 1001:  DELTA_9      Delta update Ampl, Pitch, Coeffs */
			/*  OPCODE 1010:  SETMSB_A     Load Ampl and MSBs of 3 Coeffs   */
			/*  OPCODE 1100:  LOAD_C       Load Pitch, Ampl, Coeffs         */
			/*  OPCODE 1101:  DELTA_D      Delta update Ampl, Pitch, Coeffs */
			/*  OPCODE 1110:  LOAD_E       Load Pitch, Amplitude            */
			/*  OPCODE 1111:  PAUSE        Silent pause                     */
			/* ------------------------------------------------------------ */
			default:
			{
				repeat = immed4 | (m_mode & 0x30);
				break;
			}
		}
		if (opcode != 1) m_mode &= 0xF;

		/* ---------------------------------------------------------------- */
		/*  If this was a control transfer, handle setting "fifo_sel"       */
		/*  and all that ugliness.                                          */
		/* ---------------------------------------------------------------- */
		if (ctrl_xfer)
		{
			LOG((0, _T("jumping to $%.4X.%.1X: "), m_pc >> 3, m_pc & 7));

			/* ------------------------------------------------------------ */
			/*  Set our "FIFO Selected" flag based on whether we're going   */
			/*  to the FIFO's address.                                      */
			/* ------------------------------------------------------------ */
			m_fifo_sel = m_pc == FIFO_ADDR;

			LOG((0, _T("%s "), m_fifo_sel ? "FIFO" : "ROM"));

			/* ------------------------------------------------------------ */
			/*  Control transfers to the FIFO cause it to discard the       */
			/*  partial decle that's at the front of the FIFO.              */
			/* ------------------------------------------------------------ */
			if (m_fifo_sel && m_fifo_bitp)
			{
				LOG((0, _T("bitp = %d -> Flush"), m_fifo_bitp));

				/* Discard partially-read decle. */
				if (m_fifo_tail < m_fifo_head) m_fifo_tail++;
				m_fifo_bitp = 0;
			}

			LOG((0, _T("\n")));

			continue;
		}

		/* ---------------------------------------------------------------- */
		/*  Otherwise, if we have a repeat count, then go grab the data     */
		/*  block and feed it to the filter.                                */
		/* ---------------------------------------------------------------- */
		if (!repeat) continue;

		m_filt.rpt = repeat + 1;
		LOG((0, _T("repeat = %d\n"), repeat));

		i = (opcode << 3) | (m_mode & 6);
		idx0 = sp0256_df_idx[i++];
		idx1 = sp0256_df_idx[i  ];

		//assert(idx0 >= 0 && idx1 >= 0 && idx1 >= idx0);

		/* ---------------------------------------------------------------- */
		/*  Step through control words in the description for data block.   */
		/* ---------------------------------------------------------------- */
		for (i = idx0; i <= idx1; i++)
		{
			INT32 len, shf, delta, field, prm, clra, clr5;
			INT8 value;

			/* ------------------------------------------------------------ */
			/*  Get the control word and pull out some important fields.    */
			/* ------------------------------------------------------------ */
			cr = sp0256_datafmt[i];

			len   = CR_LEN(cr);
			shf   = CR_SHF(cr);
			prm   = CR_PRM(cr);
			clra  = cr & CR_CLRA;
			clr5  = cr & CR_CLR5;
			delta = cr & CR_DELTA;
			field = cr & CR_FIELD;
			value = 0;

			LOG((0, _T("$%.4X.%.1X: len=%2d shf=%2d prm=%2d d=%d f=%d "),
						m_pc >> 3, m_pc & 7, len, shf, prm, !!delta, !!field));
			/* ------------------------------------------------------------ */
			/*  Clear any registers that were requested to be cleared.      */
			/* ------------------------------------------------------------ */
			if (clra)
			{
				for (INT32 j = 0; j < 16; j++)
					m_filt.r[j] = 0;

				m_silent = 1;
			}

			if (clr5)
				m_filt.r[B5] = m_filt.r[F5] = 0;

			/* ------------------------------------------------------------ */
			/*  If this entry has a bitfield with it, grab the bitfield.    */
			/* ------------------------------------------------------------ */
			if (len)
			{
				value = getb(len);
			}
			else
			{
				LOG((0, _T(" (no update)\n")));
				continue;
			}

			/* ------------------------------------------------------------ */
			/*  Sign extend if this is a delta update.                      */
			/* ------------------------------------------------------------ */
			if (delta)  /* Sign extend */
			{
				if (value & (1 << (len - 1))) value |= -1 << len;
			}

			/* ------------------------------------------------------------ */
			/*  Shift the value to the appropriate precision.               */
			/* ------------------------------------------------------------ */
			if (shf)
				value <<= shf;

			LOG((0, _T("v=%.2X (%c%.2X)  "), value & 0xFF,
						value & 0x80 ? '-' : '+',
						0xFF & (value & 0x80 ? -value : value)));

			m_silent = 0;

			/* ------------------------------------------------------------ */
			/*  If this is a field-replace, insert the field.               */
			/* ------------------------------------------------------------ */
			if (field)
			{
				LOG((0, _T("--field-> r[%2d] = %.2X -> "), prm, m_filt.r[prm]));

				m_filt.r[prm] &= ~(~0 << shf); /* Clear the old bits.     */
				m_filt.r[prm] |= value;        /* Merge in the new bits.  */

				LOG((0, _T("%.2X\n"), m_filt.r[prm]));

				continue;
			}

			/* ------------------------------------------------------------ */
			/*  If this is a delta update, add to the appropriate field.    */
			/* ------------------------------------------------------------ */
			if (delta)
			{
				LOG((0, _T("--delta-> r[%2d] = %.2X -> "), prm, m_filt.r[prm]));

				m_filt.r[prm] += value;

				LOG((0, _T("%.2X\n"), m_filt.r[prm]));

				continue;
			}

			/* ------------------------------------------------------------ */
			/*  Otherwise, just write the new value.                        */
			/* ------------------------------------------------------------ */
			m_filt.r[prm] = value;
			LOG((0, _T("--value-> r[%2d] = %.2X\n"), prm, m_filt.r[prm]));
		}

		/* ---------------------------------------------------------------- */
		/*  Special case:  Set PAUSE's equivalent period.                   */
		/* ---------------------------------------------------------------- */
		if (opcode == 0xF)
		{
			m_silent = 1;
			m_filt.r[1] = PER_PAUSE;
		}

		/* ---------------------------------------------------------------- */
		/*  Now that we've updated the registers, go decode them.           */
		/* ---------------------------------------------------------------- */
		lpc12_regdec(&m_filt);

		/* ---------------------------------------------------------------- */
		/*  Break out since we now have a repeat count.                     */
		/* ---------------------------------------------------------------- */
		break;
	}
}

void sp0256_ald_write(UINT8 data)
{
	/* ---------------------------------------------------------------- */
	/*  Drop writes to the ALD register if we're busy.                  */
	/* ---------------------------------------------------------------- */

	if (!m_lrq)
	{
		LOG((0, _T("sp0256: Droped ALD write\n")));
		return;
	}

	/* ---------------------------------------------------------------- */
	/*  Set LRQ to "busy" and load the 8 LSBs of the data into the ALD  */
	/*  reg.  We take the command address, and multiply by 2 bytes to   */
	/*  get the new PC address.                                         */
	/* ---------------------------------------------------------------- */
	m_lrq = 0;
	m_ald = (0xff & data) << 4;
	m_drq_cb(0);
	SET_SBY(0)

	return;
}

UINT8 sp0256_lrq_read()
{
	// force stream update
	//m_stream->update();

	return m_lrq == 0x8000;
}

UINT8 sp0256_sby_read()
{
	// TODO: force stream update??

	return m_sby_line;
}

UINT16 sp0256_spb640_read(INT32 offset)
{
	/* -------------------------------------------------------------------- */
	/*  Offset 0 returns the SP0256 LRQ status on bit 15.                   */
	/* -------------------------------------------------------------------- */
	if (offset == 0)
	{
		return m_lrq;
	}

	/* -------------------------------------------------------------------- */
	/*  Offset 1 returns the SPB640 FIFO full status on bit 15.             */
	/* -------------------------------------------------------------------- */
	if (offset == 1)
	{
		return (m_fifo_head - m_fifo_tail) >= 64 ? 0x8000 : 0;
	}

	/* -------------------------------------------------------------------- */
	/*  Just return 255 for all other addresses in our range.               */
	/* -------------------------------------------------------------------- */
	return 0x00ff;
}

void sp0256_spb640_write(UINT16 offset, UINT16 data)
{
	if (offset == 0)
	{
		sp0256_ald_write(data & 0xff);
		return;
	}

	if (offset == 1)
	{
		/* ---------------------------------------------------------------- */
		/*  If Bit 10 is set, reset the FIFO, and SP0256.                   */
		/* ---------------------------------------------------------------- */

		if (data & 0x400)
		{
			m_fifo_head = m_fifo_tail = m_fifo_bitp = 0;
			sp0256_reset();
			return;
		}

		/* ---------------------------------------------------------------- */
		/*  If the FIFO is full, drop the data.                             */
		/* ---------------------------------------------------------------- */
		if ((m_fifo_head - m_fifo_tail) >= 64)
		{
			LOG((0, _T("spb640: Dropped FIFO write\n")));
			return;
		}

		/* ---------------------------------------------------------------- */
		/*  FIFO up the lower 10 bits of the data.                          */
		/* ---------------------------------------------------------------- */

		LOG((0, _T("spb640: WR_FIFO %.3X %d.%d %d\n"), data & 0x3ff,
				m_fifo_tail, m_fifo_bitp, m_fifo_head));

		m_fifo[m_fifo_head++ & 63] = data & 0x3ff;

		return;
	}
}

static void set_lrq_timer_proc()
{
	m_lrq = 0x8000;
}

//-------------------------------------------------
//  sound_stream_update - handle a stream update
//-------------------------------------------------
void sp0256_update(INT16 *sndbuff, INT32 samples_len)
{
	INT32 output_index = 0;
	INT32 length, did_samp;

	INT32 samples = (samples_from * samples_len) / nBurnSoundLen;

	memset(mixer_buffer, 0, samples * sizeof(INT16));

	while (output_index < samples)
	{
		/* ---------------------------------------------------------------- */
		/*  First, drain as much of our scratch buffer as we can into the   */
		/*  sound buffer.                                                  */
		/* ---------------------------------------------------------------- */

		while (m_sc_tail != m_sc_head)
		{
			mixer_buffer[output_index++] = m_scratch[m_sc_tail++ & SCBUF_MASK];
			m_sc_tail &= SCBUF_MASK;

			if (output_index > samples)
				break;
		}

		/* ---------------------------------------------------------------- */
		/*  If output outputs is full, then we're done.                      */
		/* ---------------------------------------------------------------- */
		if (output_index > samples)
			break;

		length = samples - output_index;

		/* ---------------------------------------------------------------- */
		/*  Process the current set of filter coefficients as long as the   */
		/*  repeat count holds up and we have room in our scratch buffer.   */
		/* ---------------------------------------------------------------- */
		did_samp = 0;

		if (length > 0) do
		{
			INT32 do_samp;

			/* ------------------------------------------------------------ */
			/*  If our repeat count expired, emulate the microsequencer.    */
			/* ------------------------------------------------------------ */
			if (m_filt.rpt <= 0)
				micro();

			/* ------------------------------------------------------------ */
			/*  Do as many samples as we can.                               */
			/* ------------------------------------------------------------ */
			do_samp = length - did_samp;
			if (m_sc_head + do_samp - m_sc_tail > SCBUF_SIZE)
				do_samp = m_sc_tail + SCBUF_SIZE - m_sc_head;

			if (do_samp == 0) break;

			if (m_silent && m_filt.rpt <= 0)
			{
				INT32 y = m_sc_head;

				for (INT32 x = 0; x < do_samp; x++)
					m_scratch[y++ & SCBUF_MASK] = 0;
				m_sc_head += do_samp;
				did_samp  += do_samp;
			}
			else
			{
				did_samp += lpc12_update(&m_filt, do_samp,
										 m_scratch, &m_sc_head);
			}
			if ((length%READY_TIMER)==(READY_TIMER-1)) set_lrq_timer_proc();
			m_sc_head &= SCBUF_MASK;

		} while (m_filt.rpt >= 0 && length > did_samp);
	}

	for (INT32 j = 0; j < samples_len; j++)
	{
		INT32 k = (samples_from * j) / nBurnSoundLen;

		INT32 lr = mixer_buffer[k];
		sndbuff[0] = BURN_SND_CLIP(sndbuff[0] + lr);
		sndbuff[1] = BURN_SND_CLIP(sndbuff[1] + lr);
		sndbuff += 2;
	}
}
