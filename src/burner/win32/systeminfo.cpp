#include "burner.h"
#include "build_details.h"
#include <tlhelp32.h>
#include <setupapi.h>
#include <psapi.h>

#if defined _MSC_VER
 #include <intrin.h>
#endif

// these are not defined in the Cygwin/MinGW winapi package
#ifndef DISPLAY_DEVICE_ACTIVE
 #define DISPLAY_DEVICE_ACTIVE              0x00000001
#endif
#ifndef DISPLAY_DEVICE_ATTACHED
 #define DISPLAY_DEVICE_ATTACHED            0x00000002
#endif

static _EXCEPTION_POINTERS* pExceptionPointers;

static TCHAR* pszTextBuffer = NULL;
static int nTextBufferSize = 0;

static int nRecursion = 0;

static HFONT hLogFont, hCodeFont;
static HBRUSH hCodeBGBrush;

static const bool bPrintDriverInfo = false;

static int AddLine(TCHAR* pszFormat, ...)
{
	TCHAR szString[255] = _T("");

	va_list vaFormat;
	va_start(vaFormat, pszFormat);

	int nLen = _vsntprintf(szString, 170, pszFormat, vaFormat);
	nLen = (nLen >= 0 && nLen < 170) ? nLen : 170;
	nLen += _stprintf(szString + nLen, _T("\r\n"));
	TCHAR* pszNewBuffer = (TCHAR*)realloc(pszTextBuffer, (nLen + nTextBufferSize + 1) * sizeof(TCHAR));
	if (pszNewBuffer) {
		pszTextBuffer = pszNewBuffer;
		_tcsncpy(pszTextBuffer + nTextBufferSize, szString, nLen);
		nTextBufferSize += nLen;
		pszTextBuffer[nTextBufferSize] = 0;
	}

	va_end(vaFormat);

	return 0;
}

static int AddText(TCHAR* pszFormat, ...)
{
	TCHAR szString[255] = _T("");

	va_list vaFormat;
	va_start(vaFormat, pszFormat);

	int nLen = _vsntprintf(szString, 70, pszFormat, vaFormat);
	nLen = (nLen >= 0 && nLen < 70) ? nLen : 70;
	TCHAR* pszNewBuffer = (TCHAR*)realloc(pszTextBuffer, (nLen + nTextBufferSize + 1) * sizeof(TCHAR));
	if (pszNewBuffer) {
		pszTextBuffer = pszNewBuffer;
		_tcsncpy(pszTextBuffer + nTextBufferSize, szString, nLen);
		nTextBufferSize += nLen;
		pszTextBuffer[nTextBufferSize] = 0;
	}

	va_end(vaFormat);

	return 0;
}

static int PrintInterfaceInfo(InterfaceInfo* pInfo)
{
	if (pInfo == NULL) {
		return 1;
	}

	if (pInfo->pszModuleName) {
		AddLine(_T("    Selected module:    %s"), pInfo->pszModuleName);
	}
	for (int i = 0; pInfo->ppszInterfaceSettings[i]; i++) {
		AddLine(_T("    %s%s"), (i == 0) ? _T("Interface settings: ") : _T("                    "), pInfo->ppszInterfaceSettings[i]);
	}
	for (int i = 0; pInfo->ppszModuleSettings[i]; i++) {
		AddLine(_T("    %s%s"), (i == 0) ? _T("Module settings:    ") : _T("                    "), pInfo->ppszModuleSettings[i]);
	}

	return 0;
}

// Print information about the exception
int PrintExceptionInfo()
{
	static const struct { DWORD ExceptionCode; const TCHAR* szString; } ExceptionString[] = {

#define EXCEPTION_LIST_ENTRY(exception) { exception, _T(#exception) }

		EXCEPTION_LIST_ENTRY(EXCEPTION_ACCESS_VIOLATION),
		EXCEPTION_LIST_ENTRY(EXCEPTION_ARRAY_BOUNDS_EXCEEDED),
		EXCEPTION_LIST_ENTRY(EXCEPTION_BREAKPOINT),
		EXCEPTION_LIST_ENTRY(EXCEPTION_DATATYPE_MISALIGNMENT),
		EXCEPTION_LIST_ENTRY(EXCEPTION_FLT_DENORMAL_OPERAND),
		EXCEPTION_LIST_ENTRY(EXCEPTION_FLT_DIVIDE_BY_ZERO),
		EXCEPTION_LIST_ENTRY(EXCEPTION_FLT_INEXACT_RESULT),
		EXCEPTION_LIST_ENTRY(EXCEPTION_FLT_INVALID_OPERATION),
		EXCEPTION_LIST_ENTRY(EXCEPTION_FLT_OVERFLOW),
		EXCEPTION_LIST_ENTRY(EXCEPTION_FLT_STACK_CHECK),
		EXCEPTION_LIST_ENTRY(EXCEPTION_FLT_UNDERFLOW),
		EXCEPTION_LIST_ENTRY(EXCEPTION_ILLEGAL_INSTRUCTION),
		EXCEPTION_LIST_ENTRY(EXCEPTION_IN_PAGE_ERROR),
		EXCEPTION_LIST_ENTRY(EXCEPTION_INVALID_DISPOSITION),
		EXCEPTION_LIST_ENTRY(EXCEPTION_INT_DIVIDE_BY_ZERO),
		EXCEPTION_LIST_ENTRY(EXCEPTION_INT_OVERFLOW),
		EXCEPTION_LIST_ENTRY(EXCEPTION_INVALID_HANDLE),
		EXCEPTION_LIST_ENTRY(EXCEPTION_GUARD_PAGE),
		EXCEPTION_LIST_ENTRY(EXCEPTION_NONCONTINUABLE_EXCEPTION),
		EXCEPTION_LIST_ENTRY(EXCEPTION_PRIV_INSTRUCTION),
		EXCEPTION_LIST_ENTRY(EXCEPTION_SINGLE_STEP),
		EXCEPTION_LIST_ENTRY(EXCEPTION_STACK_OVERFLOW),
		{ 0, _T("unspecified exception") }

#undef EXCEPTION_LIST_ENTRY

	};

	int i;

	for (i = 0; ExceptionString[i].ExceptionCode; i++) {
		if (ExceptionString[i].ExceptionCode == pExceptionPointers->ExceptionRecord->ExceptionCode) {
			break;
		}
	}

	AddLine(_T("Exception 0x%08X (%s) thrown.\r\nEIP: 0x%p"), pExceptionPointers->ExceptionRecord->ExceptionCode, ExceptionString[i].szString, pExceptionPointers->ExceptionRecord->ExceptionAddress);
	if (pExceptionPointers->ExceptionRecord->ExceptionCode ==  EXCEPTION_ACCESS_VIOLATION) {
		AddLine(_T(" (attempting to %s address 0x%p)"), pExceptionPointers->ExceptionRecord->ExceptionInformation[0] ? _T("write") : _T("read"), pExceptionPointers->ExceptionRecord->ExceptionInformation[1]);
	}
	AddLine(_T(""));

#ifdef BUILD_X86_ASM
	AddLine(_T("EAX: 0x%08X, EBX: 0x%08X, ECX: 0x%08X, EDX: 0x%08X"), (unsigned int)pExceptionPointers->ContextRecord->Eax, (unsigned int)pExceptionPointers->ContextRecord->Ebx, (unsigned int)pExceptionPointers->ContextRecord->Ecx, (unsigned int)pExceptionPointers->ContextRecord->Edx);
	AddLine(_T("ESI: 0x%08X, EDI: 0x%08X, ESP: 0x%08X, EBP: 0x%08X"), (unsigned int)pExceptionPointers->ContextRecord->Esi, (unsigned int)pExceptionPointers->ContextRecord->Edi, (unsigned int)pExceptionPointers->ContextRecord->Esp, (unsigned int)pExceptionPointers->ContextRecord->Ebp);
#endif

	return 0;
}

