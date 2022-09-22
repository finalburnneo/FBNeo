#include "tiles_generic.h"
#include "m68000_intf.h"
#include "burn_pal.h"

static UINT8 *AllRam;
static UINT8 *RamEnd;

static UINT8 *VideoRAM[3];
static UINT8 *SpriteRAM;
static UINT8 *TileRAM;
static UINT8 *VideoRegs;
static UINT8 *SpriteRegs;
static UINT8 *ScrollRegs;
static UINT8 *WindowRegs;
static UINT8 *BlitRegs;

INT32 i4x00_irq_enable;
INT32 i4x00_blitter_timer = -1;

static INT32 i4x00_cpu_speed; // to compute blitter delay

static INT32 rombank;
static INT32 screen_control;

static INT32 flipscreen;

UINT8 DrvRecalc;

static UINT8 *gfx8x8x8;
static UINT8 *gfx4x8x8;
static UINT32 graphics_length;

static INT32 support_16x16 = 0;
static INT32 support_8bpp = 1;
static INT32 tilemap_scrolldx[3] = { 0,0,0 };

static UINT16 (*irq_cause_read_cb)() = NULL;
static void (*irq_cause_write_cb)(UINT16 data) = NULL;
static void (*soundlatch_write_cb)(UINT16 data) = NULL;
static void (*additional_video_chips_cb)() = NULL;

static INT32 lastline = 0;
static INT32 clip_min_y = 0;
static INT32 clip_max_y = 0;
INT32 i4x00_raster_update = 0;

static INT32 is_blazing = 0;

static void palette_update()
{
	UINT16 *p = (UINT16*)(BurnPalRAM + 0x2000);

	for (INT32 i = 0; i < 0x2000 / 2; i++)
	{
		BurnPalette[i] = BurnHighCol(pal5bit(BURN_ENDIAN_SWAP_INT16(p[i]) >> 6), pal5bit(BURN_ENDIAN_SWAP_INT16(p[i]) >> 11), pal5bit(BURN_ENDIAN_SWAP_INT16(p[i]) >> 1), 0);
	}
}

static inline void palette_write(INT32 offset)
{
	UINT16 p = BURN_ENDIAN_SWAP_INT16(*((UINT16*)(BurnPalRAM + (offset & 0x3ffe))));

	BurnPalette[(offset / 2) & 0xfff] = BurnHighCol(pal5bit(p >> 6), pal5bit(p >> 11), pal5bit(p >> 1), 0);
}

