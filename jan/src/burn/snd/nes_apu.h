void nesapuInit(INT32 chip, INT32 clock, UINT32 (*pSyncCallback)(INT32 samples_per_frame), INT32 nAdd);
void nesapuUpdate(INT32 chip, INT16 *buffer, INT32 samples);
void nesapuSetRoute(INT32 chip, INT32 nIndex, double nVolume, INT32 nRouteDir);
void nesapuExit();
void nesapuReset();

INT32 nesapuScan(INT32 nAction);

void nesapuWrite(INT32 chip,INT32 address, UINT8 value);
UINT8 nesapuRead(INT32 chip,INT32 address);

#define BURN_SND_NESAPU_ROUTE_1		0
#define BURN_SND_NESAPU_ROUTE_2		1

#define nesapuSetAllRoutes(i, v, d)						\
	nesapuSetRoute(i, BURN_SND_NESAPU_ROUTE_1, v, d);		\
	nesapuSetRoute(i, BURN_SND_NESAPU_ROUTE_2, v, d);
