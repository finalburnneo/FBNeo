#include <stdint.h>
#ifdef _UNICODE
//TODO: bah
#include <wchar.h>
#define __TEXT(q)    L ## q

typedef wchar_t   TCHAR;
typedef wchar_t   _TCHAR;

#else

#define __TEXT(q)    q

#ifndef RC_INVOKED
typedef char   TCHAR;
typedef char   _TCHAR;
#endif

#define _tcslen        strlen
#define _tcscpy        strcpy
#define _tcsncpy       strncpy

#define _tprintf       printf
#define _vstprintf     vsprintf
#define _vsntprintf    vsnprintf
#define _stprintf      sprintf
#define _sntprintf     snprintf
#define _ftprintf      fprintf
#define _tsprintf      sprintf

#define _tcscmp        strcmp
#define _tcsncmp       strncmp
#define _tcsicmp       strcasecmp
#define _tcsnicmp      strncasecmp
#define _tcstol        strtol
#define _tcsrchr       strrchr
#define _tcsstr        strstr

#define _fgetts        fgets
#define _fputts        fputs
#define _fputtc        fputc

#define _istspace      isspace

#define _tfopen        fopen

#define _stricmp       strcasecmp
#define stricmp        strcasecmp
#define _strnicmp      strncmp

// FBA function, change this!
#define dprintf        printf

#endif

#define _TEXT(x)    __TEXT(x)
#define _T(x)       __TEXT(x)
