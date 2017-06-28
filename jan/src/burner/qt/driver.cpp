// Driver Init module
#include "burner.h"
#include <QtWidgets>

int bDrvOkay = 0;						// 1 if the Driver has been initted okay, and it's okay to use the BurnDrv functions

static int DrvBzipOpen()
{
    BzipOpen(false);

    if (BzipStatus())
        return 1;
    return 0;
}	

static int DoLibInit()					// Do Init of Burn library driver
{
	int nRet = 0;

	if (DrvBzipOpen()) {
		return 1;
	}
    ProgressCreate();
	nRet = BurnDrvInit();

	BzipClose();

    ProgressDestroy();

	if (nRet) {
		return 3;
	} else {
		return 0;
	}
}

// Catch calls to BurnLoadRom() once the emulation has started;
// Intialise the zip module before forwarding the call, and exit cleanly.
static int __cdecl DrvLoadRom(unsigned char* Dest, int* pnWrote, int i)
{
	int nRet;

	BzipOpen(false);

	if ((nRet = BurnExtLoadRom(Dest, pnWrote, i)) != 0) {
		char* pszFilename;

		BurnDrvGetRomName(&pszFilename, i, 0);
	}

	BzipClose();
	return nRet;
}

int DrvInit(int nDrvNum, bool bRestore)
{
	int nStatus;
    bDrvOkay = 0;
	
	DrvExit();						// Make sure exitted
	nBurnDrvActive = nDrvNum;		// Set the driver number

    nMaxPlayers = BurnDrvGetMaxPlayers();
    GameInpInit();					// Init game input
    if (ConfigGameLoad(true)) {
        ConfigGameLoadHardwareDefaults();
    }
    InputMake(true);
    GameInpDefault();

	nStatus = DoLibInit();			// Init the Burn library's driver
	if (nStatus) {
		if (nStatus & 2) {
			BurnDrvExit();			// Exit the driver
		}
		return 1;
	}



	BurnExtLoadRom = DrvLoadRom;

	bDrvOkay = 1;						// Okay to use all BurnDrv functions

	if (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL) {
		bVidArcaderes = bVidArcaderesVer;
		nVidWidth	= nVidVerWidth;
		nVidHeight	= nVidVerHeight;
	} else {
		bVidArcaderes = bVidArcaderesHor;
		nVidWidth	= nVidHorWidth;
		nVidHeight	= nVidHorHeight;
	}

	nBurnLayer = 0xFF;				// show all layers
	
	// Reset the speed throttling code, so we don't 'jump' after the load
    //RunReset();
	VidExit();
	return 0;
}

int DrvInitCallback()
{
	return DrvInit(nBurnDrvActive, false);
}

int DrvExit()
{
	if (bDrvOkay) {
        ConfigGameSave(bSaveInputs);
        GameInpExit();
		BurnDrvExit();				// Exit the driver
	}

	BurnExtLoadRom = NULL;
	bDrvOkay = 0;					// Stop using the BurnDrv functions
	bRunPause = 0;					// Don't pause when exitted
	nBurnDrvActive = ~0U;			// no driver selected
	return 0;
}

