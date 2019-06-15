#include "tiles_generic.h"

#define MAX_TILEMAPS	32	// number of tile maps allowed

struct GenericTilemap {
	UINT8 initialized;
	INT32 (*pScan)(INT32 col, INT32 row);
	void (*pTile)(INT32 offs, INT32 *tile_gfx, INT32 *tile_code, INT32 *tile_color, UINT32 *tile_flags, INT32 *category);
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
	UINT8 *transparent[256];	// 0 draw, 1 skip
	INT32 transcolor;
	UINT8 *dirty_tiles;			// 1 skip, 0 draw
	INT32 dirty_tiles_enable;
};

static GenericTilemap maps[MAX_TILEMAPS];
static GenericTilemap *cur_map;
GenericTilesGfx GenericGfxData[MAX_TILEMAPS];

void GenericTilemapInit(INT32 which, INT32 (*pScan)(INT32 col, INT32 row), void (*pTile)(INT32 offs, INT32 *tile_gfx, INT32 *tile_code, INT32 *tile_color, UINT32 *tile_flags, INT32 *category), UINT32 tile_width, UINT32 tile_height, UINT32 map_width, UINT32 map_height)
{
#if defined FBNEO_DEBUG
	if (Debug_GenericTilesInitted == 0) {
		bprintf (PRINT_ERROR, _T("Please call GenericTilesInit() before GenericTilemapInit()!\n"));
		return;
	}

	if (pTile == NULL) {
		bprintf (PRINT_ERROR, _T("GenericTilemapInit %d pTile initializer cannot be NULL!\n"), which);
		return;
	}

	if (pScan == NULL) {
		bprintf (PRINT_ERROR, _T("GenericTilemapInit %d pScan initializer cannot be NULL!\n"), which);
		return;
	}

	if (map_width == 0 || map_height == 0 || tile_width == 0 || tile_height == 0) {
		bprintf (PRINT_ERROR, _T("GenericTilemapInit(%d, pScan, pTile, %d, %d, %d, %d) called with bad initializer!\n"), which, tile_width, tile_height, map_width, map_height);
		return;
	}

	if (map_width > 4096 || map_height > 4096 || tile_width > 512 || tile_height > 512) {
		bprintf (PRINT_NORMAL, _T("GenericTilemapInit(%d, pScan, pTile, %d, %d, %d, %d) called with likely bad initializer!\n"), which, tile_width, tile_height, map_width, map_height);
	}
#endif

	cur_map = &maps[which];

	memset (cur_map, 0, sizeof(GenericTilemap));

	cur_map->initialized = 1;

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

	cur_map->transparent[0] = (UINT8*)BurnMalloc(0x100); // allocate 0 by default

	cur_map->priority = -1;
	cur_map->flags = 0;
	memset (cur_map->transparent[0], 0, 256);
	cur_map->transcolor = 0xfff; // opaque by default

	cur_map->dirty_tiles = NULL; // disable by default
	cur_map->dirty_tiles_enable = 0; // disable by default
}

void GenericTilemapSetGfx(INT32 num, UINT8 *gfxbase, INT32 depth, INT32 tile_width, INT32 tile_height, INT32 gfxlen, UINT32 color_offset, UINT32 color_mask)
{
#if defined FBNEO_DEBUG
	if (Debug_GenericTilesInitted == 0) {
		bprintf (PRINT_ERROR, _T("GenericTilesInit must be called before GenericTilemapSetGfx!\n"));
		return;
	}

	// warning
	if (depth > 8 || tile_width > 512 || tile_height > 512 || gfxlen <= 0 || gfxlen > 0x10000000 || color_offset > 0x10000) {
		bprintf (PRINT_NORMAL, _T("GenericTilemapSetGfx(%d, gfxbase, %d, %d, %d, 0x%x, 0x%x, 0x%s) called with likely bad initializer(s)!\n"), num, depth, tile_width, tile_height, gfxlen, color_offset, color_mask);
	}

	// error
	if (num < 0 || num >= MAX_GFX || tile_width <= 0 || tile_height <= 0 || gfxlen <= 0 || gfxbase == NULL) {
		bprintf (PRINT_ERROR, _T("GenericTilemapSetGfx(%d, gfxbase (%s), %d, %d, %d, 0x%x, 0x%x, 0x%s) called with bad initializer(s)!\n"), num, (gfxbase == NULL) ? _T("NULL") : _T("NON-NULL"), depth, tile_width, tile_height, gfxlen, color_offset, color_mask);
		return;
	}
#endif

	GenericTilesGfx *ptr = &GenericGfxData[num];

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
		if (cur_map->transparent[0]) BurnFree(cur_map->transparent[0]);
		if (cur_map->dirty_tiles) BurnFree(cur_map->dirty_tiles);
	}

	// wipe everything else out
	memset (maps, 0, sizeof(maps));
	memset (GenericGfxData, 0, sizeof(GenericGfxData));
}

void GenericTilemapSetOffsets(INT32 which, INT32 x, INT32 y)
{
#if defined FBNEO_DEBUG
	if (which >= MAX_TILEMAPS) {
		bprintf (PRINT_ERROR, _T("GenericTilemapSetOffsets(%d, %d, %d); called with impossible tilemap!\n"), which, x, y);
		return;
	}
#endif

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

#if defined FBNEO_DEBUG
		if (counter == 0) {
			bprintf (PRINT_NORMAL, _T("GenericTilemapSetOffsets(TMAP_GLOBAL, %d, %d); called, but there are no initialized tilemaps!\n"), x, y);
		}
#endif

		return;
	}

	// set offsets to a single tile map
	cur_map = &maps[which];

#if defined FBNEO_DEBUG
	if (cur_map->initialized == 0) {
		bprintf (PRINT_ERROR, _T("GenericTilemapSetOffsets(%d, %d, %d); called without initialized tilemap!\n"), which, x, y);
		return;
	}
#endif

	cur_map->xoffset = x;
	cur_map->yoffset = y;
}

