// FBNeo hotkeys - user-configurable keyboard shortcuts for the frontend - dink [oct.2026]
//
// Every emulator hotkey lives in the table below. There are two kinds:
//  - command hotkeys: they post a WM_COMMAND (menu id) through a run-time built
//    accelerator table, the commands are handled by OnCommand() in scrn.cpp
//  - action hotkeys: things that have no menu item, or need key-up handling
//    (fast forward, rewind, volume, lua console, ...). These are matched in the
//    message loop in run.cpp with HotkeyFindAction() / HotkeyActionUsesKey()
//
// A key is stored as 0xMMVV: VV = Windows virtual-key code, MM = HOTKEY_MOD_* flags

#include "burner.h"

struct HotkeyDef {
	const TCHAR* szName;			// name used in the config file (never change these!)
	const TCHAR* szCategory;		// shown in the dialog
	const TCHAR* szDesc;			// shown in the dialog
	INT32 nCommand;					// WM_COMMAND id (accelerator) or 0
	INT32 nAction;					// HOTKEY_ACT_* (handled in run.cpp) or HOTKEY_ACT_NONE
	UINT16 nDefault[2];				// default key, default alternate key
	INT32 nMenuAlias[2];			// other menu items that display this shortcut
};

#define S	HOTKEY_MOD_SHIFT
#define C	HOTKEY_MOD_CTRL
#define A	HOTKEY_MOD_ALT
#define HK(vk, mods)	((UINT16)((vk) | (mods)))

