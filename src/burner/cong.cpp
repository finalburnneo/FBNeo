// Burner Config for Game file module
#include "burner.h"
#include "neocdlist.h"

const INT32 nConfigMinVersion = 0x020921;

bool bSaveInputs = true;

static TCHAR* GameConfigName()
{
	static TCHAR szName[MAX_PATH];

	#if defined(BUILD_SDL2) && !defined(SDL_WINDOWS)
		if (NeoCDInfo_ID()) {
		  _stprintf(szName, _T("%sngcd_%s.ini"), szAppEEPROMPath, BurnDrvGetText(DRV_NAME));
		} else {
			_stprintf(szName, _T("%s%s.ini"), szAppEEPROMPath, BurnDrvGetText(DRV_NAME));
		}
	#else
		// Return the path of the config file for this game
		if (NeoCDInfo_ID()) {
			_stprintf(szName, _T("config/games/ngcd_%s.ini"), NeoCDInfo_Text(DRV_NAME));
		} else {
			_stprintf(szName, _T("config/games/%s.ini"), BurnDrvGetText(DRV_NAME));
		}
	#endif
	return szName;
}

// Read in the config file for the game-specific inputs
INT32 ConfigGameLoad(bool bOverWrite)
{
	TCHAR szLine[256];
	INT32 nFileVersion = 0;

	FILE* h = _tfopen(GameConfigName(), _T("rt"));
	if (h == NULL) {
		// Not command line start, always -1
		if (-1 == nSubDrvSelected) return 1;

		// Command line to start a subgame - game configuration not found
		h = _tfopen(GameConfigName(), _T("wt"));	// Create
		if (h == NULL) return 1;

		ConfigGameSave(true);						// Write default configuration and save
		fclose(h);

		h = _tfopen(GameConfigName(), _T("rt"));	// Reopen
		if (h == NULL) return 1;
	}

	if (bOverWrite) {
		nAnalogSpeed = 0x0100;
		nBurnCPUSpeedAdjust = 0x0100;
	}

	// Go through each line of the config file and process inputs
	while (_fgetts(szLine, 256, h)) {
		TCHAR *szValue;
		INT32 nLen = _tcslen(szLine);

		// Get rid of the linefeed and carriage return at the end
		if (nLen > 0 && szLine[nLen - 1] == 10) {
			szLine[nLen - 1] = 0;
			nLen--;
		}
		if (nLen > 0 && szLine[nLen - 1] == 13)
		{
			szLine[nLen - 1] = 0;
			nLen--;
		}

		szValue = LabelCheck(szLine, _T("version"));
		if (szValue) {
			nFileVersion = _tcstol(szValue, NULL, 0);
		}

		if (bOverWrite) {
			szValue = LabelCheck(szLine, _T("analog"));
			if (szValue) {
				nAnalogSpeed = _tcstol(szValue, NULL, 0);
			}
			szValue = LabelCheck(szLine, _T("cpu"));
			if (szValue) {
				nBurnCPUSpeedAdjust = _tcstol(szValue, NULL, 0);
			}
		}

		if (nConfigMinVersion <= nFileVersion && nFileVersion <= nBurnVer) {
			szValue = LabelCheck(szLine, _T("input"));
			if (szValue) {
				GameInpRead(szValue, bOverWrite);
				continue;
			}

			szValue = LabelCheck(szLine, _T("macro"));
			if (szValue) {
				GameInpMacroRead(szValue, bOverWrite);
				continue;
			}

			szValue = LabelCheck(szLine, _T("afire"));
			if (szValue) {
				GameMacroAutofireRead(szValue, bOverWrite);
				continue;
			}

			szValue = LabelCheck(szLine, _T("custom"));
			if (szValue) {
				GameInpCustomRead(szValue, bOverWrite);
				continue;
			}
		}
	}

	fclose(h);
	return 0;
}

// Write out the config file for the game-specific inputs
INT32 ConfigGameSave(bool bSave)
{
	FILE* h;

	if (!bSave) {
		GameInpBlank(0);
		ConfigGameLoad(false);
	}

	h = _tfopen(GameConfigName(), _T("wt"));
	if (h == NULL) {
		return 1;
	}

	// Write title
	_ftprintf(h, _T("// ") _T(APP_TITLE) _T(" v%s --- Config File for %s (%s)\n\n"), szAppBurnVer, BurnDrvGetText(DRV_NAME), ANSIToTCHAR(BurnDrvGetTextA(DRV_FULLNAME), NULL, 0));

	_ftprintf(h, _T("// --- Miscellaneous ----------------------------------------------------------\n\n"));
	// Write version number
	_ftprintf(h, _T("version 0x%06X\n\n"), nBurnVer);
	// Write speed for relative analog controls
	_ftprintf(h, _T("analog  0x%04X\n"), nAnalogSpeed);
	// Write CPU speed adjustment
	_ftprintf(h, _T("cpu     0x%04X\n"), nBurnCPUSpeedAdjust);

	_ftprintf(h, _T("\n\n\n"));
	_ftprintf(h, _T("// --- Inputs -----------------------------------------------------------------\n\n"));

	GameInpWrite(h);

	fclose(h);
	return 0;
}
