#include "burner.h"

static int Init()
{
    return 0;
}

static int Exit()
{
    return 0;
}

// Run one frame and render the screen
static int Frame(bool bRedraw)
{
    if (bDrvOkay) {
        if (bRedraw) { // Redraw current frame
            if (BurnDrvRedraw()) {
                BurnDrvFrame(); // No redraw function provided, advance one frame
            }
        } else {
            BurnDrvFrame(); // Run one frame and draw the screen
        }

        if ((BurnDrvGetFlags() & BDF_16BIT_ONLY) && pVidTransCallback)
            pVidTransCallback();
    }

    return 0;
}

// Paint the BlitFX surface onto the primary surface
static int Paint(int bValidate)
{
    return 0;
}

static int Scale(RECT* , int, int)
{
    return 0;
}

static int GetSettings(InterfaceInfo* pInfo)
{
    return 0;
}

struct VidOut VidOutMacOS = {
    Init,
    Exit,
    Frame,
    Paint,
    Scale,
    GetSettings,
    _T("MacOS Video"),
};
