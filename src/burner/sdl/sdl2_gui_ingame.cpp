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

#define MAINMENU 0
#define DIPMENU 1
#define DIPOPTIONSMENU 2
#define CONTROLLERMENU 3
#define MAPPINGMENU 4
#define CHEATMENU 5
#define CHEATOPTIONSMENU 6
#define SAVESTATE 7
#define LOADSTATE 9
#define SCREENSHOT 9
#define RESET 10

#define MAINMENU_COUNT 8
#define RESETMENU_COUNT 2

#define TITLELENGTH 31
char MenuTitle[TITLELENGTH + 1] = "FinalBurn Neo\0";

struct MenuItem
{
	const char* name;
	int (*menuFunction)();
	char* (*menuText)();
};

// menu item tracking
static UINT16 current_menu = MAINMENU;
static UINT16 current_selected_item = 0;
UINT16 firstMenuLine = 0;
UINT16 maxLinesMenu = 16;

int MainMenuSelected()
{
	snprintf(MenuTitle, TITLELENGTH, "FinalBurn Neo");
	current_selected_item = 0;
	current_menu = MAINMENU;
	return 0;
}


// Reset game related stuff
extern bool do_reset_game;

int ResetGameNow()
{
	do_reset_game = true;
	MainMenuSelected();
	return 1;
}

int ResetMenuSelected()
{
	snprintf(MenuTitle, TITLELENGTH, "Reset?");
	current_selected_item = 0;
	current_menu = RESET;
	return 0;
}

struct MenuItem ResetMenu[RESETMENU_COUNT] =
{
	{"Reset game now!\0", ResetGameNow, NULL},
	{"BACK\0", MainMenuSelected, NULL},
};


// DIP switches related stuff
static struct MenuItem dipMenu[MAXDIPSWITCHES + 2];			// Add two lines for RESET ALL SWITCHES and BACK
static struct MenuItem DIPOptionsMenu[MAXDIPOPTIONS + 1];	// Add one line for BACK
static UINT16 dipmenucount = 0;
UINT16 dipoptionscount = 0;
UINT16 current_selected_dipgroup = 0;
bool isDIPchanged[MAXDIPSWITCHES];							// Array to keed track of changed DIPs and use a different color in list
static char previousdrvnamedips[32];
int DIPMenuSelected();
extern struct GroupOfDIPSwitches GroupDIPSwitchesArray[MAXDIPSWITCHES];


int resetAllDIPSwitches()
{
	int i = 0;
	int j = 0;

	InpDIPSWResetDIPs();
	for (i = 0; i < MAXDIPSWITCHES; i++) isDIPchanged[i] = false;
	for (j = 0; j < dipmenucount - 2; j++) {
		for (i = 0; i < GroupDIPSwitchesArray[j].dipSwitch.nSetting; i++) {
			if (i == GroupDIPSwitchesArray[j].DefaultDIPOption) GroupDIPSwitchesArray[j].OptionsNamesWithCheckBoxes[i][1] = 'X';
			else GroupDIPSwitchesArray[j].OptionsNamesWithCheckBoxes[i][1] = ' ';
		}
	}
	do_reset_game = true;
	return 0;
}

int setDIPSwitch()
{
	if (GroupDIPSwitchesArray[current_selected_dipgroup].SelectedDIPOption != current_selected_item) {
		isDIPchanged[current_selected_dipgroup] = setDIPSwitchOption(current_selected_dipgroup, current_selected_item);
		do_reset_game = true;
	}
  return 0;
}

