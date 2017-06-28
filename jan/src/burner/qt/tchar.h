#ifndef __PORT_TYPEDEFS_H
#define __PORT_TYPEDEFS_H

#include <stdint.h>
#include <wchar.h>

#include "inp_keys.h"
#define TCHAR char
#define _T(x) x
#define _tfopen fopen
#define _tcstol strtol
#define _tcsstr strstr
#define _istspace(x) isspace(x)
#define _stprintf sprintf
#define _tcslen strlen
#define _tcsicmp(a, b) strcasecmp(a, b)
#define _tcscpy(to, from) strcpy(to, from)
#define _fgetts fgets
#define _strnicmp(s1, s2, n) strncasecmp(s1, s2, n)
#define _sntprintf  snprintf
#define _tcscmp     strcmp
#define _tcsncmp strncmp
#define _tcsncpy strncpy
#define _stscanf sscanf
#define _ftprintf fprintf

#ifdef _MSC_VER
#include <tchar.h>
#define strncasecmp(s1, s2, n) _strnicmp(s1, s2, n)
#define strcasecmp(x, y) _stricmp(x, y)
#define snprintf _snprintf
#else
#define _stricmp(x, y) strcasecmp(x,y)

typedef struct { int x, y, width, height; } RECT;
#undef __cdecl
#define __cdecl

//#define bprintf(...) {}
#endif

#undef __fastcall
#undef _fastcall
#define __fastcall			/*what does this correspond to?*/
#define _fastcall			/*same as above - what does this correspond to?*/
#define ANSIToTCHAR(str, foo, bar) (str)

/* for Windows / Xbox 360 (below VS2010) - typedefs for missing stdint.h types such as uintptr_t?*/

/*FBA defines*/
#define PUF_TEXT_NO_TRANSLATE	(0)
#define PUF_TYPE_ERROR		(1)

extern TCHAR szAppBurnVer[16];

typedef int HWND;

extern int bDrvOkay;
extern int bRunPause;
#ifdef __cplusplus
extern bool bAlwaysProcessKeyboardInput;
#endif
extern HWND hScrnWnd;		// Handle to the screen window

extern void InpDIPSWResetDIPs (void);

#endif
