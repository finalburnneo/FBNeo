// Based on MESS driver by Charles MacDonald

#include "tiles_generic.h"
#include "pce.h"
#include "h6280_intf.h"

#define DINK_DEBUG 0 // debug timing / etc

UINT16 *vce_data;			// allocate externally!
static UINT16 vce_address;
static UINT16 vce_control;
static INT32 vce_clock;		// currently unused

static INT32 vce_current_line;

static INT32 main_width; // 512 (most) 320 (daimakai)

UINT8 *vdc_vidram[2];			// allocate externally!

static UINT8	vdc_register[2];
static UINT16	vdc_data[2][32];
static UINT8	vdc_latch[2];
static UINT16	vdc_yscroll[2];
static UINT16	vdc_width[2];
static UINT16	vdc_height[2];
static UINT8	vdc_inc[2];
static UINT8	vdc_dvssr_write[2];
static UINT8	vdc_status[2];
static UINT16	vdc_sprite_ram[2][0x100];

static INT32	vdc_vblank_triggered[2];
static INT32	vdc_current_segment[2];
static INT32	vdc_current_segment_line[2];
static INT32	vdc_raster_count[2];
static INT32	vdc_satb_countdown[2];

UINT16 *vdc_tmp_draw;			// allocate externally!

static UINT16 vpc_priority;
static UINT16 vpc_window1;
static UINT16 vpc_window2;
static UINT8 vpc_vdc_select;
static UINT8 vpc_prio[4];
static UINT8 vpc_vdc0_enabled[4];
static UINT8 vpc_vdc1_enabled[4];
static UINT8 vpc_prio_map[0x200];

//--------------------------------------------------------------------------------------------------------------------------------

static void vpc_update_prio_map()
{
	// scale window to playfield size
	INT32 win1_adj = vpc_window1 * vdc_width[0] / 512;
	INT32 win2_adj = vpc_window2 * vdc_width[0] / 512;

	for (INT32 i = 0; i < 512; i++)
	{
		vpc_prio_map[i] = 0;
		if (win1_adj < 0x40 || i > win1_adj) vpc_prio_map[i] |= 1;
		if (win2_adj < 0x40 || i > win2_adj) vpc_prio_map[i] |= 2;
	}
}

void vpc_write(UINT8 offset, UINT8 data)
{
#if defined FBNEO_DEBUG
	if (!DebugDev_VDCInitted) bprintf(PRINT_ERROR, _T("vpc_write called without init\n"));
#endif

	switch (offset & 0x07)
	{
		case 0x00:	/* Priority register #0 */
			vpc_priority = (vpc_priority & 0xff00) | data;
			vpc_prio[0] = ( data >> 2 ) & 3;
			vpc_vdc0_enabled[0] = data & 1;
			vpc_vdc1_enabled[0] = data & 2;
			vpc_prio[1] = ( data >> 6 ) & 3;
			vpc_vdc0_enabled[1] = data & 0x10;
			vpc_vdc1_enabled[1] = data & 0x20;
		break;

		case 0x01:	/* Priority register #1 */
			vpc_priority = (vpc_priority & 0x00ff) | (data << 8);
			vpc_prio[2] = ( data >> 2 ) & 3;
			vpc_vdc0_enabled[2] = data & 1;
			vpc_vdc1_enabled[2] = data & 2;
			vpc_prio[3] = ( data >> 6 ) & 3;
			vpc_vdc0_enabled[3] = data & 0x10;
			vpc_vdc1_enabled[3] = data & 0x20;
		break;

		case 0x02:	/* Window 1 LSB */
			vpc_window1 = (vpc_window1 & 0xff00) | data;
			vpc_update_prio_map();
		break;

		case 0x03:	/* Window 1 MSB */
			vpc_window1 = (vpc_window1 & 0x00ff) | ((data & 3) << 8);
			vpc_update_prio_map();
		break;

		case 0x04:	/* Window 2 LSB */
			vpc_window2 = (vpc_window2 & 0xff00) | data;
			vpc_update_prio_map();
		break;

		case 0x05:	/* Window 2 MSB */
			vpc_window2 = (vpc_window2 & 0x00ff) | ((data & 3) << 8);
			vpc_update_prio_map();
		break;

		case 0x06:	/* VDC I/O select */
			vpc_vdc_select = data & 1;
		break;
	}
}

UINT8 vpc_read(UINT8 offset)
{
#if defined FBNEO_DEBUG
	if (!DebugDev_VDCInitted) bprintf(PRINT_ERROR, _T("vpc_read called without init\n"));
#endif

	switch (offset & 0x07)
	{
		case 0x00:  /* Priority register #0 */
			return vpc_priority & 0xff;

		case 0x01:  /* Priority register #1 */
			return vpc_priority >> 8;

		case 0x02:  /* Window 1 LSB */
			return vpc_window1 & 0xff;

		case 0x03:  /* Window 1 MSB; high bits are 0 or 1? */
			return vpc_window1 >> 8;

		case 0x04:  /* Window 2 LSB */
			return vpc_window2 & 0xff;

		case 0x05:  /* Window 2 MSB; high bits are 0 or 1? */
			return vpc_window2 >> 8;
	}

	return 0;
}

void vpc_reset()
{
#if defined FBNEO_DEBUG
	if (!DebugDev_VDCInitted) bprintf(PRINT_ERROR, _T("vpc_reset called without init\n"));
#endif

	memset (vpc_prio, 0, sizeof(vpc_prio));
	memset (vpc_vdc0_enabled, 0, sizeof(vpc_vdc0_enabled));
	memset (vpc_vdc1_enabled, 0, sizeof(vpc_vdc1_enabled));
	memset (vpc_prio_map, 0, sizeof(vpc_prio_map));

	vpc_write(0, 0x11);
	vpc_write(1, 0x11);
	vpc_window1 = 0;
	vpc_window2 = 0;
	vpc_vdc_select = 0;
	vpc_priority = 0;
}

