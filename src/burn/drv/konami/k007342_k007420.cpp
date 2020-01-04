// license:BSD-3-Clause
// copyright-holders:Fabio Priuli,Acho A. Tang, R. Belmont

#include "tiles_generic.h"

#define K007342_OPAQUE	(1<<16)

UINT8 *K007342VidRAM[1] = { NULL };
UINT8 *K007342ScrRAM[1] = { NULL };
UINT8  K007342Regs[1][8];

static INT32 GlobalXOffsets = 0;
static INT32 GlobalYOffsets = 0;
static UINT8 *GfxBase;
static UINT8 *ColorRAM[2];
static UINT8 *VideoRAM[2];

static void (*pCallback)(INT32 layer, INT32 bank, INT32 *code, INT32 *color, INT32 *flags) = NULL;

void K007342Init(UINT8 *gfx, void (*Callback)(INT32 layer, INT32 bank, INT32 *code, INT32 *color, INT32 *flags))
{
	GfxBase = gfx;
	pCallback = Callback;

	for (INT32 i = 0; i < 2; i++) {
		ColorRAM[i] = K007342VidRAM[0] + i * 0x1000;
		VideoRAM[i] = ColorRAM[i] + 0x0800;
	}

	GlobalXOffsets = 0;
	GlobalYOffsets = 0;
}

void K007342SetOffsets(INT32 x, INT32 y)
{
	GlobalXOffsets = x;
	GlobalYOffsets = y;
}

void K007342Reset()
{
	memset (K007342Regs[0], 0, 8);
}

INT32 K007342_irq_enabled()
{
	return (K007342Regs[0][0] & 0x02);
}

