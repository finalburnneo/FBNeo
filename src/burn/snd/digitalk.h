#ifndef _DIGITALKER_H_
#define _DIGITALKER_H_

void digitalker_init(UINT8 *rom, INT32 romsize, INT32 clock, INT32 (*pCPUCyclesCB)(), INT32 nCPUMhz, INT32 AddToStream);
void digitalker_exit();
void digitalker_reset();
void digitalker_volume(double vol);
void digitalker_scan(INT32 nAction, INT32 *);
void digitalker_update(INT16 *output, INT32 samples_len);

void digitalker_cs_write(INT32 line);
void digitalker_cms_write(INT32 line);
void digitalker_wr_write(INT32 line);
INT32 digitalker_intr_read();
void digitalker_data_write(UINT8 data);

#endif
