#pragma once

#ifndef GBA_RTC_H
#define GBA_RTC_H

#include <time.h>

#define GBA_RTC_DAYS_1970_TO_2000 10957
#define GBA_RTC_STATUS_MASK 0x6a

#if defined(GBA_STANDALONE)
extern INT64 gba_rtc_test_now;
#define GBA_RTC_NOW_SECONDS() (gba_rtc_test_now)
#else
#define GBA_RTC_NOW_SECONDS() ((INT64)time(NULL))
#endif

enum gba_rtc_phase {
	GBA_RTC_IDLE = 0,
	GBA_RTC_COMMAND,
	GBA_RTC_RECEIVE,
	GBA_RTC_SEND,
	GBA_RTC_COMPLETE,
};

enum gba_rtc_register {
	GBA_RTC_RESET = 0,
	GBA_RTC_UNUSED = 1,
	GBA_RTC_DATE_TIME = 2,
	GBA_RTC_FORCE_IRQ = 3,
	GBA_RTC_STATUS = 4,
	GBA_RTC_UNUSED2 = 5,
	GBA_RTC_TIME = 6,
	GBA_RTC_UNUSED3 = 7,
};

typedef struct {
	UINT16 year;
	UINT8 month;
	UINT8 day;
	UINT8 weekday;
	UINT8 hour;
	UINT8 minute;
	UINT8 second;
} gba_rtc_civil_t;

typedef struct {
	INT64 rtc_seconds;
	INT64 host_seconds;
	UINT8 status;
	UINT8 phase;
	UINT8 command;
	UINT8 command_register;
	UINT8 command_read;
	UINT8 bit_index;
	UINT8 byte_index;
	UINT8 byte_count;
	UINT8 buffer[7];
	UINT8 last_pins;
	UINT8 sio_out;
} gba_rtc_t;

// Howard Hinnant's days_from_civil / civil_from_days (public domain). Proleptic
// Gregorian, handles any date including 2100 (not a leap year).
static FORCE_INLINE INT32 gba_rtc_days_from_civil(INT32 y, UINT32 m, UINT32 d)
{
	y -= (m <= 2);
	INT32 era = (y >= 0 ? y : y - 399) / 400;
	UINT32 yoe = (UINT32)(y - era * 400);
	UINT32 doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
	UINT32 doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
	return era * 146097 + (INT32)doe - 719468;
}

static FORCE_INLINE void gba_rtc_civil_from_days(INT32 z, INT32 *y, UINT32 *m, UINT32 *d)
{
	z += 719468;
	INT32 era = (z >= 0 ? z : z - 146096) / 146097;
	UINT32 doe = (UINT32)(z - era * 146097);
	UINT32 yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	UINT32 y0 = yoe + (UINT32)era * 400;
	UINT32 doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
	UINT32 mp = (5 * doy + 2) / 153;
	*d = doy - (153 * mp + 2) / 5 + 1;
	*m = mp + (mp < 10 ? 3 : -9);
	*y = (INT32)y0 + (*m <= 2);
}

static FORCE_INLINE INT64 gba_rtc_civil_to_seconds(const gba_rtc_civil_t *c)
{
	INT32 days = gba_rtc_days_from_civil((INT32)c->year, c->month, c->day) - GBA_RTC_DAYS_1970_TO_2000;
	return (INT64)days * 86400 + c->hour * 3600 + c->minute * 60 + c->second;
}

static FORCE_INLINE void gba_rtc_seconds_to_civil(INT64 sec, gba_rtc_civil_t *c)
{
	INT64 day = sec / 86400;
	INT32 into_day = (INT32)(sec - day * 86400);
	if (into_day < 0) { into_day += 86400; day -= 1; }
	INT32 y;
	UINT32 m, d;
	gba_rtc_civil_from_days((INT32)day + GBA_RTC_DAYS_1970_TO_2000, &y, &m, &d);
	if (y < 2000) y = 2000 + ((y - 2000) % 100 + 100) % 100;
	else if (y > 2099) y = 2000 + ((y - 2000) % 100 + 100) % 100;
	c->year = (UINT16)y;
	c->month = (UINT8)m;
	c->day = (UINT8)d;
	c->weekday = (UINT8)(((day % 7) + 7 + 6) % 7);  // 2000-01-01 = Saturday (6, 0=Sun)
	c->hour = (UINT8)(into_day / 3600);
	c->minute = (UINT8)((into_day % 3600) / 60);
	c->second = (UINT8)(into_day % 60);
}

