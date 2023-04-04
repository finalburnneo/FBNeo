#define K054539_RESET_FLAGS     0
#define K054539_REVERSE_STEREO  1
#define K054539_DISABLE_REVERB  2
#define K054539_UPDATE_AT_KEYON 4

void K054539SetFlags(INT32 chip, INT32 flags);
void K054539_set_gain(INT32 chip, INT32 channel, double gain);

void K054539Update(INT32 chip, INT16 *pBuf, INT32 length);

void K054539Write(INT32 chip, INT32 offset, UINT8 data);
UINT8 K054539Read(INT32 chip, INT32 offset);

void K054539Reset(INT32 chip);
void K054539Init(INT32 chip, INT32 clock, UINT8 *rom, INT32 nLen);
void K054539SetRoute(INT32 chip, INT32 nIndex, double nVolume, INT32 nRouteDir);
void K054539SetApanCallback(INT32 chip, void (*ApanCB)(double, double));
void K054539Exit();

void K054539SetIRQCallback(INT32 chip, void (*irq_cb)(int));

void K054539Scan(INT32 nAction, INT32 *pnMin);

#define BURN_SND_K054539_ROUTE_1		0
#define BURN_SND_K054539_ROUTE_2		1

#define K054539PCMSetAllRoutes(i, v, d)						\
	K054539SetRoute(i, BURN_SND_K054539_ROUTE_1, v, d);	\
	K054539SetRoute(i, BURN_SND_K054539_ROUTE_2, v, d);