// Print OS information
int PrintOSInfo()
{
	OSVERSIONINFOEX osvi;

	memset(&osvi, 0, sizeof(OSVERSIONINFOEX));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	if (!GetVersionEx((OSVERSIONINFO*)&osvi)) {
		osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
		GetVersionEx((OSVERSIONINFO*)&osvi);
	}

//	bprintf(PRINT_NORMAL, _T("%i, %i\n"), osvi.dwMajorVersion, osvi.dwMinorVersion);

	AddText(_T("OS:  Microsoft "));
	{
		// https://docs.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getproductinfo
		typedef BOOL(WINAPI* GETPRODUCTINFO)(DWORD, DWORD, DWORD, DWORD, PDWORD);

		GETPRODUCTINFO pGPI = NULL;
		DWORD dwType = 0;

		pGPI = (GETPRODUCTINFO)GetProcAddress(GetModuleHandle(_T("kernel32.dll")), "GetProductInfo");

		if (pGPI) {
			pGPI(osvi.dwMajorVersion, osvi.dwMinorVersion, 0, 0, &dwType);
		}

		if (osvi.dwPlatformId == VER_PLATFORM_WIN32_NT) {
			// http://msdn.microsoft.com/en-us/library/ms724833(VS.85).aspx
			if (osvi.dwMajorVersion == 6 && osvi.dwMinorVersion == 0 && osvi.wProductType == VER_NT_WORKSTATION) {
				AddText(_T("Windows Vista "));
			}
			if (osvi.dwMajorVersion == 6 && osvi.dwMinorVersion == 0 && osvi.wProductType != VER_NT_WORKSTATION) {
				AddText(_T("Windows Server 2008 "));
			}
			if (osvi.dwMajorVersion == 6 && osvi.dwMinorVersion == 1 && osvi.wProductType != VER_NT_WORKSTATION) {
				if (dwType == 0x22) {	// PRODUCT_HOME_PREMIUM_SERVER
					AddText(_T("Windows Home Server 2011"));
				}
				if (dwType == 0x32) {	// PRODUCT_SB_SOLUTION_SERVER
					AddText(_T("Windows Small Business Server 2011 Essentials"));
				}
				if (dwType == 0x13) {	// PRODUCT_HOME_SERVER
					AddText(_T("Windows Storage Server 2008 R2 Essentials"));
				}

				switch (dwType) {
				case 0x22:
				case 0x32:
				case 0x13:
					AddText(_T("%s (build %i)\r\n"), osvi.szCSDVersion, osvi.dwBuildNumber & 0xFFFF);
					return 0;
				default:
					AddText(_T("Windows Server 2008 R2 "));
				}
			}
			if (osvi.dwMajorVersion == 6 && osvi.dwMinorVersion == 1 && osvi.wProductType == VER_NT_WORKSTATION) {
				AddText(_T("Windows 7 "));
			}
			if (osvi.dwMajorVersion == 6 && osvi.dwMinorVersion == 2 && osvi.wProductType != VER_NT_WORKSTATION) {
				AddText(_T("Windows Server 2012 "));
			}
			if (osvi.dwMajorVersion == 6 && osvi.dwMinorVersion == 2 && osvi.wProductType == VER_NT_WORKSTATION) {
				AddText(_T("MWindows 8 "));
			}
			if (osvi.dwMajorVersion == 6 && osvi.dwMinorVersion == 3 && osvi.wProductType != VER_NT_WORKSTATION) {
				AddText(_T("Windows Server 2012 R2 "));
			}
			if (osvi.dwMajorVersion == 6 && osvi.dwMinorVersion == 3 && osvi.wProductType == VER_NT_WORKSTATION) {
				AddText(_T("Windows 8.1 "));
			}
			if (osvi.dwMajorVersion == 10 && osvi.dwMinorVersion == 0 && osvi.wProductType != VER_NT_WORKSTATION && osvi.dwBuildNumber <= 14393) {
				AddText(_T("Windows Server 2016 "));
			}
			if (osvi.dwMajorVersion == 10 && osvi.dwMinorVersion == 0 && osvi.wProductType == VER_NT_WORKSTATION && osvi.dwBuildNumber < 22000) {
				AddText(_T("Windows 10 "));
			}
			if (osvi.dwMajorVersion == 10 && osvi.dwMinorVersion == 0 && osvi.wProductType != VER_NT_WORKSTATION && osvi.dwBuildNumber > 14393 && osvi.dwBuildNumber <= 17763) {
				AddText(_T("Windows Server 2019 "));
			}
			if (osvi.dwMajorVersion == 10 && osvi.dwMinorVersion == 0 && osvi.wProductType == VER_NT_WORKSTATION && osvi.dwBuildNumber >= 22000) {
				AddText(_T("Windows 11 "));
			}

			if (osvi.dwMajorVersion > 5) {
				switch (dwType) {
				case 0x06: // PRODUCT_BUSINESS:
				case 0x10: // PRODUCT_BUSINESS_N:
					AddText(_T("Business"));
					break;
				case 0x12: // PRODUCT_CLUSTER_SERVER:
					AddText(_T("HPC Edition"));
					break;
				case 0x62: // PRODUCT_CORE_N:
				case 0x63: // PRODUCT_CORE_COUNTRYSPECIFIC:
				case 0x64: // PRODUCT_CORE_SINGLELANGUAGE:
				case 0x65: // PRODUCT_CORE:
					AddText(_T("Home"));
					break;
				case 0x08: // PRODUCT_DATACENTER_SERVER:
				case 0x25: // PRODUCT_DATACENTER_SERVER_V:
				case 0x50: // PRODUCT_DATACENTER_EVALUATION_SERVER:
				case 0x91: // PRODUCT_DATACENTER_A_SERVER_CORE:
				case 0x0C: // PRODUCT_DATACENTER_SERVER_CORE:
				case 0x27: // PRODUCT_DATACENTER_SERVER_CORE_V
					AddText(_T("Datacenter Edition"));
					break;
				case 0x79: // PRODUCT_EDUCATION:
				case 0x7A: // PRODUCT_EDUCATION_N:
					AddText(_T("Education"));
					break;
				case 0xBC: // PRODUCT_IOTENTERPRISE:
				case 0xBF: // PRODUCT_IOTENTERPRISE_S:
					AddText(_T("IoT Enterprise"));
					break;
				case 0x7B: // PRODUCT_IOTUAP:
				case 0x83: // PRODUCT_IOTENTERPRISE_S:
					AddText(_T("IoT Core"));
					break;
				case 0x3E: // PRODUCT_ESSENTIALBUSINESS_SERVER_ADDLSVC:
				case 0x3B: // PRODUCT_ESSENTIALBUSINESS_SERVER_MGMT:
				case 0x3D: // PRODUCT_ESSENTIALBUSINESS_SERVER_MGMTSVC:
					AddText(_T("Essential Server Solution"));
					break;
				case 0x34: // PRODUCT_STANDARD_SERVER_SOLUTIONS:
				case 0x35: // PRODUCT_STANDARD_SERVER_SOLUTIONS_CORE:
					AddText(_T("Server Solutions Premium"));
					break;
				case 0x1E: // PRODUCT_MEDIUMBUSINESS_SERVER_MANAGEMENT:
				case 0x20: // PRODUCT_MEDIUMBUSINESS_SERVER_MESSAGING:
				case 0x1F: // PRODUCT_MEDIUMBUSINESS_SERVER_SECURITY:
					AddText(_T("Essential Business Server"));
					break;
				case 0x38: // PRODUCT_SOLUTION_EMBEDDEDSERVER:
				case 0x4D: // PRODUCT_MULTIPOINT_PREMIUM_SERVER:
				case 0x4C: // PRODUCT_MULTIPOINT_STANDARD_SERVER:
					AddText(_T("MultiPoint Server"));
					break;
				case 0x09: // PRODUCT_SMALLBUSINESS_SERVER:
				case 0x19: // PRODUCT_SMALLBUSINESS_SERVER_PREMIUM:
				case 0x3F: // PRODUCT_SMALLBUSINESS_SERVER_PREMIUM_CORE:
					AddText(_T("Small Business Server"));
					break;
				case 0x4F: // PRODUCT_STANDARD_EVALUATION_SERVER:
				case 0x07: // PRODUCT_STANDARD_SERVER:
				case 0x28: // PRODUCT_STANDARD_SERVER_CORE_V:
				case 0x0D: // PRODUCT_STANDARD_SERVER_CORE:
				case 0x24: // PRODUCT_STANDARD_SERVER_V:
					AddText(_T("Standard"));
					break;
				case 0x11: // PRODUCT_WEB_SERVER:
				case 0x1D: // PRODUCT_WEB_SERVER_CORE:
					AddText(_T("Web Server"));
					break;
				case 0x04: // PRODUCT_ENTERPRISE:
				case 0x46: // PRODUCT_ENTERPRISE_E:
				case 0x7D: // PRODUCT_ENTERPRISE_S:
				case 0x1B: // PRODUCT_ENTERPRISE_N:
				case 0x48: // PRODUCT_ENTERPRISE_EVALUATION:
				case 0x54: // PRODUCT_ENTERPRISE_N_EVALUATION:
				case 0x81: // PRODUCT_ENTERPRISE_S_EVALUATION:
				case 0x7E: // PRODUCT_ENTERPRISE_S_N:
				case 0x82: // PRODUCT_ENTERPRISE_S_N_EVALUATION:
				case 0x0A: // PRODUCT_ENTERPRISE_SERVER:
				case 0x0F: // PRODUCT_ENTERPRISE_SERVER_IA64:
				case 0x0E: // PRODUCT_ENTERPRISE_SERVER_CORE:
				case 0x29: // PRODUCT_ENTERPRISE_SERVER_CORE_V:
				case 0x26: // PRODUCT_ENTERPRISE_SERVER_V:
					AddText(_T("Enterprise"));
					break;
				case 0x02: // PRODUCT_HOME_BASIC:
				case 0x05: // PRODUCT_HOME_BASIC_N:
					AddText(_T("Home Basic"));
					break;
				case 0x03: // PRODUCT_HOME_PREMIUM:
				case 0x1A: // PRODUCT_HOME_PREMIUM_N:
					AddText(_T("Home Premium"));
					break;
				case 0x01: // PRODUCT_ULTIMATE:
				case 0x1C: // PRODUCT_ULTIMATE_N:
					AddText(_T("Ultimate"));
					break;
				case 0x30: // PRODUCT_PROFESSIONAL:
				case 0x31: // PRODUCT_PROFESSIONAL_N:
				case 0xA1: // PRODUCT_PRO_WORKSTATION:
				case 0xA2: // PRODUCT_PRO_WORKSTATION_N:
					AddText(_T("Professional"));
					break;
				case 0x0B: // PRODUCT_STARTER:
				case 0x2F: // PRODUCT_STARTER_N:
					AddText(_T("Starter"));
					break;
				case 0x17: // PRODUCT_STORAGE_ENTERPRISE_SERVER:
				case 0x2E: // PRODUCT_STORAGE_ENTERPRISE_SERVER_CORE:
				case 0x14: // PRODUCT_STORAGE_EXPRESS_SERVER:
				case 0x2B: // PRODUCT_STORAGE_EXPRESS_SERVER_CORE:
				case 0x60: // PRODUCT_STORAGE_STANDARD_EVALUATION_SERVER:
				case 0x15: // PRODUCT_STORAGE_STANDARD_SERVER:
				case 0x2C: // PRODUCT_STORAGE_STANDARD_SERVER_CORE:
				case 0x5F: // PRODUCT_STORAGE_WORKGROUP_EVALUATION_SERVER:
				case 0x16: // PRODUCT_STORAGE_WORKGROUP_SERVER:
				case 0x2D: // PRODUCT_STORAGE_WORKGROUP_SERVER_CORE:
					AddText(_T("Storage Server"));
					break;
				}
			}
			if (osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 0) {
				if (osvi.wProductType != VER_NT_WORKSTATION) {
					if (osvi.wProductType & VER_SUITE_DATACENTER) {
						AddText(_T("Windows 2000 Datacenter Server"));
					} else if (osvi.wProductType & VER_SUITE_ENTERPRISE) {
						AddText(_T("Windows 2000 Advanced Server"));
					} else {
						AddText(_T("Windows 2000 Server"));
					}
				} else {
					AddText(_T("Windows 2000 Professional"));
				}
			}
			if (osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 1) {
				if (GetSystemMetrics(SM_TABLETPC) && GetSystemMetrics(SM_MEDIACENTER)) {
					AddText(_T("Windows XP Tablet PC Edition with Media Center"));	// Pro (Ultimate hack)
				} else if (GetSystemMetrics(SM_TABLETPC)) {
					AddText(_T("Windows XP Tablet PC Edition"));					// Pro
				} else if (GetSystemMetrics(SM_MEDIACENTER)) {
					AddText(_T("Windows XP Media Center Edition"));					// Pro
				} else if (GetSystemMetrics(SM_STARTER)) {
					AddText(_T("Windows XP Starter Edition"));
				} else if (osvi.wSuiteMask & VER_SUITE_PERSONAL) {
					AddText(_T("Windows XP Home Edition"));
				} else if (osvi.wSuiteMask & VER_SUITE_EMBEDDEDNT) {
					AddText(_T("Windows XP Embedded"));
				} else { 
					AddText(_T("Windows XP")); 
				}
			}
			if (osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 2) {
				if (osvi.wProductType != VER_NT_WORKSTATION) {
					if (GetSystemMetrics(SM_SERVERR2)) {
						if (osvi.wSuiteMask & VER_SUITE_DATACENTER) {
							AddText(_T("Windows Server 2003 R2, Datacenter Edition"));
						} else if (osvi.wSuiteMask & VER_SUITE_ENTERPRISE) {
							AddText(_T("Windows Server 2003 R2, Enterprise Edition"));
						} else if (osvi.wSuiteMask & VER_SUITE_ENTERPRISE) {
							AddText(_T("Windows Server 2003 R2, Enterprise Edition"));
						} else if (osvi.wSuiteMask & VER_SUITE_COMPUTE_SERVER) {
							AddText(_T("Windows Storage Server 2003 R2"));
						} else if (dwType == 0x07) {								// PRODUCT_STANDARD_SERVER
							AddText(_T("Windows Server 2003 R2, Standard Edition"));
						} else {
							AddText(_T("Windows 2003 R2"));
						}
					} else {
						if (osvi.wSuiteMask & VER_SUITE_STORAGE_SERVER) {
							AddText(_T("Windows Storage Server 2003"));
						} else if (osvi.wSuiteMask & VER_SUITE_DATACENTER) {
							AddText(_T("Windows Server 2003, Datacenter Edition"));
						} else if (osvi.wSuiteMask & VER_SUITE_ENTERPRISE) {
							AddText(_T("Windows Server 2003, Enterprise Edition"));
						} else if (osvi.wSuiteMask & VER_SUITE_BLADE) {
							AddText(_T("Windows Server 2003, Web Edition"));
						} else if (osvi.wSuiteMask & VER_SUITE_COMPUTE_SERVER) {
							AddText(_T("Windows Compute Cluster Server 2003"));
						} else {
							AddText(_T("Windows 2003"));
						}
					}
				} else {
					AddText(_T("Windows XP Professional x64 Edition"));
				}
			}
			if (osvi.dwMajorVersion < 5 || osvi.dwMinorVersion > 3) {
				AddText(_T("Windows NT %d.%d "), osvi.dwMajorVersion, osvi.dwMinorVersion);
			}
			AddText(_T("%s (build %i)\r\n"), osvi.szCSDVersion, osvi.dwBuildNumber & 0xFFFF);
		}

		if (osvi.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
			if (osvi.dwMajorVersion == 4 && osvi.dwMinorVersion < 10) {
				AddText(_T("Windows 95"));
				if (osvi.szCSDVersion[1] == _T('B') || osvi.szCSDVersion[1] == _T('C')) {
					AddText(_T(" OSR2"));
				}
			}
			if (osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 10) {
				AddText(_T("Windows 98"));
				if (osvi.szCSDVersion[1] == _T('A')) {
					AddText(_T(" SE"));
				}
			}
			if (osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 90) {
				AddText(_T("Windows Me"));
			}
			if (osvi.dwMajorVersion != 4 && osvi.dwMinorVersion < 10) {
				AddText(_T("Windows %d.%d "), osvi.dwMajorVersion, osvi.dwMinorVersion);
			}
			AddText(_T("\r\n"));
		}
	}

	return 0;
}

