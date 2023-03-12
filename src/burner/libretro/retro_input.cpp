#include <vector>
#include "retro_common.h"
#include "retro_input.h"
#include "burn_gun.h"

bool bStreetFighterLayout = false;

retro_input_state_t input_cb;
static retro_input_poll_t poll_cb;

static unsigned nDiagInputComboStartFrame = 0;
static unsigned nDiagInputHoldFrameDelay = 0;
static unsigned nSwitchCode = 0;
static unsigned nAxisNum = 0;
static unsigned nMahjongKeyboards = 0;
static unsigned nMaxControllers = 0;
static unsigned nDeviceType[MAX_PLAYERS];
static int nLibretroInputBitmask[MAX_PLAYERS];
static std::vector<retro_input_descriptor> normal_input_descriptors;
static struct KeyBind sKeyBinds[MAX_KEYBINDS];
static struct AxiBind sAxiBinds[MAX_PLAYERS*MAX_AXISES];
static INT32 pointerValues[MAX_PLAYERS][2];
static bool bAnalogRightMappingDone[MAX_PLAYERS][2][2];
static bool bButtonMapped = false;
static bool bOneDiagInputPressed = false;
static bool bAllDiagInputPressed = true;
static bool bDiagComboActivated = false;
static bool bVolumeIsFireButton = false;
static bool bInputInitialized = false;
static bool bControllersNeedRefresh = true;
static bool bControllersSetOnce = false;
static bool bLibretroSupportsBitmasks = false;
static char* pDirections[MAX_PLAYERS][6];

