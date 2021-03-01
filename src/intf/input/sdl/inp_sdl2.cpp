// Module for input using SDL
#include <SDL.h>

#include "burner.h"


#define MAX_JOYSTICKS (8)

static int FBKtoSDL[512] = { 0 };
static int SDLtoFBK[512] = { -1 };
static int nInitedSubsytems = 0;
static SDL_Joystick* JoyList[MAX_JOYSTICKS];
static int* JoyPrevAxes = NULL;
static int nJoystickCount = 0;						// Number of joysticks connected to this machine
int buttons [4][8]= { {-1,-1,-1,-1,-1,-1,-1,-1}, {-1,-1,-1,-1,-1,-1,-1,-1}, {-1,-1,-1,-1,-1,-1,-1,-1}, {-1,-1,-1,-1,-1,-1,-1,-1} }; // 4 joysticks buttons 0 -5 and start / select

void setup_kemaps(void)
{
	SDLtoFBK[SDL_SCANCODE_UNKNOWN] = 0;
	SDLtoFBK[SDL_SCANCODE_A] = FBK_A;
	SDLtoFBK[SDL_SCANCODE_B] = FBK_B;
	SDLtoFBK[SDL_SCANCODE_C] = FBK_C;
	SDLtoFBK[SDL_SCANCODE_D] = FBK_D;
	SDLtoFBK[SDL_SCANCODE_E] = FBK_E;
	SDLtoFBK[SDL_SCANCODE_F] = FBK_F;
	SDLtoFBK[SDL_SCANCODE_G] = FBK_G;
	SDLtoFBK[SDL_SCANCODE_H] = FBK_H;
	SDLtoFBK[SDL_SCANCODE_I] = FBK_I;
	SDLtoFBK[SDL_SCANCODE_J] = FBK_J;
	SDLtoFBK[SDL_SCANCODE_K] = FBK_K;
	SDLtoFBK[SDL_SCANCODE_L] = FBK_L;
	SDLtoFBK[SDL_SCANCODE_M] = FBK_M;
	SDLtoFBK[SDL_SCANCODE_N] = FBK_N;
	SDLtoFBK[SDL_SCANCODE_O] = FBK_O;
	SDLtoFBK[SDL_SCANCODE_P] = FBK_P;
	SDLtoFBK[SDL_SCANCODE_Q] = FBK_Q;
	SDLtoFBK[SDL_SCANCODE_R] = FBK_R;
	SDLtoFBK[SDL_SCANCODE_S] = FBK_S;
	SDLtoFBK[SDL_SCANCODE_T] = FBK_T;
	SDLtoFBK[SDL_SCANCODE_U] = FBK_U;
	SDLtoFBK[SDL_SCANCODE_V] = FBK_V;
	SDLtoFBK[SDL_SCANCODE_W] = FBK_W;
	SDLtoFBK[SDL_SCANCODE_X] = FBK_X;
	SDLtoFBK[SDL_SCANCODE_Y] = FBK_Y;
	SDLtoFBK[SDL_SCANCODE_Z] = FBK_Z;

	SDLtoFBK[SDL_SCANCODE_1] = FBK_1;
	SDLtoFBK[SDL_SCANCODE_2] = FBK_2;
	SDLtoFBK[SDL_SCANCODE_3] = FBK_3;
	SDLtoFBK[SDL_SCANCODE_4] = FBK_4;
	SDLtoFBK[SDL_SCANCODE_5] = FBK_5;
	SDLtoFBK[SDL_SCANCODE_6] = FBK_6;
	SDLtoFBK[SDL_SCANCODE_7] = FBK_7;
	SDLtoFBK[SDL_SCANCODE_8] = FBK_8;
	SDLtoFBK[SDL_SCANCODE_9] = FBK_9;
	SDLtoFBK[SDL_SCANCODE_0] = FBK_0;

	SDLtoFBK[SDL_SCANCODE_RETURN] = FBK_RETURN;
	SDLtoFBK[SDL_SCANCODE_ESCAPE] = FBK_ESCAPE;
	SDLtoFBK[SDL_SCANCODE_BACKSPACE] = FBK_BACK;
	SDLtoFBK[SDL_SCANCODE_TAB] = FBK_TAB;
	SDLtoFBK[SDL_SCANCODE_SPACE] = FBK_SPACE;
	SDLtoFBK[SDL_SCANCODE_MINUS] = FBK_MINUS;
	SDLtoFBK[SDL_SCANCODE_EQUALS] = FBK_EQUALS;
	SDLtoFBK[SDL_SCANCODE_LEFTBRACKET] = FBK_LBRACKET;
	SDLtoFBK[SDL_SCANCODE_RIGHTBRACKET] = FBK_RBRACKET;
	SDLtoFBK[SDL_SCANCODE_BACKSLASH] = FBK_BACKSLASH;
	SDLtoFBK[SDL_SCANCODE_NONUSHASH] = -1;
	SDLtoFBK[SDL_SCANCODE_SEMICOLON] = FBK_SEMICOLON;
	SDLtoFBK[SDL_SCANCODE_APOSTROPHE] = FBK_APOSTROPHE;
	SDLtoFBK[SDL_SCANCODE_GRAVE] = FBK_GRAVE;
	SDLtoFBK[SDL_SCANCODE_COMMA] = FBK_COMMA;
	SDLtoFBK[SDL_SCANCODE_PERIOD] = FBK_PERIOD;
	SDLtoFBK[SDL_SCANCODE_SLASH] = FBK_SLASH;
	SDLtoFBK[SDL_SCANCODE_CAPSLOCK] = FBK_CAPITAL;

	SDLtoFBK[SDL_SCANCODE_F1] = FBK_F1;
	SDLtoFBK[SDL_SCANCODE_F2] = FBK_F2;
	SDLtoFBK[SDL_SCANCODE_F3] = FBK_F3;
	SDLtoFBK[SDL_SCANCODE_F4] = FBK_F4;
	SDLtoFBK[SDL_SCANCODE_F5] = FBK_F5;
	SDLtoFBK[SDL_SCANCODE_F6] = FBK_F6;
	SDLtoFBK[SDL_SCANCODE_F7] = FBK_F7;
	SDLtoFBK[SDL_SCANCODE_F8] = FBK_F8;
	SDLtoFBK[SDL_SCANCODE_F9] = FBK_F9;
	SDLtoFBK[SDL_SCANCODE_F10] = FBK_F10;
	SDLtoFBK[SDL_SCANCODE_F11] = FBK_F11;
	SDLtoFBK[SDL_SCANCODE_F12] = FBK_F12;

	SDLtoFBK[SDL_SCANCODE_PRINTSCREEN] = FBK_SYSRQ;
	SDLtoFBK[SDL_SCANCODE_SCROLLLOCK] = FBK_SCROLL;
	SDLtoFBK[SDL_SCANCODE_PAUSE] = FBK_PAUSE;
	SDLtoFBK[SDL_SCANCODE_INSERT] = FBK_INSERT;
	SDLtoFBK[SDL_SCANCODE_HOME] = FBK_HOME;
	SDLtoFBK[SDL_SCANCODE_PAGEUP] = FBK_PRIOR;
	SDLtoFBK[SDL_SCANCODE_DELETE] = FBK_DELETE;
	SDLtoFBK[SDL_SCANCODE_END] = FBK_END;
	SDLtoFBK[SDL_SCANCODE_PAGEDOWN] = FBK_NEXT;
	SDLtoFBK[SDL_SCANCODE_RIGHT] = FBK_RIGHTARROW;
	SDLtoFBK[SDL_SCANCODE_LEFT] = FBK_LEFTARROW;
	SDLtoFBK[SDL_SCANCODE_DOWN] = FBK_DOWNARROW;
	SDLtoFBK[SDL_SCANCODE_UP] = FBK_UPARROW;

	SDLtoFBK[SDL_SCANCODE_NUMLOCKCLEAR] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_DIVIDE] = FBK_DIVIDE;
	SDLtoFBK[SDL_SCANCODE_KP_MULTIPLY] = FBK_MULTIPLY;
	SDLtoFBK[SDL_SCANCODE_KP_MINUS] = FBK_SUBTRACT;
	SDLtoFBK[SDL_SCANCODE_KP_PLUS] = FBK_ADD;
	SDLtoFBK[SDL_SCANCODE_KP_ENTER] = FBK_NUMPADENTER;
	SDLtoFBK[SDL_SCANCODE_KP_1] = FBK_NUMPAD1;
	SDLtoFBK[SDL_SCANCODE_KP_2] = FBK_NUMPAD2;
	SDLtoFBK[SDL_SCANCODE_KP_3] = FBK_NUMPAD3;
	SDLtoFBK[SDL_SCANCODE_KP_4] = FBK_NUMPAD4;
	SDLtoFBK[SDL_SCANCODE_KP_5] = FBK_NUMPAD5;
	SDLtoFBK[SDL_SCANCODE_KP_6] = FBK_NUMPAD6;
	SDLtoFBK[SDL_SCANCODE_KP_7] = FBK_NUMPAD7;
	SDLtoFBK[SDL_SCANCODE_KP_8] = FBK_NUMPAD8;
	SDLtoFBK[SDL_SCANCODE_KP_9] = FBK_NUMPAD9;
	SDLtoFBK[SDL_SCANCODE_KP_0] = FBK_NUMPAD0;
	SDLtoFBK[SDL_SCANCODE_KP_PERIOD] = FBK_DECIMAL;

	SDLtoFBK[SDL_SCANCODE_NONUSBACKSLASH] = -1;
	SDLtoFBK[SDL_SCANCODE_APPLICATION] = -1;
	SDLtoFBK[SDL_SCANCODE_POWER] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_EQUALS] = FBK_NUMPADEQUALS;
	SDLtoFBK[SDL_SCANCODE_F13] = -1;
	SDLtoFBK[SDL_SCANCODE_F14] = -1;
	SDLtoFBK[SDL_SCANCODE_F15] = -1;
	SDLtoFBK[SDL_SCANCODE_F16] = -1;
	SDLtoFBK[SDL_SCANCODE_F17] = -1;
	SDLtoFBK[SDL_SCANCODE_F18] = -1;
	SDLtoFBK[SDL_SCANCODE_F19] = -1;
	SDLtoFBK[SDL_SCANCODE_F20] = -1;
	SDLtoFBK[SDL_SCANCODE_F21] = -1;
	SDLtoFBK[SDL_SCANCODE_F22] = -1;
	SDLtoFBK[SDL_SCANCODE_F23] = -1;
	SDLtoFBK[SDL_SCANCODE_F24] = -1;
	SDLtoFBK[SDL_SCANCODE_EXECUTE] = -1;
	SDLtoFBK[SDL_SCANCODE_HELP] = -1;
	SDLtoFBK[SDL_SCANCODE_MENU] = -1;
	SDLtoFBK[SDL_SCANCODE_SELECT] = -1;
	SDLtoFBK[SDL_SCANCODE_STOP] = -1;
	SDLtoFBK[SDL_SCANCODE_AGAIN] = -1;
	SDLtoFBK[SDL_SCANCODE_UNDO] = -1;
	SDLtoFBK[SDL_SCANCODE_CUT] = -1;
	SDLtoFBK[SDL_SCANCODE_COPY] = -1;
	SDLtoFBK[SDL_SCANCODE_PASTE] = -1;
	SDLtoFBK[SDL_SCANCODE_FIND] = -1;
	SDLtoFBK[SDL_SCANCODE_MUTE] = -1;
	SDLtoFBK[SDL_SCANCODE_VOLUMEUP] = -1;
	SDLtoFBK[SDL_SCANCODE_VOLUMEDOWN] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_COMMA] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_EQUALSAS400] = -1;
	SDLtoFBK[SDL_SCANCODE_INTERNATIONAL1] = -1;
	SDLtoFBK[SDL_SCANCODE_INTERNATIONAL2] = -1;
	SDLtoFBK[SDL_SCANCODE_INTERNATIONAL3] = -1;
	SDLtoFBK[SDL_SCANCODE_INTERNATIONAL4] = -1;
	SDLtoFBK[SDL_SCANCODE_INTERNATIONAL5] = -1;
	SDLtoFBK[SDL_SCANCODE_INTERNATIONAL6] = -1;
	SDLtoFBK[SDL_SCANCODE_INTERNATIONAL7] = -1;
	SDLtoFBK[SDL_SCANCODE_INTERNATIONAL8] = -1;
	SDLtoFBK[SDL_SCANCODE_INTERNATIONAL9] = -1;
	SDLtoFBK[SDL_SCANCODE_LANG1] = -1;
	SDLtoFBK[SDL_SCANCODE_LANG2] = -1;
	SDLtoFBK[SDL_SCANCODE_LANG3] = -1;
	SDLtoFBK[SDL_SCANCODE_LANG4] = -1;
	SDLtoFBK[SDL_SCANCODE_LANG5] = -1;
	SDLtoFBK[SDL_SCANCODE_LANG6] = -1;
	SDLtoFBK[SDL_SCANCODE_LANG7] = -1;
	SDLtoFBK[SDL_SCANCODE_LANG8] = -1;
	SDLtoFBK[SDL_SCANCODE_LANG9] = -1;
	SDLtoFBK[SDL_SCANCODE_ALTERASE] = -1;
	SDLtoFBK[SDL_SCANCODE_SYSREQ] = -1;
	SDLtoFBK[SDL_SCANCODE_CANCEL] = -1;
	SDLtoFBK[SDL_SCANCODE_CLEAR] = -1;
	SDLtoFBK[SDL_SCANCODE_PRIOR] = -1;
	SDLtoFBK[SDL_SCANCODE_RETURN2] = -1;
	SDLtoFBK[SDL_SCANCODE_SEPARATOR] = -1;
	SDLtoFBK[SDL_SCANCODE_OUT] = -1;
	SDLtoFBK[SDL_SCANCODE_OPER] = -1;
	SDLtoFBK[SDL_SCANCODE_CLEARAGAIN] = -1;
	SDLtoFBK[SDL_SCANCODE_CRSEL] = -1;
	SDLtoFBK[SDL_SCANCODE_EXSEL] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_00] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_000] = -1;
	SDLtoFBK[SDL_SCANCODE_THOUSANDSSEPARATOR] = -1;
	SDLtoFBK[SDL_SCANCODE_DECIMALSEPARATOR] = -1;
	SDLtoFBK[SDL_SCANCODE_CURRENCYUNIT] = -1;
	SDLtoFBK[SDL_SCANCODE_CURRENCYSUBUNIT] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_LEFTPAREN] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_RIGHTPAREN] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_LEFTBRACE] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_RIGHTBRACE] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_TAB] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_BACKSPACE] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_A] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_B] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_C] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_D] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_E] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_F] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_XOR] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_POWER] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_PERCENT] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_LESS] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_GREATER] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_AMPERSAND] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_DBLAMPERSAND] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_VERTICALBAR] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_DBLVERTICALBAR] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_COLON] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_HASH] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_SPACE] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_AT] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_EXCLAM] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_MEMSTORE] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_MEMRECALL] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_MEMCLEAR] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_MEMADD] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_MEMSUBTRACT] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_MEMMULTIPLY] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_MEMDIVIDE] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_PLUSMINUS] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_CLEAR] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_CLEARENTRY] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_BINARY] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_OCTAL] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_DECIMAL] = -1;
	SDLtoFBK[SDL_SCANCODE_KP_HEXADECIMAL] = -1;
	SDLtoFBK[SDL_SCANCODE_LCTRL] = -1;
	SDLtoFBK[SDL_SCANCODE_LSHIFT] = -1;
	SDLtoFBK[SDL_SCANCODE_LALT] = -1;
	SDLtoFBK[SDL_SCANCODE_LGUI] = -1;
	SDLtoFBK[SDL_SCANCODE_RCTRL] = -1;
	SDLtoFBK[SDL_SCANCODE_RSHIFT] = -1;
	SDLtoFBK[SDL_SCANCODE_RALT] = -1;
	SDLtoFBK[SDL_SCANCODE_RGUI] = -1;
	SDLtoFBK[SDL_SCANCODE_MODE] = -1;
	SDLtoFBK[SDL_SCANCODE_AUDIONEXT] = -1;
	SDLtoFBK[SDL_SCANCODE_AUDIOPREV] = -1;
	SDLtoFBK[SDL_SCANCODE_AUDIOSTOP] = -1;
	SDLtoFBK[SDL_SCANCODE_AUDIOPLAY] = -1;
	SDLtoFBK[SDL_SCANCODE_AUDIOMUTE] = -1;
	SDLtoFBK[SDL_SCANCODE_MEDIASELECT] = -1;
	SDLtoFBK[SDL_SCANCODE_WWW] = -1;
	SDLtoFBK[SDL_SCANCODE_MAIL] = -1;
	SDLtoFBK[SDL_SCANCODE_CALCULATOR] = -1;
	SDLtoFBK[SDL_SCANCODE_COMPUTER] = -1;
	SDLtoFBK[SDL_SCANCODE_AC_SEARCH] = -1;
	SDLtoFBK[SDL_SCANCODE_AC_HOME] = -1;
	SDLtoFBK[SDL_SCANCODE_AC_BACK] = -1;
	SDLtoFBK[SDL_SCANCODE_AC_FORWARD] = -1;
	SDLtoFBK[SDL_SCANCODE_AC_STOP] = -1;
	SDLtoFBK[SDL_SCANCODE_AC_REFRESH] = -1;
	SDLtoFBK[SDL_SCANCODE_AC_BOOKMARKS] = -1;
	SDLtoFBK[SDL_SCANCODE_BRIGHTNESSDOWN] = -1;
	SDLtoFBK[SDL_SCANCODE_BRIGHTNESSUP] = -1;
	SDLtoFBK[SDL_SCANCODE_DISPLAYSWITCH] = -1;
	SDLtoFBK[SDL_SCANCODE_KBDILLUMTOGGLE] = -1;
	SDLtoFBK[SDL_SCANCODE_KBDILLUMDOWN] = -1;
	SDLtoFBK[SDL_SCANCODE_KBDILLUMUP] = -1;
	SDLtoFBK[SDL_SCANCODE_EJECT] = -1;
	SDLtoFBK[SDL_SCANCODE_SLEEP] = -1;
	SDLtoFBK[SDL_SCANCODE_APP1] = -1;
	SDLtoFBK[SDL_SCANCODE_APP2] = -1;
	SDLtoFBK[SDL_SCANCODE_AUDIOREWIND] = -1;
	SDLtoFBK[SDL_SCANCODE_AUDIOFASTFORWARD] = -1;
};

