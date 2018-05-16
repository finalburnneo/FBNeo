/**********************************************************************

    SP0256 Narrator Speech Processor emulation

    Copyright MESS Team.
    Visit http://mamedev.org for licensing and usage restrictions.

**********************************************************************
                            _____   _____
                   Vss   1 |*    \_/     | 28  OSC 2
                _RESET   2 |             | 27  OSC 1
           ROM DISABLE   3 |             | 26  ROM CLOCK
                    C1   4 |             | 25  _SBY RESET
                    C2   5 |             | 24  DIGITAL OUT
                    C3   6 |             | 23  Vdi
                   Vdd   7 |    SP0256   | 22  TEST
                   SBY   8 |             | 21  SER IN
                  _LRQ   9 |             | 20  _ALD
                    A8  10 |             | 19  SE
                    A7  11 |             | 18  A1
               SER OUT  12 |             | 17  A2
                    A6  13 |             | 16  A3
                    A5  14 |_____________| 15  A4

**********************************************************************/

/*
   GI SP0256 Narrator Speech Processor

   By Joe Zbiciak. Ported to MESS by tim lindner.

 Copyright Joseph Zbiciak, all rights reserved.
 Copyright tim lindner, all rights reserved.

 - This source code is released as freeware for non-commercial purposes.
 - You are free to use and redistribute this code in modified or
   unmodified form, provided you list us in the credits.
 - If you modify this source code, you must add a notice to each
   modified source file that it has been changed.  If you're a nice
   person, you will clearly mark each change too.  :)
 - If you wish to use this for commercial purposes, please contact us at
   intvnut@gmail.com (Joe Zbiciak), tlindner@macmess.org (tim lindner)
 - This entire notice must remain in the source code.

*/

void sp0256_reset();
void sp0256_init(UINT8 *rom, INT32 clock);
void sp0256_set_drq_cb(void (*cb)(UINT8));
void sp0256_set_sby_cb(void (*cb)(UINT8));
void sp0256_set_clock(INT32 clock);
void sp0256_exit();
void sp0256_scan(INT32 nAction, INT32* pnMin);

void sp0256_ald_write(UINT8 data);
UINT8 sp0256_lrq_read();
UINT8 sp0256_sby_read();
UINT16 sp0256_spb640_read();
void sp0256_spb640_write(UINT16 offset, UINT16 data);
void sp0256_set_clock(INT32 clock);

void sp0256_update(INT16 *sndbuff, INT32 samples);
