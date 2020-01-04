// Based on MAME sources by Phil Stroffolino, Aaron Giles, and Alex W. Jackson

#include "tiles_generic.h"
#include "m68000_intf.h"

UINT8 *c45RoadRAM = NULL; // external

static UINT8 *c45RoadTiles = NULL;
static UINT16 *c45RoadBitmap = NULL;
static UINT8 *c45RoadClut = NULL;
static UINT8 c45_temp_clut[0x100];
static INT32 c45_transparent_color = 0x7fffffff;

void c45RoadReset()
{
	if (c45RoadRAM != NULL) memset (c45RoadRAM, 0, 0x20000);
	if (c45RoadTiles != NULL) memset (c45RoadTiles, 0, 0x40000);
}

void c45RoadInit(UINT32 transparent_color, UINT8 *clut)
{
	c45RoadRAM = (UINT8*)BurnMalloc(0x20000);
	c45RoadTiles = (UINT8*)BurnMalloc(0x40000);

	c45RoadClut = clut;
	c45RoadBitmap = (UINT16*)BurnMalloc(0x400 * sizeof(UINT16));

	c45_transparent_color = transparent_color;
	
	if (c45RoadClut == NULL) {
		c45RoadClut = c45_temp_clut;
		for (INT32 i = 0; i < 0x100; i++) c45RoadClut[i] = i;
	}
}

void c45RoadExit()
{
	if (c45RoadRAM != NULL) BurnFree(c45RoadRAM);
	if (c45RoadTiles != NULL) BurnFree(c45RoadTiles);
	if (c45RoadBitmap != NULL) BurnFree(c45RoadBitmap);

	c45RoadRAM = NULL;
	c45RoadTiles = NULL;
	c45RoadBitmap = NULL;

	c45RoadClut = NULL;

	c45_transparent_color = 0x7fffffff;
}

static inline void update_tile_pixel(INT32 offset)
{
	UINT16 s = *((UINT16*)(c45RoadRAM + (offset * 2)));

	UINT8 *pxl = c45RoadTiles + ((offset * 8) & 0x3fff8);

	for (INT32 x = 7; x >= 0; x--) {
		pxl[7-x] = (((s >> (x + 8)) & 1) << 1) | ((s >> x) & 1);
	}
}

static void __fastcall c45_road_write_word(UINT32 address, UINT16 data)
{
	UINT16 *ram = (UINT16*)c45RoadRAM;

	INT32 offset = (address & 0x1ffff) / 2;

	if (offset < (0x1fa00 / 2)) // tiles
	{
		if (data != ram[offset]) {
			ram[offset] = data;
			update_tile_pixel(offset);
		}
	}
	else	// scroll regs
	{
		ram[offset] = data;
	}
}

static void __fastcall c45_road_write_byte(UINT32 address, UINT8 data)
{
	UINT16 *ram = (UINT16*)c45RoadRAM;

	INT32 offset = (address & 0x1ffff) ^ 1;

	if (offset < 0x1fa00)	// tiles
	{
		if (data != ram[offset]) {
			c45RoadRAM[offset] = data;
			update_tile_pixel(offset/2);
		}
	}
	else	// scroll regs
	{
		c45RoadRAM[offset] = data;
	}
}

void c45RoadMap68k(UINT32 address)
{
	SekMapHandler(7,		address, address | 0x1ffff, MAP_WRITE);
	SekSetWriteWordHandler(7,	c45_road_write_word);
	SekSetWriteByteHandler(7,	c45_road_write_byte);

	SekMapMemory(c45RoadRAM,	address, address | 0x0ffff, MAP_WRITE);
}

static void predraw_c45_road_tiles_line(UINT32 line, UINT32 startx, UINT32 pixels_wide, UINT32 incx)
{
	UINT16 *ram = (UINT16*)c45RoadRAM;

	INT32 startx_shift = startx >> 20;
	INT32 endx_shift = (((pixels_wide * incx) >> 20) + 1) + startx_shift;

	INT32 offs = ((line / 16) * 64) + startx_shift;

	INT32 code_offset = (line & 0xf) * 16;

	for (INT32 sx = startx_shift; sx < endx_shift; sx++, offs++)
	{
		INT32 color = ((ram[offs] >> 10) << 2);
		UINT8 *gfx = c45RoadTiles + ((ram[offs] & 0x3ff) * 256 + code_offset);
		UINT8 *clut = c45RoadClut + color;

		for (INT32 x = 0; x < 16; x++)
		{
			c45RoadBitmap[((sx * 16) + x) & 0x3ff] = clut[gfx[x]] + 0xf00;
		}
	}
}

