#ifndef _BURNER_QT_H
#define _BURNER_QT_H

typedef unsigned char BYTE;
/*
typedef unsigned short WORD;
*/
typedef unsigned long DWORD;

extern int bDrvOkay;
extern int bRunPause;
extern bool bAlwaysProcessKeyboardInput;
extern int nAppVirtualFps;

// main.cpp
extern bool AppProcessKeyboardInput();
extern void InpDIPSWResetDIPs (void);
extern void IpsApplyPatches(UINT8 *, char *);
extern void Reinitialise(void);
extern TCHAR *GetIsoPath();
extern int VidRecalcPal();
extern void wav_pause(bool bResume);

extern TCHAR szAppRomPaths[DIRS_MAX][MAX_PATH];
// drv.cpp
int DrvInit(int nDrvNum, bool bRestore);
int DrvInitCallback();
int DrvExit();

// progress.cpp
int ProgressUpdateBurner(double dProgress, const TCHAR* pszText, bool bAbs);
void ProgressCreate();
void ProgressDestroy();
bool ProgressIsRunning();

#ifdef _MSC_VER
#define snprintf _snprintf
#define ANSIToTCHAR(str, foo, bar) (str)
#endif

extern char *TCHARToANSI(const TCHAR* pszInString, char* pszOutString, int nOutSize);


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

extern StringSet BzipText;
extern StringSet BzipDetail;

#endif
