// PGM2 Draw - Video rendering (IGS037 video chip)
// Direct pixel rendering: sprites, BG 32x32 tiles (7bpp), FG 8x8 tiles (4bpp)
// Based on MAME pgm2_v.cpp (David Haywood) adapted to FBNeo conventions
// API: pgm2InitDraw / pgm2ExitDraw / pgm2DoDraw / pgm2SnapshotOam / pgm2SnapshotScroll / pgm2ScanDraw

#include "pgm2.h"
#include "burnint.h"

// ---------------------------------------------------------------------------
// Internal sprite render buffer
//   0x8000 = transparent
//   bit 12 = priority (1=behind BG, 0=in front of BG)
//   bits 11:0 = direct index into Pgm2SpPal[]
// ---------------------------------------------------------------------------
#define PGM2_SPRBUF_W   512
#define PGM2_SPRBUF_H   240

static UINT16 *Pgm2SpriteBuf = NULL;

// OAM double-buffer (8KB snapshot taken at vblank)
#define OAM_SNAPSHOT_SIZE  0x2000
static UINT8  OamSnapshot[OAM_SNAPSHOT_SIZE];

// Scroll/lineram snapshot (captured at vblank before IRQ fires)
#define LINERAM_SNAPSHOT_SIZE 0x400
static UINT8  SnapLineRAM[LINERAM_SNAPSHOT_SIZE];
static UINT32 SnapBgScroll;    // VideoRegs[0] at 0x30120000
static UINT32 SnapFgScroll;    // VideoRegs[2] at 0x30120008
static UINT32 SnapVidMode;     // VideoRegs[3] at 0x3012000C

// ROM geometry cached at init
static UINT32 nSprMaskBase;    // byte offset in SprROM where mask data starts
static UINT32 nSprMaskLen;     // total mask data bytes
static UINT32 nSprColBase;     // byte offset in SprROM where colour data starts
static UINT32 nSprColLen;      // total colour data bytes
static UINT32 nBgRomLen;       // BG tile ROM length

// ---------------------------------------------------------------------------
// Pixel output helper (writes directly to pBurnDraw)
// ---------------------------------------------------------------------------
static inline void pgm2PutPix(INT32 x, INT32 y, UINT32 col)
{
	if ((UINT32)x < (UINT32)nScreenWidth && (UINT32)y < (UINT32)nScreenHeight)
		PutPix(pBurnDraw + y * nBurnPitch + x * nBurnBpp, col);
}

// ---------------------------------------------------------------------------
// Init / Exit
// ---------------------------------------------------------------------------

void pgm2InitDraw()
{
	GenericTilesInit();
	GenericTilesClearClip();

	Pgm2SpriteBuf = (UINT16*)BurnMalloc(PGM2_SPRBUF_W * PGM2_SPRBUF_H * sizeof(UINT16));

	// Cache ROM geometry for sprite mask/colour windows
	nSprMaskBase = (Pgm2MaskROMOffset > 0) ? (UINT32)Pgm2MaskROMOffset : 0u;
	nSprMaskLen  = (Pgm2MaskROMLen > 0) ? (UINT32)Pgm2MaskROMLen : 0u;
	nSprColBase  = (Pgm2ColourROMOffset > 0) ? (UINT32)Pgm2ColourROMOffset : 0u;
	nSprColLen   = (Pgm2SprROMLen > (INT32)nSprColBase)
		? (UINT32)(Pgm2SprROMLen - nSprColBase) : 0u;
	nBgRomLen    = (Pgm2BgROMLen > 0) ? (UINT32)Pgm2BgROMLen : 0u;

	memset(OamSnapshot, 0, sizeof(OamSnapshot));
	memset(SnapLineRAM, 0, sizeof(SnapLineRAM));
	SnapBgScroll = 0;
	SnapFgScroll = 0;
	SnapVidMode  = 0;
}

void pgm2ExitDraw()
{
	GenericTilesExit();
	BurnFree(Pgm2SpriteBuf);
	Pgm2SpriteBuf = NULL;
}

