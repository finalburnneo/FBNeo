// FBA DWM API HANDLING FOR WINDOWS 7 by CaptainCPS-X, Jezer Andino, dink
#include "burner.h"

typedef struct _D3DKMT_OPENADAPTERFROMHDC
{
    HDC                     hDc;            // in:  DC that maps to a single display
    UINT32                  hAdapter;       // out: adapter handle
    LUID                    AdapterLuid;    // out: adapter LUID
    UINT32                  VidPnSourceId;  // out: VidPN source ID for that particular display
} D3DKMT_OPENADAPTERFROMHDC;

typedef struct _D3DKMT_CLOSEADAPTER {
  UINT32                    hAdapter;
} D3DKMT_CLOSEADAPTER;

typedef struct _D3DKMT_WAITFORVERTICALBLANKEVENT
{
    UINT32                  hAdapter;      // in: adapter handle
    UINT32                  hDevice;       // in: device handle [Optional]
    UINT32                  VidPnSourceId; // in: adapter's VidPN Source ID
} D3DKMT_WAITFORVERTICALBLANKEVENT;

#ifndef DISPLAY_DEVICE_ACTIVE
    #define DISPLAY_DEVICE_ACTIVE 0x00000001
#endif

HRESULT (WINAPI *DwmEnableComposition)			(UINT uCompositionAction);
HRESULT (WINAPI *DwmSetDxFrameDuration)			(HWND hwnd, INT cRefreshes);
HRESULT (WINAPI *DwmSetPresentParameters)		(HWND hwnd, DWM_PRESENT_PARAMETERS *pPresentParams);
HRESULT (WINAPI *DwmExtendFrameIntoClientArea)	(HWND hwnd, const MARGINS* pMarInset);
HRESULT (WINAPI *DwmSetWindowAttribute)			(HWND hwnd, DWORD dwAttribute, LPCVOID pvAttribute, DWORD cbAttribute);
HRESULT (WINAPI *DwmIsCompositionEnabled)		(BOOL*);
HRESULT (WINAPI *DwmFlush)		                (void);
LONG (APIENTRY *D3DKMTWaitForVerticalBlankEvent) (D3DKMT_WAITFORVERTICALBLANKEVENT *lpParams);
LONG (APIENTRY *D3DKMTOpenAdapterFromHdc)        (D3DKMT_OPENADAPTERFROMHDC *lpParams );
LONG (APIENTRY *D3DKMTCloseAdapter)              (D3DKMT_CLOSEADAPTER *lpParams );

HRESULT WINAPI Empty_DwmEnableComposition				(UINT) { return 0; }
HRESULT WINAPI Empty_DwmSetDxFrameDuration				(HWND, INT) { return 0; }
HRESULT WINAPI Empty_DwmSetPresentParameters			(HWND, DWM_PRESENT_PARAMETERS*) { return 0; }
HRESULT WINAPI Empty_DwmExtendFrameIntoClientArea		(HWND, const MARGINS*) { return 0; }
HRESULT WINAPI Empty_DwmSetWindowAttribute				(HWND, DWORD, LPCVOID, DWORD ) { return 0; }
HRESULT WINAPI Empty_DwmIsCompositionEnabled			(BOOL*) { return 0; }
HRESULT WINAPI Empty_DwmFlush			                (void) { return 0; }
LONG APIENTRY Empty_D3DKMTWaitForVerticalBlankEvent      (D3DKMT_WAITFORVERTICALBLANKEVENT *) { return 0; }
LONG APIENTRY Empty_D3DKMTOpenAdapterFromHdc             (D3DKMT_OPENADAPTERFROMHDC *) { return 0; }
LONG APIENTRY Empty_D3DKMTCloseAdapter                   (D3DKMT_CLOSEADAPTER * ) { return 0; }

static HINSTANCE hDwmApi;

