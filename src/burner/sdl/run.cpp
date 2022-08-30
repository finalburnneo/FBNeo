// Run module
#include "burner.h"
#ifdef BUILD_SDL2
#include "sdl2_gui_common.h"
#endif
#include <sys/time.h>

static unsigned int nDoFPS = 0;
bool bAltPause = 0;

int bAlwaysDrawFrames = 0;

int counter;                                // General purpose variable used when debugging

static unsigned int nNormalLast = 0;        // Last value of GetTime()
static int          nNormalFrac = 0;        // Extra fraction we did

static bool bAppDoStep = 0;
bool        bAppDoFast = 0;
bool        bAppShowFPS = 0;
static int  nFastSpeed = 6;
static bool bscreenshot = 0;

// SlowMo T.A. feature
int nSlowMo = 0;
static int flippy = 0; // free running RunFrame() counter

UINT32 messageFrames = 0;
char lastMessage[MESSAGE_MAX_LENGTH];

#ifdef BUILD_SDL2
static Uint32 starting_stick;
/// Ingame gui
extern SDL_Renderer* sdlRenderer;
extern void ingame_gui_start(SDL_Renderer* renderer);
/// Save States
static char Windowtitle[512];

extern void AdjustImageSize();		// vid_sdl2.cpp
#endif

int bDrvSaveAll = 0;

// The automatic save
int StatedAuto(int bSave)
{
	static TCHAR szName[MAX_PATH] = _T("");
	int nRet;

#if defined(BUILD_SDL2) && !defined(SDL_WINDOWS)
	_stprintf(szName, _T("%s%s.fs"), szAppEEPROMPath, BurnDrvGetText(DRV_NAME));
#else
	_stprintf(szName, _T("config/games/%s.fs"), BurnDrvGetText(DRV_NAME));
#endif

	if (bSave == 0)
	{
		printf("loading state %i %s\n", bDrvSaveAll, szName);
		nRet = BurnStateLoad(szName, bDrvSaveAll, NULL);		// Load ram
		if (nRet && bDrvSaveAll)
		{
			nRet = BurnStateLoad(szName, 0, NULL);				// Couldn't get all - okay just try the nvram
		}
	}
	else
	{
		printf("saving state %i %s\n", bDrvSaveAll, szName);
		nRet = BurnStateSave(szName, bDrvSaveAll);				// Save ram
	}

	return nRet;
}


/// End Save States

char fpsstring[20];

static time_t fpstimer;
static unsigned int nPreviousFrames;

static void DisplayFPSInit()
{
	nDoFPS = 0;
	fpstimer = 0;
	nPreviousFrames = nFramesRendered;
}

static void DisplayFPS()
{
	time_t temptime = clock();
	double fps = (double)(nFramesRendered - nPreviousFrames) * CLOCKS_PER_SEC / (temptime - fpstimer);
	if (bAppDoFast) {
		fps *= nFastSpeed + 1;
	}
	if (fpstimer && temptime - fpstimer > 0) { // avoid strange fps values
		sprintf(fpsstring, "%2.2lf", fps);
	}

	fpstimer = temptime;
	nPreviousFrames = nFramesRendered;
}


//crappy message system
void UpdateMessage(char* message)
{
	snprintf(lastMessage, MESSAGE_MAX_LENGTH, "%s", message);
	messageFrames = MESSAGE_MAX_FRAMES;
}

// define this function somewhere above RunMessageLoop()
void ToggleLayer(unsigned char thisLayer)
{
	nBurnLayer ^= thisLayer;                         // xor with thisLayer
	VidRedraw();
	VidPaint(0);
}


struct timeval start;

unsigned int GetTime(void)
{
	unsigned int ticks;
	struct timeval now;
	gettimeofday(&now, NULL);
	ticks = (now.tv_sec - start.tv_sec) * 1000000 + now.tv_usec - start.tv_usec;
	return ticks;
}

