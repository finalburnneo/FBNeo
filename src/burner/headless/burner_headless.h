// Platform burner header for the headless FinalBurn Neo frontend.
// Author: Leonardo Kasperavičius
//
// Built originally to support OpenEmu-Silicon
// (https://github.com/OpenEmu-Silicon/OpenEmu-Silicon).
//
// Declares the types, globals and frontend prototypes the shared burner code
// expects (the headless counterpart of burner_sdl.h / burner_macos.h), without
// any SDL or GUI dependencies. Subject to the FB Neo license (see src/license.txt).

#ifndef BURNER_HEADLESS_PLATFORM_H
#define BURNER_HEADLESS_PLATFORM_H

// Win32 type aliases the shared code relies on.
#ifndef _WIN32
typedef struct tagRECT
{
	int left;
	int top;
	int right;
	int bottom;
} RECT, * PRECT, * LPRECT;
typedef const RECT* LPCRECT;
typedef UINT32 DWORD;
typedef UINT8 BYTE;
#else
#include <windows.h>
#endif

#ifndef MAX_PATH
#define MAX_PATH    511
#endif

#ifndef __cdecl
#define __cdecl
#endif

// misc.cpp
int SetBurnHighCol(int nDepth);

extern int   nAppVirtualFps;
extern bool  bRunPause;
extern bool  bAppDoFast;
extern char  fpsstring[20];
extern bool  bAppShowFPS;
extern bool  bAlwaysProcessKeyboardInput;
extern TCHAR szAppBurnVer[16];
extern bool  bAppFullscreen;
extern bool  bIntegerScale;
extern int   nWindowScale;
extern bool  bAlwaysMenu;
extern int   nGameSelect;
extern int   nFilterSelect;
extern bool  bShowAvailableOnly;
extern bool  bShowClones;
extern int   gameSelectedFromFilter;
extern TCHAR* GetIsoPath();

TCHAR* ANSIToTCHAR(const char* pszInString, TCHAR* pszOutString, int nOutSize);
char* TCHARToANSI(const TCHAR* pszInString, char* pszOutString, int nOutSize);

#define _TtoA(a)    TCHARToANSI(a, NULL, 0)
#define _AtoT(a)    ANSIToTCHAR(a, NULL, 0)

bool AppProcessKeyboardInput();

// config
int ConfigAppLoad();
int ConfigAppSave();

// drv.cpp (headless)
extern int  bDrvOkay;
extern char szAppRomPaths[DIRS_MAX][MAX_PATH];
int DrvInit(int nDrvNum, bool bRestore);
int DrvInitCallback();
int DrvExit();
int ProgressUpdateBurner(double dProgress, const TCHAR* pszText, bool bAbs);
int AppError(TCHAR* szText, int bWarning);

// run
extern int RunMessageLoop();
extern int RunReset();

#define MESSAGE_MAX_FRAMES 180
#define MESSAGE_MAX_LENGTH 255

extern UINT32 messageFrames;
extern char lastMessage[MESSAGE_MAX_LENGTH];

void UpdateMessage(char* message);
int StatedAuto(int bSave);

// media.cpp
int MediaInit();
int MediaExit();

// inpdipsw.cpp
#define DIP_MAX_NAME 64
#define MAXDIPSWITCHES 32
#define MAXDIPOPTIONS 32
bool setDIPSwitchOption(int dipgroup, int dipoption);
int InpDIPSWCreate();
void InpDIPSWResetDIPs();

struct GroupOfDIPSwitches
{
	BurnDIPInfo dipSwitch;
	UINT16 DefaultDIPOption;
	UINT16 SelectedDIPOption;
	char OptionsNamesWithCheckBoxes[MAXDIPOPTIONS][DIP_MAX_NAME];
	BurnDIPInfo dipSwitchesOptions[MAXDIPOPTIONS];
};

// inp_interface.cpp
int InputInit();
int InputExit();
int InputMake(bool bCopy);

// stated.cpp
int QuickState(int bSave);
extern int bDrvSaveAll;

// stringset.cpp
class StringSet {
public:
	TCHAR* szText;
	int    nLen;
	int __cdecl Add(TCHAR* szFormat, ...);
	int Reset();

	StringSet();
	~StringSet();
};

// support_paths.cpp
extern TCHAR szAppListsPath[MAX_PATH];
extern TCHAR szAppDatListsPath[MAX_PATH];
extern TCHAR szAppArchivesPath[MAX_PATH];

#endif // BURNER_HEADLESS_PLATFORM_H