// Macros
UINT32 nMacroCount = 0;
UINT32 nMaxMacro = 0;

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
	struct GameInp* pgi;
	struct BurnInputInfo bii;

	INT32 nPunchx3[MAX_PLAYERS] = {0, };
	INT32 nPunchInputs[MAX_PLAYERS][3];
	INT32 nKickx3[MAX_PLAYERS] = {0, };
	INT32 nKickInputs[MAX_PLAYERS][3];
	INT32 nNeogeoButtons[MAX_PLAYERS][4];

	bStreetFighterLayout = false;
	nMahjongKeyboards = 0;
	bVolumeIsFireButton = false;
	nFireButtons = 0;
	nMacroCount = 0;
	memset(&nNeogeoButtons, 0, sizeof(nNeogeoButtons));

	for (UINT32 i = 0; i < nGameInpCount; i++) {
		bii.szName = NULL;
		BurnDrvGetInputInfo(&bii, i);
		if (bii.szName == NULL) {
			bii.szName = "";
		}

		bool bPlayerInInfo = (toupper(bii.szInfo[0]) == 'P' && bii.szInfo[1] >= '1' && bii.szInfo[1] <= '6'); // Because some of the older drivers don't use the standard input naming.
		bool bPlayerInName = (bii.szName[0] == 'P' && bii.szName[1] >= '1' && bii.szName[1] <= '6');

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

			// look for mahjong controls, if found set a number of mahjong keyboards equal to the number of players
			if ((strncmp("mah ", bii.szInfo, 4) == 0)) {
				nMahjongKeyboards = nMaxPlayers;
			}
			if ((strncmp("Volume", bii.szName, 6) == 0) && (strncmp(" fire", bii.szInfo + 2, 5) == 0)) {
				bVolumeIsFireButton = true;
			}
			if (_stricmp(" Weak Punch", bii.szName + 2) == 0) {
				nPunchx3[nPlayer] |= 1;
				nPunchInputs[nPlayer][0] = i;
			}
			if (_stricmp(" Medium Punch", bii.szName + 2) == 0) {
				nPunchx3[nPlayer] |= 2;
				nPunchInputs[nPlayer][1] = i;
			}
			if (_stricmp(" Strong Punch", bii.szName + 2) == 0) {
				nPunchx3[nPlayer] |= 4;
				nPunchInputs[nPlayer][2] = i;
			}
			if (_stricmp(" Weak Kick", bii.szName + 2) == 0) {
				nKickx3[nPlayer] |= 1;
				nKickInputs[nPlayer][0] = i;
			}
			if (_stricmp(" Medium Kick", bii.szName + 2) == 0) {
				nKickx3[nPlayer] |= 2;
				nKickInputs[nPlayer][1] = i;
			}
			if (_stricmp(" Strong Kick", bii.szName + 2) == 0) {
				nKickx3[nPlayer] |= 4;
				nKickInputs[nPlayer][2] = i;
			}

			if (bIsNeogeoCartGame || (nGameType == RETRO_GAME_TYPE_NEOCD)) {
				if (_stricmp(" Button A", bii.szName + 2) == 0) {
					nNeogeoButtons[nPlayer][0] = i;
				}
				if (_stricmp(" Button B", bii.szName + 2) == 0) {
					nNeogeoButtons[nPlayer][1] = i;
				}
				if (_stricmp(" Button C", bii.szName + 2) == 0) {
					nNeogeoButtons[nPlayer][2] = i;
				}
				if (_stricmp(" Button D", bii.szName + 2) == 0) {
					nNeogeoButtons[nPlayer][3] = i;
				}
			}
		}
	}

	pgi = GameInp + nGameInpCount;

	// We only support macros deemed "most useful" for now
	for (UINT32 nPlayer = 0; nPlayer < nMaxPlayers; nPlayer++) {
		if (nPunchx3[nPlayer] == 7) {		// Create a 3x punch macro
			pgi->nInput = GIT_MACRO_AUTO;
			pgi->nType = BIT_DIGITAL;

			sprintf(pgi->Macro.szName, "P%i Buttons 3x Punch", nPlayer + 1);
			for (INT32 j = 0; j < 3; j++) {
				BurnDrvGetInputInfo(&bii, nPunchInputs[nPlayer][j]);
				pgi->Macro.pVal[j] = bii.pVal;
				pgi->Macro.nVal[j] = 1;
			}

			nMacroCount++;
			pgi++;
		}
		if (nKickx3[nPlayer] == 7) {		// Create a 3x kick macro
			pgi->nInput = GIT_MACRO_AUTO;
			pgi->nType = BIT_DIGITAL;

			sprintf(pgi->Macro.szName, "P%i Buttons 3x Kick", nPlayer + 1);
			for (INT32 j = 0; j < 3; j++) {
				BurnDrvGetInputInfo(&bii, nKickInputs[nPlayer][j]);
				pgi->Macro.pVal[j] = bii.pVal;
				pgi->Macro.nVal[j] = 1;
			}

			nMacroCount++;
			pgi++;
		}
		// supposedly, those are the 4 most useful neogeo macros
		if (bIsNeogeoCartGame || (nGameType == RETRO_GAME_TYPE_NEOCD)) {
			pgi->nInput = GIT_MACRO_AUTO;
			pgi->nType = BIT_DIGITAL;
			pgi->Macro.nMode = 0;
			sprintf(pgi->Macro.szName, "P%i Buttons AB", nPlayer + 1);
			BurnDrvGetInputInfo(&bii, nNeogeoButtons[nPlayer][0]);
			pgi->Macro.pVal[0] = bii.pVal;
			pgi->Macro.nVal[0] = 1;
			BurnDrvGetInputInfo(&bii, nNeogeoButtons[nPlayer][1]);
			pgi->Macro.pVal[1] = bii.pVal;
			pgi->Macro.nVal[1] = 1;
			nMacroCount++;
			pgi++;

			pgi->nInput = GIT_MACRO_AUTO;
			pgi->nType = BIT_DIGITAL;
			pgi->Macro.nMode = 0;
			sprintf(pgi->Macro.szName, "P%i Buttons CD", nPlayer + 1);
			BurnDrvGetInputInfo(&bii, nNeogeoButtons[nPlayer][2]);
			pgi->Macro.pVal[0] = bii.pVal;
			pgi->Macro.nVal[0] = 1;
			BurnDrvGetInputInfo(&bii, nNeogeoButtons[nPlayer][3]);
			pgi->Macro.pVal[1] = bii.pVal;
			pgi->Macro.nVal[1] = 1;
			nMacroCount++;
			pgi++;

			pgi->nInput = GIT_MACRO_AUTO;
			pgi->nType = BIT_DIGITAL;
			pgi->Macro.nMode = 0;
			sprintf(pgi->Macro.szName, "P%i Buttons ABC", nPlayer + 1);
			BurnDrvGetInputInfo(&bii, nNeogeoButtons[nPlayer][0]);
			pgi->Macro.pVal[0] = bii.pVal;
			pgi->Macro.nVal[0] = 1;
			BurnDrvGetInputInfo(&bii, nNeogeoButtons[nPlayer][1]);
			pgi->Macro.pVal[1] = bii.pVal;
			pgi->Macro.nVal[1] = 1;
			BurnDrvGetInputInfo(&bii, nNeogeoButtons[nPlayer][2]);
			pgi->Macro.pVal[2] = bii.pVal;
			pgi->Macro.nVal[2] = 1;
			nMacroCount++;
			pgi++;

			pgi->nInput = GIT_MACRO_AUTO;
			pgi->nType = BIT_DIGITAL;
			pgi->Macro.nMode = 0;
			sprintf(pgi->Macro.szName, "P%i Buttons BCD", nPlayer + 1);
			BurnDrvGetInputInfo(&bii, nNeogeoButtons[nPlayer][1]);
			pgi->Macro.pVal[0] = bii.pVal;
			pgi->Macro.nVal[0] = 1;
			BurnDrvGetInputInfo(&bii, nNeogeoButtons[nPlayer][2]);
			pgi->Macro.pVal[1] = bii.pVal;
			pgi->Macro.nVal[1] = 1;
			BurnDrvGetInputInfo(&bii, nNeogeoButtons[nPlayer][3]);
			pgi->Macro.pVal[2] = bii.pVal;
			pgi->Macro.nVal[2] = 1;
			nMacroCount++;
			pgi++;
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
	// Count the number of inputs
	nGameInpCount = 0;
	nMacroCount = 0;

	// We only support up to 4 macros for now
	nMaxMacro = nMaxPlayers * 4;

	while (BurnDrvGetInputInfo(NULL,nGameInpCount) == 0)
		nGameInpCount++;

	// Allocate space for all the inputs
	GameInp = (struct GameInp*)calloc(nGameInpCount + nMaxMacro, sizeof(struct GameInp));
	if (GameInp == NULL) {
		return 1;
	}

	GameInpBlank(1);

	InpDIPSWResetDIPs();

	AnalyzeGameLayout();

	return 0;
}

static inline int input_cb_wrapper(unsigned port, unsigned device, unsigned index, unsigned id)
{
	if (bLibretroSupportsBitmasks && device == RETRO_DEVICE_JOYPAD)
	{
		if (nLibretroInputBitmask[port] == -1)
			nLibretroInputBitmask[port] = input_cb(port, RETRO_DEVICE_JOYPAD, index, RETRO_DEVICE_ID_JOYPAD_MASK);
		return (nLibretroInputBitmask[port] & (1 << id));
	}
	else
		return input_cb(port, device, index, id);
}

// Deadzone when faking digital controls through analog sticks
#define FAKE_ANALOG_DEADZONE 10000

static inline int CinpState(int nCode)
{
	unsigned device = sKeyBinds[nCode].device;

	// Ignore unmapped
	if (device == RETRO_DEVICE_NONE)
		return 0;

	unsigned id = sKeyBinds[nCode].id;
	unsigned port = sKeyBinds[nCode].port;
	int index = sKeyBinds[nCode].index;
	if (index == -1)
	{
		if (input_cb_wrapper(port, device, 0, id))
			return 1;

		// Handle fake analog inputs
		if (pDirections[port][PGI_ANALOG_X] == NULL && pDirections[port][PGI_LEFT] != NULL && pDirections[port][PGI_RIGHT] != NULL)
		{
			int s = input_cb_wrapper(port, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X);
			if ((s < -FAKE_ANALOG_DEADZONE && id == RETRO_DEVICE_ID_JOYPAD_LEFT) || (s > FAKE_ANALOG_DEADZONE && id == RETRO_DEVICE_ID_JOYPAD_RIGHT))
				return 1;
		}
		if (pDirections[port][PGI_ANALOG_Y] == NULL && pDirections[port][PGI_UP] != NULL && pDirections[port][PGI_DOWN] != NULL)
		{
			int s = input_cb_wrapper(port, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y);
			if ((s < -FAKE_ANALOG_DEADZONE && id == RETRO_DEVICE_ID_JOYPAD_UP) || (s > FAKE_ANALOG_DEADZONE && id == RETRO_DEVICE_ID_JOYPAD_DOWN))
				return 1;
		}
	}
	else
	{
		int s = input_cb_wrapper(port, device, index, id);
		unsigned position = sKeyBinds[nCode].position;
		// Using a large deadzone when mapping microswitches to analog axis
		// Or said axis become way too sensitive and some game become unplayable (assault)
		if ((s < -FAKE_ANALOG_DEADZONE && position == JOY_NEG) || (s > FAKE_ANALOG_DEADZONE && position == JOY_POS))
			return 1;
	}
	return 0;
}

#undef FAKE_ANALOG_DEADZONE

static inline int CinpJoyAxis(int port, int axis)
{
	int index = sAxiBinds[axis].index;

	// Ignore unmapped
	if (index == -1)
		return 0;

	// Handle analog input
	int ret;
	ret = input_cb_wrapper(port, RETRO_DEVICE_ANALOG, index, sAxiBinds[axis].id);

	// Fallback if analog trigger requested but not supported
	if (ret == 0 && index == RETRO_DEVICE_INDEX_ANALOG_BUTTON)
		ret = input_cb_wrapper(port, RETRO_DEVICE_JOYPAD, 0, sAxiBinds[axis].id) ? 0x7FFF : 0;

	// Handle fake digital inputs
	if (sAxiBinds[axis].id == RETRO_DEVICE_ID_ANALOG_X && pDirections[port][PGI_ANALOG_X] != NULL && pDirections[port][PGI_LEFT] == NULL && pDirections[port][PGI_RIGHT] == NULL)
	{
		if (nDeviceType[port] == RETRO_DEVICE_LIGHTGUN)
		{
			if (input_cb_wrapper(port, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_DPAD_LEFT))
				ret = -0x7FFF;
			if (input_cb_wrapper(port, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_DPAD_RIGHT))
				ret = 0x7FFF;
		}
		else
		{
			if (input_cb_wrapper(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT))
				ret = -0x7FFF;
			if (input_cb_wrapper(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT))
				ret = 0x7FFF;
		}
	}
	if (sAxiBinds[axis].id == RETRO_DEVICE_ID_ANALOG_Y && pDirections[port][PGI_ANALOG_Y] != NULL && pDirections[port][PGI_UP] == NULL && pDirections[port][PGI_DOWN] == NULL)
	{
		if (nDeviceType[port] == RETRO_DEVICE_LIGHTGUN)
		{
			if (input_cb_wrapper(port, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_DPAD_UP))
				ret = -0x7FFF;
			if (input_cb_wrapper(port, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_DPAD_DOWN))
				ret = 0x7FFF;
		}
		else
		{
			if (input_cb_wrapper(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP))
				ret = -0x7FFF;
			if (input_cb_wrapper(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN))
				ret = 0x7FFF;
			}
	}

	return ret;
}

static inline void CinpDirectCoord(int port, int axis)
{
	INT32 is_offscreen = (nDeviceType[port] == RETRO_DEVICE_LIGHTGUN ? input_cb_wrapper(port, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN) : 0);
	INT32 val = input_cb_wrapper(port, nDeviceType[port], 0, sAxiBinds[axis].id) + 0x7fff;
	if (val == -1 || is_offscreen == 1)
	{
		// we are offscreen and should force coords at (0,0)
		pointerValues[port][0] = 0;
		pointerValues[port][1] = 0;
	}
	else
	{
		INT32 width, height;
		BurnDrvGetVisibleSize(&width, &height);
		// Check whether this axis is X or Y, update pointerValues accordingly
		if ((nDeviceType[port] == RETRO_DEVICE_LIGHTGUN && sAxiBinds[axis].id == RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X) || (nDeviceType[port] == RETRO_DEVICE_POINTER && sAxiBinds[axis].id == RETRO_DEVICE_ID_POINTER_X))
			pointerValues[port][0] = (INT32)(width * (double(val)/double(0x10000)));
		else if ((nDeviceType[port] == RETRO_DEVICE_LIGHTGUN && sAxiBinds[axis].id == RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y) || (nDeviceType[port] == RETRO_DEVICE_POINTER && sAxiBinds[axis].id == RETRO_DEVICE_ID_POINTER_Y))
			pointerValues[port][1] = (INT32)(height * (double(val)/double(0x10000)));
	}
	int player = port;
	if ((BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_NES || (strcmp(BurnDrvGetTextA(DRV_NAME), "cybertnk") == 0))
		player = 0;
	BurnGunSetCoords(player, pointerValues[port][0], pointerValues[port][1]);
}

static inline int CinpMouseAxis(int port, int axis)
{
	return input_cb_wrapper(port, RETRO_DEVICE_MOUSE, 0, sAxiBinds[axis].id);
}

// Analog to analog mapping
static INT32 GameInpAnalog2RetroInpAnalog(struct GameInp* pgi, unsigned port, unsigned id, int index, char *szn, UINT8 nInput = GIT_JOYAXIS_FULL, INT32 nSliderValue = 0x8000, INT16 nSliderSpeed = 0x0800, INT16 nSliderCenter = 10)
{
	if(bButtonMapped || !(pgi->nType & BIT_GROUP_ANALOG)) return 0;
	if (!bInputInitialized) {
		// It shouldn't hurt to set both even if we are only gonna use one at once
		// note : in this port, there is no benefit from making a difference between JoyAxis and MouseAxis ? maybe we should forget about using gameinp.h's structs and define something more "unified" for this port
		pgi->Input.JoyAxis.nAxis = nAxisNum++;
		pgi->Input.MouseAxis.nAxis = nAxisNum;
		HandleMessage(RETRO_LOG_DEBUG, "[FBNeo] nAxisNum 0x%02X : P%d %s\n", nAxisNum, port+1, szn);
	}
	switch (nInput)
	{
		case GIT_JOYAXIS_FULL:
		{
			pgi->nInput = GIT_JOYAXIS_FULL;
			pgi->Input.JoyAxis.nJoy = (UINT8)port;
			sAxiBinds[pgi->Input.JoyAxis.nAxis].index = index;
			sAxiBinds[pgi->Input.JoyAxis.nAxis].id = id;
			retro_input_descriptor descriptor;
			descriptor.port = port;
			descriptor.device = (index == RETRO_DEVICE_INDEX_ANALOG_BUTTON ? RETRO_DEVICE_JOYPAD : RETRO_DEVICE_ANALOG);
			descriptor.index = (index == RETRO_DEVICE_INDEX_ANALOG_BUTTON ? 0 : index);
			descriptor.id = id;
			descriptor.description = szn;
			normal_input_descriptors.push_back(descriptor);
			if (index == RETRO_DEVICE_INDEX_ANALOG_LEFT)
			{
				switch (id)
				{
					case RETRO_DEVICE_ID_ANALOG_X:
						pDirections[port][PGI_ANALOG_X] = szn;
						break;
					case RETRO_DEVICE_ID_ANALOG_Y:
						pDirections[port][PGI_ANALOG_Y] = szn;
						break;
					default:
						break;
				}
			}
			break;
		}
		case GIT_MOUSEAXIS:
		{
			pgi->nInput = GIT_MOUSEAXIS;
			pgi->Input.MouseAxis.nMouse = (UINT8)port;
			sAxiBinds[pgi->Input.MouseAxis.nAxis].index = index;
			sAxiBinds[pgi->Input.MouseAxis.nAxis].id = id;
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
			pgi->Input.MouseAxis.nMouse = (UINT8)port;
			sAxiBinds[pgi->Input.MouseAxis.nAxis].index = index;
			sAxiBinds[pgi->Input.MouseAxis.nAxis].id = id;
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
static INT32 GameInpDigital2RetroInpKey(struct GameInp* pgi, unsigned port, unsigned id, char *szn, unsigned device = RETRO_DEVICE_JOYPAD, unsigned nInput = GIT_SWITCH)
{
	if(bButtonMapped || pgi->nType != BIT_DIGITAL) return 0;
	pgi->nInput = nInput;
	if(nInput == GIT_SWITCH)
	{
		if (!bInputInitialized) {
			pgi->Input.Switch.nCode = (UINT16)(nSwitchCode++);
			HandleMessage(RETRO_LOG_DEBUG, "[FBNeo] nSwitchCode 0x%02X : P%d %s\n", pgi->Input.Switch.nCode, port+1, szn);
		}
		sKeyBinds[pgi->Input.Switch.nCode].id = id;
		sKeyBinds[pgi->Input.Switch.nCode].port = port;
		sKeyBinds[pgi->Input.Switch.nCode].device = device;
		sKeyBinds[pgi->Input.Switch.nCode].index = -1;
	}
	if(nInput == GIT_MACRO_AUTO)
	{
		if (!bInputInitialized) {
			pgi->Macro.Switch.nCode = (UINT16)(nSwitchCode++);
			HandleMessage(RETRO_LOG_DEBUG, "[FBNeo] nSwitchCode 0x%02X : P%d %s\n", pgi->Macro.Switch.nCode, port+1, szn);
		}
		sKeyBinds[pgi->Macro.Switch.nCode].id = id;
		sKeyBinds[pgi->Macro.Switch.nCode].port = port;
		sKeyBinds[pgi->Macro.Switch.nCode].device = device;
		sKeyBinds[pgi->Macro.Switch.nCode].index = -1;
	}
	retro_input_descriptor descriptor;
	descriptor.port = port;
	descriptor.device = device;
	descriptor.index = 0;
	descriptor.id = id;
	descriptor.description = szn;
	normal_input_descriptors.push_back(descriptor);
	bButtonMapped = true;
	if (device == RETRO_DEVICE_JOYPAD)
	{
		switch (id)
		{
			case RETRO_DEVICE_ID_JOYPAD_UP:
				pDirections[port][PGI_UP] = szn;
				break;
			case RETRO_DEVICE_ID_JOYPAD_DOWN:
				pDirections[port][PGI_DOWN] = szn;
				break;
			case RETRO_DEVICE_ID_JOYPAD_LEFT:
				pDirections[port][PGI_LEFT] = szn;
				break;
			case RETRO_DEVICE_ID_JOYPAD_RIGHT:
				pDirections[port][PGI_RIGHT] = szn;
				break;
			default:
				break;
		}
	}
	return 0;
}

// 2 digital to 1 analog mapping
// Need to be run once for each of the 2 pgi (the 2 game inputs)
// nJoy (player) and nKey (axis) needs to be the same for each of the 2 buttons
// position is either JOY_POS or JOY_NEG (the position expected on axis to trigger the button)
// szn is the descriptor text
static INT32 GameInpDigital2RetroInpAnalogRight(struct GameInp* pgi, unsigned port, unsigned id, unsigned position, char *szn)
{
	if(bButtonMapped || pgi->nType != BIT_DIGITAL) return 0;
	pgi->nInput = GIT_SWITCH;
	if (!bInputInitialized) {
		pgi->Input.Switch.nCode = (UINT16)(nSwitchCode++);
		HandleMessage(RETRO_LOG_DEBUG, "[FBNeo] nSwitchCode 0x%02X : P%d %s\n", pgi->Input.Switch.nCode, port+1, szn);
	}
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

// Meant to fine tune mapping
static INT32 GameInpSpecialOne(struct GameInp* pgi, INT32 nPlayer, char* szb, char *szn, char *description)
{
	const char * parentrom	= BurnDrvGetTextA(DRV_PARENT);
	const char * drvname	= BurnDrvGetTextA(DRV_NAME);
	int nHardwareCode = BurnDrvGetHardwareCode();

	// We don't map volume buttons
	if (strncmp("Volume", description, 6) == 0)
	{
		pgi->nInput = GIT_SWITCH;
		if (!bInputInitialized) {
			pgi->Input.Switch.nCode = (UINT16)(nSwitchCode++);
			HandleMessage(RETRO_LOG_DEBUG, "[FBNeo] nSwitchCode 0x%02X : P%d %s (not mapped)\n", pgi->Input.Switch.nCode, nPlayer+1, szn);
		}
		bButtonMapped = true;
		return 0;
	}

	// This one is such a special case : "Lucky & Wild" has 2 x-axis inputs, and we don't want the steering one to be caught by the pointer/lightgun/mouse logic
	if ((parentrom && strcmp(parentrom, "luckywld") == 0) ||
		(drvname && strcmp(drvname, "luckywld") == 0)
	) {
		if (strcmp("Steering", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_X, RETRO_DEVICE_INDEX_ANALOG_LEFT, description);
		}
	}

	// After handling super special case(s), we can handle generic pointer/lightgun/mouse logic
	if (nDeviceType[nPlayer] == RETRO_DEVICE_POINTER && BurnGunIsActive()) {
		if (strcmp("x-axis", szb) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_POINTER_X, 0, description, GIT_DIRECT_COORD);
		}
		if (strcmp("y-axis", szb) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_POINTER_Y, 0, description, GIT_DIRECT_COORD);
		}
	}

	if (nDeviceType[nPlayer] == RETRO_DEVICE_LIGHTGUN && BurnGunIsActive()) {
		if (strcmp("x-axis", szb) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X, 0, description, GIT_DIRECT_COORD);
		}
		if (strcmp("y-axis", szb) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y, 0, description, GIT_DIRECT_COORD);
		}
		if (strcmp("fire 1", szb) == 0 || strcmp("button 1", szb) == 0 || strcmp("button", szb) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_LIGHTGUN_TRIGGER, description, RETRO_DEVICE_LIGHTGUN);
		}
		if (strcmp("fire 2", szb) == 0 || strcmp("button 2", szb) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_LIGHTGUN_AUX_A, description, RETRO_DEVICE_LIGHTGUN);
		}
		if (strcmp("fire 3", szb) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_LIGHTGUN_AUX_B, description, RETRO_DEVICE_LIGHTGUN);
		}
		if (strcmp("fire 4", szb) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_LIGHTGUN_AUX_C, description, RETRO_DEVICE_LIGHTGUN);
		}
		if (strcmp("up", szb) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_LIGHTGUN_DPAD_UP, description, RETRO_DEVICE_LIGHTGUN);
		}
		if (strcmp("down", szb) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_LIGHTGUN_DPAD_DOWN, description, RETRO_DEVICE_LIGHTGUN);
		}
		if (strcmp("left", szb) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_LIGHTGUN_DPAD_LEFT, description, RETRO_DEVICE_LIGHTGUN);
		}
		if (strcmp("right", szb) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_LIGHTGUN_DPAD_RIGHT, description, RETRO_DEVICE_LIGHTGUN);
		}
		if (strcmp("start", szb) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_LIGHTGUN_START, description, RETRO_DEVICE_LIGHTGUN);
		}
		if (strcmp("coin", szb) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_LIGHTGUN_SELECT, description, RETRO_DEVICE_LIGHTGUN);
		}
	}

	if (nDeviceType[nPlayer] == RETROMOUSE_BALL || nDeviceType[nPlayer] == RETROMOUSE_FULL) {
		if (strcmp("x-axis", szb) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_MOUSE_X, 0, description, GIT_MOUSEAXIS);
		}
		if (strcmp("y-axis", szb) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_MOUSE_Y, 0, description, GIT_MOUSEAXIS);
		}
		if (nDeviceType[nPlayer] == RETROMOUSE_FULL) {
			// Handle mouse button mapping (i will keep it simple...)
			if (strcmp("fire 1", szb) == 0 || strcmp("button 1", szb) == 0 || strcmp("button", szb) == 0) {
				GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_MOUSE_LEFT, description, RETRO_DEVICE_MOUSE);
			}
			if (strcmp("fire 2", szb) == 0 || strcmp("button 2", szb) == 0) {
				GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_MOUSE_RIGHT, description, RETRO_DEVICE_MOUSE);
			}
			if (strcmp("fire 3", szb) == 0) {
				GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_MOUSE_MIDDLE, description, RETRO_DEVICE_MOUSE);
			}
			if (strcmp("fire 4", szb) == 0) {
				GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_MOUSE_BUTTON_4, description, RETRO_DEVICE_MOUSE);
			}
			if (strcmp("fire 5", szb) == 0) {
				GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_MOUSE_BUTTON_5, description, RETRO_DEVICE_MOUSE);
			}
		}
	}

	// Let's finish "Lucky & Wild" mapping for standard gamepad
	if ((parentrom && strcmp(parentrom, "luckywld") == 0) ||
		(drvname && strcmp(drvname, "luckywld") == 0)
	) {
		if (strcmp("Accelerator", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R2, RETRO_DEVICE_INDEX_ANALOG_BUTTON, description);
		}
		if (strcmp("Brake", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_L2, RETRO_DEVICE_INDEX_ANALOG_BUTTON, description);
		}
		if (strcmp("Gun X", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_X, RETRO_DEVICE_INDEX_ANALOG_RIGHT, description);
		}
		if (strcmp("Gun Y", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_Y, RETRO_DEVICE_INDEX_ANALOG_RIGHT, description);
		}
		if (strcmp("Button 1", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R, description);
		}
	}

	// Crazy Fight
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

	// Bikkuri Card
	// Coin 2 overwrited Coin 1, which is probably part of the issue
	// I managed to pass the payout tests once, but i don't know how
	if ((parentrom && strcmp(parentrom, "bikkuric") == 0) ||
		(drvname && strcmp(drvname, "bikkuric") == 0)
	) {
		if (strcmp("Coin 2", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_START, description);
		}
	}

	// Last Survivor
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

	// Lunar Lander
	if ((parentrom && strcmp(parentrom, "llander") == 0) ||
		(drvname && strcmp(drvname, "llander") == 0)
	) {
		if (strcmp("Select Game", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R3, description);
		}
		if (strcmp("Abort", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_L3, description);
		}
	}

	// Move 2-players start to P1's R3 for the asteroids driver
	if ((parentrom && strcmp(parentrom, "asteroid") == 0) ||
		(drvname && strcmp(drvname, "asteroid") == 0) ||
		(parentrom && strcmp(parentrom, "astdelux") == 0) ||
		(drvname && strcmp(drvname, "astdelux") == 0)
	) {
		if (strcmp("Start", description) == 0 && nPlayer == 1) {
			GameInpDigital2RetroInpKey(pgi, 0, RETRO_DEVICE_ID_JOYPAD_R3, "2P Start");
		}
	}

	// Space Duel
	// Scud Hammer
	if ((parentrom && strcmp(parentrom, "spacduel") == 0) ||
		(drvname && strcmp(drvname, "spacduel") == 0) ||
		(parentrom && strcmp(parentrom, "scudhamm") == 0) ||
		(drvname && strcmp(drvname, "scudhamm") == 0)
	) {
		if (strcmp("Select", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R3, description);
		}
	}

	// Out Run
	// Super Hang-On
	// Turbo Out Run
	// Power Drift
	// A.B. Cop
	// Super Monaco GP
	// GP Rider
	// Racing Hero
	// Hang-On
	// Enduro Racer
	// Hang-On Jr.
	// Sega System32 racing games
	if ((parentrom && strcmp(parentrom, "outrun") == 0) ||
		(drvname && strcmp(drvname, "outrun") == 0) ||
		(parentrom && strcmp(parentrom, "shangon") == 0) ||
		(drvname && strcmp(drvname, "shangon") == 0) ||
		(parentrom && strcmp(parentrom, "toutrun") == 0) ||
		(drvname && strcmp(drvname, "toutrun") == 0) ||
		(parentrom && strcmp(parentrom, "pdrift") == 0) ||
		(drvname && strcmp(drvname, "pdrift") == 0) ||
		(parentrom && strcmp(parentrom, "abcop") == 0) ||
		(drvname && strcmp(drvname, "abcop") == 0) ||
		(parentrom && strcmp(parentrom, "smgp") == 0) ||
		(drvname && strcmp(drvname, "smgp") == 0) ||
		(parentrom && strcmp(parentrom, "gprider") == 0) ||
		(drvname && strcmp(drvname, "gprider") == 0) ||
		(parentrom && strcmp(parentrom, "rachero") == 0) ||
		(drvname && strcmp(drvname, "rachero") == 0) ||
		(parentrom && strcmp(parentrom, "hangon") == 0) ||
		(drvname && strcmp(drvname, "hangon") == 0) ||
		(parentrom && strcmp(parentrom, "enduror") == 0) ||
		(drvname && strcmp(drvname, "enduror") == 0) ||
		(parentrom && strcmp(parentrom, "hangonjr") == 0) ||
		(drvname && strcmp(drvname, "hangonjr") == 0) ||
		(parentrom && strcmp(parentrom, "f1en") == 0) ||
		(drvname && strcmp(drvname, "f1en") == 0) ||
		(parentrom && strcmp(parentrom, "f1lap") == 0) ||
		(drvname && strcmp(drvname, "f1lap") == 0) ||
		(parentrom && strcmp(parentrom, "radm") == 0) ||
		(drvname && strcmp(drvname, "radm") == 0) ||
		(parentrom && strcmp(parentrom, "radr") == 0) ||
		(drvname && strcmp(drvname, "radr") == 0) ||
		(parentrom && strcmp(parentrom, "slipstrm") == 0) ||
		(drvname && strcmp(drvname, "slipstrm") == 0) ||
		(parentrom && strcmp(parentrom, "orunners") == 0) ||
		(drvname && strcmp(drvname, "orunners") == 0) ||
		(parentrom && strcmp(parentrom, "gforce2") == 0) ||
		(drvname && strcmp(drvname, "gforce2") == 0) ||
		(parentrom && strcmp(parentrom, "scross") == 0) ||
		(drvname && strcmp(drvname, "scross") == 0)
	) {
		if (strcmp("Accelerate", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R2, RETRO_DEVICE_INDEX_ANALOG_BUTTON, description);
		}
		if (strcmp("Brake", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_L2, RETRO_DEVICE_INDEX_ANALOG_BUTTON, description);
			// scross uses digital for Brake
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_L2, description);
		}
		if (strcmp("Gear", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_B, description);
		}
		if (strcmp("Super Charger", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_B, description);
		}
		if (strcmp("Jump", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_B, description);
		}
		if (strcmp("Move to centre", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_B, description);
		}
		if (strcmp("Nitro", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_A, description);
		}
		if (strcmp("Suicide", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_A, description);
		}
		if (strcmp("Shift Down", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_L, description);
		}
		if (strcmp("Shift Up", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R, description);
		}
	}

	// Golden Axe
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

	// Altered Beast
	if ((parentrom && strcmp(parentrom, "altbeast") == 0) ||
		(drvname && strcmp(drvname, "altbeast") == 0)
	) {
		if (strcmp("Fire 1", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_Y, "Jump");
		}
		if (strcmp("Fire 2", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_B, "Punch");
		}
		if (strcmp("Fire 3", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_A, "Kick");
		}
	}

	// Major League
	if ((parentrom && strcmp(parentrom, "mjleague") == 0) ||
		(drvname && strcmp(drvname, "mjleague") == 0)
	) {
		if (strcmp("Bat Swing", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R2, RETRO_DEVICE_INDEX_ANALOG_BUTTON, description);
		}
		if (strcmp("Fire 1", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_B, description);
		}
		if (strcmp("Fire 2", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_A, description);
		}
		if (strcmp("Fire 3", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_Y, description);
		}
		if (strcmp("Fire 4", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_X, description);
		}
		if (strcmp("Fire 5", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R, description);
		}
	}

	// Chase HQ
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

	// SDI - Strategic Defense Initiative
	if ((parentrom && strcmp(parentrom, "sdi") == 0) ||
		(drvname && strcmp(drvname, "sdi") == 0)
	) {
		if (strcmp("Target L/R", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_X, RETRO_DEVICE_INDEX_ANALOG_RIGHT, description);
		}
		if (strcmp("Target U/D", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_Y, RETRO_DEVICE_INDEX_ANALOG_RIGHT, description);
		}
		if (strcmp("Fire 1", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R, description);
		}
	}

	// Forgotten Worlds
	if ((parentrom && strcmp(parentrom, "forgottn") == 0) ||
		(drvname && strcmp(drvname, "forgottn") == 0)
	) {
		if (strcmp("Turn (analog)", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_X, RETRO_DEVICE_INDEX_ANALOG_RIGHT, description);
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
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_X, RETRO_DEVICE_INDEX_ANALOG_LEFT, description);
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
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_X, RETRO_DEVICE_INDEX_ANALOG_LEFT, description);
		}
		if (strcmp("Y Axis", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_Y, RETRO_DEVICE_INDEX_ANALOG_LEFT, description);
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
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_X, RETRO_DEVICE_INDEX_ANALOG_LEFT, description);
		}
		if (strcmp("Y-Axis", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_Y, RETRO_DEVICE_INDEX_ANALOG_LEFT, description);
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
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_X, RETRO_DEVICE_INDEX_ANALOG_LEFT, description);
		}
		if (strcmp("Up / Down", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_Y, RETRO_DEVICE_INDEX_ANALOG_LEFT, description);
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
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_X, RETRO_DEVICE_INDEX_ANALOG_LEFT, description);
		}
		if (strcmp("Gun Y", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_Y, RETRO_DEVICE_INDEX_ANALOG_LEFT, description);
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
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_X, RETRO_DEVICE_INDEX_ANALOG_LEFT, description);
		}
		if (strcmp("Up/Down", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_Y, RETRO_DEVICE_INDEX_ANALOG_LEFT, description);
		}
		if (strcmp("Fire 1", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_B, description);
		}
	}

	// VS Block Breaker
	if ((parentrom && strcmp(parentrom, "vblokbrk") == 0) ||
		(drvname && strcmp(drvname, "vblokbrk") == 0)
	) {
		if (strcmp("Paddle", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_X, RETRO_DEVICE_INDEX_ANALOG_LEFT, description);
		}
	}

	// Puzz Loop 2
	if ((parentrom && strcmp(parentrom, "pzloop2") == 0) ||
		(drvname && strcmp(drvname, "pzloop2") == 0)
	) {
		if (strcmp("Paddle", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_X, RETRO_DEVICE_INDEX_ANALOG_LEFT, description);
		}
	}

	// G-LOC Air Battle
	// Galaxy Force 2
	// Air Rescue
	if ((parentrom && strcmp(parentrom, "gloc") == 0) ||
		(drvname && strcmp(drvname, "gloc") == 0) ||
		(parentrom && strcmp(parentrom, "strkfgtr") == 0) ||
		(drvname && strcmp(drvname, "strkfgtr") == 0) ||
		(parentrom && strcmp(parentrom, "rascot") == 0) ||
		(drvname && strcmp(drvname, "rascot") == 0) ||
		(parentrom && strcmp(parentrom, "arescue") == 0) ||
		(drvname && strcmp(drvname, "arescue") == 0)
	) {
		if (strcmp("Throttle", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R2, RETRO_DEVICE_INDEX_ANALOG_BUTTON, description);
		}
	}

	// Alien vs Predator
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
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_X, RETRO_DEVICE_INDEX_ANALOG_LEFT, description);
		}
		if (strcmp("Gun Y", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_Y, RETRO_DEVICE_INDEX_ANALOG_LEFT, description);
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

	// Thunder Ceptor & Thunder Ceptor II
	if ((parentrom && strcmp(parentrom, "tceptor") == 0) ||
		(drvname && strcmp(drvname, "tceptor") == 0)
	) {
		if (strcmp("Accelerator", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R2, RETRO_DEVICE_INDEX_ANALOG_BUTTON, description);
		}
		if (strcmp("Boost", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R, description);
		}
		if (strcmp("Right Thumb", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_B, description);
		}
		if (strcmp("Left Thumb", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_A, description);
		}
		if (strcmp("Right Trigger", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_Y, description);
		}
		if (strcmp("Left Trigger", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_X, description);
		}
	}

	// Heavyweight Champ
	if ((parentrom && strcmp(parentrom, "hwchamp") == 0) ||
		(drvname && strcmp(drvname, "hwchamp") == 0)
	) {
		if (strcmp("Right", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R2, RETRO_DEVICE_INDEX_ANALOG_BUTTON, description);
		}
		if (strcmp("Left", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_L2, RETRO_DEVICE_INDEX_ANALOG_BUTTON, description);
		}
	}

	// Chequered Flag
	if ((parentrom && strcmp(parentrom, "chqflag") == 0) ||
		(drvname && strcmp(drvname, "chqflag") == 0)
	) {
		if (strcmp("Accelerator", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R2, RETRO_DEVICE_INDEX_ANALOG_BUTTON, description);
		}
		if (strcmp("Button 1", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_B, description);
		}
		if (strcmp("Button 2", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_A, description);
		}
	}

	// Top Speed
	if ((parentrom && strcmp(parentrom, "topspeed") == 0) ||
		(drvname && strcmp(drvname, "topspeed") == 0)
	) {
		if (strcmp("Fire 1 (Accelerator)", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R2, RETRO_DEVICE_INDEX_ANALOG_BUTTON, description);
		}
		if (strcmp("Fire 2 (Brake)", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_L2, RETRO_DEVICE_INDEX_ANALOG_BUTTON, description);
		}
		if (strcmp("Fire 3 (Nitro)", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_B, description);
		}
		if (strcmp("Fire 4 (Gear)", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_A, description);
		}
	}

	// World PK Soccer V2
	if ((parentrom && strcmp(parentrom, "wpksocv2") == 0) ||
		(drvname && strcmp(drvname, "wpksocv2") == 0)
	) {
		if (strcmp("Kick", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R2, RETRO_DEVICE_INDEX_ANALOG_BUTTON, description);
		}
		if (strcmp("Select", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_B, description);
		}
	}

	// Dirt Fox
	// Final Lap 1/2/3
	// Four Trax
	// Suzuka 8 Hours 2
	// Suzuka 8 Hours
	if ((parentrom && strcmp(parentrom, "finallap") == 0) ||
		(drvname && strcmp(drvname, "finallap") == 0) ||
		(parentrom && strcmp(parentrom, "finalap2") == 0) ||
		(drvname && strcmp(drvname, "finalap2") == 0) ||
		(parentrom && strcmp(parentrom, "finalap3") == 0) ||
		(drvname && strcmp(drvname, "finalap3") == 0) ||
		(parentrom && strcmp(parentrom, "fourtrax") == 0) ||
		(drvname && strcmp(drvname, "fourtrax") == 0) ||
		(parentrom && strcmp(parentrom, "suzuk8h2") == 0) ||
		(drvname && strcmp(drvname, "suzuk8h2") == 0) ||
		(parentrom && strcmp(parentrom, "suzuka8h") == 0) ||
		(drvname && strcmp(drvname, "suzuka8h") == 0) ||
		(parentrom && strcmp(parentrom, "dirtfoxj") == 0) ||
		(drvname && strcmp(drvname, "dirtfoxj") == 0)
	) {
		if (strcmp("Accelerator", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R2, RETRO_DEVICE_INDEX_ANALOG_BUTTON, description);
		}
		if (strcmp("Brake", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_L2, RETRO_DEVICE_INDEX_ANALOG_BUTTON, description);
		}
		if (strcmp("Gear Up", description) == 0 || strcmp("Gear", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R, description);
		}
		if (strcmp("Gear Down", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_L, description);
		}
	}

	// Ground Effects / Super Ground Effects
	// WEC Le Mans 24
	// Cisco Heat
	// Big Run
	// Grand Prix Star
	// F-1 Grand Prix Star II
	if ((parentrom && strcmp(parentrom, "groundfx") == 0) ||
		(drvname && strcmp(drvname, "groundfx") == 0) ||
		(parentrom && strcmp(parentrom, "wecleman") == 0) ||
		(drvname && strcmp(drvname, "wecleman") == 0) ||
		(parentrom && strcmp(parentrom, "hotchase") == 0) ||
		(drvname && strcmp(drvname, "hotchase") == 0) ||
		(parentrom && strcmp(parentrom, "cischeat") == 0) ||
		(drvname && strcmp(drvname, "cischeat") == 0) ||
		(parentrom && strcmp(parentrom, "bigrun") == 0) ||
		(drvname && strcmp(drvname, "bigrun") == 0) ||
		(parentrom && strcmp(parentrom, "f1gpstar") == 0) ||
		(drvname && strcmp(drvname, "f1gpstar") == 0) ||
		(parentrom && strcmp(parentrom, "f1gpstr2") == 0) ||
		(drvname && strcmp(drvname, "f1gpstr2") == 0)
	) {
		if (strcmp("Accelerator", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R2, RETRO_DEVICE_INDEX_ANALOG_BUTTON, description);
			// cischeat uses digital for accelerator
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R2, description);
		}
		if (strcmp("Brake", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_L2, description);
		}
		if (strcmp("Shift Hi", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R, description);
		}
		if (strcmp("Shift Lo", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_L, description);
		}
		if (strcmp("Gear Shift", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_B, description);
		}
		if (strcmp("Horn", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_A, description);
		}
	}

	// Buggy Challenge
	if ((parentrom && strcmp(parentrom, "buggychl") == 0) ||
		(drvname && strcmp(drvname, "buggychl") == 0)
	) {
		if (strcmp("Throttle", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R2, RETRO_DEVICE_INDEX_ANALOG_BUTTON, description);
		}
	}

	// Spy Hunter
	if ((parentrom && strcmp(parentrom, "spyhunt") == 0) ||
		(drvname && strcmp(drvname, "spyhunt") == 0)
	) {
		if (strcmp("Accelerator", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R2, RETRO_DEVICE_INDEX_ANALOG_BUTTON, description);
		}
		if (strcmp("Gear Shift", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R, description);
		}
		if (strcmp("Missiles", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_B, description);
		}
		if (strcmp("Oil Slick", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_A, description);
		}
		if (strcmp("Machine Gun", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_Y, description);
		}
		if (strcmp("Smoke Screen", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_X, description);
		}
	}

	// Hydra
	if ((parentrom && strcmp(parentrom, "hydra") == 0) ||
		(drvname && strcmp(drvname, "hydra") == 0)
	) {
		if (strcmp("Accelerator", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R2, RETRO_DEVICE_INDEX_ANALOG_BUTTON, description);
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

	// Hot Rod
	if ((parentrom && strcmp(parentrom, "hotrod") == 0) ||
		(drvname && strcmp(drvname, "hotrod") == 0)
	) {
		if (strcmp("Accelerator", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R2, RETRO_DEVICE_INDEX_ANALOG_BUTTON, description);
		}
	}

	// Wacko (map aim to right stick)
	if ((parentrom && strcmp(parentrom, "wacko") == 0) ||
		(drvname && strcmp(drvname, "wacko") == 0)
	) {
		if (strcmp("Up", description) == 0) {
			GameInpDigital2RetroInpAnalogRight(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_Y, JOY_NEG, "Aim Up / Down");
		}
		if (strcmp("Down", description) == 0) {
			GameInpDigital2RetroInpAnalogRight(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_Y, JOY_POS, "Aim Up / Down");
		}
		if (strcmp("Left", description) == 0) {
			GameInpDigital2RetroInpAnalogRight(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_X, JOY_NEG, "Aim Left / Right");
		}
		if (strcmp("Right", description) == 0) {
			GameInpDigital2RetroInpAnalogRight(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_X, JOY_POS, "Aim Left / Right");
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

	// Defender
	// Stargate
	// Robotron
	// Joust
	// Sinistar
	if ((parentrom && strcmp(parentrom, "defender") == 0) ||
	  (drvname && strcmp(drvname, "defender") == 0) ||
		(parentrom && strcmp(parentrom, "stargate") == 0) ||
		(drvname && strcmp(drvname, "stargate") == 0) ||
		(parentrom && strcmp(parentrom, "robotron") == 0) ||
		(drvname && strcmp(drvname, "robotron") == 0) ||
		(parentrom && strcmp(parentrom, "joust") == 0) ||
		(drvname && strcmp(drvname, "joust") == 0) ||
		(parentrom && strcmp(parentrom, "sinistar") == 0) ||
		(drvname && strcmp(drvname, "sinistar") == 0)
	) {
		if (strcmp("Auto Up / Manual Down", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, 0, RETRO_DEVICE_ID_JOYPAD_L3, description);
		}
		if (strcmp("High Score Reset", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, 0, RETRO_DEVICE_ID_JOYPAD_R3, description);
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
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_X, RETRO_DEVICE_INDEX_ANALOG_RIGHT, description);
		}
		if (strcmp("Aim Analog", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_Y, RETRO_DEVICE_INDEX_ANALOG_RIGHT, description);
		}
	}

	// Mortal Kombat 1, 2 & 3
	// snes-like mapping seems more convenient than original arcade mapping (with the block in the middle), even on arcade sticks
	if ((parentrom && strcmp(parentrom, "mk") == 0) ||
		(drvname && strcmp(drvname, "mk") == 0) ||
		(parentrom && strcmp(parentrom, "mk2") == 0) ||
		(drvname && strcmp(drvname, "mk2") == 0) ||
		(parentrom && strcmp(parentrom, "mk3") == 0) ||
		(drvname && strcmp(drvname, "mk3") == 0) ||
		(parentrom && strcmp(parentrom, "umk3") == 0) ||
		(drvname && strcmp(drvname, "umk3") == 0)
	) {
		if (strcmp("High Punch", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_Y, description);
		}
		if (strcmp("Low Punch", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_B, description);
		}
		if (strcmp("High Kick", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_X, description);
		}
		if (strcmp("Low Kick", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_A, description);
		}
		if (strcmp("Block", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, (nDeviceType[nPlayer] == RETROPAD_MODERN ? RETRO_DEVICE_ID_JOYPAD_R : RETRO_DEVICE_ID_JOYPAD_L), description);
		}
		// mk1 & mk2
		if (strcmp("Block 2", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, (nDeviceType[nPlayer] == RETROPAD_MODERN ? RETRO_DEVICE_ID_JOYPAD_R2 : RETRO_DEVICE_ID_JOYPAD_R), description);
		}
		// mk3
		if (strcmp("Run", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, (nDeviceType[nPlayer] == RETROPAD_MODERN ? RETRO_DEVICE_ID_JOYPAD_R2 : RETRO_DEVICE_ID_JOYPAD_R), description);
		}
	}

	// Xybots
	if ((parentrom && strcmp(parentrom, "xybots") == 0) ||
		(drvname && strcmp(drvname, "xybots") == 0)
	) {
		if (strcmp("Twist Left", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_L, description);
		}
		if (strcmp("Twist Right", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R, description);
		}
	}

	// Metal Hawk
	if ((parentrom && strcmp(parentrom, "metlhawk") == 0) ||
		(drvname && strcmp(drvname, "metlhawk") == 0)
	) {
		if (strcmp("Up/Down Axis", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_Y, RETRO_DEVICE_INDEX_ANALOG_RIGHT, description);
		}
	}

	// Two Tiger
	if ((parentrom && strcmp(parentrom, "twotiger") == 0) ||
		(drvname && strcmp(drvname, "twotiger") == 0)
	) {
		if (strcmp("& P2 Dogfight Start", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, 0, RETRO_DEVICE_ID_JOYPAD_R3, "P1 & P2 Dogfight Start");
		}
	}

	// CPS3's jojo & jojoba (DC/PSX layout)
	if ((parentrom && strcmp(parentrom, "jojo") == 0) ||
		(drvname && strcmp(drvname, "jojo") == 0) ||
		(parentrom && strcmp(parentrom, "jojoba") == 0) ||
		(drvname && strcmp(drvname, "jojoba") == 0)
	) {
		if (strcmp("Weak Attack", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_Y, description);
		}
		if (strcmp("Medium Attack", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_X, description);
		}
		if (strcmp("Strong Attack", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_A, description);
		}
		if (strcmp("Stand", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_B, description);
		}
		if (strcmp("All Attacks (Fake)", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R, description);
		}
	}

	// SVG (PSP/PS2 layout)
	if ((parentrom && strcmp(parentrom, "svg") == 0) ||
		(drvname && strcmp(drvname, "svg") == 0)
	) {
		if (strcmp("Button 1", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_Y, description);
		}
		if (strcmp("Button 2", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_X, description);
		}
		if (strcmp("Button 3", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_A, description);
		}
		if (strcmp("Button 4", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_B, description);
		}
	}

	// Rabbit (Saturn layout)
	if ((parentrom && strcmp(parentrom, "rabbit") == 0) ||
		(drvname && strcmp(drvname, "rabbit") == 0)
	) {
		if (strcmp("Button 1", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_Y, description);
		}
		if (strcmp("Button 2", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_X, description);
		}
		if (strcmp("Button 3", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_B, description);
		}
		if (strcmp("Button 4", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_A, description);
		}
	}

	// Power instinct (SNES/Genesis layout)
	if ((parentrom && strcmp(parentrom, "powerins") == 0) ||
		(drvname && strcmp(drvname, "powerins") == 0) ||
		(parentrom && strcmp(parentrom, "pwrinst2") == 0) ||
		(drvname && strcmp(drvname, "pwrinst2") == 0) ||
		(parentrom && strcmp(parentrom, "plegends") == 0) ||
		(drvname && strcmp(drvname, "plegends") == 0)
	) {
		if (strcmp("Button 1", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_Y, description);
		}
		if (strcmp("Button 2", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_B, description);
		}
		if (strcmp("Button 3", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_X, description);
		}
		if (strcmp("Button 4", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_A, description);
		}
	}

	// Avengers In Galactic Storm
	// Blood Warrior
	// Daraku Tenshi - The Fallen Angels
	// Dragon Master
	// Shogun Warriors
	// Toukidenshou - Angel Eyes
	if ((parentrom && strcmp(parentrom, "avengrgs") == 0) ||
		(drvname && strcmp(drvname, "avengrgs") == 0) ||
		(parentrom && strcmp(parentrom, "bloodwar") == 0) ||
		(drvname && strcmp(drvname, "bloodwar") == 0) ||
		(parentrom && strcmp(parentrom, "daraku") == 0) ||
		(drvname && strcmp(drvname, "daraku") == 0) ||
		(parentrom && strcmp(parentrom, "drgnmst") == 0) ||
		(drvname && strcmp(drvname, "drgnmst") == 0) ||
		(parentrom && strcmp(parentrom, "shogwarr") == 0) ||
		(drvname && strcmp(drvname, "shogwarr") == 0) ||
		(parentrom && strcmp(parentrom, "tkdensho") == 0) ||
		(drvname && strcmp(drvname, "tkdensho") == 0)
	) {
		if (strcmp("Button 1", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_Y, description);
		}
		if (strcmp("Button 2", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_X, description);
		}
		if (strcmp("Button 3", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_B, description);
		}
		if (strcmp("Button 4", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_A, description);
		}
	}

	// Caliber 50
	// Time Soldiers
	if ((parentrom && strcmp(parentrom, "calibr50") == 0) ||
		(drvname && strcmp(drvname, "calibr50") == 0) ||
		(parentrom && strcmp(parentrom, "timesold") == 0) ||
		(drvname && strcmp(drvname, "timesold") == 0)
	) {
		if (strcmp("Aim X", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_X, RETRO_DEVICE_INDEX_ANALOG_RIGHT, description);
		}
		if (strcmp("Aim Y", description) == 0) {
			GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_Y, RETRO_DEVICE_INDEX_ANALOG_RIGHT, description);
		}
	}

	if (bStreetFighterLayout) {
		if (strncmp("Buttons 3x Punch", description, 16) == 0)
			GameInpDigital2RetroInpKey(pgi, nPlayer, (nDeviceType[nPlayer] == RETROPAD_MODERN ? RETRO_DEVICE_ID_JOYPAD_L : RETRO_DEVICE_ID_JOYPAD_L2), description, RETRO_DEVICE_JOYPAD, GIT_MACRO_AUTO);
		if (strncmp("Buttons 3x Kick", description, 15) == 0)
			GameInpDigital2RetroInpKey(pgi, nPlayer, (nDeviceType[nPlayer] == RETROPAD_MODERN ? RETRO_DEVICE_ID_JOYPAD_L2 : RETRO_DEVICE_ID_JOYPAD_R2), description, RETRO_DEVICE_JOYPAD, GIT_MACRO_AUTO);
	}
	if (bIsNeogeoCartGame || (nGameType == RETRO_GAME_TYPE_NEOCD)) {
		if (strncmp("Buttons ABC", description, 11) == 0)
			GameInpDigital2RetroInpKey(pgi, nPlayer, (nDeviceType[nPlayer] == RETROPAD_MODERN ? RETRO_DEVICE_ID_JOYPAD_L2 : RETRO_DEVICE_ID_JOYPAD_R2), description, RETRO_DEVICE_JOYPAD, GIT_MACRO_AUTO);
		if (strncmp("Buttons BCD", description, 11) == 0)
			GameInpDigital2RetroInpKey(pgi, nPlayer, (nDeviceType[nPlayer] == RETROPAD_MODERN ? RETRO_DEVICE_ID_JOYPAD_L : RETRO_DEVICE_ID_JOYPAD_L2), description, RETRO_DEVICE_JOYPAD, GIT_MACRO_AUTO);
		if (strncmp("Buttons AB", description, 10) == 0)
			GameInpDigital2RetroInpKey(pgi, nPlayer, (nDeviceType[nPlayer] == RETROPAD_MODERN ? RETRO_DEVICE_ID_JOYPAD_R2 : RETRO_DEVICE_ID_JOYPAD_R), description, RETRO_DEVICE_JOYPAD, GIT_MACRO_AUTO);
		if (strncmp("Buttons CD", description, 10) == 0)
			GameInpDigital2RetroInpKey(pgi, nPlayer, (nDeviceType[nPlayer] == RETROPAD_MODERN ? RETRO_DEVICE_ID_JOYPAD_R : RETRO_DEVICE_ID_JOYPAD_L), description, RETRO_DEVICE_JOYPAD, GIT_MACRO_AUTO);
	}

	// Handle megadrive
	if ((nHardwareCode & HARDWARE_PUBLIC_MASK) == HARDWARE_SEGA_MEGADRIVE) {
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
	if ((nHardwareCode & HARDWARE_PUBLIC_MASK) == HARDWARE_MSX) {
		if (strcmp("Button 1", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_B, description);
		}
		if (strcmp("Button 2", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_A, description);
		}
		if (strcmp("Key STOP", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_START, description);
		}
		if (strcmp("Key F1", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_SELECT, description);
		}
		if (strcmp("Key F2", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_Y, description);
		}
		if (strcmp("Key F3", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_X, description);
		}
		if (strcmp("Key F4", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R, description);
		}
		if (strcmp("Key F5", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_L, description);
		}
		if (strcmp("Key F6", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R2, description);
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
	if ((nHardwareCode & HARDWARE_PUBLIC_MASK) == HARDWARE_COLECO) {
		if (strcmp("1", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_L, description);
		}
		if (strcmp("2", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R, description);
		}
		if (strcmp("3", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_L2, description);
		}
		if (strcmp("4", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R2, description);
		}
		if (strcmp("5", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_L3, description);
		}
		if (strcmp("6", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R3, description);
		}
		if (strcmp("7", description) == 0) {
			GameInpDigital2RetroInpAnalogRight(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_X, JOY_NEG, "7 / 8");
		}
		if (strcmp("8", description) == 0) {
			GameInpDigital2RetroInpAnalogRight(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_X, JOY_POS, "7 / 8");
		}
		if (strcmp("9", description) == 0) {
			GameInpDigital2RetroInpAnalogRight(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_Y, JOY_POS, "0 / 9");
		}
		if (strcmp("0", description) == 0) {
			GameInpDigital2RetroInpAnalogRight(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_Y, JOY_NEG, "0 / 9");
		}
		if (strcmp("*", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_SELECT, description);
		}
		if (strcmp("#", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_START, description);
		}
	}

	// channelf reorder buttons
	if ((nHardwareCode & HARDWARE_PUBLIC_MASK) == HARDWARE_CHANNELF) {
		if (strcmp("Push Down", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_B, description);
		}
		if (strcmp("Pull Up", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_X, description);
		}
		if (strcmp("Twist Right", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_A, description);
		}
		if (strcmp("Twist Left", description) == 0) {
			GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_Y, description);
		}
	}

	// Handle Twin stick games
	if ((strcmp("Up 2", description) == 0) ||
		(strcmp("Up (right)", description) == 0) ||
		(strcmp("Right Up", description) == 0) ||
		(strcmp("Right Stick Up", description) == 0) ||
		(strcmp("Rght Stick Up", description) == 0) ||
		(strcmp("Fire Up", description) == 0) ||
		(strcmp("up 2", szb) == 0)
	) {
		GameInpDigital2RetroInpAnalogRight(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_Y, JOY_NEG, "Up/Down (Right Stick)");
	}
	if ((strcmp("Down 2", description) == 0) ||
		(strcmp("Down (right)", description) == 0) ||
		(strcmp("Right Down", description) == 0) ||
		(strcmp("Right Stick Down", description) == 0) ||
		(strcmp("Rght Stick Down", description) == 0) ||
		(strcmp("Fire Down", description) == 0) ||
		(strcmp("down 2", szb) == 0)
	) {
		GameInpDigital2RetroInpAnalogRight(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_Y, JOY_POS, "Up/Down (Right Stick)");
	}
	if ((strcmp("Left 2", description) == 0) ||
		(strcmp("Left (right)", description) == 0) ||
		(strcmp("Right Left", description) == 0) ||
		(strcmp("Right Stick Left", description) == 0) ||
		(strcmp("Rght Stick Left", description) == 0) ||
		(strcmp("Fire Left", description) == 0) ||
		(strcmp("left 2", szb) == 0)
	) {
		GameInpDigital2RetroInpAnalogRight(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_X, JOY_NEG, "Left/Right (Right Stick)");
	}
	if ((strcmp("Right 2", description) == 0) ||
		(strcmp("Right (right)", description) == 0) ||
		(strcmp("Right Right", description) == 0) ||
		(strcmp("Right Stick Right", description) == 0) ||
		(strcmp("Rght Stick Right", description) == 0) ||
		(strcmp("Fire Right", description) == 0) ||
		(strcmp("right 2", szb) == 0)
	) {
		GameInpDigital2RetroInpAnalogRight(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_X, JOY_POS, "Left/Right (Right Stick)");
	}

	// map rotate left/right to L/R
	if (strcmp("rotate left", szb) == 0) {
		GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_L, description);
	}
	if (strcmp("rotate right", szb) == 0) {
		GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R, description);
	}

	// Don't map neogeo select button anywhere
	// See https://neo-source.com/index.php?topic=3490.0
	// 2019-07-03 : actually, map it to L3, it allows access to a menu in last blade training mode
	if (strncmp("select", szb, 6) == 0 && bIsNeogeoCartGame)
		GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_L3, description);

	// map VS unisystem select button to L3
	if (strncmp("select", szb, 6) == 0 && (nHardwareCode & HARDWARE_PUBLIC_MASK) == HARDWARE_NVS)
		GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_L3, description);

	return 0;
}

