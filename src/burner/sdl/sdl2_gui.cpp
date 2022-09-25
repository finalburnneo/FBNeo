#include <SDL_image.h>
#include "burner.h"
#include "sdl2_gui_common.h"
#include "misc_image.h"

extern char videofiltering[3];

// reduce the total number of sets by this number - (isgsm, neogeo, nmk004, pgm, skns, ym2608, coleco, msx_msx, spectrum, spec128, decocass, midssio, cchip, fdsbios, ngp, bubsys)
// don't reduce for these as we display them in the list (neogeo, neocdz)
#define REDUCE_TOTAL_SETS_BIOS		16
#define MAX_STRING_SIZE 300
// Limit CPU usage
#define maxfps 20
static Uint32 starting_stick;

static SDL_Renderer* sdlRenderer = NULL;

static SDL_Texture* titleTexture = NULL;


static int nVidGuiWidth = 1024;
static int nVidGuiHeight = 600;

static int startGame = 0; // game at top of list as it is displayed on the menu
static unsigned int gamesperscreen = 12;
static unsigned int gamesperscreen_halfway = 6;
static unsigned int gametoplay = 0;
static unsigned int halfscreenheight = 0;
static unsigned int halfscreenwidth = 0;
static unsigned int thirdscreenheight = 0;
static unsigned int thirdscreenwidth = 0;
static unsigned int listoffsetY = 0;
static unsigned int listwidthY = 0;

#define JOYSTICK_DEAD_ZONE 8000				// Changed only to be coherent with all other JOYSTICK_DEAD_ZONE declarations
SDL_GameController* gGameController = NULL;
SDL_Joystick *gJoystick = NULL;				// For better compatibility with unmapped game controllers

static SDL_Rect title_texture_rect;
static SDL_Rect dest_title_texture_rect;

static char* gameAv = NULL;
static unsigned int *filterGames = NULL;
static int filterGamesCount = 0;
static int nSystemToCheckMask = HARDWARE_PUBLIC_MASK;
static char systemName[MAX_STRING_SIZE] = { 0 };
static char genre[MAX_STRING_SIZE] = { 0 };
static char searchLetters[27] = {'1','A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z'};
static UINT8 currentLetterCount = 0;

SDL_Texture* LoadTitleImage(SDL_Renderer* renderer, SDL_Texture* loadedTexture)
{

	char titlePath[MAX_PATH] = { 0 };
	int w, h;

	int currentSelected = nBurnDrvActive;
	nBurnDrvActive = gametoplay;
#ifndef _WIN32
	_stprintf(titlePath,_T("%s%s.png"), szAppTitlesPath, BurnDrvGetTextA(0));
#else
	snprintf(titlePath, MAX_PATH, "support\\titles\\%s.png", BurnDrvGetTextA(0));
#endif
	loadedTexture = IMG_LoadTexture(renderer, titlePath);
	SDL_QueryTexture(loadedTexture, NULL, NULL, &w, &h);

	title_texture_rect.x = 0; //the x coordinate
	title_texture_rect.y = 0; // the y coordinate
	title_texture_rect.w = w; //the width of the texture
	title_texture_rect.h = h; //the height of the texture


	dest_title_texture_rect.w = nVidGuiHeight * 1.33;
	dest_title_texture_rect.h = nVidGuiHeight;

	dest_title_texture_rect.x = (nVidGuiWidth - dest_title_texture_rect.w);   //the x coordinate
	dest_title_texture_rect.y = 0; // the y coordinate

	nBurnDrvActive = currentSelected;
	return loadedTexture;
}

static void CreateRomDatName(TCHAR* szRomDat)
{
#if defined(BUILD_SDL2) && !defined(SDL_WINDOWS)
	TCHAR *szSDLconfigPath = NULL;
	szSDLconfigPath = SDL_GetPrefPath("fbneo", "config");
	_stprintf(szRomDat, _T("%sroms.found"), szSDLconfigPath);
	SDL_free(szSDLconfigPath);
#else
	_stprintf(szRomDat, _T("fbneo.dat"));
#endif
	return;
}

int WriteGameAvb()
{
	TCHAR szRomDat[MAX_PATH];
	FILE* h;

	CreateRomDatName(szRomDat);

	if ((h = _tfopen(szRomDat, _T("wt"))) == NULL) {
		return 1;
	}

	_ftprintf(h, _T(APP_TITLE) _T(" v%.20s ROMs"), szAppBurnVer);	// identifier
	_ftprintf(h, _T(" 0x%04X "), nBurnDrvCount);					// no of games

	for (unsigned int i = 0; i < nBurnDrvCount; i++) {
		if (gameAv[i] & 2) {
			_fputtc(_T('*'), h);
		}
		else {
			if (gameAv[i] & 1) {
				_fputtc(_T('+'), h);
			}
			else {
				_fputtc(_T('-'), h);
			}
		}
	}

	_ftprintf(h, _T(" END"));									// end marker

	fclose(h);

	return 0;
}

