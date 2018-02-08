// Based on MAME source by Juergen Buchmueller

#include "burnint.h"
#include "tms36xx.h"

#define VERBOSE 0

#if VERBOSE
#define LOG(x) logerror x
#else
#define LOG(x)
#endif

#define VMIN	0x0000
#define VMAX	0x7fff

/* the frequencies are later adjusted by "* clock / FSCALE" */
#define FSCALE	1024

struct TMS36XX {
	INT32 samplerate; 	/* from Machine->sample_rate */

	INT32 basefreq;		/* chip's base frequency */
	INT32 octave; 		/* octave select of the TMS3615 */

	INT32 speed;			/* speed of the tune */
	INT32 tune_counter;	/* tune counter */
	INT32 note_counter;	/* note counter */

	INT32 voices; 		/* active voices */
	INT32 shift;			/* shift toggles between 0 and 6 to allow decaying voices */
	INT32 vol[12];		/* (decaying) volume of harmonics notes */
	INT32 vol_counter[12];/* volume adjustment counter */
	INT32 decay[12];		/* volume adjustment rate - dervied from decay */

	INT32 counter[12];	/* tone frequency counter */
	INT32 frequency[12];	/* tone frequency */
	INT32 output; 		/* output signal bits */
	INT32 enable; 		/* mask which harmoics */

	INT32 tune_num;		/* tune currently playing */
	INT32 tune_ofs;		/* note currently playing */
	INT32 tune_max;		/* end of tune */
};

struct TMS36XX *tms = NULL;

#define C(n)	(int)((FSCALE<<(n-1))*1.18921)	/* 2^(3/12) */
#define Cx(n)	(int)((FSCALE<<(n-1))*1.25992)	/* 2^(4/12) */
#define D(n)	(int)((FSCALE<<(n-1))*1.33484)	/* 2^(5/12) */
#define Dx(n)	(int)((FSCALE<<(n-1))*1.41421)	/* 2^(6/12) */
#define E(n)	(int)((FSCALE<<(n-1))*1.49831)	/* 2^(7/12) */
#define F(n)	(int)((FSCALE<<(n-1))*1.58740)	/* 2^(8/12) */
#define Fx(n)	(int)((FSCALE<<(n-1))*1.68179)	/* 2^(9/12) */
#define G(n)	(int)((FSCALE<<(n-1))*1.78180)	/* 2^(10/12) */
#define Gx(n)	(int)((FSCALE<<(n-1))*1.88775)	/* 2^(11/12) */
#define A(n)	(int)((FSCALE<<n))				/* A */
#define Ax(n)	(int)((FSCALE<<n)*1.05946)		/* 2^(1/12) */
#define B(n)	(int)((FSCALE<<n)*1.12246)		/* 2^(2/12) */

/*
 * Alarm sound?
 * It is unknown what this sound is like. Until somebody manages
 * trigger sound #1 of the Phoenix PCB sound chip I put just something
 * 'alarming' in here.
 */
static const INT32 tune1[96*6] = {
	C(3),	0,		0,		C(2),	0,		0,
	G(3),	0,		0,		0,		0,		0,
	C(3),	0,		0,		0,		0,		0,
	G(3),	0,		0,		0,		0,		0,
	C(3),	0,		0,		0,		0,		0,
	G(3),	0,		0,		0,		0,		0,
	C(3),	0,		0,		0,		0,		0,
	G(3),	0,		0,		0,		0,		0,
	C(3),	0,		0,		C(4),	0,		0,
	G(3),	0,		0,		0,		0,		0,
	C(3),	0,		0,		0,		0,		0,
	G(3),	0,		0,		0,		0,		0,
	C(3),	0,		0,		0,		0,		0,
	G(3),	0,		0,		0,		0,		0,
	C(3),	0,		0,		0,		0,		0,
	G(3),	0,		0,		0,		0,		0,
	C(3),	0,		0,		C(2),	0,		0,
	G(3),	0,		0,		0,		0,		0,
	C(3),	0,		0,		0,		0,		0,
	G(3),	0,		0,		0,		0,		0,
	C(3),	0,		0,		0,		0,		0,
	G(3),	0,		0,		0,		0,		0,
	C(3),	0,		0,		0,		0,		0,
	G(3),	0,		0,		0,		0,		0,
	C(3),	0,		0,		C(4),	0,		0,
	G(3),	0,		0,		0,		0,		0,
	C(3),	0,		0,		0,		0,		0,
	G(3),	0,		0,		0,		0,		0,
	C(3),	0,		0,		0,		0,		0,
	G(3),	0,		0,		0,		0,		0,
	C(3),	0,		0,		0,		0,		0,
	G(3),	0,		0,		0,		0,		0,
};