// Meant to handle most standard layouts
static INT32 GameInpStandardOne(struct GameInp* pgi, INT32 nPlayer, char* szb, char *szn, char *description)
{
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
		GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_X, RETRO_DEVICE_INDEX_ANALOG_LEFT, description);
	if (strncmp("y-axis", szb, 6) == 0)
		GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_Y, RETRO_DEVICE_INDEX_ANALOG_LEFT, description);
	if (strncmp("z-axis", szb, 6) == 0)
		GameInpAnalog2RetroInpAnalog(pgi, nPlayer, RETRO_DEVICE_ID_ANALOG_Y, RETRO_DEVICE_INDEX_ANALOG_RIGHT, description);

	if (strncmp("fire ", szb, 5) == 0) {
		char *szf = szb + 5;
		INT32 nButton = strtol(szf, NULL, 0);
		// "Modern" neogeo stick and gamepad are actually like this, see pictures of arcade stick pro and neogeo mini gamepad
		if ((bIsNeogeoCartGame || (nGameType == RETRO_GAME_TYPE_NEOCD)) && nDeviceType[nPlayer] == RETROPAD_MODERN) {
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
					case 9:
						GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_R3, description);
						break;
					case 10:
						GameInpDigital2RetroInpKey(pgi, nPlayer, RETRO_DEVICE_ID_JOYPAD_L3, description);
						break;
					default:
						log_cb(RETRO_LOG_ERROR, "We don't know where to map \"%s\" for player %d, every button seems to be already in use, but maybe the analog sticks aren't : the game will probably be unplayable, but maybe you can submit a good mapping using also the analog sticks at https://github.com/libretro/FBNeo/issues.\n", description, nPlayer+1);
						break;
				}
			}
		}
	}

	// assign per player mahjong controls (require 1 keyboard per player, does the frontend actually support this ?)
	// note : some games need to be fixed for this to work (player number must be set in szName)
	if (nMahjongKeyboards > 0)
	{
		int mahjongKeyboardPort = nMaxPlayers+nPlayer;
		if (strcmp("mah a", szb) == 0)
			GameInpDigital2RetroInpKey(pgi, mahjongKeyboardPort, RETROK_a, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("mah b", szb) == 0)
			GameInpDigital2RetroInpKey(pgi, mahjongKeyboardPort, RETROK_b, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("mah c", szb) == 0)
			GameInpDigital2RetroInpKey(pgi, mahjongKeyboardPort, RETROK_c, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("mah d", szb) == 0)
			GameInpDigital2RetroInpKey(pgi, mahjongKeyboardPort, RETROK_d, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("mah e", szb) == 0)
			GameInpDigital2RetroInpKey(pgi, mahjongKeyboardPort, RETROK_e, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("mah f", szb) == 0)
			GameInpDigital2RetroInpKey(pgi, mahjongKeyboardPort, RETROK_f, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("mah g", szb) == 0)
			GameInpDigital2RetroInpKey(pgi, mahjongKeyboardPort, RETROK_g, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("mah h", szb) == 0)
			GameInpDigital2RetroInpKey(pgi, mahjongKeyboardPort, RETROK_h, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("mah i", szb) == 0)
			GameInpDigital2RetroInpKey(pgi, mahjongKeyboardPort, RETROK_i, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("mah j", szb) == 0)
			GameInpDigital2RetroInpKey(pgi, mahjongKeyboardPort, RETROK_j, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("mah k", szb) == 0)
			GameInpDigital2RetroInpKey(pgi, mahjongKeyboardPort, RETROK_k, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("mah l", szb) == 0)
			GameInpDigital2RetroInpKey(pgi, mahjongKeyboardPort, RETROK_l, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("mah m", szb) == 0)
			GameInpDigital2RetroInpKey(pgi, mahjongKeyboardPort, RETROK_m, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("mah n", szb) == 0)
			GameInpDigital2RetroInpKey(pgi, mahjongKeyboardPort, RETROK_n, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("mah kan", szb) == 0)
			GameInpDigital2RetroInpKey(pgi, mahjongKeyboardPort, RETROK_LCTRL, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("mah pon", szb) == 0)
			GameInpDigital2RetroInpKey(pgi, mahjongKeyboardPort, RETROK_LALT, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("mah chi", szb) == 0)
			GameInpDigital2RetroInpKey(pgi, mahjongKeyboardPort, RETROK_SPACE, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("mah reach", szb) == 0)
			GameInpDigital2RetroInpKey(pgi, mahjongKeyboardPort, RETROK_LSHIFT, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("mah ron", szb) == 0)
			GameInpDigital2RetroInpKey(pgi, mahjongKeyboardPort, RETROK_z, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("mah ff", szb) == 0)
			GameInpDigital2RetroInpKey(pgi, mahjongKeyboardPort, RETROK_y, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("mah lc", szb) == 0)
			GameInpDigital2RetroInpKey(pgi, mahjongKeyboardPort, RETROK_RALT, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("mah bet", szb) == 0)
			GameInpDigital2RetroInpKey(pgi, mahjongKeyboardPort, RETROK_2, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("mah score", szb) == 0)
			GameInpDigital2RetroInpKey(pgi, mahjongKeyboardPort, RETROK_RCTRL, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("mah big", szb) == 0)
			GameInpDigital2RetroInpKey(pgi, mahjongKeyboardPort, RETROK_RETURN, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("mah wup", szb) == 0)
			GameInpDigital2RetroInpKey(pgi, mahjongKeyboardPort, RETROK_RSHIFT, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("mah small", szb) == 0)
			GameInpDigital2RetroInpKey(pgi, mahjongKeyboardPort, RETROK_BACKSPACE, szn, RETRO_DEVICE_KEYBOARD);
	}

	return 0;
}

