// XInput module
//
// Handles XInput controllers
// Keyboard and mice are handled by the DirectInput plugin
//
// Joystick numbering:
//   joystick 0-3 : XInput user slots 1-4 (fixed, hot-pluggable)
//   joystick 4+  : remaining (non-XInput) DirectInput game controllers
//
// XInput devices are filtered out of the DirectInput enumeration so the same
// physical pad doesn't show up twice.
//
// XInput gamepad sub-codes (layout follows the DirectInput plugin):
//   0x00/0x01  Left stick  left/right    (axis 0)
//   0x02/0x03  Left stick  up/down       (axis 1)
//   0x04/0x05  Right stick left/right    (axis 2)
//   0x06/0x07  Right stick up/down       (axis 3)
//   0x09       Left trigger              (axis 4, 0 .. 32767)
//   0x0B       Right trigger             (axis 5, 0 .. 32767)
//   0x10-0x13  D-pad left/right/up/down  (reported like POV hat 0)
//   0x80-0x89  A, B, X, Y, LB, RB, Back, Start, LS click, RS click
//   0x8A       Guide ("jewel") button - only via the undocumented XInputGetStateEx
//              (ordinal 100 in xinput1_3/xinput1_4), not available with xinput9_1_0

#include "burner.h"

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <xinput.h>

#ifndef XUSER_MAX_COUNT
#define XUSER_MAX_COUNT		4
#endif
#ifndef XINPUT_GAMEPAD_TRIGGER_THRESHOLD
#define XINPUT_GAMEPAD_TRIGGER_THRESHOLD	30
#endif

#define XI_MAX_PADS			XUSER_MAX_COUNT
#define XI_MAX_AXIS			8
#define XI_NUM_BUTTONS		11
#define XI_GAMEPAD_GUIDE	0x0400		// reported by XInputGetStateEx only
#define XI_DEADZONE			0x4000		// same as the DirectInput plugin
#define XI_RETRY_FRAMES		120			// how often to re-probe an empty slot (XInputGetState on an empty slot is slow)
#define XI_DINPUT_OFFSET	(XI_MAX_PADS << 8)

extern struct InputInOut InputInOutDInput;
extern bool (*pDInputGamepadFilter)(const GUID* pGuidProduct);

typedef DWORD (WINAPI* XInputGetStateFn)(DWORD, XINPUT_STATE*);

static HMODULE hXInputDLL = NULL;
static XInputGetStateFn pXInputGetState = NULL;

// XInputGetStateEx writes a slightly larger struct (one trailing reserved DWORD)
struct xiStateBuffer {
	XINPUT_STATE state;
	DWORD dwReserved;
};

struct xiPadData {
	xiStateBuffer buf;
	bool connected;
	int retryCountdown;
	unsigned char readStatus;			// 0 = not read this frame, 1 = read OK, 2 = read failed
	int axisBaseline[XI_MAX_AXIS];
};

static xiPadData xiPads[XI_MAX_PADS];

static const WORD xiButtonMask[XI_NUM_BUTTONS] = {
	XINPUT_GAMEPAD_A, XINPUT_GAMEPAD_B, XINPUT_GAMEPAD_X, XINPUT_GAMEPAD_Y,
	XINPUT_GAMEPAD_LEFT_SHOULDER, XINPUT_GAMEPAD_RIGHT_SHOULDER,
	XINPUT_GAMEPAD_BACK, XINPUT_GAMEPAD_START,
	XINPUT_GAMEPAD_LEFT_THUMB, XINPUT_GAMEPAD_RIGHT_THUMB,
	XI_GAMEPAD_GUIDE
};

static const wchar_t* xiButtonName[XI_NUM_BUTTONS] = {
	L"A", L"B", L"X", L"Y", L"LB", L"RB", L"Back", L"Start", L"Left stick click", L"Right stick click", L"Guide"
};

static const wchar_t* xiAxisName[0x0C] = {
	L"Left stick left",  L"Left stick right",  L"Left stick up",  L"Left stick down",
	L"Right stick left", L"Right stick right", L"Right stick up", L"Right stick down",
	L"Left trigger",     L"Left trigger",      L"Right trigger",  L"Right trigger"
};

static const wchar_t* xiDpadName[4] = { L"D-pad left", L"D-pad right", L"D-pad up", L"D-pad down" };

