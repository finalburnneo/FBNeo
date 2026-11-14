#include "burnint.h"
#include "burn_gun.h"
#include <math.h>

// Generic Light Gun & Trackball (dial, paddle, wheel, trackball) support for FBA
// Based on the code in Kev's opwolf driver
// Trackball/Paddle/Dial emulation by dink

// Uncomment for the fbn-original single-colour 17x17 reticle
//#define BURNGUN_CLASSIC_RETICLE

INT32 nBurnGunNumPlayers = 0;
bool bBurnGunHide[MAX_GUNS] = { 0, };
bool bBurnGunAutoHide = 1;
static bool bBurnGunDrawTargets = true; // game-configured
bool bBurnGunDrawReticles = true; // UI-configured

bool bBurnGunPositionalMode = false;

static INT32 Using_Trackball = 0;

static INT32 nBurnGunMaxX = 0;
static INT32 nBurnGunMaxY = 0;

INT32 BurnGunX[MAX_GUNS];
INT32 BurnGunY[MAX_GUNS];

INT32 BurnPaddleX[MAX_GUNS];
INT32 BurnPaddleY[MAX_GUNS];

struct GunWrap { INT32 xmin; INT32 xmax; INT32 ymin; INT32 ymax; };
static GunWrap BurnGunBoxInf[MAX_GUNS]; // Gun use
static INT32 BurnGunBoxRaw[MAX_GUNS][4];  // the box as given to BurnGunSetBox() (0 - 0xff), for resolution changes

#define a 0,
#define b 1,

UINT8 BurnGunTargetData[18][18] = {
	{ a a a a  a a a a  b a a a  a a a a  a a },
	{ a a a a  a a b b  b b b a  a a a a  a a },
	{ a a a a  b b a a  b a a b  b a a a  a a },
	{ a a a b  a a a a  b a a a  a b a a  a a },
	{ a a b a  a a a a  b a a a  a a b a  a a },
	{ a a b a  a a a b  b b a a  a a b a  a a },
	{ a b a a  a a b a  b a b a  a a a b  a a },
	{ a b a a  a b a a  a a a b  a a a b  a a },
	{ b b b b  b b b a  a a b b  b b b b  b a },
	{ a b a a  a b a a  a a a b  a a a b  a a },
	{ a b a a  a a b a  b a b a  a a a b  a a },
	{ a a b a  a a a b  b b a a  a a b a  a a },
	{ a a b a  a a a a  b a a a  a a b a  a a },
	{ a a a b  a a a a  b a a a  a b a a  a a },
	{ a a a a  b b a a  b a a b  b a a a  a a },
	{ a a a a  a a b b  b b b a  a a a a  a a },
	{ a a a a  a a a a  b a a a  a a a a  a a },
	{ a a a a  a a a a  a a a a  a a a a  a a },
};
#undef b
#undef a

static INT32 GunTargetHideTime; // default: 4 seconds
static INT32 GunTargetTimer[MAX_GUNS+1]  = {0, 0, 0, 0, 0}; // +1 because gcc 16 is being stupid.
static INT32 GunTargetLastX[MAX_GUNS+1]  = {0, 0, 0, 0, 0};
static INT32 GunTargetLastY[MAX_GUNS+1]  = {0, 0, 0, 0, 0};
static bool bGunTargetTimerReset = true; // set by BurnGunInit()

// reset the auto-hide timers
static void GunTargetTimerStart()
{
	if (!bGunTargetTimerReset) return;
	bGunTargetTimerReset = false;

	for (INT32 i = 0; i < nBurnGunNumPlayers; i++) {
		GunTargetTimer[i] = nCurrentFrame;
		GunTargetLastX[i] = BurnGunReturnX(i);
		GunTargetLastY[i] = BurnGunReturnY(i);
	}
}

static void GunTargetUpdate(INT32 player)
{
	if (player >= MAX_GUNS) return;

	GunTargetTimerStart();

	if (GunTargetLastX[player] != BurnGunReturnX(player) || GunTargetLastY[player] != BurnGunReturnY(player)) {
		GunTargetLastX[player] = BurnGunReturnX(player);
		GunTargetLastY[player] = BurnGunReturnY(player);
		GunTargetTimer[player] = nCurrentFrame;
	}
}

static UINT8 GunTargetShouldDraw(INT32 player)
{
	GunTargetTimerStart(); // drawn before any input was processed
	return ((INT32)nCurrentFrame < GunTargetTimer[player] + GunTargetHideTime);
}

INT32 BurnGunIsActive()
{
	return Debug_BurnGunInitted;
}

void BurnGunSetCoords(INT32 player, INT32 x, INT32 y)
{
	if (!Debug_BurnGunInitted) return; // callback for Libretro (fail nicely)

	//BurnGunX[player] = (x * nBurnGunMaxX / 0xff) << 8; // based on 0 - 255
	//BurnGunY[player] = (y * nBurnGunMaxY / 0xff) << 8;
	BurnGunX[player] = (x - 8) << 8; // based on emulated resolution
	BurnGunY[player] = (y - 8) << 8;
}

static void GunBoxCalc(INT32 num)
{
	BurnGunBoxInf[num].xmin = ((BurnGunBoxRaw[num][0] * nBurnGunMaxX / 0xff) - 8) << 8;
	BurnGunBoxInf[num].xmax = ((BurnGunBoxRaw[num][1] * nBurnGunMaxX / 0xff) - 8) << 8;
	BurnGunBoxInf[num].ymin = ((BurnGunBoxRaw[num][2] * nBurnGunMaxY / 0xff) - 8) << 8;
	BurnGunBoxInf[num].ymax = ((BurnGunBoxRaw[num][3] * nBurnGunMaxY / 0xff) - 8) << 8;
}

void BurnGunSetBox(INT32 num, INT32 xmin, INT32 xmax, INT32 ymin, INT32 ymax)
{
	if (num < 0 || num > MAX_GUNS - 1) return;

	BurnGunBoxRaw[num][0] = xmin;
	BurnGunBoxRaw[num][1] = xmax;
	BurnGunBoxRaw[num][2] = ymin;
	BurnGunBoxRaw[num][3] = ymax;

	GunBoxCalc(num);
}

// on reso. change, move the reticle(s) to the same relative spot on new screen
static void GunCheckResolution()
{
	INT32 nNewX = 0, nNewY = 0;

	if (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL) {
		BurnDrvGetVisibleSize(&nNewY, &nNewX);
	} else {
		BurnDrvGetVisibleSize(&nNewX, &nNewY);
	}

	if (nNewX <= 0 || nNewY <= 0) return;
	if (nNewX == nBurnGunMaxX && nNewY == nBurnGunMaxY) return;

	const INT32 nOldX = nBurnGunMaxX;
	const INT32 nOldY = nBurnGunMaxY;

	nBurnGunMaxX = nNewX;
	nBurnGunMaxY = nNewY;

	for (INT32 i = 0; i < MAX_GUNS; i++) {
		// BurnGunX/Y are the reticle's top-left (centre - 8) in 24.8 fixed point
		if (nOldX > 0) BurnGunX[i] = (INT32)(((INT64)(BurnGunX[i] + (8 << 8)) * nNewX) / nOldX) - (8 << 8);
		if (nOldY > 0) BurnGunY[i] = (INT32)(((INT64)(BurnGunY[i] + (8 << 8)) * nNewY) / nOldY) - (8 << 8);

		GunBoxCalc(i);

		if (BurnGunX[i] < BurnGunBoxInf[i].xmin) BurnGunX[i] = BurnGunBoxInf[i].xmin;
		if (BurnGunX[i] > BurnGunBoxInf[i].xmax) BurnGunX[i] = BurnGunBoxInf[i].xmax;
		if (BurnGunY[i] < BurnGunBoxInf[i].ymin) BurnGunY[i] = BurnGunBoxInf[i].ymin;
		if (BurnGunY[i] > BurnGunBoxInf[i].ymax) BurnGunY[i] = BurnGunBoxInf[i].ymax;
	}
}