void GenericTilemapSetTransparent(INT32 which, UINT32 transparent)
{
#if defined FBNEO_DEBUG
	if (which < 0 || which >= MAX_TILEMAPS) {
		bprintf (PRINT_ERROR, _T("GenericTilemapSetTransparent(%d, 0x%x); called with impossible tilemap number!\n"), which, transparent);
		return;
	}
#endif

	cur_map = &maps[which];

#if defined FBNEO_DEBUG
	if (cur_map->initialized == 0) {
		bprintf (PRINT_ERROR, _T("GenericTilemapSetTransparent(%d, 0x%x); called without initialized tilemap!\n"), which, transparent);
		return;
	}
#endif

	memset (cur_map->transparent[0], 0, 256);	// set all to opaque

	cur_map->transparent[0][transparent] = 1;	// one color opaque

	cur_map->transcolor = transparent;	// pass this to generic tile drawing
	cur_map->flags |= TMAP_TRANSPARENT;
}

void GenericTilemapSetTransSplit(INT32 which, INT32 category, UINT16 layer0, UINT16 layer1)
{
#if defined FBNEO_DEBUG
	if (which < 0 || which >= MAX_TILEMAPS) {
		bprintf (PRINT_ERROR, _T("GenericTilemapSetTransSplit(%d, %d, 0x%4.4x, 0x%4.4x); called with impossible tilemap number!\n"), which, category, layer0, layer1);
		return;
	}
#endif

	cur_map = &maps[which];

	if (category == 0) {
		GenericTilemapCategoryConfig(0, 4);
	}

	GenericTilemapSetTransMask(0, 0 | (category & 1), layer0); // TMAP_DRAWLAYER0
	GenericTilemapSetTransMask(0, 2 | (category & 1), layer1); // TMAP_DRAWLAYER1
}

void GenericTilemapSetTransMask(INT32 which, INT32 category, UINT16 transmask)
{
#if defined FBNEO_DEBUG
	if (which < 0 || which >= MAX_TILEMAPS) {
		bprintf (PRINT_ERROR, _T("GenericTilemapSetTransMask(%d, %d, 0x%4.4x); called with impossible tilemap number!\n"), which, category, transmask);
		return;
	}
#endif

	cur_map = &maps[which];

#if defined FBNEO_DEBUG
	if (cur_map->initialized == 0) {
		bprintf (PRINT_ERROR, _T("GenericTilemapsSetTransMask(%d, %d, 0x%4.4x); called without initialized tilemap!\n"), which, category, transmask);
		return;
	}

	if (cur_map->transparent[category] == NULL) {
		bprintf (PRINT_ERROR, _T("GenericTilemapSetTransMask(%d, %d, 0x%4.4x); called without configured category\n"), which, category, transmask);
		return;
	}
#endif

	memset (cur_map->transparent[category], 1, 256);

	for (INT32 i = 0; i < 16; i++) {
		if ((transmask & (1 << i)) == 0) {
			cur_map->transparent[category][i] = 0;
		}
	}

	cur_map->flags |= TMAP_TRANSMASK;
}

void GenericTilemapCategoryConfig(INT32 which, INT32 categories)
{
#if defined FBNEO_DEBUG
	if (which < 0 || which >= MAX_TILEMAPS) {
		bprintf (PRINT_ERROR, _T("GenericTilemapCategoryConfig(%d, %d); called with impossible tilemap number!\n"), which, categories);
		return;
	}

	if (categories < 0 || categories > 256) {
		bprintf (PRINT_NORMAL, _T("GenericTilemapCategoryConfig(%d, %d); called with invalid category number (<0 or >255)!\nForcing to 0!\n"), which, categories);
		categories = 0;
	}
#endif

	cur_map = &maps[which];

	if (cur_map->transparent[0]) {
		BurnFree(cur_map->transparent[0]);
	}

	cur_map->transparent[0] = BurnMalloc(256 * (categories + 1));

	for (INT32 i = 1; i < categories; i++)
	{
		cur_map->transparent[(i % categories)] = &cur_map->transparent[0][(i % categories) * 256];	
	}

	cur_map->flags |= TMAP_TRANSMASK;
}

void GenericTilemapSetCategoryEntry(INT32 which, INT32 category, INT32 entry, INT32 trans)
{
	trans = (trans) ? 1 : 0;

#if defined FBNEO_DEBUG
	if (which < 0 || which >= MAX_TILEMAPS) {
		bprintf (PRINT_ERROR, _T("GenericTilemapSetCategoryEntry(%d, %d, %d, %d); called with impossible tilemap number!\n"), which, category, entry, trans);
		return;
	}

	if (category < 0 || category > 256) {
		bprintf (PRINT_NORMAL, _T("GenericTilemapSetCategoryEntry(%d, %d, %d, %d); called with invalid category number (<0 or >255)!\nForcing to 0!\n"), which, category, entry, trans);
		category = 0;
	}

	if (entry < 0 || entry >= 256) {
		bprintf (PRINT_NORMAL, _T("GenericTilemapSetCategoryEntry(%d, %d, %d, %d); called with invalid entry number (<0 or >255)!\nForcing to 0!\n"), which, category, entry, trans);
		entry = 0;
	}
#endif

	cur_map = &maps[which];

#if defined FBNEO_DEBUG
	if (cur_map->transparent[category] == NULL) {
		bprintf (PRINT_ERROR, _T("GenericTilemapSetCategoryEntry(%d, %d, %d, %d); without configured category\n"), which, category, entry, trans);
		return;
	}
#endif

	cur_map->transparent[category][entry] = trans;
}

