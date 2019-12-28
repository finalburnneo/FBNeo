/*----------------
 * Stuff to finish:
 *
 * It wouldn't be a stretch of the imagination to think the whole of the sdl 'port' needs a redo but here are the main things wrong with this version:
 *
 *
 * The audio output code code could maybe do with some work, but it's getting there...
 * There are lots of problems with the opengl renderer, but you should just use the SDL2 build anyway
 *
 * TODO for SDL2:
 * Add autostart to menu as an ini config option
 * Add previews, etc to menu
 * Add menu for options e.g. dips, mapping, saves, IPS patches
 * Maybe a better font output setup with some sort of scaling, maybe add sdl1 support?
 * Add joypad support to menu
 * figure out what is going on with the sdl sound output, something breaks after a few frames
 * ------------------*/

#include "burner.h"

INT32 Init_Joysticks(int p1_use_joystick);

int  nAppVirtualFps = 6000;         // App fps * 100
bool bRunPause = 0;
bool bAppFullscreen = 0;
bool bAlwaysProcessKeyboardInput = 0;
int  usemenu = 0, usejoy = 0, vsync = 1, dat = 0;
bool saveconfig = 1;

TCHAR szAppBurnVer[16];
char videofiltering[3];

#ifdef BUILD_SDL2
static char* szSDLeepromPath = NULL;
static char* szSDLhiscorePath = NULL;
static char* szSDLHDDPath = NULL;
static char* szSDLSamplePath = NULL;
#endif

#define set_commandline_option(i,v) i = v; saveconfig = 0;
#define set_commandline_option_string(i, v, length) snprintf(i, length, v); saveconfig = 0;

int parseSwitches(int argc, char* argv[])
{
	for (int i = 1; i < argc; i++)
	{
		if (*argv[i] != '-')
		{
			continue;
		}

		if (strcmp(argv[i] + 1, "joy") == 0)
		{
			set_commandline_option(usejoy, 1)
		}

		if (strcmp(argv[i] + 1, "menu") == 0)
		{
			set_commandline_option(usemenu, 1)
		}

		if (strcmp(argv[i] + 1, "novsync") == 0)
		{
			set_commandline_option(vsync, 0) // might need to change this to bit flags if there is more than one gfx option
		}

		if (strcmp(argv[i] + 1, "dat") == 0)
		{
			set_commandline_option(dat, 1)
		}

		if (strcmp(argv[i] + 1, "fullscreen") == 0)
		{
			set_commandline_option(bAppFullscreen, 1)
		}

		if (strcmp(argv[i] + 1, "nearest") == 0)
		{
			set_commandline_option_string(videofiltering, "0", 3)
		}

		if (strcmp(argv[i] + 1, "linear") == 0)
		{
			set_commandline_option_string(videofiltering, "1", 3)
		}

		if (strcmp(argv[i] + 1, "best") == 0)
		{
			set_commandline_option_string(videofiltering, "2", 3)
		}
		if (strcmp(argv[i] + 1, "autosave") == 0)
		{
			bDrvSaveAll = 1;
		}
		if (strcmp(argv[i] + 1, "cd") == 0)
		{
			_tcscpy(CDEmuImage, argv[i + 1]);
		}
	}
	return 0;
}

