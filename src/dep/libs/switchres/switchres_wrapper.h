/**************************************************************

   log.h - Simple logging for Switchres

   ---------------------------------------------------------

   Switchres   Modeline generation engine for emulation

   License     GPL-2.0+
   Copyright   2010-2020 Chris Kennedy, Antonio Giner,
                         Alexandre Wodarczyk, Gil Delescluse

 **************************************************************/
 
 #ifdef __cplusplus
extern "C" {
#endif

#ifdef __linux__
#include <dlfcn.h>
#define LIBTYPE void*
#define OPENLIB(libname) dlopen((libname), RTLD_LAZY)
#define LIBFUNC(libh, fn) dlsym((libh), (fn))
#define LIBERROR dlerror
#define CLOSELIB(libh) dlclose((libh))

#elif defined _WIN32
#include <windows.h>
//#include <string>
#define LIBTYPE HINSTANCE
#define OPENLIB(libname) LoadLibrary(TEXT((libname)))
#define LIBFUNC(lib, fn) GetProcAddress((lib), (fn))
char* LIBERROR()
{
    //Get the error message, if any.
    DWORD errorMessageID = GetLastError();
    if(errorMessageID == 0)
        return NULL; //No error message has been recorded

    LPSTR messageBuffer;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                 NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);
    
    SetLastError(0);

    static char error_msg[256] = {0};
    strncpy(error_msg, messageBuffer, sizeof(error_msg)-1);
    LocalFree(messageBuffer);
    return error_msg;
}
#define CLOSELIB(libp) FreeLibrary((libp))
#endif

#ifdef _WIN32
    #ifdef MODULE_API_EXPORTS
        #define MODULE_API __declspec(dllexport)
    #else
        #define MODULE_API __declspec(dllimport)
    #endif
#else
    #define MODULE_API
#endif

// That's all the exposed data from Switchres calculation
typedef struct MODULE_API {
    int width;
    int height;
    double refresh;
    unsigned char is_refresh_off;
    unsigned char is_stretched;
    int x_scale;
    int y_scale;
    unsigned char interlace;
} sr_mode;

MODULE_API void sr_init();
MODULE_API void sr_deinit();
MODULE_API unsigned char sr_add_mode(int, int, double, unsigned char, sr_mode*);
MODULE_API unsigned char sr_switch_to_mode(int, int, double, unsigned char, sr_mode*);
MODULE_API void sr_set_monitor(const char*);


// Inspired by https://stackoverflow.com/a/1067684
typedef struct MODULE_API {
    void (*init)(void);
    void (*deinit)(void);
    unsigned char (*sr_add_mode)(int, int, double, unsigned char, sr_mode*);
    unsigned char (*sr_switch_to_mode)(int, int, double, unsigned char, sr_mode*);
} srAPI;


#ifdef __cplusplus
}
#endif
