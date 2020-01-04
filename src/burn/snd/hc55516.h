// license:BSD-3-Clause
// copyright-holders:Aaron Giles
/*****************************************************************************

    Harris HC-55516 (and related) emulator

*****************************************************************************/


void hc55516_init(INT32 (*pCPUCyclesCB)(), INT32 nCpuMHZ);

void hc55516_exit();
void hc55516_reset();
void hc55516_scan(INT32 nAction, INT32 *);

void hc55516_clock_w(INT32 state);
void hc55516_digit_w(INT32 digit);
INT32 hc55516_clock_state_r();

void hc55516_update(INT16 *inputs, INT32 sample_len);