// Sets up one Joystick (for example the range of the joystick's axes)
static int SDLinpJoystickInit(int i)
{
   SDL_GameController *temp;
	SDL_GameControllerButtonBind bind;
 
   JoyList[i] = SDL_JoystickOpen(i);

// need this for any mapps that need done might just read a local file and do a readme on how to add your controller this will do for now
   SDL_JoystickGUID guid = SDL_JoystickGetGUID(JoyList[i]);
   char guid_str[1024];
   SDL_JoystickGetGUIDString(guid, guid_str, sizeof(guid_str));


   char *mapping ="03000000ff1100003133000000000000,GameStation Gear Pc control pad,a:b2,b:b1,y:b0,x:b3,start:b11,back:b10,leftshoulder:b4,rightshoulder:b5,dpup:h0.1,dpleft:h0.8,dpdown:h0.4,dpright:h0.2,leftx:a0,lefty:a1,rightx:a2,righty:a4,lefttrigger:b6,righttrigger:b7,leftstick:b8,rightstick:b9,";
// remap my problamatic old gamepad that conflicts with sdl2 see https://github.com/gabomdq/SDL_GameControllerDB/issues/308#event-2941370014
   const char* name = SDL_JoystickName(JoyList[i]);
   if ( (strcmp(guid_str, "03000000ff1100003133000000000000") == 0) && (strcmp(name, "PC Game Controller       ") == 0) )
   {

      SDL_GameControllerAddMapping(mapping) ;
   }

   temp = SDL_GameControllerOpen(i);
   mapping = SDL_GameControllerMapping(temp);
   printf("mapping %s\n",mapping);   
   bind = SDL_GameControllerGetBindForButton(temp, SDL_CONTROLLER_BUTTON_A );
   
   bind = SDL_GameControllerGetBindForButton(temp, SDL_CONTROLLER_BUTTON_A );
   buttons[i][0] = bind.value.button;

   bind = SDL_GameControllerGetBindForButton(temp, SDL_CONTROLLER_BUTTON_B);
   buttons[i][1] = bind.value.button;

   bind = SDL_GameControllerGetBindForButton(temp, SDL_CONTROLLER_BUTTON_X );
   buttons[i][2] = bind.value.button;
   
   bind = SDL_GameControllerGetBindForButton(temp, SDL_CONTROLLER_BUTTON_Y);
   buttons[i][3] = bind.value.button;

   bind = SDL_GameControllerGetBindForButton(temp, SDL_CONTROLLER_BUTTON_LEFTSHOULDER  );
   buttons[i][4] = bind.value.button;

   bind = SDL_GameControllerGetBindForButton(temp, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER );
   buttons[i][5] = bind.value.button;

   bind = SDL_GameControllerGetBindForButton(temp, SDL_CONTROLLER_BUTTON_BACK   );
   buttons[i][6] = bind.value.button;

   bind = SDL_GameControllerGetBindForButton(temp, SDL_CONTROLLER_BUTTON_START  );
   buttons[i][7] = bind.value.button;



	return 0;
}

