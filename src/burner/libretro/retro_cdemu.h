#ifndef __RETRO_CDEMU__
#define __RETRO_CDEMU__

#include "burner.h"

TCHAR* GetIsoPath();
INT32 CDEmuInit();
INT32 CDEmuExit();
INT32 CDEmuStop();
INT32 CDEmuPlay(UINT8 M, UINT8 S, UINT8 F);
INT32 CDEmuLoadSector(INT32 LBA, char* pBuffer);
UINT8* CDEmuReadTOC(INT32 track);
UINT8* CDEmuReadQChannel();
INT32 CDEmuGetSoundBuffer(INT16* buffer, INT32 samples);
INT32 CDEmuScan(INT32 nAction, INT32 *pnMin);

extern CDEmuStatusValue CDEmuStatus;
extern TCHAR CDEmuImage[MAX_PATH];

#endif