/*
 * Fuer Elise, Beethoven
 * (Excuse my non-existent musical skill, Mr. B ;-)
 */
static const INT32 tune2[96*6] = {
	D(3),	D(4),	D(5),	0,		0,		0,
	Cx(3),	Cx(4),	Cx(5),	0,		0,		0,
	D(3),	D(4),	D(5),	0,		0,		0,
	Cx(3),	Cx(4),	Cx(5),	0,		0,		0,
	D(3),	D(4),	D(5),	0,		0,		0,
	A(2),	A(3),	A(4),	0,		0,		0,
	C(3),	C(4),	C(5),	0,		0,		0,
	Ax(2),	Ax(3),	Ax(4),	0,		0,		0,
	G(2),	G(3),	G(4),	0,		0,		0,
	D(1),	D(2),	D(3),	0,		0,		0,
	G(1),	G(2),	G(3),	0,		0,		0,
	Ax(1),	Ax(2),	Ax(3),	0,		0,		0,

	D(2),	D(3),	D(4),	0,		0,		0,
	G(2),	G(3),	G(4),	0,		0,		0,
	A(2),	A(3),	A(4),	0,		0,		0,
	D(1),	D(2),	D(3),	0,		0,		0,
	A(1),	A(2),	A(3),	0,		0,		0,
	D(2),	D(3),	D(4),	0,		0,		0,
	Fx(2),	Fx(3),	Fx(4),	0,		0,		0,
	A(2),	A(3),	A(4),	0,		0,		0,
	Ax(2),	Ax(3),	Ax(4),	0,		0,		0,
	D(1),	D(2),	D(3),	0,		0,		0,
	G(1),	G(2),	G(3),	0,		0,		0,
	Ax(1),	Ax(2),	Ax(3),	0,		0,		0,

	D(3),	D(4),	D(5),	0,		0,		0,
	Cx(3),	Cx(4),	Cx(5),	0,		0,		0,
	D(3),	D(4),	D(5),	0,		0,		0,
	Cx(3),	Cx(4),	Cx(5),	0,		0,		0,
	D(3),	D(4),	D(5),	0,		0,		0,
	A(2),	A(3),	A(4),	0,		0,		0,
	C(3),	C(4),	C(5),	0,		0,		0,
	Ax(2),	Ax(3),	Ax(4),	0,		0,		0,
	G(2),	G(3),	G(4),	0,		0,		0,
	D(1),	D(2),	D(3),	0,		0,		0,
	G(1),	G(2),	G(3),	0,		0,		0,
	Ax(1),	Ax(2),	Ax(3),	0,		0,		0,

	D(2),	D(3),	D(4),	0,		0,		0,
	G(2),	G(3),	G(4),	0,		0,		0,
	A(2),	A(3),	A(4),	0,		0,		0,
	D(1),	D(2),	D(3),	0,		0,		0,
	A(1),	A(2),	A(3),	0,		0,		0,
	D(2),	D(3),	D(4),	0,		0,		0,
	Ax(2),	Ax(3),	Ax(4),	0,		0,		0,
	A(2),	A(3),	A(4),	0,		0,		0,
	0,		0,		0,		G(2),	G(3),	G(4),
	D(1),	D(2),	D(3),	0,		0,		0,
	G(1),	G(2),	G(3),	0,		0,		0,
	0,		0,		0,		0,		0,		0
};

/*
 * The theme from Phoenix, a sad little tune.
 * Gerald Coy:
 *   The starting song from Phoenix is coming from a old french movie and
 *   it's called : "Jeux interdits" which means "unallowed games"  ;-)
 * Mirko Buffoni:
 *   It's called "Sogni proibiti" in italian, by Anonymous.
 * Magic*:
 *   This song is a classical piece called "ESTUDIO" from M.A.Robira.
 */
