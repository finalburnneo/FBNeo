#include "burner.h"
#include "sdl2_gui_common.h"

#if SDL_BYTEORDER != SDL_BIG_ENDIAN
const UINT32 amask = 0xff000000;
const UINT32 rmask = 0x00ff0000;
const UINT32 gmask = 0x0000ff00;
const UINT32 bmask = 0x000000ff;
#else
const UINT32 amask = 0x000000ff;
const UINT32 rmask = 0x0000ff00;
const UINT32 gmask = 0x00ff0000;
const UINT32 bmask = 0xff000000;
#endif


static Uint32 starting_stick;

static SDL_Renderer* sdlRenderer = NULL;
static SDL_Surface* screenshot = NULL;
static SDL_Texture* screenshotTexture = NULL;

static SDL_Rect title_texture_rect;
static SDL_Rect dest_title_texture_rect;

static int screenW = 0;
static int screenH = 0;


struct MenuItem
{
	const char* name;			// The filename of the zip file (without extension)
  int (*menuFunction)();
  char* (*menuText)();
};

#define MAINMENU 0
#define DIPMENU 1
#define CONTROLLERMENU 2
#define SAVESTATE 3
#define LOADSTATE 4
#define SCREENSHOT 5
#define RESET 6
#define CHEATMENU 7
#define CHEATOPTIONSMENU 8

#define TITLELENGTH 31
char MenuTitle[TITLELENGTH + 1] = "FinalBurn Neo\0";

// menu item tracking
static UINT16 current_menu = MAINMENU;
static UINT16 current_selected_item = 0;
UINT16 firstMenuLine = 0;
UINT16 maxLinesMenu = 16;

int QuickSave()
{
  QuickState(1);
  return 1;
}

int QuickLoad()
{
  QuickState(0);
  return 1;
}

int MainMenuSelected()
{
	snprintf(MenuTitle, TITLELENGTH, "FinalBurn Neo");
	current_selected_item = 0;
	current_menu = MAINMENU;
	return 0;
}


// Controllers stuff
#define JOYSTICK_DEAD_ZONE 8000
#define MAX_JOYSTICKS 4
// #define BUTTONS_TO_MAP 8

UINT16 current_selected_joystick = 0;
UINT16 default_joystick = 0;


// Cheats related stuff
struct MenuItem cheatMenu[CHEAT_MAX_ADDRESS];
struct MenuItem cheatOptionsMenu[CHEAT_MAX_OPTIONS];
UINT16 cheatcount = 0;
UINT16 cheatoptionscount = 0;
UINT16 current_selected_cheat = 0;
bool isCheatActivated[CHEAT_MAX_ADDRESS - 2] = {false}; // Array to keed track of activated cheats and use a different color in list
bool stayCurrentCheat = false;
int CheatMenuSelected();
int CheatOptionsMenuSelected();

int IgnoreSelection()
{
	return 0;
}

int SelectedCheatOption()
{
	CheatEnable(current_selected_cheat, current_selected_item);
	stayCurrentCheat = true;
	CheatOptionsMenuSelected();
	return 0;
}

int CheatOptionsMenuSelected()
{
	if (stayCurrentCheat) stayCurrentCheat = false;
	else {
		current_selected_cheat = current_selected_item;
		current_selected_item = 0;
	}
	current_menu = CHEATOPTIONSMENU;
	cheatoptionscount = 0;
	CheatInfo* pCurrentCheat = pCheatInfo;

	for (int i = 0; i < current_selected_cheat; i++) {
		pCurrentCheat = pCurrentCheat->pNext;
	}   // Skip to selected cheat number

	snprintf(MenuTitle, TITLELENGTH, pCurrentCheat->szCheatName);

	for (int i = 0; pCurrentCheat->pOption[i]; i++) {
		if (_tcslen(pCurrentCheat->pOption[i]->szOptionName) && strcmp(pCurrentCheat->pOption[i]->szOptionName, " ")) {

			// Look for check boxes...
			if ((pCurrentCheat->pOption[i]->szOptionName[0] == '[') && (pCurrentCheat->pOption[i]->szOptionName[2] == ']') && (pCurrentCheat->pOption[i]->szOptionName[3] == ' ')) {
				if (i == pCurrentCheat->nCurrent) pCurrentCheat->pOption[i]->szOptionName[1] = 'X';  // Active cheat option
				else pCurrentCheat->pOption[i]->szOptionName[1] = ' ';                               // Not active option
			} else {
				// Add check boxes
				char tmpoptionname[CHEAT_MAX_NAME] = {0};
				if (i == pCurrentCheat->nCurrent) snprintf(tmpoptionname, CHEAT_MAX_NAME, "[X] %s", pCurrentCheat->pOption[i]->szOptionName);  // Active cheat option
				else snprintf(tmpoptionname, CHEAT_MAX_NAME, "[ ] %s", pCurrentCheat->pOption[i]->szOptionName);                               // Not active option
				snprintf(pCurrentCheat->pOption[i]->szOptionName, CHEAT_MAX_NAME, tmpoptionname);
			}

			cheatOptionsMenu[i] = (MenuItem){pCurrentCheat->pOption[i]->szOptionName, SelectedCheatOption, NULL};

		} else cheatOptionsMenu[i] = (MenuItem){".\0", IgnoreSelection, NULL};    // Ignore cheats options without name
		cheatoptionscount++;
	}

	cheatOptionsMenu[cheatoptionscount] = (MenuItem){"BACK\0", CheatMenuSelected, NULL};
	cheatoptionscount++;
	return 0;
}

