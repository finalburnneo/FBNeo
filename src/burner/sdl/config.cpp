// Burner Config file module
#include "burner.h"

int nIniVersion = 0;

#if defined(BUILD_SDL2) && !defined(SDL_WINDOWS)
void FixAndCreateSupportPath(TCHAR *pSupportFolderPath, TCHAR *pBaseFolderName)
{
	TCHAR *szSupportFolderFixedPath = NULL;

	if (strstr(pSupportFolderPath, pBaseFolderName) == NULL) {
		if (pSupportFolderPath[strlen(pSupportFolderPath) - 1] == '/') pSupportFolderPath[strlen(pSupportFolderPath) - 1] = '\0';
		szSupportFolderFixedPath = SDL_GetPrefPath("fbneo", pSupportFolderPath);
		_stprintf(pSupportFolderPath, _T("%s"), szSupportFolderFixedPath);
		SDL_free(szSupportFolderFixedPath);
	}
}

void InitSupportPaths()
{
	TCHAR *szBaseFolderName = NULL;
	szBaseFolderName = SDL_GetPrefPath(NULL, "fbneo");
	FixAndCreateSupportPath(szAppPreviewsPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppTitlesPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppCheatsPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppHiscorePath, szBaseFolderName);
	FixAndCreateSupportPath(szAppSamplesPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppHDDPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppIpsPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppIconsPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppBlendPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppSelectPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppVersusPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppHowtoPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppScoresPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppBossesPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppGameoverPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppFlyersPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppMarqueesPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppControlsPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppCabinetsPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppPCBsPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppHistoryPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppEEPROMPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppListsPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppDatListsPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppArchivesPath, szBaseFolderName);

	TCHAR szAppPresets[MAX_PATH] = _T("config/presets/");
	FixAndCreateSupportPath(szAppPresets, szBaseFolderName);

	SDL_free(szBaseFolderName);
}
#endif

static void CreateConfigName(char* szConfig)
{
#if defined(BUILD_SDL2) && !defined(SDL_WINDOWS)
	TCHAR *szSDLconfigPath = NULL;
	szSDLconfigPath = SDL_GetPrefPath("fbneo", "config");
	_stprintf(szConfig, _T("%sfbneo.ini"), szSDLconfigPath);
	SDL_free(szSDLconfigPath);
#else
	_stprintf(szConfig, _T("fbneo.ini"));
#endif

	return;
}

// Read in the config file for the whole application
int ConfigAppLoad()
{
	char  szConfig[MAX_PATH];
	FILE* f;

	CreateConfigName(szConfig);

	if ((f = _tfopen(szConfig, _T("rt"))) == NULL)
	{
		return 1;
	}
	printf("Loading config from %s\n", szConfig);
#define VAR(x)    { char *szValue = LabelCheck(szLine, #x); \
                    if (szValue) { x = strtol(szValue, NULL, 0); } }
#define FLT(x)    { char *szValue = LabelCheck(szLine, #x); \
                    if (szValue) { x = atof(szValue); } }
