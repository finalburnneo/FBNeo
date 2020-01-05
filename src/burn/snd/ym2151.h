#ifndef _H_YM2151_
#define _H_YM2151_

/* 16- and 8-bit samples (signed) are supported*/
#define SAMPLE_BITS 16

#if (SAMPLE_BITS==16)
	typedef INT16 SAMP;
#endif
#if (SAMPLE_BITS==8)
	typedef signed char SAMP;
#endif

/*
** Initialize YM2151 emulator(s).
**
** 'num' is the number of virtual YM2151's to allocate
** 'clock' is the chip clock in Hz
** 'rate' is sampling rate
*/
//int YM2151Init(int num, int clock, int rate);
int YM2151Init(int num, int clock, int rate, void (*timer_cb)(INT32, double));

int ym2151_timer_over(int num, int timer); // for fm timer

/* shutdown the YM2151 emulators*/
void YM2151Shutdown(void);
void YM2151SetTimerInterleave(double d);

/* reset all chip registers for YM2151 number 'num'*/
void YM2151ResetChip(int num);

/*
** Generate samples for one of the YM2151's
**
** 'num' is the number of virtual YM2151
** '**buffers' is table of pointers to the buffers: left and right
** 'length' is the number of samples that should be generated
*/
void YM2151UpdateOne(int num, INT16 **buffers, int length);

/* write 'v' to register 'r' on YM2151 chip number 'n'*/
void YM2151WriteReg(int n, int r, int v);

/* read status register on YM2151 chip number 'n'*/
int YM2151ReadStatus(int n);

/* set interrupt handler on YM2151 chip number 'n'*/
void YM2151SetIrqHandler(int n, void (*handler)(int irq));

/* set port write handler on YM2151 chip number 'n'*/
void YM2151SetPortWriteHandler(int n, write8_handler handler);

/* FBAlpha-style savestate function for ym2151.c internal registers & operators */
void BurnYM2151Scan_int(INT32 nAction);

#endif /*_H_YM2151_*/