static const HotkeyDef HotkeyDefs[] = {
	// Game
	{ _T("LoadGame"),			_T("Game"),		_T("Load game"),							MENU_LOAD,					HOTKEY_ACT_NONE,			{ HK(VK_F6, 0),				0 },					{ 0, 0 } },
	{ _T("ExitGame"),			_T("Game"),		_T("Exit game"),							MENU_QUIT,					HOTKEY_ACT_NONE,			{ HK(VK_F4, C),				HK('W', C) },			{ 0, 0 } },
	{ _T("Pause"),				_T("Game"),		_T("Pause game"),							MENU_PAUSE,					HOTKEY_ACT_NONE,			{ HK(VK_PAUSE, 0),			0 },					{ 0, 0 } },
	{ _T("FastForward"),		_T("Game"),		_T("Fast forward (hold) / frame advance when paused"), 0,				HOTKEY_ACT_FFWD,			{ HK(VK_F1, 0),				0 },					{ 0, 0 } },
	{ _T("FastForwardToggle"),	_T("Game"),		_T("Fast forward (toggle)"),				0,							HOTKEY_ACT_FFWD_TOGGLE,		{ HK(VK_F1, S),				0 },					{ 0, 0 } },
	{ _T("Rewind"),				_T("Game"),		_T("Rewind (hold)"),						0,							HOTKEY_ACT_REWIND,			{ HK(VK_PRIOR, 0),			0 },					{ 0, 0 } },
	{ _T("RomDataManager"),		_T("Game"),		_T("Open RomData manager"),					MENU_ROMDATA_MANAGER,		HOTKEY_ACT_NONE,			{ HK('R', C),				0 },					{ 0, 0 } },
	{ _T("StartNeoGeoMVS"),		_T("Game"),		_T("Start Neo Geo Multi-slot"),				MENU_START_NEOGEO_MVS,		HOTKEY_ACT_NONE,			{ HK(VK_F6, C),				0 },					{ 0, 0 } },
	{ _T("StartNeoGeoCD"),		_T("Game"),		_T("Start Neo Geo CDZ"),					MENU_START_NEOGEO_CD,		HOTKEY_ACT_NONE,			{ HK(VK_F6, C | S),			0 },					{ 0, 0 } },
	{ _T("CDImageLibrary"),		_T("Game"),		_T("CD image library"),						MENU_LOAD_NEOCD,			HOTKEY_ACT_NONE,			{ HK(VK_F8, C),				0 },					{ 0, 0 } },
	{ _T("SelectCDImage"),		_T("Game"),		_T("Select CD image"),						MENU_CDIMAGE,				HOTKEY_ACT_NONE,			{ HK(VK_F12, C | S),		0 },					{ 0, 0 } },

	// Input
	{ _T("MapInputs"),			_T("Input"),	_T("Map game inputs"),						MENU_INPUT,					HOTKEY_ACT_NONE,			{ HK(VK_F5, 0),				0 },					{ 0, 0 } },
	{ _T("DipSwitches"),		_T("Input"),	_T("Set dipswitches"),						MENU_DIPSW,					HOTKEY_ACT_NONE,			{ HK(VK_F5, C),				0 },					{ 0, 0 } },

	// Save states
	{ _T("StateLoadSlot"),		_T("States"),	_T("Load state from active slot"),			MENU_STATE_LOAD_SLOT,		HOTKEY_ACT_NONE,			{ HK(VK_F9, 0),				0 },					{ 0, 0 } },
	{ _T("StateSaveSlot"),		_T("States"),	_T("Save state to active slot"),			MENU_STATE_SAVE_SLOT,		HOTKEY_ACT_NONE,			{ HK(VK_F10, 0),			0 },					{ 0, 0 } },
	{ _T("StatePrevSlot"),		_T("States"),	_T("Activate previous slot"),				MENU_STATE_PREVSLOT,		HOTKEY_ACT_NONE,			{ HK(VK_F8, 0),				0 },					{ 0, 0 } },
	{ _T("StateNextSlot"),		_T("States"),	_T("Activate next slot"),					MENU_STATE_NEXTSLOT,		HOTKEY_ACT_NONE,			{ HK(VK_F11, 0),			0 },					{ 0, 0 } },
	{ _T("StateUndo"),			_T("States"),	_T("Undo last state save"),					MENU_STATE_UNDO,			HOTKEY_ACT_NONE,			{ HK(VK_F8, S),				0 },					{ 0, 0 } },
	{ _T("StateLoadDialog"),	_T("States"),	_T("Load state..."),						MENU_STATE_LOAD_DIALOG,		HOTKEY_ACT_NONE,			{ HK(VK_F9, C),				0 },					{ 0, 0 } },
	{ _T("StateSaveDialog"),	_T("States"),	_T("Save state..."),						MENU_STATE_SAVE_DIALOG,		HOTKEY_ACT_NONE,			{ HK(VK_F10, C),			0 },					{ 0, 0 } },

	// Memory card
	{ _T("MemcardToggle"),		_T("Memcard"),	_T("Insert / eject memory card"),			MENU_MEMCARD_TOGGLE,		HOTKEY_ACT_NONE,			{ HK(VK_F7, 0),				0 },					{ MENU_MEMCARD_INSERT, MENU_MEMCARD_EJECT } },
	{ _T("MemcardCreate"),		_T("Memcard"),	_T("Create new memory card"),				MENU_MEMCARD_CREATE,		HOTKEY_ACT_NONE,			{ HK(VK_F7, C | S),			0 },					{ 0, 0 } },
	{ _T("MemcardSelect"),		_T("Memcard"),	_T("Select memory card"),					MENU_MEMCARD_SELECT,		HOTKEY_ACT_NONE,			{ HK(VK_F7, C),				0 },					{ 0, 0 } },

	// Video / display
	{ _T("FullScreen"),			_T("Video"),	_T("Toggle fullscreen mode"),				MENU_FULL,					HOTKEY_ACT_NONE,			{ HK(VK_RETURN, A),			0 },					{ 0, 0 } },
	{ _T("ExitFullScreen"),		_T("Video"),	_T("Leave fullscreen (quit if started from command line)"), 0,			HOTKEY_ACT_EXIT_FULLSCREEN,	{ HK(VK_ESCAPE, 0),			0 },					{ 0, 0 } },
	{ _T("ToggleMenu"),			_T("Video"),	_T("Show / hide menu"),						0,							HOTKEY_ACT_TOGGLE_MENU,		{ HK(VK_RETURN, C),			0 },					{ 0, 0 } },
	{ _T("ToggleFPS"),			_T("Video"),	_T("Show / hide FPS counter"),				0,							HOTKEY_ACT_TOGGLE_FPS,		{ HK(VK_BACK, 0),			0 },					{ 0, 0 } },
	{ _T("ToggleFrameCounter"),	_T("Video"),	_T("Show / hide replay frame counter"),		0,							HOTKEY_ACT_TOGGLE_FRAMECOUNTER, { HK(VK_BACK, S),		0 },					{ 0, 0 } },

	// Sound
	{ _T("Mute"),				_T("Sound"),	_T("Mute / unmute"),						0,							HOTKEY_ACT_MUTE,			{ HK('S', A),				0 },					{ 0, 0 } },
	{ _T("VolumeUp"),			_T("Sound"),	_T("Volume up"),							0,							HOTKEY_ACT_VOLUME_UP,		{ HK(VK_OEM_PLUS, A),		0 },					{ 0, 0 } },
	{ _T("VolumeDown"),			_T("Sound"),	_T("Volume down"),							0,							HOTKEY_ACT_VOLUME_DOWN,		{ HK(VK_OEM_MINUS, A),		0 },					{ 0, 0 } },
	{ _T("VolumeUpFine"),		_T("Sound"),	_T("Volume up (fine)"),						0,							HOTKEY_ACT_VOLUME_UP_FINE,	{ HK(VK_OEM_PLUS, A | C),	0 },					{ 0, 0 } },
	{ _T("VolumeDownFine"),		_T("Sound"),	_T("Volume down (fine)"),					0,							HOTKEY_ACT_VOLUME_DOWN_FINE, { HK(VK_OEM_MINUS, A | C),	0 },					{ 0, 0 } },

	// Snapshots
	{ _T("SaveSnapshot"),		_T("Snapshot"),	_T("Save snapshot"),						MENU_SAVESNAP,				HOTKEY_ACT_NONE,			{ HK(VK_F12, 0),			0 },					{ 0, 0 } },
	{ _T("SaveTitleSnapshot"),	_T("Snapshot"),	_T("Save snapshot as title"),				MENU_SAVETITLESNAP,			HOTKEY_ACT_NONE,			{ HK(VK_F10, S),			0 },					{ 0, 0 } },
	{ _T("SavePreviewSnapshot"),_T("Snapshot"),	_T("Save snapshot as preview"),				MENU_SAVEPREVIEWSNAP,		HOTKEY_ACT_NONE,			{ HK(VK_F11, S),			0 },					{ 0, 0 } },
	{ _T("ShotFactory"),		_T("Snapshot"),	_T("Shot factory"),							MENU_SNAPFACT,				HOTKEY_ACT_NONE,			{ HK(VK_F12, C),			0 },					{ 0, 0 } },

	// Cheats
	{ _T("EnableCheats"),		_T("Cheats"),	_T("Enable cheats"),						MENU_ENABLECHEAT,			HOTKEY_ACT_NONE,			{ HK(VK_F1, C),				0 },					{ 0, 0 } },
	{ _T("CheatSearchStart"),	_T("Cheats"),	_T("Cheat search: start new search"),		MENU_CHEATSEARCH_START,		HOTKEY_ACT_NONE,			{ HK(VK_HOME, C),			0 },					{ 0, 0 } },
	{ _T("CheatSearchNoChange"),_T("Cheats"),	_T("Cheat search: value has not changed"),	MENU_CHEATSEARCH_NOCHANGE,	HOTKEY_ACT_NONE,			{ HK(VK_DIVIDE, C),			0 },					{ 0, 0 } },
	{ _T("CheatSearchChange"),	_T("Cheats"),	_T("Cheat search: value has changed"),		MENU_CHEATSEARCH_CHANGE,	HOTKEY_ACT_NONE,			{ HK(VK_MULTIPLY, C),		0 },					{ 0, 0 } },
	{ _T("CheatSearchDecrease"),_T("Cheats"),	_T("Cheat search: value has decreased"),	MENU_CHEATSEARCH_DECREASE,	HOTKEY_ACT_NONE,			{ HK(VK_SUBTRACT, C),		0 },					{ 0, 0 } },
	{ _T("CheatSearchIncrease"),_T("Cheats"),	_T("Cheat search: value has increased"),	MENU_CHEATSEARCH_INCREASE,	HOTKEY_ACT_NONE,			{ HK(VK_ADD, C),			0 },					{ 0, 0 } },

	// Tools
	{ _T("Debugger"),			_T("Tools"),	_T("Invoke 68K debugger"),					MENU_DEBUG,					HOTKEY_ACT_NONE,			{ HK(VK_F1, C | S),			0 },					{ 0, 0 } },
	{ _T("PaletteViewer"),		_T("Tools"),	_T("Palette viewer"),						MENU_PALETTEVIEWER,			HOTKEY_ACT_NONE,			{ HK(VK_F12, S),			0 },					{ 0, 0 } },

	// Lua
	{ _T("LuaOpen"),			_T("Lua"),		_T("Open Lua script window"),				0,							HOTKEY_ACT_LUA_OPEN,		{ HK('L', A),				0 },					{ 0, 0 } },
	{ _T("LuaClose"),			_T("Lua"),		_T("Close Lua script window"),				0,							HOTKEY_ACT_LUA_CLOSE,		{ HK('T', A),				0 },					{ 0, 0 } },
	{ _T("LuaRun"),				_T("Lua"),		_T("Run Lua script"),						0,							HOTKEY_ACT_LUA_RUN,			{ HK('E', A),				0 },					{ 0, 0 } },
	{ _T("LuaStop"),			_T("Lua"),		_T("Stop Lua script"),						0,							HOTKEY_ACT_LUA_STOP,		{ HK('P', A),				0 },					{ 0, 0 } },
	{ _T("LuaBrowse"),			_T("Lua"),		_T("Browse for Lua script"),				0,							HOTKEY_ACT_LUA_BROWSE,		{ HK('B', A),				0 },					{ 0, 0 } },

	// Netplay
	{ _T("NetplayChat"),		_T("Netplay"),	_T("Open chat (netplay)"),					0,							HOTKEY_ACT_NETPLAY_CHAT,	{ HK(VK_OEM_2, 0),			0 },					{ 0, 0 } },
};

