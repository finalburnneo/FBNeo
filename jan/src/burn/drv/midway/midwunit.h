#pragma once


extern UINT8 nWolfUnitJoy1[32];
extern UINT8 nWolfUnitJoy2[32];
extern UINT8 nWolfUnitJoy3[32];
extern UINT8 nWolfUnitDSW[8];
extern UINT8 nWolfUnitRecalc;

#define WUNIT_GFX(msb,s)		((msb & 0xFF) << 12) | ((s & 7) << 8)
#define WUNIT_GFX_ADR(value)	((value & (0xFF << 12)) >> 12)
#define WUNIT_GFX_OFF(value)	((value & (7 << 8)) >> 8)

INT32 WolfUnitInit();
INT32 WolfUnitFrame();
INT32 WolfUnitExit();
INT32 WolfUnitDraw();
INT32 WolfUnitScan(INT32 nAction, INT32 *pnMin);