static void draw_sprites()
{
	UINT16 *m_spriteram = (UINT16*)SpriteRAM;
	UINT16 *m_videoregs = (UINT16*)VideoRegs;
	UINT16 *m_spriteregs = (UINT16*)SpriteRegs;

	int max_x = (BURN_ENDIAN_SWAP_INT16(m_spriteregs[1])+1) * 2;
	int max_y = (BURN_ENDIAN_SWAP_INT16(m_spriteregs[0])+1) * 2;

	INT32 m_sprite_xoffs = BURN_ENDIAN_SWAP_INT16(m_videoregs[0x06 / 2]) - (BURN_ENDIAN_SWAP_INT16(m_spriteregs[1])+1);
	INT32 m_sprite_yoffs = BURN_ENDIAN_SWAP_INT16(m_videoregs[0x04 / 2]) - (BURN_ENDIAN_SWAP_INT16(m_spriteregs[0])+1);

	UINT32 gfx_size = graphics_length;

	INT32 max_sprites = 0x1000 / 8;
	INT32 sprites     = BURN_ENDIAN_SWAP_INT16(m_videoregs[0x00/2]) % max_sprites;

	INT32 color_start = (BURN_ENDIAN_SWAP_INT16(m_videoregs[0x08/2]) & 0x0f) << 4;

	INT32 i, j, pri;
	static const INT32 primask[4] = { 0x0000, 0xff00, 0xff00 | 0xf0f0, 0xff00 | 0xf0f0 | 0xcccc };

	UINT16 *src;
	INT32 inc;

	if (sprites == 0)
		return;

	for (i = 0; i < 0x20; i++)
	{
		if (!(BURN_ENDIAN_SWAP_INT16(m_videoregs[0x02/2]) & 0x8000))
		{
			src = m_spriteram + (sprites - 1) * (8 / 2);
			inc = -(8 / 2);
		} else {
			src = m_spriteram;
			inc = (8 / 2);
		}

		for (j = 0; j < sprites; j++)
		{
			INT32 x, y, attr, code, color, flipx, flipy, zoom, curr_pri, width, height;

			static const INT32 zoomtable[0x40] = {
				0xAAC,0x800,0x668,0x554,0x494,0x400,0x390,0x334,0x2E8,0x2AC,0x278,0x248,0x224,0x200,0x1E0,0x1C8,
				0x1B0,0x198,0x188,0x174,0x164,0x154,0x148,0x13C,0x130,0x124,0x11C,0x110,0x108,0x100,0x0F8,0x0F0,
				0x0EC,0x0E4,0x0DC,0x0D8,0x0D4,0x0CC,0x0C8,0x0C4,0x0C0,0x0BC,0x0B8,0x0B4,0x0B0,0x0AC,0x0A8,0x0A4,
				0x0A0,0x09C,0x098,0x094,0x090,0x08C,0x088,0x080,0x078,0x070,0x068,0x060,0x058,0x050,0x048,0x040
			};

			x = BURN_ENDIAN_SWAP_INT16(src[0]);
			curr_pri = (x & 0xf800) >> 11;

			if ((curr_pri == 0x1f) || (curr_pri != i))
			{
				src += inc;
				continue;
			}

			pri = (BURN_ENDIAN_SWAP_INT16(m_videoregs[0x02/2]) & 0x0300) >> 8;

			if (!(BURN_ENDIAN_SWAP_INT16(m_videoregs[0x02/2]) & 0x8000))
			{
				if (curr_pri > (BURN_ENDIAN_SWAP_INT16(m_videoregs[0x02/2]) & 0x1f))
					pri = (BURN_ENDIAN_SWAP_INT16(m_videoregs[0x02/2]) & 0x0c00) >> 10;
			}

			y     = BURN_ENDIAN_SWAP_INT16(src[1]);
			attr  = BURN_ENDIAN_SWAP_INT16(src[2]);
			code  = BURN_ENDIAN_SWAP_INT16(src[3]);

			flipx =  attr & 0x8000;
			flipy =  attr & 0x4000;
			color = (attr & 0xf0) >> 4;

			zoom = zoomtable[(y & 0xfc00) >> 10] << (16 - 8);

			x = (x & 0x07ff) - m_sprite_xoffs;
			y = (y & 0x03ff) - m_sprite_yoffs;

			width  = (((attr >> 11) & 0x7) + 1) * 8;
			height = (((attr >>  8) & 0x7) + 1) * 8;

			UINT32 gfxstart = (8 * 8 * 4 / 8) * (((attr & 0x000f) << 16) + code);

			if (flipscreen)
			{
				flipx = !flipx;     x = max_x - x - width;
				flipy = !flipy;     y = max_y - y - height;
			}

			if (color == 0xf)   /* 8bpp */
			{
				if ((gfxstart + width * height - 1) >= gfx_size)
					continue;

				RenderZoomedPrioSprite(pTransDraw, gfx8x8x8 + gfxstart, 0, (color_start >> 4) << 8, 0xff, x, y, flipx, flipy, width, height, zoom, zoom, primask[pri]);
			}
			else
			{
				if ((gfxstart + width / 2 * height - 1) >= gfx_size)
					continue;

				RenderZoomedPrioSprite(pTransDraw, gfx4x8x8 + 2 * gfxstart, 0, (color + color_start) << 4, 0xf, x, y, flipx, flipy, width, height, zoom, zoom, primask[pri]);
			}

			src += inc;
		}
	}
}


