#include "burnint.h"
#include "burn_gun.h"

// Generic Light Gun & Trackball (dial, paddle, wheel, trackball) support for FBA
// written by Barry Harris (Treble Winner) based on the code in Kev's opwolf driver
// Trackball/Paddle/Dial emulation by dink

INT32 nBurnGunNumPlayers = 0;
bool bBurnGunAutoHide = 1;
static bool bBurnGunDrawTargets = true;

static INT32 Using_Trackball = 0;

static INT32 nBurnGunMaxX = 0;
static INT32 nBurnGunMaxY = 0;

INT32 BurnGunX[MAX_GUNS];
INT32 BurnGunY[MAX_GUNS];

struct GunWrap { INT32 xmin; INT32 xmax; INT32 ymin; INT32 ymax; };
static GunWrap BurnGunWrapInf[MAX_GUNS]; // Paddle/Dial use

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

#define GunTargetHideTime (60 * 4) /* 4 seconds @ 60 fps */
static INT32 GunTargetTimer[MAX_GUNS]  = {0, 0, 0, 0};
static INT32 GunTargetLastX[MAX_GUNS]  = {0, 0, 0, 0};
static INT32 GunTargetLastY[MAX_GUNS]  = {0, 0, 0, 0};

static void GunTargetUpdate(INT32 player)
{
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
	return (Debug_BurnGunInitted && Using_Trackball == 0);
}

