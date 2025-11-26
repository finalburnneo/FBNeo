#define SAMPLE_IGNORE		(1<<0) // don't ever play this sample
#define SAMPLE_AUTOLOOP		(1<<1) // start the looping on start
#define SAMPLE_NOLOOP		(1<<2) // don't allow this to loop

// Change this to 0 to 1 if using samples in a low-ram environment.
// May cause momentary stutter while sample loads.
#define SAMPLE_NOSTORE		(0<<3) // only keep in memory while playing
#define SAMPLE_NOSTOREF		(1<<3) // only keep in memory while playing (forced)
#define SAMPLE_NODUMP       (1<<4) // dump not available

#define SAMPLE_PLAYING		(1<<0) // playing
#define SAMPLE_PAUSED		(1<<1) // paused
#define SAMPLE_STOPPED		(  0 ) // stopped
#define SAMPLE_INVALID		( -1 ) // invalid

void BurnSamplePlay(INT32 sample);
void BurnSamplePause(INT32 sample);
void BurnSampleResume(INT32 sample);
void BurnSampleStop(INT32 sample, bool softstop = false);
void BurnSampleStopAll(bool softstop = false);
void BurnSampleSetLoop(INT32 sample, bool dothis);
INT32 BurnSampleGetStatus(INT32 sample);
INT32 BurnSampleGetPosition(INT32 sample);
void BurnSampleSetPosition(INT32 sample, UINT32 position);

void BurnSampleChannelPlay(INT32 channel, INT32 sample, INT32 loop = 0); // loop: -1, use config from sample struct
void BurnSampleChannelPause(INT32 channel, bool pause);
void BurnSampleChannelStop(INT32 channel);
INT32 BurnSampleGetChannelStatus(INT32 channel);
INT32 BurnSampleChannelGetPosition(INT32 channel);
void BurnSampleChannelSetPosition(INT32 channel, UINT32 position);
INT32 BurnSampleGetChannelSample(); // which sample is playing in this channel?

// how fast is playback for this sample? (0-400%)
void BurnSampleSetPlaybackRate(INT32 sample, INT32 rate); 
void BurnSampleReset();

void BurnSampleInit(INT32 bAdd);
void BurnSampleSetRoute(INT32 sample, INT32 nIndex, double nVolume, INT32 nRouteDir);

// BurnSampleSetRouteFade() fades up/down to volume to eliminate clicks/pops
// when a game changes volume often.
void BurnSampleSetRouteFade(INT32 sample, INT32 nIndex, double nVolume, INT32 nRouteDir);
void BurnSampleSetRouteFadeAllSamples(INT32 nIndex, double nVolume, INT32 nRouteDir);
void BurnSampleSetRouteAllSamples(INT32 nIndex, double nVolume, INT32 nRouteDir);

void BurnSampleScan(INT32 nAction, INT32 *pnMin);

void BurnSampleRender(INT16 *pDest, UINT32 pLen);
void BurnSampleExit();

// Buffered samples
void BurnSampleSetBuffered(INT32 (*pCPUCyclesCB)(), INT32 nCpuMHZ);
void BurnSampleSync();


extern INT32 bBurnSampleTrimSampleEnd; // set before BurnSampleInit();

#define BURN_SND_SAMPLE_ROUTE_1			0
#define BURN_SND_SAMPLE_ROUTE_2			1

#define BurnSampleSetAllRoutes(i, v, d)	{   				\
	BurnSampleSetRoute(i, BURN_SND_SAMPLE_ROUTE_1, v, d);	\
	BurnSampleSetRoute(i, BURN_SND_SAMPLE_ROUTE_2, v, d);   \
}

#define BurnSampleSetAllRoutesFade(i, v, d)	{   				\
	BurnSampleSetRouteFade(i, BURN_SND_SAMPLE_ROUTE_1, v, d);	\
	BurnSampleSetRouteFade(i, BURN_SND_SAMPLE_ROUTE_2, v, d);   \
}

#define BurnSampleSetAllRoutesAllSamples(v, d) {					\
	BurnSampleSetRouteAllSamples(BURN_SND_SAMPLE_ROUTE_1, v, d);	\
	BurnSampleSetRouteAllSamples(BURN_SND_SAMPLE_ROUTE_2, v, d);    \
}
