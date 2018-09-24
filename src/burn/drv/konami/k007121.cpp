#include "tiles_generic.h"

#define K007121_CHIPS 2

static UINT8 k007121_ctrlram[K007121_CHIPS][8];
static INT32 k007121_flipscreen[K007121_CHIPS];
static INT32 k007121_sprite_mask[K007121_CHIPS];

UINT8 k007121_ctrl_read(INT32 chip, UINT8 offset)
{
	return k007121_ctrlram[chip][offset & 7];
}

void k007121_ctrl_write(INT32 chip, UINT8 offset, UINT8 data)
{
	offset &= 7;

	if (offset == 7) k007121_flipscreen[chip] = data & 0x08;

	k007121_ctrlram[chip][offset] = data;
}

void k007121_draw(INT32 chip, UINT16 *dest, UINT8 *gfx, UINT8 *ctable, UINT8 *source, INT32 base_color, INT32 xoffs, INT32 yoffs, INT32 bank_base, INT32 pri_mask, INT32 color_offset)
{
	INT32 flipscreen = k007121_flipscreen[chip];
	INT32 num, inc;

	/* TODO: sprite limit is supposed to be per-line! (check MT #00185) */
	num = 0x40;

	inc = 5;
	/* when using priority buffer, draw front to back */
	if (pri_mask != -1)
	{
		source += (num - 1)*inc;
		inc = -inc;
	}

	for (INT32 i = 0; i < num; i++)
	{
		INT32 number = source[0];               /* sprite number */
		INT32 sprite_bank = source[1] & 0x0f;   /* sprite bank */
		INT32 sx = source[3];                   /* vertical position */
		INT32 sy = source[2];                   /* horizontal position */
		INT32 attr = source[4];             /* attributes */
		INT32 xflip = source[4] & 0x10;     /* flip x */
		INT32 yflip = source[4] & 0x20;     /* flip y */
		INT32 color = base_color + ((source[1] & 0xf0) >> 4);
		INT32 width, height;

		static const INT32 x_offset[4] = {0x0,0x1,0x4,0x5};
		static const INT32 y_offset[4] = {0x0,0x2,0x8,0xa};
		INT32 x,y, ex, ey, flipx, flipy, destx, desty;

		if (attr & 0x01) sx -= 256;
		if (sy >= 240) sy -= 256;

		sy -= yoffs;

		number += ((sprite_bank & 0x3) << 8) + ((attr & 0xc0) << 4);
		number = number << 2;
		number += (sprite_bank >> 2) & 3;

		number += bank_base;

		switch (attr & 0xe)
		{
			case 0x06: width = height = 1; break;
			case 0x04: width = 1; height = 2; number &= (~2); break;
			case 0x02: width = 2; height = 1; number &= (~1); break;
			case 0x00: width = height = 2; number &= (~3); break;
			case 0x08: width = height = 4; number &= (~3); break;
		    default: width = 1; height = 1;
		}

		for (y = 0; y < height; y++)
		{
			for (x = 0; x < width; x++)
			{
				ex = xflip ? (width - 1 - x) : x;
				ey = yflip ? (height - 1 - y) : y;

				if (flipscreen)
				{
					flipx = !xflip;
					flipy = !yflip;
					destx = 248 - (sx + x * 8);
					desty = 248 - (sy + y * 8);
				}
				else
				{
					flipx = xflip;
					flipy = yflip;
					destx = xoffs + sx + x * 8;
					desty = sy + y * 8;
				}

				INT32 code = (number + x_offset[ex] + y_offset[ey]) & k007121_sprite_mask[chip];

				if (pri_mask != -1)
				{
					if (ctable != NULL) {
						RenderPrioMaskTranstabSpriteOffset(dest, gfx, code, (color * 16), 0, destx, desty, flipx, flipy, 8, 8, ctable, color_offset, pri_mask);
					} else {
						if (flipy) {
							if (flipx) {
								Render8x8Tile_Prio_Mask_FlipXY_Clip(dest, code, destx, desty, color, 4, 0, color_offset, pri_mask, gfx);
							} else {
								Render8x8Tile_Prio_Mask_FlipY_Clip(dest, code, destx, desty, color, 4, 0, color_offset, pri_mask, gfx);
							}
						} else {
							if (flipx) {
								Render8x8Tile_Prio_Mask_FlipX_Clip(dest, code, destx, desty, color, 4, 0, color_offset, pri_mask, gfx);
							} else {
								Render8x8Tile_Prio_Mask_Clip(dest, code, destx, desty, color, 4, 0, color_offset, pri_mask, gfx);
							}
						}
					}
				}
				else
				{
					if (ctable != NULL) {
						RenderTileTranstabOffset(dest, gfx, code, (color * 16), 0, destx, desty, flipx, flipy, 8, 8, ctable, color_offset);
					} else {
						if (flipy) {
							if (flipx) {
								Render8x8Tile_Mask_FlipXY_Clip(dest, code, destx, desty, color, 4, 0, color_offset, gfx);
							} else {
								Render8x8Tile_Mask_FlipY_Clip(dest, code, destx, desty, color, 4, 0, color_offset, gfx);
							}
						} else {
							if (flipx) {
								Render8x8Tile_Mask_FlipX_Clip(dest, code, destx, desty, color, 4, 0, color_offset,gfx);
							} else {
								Render8x8Tile_Mask_Clip(dest, code, destx, desty, color, 4, 0, color_offset, gfx);
							}
						}
					}
				}
			}
		}

		source += inc;
	}
}

void k007121_reset()
{
	for (INT32 i = 0; i < K007121_CHIPS; i++)
	{
		k007121_flipscreen[i] = 0;

		memset (k007121_ctrlram[i], 0, 8);
	}
}

INT32 k007121_scan(INT32 nAction)
{
	if (nAction & ACB_VOLATILE)
	{
		for (INT32 i = 0; i < K007121_CHIPS; i++)
		{
			SCAN_VAR(k007121_ctrlram[i]);
			SCAN_VAR(k007121_flipscreen[i]);
		}
	}

	return 0;
}

void k007121_init(INT32 chip, INT32 sprite_mask)
{
	k007121_sprite_mask[chip] = sprite_mask;
}
