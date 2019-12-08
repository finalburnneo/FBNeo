#include "burner.h"

#include <SDL_image.h>

static SDL_Window *  sdlWindow   = NULL;
static SDL_Renderer *sdlRenderer = NULL;

static SDL_Texture *titleTexture = NULL;


static int nVidGuiWidth  = 800;
static int nVidGuiHeight = 600;

static int startGame      = 0; // game at top of list as it is displayed on the menu
static unsigned int gamesperscreen = 12;
static unsigned int gamesperscreen_halfway = 6;
static unsigned int gametoplay     = 0;

#define NUMSTARS  512

static float star_x[NUMSTARS];
static float star_y[NUMSTARS];
static float star_z[NUMSTARS];

static int star_screenx[NUMSTARS];
static int star_screeny[NUMSTARS];

static float star_zv[NUMSTARS];

static int centerx = 0;
static int centery = 0;

#define fbn_color       0xfe8a71
#define select_color    0xffffff
#define normal_color    0x1eaab7
#define info_color      0x3a3e3d

static int color_result = 0;
static double color_x      = 0.01;
static double color_y      = 0.01;
static double color_z      = 0.01;

static SDL_Rect title_texture_rect;
static SDL_Rect dest_title_texture_rect;




SDL_Texture* LoadTitleImage(SDL_Renderer* renderer)
{
		char titlePath[MAX_PATH] = { 0 };
		int w, h;
	#ifndef _WIN32
		snprintf(titlePath, MAX_PATH, "%s%s.png", "/usr/local/share/titles/", BurnDrvGetTextA(0));
	#else
		snprintf(titlePath, MAX_PATH, "\\support\\titles\\$s.png", BurnDrvGetTextA(0));
	#endif

		SDL_Texture* loadedTexture = IMG_LoadTexture(renderer, titlePath);
		SDL_QueryTexture(loadedTexture, NULL, NULL, &w, &h);

		title_texture_rect.x = 0; //the x coordinate
		title_texture_rect.y = 0; // the y coordinate
		title_texture_rect.w = w; //the width of the texture
		title_texture_rect.h = h; //the height of the texture

		dest_title_texture_rect.x = nVidGuiWidth - w; //the x coordinate
		dest_title_texture_rect.y = (nVidGuiHeight / 2) - (h/2); // the y coordinate
		dest_title_texture_rect.w = w; //the width of the texture
		dest_title_texture_rect.h = h; //the height of the texture

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
				star_x[i]  = randomRange(-1000, 1000);
				star_y[i]  = randomRange(-1000, 1000);
				star_z[i]  = randomRange(100, 1000);
				star_zv[i] = randomRange(0.5, 5);
		}
}

void star_render(SDL_Renderer *renderer)
{
		for (int i = 0; i < NUMSTARS; i++)
		{
				star_z[i]       = star_z[i] - star_zv[i];
				star_screenx[i] = star_x[i] / star_z[i] * 100 + centerx;
				star_screeny[i] = star_y[i] / star_z[i] * 100 + centery;
				if (star_screenx[i] > nVidGuiWidth || star_screeny[i] > nVidGuiHeight || star_z[i] < 1)
				{
						star_x[i]  = randomRange(-1000, 1000);
						star_y[i]  = randomRange(-1000, 1000);
						star_z[i]  = randomRange(100, 1000);
						star_zv[i] = randomRange(0.5, 5);
				}

				int b =255 - ((255) * star_zv[i]) * (1000 / star_z[i]);
				SDL_SetRenderDrawColor(renderer, b, b, b, 255);
				SDL_RenderDrawPoint(renderer, star_screenx[i], star_screeny[i]);
		}
}

void gui_exit()
{
		kill_inline_font();
		SDL_DestroyTexture(titleTexture);
		SDL_DestroyRenderer(sdlRenderer);
		SDL_DestroyWindow(sdlWindow);
}

void gui_init()
{
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

				nVidGuiWidth  = dm.w;
				nVidGuiHeight = dm.h;
				screenFlags   = SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN_DESKTOP;
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

		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, 0);
		SDL_RenderSetLogicalSize(sdlRenderer, nVidGuiWidth, nVidGuiHeight);

		inrenderer(sdlRenderer);
		prepare_inline_font();

		gamesperscreen = (nVidGuiHeight - 100) / 10;
		gamesperscreen_halfway = gamesperscreen / 2;
}


inline void calcSelectedItemColor()
{
		color_x += randomRange(0.03, 0.08);
		color_y += randomRange(0.02, 0.07);
		color_z += randomRange(0.01, 0.06);

		SDL_Color pal[1];
		pal[0].r = 190 + 64 * sin(color_x + (color_result * 0.004754));
		pal[0].g = 190 + 64 * sin(color_y + (color_result * 0.006754));
		pal[0].b = 190 + 64 * sin(color_z + (color_result * 0.004754));
		color_result++;

		incolor1(pal);
}

void gui_render()
{
		SDL_SetRenderDrawColor(sdlRenderer, 0x1a, 0x1e, 0x1d, SDL_ALPHA_OPAQUE);
		SDL_SetRenderDrawColor(sdlRenderer, 0x1a, 0x1e, 0x1d, SDL_ALPHA_OPAQUE);
		SDL_RenderClear(sdlRenderer);
		star_render(sdlRenderer);
		if (titleTexture !=NULL) // JUST FOR TESTING!!
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
								inprint_shadowed(sdlRenderer, BurnDrvGetTextA(DRV_FULLNAME), 10, 30 + (game_counter * 10));
								gametoplay = i;

								SDL_Rect fillRect = { 0, nVidGuiHeight - 70, nVidGuiWidth, nVidGuiHeight };
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
								incolor(normal_color, /* unused */ 0);
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

		while (!quit)
		{
				while (SDL_PollEvent(&e))
				{
						if (e.type == SDL_QUIT)
						{
								quit = true;
						}
						if (e.type == SDL_KEYUP)
						{
								SDL_DestroyTexture(titleTexture);
								titleTexture = LoadTitleImage(sdlRenderer);
						}
						if (e.type == SDL_KEYDOWN)
						{
								switch (e.key.keysym.sym)
								{
								case SDLK_UP:
										startGame--;
										break;

								case SDLK_DOWN:
										if (startGame < (int)nBurnDrvCount)
										{
												startGame++;
										}
										break;

								case SDLK_LEFT:
										startGame -= 10;
										break;

								case SDLK_RIGHT:
										//   if (startGame < nBurnDrvCount - 10)
								{
										startGame += 10;
								}
								break;

								case SDLK_RETURN:
										nBurnDrvActive = gametoplay;
										printf("selected %s\n", BurnDrvGetTextA(DRV_FULLNAME));
										return gametoplay;

								case SDLK_F12:
										quit = 1;
										break;

								default:
										break;
								}
								break;
						}
						if (e.type == SDL_MOUSEBUTTONDOWN)
						{
								quit = true;
						}
				}

				// TODO: Need to put more clamping logic here....
				if (startGame < -(int)gamesperscreen_halfway+1)
				{
						startGame = -gamesperscreen_halfway+1;
				}
				gui_render();
		}
		return -1;
}
