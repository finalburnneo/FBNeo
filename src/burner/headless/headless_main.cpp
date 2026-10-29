// Headless frontend for FinalBurn Neo.
// Author: Leonardo Kasperavičius
//
// Built originally to support OpenEmu-Silicon
// (https://github.com/OpenEmu-Silicon/OpenEmu-Silicon).
//
// Provides the burner-side globals the FBNeo library expects from a frontend and
// a small C API the host drives. Video/audio/input are routed through the
// intf/{video,audio,input}/headless plugins; driver init/exit lives in drv.cpp.
//
// Subject to the FB Neo license (see src/license.txt).

#include "burner.h"
#include "fbheadless.h"

// Implemented in headless_input.cpp.
void headless_map_inputs(void);

// Implemented in vid_headless.cpp (rotates the native frame to the display buffer).
void headless_video_present(void);

// --- Burner globals expected by the FBNeo library / shared burner code ---
int    nAppVirtualFps = 6000;      // app fps * 100
bool   bRunPause = 0;
bool   bAppDoFast = 0;
char   fpsstring[20] = "";
bool   bAppShowFPS = 0;
bool   bAlwaysProcessKeyboardInput = 0;
bool   bAppFullscreen = 0;
bool   bIntegerScale = false;
int    nWindowScale = 2;
bool   bAlwaysMenu = false;
int    nGameSelect = 0;
int    nFilterSelect = 0;
bool   bShowAvailableOnly = true;
bool   bShowClones = true;
int    gameSelectedFromFilter = -1;
UINT32 messageFrames = 0;
char   lastMessage[MESSAGE_MAX_LENGTH] = "";
int    bDrvSaveAll = 0;
TCHAR  szAppBurnVer[16] = _T("");

// --- Frontend audio state ---
static short *audBuffer = NULL;
static int    audFrames = 0;          // stereo sample frames produced last frame
static int    audSampleRate = 48000;

static int allocAudio()
{
	free(audBuffer);
	audBuffer = NULL;
	if (nBurnSoundLen <= 0)
		return 1;
	audBuffer = (short *)malloc((nBurnSoundLen * 2 + 16) * sizeof(short));
	if (audBuffer == NULL)
		return 1;
	memset(audBuffer, 0, (nBurnSoundLen * 2 + 16) * sizeof(short));
	return 0;
}

// --- C API ---

int FBHeadlessInit(void)
{
	BurnLibInit();
	return 0;
}

void FBHeadlessExit(void)
{
	DrvExit();
	VidExit();
	AudSoundExit();
	InputExit();
	free(audBuffer);
	audBuffer = NULL;
	BurnLibExit();
}

void FBHeadlessSetRomPath(const char *path)
{
	// BzipOpen builds zip paths as szAppRomPaths[d] + "<name>.zip" with no
	// separator, so the path must end with a slash.
	size_t n = strlen(path);
	if (n > 0 && path[n - 1] == '/')
		snprintf(szAppRomPaths[0], MAX_PATH, "%s", path);
	else
		snprintf(szAppRomPaths[0], MAX_PATH, "%s/", path);
}

int FBHeadlessLoadDriver(const char *shortName)
{
	UINT32 i;
	for (i = 0; i < nBurnDrvCount; i++) {
		nBurnDrvActive = i;
		if (strcmp(BurnDrvGetTextA(DRV_NAME), shortName) == 0)
			break;
	}
	if (i >= nBurnDrvCount)
		return 1;

	nAudSampleRate[0] = audSampleRate;
	nBurnSoundRate = audSampleRate;

	InputInit();
	AudSoundInit();

	if (DrvInit(i, true)) // true: restore battery-backed RAM
		return 1;

	VidInit(); // after DrvInit: the game's visible size is known
	headless_map_inputs();

	nAppVirtualFps = nBurnFPS;
	if (nAppVirtualFps <= 0)
		nAppVirtualFps = 6000;
	nBurnSoundLen = (audSampleRate * 100 + (nAppVirtualFps >> 1)) / nAppVirtualFps;
	allocAudio();

	FBHeadlessCheevosInit(); // build the achievement memory map
	return 0;
}

void FBHeadlessRunFrame(int draw)
{
	if (!bDrvOkay)
		return;

	InputMake(true);

	pBurnDraw = draw ? pVidImage : NULL;
	pBurnSoundOut = audBuffer;

	BurnDrvFrame();

	if (draw)
		headless_video_present();

	audFrames = nBurnSoundLen;
}

// Audio accessors
int FBHeadlessGetAudioSampleRate(void)   { return audSampleRate; }
const short *FBHeadlessGetAudioBuffer(void) { return audBuffer; }
int FBHeadlessGetAudioFrames(void)       { return audFrames; }

// Frame timing / geometry
int FBHeadlessGetFrameRate(void) { return nBurnFPS; }

int FBHeadlessGetAspectX(void)
{
	INT32 x = 0, y = 0;
	if (bDrvOkay)
		BurnDrvGetAspect(&x, &y);
	return x;
}

int FBHeadlessGetAspectY(void)
{
	INT32 x = 0, y = 0;
	if (bDrvOkay)
		BurnDrvGetAspect(&x, &y);
	return y;
}

void FBHeadlessReset(void)
{
	if (!bDrvOkay)
		return;
	DrvInit(nBurnDrvActive, true);  // full re-init; DrvExit saves battery RAM, DrvInit restores it
	VidInit();                      // DrvInit defers blitter init + frees buffers; recreate them
	headless_map_inputs();
	FBHeadlessCheevosInit();         // RAM buffers were reallocated; refresh the map
}

int FBHeadlessSaveState(const char *path)
{
	if (!bDrvOkay || path == NULL)
		return 1;
	TCHAR szName[1024];
	ANSIToTCHAR(path, szName, 1024);
	return BurnStateSave(szName, 1); // bAll=1: full state
}

int FBHeadlessLoadState(const char *path)
{
	if (!bDrvOkay || path == NULL)
		return 1;
	TCHAR szName[1024];
	ANSIToTCHAR(path, szName, 1024);
	return BurnStateLoad(szName, 1, NULL); // game already loaded, so no reload callback
}
