// Headless input mapping for FinalBurn Neo.
// Author: Leonardo Kasperavičius
//
// Built originally to support OpenEmu-Silicon
// (https://github.com/OpenEmu-Silicon/OpenEmu-Silicon).
//
// Maps a (player, FBHeadlessButton) pair to the active driver's GameInp. FBNeo
// exposes each input's canonical lowercase name via szInfo ("p1 up",
// "p1 fire 1", "p1 coin", "p1 start"); each matched digital input is rebound to
// a private switch code the headless input plugin reads from its state table.
// Subject to the FB Neo license (see src/license.txt).

#include "burner.h"
#include "fbheadless.h"

// Private switch-code space, chosen to avoid the driver's default key/joy codes.
// code = HL_CODE_BASE + player*16 + button; unmatched digital inputs get a dead
// code that is never set so they read as released.
#define HL_CODE_BASE 0x100
#define HL_CODE_DEAD 0x1FF

static int hl_code(int player, int button)
{
	return HL_CODE_BASE + player * 16 + button;
}

// Parse an FBNeo szInfo name into (player, FBHeadlessButton). Returns 1 on match.
static int hl_classify(const char *szInfo, int *outPlayer, int *outButton)
{
	if (szInfo == NULL)
		return 0;

	// "pN <control>" form (the common case).
	if ((szInfo[0] == 'p' || szInfo[0] == 'P') &&
	    szInfo[1] >= '1' && szInfo[1] <= '4' && szInfo[2] == ' ') {
		int player = szInfo[1] - '1';
		const char *r = szInfo + 3;
		int b = -1;

		if      (!strcmp(r, "up"))         b = FBHeadlessButtonUp;
		else if (!strcmp(r, "down"))       b = FBHeadlessButtonDown;
		else if (!strcmp(r, "left"))       b = FBHeadlessButtonLeft;
		else if (!strcmp(r, "right"))      b = FBHeadlessButtonRight;
		else if (!strcmp(r, "start"))      b = FBHeadlessButtonStart;
		else if (!strcmp(r, "coin"))       b = FBHeadlessButtonCoin;
		else if (!strcmp(r, "service"))    b = FBHeadlessButtonService;
		else if (!strcmp(r, "diagnostic")) b = FBHeadlessButtonService;
		else if (!strncmp(r, "fire ", 5) && r[5] >= '1' && r[5] <= '6')
			b = FBHeadlessButton1 + (r[5] - '1');
		else if (!strncmp(r, "button ", 7) && r[7] >= '1' && r[7] <= '6')
			b = FBHeadlessButton1 + (r[7] - '1');

		if (b >= 0) {
			*outPlayer = player;
			*outButton = b;
			return 1;
		}
		return 0;
	}

	// System "coin N" / "start N" form.
	if (!strncmp(szInfo, "coin ", 5) && szInfo[5] >= '1' && szInfo[5] <= '4') {
		*outPlayer = szInfo[5] - '1';
		*outButton = FBHeadlessButtonCoin;
		return 1;
	}
	if (!strncmp(szInfo, "start ", 6) && szInfo[6] >= '1' && szInfo[6] <= '4') {
		*outPlayer = szInfo[6] - '1';
		*outButton = FBHeadlessButtonStart;
		return 1;
	}

	return 0;
}

// Cabinet-wide test switch ("diag" in most drivers), which opens the game's
// settings menu.
static int hl_is_test_switch(const char *szInfo)
{
	return szInfo != NULL &&
	       (!strcmp(szInfo, "diag") || !strcmp(szInfo, "diagnostic") ||
	        !strcmp(szInfo, "diagnostics") || !strcmp(szInfo, "test"));
}

// Cabinet-wide "service" input. Drivers with a separate test switch use it as a
// service credit; older drivers without one use it as the test switch itself.
static int hl_is_service_switch(const char *szInfo)
{
	return szInfo != NULL && !strcmp(szInfo, "service");
}

// Rebind every digital input of the loaded driver to the headless code space.
// Called after DrvInit (which runs GameInpInit/GameInpDefault).
void headless_map_inputs(void)
{
	struct BurnInputInfo bii;

	// Service always opens the settings menu: it drives the test switch when the
	// driver has one (and Configure drives "service"), otherwise "service".
	int hasTestSwitch = 0;
	for (UINT32 i = 0; i < nGameInpCount; i++) {
		if (BurnDrvGetInputInfo(&bii, i) == 0 && bii.nType == BIT_DIGITAL && hl_is_test_switch(bii.szInfo)) {
			hasTestSwitch = 1;
			break;
		}
	}

	for (UINT32 i = 0; i < nGameInpCount; i++) {
		if (BurnDrvGetInputInfo(&bii, i))
			continue;
		if (bii.nType != BIT_DIGITAL)
			continue;

		GameInp[i].nInput = GIT_SWITCH;

		int player = 0, button = 0;
		if (hl_is_test_switch(bii.szInfo))
			GameInp[i].Input.Switch.nCode = (UINT16)hl_code(0, FBHeadlessButtonService);
		else if (hl_is_service_switch(bii.szInfo))
			GameInp[i].Input.Switch.nCode = (UINT16)hl_code(0, hasTestSwitch ? FBHeadlessButtonConfigure : FBHeadlessButtonService);
		else if (hl_classify(bii.szInfo, &player, &button) && player >= 0 && player < 4)
			GameInp[i].Input.Switch.nCode = (UINT16)hl_code(player, button);
		else
			GameInp[i].Input.Switch.nCode = HL_CODE_DEAD;
	}
}

extern "C" void FBHeadlessSetInput(int player, int button, int value)
{
	if (player < 0 || player >= 4)
		return;
	if (button < 0 || button >= FBHeadlessButtonCount)
		return;
	FBHeadlessSetInputState(hl_code(player, button), value);
}