void GenericTilemapSetScrollX(INT32 which, INT32 scrollx)
{
#if defined FBNEO_DEBUG
	if (which < 0 || which >= MAX_TILEMAPS) {
		bprintf (PRINT_ERROR, _T("GenericTilemapSetScrollX(%d, %d); called with impossible tilemap!\n"), which, scrollx);
		return;
	}
#endif

	cur_map = &maps[which];

#if defined FBNEO_DEBUG
	if (cur_map->initialized == 0) {
		bprintf (PRINT_ERROR, _T("GenericTilemapSetScrollX(%d, %d); called without initialized tilemap!\n"), which, scrollx);
		return;
	}
#endif

	cur_map->scrollx = scrollx % (cur_map->twidth * cur_map->mwidth);
}

void GenericTilemapSetScrollY(INT32 which, INT32 scrolly)
{
#if defined FBNEO_DEBUG
	if (which < 0 || which >= MAX_TILEMAPS) {
		bprintf (PRINT_ERROR, _T("GenericTilemapSetScrollY(%d, %d); called with impossible tilemap!\n"), which, scrolly);
		return;
	}
#endif

	cur_map = &maps[which];

#if defined FBNEO_DEBUG
	if (cur_map->initialized == 0) {
		bprintf (PRINT_ERROR, _T("GenericTilemapSetScrollY(%d, %d); called without initialized tilemap!\n"), which, scrolly);
		return;
	}
#endif

	cur_map->scrolly = scrolly % (cur_map->theight * cur_map->mheight);
}

void GenericTilemapSetScrollCols(INT32 which, UINT32 cols)
{
#if defined FBNEO_DEBUG
	if (which < 0 || which >= MAX_TILEMAPS) {
		bprintf (PRINT_ERROR, _T("GenericTilemapSetScrollCols(%d, %d); called with impossible tilemap!\n"), which, cols);
		return;
	}
#endif

	cur_map = &maps[which];

#if defined FBNEO_DEBUG
	if (cur_map->initialized == 0) {
		bprintf (PRINT_ERROR, _T("GenericTilemapSetScrollCols(%d, %d); called without initialized tilemap!\n"), which, cols);
		return;
	}

	if (cols > (cur_map->mwidth * cur_map->twidth)) {
		bprintf (PRINT_ERROR, _T("GenericTilemapSetScrollCols(%d, %d); called with more cols than tilemap is wide (%d)!\n"), which, cols, cur_map->mwidth*cur_map->twidth);
		return;
	}
#endif

	// if setting cols <= 1, use scrolly instead
	if (cols <= 1) {
		cur_map->scroll_cols = 1;
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
	}
}

void GenericTilemapSetScrollRows(INT32 which, UINT32 rows)
{
#if defined FBNEO_DEBUG
	if (which < 0 || which >= MAX_TILEMAPS) {
		bprintf (PRINT_ERROR, _T("GenericTilemapSetScrollRows(%d, %d); called with impossible tilemap!\n"), which, rows);
		return;
	}
#endif

	cur_map = &maps[which];

#if defined FBNEO_DEBUG
	if (cur_map->initialized == 0) {
		bprintf (PRINT_ERROR, _T("GenericTilemapSetScrollRows(%d, %d); called without initialized tilemap!\n"), which, rows);
		return;
	}

	if (rows > (cur_map->mheight * cur_map->theight)) {
		bprintf (PRINT_ERROR, _T("GenericTilemapSetScrollRows(%d, %d); called with more rows than tilemap is high (%d)!\n"), which, rows, cur_map->mheight*cur_map->theight);
		return;
	}
#endif

	// use scrollx instead
	if (rows <= 1) {
		cur_map->scroll_rows = 1;
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
#if defined FBNEO_DEBUG
	if (which < 0 || which >= MAX_TILEMAPS) {
		bprintf (PRINT_ERROR, _T("GenericTilemapSetScrollCol(%d, %d, %d); called with impossible tilemap!\n"), which, col, scroll);
		return;
	}
#endif

	cur_map = &maps[which];

#if defined FBNEO_DEBUG
	if (cur_map->initialized == 0) {
		bprintf (PRINT_ERROR, _T("GenericTilemapSetScrollCol(%d, %d, %d); called without initialized tilemap!\n"), which, col, scroll);
		return;
	}

	if ((INT32)cur_map->scroll_cols <= col || col < 0) {
		bprintf (PRINT_ERROR, _T("GenericTilemapSetScrollCol(%d, %d, %d); called with improper col value!\n"), which, col, scroll);
		return;
	}
#endif

	if (cur_map->scrolly_table != NULL) {
		cur_map->scrolly_table[col] = scroll % (cur_map->theight * cur_map->mheight);
	}
}

void GenericTilemapSetScrollRow(INT32 which, INT32 row, INT32 scroll)
{
#if defined FBNEO_DEBUG
	if (which < 0 || which >= MAX_TILEMAPS) {
		bprintf (PRINT_ERROR, _T("GenericTilemapSetScrollRow(%d, %d, %d); called with impossible tilemap!\n"), which, row, scroll);
		return;
	}
#endif

	cur_map = &maps[which];

#if defined FBNEO_DEBUG
	if (cur_map->initialized == 0) {
		bprintf (PRINT_ERROR, _T("GenericTilemapSetScrollRow(%d, %d, %d); called without initialized tilemap!\n"), which, row, scroll);
		return;
	}

	if ((INT32)cur_map->scroll_rows <= row || row < 0) {
		bprintf (PRINT_ERROR, _T("GenericTilemapSetScrollRow(%d, %d, %d); called with improper row value!\n"), which, row, scroll);
		return;
	}
#endif

	if (cur_map->scrollx_table != NULL) {
		cur_map->scrollx_table[row] = scroll % (cur_map->twidth * cur_map->mwidth);
	}
}

void GenericTilemapSetFlip(INT32 which, INT32 flip)
{
#if defined FBNEO_DEBUG
	if (which >= MAX_TILEMAPS) {
		bprintf (PRINT_ERROR, _T("GenericTilemapSetFlip(%d, %d); called with impossible tilemap!\n"), which, flip);
		return;
	}
#endif

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

#if defined FBNEO_DEBUG
		if (counter == 0) {
			bprintf (PRINT_ERROR, _T("GenericTilemapSetFlip(TMAP_GLOBAL, %d); called, but there are no initialized tilemaps!\n"), flip);
		}
#endif

		return;
	}

	cur_map = &maps[which];

#if defined FBNEO_DEBUG
	if (cur_map->initialized == 0) {
		bprintf (PRINT_ERROR, _T("GenericTilemapSetFlip(%d, %d); called without initialized tilemap!\n"), which, flip);
		return;
	}
#endif

	cur_map->flags &= ~(TMAP_FLIPY|TMAP_FLIPX);
	cur_map->flags |= flip;
}

