// =============================================================================
//  Epson RTC-4513 Real-Time Clock for FBNeo
// =============================================================================
// Ported from ares (ares/sfc/coprocessor/epsonrtc).
//
// The ares implementation runs a 32768*64 Hz clock thread that ticks the BCD
// registers second-by-second.  Here we drop the thread and instead catch up to
// the host wall-clock on each bus access (rtc_advance), reusing the exact same
// bit-perfect tick*() BCD logic so invalid-BCD behaviour matches hardware.
//
// The serial interface uses an immediate-ready model: ares clears "ready" for a
// few clock-thread cycles after each command byte, but with no clock thread here
// there is nothing to re-assert it, and the self-test polls "ready" before every
// access, so we keep ready asserted.  This is functionally equivalent for all
// known software and avoids carrying a wait counter in save states.
// =============================================================================

#include "epsonrtc.h"
#include <time.h>

//serial interface state
static UINT8 chipselect;	//n2
enum { STATE_Mode = 0, STATE_Seek = 1, STATE_Read = 2, STATE_Write = 3 };
static UINT8 state;
static UINT8 mdr;			//n4
static UINT8 offset;		//n4
static UINT8 ready;			//n1
static UINT8 holdtick;		//n1

//clock registers
static UINT8 secondlo, secondhi, batteryfailure;	//4,3,1
static UINT8 minutelo, minutehi, resync;			//4,3,1
static UINT8 hourlo, hourhi, meridian;				//4,2,1
static UINT8 daylo, dayhi, dayram;					//4,2,1
static UINT8 monthlo, monthhi, monthram;			//4,1,2
static UINT8 yearlo, yearhi;						//4,4
static UINT8 weekday;								//3

static UINT8 hold, calendar, irqflag, roundseconds;	//1,1,1,1
static UINT8 irqmask, irqduty, irqperiod;			//1,1,2
static UINT8 pause, stop, atime, test;				//1,1,1,1

static time_t rtc_last;		//host time of last real-time catch-up

//forward
static void tickSecond();
static void tickMinute();
static void tickHour();
static void tickDay();
static void tickMonth();
static void tickYear();

//=====================
// bit-perfect BCD tick logic (verbatim from ares time.cpp)
//=====================

static void tickSecond()
{
	if (secondlo <= 8 || secondlo == 12) {
		secondlo++;
	} else {
		secondlo = 0;
		if (secondhi < 5) {
			secondhi++;
		} else {
			secondhi = 0;
			tickMinute();
		}
	}
}

static void tickMinute()
{
	if (minutelo <= 8 || minutelo == 12) {
		minutelo++;
	} else {
		minutelo = 0;
		if (minutehi < 5) {
			minutehi++;
		} else {
			minutehi = 0;
			tickHour();
		}
	}
}

static void tickHour()
{
	if (atime) {
		if (hourhi < 2) {
			if (hourlo <= 8 || hourlo == 12) {
				hourlo++;
			} else {
				hourlo = !(hourlo & 1);
				hourhi++;
			}
		} else {
			if (hourlo != 3 && !(hourlo & 4)) {
				if (hourlo <= 8 || hourlo >= 12) {
					hourlo++;
				} else {
					hourlo = !(hourlo & 1);
					hourhi++;
				}
			} else {
				hourlo = !(hourlo & 1);
				hourhi = 0;
				tickDay();
			}
		}
	} else {
		if (hourhi == 0) {
			if (hourlo <= 8 || hourlo == 12) {
				hourlo++;
			} else {
				hourlo = !(hourlo & 1);
				hourhi ^= 1;
			}
		} else {
			if (hourlo & 1) meridian ^= 1;
			if (hourlo < 2 || hourlo == 4 || hourlo == 5 || hourlo == 8 || hourlo == 12) {
				hourlo++;
			} else {
				hourlo = !(hourlo & 1);
				hourhi ^= 1;
			}
			if (meridian == 0 && !(hourlo & 1)) tickDay();
		}
	}
}

static void tickDay()
{
	if (calendar == 0) return;
	weekday = ((weekday + 1) + (weekday == 6)) & 7;  //n3 wrap: 6 -> 0 (skip 7)

	//January - December = 0x01 - 0x09; 0x10 - 0x12
	static const UINT32 daysinmonth[32] = {
		30, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31, 30, 31, 30,
		31, 30, 31, 30, 31, 30, 31, 30, 31, 30, 31, 30, 31, 30, 31, 30,
	};

	UINT32 days = daysinmonth[monthhi << 4 | monthlo];
	if (days == 28) {
		//add one day for leap years
		if ((yearhi & 1) == 0 && ((yearlo - 0) & 3) == 0) days++;
		if ((yearhi & 1) == 1 && ((yearlo - 2) & 3) == 0) days++;
	}

	if (days == 28 && (dayhi == 3 || (dayhi == 2 && daylo >= 8))) {
		daylo = 1; dayhi = 0; tickMonth(); return;
	}

	if (days == 29 && (dayhi == 3 || (dayhi == 2 && (daylo > 8 && daylo != 12)))) {
		daylo = 1; dayhi = 0; tickMonth(); return;
	}

	if (days == 30 && (dayhi == 3 || (dayhi == 2 && (daylo == 10 || daylo == 14)))) {
		daylo = 1; dayhi = 0; tickMonth(); return;
	}

	if (days == 31 && (dayhi == 3 && (daylo & 3))) {
		daylo = 1; dayhi = 0; tickMonth(); return;
	}

	if (daylo <= 8 || daylo == 12) {
		daylo++;
	} else {
		daylo = !(daylo & 1);
		dayhi++;
	}
}

