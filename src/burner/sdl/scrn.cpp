#include "burner.h"

bool didReinitialise = false;

void Reinitialise()
{
	pVidImage = NULL;
	didReinitialise = true;
	VidReInitialise();
}

INT32 is_netgame_or_recording() // returns: 1 = netgame, 2 = recording/playback
{
	return 0;
}
