/* subtypes */
#define MM6221AA    21      /* Phoenix (fixed melodies) */
#define TMS3615 	15		/* Naughty Boy, Pleiads (13 notes, one output) */
#define TMS3617 	17		/* Monster Bash (13 notes, six outputs) */

/* MM6221AA interface functions */
void mm6221aa_tune_w(int tune);

/* TMS3615/17 interface functions */
void tms36xx_note_w(int octave, int note);

/* TMS3617 interface functions */
void tms3617_enable(int enable);

void tms36xx_init(int clock, int subtype, double *decay, double speed);
void tms36xx_deinit();
void tms36xx_reset();
void tms36xx_sound_update(INT16 *buffer, INT32 length);
void tms36xx_scan();
