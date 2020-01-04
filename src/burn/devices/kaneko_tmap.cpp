// Based on MAME sources by Luca Elia, David Haywood

#include "tiles_generic.h"
#include "kaneko_tmap.h"

static INT32 kaneko_view2_xoff[MAX_VIEW2_CHIPS] = { 0, 0 };
static INT32 kaneko_view2_yoff[MAX_VIEW2_CHIPS] = { 0, 0 };
static UINT8 *kaneko_view2_vram[MAX_VIEW2_CHIPS] = { NULL, NULL };
static UINT8 *kaneko_view2_regs[MAX_VIEW2_CHIPS] = { NULL, NULL };
static UINT8 *kaneko_view2_gfx[MAX_VIEW2_CHIPS] = { NULL, NULL };
static UINT8 *kaneko_view2_gfx_trans[MAX_VIEW2_CHIPS] = { NULL, NULL };
static INT32 kaneko_color_offset[MAX_VIEW2_CHIPS] = { 0, 0 };

void kaneko_view2_init(INT32 chip, UINT8 *video_ram, UINT8 *reg_ram, UINT8 *gfx_rom, INT32 color_offset, UINT8 *gfx_trans, INT32 global_x, INT32 global_y)
{
	kaneko_view2_vram[chip] = video_ram;
	kaneko_view2_regs[chip] = reg_ram;
	kaneko_view2_gfx[chip] = gfx_rom;
	kaneko_color_offset[chip] = color_offset;
	kaneko_view2_gfx_trans[chip] = gfx_trans;

	kaneko_view2_xoff[chip] = global_x;
	kaneko_view2_yoff[chip] = global_y;	
}

void kaneko_view2_exit()
{
	for (INT32 i = 0; i < MAX_VIEW2_CHIPS; i++) {
		kaneko_view2_vram[i] = NULL;
		kaneko_view2_regs[i] = NULL;
		kaneko_view2_gfx[i] = NULL;
		kaneko_color_offset[i] = 0;
		kaneko_view2_gfx_trans[i] = NULL;

		kaneko_view2_xoff[i] = 0;
		kaneko_view2_yoff[i] = 0;	
	}
}