static const INT32 tune3[96*6] = {
	A(2),	A(3),	A(4),	D(1),	 D(2),	  D(3),
	0,		0,		0,		0,		 0, 	  0,
	A(2),	A(3),	A(4),	0,		 0, 	  0,
	0,		0,		0,		0,		 0, 	  0,
	A(2),	A(3),	A(4),	0,		 0, 	  0,
	0,		0,		0,		0,		 0, 	  0,

	A(2),	A(3),	A(4),	A(1),	 A(2),	  A(3),
	0,		0,		0,		0,		 0, 	  0,
	G(2),	G(3),	G(4),	0,		 0, 	  0,
	0,		0,		0,		0,		 0, 	  0,
	F(2),	F(3),	F(4),	0,		 0, 	  0,
	0,		0,		0,		0,		 0, 	  0,

	F(2),	F(3),	F(4),	F(1),	 F(2),	  F(3),
	0,		0,		0,		0,		 0, 	  0,
	E(2),	E(3),	E(4),	F(1),	 F(2),	  F(3),
	0,		0,		0,		0,		 0, 	  0,
	D(2),	D(3),	D(4),	F(1),	 F(2),	  F(3),
	0,		0,		0,		0,		 0, 	  0,

	D(2),	D(3),	D(4),	A(1),	 A(2),	  A(3),
	0,		0,		0,		0,		 0, 	  0,
	F(2),	F(3),	F(4),	0,		 0, 	  0,
	0,		0,		0,		0,		 0, 	  0,
	A(2),	A(3),	A(4),	0,		 0, 	  0,
	0,		0,		0,		0,		 0, 	  0,

	D(3),	D(4),	D(5),	D(1),	 D(2),	  D(3),
	0,		0,		0,		0,		 0, 	  0,
	0,		0,		0,		D(1),	 D(2),	  D(3),
	0,		0,		0,		F(1),	 F(2),	  F(3),
	0,		0,		0,		A(1),	 A(2),	  A(3),
	0,		0,		0,		D(2),	 D(2),	  D(2),

	D(3),	D(4),	D(5),	D(1),	 D(2),	  D(3),
	0,		0,		0,		0,		 0, 	  0,
	C(3),	C(4),	C(5),	0,		 0, 	  0,
	0,		0,		0,		0,		 0, 	  0,
	Ax(2),	Ax(3),	Ax(4),	0,		 0, 	  0,
	0,		0,		0,		0,		 0, 	  0,

	Ax(2),	Ax(3),	Ax(4),	Ax(1),	 Ax(2),   Ax(3),
	0,		0,		0,		0,		 0, 	  0,
	A(2),	A(3),	A(4),	0,		 0, 	  0,
	0,		0,		0,		0,		 0, 	  0,
	G(2),	G(3),	G(4),	0,		 0, 	  0,
	0,		0,		0,		0,		 0, 	  0,

	G(2),	G(3),	G(4),	G(1),	 G(2),	  G(3),
	0,		0,		0,		0,		 0, 	  0,
	A(2),	A(3),	A(4),	0,		 0, 	  0,
	0,		0,		0,		0,		 0, 	  0,
	Ax(2),	Ax(3),	Ax(4),	0,		 0, 	  0,
	0,		0,		0,		0,		 0, 	  0,

	A(2),	A(3),	A(4),	A(1),	 A(2),	  A(3),
	0,		0,		0,		0,		 0, 	  0,
	Ax(2),	Ax(3),	Ax(4),	0,		 0, 	  0,
	0,		0,		0,		0,		 0, 	  0,
	A(2),	A(3),	A(4),	0,		 0, 	  0,
	0,		0,		0,		0,		 0, 	  0,

	Cx(3),	Cx(4),	Cx(5),	A(1),	 A(2),	  A(3),
	0,		0,		0,		0,		 0, 	  0,
	Ax(2),	Ax(3),	Ax(4),	0,		 0, 	  0,
	0,		0,		0,		0,		 0, 	  0,
	A(2),	A(3),	A(4),	0,		 0, 	  0,
	0,		0,		0,		0,		 0, 	  0,

	A(2),	A(3),	A(4),	F(1),	 F(2),	  F(3),
	0,		0,		0,		0,		 0, 	  0,
	G(2),	G(3),	G(4),	0,		 0, 	  0,
	0,		0,		0,		0,		 0, 	  0,
	F(2),	F(3),	F(4),	0,		 0, 	  0,
	0,		0,		0,		0,		 0, 	  0,

	F(2),	F(3),	F(4),	D(1),	 D(2),	  D(3),
	0,		0,		0,		0,		 0, 	  0,
	E(2),	E(3),	E(4),	0,		 0, 	  0,
	0,		0,		0,		0,		 0, 	  0,
	D(2),	D(3),	D(4),	0,		 0, 	  0,
	0,		0,		0,		0,		 0, 	  0,

	E(2),	E(3),	E(4),	E(1),	 E(2),	  E(3),
	0,		0,		0,		0,		 0, 	  0,
	E(2),	E(3),	E(4),	0,		 0, 	  0,
	0,		0,		0,		0,		 0, 	  0,
	E(2),	E(3),	E(4),	0,		 0, 	  0,
	0,		0,		0,		0,		 0, 	  0,

	E(2),	E(3),	E(4),	Ax(1),	 Ax(2),   Ax(3),
	0,		0,		0,		0,		 0, 	  0,
	F(2),	F(3),	F(4),	0,		 0, 	  0,
	0,		0,		0,		0,		 0, 	  0,
	E(2),	E(3),	E(4),	F(1),	 F(2),	  F(3),
	0,		0,		0,		0,		 0, 	  0,

	D(2),	D(3),	D(4),	D(1),	 D(2),	  D(3),
	0,		0,		0,		0,		 0, 	  0,
	F(2),	F(3),	F(4),	A(1),	 A(2),	  A(3),
	0,		0,		0,		0,		 0, 	  0,
	A(2),	A(3),	A(4),	F(1),	 F(2),	  F(3),
	0,		0,		0,		0,		 0, 	  0,

	D(3),	D(4),	D(5),	D(1),	 D(2),	  D(3),
	0,		0,		0,		0,		 0, 	  0,
	0,		0,		0,		0,		 0, 	  0,
	0,		0,		0,		0,		 0, 	  0,
	0,		0,		0,		0,		 0, 	  0,
	0,		0,		0,		0,		 0, 	  0
};