static int SuperWaitVBlank_Initialised = 0;
static int SuperWaitVBlank_DLLsLoaded = 0;
static HINSTANCE hGdi32;
static D3DKMT_WAITFORVERTICALBLANKEVENT we = { 0, 0, 0 };

static int DWMAPI_Initialised = 0;

bool bVidDWMCore = true;

void DwmFlushy() {
	if (DWMAPI_Initialised)
	{
		DwmFlush();
	}
}

void GetDllFunctionsDWM() {

	if(!DWMAPI_Initialised) return;

	DwmEnableComposition			= (HRESULT (WINAPI *)(UINT))							GetProcAddress(hDwmApi, "DwmEnableComposition");
	DwmSetDxFrameDuration			= (HRESULT (WINAPI *)(HWND, INT))						GetProcAddress(hDwmApi, "DwmSetDxFrameDuration");
	DwmSetPresentParameters			= (HRESULT (WINAPI *)(HWND, DWM_PRESENT_PARAMETERS*))	GetProcAddress(hDwmApi, "DwmSetPresentParameters");
	DwmExtendFrameIntoClientArea	= (HRESULT (WINAPI *)(HWND, const MARGINS*))			GetProcAddress(hDwmApi, "DwmExtendFrameIntoClientArea");
	DwmSetWindowAttribute			= (HRESULT (WINAPI *)(HWND, DWORD, LPCVOID, DWORD))		GetProcAddress(hDwmApi, "DwmSetWindowAttribute");
	DwmIsCompositionEnabled			= (HRESULT (WINAPI *)(BOOL*))							GetProcAddress(hDwmApi, "DwmIsCompositionEnabled");
	DwmFlush			            = (HRESULT (WINAPI *)(void))							GetProcAddress(hDwmApi, "DwmFlush");

	D3DKMTWaitForVerticalBlankEvent = (LONG (WINAPI *)(D3DKMT_WAITFORVERTICALBLANKEVENT *)) GetProcAddress(hGdi32, "D3DKMTWaitForVerticalBlankEvent");
	D3DKMTOpenAdapterFromHdc        = (LONG (WINAPI *)(D3DKMT_OPENADAPTERFROMHDC *))        GetProcAddress(hGdi32, "D3DKMTOpenAdapterFromHdc");
	D3DKMTCloseAdapter              = (LONG (WINAPI *)(D3DKMT_CLOSEADAPTER *))              GetProcAddress(hGdi32, "D3DKMTCloseAdapter");

	if(!DwmEnableComposition)			DwmEnableComposition			= Empty_DwmEnableComposition;
	if(!DwmSetDxFrameDuration)			DwmSetDxFrameDuration			= Empty_DwmSetDxFrameDuration;
	if(!DwmSetPresentParameters)		DwmSetPresentParameters			= Empty_DwmSetPresentParameters;
	if(!DwmExtendFrameIntoClientArea)	DwmExtendFrameIntoClientArea	= Empty_DwmExtendFrameIntoClientArea;
	if(!DwmSetWindowAttribute)			DwmSetWindowAttribute			= Empty_DwmSetWindowAttribute;
	if(!DwmIsCompositionEnabled)		DwmIsCompositionEnabled			= Empty_DwmIsCompositionEnabled;
	if(!DwmFlush)		                DwmFlush			            = Empty_DwmFlush;

	if(!D3DKMTWaitForVerticalBlankEvent) bprintf(0, _T("Unable to acquire D3DKMTWaitForVerticalBlankEvent()!\n"));
	if(!D3DKMTWaitForVerticalBlankEvent) D3DKMTWaitForVerticalBlankEvent = Empty_D3DKMTWaitForVerticalBlankEvent;

	if(!D3DKMTOpenAdapterFromHdc)       bprintf(0, _T("Unable to acquire D3DKMTOpenAdapterFromHdc()!\n"));
	if(!D3DKMTOpenAdapterFromHdc)       D3DKMTOpenAdapterFromHdc         = Empty_D3DKMTOpenAdapterFromHdc;

	if(!D3DKMTCloseAdapter)             bprintf(0, _T("Unable to acquire D3DKMTCloseAdapter()!\n"));
	if(!D3DKMTCloseAdapter)             D3DKMTCloseAdapter               = Empty_D3DKMTCloseAdapter;
}

