// Headless audio output for FinalBurn Neo.
// Author: Leonardo Kasperavičius
//
// Built originally to support OpenEmu-Silicon
// (https://github.com/OpenEmu-Silicon/OpenEmu-Silicon).
//
// The frontend drives sound directly (it points pBurnSoundOut at its own buffer
// and reads it after BurnDrvFrame), so this plugin only satisfies the AudOut
// interface. Subject to the FB Neo license (see src/license.txt).

#include "burner.h"

static int HeadlessSoundBlank()   { return 0; }
static int HeadlessSoundCheck()   { return 0; }

static int HeadlessSoundInit()
{
	if (nAudSampleRate[0] <= 0)
		return 1;
	bAudOkay = 1;
	return 0;
}

static int HeadlessSetCallback(int (* /* pCallback */)(int)) { return 0; }
static int HeadlessSoundPlay()      { bAudPlaying = 1; return 0; }
static int HeadlessSoundStop()      { bAudPlaying = 0; return 0; }

static int HeadlessSoundExit()
{
	bAudOkay = 0;
	bAudPlaying = 0;
	return 0;
}

static int HeadlessSoundSetVolume()           { return 0; }
static int HeadlessSoundGetSettings(InterfaceInfo *) { return 0; }

struct AudOut AudOutHeadless = {
	HeadlessSoundBlank,
	HeadlessSoundCheck,
	HeadlessSoundInit,
	HeadlessSetCallback,
	HeadlessSoundPlay,
	HeadlessSoundStop,
	HeadlessSoundExit,
	HeadlessSoundSetVolume,
	HeadlessSoundGetSettings,
	_T("Headless audio output"),
};
