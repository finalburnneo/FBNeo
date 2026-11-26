// Sega 315-5292 tile generator (System 24, Model 1, Model 2): 4 512x512 tilemaps of 8x8 4bpp chars
//  tile ram: 0x0000-0x3fff tilemaps, 0x4000 row scroll, 0x5000 scroll / control, 0x6000 window masks

#include "burnint.h"
#include "sega_315_5292.h"

static UINT8 *DrvTileRAM;
static UINT8 *DrvCharRAM;
static UINT8 *DrvCharExp;
static UINT16 *TileBitmap;	// 4 tilemaps, 512x512, bit 12 = category
static UINT32 *TileCache;	// tile ram value each TileBitmap tile was drawn with, ~0 = redraw
static UINT8 *CharDirty;	// chars written since they were last drawn
static UINT8 *TileFlags;	// per TileBitmap tile: bit 0 = category, bit 1 = has non-zero pens
static UINT16 *TileDst;
static UINT8 *TilePrio;		// optional, the layer's priority bit is or-ed in where it draws
static UINT8 TilePriMask;
static INT32 CharRAMSize;
static UINT16 TileMask;		// char number bits of a tile ram entry
static INT32 NATIVE_W;
static INT32 NATIVE_H;

void sega_315_5292_char_written(UINT32 offset)	// byte offset, even
{
	UINT16 w = BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvCharRAM + offset)));
	UINT8 *d = DrvCharExp + offset * 2;
	CharDirty[offset >> 5] = 1;
	d[0] = (w >> 12) & 0xf;
	d[1] = (w >>  8) & 0xf;
	d[2] = (w >>  4) & 0xf;
	d[3] = (w >>  0) & 0xf;
}



void sega_315_5292_invalidate()
{
	memset(TileCache, 0xff, 4 * 64 * 64 * sizeof(UINT32));
}

// only the tiles whose tile ram entry or char changed are drawn again
static void render_tilemap_bitmap(INT32 tmap)
{
	UINT16 *ram = (UINT16*)DrvTileRAM + tmap * 0x1000;
	UINT16 *dst = TileBitmap + tmap * 512 * 512;
	UINT32 *cache = TileCache + tmap * 64 * 64;

	for (INT32 ty = 0; ty < 64; ty++) {
		for (INT32 tx = 0; tx < 64; tx++) {
			UINT16 val = BURN_ENDIAN_SWAP_INT16(ram[ty * 64 + tx]);
			UINT32 *c = cache + ty * 64 + tx;
			if (*c == val && !CharDirty[val & TileMask]) continue;
			*c = val;

			UINT8 *gfx = DrvCharExp + (val & TileMask) * 64;
			UINT16 color = (((val >> 7) & 0xff) << 4) | ((val & 0x8000) ? 0x1000 : 0);
			UINT16 *d = dst + (ty * 8) * 512 + tx * 8;
			UINT8 pens = 0;

			for (INT32 y = 0; y < 8; y++, d += 512, gfx += 8) {
				for (INT32 x = 0; x < 8; x++) {
					d[x] = color | gfx[x];
					pens |= gfx[x];
				}
			}

			TileFlags[tmap * 64 * 64 + ty * 64 + tx] = (color >> 12) | (pens ? 2 : 0);
		}
	}
}

