// Headless video output for FinalBurn Neo.
// Author: Leonardo Kasperavičius
//
// Adapted from the macOS video interface by Akop Karapetyan
//   https://github.com/0xe1f/FinalBurn-X  (Apache License 2.0)
//
// Built originally to support OpenEmu-Silicon
// (https://github.com/OpenEmu-Silicon/OpenEmu-Silicon).
//
// FBNeo renders every game in its native orientation (vertical games come out
// sideways), so this keeps a native render buffer for the emulator and presents
// an upright, correctly-pitched buffer to the host, rotating when needed.
// Subject to the FB Neo license (see src/license.txt).

#include "burner.h"
#include "fbheadless.h"

static unsigned char *nativeBuffer = NULL;  // FBNeo renders here (pBurnDraw)
static unsigned char *displayBuffer = NULL; // exposed to the host (upright)
static int nativeW = 0, nativeH = 0;
static int dispW = 0, dispH = 0;
static int bpp = 0;
static int rotate = 0; // 0 none, 1 CW90, 2 180, 3 CCW90

static void freeBuffers()
{
	if (displayBuffer && displayBuffer != nativeBuffer)
		free(displayBuffer);
	free(nativeBuffer);
	nativeBuffer = NULL;
	displayBuffer = NULL;
}

static int HeadlessVideoInit()
{
	int vw = 0, vh = 0;
	int flags = BurnDrvGetFlags();
	BurnDrvGetVisibleSize(&vw, &vh);

	// 32bpp XRGB8888 output so the host renderer can consume it directly.
	nVidImageDepth = 32;
	nVidImageBPP = 4;
	bpp = 4;
	SetBurnHighCol(nVidImageDepth);

	if (flags & BDF_ORIENTATION_VERTICAL) {
		// Visible size is the upright (display) size; FBNeo renders it sideways.
		nativeW = vh; nativeH = vw;
		dispW = vw;   dispH = vh;
		rotate = (flags & BDF_ORIENTATION_FLIPPED) ? 1 : 3; // CW : CCW
	} else {
		nativeW = vw; nativeH = vh;
		dispW = vw;   dispH = vh;
		rotate = (flags & BDF_ORIENTATION_FLIPPED) ? 2 : 0; // 180 : none
	}

	freeBuffers();
	nativeBuffer = (unsigned char *)malloc(nativeW * nativeH * bpp);
	if (nativeBuffer == NULL)
		return 1;
	memset(nativeBuffer, 0, nativeW * nativeH * bpp);

	if (rotate == 0) {
		displayBuffer = nativeBuffer;
	} else {
		displayBuffer = (unsigned char *)malloc(dispW * dispH * bpp);
		if (displayBuffer == NULL)
			return 1;
		memset(displayBuffer, 0, dispW * dispH * bpp);
	}

	// FBNeo draws into the native buffer, pitch = native width.
	nVidImageWidth = nativeW;
	nVidImageHeight = nativeH;
	nVidImagePitch = nativeW * bpp;
	nBurnBpp = bpp;
	nBurnPitch = nativeW * bpp;
	pVidImage = nativeBuffer;

	return 0;
}

static int HeadlessVideoExit()
{
	freeBuffers();
	nativeW = nativeH = dispW = dispH = bpp = rotate = 0;
	return 0;
}

// Rotate the freshly rendered native frame into the upright display buffer.
// Called by the frontend after BurnDrvFrame.
void headless_video_present(void)
{
	if (rotate == 0 || displayBuffer == nativeBuffer)
		return;

	const uint32_t *src = (const uint32_t *)nativeBuffer;
	uint32_t *dst = (uint32_t *)displayBuffer;

	if (rotate == 2) { // 180
		for (int dy = 0; dy < dispH; dy++)
			for (int dx = 0; dx < dispW; dx++)
				dst[dy * dispW + dx] = src[(dispH - 1 - dy) * nativeW + (dispW - 1 - dx)];
	} else if (rotate == 1) { // CW 90 (dispW == nativeH, dispH == nativeW)
		for (int dy = 0; dy < dispH; dy++)
			for (int dx = 0; dx < dispW; dx++)
				dst[dy * dispW + dx] = src[(nativeH - 1 - dx) * nativeW + dy];
	} else { // CCW 90
		for (int dy = 0; dy < dispH; dy++)
			for (int dx = 0; dx < dispW; dx++)
				dst[dy * dispW + dx] = src[dx * nativeW + (nativeW - 1 - dy)];
	}
}

static int HeadlessVideoFrame(bool redraw)
{
	if (pVidImage == NULL || bRunPause)
		return 0;
	VidFrameCallback(redraw);
	return 0;
}

static int HeadlessVideoPaint(int /* validate */)
{
	return 0;
}

static int HeadlessVideoScale(RECT *, int, int)
{
	return 0;
}

static int HeadlessVideoGetSettings(InterfaceInfo *)
{
	return 0;
}

struct VidOut VidOutHeadless = {
	HeadlessVideoInit,
	HeadlessVideoExit,
	HeadlessVideoFrame,
	HeadlessVideoPaint,
	HeadlessVideoScale,
	HeadlessVideoGetSettings,
	_T("Headless Video"),
};

// --- C accessors for the host wrapper ---

extern "C" const unsigned char *FBHeadlessGetVideoBuffer(void) { return displayBuffer; }
extern "C" int FBHeadlessGetVideoWidth(void)  { return dispW; }
extern "C" int FBHeadlessGetVideoHeight(void) { return dispH; }
extern "C" int FBHeadlessGetVideoPitch(void)  { return dispW * bpp; }
extern "C" int FBHeadlessGetVideoBPP(void)    { return bpp; }
