// Driver Init module
#include "burner.h"
#include "neocdlist.h"

int bDrvOkay = 0;						// 1 if the Driver has been initted okay, and it's okay to use the BurnDrv functions

TCHAR szAppRomPaths[DIRS_MAX][MAX_PATH] = {
	{ _T("roms/")			},
	{ _T("roms/arcade/")	},
	{ _T("roms/megadrive/")	},
	{ _T("roms/pce/")		},
	{ _T("roms/sgx/")		},
	{ _T("roms/tg16/")		},
	{ _T("roms/coleco/")	},
	{ _T("roms/sg1000/")	},
	{ _T("roms/gamegear/")	},
	{ _T("roms/sms/")		},
	{ _T("roms/msx/")		},
	{ _T("roms/spectrum/")	},
	{ _T("roms/snes/")		},
	{ _T("roms/fds/")		},
	{ _T("roms/nes/")		},
	{ _T("roms/ngp/")		},
	{ _T("roms/channelf/")	},
	{ _T("roms/romdata/")	},
	{ _T("")				},
	{ _T("")				}
};

TCHAR szAppQuickPath[MAX_PATH] = _T("");

static bool bSaveRAM = false;

static INT32 nNeoCDZnAudSampleRateSave = 0;

static int DrvBzipOpen()
{
	BzipOpen(false);

	// If there is a problem with the romset, report it
	switch (BzipStatus()) {
		case BZIP_STATUS_BADDATA: {
			FBAPopupDisplay(PUF_TYPE_WARNING);
			break;
		}
		case BZIP_STATUS_ERROR: {
			FBAPopupDisplay(PUF_TYPE_ERROR);

#if 0 || !defined FBNEO_DEBUG
			// Don't even bother trying to start the game if we know it won't work
			BzipClose();
			return 1;
#endif

			break;
		}
		default: {

#if 0 && defined FBNEO_DEBUG
			FBAPopupDisplay(PUF_TYPE_INFO);
#else
			FBAPopupDisplay(PUF_TYPE_INFO | PUF_TYPE_LOGONLY);
#endif

		}
	}

	return 0;
}

static int DoLibInit()					// Do Init of Burn library driver
{
	int nRet = 0;

	RomDataInit();

	if (DrvBzipOpen()) {
		RomDataExit();
		return 1;
	}

	if ((BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) != HARDWARE_SNK_MVS) {
		if (!bQuietLoading) {
			ProgressCreate();
			if (BurnDrvGetTextA(DRV_SAMPLENAME) != NULL) { // has samples
				BurnSetProgressRange(0.99); // Increase range for samples
			}
		}
	}

	nRet = BurnDrvInit();

	RomDataSetFullName();

	BzipClose();

	if (!bQuietLoading) ProgressDestroy();

	IpsPatchExit(); // done loading roms, disable ips patcher

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

		FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_REQUEST), pszFilename, BurnDrvGetText(DRV_NAME));
		FBAPopupDisplay(PUF_TYPE_ERROR);
	}

	BzipClose();

	BurnExtLoadRom = DrvLoadRom;

	ScrnTitle();

	return nRet;
}

int __cdecl DrvCartridgeAccess(BurnCartrigeCommand nCommand)
{
	switch (nCommand) {
		case CART_INIT_START:
			if (!bQuietLoading) {
				ProgressCreate();
				if (BurnDrvGetTextA(DRV_SAMPLENAME) != NULL) { // has samples
					BurnSetProgressRange(0.99); // Increase range for samples
				}
			}
			if (DrvBzipOpen()) {
				return 1;
			}
			break;
		case CART_INIT_END:
			if (!bQuietLoading) ProgressDestroy();
			BzipClose();
			break;
		case CART_EXIT:
			break;
		default:
			return 1;
	}

	return 0;
}

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

