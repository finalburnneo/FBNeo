/******************************************************************************

K1GE/K2GE graphics emulation

The K1GE graphics were used in the Neogeo pocket mono; the K2GE graphics were
used in the Neogeo pocket color.

******************************************************************************/
// based on MESS .149 sources (Wilbert Pol)

#include "tiles_generic.h"
#include "burn_pal.h"
#include "k1ge.h"
#include "tlcs900_intf.h"

#define K1GE_SCREEN_HEIGHT  199

static void (*vblank_pin_w)(INT32,INT32);
static void (*hblank_pin_w)(INT32,INT32);

static INT32 current_scanline; // 
static UINT32 scanline_start_cycles; //

static UINT8 *vram;
static UINT8 wba_h, wba_v, wsi_h, wsi_v;
static void (*draw)(INT32 line);
static void (*palette)();

static void k1gePaletteInit()
{
	for (INT32 i = 0; i < 8; i++) {
		INT32 j = ( i << 5 ) | ( i << 2 ) | ( i >> 1 );

		BurnPalette[7-i] = BurnHighCol(j, j, j, 0);
	}
}

static void k2gePaletteInit()
{
	for (INT32 b = 0; b < 16; b++) {
		for (INT32 g = 0; g < 16; g++) {
			for (INT32 r = 0; r < 16; r++) {
				BurnPalette[( b << 8 ) | ( g << 4 ) | r] = BurnHighCol(( r << 4 ) | r, ( g << 4 ) | g, ( b << 4 ) | b,  0);
			}
		}
	}
}

UINT8 k1ge_r(UINT32 offset)
{
	offset &= 0x3fff;

	UINT8 data = vram[offset];

	switch( offset )
	{
	case 0x008:     /* RAS.H */
		data = (tlcs900TotalCycles() - scanline_start_cycles) / 4;
		if (data > (518/4)) data = (518/4)-1;
		// tlcs900TotalCycles() / 60 / 199 / 4; // h-pos
		break;
	case 0x009:     /* RAS.V */
		data = current_scanline;
		if (data > 198) data = 198;
		break;
	}

	return data;
}

void k1ge_w(UINT32 offset, UINT8 data)
{
	offset &= 0x3fff;

	switch( offset )
	{
	case 0x000:
		if (vblank_pin_w)
			vblank_pin_w(0, ( data & 0x80 ) ? ( ( vram[0x010] & 0x40 ) ? 1 : 0 ) : 0 );
		break;
	case 0x030:
		data &= 0x80;
		break;
	case 0x101: case 0x102: case 0x103:
	case 0x105: case 0x106: case 0x107:
	case 0x109: case 0x10a: case 0x10b:
	case 0x10d: case 0x10e: case 0x10f:
	case 0x111: case 0x112: case 0x113:
	case 0x115: case 0x116: case 0x117:
		data &= 0x07;
		break;
	case 0x7e2:
		if ( vram[0x7f0] != 0xAA )
			return;
		data &= 0x80;
		break;
	}

	/* Only the lower 4 bits of the palette entry high bytes can be written */
	if ( offset >= 0x0200 && offset < 0x0400 && ( offset & 1 ) )
	{
		data &= 0x0f;
	}

	vram[offset] = data;
}