static FORCE_INLINE INT64 gba_rtc_current_seconds(const gba_rtc_t *rtc)
{
	return rtc->rtc_seconds + (GBA_RTC_NOW_SECONDS() - rtc->host_seconds);
}

static FORCE_INLINE UINT8 gba_rtc_bcd_encode(UINT8 value)
{
	return (value % 10) | ((value / 10) << 4);
}

static FORCE_INLINE INT32 gba_rtc_bcd_decode(UINT8 value, UINT8 maximum, UINT8 *result)
{
	UINT8 low = value & 0x0f;
	UINT8 high = (value >> 4) & 0x0f;
	if (low > 9 || high > 9) return 1;
	UINT8 decoded = high * 10 + low;
	if (decoded > maximum) return 1;
	*result = decoded;
	return 0;
}

static FORCE_INLINE UINT8 gba_rtc_hour_encode(UINT8 hour_24, UINT8 status)
{
	if (status & 0x40) return gba_rtc_bcd_encode(hour_24);
	return gba_rtc_bcd_encode(hour_24 % 12) | (hour_24 >= 12 ? 0x80 : 0);
}

static FORCE_INLINE INT32 gba_rtc_hour_decode(UINT8 status, UINT8 value, UINT8 *hour)
{
	if (status & 0x40) {
		if (value & 0xc0) return 1;
		return gba_rtc_bcd_decode(value, 23, hour);
	}
	UINT8 decoded;
	if (gba_rtc_bcd_decode(value & 0x3f, 11, &decoded)) return 1;
	*hour = decoded + ((value & 0x80) ? 12 : 0);
	return 0;
}

static FORCE_INLINE void gba_rtc_transport_reset(gba_rtc_t *rtc)
{
	rtc->phase = GBA_RTC_IDLE;
	rtc->command = 0;
	rtc->command_register = 0;
	rtc->command_read = 0;
	rtc->bit_index = 0;
	rtc->byte_index = 0;
	rtc->byte_count = 0;
	memset(rtc->buffer, 0, sizeof(rtc->buffer));
	rtc->sio_out = 1;
}

static FORCE_INLINE void gba_rtc_cold_init(gba_rtc_t *rtc, const gba_rtc_civil_t *seed)
{
	memset(rtc, 0, sizeof(*rtc));
	gba_rtc_civil_t fallback = { 2000, 1, 1, 0, 0, 0, 0 };
	const gba_rtc_civil_t *s = (seed && seed->year >= 2000 && seed->year <= 2099 &&
		seed->month >= 1 && seed->month <= 12 && seed->day >= 1 &&
		seed->day <= 31 && seed->hour <= 23 && seed->minute <= 59 && seed->second <= 59) ? seed : &fallback;
	rtc->rtc_seconds = gba_rtc_civil_to_seconds(s);
	rtc->host_seconds = GBA_RTC_NOW_SECONDS();
	rtc->status = 0x40;
	gba_rtc_transport_reset(rtc);
}

static FORCE_INLINE void gba_rtc_reanchor(gba_rtc_t *rtc, INT64 rtc_seconds)
{
	rtc->rtc_seconds = rtc_seconds;
	rtc->host_seconds = GBA_RTC_NOW_SECONDS();
}

