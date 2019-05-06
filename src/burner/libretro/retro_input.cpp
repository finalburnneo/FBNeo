#include <vector>
#include "retro_common.h"
#include "retro_input.h"

bool bStreetFighterLayout = false;

static retro_input_state_t input_cb;
static retro_input_poll_t poll_cb;

static unsigned nDiagInputComboStartFrame = 0;
static unsigned nDiagInputHoldFrameDelay = 0;
static unsigned nDeviceType[5] = { -1, -1, -1, -1, -1 };
static unsigned nSwitchCode = 0;
static std::vector<retro_input_descriptor> normal_input_descriptors;
static struct KeyBind sKeyBinds[255];
static struct AxiBind sAxiBinds[5][8]; // 5 players with up to 8 axis
static bool bAnalogRightMappingDone[5][2][2];
static bool bButtonMapped = false;
static bool bOneDiagInputPressed = false;
static bool bAllDiagInputPressed = true;
static bool bDiagComboActivated = false;
static bool bVolumeIsFireButton = false;
static bool bInputInitialized = false;

void retro_set_input_state(retro_input_state_t cb) { input_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { poll_cb = cb; }

void SetDiagInpHoldFrameDelay(unsigned val)
{
	nDiagInputHoldFrameDelay = val;
}

INT32 GameInpBlank(INT32 bDipSwitch)
{
	UINT32 i = 0;
	struct GameInp* pgi = NULL;

	// Reset all inputs to undefined (even dip switches, if bDipSwitch==1)
	if (GameInp == NULL) {
		return 1;
	}

	// Get the targets in the library for the Input Values
	for (i = 0, pgi = GameInp; i < nGameInpCount; i++, pgi++) {
		struct BurnInputInfo bii;
		memset(&bii, 0, sizeof(bii));
		BurnDrvGetInputInfo(&bii, i);
		if (bDipSwitch == 0 && (bii.nType & BIT_GROUP_CONSTANT)) {		// Don't blank the dip switches
			continue;
		}

		memset(pgi, 0, sizeof(*pgi));									// Clear input

		pgi->nType = bii.nType;											// store input type
		pgi->Input.pVal = bii.pVal;										// store input pointer to value

		if (bii.nType & BIT_GROUP_CONSTANT) {							// Further initialisation for constants/DIPs
			pgi->nInput = GIT_CONSTANT;
			pgi->Input.Constant.nConst = *bii.pVal;
		}
	}

	return 0;
}

static void AnalyzeGameLayout()
{
	struct BurnInputInfo bii;

	INT32 nPunchx3[4] = {0, 0, 0, 0};
	INT32 nKickx3[4] = {0, 0, 0, 0};

	bStreetFighterLayout = false;
	bVolumeIsFireButton = false;
	nFireButtons = 0;

	for (UINT32 i = 0; i < nGameInpCount; i++) {
		bii.szName = NULL;
		BurnDrvGetInputInfo(&bii, i);
		if (bii.szName == NULL) {
			bii.szName = "";
		}

		bool bPlayerInInfo = (toupper(bii.szInfo[0]) == 'P' && bii.szInfo[1] >= '1' && bii.szInfo[1] <= '4'); // Because some of the older drivers don't use the standard input naming.
		bool bPlayerInName = (bii.szName[0] == 'P' && bii.szName[1] >= '1' && bii.szName[1] <= '4');

		if (bPlayerInInfo || bPlayerInName) {
			INT32 nPlayer = 0;

			if (bPlayerInName)
				nPlayer = bii.szName[1] - '1';
			if (bPlayerInInfo && nPlayer == 0)
				nPlayer = bii.szInfo[1] - '1';

			if (nPlayer == 0) {
				if (strncmp(" fire", bii.szInfo + 2, 5) == 0) {
					nFireButtons++;
				}
			}
			
			if ((strncmp("Volume", bii.szName, 6) == 0) && (strncmp(" fire", bii.szInfo + 2, 5) == 0)) {
				bVolumeIsFireButton = true;
			}
			if (_stricmp(" Weak Punch", bii.szName + 2) == 0) {
				nPunchx3[nPlayer] |= 1;
			}
			if (_stricmp(" Medium Punch", bii.szName + 2) == 0) {
				nPunchx3[nPlayer] |= 2;
			}
			if (_stricmp(" Strong Punch", bii.szName + 2) == 0) {
				nPunchx3[nPlayer] |= 4;
			}
			if (_stricmp(" Weak Kick", bii.szName + 2) == 0) {
				nKickx3[nPlayer] |= 1;
			}
			if (_stricmp(" Medium Kick", bii.szName + 2) == 0) {
				nKickx3[nPlayer] |= 2;
			}
			if (_stricmp(" Strong Kick", bii.szName + 2) == 0) {
				nKickx3[nPlayer] |= 4;
			}
		}
	}

	if ((nPunchx3[0] == 7) && (nKickx3[0] == 7)) {
		bStreetFighterLayout = true;
	}
	if (nFireButtons >= 5 && (BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_CAPCOM_CPS2 && !bVolumeIsFireButton) {
		bStreetFighterLayout = true;
	}
}

INT32 GameInpInit()
{
	INT32 nRet = 0;
	// Count the number of inputs
	nGameInpCount = 0;

	for (UINT32 i = 0; i < 0x1000; i++) {
		nRet = BurnDrvGetInputInfo(NULL,i);
		if (nRet) {														// end of input list
			nGameInpCount = i;
			break;
		}
	}

	// Allocate space for all the inputs
	INT32 nSize = (nGameInpCount) * sizeof(struct GameInp);
	GameInp = (struct GameInp*)malloc(nSize);
	if (GameInp == NULL) {
		return 1;
	}
	memset(GameInp, 0, nSize);

	GameInpBlank(1);

	InpDIPSWResetDIPs();

	AnalyzeGameLayout();

	return 0;
}

static inline int CinpState(int nCode)
{
	unsigned id = sKeyBinds[nCode].id;
	unsigned port = sKeyBinds[nCode].port;
	int index = sKeyBinds[nCode].index;
	if (index == -1)
	{
		return input_cb(port, sKeyBinds[nCode].device, 0, id);
	}
	else
	{
		int s = input_cb(port, sKeyBinds[nCode].device, index, id);
		unsigned position = sKeyBinds[nCode].position;
		// Using a large deadzone when mapping microswitches to analog axis
		// Or said axis become way too sensitive and some game become unplayable (assault)
		if(s < -10000 && position == JOY_NEG)
			return 1;
		if(s > 10000 && position == JOY_POS)
			return 1;
	}
	return 0;
}

static inline int CinpJoyAxis(int port, int axis)
{
	int index = sAxiBinds[port][axis].index;
	if(index != -1)
		return input_cb(port, RETRO_DEVICE_ANALOG, index, sAxiBinds[port][axis].id);
	else
		return (input_cb(port, RETRO_DEVICE_JOYPAD, 0, sAxiBinds[port][axis].id_pos) - input_cb(port, RETRO_DEVICE_JOYPAD, 0, sAxiBinds[port][axis].id_neg)) * 32768;
	return 0;
}

static INT32 pointerValues[5][2];

static inline void CinpDirectCoord(int port, int axis)
{
	UINT16 val = (32767 + input_cb(port, nDeviceType[port], 0, sAxiBinds[port][axis].id));
	INT32 width, height;
	BurnDrvGetVisibleSize(&width, &height);
	pointerValues[port][axis] = (INT32)((axis == 0 ? width : height) * (double(val)/double(65536)));
	BurnGunSetCoords(port, pointerValues[port][0], pointerValues[port][1]);
}

static inline int CinpMouseAxis(int port, int axis)
{
	return input_cb(port, RETRO_DEVICE_MOUSE, 0, sAxiBinds[port][axis].id);
}

static INT32 InputTick()
{
	struct GameInp *pgi;
	UINT32 i;

	for (i = 0, pgi = GameInp; i < nGameInpCount; i++, pgi++) {
		INT32 nAdd = 0;
		if ((pgi->nInput &  GIT_GROUP_SLIDER) == 0) {				// not a slider
			continue;
		}

		if (pgi->nInput == GIT_KEYSLIDER) {
			// Get states of the two keys
			if (CinpState(pgi->Input.Slider.SliderAxis.nSlider[0]))	{
				nAdd -= 0x100;
			}
			if (CinpState(pgi->Input.Slider.SliderAxis.nSlider[1]))	{
				nAdd += 0x100;
			}
		}

		if (pgi->nInput == GIT_JOYSLIDER) {
			// Get state of the axis
			nAdd = CinpJoyAxis(pgi->Input.Slider.JoyAxis.nJoy, pgi->Input.Slider.JoyAxis.nAxis);
			nAdd /= 0x100;
		}

		// nAdd is now -0x100 to +0x100

		// Change to slider speed
		nAdd *= pgi->Input.Slider.nSliderSpeed;
		nAdd /= 0x100;

		if (pgi->Input.Slider.nSliderCenter) {						// Attact to center
			INT32 v = pgi->Input.Slider.nSliderValue - 0x8000;
			v *= (pgi->Input.Slider.nSliderCenter - 1);
			v /= pgi->Input.Slider.nSliderCenter;
			v += 0x8000;
			pgi->Input.Slider.nSliderValue = v;
		}

		pgi->Input.Slider.nSliderValue += nAdd;
		// Limit slider
		if (pgi->Input.Slider.nSliderValue < 0x0100) {
			pgi->Input.Slider.nSliderValue = 0x0100;
		}
		if (pgi->Input.Slider.nSliderValue > 0xFF00) {
			pgi->Input.Slider.nSliderValue = 0xFF00;
		}
	}
	return 0;
}

// Analog to analog mapping
static INT32 GameInpAnalog2RetroInpAnalog(struct GameInp* pgi, unsigned port, unsigned axis, unsigned id, int index, char *szn, UINT8 nInput = GIT_JOYAXIS_FULL, INT32 nSliderValue = 0x8000, INT16 nSliderSpeed = 0x0800, INT16 nSliderCenter = 10)
{
	if(bButtonMapped) return 0;
	switch (nInput)
	{
		case GIT_JOYAXIS_FULL:
		{
			pgi->nInput = GIT_JOYAXIS_FULL;
			pgi->Input.JoyAxis.nAxis = axis;
			pgi->Input.JoyAxis.nJoy = (UINT8)port;
			sAxiBinds[port][axis].index = index;
			sAxiBinds[port][axis].id = id;
			retro_input_descriptor descriptor;
			descriptor.port = port;
			descriptor.device = RETRO_DEVICE_ANALOG;
			descriptor.index = index;
			descriptor.id = id;
			descriptor.description = szn;
			normal_input_descriptors.push_back(descriptor);
			break;
		}
		case GIT_JOYSLIDER:
		{
			pgi->nInput = GIT_JOYSLIDER;
			pgi->Input.Slider.nSliderValue = nSliderValue;
			pgi->Input.Slider.nSliderSpeed = nSliderSpeed;
			pgi->Input.Slider.nSliderCenter = nSliderCenter;
			pgi->Input.Slider.JoyAxis.nAxis = axis;
			pgi->Input.Slider.JoyAxis.nJoy = (UINT8)port;
			sAxiBinds[port][axis].index = index;
			sAxiBinds[port][axis].id = id;
			retro_input_descriptor descriptor;
			descriptor.port = port;
			descriptor.device = RETRO_DEVICE_ANALOG;
			descriptor.index = index;
			descriptor.id = id;
			descriptor.description = szn;
			normal_input_descriptors.push_back(descriptor);
			break;
		}
		case GIT_MOUSEAXIS:
		{
			pgi->nInput = GIT_MOUSEAXIS;
			pgi->Input.MouseAxis.nAxis = axis;
			pgi->Input.MouseAxis.nMouse = (UINT8)port;
			sAxiBinds[port][axis].index = index;
			sAxiBinds[port][axis].id = id;
			retro_input_descriptor descriptor;
			descriptor.port = port;
			descriptor.device = RETRO_DEVICE_MOUSE;
			descriptor.index = index;
			descriptor.id = id;
			descriptor.description = szn;
			normal_input_descriptors.push_back(descriptor);
			break;
		}
		case GIT_DIRECT_COORD:
		{
			pgi->nInput = GIT_DIRECT_COORD;
			pgi->Input.MouseAxis.nAxis = axis;
			pgi->Input.MouseAxis.nMouse = (UINT8)port;
			sAxiBinds[port][axis].index = index;
			sAxiBinds[port][axis].id = id;
			retro_input_descriptor descriptor;
			descriptor.port = port;
			descriptor.device = nDeviceType[port];
			descriptor.index = index;
			descriptor.id = id;
			descriptor.description = szn;
			normal_input_descriptors.push_back(descriptor);
			break;
		}
	}
	bButtonMapped = true;
	return 0;
}

// Digital to digital mapping
static INT32 GameInpDigital2RetroInpKey(struct GameInp* pgi, unsigned port, unsigned id, char *szn, unsigned device = RETRO_DEVICE_JOYPAD)
{
	if(bButtonMapped) return 0;
	pgi->nInput = GIT_SWITCH;
	if (!bInputInitialized)
		pgi->Input.Switch.nCode = (UINT16)(nSwitchCode++);
	sKeyBinds[pgi->Input.Switch.nCode].id = id;
	sKeyBinds[pgi->Input.Switch.nCode].port = port;
	sKeyBinds[pgi->Input.Switch.nCode].device = device;
	sKeyBinds[pgi->Input.Switch.nCode].index = -1;
	retro_input_descriptor descriptor;
	descriptor.port = port;
	descriptor.device = device;
	descriptor.index = 0;
	descriptor.id = id;
	descriptor.description = szn;
	normal_input_descriptors.push_back(descriptor);
	bButtonMapped = true;
	return 0;
}

// 2 digital to 1 analog mapping
// Need to be run once for each of the 2 pgi (the 2 game inputs)
// nJoy (player) and nKey (axis) needs to be the same for each of the 2 buttons
// position is either JOY_POS or JOY_NEG (the position expected on axis to trigger the button)
// szn is the descriptor text
static INT32 GameInpDigital2RetroInpAnalogRight(struct GameInp* pgi, unsigned port, unsigned id, unsigned position, char *szn)
{
	if(bButtonMapped) return 0;
	pgi->nInput = GIT_SWITCH;
	if (!bInputInitialized)
		pgi->Input.Switch.nCode = (UINT16)(nSwitchCode++);
	sKeyBinds[pgi->Input.Switch.nCode].id = id;
	sKeyBinds[pgi->Input.Switch.nCode].port = port;
	sKeyBinds[pgi->Input.Switch.nCode].device = RETRO_DEVICE_ANALOG;
	sKeyBinds[pgi->Input.Switch.nCode].index = RETRO_DEVICE_INDEX_ANALOG_RIGHT;
	sKeyBinds[pgi->Input.Switch.nCode].position = position;
	bAnalogRightMappingDone[port][id][position] = true;
	if(bAnalogRightMappingDone[port][id][JOY_POS] && bAnalogRightMappingDone[port][id][JOY_NEG]) {
		retro_input_descriptor descriptor;
		descriptor.id = id;
		descriptor.port = port;
		descriptor.device = RETRO_DEVICE_ANALOG;
		descriptor.index = RETRO_DEVICE_INDEX_ANALOG_RIGHT;
		descriptor.description = szn;
		normal_input_descriptors.push_back(descriptor);
	}
	bButtonMapped = true;
	return 0;
}

// 1 analog to 2 digital mapping
// Needs pgi, player, axis, 2 buttons retropad id and 2 descriptions
static INT32 GameInpAnalog2RetroInpDualKeys(struct GameInp* pgi, unsigned port, unsigned axis, unsigned id_pos, unsigned id_neg, char *szn_pos, char *szn_neg)
{
	if(bButtonMapped) return 0;
	pgi->nInput = GIT_JOYAXIS_FULL;
	pgi->Input.JoyAxis.nAxis = axis;
	pgi->Input.JoyAxis.nJoy = (UINT8)port;
	sAxiBinds[port][axis].index = -1;
	sAxiBinds[port][axis].id_pos = id_pos;
	sAxiBinds[port][axis].id_neg = id_neg;

	retro_input_descriptor descriptor;

	descriptor.id = id_pos;
	descriptor.port = port;
	descriptor.device = RETRO_DEVICE_JOYPAD;
	descriptor.index = 0;
	descriptor.description = szn_pos;
	normal_input_descriptors.push_back(descriptor);

	descriptor.id = id_neg;
	descriptor.port = port;
	descriptor.device = RETRO_DEVICE_JOYPAD;
	descriptor.index = 0;
	descriptor.description = szn_neg;
	normal_input_descriptors.push_back(descriptor);

	bButtonMapped = true;
	return 0;
}

// [WIP]
// All inputs which needs special handling need to go in the next function
// TODO : move analog accelerators to R2
static INT32 GameInpSpecialOne(struct GameInp* pgi, INT32 nPlayer, char* szi, char *szn, char *description)
{
	const char * parentrom	= BurnDrvGetTextA(DRV_PARENT);
	const char * drvname	= BurnDrvGetTextA(DRV_NAME);
	const char * systemname = BurnDrvGetTextA(DRV_SYSTEM);

	if (nDeviceType[nPlayer] == RETRO_DEVICE_POINTER && BurnGunIsActive()) {
		if (strcmp("x-axis", szi + 3) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, 0, RETRO_DEVICE_ID_POINTER_X, 0, description, GIT_DIRECT_COORD);
		}
		if (strcmp("y-axis", szi + 3) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, 1, RETRO_DEVICE_ID_POINTER_Y, 0, description, GIT_DIRECT_COORD);
		}
		if (strcmp("mouse x-axis", szi) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, 0, RETRO_DEVICE_ID_POINTER_X, 0, description, GIT_DIRECT_COORD);
		}
		if (strcmp("mouse y-axis", szi) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, 1, RETRO_DEVICE_ID_POINTER_Y, 0, description, GIT_DIRECT_COORD);
		}
	}

	if (nDeviceType[nPlayer] == RETRO_DEVICE_LIGHTGUN && BurnGunIsActive()) {
		if (strcmp("x-axis", szi + 3) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X, 0, description, GIT_DIRECT_COORD);
		}
		if (strcmp("y-axis", szi + 3) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, 1, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y, 0, description, GIT_DIRECT_COORD);
		}
		if (strcmp("mouse x-axis", szi) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X, 0, description, GIT_DIRECT_COORD);
		}
		if (strcmp("mouse y-axis", szi) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, 1, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y, 0, description, GIT_DIRECT_COORD);
		}
		if (strcmp("fire 1", szi + 3) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_LIGHTGUN_TRIGGER, description, RETRO_DEVICE_LIGHTGUN);
		}
		if (strcmp("mouse button 1", szi) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_LIGHTGUN_TRIGGER, description, RETRO_DEVICE_LIGHTGUN);
		}
		if (strcmp("mouse button", szi) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_LIGHTGUN_TRIGGER, description, RETRO_DEVICE_LIGHTGUN);
		}
		if (strcmp("fire 2", szi + 3) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_LIGHTGUN_AUX_A, description, RETRO_DEVICE_LIGHTGUN);
		}
		if (strcmp("mouse button 2", szi) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_LIGHTGUN_AUX_A, description, RETRO_DEVICE_LIGHTGUN);
		}
		if (strcmp("fire 3", szi + 3) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_LIGHTGUN_AUX_B, description, RETRO_DEVICE_LIGHTGUN);
		}
		if (strcmp("fire 4", szi + 3) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_LIGHTGUN_AUX_C, description, RETRO_DEVICE_LIGHTGUN);
		}
		if (strcmp("up", szi + 3) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_LIGHTGUN_DPAD_UP, description, RETRO_DEVICE_LIGHTGUN);
		}
		if (strcmp("down", szi + 3) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_LIGHTGUN_DPAD_DOWN, description, RETRO_DEVICE_LIGHTGUN);
		}
		if (strcmp("left", szi + 3) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_LIGHTGUN_DPAD_LEFT, description, RETRO_DEVICE_LIGHTGUN);
		}
		if (strcmp("right", szi + 3) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_LIGHTGUN_DPAD_RIGHT, description, RETRO_DEVICE_LIGHTGUN);
		}
		if (strcmp("start", szi + 3) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_LIGHTGUN_START, description, RETRO_DEVICE_LIGHTGUN);
		}
		if (strcmp("coin", szi + 3) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_LIGHTGUN_SELECT, description, RETRO_DEVICE_LIGHTGUN);
		}
	}

	if (nDeviceType[nPlayer] == RETROMOUSE_BALL || nDeviceType[nPlayer] == RETROMOUSE_FULL) {
		if (strcmp("x-axis", szi + 3) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, 0, RETRO_DEVICE_ID_MOUSE_X, 0, description, GIT_MOUSEAXIS);
		}
		if (strcmp("y-axis", szi + 3) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, 1, RETRO_DEVICE_ID_MOUSE_Y, 0, description, GIT_MOUSEAXIS);
		}
		if (strcmp("mouse x-axis", szi) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, 0, RETRO_DEVICE_ID_MOUSE_X, 0, description, GIT_MOUSEAXIS);
		}
		if (strcmp("mouse y-axis", szi) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, 1, RETRO_DEVICE_ID_MOUSE_Y, 0, description, GIT_MOUSEAXIS);
		}
		if (nDeviceType[nPlayer] == RETROMOUSE_FULL) {
			// Handle mouse button mapping (i will keep it simple...)
			if (strcmp("fire 1", szi + 3) == 0) {
				GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_MOUSE_LEFT, description, RETRO_DEVICE_MOUSE);
			}
			if (strcmp("mouse button 1", szi) == 0) {
				GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_MOUSE_LEFT, description, RETRO_DEVICE_MOUSE);
			}
			if (strcmp("mouse button", szi) == 0) {
				GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_MOUSE_LEFT, description, RETRO_DEVICE_MOUSE);
			}
			if (strcmp("fire 2", szi + 3) == 0) {
				GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_MOUSE_RIGHT, description, RETRO_DEVICE_MOUSE);
			}
			if (strcmp("mouse button 2", szi) == 0) {
				GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_MOUSE_RIGHT, description, RETRO_DEVICE_MOUSE);
			}
			if (strcmp("fire 3", szi + 3) == 0) {
				GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_MOUSE_MIDDLE, description, RETRO_DEVICE_MOUSE);
			}
			if (strcmp("fire 4", szi + 3) == 0) {
				GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_MOUSE_BUTTON_4, description, RETRO_DEVICE_MOUSE);
			}
			if (strcmp("fire 5", szi + 3) == 0) {
				GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_MOUSE_BUTTON_5, description, RETRO_DEVICE_MOUSE);
			}
		}
	}

	// Fix part of issue #102 (Crazy Fight)
	// Can't really manage to have a decent mapping on this one if you don't have a stick/pad with the following 2 rows of 3 buttons :
	// Y X R1
	// B A R2
	if ((parentrom && strcmp(parentrom, "crazyfgt") == 0) ||
		(drvname && strcmp(drvname, "crazyfgt") == 0)
	) {
		if (strcmp("top-left", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_Y, description);
		}
		if (strcmp("top-center", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_X, description);
		}
		if (strcmp("top-right", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R, description);
		}
		if (strcmp("bottom-left", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_B, description);
		}
		if (strcmp("bottom-center", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_A, description);
		}
		if (strcmp("bottom-right", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R2, description);
		}
	}

	// Fix part of issue #102 (Bikkuri Card)
	// Coin 2 overwrited Coin 1, which is probably part of the issue
	// I managed to pass the payout tests once, but i don't know how
	if ((parentrom && strcmp(parentrom, "bikkuric") == 0) ||
		(drvname && strcmp(drvname, "bikkuric") == 0)
	) {
		if (strcmp("Coin 2", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_START, description);
		}
	}

	// Fix part of issue #102 (Jackal)
	if ((parentrom && strcmp(parentrom, "jackal") == 0) ||
		(drvname && strcmp(drvname, "jackal") == 0)
	) {
		if (strcmp("Rotate Left", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_L, description);
		}
		if (strcmp("Rotate Right", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R, description);
		}
	}

	// Fix part of issue #102 (Last Survivor)
	if ((parentrom && strcmp(parentrom, "lastsurv") == 0) ||
		(drvname && strcmp(drvname, "lastsurv") == 0)
	) {
		if (strcmp("Turn Left", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_L, description);
		}
		if (strcmp("Turn Right", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R, description);
		}
	}

	// Car steer a little too much with default setting + use L/R for Shift Down/Up (Super Monaco GP)
	if ((parentrom && strcmp(parentrom, "smgp") == 0) ||
		(drvname && strcmp(drvname, "smgp") == 0)
	) {
		if (strcmp("Shift Down", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_L, description);
		}
		if (strcmp("Shift Up", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R, description);
		}
	}

	// Fix issue #133 (Night striker)
	if ((parentrom && strcmp(parentrom, "nightstr") == 0) ||
		(drvname && strcmp(drvname, "nightstr") == 0)
	) {
		if (strcmp("Stick Y", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, 1, RETRO_DEVICE_ID_ANALOG_Y, RETRO_DEVICE_INDEX_ANALOG_LEFT, description, GIT_JOYSLIDER);
		}
	}
	
	// Fix part of issue #102 (Hang On Junior)
	if ((parentrom && strcmp(parentrom, "hangonjr") == 0) ||
		(drvname && strcmp(drvname, "hangonjr") == 0)
	) {
		if (strcmp("Accelerate", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_B, description);
		}
	}
	
	// Fix part of issue #102 (Out Run)
	if ((parentrom && strcmp(parentrom, "outrun") == 0) ||
		(drvname && strcmp(drvname, "outrun") == 0)
	) {
		if (strcmp("Gear", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R, description);
		}
	}
	
	// Fix part of issue #102 (Golden Axe)
	// Use same layout as megadrive cores
	if ((parentrom && strcmp(parentrom, "goldnaxe") == 0) ||
		(drvname && strcmp(drvname, "goldnaxe") == 0)
	) {
		if (strcmp("Fire 1", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_Y, "Magic");
		}
		if (strcmp("Fire 2", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_B, "Attack");
		}
		if (strcmp("Fire 3", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_A, "Jump");
		}
	}
	
	// Fix part of issue #102 (Major League)
	if ((parentrom && strcmp(parentrom, "mjleague") == 0) ||
		(drvname && strcmp(drvname, "mjleague") == 0)
	) {
		if (strcmp("Bat Swing", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_B, description);
		}
		if (strcmp("Fire 1", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_A, description);
		}
		if (strcmp("Fire 2", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_Y, description);
		}
		if (strcmp("Fire 3", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_X, description);
		}
		if (strcmp("Fire 4", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R, description);
		}
		if (strcmp("Fire 5", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_L, description);
		}
	}
	
	// Fix part of issue #102 (Chase HQ)
	if ((parentrom && strcmp(parentrom, "chasehq") == 0) ||
		(drvname && strcmp(drvname, "chasehq") == 0)
	) {
		if (strcmp("Brake", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_A, description);
		}
		if (strcmp("Accelerate", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_B, description);
		}
		if (strcmp("Turbo", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_Y, description);
		}
		if (strcmp("Gear", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R, description);
		}
	}
	
	// Fix part of issue #102 (SDI - Strategic Defense Initiative)
	if ((parentrom && strcmp(parentrom, "sdi") == 0) ||
		(drvname && strcmp(drvname, "sdi") == 0)
	) {
		if (strcmp("Target L/R", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, 0, RETRO_DEVICE_ID_ANALOG_X, RETRO_DEVICE_INDEX_ANALOG_RIGHT, description);
		}
		if (strcmp("Target U/D", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, 1, RETRO_DEVICE_ID_ANALOG_Y, RETRO_DEVICE_INDEX_ANALOG_RIGHT, description);
		}
		if (strcmp("Fire 1", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R, description);
		}
	}
	
	// Fix part of issue #102 (Blades of Steel)
	if ((parentrom && strcmp(parentrom, "bladestl") == 0) ||
		(drvname && strcmp(drvname, "bladestl") == 0)
	) {
		if (strcmp("Trackball X", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, 0, RETRO_DEVICE_ID_ANALOG_X, RETRO_DEVICE_INDEX_ANALOG_RIGHT, description);
		}
		if (strcmp("Trackball Y", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, 1, RETRO_DEVICE_ID_ANALOG_Y, RETRO_DEVICE_INDEX_ANALOG_RIGHT, description);
		}
		if (strcmp("Button 1", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R, description);
		}
		if (strcmp("Button 2", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R2, description);
		}
		if (strcmp("Button 3", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_L, description);
		}
	}
	
	// Fix part of issue #102 (Forgotten Worlds)
	if ((parentrom && strcmp(parentrom, "forgottn") == 0) ||
		(drvname && strcmp(drvname, "forgottn") == 0)
	) {
		if (strcmp("Turn (analog)", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, 0, RETRO_DEVICE_ID_ANALOG_X, RETRO_DEVICE_INDEX_ANALOG_RIGHT, description);
		}
		if (strcmp("Attack", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R, description);
		}
		if (strcmp("Turn - (digital)", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_B, description);
		}
		if (strcmp("Turn + (digital)", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_A, description);
		}
	}
	
	// Pop 'n Bounce / Gapporin
	if ((parentrom && strcmp(parentrom, "popbounc") == 0) ||
		(drvname && strcmp(drvname, "popbounc") == 0)
	) {
		if (strcmp("Paddle", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, 0, RETRO_DEVICE_ID_ANALOG_X, RETRO_DEVICE_INDEX_ANALOG_LEFT, description);
		}
		if (strcmp("Button D", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_X, description);
		}
	}
	
	// The Irritating Maze / Ultra Denryu Iraira Bou, Shoot the Bull
	if ((parentrom && strcmp(parentrom, "irrmaze") == 0) ||
		(drvname && strcmp(drvname, "irrmaze") == 0) ||
		(parentrom && strcmp(parentrom, "shootbul") == 0) ||
		(drvname && strcmp(drvname, "shootbul") == 0)
	) {
		if (strcmp("X Axis", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, 0, RETRO_DEVICE_ID_ANALOG_X, RETRO_DEVICE_INDEX_ANALOG_LEFT, description);
		}
		if (strcmp("Y Axis", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, 1, RETRO_DEVICE_ID_ANALOG_Y, RETRO_DEVICE_INDEX_ANALOG_LEFT, description);
		}
		if (strcmp("Button A", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_B, description);
		}
		if (strcmp("Button B", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_A, description);
		}
	}
	
	// Laser Ghost
	if ((parentrom && strcmp(parentrom, "lghost") == 0) ||
		(drvname && strcmp(drvname, "lghost") == 0) ||
		(parentrom && strcmp(parentrom, "loffire") == 0) ||
		(drvname && strcmp(drvname, "loffire") == 0)
	) {
		if (strcmp("X-Axis", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, 0, RETRO_DEVICE_ID_ANALOG_X, RETRO_DEVICE_INDEX_ANALOG_LEFT, description);
		}
		if (strcmp("Y-Axis", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, 1, RETRO_DEVICE_ID_ANALOG_Y, RETRO_DEVICE_INDEX_ANALOG_LEFT, description);
		}
		if (strcmp("Fire 1", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_B, description);
		}
		if (strcmp("Fire 2", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_A, description);
		}
	}
	
	// Lord of Gun, ...
	if ((parentrom && strcmp(parentrom, "lordgun") == 0) ||
		(drvname && strcmp(drvname, "lordgun") == 0) ||
		(parentrom && strcmp(parentrom, "deerhunt") == 0) ||
		(drvname && strcmp(drvname, "deerhunt") == 0) ||
		(parentrom && strcmp(parentrom, "turkhunt") == 0) ||
		(drvname && strcmp(drvname, "turkhunt") == 0) ||
		(parentrom && strcmp(parentrom, "wschamp") == 0) ||
		(drvname && strcmp(drvname, "wschamp") == 0) ||
		(parentrom && strcmp(parentrom, "trophyh") == 0) ||
		(drvname && strcmp(drvname, "trophyh") == 0) ||
		(parentrom && strcmp(parentrom, "zombraid") == 0) ||
		(drvname && strcmp(drvname, "zombraid") == 0)
	) {
		if (strcmp("Right / left", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, 0, RETRO_DEVICE_ID_ANALOG_X, RETRO_DEVICE_INDEX_ANALOG_LEFT, description);
		}
		if (strcmp("Up / Down", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, 1, RETRO_DEVICE_ID_ANALOG_Y, RETRO_DEVICE_INDEX_ANALOG_LEFT, description);
		}
		if (strcmp("Button 1", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_B, description);
		}
		if (strcmp("Button 2", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_A, description);
		}
	}
	
	// Lethal Enforcers, Beast Busters, Bang!, Zero Point 1 & 2, Operation Thunderbolt, Operation Wolf 1 & 3, Space Gun
	if ((parentrom && strcmp(parentrom, "lethalen") == 0) ||
		(drvname && strcmp(drvname, "lethalen") == 0) ||
		(parentrom && strcmp(parentrom, "bbusters") == 0) ||
		(drvname && strcmp(drvname, "bbusters") == 0) ||
		(parentrom && strcmp(parentrom, "bang") == 0) ||
		(drvname && strcmp(drvname, "bang") == 0) ||
		(parentrom && strcmp(parentrom, "zeropnt") == 0) ||
		(drvname && strcmp(drvname, "zeropnt") == 0) ||
		(parentrom && strcmp(parentrom, "zeropnt2") == 0) ||
		(drvname && strcmp(drvname, "zeropnt2") == 0) ||
		(parentrom && strcmp(parentrom, "othunder") == 0) ||
		(drvname && strcmp(drvname, "othunder") == 0) ||
		(parentrom && strcmp(parentrom, "opwolf3") == 0) ||
		(drvname && strcmp(drvname, "opwolf3") == 0) ||
		(parentrom && strcmp(parentrom, "opwolf") == 0) ||
		(drvname && strcmp(drvname, "opwolf") == 0) ||
		(parentrom && strcmp(parentrom, "spacegun") == 0) ||
		(drvname && strcmp(drvname, "spacegun") == 0) ||
		(parentrom && strcmp(parentrom, "mechatt") == 0) ||
		(drvname && strcmp(drvname, "mechatt") == 0) ||
		(parentrom && strcmp(parentrom, "sgunner") == 0) ||
		(drvname && strcmp(drvname, "sgunner") == 0) ||
		(parentrom && strcmp(parentrom, "sgunner2") == 0) ||
		(drvname && strcmp(drvname, "sgunner2") == 0)
	) {
		if (strcmp("Gun X", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, 0, RETRO_DEVICE_ID_ANALOG_X, RETRO_DEVICE_INDEX_ANALOG_LEFT, description);
		}
		if (strcmp("Gun Y", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, 1, RETRO_DEVICE_ID_ANALOG_Y, RETRO_DEVICE_INDEX_ANALOG_LEFT, description);
		}
		if (strcmp("Button 1", description) == 0 || strcmp("Fire 1", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_B, description);
		}
		if (strcmp("Button 2", description) == 0 || strcmp("Fire 2", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_A, description);
		}
		if (strcmp("Button 3", description) == 0 || strcmp("Fire 3", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_Y, description);
		}
	}

	// Rail Chase
	if ((parentrom && strcmp(parentrom, "rchase") == 0) ||
		(drvname && strcmp(drvname, "rchase") == 0)
	) {
		if (strcmp("Left/Right", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, 0, RETRO_DEVICE_ID_ANALOG_X, RETRO_DEVICE_INDEX_ANALOG_LEFT, description);
		}
		if (strcmp("Up/Down", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, 1, RETRO_DEVICE_ID_ANALOG_Y, RETRO_DEVICE_INDEX_ANALOG_LEFT, description);
		}
		if (strcmp("Fire 1", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_B, description);
		}
	}
	
	// Fix part of issue #102 (VS Block Breaker)
	if ((parentrom && strcmp(parentrom, "vblokbrk") == 0) ||
		(drvname && strcmp(drvname, "vblokbrk") == 0)
	) {
		if (strcmp("Paddle", description) == 0) {
			GameInpAnalog2RetroInpDualKeys(pgi, nPlayer, 0, RETRO_DEVICE_ID_JOYPAD_R, RETRO_DEVICE_ID_JOYPAD_L, "Paddle Up", "Paddle Down");
		}
	}
	
	// Fix part of issue #102 (Puzz Loop 2)
	if ((parentrom && strcmp(parentrom, "pzloop2") == 0) ||
		(drvname && strcmp(drvname, "pzloop2") == 0)
	) {
		if (strcmp("Paddle", description) == 0) {
			GameInpAnalog2RetroInpDualKeys(pgi, nPlayer, 0, RETRO_DEVICE_ID_JOYPAD_R, RETRO_DEVICE_ID_JOYPAD_L, "Paddle Up", "Paddle Down");
		}
	}
	
	// Fix part of issue #102 (After burner 1 & 2)
	if ((parentrom && strcmp(parentrom, "aburner2") == 0) ||
		(drvname && strcmp(drvname, "aburner2") == 0)
	) {
		if (strcmp("Throttle", description) == 0) {
			GameInpAnalog2RetroInpDualKeys(pgi, nPlayer, 2, RETRO_DEVICE_ID_JOYPAD_R, RETRO_DEVICE_ID_JOYPAD_L, "Speed Up", "Speed Down");
		}
	}
	
	// Fix part of issue #156 (Alien vs Predator)
	// Use a layout more similar to the cabinet, with jump in the middle
	if ((parentrom && strcmp(parentrom, "avsp") == 0) ||
		(drvname && strcmp(drvname, "avsp") == 0)
	) {
		if (strcmp("Attack", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_Y, description);
		}
		if (strcmp("Jump", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_B, description);
		}
		if (strcmp("Shot", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_A, description);
		}
	}
	
	// Dragon Gun
	if ((parentrom && strcmp(parentrom, "dragngun") == 0) ||
		(drvname && strcmp(drvname, "dragngun") == 0)
	) {
		if (strcmp("Gun X", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, 0, RETRO_DEVICE_ID_ANALOG_X, RETRO_DEVICE_INDEX_ANALOG_LEFT, description);
		}
		if (strcmp("Gun Y", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, 1, RETRO_DEVICE_ID_ANALOG_Y, RETRO_DEVICE_INDEX_ANALOG_LEFT, description);
		}
	}
	
	// Beraboh Man
	if ((parentrom && strcmp(parentrom, "berabohm") == 0) ||
		(drvname && strcmp(drvname, "berabohm") == 0)
	) {
		if (strcmp("Button 1", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_B, description);
		}
		if (strcmp("Button 2", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_A, description);
		}
		if (strcmp("Button 3", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, (nDeviceType[nPlayer] == RETROPAD_MODERN ? RETRO_DEVICE_ID_JOYPAD_R2 : RETRO_DEVICE_ID_JOYPAD_R), description);
		}
		if (strcmp("Button 4", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_Y, description);
		}
		if (strcmp("Button 5", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_X, description);
		}
		if (strcmp("Button 6", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, (nDeviceType[nPlayer] == RETROPAD_MODERN ? RETRO_DEVICE_ID_JOYPAD_R : RETRO_DEVICE_ID_JOYPAD_L), description);
		}
	}
	
	// Mad Planets
	if ((parentrom && strcmp(parentrom, "mplanets") == 0) ||
		(drvname && strcmp(drvname, "mplanets") == 0)
	) {
		if (strcmp("Rotate Left", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_L, description);
		}
		if (strcmp("Rotate Right", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R, description);
		}
	}
	
	// Street Fighter Zero (CPS Changer) #245
	if ((parentrom && strcmp(parentrom, "sfzch") == 0) ||
		(drvname && strcmp(drvname, "sfzch") == 0)
	) {
		if (strcmp("Pause", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_SELECT, description);
		}
	}
	
	// Toobin' (reported on discord, layout suggestion by alfrix#8029)
	if ((parentrom && strcmp(parentrom, "toobin") == 0) ||
		(drvname && strcmp(drvname, "toobin") == 0)
	) {
		if (strcmp("Throw", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_B, description);
		}
		if (strcmp("Right.Paddle Forward", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R, description);
		}
		if (strcmp("Left Paddle Forward", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_L, description);
		}
		if (strcmp("Left Paddle Backward", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_L2, description);
		}
		if (strcmp("Right Paddle Backward", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R2, description);
		}
	}
	
	// Vindicators (reported on discord, layout suggestion by alfrix#8029)
	if ((parentrom && strcmp(parentrom, "vindictr") == 0) ||
		(drvname && strcmp(drvname, "vindictr") == 0)
	) {
		if (strcmp("Left Stick Up", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_L2, description);
		}
		if (strcmp("Left Stick Down", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_L, description);
		}
		if (strcmp("Right Stick Up", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R2, description);
		}
		if (strcmp("Right Stick Down", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R, description);
		}
		if (strcmp("-- Alt. Input --", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_L3, description);
		}
	}
	
	// Vindicators Part II (reported on discord, layout suggestion by alfrix#8029)
	if ((parentrom && strcmp(parentrom, "vindctr2") == 0) ||
		(drvname && strcmp(drvname, "vindctr2") == 0)
	) {
		if (strcmp("Left Stick Up", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_L2, description);
		}
		if (strcmp("Left Stick Down", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_L, description);
		}
		if (strcmp("Right Stick Up", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R2, description);
		}
		if (strcmp("Right Stick Down", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R, description);
		}
		if (strcmp("Right Stick Fire", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_A, description);
		}
		if (strcmp("Left Stick Thumb", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_Y, description);
		}
	}
	
	// Thunder Ceptor & Thunder Ceptor II (#33)
	if ((parentrom && strcmp(parentrom, "tceptor") == 0) ||
		(drvname && strcmp(drvname, "tceptor") == 0)
	) {
		if (strcmp("X Axis", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, 0, RETRO_DEVICE_ID_ANALOG_X, RETRO_DEVICE_INDEX_ANALOG_LEFT, description);
		}
		if (strcmp("Y Axis", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, 1, RETRO_DEVICE_ID_ANALOG_Y, RETRO_DEVICE_INDEX_ANALOG_LEFT, description);
		}
		if (strcmp("Accelerator", description) == 0) {
			// It would be nice to have this control working as analog on R2
			//GameInpAnalog2RetroInpAnalog(pgi, nPlayer, 2, RETRO_DEVICE_ID_JOYPAD_R2, RETRO_DEVICE_INDEX_ANALOG_BUTTON, description);
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R2, description);
		}
	}
	
	// Hydra (reported on discord, layout suggestion by alfrix#8029)
	if ((parentrom && strcmp(parentrom, "hydra") == 0) ||
		(drvname && strcmp(drvname, "hydra") == 0)
	) {
		if (strcmp("Accelerator", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R2, description);
		}
		if (strcmp("Boost", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_L2, description);
		}
		if (strcmp("Right Trigger", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_A, description);
		}
		if (strcmp("Left Trigger", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_B, description);
		}
		if (strcmp("Right Thumb", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_X, description);
		}
		if (strcmp("Left Thumb", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_Y, description);
		}
	}

	// Assault
	if ((parentrom && strcmp(parentrom, "assault") == 0) ||
		(drvname && strcmp(drvname, "assault") == 0)
	) {
		if (strcmp("Right Stick Up", description) == 0) {
			GameInpDigital2RetroInpAnalogRight(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_Y, JOY_NEG, "Right Stick Up / Down");
		}
		if (strcmp("Right Stick Down", description) == 0) {
			GameInpDigital2RetroInpAnalogRight(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_Y, JOY_POS, "Right Stick Up / Down");
		}
		if (strcmp("Right Stick Left", description) == 0) {
			GameInpDigital2RetroInpAnalogRight(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_X, JOY_NEG, "Right Stick Left / Right");
		}
		if (strcmp("Right Stick Right", description) == 0) {
			GameInpDigital2RetroInpAnalogRight(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_X, JOY_POS, "Right Stick Left / Right");
		}
	}

	// Angel Kids, Splat!
	if ((parentrom && strcmp(parentrom, "angelkds") == 0) ||
		(drvname && strcmp(drvname, "angelkds") == 0) ||
		(parentrom && strcmp(parentrom, "splat") == 0) ||
		(drvname && strcmp(drvname, "splat") == 0)
	) {
		if (strcmp("Left Stick Up", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_UP, description);
		}
		if (strcmp("Left Stick Down", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_DOWN, description);
		}
		if (strcmp("Left Stick Left", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_LEFT, description);
		}
		if (strcmp("Left Stick Right", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_RIGHT, description);
		}
		if (strcmp("Right Stick Up", description) == 0) {
			GameInpDigital2RetroInpAnalogRight(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_Y, JOY_NEG, "Right Stick Up / Down");
		}
		if (strcmp("Right Stick Down", description) == 0) {
			GameInpDigital2RetroInpAnalogRight(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_Y, JOY_POS, "Right Stick Up / Down");
		}
		if (strcmp("Right Stick Left", description) == 0) {
			GameInpDigital2RetroInpAnalogRight(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_X, JOY_NEG, "Right Stick Left / Right");
		}
		if (strcmp("Right Stick Right", description) == 0) {
			GameInpDigital2RetroInpAnalogRight(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_X, JOY_POS, "Right Stick Left / Right");
		}
	}

	// Rock Climber
	if ((parentrom && strcmp(parentrom, "rockclim") == 0) ||
		(drvname && strcmp(drvname, "rockclim") == 0)
	) {
		if (strcmp("Rght Stick Up", description) == 0) {
			GameInpDigital2RetroInpAnalogRight(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_Y, JOY_NEG, "Right Stick Up / Down");
		}
		if (strcmp("Rght Stick Down", description) == 0) {
			GameInpDigital2RetroInpAnalogRight(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_Y, JOY_POS, "Right Stick Up / Down");
		}
		if (strcmp("Rght Stick Left", description) == 0) {
			GameInpDigital2RetroInpAnalogRight(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_X, JOY_NEG, "Right Stick Left / Right");
		}
		if (strcmp("Rght Stick Right", description) == 0) {
			GameInpDigital2RetroInpAnalogRight(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_X, JOY_POS, "Right Stick Left / Right");
		}
	}

	// Black Widow, Robotron
	if ((parentrom && strcmp(parentrom, "bwidow") == 0) ||
		(drvname && strcmp(drvname, "bwidow") == 0) ||
		(parentrom && strcmp(parentrom, "robotron") == 0) ||
		(drvname && strcmp(drvname, "robotron") == 0)
	) {
		if (strcmp("Fire Up", description) == 0) {
			GameInpDigital2RetroInpAnalogRight(pgi, 0, RETRO_DEVICE_ID_ANALOG_Y, JOY_NEG, "Fire Up / Down");
		}
		if (strcmp("Fire Down", description) == 0) {
			GameInpDigital2RetroInpAnalogRight(pgi, 0, RETRO_DEVICE_ID_ANALOG_Y, JOY_POS, "Fire Up / Down");
		}
		if (strcmp("Fire Left", description) == 0) {
			GameInpDigital2RetroInpAnalogRight(pgi, 0, RETRO_DEVICE_ID_ANALOG_X, JOY_NEG, "Fire Left / Right");
		}
		if (strcmp("Fire Right", description) == 0) {
			GameInpDigital2RetroInpAnalogRight(pgi, 0, RETRO_DEVICE_ID_ANALOG_X, JOY_POS, "Fire Left / Right");
		}
	}
	
	// Battle Zone
	if ((parentrom && strcmp(parentrom, "bzone") == 0) ||
		(drvname && strcmp(drvname, "bzone") == 0)
	) {
		if (strcmp("Fire", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, 0, RETRO_DEVICE_ID_JOYPAD_B, description);
		}
		if (strcmp("Right Stick Down", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, 0, RETRO_DEVICE_ID_JOYPAD_R, description);
		}
		if (strcmp("Left Stick Down", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, 0, RETRO_DEVICE_ID_JOYPAD_L, description);
		}
		if (strcmp("Left Stick Up", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, 0, RETRO_DEVICE_ID_JOYPAD_L2, description);
		}
		if (strcmp("Right Stick Up", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, 0, RETRO_DEVICE_ID_JOYPAD_R2, description);
		}
	}
	
	// Ms. Pac-Man/Galaga - 20th Anniversary Class of 1981 Reunion
	// Pac-Man - 25th Anniversary Edition
	if ((parentrom && strcmp(parentrom, "20pacgal") == 0) ||
		(drvname && strcmp(drvname, "20pacgal") == 0) ||
		(parentrom && strcmp(parentrom, "25pacman") == 0) ||
		(drvname && strcmp(drvname, "25pacman") == 0)
	) {
		if (strcmp("Start (left)", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_START, description);
		}
		if (strcmp("Start (right)", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_A, description);
		}
	}
	
	// Discs of Tron
	if ((parentrom && strcmp(parentrom, "dotron") == 0) ||
		(drvname && strcmp(drvname, "dotron") == 0)
	) {
		if (strcmp("Dial", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, 0, RETRO_DEVICE_ID_ANALOG_X, RETRO_DEVICE_INDEX_ANALOG_RIGHT, description);
		}
		if (strcmp("Aim Analog", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, 1, RETRO_DEVICE_ID_ANALOG_Y, RETRO_DEVICE_INDEX_ANALOG_RIGHT, description);
		}
	}
	
	// Handle megadrive
	if ((systemname && strcmp(systemname, "Sega Megadrive") == 0)) {
		// Street Fighter 2 mapping (which is the only 6 button megadrive game ?)
		// Use same layout as arcade
		if ((parentrom && strcmp(parentrom, "md_sf2") == 0) ||
			(drvname && strcmp(drvname, "md_sf2") == 0)
		) {
			if (strcmp("Button A", description) == 0) {
				GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_B, "Weak Kick");
			}
			if (strcmp("Button B", description) == 0) {
				GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_A, "Medium Kick");
			}
			if (strcmp("Button C", description) == 0) {
				GameInpDigital2RetroInpKey(pgi, nPlayer, (nDeviceType[nPlayer] == RETROPAD_MODERN ? RETRO_DEVICE_ID_JOYPAD_R2 : RETRO_DEVICE_ID_JOYPAD_R), "Strong Kick");
			}
			if (strcmp("Button X", description) == 0) {
				GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_Y, "Weak Punch");
			}
			if (strcmp("Button Y", description) == 0) {
				GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_X, "Medium Punch");
			}
			if (strcmp("Button Z", description) == 0) {
				GameInpDigital2RetroInpKey(pgi, nPlayer, (nDeviceType[nPlayer] == RETROPAD_MODERN ? RETRO_DEVICE_ID_JOYPAD_R : RETRO_DEVICE_ID_JOYPAD_L), "Strong Punch");
			}
		}
		// Generic megadrive mapping
		// Use same layout as megadrive cores
		if (strcmp("Button A", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_Y, description);
		}
		if (strcmp("Button B", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_B, description);
		}
		if (strcmp("Button C", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_A, description);
		}
	}
	
	// Handle MSX
	if ((systemname && strcmp(systemname, "MSX") == 0)) {
		if (strcmp("Button 1", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_B, description);
		}
		if (strcmp("Button 2", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_A, description);
		}
		if (strcmp("Key F1", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_START, description);
		}
		if (strcmp("Key F2", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_SELECT, description);
		}
		if (strcmp("Key F3", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_Y, description);
		}
		if (strcmp("Key F4", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_X, description);
		}
		if (strcmp("Key F5", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R, description);
		}
		if (strcmp("Key F6", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_L, description);
		}
		if (strcmp("Key UP", description) == 0) {
			GameInpDigital2RetroInpAnalogRight(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_Y, JOY_NEG, "Key UP / Key DOWN");
		}
		if (strcmp("Key DOWN", description) == 0) {
			GameInpDigital2RetroInpAnalogRight(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_Y, JOY_POS, "Key UP / Key DOWN");
		}
		if (strcmp("Key LEFT", description) == 0) {
			GameInpDigital2RetroInpAnalogRight(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_X, JOY_NEG, "Key LEFT / Key RIGHT");
		}
		if (strcmp("Key RIGHT", description) == 0) {
			GameInpDigital2RetroInpAnalogRight(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_X, JOY_POS, "Key LEFT / Key RIGHT");
		}
	}
	
	// Handle Coleco
	if ((systemname && strcmp(systemname, "ColecoVision") == 0)) {
		if (strcmp("1", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_Y, description);
		}
		if (strcmp("2", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_X, description);
		}
		if (strcmp("3", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_L, description);
		}
		if (strcmp("4", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R, description);
		}
		if (strcmp("5", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_L2, description);
		}
		if (strcmp("6", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R2, description);
		}
		if (strcmp("7", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_L3, description);
		}
		if (strcmp("8", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R3, description);
		}
		if (strcmp("9", description) == 0) {
			GameInpDigital2RetroInpAnalogRight(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_X, JOY_POS, "0 / 9");
		}
		if (strcmp("0", description) == 0) {
			GameInpDigital2RetroInpAnalogRight(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_X, JOY_NEG, "0 / 9");
		}
		if (strcmp("*", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_SELECT, description);
		}
		if (strcmp("#", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_START, description);
		}
	}

	// Fix part of issue #102 (Twin stick games)
	if ((strcmp("Up 2", description) == 0) ||
		(strcmp("Up (right)", description) == 0) ||
		(strcmp("Right Up", description) == 0)
	) {
		GameInpDigital2RetroInpAnalogRight(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_Y, JOY_NEG, "Up / Down (Right Stick)");
	}
	if ((strcmp("Down 2", description) == 0) ||
		(strcmp("Down (right)", description) == 0) ||
		(strcmp("Right Down", description) == 0)
	) {
		GameInpDigital2RetroInpAnalogRight(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_Y, JOY_POS, "Up / Down (Right Stick)");
	}
	if ((strcmp("Left 2", description) == 0) ||
		(strcmp("Left (right)", description) == 0) ||
		(strcmp("Right Left", description) == 0)
	) {
		GameInpDigital2RetroInpAnalogRight(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_X, JOY_NEG, "Left / Right (Right Stick)");
	}
	if ((strcmp("Right 2", description) == 0) ||
		(strcmp("Right (right)", description) == 0) ||
		(strcmp("Right Right", description) == 0)
	) {
		GameInpDigital2RetroInpAnalogRight(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_X, JOY_POS, "Left / Right (Right Stick)");
	}
	
	// Default racing games's Steering control to the joyslider type of analog control
	// Joyslider is some sort of "wheel" emulation
	if ((BurnDrvGetGenreFlags() & GBF_RACING)) {
		if (strcmp("x-axis", szi + 3) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, 0, RETRO_DEVICE_ID_ANALOG_X, RETRO_DEVICE_INDEX_ANALOG_LEFT, description, GIT_JOYSLIDER);
		}
		if (strcmp("mouse x-axis", szi) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, 0, RETRO_DEVICE_ID_ANALOG_X, RETRO_DEVICE_INDEX_ANALOG_LEFT, description, GIT_JOYSLIDER);
		}
	}

	return 0;
}

