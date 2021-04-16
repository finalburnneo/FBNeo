// FB Alpha Battle Toads driver module
// Based on MAME driver by Aaron Giles

// About the Bulkhead bug: *note, this bug is fixed with a hack*
// When bodyslamming an enemy and the ground shakes - sometimes the top line(s)
// of the bulkhead(s) goes awry.  This is due to the game thinking it's on
// line 125 when it's actually on 121 (where it sets the raster split).  It
// querys the cpu around like 62 for the line# it's drawing at, then sorta
// guesses when it's on line 121.
// Tried emulating the cached/noncached waitstates as mentioned in the manual;
// it helped, but occasionally would slow down the game and/or bug still
// happened in some rare instances. d'oh!
// Fixed w/a silly hack (for now) by adding 60 "extra" lines which takes away
// enough cycles/line for the scroll registers to be written at the
// right time. -dink jan 12, 2020

#include "tiles_generic.h"
#include "tms34_intf.h"
#include "z80_intf.h"
#include "bsmt2000.h"
#include "tlc34076.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvTMSROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvBSMTData;
static UINT8 *DrvBSMTPrg;
static UINT8 *DrvNVRAM;
static UINT8 *DrvTMSRAM;
static UINT8 *DrvFgRAM[3];
static UINT8 *DrvVidRAM[2];
static UINT8 *DrvBSMTRAM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvSprScale;

static UINT8 DrvRecalc;

static UINT8 scrollx[2];
static UINT8 scrolly[2];
static UINT8 screen_control;
static UINT8 vram_page_select;
static UINT16 misc_control_data;

static UINT8 sound_to_main_data;
static INT32 sound_to_main_ready;
static UINT8 main_to_sound_data;
static INT32 main_to_sound_ready;
static INT32 sound_int_state;

static INT32 linecnt;

static INT32 nExtraCycles[3];

static UINT16 sprite_control;
static UINT8 *sprite_dest_base;
static INT32 sprite_dest_base_offs;
static INT32 sprite_dest_offs;
static INT32 sprite_source_offs;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy5[8];
static UINT8 DrvDips[1];
static UINT16 DrvInputs[5];
static UINT8 DrvReset;

static struct BurnInputInfo BtoadsInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 6,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"P3 Coin",			BIT_DIGITAL,	DrvJoy3 + 6,	"p3 coin"	},
	{"P3 Start",		BIT_DIGITAL,	DrvJoy3 + 7,	"p3 start"	},
	{"P3 Up",			BIT_DIGITAL,	DrvJoy3 + 0,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	DrvJoy3 + 2,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	DrvJoy3 + 3,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p3 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy5 + 1,	"diag"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Btoads)

static struct BurnDIPInfo BtoadsDIPList[]=
{
	DIP_OFFSET(0x1a)