// Meant to handle other pgis where a nPlayer can't be computed
static INT32 GameInpOtherOne(struct GameInp* pgi, char* szi, char *szn)
{
	const char * parentrom	= BurnDrvGetTextA(DRV_PARENT);
	const char * drvname	= BurnDrvGetTextA(DRV_NAME);
	int nHardwareCode = BurnDrvGetHardwareCode();

	// Handle Spectrum
	if ((nHardwareCode & HARDWARE_PUBLIC_MASK) == HARDWARE_SPECTRUM) {
		if (strcmp("keyb_A", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_a, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_B", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_b, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_C", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_c, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_D", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_d, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_E", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_e, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_F", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_f, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_G", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_g, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_H", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_h, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_I", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_i, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_J", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_j, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_K", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_k, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_L", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_l, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_M", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_m, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_N", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_n, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_O", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_o, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_P", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_p, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_Q", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_q, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_R", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_r, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_S", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_s, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_T", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_t, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_U", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_u, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_V", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_v, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_W", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_w, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_X", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_x, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_Y", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_y, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_Z", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_z, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_0", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_0, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_1", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_1, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_2", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_2, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_3", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_3, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_4", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_4, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_5", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_5, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_6", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_6, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_7", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_7, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_8", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_8, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_9", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_9, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_enter", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_RETURN, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_space", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_SPACE, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_left_shift", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_LSHIFT, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_right_shift", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_RSHIFT, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_insert", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_INSERT, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_caps_lock", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_CAPSLOCK, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_home", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_HOME, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_end", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_END, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_backspace", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_BACKSPACE, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_left_alt", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_LALT, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_left", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_LEFT, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_right", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_RIGHT, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_up", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_UP, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_down", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_DOWN, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_pause", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_PAUSE, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_left_ctrl", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_LCTRL, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_apost", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_QUOTEDBL, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_colon", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_COLON, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_stop", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_PERIOD, szn, RETRO_DEVICE_KEYBOARD);
		if (strcmp("keyb_comma", szi) == 0)
			GameInpDigital2RetroInpKey(pgi, 2, RETROK_COMMA, szn, RETRO_DEVICE_KEYBOARD);
	}

	// channelf console buttons
	if ((nHardwareCode & HARDWARE_PUBLIC_MASK) == HARDWARE_CHANNELF) {
		if (strcmp("Time (1)", szn) == 0) {
			GameInpDigital2RetroInpKey(pgi, 0, RETRO_DEVICE_ID_JOYPAD_R, szn);
		}
		if (strcmp("Hold (2)", szn) == 0) {
			GameInpDigital2RetroInpKey(pgi, 0, RETRO_DEVICE_ID_JOYPAD_L, szn);
		}
		if (strcmp("Mode (3)", szn) == 0) {
			GameInpDigital2RetroInpKey(pgi, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, szn);
		}
		if (strcmp("Start (4)", szn) == 0) {
			GameInpDigital2RetroInpKey(pgi, 0, RETRO_DEVICE_ID_JOYPAD_START, szn);
		}
	}

	// FDS disk buttons
	if ((nHardwareCode & HARDWARE_PUBLIC_MASK) == HARDWARE_FDS) {
		if (strcmp("Swap Disk Side", szn) == 0) {
			GameInpDigital2RetroInpKey(pgi, 0, RETRO_DEVICE_ID_JOYPAD_R3, szn);
		}
		if (strcmp("Eject/Insert Disk", szn) == 0) {
			GameInpDigital2RetroInpKey(pgi, 0, RETRO_DEVICE_ID_JOYPAD_L3, szn);
		}
	}

	// SG-1000 pause button
	if ((nHardwareCode & HARDWARE_PUBLIC_MASK) == HARDWARE_SEGA_SG1000) {
		if (strcmp("Console Pause", szn) == 0) {
			GameInpDigital2RetroInpKey(pgi, 0, RETRO_DEVICE_ID_JOYPAD_START, szn);
		}
	}

	// Service buttons needed for service menu navigation on qix hardware
	if ((parentrom && strcmp(parentrom, "qix") == 0) ||
		(drvname && strcmp(drvname, "qix") == 0) ||
		(parentrom && strcmp(parentrom, "complexx") == 0) ||
		(drvname && strcmp(drvname, "complexx") == 0) ||
		(parentrom && strcmp(parentrom, "kram") == 0) ||
		(drvname && strcmp(drvname, "kram") == 0) ||
		(parentrom && strcmp(parentrom, "elecyoyo") == 0) ||
		(drvname && strcmp(drvname, "elecyoyo") == 0) ||
		(parentrom && strcmp(parentrom, "sdungeon") == 0) ||
		(drvname && strcmp(drvname, "sdungeon") == 0) ||
		(parentrom && strcmp(parentrom, "zookeep") == 0) ||
		(drvname && strcmp(drvname, "zookeep") == 0)
	) {
		if (strcmp("Test Advance", szn) == 0) {
			GameInpDigital2RetroInpKey(pgi, 0, RETRO_DEVICE_ID_JOYPAD_R2, szn);
		}
		if (strcmp("Test Next Line", szn) == 0) {
			GameInpDigital2RetroInpKey(pgi, 0, RETRO_DEVICE_ID_JOYPAD_L2, szn);
		}
		if (strcmp("Test Slew Up", szn) == 0) {
			GameInpDigital2RetroInpKey(pgi, 0, RETRO_DEVICE_ID_JOYPAD_R3, szn);
		}
		if (strcmp("Test Slew Down", szn) == 0) {
			GameInpDigital2RetroInpKey(pgi, 0, RETRO_DEVICE_ID_JOYPAD_L3, szn);
		}
	}

	// non-single board versions of rdft2 require this button to unlock all ships
	// see https://gamefaqs.gamespot.com/arcade/564337-raiden-fighters-2-operation-hell-dive/cheats
	if ((drvname && strcmp(drvname, "rdft2") == 0) ||
		(drvname && strcmp(drvname, "rdft2u") == 0) ||
		(drvname && strcmp(drvname, "rdft2j") == 0) ||
		(drvname && strcmp(drvname, "rdft2ja") == 0) ||
		(drvname && strcmp(drvname, "rdft2jb") == 0) ||
		(drvname && strcmp(drvname, "rdft2jc") == 0) ||
		(drvname && strcmp(drvname, "rdft2it") == 0) ||
		(drvname && strcmp(drvname, "rdft2a") == 0) ||
		(drvname && strcmp(drvname, "rdft2aa") == 0) ||
		(drvname && strcmp(drvname, "rdft2t") == 0) ||
		(drvname && strcmp(drvname, "rdft2s") == 0)
	) {
		if (strcmp("Service", szn) == 0) {
			GameInpDigital2RetroInpKey(pgi, 0, RETRO_DEVICE_ID_JOYPAD_R3, szn);
		}
	}

	// Sega system 24/32's "Service 1" is required to navigate in service menu
	if ((nHardwareCode & HARDWARE_PUBLIC_MASK) == HARDWARE_SEGA_SYSTEM24 || (nHardwareCode & HARDWARE_PUBLIC_MASK) == HARDWARE_SEGA_SYSTEM32) {
		if (strcmp("Service 1", szn) == 0) {
			GameInpDigital2RetroInpKey(pgi, 0, RETRO_DEVICE_ID_JOYPAD_R3, szn);
		}
	}

	// Some cv1k games require this for special mode
	if ((nHardwareCode & HARDWARE_PUBLIC_MASK) == HARDWARE_CAVE_CV1000) {
		if (strcmp("Service", szn) == 0) {
			GameInpDigital2RetroInpKey(pgi, 0, RETRO_DEVICE_ID_JOYPAD_R3, szn);
		}
	}

	// Hidden Catch 3 requires the 2 service buttons to navigate in service menu
	if ((drvname && strcmp(drvname, "hidctch3") == 0)) {
		if (strcmp("Service 1", szn) == 0) {
			GameInpDigital2RetroInpKey(pgi, 0, RETRO_DEVICE_ID_JOYPAD_R3, szn);
		}
		if (strcmp("Service 2", szn) == 0) {
			GameInpDigital2RetroInpKey(pgi, 0, RETRO_DEVICE_ID_JOYPAD_L3, szn);
		}
	}

	// Store the pgi that controls the reset input
	if (strcmp(szi, "reset") == 0) {
		pgi->nInput = GIT_SPECIAL_SWITCH;
		if (!bInputInitialized)
			pgi->Input.Switch.nCode = SWITCH_NCODE_RESET;
		bButtonMapped = true;
		pgi_reset = pgi;
	}

	// Store the pgi that controls the diagnostic input
	if (strcmp(szi, "diag") == 0 || strcmp(szi, "diagnostics") == 0) {
		pgi->nInput = GIT_SPECIAL_SWITCH;
		if (!bInputInitialized)
			pgi->Input.Switch.nCode = SWITCH_NCODE_DIAG;
		bButtonMapped = true;
		pgi_diag = pgi;
	}

	// If no unique nCode was set yet, make sure we set one
	if(!bButtonMapped && !bInputInitialized) {
		if (pgi->nInput == GIT_SWITCH) {
			pgi->Input.Switch.nCode = (UINT16)(nSwitchCode++);
			HandleMessage(RETRO_LOG_DEBUG, "[FBNeo] nSwitchCode 0x%02X : %s (not mapped)\n", pgi->Input.Switch.nCode, szn);
		}
		if (pgi->nInput == GIT_MACRO_AUTO) {
			pgi->Macro.Switch.nCode = (UINT16)(nSwitchCode++);
			HandleMessage(RETRO_LOG_DEBUG, "[FBNeo] nSwitchCode 0x%02X : %s (not mapped)\n", pgi->Macro.Switch.nCode, szn);
		}
	}

	return 0;
}

// Handle mapping of an input
static INT32 GameInpAutoOne(struct GameInp* pgi, char* szi, char *szn)
{
	bool bPlayerInInfo = (toupper(szi[0]) == 'P' && szi[1] >= '1' && szi[1] <= (MAX_PLAYERS+'0')); // Because some of the older drivers don't use the standard input naming.
	bool bPlayerInName = (szn[0] == 'P' && szn[1] >= '1' && szn[1] <= (MAX_PLAYERS+'0'));

	bButtonMapped = false;

	if (bPlayerInInfo || bPlayerInName) {
		INT32 nPlayer = -1;

		if (bPlayerInName)
			nPlayer = szn[1] - '1';
		if (bPlayerInInfo && nPlayer == -1)
			nPlayer = szi[1] - '1';

		int offset_szb = 3;
		if (strlen(szi) > 6 && strncmp("mouse ", szi, 6) == 0)
			offset_szb = 6;
		if (strlen(szi) > 4 && strncmp("mah ", szi, 4) == 0)
			offset_szb = 0;
		char* szb = szi + offset_szb;

		// "P1 XXX" - try to exclude the "P1 " from the szName
		INT32 offset_player_x = 0;
		if (strlen(szn) > 3 && szn[0] == 'P' && szn[2] == ' ')
			offset_player_x = 3;
		char* description = szn + offset_player_x;

		GameInpSpecialOne(pgi, nPlayer, szb, szn, description);
		if(bButtonMapped) return 0;

		GameInpStandardOne(pgi, nPlayer, szb, szn, description);
		if(bButtonMapped) return 0;
	}

	GameInpOtherOne(pgi, szi, szn);
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

	// Fill in macros still undefined
	for (i = 0; i < nMacroCount; i++, pgi++) {
		GameInpAutoOne(pgi, pgi->Macro.szName, pgi->Macro.szName);
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
		// Get the extra info about the input
		bii.szInfo = NULL;
		BurnDrvGetInputInfo(&bii, i);
		if (bii.szName == NULL) {
			bii.szName = "";
		}

		// Store the pgis for Neo-Geo Debug Dips
		if (strcmp(bii.szName, "Debug Dip 1") == 0) {
			pgi_debug_dip_1 = pgi;
		}
		if (strcmp(bii.szName, "Debug Dip 2") == 0) {
			pgi_debug_dip_2 = pgi;
		}

		if (pgi->nInput) {											// Already defined - leave it alone
			continue;
		}
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

	// Fill in macros still undefined
	for (i = 0; i < nMacroCount; i++, pgi++) {
		GameInpAutoOne(pgi, pgi->Macro.szName, pgi->Macro.szName);
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
			if (input_cb_wrapper(0, RETRO_DEVICE_JOYPAD, 0, diag_input[combo_idx]) == false)
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

void SetDefaultDeviceTypes()
{
	int nHardwareCode = BurnDrvGetHardwareCode();

	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		// Retroarch is ignoring what i want, so let's force valid values
		if ((nHardwareCode & HARDWARE_PUBLIC_MASK) == HARDWARE_SPECTRUM || (nHardwareCode & HARDWARE_PUBLIC_MASK) == HARDWARE_MSX)
		{
			switch(i)
			{
				case 0:
				case 1:
					nDeviceType[i] = RETRO_DEVICE_JOYPAD;
					break;
				case 2:
					nDeviceType[i] = RETRO_DEVICE_KEYBOARD;
					break;
			}
		}
		else
		{
			nDeviceType[i] = RETROPAD_CLASSIC;
		}
	}
}

void SetControllerInfo()
{
	int nHardwareCode = BurnDrvGetHardwareCode();

	if ((nHardwareCode & HARDWARE_PUBLIC_MASK) == HARDWARE_SPECTRUM || (nHardwareCode & HARDWARE_PUBLIC_MASK) == HARDWARE_MSX) {
		// Let's have some custom spectrum device handling, so that we can use the keyboard remapping
		static const struct retro_controller_description joystick_description[] = {
			{ "Joystick", RETRO_DEVICE_JOYPAD }
		};

		static const struct retro_controller_description keyboard_description[] = {
			{ "Keyboard", RETRO_DEVICE_KEYBOARD }
		};

		static const struct retro_controller_info controller_infos[4] = {
			{ joystick_description, 1 },
			{ joystick_description, 1 },
			{ keyboard_description, 1 },
			{ NULL, 0 }
		};

		// So the number of controllers is fixed to 3 for zx spectrum
		nMaxControllers = 3;

		environ_cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)controller_infos);
	} else {
		// For anything else, let's use standard device handling for now
		static const struct retro_controller_description controller_description[] = {
			{ "Classic", RETROPAD_CLASSIC },
			{ "Modern", RETROPAD_MODERN },
			{ "Mouse (ball only)", RETROMOUSE_BALL },
			{ "Mouse (full)", RETROMOUSE_FULL },
			{ "Pointer", RETRO_DEVICE_POINTER },
			{ "Lightgun", RETRO_DEVICE_LIGHTGUN }
		};

		// kludge for nes (some 1p game want to use p2 controls)
		if (nMaxPlayers < 2 && (nHardwareCode & HARDWARE_PUBLIC_MASK) == HARDWARE_NES)
			nMaxPlayers = 2;

		// Prepare enough controllers for everything
		nMaxControllers = nMaxPlayers + nMahjongKeyboards;

		struct retro_controller_info *controller_infos = (struct retro_controller_info*)calloc(nMaxControllers+1, sizeof(struct retro_controller_info));

		for (int i = 0; i < nMaxPlayers; i++)
		{
			controller_infos[i].types = controller_description;
			controller_infos[i].num_types = sizeof(controller_description) / sizeof(controller_description[0]);
		}

		if (nMahjongKeyboards > 0)
		{
			// Assign a keyboard for each player
			static const struct retro_controller_description keyboard_description[] = {
				{ "Keyboard", RETRO_DEVICE_KEYBOARD }
			};
			for (int i = nMaxPlayers; i < nMaxControllers; i++)
			{
				controller_infos[i].types = keyboard_description;
				controller_infos[i].num_types = 1;
			}
		}

		controller_infos[nMaxPlayers].types = NULL;
		controller_infos[nMaxPlayers].num_types = 0;

		environ_cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, controller_infos);
		free(controller_infos);
	}
}