/* This is used to play single notes for the TMS3615/TMS3617 */
static const INT32 tune4[13*6] = {
/*  16'     8'      5 1/3'  4'      2 2/3'  2'      */
	B(0),	B(1),	Dx(2),	B(2),	Dx(3),	B(3),
	C(1),	C(2),	E(2),	C(3),	E(3),	C(4),
	Cx(1),	Cx(2),	F(2),	Cx(3),	F(3),	Cx(4),
	D(1),	D(2),	Fx(2),	D(3),	Fx(3),	D(4),
	Dx(1),	Dx(2),	G(2),	Dx(3),	G(3),	Dx(4),
	E(1),	E(2),	Gx(2),	E(3),	Gx(3),	E(4),
	F(1),	F(2),	A(2),	F(3),	A(3),	F(4),
	Fx(1),	Fx(2),	Ax(2),	Fx(3),	Ax(3),	Fx(4),
	G(1),	G(2),	B(2),	G(3),	B(3),	G(4),
	Gx(1),	Gx(2),	C(3),	Gx(3),	C(4),	Gx(4),
	A(1),	A(2),	Cx(3),	A(3),	Cx(4),	A(4),
	Ax(1),	Ax(2),	D(3),	Ax(3),	D(4),	Ax(4),
	B(1),	B(2),	Dx(3),	B(3),	Dx(4),	B(4)
};

static const INT32 *tunes[] = {NULL,tune1,tune2,tune3,tune4};

#define DECAY(voice)											\
	if( tms->vol[voice] > VMIN )								\
	{															\
		/* decay of first voice */								\
		tms->vol_counter[voice] -= tms->decay[voice];			\
		while( tms->vol_counter[voice] <= 0 )					\
		{														\
			tms->vol_counter[voice] += samplerate;				\
			if( tms->vol[voice]-- <= VMIN ) 					\
			{													\
				tms->frequency[voice] = 0;						\
				tms->vol[voice] = VMIN; 						\
				break;											\
			}													\
		}														\
	}

#define RESTART(voice)											\
	if( tunes[tms->tune_num][tms->tune_ofs*6+voice] )			\
	{															\
		tms->frequency[tms->shift+voice] =						\
			tunes[tms->tune_num][tms->tune_ofs*6+voice] *		\
			(tms->basefreq << tms->octave) / FSCALE;			\
		tms->vol[tms->shift+voice] = VMAX;						\
	}

#define TONE(voice)                                             \
	if( (tms->enable & (1<<voice)) && tms->frequency[voice] )	\
	{															\
		/* first note */										\
		tms->counter[voice] -= tms->frequency[voice];			\
		while( tms->counter[voice] <= 0 )						\
		{														\
			tms->counter[voice] += samplerate;					\
			tms->output ^= 1 << voice;							\
		}														\
		if (tms->output & tms->enable & (1 << voice))			\
			sum += tms->vol[voice]; 							\
	}