void GenericTilemapSetEnable(INT32 which, INT32 enable)
{
#if defined FBNEO_DEBUG
	if (which >= MAX_TILEMAPS) {
		bprintf (PRINT_ERROR, _T("GenericTilemapSetEnable(%d, %d); called with impossible tilemap!\n"), which, enable);
		return;
	}
#endif

	if (which == TMAP_GLOBAL) {
		INT32 counter = 0;
		for (INT32 i = 0; i < MAX_TILEMAPS; i++) {
			cur_map = &maps[i];
			if (cur_map->initialized) {
				cur_map->enable = enable ? 1 : 0;
				counter++;
			}
		}

#if defined FBNEO_DEBUG
		if (counter == 0) {
			bprintf (PRINT_NORMAL, _T("GenericTilemapSetEnable(TMAP_GLOBAL, %d); called, but there are no initialized tilemaps!\n"), enable);
		}
#endif
		return;
	}

	cur_map = &maps[which];

#if defined FBNEO_DEBUG
	if (cur_map->initialized == 0) {
		bprintf (PRINT_ERROR, _T("GenericTilemapSetEnable(%d, %d); called without initialized tilemap!\n"), which, enable);
		return;
	}
#endif

	cur_map->enable = enable ? 1 : 0;
}

void GenericTilemapUseDirtyTiles(INT32 which)
{
#if defined FBNEO_DEBUG
	if (which >= MAX_TILEMAPS) {
		bprintf (PRINT_ERROR, _T("GenericTilemapUseDirtyTiles(%d) called with impossible tilemap!\n"), which);
		return;
	}
#endif

	cur_map = &maps[which];

#if defined FBNEO_DEBUG
	if (cur_map->initialized == 0) {
		bprintf (PRINT_ERROR, _T("GenericTilemapUseDirtyTiles(%d) called without initialized tilemap!\n"), which);
		return;
	}
#endif

	cur_map->dirty_tiles = (UINT8*)BurnMalloc(cur_map->mwidth * cur_map->mheight);

	memset (cur_map->dirty_tiles, 1, cur_map->mwidth * cur_map->mheight); // force all dirty by default

	cur_map->dirty_tiles_enable = 1;
}

void GenericTilemapSetTileDirty(INT32 which, UINT32 offset)
{
#if defined FBNEO_DEBUG
	if (which >= MAX_TILEMAPS) {
		bprintf (PRINT_ERROR, _T("GenericTilemapSetTileDirty(%d, %x); called with impossible tilemap!\n"), which, offset);
		return;
	}
#endif

	cur_map = &maps[which];

#if defined FBNEO_DEBUG
	if (cur_map->initialized == 0) {
		bprintf (PRINT_ERROR, _T("GenericTilemapSetTileDirty(%d, %x) called without initialized tilemap!\n"), which, offset);
		return;
	}

	if (cur_map->dirty_tiles_enable == 0) {
		bprintf (PRINT_ERROR, _T("GenericTilemapSetTileDirty(%d, %x) called without calling GenericTilemapUseDirtyTiles first!\n"), which, offset);
		return;
	}
#endif

	cur_map->dirty_tiles[offset % (cur_map->mwidth * cur_map->mheight)] = 1;
}

void GenericTilemapAllTilesDirty(INT32 which)
{
#if defined FBNEO_DEBUG
	if (which >= MAX_TILEMAPS) {
		bprintf (PRINT_ERROR, _T("GenericTilemapAllTilesDirty(%d); called with impossible tilemap!\n"), which);
		return;
	}
#endif

	cur_map = &maps[which];

#if defined FBNEO_DEBUG
	if (cur_map->initialized == 0) {
		bprintf (PRINT_ERROR, _T("GenericTilemapAllTilesDirty(%d) called without initialized tilemap!\n"), which);
		return;
	}

	if (cur_map->dirty_tiles_enable == 0) {
		bprintf (PRINT_ERROR, _T("GenericTilemapAllTilesDirty(%d) called without calling GenericTilemapUseDirtyTiles first!\n"), which);
		return;
	}
#endif

	memset (cur_map->dirty_tiles, 1, cur_map->mwidth * cur_map->mheight);
}

INT32 GenericTilemapGetTileDirty(INT32 which, UINT32 offset)
{
#if defined FBNEO_DEBUG
	if (which >= MAX_TILEMAPS) {
		bprintf (PRINT_ERROR, _T("GenericTilemapGetTileDirty(%d, %x); called with impossible tilemap!\n"), which, offset);
		return 1; // default to dirty tile
	}
#endif

	cur_map = &maps[which];

#if defined FBNEO_DEBUG
	if (cur_map->initialized == 0) {
		bprintf (PRINT_ERROR, _T("GenericTilemapGetTileDirty(%d, %x) called without initialized tilemap!\n"), which, offset);
		return 1; // default to dirty tile
	}

	if (cur_map->dirty_tiles_enable == 0) {
		bprintf (PRINT_ERROR, _T("GenericTilemapGetTileDirty(%d, %x) called without calling GenericTilemapUseDirtyTiles first!\n"), which, offset);
		return 1; // default to dirty tile
	}
#endif

	return cur_map->dirty_tiles[offset % (cur_map->mwidth * cur_map->mheight)];
}

