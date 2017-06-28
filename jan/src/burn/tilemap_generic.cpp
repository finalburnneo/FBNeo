#include "tiles_generic.h"

#define MAX_TILEMAPS	32	// number of tile maps allowed
#define MAX_GFX		32	// number of graphics data regions allowed

struct GenericTilemap {
	UINT8 initialized;
	INT32 (*pScan)(INT32 col, INT32 row);
	void (*pTile)(INT32 offs, INT32 *tile_gfx, INT32 *tile_code, INT32 *tile_color, UINT32 *tile_flags);
	UINT8 enable;
	UINT32 mwidth;
	UINT32 mheight;
	UINT32 twidth;
	UINT32 theight;
	UINT32 scroll_cols; // sy
	UINT32 scroll_rows; // sx
	INT32 scrollx;
	INT32 scrolly;
	INT32 *scrollx_table;
	INT32 *scrolly_table;
	INT32 priority;
	INT32 xoffset;
	INT32 yoffset;
	UINT32 flags;
	UINT8 transparent[256]; // 0 draw, 1 skip
	INT32 transcolor;
};

struct GenericTilemapGfx {
	UINT8 *gfxbase;		// pointer to graphics data
	INT32 depth;		// bits per pixel
	INT32 width;		// tile width
	INT32 height;		// tile height
	UINT32 gfx_len;		// full size of the tile data
	UINT32 code_mask;	// gfx_len / width / height
	UINT32 color_offset;	// is there a color offset for this graphics region?
	UINT32 color_mask;	// mask the color added to the pixels
};

static GenericTilemap maps[MAX_TILEMAPS];
static GenericTilemapGfx gfxdata[MAX_TILEMAPS];
static GenericTilemap *cur_map;

void GenericTilemapInit(INT32 which, INT32 (*pScan)(INT32 col, INT32 row), void (*pTile)(INT32 offs, INT32 *tile_gfx, INT32 *tile_code, INT32 *tile_color, UINT32 *tile_flags), UINT32 tile_width, UINT32 tile_height, UINT32 map_width, UINT32 map_height)
{
	if (Debug_GenericTilesInitted == 0) {
		bprintf (0, _T("Please call GenericTilesInit() before GenericTilemapInit()!\n"));
		return;
	}

	cur_map = &maps[which];

	memset (cur_map, 0, sizeof(GenericTilemap));

	cur_map->initialized = 1;

	// error
	if (pTile == NULL) {
		bprintf (0, _T("GenericTilemapInit %d pTile initializer cannot be NULL!\n"), which);
		return;
	}

	// error
	if (pScan == NULL) {
		bprintf (0, _T("GenericTilemapInit %d pScan initializer cannot be NULL!\n"), which);
		return;
	}

	// error
	if (map_width == 0 || map_height == 0 || tile_width == 0 || tile_height == 0) {
		bprintf (0, _T("GenericTilemapInit(%d, pScan, pTile, %d, %d, %d, %d) called with bad initializer!\n"), which, tile_width, tile_height, map_width, map_height);
		return;
	}

	// warning
	if (map_width > 2048 || map_height > 2048 || tile_width > 512 || tile_height > 512) {
		bprintf (0, _T("GenericTilemapInit(%d, pScan, pTile, %d, %d, %d, %d) called with likely bad initializer!\n"), which, tile_width, tile_height, map_width, map_height);
	}

	cur_map->pTile = pTile;
	cur_map->pScan = pScan;

	cur_map->enable = 1;

	cur_map->mwidth = map_width;
	cur_map->mheight = map_height;
	cur_map->twidth = tile_width;
	cur_map->theight = tile_height;

	cur_map->scroll_cols = 1;
	cur_map->scroll_rows = 1;
	cur_map->scrollx = 0;
	cur_map->scrolly = 0;
	cur_map->scrollx_table = NULL;
	cur_map->scrolly_table = NULL;

	cur_map->xoffset = 0;
	cur_map->yoffset = 0;

	cur_map->priority = -1;
	cur_map->flags = 0;
	memset (cur_map->transparent, 0, 256);
	cur_map->transcolor = 0xfff; // opaque by default
}

