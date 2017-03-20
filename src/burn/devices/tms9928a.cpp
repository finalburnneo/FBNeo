// Based on MAME sources by Sean Young, Nathan Woods, Aaron Giles, Wilbert Pol, and hap

#include "tiles_generic.h"
#include "tms9928a.h"

/* Some defines used in defining the screens */
#define TMS9928A_TOTAL_HORZ                 342
#define TMS9928A_TOTAL_VERT_NTSC            262
#define TMS9928A_TOTAL_VERT_PAL             313

#define TMS9928A_HORZ_DISPLAY_START         (2 + 14 + 8 + 13)
#define TMS9928A_VERT_DISPLAY_START_PAL     16
#define TMS9928A_VERT_DISPLAY_START_NTSC    16

static INT32 TMS9928A_palette[16] = {
	0x000000, 0x000000, 0x21c842, 0x5edc78, 0x5455ed, 0x7d76fc, 0xd4524d, 0x42ebf5,
	0xfc5554, 0xff7978, 0xd4c154, 0xe6ce80, 0x21b03b, 0xc95bba, 0xcccccc, 0xffffff
};

typedef struct {
	UINT8 mode;
	UINT8 ReadAhead;
	UINT8 Regs[8];
	UINT8 StatusReg;
	UINT8 FifthSprite;
	UINT8 FirstByte;
	UINT8 latch;
	UINT8 INT;
	INT32 Addr;
	INT32 colour;
	INT32 pattern;
	INT32 nametbl;
	INT32 spriteattribute;
	INT32 spritepattern;
	INT32 colourmask;
	INT32 patternmask;

	UINT8 *vMem;
	UINT16 *tmpbmp;
	INT32 tmpbmpsize;
	INT32 vramsize;
	INT32 model;
	INT32 revA;

	INT32 LimitSprites;
	INT32 top_border;
	INT32 bottom_border;
	INT32 vertical_size;

	void (*INTCallback)(INT32);
} TMS9928A;

static TMS9928A tms;

static INT32 TMS9928A_initted = 0;

static UINT32 Palette[16]; // high color support

static void TMS89928aPaletteRecalc()
{
	for (INT32 i = 0; i < 16; i++) {
		Palette[i] = BurnHighCol(TMS9928A_palette[i] >> 16, TMS9928A_palette[i] >> 8, TMS9928A_palette[i] >> 0 , 0);
	}
}

static void check_interrupt()
{
	INT32 b = (tms.StatusReg & 0x80 && tms.Regs[1] & 0x20) ? 1 : 0;
	if (b != tms.INT) {
		tms.INT = b;
		if (tms.INTCallback) tms.INTCallback (tms.INT);
	}
}

static void update_table_masks()
{
	tms.colourmask = (tms.Regs[3] & 0x7f) * 8 | 7;
	tms.patternmask = (tms.Regs[4] & 3) * 256 |	(tms.colourmask & 0xff);
}