int DisableAllCheats()
{
	for (int i = 0; i < CHEAT_MAX_ADDRESS - 2; i++) {
		if (isCheatActivated[i]) {
			CheatEnable(i, 0);
			isCheatActivated[i] = false;
		}
	}
	return 0;
	}

int CheatMenuSelected()
{
	current_selected_item = 0;
	current_menu = CHEATMENU;
	cheatcount = 0;
	int i = 0;
	CheatInfo* pCurrentCheat = pCheatInfo;

	snprintf(MenuTitle, TITLELENGTH, "Cheats");

	while ((pCurrentCheat) && (i < CHEAT_MAX_ADDRESS - 2)) {  // Save two lines for DISABLE ALL and for BACK
		if (_tcslen(pCurrentCheat->szCheatName) && strcmp(pCurrentCheat->szCheatName, " ")) {
			cheatMenu[i] = (MenuItem){pCurrentCheat->szCheatName, CheatOptionsMenuSelected, NULL};
			if (pCurrentCheat->nCurrent) isCheatActivated[i] = true;
			else isCheatActivated[i] = false;
		} else cheatMenu[i] = (MenuItem){".\0", IgnoreSelection, NULL};    // Ignore cheats without name
		pCurrentCheat = pCurrentCheat->pNext;
		i++;
	}
	cheatMenu[i] = (MenuItem){"DISABLE ALL CHEATS\0", DisableAllCheats, NULL};
	i++;

	cheatMenu[i] = (MenuItem){"BACK\0", MainMenuSelected, NULL};
	cheatcount = i + 1;
	return 0;
}

int ControllerMenuSelected()
{
	snprintf(MenuTitle, TITLELENGTH, "Controller Options");
	current_selected_item = 0;
	current_menu = CONTROLLERMENU;
	//TODO work out UI for controller mappings
	return 0;
}

int DIPMenuSelected()
{
	current_selected_item = 0;
	current_menu = DIPMENU;
	snprintf(MenuTitle, TITLELENGTH, "DIP Switches");
	//TODO Load the dips into an array of MenuItems
	return 0;
}

int BackToGameSelected()
{
	return 1;
}

#define MAINMENU_COUNT 7

struct MenuItem mainMenu[MAINMENU_COUNT] =
{
 {"DIP Switches\0", DIPMenuSelected, NULL},
 {"Controller Options\0", ControllerMenuSelected, NULL},
 {"Cheats\0", CheatMenuSelected, NULL},
 {"Save State\0", QuickSave, NULL},
 {"Load State\0", QuickLoad, NULL},
 {"Save Screenshot\0", MakeScreenShot, NULL},
 {"Back to Game!\0", BackToGameSelected, NULL},
};

#define DIPMENU_COUNT 1

struct MenuItem dipMenu[DIPMENU_COUNT] =
{
	{"BACK \0", MainMenuSelected, NULL},
};

#define CONTROLLERMENU_COUNT 1

struct MenuItem controllerMenu[CONTROLLERMENU_COUNT] =
{
	{"BACK \0", MainMenuSelected, NULL},
};

// menu instance tracking
struct MenuItem *current_menu_items = mainMenu;
static UINT16 current_item_count = MAINMENU_COUNT;

void ingame_gui_init()
{
	AudSoundStop();
}

