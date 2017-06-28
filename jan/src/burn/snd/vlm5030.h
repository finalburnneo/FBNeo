void vlm5030Reset(INT32 nChip);
void vlm5030Init(INT32 nChip, INT32 clock, UINT32 (*pSyncCallback)(INT32 sample_rate), UINT8 *rom, INT32 rom_size, INT32 nAdd);
void vlm5030Update(INT32 nChip, INT16 *buf, INT32 samples);
void vlm5030SetRoute(INT32 chip, INT32 nIndex, double nVolume, INT32 nRouteDir);
void vlm5030Exit();

INT32 vlm5030Scan(INT32 nAction);

void vlm5030_set_rom(INT32 nChip, void *speech_rom);
INT32 vlm5030_bsy(INT32 nChip);
void vlm5030_st(INT32 nChip, INT32 pin);
void vlm5030_vcu(INT32 nChip, INT32 pin);
void vlm5030_rst(INT32 nChip, INT32 pin);
void vlm5030_data_write(INT32 nChip, UINT8 data);

#define BURN_SND_VLM5030_ROUTE_1		0
#define BURN_SND_VLM5030_ROUTE_2		1

#define vlm5030SetAllRoutes(i, v, d)						\
	vlm5030SetRoute(i, BURN_SND_VLM5030_ROUTE_1, v, d);		\
	vlm5030SetRoute(i, BURN_SND_VLM5030_ROUTE_2, v, d);
