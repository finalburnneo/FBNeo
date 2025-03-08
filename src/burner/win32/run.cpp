// Run module
#include "burner.h"

int bRunPause = 0;
int bAltPause = 0;

int bAlwaysDrawFrames = 0;

static bool bShowFPS = false;
static unsigned int nDoFPS = 0;

static bool bMute = false;
static int nOldAudVolume;

int kNetGame = 0;							// Non-zero if Kaillera is being used

#ifdef FBNEO_DEBUG
int counter;								// General purpose variable used when debugging
#endif

static unsigned int nNormalLast = 0;		// Last value of timeGetTime()
static int nNormalFrac = 0;					// Extra fraction we did

static bool bAppDoStep = 0;
static bool bAppDoFast = 0;
static bool bAppDoFasttoggled = 0;
static bool bAppDoRewind = 0;

static int nFastSpeed = 6;
static void DisplayFPSInit(); // forward

// in FFWD, the avi-writer still needs to write all frames (skipped or not)
#define FFWD_GHOST_FRAME 0x8000

// SlowMo T.A. feature
int nSlowMo = 0;
static int flippy = 0; // free running RunFrame() counter

// For System Macros (below)
static int prevPause = 0, prevFFWD = 0, prevFrame = 0, prevSState = 0, prevLState = 0, prevNState = 0, prevPState = 0, prevUState = 0;
UINT32 prevPause_debounce = 0;

struct lua_hotkey_handler {
	UINT8 * macroSystemLuaHotkey_ref;
	UINT8 macroSystemLuaHotkey_val;
	int prev_value;
	char *hotkey_name;
};

struct lua_hotkey_handler hotkey_debounces[] = {
	{ &macroSystemLuaHotkey1, macroSystemLuaHotkey1, 0, "lua_hotkey" },
	{ &macroSystemLuaHotkey2, macroSystemLuaHotkey2, 0, "lua_hotkey" },
	{ &macroSystemLuaHotkey3, macroSystemLuaHotkey3, 0, "lua_hotkey" },
	{ &macroSystemLuaHotkey4, macroSystemLuaHotkey4, 0, "lua_hotkey" },
	{ &macroSystemLuaHotkey5, macroSystemLuaHotkey5, 0, "lua_hotkey" },
	{ &macroSystemLuaHotkey6, macroSystemLuaHotkey6, 0, "lua_hotkey" },
	{ &macroSystemLuaHotkey7, macroSystemLuaHotkey7, 0, "lua_hotkey" },
	{ &macroSystemLuaHotkey8, macroSystemLuaHotkey8, 0, "lua_hotkey" },
	{ &macroSystemLuaHotkey9, macroSystemLuaHotkey9, 0, "lua_hotkey" },
	{ NULL, 0, 0, NULL }
};

void EmulatorAppDoFast(bool dofast) {
	bAppDoFast = dofast;
	bAppDoFasttoggled = 0;
}

