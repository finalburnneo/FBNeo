#define SAMPLE_IGNORE		(1<<0) // don't ever play this sample
#define SAMPLE_AUTOLOOP		(1<<1) // start the looping on start
#define SAMPLE_NOLOOP		(1<<2) // don't allow this to loop

// Change this to 0 to 1 if using samples in a low-ram environment.
// May cause momentary stutter while sample loads.
#define SAMPLE_NOSTORE		(0<<3) // only keep in memory while playing

void BurnSamplePlay(INT32 sample);
void BurnSamplePause(INT32 sample);
void BurnSampleResume(INT32 sample);
void BurnSampleStop(INT32 sample);

void BurnSampleSetLoop(INT32 sample, bool dothis);

INT32  BurnSampleGetStatus(INT32 sample);

INT32  BurnSampleGetPosition(INT32 sample);
void BurnSampleSetPosition(INT32 sample, UINT32 position);

// how fast is playback for this sample? (0-400%)
void BurnSampleSetPlaybackRate(INT32 sample, INT32 rate); 
void BurnSampleReset();

void BurnSampleInit(INT32 bAdd);
void BurnSampleSetRoute(INT32 sample, INT32 nIndex, double nVolume, INT32 nRouteDir);
void BurnSampleSetRouteAllSamples(INT32 nIndex, double nVolume, INT32 nRouteDir);

void BurnSampleScan(INT32 nAction, INT32 *pnMin);

void BurnSampleRender(INT16 *pDest, UINT32 pLen);
void BurnSampleExit();

extern INT32 bBurnSampleTrimSampleEnd; // set before BurnSampleInit();

#define BURN_SND_SAMPLE_ROUTE_1			0
#define BURN_SND_SAMPLE_ROUTE_2			1

#define BurnSampleSetAllRoutes(i, v, d)						\
	BurnSampleSetRoute(i, BURN_SND_SAMPLE_ROUTE_1, v, d);	\
	BurnSampleSetRoute(i, BURN_SND_SAMPLE_ROUTE_2, v, d);

#define BurnSampleSetAllRoutesAllSamples(v, d)						\
	BurnSampleSetRouteAllSamples(BURN_SND_SAMPLE_ROUTE_1, v, d);	\
	BurnSampleSetRouteAllSamples(BURN_SND_SAMPLE_ROUTE_2, v, d);
