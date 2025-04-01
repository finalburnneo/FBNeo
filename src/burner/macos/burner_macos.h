#ifndef BURNER_MACOS_H
#define BURNER_MACOS_H

#define stricmp strcasecmp

// defines to override various #ifndef _WIN32
typedef struct tagRECT {
    int left;
    int top;
    int right;
    int bottom;
} RECT,*PRECT,*LPRECT;
typedef const RECT *LPCRECT;

typedef unsigned int DWORD;
typedef unsigned char BYTE;

#ifndef MAX_PATH
#define MAX_PATH 511
#endif

//main.cpp
int SetBurnHighCol(int nDepth);
extern int nAppVirtualFps;
extern bool bRunPause;
extern bool bAlwaysProcessKeyboardInput;
TCHAR* ANSIToTCHAR(const char* pszInString, TCHAR* pszOutString, int nOutSize);
char* TCHARToANSI(const TCHAR* pszInString, char* pszOutString, int nOutSize);
#define _TtoA(a)	TCHARToANSI(a, NULL, 0)
#define _AtoT(a)	ANSIToTCHAR(a, NULL, 0)
bool AppProcessKeyboardInput();

// drv.cpp
extern int bDrvOkay; // 1 if the Driver has been initted okay, and it's okay to use the BurnDrv functions
extern char szAppRomPaths[DIRS_MAX][MAX_PATH];
int DrvInit(int nDrvNum, bool bRestore);
int DrvInitCallback(); // Used when Burn library needs to load a game. DrvInit(nBurnSelect, false)
int DrvExit();
int ProgressUpdateBurner(double dProgress, const TCHAR* pszText, bool bAbs);
int AppError(TCHAR* szText, int bWarning);

//run.cpp
extern int RunReset();
#define MESSAGE_MAX_FRAMES 180 // assuming 60fps this would be 3 seconds...
#define MESSAGE_MAX_LENGTH 255

extern UINT32 messageFrames;
extern char lastMessage[MESSAGE_MAX_LENGTH];
void UpdateMessage(char* message);

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

//TODO:
#define szAppBurnVer "1"

//stringset.cpp
class StringSet {
public:
    TCHAR* szText;
    int nLen;
    // printf function to add text to the Bzip string
    int __cdecl Add(TCHAR* szFormat, ...);
    int Reset();
    StringSet();
    ~StringSet();
};

// SDL substitutes
unsigned int SDL_GetTicks();
void SDL_Delay(unsigned int ms);

#endif