#undef S
#undef C
#undef A

#define HOTKEY_COUNT	((INT32)(sizeof(HotkeyDefs) / sizeof(HotkeyDefs[0])))

static UINT16 nHotkeys[HOTKEY_COUNT][2];
static bool bHotkeysInit = false;

static void HotkeysInit()
{
	if (bHotkeysInit) return;

	for (INT32 i = 0; i < HOTKEY_COUNT; i++) {
		nHotkeys[i][0] = HotkeyDefs[i].nDefault[0];
		nHotkeys[i][1] = HotkeyDefs[i].nDefault[1];
	}

	bHotkeysInit = true;
}

// ---------------------------------------------------------------------------
// Key names

static void HotkeyKeyName(UINT nVK, TCHAR* szName, INT32 nLen)
{
	if (nVK >= VK_F1 && nVK <= VK_F24) {
		_sntprintf(szName, nLen, _T("F%d"), nVK - VK_F1 + 1);
		return;
	}
	if ((nVK >= '0' && nVK <= '9') || (nVK >= 'A' && nVK <= 'Z')) {
		_sntprintf(szName, nLen, _T("%c"), (TCHAR)nVK);
		return;
	}
	if (nVK >= VK_NUMPAD0 && nVK <= VK_NUMPAD9) {
		_sntprintf(szName, nLen, _T("Num %d"), nVK - VK_NUMPAD0);
		return;
	}

	const TCHAR* szFixed = NULL;
	switch (nVK) {
		case VK_PAUSE:		szFixed = _T("Pause");		break;
		case VK_RETURN:		szFixed = _T("Enter");		break;
		case VK_ESCAPE:		szFixed = _T("Esc");		break;
		case VK_BACK:		szFixed = _T("Backspace");	break;
		case VK_TAB:		szFixed = _T("Tab");		break;
		case VK_SPACE:		szFixed = _T("Space");		break;
		case VK_PRIOR:		szFixed = _T("PgUp");		break;
		case VK_NEXT:		szFixed = _T("PgDn");		break;
		case VK_HOME:		szFixed = _T("Home");		break;
		case VK_END:		szFixed = _T("End");		break;
		case VK_INSERT:		szFixed = _T("Ins");		break;
		case VK_DELETE:		szFixed = _T("Del");		break;
		case VK_LEFT:		szFixed = _T("Left");		break;
		case VK_RIGHT:		szFixed = _T("Right");		break;
		case VK_UP:			szFixed = _T("Up");			break;
		case VK_DOWN:		szFixed = _T("Down");		break;
		case VK_SCROLL:		szFixed = _T("Scroll Lock");break;
		case VK_SNAPSHOT:	szFixed = _T("PrtScn");		break;
		case VK_DIVIDE:		szFixed = _T("Num /");		break;
		case VK_MULTIPLY:	szFixed = _T("Num *");		break;
		case VK_SUBTRACT:	szFixed = _T("Num -");		break;
		case VK_ADD:		szFixed = _T("Num +");		break;
		case VK_DECIMAL:	szFixed = _T("Num .");		break;
		case VK_CLEAR:		szFixed = _T("Num 5");		break;
	}
	if (szFixed) {
		_sntprintf(szName, nLen, _T("%s"), szFixed);
		return;
	}

	// Let Windows name it (this gives the right symbol for OEM keys on the current layout)
	UINT nScan = MapVirtualKey(nVK, 0 /* MAPVK_VK_TO_VSC */);
	if (nScan && GetKeyNameText((LONG)(nScan << 16), szName, nLen) > 0) {
		return;
	}

	_sntprintf(szName, nLen, _T("Key 0x%02X"), nVK);
}