static inline UINT8 get_tile_pix(UINT16 code, UINT8 x, UINT8 y, INT32 big, UINT16 *pix)
{
	UINT16 *tiletable = (UINT16*)TileRAM;

	INT32 table_index = (code & 0x1ff0) >> 3;
	UINT32 tile = (BURN_ENDIAN_SWAP_INT16(tiletable[table_index + 0]) << 16) + BURN_ENDIAN_SWAP_INT16(tiletable[table_index + 1]);

	if (code & 0x8000)
	{
		*pix = code & 0x0fff;

		if ((*pix & 0xf) != 0xf)
			return 1;
		else
			return 0;
	}
	else if (((tile & 0x00f00000) == 0x00f00000) && (support_8bpp))
	{
		UINT32 tile2 = big ? ((((tile & 0xfffff) + (8*(code & 0xf))))) : ((((tile & 0xfffff) + (2*(code & 0xf)))));

		tile2 *= (big) ? 0x80 : 0x20; // big is a guess

		if (tile2 >= graphics_length) {
			return 0;
		}

		const UINT8 *data = gfx8x8x8 + tile2;

		switch (code & 0x6000)
		{
			case 0x0000: *pix = data[(y              * (big?16:8)) + x];              break;
			case 0x2000: *pix = data[(((big?15:7)-y) * (big?16:8)) + x];              break;
			case 0x4000: *pix = data[(y              * (big?16:8)) + ((big?15:7)-x)]; break;
			case 0x6000: *pix = data[(((big?15:7)-y) * (big?16:8)) + ((big?15:7)-x)]; break;
		}

		if (*pix == 0xff) {
			return 0;
		}

		*pix |= (tile & 0x0f000000) >> 16;

		return 1;
	}
	else
	{
		UINT32 tile2 = big ? ((((tile & 0xfffff) + 4*(code & 0xf))) * 0x40) : ((((tile & 0xfffff) + (code & 0xf))) * 0x40);

		if (tile2 >= (graphics_length*2)) {
			return 0;
		}

		const UINT8 *data = gfx4x8x8 + tile2;

		switch (code & 0x6000)
		{
			case 0x0000: *pix = data[(y              * (big?16:8)) + x];              break;
			case 0x2000: *pix = data[(((big?15:7)-y) * (big?16:8)) + x];              break;
			case 0x4000: *pix = data[(y              * (big?16:8)) + ((big?15:7)-x)]; break;
			case 0x6000: *pix = data[(((big?15:7)-y) * (big?16:8)) + ((big?15:7)-x)]; break;
		}

		if (*pix == 0xf) {
			return 0;
		}

		*pix |= (tile & 0x0ff00000) >> 16;

		return 1;
	}
}

static void draw_tilemap(UINT32 ,UINT32 pcode,int sx, int sy, int wx, int wy, int big, UINT16 *tilemapram, int layer)
{
	UINT8 * priority_bitmap = pPrioDraw;

	int width  = big ? 4096 : 2048;
	int height = big ? 4096 : 2048;

	int scrwidth  = nScreenWidth;
	int scrheight = nScreenHeight;

	int windowwidth  = width >> 2;
	int windowheight = height >> 3;

	sx += tilemap_scrolldx[layer] * (flipscreen ? 1 : -1);

	for (INT32 y = clip_min_y; y < clip_max_y; y++)
	{
		int scrolly = (sy+y-wy)&(windowheight-1);
		int x;
		UINT16 *dst;
		UINT8 *priority_baseaddr;
		int srcline = (wy+scrolly)&(height-1);
		int srctilerow = srcline >> (big ? 4 : 3);

		if (!flipscreen)
		{
			dst = pTransDraw + (y * nScreenWidth);
			priority_baseaddr = priority_bitmap + (y * nScreenWidth);

			for (x = 0; x < scrwidth; x++)
			{
				int scrollx = (sx+x-wx)&(windowwidth-1);
				int srccol = (wx+scrollx)&(width-1);
				int srctilecol = srccol >> (big ? 4 : 3);
				int tileoffs = srctilecol + srctilerow * 0x100;

				UINT16 dat = 0;

				UINT16 tile = BURN_ENDIAN_SWAP_INT16(tilemapram[tileoffs]);
				UINT8 draw = get_tile_pix(tile, big ? (srccol&0xf) : (srccol&0x7), big ? (srcline&0xf) : (srcline&0x7), big, &dat);

				if (draw)
				{
					dst[x] = dat;
					priority_baseaddr[x] = (priority_baseaddr[x] & (pcode >> 8)) | pcode;
				}
			}
		}
		else // flipped case
		{
			dst = pTransDraw + ((scrheight-y-1) * nScreenWidth);
			priority_baseaddr = priority_bitmap + ((scrheight-y-1) * nScreenWidth);

			for (x = 0; x < scrwidth; x++)
			{
				int scrollx = (sx+x-wx)&(windowwidth-1);
				int srccol = (wx+scrollx)&(width-1);
				int srctilecol = srccol >> (big ? 4 : 3);
				int tileoffs = srctilecol + srctilerow * 0x100;

				UINT16 dat = 0;

				UINT16 tile = BURN_ENDIAN_SWAP_INT16(tilemapram[tileoffs]);
				UINT8 draw = get_tile_pix(tile, big ? (srccol&0xf) : (srccol&0x7), big ? (srcline&0xf) : (srcline&0x7), big, &dat);

				if (draw)
				{
					dst[x] = dat;
					priority_baseaddr[scrwidth-x-1] = (priority_baseaddr[scrwidth-x-1] & (pcode >> 8)) | pcode;
				}
			}
		}
	}
}