// ---------------------------------------------------------------------------
// XInput device detection (used to keep XInput pads out of DirectInput)
// The raw input device path of an XInput device contains "IG_".
// DirectInput's guidProduct.Data1 is MAKELONG(VID, PID).

static bool xiIsXInputDevice(const GUID* pGuidProduct)
{
	if (pGuidProduct == NULL) {
		return false;
	}

	UINT nDevices = 0;
	if (GetRawInputDeviceList(NULL, &nDevices, sizeof(RAWINPUTDEVICELIST)) != 0 || nDevices == 0) {
		return false;
	}

	RAWINPUTDEVICELIST* pList = (RAWINPUTDEVICELIST*)malloc(sizeof(RAWINPUTDEVICELIST) * nDevices);
	if (pList == NULL) {
		return false;
	}

	bool bResult = false;
	INT32 nGot = (INT32)GetRawInputDeviceList(pList, &nDevices, sizeof(RAWINPUTDEVICELIST));

	for (INT32 i = 0; i < nGot && !bResult; i++) {
		if (pList[i].dwType != RIM_TYPEHID) {
			continue;
		}

		RID_DEVICE_INFO rdi;
		memset(&rdi, 0, sizeof(rdi));
		rdi.cbSize = sizeof(rdi);
		UINT nSize = sizeof(rdi);
		if ((INT32)GetRawInputDeviceInfoW(pList[i].hDevice, RIDI_DEVICEINFO, &rdi, &nSize) < 0) {
			continue;
		}

		if ((DWORD)MAKELONG(rdi.hid.dwVendorId, rdi.hid.dwProductId) != pGuidProduct->Data1) {
			continue;
		}

		wchar_t szName[256];
		nSize = sizeof(szName) / sizeof(szName[0]);
		if ((INT32)GetRawInputDeviceInfoW(pList[i].hDevice, RIDI_DEVICENAME, szName, &nSize) < 0) {
			continue;
		}
		szName[255] = 0;

		if (wcsstr(szName, L"IG_") || wcsstr(szName, L"ig_")) {
			bResult = true;
		}
	}

	free(pList);

	return bResult;
}

// ---------------------------------------------------------------------------

