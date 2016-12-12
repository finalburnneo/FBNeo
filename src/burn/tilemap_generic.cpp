#include "tiles_generic.h"

#define MAX_TILEMAPS	32

struct GenericTilemap {
	INT32 (*pScan)(INT32 cols, INT32 rows, INT32 col, INT32 row);
	void (*pTile)(INT32 offs, INT32 *gfx, INT32 *code, INT32 *color, UINT32 *flags);
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
};

struct GenericTilemapGfx {
	UINT8 *gfxbase;
	INT32 depth;
	INT32 width;
	INT32 height;
	UINT32 gfx_len;
	UINT32 code_mask;
	UINT32 color_offset;
	UINT32 color_mask;
};

static GenericTilemap maps[MAX_TILEMAPS];
static GenericTilemapGfx gfxdata[MAX_TILEMAPS];
static GenericTilemap *cur_map;

void GenericTilemapInit(INT32 which, INT32 (*pScan)(INT32 cols, INT32 rows, INT32 col, INT32 row), void (*pTile)(INT32 offs, INT32 *gfx, INT32 *code, INT32 *color, UINT32 *flags), UINT32 tile_width, UINT32 tile_height, UINT32 map_width, UINT32 map_height)
{
	cur_map = &maps[which];

	memset (cur_map, 0, sizeof(GenericTilemap));

	cur_map->pTile = pTile;
	cur_map->pScan = pScan;

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
}

void GenericTilemapSetGfx(INT32 num, UINT8 *gfxbase, INT32 depth, INT32 tile_width, INT32 tile_height, INT32 gfxlen, UINT32 color_offset, UINT32 color_mask)
{
	GenericTilemapGfx *ptr = &gfxdata[num];

	ptr->gfxbase = gfxbase;
	ptr->depth = depth;
	ptr->width = tile_width;
	ptr->height = tile_height;
	ptr->gfx_len = gfxlen;

	ptr->color_offset = color_offset;
	ptr->color_mask = color_mask;

	UINT32 t = gfxlen / (tile_width * tile_height);

	// create a mask for the tile number (prevent crashes)
	for (UINT32 i = 1; i < (UINT32)(1 << 31); i <<= 1) {
		if (i >= t) {
			ptr->code_mask = i - 1;
			break;
		}
	}
}

void GenericTilemapExit()
{
	memset (maps, 0, sizeof(maps));
	memset (gfxdata, 0, sizeof(gfxdata));
}

void GenericTilemapSetOffsets(INT32 which, INT32 x, INT32 y)
{
	cur_map = &maps[which];

	cur_map->xoffset = x;
	cur_map->yoffset = y;
}

void GenericTilemapSetTransparent(INT32 which, UINT32 transparent)
{
	cur_map = &maps[which];
	cur_map->transparent[0] = transparent;
	cur_map->flags |= TMAP_TRANSPARENT;
}

void GenericTilemapSetTransMask(INT32 which, UINT16 transmask)
{
	cur_map = &maps[which];

	memset (cur_map->transparent, 1, 256);

	for (INT32 i = 0; i < 16; i++) {
		if ((transmask & (1 << i)) == 0) {
			cur_map->transparent[i] = 0;
		}
	}

	cur_map->flags |= TMAP_TRANSMASK;
}

void GenericTilemapSetScrollX(INT32 which, INT32 scrollx)
{
	cur_map = &maps[which];
	cur_map->scrollx = scrollx % (cur_map->twidth * cur_map->mwidth);
}

void GenericTilemapSetScrollY(INT32 which, INT32 scrolly)
{
	cur_map = &maps[which];
	cur_map->scrolly = scrolly % (cur_map->theight * cur_map->mheight);
}

void GenericTilemapSetScrollCols(INT32 which, UINT32 cols)
{
	cur_map = &maps[which];
	cur_map->scroll_cols = cols;
	cur_map->scrolly_table = (INT32*)BurnMalloc(cols * sizeof(INT32));

	memset (cur_map->scrolly_table, 0, cols * sizeof(INT32));

	if (cols > cur_map->mwidth) {
		bprintf (0, _T("Line scroll not supported at this time (cols).\n"));
	}
}

void GenericTilemapSetScrollRows(INT32 which, UINT32 rows)
{
	cur_map = &maps[which];
	cur_map->scroll_rows = rows;
	cur_map->scrollx_table = (INT32*)BurnMalloc(rows * sizeof(INT32));

	memset (cur_map->scrollx_table, 0, rows * sizeof(INT32));

	if (rows > cur_map->mheight) {
		bprintf (0, _T("Setting line scroll (%d rows).\n"), rows);
	}
}

void GenericTilemapSetScrollRow(INT32 which, INT32 row, INT32 scroll)
{
	cur_map = &maps[which];
	if (cur_map->scrollx_table != NULL) {
		cur_map->scrollx_table[row] = scroll;
	}
}