//--------------------------------------------------------------------------------------------------------------------------------
static INT32 ws_counter;
static void waitstate()
{
	ws_counter++;
	if (ws_counter >= 1) {
		ws_counter -= 1;
		// just a guess?
		// wonder momo titlescreen offset
		//h6280EatCycles(4);
	}
}

INT32 vce_linecount()
{
	return ((vce_control & 0x04) ? 263 : 262);
}

UINT8 vce_read(UINT8 offset)
{
#if defined FBNEO_DEBUG
	if (!DebugDev_VDCInitted) bprintf(PRINT_ERROR, _T("vce_read called without init\n"));
#endif

	switch (offset & 7)
	{
		case 0x04:
			waitstate();
			return (vce_data[vce_address] >> 0) & 0xff; //.l

		case 0x05:
			waitstate();
			UINT8 ret = ((vce_data[vce_address] >> 8) & 0xff) | 0xfe; //.h
			vce_address = (vce_address + 1) & 0x01ff;
			return ret;
	}

	return 0xff;
}

void vce_write(UINT8 offset, UINT8 data)
{
#if defined FBNEO_DEBUG
	if (!DebugDev_VDCInitted) bprintf(PRINT_ERROR, _T("vce_write called without init\n"));
#endif

	switch (offset & 7)
	{
		case 0x00:
			vce_control = data;
			switch (data & 3) {
				case 0: vce_clock = 0; break;
				case 1: vce_clock = 1; break;
				case 2:
				case 3: vce_clock = 2; break;
			}
			//bprintf(0, _T("vce_clock:  %x\tvce_control:  %x\n"), vce_clock, vce_control);
			break;

		case 0x02:
			vce_address = (vce_address & 0x100) | data;
			break;

		case 0x03:
			vce_address = (vce_address & 0x0ff) | ((data & 1) << 8);
			break;

		case 0x04:
			waitstate();
			vce_data[vce_address] = (vce_data[vce_address] & 0x100) | data;
			break;

		case 0x05:
			waitstate();
			vce_data[vce_address] = (vce_data[vce_address] & 0x0ff) | ((data & 1) << 8);
			vce_address = (vce_address + 1) & 0x01ff;
			break;
	}
}

void vce_palette_init(UINT32 *Palette)
{
#if defined FBNEO_DEBUG
	if (!DebugDev_VDCInitted) bprintf(PRINT_ERROR, _T("vce_palette_init called without init\n"));
#endif

	for (INT32 i = 0; i < 512; i++)
	{
		INT32 r = ((i >> 3) & 7) << 5;
		INT32 g = ((i >> 6) & 7) << 5;
		INT32 b = ((i >> 0) & 7) << 5;

		INT32 y = ((66 * r + 129 * g +  25 * b + 128) >> 8) +  16;

		Palette[0x000 + i] = BurnHighCol(r, g, b, 0);
		Palette[0x200 + i] = BurnHighCol(y, y, y, 0);
	}
}

void vce_reset()
{
#if defined FBNEO_DEBUG
	if (!DebugDev_VDCInitted) bprintf(PRINT_ERROR, _T("vce_reset called without init\n"));
#endif

	memset (vce_data, 0, 512 * sizeof(UINT16));

	vce_address = 0;
	vce_control = 0;

	vce_current_line = 0;
}


//--------------------------------------------------------------------------------------------------------------------------------

#define STATE_VSW		0
#define STATE_VDS		1
#define STATE_VDW		2
#define STATE_VCR		3

#define VDC_WPF			684		/* width of a line in frame including blanking areas */
#define VDC_LPF		 262	 /* number of lines in a single frame */

#define VDC_BSY		 0x40	/* Set when the VDC accesses VRAM */
#define VDC_VD		  0x20	/* Set when in the vertical blanking period */
#define VDC_DV		  0x10	/* Set when a VRAM > VRAM DMA transfer is done */
#define VDC_DS		  0x08	/* Set when a VRAM > SATB DMA transfer is done */
#define VDC_RR		  0x04	/* Set when the current scanline equals the RCR register */
#define VDC_OR		  0x02	/* Set when there are more than 16 sprites on a line */
#define VDC_CR		  0x01	/* Set when sprite #0 overlaps with another sprite */

#define CR_BB		   0x80	/* Background blanking */
#define CR_SB		   0x40	/* Object blanking */
#define CR_VR		   0x08	/* Interrupt on vertical blank enable */
#define CR_RC		   0x04	/* Interrupt on line compare enable */
#define CR_OV		   0x02	/* Interrupt on sprite overflow enable */
#define CR_CC		   0x01	/* Interrupt on sprite #0 collision enable */

#define DCR_DSR		 0x10	/* VRAM > SATB auto-transfer enable */
#define DCR_DID		 0x08	/* Destination diretion */
#define DCR_SID		 0x04	/* Source direction */
#define DCR_DVC		 0x02	/* VRAM > VRAM EOT interrupt enable */
#define DCR_DSC		 0x01	/* VRAM > SATB EOT interrupt enable */

//  		          0     1    2     3     4   5    6    7    8    9    a    b    c    d    e    f    10    11    12     13
enum vdc_regs {MAWR = 0, MARR, VxR, reg3, reg4, CR, RCR, BXR, BYR, MWR, HSR, HDR, VPR, VDW, VCR, DCR, SOUR, DESR, LENR, DVSSR };


static void conv_obj(INT32 which, INT32 i, INT32 l, INT32 hf, INT32 vf, UINT8 *buf)
{
	INT32 b0, b1, b2, b3, i0, i1, i2, i3, x;
	INT32 xi;
	INT32 tmp;

	l &= 0x0F;
	if(vf) l = (15 - l);

	tmp = l + ( i << 5);

	b0  = vdc_vidram[which][(tmp + 0x00) * 2 + 0];
	b0 |= vdc_vidram[which][(tmp + 0x00) * 2 + 1]<<8;
	b1  = vdc_vidram[which][(tmp + 0x10) * 2 + 0];
	b1 |= vdc_vidram[which][(tmp + 0x10) * 2 + 1]<<8;
	b2  = vdc_vidram[which][(tmp + 0x20) * 2 + 0];
	b2 |= vdc_vidram[which][(tmp + 0x20) * 2 + 1]<<8;
	b3  = vdc_vidram[which][(tmp + 0x30) * 2 + 0];
	b3 |= vdc_vidram[which][(tmp + 0x30) * 2 + 1]<<8;

	for(x=0;x<16;x++)
	{
		if(hf) xi = x; else xi = (15 - x);
		i0 = (b0 >> xi) & 1;
		i1 = (b1 >> xi) & 1;
		i2 = (b2 >> xi) & 1;
		i3 = (b3 >> xi) & 1;
		buf[x] = (i3 << 3 | i2 << 2 | i1 << 1 | i0);
	}
}