void GenericTilemapDraw(INT32 which, UINT16 *Bitmap, INT32 priority, INT32 priority_mask)
{
#if defined FBNEO_DEBUG
	if (Bitmap == NULL) {
		bprintf (PRINT_ERROR, _T("GenericTilemapDraw(%d, Bitmap, %d); called without initialized Bitmap!\n"), which, priority);
		return;
	}
#endif

	cur_map = &maps[which];

#if defined FBNEO_DEBUG
	if (cur_map->initialized == 0) {
		bprintf (PRINT_ERROR, _T("GenericTilemapDraw(%d, Bitmap, %d); called without initialized tilemap!\n"), which, priority);
		return;
	}
#endif

	if (cur_map->enable == 0) { // layer disabled!
		return;
	}

	INT32 minx, maxx, miny, maxy;
	GenericTilesGetClip(&minx, &maxx, &miny, &maxy);

	// check clipping and fix clipping sizes if out of bounds
	if (minx < 0 || maxx > nScreenWidth || miny < 0 || maxy > nScreenHeight) {
		bprintf (PRINT_ERROR, _T("GenericTilemapDraw(%d, Bitmap, %d) called with improper clipping values (%d, %d, %d, %d)!"), which, priority, minx, maxx, miny, maxy);
		bprintf (PRINT_ERROR, _T("pPrioDraw is %d pixels wide and %d pixels high!\n"), nScreenWidth, nScreenHeight);

		if (minx < 0) minx = 0;
		if (maxx >= nScreenWidth) maxx = nScreenWidth;
		if (miny < 0) miny = 0;
		if (maxy >= nScreenHeight) maxy = nScreenHeight;
	}

	GenericTilesPRIMASK = priority_mask;

	INT32 category_or = (priority & TMAP_DRAWLAYER1) ? 2 : 0;
	INT32 opaque = priority & TMAP_FORCEOPAQUE;
	INT32 opaque2 = priority & TMAP_DRAWOPAQUE;
	INT32 tgroup = (priority >> 8) & 0xff;
	priority &= 0xff;

	// column (less than tile size) and line scroll
	if ((cur_map->scrolly_table != NULL) && (cur_map->scroll_cols > cur_map->mwidth))
	{
		INT32 bitmap_width = maxx - minx;
		INT32 scrymod = (cur_map->mheight * cur_map->theight);
		INT32 scrxmod = (cur_map->mwidth * cur_map->twidth);

		for (INT32 y = miny; y < maxy; y++)
		{
			for (INT32 x = 0; x < bitmap_width; x++)
			{
				INT32 sx;
				if (cur_map->scrollx_table != NULL)
					sx = (x + cur_map->scrollx_table[(y * cur_map->scroll_rows) / scrymod] - cur_map->xoffset) % scrxmod;
				else
					sx = (x + cur_map->scrolly - cur_map->xoffset) % scrxmod;

				INT32 sy = (y + cur_map->scrolly_table[(sx * cur_map->scroll_cols) / scrxmod] - cur_map->yoffset) % scrymod;

				INT32 row = sy / cur_map->theight;
				INT32 col = sx / cur_map->twidth;

				INT32 by = sy;
				if (cur_map->flags & TMAP_FLIPY) {
					by = (bitmap_width - cur_map->theight) - by;
				}

				INT32 bx = sx;
				if (cur_map->flags & TMAP_FLIPX) {
					bx = (bitmap_width - cur_map->twidth) - bx;
				}

				INT32 code, color, group, gfxnum, category = 0, offset = cur_map->pScan(col,row);
				UINT32 flags;

				if (cur_map->dirty_tiles_enable) {
					if (cur_map->dirty_tiles[offset] == 0) continue;
					cur_map->dirty_tiles[offset] = 0;
				}

				cur_map->pTile(offset, &gfxnum, &code, &color, &flags, &category);

				category |= category_or;
				
				if (category && (cur_map->flags & TMAP_TRANSMASK)) {
					if (cur_map->transparent[category] == NULL) {
						category = 0;
					}
				}

				if (opaque == 0)
				{
					if (flags & TILE_SKIP) continue; // skip this tile

					if (flags & TILE_GROUP_ENABLE) {
						group = (flags >> 16) & 0xff;

						if (group != tgroup) {
							continue;
						}
					}
				}

				GenericTilesGfx *gfx = &GenericGfxData[gfxnum];

				if (gfx->gfxbase == NULL) {
					bprintf (PRINT_ERROR,_T("GenericTilemapDraw(%d) gfx[%d] not initialized!\n"), which, gfxnum);
					continue;
				}

				color = ((color & gfx->color_mask) << gfx->depth) + gfx->color_offset;
				code %= gfx->code_mask;

				INT32 flipx = flags & TILE_FLIPX; 
				INT32 flipy = flags & TILE_FLIPY;
				if (cur_map->flags & TMAP_FLIPY) flipy ^= TILE_FLIPY;
				if (cur_map->flags & TMAP_FLIPX) flipy ^= TILE_FLIPX;

				INT32 goffs = 0;
				if (flipy) { goffs += (cur_map->theight - 1) - (sy % cur_map->theight); } else { goffs += (sy % cur_map->theight); }
				goffs *= cur_map->twidth;
				if (flipx) { goffs += (cur_map->twidth  - 1) - (sx % cur_map->twidth);  } else { goffs += (sx % cur_map->twidth); }

				UINT8 *gfxsrc = gfx->gfxbase + (code * cur_map->twidth * cur_map->theight) + goffs;

				UINT8 *trans_ptr = cur_map->transparent[category];

				if (trans_ptr[*gfxsrc] == 0)
				{
					Bitmap[y * bitmap_width + x] = *gfxsrc + color;
					pPrioDraw[y * bitmap_width + x] = priority | (pPrioDraw[y * bitmap_width + x] & GenericTilesPRIMASK);
				}
			}
		}
		
		return;
	}
	// line scroll
	else if ((cur_map->scrollx_table != NULL) && (cur_map->scroll_rows > cur_map->mheight))
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

				INT32 code, color, group, gfxnum, category = 0, offset = cur_map->pScan(col,row);
				UINT32 flags;

				if (cur_map->dirty_tiles_enable) {
					if (cur_map->dirty_tiles[offset] == 0) continue;
					cur_map->dirty_tiles[offset] = 0;
				}

				cur_map->pTile(offset, &gfxnum, &code, &color, &flags, &category);

				category |= category_or;
				
				if (category && (cur_map->flags & TMAP_TRANSMASK)) {
					if (cur_map->transparent[category] == NULL) {
						category = 0;
					}
				}

				if (opaque == 0)
				{
					if (flags & TILE_SKIP) continue; // skip this tile

					if (flags & TILE_GROUP_ENABLE) {
						group = (flags >> 16) & 0xff;

						if (group != tgroup) {
							continue;
						}
					}
				}

				GenericTilesGfx *gfx = &GenericGfxData[gfxnum];

				if (gfx->gfxbase == NULL) {
					bprintf (PRINT_ERROR,_T("GenericTilemapDraw(%d) gfx[%d] not initialized!\n"), which, gfxnum);
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
				UINT8 *trans_ptr = cur_map->transparent[category];

				if (flipx)
				{
					INT32 flip_wide = cur_map->twidth - 1;

					for (UINT32 dx = 0; dx < cur_map->twidth; dx++)
					{
						INT32 dst = (sx + dx) - scrx;
						if (dst < minx || dst >= maxx) continue;

						if (trans_ptr[gfxsrc[flip_wide - dx]] == 0) {
							dest[dst] = color + gfxsrc[flip_wide - dx];
							prio[dst] = priority | (prio[dst] & GenericTilesPRIMASK);
						}
					}
				}
				else
				{
					for (UINT32 dx = 0; dx < cur_map->twidth; dx++)
					{
						INT32 dst = (sx + dx) - scrx;
						if (dst < minx || dst >= maxx) continue;

						if (trans_ptr[gfxsrc[dx]] == 0) {
							dest[dst] = color + gfxsrc[dx];
							prio[dst] = priority | (prio[dst] & GenericTilesPRIMASK);
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

		// start drawing at tile-border, and let RenderCustomTile..Clip() take care of the sub-tile clipping.
		INT32 starty = miny - (miny % cur_map->theight);
		INT32 startx = minx - (minx % cur_map->twidth);
		INT32 endx = maxx + cur_map->twidth;
		INT32 endy = maxy + cur_map->theight;
		
		if (cur_map->flags & TMAP_FLIPX) {
			INT32 tmp = ((cur_map->mwidth - 1) * cur_map->twidth) - (endx - cur_map->twidth);
			endx = (((cur_map->mwidth - 1) * cur_map->twidth) - startx) + cur_map->twidth;
			startx = tmp;
		}

		if (cur_map->flags & TMAP_FLIPY) {
			INT32 tmp = ((cur_map->mheight - 1) * cur_map->theight) - (endy - cur_map->theight);
			endy = (((cur_map->mheight - 1) * cur_map->theight) - starty) + cur_map->theight;
			starty = tmp;
		}

		for (INT32 y = starty; y < endy; y += cur_map->theight)
		{
			INT32 syy = (y + scrolly) % (cur_map->theight * cur_map->mheight);

			for (INT32 x = startx; x < endx; x += cur_map->twidth)
			{
				INT32 sxx = (x + scrollx) % (cur_map->twidth * cur_map->mwidth);

				INT32 code, color, group, gfxnum, category = 0;
				INT32 offset = cur_map->pScan(sxx/cur_map->twidth,syy/cur_map->theight);
				UINT32 flags;

				if (cur_map->dirty_tiles_enable) {
					if (cur_map->dirty_tiles[offset] == 0) continue;
					cur_map->dirty_tiles[offset] = 0;
				}

				cur_map->pTile(offset, &gfxnum, &code, &color, &flags, &category);

				category |= category_or;

				if (category && (cur_map->flags & TMAP_TRANSMASK)) {
					if (cur_map->transparent[category] == NULL) {
						category = 0;
					}
				}

				if (opaque == 0)
				{
					if (flags & TILE_SKIP) continue; // skip this tile

					if (flags & TILE_GROUP_ENABLE) {
						group = (flags >> 16) & 0xff;
	
						if (group != tgroup) {
							continue;
						}
					}
				}

				GenericTilesGfx *gfx = &GenericGfxData[gfxnum];

				if (gfx->gfxbase == NULL) {
					bprintf (PRINT_ERROR,_T("GenericTilemapDraw(%d) gfx[%d] not initialized!\n"), which, gfxnum);
					continue;
				}

				color &= gfx->color_mask;
				code %= gfx->code_mask;

				INT32 sy = y - syshift;
				INT32 sx = x - sxshift;

				INT32 flipx = flags & TILE_FLIPX; 
				INT32 flipy = flags & TILE_FLIPY;

				if (cur_map->flags & TMAP_FLIPY) {
					sy = ((cur_map->mheight - 1) * cur_map->theight) - sy;
					flipy ^= TILE_FLIPY;
				}

				if (cur_map->flags & TMAP_FLIPX) {
					sx = ((cur_map->mwidth - 1) * cur_map->twidth) - sx;
					flipx ^= TILE_FLIPX;
				}

				// skip tiles that are out of the visible area
				if ((sx >= maxx) || (sy >= maxy) || (sx < (INT32)(minx - (cur_map->twidth - 1))) || (sy < (INT32)(miny - (cur_map->theight - 1)))) {
					continue;
				}

				if (sx < minx || sy < miny || sx >= (INT32)(maxx - cur_map->twidth - 1) || sy >= (INT32)(maxy - cur_map->theight - 1))
				{
					if ((cur_map->flags & TMAP_TRANSPARENT) && (flags & TILE_OPAQUE) == 0 && opaque == 0 && opaque2 == 0)
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
					else if ((cur_map->flags & TMAP_TRANSMASK) && (flags & TILE_OPAQUE) == 0 && opaque == 0 && opaque2 == 0)
					{
						if (flipy) {
							if (flipx) {
								RenderCustomTile_Prio_TransMask_FlipXY_Clip(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent[category], gfx->color_offset, priority, gfx->gfxbase);
							} else {
								RenderCustomTile_Prio_TransMask_FlipY_Clip(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent[category], gfx->color_offset, priority, gfx->gfxbase);
							}
						} else {
							if (flipx) {
								RenderCustomTile_Prio_TransMask_FlipX_Clip(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent[category], gfx->color_offset, priority, gfx->gfxbase);
							} else {
								RenderCustomTile_Prio_TransMask_Clip(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent[category], gfx->color_offset, priority, gfx->gfxbase);
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
					if ((cur_map->flags & TMAP_TRANSPARENT) && (flags & TILE_OPAQUE) == 0 && opaque == 0 && opaque2 == 0)
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
					else if ((cur_map->flags & TMAP_TRANSMASK) && (flags & TILE_OPAQUE) == 0 && opaque == 0 && opaque2 == 0)
					{
						if (flipy) {
							if (flipx) {
								RenderCustomTile_Prio_TransMask_FlipXY(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent[category], gfx->color_offset, priority, gfx->gfxbase);
							} else {
								RenderCustomTile_Prio_TransMask_FlipY(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent[category], gfx->color_offset, priority, gfx->gfxbase);
							}
						} else {
							if (flipx) {
								RenderCustomTile_Prio_TransMask_FlipX(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent[category], gfx->color_offset, priority, gfx->gfxbase);
							} else {
								RenderCustomTile_Prio_TransMask(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent[category], gfx->color_offset, priority, gfx->gfxbase);
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

		INT32 code, color, group, gfxnum, category = 0, offset = cur_map->pScan(col,row);
		UINT32 flags;

		if (cur_map->dirty_tiles_enable) {
			if (cur_map->dirty_tiles[offset] == 0) continue;
			cur_map->dirty_tiles[offset] = 0;
		}

		cur_map->pTile(offset, &gfxnum, &code, &color, &flags, &category);

		category |= category_or;

		if (category && (cur_map->flags & TMAP_TRANSMASK)) {
			if (cur_map->transparent[category] == NULL) {
				category = 0;
			}
		}

		if (opaque == 0)
		{
			if (flags & TILE_SKIP) continue; // skip this tile

			if (flags & TILE_GROUP_ENABLE) {
				group = (flags >> 16) & 0xff;

				if (group != tgroup) {
					continue;
				}
			}
		}

		GenericTilesGfx *gfx = &GenericGfxData[gfxnum];

		if (gfx->gfxbase == NULL) {
			bprintf (PRINT_ERROR,_T("GenericTilemapDraw(%d) gfx[%d] not initialized!\n"), which, gfxnum);
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

		if (sx < (INT32)(1-cur_map->twidth)) sx += cur_map->twidth * cur_map->mwidth;
		if (sy < (INT32)(1-cur_map->theight)) sy += cur_map->theight * cur_map->mheight;

		INT32 flipx = flags & TILE_FLIPX;
		INT32 flipy = flags & TILE_FLIPY;

		sx += cur_map->xoffset;
		sy += cur_map->yoffset;

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
			if ((cur_map->flags & TMAP_TRANSPARENT) && (flags & TILE_OPAQUE) == 0 && opaque == 0 && opaque2 == 0)
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
			else if ((cur_map->flags & TMAP_TRANSMASK) && (flags & TILE_OPAQUE) == 0 && opaque == 0 && opaque2 == 0)
			{
				if (flipy) {
					if (flipx) {
						RenderCustomTile_Prio_TransMask_FlipXY_Clip(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent[category], gfx->color_offset, priority, gfx->gfxbase);
					} else {
						RenderCustomTile_Prio_TransMask_FlipY_Clip(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent[category], gfx->color_offset, priority, gfx->gfxbase);
					}
				} else {
					if (flipx) {
						RenderCustomTile_Prio_TransMask_FlipX_Clip(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent[category], gfx->color_offset, priority, gfx->gfxbase);
					} else {
						RenderCustomTile_Prio_TransMask_Clip(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent[category], gfx->color_offset, priority, gfx->gfxbase);
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
			if ((cur_map->flags & TMAP_TRANSPARENT) && (flags & TILE_OPAQUE) == 0 && opaque == 0 && opaque2 == 0)
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
			else if ((cur_map->flags & TMAP_TRANSMASK) && (flags & TILE_OPAQUE) == 0 && opaque == 0 && opaque2 == 0)
			{
				if (flipy) {
					if (flipx) {
						RenderCustomTile_Prio_TransMask_FlipXY(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent[category], gfx->color_offset, priority, gfx->gfxbase);
					} else {
						RenderCustomTile_Prio_TransMask_FlipY(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent[category], gfx->color_offset, priority, gfx->gfxbase);
					}
				} else {
					if (flipx) {
						RenderCustomTile_Prio_TransMask_FlipX(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent[category], gfx->color_offset, priority, gfx->gfxbase);
					} else {
						RenderCustomTile_Prio_TransMask(Bitmap, cur_map->twidth, cur_map->theight, code, sx, sy, color, gfx->depth, cur_map->transparent[category], gfx->color_offset, priority, gfx->gfxbase);
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

// generic drawing using bitmap manager
void GenericTilemapDraw(INT32 which, INT32 bitmap, INT32 priority)
{
	// this is hacky for now
	// ideally we would pass the bitmap struct
	INT32 swap_bitmaps = (pTransDraw == BurnBitmapGetBitmap(bitmap)) ? 0 : 1;

	if (swap_bitmaps)
	{
		INT32 nMinx=0, nMaxx=0, nMiny=0, nMaxy=0;

		// get dimensions of the destination bitmap and store 
		BurnBitmapGetDimensions(bitmap, &nScreenWidth, &nScreenHeight);

		// get clipping information from the destination bitmap
		BurnBitmapGetClipDims(bitmap, &nMinx, &nMaxx, &nMiny, &nMaxy);

		// set clipping using destination bitmap's clipping info
		GenericTilesSetClipRaw(nMinx, nMaxx, nMiny, nMaxy);

		// point to drawing surface
		pTransDraw = BurnBitmapGetBitmap(bitmap);

		// change priority map pointer as to not override the default priority map
		pPrioDraw = BurnBitmapGetPriomap(bitmap);
	}

	// draw to our bitmap!
	GenericTilemapDraw(which, pTransDraw, priority);

	if (swap_bitmaps)
	{
		// bitmap #0 is always pTransDraw, restore drawing surface, priority, dimensions, and clip
		pTransDraw = BurnBitmapGetBitmap(0);
		pPrioDraw = BurnBitmapGetPriomap(0);
		BurnBitmapGetDimensions(0, &nScreenWidth, &nScreenHeight);
		GenericTilesClearClipRaw();
	}
}

void GenericTilemapDumpToBitmap()
{
#if defined FBNEO_DEBUG
	if (pBurnDrvPalette == NULL) {
		bprintf (PRINT_ERROR, _T("GenericTilemapDumptoBitmap called with pBurnDrvPalette == NULL\n"));
		return;
	}

	if (nBurnBpp < 3) {
		bprintf (PRINT_ERROR, _T("GenericTilemapDumptoBitmap called with pBurnBpp < 24 bit\n"));
		return;
	}	
#endif

	GenericTilemap *tmp_map = cur_map;

#define SET_FILE_SIZE(x)	\
	bmp_data[2] = (x);	\
	bmp_data[3] = (x)>>8;	\
	bmp_data[4] = (x)>>16

#define SET_BITMAP_SIZE(x)	\
	bmp_data[0x22] = (x);	\
	bmp_data[0x23] = (x)>>8;	\
	bmp_data[0x24] = (x)>>16

#define SET_BITMAP_WIDTH(x)	\
	bmp_data[0x12] = (x);	\
	bmp_data[0x13] = (x)>>8;	\
	bmp_data[0x14] = (x)>>16

#define SET_BITMAP_HEIGHT(x)	\
	bmp_data[0x16] = (x);	\
	bmp_data[0x17] = (x)>>8;	\
	bmp_data[0x18] = (x)>>16

	UINT8 bmp_data[0x36] = {
		0x42, 0x4D, 			// 'BM' (leave alone)
		0x00, 0x00, 0x00, 0x00, // file size
		0x00, 0x00, 0x00, 0x00, // padding
		0x36, 0x00, 0x00, 0x00, // offset to data (leave alone)
		0x28, 0x00, 0x00, 0x00, // windows mode (leave alone)
		0x00, 0x00, 0x00, 0x00, // bitmap width
		0x00, 0x00, 0x00, 0x00, // bitmap height
		0x01, 0x00,				// planes (1) always!
		0x20, 0x00, 			// bits per pixel (let's do 32 so no conversion!)
		0x00, 0x00, 0x00, 0x00, // compression (none)
		0x00, 0x00, 0x00, 0x00, // size of bitmap data
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00
	};

	for (INT32 i = 0; i < MAX_TILEMAPS; i++)
	{
		cur_map = &maps[i];
		if (cur_map->initialized == 0) continue;

		char tmp[256];
		sprintf (tmp, "%s_layer%2.2d_dump.bmp", BurnDrvGetTextA(DRV_NAME), i);

		FILE *fa = fopen(tmp, "wb");

		INT32 wide = cur_map->mwidth * cur_map->twidth;
		INT32 high = cur_map->mheight * cur_map->theight;

		SET_FILE_SIZE((wide*high*4)+54);
		SET_BITMAP_SIZE(wide*high*4);
		SET_BITMAP_WIDTH(wide);
		SET_BITMAP_HEIGHT(high);

		fwrite (bmp_data, 54, 1, fa);

		UINT32 *bitmap = (UINT32*)BurnMalloc(wide*high*4);

		{
			INT32 tmap_width = (cur_map->mwidth * cur_map->twidth);

			for (INT32 row = (INT32)cur_map->mheight-1; row >= 0; row--)	// upside down due to bitmap format
			{
				INT32 sy = row * cur_map->theight;

				for (UINT32 col = 0; col < cur_map->mwidth; col++)
				{
					INT32 sx = col * cur_map->twidth;
					INT32 code, color, gfxnum, category = 0;
					UINT32 flags;

					cur_map->pTile(cur_map->pScan(col, row), &gfxnum, &code, &color, &flags, &category);

					{
						GenericTilesGfx *gfxptr = &GenericGfxData[gfxnum];

						UINT8 *gfx = gfxptr->gfxbase + (code * gfxptr->width * gfxptr->height);

						UINT32 *palette = pBurnDrvPalette + (((color & gfxptr->color_mask) << gfxptr->depth) + gfxptr->color_offset);

						INT32 flipx = (flags & TILE_FLIPX) ? (gfxptr->width-1) : 0;
						INT32 flipy = (flags & TILE_FLIPY) ? (gfxptr->height-1) : 0;

						UINT32 *dest = bitmap + (sy * tmap_width) + sx;

						for (INT32 yy = 0; yy < gfxptr->height; yy++)
						{
							UINT8 *g = gfx + (yy ^ flipy) * gfxptr->width;

							for (INT32 xx = 0; xx < gfxptr->width; xx++)
							{
								dest[xx] = palette[g[xx ^ flipx]];
							}

							dest += tmap_width;
						}
					}
				}
			}
		}

		fwrite (bitmap, wide*high*4, 1, fa);

		fclose (fa);
		BurnFree (bitmap);
	}

	cur_map = tmp_map;
}

tilemap_scan( scan_rows )
{
	return (cur_map->mwidth * row) + col;
}

tilemap_scan( scan_cols )
{
	return (cur_map->mheight * col) + row;
}
