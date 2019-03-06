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
void BurnGunDrawTargets(); // call this after BurnTransferCopy();
INT32 BurnGunIsActive(); // is the gun system active?
void BurnGunSetCoords(INT32 player, INT32 x, INT32 y); // manually set the gun coords, for libretro's lightgun emulation

// NOTE: *depreciated* BurnPaddle is now the lowlevel code for BurnTrackball!
// Using BurnPaddle gives you 2 paddles (A & B) per player initted.
// BurnPaddleReturn[A/B] returns Velocity and directional data, see
BurnDialINF BurnPaddleReturnA(INT32 num);
BurnDialINF BurnPaddleReturnB(INT32 num);
void BurnPaddleSetWrap(INT32 num, INT32 xmin, INT32 xmax, INT32 ymin, INT32 ymax);
void BurnPaddleMakeInputs(INT32 num, INT16 x, INT16 y);
#define BurnPaddleInit BurnTrackballInit
#define BurnPaddleExit BurnGunExit
#define BurnPaddleScan BurnGunScan

// Trackball helpers (extension of BurnPaddle)
// see d_millipede.cpp or d_tempest.cpp for hook-up examples

void BurnTrackballInit(INT32 nNumPlayers);

#define AXIS_NORMAL 0
#define AXIS_REVERSED 1

// BurnTrackballFrame() is called at the beginning of a frame.  updates the analog positional status, etc.
void BurnTrackballFrame(INT32 dev, INT16 PortA, INT16 PortB, INT32 VelocityStart, INT32 VelocityMax);

// BurnTrackballUpdate() is called once per frame, sometimes more to translate the velocity to movement (Tempest uses this)
void BurnTrackballUpdate(INT32 dev);
// slither (taito/d_qix.cpp) needs a weird divider circuit on down + right signals
void BurnTrackballUpdateSlither(INT32 dev);

// BurnTrackballUDLR() can be used to load digital inputs into the trackball
void BurnTrackballUDLR(INT32 dev, INT32 u, INT32 d, INT32 l, INT32 r);

// Configure if an axis (Port) is reversed (1) or normal (0)
void BurnTrackballConfig(INT32 dev, INT32 PortA_rev, INT32 PortB_rev);

// Read the position counter (8/16bit)
UINT8 BurnTrackballRead(INT32 dev, INT32 isB);
UINT16 BurnTrackballReadWord(INT32 dev, INT32 isB);

// TODO: add configurable start/stop points

#define BurnTrackballExit BurnGunExit
#define BurnTrackballScan BurnGunScan
