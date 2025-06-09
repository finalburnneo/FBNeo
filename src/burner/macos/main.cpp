#include "burner.h"
#include "main.h"

#include <Carbon/Carbon.h>

extern int RunIdle();
extern int RunInit();
extern int RunExit();

int nAppVirtualFps = 6000; // App fps * 100
bool bRunPause = 0;

static char NVRAMPath[MAX_PATH];

void SetNVRAMPath(const char *path)
{
    strncpy(NVRAMPath, path, MAX_PATH);
}

int OneTimeInit()
{
    return BurnLibInit();
}

int OneTimeEnd()
{
    return BurnLibExit();
}

int MainInit(const char *path, const char *setname)
{
    if (path == NULL || setname == NULL) {
        fprintf(stderr, "Path or set name uninitialized\n");
        return 1;
    }

    char message[1024];
    snprintf(message, sizeof(message), "Initializing '%s' in '%s'...\n", setname, path);
    ProgressUpdateBurner(0, message, false);

    int i;
    for (i = 0; i < nBurnDrvCount; i++) {
        nBurnDrvActive = i;
        if (strcmp(BurnDrvGetTextA(0), setname) == 0) {
            break;
        }
    }

    if (i == nBurnDrvCount) {
        snprintf(message, sizeof(message), "'%s' is not supported by FB Neo.", setname);
        AppError(message, false);
        return 1;
    }

    bCheatsAllowed = false;
    sprintf(szAppRomPaths[0], path);

    if (DrvInit(i, 0))
        return 1;

    MediaInit();
    RunInit();

    // Load NVRAM
    char temp[MAX_PATH];
    snprintf(temp, MAX_PATH, "%s/%s.nvr", NVRAMPath, BurnDrvGetTextA(0));
    BurnStateLoad(temp, 0, NULL);

    if (bRunPause)
        AudSoundStop();

    ProgressUpdateBurner(0, "Initialization successful", false);

    return 0;
}

int MainFrame()
{
    RunIdle();

    return 0;
}

int MainEnd()
{
    // Save NVRAM
    char temp[MAX_PATH];
    snprintf(temp, MAX_PATH, "%s/%s.nvr", NVRAMPath, BurnDrvGetTextA(0));
    BurnStateSave(temp, 0);

    // Cleanup
    RunExit();
    DrvExit();
    MediaExit();

    return 0;
}

#pragma mark - SDL substitutes

unsigned int SDL_GetTicks()
{
    UnsignedWide uw;
    Microseconds(&uw);
    return ((double) UnsignedWideToUInt64(uw) + 500.0) / 1000.0;
}

void SDL_Delay(unsigned int ms)
{
    unsigned int stop, now;
    stop = SDL_GetTicks() + ms;
    do {
        MPYield();
        now = SDL_GetTicks();
    } while (stop > now);
}

TCHAR* AdaptiveEncodingReads(const TCHAR* pszFileName)
{
	return NULL;
}