// copy a span of a tilemap bitmap to the screen
// tpri: category to draw, opaque: 0 = transparent pen 0 and matching category only,
// 1 = every pixel (normal mode), 2 = matching category, pen 0 included (special modes)
static inline void draw_tile_span(UINT16 *src_line, INT32 srcx, INT32 y, INT32 x1, INT32 x2, INT32 tpri, INT32 opaque, const UINT16 *mask, INT32 win)
{
	UINT16 *dst = TileDst + y * NATIVE_W;
	UINT8 *pri = TilePrio ? (TilePrio + y * NATIVE_W) : NULL;
	const UINT16 cat = tpri << 12;

	// tiles of the other category (and without pens, when transparent) are skipped
	INT32 ofs = (INT32)(src_line - TileBitmap);
	const UINT8 *flags = TileFlags + (ofs >> 18) * 64 * 64 + ((ofs >> 12) & 0x3f) * 64;
	const UINT8 want = tpri | (opaque == 0 ? 2 : 0);
	const UINT8 test = (opaque == 0) ? 3 : 1;

	// the window mask has a bit per 8 screen pixels
	for (INT32 x = x1; x < x2; ) {
		INT32 xe = (x | 7) + 1;
		if (xe > x2) xe = x2;

		if (mask) {
			UINT16 m = BURN_ENDIAN_SWAP_INT16(mask[x >> 7]);
			if (win) m = ~m;
			if (m & (0x8000 >> ((x & 0x7f) >> 3))) {
				x = xe;
				continue;
			}
		}

		if (opaque != 1) {
			UINT8 f1 = flags[((srcx + x) >> 3) & 0x3f];
			UINT8 f2 = flags[((srcx + xe - 1) >> 3) & 0x3f];
			if ((f1 & test) != want && (f2 & test) != want) {
				x = xe;
				continue;
			}
		}

		if (pri) {
			for (; x < xe; x++) {
				UINT16 pix = src_line[(srcx + x) & 0x1ff];
				if (opaque == 1 || ((pix & 0x1000) == cat && (opaque == 2 || (pix & 0xf)))) {
					dst[x] = pix & 0xfff;
					pri[x] |= TilePriMask;
				}
			}
			continue;
		}

		switch (opaque) {
			case 1:
				for (; x < xe; x++) dst[x] = src_line[(srcx + x) & 0x1ff] & 0xfff;
				break;

			case 2:
				for (; x < xe; x++) {
					UINT16 pix = src_line[(srcx + x) & 0x1ff];
					if ((pix & 0x1000) == cat) dst[x] = pix & 0xfff;
				}
				break;

			default:
				for (; x < xe; x++) {
					UINT16 pix = src_line[(srcx + x) & 0x1ff];
					if ((pix & 0x1000) == cat && (pix & 0xf)) dst[x] = pix & 0xfff;
				}
				break;
		}
	}
}

void sega_315_5292_draw_layer(INT32 layer, INT32 opaque, UINT16 *dst, UINT8 *prio, UINT8 primask)
{
	TileDst = dst;
	TilePrio = prio;
	TilePriMask = primask;

	UINT16 *tile_ram = (UINT16*)DrvTileRAM;
	UINT16 hscr = BURN_ENDIAN_SWAP_INT16(tile_ram[0x5000 + (layer >> 1)]);
	UINT16 vscr = BURN_ENDIAN_SWAP_INT16(tile_ram[0x5004 + (layer >> 1)]);
	UINT16 ctrl = BURN_ENDIAN_SWAP_INT16(tile_ram[0x5004 + ((layer >> 1) & 2)]);
	UINT16 *mask = tile_ram + ((layer & 4) ? 0x6800 : 0x6000);
	INT32 tpri = layer & 1;

	layer >>= 1;

	// layer disable
	if (vscr & 0x8000) return;

	if (ctrl & 0x6000) {
		// special window/scroll modes, on pairs of tilemaps
		if (layer & 1) return;

		INT32 op = opaque ? 2 : 0;
		INT32 sy = vscr & 0x1ff;

		if (hscr & 0x8000) {
			UINT16 *hscrtb = tile_ram + 0x4000 + 0x200 * layer;

			switch ((ctrl & 0x6000) >> 13) {
				case 1: {
					UINT16 v = (-vscr) & 0x1ff;
					INT32 l = layer;
					if (!((-vscr) & 0x200)) l ^= 1;

					for (INT32 y = 0; y < NATIVE_H; y++) {
						INT32 l1 = (y >= v) ? (l ^ 1) : l;
						INT32 h = BURN_ENDIAN_SWAP_INT16(hscrtb[y]) & 0x1ff;
						UINT16 *src = TileBitmap + l1 * 512 * 512 + ((y + sy) & 0x1ff) * 512;
						draw_tile_span(src, -h, y, 0, NATIVE_W, tpri, op, NULL, 0);
					}
					break;
				}

				case 2:
				case 3: {
					for (INT32 y = 0; y < NATIVE_H; y++) {
						UINT16 hv = BURN_ENDIAN_SWAP_INT16(hscrtb[y]);
						INT32 h = hv & 0x1ff;
						INT32 l1 = layer;
						if (!(hv & 0x200)) l1 ^= 1;	// the line's own scroll value picks the side

						INT32 split = (h < NATIVE_W) ? h : NATIVE_W;
						UINT16 *src1 = TileBitmap + l1 * 512 * 512 + ((y + sy) & 0x1ff) * 512;
						UINT16 *src2 = TileBitmap + (l1 ^ 1) * 512 * 512 + ((y + sy) & 0x1ff) * 512;
						draw_tile_span(src1, -h, y, 0, split, tpri, op, NULL, 0);
						draw_tile_span(src2, -h, y, split, NATIVE_W, tpri, op, NULL, 0);
					}
					break;
				}
			}
		} else {
			INT32 h = hscr & 0x1ff;

			switch ((ctrl & 0x6000) >> 13) {
				case 1: {
					UINT16 v = (-vscr) & 0x1ff;
					INT32 l = layer;
					if (!((-vscr) & 0x200)) l ^= 1;

					for (INT32 y = 0; y < NATIVE_H; y++) {
						INT32 l1 = (y >= v) ? (l ^ 1) : l;
						UINT16 *src = TileBitmap + l1 * 512 * 512 + ((y + sy) & 0x1ff) * 512;
						draw_tile_span(src, -h, y, 0, NATIVE_W, tpri, op, NULL, 0);
					}
					break;
				}

				case 2:
				case 3: {
					INT32 l = layer;
					if (!(hscr & 0x200)) l ^= 1;
					INT32 split = (h < NATIVE_W) ? h : NATIVE_W;

					for (INT32 y = 0; y < NATIVE_H; y++) {
						UINT16 *src1 = TileBitmap + l * 512 * 512 + ((y + sy) & 0x1ff) * 512;
						UINT16 *src2 = TileBitmap + (l ^ 1) * 512 * 512 + ((y + sy) & 0x1ff) * 512;
						draw_tile_span(src1, -h, y, 0, split, tpri, op, NULL, 0);
						draw_tile_span(src2, -h, y, split, NATIVE_W, tpri, op, NULL, 0);
					}
					break;
				}
			}
		}
	} else {
		// normal mode, with the window mask
		INT32 win = layer & 1;
		UINT16 *bm = TileBitmap + layer * 512 * 512;
		INT32 op = opaque ? 1 : 0;

		if (hscr & 0x8000) {
			UINT16 *hscrtb = tile_ram + 0x4000 + 0x200 * layer;
			INT32 sy = vscr & 0x1ff;

			for (INT32 y = 0; y < NATIVE_H; y++) {
				INT32 h = (-BURN_ENDIAN_SWAP_INT16(hscrtb[y])) & 0x1ff;
				draw_tile_span(bm + ((sy + y) & 0x1ff) * 512, h, y, 0, NATIVE_W, tpri, op, mask + y * 4, win);
			}
		} else {
			INT32 h = (-hscr) & 0x1ff;
			INT32 sy = vscr & 0x1ff;

			for (INT32 y = 0; y < NATIVE_H; y++) {
				draw_tile_span(bm + ((sy + y) & 0x1ff) * 512, h, y, 0, NATIVE_W, tpri, op, mask + y * 4, win);
			}
		}
	}
}

