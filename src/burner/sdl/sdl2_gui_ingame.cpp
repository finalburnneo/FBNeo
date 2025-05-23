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
#define PLAYERSANDJOYSTICKS 3
#define MAPPINGJOYSTICKMENU 4
#define MAPPINGPLAYERINPUTSMENU 5
#define DEFAULTPLAYERINPUTSMENU 6
#define AUTOFIREMENU 7
#define CHEATMENU 8
#define CHEATOPTIONSMENU 9
#define SAVESTATE 10
#define LOADSTATE 11
#define RESETMENU 12

#define TITLELENGTH 31
char MenuTitle[TITLELENGTH + 1] = "FinalBurn Neo";

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

#define RESETMENU_COUNT 2
int ResetMenuSelected()
{
	snprintf(MenuTitle, TITLELENGTH, "Reset?");
	current_selected_item = 0;
	current_menu = RESETMENU;
	return 0;
}

struct MenuItem ResetMenu[RESETMENU_COUNT] =
{
	{"Reset game now!", ResetGameNow, NULL},
	{"BACK", MainMenuSelected, NULL},
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
	snprintf(MenuTitle, TITLELENGTH, "%s", GroupDIPSwitchesArray[current_selected_dipgroup].dipSwitch.szText);
	current_selected_item = 0;
	current_menu = DIPOPTIONSMENU;

	for (int i = 0; i < GroupDIPSwitchesArray[current_selected_dipgroup].dipSwitch.nSetting; i++) {
		DIPOptionsMenu[i] = (MenuItem){GroupDIPSwitchesArray[current_selected_dipgroup].OptionsNamesWithCheckBoxes[i], setDIPSwitch, NULL};
	}
	DIPOptionsMenu[GroupDIPSwitchesArray[current_selected_dipgroup].dipSwitch.nSetting] = (MenuItem){"BACK", DIPMenuSelected, NULL};
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
		snprintf(previousdrvnamedips, 32, "%s", drvname);		// Save new game name
		dipmenucount = InpDIPSWCreate();
		int i = 0;
		while (i < dipmenucount) {
			dipMenu[i] = (MenuItem){GroupDIPSwitchesArray[i].dipSwitch.szText, DIPOptionsMenuSelected, NULL};
			isDIPchanged[i] = (GroupDIPSwitchesArray[i].DefaultDIPOption != GroupDIPSwitchesArray[i].SelectedDIPOption);
			i++;
		}
		if (dipmenucount > 0) {
			dipMenu[dipmenucount] = (MenuItem){"RESET ALL DIPSWITCHES", resetAllDIPSwitches, NULL};
			dipmenucount++;
			dipMenu[dipmenucount] = (MenuItem){"BACK", MainMenuSelected, NULL};
			dipmenucount++;
		} else {
			dipMenu[0] = (MenuItem){"BACK (no DIP switches found)", MainMenuSelected, NULL};
			dipmenucount = 1;
		}
	}
	return 0;
}

// Controllers stuff
#define MAX_JOYSTICKS 8
#define JOYSTICK_DEAD_ZONE 8000
#define BUTTONS_TO_MAP 8
#define MAX_PLAYERS 8

struct MenuItem playerAndJoystickMenu[MAX_JOYSTICKS + MAX_PLAYERS + 5];	// 4 player's defaults and 1 for BACK
UINT16 mappingmenucount = 0;
struct MenuItem mappingJoystickMenu[BUTTONS_TO_MAP * 2 + 4];	// 1 top "help text", 2 lines per button and 3 more for RESET and SAVE and BACK
UINT16 current_selected_joystick = 0;
UINT16 default_joystick = 0;
const char* JoystickNames[MAX_JOYSTICKS];

char playerNames[MAX_PLAYERS][9] = {
	"Player 1", 
	"Player 2", 
	"Player 3", 
	"Player 4", 
	"Player 5", 
	"Player 6", 
	"Player 7", 
	"Player 8"};

char buttonsList[BUTTONS_TO_MAP][41] = {
	"Button A (or weak kick)", 
	"Button B (or middle kick)", 
	"Button X (or weak punch)", 
	"Button Y (or middle punch)", 
	"Button LEFT_SHOULDER (or strong punch)", 
	"Button RIGHT_SHOULDER (or strong kick)", 
	"Button BACK (or COIN)", 
	"Button START"};
char mappingJoystickMenuList[BUTTONS_TO_MAP][41];
int mappedbuttons[BUTTONS_TO_MAP] = {-1,-1,-1,-1,-1,-1,-1,-1};

int lastTimestamp = 0;		// This is to avoid each analog joystick movement is registered multiple times
#define EVENTSTIMEOUT 250

void ingame_gui_render();
int MappingMenuSelected();
int UpdatePlayerInputsMappingMenu();
int UpdateMappingJoystickMenuSelected();
// extern bool do_reload_game;	// To reload game when buttons mapping changed
extern int getFBKfromScancode(int key);
extern int usejoy;		// Needed to remap joystick (globally)
extern INT32 MapJoystick(struct GameInp* pgi, char* szi, INT32 nPlayer, INT32 nDevice);
extern int buttons [4][8];

struct playerInputInfo {
	char szInfo[24];
	char szName[32];
	char MenuText[41];
	UINT8 nInput;		// GIT_SWITCH for game inputs or GIT_GROUP_MACRO
	int nCode;
	int inpIndex;
	bool autoFire;
};

#define MAX_DEFAULT_INPUTS 34
struct playerInputInfo PlayerDefaultInputs[4][MAX_DEFAULT_INPUTS] = {
	{
		{"p1 start", "", "", GIT_SWITCH, FBK_1, -1, false},
		{"p1 select", "", "", GIT_SWITCH, FBK_3, -1, false},
		{"p1 coin", "", "", GIT_SWITCH, FBK_5, -1, false},
		{"p1 up", "", "", GIT_SWITCH, FBK_UPARROW, -1, false},
		{"p1 down", "", "", GIT_SWITCH, FBK_DOWNARROW, -1, false},
		{"p1 left", "", "", GIT_SWITCH, FBK_LEFTARROW, -1, false},
		{"p1 right", "", "", GIT_SWITCH, FBK_RIGHTARROW, -1, false},
		{"p1 fire 1", "", "", GIT_SWITCH, FBK_Z, -1, false},
		{"p1 fire 2", "", "", GIT_SWITCH, FBK_X, -1, false},
		{"p1 fire 3", "", "", GIT_SWITCH, FBK_C, -1, false},
		{"p1 fire 4", "", "", GIT_SWITCH, FBK_A, -1, false},
		{"p1 fire 5", "", "", GIT_SWITCH, FBK_S, -1, false},
		{"p1 fire 6", "", "", GIT_SWITCH, FBK_D, -1, false},
		{"p1 fire 7", "", "", GIT_SWITCH, FBK_Q, -1, false},
		{"p1 fire 8", "", "", GIT_SWITCH, FBK_W, -1, false},
		{"p1 fire 9", "", "", GIT_SWITCH, FBK_E, -1, false},
		{"p1 fire 10", "", "", GIT_SWITCH, 0, -1, false},
		{"p1 fire 11", "", "", GIT_SWITCH, 0, -1, false},
		{"p1 fire 12", "", "", GIT_SWITCH, 0, -1, false},
		{"p1 fire 13", "", "", GIT_SWITCH, 0, -1, false},
		{"p1 fire 14", "", "", GIT_SWITCH, 0, -1, false},
		{"p1 fire 15", "", "", GIT_SWITCH, 0, -1, false},
		{"diag", "", "", GIT_SWITCH, FBK_F2, -1, false},
		{"reset", "", "", GIT_SWITCH, FBK_F3, -1, false},
		{"service", "", "", GIT_SWITCH, FBK_9, -1, false},
		{"service2", "", "", GIT_SWITCH, FBK_0, -1, false},
		{"service3", "", "", GIT_SWITCH, FBK_MINUS, -1, false},
		{"service4", "", "", GIT_SWITCH, FBK_EQUALS, -1, false},
		{"tilt", "", "", GIT_SWITCH, FBK_T, -1, false},
		{"swap", "", "", GIT_SWITCH, FBK_S, -1, false},
		{"op menu", "", "", GIT_SWITCH, FBK_F, -1, false},
		{"clear credit", "", "", GIT_SWITCH, FBK_G, -1, false},
		{"hopper", "", "", GIT_SWITCH, FBK_H, -1, false},
		{"", "", "", 0, 0, -1, false}
	}, {
		{"p2 start", "", "", GIT_SWITCH, FBK_2, -1, false},
		{"p2 select", "", "", GIT_SWITCH, FBK_4, -1, false},
		{"p2 coin", "", "", GIT_SWITCH, FBK_6, -1, false},
		{"p2 up", "", "", GIT_SWITCH, 0x4002, -1, false},
		{"p2 down", "", "", GIT_SWITCH, 0x4003, -1, false},
		{"p2 left", "", "", GIT_SWITCH, 0x4000, -1, false},
		{"p2 right", "", "", GIT_SWITCH, 0x4001, -1, false},
		{"p2 fire 1", "", "", GIT_SWITCH, 0x4080, -1, false},
		{"p2 fire 2", "", "", GIT_SWITCH, 0x4081, -1, false},
		{"p2 fire 3", "", "", GIT_SWITCH, 0x4082, -1, false},
		{"p2 fire 4", "", "", GIT_SWITCH, 0x4083, -1, false},
		{"p2 fire 5", "", "", GIT_SWITCH, 0x4084, -1, false},
		{"p2 fire 6", "", "", GIT_SWITCH, 0x4085, -1, false},
		{"p2 fire 7", "", "", GIT_SWITCH, 0, -1, false},
		{"p2 fire 8", "", "", GIT_SWITCH, 0, -1, false},
		{"p2 fire 9", "", "", GIT_SWITCH, 0, -1, false},
		{"p2 fire 10", "", "", GIT_SWITCH, 0, -1, false},
		{"p2 fire 11", "", "", GIT_SWITCH, 0, -1, false},
		{"p2 fire 12", "", "", GIT_SWITCH, 0, -1, false},
		{"p2 fire 13", "", "", GIT_SWITCH, 0, -1, false},
		{"p2 fire 14", "", "", GIT_SWITCH, 0, -1, false},
		{"p2 fire 15", "", "", GIT_SWITCH, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false}
	}, {
		{"p3 start", "", "", GIT_SWITCH, FBK_3, -1, false},
		{"p3 coin", "", "", GIT_SWITCH, FBK_7, -1, false},
		{"p3 up", "", "", GIT_SWITCH, 0x4102, -1, false},
		{"p3 down", "", "", GIT_SWITCH, 0x4103, -1, false},
		{"p3 left", "", "", GIT_SWITCH, 0x4100, -1, false},
		{"p3 right", "", "", GIT_SWITCH, 0x4101, -1, false},
		{"p3 fire 1", "", "", GIT_SWITCH, 0x4180, -1, false},
		{"p3 fire 2", "", "", GIT_SWITCH, 0x4181, -1, false},
		{"p3 fire 3", "", "", GIT_SWITCH, 0x4182, -1, false},
		{"p3 fire 4", "", "", GIT_SWITCH, 0x4183, -1, false},
		{"p3 fire 5", "", "", GIT_SWITCH, 0x4184, -1, false},
		{"p3 fire 6", "", "", GIT_SWITCH, 0x4185, -1, false},
		{"p3 fire 7", "", "", GIT_SWITCH, 0, -1, false},
		{"p3 fire 8", "", "", GIT_SWITCH, 0, -1, false},
		{"p3 fire 9", "", "", GIT_SWITCH, 0, -1, false},
		{"p3 fire 10", "", "", GIT_SWITCH, 0, -1, false},
		{"p3 fire 11", "", "", GIT_SWITCH, 0, -1, false},
		{"p3 fire 12", "", "", GIT_SWITCH, 0, -1, false},
		{"p3 fire 13", "", "", GIT_SWITCH, 0, -1, false},
		{"p3 fire 14", "", "", GIT_SWITCH, 0, -1, false},
		{"p3 fire 15", "", "", GIT_SWITCH, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false}
	}, {
		{"p4 start", "", "", GIT_SWITCH, FBK_4, -1, false},
		{"p4 coin", "", "", GIT_SWITCH, FBK_8, -1, false},
		{"p4 up", "", "", GIT_SWITCH, 0x4202, -1, false},
		{"p4 down", "", "", GIT_SWITCH, 0x4203, -1, false},
		{"p4 left", "", "", GIT_SWITCH, 0x4200, -1, false},
		{"p4 right", "", "", GIT_SWITCH, 0x4201, -1, false},
		{"p4 fire 1", "", "", GIT_SWITCH, 0x4280, -1, false},
		{"p4 fire 2", "", "", GIT_SWITCH, 0x4281, -1, false},
		{"p4 fire 3", "", "", GIT_SWITCH, 0x4282, -1, false},
		{"p4 fire 4", "", "", GIT_SWITCH, 0x4283, -1, false},
		{"p4 fire 5", "", "", GIT_SWITCH, 0x4284, -1, false},
		{"p4 fire 6", "", "", GIT_SWITCH, 0x4285, -1, false},
		{"p4 fire 7", "", "", GIT_SWITCH, 0, -1, false},
		{"p4 fire 8", "", "", GIT_SWITCH, 0, -1, false},
		{"p4 fire 9", "", "", GIT_SWITCH, 0, -1, false},
		{"p4 fire 10", "", "", GIT_SWITCH, 0, -1, false},
		{"p4 fire 11", "", "", GIT_SWITCH, 0, -1, false},
		{"p4 fire 12", "", "", GIT_SWITCH, 0, -1, false},
		{"p4 fire 13", "", "", GIT_SWITCH, 0, -1, false},
		{"p4 fire 14", "", "", GIT_SWITCH, 0, -1, false},
		{"p4 fire 15", "", "", GIT_SWITCH, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false}
	}
};