static void pce_refresh_sprites(INT32 which, INT32 line, UINT8 *drawn, UINT16 *line_buffer)
{
	INT32 i;
	UINT8 sprites_drawn = 0;

	/* Are we in greyscale mode or in color mode? */
	INT32 color_base = vce_control & 0x80 ? 512 : 0;

	/* count up: Highest priority is Sprite 0 */
	for(i = 0; i < 64; i++)
	{
		static const INT32 cgy_table[] = {16, 32, 64, 64};

		INT32 obj_y = (vdc_sprite_ram[which][(i << 2) + 0] & 0x03FF) - 64;
		INT32 obj_x = (vdc_sprite_ram[which][(i << 2) + 1] & 0x03FF) - 32;
		INT32 obj_i = (vdc_sprite_ram[which][(i << 2) + 2] & 0x07FE);
		INT32 obj_a = (vdc_sprite_ram[which][(i << 2) + 3]);
		INT32 cgx   = (obj_a >> 8) & 1;   /* sprite width */
		INT32 cgy   = (obj_a >> 12) & 3;  /* sprite height */
		INT32 hf	= (obj_a >> 11) & 1;  /* horizontal flip */
		INT32 vf	= (obj_a >> 15) & 1;  /* vertical flip */
		INT32 palette = (obj_a & 0x000F);
		INT32 priority = (obj_a >> 7) & 1;
		INT32 obj_h = cgy_table[cgy];
		INT32 obj_l = (line - obj_y);
		INT32 cgypos;
		UINT8 buf[16];

		if ((obj_y == -64) || (obj_y > line)) continue;
		if ((obj_x == -32) || (obj_x >= vdc_width[which])) continue;

		/* no need to draw an object that's ABOVE where we are. */
		if((obj_y + obj_h) < line) continue;

		/* If CGX is set, bit 0 of sprite pattern index is forced to 0 */
		if ( cgx )
			obj_i &= ~2;

		/* If CGY is set to 1, bit 1 of the sprite pattern index is forced to 0. */
		if ( cgy & 1 )
			obj_i &= ~4;

		/* If CGY is set to 2 or 3, bit 1 and 2 of the sprite pattern index are forced to 0. */
		if ( cgy & 2 )
			obj_i &= ~12;

		if (obj_l < obj_h)
		{
			sprites_drawn++;
			if(sprites_drawn > 16)
			{
				if(vdc_data[which][0x05] & 0x02)
				{
					/* note: flag is set only if irq is taken, Mizubaku Daibouken relies on this behaviour */
					vdc_status[which] |= 0x02;
					h6280SetIRQLine(0, CPU_IRQSTATUS_ACK);
#if DINK_DEBUG
					bprintf(0, _T("SPR-Overflow  line %d\n"), vce_current_line);
#endif
				}
				if (~PCEDips[2] & 0x10) // check sprite limit enforcement
					continue;
			}

			cgypos = (obj_l >> 4);
			if(vf) cgypos = ((obj_h - 1) >> 4) - cgypos;

			if(cgx == 0)
			{
				INT32 x;
				INT32 pixel_x = ( ( obj_x * main_width ) / vdc_width[which] );

				conv_obj(which, obj_i + (cgypos << 2), obj_l, hf, vf, buf);

				for(x = 0; x < 16; x++)
				{
					if(((obj_x + x) < (vdc_width[which])) && ((obj_x + x) >= 0))
					{
						if ( buf[x] )
						{
							if( drawn[pixel_x] < 2 )
							{
								if( priority || drawn[pixel_x] == 0 )
								{
									line_buffer[pixel_x] = color_base + vce_data[0x100 + (palette << 4) + buf[x]];

									if ( vdc_width[which] != 512 )
									{
										INT32 dp = 1;
										while ( pixel_x + dp < ( ( ( obj_x + x + 1 ) * main_width ) / vdc_width[which] ) )
										{
											drawn[pixel_x + dp] = i + 2;
											line_buffer[pixel_x + dp] = color_base + vce_data[0x100 + (palette << 4) + buf[x]];
											dp++;
										}
									}
								}
								drawn[pixel_x] = i + 2;
							}
							/* Check for sprite #0 collision */
							else if (drawn[pixel_x] == 2)
							{
								if(vdc_data[which][0x05] & 0x01) {
									h6280SetIRQLine(0, CPU_IRQSTATUS_ACK);
#if DINK_DEBUG
									bprintf(0, _T("SPR-Collision0  line %d\n"), vce_current_line);
#endif
								}
								vdc_status[which] |= 0x01;
							}
						}
					}
					if ( vdc_width[which] != 512 )
					{
						pixel_x = ( ( obj_x + x + 1 ) * main_width ) / vdc_width[which];
					}
					else
					{
						pixel_x += 1;
					}
				}
			}
			else
			{
				INT32 x;
				INT32 pixel_x = ( ( obj_x * main_width ) / vdc_width[which] );

				conv_obj(which, obj_i + (cgypos << 2) + (hf ? 2 : 0), obj_l, hf, vf, buf);

				for(x = 0; x < 16; x++)
				{
					if(((obj_x + x) < (vdc_width[which])) && ((obj_x + x) >= 0))
					{
						if ( buf[x] )
						{
							if( drawn[pixel_x] < 2 )
							{
								if ( priority || drawn[pixel_x] == 0 )
								{
									line_buffer[pixel_x] = color_base + vce_data[0x100 + (palette << 4) + buf[x]];
									if ( vdc_width[which] != 512 )
									{
										INT32 dp = 1;
										while ( pixel_x + dp < ( ( ( obj_x + x + 1 ) * main_width ) / vdc_width[which] ) )
										{
											drawn[pixel_x + dp] = i + 2;
											line_buffer[pixel_x + dp] = color_base + vce_data[0x100 + (palette << 4) + buf[x]];
											dp++;
										}
									}
								}
								drawn[pixel_x] = i + 2;
							}
							/* Check for sprite #0 collision */
							else if ( drawn[pixel_x] == 2 )
							{
								if(vdc_data[which][0x05] & 0x01) {
									h6280SetIRQLine(0, CPU_IRQSTATUS_ACK);
#if DINK_DEBUG
									bprintf(0, _T("SPR-Collision1  line %d\n"), vce_current_line);
#endif
								}
								vdc_status[which] |= 0x01;
							}
						}
					}
					if ( vdc_width[which] != 512 )
					{
						pixel_x = ( ( obj_x + x + 1 ) * main_width ) / vdc_width[which];
					}
					else
					{
						pixel_x += 1;
					}
				}

				/* 32 pixel wide sprites are counted as 2 sprites and the right half
				   		is only drawn if there are 2 open slots.
						*/
				sprites_drawn++;
				if( sprites_drawn > 16 )
				{
					if(vdc_data[which][0x05] & 0x02)
					{
						/* note: flag is set only if irq is taken, Mizubaku Daibouken relies on this behaviour */
						vdc_status[which] |= 0x02;
						h6280SetIRQLine(0, CPU_IRQSTATUS_ACK);
#if DINK_DEBUG
						bprintf(0, _T("SPR-Overflow  line %d\n"), vce_current_line);
#endif
					}
					if (~PCEDips[2] & 0x10) // check sprite limit enforcement
						continue;
				}
				
				{
					conv_obj(which, obj_i + (cgypos << 2) + (hf ? 0 : 2), obj_l, hf, vf, buf);
					for(x = 0; x < 16; x++)
					{
						if(((obj_x + 0x10 + x) < (vdc_width[which])) && ((obj_x + 0x10 + x) >= 0))
						{
							if ( buf[x] )
							{
								if( drawn[pixel_x] < 2 )
								{
									if( priority || drawn[pixel_x] == 0 )
									{
										line_buffer[pixel_x] = color_base + vce_data[0x100 + (palette << 4) + buf[x]];
										if ( vdc_width[which] != 512 )
										{
											INT32 dp = 1;
											while ( pixel_x + dp < ( ( ( obj_x + x + 17 ) * main_width ) / vdc_width[which] ) )
											{
												drawn[pixel_x + dp] = i + 2;
												line_buffer[pixel_x + dp] = color_base + vce_data[0x100 + (palette << 4) + buf[x]];
												dp++;
											}
										}
									}
									drawn[pixel_x] = i + 2;
								}
								/* Check for sprite #0 collision */
								else if ( drawn[pixel_x] == 2 )
								{
									if(vdc_data[which][0x05] & 0x01) {
										h6280SetIRQLine(0, CPU_IRQSTATUS_ACK);
#if DINK_DEBUG
										bprintf(0, _T("SPR-Collision2  line %d\n"), vce_current_line);
#endif
									}
									vdc_status[which] |= 0x01;
								}
							}
						}
						if ( vdc_width[which] != 512 )
						{
							pixel_x = ( ( obj_x + x + 17 ) * main_width ) / vdc_width[which];
						}
						else
						{
							pixel_x += 1;
						}
					}
				}
			}
		}
	}
}


