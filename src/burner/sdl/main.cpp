/*----------------
Stuff to finish:

It wouldn't be a stretch of the imagination to think the whole of the sdl 'port' needs a redo but here are the main things wrong with this version:


There is OSD of any kind which makes it hard to display info to the users.
There are lots of problems with the audio output code.
There are lots of problems with the opengl renderer
probably many other things.
------------------*/


#include "burner.h"
#define KEY(x) { pgi->nInput = GIT_SWITCH; pgi->Input.Switch.nCode = (UINT16)(x); }
static int GameInpConfig(int nPlayer, int nPcDev, int nAnalog);
static void GameInpConfigOne(int nPlayer, int nPcDev, int nAnalog, struct GameInp* pgi, char* szi);
INT32 display_set_controls();
INT32 Mapcoins(struct GameInp* pgi, char* szi, INT32 nPlayer, INT32 nDevice);

int nAppVirtualFps = 6000;			// App fps * 100
bool bRunPause=0;
bool bAlwaysProcessKeyboardInput=0;
int usejoy=0;
void CheckFirstTime()
{

}

int parseSwitches(int argc, char *argv[])
{
        for (int i = 1; i < argc; i++) {
                if (*argv[i] != '-') {
			continue;
		}

		if (strcmp(argv[i] + 1, "joy") == 0) {
			usejoy=1;
		}

	}

	return 0;
}

#undef main

int main(int argc, char *argv[])
{
	
	const char *romname = NULL;
	UINT32 i=0;

	for (int i = 1; i < argc; i++) {
		if (*argv[i] != '-') {
			romname = argv[i];
		}
	}

	if (romname == NULL) {
		printf("Usage: %s [-joy]  <romname>\n", argv[0]);
		printf("e.g.: %s mslug\n", argv[0]);

		return 0;
	}
	
	ConfigAppLoad();

	CheckFirstTime(); // check for first time run

	SDL_Init(SDL_INIT_TIMER|SDL_INIT_VIDEO);

	BurnLibInit();

	SDL_WM_SetCaption("FinalBurn Neo", "FinalBurn Neo");
	SDL_ShowCursor(SDL_DISABLE);

int driverId = -1;
	for (int i = 0; i < (int) nBurnDrvCount; i++) {
		nBurnDrvActive = i;
		if (strcmp(BurnDrvGetTextA(0), romname) == 0) {
			driverId = i;
			break;
		}
	}

	if (driverId < 0) {
		fprintf(stderr, "%s is not supported by FinalBurn Neo\n", romname);
		return 1;
	}

	printf("Starting %s\n", romname);
	
	bCheatsAllowed = false;
	ConfigAppLoad();
	ConfigAppSave();
	if (!DrvInit(driverId, 0))
	{
		parseSwitches(argc, argv);

		if (!usejoy){
			/*keyboard p1, joy0 p2) */
			GameInpConfig(0, 0, 1);
			GameInpConfig(1, 1, 1);
		}
		else 
		{
			/* p1 joy0 , p2 joy1 */
			GameInpConfig(0, 1, 1);
			GameInpConfig(1, 2, 1);
		}
		display_set_controls();
		RunMessageLoop();
	}
	else
	{
		printf("There was an error loading your selected game.\n");
	}

	DrvExit();
	MediaExit();

	ConfigAppSave();
	BurnLibExit();
	SDL_Quit();

	return 0;
}


/* const */ TCHAR* ANSIToTCHAR(const char* pszInString, TCHAR* pszOutString, int nOutSize)
{
#if defined (UNICODE)
	static TCHAR szStringBuffer[1024];

	TCHAR* pszBuffer = pszOutString ? pszOutString : szStringBuffer;
	int nBufferSize  = pszOutString ? nOutSize * 2 : sizeof(szStringBuffer);

	if (MultiByteToWideChar(CP_ACP, 0, pszInString, -1, pszBuffer, nBufferSize)) {
		return pszBuffer;
	}

	return NULL;
#else
	if (pszOutString) {
		_tcscpy(pszOutString, pszInString);
		return pszOutString;
	}

	return (TCHAR*)pszInString;
#endif
}


/* const */ char* TCHARToANSI(const TCHAR* pszInString, char* pszOutString, int nOutSize)
{
#if defined (UNICODE)
	static char szStringBuffer[1024];
	memset(szStringBuffer, 0, sizeof(szStringBuffer));

	char* pszBuffer = pszOutString ? pszOutString : szStringBuffer;
	int nBufferSize = pszOutString ? nOutSize * 2 : sizeof(szStringBuffer);

	if (WideCharToMultiByte(CP_ACP, 0, pszInString, -1, pszBuffer, nBufferSize, NULL, NULL)) {
		return pszBuffer;
	}

	return NULL;
#else
	if (pszOutString) {
		strcpy(pszOutString, pszInString);
		return pszOutString;
	}

	return (char*)pszInString;
#endif
}


