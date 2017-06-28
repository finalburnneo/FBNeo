void K005289Reset();
void K005289Init(INT32 clock, UINT8 *prom);
void K005289Exit();

#define BURN_SND_K005289_ROUTE_1		0

void K005289SetRoute(INT32 nIndex, double nVolume, INT32 nRouteDir);

INT32 K005289Scan(INT32 nAction, INT32 *pnMin);

void K005289Update(INT16 *buffer, INT32 samples);
void K005289ControlAWrite(UINT8 data);
void K005289ControlBWrite(UINT8 data);
void K005289Ld1Write(INT32 offset);
void K005289Ld2Write(INT32 offset);
void K005289Tg1Write();
void K005289Tg2Write();