UINT8 BurnGunReturnX(INT32 num)
{
#if defined FBNEO_DEBUG
	if (!Debug_BurnGunInitted) bprintf(PRINT_ERROR, _T("BurnGunReturnX called without init\n"));
	if (num >= nBurnGunNumPlayers) bprintf(PRINT_ERROR, _T("BurnGunReturnX called with invalid player %x\n"), num);
#endif

	if (num > MAX_GUNS - 1) return 0xff;

	float temp = (float)((BurnGunX[num] >> 8) + 8) / nBurnGunMaxX * 0xff;
	return (UINT8)temp;
}

UINT8 BurnGunReturnY(INT32 num)
{
#if defined FBNEO_DEBUG
	if (!Debug_BurnGunInitted) bprintf(PRINT_ERROR, _T("BurnGunReturnY called without init\n"));
	if (num >= nBurnGunNumPlayers) bprintf(PRINT_ERROR, _T("BurnGunReturnY called with invalid player %x\n"), num);
#endif

	if (num > MAX_GUNS - 1) return 0xff;
	
	float temp = (float)((BurnGunY[num] >> 8) + 8) / nBurnGunMaxY * 0xff;
	return (UINT8)temp;
}

// Paddle/Dial stuff
static INT32 PaddleLast[MAX_GUNS * 2];

void BurnPaddleReturn(BurnDialINF &dial, INT32 num, INT32 isB)
{
#if defined FBNEO_DEBUG
	if (!Debug_BurnGunInitted) bprintf(PRINT_ERROR, _T("BurnPaddleReturn called without init\n"));
	if (num >= nBurnGunNumPlayers) bprintf(PRINT_ERROR, _T("BurnPaddleReturn called with invalid player %x\n"), num);
#endif

	dial.Velocity = 0;
	dial.Backward = 0;
	dial.Forward  = 0;

	if (num > MAX_GUNS - 1) return;

	INT32 Paddle = ((isB) ? BurnPaddleY[num] : BurnPaddleX[num]) / 0x80;
	INT32 device = (num * 2) + isB;

	if (Paddle < PaddleLast[device]) {
		dial.Velocity = (PaddleLast[device] - Paddle);
		if (dial.Velocity > dial.VelocityMax)   dial.Velocity = dial.VelocityMax;
		if (dial.Velocity < dial.VelocityStart) dial.Velocity = dial.VelocityStart;

		dial.Backward = 1;
	}
	else if (Paddle > PaddleLast[device]) {
		dial.Velocity = (Paddle - PaddleLast[device]);
		if (dial.Velocity > dial.VelocityMax)   dial.Velocity = dial.VelocityMax;
		if (dial.Velocity < dial.VelocityStart) dial.Velocity = dial.VelocityStart;

		dial.Forward = 1;
	}

	PaddleLast[device] = Paddle;
}

// Trackball Helpers
static INT32 TrackA[MAX_GUNS]; // trackball counters / main accumulator
static INT32 TrackA_Prev[MAX_GUNS]; // value from previous frame
static INT32 TrackB[MAX_GUNS];
static INT32 TrackB_Prev[MAX_GUNS];
static INT32 TrackDefault; // default value to load w/ ReadReset() (usually 0)

static UINT8 CURVE[0x100];
static INT32 bLogarithmicCurve;

static INT32 DIAL_INC[MAX_GUNS * 2]; // velocity accumulator latch (w/linear / log curve applied)
static INT32 DIAL_VEL[MAX_GUNS * 2]; // velocity accumulator
static INT32 DIAL_VELx[MAX_GUNS * 2]; // velocity accumulator half'd
static UINT8 DrvJoyT[MAX_GUNS * 4];  // direction bytes
static UINT8 TrackRev[MAX_GUNS * 2]; // normal/reversed config
static INT32 TrackStart[MAX_GUNS * 2]; // Start / Stop points
static INT32 TrackStop[MAX_GUNS * 2];
static INT32 UDLRSpeed[MAX_GUNS];

static INT32 Max_Scanlines; // for interpolated reading

void BurnTrackballFrame(INT32 dev, INT32 PortA, INT32 PortB, INT32 VelocityStart, INT32 VelocityMax, INT32 MaxScanlines)
{
	BurnDialINF dial = { VelocityStart, VelocityMax, (VelocityStart + VelocityMax) / 2, 0, 0, 0 };

	DIAL_INC[(dev*2) + 0] = (UDLRSpeed[dev]) ? UDLRSpeed[dev] : dial.VelocityMidpoint;		// defaults for digital (UDLR)
	DIAL_INC[(dev*2) + 1] = (UDLRSpeed[dev]) ? UDLRSpeed[dev] : dial.VelocityMidpoint;

	DIAL_VEL[(dev*2) + 0] = 0; // testing!
	DIAL_VEL[(dev*2) + 1] = 0;
	DIAL_VELx[(dev*2) + 0] = 0; // testing!
	DIAL_VELx[(dev*2) + 1] = 0;

	Max_Scanlines = MaxScanlines;

	memset(&DrvJoyT[dev*4], 0, 4); 						// zero directional bytes

	BurnPaddleMakeInputs(dev, dial, AnalogDeadZone(PortA), AnalogDeadZone(PortB)); // analog -> main accumulators

	BurnPaddleReturn(dial, dev, 0);                     // accumulator -> directional + velocity data PortA
	if (dial.Backward || dial.Forward) {
		if (dial.Backward) DrvJoyT[(dev*4) + 0] = 1;
		if (dial.Forward)  DrvJoyT[(dev*4) + 1] = 1;
		DIAL_INC[(dev*2) + 0] = CURVE[dial.Velocity];
		DIAL_VEL[(dev*2) + 0] = dial.Velocity*5;
		DIAL_VELx[(dev*2) + 0] = (dial.Velocity*5) / 2;
		//bprintf(0, _T("VELO A: %d\n"), CURVE[dial.Velocity]);
	}

	BurnPaddleReturn(dial, dev, 1);                     // accumulator -> directional + velocity data PortB
	if (dial.Backward || dial.Forward) {
		if (dial.Backward) DrvJoyT[(dev*4) + 2] = 1;
		if (dial.Forward)  DrvJoyT[(dev*4) + 3] = 1;
		DIAL_INC[(dev*2) + 1] = CURVE[dial.Velocity];
		DIAL_VEL[(dev*2) + 1] = dial.Velocity*5;
		DIAL_VELx[(dev*2) + 1] = (dial.Velocity*5) / 2;
		//bprintf(0, _T("VELO B: %d\n"), CURVE[dial.Velocity]);
	}

	if (Max_Scanlines > 0) {
		TrackA_Prev[dev] = TrackA[dev];
		TrackB_Prev[dev] = TrackB[dev];
	}
}

// Theory of bLogarithmicCurve-mode:
// For the first half of the velocity accumulator (aka DIAL_VEL) sequence
// the main accumulator (aka Track[A/B]) is inc'd every call to BurnTrackballUpdate().
// After which the main accu. is inc'd every second call.
// The logarithmic curve is only applied to the velocity of the main accumulator's
// latch (aka DIAL_INC).
// Usage: "BurnTrackballSetVelocityCurve(1);" after Init

static INT32 attenuate_velocity(INT32 dev)
{
	if (bLogarithmicCurve) {
		if (DIAL_VEL[dev] >= DIAL_VELx[dev]) {
			return DIAL_INC[dev];
		}

		return (DIAL_VEL[dev] & 1) ? DIAL_INC[dev] : 0;
	}

	return DIAL_INC[dev];
}