struct playerInputInfo PlayerDefaultMacros[4][6] = {
	{
		{"System SlowMo Normal", "", "", GIT_MACRO_AUTO, 0, -1, false},
		{"System SlowMo Slow", "", "", GIT_MACRO_AUTO, 0, -1, false},
		{"System SlowMo Slower", "", "", GIT_MACRO_AUTO, 0, -1, false},
		{"System SlowMo Slowest", "", "", GIT_MACRO_AUTO, 0, -1, false},
		{"System SlowMo Slowerest", "", "", GIT_MACRO_AUTO, 0, -1, false},
		{"", "", "", 0, 0, -1, false}
	}, {
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false}
	}, {
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false}
	}, {
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false},
		{"", "", "", 0, 0, -1, false}
	}
};

struct playerInputInfo playerControlsList[128];
struct MenuItem mappingPlayerInputsMenu[260];	// First line is "help", 2 per input and 3 more for RESET, SAVE and BACK
UINT16 mappingplayerinputsmenucount = 0;
UINT16 defaultplayerinputsmenucount = 0;
UINT8 selectedPlayer = 0;		// Player 1
UINT8 gamehw_cfg_index = 18;		// Last and null in gamehw_cfg array
static char previousdrvnameinputs[32];
char tempHardwareMenuText[41];
bool keepsearchinghwcfg = true;

int remDuplicatesPlayerControlListandUpdate(int currentInputSelected)
{
	if (playerControlsList[currentInputSelected].nInput == GIT_SWITCH) {
		int i = 0;
		while (playerControlsList[i].nInput) {
			if ((playerControlsList[i].nInput == GIT_SWITCH) && (i != currentInputSelected) && (playerControlsList[i].nCode == playerControlsList[currentInputSelected].nCode))
				playerControlsList[i].nCode = 0;		// Remove duplicated keys form inputs, not macros
			i++;
		}
	}
	return UpdatePlayerInputsMappingMenu();
}

int getPlayerGameInputs(UINT8 playernum, UINT16 previouscount)
{
	UINT16 totalcount = previouscount;
	int i, j;
	struct GameInp* pgi = NULL;
	for (j = 0, pgi = GameInp; j < nGameInpCount; j++, pgi++) {

		if (pgi->nInput != GIT_SWITCH) continue;
		struct BurnInputInfo bii;
		bii.szName = NULL;
		BurnDrvGetInputInfo(&bii, j);

		//	0 == Player 1   ;   255 == all players
		if (playernum != 255) {
			if ((bii.szName[0] == 'p' || bii.szName[0] == 'P') && (bii.szName[1] > '0') && (bii.szName[1] < '1' + nMaxPlayers) && (bii.szName[2] == ' ')) {
				if (bii.szName[1] != '1' + playernum) continue;		// Reject if not "playernum" input
			} else if (playernum) continue;		// Reject if not a specific player input and not player 1
		}

		// Search for input with same name
		i = 0;
		while ((i < totalcount) && (strcmp(playerControlsList[i].szInfo, bii.szInfo) || (playerControlsList[i].nInput != GIT_SWITCH))) i++;

		if (i == totalcount) {		// End of array, is unknown input? Add to end of array anyway...
			playerControlsList[i].nInput = GIT_SWITCH;
			snprintf(playerControlsList[i].szInfo, 24, "%s", bii.szInfo);
			playerControlsList[i].MenuText[0] = '\0';
			playerControlsList[i].nCode = pgi->Input.Switch.nCode;
			playerControlsList[i].autoFire = false;
			totalcount++;
		}
		snprintf(playerControlsList[i].szName, 32, "%s", bii.szName);
		playerControlsList[i].inpIndex = j;
	}

	playerControlsList[totalcount].nInput = 0;		// Make sure end of array is marked
	return totalcount;
}

int getPlayerMacros(UINT8 playernum, UINT16 previouscount)
{
	UINT16 totalcount = previouscount;
	int i, j;
	struct GameInp* pgi = NULL;
	for (j = 0, pgi = GameInp + nGameInpCount; j < nMacroCount; j++, pgi++) {
		if (pgi->nInput != GIT_MACRO_AUTO) continue;
		if ((pgi->Macro.nSysMacro == 1) && strncmp(pgi->Macro.szName, "System SlowMo", 13)) continue;

		//	0 == Player 1   ;   255 == all players
		if (playernum != 255) {
			if ((pgi->Macro.szName[0] == 'p' || pgi->Macro.szName[0] == 'P') && (pgi->Macro.szName[1] > '0') && (pgi->Macro.szName[1] < '1' + nMaxPlayers) && (pgi->Macro.szName[2] == ' ')) {
				if (pgi->Macro.szName[1] != '1' + playernum) continue;		// Reject if not "playernum" macro
			} else if (playernum) continue;		// Reject if not a specific player macro and not player 1
		}

		// Search for Macro with same name
		i = 0;
		while ((i < totalcount) && (strcmp(playerControlsList[i].szInfo, pgi->Macro.szName) || (playerControlsList[i].nInput != GIT_MACRO_AUTO))) i++;

		if (i == totalcount) {		// End of array, is unknown macro? Add to end of array anyway...
			playerControlsList[i].nInput = GIT_MACRO_AUTO;
			snprintf(playerControlsList[i].szInfo, 24, "%s", pgi->Macro.szName);
			snprintf(playerControlsList[i].szName, 32, "(macro) %s", pgi->Macro.szName);
			playerControlsList[i].MenuText[0] = '\0';
			if (pgi->Macro.nMode && pgi->Macro.Switch.nCode) playerControlsList[i].nCode = pgi->Macro.Switch.nCode;		// Is it enabled?
			else playerControlsList[i].nCode = 0;
			playerControlsList[i].autoFire = (pgi->Macro.nSysMacro == 15);
			totalcount++;
		}
		playerControlsList[i].inpIndex = j + nGameInpCount;
	}

	playerControlsList[totalcount].nInput = 0;		// Make sure end of array is marked
	return totalcount;
}

int SavePlayerInputsMapping()
{
	int i = 0;
	while (playerControlsList[i].nInput) {
		switch (playerControlsList[i].nInput) {
			case GIT_SWITCH:
				// GameInp[playerControlsList[i].inpIndex].nInput = GIT_SWITCH;
				GameInp[playerControlsList[i].inpIndex].Input.Switch.nCode = playerControlsList[i].nCode;
				break;
			case GIT_MACRO_AUTO:
				// GameInp[playerControlsList[i].inpIndex].nInput = GIT_MACRO_AUTO;
				if (playerControlsList[i].nCode > 0) {
					GameInp[playerControlsList[i].inpIndex].Macro.nMode = 1;
					GameInp[playerControlsList[i].inpIndex].Macro.Switch.nCode = playerControlsList[i].nCode;
				} else GameInp[playerControlsList[i].inpIndex].Macro.nMode = 0;
				if (playerControlsList[i].autoFire) GameInp[playerControlsList[i].inpIndex].Macro.nSysMacro = 15;
				else if (strncmp(playerControlsList[i].szInfo, "System SlowMo", 13)) GameInp[playerControlsList[i].inpIndex].Macro.nSysMacro = 0;
				else GameInp[playerControlsList[i].inpIndex].Macro.nSysMacro = 1;
				break;
		}
		i++;
	}
	bSaveInputs = true;		// Save this game inputs to romname.ini when we exit game
	return MappingMenuSelected();
}

SDL_Event e;		// Needed global to check if button/RETURN is hold
extern const Uint8* SDLinpKeyboardState;

