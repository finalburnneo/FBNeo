#include <QtWidgets>
#include "burner.h"
#include "mainwindow.h"

int bRunPause = 0;
bool bAlwaysProcessKeyboardInput;
bool bDoIpsPatch;

TCHAR szAppBurnVer[16];

int nAppVirtualFps = 6000;

TCHAR *GetIsoPath()
{
	return NULL;
}


void IpsApplyPatches(UINT8 *, char *)
{

}

void InpDIPSWResetDIPs()
{
}


void Reinitialise(void)
{
	
}

void wav_pause(bool bResume)
{

}

void wav_exit()
{

}

bool AppProcessKeyboardInput()
{
	return true;
}

char *TCHARToANSI(const TCHAR* pszInString, char* pszOutString, int nOutSize)
{
	if (pszOutString) {
		strcpy(pszOutString, pszInString);
		return pszOutString;
	}

	return (char*)pszInString;
}


int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    MainWindow mw;
    mw.show();

    return mw.main(app);
}
