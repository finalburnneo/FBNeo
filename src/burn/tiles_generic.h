#include "burnint.h"
#include "burn_bitmap.h"
#include "tilemap_generic.h"

#define MAX_GFX		32	// number of graphics data regions allowed

struct GenericTilesGfx {
	UINT8 *gfxbase;		// pointer to graphics data
	INT32 depth;		// bits per pixel
	INT32 width;		// tile width
	INT32 height;		// tile height
	UINT32 gfx_len;		// full size of the tile data
	UINT32 code_mask;	// gfx_len / width / height
	UINT32 color_offset;// is there a color offset for this graphics region?
	UINT32 color_mask;	// mask the color added to the pixels
};

extern GenericTilesGfx GenericGfxData[];
void GenericTilesSetGfx(INT32 nNum, UINT8 *GfxBase, INT32 nDepth, INT32 nTileWidth, INT32 nTileHeight, INT32 nGfxLen, UINT32 nColorOffset, UINT32 nColorMask);

extern UINT8* pTileData;
extern INT32 nScreenWidth, nScreenHeight;
extern UINT8 GenericTilesPRIMASK;

INT32 GenericTilesInit();
INT32 GenericTilesExit();

// Tile decoding macros
#define RGN_FRAC(length, numerator, denominator) ((((length) * 8) * (numerator)) / (denominator))

#define STEP2(start, step)	((start) + ((step)*0)), ((start) + ((step)*1))
#define STEP4(start, step)	STEP2(start, step),  STEP2((start)+((step)*2), step)
#define STEP8(start, step)	STEP4(start, step),  STEP4((start)+((step)*4), step)
#define STEP16(start, step)	STEP8(start, step),  STEP8((start)+((step)*8), step)
#define STEP32(start, step)	STEP16(start, step), STEP16((start)+((step)*16), step)
#define STEP64(start, step)	STEP32(start, step), STEP32((start)+((step)*32), step)

void GfxDecode(INT32 num, INT32 numPlanes, INT32 xSize, INT32 ySize, INT32 planeoffsets[], INT32 xoffsets[], INT32 yoffsets[], INT32 modulo, UINT8 *pSrc, UINT8 *pDest);
void GfxDecodeSingle(INT32 which, INT32 numPlanes, INT32 xSize, INT32 ySize, INT32 planeoffsets[], INT32 xoffsets[], INT32 yoffsets[], INT32 modulo, UINT8 *pSrc, UINT8 *pDest);

void GenericTilesSetClip(INT32 nMinx, INT32 nMaxx, INT32 nMiny, INT32 nMaxy);
void GenericTilesGetClip(INT32 *nMinx, INT32 *nMaxx, INT32 *nMiny, INT32 *nMaxy);
void GenericTilesClearClip();
void GenericTilesSetClipRaw(INT32 nMinx, INT32 nMaxx, INT32 nMiny, INT32 nMaxy);
void GenericTilesClearClipRaw();
void GenericTilesSetScanline(INT32 nScanline);

// Sprite priority handling is different than tile!
void RenderPrioSprite(UINT16 *dest, UINT8 *gfx, INT32 code, INT32 color, INT32 t, INT32 sx, INT32 sy, INT32 fx, INT32 fy, INT32 width, INT32 height, INT32 priority);
void RenderZoomedPrioSprite(UINT16 *dest, UINT8 *gfx, INT32 code, INT32 color, INT32 t, INT32 sx, INT32 sy, INT32 fx, INT32 fy, INT32 width, INT32 height, INT32 zoomx, INT32 zoomy, INT32 priority);
void RenderPrioMaskTranstabSprite(UINT16 *dest, UINT8 *gfx, INT32 code, INT32 color, INT32 trans_col, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy, INT32 width, INT32 height, UINT8 *tab, UINT32 priority);
void RenderPrioMaskTranstabSpriteOffset(UINT16 *dest, UINT8 *gfx, INT32 code, INT32 color, INT32 trans_col, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy, INT32 width, INT32 height, UINT8 *tab, UINT32 color_offset, UINT32 priority);
void RenderZoomedPrioTranstabSprite(UINT16 *dest, UINT8 *gfx, INT32 code, INT32 color, INT32 t, INT32 sx, INT32 sy, INT32 fx, INT32 fy, INT32 width, INT32 height, INT32 zoomx, INT32 zoomy, UINT8 *tab, INT32 priority);
void RenderZoomedPrioTranstabSpriteOffset(UINT16 *dest, UINT8 *gfx, INT32 code, INT32 color, INT32 t, INT32 sx, INT32 sy, INT32 fx, INT32 fy, INT32 width, INT32 height, INT32 zoomx, INT32 zoomy, UINT8 *tab, UINT32 color_offset, INT32 priority);