void sega_315_5292_update()
{
	UINT16 *tile_ram = (UINT16*)DrvTileRAM;
	for (INT32 i = 0; i < 4; i++) {
		if (!(BURN_ENDIAN_SWAP_INT16(tile_ram[0x5004 + i]) & 0x8000) || (BURN_ENDIAN_SWAP_INT16(tile_ram[0x5004 + (i & 2)]) & 0x6000))
			render_tilemap_bitmap(i);
		else
			memset(TileCache + i * 64 * 64, 0xff, 64 * 64 * sizeof(UINT32));	// misses the char changes
	}
	memset(CharDirty, 0, 0x4000);
}


void sega_315_5292_init(UINT8 *tileram, UINT8 *charram, INT32 charram_size, UINT16 tile_mask, INT32 width, INT32 height)
{
	DrvTileRAM = tileram;
	DrvCharRAM = charram;
	CharRAMSize = charram_size;
	TileMask = tile_mask;
	NATIVE_W = width;
	NATIVE_H = height;

	DrvCharExp = (UINT8*)BurnMalloc(0x100000);
	TileBitmap = (UINT16*)BurnMalloc(4 * 512 * 512 * sizeof(UINT16));
	TileCache = (UINT32*)BurnMalloc(4 * 64 * 64 * sizeof(UINT32));
	CharDirty = (UINT8*)BurnMalloc(0x4000);
	TileFlags = (UINT8*)BurnMalloc(4 * 64 * 64);

	memset(DrvCharExp, 0, 0x100000);
	memset(TileBitmap, 0, 4 * 512 * 512 * sizeof(UINT16));
	memset(CharDirty, 0, 0x4000);
	memset(TileFlags, 0, 4 * 64 * 64);
	sega_315_5292_invalidate();
}

void sega_315_5292_exit()
{
	BurnFree(DrvCharExp);
	BurnFree(TileBitmap);
	BurnFree(TileCache);
	BurnFree(CharDirty);
	BurnFree(TileFlags);
}

void sega_315_5292_reset()
{
	memset(DrvCharExp, 0, 0x100000);
	sega_315_5292_invalidate();
}

// after a state load: the chars are expanded again from the char ram
void sega_315_5292_recalc()
{
	for (INT32 i = 0; i < CharRAMSize; i += 2) {
		sega_315_5292_char_written(i);
	}
	sega_315_5292_invalidate();
}