void tms36xx_sound_update(INT16 *buffer, INT32 length)
{
	INT32 samplerate = tms->samplerate;

    /* no tune played? */
	if( !tunes[tms->tune_num] || tms->voices == 0 ) {
		return;
	}

	while( length-- > 0 )
	{
		INT32 sum = 0;

		/* decay the twelve voices */
		DECAY( 0) DECAY( 1) DECAY( 2) DECAY( 3) DECAY( 4) DECAY( 5)
		DECAY( 6) DECAY( 7) DECAY( 8) DECAY( 9) DECAY(10) DECAY(11)

		/* musical note timing */
		tms->tune_counter -= tms->speed;
		if( tms->tune_counter <= 0 )
		{
			INT32 n = (-tms->tune_counter / samplerate) + 1;
			tms->tune_counter += n * samplerate;

			if( (tms->note_counter -= n) <= 0 )
			{
				tms->note_counter += VMAX;
				if (tms->tune_ofs < tms->tune_max)
				{
					/* shift to the other 'bank' of voices */
                    tms->shift ^= 6;
					/* restart one 'bank' of voices */
					RESTART(0) RESTART(1) RESTART(2)
					RESTART(3) RESTART(4) RESTART(5)
					tms->tune_ofs++;
				}
			}
		}

		/* update the twelve voices */
		TONE( 0) TONE( 1) TONE( 2) TONE( 3) TONE( 4) TONE( 5)
		TONE( 6) TONE( 7) TONE( 8) TONE( 9) TONE(10) TONE(11)

		INT16 sam = (sum / tms->voices) * 0.60;
		*buffer = BURN_SND_CLIP(*buffer + sam);
		buffer++;
		*buffer = BURN_SND_CLIP(*buffer + sam);
		buffer++;
	}
}

static void tms36xx_reset_counters()
{
    tms->tune_counter = 0;
    tms->note_counter = 0;
	memset(tms->vol_counter, 0, sizeof(tms->vol_counter));
	memset(tms->counter, 0, sizeof(tms->counter));
}

void tms36xx_reset()
{
	tms36xx_reset_counters();
	tms->tune_num = 0;
}

void mm6221aa_tune_w(INT32 tune)
{
    /* which tune? */
    tune &= 3;
    if( tune == tms->tune_num )
        return;

	//bprintf(0, _T("tune: %X\n"), tune);

    tms->tune_num = tune;
    tms->tune_ofs = 0;
    tms->tune_max = 96; /* fixed for now */
}

void tms36xx_note_w(INT32 octave, INT32 note)
{
	octave &= 3;
	note &= 15;

	if (note > 12)
        return;

	//LOG(("%s octave:%X note:%X\n", tms->subtype, octave, note));

	/* play a single note from 'tune 4', a list of the 13 tones */
	tms36xx_reset_counters();
	tms->octave = octave;
    tms->tune_num = 4;
	tms->tune_ofs = note;
	tms->tune_max = note + 1;
}

void tms3617_enable(INT32 enable)
{
	INT32 bits = 0;

	/* duplicate the 6 voice enable bits */
    enable = (enable & 0x3f) | ((enable & 0x3f) << 6);
	if (enable == tms->enable)
		return;

    for (INT32 i = 0; i < 6; i++) {
		if (enable & (1 << i))
			bits += 2;	/* each voice has two instances */
	}

	/* set the enable mask and number of active voices */
	tms->enable = enable;
    tms->voices = bits;
}

void tms36xx_init(INT32 clock, INT32 subtype, double *decay, double speed)
{
	INT32 enable;

	tms = (TMS36XX *)BurnMalloc(sizeof(TMS36XX));
	memset(tms, 0, sizeof(*tms));

	tms->samplerate = nBurnSoundRate;
	tms->basefreq = clock;
	enable = 0;

	for (INT32 j = 0; j < 6; j++)
	{
		if( decay[j] > 0 )
		{
			tms->decay[j+0] = tms->decay[j+6] = VMAX / decay[j];
			enable |= 0x41 << j;
		}
	}

	tms->speed = (speed > 0) ? VMAX / speed : VMAX;
	tms3617_enable(enable);
}

void tms36xx_deinit()
{
	BurnFree(tms);
}

void tms36xx_scan(INT32, INT32*)
{
	SCAN_VAR(tms->octave);
	SCAN_VAR(tms->tune_counter);
	SCAN_VAR(tms->note_counter);
	SCAN_VAR(tms->voices);
	SCAN_VAR(tms->shift);
	SCAN_VAR(tms->vol);
	SCAN_VAR(tms->vol_counter);
	SCAN_VAR(tms->counter);
	SCAN_VAR(tms->frequency);
	SCAN_VAR(tms->output);
	SCAN_VAR(tms->enable);
	SCAN_VAR(tms->tune_num);
	SCAN_VAR(tms->tune_ofs);
	SCAN_VAR(tms->tune_max);
}

