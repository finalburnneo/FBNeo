// Headless driver glue for FinalBurn Neo.
// Author: Leonardo Kasperavičius
//
// Built originally to support OpenEmu-Silicon
// (https://github.com/OpenEmu-Silicon/OpenEmu-Silicon).
//
// The headless counterpart of the SDL burner's drv.cpp: wraps BurnDrvInit in the
// zip loader and provides the frontend callbacks the shared burner code calls.
// CD systems (NeoGeo CD / PCE CD) are not wired yet. Subject to the FB Neo
// license (see src/license.txt).

#include "burner.h"

int bDrvOkay = 0;
char szAppRomPaths[DIRS_MAX][MAX_PATH] = { { "roms/" }, };

static bool bSaveRAM = false;

// Battery-backed RAM (ACB_NVRAM: Mortal Kombat's settings, Robotron's high
// scores...) is not saved by the driver, unlike the EEPROM device. Like the
// Windows burner's StatedAuto, keep it in <szAppEEPROMPath><set>.fs: loaded
// after init and saved before exit. Drivers without NVRAM write no file.
static void DrvNvramAuto(int bSave)
{
	TCHAR szName[MAX_PATH];
	_stprintf(szName, _T("%s%s.fs"), szAppEEPROMPath, BurnDrvGetText(DRV_NAME));
	if (bSave)
		BurnStateSave(szName, 0);
	else
		BurnStateLoad(szName, 0, NULL);
}

static int DoLibInit() // Init the Burn library's driver
{
	BzipOpen(false);
	int nRet = BurnDrvInit();
	BzipClose();
	return nRet ? 1 : 0;
}

// Catch BurnLoadRom() calls once emulation has started: open the zip module,
// forward, then exit cleanly.
static int DrvLoadRom(unsigned char* Dest, int* pnWrote, int i)
{
	BzipOpen(false);
	int nRet = BurnExtLoadRom(Dest, pnWrote, i);
	BzipClose();

	BurnExtLoadRom = DrvLoadRom;
	return nRet;
}

int DrvInit(int nDrvNum, bool bRestore)
{
	DrvExit();						// Make sure exited
	MediaExit();

	nBurnDrvActive = nDrvNum;

	{ // Init input and audio; defer blitter init.
		bVidOkay = 1;
		bAudOkay = 1;
		MediaInit();
		bVidOkay = 0;
		bAudOkay = 0;
	}

	nMaxPlayers = BurnDrvGetMaxPlayers();
	GameInpInit();

	bSaveInputs = ConfigGameLoad(true);
	if (bSaveInputs) ConfigGameLoadHardwareDefaults();

	InputMake(true);
	GameInpDefault();

	if (DoLibInit()) {
		BurnDrvExit();
		return 1;
	}

	BurnExtLoadRom = DrvLoadRom;

	bDrvOkay = 1;
	bSaveRAM = false;
	nBurnLayer = 0xFF;
	ConfigCheatLoad();
	RunReset();
	if (bRestore) {
		DrvNvramAuto(0);
		bSaveRAM = true;
	}
	return 0;
}

int DrvInitCallback()
{
	return DrvInit(nBurnDrvActive, false);
}

int DrvExit()
{
	if (bDrvOkay) {
		if (nBurnDrvActive < nBurnDrvCount) {
			if (bSaveRAM) {
				DrvNvramAuto(1);
				bSaveRAM = false;
			}
			ConfigGameSave(bSaveInputs);
			GameInpExit();
			BurnDrvExit();
		}
	}

	BurnExtLoadRom = NULL;
	bDrvOkay = 0;
	return 0;
}

int ProgressUpdateBurner(double /* dProgress */, const TCHAR* /* pszText */, bool /* bAbs */)
{
	return 0;
}

int AppError(TCHAR* /* szText */, int /* bWarning */)
{
	return 0;
}

// The host drives frame timing, so there is no throttle to reset.
int RunReset()
{
	return 0;
}