int DIPOptionsMenuSelected()
{
	current_selected_dipgroup = current_selected_item;
	snprintf(MenuTitle, TITLELENGTH, GroupDIPSwitchesArray[current_selected_dipgroup].dipSwitch.szText);
	current_selected_item = 0;
	current_menu = DIPOPTIONSMENU;

	for (int i = 0; i < GroupDIPSwitchesArray[current_selected_dipgroup].dipSwitch.nSetting; i++) {
		DIPOptionsMenu[i] = (MenuItem){GroupDIPSwitchesArray[current_selected_dipgroup].OptionsNamesWithCheckBoxes[i], setDIPSwitch, NULL};
	}
	DIPOptionsMenu[GroupDIPSwitchesArray[current_selected_dipgroup].dipSwitch.nSetting] = (MenuItem){"BACK\0", DIPMenuSelected, NULL};
	dipoptionscount = GroupDIPSwitchesArray[current_selected_dipgroup].dipSwitch.nSetting + 1;
	return 0;
}

int DIPMenuSelected()
{
	current_selected_item = 0;
	current_menu = DIPMENU;
	snprintf(MenuTitle, TITLELENGTH, "DIP Switches");
	const char* drvname = BurnDrvGetTextA(DRV_NAME);

	if (strcmp(previousdrvnamedips, drvname)) {			// It's a new or different game, rebuild GroupDIPSwitchesArray
		snprintf(previousdrvnamedips, 32, drvname);		// Save new game name
		dipmenucount = InpDIPSWCreate();
		int i = 0;
		while (i < dipmenucount) {
			dipMenu[i] = (MenuItem){GroupDIPSwitchesArray[i].dipSwitch.szText, DIPOptionsMenuSelected, NULL};
			isDIPchanged[i] = (GroupDIPSwitchesArray[i].DefaultDIPOption != GroupDIPSwitchesArray[i].SelectedDIPOption );
			i++;
		}
		if (dipmenucount > 0) {
			dipMenu[dipmenucount] = (MenuItem){"RESET ALL DIPSWITCHES\0", resetAllDIPSwitches, NULL};
			dipmenucount++;
			dipMenu[dipmenucount] = (MenuItem){"BACK\0", MainMenuSelected, NULL};
			dipmenucount++;
		} else {
			dipMenu[0] = (MenuItem){"BACK (no DIP switches found)\0", MainMenuSelected, NULL};
			dipmenucount = 1;
		}
	}
	return 0;
}


// Controllers stuff
#define MAX_JOYSTICKS 8
#define JOYSTICK_DEAD_ZONE 8000
#define BUTTONS_TO_MAP 8

struct MenuItem controllerMenu[MAX_JOYSTICKS + 1];	// One more for BACK
UINT16 controllermenucount = 0;
struct MenuItem mappingMenu[BUTTONS_TO_MAP + 3];	// Three more for RESET and SAVE and BACK
UINT16 current_selected_joystick = 0;
UINT16 default_joystick = 0;
const char* joystickNames[MAX_JOYSTICKS];

char buttonsList[BUTTONS_TO_MAP][64] = {
	"Button A (or weak kick):\0", 
	"Button B (or middle kick):\0", 
	"Button X (or weak punch):\0", 
	"Button Y (or middle punch):\0", 
	"Button LEFT_SHOULDER (or strong punch):\0", 
	"Button RIGHT_SHOULDER (or strong kick):\0", 
	"Button BACK (or COIN):\0", 
	"Button START:\0"};
char mappingMenuList[BUTTONS_TO_MAP][64];
int mappedbuttons[BUTTONS_TO_MAP] = {-1,-1,-1,-1,-1,-1,-1,-1};

int ControllerMenuSelected();
extern bool do_reload_game;	// To reload game when buttons mapping changed
static char* szSDLconfigPath = NULL;