bool AppProcessKeyboardInput()
{
	return true;
}


static int GameInpConfig(int nPlayer, int nPcDev, int nAnalog) {
	struct GameInp* pgi = NULL;
	unsigned int i;
	for (i = 0, pgi = GameInp; i < nGameInpCount; i++, pgi++) {
		struct BurnInputInfo bii;
		// Get the extra info about the input
		bii.szInfo = NULL;
		BurnDrvGetInputInfo(&bii, i);
		if (bii.pVal == NULL) {
			continue;
		}
		if (bii.szInfo == NULL) {
			bii.szInfo = "";
		}
		GameInpConfigOne(nPlayer, nPcDev, nAnalog, pgi, bii.szInfo);
	}
	for (i = 0; i < nMacroCount; i++, pgi++) {
		GameInpConfigOne(nPlayer, nPcDev, nAnalog, pgi, pgi->Macro.szName);
	}
	GameInpCheckLeftAlt();
	return 0;
}

static void GameInpConfigOne(int nPlayer, int nPcDev, int nAnalog, struct GameInp* pgi, char* szi) {
	switch (nPcDev) {
		case 0:
			GamcPlayer(pgi, szi, nPlayer, -1); // Keyboard
			GamcAnalogKey(pgi, szi, nPlayer, nAnalog);
			GamcMisc(pgi, szi, nPlayer);
 			Mapcoins(pgi, szi, nPlayer, nPcDev);
			break;
		case 1:
			GamcPlayer(pgi, szi, nPlayer, 0); // Joystick 1
			GamcAnalogJoy(pgi, szi, nPlayer, 0, nAnalog);
			GamcMisc(pgi, szi, nPlayer);
			 Mapcoins(pgi, szi, nPlayer, nPcDev);


			break;
		case 2:
			GamcPlayer(pgi, szi, nPlayer, 1); // Joystick 2
			GamcAnalogJoy(pgi, szi, nPlayer, 1, nAnalog);
			GamcMisc(pgi, szi, nPlayer);
                        Mapcoins(pgi, szi, nPlayer, nPcDev);

			break;
		case 3:
			GamcPlayer(pgi, szi, nPlayer, 2); // Joystick 3
			GamcAnalogJoy(pgi, szi, nPlayer, 2, nAnalog);
			GamcMisc(pgi, szi, nPlayer);
			break;
		case 4:
			GamcPlayerHotRod(pgi, szi, nPlayer, 0x10, nAnalog); // X-Arcade left side
			GamcMisc(pgi, szi, -1);
			break;
		case 5:
			GamcPlayerHotRod(pgi, szi, nPlayer, 0x11, nAnalog); // X-Arcade right side
			GamcMisc(pgi, szi, -1);
			break;
		case 6:
			GamcPlayerHotRod(pgi, szi, nPlayer, 0x00, nAnalog); // HotRod left side
			GamcMisc(pgi, szi, -1);
			break;
		case 7:
			GamcPlayerHotRod(pgi, szi, nPlayer, 0x01, nAnalog); // HotRod right size
			GamcMisc(pgi, szi, -1);
			break;
	}
}

INT32 Mapcoins(struct GameInp* pgi, char* szi, INT32 nPlayer, INT32 nDevice)
{
	INT32 nJoyBase = 0;
	if (nDevice == 0) return 0; // using keyboard for this player
 	nDevice--; 
	nJoyBase = 0x4000;
        nJoyBase |= nDevice << 8;

	switch (nPlayer) 
	{
		case 0:
			if (strcmp(szi, "p1 coin" ) == 0 && nPlayer == 0 ) 
			{
                                KEY(nJoyBase + 0x80 + 8);
                                return 0;
			        
        		}
                        if (strcmp(szi, "p1 start") == 0 && nPlayer == 0 )
                        {
                                KEY(nJoyBase + 0x80 + 9);
                                return 0;

                        }

			break;
                case 1:
                        if (strcmp(szi, "p2 coin" ) == 0 && nPlayer == 1 )
                        {
                                KEY(nJoyBase + 0x80 + 8);
                                return 0;

                        }
                        if (strcmp(szi, "p2 start") == 0 && nPlayer == 1 )
                        {
                                KEY(nJoyBase + 0x80 + 9);
                                return 0;

                        }

                        break;

	}

	return 0;
}


INT32 display_set_controls()
{
        struct GameInp* pgi = NULL;
        unsigned int i;
        for (i = 0, pgi = GameInp; i < nGameInpCount; i++, pgi++) {
                struct BurnInputInfo bii;
                // Get the extra info about the input
                bii.szInfo = NULL;
                BurnDrvGetInputInfo(&bii, i);
                if (bii.pVal == NULL) {
                        continue;
                }
                if (bii.szInfo == NULL) {
                        bii.szInfo = "";
                }
                printf("%s %s\n",  bii.szInfo ,InputCodeDesc( pgi->Input.Switch.nCode));

        }

        return 0;
}

