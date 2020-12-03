#include <SDL_image.h>
#include "burner.h"
#include "sdl2_gui_common.h"
#include "misc_image.h"

extern char videofiltering[3];

static SDL_Renderer* sdlRenderer = NULL;

static SDL_Texture* titleTexture = NULL;
static SDL_Surface *miscImage=NULL;

static int nVidGuiWidth = 800;
static int nVidGuiHeight = 600;

static int startGame = 0; // game at top of list as it is displayed on the menu
static unsigned int gamesperscreen = 12;
static unsigned int gamesperscreen_halfway = 6;
static unsigned int gametoplay = 0;
static unsigned int halfscreenheight = 0;
static unsigned int halfscreenwidth = 0;
static unsigned int thirdscreenheight =0;
static unsigned int thirdscreenwidth =0;
static unsigned int listoffsetY =0;
static unsigned int listwidthY =0;


const int JOYSTICK_DEAD_ZONE = 8000;
SDL_GameController* gGameController = NULL;

static SDL_Rect title_texture_rect;
static SDL_Rect dest_title_texture_rect;

static char* gameAv = NULL;
static unsigned int *filterGames= NULL;
static int filterGamesCount = 0;
static bool bShowAvailableOnly = true;
static bool bShowClones = true;
static int nSystemToCheckMask = HARDWARE_PUBLIC_MASK;
static char systemName[MAX_PATH] = { 0 };
static int gameSelectedFromFilter = -1;
static char searchLetters[27] = {'1','A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z'};
static UINT8 currentLetterCount = 0;



SDL_Texture* LoadTitleImage(SDL_Renderer* renderer, SDL_Texture* loadedTexture)
{
	char titlePath[MAX_PATH] = { 0 };
	int w, h;

	int currentSelected = nBurnDrvActive;
	nBurnDrvActive = gametoplay;
#ifndef _WIN32
	snprintf(titlePath, MAX_PATH, "%s%s.png", "/usr/local/share/titles/", BurnDrvGetTextA(0));
#else
	snprintf(titlePath, MAX_PATH, "support\\titles\\%s.png", BurnDrvGetTextA(0));
#endif
	loadedTexture = IMG_LoadTexture(renderer, titlePath);
	SDL_QueryTexture(loadedTexture, NULL, NULL, &w, &h);

	title_texture_rect.x = 0; //the x coordinate
	title_texture_rect.y = 0; // the y coordinate
	title_texture_rect.w = w; //the width of the texture
	title_texture_rect.h = h; //the height of the texture

	dest_title_texture_rect.x = 0; //the x coordinate
	dest_title_texture_rect.y = 0; // the y coordinate
	dest_title_texture_rect.w = nVidGuiWidth;
	dest_title_texture_rect.h = nVidGuiHeight;

	nBurnDrvActive = currentSelected;
	return loadedTexture;
}