static void change_register(INT32 reg, UINT8 val)
{
	static const UINT8 Mask[8] = { 0x03, 0xfb, 0x0f, 0xff, 0x07, 0x7f, 0x07, 0xff };

	val &= Mask[reg];
	tms.Regs[reg] = val;

	switch (reg)
	{
		case 0:
		{
			if (val & 2) {
				tms.colour = ((tms.Regs[3] & 0x80) * 64) & (tms.vramsize - 1);
				tms.pattern = ((tms.Regs[4] & 4) * 2048) & (tms.vramsize - 1);
				update_table_masks();
			} else {
				tms.colour = (tms.Regs[3] * 64) & (tms.vramsize - 1);
				tms.pattern = (tms.Regs[4] * 2048) & (tms.vramsize - 1);
			}
			tms.mode = ( (tms.revA ? (tms.Regs[0] & 2) : 0) | ((tms.Regs[1] & 0x10)>>4) | ((tms.Regs[1] & 8)>>1));
		}
		break;

		case 1:
		{
			tms.mode = ( (tms.revA ? (tms.Regs[0] & 2) : 0) | ((tms.Regs[1] & 0x10)>>4) | ((tms.Regs[1] & 8)>>1));
			check_interrupt();
		}
		break;

		case 2:
			tms.nametbl = (val * 1024) & (tms.vramsize - 1);
		break;

		case 3:
		{
			if (tms.Regs[0] & 2) {
				tms.colour = ((val & 0x80) * 64) & (tms.vramsize - 1);
				update_table_masks();
			} else {
				tms.colour = (val * 64) & (tms.vramsize - 1);
			}
		}
		break;

		case 4:
		{
			if (tms.Regs[0] & 2) {
				tms.pattern = ((val & 4) * 2048) & (tms.vramsize - 1);
				update_table_masks();
			} else {
				tms.pattern = (val * 2048) & (tms.vramsize - 1);
			}
		}
		break;

		case 5:
			tms.spriteattribute = (val * 128) & (tms.vramsize - 1);
		break;

		case 6:
			tms.spritepattern = (val * 2048) & (tms.vramsize - 1);
		break;

		case 7:
			/* The backdrop is updated at TMS9928A_refresh() */
		break;
	}
}

void TMS9928AReset()
{
	for (INT32 i = 0; i < 8; i++)
		tms.Regs[i] = 0;

	memset(tms.vMem, 0, tms.vramsize);
	memset(tms.tmpbmp, 0, tms.tmpbmpsize);

	tms.StatusReg = 0;
	tms.FifthSprite = 31;
	tms.nametbl = tms.pattern = tms.colour = 0;
	tms.spritepattern = tms.spriteattribute = 0;
	tms.colourmask = tms.patternmask = 0x3fff;
	tms.Addr = tms.ReadAhead = tms.INT = 0;
	tms.FirstByte = 0;
	tms.latch = 0;
	tms.mode = 0;
}

void TMS9928AInit(INT32 model, INT32 vram, INT32 borderx, INT32 bordery, void (*INTCallback)(int))
{
	TMS9928A_initted = 1;

	GenericTilesInit();

	memset(&tms, 0, sizeof(tms));
	tms.model = model;
	tms.revA = 1;

	tms.INTCallback = INTCallback;

	tms.top_border		= TMS9928A_VERT_DISPLAY_START_NTSC;
	tms.bottom_border	= ((tms.model == TMS9929) || (tms.model == TMS9929A)) ? 51 : 24;
	tms.vertical_size   = TMS9928A_TOTAL_VERT_NTSC;

	tms.vramsize = vram;
	tms.vMem = (UINT8*)malloc(tms.vramsize);

	tms.tmpbmpsize = TMS9928A_TOTAL_HORZ * TMS9928A_TOTAL_VERT_PAL * sizeof(short) * 2;
	tms.tmpbmp = (UINT16*)malloc(tms.tmpbmpsize);

	TMS9928AReset ();
	tms.LimitSprites = 1;
}

void TMS9928AExit()
{
	if (!TMS9928A_initted) return;

	TMS9928A_initted = 0;
	TMS9928AReset();

	GenericTilesExit();

	free (tms.tmpbmp);
	free (tms.vMem);
}

void TMS9928APostLoad()
{
	for (INT32 i = 0; i < 8; i++)
		change_register(i, tms.Regs[i]);

	if (tms.INTCallback) tms.INTCallback(tms.INT);
}

UINT8 TMS9928AReadVRAM()
{
	INT32 b = tms.ReadAhead;
	tms.ReadAhead = tms.vMem[tms.Addr];
	tms.Addr = (tms.Addr + 1) & (tms.vramsize - 1);
	tms.latch = 0;
	return b;
}

