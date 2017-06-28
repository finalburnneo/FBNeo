#include "cps.h"

// CPS Tile Variants
// horizontal/vertical clip rolls
UINT32 nCtvRollX=0,nCtvRollY=0;
// Add 0x7fff after each pixel/line
// If nRollX/Y&0x20004000 both == 0, you can draw the pixel

UINT8 *pCtvTile=NULL; // Pointer to tile data
INT32 nCtvTileAdd=0; // Amount to add after each tile line
UINT8 *pCtvLine=NULL; // Pointer to output bitmap

static inline UINT32 alpha_blend(UINT32 d, UINT32 s, UINT32 p)
{
	INT32 a = 255 - p;

	return (((((s & 0xff00ff) * p) + ((d & 0xff00ff) * a)) & 0xff00ff00) +
		((((s & 0x00ff00) * p) + ((d & 0x00ff00) * a)) & 0x00ff0000)) >> 8;
}

// Include all tile variants:
#include "ctv.h"

static INT32 nLastBpp=0;
INT32 CtvReady()
{
  // Set up the CtvDoX functions to point to the correct bpp functions.
  // Must be called before calling CpstOne
  if (nBurnBpp!=nLastBpp)
  {
	  if (nBurnBpp==2) {
		memcpy(CtvDoX,CtvDo2,sizeof(CtvDoX));
		memcpy(CtvDoXM,CtvDo2m,sizeof(CtvDoXM));
		memcpy(CtvDoXB,CtvDo2b,sizeof(CtvDoXB));
	  }
	  else if (nBurnBpp==3) {
		memcpy(CtvDoX,CtvDo3,sizeof(CtvDoX));
		memcpy(CtvDoXM,CtvDo3m,sizeof(CtvDoXM));
		memcpy(CtvDoXB,CtvDo3b,sizeof(CtvDoXB));
	  }
	  else if (nBurnBpp==4) {
		memcpy(CtvDoX,CtvDo4,sizeof(CtvDoX));
		memcpy(CtvDoXM,CtvDo4m,sizeof(CtvDoXM));
		memcpy(CtvDoXB,CtvDo4b,sizeof(CtvDoXB));
	  }
  }
  nLastBpp=nBurnBpp;
  return 0;
}
