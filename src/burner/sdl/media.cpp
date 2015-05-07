// Media module
#include "burner.h"

int MediaInit()
{
	//ScrnInit()			// Init the Scrn Window

	if (!bInputOkay) {
		InputInit();		// Init Input
	}

	nAppVirtualFps = nBurnFPS;

	if (!bAudOkay) {
		AudSoundInit();		// Init Sound (not critical if it fails)
	}

	nBurnSoundRate = 0;		// Assume no sound
	pBurnSoundOut = NULL;
	if (bAudOkay) {
		nBurnSoundRate = nAudSampleRate[nAudSelect];
		nBurnSoundLen = nAudSegLen;
	}

	if (!bVidOkay) {
		// Reinit the video plugin
		VidInit();
		if (!bVidOkay && nVidFullscreen) {

			nVidFullscreen = 0;

			MediaExit();
			return (MediaInit());
		}
		if (!nVidFullscreen) {
			//ScrnSize();
		}

		if (!bVidOkay) {
			printf("Initialized video: %s\n", VidGetModuleName());
		}

		if (bVidOkay && (bRunPause || !bDrvOkay)) {
			VidRedraw();
		}
	}

	return 0;
}

int MediaExit()
{
	nBurnSoundRate = 0;		// Blank sound
	pBurnSoundOut = NULL;

	AudSoundExit();			// Exit sound

	VidExit();

	InputExit();

	//ScrnExit();			// Exit the Scrn Window

	return 0;
}
 