// With or without sound, run one frame.
// If bDraw is true, it's the last frame before we are up to date, and so we should draw the screen
static int RunFrame(int bDraw, int bPause)
{
	if (!bDrvOkay)
	{
		return 1;
	}

	// SlowMo stuff
	flippy++;
	nSlowMo = macroSystemSlowMo[0] + macroSystemSlowMo[1] * 2 + macroSystemSlowMo[2] * 3 + macroSystemSlowMo[3] * 4 + macroSystemSlowMo[4] * 5;
	if ((nSlowMo == 1) && ((flippy % 4) == 0)) return 0;		// 75% speed
	else if ((nSlowMo > 1) && (nSlowMo < 6) && (flippy % ((nSlowMo - 1) * 2)) < (((nSlowMo - 1) * 2) - 1)) return 0;		// 50% and less

	if (bPause)
	{
		InputMake(false);
		VidPaint(0);
	}
	else
	{
		nFramesEmulated++;
		nCurrentFrame++;
		InputMake(true);
	}

	if (bDraw)
	{
		nFramesRendered++;

		if (!bRunAhead || bAppDoFast) {     // Run-Ahead feature 				-dink aug 02, 2021
			if (VidFrame()) {				// Do one frame w/o RunAhead or if FFWD is pressed.
				AudBlankSound();
			}
		} else {
			pBurnDraw = NULL;               // Do one frame w/RunAhead
			BurnDrvFrame();
			StateRunAheadSave();
			pBurnSoundOut = NULL;
			VidFrame();
			StateRunAheadLoad();
		}

		VidPaint(0);                                              // paint the screen (no need to validate)
	}
	else
	{                                       // frame skipping
		pBurnDraw = NULL;                    // Make sure no image is drawn
		BurnDrvFrame();
	}

	if (bAppShowFPS) {
		if (nDoFPS < nFramesRendered) {
			DisplayFPS();
			nDoFPS = nFramesRendered + 30;
		}
	}

	return 0;
}

// Callback used when DSound needs more sound
static int RunGetNextSound(int bDraw)
{
	if (nAudNextSound == NULL)
	{
		return 1;
	}

	if (bRunPause)
	{
		if (bAppDoStep)
		{
			RunFrame(bDraw, 0);
			memset(nAudNextSound, 0, nAudSegLen << 2);                                        // Write silence into the buffer
		}
		else
		{
			RunFrame(bDraw, 1);
		}

		bAppDoStep = 0;                                                   // done one step
		return 0;
	}

	if (bAppDoFast)
	{                                            // do more frames
		for (int i = 0; i < nFastSpeed; i++)
		{
			RunFrame(0, 0);
		}
	}

	// Render frame with sound
	pBurnSoundOut = nAudNextSound;
	RunFrame(bDraw, 0);
	if (bAppDoStep)
	{
		memset(nAudNextSound, 0, nAudSegLen << 2);                // Write silence into the buffer
	}
	bAppDoStep = 0;                                              // done one step

	return 0;
}

int delay_ticks(int ticks)
{
//sdl_delay can take up to 10 - 15 ticks it doesnt guarentee below this
   int startTicks = 0;
   int endTicks = 0;
   int checkTicks = 0;

   startTicks=SDL_GetTicks();

   while (checkTicks <= ticks)
   {
      endTicks=SDL_GetTicks();
      checkTicks = endTicks - startTicks;
   }

   return ticks;
}
int RunIdle()
{
	int nTime, nCount;

	if (bAudPlaying)
	{
		// Run with sound
		AudSoundCheck();
		return 0;
	}

	// Run without sound
	nTime = GetTime() - nNormalLast;
	nCount = (nTime * nAppVirtualFps - nNormalFrac) / 100000;
	if (nCount <= 0) {						// No need to do anything for a bit
		//delay_ticks(2);
		return 0;
	}

	nNormalFrac += nCount * 100000;
	nNormalLast += nNormalFrac / nAppVirtualFps;
	nNormalFrac %= nAppVirtualFps;

	if (nCount > 100) {						// Limit frame skipping
		nCount = 100;
	}
	if (bRunPause) {
		if (bAppDoStep) {					// Step one frame
			nCount = 10;
		}
		else {
			RunFrame(1, 1);					// Paused
			return 0;
		}
	}
	bAppDoStep = 0;


	if (bAppDoFast)
	{									// do more frames
		for (int i = 0; i < nFastSpeed; i++)
		{
			RunFrame(0, 0);
		}
	}

	if (!bAlwaysDrawFrames)
	{
		for (int i = nCount / 10; i > 0; i--)
		{              // Mid-frames
			RunFrame(0, 0);
		}
	}
	RunFrame(1, 0);                                  // End-frame
	// temp added for SDLFBA
	//VidPaint(0);
	return 0;
}

int RunReset()
{
	// Reset the speed throttling code
	nNormalLast = 0; nNormalFrac = 0;
	if (!bAudPlaying)
	{
		// run without sound
		nNormalLast = GetTime();
	}
	return 0;
}

int RunInit()
{
	gettimeofday(&start, NULL);
	DisplayFPSInit();
	// Try to run with sound
	AudSetCallback(RunGetNextSound);
	AudSoundPlay();

	RunReset();
	StatedAuto(0);
	return 0;
}

int RunExit()
{
	nNormalLast = 0;
	StatedAuto(1);
	return 0;
}