void GetDllFunctionsGDI() {

	hGdi32 = LoadLibrary(_T("gdi32.dll"));

	if(!hGdi32) return;

	bprintf(0, _T("GDI32 loaded.\n"));
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

// This will need re-init every vid init in the case of multiple monitors.
void SuperWaitVBlankInit()
{
	if(!IsWindows7Plus()) return;

	if (!SuperWaitVBlank_DLLsLoaded) {
		GetDllFunctionsGDI();
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

int InitDWMAPI() {

	if(!IsWindows7Plus()) return 0;

	hDwmApi = LoadLibrary(_T("dwmapi.dll"));

	if (hDwmApi) {
		DWMAPI_Initialised = 1;

		// Try to init DWM API Functions
		bprintf(PRINT_IMPORTANT, _T("[Win7+] Loading of DWMAPI.DLL was succesfull.\n"));

		GetDllFunctionsDWM();

		FreeLibrary(hDwmApi);

		// Functions Adquired
		bprintf(PRINT_IMPORTANT, _T("[Win7+] DWMAPI.DLL Functions acquired.\n"));

		return 1;
	}

	bprintf(PRINT_IMPORTANT, _T("[Win7+] Loading of DWMAPI.DLL failed.\n"));
	DWMAPI_Initialised = 0;
	return 0;
}

void ExtendIntoClientAll(HWND hwnd) {
	// Negative margins have special meaning to DwmExtendFrameIntoClientArea.
	// Negative margins create the "sheet of glass" effect, where the client area
	// is rendered as a solid surface with no window border.
	MARGINS margins = {-1, -1, -1, -1};

	// Extend the frame across the entire window.
	DwmExtendFrameIntoClientArea(hwnd, &margins);
}

BOOL IsWindows7Plus() {

	OSVERSIONINFO osvi;
	memset(&osvi, 0, sizeof(OSVERSIONINFO));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

	// Verify if the FBA is running on Windows 7 or greater
	GetVersionEx(&osvi);

	return (osvi.dwMajorVersion >= 6);
}

// FIX FOR FRAME STUTTERING ON WINDOWS 7
void DWM_StutterFix()
{
	// Windows 7 found...
	if(IsWindows7Plus() && !nVidFullscreen)
	{
		// If the DWM API Functions are loaded, continue.
		if(DWMAPI_Initialised)
		{
			// Accurate
			UNSIGNED_RATIO fps;
			memset(&fps, 0, sizeof(UNSIGNED_RATIO));
			fps.uiNumerator		= 60000;
			fps.uiDenominator	= 1000;

			// Stuttering frames fix for Windows 7
			DWM_PRESENT_PARAMETERS params;
			memset(&params, 0, sizeof(DWM_PRESENT_PARAMETERS));
			params.cbSize				= sizeof(DWM_PRESENT_PARAMETERS);
			params.fQueue				= TRUE;
			params.cBuffer				= 2;
			params.cRefreshStart		= 0;
			params.fUseSourceRate		= TRUE;
			params.rateSource			= fps;
			//params.cRefreshesPerFrame	= 1;
			params.eSampling = DWM_SOURCE_FRAME_SAMPLING_POINT;

			DwmSetPresentParameters(hVidWnd, &params);

			bprintf(PRINT_IMPORTANT, _T("[Win7] DWM Presentation parameters were set.\n"));

			// Aero effects (disabled, useful in the future)
			//ExtendIntoClientAll(hScrnWnd);
		}
	}
}

BOOL IsCompositeOn()
{
	BOOL bStatus = false;

	// check DWM composition status
	DwmIsCompositionEnabled(&bStatus);

	return bStatus;
}
