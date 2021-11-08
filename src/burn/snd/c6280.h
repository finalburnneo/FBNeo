
void c6280_init(INT32 clk, INT32 bAdd, INT32 lostsunh_hf_hack = 0);
void c6280_set_renderer(INT32 new_version);
void c6280_set_route(INT32 nIndex, double nVolume, INT32 nRouteDir);
void c6280_exit();

void c6280_reset();

void c6280_update(INT16 *pBuf, INT32 samples);

void c6280_write(UINT8 offset, UINT8 data);
UINT8 c6280_read();

void c6280_scan(INT32 nAction, INT32 *pnMin);

#define BURN_SND_C6280_ROUTE_1		0
#define BURN_SND_C6280_ROUTE_2		1

#define c6280_set_all_routes(v, d)						\
	c6280_set_route(BURN_SND_C6280_ROUTE_1, v, d);	\
	c6280_set_route(BURN_SND_C6280_ROUTE_2, v, d);