static void SetFakeInputDescriptors()
{
	// WIP : if left analog mapped but not dpad, or reverse, do something to dual map
	for (int i = 0; i < MAX_PLAYERS; i++) {
		// dual mapping for those devices doesn't make much sense
		if (nDeviceType[i] == RETROMOUSE_BALL
		 || nDeviceType[i] == RETROMOUSE_FULL
		 || nDeviceType[i] == RETRO_DEVICE_POINTER
		) {
			continue;
		}
		if (pDirections[i][PGI_ANALOG_X] != NULL && pDirections[i][PGI_LEFT] == NULL && pDirections[i][PGI_RIGHT] == NULL)
		{
			retro_input_descriptor descriptor;
			std::string description;

			descriptor.id = (nDeviceType[i] == RETRO_DEVICE_LIGHTGUN ? RETRO_DEVICE_ID_LIGHTGUN_DPAD_LEFT : RETRO_DEVICE_ID_JOYPAD_LEFT);
			descriptor.port = i;
			descriptor.device = (nDeviceType[i] == RETRO_DEVICE_LIGHTGUN ? RETRO_DEVICE_LIGHTGUN : RETRO_DEVICE_JOYPAD);
			descriptor.index = 0;
			description = SSTR( pDirections[i][PGI_ANALOG_X] << " (Fake Digital Left)" );
			descriptor.description = strdup(description.c_str());
			normal_input_descriptors.push_back(descriptor);

			descriptor.id = (nDeviceType[i] == RETRO_DEVICE_LIGHTGUN ? RETRO_DEVICE_ID_LIGHTGUN_DPAD_RIGHT : RETRO_DEVICE_ID_JOYPAD_RIGHT);
			descriptor.port = i;
			descriptor.device = (nDeviceType[i] == RETRO_DEVICE_LIGHTGUN ? RETRO_DEVICE_LIGHTGUN : RETRO_DEVICE_JOYPAD);
			descriptor.index = 0;
			description = SSTR( pDirections[i][PGI_ANALOG_X] << " (Fake Digital Right)" );
			descriptor.description = strdup(description.c_str());
			normal_input_descriptors.push_back(descriptor);
		}
		if (pDirections[i][PGI_ANALOG_Y] != NULL && pDirections[i][PGI_UP] == NULL && pDirections[i][PGI_DOWN] == NULL)
		{
			retro_input_descriptor descriptor;
			std::string description;

			descriptor.id = (nDeviceType[i] == RETRO_DEVICE_LIGHTGUN ? RETRO_DEVICE_ID_LIGHTGUN_DPAD_UP : RETRO_DEVICE_ID_JOYPAD_UP);
			descriptor.port = i;
			descriptor.device = (nDeviceType[i] == RETRO_DEVICE_LIGHTGUN ? RETRO_DEVICE_LIGHTGUN : RETRO_DEVICE_JOYPAD);
			descriptor.index = 0;
			description = SSTR( pDirections[i][PGI_ANALOG_Y] << " (Fake Digital Up)" );
			descriptor.description = strdup(description.c_str());
			normal_input_descriptors.push_back(descriptor);

			descriptor.id = (nDeviceType[i] == RETRO_DEVICE_LIGHTGUN ? RETRO_DEVICE_ID_LIGHTGUN_DPAD_DOWN : RETRO_DEVICE_ID_JOYPAD_DOWN);
			descriptor.port = i;
			descriptor.device = (nDeviceType[i] == RETRO_DEVICE_LIGHTGUN ? RETRO_DEVICE_LIGHTGUN : RETRO_DEVICE_JOYPAD);
			descriptor.index = 0;
			description = SSTR( pDirections[i][PGI_ANALOG_Y] << " (Fake Digital Down)" );
			descriptor.description = strdup(description.c_str());
			normal_input_descriptors.push_back(descriptor);
		}
		if (pDirections[i][PGI_ANALOG_X] == NULL && pDirections[i][PGI_LEFT] != NULL && pDirections[i][PGI_RIGHT] != NULL)
		{
			retro_input_descriptor descriptor;

			descriptor.port = i;
			descriptor.device = RETRO_DEVICE_ANALOG;
			descriptor.index = RETRO_DEVICE_INDEX_ANALOG_LEFT;
			descriptor.id = RETRO_DEVICE_ID_ANALOG_X;
			descriptor.description = "Left/Right (Fake Left Stick)";
			normal_input_descriptors.push_back(descriptor);
		}
		if (pDirections[i][PGI_ANALOG_Y] == NULL && pDirections[i][PGI_UP] != NULL && pDirections[i][PGI_DOWN] != NULL)
		{
			retro_input_descriptor descriptor;

			descriptor.port = i;
			descriptor.device = RETRO_DEVICE_ANALOG;
			descriptor.index = RETRO_DEVICE_INDEX_ANALOG_LEFT;
			descriptor.id = RETRO_DEVICE_ID_ANALOG_Y;
			descriptor.description = "Up/Down (Fake Left Stick)";
			normal_input_descriptors.push_back(descriptor);
		}
	}
}