inline void k1ge_draw_scroll_plane(UINT16 *p, UINT16 base, int line, int scroll_x, int scroll_y, int pal_base )
{
	if ((nBurnLayer & 1) == 0) return;

	int i;
	int offset_x = ( scroll_x >> 3 ) * 2;
	int px = scroll_x & 0x07;
	UINT16 map_data;
	UINT16 hflip;
	UINT16 pcode;
	UINT16 tile_addr;
	UINT16 tile_data;

	base += ( ( ( ( scroll_y + line ) >> 3 ) * 0x0040 ) & 0x7ff );

	/* setup */
	map_data = vram[ base + offset_x  ] | ( vram[ base + offset_x + 1 ] << 8 );
	hflip = map_data & 0x8000;
	pcode = pal_base + ( ( map_data & 0x2000 ) ? 4 : 0 );
	tile_addr = 0x2000 + ( ( map_data & 0x1ff ) * 16 );
	if ( map_data & 0x4000 )
		tile_addr += ( 7 - ( ( scroll_y + line ) & 0x07 ) ) * 2;
	else
		tile_addr += ( ( scroll_y + line ) & 0x07 ) * 2;
	tile_data = vram[ tile_addr ] | ( vram[ tile_addr + 1 ] << 8 );
	if ( hflip )
		tile_data >>= 2 * ( scroll_x & 0x07 );
	else
		tile_data <<= 2 * ( scroll_x & 0x07 );

	/* draw pixels */
	for ( i = 0; i < 160; i++ )
	{
		UINT16 col;

		if ( hflip )
		{
			col = tile_data & 0x0003;
			tile_data >>= 2;
		}
		else
		{
			col = tile_data >> 14;
			tile_data <<= 2;
		}

		if ( col )
		{
			p[ i ] = vram[ pcode + col ];
		}

		px++;
		if ( px >= 8 )
		{
			offset_x = ( offset_x + 2 ) & 0x3f;
			map_data = vram[ base + offset_x ] | ( vram[ base + offset_x + 1 ] << 8 );
			hflip = map_data & 0x8000;
			pcode = pal_base + ( ( map_data & 0x2000 ) ? 4 : 0 );
			tile_addr = 0x2000 + ( ( map_data & 0x1ff ) * 16 );
			if ( map_data & 0x4000 )
				tile_addr += ( 7 - ( ( scroll_y + line ) & 0x07 ) ) * 2;
			else
				tile_addr += ( ( scroll_y + line ) & 0x07 ) * 2;
			tile_data = vram[ tile_addr ] | ( vram[ tile_addr + 1 ] << 8 );
			px = 0;
		}
	}
}


inline void k1ge_draw_sprite_plane(UINT16 *p, UINT16 priority, int line, int scroll_x, int scroll_y )
{
	if ((nSpriteEnable & 1) == 0) return;

	struct {
		UINT16 spr_data;
		UINT8 x;
		UINT8 y;
	} spr[64];
	int num_sprites = 0;
	UINT8 spr_y = 0;
	UINT8 spr_x = 0;
	int i;

	priority <<= 11;

	/* Select sprites */
	for ( i = 0; i < 256; i += 4 )
	{
		UINT16 spr_data = vram[ 0x800 + i ] | ( vram[ 0x801 + i ] << 8 );
		UINT8 x = vram[ 0x802 + i ];
		UINT8 y = vram[ 0x803 + i ];

		spr_x = ( spr_data & 0x0400 ) ? ( spr_x + x ) :  ( scroll_x + x );
		spr_y = ( spr_data & 0x0200 ) ? ( spr_y + y ) :  ( scroll_y + y );

		if ( ( spr_data & 0x1800 ) == priority )
		{
			if ( ( line >= spr_y || spr_y > 0xf8 ) && line < ( ( spr_y + 8 ) & 0xff ) )
			{
				spr[num_sprites].spr_data = spr_data;
				spr[num_sprites].y = spr_y;
				spr[num_sprites].x = spr_x;
				num_sprites++;
			}
		}
	}

	/* Draw sprites */
	for ( i = num_sprites-1; i >= 0; i-- )
	{
		int j;
		UINT16 tile_addr;
		UINT16 tile_data;
		UINT16 pcode = 0x100 + ( ( spr[i].spr_data & 0x2000 ) ? 4 : 0 );

		tile_addr = 0x2000 + ( ( spr[i].spr_data & 0x1ff ) * 16 );
		if ( spr[i].spr_data & 0x4000 )
			tile_addr += ( 7 - ( ( line - spr[i].y ) & 0x07 ) ) * 2;
		else
			tile_addr += ( ( line - spr[i].y ) & 0x07 ) * 2;
		tile_data = vram[ tile_addr ] | ( vram[ tile_addr + 1 ] << 8 );

		for ( j = 0; j < 8; j++ )
		{
			UINT16 col;

			spr_x = spr[i].x + j;

			if ( spr[i].spr_data & 0x8000 )
			{
				col = tile_data & 0x03;
				tile_data >>= 2;
			}
			else
			{
				col = tile_data >> 14;
				tile_data <<= 2;
			}

			if ( spr_x < 160 && col )
			{
				p[ spr_x ] = vram[ pcode + col ];
			}
		}
	}
}


