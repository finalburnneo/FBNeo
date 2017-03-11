#include <stdio.h>

int main(int /*argc*/, char** /*argv*/)
{
	printf("#define BUILD_TIME %s\n", __TIME__);
	printf("#define BUILD_DATE %s\n", __DATE__);

#ifdef _UNICODE
	printf("#define BUILD_CHAR Unicode\n");
#else
	printf("#define BUILD_CHAR ANSI\n");
#endif

#if !defined BUILD_X64_EXE
	printf("#define BUILD_CPU  X86\n");
#else
	printf("#define BUILD_CPU  X64\n");
#endif

#if defined __GNUC__
	printf("#define BUILD_COMP GCC %i.%i.%i\n", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#elif defined _MSC_VER
 #if _MSC_VER >= 1300 && _MSC_VER < 1310
	printf("#define BUILD_COMP Visual C++ 2002\n");
 #elif _MSC_VER >= 1310 && _MSC_VER < 1400
	printf("#define BUILD_COMP Visual C++ 2003\n");
 #elif _MSC_VER >= 1400 && _MSC_VER < 1500
	printf("#define BUILD_COMP Visual C++ 2005\n");
 #elif _MSC_VER >= 1500 && _MSC_VER < 1600
	printf("#define BUILD_COMP Visual C++ 2008\n");
 #elif _MSC_VER >= 1600 && _MSC_VER < 1700
	printf("#define BUILD_COMP Visual C++ 2010\n");
 #elif _MSC_VER >= 1700 && _MSC_VER < 1800
  #if defined BUILD_VS_XP_TARGET
    printf("#define BUILD_COMP Visual C++ 2012 (XP)\n");
  #else
	printf("#define BUILD_COMP Visual C++ 2012\n");
  #endif
 #elif _MSC_VER >= 1800 && _MSC_VER < 1900
  #if defined BUILD_VS_XP_TARGET
    printf("#define BUILD_COMP Visual C++ 2013 (XP)\n");
  #else
	printf("#define BUILD_COMP Visual C++ 2013\n");
  #endif
 #elif _MSC_VER >= 1900 && _MSC_VER < 1910
  #if defined BUILD_VS_XP_TARGET
    printf("#define BUILD_COMP Visual C++ 2015 (XP)\n");
  #else
	printf("#define BUILD_COMP Visual C++ 2015\n");
  #endif
 #elif _MSC_VER >= 1910 && _MSC_VER < 1920
    printf("#define BUILD_COMP Visual C++ 2017\n");
 #else
	printf("#define BUILD_COMP Visual C++ %i.%i\n", _MSC_VER / 100 - 6, _MSC_VER % 100 / 10);
 #endif
#else
	printf("#define BUILD_COMP Unknown compiler\n");
#endif

// Visual C's resource compiler doesn't define _MSC_VER, but we need it for VERSION resources
#ifdef _MSC_VER
	printf("#ifndef _MSC_VER\n");
	printf(" #define _MSC_VER  %i\n", _MSC_VER);
	printf("#endif\n");
#endif

	return 0;
}