	{0x00, 0xff, 0xff, 0x78, NULL					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x00, 0x01, 0x01, 0x01, "Off"					},
	{0x00, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Stereo"				},
	{0x00, 0x01, 0x02, 0x02, "Off"					},
	{0x00, 0x01, 0x02, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Common Coin Mech"		},
	{0x00, 0x01, 0x04, 0x04, "Off"					},
	{0x00, 0x01, 0x04, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Three Players"		},
	{0x00, 0x01, 0x08, 0x08, "Off"					},
	{0x00, 0x01, 0x08, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Free Play"			},
	{0x00, 0x01, 0x10, 0x10, "Off"					},
	{0x00, 0x01, 0x10, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Blood Free Mode"		},
	{0x00, 0x01, 0x20, 0x20, "Off"					},
	{0x00, 0x01, 0x20, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Credit Retention"		},
	{0x00, 0x01, 0x40, 0x40, "Off"					},
	{0x00, 0x01, 0x40, 0x00, "On"					},
};

STDDIPINFO(Btoads)

static void vram_bg0_write(UINT32 offset, UINT16 data)
{
	offset &= 0x3fffff;
	UINT16 *vram = (UINT16*)DrvVidRAM[0];
	vram[TOWORD(offset) & 0x3fcff] = BURN_ENDIAN_SWAP_INT16(data);
}

static void vram_bg1_write(UINT32 offset, UINT16 data)
{
	offset &= 0x3fffff;
	UINT16 *vram = (UINT16*)DrvVidRAM[1];
	vram[TOWORD(offset) & 0x3fcff] = BURN_ENDIAN_SWAP_INT16(data);
}

static UINT16 vram_bg0_read(UINT32 offset)
{
	offset &= 0x3fffff;
	UINT16 *vram = (UINT16*)DrvVidRAM[0];
	return BURN_ENDIAN_SWAP_INT16(vram[TOWORD(offset) & 0x3fcff]);
}

static UINT16 vram_bg1_read(UINT32 offset)
{
	offset &= 0x3fffff;
	UINT16 *vram = (UINT16*)DrvVidRAM[1];
	return vram[TOWORD(offset) & 0x3fcff];
}

static void fg_draw_write(UINT32 offset, UINT16 data)
{
	DrvFgRAM[vram_page_select][TOWORD(offset & 0x3fffff)] = data & 0xff;
}

static UINT16 fg_draw_read(UINT32 offset)
{
	return DrvFgRAM[vram_page_select][TOWORD(offset & 0x3fffff)];
}

static void fg_display_write(UINT32 offset, UINT16 data)
{
	DrvFgRAM[vram_page_select^1][TOWORD(offset & 0x3fffff)] = data & 0xff;
}

static UINT16 fg_display_read(UINT32 offset)
{
	return DrvFgRAM[vram_page_select^1][TOWORD(offset & 0x3fffff)];
} 

static void sync_sound()
{
	INT32 todo = (TMS34010TotalCycles() * 6 / 8) - ZetTotalCycles();
	if (todo > 0)
		ZetRun(todo);
}

static void sync_bsmt()
{
	INT32 todo = tms32010TotalCycles() - ZetTotalCycles();
	if (todo > 0)
		tms32010Run(todo);
}

// page size is 1000
static void control_write(UINT32 address, UINT16 data)
{
	address &= 0xfff;

	INT32 offset = TOWORD(address & 0xff);

	sync_sound(); // keep sound cpu tightly in-sync with main

	switch (address >> 7)
	{
		case 0x00: // <= 0xff
		case 0x01:
		{
			UINT16 *ram = (UINT16*)DrvSprScale;
			ram[(address >> 7) & 1] = BURN_ENDIAN_SWAP_INT16(data);
		}
		return;

		case 0x02: // <= 0x017f
			sprite_control = data;
		return;

		case 0x03: // <= 0x01ff
		{
			data >>= 8;

			vram_page_select = data >> 7;
			// if page select, fg_draw = fg1, fg_display = fg0
			// if not fg_draw = fg0, fg_display = fg1

			screen_control = data;
		}
		return;

		case 0x04: // <= 0x027f
		{
			scrolly[0] = data >> 8;
			scrollx[0] = data & 0xff;
			//bprintf(0, _T("0 sy %x  sx %x\t\tscanline %d\tcyc %d\n"), scrolly[0], scrollx[0], sline, linestart);
		}
		return;

		case 0x05: // <= 0x2ff
		{
			scrolly[1] = data >> 8;
			scrollx[1] = data & 0xff;
			//bprintf(0, _T("1 sy %x  sx %x\t\tscanline %d\tcyc %d\n"), scrolly[1], scrollx[1], sline, linestart);
		}
		return;

		case 0x06: // <= 0x37f
		{
			tlc34076_write(offset >> 1, data);
		}
		return;

		case 0x07: // <= 0x3ff
		{
			main_to_sound_data = data & 0xff;
			main_to_sound_ready = 1;
		}
		return;

		case 0x08: // <= 0x47f
		{
			misc_control_data = data;

			ZetSetRESETLine((data & 8) ? 0 : 1);
		}
		return;
	}
}

static UINT16 control_read(UINT32 address)
{
	address &= 0xfff;

	INT32 offset = TOWORD(address & 0xff);

	sync_sound(); // keep sound cpu tightly in-sync with main

	switch (address >> 7)
	{
		case 0x00: // <= 0x007f
			return DrvInputs[0];

		case 0x01: // <= 0x00ff
			return DrvInputs[1];

		case 0x02: // <= 0x017f
			return DrvInputs[2];

		case 0x03: // <= 0x01ff
			return 0xffff;

		case 0x04: // <= 0x027f
		{
			UINT16 ret = 0xff7c | (DrvInputs[4] & 0x02);

			if (sound_to_main_ready) ret |= 0x01;
			if (main_to_sound_ready) ret |= 0x80;

			return ret;
		}

		case 0x05: // <= 0x02ff
			return 0xff80 | DrvDips[0]; // input 5

		case 0x06: // <= 0x037f
			return tlc34076_read(offset >> 1);

		case 0x07: // <= 0x03ff
			sound_to_main_ready = 0;

			return sound_to_main_data;
	}

	return 0;
}


static void __fastcall sound_write_port(UINT16 port, UINT8 data)
{
	sync_bsmt();

	if (port < 0x8000) {
		bsmt2k_write_reg(port >> 8);
		bsmt2k_write_data(((port & 0xff) << 8) | data);
		return;
	}

	switch (port)
	{
		case 0x8000:
			sound_to_main_data = data;
			sound_to_main_ready = 1;
		return;

		case 0x8002:
		{
			if (!(sound_int_state & 0x80) && (data & 0x80)) {
				bsmt2kResetCpu();
			}

			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);

			sound_int_state = data;
		}
		return;
	}
}

static UINT8 __fastcall sound_read_port(UINT16 port)
{
	sync_bsmt();

	switch (port)
	{
		case 0x8000:
			main_to_sound_ready = 0;

			return main_to_sound_data;

		case 0x8004:
			return (main_to_sound_ready) ? 0x00 : 0x80;

		case 0x8005:
			return (sound_to_main_ready) ? 0x00 : 0x80;

		case 0x8006:
			return bsmt2k_read_status() << 7;
	}

	return 0;
}

void render_sprite_row(void *spr_source, UINT32 address)
{
	UINT16 *sprite_source = (UINT16*)spr_source;
	UINT16 *sprite_scale = (UINT16*)DrvSprScale;
	int flipxor = ((sprite_control >> 10) & 1) ? 0xffff : 0x0000;
	int width = (~sprite_control & 0x1ff) + 2;
	int color = (~sprite_control >> 8) & 0xf0;
	int srcoffs = sprite_source_offs << 8;
	int srcend = srcoffs + (width << 8);
	int srcstep = 0x100 - BURN_ENDIAN_SWAP_INT16(sprite_scale[0]);
	int dststep = 0x100 - BURN_ENDIAN_SWAP_INT16(sprite_scale[1]);
	int dstoffs = sprite_dest_offs << 8;

	if (!(misc_control_data & 0x10))
	{
		for ( ; srcoffs < srcend; srcoffs += srcstep, dstoffs += dststep)
		{
			UINT16 src = BURN_ENDIAN_SWAP_INT16(sprite_source[(srcoffs >> 10) & 0x1ff]);
			if (src)
			{
				src = (src >> (((srcoffs ^ flipxor) >> 6) & 0x0c)) & 0x0f;
				if (src)
					sprite_dest_base[(dstoffs >> 8) & 0x1ff] = src | color;
			}
		}
	}
	else
	{
		for ( ; srcoffs < srcend; srcoffs += srcstep, dstoffs += dststep)
		{
			UINT16 src = BURN_ENDIAN_SWAP_INT16(sprite_source[(srcoffs >> 10) & 0x1ff]);
			if (src)
			{
				src = (src >> (((srcoffs ^ flipxor) >> 6) & 0x0c)) & 0x0f;
				if (src)
					sprite_dest_base[(dstoffs >> 8) & 0x1ff] = color;
			}
		}
	}

	sprite_source_offs += width;
	sprite_dest_offs = dstoffs >> 8;
}

static void to_shiftreg(UINT32 address, UINT16 *shiftreg)
{
	address &= ~0x40000000;

	if (address >= 0xa0000000 && address <= 0xa3ffffff)
	{
		UINT8 *ram = DrvFgRAM[vram_page_select^1]; // display
		memcpy(shiftreg, &ram[(address & 0x3fffff) >> 4], 0x200);
	}
	else if (address >= 0xa4000000 && address <= 0xa7ffffff)
	{
		UINT8 *ram = DrvFgRAM[vram_page_select]; // draw
		sprite_dest_base = &ram[(address & 0x3fc000) >> 4];
		sprite_dest_base_offs = (address & 0x3fc000) >> 4;
		sprite_dest_offs = (address & 0x003fff) >> 5;
	}
	else if (address >= 0xa8000000 && address <= 0xabffffff)
	{
		UINT16 *ram = (UINT16*)DrvFgRAM[2]; // data
		memcpy(shiftreg, &ram[(address & 0x7fc000) >> 4], 0x400);
		sprite_source_offs = (address & 0x003fff) >> 3;
	}
}

static void from_shiftreg(UINT32 address, UINT16 *shiftreg)
{
	address &= ~0x40000000;

	if (address >= 0xa0000000 && address <= 0xa3ffffff)
	{
		UINT8 *ram = DrvFgRAM[vram_page_select^1]; // display
		memcpy(&ram[(address & 0x3fc000) >> 4], shiftreg, 0x200);
	}
	else if (address >= 0xa8000000 && address <= 0xabffffff)
	{
		UINT16 *ram = (UINT16*)DrvFgRAM[2]; // data
		memcpy(&ram[(address & 0x7fc000) >> 4], shiftreg, 0x400);
	}
	else if (address >= 0xac000000 && address <= 0xafffffff)
	{
		if (nSpriteEnable & 1)
			render_sprite_row(shiftreg, address);
	}
}

static INT32 ScanlineRender(INT32 scanline, TMS34010Display *info)
{
	scanline -= info->veblnk; // clipping

	if (scanline < 0 || scanline >= nScreenHeight) return 0;

	UINT16 *vram0 = (UINT16*)DrvVidRAM[0];
	UINT16 *vram1 = (UINT16*)DrvVidRAM[1];
	UINT32 fulladdr = ((info->rowaddr << 16) | info->coladdr) >> 4;
	UINT16 *bg0_base = &vram0[(fulladdr + (scrolly[0] << 10)) & 0x3fc00];
	UINT16 *bg1_base = &vram1[(fulladdr + (scrolly[1] << 10)) & 0x3fc00];
	UINT8 *spr_base = &DrvFgRAM[vram_page_select^1][fulladdr & 0x3fc00];
	UINT16 *dst = pTransDraw + (scanline * nScreenWidth);
	INT32 coladdr = fulladdr & 0x3ff;

#if 0
	if (scanline == 0) {
		bprintf(0, _T("he %d\n"), info->heblnk);
		bprintf(0, _T("hs %d\n"), info->hsblnk);
		bprintf(0, _T("ve %d\n"), info->veblnk);
		bprintf(0, _T("vs %d\n"), info->vsblnk);
		bprintf(0, _T("vt %d\n"), info->vtotal);
		bprintf(0, _T("ht %d\n"), info->htotal);
	}
#endif

	switch (screen_control & 3)
	{
		case 0:
			for (INT32 x = info->heblnk; x < info->hsblnk; x += 2, coladdr++)
			{
				INT32 ex = x - info->heblnk; // clipping
				UINT8 sprpix = spr_base[coladdr & 0xff];

				if (sprpix && !(sprpix & 0x80))
				{
					dst[ex + 0] = sprpix;
					dst[ex + 1] = sprpix;
				}
				else
				{
					UINT16 bg0pix = BURN_ENDIAN_SWAP_INT16(bg0_base[(coladdr + scrollx[0]) & 0xff]);
					UINT16 bg1pix = BURN_ENDIAN_SWAP_INT16(bg1_base[(coladdr + scrollx[1]) & 0xff]);
					sprpix = spr_base[coladdr & 0xff];

					if (bg1pix & 0x80)
						dst[ex + 0] = bg1pix & 0xff;
					else if (sprpix)
						dst[ex + 0] = sprpix;
					else if (bg1pix & 0xff)
						dst[ex + 0] = bg1pix & 0xff;
					else
						dst[ex + 0] = bg0pix & 0xff;

					if (bg1pix & 0x8000)
						dst[ex + 1] = bg1pix >> 8;
					else if (sprpix)
						dst[ex + 1] = sprpix;
					else if (bg1pix >> 8)
						dst[ex + 1] = bg1pix >> 8;
					else
						dst[ex + 1] = bg0pix >> 8;
				}
			}
		break;

		case 1:
			for (INT32 x = info->heblnk; x < info->hsblnk; x += 2, coladdr++)
			{
				INT32 ex = x - info->heblnk;
				UINT8 sprpix = spr_base[coladdr & 0xff];

				if (sprpix && !(sprpix & 0x80))
				{
					dst[ex + 0] = sprpix;
					dst[ex + 1] = sprpix;
				}
				else
				{
					UINT16 bg0pix = BURN_ENDIAN_SWAP_INT16(bg0_base[(coladdr + scrollx[0]) & 0xff]);
					UINT16 bg1pix = BURN_ENDIAN_SWAP_INT16(bg1_base[(coladdr + scrollx[1]) & 0xff]);

					if (bg0pix & 0xff)
						dst[ex + 0] = bg0pix & 0xff;
					else if (bg1pix & 0x80)
						dst[ex + 0] = bg1pix & 0xff;
					else if (sprpix)
						dst[ex + 0] = sprpix;
					else
						dst[ex + 0] = bg1pix & 0xff;

					if (bg0pix >> 8)
						dst[ex + 1] = bg0pix >> 8;
					else if (bg1pix & 0x8000)
						dst[ex + 1] = bg1pix >> 8;
					else if (sprpix)
						dst[ex + 1] = sprpix;
					else
						dst[ex + 1] = bg1pix >> 8;
				}
			}
		break;

		case 2:
			for (INT32 x = info->heblnk; x < info->hsblnk; x += 2, coladdr++)
			{
				INT32 ex = x - info->heblnk;
				UINT8 sprpix = spr_base[coladdr & 0xff];

				if (sprpix)
				{
					dst[ex + 0] = sprpix;
					dst[ex + 1] = sprpix;
				}
				else
				{
					UINT16 bg0pix = BURN_ENDIAN_SWAP_INT16(bg0_base[(coladdr + scrollx[0]) & 0xff]);
					UINT16 bg1pix = BURN_ENDIAN_SWAP_INT16(bg1_base[(coladdr + scrollx[1]) & 0xff]);

					if (bg1pix & 0xff)
						dst[ex + 0] = bg1pix & 0xff;
					else
						dst[ex + 0] = bg0pix & 0xff;

					if (bg1pix >> 8)
						dst[ex + 1] = bg1pix >> 8;
					else
						dst[ex + 1] = bg0pix >> 8;
				}
			}
		break;

		case 3:
			for (INT32 x = info->heblnk; x < info->hsblnk; x += 2, coladdr++)
			{
				INT32 ex = x - info->heblnk;
				UINT16 bg0pix = BURN_ENDIAN_SWAP_INT16(bg0_base[(coladdr + scrollx[0]) & 0xff]);
				UINT16 bg1pix = BURN_ENDIAN_SWAP_INT16(bg1_base[(coladdr + scrollx[1]) & 0xff]);
				UINT8 sprpix = spr_base[coladdr & 0xff];

				if (bg1pix & 0x80)
					dst[ex + 0] = bg1pix & 0xff;
				else if (sprpix & 0x80)
					dst[ex + 0] = sprpix;
				else if (bg1pix & 0xff)
					dst[ex + 0] = bg1pix & 0xff;
				else if (sprpix)
					dst[ex + 0] = sprpix;
				else
					dst[ex + 0] = bg0pix & 0xff;

				if (bg1pix & 0x8000)
					dst[ex + 1] = bg1pix >> 8;
				else if (sprpix & 0x80)
					dst[ex + 1] = sprpix;
				else if (bg1pix >> 8)
					dst[ex + 1] = bg1pix >> 8;
				else if (sprpix)
					dst[ex + 1] = sprpix;
				else
					dst[ex + 1] = bg0pix >> 8;
			}
		break;
	}

	return 0;
}

static void sprite_dest_rebase()
{
	// re-base sprite dest base pointer on state load
	UINT8 *ram = DrvFgRAM[vram_page_select]; // draw
	sprite_dest_base = &ram[sprite_dest_base_offs];
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	TMS34010Open(0);
	TMS34010Reset();
	TMS34010Close();

	ZetOpen(0);
	ZetReset();
	ZetClose();

	tlc34076_reset(6);

	memset (scrollx, 0, sizeof(scrollx));
	memset (scrolly, 0, sizeof(scrolly));
	screen_control = 0;
	vram_page_select = 0;
	misc_control_data = 0;

	sound_to_main_data = 0;
	sound_to_main_ready = 0;
	main_to_sound_data = 0;
	main_to_sound_ready = 0;
	sound_int_state = 0;
	linecnt = 0;

	sprite_control = 0;
	sprite_dest_rebase();
	sprite_dest_offs = 0;
	sprite_source_offs = 0;

	nExtraCycles[0] = nExtraCycles[1] = nExtraCycles[2] = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvTMSROM		= Next; Next += 0x0800000 * 2;

	DrvZ80ROM		= Next; Next += 0x0008000;

	DrvBSMTData		= Next; Next += 0x1000000;
	DrvBSMTPrg		= Next; Next += 0x0002000;

	pBurnDrvPalette	= (UINT32*)Next; Next += 0x0100 * sizeof(UINT32);

	DrvNVRAM		= Next; Next += 0x0008000;

	AllRam			= Next;

	DrvTMSRAM		= Next; Next += 0x0080000; // 0x400000 >> 3
	DrvFgRAM[2]		= Next; Next += 0x0100000;
	DrvFgRAM[1]		= Next; Next += 0x0080000;
	DrvFgRAM[0]		= Next; Next += 0x0080000;
	DrvVidRAM[1]	= Next; Next += 0x0080000;
	DrvVidRAM[0]	= Next; Next += 0x0080000;
	DrvBSMTRAM		= Next; Next += 0x0000200;
	DrvZ80RAM		= Next; Next += 0x0008000;

	DrvSprScale		= Next; Next += 0x0000004;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvZ80ROM  + 0x000000,  k++, 1)) return 1;

		if (BurnLoadRomExt(DrvTMSROM + 0x0000000, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvTMSROM + 0x0000002, k++, 4, LD_GROUP(2))) return 1;

		if (BurnLoadRom(DrvBSMTData  + 0x000000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvBSMTPrg   + 0x000000,  k++, 1)) return 1;
	}

	TMS34020Init(0); // 34020 !
	TMS34010Open(0);
	TMS34010SetPixClock(10000000, 1);
	TMS34010SetCpuCyclesPerFrame(8000000 / 60);

	TMS34010SetScanlineRender(ScanlineRender);
	TMS34010SetToShift(to_shiftreg);
	TMS34010SetFromShift(from_shiftreg);

	TMS34010MapMemory(DrvTMSROM,	0xfc000000, 0xffffffff, MAP_READ);

	TMS34010MapMemory(DrvTMSRAM,	0x00000000, 0x003fffff, MAP_READ | MAP_WRITE);
	TMS34010MapMemory(DrvFgRAM[2],	0xa8000000, 0xa87fffff, MAP_READ | MAP_WRITE); // fg_data
	TMS34010MapMemory(DrvNVRAM,		0x60000000, 0x6003ffff, MAP_READ | MAP_WRITE);

	// nop a8800000 - a8ffffff
	// nop 40000000 - 4000000f -- watchdog?

	TMS34010SetHandlers(0x01, 	vram_bg0_read, vram_bg0_write);
	TMS34010MapHandler(0x01,	0xb0000000, 0xb03fffff, MAP_READ | MAP_WRITE);

	TMS34010SetHandlers(0x02,	vram_bg1_read, vram_bg1_write);
	TMS34010MapHandler(0x02,	0xb4000000, 0xb43fffff, MAP_READ | MAP_WRITE);

	TMS34010SetHandlers(0x03,	fg_draw_read, fg_draw_write);
	TMS34010MapHandler(0x03, 	0xa4000000, 0xa43fffff, MAP_READ | MAP_WRITE);

	TMS34010SetHandlers(0x04,	fg_display_read, fg_display_write);
	TMS34010MapHandler(0x04, 	0xa0000000, 0xa03fffff, MAP_READ | MAP_WRITE);

	TMS34010SetHandlers(0x05, 	control_read, control_write);
	TMS34010MapHandler(0x05, 	0x20000000, 0x20000fff, MAP_READ | MAP_WRITE);
	TMS34010Close();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,		0x8000, 0xffff, MAP_RAM);
	ZetSetOutHandler(sound_write_port);
	ZetSetInHandler(sound_read_port);
	ZetClose();

	bsmt2kInit(24000000 / 4, DrvBSMTPrg, DrvBSMTRAM, DrvBSMTData, 0x1000000, NULL);

	GenericTilesInit();

	memset (DrvNVRAM, 0xff, 0x8000);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	TMS34010Exit();
	ZetExit();
	bsmt2kExit();

	BurnFreeMemIndex();

	return 0;
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		tlc34076_recalc_palette();
		DrvRecalc = 0;
	}