// Set the input descriptors
static void SetInputDescriptors()
{
	SetFakeInputDescriptors();

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

void (*cBurnerKeyCallback)(UINT8 code, UINT8 KeyType, UINT8 down) = NULL;

static void BurnerHandlerKeyCallback()
{
	UINT8 KeyType = 0;
	INT32 i = 0;

	// TODO : complete this list
	INT32 keyMatrix[][2] = {
		{'0', RETROK_0}, {'1', RETROK_1}, {'2', RETROK_2}, {'3', RETROK_3}, {'4', RETROK_4},
		{'5', RETROK_5}, {'6', RETROK_6}, {'7', RETROK_7}, {'8', RETROK_8}, {'9', RETROK_9},
		{'a', RETROK_a}, {'b', RETROK_b}, {'c', RETROK_c}, {'d', RETROK_d}, {'e', RETROK_e},
		{'f', RETROK_f}, {'g', RETROK_g}, {'h', RETROK_h}, {'i', RETROK_i}, {'j', RETROK_j},
		{'k', RETROK_k}, {'l', RETROK_l}, {'m', RETROK_m}, {'n', RETROK_n}, {'o', RETROK_o},
		{'p', RETROK_p}, {'q', RETROK_q}, {'r', RETROK_r}, {'s', RETROK_s}, {'t', RETROK_t},
		{'u', RETROK_u}, {'v', RETROK_v}, {'w', RETROK_w}, {'x', RETROK_x}, {'y', RETROK_y},
		{'z', RETROK_z},
		{0x0d, RETROK_RETURN}, {0x1b, RETROK_ESCAPE}, {' ', RETROK_SPACE},
		{0xf1, RETROK_F1}, {0xf2, RETROK_F2}, {0xf3, RETROK_F3},
		{0xf4, RETROK_F4}, {0xf5, RETROK_F5}, {0xf6, RETROK_F6},
		{'\0', 0}
	};

	// Check if shift is pressed
	if(input_cb_wrapper(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_LSHIFT) == 1 || input_cb_wrapper(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_RSHIFT) == 1)
		KeyType = 0xf0|0;
	else
		KeyType = 0;

	// Send inputs
	while (keyMatrix[i][0] != '\0') {
		if(input_cb_wrapper(2, RETRO_DEVICE_KEYBOARD, 0, keyMatrix[i][1]) == 1)
			cBurnerKeyCallback(keyMatrix[i][0], KeyType, 1);
		else
			cBurnerKeyCallback(keyMatrix[i][0], KeyType, 0);
		i++;
	}
}

static void RefreshControllers()
{
	if (bControllersNeedRefresh)
	{
		GameInpReassign();
		SetInputDescriptors();
		RefreshLightgunCrosshair();
		bControllersNeedRefresh = false;
		bControllersSetOnce = true;
	}
}

#ifdef FBNEO_DEBUG
void debuginputs()
{
	// keys debug
	struct GameInp *pgi2zz = GameInp;
 
	for (int i = 0; i < nGameInpCount; i++, pgi2zz++) {
 
		if (pgi2zz->Input.pVal == NULL) {
			continue;
		}
 
		struct BurnInputInfo bii;
		memset(&bii, 0, sizeof(bii));
		BurnDrvGetInputInfo(&bii, i);
 
		bprintf(0, _T("name %S  val %x \n"), bii.szName, *(pgi2zz->Input.pVal));
	}
}
#endif

void InputMake(void)
{
	// make sure controllers are properly set before polling
	RefreshControllers();

	for (int i = 0; i < MAX_PLAYERS; i++)
		nLibretroInputBitmask[i] = -1;

	poll_cb();

	if (PollDiagInput())
		return;

	if (cBurnerKeyCallback)
		BurnerHandlerKeyCallback();

	struct GameInp* pgi;
	UINT32 i;

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
					*(pgi->Input.pShortVal) = pgi->Input.nVal;
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
			case GIT_SPECIAL_SWITCH:
				// used for special inputs like diag and reset, we are not "mapping" those,
				// so we just want their state to be reset at every frame, nothing else
				pgi->Input.nVal = 0;
				*(pgi->Input.pVal) = pgi->Input.nVal;
				break;
			case GIT_MOUSEAXIS:						// Mouse axis
				pgi->Input.nVal = (UINT16)(CinpMouseAxis(pgi->Input.MouseAxis.nMouse, pgi->Input.MouseAxis.nAxis) * nAnalogSpeed);
				*(pgi->Input.pShortVal) = pgi->Input.nVal;
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
				*(pgi->Input.pShortVal) = pgi->Input.nVal;

				break;
			}
		}
	}

	for (i = 0; i < nMacroCount; i++, pgi++) {
		if (CinpState(pgi->Macro.Switch.nCode)) {
			for (INT32 j = 0; j < 4; j++) {
				if (pgi->Macro.pVal[j]) {
					*(pgi->Macro.pVal[j]) = pgi->Macro.nVal[j];
				}
			}
		}
	}
}

