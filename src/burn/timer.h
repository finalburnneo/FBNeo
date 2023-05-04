// FM timers

#define TIMER_TICKS_PER_SECOND (2048000000)
#define MAKE_TIMER_TICKS(n, m) ((INT64)(n) * TIMER_TICKS_PER_SECOND / (m))
#define MAKE_CPU_CYLES(n, m) ((INT64)(n) * (m) / TIMER_TICKS_PER_SECOND)

extern "C" double BurnTimerGetTime();

// Callbacks for various sound chips
void BurnOPMTimerCallback(INT32 n, INT32 c, double period);             // ym2151:			period in  s
void BurnOPNTimerCallback(INT32 n, INT32 c, INT32 cnt, double stepTime);// ym2203, ym2612:	period = cnt * stepTime in s
void BurnOPLTimerCallback(INT32 c, double period);					    // y8950, ym3526,
																		// ym3812: 			period in  s
void BurnYMFTimerCallback(INT32 n, INT32 c, double period);				// ymf278b: 		period in us
void BurnYMF262TimerCallback(INT32 n, INT32 c, double period);          // period in  s

// Start / stop a timer
void BurnTimerSetRetrig(INT32 c, double period);						// period in  s
void BurnTimerSetOneshot(INT32 c, double period);						// period in  s

void BurnTimerSetRetrig(INT32 c, UINT64 timer_ticks);
void BurnTimerSetOneshot(INT32 c, UINT64 timer_ticks);

extern double dTime;

void BurnTimerExit();
void BurnTimerReset();
INT32 BurnTimerInit(INT32 (*pOverCallback)(INT32, INT32), double (*pTimeCallback)());
INT32 BurnTimerAttachNull(INT32 nClockspeed);
INT32 BurnTimerAttach(cpu_core_config *cpuptr, INT32 nClockspeed);
void BurnTimerScan(INT32 nAction, INT32* pnMin);
INT32 BurnTimerUpdate(INT32 nCycles);
void BurnTimerUpdateEnd();
void BurnTimerEndFrame(INT32 nCycles);

void NullNewFrame();
INT32 NullTotalCycles();
INT32 NullRun(const INT32 nCycles);

extern INT32 BurnTimerCPUClockspeed;
extern INT32(*BurnTimerCPUTotalCycles)();
