#include "burner.h"
#include <sys/stat.h>
#include <unistd.h>

extern int nExitEmulator;
extern int nEnableFreeplayHack;

int nAppVirtualFps = 6000;			// App fps * 100
bool bRunPause = 0;
bool bAlwaysProcessKeyboardInput=0;
TCHAR szAppBurnVer[16];

void formatBinary(int number, int sizeBytes, char *dest, int len)
{
	int size = sizeBytes * 8;
	if (size + 1 > len) {
		*dest = '\0';
		return;
	}

	char *ch = dest + size;
	*ch = '\0';
	ch--;

	for (int i = 0; i < size; i++, ch--) {
		*ch = (number & 1) + '0';
		number >>= 1;
	}
}

void dumpDipSwitches()
{
	BurnDIPInfo bdi;
	char flags[9], mask[9], setting[9];

	for (int i = 0; BurnDrvGetDIPInfo(&bdi, i) == 0; i++) {
		formatBinary(bdi.nFlags, sizeof(bdi.nFlags), flags, sizeof(flags));
		formatBinary(bdi.nMask, sizeof(bdi.nMask), mask, sizeof(mask));
		formatBinary(bdi.nSetting, sizeof(bdi.nSetting), setting, sizeof(setting));

		printf("%d: % 3d - 0x%02x (%s) - 0x%02x (%s) - 0x%02x (%s) - %s\n",
			i,
			bdi.nInput,
			bdi.nFlags, flags,
			bdi.nMask, mask,
			bdi.nSetting, setting,
			bdi.szText);
	}
}

int enableFreeplay()
{
	int dipOffset = 0;
	int switchFound = 0;
	BurnDIPInfo dipSwitch;

	for (int i = 0; !switchFound && BurnDrvGetDIPInfo(&dipSwitch, i) == 0; i++) {
		if (dipSwitch.nFlags == 0xF0) {
			dipOffset = dipSwitch.nInput;
		}
		if (dipSwitch.szText && (strcasecmp(dipSwitch.szText, "freeplay") == 0
			|| strcasecmp(dipSwitch.szText, "free play") == 0)) {
			// Found freeplay. Is it a group or the actual switch?
			if (dipSwitch.nFlags & 0x40) {
				// It's a group. Find the switch
				for (int j = i + 1; !switchFound && BurnDrvGetDIPInfo(&dipSwitch, j) == 0 && !(dipSwitch.nFlags & 0x40); j++) {
					if (dipSwitch.szText && strcasecmp(dipSwitch.szText, "on") == 0) {
						// Found the switch
						switchFound = 1;
					}
				}
			} else {
				// It's a switch
				switchFound = 1;
			}
		}
	}

	if (switchFound) {
		struct GameInp *pgi = GameInp + dipSwitch.nInput + dipOffset;
		pgi->Input.Constant.nConst = (pgi->Input.Constant.nConst & ~dipSwitch.nMask) | (dipSwitch.nSetting & dipSwitch.nMask);
	}

	return switchFound;
}

int setDipSwitch(int switchNo)
{
	int dipOffset = 0;
	int groupSet = 0;
	BurnDIPInfo dipSwitch;
	BurnDIPInfo group;

	for (int i = 0; BurnDrvGetDIPInfo(&dipSwitch, i) == 0; i++) {
		if (dipSwitch.nFlags == 0xF0) {
			dipOffset = dipSwitch.nInput;
		}
		if (dipSwitch.nFlags & 0x40) {
			groupSet = 1;
			BurnDrvGetDIPInfo(&group, i);
		}
		if (i != switchNo) {
			continue;
		}
		if (dipSwitch.nFlags & 0x40) {
			// A group
			fprintf(stderr, "Can't set dipswitch - group, not switch\n");
			break;
		}

		struct GameInp *pgi = GameInp + dipSwitch.nInput + dipOffset;
		pgi->Input.Constant.nConst = (pgi->Input.Constant.nConst & ~dipSwitch.nMask) | (dipSwitch.nSetting & dipSwitch.nMask);

		if (groupSet) {
			printf("Set '%s': %s\n", group.szText, dipSwitch.szText);
		} else {
			printf("Set %s\n", dipSwitch.szText);
		}

		return 1;
	}

	return 0;
}