void HotkeyToString(UINT16 nKey, TCHAR* szString, INT32 nLen)
{
	szString[0] = 0;
	if ((nKey & 0xFF) == 0) {
		return;
	}

	TCHAR szKey[64];
	HotkeyKeyName(nKey & 0xFF, szKey, 64);

	_sntprintf(szString, nLen, _T("%s%s%s%s"),
		(nKey & HOTKEY_MOD_CTRL)  ? _T("Ctrl+")  : _T(""),
		(nKey & HOTKEY_MOD_ALT)   ? _T("Alt+")   : _T(""),
		(nKey & HOTKEY_MOD_SHIFT) ? _T("Shift+") : _T(""),
		szKey);
	szString[nLen - 1] = 0;
}

// ---------------------------------------------------------------------------
// Run-time matching (used by the message loop in run.cpp)

UINT32 HotkeyGetModifiers(const MSG* pMsg)
{
	UINT32 nMods = 0;

	if (GetKeyState(VK_SHIFT) & 0x8000)		nMods |= HOTKEY_MOD_SHIFT;
	if (GetKeyState(VK_CONTROL) & 0x8000)	nMods |= HOTKEY_MOD_CTRL;
	if (pMsg->lParam & 0x20000000)			nMods |= HOTKEY_MOD_ALT;		// context code: Alt is down

	return nMods;
}

INT32 HotkeyFindAction(UINT nVK, UINT32 nMods)
{
	HotkeysInit();

	if (nVK == 0 || nVK > 0xFF) return HOTKEY_ACT_NONE;

	UINT16 nKey = (UINT16)(nVK | nMods);

	for (INT32 i = 0; i < HOTKEY_COUNT; i++) {
		if (HotkeyDefs[i].nAction == HOTKEY_ACT_NONE) continue;
		if (nHotkeys[i][0] == nKey || nHotkeys[i][1] == nKey) {
			return HotkeyDefs[i].nAction;
		}
	}

	return HOTKEY_ACT_NONE;
}

// Is nVK (ignoring modifiers) bound to nAction? Used for key-up handling of "hold" hotkeys
bool HotkeyActionUsesKey(INT32 nAction, UINT nVK)
{
	HotkeysInit();

	if (nVK == 0 || nVK > 0xFF) return false;

	for (INT32 i = 0; i < HOTKEY_COUNT; i++) {
		if (HotkeyDefs[i].nAction != nAction) continue;
		for (INT32 j = 0; j < 2; j++) {
			if (nHotkeys[i][j] && (nHotkeys[i][j] & 0xFF) == nVK) {
				return true;
			}
		}
	}

	return false;
}

// ---------------------------------------------------------------------------
// Accelerator table + menu labels

static void HotkeysBuildAccelerators()
{
	ACCEL Accel[HOTKEY_COUNT * 2];
	INT32 nAccel = 0;

	for (INT32 i = 0; i < HOTKEY_COUNT; i++) {
		if (HotkeyDefs[i].nCommand == 0) continue;

		for (INT32 j = 0; j < 2; j++) {
			UINT16 nKey = nHotkeys[i][j];
			if ((nKey & 0xFF) == 0) continue;

			BYTE fVirt = FVIRTKEY | FNOINVERT;
			if (nKey & HOTKEY_MOD_SHIFT) fVirt |= FSHIFT;
			if (nKey & HOTKEY_MOD_CTRL)  fVirt |= FCONTROL;
			if (nKey & HOTKEY_MOD_ALT)   fVirt |= FALT;

			Accel[nAccel].fVirt = fVirt;
			Accel[nAccel].key   = (WORD)(nKey & 0xFF);
			Accel[nAccel].cmd   = (WORD)HotkeyDefs[i].nCommand;
			nAccel++;
		}
	}

	if (hAccel) {
		DestroyAcceleratorTable(hAccel);
		hAccel = NULL;
	}

	if (nAccel) {
		hAccel = CreateAcceleratorTable(Accel, nAccel);
	}
}

