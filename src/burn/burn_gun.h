#define MAX_GUNS	4

struct BurnDialINF {
	INT32 VelocityStart;
	INT32 VelocityMax;
	INT32 VelocityMidpoint;
	INT32 Velocity;
	INT32 Backward;
	INT32 Forward;
};

extern INT32 nBurnGunNumPlayers;
extern bool bBurnGunHide[MAX_GUNS];
extern bool bBurnGunAutoHide;

extern INT32 BurnGunX[MAX_GUNS];
extern INT32 BurnGunY[MAX_GUNS];

UINT8 BurnGunReturnX(INT32 num);
UINT8 BurnGunReturnY(INT32 num);

extern void BurnGunInit(INT32 nNumPlayers, bool bDrawTargets);
void BurnGunExit();
void BurnGunScan();
void BurnGunResolutionChanged();
extern void BurnGunDrawTarget(INT32 num, INT32 x, INT32 y);
void BurnGunSetTargetScale(INT32 nScale); // draw the targets nScale times larger (a driver drawing at nScale x its resolution)
extern void BurnGunMakeInputs(INT32 num, INT16 x, INT16 y);
void BurnGunDrawTargets(); // call this after BurnTransferCopy();
INT32 BurnGunIsActive(); // is the gun system active?
void BurnGunSetBox(INT32 num, INT32 xmin, INT32 xmax, INT32 ymin, INT32 ymax);
void BurnGunSetCoords(INT32 player, INT32 x, INT32 y); // manually set the gun coords, for libretro's lightgun emulation
void BurnGunSetHideTime(INT32 nHideTimeInFrames); // when the inactive reticle disappears

// NOTE: *depreciated* BurnPaddle is now the lowlevel code for BurnTrackball!
// Using BurnPaddle gives you 2 paddles (A & B) per player initted.
// BurnPaddleReturn[A/B] returns Velocity and directional data.
void BurnPaddleReturn(BurnDialINF &dial, INT32 num, INT32 isB);
void BurnPaddleGetDial(BurnDialINF &dial, INT32 num, INT32 isB);
void BurnPaddleSetWrap(INT32 num, INT32 xmin, INT32 xmax, INT32 ymin, INT32 ymax);
void BurnPaddleMakeInputs(INT32 num, BurnDialINF &dial, INT32 x, INT32 y);
#define BurnPaddleInit BurnTrackballInit
#define BurnPaddleExit BurnGunExit
#define BurnPaddleScan BurnGunScan

// Trackball Device (extension of BurnPaddle)
// see d_millipede.cpp, d_cabal or d_tempest.cpp for hook-up examples

void BurnTrackballInit(INT32 nNumPlayers);
void BurnTrackballInit(INT32 nNumPlayers, INT32 nDefault);

#define AXIS_NORMAL 0
#define AXIS_REVERSED 1

// BurnTrackballFrame() is called at the beginning of a frame.  updates the analog positional status, etc.
void BurnTrackballFrame(INT32 dev, INT32 PortA, INT32 PortB, INT32 VelocityStart, INT32 VelocityMax, INT32 MaxScanlines = -1);

// BurnTrackballUpdate() is called once per frame, sometimes more to translate the velocity to movement (Tempest uses this)
void BurnTrackballUpdate(INT32 dev);
void BurnTrackballUpdatePortA(INT32 dev);
void BurnTrackballUpdatePortB(INT32 dev);
// slither (taito/d_qix.cpp) has a divider circuit on down + right signals
void BurnTrackballUpdateSlither(INT32 dev);

// BurnTrackballUDLR() can be used to load digital inputs into the trackball
void BurnTrackballUDLR(INT32 dev, INT32 u, INT32 d, INT32 l, INT32 r, INT32 speed = 0);

// Configure if an axis (Port) is reversed (1) or normal (0)
void BurnTrackballConfig(INT32 dev, INT32 PortA_rev, INT32 PortB_rev);
void BurnTrackballConfigStartStopPoints(INT32 dev, INT32 PortA_Start, INT32 PortA_Stop, INT32 PortB_Start, INT32 PortB_Stop);
void BurnTrackballSetVelocityCurve(INT32 bLogarithmic); // mostly for itech32