void TMS9928AWriteVRAM(INT32 data)
{
	tms.vMem[tms.Addr] = data;
	tms.Addr = (tms.Addr + 1) & (tms.vramsize - 1);
	tms.ReadAhead = data;
	tms.latch = 0;
}

UINT8 TMS9928AReadRegs()
{
	INT32 b = tms.StatusReg;
	tms.StatusReg = tms.FifthSprite;
	check_interrupt();
	tms.latch = 0;
	return b;
}

void TMS9928AWriteRegs(INT32 data)
{
	if (tms.latch) {
		/* set high part of read/write address */
		tms.Addr = ((UINT16)data << 8 | (tms.Addr & 0xff)) & (tms.vramsize - 1);
		if (data & 0x80) {
			change_register(data & 0x07, tms.FirstByte);
		} else {
			if (!(data & 0x40)) {
				TMS9928AReadVRAM();
			}
		}

		tms.latch = 0;
	} else {
		/* set low part of read/write address */
		tms.Addr = ((tms.Addr & 0xff00) | data) & (tms.vramsize - 1);
		tms.FirstByte = data;
		tms.latch = 1;
	}
}

void TMS9928ASetSpriteslimit(INT32 limit)
{
	tms.LimitSprites = limit;
}

static inline UINT8 readvmem(INT32 vaddr)
{
	return tms.vMem[vaddr];
}