int GetNewKeyButtonJoyToMap()
{
	bool mapdone = false;
	unsigned int delaytimer = 0;
	int currentInputSelected = (current_selected_item - 2) / 2;

	if ((e.type == SDL_KEYDOWN) && (e.key.keysym.scancode == SDL_SCANCODE_RETURN)) {
		delaytimer = SDL_GetTicks();
		while (SDLinpKeyboardState[SDL_SCANCODE_RETURN]) {		// Check if RETURN is hold pressed
			if (SDL_GetTicks() - delaytimer < 1300) {
				SDL_WaitEventTimeout(&e, 100);
			} else if (SDL_GetTicks() - delaytimer < 2500) {		// Eneble or disable macro auto fire if hold > 2 seconds
				 if (!mapdone) {
					playerControlsList[currentInputSelected].autoFire = !playerControlsList[currentInputSelected].autoFire;
					UpdatePlayerInputsMappingMenu();
					ingame_gui_render();
					mapdone = true;
				}
				SDL_WaitEventTimeout(&e, 100);
			} else {		// Delete input/macro key/button
				if (playerControlsList[currentInputSelected].nCode) {
					playerControlsList[currentInputSelected].nCode = 0;
					if (playerControlsList[currentInputSelected].nInput == GIT_MACRO_AUTO) playerControlsList[currentInputSelected].autoFire = 0;
					UpdatePlayerInputsMappingMenu();
					ingame_gui_render();
				}
				mapdone = true;
				SDL_WaitEventTimeout(&e, 100);
			}
		}
	} else if (e.type == SDL_JOYBUTTONDOWN) {
		delaytimer = SDL_GetTicks();
		int pressedbutton = e.jbutton.button;
		SDL_Joystick* joystick = SDL_JoystickFromInstanceID(SDL_JoystickGetDeviceInstanceID(e.jbutton.which));
		while (SDL_JoystickGetButton(joystick, pressedbutton)) {		// Check if button is hold pressed
			if (SDL_GetTicks() - delaytimer < 1300) {
				SDL_WaitEventTimeout(&e, 100);
			} else if (SDL_GetTicks() - delaytimer < 2500) {		// Eneble or disable macro auto fire if hold > 2 seconds
				 if (!mapdone) {
					playerControlsList[currentInputSelected].autoFire = !playerControlsList[currentInputSelected].autoFire;
					UpdatePlayerInputsMappingMenu();
					ingame_gui_render();
					mapdone = true;
				}
				SDL_WaitEventTimeout(&e, 100);
			} else {		// Delete input/macro key/button
				if (playerControlsList[currentInputSelected].nCode) {
					playerControlsList[currentInputSelected].nCode = 0;
					if (playerControlsList[currentInputSelected].nInput == GIT_MACRO_AUTO) playerControlsList[currentInputSelected].autoFire = 0;
					UpdatePlayerInputsMappingMenu();
					ingame_gui_render();
				}
				mapdone = true;
				SDL_WaitEventTimeout(&e, 100);
			}
		}
	}
	if (!mapdone) {
		playerControlsList[currentInputSelected].nCode = -1;
		UpdatePlayerInputsMappingMenu();
		ingame_gui_render();
		SDL_FlushEvents(SDL_KEYDOWN, SDL_CONTROLLERBUTTONUP);
		while (!mapdone) {
			if (SDL_WaitEventTimeout(&e, 1000)) {
				INT32 nJoyBase = 0x4000;
				switch (e.type)
				{
					case SDL_QUIT:
						return 1;
					case SDL_KEYDOWN:
						if (!e.key.repeat && (e.key.keysym.scancode != SDL_SCANCODE_TAB) && (e.key.keysym.scancode != SDL_SCANCODE_RETURN) && (getFBKfromScancode(e.key.keysym.scancode) > 0)) {
							playerControlsList[currentInputSelected].nCode = getFBKfromScancode(e.key.keysym.scancode);		// Store pressed key
							mapdone = true;
						}
						break;
					case SDL_JOYAXISMOTION:
						if ((abs(e.jaxis.value) > JOYSTICK_DEAD_ZONE)) {
							nJoyBase |= e.jaxis.which << 8;
							nJoyBase |= e.jaxis.axis << 1;
							if (e.jaxis.value > 0) nJoyBase++;
							playerControlsList[currentInputSelected].nCode = nJoyBase;		// Store mapped joystick
							lastTimestamp = e.jaxis.timestamp;
							mapdone = true;
						}
						break;
					case SDL_JOYHATMOTION:
						if (e.jhat.value > 0) {
							mapdone = true;
							nJoyBase |= e.jhat.which << 8;
							nJoyBase |= e.jhat.hat << 2;
							// Store pressed D-PAD/HAT
							switch (e.jhat.value)
							{
								case SDL_HAT_LEFT:
									playerControlsList[currentInputSelected].nCode = nJoyBase + 0x10;
									break;
								case SDL_HAT_RIGHT:
									playerControlsList[currentInputSelected].nCode = nJoyBase + 0x10 + 1;
									break;
								case SDL_HAT_UP:
									playerControlsList[currentInputSelected].nCode = nJoyBase + 0x10 + 2;
									break;
								case SDL_HAT_DOWN:
									playerControlsList[currentInputSelected].nCode = nJoyBase + 0x10 + 3;
									break;
							}
						}
						break;
					case SDL_JOYBUTTONDOWN:
						nJoyBase |= e.jbutton.which << 8;
						playerControlsList[currentInputSelected].nCode = nJoyBase + 0x80 + e.jbutton.button;		// Store mapped joystick button
						mapdone = true;
						break;
					case SDL_CONTROLLERDEVICEREMOVED:
					case SDL_JOYDEVICEREMOVED:
						// Try not to lose total control but this is not great solution
						for (int i = 0; (i < SDL_NumJoysticks()) && (i < MAX_JOYSTICKS); i++) {
							if (SDL_GameControllerNameForIndex(i)) {
								default_joystick = i;
								current_selected_joystick = i;
								break;
							}
						}
						break;
				}
			}
		}
		return remDuplicatesPlayerControlListandUpdate(currentInputSelected);
	}
	return 0;
}

int InitPlayerDefaultInputs(UINT8 selectedPlayer)
{
	int i = 0;
	UINT16 totalcount = 0;
	// Copy default array to inputs list
	while (PlayerDefaultInputs[selectedPlayer][totalcount].nInput) {
		snprintf(playerControlsList[totalcount].szInfo, 24, "%s", PlayerDefaultInputs[selectedPlayer][totalcount].szInfo);
		snprintf(playerControlsList[totalcount].szName, 32, "%s", PlayerDefaultInputs[selectedPlayer][totalcount].szInfo);
		playerControlsList[totalcount].MenuText[0] = '\0';
		playerControlsList[totalcount].nInput = GIT_SWITCH;
		playerControlsList[totalcount].nCode = PlayerDefaultInputs[selectedPlayer][totalcount].nCode;
		playerControlsList[totalcount].inpIndex = -1;
		playerControlsList[totalcount].autoFire = false;
		totalcount++;
	}

	// Update inputs list with defaults from p?defaults.ini
	char szLine[1024];
	INT32 nFileVersion = 0;
	char* szValue;
	INT32 nLen;
	FILE* h = _tfopen(szPlayerDefaultIni[selectedPlayer], _T("rt"));
	if (h) {
		// Go through each line of the config file and process inputs
		while (fgets(szLine, 1024, h)) {

			// Get rid of the linefeed and carriage return at the end
			nLen = strlen(szLine);
			if (nLen > 0 && szLine[nLen - 1] == 10) {
				szLine[nLen - 1] = 0;
				nLen--;
			}
			if (nLen > 0 && szLine[nLen - 1] == 13) {
				szLine[nLen - 1] = 0;
				nLen--;
			}

			szValue = LabelCheck(szLine, "version");
			if (szValue) nFileVersion = strtol(szValue, NULL, 0);

			if (nConfigMinVersion <= nFileVersion && nFileVersion <= nBurnVer) {
				szValue = LabelCheck(szLine, "input");
				if (szValue) {
					char* szQuote = NULL;
					char* szEnd = NULL;
					if (QuoteRead(&szQuote, &szEnd, szValue)) continue;

					if ((szQuote[0] == 'p' || szQuote[0] == 'P') && (szQuote[1] > '0') && (szQuote[1] < '1' + nMaxPlayers) && (szQuote[2] == ' ')) {
						if (szQuote[1] != '1' + selectedPlayer) continue;		// Reject if not "selectedPlayer" input
					} else if (selectedPlayer) continue;		// Reject if not a player input and not player 1

					// Search for input with same name
					i = 0;
					while ((i < totalcount) && (strcmp(playerControlsList[i].szInfo, szQuote) || (playerControlsList[i].nInput != GIT_SWITCH))) i++;

					if (i < totalcount) {
						szQuote = LabelCheck(szEnd, "switch");
						if (szQuote) playerControlsList[i].nCode = (UINT16)strtol(szQuote, NULL, 0);
					} else {		// End of array, is unknown input? Add to end of array anyway...
						playerControlsList[i].nInput = GIT_SWITCH;
						snprintf(playerControlsList[i].szInfo, 24, "%s", szQuote);
						snprintf(playerControlsList[i].szName, 32, "%s", szQuote);
						playerControlsList[i].MenuText[0] = '\0';
						szQuote = LabelCheck(szEnd, "switch");
						if (szQuote) playerControlsList[i].nCode = (UINT16)strtol(szQuote, NULL, 0);
						else playerControlsList[i].nCode = 0;
						playerControlsList[i].inpIndex = -1;
						playerControlsList[i].autoFire = false;
						totalcount++;
					}
				}
			}
		}
	}

	// Update inputs list with custom names and inputs used in current game
	totalcount = getPlayerGameInputs(selectedPlayer, totalcount);

	// Now add macros to playerControlsList
	i = 0;
	while (PlayerDefaultMacros[selectedPlayer][i].nInput) {
		snprintf(playerControlsList[totalcount].szInfo, 24, "%s", PlayerDefaultMacros[selectedPlayer][i].szInfo);
		snprintf(playerControlsList[totalcount].szName, 32, "(macro) %s", PlayerDefaultMacros[selectedPlayer][i].szInfo);
		playerControlsList[totalcount].MenuText[0] = '\0';
		playerControlsList[totalcount].nInput = GIT_MACRO_AUTO;
		playerControlsList[totalcount].nCode = PlayerDefaultMacros[selectedPlayer][i].nCode;
		playerControlsList[totalcount].inpIndex = -1;
		playerControlsList[totalcount].autoFire = false;
		i++;
		totalcount++;
	}

	// Update macros list with defaults from p?defaults.ini
	if (h) {
		rewind(h);
		// Go through each line of the config file and process macros
		while (fgets(szLine, 1024, h)) {

			// Get rid of the linefeed and carriage return at the end
			nLen = strlen(szLine);
			if (nLen > 0 && szLine[nLen - 1] == 10) {
				szLine[nLen - 1] = 0;
				nLen--;
			}
			if (nLen > 0 && szLine[nLen - 1] == 13) {
				szLine[nLen - 1] = 0;
				nLen--;
			}

			szValue = LabelCheck(szLine, "version");
			if (szValue) nFileVersion = strtol(szValue, NULL, 0);

			if (nConfigMinVersion <= nFileVersion && nFileVersion <= nBurnVer) {
				szValue = LabelCheck(szLine, "macro");
				if (szValue) {
					char* szQuote = NULL;
					char* szEnd = NULL;
					if (QuoteRead(&szQuote, &szEnd, szValue)) continue;

					if ((szQuote[0] == 'p' || szQuote[0] == 'P') && (szQuote[1] > '0') && (szQuote[1] < '1' + nMaxPlayers) && (szQuote[2] == ' ')) {
						if (szQuote[1] != '1' + selectedPlayer) continue;		// Reject if not "selectedPlayer" macro
					} else if (selectedPlayer) continue;		// Reject if not a player macro and not player 1

					// Search for macro with same name
					i = 0;
					while ((i < totalcount) && (strcmp(playerControlsList[i].szInfo, szQuote) || (playerControlsList[i].nInput != GIT_MACRO_AUTO))) i++;

					if (i < totalcount) {
						szQuote = LabelCheck(szEnd, "switch");
						if (szQuote) playerControlsList[i].nCode = (UINT16)strtol(szQuote, NULL, 0);
					} else {		// End of array, is unknown macro? Add to end of array anyway...
						playerControlsList[i].nInput = GIT_MACRO_AUTO;
						snprintf(playerControlsList[i].szInfo, 24, "%s", szQuote);
						snprintf(playerControlsList[i].szName, 32, "(macro) %s", szQuote);
						playerControlsList[i].MenuText[0] = '\0';
						szQuote = LabelCheck(szEnd, "switch");
						if (szQuote) playerControlsList[i].nCode = (UINT16)strtol(szQuote, NULL, 0);
						else playerControlsList[i].nCode = 0;
						playerControlsList[i].inpIndex = -1;
						playerControlsList[i].autoFire = false;
						totalcount++;
					}
				}
			}
		}
	}

	// Update list with macros used in current game
	totalcount = getPlayerMacros(selectedPlayer, totalcount);

	// Update auto fire with defaults from p?defaults.ini
	if (h) {
		rewind(h);
		// Go through each line of the config file and process afire
		while (fgets(szLine, 1024, h)) {

			// Get rid of the linefeed and carriage return at the end
			nLen = strlen(szLine);
			if (nLen > 0 && szLine[nLen - 1] == 10) {
				szLine[nLen - 1] = 0;
				nLen--;
			}
			if (nLen > 0 && szLine[nLen - 1] == 13) {
				szLine[nLen - 1] = 0;
				nLen--;
			}

			szValue = LabelCheck(szLine, "version");
			if (szValue) nFileVersion = strtol(szValue, NULL, 0);

			if (nConfigMinVersion <= nFileVersion && nFileVersion <= nBurnVer) {
				szValue = LabelCheck(szLine, "afire");
				if (szValue) {
					char* szQuote = NULL;
					char* szEnd = NULL;
					if (QuoteRead(&szQuote, &szEnd, szValue)) continue;

					if ((szQuote[0] == 'p' || szQuote[0] == 'P') && (szQuote[1] > '0') && (szQuote[1] < '1' + nMaxPlayers) && (szQuote[2] == ' ')) {
						if (szQuote[1] != '1' + selectedPlayer) continue;		// Reject if not "selectedPlayer" afire
					} else if (selectedPlayer) continue;		// Reject if not a player afire and not player 1

					// Search for macro with same name
					i = 0;
					while ((i < totalcount) && (strcmp(playerControlsList[i].szInfo, szQuote) || (playerControlsList[i].nInput != GIT_MACRO_AUTO))) i++;

					if (i == totalcount) {		// End of array, is unknown macro? Add to end of array anyway...
						playerControlsList[i].nInput = GIT_MACRO_AUTO;
						snprintf(playerControlsList[i].szInfo, 24, "%s", szQuote);
						snprintf(playerControlsList[i].szName, 32, "(macro) %s", szQuote);
						playerControlsList[i].MenuText[0] = '\0';
						playerControlsList[i].nCode = 0;
						playerControlsList[i].inpIndex = -1;
						totalcount++;
					}
					playerControlsList[i].autoFire = true;
				}
			}
		}
		fclose(h);
	}

	playerControlsList[totalcount].nInput = 0;		// Make sure end of array is marked
	return totalcount;
}

