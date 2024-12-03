// Header for SDL 1.2 & SDL2
#include <SDL.h>

#ifdef BUILD_SDL2
#include "sdl2_gui.h"
#include "sdl2_inprint.h"
#endif

// defines to override various #ifndef _WIN32
#ifndef _WIN32
typedef struct tagRECT
{
	int left;
	int top;
	int right;
	int bottom;
} RECT, * PRECT, * LPRECT;
typedef const RECT* LPCRECT;
#else
#include <windows.h>
#endif

typedef unsigned long   DWORD;
typedef unsigned char   BYTE;

#ifndef MAX_PATH
#define MAX_PATH    511
#endif

#ifndef __cdecl
#define __cdecl
#endif

//main.cpp
int SetBurnHighCol(int nDepth);

extern int   nAppVirtualFps;
extern bool  bRunPause;
extern bool  bAppDoFast;    // TODO: bad
extern char  fpsstring[20]; // TODO: also bad
extern bool  bAppShowFPS;   // TODO: Also also bad
extern bool  bAlwaysProcessKeyboardInput;
extern TCHAR szAppBurnVer[16];
extern bool  bAppFullscreen;
extern bool bIntegerScale;
extern int  nWindowScale;
extern bool bAlwaysMenu;
extern int 	nGameSelect;
extern int 	nFilterSelect;
extern bool bShowAvailableOnly;
extern bool bShowClones;
extern int gameSelectedFromFilter;

#ifdef BUILD_SDL2
extern SDL_Window* sdlWindow;
extern int nJoystickCount;
#endif
extern TCHAR* GetIsoPath();

TCHAR* ANSIToTCHAR(const char* pszInString, TCHAR* pszOutString, int nOutSize);
char* TCHARToANSI(const TCHAR* pszInString, char* pszOutString, int nOutSize);

#define _TtoA(a)    TCHARToANSI(a, NULL, 0)
#define _AtoT(a)    ANSIToTCHAR(a, NULL, 0)


bool AppProcessKeyboardInput();

//config.cpp
int ConfigAppLoad();
int ConfigAppSave();

// drv.cpp
extern int  bDrvOkay; // 1 if the Driver has been initted okay, and it's okay to use the BurnDrv functions
extern char szAppRomPaths[DIRS_MAX][MAX_PATH];
int DrvInit(int nDrvNum, bool bRestore);
int DrvInitCallback(); // Used when Burn library needs to load a game. DrvInit(nBurnSelect, false)
int DrvExit();
int ProgressUpdateBurner(double dProgress, const TCHAR* pszText, bool bAbs);
int AppError(TCHAR* szText, int bWarning);

//run.cpp
extern int RunMessageLoop();
extern int RunReset();

#define MESSAGE_MAX_FRAMES 180 // assuming 60fps this would be 3 seconds...
#define MESSAGE_MAX_LENGTH 255

extern UINT32 messageFrames;
extern char lastMessage[MESSAGE_MAX_LENGTH];

void UpdateMessage(char* message);
int StatedAuto(int bSave);

// media.cpp
int MediaInit();
int MediaExit();

//inpdipsw.cpp
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

//interface/inp_interface.cpp
int InputInit();
int InputExit();
int InputMake(bool bCopy);

// stated.cpp
int QuickState(int bSave);
extern int bDrvSaveAll;

//stringset.cpp
class StringSet {
public:
	TCHAR* szText;
	int    nLen;
	// printf function to add text to the Bzip string
	int __cdecl Add(TCHAR* szFormat, ...);
	int Reset();

	StringSet();
	~StringSet();
};

// support_paths.cpp
extern TCHAR szAppListsPath[MAX_PATH];
extern TCHAR szAppDatListsPath[MAX_PATH];
extern TCHAR szAppArchivesPath[MAX_PATH];