void RenderZoomedTile(UINT16 *pDestDraw, UINT8 *gfx, INT32 code, INT32 color, INT32 trans_color, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy, INT32 width, INT32 height, INT32 zoomx, INT32 zoomy);
void RenderZoomedPrioTile(UINT16 *dest, UINT8 *gfx, INT32 code, INT32 color, INT32 t, INT32 sx, INT32 sy, INT32 fx, INT32 fy, INT32 width, INT32 height, INT32 zoomx, INT32 zoomy, INT32 priority);

void RenderTileTranstab(UINT16 *dest, UINT8 *gfx, INT32 code, INT32 color, INT32 trans_col, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy, INT32 width, INT32 height, UINT8 *tab);
void RenderTileTranstabOffset(UINT16 *dest, UINT8 *gfx, INT32 code, INT32 color, INT32 trans_col, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy, INT32 width, INT32 height, UINT8 *tab, UINT32 color_offset);
void RenderTilePrioTranstab(UINT16 *dest, UINT8 *gfx, INT32 code, INT32 color, INT32 trans_col, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy, INT32 width, INT32 height, UINT8 *tab, INT32 priority);
void RenderTilePrioTranstabOffset(UINT16 *dest, UINT8 *gfx, INT32 code, INT32 color, INT32 trans_col, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy, INT32 width, INT32 height, UINT8 *tab, UINT32 color_offset, INT32 priority);

// draw single tile - Set gfx with GenericTilesSetGfx!
// respects clipping for bitmap!
void DrawGfxTile(INT32 nBitmap, INT32 nGfx, INT32 nTileNumber, INT32 nStartX, INT32 nStartY, INT32 nFlipx, INT32 nFlipy, INT32 nPalette);
void DrawGfxMaskTile(INT32 nBitmap, INT32 nGfx, INT32 nTileNumber, INT32 nStartX, INT32 nStartY, INT32 nFlipx, INT32 nFlipy, INT32 nPalette, INT32 nMaskColor);
void DrawGfxPrioTile(INT32 nBitmap, INT32 nGfx, INT32 nTileNumber, INT32 nStartX, INT32 nStartY, INT32 nFlipx, INT32 nFlipy, INT32 nPalette, INT32 nPriority);
void DrawGfxPrioMaskTile(INT32 nBitmap, INT32 nGfx, INT32 nTileNumber, INT32 nStartX, INT32 nStartY, INT32 nFlipx, INT32 nFlipy, INT32 nPalette, INT32 nMaskColor, INT32 nPriority);

// draw single tile with flipping, auto-clips if necessary
void Draw8x8Tile(UINT16 *pDestDraw, INT32 NTileNumber, INT32 StartX, INT32 StartY, INT32 FlipX, INT32 FlipY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile);
void Draw8x8MaskTile(UINT16 *pDestDraw, INT32 NTileNumber, INT32 StartX, INT32 StartY, INT32 FlipX, INT32 FlipY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile);
void Draw8x8PrioTile(UINT16 *pDestDraw, INT32 NTileNumber, INT32 StartX, INT32 StartY, INT32 FlipX, INT32 FlipY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void Draw8x8PrioMaskTile(UINT16 *pDestDraw, INT32 NTileNumber, INT32 StartX, INT32 StartY, INT32 FlipX, INT32 FlipY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);

void Draw16x16Tile(UINT16 *pDestDraw, INT32 NTileNumber, INT32 StartX, INT32 StartY, INT32 FlipX, INT32 FlipY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile);
void Draw16x16MaskTile(UINT16 *pDestDraw, INT32 NTileNumber, INT32 StartX, INT32 StartY, INT32 FlipX, INT32 FlipY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile);
void Draw16x16PrioTile(UINT16 *pDestDraw, INT32 NTileNumber, INT32 StartX, INT32 StartY, INT32 FlipX, INT32 FlipY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void Draw16x16PrioMaskTile(UINT16 *pDestDraw, INT32 NTileNumber, INT32 StartX, INT32 StartY, INT32 FlipX, INT32 FlipY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);

void Draw32x32Tile(UINT16 *pDestDraw, INT32 NTileNumber, INT32 StartX, INT32 StartY, INT32 FlipX, INT32 FlipY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile);
void Draw32x32MaskTile(UINT16 *pDestDraw, INT32 NTileNumber, INT32 StartX, INT32 StartY, INT32 FlipX, INT32 FlipY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile);
void Draw32x32PrioTile(UINT16 *pDestDraw, INT32 NTileNumber, INT32 StartX, INT32 StartY, INT32 FlipX, INT32 FlipY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void Draw32x32PrioMaskTile(UINT16 *pDestDraw, INT32 NTileNumber, INT32 StartX, INT32 StartY, INT32 FlipX, INT32 FlipY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);

void DrawCustomTile(UINT16 *pDestDraw, INT32 nWidth, INT32 nHeight, INT32 NTileNumber, INT32 StartX, INT32 StartY, INT32 FlipX, INT32 FlipY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile);
void DrawCustomMaskTile(UINT16 *pDestDraw, INT32 nWidth, INT32 nHeight, INT32 NTileNumber, INT32 StartX, INT32 StartY, INT32 FlipX, INT32 FlipY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile);
void DrawCustomPrioTile(UINT16 *pDestDraw, INT32 nWidth, INT32 nHeight, INT32 NTileNumber, INT32 StartX, INT32 StartY, INT32 FlipX, INT32 FlipY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void DrawCustomPrioMaskTile(UINT16 *pDestDraw, INT32 nWidth, INT32 nHeight, INT32 NTileNumber, INT32 StartX, INT32 StartY, INT32 FlipX, INT32 FlipY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);

// draw single tile, clipping & flipping must be called
void Render8x8Tile(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile);
void Render8x8Tile_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile);
void Render8x8Tile_FlipX(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile);
void Render8x8Tile_FlipX_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile);
void Render8x8Tile_FlipY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile);
void Render8x8Tile_FlipY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile);
void Render8x8Tile_FlipXY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile);
void Render8x8Tile_FlipXY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile);