static FORCE_INLINE void gba_rtc_latch_read(gba_rtc_t *rtc)
{
	memset(rtc->buffer, 0xff, sizeof(rtc->buffer));
	switch (rtc->command_register) {
		case GBA_RTC_STATUS:
			rtc->byte_count = 1;
			rtc->buffer[0] = rtc->status;
			break;
		case GBA_RTC_DATE_TIME: {
			gba_rtc_civil_t c;
			gba_rtc_seconds_to_civil(gba_rtc_current_seconds(rtc), &c);
			rtc->byte_count = 7;
			rtc->buffer[0] = gba_rtc_bcd_encode((UINT8)(c.year % 100));
			rtc->buffer[1] = gba_rtc_bcd_encode(c.month);
			rtc->buffer[2] = gba_rtc_bcd_encode(c.day);
			rtc->buffer[3] = gba_rtc_bcd_encode(c.weekday);
			rtc->buffer[4] = gba_rtc_hour_encode(c.hour, rtc->status);
			rtc->buffer[5] = gba_rtc_bcd_encode(c.minute);
			rtc->buffer[6] = gba_rtc_bcd_encode(c.second);
			break;
		}
		case GBA_RTC_TIME: {
			gba_rtc_civil_t c;
			gba_rtc_seconds_to_civil(gba_rtc_current_seconds(rtc), &c);
			rtc->byte_count = 3;
			rtc->buffer[0] = gba_rtc_hour_encode(c.hour, rtc->status);
			rtc->buffer[1] = gba_rtc_bcd_encode(c.minute);
			rtc->buffer[2] = gba_rtc_bcd_encode(c.second);
			break;
		}
		default:
			rtc->byte_count = 0;
			break;
	}
}

static FORCE_INLINE INT32 gba_rtc_commit_datetime(gba_rtc_t *rtc)
{
	gba_rtc_civil_t c;
	gba_rtc_seconds_to_civil(gba_rtc_current_seconds(rtc), &c);
	UINT8 year;
	if (gba_rtc_bcd_decode(rtc->buffer[0], 99, &year)) return 1;
	c.year = 2000 + year;
	if (gba_rtc_bcd_decode(rtc->buffer[1], 12, &c.month) || c.month == 0) return 1;
	if (gba_rtc_bcd_decode(rtc->buffer[2], 31, &c.day) || c.day == 0) return 1;
	if (gba_rtc_bcd_decode(rtc->buffer[3], 6, &c.weekday)) return 1;
	if (gba_rtc_hour_decode(rtc->status, rtc->buffer[4], &c.hour)) return 1;
	if (gba_rtc_bcd_decode(rtc->buffer[5], 59, &c.minute)) return 1;
	if (gba_rtc_bcd_decode(rtc->buffer[6], 59, &c.second)) return 1;
	if (c.year < 2000 || c.year > 2099 || c.month < 1 || c.month > 12 ||
		c.day < 1 || c.day > 31 || c.hour > 23) return 1;
	gba_rtc_reanchor(rtc, gba_rtc_civil_to_seconds(&c));
	return 0;
}

static FORCE_INLINE INT32 gba_rtc_commit_time(gba_rtc_t *rtc)
{
	gba_rtc_civil_t c;
	gba_rtc_seconds_to_civil(gba_rtc_current_seconds(rtc), &c);
	if (gba_rtc_hour_decode(rtc->status, rtc->buffer[0], &c.hour)) return 1;
	if (gba_rtc_bcd_decode(rtc->buffer[1], 59, &c.minute)) return 1;
	if (gba_rtc_bcd_decode(rtc->buffer[2], 59, &c.second)) return 1;
	gba_rtc_reanchor(rtc, gba_rtc_civil_to_seconds(&c));
	return 0;
}

static FORCE_INLINE void gba_rtc_commit_write(gba_rtc_t *rtc)
{
	switch (rtc->command_register) {
		case GBA_RTC_STATUS:
			rtc->status = rtc->buffer[0] & GBA_RTC_STATUS_MASK;
			break;
		case GBA_RTC_DATE_TIME:
			gba_rtc_commit_datetime(rtc);
			break;
		case GBA_RTC_TIME:
			gba_rtc_commit_time(rtc);
			break;
	}
}

static FORCE_INLINE UINT8 gba_rtc_reverse8(UINT8 value)
{
	value = (value >> 4) | (value << 4);
	value = ((value & 0x33) << 2) | ((value & 0xcc) >> 2);
	return ((value & 0x55) << 1) | ((value & 0xaa) >> 1);
}