#ifdef BUILD_SDL2
void pause_game()
{
	AudSoundStop();	
	
	if(nVidSelect) {
		// no Text in OpenGL...
		SDL_GL_SwapWindow(sdlWindow);
	}else{
		inprint_shadowed(sdlRenderer, "PAUSE", 10, 10);
		SDL_RenderPresent(sdlRenderer);
	}
	
    int finished = 0;
	while (!finished)
  	{
		starting_stick = SDL_GetTicks();
		
 		SDL_Event e;

		while (SDL_PollEvent(&e))
		{
			if (e.type == SDL_QUIT)
			{
				finished=1;
			}
			if (e.type == SDL_KEYDOWN)
			{
			  switch (e.key.keysym.sym)
			  {
				  case SDLK_TAB:
				  case SDLK_p:
					finished=1;
					break;
				  default:
					break;
			  }
			}
			if (e.type == SDL_WINDOWEVENT)  
			{ // Window Event
				switch (e.window.event) 
				{
					//case SDL_WINDOWEVENT_RESTORED: // keep pause when restore window
					case SDL_WINDOWEVENT_FOCUS_GAINED:
						finished=1;
						break;
					case SDL_WINDOWEVENT_CLOSE:
						finished=1;
						RunExit();
						break;
					default:
						break;
				}
			}
		}
		
		// limit 5 FPS (free CPU usage)		
		if ( ( 1000 / 5 ) > SDL_GetTicks() - starting_stick) {
			SDL_Delay( 1000 / 5 - ( SDL_GetTicks() - starting_stick ) );
		}
		
	}	
	
	AudSoundPlay();	
}
#endif

#ifndef BUILD_MACOS
// The main message loop
int RunMessageLoop()
{
	int quit = 0;

	RunInit();
	GameInpCheckMouse();                                                                     // Hide the cursor

	while (!quit)
	{
		
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
			case SDL_QUIT:                                        /* Windows was closed */
				quit = 1;
				break;

#ifdef BUILD_SDL2
			case SDL_WINDOWEVENT:  // Window Event
				switch (event.window.event) 
				{
					case SDL_WINDOWEVENT_MINIMIZED:
					case SDL_WINDOWEVENT_FOCUS_LOST:
						pause_game();
						break;
				}
				break;			
#endif
					
			case SDL_KEYDOWN:                                                // need to find a nicer way of doing this...
				switch (event.key.keysym.sym)
				{
				case SDLK_F1:
					bAppDoFast = 1;
					break;
				case SDLK_F9:
					QuickState(0);
					break;
				case SDLK_F10:
					QuickState(1);
					break;
				case SDLK_F11:
					bAppShowFPS = !bAppShowFPS;
#ifdef BUILD_SDL2
					if (!bAppShowFPS)
					{
						sprintf(Windowtitle, "FBNeo - %s - %s", BurnDrvGetTextA(DRV_NAME), BurnDrvGetTextA(DRV_FULLNAME));
						SDL_SetWindowTitle(sdlWindow, Windowtitle);
					}
#endif
					break;
#ifdef BUILD_SDL2
				case SDLK_TAB:
					if(!nVidSelect) {
						ingame_gui_start(sdlRenderer);
					} else {
						// Pause with SDL2 OpenGL mode
						pause_game();
					}
					break;
				
				case SDLK_RETURN:
					if (event.key.keysym.mod & KMOD_ALT) 
					{
						SetFullscreen(!GetFullscreen());
						AdjustImageSize();
					}
					break;
#endif
				case SDLK_F6: // screeenshot
					if (!bscreenshot) {
						MakeScreenShot();
						bscreenshot = 1;
					}
					break;
				case SDLK_KP_MINUS: // volumme -
					nAudVolume -= 500;
					if (nAudVolume < 0) {
						nAudVolume = 0;
					}
					if (AudSoundSetVolume() == 0) {
					}
					break;
				case SDLK_KP_PLUS: // volume -+
					nAudVolume += 500;
					if (nAudVolume > 10000) {
						nAudVolume = 10000;
					}
					if (AudSoundSetVolume() == 0) {
					}
					break;
				default:
					break;
				}
				break;

			case SDL_KEYUP:                                                // need to find a nicer way of doing this...
				switch (event.key.keysym.sym)
				{
				case SDLK_F1:
					bAppDoFast = 0;
					break;
				case SDLK_F6: 
					bscreenshot = 0;
					break;
				case SDLK_F12:
					quit = 1;
					break;

				default:
					break;
				}
				break;
			}
		}
		
		RunIdle();

	}

	RunExit();

	return 0;
}

#endif