void BurnTrackballUpdate(INT32 dev)
{
	BurnTrackballUpdatePortA(dev);
	BurnTrackballUpdatePortB(dev);
}

void BurnTrackballUpdatePortA(INT32 dev)
{
	// PortA (usually X-Axis)
	if (DrvJoyT[(dev*4) + 0]) { // Backward
		if (TrackRev[(dev*2) + 0])
			TrackA[dev] += attenuate_velocity((dev*2) + 0);
		else
			TrackA[dev] -= attenuate_velocity((dev*2) + 0);
	}
	if (DrvJoyT[(dev*4) + 1]) { // Forward
		if (TrackRev[(dev*2) + 0])
			TrackA[dev] -= attenuate_velocity((dev*2) + 0);
		else
			TrackA[dev] += attenuate_velocity((dev*2) + 0);
	}

	// PortA Start / Stop points (if configured)
	if (TrackStart[(dev*2) + 0] != -1 && TrackA[dev] < TrackStart[(dev*2) + 0])
		TrackA[dev] = TrackStart[(dev*2) + 0];

	if (TrackStop[(dev*2) + 0] != -1 && TrackA[dev] > TrackStop[(dev*2) + 0])
		TrackA[dev] = TrackStop[(dev*2) + 0];

	if (bLogarithmicCurve) {
		if (DIAL_VEL[(dev*2) + 0]) {
			DIAL_VEL[(dev*2) + 0]--;
		} else {
			DIAL_INC[(dev*2) + 0] = 0;
		}
	}
}

void BurnTrackballUpdatePortB(INT32 dev)
{
	// PortB (usually Y-Axis)
	if (DrvJoyT[(dev*4) + 2]) { // Backward
		if (TrackRev[(dev*2) + 1])
			TrackB[dev] += attenuate_velocity((dev*2) + 1);
		else
			TrackB[dev] -= attenuate_velocity((dev*2) + 1);
	}
	if (DrvJoyT[(dev*4) + 3]) { // Forward
		if (TrackRev[(dev*2) + 1])
			TrackB[dev] -= attenuate_velocity((dev*2) + 1);
		else
			TrackB[dev] += attenuate_velocity((dev*2) + 1);
	}

	// PortB Start / Stop points (if configured)
	if (TrackStart[(dev*2) + 1] != -1 && TrackB[dev] < TrackStart[(dev*2) + 1])
		TrackB[dev] = TrackStart[(dev*2) + 1];

	if (TrackStop[(dev*2) + 1] != -1 && TrackB[dev] > TrackStop[(dev*2) + 1])
		TrackB[dev] = TrackStop[(dev*2) + 1];

	if (bLogarithmicCurve) {
		if (DIAL_VEL[(dev*2) + 1]) {
			DIAL_VEL[(dev*2) + 1]--;
		} else {
			DIAL_INC[(dev*2) + 1] = 0;
		}
	}
}

void BurnTrackballUpdateSlither(INT32 dev)
{
	// simulate the divider circuit on down + right(V) for Slither (taito/d_qix.cpp)
	static INT32 flippy[2] = { 0, 0 };
	// PortA (usually X-Axis)
	if (DrvJoyT[(dev*4) + 0]) { // Backward
		flippy[0] ^= 1;
		if (flippy[0]) return;
		if (TrackRev[(dev*2) + 0])
			TrackA[dev] += DIAL_INC[(dev*2) + 0];
		else
			TrackA[dev] -= DIAL_INC[(dev*2) + 0];
	}
	if (DrvJoyT[(dev*4) + 1]) { // Forward
		if (TrackRev[(dev*2) + 0])
			TrackA[dev] -= DIAL_INC[(dev*2) + 0];
		else
			TrackA[dev] += DIAL_INC[(dev*2) + 0];
	}
	// PortB (usually Y-Axis)
	if (DrvJoyT[(dev*4) + 2]) { // Backward
		if (TrackRev[(dev*2) + 1])
			TrackB[dev] += DIAL_INC[(dev*2) + 1];
		else
			TrackB[dev] -= DIAL_INC[(dev*2) + 1];
	}
	if (DrvJoyT[(dev*4) + 3]) { // Forward
		flippy[1] ^= 1;
		if (flippy[1]) return;
		if (TrackRev[(dev*2) + 1])
			TrackB[dev] -= DIAL_INC[(dev*2) + 1];
		else
			TrackB[dev] += DIAL_INC[(dev*2) + 1];
	}
}

// returns current dial.Forward, dial.Backward, dial.Velocity
void BurnPaddleGetDial(BurnDialINF &dial, INT32 num, INT32 isB)
{
	if (num > MAX_GUNS - 1) return;

	dial.Velocity = DIAL_VEL[(num*2) + ((isB) ? 1 : 0)];

	if (TrackRev[(num*2) + ((isB) ? 1 : 0)]) { // reversed!
		dial.Backward = DrvJoyT[(num*4) + ((isB) ? 2 : 0) + 1];
		dial.Forward  = DrvJoyT[(num*4) + ((isB) ? 2 : 0) + 0];
	} else {
		dial.Backward = DrvJoyT[(num*4) + ((isB) ? 2 : 0) + 0];
		dial.Forward  = DrvJoyT[(num*4) + ((isB) ? 2 : 0) + 1];
	}
}

INT32 BurnTrackballGetDirection(INT32 num, INT32 isB)
{
	if (num > MAX_GUNS - 1) return 0;

	BurnDialINF dial;
	BurnPaddleGetDial(dial, num, isB);

	if (dial.Backward) return -1;
	if (dial.Forward) return +1;

	return 0;
}

INT32 BurnTrackballGetDirection(INT32 dev)
{
	return BurnTrackballGetDirection(dev >> 1, dev & 1);
}

INT32 BurnTrackballGetVelocity(INT32 num, INT32 isB)
{
	if (num > MAX_GUNS - 1) return 0;

	return DIAL_INC[(num*2) + ((isB) ? 1 : 0)];
}

INT32 BurnTrackballGetVelocity(INT32 dev)
{
	return BurnTrackballGetVelocity(dev >> 1, dev & 1);
}

UINT8 BurnTrackballRead(INT32 dev) // linear device #
{
	return BurnTrackballRead(dev >> 1, dev & 1);
}

UINT8 BurnTrackballRead(INT32 dev, INT32 isB) // 2 axis per device #
{
	if (isB)
		return TrackB[dev] & 0xff;
	else
		return TrackA[dev] & 0xff;
}

UINT16 BurnTrackballReadWord(INT32 dev)
{
	return BurnTrackballReadWord(dev >> 1, dev & 1);
}

UINT16 BurnTrackballReadWord(INT32 dev, INT32 isB)
{
	if (isB)
		return TrackB[dev] & 0xffff;
	else
		return TrackA[dev] & 0xffff;
}

INT32 BurnTrackballReadSigned(INT32 dev)
{
	return BurnTrackballReadSigned(dev >> 1, dev & 1);
}

INT32 BurnTrackballReadSigned(INT32 dev, INT32 isB)
{
	if (isB)
		return TrackB[dev];
	else
		return TrackA[dev];
}

UINT8 BurnTrackballReadInterpolated(INT32 dev, INT32 scanline) // linear device #
{
	return BurnTrackballReadInterpolated(dev >> 1, dev & 1, scanline);
}

UINT8 BurnTrackballReadInterpolated(INT32 dev, INT32 isB, INT32 scanline) // 2 axis per device #
{
	if (Max_Scanlines == -1) {
		bprintf(0, _T("BurnTrackballReadInterpolated(): Max_Scanlines not set!\n"));
	}

	INT32 now = 0;
	INT32 prev = 0;

	if (isB) {
		now = TrackB[dev];
		prev = TrackB_Prev[dev];
	} else {
		now = TrackA[dev];
		prev = TrackA_Prev[dev];
	}

	return (prev + ((now - prev) * scanline / (Max_Scanlines-1))) & 0xff;
}