static bool CheckIfSystem(INT32 gameTocheck)
{
	int currentSelected = nBurnDrvActive;
	nBurnDrvActive = gameTocheck;

	bool bRet = false;

	if ((BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == nSystemToCheckMask)
	{
		bRet = true;
	}

	switch (nSystemToCheckMask)
	{
		case HARDWARE_PREFIX_CAPCOM:
			switch (BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK)
			{
				case HARDWARE_CAPCOM_CPS1:
				case HARDWARE_CAPCOM_CPS1_QSOUND:
				case HARDWARE_CAPCOM_CPS1_GENERIC:
				case HARDWARE_CAPCOM_CPSCHANGER:
				case HARDWARE_CAPCOM_CPS2:
				case HARDWARE_CAPCOM_CPS2_SIMM:
				case HARDWARE_CAPCOM_CPS3:
					bRet = true;
					break;
			}
			break;
		case HARDWARE_PREFIX_SEGA:
			switch (BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK)
			{
				case HARDWARE_SEGA_SYSTEMX:
				case HARDWARE_SEGA_SYSTEMY:
				case HARDWARE_SEGA_SYSTEM16A:
				case HARDWARE_SEGA_SYSTEM16B:
				case HARDWARE_SEGA_SYSTEM16M:
				case HARDWARE_SEGA_SYSTEM18:
				case HARDWARE_SEGA_HANGON:
				case HARDWARE_SEGA_OUTRUN:
				case HARDWARE_SEGA_SYSTEM1:
				case HARDWARE_SEGA_MISC:
				case HARDWARE_SEGA_SYSTEM24:
					bRet = true;
					break;
			}
			break;
		case HARDWARE_PREFIX_KONAMI:
			switch (BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK)
			{
				case HARDWARE_KONAMI_68K_Z80:
				case HARDWARE_KONAMI_68K_ONLY:
					bRet = true;
					break;
			}
			break;

		case HARDWARE_PREFIX_TOAPLAN:
			switch (BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK)
			{
				case HARDWARE_TOAPLAN_RAIZING:
				case HARDWARE_TOAPLAN_68K_Zx80:
				case HARDWARE_TOAPLAN_68K_ONLY:
				case HARDWARE_TOAPLAN_MISC:
					bRet = true;
					break;
			}
			break;
		case HARDWARE_PREFIX_TAITO:
			switch (BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK)
			{
				case HARDWARE_TAITO_TAITOZ:
				case HARDWARE_TAITO_TAITOF2:
				case HARDWARE_TAITO_MISC:
				case HARDWARE_TAITO_TAITOX:
				case HARDWARE_TAITO_TAITOB:
					bRet = true;
					break;
			}
			break;
		case HARDWARE_PREFIX_IREM:
			switch (BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK)
			{
				case HARDWARE_IREM_M62:
				case HARDWARE_IREM_M63:
				case HARDWARE_IREM_M72:
				case HARDWARE_IREM_M90:
				case HARDWARE_IREM_M92:
				case HARDWARE_IREM_MISC:
					bRet = true;
					break;
			}
			break;

		case HARDWARE_PREFIX_KANEKO:
			switch (BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK)
			{
				case HARDWARE_KANEKO16:
				case HARDWARE_KANEKO_MISC:
				case HARDWARE_KANEKO_SKNS:
					bRet = true;
					break;
			}
			break;

		case HARDWARE_PREFIX_SETA:
			switch (BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK)
			{
				case HARDWARE_SETA1:
				case HARDWARE_SETA2:
				case HARDWARE_SETA_SSV:
					bRet = true;
					break;
			}
			break;

		case HARDWARE_PREFIX_MIDWAY:
			switch (BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK)
			{
				case HARDWARE_MIDWAY_KINST:
				case HARDWARE_MIDWAY_TUNIT:
				case HARDWARE_MIDWAY_WUNIT:
				case HARDWARE_MIDWAY_YUNIT:
					bRet = true;
					break;
			}
			break;

		case HARDWARE_PREFIX_NGP:
			switch (BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK)
			{
				case HARDWARE_SNK_NGP:
				case HARDWARE_SNK_NGPC:
					bRet = true;
					break;
			}
			break;

		case HARDWARE_PREFIX_FDS:
			switch (BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK)
			{
				case HARDWARE_FDS:
					bRet = true;
					break;
			}
			break;

		case HARDWARE_PREFIX_CHANNELF:
			switch (BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK)
			{
				case HARDWARE_CHANNELF:
					bRet = true;
					break;
			}
			break;

		case HARDWARE_PUBLIC_MASK:
			bRet = true;
			break;
		default:
			break;
	}

	nBurnDrvActive = currentSelected;
	return bRet;
}

static void DoFilterGames()
{
	int count = 0;
	int currentSelected = nBurnDrvActive;
	if (filterGames!=NULL)
	{
		free(filterGames);
		filterGames = NULL;
	}

	filterGames = (unsigned int*)malloc(nBurnDrvCount * sizeof(unsigned int));
	filterGamesCount = 0;

	if (bShowAvailableOnly)
	{
		for(UINT32 i = 0; i < nBurnDrvCount; i++)
		{
			nBurnDrvActive = i;
			if (gameAv[i] && CheckIfSystem(i) && !(BurnDrvGetGenreFlags() & GBF_BIOS)	)
			{
				if(bShowClones)
				{
					count++;
				}
				else
				{
					if (BurnDrvGetTextA(DRV_PARENT) == NULL)
					{
						count++;
					}
				}

			}
		}

		for(UINT32 i = 0; i < nBurnDrvCount; i++)
		{
			nBurnDrvActive = i;
			if (gameAv[i] && CheckIfSystem(i) && !(BurnDrvGetGenreFlags() & GBF_BIOS)	)
			{
				if(bShowClones)
				{
					filterGames[filterGamesCount] = i;
					filterGamesCount++;
				}
				else
				{
					if(BurnDrvGetTextA(DRV_PARENT) == NULL)
					{
						filterGames[filterGamesCount] = i;
						filterGamesCount++;
					}
				}

			}
		}
	}
	else
	{
		for(UINT32 i = 0; i < nBurnDrvCount; i++)
		{
			nBurnDrvActive = i;
			if (CheckIfSystem(i) && !(BurnDrvGetGenreFlags() & GBF_BIOS)	)
			{
				if(bShowClones)
				{
					filterGames[filterGamesCount] = i;
					filterGamesCount++;
				}
				else
				{
					if(BurnDrvGetTextA(DRV_PARENT) == NULL)
					{
						filterGames[filterGamesCount] = i;
						filterGamesCount++;
					}
				}
			}
		}
	}
	nBurnDrvActive = currentSelected;

}

static void SystemToCheck()
{
	switch(nSystemToCheckMask)
	{
		case HARDWARE_PREFIX_CAPCOM:
			snprintf(systemName, MAX_STRING_SIZE, "CAPCOM CPS1/2/3");
			break;
		case HARDWARE_MISC_PRE90S:
			snprintf(systemName, MAX_STRING_SIZE, "Pre 1990s");
			break;
		case HARDWARE_MISC_POST90S:
			snprintf(systemName, MAX_STRING_SIZE, "Post 1990s");
			break;
		case HARDWARE_PREFIX_MIDWAY:
			snprintf(systemName, MAX_STRING_SIZE, "Midway");
			break;
		case HARDWARE_PREFIX_SEGA:
			snprintf(systemName, MAX_STRING_SIZE, "SEGA");
			break;
		case HARDWARE_PREFIX_KONAMI:
			snprintf(systemName, MAX_STRING_SIZE, "Konami");
			break;
		case HARDWARE_PREFIX_TOAPLAN:
			snprintf(systemName, MAX_STRING_SIZE, "Toaplan");
			break;
		case HARDWARE_SNK_NEOGEO:
			snprintf(systemName, MAX_STRING_SIZE, "SNK NeoGeo");
			break;
		case HARDWARE_PREFIX_NGP:
			snprintf(systemName, MAX_STRING_SIZE, "SNK NeoGeo Pocket");
			break;
		case HARDWARE_PREFIX_CAVE:
			snprintf(systemName, MAX_STRING_SIZE, "CAVE");
			break;
		case HARDWARE_PREFIX_IGS_PGM:
			snprintf(systemName, MAX_STRING_SIZE, "IGS PGM");
			break;
		case HARDWARE_PREFIX_TAITO:
			snprintf(systemName, MAX_STRING_SIZE, "Taito");
			break;
		case HARDWARE_PREFIX_PSIKYO:
			snprintf(systemName, MAX_STRING_SIZE, "Psikyo");
			break;
		case HARDWARE_PREFIX_KANEKO:
			snprintf(systemName, MAX_STRING_SIZE, "Kaneko");
			break;
		case HARDWARE_PREFIX_IREM:
			snprintf(systemName, MAX_STRING_SIZE, "IREM");
			break;
		case HARDWARE_PREFIX_DATAEAST:
			snprintf(systemName, MAX_STRING_SIZE, "Data East");
			break;
		case HARDWARE_PREFIX_SETA:
			snprintf(systemName, MAX_STRING_SIZE, "Seta");
			break;
		case HARDWARE_PREFIX_TECHNOS:
			snprintf(systemName, MAX_STRING_SIZE, "Technos");
			break;
		case HARDWARE_SEGA_MEGADRIVE:
			snprintf(systemName, MAX_STRING_SIZE, "Sega Megadrive / Genesis");
			break;
		case HARDWARE_PCENGINE_PCENGINE:
			snprintf(systemName, MAX_STRING_SIZE, "NEC PC Engine");
			break;
		case HARDWARE_PCENGINE_TG16:
			snprintf(systemName, MAX_STRING_SIZE, "NEC Turbographx 16");
			break;
		case HARDWARE_PCENGINE_SGX:
			snprintf(systemName, MAX_STRING_SIZE, "NEC SGX");
			break;
		case HARDWARE_SEGA_SG1000:
			snprintf(systemName, MAX_STRING_SIZE, "Sega SG-1000");
			break;
		case HARDWARE_COLECO:
			snprintf(systemName, MAX_STRING_SIZE, "ColecoVision");
			break;
		case HARDWARE_SEGA_MASTER_SYSTEM:
			snprintf(systemName, MAX_STRING_SIZE, "Sega Master System");
			break;
		case HARDWARE_SEGA_GAME_GEAR:
			snprintf(systemName, MAX_STRING_SIZE, "Sega Game Gear");
			break;
		case HARDWARE_MSX:
			snprintf(systemName, MAX_STRING_SIZE, "MSX");
			break;
		case HARDWARE_SPECTRUM:
			snprintf(systemName, MAX_STRING_SIZE, "Sinclar Spectrum");
			break;
		case HARDWARE_NES:
			snprintf(systemName, MAX_STRING_SIZE, "Nintendo Entertainment System / Famicom");
			break;
		case HARDWARE_FDS:
			snprintf(systemName, MAX_STRING_SIZE, "Nintendo Famicom Disk System");
			break;
		case HARDWARE_PREFIX_CHANNELF:
			snprintf(systemName, MAX_STRING_SIZE, "Fairchild Channel F");
			break;
		default:
			snprintf(systemName, MAX_STRING_SIZE, "Everything");
			break;
	}
}

static void SwapSystemToCheck()
{
	startGame = -gamesperscreen_halfway;
	switch(nSystemToCheckMask)
	{
		case HARDWARE_PUBLIC_MASK:
			nSystemToCheckMask = HARDWARE_PREFIX_CAPCOM;
			break;
		case HARDWARE_PREFIX_CAPCOM:
			nSystemToCheckMask = HARDWARE_PREFIX_CAVE;
			break;
		case HARDWARE_PREFIX_CAVE:
			nSystemToCheckMask = HARDWARE_MISC_PRE90S;
			break;
		case HARDWARE_MISC_PRE90S:
			nSystemToCheckMask = HARDWARE_MISC_POST90S;
			break;
		case HARDWARE_MISC_POST90S:
			nSystemToCheckMask = HARDWARE_PREFIX_MIDWAY;
			break;
		case HARDWARE_PREFIX_MIDWAY:
			nSystemToCheckMask = HARDWARE_PREFIX_SEGA;
			break;
		case HARDWARE_PREFIX_SEGA:
			nSystemToCheckMask = HARDWARE_PREFIX_KONAMI;
			break;
		case HARDWARE_PREFIX_KONAMI:
			nSystemToCheckMask = HARDWARE_PREFIX_TOAPLAN;
			break;
		case HARDWARE_PREFIX_TOAPLAN:
			nSystemToCheckMask = HARDWARE_SNK_NEOGEO;
			break;
		case HARDWARE_SNK_NEOGEO:
			nSystemToCheckMask = HARDWARE_PREFIX_NGP;
			break;
		case HARDWARE_PREFIX_NGP:
			nSystemToCheckMask = HARDWARE_PREFIX_IGS_PGM;
			break;
		case HARDWARE_PREFIX_IGS_PGM:
			nSystemToCheckMask = HARDWARE_PREFIX_CHANNELF;
			break;
		case HARDWARE_PREFIX_CHANNELF:
			nSystemToCheckMask = HARDWARE_PREFIX_TAITO;
			break;
		case HARDWARE_PREFIX_TAITO:
			nSystemToCheckMask = HARDWARE_PREFIX_PSIKYO;
			break;
		case HARDWARE_PREFIX_PSIKYO:
			nSystemToCheckMask = HARDWARE_PREFIX_KANEKO;
			break;
		case HARDWARE_PREFIX_KANEKO:
			nSystemToCheckMask = HARDWARE_PREFIX_IREM;
			break;
		case HARDWARE_PREFIX_IREM:
			nSystemToCheckMask = HARDWARE_PREFIX_DATAEAST;
			break;
		case HARDWARE_PREFIX_DATAEAST:
			nSystemToCheckMask = HARDWARE_PREFIX_SETA;
			break;
		case HARDWARE_PREFIX_SETA:
			nSystemToCheckMask = HARDWARE_PREFIX_TECHNOS;
			break;
		case HARDWARE_PREFIX_TECHNOS:
			nSystemToCheckMask = HARDWARE_SEGA_MEGADRIVE;
			break;
		case HARDWARE_SEGA_MEGADRIVE:
			nSystemToCheckMask = HARDWARE_PCENGINE_PCENGINE;
			break;
		case HARDWARE_PCENGINE_PCENGINE:
			nSystemToCheckMask = HARDWARE_PCENGINE_TG16;
			break;
		case HARDWARE_PCENGINE_TG16:
			nSystemToCheckMask = HARDWARE_PCENGINE_SGX;
			break;
		case HARDWARE_PCENGINE_SGX:
			nSystemToCheckMask = HARDWARE_SEGA_SG1000;
			break;
		case HARDWARE_SEGA_SG1000:
			nSystemToCheckMask = HARDWARE_COLECO;
			break;
		case HARDWARE_COLECO:
			nSystemToCheckMask = HARDWARE_SEGA_MASTER_SYSTEM;
			break;
		case HARDWARE_SEGA_MASTER_SYSTEM:
			nSystemToCheckMask = HARDWARE_SEGA_GAME_GEAR;
			break;
		case HARDWARE_SEGA_GAME_GEAR:
			nSystemToCheckMask = HARDWARE_MSX;
			break;
		case HARDWARE_MSX:
			nSystemToCheckMask = HARDWARE_SPECTRUM;
			break;
		case HARDWARE_SPECTRUM:
			nSystemToCheckMask = HARDWARE_NES;
			break;
		case HARDWARE_NES:
			nSystemToCheckMask = HARDWARE_FDS;
			break;
		default:
			nSystemToCheckMask = HARDWARE_PUBLIC_MASK;
			break;
	}
	SystemToCheck();
	nFilterSelect=nSystemToCheckMask;
	DoFilterGames();
}


void findNextLetter()
{
	int currentSelected = nBurnDrvActive;
	bool found = false;

	currentLetterCount++;
	if (currentLetterCount >= 27)
	{
		currentLetterCount = 0;
	}
	char letterToFind = searchLetters[currentLetterCount];

	char checkChar;

	for (int i = 0; i < filterGamesCount; i++)
	{
		nBurnDrvActive = filterGames[i];
		checkChar = BurnDrvGetTextA(DRV_FULLNAME)[0];
		if (!found && (checkChar == letterToFind))
		{
			found = true;
			startGame = i;
			startGame -= gamesperscreen_halfway;
		}
	}

	nBurnDrvActive = currentSelected;
}


void findPrevLetter()
{
	int currentSelected = nBurnDrvActive;
	bool found = false;

	if (currentLetterCount == 0)
	{
		currentLetterCount = 27;
	}
	currentLetterCount--;

	char letterToFind = searchLetters[currentLetterCount];

	char checkChar;

	for (int i = 0; i < filterGamesCount; i++)
	{
		nBurnDrvActive = filterGames[i];
		checkChar = BurnDrvGetTextA(DRV_FULLNAME)[0];
		if (!found && (checkChar == letterToFind))
		{
			found = true;
			startGame = i;
			startGame -= gamesperscreen_halfway;
		}
	}

	nBurnDrvActive = currentSelected;
}



static int DoCheck(TCHAR* buffPos)
{
	TCHAR label[256];

	// Check identifier
	memset(label, 0, sizeof(label));
	_stprintf(label, _T(APP_TITLE) _T(" v%.20s ROMs"), szAppBurnVer);
	if ((buffPos = LabelCheck(buffPos, label)) == NULL) {
		return 1;
	}

	// Check no of supported games
	memset(label, 0, sizeof(label));
	memcpy(label, buffPos, 16);
	buffPos += 8;
	unsigned int n = _tcstol(label, NULL, 0);
	if (n != nBurnDrvCount) {
		return 1;
	}

	for (unsigned int i = 0; i < nBurnDrvCount; i++) {
		if (*buffPos == _T('*')) {
			gameAv[i] = 3;
		}
		else {
			if (*buffPos == _T('+')) {
				gameAv[i] = 1;
			}
			else {
				if (*buffPos == _T('-')) {
					gameAv[i] = 0;
				}
				else {
					return 1;
				}
			}
		}

		buffPos++;
	}

	memset(label, 0, sizeof(label));
	_stprintf(label, _T(" END"));
	if (LabelCheck(buffPos, label) == NULL) {
		return 0;
	}
	else {
		return 1;
	}
}


int CheckGameAvb()
{
	TCHAR szRomDat[MAX_PATH];
	FILE* h;
	int bOK;
	int nBufferSize = nBurnDrvCount + 256;
	TCHAR* buffer = (TCHAR*)malloc(nBufferSize * sizeof(TCHAR));
	if (buffer == NULL) {
		return 1;
	}

	memset(buffer, 0, nBufferSize * sizeof(TCHAR));
	CreateRomDatName(szRomDat);

	if ((h = _tfopen(szRomDat, _T("r"))) == NULL) {
		if (buffer)
		{
			free(buffer);
			buffer = NULL;
		}
		return 1;
	}

	_fgetts(buffer, nBufferSize, h);
	fclose(h);

	bOK = DoCheck(buffer);

	if (buffer) {
		free(buffer);
		buffer = NULL;
	}

	DoFilterGames();

	return bOK;
}

void reset_filters()
{
	// Reset Filters
	bShowClones = true;
	bShowAvailableOnly = true;
	nSystemToCheckMask = HARDWARE_PUBLIC_MASK;
 	nFilterSelect = HARDWARE_PUBLIC_MASK;

	startGame = -gamesperscreen_halfway;
}

//TODO: multithread the rendering...
void RefreshRomList(bool force_rescan)
{
	UINT32 tempgame;
	SDL_Event e;
	float screenpercentage = nVidGuiWidth / 100;

	SDL_Rect fillRect = { 0, 70, 0, 70 };

	tempgame = nBurnDrvActive;
	nBurnDrvActive = 0;
	filterGamesCount = 0;
	if (!CheckGameAvb() && !force_rescan)
	{
		return;
	}

	for (UINT32 i = 0; i < nBurnDrvCount; i++)
	{
		nBurnDrvActive = i;
		switch (BzipOpen(true))
		{
		case 0:
			gameAv[i] = 3;
			break;
		case 2:
			gameAv[i] = 1;
			break;
		case 1:
			gameAv[i] = 0;
			break;
		}
		BzipClose();

		if (i % 200 == 0)
		{
			SDL_SetRenderDrawColor(sdlRenderer, 0x1a, 0x1e, 0x1d, SDL_ALPHA_OPAQUE);
			SDL_RenderClear(sdlRenderer);

			// draw a progress bar
			float x = (i * 100) / nBurnDrvCount;
			fillRect = (SDL_Rect){ 0, 80, (int)(x * screenpercentage), 70 };

			SDL_SetRenderDrawColor(sdlRenderer, 0, 0xb3, 0x3b, 0xFF);
			SDL_RenderFillRect(sdlRenderer, &fillRect);

			incolor(fbn_color, /* unused */ 0);
			inprint(sdlRenderer, "FinalBurn Neo", 10, 10);
			inprint(sdlRenderer, "=============", 10, 20);

			inprint(sdlRenderer, "Scanning for ROM:", 10, 40);
			inprint(sdlRenderer, BurnDrvGetTextA(DRV_FULLNAME), 10, 50);

			incolor(unavailable_color, /* unused */ 0);
			char newLine[MAX_STRING_SIZE];
			snprintf(newLine, MAX_STRING_SIZE, "%d", (int)(x));
			inprint(sdlRenderer, newLine, (int)((x * screenpercentage) - 30), 110);
			// TODO : Progress %

			SDL_RenderPresent(sdlRenderer);
			SDL_PollEvent(&e); // poll some events so OS doesn't think it's crashed
		}
	}
	WriteGameAvb();
	nBurnDrvActive = tempgame;
	reset_filters(); // reset filterss after a full rescan
	DoFilterGames();
}

void gui_exit()
{
	if (gGameController!=NULL) {
		SDL_GameControllerClose( gGameController );
		gGameController = NULL;
	}
	if (gJoystick!=NULL) {
		SDL_JoystickClose( gJoystick );
		gJoystick = NULL;
	}
	if (filterGames!=NULL)
	{
		free(filterGames);
		filterGames = NULL;
	}
	kill_inline_font();
	if (titleTexture != NULL) {
		SDL_DestroyTexture(titleTexture);
	}
	SDL_DestroyRenderer(sdlRenderer);
	SDL_DestroyWindow(sdlWindow);
	free(gameAv);
}

void gui_init()
{

	gameAv = (char*)malloc(nBurnDrvCount);
	memset(gameAv, 0, nBurnDrvCount);

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) < 0)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s", SDL_GetError());
		return;
	}

	nJoystickCount = SDL_NumJoysticks();
	if( nJoystickCount < 1 )
	{
		printf( "Warning: No joysticks connected!\n" );
	}
	else
	{
		for (int i = 0; i < nJoystickCount; ++i) {
			gJoystick = SDL_JoystickOpen(i);
			if (gJoystick) {
				if (SDL_IsGameController(i)) {
					gGameController = SDL_GameControllerOpen(i);
					if (gGameController) {
						printf("Found a game controller: %s\n", SDL_GameControllerName(gGameController));
					}
				} else {
					// Even if not mapped, a game controller can be used as joystick
					printf("Found a joystick: %s\n", SDL_JoystickName(gJoystick));
				}
				break;
			} else {
				printf("Could not open joystick %i: %s\n", i, SDL_GetError());
			}
		}
	}

	Uint32 screenFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;

	if (bAppFullscreen)
	{
		SDL_DisplayMode dm;
		if (SDL_GetDesktopDisplayMode(0, &dm) != 0)
		{
			SDL_Log("SDL_GetDesktopDisplayMode failed: %s", SDL_GetError());
			return;
		}

		nVidGuiWidth = dm.w;
		nVidGuiHeight = dm.h;
		screenFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN_DESKTOP;
	}


	sdlWindow = SDL_CreateWindow(
		"FBNeo - Select Game...",
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		nVidGuiWidth,
		nVidGuiHeight,
		screenFlags
	);

	// Check that the window was successfully created
	if (sdlWindow == NULL)
	{
		// In the case that the window could not be made...
		printf("Could not create window: %s\n", SDL_GetError());
		return;
	}

	// TODO: I guess it make sense to always vsync on the menu??
	sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (sdlRenderer == NULL)
	{
		// failed back to SOFTWARE renderer
		sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, SDL_RENDERER_SOFTWARE);
		if (sdlRenderer == NULL)
		{
			// In the case that the window could not be made...
			printf("Could not create renderer: %s\n", SDL_GetError());
			return;
		}
	}
	if (bIntegerScale)
	{
		SDL_RenderSetIntegerScale(sdlRenderer, SDL_TRUE);
	}
	inrenderer(sdlRenderer);
	prepare_inline_font();



	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, videofiltering);
	SDL_RenderSetLogicalSize(sdlRenderer, nVidGuiWidth, nVidGuiHeight);


	halfscreenheight = nVidGuiHeight / 2;
	halfscreenwidth = nVidGuiWidth / 2;
	thirdscreenheight =nVidGuiHeight/ 3;
	thirdscreenwidth = nVidGuiWidth / 3;

	//gamesperscreen = (thirdscreenheight * 2) / 11;
	gamesperscreen = (nVidGuiHeight-55) / 11;
	gamesperscreen_halfway = gamesperscreen / 2;

	listoffsetY = 0;
	listwidthY = thirdscreenwidth * 2;

	RefreshRomList(false);

	nSystemToCheckMask = nFilterSelect;
	nBurnDrvActive = nGameSelect;

	SystemToCheck();

	// assume if the filter list exists we are returning from a launched game.
	if (filterGamesCount > 0)
	{
		startGame = gameSelectedFromFilter - gamesperscreen_halfway;
	}
	else
	{
		startGame = nBurnDrvActive - gamesperscreen_halfway;
	}

	DoFilterGames();
}

