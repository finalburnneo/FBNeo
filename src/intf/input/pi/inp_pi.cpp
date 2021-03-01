
#include <SDL.h>
#include <unistd.h>
#include <sys/time.h>

#include "burner.h"
#include "inp_sdl_keys.h"

// Note that these constants are tied to the JOY_ macros!
#define MAX_JOYSTICKS   8
#define MAX_JOY_BUTTONS 28

#define FREEPLAY_HACK_COIN_DURATION_MS 1e3 / 8

// #define DEBUG_INPUT

int nEnableFreeplayHack = 0;

static struct timeval lastInputEvent;
static uint64_t startPressUsec[4];

static unsigned int joyButtonStates[MAX_JOYSTICKS];
static int *joyLookupTable = NULL;
static int keyLookupTable[512];

static int mouseScanned = 0;
static int inputEventOccurred = 1;

static void scanKeyboard();
static void scanJoysticks();
static void scanMouse();

static void resetJoystickMap();
static bool usesStreetFighterLayout();
static int checkMouseState(unsigned int nSubCode);
static int handleFreeplayHack(int player, int code);

#define JOY_DIR_LEFT  0x00
#define JOY_DIR_RIGHT 0x01
#define JOY_DIR_UP    0x02
#define JOY_DIR_DOWN  0x03

#define JOY_MAP_DIR(joy,dir) ((((joy)&0xff)<<8)|((dir)&0x3))
#define JOY_MAP_BUTTON(joy,button) ((((joy)&0xff)<<8)|(((button)+4)&0x1f))
#define JOY_IS_DOWN(value) (joyButtonStates[((value)>>8)&0x7]&(1<<((value)&0xff)))
#define JOY_CLEAR(value) joyButtonStates[((value)>>8)&0x7]&=~(1<<((value)&0xff))
#define KEY_IS_DOWN(key) keyState[(key)]

#define JOY_DEADZONE 0x4000

static unsigned int defaultLayout[] = {
	FBK_Z, FBK_X, FBK_C, FBK_A, FBK_S, FBK_D, FBK_Q, FBK_W, FBK_E,
};
static unsigned int capcom6Layout[] = {
	FBK_A, FBK_S, FBK_D, FBK_Z, FBK_X, FBK_C,
};

static unsigned char* keyState = NULL;
static struct {
	unsigned char buttons;
	int xdelta;
	int ydelta;
} mouseState;

extern int nExitEmulator;

static int nInitedSubsytems = 0;
static SDL_Joystick* JoyList[MAX_JOYSTICKS];
static int nJoystickCount = 0;						// Number of joysticks connected to this machine

static int piInputExit()
{
	// Close all joysticks
	for (int i = 0; i < MAX_JOYSTICKS; i++) {
		if (JoyList[i]) {
			SDL_JoystickClose(JoyList[i]);
			JoyList[i] = NULL;
		}
	}

	free(joyLookupTable);
	joyLookupTable = NULL;

	nJoystickCount = 0;

	if (!(nInitedSubsytems & SDL_INIT_JOYSTICK)) {
		SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
	}

	nInitedSubsytems = 0;

	return 0;
}

static int piInputInit()
{
	piInputExit();

	// Allocate memory for the lookup table
	if ((joyLookupTable = (int *)malloc(0x8000 * sizeof(int))) == NULL) {
		return 1;
	}

	memset(&JoyList, 0, sizeof(JoyList));

	nInitedSubsytems = SDL_WasInit(SDL_INIT_JOYSTICK);
	gettimeofday(&lastInputEvent, NULL);

	if (!(nInitedSubsytems & SDL_INIT_JOYSTICK)) {
		SDL_InitSubSystem(SDL_INIT_JOYSTICK);
	}

	// Set up the joysticks
	nJoystickCount = SDL_NumJoysticks();
	for (int i = 0; i < nJoystickCount; i++) {
		JoyList[i] = SDL_JoystickOpen(i);
	}
	SDL_JoystickEventState(SDL_IGNORE);

	resetJoystickMap();

	return 0;
}

static int piInputStart()
{
	// Update SDL event queue
	SDL_PumpEvents();

	scanJoysticks();
	scanKeyboard();

	struct timeval now;
	gettimeofday(&now, NULL);

	if (nEnableFreeplayHack) {
		for (int i = 0; i < 4; i++) {
			if (!JOY_IS_DOWN(joyLookupTable[FBK_1 + i]) && !KEY_IS_DOWN(keyLookupTable[FBK_1 + i])) {
				startPressUsec[i] = 0;
			} else if (startPressUsec[i] == 0) {
				startPressUsec[i] = now.tv_sec * 1e3 + now.tv_usec / 1e3;
			}
		}
	}

	// Mouse not read this frame
	mouseScanned = 0;

	return 0;
}