// Read the position counter
UINT8 BurnTrackballRead(INT32 dev, INT32 isB); // 2 axis per device #
UINT8 BurnTrackballRead(INT32 dev);            // linear device #
UINT16 BurnTrackballReadWord(INT32 dev, INT32 isB);
UINT16 BurnTrackballReadWord(INT32 dev);
INT32 BurnTrackballReadSigned(INT32 dev, INT32 isB);
INT32 BurnTrackballReadSigned(INT32 dev);

// Read scanline-interpolated value (
UINT8 BurnTrackballReadInterpolated(INT32 dev, INT32 scanline);
UINT8 BurnTrackballReadInterpolated(INT32 dev, INT32 isB, INT32 scanline);

INT32 BurnTrackballGetDirection(INT32 num, INT32 isB); // -1 backwards (left, down), +1 forward (right, up), 0 idle
INT32 BurnTrackballGetDirection(INT32 dev);
INT32 BurnTrackballGetVelocity(INT32 num, INT32 isB);
INT32 BurnTrackballGetVelocity(INT32 dev);

// Reset the position counters
void BurnTrackballReadReset(); // all devices
void BurnTrackballReadReset(INT32 num, INT32 isB);
void BurnTrackballReadReset(INT32 dev);
void BurnTrackballSetResetDefault(INT32 nDefault); // default is 0

#define BurnTrackballExit BurnGunExit
#define BurnTrackballScan BurnGunScan

// ---------------------------------------------------------------------------
// Gun recoil (force feedback through BurnRumble)
//
// Usage:
//   Init:   BurnRecoilInit(2, RECOIL_MACHINEGUN);     (players 0..3, default profile for all)
//           BurnRecoilSetProfile(1, RECOIL_SHOTGUN);   (optional, per player)
//   Reset:  BurnRecoilReset();
//   Fire:   BurnRecoilLine(player, state)   - feed a recoil solenoid / piston output line from
//                                             the hardware, at write time. A shot fires on the
//                                             rising edge, and machine guns keep cycling at the
//                                             profile's rate while the line is held.
//           BurnRecoilFire(player)          - fire one shot of the player's profile
//           BurnRecoilFireEx(player, prof)  - fire one shot of a given profile (grenade, etc)
//   Frame:  BurnRecoilUpdate();             - once per frame, at the end of the frame function
//   Scan:   BurnRecoilScan(nAction);        - in the driver's scan function
//   Exit:   BurnRecoilExit();
// All functions are safe to call when BurnRecoilInit() wasn't called (they do nothing),
// so shared driver code can call Update / Scan / Reset / Exit unconditionally.

enum BurnRecoilProfile {
	RECOIL_HANDGUN = 0,		// 9mm pistol: sharp, short snap
	RECOIL_MAGNUM,			// heavy revolver: big kick, short tail
	RECOIL_SHOTGUN,			// huge shove, long rolling tail
	RECOIL_RIFLE,			// crisp crack with a medium thump
	RECOIL_MACHINEGUN,		// Uzi / SMG / MG: short punches, cycles ~20 shots/sec while held
	RECOIL_LASER,			// sci-fi blaster: high-frequency zap, little thump
	RECOIL_ROCKET,			// rocket / grenade launcher: heavy launch thump and long rumble
	RECOIL_PROFILE_COUNT
};

void BurnRecoilInit(INT32 nNumPlayers, INT32 nDefaultProfile);
void BurnRecoilSetProfile(INT32 nPlayer, INT32 nProfile);
void BurnRecoilExit();
void BurnRecoilReset();
void BurnRecoilScan(INT32 nAction);
void BurnRecoilLine(INT32 nPlayer, INT32 nState);
void BurnRecoilFire(INT32 nPlayer);
void BurnRecoilFireEx(INT32 nPlayer, INT32 nProfile);
void BurnRecoilUpdate();