// Set up the keyboard
static int SDLinpKeyboardInit()
{
	for (int i = 0; i < 512; i++) {
		if (SDLtoFBK[i] > 0)
			FBKtoSDL[SDLtoFBK[i]] = i;
	}

	return 0;
}

// Get an interface to the mouse
static int SDLinpMouseInit()
{
	return 0;
}

int SDLinpSetCooperativeLevel(bool bExclusive, bool /*bForeGround*/)
{
	SDL_SetRelativeMouseMode((bDrvOkay && (bExclusive || nVidFullscreen)) ? SDL_TRUE : SDL_FALSE);

	return 0;
}

int SDLinpExit()
{
	// Close all joysticks
	for (int i = 0; i < MAX_JOYSTICKS; i++) {
		if (JoyList[i]) {
			SDL_JoystickClose(JoyList[i]);
			JoyList[i] = NULL;
		}
	}

	nJoystickCount = 0;

	free(JoyPrevAxes);
	JoyPrevAxes = NULL;

	nInitedSubsytems = 0;

	return 0;
}

int SDLinpInit()
{
	int nSize;
	setup_kemaps();
	SDLinpExit();

	memset(&JoyList, 0, sizeof(JoyList));

	nSize = MAX_JOYSTICKS * 8 * sizeof(int);
	if ((JoyPrevAxes = (int*)malloc(nSize)) == NULL) {
		SDLinpExit();
		return 1;
	}
	memset(JoyPrevAxes, 0, nSize);

	nInitedSubsytems = SDL_WasInit(SDL_INIT_JOYSTICK);

	if (!(nInitedSubsytems & SDL_INIT_JOYSTICK)) {
		SDL_Init(SDL_INIT_JOYSTICK);
	}

	// Set up the joysticks
	nJoystickCount = SDL_NumJoysticks();
	for (int i = 0; i < nJoystickCount; i++) {
		SDLinpJoystickInit(i);
	}
	SDL_JoystickEventState(SDL_IGNORE);

	// Set up the keyboard
	SDLinpKeyboardInit();

	// Set up the mouse
	SDLinpMouseInit();

	return 0;
}

