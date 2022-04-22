#ifndef __PORT_TYPEDEFS_H
#define __PORT_TYPEDEFS_H

#include "libretro.h"
#if 0
#ifndef IOS
	// This is a nightmarish hack brought forth by file_stream_transform...
	// Code using string types, char types and file functions will fail
	// on msvc2017 if we don't include the following libraries before
	// file_stream_transforms due to conflicting declarations.
	// It also seems one of those includes provide math on msvc2017, so
	// we'll lack M_PI if we don't define _USE_MATH_DEFINES right now.
	// Furthermore, <string> breaks FBNEO_DEBUG while <string.h> breaks
	// msvc2017 x86 builds...
	#define _USE_MATH_DEFINES
	#include <wchar.h>
	#ifndef _MSC_VER
		#include <string.h>
	#else
		#include <string>
	#endif
#endif
#endif
#if defined(_MSC_VER) || defined(__MINGW32__)
	#define _USE_MATH_DEFINES
	#include <wchar.h>
	#include <string>
#endif
#include "streams/file_stream_transforms.h"

extern int kNetGame;
extern int bRunPause;

#ifdef USE_CYCLONE
	#define SEK_CORE_C68K (0)
	#define SEK_CORE_M68K (1)
	extern int nSekCpuCore;  // 0 - c68k, 1 - m68k
#endif

/* fastcall only works on x86_32 */
#ifndef FASTCALL
	#undef __fastcall
	#define __fastcall
#else
	#ifndef _MSC_VER
		#undef __fastcall
		#define __fastcall __attribute__((fastcall))
	#endif
#endif

#ifndef _MSC_VER
	#include <stdint.h>
#else
	#undef _UNICODE
	#include "compat/msvc.h"
	#include "compat/posix_string.h"
#endif

#define _T(x) x
#define _tfopen fopen
#define _fgetts fgets
#define _stprintf sprintf
#define _tcslen strlen
#define _tcscpy strcpy
#define _tcstol strtol
#define _istspace isspace
#define _tcsncmp strncmp
#define _tcsncpy strncpy
#define _tcsicmp strcasecmp
#define _stricmp strcasecmp
#define stricmp strcasecmp
#define _ftprintf fprintf
#define _tcsstr strstr

typedef char TCHAR;

#endif