static void TMS9928AScanline_INT(INT32 vpos)
{
	UINT16 BackColour = tms.Regs[7] & 0xf;
	UINT16 *p = tms.tmpbmp + (vpos * TMS9928A_TOTAL_HORZ);

	INT32 y = vpos - tms.top_border;

	if ( y < 0 || y >= 192 || !(tms.Regs[1] & 0x40) )
	{
		/* Draw backdrop colour */
		for ( INT32 i = 0; i < TMS9928A_TOTAL_HORZ; i++ )
			p[i] = BackColour;

		/* vblank is set at the last cycle of the first inactive line */
		if ( y == 193 )
		{
			tms.StatusReg |= 0x80;
			check_interrupt();
		}
	}
	else
	{
		/* Draw regular line */

		/* Left border */
		for ( INT32 i = 0; i < TMS9928A_HORZ_DISPLAY_START; i++ )
			p[i] = BackColour;

		/* Active display */

		switch( tms.mode )
		{
		case 0:             /* MODE 0 */
			{
				UINT16 addr = tms.nametbl + ( ( y & 0xF8 ) << 2 );

				for ( INT32 x = TMS9928A_HORZ_DISPLAY_START; x < TMS9928A_HORZ_DISPLAY_START + 256; x+= 8, addr++ )
				{
					UINT8 charcode = readvmem( addr );
					UINT8 pattern =  readvmem( tms.pattern + ( charcode << 3 ) + ( y & 7 ) );
					UINT8 colour =  readvmem( tms.colour + ( charcode >> 3 ) );
					UINT16 fg = (colour >> 4) ? (colour >> 4) : BackColour;
					UINT16 bg = (colour & 15) ? (colour & 15) : BackColour;

					for ( INT32 i = 0; i < 8; pattern <<= 1, i++ )
						p[x+i] = ( pattern & 0x80 ) ? fg : bg;
				}
			}
			break;

		case 1:             /* MODE 1 */
			{
				UINT16 addr = tms.nametbl + ( ( y >> 3 ) * 40 );
				UINT16 fg = (tms.Regs[7] >> 4) ? (tms.Regs[7] >> 4) : BackColour;
				UINT16 bg = BackColour;

				/* Extra 6 pixels left border */
				for ( INT32 x = TMS9928A_HORZ_DISPLAY_START; x < TMS9928A_HORZ_DISPLAY_START + 6; x++ )
					p[x] = bg;

				for ( INT32 x = TMS9928A_HORZ_DISPLAY_START + 6; x < TMS9928A_HORZ_DISPLAY_START + 246; x+= 6, addr++ )
				{
					UINT16 charcode =  readvmem( addr );
					UINT8 pattern =  readvmem( tms.pattern + ( charcode << 3 ) + ( y & 7 ) );

					for ( INT32 i = 0; i < 6; pattern <<= 1, i++ )
						p[x+i] = ( pattern & 0x80 ) ? fg : bg;
				}

				/* Extra 10 pixels right border */
				for ( INT32 x = TMS9928A_HORZ_DISPLAY_START + 246; x < TMS9928A_HORZ_DISPLAY_START + 256; x++ )
					p[x] = bg;
			}
			break;

		case 2:             /* MODE 2 */
			{
				UINT16 addr = tms.nametbl + ( ( y >> 3 ) * 32 );

				for ( INT32 x = TMS9928A_HORZ_DISPLAY_START; x < TMS9928A_HORZ_DISPLAY_START + 256; x+= 8, addr++ )
				{
					UINT16 charcode =  readvmem( addr ) + ( ( y >> 6 ) << 8 );
					UINT8 pattern =  readvmem( tms.pattern + ( ( charcode & tms.patternmask ) << 3 ) + ( y & 7 ) );
					UINT8 colour =  readvmem( tms.colour + ( ( charcode & tms.colourmask ) << 3 ) + ( y & 7 ) );
					UINT16 fg = (colour >> 4) ? (colour >> 4) : BackColour;
					UINT16 bg = (colour & 15) ? (colour & 15) : BackColour;

					for ( INT32 i = 0; i < 8; pattern <<= 1, i++ )
						p[x+i] = ( pattern & 0x80 ) ? fg : bg;
				}
			}
			break;

		case 3:             /* MODE 1+2 */
			{
				UINT16 addr = tms.nametbl + ( ( y >> 3 ) * 40 );
				UINT16 fg = (tms.Regs[7] >> 4) ? (tms.Regs[7] >> 4) : BackColour;
				UINT16 bg = BackColour;

				/* Extra 6 pixels left border */
				for ( INT32 x = TMS9928A_HORZ_DISPLAY_START; x < TMS9928A_HORZ_DISPLAY_START + 6; x++ )
					p[x] = bg;

				for ( INT32 x = TMS9928A_HORZ_DISPLAY_START + 6; x < TMS9928A_HORZ_DISPLAY_START + 246; x+= 6, addr++ )
				{
					UINT16 charcode = (  readvmem( addr ) + ( ( y >> 6 ) << 8 ) ) & tms.patternmask;
					UINT8 pattern = readvmem( tms.pattern + ( charcode << 3 ) + ( y & 7 ) );

					for ( INT32 i = 0; i < 6; pattern <<= 1, i++ )
						p[x+i] = ( pattern & 0x80 ) ? fg : bg;
				}

				/* Extra 10 pixels right border */
				for ( INT32 x = TMS9928A_HORZ_DISPLAY_START + 246; x < TMS9928A_HORZ_DISPLAY_START + 256; x++ )
					p[x] = bg;
			}
			break;

		case 4:             /* MODE 3 */
			{
				UINT16 addr = tms.nametbl + ( ( y >> 3 ) * 32 );

				for ( INT32 x = TMS9928A_HORZ_DISPLAY_START; x < TMS9928A_HORZ_DISPLAY_START + 256; x+= 8, addr++ )
				{
					UINT8 charcode =  readvmem( addr );
					UINT8 colour =  readvmem( tms.pattern + ( charcode << 3 ) + ( ( y >> 2 ) & 7 ) );
					UINT16 fg = (colour >> 4) ? (colour >> 4) : BackColour;
					UINT16 bg = (colour & 15) ? (colour & 15) : BackColour;

					p[x+0] = p[x+1] = p[x+2] = p[x+3] = fg;
					p[x+4] = p[x+5] = p[x+6] = p[x+7] = bg;
				}
			}
			break;

		case 5: case 7:     /* MODE bogus */
			{
				UINT16 fg = (tms.Regs[7] >> 4) ? (tms.Regs[7] >> 4) : BackColour;
				UINT16 bg = BackColour;

				/* Extra 6 pixels left border */
				for ( INT32 x = TMS9928A_HORZ_DISPLAY_START; x < TMS9928A_HORZ_DISPLAY_START + 6; x++ )
					p[x] = bg;

				for ( INT32 x = TMS9928A_HORZ_DISPLAY_START + 6; x < TMS9928A_HORZ_DISPLAY_START + 246; x+= 6 )
				{
					p[x+0] = p[x+1] = p[x+2] = p[x+3] = fg;
					p[x+4] = p[x+5] = bg;
				}

				/* Extra 10 pixels right border */
				for ( INT32 x = TMS9928A_HORZ_DISPLAY_START + 246; x < TMS9928A_HORZ_DISPLAY_START + 256; x++ )
					p[x] = bg;
			}
			break;

		case 6:             /* MODE 2+3 */
			{
				UINT16 addr = tms.nametbl + ( ( y >> 3 ) * 32 );

				for ( INT32 x = TMS9928A_HORZ_DISPLAY_START; x < TMS9928A_HORZ_DISPLAY_START + 256; x+= 8, addr++ )
				{
					UINT8 charcode =  readvmem( addr );
					UINT8 colour =  readvmem( tms.pattern + ( ( ( charcode + ( ( y >> 2 ) & 7 ) + ( ( y >> 6 ) << 8 ) ) & tms.patternmask ) << 3 ) );
					UINT16 fg = (colour >> 4) ? (colour >> 4) : BackColour;
					UINT16 bg = (colour & 15) ? (colour & 15) : BackColour;

					p[x+0] = p[x+1] = p[x+2] = p[x+3] = fg;
					p[x+4] = p[x+5] = p[x+6] = p[x+7] = bg;
				}
			}
			break;
		}

		/* Draw sprites */
		if ( ( tms.Regs[1] & 0x50 ) != 0x40 )
		{
			/* sprites are disabled */
			tms.FifthSprite = 31;
		}
		else
		{
			UINT8 sprite_size = ( tms.Regs[1] & 0x02 ) ? 16 : 8;
			UINT8 sprite_mag = tms.Regs[1] & 0x01;
			UINT8 sprite_height = sprite_size * ( sprite_mag + 1 );
			UINT8 spr_drawn[32+256+32] = { 0 };
			UINT8 num_sprites = 0;
			bool fifth_encountered = false;

			for ( UINT16 sprattr = 0; sprattr < 128; sprattr += 4 )
			{
				INT32 spr_y =  readvmem( tms.spriteattribute + sprattr + 0 );

				tms.FifthSprite = sprattr / 4;

				/* Stop processing sprites */
				if ( spr_y == 208 )
					break;

				if ( spr_y > 0xE0 )
					spr_y -= 256;

				/* vert pos 255 is displayed on the first line of the screen */
				spr_y++;

				/* is sprite enabled on this line? */
				if ( spr_y <= y && y < spr_y + sprite_height )
				{
					INT32 spr_x =  readvmem( tms.spriteattribute + sprattr + 1 );
					UINT8 sprcode =  readvmem( tms.spriteattribute + sprattr + 2 );
					UINT8 sprcol =  readvmem( tms.spriteattribute + sprattr + 3 );
					UINT16 pataddr = tms.spritepattern + ( ( sprite_size == 16 ) ? sprcode & ~0x03 : sprcode ) * 8;

					num_sprites++;

					/* Fifth sprite encountered? */
					if ( num_sprites == 5 )
					{
						fifth_encountered = true;
						break;
					}

					if ( sprite_mag )
						pataddr += ( ( ( y - spr_y ) & 0x1F ) >> 1 );
					else
						pataddr += ( ( y - spr_y ) & 0x0F );

					UINT8 pattern =  readvmem( pataddr );

					if ( sprcol & 0x80 )
						spr_x -= 32;

					sprcol &= 0x0f;

					for ( INT32 s = 0; s < sprite_size; s += 8 )
					{
						for ( INT32 i = 0; i < 8; pattern <<= 1, i++ )
						{
							INT32 colission_index = spr_x + ( sprite_mag ? i * 2 : i ) + 32;

							for ( INT32 z = 0; z <= sprite_mag; colission_index++, z++ )
							{
								/* Check if pixel should be drawn */
								if ( pattern & 0x80 )
								{
									if ( colission_index >= 32 && colission_index < 32 + 256 )
									{
										/* Check for colission */
										if ( spr_drawn[ colission_index ] )
											tms.StatusReg |= 0x20;
										spr_drawn[ colission_index ] |= 0x01;

										if ( sprcol )
										{
											/* Has another sprite already drawn here? */
											if ( ! ( spr_drawn[ colission_index ] & 0x02 ) )
											{
												spr_drawn[ colission_index ] |= 0x02;
												p[ TMS9928A_HORZ_DISPLAY_START + colission_index - 32 ] = sprcol;
											}
										}
									}
								}
							}
						}

						pattern =  readvmem( pataddr + 16 );
						spr_x += sprite_mag ? 16 : 8;
					}
				}
			}

			/* Update sprite overflow bits */
			if (~tms.StatusReg & 0x40)
			{
				tms.StatusReg = (tms.StatusReg & 0xe0) | tms.FifthSprite;
				if (fifth_encountered && ~tms.StatusReg & 0x80)
					tms.StatusReg |= 0x40;
			}
		}

		/* Right border */
		for ( INT32 i = TMS9928A_HORZ_DISPLAY_START + 256; i < TMS9928A_TOTAL_HORZ; i++ )
			p[i] = BackColour;
	}
}