// Handle mapping of an input
// Will delegate to GameInpSpecialOne for cases which needs "fine tuning"
// Use GameInp2RetroInp for the actual mapping
INT32 GameInpAutoOne(struct GameInp* pgi, char* szi, char *szn)
{
	bool bPlayerInInfo = (toupper(szi[0]) == 'P' && szi[1] >= '1' && szi[1] <= '5'); // Because some of the older drivers don't use the standard input naming.
	bool bPlayerInName = (szn[0] == 'P' && szn[1] >= '1' && szn[1] <= '5');

	bButtonMapped = false;

	if (bPlayerInInfo || bPlayerInName) {
		INT32 nPlayer = -1;

		if (bPlayerInName)
			nPlayer = szn[1] - '1';
		if (bPlayerInInfo && nPlayer == -1)
			nPlayer = szi[1] - '1';

		// We don't know the device for the player yet, going further is absurd
		//if (nDeviceType[nPlayer-1] == -1) return 0;

		char* szb = szi + 3;

		// "P1 XXX" - try to exclude the "P1 " from the szName
		INT32 offset_player_x = 0;
		if (strlen(szn) > 3 && szn[0] == 'P' && szn[2] == ' ')
			offset_player_x = 3;
		char* description = szn + offset_player_x;

		GameInpSpecialOne(pgi, nPlayer, szi, szn, description);
		if(bButtonMapped) return 0;

		if (strncmp("select", szb, 6) == 0)
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_SELECT, description);
		if (strncmp("coin", szb, 4) == 0)
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_SELECT, description);
		if (strncmp("start", szb, 5) == 0)
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_START, description);
		if (strncmp("up", szb, 2) == 0)
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_UP, description);
		if (strncmp("down", szb, 4) == 0)
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_DOWN, description);
		if (strncmp("left", szb, 4) == 0)
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_LEFT, description);
		if (strncmp("right", szb, 5) == 0)
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_RIGHT, description);
		if (strncmp("x-axis", szb, 6) == 0)
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, 0, RETRO_DEVICE_ID_ANALOG_X, RETRO_DEVICE_INDEX_ANALOG_LEFT, description);
		if (strncmp("y-axis", szb, 6) == 0)
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, 1, RETRO_DEVICE_ID_ANALOG_Y, RETRO_DEVICE_INDEX_ANALOG_LEFT, description);
		// Some gun games have their default mapping set to mouse upstream, let's handle them
		if (strcmp("mouse x-axis", szi) == 0)
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, 0, RETRO_DEVICE_ID_ANALOG_X, RETRO_DEVICE_INDEX_ANALOG_LEFT, description);
		if (strcmp("mouse y-axis", szi) == 0)
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, 1, RETRO_DEVICE_ID_ANALOG_Y, RETRO_DEVICE_INDEX_ANALOG_LEFT, description);
		if (strcmp("mouse button 1", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_B, description);
		if (strcmp("mouse button 2", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_A, description);

		if (strncmp("fire ", szb, 5) == 0) {
			char *szf = szb + 5;
			INT32 nButton = strtol(szf, NULL, 0);
			// The "Modern" mapping on neogeo (which mimic mapping from remakes and further opus of the series)
			// doesn't make sense and is kinda harmful on games other than kof, fatal fury and samourai showdown
			// So we restrain it to those 3 series.
			if ((BurnDrvGetGenreFlags() & GBF_VSFIGHT) && is_neogeo_game && nDeviceType[nPlayer] == RETROPAD_MODERN) {
				switch (nButton) {
					case 1:
						GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_Y, description);
						break;
					case 2:
						GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_B, description);
						break;
					case 3:
						GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_X, description);
						break;
					case 4:
						GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_A, description);
						break;
				}
			} else {
				// Handle 6 buttons fighting games with 3xPunchs and 3xKicks
				if (bStreetFighterLayout) {
					switch (nButton) {
						case 1:
							GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_Y, description);
							break;
						case 2:
							GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_X, description);
							break;
						case 3:
							GameInpDigital2RetroInpKey(pgi, nPlayer, (nDeviceType[nPlayer] == RETROPAD_MODERN ? RETRO_DEVICE_ID_JOYPAD_R : RETRO_DEVICE_ID_JOYPAD_L), description);
							break;
						case 4:
							GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_B, description);
							break;
						case 5:
							GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_A, description);
							break;
						case 6:
							GameInpDigital2RetroInpKey(pgi, nPlayer, (nDeviceType[nPlayer] == RETROPAD_MODERN ? RETRO_DEVICE_ID_JOYPAD_R2 : RETRO_DEVICE_ID_JOYPAD_R), description);
							break;
					}
				// Handle generic mapping of everything else
				} else {
					switch (nButton) {
						case 1:
							GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_B, description);
							break;
						case 2:
							GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_A, description);
							break;
						case 3:
							GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_Y, description);
							break;
						case 4:
							GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_X, description);
							break;
						case 5:
							GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R, description);
							break;
						case 6:
							GameInpDigital2RetroInpKey(pgi, nPlayer, (nDeviceType[nPlayer] == RETROPAD_MODERN ? RETRO_DEVICE_ID_JOYPAD_R2 : RETRO_DEVICE_ID_JOYPAD_L), description);
							break;
						case 7:
							GameInpDigital2RetroInpKey(pgi, nPlayer, (nDeviceType[nPlayer] == RETROPAD_MODERN ? RETRO_DEVICE_ID_JOYPAD_L : RETRO_DEVICE_ID_JOYPAD_R2), description);
							break;
						case 8:
							GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_L2, description);
							break;
					}
				}
			}
		}
	}

	const char * systemname = BurnDrvGetTextA(DRV_SYSTEM);

	// Handle Spectrum
	if ((systemname && strcmp(systemname, "ZX Spectrum") == 0)) {
		if (strcmp("keyb_A", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_a, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_B", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_b, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_C", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_c, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_D", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_d, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_E", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_e, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_F", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_f, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_G", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_g, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_H", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_h, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_I", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_i, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_J", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_j, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_K", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_k, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_L", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_l, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_M", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_m, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_N", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_n, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_O", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_o, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_P", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_p, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_Q", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_q, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_R", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_r, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_S", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_s, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_T", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_t, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_U", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_u, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_V", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_v, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_W", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_w, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_X", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_x, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_Y", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_y, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_Z", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_z, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_0", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_0, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_1", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_1, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_2", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_2, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_3", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_3, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_4", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_4, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_5", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_5, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_6", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_6, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_7", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_7, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_8", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_8, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_9", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_9, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_enter", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_RETURN, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_space", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_SPACE, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_left_shift", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_LSHIFT, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_right_shift", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_RSHIFT, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_insert", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_INSERT, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_caps_lock", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_CAPSLOCK, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_home", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_HOME, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_end", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_END, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_backspace", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_BACKSPACE, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_left_alt", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_LALT, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_left", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_LEFT, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_right", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_RIGHT, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_up", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_UP, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_down", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_DOWN, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_pause", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_PAUSE, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_left_ctrl", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_LCTRL, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_apost", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_QUOTEDBL, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_colon", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_COLON, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_stop", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_PERIOD, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_comma", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 0, RETROK_COMMA, szn, RETRO_DEVICE_KEYBOARD);
	}

	// Store the pgi that controls the reset input
	if (strcmp(szi, "reset") == 0) {
		pgi->nInput = GIT_SWITCH;
		if (!bInputInitialized)
			pgi->Input.Switch.nCode = (UINT16)(nSwitchCode++);
		pgi_reset = pgi;
	}

	// Store the pgi that controls the diagnostic input
	if (strcmp(szi, "diag") == 0) {
		pgi->nInput = GIT_SWITCH;
		if (!bInputInitialized)
			pgi->Input.Switch.nCode = (UINT16)(nSwitchCode++);
		pgi_diag = pgi;
	}
	return 0;
}

// Call this one when device type is changed
static INT32 GameInpReassign()
{
	struct GameInp* pgi;
	struct BurnInputInfo bii;
	UINT32 i;

	normal_input_descriptors.clear();

	for (i = 0, pgi = GameInp; i < nGameInpCount; i++, pgi++) {
		BurnDrvGetInputInfo(&bii, i);
		GameInpAutoOne(pgi, bii.szInfo, bii.szName);
	}

	return 0;
}

// Auto-configure any undefined inputs to defaults
INT32 GameInpDefault()
{
	struct GameInp* pgi;
	struct BurnInputInfo bii;
	UINT32 i;

	pgi_reset = NULL;
	pgi_diag = NULL;

	// Fill all inputs still undefined
	for (i = 0, pgi = GameInp; i < nGameInpCount; i++, pgi++) {
		if (pgi->nInput) {											// Already defined - leave it alone
			continue;
		}

		// Get the extra info about the input
		bii.szInfo = NULL;
		BurnDrvGetInputInfo(&bii, i);
		if (bii.pVal == NULL) {
			continue;
		}
		if (bii.szInfo == NULL) {
			bii.szInfo = "";
		}

		// Dip switches - set to constant
		if (bii.nType & BIT_GROUP_CONSTANT) {
			pgi->nInput = GIT_CONSTANT;
			continue;
		}

		GameInpAutoOne(pgi, bii.szInfo, bii.szName);
	}

	return 0;
}

static bool PollDiagInput()
{
	if (pgi_diag && diag_input)
	{
		bOneDiagInputPressed = false;
		bAllDiagInputPressed = true;

		for (int combo_idx = 0; diag_input[combo_idx] != RETRO_DEVICE_ID_JOYPAD_EMPTY; combo_idx++)
		{
			if (input_cb(0, RETRO_DEVICE_JOYPAD, 0, diag_input[combo_idx]) == false)
				bAllDiagInputPressed = false;
			else
				bOneDiagInputPressed = true;
		}

		if (bDiagComboActivated == false && bAllDiagInputPressed)
		{
			if (nDiagInputComboStartFrame == 0) // => User starts holding all the combo inputs
				nDiagInputComboStartFrame = nCurrentFrame;
			else if ((nCurrentFrame - nDiagInputComboStartFrame) > nDiagInputHoldFrameDelay) // Delays of the hold reached
				bDiagComboActivated = true;
		}
		else if (bOneDiagInputPressed == false)
		{
			bDiagComboActivated = false;
			nDiagInputComboStartFrame = 0;
		}

		if (bDiagComboActivated)
		{
			// Cancel each input of the combo at the emulator side to not interfere when the diagnostic menu will be opened and the combo not yet released
			struct GameInp* pgi = GameInp;
			for (int combo_idx = 0; diag_input[combo_idx] != RETRO_DEVICE_ID_JOYPAD_EMPTY; combo_idx++)
			{
				for (int i = 0; i < nGameInpCount; i++, pgi++)
				{
					if (pgi->nInput == GIT_SWITCH)
					{
						pgi->Input.nVal = 0;
						*(pgi->Input.pVal) = pgi->Input.nVal;
					}
				}
			}

			// Activate the diagnostic key
			pgi_diag->Input.nVal = 1;
			*(pgi_diag->Input.pVal) = pgi_diag->Input.nVal;

			// Return true to stop polling game inputs while diagnostic combo inputs is pressed
			return true;
		}
	}

	// Return false to poll game inputs
	return false;
}

void SetControllerInfo()
{
	static const struct retro_controller_description controller_description[] = {
		{ "Classic", RETROPAD_CLASSIC },
		{ "Modern", RETROPAD_MODERN },
		{ "Mouse (ball only)", RETROMOUSE_BALL },
		{ "Mouse (full)", RETROMOUSE_FULL },
		{ "Pointer", RETRO_DEVICE_POINTER },
		{ "Lightgun", RETRO_DEVICE_LIGHTGUN }
	};

	struct retro_controller_info *controller_infos = (struct retro_controller_info*)calloc(nMaxPlayers+1, sizeof(struct retro_controller_info));

	for (int i = 0; i < nMaxPlayers; i++)
	{
		controller_infos[i].types = controller_description;
		controller_infos[i].num_types = sizeof(controller_description) / sizeof(controller_description[0]);
	}

	controller_infos[nMaxPlayers].types = NULL;
	controller_infos[nMaxPlayers].num_types = 0;

	environ_cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, controller_infos);
	free(controller_infos);
}

