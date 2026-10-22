// Headless input for FinalBurn Neo.
// Author: Leonardo Kasperavičius
//
// Built originally to support OpenEmu-Silicon
// (https://github.com/OpenEmu-Silicon/OpenEmu-Silicon).
//
// Reads a digital switch state table populated by the host via
// FBHeadlessSetInputState(). InputMake() calls ReadSwitch() while building each
// frame's GameInp. Subject to the FB Neo license (see src/license.txt).

#include "burner.h"
#include "fbheadless.h"

// Indexed by FBNeo input switch code (masked); 0 = released, non-zero = pressed.
#define HEADLESS_INPUT_TABLE_SIZE 0x200
static unsigned char inputState[HEADLESS_INPUT_TABLE_SIZE];

static int HeadlessInpInit()
{
	memset(inputState, 0, sizeof(inputState));
	bInputOkay = 1;
	return 0;
}

static int HeadlessInpExit()
{
	bInputOkay = 0;
	return 0;
}

static int HeadlessInpSetCooperativeLevel(bool /* bExclusive */, bool /* bForeground */) { return 0; }
static int HeadlessInpNewFrame() { return 0; }

static int HeadlessInpReadSwitch(int nCode)
{
	return inputState[nCode & (HEADLESS_INPUT_TABLE_SIZE - 1)] ? 1 : 0;
}

static int HeadlessInpReadJoyAxis(int /* i */, int /* nAxis */)   { return 0; }
static int HeadlessInpReadMouseAxis(int /* i */, int /* nAxis */) { return 0; }
static int HeadlessInpFind(bool /* createBaseline */)             { return -1; }

static int HeadlessInpGetControlName(int /* nCode */, TCHAR * /* dev */, TCHAR * /* ctrl */) { return 0; }
static int HeadlessInpGetSettings(InterfaceInfo *) { return 0; }

struct InputInOut InputInOutHeadless = {
	HeadlessInpInit,
	HeadlessInpExit,
	HeadlessInpSetCooperativeLevel,
	HeadlessInpNewFrame,
	HeadlessInpReadSwitch,
	HeadlessInpReadJoyAxis,
	HeadlessInpReadMouseAxis,
	HeadlessInpFind,
	HeadlessInpGetControlName,
	HeadlessInpGetSettings,
	_T("Headless input"),
};

extern "C" void FBHeadlessSetInputState(int code, int value)
{
	inputState[code & (HEADLESS_INPUT_TABLE_SIZE - 1)] = value ? 1 : 0;
}