void BurnTrackballReadReset(INT32 dev)
{
	BurnTrackballReadReset(dev >> 1, dev & 1);
}

void BurnTrackballReadReset(INT32 dev, INT32 isB)
{
	if (isB)
		TrackB[dev] = TrackDefault;
	else
		TrackA[dev] = TrackDefault;
}

void BurnTrackballReadReset()
{
	for (INT32 i = 0; i < MAX_GUNS; i++) {
		BurnTrackballReadReset(i, 0);
		BurnTrackballReadReset(i, 1);
	}
}

void BurnTrackballSetResetDefault(INT32 nDefault)
{
	TrackDefault = nDefault;
}

void BurnTrackballUDLR(INT32 dev, INT32 u, INT32 d, INT32 l, INT32 r, INT32 speed)
{
	DrvJoyT[(dev*4) + 0] |= l;
	DrvJoyT[(dev*4) + 1] |= r;
	DrvJoyT[(dev*4) + 2] |= u;
	DrvJoyT[(dev*4) + 3] |= d;

	UDLRSpeed[dev] = speed;
}

void BurnTrackballConfig(INT32 dev, INT32 PortA_rev, INT32 PortB_rev)
{
	TrackRev[(dev*2) + 0] = PortA_rev;
	TrackRev[(dev*2) + 1] = PortB_rev;
}

void BurnTrackballConfigStartStopPoints(INT32 dev, INT32 PortA_Start, INT32 PortA_Stop, INT32 PortB_Start, INT32 PortB_Stop)
{
	TrackStart[(dev*2) + 0] = PortA_Start;
	TrackStart[(dev*2) + 1] = PortB_Start;
	TrackStop[(dev*2) + 0] = PortA_Stop;
	TrackStop[(dev*2) + 1] = PortB_Stop;
}

// end Trackball Helpers

void BurnPaddleMakeInputs(INT32 num, BurnDialINF &dial, INT32 x, INT32 y)
{
#if defined FBNEO_DEBUG
	if (!Debug_BurnGunInitted) bprintf(PRINT_ERROR, _T("BurnGunMakeInputs called without init\n"));
	if (num >= nBurnGunNumPlayers) bprintf(PRINT_ERROR, _T("BurnGunMakeInputs called with invalid player %x\n"), num);
#endif

	if (num > MAX_GUNS - 1) return;
	
	if (y == 1 || y == -1) y = 0;
	if (x == 1 || x == -1) x = 0; // prevent walking crosshair

	BurnPaddleX[num] += x;
	BurnPaddleY[num] += y;
}

void BurnGunMakeInputs(INT32 num, INT16 x, INT16 y)
{
#if defined FBNEO_DEBUG
	if (!Debug_BurnGunInitted) bprintf(PRINT_ERROR, _T("BurnGunMakeInputs called without init\n"));
	if (num >= nBurnGunNumPlayers) bprintf(PRINT_ERROR, _T("BurnGunMakeInputs called with invalid player %x\n"), num);
#endif

	if (num > MAX_GUNS - 1) return;

	if (bBurnRunAheadFrame) return; // remove jitter w/runahead

	if (!Using_Trackball) GunCheckResolution();

	if (bBurnGunPositionalMode) {
		x = ProcessAnalog(x, 0, INPUT_DEADZONE, 0x00, 0xff);
		y = ProcessAnalog(y, 0, INPUT_DEADZONE, 0x00, 0xff);

		BurnGunX[num] = ((x * nBurnGunMaxX / 0xff) - 8) << 8;
		BurnGunY[num] = ((y * nBurnGunMaxY / 0xff) - 8) << 8;

		for (INT32 i = 0; i < nBurnGunNumPlayers; i++)
			GunTargetUpdate(i);

		return;
	}

	if (y == 1 || y == -1) y = 0;
	if (x == 1 || x == -1) x = 0; // prevent walking crosshair

	BurnGunX[num] += x;
	BurnGunY[num] += y;

	if (BurnGunX[num] < BurnGunBoxInf[num].xmin) BurnGunX[num] = BurnGunBoxInf[num].xmin;
	if (BurnGunX[num] > BurnGunBoxInf[num].xmax) BurnGunX[num] = BurnGunBoxInf[num].xmax;
	if (BurnGunY[num] < BurnGunBoxInf[num].ymin) BurnGunY[num] = BurnGunBoxInf[num].ymin;
	if (BurnGunY[num] > BurnGunBoxInf[num].ymax) BurnGunY[num] = BurnGunBoxInf[num].ymax;

	for (INT32 i = 0; i < nBurnGunNumPlayers; i++)
		GunTargetUpdate(i);
}

void BurnTrackballSetVelocityCurve(INT32 bLogarithmic)
{
	bLogarithmicCurve = bLogarithmic;

	if (bLogarithmic) {
		for (INT32 i = 0; i < 0x100; i++) {
			CURVE[i] = 1 + (log((double)i) * 1.2);
		}
	} else {
		for (INT32 i = 0; i < 0x100; i++) {
			CURVE[i] = i;
		}
	}
}

void BurnGunSetHideTime(INT32 nHideTimeInFrames)
{
	GunTargetHideTime = nHideTimeInFrames;
}

void BurnTrackballInit(INT32 nNumPlayers)
{
	Using_Trackball = 1;

	BurnTrackballSetVelocityCurve(0);

	BurnGunInit(nNumPlayers, false);

	// When using trackball device, we set the mouse axis deltas to a more
	// usable rate. (when mouse is mapped to the tb input)
	BurnSetMouseDivider(10);
}

void BurnTrackballInit(INT32 nNumPlayers, INT32 nDefault)
{
	BurnTrackballInit(nNumPlayers);

	BurnTrackballSetResetDefault(nDefault);
}

// stagger multi-player gun reticles for a nice startup look
static void GunStartPosition(INT32 num)
{
	INT32 nGuns = nBurnGunNumPlayers;
	if (nGuns < 1 || num >= nGuns) nGuns = 1; // unused guns: centre
	const INT32 nSlot = (nGuns == 1) ? 0 : num;

	// fraction of the way across the displayed screen: (slot + 1) / (guns + 1)
	INT32 cx = nBurnGunMaxX / 2;
	INT32 cy = nBurnGunMaxY / 2;

	const UINT32 nFlags = BurnDrvGetFlags();

	if (nFlags & BDF_ORIENTATION_VERTICAL) {
		// the screen is rotated: displayed left->right runs along the framebuffer's
		// y axis (downwards for ROT270, upwards when also flipped (ROT90))
		INT32 p = (nBurnGunMaxY * (nSlot + 1)) / (nGuns + 1);
		cy = (nFlags & BDF_ORIENTATION_FLIPPED) ? (nBurnGunMaxY - 1 - p) : p;
	} else {
		cx = (nBurnGunMaxX * (nSlot + 1)) / (nGuns + 1);
	}

	BurnGunX[num] = (cx - 8) << 8; // BurnGunX/Y are the reticle's top-left (centre - 8)
	BurnGunY[num] = (cy - 8) << 8;

	if (BurnGunX[num] < BurnGunBoxInf[num].xmin) BurnGunX[num] = BurnGunBoxInf[num].xmin;
	if (BurnGunX[num] > BurnGunBoxInf[num].xmax) BurnGunX[num] = BurnGunBoxInf[num].xmax;
	if (BurnGunY[num] < BurnGunBoxInf[num].ymin) BurnGunY[num] = BurnGunBoxInf[num].ymin;
	if (BurnGunY[num] > BurnGunBoxInf[num].ymax) BurnGunY[num] = BurnGunBoxInf[num].ymax;
}