int SaveMappedButtons()
{
	char guid_str[512];
	SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(current_selected_joystick), guid_str, sizeof(guid_str));

	char *gamecontrollerdbline = NULL;
	size_t len = 0;
	ssize_t nread;
	FILE* gamecontrollerdbfile;
	FILE* tempfile;
	bool buttonsaremapped = false, overwritedb = false;
	char* pos = NULL;
	char gamecontrollerdbpath[MAX_PATH] = { 0 };
	char tempgamecontrollerdbpath[MAX_PATH] = { 0 };
	if (szSDLconfigPath == NULL) {
		szSDLconfigPath = SDL_GetPrefPath("fbneo", "config");
	}
	snprintf(gamecontrollerdbpath, MAX_PATH, "%sgamecontrollerdb.txt", szSDLconfigPath);
	snprintf(tempgamecontrollerdbpath, MAX_PATH, "%sgamecontrollerdb.TEMP.txt", szSDLconfigPath);

	for (int i = 0; i < BUTTONS_TO_MAP; i++) {
		if (mappedbuttons[i] > -1) {
			buttonsaremapped = true;
			break;
		}
	}
	if (buttonsaremapped) {
		// Delete old mappings for this joystick that may exist in gamecontrollerdb.txt
		if (((gamecontrollerdbfile = fopen(gamecontrollerdbpath, "rt")) != NULL) && ((tempfile = fopen(tempgamecontrollerdbpath, "wt")) != NULL)) {
			while ((nread = getline(&gamecontrollerdbline, &len, gamecontrollerdbfile)) != -1) {
				pos = strstr(gamecontrollerdbline, guid_str);
				if (pos) overwritedb = true;	// Old mapping found, ignore line and overwrite gamecontrollerdb.txt
				else {
					if (gamecontrollerdbline[nread - 1] == '\n') fprintf(tempfile, "%s", gamecontrollerdbline);
					else fprintf(tempfile, "%s\n", gamecontrollerdbline);
				}
			}
			free(gamecontrollerdbline);
			fclose(tempfile);
			fclose(gamecontrollerdbfile);
			if (overwritedb) {
				remove(gamecontrollerdbpath);
				rename(tempgamecontrollerdbpath, gamecontrollerdbpath);
			}
			else remove(tempgamecontrollerdbpath);
		}
		// Add new mapping string to gamecontrollerdb.txt
		if ((gamecontrollerdbfile = fopen(gamecontrollerdbpath, "at")) != NULL) {
			fprintf(gamecontrollerdbfile, "%s,", guid_str);
			fprintf(gamecontrollerdbfile, "%s,", joystickNames[current_selected_joystick]);
			if (mappedbuttons[0] > -1) fprintf(gamecontrollerdbfile, "a:b%d,", mappedbuttons[0]);
			if (mappedbuttons[1] > -1) fprintf(gamecontrollerdbfile, "b:b%d,", mappedbuttons[1]);
			if (mappedbuttons[2] > -1) fprintf(gamecontrollerdbfile, "x:b%d,", mappedbuttons[2]);
			if (mappedbuttons[3] > -1) fprintf(gamecontrollerdbfile, "y:b%d,", mappedbuttons[3]);
			if (mappedbuttons[4] > -1) fprintf(gamecontrollerdbfile, "leftshoulder:b%d,", mappedbuttons[4]);
			if (mappedbuttons[5] > -1) fprintf(gamecontrollerdbfile, "rightshoulder:b%d,", mappedbuttons[5]);
			if (mappedbuttons[6] > -1) fprintf(gamecontrollerdbfile, "back:b%d,", mappedbuttons[6]);
			if (mappedbuttons[7] > -1) fprintf(gamecontrollerdbfile, "start:b%d,", mappedbuttons[7]);
#ifdef SDL_WINDOWS
			fprintf(gamecontrollerdbfile, "leftx:a0,lefty:a1,platform:Windows,\n");
#else
	#ifdef DARWIN
			fprintf(gamecontrollerdbfile, "leftx:a0,lefty:a1,platform:Mac OS X,\n");
	#else
			fprintf(gamecontrollerdbfile, "leftx:a0,lefty:a1,platform:Linux,\n");
	#endif
#endif
			fclose(gamecontrollerdbfile);
			printf("Saved mapping of \"%s\" in: %s\n", joystickNames[current_selected_joystick], gamecontrollerdbpath);
			SDL_GameControllerAddMappingsFromFile(gamecontrollerdbpath);	// Load updated mappings
			current_selected_joystick = default_joystick;	// Return control to first joystick
			MainMenuSelected();				// Go back to Main Menu
			SDL_Event sdlevent = {};		// Force exit and reload game to load new mapping
			sdlevent.type = SDL_KEYUP;
			sdlevent.key.keysym.sym = SDLK_F12;
			SDL_PushEvent(&sdlevent);
			do_reload_game = true;
			return 1;
		}
	}
	return 0;
}

