#include "burnint.h"
#include "burn_gun.h"
#include <math.h>

// Generic Light Gun & Trackball (dial, paddle, wheel, trackball) support for FBA
// Based on the code in Kev's opwolf driver
// Trackball/Paddle/Dial emulation by dink

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
static INT32 GunTargetTimer[MAX_GUNS]  = {0, 0, 0, 0};
static INT32 GunTargetLastX[MAX_GUNS]  = {0, 0, 0, 0};
static INT32 GunTargetLastY[MAX_GUNS]  = {0, 0, 0, 0};

static void GunTargetUpdate(INT32 player)
{
	if (player >= MAX_GUNS) return;

	if (GunTargetLastX[player] != BurnGunReturnX(player) || GunTargetLastY[player] != BurnGunReturnY(player)) {
		GunTargetLastX[player] = BurnGunReturnX(player);
		GunTargetLastY[player] = BurnGunReturnY(player);
		GunTargetTimer[player] = nCurrentFrame;
	}
}

static UINT8 GunTargetShouldDraw(INT32 player)
{
	return ((INT32)nCurrentFrame < GunTargetTimer[player] + GunTargetHideTime);
}
#undef GunTargetHideTime

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

void BurnGunSetBox(INT32 num, INT32 xmin, INT32 xmax, INT32 ymin, INT32 ymax)
{
	BurnGunBoxInf[num].xmin = ((xmin * nBurnGunMaxX / 0xff) - 8) << 8;
	BurnGunBoxInf[num].xmax = ((xmax * nBurnGunMaxX / 0xff) - 8) << 8;
	BurnGunBoxInf[num].ymin = ((ymin * nBurnGunMaxY / 0xff) - 8) << 8;
	BurnGunBoxInf[num].ymax = ((ymax * nBurnGunMaxY / 0xff) - 8) << 8;
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
		BurnGunX[i] = ((nBurnGunMaxX >> 1) - 7) << 8;
		BurnGunY[i] = ((nBurnGunMaxY >> 1) - 8) << 8;

		BurnGunSetBox(i, 0, 0xff, 0, 0xff); // Gun stuff
	}

	GunTargetHideTime = (nBurnFPS / 100) * 4; // 4 seconds

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

void BurnGunDrawTarget(INT32 num, INT32 x, INT32 y)
{
#if defined FBNEO_DEBUG
	if (!Debug_BurnGunInitted) bprintf(PRINT_ERROR, _T("BurnGunDrawTarget called without init\n"));
	if (num >= nBurnGunNumPlayers) bprintf(PRINT_ERROR, _T("BurnGunDrawTarget called with invalid player %x\n"), num);
#endif

	if (bBurnGunDrawTargets == false) return; // game-configured setting
	if (bBurnGunDrawReticles == false) return; // UI-configured setting

	if (num > MAX_GUNS - 1) return;

	if (bBurnGunHide[num] || (bBurnGunAutoHide && !GunTargetShouldDraw(num))) return;

	UINT8* pTile = pBurnDraw + nBurnGunMaxX * nBurnBpp * (y - 1) + nBurnBpp * x;
	
	UINT32 nTargetCol = 0;
	if (num == 0) nTargetCol = BurnHighCol(0xfc, 0x12, 0xee, 0);
	if (num == 1) nTargetCol = BurnHighCol(0x1c, 0xfc, 0x1c, 0);
	if (num == 2) nTargetCol = BurnHighCol(0x15, 0x93, 0xfd, 0);
	if (num == 3) nTargetCol = BurnHighCol(0xf7, 0xfa, 0x0e, 0);

	for (INT32 y2 = 0; y2 < 17; y2++) {

		pTile += nBurnGunMaxX * nBurnBpp;

		if ((y + y2) < 0 || (y + y2) > nBurnGunMaxY - 1) {
			continue;
		}

		for (INT32 x2 = 0; x2 < 17; x2++) {

			if ((x + x2) < 0 || (x + x2) > nBurnGunMaxX - 1) {
				continue;
			}

			if (BurnGunTargetData[y2][x2]) {
				if (nBurnBpp == 2) {
					((UINT16*)pTile)[x2] = (UINT16)nTargetCol;
				} else {
					((UINT32*)pTile)[x2] = nTargetCol;
				}
			}
		}
	}
}

void BurnGunDrawTargets()
{
	for (INT32 i = 0; i < nBurnGunNumPlayers; i++) {
		BurnGunDrawTarget(i, BurnGunX[i] >> 8, BurnGunY[i] >> 8);
	}
}