void BurnGunInit(INT32 nNumPlayers, bool bDrawTargets)
{
	Debug_BurnGunInitted = 1;

	if (nNumPlayers > MAX_GUNS) nNumPlayers = MAX_GUNS;
	nBurnGunNumPlayers = nNumPlayers;
	bBurnGunDrawTargets = bDrawTargets;
	
	if (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL) {
		BurnDrvGetVisibleSize(&nBurnGunMaxY, &nBurnGunMaxX);
	} else {
		BurnDrvGetVisibleSize(&nBurnGunMaxX, &nBurnGunMaxY);
	}
	
	for (INT32 i = 0; i < MAX_GUNS; i++) {
		BurnGunSetBox(i, 0, 0xff, 0, 0xff); // Gun stuff
		GunStartPosition(i);
	}

	GunTargetHideTime = (nBurnFPS / 100) * 4; // 4 seconds

	// forget the previous game's timers / positions
	memset(GunTargetTimer, 0, sizeof(GunTargetTimer));
	memset(GunTargetLastX, 0, sizeof(GunTargetLastX));
	memset(GunTargetLastY, 0, sizeof(GunTargetLastY));
	bGunTargetTimerReset = true;

	// Trackball stuff (init)
	memset(&TrackA, 0, sizeof(TrackA));
	memset(&TrackA_Prev, 0, sizeof(TrackA_Prev));
	memset(&TrackB, 0, sizeof(TrackB));
	memset(&TrackB_Prev, 0, sizeof(TrackB_Prev));
	memset(&DrvJoyT, 0, sizeof(DrvJoyT));
	memset(&DIAL_INC, 0, sizeof(DIAL_INC));
	memset(&TrackRev, 0, sizeof(TrackRev));
	memset(&UDLRSpeed, 0, sizeof(UDLRSpeed));
	memset(&PaddleLast, 0, sizeof(PaddleLast));
	memset(&BurnPaddleX, 0, sizeof(BurnPaddleX));
	memset(&BurnPaddleY, 0, sizeof(BurnPaddleY));

	for (INT32 i = 0; i < MAX_GUNS*2; i++) {
		TrackStart[i] = -1;
		TrackStop[i]  = -1;
	}

	TrackDefault = 0;
}

void BurnGunResolutionChanged()
{
	GunCheckResolution();
}

void BurnGunExit()
{
#if defined FBNEO_DEBUG
	if (!Debug_BurnGunInitted) bprintf(PRINT_ERROR, _T("BurnGunExit called without init\n"));
#endif

	nBurnGunNumPlayers = 0;
	bBurnGunDrawTargets = true;
	
	nBurnGunMaxX = 0;
	nBurnGunMaxY = 0;
	
	for (INT32 i = 0; i < MAX_GUNS; i++) {
		BurnGunX[i] = 0;
		BurnGunY[i] = 0;
	}

	Using_Trackball = 0;
	Debug_BurnGunInitted = 0;
}

void BurnGunScan()
{
#if defined FBNEO_DEBUG
	if (!Debug_BurnGunInitted) bprintf(PRINT_ERROR, _T("BurnGunScan called without init\n"));
#endif

	if (Using_Trackball) {
		SCAN_VAR(BurnPaddleX);
		SCAN_VAR(BurnPaddleY);
		SCAN_VAR(TrackA);
		SCAN_VAR(TrackB);
		SCAN_VAR(TrackA_Prev);
		SCAN_VAR(TrackB_Prev);

		SCAN_VAR(PaddleLast);

		SCAN_VAR(DIAL_INC);
		SCAN_VAR(DIAL_VEL);
		SCAN_VAR(DIAL_VELx);
		SCAN_VAR(DrvJoyT);
	} else {
		// guns only!
		SCAN_VAR(BurnGunX);
		SCAN_VAR(BurnGunY);
		SCAN_VAR(GunTargetTimer);
		SCAN_VAR(GunTargetLastX);
		SCAN_VAR(GunTargetLastY);
	}
}

// Reticle drawing

#define GUN_RETICLE_REF_LINES	240.0f	// retical is sized to 17px @ 256x240 lines

// pixels covered by one "reference pixel" of the reticle, per axis
static void GunReticleGetScale(float &fScaleX, float &fScaleY)
{
	INT32 nAspX = 0, nAspY = 0;

	if (nBurnGunMaxX <= 0 || nBurnGunMaxY <= 0) {
		fScaleX = fScaleY = 1.0f;
		return;
	}

	if (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL) {
		BurnDrvGetAspect(&nAspY, &nAspX);
	} else {
		BurnDrvGetAspect(&nAspX, &nAspY);
	}

	if (nAspX <= 0 || nAspY <= 0) { // unknown, assume square pixels
		nAspX = nBurnGunMaxX;
		nAspY = nBurnGunMaxY;
	}

	fScaleY = (float)nBurnGunMaxY / GUN_RETICLE_REF_LINES;
	fScaleX = ((float)nBurnGunMaxX * nAspY) / ((float)nAspX * GUN_RETICLE_REF_LINES);

	if (fScaleX < 0.5f) fScaleX = 0.5f;
	if (fScaleY < 0.5f) fScaleY = 0.5f;
	if (fScaleX > 16.0f) fScaleX = 16.0f;
	if (fScaleY > 16.0f) fScaleY = 16.0f;
}

static inline UINT8* GunPixAddr(INT32 x, INT32 y)
{
	return pBurnDraw + (y * nBurnPitch) + (x * nBurnBpp);
}

static inline UINT32 GunGetPix(UINT8* pPix)
{
	if (nBurnBpp >= 4) return *((UINT32*)pPix);
	if (nBurnBpp == 2) return *((UINT16*)pPix);
	return pPix[0] | (pPix[1] << 8) | (pPix[2] << 16);
}

// Deprecated: the reticle now sizes itself from the game's resolution
// (kept so drivers calling it still build)
void BurnGunSetTargetScale(INT32 /*nScale*/)
{
}

#if defined BURNGUN_CLASSIC_RETICLE

// Original reticle: 17x17 bitmap, one solid colour per player, scaled by
// whole pixels so its lines stay crisp
static void GunDrawReticle(INT32 num, INT32 x, INT32 y)
{
	static const UINT8 TargetRGB[MAX_GUNS][3] = {
		{ 0xfc, 0x12, 0xee }, { 0x1c, 0xfc, 0x1c }, { 0x15, 0x93, 0xfd }, { 0xf7, 0xfa, 0x0e }
	};

	const UINT32 nTargetCol = BurnHighCol(TargetRGB[num][0], TargetRGB[num][1], TargetRGB[num][2], 0);

	float fScaleX, fScaleY;
	GunReticleGetScale(fScaleX, fScaleY);

	INT32 sx = (INT32)(fScaleX + 0.5f); if (sx < 1) sx = 1;
	INT32 sy = (INT32)(fScaleY + 0.5f); if (sy < 1) sy = 1;

	// keep the centre pixel (8, 8) of the bitmap at (x + 8, y + 8)
	const INT32 x0 = x + 8 - 8 * sx - (sx - 1) / 2;
	const INT32 y0 = y + 8 - 8 * sy - (sy - 1) / 2;

	for (INT32 py = 0; py < 17 * sy; py++) {
		const INT32 dy = y0 + py;
		if (dy < 0 || dy > nBurnGunMaxY - 1) continue;

		for (INT32 px = 0; px < 17 * sx; px++) {
			const INT32 dx = x0 + px;
			if (dx < 0 || dx > nBurnGunMaxX - 1) continue;

			if (BurnGunTargetData[py / sy][px / sx]) {
				PutPix(GunPixAddr(dx, dy), nTargetCol);
			}
		}
	}
}