static void tickMonth()
{
	if (monthhi == 0 || !(monthlo & 2)) {
		if (monthlo <= 8 || monthlo == 12) {
			monthlo++;
		} else {
			monthlo = !(monthlo & 1);
			monthhi ^= 1;
		}
	} else {
		monthlo = !(monthlo & 1);
		monthhi = 0;
		tickYear();
	}
}

static void tickYear()
{
	if (yearlo <= 8 || yearlo == 12) {
		yearlo++;
	} else {
		yearlo = !(yearlo & 1);
		if (yearhi <= 8 || yearhi == 12) {
			yearhi++;
		} else {
			yearhi = !(yearhi & 1);
		}
	}
}

//=====================
// register access (verbatim from ares memory.cpp)
//=====================

static void rtcReset()
{
	state  = STATE_Mode;
	offset = 0;
	resync = 0;
	pause  = 0;
	test   = 0;
}

static UINT8 rtcRead(UINT8 address)
{
	switch (address & 15) { default:
		case  0: return secondlo;
		case  1: return (secondhi | batteryfailure << 3) & 15;
		case  2: return minutelo;
		case  3: return (minutehi | resync   << 3) & 15;
		case  4: return hourlo;
		case  5: return (hourhi   | meridian << 2 | resync << 3) & 15;
		case  6: return daylo;
		case  7: return (dayhi    | dayram   << 2 | resync << 3) & 15;
		case  8: return monthlo;
		case  9: return (monthhi  | monthram << 1 | resync << 3) & 15;
		case 10: return yearlo;
		case 11: return yearhi;
		case 12: return (weekday  | resync   << 3) & 15;
		case 13: {
			UINT8 readflag = irqflag & !irqmask;
			irqflag = 0;
			return (hold | calendar << 1 | readflag << 2 | roundseconds << 3) & 15;
		}
		case 14: return (irqmask | irqduty << 1 | irqperiod << 2) & 15;
		case 15: return (pause   | stop    << 1 | atime     << 2 | test << 3) & 15;
	}
}

static void rtcWrite(UINT8 address, UINT8 data)
{
	data &= 15;
	switch (address & 15) {
		case 0:
			secondlo = data;
			break;
		case 1:
			secondhi       = data  & 7;
			batteryfailure = data >> 3;
			break;
		case 2:
			minutelo = data;
			break;
		case 3:
			minutehi = data & 7;
			break;
		case 4:
			hourlo = data;
			break;
		case 5:
			hourhi   =  data  & 3;
			meridian = (data >> 2) & 1;
			if (atime == 1) meridian = 0;
			if (atime == 0) hourhi  &= 1;
			break;
		case 6:
			daylo = data;
			break;
		case 7:
			dayhi  = data & 3;
			dayram = (data >> 2) & 1;
			break;
		case 8:
			monthlo = data;
			break;
		case 9:
			monthhi  = data & 1;
			monthram = (data >> 1) & 3;
			break;
		case 10:
			yearlo = data;
			break;
		case 11:
			yearhi = data;
			break;
		case 12:
			weekday = data & 7;
			break;
		case 13: {
			UINT8 held = hold;
			hold         =  data  & 1;
			calendar     = (data >> 1) & 1;
			roundseconds = (data >> 3) & 1;
			if (held == 1 && hold == 0 && holdtick == 1) {
				//if a second has passed during hold, increment one second upon resuming
				holdtick = 0;
				tickSecond();
			}
		} break;
		case 14:
			irqmask   =  data  & 1;
			irqduty   = (data >> 1) & 1;
			irqperiod = (data >> 2) & 3;
			break;
		case 15:
			pause =  data  & 1;
			stop  = (data >> 1) & 1;
			atime = (data >> 2) & 1;
			test  = (data >> 3) & 1;
			if (atime == 1) meridian = 0;
			if (atime == 0) hourhi &= 1;
			if (pause) {
				secondlo = 0;
				secondhi = 0;
			}
			break;
	}
}

//=====================
// host wall-clock seeding + real-time catch-up
//=====================