int DrvInit(int nDrvNum, bool bRestore)
{
	int nStatus;

	DrvExit();						// Make sure exitted
	MediaExit();

	nBurnDrvActive = nDrvNum;		// Set the driver number

	if ((BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_SNK_MVS) {

		BurnExtCartridgeSetupCallback = DrvCartridgeAccess;

		if (!bQuietLoading) {		// not from cmdline - show slot selector
			if (SelMVSDialog()) {
				POST_INITIALISE_MESSAGE;
				return 0;
			}
		}
	}

	if ((BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_SNK_NEOCD) {
		if (CDEmuInit()) {
			FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_CDEMU_INI_FAIL));
			FBAPopupDisplay(PUF_TYPE_ERROR);

			POST_INITIALISE_MESSAGE;
			return 0;
		}

		NeoCDInfo_Init();

		NeoCDZRateChange();
	}

	{ // Init input, save audio and blitter init for later. (reduce # of mode changes, nice for emu front-ends)
		bVidOkay = 1; // don't init video yet
		bAudOkay = 1; // don't init audio yet, but grab soundcard params (nBurnSoundRate) so soundcores can init.
		if (bDontInitMedia == false) MediaInit();
		bVidOkay = 0;
		bAudOkay = 0;
	}

	// Define nMaxPlayers early; GameInpInit() needs it (normally defined in DoLibInit()).
	nMaxPlayers = BurnDrvGetMaxPlayers();
	GameInpInit();					// Init game input

	if(ConfigGameLoad(true)) {
		ConfigGameLoadHardwareDefaults();
	}
	InputMake(true);
	GameInpDefault();

	if (kNetGame) {
		nBurnCPUSpeedAdjust = 0x0100;
	}

	nStatus = DoLibInit();			// Init the Burn library's driver

	if (nStatus) {
		if (nStatus & 2) {
			BurnDrvExit();			// Exit the driver

			ScrnTitle();

			FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_BURN_INIT), BurnDrvGetText(DRV_FULLNAME));
			FBAPopupDisplay(PUF_TYPE_WARNING);

			// When romdata loading fails, the data within the structure must be emptied to restore the original data content.
			// The time to quit must be after the correct name of the game corresponding to Romdata has been displayed.
			if (NULL != pDataRomDesc) {
				RomDataExit();
			}
		}

		NeoCDZRateChangeback();

		POST_INITIALISE_MESSAGE;
		return 1;
	}

	BurnExtLoadRom = DrvLoadRom;

	bDrvOkay = 1;						// Okay to use all BurnDrv functions

	if (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL) {
		nScreenSize = nScreenSizeVer;
		bVidArcaderes = bVidArcaderesVer;
		nVidWidth	= nVidVerWidth;
		nVidHeight	= nVidVerHeight;
	} else {
		nScreenSize = nScreenSizeHor;
		bVidArcaderes = bVidArcaderesHor;
		nVidWidth	= nVidHorWidth;
		nVidHeight	= nVidHorHeight;
	}

	bSaveRAM = false;
	if (kNetGame) {
		KailleraInitInput();
		KailleraGetInput();
	} else {
		if (bRestore) {
			StatedAuto(0);
			bSaveRAM = true;
		}
		ConfigCheatLoad();
	}

	nBurnLayer = 0xFF;				// show all layers

	// Reset the speed throttling code, so we don't 'jump' after the load
	RunReset();

	VidExit();
	POST_INITIALISE_MESSAGE;
	CallRegisteredLuaFunctions(LUACALL_ONSTART);

	return 0;
}

int DrvInitCallback()
{
	return DrvInit(nBurnDrvActive, false);
}

int DrvExit()
{
	if (bDrvOkay) {
		NeoCDZRateChangeback();
		StopReplay();
		VidExit();

		InvalidateRect(hScrnWnd, NULL, 1);
		UpdateWindow(hScrnWnd);			// Blank screen window

		DestroyWindow(hInpdDlg);		// Make sure the Input Dialog is exited
		DestroyWindow(hInpDIPSWDlg);	// Make sure the DipSwitch Dialog is exited
		DestroyWindow(hInpCheatDlg);	// Make sure the Cheat Dialog is exited

		if (nBurnDrvActive < nBurnDrvCount) {
			MemCardEject();				// Eject memory card if present

			if (bSaveRAM) {
				StatedAuto(1);			// Save NV (or full) RAM
				bSaveRAM = false;
			}

			ConfigGameSave(bSaveInputs);
			GameInpExit();				// Exit game input
			BurnDrvExit();				// Exit the driver
		}
	}

	BurnExtLoadRom = NULL;
	bDrvOkay       = 0;					// Stop using the BurnDrv functions
	bRunPause      = 0;					// Don't pause when exitted

	if (bAudOkay && pBurnSoundOut) {
		// Write silence into the sound buffer on exit, and for drivers which don't use pBurnSoundOut
		memset(nAudNextSound, 0, nAudSegLen << 2);
	}

	CDEmuExit();
	RomDataExit();

	BurnExtCartridgeSetupCallback = NULL;
	nBurnDrvActive = ~0U;				// no driver selected

	return 0;
}