// player - 0-4
static int handleFreeplayHack(int player, int code)
{
	if (startPressUsec[player] != 0) {
		struct timeval now;
		gettimeofday(&now, NULL);
		uint64_t now_usec = now.tv_sec * 1e3 + now.tv_usec / 1e3;
		uint64_t diff = now_usec - startPressUsec[player];
		if (diff < FREEPLAY_HACK_COIN_DURATION_MS) {
			if (code == FBK_5 + player) {
				return 1;
			}
		} else {
			if (code == FBK_1 + player) {
				return 1;
			}
		}
	}

	return 0;
}

static int piInputState(int nCode)
{
	if (nCode < 0) {
		return 0;
	}

	int player = -1;
	if (nCode == FBK_1 || nCode == FBK_2 || nCode == FBK_3 || nCode == FBK_4) {
		player = nCode - FBK_1;
	} else if (nCode == FBK_5 || nCode == FBK_6 || nCode == FBK_7 || nCode == FBK_8) {
		player = nCode - FBK_5;
	}

	if (nCode < 0x100) {
		int mapped = keyLookupTable[nCode];
		if (nEnableFreeplayHack && player != -1) {
			if (handleFreeplayHack(player, nCode)) {
				return 1;
			}
		}
		if (KEY_IS_DOWN(mapped)) {
			inputEventOccurred = 1;
			return 1;
		}
	}

	if (/* nCode >= 0x4000 && */ nCode < 0x8000) {
		int mapped = joyLookupTable[nCode];
		if (mapped != -1) {
			if (nEnableFreeplayHack && player != -1) {
				handleFreeplayHack(player, nCode);
			}
			if (JOY_IS_DOWN(mapped)) {
				JOY_CLEAR(mapped);
				return 1;
			}
		}
	}

	if (nCode >= 0x8000 && nCode < 0xC000) {
		// Codes 8000-C000 = Mouse
		if ((nCode - 0x8000) >> 8) {
			// Only the system mouse is supported by SDL
			return 0;
		}
		if (!mouseScanned) {
			scanMouse();
			mouseScanned = 1;
		}
		return checkMouseState(nCode & 0xFF);
	}

	return 0;
}

