#include "burner.h"

#include <SDL_image.h>

extern char videofiltering[3];

static SDL_Window* sdlWindow = NULL;
static SDL_Renderer* sdlRenderer = NULL;

static SDL_Texture* titleTexture = NULL;

static int nVidGuiWidth = 800;
static int nVidGuiHeight = 600;

static int startGame = 0; // game at top of list as it is displayed on the menu
static unsigned int gamesperscreen = 12;
static unsigned int gamesperscreen_halfway = 6;
static unsigned int gametoplay = 0;

const int JOYSTICK_DEAD_ZONE = 8000;
SDL_GameController* gGameController = NULL;

#define NUMSTARS  512

static float star_x[NUMSTARS];
static float star_y[NUMSTARS];
static float star_z[NUMSTARS];

static int star_screenx[NUMSTARS];
static int star_screeny[NUMSTARS];

static float star_zv[NUMSTARS];

static int centerx = 0;
static int centery = 0;

#define fbn_color           0xfe8a71
#define select_color        0xffffff
#define normal_color        0x1eaab7
#define normal_color_parent 0xaebac7
#define unavailable_color   0xFF0000
#define info_color          0x3a3e3d

static int color_result = 0;
static double color_x = 0.01;
static double color_y = 0.01;
static double color_z = 0.01;

static SDL_Rect title_texture_rect;
static SDL_Rect dest_title_texture_rect;


static char* gameAv = NULL;


SDL_Texture* LoadTitleImage(SDL_Renderer* renderer, SDL_Texture* loadedTexture)
{
	char titlePath[MAX_PATH] = { 0 };
	int w, h;

	int currentSelected = nBurnDrvActive;
	nBurnDrvActive = gametoplay;
#ifndef _WIN32
	snprintf(titlePath, MAX_PATH, "%s%s.png", "/usr/local/share/titles/", BurnDrvGetTextA(0));
#else
	snprintf(titlePath, MAX_PATH, "\\support\\titles\\%s.png", BurnDrvGetTextA(0));
#endif

	loadedTexture = IMG_LoadTexture(renderer, titlePath);
	SDL_QueryTexture(loadedTexture, NULL, NULL, &w, &h);

	title_texture_rect.x = 0; //the x coordinate
	title_texture_rect.y = 0; // the y coordinate
	title_texture_rect.w = w; //the width of the texture
	title_texture_rect.h = h; //the height of the texture

	dest_title_texture_rect.x = nVidGuiWidth - w; //the x coordinate
	dest_title_texture_rect.y = (nVidGuiHeight / 2) - (h / 2); // the y coordinate
	dest_title_texture_rect.w = w; //the width of the texture
	dest_title_texture_rect.h = h; //the height of the texture
	nBurnDrvActive = currentSelected;
	return loadedTexture;
}


float random_gen()
{
	return rand() / (float)RAND_MAX;
}

float randomRange(float low, float high)
{
	float range = high - low;

	return (float)(random_gen() * range) + low;
}

void star_init(int screencenterx, int screencentery)
{
	centerx = screencenterx;
	centery = screencentery;
	srand(time(0));
	for (int i = 0; i < NUMSTARS; i++)
	{
		star_x[i] = randomRange(-1000, 1000);
		star_y[i] = randomRange(-1000, 1000);
		star_z[i] = randomRange(100, 1000);
		star_zv[i] = randomRange(0.5, 5);
	}
}

void star_render(SDL_Renderer* renderer)
{
	for (int i = 0; i < NUMSTARS; i++)
	{
		star_z[i] = star_z[i] - star_zv[i];
		star_screenx[i] = star_x[i] / star_z[i] * 100 + centerx;
		star_screeny[i] = star_y[i] / star_z[i] * 100 + centery;
		if (star_screenx[i] > nVidGuiWidth || star_screeny[i] > nVidGuiHeight || star_z[i] < 1)
		{
			star_x[i] = randomRange(-1000, 1000);
			star_y[i] = randomRange(-1000, 1000);
			star_z[i] = randomRange(100, 1000);
			star_zv[i] = randomRange(0.5, 5);
		}

		int b = 255 - ((255) * star_zv[i]) * (1000 / star_z[i]);
		SDL_SetRenderDrawColor(renderer, b, b, b, 255);
		SDL_RenderDrawPoint(renderer, star_screenx[i], star_screeny[i]);
	}
}


static void CreateRomDatName(TCHAR* szRomDat)
{
	_stprintf(szRomDat, _T("%s/roms.found"), SDL_GetPrefPath("fbneo", "config"));

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

	kill_inline_font();
	SDL_DestroyTexture(titleTexture);
	SDL_DestroyRenderer(sdlRenderer);
	SDL_DestroyWindow(sdlWindow);
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

	star_init(nVidGuiWidth / 2, nVidGuiHeight / 2);

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
		// In the case that the window could not be made...
		printf("Could not create renderer: %s\n", SDL_GetError());
		return;
	}

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, videofiltering);
	SDL_RenderSetLogicalSize(sdlRenderer, nVidGuiWidth, nVidGuiHeight);

	inrenderer(sdlRenderer);
	prepare_inline_font();

	gamesperscreen = (nVidGuiHeight - 100) / 10;
	gamesperscreen_halfway = gamesperscreen / 2;

	startGame = nBurnDrvActive - gamesperscreen_halfway;

	RefreshRomList(false);
}