void generateDats()
{
	char filename[1024] = { 0 };

#if defined(BUILD_SDL2)
	printf("Creating fbneo dats in %s\n", SDL_GetPrefPath("fbneo", "dats"));
#if defined(ALT_DAT)
	sprintf(filename, "%sFBNeo_-_ALL_SYSTEMS.dat", SDL_GetPrefPath("fbneo", "dats"));
	create_datfile(filename, -1);
#endif
	sprintf(filename, "%sFBNeo_-_Arcade.dat", SDL_GetPrefPath("fbneo", "dats"));
	create_datfile(filename, DAT_ARCADE_ONLY);

	sprintf(filename, "%sFBNeo_-_MegaDrive.dat", SDL_GetPrefPath("fbneo", "dats"));
	create_datfile(filename, DAT_MEGADRIVE_ONLY);

	sprintf(filename, "%sFBNeo_-_PC_ENGINE.dat", SDL_GetPrefPath("fbneo", "dats"));
	create_datfile(filename, DAT_PCENGINE_ONLY);

	sprintf(filename, "%sFBNeo_-_TurboGrafx-16.dat", SDL_GetPrefPath("fbneo", "dats"));
	create_datfile(filename, DAT_TG16_ONLY);

	sprintf(filename, "%sFBNeo_-_PC_Engine_SuperGrafx.dat", SDL_GetPrefPath("fbneo", "dats"));
	create_datfile(filename, DAT_SGX_ONLY);

	sprintf(filename, "%sFBNeo_-_Sega_SG-1000.dat", SDL_GetPrefPath("fbneo", "dats"));
	create_datfile(filename, DAT_SG1000_ONLY);

	sprintf(filename, "%sFBNeo_-_ColecoVision.dat", SDL_GetPrefPath("fbneo", "dats"));
	create_datfile(filename, DAT_COLECO_ONLY);

	sprintf(filename, "%sFBNeo_-_Sega_Master_System.dat", SDL_GetPrefPath("fbneo", "dats"));
	create_datfile(filename, DAT_MASTERSYSTEM_ONLY);

	sprintf(filename, "%sFBNeo_-_Sega_Game_Gear.dat", SDL_GetPrefPath("fbneo", "dats"));
	create_datfile(filename, DAT_GAMEGEAR_ONLY);

	sprintf(filename, "%sFBNeo_-_MSX.dat", SDL_GetPrefPath("fbneo", "dats"));
	create_datfile(filename, DAT_MSX_ONLY);

	sprintf(filename, "%sFBNeo_-_Sinclair_ZX_Spectrum.dat", SDL_GetPrefPath("fbneo", "dats"));
	create_datfile(filename, DAT_SPECTRUM_ONLY);

	sprintf(filename, "%sFBNeo_-_Neogeo.dat", SDL_GetPrefPath("fbneo", "dats"));
	create_datfile(filename, DAT_NEOGEO_ONLY);
#else
	printf("Creating fbneo dats\n");

#if defined(ALT_DAT)
	sprintf(filename, "FBNeo_-_ALL_SYSTEMS.dat");
	create_datfile(filename, -1);
#endif
	sprintf(filename, "FBNeo_-_Arcade.dat");
	create_datfile(filename, DAT_ARCADE_ONLY);

	sprintf(filename, "FBNeo_-_MegaDrive.dat");
	create_datfile(filename, DAT_MEGADRIVE_ONLY);

	sprintf(filename, "FBNeo_-_PC_ENGINE.dat");
	create_datfile(filename, DAT_PCENGINE_ONLY);

	sprintf(filename, "FBNeo_-_TurboGrafx-16.dat");
	create_datfile(filename, DAT_TG16_ONLY);

	sprintf(filename, "FBNeo_-_PC_Engine_SuperGrafx.dat");
	create_datfile(filename, DAT_SGX_ONLY);

	sprintf(filename, "FBNeo_-_Sega_SG-1000.dat");
	create_datfile(filename, DAT_SG1000_ONLY);

	sprintf(filename, "FBNeo_-_ColecoVision.dat");
	create_datfile(filename, DAT_COLECO_ONLY);

	sprintf(filename, "FBNeo_-_Sega_Master_System.dat");
	create_datfile(filename, DAT_MASTERSYSTEM_ONLY);

	sprintf(filename, "FBNeo_-_Sega_Game_Gear.dat");
	create_datfile(filename, DAT_GAMEGEAR_ONLY);

	sprintf(filename, "FBNeo_-_MSX.dat");
	create_datfile(filename, DAT_MSX_ONLY);

	sprintf(filename, "FBNeo_-_Sinclair_ZX_Spectrum.dat");
	create_datfile(filename, DAT_SPECTRUM_ONLY);

	sprintf(filename, "FBNeo_-_Neogeo.dat");
	create_datfile(filename, DAT_NEOGEO_ONLY);
#endif
}


void DoGame(int gameToRun)
{
	if (!DrvInit(gameToRun, 0))
	{
		MediaInit();
		Init_Joysticks(usejoy);
		RunMessageLoop();
	}
	else
	{
		printf("There was an error loading your selected game.\n");
	}

	if (saveconfig)
	{
		ConfigAppSave();
	}
	DrvExit();
	MediaExit();
}

void bye(void)
{
	printf("Doing exit cleanup\n");

	DrvExit();
	MediaExit();
	BurnLibExit();
	SDL_Quit();
}

static int __cdecl AppDebugPrintf(int nStatus, TCHAR* pszFormat, ...)
{

	va_list args;
	va_start(args, pszFormat);
	printf(pszFormat, args);
	va_end(args);

	return 0;
}