void TMS9928AScanline(INT32 vpos)
{
	if (vpos==0) // draw the border on the first scanline, border shouldn't use any cpu
	{            // to render.  this keeps cv defender's radar working.
		for (INT32 i = 0; i < tms.top_border+1; i++)
		{
			TMS9928AScanline_INT(i);
		}
	} else {
		TMS9928AScanline_INT(vpos + tms.top_border);
	}
}

INT32 TMS9928ADraw()
{
	TMS89928aPaletteRecalc();

	{
		for (INT32 y = 0; y < nScreenHeight; y++)
		{
			for (INT32 x = 0; x < nScreenWidth; x++)
			{
				pTransDraw[y * nScreenWidth + x] = tms.tmpbmp[y * TMS9928A_TOTAL_HORZ + ((TMS9928A_HORZ_DISPLAY_START/2)+10)+x];
			}
		}
	}

	BurnTransferCopy(Palette);

	return 0;
}

INT32 TMS9928AScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029708;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data   = tms.vMem;
		ba.nLen   = tms.vramsize;
		ba.szName = "video ram";
		BurnAcb(&ba);

		ba.Data	  = tms.Regs;
		ba.nLen	  = 8;
		ba.szName = "tms registers";
		BurnAcb(&ba);

		SCAN_VAR(tms.ReadAhead);
		SCAN_VAR(tms.StatusReg);
		SCAN_VAR(tms.FirstByte);
		SCAN_VAR(tms.latch);
		SCAN_VAR(tms.mode);
		SCAN_VAR(tms.INT);
		SCAN_VAR(tms.Addr);
		SCAN_VAR(tms.colour);
		SCAN_VAR(tms.pattern);
		SCAN_VAR(tms.nametbl);
		SCAN_VAR(tms.spriteattribute);
		SCAN_VAR(tms.spritepattern);
		SCAN_VAR(tms.colourmask);
		SCAN_VAR(tms.patternmask);
	}

	return 0;
}