void GenericTilemapSetGfx(INT32 num, UINT8 *gfxbase, INT32 depth, INT32 tile_width, INT32 tile_height, INT32 gfxlen, UINT32 color_offset, UINT32 color_mask)
{
	if (Debug_GenericTilesInitted == 0) {
		bprintf (0, _T("GenericTilesInit must be called before GenericTilemapSetGfx!\n"));
		return;
	}

	// warning
	if (depth > 8 || tile_width > 512 || tile_height > 512 || gfxlen <= 0 || gfxlen > 0x10000000 || color_offset > 0x10000) {
		bprintf (0, _T("GenericTilemapSetGfx(%d, gfxbase, %d, %d, %d, 0x%x, 0x%x, 0x%s) called with likely bad initializer(s)!\n"), num, depth, tile_width, tile_height, gfxlen, color_offset, color_mask);
	}

	// error
	if (num < 0 || num >= MAX_GFX || tile_width <= 0 || tile_height <= 0 || gfxlen <= 0 || gfxbase == NULL) {
		bprintf (0, _T("GenericTilemapSetGfx(%d, gfxbase (%s), %d, %d, %d, 0x%x, 0x%x, 0x%s) called with bad initializer(s)!\n"), num, (gfxbase == NULL) ? _T("NULL") : _T("NON-NULL"), depth, tile_width, tile_height, gfxlen, color_offset, color_mask);
		return;
	}

	GenericTilemapGfx *ptr = &gfxdata[num];

	ptr->gfxbase = gfxbase;
	ptr->depth = depth;
	ptr->width = tile_width;
	ptr->height = tile_height;
	ptr->gfx_len = gfxlen;

	ptr->color_offset = color_offset;
	ptr->color_mask = color_mask;

#if 0
	UINT32 t = gfxlen / (tile_width * tile_height);

	// create a mask for the tile number (prevent crashes)
	for (UINT32 i = 1; i < (UINT32)(1 << 31); i <<= 1) {
		if (i >= t) {
			ptr->code_mask = i - 1;
			break;
		}
	}
#else
	// we're using much safer modulos for limiting the tile number
	ptr->code_mask = gfxlen / (tile_width * tile_height);
#endif
}

void GenericTilemapExit()
{
	// de-allocate any row/col scroll tables
	for (INT32 i = 0; i < MAX_TILEMAPS; i++) {
		cur_map = &maps[i];
		if (cur_map->scrolly_table) BurnFree(cur_map->scrolly_table);
		if (cur_map->scrollx_table) BurnFree(cur_map->scrollx_table);
	}

	// wipe everything else out
	memset (maps, 0, sizeof(maps));
	memset (gfxdata, 0, sizeof(gfxdata));
}

void GenericTilemapSetOffsets(INT32 which, INT32 x, INT32 y)
{
	if (which >= MAX_TILEMAPS) {
		bprintf (0, _T("GenericTilemapSetOffsets(%d, %d, %d); called with impossible tilemap!\n"), which, x, y);
		return;
	}

	// set offsets globally
	if (which == TMAP_GLOBAL)
	{
		INT32 counter = 0;

		for (INT32 i = 0; i < MAX_TILEMAPS; i++) {
			cur_map = &maps[i];
			if (cur_map->initialized) {
				cur_map->xoffset = x;
				cur_map->yoffset = y;

				counter++;
			}
		}

		if (counter == 0) {
			bprintf (0, _T("GenericTilemapSetOffsets(TMAP_GLOBAL, %d, %d); called, but there are no initialized tilemaps!\n"), x, y);
		}

		return;
	}

	// set offsets to a single tile map
	cur_map = &maps[which];

	if (cur_map->initialized == 0) {
		bprintf (0, _T("GenericTilemapSetOffsets(%d, %d, %d); called without initialized tilemap!\n"), which, x, y);
		return;
	}

	cur_map->xoffset = x;
	cur_map->yoffset = y;
}

void GenericTilemapSetTransparent(INT32 which, UINT32 transparent)
{
	if (which < 0 || which >= MAX_TILEMAPS) {
		bprintf (0, _T("GenericTilemapSetTransparent(%d, 0x%x); called with impossible tilemap number!\n"), which, transparent);
		return;
	}

	cur_map = &maps[which];

	if (cur_map->initialized == 0) {
		bprintf (0, _T("GenericTilemapSetTransparent(%d, 0x%x); called without initialized tilemap!\n"), which, transparent);
		return;
	}

	memset (cur_map->transparent, 0, 256);	// set all to opaque

	cur_map->transparent[transparent] = 1;	// one color opaque

	cur_map->transcolor = transparent;	// pass this to generic tile drawing
	cur_map->flags |= TMAP_TRANSPARENT;
}

void GenericTilemapSetTransMask(INT32 which, UINT16 transmask)
{
	if (which < 0 || which >= MAX_TILEMAPS) {
		bprintf (0, _T("GenericTilemapSetTransMask(%d, 0x%4.4x); called with impossible tilemap number!\n"), which, transmask);
		return;
	}

	cur_map = &maps[which];

	if (cur_map->initialized == 0) {
		bprintf (0, _T("GenericTilemapsSetTransMask(%d, 0x%4.4x); called without initialized tilemap!\n"), which, transmask);
		return;
	}

	memset (cur_map->transparent, 1, 256);

	for (INT32 i = 0; i < 16; i++) {
		if ((transmask & (1 << i)) == 0) {
			cur_map->transparent[i] = 0;
		}
	}

	cur_map->flags |= TMAP_TRANSMASK;
}

