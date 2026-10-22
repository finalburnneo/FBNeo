// Headless video output for FinalBurn Neo.
// Author: Leonardo Kasperavičius
//
// Adapted from the macOS video interface by Akop Karapetyan
//   https://github.com/0xe1f/FinalBurn-X  (Apache License 2.0)
//
// Built originally to support OpenEmu-Silicon
// (https://github.com/OpenEmu-Silicon/OpenEmu-Silicon).
//
// Instead of drawing to a view, it keeps the emulated frame in a plain buffer
// the host reads each frame. Subject to the FB Neo license (see src/license.txt).

#include "burner.h"
#include "fbheadless.h"

static unsigned char *screenBuffer = NULL;
static int bufferWidth = 0;
static int bufferHeight = 0;
static int bufferBPP = 0;
static int bufferPitch = 0;

static int HeadlessVideoInit()
{
	int gameWidth = 0, gameHeight = 0;
	int rotationMode = 0;
	int flags = BurnDrvGetFlags();

	BurnDrvGetVisibleSize(&gameWidth, &gameHeight);

	if (flags & BDF_ORIENTATION_VERTICAL) rotationMode |= 1;
	if (flags & BDF_ORIENTATION_FLIPPED)  rotationMode ^= 2;

	// 32bpp XRGB8888 output so the host renderer can consume it directly.
	nVidImageDepth = 32;
	nVidImageBPP   = nVidImageDepth / 8;
	nVidImageWidth = gameWidth;
	nVidImageHeight = gameHeight;
	if (!rotationMode)
		nVidImagePitch = nVidImageWidth * nVidImageBPP;
	else
		nVidImagePitch = nVidImageHeight * nVidImageBPP;

	SetBurnHighCol(nVidImageDepth);

	bufferBPP = nVidImageBPP;
	bufferWidth = gameWidth;
	bufferHeight = gameHeight;
	bufferPitch = nVidImagePitch;

	int bufSize = bufferWidth * bufferHeight * nVidImageBPP;
	free(screenBuffer);
	screenBuffer = (unsigned char *)malloc(bufSize);
	if (screenBuffer == NULL)
		return 1;
	memset(screenBuffer, 0, bufSize);

	nBurnBpp = nVidImageBPP;
	nBurnPitch = nVidImagePitch;
	pVidImage = screenBuffer;

	return 0;
}

static int HeadlessVideoExit()
{
	free(screenBuffer);
	screenBuffer = NULL;
	bufferWidth = bufferHeight = bufferBPP = bufferPitch = 0;
	return 0;
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

extern "C" const unsigned char *FBHeadlessGetVideoBuffer(void) { return screenBuffer; }
extern "C" int FBHeadlessGetVideoWidth(void)  { return bufferWidth; }
extern "C" int FBHeadlessGetVideoHeight(void) { return bufferHeight; }
extern "C" int FBHeadlessGetVideoPitch(void)  { return bufferPitch; }
extern "C" int FBHeadlessGetVideoBPP(void)    { return bufferBPP; }
