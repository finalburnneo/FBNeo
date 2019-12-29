// Support functions for all blitters
#include "burner.h"
#include "vid_support.h"

// ---------------------------------------------------------------------------
// General

static UINT8* pVidSFullImage = nullptr;

void VidSFreeVidImage()
{
	if (pVidSFullImage) {
		free(pVidSFullImage);
		pVidSFullImage = nullptr;
	}
	pVidImage = nullptr;
}

INT32 VidSAllocVidImage()
{
	INT32 nMemLen = 0;

	VidSFreeVidImage();

	// Allocate an extra line above and below the image to accomodate effects
	nVidImagePitch = nVidImageWidth * ((nVidImageDepth + 7) >> 3);
	nMemLen = (nVidImageHeight + 2) * nVidImagePitch;
	pVidSFullImage = (UINT8*)malloc(nMemLen);

	if (pVidSFullImage) {
		memset(pVidSFullImage, 0, nMemLen);
		pVidImage = pVidSFullImage + nVidImagePitch;
		return 0;
	} else {
		pVidImage = nullptr;
		return 1;
	}
}

