// license:BSD-3-Clause
// copyright-holders:Ed Bernard, Jonathan Gevaryahu, hap
// thanks-to:Kevin Horton
/*
    SSi TSI S14001A speech IC emulator
*/

void s14001a_render(INT16 *buffer, INT32 length);
void s14001a_init(UINT8 *rom, INT32 (*pCPUCyclesCB)(), INT32 nCpuMHZ);
void s14001a_exit();
void s14001a_reset();
void s14001a_scan(INT32 nAction, INT32 *pnMin);

INT32 s14001a_romen_read();
INT32 s14001a_busy_read();
void s14001a_data_write(UINT8 data);
void s14001a_start_write(INT32 state);
void s14001a_set_clock(INT32 clock);
void s14001a_set_volume(double volume);
void s14001a_force_update();