static int xiLoadLibrary()
{
	static const wchar_t* szDLLs[] = { L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll" };

	if (hXInputDLL) {
		return 0;
	}

	for (UINT32 i = 0; i < sizeof(szDLLs) / sizeof(szDLLs[0]); i++) {
		hXInputDLL = LoadLibraryW(szDLLs[i]);
		if (hXInputDLL) {
			// Prefer XInputGetStateEx (ordinal 100): same as XInputGetState, but also reports the Guide button
			pXInputGetState = (XInputGetStateFn)GetProcAddress(hXInputDLL, (LPCSTR)100);
			if (pXInputGetState == NULL) {
				pXInputGetState = (XInputGetStateFn)GetProcAddress(hXInputDLL, "XInputGetState");
			}
			if (pXInputGetState) {
				return 0;
			}
			FreeLibrary(hXInputDLL);
			hXInputDLL = NULL;
		}
	}

	pXInputGetState = NULL;

	return 1;
}

static void xiFreeLibrary()
{
	if (hXInputDLL) {
		FreeLibrary(hXInputDLL);
		hXInputDLL = NULL;
	}
	pXInputGetState = NULL;
}

// Read one XInput pad (once per frame)
static int xiReadPad(int i)
{
	xiPadData* pad = &xiPads[i];

	if (pad->readStatus == 1) {
		return 0;
	}
	if (pad->readStatus == 2 || pXInputGetState == NULL) {
		return 1;
	}

	pad->readStatus = 2;

	// Don't hammer empty slots every frame
	if (!pad->connected && pad->retryCountdown > 0) {
		return 1;
	}

	if (pXInputGetState(i, &pad->buf.state) != ERROR_SUCCESS) {
		memset(&pad->buf, 0, sizeof(pad->buf));
		pad->connected = false;
		pad->retryCountdown = XI_RETRY_FRAMES;
		return 1;
	}

	pad->connected = true;
	pad->readStatus = 1;

	return 0;
}

// Axis values in DirectInput plugin convention: -32768 .. 32767, Y positive = down
static int xiAxisValue(xiPadData* pad, int axis)
{
	const XINPUT_GAMEPAD* g = &pad->buf.state.Gamepad;

	switch (axis) {
		case 0: return g->sThumbLX;
		case 1: return ~(int)g->sThumbLY;				// invert: XInput Y is up-positive
		case 2: return g->sThumbRX;
		case 3: return ~(int)g->sThumbRY;
		case 4: return (g->bLeftTrigger  * 32767) / 255;
		case 5: return (g->bRightTrigger * 32767) / 255;
	}

	return 0;
}

static int xiPadState(xiPadData* pad, unsigned int subCode)
{
	const XINPUT_GAMEPAD* g = &pad->buf.state.Gamepad;

	if (subCode < 0x10) {								// axes
		switch (subCode) {
			case 0x00: return xiAxisValue(pad, 0) < -XI_DEADZONE;
			case 0x01: return xiAxisValue(pad, 0) >  XI_DEADZONE;
			case 0x02: return xiAxisValue(pad, 1) < -XI_DEADZONE;
			case 0x03: return xiAxisValue(pad, 1) >  XI_DEADZONE;
			case 0x04: return xiAxisValue(pad, 2) < -XI_DEADZONE;
			case 0x05: return xiAxisValue(pad, 2) >  XI_DEADZONE;
			case 0x06: return xiAxisValue(pad, 3) < -XI_DEADZONE;
			case 0x07: return xiAxisValue(pad, 3) >  XI_DEADZONE;
			case 0x09: return g->bLeftTrigger  > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
			case 0x0B: return g->bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
		}
		return 0;
	}

	if (subCode < 0x14) {								// d-pad (as POV hat 0)
		switch (subCode & 3) {
			case 0: return (g->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)  ? 1 : 0;
			case 1: return (g->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) ? 1 : 0;
			case 2: return (g->wButtons & XINPUT_GAMEPAD_DPAD_UP)    ? 1 : 0;
			case 3: return (g->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)  ? 1 : 0;
		}
	}

	if (subCode >= 0x80 && subCode < 0x80 + XI_NUM_BUTTONS) {
		return (g->wButtons & xiButtonMask[subCode & 0x7F]) ? 1 : 0;
	}

	return 0;
}

static int xiReadAxis(int i, int axis)
{
	if (i < 0 || i >= XI_MAX_PADS) {
		return 0;
	}
	if (xiReadPad(i)) {
		return 0;
	}

	return xiAxisValue(&xiPads[i], axis);
}

// Scan the XInput pads for an active control. Returns -1 if none.
static int xiFind(bool createBaseline)
{
	int retVal = -1;

	for (int i = 0; i < XI_MAX_PADS && retVal < 0; i++) {
		xiPadData* pad = &xiPads[i];
		if (xiReadPad(i)) {
			continue;
		}

		for (unsigned int j = 0; j < 0x0C && retVal < 0; j++) {			// axes / triggers
			int delta = pad->axisBaseline[j >> 1] - xiAxisValue(pad, j >> 1);
			if (delta < -XI_DEADZONE || delta > XI_DEADZONE) {
				if (xiPadState(pad, j)) {
					retVal = 0x4000 | (i << 8) | j;
				}
			}
		}

		for (unsigned int j = 0x10; j < 0x14 && retVal < 0; j++) {		// d-pad
			if (xiPadState(pad, j)) {
				retVal = 0x4000 | (i << 8) | j;
			}
		}

		for (unsigned int j = 0x80; j < 0x80 + XI_NUM_BUTTONS && retVal < 0; j++) {	// buttons
			if (xiPadState(pad, j)) {
				retVal = 0x4000 | (i << 8) | j;
			}
		}
	}

	if (createBaseline) {
		for (int i = 0; i < XI_MAX_PADS; i++) {
			for (int j = 0; j < XI_MAX_AXIS; j++) {
				xiPads[i].axisBaseline[j] = xiReadAxis(i, j);
			}
		}
	}

	return retVal;
}

// ---------------------------------------------------------------------------
// InputInOut interface

static int XInputExit()
{
	pDInputGamepadFilter = NULL;
	InputInOutDInput.Exit();

	xiFreeLibrary();
	memset(xiPads, 0, sizeof(xiPads));

	return 0;
}

static int XInputInit()
{
	XInputExit();

	// If XInput isn't available, carry on with keyboard/mouse/DirectInput pads only
	if (xiLoadLibrary() == 0) {
		pDInputGamepadFilter = xiIsXInputDevice;
	}

	int nRet = InputInOutDInput.Init();

	for (int i = 0; i < XI_MAX_PADS; i++) {
		xiPads[i].readStatus = 0;
		xiPads[i].retryCountdown = 0;		// probe every slot right away
		xiReadPad(i);
		xiPads[i].readStatus = 0;
	}

	return nRet;
}

static int XInputSetCooperativeLevel(bool bExclusive, bool bForeground)
{
	return InputInOutDInput.SetCooperativeLevel(bExclusive, bForeground);
}

static int XInputNewFrame()
{
	for (int i = 0; i < XI_MAX_PADS; i++) {
		xiPads[i].readStatus = 0;
		if (!xiPads[i].connected && xiPads[i].retryCountdown > 0) {
			xiPads[i].retryCountdown--;
		}
	}

	return InputInOutDInput.NewFrame();
}

static int XInputReadSwitch(int nCode)
{
	if (nCode >= 0x4000 && nCode < 0x8000) {
		int i = (nCode - 0x4000) >> 8;

		if (i < XI_MAX_PADS) {
			if (xiReadPad(i)) {
				return 0;
			}
			return xiPadState(&xiPads[i], nCode & 0xFF);
		}

		return InputInOutDInput.ReadSwitch(nCode - XI_DINPUT_OFFSET);
	}

	return InputInOutDInput.ReadSwitch(nCode);
}

static int XInputReadJoyAxis(int i, int nAxis)
{
	if (i < 0) {
		return 0;
	}
	if (i < XI_MAX_PADS) {
		return xiReadAxis(i, nAxis);
	}

	return InputInOutDInput.ReadJoyAxis(i - XI_MAX_PADS, nAxis);
}

static int XInputReadMouseAxis(int i, int nAxis)
{
	return InputInOutDInput.ReadMouseAxis(i, nAxis);
}

static int XInputFind(bool createBaseline)
{
	// Both calls also (re)build their axis baselines when asked to
	int nDInput = InputInOutDInput.Find(createBaseline);
	int nXInput = xiFind(createBaseline);

	if (nDInput >= 0 && nDInput < 0x4000) {			// keyboard first, as in the DirectInput plugin
		return nDInput;
	}
	if (nXInput >= 0) {								// then XInput pads
		return nXInput;
	}
	if (nDInput >= 0x4000 && nDInput < 0x8000) {	// then DirectInput pads (renumbered)
		return nDInput + XI_DINPUT_OFFSET;
	}

	return nDInput;									// mouse or nothing
}

static int XInputGetControlName(int nCode, wchar_t* pszDeviceName, wchar_t* pszControlName)
{
	if ((nCode & 0xC000) != 0x4000) {
		return InputInOutDInput.GetControlName(nCode, pszDeviceName, pszControlName);
	}

	int i = (nCode >> 8) & 0x3F;

	if (i >= XI_MAX_PADS) {
		return InputInOutDInput.GetControlName(nCode - XI_DINPUT_OFFSET, pszDeviceName, pszControlName);
	}

	if (pszDeviceName) {
		pszDeviceName[0] = 0;
	}
	if (pszControlName) {
		pszControlName[0] = 0;
	}

	if (pXInputGetState == NULL) {					// XInput not available: no XInput slots
		return 0;
	}

	if (pszDeviceName) {
		xiReadPad(i);
		_snwprintf(pszDeviceName, MAX_PATH, L"XInput controller #%d%s", i + 1, xiPads[i].connected ? L"" : L" (not connected)");
		pszDeviceName[MAX_PATH - 1] = 0;
	}

	if (pszControlName) {
		unsigned int subCode = nCode & 0xFF;
		const wchar_t* szName = NULL;

		if (subCode < 0x0C) {
			szName = xiAxisName[subCode];
		} else if (subCode >= 0x10 && subCode < 0x14) {
			szName = xiDpadName[subCode & 3];
		} else if (subCode >= 0x80 && subCode < 0x80 + XI_NUM_BUTTONS) {
			szName = xiButtonName[subCode & 0x7F];
		}

		if (szName) {
			wcsncpy(pszControlName, szName, MAX_PATH);
			pszControlName[MAX_PATH - 1] = 0;
		}
	}

	return 0;
}

struct InputInOut InputInOutXInput = { XInputInit, XInputExit, XInputSetCooperativeLevel, XInputNewFrame, XInputReadSwitch, XInputReadJoyAxis, XInputReadMouseAxis, XInputFind, XInputGetControlName, NULL, _T("XInput + DirectInput8 input") };