static void CheckSystemMacros() // These are the Pause / FFWD macros added to the input dialog
{
	// Pause
	if (macroSystemPause && macroSystemPause != prevPause && timeGetTime() > prevPause_debounce+90) {
		if (bHasFocus) {
			PostMessage(hScrnWnd, WM_KEYDOWN, VK_PAUSE, 0);
			prevPause_debounce = timeGetTime();
		}
	}
	prevPause = macroSystemPause;

	if (!kNetGame) {
		// FFWD
		int bPrevAppDoFast = bAppDoFast;
		if (macroSystemFFWD) {
			bAppDoFast = 1; prevFFWD = 1;
			if (!bPrevAppDoFast) DisplayFPSInit(); // resync fps display
		} else if (prevFFWD) {
			bAppDoFast = 0; prevFFWD = 0;
			if (bPrevAppDoFast) DisplayFPSInit(); // resync fps display
		}

		// Frame
		if (macroSystemFrame) {
			bRunPause = 1;
			if (!prevFrame) {
				bAppDoStep = 1;
				prevFrame = 1;
			}
		} else {
			prevFrame = 0;
		}

		// SlowMo
		int slow = macroSystemSlowMo[0] * 1 + macroSystemSlowMo[1] * 2 + macroSystemSlowMo[2] * 3 + macroSystemSlowMo[3] * 4 + macroSystemSlowMo[4] * 5;
		if (slow) {
			slow -= 1;
			if (slow >= 0 && slow <= 4) {
				nSlowMo = slow;
				MenuUpdateSlowMo();
			}
		}

		// Rewind handled in RunFrame()!

		for (int hotkey_num = 0; hotkey_debounces[hotkey_num].macroSystemLuaHotkey_ref != NULL; hotkey_num++) {
			// Use the reference to the hotkey variable in order to update our stored value
			// Because the hotkeys are hard coded into variables this allows us to iterate on them
			hotkey_debounces[hotkey_num].macroSystemLuaHotkey_val = *hotkey_debounces[hotkey_num].macroSystemLuaHotkey_ref;
			if (
				hotkey_debounces[hotkey_num].macroSystemLuaHotkey_val &&
				hotkey_debounces[hotkey_num].macroSystemLuaHotkey_val != hotkey_debounces[hotkey_num].prev_value
			) {
				CallRegisteredLuaFunctions((LuaCallID)(LUACALL_HOTKEY_1 + hotkey_num));
			}
			hotkey_debounces[hotkey_num].prev_value = hotkey_debounces[hotkey_num].macroSystemLuaHotkey_val;
		}

	}
	// Load State
	if (macroSystemLoadState && macroSystemLoadState != prevLState) {
		PostMessage(hScrnWnd, WM_KEYDOWN, VK_F9, 0);
	}
	prevLState = macroSystemLoadState;
	// Save State
	if (macroSystemSaveState && macroSystemSaveState != prevSState) {
		PostMessage(hScrnWnd, WM_KEYDOWN, VK_F10, 0);
	}
	prevSState = macroSystemSaveState;
	// Next State
	if (macroSystemNextState && macroSystemNextState != prevNState) {
		PostMessage(hScrnWnd, WM_KEYDOWN, VK_F11, 0);
	}
	prevNState = macroSystemNextState;
	// Previous State
	if (macroSystemPreviousState && macroSystemPreviousState != prevPState) {
		PostMessage(hScrnWnd, WM_KEYDOWN, VK_F8, 0);
	}
	prevPState = macroSystemPreviousState;
	// UNDO State
	if (macroSystemUNDOState && macroSystemUNDOState != prevUState) {
		scrnSSUndo();
	}
	prevUState = macroSystemUNDOState;
}

static int GetInput(bool bCopy)
{
	static unsigned int i = 0;
	InputMake(bCopy); 						// get input

	CheckSystemMacros();

	// Update Input dialog every 3rd frame
	if ((i%3) == 0) {
		InpdUpdate();
	}

	// Update Input Set dialog
	InpsUpdate();
	return 0;
}

static time_t fpstimer;
static unsigned int nPreviousFrames;

static void DisplayFPSInit()
{
	nDoFPS = 0;
	fpstimer = 0;
	nPreviousFrames = (bAppDoFast) ? nFramesEmulated : nFramesRendered;
}

static void DisplayFPS()
{
	const int nFPSTotal = (bAppDoFast) ? nFramesEmulated : nFramesRendered;

	TCHAR fpsstring[20];
	time_t temptime = clock();
	double fps = (double)(nFPSTotal - nPreviousFrames) * CLOCKS_PER_SEC / (temptime - fpstimer);

	_sntprintf(fpsstring, 7, _T("%2.2lf"), fps);
	if (fpstimer && temptime - fpstimer>0) { // avoid strange fps values
		VidSNewShortMsg(fpsstring, 0xDFDFFF, 480, 0);
	}

	fpstimer = temptime;
	nPreviousFrames = nFPSTotal;
}

// define this function somewhere above RunMessageLoop()
void ToggleLayer(unsigned char thisLayer)
{
	nBurnLayer ^= thisLayer;				// xor with thisLayer
	VidRedraw();
	VidPaint(0);
}

