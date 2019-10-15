#ifndef BURNER_MACOS_H
#define BURNER_MACOS_H

typedef struct tagRECT {
	int left;
	int top;
	int right;
	int bottom;
} RECT,*PRECT,*LPRECT;
typedef const RECT *LPCRECT;

typedef unsigned long DWORD;
typedef unsigned char BYTE;

#ifndef __cdecl
#define	__cdecl
#endif

//main.cpp
extern int nAppVirtualFps;
extern bool bRunPause;
extern bool bAlwaysProcessKeyboardInput;
TCHAR* ANSIToTCHAR(const char* pszInString, TCHAR* pszOutString, int nOutSize);
char* TCHARToANSI(const TCHAR* pszInString, char* pszOutString, int nOutSize);
bool AppProcessKeyboardInput();

// drv.cpp
extern int bDrvOkay; // 1 if the Driver has been initted okay, and it's okay to use the BurnDrv functions
int DrvInit(int nDrvNum, bool bRestore);
int DrvExit();

//inpdipsw.cpp
void InpDIPSWResetDIPs();

#define stricmp strcasecmp

//TODO:
#define szAppBurnVer "1"

#endif