static void seedFromHost()
{
	time_t systime = time(NULL);
	struct tm* ti = localtime(&systime);
	if (!ti) return;

	UINT32 second = ti->tm_sec; if (second > 59) second = 59;
	secondlo = second % 10;
	secondhi = second / 10;

	UINT32 minute = ti->tm_min;
	minutelo = minute % 10;
	minutehi = minute / 10;

	UINT32 hour = ti->tm_hour;
	if (atime) {
		hourlo = hour % 10;
		hourhi = hour / 10;
	} else {
		meridian = hour >= 12;
		hour %= 12;
		if (hour == 0) hour = 12;
		hourlo = hour % 10;
		hourhi = hour / 10;
	}

	UINT32 day = ti->tm_mday;
	daylo = day % 10;
	dayhi = day / 10;

	UINT32 month = 1 + ti->tm_mon;
	monthlo = month % 10;
	monthhi = month / 10;

	UINT32 year = ti->tm_year % 100;
	yearlo = year % 10;
	yearhi = year / 10;

	weekday = ti->tm_wday & 7;

	resync   = 1;						//alert program that time has changed
	rtc_last = systime;
}

//advance the clock by the real time elapsed since the last access
static void rtc_advance()
{
	if (stop || pause || hold) return;	//frozen: keep rtc_last, accumulate elapsed
	time_t now = time(NULL);
	INT64 diff = (INT64)now - (INT64)rtc_last;
	if (diff <= 0) { rtc_last = now; return; }
	rtc_last   = now;

	resync = 1;
	while (diff >= 86400) { tickDay();    diff -= 86400; }
	while (diff >= 3600)  { tickHour();   diff -= 3600;  }
	while (diff >= 60)    { tickMinute(); diff -= 60;    }
	while (diff-- > 0)    { tickSecond();               }
}

//=====================
// bus interface (verbatim protocol from ares epsonrtc.cpp, immediate-ready model)
//=====================

UINT8 snes_epsonrtc_read(UINT8 address)
{
	rtc_advance();
	address &= 3;

	if (address == 0) {
		return chipselect;
	}

	if (address == 1) {
		if (chipselect != 1)      return 0;
		if (ready == 0)           return 0;
		if (state == STATE_Write) return mdr;
		if (state != STATE_Read)  return 0;

		return rtcRead(offset++);
	}

	if (address == 2) {
		return ready << 7;
	}

	return 0;
}

void snes_epsonrtc_write(UINT8 address, UINT8 data)
{
	rtc_advance();
	address &= 3;
	data &= 15;

	if (address == 0) {
		chipselect = data & 3;
		if (chipselect != 1) rtcReset();
		ready = 1;
	}

	if (address == 1) {
		if (chipselect != 1) return;
		if (ready == 0)      return;

		if (state == STATE_Mode) {
			if (data != 0x03 && data != 0x0c) return;
			state = STATE_Seek;
			ready = 1;	//immediate: no clock thread, ready stays asserted (see header)
			mdr   = data;
		} else if (state == STATE_Seek) {
			if (mdr == 0x03) state = STATE_Write;
			if (mdr == 0x0c) state = STATE_Read;
			offset = data;
			ready  = 1;	//immediate
			mdr    = data;
		} else if (state == STATE_Write) {
			rtcWrite(offset++, data);
			ready = 1;	//immediate
			mdr   = data;
		}
	}
}

//=====================
// init / reset / state
//=====================

// Cold power-on (cartridge inserted / hard boot with a fresh RTC).  On real
// hardware the RTC-4513 is kept alive by its own coin cell, so the calendar
// registers survive console reset and power-off; only inserting the cartridge
// (or a dead battery) starts them from scratch.  We model "battery good" by
// seeding the calendar from the host wall-clock, so batteryfailure stays clear.
void snes_epsonrtc_power()
{
	//power-on defaults (ares initialize())
	batteryfailure = 0;  //seeded from host clock below -> battery is valid
	minutelo = minutehi = 0; resync = 0;
	hourlo   = hourhi   = meridian  = 0;
	daylo    = dayhi    = dayram    = 0;
	monthlo  = monthhi  = monthram  = 0;
	yearlo   = yearhi   = 0;
	weekday  = 0;
	hold     = calendar = irqflag    = roundseconds = 0;
	irqmask  = irqduty  = irqperiod  = 0;
	pause    = stop     = atime      = test         = 0;
	secondlo = secondhi = 0;

	chipselect = 0;
	state      = STATE_Mode;
	offset     = 0;
	ready      = 0;
	holdtick   = 0;

	seedFromHost();  //seed calendar from host wall clock
}

// Warm reset (console reset button / soft reset).  The coin-cell-backed clock
// registers are untouched; only the serial bus interface returns to idle.
void snes_epsonrtc_reset()
{
	chipselect = 0;
	state      = STATE_Mode;
	offset     = 0;
	ready      = 0;
	holdtick   = 0;
}

void snes_epsonrtc_handleState(StateHandler* sh)
{
	sh_handleBytes(sh,
		&chipselect, &state, &mdr, &offset, &ready, &holdtick,
		&secondlo, &secondhi, &batteryfailure,
		&minutelo, &minutehi, &resync,
		&hourlo, &hourhi, &meridian,
		&daylo, &dayhi, &dayram,
		&monthlo, &monthhi, &monthram,
		&yearlo, &yearhi, &weekday,
		&hold, &calendar, &irqflag, &roundseconds,
		&irqmask, &irqduty, &irqperiod,
		&pause, &stop, &atime, &test, NULL);

	//do not persist rtc_last across states; re-anchor to current host time on load
	if (!sh->saving) rtc_last = time(NULL);
}