void ingame_gui_exit()
{
  AudSoundPlay();
  SDL_FreeSurface(screenshot);
  SDL_DestroyTexture(screenshotTexture);
}

void ingame_gui_render()
{
	SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, SDL_ALPHA_OPAQUE);   // Changed background color to black as the old color would stay visible after returning to game
	SDL_RenderClear(sdlRenderer);
	if (current_menu == MAINMENU) {		// Show screenshot only in main menu because it can make text hard to read in some sub menus
		SDL_RenderCopy(sdlRenderer, screenshotTexture, &title_texture_rect, &dest_title_texture_rect);
	}
	incolor(fbn_color, /* unused */ 0);
	inprint(sdlRenderer, MenuTitle, 10, 10);
	inprint(sdlRenderer, "==============================", 10, 20);
	switch (current_menu)
	{
		case MAINMENU:
			current_item_count = MAINMENU_COUNT;
			current_menu_items = mainMenu;
			break;
		case DIPMENU:
			current_item_count = DIPMENU_COUNT;
			current_menu_items = dipMenu;
			break;
		case CONTROLLERMENU:
			current_item_count = CONTROLLERMENU_COUNT;
			current_menu_items = controllerMenu;
			break;
		case CHEATMENU:
			current_item_count = cheatcount;
			current_menu_items = cheatMenu;
			break;
		case CHEATOPTIONSMENU:
			current_item_count = cheatoptionscount;
			current_menu_items = cheatOptionsMenu;
			break;
	}

	int c = 0;
	// Keep selected line always visible in screen
	if (current_selected_item > firstMenuLine + maxLinesMenu) {
		firstMenuLine = current_selected_item - maxLinesMenu;
	} else if (current_selected_item < firstMenuLine) {
		firstMenuLine = current_selected_item;
	}

	if (firstMenuLine > 0) {
		incolor(normal_color, /* unused */ 0);
		inprint(sdlRenderer, "( ... more ... )", 10, 30+(10*c));
	}

	for (int i = firstMenuLine; ((i < current_item_count) && (i < firstMenuLine + maxLinesMenu + 1)); i++) {
		if (i == current_selected_item) {
			calcSelectedItemColor();
		} else if ((current_menu == CHEATMENU) && isCheatActivated[i]) {
			incolor(0x009000, /* unused */ 0);
		} else {
			incolor(normal_color, /* unused */ 0);
		}
		c++;
		inprint(sdlRenderer,current_menu_items[i].name , 10, 30+(10*c));
	}
	if (current_item_count > firstMenuLine + maxLinesMenu + 1 ) {
		incolor(normal_color, /* unused */ 0);
		inprint(sdlRenderer, "( ... more ... )", 10, 40+(10*c));
	}
	SDL_RenderPresent(sdlRenderer);
}

