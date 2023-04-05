// simple timer system -dink 2019-2022, v2.1.2 (2022-upgreydde ver.)

/*

better examples in d_exidy440.cpp, d_zaccaria.cpp, 6480ptm.cpp

make a simple timer:
#include "dtimer.h"

dtimer mytimer;

callback func:
static mytimer_cb(INT32 parameter)
{
	// time's up!
}

init:
timerInit();
timerAdd(mytimer, 0, mytimer_cb);  // timer_var, param, callback_func

DrvFrame():
timerNewFrame();
..
CPU_RUN(x, timer);

elsewhere:
mytimer.start(x, 0, 1, 0);
// x = cycles to start at
// 0 = parameter (use -1 to use the parameter config'd by timerAdd!)
// 1 = run now
// 0 = retrigger

mytimer.stop() - stop timer
mytimer.isrunning() - are we running?
mytimer.timeleft() - how many cycles left until callback is executed

cycle calculation helpers:
msec_to_cycles(), usec_to_cycles(), nsec_to_cycles() & clockscale_cycles()

don't forget:
timerReset(), timerExit(), timerScan()

*/

#define CLOCKSOURCE_ID	-313

struct dtimer
{
	INT32 running;
	UINT32 time_trig;
	UINT32 time_current;
	INT32 timer_param;
	INT32 timer_prescaler;
	UINT32 prescale_counter;
	INT32 retrig;
	INT32 pulse;
	void (*timer_exec)(int);

	void scan() {
		SCAN_VAR(running);
		SCAN_VAR(time_trig);
		SCAN_VAR(time_current);
		SCAN_VAR(timer_param);
		SCAN_VAR(timer_prescaler);
		SCAN_VAR(prescale_counter);
		SCAN_VAR(retrig);
		SCAN_VAR(pulse);
	}

	void start(UINT32 trig_at, INT32 tparam, INT32 run_now, INT32 retrigger) {
		running = run_now;
		if (tparam != -1) timer_param = tparam;
		time_trig = trig_at;
		time_current = 0;
		//prescale_counter = 0; <-- not here!
		retrig = retrigger;
		//extern int counter;
		//if (counter) bprintf(0, _T("timer %d START:  %d  cycles - running: %d\n"), timer_param, time_trig, running);
		//if (counter) bprintf(0, _T("timer %d   running %d - timeleft  %d  time_trig %d  time_current %d\n"), timer_param, running, time_trig - time_current, time_trig, time_current);
	}

	void init(INT32 tparam, void (*callback)(int)) {
		config(tparam, callback);
		reset();
	}

	void config(INT32 tparam, void (*callback)(int)) {
		timer_param = tparam;
		timer_exec = callback;
		timer_prescaler = 1;// * ratio_multi;
	}

	void config_clock_source(UINT32 speed, void (*callback)(int)) {
		timer_param = CLOCKSOURCE_ID;
		timer_exec = callback;
		timer_prescaler = 1;// * ratio_multi;

		pulse = 0;

		start(speed, -1, 1, 1);
	}

	void set_prescaler(INT32 prescale) {
		timer_prescaler = prescale;
	}

	void run_prescale(INT32 cyc) {
		prescale_counter += cyc;// * m_ratio;
		while (prescale_counter >= timer_prescaler) {
			prescale_counter -= timer_prescaler;

			// note: we can't optimize this, f.ex:
			//run(prescale_counter / timer_prescaler); prescale_counter %= timer_prescaler;
			// why? when the timer hits, the prescaler can & will change.
			run(1);
		}
	}

	void run(INT32 cyc) {
		if (running) {
			time_current += cyc;

			if (time_current >= time_trig) {
				//extern int counter;
				//if (counter) bprintf(0, _T("timer %d hits @ %d  sekcyc %d\n"), timer_param, time_current, SekTotalCycles());

				if (retrig == 0) {
					running = 0;
					//break;
				}

				time_current -= time_trig;

				INT32 tc_restarted = time_current;

				if (timer_exec) {
					if (timer_param == CLOCKSOURCE_ID) {
						timer_exec(pulse);
						pulse ^= 1;
					} else {
						timer_exec(timer_param); // NOTE: this cb _might_ re-start/init the timer!
					}
				}
				if (retrig == 0 && running) {
					// timer restarted from cb, restore accumulator
					time_current = tc_restarted;
				}
				//if (running == 0) break;
			}
		}
	}

	void reset() {
		stop();
		prescale_counter = 0; // this must be free-running (only reset here!)
	}
	void stop() {
		if (retrig == 0) { running = 0; }
		time_current = 0;
	}
	void stop_retrig() {
		running = 0;
		time_current = 0;
	}
	INT32 isrunning() {
		return running;
	}
	UINT32 timeleft() {
		return time_trig - time_current;
	}
};

void timerNewFrame();
void timerInit();
void timerExit();
void timerAdd(dtimer &timer, INT32 tparam, void (*callback)(int));
void timerAdd(dtimer &timer); // already initted, just add to subsystem
void timerAddClockSource(dtimer &timer, UINT32 speed, void (*callback)(int));
void timerReset();
INT32 timerRun(INT32 cyc);
INT32 timerIdle(INT32 cyc);
INT32 timerTotalCycles();
void timerScan();
INT32 msec_to_cycles(INT32 mhz, double msec);
INT32 usec_to_cycles(INT32 mhz, double usec);
INT32 nsec_to_cycles(INT32 mhz, double nsec);
INT32 clockscale_cycles(INT32 host_clock, INT32 cycles, INT32 clock_scaleto);