#define STR(x)    { char *szValue = LabelCheck(szLine, #x " "); \
                    if (szValue) { strcpy(x, szValue); } }

	// Go through each line of the config file
	char szLine[256];
	while (fgets(szLine, sizeof(szLine), f))
	{
		// Get rid of the linefeed and carriage return at the end
		int nLen = strlen(szLine);
		if (nLen > 0 && szLine[nLen - 1] == 10)
		{
			szLine[nLen - 1] = 0;
			nLen--;
		}
		if (nLen > 0 && szLine[nLen - 1] == 13)
		{
			szLine[nLen - 1] = 0;
			nLen--;
		}

		VAR(nIniVersion);
#ifndef BUILD_PI
		VAR(nVidSelect);                           // video mode select
		VAR(bVidFullStretch);
		VAR(nAutoFireRate);
		VAR(bAlwaysMenu);
		VAR(nGameSelect);
		VAR(nFilterSelect);
		VAR(bShowAvailableOnly);
		VAR(bShowClones);
		VAR(gameSelectedFromFilter);
#endif
		VAR(bVidScanlines);
		VAR(bDoGamma);
		FLT(nGamma);
		VAR(nAudSampleRate[0]);
		VAR(nAudDSPModule[0]);
		VAR(nInterpolation);
		VAR(nFMInterpolation);
		VAR(EnableHiscores);
		// Other
		STR(szAppRomPaths[0]);
		STR(szAppRomPaths[1]);
		STR(szAppRomPaths[2]);
		STR(szAppRomPaths[3]);
		STR(szAppRomPaths[4]);
		STR(szAppRomPaths[5]);
		STR(szAppRomPaths[6]);
		STR(szAppRomPaths[7]);
		STR(szAppRomPaths[8]);
		STR(szAppRomPaths[9]);
		STR(szAppRomPaths[10]);
		STR(szAppRomPaths[11]);
		STR(szAppRomPaths[12]);
		STR(szAppRomPaths[13]);
		STR(szAppRomPaths[14]);
		STR(szAppRomPaths[15]);
		STR(szAppRomPaths[16]);
		STR(szAppRomPaths[17]);
		STR(szAppRomPaths[18]);
		STR(szAppRomPaths[19]);
		STR(szAppPreviewsPath);
		STR(szAppTitlesPath);
		STR(szAppCheatsPath);
		STR(szAppHiscorePath);
		STR(szAppSamplesPath);
		STR(szAppHDDPath);
		STR(szAppIpsPath);
		STR(szAppIconsPath);
		STR(szAppBlendPath);
		STR(szAppSelectPath);
		STR(szAppVersusPath);
		STR(szAppHowtoPath);
		STR(szAppScoresPath);
		STR(szAppBossesPath);
		STR(szAppGameoverPath);
		STR(szAppFlyersPath);
		STR(szAppMarqueesPath);
		STR(szAppControlsPath);
		STR(szAppCabinetsPath);
		STR(szAppPCBsPath);
		STR(szAppHistoryPath);
		STR(szAppEEPROMPath);
		STR(szAppListsPath);
		STR(szAppDatListsPath);
		STR(szAppArchivesPath);

#undef STR
#undef FLT
#undef VAR
	}

	fclose(f);
	return 0;
}