static void k1ge_draw( int line )
{
	UINT16 *p = pTransDraw + line * nScreenWidth;
	UINT16 oowcol = vram[0x012] & 0x07;
	int i;

	if ( line < wba_v || line >= wba_v + wsi_v )
	{
		for( i = 0; i < 160; i++ )
		{
			p[i] = oowcol;
		}
	}
	else
	{
		UINT16 col = ( ( vram[0x118] & 0xc0 ) == 0x80 ) ? vram[0x118] & 0x07 : 0;

		for ( i = 0; i < 160; i++ )
			p[i] = col;

		if ( vram[0x030] & 0x80 )
		{
			/* Draw sprites with 01 priority */
			k1ge_draw_sprite_plane(p, 1, line, vram[0x020], vram[0x021] );

			/* Draw PF1 */
			k1ge_draw_scroll_plane(p, 0x1000, line, vram[0x032], vram[0x033], 0x108 );

			/* Draw sprites with 10 priority */
			k1ge_draw_sprite_plane(p, 2, line, vram[0x020], vram[0x021] );

			/* Draw PF2 */
			k1ge_draw_scroll_plane(p, 0x1800, line, vram[0x034], vram[0x035], 0x110 );

			/* Draw sprites with 11 priority */
			k1ge_draw_sprite_plane(p, 3, line, vram[0x020], vram[0x021] );
		}
		else
		{
			/* Draw sprites with 01 priority */
			k1ge_draw_sprite_plane(p, 1, line, vram[0x020], vram[0x021] );

			/* Draw PF2 */
			k1ge_draw_scroll_plane(p, 0x1800, line, vram[0x034], vram[0x035], 0x110 );

			/* Draw sprites with 10 priority */
			k1ge_draw_sprite_plane(p, 2, line, vram[0x020], vram[0x021] );

			/* Draw PF1 */
			k1ge_draw_scroll_plane(p, 0x1000, line, vram[0x032], vram[0x033], 0x108 );

			/* Draw sprites with 11 priority */
			k1ge_draw_sprite_plane(p, 3, line, vram[0x020], vram[0x021] );
		}

		for( i = 0; i < wba_h; i++ )
		{
			p[i] = oowcol;
		}

		for( i = wba_h + wsi_h; i < 160; i++ )
		{
			p[i] = oowcol;
		}
	}
}