#ifdef FBNEO_DEBUG
void do_shonky_profile()
{
	bShonkyProfileMode = false; // run once!
	pBurnDraw = NULL; //pVidImage;

	const int total_frames = 5000;
	unsigned int start_time = timeGetTime();

	for (int i = 0; i < total_frames; i++) {
		BurnDrvFrame();
	}
	unsigned int end_time = timeGetTime();

	unsigned int total_time = end_time - start_time;
	float total_seconds = (float)total_time / 1000.0;

	bprintf(0, _T("shonky profile mode %d\n"), total_frames);
	bprintf(0, _T("shonky profile:  %d frames,  %dms,  %dfps\n"), total_frames, total_time, (int)(total_frames / total_seconds));
}
#endif

// With or without sound, run one frame.
// If bDraw is true, it's the last frame before we are up to date, and so we should draw the screen
int RunFrame(int bDraw, int bPause)
{
	static int bPrevPause = 0;
	static int bPrevDraw = 0;

	if (bPrevDraw && !bPause) {
		VidPaint(0);							// paint the screen (no need to validate)

		if (!nVidFullscreen && bVidDWMSync)     // Sync to DWM (Win7+, windowed)
			SuperWaitVBlank();
	}

	flippy++;
	if (nSlowMo) { // SlowMo T.A.
		if (nSlowMo == 1) {	if ((flippy % 4) == 0) return 0; } // 75% speed
		else if ((flippy % ((nSlowMo-1) * 2)) < (((nSlowMo-1) * 2) - 1)) return 0; // 50% and less
	}

	if (!bDrvOkay) {
		return 1;
	}

	if (bPause) {
		GetInput(false);						// Update burner inputs, but not game inputs
		if (bPause != bPrevPause) {
			VidPaint(2);                        // Redraw the screen (to ensure mode indicators are updated)
		}
	} else {

		nFramesEmulated++;
		nCurrentFrame++;

		if (kNetGame) {
			GetInput(true);						// Update inputs
			if (KailleraGetInput()) {			// Synchronize input with Kaillera
				return 0;
			}
		} else {
			if (nReplayStatus == 2) {
				GetInput(false);				// Update burner inputs, but not game inputs
				if (ReplayInput()) {			// Read input from file
					SetPauseMode(1);            // Replay has finished
					bAppDoFast = 0;
					bAppDoFasttoggled = 0;      // Disable FFWD
					MenuEnableItems();
					InputSetCooperativeLevel(false, false);
					return 0;
				}
			} else {
				GetInput(true);					// Update inputs
			}
		}

		CallRegisteredLuaFunctions(LUACALL_BEFOREEMULATION);
		if (FBA_LuaUsingJoypad()) {
			FBA_LuaReadJoypad();
		}

		if (nReplayStatus == 1) {
			RecordInput();						// Write input to file
		}

		if (bDraw) {                            // Draw Frame
			if ((bDraw & FFWD_GHOST_FRAME) == 0)
				nFramesRendered++;

			if (!bRunAhead || (BurnDrvGetFlags() & BDF_RUNAHEAD_DISABLED) || bAppDoFast) {
#ifdef FBNEO_DEBUG
				if (bShonkyProfileMode) do_shonky_profile();
#endif
				if (VidFrame()) {				// Do one normal frame (w/o RunAhead)

					// VidFrame() failed, but we must run a driver frame because we have
					// a clocked input.  Possibly from recording or netplay(!)
					// Note: VidFrame() calls BurnDrvFrame() on success.
					pBurnDraw = NULL;			// Make sure no image is drawn
					BurnDrvFrame();

					AudBlankSound();
				}
			} else {
				pBurnDraw = (BurnDrvGetFlags() & BDF_RUNAHEAD_DRAWSYNC) ? pVidImage : NULL;
				BurnDrvFrame();
				StateRunAheadSave();
				INT16 *pBurnSoundOut_temp = pBurnSoundOut;
				pBurnSoundOut = NULL;
				nCurrentFrame++;
				bBurnRunAheadFrame = 1;

				if (VidFrame()) {
					// VidFrame() failed, but we must run a driver frame because we have
					// an input.  Possibly from recording or netplay(!)
					pBurnDraw = NULL;			// Make sure no image is drawn, since video failed this time 'round.
					BurnDrvFrame();
				}

				bBurnRunAheadFrame = 0;
				nCurrentFrame--;
				StateRunAheadLoad();
				pBurnSoundOut = pBurnSoundOut_temp; // restore pointer, for wav & avi writer
			}
		} else {								// frame skipping / ffwd-frame (without avi writing)
			pBurnDraw = NULL;					// Make sure no image is drawn
			BurnDrvFrame();
		}

		if (kNetGame == 0) {                    // Rewind Implementation
			StateRewindDoFrame(macroSystemRewind || bAppDoRewind, macroSystemRewindCancel, bRunPause);
		}

		if (bShowFPS) {
			if (nDoFPS < nFramesRendered) {
				DisplayFPS();
				nDoFPS = nFramesRendered + ((bAppDoFast) ? 15 : 30);
			}
		}

		FBA_LuaFrameBoundary();
		CallRegisteredLuaFunctions(LUACALL_AFTEREMULATION);

#ifdef INCLUDE_AVI_RECORDING
		if (nAviStatus) {
			if (AviRecordFrame(bDraw)) {
				AviStop();
			}
		}
#endif
	}

	bPrevPause = bPause;
	if (bDraw & FFWD_GHOST_FRAME) bDraw = 0; // ffwd-frame w/avi write, do not draw!
	bPrevDraw = bDraw;

	return 0;
}