void c45RoadDraw()
{
	INT32 min_x, min_y, max_x, max_y;

	GenericTilesGetClip(&min_x, &max_x, &min_y, &max_y);

	UINT16 *m_lineram = (UINT16*)(c45RoadRAM + 0x1fa00);

	UINT32 yscroll = m_lineram[0x3fe/2];

	static INT32 ROAD_COLS = 64;
	static INT32 ROAD_ROWS = 512;
	static INT32 ROAD_TILE_SIZE = 16;
	static INT32 ROAD_TILEMAP_WIDTH = ROAD_TILE_SIZE * ROAD_COLS;
	static INT32 ROAD_TILEMAP_HEIGHT = ROAD_TILE_SIZE * ROAD_ROWS;

	// loop over scanlines
	for (INT32 y = min_y; y < max_y; y++)
	{
		// skip if we are not the right priority
		INT32 screenx = m_lineram[y + 15];

		INT32 pri = (screenx & 0xf000) >> 12;

		// skip if we don't have a valid zoom factor
		UINT32 zoomx = m_lineram[0x400/2 + y + 15] & 0x3ff;
		if (zoomx == 0)
			continue;

		// skip if we don't have a valid source increment
		UINT32 sourcey = m_lineram[0x200/2 + y + 15] + yscroll;
		const UINT16 *source_gfx = c45RoadBitmap;
		UINT32 dsourcex = (ROAD_TILEMAP_WIDTH << 16) / zoomx;
		if (dsourcex == 0)
			continue;

		// mask off priority bits and sign-extend
		screenx &= 0x0fff;
		if (screenx & 0x0800)
			screenx |= ~0x7ff;

		// adjust the horizontal placement
		screenx -= (64+16); // needs adjustment to left

		INT32 numpixels = (44 * ROAD_TILE_SIZE << 16) / dsourcex;
		UINT32 sourcex = 0;

		// crop left
		INT32 clip_pixels = min_x - screenx;
		if (clip_pixels > 0)
		{
			numpixels -= clip_pixels;
			sourcex += dsourcex*clip_pixels;
			screenx = min_x;
		}

		// crop right
		clip_pixels = (screenx + numpixels) - (max_x + 1);
		if (clip_pixels > 0)
			numpixels -= clip_pixels;

		UINT16 *dest = pTransDraw + y * nScreenWidth;
		UINT8 *pdest = pPrioDraw + y * nScreenWidth;

		predraw_c45_road_tiles_line(sourcey & (ROAD_TILEMAP_HEIGHT - 1), sourcex, numpixels, dsourcex);

		while (numpixels-- > 0)
		{
			INT32 destpri = pdest[screenx];
			INT32 pixel = source_gfx[sourcex >> 16];

			if (destpri <= pri ) {
				if (pixel != c45_transparent_color) {
					dest[screenx] = pixel;
				}
				pdest[screenx] = pri;
			}
			screenx++;
			sourcex += dsourcex;
		}
	}
}

// call from save state
void c45RoadState(INT32 nAction)
{
	if (c45RoadRAM == NULL) return;

	struct BurnArea ba;
	memset(&ba, 0, sizeof(ba));
	ba.Data		= c45RoadRAM;
	ba.nLen		= 0x20000;
	ba.szName	= "C45 Road RAM";
	BurnAcb(&ba);

	if (nAction & ACB_WRITE)
	{
		for (INT32 i = 0x10000; i < 0x1fa00; i++) {
			update_tile_pixel(i / 2);
		}
#if 0
		INT32 Planes[2] = { 0, 8 };
		INT32 XOffs[16] = { STEP8(0, 1), STEP8(16, 1) };
		INT32 YOffs[16] = { STEP16(0, 32) };

		GfxDecode(0x0400, 2, 16, 16, Planes, XOffs, YOffs, 0x200, c45RoadRAM + 0x10000, c45RoadTiles);
#endif
	}
}