int UpdateMappingMenuSelected()
{
	for (int i = 0; i < BUTTONS_TO_MAP; i++) {
		if (mappedbuttons[i] > -1) {
			snprintf(mappingMenuList[i], 64, "%s button %d", buttonsList[i], mappedbuttons[i]);
		} else snprintf(mappingMenuList[i], 64, "%s none", buttonsList[i]);
		mappingMenu[i] = (MenuItem){mappingMenuList[i], UpdateMappingMenuSelected, NULL};
	}
	return 0;
}

int ResetMappedButtons()
{
	SDL_GameControllerButtonBind bind;
	SDL_GameController *currentGameController = SDL_GameControllerOpen(current_selected_joystick);
	if (currentGameController)
	{
		bind = SDL_GameControllerGetBindForButton(currentGameController, SDL_CONTROLLER_BUTTON_A );
		mappedbuttons[0] = bind.value.button;
		bind = SDL_GameControllerGetBindForButton(currentGameController, SDL_CONTROLLER_BUTTON_B);
		mappedbuttons[1] = bind.value.button;
		bind = SDL_GameControllerGetBindForButton(currentGameController, SDL_CONTROLLER_BUTTON_X );
		mappedbuttons[2] = bind.value.button;
		bind = SDL_GameControllerGetBindForButton(currentGameController, SDL_CONTROLLER_BUTTON_Y);
		mappedbuttons[3] = bind.value.button;
		bind = SDL_GameControllerGetBindForButton(currentGameController, SDL_CONTROLLER_BUTTON_LEFTSHOULDER  );
		mappedbuttons[4] = bind.value.button;
		bind = SDL_GameControllerGetBindForButton(currentGameController, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER );
		mappedbuttons[5] = bind.value.button;
		bind = SDL_GameControllerGetBindForButton(currentGameController, SDL_CONTROLLER_BUTTON_BACK   );
		mappedbuttons[6] = bind.value.button;
		bind = SDL_GameControllerGetBindForButton(currentGameController, SDL_CONTROLLER_BUTTON_START  );
		mappedbuttons[7] = bind.value.button;
	} else for (int i = 0; i < BUTTONS_TO_MAP; i++) {
		mappedbuttons[i] = -1;
	}
	UpdateMappingMenuSelected();
	return 0;
}

