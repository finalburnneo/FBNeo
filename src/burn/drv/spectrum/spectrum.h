extern UINT8 SpecDips[2];
extern UINT8 SpecInput[0x10];
extern UINT8 SpecReset;
extern UINT8 SpecRecalc;
extern UINT8 SpecInputKbd[0x10][0x05];
extern struct BurnInputInfo SpecInputList[];

INT32 SpecInit();
INT32 SpecSlowTAPInit();
INT32 Spec128KInit();
INT32 Spec128KSlowTAPInit();
INT32 Spec128KPlus2Init();
INT32 Spec128KInvesInit();
INT32 SpecExit();
INT32 SpecDraw();
INT32 SpecFrame();
INT32 SpecScan(INT32 nAction, INT32* pnMin);
INT32 SpectrumGetZipName(char** pszName, UINT32 i);