int SavePlayerDefaultMapping()
{
	FILE* h;

	h = _tfopen(szPlayerDefaultIni[selectedPlayer], _T("wt"));
	if (h) {
		printf("Saving default mapping for %s in \"%s\"\n", playerNames[selectedPlayer], szPlayerDefaultIni[selectedPlayer]);
		// Write title
		fprintf(h, "// %s v%s --- Default Inputs for %s\n\n", APP_TITLE, szAppBurnVer, playerNames[selectedPlayer]);

		// Write version number
		fprintf(h, "version 0x%06X\n\n", nBurnVer);

		fprintf(h, "\n\n\n");
		fprintf(h, "// --- Inputs -----------------------------------------------------------------\n\n");

		// Write inputs and macros to file and also activate them in current game
		int i = 0;
		while (playerControlsList[i].nInput) {
			switch (playerControlsList[i].nInput) {
				case GIT_SWITCH:
					if (playerControlsList[i].nCode > 0) fprintf(h, "input \"%s\" switch 0x%.2X\n", playerControlsList[i].szInfo, playerControlsList[i].nCode);
					else fprintf(h, "input \"%s\" undefined\n", playerControlsList[i].szInfo);
					if (playerControlsList[i].inpIndex > -1) {
//						GameInp[playerControlsList[i].inpIndex].nInput = GIT_SWITCH;
						GameInp[playerControlsList[i].inpIndex].Input.Switch.nCode = playerControlsList[i].nCode;
					}
					break;
				case GIT_MACRO_AUTO:
					if (playerControlsList[i].nCode > 0) {
						fprintf(h, "macro \"%s\" switch 0x%.2X\n", playerControlsList[i].szInfo, playerControlsList[i].nCode);
						if (playerControlsList[i].autoFire) fprintf(h, "afire \"%s\"\n", playerControlsList[i].szInfo);
					} else fprintf(h, "macro \"%s\" undefined\n", playerControlsList[i].szInfo);
					if (playerControlsList[i].inpIndex > -1) {
//						GameInp[playerControlsList[i].inpIndex].nInput = GIT_MACRO_AUTO;
						if (playerControlsList[i].nCode > 0) GameInp[playerControlsList[i].inpIndex].Macro.nMode = 1;
						else GameInp[playerControlsList[i].inpIndex].Macro.nMode = 0;
						GameInp[playerControlsList[i].inpIndex].Macro.Switch.nCode = playerControlsList[i].nCode;
						if (playerControlsList[i].autoFire) GameInp[playerControlsList[i].inpIndex].Macro.nSysMacro = 15;
						else if (strncmp(playerControlsList[i].szInfo, "System SlowMo", 13)) GameInp[playerControlsList[i].inpIndex].Macro.nSysMacro = 0;
						else GameInp[playerControlsList[i].inpIndex].Macro.nSysMacro = 1;
					}
					break;
			}
			i++;
		}

		fprintf(h, "\n");
		fclose(h);
		bSaveInputs = true;		// Save this game inputs to romname.ini when we exit game
		return MappingMenuSelected();
	} else return 1;
}

int ResetPlayerDefaultsMenuSelected()
{
	InitPlayerDefaultInputs(selectedPlayer);
	UpdatePlayerInputsMappingMenu();
	current_selected_item = 0;
	return 0;
}

int PlayerDefaultsMenuSelected()
{
	selectedPlayer =  current_selected_item + 6 - mappingmenucount;
	if (keepsearchinghwcfg) selectedPlayer--;
	snprintf(MenuTitle, TITLELENGTH, "Defaults for %s", playerNames[selectedPlayer]);
	current_menu = DEFAULTPLAYERINPUTSMENU;
	current_selected_item = 0;

	defaultplayerinputsmenucount = InitPlayerDefaultInputs(selectedPlayer);		// Number of inputs+macros, not menu rows
	UpdatePlayerInputsMappingMenu();

	mappingPlayerInputsMenu[0] = (MenuItem){"(Press ENTER or Joy button)", NULL, NULL};

	int i = 0;
	while (i < defaultplayerinputsmenucount) {
		mappingPlayerInputsMenu[2 * i + 1] = (MenuItem){playerControlsList[i].szName, NULL, NULL};
		mappingPlayerInputsMenu[2 * i + 2] = (MenuItem){playerControlsList[i].MenuText, GetNewKeyButtonJoyToMap, NULL};
		i++;
	}
	defaultplayerinputsmenucount = defaultplayerinputsmenucount * 2 + 1;

	mappingPlayerInputsMenu[defaultplayerinputsmenucount] = (MenuItem){"RESET MAPPING TO DEFAULTS", ResetPlayerDefaultsMenuSelected, NULL};
	defaultplayerinputsmenucount++;
	mappingPlayerInputsMenu[defaultplayerinputsmenucount] = (MenuItem){"SAVE MAPPING AS DEFAULTS", SavePlayerDefaultMapping, NULL};
	defaultplayerinputsmenucount++;
	mappingPlayerInputsMenu[defaultplayerinputsmenucount] = (MenuItem){"BACK (WITHOUT SAVING)", MappingMenuSelected, NULL};
	defaultplayerinputsmenucount++;
	return 0;
}