void BurnGunSetCoords(INT32 player, INT32 x, INT32 y)
{
	if (!Debug_BurnGunInitted) return; // callback for Libretro (fail nicely)

	//BurnGunX[player] = (x * nBurnGunMaxX / 0xff) << 8; // based on 0 - 255
	//BurnGunY[player] = (y * nBurnGunMaxY / 0xff) << 8;
	BurnGunX[player] = (x - 8) << 8; // based on emulated resolution
	BurnGunY[player] = (y - 8) << 8;
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

	INT32 Paddle = ((isB) ? BurnGunY[num] : BurnGunX[num]) >> 7;
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
static INT32 TrackA[MAX_GUNS]; // trackball counters
static INT32 TrackB[MAX_GUNS];

static INT32 DIAL_INC[MAX_GUNS * 2]; // velocity counter
static UINT8 DrvJoyT[MAX_GUNS * 4];  // direction bytes
static UINT8 TrackRev[MAX_GUNS * 2]; // normal/reversed config
static INT32 TrackStart[MAX_GUNS * 2]; // Start / Stop points
static INT32 TrackStop[MAX_GUNS * 2];

void BurnTrackballFrame(INT32 dev, INT16 PortA, INT16 PortB, INT32 VelocityStart, INT32 VelocityMax)
{
	BurnDialINF dial = { VelocityStart, VelocityMax, (VelocityStart + VelocityMax) / 2, 0, 0, 0 };

	DIAL_INC[(dev*2) + 0] = dial.VelocityMidpoint;		// defaults for digital (UDLR)
	DIAL_INC[(dev*2) + 1] = dial.VelocityMidpoint;

	memset(&DrvJoyT[dev*4], 0, 4); 						// zero directional bytes

	BurnPaddleMakeInputs(dev, dial, AnalogDeadZone(PortA), AnalogDeadZone(PortB)); // analog -> main accumulators

	BurnPaddleReturn(dial, dev, 0);                     // accumulator -> directional + velocity data PortA
	if (dial.Backward || dial.Forward) {
		if (dial.Backward) DrvJoyT[(dev*4) + 0] = 1;
		if (dial.Forward)  DrvJoyT[(dev*4) + 1] = 1;
		DIAL_INC[(dev*2) + 0] = dial.Velocity;
	}

	BurnPaddleReturn(dial, dev, 1);                     // accumulator -> directional + velocity data PortB
	if (dial.Backward || dial.Forward) {
		if (dial.Backward) DrvJoyT[(dev*4) + 2] = 1;
		if (dial.Forward)  DrvJoyT[(dev*4) + 3] = 1;
		DIAL_INC[(dev*2) + 1] = dial.Velocity;
	}
}

void BurnTrackballUpdate(INT32 dev)
{
	// PortA (usually X-Axis)
	if (DrvJoyT[(dev*4) + 0]) { // Backward
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

	// PortA Start / Stop points (if configured)
	if (TrackStart[(dev*2) + 0] != -1 && TrackA[dev] < TrackStart[(dev*2) + 0])
		TrackA[dev] = TrackStart[(dev*2) + 0];

	if (TrackStop[(dev*2) + 0] != -1 && TrackA[dev] > TrackStop[(dev*2) + 0])
		TrackA[dev] = TrackStop[(dev*2) + 0];

	// PortB (usually Y-Axis)
	if (DrvJoyT[(dev*4) + 2]) { // Backward
		if (TrackRev[(dev*2) + 1])
			TrackB[dev] += DIAL_INC[(dev*2) + 1];
		else
			TrackB[dev] -= DIAL_INC[(dev*2) + 1];
	}
	if (DrvJoyT[(dev*4) + 3]) { // Forward
		if (TrackRev[(dev*2) + 1])
			TrackB[dev] -= DIAL_INC[(dev*2) + 1];
		else
			TrackB[dev] += DIAL_INC[(dev*2) + 1];
	}

	// PortB Start / Stop points (if configured)
	if (TrackStart[(dev*2) + 1] != -1 && TrackB[dev] < TrackStart[(dev*2) + 1])
		TrackB[dev] = TrackStart[(dev*2) + 1];

	if (TrackStop[(dev*2) + 1] != -1 && TrackB[dev] > TrackStop[(dev*2) + 1])
		TrackB[dev] = TrackStop[(dev*2) + 1];
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

UINT8 BurnTrackballRead(INT32 dev, INT32 isB)
{
	if (isB)
		return TrackB[dev] & 0xff;
	else
		return TrackA[dev] & 0xff;
}

UINT16 BurnTrackballReadWord(INT32 dev, INT32 isB)
{
	if (isB)
		return TrackB[dev] & 0xffff;
	else
		return TrackA[dev] & 0xffff;
}

void BurnTrackballUDLR(INT32 dev, INT32 u, INT32 d, INT32 l, INT32 r)
{
	DrvJoyT[(dev*4) + 0] |= l;
	DrvJoyT[(dev*4) + 1] |= r;
	DrvJoyT[(dev*4) + 2] |= u;
	DrvJoyT[(dev*4) + 3] |= d;
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

void BurnPaddleSetWrap(INT32 num, INT32 xmin, INT32 xmax, INT32 ymin, INT32 ymax)
{
	BurnGunWrapInf[num].xmin = xmin * 0x10; BurnGunWrapInf[num].xmax = xmax * 0x10;
	BurnGunWrapInf[num].ymin = ymin * 0x10; BurnGunWrapInf[num].ymax = ymax * 0x10;
}

void BurnPaddleMakeInputs(INT32 num, BurnDialINF &dial, INT16 x, INT16 y)
{
#if defined FBNEO_DEBUG
	if (!Debug_BurnGunInitted) bprintf(PRINT_ERROR, _T("BurnGunMakeInputs called without init\n"));
	if (num >= nBurnGunNumPlayers) bprintf(PRINT_ERROR, _T("BurnGunMakeInputs called with invalid player %x\n"), num);
#endif

	if (num > MAX_GUNS - 1) return;
	
	if (y == 1 || y == -1) y = 0;
	if (x == 1 || x == -1) x = 0; // prevent walking crosshair

	BurnGunX[num] += x;
	BurnGunY[num] += y;

	// Wrapping (for dial/paddle use)
	if (BurnGunWrapInf[num].xmin != -1)
		if (BurnGunX[num] < BurnGunWrapInf[num].xmin * 0x100) {
			BurnGunX[num] = BurnGunWrapInf[num].xmax * 0x100;
			BurnPaddleReturn(dial, num, 0); // rebase PaddleLast* on wrap
		}
	if (BurnGunWrapInf[num].xmax != -1)
		if (BurnGunX[num] > BurnGunWrapInf[num].xmax * 0x100) {
			BurnGunX[num] = BurnGunWrapInf[num].xmin * 0x100;
			BurnPaddleReturn(dial, num, 0); // rebase PaddleLast* on wrap
		}

	if (BurnGunWrapInf[num].ymin != -1)
		if (BurnGunY[num] < BurnGunWrapInf[num].ymin * 0x100) {
			BurnGunY[num] = BurnGunWrapInf[num].ymax * 0x100;
			BurnPaddleReturn(dial, num, 1); // rebase PaddleLast* on wrap
		}
	if (BurnGunWrapInf[num].ymax != -1)
		if (BurnGunY[num] > BurnGunWrapInf[num].ymax * 0x100) {
			BurnGunY[num] = BurnGunWrapInf[num].ymin * 0x100;
			BurnPaddleReturn(dial, num, 1); // rebase PaddleLast* on wrap
		}
}

void BurnGunMakeInputs(INT32 num, INT16 x, INT16 y)
{
#if defined FBNEO_DEBUG
	if (!Debug_BurnGunInitted) bprintf(PRINT_ERROR, _T("BurnGunMakeInputs called without init\n"));
	if (num >= nBurnGunNumPlayers) bprintf(PRINT_ERROR, _T("BurnGunMakeInputs called with invalid player %x\n"), num);
#endif

	if (num > MAX_GUNS - 1) return;
	
	const INT32 MinX = -8 * 0x100;
	const INT32 MinY = -8 * 0x100;

	if (y == 1 || y == -1) y = 0;
	if (x == 1 || x == -1) x = 0; // prevent walking crosshair

	BurnGunX[num] += x;
	BurnGunY[num] += y;

	if (BurnGunX[num] < MinX) BurnGunX[num] = MinX;
	if (BurnGunX[num] > MinX + nBurnGunMaxX * 0x100) BurnGunX[num] = MinX + nBurnGunMaxX * 0x100;
	if (BurnGunY[num] < MinY) BurnGunY[num] = MinY;
	if (BurnGunY[num] > MinY + nBurnGunMaxY * 0x100) BurnGunY[num] = MinY + nBurnGunMaxY * 0x100;

	for (INT32 i = 0; i < nBurnGunNumPlayers; i++)
		GunTargetUpdate(i);
}

void BurnTrackballInit(INT32 nNumPlayers)
{
	Using_Trackball = 1;
	BurnGunInit(nNumPlayers, false);
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

		BurnPaddleSetWrap(i, 0, 0xf0, 0, 0xf0); // Paddle/dial stuff
	}

	// Trackball stuff (init)
	memset(&TrackA, 0, sizeof(TrackA));
	memset(&TrackB, 0, sizeof(TrackB));
	memset(&DrvJoyT, 0, sizeof(DrvJoyT));
	memset(&DIAL_INC, 0, sizeof(DIAL_INC));
	memset(&TrackRev, 0, sizeof(TrackRev));
	for (INT32 i = 0; i < MAX_GUNS*2; i++) {
		TrackStart[i] = -1;
		TrackStop[i]  = -1;
	}
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

	SCAN_VAR(BurnGunX);
	SCAN_VAR(BurnGunY);

	if (Using_Trackball) {
		SCAN_VAR(TrackA);
		SCAN_VAR(TrackB);

		SCAN_VAR(PaddleLast);

		SCAN_VAR(DIAL_INC);
		SCAN_VAR(DrvJoyT);
	}
}

void BurnGunDrawTarget(INT32 num, INT32 x, INT32 y)
{
#if defined FBNEO_DEBUG
	if (!Debug_BurnGunInitted) bprintf(PRINT_ERROR, _T("BurnGunDrawTarget called without init\n"));
	if (num >= nBurnGunNumPlayers) bprintf(PRINT_ERROR, _T("BurnGunDrawTarget called with invalid player %x\n"), num);
#endif

	if (bBurnGunDrawTargets == false) return;
	
	if (num > MAX_GUNS - 1) return;

	if (bBurnGunAutoHide && !GunTargetShouldDraw(num)) return;

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