#else

// animated reticle
#define RETICLE_PI			3.14159265f
#define RETICLE_RING_R		6.75f	// ring radius (reference pixels)
#define RETICLE_RING_HT		0.75f	// ring half thickness
#define RETICLE_ARC_FILL	0.66f	// portion of each arc segment that's drawn
#define RETICLE_TICK_HW		0.5f	// crosshair half width (1 pixel line at 1x)
#define RETICLE_TICK_OUT	9.5f	// crosshair outer end
#define RETICLE_OUTLINE		0.8f	// black outline width (at least 1 pixel)
#define RETICLE_OUTLINE_A	0.9f	// black outline opacity
#define RETICLE_EXTENT		(RETICLE_TICK_OUT + RETICLE_OUTLINE + 3.0f)

// animation speeds (per 60hz frame)
#define RETICLE_SPIN_SPEED	0.028f	// ring rotation, turns per frame (~0.6 sec per turn)
#define RETICLE_PULSE_SPEED	0.28f	// crosshair / dot pulse, radians per frame (~0.37 sec period)
#define RETICLE_COLOR_SPEED	0.21f	// colour sweep around the ring, radians per frame

// framebuffer format, worked out from BurnHighCol(), so we can blend with
// whatever the game drew (15/16/24/32-bit, any channel order)
struct GunChannel { UINT32 nMask; INT32 nShift; UINT32 nMax; };
static GunChannel GunChan[3];
static bool bGunCanBlend = false;

static bool GunProbeChannel(GunChannel &c, UINT32 nMask)
{
	c.nMask = nMask;
	c.nShift = 0;
	c.nMax = 0;

	if (nMask == 0) return false;

	while (((nMask >> c.nShift) & 1) == 0) c.nShift++;
	c.nMax = nMask >> c.nShift;

	return (c.nMax & (c.nMax + 1)) == 0; // contiguous bits only
}

static void GunProbePixelFormat()
{
	bGunCanBlend  = GunProbeChannel(GunChan[0], BurnHighCol(0xff, 0, 0, 0));
	bGunCanBlend &= GunProbeChannel(GunChan[1], BurnHighCol(0, 0xff, 0, 0));
	bGunCanBlend &= GunProbeChannel(GunChan[2], BurnHighCol(0, 0, 0xff, 0));
}

static inline float GunUnpack(UINT32 nPix, INT32 c)
{
	return (float)((nPix & GunChan[c].nMask) >> GunChan[c].nShift) * 255.0f / GunChan[c].nMax;
}

static inline float GunClamp01(float f)
{
	return (f < 0.0f) ? 0.0f : ((f > 1.0f) ? 1.0f : f);
}

static void GunHSV(float h, float s, float v, float *rgb)
{
	h = h - floorf(h / 360.0f) * 360.0f;
	const float c = v * s;
	const float hh = h / 60.0f;
	const float x = c * (1.0f - fabsf(fmodf(hh, 2.0f) - 1.0f));
	float r = 0, g = 0, b = 0;

	switch ((INT32)hh) {
		case 0:  r = c; g = x; break;
		case 1:  r = x; g = c; break;
		case 2:  g = c; b = x; break;
		case 3:  g = x; b = c; break;
		case 4:  r = x; b = c; break;
		default: r = c; b = x; break;
	}

	const float m = v - c;
	rgb[0] = (r + m) * 255.0f;
	rgb[1] = (g + m) * 255.0f;
	rgb[2] = (b + m) * 255.0f;
}

static void GunDrawReticle(INT32 num, INT32 x, INT32 y)
{
	static const float PlayerHue[MAX_GUNS] = { 212.0f, 120.0f, 305.0f, 58.0f }; // blue, green, magenta, yellow
	// minimum perceived brightness of the ring colours (0..1), so dark hues
	// (deep blue) are lifted to match the others. 0 = leave as-is
	static const float PlayerMinLuma[MAX_GUNS] = { 0.62f, 0.0f, 0.0f, 0.0f };

	GunProbePixelFormat();

	float fScaleX, fScaleY;
	GunReticleGetScale(fScaleX, fScaleY);

	// fade out over the last 1/8th of the auto-hide time
	float fFade = 1.0f;
	if (bBurnGunAutoHide && GunTargetHideTime > 0) {
		INT32 nFadeLen = GunTargetHideTime / 8;
		if (nFadeLen < 1) nFadeLen = 1;
		const INT32 nLeft = GunTargetTimer[num] + GunTargetHideTime - (INT32)nCurrentFrame;
		if (nLeft < nFadeLen) fFade = (float)nLeft / nFadeLen;
	}
	if (fFade <= 0.0f) return;

	// animation clock, in 60hz frames (independent of the game's refresh rate)
	const float t = (nBurnFPS > 0) ? ((float)nCurrentFrame * 6000.0f / nBurnFPS) : (float)nCurrentFrame;

	const INT32 nArcs   = (num & 1) ? 3 : 4;
	const float fDir    = (num & 1) ? -1.0f : 1.0f;
	const float fRot    = fDir * t * RETICLE_SPIN_SPEED;             // ring rotation, in turns
	const float fBreath = 0.5f + 0.5f * sinf(t * RETICLE_PULSE_SPEED); // 0..1
	const float fTickIn = 2.6f + 0.9f * fBreath;                     // crosshair inner end
	const float fTickMid  = (fTickIn + RETICLE_TICK_OUT) * 0.5f;
	const float fTickHalf = (RETICLE_TICK_OUT - fTickIn) * 0.5f;
	const float fDotR   = 0.75f + 0.35f * fBreath;
	const float fArcLen = 2.0f * RETICLE_PI * RETICLE_RING_R / nArcs;
	const float fHue    = PlayerHue[num];

	float TickRGB[3], DotRGB[3];
	GunHSV(fHue, 0.65f, 1.0f, TickRGB);
	GunHSV(fHue, 0.15f + 0.25f * (1.0f - fBreath), 1.0f, DotRGB);

	// one framebuffer pixel, in reference pixels (anti-aliasing width)
	const float fPix = 2.0f / (fScaleX + fScaleY);

	// in low-res games the reticle is drawn smaller than 1:1: keep every line
	// at least one full pixel wide (thinner lines get blended with the
	// background and look pale), and the outline at least one pixel
	const float fPixMax  = 1.0f / ((fScaleX < fScaleY) ? fScaleX : fScaleY);
	const float fRingHT  = (RETICLE_RING_HT > 0.75f * fPixMax) ? RETICLE_RING_HT : 0.75f * fPixMax;
	const float fTickHW  = (RETICLE_TICK_HW > 0.5f * fPixMax) ? RETICLE_TICK_HW : 0.5f * fPixMax;
	const float fOutline = (RETICLE_OUTLINE > fPixMax) ? RETICLE_OUTLINE : fPixMax;

	// centre of the old 17x17 reticle's middle pixel
	const float cx = (float)(x + 8) + 0.5f;
	const float cy = (float)(y + 8) + 0.5f;

	INT32 x0 = (INT32)floorf(cx - RETICLE_EXTENT * fScaleX), x1 = (INT32)ceilf(cx + RETICLE_EXTENT * fScaleX);
	INT32 y0 = (INT32)floorf(cy - RETICLE_EXTENT * fScaleY), y1 = (INT32)ceilf(cy + RETICLE_EXTENT * fScaleY);
	if (x0 < 0) x0 = 0;
	if (y0 < 0) y0 = 0;
	if (x1 > nBurnGunMaxX - 1) x1 = nBurnGunMaxX - 1;
	if (y1 > nBurnGunMaxY - 1) y1 = nBurnGunMaxY - 1;

	for (INT32 py = y0; py <= y1; py++) {
		const float dy = ((float)py + 0.5f - cy) / fScaleY;
		const float ay = fabsf(dy);

		for (INT32 px = x0; px <= x1; px++) {
			const float dx = ((float)px + 0.5f - cx) / fScaleX;
			const float ax = fabsf(dx);
			const float r = sqrtf(dx * dx + dy * dy);

			if (r > RETICLE_EXTENT) continue;

			// segmented, rotating ring
			const float th = atan2f(dy, dx);
			float fPhase = th / (2.0f * RETICLE_PI) * nArcs + fRot;
			fPhase -= floorf(fPhase);
			float dArc;
			if (fPhase < RETICLE_ARC_FILL) {
				const float e = (fPhase < RETICLE_ARC_FILL - fPhase) ? fPhase : (RETICLE_ARC_FILL - fPhase);
				dArc = -e * fArcLen;
			} else {
				const float e = ((fPhase - RETICLE_ARC_FILL) < (1.0f - fPhase)) ? (fPhase - RETICLE_ARC_FILL) : (1.0f - fPhase);
				dArc = e * fArcLen;
			}
			float dRing = fabsf(r - RETICLE_RING_R) - fRingHT;
			if (dArc > dRing) dRing = dArc;

			// crosshair
			float dH = fabsf(ax - fTickMid) - fTickHalf; if (ay - fTickHW > dH) dH = ay - fTickHW;
			float dV = fabsf(ay - fTickMid) - fTickHalf; if (ax - fTickHW > dV) dV = ax - fTickHW;
			const float dTick = (dH < dV) ? dH : dV;

			// centre dot
			const float dDot = r - fDotR;

			float d = dRing;
			INT32 nPart = 0;
			if (dTick < d) { d = dTick; nPart = 1; }
			if (dDot  < d) { d = dDot;  nPart = 2; }

			if (d > fOutline + fPix) continue;

			const float aFill   = GunClamp01(0.5f - d / fPix) * fFade;
			const float aShadow = GunClamp01(0.5f - (d - fOutline) / fPix) * RETICLE_OUTLINE_A * fFade;

			float Col[3];
			if (nPart == 0) { // colour sweeps around the ring and cycles over time
				GunHSV(fHue + 45.0f * sinf(th - t * RETICLE_COLOR_SPEED), 0.85f, 1.0f, Col);

				// too dark? mix towards white until it's bright enough
				const float fLuma = (0.299f * Col[0] + 0.587f * Col[1] + 0.114f * Col[2]) / 255.0f;
				if (fLuma < PlayerMinLuma[num]) {
					const float k = (PlayerMinLuma[num] - fLuma) / (1.0f - fLuma);
					for (INT32 c = 0; c < 3; c++) Col[c] += (255.0f - Col[c]) * k;
				}
			} else if (nPart == 1) {
				Col[0] = TickRGB[0]; Col[1] = TickRGB[1]; Col[2] = TickRGB[2];
			} else {
				Col[0] = DotRGB[0]; Col[1] = DotRGB[1]; Col[2] = DotRGB[2];
			}

			UINT8* pPix = GunPixAddr(px, py);

			if (bGunCanBlend) {
				const UINT32 nBg = GunGetPix(pPix);
				float Out[3];
				for (INT32 c = 0; c < 3; c++) {
					const float bg = GunUnpack(nBg, c) * (1.0f - aShadow);
					Out[c] = bg + (Col[c] - bg) * aFill;
				}
				PutPix(pPix, BurnHighCol((INT32)(Out[0] + 0.5f), (INT32)(Out[1] + 0.5f), (INT32)(Out[2] + 0.5f), 0));
			} else { // unknown pixel format: solid pixels only
				if (aFill >= 0.5f) {
					PutPix(pPix, BurnHighCol((INT32)Col[0], (INT32)Col[1], (INT32)Col[2], 0));
				} else if (aShadow >= 0.3f) {
					PutPix(pPix, BurnHighCol(0, 0, 0, 0));
				}
			}
		}
	}
}