	BurnTransferCopy(pBurnDrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		DrvInputs[0] = 0xffff;
		DrvInputs[1] = 0xffff;
		DrvInputs[2] = 0xffff;
		DrvInputs[4] = 0xffff;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[4] ^= (DrvJoy5[i] & 1) << i;
		}
	}

	ZetNewFrame();
	TMS34010NewFrame();
	bsmt2kNewFrame();

	INT32 nInterleave = 258+60;
	INT32 nCyclesTotal[3] = { 8000000 / 60 /*tms*/, 6000000 / 60/*Z80*/, 24000000 / 4 / 60/*BSMT*/ };
	INT32 nCyclesDone[3] = { nExtraCycles[0], nExtraCycles[1], nExtraCycles[2] };

	ZetOpen(0);
	TMS34010Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN_SYNCINT(0, TMS34010);

		TMS34010GenerateScanline(i);

		CPU_RUN_SYNCINT(1, Zet);

		CPU_RUN_SYNCINT(2, tms32010);

		if (linecnt++ >= (nInterleave / 3.05)) {
			linecnt = 0;
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		}
	}

	if (pBurnSoundOut) {
		bsmt2k_update();
	}

	nExtraCycles[0] = TMS34010TotalCycles() - nCyclesTotal[0];
	nExtraCycles[1] = ZetTotalCycles() - nCyclesTotal[1];
	nExtraCycles[2] = tms32010TotalCycles() - nCyclesTotal[2];

	ZetClose();
	TMS34010Close();

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029704;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_NVRAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data		= DrvNVRAM;
		ba.nLen		= 0x8000;
		ba.nAddress	= 0;
		ba.szName	= "NV RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA)
	{
		TMS34010Scan(nAction);
		ZetScan(nAction);

		tlc34076_Scan(nAction);

		bsmt2kScan(nAction, pnMin);

		SCAN_VAR(scrollx);
		SCAN_VAR(scrolly);
		SCAN_VAR(screen_control);
		SCAN_VAR(vram_page_select);
		SCAN_VAR(misc_control_data);

		SCAN_VAR(sound_to_main_data);
		SCAN_VAR(sound_to_main_ready);
		SCAN_VAR(main_to_sound_data);
		SCAN_VAR(main_to_sound_ready);
		SCAN_VAR(sound_int_state);
		SCAN_VAR(linecnt);

		SCAN_VAR(sprite_control);
		SCAN_VAR(sprite_dest_base_offs);
		SCAN_VAR(sprite_dest_offs);
		SCAN_VAR(sprite_source_offs);

		SCAN_VAR(nExtraCycles);
	}

	if (nAction & ACB_WRITE) {
		sprite_dest_rebase();
	}

	return 0;
}


