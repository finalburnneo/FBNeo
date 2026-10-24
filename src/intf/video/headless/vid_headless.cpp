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

static unsigned char *nativeBuffer = NULL;  // FBNeo renders here (pBurnDraw), nativeBpp deep
static unsigned char *displayBuffer = NULL; // exposed to the host, always 32bpp upright
static int nativeW = 0, nativeH = 0;
static int dispW = 0, dispH = 0;
static int nativeBpp = 0; // FBNeo render depth in bytes: 2 (BDF_16BIT_ONLY) or 4
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

	// Some drivers (Cave, CPS3, Megadrive) are BDF_16BIT_ONLY: their blitters only
	// write RGB565, so render those at 16bpp and convert to 32bpp on present. All
	// others render straight 32bpp XRGB8888, which the host consumes directly.
	if (flags & BDF_16BIT_ONLY) {
		nativeBpp = 2;
		nVidImageDepth = 16;
	} else {
		nativeBpp = 4;
		nVidImageDepth = 32;
	}
	nVidImageBPP = nativeBpp;
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
	nativeBuffer = (unsigned char *)malloc(nativeW * nativeH * nativeBpp);
	if (nativeBuffer == NULL)
		return 1;
	memset(nativeBuffer, 0, nativeW * nativeH * nativeBpp);

	// The host always receives 32bpp. Zero-copy only when FBNeo already renders
	// 32bpp AND upright; otherwise present() fills a dedicated 32bpp buffer
	// (converting 16->32 and/or rotating).
	if (nativeBpp == 4 && rotate == 0) {
		displayBuffer = nativeBuffer;
	} else {
		displayBuffer = (unsigned char *)malloc(dispW * dispH * 4);
		if (displayBuffer == NULL)
			return 1;
		memset(displayBuffer, 0, dispW * dispH * 4);
	}

	// FBNeo draws into the native buffer, pitch = native width.
	nVidImageWidth = nativeW;
	nVidImageHeight = nativeH;
	nVidImagePitch = nativeW * nativeBpp;
	nBurnBpp = nativeBpp;
	nBurnPitch = nativeW * nativeBpp;
	pVidImage = nativeBuffer;

	return 0;
}

static int HeadlessVideoExit()
{
	freeBuffers();
	nativeW = nativeH = dispW = dispH = nativeBpp = rotate = 0;
	return 0;
}

// RGB565 -> 0x00RRGGBB, matching SetBurnHighCol(32)'s component order.
static inline uint32_t rgb565_to_xrgb8888(uint16_t p)
{
	uint32_t r = (p >> 11) & 0x1F;
	uint32_t g = (p >> 5)  & 0x3F;
	uint32_t b =  p        & 0x1F;
	r = (r << 3) | (r >> 2);
	g = (g << 2) | (g >> 4);
	b = (b << 3) | (b >> 2);
	return (r << 16) | (g << 8) | b;
}

// Map a display pixel (dx,dy) back to its native-buffer coordinate for the
// current rotation. Native/display dims are swapped for the 90-degree cases.
static inline void src_coord(int dx, int dy, int *sx, int *sy)
{
	switch (rotate) {
	case 1:  *sx = dy;               *sy = nativeH - 1 - dx; break; // CW 90
	case 3:  *sx = nativeW - 1 - dy; *sy = dx;               break; // CCW 90
	case 2:  *sx = dispW - 1 - dx;   *sy = dispH - 1 - dy;   break; // 180
	default: *sx = dx;               *sy = dy;               break; // none
	}
}

// Present the freshly rendered native frame to the host: convert 16->32bpp for
// BDF_16BIT_ONLY drivers and/or rotate vertical games upright. Called by the
// frontend after BurnDrvFrame. The zero-copy case (32bpp, upright) is a no-op.
void headless_video_present(void)
{
	if (displayBuffer == nativeBuffer)
		return;

	uint32_t *dst = (uint32_t *)displayBuffer;

	if (nativeBpp == 2) {
		const uint16_t *src = (const uint16_t *)nativeBuffer;
		for (int dy = 0; dy < dispH; dy++)
			for (int dx = 0; dx < dispW; dx++) {
				int sx, sy;
				src_coord(dx, dy, &sx, &sy);
				dst[dy * dispW + dx] = rgb565_to_xrgb8888(src[sy * nativeW + sx]);
			}
	} else {
		const uint32_t *src = (const uint32_t *)nativeBuffer;
		for (int dy = 0; dy < dispH; dy++)
			for (int dx = 0; dx < dispW; dx++) {
				int sx, sy;
				src_coord(dx, dy, &sx, &sy);
				dst[dy * dispW + dx] = src[sy * nativeW + sx];
			}
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
extern "C" int FBHeadlessGetVideoPitch(void)  { return dispW * 4; }
extern "C" int FBHeadlessGetVideoBPP(void)    { return 4; }