void Render8x8Tile_Mask(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile);
void Render8x8Tile_Mask_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile);
void Render8x8Tile_Mask_FlipX(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile);
void Render8x8Tile_Mask_FlipX_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile);
void Render8x8Tile_Mask_FlipY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile);
void Render8x8Tile_Mask_FlipY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile);
void Render8x8Tile_Mask_FlipXY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile);
void Render8x8Tile_Mask_FlipXY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile);

void Render8x8Tile_Prio(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void Render8x8Tile_Prio_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void Render8x8Tile_Prio_FlipX(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void Render8x8Tile_Prio_FlipX_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void Render8x8Tile_Prio_FlipY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void Render8x8Tile_Prio_FlipY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void Render8x8Tile_Prio_FlipXY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void Render8x8Tile_Prio_FlipXY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);

void Render8x8Tile_Prio_Mask(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void Render8x8Tile_Prio_Mask_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void Render8x8Tile_Prio_Mask_FlipX(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void Render8x8Tile_Prio_Mask_FlipX_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void Render8x8Tile_Prio_Mask_FlipY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void Render8x8Tile_Prio_Mask_FlipY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void Render8x8Tile_Prio_Mask_FlipXY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void Render8x8Tile_Prio_Mask_FlipXY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);

void Render16x16Tile(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile);
void Render16x16Tile_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile);
void Render16x16Tile_FlipX(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile);
void Render16x16Tile_FlipX_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile);
void Render16x16Tile_FlipY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile);
void Render16x16Tile_FlipY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile);
void Render16x16Tile_FlipXY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile);
void Render16x16Tile_FlipXY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile);

void Render16x16Tile_Mask(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile);
void Render16x16Tile_Mask_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile);
void Render16x16Tile_Mask_FlipX(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile);
void Render16x16Tile_Mask_FlipX_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile);
void Render16x16Tile_Mask_FlipY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile);
void Render16x16Tile_Mask_FlipY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile);
void Render16x16Tile_Mask_FlipXY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile);
void Render16x16Tile_Mask_FlipXY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile);

void Render16x16Tile_Prio(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void Render16x16Tile_Prio_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void Render16x16Tile_Prio_FlipX(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void Render16x16Tile_Prio_FlipX_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void Render16x16Tile_Prio_FlipY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void Render16x16Tile_Prio_FlipY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void Render16x16Tile_Prio_FlipXY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void Render16x16Tile_Prio_FlipXY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);

void Render16x16Tile_Prio_Mask(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void Render16x16Tile_Prio_Mask_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void Render16x16Tile_Prio_Mask_FlipX(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void Render16x16Tile_Prio_Mask_FlipX_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void Render16x16Tile_Prio_Mask_FlipY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void Render16x16Tile_Prio_Mask_FlipY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void Render16x16Tile_Prio_Mask_FlipXY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void Render16x16Tile_Prio_Mask_FlipXY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);

void Render32x32Tile(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile);
void Render32x32Tile_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile);
void Render32x32Tile_FlipX(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile);
void Render32x32Tile_FlipX_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile);
void Render32x32Tile_FlipY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile);
void Render32x32Tile_FlipY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile);
void Render32x32Tile_FlipXY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile);
void Render32x32Tile_FlipXY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile);