static void draw_layers(int pri)
{
	UINT16 *m_videoregs = (UINT16*)VideoRegs;
	UINT16 *m_scroll = (UINT16*)ScrollRegs;
	UINT16 *m_window = (UINT16*)WindowRegs;
	UINT16 layers_pri = BURN_ENDIAN_SWAP_INT16(m_videoregs[0x10 / 2]);
	int layer;

	for (layer = 2; layer >= 0; layer--)
	{
		if (pri == ((layers_pri >> (layer * 2)) & 3))
		{
			UINT16 sy = BURN_ENDIAN_SWAP_INT16(m_scroll[layer * 2 + 0]);
			UINT16 sx = BURN_ENDIAN_SWAP_INT16(m_scroll[layer * 2 + 1]);
			UINT16 wy = BURN_ENDIAN_SWAP_INT16(m_window[layer * 2 + 0]);
			UINT16 wx = BURN_ENDIAN_SWAP_INT16(m_window[layer * 2 + 1]);

			UINT16 *tilemapram = (UINT16*)(VideoRAM[layer]);

			int big = support_16x16 && (screen_control & (0x0020 << layer));

			draw_tilemap(0, 1 << (3 - pri), sx, sy, wx, wy, big, tilemapram, layer);
		}
	}
}

// partial updates & draw-zone

void i4x00_draw_begin()
{
	lastline = 0;

	if (!pBurnDraw) return;

	if (DrvRecalc) {
		palette_update();
		DrvRecalc = 0;
	}

	UINT16 *m_videoregs = (UINT16*)VideoRegs;

	BurnTransferClear((BURN_ENDIAN_SWAP_INT16(m_videoregs[0x12 / 2]) & 0x0fff));
}

void i4x00_draw_scanline(INT32 drawto)
{
	if (drawto > nScreenHeight) drawto = nScreenHeight;
	if (!pBurnDraw || drawto < 1 || drawto == lastline) return;

	clip_min_y = lastline;
	clip_max_y = drawto;
    //bprintf(0, _T("drawscanline ystart = %d  yend = %d\n"), clip_min_y, clip_max_y);
	GenericTilesSetClip(0, nScreenWidth, clip_min_y, clip_max_y); // for sprites
	lastline = drawto;

	if ((screen_control & 2) == 0)
	{
		flipscreen = screen_control & 1;

		if (additional_video_chips_cb) {
			additional_video_chips_cb();
		}

		for (INT32 pri = 3; pri >= 0; pri--)
		{
			if (nBurnLayer & (1<<pri)) draw_layers(pri);
		}

		if (nSpriteEnable & 1) draw_sprites();
	}

	GenericTilesClearClip();
}

void i4x00_draw_end()
{
	if (!pBurnDraw) return;

	i4x00_draw_scanline(nScreenHeight);

	BurnTransferCopy(BurnPalette);
}

INT32 i4x00_draw()
{
	i4x00_draw_begin();
	i4x00_draw_scanline(nScreenHeight);
	i4x00_draw_end();

	return 0;
}

static INT32 usec_to_cycles(INT32 mhz, double usec) {
	return ((double)((double)mhz / 1000000) * usec);
}