// Use current_selected_item as the joystick index
int MappingMenuSelected()
{
	current_selected_joystick = current_selected_item;
	snprintf(MenuTitle, TITLELENGTH, joystickNames[current_selected_joystick]);
	current_selected_item = 0;
	current_menu = MAPPINGMENU;

	SDL_GameControllerButtonBind bind;
	SDL_GameController *currentGameController = SDL_GameControllerOpen(current_selected_joystick);
	if (currentGameController)
	{
		bind = SDL_GameControllerGetBindForButton(currentGameController, SDL_CONTROLLER_BUTTON_A );
		mappedbuttons[0] = bind.value.button;
		bind = SDL_GameControllerGetBindForButton(currentGameController, SDL_CONTROLLER_BUTTON_B);
		mappedbuttons[1] = bind.value.button;
		bind = SDL_GameControllerGetBindForButton(currentGameController, SDL_CONTROLLER_BUTTON_X );
		mappedbuttons[2] = bind.value.button;
		bind = SDL_GameControllerGetBindForButton(currentGameController, SDL_CONTROLLER_BUTTON_Y);
		mappedbuttons[3] = bind.value.button;
		bind = SDL_GameControllerGetBindForButton(currentGameController, SDL_CONTROLLER_BUTTON_LEFTSHOULDER  );
		mappedbuttons[4] = bind.value.button;
		bind = SDL_GameControllerGetBindForButton(currentGameController, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER );
		mappedbuttons[5] = bind.value.button;
		bind = SDL_GameControllerGetBindForButton(currentGameController, SDL_CONTROLLER_BUTTON_BACK   );
		mappedbuttons[6] = bind.value.button;
		bind = SDL_GameControllerGetBindForButton(currentGameController, SDL_CONTROLLER_BUTTON_START  );
		mappedbuttons[7] = bind.value.button;
	} else for (int i = 0; i < BUTTONS_TO_MAP; i++) {
		mappedbuttons[i] = -1;
	}

	for (int i = 0; i < BUTTONS_TO_MAP; i++) {
		if (mappedbuttons[i] > -1) {
			snprintf(mappingMenuList[i], 64, "%s button %d", buttonsList[i], mappedbuttons[i]);
		} else snprintf(mappingMenuList[i], 64, "%s none", buttonsList[i]);
		mappingMenu[i] = (MenuItem){mappingMenuList[i], UpdateMappingMenuSelected, NULL};
	}
	mappingMenu[BUTTONS_TO_MAP] = (MenuItem){"Save mapping (exit game to load new mapping)\0", SaveMappedButtons, NULL};
	mappingMenu[BUTTONS_TO_MAP + 1] = (MenuItem){"Reset mapping\0", ResetMappedButtons, NULL};
	mappingMenu[BUTTONS_TO_MAP + 2] = (MenuItem){"BACK\0", ControllerMenuSelected, NULL};
	
	SDL_GameControllerClose(currentGameController);		// Is this necessary?
	return 0;
}

int ControllerMenuSelected()
{
	snprintf(MenuTitle, TITLELENGTH, "Controller Options");
	current_selected_item = 0;
	current_menu = CONTROLLERMENU;
	controllermenucount = 0;
	current_selected_joystick = default_joystick;	// Return control to first joystick

	for (int i = 0; (i < SDL_NumJoysticks()) && (i < MAX_JOYSTICKS); i++) {
		joystickNames[i] = SDL_GameControllerNameForIndex(i);
		if (joystickNames[i] == NULL) joystickNames[i] = SDL_JoystickNameForIndex(i);	// Get joystick name if got no game controller name
		controllerMenu[i] = (MenuItem){joystickNames[i], MappingMenuSelected, NULL};
		controllermenucount++;
	}
	if (controllermenucount > 0) controllerMenu[controllermenucount] = (MenuItem){"BACK\0", MainMenuSelected, NULL};
	else controllerMenu[controllermenucount] = (MenuItem){"BACK (no controllers found)\0", MainMenuSelected, NULL};
	controllermenucount++;
  return 0;
}


// Cheats related stuff
static struct MenuItem cheatMenu[CHEAT_MAX_ADDRESS + 2];		// Add two lines for DISABLE ALL and for BACK
static struct MenuItem cheatOptionsMenu[CHEAT_MAX_OPTIONS + 1];		// Add one line for BACK
UINT16 cheatcount = 0;
UINT16 cheatoptionscount = 0;
UINT16 current_selected_cheat = 0;
bool isCheatActivated[CHEAT_MAX_ADDRESS]; // Array to keed track of activated cheats and use a different color in list
bool stayCurrentCheat = false;
CheatInfo* pCurrentCheat = NULL;
static char previousdrvnamecheats[32];
int CheatMenuSelected();
int CheatOptionsMenuSelected();

int SelectedCheatOption()
{
	CheatEnable(current_selected_cheat, current_selected_item);
	isCheatActivated[current_selected_cheat] = current_selected_item;
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
		pCurrentCheat = pCheatInfo;
		for (int i = 0; i < current_selected_cheat; i++) {
			pCurrentCheat = pCurrentCheat->pNext;
		}   // Skip to selected cheat number
	}

	cheatoptionscount = 0;
	for (int i = 0; pCurrentCheat->pOption[i] && (i < CHEAT_MAX_OPTIONS); i++) {
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
		cheatoptionscount++;
	}
	if (cheatoptionscount) {
		cheatOptionsMenu[cheatoptionscount] = (MenuItem){"BACK\0", CheatMenuSelected, NULL};
		cheatoptionscount++;
		current_menu = CHEATOPTIONSMENU;
		snprintf(MenuTitle, TITLELENGTH, pCurrentCheat->szCheatName);
	}
	return 0;
}