static unsigned char bKeyboardRead = 0;
const Uint8* SDLinpKeyboardState;

static unsigned char bJoystickRead = 0;

static unsigned char bMouseRead = 0;
static struct { unsigned char buttons; int xdelta; int ydelta; } SDLinpMouseState;

#define SDL_KEY_IS_DOWN(key) (FBKtoSDL[key] > 0 ? SDLinpKeyboardState[FBKtoSDL[key]] : 0)

// Call before checking for Input in a frame
int SDLinpStart()
{
	// Update SDL event queue
	SDL_PumpEvents();

	// Keyboard not read this frame
	bKeyboardRead = 0;

	// No joysticks have been read for this frame
	bJoystickRead = 0;

	// Mouse not read this frame
	bMouseRead = 0;

	return 0;
}

// Read one of the joysticks
static int ReadJoystick()
{
	if (bJoystickRead) {
		return 0;
	}

	SDL_JoystickUpdate();

	// All joysticks have been Read this frame
	bJoystickRead = 1;

	return 0;
}

// Read one joystick axis
int SDLinpJoyAxis(int i, int nAxis)
{
	if (i < 0 || i >= nJoystickCount) {				// This joystick number isn't connected
		return 0;
	}

	if (ReadJoystick() != 0) {						// There was an error polling the joystick
		return 0;
	}

	if (nAxis >= SDL_JoystickNumAxes(JoyList[i])) {
		return 0;
	}

	return SDL_JoystickGetAxis(JoyList[i], nAxis) << 1;
}