// Callback used when DSound needs more sound
static int RunGetNextSound(int bDraw)
{
	if (nAudNextSound == NULL) {
		return 1;
	}

	if (bRunPause) {
		if (bAppDoStep) {
			bAppDoStep = 0;									// done one step
			RunFrame(bDraw, 0);
		} else {
			RunFrame(bDraw, 1);
		}
		memset(nAudNextSound, 0, nAudSegLen << 2);	// Write silence into the buffer

		return 0;
	}

	int bBrokeOutOfFFWD = 0;
	if (bAppDoFast) {									// do more frames
		for (int i = 0; i < nFastSpeed; i++) {
			if (!bAppDoFast || bRunPause) {
				bBrokeOutOfFFWD = 1; // recording ended, etc.
				break;
			}                     // break out if no longer in ffwd
#ifdef INCLUDE_AVI_RECORDING
			if (nAviStatus) {
				// Render frame with sound
				pBurnSoundOut = nAudNextSound;
				RunFrame(bDraw | FFWD_GHOST_FRAME, 0);
			} else {
				RunFrame(0, 0);
			}
#else
			RunFrame(0, 0);
#endif
		}
	}

	if (!bBrokeOutOfFFWD) {
		// Render frame with sound
		pBurnSoundOut = nAudNextSound;
		RunFrame(bDraw, 0);
	}

	if (WaveLog != NULL && pBurnSoundOut != NULL) {		// log to the file
		fwrite(pBurnSoundOut, 1, nBurnSoundLen << 2, WaveLog);
		pBurnSoundOut = NULL;
	}

	if (bAppDoStep || (bBrokeOutOfFFWD && bRunPause)) {
		memset(nAudNextSound, 0, nAudSegLen << 2);		// Write silence into the buffer
		AudBlankSound();
	}
	bAppDoStep = 0;										// done one step

	return 0;
}