inline void calcSelectedItemColor()
{
	color_x += randomRange(0.01, 0.02);
	color_y += randomRange(0.01, 0.07);
	color_z += randomRange(0.01, 0.09);

	SDL_Color pal[1];
	pal[0].r = 190 + 64 * sin(color_x + (color_result * 0.004754));
	pal[0].g = 190 + 64 * sin(color_y + (color_result * 0.006754));
	pal[0].b = 190 + 64 * sin(color_z + (color_result * 0.005754));
	color_result+5;

	incolor1(pal);
}

void gui_render()
{

	SDL_SetRenderDrawColor(sdlRenderer, 0x1a, 0x1e, 0x1d, SDL_ALPHA_OPAQUE);
	SDL_RenderClear(sdlRenderer);

	SDL_Rect fillRect = { 0,  28 + (gamesperscreen_halfway * 10), nVidGuiWidth, 12};
	SDL_SetRenderDrawColor(sdlRenderer, 0x41, 0x1d, 0x62, 0xFF);
	SDL_RenderFillRect(sdlRenderer, &fillRect);

	star_render(sdlRenderer);

	if (titleTexture != NULL) // JUST FOR TESTING!!
	{
		SDL_RenderCopy(sdlRenderer, titleTexture, &title_texture_rect, &dest_title_texture_rect);
	}

	incolor(fbn_color, /* unused */ 0);
	inprint(sdlRenderer, "FinalBurn Neo", 10, 10);
	inprint(sdlRenderer, "=============", 10, 20);
	incolor(normal_color, /* unused */ 0);
	for (unsigned int i = startGame, game_counter = 0; game_counter < gamesperscreen; i++, game_counter++)
	{
		if (i > 0 && i < nBurnDrvCount)
		{
			nBurnDrvActive = i;
			if (game_counter == gamesperscreen_halfway)
			{
				calcSelectedItemColor();
				//incolor(select_color, /* unused */ 0);
				inprint_shadowed(sdlRenderer, BurnDrvGetTextA(DRV_FULLNAME), 10, 30 + (gamesperscreen_halfway * 10));
				gametoplay = i;

				fillRect = { 0, nVidGuiHeight - 70, nVidGuiWidth, nVidGuiHeight };
				SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 0xFF);
				SDL_RenderFillRect(sdlRenderer, &fillRect);

				incolor(info_color, /* unused */ 0);
				char infoLine[512];
				snprintf(infoLine, 512, "Year: %s - Manufacturer: %s - System: %s", BurnDrvGetTextA(DRV_DATE), BurnDrvGetTextA(DRV_MANUFACTURER), BurnDrvGetTextA(DRV_SYSTEM));
				char romLine[512];
				snprintf(romLine, 512, "Romset: %s - Parent: %s", BurnDrvGetTextA(DRV_NAME), BurnDrvGetTextA(DRV_PARENT));

				inprint_shadowed(sdlRenderer, BurnDrvGetTextA(DRV_FULLNAME), 10, nVidGuiHeight - 60);
				inprint_shadowed(sdlRenderer, infoLine, 10, nVidGuiHeight - 50);
				inprint_shadowed(sdlRenderer, romLine, 10, nVidGuiHeight - 40);
				inprint_shadowed(sdlRenderer, BurnDrvGetTextA(DRV_COMMENT), 10, nVidGuiHeight - 30);
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
				inprint(sdlRenderer, BurnDrvGetTextA(DRV_FULLNAME), 10, 30 + (game_counter * 10));
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

				case SDLK_LEFT:
					startGame -= 10;
					break;

				case SDLK_RIGHT:
					startGame += 10;
					break;

				case SDLK_RETURN:
					nBurnDrvActive = gametoplay;
					if (gameAv[nBurnDrvActive])
					{
						return gametoplay;
					}
					break;
				case SDLK_F1:
					RefreshRomList(true);
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
		if (startGame < -(int)gamesperscreen_halfway + 1)
		{
			startGame = -gamesperscreen_halfway + 1;
		}

		if (startGame > (int)nBurnDrvCount - (int)gamesperscreen_halfway - 1)
		{
			startGame = nBurnDrvCount - gamesperscreen_halfway - 1;
		}

		if (previousSelected != gametoplay)
		{
			SDL_DestroyTexture(titleTexture);
			titleTexture = LoadTitleImage(sdlRenderer, titleTexture);
		}

		previousSelected = gametoplay;

		gui_render();
	}
	return -1;
}
