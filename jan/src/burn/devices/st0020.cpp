// Based on MAME sources by Luca Elia,David Haywood

#include "tiles_generic.h"
#include "st0020.h"

// directly map sprite ram

UINT8 *st0020BlitRAM;
UINT8 *st0020SprRAM;
UINT8 *st0020GfxRAM;
UINT8 *st0020GfxROM;
INT32 st0020GfxROMLen;

static INT32 is_jclub2 = 0;
static INT32 is_st0032 = 0;

UINT16 st0020GfxramReadWord(UINT32 offset)
{
	INT32 bank = (st0020BlitRAM[0x8a] & 3) * 0x100000;

	UINT16 *ram = (UINT16*)(st0020GfxRAM + (offset & 0xfffff) + bank);

	return (ram[0] << 8) | (ram[0] >> 8);
}

UINT8 st0020GfxramReadByte(UINT32 offset)
{
	INT32 bank = (st0020BlitRAM[0x8a] & 3) * 0x100000;

	return st0020GfxRAM[((offset & 0xfffff) + bank)^1];
}

void st0020GfxramWriteByte(UINT32 offset, UINT8 data)
{
	INT32 bank = (st0020BlitRAM[0x8a] & 3) * 0x100000;

	st0020GfxRAM[((offset & 0xfffff) + bank) ^ 1] = data;
}

void st0020GfxramWriteWord(UINT32 offset, UINT16 data)
{
	INT32 bank = (st0020BlitRAM[0x8a] & 3) * 0x100000;

	UINT16 *ram = (UINT16*)(st0020GfxRAM + (offset & 0xfffff) + bank);

	data = (data << 8) | (data >> 8);

	ram[0] = data;
}

static void st0020_blitter_write()
{
	UINT16 *st0020_blitram = (UINT16*)st0020BlitRAM;
	UINT16 *st0020_gfxram = (UINT16*)st0020GfxRAM;

	UINT32 src  = (st0020_blitram[0xc0/2] + (st0020_blitram[0xc2/2] << 16)) << 1;
	UINT32 dst  = (st0020_blitram[0xc4/2] + (st0020_blitram[0xc6/2] << 16)) << 4;
	UINT32 len  = (st0020_blitram[0xc8/2]) << 4;
	UINT32 size = st0020GfxROMLen;

	dst &= 0x3fffff;
	src &= 0xffffff;

	if ((src+len <= size) && (dst+len <= 4 * 0x100000) )
	{
		memcpy( &st0020_gfxram[dst/2], &st0020GfxROM[src], len );
	}
}

void st0020_blitram_write_word(UINT32 offset, UINT16 data)
{
	UINT16 *st0020_blitram = (UINT16*)st0020BlitRAM;

	st0020_blitram[(offset/2)&0x7f] = data;

	if ((offset & 0xfe) == 0xca) st0020_blitter_write();
}

UINT16 st0020_blitram_read_word(UINT32 offset)
{
	UINT16 *st0020_blitram = (UINT16*)st0020BlitRAM;

	return st0020_blitram[(offset/2)&0x7f];
}

void st0020_blitram_write_byte(UINT32 offset, UINT8 data)
{
	st0020BlitRAM[(offset & 0xff)] = data;
	if ((offset & 0xfe) == 0xca) st0020_blitter_write();
}

static void st0020_draw_zooming_sprites(INT32 priority)
{
	UINT16 *spriteram16_2	= (UINT16*)st0020SprRAM;
	UINT16 *s1  		= spriteram16_2;
	UINT16 *end1    	= spriteram16_2 + 0x02000/2;

	const INT32 ramoffs[2][4] = { { 0, 1, 2, 3 }, { 2, 3, 1, 0 } };

	priority <<= 4;

	for ( ; s1 < end1; s1+=8/2 )
	{
		int attr, code, color, num, sprite, zoom, size;
		int sx, x, xoffs, flipx, xnum, xstart, xend, xinc, xdim, xscale;
		int sy, y, yoffs, flipy, ynum, ystart, yend, yinc, ydim, yscale;

		xoffs   =       s1[ ramoffs[is_st0032][0] ];
		yoffs   =       s1[ ramoffs[is_st0032][1] ];
		sprite  =       s1[ ramoffs[is_st0032][2] ];
		num     =       s1[ ramoffs[is_st0032][3] ] % 0x101; // how many?

		if (sprite & 0x8000) break;

		int s2 = 0;
		int spritebase = (sprite & 0x7fff) * 16/2;

		for( ; num > 0; num--,s2+=16/2 )
		{
			code    =   spriteram16_2[(spritebase + s2 + 0 )&0x3ffff];
			attr    =   spriteram16_2[(spritebase + s2 + 1 )&0x3ffff];
			sx      =   spriteram16_2[(spritebase + s2 + 2 )&0x3ffff];
			sy      =   spriteram16_2[(spritebase + s2 + 3 )&0x3ffff];
			zoom    =   spriteram16_2[(spritebase + s2 + 4 )&0x3ffff];
			size    =   spriteram16_2[(spritebase + s2 + 5 )&0x3ffff];

			if (priority != (size & 0xf0))
				break;

			flipx   =   (attr & 0x8000);
			flipy   =   (attr & 0x4000);

			color   =   (attr & 0x0400) ? attr : attr * 4;

			xnum = 1 << ((size >> 0) & 3);
			ynum = 1 << ((size >> 2) & 3);

			xnum = (xnum + 1) / 2;

			if (flipx)  { xstart = xnum-1;  xend = -1;    xinc = -1; }
			else        { xstart = 0;       xend = xnum;  xinc = +1; }

			if (flipy)  { ystart = ynum-1;  yend = -1;    yinc = -1; }
			else        { ystart = 0;       yend = ynum;  yinc = +1; }

			sx  +=  xoffs;
			sy  +=  yoffs;

			sx  =   (sx & 0x1ff) - (sx & 0x200);
			sy  =   (sy & 0x1ff) - (sy & 0x200);

			sy  =   -sy;

			if (is_jclub2)
				sy += 0x100;

			sx <<= 16;
			sy <<= 16;

			xdim    =   ( ( ((zoom >> 0) & 0xff) + 1) << 16 ) / xnum;
			ydim    =   ( ( ((zoom >> 8) & 0xff) + 1) << 16 ) / ynum;

			xscale  =   xdim / 16;
			yscale  =   ydim / 8;

			if (xscale & 0xffff)    xscale += (1<<16) / 16;
			if (yscale & 0xffff)    yscale += (1<<16) / 8;

			for (x = xstart; x != xend; x += xinc)
			{
				for (y = ystart; y != yend; y += yinc, code++)
				{
					RenderZoomedTile(pTransDraw, st0020GfxRAM, code % (0x400000/0x80), color * 64, 0, (sx + x * xdim) / 0x10000, (sy + y * ydim) / 0x10000, flipx, flipy, 16, 8, xscale, yscale);
				}
			}
		}
	}
}

void st0020Draw()
{
	for (int pri = 0; pri < 0x10; pri++)
		st0020_draw_zooming_sprites(pri);
}
