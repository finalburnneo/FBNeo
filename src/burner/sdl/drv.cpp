// Driver Init module
#include "burner.h"
#include "neocdlist.h"
int bDrvOkay = 0;                       // 1 if the Driver has been initted okay, and it's okay to use the BurnDrv functions

char szAppRomPaths[DIRS_MAX][MAX_PATH] = { { "/usr/local/share/roms/" }, { "roms/" }, };

static bool bSaveRAM = false;

static INT32 nNeoCDZnAudSampleRateSave = 0;

void NeoCDZRateChangeback()
{
	if (nNeoCDZnAudSampleRateSave != 0) {
		bprintf(PRINT_IMPORTANT, _T("Switching sound rate back to user-selected %dhz\n"), nNeoCDZnAudSampleRateSave);
		nAudSampleRate[nAudSelect] = nNeoCDZnAudSampleRateSave;
		nNeoCDZnAudSampleRateSave = 0;
	}
}

static void NeoCDZRateChange()
{
	if (nAudSampleRate[nAudSelect] != 44100) {
		nNeoCDZnAudSampleRateSave = nAudSampleRate[nAudSelect];
		bprintf(PRINT_IMPORTANT, _T("Switching sound rate to 44100hz (from %dhz) as required by NeoGeo CDZ\n"), nNeoCDZnAudSampleRateSave);
		nAudSampleRate[nAudSelect] = 44100; // force 44100hz for CDDA
	}
}

static int DoLibInit()                  // Do Init of Burn library driver
{
	int nRet;

	BzipOpen(false);

	//ProgressCreate();

	nRet = BurnDrvInit();

	BzipClose();

	//ProgressDestroy();

	if (nRet)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

// Catch calls to BurnLoadRom() once the emulation has started;
// Intialise the zip module before forwarding the call, and exit cleanly.
static int DrvLoadRom(unsigned char* Dest, int* pnWrote, int i)
{
	int nRet;

	BzipOpen(false);

	if ((nRet = BurnExtLoadRom(Dest, pnWrote, i)) != 0)
	{
		char  szText[256] = "";
		char* pszFilename;

		BurnDrvGetRomName(&pszFilename, i, 0);
		sprintf(szText, "Error loading %s, requested by %s.\nThe emulation will likely suffer problems.", pszFilename, BurnDrvGetTextA(0));
	}

	BzipClose();

	BurnExtLoadRom = DrvLoadRom;

	//ScrnTitle();

	return nRet;
}

int DrvInit(int nDrvNum, bool bRestore)
{
	DrvExit();						// Make sure exitted
	MediaExit();

	nBurnDrvActive = nDrvNum;		// Set the driver number


	if ((BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_SNK_NEOCD) {
		if (CDEmuInit()) {
			printf("CD emu failed\n");
			return 1;
		}

		NeoCDInfo_Init();

		NeoCDZRateChange();
	}

	{ // Init input and audio, save blitter init for later. (reduce # of mode changes, nice for emu front-ends)
		bVidOkay = 1;
		MediaInit();
		bVidOkay = 0;
	}

	// Define nMaxPlayers early; GameInpInit() needs it (normally defined in DoLibInit()).
	nMaxPlayers = BurnDrvGetMaxPlayers();
	GameInpInit();                           // Init game input

	// if a config file was loaded, set bSaveInputs so that this config file won't be reset to default at exit
	bSaveInputs = ConfigGameLoad(true);
	InputMake(true);

	GameInpDefault();

	if (DoLibInit())                         // Init the Burn library's driver
	{
		char szTemp[512];

		BurnDrvExit();                                // Exit the driver

		_stprintf(szTemp, _T("There was an error starting '%s'.\n"), BurnDrvGetText(DRV_FULLNAME));
		return 1;
	}

	BurnExtLoadRom = DrvLoadRom;

	bDrvOkay = 1;                            // Okay to use all BurnDrv functions

	bSaveRAM = false;
	nBurnLayer = 0xFF;                       // show all layers
	ConfigCheatLoad();
	// Reset the speed throttling code, so we don't 'jump' after the load
	RunReset();
	VidExit();
	AudSoundExit();
	return 0;
}

int DrvInitCallback()
{
	return DrvInit(nBurnDrvActive, false);
}

int DrvExit()
{
	if (bDrvOkay)
	{
		if (nBurnDrvActive < nBurnDrvCount)
		{
			if (bSaveRAM)
			{
				bSaveRAM = false;
			}

			ConfigGameSave(bSaveInputs);
			GameInpExit();                                         // Exit game input
			BurnDrvExit();                                         // Exit the driver
		}
	}

	BurnExtLoadRom = NULL;

	bDrvOkay = 0;                   // Stop using the BurnDrv functions
//	nBurnDrvActive = ~0U;                 // no driver selected

	return 0;
}

#ifndef BUILD_MACOS

int ProgressUpdateBurner(double dProgress, const TCHAR* pszText, bool bAbs)
{
	return 0;
}

int AppError(TCHAR* szText, int bWarning)
{
	return 0;
}

#endif