int ingame_gui_process()
{
	SDL_Event e;
	while (SDL_PollEvent(&e))
	{
		switch (e.type)
		{
			case SDL_QUIT:
				return 1;
				break;
			case SDL_KEYDOWN:
				switch (e.key.keysym.sym)
				{
					case SDLK_TAB:
						return 1;
						break;
					case SDLK_UP:
						if (current_selected_item > 0) current_selected_item--;
						break;
					case SDLK_DOWN:
						if (current_selected_item < current_item_count - 1) current_selected_item++;
						break;
					case SDLK_LEFT:
						if (current_selected_item > 10) current_selected_item -= 10;
						else current_selected_item = 0;
						break;
					case SDLK_RIGHT:
						if (current_selected_item < current_item_count - 11) current_selected_item += 10;
						else current_selected_item = current_item_count - 1;
						break;
					case SDLK_RETURN:
						if (current_menu_items[current_selected_item].menuFunction!=NULL)
						{
							int (*menuFunction)();
							menuFunction = current_menu_items[current_selected_item].menuFunction;
							return menuFunction();
						}
						break;
				}
				break;
			case SDL_JOYAXISMOTION:
				if (e.jaxis.which == current_selected_joystick) {
					switch (e.jaxis.axis)
					{
						case 1:
							if ((e.jaxis.value < -JOYSTICK_DEAD_ZONE) && (current_selected_item > 0)) current_selected_item--;
							else if ((e.jaxis.value > JOYSTICK_DEAD_ZONE) && (current_selected_item < current_item_count - 1)) current_selected_item++;
							break;
						case 0:
							if (e.jaxis.value < -JOYSTICK_DEAD_ZONE) {
								if (current_selected_item > 10) current_selected_item -= 10;
								else current_selected_item = 0;
							} else if (e.jaxis.value > JOYSTICK_DEAD_ZONE) {
								if (current_selected_item < current_item_count - 11) current_selected_item += 10;
								else current_selected_item = current_item_count - 1;
							}
							break;
					}
				}
				break;
			case SDL_JOYHATMOTION:
				if (e.jhat.which == current_selected_joystick) {
					switch (e.jhat.value)
					{
						case SDL_HAT_UP:
							if (current_selected_item > 0) current_selected_item--;
							break;
						case SDL_HAT_DOWN:
							if (current_selected_item < current_item_count - 1) current_selected_item++;
							break;
						case SDL_HAT_LEFT:
							if (current_selected_item > 10) current_selected_item -= 10;
							else current_selected_item = 0;
							break;
						case SDL_HAT_RIGHT:
							if (current_selected_item < current_item_count - 11) current_selected_item += 10;
							else current_selected_item = current_item_count - 1;
							break;
					}
				}
				break;
			case SDL_JOYBUTTONDOWN:
				if (e.jbutton.which == current_selected_joystick) {
//					if ((current_menu == MAPPINGMENU) && (current_selected_item < BUTTONS_TO_MAP)) mappedbuttons[current_selected_item] = e.jbutton.button;	// Store pressed button
					if (current_menu_items[current_selected_item].menuFunction!=NULL) {		// Execute selected option
						int (*menuFunction)();
						menuFunction = current_menu_items[current_selected_item].menuFunction;
						return menuFunction();
					}
				}
				break;
			case SDL_CONTROLLERDEVICEREMOVED:
			case SDL_JOYDEVICEREMOVED:
				// Try not to lose total control but this solution has many ways to fail
				for (int i = 0; (i < SDL_NumJoysticks()) && (i < MAX_JOYSTICKS); i++) {
					if (SDL_GameControllerNameForIndex(i) != NULL) {
						default_joystick = i;
						current_selected_joystick = i;
						break;
					}
				}
				break;
		}
	}
	return 0;
}

void ingame_gui_start(SDL_Renderer* renderer)
{
	int finished = 0;

	sdlRenderer = renderer;
	SDL_GetRendererOutputSize(sdlRenderer, &screenW, &screenH);

	screenshot =  SDL_CreateRGBSurface(0, screenW, screenH, 32, rmask, gmask, bmask, amask);
	SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888, screenshot->pixels, screenshot->pitch);
	screenshotTexture = SDL_CreateTextureFromSurface(renderer, screenshot);
	SDL_FreeSurface(screenshot);
	screenshot = NULL;

	title_texture_rect.x = 0;			// the x coordinate
	title_texture_rect.y = 0;			// the y coordinate
	title_texture_rect.w = screenW;		// the width of the texture
	title_texture_rect.h = screenH;		// the height of the texture

	UINT16 gameH = maxLinesMenu * 10 + 60;
	UINT16 gameW = gameH * screenW / screenH;

	dest_title_texture_rect.x = gameW * 2 / 5;	// the x coordinate
	dest_title_texture_rect.y = gameH / 6;	// the y coordinate
	dest_title_texture_rect.w = gameW / 3;	// the width of the texture
	dest_title_texture_rect.h = gameH / 3;	// the height of the texture
	
	// Set default joystick to use
	// When first joystick is removed "joystick 0" is no longer available
	for (int i = 0; (i < SDL_NumJoysticks()) && (i < MAX_JOYSTICKS); i++) {
		if (SDL_GameControllerNameForIndex(i) != NULL) {
			default_joystick = i;
			current_selected_joystick = i;
			break;
		}
	}

	SDL_JoystickEventState(SDL_ENABLE);
	SDL_GameControllerEventState(SDL_ENABLE);
	ingame_gui_init();

	while (!finished)
	{
		starting_stick = SDL_GetTicks();

		finished = ingame_gui_process();
		ingame_gui_render();
		// limit 5 FPS (free CPU usage)
		if ( ( 1000 / 5 ) > SDL_GetTicks() - starting_stick)
		{
			SDL_Delay( 1000 / 5 - ( SDL_GetTicks() - starting_stick ) );
		}
	}

	ingame_gui_exit();
	SDL_GameControllerEventState(SDL_IGNORE);
	SDL_JoystickEventState(SDL_IGNORE);
}
