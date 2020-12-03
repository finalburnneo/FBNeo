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
extern bool bAlwaysMenu;
#ifdef BUILD_SDL2
extern SDL_Window* sdlWindow;
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
void InpDIPSWResetDIPs();

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
extern TCHAR szAppPreviewsPath[MAX_PATH];
extern TCHAR szAppTitlesPath[MAX_PATH];
extern TCHAR szAppSelectPath[MAX_PATH];
extern TCHAR szAppVersusPath[MAX_PATH];
extern TCHAR szAppHowtoPath[MAX_PATH];
extern TCHAR szAppScoresPath[MAX_PATH];
extern TCHAR szAppBossesPath[MAX_PATH];
extern TCHAR szAppGameoverPath[MAX_PATH];
extern TCHAR szAppFlyersPath[MAX_PATH];
extern TCHAR szAppMarqueesPath[MAX_PATH];
extern TCHAR szAppControlsPath[MAX_PATH];
extern TCHAR szAppCabinetsPath[MAX_PATH];
extern TCHAR szAppPCBsPath[MAX_PATH];
extern TCHAR szAppCheatsPath[MAX_PATH];
extern TCHAR szAppHistoryPath[MAX_PATH];
extern TCHAR szAppListsPath[MAX_PATH];
extern TCHAR szAppDatListsPath[MAX_PATH];
extern TCHAR szAppIpsPath[MAX_PATH];
extern TCHAR szAppIconsPath[MAX_PATH];
extern TCHAR szAppArchivesPath[MAX_PATH];