static void blitter_write()
{
	{
		UINT16 *m_blitter_regs = (UINT16*)BlitRegs;
		UINT8 *ramdst[4] = { NULL, VideoRAM[0], VideoRAM[1], VideoRAM[2] };

		UINT8 *src     = gfx8x8x8;
		UINT32 src_len = graphics_length;

		UINT32 tmap     = (BURN_ENDIAN_SWAP_INT16(m_blitter_regs[0x00 / 2]) << 16) + BURN_ENDIAN_SWAP_INT16(m_blitter_regs[0x02 / 2]);
		UINT32 src_offs = (BURN_ENDIAN_SWAP_INT16(m_blitter_regs[0x04 / 2]) << 16) + BURN_ENDIAN_SWAP_INT16(m_blitter_regs[0x06 / 2]);
		UINT32 dst_offs = (BURN_ENDIAN_SWAP_INT16(m_blitter_regs[0x08 / 2]) << 16) + BURN_ENDIAN_SWAP_INT16(m_blitter_regs[0x0a / 2]);

		UINT8 *dst = ramdst[tmap];

		if (tmap == 0) {
			bprintf(0, _T("i4x00: dma-blit to non-existant tmap 0!\n"));
			return;
		}

		INT32 offs2 = (~dst_offs >> 7) & 1;
		dst_offs >>=  8;

		while (1)
		{
			UINT16 b1, b2, count;

			src_offs %= src_len;
			b1 = src[src_offs];
			src_offs++;

			count = ((~b1) & 0x3f) + 1;

			switch ((b1 & 0xc0) >> 6)
			{
				case 0:
				{
					if (b1 == 0)
					{
						//i4x00_blitter_timer = 5000; // 500usec -> (10000000 / 1000000) * 500;
						i4x00_blitter_timer = usec_to_cycles(i4x00_cpu_speed, 500);
						return;
					}

					while (count--)
					{
						src_offs %= src_len;
						b2 = src[src_offs];
						src_offs++;

						dst_offs &= 0xffff;
						dst[(dst_offs*2+offs2) & 0x1ffff] = b2;
						dst_offs = ((dst_offs + 1) & (0x100 - 1)) | (dst_offs & (~(0x100 - 1)));
					}
					break;
				}

				case 1:
				{
					src_offs %= src_len;
					b2 = src[src_offs];
					src_offs++;

					while (count--)
					{
						dst_offs &= 0xffff;
						dst[(dst_offs*2+offs2) & 0x1ffff] = b2;
						dst_offs = ((dst_offs + 1) & (0x100 - 1)) | (dst_offs & (~(0x100 - 1)));
						b2++;
					}
					break;
				}

				case 2:
				{
					src_offs %= src_len;
					b2 = src[src_offs];
					src_offs++;

					while (count--)
					{
						dst_offs &= 0xffff;
						dst[(dst_offs*2+offs2) & 0x1ffff] = b2;
						dst_offs = ((dst_offs + 1) & (0x100 - 1)) | (dst_offs & (~(0x100 - 1)));
					}
					break;
				}

				case 3:
				{
					if (b1 == 0xc0)
					{
						dst_offs +=   0x100;
						dst_offs &= ~(0x100 - 1);
						dst_offs |=  (0x100 - 1) & (BURN_ENDIAN_SWAP_INT16(m_blitter_regs[0x0a / 2]) >> (7 + 1));
					}
					else
					{
						dst_offs += count;
					}
					break;
				}
			}
		}
	}
}

static void __fastcall i4x00_write_word(UINT32 address, UINT16 data)
{
	address &= 0x7fffe;
	
	if ((address & 0xfff0000) == 0x60000) { //blazing tornado
		return; // nop
	}
	
	if ((address & 0xfffe000) == 0x72000) {
		*((UINT16*)(BurnPalRAM + (address & 0x3ffe))) = BURN_ENDIAN_SWAP_INT16(data);
		palette_write(address);
		return;
	}

	if ((address & 0xffff000) == 0x75000) {
		UINT16 *dst = (UINT16*)VideoRAM[0];
		dst[((address & 0x7f) + ((address & 0xf80) * 4)) / 2] = BURN_ENDIAN_SWAP_INT16(data);
		return;
	}

	if ((address & 0xffff000) == 0x76000) {
		UINT16 *dst = (UINT16*)VideoRAM[1];
		dst[((address & 0x7f) + ((address & 0xf80) * 4)) / 2] = BURN_ENDIAN_SWAP_INT16(data);
		return;
	}

	if ((address & 0xffff000) == 0x77000) {
		UINT16 *dst = (UINT16*)VideoRAM[2];
		dst[((address & 0x7f) + ((address & 0xf80) * 4)) / 2] = BURN_ENDIAN_SWAP_INT16(data);
		return;
	}

	if (address >= 0x78840 && address <= 0x7884d) {
		*((UINT16*)(BlitRegs + (address & 0xf))) = BURN_ENDIAN_SWAP_INT16(data);
		if (address == 0x7884c) blitter_write();
		return;
	}
	
	if (address >= 0x78850 && address <= 0x78853) {
		*((UINT16*)(SpriteRegs + (address & 0x03))) = BURN_ENDIAN_SWAP_INT16(data);
		return;
	}

	if (address >= 0x78860 && address <= 0x7886b) {
		*((UINT16*)(WindowRegs + (address & 0xf))) = BURN_ENDIAN_SWAP_INT16(data);
		return;
	}

	if (address >= 0x78870 && address <= 0x7887b) {
		*((UINT16*)(ScrollRegs + (address & 0xf))) = BURN_ENDIAN_SWAP_INT16(data);
		i4x00_raster_update = 1;
		return;
	}

	if ((address >= 0x78800 && address <= 0x78813) || (address >= 0x079700 && address <= 0x79713)) {
		if (is_blazing && address == 0x78802) return;
		*((UINT16*)(VideoRegs + (address & 0x1f))) = BURN_ENDIAN_SWAP_INT16(data);
		return;
	}

	switch (address)
	{
		case 0x7887c:
		return;

		case 0x78880:
		case 0x78890:
		case 0x788a0:
		return; // crtc regs

		case 0x788a2:
			if (irq_cause_write_cb) {
				irq_cause_write_cb(data);
			}
		return;

		case 0x788a4:
			i4x00_irq_enable = data;
		return;

		case 0x788a6:
		return; // nop?

		case 0x788a8: // metro...
			if (soundlatch_write_cb) {
				soundlatch_write_cb(data);
			}
		return;

		case 0x788aa:
			rombank = data;
		return;

		case 0x788ac:
			screen_control = data;
		return;
	}
	
	bprintf (0, _T("i4x00 unmapped word write (%5.5x, %4.4x)\n"), address, data);
}