static void HotkeySetMenuLabel(INT32 nId, UINT16 nKey)
{
	if (hMenu == NULL || nId == 0) return;

	TCHAR szText[256] = { 0 };
	MENUITEMINFO mii;
	memset(&mii, 0, sizeof(mii));
	mii.cbSize     = sizeof(MENUITEMINFO);
	mii.fMask      = MIIM_STRING;
	mii.dwTypeData = szText;
	mii.cch        = 256;

	if (!GetMenuItemInfo(hMenu, nId, FALSE, &mii)) return;
	szText[255] = 0;

	TCHAR* pTab = _tcschr(szText, _T('\t'));
	if (pTab) *pTab = 0;

	if (nKey) {
		TCHAR szKey[64];
		HotkeyToString(nKey, szKey, 64);
		INT32 nUsed = _tcslen(szText);
		_sntprintf(szText + nUsed, 256 - nUsed, _T("\t%s"), szKey);
		szText[255] = 0;
	}

	memset(&mii, 0, sizeof(mii));
	mii.cbSize     = sizeof(MENUITEMINFO);
	mii.fMask      = MIIM_STRING;
	mii.dwTypeData = szText;
	mii.cch        = _tcslen(szText);
	SetMenuItemInfo(hMenu, nId, FALSE, &mii);
}

// Rewrite the shortcut part ("\tF5") of every menu item that has a hotkey
void HotkeysUpdateMenu()
{
	HotkeysInit();

	if (hMenu == NULL) return;

	for (INT32 i = 0; i < HOTKEY_COUNT; i++) {
		if (HotkeyDefs[i].nCommand == 0) continue;

		UINT16 nKey = nHotkeys[i][0] ? nHotkeys[i][0] : nHotkeys[i][1];

		HotkeySetMenuLabel(HotkeyDefs[i].nCommand, nKey);
		HotkeySetMenuLabel(HotkeyDefs[i].nMenuAlias[0], nKey);
		HotkeySetMenuLabel(HotkeyDefs[i].nMenuAlias[1], nKey);
	}
}

void HotkeysApply()
{
	HotkeysInit();
	HotkeysBuildAccelerators();
	HotkeysUpdateMenu();
}

// ---------------------------------------------------------------------------
// Config file

void HotkeysConfigLoadLine(TCHAR* szLine)
{
	static TCHAR szLabel[] = _T("Hotkey ");

	HotkeysInit();

	TCHAR* p = LabelCheck(szLine, szLabel);
	if (p == NULL) return;

	while (*p == _T(' ') || *p == _T('\t')) p++;

	TCHAR szName[64];
	INT32 n = 0;
	while (*p && *p != _T(' ') && *p != _T('\t') && n < 63) {
		szName[n++] = *p++;
	}
	szName[n] = 0;

	TCHAR* pEnd = NULL;
	UINT32 nKey0 = _tcstoul(p, &pEnd, 0);
	if (pEnd == p) return;
	UINT32 nKey1 = _tcstoul(pEnd, NULL, 0);

	for (INT32 i = 0; i < HOTKEY_COUNT; i++) {
		if (_tcsicmp(szName, HotkeyDefs[i].szName) == 0) {
			nHotkeys[i][0] = (UINT16)(nKey0 & HOTKEY_KEY_MASK);
			nHotkeys[i][1] = (UINT16)(nKey1 & HOTKEY_KEY_MASK);
			break;
		}
	}
}

void HotkeysConfigSave(FILE* h)
{
	HotkeysInit();

	_ftprintf(h, _T("\n\n\n"));
	_ftprintf(h, _T("// --- Emulator hotkeys --------------------------------------------------------\n"));
	_ftprintf(h, _T("\n// Hotkey <function> <key> <alternate key>  (use Input -> Configure emulator hotkeys to change these)\n"));
	_ftprintf(h, _T("// Keys are 0xMMVV: VV = Windows virtual-key code, MM = modifiers (1 = Shift, 2 = Ctrl, 4 = Alt), 0 = not assigned\n"));

	for (INT32 i = 0; i < HOTKEY_COUNT; i++) {
		_ftprintf(h, _T("Hotkey %-22s 0x%04X 0x%04X\n"), HotkeyDefs[i].szName, nHotkeys[i][0], nHotkeys[i][1]);
	}
}

// ---------------------------------------------------------------------------
// Dialog

#define HK_TIMER_ID			1
#define HK_TIMER_MS			15
#define HK_CAPTURE_TIMEOUT	5000

enum { CAPTURE_IDLE = 0, CAPTURE_WAIT_RELEASE, CAPTURE_LISTEN, CAPTURE_GOT_KEY };

static UINT16 nEditKeys[HOTKEY_COUNT][2];
static HWND hHkDlg = NULL;
static HWND hHkList = NULL;
static INT32 nCaptureState = CAPTURE_IDLE;
static INT32 nCaptureRow = -1;
static INT32 nCaptureSlot = 0;
static UINT16 nCapturedKey = 0;
static DWORD nCaptureStart = 0;
static INT32 nCaptureSecondsShown = -1;

static const INT32 nHkButtons[] = { IDC_HOTKEYS_SET, IDC_HOTKEYS_SETALT, IDC_HOTKEYS_CLEAR, IDC_HOTKEYS_DEFAULT, IDC_HOTKEYS_RESETALL, IDOK, IDCANCEL };