static void clear_line(INT32 line)
{
	/* Are we in greyscale mode or in color mode? */
	INT32 color_base = vce_control & 0x80 ? 512 : 0;

	/* our line buffer */
	UINT16 *line_buffer = vdc_tmp_draw + line * 684;

	for (INT32 i = 0; i < 684; i++)
		line_buffer[i] = color_base + vce_data[0x100];
}

static void clear_line_sgx(INT32 line)
{
	/* Are we in greyscale mode or in color mode? */
	INT32 color_base = vce_control & 0x80 ? 512 : 0;

	/* our line buffer */
	UINT16 *line_buffer = vdc_tmp_draw + line * 684;

	for (INT32 i = 0; i < VDC_WPF; i++)
		line_buffer[i] = color_base + vce_data[0];
}

void vdc_check_hblank_raster_irq(INT32 which)
{
	/* generate interrupt on line compare if necessary */
	if ( vdc_raster_count[which] == (vdc_data[which][RCR] & 0x3ff) && vdc_data[which][CR] & CR_RC )
	{
#if DINK_DEBUG
		bprintf(0, _T("raster @ %d\tscanline %d\n"),vdc_raster_count[which], vce_current_line);
#endif
		vdc_status[which] |= VDC_RR;
		h6280SetIRQLine(0, CPU_IRQSTATUS_ACK);
	}

}

static void do_vblank(INT32 which)
{
	if (vdc_vblank_triggered[which] == 1) return; // once per frame

	/* Generate VBlank interrupt */
	vdc_vblank_triggered[which] = 1;

	if ( vdc_data[which][CR] & CR_VR )
	{
#if DINK_DEBUG
		bprintf(0, _T("vbl @ scanline %d\n"), vce_current_line);
#endif
		h6280Run(30/3); // 30 m-cycles past start of line
		vdc_status[which] |= VDC_VD;
		h6280Run(2); // give cpu a chance to recognize status
		h6280SetIRQLine(0, CPU_IRQSTATUS_ACK);
	}

	/* do VRAM > SATB DMA if the enable bit is set or the DVSSR reg. was written to */
	if( ( vdc_data[which][DCR] & DCR_DSR ) || vdc_dvssr_write[which] )
	{
		vdc_dvssr_write[which] = 0;

		for(INT32 i = 0; i < 256; i++ )
		{
			vdc_sprite_ram[which][i] = ( vdc_vidram[which][ ( vdc_data[which][DVSSR] << 1 ) + i * 2 + 1 ] << 8 ) | vdc_vidram[which][ ( vdc_data[which][DVSSR] << 1 ) + i * 2 ];
		}

		/* generate interrupt if needed */
		if ( vdc_data[which][DCR] & DCR_DSC )
		{
			vdc_satb_countdown[which] = 4;
		}
	}
}


