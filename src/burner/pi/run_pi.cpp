// Run module
#include "burner.h"

bool bAltPause = 0;

int bAlwaysDrawFrames = 0;

static bool bShowFPS = false;

int counter;								// General purpose variable used when debugging
int nExitEmulator = 0;

static unsigned int nNormalLast = 0;		// Last value of timeGetTime()
static int nNormalFrac = 0;					// Extra fraction we did

static bool bAppDoStep = 0;
bool bAppDoFast = 0;
static int nFastSpeed = 6;

int SaveNVRAM();
int ReadNVRAM();

// define this function somewhere above RunMessageLoop()
void ToggleLayer(unsigned char thisLayer)
{
	nBurnLayer ^= thisLayer;				// xor with thisLayer
	VidRedraw();
	VidPaint(0);
}

// With or without sound, run one frame.
// If bDraw is true, it's the last frame before we are up to date, and so we should draw the screen
static int RunFrame(int bDraw, int bPause)
{
	static int bPrevPause = 0;
	static int bPrevDraw = 0;

	if (bPrevDraw && !bPause) {
		VidPaint(0);
	}
	if (!bDrvOkay) {
		return 1;
	}

	if (bPause) {
		InputMake(false);
		if (bPause != bPrevPause) {
			VidPaint(2);
		}
	} else {
		nFramesEmulated++;
		nCurrentFrame++;
		InputMake(true);
	}

	if (bDraw) {
		nFramesRendered++;
		if (VidFrame()) {
			AudBlankSound();
		}
	} else {
		pBurnDraw = NULL;
		BurnDrvFrame();
	}

	bPrevPause = bPause;
	bPrevDraw = bDraw;

	return 0;
}

// Callback used when DSound needs more sound
static int RunGetNextSound(int bDraw)
{
	if (nAudNextSound == NULL) {
		return 1;
	}

	if (bRunPause) {
		if (bAppDoStep) {
			RunFrame(bDraw, 0);
			memset(nAudNextSound, 0, nAudSegLen << 2);	// Write silence into the buffer
		} else {
			RunFrame(bDraw, 1);
		}

		bAppDoStep = 0;									// done one step
		return 0;
	}

	if (bAppDoFast) {									// do more frames
		for (int i = 0; i < nFastSpeed; i++) {
			RunFrame(0, 0);
		}
	}

	// Render frame with sound
	pBurnSoundOut = nAudNextSound;
	RunFrame(bDraw, 0);
	if (bAppDoStep) {
		memset(nAudNextSound, 0, nAudSegLen << 2);		// Write silence into the buffer
	}
	bAppDoStep = 0;										// done one step

	return 0;
}

int RunIdle()
{
	int nTime, nCount;

	if (bAudPlaying) {
		// Run with sound
		AudSoundCheck();
		return 0;
	}

	// Run without sound
	nTime = SDL_GetTicks() - nNormalLast;
	nCount = (nTime * nAppVirtualFps - nNormalFrac) / 100000;
	if (nCount <= 0) {						// No need to do anything for a bit
		SDL_Delay(3);

		return 0;
	}

	nNormalFrac += nCount * 100000;
	nNormalLast += nNormalFrac / nAppVirtualFps;
	nNormalFrac %= nAppVirtualFps;

	if (bAppDoFast){						// Temporarily increase virtual fps
		nCount *= nFastSpeed;
	}
	if (nCount > 100) {						// Limit frame skipping
		nCount = 100;
	}
	if (bRunPause) {
		if (bAppDoStep) {					// Step one frame
			nCount = 10;
		} else {
			RunFrame(1, 1);					// Paused
			return 0;
		}
	}
	bAppDoStep = 0;

	for (int i = nCount / 10; i > 0; i--) {	// Mid-frames
		RunFrame(!bAlwaysDrawFrames, 0);
	}
	RunFrame(1, 0);							// End-frame
	// temp added for SDLFBA
	//VidPaint(0);
	return 0;
}

int RunReset()
{
	// Reset the speed throttling code
	nNormalLast = 0; nNormalFrac = 0;
	if (!bAudPlaying) {
		// run without sound
		nNormalLast = SDL_GetTicks();
	}
	return 0;
}

static int RunInit()
{
	// Try to run with sound
	AudSetCallback(RunGetNextSound);
	AudSoundPlay();

	RunReset();

	ReadNVRAM();

	return 0;
}

static int RunExit()
{
	nNormalLast = 0;
	SaveNVRAM();

	return 0;
}

// The main message loop
int RunMessageLoop()
{
	nExitEmulator = 0;
	MediaInit();

	RunInit();

	GameInpCheckMouse();
	while (!nExitEmulator) {
		SDL_Event event;
		if ( SDL_PollEvent(&event) ) {
		switch (event.type) {
			case SDL_QUIT:
				nExitEmulator = 1;
				break;
			}
		} else {
			RunIdle();
		}
	}

	RunExit();

	return 0;
}

int SaveNVRAM()
{
	char temp[256];
	snprintf(temp, 255, "nvram/%s.nvr", BurnDrvGetTextA(0));

	fprintf(stderr, "Writing NVRAM to \"%s\"\n", temp);
	BurnStateSave(temp, 0);

    return 0;
}

int ReadNVRAM()
{
	char temp[256];
	snprintf(temp, 255, "nvram/%s.nvr", BurnDrvGetTextA(0));

	fprintf(stderr, "Reading NVRAM from \"%s\"\n", temp);
    BurnStateLoad(temp, 0, NULL);

    return 0;
}
