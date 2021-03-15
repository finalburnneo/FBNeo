#ifndef DS2404_H
#define DS2404_H

void ds2404Init(UINT8 *defaultNVRAM, int ref_year, int ref_month, int ref_day);

void ds2404_1w_reset_write(UINT8);

/* 3-wire interface reset  */
void ds2404_3w_reset_write(UINT8);

UINT8 ds2404_data_read();
void ds2404_data_write(UINT8);
void ds2404_clk_write(UINT8);

extern INT32 ds2404_counter;
void ds2404_timer_update(); // call 256x per sec.

INT32 ds2404_scan(INT32 nAction, INT32 *pnMin);


#endif
