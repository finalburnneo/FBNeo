#define MAX_GUNS	4

struct BurnDialINF {
	INT32 Velocity;
	INT32 Backward;
	INT32 Forward;
};

extern INT32 nBurnGunNumPlayers;
extern bool bBurnGunAutoHide;

extern INT32 BurnGunX[MAX_GUNS];
extern INT32 BurnGunY[MAX_GUNS];

UINT8 BurnGunReturnX(INT32 num);
UINT8 BurnGunReturnY(INT32 num);

extern void BurnGunInit(INT32 nNumPlayers, bool bDrawTargets);
void BurnGunExit();
void BurnGunScan();
extern void BurnGunDrawTarget(INT32 num, INT32 x, INT32 y);
extern void BurnGunMakeInputs(INT32 num, INT16 x, INT16 y);

// Using BurnPaddle gives you 2 paddles (A & B) per player initted.
// BurnPaddleReturn[A/B] returns Velocity and directional data, see
// drv/pre90s/d_tempest.cpp for use.
BurnDialINF BurnPaddleReturnA(INT32 num);
BurnDialINF BurnPaddleReturnB(INT32 num);
void BurnPaddleSetWrap(INT32 num, INT32 xmin, INT32 xmax, INT32 ymin, INT32 ymax);
void BurnPaddleMakeInputs(INT32 num, INT16 x, INT16 y);
#define BurnPaddleInit BurnGunInit
#define BurnPaddleExit BurnGunExit
#define BurnPaddleScan BurnGunScan
