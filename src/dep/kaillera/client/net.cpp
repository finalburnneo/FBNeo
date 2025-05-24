#ifdef _UNICODE
#undef _UNICODE
#endif

#include <stdio.h>
#include <windows.h>
#include "net.h"

HMODULE Kaillera_HDLL;
int Kaillera_Initialised;

int (WINAPI *Kaillera_Get_Version) (char *version);
int (WINAPI *Kaillera_Init) ();
int (WINAPI *Kaillera_Shutdown) ();
int (WINAPI *Kaillera_Set_Infos) (kailleraInfos *infos);
int (WINAPI *Kaillera_Select_Server_Dialog) (HWND parent);
int (WINAPI *Kaillera_Modify_Play_Values) (void *values, int size);
int (WINAPI *Kaillera_Chat_Send) (char *text);
int (WINAPI *Kaillera_End_Game) ();


int WINAPI Empty_Kaillera_Get_Version(char* /*version*/)
{
	return 0;
}

int WINAPI Empty_Kaillera_Init()
{
	return 0;
}

int WINAPI Empty_Kaillera_Shutdown()
{
	return 0;
}

int WINAPI Empty_Kaillera_Set_Infos(kailleraInfos* /*infos*/)
{
	return 0;
}

int WINAPI Empty_Kaillera_Select_Server_Dialog(HWND /*parent*/)
{
	return 0;
}

int WINAPI Empty_Kaillera_Modify_Play_Values(void* /*values*/, int /*size*/)
{
	return 0;
}

int WINAPI Empty_Kaillera_Chat_Send(char* /*text*/)
{
	return 0;
}

int WINAPI Empty_Kaillera_End_Game()
{
	return 0;
}

int Init_Network(void)
{
#if 0
#if defined (_UNICODE)
	Kaillera_HDLL = LoadLibrary(L"kailleraclient.dll");
#else
	Kaillera_HDLL = LoadLibraryA("kailleraclient.dll");
#endif
#endif // 0

#ifdef BUILD_X64_EXE
#define	kailleraGetVersion			"kailleraGetVersion"
#define	kailleraInit				"kailleraInit"
#define	kailleraShutdown			"kailleraShutdown"
#define	kailleraSetInfos			"kailleraSetInfos"
#define	kailleraSelectServerDialog	"kailleraSelectServerDialog"
#define	kailleraModifyPlayValues	"kailleraModifyPlayValues"
#define	kailleraChatSend			"kailleraChatSend"
#define	kailleraEndGame				"kailleraEndGame"
#else
#define	kailleraGetVersion			"_kailleraGetVersion@4"
#define	kailleraInit				"_kailleraInit@0"
#define	kailleraShutdown			"_kailleraShutdown@0"
#define	kailleraSetInfos			"_kailleraSetInfos@4"
#define	kailleraSelectServerDialog	"_kailleraSelectServerDialog@4"
#define	kailleraModifyPlayValues	"_kailleraModifyPlayValues@8"
#define	kailleraChatSend			"_kailleraChatSend@4"
#define	kailleraEndGame				"_kailleraEndGame@0"
#endif

	Kaillera_HDLL = LoadLibraryA("kailleraclient.dll");

	if ((Kaillera_HDLL == NULL) || (ERROR_BAD_EXE_FORMAT == GetLastError())) {
		Kaillera_HDLL = LoadLibraryA("kailleraclient64.dll");

	}
	
	if (Kaillera_HDLL != NULL)
	{
		Kaillera_Get_Version          = (int (WINAPI *)(char *version)) GetProcAddress(Kaillera_HDLL, kailleraGetVersion);
		Kaillera_Init                 = (int (WINAPI *)()) GetProcAddress(Kaillera_HDLL, kailleraInit);
		Kaillera_Shutdown             = (int (WINAPI *)()) GetProcAddress(Kaillera_HDLL, kailleraShutdown);
		Kaillera_Set_Infos            = (int (WINAPI *)(kailleraInfos *infos)) GetProcAddress(Kaillera_HDLL, kailleraSetInfos);
		Kaillera_Select_Server_Dialog = (int (WINAPI *)(HWND parent)) GetProcAddress(Kaillera_HDLL, kailleraSelectServerDialog);
		Kaillera_Modify_Play_Values   = (int (WINAPI *)(void *values, int size)) GetProcAddress(Kaillera_HDLL, kailleraModifyPlayValues);
		Kaillera_Chat_Send            = (int (WINAPI *)(char *text)) GetProcAddress(Kaillera_HDLL, kailleraChatSend);
		Kaillera_End_Game             = (int (WINAPI *)()) GetProcAddress(Kaillera_HDLL, kailleraEndGame);

		if ((Kaillera_Get_Version != NULL) && (Kaillera_Init != NULL) && (Kaillera_Shutdown != NULL) && (Kaillera_Set_Infos != NULL) && (Kaillera_Select_Server_Dialog != NULL) && (Kaillera_Modify_Play_Values != NULL) && (Kaillera_Chat_Send != NULL) && (Kaillera_End_Game != NULL))
		{			
			Kaillera_Init();
			Kaillera_Initialised = 1;
			return 0;
		}

		FreeLibrary(Kaillera_HDLL);
	} else {
	}

	Kaillera_Get_Version          = Empty_Kaillera_Get_Version;
	Kaillera_Init                 = Empty_Kaillera_Init;
	Kaillera_Shutdown             = Empty_Kaillera_Shutdown;
	Kaillera_Set_Infos            = Empty_Kaillera_Set_Infos;
	Kaillera_Select_Server_Dialog = Empty_Kaillera_Select_Server_Dialog;
	Kaillera_Modify_Play_Values   = Empty_Kaillera_Modify_Play_Values;
	Kaillera_Chat_Send            = Empty_Kaillera_Chat_Send;
	Kaillera_End_Game             = Empty_Kaillera_End_Game;

	Kaillera_Initialised = 0;
	return 1;
}


void End_Network(void)
{
	if (Kaillera_Initialised)
	{
		Kaillera_Shutdown();
		FreeLibrary(Kaillera_HDLL);
	}
}