inline void k2ge_draw_scroll_plane(UINT16 *p, UINT16 base, int line, int scroll_x, int scroll_y, UINT16 pal_base )
{
	int i;
	int offset_x = ( scroll_x >> 3 ) * 2;
	int px = scroll_x & 0x07;
	UINT16 map_data;
	UINT16 hflip;
	UINT16 pcode;
	UINT16 tile_addr;
	UINT16 tile_data;

	base += ( ( ( ( scroll_y + line ) >> 3 ) * 0x0040 ) & 0x7ff );

	/* setup */
	map_data = vram[ base + offset_x  ] | ( vram[ base + offset_x + 1 ] << 8 );
	hflip = map_data & 0x8000;
	pcode = pal_base + ( ( map_data & 0x1e00 ) >> 6 );
	tile_addr = 0x2000 + ( ( map_data & 0x1ff ) * 16 );
	if ( map_data & 0x4000 )
		tile_addr += ( 7 - ( ( scroll_y + line ) & 0x07 ) ) * 2;
	else
		tile_addr += ( ( scroll_y + line ) & 0x07 ) * 2;
	tile_data = vram[ tile_addr ] | ( vram[ tile_addr + 1 ] << 8 );
	if ( hflip )
		tile_data >>= 2 * ( scroll_x & 0x07 );
	else
		tile_data <<= 2 * ( scroll_x & 0x07 );

	/* draw pixels */
	for ( i = 0; i < 160; i++ )
	{
		UINT16 col;

		if ( hflip )
		{
			col = tile_data & 0x0003;
			tile_data >>= 2;
		}
		else
		{
			col = tile_data >> 14;
			tile_data <<= 2;
		}

		if ( col )
		{
			p[ i ]  = vram[ pcode + col * 2 ] | ( vram[ pcode + col * 2 + 1 ] << 8 );
		}

		px++;
		if ( px >= 8 )
		{
			offset_x = ( offset_x + 2 ) & 0x3f;
			map_data = vram[ base + offset_x ] | ( vram[ base + offset_x + 1 ] << 8 );
			hflip = map_data & 0x8000;
			pcode = pal_base + ( ( map_data & 0x1e00 ) >> 6 );
			tile_addr = 0x2000 + ( ( map_data & 0x1ff ) * 16 );
			if ( map_data & 0x4000 )
				tile_addr += ( 7 - ( ( scroll_y + line ) & 0x07 ) ) * 2;
			else
				tile_addr += ( ( scroll_y + line ) & 0x07 ) * 2;
			tile_data = vram[ tile_addr ] | ( vram[ tile_addr + 1 ] << 8 );
			px = 0;
		}
	}
}


inline void k2ge_draw_sprite_plane(UINT16 *p, UINT16 priority, int line, int scroll_x, int scroll_y )
{
	struct {
		UINT16 spr_data;
		UINT8 x;
		UINT8 y;
		UINT8 index;
	} spr[64];
	int num_sprites = 0;
	UINT8 spr_y = 0;
	UINT8 spr_x = 0;
	int i;

	priority <<= 11;

	/* Select sprites */
	for ( i = 0; i < 256; i += 4 )
	{
		UINT16 spr_data = vram[ 0x800 + i ] | ( vram[ 0x801 + i ] << 8 );
		UINT8 x = vram[ 0x802 + i ];
		UINT8 y = vram[ 0x803 + i ];

		spr_x = ( spr_data & 0x0400 ) ? ( spr_x + x ) :  ( scroll_x + x );
		spr_y = ( spr_data & 0x0200 ) ? ( spr_y + y ) :  ( scroll_y + y );

		if ( ( spr_data & 0x1800 ) == priority )
		{
			if ( ( line >= spr_y || spr_y > 0xf8 ) && line < ( ( spr_y + 8 ) & 0xff ) )
			{
				spr[num_sprites].spr_data = spr_data;
				spr[num_sprites].y = spr_y;
				spr[num_sprites].x = spr_x;
				spr[num_sprites].index = i >> 2;
				num_sprites++;
			}
		}
	}

	/* Draw sprites */
	for ( i = num_sprites-1; i >= 0; i-- )
	{
		int j;
		UINT16 tile_addr;
		UINT16 tile_data;
		UINT16 pcode = 0x0200 + ( ( vram[0x0c00 + spr[i].index ] & 0x0f ) << 3 );

		tile_addr = 0x2000 + ( ( spr[i].spr_data & 0x1ff ) * 16 );
		if ( spr[i].spr_data & 0x4000 )
			tile_addr += ( 7 - ( ( line - spr[i].y ) & 0x07 ) ) * 2;
		else
			tile_addr += ( ( line - spr[i].y ) & 0x07 ) * 2;
		tile_data = vram[ tile_addr ] | ( vram[ tile_addr + 1 ] << 8 );

		for ( j = 0; j < 8; j++ )
		{
			UINT16 col;

			spr_x = spr[i].x + j;

			if ( spr[i].spr_data & 0x8000 )
			{
				col = tile_data & 0x03;
				tile_data >>= 2;
			}
			else
			{
				col = tile_data >> 14;
				tile_data <<= 2;
			}

			if ( spr_x < 160 && col )
			{
				p[ spr_x ] = vram[ pcode + col * 2 ] | ( vram[ pcode + col * 2 + 1 ] << 8 );
			}
		}
	}
}