void K007342DrawLayer(INT32 layer, INT32 baseflags, INT32 /*priority*/)
{
//	INT32 flipscreen = K007342Regs[0][0] & 0x10;

	INT32 scrollx0 = ((K007342Regs[0][2] & 0x01) * 256) + K007342Regs[0][3];
	INT32 scrollx1 = ((K007342Regs[0][2] & 0x02) * 128) + K007342Regs[0][5];
	INT32 scrolly0 = K007342Regs[0][4];
	INT32 scrolly1 = K007342Regs[0][6];

	INT32 scrollx = ((layer) ? scrollx1 : scrollx0);
	INT32 scrolly = ((layer) ? scrolly1 : scrolly0);

	scrollx = (scrollx + GlobalXOffsets) & 0x1ff;
	scrolly = (scrolly + GlobalYOffsets) & 0xff;

	INT32 scroll_ctrl = K007342Regs[0][2] & 0x1c;
	INT32 opaque = baseflags & K007342_OPAQUE;
	INT32 category_select = baseflags & 1;

	if (scroll_ctrl == 0x00 || scroll_ctrl == 0x08 || layer == 1)
	{
		for (INT32 offs = 0; offs < 64 * 32; offs++)
		{
			INT32 sx = (offs & 0x3f) * 8;
			INT32 sy = (offs / 0x40) * 8;

			INT32 ofst = (offs & 0x1f) + (((offs / 0x40) + (offs & 0x20)) * 0x20);

			sx -= scrollx;
			if (sx < -7) sx += 512;
			sy -= scrolly;
			if (sy < -7) sy += 256;
			if (sx >= nScreenWidth || sy >= nScreenHeight) continue;

			INT32 code  = VideoRAM[layer][ofst];
			INT32 color = ColorRAM[layer][ofst];

			INT32 flags = (color >> 4) & 3;

			INT32 category = (color >> 7) & 1;
			if (category_select != category) continue; // right?

			if (pCallback) {
				pCallback(layer, K007342Regs[0][1], &code, &color, &flags);
			}

			INT32 flipx = flags & 1;
			INT32 flipy = flags & 2;

			if (opaque) {
				if (flipy) {
					if (flipx) {
						Render8x8Tile_FlipXY_Clip(pTransDraw, code, sx, sy, color, 4, 0, GfxBase);
					} else {
						Render8x8Tile_FlipY_Clip(pTransDraw, code, sx, sy, color, 4, 0, GfxBase);
					}		
				} else {
					if (flipx) {
						Render8x8Tile_FlipX_Clip(pTransDraw, code, sx, sy, color, 4, 0, GfxBase);
					} else {
						Render8x8Tile_Clip(pTransDraw, code, sx, sy, color, 4, 0, GfxBase);
					}
				}
			} else {
				if (flipy) {
					if (flipx) {
						Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0, GfxBase);
					} else {
						Render8x8Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0, GfxBase);
					}		
				} else {
					if (flipx) {
						Render8x8Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0, GfxBase);
					} else {
						Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0, GfxBase);
					}
				}
			}
		}
	}
	else if (scroll_ctrl == 0x0c) // 32 columns
	{
		for (INT32 y = 0; y < nScreenHeight + 8; y+=8)
		{
			for (INT32 x = 0; x < nScreenWidth + 8; x+=8)
			{
				INT32 sxx = (scrollx + x) & 0x1ff;

				INT32 yscroll = (K007342ScrRAM[0][((sxx/8)&0x1f) * 2] + (K007342ScrRAM[0][((sxx/8)&0x1f) * 2 + 1] * 256) + GlobalYOffsets) & 0xff;

				INT32 syy = (yscroll + y) & 0xff;

				INT32 ofst = ((sxx / 8) & 0x1f) + (((syy / 8) + ((sxx / 8) & 0x20)) * 0x20);

				INT32 code  = VideoRAM[layer][ofst];
				INT32 color = ColorRAM[layer][ofst];
	
				INT32 flags = (color >> 4) & 3;
				INT32 category = color >> 7;
	
				if (category_select != category) continue; // right?
	
				if (pCallback) {
					pCallback(layer, K007342Regs[0][1], &code, &color, &flags);
				}
	
				INT32 flipx = (flags & 1) * 0x07;
				INT32 flipy = (flags & 2) * 0x38;

				if (opaque) {
					if (flipy) {
						if (flipx) {
							Render8x8Tile_FlipXY_Clip(pTransDraw, code, x - (scrollx & 7), y - (yscroll & 7), color, 4, 0, GfxBase);
						} else {
							Render8x8Tile_FlipY_Clip(pTransDraw, code, x - (scrollx & 7), y - (yscroll & 7), color, 4, 0, GfxBase);
						}		
					} else {
						if (flipx) {
							Render8x8Tile_FlipX_Clip(pTransDraw, code, x - (scrollx & 7), y - (yscroll & 7), color, 4, 0, GfxBase);
						} else {
							Render8x8Tile_Clip(pTransDraw, code, x - (scrollx & 7), y - (yscroll & 7), color, 4, 0, GfxBase);
						}
					}
				} else {
					if (flipy) {
						if (flipx) {
							Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, code, x - (scrollx & 7), y - (yscroll & 7), color, 4, 0, 0, GfxBase);
						} else {
							Render8x8Tile_Mask_FlipY_Clip(pTransDraw, code, x - (scrollx & 7), y - (yscroll & 7), color, 4, 0, 0, GfxBase);
						}		
					} else {
						if (flipx) {
							Render8x8Tile_Mask_FlipX_Clip(pTransDraw, code, x - (scrollx & 7), y - (yscroll & 7), color, 4, 0, 0, GfxBase);
						} else {
							Render8x8Tile_Mask_Clip(pTransDraw, code, x - (scrollx & 7), y - (yscroll & 7), color, 4, 0, 0, GfxBase);
						}
					}
				}
			}
		}
	}
	else if (scroll_ctrl == 0x14) // 256 rows
	{
		for (INT32 y = 0; y < nScreenHeight; y++)
		{
			INT32 syy = (y + scrolly) & 0xff;
			INT32 xscroll = (K007342ScrRAM[0][syy * 2] + (K007342ScrRAM[0][syy * 2 + 1] * 256) + GlobalXOffsets) & 0x1ff;

			UINT16 *dest = pTransDraw + y * nScreenWidth;

			for (INT32 x = 0; x < nScreenWidth + 8; x += 8)
			{
				INT32 sxx = (xscroll + x) & 0x1ff;
				INT32 ofst = ((sxx / 8) & 0x1f) + (((syy / 8) + ((sxx / 8) & 0x20)) * 0x20);

//				return (col & 0x1f) + ((row & 0x1f) << 5) + ((col & 0x20) << 5);

				INT32 code  = VideoRAM[layer][ofst];
				INT32 color = ColorRAM[layer][ofst];
	
				INT32 flags = (color >> 4) & 3;
				INT32 category = color >> 7;
	
				if (category_select != category) continue; // right?
	
				if (pCallback) {
					pCallback(layer, K007342Regs[0][1], &code, &color, &flags);
				}
	
				INT32 flipx = (flags & 1) * 0x07;
				INT32 flipy = (flags & 2) * 0x38;

				{
					color *= 16;
					INT32 sxxx = x - (xscroll & 7);
					UINT8 *base = GfxBase + (code * 0x40) + (((y & 7) * 8) ^ flipy);

					for (INT32 xx = 0; xx < 8; xx++, sxxx++)
					{
						if (sxxx < 0) continue;
						if (sxxx >= nScreenWidth) break;

						INT32 pxl = base[xx ^ flipx];

						if (pxl || opaque) {
							dest[sxxx] = pxl + color;
						}
					}
				}
			}
		}
	}
}

