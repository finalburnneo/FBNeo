#include <QtWidgets>
#include "burner.h"
#include "mainwindow.h"

int bRunPause = 0;
bool bAlwaysProcessKeyboardInput;
bool bDoIpsPatch;

TCHAR szAppBurnVer[16];
TCHAR szAppBlendPath[MAX_PATH];

int nAppVirtualFps = 6000;

TCHAR *GetIsoPath()
{
	return NULL;
}


void IpsApplyPatches(UINT8 *, char *, bool)
{

}

void IpsPatchInit()
{

}

void InpDIPSWResetDIPs()
{

}


void Reinitialise(void)
{

}

void ReinitialiseVideo(void)
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


QString loadAppStyle(const QString &name)
{
    QFile file(name);
    file.open(QFile::ReadOnly);

    return QString(file.readAll());
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    app.setStyleSheet(loadAppStyle(":/resource/dark-flat.css"));

    MainWindow mw;
    mw.show();

    return mw.main(app);
}