void getGenre()
{
	if (BurnDrvGetGenreFlags() & GBF_HORSHOOT)			snprintf(genre, MAX_STRING_SIZE, "Shooter / Horizontal / Sh'mup");
	if (BurnDrvGetGenreFlags() & GBF_VERSHOOT)			snprintf(genre, MAX_STRING_SIZE, "Shooter / Vertical / Sh'mup");
	if (BurnDrvGetGenreFlags() & GBF_SCRFIGHT)			snprintf(genre, MAX_STRING_SIZE, "Fighting / Beat 'em Up");
	if (BurnDrvGetGenreFlags() & GBF_VSFIGHT)			snprintf(genre, MAX_STRING_SIZE, "Fighting / Versus");
	if (BurnDrvGetGenreFlags() & GBF_BIOS)				snprintf(genre, MAX_STRING_SIZE, "BIOS");
	if (BurnDrvGetGenreFlags() & GBF_BREAKOUT)			snprintf(genre, MAX_STRING_SIZE, "Breakout");
	if (BurnDrvGetGenreFlags() & GBF_CASINO)			snprintf(genre, MAX_STRING_SIZE, "Casino");
	if (BurnDrvGetGenreFlags() & GBF_BALLPADDLE)		snprintf(genre, MAX_STRING_SIZE, "Ball & Paddle");
	if (BurnDrvGetGenreFlags() & GBF_MAZE)				snprintf(genre, MAX_STRING_SIZE, "Maze");
	if (BurnDrvGetGenreFlags() & GBF_MINIGAMES)			snprintf(genre, MAX_STRING_SIZE, "Mini-Games");
	if (BurnDrvGetGenreFlags() & GBF_PINBALL)			snprintf(genre, MAX_STRING_SIZE, "Pinball");
	if (BurnDrvGetGenreFlags() & GBF_PLATFORM)			snprintf(genre, MAX_STRING_SIZE, "Platformer");
	if (BurnDrvGetGenreFlags() & GBF_PUZZLE)			snprintf(genre, MAX_STRING_SIZE, "Puzzle");
	if (BurnDrvGetGenreFlags() & GBF_QUIZ)				snprintf(genre, MAX_STRING_SIZE, "Quiz");
	if (BurnDrvGetGenreFlags() & GBF_SPORTSMISC)		snprintf(genre, MAX_STRING_SIZE, "Sports");
	if (BurnDrvGetGenreFlags() & GBF_SPORTSFOOTBALL)	snprintf(genre, MAX_STRING_SIZE, "Sports / Football");
	if (BurnDrvGetGenreFlags() & GBF_MISC)				snprintf(genre, MAX_STRING_SIZE, "Misc");
	if (BurnDrvGetGenreFlags() & GBF_MAHJONG)			snprintf(genre, MAX_STRING_SIZE, "Mahjong");
	if (BurnDrvGetGenreFlags() & GBF_RACING)			snprintf(genre, MAX_STRING_SIZE, "Racing");
	if (BurnDrvGetGenreFlags() & GBF_SHOOT)				snprintf(genre, MAX_STRING_SIZE, "Shooter");
	if (BurnDrvGetGenreFlags() & GBF_ACTION)			snprintf(genre, MAX_STRING_SIZE, "Run 'n Gun (Shooter)");
	if (BurnDrvGetGenreFlags() & GBF_RUNGUN)			snprintf(genre, MAX_STRING_SIZE, "Strategy");
	if (BurnDrvGetGenreFlags() & GBF_STRATEGY)			snprintf(genre, MAX_STRING_SIZE, "Action (Classic)");
	if (BurnDrvGetGenreFlags() & GBF_RPG)				snprintf(genre, MAX_STRING_SIZE, "RPG");
	if (BurnDrvGetGenreFlags() & GBF_SIM)				snprintf(genre, MAX_STRING_SIZE, "Simulator");
	if (BurnDrvGetGenreFlags() & GBF_ADV)				snprintf(genre, MAX_STRING_SIZE, "Adventure");
}

