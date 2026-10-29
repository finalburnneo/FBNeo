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

// Index of a direction word at the start of s (0 left, 1 up, 2 down, 3 right),
// or -1. *len receives its length.
static int hl_direction(const char *s, size_t *len)
{
	static const char *const dirs[] = { "left", "up", "down", "right" };
	for (int d = 0; d < 4; d++) {
		size_t n = strlen(dirs[d]);
		if (!strncasecmp(s, dirs[d], n)) {
			*len = n;
			return d;
		}
	}
	return -1;
}

// Twin-stick games (Robotron, Smash TV, Angel Kids, Ikari Warriors...), mapped
// the way the MAME core does: the left stick on the player's d-pad, the right
// stick on buttons 1-4 (left, up, down, right), since OpenEmu's arcade
// controller has one stick. Drivers name the sticks inconsistently: the second
// stick may be "pN <dir> 2", or either stick may borrow another player's plain
// d-pad ("p2 up" for Robotron's "Fire Up", "p3 up" for Angel Kids' "P1 Left
// Stick Up"). The display name, when it names a stick, decides the stick and
// the player; otherwise "pN <dir> 2" is player N's right stick.
static int hl_twin_stick(const char *szName, const char *szInfo, int *outPlayer, int *outButton)
{
	if (szName == NULL || szInfo == NULL)
		return 0;
	if (szInfo[0] != 'p' || szInfo[1] < '1' || szInfo[1] > '4' || szInfo[2] != ' ')
		return 0;

	size_t len = 0;
	int dir = hl_direction(szInfo + 3, &len);
	if (dir < 0)
		return 0;
	const char *rest = szInfo + 3 + len;
	int secondStick = !strcmp(rest, " 2");
	if (!secondStick && *rest != '\0')
		return 0;

	static const struct { const char *prefix; int right; } sticks[] = {
		{ "Left Stick ", 0 }, { "Lft Stick ", 0 }, { "L Stick ", 0 }, { "Left ", 0 },
		{ "Right Stick ", 1 }, { "Rght Stick ", 1 }, { "ight Stick ", 1 }, { "R Stick ", 1 },
		{ "Right ", 1 }, { "Fire ", 1 }, { "Shoot ", 1 }, { "Aim ", 1 },
	};
	static const int dpad[] = { FBHeadlessButtonLeft, FBHeadlessButtonUp, FBHeadlessButtonDown, FBHeadlessButtonRight };

	const char *n = szName;
	int player = 0;
	if ((n[0] == 'P' || n[0] == 'p') && n[1] >= '1' && n[1] <= '4' && n[2] == ' ') {
		player = n[1] - '1';
		n += 3;
	}
	for (size_t s = 0; s < sizeof(sticks) / sizeof(sticks[0]); s++) {
		size_t sl = strlen(sticks[s].prefix);
		size_t nameDirLen = 0;
		if (strncasecmp(n, sticks[s].prefix, sl) != 0)
			continue;
		if (hl_direction(n + sl, &nameDirLen) != dir || n[sl + nameDirLen] != '\0')
			break;
		*outPlayer = player;
		*outButton = sticks[s].right ? FBHeadlessButton1 + dir : dpad[dir];
		return 1;
	}

	if (secondStick) {
		*outPlayer = szInfo[1] - '1';
		*outButton = FBHeadlessButton1 + dir;
		return 1;
	}
	return 0;
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
		else if (hl_twin_stick(bii.szName, bii.szInfo, &player, &button))
			GameInp[i].Input.Switch.nCode = (UINT16)hl_code(player, button);
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