int PrintCPUInfo()
{
	AddText(_T("CPU: "));

	char CPUBrandStringFinal[0x40] = { 0 };

#if defined _MSC_VER
	int CPUInfo[4] = {-1};
	__cpuid(CPUInfo, 0x80000000);
	unsigned int nExIds = CPUInfo[0];
	unsigned int i, j = 0;

	// Get the information associated with each extended ID.
	char CPUBrandString[0x40] = { 0 };

    for (i = 0x80000000; i <= nExIds; ++i) {
		__cpuid(CPUInfo, i);

		// Interpret CPU brand string and cache information.
		if (i == 0x80000002) {
			memcpy( CPUBrandString,
			CPUInfo,
			sizeof(CPUInfo));
		} else if (i == 0x80000003) {
			memcpy( CPUBrandString + 16,
			CPUInfo,
			sizeof(CPUInfo));
		} else if (i == 0x80000004) {
			memcpy(CPUBrandString + 32, CPUInfo, sizeof(CPUInfo));
		}
	}

	// trim the leading white space
	int nCheckForSpace = 1;
	for (i = 0; CPUBrandString[i] != '\0'; i++) {
		if (!isspace(CPUBrandString[i])) nCheckForSpace = 0;

		if (nCheckForSpace) {
			if (!isspace(CPUBrandString[i])) {
				CPUBrandStringFinal[j++] = CPUBrandString[i];
			}
		} else {
			CPUBrandStringFinal[j++] = CPUBrandString[i];
		}
	}
#else
	HKEY hKey = NULL;

	if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0"), 0, KEY_QUERY_VALUE, &hKey)) {
		TCHAR szCpuName[0x40] = _T("");
		DWORD nNameSize = sizeof(szCpuName);
		DWORD nType = REG_SZ;

		if (ERROR_SUCCESS == RegQueryValueEx(hKey, _T("ProcessorNameString"), NULL, &nType, (BYTE*)szCpuName, &nNameSize)) {
			// trim the leading white space
			int nCheckForSpace = 1;
			for (int i = 0, j = 0; szCpuName[i] != '\0'; i++) {
				if (!isspace(szCpuName[i])) {
					nCheckForSpace = 0;
				}
				if (nCheckForSpace) {
					if (!isspace(szCpuName[i])) {
						CPUBrandStringFinal[j++] = szCpuName[i];
					}
				} else { CPUBrandStringFinal[j++] = szCpuName[i]; }
			}
		} else { sprintf(CPUBrandStringFinal, "%s", "CPU Detection not enabled for GCC builds"); }

		RegCloseKey(hKey);
	}