static void vdc_advance_line(INT32 which)
{
	if (which == 0) // increment master scanline
		vce_current_line = (vce_current_line + 1) % vce_linecount();

	vdc_current_segment_line[which] += 1;
	vdc_raster_count[which] += 1;

	if ( vdc_satb_countdown[which] )
	{
		vdc_satb_countdown[which] -= 1;
		if ( vdc_satb_countdown[which] == 0 )
		{
			if ( vdc_data[which][DCR] & DCR_DSC )
			{
				vdc_status[which] |= VDC_DS;	/* set satb done flag */
				h6280SetIRQLine(0, CPU_IRQSTATUS_ACK);
			}
		}
	}

	if ( vce_current_line == 0 )
	{
		vdc_current_segment[which] = STATE_VSW;
		vdc_current_segment_line[which] = 0;
		vdc_vblank_triggered[which] = 0;
		if (vce_current_line != 0) {
			bprintf(0, _T("vdc_advance_line(): vce_current_line == %d - desynch.\n"), vce_current_line);
		}
#if DINK_DEBUG
		bprintf(0, _T("vsw @ %d\tvds @ %d\n"), ((vdc_data[which][VPR]&0xff) & 0x1F), (vdc_data[which][VPR] >> 8));
		bprintf(0, _T("vdw length is %d\n"), ( vdc_data[which][VDW] & 0x01FF ));
#endif
	}

	if ( STATE_VSW == vdc_current_segment[which] && vdc_current_segment_line[which] == (vdc_data[which][VPR] & 0x1f) + 1 )
	{
		vdc_current_segment[which] = STATE_VDS;
		vdc_current_segment_line[which] = 0;
	}

	if ( STATE_VDS == vdc_current_segment[which] && vdc_current_segment_line[which] == (vdc_data[which][VPR] >> 8) + 2 )
	{
		vdc_current_segment[which] = STATE_VDW;
		vdc_current_segment_line[which] = 0;
		vdc_raster_count[which] = 0x40;
		//bprintf(0, _T("VDW_start(vdw:%x,bmp:%x)"), vdc_data[which][VDW] & 0x01FF,vce_current_line);
	}

	if ( STATE_VDW == vdc_current_segment[which] && vdc_current_segment_line[which] == (vdc_data[which][VDW] & 0x01ff) + 1 )
	{
		vdc_current_segment[which] = STATE_VCR;
		vdc_current_segment_line[which] = 0;

		do_vblank(which);
#if DINK_DEBUG
		bprintf(0, _T("end vdw vblank, line %d, vdc_vblank_triggered[which] == %x\n"),vce_current_line, vdc_vblank_triggered[which]);
#endif
	}

	if ( STATE_VCR == vdc_current_segment[which] && vdc_current_segment_line[which] > (vdc_data[which][VCR]&0xff) ) {
		// this state often gets skipped due to being moved past line 262/263
		//bprintf(0, _T("end VCR @ line %d\n"), vce_current_line);
		vdc_current_segment[which] = STATE_VSW;
		vdc_current_segment_line[which] = 0;
	}

	if ( vce_current_line == vce_linecount() - 1 && !vdc_vblank_triggered[which] )
	{
#if DINK_DEBUG
		bprintf(0, _T("forced vblank :: line %d, vdc_vblank_triggered[which] == %x\n"),vce_current_line, vdc_vblank_triggered[which]);
#endif
		// it's possible that a game set a large vertical linecount which makes
		// getting past the VDW state impossible (final blaster, intro).  We still need
		// to generate vblank, though.  -dink

		do_vblank(which);
	}
}

