// Burner Config file module
#include "burner.h"

int nIniVersion = 0;

#ifdef BUILD_SDL2
static char* szSDLconfigPath = NULL;
#endif

static void CreateConfigName(char* szConfig)
{
#ifndef _WIN32
	char cfgdir[MAX_PATH] = { 0 };
#ifdef BUILD_SDL2
	if (szSDLconfigPath == NULL)
	{
		szSDLconfigPath = SDL_GetPrefPath("fbneo", "config");
	}

	snprintf(cfgdir, MAX_PATH, "%sfbneo.ini", szSDLconfigPath);
#else
	snprintf(cfgdir, MAX_PATH, "fbneo.ini");
#endif
	memcpy(szConfig, cfgdir, sizeof(cfgdir));
#else
	memcpy(szConfig, "fbneo.ini", 10);
#endif

	return;
}

// Read in the config file for the whole application
int ConfigAppLoad()
{
	char  szConfig[MAX_PATH];
	FILE* f;

	CreateConfigName(szConfig);

	if ((f = fopen(szConfig, "rt")) == NULL)
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
		// Get rid of the linefeed at the end
		int nLen = strlen(szLine);
		if (szLine[nLen - 1] == 10)
		{
			szLine[nLen - 1] = 0;
			nLen--;
		}

		VAR(nIniVersion);
#ifndef BUILD_PI
		VAR(nVidSelect);                           // video mode select
		VAR(bVidFullStretch);
		VAR(nAutoFireRate);
#endif
		VAR(bVidScanlines);
		VAR(bDoGamma);
		FLT(nGamma);
		VAR(nAudSampleRate[0]);
		VAR(nAudDSPModule[0]);
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

	if ((f = fopen(szConfig, "wt")) == NULL)
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
	fprintf(f, "\n// video mode 0 = standard SDL 1= (very expiermental) opengl\n");
	VAR(nVidSelect);              // video mode select
	fprintf(f, "\n// If non-zero, allow stretching of the image to any size\n");
	VAR(bVidFullStretch);
	fprintf(f, "\n// Auto-Fire Rate, non-linear - use the GUI to change this setting!\n");
	VAR(nAutoFireRate);
#endif
	fprintf(f, "\n// If non-zero, enable scanlines\n");
	VAR(bVidScanlines);
	fprintf(f, "\n// If non-zero, enable software gamma correction\n");
	VAR(bDoGamma);
	_ftprintf(f, _T("\n// Gamma to correct with\n"));
	FLT(nGamma);
	VAR(nAudSampleRate[0]);
	fprintf(f, "\n// If non-zero, enable DSP filter\n");
	VAR(nAudDSPModule[0]);

	_ftprintf(f, _T("\n// If non-zero, enable high score saving support.\n"));
	VAR(EnableHiscores);

	fprintf(f, "\n// The paths to search for rom zips. (include trailing backslash)\n");
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

	fprintf(f, "\n\n\n");

#undef STR
#undef FLT
#undef VAR

	fclose(f);
	return 0;
}