#endif
	AddLine(_T("%hs"), CPUBrandStringFinal);

	return 0;
}

// Print global memory information
int PrintGlobalMemoryInfo()
{
	MEMORYSTATUSEX stat;

	stat.dwLength = sizeof(MEMORYSTATUSEX);

	GlobalMemoryStatusEx(&stat);

	float fTotalPhys = stat.ullTotalPhys / pow(1024, 2);
	float fAvailPhys = stat.ullAvailPhys / pow(1024, 2);
	float fTotalPageFile = stat.ullTotalPageFile / pow(1024, 2);
	float fAvailPageFile = stat.ullAvailPageFile / pow(1024, 2);

	AddLine(_T("Physical RAM: %.2f MB (%.2f GB) total, %.2f MB (%.2f GB) avail"), fTotalPhys, fTotalPhys / 1024, fAvailPhys, fAvailPhys / 1024);
	AddLine(_T("PageFile RAM: %.2f MB (%.2f GB) total, %.2f MB (%.2f GB) avail"), fTotalPageFile, fTotalPageFile / 1024, fAvailPageFile, fAvailPageFile / 1024);

	// Information on FB Neo memory usage
	BOOL (WINAPI* pGetProcessMemoryInfo)(HANDLE, PPROCESS_MEMORY_COUNTERS, DWORD) = NULL;
	HINSTANCE hPsapiDLL;

	hPsapiDLL = LoadLibrary(_T("psapi.dll"));
	if (hPsapiDLL) {
		pGetProcessMemoryInfo = (BOOL (WINAPI*)(HANDLE, PPROCESS_MEMORY_COUNTERS, DWORD))GetProcAddress(hPsapiDLL, "GetProcessMemoryInfo");
		if (pGetProcessMemoryInfo) {
			PROCESS_MEMORY_COUNTERS pmc;

			if (pGetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
				TCHAR szLine[1024] = _T("");
				int length = _tcslen(_T(APP_TITLE));
				if (length > 12) {
					length = 12;
				}
				_sntprintf(szLine, 12, _T(APP_TITLE));
				_sntprintf(szLine + length, 14 - length, _T(":                 "));

				float fWorkingSetSize = pmc.WorkingSetSize / pow(1024, 2);
				float fPeakWorkingSetSize = pmc.PeakWorkingSetSize / pow(1024, 2);
				float fPagefileUsage = pmc.PagefileUsage / pow(1024, 2);

				AddLine(_T("%s%.2f MB in use (%.2f MB peak, %.2f MB virtual)"), szLine, fWorkingSetSize, fPeakWorkingSetSize, fPagefileUsage);
			}
		}
		FreeLibrary(hPsapiDLL);
	}

	return 0;
}

// Find the registry key for this display in the hardware area of the registry
// szHardwareID is the PNP ID
// szDriver     is the GUID/instance of the driver
HKEY FindMonitor(TCHAR* szHardwareID, TCHAR* szDriver)
{
	TCHAR szName[1024] = _T("");  DWORD nNameSize = sizeof(szName);
	TCHAR szClass[1024] = _T(""); DWORD nClassSize = sizeof(szClass);
	FILETIME ftLastWriteTime;
	HKEY hKey = NULL;
	HKEY hMonitorKey = NULL;

	bool bFound = false;

	// We need to enumerate all displays and all instances to check the values inside them
	RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("SYSTEM\\CurrentControlSet\\Enum\\DISPLAY\\"), 0, KEY_READ, &hKey);
	for (int i = 0; !bFound && RegEnumKeyEx(hKey, i, szName, &nNameSize, NULL, szClass, &nClassSize, &ftLastWriteTime) != ERROR_NO_MORE_ITEMS; i++) {
		TCHAR szSubName[1024] = _T("");  DWORD nSubNameSize = sizeof(szSubName);
		TCHAR szSubClass[1024] = _T(""); DWORD nSubClassSize = sizeof(szSubClass);
		FILETIME ftSubLastWriteTime;
		HKEY hSubKey = NULL;

		nNameSize = sizeof(szName);
		nClassSize = sizeof(szClass);

		RegOpenKeyEx(hKey, szName, 0, KEY_READ, &hSubKey);
		for (int j = 0; !bFound && RegEnumKeyEx(hSubKey, j, szSubName, &nSubNameSize, NULL, szSubClass, &nSubClassSize, &ftSubLastWriteTime) != ERROR_NO_MORE_ITEMS; j++) {
			HKEY hMonitorInfoKey = NULL;
			TCHAR szKeyValue[1024]; DWORD nSize = sizeof(szKeyValue);
			DWORD nType;

			nSubNameSize = sizeof(szSubName);
			nSubClassSize = sizeof(szSubClass);

			RegOpenKeyEx(hSubKey, szSubName, 0, KEY_READ, &hMonitorKey);

			// Check if this instance is the one we're looking for
			nType = REG_SZ; nSize = sizeof(szKeyValue);
			RegQueryValueEx(hMonitorKey, _T("HardwareID"), NULL, &nType, (BYTE*)szKeyValue, &nSize);
			if (_tcsicmp(szKeyValue, szHardwareID)) {
				continue;
			}
			nType = REG_SZ; nSize = sizeof(szKeyValue);
			RegQueryValueEx(hMonitorKey, _T("Driver"), NULL, &nType, (BYTE*)szKeyValue, &nSize);
			if (_tcsicmp(szKeyValue, szDriver)) {
				continue;
			}

			// Make sure the "Device Parameters" key which contains any EDID data is present
			RegOpenKeyEx(hMonitorKey, _T("Device Parameters"), 0, KEY_READ, &hMonitorInfoKey);
			if (!hMonitorInfoKey) {
				continue;
			}

			// We've found the display we're looking for
			bFound = true;

			RegCloseKey(hMonitorInfoKey);
		}
		RegCloseKey(hSubKey);
	}
	RegCloseKey(hKey);

	if (!bFound) {
		return NULL;
	}

	return hMonitorKey;
}