#endif

void BurnGunDrawTarget(INT32 num, INT32 x, INT32 y)
{
#if defined FBNEO_DEBUG
	if (!Debug_BurnGunInitted) bprintf(PRINT_ERROR, _T("BurnGunDrawTarget called without init\n"));
	if (num >= nBurnGunNumPlayers) bprintf(PRINT_ERROR, _T("BurnGunDrawTarget called with invalid player %x\n"), num);
#endif

	if (bBurnGunDrawTargets == false) return; // game-configured setting
	if (bBurnGunDrawReticles == false) return; // UI-configured setting
	if (pBurnDraw == NULL) return; // nothing to draw to

	if (num < 0 || num > MAX_GUNS - 1) return;

	GunCheckResolution(); // never draw with a stale resolution (outside the screen)

	if (bBurnGunHide[num] || (bBurnGunAutoHide && !GunTargetShouldDraw(num))) return;

	GunDrawReticle(num, x, y);
}

void BurnGunDrawTargets()
{
	for (INT32 i = 0; i < nBurnGunNumPlayers; i++) {
		BurnGunDrawTarget(i, BurnGunX[i] >> 8, BurnGunY[i] >> 8);
	}
}


// Gun recoil

#define RECOIL_MAX_PLAYERS	4

struct RecoilStep { UINT16 nStrong; UINT16 nWeak; };

struct RecoilProfileDef {
	const RecoilStep* pEnv;
	INT32 nLen;				// frames
	INT32 nAutoRate;		// while the recoil line is held: frames between shots (0 = one shot per edge)
	INT32 nMinRetrigger;	// a new shot can't restart the envelope before this many frames (it's queued)
};

static const RecoilStep RecoilEnvHandgun[] = {
	{ 0xffff, 0xffff }, { 0xb000, 0x6000 }, { 0x5000, 0x1800 }, { 0x1800, 0x0000 }
};
static const RecoilStep RecoilEnvMagnum[] = {
	{ 0xffff, 0xffff }, { 0xffff, 0xc000 }, { 0xb000, 0x4000 }, { 0x7000, 0x1000 }, { 0x3800, 0x0000 }, { 0x1400, 0x0000 }
};
static const RecoilStep RecoilEnvShotgun[] = {
	{ 0xffff, 0xffff }, { 0xffff, 0xffff }, { 0xffff, 0xa000 }, { 0xe000, 0x5000 }, { 0xb000, 0x2400 },
	{ 0x8800, 0x0c00 }, { 0x6000, 0x0000 }, { 0x4000, 0x0000 }, { 0x2400, 0x0000 }, { 0x1000, 0x0000 }
};
static const RecoilStep RecoilEnvRifle[] = {
	{ 0xffff, 0xffff }, { 0xd000, 0x7000 }, { 0x8000, 0x2000 }, { 0x4400, 0x0000 }, { 0x1800, 0x0000 }
};
static const RecoilStep RecoilEnvMachinegun[] = {
	{ 0xe000, 0xffff }, { 0x9000, 0x7000 }, { 0x5000, 0x2000 }
};
static const RecoilStep RecoilEnvLaser[] = {
	{ 0x4000, 0xffff }, { 0x2000, 0xc000 }, { 0x0800, 0x8000 }, { 0x0000, 0x4000 }, { 0x0000, 0x1800 }
};
static const RecoilStep RecoilEnvRocket[] = {
	{ 0xffff, 0x8000 }, { 0xffff, 0x4000 }, { 0xffff, 0x2000 }, { 0xe000, 0x1000 }, { 0xc000, 0x0000 }, { 0xa000, 0x0000 },
	{ 0x8000, 0x0000 }, { 0x6000, 0x0000 }, { 0x4800, 0x0000 }, { 0x3000, 0x0000 }, { 0x1c00, 0x0000 }, { 0x0c00, 0x0000 }
};

