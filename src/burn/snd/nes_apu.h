// mix in nes apu &/| ext (ext = vrc, namco163, fds, etc)
enum nesapu_mixermodes { MIXER_APU = 0x01, MIXER_EXT = 0x02 };
extern INT32 nesapu_mixermode;

void nesapuInitPal(INT32 chip, INT32 clock, INT32 bAdd); // pal nes
void nesapuInit(INT32 chip, INT32 clock, INT32 bAdd); // ntsc nes
void nesapuInit(INT32 chip, INT32 clock, INT32 is_pal, UINT32 (*pSyncCallback)(INT32 samples_per_frame), INT32 nAdd);
void nesapuSetArcade(INT32 mode);
void nesapuUpdate(INT32 chip, INT16 *buffer, INT32 samples);
void nesapuSetRoute(INT32 chip, INT32 nIndex, double nVolume, INT32 nRouteDir);
void nesapuSetDMCBitDirection(INT32 reversed);
void nesapuExit();
void nesapuReset();

void nesapuScan(INT32 nAction, INT32 *pnMin);

void nesapuWrite(INT32 chip,INT32 address, UINT8 value);
UINT8 nesapuRead(INT32 chip,INT32 address, UINT8 open_bus = 0xff);
void nesapuSetMode4017(UINT8 val);
void nesapu4017hack(INT32 enable);

void nesapu_runclock(INT32 cycle);
extern INT16 (*nes_ext_sound_cb)();

#define BURN_SND_NESAPU_ROUTE_1		0
#define BURN_SND_NESAPU_ROUTE_2		1

#define nesapuSetAllRoutes(i, v, d)						\
	nesapuSetRoute(i, BURN_SND_NESAPU_ROUTE_1, v, d);		\
	nesapuSetRoute(i, BURN_SND_NESAPU_ROUTE_2, v, d);