void GenericTilemapSetScrollCol(INT32 which, INT32 col, INT32 scroll)
{
	cur_map = &maps[which];
	if (cur_map->scrolly_table != NULL) {
		cur_map->scrolly_table[col] = scroll;
	}
}

void GenericTilemapSetFlip(INT32 which, INT32 flip)
{
	cur_map = &maps[which];
	cur_map->flags &= ~(TMAP_FLIPY|TMAP_FLIPX);
	cur_map->flags |= flip;
}

void GenericTilemapDraw(INT32 which, UINT16 *Bitmap, INT32 priority)
{
	cur_map = &maps[which];

	INT32 minx, maxx, miny, maxy;
	GenericTilesGetClip(&minx, &maxx, &miny, &maxy);

	INT32 tgroup = (priority >> 8) & 0xff;
	priority &= 0xff;

	if ((cur_map->scrollx_table != NULL) && (cur_map->scroll_rows > cur_map->mheight))
	{
		INT32 transparent = 0xffff;
		if (cur_map->flags & TMAP_TRANSPARENT) transparent = cur_map->transparent[0];
		INT32 bitmap_width = maxx - minx;
		UINT16 *dest = Bitmap;
		UINT8 *prio = pPrioDraw;
	
		for (INT32 y = miny; y < maxy; y++, dest += bitmap_width, prio += bitmap_width) // line by line
		{
			INT32 scrolly = (cur_map->scrolly + y + cur_map->yoffset) % (cur_map->mheight * cur_map->theight);
	
			INT32 scrollx = cur_map->scrollx_table[(scrolly * cur_map->scroll_rows) / cur_map->mheight] + cur_map->xoffset;

			INT32 row = scrolly / cur_map->theight;

			INT32 scry = scrolly % (cur_map->theight);
			INT32 scrx = scrollx % (cur_map->twidth);

			for (UINT32 x = 0; x < bitmap_width + (cur_map->twidth-1); x+=cur_map->twidth)
			{
				INT32 col = ((x + scrollx) % (cur_map->mwidth * cur_map->twidth)) / cur_map->twidth;

				INT32 code = 0, color = 0, flip = 0, group = 0, gfxnum = 0;
				UINT32 flags = 0;

				cur_map->pTile(cur_map->pScan(cur_map->mwidth,cur_map->mheight,col,row), &gfxnum, &code, &color, &flags);

				if (flags & TILE_SKIP) continue; // skip this tile

				flip = flags & 3;
				if (flags & TILE_GROUP_ENABLE) group = flags >> 16;
				if (cur_map->flags & TMAP_TRANSMASK) {
					if (group != tgroup) {
						continue;
					}
				}

				GenericTilemapGfx *gfx = &gfxdata[gfxnum];
			
				color &= gfx->color_mask;
				color = (color << gfx->depth) + gfx->color_offset;

				code &= gfx->code_mask;

				INT32 scy = scry;
				if (flip & 2) scry = (cur_map->theight - 1) - scry;

				INT32 flipx = 0;
				if (flip & 1) flipx = (cur_map->twidth - 1);
	
				UINT8 *gfxsrc = gfx->gfxbase + (code * cur_map->twidth * cur_map->theight) + (scy * cur_map->twidth);
	
				for (UINT32 dx = 0; dx < cur_map->twidth; dx++)
				{
					INT32 dst = (x + dx) - scrx;
					if (dst < minx || dst >= maxx) continue;
	
					if (gfxsrc[dx] != transparent) {
						dest[dst] = color + gfxsrc[dx];
						prio[dst] = priority;
					}
				}
			}
		}

		return;
	}

	for (UINT32 offs = 0; offs < (cur_map->mwidth * cur_map->mheight); offs++)
	{
		INT32 col = offs % cur_map->mwidth; //x
		INT32 row = offs / cur_map->mwidth; //y

		INT32 code = 0, color = 0, flip = 0, group = 0, gfxnum = 0;
		UINT32 flags = 0;
			
		cur_map->pTile(cur_map->pScan(cur_map->mwidth,cur_map->mheight,col,row), &gfxnum, &code, &color, &flags);

		if (flags & TILE_SKIP) continue; // skip this tile

		if (flags & TILE_GROUP_ENABLE) flip = flags & 3;
		group = flags >> 16;

		if (cur_map->flags & TMAP_TRANSMASK) {
			if (group != tgroup) {
				continue;
			}
		}

		GenericTilemapGfx *gfx = &gfxdata[gfxnum];

		color &= gfx->color_mask;
		code &= gfx->code_mask;

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

		if (cur_map->flags & TMAP_FLIPY) {
			sy = ((maxy - miny) - cur_map->theight) - sy;
			flip ^= TMAP_FLIPY;
		}

		if (cur_map->flags & TMAP_FLIPX) {
			sx = ((maxx - minx) - cur_map->twidth) - sx;
			flip ^= TMAP_FLIPX;
		}

		if (sx >= maxx || sy >= maxy) continue;

		if (sx < minx || sy < miny || sx >= (INT32)(maxx - cur_map->twidth - 1) || sy >= (INT32)(maxy - cur_map->theight - 1))
		{
			if ((cur_map->flags & TMAP_TRANSPARENT) && (flags & TILE_OPAQUE) == 0)
			{
				if (flip & TMAP_FLIPY) {
					if (flip & TMAP_FLIPX) {
						RenderCustomTile_Prio_Mask_FlipXY_Clip(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent[0], gfx->color_offset, priority, gfx->gfxbase);
					} else {
						RenderCustomTile_Prio_Mask_FlipY_Clip(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent[0], gfx->color_offset, priority, gfx->gfxbase);
					}
				} else {
					if (flip & TMAP_FLIPX) {
						RenderCustomTile_Prio_Mask_FlipX_Clip(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent[0], gfx->color_offset, priority, gfx->gfxbase);
					} else {
						RenderCustomTile_Prio_Mask_Clip(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent[0], gfx->color_offset, priority, gfx->gfxbase);
					}
				}
			}
			else if ((cur_map->flags & TMAP_TRANSMASK) && (flags & TILE_OPAQUE) == 0)
			{
				if (flip & TMAP_FLIPY) {
					if (flip & TMAP_FLIPX) {
						RenderCustomTile_Prio_TransMask_FlipXY_Clip(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent, gfx->color_offset, priority, gfx->gfxbase);
					} else {
						RenderCustomTile_Prio_TransMask_FlipY_Clip(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent, gfx->color_offset, priority, gfx->gfxbase);
					}
				} else {
					if (flip & TMAP_FLIPX) {
						RenderCustomTile_Prio_TransMask_FlipX_Clip(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent, gfx->color_offset, priority, gfx->gfxbase);
					} else {
						RenderCustomTile_Prio_TransMask_Clip(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent, gfx->color_offset, priority, gfx->gfxbase);
					}
				}	
			}
			else
			{
				if (flip & TMAP_FLIPY) {
					if (flip & TMAP_FLIPX) {
						RenderCustomTile_Prio_FlipXY_Clip(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, gfx->color_offset, priority, gfx->gfxbase);
					} else {
						RenderCustomTile_Prio_FlipY_Clip(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, gfx->color_offset, priority, gfx->gfxbase);
					}
				} else {
					if (flip & TMAP_FLIPX) {
						RenderCustomTile_Prio_FlipX_Clip(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, gfx->color_offset, priority, gfx->gfxbase);
					} else {
						RenderCustomTile_Prio_Clip(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, gfx->color_offset, priority, gfx->gfxbase);
					}
				}
			}
		} 
		else
		{
			if ((cur_map->flags & TMAP_TRANSPARENT) && (flags & TILE_OPAQUE) == 0)
			{
				if (flip & TMAP_FLIPY) {
					if (flip & TMAP_FLIPX) {
						RenderCustomTile_Prio_Mask_FlipXY(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent[0], gfx->color_offset, priority, gfx->gfxbase);
					} else {
						RenderCustomTile_Prio_Mask_FlipY(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent[0], gfx->color_offset, priority, gfx->gfxbase);
					}
				} else {
					if (flip & TMAP_FLIPX) {
						RenderCustomTile_Prio_Mask_FlipX(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent[0], gfx->color_offset, priority, gfx->gfxbase);
					} else {
						RenderCustomTile_Prio_Mask(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent[0], gfx->color_offset, priority, gfx->gfxbase);
					}
				}
			}
			else if ((cur_map->flags & TMAP_TRANSMASK) && (flags & TILE_OPAQUE) == 0)
			{
				if (flip & TMAP_FLIPY) {
					if (flip & TMAP_FLIPX) {
						RenderCustomTile_Prio_TransMask_FlipXY(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent, gfx->color_offset, priority, gfx->gfxbase);
					} else {
						RenderCustomTile_Prio_TransMask_FlipY(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent, gfx->color_offset, priority, gfx->gfxbase);
					}
				} else {
					if (flip & TMAP_FLIPX) {
						RenderCustomTile_Prio_TransMask_FlipX(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent, gfx->color_offset, priority, gfx->gfxbase);
					} else {
						RenderCustomTile_Prio_TransMask(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent, gfx->color_offset, priority, gfx->gfxbase);
					}
				}	
			}
			else
			{
				if (flip & TMAP_FLIPY) {
					if (flip & TMAP_FLIPX) {
						RenderCustomTile_Prio_FlipXY(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, gfx->color_offset, priority, gfx->gfxbase);
					} else {
						RenderCustomTile_Prio_FlipY(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, gfx->color_offset, priority, gfx->gfxbase);
					}
				} else {
					if (flip & TMAP_FLIPX) {
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
	return (cols * row) + col;

	if (rows) return 1; // kill warnings
}

tilemap_scan( scan_cols )
{
	return (rows * col) + row;

	if (cols) return 1; // kill warnings
}