int ProcessEDID(HKEY hMonitorKey)
{
	HKEY hMonitorInfoKey = NULL;
	BYTE EDIDData[1024]; DWORD nEDIDSize = sizeof(EDIDData);
	DWORD nType;

	if (hMonitorKey == NULL) {
		return 1;
	}

	RegOpenKeyEx(hMonitorKey, _T("Device Parameters"), 0, KEY_READ, &hMonitorInfoKey);
	if (hMonitorInfoKey == NULL) {
		return 1;
	}

	// When Windows can't get valid EDID data from a Display, it creates a BAD_EDID value instead of EDID
	// Thus we can forego ensuring the validity of the EDID data ourselves
	nType = REG_BINARY; nEDIDSize = sizeof(EDIDData);
	if (RegQueryValueEx(hMonitorInfoKey, _T("BAD_EDID"), NULL, &nType, EDIDData, &nEDIDSize) == 0) {
		AddLine(_T("        No EDID data present for this device"));
	}
	nType = REG_BINARY; nEDIDSize = sizeof(EDIDData);
	if (RegQueryValueEx(hMonitorInfoKey, _T("EDID"), NULL, &nType, EDIDData, &nEDIDSize) == 0) {

		// Print some basic information about this display
		AddLine(_T("        Display size ~%dx%dcm, Gamma %1.2lf"), EDIDData[0x15], EDIDData[0x16], ((double)EDIDData[0x17] + 100.0) / 100.0);

		// Print the preferred mode for this display
		if (EDIDData[0x18] & 2) {
			int nPixelClock = ((EDIDData[0x36 + 0x01] << 8) | EDIDData[0x36 + 0x00]) * 10000;

			// Size of the display image in pixels (including blanking and sync)
			int nActiveH = ((EDIDData[0x36 + 0x04] & 0xF0) << 4) | EDIDData[0x36 + 0x02];
			int nBlankH  = ((EDIDData[0x36 + 0x04] & 0x0F) << 8) | EDIDData[0x36 + 0x03];
			int nActiveV = ((EDIDData[0x36 + 0x07] & 0xF0) << 4) | EDIDData[0x36 + 0x05];
			int nBlankV  = ((EDIDData[0x36 + 0x07] & 0x0F) << 8) | EDIDData[0x36 + 0x06];

			// Size of the display image in mm
			int nSizeH = ((EDIDData[0x36 + 0x0E] & 0xF0) << 4) | EDIDData[0x36 + 0x0C];
			int nSizeV = ((EDIDData[0x36 + 0x0E] & 0x0F) << 8) | EDIDData[0x36 + 0x0D];

			// We need to calculate the refresh rate ourselves based on the other numbers
			double dRefresh = 1.0 / ((double)(nActiveH + nBlankH) * (nActiveV + nBlankV) / nPixelClock);

			AddLine(_T("        Preferred mode %dx%d, %1.3lf Hz (%dx%dmm, %1.3lf MHz)"), nActiveH, nActiveV, dRefresh, nSizeH, nSizeV, nPixelClock / 1000000.0);
		}

		{
			// Print the signal limits for this display

			int nLimitsOffset = 0;

			// Find the data block containing the limits
			for (int i = 0; i < 4; i++) {
				if (EDIDData[0x36 + i * 0x12 + 0] == 0x00 && EDIDData[0x36 + i * 0x12 + 1] == 0x00 && EDIDData[0x36 + i * 0x12 + 2] == 0x00 && EDIDData[0x36 + i * 0x12 + 3] == 0xFD) {
					nLimitsOffset = 0x36 + i * 0x12;
					break;
				}
			}

			if (nLimitsOffset) {
				AddLine(_T("        Max. bandwidth %d MHz, H sync %d-%d KHz, V sync %d-%d Hz"), EDIDData[nLimitsOffset + 0x09] * 10, EDIDData[nLimitsOffset + 0x07], EDIDData[nLimitsOffset + 0x08], EDIDData[nLimitsOffset + 0x05], EDIDData[nLimitsOffset + 0x06]);
			}
		}

#if 0
		{

			// Print the raw data block
			BYTE* pData = EDIDData;

			AddText(_T("\r\n        Raw EDID data\r\n"));
			for (int n = 0; n < 8; n++) {
				AddText(_T("            "));

				for (int m = 0; m < 16; m++) {
					AddText(_T("%02X "), *pData & 255);
					pData++;
				}
				AddText(_T(""));
			}
			AddText(_T(""));
		}
#endif

	}

	RegCloseKey(hMonitorInfoKey);

	return 0;
}

