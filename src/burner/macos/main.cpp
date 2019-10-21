#include "burner.h"

extern int RunIdle();
extern int RunInit();
extern int RunExit();

int nAppVirtualFps = 6000; // App fps * 100
bool bRunPause = 0;

int MainInit(const char *path, const char *setname)
{
    if (path == NULL || setname == NULL) {
        fprintf(stderr, "Path or set name uninitialized\n");
        return 0;
    }

    fprintf(stderr, "Initializing '%s' in '%s'\n", setname, path);

    SDL_Init(SDL_INIT_AUDIO);
    BurnLibInit();

    int i;
    for (i = 0; i < nBurnDrvCount; i++) {
        nBurnDrvActive = i;
        if (strcmp(BurnDrvGetTextA(0), setname) == 0) {
            break;
        }
    }

    if (i == nBurnDrvCount) {
        fprintf(stderr, "%s is not supported by FB Neo.", setname);
        return 0;
    }

    bCheatsAllowed = false;
    sprintf(szAppRomPaths[0], path);

    if (DrvInit(i, 0))
        return 0;

    MediaInit();
    RunInit();

    return 1;
}

int MainFrame()
{
    RunIdle();

    return 1;
}

int MainEnd()
{
    RunExit();
    DrvExit();
    MediaExit();
    SDL_Quit();

    return 1;
}