int RunIdle()
{
	int nTime, nCount;

	if (bAudPlaying) {
		// Run with sound
		AudSoundCheck();
		return 0;
	}

	// Run without sound
	nTime = timeGetTime() - nNormalLast;
	nCount = (nTime * nAppVirtualFps - nNormalFrac) / 100000;
	if (nCount <= 0) {						// No need to do anything for a bit
		//Sleep(2);
		return 0;
	}

	nNormalFrac += nCount * 100000;
	nNormalLast += nNormalFrac / nAppVirtualFps;
	nNormalFrac %= nAppVirtualFps;

	//if (bAppDoFast){						// Temporarily increase virtual fps
	//	nCount *= nFastSpeed;
	//}
	if (nCount > 100) {						// Limit frame skipping
		nCount = 100;
	}
	if (bRunPause) {
		if (bAppDoStep) {					// Step one frame
			nCount = 10;
		} else {
			RunFrame(1, 1);					// Paused
			return 0;
		}
	}
	bAppDoStep = 0;

	if (bAppDoFast) {									// do more frames
		for (int i = 0; i < nFastSpeed; i++) {
#ifdef INCLUDE_AVI_RECORDING
			if (nAviStatus) {
				RunFrame(1, 0);
			} else {
				RunFrame(0, 0);
			}
#else
			RunFrame(0, 0);
#endif
		}
	}

	if(!bAlwaysDrawFrames) {
		for (int i = nCount / 10; i > 0; i--) {	// Mid-frames
			RunFrame(0, 0);
		}
	}
	RunFrame(1, 0);							// End-frame

	return 0;
}

int RunReset()
{
	// Reset the speed throttling code
	nNormalLast = 0; nNormalFrac = 0;

	// Reset FPS display
	DisplayFPSInit();

	if (!bAudPlaying) {
		// run without sound
		nNormalLast = timeGetTime();
	}
	return 0;
}

static int RunInit()
{
	// Try to run with sound
	AudSetCallback(RunGetNextSound);
	AudSoundPlay();

	RunReset();

	return 0;
}

static int RunExit()
{
	nNormalLast = 0;
	// Stop sound if it was playing
	AudSoundStop();

	bAppDoFast = 0;
	bAppDoFasttoggled = 0;
	bAppDoRewind = 0;

	return 0;
}

// Keyboard callback function, a way to provide a driver with shifted/unshifted ASCII data from the keyboard.  see drv/msx/d_msx.cpp for usage
void (*cBurnerKeyCallback)(UINT8 code, UINT8 KeyType, UINT8 down) = NULL;

static void BurnerHandlerKeyCallback(MSG *Msg, INT32 KeyDown, INT32 KeyType)
{
	INT32 shifted = (GetAsyncKeyState(VK_SHIFT) & 0x80000000) ? 0xf0 : 0;
	INT32 scancode = (Msg->lParam >> 16) & 0xFF;
	UINT8 keyboardState[256];
	GetKeyboardState(keyboardState);
	char charvalue[2];
	if (ToAsciiEx(Msg->wParam, scancode, keyboardState, (LPWORD)&charvalue[0], 0, GetKeyboardLayout(0)) == 1) {
		cBurnerKeyCallback(charvalue[0], shifted|KeyType, KeyDown);
	}
}

// Kaillera signals
// they usually come from a different thread, so we have to keep it simple
// (and not involve the windows message pump)
int k_bLoadNetgame = 0;          // (1) signal to start net-game, (2) update menus
int k_player_id = 0;             // data from the Kaillera Client
int k_numplayers = 0;            // ""
char k_game_str[255] = { 0, };   // ""