// ---------------------------------------------------------------------------
// OAM / Scroll snapshot (called from pgm2Frame at vblank)
// ---------------------------------------------------------------------------

void pgm2SnapshotOam()
{
	if (Pgm2SpOAM)
		memcpy(OamSnapshot, Pgm2SpOAM, OAM_SNAPSHOT_SIZE);
}

void pgm2SnapshotScroll()
{
	if (Pgm2VideoRegs) {
		SnapBgScroll = Pgm2VideoRegs[0];
		SnapFgScroll = Pgm2VideoRegs[2];
		SnapVidMode  = Pgm2VideoRegs[3];
	}
	if (Pgm2LineRAM)
		memcpy(SnapLineRAM, Pgm2LineRAM, LINERAM_SNAPSHOT_SIZE);
}

// ---------------------------------------------------------------------------
// Sprite rendering (1bpp mask + 6bpp colour, per-bit zoom from ZoomRAM)
// ---------------------------------------------------------------------------

static inline INT32 popcount32(UINT32 x)
{
	x = x - ((x >> 1) & 0x55555555);
	x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
	return (((x + (x >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}

static inline void pgm2_draw_sprite_pixel(UINT32 colour_base, UINT32 colour_mask,
	UINT32 palette_offset, INT32 realx, INT32 realy, UINT16 pal)
{
	if ((UINT32)realx >= PGM2_SPRBUF_W || (UINT32)realy >= PGM2_SPRBUF_H) return;
	UINT8 pix = Pgm2SprROM[colour_base + (palette_offset & colour_mask)] & 0x3F;
	Pgm2SpriteBuf[realy * PGM2_SPRBUF_W + realx] = (UINT16)(pix + pal);
}

static inline void pgm2_skip_sprite_chunk(UINT32 &palette_offset, UINT32 maskdata,
	bool reverse, UINT32 colour_mask)
{
	INT32 bits = popcount32(maskdata);
	if (!reverse) palette_offset += bits;
	else          palette_offset -= bits;
	palette_offset &= colour_mask;
}

static void pgm2_draw_sprite_chunk(UINT32 colour_base, UINT32 colour_mask,
	UINT32 &palette_offset, INT32 x, INT32 realy, UINT16 pal,
	UINT32 maskdata, UINT32 zoomx_bits, UINT8 repeats,
	INT32 &realxdraw, INT32 realdraw_inc, INT32 palette_inc)
{
	for (INT32 xchunk = 0; xchunk < 32; xchunk++)
	{
		UINT8 pix, xzoombit;
		if (palette_inc == -1) {
			pix = (maskdata >> xchunk) & 1;
			xzoombit = (zoomx_bits >> xchunk) & 1;
		} else {
			pix = (maskdata >> (31 - xchunk)) & 1;
			xzoombit = (zoomx_bits >> (31 - xchunk)) & 1;
		}

		if (pix) {
			if (repeats != 0) {
				for (INT32 i = 0; i < repeats; i++) {
					pgm2_draw_sprite_pixel(colour_base, colour_mask, palette_offset,
						x + realxdraw, realy, pal);
					realxdraw += realdraw_inc;
				}
				if (xzoombit) {
					pgm2_draw_sprite_pixel(colour_base, colour_mask, palette_offset,
						x + realxdraw, realy, pal);
					realxdraw += realdraw_inc;
				}
				palette_offset = (palette_offset + palette_inc) & colour_mask;
			} else {
				if (xzoombit)
					pgm2_draw_sprite_pixel(colour_base, colour_mask, palette_offset,
						x + realxdraw, realy, pal);
				palette_offset = (palette_offset + palette_inc) & colour_mask;
				if (xzoombit) realxdraw += realdraw_inc;
			}
		} else {
			if (repeats != 0) {
				for (INT32 i = 0; i < repeats; i++) realxdraw += realdraw_inc;
				if (xzoombit) realxdraw += realdraw_inc;
			} else {
				if (xzoombit) realxdraw += realdraw_inc;
			}
		}
	}
}

static void pgm2_draw_sprite_line(UINT32 mask_base, UINT32 mask_mask,
	UINT32 colour_base, UINT32 colour_mask, UINT32 realspritekey,
	UINT32 &mask_offset, UINT32 &palette_offset,
	INT32 x, INT32 realy, bool flipx, bool reverse,
	UINT16 sizex, UINT16 pal, UINT8 zoomybit, UINT32 zoomx_bits, UINT8 xrepeats)
{
	INT32 realxdraw = 0;
	if (flipx ^ reverse)
		realxdraw = (popcount32(zoomx_bits) * sizex) - 1;

	for (INT32 xdraw = 0; xdraw < sizex; xdraw++)
	{
		UINT32 moff = mask_base + (mask_offset & mask_mask);
		UINT32 maskdata = ((UINT32)Pgm2SprROM[moff + 0] << 24)
			| ((UINT32)Pgm2SprROM[moff + 1] << 16)
			| ((UINT32)Pgm2SprROM[moff + 2] << 8)
			| ((UINT32)Pgm2SprROM[moff + 3]);
		maskdata ^= realspritekey;

		if (reverse) mask_offset -= 4;
		else         mask_offset += 4;
		mask_offset &= mask_mask;

		if (zoomybit) {
			if (!flipx) {
				if (!reverse)
					pgm2_draw_sprite_chunk(colour_base, colour_mask, palette_offset,
						x, realy, pal, maskdata, zoomx_bits, xrepeats, realxdraw, 1, 1);
				else
					pgm2_draw_sprite_chunk(colour_base, colour_mask, palette_offset,
						x, realy, pal, maskdata, zoomx_bits, xrepeats, realxdraw, -1, -1);
			} else {
				if (!reverse)
					pgm2_draw_sprite_chunk(colour_base, colour_mask, palette_offset,
						x, realy, pal, maskdata, zoomx_bits, xrepeats, realxdraw, -1, 1);
				else
					pgm2_draw_sprite_chunk(colour_base, colour_mask, palette_offset,
						x, realy, pal, maskdata, zoomx_bits, xrepeats, realxdraw, 1, -1);
			}
		} else {
			pgm2_skip_sprite_chunk(palette_offset, maskdata, reverse, colour_mask);
		}
	}
}

static void pgm2_draw_sprites()
{
	if (!Pgm2SpriteBuf || !Pgm2SprROM) return;
	if (!nSprMaskLen || !nSprColLen) return;

	// Clear buffer to transparent
	for (INT32 i = 0; i < PGM2_SPRBUF_W * PGM2_SPRBUF_H; i++)
		Pgm2SpriteBuf[i] = 0x8000;

	UINT32 *oam = (UINT32*)OamSnapshot;

	// Sprite mask encryption key (bitswap<32>(key ^ 0x90055555, 0..31))
	UINT32 realspritekey = 0;
	if (Pgm2VideoRegs) {
		UINT32 v = BURN_ENDIAN_SWAP_INT32(Pgm2VideoRegs[0x38 / 4]) ^ 0x90055555;
		v = ((v >> 1) & 0x55555555) | ((v & 0x55555555) << 1);
		v = ((v >> 2) & 0x33333333) | ((v & 0x33333333) << 2);
		v = ((v >> 4) & 0x0F0F0F0F) | ((v & 0x0F0F0F0F) << 4);
		v = ((v >> 8) & 0x00FF00FF) | ((v & 0x00FF00FF) << 8);
		v = (v >> 16) | (v << 16);
		realspritekey = v;
	}

	// Find end-of-list (w2 bit 31)
	INT32 eol_dword = -1;
	for (INT32 di = 0; di < (OAM_SNAPSHOT_SIZE / 4); di += 4)
		if (BURN_ENDIAN_SWAP_INT32(oam[di + 2]) & 0x80000000u)
			{ eol_dword = di; break; }
	if (eol_dword == -1) return;

	UINT32 *zoom_tab = Pgm2SpZoom;
	UINT32 mask_mask = nSprMaskLen - 1;
	UINT32 colour_mask = nSprColLen - 1;

	for (INT32 di = 0; di < eol_dword - 2; di += 4)
	{
		UINT32 w0 = BURN_ENDIAN_SWAP_INT32(oam[di + 0]);
		UINT32 w1 = BURN_ENDIAN_SWAP_INT32(oam[di + 1]);
		UINT32 w2 = BURN_ENDIAN_SWAP_INT32(oam[di + 2]);
		UINT32 w3 = BURN_ENDIAN_SWAP_INT32(oam[di + 3]);

		INT32  sx = (INT32)(w0 & 0x7FFu); if (sx & 0x400) sx -= 0x800;
		INT32  sy = (INT32)((w0 >> 11) & 0x7FFu); if (sy & 0x400) sy -= 0x800;
		INT32  pal = (INT32)((w0 >> 22) & 0x3Fu);
		INT32  pri = (INT32)((w0 >> 31) & 1);
		if (w0 & 0x40000000) continue; // disable bit

		INT32  sizex   = (INT32)(w1 & 0x3Fu);
		INT32  sizey   = (INT32)((w1 >> 6) & 0x1FFu);
		INT32  zoomx_i = (INT32)((w1 >> 16) & 0x7Fu);
		INT32  flipx   = (INT32)((w1 >> 23) & 1);
		INT32  zoomy_i = (INT32)((w1 >> 24) & 0x7Fu);
		INT32  reverse = (INT32)((w1 >> 31) & 1);

		UINT32 mask_off = (w2 << 1);
		UINT32 col_off  = w3;

		if (!sizex || !sizey) continue;

		UINT32 zx_bits = zoom_tab ? BURN_ENDIAN_SWAP_INT32(zoom_tab[zoomx_i & 0x7F]) : 0xFFFFFFFFu;
		UINT32 zy_bits = zoom_tab ? BURN_ENDIAN_SWAP_INT32(zoom_tab[zoomy_i & 0x7F]) : 0xFFFFFFFFu;
		INT32 xrepeats = (zoomx_i & 0x60) >> 5;
		INT32 yrepeats = (zoomy_i & 0x60) >> 5;

		// pal_base: bits 11:0 = pal * 64, bit 12 = priority
		UINT16 pal_base = (UINT16)((pal * 0x40) | (pri << 12));

		if (reverse) mask_off -= 2;
		mask_off &= mask_mask;
		col_off  &= colour_mask;

		INT32 realy = sy;
		INT32 sourceline = 0;

		for (INT32 ydraw = 0; ydraw < sizey; sourceline++)
		{
			UINT8 zy_bit = (zy_bits >> (sourceline & 0x1F)) & 1;
			UINT32 pre_mask_off = mask_off;
			UINT32 pre_col_off = col_off;

			if (yrepeats != 0) {
				for (INT32 rept = 0; rept < yrepeats; rept++) {
					mask_off = pre_mask_off;
					col_off = pre_col_off;
					pgm2_draw_sprite_line(nSprMaskBase, mask_mask, nSprColBase, colour_mask,
						realspritekey, mask_off, col_off, sx, realy,
						flipx != 0, reverse != 0, (UINT16)sizex, pal_base,
						1, zx_bits, (UINT8)xrepeats);
					realy++;
				}
				if (zy_bit) {
					mask_off = pre_mask_off;
					col_off = pre_col_off;
					pgm2_draw_sprite_line(nSprMaskBase, mask_mask, nSprColBase, colour_mask,
						realspritekey, mask_off, col_off, sx, realy,
						flipx != 0, reverse != 0, (UINT16)sizex, pal_base,
						1, zx_bits, (UINT8)xrepeats);
					realy++;
				}
				ydraw++;
			} else {
				pgm2_draw_sprite_line(nSprMaskBase, mask_mask, nSprColBase, colour_mask,
					realspritekey, mask_off, col_off, sx, realy,
					flipx != 0, reverse != 0, (UINT16)sizex, pal_base,
					1, zx_bits, (UINT8)xrepeats);
				if (zy_bit) realy++;
				ydraw++;
			}
		}
	}
}

// Blit one priority layer from sprite buffer to screen (direct RGB)
static void pgm2_blit_sprites(INT32 w, INT32 h, INT32 pri)
{
	if (!Pgm2SpriteBuf || !Pgm2SpPal) return;
	UINT16 pri_mask = (UINT16)(pri << 12);
	for (INT32 y = 0; y < h; y++) {
		UINT16 *src = Pgm2SpriteBuf + y * PGM2_SPRBUF_W;
		for (INT32 x = 0; x < w; x++) {
			UINT16 pix = src[x];
			if (pix != 0x8000 && (pix & 0x1000) == pri_mask) {
				UINT32 rgb = BURN_ENDIAN_SWAP_INT32(Pgm2SpPal[pix & 0xFFF]);
				pgm2PutPix(x, y, BurnHighCol((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF, 0));
			}
		}
	}
}

// ---------------------------------------------------------------------------
// BG tilemap  (32x32 pixel tiles, 64x32 tile map, 7bpp)
// Tile data: raw bytes in Pgm2SprROM[0..BgROMLen-1], pen = byte & 0x7F
// VRAM entry: bits 17:0 = tile number, bits 21:18 = colour (×128), bits 24:23 = flipxy
// ---------------------------------------------------------------------------
static void pgm2_draw_bg(INT32 w, INT32 h)
{
	if (!Pgm2BgVRAM || !Pgm2SprROM || !Pgm2BgPal) return;

	UINT32 *vram = (UINT32*)Pgm2BgVRAM;
	UINT32 bgscroll = BURN_ENDIAN_SWAP_INT32(SnapBgScroll);
	INT32  scroll_x = (INT32)(bgscroll & 0xFFFF);
	INT32  scroll_y = (INT32)((bgscroll >> 16) & 0xFFFF);

	const INT32 MAP_W = 64, MAP_H = 32;
	const INT32 TILE_W = 32, TILE_H = 32;
	const INT32 VIRT_W = MAP_W * TILE_W;   // 2048
	const INT32 VIRT_H = MAP_H * TILE_H;   // 1024
	const UINT32 BG_TILE_SZ = TILE_W * TILE_H; // 1024 bytes per tile
	const UINT32 bg_tile_count = (nBgRomLen >= BG_TILE_SZ) ? (nBgRomLen / BG_TILE_SZ) : 0u;
	if (bg_tile_count == 0) return;

	const bool tile_count_pow2 = (bg_tile_count & (bg_tile_count - 1)) == 0;
	UINT32 *lineram = (UINT32*)SnapLineRAM;

	for (INT32 py = 0; py < h; py++)
	{
		INT32 src_y = (py + scroll_y) & (VIRT_H - 1);
		INT32 ty = src_y / TILE_H, ty_off = src_y % TILE_H;

		// Per-line BG scroll (packed: even line = low 16, odd line = high 16)
		// MAME indexes lineram by screen row (py), not tilemap-space row (src_y)
		INT32 line_scroll_x = scroll_x;
		if (lineram) {
			UINT32 lrval = BURN_ENDIAN_SWAP_INT32(lineram[(py >> 1) & 0xFF]);
			UINT16 ls16 = (py & 1) ? (UINT16)((lrval >> 16) & 0xFFFF) : (UINT16)(lrval & 0xFFFF);
			line_scroll_x += (INT32)(INT16)ls16;
		}

		INT32 px = 0;
		while (px < w)
		{
			INT32 src_x = (px + line_scroll_x) & (VIRT_W - 1);
			INT32 tx = src_x / TILE_W;
			INT32 tx_start = src_x % TILE_W;

			UINT32 entry = BURN_ENDIAN_SWAP_INT32(vram[ty * MAP_W + tx]);
			UINT32 tileno = entry & 0x3FFFFu;
			UINT8  colour = (entry >> 18) & 0xFu;
			UINT8  flipxy = (entry >> 24) & 3u;

			if (tile_count_pow2) tileno &= (bg_tile_count - 1);
			else                 tileno %= bg_tile_count;

			UINT32 tile_base = tileno * BG_TILE_SZ;
			bool tile_valid = (tile_base + BG_TILE_SZ <= nBgRomLen);

			INT32 fy_base = (flipxy & 2) ? (TILE_H - 1 - ty_off) : ty_off;
			const UINT8 *tile_row = tile_valid ? (Pgm2SprROM + tile_base + fy_base * TILE_W) : NULL;

			INT32 tx_end = TILE_W;
			if (px + (tx_end - tx_start) > w) tx_end = tx_start + (w - px);

			if (tile_row) {
				UINT32 pal_base = colour * 0x80u;
				for (INT32 tx_off = tx_start; tx_off < tx_end; tx_off++, px++) {
					INT32 fx = (flipxy & 1) ? (TILE_W - 1 - tx_off) : tx_off;
					UINT8 pix = tile_row[fx] & 0x7F;
					if (pix == 0) continue;
					UINT32 rgb = BURN_ENDIAN_SWAP_INT32(Pgm2BgPal[(pal_base + pix) & 0x7FFu]);
					pgm2PutPix(px, py, BurnHighCol((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF, 0));
				}
			} else {
				px += (tx_end - tx_start);
			}
		}
	}
}

// ---------------------------------------------------------------------------
// FG (text) tilemap  (8x8 tiles, 4bpp packed, 96×64 tile map)
// gfx_8x8x4_packed_lsb: even pixel = low nibble, odd pixel = high nibble
// VRAM entry: bits 17:0 = tile number, bits 22:18 = colour (×16), bits 24:23 = flipxy
// ---------------------------------------------------------------------------
static void pgm2_draw_fg(INT32 w, INT32 h)
{
	if (!Pgm2FgVRAM || !Pgm2TileROM || !Pgm2TxPal) return;

	UINT32 *vram = (UINT32*)Pgm2FgVRAM;
	UINT32 fgscroll = BURN_ENDIAN_SWAP_INT32(SnapFgScroll);
	INT32  scroll_x = (INT32)(fgscroll & 0xFFFF);
	INT32  scroll_y = (INT32)((fgscroll >> 16) & 0xFFFF);

	const INT32 MAP_W = 96, MAP_H = 64;
	const INT32 TILE_W = 8, TILE_H = 8;
	const INT32 VIRT_W = MAP_W * TILE_W;   // 768
	const INT32 VIRT_H = MAP_H * TILE_H;   // 512
	const UINT32 FG_TILE_SZ = 32;          // 8×8 4bpp = 32 bytes per tile
	const UINT32 fg_rom_len = (Pgm2TileROMLen > 0) ? (UINT32)Pgm2TileROMLen : 0u;
	const UINT32 fg_tile_count = (fg_rom_len >= FG_TILE_SZ) ? (fg_rom_len / FG_TILE_SZ) : 0u;
	if (fg_tile_count == 0) return;

	const bool tile_count_pow2 = (fg_tile_count & (fg_tile_count - 1)) == 0;

	for (INT32 py = 0; py < h; py++)
	{
		INT32 src_y = ((py + scroll_y) % VIRT_H + VIRT_H) % VIRT_H;
		INT32 ty = src_y / TILE_H, ty_off = src_y % TILE_H;

		INT32 px = 0;
		while (px < w)
		{
			INT32 src_x = ((px + scroll_x) % VIRT_W + VIRT_W) % VIRT_W;
			INT32 tx = src_x / TILE_W;
			INT32 tx_start = src_x % TILE_W;

			UINT32 entry = BURN_ENDIAN_SWAP_INT32(vram[ty * MAP_W + tx]);
			UINT32 tileno = entry & 0x3FFFFu;
			UINT8  colour = (entry >> 18) & 0x1Fu;
			UINT8  flipxy = (entry >> 24) & 3u;

			if (tile_count_pow2) tileno &= (fg_tile_count - 1);
			else                 tileno %= fg_tile_count;

			INT32 fy = (flipxy & 2) ? (TILE_H - 1 - ty_off) : ty_off;
			UINT32 tile_row_base = tileno * FG_TILE_SZ + fy * (TILE_W / 2);
			bool tile_valid = (tile_row_base + (TILE_W / 2) <= fg_rom_len);

			INT32 tx_end = TILE_W;
			if (px + (tx_end - tx_start) > w) tx_end = tx_start + (w - px);

			if (tile_valid) {
				UINT32 pal_base = colour * 0x10u;
				for (INT32 tx_off = tx_start; tx_off < tx_end; tx_off++, px++) {
					INT32 fx = (flipxy & 1) ? (TILE_W - 1 - tx_off) : tx_off;
					UINT8 byte_val = Pgm2TileROM[tile_row_base + fx / 2];
					UINT8 pix = (fx & 1) ? (byte_val >> 4) : (byte_val & 0x0F);
					if (pix == 0) continue;
					UINT32 rgb = BURN_ENDIAN_SWAP_INT32(Pgm2TxPal[(pal_base + pix) & 0x1FFu]);
					pgm2PutPix(px, py, BurnHighCol((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF, 0));
				}
			} else {
				px += (tx_end - tx_start);
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Main draw function (called from pgm2Frame after all scanlines)
// ---------------------------------------------------------------------------

INT32 pgm2DoDraw()
{
	if (!pBurnDraw) return 0;

	// Determine screen size from video mode (snapshotted at vblank)
	INT32 w = 320, h = 240;
	{
		UINT32 vmode = BURN_ENDIAN_SWAP_INT32(SnapVidMode);
		switch ((vmode >> 16) & 3) {
			case 1: w = 448; h = 224; break;
			case 2: w = 512; h = 240; break;
		}
	}
	if (w > nScreenWidth) w = nScreenWidth;
	if (h > nScreenHeight) h = nScreenHeight;

	// Fill entire output with BG palette entry 0
	UINT32 bg_col = 0;
	if (Pgm2BgPal) {
		UINT32 rgb = BURN_ENDIAN_SWAP_INT32(Pgm2BgPal[0]);
		bg_col = BurnHighCol((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF, 0);
	}
	for (INT32 y = 0; y < nScreenHeight; y++)
		for (INT32 x = 0; x < nScreenWidth; x++)
			pgm2PutPix(x, y, bg_col);

	// Layer compositing (matches MAME screen_update):
	//   1. hi-pri sprites (behind BG)
	//   2. BG tilemap (32×32 tiles, 7bpp)
	//   3. lo-pri sprites (above BG, below FG)
	//   4. FG text tilemap (8×8 tiles, 4bpp)
	pgm2_draw_sprites();
	pgm2_blit_sprites(w, h, 1);
	pgm2_draw_bg(w, h);
	pgm2_blit_sprites(w, h, 0);
	pgm2_draw_fg(w, h);

	return 0;
}

// ---------------------------------------------------------------------------
// State save/restore for draw module
// ---------------------------------------------------------------------------

void pgm2ScanDraw(INT32 nAction)
{
	if (nAction & ACB_VOLATILE) {
		ScanVar(OamSnapshot, sizeof(OamSnapshot), "PGM2 OAM Snapshot");
		ScanVar(SnapLineRAM, sizeof(SnapLineRAM), "PGM2 Line Scroll Snap");
		SCAN_VAR(SnapBgScroll);
		SCAN_VAR(SnapFgScroll);
		SCAN_VAR(SnapVidMode);
	}
}
