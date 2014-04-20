void MSM5232Write(INT32 offset, UINT8 data);
void MSM5232Reset();
void MSM5232SetCapacitors(double cap1, double cap2, double cap3, double cap4, double cap5, double cap6, double cap7, double cap8);
void MSM5232SetGateCallback(void (*callback)(INT32));
void MSM5232Init(INT32 clock, INT32 bAdd);
void MSM5232SetClock(INT32 clock);
void MSM5232Update(INT16 *buffer, INT32 samples);
void MSM5232Exit();
INT32 MSM5232Scan(INT32 nAction, INT32 *);

void MSM5232SetRoute(double volume, INT32 route);

#define BURN_SND_MSM5232_ROUTE_0		0
#define BURN_SND_MSM5232_ROUTE_1		1
#define BURN_SND_MSM5232_ROUTE_2		2
#define BURN_SND_MSM5232_ROUTE_3		3
#define BURN_SND_MSM5232_ROUTE_4		4
#define BURN_SND_MSM5232_ROUTE_5		5
#define BURN_SND_MSM5232_ROUTE_6		6
#define BURN_SND_MSM5232_ROUTE_7		7
#define BURN_SND_MSM5232_ROUTE_SOLO8		8
#define BURN_SND_MSM5232_ROUTE_SOLO16		9
#define BURN_SND_MSM5232_ROUTE_NOISE		10