// The main message loop
int RunMessageLoop()
{
	int bRestartVideo;
	MSG Msg;

	do {
		bRestartVideo = 0;

		// Remove pending initialisation messages from the queue
		while (PeekMessage(&Msg, NULL, WM_APP + 0, WM_APP + 0, PM_NOREMOVE)) {
			if (Msg.message != WM_QUIT)	{
				PeekMessage(&Msg, NULL, WM_APP + 0, WM_APP + 0, PM_REMOVE);
			}
		}

		RunInit();

		ShowWindow(hScrnWnd, nAppShowCmd);												// Show the screen window
		nAppShowCmd = SW_NORMAL;

		SetForegroundWindow(hScrnWnd);

		GameInpCheckLeftAlt();
		GameInpCheckMouse();															// Hide the cursor

		if (bVidDWMSync) {
			bprintf(0, _T("[Win7+] Sync to DWM is enabled (if available).\n"));
			SuperWaitVBlankInit();
		}

		if (bAutoLoadGameList && !nCmdOptUsed) {
			static INT32 bLoaded = 0; // only do this once (at startup!)

			if (bLoaded == 0)
				PostMessage(hScrnWnd, WM_KEYDOWN, VK_F6, 0);
			bLoaded = 1;
		}

		while (1) {
			if (k_bLoadNetgame) {
				if (k_bLoadNetgame == 2) {
					k_bLoadNetgame = 0;
					MenuEnableItems();
					continue;
				}
				k_bLoadNetgame = 0;

				if (bDrvOkay) {
					StopReplay();
#ifdef INCLUDE_AVI_RECORDING
					AviStop();
#endif
					AudSoundStop();										// Stop while we load roms
					DrvExit();
				}

				// find the game
				bool bFound = false;

				for (nBurnDrvActive = 0; nBurnDrvActive < nBurnDrvCount; nBurnDrvActive++) {
					char* szDecoratedName = DecorateKailleraGameName(nBurnDrvActive);

					if (!strcmp(szDecoratedName, k_game_str)) {
						bFound = true;
						break;
					}
				}

				if (!bFound) {
					bprintf(PRINT_ERROR, _T("Kaillera: Can't find game in list!\n"));
					Kaillera_End_Game();
					continue; // carry on!
				}

				kNetGame = 1;

				bCheatsAllowed = false;								// Disable cheats during netplay
				AudSoundStop();										// Stop while we load roms
				DrvInit(nBurnDrvActive, false);						// Init the game driver

				AudSoundPlay();										// Restart sound
				SetFocus(hScrnWnd);
				POST_INITIALISE_MESSAGE;                            // Re-create main window with selected game's dimensions

				TCHAR szTemp1[256];
				TCHAR szTemp2[256];
				VidSAddChatMsg(FBALoadStringEx(hAppInst, IDS_NETPLAY_START, true), 0xFFFFFF, BurnDrvGetText(DRV_FULLNAME), 0xFFBFBF);
				_sntprintf(szTemp1, 256, FBALoadStringEx(hAppInst, IDS_NETPLAY_START_YOU, true), k_player_id);
				_sntprintf(szTemp2, 256, FBALoadStringEx(hAppInst, IDS_NETPLAY_START_TOTAL, true), k_numplayers);
				VidSAddChatMsg(szTemp1, 0xFFFFFF, szTemp2, 0xFFBFBF);
			}

			if (PeekMessage(&Msg, NULL, 0, 0, PM_REMOVE)) {
				// A message is waiting to be processed
				if (Msg.message == WM_QUIT)	{											// Quit program
					break;
				}
				if (Msg.message == (WM_APP + 0)) {										// Restart video
					bRestartVideo = 1;
					break;
				}

				if (bMenuEnabled && nVidFullscreen == 0) {								// Handle keyboard messages for the menu
					if (MenuHandleKeyboard(&Msg)) {
						continue;
					}
				}

				if (Msg.message == WM_SYSKEYDOWN || Msg.message == WM_KEYDOWN) {
					if (Msg.lParam & 0x20000000) {
						// An Alt/AltGr-key was pressed
						switch (Msg.wParam) {

#if defined (FBNEO_DEBUG)
							case 'C': {
								static int count = 0;
								if (count == 0) {
									count++;
									{ char* p = NULL; if (*p) { printf("crash...\n"); } }
								}
								break;
							}
#endif

              				// 'Silence' & 'Sound Restored' Code (added by CaptainCPS-X)
							case 'S': {
								TCHAR buffer[60];
								bMute = !bMute;

								if (bMute) {
									nOldAudVolume = nAudVolume;
									nAudVolume = 0;// mute sound
									_stprintf(buffer, FBALoadStringEx(hAppInst, IDS_SOUND_MUTE, true), nAudVolume / 100);
								} else {
									nAudVolume = nOldAudVolume;// restore volume
									_stprintf(buffer, FBALoadStringEx(hAppInst, IDS_SOUND_MUTE_OFF, true), nAudVolume / 100);
								}
								if (AudSoundSetVolume() == 0) {
									VidSNewShortMsg(FBALoadStringEx(hAppInst, IDS_SOUND_NOVOLUME, true));
								} else {
									VidSNewShortMsg(buffer);
								}
								break;
							}

							case VK_OEM_PLUS: {
								if (bMute) break; // if mute, not do this
								nOldAudVolume = nAudVolume;
								TCHAR buffer[60];

								if (GetAsyncKeyState(VK_CONTROL) & 0x80000000) {
									nAudVolume += 100;
								} else {
									nAudVolume += 1000;
								}

								if (nAudVolume > 10000) {
									nAudVolume = 10000;
								}
								if (AudSoundSetVolume() == 0) {
									VidSNewShortMsg(FBALoadStringEx(hAppInst, IDS_SOUND_NOVOLUME, true));
								} else {
									_stprintf(buffer, FBALoadStringEx(hAppInst, IDS_SOUND_VOLUMESET, true), nAudVolume / 100);
									VidSNewShortMsg(buffer);
								}
								MenuUpdateVolume();
								break;
							}
							case VK_OEM_MINUS: {
								if (bMute) break; // if mute, not do this
							  	nOldAudVolume = nAudVolume;
								TCHAR buffer[60];

								if (GetAsyncKeyState(VK_CONTROL) & 0x80000000) {
									nAudVolume -= 100;
								} else {
									nAudVolume -= 1000;
								}

								if (nAudVolume < 0) {
									nAudVolume = 0;
								}
								if (AudSoundSetVolume() == 0) {
									VidSNewShortMsg(FBALoadStringEx(hAppInst, IDS_SOUND_NOVOLUME, true));
								} else {
									_stprintf(buffer, FBALoadStringEx(hAppInst, IDS_SOUND_VOLUMESET, true), nAudVolume / 100);
									VidSNewShortMsg(buffer);
								}
								MenuUpdateVolume();
								break;
							}
							case VK_MENU: {
								continue;
							}

							// Open (L)ua Dialog
							case 'L': {
								ScrnInitLua();
								break;
							}
							case 'T': {
								PostMessage(LuaConsoleHWnd, WM_CLOSE, 0, 0);
								break;
							}
							case 'E': {
								PostMessage(LuaConsoleHWnd, WM_COMMAND, IDC_BUTTON_LUARUN, 0);
								break;
							}
			                // (P)ause Lua Scripting (This is the stop button)
							case 'P': {
								PostMessage(LuaConsoleHWnd, WM_COMMAND, IDC_BUTTON_LUASTOP, 0);
								break;
							}
							// (B)rowse Lua Scripts
							case 'B': {
								PostMessage(LuaConsoleHWnd, WM_COMMAND, IDC_BUTTON_LUABROWSE, 0);
								break;
							}
							break;
						}
					} else {

						if (cBurnerKeyCallback)
							BurnerHandlerKeyCallback(&Msg, (Msg.message == WM_KEYDOWN) ? 1 : 0, 0);

						switch (Msg.wParam) {

#if defined (FBNEO_DEBUG)
							case 'N':
								counter--;
								if (counter < 0) {
									bprintf(PRINT_IMPORTANT, _T("*** New counter value: %04X (%d).\n"), counter, counter);
								} else {
									bprintf(PRINT_IMPORTANT, _T("*** New counter value: %04X.\n"), counter);
								}
								break;
							case 'M':
								counter++;
								if (counter < 0) {
									bprintf(PRINT_IMPORTANT, _T("*** New counter value: %04X (%d).\n"), counter, counter);
								} else {
									bprintf(PRINT_IMPORTANT, _T("*** New counter value: %04X.\n"), counter);
								}
								break;
#endif
							case VK_ESCAPE: {
								if (hwndChat) {
									DeActivateChat();
								} else {
									if (nCmdOptUsed & 1) {
										PostQuitMessage(0);
									} else {
										if (nVidFullscreen) {
											nVidFullscreen = 0;
											POST_INITIALISE_MESSAGE;
										}
									}
								}
								break;
							}
							case VK_RETURN: {
								if (hwndChat) {
									int i = 0;
									while (EditText[i]) {
										if (EditText[i++] != 0x20) {
											break;
										}
									}
									if (i) {
										Kaillera_Chat_Send(TCHARToANSI(EditText, NULL, 0));
										//kailleraChatSend(TCHARToANSI(EditText, NULL, 0));
									}
									DeActivateChat();

									break;
								}
								if (GetAsyncKeyState(VK_CONTROL) & 0x80000000) {
									bMenuEnabled = !bMenuEnabled;
									POST_INITIALISE_MESSAGE;

									break;
								}

								break;
							}
							case VK_PRIOR: {
								bAppDoRewind = 1;
								break;
							}
							case VK_F1: {
								bool bOldAppDoFast = bAppDoFast;

								if (kNetGame) {
									break;
								}

								if (((GetAsyncKeyState(VK_CONTROL) | GetAsyncKeyState(VK_SHIFT)) & 0x80000000) == 0) {
									if (bRunPause) {
										bAppDoStep = 1;
									} else {
										bAppDoFast = 1;
									}
								}

								if ((GetAsyncKeyState(VK_SHIFT) & 0x80000000) && !GetAsyncKeyState(VK_CONTROL)) { // Shift-F1: toggles FFWD state
									bAppDoFast = !bAppDoFast;
									bAppDoFasttoggled = bAppDoFast;
								}

								if (bOldAppDoFast != bAppDoFast) {
									DisplayFPSInit(); // resync fps display
								}
								break;
							}
							case VK_BACK: {
								if ((GetAsyncKeyState(VK_SHIFT) & 0x80000000) && !GetAsyncKeyState(VK_CONTROL))
								{ // Shift-Backspace: toggles recording/replay frame counter
									bReplayFrameCounterDisplay = !bReplayFrameCounterDisplay;
									if (!bReplayFrameCounterDisplay) {
										VidSKillTinyMsg();
									}
								} else
								{ // Backspace: toggles FPS counter
									bShowFPS = !bShowFPS;
									if (bShowFPS) {
										DisplayFPSInit();
									} else {
										VidSKillShortMsg();
									}
								}
								break;
							}
							case VK_OEM_2: {
								if (kNetGame && hwndChat == NULL) {
									if (AppMessage(&Msg)) {
										ActivateChat();
									}
								}
								break;
							}
						}
					}
				} else {
					if (Msg.message == WM_SYSKEYUP || Msg.message == WM_KEYUP) {

						if (cBurnerKeyCallback)
							BurnerHandlerKeyCallback(&Msg, (Msg.message == WM_KEYDOWN) ? 1 : 0, 0);

						switch (Msg.wParam) {
							case VK_MENU:
								continue;

							case VK_PRIOR:
								bAppDoRewind = 0;
								break;

							case VK_F1: {
								bool bOldAppDoFast = bAppDoFast;

								if (!bAppDoFasttoggled)
									bAppDoFast = 0;
								bAppDoFasttoggled = 0;
								if (bOldAppDoFast != bAppDoFast) {
									DisplayFPSInit(); // resync fps display
								}
								break;
							}
						}
					}
				}

				// Check for messages for dialogs etc.
				if (AppMessage(&Msg)) {
					if (TranslateAccelerator(hScrnWnd, hAccel, &Msg) == 0) {
						if (hwndChat) {
							TranslateMessage(&Msg);
						}
						DispatchMessage(&Msg);
					}
				}
			} else {
				// No messages are waiting
				SplashDestroy(0);
				RunIdle();
			}
		}

		RunExit();
		MediaExit();
		if (bRestartVideo) {
			MediaInit();
		}
	} while (bRestartVideo);

	return 0;
}
