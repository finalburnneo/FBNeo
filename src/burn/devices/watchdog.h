
// call this when resetting the watchdog timer on writes
// the watchdog will never trigger if the timer hasn't been reset once
void BurnWatchdogWrite();

// call this when resetting the watchdog time on reads
// the watchdog will never trigger if the timer hasn't been reset once
UINT8 BurnWatchdogRead();

// call this in the DrvReset
void BurnWatchdogReset();
void BurnWatchdogResetEnable(); // with auto-enable

// call this to initialize the watchdog
// pass the DrvReset routine though this (note INT32 DrvReset(INT32 clear_mem))
// note watchdog should not clear ram! It should really only reset the cpus and
// sound cores(?)
// pass the number of frames without resetting the timer before triggering
// pass -1 for frames to disable watchdog entirely
void BurnWatchdogInit(INT32 (*reset)(INT32 clear_mem), INT32 frames);

// exit
void BurnWatchdogExit();

// update the wathdog timer, call at the start of the 'Frame function.
void BurnWatchdogUpdate();

// save state support
INT32 BurnWatchdogScan(INT32 nAction);