static void pce_refresh_line(INT32 which, INT32 /*line*/, INT32 external_input, UINT8 *drawn, UINT16 *line_buffer)
{
	static const INT32 width_table[4] = {5, 6, 7, 7};

	INT32 scroll_y = ( vdc_yscroll[which] & 0x01FF);
	INT32 scroll_x = (vdc_data[which][BXR] & 0x03FF);
	INT32 nt_index;

	/* is virtual map 32 or 64 characters tall ? (256 or 512 pixels) */
	INT32 v_line = (scroll_y) & (vdc_data[which][MWR] & 0x0040 ? 0x1FF : 0x0FF);

#if DINK_DEBUG
	if (vdc_current_segment_line[which] > 235) {
		bprintf(0, _T("pce line:  %d\tBYR %x\tscrolly  %d\tv_line  %d\n"),vdc_current_segment_line[which],vdc_data[which][BYR], vdc_yscroll[which], v_line);
	}
#endif

	/* row within character */
	INT32 v_row = (v_line & 7);

	/* row of characters in BAT */
	INT32 nt_row = (v_line >> 3);

	/* virtual X size (# bits to shift) */
	INT32 v_width =		width_table[(vdc_data[which][MWR] >> 4) & 3];

	/* pointer to the name table (Background Attribute Table) in VRAM */
	UINT8 *bat = &(vdc_vidram[which][nt_row << (v_width+1)]);

	/* Are we in greyscale mode or in color mode? */
	INT32 color_base = vce_control & 0x80 ? 512 : 0;

	INT32 b0, b1, b2, b3;
	INT32 i0, i1, i2, i3;
	INT32 cell_pattern_index;
	INT32 cell_palette;
	INT32 x, c, i;

	/* character blanking bit */
	if(!(vdc_data[which][CR] & CR_BB))
	{
		return;
	}
	else
	{
		INT32	pixel = 0;
		INT32 phys_x = - ( scroll_x & 0x07 );

		for(i=0;i<(vdc_width[which] >> 3) + 1;i++)
		{
			nt_index = (i + (scroll_x >> 3)) & ((2 << (v_width-1))-1);
			nt_index *= 2;

			/* get name table data: */

			/* palette # = index from 0-15 */
			cell_palette = ( bat[nt_index + 1] >> 4 ) & 0x0F;

			/* This is the 'character number', from 0-0x0FFF		 */
			/* then it is shifted left 4 bits to form a VRAM address */
			/* and one more bit to convert VRAM word offset to a	 */
			/* byte-offset within the VRAM space					 */
			cell_pattern_index = ( ( ( bat[nt_index + 1] << 8 ) | bat[nt_index] ) & 0x0FFF) << 5;

			INT32 vram_offs = (cell_pattern_index + (v_row << 1)) & 0xffff;

			b0 = vdc_vidram[which][vram_offs + 0x00];
			b1 = vdc_vidram[which][vram_offs + 0x01];
			b2 = vdc_vidram[which][vram_offs + 0x10];
			b3 = vdc_vidram[which][vram_offs + 0x11];

			for(x=0;x<8;x++)
			{
				i0 = (b0 >> (7-x)) & 1;
				i1 = (b1 >> (7-x)) & 1;
				i2 = (b2 >> (7-x)) & 1;
				i3 = (b3 >> (7-x)) & 1;
				c = (cell_palette << 4 | i3 << 3 | i2 << 2 | i1 << 1 | i0);

				/* colour #0 always comes from palette #0 */
				if ( ! ( c & 0x0F ) )
					c &= 0x0F;

				if ( phys_x >= 0 && phys_x < vdc_width[which] )
				{
					drawn[ pixel ] = c ? 1 : 0;
					if ( c || ! external_input )
						line_buffer[ pixel ] = color_base + vce_data[c];
					pixel++;
					if ( vdc_width[which] != 512 )
					{
						while ( pixel < ( ( ( phys_x + 1 ) * main_width ) / vdc_width[which] ) )
						{
							drawn[ pixel ] = c ? 1 : 0;
							if ( c || ! external_input )
								line_buffer[ pixel ] = color_base + vce_data[c];
							pixel++;
						}
					}
				}
				phys_x += 1;
			}
		}
	}
}

void pce_hblank()
{
	vdc_check_hblank_raster_irq(0);

	INT32 which = 0; // only 1 on pce

	if (vdc_current_segment[which] == STATE_VDW && vdc_current_segment_line[0] < 262 )
	{
		UINT8 drawn[684];
		UINT16 *line_buffer = vdc_tmp_draw + (vdc_current_segment_line[which] * 684) + 86;
		clear_line(vdc_current_segment_line[which]);

		memset (drawn, 0, 684);

		vdc_yscroll[which] = (vdc_current_segment_line[which] == 0) ? vdc_data[which][BYR] : (vdc_yscroll[which] + 1);

		if (nBurnLayer & 1)
			pce_refresh_line(0, vdc_current_segment_line[which], 0, drawn, line_buffer);

		if (vdc_data[which][CR] & CR_SB)
		{
			if (nSpriteEnable & 1)
				pce_refresh_sprites(0, vdc_current_segment_line[which], drawn, line_buffer);
		}
	}
}

void sgx_hblank()
{
	vdc_check_hblank_raster_irq(0);
	vdc_check_hblank_raster_irq(1);

	if ( vdc_current_segment[0] == STATE_VDW && vdc_current_segment_line[0] < 262 )
	{
		UINT8 drawn[2][512*2];
		UINT16 *line_buffer;
		UINT16 temp_buffer[2][512*2];
		INT32 i;

		clear_line_sgx(vdc_current_segment_line[0]); // clear line

		memset( drawn, 0, sizeof(drawn) );
		memset( temp_buffer, 0, sizeof(temp_buffer) );

		vdc_yscroll[0] = ( vdc_current_segment_line[0] == 0 ) ? vdc_data[0][BYR] : ( vdc_yscroll[0] + 1 );
		vdc_yscroll[1] = ( vdc_current_segment_line[1] == 0 ) ? vdc_data[1][BYR] : ( vdc_yscroll[1] + 1 );

		if (nBurnLayer & 1)
			pce_refresh_line( 0, vdc_current_segment_line[0], 0, drawn[0], temp_buffer[0]);

		if(vdc_data[0][CR] & CR_SB)
		{
			if (nSpriteEnable & 1)
				pce_refresh_sprites(0, vdc_current_segment_line[0], drawn[0], temp_buffer[0]);
		}

		if (nBurnLayer & 2)
			pce_refresh_line( 1, vdc_current_segment_line[1], 1, drawn[1], temp_buffer[1]);

		if ( vdc_data[1][CR] & CR_SB )
		{
			if (nSpriteEnable & 2)
				pce_refresh_sprites(1, vdc_current_segment_line[1], drawn[1], temp_buffer[1]);
		}

		line_buffer = vdc_tmp_draw + (vdc_current_segment_line[0] * 684) + 86;

		for( i = 0; i < 512; i++ )
		{
			INT32 cur_prio = vpc_prio_map[i];

			if ( vpc_vdc0_enabled[cur_prio] )
			{
				if ( vpc_vdc1_enabled[cur_prio] )
				{
					switch( vpc_prio[cur_prio] )
					{
						case 0:	/* BG1 SP1 BG0 SP0 */
							if ( drawn[0][i] )
							{
								line_buffer[i] = temp_buffer[0][i];
							}
							else if ( drawn[1][i] )
							{
								line_buffer[i] = temp_buffer[1][i];
							}
							break;
						case 1:	/* BG1 BG0 SP1 SP0 */
							if ( drawn[0][i] )
							{
								if ( drawn[0][i] > 1 )
								{
									line_buffer[i] = temp_buffer[0][i];
								}
								else
								{
									if ( drawn[1][i] > 1 )
									{
										line_buffer[i] = temp_buffer[1][i];
									}
									else
									{
										line_buffer[i] = temp_buffer[0][i];
									}
								}
							}
							else if ( drawn[1][i] )
							{
								line_buffer[i] = temp_buffer[1][i];
							}
							break;
						case 2:
							if ( drawn[0][i] )
							{
								if ( drawn[0][i] > 1 )
								{
									if ( drawn[1][i] == 1 )
									{
										line_buffer[i] = temp_buffer[1][i];
									}
									else
									{
										line_buffer[i] = temp_buffer[0][i];
									}
								}
								else
								{
									line_buffer[i] = temp_buffer[0][i];
								}
							}
							else if ( drawn[1][i] )
							{
								line_buffer[i] = temp_buffer[1][i];
							}
							break;
					}
				}
				else
				{
					if ( drawn[0][i] )
					{
						line_buffer[i] = temp_buffer[0][i];
					}
				}
			}
			else
			{
				if ( vpc_vdc1_enabled[cur_prio] )
				{
					if ( drawn[1][i] )
					{
						line_buffer[i] = temp_buffer[1][i];
					}
				}
			}
		}
	}
}