// Battletoads

static struct BurnRomInfo btoadsRomDesc[] = {
	{ "bt.u102",		0x008000, 0xa90b911a, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code

	{ "btc0-p0.u120",	0x400000, 0x0dfd1e35, 2 | BRF_PRG },           //  1 TMS Program
	{ "btc0-p1.u121",	0x400000, 0xdf7487e1, 2 | BRF_PRG },           //  2

	{ "btc0-s.u109",	0x200000, 0xd9612ddb, 3 | BRF_SND },           //  3 BSMT Sample Data

	{ "bsmt2000.bin",	0x002000, 0xc2a265af, 4 | BRF_PRG | BRF_ESS }, //  4 BSMT Program Data

	{ "u10.bin",		0x000157, 0xb1144178, 5 | BRF_OPT },           //  5 PLDs
	{ "u11.bin",		0x000157, 0x7c6beb96, 5 | BRF_OPT },           //  6
	{ "u57.bin",		0x000157, 0xbe355a56, 5 | BRF_OPT },           //  7
	{ "u58.bin",		0x000157, 0x41ed339c, 5 | BRF_OPT },           //  8
	{ "u90.bin",		0x000157, 0xa0d0c3f1, 5 | BRF_OPT },           //  9
	{ "u144.bin",		0x000157, 0x8597017f, 5 | BRF_OPT },           // 10
};

STD_ROM_PICK(btoads)
STD_ROM_FN(btoads)

struct BurnDriver BurnDrvBtoads = {
	"btoads", NULL, NULL, NULL, "1994",
	"Battletoads\0", NULL, "Rare / Electronic Arts", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 3, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, btoadsRomInfo, btoadsRomName, NULL, NULL, NULL, NULL, BtoadsInputInfo, BtoadsDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	512, 224, 4, 3
};