// Read the keyboard
static int ReadKeyboard()
{
	int numkeys;

	if (bKeyboardRead) {							// already read this frame - ready to go
		return 0;
	}

	SDLinpKeyboardState = SDL_GetKeyboardState(&numkeys);
	if (SDLinpKeyboardState == NULL) {
		return 1;
	}
	// The keyboard has been successfully Read this frame
	bKeyboardRead = 1;

	return 0;
}

static int ReadMouse()
{
	if (bMouseRead) {
		return 0;
	}

	SDLinpMouseState.buttons = SDL_GetRelativeMouseState(&(SDLinpMouseState.xdelta), &(SDLinpMouseState.ydelta));

	bMouseRead = 1;

	return 0;
}

// Read one mouse axis
int SDLinpMouseAxis(int i, int nAxis)
{
	if (i < 0 || i >= 1) {									// Only the system mouse is supported by SDL
		return 0;
	}

	switch (nAxis) {
	case 0:
		return SDLinpMouseState.xdelta;
	case 1:
		return SDLinpMouseState.ydelta;
	}

	return 0;
}

// Check a subcode (the 40xx bit in 4001, 4102 etc) for a joystick input code
static int JoystickState(int i, int nSubCode)
{
	if (i < 0 || i >= nJoystickCount) {							// This joystick isn't connected
		return 0;
	}

	if (ReadJoystick() != 0) {									// Error polling the joystick
		return 0;
	}

	if (nSubCode < 0x10) {										// Joystick directions
		const int DEADZONE = 0x4000;

		if (SDL_JoystickNumAxes(JoyList[i]) <= nSubCode) {
			return 0;
		}

		switch (nSubCode) {
		case 0x00: return SDL_JoystickGetAxis(JoyList[i], 0) < -DEADZONE;		// Left
		case 0x01: return SDL_JoystickGetAxis(JoyList[i], 0) > DEADZONE;		// Right
		case 0x02: return SDL_JoystickGetAxis(JoyList[i], 1) < -DEADZONE;		// Up
		case 0x03: return SDL_JoystickGetAxis(JoyList[i], 1) > DEADZONE;		// Down
		case 0x04: return SDL_JoystickGetAxis(JoyList[i], 2) < -DEADZONE;
		case 0x05: return SDL_JoystickGetAxis(JoyList[i], 2) > DEADZONE;
		case 0x06: return SDL_JoystickGetAxis(JoyList[i], 3) < -DEADZONE;
		case 0x07: return SDL_JoystickGetAxis(JoyList[i], 3) > DEADZONE;
		case 0x08: return SDL_JoystickGetAxis(JoyList[i], 4) < -DEADZONE;
		case 0x09: return SDL_JoystickGetAxis(JoyList[i], 4) > DEADZONE;
		case 0x0A: return SDL_JoystickGetAxis(JoyList[i], 5) < -DEADZONE;
		case 0x0B: return SDL_JoystickGetAxis(JoyList[i], 5) > DEADZONE;
		case 0x0C: return SDL_JoystickGetAxis(JoyList[i], 6) < -DEADZONE;
		case 0x0D: return SDL_JoystickGetAxis(JoyList[i], 6) > DEADZONE;
		case 0x0E: return SDL_JoystickGetAxis(JoyList[i], 7) < -DEADZONE;
		case 0x0F: return SDL_JoystickGetAxis(JoyList[i], 7) > DEADZONE;
		}
	}
	if (nSubCode < 0x20) {										// POV hat controls
		if (SDL_JoystickNumHats(JoyList[i]) <= ((nSubCode & 0x0F) >> 2)) {
			return 0;
		}

		switch (nSubCode & 3) {
		case 0:												// Left
			return SDL_JoystickGetHat(JoyList[i], (nSubCode & 0x0F) >> 2)& SDL_HAT_LEFT;
		case 1:												// Right
			return SDL_JoystickGetHat(JoyList[i], (nSubCode & 0x0F) >> 2)& SDL_HAT_RIGHT;
		case 2:												// Up
			return SDL_JoystickGetHat(JoyList[i], (nSubCode & 0x0F) >> 2)& SDL_HAT_UP;
		case 3:												// Down
			return SDL_JoystickGetHat(JoyList[i], (nSubCode & 0x0F) >> 2)& SDL_HAT_DOWN;
		}

		return 0;
	}
	if (nSubCode < 0x80) {										// Undefined
		return 0;
	}
	if (nSubCode < 0x80 + SDL_JoystickNumButtons(JoyList[i])) {	// Joystick buttons
		return SDL_JoystickGetButton(JoyList[i], nSubCode & 0x7F);
	}

	return 0;
}

