extern UINT8* NamcoSoundProm;
void NamcoSoundUpdate(INT16* buffer, INT32 length);
void NamcoSoundUpdateStereo(INT16* buffer, INT32 length);
void NamcoSoundWrite(UINT32 offset, UINT8 data);
void NamcoSoundInit(INT32 clock, INT32 num_voices, INT32 bAdd);
void NacmoSoundSetRoute(INT32 nIndex, double nVolume, INT32 nRouteDir);
void NamcoSoundExit();
void NamcoSoundScan(INT32 nAction,INT32 *pnMin);
void NamcoSoundReset();

void namcos1_custom30_write(INT32 offset, INT32 data);
UINT8 namcos1_custom30_read(INT32 offset);

void namco_15xx_write(INT32 offset, UINT8 data);
void namco_15xx_sharedram_write(INT32 offset, UINT8 data);
UINT8 namco_15xx_sharedram_read(INT32 offset);
void namco_15xx_sound_enable(INT32 value);

#define BURN_SND_NAMCOSND_ROUTE_1		0
#define BURN_SND_NAMCOSND_ROUTE_2		1

#define NacmoSoundSetAllRoutes(v, d)						\
	NacmoSoundSetRoute(BURN_SND_NAMCOSND_ROUTE_1, v, d);	\
	NacmoSoundSetRoute(BURN_SND_NAMCOSND_ROUTE_2, v, d);