void gui_render()
{
	char newLine[MAX_STRING_SIZE];

	SDL_SetRenderDrawColor(sdlRenderer, 0x1a, 0x1e, 0x1d, SDL_ALPHA_OPAQUE);
	SDL_RenderClear(sdlRenderer);
	if (titleTexture != NULL) // JUST FOR TESTING!!
	{
		SDL_RenderCopy(sdlRenderer, titleTexture, &title_texture_rect, &dest_title_texture_rect);
	}

	// header
	renderPanel(sdlRenderer, 0,  0, nVidGuiWidth, 28,  0x00, 0x00, 0x00);

	// Game List
//	renderPanel(sdlRenderer, listoffsetY, 28, listwidthY, (thirdscreenheight*2)-28,  0x40, 0x20, 0x0b);
	renderPanel(sdlRenderer, listoffsetY, 28, listwidthY, nVidGuiHeight-28-65,  0x20, 0x10, 0x00);


	// Selected game background
	renderPanel(sdlRenderer, 0,  28 + (gamesperscreen_halfway * 10), nVidGuiWidth, 12,  0x41, 0x1d, 0x62);

	// game info
	renderPanel(sdlRenderer,  0, nVidGuiHeight - 65, nVidGuiWidth, nVidGuiHeight,  0x41, 0x1d, 0xf2);

	incolor(fbn_color, /* unused */ 0);
	inprint(sdlRenderer, "FB Neo *** A/ENTER: Start game  B/F5: Reset Filters  X/F4: Filter Clones  Y/F2: Filter Missing  COIN/F3: System Filter  START/F1: Rescan ROMs  F12: Quit ***", 10, 5);
	if (strlen(systemName) != 0) {
		snprintf(newLine, MAX_STRING_SIZE, "Filter System: %s / Missing: %s / Clones: %s / Showing : %d of %d", systemName, (bShowAvailableOnly?"No":"Yes"), (bShowClones?"Yes":"No"), filterGamesCount, (nBurnDrvCount + 1 - REDUCE_TOTAL_SETS_BIOS));
		inprint(sdlRenderer, newLine, 10, 15);
	}
	incolor(normal_color, /* unused */ 0);
	for (unsigned int i = startGame, game_counter = 0; game_counter < gamesperscreen; i++, game_counter++)
	{
		if (i >= 0 && i < filterGamesCount)
		{
			nBurnDrvActive = filterGames[i];
			if (game_counter == gamesperscreen_halfway)
			{

				calcSelectedItemColor();
				//incolor(select_color, /* unused */ 0);

				inprint_shadowed(sdlRenderer, BurnDrvGetTextA(DRV_FULLNAME), listoffsetY, 30 + (gamesperscreen_halfway * 10));
				gametoplay = filterGames[i];
				gameSelectedFromFilter = i;

				incolor(info_color, /* unused */ 0);

				snprintf(newLine, MAX_STRING_SIZE, "Game: %s",  BurnDrvGetTextA(DRV_FULLNAME));
				inprint_shadowed(sdlRenderer, newLine, listoffsetY, nVidGuiHeight - 60);

				if (!BurnDrvGetTextA(DRV_PARENT)) {
					snprintf(newLine, MAX_STRING_SIZE, "Rom: %s", BurnDrvGetTextA(DRV_NAME));
				}else{
					snprintf(newLine, MAX_STRING_SIZE, "Rom: %s (Clone: %s)", BurnDrvGetTextA(DRV_NAME), BurnDrvGetTextA(DRV_PARENT));
				}
				inprint_shadowed(sdlRenderer, newLine, listoffsetY, nVidGuiHeight - 50);

				if (BurnDrvGetMaxPlayers() == 1) {
					snprintf(newLine, MAX_STRING_SIZE, "Info: 1 Player");
				}else{
					snprintf(newLine, MAX_STRING_SIZE, "Info: %d Players Max", BurnDrvGetMaxPlayers());
				}
				inprint_shadowed(sdlRenderer, newLine, listoffsetY, nVidGuiHeight - 40);

				snprintf(newLine, MAX_STRING_SIZE, "Release: %s (%s, %s Hardware)", BurnDrvGetTextA(DRV_MANUFACTURER), BurnDrvGetTextA(DRV_DATE), BurnDrvGetTextA(DRV_SYSTEM));
				inprint_shadowed(sdlRenderer, newLine, listoffsetY, nVidGuiHeight - 30);

				getGenre();
				snprintf(newLine, MAX_STRING_SIZE, "Genre: %s", genre);
				inprint_shadowed(sdlRenderer, newLine, listoffsetY, nVidGuiHeight - 20);

				if (BurnDrvGetTextA(DRV_COMMENT)) {
					snprintf(newLine, MAX_STRING_SIZE, "Note: %s", BurnDrvGetTextA(DRV_COMMENT));
					inprint_shadowed(sdlRenderer, newLine, listoffsetY, nVidGuiHeight - 10);
				}

			}
			else
			{
				if (!gameAv[nBurnDrvActive])
				{
					incolor(unavailable_color, /* unused */ 0);
				}
				else if (BurnDrvGetTextA(DRV_PARENT) == NULL)
				{
					incolor(normal_color_parent, /* unused */ 0);
				}
				else
				{
					incolor(normal_color, /* unused */ 0);
				}
				inprint(sdlRenderer, BurnDrvGetTextA(DRV_FULLNAME), listoffsetY, 30 + (game_counter * 10));
			}
		}
	}

	SDL_RenderPresent(sdlRenderer);
}