int main(int argc, char* argv[])
{
	const char* romname = NULL;
	UINT32      i = 0;
	bool gamefound = 0;
	atexit(bye);

	// Make version string
	if (nBurnVer & 0xFF)
	{
		// private version (alpha)
		_stprintf(szAppBurnVer, _T("%x.%x.%x.%02x"), nBurnVer >> 20, (nBurnVer >> 16) & 0x0F, (nBurnVer >> 8) & 0xFF, nBurnVer & 0xFF);
	}
	else
	{
		// public version
		_stprintf(szAppBurnVer, _T("%x.%x.%x"), nBurnVer >> 20, (nBurnVer >> 16) & 0x0F, (nBurnVer >> 8) & 0xFF);
	}

	// set default videofiltering
	snprintf(videofiltering, 3, "0");

	printf("FBNeo v%s\n", szAppBurnVer);
	for (int i = 1; i < argc; i++)
	{
		if (*argv[i] != '-' && !gamefound)
		{
			romname = argv[i];
			gamefound = 1;
		}
	}

	parseSwitches(argc, argv);

	if (romname == NULL)
	{
		printf("Usage: %s [-cd] [-joy] [-menu] [-novsync] [-fullscreen] [-dat] [-autosave] [-nearest] [-linear] [-best] <romname>\n", argv[0]);
		printf("Note the -menu switch does not require a romname\n");
		printf("e.g.: %s mslug\n", argv[0]);
		printf("e.g.: %s -menu -joy\n", argv[0]);
		printf("For NeoCD games:\n");
		printf("%s neocdz -cd path/to/ccd/filename.cue (or .ccd)\n", argv[0]);
		printf("Usage is restricted by the license at https://raw.githubusercontent.com/finalburnneo/FBNeo/master/src/license.txt\n");

		if (!usemenu && !dat)
		{
			return 0;
		}
	}

	// Do these bits before override via ConfigAppLoad
	bCheatsAllowed = false;
	nAudDSPModule[0] = 1;
	EnableHiscores = 1;

#ifdef BUILD_SDL
	SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO);

	SDL_WM_SetCaption("FinalBurn Neo", "FinalBurn Neo");
	SDL_ShowCursor(SDL_DISABLE);
#endif

#ifdef BUILD_SDL2
#ifdef _WIN32
	SDL_setenv("SDL_AUDIODRIVER", "directsound", true);        // fix audio for windows
#endif
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_JOYSTICK | SDL_INIT_AUDIO) < 0)
	{
		printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
		return 0;
	}

#endif

#ifdef BUILD_SDL2
	szSDLhiscorePath = SDL_GetPrefPath("fbneo", "hiscore");
	szSDLeepromPath = SDL_GetPrefPath("fbneo", "eeprom");
	szSDLHDDPath = SDL_GetPrefPath("fbneo", "hdd");
	szSDLSamplePath = SDL_GetPrefPath("fbneo", "samples");
	_stprintf(szAppHiscorePath, _T("%s"), szSDLhiscorePath);
	_stprintf(szAppEEPROMPath, _T("%s"), szSDLeepromPath);
	_stprintf(szAppHDDPath, _T("%s"), szSDLHDDPath);
	_stprintf(szAppSamplesPath, _T("%s"), szSDLSamplePath);
#endif

	ConfigAppLoad();
	ComputeGammaLUT();
	bprintf = AppDebugPrintf;
	BurnLibInit();

	// Search for a game now, for use in the menu and loading a games
	if (romname != NULL)
	{
		for (i = 0; i < nBurnDrvCount; i++)
		{
			nBurnDrvActive = i;
			if (strcmp(BurnDrvGetTextA(DRV_NAME), romname) == 0)
			{
				break;
			}
		}
	}

	if (usemenu)
	{
#ifdef BUILD_SDL2
		bool quit = 0;

		while (!quit)
		{
			gui_init();
			int selectedOk = gui_process();
			gui_exit();

			switch (selectedOk)
			{
			case -1:
				BurnLibExit();
				SDL_Quit();
				quit = 1;
				return 0;

			default:
				DoGame(selectedOk);
				break;
			}
		}
#endif
	}
	else if (dat)
	{
		generateDats();
	}
	else
	{
		if (i == nBurnDrvCount)
		{
			printf("%s is not supported by FinalBurn Neo.\n", romname);
			return 1;
		}

		DoGame(i);
	}

	return 0;
}

/* const */ TCHAR* ANSIToTCHAR(const char* pszInString, TCHAR* pszOutString, int nOutSize)
{
#if defined (UNICODE)
	static TCHAR szStringBuffer[1024];

	TCHAR* pszBuffer = pszOutString ? pszOutString : szStringBuffer;
	int    nBufferSize = pszOutString ? nOutSize * 2 : sizeof(szStringBuffer);

	if (MultiByteToWideChar(CP_ACP, 0, pszInString, -1, pszBuffer, nBufferSize))
	{
		return pszBuffer;
	}

	return NULL;
#else
	if (pszOutString)
	{
		_tcscpy(pszOutString, pszInString);
		return pszOutString;
	}

	return (TCHAR*)pszInString;
#endif
}

/* const */ char* TCHARToANSI(const TCHAR* pszInString, char* pszOutString, int nOutSize)
{
#if defined (UNICODE)
	static char szStringBuffer[1024];
	memset(szStringBuffer, 0, sizeof(szStringBuffer));

	char* pszBuffer = pszOutString ? pszOutString : szStringBuffer;
	int   nBufferSize = pszOutString ? nOutSize * 2 : sizeof(szStringBuffer);

	if (WideCharToMultiByte(CP_ACP, 0, pszInString, -1, pszBuffer, nBufferSize, NULL, NULL))
	{
		return pszBuffer;
	}

	return NULL;
#else
	if (pszOutString)
	{
		strcpy(pszOutString, pszInString);
		return pszOutString;
	}

	return (char*)pszInString;
#endif
}

bool AppProcessKeyboardInput()
{
	return true;
}