// Set the input descriptors
static void SetInputDescriptors()
{
	struct retro_input_descriptor *input_descriptors = (struct retro_input_descriptor*)calloc(normal_input_descriptors.size() + 1, sizeof(struct retro_input_descriptor));

	unsigned input_descriptor_idx = 0;

	for (unsigned i = 0; i < normal_input_descriptors.size(); i++, input_descriptor_idx++)
	{
		input_descriptors[input_descriptor_idx] = normal_input_descriptors[i];
	}

	input_descriptors[input_descriptor_idx].description = NULL;

	environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, input_descriptors);
	free(input_descriptors);
}

void InputMake(void)
{
	poll_cb();

	if (PollDiagInput())
		return;

	struct GameInp* pgi;
	UINT32 i;

	InputTick();

	for (i = 0, pgi = GameInp; i < nGameInpCount; i++, pgi++) {
		if (pgi->Input.pVal == NULL) {
			continue;
		}

		switch (pgi->nInput) {
			case 0:									// Undefined
				pgi->Input.nVal = 0;
				break;
			case GIT_CONSTANT:						// Constant value
				pgi->Input.nVal = pgi->Input.Constant.nConst;
				*(pgi->Input.pVal) = pgi->Input.nVal;
				break;
			case GIT_SWITCH: {						// Digital input
				INT32 s = CinpState(pgi->Input.Switch.nCode);

				if (pgi->nType & BIT_GROUP_ANALOG) {
					// Set analog controls to full
					if (s) {
						pgi->Input.nVal = 0xFFFF;
					} else {
						pgi->Input.nVal = 0x0001;
					}
#ifdef LSB_FIRST
					*(pgi->Input.pShortVal) = pgi->Input.nVal;
#else
					*((int *)pgi->Input.pShortVal) = pgi->Input.nVal;
#endif
				} else {
					// Binary controls
					if (s) {
						pgi->Input.nVal = 1;
					} else {
						pgi->Input.nVal = 0;
					}
					*(pgi->Input.pVal) = pgi->Input.nVal;
				}

				break;
			}
			case GIT_KEYSLIDER:						// Keyboard slider
			case GIT_JOYSLIDER:	{					// Joystick slider
				INT32 nSlider = pgi->Input.Slider.nSliderValue;
				if (pgi->nType == BIT_ANALOG_REL) {
					nSlider -= 0x8000;
					nSlider >>= 4;
				}

				pgi->Input.nVal = (UINT16)nSlider;
#ifdef LSB_FIRST
				*(pgi->Input.pShortVal) = pgi->Input.nVal;
#else
				*((int *)pgi->Input.pShortVal) = pgi->Input.nVal;
#endif
				break;
			}
			case GIT_MOUSEAXIS:						// Mouse axis
				pgi->Input.nVal = (UINT16)(CinpMouseAxis(pgi->Input.MouseAxis.nMouse, pgi->Input.MouseAxis.nAxis) * nAnalogSpeed);
#ifdef LSB_FIRST
				*(pgi->Input.pShortVal) = pgi->Input.nVal;
#else
				*((int *)pgi->Input.pShortVal) = pgi->Input.nVal;
#endif
				break;
			case GIT_DIRECT_COORD:
				CinpDirectCoord(pgi->Input.MouseAxis.nMouse, pgi->Input.MouseAxis.nAxis);
				break;
			case GIT_JOYAXIS_FULL:	{				// Joystick axis
				INT32 nJoy = CinpJoyAxis(pgi->Input.JoyAxis.nJoy, pgi->Input.JoyAxis.nAxis);

				if (pgi->nType == BIT_ANALOG_REL) {
					nJoy *= nAnalogSpeed;
					nJoy >>= 13;

					// Clip axis to 8 bits
					if (nJoy < -32768) {
						nJoy = -32768;
					}
					if (nJoy >  32767) {
						nJoy =  32767;
					}
				} else {
					nJoy >>= 1;
					nJoy += 0x8000;

					// Clip axis to 16 bits
					if (nJoy < 0x0001) {
						nJoy = 0x0001;
					}
					if (nJoy > 0xFFFF) {
						nJoy = 0xFFFF;
					}
				}

				pgi->Input.nVal = (UINT16)nJoy;
#ifdef LSB_FIRST
				*(pgi->Input.pShortVal) = pgi->Input.nVal;
#else
				*((int *)pgi->Input.pShortVal) = pgi->Input.nVal;
#endif

				break;
			}
			case GIT_JOYAXIS_NEG:	{				// Joystick axis Lo
				INT32 nJoy = CinpJoyAxis(pgi->Input.JoyAxis.nJoy, pgi->Input.JoyAxis.nAxis);
				if (nJoy < 32767) {
					nJoy = -nJoy;

					if (nJoy < 0x0000) {
						nJoy = 0x0000;
					}
					if (nJoy > 0xFFFF) {
						nJoy = 0xFFFF;
					}

					pgi->Input.nVal = (UINT16)nJoy;
				} else {
					pgi->Input.nVal = 0;
				}

#ifdef LSB_FIRST
				*(pgi->Input.pShortVal) = pgi->Input.nVal;
#else
				*((int *)pgi->Input.pShortVal) = pgi->Input.nVal;
#endif
				break;
			}
			case GIT_JOYAXIS_POS:	{				// Joystick axis Hi
				INT32 nJoy = CinpJoyAxis(pgi->Input.JoyAxis.nJoy, pgi->Input.JoyAxis.nAxis);
				if (nJoy > 32767) {

					if (nJoy < 0x0000) {
						nJoy = 0x0000;
					}
					if (nJoy > 0xFFFF) {
						nJoy = 0xFFFF;
					}

					pgi->Input.nVal = (UINT16)nJoy;
				} else {
					pgi->Input.nVal = 0;
				}

#ifdef LSB_FIRST
				*(pgi->Input.pShortVal) = pgi->Input.nVal;
#else
				*((int *)pgi->Input.pShortVal) = pgi->Input.nVal;
#endif
				break;
			}
		}
	}
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
	if (port < nMaxPlayers && nDeviceType[port] != device)
	{
		nDeviceType[port] = device;
		bool bAllDevicesReady = true;
		for (int i = 0; i < nMaxPlayers; i++)
		{
			if (nDeviceType[i] == -1)
				bAllDevicesReady = false;
		}
		// All devices id were set, we can do the following
		if (bAllDevicesReady) {
			GameInpReassign();
			SetInputDescriptors();
		}
	}
}

void InputInit()
{
	nSwitchCode = 0;

	normal_input_descriptors.clear();

	GameInpInit();
	GameInpDefault();

	// Update core option for diagnostic inputs
	set_environment();
	// Read the user core option values
	check_variables();

	// The list of input descriptors is filled, we can assign them to retroarch
	//SetInputDescriptors();

	/* serialization quirks for netplay, cps3 seems problematic, neogeo, cps1 and 2 seem to be good to go 
	uint64_t serialization_quirks = RETRO_SERIALIZATION_QUIRK_SINGLE_SESSION;
	if(!strcmp(systemname, "CPS-3"))
	environ_cb(RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS, &serialization_quirks);*/

	bInputInitialized = true;
}

void InputDeInit()
{
	bInputInitialized = false;
}