static FORCE_INLINE void gba_rtc_decode_command(gba_rtc_t *rtc)
{
	UINT8 command = rtc->command;
	if ((command & 0x0f) != 0x06 && (command & 0xf0) == 0x60) command = gba_rtc_reverse8(command);
	if ((command & 0x0f) != 0x06) {
		rtc->phase = GBA_RTC_COMPLETE;
		return;
	}

	rtc->command = command;
	rtc->command_register = (command >> 4) & 7;
	rtc->command_read = (command >> 7) & 1;
	rtc->bit_index = 0;
	rtc->byte_index = 0;
	memset(rtc->buffer, 0, sizeof(rtc->buffer));

	if (rtc->command_register == GBA_RTC_RESET) {
		if (!rtc->command_read) rtc->status = 0;
		rtc->phase = GBA_RTC_COMPLETE;
		return;
	}
	if (rtc->command_register == GBA_RTC_FORCE_IRQ || rtc->command_register == GBA_RTC_UNUSED ||
		rtc->command_register == GBA_RTC_UNUSED2 || rtc->command_register == GBA_RTC_UNUSED3) {
		rtc->phase = GBA_RTC_COMPLETE;
		return;
	}

	if (rtc->command_read) {
		gba_rtc_latch_read(rtc);
		rtc->phase = rtc->byte_count ? GBA_RTC_SEND : GBA_RTC_COMPLETE;
	} else {
		rtc->byte_count = rtc->command_register == GBA_RTC_STATUS ? 1 :
			rtc->command_register == GBA_RTC_DATE_TIME ? 7 :
			rtc->command_register == GBA_RTC_TIME ? 3 : 0;
		rtc->phase = rtc->byte_count ? GBA_RTC_RECEIVE : GBA_RTC_COMPLETE;
	}
}

static FORCE_INLINE void gba_rtc_sample_input(gba_rtc_t *rtc, UINT8 sio)
{
	if (rtc->phase == GBA_RTC_COMMAND) {
		rtc->command |= (sio & 1) << rtc->bit_index;
		if (++rtc->bit_index == 8) gba_rtc_decode_command(rtc);
		return;
	}
	if (rtc->phase != GBA_RTC_RECEIVE) return;

	rtc->buffer[rtc->byte_index] |= (sio & 1) << rtc->bit_index;
	if (++rtc->bit_index < 8) return;
	rtc->bit_index = 0;
	if (++rtc->byte_index < rtc->byte_count) return;
	gba_rtc_commit_write(rtc);
	rtc->phase = GBA_RTC_COMPLETE;
}

static FORCE_INLINE void gba_rtc_shift_output(gba_rtc_t *rtc)
{
	if (rtc->phase != GBA_RTC_SEND) {
		rtc->sio_out = 1;
		return;
	}
	rtc->sio_out = (rtc->buffer[rtc->byte_index] >> rtc->bit_index) & 1;
	if (++rtc->bit_index < 8) return;
	rtc->bit_index = 0;
	if (++rtc->byte_index < rtc->byte_count) return;
	rtc->phase = GBA_RTC_COMPLETE;
}

static FORCE_INLINE UINT8 gba_rtc_update_pins(gba_rtc_t *rtc, UINT8 pins)
{
	UINT8 old = rtc->last_pins;
	UINT8 cs = (pins >> 2) & 1;
	UINT8 old_cs = (old >> 2) & 1;
	UINT8 sck = pins & 1;
	UINT8 old_sck = old & 1;

	if (!cs) {
		gba_rtc_transport_reset(rtc);
		rtc->last_pins = pins;
		return rtc->sio_out;
	}
	if (!old_cs) {
		gba_rtc_transport_reset(rtc);
		rtc->phase = GBA_RTC_COMMAND;
		rtc->last_pins = pins;
		return rtc->sio_out;
	}
	if (!old_sck && sck) gba_rtc_sample_input(rtc, (old >> 1) & 1);
	if (old_sck && !sck) gba_rtc_shift_output(rtc);
	rtc->last_pins = pins;
	return rtc->sio_out;
}

#endif
