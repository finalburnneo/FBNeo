#define SCREEN_WIDTH 256
#define SCREEN_HEIGHT (240-16) // top and bottom 8px is overscan
#define SCREEN_HEIGHT_PAL (SCREEN_HEIGHT + 8)

extern UINT8 NESRecalc;
extern UINT8 NESDrvReset;

extern UINT8 NESJoy1[8];
extern UINT8 NESJoy2[8];
extern UINT8 NESJoy3[8];
extern UINT8 NESJoy4[8];
extern UINT8 NESCoin[8];
extern UINT8 NESDips[4];

// Zapper emulation
extern INT16 ZapperX;
extern INT16 ZapperY;
extern UINT8 ZapperFire;
extern UINT8 ZapperReload;
extern UINT8 ZapperReloadTimer;
extern UINT8 FDSEject;
extern UINT8 FDSSwap;

INT32 NESInit();
INT32 NES4ScoreInit();
INT32 NESHori4pInit();
INT32 NESReversedInit();
INT32 NESZapperInit();
INT32 topriderInit();
INT32 NESExit();
INT32 NESDraw();
INT32 NESFrame();
INT32 NESScan(INT32 nAction, INT32 *pnMin);