int parseSwitches(int argc, char *argv[])
{
	int dipSwitchSet = 0;
        for (int i = 1; i < argc; i++) {
                if (*argv[i] != '-') {
			continue;
		}

		if (strcmp(argv[i] + 1, "f") == 0) {
			if (enableFreeplay()) {
				printf("Freeplay enabled\n");
			} else {
				fprintf(stderr, "Don't know how to enable freeplay - try the hack\n");
			}
		} else if (strcmp(argv[i] + 1, "F") == 0) {
			nEnableFreeplayHack = 1;
			printf("Freeplay hack enabled\n");
		} else if (strcmp(argv[i] + 1, "dumpswitches") == 0) {
			dumpDipSwitches();
		} else if (strncmp(argv[i] + 1, "ds=", 3) == 0) {
			char format[16];
			strncpy(format, argv[i] + 4, sizeof(format) - 1);
			int switchNo = atoi(format);
			if (switchNo != 0) {
				dipSwitchSet |= setDipSwitch(switchNo);
			}
		}
	}

	return dipSwitchSet;
}

int main(int argc, char *argv[])
{
	const char *romname = NULL;
	nEnableFreeplayHack = 0;

	// Make version string
	if (nBurnVer & 0xFF)
	{
		 // private version (alpha)
		 _stprintf(szAppBurnVer, _T("%x.%x.%x.%02x"), nBurnVer >> 20, (nBurnVer >> 16) & 0x0F, (nBurnVer >> 8) & 0xFF, nBurnVer & 0xFF);
	}
	else
	{
		 // public version
		 _stprintf(szAppBurnVer, _T("%x.%x.%x"), nBurnVer >> 20, (nBurnVer >> 16) & 0x0F, (nBurnVer >> 8) & 0xFF);
	}


	for (int i = 1; i < argc; i++) {
		if (*argv[i] != '-') {
			romname = argv[i];
		}
	}

	if (romname == NULL) {
		printf("Usage: %s [-f] [-F] [-dumpswitches] <romname>\n", argv[0]);
		printf("e.g.: %s mslug\n", argv[0]);

		return 0;
	}

	ConfigAppLoad();
	SDL_Init(SDL_INIT_TIMER|SDL_INIT_VIDEO);
	BurnLibInit();

	int driverId = -1;
	for (int i = 0; i < (int) nBurnDrvCount; i++) {
		nBurnDrvActive = i;
		if (strcmp(BurnDrvGetTextA(0), romname) == 0) {
			driverId = i;
			break;
		}
	}

	if (driverId < 0) {
		fprintf(stderr, "%s is not supported by FB Alpha\n", romname);
		return 1;
	}

	printf("Starting %s\n", romname);

	// Create the nvram directory, if it doesn't exist
	const char *nvramPath = "./nvram";
	if (access(nvramPath, F_OK) == -1) {
		fprintf(stderr, "Creating NVRAM directory at \"%s\"\n", nvramPath);
		mkdir(nvramPath, 0777);
	}

	bCheatsAllowed = false;

	if (DrvInit(driverId, 0) != 0) {
		fprintf(stderr, "Driver init failed\n");
		return 0;
	}

	parseSwitches(argc, argv);
	RunMessageLoop();

	DrvExit();
	MediaExit();

	ConfigAppSave();
	BurnLibExit();
	SDL_Quit();

	if (nExitEmulator == 2) {
		return 2; // timeout
	}

	return 0;
}


/* const */ TCHAR* ANSIToTCHAR(const char* pszInString, TCHAR* pszOutString, int nOutSize)
{
	if (pszOutString) {
		_tcscpy(pszOutString, pszInString);
		return pszOutString;
	}

	return (TCHAR*)pszInString;
}


/* const */ char* TCHARToANSI(const TCHAR* pszInString, char* pszOutString, int nOutSize)
{
	if (pszOutString) {
		strcpy(pszOutString, pszInString);
		return pszOutString;
	}

	return (char*)pszInString;
}


bool AppProcessKeyboardInput()
{
	return true;
}
