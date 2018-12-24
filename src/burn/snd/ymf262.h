#pragma once

#ifndef __YMF262_H__
#define __YMF262_H__

/* select number of output bits: 8 or 16 */
#define OPL3_SAMPLE_BITS 16

typedef INT16 OPL3SAMPLE;

typedef void (*OPL3_TIMERHANDLER)(INT32, INT32 timer,double period);
typedef void (*OPL3_IRQHANDLER)(INT32, INT32 irq);
typedef void (*OPL3_UPDATEHANDLER)();


void *ymf262_init(int clock, int rate, void (*irq_cb)(INT32, INT32), void (*timer_cb)(INT32, INT32, double));
void ymf262_shutdown(void *chip);
void ymf262_reset_chip(void *chip);
int  ymf262_write(void *chip, int a, int v);
unsigned char ymf262_read(void *chip, int a);
int  ymf262_timer_over(void *chip, int c);
void ymf262_update_one(void *chip, INT16 **buffers, int length);
void ymf262_save_state(void *chippy, INT32 nAction);

#endif /* __YMF262_H__ */
