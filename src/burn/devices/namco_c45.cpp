#include "tiles_generic.h"
#include "m68000_intf.h"

UINT8 *c45RoadRAM = NULL; // external

static UINT8 *c45RoadTiles = NULL;
static UINT16 *c45RoadBitmap = NULL;
static UINT8 *c45RoadBuffer = NULL;
static UINT8 *c45RoadClut = NULL;
static INT32 c45_road_force_update = 0;
static INT32 c45_road_tiles_changed = 0;
static UINT32 c45_road_transparent_color = 0;
static UINT8 c45_temp_clut[0x100];

void c45RoadReset()
{
	c45_road_force_update = 1;
	c45_road_tiles_changed = 1;
}

void c45RoadInit(UINT32 trans_color, UINT8 *clut)
{
	c45RoadRAM = (UINT8*)BurnMalloc(0x20000);
	c45RoadBuffer = (UINT8*)BurnMalloc(0x10000);
	c45RoadTiles = (UINT8*)BurnMalloc(0x40000);

	c45RoadClut = clut;
	c45RoadBitmap = (UINT16*)BurnMalloc(0x2000 * 0x400 * sizeof(UINT16));

	c45_road_force_update = 1;
	c45_road_transparent_color = trans_color;

	if (c45RoadClut == NULL) {
		c45RoadClut = c45_temp_clut;
		for (INT32 i = 0; i < 0x100; i++) c45RoadClut[i] = i;
	}
}

void c45RoadExit()
{
	if (c45RoadRAM != NULL) BurnFree(c45RoadRAM);
	if (c45RoadBuffer != NULL) BurnFree(c45RoadBuffer);
	if (c45RoadTiles != NULL) BurnFree(c45RoadTiles);
	if (c45RoadBitmap != NULL) BurnFree(c45RoadBitmap);

	c45RoadClut = NULL;
}

static inline void update_tile_pixel(INT32 offset)
{
	UINT16 s = *((UINT16*)(c45RoadRAM + (offset * 2)));

	UINT8 *pxl = c45RoadTiles + ((offset * 8) & 0x3fff8);

	// this may need some work
	for (INT32 x = 0; x < 8; x++) {
		pxl[x] = (((s >> ((7 - (x & 7)) + 8)) & 1) << 1) | (((s >> (7 - (x & 7))) & 1) << 0);
	}

	c45_road_force_update = 1;
}

static void __fastcall c45_road_write_word(UINT32 address, UINT16 data)
{
	UINT16 *ram = (UINT16*)c45RoadRAM;

	INT32 offset = (address & 0x1ffff) / 2;

	if (offset < (0x10000 / 2)) // tile map
	{
		if (data != ram[offset]) {
			ram[offset] = data;
			c45RoadBuffer[offset] = 1;
			c45_road_tiles_changed = 1;
		}
	}
	else if (offset < (0x1fa00 / 2)) // tiles
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

	if (offset < 0x10000)	// tile map
	{
		if (data != ram[offset]) {
			c45RoadRAM[offset] = data;
			c45RoadBuffer[offset/2] = 1;
			c45_road_tiles_changed = 1;
		}
	}
	else if (offset < 0x1fa00)	// tiles
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
}

static void predraw_c45_road_tiles()
{
	// tiles not dirty
	if (c45_road_tiles_changed == 0 && c45_road_force_update == 0)
	{
		return;
	}

	UINT16 *ram = (UINT16*)c45RoadRAM;

	for (INT32 offs = 0; offs < 64 * 512; offs++)
	{
		if (c45RoadBuffer[offs] == 0 && c45_road_force_update == 0) {
			continue;
		}
		c45RoadBuffer[offs] = 0;

		INT32 sx = (offs & 0x3f) * 16;
		INT32 sy = (offs / 0x40) * 16;

		INT32 code  = (ram[offs] & 0x3ff) * 0x100;
		INT32 color = ((ram[offs] >> 10) << 2);
		UINT8 *gfx = c45RoadTiles + code;
		UINT16 *dst = c45RoadBitmap + (sy * 0x400) + sx;

		UINT8 *clut = c45RoadClut + color;

		for (INT32 y = 0; y < 16; y++)
		{
			for (INT32 x = 0; x < 16; x++)
			{
				dst[x] = clut[gfx[x]] + 0xf00;
			}

			gfx += 16;
			dst += 0x400;
		}
	}

	c45_road_force_update = 0;
	c45_road_tiles_changed = 0;
}