int SaveHardwareDefaultMapping()
{
	TCHAR *szFolderName = NULL;
	TCHAR szFileName[MAX_PATH] = _T("");
	szFolderName = SDL_GetPrefPath(NULL, "fbneo");		// Get fbneo folder path
	_stprintf(szFileName, _T("%s%s"), szFolderName, gamehw_cfg[gamehw_cfg_index].ini);
	SDL_free(szFolderName);

	FILE* h = _tfopen(szFileName, _T("wt"));
	if (h) {
		printf("Saving default mapping for %s in \"%s\"\n", gamehw_cfg[gamehw_cfg_index].system, szFileName);
		fprintf(h, "// %s v%s --- Default Inputs for %s\n\n", APP_TITLE, szAppBurnVer, gamehw_cfg[gamehw_cfg_index].system);
		fprintf(h, "version 0x%06X\n\n", nBurnVer);
		fprintf(h, "// --- Inputs -----------------------------------------------------------------\n\n");
		int i = 0;
		while (playerControlsList[i].nInput) {
			switch (playerControlsList[i].nInput)
			{
				case GIT_SWITCH:
					if (playerControlsList[i].nCode > 0) fprintf(h, "input \"%s\" switch 0x%.2X\n", playerControlsList[i].szInfo, playerControlsList[i].nCode);
					else fprintf(h, "input \"%s\" undefined\n", playerControlsList[i].szInfo);
					if (playerControlsList[i].inpIndex > -1) {
//						GameInp[playerControlsList[i].inpIndex].nInput = GIT_SWITCH;
						GameInp[playerControlsList[i].inpIndex].Input.Switch.nCode = playerControlsList[i].nCode;
					}
					break;
				case GIT_MACRO_AUTO:
					if (playerControlsList[i].nCode > 0) {
						fprintf(h, "macro \"%s\" switch 0x%.2X\n", playerControlsList[i].szInfo, playerControlsList[i].nCode);
						if (playerControlsList[i].autoFire) fprintf(h, "afire \"%s\"\n", playerControlsList[i].szInfo);
					} else fprintf(h, "macro \"%s\" undefined\n", playerControlsList[i].szInfo);
					if (playerControlsList[i].inpIndex > -1) {
//						GameInp[playerControlsList[i].inpIndex].nInput = GIT_MACRO_AUTO;
						if (playerControlsList[i].nCode > 0) GameInp[playerControlsList[i].inpIndex].Macro.nMode = 1;
						else GameInp[playerControlsList[i].inpIndex].Macro.nMode = 0;
						GameInp[playerControlsList[i].inpIndex].Macro.Switch.nCode = playerControlsList[i].nCode;
						if (playerControlsList[i].autoFire) GameInp[playerControlsList[i].inpIndex].Macro.nSysMacro = 15;
						else if (strncmp(playerControlsList[i].szInfo, "System SlowMo", 13)) GameInp[playerControlsList[i].inpIndex].Macro.nSysMacro = 0;
						else GameInp[playerControlsList[i].inpIndex].Macro.nSysMacro = 1;
					}
					break;
			}
			i++;
		}
		fprintf(h, "\n");
		fclose(h);
		bSaveInputs = true;		// Save this game inputs to romname.ini when we exit game
		return MappingMenuSelected();
	} else {
		return 1;
	}
}

int UpdatePlayerInputsMappingMenu()
{
	int i = 0;
	while (playerControlsList[i].nInput) {
		if (playerControlsList[i].nCode > 0) snprintf(playerControlsList[i].MenuText, 41, "%s %s", (playerControlsList[i].nInput == GIT_MACRO_AUTO ? "[ ]" : ">>>"), InputCodeDesc(playerControlsList[i].nCode));
		else if (playerControlsList[i].nCode == 0) snprintf(playerControlsList[i].MenuText, 41, "%s none", (playerControlsList[i].nInput == GIT_MACRO_AUTO ? "[ ]" : ">>>"));
		else snprintf(playerControlsList[i].MenuText, 41, "%s PRESS KEY/BUTTON or MOVE JOY/DPAD", (playerControlsList[i].nInput == GIT_MACRO_AUTO ? "[ ]" : ">>>"));
		if (playerControlsList[i].autoFire && (playerControlsList[i].nInput == GIT_MACRO_AUTO)) playerControlsList[i].MenuText[1] = 'X';
		mappingPlayerInputsMenu[i * 2 + 2] = (MenuItem){playerControlsList[i].MenuText, GetNewKeyButtonJoyToMap, NULL};
		i++;
	}
	return 0;		// Return number of inputs/macros
}

int ResetMappingPlayerInputsSelected()
{
	getPlayerMacros(selectedPlayer, getPlayerGameInputs(selectedPlayer, 0));
	UpdatePlayerInputsMappingMenu();
	return 0;
}

int MappingPlayerInputsSelected()
{
	snprintf(MenuTitle, TITLELENGTH, "%s", playerNames[current_selected_item]);
	selectedPlayer = current_selected_item;
	current_selected_item = 0;
	current_menu = MAPPINGPLAYERINPUTSMENU;

	mappingplayerinputsmenucount = getPlayerMacros(selectedPlayer, getPlayerGameInputs(selectedPlayer, 0));		// Get player macros after player inputs
	UpdatePlayerInputsMappingMenu();

	mappingPlayerInputsMenu[0] = (MenuItem){"(Press ENTER or Joy button)", NULL, NULL};
	int i = 0;
	while (i < mappingplayerinputsmenucount) {
		mappingPlayerInputsMenu[2 * i + 1] = (MenuItem){playerControlsList[i].szName, NULL, NULL};
		mappingPlayerInputsMenu[2 * i + 2] = (MenuItem){playerControlsList[i].MenuText, GetNewKeyButtonJoyToMap, NULL};
		i++;
	}
	mappingplayerinputsmenucount = mappingplayerinputsmenucount * 2 + 1;

	mappingPlayerInputsMenu[mappingplayerinputsmenucount] = (MenuItem){"RESET MAPPING", ResetMappingPlayerInputsSelected, NULL};
	mappingplayerinputsmenucount++;
	mappingPlayerInputsMenu[mappingplayerinputsmenucount] = (MenuItem){"SAVE MAPPING (FOR THIS GAME)", SavePlayerInputsMapping, NULL};
	mappingplayerinputsmenucount++;
	mappingPlayerInputsMenu[mappingplayerinputsmenucount] = (MenuItem){"BACK (WITHOUT SAVING)", MappingMenuSelected, NULL};
	mappingplayerinputsmenucount++;
	return 0;
}

int InitHardwareDefaultInputs()
{
	int i = 0;
	// Get inputs list used in current game
	UINT16 totalcount = getPlayerGameInputs(255, 0);

	TCHAR *szFolderName = NULL;
	TCHAR szFileName[MAX_PATH] = _T("");
	szFolderName = SDL_GetPrefPath(NULL, "fbneo");		// Get fbneo folder path
	_stprintf(szFileName, _T("%s%s"), szFolderName, gamehw_cfg[gamehw_cfg_index].ini);
	SDL_free(szFolderName);

	// Update inputs list with defaults from presets/*.ini
	char szLine[1024];
	INT32 nFileVersion = 0;
	char* szValue;
	INT32 nLen;
	FILE* h = _tfopen(szFileName, _T("rt"));
	if (h) {
		// Go through each line of the config file and process inputs
		while (fgets(szLine, 1024, h)) {

			// Get rid of the linefeed and carriage return at the end
			nLen = strlen(szLine);
			if (nLen > 0 && szLine[nLen - 1] == 10) {
				szLine[nLen - 1] = 0;
				nLen--;
			}
			if (nLen > 0 && szLine[nLen - 1] == 13) {
				szLine[nLen - 1] = 0;
				nLen--;
			}

			szValue = LabelCheck(szLine, "version");
			if (szValue) nFileVersion = strtol(szValue, NULL, 0);

			if (nConfigMinVersion <= nFileVersion && nFileVersion <= nBurnVer) {
				szValue = LabelCheck(szLine, "input");
				if (szValue) {
					char* szQuote = NULL;
					char* szEnd = NULL;
					if (QuoteRead(&szQuote, &szEnd, szValue)) continue;

					// Search for input with same name
					i = 0;
					while ((i < totalcount) && (strcmp(playerControlsList[i].szInfo, szQuote) || (playerControlsList[i].nInput != GIT_SWITCH))) i++;

					if (i < totalcount) {
						szQuote = LabelCheck(szEnd, "switch");
						if (szQuote) playerControlsList[i].nCode = (UINT16)strtol(szQuote, NULL, 0);
					} else {		// End of array, is unknown input? Add to end of array anyway...
						playerControlsList[i].nInput = GIT_SWITCH;
						snprintf(playerControlsList[i].szInfo, 24, "%s", szQuote);
						snprintf(playerControlsList[i].szName, 32, "%s", szQuote);
						playerControlsList[i].MenuText[0] = '\0';
						szQuote = LabelCheck(szEnd, "switch");
						if (szQuote) playerControlsList[i].nCode = (UINT16)strtol(szQuote, NULL, 0);
						else playerControlsList[i].nCode = 0;
						playerControlsList[i].inpIndex = -1;
						playerControlsList[i].autoFire = false;
						totalcount++;
					}
				}
			}
		}
	}

	// Now add macros to playerControlsList
	totalcount = getPlayerMacros(255, totalcount);

	// Update macros list with defaults from presets/*.ini
	if (h) {
		rewind(h);
		// Go through each line of the config file and process macros
		while (fgets(szLine, 1024, h)) {

			// Get rid of the linefeed and carriage return at the end
			nLen = strlen(szLine);
			if (nLen > 0 && szLine[nLen - 1] == 10) {
				szLine[nLen - 1] = 0;
				nLen--;
			}
			if (nLen > 0 && szLine[nLen - 1] == 13) {
				szLine[nLen - 1] = 0;
				nLen--;
			}

			szValue = LabelCheck(szLine, "version");
			if (szValue) nFileVersion = strtol(szValue, NULL, 0);

			if (nConfigMinVersion <= nFileVersion && nFileVersion <= nBurnVer) {
				szValue = LabelCheck(szLine, "macro");
				if (szValue) {
					char* szQuote = NULL;
					char* szEnd = NULL;
					if (QuoteRead(&szQuote, &szEnd, szValue)) continue;

					// Search for macro with same name
					i = 0;
					while ((i < totalcount) && (strcmp(playerControlsList[i].szInfo, szQuote) || (playerControlsList[i].nInput != GIT_MACRO_AUTO))) i++;

					if (i < totalcount) {
						szQuote = LabelCheck(szEnd, "switch");
						if (szQuote) playerControlsList[i].nCode = (UINT16)strtol(szQuote, NULL, 0);
					} else {		// End of array, is unknown macro? Add to end of array anyway...
						playerControlsList[i].nInput = GIT_MACRO_AUTO;
						snprintf(playerControlsList[i].szInfo, 24, "%s", szQuote);
						snprintf(playerControlsList[i].szName, 32, "(macro) %s", szQuote);
						playerControlsList[i].MenuText[0] = '\0';
						szQuote = LabelCheck(szEnd, "switch");
						if (szQuote) playerControlsList[i].nCode = (UINT16)strtol(szQuote, NULL, 0);
						else playerControlsList[i].nCode = 0;
						playerControlsList[i].inpIndex = -1;
						playerControlsList[i].autoFire = false;
						totalcount++;
					}
				}
			}
		}
	}

	// Update auto fire with defaults from presets/*.ini
	if (h) {
		rewind(h);
		// Go through each line of the config file and process afire
		while (fgets(szLine, 1024, h)) {

			// Get rid of the linefeed and carriage return at the end
			nLen = strlen(szLine);
			if (nLen > 0 && szLine[nLen - 1] == 10) {
				szLine[nLen - 1] = 0;
				nLen--;
			}
			if (nLen > 0 && szLine[nLen - 1] == 13) {
				szLine[nLen - 1] = 0;
				nLen--;
			}

			szValue = LabelCheck(szLine, "version");
			if (szValue) nFileVersion = strtol(szValue, NULL, 0);

			if (nConfigMinVersion <= nFileVersion && nFileVersion <= nBurnVer) {
				szValue = LabelCheck(szLine, "afire");
				if (szValue) {
					char* szQuote = NULL;
					char* szEnd = NULL;
					if (QuoteRead(&szQuote, &szEnd, szValue)) continue;

					// Search for macro with same name
					i = 0;
					while ((i < totalcount) && (strcmp(playerControlsList[i].szInfo, szQuote) || (playerControlsList[i].nInput != GIT_MACRO_AUTO))) i++;

					if (i == totalcount) {		// End of array, is unknown macro? Add to end of array anyway...
						playerControlsList[i].nInput = GIT_MACRO_AUTO;
						snprintf(playerControlsList[i].szInfo, 24, "%s", szQuote);
						snprintf(playerControlsList[i].szName, 32, "(macro) %s", szQuote);
						playerControlsList[i].MenuText[0] = '\0';
						playerControlsList[i].nCode = 0;
						playerControlsList[i].inpIndex = -1;
						totalcount++;
					}
					playerControlsList[i].autoFire = true;
				}
			}
		}
		fclose(h);
	}

	playerControlsList[totalcount].nInput = 0;		// Make sure end of array is marked
	return totalcount;
}