void k2ge_k1ge_draw_scroll_plane(UINT16 *p, UINT16 base, int line, int scroll_x, int scroll_y, UINT16 pal_lut_base, UINT16 k2ge_lut_base )
{
	int i;
	int offset_x = ( scroll_x >> 3 ) * 2;
	int px = scroll_x & 0x07;
	UINT16 map_data;
	UINT16 hflip;
	UINT16 pcode;
	UINT16 tile_addr;
	UINT16 tile_data;

	base += ( ( ( ( scroll_y + line ) >> 3 ) * 0x0040 ) & 0x7ff );

	/* setup */
	map_data = vram[ base + offset_x  ] | ( vram[ base + offset_x + 1 ] << 8 );
	hflip = map_data & 0x8000;
	pcode = ( map_data & 0x2000 ) ? 1 : 0;
	tile_addr = 0x2000 + ( ( map_data & 0x1ff ) * 16 );
	if ( map_data & 0x4000 )
		tile_addr += ( 7 - ( ( scroll_y + line ) & 0x07 ) ) * 2;
	else
		tile_addr += ( ( scroll_y + line ) & 0x07 ) * 2;
	tile_data = vram[ tile_addr ] | ( vram[ tile_addr + 1 ] << 8 );
	if ( hflip )
		tile_data >>= 2 * ( scroll_x & 0x07 );
	else
		tile_data <<= 2 * ( scroll_x & 0x07 );

	/* draw pixels */
	for ( i = 0; i < 160; i++ )
	{
		UINT16 col;

		if ( hflip )
		{
			col = tile_data & 0x0003;
			tile_data >>= 2;
		}
		else
		{
			col = tile_data >> 14;
			tile_data <<= 2;
		}

		if ( col )
		{
			UINT16 col2 = 16 * pcode + ( vram[ pal_lut_base + 4 * pcode + col ] * 2 );
			p[ i ]  = vram[ k2ge_lut_base + col2 ] | ( vram[ k2ge_lut_base + col2 + 1 ] << 8 );
		}

		px++;
		if ( px >= 8 )
		{
			offset_x = ( offset_x + 2 ) & 0x3f;
			map_data = vram[ base + offset_x ] | ( vram[ base + offset_x + 1 ] << 8 );
			hflip = map_data & 0x8000;
			pcode = ( map_data & 0x2000 ) ? 1 : 0;
			tile_addr = 0x2000 + ( ( map_data & 0x1ff ) * 16 );
			if ( map_data & 0x4000 )
				tile_addr += ( 7 - ( ( scroll_y + line ) & 0x07 ) ) * 2;
			else
				tile_addr += ( ( scroll_y + line ) & 0x07 ) * 2;
			tile_data = vram[ tile_addr ] | ( vram[ tile_addr + 1 ] << 8 );
			px = 0;
		}
	}
}


