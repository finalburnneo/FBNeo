#include "burnint.h"
#include "dtimer.h"

// dtimer subsystem (runs like a cpu, see d_exidy440.cpp, d_zaccaria.cpp)

// todo:
// x: make retrig timers have an optional flag, so reset & stop will not
// affect them. (ie: timers which are part of a system, like in kaneko suprnova)


static const INT32 max_timers = 0x10;
static dtimer *timer_array[max_timers];
static INT32 timer_count;
static INT32 timer_cycles;

void timerNewFrame()
{
	timer_cycles = 0;
}

void timerInit()
{
	timer_count = 0;
}

void timerExit()
{
	timerInit();
}

void timerAdd(dtimer &timer, INT32 tparam, void (*callback)(int))
{
	if (timer_count + 1 < max_timers) {
		timer_array[timer_count++] = &timer;
		timer.init(tparam, callback);
	} else {
		bprintf(0, _T("timerAdd(): ran out of timer slots!\n"));
	}
}

void timerAdd(dtimer &timer) // already initted, just add to subsystem
{
	if (timer_count + 1 < max_timers) {
		timer_array[timer_count++] = &timer;
	} else {
		bprintf(0, _T("timerAdd(): ran out of timer slots!\n"));
	}
}

void timerAddClockSource(dtimer &timer, UINT32 speed, void (*callback)(int))
{
	if (timer_count + 1 < max_timers) {
		timer_array[timer_count++] = &timer;
		timer.config_clock_source(speed, callback);
	} else {
		bprintf(0, _T("timerAddClockSource(): ran out of timer slots!\n"));
	}
}

void timerReset()
{
	for (INT32 i = 0; i < timer_count; i++) {
		timer_array[i]->reset();
	}
}

INT32 timerRun(INT32 cyc)
{
	for (INT32 i = 0; i < timer_count; i++) {
		timer_array[i]->run(cyc);
	}

	timer_cycles += cyc;

	return cyc;
}

INT32 timerIdle(INT32 cyc)
{
	timer_cycles += cyc;

	return cyc;
}

INT32 timerTotalCycles()
{
	return timer_cycles;
}

void timerScan()
{
	for (INT32 i = 0; i < timer_count; i++) {
		timer_array[i]->scan();
	}
}

// handy converters
INT32 msec_to_cycles(INT32 mhz, double msec) {
	return ((double)((double)mhz / 1000) * msec);
}

INT32 usec_to_cycles(INT32 mhz, double usec) {
	return ((double)((double)mhz / 1000000) * usec);
}

INT32 nsec_to_cycles(INT32 mhz, double nsec) {
	return ((double)((double)mhz / 1000000000) * nsec);
}

INT32 clockscale_cycles(INT32 host_clock, INT32 cycles, INT32 clock_scaleto)
{
	return (clock_scaleto) * (cycles * (1.0 / host_clock));
}