void pce_interrupt()
{
#if defined FBNEO_DEBUG
	if (!DebugDev_VDCInitted) bprintf(PRINT_ERROR, _T("pce_interrupt called without init\n"));
#endif

	vdc_advance_line(0);
}

void sgx_interrupt()
{
#if defined FBNEO_DEBUG
	if (!DebugDev_VDCInitted) bprintf(PRINT_ERROR, _T("sgx_interrupt called without init\n"));
#endif

	vdc_advance_line( 0 );
	vdc_advance_line( 1 );
}

static void vdc_do_dma(INT32 which)
{
	INT32 src = vdc_data[which][0x10];
	INT32 dst = vdc_data[which][0x11];
	INT32 len = vdc_data[which][0x12];

	INT32 did = (vdc_data[which][0x0f] >> 3) & 1;
	INT32 sid = (vdc_data[which][0x0f] >> 2) & 1;
	INT32 dvc = (vdc_data[which][0x0f] >> 1) & 1;

	do {
		UINT8 l, h;

		l = vdc_vidram[which][((src * 2) + 0) & 0xffff];
		h = vdc_vidram[which][((src * 2) + 1) & 0xffff];

		if ((dst & 0x8000) == 0) {
			vdc_vidram[which][(dst * 2) + 0] = l;
			vdc_vidram[which][(dst * 2) + 1] = h;
		}

		if(sid) src = (src - 1) & 0xffff;
		else	src = (src + 1) & 0xffff;

		if(did) dst = (dst - 1) & 0xffff;
		else	dst = (dst + 1) & 0xffff;

		len = (len - 1) & 0xffff;

	} while (len != 0xffff);

	vdc_status[which] |= 0x10;
	vdc_data[which][0x10] = src;
	vdc_data[which][0x11] = dst;
	vdc_data[which][0x12] = len;

	if (dvc)
	{
		h6280SetIRQLine(0, CPU_IRQSTATUS_ACK);
	}
}

void vdc_write(INT32 which, UINT8 offset, UINT8 data)
{
#if defined FBNEO_DEBUG
	if (!DebugDev_VDCInitted) bprintf(PRINT_ERROR, _T("vdc_write called without init\n"));
#endif

	switch (offset & 3)
	{
		case 0x00:
			vdc_register[which] = data & 0x1f;
		break;

		case 0x02:
		{
			vdc_data[which][vdc_register[which]] = (vdc_data[which][vdc_register[which]] & 0xff00) | data;

			switch (vdc_register[which])
			{
				case VxR:{
					vdc_latch[which] = data;
				}
				break;

				case BYR: {
					vdc_yscroll[which]=vdc_data[which][BYR];
					//bprintf(0, _T("vdc BYR  %x  @  line  %d\n"), vdc_yscroll[which], vce_current_line);
				}
				break;

				case HDR: {
					vdc_width[which] = ((data & 0x003F) + 1) << 3;
					bprintf(0, _T("vdc width  %d\n"), vdc_width[which]);
				}
				break;

				case VDW: {
					vdc_height[which] &= 0xFF00;
					vdc_height[which] |= (data & 0xFF);
					vdc_height[which] &= 0x01FF;
				}
				break;

				case LENR:
				case SOUR:
				case DESR:
				break;
			}
		}
		break;

		case 0x03:
		{
			vdc_data[which][vdc_register[which]] = (vdc_data[which][vdc_register[which]] & 0x00ff) | (data << 8);

			switch (vdc_register[which])
			{
				case VxR:
				{
					waitstate();
					INT32 voff = vdc_data[which][MAWR] * 2;
					if ((voff & 0x10000) == 0) {
						vdc_vidram[which][voff + 0] = vdc_latch[which];
						vdc_vidram[which][voff + 1] = data;
					}
					vdc_data[which][MAWR] += vdc_inc[which];
				}
				break;

				case CR:
				{
					static const UINT8 inctab[] = {1, 32, 64, 128};
					vdc_inc[which] = inctab[(data >> 3) & 3];
				}
				break;

				case VDW:
				{
					vdc_height[which] &= 0x00FF;
					vdc_height[which] |= (data << 8);
					vdc_height[which] &= 0x01FF;
				}
				break;

				case DVSSR:
					vdc_dvssr_write[which] = 1;
					break;

				case BYR:
					vdc_yscroll[which]=vdc_data[which][BYR];
					break;

				case LENR:
					//bprintf(0, _T("DO DMA @ line  %d\n"), vce_current_line);
					vdc_do_dma( which );
					break;

				case SOUR:
				case DESR:
				break;
			}
		}
		break;
	}
#if 0
	// save for later debugging -dink
	if (vdc_register[which] == RCR && (offset&3)==3) {
		//bprintf(0, _T("RCR  %d\n"), vdc_data[which][vdc_register[which]]);
	}
	if (vdc_register[which] == CR) {// && (offset&3)==3) {
		//bprintf(0, _T("------CR  %x\n"), vdc_data[which][vdc_register[which]]);
	}

	if (vdc_register[which] == VDW) {// && (offset&3)==3) {
		//bprintf(0, _T("------VDW  %x\n"), vdc_data[which][vdc_register[which]]);
	}
#endif
}