// Print info about displays and display adapters
int PrintDisplayInfo()
{
	BOOL (WINAPI* pEnumDisplayDevices)(LPCTSTR, DWORD, PDISPLAY_DEVICE, DWORD) = NULL;
	HINSTANCE hUser32DLL;

	// The EnumDisplayDevices() function is only available on NT based OSes
	hUser32DLL = LoadLibrary(_T("user32.dll"));
	if (hUser32DLL) {
#if defined (UNICODE)
		pEnumDisplayDevices = (BOOL (WINAPI*)(LPCTSTR, DWORD, PDISPLAY_DEVICE, DWORD))GetProcAddress(hUser32DLL, "EnumDisplayDevicesW");
#else
		pEnumDisplayDevices = (BOOL (WINAPI*)(LPCTSTR, DWORD, PDISPLAY_DEVICE, DWORD))GetProcAddress(hUser32DLL, "EnumDisplayDevicesA");
#endif
		if (pEnumDisplayDevices) {
			DISPLAY_DEVICE ddAdapter;
			DISPLAY_DEVICE ddDisplay;

			ddAdapter.cb = sizeof(DISPLAY_DEVICE);
			ddDisplay.cb = sizeof(DISPLAY_DEVICE);

			// Now that we've ensured we can use the EnumDisplayDevices() function, use it to enumerate the connected displays
			AddLine(_T("Installed displays and display adapters:"));

			for (int i = 0; pEnumDisplayDevices(NULL, i, &ddAdapter, 0); i++) {

				// We're only interested in real display adapters
				if (!(ddAdapter.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER)) {
					for (int j = 0; pEnumDisplayDevices(ddAdapter.DeviceName, j, &ddDisplay, 0); j++) {

						HKEY hKey = NULL;
						HKEY hMonitorKey = NULL;

						TCHAR szMfg[1024] = _T("");		   DWORD nMfgSize = sizeof(szMfg);
						TCHAR szDeviceDesc[1024] = _T(""); DWORD nDeviceDescSize = sizeof(szDeviceDesc);

						// If the display is active, get the data about it
						if (ddDisplay.StateFlags & DISPLAY_DEVICE_ACTIVE) {
							if (!_tcsnicmp(ddDisplay.DeviceKey, _T("\\Registry\\Machine\\"), 18)) {
								TCHAR szDriver[1024]; DWORD nSize = sizeof(szDriver);
								DWORD nType = REG_SZ;
								RegOpenKeyEx(HKEY_LOCAL_MACHINE, ddDisplay.DeviceKey + 18, 0, KEY_READ, &hKey);

								// Find the registry key for this display in the hardware area of the registry
								RegQueryValueEx(hKey, _T("MatchingDeviceId"), NULL, &nType, (BYTE*)szDriver, &nSize);
								hMonitorKey = FindMonitor(szDriver, ddDisplay.DeviceKey + 57);

								RegQueryValueEx(hMonitorKey, _T("DeviceDesc"), NULL, &nType, (BYTE*)szDeviceDesc, &nDeviceDescSize);
								RegQueryValueEx(hMonitorKey, _T("Mfg"), NULL, &nType, (BYTE*)szMfg, &nMfgSize);
							}

							// Print the information we've got so far
							TCHAR* pszStatus = _T("");
							if (ddAdapter.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) {
								pszStatus = _T(" (primary)");
							}
							if (!(ddAdapter.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)) {
								pszStatus = _T(" (disabled)");
							}
							if (szMfg[0] && szDeviceDesc[0]) {
								AddLine(_T("    %s %s on %s%s"), szMfg, szDeviceDesc, ddAdapter.DeviceString, pszStatus);
							} else {
								AddLine(_T("    %s on %s%s"), ddDisplay.DeviceString, ddAdapter.DeviceString, pszStatus);
							}

							ProcessEDID(hMonitorKey);

							if (hMonitorKey) {
								RegCloseKey(hMonitorKey);
							}
							if (hKey) {
								RegCloseKey(hKey);
							}
						}
					}
				}
			}
		}
		FreeLibrary(hUser32DLL);
	}

	return 0;
}

// Print FB Alpha settings
int PrintFBAInfo()
{
	InterfaceInfo* pInfo;

	AddLine(_T(APP_TITLE) _T(" information:"));
	AddLine(_T(""));

	AddLine(_T("Built on ") _T(MAKE_STRING(BUILD_DATE)) _T(", ") _T(MAKE_STRING(BUILD_TIME))  _T(", using ") _T(MAKE_STRING(BUILD_COMP)) _T("."));
	AddLine(_T("    Optimised for ") _T(MAKE_STRING(BUILD_CPU)) _T(" CPUs."));
#if defined (_UNICODE)
	AddLine(_T("    Using Unicode for all text."));
#else
	AddLine(_T("    Using multi-byte characters for all text, active codepage is %d."), GetACP());
#endif
#if defined (FBNEO_DEBUG)
	AddLine(_T("    Debug functionality present."));
#else
	AddLine(_T("    Debug functionality absent."));
#endif
	AddLine(_T(""));

	AddLine(_T("MMX optimisations %s."), bBurnUseMMX ? _T("enabled") : _T("disabled"));
#ifdef BUILD_A68K
	if (bBurnUseASMCPUEmulation) {
		AddLine(_T("A68K emulation core enabled for MC68000 emulation."));
		AddLine(_T("Musashi emulation core enabled for MC68010/MC68EC020 emulation."));
	} else {
		AddLine(_T("Musashi emulation core enabled for MC680x0 family emulation."));
	}
#else
	AddLine(_T("Musashi emulation core enabled for MC680x0 family emulation."));
#endif
	AddLine(_T(""));

	if (bDrvOkay) {
		TCHAR szName[1024];

		int n = _stprintf(szName, _T("Emulating %s (%hs)"), BurnDrvGetText(DRV_NAME), DecorateGameName(nBurnDrvActive));
		if (n >= 70) {
			_tcscpy(szName + 66, _T("...)"));
		}
		AddLine(_T("%s"), szName);
		AddLine(_T("    Vertical refresh is %.2lf Hz."), (double)nBurnFPS / 100);
		AddLine(_T("    CPU running at %i%% of normal frequency."), (int)((double)nBurnCPUSpeedAdjust * 100 / 256 + 0.5));
		AddLine(_T(""));
	} else {
		AddLine(_T("Not emulating any game."));
		AddLine(_T(""));
	}

	if ((pInfo = VidGetInfo()) != NULL) {
		AddLine(_T("Video settings:"));
		PrintInterfaceInfo(pInfo);
	}
	AddLine(_T(""));

	if ((pInfo = AudGetInfo()) != NULL) {
		AddLine(_T("Audio settings:"));
		PrintInterfaceInfo(pInfo);
	}
	AddLine(_T(""));

	if ((pInfo = InputGetInfo()) != NULL) {
		AddLine(_T("Input settings:"));
		PrintInterfaceInfo(pInfo);
	}
	AddLine(_T(""));

	if ((pInfo = ProfileGetInfo()) != NULL) {
		AddLine(_T("Profiling settings:"));
		PrintInterfaceInfo(pInfo);
	}

	return 0;
}

// Print process information
int PrintProcessInfo()
{
#if 1

	HANDLE hModuleSnap;
	MODULEENTRY32 me32;

	hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
	if (hModuleSnap == INVALID_HANDLE_VALUE) {
		AddLine(_T("Unable to retrieve detailed process information."));
	} else {
		me32.dwSize = sizeof(MODULEENTRY32);
		if(!Module32First(hModuleSnap, &me32)) {
			AddLine(_T("Unable to retrieve detailed process information."));
		} else {
			AddLine(_T("Detailed process information about %s:"), me32.szModule);
			AddLine(_T(""));
			AddLine(_T("%s (base address 0x%p, size %i KB)"), me32.szModule, me32.modBaseAddr, me32.modBaseSize / 1024);
			AddLine(_T(""));

			if (pExceptionPointers) {
				bool bFound = false;
				do {
					if (me32.modBaseAddr <= pExceptionPointers->ExceptionRecord->ExceptionAddress && (me32.modBaseAddr + me32.modBaseSize) > pExceptionPointers->ExceptionRecord->ExceptionAddress) {
						bFound = true;
						break;
					}
				} while (Module32Next(hModuleSnap, &me32));

				if (bFound) {
					AddLine(_T("Exception occurred in module %s:"), me32.szModule);
					AddLine(_T("    %20s (base address 0x%p, size %6i KB)"), me32.szModule, me32.modBaseAddr, me32.modBaseSize / 1024);
					AddLine(_T(""));
				} else {
					AddLine(_T("Unable to locate module in which exception occurred"));
					AddLine(_T(""));
				}
			}

			Module32First(hModuleSnap, &me32);
			AddLine(_T("Modules loaded by %s:"), me32.szModule);
			while (Module32Next(hModuleSnap, &me32)){
				AddLine(_T("    %20s (base address 0x%p, size %6i KB)"), me32.szModule, me32.modBaseAddr, me32.modBaseSize / 1024);
			}
		}

		CloseHandle(hModuleSnap);
	}

#endif

	return 0;
}

