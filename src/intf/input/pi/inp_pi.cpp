
#include <SDL.h>
#include <unistd.h>
#include <sys/time.h>

#include "burner.h"
#include "inp_sdl_keys.h"

extern "C" {
#include "inp_udev.h"
#include "cJSON.h"
}

int InputFindCode(const char *keystring);

// Note that these constants are tied to the JOY_ macros!
#define MAX_JOYSTICKS   8
#define MAX_JOY_BUTTONS 28

#define FREEPLAY_HACK_COIN_DURATION_MS 1e3 / 8

//#define DEBUG_INPUT

int nKioskTimeout = 0;
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
static void handleFreeplayHack();

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

static unsigned char* keyState = NULL;
static struct {
	unsigned char buttons;
	int xdelta;
	int ydelta;
} mouseState;


static char* globFile(const char *path);

///

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
	phl_udev_shutdown();

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

	phl_udev_init();
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
	if (nKioskTimeout > 0) {
		if (inputEventOccurred) {
			lastInputEvent = now;
			inputEventOccurred = 0;
		} else if (now.tv_sec - lastInputEvent.tv_sec > nKioskTimeout) {
			nExitEmulator = 2;
			fprintf(stderr, "Kiosk mode - %ds timeout exceeded\n", nKioskTimeout);
		}
	}

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

struct JoyConfig {
	char **labels;
	int labelCount;
};

static struct JoyConfig* createJoyConfig(cJSON *root)
{
	struct JoyConfig *jc = (struct JoyConfig *)malloc(sizeof(struct JoyConfig));
	if (jc) {
		jc->labels = NULL;
		jc->labelCount = 0;

		cJSON *labelParent = cJSON_GetObjectItem(root, "labels");
		cJSON *labelNode;
		
		if (labelParent) {
			labelNode = labelParent->child;
			while (labelNode) {
				if (strncmp(labelNode->string, "button", 6) == 0) {
					int index = atoi(labelNode->string + 6);
					if (index > 0 && index <= MAX_JOY_BUTTONS && index > jc->labelCount) {
						jc->labelCount = index;
					}
				}
				labelNode = labelNode->next;
			}

			if (jc->labelCount > 0) {
				jc->labels = (char **)calloc(jc->labelCount, sizeof(char *));
				if (jc->labels) {
					labelNode = labelParent->child;
					while (labelNode) {
						int index = atoi(labelNode->string + 6) - 1;
						if (index >= 0 && index < MAX_JOY_BUTTONS) {
							jc->labels[index] = strdup(labelNode->valuestring);
						}
						labelNode = labelNode->next;
					}
				}
			}
		}
	}

	return jc;
}

static void dumpJoyConfig(struct JoyConfig *jc)
{
	for (int i = 0; i < jc->labelCount; i++) {
		fprintf(stderr, "labels[%d]='%s'\n", i, jc->labels[i]);
	}
}

static void destroyJoyConfig(struct JoyConfig *jc)
{
	for (int i = 0; i < jc->labelCount; i++) {
		free(jc->labels[i]);
	}
	free(jc->labels);
	jc->labels = NULL;
	jc->labelCount = 0;
	free(jc);
}

static int findButton(struct JoyConfig *jc, const char *label)
{
	for (int i = 0; i < jc->labelCount; i++) {
		if (jc->labels[i] && strcasecmp(jc->labels[i], label) == 0) {
			return i;
		}
	}

	if (strncmp(label, "button", 6) == 0) {
		int index = atoi(label + 6) - 1;
		if (index >= 0 && index < MAX_JOY_BUTTONS) {
			return index;
		}
	}

	return -1;
}

static void parseButtons(int player, struct JoyConfig *jc, cJSON *json)
{
	cJSON *node = json->child;
	char temp[100];
	while (node) {
		int index = findButton(jc, node->string);
		if (index > -1) {
			int code;
			if ((code = InputFindCode(node->valuestring)) != -1) {
				joyLookupTable[code] = JOY_MAP_BUTTON(player, index);
			} else {
				// No literal match - try prepending player id
				snprintf(temp, 99, "P%d %s", player + 1, node->valuestring);
				if ((code = InputFindCode(temp)) != -1) {
					joyLookupTable[code] = JOY_MAP_BUTTON(player, index);
				}
			}
		}
		node = node->next;
	}
}

static void parsePlayerConfig(int player, struct JoyConfig *jc, cJSON *json)
{
	cJSON *node;
	int code;

	const char *dirnames[] = { "up","down","left","right" };
	int dirconsts[] = { JOY_DIR_UP,JOY_DIR_DOWN,JOY_DIR_LEFT,JOY_DIR_RIGHT };
	char temp[100];

	for (int i = 0; i < 4; i++) {
		if ((node = cJSON_GetObjectItem(json, dirnames[i]))) {
			if ((code = InputFindCode(node->valuestring)) != -1) {
				joyLookupTable[code] = JOY_MAP_DIR(player, dirconsts[i]);
			} else {
				// No literal match - try prepending player id (e.g. P1, etc)
				snprintf(temp, 99, "P%d %s", player + 1, node->valuestring);
				if ((code = InputFindCode(temp)) != -1) {
					joyLookupTable[code] = JOY_MAP_DIR(player, dirconsts[i]);
				}
			}
		}
	}
	
	parseButtons(player, jc, json);
	if (usesStreetFighterLayout()) {
		cJSON *sf = cJSON_GetObjectItem(json, "sfLayout");
		if (sf) {
			parseButtons(player, jc, sf);
		}
	}
}

