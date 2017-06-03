#include "tiles_generic.h"


UINT8 *c45RoadRAM; // external

static UINT8 *c45RoadTiles = NULL;
static UINT16 *c45RoadBitmap = NULL;
static UINT8 *c45RoadBuffer = NULL;
static UINT8 *c45RoadClut = NULL;
static INT32 c45_road_force_update = 0;
static INT32 frame_decoded;
static UINT32 c45_road_transparent_color = 0;
static UINT8 c45_temp_clut[0x100];

void c45RoadReset()
{
	c45_road_force_update = 1;
	frame_decoded = -1;
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

static void c45RoadGfxDecode()
{
	INT32 Planes[2] = { 0, 8 };
	INT32 XOffs[16] = { STEP8(0, 1), STEP8(16, 1) };
	INT32 YOffs[16] = { STEP16(0, 32) };

	GfxDecode(0x0400, 2, 16, 16, Planes, XOffs, YOffs, 0x200, c45RoadRAM + 0x10000, c45RoadTiles);
}

static void predraw_c45_road_tiles()
{
	UINT16 *ram = (UINT16*)c45RoadRAM;
//	UINT16 *buf = (UINT16*)c45RoadBuffer;

	for (INT32 offs = 0; offs < 64 * 512; offs++)
	{
		INT32 sx = (offs & 0x3f) * 16;
		INT32 sy = (offs / 0x40) * 16;

	//	if ((ram[offs] == buf[offs]) && c45_road_force_update == 0) continue;
	//	buf[offs] = ram[offs];

		INT32 code  = (ram[offs] & 0x3ff) * 0x100;
		INT32 color = ((ram[offs] >> 10) << 2) + 0xf00;

		UINT8 *gfx = c45RoadTiles + code;
		UINT16 *dst = c45RoadBitmap + (sy * 1024) + sx;

		for (INT32 y = 0; y < 16; y++)
		{
			for (INT32 x = 0; x < 16; x++)
			{
				dst[x] = c45RoadClut[gfx[x]] + color;
			}

			gfx += 16;
			dst += 1024;
		}
	}

	c45_road_force_update = 0;
}

#if 0
void c45RoadDraw(int pri)
{
	if (frame_decoded != (INT32)(nCurrentFrame & 1)) {
		c45RoadGfxDecode();
		predraw_c45_road_tiles();
	}
	frame_decoded = (nCurrentFrame & 1);

	INT32 min_x, min_y, max_x, max_y;

	GenericTilesGetClip(&min_x, &max_x, &min_y, &max_y);

	max_x--;
	max_y--;

	UINT16 *m_lineram = (UINT16*)(c45RoadRAM + 0x1fa00);
	UINT32 yscroll = m_lineram[0x3fe/2];

	static int ROAD_COLS = 64;
	static int ROAD_ROWS = 512;
	static int ROAD_TILE_SIZE = 16;
	static int ROAD_TILEMAP_WIDTH = ROAD_TILE_SIZE * ROAD_COLS;
	static int ROAD_TILEMAP_HEIGHT = ROAD_TILE_SIZE * ROAD_ROWS;

	for (int y = min_y; y <= max_y; y++)
	{
		int screenx = m_lineram[y + 15];
		if (pri != ((screenx & 0xf000) >> 12))
			continue;

		unsigned zoomx = m_lineram[0x400/2 + y + 15] & 0x3ff;
		if (zoomx == 0)
			continue;

		unsigned sourcey = m_lineram[0x200/2 + y + 15] + yscroll;
		const UINT16 *source_gfx = c45RoadBitmap + (sourcey & (ROAD_TILEMAP_HEIGHT - 1)) * ROAD_TILEMAP_WIDTH;
		unsigned dsourcex = (ROAD_TILEMAP_WIDTH << 16) / zoomx;
		if (dsourcex == 0)
			continue;

		screenx &= 0x0fff;
		if (screenx & 0x0800)
			screenx |= ~0x7ff;

		screenx -= 64; // needs adjustment to left

		int numpixels = (44 * (16 << 16)) / dsourcex;
		unsigned sourcex = 0;

		int clip_pixels = min_x - screenx;
		if (clip_pixels > 0)
		{
			numpixels -= clip_pixels;
			sourcex += dsourcex*clip_pixels;
			screenx = min_x;
		}

		clip_pixels = (screenx + numpixels) - (max_x + 1);
		if (clip_pixels > 0)
			numpixels -= clip_pixels;

		UINT16 *dest = pTransDraw + y * nScreenWidth;

		while (numpixels-- > 0)
		{
			int pen = source_gfx[sourcex >> 16];
			if (c45RoadClut != NULL)
				pen = (pen & ~0xff) | c45RoadClut[pen & 0xff];
			dest[screenx++] = pen;
			sourcex += dsourcex;
		}

	}
}
#endif


void c45RoadDraw(int pri)
{
	if (frame_decoded != (INT32)(nCurrentFrame & 1)) {
		c45RoadGfxDecode();
		predraw_c45_road_tiles();
	}
	frame_decoded = (nCurrentFrame & 1);

	INT32 min_x, min_y, max_x, max_y;

	min_x = min_y = 0;
	max_y = nScreenHeight - 1;
	max_x = nScreenWidth - 1;

	UINT16 *m_lineram = (UINT16*)(c45RoadRAM + 0x1fa00);

	static int ROAD_COLS = 64;
	static int ROAD_ROWS = 512;
	static int ROAD_TILE_SIZE = 16;
	static int ROAD_TILEMAP_WIDTH = ROAD_TILE_SIZE * ROAD_COLS;
	static int ROAD_TILEMAP_HEIGHT = ROAD_TILE_SIZE * ROAD_ROWS;

	static UINT8 *m_clut = c45RoadClut;
#define nullptr		NULL

	static UINT32 m_transparent_color = c45_road_transparent_color;





	unsigned yscroll = m_lineram[0x3fe/2];

	// loop over scanlines
	for (int y = min_y; y <= max_y; y++)
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
//		const uint16_t *source_gfx = &source_bitmap.pix(sourcey & (ROAD_TILEMAP_HEIGHT - 1));
		const uint16_t *source_gfx = c45RoadBitmap + (sourcey & (ROAD_TILEMAP_HEIGHT - 1)) * ROAD_TILEMAP_WIDTH;
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
		clip_pixels = (screenx + numpixels) - (max_x + 1);
		if (clip_pixels > 0)
			numpixels -= clip_pixels;

		// TBA: work out palette mapping for Final Lap, Suzuka

		// BUT: support transparent color for Thunder Ceptor
	//	uint16_t *dest = &bitmap.pix(y);
		uint16_t *dest = pTransDraw + y * nScreenWidth;
		if (m_transparent_color != ~0)
		{
			while (numpixels-- > 0)
			{
				int pen = source_gfx[sourcex >> 16];
			//	if (palette().pen_indirect(pen) != m_transparent_color)
				{
					if (m_clut != nullptr)
						pen = (pen & ~0xff) | m_clut[pen & 0xff];
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
				int pen = source_gfx[sourcex >> 16];
				if (m_clut != nullptr)
					pen = (pen & ~0xff) | m_clut[pen & 0xff];
				dest[screenx++] = pen;
				sourcex += dsourcex;
			}
		}
	}
}