void RefreshLightgunCrosshair()
{
	if ((BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_NES)
		bBurnGunHide[0] = (nLightgunCrosshairEmulation == 0 && nDeviceType[1] == RETRO_DEVICE_LIGHTGUN) || (nLightgunCrosshairEmulation == 1);
	else
		for (int i = 0; i < MAX_GUNS; i++)
			bBurnGunHide[i] = (nLightgunCrosshairEmulation == 0 && nDeviceType[i] == RETRO_DEVICE_LIGHTGUN) || (nLightgunCrosshairEmulation == 1);
}

void InputInit()
{
	if (environ_cb(RETRO_ENVIRONMENT_GET_INPUT_BITMASKS, NULL))
		bLibretroSupportsBitmasks = true;

	// make sure everything is clean before processing this games's inputs
	nSwitchCode = 0;
	nAxisNum = 0;
	for (int i = 0; i < MAX_KEYBINDS; i++)
		sKeyBinds[i] = KeyBind();
	for (int i = 0; i < MAX_PLAYERS*MAX_AXISES; i++)
		sAxiBinds[i] = AxiBind();
	for (int i = 0; i < MAX_PLAYERS; i++)
		for (int j = 0; j < 6; j++)
			pDirections[i][j] = NULL;
	for (int i = 0; i < MAX_PLAYERS; i++)
		for (int j = 0; j < 2; j++)
			for (int k = 0; k < 2; k++)
				bAnalogRightMappingDone[i][j][k] = false;
	pgi_reset = NULL;
	pgi_diag = NULL;
	pgi_debug_dip_1 = NULL;
	pgi_debug_dip_2 = NULL;
	normal_input_descriptors.clear();

	GameInpInit();
	GameInpDefault();

	bInputInitialized = true;
}

void InputExit()
{
	if (GameInp) {
		free(GameInp);
		GameInp = NULL;
	}

	bInputInitialized = false;
	bLibretroSupportsBitmasks = false;
}

void retro_set_input_state(retro_input_state_t cb) { input_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { poll_cb = cb; }
void retro_set_controller_port_device(unsigned port, unsigned device)
{
	// Retroarch is ignoring what i want, so let's force valid values
	if (nBurnDrvActive != ~0U)
	{
		int nHardwareCode = BurnDrvGetHardwareCode();
		if ((nHardwareCode & HARDWARE_PUBLIC_MASK) == HARDWARE_SPECTRUM || (nHardwareCode & HARDWARE_PUBLIC_MASK) == HARDWARE_MSX)
		{
			switch(port)
			{
				case 0:
				case 1:
					if(device != RETRO_DEVICE_JOYPAD)
					{
						device = RETRO_DEVICE_JOYPAD;
						HandleMessage(RETRO_LOG_INFO, "[FBNeo] Unknown device type for port %d, forcing \"Joystick\" instead\n", port);
					}
					break;
				case 2:
					if(device != RETRO_DEVICE_KEYBOARD)
					{
						device = RETRO_DEVICE_KEYBOARD;
						HandleMessage(RETRO_LOG_INFO, "[FBNeo] Unknown device type for port %d, forcing \"Keyboard\" instead\n", port);
					}
					break;
			}
		}
		else
		{
			if (device != RETROPAD_CLASSIC &&
				device != RETROPAD_MODERN &&
				device != RETROMOUSE_BALL &&
				device != RETROMOUSE_FULL &&
				device != RETRO_DEVICE_POINTER &&
				device != RETRO_DEVICE_LIGHTGUN)
			{
				device = RETROPAD_CLASSIC;
				HandleMessage(RETRO_LOG_INFO, "[FBNeo] Unknown device type for port %d, forcing \"Classic\" instead\n", port);
			}
		}
	}

	if (port < nMaxControllers && nDeviceType[port] != device)
	{
		nDeviceType[port] = device;
		bControllersNeedRefresh = true;
		if (bControllersSetOnce)
			RefreshControllers();
	}
}