static void __fastcall i4x00_write_byte(UINT32 address, UINT8 data)
{
	address &= 0x7ffff;
	
	if ((address & 0xfffe000) == 0x072000) {
		BurnPalRAM[(address & 0x3fff) ^ 1] = data;
		palette_write(address);
		return;
	}

	if (address >= 0x78840 && address <= 0x7884d) {
		BlitRegs[(address & 0xf) ^ 1] = data;
		if (address == 0x7884c) blitter_write();
		return;
	}

	switch (address)
	{
		case 0x788a3:
		if (irq_cause_write_cb) {
			irq_cause_write_cb(data);
		}
		return;

		case 0x788a5:
			i4x00_irq_enable = data;
		return;
	}
	
	bprintf (0, _T("i4x00 unmapped byte write (%5.5x, %2.2x)\n"), address, data);
}

static UINT16 __fastcall i4x00_read_word(UINT32 address)
{
	address &= 0x7fffe;

	if ((address & 0xfff0000) == 0x60000) {
		INT32 offset = (rombank * 0x10000) + (address & 0xfffe);
		if (offset >= graphics_length) return 0xffff;
		return gfx8x8x8[offset + 0] * 256 + gfx8x8x8[offset + 1];
	}

	if ((address & 0xffff000) == 0x75000) {
		UINT16 *dst = (UINT16*)VideoRAM[0];
		return dst[((address & 0x7f) + ((address & 0xf80) * 4)) / 2];
	}

	if ((address & 0xffff000) == 0x76000) {
		UINT16 *dst = (UINT16*)VideoRAM[1];
		return dst[((address & 0x7f) + ((address & 0xf80) * 4)) / 2];
	}

	if ((address & 0xffff000) == 0x77000) {
		UINT16 *dst = (UINT16*)VideoRAM[2];
		return dst[((address & 0x7f) + ((address & 0xf80) * 4)) / 2];
	}

	if ((address >= 0x078800 && address <= 0x078813) || (address >= 0x079700 && address <= 0x079713)) {
		return *((UINT16*)(VideoRegs + (address & 0x1f)));
	}

	switch (address)
	{
		case 0x788a2:
			if (!irq_cause_read_cb) return 0;
			return irq_cause_read_cb();
	}

	bprintf (0, _T("ix400 unmapped word read (%5.5x)\n"), address);

	return 0;
}

