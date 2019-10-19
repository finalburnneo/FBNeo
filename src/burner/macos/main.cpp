#include "burner.h"

extern int RunIdle();
extern int RunInit();
extern int RunExit();

int MainInit()
{
    SDL_Init(SDL_INIT_AUDIO);
    BurnLibInit();

    const char *set = "sfiii3";
    int i;
    for (i = 0; i < nBurnDrvCount; i++) {
        nBurnDrvActive = i;
        if (strcmp(BurnDrvGetTextA(0), set) == 0) {
            break;
        }
    }

    if (i == nBurnDrvCount) {
        printf("%s is not supported by FB Neo.", set);
        return 0;
    }

    bCheatsAllowed = false;

    sprintf(szAppRomPaths[0], "/Volumes/Stuff/Emulation/mame/");
    DrvInit(i, 0);

    MediaInit();
    RunInit();

    return 1;
}

int MainFrame()
{
//    SDL_Event event;
//    while (SDL_PollEvent(&event)) {
//            switch (event.type) {
//                case SDL_QUIT: /* Windows was closed */
//                    quit = 1;
//                    break;
//            }
//    }
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
