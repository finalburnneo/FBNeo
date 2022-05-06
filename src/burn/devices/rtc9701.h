// license:BSD-3-Clause
// copyright-holders:Angelo Salese, David Haywood
/***************************************************************************

    rtc9701.h

    Serial rtc9701s.

***************************************************************************/

void rtc9701_init();
void rtc9701_exit();
void rtc9701_reset();
void rtc9701_scan(INT32 nAction, INT32 *pnMin);

void rtc9701_once_per_frame();

void rtc9701_write_bit(UINT8 data);
UINT8 rtc9701_read_bit();
void rtc9701_set_cs_line(UINT8 data);
void rtc9701_set_clock_line(UINT8 data);

