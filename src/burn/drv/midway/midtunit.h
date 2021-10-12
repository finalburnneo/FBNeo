#pragma once


extern UINT8 nTUnitJoy1[32];
extern UINT8 nTUnitJoy2[32];
extern UINT8 nTUnitJoy3[32];
extern UINT8 nTUnitDSW[2];
extern UINT8 nTUnitReset;
extern UINT8 nTUnitRecalc;

#define TUNIT_GFX(msb,s)		((msb & 0xFF) << 12) | ((s & 7) << 8)
#define TUNIT_GFX_ADR(value)	((value & (0xFF << 12)) >> 12)
#define TUNIT_GFX_OFF(value)	((value & (7 << 8)) >> 8)

INT32 TUnitInit();
INT32 TUnitFrame();
INT32 TUnitExit();
INT32 TUnitDraw();
INT32 TUnitScan(INT32 nAction, INT32 *pnMin);

extern UINT8 TUnitIsMK;
extern UINT8 TUnitIsMKTurbo;
extern UINT8 TUnitIsMK2;
extern UINT8 TUnitIsNbajam;
extern UINT8 TUnitIsNbajamTe;
extern UINT8 TUnitIsJdreddp;