void GenericTilemapSetTransTable(INT32 which, INT32 color, INT32 transparent)
{
	if (which < 0 || which >= MAX_TILEMAPS) {
		bprintf (0, _T("GenericTilemapSetTransTable(%d, %d, %d); called with impossible tilemap number!\n"), which, color, transparent);
		return;
	}

	cur_map = &maps[which];

	if (cur_map->initialized == 0) {
		bprintf (0, _T("GenericTilemapSetTransTable(%d, %d, %d); called without initialized tilemap!\n"), which, color, transparent);
		return;
	}

	if (color < 0 || color >= 256) {
		bprintf (0, _T("GenericTilemapSetTransTable(%d, %d, %d); called with color entry outside of bounds (0-255)!\n"), which, color, transparent);
		return;
	}

	cur_map->transparent[color] = (transparent) ? 1 : 0;
	cur_map->flags |= TMAP_TRANSMASK;
}	

void GenericTilemapSetScrollX(INT32 which, INT32 scrollx)
{
	if (which < 0 || which >= MAX_TILEMAPS) {
		bprintf (0, _T("GenericTilemapSetScrollX(%d, %d); called with impossible tilemap!\n"), which, scrollx);
		return;
	}

	cur_map = &maps[which];

	if (cur_map->initialized == 0) {
		bprintf (0, _T("GenericTilemapSetScrollX(%d, %d); called without initialized tilemap!\n"), which, scrollx);
		return;
	}

	cur_map->scrollx = scrollx % (cur_map->twidth * cur_map->mwidth);
}

void GenericTilemapSetScrollY(INT32 which, INT32 scrolly)
{
	if (which < 0 || which >= MAX_TILEMAPS) {
		bprintf (0, _T("GenericTilemapSetScrollY(%d, %d); called with impossible tilemap!\n"), which, scrolly);
		return;
	}

	cur_map = &maps[which];

	if (cur_map->initialized == 0) {
		bprintf (0, _T("GenericTilemapSetScrollY(%d, %d); called without initialized tilemap!\n"), which, scrolly);
		return;
	}

	cur_map->scrolly = scrolly % (cur_map->theight * cur_map->mheight);
}

void GenericTilemapSetScrollCols(INT32 which, UINT32 cols)
{
	if (which < 0 || which >= MAX_TILEMAPS) {
		bprintf (0, _T("GenericTilemapSetScrollCols(%d, %d); called with impossible tilemap!\n"), which, cols);
		return;
	}

	cur_map = &maps[which];

	if (cur_map->initialized == 0) {
		bprintf (0, _T("GenericTilemapSetScrollCols(%d, %d); called without initialized tilemap!\n"), which, cols);
		return;
	}

	if (cols > (cur_map->mwidth * cur_map->twidth)) {
		bprintf (0, _T("GenericTilemapSetScrollCols(%d, %d); called with more cols than tilemap is wide (%d)!\n"), which, cols, cur_map->mwidth*cur_map->twidth);
		return;
	}

	// use scrolly instead
	if (cols <= 0) cols = 1;
	if (cols == 1) {
		cur_map->scroll_rows = cols;
		if (cur_map->scrolly_table) {
			BurnFree(cur_map->scrolly_table);
		}
		return;
	}

	if (cur_map->scroll_cols != cols)
	{
		cur_map->scroll_cols = cols;

		if (cur_map->scrolly_table) {
			BurnFree(cur_map->scrolly_table);
		}

		cur_map->scrolly_table = (INT32*)BurnMalloc(cols * sizeof(INT32));

		memset (cur_map->scrolly_table, 0, cols * sizeof(INT32));

		if (cols > cur_map->mwidth) {
			bprintf (0, _T("GenericTilemapSetScrollCols(%d, %d) line scroll not supported at this time (cols).\n"), which, cols);
		}
	}
}

void GenericTilemapSetScrollRows(INT32 which, UINT32 rows)
{
	if (which < 0 || which >= MAX_TILEMAPS) {
		bprintf (0, _T("GenericTilemapSetScrollRows(%d, %d); called with impossible tilemap!\n"), which, rows);
		return;
	}

	cur_map = &maps[which];

	if (cur_map->initialized == 0) {
		bprintf (0, _T("GenericTilemapSetScrollRows(%d, %d); called without initialized tilemap!\n"), which, rows);
		return;
	}

	if (rows > (cur_map->mheight * cur_map->theight)) {
		bprintf (0, _T("GenericTilemapSetScrollRows(%d, %d); called with more rows than tilemap is high (%d)!\n"), which, rows, cur_map->mheight*cur_map->theight);
		return;
	}

	// use scrollx instead
	if (rows <= 0) rows = 1;
	if (rows == 1) {
		cur_map->scroll_rows = rows;
		if (cur_map->scrollx_table) {
			BurnFree(cur_map->scrollx_table);
		}
		return;
	}

	if (cur_map->scroll_rows != rows)
	{
		cur_map->scroll_rows = rows;

		if (cur_map->scrollx_table) {
			BurnFree(cur_map->scrollx_table);
		}

		cur_map->scrollx_table = (INT32*)BurnMalloc(rows * sizeof(INT32));

		memset (cur_map->scrollx_table, 0, rows * sizeof(INT32));
	}
}