int DisableAllCheats()
{
	for (int i = 0; i < CHEAT_MAX_ADDRESS; i++) {
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
	int i = 0;

	snprintf(MenuTitle, TITLELENGTH, "Cheats");
	const char* drvname = BurnDrvGetTextA(DRV_NAME);

	if (strcmp(previousdrvnamecheats, drvname)) {		// It's a new or different game, rebuild cheatMenu
		snprintf(previousdrvnamecheats, 32, drvname);
		for (int i = 0; i < CHEAT_MAX_ADDRESS; i++) {
			isCheatActivated[i] = false;
		}
		cheatcount = 0;
		pCurrentCheat = pCheatInfo;
		while ((pCurrentCheat) && (i < CHEAT_MAX_ADDRESS)) {
			cheatMenu[i] = (MenuItem){pCurrentCheat->szCheatName, CheatOptionsMenuSelected, NULL};
			if (pCurrentCheat->nCurrent) isCheatActivated[i] = true;
			else isCheatActivated[i] = false;
			pCurrentCheat = pCurrentCheat->pNext;
			i++;
		}
		cheatMenu[i] = (MenuItem){"DISABLE ALL CHEATS\0", DisableAllCheats, NULL};
		i++;

		cheatMenu[i] = (MenuItem){"BACK\0", MainMenuSelected, NULL};
		cheatcount = i + 1;
	}
	return 0;
}

// Save state related stuff
int QuickSave()
{
	QuickState(1);
	return 1;
}

// Load state related stuff
int QuickLoad()
{
	QuickState(0);
	return 1;
}

// Main menu related stuff
int BackToGameSelected()
{
	return 1;
}

struct MenuItem mainMenu[MAINMENU_COUNT] =
{
	{"DIP Switches\0", DIPMenuSelected, NULL},
	{"Controller Options\0", ControllerMenuSelected, NULL},
	{"Cheats\0", CheatMenuSelected, NULL},
	{"Save State\0", QuickSave, NULL},
	{"Load State\0", QuickLoad, NULL},
	{"Save Screenshot\0", MakeScreenShot, NULL},
	{"Reset the game\0", ResetMenuSelected, NULL},
	{"Back to Game!\0", BackToGameSelected, NULL},
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
			current_item_count = dipmenucount;
			current_menu_items = dipMenu;
			break;
		case DIPOPTIONSMENU:
			current_item_count = dipoptionscount;
			current_menu_items = DIPOptionsMenu;
			break;
		case CONTROLLERMENU:
			current_item_count = controllermenucount;
			current_menu_items = controllerMenu;
			break;
		case MAPPINGMENU:
			current_item_count = BUTTONS_TO_MAP + 3;
			current_menu_items = mappingMenu;
			break;
		case CHEATMENU:
			current_item_count = cheatcount;
			current_menu_items = cheatMenu;
			break;
		case CHEATOPTIONSMENU:
			current_item_count = cheatoptionscount;
			current_menu_items = cheatOptionsMenu;
			break;
		case RESET:
			current_item_count = RESETMENU_COUNT;
			current_menu_items = ResetMenu;
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
		} else if ((current_menu == DIPMENU) && isDIPchanged[i]) {
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
					if ((current_menu == MAPPINGMENU) && (current_selected_item < BUTTONS_TO_MAP)) {
						mappedbuttons[current_selected_item] = e.jbutton.button;	// Store pressed button
						for (int i = 0; i < BUTTONS_TO_MAP; i++) {
							if ((i != current_selected_item) && (mappedbuttons[i] == mappedbuttons[current_selected_item]))
								mappedbuttons[i] = -1;		// Remove duplicated buttons
						}
					}
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