static void CreateRomDatName(TCHAR* szRomDat)
{
#if defined(BUILD_SDL2) && !defined(SDL_WINDOWS)
	_stprintf(szRomDat, _T("%s/roms.found"), SDL_GetPrefPath("fbneo", "config"));
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

	if (bShowAvailableOnly)
	{
		for(UINT32 i = 0; i < nBurnDrvCount; i++)
		{
			nBurnDrvActive = i;
			if (gameAv[i] && CheckIfSystem(i))
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

		filterGames = (unsigned int*)malloc(count * sizeof(unsigned int));

		filterGamesCount = 0;

		for(UINT32 i = 0; i < nBurnDrvCount; i++)
		{
			nBurnDrvActive = i;
			if (gameAv[i] && CheckIfSystem(i))
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
		filterGames = (unsigned int*)malloc(nBurnDrvCount * sizeof(unsigned int));
		filterGamesCount = 0;
		for(UINT32 i = 0; i < nBurnDrvCount; i++)
		{
			nBurnDrvActive = i;
			if (CheckIfSystem(i))
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


static void SwapSystemToCheck()
{
	startGame = -gamesperscreen_halfway;
	switch(nSystemToCheckMask)
	{
		case HARDWARE_PUBLIC_MASK:
			snprintf(systemName, MAX_PATH, "CAPCOM CPS1/2/3");
			nSystemToCheckMask = HARDWARE_PREFIX_CAPCOM;
			break;
		case HARDWARE_PREFIX_CAPCOM:
			snprintf(systemName, MAX_PATH, "Pre 1990s");
			nSystemToCheckMask = HARDWARE_MISC_PRE90S;
			break;
		case HARDWARE_MISC_PRE90S:
			snprintf(systemName, MAX_PATH, "Post 1990s");
			nSystemToCheckMask = HARDWARE_MISC_POST90S;
			break;
		case HARDWARE_MISC_POST90S:
			snprintf(systemName, MAX_PATH, "Midway");
			nSystemToCheckMask = HARDWARE_PREFIX_MIDWAY;
			break;
		case HARDWARE_PREFIX_MIDWAY:
			snprintf(systemName, MAX_PATH, "SEGA");
			nSystemToCheckMask = HARDWARE_PREFIX_SEGA;
			break;
		case HARDWARE_PREFIX_SEGA:
			snprintf(systemName, MAX_PATH, "Konami");
			nSystemToCheckMask = HARDWARE_PREFIX_KONAMI;
			break;
		case HARDWARE_PREFIX_KONAMI:
			snprintf(systemName, MAX_PATH, "Toaplan");
			nSystemToCheckMask = HARDWARE_PREFIX_TOAPLAN;
			break;
		case HARDWARE_PREFIX_TOAPLAN:
			snprintf(systemName, MAX_PATH, "SNK NeoGeo");
			nSystemToCheckMask = HARDWARE_SNK_NEOGEO;
			break;
		case HARDWARE_SNK_NEOGEO:
			snprintf(systemName, MAX_PATH, "CAVE");
			nSystemToCheckMask = HARDWARE_PREFIX_CAVE;
			break;
		case HARDWARE_PREFIX_CAVE:
			snprintf(systemName, MAX_PATH, "IGS PGM");
			nSystemToCheckMask = HARDWARE_PREFIX_IGS_PGM;
			break;
		case HARDWARE_PREFIX_IGS_PGM:
			snprintf(systemName, MAX_PATH, "Taito");
			nSystemToCheckMask = HARDWARE_PREFIX_TAITO;
			break;
		case HARDWARE_PREFIX_TAITO:
			snprintf(systemName, MAX_PATH, "Psikyo");
			nSystemToCheckMask = HARDWARE_PREFIX_PSIKYO;
			break;
		case HARDWARE_PREFIX_PSIKYO:
			snprintf(systemName, MAX_PATH, "Kaneko");
			nSystemToCheckMask = HARDWARE_PREFIX_KANEKO;
			break;
		case HARDWARE_PREFIX_KANEKO:
			snprintf(systemName, MAX_PATH, "IREM");
			nSystemToCheckMask = HARDWARE_PREFIX_IREM;
			break;
		case HARDWARE_PREFIX_IREM:
			snprintf(systemName, MAX_PATH, "Data East");
			nSystemToCheckMask = HARDWARE_PREFIX_DATAEAST;
			break;
		case HARDWARE_PREFIX_DATAEAST:
			snprintf(systemName, MAX_PATH, "Seta");
			nSystemToCheckMask = HARDWARE_PREFIX_SETA;
			break;
		case HARDWARE_PREFIX_SETA:
			snprintf(systemName, MAX_PATH, "Technos");
			nSystemToCheckMask = HARDWARE_PREFIX_TECHNOS;
			break;
		case HARDWARE_PREFIX_TECHNOS:
			snprintf(systemName, MAX_PATH, "Sega Megadrive / Genesis");
			nSystemToCheckMask = HARDWARE_SEGA_MEGADRIVE;
			break;
		case HARDWARE_SEGA_MEGADRIVE:
			snprintf(systemName, MAX_PATH, "NEC PC Engine");
			nSystemToCheckMask = HARDWARE_PCENGINE_PCENGINE;
			break;
		case HARDWARE_PCENGINE_PCENGINE:
			snprintf(systemName, MAX_PATH, "NEC Turbographx 16");
			nSystemToCheckMask = HARDWARE_PCENGINE_TG16;
			break;
		case HARDWARE_PCENGINE_TG16:
			snprintf(systemName, MAX_PATH, "NEC SGX");
			nSystemToCheckMask = HARDWARE_PCENGINE_SGX;
			break;
		case HARDWARE_PCENGINE_SGX:
			snprintf(systemName, MAX_PATH, "Sega SG-1000");
			nSystemToCheckMask = HARDWARE_SEGA_SG1000;
			break;
		case HARDWARE_SEGA_SG1000:
			snprintf(systemName, MAX_PATH, "ColecoVision");
			nSystemToCheckMask = HARDWARE_COLECO;
			break;
		case HARDWARE_COLECO:
			snprintf(systemName, MAX_PATH, "Sega Master System");
			nSystemToCheckMask = HARDWARE_SEGA_MASTER_SYSTEM;
			break;
		case HARDWARE_SEGA_MASTER_SYSTEM:
			snprintf(systemName, MAX_PATH, "Sega Game Gear");
			nSystemToCheckMask = HARDWARE_SEGA_GAME_GEAR;
			break;
		case HARDWARE_SEGA_GAME_GEAR:
			snprintf(systemName, MAX_PATH, "MSX");
			nSystemToCheckMask = HARDWARE_MSX;
			break;
		case HARDWARE_MSX:
			snprintf(systemName, MAX_PATH, "Sinclar Spectrum");
			nSystemToCheckMask = HARDWARE_SPECTRUM;
			break;
		case HARDWARE_SPECTRUM:
			snprintf(systemName, MAX_PATH, "Nintendo Entertainment System / Famicom");
			nSystemToCheckMask = HARDWARE_NES;
			break;
		case HARDWARE_NES:
			snprintf(systemName, MAX_PATH, "Nintendo Famicom Disk System");
			nSystemToCheckMask = HARDWARE_FDS;
			break;
		default:
			snprintf(systemName, MAX_PATH, "Everything");
			nSystemToCheckMask = HARDWARE_PUBLIC_MASK;
			break;
	}
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

//TODO: multithread the rendering...
void RefreshRomList(bool force_rescan)
{
	UINT32 tempgame;
	SDL_Event e;
	float screenpercentage = nVidGuiWidth / 100;

	SDL_Rect fillRect = { 0, 70, 0, 70 };

	tempgame = nBurnDrvActive;
	nBurnDrvActive = 0;

	if (!CheckGameAvb() && !force_rescan)
	{
		return;
	}

	for (INT32 i = 0; i < nBurnDrvCount; i++)
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
			fillRect = { 0, 70, (int)(((i * 100) / nBurnDrvCount) * screenpercentage) , 70 };

			SDL_SetRenderDrawColor(sdlRenderer, 0, 0xb3, 0x3b, 0xFF);
			SDL_RenderFillRect(sdlRenderer, &fillRect);

			incolor(fbn_color, /* unused */ 0);
			inprint(sdlRenderer, "FinalBurn Neo", 10, 10);
			inprint(sdlRenderer, "=============", 10, 20);

			inprint(sdlRenderer, "Scanning for ROM:", 10, 30);
			inprint(sdlRenderer, BurnDrvGetTextA(DRV_FULLNAME), 10, 40);
			SDL_RenderPresent(sdlRenderer);
			SDL_PollEvent(&e); // poll some events so OS doesn't think it's crashed
		}
	}
	WriteGameAvb();
	nBurnDrvActive = tempgame;
}

void gui_exit()
{
	SDL_GameControllerClose( gGameController );
	gGameController = NULL;

	if (filterGames!=NULL)
	{
		free(filterGames);
		filterGames = NULL;
	}

	kill_inline_font();
	SDL_DestroyTexture(titleTexture);
	SDL_DestroyRenderer(sdlRenderer);
	SDL_DestroyWindow(sdlWindow);
	SDL_FreeSurface(miscImage);
	free(gameAv);
}

void gui_init()
{
	gameAv = (char*)malloc(nBurnDrvCount);
	memset(gameAv, 0, nBurnDrvCount);

	if( SDL_NumJoysticks() < 1 )
	{
		printf( "Warning: No joysticks connected!\n" );
	}
	else
	{
		for (int i = 0; i < SDL_NumJoysticks(); ++i) {
		    if (SDL_IsGameController(i)) {
		        gGameController = SDL_GameControllerOpen(i);
		        if (gGameController) {
							  printf("Found a joypad!\n");
		            break;
		        } else {
		            printf("Could not open gamecontroller %i: %s\n", i, SDL_GetError());
		        }
		    }
		}

	}

	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		printf("vid init error\n");
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s", SDL_GetError());
		return;
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
		"FBNeo - Choose your weapon...",
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
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, videofiltering);
	SDL_RenderSetLogicalSize(sdlRenderer, nVidGuiWidth, nVidGuiHeight);

	miscImage = IMG_ReadXPMFromArray(misc_image);
	if(!miscImage) {
		printf("IMG_ReadXPMFromArray: %s\n", IMG_GetError());
		// handle error
	}
	inrenderer(sdlRenderer);
	prepare_inline_font();

	halfscreenheight = nVidGuiHeight / 2;
	halfscreenwidth = nVidGuiWidth / 2;
	thirdscreenheight =nVidGuiHeight/ 3;
	thirdscreenwidth = nVidGuiWidth / 3;

	gamesperscreen = (thirdscreenheight * 2) / 11;
	gamesperscreen_halfway = gamesperscreen / 2;
	
	listoffsetY = thirdscreenwidth / 2;
	listwidthY = thirdscreenwidth * 2;

	// assume if the filter list exists we are returning from a launched game.
	if (filterGamesCount > 0)
	{
		startGame = gameSelectedFromFilter - gamesperscreen_halfway;
	}
	else
	{
		startGame = nBurnDrvActive - gamesperscreen_halfway;
	}
	RefreshRomList(false);
	DoFilterGames();
}

void gui_render()
{

	SDL_SetRenderDrawColor(sdlRenderer, 0x1a, 0x1e, 0x1d, SDL_ALPHA_OPAQUE);
	SDL_RenderClear(sdlRenderer);
	if (titleTexture != NULL) // JUST FOR TESTING!!
	{
		SDL_RenderCopy(sdlRenderer, titleTexture, &title_texture_rect, &dest_title_texture_rect);
	}
	
		// Game List
	renderPanel(sdlRenderer, 0,  0, nVidGuiWidth, 28,  0x00, 0x00, 0x00);

	// Game List
	renderPanel(sdlRenderer, listoffsetY,  28, listwidthY, (thirdscreenheight*2)-28,  0x40, 0x20, 0x0b);

	// Selected game background
	renderPanel(sdlRenderer, 0,  28 + (gamesperscreen_halfway * 10), nVidGuiWidth, 12,  0x41, 0x1d, 0x62);
	// game info
	renderPanel(sdlRenderer,  0, nVidGuiHeight - 60, nVidGuiWidth, nVidGuiHeight,  0x41, 0x1d, 0xf2);

	incolor(fbn_color, /* unused */ 0);
	inprint(sdlRenderer, "FinalBurn Neo * F1 - Rescan / F2 - Filter Missing / F3 - System Filter / F4 - Filter Clones / F12 - Quit *", 10, 10);
	inprint(sdlRenderer, systemName, 10, 20);
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
				char infoLine[512];
				snprintf(infoLine, 512, "Year: %s - Manufacturer: %s - System: %s", BurnDrvGetTextA(DRV_DATE), BurnDrvGetTextA(DRV_MANUFACTURER), BurnDrvGetTextA(DRV_SYSTEM));
				char romLine[512];
				snprintf(romLine, 512, "Romset: %s - Parent: %s", BurnDrvGetTextA(DRV_NAME), BurnDrvGetTextA(DRV_PARENT));

				inprint_shadowed(sdlRenderer, BurnDrvGetTextA(DRV_FULLNAME), listoffsetY, nVidGuiHeight - 60);
				inprint_shadowed(sdlRenderer, infoLine, listoffsetY, nVidGuiHeight - 50);
				inprint_shadowed(sdlRenderer, romLine, listoffsetY, nVidGuiHeight - 40);
				inprint_shadowed(sdlRenderer, BurnDrvGetTextA(DRV_COMMENT), listoffsetY, nVidGuiHeight - 30);
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
	static UINT32 previousSelected;

	while (!quit)
	{
		//TODO: probably move this down inside the while (SDL_pollevent) bit...
		SDL_GameControllerUpdate();
		if (SDL_GameControllerGetAxis(gGameController, SDL_CONTROLLER_AXIS_LEFTY)<= -JOYSTICK_DEAD_ZONE)
		{
			startGame--;
		}
		else if (SDL_GameControllerGetAxis(gGameController, SDL_CONTROLLER_AXIS_LEFTY)>=JOYSTICK_DEAD_ZONE)
		{
			startGame++;
		}
		if (SDL_GameControllerGetButton(gGameController, SDL_CONTROLLER_BUTTON_A))
		{
			nBurnDrvActive = gametoplay;

			if (gameAv[nBurnDrvActive])
			{
				return gametoplay;
			}
		}
		if (SDL_GameControllerGetButton(gGameController, SDL_CONTROLLER_BUTTON_Y))
		{
			RefreshRomList(true);
		}
		if (SDL_GameControllerGetButton(gGameController, SDL_CONTROLLER_BUTTON_LEFTSHOULDER))
		{
			findPrevLetter();
		}
		if (SDL_GameControllerGetButton(gGameController, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER))
		{
			findNextLetter();
		}
		while (SDL_PollEvent(&e))
		{
			if (e.type == SDL_QUIT)
			{
				quit = true;
			}
			if (e.type == SDL_MOUSEWHEEL)
			{
				if (e.wheel.y > 0) // scroll up
				{
					startGame--;
				}
				else if (e.wheel.y < 0) // scroll down
				{
					startGame++;
				}
			}
			if (e.type == SDL_MOUSEBUTTONDOWN)
			{
				switch (e.button.button)
				{
				case SDL_BUTTON_LEFT:
					nBurnDrvActive = gametoplay;
					if (gameAv[nBurnDrvActive])
					{
						return gametoplay;
					}
					break;

				case SDL_BUTTON_RIGHT:
					quit = 1;
					break;
				}
			}
			if (e.type == SDL_KEYDOWN)
			{
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
				case SDLK_RETURN:
					if (e.key.keysym.mod & KMOD_ALT) 
					{
						SetFullscreen(!GetFullscreen());
					} 
					else 
					{
						nBurnDrvActive = gametoplay;
						if (gameAv[nBurnDrvActive])
						{
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
				case SDLK_F12:
					quit = 1;
					break;

				default:
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

		if (previousSelected != gametoplay)
		{
			SDL_DestroyTexture(titleTexture);
			titleTexture = LoadTitleImage(sdlRenderer, titleTexture);
			if (titleTexture==NULL)
			{
				// Commented out for now :)
				// int w, h;
				// titleTexture = SDL_CreateTextureFromSurface(sdlRenderer, miscImage);
				// SDL_QueryTexture(titleTexture, NULL, NULL, &w, &h);
				// title_texture_rect.x = 0; //the x coordinate
				// title_texture_rect.y = 0; // the y coordinate
				// title_texture_rect.w = w; //the width of the texture
				// title_texture_rect.h = h; //the height of the texture

				// dest_title_texture_rect.x = 0; //the x coordinate
				// dest_title_texture_rect.y = 0; // the y coordinate
				// dest_title_texture_rect.w = nVidGuiWidth;
				// dest_title_texture_rect.h = nVidGuiHeight;
			}
		}

		previousSelected = gametoplay;

		gui_render();
	}
	return -1;
}
