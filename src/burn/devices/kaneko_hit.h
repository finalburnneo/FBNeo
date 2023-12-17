// note - requires watchdog to be initialized!

void kaneko_hit_calc_reset();
void kaneko_hit_calc_init(INT32 nMapNumber, UINT32 address);
INT32 kaneko_hit_calc_scan(INT32 nAction); // scans random & watchdog