inline void k2ge_k1ge_draw_sprite_plane(UINT16 *p, UINT16 priority, int line, int scroll_x, int scroll_y )
{
	struct {
		UINT16 spr_data;
		UINT8 x;
		UINT8 y;
	} spr[64];
	int num_sprites = 0;
	UINT8 spr_y = 0;
	UINT8 spr_x = 0;
	int i;

	priority <<= 11;

	/* Select sprites */
	for ( i = 0; i < 256; i += 4 )
	{
		UINT16 spr_data = vram[ 0x800 + i ] | ( vram[ 0x801 + i ] << 8 );
		UINT8 x = vram[ 0x802 + i ];
		UINT8 y = vram[ 0x803 + i ];

		spr_x = ( spr_data & 0x0400 ) ? ( spr_x + x ) :  ( scroll_x + x );
		spr_y = ( spr_data & 0x0200 ) ? ( spr_y + y ) :  ( scroll_y + y );

		if ( ( spr_data & 0x1800 ) == priority )
		{
			if ( ( line >= spr_y || spr_y > 0xf8 ) && line < ( ( spr_y + 8 ) & 0xff ) )
			{
				spr[num_sprites].spr_data = spr_data;
				spr[num_sprites].y = spr_y;
				spr[num_sprites].x = spr_x;
				num_sprites++;
			}
		}
	}

	/* Draw sprites */
	for ( i = num_sprites-1; i >= 0; i-- )
	{
		int j;
		UINT16 tile_addr;
		UINT16 tile_data;
		UINT16 pcode = ( spr[i].spr_data & 0x2000 ) ? 1 : 0;

		tile_addr = 0x2000 + ( ( spr[i].spr_data & 0x1ff ) * 16 );
		if ( spr[i].spr_data & 0x4000 )
			tile_addr += ( 7 - ( ( line - spr[i].y ) & 0x07 ) ) * 2;
		else
			tile_addr += ( ( line - spr[i].y ) & 0x07 ) * 2;
		tile_data = vram[ tile_addr ] | ( vram[ tile_addr + 1 ] << 8 );

		for ( j = 0; j < 8; j++ )
		{
			UINT16 col;

			spr_x = spr[i].x + j;

			if ( spr[i].spr_data & 0x8000 )
			{
				col = tile_data & 0x03;
				tile_data >>= 2;
			}
			else
			{
				col = tile_data >> 14;
				tile_data <<= 2;
			}

			if ( spr_x < 160 && col )
			{
				UINT16 col2 = 16 * pcode + vram[ 0x100 + 4 * pcode + col ] * 2;
				p[ spr_x ] = vram[ 0x380 + col2 ] | ( vram[ 0x381 + col2 ] << 8 );
			}
		}
	}
}