// Keys that can't be used as a hotkey: mouse buttons, modifiers, lock keys, Windows keys, IME keys
static bool HotkeyVKIsAssignable(UINT nVK)
{
	if (nVK < 0x08) return false;
	switch (nVK) {
		case VK_SHIFT: case VK_CONTROL: case VK_MENU:
		case VK_LSHIFT: case VK_RSHIFT: case VK_LCONTROL: case VK_RCONTROL: case VK_LMENU: case VK_RMENU:
		case VK_LWIN: case VK_RWIN: case VK_APPS:
		case VK_CAPITAL: case VK_NUMLOCK:
		case 0xE5: /* VK_PROCESSKEY */
		case 0xE7: /* VK_PACKET */
		case 0xFF:
			return false;
	}
	if (nVK >= 0x15 && nVK <= 0x1A) return false;		// IME
	if (nVK >= 0x1C && nVK <= 0x1F) return false;		// IME
	return true;
}

static UINT HotkeyFirstKeyDown()
{
	for (UINT vk = 0x08; vk <= 0xFE; vk++) {
		if (HotkeyVKIsAssignable(vk) && (GetAsyncKeyState(vk) & 0x8000)) {
			return vk;
		}
	}
	return 0;
}

static UINT32 HotkeyAsyncModifiers()
{
	UINT32 nMods = 0;
	if (GetAsyncKeyState(VK_SHIFT)   & 0x8000) nMods |= HOTKEY_MOD_SHIFT;
	if (GetAsyncKeyState(VK_CONTROL) & 0x8000) nMods |= HOTKEY_MOD_CTRL;
	if (GetAsyncKeyState(VK_MENU)    & 0x8000) nMods |= HOTKEY_MOD_ALT;
	return nMods;
}

static bool HotkeyIsReserved(UINT16 nKey)
{
	UINT nVK = nKey & 0xFF;
	UINT32 nMods = nKey & HOTKEY_MOD_MASK;

	if (nMods == HOTKEY_MOD_ALT  && (nVK == VK_F4 || nVK == VK_TAB || nVK == VK_ESCAPE || nVK == VK_SPACE)) return true;
	if (nMods == HOTKEY_MOD_CTRL && nVK == VK_ESCAPE) return true;

	return false;
}

static void HkUpdateRow(INT32 nRow)
{
	TCHAR szKey[64];

	HotkeyToString(nEditKeys[nRow][0], szKey, 64);
	ListView_SetItemText(hHkList, nRow, 2, szKey);
	HotkeyToString(nEditKeys[nRow][1], szKey, 64);
	ListView_SetItemText(hHkList, nRow, 3, szKey);
}

static void HkFillList()
{
	ListView_DeleteAllItems(hHkList);

	for (INT32 i = 0; i < HOTKEY_COUNT; i++) {
		LVITEM lvi;
		memset(&lvi, 0, sizeof(lvi));
		lvi.mask     = LVIF_TEXT | LVIF_PARAM;
		lvi.iItem    = i;
		lvi.iSubItem = 0;
		lvi.pszText  = (TCHAR*)HotkeyDefs[i].szCategory;
		lvi.lParam   = i;
		ListView_InsertItem(hHkList, &lvi);

		ListView_SetItemText(hHkList, i, 1, (TCHAR*)HotkeyDefs[i].szDesc);
		HkUpdateRow(i);
	}
}

static INT32 HkSelectedRow()
{
	return ListView_GetNextItem(hHkList, -1, LVNI_SELECTED);
}

static void HkSetStatus(const TCHAR* szText)
{
	SetDlgItemText(hHkDlg, IDC_HOTKEYS_STATUS, szText);
}

static void HkSetDefaultStatus()
{
	HkSetStatus(_T("Select a function, then click Set Key (or double-click it). Del clears the selected function."));
}

static void HkUpdateButtons()
{
	BOOL bSel = (HkSelectedRow() >= 0);

	EnableWindow(GetDlgItem(hHkDlg, IDC_HOTKEYS_SET),     bSel);
	EnableWindow(GetDlgItem(hHkDlg, IDC_HOTKEYS_SETALT),  bSel);
	EnableWindow(GetDlgItem(hHkDlg, IDC_HOTKEYS_CLEAR),   bSel);
	EnableWindow(GetDlgItem(hHkDlg, IDC_HOTKEYS_DEFAULT), bSel);
}

static void HkEnableControls(BOOL bEnable)
{
	EnableWindow(hHkList, bEnable);
	for (INT32 i = 0; i < (INT32)(sizeof(nHkButtons) / sizeof(nHkButtons[0])); i++) {
		EnableWindow(GetDlgItem(hHkDlg, nHkButtons[i]), bEnable);
	}
	if (bEnable) {
		HkUpdateButtons();
	}
}

static void HkShowCaptureStatus()
{
	INT32 nSecondsLeft = (INT32)((HK_CAPTURE_TIMEOUT - (GetTickCount() - nCaptureStart) + 999) / 1000);
	if (nSecondsLeft < 0) nSecondsLeft = 0;

	if (nSecondsLeft == nCaptureSecondsShown) return;
	nCaptureSecondsShown = nSecondsLeft;

	TCHAR szText[256];
	_sntprintf(szText, 256, _T("Press the %s for \"%s\" now... (cancels in %d s)"),
		nCaptureSlot ? _T("alternate key") : _T("key"), HotkeyDefs[nCaptureRow].szDesc, nSecondsLeft);
	szText[255] = 0;
	HkSetStatus(szText);
}