int ResetHardwareDefaultsSelected()
{
	InitHardwareDefaultInputs();
	UpdatePlayerInputsMappingMenu();
	current_selected_item = 0;
	return 0;
}

int MappingHardwareDefaultsSelected()
{
	snprintf(MenuTitle, TITLELENGTH, "%s", tempHardwareMenuText);
	current_menu = MAPPINGPLAYERINPUTSMENU;
	current_selected_item = 0;

	mappingplayerinputsmenucount = InitHardwareDefaultInputs();		// Number of inputs+macros, not menu rows
	UpdatePlayerInputsMappingMenu();

	mappingPlayerInputsMenu[0] = (MenuItem){"(Press ENTER or Joy button)", NULL, NULL};

	int i = 0;
	while (i < mappingplayerinputsmenucount) {
		mappingPlayerInputsMenu[2 * i + 1] = (MenuItem){playerControlsList[i].szName, NULL, NULL};
		mappingPlayerInputsMenu[2 * i + 2] = (MenuItem){playerControlsList[i].MenuText, GetNewKeyButtonJoyToMap, NULL};
		i++;
	}
	mappingplayerinputsmenucount = mappingplayerinputsmenucount * 2 + 1;

	mappingPlayerInputsMenu[mappingplayerinputsmenucount] = (MenuItem){"RESET MAPPING", ResetHardwareDefaultsSelected, NULL};
	mappingplayerinputsmenucount++;
	mappingPlayerInputsMenu[mappingplayerinputsmenucount] = (MenuItem){"SAVE MAPPING (FOR THIS HARDWARE)", SaveHardwareDefaultMapping, NULL};
	mappingplayerinputsmenucount++;
	mappingPlayerInputsMenu[mappingplayerinputsmenucount] = (MenuItem){"BACK (WITHOUT SAVING)", MappingMenuSelected, NULL};
	mappingplayerinputsmenucount++;
	return 0;
}

int GetNewJoyButtonToMap()
{
	SDL_Event e;
	bool mapdone = false;
	int currentSelectedButton = (current_selected_item - 2) / 2;

	mappedbuttons[currentSelectedButton] = -2;
	UpdateMappingJoystickMenuSelected();
	ingame_gui_render();
	SDL_FlushEvents(SDL_KEYDOWN, SDL_CONTROLLERBUTTONUP);

	while (!mapdone) {
		if (SDL_WaitEventTimeout(&e, 1000)) {
			switch (e.type)
			{
				case SDL_QUIT:
					return 1;
				case SDL_JOYBUTTONDOWN:
					if (e.jbutton.which == current_selected_joystick) {
						mappedbuttons[currentSelectedButton] = e.jbutton.button;	// Store pressed button
						for (int i = 0; i < BUTTONS_TO_MAP; i++) {
							if ((i != currentSelectedButton) && (mappedbuttons[i] == mappedbuttons[currentSelectedButton]))
								mappedbuttons[i] = -1;		// Remove duplicated buttons
						}
						mapdone = true;
					}
					break;
				case SDL_CONTROLLERDEVICEREMOVED:
				case SDL_JOYDEVICEREMOVED:
					// Try not to lose total control but this is not great solution
					for (int i = 0; (i < SDL_NumJoysticks()) && (i < MAX_JOYSTICKS); i++) {
						if (SDL_GameControllerNameForIndex(i)) {
							default_joystick = i;
							current_selected_joystick = i;
							break;
						}
					}
					break;
			}
		}
	}
	return UpdateMappingJoystickMenuSelected();
}

int SaveMappedButtons()
{
	bool buttonsaremapped = false;

	for (int i = 0; i < BUTTONS_TO_MAP; i++) {
		if (mappedbuttons[i] > -1) {
			buttonsaremapped = true;
			break;
		}
	}
	if (buttonsaremapped) {
		char guid_str[512];
		SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(current_selected_joystick), guid_str, sizeof(guid_str));

		char gamecontrollerdbline[1024];
		FILE* gamecontrollerdbfile;
		FILE* tempfile;
		bool overwritedb = false;
		char* pos = NULL;
		TCHAR gamecontrollerdbpath[MAX_PATH] = {0};
		TCHAR tempgamecontrollerdbpath[MAX_PATH] = {0};
		TCHAR *szSDLconfigPath = NULL;
		szSDLconfigPath = SDL_GetPrefPath("fbneo", "config");
		_stprintf(gamecontrollerdbpath, _T("%sgamecontrollerdb.txt"), szSDLconfigPath);
		_stprintf(tempgamecontrollerdbpath, _T("%sgamecontrollerdb.TEMP.txt"), szSDLconfigPath);
		SDL_free(szSDLconfigPath);

		// Delete old mappings for this joystick that may exist in gamecontrollerdb.txt
		if ((gamecontrollerdbfile = _tfopen(gamecontrollerdbpath, _T("rt"))) && (tempfile = _tfopen(tempgamecontrollerdbpath, _T("wt")))) {
			while (fgets(gamecontrollerdbline, 1024, gamecontrollerdbfile)) {
				pos = strstr(gamecontrollerdbline, guid_str);
				if (pos) overwritedb = true;	// Old mapping found, ignore line and overwrite gamecontrollerdb.txt
				else {
					if (gamecontrollerdbline[strlen(gamecontrollerdbline) - 1] == '\n') fprintf(tempfile, "%s", gamecontrollerdbline);
					else fprintf(tempfile, "%s\n", gamecontrollerdbline);
				}
			}
			fclose(tempfile);
			fclose(gamecontrollerdbfile);
			if (overwritedb) {
				remove(gamecontrollerdbpath);
				rename(tempgamecontrollerdbpath, gamecontrollerdbpath);
			}
			else remove(tempgamecontrollerdbpath);
		}
		// Add new mapping string to gamecontrollerdb.txt
		if (gamecontrollerdbfile = _tfopen(gamecontrollerdbpath, _T("at"))) {
			fprintf(gamecontrollerdbfile, "%s,", guid_str);
			fprintf(gamecontrollerdbfile, "%s,", JoystickNames[current_selected_joystick]);
			if (mappedbuttons[0] > -1) {
				fprintf(gamecontrollerdbfile, "a:b%d,", mappedbuttons[0]);
				buttons[current_selected_joystick][0] = mappedbuttons[0];
			}
			if (mappedbuttons[1] > -1) {
				fprintf(gamecontrollerdbfile, "b:b%d,", mappedbuttons[1]);
				buttons[current_selected_joystick][1] = mappedbuttons[1];
			}
			if (mappedbuttons[2] > -1) {
				fprintf(gamecontrollerdbfile, "x:b%d,", mappedbuttons[2]);
				buttons[current_selected_joystick][2] = mappedbuttons[2];
			}
			if (mappedbuttons[3] > -1) {
				fprintf(gamecontrollerdbfile, "y:b%d,", mappedbuttons[3]);
				buttons[current_selected_joystick][3] = mappedbuttons[3];
			}
			if (mappedbuttons[4] > -1) {
				fprintf(gamecontrollerdbfile, "leftshoulder:b%d,", mappedbuttons[4]);
				buttons[current_selected_joystick][4] = mappedbuttons[4];
			}
			if (mappedbuttons[5] > -1) {
				fprintf(gamecontrollerdbfile, "rightshoulder:b%d,", mappedbuttons[5]);
				buttons[current_selected_joystick][5] = mappedbuttons[5];
			}
			if (mappedbuttons[6] > -1) {
				fprintf(gamecontrollerdbfile, "back:b%d,", mappedbuttons[6]);
				buttons[current_selected_joystick][6] = mappedbuttons[6];
			}
			if (mappedbuttons[7] > -1) {
				fprintf(gamecontrollerdbfile, "start:b%d,", mappedbuttons[7]);
				buttons[current_selected_joystick][7] = mappedbuttons[7];
			}
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
			printf("Saved mapping of \"%s\" in: %s\n", JoystickNames[current_selected_joystick], gamecontrollerdbpath);
			SDL_GameControllerAddMappingsFromFile(gamecontrollerdbpath);	// Load updated mappings

			// Re config input because of joystick remapp
			int i;
			struct GameInp* pgi = NULL;
			for (i = 0, pgi = GameInp; i < nGameInpCount; i++, pgi++) {
				struct BurnInputInfo bii;
				// Get the extra info about the input
				bii.szInfo = NULL;
				BurnDrvGetInputInfo(&bii, i);
				if (bii.pVal == NULL) continue;
				if (bii.szInfo == NULL) bii.szInfo = "";

				if (usejoy) MapJoystick(pgi, bii.szInfo, current_selected_joystick, current_selected_joystick + 1);
				else MapJoystick(pgi, bii.szInfo, current_selected_joystick + 1, current_selected_joystick + 1);
			}
			for (i = 0; i < nMacroCount; i++, pgi++) {
				if (usejoy) MapJoystick(pgi, pgi->Macro.szName, current_selected_joystick, current_selected_joystick + 1);
				else MapJoystick(pgi, pgi->Macro.szName, current_selected_joystick + 1, current_selected_joystick + 1);
			}
			GameInpCheckLeftAlt();

			bSaveInputs = true;		// Save this game inputs to romname.ini when we exit game
			current_selected_joystick = default_joystick;	// Return control to first joystick
			return MappingMenuSelected();
		} else return 1;
	}
	return 0;
}