void kaneko_view2_draw_layer(INT32 chip, INT32 layer, INT32 priority)
{
	UINT16 *vram = (UINT16*)(kaneko_view2_vram[chip] + (layer ? 0x0000 : 0x1000));
	UINT16 *sram = (UINT16*)(kaneko_view2_vram[chip] + (layer ? 0x2000 : 0x3000));
	UINT16 *regs = (UINT16*)kaneko_view2_regs[chip];

	INT32 tmflip = BURN_ENDIAN_SWAP_INT16(regs[4]);

	INT32 enable = ~tmflip & (layer ? 0x0010 : 0x1000);
	if (enable == 0) return; // disable!

	INT32 tmflipx = tmflip & 0x0200; // flip whole tilemap x
	INT32 tmflipy = tmflip & 0x0100; // flip whole tilemap y

	INT32 lsenable = tmflip & (layer ? 0x0008 : 0x0800); // linescroll

	INT32 xscroll = BURN_ENDIAN_SWAP_INT16(regs[2 - (layer * 2)]);
	INT32 yscroll = BURN_ENDIAN_SWAP_INT16(regs[3 - (layer * 2)]) >> 6;

	xscroll += (tmflipx) ? -((344 + (layer * 2)) * 64) : ((kaneko_view2_xoff[chip] + (layer * 2)) * 64);
	yscroll += ((tmflipy) ? -260 : 11) + kaneko_view2_yoff[chip];
	yscroll &= 0x1ff;

	if (lsenable)
	{
		UINT16 *dest = pTransDraw;

		for (INT32 y = 0; y < nScreenHeight; y++, dest += nScreenWidth) // line by line
		{
			INT32 scrollyy = (yscroll + y) & 0x1ff;
			INT32 scrollxx = ((xscroll + BURN_ENDIAN_SWAP_INT16(sram[y])) >> 6) & 0x1ff;
	
			INT32 srcy = (scrollyy & 0x1ff) >> 4;
			INT32 srcx = (scrollxx & 0x1ff) >> 4;

			for (INT32 x = 0; x < nScreenWidth + 16; x+=16)
			{
				INT32 offs = ((srcy << 5) | ((srcx + (x >> 4)) & 0x1f));

				INT32 attr  = BURN_ENDIAN_SWAP_INT16(vram[offs * 2 + 0]);
				INT32 code  = BURN_ENDIAN_SWAP_INT16(vram[offs * 2 + 1]) & 0x1fff;
				INT32 color = ((attr & 0x00fc) << 2) + kaneko_color_offset[chip];
				INT32 flipx = (attr & 0x0002) ? 0x0f : 0;
				INT32 flipy = (attr & 0x0001) ? 0xf0 : 0;
				INT32 group = (attr & 0x0700) >> 8;

				if (kaneko_view2_gfx_trans[chip]) {
					if (kaneko_view2_gfx_trans[chip][code]) continue;
				}

				if (group != priority) continue;
	
				UINT8 *gfxsrc = kaneko_view2_gfx[chip] + (code << 8) + (((scrollyy & 0x0f) << 4) ^ flipy);
	
				for (INT32 dx = 0; dx < 16; dx++)
				{
					INT32 dst = (x + dx) - (scrollxx & 0x0f);
					if (dst < 0 || dst >= nScreenWidth) continue;
	
					if (gfxsrc[dx^flipx]) {
						dest[dst] = color + gfxsrc[dx^flipx];
					}
				}
			}
		}
	}
	else
	{
		INT32 scrollx = (xscroll >> 6) & 0x1ff;

		for (INT32 offs = 0; offs < 32 * 32; offs++)
		{
			INT32 sx = (offs & 0x1f) * 16;
			INT32 sy = (offs / 0x20) * 16;
	
			sy -= yscroll;
			if (sy < -15) sy += 512;
			sx -= scrollx;
			if (sx < -15) sx += 512;

			if (sx >= nScreenWidth || sy >= nScreenHeight) continue;

			INT32 attr  = BURN_ENDIAN_SWAP_INT16(vram[offs * 2 + 0]);
			INT32 code  = BURN_ENDIAN_SWAP_INT16(vram[offs * 2 + 1]) & 0x1fff;
			INT32 color = ((attr & 0x00fc) >> 2) + (0x400 >> 4);
			INT32 flipx = (attr & 0x0002);
			INT32 flipy = (attr & 0x0001);
			INT32 group = (attr & 0x0700) >> 8;

			if (kaneko_view2_gfx_trans[chip]) {
				if (kaneko_view2_gfx_trans[chip][code]) continue;
			}

			if (tmflipy) {
				flipy ^= 1;
				sy = 224 - sy; // fix later
			}
	
			if (tmflipx) {
				flipx ^= 2;
				sx = 304 - sx; // fix later!
			}
	
			if (group != priority) continue;

			if (sx >= 0 && sy >=0 && sx <= (nScreenWidth - 16) && sy <= (nScreenHeight-16)) // non-clipped
			{
				if (flipy) {
					if (flipx) {
						Render16x16Tile_Mask_FlipXY(pTransDraw, code, sx, sy, color, 4, 0, 0, kaneko_view2_gfx[chip]);
					} else {
						Render16x16Tile_Mask_FlipY(pTransDraw, code, sx, sy, color, 4, 0, 0, kaneko_view2_gfx[chip]);
					}
				} else {
					if (flipx) {
						Render16x16Tile_Mask_FlipX(pTransDraw, code, sx, sy, color, 4, 0, 0, kaneko_view2_gfx[chip]);
					} else {
						Render16x16Tile_Mask(pTransDraw, code, sx, sy, color, 4, 0, 0, kaneko_view2_gfx[chip]);
					}
				}
			}
			else	// clipped
			{
				if (flipy) {
					if (flipx) {
						Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0, kaneko_view2_gfx[chip]);
					} else {
						Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0, kaneko_view2_gfx[chip]);
					}
				} else {
					if (flipx) {
						Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0, kaneko_view2_gfx[chip]);
					} else {
						Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0, kaneko_view2_gfx[chip]);
					}
				}
			}
		}
	}
}