INT32 K007342Scan(INT32 nAction)
{
	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(K007342Regs);
	}

	return 0;
}

static INT32 K007420GlobalXOffset = 0;
static INT32 K007420GlobalYOffset = 0;

UINT8 *K007420RAM[1] = { NULL };
static INT32 K007420_banklimit = ~0;
static void (*pSprCallback)(INT32 *code, INT32 *color) = NULL;

void K007420Init(INT32 banklimit, void (*Callback)(INT32 *code, INT32 *color))
{
	pSprCallback = Callback;
	K007420_banklimit = banklimit;

	K007420GlobalXOffset = K007420GlobalYOffset = 0;
}

void K007420SetOffsets(INT32 x, INT32 y)
{
	K007420GlobalXOffset = x;
	K007420GlobalYOffset = y;
}

void K007420DrawSprites(UINT8 *gfxbase)
{
	INT32 codemask = K007420_banklimit;
	INT32 bankmask = ~K007420_banklimit;

	UINT8 *m_ram = K007420RAM[0];

	for (INT32 offs = 0x200 - 8; offs >= 0; offs -= 8)
	{
		INT32 ox, oy, code, color, flipx, flipy, zoom, w, h, x, y, bank;
		static const INT32 xoffset[4] = { 0, 1, 4, 5 };
		static const INT32 yoffset[4] = { 0, 2, 8, 10 };

		code = m_ram[offs + 1];
		color = m_ram[offs + 2];
		ox = m_ram[offs + 3] - ((m_ram[offs + 4] & 0x80) << 1);
		oy = 256 - m_ram[offs + 0];
		flipx = m_ram[offs + 4] & 0x04;
		flipy = m_ram[offs + 4] & 0x08;

		pSprCallback(&code, &color);

		bank = code & bankmask;
		code &= codemask;

		zoom = m_ram[offs + 5] | ((m_ram[offs + 4] & 0x03) << 8);
		if (!zoom)
			continue;
		zoom = 0x10000 * 128 / zoom;

		switch (m_ram[offs + 4] & 0x70)
		{
			case 0x30: w = h = 1; break;
			case 0x20: w = 2; h = 1; code &= (~1); break;
			case 0x10: w = 1; h = 2; code &= (~2); break;
			case 0x00: w = h = 2; code &= (~3); break;
			case 0x40: w = h = 4; code &= (~3); break;
			default: w = 1; h = 1;
		}

		if (K007342Regs[0][2] & 0x10)
		{
			ox = 256 - ox - ((zoom * w + (1 << 12)) >> 13);
			oy = 256 - oy - ((zoom * h + (1 << 12)) >> 13);
			flipx = !flipx;
			flipy = !flipy;
		}

		if (zoom == 0x10000)
		{
			INT32 sx, sy;

			for (y = 0; y < h; y++)
			{
				sy = oy + 8 * y;

				for (x = 0; x < w; x++)
				{
					INT32 c = code;

					sx = ox + 8 * x;
					if (flipx)
						c += xoffset[(w - 1 - x)];
					else
						c += xoffset[x];

					if (flipy)
						c += yoffset[(h - 1 - y)];
					else
						c += yoffset[y];

					if (c & bankmask)
						continue;
					else
						c += bank;

					if (flipy) {
						if (flipx) {
							Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, c, sx - K007420GlobalXOffset, sy - K007420GlobalYOffset, color, 4, 0, 0, gfxbase);
						} else {
							Render8x8Tile_Mask_FlipY_Clip(pTransDraw, c, sx - K007420GlobalXOffset, sy - K007420GlobalYOffset, color, 4, 0, 0, gfxbase);
						}		
					} else {
						if (flipx) {
							Render8x8Tile_Mask_FlipX_Clip(pTransDraw, c, sx - K007420GlobalXOffset, sy - K007420GlobalYOffset, color, 4, 0, 0, gfxbase);
						} else {
							Render8x8Tile_Mask_Clip(pTransDraw, c, sx - K007420GlobalXOffset, sy - K007420GlobalYOffset, color, 4, 0, 0, gfxbase);
						}
					}

					if (K007342Regs[0][2] & 0x80) { // wrap
						if (flipy) {
							if (flipx) {
								Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, c, sx - K007420GlobalXOffset, (sy-K007420GlobalYOffset)-256, color, 4, 0, 0, gfxbase);
							} else {
								Render8x8Tile_Mask_FlipY_Clip(pTransDraw, c, sx - K007420GlobalXOffset, (sy-K007420GlobalYOffset)-256, color, 4, 0, 0, gfxbase);
							}		
						} else {
							if (flipx) {
								Render8x8Tile_Mask_FlipX_Clip(pTransDraw, c, sx - K007420GlobalXOffset, (sy-K007420GlobalYOffset)-256, color, 4, 0, 0, gfxbase);
							} else {
								Render8x8Tile_Mask_Clip(pTransDraw, c, sx - K007420GlobalXOffset, (sy-K007420GlobalYOffset)-256, color, 4, 0, 0, gfxbase);
							}
						}
					}
				}
			}
		}
		else
		{
			INT32 sx, sy, zw, zh;
			for (y = 0; y < h; y++)
			{
				sy = oy + ((zoom * y + (1 << 12)) >> 13);
				zh = (oy + ((zoom * (y + 1) + (1 << 12)) >> 13)) - sy;

				for (x = 0; x < w; x++)
				{
					INT32 c = code;

					sx = ox + ((zoom * x + (1<<12)) >> 13);
					zw = (ox + ((zoom * (x + 1) + (1 << 12)) >> 13)) - sx;
					if (flipx)
						c += xoffset[(w - 1 - x)];
					else
						c += xoffset[x];

					if (flipy)
						c += yoffset[(h - 1 - y)];
					else
						c += yoffset[y];

					if (c & bankmask)
						continue;
					else
						c += bank;

					RenderZoomedTile(pTransDraw, gfxbase, c, color*16, 0, sx - K007420GlobalXOffset, (sy-K007420GlobalYOffset), flipx, flipy, 8, 8, (zw << 16) / 8,(zh << 16) / 8);

					if (K007342Regs[0][2] & 0x80) { // wrap
						RenderZoomedTile(pTransDraw, gfxbase, c, color*16, 0, sx - K007420GlobalXOffset, (sy-K007420GlobalYOffset)-256, flipx, flipy, 8, 8, (zw << 16) / 8,(zh << 16) / 8);
					}
				}
			}
		}
	}
}