#define RECOIL_ENV(x)	x, (INT32)(sizeof(x) / sizeof(x[0]))

static const RecoilProfileDef RecoilProfiles[RECOIL_PROFILE_COUNT] = {
	{ RECOIL_ENV(RecoilEnvHandgun),    0, 2 },	// RECOIL_HANDGUN
	{ RECOIL_ENV(RecoilEnvMagnum),     0, 3 },	// RECOIL_MAGNUM
	{ RECOIL_ENV(RecoilEnvShotgun),    0, 4 },	// RECOIL_SHOTGUN
	{ RECOIL_ENV(RecoilEnvRifle),      0, 2 },	// RECOIL_RIFLE
	{ RECOIL_ENV(RecoilEnvMachinegun), 3, 3 },	// RECOIL_MACHINEGUN
	{ RECOIL_ENV(RecoilEnvLaser),      0, 2 },	// RECOIL_LASER
	{ RECOIL_ENV(RecoilEnvRocket),     0, 6 },	// RECOIL_ROCKET
};

struct RecoilVoice {
	INT32 nProfile;			// -1 = idle
	INT32 nPos;				// frame position in the envelope
	INT32 nScale;			// per-shot strength variation, 0..0x10000
};

struct RecoilPlayer {
	INT32 nProfile;			// this player's gun
	INT32 nLine;			// current state of the recoil output line
	INT32 nHeld;			// frames the line has been held
	INT32 nPending;			// queued shot: profile + 1, 0 = none
	RecoilVoice Voice[2];	// current shot + the tail of the previous one (they're mixed)
};

static RecoilPlayer RecoilState[RECOIL_MAX_PLAYERS];
static INT32 nRecoilNumPlayers = 0;
static UINT32 nRecoilRand = 0;	// deterministic (save-stated) variation
static bool bRecoilInitted = false;

static INT32 RecoilValidPlayer(INT32 nPlayer)
{
	return bRecoilInitted && nPlayer >= 0 && nPlayer < nRecoilNumPlayers;
}

static INT32 RecoilValidProfile(INT32 nProfile)
{
	return nProfile >= 0 && nProfile < RECOIL_PROFILE_COUNT;
}

void BurnRecoilReset()
{
	if (!bRecoilInitted) return;

	for (INT32 i = 0; i < RECOIL_MAX_PLAYERS; i++) {
		RecoilState[i].nLine = 0;
		RecoilState[i].nHeld = 0;
		RecoilState[i].nPending = 0;
		RecoilState[i].Voice[0].nProfile = RecoilState[i].Voice[1].nProfile = -1;
		RecoilState[i].Voice[0].nPos = RecoilState[i].Voice[1].nPos = 0;
	}
	nRecoilRand = 0x2545f491;
}

void BurnRecoilInit(INT32 nNumPlayers, INT32 nDefaultProfile)
{
	if (nNumPlayers > RECOIL_MAX_PLAYERS) nNumPlayers = RECOIL_MAX_PLAYERS;
	if (nNumPlayers < 1) nNumPlayers = 1;
	if (!RecoilValidProfile(nDefaultProfile)) nDefaultProfile = RECOIL_HANDGUN;

	memset(RecoilState, 0, sizeof(RecoilState));
	nRecoilNumPlayers = nNumPlayers;
	bRecoilInitted = true;

	for (INT32 i = 0; i < RECOIL_MAX_PLAYERS; i++) {
		RecoilState[i].nProfile = nDefaultProfile;
	}

	BurnRecoilReset();
}

void BurnRecoilSetProfile(INT32 nPlayer, INT32 nProfile)
{
	if (!RecoilValidPlayer(nPlayer) || !RecoilValidProfile(nProfile)) return;

	RecoilState[nPlayer].nProfile = nProfile;
}

void BurnRecoilExit()
{
	if (!bRecoilInitted) return;

	for (INT32 i = 0; i < nRecoilNumPlayers; i++) {
		BurnRumble(i, 0, 0);
	}

	memset(RecoilState, 0, sizeof(RecoilState));
	nRecoilNumPlayers = 0;
	bRecoilInitted = false;
}

void BurnRecoilScan(INT32 nAction)
{
	if (!bRecoilInitted) return;

	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(RecoilState);
		SCAN_VAR(nRecoilRand);
	}
}

void BurnRecoilFireEx(INT32 nPlayer, INT32 nProfile)
{
	if (!RecoilValidPlayer(nPlayer) || !RecoilValidProfile(nProfile)) return;

	RecoilState[nPlayer].nPending = nProfile + 1;	// max. one queued shot
}

void BurnRecoilFire(INT32 nPlayer)
{
	if (!RecoilValidPlayer(nPlayer)) return;

	BurnRecoilFireEx(nPlayer, RecoilState[nPlayer].nProfile);
}

void BurnRecoilLine(INT32 nPlayer, INT32 nState)
{
	if (!RecoilValidPlayer(nPlayer)) return;

	RecoilPlayer* p = &RecoilState[nPlayer];
	nState = nState ? 1 : 0;

	if (nState && !p->nLine) {			// rising edge: fire (caught even if the pulse is shorter than a frame)
		BurnRecoilFire(nPlayer);
		p->nHeld = 0;
	}

	p->nLine = nState;
}

static void RecoilStartShot(RecoilPlayer* p, INT32 nProfile)
{
	p->Voice[1] = p->Voice[0];			// the previous shot keeps decaying underneath

	nRecoilRand = nRecoilRand * 1103515245 + 12345;
	p->Voice[0].nProfile = nProfile;
	p->Voice[0].nPos = 0;
	p->Voice[0].nScale = 0xe800 + ((nRecoilRand >> 16) % 0x1801);	// ~91% .. 100%
}

void BurnRecoilUpdate()
{
	if (!bRecoilInitted) return;

	for (INT32 i = 0; i < nRecoilNumPlayers; i++) {
		RecoilPlayer* p = &RecoilState[i];
		const RecoilProfileDef* pDef = &RecoilProfiles[p->nProfile];

		// automatic fire while the line is held (machine guns)
		if (p->nLine) {
			if (p->nHeld > 0 && pDef->nAutoRate && (p->nHeld % pDef->nAutoRate) == 0) {
				p->nPending = p->nProfile + 1;
			}
			p->nHeld++;
		} else {
			p->nHeld = 0;
		}

		// start a queued shot, once the current one has had its minimum time
		if (p->nPending) {
			RecoilVoice* v = &p->Voice[0];
			if (v->nProfile < 0 || v->nPos >= RecoilProfiles[v->nProfile].nMinRetrigger) {
				RecoilStartShot(p, p->nPending - 1);
				p->nPending = 0;
			}
		}

		// mix the voices (max) and advance them
		INT32 nStrong = 0, nWeak = 0;

		for (INT32 j = 0; j < 2; j++) {
			RecoilVoice* v = &p->Voice[j];
			if (v->nProfile < 0) continue;

			const RecoilProfileDef* pVoiceDef = &RecoilProfiles[v->nProfile];
			if (v->nPos >= pVoiceDef->nLen) {
				v->nProfile = -1;
				continue;
			}

			INT32 s = (INT32)(((UINT32)pVoiceDef->pEnv[v->nPos].nStrong * (UINT32)v->nScale) >> 16);
			INT32 w = (INT32)(((UINT32)pVoiceDef->pEnv[v->nPos].nWeak * (UINT32)v->nScale) >> 16);
			if (s > nStrong) nStrong = s;
			if (w > nWeak) nWeak = w;

			v->nPos++;
		}

		BurnRumble(i, nStrong, nWeak);
	}
}
