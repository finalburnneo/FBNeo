#define TIMEKEEPER_M48T02		1
#define TIMEKEEPER_M48T35		2
#define TIMEKEEPER_M48T37		3
#define TIMEKEEPER_M48T58		4
#define TIMEKEEPER_MK48T08		5

UINT8 TimeKeeperRead(UINT32 offset);
void TimeKeeperWrite(INT32 offset, UINT8 data);
void TimeKeeperTick();
void TimeKeeperInit(INT32 type, UINT8 *data);
void TimeKeeperExit();
void TimeKeeperScan(INT32 nAction);
INT32 TimeKeeperIsEmpty();
UINT8* TimeKeeperGetRaw();

