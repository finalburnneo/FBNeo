/*----------------
Stuff to finish:

It wouldn't be a stretch of the imagination to think the whole of the sdl 'port' needs a redo but here are the main things wrong with this version:


There is OSD of any kind which makes it hard to display info to the users.
There are lots of problems with the audio output code.
There are lots of problems with the opengl renderer
probably many other things.
------------------*/
#include "burner.h"

int nAppVirtualFps = 6000;			// App fps * 100
bool bRunPause=0;
bool bAlwaysProcessKeyboardInput=0;

void CheckFirstTime()
{

}

void ProcessCommandLine(int argc, char *argv[])
{

}

#undef main

int main(int argc, char *argv[])
{
	UINT32 i=0;

	ConfigAppLoad();

	CheckFirstTime(); // check for first time run

	SDL_Init(SDL_INIT_TIMER|SDL_INIT_VIDEO);

	BurnLibInit();

	SDL_WM_SetCaption( "FBA, SDL port.", "FBA, SDL port.");
	SDL_ShowCursor(SDL_DISABLE);

	if (argc < 2)
	{
		int c;
		printf ("Usage: fbasdl <romname>\n   ie: fbasdl uopoko\n Note: no extension.\n\n");

		return 0;
	}

	if (argc == 2)
	{
		for (i = 0; i < nBurnDrvCount; i++) {
			//nBurnDrvSelect[0] = i;
			nBurnDrvActive = i;
			if (strcmp(BurnDrvGetTextA(0), argv[1]) == 0) {
				break;
			}
		}

		if (i == nBurnDrvCount) {
			printf("%s is not supported by FB Alpha.",argv[1]);
			return 1;
		}
	}

	bBurnUseASMCPUEmulation = 0;
	bCheatsAllowed = false;
	ConfigAppLoad();
	ConfigAppSave();

	DrvInit(i, 0);

	RunMessageLoop();

	DrvExit();
	MediaExit();

	ConfigAppSave();
	BurnLibExit();
	//SDL_Quit();

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