int UpdateMappingJoystickMenuSelected()
{
	bool buttonsaremapped = false;
	for (int i = 0; i < BUTTONS_TO_MAP; i++) {
		if (mappedbuttons[i] > -1) {
			snprintf(mappingJoystickMenuList[i], 40, ">>> button %d", mappedbuttons[i]);
			buttonsaremapped = true;
		} else if (mappedbuttons[i] == -1) {
			snprintf(mappingJoystickMenuList[i], 40, ">>> none");
		} else snprintf(mappingJoystickMenuList[i], 40, ">>> PRESS KEY/BUTTON or MOVE JOY/DPAD");
		mappingJoystickMenu[i * 2 + 2] = (MenuItem){mappingJoystickMenuList[i], GetNewJoyButtonToMap, NULL};
	}
	if (buttonsaremapped) mappingJoystickMenu[BUTTONS_TO_MAP * 2 + 2] = (MenuItem){"SAVE MAPPING (GLOBALLY)", SaveMappedButtons, NULL};
	else mappingJoystickMenu[BUTTONS_TO_MAP * 2 + 2] = (MenuItem){"(MAP A BUTTON TO SAVE)", NULL, NULL};
	return 0;
}

int ResetMappedButtons()
{
	SDL_GameControllerButtonBind bind;
	SDL_GameController *currentGameController = SDL_GameControllerOpen(current_selected_joystick);
	if (currentGameController)
	{
		bind = SDL_GameControllerGetBindForButton(currentGameController, SDL_CONTROLLER_BUTTON_A);
		mappedbuttons[0] = bind.value.button;
		bind = SDL_GameControllerGetBindForButton(currentGameController, SDL_CONTROLLER_BUTTON_B);
		mappedbuttons[1] = bind.value.button;
		bind = SDL_GameControllerGetBindForButton(currentGameController, SDL_CONTROLLER_BUTTON_X);
		mappedbuttons[2] = bind.value.button;
		bind = SDL_GameControllerGetBindForButton(currentGameController, SDL_CONTROLLER_BUTTON_Y);
		mappedbuttons[3] = bind.value.button;
		bind = SDL_GameControllerGetBindForButton(currentGameController, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
		mappedbuttons[4] = bind.value.button;
		bind = SDL_GameControllerGetBindForButton(currentGameController, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
		mappedbuttons[5] = bind.value.button;
		bind = SDL_GameControllerGetBindForButton(currentGameController, SDL_CONTROLLER_BUTTON_BACK);
		mappedbuttons[6] = bind.value.button;
		bind = SDL_GameControllerGetBindForButton(currentGameController, SDL_CONTROLLER_BUTTON_START);
		mappedbuttons[7] = bind.value.button;
	} else for (int i = 0; i < BUTTONS_TO_MAP; i++) {
		mappedbuttons[i] = -1;
	}
	return UpdateMappingJoystickMenuSelected();
}

// Use current_selected_item as the joystick index
int MappingJoystickMenuSelected()
{
	current_selected_joystick = current_selected_item - nMaxPlayers;
	snprintf(MenuTitle, TITLELENGTH, "%s", JoystickNames[current_selected_joystick]);
	current_selected_item = 0;
	current_menu = MAPPINGJOYSTICKMENU;
	bool buttonsaremapped = false;

	SDL_GameControllerButtonBind bind;
	SDL_GameController *currentGameController = SDL_GameControllerOpen(current_selected_joystick);
	if (currentGameController)
	{
		bind = SDL_GameControllerGetBindForButton(currentGameController, SDL_CONTROLLER_BUTTON_A);
		mappedbuttons[0] = bind.value.button;
		bind = SDL_GameControllerGetBindForButton(currentGameController, SDL_CONTROLLER_BUTTON_B);
		mappedbuttons[1] = bind.value.button;
		bind = SDL_GameControllerGetBindForButton(currentGameController, SDL_CONTROLLER_BUTTON_X);
		mappedbuttons[2] = bind.value.button;
		bind = SDL_GameControllerGetBindForButton(currentGameController, SDL_CONTROLLER_BUTTON_Y);
		mappedbuttons[3] = bind.value.button;
		bind = SDL_GameControllerGetBindForButton(currentGameController, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
		mappedbuttons[4] = bind.value.button;
		bind = SDL_GameControllerGetBindForButton(currentGameController, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
		mappedbuttons[5] = bind.value.button;
		bind = SDL_GameControllerGetBindForButton(currentGameController, SDL_CONTROLLER_BUTTON_BACK);
		mappedbuttons[6] = bind.value.button;
		bind = SDL_GameControllerGetBindForButton(currentGameController, SDL_CONTROLLER_BUTTON_START);
		mappedbuttons[7] = bind.value.button;
	} else for (int i = 0; i < BUTTONS_TO_MAP; i++) {
		mappedbuttons[i] = -1;
	}

	mappingJoystickMenu[0] = (MenuItem){"(Press ENTER or Joy button)", NULL, NULL};
	for (int i = 0; i < BUTTONS_TO_MAP; i++) {
		mappingJoystickMenu[i * 2 + 1] = (MenuItem){buttonsList[i], NULL, NULL};
		if (mappedbuttons[i] > -1) {
			snprintf(mappingJoystickMenuList[i], 40, ">>> button %d", mappedbuttons[i]);
			buttonsaremapped = true;
		} else snprintf(mappingJoystickMenuList[i], 40, ">>> none");
		mappingJoystickMenu[i * 2 + 2] = (MenuItem){mappingJoystickMenuList[i], GetNewJoyButtonToMap, NULL};
	}
	mappingJoystickMenu[BUTTONS_TO_MAP * 2 + 1] = (MenuItem){"RESET MAPPING", ResetMappedButtons, NULL};
	if (buttonsaremapped) mappingJoystickMenu[BUTTONS_TO_MAP * 2 + 2] = (MenuItem){"SAVE MAPPING (GLOBALLY)", SaveMappedButtons, NULL};
	else mappingJoystickMenu[BUTTONS_TO_MAP * 2 + 2] = (MenuItem){"(MAP A BUTTON TO SAVE)", NULL, NULL};
	mappingJoystickMenu[BUTTONS_TO_MAP * 2 + 3] = (MenuItem){"BACK (WITHOUT SAVING)", MappingMenuSelected, NULL};
	
	SDL_GameControllerClose(currentGameController);		// Is this necessary?
	return 0;
}

int MappingMenuSelected()
{
	snprintf(MenuTitle, TITLELENGTH, "Mapping Options");
	current_selected_item = 0;
	current_menu = PLAYERSANDJOYSTICKS;
	mappingmenucount = 0;
	current_selected_joystick = default_joystick;	// Return control to first joystick

	for (int i = 0; i < nMaxPlayers; i++) {
		playerAndJoystickMenu[i] = (MenuItem){playerNames[i], MappingPlayerInputsSelected, NULL};
	}
	mappingmenucount = nMaxPlayers;
	for (int i = 0; (i < nJoystickCount) && (i < MAX_JOYSTICKS); i++) {
		JoystickNames[i] = SDL_GameControllerNameForIndex(i);
		if (JoystickNames[i] == NULL) JoystickNames[i] = SDL_JoystickNameForIndex(i);	// Get joystick name if got no game controller name
		playerAndJoystickMenu[mappingmenucount] = (MenuItem){JoystickNames[i], MappingJoystickMenuSelected, NULL};
		mappingmenucount++;
	}
	playerAndJoystickMenu[mappingmenucount] = (MenuItem){"Defaults for Player 1 (Globally)", PlayerDefaultsMenuSelected, NULL};
	mappingmenucount++;
	playerAndJoystickMenu[mappingmenucount] = (MenuItem){"Defaults for Player 2 (Globally)", PlayerDefaultsMenuSelected, NULL};
	mappingmenucount++;
	playerAndJoystickMenu[mappingmenucount] = (MenuItem){"Defaults for Player 3 (Globally)", PlayerDefaultsMenuSelected, NULL};
	mappingmenucount++;
	playerAndJoystickMenu[mappingmenucount] = (MenuItem){"Defaults for Player 4 (Globally)", PlayerDefaultsMenuSelected, NULL};
	mappingmenucount++;

	const char* drvname = BurnDrvGetTextA(DRV_NAME);
	if (strcmp(previousdrvnameinputs, drvname)) {		// It's a new or different game, check what hardware is being emulated for input defaults file
		snprintf(previousdrvnameinputs, 32, "%s", drvname);

		INT32 nHardwareFlag = (BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK);
		gamehw_cfg_index = 0;
		keepsearchinghwcfg = true;
		while (keepsearchinghwcfg && (gamehw_cfg[gamehw_cfg_index].ini[0] != '\0')) {
			for (INT32 hw = 0; gamehw_cfg[gamehw_cfg_index].hw[hw]; hw++) {
				if (gamehw_cfg[gamehw_cfg_index].hw[hw] == nHardwareFlag) {
					keepsearchinghwcfg = false;
					break;
				}
			}
			if (keepsearchinghwcfg) gamehw_cfg_index++;
		}
	}
	if (!keepsearchinghwcfg) {
		snprintf(tempHardwareMenuText, 41, "Defaults for %s", gamehw_cfg[gamehw_cfg_index].system);
		playerAndJoystickMenu[mappingmenucount] = (MenuItem){tempHardwareMenuText, MappingHardwareDefaultsSelected, NULL};
		mappingmenucount++;
	}
	playerAndJoystickMenu[mappingmenucount] = (MenuItem){"BACK", MainMenuSelected, NULL};
	mappingmenucount++;
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
	return CheatOptionsMenuSelected();
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
			snprintf(pCurrentCheat->pOption[i]->szOptionName, CHEAT_MAX_NAME, "%s", tmpoptionname);
		}
		cheatOptionsMenu[i] = (MenuItem){pCurrentCheat->pOption[i]->szOptionName, SelectedCheatOption, NULL};
		cheatoptionscount++;
	}
	if (cheatoptionscount) {
		cheatOptionsMenu[cheatoptionscount] = (MenuItem){"BACK", CheatMenuSelected, NULL};
		cheatoptionscount++;
		current_menu = CHEATOPTIONSMENU;
		snprintf(MenuTitle, TITLELENGTH, "%s", pCurrentCheat->szCheatName);
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
		snprintf(previousdrvnamecheats, 32, "%s", drvname);
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
		if (i > 0) {
			cheatMenu[i] = (MenuItem){"DISABLE ALL CHEATS", DisableAllCheats, NULL};
			i++;
			cheatMenu[i] = (MenuItem){"BACK", MainMenuSelected, NULL};
			cheatcount = i + 1;
		} else {
			cheatMenu[i] = (MenuItem){"BACK (no cheats found)", MainMenuSelected, NULL};
			cheatcount = 1;
		}
	}
	return 0;
}

// Auto Fire retaled stuff
UINT16 autofiremenucount = 8;
struct MenuItem autoFireMenu[8];
char AutoFireMenuLines[8][40];

INT32 AutoFireRateList[6] = {24, 12, 8, 6, 4, 2};
char AutoFireRateLabels[6][10] = {"Slow", "Medium", "Fast", "Faster", "Very Fast", "Fastest"};

int UpdateAutoFireMenu()
{
	for (int i = 0; i < 6; i++) {
		snprintf(AutoFireMenuLines[i], 39, "%s %s", (AutoFireRateList[i] == nAutoFireRate ? "[X]" : "[ ]"), AutoFireRateLabels[i]);
	}
	return 0;
}

int SetAutoFireRate()
{
	nAutoFireRate = AutoFireRateList[current_selected_item];
	return UpdateAutoFireMenu();
}

int AutoFireMenuSelected()
{
	snprintf(MenuTitle, TITLELENGTH, "Auto Fire Rate");
	current_menu = AUTOFIREMENU;
	current_selected_item = 0;
	for (int i = 0; i < 6; i++) {
		autoFireMenu[i] = (MenuItem){AutoFireMenuLines[i], SetAutoFireRate, NULL};
	}
	autoFireMenu[6] = (MenuItem){"(hold ENTER/button over a macro)", NULL, NULL};
	autoFireMenu[7] = (MenuItem){"BACK", MainMenuSelected, NULL};
	
	return UpdateAutoFireMenu();
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

// Screenshot related stuff
int MakeScreenShotSelected()
{
	if (MakeScreenShot(0)) UpdateMessage("There was some error saving the screenshot!");
	else UpdateMessage("Screenshot saved.");
	return 1;
}

// Main menu related stuff
int BackToGameSelected()
{
	return 1;
}

#define MAINMENU_COUNT 9
struct MenuItem mainMenu[MAINMENU_COUNT] =
{
	{"DIP Switches", DIPMenuSelected, NULL},
	{"Cheats", CheatMenuSelected, NULL},
	{"Mapping Options", MappingMenuSelected, NULL},
	{"Auto Fire Rate", AutoFireMenuSelected, NULL},
	{"Save State", QuickSave, NULL},
	{"Load State", QuickLoad, NULL},
	{"Save Screenshot", MakeScreenShotSelected, NULL},
	{"Reset the game", ResetMenuSelected, NULL},
	{"Back to Game!", BackToGameSelected, NULL},
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
		case PLAYERSANDJOYSTICKS:
			current_item_count = mappingmenucount;
			current_menu_items = playerAndJoystickMenu;
			break;
		case MAPPINGJOYSTICKMENU:
			current_item_count = BUTTONS_TO_MAP * 2 + 4;
			current_menu_items = mappingJoystickMenu;
			break;
		case MAPPINGPLAYERINPUTSMENU:
			current_item_count = mappingplayerinputsmenucount;
			current_menu_items = mappingPlayerInputsMenu;
			break;
		case DEFAULTPLAYERINPUTSMENU:
			current_item_count = defaultplayerinputsmenucount;
			current_menu_items = mappingPlayerInputsMenu;
			break;
		case AUTOFIREMENU:
			current_item_count = autofiremenucount;
			current_menu_items = autoFireMenu;
			break;
		case CHEATMENU:
			current_item_count = cheatcount;
			current_menu_items = cheatMenu;
			break;
		case CHEATOPTIONSMENU:
			current_item_count = cheatoptionscount;
			current_menu_items = cheatOptionsMenu;
			break;
		case RESETMENU:
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
		} else if (((current_menu == MAPPINGJOYSTICKMENU) || (current_menu == MAPPINGPLAYERINPUTSMENU) || (current_menu == DEFAULTPLAYERINPUTSMENU)) && ((current_menu_items[i].name[0] == '>') || (current_menu_items[i].name[0] == '['))) {
			incolor(0x009000, /* unused */ 0);
		} else {
			incolor(normal_color, /* unused */ 0);
		}
		c++;
		inprint(sdlRenderer,current_menu_items[i].name , 10, 30+(10*c));
	}
	if (current_item_count > firstMenuLine + maxLinesMenu + 1) {
		incolor(normal_color, /* unused */ 0);
		inprint(sdlRenderer, "( ... more ... )", 10, 40+(10*c));
	}
	SDL_RenderPresent(sdlRenderer);
}

int ingame_gui_process()
{
	while (SDL_PollEvent(&e)) {
		switch (e.type)
		{
			case SDL_QUIT:
				return 1;
			case SDL_KEYDOWN:
				switch (e.key.keysym.sym)
				{
					case SDLK_TAB:
						MainMenuSelected();
						return 1;
					case SDLK_RETURN:
						if (current_menu_items[current_selected_item].menuFunction) {
							return current_menu_items[current_selected_item].menuFunction();
						} else {
							return 0;
						}
						break;
					case SDLK_UP:
						if (((current_menu == MAPPINGPLAYERINPUTSMENU) || (current_menu == MAPPINGJOYSTICKMENU) || (current_menu == DEFAULTPLAYERINPUTSMENU)) && (current_selected_item > 1) && (current_selected_item < current_item_count - 3)) current_selected_item -= 2;
						else if (current_selected_item > 0) current_selected_item--;
						break;
					case SDLK_DOWN:
						if (((current_menu == MAPPINGPLAYERINPUTSMENU) || (current_menu == MAPPINGJOYSTICKMENU) || (current_menu == DEFAULTPLAYERINPUTSMENU)) && (current_selected_item < current_item_count - 5)) current_selected_item += 2;
						else if ((current_selected_item < current_item_count - 1)) current_selected_item++;
						break;
					case SDLK_LEFT:
						if (current_selected_item > 10) {
							current_selected_item -= 10;
							if ((current_menu == MAPPINGPLAYERINPUTSMENU) || (current_menu == MAPPINGJOYSTICKMENU) || (current_menu == DEFAULTPLAYERINPUTSMENU)) current_selected_item -= current_selected_item % 2;		// Make sure we jump to even rows
						} else current_selected_item = 0;
						break;
					case SDLK_RIGHT:
						if (current_selected_item < current_item_count - 11) current_selected_item += 10;
						else current_selected_item = current_item_count - 1;
						break;
				}
				break;
			case SDL_JOYAXISMOTION:
				if (e.jaxis.timestamp - lastTimestamp > EVENTSTIMEOUT) {
					switch (e.jaxis.axis)
					{
						case 1:
							if (e.jaxis.value < -JOYSTICK_DEAD_ZONE) {
								lastTimestamp = e.jaxis.timestamp;
								if (((current_menu == MAPPINGPLAYERINPUTSMENU) || (current_menu == MAPPINGJOYSTICKMENU) || (current_menu == DEFAULTPLAYERINPUTSMENU)) && (current_selected_item > 1) && (current_selected_item < current_item_count - 3)) current_selected_item -= 2;
								else if (current_selected_item > 0) current_selected_item--;
							} else if (e.jaxis.value > JOYSTICK_DEAD_ZONE) {
								lastTimestamp = e.jaxis.timestamp;
								if (((current_menu == MAPPINGPLAYERINPUTSMENU) || (current_menu == MAPPINGJOYSTICKMENU) || (current_menu == DEFAULTPLAYERINPUTSMENU)) && (current_selected_item < current_item_count - 5)) current_selected_item += 2;
								else if ((current_selected_item < current_item_count - 1)) current_selected_item++;
							}
							break;
						case 0:
							if (e.jaxis.value < -JOYSTICK_DEAD_ZONE) {
								lastTimestamp = e.jaxis.timestamp;
								if (current_selected_item > 10) {
									current_selected_item -= 10;
									if ((current_menu == MAPPINGPLAYERINPUTSMENU) || (current_menu == MAPPINGJOYSTICKMENU) || (current_menu == DEFAULTPLAYERINPUTSMENU)) current_selected_item -= current_selected_item % 2;		// Make sure we jump to even rows
								} else current_selected_item = 0;
							} else if (e.jaxis.value > JOYSTICK_DEAD_ZONE) {
								lastTimestamp = e.jaxis.timestamp;
								if (current_selected_item < current_item_count - 11) current_selected_item += 10;
								else current_selected_item = current_item_count - 1;
							}
							break;
					}
				}
				break;
			case SDL_JOYHATMOTION:
				switch (e.jhat.value)
				{
					case SDL_HAT_UP:
						if (((current_menu == MAPPINGPLAYERINPUTSMENU) || (current_menu == MAPPINGJOYSTICKMENU) || (current_menu == DEFAULTPLAYERINPUTSMENU)) && (current_selected_item > 1) && (current_selected_item < current_item_count - 3)) current_selected_item -= 2;
						else if (current_selected_item > 0) current_selected_item--;
						break;
					case SDL_HAT_DOWN:
						if (((current_menu == MAPPINGPLAYERINPUTSMENU) || (current_menu == MAPPINGJOYSTICKMENU) || (current_menu == DEFAULTPLAYERINPUTSMENU)) && (current_selected_item < current_item_count - 5)) current_selected_item += 2;
						else if ((current_selected_item < current_item_count - 1)) current_selected_item++;
						break;
					case SDL_HAT_LEFT:
						if (current_selected_item > 10) {
							current_selected_item -= 10;
							if ((current_menu == MAPPINGPLAYERINPUTSMENU) || (current_menu == MAPPINGJOYSTICKMENU) || (current_menu == DEFAULTPLAYERINPUTSMENU)) current_selected_item -= current_selected_item % 2;		// Make sure we jump to even rows
						} else current_selected_item = 0;
						break;
					case SDL_HAT_RIGHT:
						if (current_selected_item < current_item_count - 11) current_selected_item += 10;
						else current_selected_item = current_item_count - 1;
						break;
				}
				break;
			case SDL_JOYBUTTONDOWN:
				if (current_menu_items[current_selected_item].menuFunction) {
					return current_menu_items[current_selected_item].menuFunction();
				} else {
					return 0;
				}
				break;
			case SDL_CONTROLLERDEVICEREMOVED:
			case SDL_JOYDEVICEREMOVED:
				// Try not to lose total control but this is not great solution
				for (int i = 0; (i < SDL_NumJoysticks()) && (i < MAX_JOYSTICKS); i++) {
					if (SDL_GameControllerNameForIndex(i)) {
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
	for (int i = 0; (i < nJoystickCount) && (i < MAX_JOYSTICKS); i++) {
		if (SDL_GameControllerNameForIndex(i)) {
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
