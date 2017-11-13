#include "m6809_intf.h"
#include "bsmt2000.h"
#include "dac.h"

extern INT32 bsmt_in_reset;

void tattass_sound_cb(UINT16 data);
void decobsmt_reset_line(INT32 state);
void decobsmt_firq_interrupt(); // 489 times per second (58hz) - 8.5 times
void decobsmt_reset();
void decobsmt_new_frame();
void decobsmt_init(UINT8 *m6809_rom, UINT8 *m6809_ram, UINT8 *tmsrom, UINT8 *tmsram, UINT8 *datarom, INT32 datarom_size);
void decobsmt_exit();
void decobsmt_scan(INT32 nAction, INT32 *pnMin);
void decobsmt_update();

void bsmt2kNewFrame();