// Write out the config file for the whole application
int ConfigAppSave()
{
	char  szConfig[MAX_PATH];
	FILE* f;

	CreateConfigName(szConfig);

	if ((f = _tfopen(szConfig, _T("wt"))) == NULL)
	{
		return 1;
	}


#define VAR(x)    fprintf(f, #x " %d\n", x)
#define FLT(x)    fprintf(f, #x " %f\n", x)
#define STR(x)    fprintf(f, #x " %s\n", x)

	fprintf(f, "\n// The application version this file was saved from\n");
	// We can't use the macros for this!
	fprintf(f, "nIniVersion 0x%06X", nBurnVer);

#ifndef BUILD_PI
	fprintf(f, "\n// video mode 0 = standard SDL. 1 = SDL1 opengl (don't use on SDL2!!!!!!)\n");
	VAR(nVidSelect);              // video mode select
	fprintf(f, "\n// If non-zero, allow stretching of the image to any size\n");
	VAR(bVidFullStretch);
	fprintf(f, "\n// Auto-Fire Rate, non-linear - use the GUI to change this setting!\n");
	VAR(nAutoFireRate);
	fprintf(f, "\n// Automatically go to the menu\n");
	VAR(bAlwaysMenu);
	fprintf(f, "\n// Last game selection\n");
	VAR(nGameSelect);
	fprintf(f, "\n// Last filter selection\n");
	VAR(nFilterSelect);
	fprintf(f, "\n// show available game only filter\n");
	VAR(bShowAvailableOnly);
	fprintf(f, "\n// show clones game filter\n");
	VAR(bShowClones);
	fprintf(f, "\n// Last game selected from filter\n");
	VAR(gameSelectedFromFilter);
#endif
	fprintf(f, "\n// If non-zero, enable scanlines\n");
	VAR(bVidScanlines);
	fprintf(f, "\n// If non-zero, enable software gamma correction\n");
	VAR(bDoGamma);
	_ftprintf(f, _T("\n// Gamma to correct with\n"));
	FLT(nGamma);
	fprintf(f, "\n// Audio sample rate\n");
	VAR(nAudSampleRate[0]);
	fprintf(f, "\n// If non-zero, enable DSP filter\n");
	VAR(nAudDSPModule[0]);
	_ftprintf(f, _T("\n// The order of PCM/ADPCM interpolation\n"));
	VAR(nInterpolation);
	_ftprintf(f, _T("\n// The order of FM interpolation\n"));
	VAR(nFMInterpolation);
	_ftprintf(f, _T("\n// If non-zero, enable high score saving support.\n"));
	VAR(EnableHiscores);

	fprintf(f, "\n// The paths to search for rom zips. (include trailing slash)\n");
	STR(szAppRomPaths[0]);
	STR(szAppRomPaths[1]);
	STR(szAppRomPaths[2]);
	STR(szAppRomPaths[3]);
	STR(szAppRomPaths[4]);
	STR(szAppRomPaths[5]);
	STR(szAppRomPaths[6]);
	STR(szAppRomPaths[7]);
	STR(szAppRomPaths[8]);
	STR(szAppRomPaths[9]);
	STR(szAppRomPaths[10]);
	STR(szAppRomPaths[11]);
	STR(szAppRomPaths[12]);
	STR(szAppRomPaths[13]);
	STR(szAppRomPaths[14]);
	STR(szAppRomPaths[15]);
	STR(szAppRomPaths[16]);
	STR(szAppRomPaths[17]);
	STR(szAppRomPaths[18]);
	STR(szAppRomPaths[19]);
	fprintf(f, "\n// UNUSED CURRENTLY (include trailing slash)\n");
	STR(szAppPreviewsPath);
	fprintf(f, "\n// Path to titlescreen images for use on the menu (include trailing slash)\n");
	STR(szAppTitlesPath);
	fprintf(f, "\n// Cheat files path (include trailing slash)\n");
	STR(szAppCheatsPath);
	fprintf(f, "\n// Hiscore save path (include trailing slash)\n");
	STR(szAppHiscorePath);
	fprintf(f, "\n// Game Samples (where required) (include trailing slash)\n");
	STR(szAppSamplesPath);
	fprintf(f, "\n// HDD image path (include trailing slash)\n");
	STR(szAppHDDPath);
	fprintf(f, "\n// EEPROM save path (include trailing slash)\n");
	STR(szAppEEPROMPath);
	fprintf(f, "\n// UNUSED CURRENTLY (include trailing slash)\n");
	STR(szAppIpsPath);
	fprintf(f, "\n// UNUSED CURRENTLY (include trailing slash)\n");
	STR(szAppIconsPath);
	fprintf(f, "\n// UNUSED CURRENTLY (include trailing slash)\n");
	STR(szAppBlendPath);
	fprintf(f, "\n// UNUSED CURRENTLY (include trailing slash)\n");
	STR(szAppSelectPath);
	fprintf(f, "\n// UNUSED CURRENTLY (include trailing slash)\n");
	STR(szAppVersusPath);
	fprintf(f, "\n// UNUSED CURRENTLY (include trailing slash)\n");
	STR(szAppHowtoPath);
	fprintf(f, "\n// UNUSED CURRENTLY (include trailing slash)\n");
	STR(szAppScoresPath);
	fprintf(f, "\n// UNUSED CURRENTLY (include trailing slash)\n");
	STR(szAppBossesPath);
	fprintf(f, "\n// UNUSED CURRENTLY (include trailing slash)\n");
	STR(szAppGameoverPath);
	fprintf(f, "\n// UNUSED CURRENTLY (include trailing slash)\n");
	STR(szAppFlyersPath);
	fprintf(f, "\n// UNUSED CURRENTLY (include trailing slash)\n");
	STR(szAppMarqueesPath);
	fprintf(f, "\n// UNUSED CURRENTLY (include trailing slash)\n");
	STR(szAppControlsPath);
	fprintf(f, "\n// UNUSED CURRENTLY (include trailing slash)\n");
	STR(szAppCabinetsPath);
	fprintf(f, "\n// UNUSED CURRENTLY (include trailing slash)\n");
	STR(szAppPCBsPath);
	fprintf(f, "\n// UNUSED CURRENTLY (include trailing slash)\n");
	STR(szAppHistoryPath);
	fprintf(f, "\n// UNUSED CURRENTLY (include trailing slash)\n");
	STR(szAppListsPath);
	fprintf(f, "\n// UNUSED CURRENTLY (include trailing slash)\n");
	STR(szAppDatListsPath);
	fprintf(f, "\n// UNUSED CURRENTLY (include trailing slash)\n");
	STR(szAppArchivesPath);
	fprintf(f, "\n\n\n");

#undef STR
#undef FLT
#undef VAR

	fclose(f);
	return 0;
}