// Check a subcode (the 80xx bit in 8001, 8102 etc) for a mouse input code
static int CheckMouseState(unsigned int nSubCode)
{
	switch (nSubCode & 0x7F) {
	case 0:
		return (SDLinpMouseState.buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
	case 1:
		return (SDLinpMouseState.buttons & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;
	case 2:
		return (SDLinpMouseState.buttons & SDL_BUTTON(SDL_BUTTON_MIDDLE)) != 0;
	}

	return 0;
}

// Get the state (pressed = 1, not pressed = 0) of a particular input code
int SDLinpState(int nCode)
{
	if (nCode < 0) {
		return 0;
	}

	if (nCode < 0x100) {
		if (ReadKeyboard() != 0) {							// Check keyboard has been read - return not pressed on error
			return 0;
		}
		return SDL_KEY_IS_DOWN(nCode);							// Return key state
	}

	if (nCode < 0x4000) {
		return 0;
	}

	if (nCode < 0x8000) {
		// Codes 4000-8000 = Joysticks
		int nJoyNumber = (nCode - 0x4000) >> 8;

		// Find the joystick state in our array
		return JoystickState(nJoyNumber, nCode & 0xFF);
	}

	if (nCode < 0xC000) {
		// Codes 8000-C000 = Mouse
		if ((nCode - 0x8000) >> 8) {						// Only the system mouse is supported by SDL
			return 0;
		}
		if (ReadMouse() != 0) {								// Error polling the mouse
			return 0;
		}
		return CheckMouseState(nCode & 0xFF);
	}

	return 0;
}

// This function finds which key is pressed, and returns its code
int SDLinpFind(bool CreateBaseline)
{
	int nRetVal = -1;										// assume nothing pressed

	// check if any keyboard keys are pressed
	if (ReadKeyboard() == 0) {
		for (int i = 0; i < 0x100; i++) {
			if (SDL_KEY_IS_DOWN(i) > 0) {
				nRetVal = i;
				goto End;
			}
		}
	}

	// Now check all the connected joysticks
	for (int i = 0; i < nJoystickCount; i++) {
		int j;
		if (ReadJoystick() != 0) {							// There was an error polling the joystick
			continue;
		}

		for (j = 0; j < 0x10; j++) {						// Axes
			int nDelta = JoyPrevAxes[(i << 3) + (j >> 1)] - SDLinpJoyAxis(i, (j >> 1));
			if (nDelta < -0x4000 || nDelta > 0x4000) {
				if (JoystickState(i, j)) {
					nRetVal = 0x4000 | (i << 8) | j;
					goto End;
				}
			}
		}

		for (j = 0x10; j < 0x20; j++) {						// POV hats
			if (JoystickState(i, j)) {
				nRetVal = 0x4000 | (i << 8) | j;
				goto End;
			}
		}

		for (j = 0x80; j < 0x80 + SDL_JoystickNumButtons(JoyList[i]); j++) {
			if (JoystickState(i, j)) {
				nRetVal = 0x4000 | (i << 8) | j;
				goto End;
			}
		}
	}

	// Now the mouse
	if (ReadMouse() == 0) {
		int nDeltaX, nDeltaY;

		for (unsigned int j = 0x80; j < 0x80 + 0x80; j++) {
			if (CheckMouseState(j)) {
				nRetVal = 0x8000 | j;
				goto End;
			}
		}

		nDeltaX = SDLinpMouseAxis(0, 0);
		nDeltaY = SDLinpMouseAxis(0, 1);
		if (abs(nDeltaX) < abs(nDeltaY)) {
			if (nDeltaY != 0) {
				return 0x8000 | 1;
			}
		}
		else {
			if (nDeltaX != 0) {
				return 0x8000 | 0;
			}
		}
	}

End:

	if (CreateBaseline) {
		for (int i = 0; i < nJoystickCount; i++) {
			for (int j = 0; j < 8; j++) {
				JoyPrevAxes[(i << 3) + j] = SDLinpJoyAxis(i, j);
			}
		}
	}

	return nRetVal;
}

int SDLinpGetControlName(int nCode, TCHAR* pszDeviceName, TCHAR* pszControlName)
{
	if (pszDeviceName) {
		pszDeviceName[0] = _T('\0');
	}
	if (pszControlName) {
		pszControlName[0] = _T('\0');
	}

	switch (nCode & 0xC000) {
	case 0x0000: {
		_tcscpy(pszDeviceName, _T("System keyboard"));

		break;
	}
	case 0x4000: {
		int i = (nCode >> 8) & 0x3F;

		if (i >= nJoystickCount) {				// This joystick isn't connected
			return 0;
		}
		//	_tsprintf(pszDeviceName, "%hs", SDL_JoystickName(i));

		break;
	}
	case 0x8000: {
		int i = (nCode >> 8) & 0x3F;

		if (i >= 1) {
			return 0;
		}
		_tcscpy(pszDeviceName, _T("System mouse"));

		break;
	}
	}

	return 0;
}

struct InputInOut InputInOutSDL2 = { SDLinpInit, SDLinpExit, SDLinpSetCooperativeLevel, SDLinpStart, SDLinpState, SDLinpJoyAxis, SDLinpMouseAxis, SDLinpFind, SDLinpGetControlName, NULL, _T("SDL input") };