int gui_process()
{
	SDL_Event e;
	bool quit = false;

	static UINT32 previousSelected = 0;

	while (!quit)
	{
		starting_stick = SDL_GetTicks();
		SDL_JoystickEventState(SDL_ENABLE);
		SDL_GameControllerEventState(SDL_ENABLE);

		while (SDL_PollEvent(&e))
		{
			switch (e.type)
			{
				case SDL_WINDOWEVENT:
					switch (e.window.event)
					{
						case SDL_WINDOWEVENT_SIZE_CHANGED:
							nVidGuiWidth = e.window.data1;
							nVidGuiHeight = e.window.data2;

							SDL_RenderSetLogicalSize(sdlRenderer, nVidGuiWidth, nVidGuiHeight);

							halfscreenheight = nVidGuiHeight / 2;
							halfscreenwidth = nVidGuiWidth / 2;
							thirdscreenheight =nVidGuiHeight/ 3;
							thirdscreenwidth = nVidGuiWidth / 3;

							//gamesperscreen = (thirdscreenheight * 2) / 11;
							gamesperscreen = (nVidGuiHeight-55) / 11;
							gamesperscreen_halfway = gamesperscreen / 2;

							listoffsetY = 0;
							listwidthY = thirdscreenwidth * 2;								
							break;
					}
					break;
				case SDL_CONTROLLERDEVICEREMOVED:
				case SDL_JOYDEVICEREMOVED:
					if (!SDL_GameControllerGetAttached(gGameController) && !SDL_JoystickGetAttached(gJoystick)) {
						if (gGameController!=NULL) {
							SDL_GameControllerClose( gGameController );
							gGameController = NULL;
						}
						if (gJoystick!=NULL) {
							SDL_JoystickClose( gJoystick );
							gJoystick = NULL;
						}
						for (int i = 0; i < SDL_NumJoysticks(); ++i) {
							gJoystick = SDL_JoystickOpen(i);
							if (gJoystick) {
								if (SDL_IsGameController(i)) {
									gGameController = SDL_GameControllerOpen(i);
									if (gGameController) {
										printf("Found a game controller: %s\n", SDL_GameControllerName(gGameController));
									}
								} else {
									printf("Found a joystick: %s\n", SDL_JoystickName(gJoystick));
								}
								break;
							} else {
								printf("Could not open joystick %i: %s\n", i, SDL_GetError());
							}
						}
					}
					break;
				case SDL_JOYAXISMOTION:				// Using this instead of CONTROLLERAXIS for compatibility with unmapped controllers
					switch (e.jaxis.axis)
					{
						case 1:
							if (e.jaxis.value < -JOYSTICK_DEAD_ZONE)
								startGame--;
							else if (e.jaxis.value > JOYSTICK_DEAD_ZONE)
								startGame++;
							break;
						case 0:
							if (e.jaxis.value < -JOYSTICK_DEAD_ZONE)
								startGame -= 10;
							else if (e.jaxis.value > JOYSTICK_DEAD_ZONE)
								startGame += 10;
							break;
					}
					break;
				case SDL_JOYHATMOTION:				// Using this instead of DPAD for compatibility with unmapped controllers
					switch (e.jhat.value)
					{
						case SDL_HAT_UP:
							startGame--;
							break;
						case SDL_HAT_DOWN:
							startGame++;
							break;
						case SDL_HAT_LEFT:
							startGame -= 10;
							break;
						case SDL_HAT_RIGHT:
							startGame += 10;
							break;
					}
					break;
				case SDL_JOYBUTTONDOWN:
					if (gGameController == NULL) {	// Don't use JOYBUTTON if game controller is mapped
						previousSelected = -1;
						nBurnDrvActive = gametoplay;
						if (gameAv[nBurnDrvActive])
						{
							SDL_GameControllerEventState(SDL_IGNORE);
							SDL_JoystickEventState(SDL_IGNORE);
							return gametoplay;
						}
					}
					break;
				case SDL_CONTROLLERBUTTONDOWN:
					switch (e.cbutton.button)
					{
						case SDL_CONTROLLER_BUTTON_START:
							RefreshRomList(true);
							break;
						case SDL_CONTROLLER_BUTTON_BACK:
							SwapSystemToCheck();
							break;
						case SDL_CONTROLLER_BUTTON_X:
							bShowClones = !bShowClones;
							DoFilterGames();
							break;
						case SDL_CONTROLLER_BUTTON_Y:
							bShowAvailableOnly = !bShowAvailableOnly;
							DoFilterGames();
							break;
						case SDL_CONTROLLER_BUTTON_B:
							reset_filters();
							SystemToCheck();
							DoFilterGames();
							break;
						case SDL_CONTROLLER_BUTTON_A:
							previousSelected = -1;
							nBurnDrvActive = gametoplay;
							if (gameAv[nBurnDrvActive])
							{
								SDL_GameControllerEventState(SDL_IGNORE);
								SDL_JoystickEventState(SDL_IGNORE);
								return gametoplay;
							}
							break;
						case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
							findPrevLetter();
							break;
						case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
							findNextLetter();
							break;
						case SDL_CONTROLLER_BUTTON_LEFTSTICK:
							startGame = -gamesperscreen_halfway;
							break;
						case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
							startGame = filterGamesCount;
							break;
					}
					break;
				case SDL_QUIT:
					quit = true;
					break;
				case SDL_MOUSEWHEEL:
					if (e.wheel.y > 0) // scroll up
						startGame--;
					else if (e.wheel.y < 0) // scroll down
						startGame++;
					break;
				case SDL_MOUSEBUTTONDOWN:
					switch (e.button.button)
					{
						case SDL_BUTTON_LEFT:
							previousSelected = 0;
							nBurnDrvActive = gametoplay;
							if (gameAv[nBurnDrvActive])
							{
								SDL_GameControllerEventState(SDL_IGNORE);
								SDL_JoystickEventState(SDL_IGNORE);
								return gametoplay;
							}
							break;
						case SDL_BUTTON_RIGHT:
							quit = 1;
							break;
					}
					break;
				case SDL_KEYDOWN:
					switch (e.key.keysym.sym)
					{
						case SDLK_UP:
							startGame--;
							break;
						case SDLK_DOWN:
							startGame++;
							break;
						case SDLK_HOME:
							startGame = -gamesperscreen_halfway;
							break;
						case SDLK_END:
							startGame = filterGamesCount;
							break;
						case SDLK_PAGEUP:
							startGame -= gamesperscreen_halfway;
							break;
						case SDLK_PAGEDOWN:
							startGame += gamesperscreen_halfway;
							break;
						case SDLK_LEFT:
							startGame -= 10;
							break;
						case SDLK_RIGHT:
							startGame += 10;
							break;
						case SDLK_w:
							findNextLetter();
							break;
						case SDLK_q:
							findPrevLetter();
							break;
						case SDLK_KP_ENTER:
						case SDLK_RETURN:
							if (e.key.keysym.mod & KMOD_ALT)
								SetFullscreen(!GetFullscreen());
							else {
								nBurnDrvActive = gametoplay;
								previousSelected = 0;
								if (gameAv[nBurnDrvActive])
								{
									SDL_GameControllerEventState(SDL_IGNORE);
									SDL_JoystickEventState(SDL_IGNORE);
									return gametoplay;
								}
							}
							break;
						case SDLK_F1:
							RefreshRomList(true);
							break;
						case SDLK_F2:
							bShowAvailableOnly = !bShowAvailableOnly;
							DoFilterGames();
							break;
						case SDLK_F3:
							SwapSystemToCheck();
							break;
						case SDLK_F4:
							bShowClones = !bShowClones;
							DoFilterGames();
							break;
						case SDLK_F5:
							reset_filters();
							SystemToCheck();
							DoFilterGames();
							break;
						case SDLK_F12:
							quit = 1;
							break;
					}
					break;
			}
		}

		// TODO: Need to put more clamping logic here....
		if (startGame < -(int)gamesperscreen_halfway)
		{
			startGame = -gamesperscreen_halfway;
		}

		if (startGame > (int)filterGamesCount - (int)gamesperscreen_halfway - 1)
		{
			startGame = filterGamesCount - gamesperscreen_halfway - 1;
		}

		if (previousSelected != gametoplay || previousSelected == 0)
		{
			if (titleTexture != NULL) SDL_DestroyTexture(titleTexture);
			titleTexture = LoadTitleImage(sdlRenderer, titleTexture);
			nGameSelect = nBurnDrvActive;
		}

		previousSelected = gametoplay;

		gui_render();

		if ( ( 1000 / maxfps ) > SDL_GetTicks() - starting_stick) {
			SDL_Delay( 1000 / maxfps - ( SDL_GetTicks() - starting_stick ) );
		}
	}

	// save config (game and filters selection)
	ConfigAppSave();

	return -1;
}
