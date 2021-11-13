#ifndef _LUAENGINE_H
#define _LUAENGINE_H

enum LuaCallID
{
	LUACALL_BEFOREEMULATION,
	LUACALL_AFTEREMULATION,
	LUACALL_BEFOREEXIT,
	LUACALL_ONSTART,

	LUACALL_HOTKEY_1,
	LUACALL_HOTKEY_2,
	LUACALL_HOTKEY_3,
	LUACALL_HOTKEY_4,
	LUACALL_HOTKEY_5,
	LUACALL_HOTKEY_6,
	LUACALL_HOTKEY_7,
	LUACALL_HOTKEY_8,
	LUACALL_HOTKEY_9,

	LUACALL_COUNT
};
void CallRegisteredLuaFunctions(LuaCallID calltype);

enum LuaMemHookType
{
	LUAMEMHOOK_WRITE,
	LUAMEMHOOK_READ,
	LUAMEMHOOK_EXEC,
	LUAMEMHOOK_WRITE_SUB,
	LUAMEMHOOK_READ_SUB,
	LUAMEMHOOK_EXEC_SUB,

	LUAMEMHOOK_COUNT
};
void CallRegisteredLuaMemHook(unsigned int address, int size, unsigned int value, LuaMemHookType hookType);

//void FBA_LuaWrite(UINT32 addr);
void FBA_LuaFrameBoundary();
int FBA_LoadLuaCode(const char *filename);
void FBA_ReloadLuaCode();
void FBA_LuaStop();
int FBA_LuaRunning();

int FBA_LuaUsingJoypad();
UINT32 FBA_LuaReadJoypad();
int FBA_LuaSpeed();
//int FBA_LuaFrameskip();
int FBA_LuaRerecordCountSkip();

void FBA_LuaGui(unsigned char *s, int width, int height, int bpp, int pitch);

//void FBA_LuaWriteInform(); // DEADBEEF

void FBA_LuaClearGui();
void FBA_LuaEnableGui(UINT8 enabled);

char* FBA_GetLuaScriptName();
struct lua_State* FBA_GetLuaState();

void luasav_save(const char *filename);
void luasav_load(const char *filename);

INT_PTR CALLBACK DlgLuaScriptDialog(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);

#endif