void Render32x32Tile_Mask(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile);
void Render32x32Tile_Mask_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile);
void Render32x32Tile_Mask_FlipX(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile);
void Render32x32Tile_Mask_FlipX_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile);
void Render32x32Tile_Mask_FlipY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile);
void Render32x32Tile_Mask_FlipY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile);
void Render32x32Tile_Mask_FlipXY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile);
void Render32x32Tile_Mask_FlipXY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile);

void Render32x32Tile_Prio(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void Render32x32Tile_Prio_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void Render32x32Tile_Prio_FlipX(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void Render32x32Tile_Prio_FlipX_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void Render32x32Tile_Prio_FlipY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void Render32x32Tile_Prio_FlipY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void Render32x32Tile_Prio_FlipXY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void Render32x32Tile_Prio_FlipXY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);

void Render32x32Tile_Prio_Mask(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void Render32x32Tile_Prio_Mask_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void Render32x32Tile_Prio_Mask_FlipX(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void Render32x32Tile_Prio_Mask_FlipX_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void Render32x32Tile_Prio_Mask_FlipY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void Render32x32Tile_Prio_Mask_FlipY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void Render32x32Tile_Prio_Mask_FlipXY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void Render32x32Tile_Prio_Mask_FlipXY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);

void RenderCustomTile(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile);
void RenderCustomTile_Clip(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile);
void RenderCustomTile_FlipX(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile);
void RenderCustomTile_FlipX_Clip(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile);
void RenderCustomTile_FlipY(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile);
void RenderCustomTile_FlipY_Clip(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile);
void RenderCustomTile_FlipXY(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile);
void RenderCustomTile_FlipXY_Clip(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile);

void RenderCustomTile_Mask(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile);
void RenderCustomTile_Mask_Clip(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile);
void RenderCustomTile_Mask_FlipX(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile);
void RenderCustomTile_Mask_FlipX_Clip(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile);
void RenderCustomTile_Mask_FlipY(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile);
void RenderCustomTile_Mask_FlipY_Clip(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile);
void RenderCustomTile_Mask_FlipXY(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile);
void RenderCustomTile_Mask_FlipXY_Clip(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile);

void RenderCustomTile_Prio(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void RenderCustomTile_Prio_Clip(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void RenderCustomTile_Prio_FlipX(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void RenderCustomTile_Prio_FlipX_Clip(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void RenderCustomTile_Prio_FlipY(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void RenderCustomTile_Prio_FlipY_Clip(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void RenderCustomTile_Prio_FlipXY(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void RenderCustomTile_Prio_FlipXY_Clip(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);

void RenderCustomTile_Prio_Mask(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void RenderCustomTile_Prio_Mask_Clip(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void RenderCustomTile_Prio_Mask_FlipX(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void RenderCustomTile_Prio_Mask_FlipX_Clip(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void RenderCustomTile_Prio_Mask_FlipY(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void RenderCustomTile_Prio_Mask_FlipY_Clip(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void RenderCustomTile_Prio_Mask_FlipXY(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void RenderCustomTile_Prio_Mask_FlipXY_Clip(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);

void RenderCustomTile_Prio_TransMask(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, UINT8 *pTransTable, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void RenderCustomTile_Prio_TransMask_Clip(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, UINT8 *pTransTable, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void RenderCustomTile_Prio_TransMask_FlipX(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, UINT8 *pTransTable, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void RenderCustomTile_Prio_TransMask_FlipX_Clip(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, UINT8 *pTransTable, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void RenderCustomTile_Prio_TransMask_FlipY(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, UINT8 *pTransTable, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void RenderCustomTile_Prio_TransMask_FlipY_Clip(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, UINT8 *pTransTable, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void RenderCustomTile_Prio_TransMask_FlipXY(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, UINT8 *pTransTable, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);
void RenderCustomTile_Prio_TransMask_FlipXY_Clip(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, UINT8 *pTransTable, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile);


// ---------------------------------------------------------------------------
// Colour-depth independant image transfer

extern UINT16* pTransDraw;
extern UINT8* pPrioDraw;

void BurnTransferClear();
void BurnTransferClear(UINT16 nFillPattern);
void BurnPrioClear();
INT32 BurnTransferCopy(UINT32* pPalette);
void BurnTransferExit();
INT32 BurnTransferInit();