// Print information about installed devices
int PrintDeviceInfo()
{
	// Get a list of all devices that are present and enabled
	HDEVINFO hDevInfo = SetupDiGetClassDevs(NULL, NULL, NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT | DIGCF_PROFILE);
	if (hDevInfo != INVALID_HANDLE_VALUE) {
		SC_HANDLE (WINAPI* pOpenSCManager)(LPCTSTR, LPCTSTR, DWORD) = NULL;
		SC_HANDLE (WINAPI* pOpenService)(SC_HANDLE, LPCTSTR, DWORD) = NULL;
		BOOL (WINAPI* pQueryServiceConfig)(SC_HANDLE, LPQUERY_SERVICE_CONFIG, DWORD, LPDWORD) = NULL;
		BOOL (WINAPI* pCloseServiceHandle)(SC_HANDLE) = NULL;
		HINSTANCE hAdvapi32DLL;

		SP_DEVINFO_DATA did;

		hAdvapi32DLL = LoadLibrary(_T("advapi32.dll"));
		if (hAdvapi32DLL) {
#if defined (_UNICODE)
			pOpenSCManager = (SC_HANDLE (WINAPI*)(LPCTSTR, LPCTSTR, DWORD))GetProcAddress(hAdvapi32DLL, "OpenSCManagerW");
			pOpenService = (SC_HANDLE (WINAPI*)(SC_HANDLE, LPCTSTR, DWORD))GetProcAddress(hAdvapi32DLL, "OpenServiceW");
			pQueryServiceConfig = (BOOL (WINAPI*)(SC_HANDLE, LPQUERY_SERVICE_CONFIG, DWORD, LPDWORD))GetProcAddress(hAdvapi32DLL, "QueryServiceConfigW");
#else
			pOpenSCManager = (SC_HANDLE (WINAPI*)(LPCTSTR, LPCTSTR, DWORD))GetProcAddress(hAdvapi32DLL, "OpenSCManagerA");
			pOpenService = (SC_HANDLE (WINAPI*)(SC_HANDLE, LPCTSTR, DWORD))GetProcAddress(hAdvapi32DLL, "OpenServiceA");
			pQueryServiceConfig = (BOOL (WINAPI*)(SC_HANDLE, LPQUERY_SERVICE_CONFIG, DWORD, LPDWORD))GetProcAddress(hAdvapi32DLL, "QueryServiceConfigA");
#endif
			pCloseServiceHandle = (BOOL (WINAPI*)(SC_HANDLE))GetProcAddress(hAdvapi32DLL, "CloseServiceHandle");
		}

		AddLine(_T("Installed devices (partial list):"));
		AddLine(_T(""));

		// Enumerate all devices in the list
		did.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
		for (int i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &did); i++) {
			TCHAR* list[] = { _T("display"), _T("media"), _T("hid"), _T("hidclass"), _T("mouse"), _T("system"), NULL };
			TCHAR szClass[1024] = _T("");

			// Get the installer class of a device
			SetupDiGetDeviceRegistryProperty(hDevInfo, &did, SPDRP_CLASS, NULL, (BYTE*)szClass, 1024, NULL);

			// determine if the device is of a class we're interested in
			for (int j = 0; list[j]; j++) {
				if (!_tcsicmp(list[j], szClass)) {
					TCHAR szName[1024] = _T("");
					TCHAR szService[1024] = _T("");
					TCHAR szDriverDate[1024] = _T("");
					TCHAR szDriverVersion[1024] = _T("");
					TCHAR szImagePath[1024] = _T("");
					HKEY hKey;

					// Get the device name
					SetupDiGetDeviceRegistryProperty(hDevInfo, &did, SPDRP_DEVICEDESC, NULL, (BYTE*)szName, 1024, NULL);

					// Check if there are any lower filter drivers
					SetupDiGetDeviceRegistryProperty(hDevInfo, &did, SPDRP_LOWERFILTERS, NULL, (BYTE*)szService, 1024, NULL);
					if (szService[0] == _T('\0')) {
						// If we haven't got any lower filter drivers, just use the service key
						SetupDiGetDeviceRegistryProperty(hDevInfo, &did, SPDRP_SERVICE, NULL, (BYTE*)szService, 1024, NULL);
					}

					// Get driver info
					hKey = SetupDiOpenDevRegKey(hDevInfo, &did, DICS_FLAG_GLOBAL, 0, DIREG_DRV, KEY_READ);
					if (hKey != INVALID_HANDLE_VALUE) {
						DWORD nType = REG_SZ;
						DWORD nSize = 1024;

						RegQueryValueEx(hKey, _T("DriverVersion"), NULL, &nType, (BYTE*)szDriverVersion, &nSize);
						RegQueryValueEx(hKey, _T("DriverDate"), NULL, &nType, (BYTE*)szDriverDate, &nSize);
						RegCloseKey(hKey);
					}

					// If we have a driver, get the filename
					if (szService[0] && pOpenSCManager && pOpenService && pQueryServiceConfig && pCloseServiceHandle) {
						SC_HANDLE hSCManager, hService;
						QUERY_SERVICE_CONFIG* pSC = NULL;
						DWORD nSize;

						hSCManager = OpenSCManager(NULL, NULL, GENERIC_READ);
						hService = OpenService(hSCManager, szService, GENERIC_READ);

						QueryServiceConfig(hService, NULL, 0, &nSize);
						pSC = (QUERY_SERVICE_CONFIG*)malloc(nSize);

						if (QueryServiceConfig(hService, pSC, nSize, &nSize)) {
							_tcscpy(szImagePath, pSC->lpBinaryPathName);
						}

						if (pSC) {
							free(pSC);
							pSC = NULL;
						}

						CloseServiceHandle(hService);
						CloseServiceHandle(hSCManager);
					}

#if 0
					if (szImagePath[0] == '\0') {
						if (osvi.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
							// Officially, we shouldn't be doing this, but there's no API for getting this info on Win9x
							const TCHAR* pszServices = "SYSTEM\\CurrentControlSet\\Services\\";
							char szFullService[1024] = "";

							_tcscpy(szFullService, pszServices);
							_tcscat(szFullService, szService);
							if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, szFullService, 0, KEY_EXECUTE, &hKey) == ERROR_SUCCESS) {
								DWORD nType = REG_SZ;
								DWORD nSize = 1024;

								RegQueryValueEx(hKey, _T("ImagePath"), NULL, &nType, szImagePath, &nSize);
								RegCloseKey(hKey);
							}
						}
					}
#endif

					// Print the information
					if (j < 2 || szImagePath[0]) {
						AddLine(_T("    %s"), szName);
						AddLine(_T("        %s"), szImagePath[0] ? szImagePath : _T("no driver needed"));

						if (szDriverVersion[0]) {
							AddLine(_T("        version %s (%s)"), szDriverVersion, szDriverDate[0] ? szDriverDate : _T("no date"));
						}
					}

					break;
				}
			}
		}

		FreeLibrary(hAdvapi32DLL);

		SetupDiDestroyDeviceInfoList(hDevInfo);
	}

	return 0;
}