static int setupDefaults(int pindex)
{
	int fbDirs[4];
	int fbButtons[6]; // absolute max
	int buttonCount = 0;

	if (pindex == 0) {
		int i = 0;
		fbDirs[i++] = FBK_UPARROW;
		fbDirs[i++] = FBK_LEFTARROW;
		fbDirs[i++] = FBK_RIGHTARROW;
		fbDirs[i++] = FBK_DOWNARROW;

		if (usesStreetFighterLayout()) {
			fbButtons[buttonCount++] = FBK_A;
			fbButtons[buttonCount++] = FBK_S;
			fbButtons[buttonCount++] = FBK_D;
			fbButtons[buttonCount++] = FBK_Z;
			fbButtons[buttonCount++] = FBK_X;
			fbButtons[buttonCount++] = FBK_C;
		} else {
			fbButtons[buttonCount++] = FBK_A;
			fbButtons[buttonCount++] = FBK_S;
			fbButtons[buttonCount++] = FBK_D;
			fbButtons[buttonCount++] = FBK_Z;
//			fbButtons[buttonCount++] = FBK_X;
//			fbButtons[buttonCount++] = FBK_C;
//			fbButtons[buttonCount++] = FBK_V;
		}
	} else if (pindex >= 1 && pindex <= 3) {
		// P2 to P4
		int pmasks[] = { 0x4000, 0x4100, 0x4200 };
		int pmask = pmasks[pindex - 1];

		int i = 0;
		fbDirs[i++] = pmask|0x02;
		fbDirs[i++] = pmask|0x00;
		fbDirs[i++] = pmask|0x01;
		fbDirs[i++] = pmask|0x03;

		for (; buttonCount < 6; buttonCount++) {
			fbButtons[buttonCount] = pmask|(0x80 + buttonCount);
		}
	} else {
		return 0;
	}

	// Set direction defaults
	int joyDirs[] = { JOY_DIR_UP, JOY_DIR_LEFT, JOY_DIR_RIGHT, JOY_DIR_DOWN };
	for (int i = 0; i < 4; i++) {
		joyLookupTable[fbDirs[i]] = JOY_MAP_DIR(pindex, joyDirs[i]);
	}

	// Set button defaults
	for (int i = 0; i < buttonCount; i++) {
		joyLookupTable[fbButtons[i]] = JOY_MAP_BUTTON(pindex, i);
	}

	return 1;
}

static int readConfigFile(int pindex, const char *path)
{
	int success = 0;
	if (pindex >= 0 && pindex <= 3) {
		char *pnodes[] = { "joy1", "joy2", "joy3", "joy4" };
		char *contents = globFile(path);
		if (contents) {
			cJSON *root = cJSON_Parse(contents);
			if (root) {
				struct JoyConfig *jc = createJoyConfig(root);
				
				cJSON *player;
				if ((player = cJSON_GetObjectItem(root, "default"))) {
					parsePlayerConfig(pindex, jc, player);
				}
				if ((player = cJSON_GetObjectItem(root, pnodes[pindex]))) {
					parsePlayerConfig(pindex, jc, player);
				}

				destroyJoyConfig(jc);
				cJSON_Delete(root);

				success = 1;
			}
			free(contents);
		}
	}

	return success;
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

	for (int i = 0, n = phl_udev_joy_count(); i < n; i++) {
		const char *devId = phl_udev_joy_id(i);
		fprintf(stderr, "Detected \"%s\" (USB id %s) in port %d\n",
			phl_udev_joy_name(i), devId, i + 1);

		// Set the defaults
		setupDefaults(i);

		// Try loading a config file
		if (devId != NULL) {
			char path[100];
			snprintf(path, 99, "joyconfig/%s.joy", devId);
			
			char *colon = strchr(path, ':');
			if (colon) *colon = '-';

			if (access(path, F_OK) != -1) {
				fprintf(stderr, " * Found configuration file \"%s\"\n", path);
				if (!readConfigFile(i, path)) {
					fprintf(stderr, "Error reading configuration file - check format\n");
				}
			} else {
				fprintf(stderr, " * No configuration file at \"%s\" - will use defaults\n", path);
			}
		}
	}
}

static char* globFile(const char *path)
{
	char *contents = NULL;
	
	FILE *file = fopen(path,"r");
	if (file) {
		// Determine size
		fseek(file, 0L, SEEK_END);
		long size = ftell(file);
		rewind(file);
	
		// Allocate memory
		contents = (char *)malloc(size + 1);
		if (contents) {
			contents[size] = '\0';
			// Read contents
			if (fread(contents, size, 1, file) != 1) {
				free(contents);
				contents = NULL;
			}
		}

		fclose(file);
	}
	
	return contents;
}

struct InputInOut InputInOutSDL = { piInputInit, piInputExit, NULL, piInputStart, piInputState, NULL, NULL, NULL, NULL, NULL, _T("Raspberry Pi input") };
