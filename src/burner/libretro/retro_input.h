#ifndef __RETRO_INPUT__
#define __RETRO_INPUT__

struct KeyBind
{
	unsigned id;
	unsigned port;
	unsigned device;
	int index;
	unsigned position;
	KeyBind()
	{
		device = RETRO_DEVICE_NONE;
	}
};

struct AxiBind
{
	unsigned id;
	int index;
	AxiBind()
	{
		index = -1;
	}
};

extern retro_input_state_t input_cb;

#define MAX_PLAYERS         6                // highest number of supported players is 6 for xmen6p and a few other games
#define MAX_AXISES          6                // libretro supports 6 analog axises (2 analog button + 2 analog sticks), 4 would probably be sufficient for FBNeo though
#define MAX_KEYBINDS        255              // max number of inputs/macros we can assign
#define SWITCH_NCODE_RESET  (MAX_KEYBINDS+1) // fixed switch ncode for reset button
#define SWITCH_NCODE_DIAG   (MAX_KEYBINDS+2) // fixed switch ncode for diag button

#define RETROPAD_CLASSIC	RETRO_DEVICE_ANALOG
#define RETROPAD_MODERN		RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_ANALOG, 1)
#define RETROMOUSE_BALL		RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_ANALOG, 2)
#define RETROMOUSE_FULL		RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_MOUSE, 1)

#define GIT_SPECIAL_SWITCH	(0x03)
#define GIT_DIRECT_COORD	(0x11)

#define JOY_NEG 0
#define JOY_POS 1

#define RETRO_DEVICE_ID_JOYPAD_EMPTY 255

#define PGI_UP       0
#define PGI_DOWN     1
#define PGI_LEFT     2
#define PGI_RIGHT    3
#define PGI_ANALOG_X 4
#define PGI_ANALOG_Y 5

void SetDiagInpHoldFrameDelay(unsigned val);
void RefreshLightgunCrosshair();
void InputMake(void);
void InputInit();
void InputExit();
void SetDefaultDeviceTypes();
void SetControllerInfo();

#endif