static void k2ge_draw( int line )
{
	UINT16 *p = pTransDraw + line * nScreenWidth;
	UINT16 col = 0;
	UINT16 oowcol;
	int i;

	oowcol = ( vram[0x012] & 0x07 ) * 2;
	oowcol = vram[0x3f0 + oowcol ] | ( vram[0x3f1 + oowcol ] << 8 );

	if ( line < wba_v || line >= wba_v + wsi_v )
	{
		for( i = 0; i < 160; i++ )
		{
			p[i] = oowcol;
		}
	}
	else
	{
		/* Determine the background color */
		if ( ( vram[0x118] & 0xc0 ) == 0x80 )
		{
			col = ( vram[0x118] & 0x07 ) * 2;
		}
		col = vram[0x3e0 + col ] | ( vram[0x3e1 + col ] << 8 );

		/* Set the bacground color */
		for ( i = 0; i < 160; i++ )
		{
			p[i] = col;
		}

		if ( vram[0x7e2] & 0x80 )
		{
			/* K1GE compatibility mode */
			if ( vram[0x030] & 0x80 )
			{
				/* Draw sprites with 01 priority */
				k2ge_k1ge_draw_sprite_plane(p, 1, line, vram[0x020], vram[0x021] );

				/* Draw PF1 */
				k2ge_k1ge_draw_scroll_plane(p, 0x1000, line, vram[0x032], vram[0x033], 0x108, 0x3a0 );

				/* Draw sprites with 10 priority */
				k2ge_k1ge_draw_sprite_plane(p, 2, line, vram[0x020], vram[0x021] );

				/* Draw PF2 */
				k2ge_k1ge_draw_scroll_plane(p, 0x1800, line, vram[0x034], vram[0x035], 0x110, 0x3c0 );

				/* Draw sprites with 11 priority */
				k2ge_k1ge_draw_sprite_plane(p, 3, line, vram[0x020], vram[0x021] );
			}
			else
			{
				/* Draw sprites with 01 priority */
				k2ge_k1ge_draw_sprite_plane(p, 1, line, vram[0x020], vram[0x021] );

				/* Draw PF2 */
				k2ge_k1ge_draw_scroll_plane(p, 0x1800, line, vram[0x034], vram[0x035], 0x110, 0x3c0 );

				/* Draw sprites with 10 priority */
				k2ge_k1ge_draw_sprite_plane(p, 2, line, vram[0x020], vram[0x021] );

				/* Draw PF1 */
				k2ge_k1ge_draw_scroll_plane(p, 0x1000, line, vram[0x032], vram[0x033], 0x108, 0x3a0 );

				/* Draw sprites with 11 priority */
				k2ge_k1ge_draw_sprite_plane(p, 3, line, vram[0x020], vram[0x021] );
			}
		}
		else
		{
			/* K2GE mode */
			if ( vram[0x030] & 0x80 )
			{
				/* Draw sprites with 01 priority */
				k2ge_draw_sprite_plane(p, 1, line, vram[0x020], vram[0x021] );

				/* Draw PF1 */
				k2ge_draw_scroll_plane(p, 0x1000, line, vram[0x032], vram[0x033], 0x280 );

				/* Draw sprites with 10 priority */
				k2ge_draw_sprite_plane(p, 2, line, vram[0x020], vram[0x021] );

				/* Draw PF2 */
				k2ge_draw_scroll_plane(p, 0x1800, line, vram[0x034], vram[0x035], 0x300 );

				/* Draw sprites with 11 priority */
				k2ge_draw_sprite_plane(p, 3, line, vram[0x020], vram[0x021] );
			}
			else
			{
				/* Draw sprites with 01 priority */
				k2ge_draw_sprite_plane(p, 1, line, vram[0x020], vram[0x021] );

				/* Draw PF2 */
				k2ge_draw_scroll_plane(p, 0x1800, line, vram[0x034], vram[0x035], 0x300 );

				/* Draw sprites with 10 priority */
				k2ge_draw_sprite_plane(p, 2, line, vram[0x020], vram[0x021] );

				/* Draw PF1 */
				k2ge_draw_scroll_plane(p, 0x1000, line, vram[0x032], vram[0x033], 0x280 );

				/* Draw sprites with 11 priority */
				k2ge_draw_sprite_plane(p, 3, line, vram[0x020], vram[0x021] );
			}
		}

		for ( i = 0; i < wba_h; i++ )
		{
			p[i] = oowcol;
		}

		for ( i = wba_h + wsi_h; i < 160; i++ )
		{
			p[i] = oowcol;
		}
	}
}


static INT32 hblank_timer = 0;

void k1ge_hblank_on_timer_callback()
{
	if (hblank_pin_w && hblank_timer)
	{
		hblank_timer = 0;
		hblank_pin_w(0, 0);
	}
}

INT32 k1ge_scanline_timer_callback(INT32 scanline)
{
	int y = scanline;
	current_scanline = y;
	scanline_start_cycles = tlcs900TotalCycles(); // hacky way to get hpos

	/* Check for start of VBlank */
	if ( y >= 152 )
	{
		vram[0x010] |= 0x40;
		if ((vram[0x000] & 0x80 ) && vblank_pin_w)
			vblank_pin_w(0, 1);
	}

	/* Check for end of VBlank */
	if ( y == 0 )
	{
		wba_h = vram[0x002];
		if (wba_h > 159) wba_h = 0;
		wba_v = vram[0x003];
		wsi_h = vram[0x004];
		wsi_v = vram[0x005];
		vram[0x010] &= ~ 0x40;
		if ((vram[0x000] & 0x80 ) && vblank_pin_w)
			vblank_pin_w(0, 0);
	}

	/* Check if Hint should be triggered */
	if ( y == K1GE_SCREEN_HEIGHT - 1 || y < 151 )
	{
		if (hblank_pin_w)
		{
			if ( vram[0x000] & 0x40 )
				hblank_pin_w(0, 1);
			hblank_timer = 1; //hblank_on_timer->adjust( screen->time_until_pos(y, 480 ) );
		}
	}

	/* Draw a line when inside visible area */
	if ( y && y < 153 )
	{
		draw(y - 1);
	}
	
	return hblank_timer;
}

