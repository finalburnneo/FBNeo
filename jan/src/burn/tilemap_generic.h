// Tilemap defines

// use this when in place of "which" to have this apply to all initialized tilemaps
// works for: GenericTilemapSetEnable, GenericTilemapSetFlip, GenericTilemapSetOffsets
#define TMAP_GLOBAL		-1

// flip the tilemap on the screen horizontally (used with GenericTilemapSetFlip )
#define TMAP_FLIPX		(1 <<  0)

// flip the tilemap on the screen vertically (used with GenericTilemapSetFlip )
#define TMAP_FLIPY		(1 <<  1)

// flip the tilemap on the screen vertically and horizontally (used with GenericTilemapSetFlip )
#define TMAP_FLIPXY		(TMAP_FLIPX | TMAP_FLIPY)

// force the tilemap to ignore any transparency settings
#define TMAP_FORCEOPAQUE	(1 << 24)

// set tilemap to use transparent color (is set when using GenericTilemapSetTransparent)
#define TMAP_TRANSPARENT	(1 <<  9)

// set tilemap to use a transparency mask (is set when using GenericTilemapSetTransMask)
#define TMAP_TRANSMASK		(1 << 10)

// select which group to use in the tilemap (group is set in the tilemap_callback)
// pass this in the priority variable in GenericTilemapDraw.
#define TMAP_SET_GROUP(x)	((x) <<8)

// Tile defines (used in tilemap_callback)

// flip this tile horizontally
#define TILE_FLIPX		TMAP_FLIPX

// flip this tile vertically
#define TILE_FLIPY		TMAP_FLIPY

// ignore transparency for this tile only
#define TILE_OPAQUE		(1 <<  2)

// skip drawing this tile (can be used for speedups)
#define TILE_SKIP		(1 <<  3)

// enabled in TILE_GROUP to show that the group is enabled
#define TILE_GROUP_ENABLE	(1 <<  4)

// set flipping (pass variable through these to set them as a pair)
#define TILE_FLIPXY(x)		((((x) & 2) >> 1) | (((x) & 1) << 1))
#define TILE_FLIPYX(x)		((x) & 3)

// set group, pass this as a flag
#define TILE_GROUP(x)		(((x) << 16) | TILE_GROUP_ENABLE)

// This are used to make sure the tilemap functions are given standard names
#define tilemap_callback( xname )	void xname##_map_callback(INT32 offs, INT32 *tile_gfx, INT32 *tile_code, INT32 *tile_color, UINT32 *tile_flags)
#define tilemap_scan( xname )		INT32 xname##_map_scan(INT32 col, INT32 row)

// Pass the tilemap callback variables using this macro (looks nice)
#define TILE_SET_INFO(ttgfx, ttcode, ttcolor, ttflags)	\
	*tile_gfx = (ttgfx);	\
	*tile_code = (ttcode);	\
	*tile_color = (ttcolor);	\
	*tile_flags = (ttflags)

// The tilemap scan calculates the offset of the current tile information in the video ram
// these are the two most common variations
extern tilemap_scan( scan_rows );
extern tilemap_scan( scan_cols );

// Now give them nice names
#define TILEMAP_SCAN_ROWS	scan_rows_map_scan
#define TILEMAP_SCAN_COLS	scan_cols_map_scan

// The actual initialization routine for the tilemap
// which	- which actual tilemap do you want to set this as 
// pScan	- pointer to "scan" routine, described above...
// pTile	- pointer to tile info callback
// tile_width	- how many pixels wide are the tiles
// tile_height	- how many pixels tall are the tiles
// map_width	- how many tiles wide is the tile map
// map_height	- how many tiles high is the tile map
void GenericTilemapInit(INT32 which, INT32 (*pScan)(INT32 col, INT32 row), void (*pTile)(INT32 offs, INT32 *tile_gfx, INT32 *tile_code, INT32 *tile_color, UINT32 *tile_flags), UINT32 tile_width, UINT32 tile_height, UINT32 map_width, UINT32 map_height);

// SetGfx sets information on the tile data the tile map is drawing.
// This MUST be used in conjunction with GenericTilemapInit
// num		- which graphics data to set as
// gfxbase	- pointer to tile data
// depth	- how many bits per pixel
// tile_width	- how many pixels wide is the tile
// tile_high	- how many pixels high is the tile
// gfxlen	- how many pixels bytes total are there in the tile data (this is used to ensure we don't go outside the maximum)
// color_offset - this is where the color for this tilemap starts
// color_mask	- how many colors can the tilemap use for *color (called in pScan)
void GenericTilemapSetGfx(INT32 num, UINT8 *gfxbase, INT32 depth, INT32 tile_width, INT32 tile_height, INT32 gfxlen, UINT32 color_offset, UINT32 color_mask);

// Exit tilemap (called in tiles_generic)
void GenericTilemapExit();

// Set a single transparent color 0 - 255
void GenericTilemapSetTransparent(INT32 which, UINT32 transparent);

// Set a mask for transparent colors, only works with 4bpp or less tiles
void GenericTilemapSetTransMask(INT32 which, UINT16 transmask);

// Set entry (color) in transparency table (1 if transparent, 0 if opaque)
// works for depths from 1bpp (2 entries) to 8bpp (256 entries)
void GenericTilemapSetTransTable(INT32 which, INT32 color, INT32 transparent);

// Set scroll x (horizontal) for the tilemap
void GenericTilemapSetScrollX(INT32 which, INT32 scrollx);

// Set scroll y (vertical) for the tile map
void GenericTilemapSetScrollY(INT32 which, INT32 scrolly);

// Set how many columns there are to scroll. Used in conjunction with TilemapSetScrollCol...
void GenericTilemapSetScrollCols(INT32 which, UINT32 cols);

// Set how many rows there are to scroll. Used in conjunction with TilemapSetScrollRow...
void GenericTilemapSetScrollRows(INT32 which, UINT32 rows);

// Set scroll value for individual row. Must set GenericTilemapSetScrollRows first!
void GenericTilemapSetScrollRow(INT32 which, INT32 row, INT32 scroll);

// Set scroll value for individual column. Must set GenericTilemapSetScrollCols first!
void GenericTilemapSetScrollCol(INT32 which, INT32 col, INT32 scroll);

// Set video offsets, this allows adjusting where the tilemap is displayed on the screen
// This is applied AFTER the scroll values
void GenericTilemapSetOffsets(INT32 which, INT32 x, INT32 y);

// Used to flip the tilemap on the screen vertically or horizontally
// Very useful for *flipscreen
void GenericTilemapSetFlip(INT32 which, INT32 flip);

// Disable (0) or enable (1) tile map (draw or don't draw)
void GenericTilemapSetEnable(INT32 which, INT32 enable);

// Actually draw the tilemap.
// which 	- select which tilemap to draw
// Bitmap	- pointer to the bitmap to draw the tilemap
// priority	- this will be used to set priority data, group is data passed in this variable using TILE_GROUP(x)
void GenericTilemapDraw(INT32 which, UINT16 *Bitmap, INT32 priority);