// Check a subcode (the 80xx bit in 8001, 8102 etc) for a mouse input code
static int checkMouseState(unsigned int nSubCode)
{
	switch (nSubCode & 0x7F) {
	case 0: return (mouseState.buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
	case 1: return (mouseState.buttons & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;
	case 2: return (mouseState.buttons & SDL_BUTTON(SDL_BUTTON_MIDDLE)) != 0;
	}

	return 0;
}

static void scanMouse()
{
	mouseState.buttons = SDL_GetRelativeMouseState(&(mouseState.xdelta),
		&(mouseState.ydelta));
}

static void scanKeyboard()
{
	static int screenshotDown = 0;
	static Uint8 emptyStates[512] = { 0 };

	if ((keyState = SDL_GetKeyState(NULL)) == NULL) {
		keyState = emptyStates;
	}

	if (keyState[SDLK_ESCAPE]) {
		nExitEmulator = 1;
	}
	if (!screenshotDown && keyState[SDLK_F10]) {
		MakeScreenShot();
	}
	screenshotDown = keyState[SDLK_F10];
}

static void scanJoysticks()
{
	SDL_JoystickUpdate();

	int joyCount = nJoystickCount;
	if (joyCount > MAX_JOYSTICKS) {
		joyCount = MAX_JOYSTICKS;
	}

#ifdef DEBUG_INPUT
	static unsigned int oldButtonStates[MAX_JOYSTICKS];
#endif
	for (int joy = 0; joy < joyCount; joy++) {
		joyButtonStates[joy] = 0;
		SDL_Joystick *joystick = JoyList[joy];

		int xPos = SDL_JoystickGetAxis(joystick, 0);
		int yPos = SDL_JoystickGetAxis(joystick, 1);

		// Directions
		int dirStates[] = {
			(xPos < -JOY_DEADZONE), // left
			(xPos > JOY_DEADZONE),  // right
			(yPos < -JOY_DEADZONE), // up
			(yPos > JOY_DEADZONE),  // down
		};
		for (int i = 0; i < 4; i++) {
			joyButtonStates[joy] |= (dirStates[i] << i);
			if (dirStates[i] && joyLookupTable[FBK_F12] == JOY_MAP_DIR(joy, i)) {
				nExitEmulator = 1;
				return;
			}
		}

		// Buttons
		int buttonCount = SDL_JoystickNumButtons(joystick);
		if (buttonCount > MAX_JOY_BUTTONS) {
			buttonCount = MAX_JOY_BUTTONS;
		}
		for (int button = 0, shift = 4; button < buttonCount; button++, shift++) {
			int state = SDL_JoystickGetButton(joystick, button);
			joyButtonStates[joy] |= ((state & 1) << shift);

			if (state && joyLookupTable[FBK_F12] == JOY_MAP_BUTTON(joy, button)) {
				nExitEmulator = 1;
				return;
			}
		}

		if (joyButtonStates[joy] != 0) {
			inputEventOccurred = 1;
		}

#ifdef DEBUG_INPUT
		if (oldButtonStates[joy] != joyButtonStates[joy]) {
			static char temp[33] = {'\0'};
			char *ch = &temp[buttonCount+4]; //32-(buttonCount+4)];
			for (int i = buttonCount + 4; i >= 0; i--,ch--) {
				*ch = ((joyButtonStates[joy] >> i) & 1) + '0';
			}

			fprintf(stderr, "J%d: %s\n", joy + 1,  ch + 1);
			oldButtonStates[joy] = joyButtonStates[joy];
		}
#endif
	}
}

static bool usesStreetFighterLayout()
{
	int fireButtons = 0;
	struct BurnInputInfo bii;

	for (UINT32 i = 0; i < 0x1000; i++) {
		bii.szName = NULL;
		if (BurnDrvGetInputInfo(&bii,i)) {
			break;
		}
		
		BurnDrvGetInputInfo(&bii, i);
		if (bii.szName == NULL) {
			bii.szName = "";
		}

		bool bPlayerInInfo = (toupper(bii.szInfo[0]) == 'P' && bii.szInfo[1] >= '1' && bii.szInfo[1] <= '4'); // Because some of the older drivers don't use the standard input naming.
		bool bPlayerInName = (bii.szName[0] == 'P' && bii.szName[1] >= '1' && bii.szName[1] <= '4');

		if (bPlayerInInfo || bPlayerInName) {
			INT32 nPlayer = 0;

			if (bPlayerInName) {
				nPlayer = bii.szName[1] - '1';
			}
			if (bPlayerInInfo && nPlayer == 0) {
				nPlayer = bii.szInfo[1] - '1';
			}
			if (nPlayer == 0) {
				if (strncmp(" fire", bii.szInfo + 2, 5) == 0) {
					fireButtons++;
				}
			}
		}
	}

	int hardwareMask = BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK;
	return fireButtons >= 6 &&
		(hardwareMask == HARDWARE_CAPCOM_CPS1 ||
		hardwareMask == HARDWARE_CAPCOM_CPS2 ||
		hardwareMask == HARDWARE_CAPCOM_CPS3);
}

static int setupDefaults(int pindex)
{
	if (pindex == 0) {
		joyLookupTable[FBK_UPARROW] = JOY_MAP_DIR(pindex, JOY_DIR_UP);
		joyLookupTable[FBK_LEFTARROW] = JOY_MAP_DIR(pindex, JOY_DIR_LEFT);
		joyLookupTable[FBK_RIGHTARROW] = JOY_MAP_DIR(pindex, JOY_DIR_RIGHT);
		joyLookupTable[FBK_DOWNARROW] = JOY_MAP_DIR(pindex, JOY_DIR_DOWN);

		if (usesStreetFighterLayout())
			for (int i = 0; i < 6; i++)
				joyLookupTable[capcom6Layout[i]] = JOY_MAP_BUTTON(pindex, i);
		else
			for (int i = 0; i < 9; i++)
				joyLookupTable[defaultLayout[i]] = JOY_MAP_BUTTON(pindex, i);
	} else {
		unsigned int mask = 0x4000 + 0x100 * (pindex - 1);
		joyLookupTable[mask|0x02] = JOY_MAP_DIR(pindex, JOY_DIR_UP);
		joyLookupTable[mask|0x00] = JOY_MAP_DIR(pindex, JOY_DIR_LEFT);
		joyLookupTable[mask|0x01] = JOY_MAP_DIR(pindex, JOY_DIR_RIGHT);
		joyLookupTable[mask|0x03] = JOY_MAP_DIR(pindex, JOY_DIR_DOWN);

		for (int i = 0; i < 9; i++)
			joyLookupTable[mask|(0x80 + i)] = JOY_MAP_BUTTON(pindex, i);
	}

	return 1;
}

static int readConfigFile(int pindex, const char *path, int sixButton)
{
	FILE *f = fopen(path, "r");
	if (!f)
		return 0;

	char line[256];
	while (fgets(line, sizeof(line), f)) {
		char *delim = strchr(line, '=');
		if (!delim) continue;
		*delim = '\0';

		char *eol = strchr(delim, '\n');
		if (!eol) continue;
		*eol = '\0';

		int hbindex = atoi(delim + 1) - 1;
		if (pindex == 0 && strncmp(line, "TEST", 4) == 0) {
			joyLookupTable[FBK_F2] = JOY_MAP_BUTTON(pindex, hbindex);
		} else if (pindex == 0 && strncmp(line, "SERVICE", 7) == 0) {
			joyLookupTable[FBK_9] = JOY_MAP_BUTTON(pindex, hbindex);
		} else if (pindex == 0 && strncmp(line, "RESET", 5) == 0) {
			joyLookupTable[FBK_F3] = JOY_MAP_BUTTON(pindex, hbindex);
		} else if (pindex == 0 && strncmp(line, "QUIT", 4) == 0) {
			joyLookupTable[FBK_ESCAPE] = JOY_MAP_BUTTON(pindex, hbindex);
		} else if (strncmp(line, "START", 5) == 0) {
			joyLookupTable[FBK_1 + pindex] = JOY_MAP_BUTTON(pindex, hbindex);
		} else if (strncmp(line, "COIN", 4) == 0) {
			joyLookupTable[FBK_5 + pindex] = JOY_MAP_BUTTON(pindex, hbindex);
		} else if (pindex == 0) {
			if (strncmp(line, "BUTTON", 6) == 0) {
				int vbindex = atoi(line + 6) - 1;
				if (sixButton && vbindex < 6)
					joyLookupTable[capcom6Layout[vbindex]] = JOY_MAP_BUTTON(pindex, hbindex);
				else if (!sixButton && vbindex < 9)
					joyLookupTable[defaultLayout[vbindex]] = JOY_MAP_BUTTON(pindex, hbindex);
			} else if (strncmp(line, "UP", 2) == 0) {
				joyLookupTable[FBK_UPARROW] = JOY_MAP_DIR(pindex, JOY_DIR_UP);
			} else if (strncmp(line, "DOWN", 4) == 0) {
				joyLookupTable[FBK_DOWNARROW] = JOY_MAP_DIR(pindex, JOY_DIR_DOWN);
			} else if (strncmp(line, "LEFT", 4) == 0) {
				joyLookupTable[FBK_LEFTARROW] = JOY_MAP_DIR(pindex, JOY_DIR_LEFT);
			} else if (strncmp(line, "RIGHT", 5) == 0) {
				joyLookupTable[FBK_RIGHTARROW] = JOY_MAP_DIR(pindex, JOY_DIR_RIGHT);
			}
		} else /* if (pIndex > 0) */ {
			unsigned int mask = 0x4000 + (0x100 * (pindex - 1));
			if (strncmp(line, "BUTTON", 6) == 0) {
				int vbindex = atoi(line + 6) - 1;
				joyLookupTable[mask | (0x80 + vbindex)] = JOY_MAP_BUTTON(pindex, hbindex);
			} else if (strncmp(line, "UP", 2) == 0) {
				joyLookupTable[mask | 0x02] = JOY_MAP_DIR(pindex, JOY_DIR_UP);
			} else if (strncmp(line, "DOWN", 4) == 0) {
				joyLookupTable[mask | 0x03] = JOY_MAP_DIR(pindex, JOY_DIR_DOWN);
			} else if (strncmp(line, "LEFT", 4) == 0) {
				joyLookupTable[mask | 0x00] = JOY_MAP_DIR(pindex, JOY_DIR_LEFT);
			} else if (strncmp(line, "RIGHT", 5) == 0) {
				joyLookupTable[mask | 0x01] = JOY_MAP_DIR(pindex, JOY_DIR_RIGHT);
			}
		}
	}
	fclose(f);

	return 1;
}

static int configExists(char *path, int pathSize, const char *name)
{
	snprintf(path, pathSize, "joyconfig/%s.jc", name);
	return access(path, F_OK) != -1;
}

static void resetJoystickMap()
{
	for (int i = 0; i < 0x8000; i++) {
		joyLookupTable[i] = -1;
	}
	
	for (int i = 0; i < 512; i++) {
		int code = SDLtoFBK[i];
		keyLookupTable[code] = (code > 0) ? i : -1;
	}

	char path[512];
	int sixButton = usesStreetFighterLayout();

	int loadConfig = configExists(path, sizeof(path), BurnDrvGetText(DRV_NAME))
		|| configExists(path, sizeof(path), BurnDrvGetText(DRV_PARENT))
		|| (sixButton && configExists(path, sizeof(path), "capcom6"))
		|| configExists(path, sizeof(path), "default");

	if (loadConfig)
		printf("Found joyconfig file in path '%s'\n", path);

	for (int i = 0; i < 4; i++) {
		setupDefaults(i);
		if (loadConfig)
			readConfigFile(i, path, sixButton);
	}
}

struct InputInOut InputInOutSDL = { piInputInit, piInputExit, NULL, piInputStart, piInputState, NULL, NULL, NULL, NULL, NULL, _T("Raspberry Pi input") };
