// D3DKMTWaitForVerticalBlankEvent (codename: SuperWaitVBlank) interface.  - dink 2018
#include "burner.h"

typedef struct _D3DKMT_OPENADAPTERFROMHDC
{
    HDC     hDc;
    UINT32  hAdapter;
    LUID    AdapterLuid;
    UINT32  VidPnSourceId;
} D3DKMT_OPENADAPTERFROMHDC;

typedef struct _D3DKMT_CLOSEADAPTER {
    UINT32  hAdapter;
} D3DKMT_CLOSEADAPTER;

typedef struct _D3DKMT_WAITFORVERTICALBLANKEVENT
{
    UINT32  hAdapter;
    UINT32  hDevice;
    UINT32  VidPnSourceId;
} D3DKMT_WAITFORVERTICALBLANKEVENT;

#ifndef DISPLAY_DEVICE_ACTIVE
    #define DISPLAY_DEVICE_ACTIVE 0x00000001
#endif

LONG (APIENTRY *D3DKMTWaitForVerticalBlankEvent) (D3DKMT_WAITFORVERTICALBLANKEVENT *lpParams);
LONG (APIENTRY *D3DKMTOpenAdapterFromHdc)        (D3DKMT_OPENADAPTERFROMHDC *lpParams );
LONG (APIENTRY *D3DKMTCloseAdapter)              (D3DKMT_CLOSEADAPTER *lpParams );

LONG APIENTRY Empty_D3DKMTWaitForVerticalBlankEvent      (D3DKMT_WAITFORVERTICALBLANKEVENT *) { return 0; }
LONG APIENTRY Empty_D3DKMTOpenAdapterFromHdc             (D3DKMT_OPENADAPTERFROMHDC *) { return 0; }
LONG APIENTRY Empty_D3DKMTCloseAdapter                   (D3DKMT_CLOSEADAPTER * ) { return 0; }

static int SuperWaitVBlank_Initialised = 0;
static int SuperWaitVBlank_DLLsLoaded = 0;
static HINSTANCE hGdi32;
static D3DKMT_WAITFORVERTICALBLANKEVENT we = { 0, 0, 0 };

bool bVidDWMSync = true;

void LoadGDIFunctions() {

	hGdi32 = LoadLibrary(_T("gdi32.dll"));

	if(!hGdi32) return;

	D3DKMTWaitForVerticalBlankEvent = (LONG (WINAPI *)(D3DKMT_WAITFORVERTICALBLANKEVENT *)) GetProcAddress(hGdi32, "D3DKMTWaitForVerticalBlankEvent");
	D3DKMTOpenAdapterFromHdc        = (LONG (WINAPI *)(D3DKMT_OPENADAPTERFROMHDC *))        GetProcAddress(hGdi32, "D3DKMTOpenAdapterFromHdc");
	D3DKMTCloseAdapter              = (LONG (WINAPI *)(D3DKMT_CLOSEADAPTER *))              GetProcAddress(hGdi32, "D3DKMTCloseAdapter");

	if(!D3DKMTWaitForVerticalBlankEvent) bprintf(0, _T("Unable to acquire D3DKMTWaitForVerticalBlankEvent()!\n"));
	if(!D3DKMTWaitForVerticalBlankEvent) D3DKMTWaitForVerticalBlankEvent = Empty_D3DKMTWaitForVerticalBlankEvent;

	if(!D3DKMTOpenAdapterFromHdc)       bprintf(0, _T("Unable to acquire D3DKMTOpenAdapterFromHdc()!\n"));
	if(!D3DKMTOpenAdapterFromHdc)       D3DKMTOpenAdapterFromHdc         = Empty_D3DKMTOpenAdapterFromHdc;

	if(!D3DKMTCloseAdapter)             bprintf(0, _T("Unable to acquire D3DKMTCloseAdapter()!\n"));
	if(!D3DKMTCloseAdapter)             D3DKMTCloseAdapter               = Empty_D3DKMTCloseAdapter;

	FreeLibrary(hGdi32);

	SuperWaitVBlank_DLLsLoaded = 1;
}

BOOL IsWindows7Plus() {

	OSVERSIONINFO osvi;
	memset(&osvi, 0, sizeof(OSVERSIONINFO));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

	GetVersionEx(&osvi);

	return (osvi.dwMajorVersion >= 6); // win7+
}

// This will need re-init every vid init in the case of multiple monitors.
void SuperWaitVBlankInit()
{
	if(!IsWindows7Plus()) return;

	if (!SuperWaitVBlank_DLLsLoaded) {
		LoadGDIFunctions();
	}

	if (SuperWaitVBlank_Initialised) {
		SuperWaitVBlankExit();
	}

	MONITORINFOEX mi;
	memset(&mi, 0, sizeof(MONITORINFOEX));
	mi.cbSize = sizeof(MONITORINFOEX);
	HMONITOR hmon = MonitorFromWindow(hScrnWnd, MONITOR_DEFAULTTONEAREST); // get the monitor fba is running on
	GetMonitorInfo(hmon, &mi);

	HDC hDC = CreateDC(NULL, (LPWSTR)mi.szDevice, NULL, NULL);

	if (hDC) {
		D3DKMT_OPENADAPTERFROMHDC oa;
		oa.hDc = hDC;
		D3DKMTOpenAdapterFromHdc(&oa);
		DeleteDC(hDC);
		we.hAdapter = oa.hAdapter;
		we.hDevice = 0;
		we.VidPnSourceId = oa.VidPnSourceId;
		bprintf(0, _T("SuperWaitVBlankInit() on %s\n"), mi.szDevice);

		SuperWaitVBlank_Initialised = 1;
	}
}

void SuperWaitVBlankExit()
{
	if (SuperWaitVBlank_Initialised) {
		D3DKMT_CLOSEADAPTER ca = { we.hAdapter };
		D3DKMTCloseAdapter(&ca);
		memset(&we, 0, sizeof(we));

		SuperWaitVBlank_Initialised = 0;
	}
}

int SuperWaitVBlank()
{
	if (SuperWaitVBlank_Initialised) {
		return D3DKMTWaitForVerticalBlankEvent(&we);
	} else {
		return 0xdead;
	}
}
