#include "burner.h"

/// Save States
#ifdef BUILD_SDL2
static char* szSDLSavePath = NULL;
#endif

// The automatic save
int QuickState(int bSave)
{
	static TCHAR szName[MAX_PATH] = _T("");
	int nRet;

#if defined(BUILD_SDL2) && !defined(SDL_WINDOWS)
	if (szSDLSavePath == NULL)
	{
		szSDLSavePath = SDL_GetPrefPath("fbneo", "states");
	}

	snprintf(szName, MAX_PATH, "%squick_%s.fs", szSDLSavePath, BurnDrvGetText(DRV_NAME));
#else

	_stprintf(szName, _T("config/games/quick_%s.fs"), BurnDrvGetText(DRV_NAME));

#endif

	if (bSave == 0)
	{
		nRet = BurnStateLoad(szName, 1, NULL);		// Load ram
		UpdateMessage("Quicksave: State Loaded");
	}
	else
	{
		nRet = BurnStateSave(szName, 1);				// Save ram
		UpdateMessage("Quicksave: State Saved");
	}

	return nRet;
}
