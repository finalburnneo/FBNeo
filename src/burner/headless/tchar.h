#ifndef TCHAR_H
#define TCHAR_H

// Always available so non-_UNICODE code that still handles wide buffers
// (e.g. burn.cpp zip-name parsing) can call wcslen/wcscpy.
#include <wchar.h>

#ifdef _UNICODE

#include <wchar.h>

#define	__TEXT(q)	L##q

typedef	wchar_t	TCHAR;
typedef wchar_t _TCHAR;

#else /* ifndef _UNICODE */

#define	__TEXT(q)	q

typedef char	TCHAR;
typedef char	_TCHAR;

//#define wcslen(void)

#define _tcslen     strlen
#define _tcscpy     strcpy
#define _tcsncpy    strncpy

#define _tprintf    printf
#define _vstprintf  vsprintf
#define _vsntprintf vsnprintf
#define _stprintf   sprintf
#define _sntprintf  snprintf
#define _ftprintf   fprintf
#define _tsprintf   sprintf

#define _tcscmp     strcmp
#define _tcsncmp    strncmp
#define _tcsicmp    strcasecmp
#define _tcsnicmp   strncasecmp
#define _tcstol     strtol
#define _tcsrchr    strrchr
#define _tcsstr     strstr

#define _tcsstr     strstr

#define _fgetts     fgets
#define _fputts     fputs

#define	_istspace	isspace

#define _tfopen     fopen

#define _stricmp strcmp
#define _strnicmp strncmp

// Win32 string/char shims the shared code uses on POSIX builds.
#include <ctype.h>
#define stricmp     strcasecmp
#define strnicmp    strncasecmp
#define _totlower(c) tolower((unsigned char)(c))
#define _totupper(c) toupper((unsigned char)(c))

#define dprintf(...) fprintf (stderr, __VA_ARGS__)

#endif

#define _TEXT(x)	__TEXT(x)
#define	_T(x)		__TEXT(x)

#endif
