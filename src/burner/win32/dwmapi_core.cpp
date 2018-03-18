// FBA DWM API HANDLING FOR WINDOWS 7 by CaptainCPS-X, Jezer Andino, dink
#include "burner.h"

HRESULT (WINAPI *DwmEnableComposition)			(UINT uCompositionAction);
HRESULT (WINAPI *DwmSetDxFrameDuration)			(HWND hwnd, INT cRefreshes);
HRESULT (WINAPI *DwmSetPresentParameters)		(HWND hwnd, DWM_PRESENT_PARAMETERS *pPresentParams);
HRESULT (WINAPI *DwmExtendFrameIntoClientArea)	(HWND hwnd, const MARGINS* pMarInset);
HRESULT (WINAPI *DwmSetWindowAttribute)			(HWND hwnd, DWORD dwAttribute, LPCVOID pvAttribute, DWORD cbAttribute);
HRESULT (WINAPI *DwmIsCompositionEnabled)		(BOOL*);
HRESULT (WINAPI *DwmFlush)		                (void);

HRESULT WINAPI Empty_DwmEnableComposition				(UINT) { return 0; }
HRESULT WINAPI Empty_DwmSetDxFrameDuration				(HWND, INT) { return 0; }
HRESULT WINAPI Empty_DwmSetPresentParameters			(HWND, DWM_PRESENT_PARAMETERS*) { return 0; }
HRESULT WINAPI Empty_DwmExtendFrameIntoClientArea		(HWND, const MARGINS*) { return 0; }
HRESULT WINAPI Empty_DwmSetWindowAttribute				(HWND, DWORD, LPCVOID, DWORD ) { return 0; }
HRESULT WINAPI Empty_DwmIsCompositionEnabled			(BOOL*) { return 0; }
HRESULT WINAPI Empty_DwmFlush			                (void) { return 0; }

static HINSTANCE hDwmApi;
static int DWMAPI_Initialised = 0;

bool bVidDWMCore = true;

void DwmFlushy() {
	if (DWMAPI_Initialised) // && IsCompositeOn())
	{
		DwmFlush();
	}
}

void GetDllFunctions() {

	if(!DWMAPI_Initialised) return;

	DwmEnableComposition			= (HRESULT (WINAPI *)(UINT))							GetProcAddress(hDwmApi, "DwmEnableComposition");
	DwmSetDxFrameDuration			= (HRESULT (WINAPI *)(HWND, INT))						GetProcAddress(hDwmApi, "DwmSetDxFrameDuration");
	DwmSetPresentParameters			= (HRESULT (WINAPI *)(HWND, DWM_PRESENT_PARAMETERS*))	GetProcAddress(hDwmApi, "DwmSetPresentParameters");
	DwmExtendFrameIntoClientArea	= (HRESULT (WINAPI *)(HWND, const MARGINS*))			GetProcAddress(hDwmApi, "DwmExtendFrameIntoClientArea");
	DwmSetWindowAttribute			= (HRESULT (WINAPI *)(HWND, DWORD, LPCVOID, DWORD))		GetProcAddress(hDwmApi, "DwmSetWindowAttribute");
	DwmIsCompositionEnabled			= (HRESULT (WINAPI *)(BOOL*))							GetProcAddress(hDwmApi, "DwmIsCompositionEnabled");
	DwmFlush			            = (HRESULT (WINAPI *)(void))							GetProcAddress(hDwmApi, "DwmFlush");

	if(!DwmEnableComposition)			DwmEnableComposition			= Empty_DwmEnableComposition;
	if(!DwmSetDxFrameDuration)			DwmSetDxFrameDuration			= Empty_DwmSetDxFrameDuration;
	if(!DwmSetPresentParameters)		DwmSetPresentParameters			= Empty_DwmSetPresentParameters;
	if(!DwmExtendFrameIntoClientArea)	DwmExtendFrameIntoClientArea	= Empty_DwmExtendFrameIntoClientArea;
	if(!DwmSetWindowAttribute)			DwmSetWindowAttribute			= Empty_DwmSetWindowAttribute;
	if(!DwmIsCompositionEnabled)		DwmIsCompositionEnabled			= Empty_DwmIsCompositionEnabled;
	if(!DwmFlush)		                DwmFlush			            = Empty_DwmFlush;
}

int InitDWMAPI() {

	if(!IsWindows7()) return 0;

	hDwmApi = LoadLibrary(_T("dwmapi.dll"));

	if (hDwmApi) {
		DWMAPI_Initialised = 1;

		// Try to init DWM API Functions
		bprintf(PRINT_IMPORTANT, _T("[Win7+] Loading of DWMAPI.DLL was succesfull.\n"));

		GetDllFunctions();

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

BOOL IsWindows7() { // is win7+

	OSVERSIONINFO osvi;
	memset(&osvi, 0, sizeof(OSVERSIONINFO));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

	// Verify if the FBA is running on Windows 7
	GetVersionEx(&osvi);

	return (osvi.dwMajorVersion >= 6 && osvi.dwMinorVersion >= 0);
}

// FIX FOR FRAME STUTTERING ON WINDOWS 7
void DWM_StutterFix()
{
	// Windows 7 found...
	if(IsWindows7() && !nVidFullscreen)
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
