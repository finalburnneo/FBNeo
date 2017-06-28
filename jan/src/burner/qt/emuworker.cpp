#include <QtWidgets>
#include "emuworker.h"
#include "burner.h"

static int GetInput(bool bCopy);
static int RunFrame(int bDraw, int bPause);
static int RunGetNextSound(int bDraw);

EmuWorker::EmuWorker(QObject *parent) :
    QObject(parent)
{
    m_isRunning = false;
}

bool EmuWorker::init()
{
    AudSoundStop();

    if (bDrvOkay)
        DrvExit();

    // we need to initialize sound first
    AudSoundInit();
    AudSetCallback(RunGetNextSound);
    bAudOkay = 1;

    DrvInit(m_game, false);
    if (!bDrvOkay)
        return false;

    VidInit();

    return true;
}

void EmuWorker::resume()
{
    AudSoundPlay();
    if (!bDrvOkay) {
        m_isRunning = false;
        return;
    }
    m_isRunning = true;
}


void EmuWorker::close()
{
    AudSoundStop();
    DrvExit();
}

void EmuWorker::pause()
{
    m_isRunning = false;
}

void EmuWorker::setGame(int no)
{
    m_game = no;
}

void EmuWorker::run()
{
    if (m_isRunning && bDrvOkay) {
        AudSoundCheck();
    }
}

int GetInput(bool bCopy)
{
    InputMake(bCopy); 						// get input
    return 0;
}

int RunFrame(int bDraw, int bPause)
{
    static int bPrevPause = 0;
    static int bPrevDraw = 0;

    if (bPrevDraw && !bPause) {
        VidPaint(0);							// paint the screen (no need to validate)
    }

    if (!bDrvOkay) {
        return 1;
    }

    if (bPause)
    {
        GetInput(false);						// Update burner inputs, but not game inputs
        if (bPause != bPrevPause)
        {
            VidPaint(2);                        // Redraw the screen (to ensure mode indicators are updated)
        }
    }
    else
    {
        nFramesEmulated++;
        nCurrentFrame++;
        GetInput(true);					// Update inputs
    }
    if (bDraw) {
        nFramesRendered++;
        if (VidFrame()) {					// Do one frame
            AudBlankSound();
        }
    }
    else {								// frame skipping
        pBurnDraw = NULL;					// Make sure no image is drawn
        BurnDrvFrame();
    }
    bPrevPause = bPause;
    bPrevDraw = bDraw;

    return 0;
}

int RunGetNextSound(int bDraw)
{
    if (nAudNextSound == NULL) {
        return 1;
    }
    // Render frame with sound
    pBurnSoundOut = nAudNextSound;
    RunFrame(bDraw, 0);
    return 0;
}