void GenericTilemapSetScrollCol(INT32 which, INT32 col, INT32 scroll)
{
	if (which < 0 || which >= MAX_TILEMAPS) {
		bprintf (0, _T("GenericTilemapSetScrollCol(%d, %d, %d); called with impossible tilemap!\n"), which, col, scroll);
		return;
	}

	cur_map = &maps[which];

	if (cur_map->initialized == 0) {
		bprintf (0, _T("GenericTilemapSetScrollCol(%d, %d, %d); called without initialized tilemap!\n"), which, col, scroll);
		return;
	}

	if ((INT32)cur_map->scroll_cols <= col || col < 0) {
		bprintf (0, _T("GenericTilemapSetScrollCol(%d, %d, %d); called with improper col value!\n"), which, col, scroll);
		return;
	}

	if (cur_map->scrolly_table != NULL) {
		cur_map->scrolly_table[col] = scroll % (cur_map->theight * cur_map->mheight);
	}
}

void GenericTilemapSetScrollRow(INT32 which, INT32 row, INT32 scroll)
{
	if (which < 0 || which >= MAX_TILEMAPS) {
		bprintf (0, _T("GenericTilemapSetScrollRow(%d, %d, %d); called with impossible tilemap!\n"), which, row, scroll);
		return;
	}

	cur_map = &maps[which];

	if (cur_map->initialized == 0) {
		bprintf (0, _T("GenericTilemapSetScrollRow(%d, %d, %d); called without initialized tilemap!\n"), which, row, scroll);
		return;
	}

	if ((INT32)cur_map->scroll_rows <= row || row < 0) {
		bprintf (0, _T("GenericTilemapSetScrollRow(%d, %d, %d); called with improper row value!\n"), which, row, scroll);
		return;
	}

	if (cur_map->scrollx_table != NULL) {
		cur_map->scrollx_table[row] = scroll % (cur_map->twidth * cur_map->mwidth);
	}
}

void GenericTilemapSetFlip(INT32 which, INT32 flip)
{
	if (which >= MAX_TILEMAPS) {
		bprintf (0, _T("GenericTilemapSetFlip(%d, %d); called with impossible tilemap!\n"), which, flip);
		return;
	}

	if (which == TMAP_GLOBAL) {
		INT32 counter = 0;
		for (INT32 i = 0; i < MAX_TILEMAPS; i++) {
			cur_map = &maps[i];
			if (cur_map->initialized) {
				cur_map->flags &= ~(TMAP_FLIPY|TMAP_FLIPX);
				cur_map->flags |= flip;

				counter++;
			}
		}

		if (counter == 0) {
			bprintf (0, _T("GenericTilemapSetFlip(TMAP_GLOBAL, %d); called, but there are no initialized tilemaps!\n"), flip);
		}

		return;
	}

	cur_map = &maps[which];

	if (cur_map->initialized == 0) {
		bprintf (0, _T("GenericTilemapSetFlip(%d, %d); called without initialized tilemap!\n"), which, flip);
		return;
	}

	cur_map->flags &= ~(TMAP_FLIPY|TMAP_FLIPX);
	cur_map->flags |= flip;
}

void GenericTilemapSetEnable(INT32 which, INT32 enable)
{
	if (which >= MAX_TILEMAPS) {
		bprintf (0, _T("GenericTilemapSetEnable(%d, %d); called with impossible tilemap!\n"), which, enable);
		return;
	}

	if (which == TMAP_GLOBAL) {
		INT32 counter = 0;
		for (INT32 i = 0; i < MAX_TILEMAPS; i++) {
			cur_map = &maps[i];
			if (cur_map->initialized) {
				cur_map->enable = enable ? 1 : 0;
				counter++;
			}
		}

		if (counter == 0) {
			bprintf (0, _T("GenericTilemapSetEnable(TMAP_GLOBAL, %d); called, but there are no initialized tilemaps!\n"), enable);
		}
		return;
	}

	cur_map = &maps[which];

	if (cur_map->initialized == 0) {
		bprintf (0, _T("GenericTilemapSetEnable(%d, %d); called without initialized tilemap!\n"), which, enable);
		return;
	}

	cur_map->enable = enable ? 1 : 0;
}