static void HkEndCapture()
{
	KillTimer(hHkDlg, HK_TIMER_ID);
	nCaptureState = CAPTURE_IDLE;

	HkEnableControls(TRUE);
	if (nCaptureRow >= 0) {
		ListView_SetItemState(hHkList, nCaptureRow, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		ListView_EnsureVisible(hHkList, nCaptureRow, FALSE);
	}
	SetFocus(hHkList);
	HkSetDefaultStatus();
}

static void HkStartCapture(INT32 nRow, INT32 nSlot)
{
	if (nRow < 0 || nRow >= HOTKEY_COUNT || nCaptureState != CAPTURE_IDLE) return;

	nCaptureRow          = nRow;
	nCaptureSlot         = nSlot;
	nCapturedKey         = 0;
	nCaptureStart        = GetTickCount();
	nCaptureSecondsShown = -1;
	nCaptureState        = CAPTURE_WAIT_RELEASE;		// first wait until nothing is held (e.g. the key used to press the button)

	HkEnableControls(FALSE);
	SetFocus(hHkDlg);
	HkShowCaptureStatus();

	SetTimer(hHkDlg, HK_TIMER_ID, HK_TIMER_MS, NULL);
}

// Called once the captured key combination has been fully released
static void HkCommitCapture()
{
	INT32 nRow  = nCaptureRow;
	INT32 nSlot = nCaptureSlot;
	UINT16 nKey = nCapturedKey;

	HkEndCapture();

	TCHAR szKey[64];
	HotkeyToString(nKey, szKey, 64);

	if (HotkeyIsReserved(nKey)) {
		TCHAR szMsg[256];
		_sntprintf(szMsg, 256, _T("%s is reserved by Windows and can't be used as a hotkey."), szKey);
		szMsg[255] = 0;
		MessageBox(hHkDlg, szMsg, _T(APP_TITLE), MB_OK | MB_ICONWARNING);
		return;
	}

	// Already in use?
	for (INT32 i = 0; i < HOTKEY_COUNT; i++) {
		for (INT32 j = 0; j < 2; j++) {
			if ((i == nRow && j == nSlot) || nEditKeys[i][j] != nKey) continue;

			if (i != nRow) {
				TCHAR szMsg[512];
				_sntprintf(szMsg, 512, _T("%s is already assigned to \"%s\".\n\nAssign it to \"%s\" instead?"),
					szKey, HotkeyDefs[i].szDesc, HotkeyDefs[nRow].szDesc);
				szMsg[511] = 0;
				if (MessageBox(hHkDlg, szMsg, _T(APP_TITLE), MB_YESNO | MB_ICONQUESTION) != IDYES) {
					return;
				}
			}

			nEditKeys[i][j] = 0;
			HkUpdateRow(i);
		}
	}

	nEditKeys[nRow][nSlot] = nKey;
	HkUpdateRow(nRow);
}

static void HkOnTimer()
{
	UINT nVK = HotkeyFirstKeyDown();
	UINT32 nMods = HotkeyAsyncModifiers();

	switch (nCaptureState) {
		case CAPTURE_WAIT_RELEASE:
			if (nVK == 0 && nMods == 0) {
				nCaptureState = CAPTURE_LISTEN;
			}
			// fall through - the timeout applies here too
		case CAPTURE_LISTEN:
			if (nCaptureState == CAPTURE_LISTEN && nVK) {
				nCapturedKey  = (UINT16)(nVK | nMods);
				nCaptureState = CAPTURE_GOT_KEY;

				TCHAR szKey[64], szText[128];
				HotkeyToString(nCapturedKey, szKey, 64);
				_sntprintf(szText, 128, _T("%s - release the key(s) to confirm."), szKey);
				szText[127] = 0;
				HkSetStatus(szText);
				break;
			}
			if (GetTickCount() - nCaptureStart > HK_CAPTURE_TIMEOUT) {
				HkEndCapture();
				break;
			}
			HkShowCaptureStatus();
			break;

		case CAPTURE_GOT_KEY:
			// Wait until everything is released before committing, so the keypress can't
			// leak into the dialog (Enter/Esc closing it) or into a confirmation message box
			if (nVK == 0 && nMods == 0) {
				HkCommitCapture();
			}
			break;
	}
}

static void HkInitList()
{
	ListView_SetExtendedListViewStyle(hHkList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

	RECT rc;
	GetClientRect(hHkList, &rc);
	INT32 nWidth = rc.right - rc.left - GetSystemMetrics(SM_CXVSCROLL) - 2;

	const TCHAR* szColumns[4] = { _T("Category"), _T("Function"), _T("Key"), _T("Alternate") };
	const INT32 nColumnWidth[4] = { nWidth * 15 / 100, nWidth * 47 / 100, nWidth * 19 / 100, 0 };

	for (INT32 i = 0; i < 4; i++) {
		LVCOLUMN lvc;
		memset(&lvc, 0, sizeof(lvc));
		lvc.mask     = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
		lvc.cx       = (i == 3) ? (nWidth - nColumnWidth[0] - nColumnWidth[1] - nColumnWidth[2]) : nColumnWidth[i];
		lvc.pszText  = (TCHAR*)szColumns[i];
		lvc.iSubItem = i;
		ListView_InsertColumn(hHkList, i, &lvc);
	}

	HkFillList();
}

static INT_PTR CALLBACK HotkeysDlgProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	switch (Msg) {
		case WM_INITDIALOG: {
			hHkDlg  = hDlg;
			hHkList = GetDlgItem(hDlg, IDC_HOTKEYS_LIST);

			nCaptureState = CAPTURE_IDLE;
			nCaptureRow   = -1;
			memcpy(nEditKeys, nHotkeys, sizeof(nEditKeys));

			HkInitList();
			HkUpdateButtons();
			HkSetDefaultStatus();

			WndInMid(hDlg, hScrnWnd);
			SetFocus(hHkList);
			return FALSE;		// we set the focus
		}

		case WM_TIMER:
			if (wParam == HK_TIMER_ID && nCaptureState != CAPTURE_IDLE) {
				HkOnTimer();
			}
			return TRUE;

		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
			// Clicking the dialog cancels a capture that hasn't got a key yet
			if (nCaptureState == CAPTURE_WAIT_RELEASE || nCaptureState == CAPTURE_LISTEN) {
				HkEndCapture();
			}
			break;

		case WM_SYSCOMMAND:
			// Don't let Alt / F10 open the window menu while we're reading a key
			if (nCaptureState != CAPTURE_IDLE && (wParam & 0xFFF0) == SC_KEYMENU) {
				SetWindowLongPtr(hDlg, DWLP_MSGRESULT, 0);
				return TRUE;
			}
			break;

		case WM_NOTIFY: {
			NMHDR* pNmh = (NMHDR*)lParam;
			if (pNmh->idFrom != IDC_HOTKEYS_LIST || nCaptureState != CAPTURE_IDLE) break;

			switch (pNmh->code) {
				case NM_DBLCLK: {
					NMITEMACTIVATE* pAct = (NMITEMACTIVATE*)lParam;
					if (pAct->iItem >= 0) {
						HkStartCapture(pAct->iItem, (pAct->iSubItem == 3) ? 1 : 0);
					}
					break;
				}
				case LVN_KEYDOWN: {
					NMLVKEYDOWN* pKey = (NMLVKEYDOWN*)lParam;
					INT32 nRow = HkSelectedRow();
					if (pKey->wVKey == VK_DELETE && nRow >= 0) {
						nEditKeys[nRow][0] = nEditKeys[nRow][1] = 0;
						HkUpdateRow(nRow);
					}
					break;
				}
				case LVN_ITEMCHANGED:
					HkUpdateButtons();
					break;
			}
			break;
		}

		case WM_COMMAND: {
			// While reading a key, Enter / Esc / Alt+F4 reach us as IDOK / IDCANCEL - ignore them
			if (nCaptureState != CAPTURE_IDLE) {
				return TRUE;
			}

			INT32 nRow = HkSelectedRow();

			switch (LOWORD(wParam)) {
				case IDC_HOTKEYS_SET:
					HkStartCapture(nRow, 0);
					break;
				case IDC_HOTKEYS_SETALT:
					HkStartCapture(nRow, 1);
					break;
				case IDC_HOTKEYS_CLEAR:
					if (nRow >= 0) {
						nEditKeys[nRow][0] = nEditKeys[nRow][1] = 0;
						HkUpdateRow(nRow);
					}
					break;
				case IDC_HOTKEYS_DEFAULT:
					if (nRow >= 0) {
						// Take the defaults back from any other function that is using them now
						for (INT32 d = 0; d < 2; d++) {
							UINT16 nKey = HotkeyDefs[nRow].nDefault[d];
							if (nKey == 0) continue;
							for (INT32 i = 0; i < HOTKEY_COUNT; i++) {
								for (INT32 j = 0; j < 2; j++) {
									if (i != nRow && nEditKeys[i][j] == nKey) {
										nEditKeys[i][j] = 0;
										HkUpdateRow(i);
									}
								}
							}
						}
						nEditKeys[nRow][0] = HotkeyDefs[nRow].nDefault[0];
						nEditKeys[nRow][1] = HotkeyDefs[nRow].nDefault[1];
						HkUpdateRow(nRow);
					}
					break;
				case IDC_HOTKEYS_RESETALL:
					if (MessageBox(hDlg, _T("Reset all hotkeys to their default keys?"), _T(APP_TITLE), MB_YESNO | MB_ICONQUESTION) == IDYES) {
						for (INT32 i = 0; i < HOTKEY_COUNT; i++) {
							nEditKeys[i][0] = HotkeyDefs[i].nDefault[0];
							nEditKeys[i][1] = HotkeyDefs[i].nDefault[1];
							HkUpdateRow(i);
						}
					}
					break;
				case IDOK:
					memcpy(nHotkeys, nEditKeys, sizeof(nHotkeys));
					HotkeysApply();
					EndDialog(hDlg, 1);
					break;
				case IDCANCEL:
					EndDialog(hDlg, 0);
					break;
			}
			return TRUE;
		}

		case WM_CLOSE:
			if (nCaptureState == CAPTURE_IDLE) {
				EndDialog(hDlg, 0);
			}
			return TRUE;

		case WM_DESTROY:
			KillTimer(hDlg, HK_TIMER_ID);
			nCaptureState = CAPTURE_IDLE;
			hHkDlg  = NULL;
			hHkList = NULL;
			break;
	}

	return FALSE;
}

int HotkeysCreate()
{
	HotkeysInit();

	return (int)FBADialogBox(hAppInst, MAKEINTRESOURCE(IDD_HOTKEYS), hScrnWnd, (DLGPROC)HotkeysDlgProc);
}