UINT8 vdc_read(INT32 which, UINT8 offset)
{
#if defined FBNEO_DEBUG
	if (!DebugDev_VDCInitted) bprintf(PRINT_ERROR, _T("vdc_read called without init\n"));
#endif

	switch(offset & 3)
	{
		case 0x00: {
			UINT8 ret = vdc_status[which];
			vdc_status[which] &= ~0x3f;
			h6280SetIRQLine(0, CPU_IRQSTATUS_NONE);
			return ret;
		}

		case 0x02: {
			waitstate();
			INT32 voff = (vdc_data[which][1] * 2 + 0) & 0xffff;
			return vdc_vidram[which][voff];
		}

		case 0x03: {
			waitstate();
			INT32 voff = (vdc_data[which][1] * 2 + 1) & 0xffff;
			if (vdc_register[which] == 0x02) vdc_data[which][1] += vdc_inc[which];
			return vdc_vidram[which][voff];
		}
	}

	return 0;
}

void sgx_vdc_write(UINT8 offset, UINT8 data)
{
#if defined FBNEO_DEBUG
	if (!DebugDev_VDCInitted) bprintf(PRINT_ERROR, _T("sgx_vdc_write called without init\n"));
#endif

	if (vpc_vdc_select)
	{
		vdc_write( 1, offset, data );
	}
	else
	{
		vdc_write( 0, offset, data );
	}
}

UINT8 sgx_vdc_read(UINT8 offset)
{
#if defined FBNEO_DEBUG
	if (!DebugDev_VDCInitted) bprintf(PRINT_ERROR, _T("sgx_vdc_read called without init\n"));
#endif

	return (vpc_vdc_select) ? vdc_read( 1, offset ) : vdc_read( 0, offset );
}

void vdc_reset()
{
#if defined FBNEO_DEBUG
	if (!DebugDev_VDCInitted) bprintf(PRINT_ERROR, _T("vdc_reset called without init\n"));
#endif

	memset (vdc_register,				0, sizeof(vdc_register));
	memset (vdc_data,					0, sizeof(vdc_data));
	memset (vdc_latch,					0, sizeof(vdc_latch));
	memset (vdc_yscroll,				0, sizeof(vdc_yscroll));
	memset (vdc_width,					0, sizeof(vdc_width));
	memset (vdc_height,					0, sizeof(vdc_height));
	memset (vdc_inc,					0, sizeof(vdc_inc));
	memset (vdc_dvssr_write,			0, sizeof(vdc_dvssr_write));
	memset (vdc_status,					0, sizeof(vdc_status));
	memset (vdc_sprite_ram,				0, sizeof(vdc_sprite_ram));
	memset (vdc_vblank_triggered,		0, sizeof(vdc_vblank_triggered));
	memset (vdc_current_segment,		0, sizeof(vdc_current_segment));
	memset (vdc_current_segment_line,	0, sizeof(vdc_current_segment_line));
	memset (vdc_raster_count,			0, sizeof(vdc_raster_count));
	memset (vdc_satb_countdown,			0, sizeof(vdc_satb_countdown));

	for (INT32 chip = 0; chip < 2; chip++) {
		vdc_data[chip][MWR] = 0x0010;
		vdc_data[chip][HSR] = 0x0202;
		vdc_data[chip][HDR] = 0x031f;
		vdc_data[chip][VPR] = 0x0f02;
		vdc_data[chip][VDW] = 0x00ef;
		vdc_data[chip][VCR] = 0x0003;

		vdc_inc[chip] = 1;
		vdc_raster_count[chip] = 0x4000;
	}

	main_width = nScreenWidth;
}

void vdc_get_dimensions(INT32 which, INT32 *x, INT32 *y)
{
#if defined FBNEO_DEBUG
	if (!DebugDev_VDCInitted) bprintf(PRINT_ERROR, _T("vdc_get_dimensions called without init\n"));
#endif

	*x = vdc_width[which] * 2;
	*y = vdc_height[which];
}

void vdc_init()
{
	DebugDev_VDCInitted = 1;
}

void vdc_exit()
{
#if defined FBNEO_DEBUG
	if (!DebugDev_VDCInitted) bprintf(PRINT_ERROR, _T("vdc_exit called without init\n"));
#endif

	DebugDev_VDCInitted = 0;
}

INT32 vdc_scan(INT32 nAction, INT32 *pnMin)
{
#if defined FBNEO_DEBUG
	if (!DebugDev_VDCInitted) bprintf(PRINT_ERROR, _T("vdc_scan called without init\n"));
#endif

	if (pnMin) {
		*pnMin =  0x029702;
	}

	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(vdc_register);
		SCAN_VAR(vdc_data);
		SCAN_VAR(vdc_latch);
		SCAN_VAR(vdc_yscroll);
		SCAN_VAR(vdc_width);
		SCAN_VAR(vdc_height);
		SCAN_VAR(vdc_inc);
		SCAN_VAR(vdc_dvssr_write);
		SCAN_VAR(vdc_status);
		SCAN_VAR(vdc_sprite_ram);

		SCAN_VAR(vdc_vblank_triggered);
		SCAN_VAR(vdc_current_segment);
		SCAN_VAR(vdc_current_segment_line);

		SCAN_VAR(vdc_raster_count);
		SCAN_VAR(vdc_satb_countdown);

		SCAN_VAR(vce_address);
		SCAN_VAR(vce_control);
		SCAN_VAR(vce_current_line);

		SCAN_VAR(vpc_window1);
		SCAN_VAR(vpc_window2);
		SCAN_VAR(vpc_vdc_select);
		SCAN_VAR(vpc_priority);

		SCAN_VAR(vpc_prio);
		SCAN_VAR(vpc_vdc0_enabled);
		SCAN_VAR(vpc_vdc1_enabled);
		SCAN_VAR(vpc_prio_map);
	}

	return 0;
}