INT32 k1geDraw()
{
	if (BurnRecalc) {
		palette();
		BurnRecalc = 0;
	}

	BurnTransferCopy(BurnPalette);

	return 0;
}

void k1geInit(INT32 color, void (*vblank_pin_cb)(INT32,INT32), void (*hblank_pin_cb)(INT32,INT32))
{
	draw = (color) ? k2ge_draw : k1ge_draw;
	palette = (color) ? k2gePaletteInit : k1gePaletteInit;
	BurnPalette = (UINT32*)BurnMalloc(4096 * sizeof(UINT32)); //(color ? 4096 : 8) * sizeof(UINT32));
	vram = (UINT8*)BurnMalloc(0x4000);
	vblank_pin_w = vblank_pin_cb;
	hblank_pin_w = hblank_pin_cb;
}

void k1geExit()
{
	BurnFree(BurnPalette);
	BurnFree(vram);
}

void k1geScan(INT32 nAction, INT32 *)
{
	struct BurnArea ba;

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = vram;
		ba.nLen	  = 0x4000;
		ba.szName = "k1ge Video Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(wba_h);
		SCAN_VAR(wba_v);
		SCAN_VAR(wsi_h);
		SCAN_VAR(wsi_v);
	}
}

void k1geReset()
{
	memset (vram, 0, 0x4000);
	
	vram[0x000] = 0x00;   /* Interrupt enable */
	vram[0x002] = 0x00;   /* WBA.H */
	vram[0x003] = 0x00;   /* WVA.V */
	vram[0x004] = 0xFF;   /* WSI.H */
	vram[0x005] = 0xFF;   /* WSI.V */
	vram[0x007] = 0xc6;   /* REF */
	vram[0x012] = 0x00;   /* 2D control */
	vram[0x020] = 0x00;   /* PO.H */
	vram[0x021] = 0x00;   /* PO.V */
	vram[0x030] = 0x00;   /* PF */
	vram[0x032] = 0x00;   /* S1SO.H */
	vram[0x033] = 0x00;   /* S1SO.V */
	vram[0x034] = 0x00;   /* S2SO.H */
	vram[0x035] = 0x00;   /* S2SO.V */
	vram[0x101] = 0x07;   /* SPPLT01 */
	vram[0x102] = 0x07;   /* SPPLT02 */
	vram[0x103] = 0x07;   /* SPPLT03 */
	vram[0x105] = 0x07;   /* SPPLT11 */
	vram[0x106] = 0x07;   /* SPPLT12 */
	vram[0x107] = 0x07;   /* SPPLT13 */
	vram[0x109] = 0x07;   /* SC1PLT01 */
	vram[0x10a] = 0x07;   /* SC1PLT02 */
	vram[0x10b] = 0x07;   /* SC1PLT03 */
	vram[0x10d] = 0x07;   /* SC1PLT11 */
	vram[0x10e] = 0x07;   /* SC1PLT12 */
	vram[0x10f] = 0x07;   /* SC1PLT13 */
	vram[0x111] = 0x07;   /* SC2PLT01 */
	vram[0x112] = 0x07;   /* SC2PLT02 */
	vram[0x113] = 0x07;   /* SC2PLT03 */
	vram[0x115] = 0x07;   /* SC2PLT11 */
	vram[0x116] = 0x07;   /* SC2PLT12 */
	vram[0x117] = 0x07;   /* SC2PLT13 */
	vram[0x118] = 0x07;   /* BG */
	vram[0x400] = 0xFF;   /* LED control */
	vram[0x402] = 0x80;   /* LEDFREG */
	vram[0x7e0] = 0x52;   /* RESET */
	vram[0x7e2] = 0x00;   /* MODE */
}
