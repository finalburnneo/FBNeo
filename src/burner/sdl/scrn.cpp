#include "burner.h"

bool didReinitialise = false;

void Reinitialise()
{
	didReinitialise = true;
	VidReInitialise();
	pBurnDraw = NULL;
}

void ReinitialiseVideo()
{
	Reinitialise();
}

INT32 is_netgame_or_recording() // returns: 1 = netgame, 2 = recording/playback
{
	return 0;
}