void GenericTilemapDraw(INT32 which, UINT16 *Bitmap, INT32 priority)
{
	if (Bitmap == NULL) {
		bprintf (0, _T("GenericTilemapDraw(%d, Bitmap, %d); called without initialized Bitmap!\n"), which, priority);
		return;
	}

	cur_map = &maps[which];

	if (cur_map->initialized == 0) {
		bprintf (0, _T("GenericTilemapDraw(%d, Bitmap, %d); called without initialized tilemap!\n"), which, priority);
		return;
	}

	if (cur_map->enable == 0) { // layer disabled!
		return;
	}

	INT32 minx, maxx, miny, maxy;
	GenericTilesGetClip(&minx, &maxx, &miny, &maxy);

	// check clipping and fix clipping sizes if out of bounds
	if (minx < 0 || maxx > nScreenWidth || miny < 0 || maxy > nScreenHeight) {
		bprintf (0, _T("GenericTilemapDraw(%d, Bitmap, %d) called with improper clipping values (%d, %d, %d, %d)!"), which, priority, minx, maxx, miny, maxy);
		bprintf (0, _T("pPrioDraw is %d pixels wide and %d pixels high!\n"), nScreenWidth, nScreenHeight);

		if (minx < 0) minx = 0;
		if (maxx >= nScreenWidth) maxx = nScreenWidth;
		if (miny < 0) miny = 0;
		if (maxy >= nScreenHeight) maxy = nScreenHeight;
	}

	INT32 opaque = priority & TMAP_FORCEOPAQUE;

	INT32 tgroup = (priority >> 8) & 0xff;
	priority &= 0xff;

	// line / column scroll
	if ((cur_map->scrollx_table != NULL) && (cur_map->scroll_rows > cur_map->mheight))
	{
		INT32 bitmap_width = maxx - minx;
		UINT16 *dest = Bitmap;
		UINT8 *prio = pPrioDraw;

		for (INT32 y = miny; y < maxy; y++, prio += bitmap_width) // line by line
		{
			INT32 scrolly = (cur_map->scrolly + y + cur_map->yoffset) % (cur_map->mheight * cur_map->theight);

			INT32 scrollx = cur_map->scrollx_table[(scrolly * cur_map->scroll_rows) / (cur_map->mheight * cur_map->theight)] - cur_map->xoffset;

			scrollx %= (cur_map->twidth * cur_map->mwidth);

			INT32 row = scrolly / cur_map->theight;

			INT32 scry = scrolly % (cur_map->theight);
			INT32 scrx = scrollx % (cur_map->twidth);

			INT32 sy = y;
			if (cur_map->flags & TMAP_FLIPY) {
				sy = ((maxy - miny) - cur_map->theight) - sy;
			}

			dest = Bitmap + sy * nScreenWidth;
			prio = pPrioDraw + sy * nScreenWidth;

			for (UINT32 x = 0; x < bitmap_width + cur_map->twidth; x+=cur_map->twidth)
			{
				INT32 sx = x;
				INT32 col = ((x + scrollx) % (cur_map->mwidth * cur_map->twidth)) / cur_map->twidth;

				INT32 code = 0, color = 0, group = 0, gfxnum = 0;
				UINT32 flags = 0;

				cur_map->pTile(cur_map->pScan(col,row), &gfxnum, &code, &color, &flags);

				if (flags & TILE_SKIP) continue; // skip this tile

				if (flags & TILE_GROUP_ENABLE) {
					group = (flags >> 16) & 0xff;

					if (group != tgroup) {
						continue;
					}
				}

				GenericTilemapGfx *gfx = &gfxdata[gfxnum];

				if (gfx->gfxbase == NULL) {
					bprintf (0,_T("GenericTilemapDraw(%d) gfx[%d] not initialized!\n"), which, gfxnum);
					continue;
				}

				color = ((color & gfx->color_mask) << gfx->depth) + gfx->color_offset;
				code %= gfx->code_mask;

				INT32 flipx = flags & TILE_FLIPX; 
				INT32 flipy = flags & TILE_FLIPY;

				if (cur_map->flags & TMAP_FLIPY) {
					flipy ^= TILE_FLIPY;
				}

				INT32 scy;
				if (flipy)
					scy = (cur_map->theight - 1) - scry;
				else
					scy = scry;

				if (cur_map->flags & TMAP_FLIPX) {
					sx = ((maxx - minx) - cur_map->twidth) - sx;
					scrx = ((cur_map->twidth) - 1) - scrx;
					flipx ^= TILE_FLIPX;
				}

				UINT8 *gfxsrc = gfx->gfxbase + (code * cur_map->twidth * cur_map->theight) + (scy * cur_map->twidth);

				if (flipx)
				{
					INT32 flip_wide = cur_map->twidth - 1;

					for (UINT32 dx = 0; dx < cur_map->twidth; dx++)
					{
						INT32 dst = (sx + dx) - scrx;
						if (dst < minx || dst >= maxx) continue;

						if (cur_map->transparent[gfxsrc[flip_wide - dx]] == 0) {
							dest[dst] = color + gfxsrc[flip_wide - dx];
							prio[dst] = priority;
						}
					}
				}
				else
				{
					for (UINT32 dx = 0; dx < cur_map->twidth; dx++)
					{
						INT32 dst = (sx + dx) - scrx;
						if (dst < minx || dst >= maxx) continue;

						if (cur_map->transparent[gfxsrc[dx]] == 0) {
							dest[dst] = color + gfxsrc[dx];
							prio[dst] = priority;
						}
					}
				}
			}
		}

		return;
	}
	// scrollx and scrolly
	else if (cur_map->scroll_rows <= 1 && cur_map->scroll_cols <= 1) // one scroll row and column. Fast!
	{
		INT32 syshift = ((cur_map->scrolly - cur_map->yoffset) % cur_map->theight);
		INT32 scrolly = ((cur_map->scrolly - cur_map->yoffset) / cur_map->theight) * cur_map->theight;

		INT32 sxshift = ((cur_map->scrollx - cur_map->xoffset) % cur_map->twidth);
		INT32 scrollx = ((cur_map->scrollx - cur_map->xoffset) / cur_map->twidth) * cur_map->twidth;

		for (INT32 y = miny; y < (INT32)(maxy + cur_map->theight); y += cur_map->theight)
		{
			INT32 syy = (y + scrolly) % (cur_map->theight * cur_map->mheight);

			for (INT32 x = minx; x < (INT32)(maxx + cur_map->twidth); x += cur_map->twidth)
			{
				INT32 sxx = (x + scrollx) % (cur_map->twidth * cur_map->mwidth);

				INT32 code = 0, color = 0, group = 0, gfxnum = 0;
				UINT32 flags = 0;
			
				cur_map->pTile(cur_map->pScan(sxx/cur_map->twidth,syy/cur_map->theight), &gfxnum, &code, &color, &flags);

				if (flags & TILE_SKIP) continue; // skip this tile

				if (flags & TILE_GROUP_ENABLE) {
					group = (flags >> 16) & 0xff;
	
					if (group != tgroup) {
						continue;
					}
				}

				GenericTilemapGfx *gfx = &gfxdata[gfxnum];

				if (gfx->gfxbase == NULL) {
					bprintf (0,_T("GenericTilemapDraw(%d) gfx[%d] not initialized!\n"), which, gfxnum);
					continue;
				}

				color &= gfx->color_mask;
				code %= gfx->code_mask;

				INT32 sy = y - syshift;
				INT32 sx = x - sxshift;

				INT32 flipx = flags & TILE_FLIPX; 
				INT32 flipy = flags & TILE_FLIPY;

				if (cur_map->flags & TMAP_FLIPY) {
					sy = ((maxy - miny) - cur_map->theight) - sy;
					flipy ^= TILE_FLIPY;
				}

				if (cur_map->flags & TMAP_FLIPX) {
					sx = ((maxx - minx) - cur_map->twidth) - sx;
					flipx ^= TILE_FLIPX;
				}

				// skip tiles that are out of the visible area
				if ((sx >= maxx) || (sy >= maxy) || (sx < (INT32)(minx - (cur_map->twidth - 1))) || (sy < (INT32)(miny - (cur_map->theight - 1)))) {
					continue;
				}

				if (sx < minx || sy < miny || sx >= (INT32)(maxx - cur_map->twidth - 1) || sy >= (INT32)(maxy - cur_map->theight - 1))
				{
					if ((cur_map->flags & TMAP_TRANSPARENT) && (flags & TILE_OPAQUE) == 0 && opaque == 0)
					{
						if (flipy) {
							if (flipx) {
								RenderCustomTile_Prio_Mask_FlipXY_Clip(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transcolor, gfx->color_offset, priority, gfx->gfxbase);
							} else {
								RenderCustomTile_Prio_Mask_FlipY_Clip(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transcolor, gfx->color_offset, priority, gfx->gfxbase);
							}
						} else {
							if (flipx) {
								RenderCustomTile_Prio_Mask_FlipX_Clip(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transcolor, gfx->color_offset, priority, gfx->gfxbase);
							} else {
								RenderCustomTile_Prio_Mask_Clip(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transcolor, gfx->color_offset, priority, gfx->gfxbase);
							}
						}
					}
					else if ((cur_map->flags & TMAP_TRANSMASK) && (flags & TILE_OPAQUE) == 0 && opaque == 0)
					{
						if (flipy) {
							if (flipx) {
								RenderCustomTile_Prio_TransMask_FlipXY_Clip(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent, gfx->color_offset, priority, gfx->gfxbase);
							} else {
								RenderCustomTile_Prio_TransMask_FlipY_Clip(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent, gfx->color_offset, priority, gfx->gfxbase);
							}
						} else {
							if (flipx) {
								RenderCustomTile_Prio_TransMask_FlipX_Clip(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent, gfx->color_offset, priority, gfx->gfxbase);
							} else {
								RenderCustomTile_Prio_TransMask_Clip(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent, gfx->color_offset, priority, gfx->gfxbase);
							}
						}	
					}
					else
					{
						if (flipy) {
							if (flipx) {
								RenderCustomTile_Prio_FlipXY_Clip(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, gfx->color_offset, priority, gfx->gfxbase);
							} else {
								RenderCustomTile_Prio_FlipY_Clip(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, gfx->color_offset, priority, gfx->gfxbase);
							}
						} else {
							if (flipx) {
								RenderCustomTile_Prio_FlipX_Clip(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, gfx->color_offset, priority, gfx->gfxbase);
							} else {
								RenderCustomTile_Prio_Clip(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, gfx->color_offset, priority, gfx->gfxbase);
							}
						}
					}
				} 
				else
				{
					if ((cur_map->flags & TMAP_TRANSPARENT) && (flags & TILE_OPAQUE) == 0 && opaque == 0)
					{
						if (flipy) {
							if (flipx) {
								RenderCustomTile_Prio_Mask_FlipXY(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transcolor, gfx->color_offset, priority, gfx->gfxbase);
							} else {
								RenderCustomTile_Prio_Mask_FlipY(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transcolor, gfx->color_offset, priority, gfx->gfxbase);
							}
						} else {
							if (flipx) {
								RenderCustomTile_Prio_Mask_FlipX(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transcolor, gfx->color_offset, priority, gfx->gfxbase);
							} else {
								RenderCustomTile_Prio_Mask(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transcolor, gfx->color_offset, priority, gfx->gfxbase);
							}
						}
					}
					else if ((cur_map->flags & TMAP_TRANSMASK) && (flags & TILE_OPAQUE) == 0 && opaque == 0)
					{
						if (flipy) {
							if (flipx) {
								RenderCustomTile_Prio_TransMask_FlipXY(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent, gfx->color_offset, priority, gfx->gfxbase);
							} else {
								RenderCustomTile_Prio_TransMask_FlipY(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent, gfx->color_offset, priority, gfx->gfxbase);
							}
						} else {
							if (flipx) {
								RenderCustomTile_Prio_TransMask_FlipX(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent, gfx->color_offset, priority, gfx->gfxbase);
							} else {
								RenderCustomTile_Prio_TransMask(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent, gfx->color_offset, priority, gfx->gfxbase);
							}
						}	
					}
					else
					{
						if (flipy) {
							if (flipx) {
								RenderCustomTile_Prio_FlipXY(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, gfx->color_offset, priority, gfx->gfxbase);
							} else {
								RenderCustomTile_Prio_FlipY(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, gfx->color_offset, priority, gfx->gfxbase);
							}
						} else {
							if (flipx) {
								RenderCustomTile_Prio_FlipX(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, gfx->color_offset, priority, gfx->gfxbase);
							} else {
								RenderCustomTile_Prio(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, gfx->color_offset, priority, gfx->gfxbase);
							}
						}
					}
				}
			}
		}

		return;
	}

	// column / row scroll (greater >= tile size)
	for (UINT32 offs = 0; offs < (cur_map->mwidth * cur_map->mheight); offs++)
	{
		INT32 col = offs % cur_map->mwidth; //x
		INT32 row = offs / cur_map->mwidth; //y

		INT32 code = 0, color = 0, group = 0, gfxnum = 0;
		UINT32 flags = 0;
			
		cur_map->pTile(cur_map->pScan(col,row), &gfxnum, &code, &color, &flags);

		if (flags & TILE_SKIP) continue; // skip this tile

		if (flags & TILE_GROUP_ENABLE) {
			group = (flags >> 16) & 0xff;

			if (group != tgroup) {
				continue;
			}
		}

		GenericTilemapGfx *gfx = &gfxdata[gfxnum];

		if (gfx->gfxbase == NULL) {
			bprintf (0,_T("GenericTilemapDraw(%d) gfx[%d] not initialized!\n"), which, gfxnum);
			continue;
		}

		color &= gfx->color_mask;
		code %= gfx->code_mask;

		INT32 sx = col * cur_map->twidth;
		INT32 sy = row * cur_map->theight;

		if (cur_map->scroll_rows <= 1) {
			sx -= cur_map->scrollx;
		} else {
			INT32 r = (row * cur_map->scroll_rows) / cur_map->mheight;
			sx -= (cur_map->scrollx + cur_map->scrollx_table[r]) % (cur_map->twidth * cur_map->mwidth);
		}

		if (cur_map->scroll_cols <= 1) {
			sy -= cur_map->scrolly;
		} else {
			INT32 r = (col * cur_map->scroll_cols) / cur_map->mwidth;
			sy -= (cur_map->scrolly + cur_map->scrolly_table[r]) % (cur_map->theight * cur_map->mheight);
		}

		sx += cur_map->xoffset;
		sy += cur_map->yoffset;

		if (sx < (INT32)(1-cur_map->twidth)) sx += cur_map->twidth * cur_map->mwidth;
		if (sy < (INT32)(1-cur_map->theight)) sy += cur_map->theight * cur_map->mheight;

		INT32 flipx = flags & TILE_FLIPX; 
		INT32 flipy = flags & TILE_FLIPY;

		if (cur_map->flags & TMAP_FLIPY) {
			sy = ((maxy - miny) - cur_map->theight) - sy;
			flipy ^= TILE_FLIPY;
		}

		if (cur_map->flags & TMAP_FLIPX) {
			sx = ((maxx - minx) - cur_map->twidth) - sx;
			flipx ^= TILE_FLIPX;
		}

		// skip tiles that are out of the visible area
		if ((sx >= maxx) || (sy >= maxy) || (sx < (INT32)(minx - (cur_map->twidth - 1))) || (sy < (INT32)(miny - (cur_map->theight - 1)))) {
			continue;
		}

		if (sx < minx || sy < miny || sx >= (INT32)(maxx - cur_map->twidth - 1) || sy >= (INT32)(maxy - cur_map->theight - 1))
		{
			if ((cur_map->flags & TMAP_TRANSPARENT) && (flags & TILE_OPAQUE) == 0 && opaque == 0)
			{
				if (flipy) {
					if (flipx) {
						RenderCustomTile_Prio_Mask_FlipXY_Clip(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transcolor, gfx->color_offset, priority, gfx->gfxbase);
					} else {
						RenderCustomTile_Prio_Mask_FlipY_Clip(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transcolor, gfx->color_offset, priority, gfx->gfxbase);
					}
				} else {
					if (flipx) {
						RenderCustomTile_Prio_Mask_FlipX_Clip(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transcolor, gfx->color_offset, priority, gfx->gfxbase);
					} else {
						RenderCustomTile_Prio_Mask_Clip(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transcolor, gfx->color_offset, priority, gfx->gfxbase);
					}
				}
			}
			else if ((cur_map->flags & TMAP_TRANSMASK) && (flags & TILE_OPAQUE) == 0 && opaque == 0)
			{
				if (flipy) {
					if (flipx) {
						RenderCustomTile_Prio_TransMask_FlipXY_Clip(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent, gfx->color_offset, priority, gfx->gfxbase);
					} else {
						RenderCustomTile_Prio_TransMask_FlipY_Clip(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent, gfx->color_offset, priority, gfx->gfxbase);
					}
				} else {
					if (flipx) {
						RenderCustomTile_Prio_TransMask_FlipX_Clip(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent, gfx->color_offset, priority, gfx->gfxbase);
					} else {
						RenderCustomTile_Prio_TransMask_Clip(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent, gfx->color_offset, priority, gfx->gfxbase);
					}
				}	
			}
			else
			{
				if (flipy) {
					if (flipx) {
						RenderCustomTile_Prio_FlipXY_Clip(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, gfx->color_offset, priority, gfx->gfxbase);
					} else {
						RenderCustomTile_Prio_FlipY_Clip(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, gfx->color_offset, priority, gfx->gfxbase);
					}
				} else {
					if (flipx) {
						RenderCustomTile_Prio_FlipX_Clip(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, gfx->color_offset, priority, gfx->gfxbase);
					} else {
						RenderCustomTile_Prio_Clip(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, gfx->color_offset, priority, gfx->gfxbase);
					}
				}
			}
		} 
		else
		{
			if ((cur_map->flags & TMAP_TRANSPARENT) && (flags & TILE_OPAQUE) == 0 && opaque == 0)
			{
				if (flipy) {
					if (flipx) {
						RenderCustomTile_Prio_Mask_FlipXY(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transcolor, gfx->color_offset, priority, gfx->gfxbase);
					} else {
						RenderCustomTile_Prio_Mask_FlipY(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transcolor, gfx->color_offset, priority, gfx->gfxbase);
					}
				} else {
					if (flipx) {
						RenderCustomTile_Prio_Mask_FlipX(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transcolor, gfx->color_offset, priority, gfx->gfxbase);
					} else {
						RenderCustomTile_Prio_Mask(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transcolor, gfx->color_offset, priority, gfx->gfxbase);
					}
				}
			}
			else if ((cur_map->flags & TMAP_TRANSMASK) && (flags & TILE_OPAQUE) == 0 && opaque == 0)
			{
				if (flipy) {
					if (flipx) {
						RenderCustomTile_Prio_TransMask_FlipXY(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent, gfx->color_offset, priority, gfx->gfxbase);
					} else {
						RenderCustomTile_Prio_TransMask_FlipY(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent, gfx->color_offset, priority, gfx->gfxbase);
					}
				} else {
					if (flipx) {
						RenderCustomTile_Prio_TransMask_FlipX(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent, gfx->color_offset, priority, gfx->gfxbase);
					} else {
						RenderCustomTile_Prio_TransMask(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent, gfx->color_offset, priority, gfx->gfxbase);
					}
				}	
			}
			else
			{
				if (flipy) {
					if (flipx) {
						RenderCustomTile_Prio_FlipXY(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, gfx->color_offset, priority, gfx->gfxbase);
					} else {
						RenderCustomTile_Prio_FlipY(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, gfx->color_offset, priority, gfx->gfxbase);
					}
				} else {
					if (flipx) {
						RenderCustomTile_Prio_FlipX(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, gfx->color_offset, priority, gfx->gfxbase);
					} else {
						RenderCustomTile_Prio(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, gfx->color_offset, priority, gfx->gfxbase);
					}
				}
			}
		}
	}
}

tilemap_scan( scan_rows )
{
	return (cur_map->mwidth * row) + col;
}

tilemap_scan( scan_cols )
{
	return (cur_map->mheight * col) + row;
}