static INT_PTR CALLBACK SysInfoProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	static int nReturnCode = 0;

	switch (Msg) {
		case WM_INITDIALOG: {
		    time_t nTime;
			tm* tmTime;

			nReturnCode = 0;

			pszTextBuffer = NULL;
			nTextBufferSize = 0;

			time(&nTime);
			tmTime = localtime(&nTime);

			hLogFont = CreateFont(14, 0, 0, 0, FW_NORMAL, 0, 0, 0, 0, 0, 0, DEFAULT_QUALITY, FF_MODERN, _T(""));

			hCodeBGBrush = NULL;
			hCodeFont = NULL;

			if (pExceptionPointers) {
				TCHAR szText[1024] = _T("");
				hCodeBGBrush = CreateSolidBrush(RGB(0,0,0));
				hCodeFont = CreateFont(22, 0, 0, 0, FW_BOLD, 0, 0, 0, 0, 0, 0, ANTIALIASED_QUALITY, FF_MODERN, _T("Lucida Console"));
				SendDlgItemMessage(hDlg, IDC_SYSINFO_CODE, WM_SETFONT, (WPARAM)hCodeFont, (LPARAM)0);

				_stprintf(szText, _T("Guru Meditation #%08X.%08X"), pExceptionPointers->ExceptionRecord->ExceptionCode, (INT_PTR)pExceptionPointers->ExceptionRecord->ExceptionAddress);
				SendDlgItemMessage(hDlg, IDC_SYSINFO_CODE, WM_SETTEXT, (WPARAM)0, (LPARAM)szText);

				AddLine(_T(APP_TITLE) _T(" v%.20s fatal exception report (%s)."), szAppBurnVer, _tasctime(tmTime));

				AddLine(_T(""));
				AddLine(_T("----------------------------------------------------------------------"));

				// Exception
				PrintExceptionInfo();
			} else {
				AddLine(_T(APP_TITLE) _T(" v%.20s system information (%s)."), szAppBurnVer, _tasctime(tmTime));
			}

			AddLine(_T(""));
			AddLine(_T("----------------------------------------------------------------------"));

			AddLine(_T("System information:"));
			AddLine(_T(""));

			// OS information
			PrintOSInfo();

			// CPU information
			PrintCPUInfo();

			AddLine(_T(""));

			// Global memory information
			PrintGlobalMemoryInfo();

			AddLine(_T(""));

			// Displays and display adapters
			PrintDisplayInfo();

			AddLine(_T(""));
			AddLine(_T("----------------------------------------------------------------------"));

			PrintFBAInfo();

			AddLine(_T(""));
			AddLine(_T("----------------------------------------------------------------------"));

			// Process information
			PrintProcessInfo();

			AddLine(_T(""));
			AddLine(_T("----------------------------------------------------------------------"));

			if (bPrintDriverInfo) {
				// Device information
				PrintDeviceInfo();

				AddLine(_T(""));
				AddLine(_T("----------------------------------------------------------------------"));
			}

			SendDlgItemMessage(hDlg, IDC_SYSINFO_EDIT, WM_SETFONT, (WPARAM)hLogFont, (LPARAM)0);
			SendDlgItemMessage(hDlg, IDC_SYSINFO_EDIT, EM_SETMARGINS, (WPARAM)EC_LEFTMARGIN, (LPARAM)3);
			SendDlgItemMessage(hDlg, IDC_SYSINFO_EDIT, WM_SETTEXT, (WPARAM)0, (LPARAM)pszTextBuffer);

			WndInMid(hDlg, hScrnWnd);

			return TRUE;
		}

		case WM_CLOSE: {
			if (pszTextBuffer) {
				free(pszTextBuffer);
				pszTextBuffer = NULL;
			}
			nTextBufferSize = 0;

			DeleteObject(hLogFont);
			if (hCodeFont) {
				DeleteObject(hCodeFont);
			}
			if (hCodeBGBrush) {
				DeleteObject(hCodeBGBrush);
			}
			EndDialog(hDlg, nReturnCode);
			break;
		}

		case WM_CTLCOLORSTATIC: {
			if ((HWND)lParam == GetDlgItem(hDlg, IDC_SYSINFO_EDIT)) {
				return (INT_PTR)GetSysColorBrush(15);
			}
			if ((HWND)lParam == GetDlgItem(hDlg, IDC_SYSINFO_CODE)) {
				SetTextColor((HDC)wParam, RGB(255, 0, 0));
				SetBkMode((HDC)wParam, TRANSPARENT);
				return (INT_PTR)hCodeBGBrush;
			}
			break;
		}

		case WM_COMMAND: {
			if (HIWORD(wParam) == BN_CLICKED) {
				if (LOWORD(wParam) == IDOK) {
					SendMessage(hDlg, WM_CLOSE, 0, 0);
				}
				if (LOWORD(wParam) == IDC_SYSINFO_DEBUG) {
					nReturnCode = 1;
					SendMessage(hDlg, WM_CLOSE, 0, 0);
				}
				if (LOWORD(wParam) == IDC_SYSINFO_SHOW && pExceptionPointers) {
					RECT rect = { 0, 0, 0, 125 };

					SendMessage(hDlg, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(hDlg, IDOK), TRUE);
					EnableWindow(GetDlgItem(hDlg, IDC_SYSINFO_SHOW), FALSE);

					MapDialogRect(hDlg, &rect);
					int nSize = rect.bottom;
					GetWindowRect(hDlg, &rect);
					MoveWindow(hDlg, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top + nSize, TRUE);

					WndInMid(hDlg, hScrnWnd);
				}
				if (LOWORD(wParam) == IDC_SYSINFO_LOG_SAVE) {
					FILE* fp = NULL;

					SendMessage(hDlg, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(hDlg, IDOK), TRUE);

					if (pExceptionPointers) {
						TCHAR szLogName[MAX_PATH];
						_stprintf(szLogName, _T("config/%s.error.log"), szAppExeName);
						fp = _tfopen(szLogName, _T("ab"));

						EnableWindow(GetDlgItem(hDlg, IDC_SYSINFO_LOG_SAVE), FALSE);
					} else {
						TCHAR szFilter[1024];

						_stprintf(szFilter, FBALoadStringEx(hAppInst, IDS_DISK_TEXT, true));
						memcpy(szFilter + _tcslen(szFilter), _T(" (*.txt)\0*.txt\0\0"), 16 * sizeof(TCHAR));

						_stprintf(szChoice, _T("%s system information.txt"), szAppExeName);

						memset(&ofn, 0, sizeof(ofn));
						ofn.lStructSize = sizeof(ofn);
						ofn.hwndOwner = hDlg;
						ofn.lpstrFilter = szFilter;
						ofn.lpstrFile = szChoice;
						ofn.nMaxFile = sizeof(szChoice) / sizeof(TCHAR);
						ofn.lpstrInitialDir = _T(".");
						ofn.Flags = OFN_NOCHANGEDIR | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
						ofn.lpstrDefExt = _T("txt");
						ofn.lpstrTitle = FBALoadStringEx(hAppInst, IDS_DISK_SAVEREPORT, true);

						if (GetSaveFileName(&ofn)) {
							fp = _tfopen(szChoice, _T("wb"));
						}
					}

					if (fp) {
						TCHAR* szText;
						int nSize = SendDlgItemMessage(hDlg, IDC_SYSINFO_EDIT, WM_GETTEXTLENGTH, 0, 0);
						szText = (TCHAR*)malloc((nSize + 1) * sizeof(TCHAR));

#if defined (_UNICODE)
						// write a Unicode Byte Order Mark if needed
						if (ftell(fp) == 0) {
							WRITE_UNICODE_BOM(fp);
						}
#endif

						if (nSize && szText) {
							SendDlgItemMessage(hDlg, IDC_SYSINFO_EDIT, WM_GETTEXT, (WPARAM)(nSize + 1) * sizeof(TCHAR), (LPARAM)szText);
							fwrite(szText, sizeof(TCHAR), nSize, fp);
							_ftprintf(fp, _T(""));
						}
						if (szText) {
							free(szText);
							szText = NULL;
						}
						fclose(fp);
					}
				}
			}
		}
	}

	return 0;
}

LONG CALLBACK ExceptionFilter(_EXCEPTION_POINTERS* pExceptionInfo)
{
	int nRet;

	// If we're getting recursive calls to this function, bail out
	if (nRecursion++) {
		if (nRecursion <= 2) {
			MessageBox(hScrnWnd, _T(APP_TITLE) _T(" will now be terminated."), _T(APP_TITLE) _T(" Fatal exception"), MB_OK | MB_SETFOREGROUND);
			AppCleanup();
		}
#ifdef _DEBUG
		return EXCEPTION_CONTINUE_SEARCH;
#else
		return EXCEPTION_EXECUTE_HANDLER;
#endif
	}

	SplashDestroy(1);
	AudSoundStop();

	pExceptionPointers = pExceptionInfo;

	nRet = FBADialogBox(hAppInst, MAKEINTRESOURCE(IDD_EXCEPTION), hScrnWnd, (DLGPROC)SysInfoProc);

	switch (nRet) {
		case 1:
			return EXCEPTION_CONTINUE_SEARCH;
		default:
			AppCleanup();
			return EXCEPTION_EXECUTE_HANDLER;
	}
}

int SystemInfoCreate()
{
	pExceptionPointers = NULL;

	FBADialogBox(hAppInst, MAKEINTRESOURCE(IDD_SYSINFO), hScrnWnd, (DLGPROC)SysInfoProc);

	return 0;
}
