extern UINT8 SMSPaletteRecalc;

extern UINT8 SMSReset;
extern UINT8 MastInput[2];
extern UINT8 SMSDips[3];
extern UINT8 SMSJoy1[12];
INT32 SMSGetZipName(char** pszName, UINT32 i);

INT32 SMSInit();
INT32 SMSExit();
INT32 SMSDraw();
INT32 SMSFrame();
INT32 SMSScan(INT32 nAction, INT32 *pnMin);