static UINT8 __fastcall i4x00_read_byte(UINT32 address)
{
	address &= 0x7ffff;

	if ((address & 0xfff0000) == 0x60000) {
		INT32 offset = (rombank * 0x10000) + (address & 0xffff);
		if (offset >= graphics_length) return 0xff;
		return gfx8x8x8[offset];
	}

	switch (address)
	{
		case 0x788a3:
			if (!irq_cause_read_cb) return 0;
			return irq_cause_read_cb();
	}
	
	bprintf (0, _T("i4x00 unmapped byte read (%5.5x)!\n"), address); 

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllRam;

	VideoRAM[0]		= Next; Next += 0x020000;
	VideoRAM[1]		= Next; Next += 0x020000;
	VideoRAM[2]		= Next; Next += 0x020000;
	BurnPalRAM		= Next; Next += 0x004000;
	SpriteRAM		= Next; Next += 0x004000;
	TileRAM			= Next; Next += 0x000800;

	BlitRegs		= Next; Next += 0x000010;
	WindowRegs		= Next; Next += 0x000010;
	ScrollRegs		= Next; Next += 0x000010;
	VideoRegs		= Next; Next += 0x000020;
	SpriteRegs		= Next; Next += 0x000004;

	RamEnd			= Next;

	return 0;
}

void i4x00_reset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	flipscreen = 0;
	rombank = 0;
	screen_control = 0;
	i4x00_irq_enable = 0xff;
	i4x00_blitter_timer = -1;

	i4x00_raster_update = 0;
}

void i4x00_set_offsets(INT32 layer0, INT32 layer1, INT32 layer2)
{
	tilemap_scrolldx[0] = layer0;
	tilemap_scrolldx[1] = layer1;
	tilemap_scrolldx[2] = layer2;
}

void i4x00_set_blazing() // blzntrnd
{
	is_blazing = 1;
}

void i4x00_set_extrachip_callback(void (*callback)())
{
	additional_video_chips_cb = callback;
}

void i4x00_init(INT32 cpu_speed, UINT32 address, UINT8 *gfx8, UINT8 *gfx4, UINT32 gfxlen, void (*irqcausewrite)(UINT16), UINT16 (*irqcauseread)(), void (*soundlatch)(UINT16), INT32 has_8bpp, INT32 has_16bpp)
{
	AllRam = NULL;
	MemIndex();
	INT32 nLen = RamEnd - (UINT8 *)0;
	if ((AllRam = (UINT8 *)BurnMalloc(nLen)) == NULL) return;
	memset(AllRam, 0, nLen);
	MemIndex();
	
	BurnPalette = (UINT32*)BurnMalloc(0x1000 * sizeof(UINT32));

	i4x00_cpu_speed = cpu_speed;

	// catch anything not mapped to ram/rom
	SekMapHandler(5, 						0x00000 + address, 0x7ffff + address, MAP_READ | MAP_WRITE);
	SekSetWriteWordHandler(5,				i4x00_write_word);
	SekSetWriteByteHandler(5,				i4x00_write_byte);
	SekSetReadWordHandler(5,				i4x00_read_word);
	SekSetReadByteHandler(5,				i4x00_read_byte);

	SekMapMemory(VideoRAM[0],			0x00000 + address, 0x1ffff + address, MAP_RAM);
	SekMapMemory(VideoRAM[1],			0x20000 + address, 0x3ffff + address, MAP_RAM);
	SekMapMemory(VideoRAM[2],			0x40000 + address, 0x5ffff + address, MAP_RAM);
	SekMapMemory(BurnPalRAM,			0x70000 + address, 0x71fff + address, MAP_RAM);
	SekMapMemory(BurnPalRAM + 0x2000,	0x72000 + address, 0x73fff + address, MAP_ROM); // write through handler
	SekMapMemory(SpriteRAM,				0x74000 + address, 0x74fff + address, MAP_RAM); // 0-fff
	SekMapMemory(TileRAM,				0x78000 + address, 0x787ff + address, MAP_RAM);

	irq_cause_read_cb = irqcauseread;
	irq_cause_write_cb = irqcausewrite;
	soundlatch_write_cb = soundlatch;

	support_16x16 = has_16bpp;
	support_8bpp = has_8bpp;
	
	gfx8x8x8 = gfx8 ? gfx8 : gfx4;
	gfx4x8x8 = gfx4;
	graphics_length = gfxlen;
}

void i4x00_exit()
{
	BurnFree(AllRam);
	BurnFree(BurnPalette);

	irq_cause_read_cb = NULL;
	irq_cause_write_cb = NULL;
	soundlatch_write_cb = NULL;
	additional_video_chips_cb = NULL;

	is_blazing = 0;

	i4x00_set_offsets(0,0,0);
}

void i4x00_scan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029698;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(rombank);
		SCAN_VAR(i4x00_irq_enable);
		SCAN_VAR(screen_control);
		SCAN_VAR(i4x00_blitter_timer);
	}
}