void c45RoadDraw(int pri, int min_y_ovrride, int max_y_ovrride)
{
	predraw_c45_road_tiles();

	INT32 min_x, min_y, max_x, max_y;

	GenericTilesGetClip(&min_x, &max_x, &min_y, &max_y);

	if (min_y_ovrride != -1) min_y = min_y_ovrride;
	if (max_y_ovrride != -1) max_y = max_y_ovrride;

	UINT16 *m_lineram = (UINT16*)(c45RoadRAM + 0x1fa00);

	unsigned yscroll = m_lineram[0x3fe/2];

	static int ROAD_COLS = 64;
	static int ROAD_ROWS = 512;
	static int ROAD_TILE_SIZE = 16;
	static int ROAD_TILEMAP_WIDTH = ROAD_TILE_SIZE * ROAD_COLS;
	static int ROAD_TILEMAP_HEIGHT = ROAD_TILE_SIZE * ROAD_ROWS;

	// loop over scanlines
	for (int y = min_y; y < max_y; y++)
	{
		// skip if we are not the right priority
		int screenx = m_lineram[y + 15];
		if (pri != ((screenx & 0xf000) >> 12))
			continue;

		// skip if we don't have a valid zoom factor
		unsigned zoomx = m_lineram[0x400/2 + y + 15] & 0x3ff;
		if (zoomx == 0)
			continue;

		// skip if we don't have a valid source increment
		unsigned sourcey = m_lineram[0x200/2 + y + 15] + yscroll;
		const uint16_t *source_gfx = c45RoadBitmap + ((sourcey & (ROAD_TILEMAP_HEIGHT - 1))) * 0x400;
		unsigned dsourcex = (ROAD_TILEMAP_WIDTH << 16) / zoomx;
		if (dsourcex == 0)
			continue;

		// mask off priority bits and sign-extend
		screenx &= 0x0fff;
		if (screenx & 0x0800)
			screenx |= ~0x7ff;

		// adjust the horizontal placement
		screenx -= 64; // needs adjustment to left

		int numpixels = (44 * ROAD_TILE_SIZE << 16) / dsourcex;
		unsigned sourcex = 0;

		// crop left
		int clip_pixels = min_x - screenx;
		if (clip_pixels > 0)
		{
			numpixels -= clip_pixels;
			sourcex += dsourcex*clip_pixels;
			screenx = min_x;
		}

		// crop right
		clip_pixels = (screenx + numpixels) - (max_x);
		if (clip_pixels > 0)
			numpixels -= clip_pixels;

		// TBA: work out palette mapping for Final Lap, Suzuka

		// BUT: support transparent color for Thunder Ceptor
		UINT16 *dest = pTransDraw + y * nScreenWidth;

		if (c45_road_transparent_color != (UINT32)~0)
		{
			while (numpixels-- > 0)
			{
				int pen = source_gfx[sourcex >> 16];
			//	if (palette().pen_indirect(pen) != m_transparent_color)
				{
					dest[screenx] = pen;
				}
				screenx++;
				sourcex += dsourcex;
			}
		}
		else
		{
			while (numpixels-- > 0)
			{
				dest[screenx++] = source_gfx[sourcex >> 16];
				sourcex += dsourcex;
			}
		}
	}
}



#if 0
// call from save state
static void c45RoadGfxDecode()
{
	if (c45RoadRAM == NULL) return;

	INT32 Planes[2] = { 0, 8 };
	INT32 XOffs[16] = { STEP8(0, 1), STEP8(16, 1) };
	INT32 YOffs[16] = { STEP16(0, 32) };

	GfxDecode(0x0400, 2, 16, 16, Planes, XOffs, YOffs, 0x200, c45RoadRAM + 0x10000, c45RoadTiles);
}
#endif
