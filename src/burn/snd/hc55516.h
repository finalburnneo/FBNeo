// license:BSD-3-Clause
// copyright-holders:Aaron Giles
/*****************************************************************************

    Harris HC-55516 (and related) emulator

*****************************************************************************/


void hc55516_init(INT32 (*pCPUCyclesCB)(), INT32 nCpuMHZ);
void mc3417_init(INT32 (*pCPUCyclesCB)(), INT32 nCpuMHZ);
void mc3418_init(INT32 (*pCPUCyclesCB)(), INT32 nCpuMHZ);
void hc55516_clock(INT32 clock); // have the core clock itself (instead of soft-clocking with clock_w())

void hc55516_exit();
void hc55516_reset();
void hc55516_scan(INT32 nAction, INT32 *);

void hc55516_volume(double vol);

void hc55516_clock_w(INT32 state);
void hc55516_digit_w(INT32 digit);
void hc55516_mute_w(INT32 state);
INT32 hc55516_clock_state_r();

void hc55516_update(INT16 *inputs, INT32 sample_len);
